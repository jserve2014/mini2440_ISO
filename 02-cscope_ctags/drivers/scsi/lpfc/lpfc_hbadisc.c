/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2009 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 * Portions Copyright (C) 2004-2005 Christoph Hellwig              *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

#include <linux/blkdev.h>
#include <linux/pci.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"
#include "lpfc_debugfs.h"

/* AlpaArray for assignment of scsid for scan-down and bind_method */
static uint8_t lpfcAlpaArray[] = {
	0xEF, 0xE8, 0xE4, 0xE2, 0xE1, 0xE0, 0xDC, 0xDA, 0xD9, 0xD6,
	0xD5, 0xD4, 0xD3, 0xD2, 0xD1, 0xCE, 0xCD, 0xCC, 0xCB, 0xCA,
	0xC9, 0xC7, 0xC6, 0xC5, 0xC3, 0xBC, 0xBA, 0xB9, 0xB6, 0xB5,
	0xB4, 0xB3, 0xB2, 0xB1, 0xAE, 0xAD, 0xAC, 0xAB, 0xAA, 0xA9,
	0xA7, 0xA6, 0xA5, 0xA3, 0x9F, 0x9E, 0x9D, 0x9B, 0x98, 0x97,
	0x90, 0x8F, 0x88, 0x84, 0x82, 0x81, 0x80, 0x7C, 0x7A, 0x79,
	0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x6E, 0x6D, 0x6C, 0x6B,
	0x6A, 0x69, 0x67, 0x66, 0x65, 0x63, 0x5C, 0x5A, 0x59, 0x56,
	0x55, 0x54, 0x53, 0x52, 0x51, 0x4E, 0x4D, 0x4C, 0x4B, 0x4A,
	0x49, 0x47, 0x46, 0x45, 0x43, 0x3C, 0x3A, 0x39, 0x36, 0x35,
	0x34, 0x33, 0x32, 0x31, 0x2E, 0x2D, 0x2C, 0x2B, 0x2A, 0x29,
	0x27, 0x26, 0x25, 0x23, 0x1F, 0x1E, 0x1D, 0x1B, 0x18, 0x17,
	0x10, 0x0F, 0x08, 0x04, 0x02, 0x01
};

static void lpfc_disc_timeout_handler(struct lpfc_vport *);
static void lpfc_disc_flush_list(struct lpfc_vport *vport);
static void lpfc_unregister_fcfi_cmpl(struct lpfc_hba *, LPFC_MBOXQ_t *);

void
lpfc_terminate_rport_io(struct fc_rport *rport)
{
	struct lpfc_rport_data *rdata;
	struct lpfc_nodelist * ndlp;
	struct lpfc_hba *phba;

	rdata = rport->dd_data;
	ndlp = rdata->pnode;

	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		if (rport->roles & FC_RPORT_ROLE_FCP_TARGET)
			printk(KERN_ERR "Cannot find remote node"
			" to terminate I/O Data x%x\n",
			rport->port_id);
		return;
	}

	phba  = ndlp->phba;

	lpfc_debugfs_disc_trc(ndlp->vport, LPFC_DISC_TRC_RPORT,
		"rport terminate: sid:x%x did:x%x flg:x%x",
		ndlp->nlp_sid, ndlp->nlp_DID, ndlp->nlp_flag);

	if (ndlp->nlp_sid != NLP_NO_SID) {
		lpfc_sli_abort_iocb(ndlp->vport,
			&phba->sli.ring[phba->sli.fcp_ring],
			ndlp->nlp_sid, 0, LPFC_CTX_TGT);
	}
}

/*
 * This function will be called when dev_loss_tmo fire.
 */
void
lpfc_dev_loss_tmo_callbk(struct fc_rport *rport)
{
	struct lpfc_rport_data *rdata;
	struct lpfc_nodelist * ndlp;
	struct lpfc_vport *vport;
	struct lpfc_hba   *phba;
	struct lpfc_work_evt *evtp;
	int  put_node;
	int  put_rport;

	rdata = rport->dd_data;
	ndlp = rdata->pnode;
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp))
		return;

	vport = ndlp->vport;
	phba  = vport->phba;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_RPORT,
		"rport devlosscb: sid:x%x did:x%x flg:x%x",
		ndlp->nlp_sid, ndlp->nlp_DID, ndlp->nlp_flag);

	/* Don't defer this if we are in the process of deleting the vport
	 * or unloading the driver. The unload will cleanup the node
	 * appropriately we just need to cleanup the ndlp rport info here.
	 */
	if (vport->load_flag & FC_UNLOADING) {
		put_node = rdata->pnode != NULL;
		put_rport = ndlp->rport != NULL;
		rdata->pnode = NULL;
		ndlp->rport = NULL;
		if (put_node)
			lpfc_nlp_put(ndlp);
		if (put_rport)
			put_device(&rport->dev);
		return;
	}

	if (ndlp->nlp_state == NLP_STE_MAPPED_NODE)
		return;

	evtp = &ndlp->dev_loss_evt;

	if (!list_empty(&evtp->evt_listp))
		return;

	spin_lock_irq(&phba->hbalock);
	/* We need to hold the node by incrementing the reference
	 * count until this queued work is done
	 */
	evtp->evt_arg1  = lpfc_nlp_get(ndlp);
	if (evtp->evt_arg1) {
		evtp->evt = LPFC_EVT_DEV_LOSS;
		list_add_tail(&evtp->evt_listp, &phba->work_list);
		lpfc_worker_wake_up(phba);
	}
	spin_unlock_irq(&phba->hbalock);

	return;
}

/*
 * This function is called from the worker thread when dev_loss_tmo
 * expire.
 */
static void
lpfc_dev_loss_tmo_handler(struct lpfc_nodelist *ndlp)
{
	struct lpfc_rport_data *rdata;
	struct fc_rport   *rport;
	struct lpfc_vport *vport;
	struct lpfc_hba   *phba;
	uint8_t *name;
	int  put_node;
	int  put_rport;
	int warn_on = 0;

	rport = ndlp->rport;

	if (!rport)
		return;

	rdata = rport->dd_data;
	name = (uint8_t *) &ndlp->nlp_portname;
	vport = ndlp->vport;
	phba  = vport->phba;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_RPORT,
		"rport devlosstmo:did:x%x type:x%x id:x%x",
		ndlp->nlp_DID, ndlp->nlp_type, rport->scsi_target_id);

	/* Don't defer this if we are in the process of deleting the vport
	 * or unloading the driver. The unload will cleanup the node
	 * appropriately we just need to cleanup the ndlp rport info here.
	 */
	if (vport->load_flag & FC_UNLOADING) {
		if (ndlp->nlp_sid != NLP_NO_SID) {
			/* flush the target */
			lpfc_sli_abort_iocb(vport,
					&phba->sli.ring[phba->sli.fcp_ring],
					ndlp->nlp_sid, 0, LPFC_CTX_TGT);
		}
		put_node = rdata->pnode != NULL;
		put_rport = ndlp->rport != NULL;
		rdata->pnode = NULL;
		ndlp->rport = NULL;
		if (put_node)
			lpfc_nlp_put(ndlp);
		if (put_rport)
			put_device(&rport->dev);
		return;
	}

	if (ndlp->nlp_state == NLP_STE_MAPPED_NODE) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
				 "0284 Devloss timeout Ignored on "
				 "WWPN %x:%x:%x:%x:%x:%x:%x:%x "
				 "NPort x%x\n",
				 *name, *(name+1), *(name+2), *(name+3),
				 *(name+4), *(name+5), *(name+6), *(name+7),
				 ndlp->nlp_DID);
		return;
	}

	if (ndlp->nlp_type & NLP_FABRIC) {
		/* We will clean up these Nodes in linkup */
		put_node = rdata->pnode != NULL;
		put_rport = ndlp->rport != NULL;
		rdata->pnode = NULL;
		ndlp->rport = NULL;
		if (put_node)
			lpfc_nlp_put(ndlp);
		if (put_rport)
			put_device(&rport->dev);
		return;
	}

	if (ndlp->nlp_sid != NLP_NO_SID) {
		warn_on = 1;
		/* flush the target */
		lpfc_sli_abort_iocb(vport, &phba->sli.ring[phba->sli.fcp_ring],
				    ndlp->nlp_sid, 0, LPFC_CTX_TGT);
	}

	if (warn_on) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_DISCOVERY,
				 "0203 Devloss timeout on "
				 "WWPN %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x "
				 "NPort x%06x Data: x%x x%x x%x\n",
				 *name, *(name+1), *(name+2), *(name+3),
				 *(name+4), *(name+5), *(name+6), *(name+7),
				 ndlp->nlp_DID, ndlp->nlp_flag,
				 ndlp->nlp_state, ndlp->nlp_rpi);
	} else {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
				 "0204 Devloss timeout on "
				 "WWPN %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x "
				 "NPort x%06x Data: x%x x%x x%x\n",
				 *name, *(name+1), *(name+2), *(name+3),
				 *(name+4), *(name+5), *(name+6), *(name+7),
				 ndlp->nlp_DID, ndlp->nlp_flag,
				 ndlp->nlp_state, ndlp->nlp_rpi);
	}

	put_node = rdata->pnode != NULL;
	put_rport = ndlp->rport != NULL;
	rdata->pnode = NULL;
	ndlp->rport = NULL;
	if (put_node)
		lpfc_nlp_put(ndlp);
	if (put_rport)
		put_device(&rport->dev);

	if (!(vport->load_flag & FC_UNLOADING) &&
	    !(ndlp->nlp_flag & NLP_DELAY_TMO) &&
	    !(ndlp->nlp_flag & NLP_NPR_2B_DISC) &&
	    (ndlp->nlp_state != NLP_STE_UNMAPPED_NODE))
		lpfc_disc_state_machine(vport, ndlp, NULL, NLP_EVT_DEVICE_RM);

	lpfc_unregister_unused_fcf(phba);
}

/**
 * lpfc_alloc_fast_evt - Allocates data structure for posting event
 * @phba: Pointer to hba context object.
 *
 * This function is called from the functions which need to post
 * events from interrupt context. This function allocates data
 * structure required for posting event. It also keeps track of
 * number of events pending and prevent event storm when there are
 * too many events.
 **/
struct lpfc_fast_path_event *
lpfc_alloc_fast_evt(struct lpfc_hba *phba) {
	struct lpfc_fast_path_event *ret;

	/* If there are lot of fast event do not exhaust memory due to this */
	if (atomic_read(&phba->fast_event_count) > LPFC_MAX_EVT_COUNT)
		return NULL;

	ret = kzalloc(sizeof(struct lpfc_fast_path_event),
			GFP_ATOMIC);
	if (ret) {
		atomic_inc(&phba->fast_event_count);
		INIT_LIST_HEAD(&ret->work_evt.evt_listp);
		ret->work_evt.evt = LPFC_EVT_FASTPATH_MGMT_EVT;
	}
	return ret;
}

/**
 * lpfc_free_fast_evt - Frees event data structure
 * @phba: Pointer to hba context object.
 * @evt:  Event object which need to be freed.
 *
 * This function frees the data structure required for posting
 * events.
 **/
void
lpfc_free_fast_evt(struct lpfc_hba *phba,
		struct lpfc_fast_path_event *evt) {

	atomic_dec(&phba->fast_event_count);
	kfree(evt);
}

/**
 * lpfc_send_fastpath_evt - Posts events generated from fast path
 * @phba: Pointer to hba context object.
 * @evtp: Event data structure.
 *
 * This function is called from worker thread, when the interrupt
 * context need to post an event. This function posts the event
 * to fc transport netlink interface.
 **/
static void
lpfc_send_fastpath_evt(struct lpfc_hba *phba,
		struct lpfc_work_evt *evtp)
{
	unsigned long evt_category, evt_sub_category;
	struct lpfc_fast_path_event *fast_evt_data;
	char *evt_data;
	uint32_t evt_data_size;
	struct Scsi_Host *shost;

	fast_evt_data = container_of(evtp, struct lpfc_fast_path_event,
		work_evt);

	evt_category = (unsigned long) fast_evt_data->un.fabric_evt.event_type;
	evt_sub_category = (unsigned long) fast_evt_data->un.
			fabric_evt.subcategory;
	shost = lpfc_shost_from_vport(fast_evt_data->vport);
	if (evt_category == FC_REG_FABRIC_EVENT) {
		if (evt_sub_category == LPFC_EVENT_FCPRDCHKERR) {
			evt_data = (char *) &fast_evt_data->un.read_check_error;
			evt_data_size = sizeof(fast_evt_data->un.
				read_check_error);
		} else if ((evt_sub_category == LPFC_EVENT_FABRIC_BUSY) ||
			(evt_sub_category == LPFC_EVENT_PORT_BUSY)) {
			evt_data = (char *) &fast_evt_data->un.fabric_evt;
			evt_data_size = sizeof(fast_evt_data->un.fabric_evt);
		} else {
			lpfc_free_fast_evt(phba, fast_evt_data);
			return;
		}
	} else if (evt_category == FC_REG_SCSI_EVENT) {
		switch (evt_sub_category) {
		case LPFC_EVENT_QFULL:
		case LPFC_EVENT_DEVBSY:
			evt_data = (char *) &fast_evt_data->un.scsi_evt;
			evt_data_size = sizeof(fast_evt_data->un.scsi_evt);
			break;
		case LPFC_EVENT_CHECK_COND:
			evt_data = (char *) &fast_evt_data->un.check_cond_evt;
			evt_data_size =  sizeof(fast_evt_data->un.
				check_cond_evt);
			break;
		case LPFC_EVENT_VARQUEDEPTH:
			evt_data = (char *) &fast_evt_data->un.queue_depth_evt;
			evt_data_size = sizeof(fast_evt_data->un.
				queue_depth_evt);
			break;
		default:
			lpfc_free_fast_evt(phba, fast_evt_data);
			return;
		}
	} else {
		lpfc_free_fast_evt(phba, fast_evt_data);
		return;
	}

	fc_host_post_vendor_event(shost,
		fc_get_event_number(),
		evt_data_size,
		evt_data,
		LPFC_NL_VENDOR_ID);

	lpfc_free_fast_evt(phba, fast_evt_data);
	return;
}

static void
lpfc_work_list_done(struct lpfc_hba *phba)
{
	struct lpfc_work_evt  *evtp = NULL;
	struct lpfc_nodelist  *ndlp;
	int free_evt;

	spin_lock_irq(&phba->hbalock);
	while (!list_empty(&phba->work_list)) {
		list_remove_head((&phba->work_list), evtp, typeof(*evtp),
				 evt_listp);
		spin_unlock_irq(&phba->hbalock);
		free_evt = 1;
		switch (evtp->evt) {
		case LPFC_EVT_ELS_RETRY:
			ndlp = (struct lpfc_nodelist *) (evtp->evt_arg1);
			lpfc_els_retry_delay_handler(ndlp);
			free_evt = 0; /* evt is part of ndlp */
			/* decrement the node reference count held
			 * for this queued work
			 */
			lpfc_nlp_put(ndlp);
			break;
		case LPFC_EVT_DEV_LOSS:
			ndlp = (struct lpfc_nodelist *)(evtp->evt_arg1);
			lpfc_dev_loss_tmo_handler(ndlp);
			free_evt = 0;
			/* decrement the node reference count held for
			 * this queued work
			 */
			lpfc_nlp_put(ndlp);
			break;
		case LPFC_EVT_ONLINE:
			if (phba->link_state < LPFC_LINK_DOWN)
				*(int *) (evtp->evt_arg1) = lpfc_online(phba);
			else
				*(int *) (evtp->evt_arg1) = 0;
			complete((struct completion *)(evtp->evt_arg2));
			break;
		case LPFC_EVT_OFFLINE_PREP:
			if (phba->link_state >= LPFC_LINK_DOWN)
				lpfc_offline_prep(phba);
			*(int *)(evtp->evt_arg1) = 0;
			complete((struct completion *)(evtp->evt_arg2));
			break;
		case LPFC_EVT_OFFLINE:
			lpfc_offline(phba);
			lpfc_sli_brdrestart(phba);
			*(int *)(evtp->evt_arg1) =
				lpfc_sli_brdready(phba, HS_FFRDY | HS_MBRDY);
			lpfc_unblock_mgmt_io(phba);
			complete((struct completion *)(evtp->evt_arg2));
			break;
		case LPFC_EVT_WARM_START:
			lpfc_offline(phba);
			lpfc_reset_barrier(phba);
			lpfc_sli_brdreset(phba);
			lpfc_hba_down_post(phba);
			*(int *)(evtp->evt_arg1) =
				lpfc_sli_brdready(phba, HS_MBRDY);
			lpfc_unblock_mgmt_io(phba);
			complete((struct completion *)(evtp->evt_arg2));
			break;
		case LPFC_EVT_KILL:
			lpfc_offline(phba);
			*(int *)(evtp->evt_arg1)
				= (phba->pport->stopped)
				        ? 0 : lpfc_sli_brdkill(phba);
			lpfc_unblock_mgmt_io(phba);
			complete((struct completion *)(evtp->evt_arg2));
			break;
		case LPFC_EVT_FASTPATH_MGMT_EVT:
			lpfc_send_fastpath_evt(phba, evtp);
			free_evt = 0;
			break;
		}
		if (free_evt)
			kfree(evtp);
		spin_lock_irq(&phba->hbalock);
	}
	spin_unlock_irq(&phba->hbalock);

}

static void
lpfc_work_done(struct lpfc_hba *phba)
{
	struct lpfc_sli_ring *pring;
	uint32_t ha_copy, status, control, work_port_events;
	struct lpfc_vport **vports;
	struct lpfc_vport *vport;
	int i;

	spin_lock_irq(&phba->hbalock);
	ha_copy = phba->work_ha;
	phba->work_ha = 0;
	spin_unlock_irq(&phba->hbalock);

	/* First, try to post the next mailbox command to SLI4 device */
	if (phba->pci_dev_grp == LPFC_PCI_DEV_OC)
		lpfc_sli4_post_async_mbox(phba);

	if (ha_copy & HA_ERATT)
		/* Handle the error attention event */
		lpfc_handle_eratt(phba);

	if (ha_copy & HA_MBATT)
		lpfc_sli_handle_mb_event(phba);

	if (ha_copy & HA_LATT)
		lpfc_handle_latt(phba);

	/* Process SLI4 events */
	if (phba->pci_dev_grp == LPFC_PCI_DEV_OC) {
		if (phba->hba_flag & FCP_XRI_ABORT_EVENT)
			lpfc_sli4_fcp_xri_abort_event_proc(phba);
		if (phba->hba_flag & ELS_XRI_ABORT_EVENT)
			lpfc_sli4_els_xri_abort_event_proc(phba);
		if (phba->hba_flag & ASYNC_EVENT)
			lpfc_sli4_async_event_proc(phba);
		if (phba->hba_flag & HBA_POST_RECEIVE_BUFFER) {
			spin_lock_irq(&phba->hbalock);
			phba->hba_flag &= ~HBA_POST_RECEIVE_BUFFER;
			spin_unlock_irq(&phba->hbalock);
			lpfc_sli_hbqbuf_add_hbqs(phba, LPFC_ELS_HBQ);
		}
		if (phba->hba_flag & HBA_RECEIVE_BUFFER)
			lpfc_sli4_handle_received_buffer(phba);
	}

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports; i++) {
			/*
			 * We could have no vports in array if unloading, so if
			 * this happens then just use the pport
			 */
			if (vports[i] == NULL && i == 0)
				vport = phba->pport;
			else
				vport = vports[i];
			if (vport == NULL)
				break;
			spin_lock_irq(&vport->work_port_lock);
			work_port_events = vport->work_port_events;
			vport->work_port_events &= ~work_port_events;
			spin_unlock_irq(&vport->work_port_lock);
			if (work_port_events & WORKER_DISC_TMO)
				lpfc_disc_timeout_handler(vport);
			if (work_port_events & WORKER_ELS_TMO)
				lpfc_els_timeout_handler(vport);
			if (work_port_events & WORKER_HB_TMO)
				lpfc_hb_timeout_handler(phba);
			if (work_port_events & WORKER_MBOX_TMO)
				lpfc_mbox_timeout_handler(phba);
			if (work_port_events & WORKER_FABRIC_BLOCK_TMO)
				lpfc_unblock_fabric_iocbs(phba);
			if (work_port_events & WORKER_FDMI_TMO)
				lpfc_fdmi_timeout_handler(vport);
			if (work_port_events & WORKER_RAMP_DOWN_QUEUE)
				lpfc_ramp_down_queue_handler(phba);
			if (work_port_events & WORKER_RAMP_UP_QUEUE)
				lpfc_ramp_up_queue_handler(phba);
		}
	lpfc_destroy_vport_work_array(phba, vports);

	pring = &phba->sli.ring[LPFC_ELS_RING];
	status = (ha_copy & (HA_RXMASK  << (4*LPFC_ELS_RING)));
	status >>= (4*LPFC_ELS_RING);
	if ((status & HA_RXMASK)
		|| (pring->flag & LPFC_DEFERRED_RING_EVENT)) {
		if (pring->flag & LPFC_STOP_IOCB_EVENT) {
			pring->flag |= LPFC_DEFERRED_RING_EVENT;
			/* Set the lpfc data pending flag */
			set_bit(LPFC_DATA_READY, &phba->data_flags);
		} else {
			pring->flag &= ~LPFC_DEFERRED_RING_EVENT;
			lpfc_sli_handle_slow_ring_event(phba, pring,
							(status &
							 HA_RXMASK));
		}
		/*
		 * Turn on Ring interrupts
		 */
		if (phba->sli_rev <= LPFC_SLI_REV3) {
			spin_lock_irq(&phba->hbalock);
			control = readl(phba->HCregaddr);
			if (!(control & (HC_R0INT_ENA << LPFC_ELS_RING))) {
				lpfc_debugfs_slow_ring_trc(phba,
					"WRK Enable ring: cntl:x%x hacopy:x%x",
					control, ha_copy, 0);

				control |= (HC_R0INT_ENA << LPFC_ELS_RING);
				writel(control, phba->HCregaddr);
				readl(phba->HCregaddr); /* flush */
			} else {
				lpfc_debugfs_slow_ring_trc(phba,
					"WRK Ring ok:     cntl:x%x hacopy:x%x",
					control, ha_copy, 0);
			}
			spin_unlock_irq(&phba->hbalock);
		}
	}
	lpfc_work_list_done(phba);
}

