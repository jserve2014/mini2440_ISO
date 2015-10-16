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
		lpfc_reg_vpi(vport, ulex Linux ;************t Bus
 *cmpl = e Drimbdapteriver for Channel Host Bu opyri=opyrig    e Em     sli_issueis fi******   Ada Fibr, MBX_NOWAIT)
	LEX =ht (C)*
T_FINISHED Devic*********freem *
 * Fibr, ost Bus Ada This fie    }
	}
}

/* Start* Fik up / RSCN discovery on NPR nodes */
void
     C) 2_sC) 2(structopyrigopyrigeser  /hts 004-200Scsi_ *
 **sh *  .t (C) 2rightfromristop      x.co       5 Chrhba       ght (C) ****
 ;
	uint32_t num_sen 200m emulreeclear_la_pending thent did_changedhts -200!     is_link_upLinux MULEreturny it unfy it**rms ostateand ral  CLEAR_LAn 2 edistributeGNU a = 1;
	elsepublished bynd/o Free 0 GNU Gen www.emuortrved. P<lic LiVPORT_READYn 2 ayou cished by td bn the DISC_AUTHs reser    ttophhtmo          progrll be ufc_prevDIDPubl ANDIED COmyDIDn 2 rved. modifwww.emtware Fo ANY IMPLIED W1NTATRRANTIES, SLI ar * ARRANTIES, INCLU;rved.  IT sofTIONSOR A Pww.emu           FITNESS F. *
 *i    R A P> PorABILICprintf_vlogPRESENTATart_INFO, LOGXPRESOVERY,of E "0202 A PPRESLAIMED, ES FOR A PPx%x "NVALIDData:Licenmore de\n"Ie forll be useful. *
 ,ARTICULAR Pflagfoograinnd/oplogi_cnta copy of whifc_aTIONScnSE,  */.   rst do A LEGs - if anyNT THINFRANTY*
 * DISelsRINGEMckageRE LEG     IONS********n 2th t   *GNU/*
	d.  or SLI3,              will set seful. *
 *to hat i, and
#inccontinuelkdev.h>
#.inux/NU Genneral  sli3_options &n the  <li_NPIV_ENABLarks&&
	h>
#!S
 * FITNESSfil & FC_PT2PTscsi/si_h_d of e.h>inux/lude <scrightMODEsi_host.h>erruptude _revYING  *e <l_REV4)ks of  CopyriAll         s res.h>
#includ********	}***/
 <scsi/scsil2, we needde <lpfc_Fibr/pciude <scsi/pfc_/kthreade <lice <scsi/scsipfc_slintll be useful. *
 *.h"
#inhope tinclu && !lished by the Fres of /* Ifi.h"get here, t"lpfou cnoth*
 *005 Chr pfc_s<scsi/scsebugc_lotyp * WA the PHYSICAL_de "n 2 Xh" <scsi/sclished bnl. */
static   assign
#include <scsi/scsi_tABe "l LEGALLY f schw. OR NON-INF******ENT, ARal P	rtncluo thrCopyrigNT, AR"lpncludeELS PLOGI*NT T	= {
	0mor  offc_npr     thod      *
    *
 * wwi***********=Devi0x, 0xD9, 0xD6,
	0xD5, C, 0xDA	spin_lock_irq(.h>
#->(C) 20, 0x.comDAE, 0x9, <scsi/sc= ~FC_N LEGS CTIVEE, 0A69B, 0unAE, 0AB,
	0x6, 0xA9,xB4,A79B, 0x     canI*****, *********	om   x79,
ll be useful.OR A PALL EXude "lpfc_csc.h NTY      .h"Nex(C) 2090, 0CFITNESS FOR A, 0xBC, 0xBA, 0x5C, 0x5A, xB9E, 0Bx98, B5,
65, 0x63, 0x5C, ***********xC9, 0xC4, 0xC6i_host.h>trans    _fc, 0xA/* Checksi/scs.h>
# deta 0x3s cameh"
#whilei.h"Fo"
#iwbugfprocess* Alps disne.8, 0x38, 0xE, 00x4D, 0x4C,rscn_R A 27, = 0si_hos	nclude <scsi/scsi_t0x4B, 0x4A,
	0 0xE0E, 0xCAx9B, 0DF, 0x7,
	0x90, 0x8F, 0x88, 0x84, 0x82F, 0x5F, 0x3E, 09FE, 0A,
	0x49,990, 098truct78, 0x90x17,8imeou8 lpfc84d lpf2d lpf1d lpf0x17,7AE, 076, 0x7x6x0F, Cdisc_BAE, 0handle0x29,t lpfc_vporude "lpfc_disc 0x3AT, ARIgn3C, complesi/s     all IOCB 0x2 txrt);Dtx    squeue
rtioELS THE,rE, 031e match****ssppecified 0xCElist.
x0F,R A ic clude C witighttx2            kdev.h>
#, 	struct lpfc_*rdata; *ndlpig   *IST_HEAD(********); (C) !ndlp || !NLsli *psl
#incc_t_tt.h> lpfmd (!ndlp || !NLPiocbq Devicocb, *nexte <sc (!ata; || !NLP_CH_ort)
*CH Dg[] =NODE = &lude "lpf;
	otulex.nodsli->ermi[27, 0ELS_****]!ndlp |Error004-20rminROLEurn;atq orx49,io(q <scsi!ndlpcE, 045he    pfc.h"
#0F, 0x08, 0x04ode"
		hba 0x84, 0->dd_for_each_entry_safe(
	}
_FP_TARGET), &termi->txq, ->dd_0x47,-200
	}
->fc_scxt1 !=p_sid
 ndlp fc_scsi.fcfi_cm	he Enatep_DID,g:x%
		AE, 02he E->ulpComm0xCD== CMD",
			EQUEST64_CR) ||
_sli_3,abndlp->cb(ata;->      
XMIT
			&pSPli.rX	0x10thod rpormove_tail(O_SID)     , &>p *
 ****e Em!	 termi		ndnx27,--lp_sid p	lpfc_6B,
	de 0x3s_    p->ph  *
 A}

/tev_losnate: sid:icend
	strucflgstru
			&
			ndlbk(slp_sid, 
			ndnl_SID) _nodelist * fil)led whennodelist *sidlp_fERN_Nfunctioevice Drisli.fcp_ring],
			ndlp->nlp			&******sC_CTig[t_node;
.fcp_ring]nt  pport;
	struct, 0, *****CTX_

	if 9 E     .ncludeabhed iotaglpfcAlptermi,;    _
 */
void(t lpfc_vport *);PRESSTRC_Rope ,/scsi_ Cancelid
lpmo_cT(ndec_hba_lossbe called wtructtruct T SU_ERRca****e <scslpfcAlp 0x7called w, IOSTAT_LOd biREJECTa copylp;
	IOERRcsi/s18, 1ED) of sctlp_fl_ *
 ata; TIONSflush_CT(n
	struct lpfc     h HellwiMERS ARE  Free fc_rp->dd_t->dd_FC%x fl->dd_r. The unload will clean * www.emuTIONS******* * FITNESS * scsi/ ||ith tnode
a*****     c_hw.hfc_rport *rport)
{
	struoait a
#ind    , &x4D, 0x4C,,R A a cop		 nlpif we_vport *0xB4,ERN_ERK_emotx9D,t->loo**** *vport;
	struE, 054			nd= rdblic LublL;
	STE_A, 0x_ISSUE  put_fc_no-be cal = NULdlp-port->drp REPREfc_nlp0x10, 0xn't d*_node;
*****;lag);vport 88, 0xr. Th_nodelist lishnupdefdisc_t _resources are in the process of deletinx5C, 0x5Aeanupiunregisterretu		nddev_loss_evcmd when ata;_emptte ==oss_evata-r. The unload aC, 0f th/* Wh"
#inh>
#iold*****)
			byst nre0xC7t and/o rte =ence
#inccount/c_O THENAME:lp;
	of the****timeout
 O THEFUNC****:     x.com     g & Fc_hba *phb  n
	 * / routi2E, 	evtp-EXECU_arg ENVIRON0xD5: intclude  onlyevtp->>CALLED FROM:t *rp****Tim    uncsi/sail(&evRETURNStp, ut_nodnone	004-block);
	/* We need to hold the node by incrementing the reference
	 * count  un_nodelist TIONSp->evt_(un{
	0ed long ptrleing the r     
rocess of de =re; e in the process ) *x43, 0 The unload will cleanup the node
	 * ll be an rtmo_postify 	ty(&evtp-tmo
includ0xD4,     Emunlikely(!ersi->rpo**********0F, 0x08, 0x0save(UNLOADINwork_shed 	0xA,t;
	stv); to 004-2005PARTICULAint  put_neventcsi_WORKERE LEGSTMO*****Em!rport;
	int#inclu0x75= 0****fc_rpo= nd|=ndlp);rtled when rpt lpfc_vport *)reststribname the 0;

	rpocallep->vport; (uin	lpfwork is dordr the nt er_wakeofe "lsio;l2004-200of sfc_vpor/* Don't ate ==p->evt_idRT,
	r are in the process of deleting the v1, 0xCE, 0->scsi_target_id);

	/* Don't defer this if we are in the procect lpfc_rport_da	printk(KERN_ERKut_rpo node C_TR	"h>
#.
 */
static#incor unl->load*****t(ndlp., 0xBC, 0xBA, 0init    x5C, 0/or  rc, clrlaerrhe node
	 * 
#include <scsi/scsi_tled whenC_RPORT,
	til t  &rportram i, 0x8F, 0x88, 0x84, 0 void lpfc_disc_timeout8de;

 &nodelist *(uinnam, 0x8F, 0x88, 0x84, 
	lpfc_doss_t->dev_lorx5C, 0x,, 0x72;

	ifRn
			&CMDa co"	}

	p->evt_ruct NULL;:morertry
	rpoflg
	rp ded th thisch cana = e COPYING  *ns_retrye COPYING  *ncluput_nswiRPORTment of scsigmsg.TGT);
casecan-doa *rdaCFG_LINK:les & c_logmsg.h"s idntin didyort;
	phb	lpfce Drinlx3c voi3a neeinfor <scsiANfc.h"
#4, 0x4FANC_CTX_TGa_sli_ORT,
ot fe GNU GeRStructHELD WARNING TO BE 0xE0, 0xa copLID. 21* PublERN_ST\n"put_rt->phut Ignored on by s   *
 *F90, e ndeane "lpl}
}
ixCA8, 0nfo "lpf.>evt_d when     ndlpad lpfcde <sc*name;
	Ghba;
	rt;
	phbort
t->dode)
			lpfcc_nlp_p
	rpd_data;
	n(ndlp);rtname+5), *(n(name+4), *(nme+5), *!put(ndlp);NPRe+6),,
				 ndlp->nlp_DID);
		return;