int
lpfc_do_work(void *p)
{
	struct lpfc_hba *phba = p;
	int rc;

	set_user_nice(current, -20);
	phba->data_flags = 0;

	while (!kthread_should_stop()) {
		/* wait and check worker queue activities */
		rc = wait_event_interruptible(phba->work_waitq,
					(test_and_clear_bit(LPFC_DATA_READY,
							    &phba->data_flags)
					 || kthread_should_stop()));
		/* Signal wakeup shall terminate the worker thread */
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_ELS,
					"0433 Wakeup on signal: rc=x%x\n", rc);
			break;
		}

		/* Attend pending lpfc data processing */
		lpfc_work_done(phba);
	}
	phba->worker_thread = NULL;
	lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
			"0432 Worker thread stopped.\n");
	return 0;
}

/*
 * This is only called to handle FC worker events. Since this a rare
 * occurance, we allocate a struct lpfc_work_evt structure here instead of
 * embedding it in the IOCB.
 */
int
lpfc_workq_post_event(struct lpfc_hba *phba, void *arg1, void *arg2,
		      uint32_t evt)
{
	struct lpfc_work_evt  *evtp;
	unsigned long flags;

	/*
	 * All Mailbox completions and LPFC_ELS_RING rcv ring IOCB events will
	 * be queued to worker thread for processing
	 */
	evtp = kmalloc(sizeof(struct lpfc_work_evt), GFP_ATOMIC);
	if (!evtp)
		return 0;

	evtp->evt_arg1  = arg1;
	evtp->evt_arg2  = arg2;
	evtp->evt       = evt;

	spin_lock_irqsave(&phba->hbalock, flags);
	list_add_tail(&evtp->evt_listp, &phba->work_list);
	spin_unlock_irqrestore(&phba->hbalock, flags);

	lpfc_worker_wake_up(phba);

	return 1;
}

void
lpfc_cleanup_rpis(struct lpfc_vport *vport, int remove)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_nodelist *ndlp, *next_ndlp;
	int  rc;

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;
		if (ndlp->nlp_state == NLP_STE_UNUSED_NODE)
			continue;
		if ((phba->sli3_options & LPFC_SLI3_VPORT_TEARDOWN) ||
			((vport->port_type == LPFC_NPIV_PORT) &&
			(ndlp->nlp_DID == NameServer_DID)))
			lpfc_unreg_rpi(vport, ndlp);

		/* Leave Fabric nodes alone on link down */
		if (!remove && ndlp->nlp_type & NLP_FABRIC)
			continue;
		rc = lpfc_disc_state_machine(vport, ndlp, NULL,
					     remove
					     ? NLP_EVT_DEVICE_RM
					     : NLP_EVT_DEVICE_RECOVERY);
	}
	if (phba->sli3_options & LPFC_SLI3_VPORT_TEARDOWN) {
		lpfc_mbx_unreg_vpi(vport);
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
		spin_unlock_irq(shost->host_lock);
	}
}

void
lpfc_port_link_failure(struct lpfc_vport *vport)
{
	/* Cleanup any outstanding RSCN activity */
	lpfc_els_flush_rscn(vport);

	/* Cleanup any outstanding ELS commands */
	lpfc_els_flush_cmd(vport);

	lpfc_cleanup_rpis(vport, 0);

	/* Turn off discovery timer if its running */
	lpfc_can_disctmo(vport);
}

void
lpfc_linkdown_port(struct lpfc_vport *vport)
{
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);

	fc_host_post_event(shost, fc_get_event_number(), FCH_EVT_LINKDOWN, 0);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Link Down:       state:x%x rtry:x%x flg:x%x",
		vport->port_state, vport->fc_ns_retry, vport->fc_flag);

	lpfc_port_link_failure(vport);

}

int
lpfc_linkdown(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_vport **vports;
	LPFC_MBOXQ_t          *mb;
	int i;

	if (phba->link_state == LPFC_LINK_DOWN)
		return 0;
	spin_lock_irq(&phba->hbalock);
	phba->fcf.fcf_flag &= ~(FCF_AVAILABLE | FCF_DISCOVERED);
	if (phba->link_state > LPFC_LINK_DOWN) {
		phba->link_state = LPFC_LINK_DOWN;
		phba->pport->fc_flag &= ~FC_LBIT;
	}
	spin_unlock_irq(&phba->hbalock);
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			/* Issue a LINK DOWN event to all nodes */
			lpfc_linkdown_port(vports[i]);
		}
	lpfc_destroy_vport_work_array(phba, vports);
	/* Clean up any firmware default rpi's */
	mb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mb) {
		lpfc_unreg_did(phba, 0xffff, 0xffffffff, mb);
		mb->vport = vport;
		mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		if (lpfc_sli_issue_mbox(phba, mb, MBX_NOWAIT)
		    == MBX_NOT_FINISHED) {
			mempool_free(mb, phba->mbox_mem_pool);
		}
	}

	/* Setup myDID for link up if we are in pt2pt mode */
	if (phba->pport->fc_flag & FC_PT2PT) {
		phba->pport->fc_myDID = 0;
		mb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (mb) {
			lpfc_config_link(phba, mb);
			mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
			mb->vport = vport;
			if (lpfc_sli_issue_mbox(phba, mb, MBX_NOWAIT)
			    == MBX_NOT_FINISHED) {
				mempool_free(mb, phba->mbox_mem_pool);
			}
		}
		spin_lock_irq(shost->host_lock);
		phba->pport->fc_flag &= ~(FC_PT2PT | FC_PT2PT_PLOGI);
		spin_unlock_irq(shost->host_lock);
	}

	return 0;
}

static void
lpfc_linkup_cleanup_nodes(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp;

	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;
		if (ndlp->nlp_state == NLP_STE_UNUSED_NODE)
			continue;
		if (ndlp->nlp_type & NLP_FABRIC) {
			/* On Linkup its safe to clean up the ndlp
			 * from Fabric connections.
			 */
			if (ndlp->nlp_DID != Fabric_DID)
				lpfc_unreg_rpi(vport, ndlp);
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		} else if (!(ndlp->nlp_flag & NLP_NPR_ADISC)) {
			/* Fail outstanding IO now since device is
			 * marked for PLOGI.
			 */
			lpfc_unreg_rpi(vport, ndlp);
		}
	}
}

static void
lpfc_linkup_port(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;

	if ((vport->load_flag & FC_UNLOADING) != 0)
		return;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Link Up:         top:x%x speed:x%x flg:x%x",
		phba->fc_topology, phba->fc_linkspeed, phba->link_flag);

	/* If NPIV is not enabled, only bring the physical port up */
	if (!(phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
		(vport != phba->pport))
		return;

	fc_host_post_event(shost, fc_get_event_number(), FCH_EVT_LINKUP, 0);

	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~(FC_PT2PT | FC_PT2PT_PLOGI | FC_ABORT_DISCOVERY |
			    FC_RSCN_MODE | FC_NLP_MORE | FC_RSCN_DISCOVERY);
	vport->fc_flag |= FC_NDISC_ACTIVE;
	vport->fc_ns_retry = 0;
	spin_unlock_irq(shost->host_lock);

	if (vport->fc_flag & FC_LBIT)
		lpfc_linkup_cleanup_nodes(vport);

}

static int
lpfc_linkup(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	int i;

	phba->link_state = LPFC_LINK_UP;

	/* Unblock fabric iocbs if they are blocked */
	clear_bit(FABRIC_COMANDS_BLOCKED, &phba->bit_flags);
	del_timer_sync(&phba->fabric_block_timer);

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++)
			lpfc_linkup_port(vports[i]);
	lpfc_destroy_vport_work_array(phba, vports);
	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
	    (phba->sli_rev < LPFC_SLI_REV4))
		lpfc_issue_clear_la(phba, phba->pport);

	return 0;
}

/*
 * This routine handles processing a CLEAR_LA mailbox
 * command upon completion. It is setup in the LPFC_MBOXQ
 * as the completion routine when the command is
 * handed off to the SLI layer.
 */
static void
lpfc_mbx_cmpl_clear_la(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_sli   *psli = &phba->sli;
	MAILBOX_t *mb = &pmb->u.mb;
	uint32_t control;

	/* Since we don't do discovery right now, turn these off here */
	psli->ring[psli->extra_ring].flag &= ~LPFC_STOP_IOCB_EVENT;
	psli->ring[psli->fcp_ring].flag &= ~LPFC_STOP_IOCB_EVENT;
	psli->ring[psli->next_ring].flag &= ~LPFC_STOP_IOCB_EVENT;

	/* Check for error */
	if ((mb->mbxStatus) && (mb->mbxStatus != 0x1601)) {
		/* CLEAR_LA mbox error <mbxStatus> state <hba_state> */
		lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
				 "0320 CLEAR_LA mbxStatus error x%x hba "
				 "state x%x\n",
				 mb->mbxStatus, vport->port_state);
		phba->link_state = LPFC_HBA_ERROR;
		goto out;
	}

	if (vport->port_type == LPFC_PHYSICAL_PORT)
		phba->link_state = LPFC_HBA_READY;

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_PROCESS_LA;
	control = readl(phba->HCregaddr);
	control |= HC_LAINT_ENA;
	writel(control, phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */
	spin_unlock_irq(&phba->hbalock);
	mempool_free(pmb, phba->mbox_mem_pool);
	return;

out:
	/* Device Discovery completes */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0225 Device Discovery completes\n");
	mempool_free(pmb, phba->mbox_mem_pool);

	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~FC_ABORT_DISCOVERY;
	spin_unlock_irq(shost->host_lock);

	lpfc_can_disctmo(vport);

	/* turn on Link Attention interrupts */

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_PROCESS_LA;
	control = readl(phba->HCregaddr);
	control |= HC_LAINT_ENA;
	writel(control, phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */
	spin_unlock_irq(&phba->hbalock);

	return;
}


static void
lpfc_mbx_cmpl_local_config_link(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;

	if (pmb->u.mb.mbxStatus)
		goto out;

	mempool_free(pmb, phba->mbox_mem_pool);

	if (phba->fc_topology == TOPOLOGY_LOOP &&
	    vport->fc_flag & FC_PUBLIC_LOOP &&
	    !(vport->fc_flag & FC_LBIT)) {
			/* Need to wait for FAN - use discovery timer
			 * for timeout.  port_state is identically
			 * LPFC_LOCAL_CFG_LINK while waiting for FAN
			 */
			lpfc_set_disctmo(vport);
			return;
	}

	/* Start discovery by sending a FLOGI. port_state is identically
	 * LPFC_FLOGI while waiting for FLOGI cmpl
	 */
	if (vport->port_state != LPFC_FLOGI) {
		lpfc_initial_flogi(vport);
	}
	return;

out:
	lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
			 "0306 CONFIG_LINK mbxStatus error x%x "
			 "HBA state x%x\n",
			 pmb->u.mb.mbxStatus, vport->port_state);
	mempool_free(pmb, phba->mbox_mem_pool);

	lpfc_linkdown(phba);

	lpfc_printf_vlog(vport, KERN_ERR, LOG_DISCOVERY,
			 "0200 CONFIG_LINK bad hba state x%x\n",
			 vport->port_state);

	lpfc_issue_clear_la(phba, vport);
	return;
}

static void
lpfc_mbx_cmpl_reg_fcfi(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	struct lpfc_vport *vport = mboxq->vport;
	unsigned long flags;

	if (mboxq->u.mb.mbxStatus) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
			 "2017 REG_FCFI mbxStatus error x%x "
			 "HBA state x%x\n",
			 mboxq->u.mb.mbxStatus, vport->port_state);
		mempool_free(mboxq, phba->mbox_mem_pool);
		return;
	}

	/* Start FCoE discovery by sending a FLOGI. */
	phba->fcf.fcfi = bf_get(lpfc_reg_fcfi_fcfi, &mboxq->u.mqe.un.reg_fcfi);
	/* Set the FCFI registered flag */
	spin_lock_irqsave(&phba->hbalock, flags);
	phba->fcf.fcf_flag |= FCF_REGISTERED;
	spin_unlock_irqrestore(&phba->hbalock, flags);
	/* If there is a pending FCoE event, restart FCF table scan. */
	if (lpfc_check_pending_fcoe_event(phba, 1)) {
		mempool_free(mboxq, phba->mbox_mem_pool);
		return;
	}
	if (vport->port_state != LPFC_FLOGI) {
		spin_lock_irqsave(&phba->hbalock, flags);
		phba->fcf.fcf_flag |= (FCF_DISCOVERED | FCF_IN_USE);
		phba->hba_flag &= ~FCF_DISC_INPROGRESS;
		spin_unlock_irqrestore(&phba->hbalock, flags);
		lpfc_initial_flogi(vport);
	}

	mempool_free(mboxq, phba->mbox_mem_pool);
	return;
}

/**
 * lpfc_fab_name_match - Check if the fcf fabric name match.
 * @fab_name: pointer to fabric name.
 * @new_fcf_record: pointer to fcf record.
 *
 * This routine compare the fcf record's fabric name with provided
 * fabric name. If the fabric name are identical this function
 * returns 1 else return 0.
 **/
static uint32_t
lpfc_fab_name_match(uint8_t *fab_name, struct fcf_record *new_fcf_record)
{
	if ((fab_name[0] ==
		bf_get(lpfc_fcf_record_fab_name_0, new_fcf_record)) &&
	    (fab_name[1] ==
		bf_get(lpfc_fcf_record_fab_name_1, new_fcf_record)) &&
	    (fab_name[2] ==
		bf_get(lpfc_fcf_record_fab_name_2, new_fcf_record)) &&
	    (fab_name[3] ==
		bf_get(lpfc_fcf_record_fab_name_3, new_fcf_record)) &&
	    (fab_name[4] ==
		bf_get(lpfc_fcf_record_fab_name_4, new_fcf_record)) &&
	    (fab_name[5] ==
		bf_get(lpfc_fcf_record_fab_name_5, new_fcf_record)) &&
	    (fab_name[6] ==
		bf_get(lpfc_fcf_record_fab_name_6, new_fcf_record)) &&
	    (fab_name[7] ==
		bf_get(lpfc_fcf_record_fab_name_7, new_fcf_record)))
		return 1;
	else
		return 0;
}

/**
 * lpfc_sw_name_match - Check if the fcf switch name match.
 * @fab_name: pointer to fabric name.
 * @new_fcf_record: pointer to fcf record.
 *
 * This routine compare the fcf record's switch name with provided
 * switch name. If the switch name are identical this function
 * returns 1 else return 0.
 **/
static uint32_t
lpfc_sw_name_match(uint8_t *sw_name, struct fcf_record *new_fcf_record)
{
	if ((sw_name[0] ==
		bf_get(lpfc_fcf_record_switch_name_0, new_fcf_record)) &&
	    (sw_name[1] ==
		bf_get(lpfc_fcf_record_switch_name_1, new_fcf_record)) &&
	    (sw_name[2] ==
		bf_get(lpfc_fcf_record_switch_name_2, new_fcf_record)) &&
	    (sw_name[3] ==
		bf_get(lpfc_fcf_record_switch_name_3, new_fcf_record)) &&
	    (sw_name[4] ==
		bf_get(lpfc_fcf_record_switch_name_4, new_fcf_record)) &&
	    (sw_name[5] ==
		bf_get(lpfc_fcf_record_switch_name_5, new_fcf_record)) &&
	    (sw_name[6] ==
		bf_get(lpfc_fcf_record_switch_name_6, new_fcf_record)) &&
	    (sw_name[7] ==
		bf_get(lpfc_fcf_record_switch_name_7, new_fcf_record)))
		return 1;
	else
		return 0;
}

/**
 * lpfc_mac_addr_match - Check if the fcf mac address match.
 * @phba: pointer to lpfc hba data structure.
 * @new_fcf_record: pointer to fcf record.
 *
 * This routine compare the fcf record's mac address with HBA's
 * FCF mac address. If the mac addresses are identical this function
 * returns 1 else return 0.
 **/
static uint32_t
lpfc_mac_addr_match(struct lpfc_hba *phba, struct fcf_record *new_fcf_record)
{
	if ((phba->fcf.mac_addr[0] ==
		bf_get(lpfc_fcf_record_mac_0, new_fcf_record)) &&
	    (phba->fcf.mac_addr[1] ==
		bf_get(lpfc_fcf_record_mac_1, new_fcf_record)) &&
	    (phba->fcf.mac_addr[2] ==
		bf_get(lpfc_fcf_record_mac_2, new_fcf_record)) &&
	    (phba->fcf.mac_addr[3] ==
		bf_get(lpfc_fcf_record_mac_3, new_fcf_record)) &&
	    (phba->fcf.mac_addr[4] ==
		bf_get(lpfc_fcf_record_mac_4, new_fcf_record)) &&
	    (phba->fcf.mac_addr[5] ==
		bf_get(lpfc_fcf_record_mac_5, new_fcf_record)))
		return 1;
	else
		return 0;
}

/**
 * lpfc_copy_fcf_record - Copy fcf information to lpfc_hba.
 * @phba: pointer to lpfc hba data structure.
 * @new_fcf_record: pointer to fcf record.
 *
 * This routine copies the FCF information from the FCF
 * record to lpfc_hba data structure.
 **/
static void
lpfc_copy_fcf_record(struct lpfc_hba *phba, struct fcf_record *new_fcf_record)
{
	phba->fcf.fabric_name[0] =
		bf_get(lpfc_fcf_record_fab_name_0, new_fcf_record);
	phba->fcf.fabric_name[1] =
		bf_get(lpfc_fcf_record_fab_name_1, new_fcf_record);
	phba->fcf.fabric_name[2] =
		bf_get(lpfc_fcf_record_fab_name_2, new_fcf_record);
	phba->fcf.fabric_name[3] =
		bf_get(lpfc_fcf_record_fab_name_3, new_fcf_record);
	phba->fcf.fabric_name[4] =
		bf_get(lpfc_fcf_record_fab_name_4, new_fcf_record);
	phba->fcf.fabric_name[5] =
		bf_get(lpfc_fcf_record_fab_name_5, new_fcf_record);
	phba->fcf.fabric_name[6] =
		bf_get(lpfc_fcf_record_fab_name_6, new_fcf_record);
	phba->fcf.fabric_name[7] =
		bf_get(lpfc_fcf_record_fab_name_7, new_fcf_record);
	phba->fcf.mac_addr[0] =
		bf_get(lpfc_fcf_record_mac_0, new_fcf_record);
	phba->fcf.mac_addr[1] =
		bf_get(lpfc_fcf_record_mac_1, new_fcf_record);
	phba->fcf.mac_addr[2] =
		bf_get(lpfc_fcf_record_mac_2, new_fcf_record);
	phba->fcf.mac_addr[3] =
		bf_get(lpfc_fcf_record_mac_3, new_fcf_record);
	phba->fcf.mac_addr[4] =
		bf_get(lpfc_fcf_record_mac_4, new_fcf_record);
	phba->fcf.mac_addr[5] =
		bf_get(lpfc_fcf_record_mac_5, new_fcf_record);
	phba->fcf.fcf_indx = bf_get(lpfc_fcf_record_fcf_index, new_fcf_record);
	phba->fcf.priority = new_fcf_record->fip_priority;
	phba->fcf.switch_name[0] =
		bf_get(lpfc_fcf_record_switch_name_0, new_fcf_record);
	phba->fcf.switch_name[1] =
		bf_get(lpfc_fcf_record_switch_name_1, new_fcf_record);
	phba->fcf.switch_name[2] =
		bf_get(lpfc_fcf_record_switch_name_2, new_fcf_record);
	phba->fcf.switch_name[3] =
		bf_get(lpfc_fcf_record_switch_name_3, new_fcf_record);
	phba->fcf.switch_name[4] =
		bf_get(lpfc_fcf_record_switch_name_4, new_fcf_record);
	phba->fcf.switch_name[5] =
		bf_get(lpfc_fcf_record_switch_name_5, new_fcf_record);
	phba->fcf.switch_name[6] =
		bf_get(lpfc_fcf_record_switch_name_6, new_fcf_record);
	phba->fcf.switch_name[7] =
		bf_get(lpfc_fcf_record_switch_name_7, new_fcf_record);
}

/**
 * lpfc_register_fcf - Register the FCF with hba.
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine issues a register fcfi mailbox command to register
 * the fcf with HBA.
 **/
static void
lpfc_register_fcf(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *fcf_mbxq;
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&phba->hbalock, flags);

	/* If the FCF is not availabe do nothing. */
	if (!(phba->fcf.fcf_flag & FCF_AVAILABLE)) {
		spin_unlock_irqrestore(&phba->hbalock, flags);
		return;
	}

	/* The FCF is already registered, start discovery */
	if (phba->fcf.fcf_flag & FCF_REGISTERED) {
		phba->fcf.fcf_flag |= (FCF_DISCOVERED | FCF_IN_USE);
		phba->hba_flag &= ~FCF_DISC_INPROGRESS;
		spin_unlock_irqrestore(&phba->hbalock, flags);
		if (phba->pport->port_state != LPFC_FLOGI)
			lpfc_initial_flogi(phba->pport);
		return;
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);

	fcf_mbxq = mempool_alloc(phba->mbox_mem_pool,
		GFP_KERNEL);
	if (!fcf_mbxq)
		return;

	lpfc_reg_fcfi(phba, fcf_mbxq);
	fcf_mbxq->vport = phba->pport;
	fcf_mbxq->mbox_cmpl = lpfc_mbx_cmpl_reg_fcfi;
	rc = lpfc_sli_issue_mbox(phba, fcf_mbxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		mempool_free(fcf_mbxq, phba->mbox_mem_pool);

	return;
}

/**
 * lpfc_match_fcf_conn_list - Check if the FCF record can be used for discovery.
 * @phba: pointer to lpfc hba data structure.
 * @new_fcf_record: pointer to fcf record.
 * @boot_flag: Indicates if this record used by boot bios.
 * @addr_mode: The address mode to be used by this FCF
 *
 * This routine compare the fcf record with connect list obtained from the
 * config region to decide if this FCF can be used for SAN discovery. It returns
 * 1 if this record can be used for SAN discovery else return zero. If this FCF
 * record can be used for SAN discovery, the boot_flag will indicate if this FCF
 * is used by boot bios and addr_mode will indicate the addressing mode to be
 * used for this FCF when the function returns.
 * If the FCF record need to be used with a particular vlan id, the vlan is
 * set in the vlan_id on return of the function. If not VLAN tagging need to
 * be used with the FCF vlan_id will be set to 0xFFFF;
 **/
static int
lpfc_match_fcf_conn_list(struct lpfc_hba *phba,
			struct fcf_record *new_fcf_record,
			uint32_t *boot_flag, uint32_t *addr_mode,
			uint16_t *vlan_id)
{
	struct lpfc_fcf_conn_entry *conn_entry;

	/* If FCF not available return 0 */
	if (!bf_get(lpfc_fcf_record_fcf_avail, new_fcf_record) ||
		!bf_get(lpfc_fcf_record_fcf_valid, new_fcf_record))
		return 0;

	if (!phba->cfg_enable_fip) {
		*boot_flag = 0;
		*addr_mode = bf_get(lpfc_fcf_record_mac_addr_prov,
				new_fcf_record);
		if (phba->valid_vlan)
			*vlan_id = phba->vlan_id;
		else
			*vlan_id = 0xFFFF;
		return 1;
	}

	/*
	 * If there are no FCF connection table entry, driver connect to all
	 * FCFs.
	 */
	if (list_empty(&phba->fcf_conn_rec_list)) {
		*boot_flag = 0;
		*addr_mode = bf_get(lpfc_fcf_record_mac_addr_prov,
			new_fcf_record);

		/*
		 * When there are no FCF connect entries, use driver's default
		 * addressing mode - FPMA.
		 */
		if (*addr_mode & LPFC_FCF_FPMA)
			*addr_mode = LPFC_FCF_FPMA;

		*vlan_id = 0xFFFF;
		return 1;
	}

	list_for_each_entry(conn_entry, &phba->fcf_conn_rec_list, list) {
		if (!(conn_entry->conn_rec.flags & FCFCNCT_VALID))
			continue;

		if ((conn_entry->conn_rec.flags & FCFCNCT_FBNM_VALID) &&
			!lpfc_fab_name_match(conn_entry->conn_rec.fabric_name,
					     new_fcf_record))
			continue;
		if ((conn_entry->conn_rec.flags & FCFCNCT_SWNM_VALID) &&
			!lpfc_sw_name_match(conn_entry->conn_rec.switch_name,
					    new_fcf_record))
			continue;
		if (conn_entry->conn_rec.flags & FCFCNCT_VLAN_VALID) {
			/*
			 * If the vlan bit map does not have the bit set for the
			 * vlan id to be used, then it is not a match.
			 */
			if (!(new_fcf_record->vlan_bitmap
				[conn_entry->conn_rec.vlan_tag / 8] &
				(1 << (conn_entry->conn_rec.vlan_tag % 8))))
				continue;
		}

		/*
		 * If connection record does not support any addressing mode,
		 * skip the FCF record.
		 */
		if (!(bf_get(lpfc_fcf_record_mac_addr_prov, new_fcf_record)
			& (LPFC_FCF_FPMA | LPFC_FCF_SPMA)))
			continue;

		/*
		 * Check if the connection record specifies a required
		 * addressing mode.
		 */
		if ((conn_entry->conn_rec.flags & FCFCNCT_AM_VALID) &&
			!(conn_entry->conn_rec.flags & FCFCNCT_AM_PREFERRED)) {

			/*
			 * If SPMA required but FCF not support this continue.
			 */
			if ((conn_entry->conn_rec.flags & FCFCNCT_AM_SPMA) &&
				!(bf_get(lpfc_fcf_record_mac_addr_prov,
					new_fcf_record) & LPFC_FCF_SPMA))
				continue;

			/*
			 * If FPMA required but FCF not support this continue.
			 */
			if (!(conn_entry->conn_rec.flags & FCFCNCT_AM_SPMA) &&
				!(bf_get(lpfc_fcf_record_mac_addr_prov,
				new_fcf_record) & LPFC_FCF_FPMA))
				continue;
		}

		/*
		 * This fcf record matches filtering criteria.
		 */
		if (conn_entry->conn_rec.flags & FCFCNCT_BOOT)
			*boot_flag = 1;
		else
			*boot_flag = 0;

		/*
		 * If user did not specify any addressing mode, or if the
		 * prefered addressing mode specified by user is not supported
		 * by FCF, allow fabric to pick the addressing mode.
		 */
		*addr_mode = bf_get(lpfc_fcf_record_mac_addr_prov,
				new_fcf_record);
		/*
		 * If the user specified a required address mode, assign that
		 * address mode
		 */
		if ((conn_entry->conn_rec.flags & FCFCNCT_AM_VALID) &&
			(!(conn_entry->conn_rec.flags & FCFCNCT_AM_PREFERRED)))
			*addr_mode = (conn_entry->conn_rec.flags &
				FCFCNCT_AM_SPMA) ?
				LPFC_FCF_SPMA : LPFC_FCF_FPMA;
		/*
		 * If the user specified a prefered address mode, use the
		 * addr mode only if FCF support the addr_mode.
		 */
		else if ((conn_entry->conn_rec.flags & FCFCNCT_AM_VALID) &&
			(conn_entry->conn_rec.flags & FCFCNCT_AM_PREFERRED) &&
			(conn_entry->conn_rec.flags & FCFCNCT_AM_SPMA) &&
			(*addr_mode & LPFC_FCF_SPMA))
				*addr_mode = LPFC_FCF_SPMA;
		else if ((conn_entry->conn_rec.flags & FCFCNCT_AM_VALID) &&
			(conn_entry->conn_rec.flags & FCFCNCT_AM_PREFERRED) &&
			!(conn_entry->conn_rec.flags & FCFCNCT_AM_SPMA) &&
			(*addr_mode & LPFC_FCF_FPMA))
				*addr_mode = LPFC_FCF_FPMA;

		if (conn_entry->conn_rec.flags & FCFCNCT_VLAN_VALID)
			*vlan_id = conn_entry->conn_rec.vlan_tag;
		else
			*vlan_id = 0xFFFF;

		return 1;
	}

	return 0;
}

/**
 * lpfc_check_pending_fcoe_event - Check if there is pending fcoe event.
 * @phba: pointer to lpfc hba data structure.
 * @unreg_fcf: Unregister FCF if FCF table need to be re-scaned.
 *
 * This function check if there is any fcoe event pending while driver
 * scan FCF entries. If there is any pending event, it will restart the
 * FCF saning and return 1 else return 0.
 */
int
lpfc_check_pending_fcoe_event(struct lpfc_hba *phba, uint8_t unreg_fcf)
{
	LPFC_MBOXQ_t *mbox;
	int rc;
	/*
	 * If the Link is up and no FCoE events while in the
	 * FCF discovery, no need to restart FCF discovery.
	 */
	if ((phba->link_state  >= LPFC_LINK_UP) &&
		(phba->fcoe_eventtag == phba->fcoe_eventtag_at_fcf_scan))
		return 0;

	spin_lock_irq(&phba->hbalock);
	phba->fcf.fcf_flag &= ~FCF_AVAILABLE;
	spin_unlock_irq(&phba->hbalock);

	if (phba->link_state >= LPFC_LINK_UP)
		lpfc_sli4_read_fcf_record(phba, LPFC_FCOE_FCF_GET_FIRST);

	if (unreg_fcf) {
		spin_lock_irq(&phba->hbalock);
		phba->fcf.fcf_flag &= ~FCF_REGISTERED;
		spin_unlock_irq(&phba->hbalock);
		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (!mbox) {
			lpfc_printf_log(phba, KERN_ERR,
				LOG_DISCOVERY|LOG_MBOX,
				"2610 UNREG_FCFI mbox allocation failed\n");
			return 1;
		}
		lpfc_unreg_fcfi(mbox, phba->fcf.fcfi);
		mbox->vport = phba->pport;
		mbox->mbox_cmpl = lpfc_unregister_fcfi_cmpl;
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
		if (rc == MBX_NOT_FINISHED) {
			lpfc_printf_log(phba, KERN_ERR, LOG_DISCOVERY|LOG_MBOX,
				"2611 UNREG_FCFI issue mbox failed\n");
			mempool_free(mbox, phba->mbox_mem_pool);
		}
	}

	return 1;
}

/**
 * lpfc_mbx_cmpl_read_fcf_record - Completion handler for read_fcf mbox.
 * @phba: pointer to lpfc hba data structure.
 * @mboxq: pointer to mailbox object.
 *
 * This function iterate through all the fcf records available in
 * HBA and choose the optimal FCF record for discovery. After finding
 * the FCF for discovery it register the FCF record and kick start
 * discovery.
 * If FCF_IN_USE flag is set in currently used FCF, the routine try to
 * use a FCF record which match fabric name and mac address of the
 * currently used FCF record.
 * If the driver support only one FCF, it will try to use the FCF record
 * used by BOOT_BIOS.
 */
void
lpfc_mbx_cmpl_read_fcf_record(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	void *virt_addr;
	dma_addr_t phys_addr;
	uint8_t *bytep;
	struct lpfc_mbx_sge sge;
	struct lpfc_mbx_read_fcf_tbl *read_fcf;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	struct fcf_record *new_fcf_record;
	int rc;
	uint32_t boot_flag, addr_mode;
	uint32_t next_fcf_index;
	unsigned long flags;
	uint16_t vlan_id;

	/* If there is pending FCoE event restart FCF table scan */
	if (lpfc_check_pending_fcoe_event(phba, 0)) {
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
		return;
	}

	/* Get the first SGE entry from the non-embedded DMA memory. This
	 * routine only uses a single SGE.
	 */
	lpfc_sli4_mbx_sge_get(mboxq, 0, &sge);
	phys_addr = getPaddr(sge.pa_hi, sge.pa_lo);
	if (unlikely(!mboxq->sge_array)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX,
				"2524 Failed to get the non-embedded SGE "
				"virtual address\n");
		goto out;
	}
	virt_addr = mboxq->sge_array->addr[0];

	shdr = (union lpfc_sli4_cfg_shdr *)virt_addr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status,
				 &shdr->response);
	/*
	 * The FCF Record was read and there is no reason for the driver
	 * to maintain the FCF record data or memory. Instead, just need
	 * to book keeping the FCFIs can be used.
	 */
	if (shdr_status || shdr_add_status) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2521 READ_FCF_RECORD mailbox failed "
				"with status x%x add_status x%x, mbx\n",
				shdr_status, shdr_add_status);
		goto out;
	}
	/* Interpreting the returned information of FCF records */
	read_fcf = (struct lpfc_mbx_read_fcf_tbl *)virt_addr;
	lpfc_sli_pcimem_bcopy(read_fcf, read_fcf,
			      sizeof(struct lpfc_mbx_read_fcf_tbl));
	next_fcf_index = bf_get(lpfc_mbx_read_fcf_tbl_nxt_vindx, read_fcf);

	new_fcf_record = (struct fcf_record *)(virt_addr +
			  sizeof(struct lpfc_mbx_read_fcf_tbl));
	lpfc_sli_pcimem_bcopy(new_fcf_record, new_fcf_record,
			      sizeof(struct fcf_record));
	bytep = virt_addr + sizeof(union lpfc_sli4_cfg_shdr);

	rc = lpfc_match_fcf_conn_list(phba, new_fcf_record,
				      &boot_flag, &addr_mode,
					&vlan_id);
	/*
	 * If the fcf record does not match with connect list entries
	 * read the next entry.
	 */
	if (!rc)
		goto read_next_fcf;
	/*
	 * If this is not the first FCF discovery of the HBA, use last
	 * FCF record for the discovery.
	 */
	spin_lock_irqsave(&phba->hbalock, flags);
	if (phba->fcf.fcf_flag & FCF_IN_USE) {
		if (lpfc_fab_name_match(phba->fcf.fabric_name,
					new_fcf_record) &&
		    lpfc_sw_name_match(phba->fcf.switch_name,
					new_fcf_record) &&
		    lpfc_mac_addr_match(phba, new_fcf_record)) {
			phba->fcf.fcf_flag |= FCF_AVAILABLE;
			spin_unlock_irqrestore(&phba->hbalock, flags);
			goto out;
		}
		spin_unlock_irqrestore(&phba->hbalock, flags);
		goto read_next_fcf;
	}
	if (phba->fcf.fcf_flag & FCF_AVAILABLE) {
		/*
		 * If the current FCF record does not have boot flag
		 * set and new fcf record has boot flag set, use the
		 * new fcf record.
		 */
		if (boot_flag && !(phba->fcf.fcf_flag & FCF_BOOT_ENABLE)) {
			/* Use this FCF record */
			lpfc_copy_fcf_record(phba, new_fcf_record);
			phba->fcf.addr_mode = addr_mode;
			phba->fcf.fcf_flag |= FCF_BOOT_ENABLE;
			if (vlan_id != 0xFFFF) {
				phba->fcf.fcf_flag |= FCF_VALID_VLAN;
				phba->fcf.vlan_id = vlan_id;
			}
			spin_unlock_irqrestore(&phba->hbalock, flags);
			goto read_next_fcf;
		}
		/*
		 * If the current FCF record has boot flag set and the
		 * new FCF record does not have boot flag, read the next
		 * FCF record.
		 */
		if (!boot_flag && (phba->fcf.fcf_flag & FCF_BOOT_ENABLE)) {
			spin_unlock_irqrestore(&phba->hbalock, flags);
			goto read_next_fcf;
		}
		/*
		 * If there is a record with lower priority value for
		 * the current FCF, use that record.
		 */
		if (lpfc_fab_name_match(phba->fcf.fabric_name,
					new_fcf_record) &&
		    (new_fcf_record->fip_priority < phba->fcf.priority)) {
			/* Use this FCF record */
			lpfc_copy_fcf_record(phba, new_fcf_record);
			phba->fcf.addr_mode = addr_mode;
			if (vlan_id != 0xFFFF) {
				phba->fcf.fcf_flag |= FCF_VALID_VLAN;
				phba->fcf.vlan_id = vlan_id;
			}
			spin_unlock_irqrestore(&phba->hbalock, flags);
			goto read_next_fcf;
		}
		spin_unlock_irqrestore(&phba->hbalock, flags);
		goto read_next_fcf;
	}
	/*
	 * This is the first available FCF record, use this
	 * record.
	 */
	lpfc_copy_fcf_record(phba, new_fcf_record);
	phba->fcf.addr_mode = addr_mode;
	if (boot_flag)
		phba->fcf.fcf_flag |= FCF_BOOT_ENABLE;
	phba->fcf.fcf_flag |= FCF_AVAILABLE;
	if (vlan_id != 0xFFFF) {
		phba->fcf.fcf_flag |= FCF_VALID_VLAN;
		phba->fcf.vlan_id = vlan_id;
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	goto read_next_fcf;

read_next_fcf:
	lpfc_sli4_mbox_cmd_free(phba, mboxq);
	if (next_fcf_index == LPFC_FCOE_FCF_NEXT_NONE || next_fcf_index == 0)
		lpfc_register_fcf(phba);
	else
		lpfc_sli4_read_fcf_record(phba, next_fcf_index);
	return;

out:
	lpfc_sli4_mbox_cmd_free(phba, mboxq);
	lpfc_register_fcf(phba);

	return;
}

/**
 * lpfc_init_vpi_cmpl - Completion handler for init_vpi mbox command.
 * @phba: pointer to lpfc hba data structure.
 * @mboxq: pointer to mailbox data structure.
 *
 * This function handles completion of init vpi mailbox command.
 */
static void
lpfc_init_vpi_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	struct lpfc_vport *vport = mboxq->vport;
	if (mboxq->u.mb.mbxStatus) {
		lpfc_printf_vlog(vport, KERN_ERR,
				LOG_MBOX,
				"2609 Init VPI mailbox failed 0x%x\n",
				mboxq->u.mb.mbxStatus);
		mempool_free(mboxq, phba->mbox_mem_pool);
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		return;
	}
	vport->fc_flag &= ~FC_VPORT_NEEDS_INIT_VPI;

	if (phba->link_flag & LS_NPIV_FAB_SUPPORTED)
		lpfc_initial_fdisc(vport);
	else {
		lpfc_vport_set_state(vport, FC_VPORT_NO_FABRIC_SUPP);
		lpfc_printf_vlog(vport, KERN_ERR,
			LOG_ELS,
			"2606 No NPIV Fabric support\n");
	}
	return;
}

/**
 * lpfc_start_fdiscs - send fdiscs for each vports on this port.
 * @phba: pointer to lpfc hba data structure.
 *
 * This function loops through the list of vports on the @phba and issues an
 * FDISC if possible.
 */
void
lpfc_start_fdiscs(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	int i;
	LPFC_MBOXQ_t *mboxq;
	int rc;

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL) {
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			if (vports[i]->port_type == LPFC_PHYSICAL_PORT)
				continue;
			/* There are no vpi for this vport */
			if (vports[i]->vpi > phba->max_vpi) {
				lpfc_vport_set_state(vports[i],
						     FC_VPORT_FAILED);
				continue;
			}
			if (phba->fc_topology == TOPOLOGY_LOOP) {
				lpfc_vport_set_state(vports[i],
						     FC_VPORT_LINKDOWN);
				continue;
			}
			if (vports[i]->fc_flag & FC_VPORT_NEEDS_INIT_VPI) {
				mboxq = mempool_alloc(phba->mbox_mem_pool,
					GFP_KERNEL);
				if (!mboxq) {
					lpfc_printf_vlog(vports[i], KERN_ERR,
					LOG_MBOX, "2607 Failed to allocate "
					"init_vpi mailbox\n");
					continue;
				}
				lpfc_init_vpi(phba, mboxq, vports[i]->vpi);
				mboxq->vport = vports[i];
				mboxq->mbox_cmpl = lpfc_init_vpi_cmpl;
				rc = lpfc_sli_issue_mbox(phba, mboxq,
					MBX_NOWAIT);
				if (rc == MBX_NOT_FINISHED) {
					lpfc_printf_vlog(vports[i], KERN_ERR,
					LOG_MBOX, "2608 Failed to issue "
					"init_vpi mailbox\n");
					mempool_free(mboxq,
						phba->mbox_mem_pool);
				}
				continue;
			}
			if (phba->link_flag & LS_NPIV_FAB_SUPPORTED)
				lpfc_initial_fdisc(vports[i]);
			else {
				lpfc_vport_set_state(vports[i],
						     FC_VPORT_NO_FABRIC_SUPP);
				lpfc_printf_vlog(vports[i], KERN_ERR,
						 LOG_ELS,
						 "0259 No NPIV "
						 "Fabric support\n");
			}
		}
	}
	lpfc_destroy_vport_work_array(phba, vports);
}