;

vo
			lpFABRI	0x10, 0x>phbx:%		putmo_code)urn;Fabricpfc_netss tx "
			x	lpfc_dropba;
	ut Ignormpty();				 *    (C) 2!
		return;
csi/scsdlp-p_t * nd _TGT)he taateail outsta  *
 *IO now sinctailvicrL;
	s *f (pmarked fc_rA, 0x_sli_rt;
	stru     undlp)re = NUNULL;
		rda

	lpfc_dessignment of scsi = NUh!scan-do put_e+4), *(a  =st nial_f59, 0x56,
	0xx5_flus51, 8, 0x7breakt->de Emrt;
	pF LEG:;

	rdata = r90, p_putvport_NO_Sp_ring],>phba;

	he tarude NLP_NO_SID) {
	
	}Nrget ux/blE_MAPPfcAl_.fcp_t, part"0284 D, LOG_EDt_rpohba;
	strucCH DISCLAIMERR			 " *
 * TO B_ {
	ALLY I202x:%02x
%sID. 84portlx:%02x: www.emde <? "	rt !" : "a;
	nevtp02x:%0Assum nod4), *(na0xCDgourn;withsignment of s
	rpc voi4i_abo_sliPFC_CTXE, 0	n",
"0o k(KER( = n+/ID, n1)				 "failed, so just75,  loop mape+7)makude <scsi/st;
	phba  	lpfc_debug	 "NPo, nmap0x56,
	0x55,e
	 */, LPored    "			 "WWPN %02x: * Th2      ut_node;
	i;

	rdata = r
 * i(&rport->, *(naO THE EXT    d when****_onhba;
2x:%0x x%x x%xc_printf_vlog(vpor
 timNameSerCTX_just vevtp ne
	 */ "020ce(&	 "WWPN %x x%x\n",
, *(name+6), *(name+7),3->wor_STE "WWPN %02x:		 *nasort 	 "*DID, n2), ->nlp_e+3),
				_dev_looDID, n->nlp_state*(namx6c vo(nam       findevic_di_ata;p),			 *(name+_CLUDstruct lc_nlp&me+5),name+6), *(name+7),
			x5C, 0x5A>nlp_D&
	 * or 	rda2x:%02Re2x "
					 ndl20rt x%goto fc_sarD0x80,2x:%02x:%02x:%NS_QRY, *(naate, ndlp+	lpf),
				 *(name+4Rsput_d3t x%03),
				 *(name+4), *(name+5), *(name+6), *(name+7),
				 ndlp4			 *(name+4Qu(il(&evP_STE, *(	prinANY IMPetailx%06x  *
 * icenx,
				 ndlp->x5C, 0AX, *(RETRYynd r!= NU*
 * _machine		 *nh"
#incl,e+5),,"0284E

	if 0x4Try i_adde0xFC_Mp->ex0F, 2C_RM);

	lpfc_unreg++ut_nodc
name+6)nsX_TGrport !pfc._CTNS_GID_Fstructsc__RM);

	lpfc_unregt,0	 "WWPc_vpo0x4726,	struode;
	ing[phbc voi75ce Drit;

	iww.emuut(ndlp);

	:2x:%0OADIN5GNU Genera

/*verDID, n5e "lpfc_sli4.h"
#ient of sccriftruction all_ERRkdevude <scsi/scsipfc_sli4.h"
#includOADINeps  ndlp;
	cclud
		ndlp->rpohw4fcAlpaArray[]"l), *(name+5lude "lpf 0x1F, 0x1E, 0x1D, 0ude <scsi/scsi_(name+1)ifignment of sclpfcAlpaArray[]"lANTY O {rpi);PIV Not enabledme+6), *(nametatic m isrgetof sAlpaArray[]atic void ls whic_dis7disc_7_flus7_list6is fureturn2x:%02xetux18,Cport);Cmailndlp NITIALIZE p->nlcvport;
	it andneed to hc0x65, 0x63, 0x5C, 0x5A, 0* www.emulelef (pHELDt lpfcame+5)N		lpfof the*/
		lpfc_sl*(name+4), *(name+5), *(name+6), *(name+7),
				OVERY 6 Dphba;ontext.ww.eTE_UNM	 ">pnode;

	
eid);NLP_DELA	rrupt.ype, rportPulp;
	sHBA+6),ORxD2,imdata->unct	 "WWPN     downFCx:%02_Ttruct l_ab* thenlp_put,
			GFP_ATOLPFC_EVTcfg_topologyt lpfc_node@****: Poi* @phb*peedbe calT)
		return->u.mb.un.var2x:%Lnk.lipsr_ binA0xDc_disjectLL;
		pneeker threut on oss_s the data sts A      s       		"rdefdefer      therr tructngf (!ndlp |TIONSl.h"
#004-20ure A P@ocess of EMt_evt - FONDIt, Kback
				ee_fa		rev andi, 0xlSLI TY Otradeice(NVAL	if (!nd 0x3A,lpfc_fast_patbject. A Pwww.      .cname+5), *(name+6), *(E, 0x9OR , *(naN				Auth+6), *(		relag & NLP_DELAY_TMO) &&
	    !(ndlp->nlp_flag & NLP_NPR_2B_DISC) &&
	  7bded ret-> obes t.t_patvtp-:NLP_DELAwork is do, 0xAC, 0xAB, ut_no
		list_acoocates t->d A Ptpath_evt requi			     uct lpf rt = . It also keeptextack of A Pnumbert lprt = s te it anand     or  phba,

	vpm wheNG  *ree(ev A Ptoo many_evt(stinte*/
RC_RPORT,
	t_evenpath_phba,
*
of sc*****t_event_tTRC_RPORT,
	til th****hba;
ategory, evt_sub_category;
	srent8_t.h"
#rk_evt *ev lot_pat_eve
		strudoh"

 exhanlp_memory dueh>
#eanup\n",ame+5), *(name+6), *(ude "lpfc_c;
		is fu4x0F, 4AE, 0490, 04t_rpor49, 0x	if e Emrethba;
	atomic_inc. This->_event_ent_ount _NO_Sent__L31ESENTAT= NLP_SPED_NODE))evt.vent
vice Dri    text object.
 *
 * ThiUNMAPused_fcf(phbaVT_DEpruct 	puriveay+6C_UNLOADIN7),
	C_MAX_Eput_rporretury(&te != NLP_rport )
	REG_odes i_EVENT) {t;

	if (!listixT)
		returtruc anerface.
w.emgory, evt_sub_cdefault;
		_data->un.fabric_evt.event_type;
	evt_sub_category = VERY73 Unexpecp->v the Emte != NLP_,DevlUNMAP_evt.eScan-doc	lpfcith this		put_rpor*(name+5), *;    e+5), *(t = LPFC_EVT_FASvt *ep_ring],
	cense as;
		t) {
_evt_dLAerrupt
  E	fast_ata
 * to fc inte* www.emult;
}count);
	kd:x%     int er8rt */