void
lpfc_mbx_cmpl_reg_vfi(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	struct lpfc_dmabuf *dmabuf = mboxq->context1;
	struct lpfc_vport *vport = mboxq->vport;

	if (mboxq->u.mb.mbxStatus) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
			 "2018 REG_VFI mbxStatus error x%x "
			 "HBA state x%x\n",
			 mboxq->u.mb.mbxStatus, vport->port_state);
		if (phba->fc_topology == TOPOLOGY_LOOP) {
			/* FLOGI failed, use loop map to make discovery list */
			lpfc_disc_list_loopmap(vport);
			/* Start discovery */
			lpfc_disc_start(vport);
			goto fail_free_mem;
		}
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		goto fail_free_mem;
	}
	/* Mark the vport has registered with its VFI */
	vport->vfi_state |= LPFC_VFI_REGISTERED;

	if (vport->port_state == LPFC_FABRIC_CFG_LINK) {
		lpfc_start_fdiscs(phba);
		lpfc_do_scr_ns_plogi(phba, vport);
	}

fail_free_mem:
	mempool_free(mboxq, phba->mbox_mem_pool);
	lpfc_mbuf_free(phba, dmabuf->virt, dmabuf->phys);
	kfree(dmabuf);
	return;
}

static void
lpfc_mbx_cmpl_read_sparam(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	MAILBOX_t *mb = &pmb->u.mb;
	struct lpfc_dmabuf *mp = (struct lpfc_dmabuf *) pmb->context1;
	struct lpfc_vport  *vport = pmb->vport;


	/* Check for error */
	if (mb->mbxStatus) {
		/* READ_SPARAM mbox error <mbxStatus> state <hba_state> */
		lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
				 "0319 READ_SPARAM mbxStatus error x%x "
				 "hba state x%x>\n",
				 mb->mbxStatus, vport->port_state);
		lpfc_linkdown(phba);
		goto out;
	}

	memcpy((uint8_t *) &vport->fc_sparam, (uint8_t *) mp->virt,
	       sizeof (struct serv_parm));
	if (phba->cfg_soft_wwnn)
		u64_to_wwn(phba->cfg_soft_wwnn,
			   vport->fc_sparam.nodeName.u.wwn);
	if (phba->cfg_soft_wwpn)
		u64_to_wwn(phba->cfg_soft_wwpn,
			   vport->fc_sparam.portName.u.wwn);
	memcpy(&vport->fc_nodename, &vport->fc_sparam.nodeName,
	       sizeof(vport->fc_nodename));
	memcpy(&vport->fc_portname, &vport->fc_sparam.portName,
	       sizeof(vport->fc_portname));
	if (vport->port_type == LPFC_PHYSICAL_PORT) {
		memcpy(&phba->wwnn, &vport->fc_nodename, sizeof(phba->wwnn));
		memcpy(&phba->wwpn, &vport->fc_portname, sizeof(phba->wwnn));
	}

	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mempool_free(pmb, phba->mbox_mem_pool);
	return;

out:
	pmb->context1 = NULL;
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	lpfc_issue_clear_la(phba, vport);
	mempool_free(pmb, phba->mbox_mem_pool);
	return;
}

static void
lpfc_mbx_process_link_up(struct lpfc_hba *phba, READ_LA_VAR *la)
{
	struct lpfc_vport *vport = phba->pport;
	LPFC_MBOXQ_t *sparam_mbox, *cfglink_mbox = NULL;
	int i;
	struct lpfc_dmabuf *mp;
	int rc;
	struct fcf_record *fcf_record;

	sparam_mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);

	spin_lock_irq(&phba->hbalock);
	switch (la->UlnkSpeed) {
	case LA_1GHZ_LINK:
		phba->fc_linkspeed = LA_1GHZ_LINK;
		break;
	case LA_2GHZ_LINK:
		phba->fc_linkspeed = LA_2GHZ_LINK;
		break;
	case LA_4GHZ_LINK:
		phba->fc_linkspeed = LA_4GHZ_LINK;
		break;
	case LA_8GHZ_LINK:
		phba->fc_linkspeed = LA_8GHZ_LINK;
		break;
	case LA_10GHZ_LINK:
		phba->fc_linkspeed = LA_10GHZ_LINK;
		break;
	default:
		phba->fc_linkspeed = LA_UNKNW_LINK;
		break;
	}

	phba->fc_topology = la->topology;
	phba->link_flag &= ~LS_NPIV_FAB_SUPPORTED;

	if (phba->fc_topology == TOPOLOGY_LOOP) {
		phba->sli3_options &= ~LPFC_SLI3_NPIV_ENABLED;

		if (phba->cfg_enable_npiv)
			lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
				"1309 Link Up Event npiv not supported in loop "
				"topology\n");
				/* Get Loop Map information */
		if (la->il)
			vport->fc_flag |= FC_LBIT;

		vport->fc_myDID = la->granted_AL_PA;
		i = la->un.lilpBde64.tus.f.bdeSize;

		if (i == 0) {
			phba->alpa_map[0] = 0;
		} else {
			if (vport->cfg_log_verbose & LOG_LINK_EVENT) {
				int numalpa, j, k;
				union {
					uint8_t pamap[16];
					struct {
						uint32_t wd1;
						uint32_t wd2;
						uint32_t wd3;
						uint32_t wd4;
					} pa;
				} un;
				numalpa = phba->alpa_map[0];
				j = 0;
				while (j < numalpa) {
					memset(un.pamap, 0, 16);
					for (k = 1; j < numalpa; k++) {
						un.pamap[k - 1] =
							phba->alpa_map[j + 1];
						j++;
						if (k == 16)
							break;
					}
					/* Link Up Event ALPA map */
					lpfc_printf_log(phba,
							KERN_WARNING,
							LOG_LINK_EVENT,
							"1304 Link Up Event "
							"ALPA map Data: x%x "
							"x%x x%x x%x\n",
							un.pa.wd1, un.pa.wd2,
							un.pa.wd3, un.pa.wd4);
				}
			}
		}
	} else {
		if (!(phba->sli3_options & LPFC_SLI3_NPIV_ENABLED)) {
			if (phba->max_vpi && phba->cfg_enable_npiv &&
			   (phba->sli_rev == 3))
				phba->sli3_options |= LPFC_SLI3_NPIV_ENABLED;
		}
		vport->fc_myDID = phba->fc_pref_DID;
		vport->fc_flag |= FC_LBIT;
	}
	spin_unlock_irq(&phba->hbalock);

	lpfc_linkup(phba);
	if (sparam_mbox) {
		lpfc_read_sparam(phba, sparam_mbox, 0);
		sparam_mbox->vport = vport;
		sparam_mbox->mbox_cmpl = lpfc_mbx_cmpl_read_sparam;
		rc = lpfc_sli_issue_mbox(phba, sparam_mbox, MBX_NOWAIT);
		if (rc == MBX_NOT_FINISHED) {
			mp = (struct lpfc_dmabuf *) sparam_mbox->context1;
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
			mempool_free(sparam_mbox, phba->mbox_mem_pool);
			goto out;
		}
	}

	if (!(phba->hba_flag & HBA_FCOE_SUPPORT)) {
		cfglink_mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (!cfglink_mbox)
			goto out;
		vport->port_state = LPFC_LOCAL_CFG_LINK;
		lpfc_config_link(phba, cfglink_mbox);
		cfglink_mbox->vport = vport;
		cfglink_mbox->mbox_cmpl = lpfc_mbx_cmpl_local_config_link;
		rc = lpfc_sli_issue_mbox(phba, cfglink_mbox, MBX_NOWAIT);
		if (rc == MBX_NOT_FINISHED) {
			mempool_free(cfglink_mbox, phba->mbox_mem_pool);
			goto out;
		}
	} else {
		vport->port_state = LPFC_VPORT_UNKNOWN;
		/*
		 * Add the driver's default FCF record at FCF index 0 now. This
		 * is phase 1 implementation that support FCF index 0 and driver
		 * defaults.
		 */
		if (phba->cfg_enable_fip == 0) {
			fcf_record = kzalloc(sizeof(struct fcf_record),
					GFP_KERNEL);
			if (unlikely(!fcf_record)) {
				lpfc_printf_log(phba, KERN_ERR,
					LOG_MBOX | LOG_SLI,
					"2554 Could not allocate memmory for "
					"fcf record\n");
				rc = -ENODEV;
				goto out;
			}

			lpfc_sli4_build_dflt_fcf_record(phba, fcf_record,
						LPFC_FCOE_FCF_DEF_INDEX);
			rc = lpfc_sli4_add_fcf_record(phba, fcf_record);
			if (unlikely(rc)) {
				lpfc_printf_log(phba, KERN_ERR,
					LOG_MBOX | LOG_SLI,
					"2013 Could not manually add FCF "
					"record 0, status %d\n", rc);
				rc = -ENODEV;
				kfree(fcf_record);
				goto out;
			}
			kfree(fcf_record);
		}
		/*
		 * The driver is expected to do FIP/FCF. Call the port
		 * and get the FCF Table.
		 */
		spin_lock_irq(&phba->hbalock);
		if (phba->hba_flag & FCF_DISC_INPROGRESS) {
			spin_unlock_irq(&phba->hbalock);
			return;
		}
		spin_unlock_irq(&phba->hbalock);
		rc = lpfc_sli4_read_fcf_record(phba,
					LPFC_FCOE_FCF_GET_FIRST);
		if (rc)
			goto out;
	}

	return;
out:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
			 "0263 Discovery Mailbox error: state: 0x%x : %p %p\n",
			 vport->port_state, sparam_mbox, cfglink_mbox);
	lpfc_issue_clear_la(phba, vport);
	return;
}

static void
lpfc_enable_la(struct lpfc_hba *phba)
{
	uint32_t control;
	struct lpfc_sli *psli = &phba->sli;
	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_PROCESS_LA;
	if (phba->sli_rev <= LPFC_SLI_REV3) {
		control = readl(phba->HCregaddr);
		control |= HC_LAINT_ENA;
		writel(control, phba->HCregaddr);
		readl(phba->HCregaddr); /* flush */
	}
	spin_unlock_irq(&phba->hbalock);
}

static void
lpfc_mbx_issue_link_down(struct lpfc_hba *phba)
{
	lpfc_linkdown(phba);
	lpfc_enable_la(phba);
	lpfc_unregister_unused_fcf(phba);
	/* turn on Link Attention interrupts - no CLEAR_LA needed */
}


/*
 * This routine handles processing a READ_LA mailbox
 * command upon completion. It is setup in the LPFC_MBOXQ
 * as the completion routine when the command is
 * handed off to the SLI layer.
 */
void
lpfc_mbx_cmpl_read_la(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	READ_LA_VAR *la;
	MAILBOX_t *mb = &pmb->u.mb;
	struct lpfc_dmabuf *mp = (struct lpfc_dmabuf *) (pmb->context1);

	/* Unblock ELS traffic */
	phba->sli.ring[LPFC_ELS_RING].flag &= ~LPFC_STOP_IOCB_EVENT;
	/* Check for error */
	if (mb->mbxStatus) {
		lpfc_printf_log(phba, KERN_INFO, LOG_LINK_EVENT,
				"1307 READ_LA mbox error x%x state x%x\n",
				mb->mbxStatus, vport->port_state);
		lpfc_mbx_issue_link_down(phba);
		phba->link_state = LPFC_HBA_ERROR;
		goto lpfc_mbx_cmpl_read_la_free_mbuf;
	}

	la = (READ_LA_VAR *) &pmb->u.mb.un.varReadLA;

	memcpy(&phba->alpa_map[0], mp->virt, 128);

	spin_lock_irq(shost->host_lock);
	if (la->pb)
		vport->fc_flag |= FC_BYPASSED_MODE;
	else
		vport->fc_flag &= ~FC_BYPASSED_MODE;
	spin_unlock_irq(shost->host_lock);

	if ((phba->fc_eventTag  < la->eventTag) ||
	    (phba->fc_eventTag == la->eventTag)) {
		phba->fc_stat.LinkMultiEvent++;
		if (la->attType == AT_LINK_UP)
			if (phba->fc_eventTag != 0)
				lpfc_linkdown(phba);
	}

	phba->fc_eventTag = la->eventTag;
	if (la->mm)
		phba->sli.sli_flag |= LPFC_MENLO_MAINT;
	else
		phba->sli.sli_flag &= ~LPFC_MENLO_MAINT;

	if (la->attType == AT_LINK_UP && (!la->mm)) {
		phba->fc_stat.LinkUp++;
		if (phba->link_flag & LS_LOOPBACK_MODE) {
			lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
					"1306 Link Up Event in loop back mode "
					"x%x received Data: x%x x%x x%x x%x\n",
					la->eventTag, phba->fc_eventTag,
					la->granted_AL_PA, la->UlnkSpeed,
					phba->alpa_map[0]);
		} else {
			lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
					"1303 Link Up Event x%x received "
					"Data: x%x x%x x%x x%x x%x x%x %d\n",
					la->eventTag, phba->fc_eventTag,
					la->granted_AL_PA, la->UlnkSpeed,
					phba->alpa_map[0],
					la->mm, la->fa,
					phba->wait_4_mlo_maint_flg);
		}
		lpfc_mbx_process_link_up(phba, la);
	} else if (la->attType == AT_LINK_DOWN) {
		phba->fc_stat.LinkDown++;
		if (phba->link_flag & LS_LOOPBACK_MODE) {
			lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
				"1308 Link Down Event in loop back mode "
				"x%x received "
				"Data: x%x x%x x%x\n",
				la->eventTag, phba->fc_eventTag,
				phba->pport->port_state, vport->fc_flag);
		}
		else {
			lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
				"1305 Link Down Event x%x received "
				"Data: x%x x%x x%x x%x x%x\n",
				la->eventTag, phba->fc_eventTag,
				phba->pport->port_state, vport->fc_flag,
				la->mm, la->fa);
		}
		lpfc_mbx_issue_link_down(phba);
	}
	if (la->mm && la->attType == AT_LINK_UP) {
		if (phba->link_state != LPFC_LINK_DOWN) {
			phba->fc_stat.LinkDown++;
			lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
				"1312 Link Down Event x%x received "
				"Data: x%x x%x x%x\n",
				la->eventTag, phba->fc_eventTag,
				phba->pport->port_state, vport->fc_flag);
			lpfc_mbx_issue_link_down(phba);
		} else
			lpfc_enable_la(phba);

		lpfc_printf_log(phba, KERN_ERR, LOG_LINK_EVENT,
				"1310 Menlo Maint Mode Link up Event x%x rcvd "
				"Data: x%x x%x x%x\n",
				la->eventTag, phba->fc_eventTag,
				phba->pport->port_state, vport->fc_flag);
		/*
		 * The cmnd that triggered this will be waiting for this
		 * signal.
		 */
		/* WAKEUP for MENLO_SET_MODE or MENLO_RESET command. */
		if (phba->wait_4_mlo_maint_flg) {
			phba->wait_4_mlo_maint_flg = 0;
			wake_up_interruptible(&phba->wait_4_mlo_m_q);
		}
	}

	if (la->fa) {
		if (la->mm)
			lpfc_issue_clear_la(phba, vport);
		lpfc_printf_log(phba, KERN_INFO, LOG_LINK_EVENT,
				"1311 fa %d\n", la->fa);
	}

lpfc_mbx_cmpl_read_la_free_mbuf:
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mempool_free(pmb, phba->mbox_mem_pool);
	return;
}

/*
 * This routine handles processing a REG_LOGIN mailbox
 * command upon completion. It is setup in the LPFC_MBOXQ
 * as the completion routine when the command is
 * handed off to the SLI layer.
 */
void
lpfc_mbx_cmpl_reg_login(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport  *vport = pmb->vport;
	struct lpfc_dmabuf *mp = (struct lpfc_dmabuf *) (pmb->context1);
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) pmb->context2;

	pmb->context1 = NULL;

	/* Good status, call state machine */
	lpfc_disc_state_machine(vport, ndlp, pmb, NLP_EVT_CMPL_REG_LOGIN);
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mempool_free(pmb, phba->mbox_mem_pool);
	/* decrement the node reference count held for this callback
	 * function.
	 */
	lpfc_nlp_put(ndlp);

	return;
}

static void
lpfc_mbx_cmpl_unreg_vpi(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	MAILBOX_t *mb = &pmb->u.mb;
	struct lpfc_vport *vport = pmb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);

	switch (mb->mbxStatus) {
	case 0x0011:
	case 0x0020:
	case 0x9700:
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
				 "0911 cmpl_unreg_vpi, mb status = 0x%x\n",
				 mb->mbxStatus);
		break;
	}
	vport->unreg_vpi_cmpl = VPORT_OK;
	mempool_free(pmb, phba->mbox_mem_pool);
	/*
	 * This shost reference might have been taken at the beginning of
	 * lpfc_vport_delete()
	 */
	if (vport->load_flag & FC_UNLOADING)
		scsi_host_put(shost);
}

int
lpfc_mbx_unreg_vpi(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	int rc;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return 1;

	lpfc_unreg_vpi(phba, vport->vpi, mbox);
	mbox->vport = vport;
	mbox->mbox_cmpl = lpfc_mbx_cmpl_unreg_vpi;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX | LOG_VPORT,
				 "1800 Could not issue unreg_vpi\n");
		mempool_free(mbox, phba->mbox_mem_pool);
		vport->unreg_vpi_cmpl = VPORT_ERROR;
		return rc;
	}
	return 0;
}

static void
lpfc_mbx_cmpl_reg_vpi(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	MAILBOX_t *mb = &pmb->u.mb;

	switch (mb->mbxStatus) {
	case 0x0011:
	case 0x9601:
	case 0x9602:
		lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
				 "0912 cmpl_reg_vpi, mb status = 0x%x\n",
				 mb->mbxStatus);
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
		spin_unlock_irq(shost->host_lock);
		vport->fc_myDID = 0;
		goto out;
	}

	vport->num_disc_nodes = 0;
	/* go thru NPR list and issue ELS PLOGIs */
	if (vport->fc_npr_cnt)
		lpfc_els_disc_plogi(vport);

	if (!vport->num_disc_nodes) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~FC_NDISC_ACTIVE;
		spin_unlock_irq(shost->host_lock);
		lpfc_can_disctmo(vport);
	}
	vport->port_state = LPFC_VPORT_READY;

out:
	mempool_free(pmb, phba->mbox_mem_pool);
	return;
}

/**
 * lpfc_create_static_vport - Read HBA config region to create static vports.
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine issue a DUMP mailbox command for config region 22 to get
 * the list of static vports to be created. The function create vports
 * based on the information returned from the HBA.
 **/
void
lpfc_create_static_vport(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *pmb = NULL;
	MAILBOX_t *mb;
	struct static_vport_info *vport_info;
	int rc = 0, i;
	struct fc_vport_identifiers vport_id;
	struct fc_vport *new_fc_vport;
	struct Scsi_Host *shost;
	struct lpfc_vport *vport;
	uint16_t offset = 0;
	uint8_t *vport_buff;
	struct lpfc_dmabuf *mp;
	uint32_t byte_count = 0;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0542 lpfc_create_static_vport failed to"
				" allocate mailbox memory\n");
		return;
	}

	mb = &pmb->u.mb;

	vport_info = kzalloc(sizeof(struct static_vport_info), GFP_KERNEL);
	if (!vport_info) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0543 lpfc_create_static_vport failed to"
				" allocate vport_info\n");
		mempool_free(pmb, phba->mbox_mem_pool);
		return;
	}

	vport_buff = (uint8_t *) vport_info;
	do {
		if (lpfc_dump_static_vport(phba, pmb, offset))
			goto out;

		pmb->vport = phba->pport;
		rc = lpfc_sli_issue_mbox_wait(phba, pmb, LPFC_MBOX_TMO);

		if ((rc != MBX_SUCCESS) || mb->mbxStatus) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0544 lpfc_create_static_vport failed to"
				" issue dump mailbox command ret 0x%x "
				"status 0x%x\n",
				rc, mb->mbxStatus);
			goto out;
		}

		if (phba->sli_rev == LPFC_SLI_REV4) {
			byte_count = pmb->u.mqe.un.mb_words[5];
			mp = (struct lpfc_dmabuf *) pmb->context2;
			if (byte_count > sizeof(struct static_vport_info) -
					offset)
				byte_count = sizeof(struct static_vport_info)
					- offset;
			memcpy(vport_buff + offset, mp->virt, byte_count);
			offset += byte_count;
		} else {
			if (mb->un.varDmp.word_cnt >
				sizeof(struct static_vport_info) - offset)
				mb->un.varDmp.word_cnt =
					sizeof(struct static_vport_info)
						- offset;
			byte_count = mb->un.varDmp.word_cnt;
			lpfc_sli_pcimem_bcopy(((uint8_t *)mb) + DMP_RSP_OFFSET,
				vport_buff + offset,
				byte_count);

			offset += byte_count;
		}

	} while (byte_count &&
		offset < sizeof(struct static_vport_info));


	if ((le32_to_cpu(vport_info->signature) != VPORT_INFO_SIG) ||
		((le32_to_cpu(vport_info->rev) & VPORT_INFO_REV_MASK)
			!= VPORT_INFO_REV)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"0545 lpfc_create_static_vport bad"
			" information header 0x%x 0x%x\n",
			le32_to_cpu(vport_info->signature),
			le32_to_cpu(vport_info->rev) & VPORT_INFO_REV_MASK);

		goto out;
	}

	shost = lpfc_shost_from_vport(phba->pport);

	for (i = 0; i < MAX_STATIC_VPORT_COUNT; i++) {
		memset(&vport_id, 0, sizeof(vport_id));
		vport_id.port_name = wwn_to_u64(vport_info->vport_list[i].wwpn);
		vport_id.node_name = wwn_to_u64(vport_info->vport_list[i].wwnn);
		if (!vport_id.port_name || !vport_id.node_name)
			continue;

		vport_id.roles = FC_PORT_ROLE_FCP_INITIATOR;
		vport_id.vport_type = FC_PORTTYPE_NPIV;
		vport_id.disable = false;
		new_fc_vport = fc_vport_create(shost, 0, &vport_id);

		if (!new_fc_vport) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0546 lpfc_create_static_vport failed to"
				" create vport\n");
			continue;
		}

		vport = *(struct lpfc_vport **)new_fc_vport->dd_data;
		vport->vport_flag |= STATIC_VPORT;
	}

out:
	kfree(vport_info);
	if (rc != MBX_TIMEOUT) {
		if (pmb->context2) {
			mp = (struct lpfc_dmabuf *) pmb->context2;
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
		}
		mempool_free(pmb, phba->mbox_mem_pool);
	}

	return;
}

/*
 * This routine handles processing a Fabric REG_LOGIN mailbox
 * command upon completion. It is setup in the LPFC_MBOXQ
 * as the completion routine when the command is
 * handed off to the SLI layer.
 */
void
lpfc_mbx_cmpl_fabric_reg_login(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport *vport = pmb->vport;
	MAILBOX_t *mb = &pmb->u.mb;
	struct lpfc_dmabuf *mp = (struct lpfc_dmabuf *) (pmb->context1);
	struct lpfc_nodelist *ndlp;

	ndlp = (struct lpfc_nodelist *) pmb->context2;
	pmb->context1 = NULL;
	pmb->context2 = NULL;
	if (mb->mbxStatus) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX,
				 "0258 Register Fabric login error: 0x%x\n",
				 mb->mbxStatus);
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
		mempool_free(pmb, phba->mbox_mem_pool);

		if (phba->fc_topology == TOPOLOGY_LOOP) {
			/* FLOGI failed, use loop map to make discovery list */
			lpfc_disc_list_loopmap(vport);

			/* Start discovery */
			lpfc_disc_start(vport);
			/* Decrement the reference count to ndlp after the
			 * reference to the ndlp are done.
			 */
			lpfc_nlp_put(ndlp);
			return;
		}

		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		/* Decrement the reference count to ndlp after the reference
		 * to the ndlp are done.
		 */
		lpfc_nlp_put(ndlp);
		return;
	}

	ndlp->nlp_rpi = mb->un.varWords[0];
	ndlp->nlp_flag |= NLP_RPI_VALID;
	ndlp->nlp_type |= NLP_FABRIC;
	lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);

	if (vport->port_state == LPFC_FABRIC_CFG_LINK) {
		lpfc_start_fdiscs(phba);
		lpfc_do_scr_ns_plogi(phba, vport);
	}

	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mempool_free(pmb, phba->mbox_mem_pool);

	/* Drop the reference count from the mbox at the end after
	 * all the current reference to the ndlp have been done.
	 */
	lpfc_nlp_put(ndlp);
	return;
}

/*
 * This routine handles processing a NameServer REG_LOGIN mailbox
 * command upon completion. It is setup in the LPFC_MBOXQ
 * as the completion routine when the command is
 * handed off to the SLI layer.
 */
void
lpfc_mbx_cmpl_ns_reg_login(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	MAILBOX_t *mb = &pmb->u.mb;
	struct lpfc_dmabuf *mp = (struct lpfc_dmabuf *) (pmb->context1);
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) pmb->context2;
	struct lpfc_vport *vport = pmb->vport;

	if (mb->mbxStatus) {
out:
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0260 Register NameServer error: 0x%x\n",
				 mb->mbxStatus);
		/* decrement the node reference count held for this
		 * callback function.
		 */
		lpfc_nlp_put(ndlp);
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
		mempool_free(pmb, phba->mbox_mem_pool);

		/* If no other thread is using the ndlp, free it */
		lpfc_nlp_not_used(ndlp);

		if (phba->fc_topology == TOPOLOGY_LOOP) {
			/*
			 * RegLogin failed, use loop map to make discovery
			 * list
			 */
			lpfc_disc_list_loopmap(vport);

			/* Start discovery */
			lpfc_disc_start(vport);
			return;
		}
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
		return;
	}

	pmb->context1 = NULL;

	ndlp->nlp_rpi = mb->un.varWords[0];
	ndlp->nlp_flag |= NLP_RPI_VALID;
	ndlp->nlp_type |= NLP_FABRIC;
	lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);

	if (vport->port_state < LPFC_VPORT_READY) {
		/* Link up discovery requires Fabric registration. */
		lpfc_ns_cmd(vport, SLI_CTNS_RFF_ID, 0, 0); /* Do this first! */
		lpfc_ns_cmd(vport, SLI_CTNS_RNN_ID, 0, 0);
		lpfc_ns_cmd(vport, SLI_CTNS_RSNN_NN, 0, 0);
		lpfc_ns_cmd(vport, SLI_CTNS_RSPN_ID, 0, 0);
		lpfc_ns_cmd(vport, SLI_CTNS_RFT_ID, 0, 0);

		/* Issue SCR just before NameServer GID_FT Query */
		lpfc_issue_els_scr(vport, SCR_DID, 0);
	}

	vport->fc_ns_retry = 0;
	/* Good status, issue CT Request to NameServer */
	if (lpfc_ns_cmd(vport, SLI_CTNS_GID_FT, 0, 0)) {
		/* Cannot issue NameServer Query, so finish up discovery */
		goto out;
	}

	/* decrement the node reference count held for this
	 * callback function.
	 */
	lpfc_nlp_put(ndlp);
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mempool_free(pmb, phba->mbox_mem_pool);

	return;
}

static void
lpfc_register_remote_port(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct fc_rport  *rport;
	struct lpfc_rport_data *rdata;
	struct fc_rport_identifiers rport_ids;
	struct lpfc_hba  *phba = vport->phba;

	/* Remote port has reappeared. Re-register w/ FC transport */
	rport_ids.node_name = wwn_to_u64(ndlp->nlp_nodename.u.wwn);
	rport_ids.port_name = wwn_to_u64(ndlp->nlp_portname.u.wwn);
	rport_ids.port_id = ndlp->nlp_DID;
	rport_ids.roles = FC_RPORT_ROLE_UNKNOWN;

	/*
	 * We leave our node pointer in rport->dd_data when we unregister a
	 * FCP target port.  But fc_remote_port_add zeros the space to which
	 * rport->dd_data points.  So, if we're reusing a previously
	 * registered port, drop the reference that we took the last time we
	 * registered the port.
	 */
	if (ndlp->rport && ndlp->rport->dd_data &&
	    ((struct lpfc_rport_data *) ndlp->rport->dd_data)->pnode == ndlp)
		lpfc_nlp_put(ndlp);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_RPORT,
		"rport add:       did:x%x flg:x%x type x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_type);

	ndlp->rport = rport = fc_remote_port_add(shost, 0, &rport_ids);
	if (!rport || !get_device(&rport->dev)) {
		dev_printk(KERN_WARNING, &phba->pcidev->dev,
			   "Warning: fc_remote_port_add failed\n");
		return;
	}

	/* initialize static port data */
	rport->maxframe_size = ndlp->nlp_maxframe;
	rport->supported_classes = ndlp->nlp_class_sup;
	rdata = rport->dd_data;
	rdata->pnode = lpfc_nlp_get(ndlp);

	if (ndlp->nlp_type & NLP_FCP_TARGET)
		rport_ids.roles |= FC_RPORT_ROLE_FCP_TARGET;
	if (ndlp->nlp_type & NLP_FCP_INITIATOR)
		rport_ids.roles |= FC_RPORT_ROLE_FCP_INITIATOR;


	if (rport_ids.roles !=  FC_RPORT_ROLE_UNKNOWN)
		fc_remote_port_rolechg(rport, rport_ids.roles);

	if ((rport->scsi_target_id != -1) &&
	    (rport->scsi_target_id < LPFC_MAX_TARGET)) {
		ndlp->nlp_sid = rport->scsi_target_id;
	}
	return;
}

static void
lpfc_unregister_remote_port(struct lpfc_nodelist *ndlp)
{
	struct fc_rport *rport = ndlp->rport;

	lpfc_debugfs_disc_trc(ndlp->vport, LPFC_DISC_TRC_RPORT,
		"rport delete:    did:x%x flg:x%x type x%x",
		ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_type);

	fc_remote_port_delete(rport);

	return;
}

static void
lpfc_nlp_counters(struct lpfc_vport *vport, int state, int count)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	spin_lock_irq(shost->host_lock);
	switch (state) {
	case NLP_STE_UNUSED_NODE:
		vport->fc_unused_cnt += count;
		break;
	case NLP_STE_PLOGI_ISSUE:
		vport->fc_plogi_cnt += count;
		break;
	case NLP_STE_ADISC_ISSUE:
		vport->fc_adisc_cnt += count;
		break;
	case NLP_STE_REG_LOGIN_ISSUE:
		vport->fc_reglogin_cnt += count;
		break;
	case NLP_STE_PRLI_ISSUE:
		vport->fc_prli_cnt += count;
		break;
	case NLP_STE_UNMAPPED_NODE:
		vport->fc_unmap_cnt += count;
		break;
	case NLP_STE_MAPPED_NODE:
		vport->fc_map_cnt += count;
		break;
	case NLP_STE_NPR_NODE:
		vport->fc_npr_cnt += count;
		break;
	}
	spin_unlock_irq(shost->host_lock);
}

static void
lpfc_nlp_state_cleanup(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		       int old_state, int new_state)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (new_state == NLP_STE_UNMAPPED_NODE) {
		ndlp->nlp_type &= ~(NLP_FCP_TARGET | NLP_FCP_INITIATOR);
		ndlp->nlp_flag &= ~NLP_NODEV_REMOVE;
		ndlp->nlp_type |= NLP_FC_NODE;
	}
	if (new_state == NLP_STE_MAPPED_NODE)
		ndlp->nlp_flag &= ~NLP_NODEV_REMOVE;
	if (new_state == NLP_STE_NPR_NODE)
		ndlp->nlp_flag &= ~NLP_RCV_PLOGI;

	/* Transport interface */
	if (ndlp->rport && (old_state == NLP_STE_MAPPED_NODE ||
			    old_state == NLP_STE_UNMAPPED_NODE)) {
		vport->phba->nport_event_cnt++;
		lpfc_unregister_remote_port(ndlp);
	}

	if (new_state ==  NLP_STE_MAPPED_NODE ||
	    new_state == NLP_STE_UNMAPPED_NODE) {
		vport->phba->nport_event_cnt++;
		/*
		 * Tell the fc transport about the port, if we haven't
		 * already. If we have, and it's a scsi entity, be
		 * sure to unblock any attached scsi devices
		 */
		lpfc_register_remote_port(vport, ndlp);
	}
	if ((new_state ==  NLP_STE_MAPPED_NODE) &&
		(vport->stat_data_enabled)) {
		/*
		 * A new target is discovered, if there is no buffer for
		 * statistical data collection allocate buffer.
		 */
		ndlp->lat_data = kcalloc(LPFC_MAX_BUCKET_COUNT,
					 sizeof(struct lpfc_scsicmd_bkt),
					 GFP_KERNEL);

		if (!ndlp->lat_data)
			lpfc_printf_vlog(vport, KERN_ERR, LOG_NODE,
				"0286 lpfc_nlp_state_cleanup failed to "
				"allocate statistical data buffer DID "
				"0x%x\n", ndlp->nlp_DID);
	}
	/*
	 * if we added to Mapped list, but the remote port
	 * registration failed or assigned a target id outside
	 * our presentable range - move the node to the
	 * Unmapped List
	 */
	if (new_state == NLP_STE_MAPPED_NODE &&
	    (!ndlp->rport ||
	     ndlp->rport->scsi_target_id == -1 ||
	     ndlp->rport->scsi_target_id >= LPFC_MAX_TARGET)) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_TGT_NO_SCSIID;
		spin_unlock_irq(shost->host_lock);
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);
	}
}

static char *
lpfc_nlp_state_name(char *buffer, size_t size, int state)
{
	static char *states[] = {
		[NLP_STE_UNUSED_NODE] = "UNUSED",
		[NLP_STE_PLOGI_ISSUE] = "PLOGI",
		[NLP_STE_ADISC_ISSUE] = "ADISC",
		[NLP_STE_REG_LOGIN_ISSUE] = "REGLOGIN",
		[NLP_STE_PRLI_ISSUE] = "PRLI",
		[NLP_STE_UNMAPPED_NODE] = "UNMAPPED",
		[NLP_STE_MAPPED_NODE] = "MAPPED",
		[NLP_STE_NPR_NODE] = "NPR",
	};

	if (state < NLP_STE_MAX_STATE && states[state])
		strlcpy(buffer, states[state], size);
	else
		snprintf(buffer, size, "unknown (%d)", state);
	return buffer;
}

void
lpfc_nlp_set_state(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		   int state)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	int  old_state = ndlp->nlp_state;
	char name1[16], name2[16];

	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
			 "0904 NPort state transition x%06x, %s -> %s\n",
			 ndlp->nlp_DID,
			 lpfc_nlp_state_name(name1, sizeof(name1), old_state),
			 lpfc_nlp_state_name(name2, sizeof(name2), state));

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_NODE,
		"node statechg    did:x%x old:%d ste:%d",
		ndlp->nlp_DID, old_state, state);

	if (old_state == NLP_STE_NPR_NODE &&
	    state != NLP_STE_NPR_NODE)
		lpfc_cancel_retry_delay_tmo(vport, ndlp);
	if (old_state == NLP_STE_UNMAPPED_NODE) {
		ndlp->nlp_flag &= ~NLP_TGT_NO_SCSIID;
		ndlp->nlp_type &= ~NLP_FC_NODE;
	}

	if (list_empty(&ndlp->nlp_listp)) {
		spin_lock_irq(shost->host_lock);
		list_add_tail(&ndlp->nlp_listp, &vport->fc_nodes);
		spin_unlock_irq(shost->host_lock);
	} else if (old_state)
		lpfc_nlp_counters(vport, old_state, -1);

	ndlp->nlp_state = state;
	lpfc_nlp_counters(vport, state, 1);
	lpfc_nlp_state_cleanup(vport, ndlp, old_state, state);
}

void
lpfc_enqueue_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (list_empty(&ndlp->nlp_listp)) {
		spin_lock_irq(shost->host_lock);
		list_add_tail(&ndlp->nlp_listp, &vport->fc_nodes);
		spin_unlock_irq(shost->host_lock);
	}
}

void
lpfc_dequeue_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	lpfc_cancel_retry_delay_tmo(vport, ndlp);
	if (ndlp->nlp_state && !list_empty(&ndlp->nlp_listp))
		lpfc_nlp_counters(vport, ndlp->nlp_state, -1);
	spin_lock_irq(shost->host_lock);
	list_del_init(&ndlp->nlp_listp);
	spin_unlock_irq(shost->host_lock);
	lpfc_nlp_state_cleanup(vport, ndlp, ndlp->nlp_state,
				NLP_STE_UNUSED_NODE);
}

static void
lpfc_disable_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	lpfc_cancel_retry_delay_tmo(vport, ndlp);
	if (ndlp->nlp_state && !list_empty(&ndlp->nlp_listp))
		lpfc_nlp_counters(vport, ndlp->nlp_state, -1);
	lpfc_nlp_state_cleanup(vport, ndlp, ndlp->nlp_state,
				NLP_STE_UNUSED_NODE);
}
/**
 * lpfc_initialize_node - Initialize all fields of node object
 * @vport: Pointer to Virtual Port object.
 * @ndlp: Pointer to FC node object.
 * @did: FC_ID of the node.
 *
 * This function is always called when node object need to be initialized.
 * It initializes all the fields of the node object. Although the reference
 * to phba from @ndlp can be obtained indirectly through it's reference to
 * @vport, a direct reference to phba is taken here by @ndlp. This is due
 * to the life-span of the @ndlp might go beyond the existence of @vport as
 * the final release of ndlp is determined by its reference count. And, the
 * operation on @ndlp needs the reference to phba.
 **/
static inline void
lpfc_initialize_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
	uint32_t did)
{
	INIT_LIST_HEAD(&ndlp->els_retry_evt.evt_listp);
	INIT_LIST_HEAD(&ndlp->dev_loss_evt.evt_listp);
	init_timer(&ndlp->nlp_delayfunc);
	ndlp->nlp_delayfunc.function = lpfc_els_retry_delay;
	ndlp->nlp_delayfunc.data = (unsigned long)ndlp;
	ndlp->nlp_DID = did;
	ndlp->vport = vport;
	ndlp->phba = vport->phba;
	ndlp->nlp_sid = NLP_NO_SID;
	kref_init(&ndlp->kref);
	NLP_INT_NODE_ACT(ndlp);
	atomic_set(&ndlp->cmd_pending, 0);
	ndlp->cmd_qdepth = LPFC_MAX_TGT_QDEPTH;
}

struct lpfc_nodelist *
lpfc_enable_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		 int state)
{
	struct lpfc_hba *phba = vport->phba;
	uint32_t did;
	unsigned long flags;

	if (!ndlp)
		return NULL;

	spin_lock_irqsave(&phba->ndlp_lock, flags);
	/* The ndlp should not be in memory free mode */
	if (NLP_CHK_FREE_REQ(ndlp)) {
		spin_unlock_irqrestore(&phba->ndlp_lock, flags);
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_NODE,
				"0277 lpfc_enable_node: ndlp:x%p "
				"usgmap:x%x refcnt:%d\n",
				(void *)ndlp, ndlp->nlp_usg_map,
				atomic_read(&ndlp->kref.refcount));
		return NULL;
	}
	/* The ndlp should not already be in active mode */
	if (NLP_CHK_NODE_ACT(ndlp)) {
		spin_unlock_irqrestore(&phba->ndlp_lock, flags);
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_NODE,
				"0278 lpfc_enable_node: ndlp:x%p "
				"usgmap:x%x refcnt:%d\n",
				(void *)ndlp, ndlp->nlp_usg_map,
				atomic_read(&ndlp->kref.refcount));
		return NULL;
	}

	/* Keep the original DID */
	did = ndlp->nlp_DID;

	/* re-initialize ndlp except of ndlp linked list pointer */
	memset((((char *)ndlp) + sizeof (struct list_head)), 0,
		sizeof (struct lpfc_nodelist) - sizeof (struct list_head));
	lpfc_initialize_node(vport, ndlp, did);

	spin_unlock_irqrestore(&phba->ndlp_lock, flags);

	if (state != NLP_STE_UNUSED_NODE)
		lpfc_nlp_set_state(vport, ndlp, state);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_NODE,
		"node enable:       did:x%x",
		ndlp->nlp_DID, 0, 0);
	return ndlp;
}

void
lpfc_drop_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	/*
	 * Use of lpfc_drop_node and UNUSED list: lpfc_drop_node should
	 * be used if we wish to issue the "last" lpfc_nlp_put() to remove
	 * the ndlp from the vport. The ndlp marked as UNUSED on the list
	 * until ALL other outstanding threads have completed. We check
	 * that the ndlp not already in the UNUSED state before we proceed.
	 */
	if (ndlp->nlp_state == NLP_STE_UNUSED_NODE)
		return;
	lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNUSED_NODE);
	lpfc_nlp_put(ndlp);
	return;
}

/*
 * Start / ReStart rescue timer for Discovery / RSCN handling
 */
void
lpfc_set_disctmo(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	uint32_t tmo;

	if (vport->port_state == LPFC_LOCAL_CFG_LINK) {
		/* For FAN, timeout should be greater than edtov */
		tmo = (((phba->fc_edtov + 999) / 1000) + 1);
	} else {
		/* Normal discovery timeout should be > than ELS/CT timeout
		 * FC spec states we need 3 * ratov for CT requests
		 */
		tmo = ((phba->fc_ratov * 3) + 3);
	}


	if (!timer_pending(&vport->fc_disctmo)) {
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
			"set disc timer:  tmo:x%x state:x%x flg:x%x",
			tmo, vport->port_state, vport->fc_flag);
	}

	mod_timer(&vport->fc_disctmo, jiffies + HZ * tmo);
	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_DISC_TMO;
	spin_unlock_irq(shost->host_lock);

	/* Start Discovery Timer state <hba_state> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0247 Start Discovery Timer state x%x "
			 "Data: x%x x%lx x%x x%x\n",
			 vport->port_state, tmo,
			 (unsigned long)&vport->fc_disctmo, vport->fc_plogi_cnt,
			 vport->fc_adisc_cnt);

	return;
}

/*
 * Cancel rescue timer for Discovery / RSCN handling
 */