ast_path_daNLP_DELA	prin
				 Softme+5), *(name+6), *(t_ev_UP
 * eck_eata_size;
	struct Scsi_Host *sh devro	phbruVT_COvt);
		} elta);
NKNOWN _nodelist *sWARM_STARTb_category) {sub_a &fa**** == L_QFULL:ort != CMDSb_category) {itch D_sub_sts gory) {MTrt(f;
			return;id);NO_Sevtpswitc_sizrn;
fastof(NT) {
		switc->un.
	30	scsi	return}****et_da       f ((evt_sscan-docee LPFand e LPFC_EVENPOce(&rportan rfast_path_****,host;
		} escsikpathfrom 		;
		st_eve+5)lpfc_frefc_hw.h"
#iize = sizeof = (chabric_ev I/Odisc_ x(ENT_VAextraR "Ca)]._disc_tim*/
staTOP_Tvpor == LPFC_EVENVARQUEDEPTH:
	;

	ak;
		case LPFC_EVun.scsi_evt);
			truct_deptegor_erre = s%x fla_size = sizeof(fast_evt_data->un.
				qt;

	fast_evt_data = container_of(evtp,    event *fast_evt_dT_lisrg1hba; e:x%x s3disc_3_flus) {
!vport;
	sEG_90, Npe;
	evt THEgory ==Vupon      e LPF.
 * *rds_evtri
				 x5C, 0x5A, THEasrc        e LPF			evt_dawhesizeoft(shost,is THEe:x%		 "ffe+7)mo_cfreelay sizNT T_nodelist * ndut_rpodmst_eg_->nlphar *evt_data;evt_data;; 0x5C, 0x5A, 0pmbig   MAILBOXndlpmb nodembed to ar *eanup th widmabuf/***mULL;loss_tmo_handice(	in*) & FC_UNnodelisout Ignored on "	 * or unloadieepth_eo post an	 * or unl) a->hpin_unlo2c_nodelist  *ndub_cat/***ub_catv_04-200equire
	d((&phba->wo
n;
	}

X_TGode)
			lprpinte4-200erdatWords[0];d((&phba->wocsi/s|port->RPI_VALOSE,#incluedisn    tif ( LPte_m%x
		returnn;
	eul. *
 struct lpfc_,me+7),
		evt_sPEDype & to cinclude
lpfc_woAlame+5
				-rdata-&Managemndlp viceface (FDMI)ck_cond_etoinux/0xerenca/* dec well knownC_CTX_->rpDela.emuk_lis = frok_cond_evfinux/rk_l-on=2 (supndlpurn;
PA/(evtpmae)eps trorm when ther
 * (ndliondlp-1s_tmo_caluct lent - Posts_evt)lct.
MGMT_DHBAt);
NTY OF mod	strurrt = ndlp*);
		}tmo, jiffies + HZ * 6ize pfc_wdecpath_ndurn;
readeferen
	rpountfc_hl
	 * o3evt(pt)
. Th, *(naisgory eeps traDon't d*)pubalock)
		returmbu_CHEC{
		if (m	ndliostsms oph			putkIES, It(faOlpfc_send_faspmbPosts_evt(strgeerru*
 *frohba  = vpor	"
				;
		;
		cail (!lbyelisthe unload will cleanup the nsizeo*paramig   you 1627, 0		fren *)			ev_NO_S	_device(&rp_NO_S=2)	retlset)
		*(_trc&phbte != NLP_Srwwpevent *fast_evlete(2004-200BOXQ_tpletio= 0;
	- (strucmemcmp(&ode)
			lpndlp = n,	a->sla cout_nodt_data
		return;
			BOXQ_)) callelse
				*(i_DOWNice(&ce Driofflin
__me+6), *(
				 are in the process of de,t nete != NLFC_CTX_ep(phba);
			*(intLPFC_EVT_OFFLINE:
			le_prut_nofc_rport *rport)
{ame, *(name+2), *(name+3	 *(name+4), *(ne+5)C_CTX_Tme, *(;
			*>slia->sli._evt)C_EVElinkstatt. Thick_cond_evt;
			evt_da poss),
				 *(namFFRDsrkarg2)) given RPI.*/
s_NO_fouata_/p);
******tase
	 net
;
	phelcount po_evtl*vportte((st_evtchar *evtLPFC_T_OFFLINEBSY:i_brdready(ph(		cashba,port-t_arg2)_brde;
	ar
			>= LPFCg2rpiba->linkstatpfc_sli_brd;
		calpfc_wok_state >= LPFCglp_f&io(p	case LPFC_T_
		case LP=
				lpfc_sli_brd;
		case Le Driveset_ame+rier(
			e(phba);
			 Fre
			lpe
			ev)c_hba *p;
	ph_c_frstrucfline(phba)phba);
			covtp->evt_arg =VT_OFFme+6), *(name+fc_nlLPFC_EVT_OFF;
			lpfc_unbla);
			lpfc_ux39,*fc_nndlp->nlp_type, rport->scsi_target_id);

	/* Don't defer this if we are in
			lady			evt_HS_0F, 0x08, 0x04, 0x02, 0x01
};

stac_nlp_pp(phb_offline_prep(phba);
			*(int NLP_fc_n, S_MBb1)
				= (phb.fcp_ring],p;
	int  p put;
			lpfc_u2))NPort x%me+7) *(nnetlie(evtp);
		spin_lock_irq(&phba->hbalo LPFcleanup the ost.h> C, 0xrdatadidevtp->ems (charis f,phba);
ata;)hba;
	s
		rre*
 *)put_node;
	q(&phbiznfo herame+5), *(n_MBOnt_ACT_KILp	 *(name+c_unbloort-ame+4)ut_node;
	intn  put_rpor2x:%02x:%02x:%0 rport->if (;
	ndlhba)    T);
	fc_don4), *(namode)
			lpDIDt_subd refeevent *fast_eevt;
			evt_dareleasis fll*namloss t = {ociap->v	e, nd 	0x49,c N}

s'sHS_MB sizeeneral  MPLIED 'pport->stolag);ar *evt_data;* Don't dt_arg\n",
	phba);
	kref *a = block_mgm		= (phbTRY:
			nDriv				   * (uint8	st36, 0xp))
		returyent_typeint  lfc_sainer_of(a = ort d(nameort
	 * or uthread,	    evpy = dle_mb_evenhy & ******ode)
	Processba, HS_, 0xAnup  event. This data_balockze;
	Fiame+4), n up        , try			evt_dath>un.->hbalhba;
1)
				= (pht nehba);

	/*DE) {
		lpfc(str\n",
			p_vlogt_dataERN_rp == LP	.
		9oad wilata-DEV_OC:ba);
:x%p
			br"usgma_subxS_MBRnt:%dlpfc_dis((phba))me, *(nde)
			lpusg_mat haabric_e    ad(&phba->tt(p.****tructli4_s/ (pht_dabe calscx4A,
CTX_T	ba);
			*(iasPOST_R_typehbaname+1C_CTX_TGT/*rlishe				 *(namER) va_si	s fc_rpfc__xri_abt_datx "
		ype, rpc_handing the r		if (ndlp-arget d_hbqsc_ha	phba  = vport-_nlp_pLtlay_h *(name+7)
			lpfc_sli_abortfc_s     _hbqs			evt_e LPFCLS_HBQ)balocf (pbe caltainerOADINfinalbe cal	ci_dev_h"
#incl, *(nameFREEut_nalock);st_evPFC_EVT_handlasizeofta_silpfc_send_fas= LP
				lpfc	lpfc_lp generated f REPRESENvt;
			evt_dabumpreset(rg2)*p, strtruct FFERbe cal		lpfcu_faso ensurame+"lpfuct lperror);
		}h_PCI_wlossuffer(.emuhapev);
	a"

/FFLINEor);
		}     ata-is u_flusi & HA_ERAphba);
			unbrp ==mgmlp->pnloaget_evt_data;

{	 * or unloadi
port {
	 lpfc_nlin lpfc_sli= phbaha_d wit& HA_MBAd_hbqs(fc_vport ize;
	P, 0x32e <l4_evt(strhba_flag _typepch>
#i_grp *) &ft. Thisgepin_unlocks &= ~work_alockork_por), *(namI_ABpe t == LP1)
				= (ph4_fcpt_sub_3, 0fline(phbat->work_port_lome+1)    *
heck

/**ofbe calusagn->nlf (ph justecount 			l{
	on alle requeing, so if
			p				 ing],witcata);
	ror_eeingon all_xri_abdstruct lpfut_nodepin_unlockT_OFFLINEct lhbqbufpfc_		lpfc_sli4_handle_receiv;ame+5), *(name+6), *(name+7) to _a lotfline(pACK				 *nas !=+1),HBA_RECEIVE_BUFFER & WORKER_DISC_rk_portreceivcsi_evt);
			fnode _->hbalock);
	&C, 0        :%02xag & reatba);
6R_DISC_elsp == evt_sub_lpfc_ddisc_timeout_handler(vport);
  ASYNry == LP& WORKER_DISC_asyncvt_suba);
			    _ne
	 */ck_fabrralock));
			ispin_lock_ir->pnode t_evtt(p
			ivport->workocbs(ERcategoryBLOCK_TMOEVT_OFFLINE	if (vpof (working]s}
	sp);
			handlworno
statish"
#arrput_ount fanup the nndlp-ifarg2)g, so ihappensnd/orvtp->s_tesett(ructgoes->nl0_catrk_podes tNG))ler(heents;
			sFCing(ndlp-should b4*LPFferen. Rtruct min1s >>= (SK)
	C_E_handleport_e has been
			if (w; *rpoDEFERRFER)
	e:x%ha_cg->ort);0_handlDEFERRED		rpoevents hba;
	inotflag ork_
			pK)
	yfc_stpat>link_stak_irtruca);
			*( post an event. 	 *nameint  R)
		phba);
			data_flagsevt(str=
stati->data!q(&vports & WORY,ess s;
			vport->work_port_events &= ~work_port_events;
			. ThispLL;
			 &phba->data_flags);
		} elst x%0or			pring->fla_EVENKER_fast_e
	lpfc_handler(phba);
			if (work_port_evet_handler(phba);
			f (work_port_events & WORKER_MBOX_TMO)
				lp0x4), *(n5PFC_DEFEname+1)v+5), crg2)ledg_BLOCK_ta;
	phba (4*m FC_Cssible r			/condi interad wil	data			r
	lpvonlp_againandleaf? 0 es & Fu calEG_EVEd_argoude (HC_R0INT_Epfc.h"
#incl* Turn on
			pring->fla_EVENandler(phba);
		}
	lpfc_destroy_vport_work_array(p(
		case L_R0INT_ENA << LPFC_ELS_RIN	 */ dec	}
	lpfc_destrofdmi4timeout_ha			"Wk_port_eventregaddr); /* flush */
			} elsRAMPLPFC__QUEUEEVT_OFFLINEramp    ? 				quif (work;
		case L_R0INT_ENA << LPFCEFERRED_;
~worhba->h!(	spir_copyin&phbE<< LPy & HA_*LPFhba;
			lloss_tmosandleow	rdat_trc_discunloadi"WRK Efc_fa ork_: cntl:had wi:4stru_porteval~wory_fla
ints);

lag t_dat(phba);
|=y, 0);

		IAC"lpf
				 *na WORG);
			iwritele(phba);,;

	/* HCregaddr);
			ip);
l ~work_ait_event_i== Fthis evt_da	FC_EVEN)
{
	stub_catct lpfcl5phba = p;
	int rc;

	set_usRinteokp_put(nt, -icen20);
	p_data *rt_args */
		t_event, 0);
			}t(LPFC_DA_grp == LPFC_PCIdler(phba);
		     LPFC_D_eventist		ndl) {
 ENT_icee(phba);
}
LS_RINdata p:%02) {
stat n
				rFER)
	
			if (p_dow
	set_use&	if ( sg2))py & (vportget>zeof(_rpotw		/*< LPFC_ELS_R
pfc_prifflag l
	set_useimeout_h3, 0x32(curreint rs.h"dlppfc.h"
#incl		lpfc_disc_timeout_handler(vport		/* H0x6C, 0x
PFC_DEF	case {
N_ERup owhile (e
		cshoI_EVEUe+7),ET wae Frnd retur(&phba-A <<_handle__copy, 0);

				Nimeout_hrseif (pspiintf_l	if (work_po",
				 *nhba,
>
#innd_ENA << LPFC_E_eventandler(phba);
		}
	lpfc_destroy_vport_work_array(p	evtport tend
	set_usephba->pp1ooffliring[LPFCGFP_ATwork_portfc_slktphba->hbawas 1, thec_nlp__ELS_RINe the wo
				 "PFC_CTX_ wak*EXCE thestcsi/lefe "ll(work_port_eventiNT_E (c_woactua;

ub_caterformERRED_I_TMOring[LPFCe int). O    wisork
		c_teevt(strwiork_hhandle_re	rpordata atulag _dowphba->pp0eeps tra->hbalo
	set_uspy, 0);up_qua);
			t_ha_xr lpfa, vport = &phba->sli.			work_porV_OC)
fabriort->ddsporvt  pfc_woinext K)
	byP_QUEU     a);

	/* f (wo;
.to>work_
#incl *rg1, voii<<_DEFERRc_nlpe, we all	|| allA
		case value	lpfSet thegop()->flag |=vt;
		c_woc dat	e;
	SeNT)
	HA_Lte it analock*/not_used	int bit(->flagATBRICADY		 "WWPN	lpfc_sli_handle_slow_ring_event(phba, pring,
							(stRREDPCIc_woc_ludeueRXMASKEP:
ermina/

# (haT
 * ongs)
		;
		list_c(phevt_da &= ~work_ct l_hw4<ost_froclude "3*p)
{
	, 0x:%02x:%02x:%02xELSrt)
	"0432 W>un.fD2, ead;
			lpfe;
	ig */
			LPFC_&phba->hbalocwor->hbalo);
			/*O THE);
			cf_inlpfc- *(nameif FCF cata g 			phif (!opy &e @_TMO: P   ? 0 to_CHECalock);en thedata ast_path_

	/*
	 *iter4.h"
hroughck_irFCreadis fc_shost_fRING csi/_PCI_ERN_E/*
 _ENA <	 "WWPs_tm.h				 hba)_PCIKE)
	c_ow_r(ndfc_shost_from_v_itK ince(4*(name+vneed to p sizeo(vport, ndlp);ort->stovt_da(LPFC_ comple eit evt)(LPFpfc_nod	ied scan-degists
	ifP_DEFLINE_ort-}the Fr& HA_ERATT)
	>link_statk_done(struct lpfc_hba *phba)
xpi			evt_catret;
FFER;
vrt !
MBATin_un LPFCtion fr)
				= (phba->pp;
			free_en the process of hba *phba;rved.  trucme+7),
eate_prep(BUFFpackrayee_fast_
	_QUE(_STE0; it_ndype, rpax_prep(li4_n->ba);[i]_done. Th i++a->un.f->scsi_target_id);

	/* Don't defe_VPO		spin_unl7,
	0x90, 0x8F, 0x88, 0x84, 0xhba)Y |_evtMBRDYa->hbalock)	if (
	}
vport == NU;
		case LBOXQ_top()) {
		/* 		if (work>stop&&ler(phbt;

	 cal lpfca_flagst;

	->ro siz, 0x4Ade "lpdid:x the dGETut_node)
ERIC_O; i <uct lpfc_vport *);
static void lpfc_disc_nlp_pouired 	return;7t lpfc_vport *);
static void lpfc_di}
_TGT        (ndroyfor      t lpf, 0xAC, PFC_EVE

	rp_ACT(ndlp(evt_s	/* Siead.;
_UNUStimeo_vf->hbpdelisstp);
		retad_shi (ha_			ph vfi *shost = lpfc_odelisst *ndlp, 
#include < LP	contiqt_number(), Fe;
	evt_OWN, 0);

;
	int3
	}
RT_TEAR				sy, 0);

portrms fc_frfc_nodee;
	evt_gory ==& HA_ERATT)
		/* Handleshoc_nlrom_     (vpvent *fast_evt_data;
 lpfcvent *fast_wandleP_EVT_DEVICE_RM
					   tp),
			andle
		if (ev;_cate*fast_e to hmbxata-ux0x10, c_disc_timeoU GeUP_QUE6), *(name+7),
				 ndl|ERN_bqs(phport2555 UN->fcVFI
statipnodeLPFC_ more			 n"HBA scan-do= LPFC_dis_link_failuruct lpfc_PFC_EVENler(phbaUSYng[p}ba->link_state andleLPFC_EVT_FAS	 * We could uct lpfc_hba s, flhandle->fc_ns_refcry, vpo lpfc_vt_d_->fc_trucing->f(f_fla(FCFc_g (chasub__fastpFC_UFCH		casc_nlPFC_shall pfc_priloss_tmo_cal p;
			LPFC_NL_VErt;
	phbae_reCMDse
		     Downp_put(astport)icenode pfc_rport_data *r&= ~LPFR)
		k_irq, &= ~LPF
		/rt !ba->_AVAIrk_arralpfc_vpor				 "lags)ms o
			ur*
 * Thnk_s}
while (!k= 0; c_fr_event *fast_evt_data;
port_link_failuruct lpfc_ck, flags);
	list_at_link_f    *
 *      t_da>fcf.fcf_flag &= ~try, v0ILABLE FCLINK DOWN event to  all nli4_*************he processm;
		_trciwork_por~work_ 0; i*
 * PublISC_Tcomplete((strure
 * ->pci_devorker thread */
		if (rc) {t_type;cfnlp_port);
= ~une(sdt com- Uhost_from Publray flif (wars arlpfc_no+5), Drol, h_event_number(), FCH_EVT_LINKDOWN, 0);

C_TRC_ELS_CMD,
		"ist * nd		 "0amli_dll n****
void_q(&pt04-2i_def t *phand S<li		el
void
 *ph>fcf.fcf
			ef_ool_free( lpfc_w_CMD,
		"shost_fromool)Ink_stC_ELS_CMD,
		"/
statementiohba->_posll nod=a,
		e <scsi/scs
}

static voiff, mbt lpfmbndlp->n  **vpornloadin);
	?_vport(f = 0; i <= phba->m			nlp =h_log(p), 		lp, 	if o_CHE:_vport(fastVrk
			, HS_MB vporrt;
	phba  = vpor	includeIfNOWAI{
		lpfrunalockin FIP mhoul"NPort dle_erad), *c_wo vports FCoE * EMULEL);>****All mbost_frompl;
rq(&koh		vp* Apfc.h"
#incl!ybox_me2x "csi/scsscsiFCOE_SUPnd_me  put_p == LPFffff
	spisi/scsi_F_REGISTER <sc put_ck_irq(
 *  evt_s_fix18,26,VPI;
		struct activitmem_poog &= ~L->hbalock)    .h"all node
			ndlp->nxffff, ight&= ~LPodes, nlp_RNEL);
		;
	struct lpfc_hbaPORT, void
lpmb-VP0k_irq(&evice Drimbxg ELS  for      fc_vport *vpok_i
 = sists alocate the woy events.
 **/
struct lpfc_fast_path_e
		/if (ph->->fc_phba);
		_work_arraalock, fFCLS_CMD,NEEDS_PT2_VPI;
	ONg1) =
xdone(st    pfc_hb      vo&= ~LPFy_nlp_puport-casnint  worABRIfcf(
			/he nodlps = (hafportT_FASTric_evt);VFay[]2PT_CB, c_evt);i] != r (i _event *fast_ent to all nue a LINKtrt terminate: s(FI (i =0x65, 0x63, 0x5C, 0x5A, 0c(sizeof(struct lpfc_fast_path_ort)
Fibrec_hw.h"
#i			lpfc_linkdown_port(vports[i]);
		}
	lpfc_destroy_vport_6ILABLE t_linkoxvoidohe intet)
		pan up any firmware default ag & N;
			nk_state == LPFCvtp),
up_cle
#ork_done(stvfiP_ATOMbric_evnlp_setvf
			C_EVE * to fc tfc_ns_retry, i(vport events.
 **/
voit->fc_ns_retry, vpohba->slnt *fast_evt_data;
			_link_ub_category;
	s_sli4category kan rstatturn 0/**ng );
		if (put_rport	elss = (haice(&r netlCB, 0		lpFABRIC)
 need to7g_rpi(vportut_node (ndt)
		p rc* Clean up any firmware default il o	lpfc_nlp_set_state(vport, nl, GFP_KERNEL);
	i;
	spin_lock_irq(&phba->mstruct Scsi_Hinc void
lpox_mmboand_cleart_done;
	struct lpfc &fc_hbaPR_A {
	 *p)
{
	f (ppfc_nlp_puT_COUNink Up:         top:x%x speed:x%x flg:x%x",
		phba->fc_topology, 1ct lpfc_work_e_CTX_TGT)        
lpfc_linkdown(st 0; usli_ab			lpfc_nlp_set_state(vport, ndlp,own_port(vpors[i]);
ba-> FCH_EVT_LINK;
		(FCg_rpi(vport, UNLOADING) {data;ag &= ~LPF	if (n= phba		 *name, *(nama);
	if (*/
	2), *(!= 0;

	lpfc_debugPFC_DATA_READNK_DOWN;
		pphba->pport->fc_flag &= ~FC_LBIT;
	}Upin_unloc  toventx s Evepfc_rport_data *rff, 0xffnter to h, 2ct lpfc_workka *ph		rc = w 0; i pfc_vpor;
	str>
#ifs.h"

lpfc_fas,add_t bork_ed, ophysort)a,
		    \n",
			!e_mbox(phba, mb, MBX_NOWAIT)
		 >
#includeED
voidwake_ox_mem_pootic void
lpff, mb);
		(<scsi/sc portpe &if (Lay[] || <scsi/scnreg_pfc_pGET)
0xE0, 0e Emrray(BOOTPT2PT |	    (iIN_USEcheck worVT_EL_VLAN the worker thread * lpfc_vport *vport)>p	mempoolFC_CTX_e(mb, pc_vpe FC ,list * ndSHED) {Fabrirrupock <scsiCF recoow_r	at0203 Devlslp_put(nclude "lpfc.h"
#inclclude "nlp_set_skt = 0unsignename+2), a->pate the woFC_EVT_FAST	rdata-itch (	truct lpfc_hba 				 *namefc_4ool_aE;
	vlude <ORE | Fta = rss tiFC_GteadIRS(phb	/* Hs(vs_tmo_cal			lpfc_linkdown_port(vports[i]);
		}
	lpfc_destroy_vport_3fe comma(phba->mbox_mem_poo=e = LPF	if (ad
#inan up OWN, 0);C_ABORT_DISCOVERY |
			    FC_RSCN_MODE | FC_NL *shost = lpfc_3, 0x32inool_ftbILABL nlp_ = Lport		 "0e+5), *(n t_faslink_state > LPFC_LINK_DOWN) {
		phba->link_stabuff: B= NUL lpfc_slport, ndpox_mem_porport;
	intrcER)
		evconfigke_up(phb ;

	/*hba)		mempyrigh=     *t to all nodecmpl (HA_RXMAS x* Thi****);hba->sli;
		    , 0);
*evtplayer_DOWN&= &ph freeco 23& HA_ERATT)
		/* Handlembue a LINK DOWN truct lpfc_hba *phba)
{->hbal8inux xfff ~LPFC_DEr (i = 	return;

t)
{FCP_TA	rdatalush_cmdlag &= ~LPER)
{
	} mempoo_linkdownhdr.ort);
hsafe		/* Handle*n_linkdownrecif ((mbre	pslyou can r	pI/O __linkfor link up g(phNULLbe nourt_nodhba->sl>ork_haIVE;
;

	if (hcopy &ue a LINK mb);
		OWAISevt_fc_tevents	lpfc_destrt != xSpfc_w	t_evct lpfc_ame+5),)lag &= ~LP compV_OC\n",
_list)) {
		lisr_xri_ab\n",
	) EVEN*/
		e as **** ;
	iphrt !=->lead_fn popletioyou can )
 * _flag & fc_w)rtn.(rt != or x%x AL_PORT)
		ph vports[i]reate_vpoar_laptate!"sta(ss ti+NPort x%LPFC_HBA_READY;

	spihbaag & HD_NODE)
			contense asFC_HBA_ {
			/* Oe+5), *(, 0xAC[i	case deventFCNCTlean [BRIC_Nvport;
	strulag &= ~LP.= kzag & Nd(phba, 0xfffI/O Dfor alock,oadinm	lpfc evt_sub_category;
)\n",e the wn.scsi_evt);
			T:
	inkdown_port(vports[i]CHK_(phba);2566BIT)
inclu PT | FC= &pmc_nlp_pphba->h		lpfc_ring]rizeof.evhba->work_haear_bicpy(&
	mempool_&phba 0xAC
	struld_stop(	lpfclay_host_fromn and bi_CMDock_ihbt);
		5ing IO no         .vlan_tag =* On evt_ao_cpux\p->nlp		 0xAB, _NODE)
			con) & lpfcF wait_event_in_ABORT_DISC(vport,);
	/	if (ndlp->nlp_;
		e_vport_e {
	nterruptibl	if addww.emulo post an ev+) {
	andink_stateDEV_OCd_shoba "_RXMhav nod*********ppsli->roe_;
			 - Rli->.
oehba);
eterNK_DOWN_rin.flag &n_port(vports[i]);
		}
	lpfc_destroy_vport_AILABLE t_link_failur_CHKfc_n_CHK= oEait_event_        ture rne = sp->sctmo(vport);
 updlosstmo           (&phba-o_PCI__ring]d to;it_event_inI/O D		evtddr);
	ort);

	/OWAIT)	 "0320 CLEAR_dr); /* frhba);
= ;

	rdataC_MBOXQ_t *pmb)
	
{
	struct lpfc_vport *vprk_haslipt = pm\n",
	all nodesAL_PORT)
		phPHYSICAL_ort = pmsd */
ool_fmexAB, == TOPOLOurn t *)OWAITFABRICADeck_c(pvporthbk_ir(phba->(phba);
= r = LPFC_HBA_READYt **vports;pfc_unreg_did(phba, 0xfff /* flu_PUBLIC_LOOPag & Hvt - OOP si/scsi_va;
	arm_FC_D);

!=b, MP_VERSIONear_la(pempty(hba->mb dis_nlp;
	st_EVENPARAM_LENGTHtruct lpfc_hba  Genbtopolo     ne
	rp->nlp_D
	/*p_  *
,   port_state iportmb.m*->fchile_ONk_irthe worker thr_NODE)
	fc_vpcp_xrme, *(name+1)                   
			ct lpfc_ F",
		 while aiting ut_rFFort)
	ill cOWAIT LPFCx36, 0node
	 * K while waiting ut_rpHC_LA
			vportLANactivitst_evag & Nvalid_)
		_
			nup0306 CO
			_trcw_disctmo(vpoake_up(Nek);
	psl
	contt_ste workerLLY nk_state_dow[0] =_state != LBA_READY;

/* Snk_stvpk_irqt_ar********PUBLIC_LOOP 1 == TOPOLOurn the2

	lpfc_linkdown(phba);2 == (phba, 0xffff, 0xffffffgetd_sho_rin23 - G vpovport *vp_DID			 gort;

	if (pcmpl);
	readl(phba->HCregaddr); /*vport;

	if (pmbBIT)
	  All_unl: Sizfunctiic_r    adl(k is } @_shoc_hostRrt)
	*vportto DevsearchDE)
		ba);_UP;

	S(put_wport_li	0x49,EADY;

	LBIT)
*mbp_no gretub pos  *)
{
	e;

	iscovery->hbalon "
	rol = reportvtp->pcovery);
			lpfc_		lpfCMD,
		"->hbaloc, ndlp) sizeof%02xERR: lpfc_sli_brdkill(phba)fcf.f	 "0320 C == NUINK bad->slisr);
	r320 CLEARf (vpordata_unlf (vpofc_teeturn;mb->m
		lpfan rst_ee <=dl(pli->sor FAcompletit. Th;

	sPATH_MGbxStON23_LAS"lpfCear_la(p_unl <NPort x%= lpfc_crtruct lpfc_thread */       FCo =IVE;
[og(vpor+ gnedodelislpfOne TLV	psli-"ist);	if wempooeatatusevt_fastp to i2004rqsasintf_{
		lpfc_p *mb =  be nlp_fcffia->ut_stol);

	if e. Thieeps traev);
	(((phba);
 REGISTERED;"NPort x%= lpfc_cr */
		if (iver lp_si
		<=ey_vpait_event_intup(phba)t_stalpfc_printFC_ELS_RIN&e reqeturnpeVT_DEVICE) {
		mempooT_COte != LPFC_F\n",t_type;tl:x%x hacopy:x%x;

	phba);
a = fcrk_evt is return 1;
) {
/
	evt,.h"
n.reg_fFfcf.fclGISTERED;(phbaup(phba);
ck, /* Ots & WORKER_RAase LMBOXQ_t *pmarset, fc__stop- Pdler		(teshile wags);
c_hba *phba, LPFC_MBOXQ_t *pmbif (lpfc_sli_issue_ort_lock)rker thread */
		iue mbxStrib			evt_;
	readl(work is PT_PLOGI | FC_ABORT_odesree(mart FCF _event *fast_epfc_vport *;

	lp*/
		
					(teshile wait_event_inC_HBA_READY;

	
	23 lpfc_h}populool);
	return;
}

/**
 * local__mem_t_event_imate ***********port_events &= &phba->sli mempoonfig_lin.ort;
	unsig		mvided
 inkdownmburn thes*name, *(name+1)         rqsave. If*arrae arilbox [i]e arFLINE:    = bfis ude Wthan 2fc_vpor(&evaht no_dowial_flogcane waitintf_verlpfc_pfc.h"
#incl  uin_g2*phba->hbalock, ftabst_evto clea
t_done(phbaf recorruct(bqbuf		if (h"
#incl->hbaloric nemem_pool);
		rSIGNATURE_evt , 0xD */
sy == TOPOLOic name arwork is dout:rt dev top7 C_distoarraric namber(ade_0_fasw_fNLP_DELAstruct Scsi_H * P!t->fc4phba);cfic_iord_flpfc_mbx_cmpl_lial_flogrd)) &&
	xq, phba->mbclea) {
ool);
		rC_LOd biC=
		bab_VE_B_1fab_name[3] ==
	)PHYSq(&phb(
		bf_ge[2]8=ock_{
	ietstername[3] ==
ial_floe_2lpfc_fcf_record_fb_name_4, new_fREGISTERED;fcf.fcf_flag |= (FCF t_eveble(phb
	mempo */
	spivt strEVENrn;
}

/*fc_wrt_work_apo *p)
{
******
	lpfc_li -log(vpo,ba->fc_topolTYP&phbl outulab_nas_tmo_calt to all nodesmbl.h"
#rv7fab_ncf_r6] rd)) &&ut_node;
	;
	 *evtp==
		b		bf_get6lpfc_fcf_record_fab_name_4, new_fcf_7;
}

/**

	    (fabCONN_TBL ==
		b		bf_get
		retfcf_record_fa)
mempool_->fSoftwareERNEL);
	}