int
lpfc_can_disctmo(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	unsigned long iflags;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"can disc timer:  state:x%x rtry:x%x flg:x%x",
		vport->port_state, vport->fc_ns_retry, vport->fc_flag);

	/* Turn off discovery timer if its running */
	if (vport->fc_flag & FC_DISC_TMO) {
		spin_lock_irqsave(shost->host_lock, iflags);
		vport->fc_flag &= ~FC_DISC_TMO;
		spin_unlock_irqrestore(shost->host_lock, iflags);
		del_timer_sync(&vport->fc_disctmo);
		spin_lock_irqsave(&vport->work_port_lock, iflags);
		vport->work_port_events &= ~WORKER_DISC_TMO;
		spin_unlock_irqrestore(&vport->work_port_lock, iflags);
	}

	/* Cancel Discovery Timer state <hba_state> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0248 Cancel Discovery Timer state x%x "
			 "Data: x%x x%x x%x\n",
			 vport->port_state, vport->fc_flag,
			 vport->fc_plogi_cnt, vport->fc_adisc_cnt);
	return 0;
}

/*
 * Check specified ring for outstanding IOCB on the SLI queue
 * Return true if iocb matches the specified nport
 */
int
lpfc_check_sli_ndlp(struct lpfc_hba *phba,
		    struct lpfc_sli_ring *pring,
		    struct lpfc_iocbq *iocb,
		    struct lpfc_nodelist *ndlp)
{
	struct lpfc_sli *psli = &phba->sli;
	IOCB_t *icmd = &iocb->iocb;
	struct lpfc_vport    *vport = ndlp->vport;

	if (iocb->vport != vport)
		return 0;

	if (pring->ringno == LPFC_ELS_RING) {
		switch (icmd->ulpCommand) {
		case CMD_GEN_REQUEST64_CR:
			if (iocb->context_un.ndlp == ndlp)
				return 1;
		case CMD_ELS_REQUEST64_CR:
			if (icmd->un.elsreq64.remoteID == ndlp->nlp_DID)
				return 1;
		case CMD_XMIT_ELS_RSP64_CX:
			if (iocb->context1 == (uint8_t *) ndlp)
				return 1;
		}
	} else if (pring->ringno == psli->extra_ring) {

	} else if (pring->ringno == psli->fcp_ring) {
		/* Skip match check if waiting to relogin to FCP target */
		if ((ndlp->nlp_type & NLP_FCP_TARGET) &&
		    (ndlp->nlp_flag & NLP_DELAY_TMO)) {
			return 0;
		}
		if (icmd->ulpContext == (volatile ushort)ndlp->nlp_rpi) {
			return 1;
		}
	} else if (pring->ringno == psli->next_ring) {

	}
	return 0;
}

/*
 * Free resources / clean up outstanding I/Os
 * associated with nlp_rpi in the LPFC_NODELIST entry.
 */
static int
lpfc_no_rpi(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp)
{
	LIST_HEAD(completions);
	struct lpfc_sli *psli;
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *iocb, *next_iocb;
	uint32_t rpi, i;

	lpfc_fabric_abort_nport(ndlp);

	/*
	 * Everything that matches on txcmplq will be returned
	 * by firmware with a no rpi error.
	 */
	psli = &phba->sli;
	rpi = ndlp->nlp_rpi;
	if (ndlp->nlp_flag & NLP_RPI_VALID) {
		/* Now process each ring */
		for (i = 0; i < psli->num_rings; i++) {
			pring = &psli->ring[i];

			spin_lock_irq(&phba->hbalock);
			list_for_each_entry_safe(iocb, next_iocb, &pring->txq,
						 list) {
				/*
				 * Check to see if iocb matches the nport we are
				 * looking for
				 */
				if ((lpfc_check_sli_ndlp(phba, pring, iocb,
							 ndlp))) {
					/* It matches, so deque and call compl
					   with an error */
					list_move_tail(&iocb->list,
						       &completions);
					pring->txq_cnt--;
				}
			}
			spin_unlock_irq(&phba->hbalock);
		}
	}

	/* Cancel all the IOCBs from the completions list */
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);

	return 0;
}

/*
 * Free rpi associated with LPFC_NODELIST entry.
 * This routine is called from lpfc_freenode(), when we are removing
 * a LPFC_NODELIST entry. It is also called if the driver initiates a
 * LOGO that completes successfully, and we are waiting to PLOGI back
 * to the remote NPort. In addition, it is called after we receive
 * and unsolicated ELS cmd, send back a rsp, the rsp completes and
 * we are waiting to PLOGI back to the remote NPort.
 */
int
lpfc_unreg_rpi(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba *phba = vport->phba;
	LPFC_MBOXQ_t    *mbox;
	int rc;

	if (ndlp->nlp_flag & NLP_RPI_VALID) {
		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (mbox) {
			lpfc_unreg_login(phba, vport->vpi, ndlp->nlp_rpi, mbox);
			mbox->vport = vport;
			mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
			rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
			if (rc == MBX_NOT_FINISHED)
				mempool_free(mbox, phba->mbox_mem_pool);
		}
		lpfc_no_rpi(phba, ndlp);
		ndlp->nlp_rpi = 0;
		ndlp->nlp_flag &= ~NLP_RPI_VALID;
		ndlp->nlp_flag &= ~NLP_NPR_ADISC;
		return 1;
	}
	return 0;
}

void
lpfc_unreg_all_rpis(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba  = vport->phba;
	LPFC_MBOXQ_t     *mbox;
	int rc;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mbox) {
		lpfc_unreg_login(phba, vport->vpi, 0xffff, mbox);
		mbox->vport = vport;
		mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		mbox->context1 = NULL;
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, LPFC_MBOX_TMO);
		if (rc != MBX_TIMEOUT)
			mempool_free(mbox, phba->mbox_mem_pool);

		if ((rc == MBX_TIMEOUT) || (rc == MBX_NOT_FINISHED))
			lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX | LOG_VPORT,
				"1836 Could not issue "
				"unreg_login(all_rpis) status %d\n", rc);
	}
}

void
lpfc_unreg_default_rpis(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba  = vport->phba;
	LPFC_MBOXQ_t     *mbox;
	int rc;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mbox) {
		lpfc_unreg_did(phba, vport->vpi, 0xffffffff, mbox);
		mbox->vport = vport;
		mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		mbox->context1 = NULL;
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, LPFC_MBOX_TMO);
		if (rc != MBX_TIMEOUT)
			mempool_free(mbox, phba->mbox_mem_pool);

		if ((rc == MBX_TIMEOUT) || (rc == MBX_NOT_FINISHED))
			lpfc_printf_vlog(vport, KERN_ERR, LOG_MBOX | LOG_VPORT,
					 "1815 Could not issue "
					 "unreg_did (default rpis) status %d\n",
					 rc);
	}
}

/*
 * Free resources associated with LPFC_NODELIST entry
 * so it can be freed.
 */
static int
lpfc_cleanup_node(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mb, *nextmb;
	struct lpfc_dmabuf *mp;

	/* Cleanup node for NPort <nlp_DID> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
			 "0900 Cleanup node for NPort x%x "
			 "Data: x%x x%x x%x\n",
			 ndlp->nlp_DID, ndlp->nlp_flag,
			 ndlp->nlp_state, ndlp->nlp_rpi);
	if (NLP_CHK_FREE_REQ(ndlp)) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_NODE,
				"0280 lpfc_cleanup_node: ndlp:x%p "
				"usgmap:x%x refcnt:%d\n",
				(void *)ndlp, ndlp->nlp_usg_map,
				atomic_read(&ndlp->kref.refcount));
		lpfc_dequeue_node(vport, ndlp);
	} else {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_NODE,
				"0281 lpfc_cleanup_node: ndlp:x%p "
				"usgmap:x%x refcnt:%d\n",
				(void *)ndlp, ndlp->nlp_usg_map,
				atomic_read(&ndlp->kref.refcount));
		lpfc_disable_node(vport, ndlp);
	}

	/* cleanup any ndlp on mbox q waiting for reglogin cmpl */
	if ((mb = phba->sli.mbox_active)) {
		if ((mb->u.mb.mbxCommand == MBX_REG_LOGIN64) &&
		   (ndlp == (struct lpfc_nodelist *) mb->context2)) {
			mb->context2 = NULL;
			mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		}
	}

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(mb, nextmb, &phba->sli.mboxq, list) {
		if ((mb->u.mb.mbxCommand == MBX_REG_LOGIN64) &&
		    (ndlp == (struct lpfc_nodelist *) mb->context2)) {
			mp = (struct lpfc_dmabuf *) (mb->context1);
			if (mp) {
				__lpfc_mbuf_free(phba, mp->virt, mp->phys);
				kfree(mp);
			}
			list_del(&mb->list);
			mempool_free(mb, phba->mbox_mem_pool);
			/* We shall not invoke the lpfc_nlp_put to decrement
			 * the ndlp reference count as we are in the process
			 * of lpfc_nlp_release.
			 */
		}
	}
	spin_unlock_irq(&phba->hbalock);

	lpfc_els_abort(phba, ndlp);

	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(shost->host_lock);

	ndlp->nlp_last_elscmd = 0;
	del_timer_sync(&ndlp->nlp_delayfunc);

	list_del_init(&ndlp->els_retry_evt.evt_listp);
	list_del_init(&ndlp->dev_loss_evt.evt_listp);

	lpfc_unreg_rpi(vport, ndlp);

	return 0;
}

/*
 * Check to see if we can free the nlp back to the freelist.
 * If we are in the middle of using the nlp in the discovery state
 * machine, defer the free till we reach the end of the state machine.
 */
static void
lpfc_nlp_remove(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)
{
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_rport_data *rdata;
	LPFC_MBOXQ_t *mbox;
	int rc;

	lpfc_cancel_retry_delay_tmo(vport, ndlp);
	if ((ndlp->nlp_flag & NLP_DEFER_RM) &&
	    !(ndlp->nlp_flag & NLP_RPI_VALID)) {
		/* For this case we need to cleanup the default rpi
		 * allocated by the firmware.
		 */
		if ((mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL))
			!= NULL) {
			rc = lpfc_reg_rpi(phba, vport->vpi, ndlp->nlp_DID,
			    (uint8_t *) &vport->fc_sparam, mbox, 0);
			if (rc) {
				mempool_free(mbox, phba->mbox_mem_pool);
			}
			else {
				mbox->mbox_flag |= LPFC_MBX_IMED_UNREG;
				mbox->mbox_cmpl = lpfc_mbx_cmpl_dflt_rpi;
				mbox->vport = vport;
				mbox->context2 = NULL;
				rc =lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
				if (rc == MBX_NOT_FINISHED) {
					mempool_free(mbox, phba->mbox_mem_pool);
				}
			}
		}
	}
	lpfc_cleanup_node(vport, ndlp);

	/*
	 * We can get here with a non-NULL ndlp->rport because when we
	 * unregister a rport we don't break the rport/node linkage.  So if we
	 * do, make sure we don't leaving any dangling pointers behind.
	 */
	if (ndlp->rport) {
		rdata = ndlp->rport->dd_data;
		rdata->pnode = NULL;
		ndlp->rport = NULL;
	}
}

static int
lpfc_matchdid(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
	      uint32_t did)
{
	D_ID mydid, ndlpdid, matchdid;

	if (did == Bcast_DID)
		return 0;

	/* First check for Direct match */
	if (ndlp->nlp_DID == did)
		return 1;

	/* Next check for area/domain identically equals 0 match */
	mydid.un.word = vport->fc_myDID;
	if ((mydid.un.b.domain == 0) && (mydid.un.b.area == 0)) {
		return 0;
	}

	matchdid.un.word = did;
	ndlpdid.un.word = ndlp->nlp_DID;
	if (matchdid.un.b.id == ndlpdid.un.b.id) {
		if ((mydid.un.b.domain == matchdid.un.b.domain) &&
		    (mydid.un.b.area == matchdid.un.b.area)) {
			if ((ndlpdid.un.b.domain == 0) &&
			    (ndlpdid.un.b.area == 0)) {
				if (ndlpdid.un.b.id)
					return 1;
			}
			return 0;
		}

		matchdid.un.word = ndlp->nlp_DID;
		if ((mydid.un.b.domain == ndlpdid.un.b.domain) &&
		    (mydid.un.b.area == ndlpdid.un.b.area)) {
			if ((matchdid.un.b.domain == 0) &&
			    (matchdid.un.b.area == 0)) {
				if (matchdid.un.b.id)
					return 1;
			}
		}
	}
	return 0;
}

/* Search for a nodelist entry */
static struct lpfc_nodelist *
__lpfc_findnode_did(struct lpfc_vport *vport, uint32_t did)
{
	struct lpfc_nodelist *ndlp;
	uint32_t data1;

	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (lpfc_matchdid(vport, ndlp, did)) {
			data1 = (((uint32_t) ndlp->nlp_state << 24) |
				 ((uint32_t) ndlp->nlp_xri << 16) |
				 ((uint32_t) ndlp->nlp_type << 8) |
				 ((uint32_t) ndlp->nlp_rpi & 0xff));
			lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
					 "0929 FIND node DID "
					 "Data: x%p x%x x%x x%x\n",
					 ndlp, ndlp->nlp_DID,
					 ndlp->nlp_flag, data1);
			return ndlp;
		}
	}

	/* FIND node did <did> NOT FOUND */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_NODE,
			 "0932 FIND node did x%x NOT FOUND.\n", did);
	return NULL;
}

struct lpfc_nodelist *
lpfc_findnode_did(struct lpfc_vport *vport, uint32_t did)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp;

	spin_lock_irq(shost->host_lock);
	ndlp = __lpfc_findnode_did(vport, did);
	spin_unlock_irq(shost->host_lock);
	return ndlp;
}

struct lpfc_nodelist *
lpfc_setup_disc_node(struct lpfc_vport *vport, uint32_t did)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp;

	ndlp = lpfc_findnode_did(vport, did);
	if (!ndlp) {
		if ((vport->fc_flag & FC_RSCN_MODE) != 0 &&
		    lpfc_rscn_payload_check(vport, did) == 0)
			return NULL;
		ndlp = (struct lpfc_nodelist *)
		     mempool_alloc(vport->phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return NULL;
		lpfc_nlp_init(vport, ndlp, did);
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
		return ndlp;
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_NPR_NODE);
		if (!ndlp)
			return NULL;
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
		return ndlp;
	}

	if ((vport->fc_flag & FC_RSCN_MODE) &&
	    !(vport->fc_flag & FC_NDISC_ACTIVE)) {
		if (lpfc_rscn_payload_check(vport, did)) {
			/* If we've already recieved a PLOGI from this NPort
			 * we don't need to try to discover it again.
			 */
			if (ndlp->nlp_flag & NLP_RCV_PLOGI)
				return NULL;

			/* Since this node is marked for discovery,
			 * delay timeout is not needed.
			 */
			lpfc_cancel_retry_delay_tmo(vport, ndlp);
			spin_lock_irq(shost->host_lock);
			ndlp->nlp_flag |= NLP_NPR_2B_DISC;
			spin_unlock_irq(shost->host_lock);
		} else
			ndlp = NULL;
	} else {
		/* If we've already recieved a PLOGI from this NPort,
		 * or we are already in the process of discovery on it,
		 * we don't need to try to discover it again.
		 */
		if (ndlp->nlp_state == NLP_STE_ADISC_ISSUE ||
		    ndlp->nlp_state == NLP_STE_PLOGI_ISSUE ||
		    ndlp->nlp_flag & NLP_RCV_PLOGI)
			return NULL;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
	}
	return ndlp;
}

/* Build a list of nodes to discover based on the loopmap */
void
lpfc_disc_list_loopmap(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	int j;
	uint32_t alpa, index;

	if (!lpfc_is_link_up(phba))
		return;

	if (phba->fc_topology != TOPOLOGY_LOOP)
		return;

	/* Check for loop map present or not */
	if (phba->alpa_map[0]) {
		for (j = 1; j <= phba->alpa_map[0]; j++) {
			alpa = phba->alpa_map[j];
			if (((vport->fc_myDID & 0xff) == alpa) || (alpa == 0))
				continue;
			lpfc_setup_disc_node(vport, alpa);
		}
	} else {
		/* No alpamap, so try all alpa's */
		for (j = 0; j < FC_MAXLOOP; j++) {
			/* If cfg_scan_down is set, start from highest
			 * ALPA (0xef) to lowest (0x1).
			 */
			if (vport->cfg_scan_down)
				index = j;
			else
				index = FC_MAXLOOP - j - 1;
			alpa = lpfcAlpaArray[index];
			if ((vport->fc_myDID & 0xff) == alpa)
				continue;
			lpfc_setup_disc_node(vport, alpa);
		}
	}
	return;
}

void
lpfc_issue_clear_la(struct lpfc_hba *phba, struct lpfc_vport *vport)
{
	LPFC_MBOXQ_t *mbox;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *extra_ring = &psli->ring[psli->extra_ring];
	struct lpfc_sli_ring *fcp_ring   = &psli->ring[psli->fcp_ring];
	struct lpfc_sli_ring *next_ring  = &psli->ring[psli->next_ring];
	int  rc;

	/*
	 * if it's not a physical port or if we already send
	 * clear_la then don't send it.
	 */
	if ((phba->link_state >= LPFC_CLEAR_LA) ||
	    (vport->port_type != LPFC_PHYSICAL_PORT) ||
		(phba->sli_rev == LPFC_SLI_REV4))
		return;

			/* Link up discovery */
	if ((mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL)) != NULL) {
		phba->link_state = LPFC_CLEAR_LA;
		lpfc_clear_la(phba, mbox);
		mbox->mbox_cmpl = lpfc_mbx_cmpl_clear_la;
		mbox->vport = vport;
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
		if (rc == MBX_NOT_FINISHED) {
			mempool_free(mbox, phba->mbox_mem_pool);
			lpfc_disc_flush_list(vport);
			extra_ring->flag &= ~LPFC_STOP_IOCB_EVENT;
			fcp_ring->flag &= ~LPFC_STOP_IOCB_EVENT;
			next_ring->flag &= ~LPFC_STOP_IOCB_EVENT;
			phba->link_state = LPFC_HBA_ERROR;
		}
	}
}

/* Reg_vpi to tell firmware to resume normal operations */
void
lpfc_issue_reg_vpi(struct lpfc_hba *phba, struct lpfc_vport *vport)
{
	LPFC_MBOXQ_t *regvpimbox;

	********** = mempool_alloc(phba->****_mem_****, GFP_KERNEL);
	if (**********) {
		lpfc_reg_vpi(vport, ulex Linux ;
	***************
 *cmpl = e Drimbdapteriver for Channel Host Bu      =       Chae Eme Drisli_issue_*********   *
 * Fibr, MBX_NOWAIT)
	LEX ==     *
T_FINISHED Devic	********freemulex Linux, ********
 * This fie Cha}
	}
}

/* Start Link up / RSCN discovery on NPR nodes */
void
e Dri    _s    (struct      opyrig*     /****004-200Scsi_Host *sh *  .           _fromristop      e Ch004-2005 Chrhba  *****ght (C) ->****;
	uint32_t num_sen 200m is freeclear_la_pending thent did_changed****e Em!e Driis_link_up*****)MULEreturny it und******rms ostate == *****CLEAR_LAMULEedistribute it a = 1;
	elsepublished by the Free 0y it und * This ort *
 * P<lic LiVPORT_READYMULEam is distributed blic LiDISC_AUTH****      etyrightmo          program is dfc_prevDIDPubl AND      myDIDMULE *
 * modif* Thistware Fo ANY IMPLIED W1NTAT AND          *
 * ARRANTIES, INCLU; *
 * FIT sofright *
 *  This p         D         buted ihba *
 * > PorLIED Cprintf_vlog        part_INFO, LOGXPRESOVERY,of E "0202
 * DISCLAIMED, E    *
 * Px%x "NVALIDData:Licenmore de\n"INVALIam is distributed,ARRANTIES, flagfound in theplogi_cnta copy of whifc_arightcnSENTAT/* First do APRESs - if anyNT TH softwar        elsRINGEMckageREPRESENTATIONS softwarn 2 of the GNU/*
	 * For SLI3,              will set istributed to hat i, and
#inccontinue          .
#in/it undneral  sli3_options &lic Li <li_NPIV_ENABLarks&&
	    !S AND       fil & FC_PT2PTscsi/scsi_device.h>
#include <sc    _MODEscsi/scsi_errupt.h>
_rev in the SLI_REV4) Device Dri  All ver for s rese          of the G	}***/

#include <l2, we needh>
#de <linux/pci.h>
#inclue <l/kthread.
#inc>
#include <linux/intam is distributed in the hope that i && !edistribute it a Devic/* Ifi.h"get here, t"lpf is noth Fret       linux#include "lpfc_lotyp Public LiPHYSICAL_ope MULEXh"
#includeedistribnl.h"
#include  assigndevice.h>
#include <scABpe t LEGALLY pfc_hw. OR NON-INFRINGEMENT, ARE   	rt.h"go thru      *
 *  "lp  All ELS PLOGI* Por	assignment offc_npr.    ULEX **************
 * iREPRESENTAT= {
	0xOR NON-INFRINGEMENT, C, 0xDA	spin_lock_irq(     ->     C, 0e ChaDA, 0xD9,
#include= ~FC_NPRESS CTIVE, 0xA6, 0xAunC, 0xAB, 0xAA, 0xA9,
	0xA7, 0xA6e DricanITIONS, REPRESENT	om   x79,
ll be useful. *
 * ALL EXhope that isc.h warevport.h"Nex     CB, 0xC          *
  *************************xB9, 0xB6, 0xB5,
**************** of the GNUxC9, 0xC7, 0xC6csi/scsi_transport_fc 0xDA/* Checkclude e    more     s came in while we Fo#incwbugfprocess* Alphis one.,
	0x3,
	0xC9,  0xC7, 0xC6rscn_*
 *****= 0scsi/s	

#inc
#include <scsi/scsi_transp 0xE0, 0xDCAE, 0xAD, 0xAC, 0xAB, 0xAA, 0xA9,
	0xA7, 0xA6, 0xA5, 0xA3, 0x9F, 0xransport_9B, 0x98, 0x97,
	0x90, 0x8F, 0x88, 0x84, 0x82, 0x81, 0x80, 0x7C, 0x7A, 0x7x6D, 0xC3, 0xBC, 0xhandle0x29,7C, 0x7A, 0      of the G     
 *  Ign3C, compleclud for all IOCB 0x2 tx 0xCDtxptersqueue

voiELSba *,r, 0x31e match
{
	ssppecified  *
 list.
D, 0*
 *ic tions Copy    _tx2004-2005 Chr         , 004-2005 Chr *rdata; *ndlp/*****IST_HEAD(BOXQ_t *);s                sli *psl    fc_t_tcsi_ *icmd               iocbq) {
		ocb, *next& FC_ (!ndlp || !NLP_CH_ort)
*CH Dg[] =NODE = &rrupt.h>
;
	ot fie nodsli->t fi[*****ELS_RING]       ErrorstructrminROLErminatq orort_io(q
#incl     c, 0x45hehba e <linux, 0xAC, 0xAB, ode"
		hba	0xA7, 0ata;_for_each_entry_safe(ROLE_FP_TARGET), &ot fi->txq, ata; 0x47,e EmROLE->kthrext1 != ata;
0x47, kthread.fcfi_cm		if e nop_DID,GET)
		C, 0x2	if ->ulpComm "lp== CMD",
			EQUEST64_CR) ||
, 0x23,abort_iocb(ndlp->vport,
XMIT",
			SPli.rX	0x10thod rpormove_tail(O_SID) 
}

, &>pnode;

	if (!	 termilp->nx27,--fcfi_cmpl     6B,
	debugfs_discp->phNT THArport terminate: sid:x%x did:x%x flg:x%x",
		ndlp->p->phlp_sid, ndlp->nlp_DID, ndlp->nlp_flag);

	if (ndlp->nlp_sid != NLP_NO_SID) {
		lpfc_sliabort_iocb(ndlp->vport,
			&phba->sli.rig[phba->sl.fcp_ring],
			ndlp->nlp_sid, 0, LPFC_CTX_0x47, 9 Emulex.  All abstriiotagnl.h"
#ot fi,;
	}
_fcfi_cmpl(98, 0x97,
	0x90,DISC_TRC_RPORT,
	     Cancelid
lps_difc_ter    _debu>pnode;

	ip_sidNT THAT SU_ERRca;

	& FC_snl.h"
# be called w, IOSTAT_LOd biREJECTINVALI, ndlIOERRnclud 0xE1ED) lpfcct lpfc_nodelist rightflush_ fc_2004-2005 Christoph Hellwig          a = rport->dd_data;_FCP_TARata;                         *
 * This progrATIONS AND        * inclu ||ith this package.    Devicerport terminate: sid:x%xoadingthe drive, &0xC7, 0xC6,*
 *INVAL		 nlpif we);

	if {
	0xNLP_CHK_NODEx9D,t->loon 2 f (ndlp->nlp_s, 0x54dlp->= rd*
 * PublL;
	STE_CB, 0_ISSUEg[phbad, nd->pnode = NULL;
		ndlp->rpo     NULL;
AE, 0xADlist * ndlp;
*phba;ata;
A, 0x79,
	0x     tions Copyedisnupdefer     _resources2004-2005 Christoph Hellwig   *********this iunregister_fcflp->dev_loss_evcmd	if (!list_emptefer this if we              alock);
	/* We need to hold the node by incrementing the reference
	 * count/c_hba *NAME:, ndlreturn;

	timeout
 hba *FUNCTION: Fibre Channel driver           ne
	 */ routi2E, 	evtp-EXECU_arg ENVIRONMENT: interrupt only	evtp->CALLED FROM:ba *,phbaTimer funcclud	evtp->RETURNStp, &phba-none	strubalock);
	/* We need to hold the node by incrementing the reference
	 * count untions Copyrightne
	 */(unsigned long ptrleting the vport
istoph Hellw = 2004-2005 Christoph ) * e                           *
 * This program is freetmo_postify 	dev_loss_tmo
  fil ARE     e Emunlikely(!ersion 2 of the GNU, 0xAC, 0xAB,save(UNLOADINwork_istriC, 0,t;
	stv);
;
	struct  PARTICULAint  put_nevente <sWORKERXPRESSTMO the Em!;
	struct t will be u= 0;

	rport = nd|=p->rport;

	if (!rp98, 0x97,
	0x90restort *name;
	int  put_node;
	int  putrport)
		return;

	rde Driint er_wakeof versio;l(struct lpfcag);

	/* Don't defer ne
	 */id lpfcr2004-2005 Christoph Hellwig              *
 *                                                                 *
 * This prograndlp || !NLP_CHKK_NODEe node"
			" to g the vport
	 * or unloading the driver. **************initrms ******/or  rc, clrlaerr* This progrdevice.h>
#include <sc;

	if (truct lpfc_hba   *phba;
	uin 0xAA, 0xA9,
	0xA7, 0, 0xA5, 0xA3, 0x9F, 0xt8_t *) &ndlp->nlp_portnam 0xAA, 0xA9,
	0xA7, 		returnebugf*******tr*******,ALL EXPRESSTRn",
		CMDINVA"    tp->evt_ueued*
 * :icenrtryput_nflgput_ a co of which can be found in thens_retryfound in the fil	&phbswiuct lude "lpfc_logmsg.TGT);
caselic Li,
		ndCFG_LINK:     istributed is identically put_node)
			lpfc_nlx36, 0x3aitrminfor
#inclAN <linux7, 0x4FANtp->evt_a, 0x2AT SUCH DISCLAIMERS ARE HELD WARNING TO BE LEGALLY INVALLID. 21te == NLP_ST\n"v);
	                   by sthe FreFB, 0e ndean    old rpixCA,
	0nfo here.
	 */
	if (vport->load_flag & FC_UNLOADING) {
		put_node = rdata->pnode != NULL;
		put_rport = ndlp->rport != NULL;
		rdata->pnode = NULL;
!	ndlp->rpoNPRt_rpo>rport != NULL;
		rdata->pnode = d for&ndlp-FABRICAE, 0xAD>phbx:%x:%x:s_di->pnrminFabric/kthnet);
	xCA,
	0x	returnrop {
		        ->dev);port *vpor    (!a->pnode = nclude L;
	p_ty     	0x10, 0xtateail outstahe FreIO now since devicrt);
s *f (pmarked

voiCB, 0, 0x2lp->nlp_se Driunver ror        ->dev);
		return;#include "lpfc_logmsg.h!blic Li %x:%a->pnode;
	ist nial_fB9, 0xB6, 0xBx52, 0x51, ,
	0x7breakdataif (put_noFPRES:fcp_ring],
		B, 0p_put(ndlp);
		if (put_rport)
		he target dev);
		return;
	}N %x:%ux/blE_MAPP.h"
_abortt, KERN NLP_STE_MAPPED_NODE) {
		lpfc_printf_vlogERR KERN_INFO, LOG_DISCOVERY,2SCOVERY,
%s "0284 Devl_DISCOV * Thish>
#? "			  " : "id, 0lossOG_DISAssume no>pnode ! "lpgorminwithinclude "lpfcput_6, 0x4portnlp_put(ndlp, 0x				 "0o | !NL(name+/name+1), KERNfailed, so just use loop mapdlp-mak.h>
#includISC_TRC_RP	return;

	nfo he, nmap0xB6, 0xB5,
imeout Ignored on "pfc_printf_vlog (C) 2port, &phba->sli.fcp_ring],
		des i		lpfc_nlp_put(O THE EXT
	}

	if (warn_on) {
	2x:%02x:%02x:%dev);
		return;
	}

	  NameSerdlp) * invloss timeout on "
				 "WWPN %02x:%02x:%02x:%02x:%02x:%02x:%02x3->worvt_a_printf_vlog(vportse fo	 "*(name+2), *(namevloss time6B,
	loo(name+*(name+2),  rdatx66, 0rdat.      find{
		_di_listp),de = rdata-_CLUDlpfc_sliULL;
&= NULL	put_rport = ndlp->rpor*********| !NL(&rport->dev);G_DISCReOVERY,
				 "0204 Devgoto e;
	arDITIONi.fcp_ring],
	NS_QRYp_put(), *(name+	rett_node = rdata->Rsp"0203 Devloss timeout on "
				 "WWPN %02x:%02x:%02x:%02x:%02x:%02x:%02x4de = rdata->Qu(evtp->evt_a			 ndlp *
 * more dex%06x Data: x%x x>rport != NUL******AXloadRETRYy[] = {
	0state_machine(vpor in the , NULL, NLP_E0x47, 0x4Try i_adde0x3C, ne
	D, 0x2Cstate_machine(vpor++&phba-c
	put_rpnsevt_listp),e <l_CTNS_GID_Flp_sidsc_state_machine(vport,0 &phbae Emuc 0x26,nlp_sa->sli.,
	0x76, 0x75	lpfc_unregi This rt->dev);

	:G_DISname+5CLAIMED, E, 0xver(name+5de <linux/pci.h>
#de "lpfc_crifpfc_s(name+5x/blkdev.h>
#include <linux/pci.h>
#incluname+ <liDID, ndlction lude "lpfc_hw4.h"
#include "lpnode != NUrrupt.h>

#include <scsi/scsi.h>
#include <sn;
	}

	ifnclude "lpfc_nl.h"
#include "ltware  {rpi);PIV Not enabledut_rport = ndtatic uint8_t lpfcAlpaArray[]0xA6, 0xA5,, 0x74, 0x73, 0x72, 0x71, 0x6E, 0x79,
	0G_DISCOetup 0xCD, 0xCCmail****INITIALIZE n",
	c(ndlp->nding t need to c*************************
 * This file is part of th!= NULNT)
		returna->pnode;
	iut on "
				 "WWPN %02x:%02x:%02x:%02x:%02x:%02xLID.  6 Dport)ontext. Thi			 nd	 "BOXQ_t *);
eid);evloss t	eral       *
 * Pu, ndlpHBA:%02ORo thim the functc_printfrms downFC_DISC_Tpfc_sli_ab* lpf(&rportNT)
		return       *cfg_topologyp_sid, ndlp@phba: Poin     *peed>pnodet need to c->u.mb.un.varCOVELnk.lipsr_ binA0xD4, 0xject which neeopyright (C) 2004-ect which nees Adapters.      _ERRdef righaptere "lpr postinglex.  All rights resestructure
 * @    *
 * EMt_evt - FONDIe, nbackrportee_fast_evion is calSLI are trademarkof Emulex.       structure
 * @phba: 
 * www.emulex.c"WWPN %02x:%02x:%02x:%PRESS OR p_put(Node Auth_rport *);
0203 Devloss timeout on "
				 "WWPN %02x:%02x:%02x:%02x:%02x:%02x:%02x7ba context object.
 * @evtp:evloss treturn;

	spin_lock_irq(&phbanterrupt coocates data
 * structure required for posting event. It also keeps track of
 * number of events pending and prevent event storm when there are
 * too many events.
 **/
struct lpfc_fast_path_event *
lpfc_alloc_fast_evt(struct lpfc_hba *phba) {
	struct lpfc_fast_path_event *ret;

	/* If there are lot of fast event do not exhaust memory due to this */
	WWPN %02x:%02x:%02x:%hope that iinteE, 0x4D, 0x4C, 0x4B, 0x4A,
	0x49, 0x47, if (ret) {
		atomic_inc(&phba->fast_event_count);
		INIT_L31      p->evt_a *
 * moreevt.evt_l
		lpfc_discc_state_machine(vport, ndlp, NULL, NLP_EVT_DEp */
		puc_reay+6), *(name+7),
	C_MAX_ExCA,
	0x_empty(&evtp->evt_listp))
	REG_FABRIC_EVENT) {unregister_fcfixt need to post an event. Thisuct lpfc_fast_pdefaultinteif (ret) {
		atomic_inc(&phba->fast_event_count);
		IID. 73 Unexpec	int;
	if (evtp->evt_,STE_UNMAPpfc_deSlic Licex%06y of which can be 	 "WWPN %02x;
}

 NULL;
	eral       *
 * re arif (put_nocense asintet_evt_ense LA* @evtp: Event data structure.
 *
 * This function is called from worker8r *) &fast_evt_daevloss tndlp rport SoftWPN %02x:%02x:%02x:%c_nl_UPd_check_e*ret;

	/* If there are lot of /* DronoderuVT_COfast_evt_data);
NKNOWN  ndlp->nlp_sWARM_START  ndlp->nlp_sent_ase LPFC_EVENT_QFULL:
		    CMDS  ndlp->nlp_sata);D_sub_category) {MT_EVT;
	d_check_error;
			evt_data_size = sizeof(fast_evt_data->un.
	30	read_check_} else if    rms STE_UNMAPblic LiceLPFC_ == LPFC_EVENT_PO
			lpfc_free_fast_evt(phba, fast_evt_dMT_Ek_evt);

			(evt_sub_ NULndlp rpo Device Dri
			evt_data = (char *) &f I/O Data x( I/O DextraR "Ca)].A3, 0x9F,"
#incTOP_T(ndlEVENTC_EVENT_VARQUEDEPTH:
	fcp_data = (char *) &fast_evt_data->un.queue_depth_evt;
			evtP_TARdata = (char *) &fast_evt_data->un.queuest event do not exhaust memory due to t
}

struct lpfc_hba *T1, 0rg1) {
 d lpfcs33, 0x32, 0xa   !(ndlp->nlEG_B, 0N->fast_eba *C_MAX_EVuponport, LPFC_. Itis fsmic_rinode =**********ba *asrc(vport, LPFC_(phba, fawhet_dataC_MAX_EVisba *d lped offdlp-s_diSLI layctio Portions Copy         fdmfc_hg_*(nam
	struct lpfc_hba *phba;**************pmb/****MAILBOX****mbe nodmbeed to. The unload widmabuf)
{
m;
	p2004-2005 Chrlp;
	in*) (truct ndlp->n                port->dd_data;e_evt;

	spin_loport->dd_d) a->hbalock);2. The unload wipfc_de)
{
pfc_dev_truct (C) 20
	a->hbalock);
 = NULLp);
->pnode = rping ruct e freWords[0];a->hbalock);nclud|	ndlp-RPI_VALOSE, will clean up tcase LP%x x%xist_empte = Neributedlp_sid != NL,ndlp->rpoUNMAPPEDype & ****/

#inc        AlWWPN node -_HEAD(&Managem****I
		lface (FDMI)free_fastto
#inc0xferena/* dec well known(ndlp) = nDelahis e_evt =unt ree_fast_f
#incrk_l-on=2 (supistrrminRPA/    nmae) <linu
#include "lpPoinrk_lion>vpo1ugfs_discrk_lient
 * @phb;
			la: PMGMT_DHBAta->ware Fomodd:x%xr *name;
	*)(evtptmo, jiffies + HZ * 6s fu     decr of ndde =  conrefereput_coune "llrport-31, 0rt)
(&php_put(ist);
	e <linuxdelist *)put ndlp-ist_emptmbuf      *phba;mp->vi@phbink_phy put_k     mEVT_Omulex.       pmbPosts events generated froRC_RPORT,
		"rportintelist *ilter_byp->nlg the vport
	 * or unloadingtion *param/****m is16*****		fren *)(phba);
			node)
			lp);
		=2));
else
				*(int *) (evtp->evt_arwwpstruct lpfc_hblete((struct completion *)(evtp-case LPmemcmp(&->pnode = istrname,	breakINVA&phba-sizeofa->pnode = 			compl)) 0x26RT,
		"rport_DOWN)
				lpfc_offlin
__ut_rport rport 004-2005 Christoph Hellw,d forvtp->ev tp->evompletion *)(evtp-_DOWN)
				lpfc_offline_pr&phbarport terminate: st->loadUNLOADING) {
		pu = rdata->pnode  NULtp->evt->loadn *)(e 0x52, 0x51;
			 elset *)(evtq(&phbfree_fast_evt(phba, fa
	pust_node = rdatFFRDsrk
			 e given RPI."
#i);
	fouast_/
	e of theta,
		 for
SC_TRel of ndpo;
		lx6D, 0	break;
		c
	structFC_EVT_OFFLINE:
			lpfc_offline(ba);
g1) = 0;
			comp_brdrestart(ph>evt_arg2rpi*(int *)(evtfc_offline(phba);       ) (evtp->evt_arg1), &io(pase LPFC_EVT_WARM_START:
			lpfc_offline(phba);
			lpfc_reset_WWPNrier(*)(ea);
			lpfc_sli_brdreset(phba)lpfc_hbaSC_TR_down_post(phba);
			*(int *)(evtp->evt_arg1) =
				lut_rport = ndl_LINK_DOWN)
				l_brdrestart(phevtp->evt_argx39,*_LINig              *
 *                                                      brdready(phba, HS_, 0xAC, 0xAB, 0xAA, 0xA9,
	0xA7, 0ULL;
	pmplete((struct completion *)(evtp->evt__LIN, 
			b
			lpfc_sli_abort_iocb(vport,
					&ptp->evt_arg2))}

	if (ndlp-= rdfor plete((struct completion *)(evtp->evt_ar * or unloadin/scsi_ lock_ freedid/****memseLPFC_E, 0,letion list)) {
		list_remove)	&phba->sli_abortizerport != NULL;
		, cont_ACT:
		p = rdata-arg1) = 0;
ata->p&phba->sli.ring[phba->sli.fcp_ring],
					ndlp-_rpod, 0, for
for ueued   con>pnode !=->pnode = DIDeventde refstruct lpfc_ht_evt(phba, fareleasE, 0llort-_MAPPE associa	int	 *(na sport_c NPort's;
			vt_dand*********    'reset(phbaata;
	struct lpfc_nodelist _OFFL*/
	if;
			lpfkref *n ev) =
				lpfc_sli_ill cleanfc_rport   *rport;
	stwhile (!list_empty(&phba->work_lkthrainer_of(n eva;

	rdata = rport->ddba cont	 on evpy = phba->work_ha;
	phba->->pnodwork_ha = 0;
	spin_unlock_irq(&phba error ueued
	/* Fiata->pno d fo* First, try to post theevtp->evt) {
	
			lpfc_slid fopy = phba-CH DISCLAIMEnts */
	if (pHELD *
 * TO BElock_irq	.
		9vport
	the error :			lp:x%p>un.
	"usgmaventx
			cnt:%dx%06x Da(pletio)->load_>pnode = usg_mat ha		atomirivead>hbalockn ev.phbaqueuts;
	s/_sli*
 *>pnodesc_trandlp);		break;
		casPOST_Rhba->hba_flag (ndlp);
	/*redistode = rdatER) vet;
	s

void
lp error  if (xCA,
	     *
		lpfcting the *phba;
	uint8_t *******		lpnode;
	int  put_L;
		Ltype &rt = ndlp-&ndlp->nlp_portname;
	vport_hbqs(phba, LPFC_ELS_HBQ)>hba_    >pnodememoryname+final>pnode		spin_uinux/intL;
		putFREE&phb ndlp->0x47,DOWN)
		lpfclat_datast_evmulex.       ENT)
			lpfc_hbqs(plpwww.emulex.co        _evt(phba, fabumpta,
					 * this queue
void>pnodeevtp->uc_fao ensur
	}
 thapfc_al;
	if (evtphphba won'tuffer(his hap36, 0xa"

/lp);
	if (evtpvportthe is u2, 0xi;
	struct;
			lpfc_unblock_mgmt_io(
			gec_hba *phba)
{port->dd_data;
	ndlfc_handle_eratt(phba);

	if (ha_copy & HA_MBA*******ag);

	if

	/* Process SLI4 events */
	if (phba->pci_dev_grp == LPq(&phbagebalock);
		if (phba->hba_fphba->hpnode !=I_ABORT_EVENT)
			lpfc_sli4_fcpevent_proc(phba);
		if (phba->hba_flag &

/**
c_fr, 0x4of>pnodeusagn jus    ****ine countrt)
{
	name+5pfc_cre	 * this queuepport (put_data3, 0x32rt_ebeingname+5 error dts pending&phba->hbalock);
				lpfc_sli_hbqbuf_add_hbqs(phba, LPFC_ELS_HBQ);!= NULL;
		put_rport = ndlp-d to_array(phba);ACKf (vports !=g & HBA_RECEIVE_BUFFER)
			lpfc_sli4_handle_receivevt_data->un.fabric_ba->hba_flag & ELS(vport, KERN_ts;
			sp			lp6c_sli4_elsock_it_event_proc(pphba);
		if (phba->hba_flag &  ASYNC_EVENT)
			lpfc_sli4_async_even			lpfc_disc_timeout_handler(vport);
				break;
		casrt *vport);
n ev
				;
		if (phbocbs(ER_FABRIC_BLOCK_TMO)
				lpfc_unblock_fabric_iocbs
			break;
lpfc_worno vports in arrnce countf unloading, so if
			 * this happens thenrier(s_tivent(ueuegoes jus0c_derk_podjectNG))_dishe_grp == LPFCing;
	uinshould b4*LPF    d. Rf thermin1s >>= (4*LPFC_Epfc_crepci_de has been
				lpfc;rmina(4*LPF	vportd lp   *g->flag 0 LPFC_DEFERRED_RING_EVENT)) {
		inotif (pring->fla*LPFye;
	strunt *) (ev	case LPbreak;
			spin_lock_irq(&vport->work_port_lock);
			work_port_events = vport->work!ata;
	nts & WORY,  *

	/* Process SLI4 events */
	if (phba->pci_dev_grp == L(&phbapTGT);
	&vport->work_port_lock);
			if (wor_port_events & WORKER_DISC_TMO)
			lpfc_disc_timeout_handler(vport);
	&phba->hbalock);
			lpfc_sli_hbqbuf_add_hbqs(phba, LPFC_ELS_HBQ);0x46, 0x45ED_RING_;
	}

	vNULL c			 ledgCEIVE_Blp->pletis_tim t(ndssible r			/condit.
 * port
				puS_RIMO)
vo(&rpagainpfc_dafn_po    iou 0x2E{
		id		cool & (HC_R0INT_Ee <linux/int		if (work_port_events & WORER_FABRIC_BLOCK_TMO)
				lpfc_unblock_fabric_iocbs(hba);
			if (work_port_events & WORKER_FDMI_TMO)
				lpfc_fdmi4c_sli4_els
				(vport);
			if (work_port_events & WORKER_RAMP_DOWN_QUEUE)
				lpfc_ramp_down_queue_handler(phba);
			if (work_port_evenNG_EVENT;
phba		if (!(control & inST_RE			 lort;
	sNG))) {
				ldebugfs_spfc_dow_ring_trc(phba,
					"WRK Enable ring: cntl:hacopy:4_posput_devalphbay_por
int
lpfcrmin*
 * control |= (HC_R0INTIAC thaif (vports !=G);
				writel(control, phba->HCregaddr);
				readl(phba->HCregaddr); /* flush */
			} else {
				lpfc_debugfs_sl5w_ring_trc(phba,
					"WRK Ring ok:     cntl:x%x hacopy:x%x",
					control, ha_copy, 0);
			}
			spin_unlock_irq(&phba->hbalock);
		}
	}
	lpfc_work_listludela    e ifice(control &  & WOR;
	strnlp_rpi)use  nnode r	vport
				lpfasync			"WRK E& LPFC sx39,s happhphbaget>data_n betw (prt_events & W

	lpfc_fts = l			"WRK E
		if (pprocessring: phba,s.h"dlpe <linux/intt_proc(phba);
		if (phba->hba_flac_nodevport.h"
>>= (4*a);
}

sN_ERup o
int
lpeead_shoVT_COUdlp->ET wait and check;
			ifA << LPFC_ELol & (HC_R0INT_EN
		if (prsee
			spi_work_array(phba);
	if (vporvent to handrk_port_eventswork_lER_FABRIC_BLOCK_TMO)
				lpfc_unblock_fabric_iocbs hba e fotend			"WRK Eli_brdre1one(phnce count	returler(vportinux/kt			if (wowas 1, lpfULL;
	f unloadspin_unl	lpfc_put(ndlp) ha_* but lpfstncluleft heldler(vport);
			iwork (NT;
actut)
	pfc_deerformLPFC_E KERNnce countct.
 ). Ovporwise_evt OCB events wiring[LPFC_ELS_RING];
	status = syncli_brdre0e <linuxp->evt_			"WRK c_ramp_up_quion *)(_els_xri_ab		break; vports in arrba);

	if (V_OC)
data *rdata;uirevt  VENT;
in_sta*LPFbyvport)vport = phba->pport;
.to SLI4 device *rg1, voii<< (4*LPFULL;

		if (pr	|| (prAhba);
		valuert_eng->flag |= LPFC_DEFE_evt(pNT;
ye;
			/* Set theata pending flag */not_used	set_bit(LPFC_DATA_READY, &phba-

	/* Process SLI4 events */
	if (phba->pci_dev_grp == LPFC_PCINT;
c_clueueRXMASK));
		}
		/*
		 * Turn on Ring interrupts
		 */
		if (phba->sli_rev <= LPFC_SLI_REV3) {
			spinKERN_INFO, LOG_ELS,
			"0432 Worker thread 004-2009 Emu	case LPFC_EV)(evtp->evt__worp->evt_reak;
/*hba *t *)(ecf_instat-FC_UNLOif FCF casing  (ndlister;

	re @****: Pdown_poto_CHEC ndlp-> objec;
	stfast_evt(ut(ndlp) iterci.h>hrough(phbaFC 0xCE, 0rp == LPF
			incluphba     eup ork_por &phbaugfs.hport for
	 *(K)
		c_r			(ndgrp == LPFC_PCI_itK  << (4NameServpfc_unregvt_datp == LPFC_PCI_set(phbaba *p		lpfc&phba-s eivporti		lp	 */
			iedublic Lor its_rpolossp);
		 0;
}e it a
	struct lpfcnt *) (evt	if (ndl
	struct lpfc_hba *phbxpire.
 */
static void
lvtp),
MBATor  iNT) {0xD4, 0			lpfc_sli_brdready(phba, H            *
 *         ,  *
 * F AREndlp->reatet compc_trc_arrayFC_DISC_
	port(ing 0; i <=      *
axt comps;
	n->fc_f[i]if (n(&ph i++ worker                                  _VPOeue_handleC, 0xAB, 0xAA, 0xA9,
	0xA7, 0xFFRDY | HS_MBRDY);
			lpfc_unblo_VPOck_mgmt_io(phba);
			comple |= (HC_R0INTt_handler(phba)&&>hbalocunreg 0x25, 0xk_port_unreg->rovt_dcsi_trope thOLE_FCP_TARGET	0x10, 0xE_RECOast_e0x98, 0x97,
	0x90, 0x8F, 0x88, 0x84, 0x82(&rpoou 2004x79,
	0x798, 0x97,
	0x90, 0x8F, 0x88, 0x84, 0}
_TGTvport, instroyvpi(vport);
		spin_locC_EVENT put_p->evt_retp))
			continue;
_UNUSED_NO_vfp->eplp->nOXQ_t *);
e:x%x i	 * t (ndl vfi)
			continue;
		if ((phba->sli3_options & LP @****qnue;
		if ((p>fast_evtions & LPFC_SLI3_VPORT_TEARNT_Es (HC_R0I on link down */
		i>fast_evC_MAX_E
	struct lpfc_nodelist shost_from_vport(vptruct lpfc_hba *phba)
{
	struct lpfc_wpfc_dxpire.
 */
static void
lpfc_dev_pfc_dvt_listp);
 NUL lpfc_hd to mbxegoruxAE, 0xc(phba);
		iAIME*vport%02x:%02x:%02x:%02x:%02|O BE****			spi2555 UNhostVFI vport *vpet->woLicense fo"HBAublic Licex%06x Dastruct lpfc_vport *vpC_EVENT_FABRIC_BUSY) ||}*(int *) (evtp-pfc_d       *
 * www.emulex.costruct lpfc_hst = lpfc_shost_from_fcport(vport);

	fc_host_post_event(shost(FCFc_get_event_number(), FCH_EVT_LINKDOWN, 0);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Link Down:       state:x%x rtry:x%x flg:x%x",
		vport->port_state, vport->fc_ns_re(FCF_AVAIrt->fc_flag);

	lpfc_port_link_failure(vport);

}

int
lpfc_linkdown(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(0port);
FCstruct lpfc_vport **vports;
	LPFC_MBOXQ_t          *mb;
	int i;

	if (phba->link_state == LPFC_LINK_DOWN)
		return 0;
	spin_lock_irq(&phba->hbalock);
	phba->fcf.fcf_flag &= ~u(ndldba); - UUNUSED_NOe ==     ll_rport)s ar		 */
	NULL;DE)
			continue;
		if ((phba->sli3_options & LPFC_SLI3_VPORT_TEAR>nlp_DID == Namli_dportmbox_cmpl_POSTttrucr		 * ttend ==  <li		el_cmpl tend= lpfc_sli_def_mbox_cmpl	statusPORT_TEAR_UNUSED_NOool)I);

	LI3_VPORT_TEARalso tremento_stat			vport == port>
#include  Portions Copyff, mb);
		mb->vport  remove
					     ? NLP_EVTt_link_failure(vpoleanup thk_list), evtp, typeof   : NLP_EVT_DEV_evt = 0;
			breakDISC_TRC_RPORT,
		/

#incIf LPFCn_lock_runflag in FIP m con	}

	ifc_sli_idING)NT;
break;
 FCoEWAIT)
			  >mboxsue_mboUSED_NODE)
p = koh"

/* Ae <linux/int!y == LPO THnclude MT_EFCOE_SUPnd_meg[phbaock_irq(fcf.				nclude <sF_REGISTERarks[phbay == LPPoinpfc_fa_fip 0x26, worker queue activit>vport = vport;
			lpfc_disc.h"vport = ndlp->vport;
	phba  = vport--2009 Emu					     ersion 2 of the GNct lpport;
		mb-VP0xCA,
	 {
		lpfc_mbx_unreg_vpi(vport);
		spin_lock_i
	evt_cateflag		spin_unlh>

#include <scsi/scsi.h>
#include <sfc_n(shost->host_lock);
		vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
	ONLINE:
xf (ndlpfor      
	}
}

vovport->y outsta {
		casnt,
		worNEEDL, NG_VPIthe ndlp
			 * fvpor*
 * P *) &fastVFude 2PT_PLOG) &fastnkdown_port(struct lpfc_vport *vport)
{
	strut_for_each_entry(FIport_*****************************
 * This file is part of the Em!inux Device Dri>pport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(6port);
	strucoxcmploect.
 *
				 rts;
	LPFC_MBOXQ_t         ******eak;
_FABRIC_BUSY) ||
pfc_disc.h"
#

	if (ndlpvfieturnar *) &flpfc_vpvf			b elsestructure st_from_vport);
	strs Adapters.      shost_from_vport(vp*****truct lpfc_hba *phba,
		structst_path_event *evt) count);
	kfree(evt);
}

/**ng IO now since device is
			 * marked for PLOGI.
			 */
			lpfc_unr7port);
	str&phba-> ndl
				  rc**vports;
	LPFC_MBOXQ_t         the truct lpfc_vport *vport)
{
	NK_DOWN)
		return       *
 * www.emulex.compfc_disc.h"
#invport;
		mb->mbo		} else if (!(ndlp->nlp_flag & NLP_NPR_ADISC)) {
			/* Fail outstanding IO now since device is
			 * marked for PLOGI.
			 */
			lpfc_unr1ork_array(phbandlp);
		}
	}
}

static void
lpfc_linkup_port(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost (FCF(phba->sli3_o&= ~(FCport);
	struct lpfc_hba  *phba = vport->phba;

	if ((vport->load_fl(FCF_AVAIa->hDING) != 0)
		return;

	lpfc_debugfs_disc_trc(vpoort, LPFC_DISC_TRC_ELS_CMD,
		"Link Up:         top:x%x speed:x%x flg:x%x",
		phba->fc_topology, 2ork_array(phkspeed, phba->link_flag);

	/* If NPIV is not enabled, only bring the physical port up */
	if (!(phba->sli3_options & LPFC_SLI3_NPIV_ENABLED_cmpl;
			mb->vport = vport;
			_flag &= ~(FC_PT2PT = ~( FC_AVAILude  || FC_PT2PT_PLOG phba FC_ LEGALLYif (| FC_BOOTnclude 	for (iIN_USE (vports VT_EL_VLANpin_unlock_irq(&phbart;
	phba  = vport->pT)
			  t(ndlp)e(mb, px97,ait a,->nlp_DID == NameServeralock
#inclCF recor				at NLP_STE_s&rport->
#include <linux/interrupt.lpfc_vpork_, 0x4B, 0x4UNLOADINGI);
		spin_unl     *
 * Pin the ata);
	on 2 of the GNUf (vport->fc_4(phbact lpions &truct Sg],
		);
	 FC_GteadIRSanup_nodes(vugfs_disc>pport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(3ffffffff, mb);
		mb->vport =a->linkrportad3_oprts;
	tions & id
lpfc_linkup_port(struct lpfc_vport *vport)
{)
			continue;
processinmbox_tbport)nreg_c_destro == N NULL;
		 tc_fac_get_event_number(), FCH_EVT_LINKDOWN, 0);

	lbuff: Buffert(phba);rt)
{
	spmb->vport;
	struct  rcvport_evconfigp, &phba-  phba-p);
IT)
		    == MBX_Nort *vport = = 0;ppens then x_mem_pool);>vport;
	sfc_slins & LAILBOlayersc_tr&pmb->32_t co 23
	struct lpfc_nodelist mb)
{
	struct lpf
	struct lpfc_hba *phbap->evt8****);
	port->work_port_l
	struct te: s *next_ring]_FCP_TARnext_ring]ERY);
	}
	if (p
	struct hdr.flag &hdlpfc_nodelist *n
	struct rec.flag &re mb)m is free	psli-_strucsli_def_mboxg(phfer(b = &urr****>vport;>ring[pruct fc_rport *rport)
{
	strulag &= ~LPFCSTOP_IOCB_EVENTO)
				lpfcb->mbxStatus	ha_clp_sid, != NULL)next_ring]nup_nror */
	i_evt;

	spin_lor error */
	if) );
	a->hbR_LA mbox ent(phmb->m->lengthn potion *m is fre) str

	if (tatus) && (mb->mbxStatusmb->mbxStatus  vport->port_state);
		ptus !"sta();
	 +}

	if (ort->port_state);
		phbats;
	s(shost->host_loLEAR_LA mbox eREG_VPI;
	 NULL;
	spin_l[i= (chads */
FCNCTports[if (!N(ndlp->nlp_snext_ring].= kz******>hbalock);
	psli->sli_flag |				 mO)
		pfc_fast_path_event)*/
	spin_unst_evt_data->un.fa
	struct Scsi_Host  *shoock_	lpfc_f2566	lpfce SLI p);
		}= &pmULL;
					if (>ring[pte: srk_evt.ev->sli.ring[prt up cpy(&*/
	spin_uhbalopin_lid:x%ontrol |=O)
		ype == LPFC_PHYSICAL_PORT)
		phbta->un5 Device Discovery .vlan_tag =* On e_argo_cpux\n",
				 k_irq(shost->host_l) & 0xFFF->HCregaddr);
k_irq(shost;
	struk);
	vport->fc_flag &= ~FC_ABORT_DISC
				readl(rporadd This f
	spin_lock_lpfc_handbxStatus error x%x hba "uld have noMBOXQ_t *pmb)
{
	oe_n *)( - Rayer.
oeetion eterdisc_tr&pmb32_t conScsi_Host  *shost = lpfc_shost_from_vport(vport);
	struct lpfc_sli   *psli = oEHCregaddr)scovery right n		evtp->ort->fc_flag & updon't do discovery right no	 *(n&pmb->u.mb;Cregaddr);
sli->extra_ring].flag &= ~LPFC_STOP_IOCB_EVENT;
	psli->rntrol = fcp_ring].flag &= ~LPFC_S	STOP_IOCB_EVENT;
	psli->ring[psliprol = */
	ifvport = pmb->mbxStatus) && (mb-ntrol = shba->mbox_me_irq(->mbox_mem_poe = LPFC_HBA_READ_free(pmb, phb"stank_statcontrol = r vport->port_statfc_topologyk_irq(&phba->hbalock);
	psli->sl_free(pmb, pts;
	s, 0x2OOP &&
	    vp->parm_versg &=!=b, MP_VERSIONI);
		sp  port_state is o out;
		 *);
	PARAM_LENGTHon 2 of the GNU Genb		lpfcvery timer
			 * f timp_X_NO, OOP &&
	    vpthremb.m* LPFort__ON"stain_unlock_irq(shost->hNT;
		
			return;
	}

	/* Start discovery by sending a FLOGI. port_sate is idenFFically
	 * LPFC_FLOGI whilehis progr  port_state is identHC_LAINT_ LPFCLANwritel(0x47,******valid_->ho_cleanup0306 CO>hosint w	vport->fc_f
			/* Neerror x%;
	spimb.m_unlock_VERYxStatus sync[0] =ending a FLort_state);imer);

	vpstate1;
	mempool_free(pmb, ph1a->mbox_mem_pool)2;
	mempool_free(pmb, ph2a->mhbalock);
	phba->fcf.fcgetx%x h&pmb23 - Gbrea
	psli->r forifc_ging].flag &== 0;(vport);
	struct lpfc_sli   *p_ring].flag &= ~	lpfc_issue

	i: Siz&phbaetup over);
	urn;
} @%x hlag & R,
			 vportto_STEsearch;

	retrol;

	/* Since w
{
	strsport_state);

	lpfc*mboort g_fcfbegi   *_STOPeg_fcf	psli->p->evt    byLEAR_LA vporrier(ppsli->);
			statuK)
		ORT_TEARp->evt__down_povt_dataERN_ERRst(phba);
			*(int *)(ev lpfcTOP_IOCB_mt_io(INK bad hba st(vporIOCB_EVENblock_ free

	iblock_IOCB *phba, evtp->evt freeoffde <=;
	s%x hb out;up_nodesq(&phe);
	PATH_MGpfc_ON23_LAS thaCI);
		sp

	i <}

	if (vport->poon 2 of therq(&phba- Start FCo =ruct [urn;
	}+ 

	l		if (lpfOne TLVr x%x "

		i		cow	spineaderk_evtnumber.mb.i(strrqsasem_pop->evt    port_ev.un.reg_fcffi wormb.mbxStatus) e(&phbe <linux36, 0x(( Set the .un.reg_fcf	}

	if (vport->poba->hbaloc_reg_fcfi_
		<=ee(mbHCregaddr);
	/* Set tsendi>mbox_mem_ents & WOR&pfc_check_pe[] = {
	0pfc_check_pendinng a FLOGI. */
	phba->fvents & WORKER_RA
		 Set theing_fc there is a pending FCoE event, restart FCF lpfc_ln.reg_fcfi);
	/* Set the FCFI;
			break;
		case LPFcontinue;
parse{
			/rol |- P->hbush */ort_stai(strsli->extra_ring].flag &= ~LPFC	continue;
		if ((ple_eratt(ock_irq(&phba->hbaue_clear_la(phba, vport);
	return;
}

static void
lpfc_mbx_cmpl_reg_fcfi(struct lpfc_htrol;

	/* t;
	sta->hb* flush */ort_staCregaddr);
->port_state);

	23;
		}
	}popultatic void
lpfc_mbx_cmpl_local_	struregaddr);mb = mempool_alla->hba_flag &= b->vport;

	if (pmb->u.mb.rt_state);
		mvided
 l_free(mbm_pool);
		return;
	}

	/* Start FCoE ded
 * fab);
	t lpfs[i]);
	lpfc_overy= bfis l & Wthan 2
	phba v_loathen syncically
	canNT;
		em_povervt   e <linux/int = bf_g2*nt, restart FCF tabl**********/
if (!(contrflag &=atch(uint8f     inux/intp->evt_;
		meng a FLOGI. */SIGNATURE, lpfc_hw.h"
#iba->mbox_mem_pool);
	return;

out:
	/* Dvice7 Cter to fabric na
		ifade_0, new_fevloss tpfc_disc.h"
#ate != LPF4lpfc_fcf_record_fcovery right noically
	inux/intpfc_check_penh the taFLOGI. */C_LOCAL_Cord_fab_name_1, new_fcf_record)) &&
	    (fab_name[2]8==
		bf_get(lpfc_fcf_recorically
e_2, new_fcf_record) &&
	    (fab_n.un.reg_fcfi);
	/* Set the FCFI regadl(phba-*/
	spir x%x "
);
	iflse rpfc_mbx_atus, vport->po)) {
		mempoo
	mempool -turn;
	,		 */
			lpfTYPhandhe Emullse rugfs_discort *vport = pmbs reserv7, newame[6] ==
		bf&phba->sli;
	MAILBOcord_fab_name_6, new_fcf_record)) &&
	    (fab_name7] ==
		bf_get(lpfc_CONN_TBLecord_fab_name_7, new_fcf_record)))
ing[psli->f1;
	else
		return}
