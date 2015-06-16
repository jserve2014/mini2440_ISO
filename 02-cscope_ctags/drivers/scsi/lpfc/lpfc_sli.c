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
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/fc/fc_fs.h>

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_crtn.h"
#include "lpfc_logmsg.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "lpfc_vport.h"

/* There are only four IOCB completion types. */
typedef enum _lpfc_iocb_type {
	LPFC_UNKNOWN_IOCB,
	LPFC_UNSOL_IOCB,
	LPFC_SOL_IOCB,
	LPFC_ABORT_IOCB
} lpfc_iocb_type;


/* Provide function prototypes local to this module. */
static int lpfc_sli_issue_mbox_s4(struct lpfc_hba *, LPFC_MBOXQ_t *,
				  uint32_t);
static int lpfc_sli4_read_rev(struct lpfc_hba *, LPFC_MBOXQ_t *,
			    uint8_t *, uint32_t *);

static IOCB_t *
lpfc_get_iocb_from_iocbq(struct lpfc_iocbq *iocbq)
{
	return &iocbq->iocb;
}

/**
 * lpfc_sli4_wq_put - Put a Work Queue Entry on an Work Queue
 * @q: The Work Queue to operate on.
 * @wqe: The work Queue Entry to put on the Work queue.
 *
 * This routine will copy the contents of @wqe to the next available entry on
 * the @q. This function will then ring the Work Queue Doorbell to signal the
 * HBA to start processing the Work Queue Entry. This function returns 0 if
 * successful. If no entries are available on @q then this function will return
 * -ENOMEM.
 * The caller is expected to hold the hbalock when calling this routine.
 **/
static uint32_t
lpfc_sli4_wq_put(struct lpfc_queue *q, union lpfc_wqe *wqe)
{
	union lpfc_wqe *temp_wqe = q->qe[q->host_index].wqe;
	struct lpfc_register doorbell;
	uint32_t host_index;

	/* If the host has not yet processed the next entry then we are done */
	if (((q->host_index + 1) % q->entry_count) == q->hba_index)
		return -ENOMEM;
	/* set consumption flag every once in a while */
	if (!((q->host_index + 1) % LPFC_RELEASE_NOTIFICATION_INTERVAL))
		bf_set(lpfc_wqe_gen_wqec, &wqe->generic, 1);

	lpfc_sli_pcimem_bcopy(wqe, temp_wqe, q->entry_size);

	/* Update the host index before invoking device */
	host_index = q->host_index;
	q->host_index = ((q->host_index + 1) % q->entry_count);

	/* Ring Doorbell */
	doorbell.word0 = 0;
	bf_set(lpfc_wq_doorbell_num_posted, &doorbell, 1);
	bf_set(lpfc_wq_doorbell_index, &doorbell, host_index);
	bf_set(lpfc_wq_doorbell_id, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.WQDBregaddr);
	readl(q->phba->sli4_hba.WQDBregaddr); /* Flush */

	return 0;
}

/**
 * lpfc_sli4_wq_release - Updates internal hba index for WQ
 * @q: The Work Queue to operate on.
 * @index: The index to advance the hba index to.
 *
 * This routine will update the HBA index of a queue to reflect consumption of
 * Work Queue Entries by the HBA. When the HBA indicates that it has consumed
 * an entry the host calls this function to update the queue's internal
 * pointers. This routine returns the number of entries that were consumed by
 * the HBA.
 **/
static uint32_t
lpfc_sli4_wq_release(struct lpfc_queue *q, uint32_t index)
{
	uint32_t released = 0;

	if (q->hba_index == index)
		return 0;
	do {
		q->hba_index = ((q->hba_index + 1) % q->entry_count);
		released++;
	} while (q->hba_index != index);
	return released;
}

/**
 * lpfc_sli4_mq_put - Put a Mailbox Queue Entry on an Mailbox Queue
 * @q: The Mailbox Queue to operate on.
 * @wqe: The Mailbox Queue Entry to put on the Work queue.
 *
 * This routine will copy the contents of @mqe to the next available entry on
 * the @q. This function will then ring the Work Queue Doorbell to signal the
 * HBA to start processing the Work Queue Entry. This function returns 0 if
 * successful. If no entries are available on @q then this function will return
 * -ENOMEM.
 * The caller is expected to hold the hbalock when calling this routine.
 **/
static uint32_t
lpfc_sli4_mq_put(struct lpfc_queue *q, struct lpfc_mqe *mqe)
{
	struct lpfc_mqe *temp_mqe = q->qe[q->host_index].mqe;
	struct lpfc_register doorbell;
	uint32_t host_index;

	/* If the host has not yet processed the next entry then we are done */
	if (((q->host_index + 1) % q->entry_count) == q->hba_index)
		return -ENOMEM;
	lpfc_sli_pcimem_bcopy(mqe, temp_mqe, q->entry_size);
	/* Save off the mailbox pointer for completion */
	q->phba->mbox = (MAILBOX_t *)temp_mqe;

	/* Update the host index before invoking device */
	host_index = q->host_index;
	q->host_index = ((q->host_index + 1) % q->entry_count);

	/* Ring Doorbell */
	doorbell.word0 = 0;
	bf_set(lpfc_mq_doorbell_num_posted, &doorbell, 1);
	bf_set(lpfc_mq_doorbell_id, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.MQDBregaddr);
	readl(q->phba->sli4_hba.MQDBregaddr); /* Flush */
	return 0;
}

/**
 * lpfc_sli4_mq_release - Updates internal hba index for MQ
 * @q: The Mailbox Queue to operate on.
 *
 * This routine will update the HBA index of a queue to reflect consumption of
 * a Mailbox Queue Entry by the HBA. When the HBA indicates that it has consumed
 * an entry the host calls this function to update the queue's internal
 * pointers. This routine returns the number of entries that were consumed by
 * the HBA.
 **/
static uint32_t
lpfc_sli4_mq_release(struct lpfc_queue *q)
{
	/* Clear the mailbox pointer for completion */
	q->phba->mbox = NULL;
	q->hba_index = ((q->hba_index + 1) % q->entry_count);
	return 1;
}

/**
 * lpfc_sli4_eq_get - Gets the next valid EQE from a EQ
 * @q: The Event Queue to get the first valid EQE from
 *
 * This routine will get the first valid Event Queue Entry from @q, update
 * the queue's internal hba index, and return the EQE. If no valid EQEs are in
 * the Queue (no more work to do), or the Queue is full of EQEs that have been
 * processed, but not popped back to the HBA then this routine will return NULL.
 **/
static struct lpfc_eqe *
lpfc_sli4_eq_get(struct lpfc_queue *q)
{
	struct lpfc_eqe *eqe = q->qe[q->hba_index].eqe;

	/* If the next EQE is not valid then we are done */
	if (!bf_get(lpfc_eqe_valid, eqe))
		return NULL;
	/* If the host has not yet processed the next entry then we are done */
	if (((q->hba_index + 1) % q->entry_count) == q->host_index)
		return NULL;

	q->hba_index = ((q->hba_index + 1) % q->entry_count);
	return eqe;
}

/**
 * lpfc_sli4_eq_release - Indicates the host has finished processing an EQ
 * @q: The Event Queue that the host has completed processing for.
 * @arm: Indicates whether the host wants to arms this CQ.
 *
 * This routine will mark all Event Queue Entries on @q, from the last
 * known completed entry to the last entry that was processed, as completed
 * by clearing the valid bit for each completion queue entry. Then it will
 * notify the HBA, by ringing the doorbell, that the EQEs have been processed.
 * The internal host index in the @q will be updated by this routine to indicate
 * that the host has finished processing the entries. The @arm parameter
 * indicates that the queue should be rearmed when ringing the doorbell.
 *
 * This function will return the number of EQEs that were popped.
 **/
uint32_t
lpfc_sli4_eq_release(struct lpfc_queue *q, bool arm)
{
	uint32_t released = 0;
	struct lpfc_eqe *temp_eqe;
	struct lpfc_register doorbell;

	/* while there are valid entries */
	while (q->hba_index != q->host_index) {
		temp_eqe = q->qe[q->host_index].eqe;
		bf_set(lpfc_eqe_valid, temp_eqe, 0);
		released++;
		q->host_index = ((q->host_index + 1) % q->entry_count);
	}
	if (unlikely(released == 0 && !arm))
		return 0;

	/* ring doorbell for number popped */
	doorbell.word0 = 0;
	if (arm) {
		bf_set(lpfc_eqcq_doorbell_arm, &doorbell, 1);
		bf_set(lpfc_eqcq_doorbell_eqci, &doorbell, 1);
	}
	bf_set(lpfc_eqcq_doorbell_num_released, &doorbell, released);
	bf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFC_QUEUE_TYPE_EVENT);
	bf_set(lpfc_eqcq_doorbell_eqid, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.EQCQDBregaddr);
	return released;
}

/**
 * lpfc_sli4_cq_get - Gets the next valid CQE from a CQ
 * @q: The Completion Queue to get the first valid CQE from
 *
 * This routine will get the first valid Completion Queue Entry from @q, update
 * the queue's internal hba index, and return the CQE. If no valid CQEs are in
 * the Queue (no more work to do), or the Queue is full of CQEs that have been
 * processed, but not popped back to the HBA then this routine will return NULL.
 **/
static struct lpfc_cqe *
lpfc_sli4_cq_get(struct lpfc_queue *q)
{
	struct lpfc_cqe *cqe;

	/* If the next CQE is not valid then we are done */
	if (!bf_get(lpfc_cqe_valid, q->qe[q->hba_index].cqe))
		return NULL;
	/* If the host has not yet processed the next entry then we are done */
	if (((q->hba_index + 1) % q->entry_count) == q->host_index)
		return NULL;

	cqe = q->qe[q->hba_index].cqe;
	q->hba_index = ((q->hba_index + 1) % q->entry_count);
	return cqe;
}

/**
 * lpfc_sli4_cq_release - Indicates the host has finished processing a CQ
 * @q: The Completion Queue that the host has completed processing for.
 * @arm: Indicates whether the host wants to arms this CQ.
 *
 * This routine will mark all Completion queue entries on @q, from the last
 * known completed entry to the last entry that was processed, as completed
 * by clearing the valid bit for each completion queue entry. Then it will
 * notify the HBA, by ringing the doorbell, that the CQEs have been processed.
 * The internal host index in the @q will be updated by this routine to indicate
 * that the host has finished processing the entries. The @arm parameter
 * indicates that the queue should be rearmed when ringing the doorbell.
 *
 * This function will return the number of CQEs that were released.
 **/
uint32_t
lpfc_sli4_cq_release(struct lpfc_queue *q, bool arm)
{
	uint32_t released = 0;
	struct lpfc_cqe *temp_qe;
	struct lpfc_register doorbell;

	/* while there are valid entries */
	while (q->hba_index != q->host_index) {
		temp_qe = q->qe[q->host_index].cqe;
		bf_set(lpfc_cqe_valid, temp_qe, 0);
		released++;
		q->host_index = ((q->host_index + 1) % q->entry_count);
	}
	if (unlikely(released == 0 && !arm))
		return 0;

	/* ring doorbell for number popped */
	doorbell.word0 = 0;
	if (arm)
		bf_set(lpfc_eqcq_doorbell_arm, &doorbell, 1);
	bf_set(lpfc_eqcq_doorbell_num_released, &doorbell, released);
	bf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFC_QUEUE_TYPE_COMPLETION);
	bf_set(lpfc_eqcq_doorbell_cqid, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.EQCQDBregaddr);
	return released;
}

/**
 * lpfc_sli4_rq_put - Put a Receive Buffer Queue Entry on a Receive Queue
 * @q: The Header Receive Queue to operate on.
 * @wqe: The Receive Queue Entry to put on the Receive queue.
 *
 * This routine will copy the contents of @wqe to the next available entry on
 * the @q. This function will then ring the Receive Queue Doorbell to signal the
 * HBA to start processing the Receive Queue Entry. This function returns the
 * index that the rqe was copied to if successful. If no entries are available
 * on @q then this function will return -ENOMEM.
 * The caller is expected to hold the hbalock when calling this routine.
 **/
static int
lpfc_sli4_rq_put(struct lpfc_queue *hq, struct lpfc_queue *dq,
		 struct lpfc_rqe *hrqe, struct lpfc_rqe *drqe)
{
	struct lpfc_rqe *temp_hrqe = hq->qe[hq->host_index].rqe;
	struct lpfc_rqe *temp_drqe = dq->qe[dq->host_index].rqe;
	struct lpfc_register doorbell;
	int put_index = hq->host_index;

	if (hq->type != LPFC_HRQ || dq->type != LPFC_DRQ)
		return -EINVAL;
	if (hq->host_index != dq->host_index)
		return -EINVAL;
	/* If the host has not yet processed the next entry then we are done */
	if (((hq->host_index + 1) % hq->entry_count) == hq->hba_index)
		return -EBUSY;
	lpfc_sli_pcimem_bcopy(hrqe, temp_hrqe, hq->entry_size);
	lpfc_sli_pcimem_bcopy(drqe, temp_drqe, dq->entry_size);

	/* Update the host index to point to the next slot */
	hq->host_index = ((hq->host_index + 1) % hq->entry_count);
	dq->host_index = ((dq->host_index + 1) % dq->entry_count);

	/* Ring The Header Receive Queue Doorbell */
	if (!(hq->host_index % LPFC_RQ_POST_BATCH)) {
		doorbell.word0 = 0;
		bf_set(lpfc_rq_doorbell_num_posted, &doorbell,
		       LPFC_RQ_POST_BATCH);
		bf_set(lpfc_rq_doorbell_id, &doorbell, hq->queue_id);
		writel(doorbell.word0, hq->phba->sli4_hba.RQDBregaddr);
	}
	return put_index;
}

/**
 * lpfc_sli4_rq_release - Updates internal hba index for RQ
 * @q: The Header Receive Queue to operate on.
 *
 * This routine will update the HBA index of a queue to reflect consumption of
 * one Receive Queue Entry by the HBA. When the HBA indicates that it has
 * consumed an entry the host calls this function to update the queue's
 * internal pointers. This routine returns the number of entries that were
 * consumed by the HBA.
 **/
static uint32_t
lpfc_sli4_rq_release(struct lpfc_queue *hq, struct lpfc_queue *dq)
{
	if ((hq->type != LPFC_HRQ) || (dq->type != LPFC_DRQ))
		return 0;
	hq->hba_index = ((hq->hba_index + 1) % hq->entry_count);
	dq->hba_index = ((dq->hba_index + 1) % dq->entry_count);
	return 1;
}

/**
 * lpfc_cmd_iocb - Get next command iocb entry in the ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function returns pointer to next command iocb entry
 * in the command ring. The caller must hold hbalock to prevent
 * other threads consume the next command iocb.
 * SLI-2/SLI-3 provide different sized iocbs.
 **/
static inline IOCB_t *
lpfc_cmd_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	return (IOCB_t *) (((char *) pring->cmdringaddr) +
			   pring->cmdidx * phba->iocb_cmd_size);
}

/**
 * lpfc_resp_iocb - Get next response iocb entry in the ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function returns pointer to next response iocb entry
 * in the response ring. The caller must hold hbalock to make sure
 * that no other thread consume the next response iocb.
 * SLI-2/SLI-3 provide different sized iocbs.
 **/
static inline IOCB_t *
lpfc_resp_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	return (IOCB_t *) (((char *) pring->rspringaddr) +
			   pring->rspidx * phba->iocb_rsp_size);
}

/**
 * __lpfc_sli_get_iocbq - Allocates an iocb object from iocb pool
 * @phba: Pointer to HBA context object.
 *
 * This function is called with hbalock held. This function
 * allocates a new driver iocb object from the iocb pool. If the
 * allocation is successful, it returns pointer to the newly
 * allocated iocb object else it returns NULL.
 **/
static struct lpfc_iocbq *
__lpfc_sli_get_iocbq(struct lpfc_hba *phba)
{
	struct list_head *lpfc_iocb_list = &phba->lpfc_iocb_list;
	struct lpfc_iocbq * iocbq = NULL;

	list_remove_head(lpfc_iocb_list, iocbq, struct lpfc_iocbq, list);
	return iocbq;
}

/**
 * __lpfc_clear_active_sglq - Remove the active sglq for this XRI.
 * @phba: Pointer to HBA context object.
 * @xritag: XRI value.
 *
 * This function clears the sglq pointer from the array of acive
 * sglq's. The xritag that is passed in is used to index into the
 * array. Before the xritag can be used it needs to be adjusted
 * by subtracting the xribase.
 *
 * Returns sglq ponter = success, NULL = Failure.
 **/
static struct lpfc_sglq *
__lpfc_clear_active_sglq(struct lpfc_hba *phba, uint16_t xritag)
{
	uint16_t adj_xri;
	struct lpfc_sglq *sglq;
	adj_xri = xritag - phba->sli4_hba.max_cfg_param.xri_base;
	if (adj_xri > phba->sli4_hba.max_cfg_param.max_xri)
		return NULL;
	sglq = phba->sli4_hba.lpfc_sglq_active_list[adj_xri];
	phba->sli4_hba.lpfc_sglq_active_list[adj_xri] = NULL;
	return sglq;
}

/**
 * __lpfc_get_active_sglq - Get the active sglq for this XRI.
 * @phba: Pointer to HBA context object.
 * @xritag: XRI value.
 *
 * This function returns the sglq pointer from the array of acive
 * sglq's. The xritag that is passed in is used to index into the
 * array. Before the xritag can be used it needs to be adjusted
 * by subtracting the xribase.
 *
 * Returns sglq ponter = success, NULL = Failure.
 **/
static struct lpfc_sglq *
__lpfc_get_active_sglq(struct lpfc_hba *phba, uint16_t xritag)
{
	uint16_t adj_xri;
	struct lpfc_sglq *sglq;
	adj_xri = xritag - phba->sli4_hba.max_cfg_param.xri_base;
	if (adj_xri > phba->sli4_hba.max_cfg_param.max_xri)
		return NULL;
	sglq =  phba->sli4_hba.lpfc_sglq_active_list[adj_xri];
	return sglq;
}

/**
 * __lpfc_sli_get_sglq - Allocates an iocb object from sgl pool
 * @phba: Pointer to HBA context object.
 *
 * This function is called with hbalock held. This function
 * Gets a new driver sglq object from the sglq list. If the
 * list is not empty then it is successful, it returns pointer to the newly
 * allocated sglq object else it returns NULL.
 **/
static struct lpfc_sglq *
__lpfc_sli_get_sglq(struct lpfc_hba *phba)
{
	struct list_head *lpfc_sgl_list = &phba->sli4_hba.lpfc_sgl_list;
	struct lpfc_sglq *sglq = NULL;
	uint16_t adj_xri;
	list_remove_head(lpfc_sgl_list, sglq, struct lpfc_sglq, list);
	adj_xri = sglq->sli4_xritag - phba->sli4_hba.max_cfg_param.xri_base;
	phba->sli4_hba.lpfc_sglq_active_list[adj_xri] = sglq;
	return sglq;
}

/**
 * lpfc_sli_get_iocbq - Allocates an iocb object from iocb pool
 * @phba: Pointer to HBA context object.
 *
 * This function is called with no lock held. This function
 * allocates a new driver iocb object from the iocb pool. If the
 * allocation is successful, it returns pointer to the newly
 * allocated iocb object else it returns NULL.
 **/
struct lpfc_iocbq *
lpfc_sli_get_iocbq(struct lpfc_hba *phba)
{
	struct lpfc_iocbq * iocbq = NULL;
	unsigned long iflags;

	spin_lock_irqsave(&phba->hbalock, iflags);
	iocbq = __lpfc_sli_get_iocbq(phba);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return iocbq;
}

/**
 * __lpfc_sli_release_iocbq_s4 - Release iocb to the iocb pool
 * @phba: Pointer to HBA context object.
 * @iocbq: Pointer to driver iocb object.
 *
 * This function is called with hbalock held to release driver
 * iocb object to the iocb pool. The iotag in the iocb object
 * does not change for each use of the iocb object. This function
 * clears all other fields of the iocb object when it is freed.
 * The sqlq structure that holds the xritag and phys and virtual
 * mappings for the scatter gather list is retrieved from the
 * active array of sglq. The get of the sglq pointer also clears
 * the entry in the array. If the status of the IO indiactes that
 * this IO was aborted then the sglq entry it put on the
 * lpfc_abts_els_sgl_list until the CQ_ABORTED_XRI is received. If the
 * IO has good status or fails for any other reason then the sglq
 * entry is added to the free list (lpfc_sgl_list).
 **/
static void
__lpfc_sli_release_iocbq_s4(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	struct lpfc_sglq *sglq;
	size_t start_clean = offsetof(struct lpfc_iocbq, iocb);
	unsigned long iflag;

	if (iocbq->sli4_xritag == NO_XRI)
		sglq = NULL;
	else
		sglq = __lpfc_clear_active_sglq(phba, iocbq->sli4_xritag);
	if (sglq)  {
		if (iocbq->iocb_flag & LPFC_DRIVER_ABORTED
			|| ((iocbq->iocb.ulpStatus == IOSTAT_LOCAL_REJECT)
			&& (iocbq->iocb.un.ulpWord[4]
				== IOERR_SLI_ABORTED))) {
			spin_lock_irqsave(&phba->sli4_hba.abts_sgl_list_lock,
					iflag);
			list_add(&sglq->list,
				&phba->sli4_hba.lpfc_abts_els_sgl_list);
			spin_unlock_irqrestore(
				&phba->sli4_hba.abts_sgl_list_lock, iflag);
		} else
			list_add(&sglq->list, &phba->sli4_hba.lpfc_sgl_list);
	}


	/*
	 * Clean all volatile data fields, preserve iotag and node struct.
	 */
	memset((char *)iocbq + start_clean, 0, sizeof(*iocbq) - start_clean);
	iocbq->sli4_xritag = NO_XRI;
	list_add_tail(&iocbq->list, &phba->lpfc_iocb_list);
}

/**
 * __lpfc_sli_release_iocbq_s3 - Release iocb to the iocb pool
 * @phba: Pointer to HBA context object.
 * @iocbq: Pointer to driver iocb object.
 *
 * This function is called with hbalock held to release driver
 * iocb object to the iocb pool. The iotag in the iocb object
 * does not change for each use of the iocb object. This function
 * clears all other fields of the iocb object when it is freed.
 **/
static void
__lpfc_sli_release_iocbq_s3(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	size_t start_clean = offsetof(struct lpfc_iocbq, iocb);

	/*
	 * Clean all volatile data fields, preserve iotag and node struct.
	 */
	memset((char*)iocbq + start_clean, 0, sizeof(*iocbq) - start_clean);
	iocbq->sli4_xritag = NO_XRI;
	list_add_tail(&iocbq->list, &phba->lpfc_iocb_list);
}

/**
 * __lpfc_sli_release_iocbq - Release iocb to the iocb pool
 * @phba: Pointer to HBA context object.
 * @iocbq: Pointer to driver iocb object.
 *
 * This function is called with hbalock held to release driver
 * iocb object to the iocb pool. The iotag in the iocb object
 * does not change for each use of the iocb object. This function
 * clears all other fields of the iocb object when it is freed.
 **/
static void
__lpfc_sli_release_iocbq(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	phba->__lpfc_sli_release_iocbq(phba, iocbq);
}

/**
 * lpfc_sli_release_iocbq - Release iocb to the iocb pool
 * @phba: Pointer to HBA context object.
 * @iocbq: Pointer to driver iocb object.
 *
 * This function is called with no lock held to release the iocb to
 * iocb pool.
 **/
void
lpfc_sli_release_iocbq(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	unsigned long iflags;

	/*
	 * Clean all volatile data fields, preserve iotag and node struct.
	 */
	spin_lock_irqsave(&phba->hbalock, iflags);
	__lpfc_sli_release_iocbq(phba, iocbq);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
}

/**
 * lpfc_sli_cancel_iocbs - Cancel all iocbs from a list.
 * @phba: Pointer to HBA context object.
 * @iocblist: List of IOCBs.
 * @ulpstatus: ULP status in IOCB command field.
 * @ulpWord4: ULP word-4 in IOCB command field.
 *
 * This function is called with a list of IOCBs to cancel. It cancels the IOCB
 * on the list by invoking the complete callback function associated with the
 * IOCB with the provided @ulpstatus and @ulpword4 set to the IOCB commond
 * fields.
 **/
void
lpfc_sli_cancel_iocbs(struct lpfc_hba *phba, struct list_head *iocblist,
		      uint32_t ulpstatus, uint32_t ulpWord4)
{
	struct lpfc_iocbq *piocb;

	while (!list_empty(iocblist)) {
		list_remove_head(iocblist, piocb, struct lpfc_iocbq, list);

		if (!piocb->iocb_cmpl)
			lpfc_sli_release_iocbq(phba, piocb);
		else {
			piocb->iocb.ulpStatus = ulpstatus;
			piocb->iocb.un.ulpWord[4] = ulpWord4;
			(piocb->iocb_cmpl) (phba, piocb, piocb);
		}
	}
	return;
}

/**
 * lpfc_sli_iocb_cmd_type - Get the iocb type
 * @iocb_cmnd: iocb command code.
 *
 * This function is called by ring event handler function to get the iocb type.
 * This function translates the iocb command to an iocb command type used to
 * decide the final disposition of each completed IOCB.
 * The function returns
 * LPFC_UNKNOWN_IOCB if it is an unsupported iocb
 * LPFC_SOL_IOCB     if it is a solicited iocb completion
 * LPFC_ABORT_IOCB   if it is an abort iocb
 * LPFC_UNSOL_IOCB   if it is an unsolicited iocb
 *
 * The caller is not required to hold any lock.
 **/
static lpfc_iocb_type
lpfc_sli_iocb_cmd_type(uint8_t iocb_cmnd)
{
	lpfc_iocb_type type = LPFC_UNKNOWN_IOCB;

	if (iocb_cmnd > CMD_MAX_IOCB_CMD)
		return 0;

	switch (iocb_cmnd) {
	case CMD_XMIT_SEQUENCE_CR:
	case CMD_XMIT_SEQUENCE_CX:
	case CMD_XMIT_BCAST_CN:
	case CMD_XMIT_BCAST_CX:
	case CMD_ELS_REQUEST_CR:
	case CMD_ELS_REQUEST_CX:
	case CMD_CREATE_XRI_CR:
	case CMD_CREATE_XRI_CX:
	case CMD_GET_RPI_CN:
	case CMD_XMIT_ELS_RSP_CX:
	case CMD_GET_RPI_CR:
	case CMD_FCP_IWRITE_CR:
	case CMD_FCP_IWRITE_CX:
	case CMD_FCP_IREAD_CR:
	case CMD_FCP_IREAD_CX:
	case CMD_FCP_ICMND_CR:
	case CMD_FCP_ICMND_CX:
	case CMD_FCP_TSEND_CX:
	case CMD_FCP_TRSP_CX:
	case CMD_FCP_TRECEIVE_CX:
	case CMD_FCP_AUTO_TRSP_CX:
	case CMD_ADAPTER_MSG:
	case CMD_ADAPTER_DUMP:
	case CMD_XMIT_SEQUENCE64_CR:
	case CMD_XMIT_SEQUENCE64_CX:
	case CMD_XMIT_BCAST64_CN:
	case CMD_XMIT_BCAST64_CX:
	case CMD_ELS_REQUEST64_CR:
	case CMD_ELS_REQUEST64_CX:
	case CMD_FCP_IWRITE64_CR:
	case CMD_FCP_IWRITE64_CX:
	case CMD_FCP_IREAD64_CR:
	case CMD_FCP_IREAD64_CX:
	case CMD_FCP_ICMND64_CR:
	case CMD_FCP_ICMND64_CX:
	case CMD_FCP_TSEND64_CX:
	case CMD_FCP_TRSP64_CX:
	case CMD_FCP_TRECEIVE64_CX:
	case CMD_GEN_REQUEST64_CR:
	case CMD_GEN_REQUEST64_CX:
	case CMD_XMIT_ELS_RSP64_CX:
	case DSSCMD_IWRITE64_CR:
	case DSSCMD_IWRITE64_CX:
	case DSSCMD_IREAD64_CR:
	case DSSCMD_IREAD64_CX:
	case DSSCMD_INVALIDATE_DEK:
	case DSSCMD_SET_KEK:
	case DSSCMD_GET_KEK_ID:
	case DSSCMD_GEN_XFER:
		type = LPFC_SOL_IOCB;
		break;
	case CMD_ABORT_XRI_CN:
	case CMD_ABORT_XRI_CX:
	case CMD_CLOSE_XRI_CN:
	case CMD_CLOSE_XRI_CX:
	case CMD_XRI_ABORTED_CX:
	case CMD_ABORT_MXRI64_CN:
		type = LPFC_ABORT_IOCB;
		break;
	case CMD_RCV_SEQUENCE_CX:
	case CMD_RCV_ELS_REQ_CX:
	case CMD_RCV_SEQUENCE64_CX:
	case CMD_RCV_ELS_REQ64_CX:
	case CMD_ASYNC_STATUS:
	case CMD_IOCB_RCV_SEQ64_CX:
	case CMD_IOCB_RCV_ELS64_CX:
	case CMD_IOCB_RCV_CONT64_CX:
	case CMD_IOCB_RET_XRI64_CX:
		type = LPFC_UNSOL_IOCB;
		break;
	case CMD_IOCB_XMIT_MSEQ64_CR:
	case CMD_IOCB_XMIT_MSEQ64_CX:
	case CMD_IOCB_RCV_SEQ_LIST64_CX:
	case CMD_IOCB_RCV_ELS_LIST64_CX:
	case CMD_IOCB_CLOSE_EXTENDED_CN:
	case CMD_IOCB_ABORT_EXTENDED_CN:
	case CMD_IOCB_RET_HBQE64_CN:
	case CMD_IOCB_FCP_IBIDIR64_CR:
	case CMD_IOCB_FCP_IBIDIR64_CX:
	case CMD_IOCB_FCP_ITASKMGT64_CX:
	case CMD_IOCB_LOGENTRY_CN:
	case CMD_IOCB_LOGENTRY_ASYNC_CN:
		printk("%s - Unhandled SLI-3 Command x%x\n",
				__func__, iocb_cmnd);
		type = LPFC_UNKNOWN_IOCB;
		break;
	default:
		type = LPFC_UNKNOWN_IOCB;
		break;
	}

	return type;
}

/**
 * lpfc_sli_ring_map - Issue config_ring mbox for all rings
 * @phba: Pointer to HBA context object.
 *
 * This function is called from SLI initialization code
 * to configure every ring of the HBA's SLI interface. The
 * caller is not required to hold any lock. This function issues
 * a config_ring mailbox command for each ring.
 * This function returns zero if successful else returns a negative
 * error code.
 **/
static int
lpfc_sli_ring_map(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *pmbox;
	int i, rc, ret = 0;

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb)
		return -ENOMEM;
	pmbox = &pmb->u.mb;
	phba->link_state = LPFC_INIT_MBX_CMDS;
	for (i = 0; i < psli->num_rings; i++) {
		lpfc_config_ring(phba, i, pmb);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0446 Adapter failed to init (%d), "
					"mbxCmd x%x CFG_RING, mbxStatus x%x, "
					"ring %d\n",
					rc, pmbox->mbxCommand,
					pmbox->mbxStatus, i);
			phba->link_state = LPFC_HBA_ERROR;
			ret = -ENXIO;
			break;
		}
	}
	mempool_free(pmb, phba->mbox_mem_pool);
	return ret;
}

/**
 * lpfc_sli_ringtxcmpl_put - Adds new iocb to the txcmplq
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @piocb: Pointer to the driver iocb object.
 *
 * This function is called with hbalock held. The function adds the
 * new iocb to txcmplq of the given ring. This function always returns
 * 0. If this function is called for ELS ring, this function checks if
 * there is a vport associated with the ELS command. This function also
 * starts els_tmofunc timer if this is an ELS command.
 **/
static int
lpfc_sli_ringtxcmpl_put(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			struct lpfc_iocbq *piocb)
{
	list_add_tail(&piocb->list, &pring->txcmplq);
	pring->txcmplq_cnt++;
	if ((unlikely(pring->ringno == LPFC_ELS_RING)) &&
	   (piocb->iocb.ulpCommand != CMD_ABORT_XRI_CN) &&
	   (piocb->iocb.ulpCommand != CMD_CLOSE_XRI_CN)) {
		if (!piocb->vport)
			BUG();
		else
			mod_timer(&piocb->vport->els_tmofunc,
				  jiffies + HZ * (phba->fc_ratov << 1));
	}


	return 0;
}

/**
 * lpfc_sli_ringtx_get - Get first element of the txq
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function is called with hbalock held to get next
 * iocb in txq of the given ring. If there is any iocb in
 * the txq, the function returns first iocb in the list after
 * removing the iocb from the list, else it returns NULL.
 **/
static struct lpfc_iocbq *
lpfc_sli_ringtx_get(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	struct lpfc_iocbq *cmd_iocb;

	list_remove_head((&pring->txq), cmd_iocb, struct lpfc_iocbq, list);
	if (cmd_iocb != NULL)
		pring->txq_cnt--;
	return cmd_iocb;
}

/**
 * lpfc_sli_next_iocb_slot - Get next iocb slot in the ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function is called with hbalock held and the caller must post the
 * iocb without releasing the lock. If the caller releases the lock,
 * iocb slot returned by the function is not guaranteed to be available.
 * The function returns pointer to the next available iocb slot if there
 * is available slot in the ring, else it returns NULL.
 * If the get index of the ring is ahead of the put index, the function
 * will post an error attention event to the worker thread to take the
 * HBA to offline state.
 **/
static IOCB_t *
lpfc_sli_next_iocb_slot (struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	uint32_t  max_cmd_idx = pring->numCiocb;
	if ((pring->next_cmdidx == pring->cmdidx) &&
	   (++pring->next_cmdidx >= max_cmd_idx))
		pring->next_cmdidx = 0;

	if (unlikely(pring->local_getidx == pring->next_cmdidx)) {

		pring->local_getidx = le32_to_cpu(pgp->cmdGetInx);

		if (unlikely(pring->local_getidx >= max_cmd_idx)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"0315 Ring %d issue: portCmdGet %d "
					"is bigger than cmd ring %d\n",
					pring->ringno,
					pring->local_getidx, max_cmd_idx);

			phba->link_state = LPFC_HBA_ERROR;
			/*
			 * All error attention handlers are posted to
			 * worker thread
			 */
			phba->work_ha |= HA_ERATT;
			phba->work_hs = HS_FFER3;

			lpfc_worker_wake_up(phba);

			return NULL;
		}

		if (pring->local_getidx == pring->next_cmdidx)
			return NULL;
	}

	return lpfc_cmd_iocb(phba, pring);
}

/**
 * lpfc_sli_next_iotag - Get an iotag for the iocb
 * @phba: Pointer to HBA context object.
 * @iocbq: Pointer to driver iocb object.
 *
 * This function gets an iotag for the iocb. If there is no unused iotag and
 * the iocbq_lookup_len < 0xffff, this function allocates a bigger iotag_lookup
 * array and assigns a new iotag.
 * The function returns the allocated iotag if successful, else returns zero.
 * Zero is not a valid iotag.
 * The caller is not required to hold any lock.
 **/
uint16_t
lpfc_sli_next_iotag(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	struct lpfc_iocbq **new_arr;
	struct lpfc_iocbq **old_arr;
	size_t new_len;
	struct lpfc_sli *psli = &phba->sli;
	uint16_t iotag;

	spin_lock_irq(&phba->hbalock);
	iotag = psli->last_iotag;
	if(++iotag < psli->iocbq_lookup_len) {
		psli->last_iotag = iotag;
		psli->iocbq_lookup[iotag] = iocbq;
		spin_unlock_irq(&phba->hbalock);
		iocbq->iotag = iotag;
		return iotag;
	} else if (psli->iocbq_lookup_len < (0xffff
					   - LPFC_IOCBQ_LOOKUP_INCREMENT)) {
		new_len = psli->iocbq_lookup_len + LPFC_IOCBQ_LOOKUP_INCREMENT;
		spin_unlock_irq(&phba->hbalock);
		new_arr = kzalloc(new_len * sizeof (struct lpfc_iocbq *),
				  GFP_KERNEL);
		if (new_arr) {
			spin_lock_irq(&phba->hbalock);
			old_arr = psli->iocbq_lookup;
			if (new_len <= psli->iocbq_lookup_len) {
				/* highly unprobable case */
				kfree(new_arr);
				iotag = psli->last_iotag;
				if(++iotag < psli->iocbq_lookup_len) {
					psli->last_iotag = iotag;
					psli->iocbq_lookup[iotag] = iocbq;
					spin_unlock_irq(&phba->hbalock);
					iocbq->iotag = iotag;
					return iotag;
				}
				spin_unlock_irq(&phba->hbalock);
				return 0;
			}
			if (psli->iocbq_lookup)
				memcpy(new_arr, old_arr,
				       ((psli->last_iotag  + 1) *
					sizeof (struct lpfc_iocbq *)));
			psli->iocbq_lookup = new_arr;
			psli->iocbq_lookup_len = new_len;
			psli->last_iotag = iotag;
			psli->iocbq_lookup[iotag] = iocbq;
			spin_unlock_irq(&phba->hbalock);
			iocbq->iotag = iotag;
			kfree(old_arr);
			return iotag;
		}
	} else
		spin_unlock_irq(&phba->hbalock);

	lpfc_printf_log(phba, KERN_ERR,LOG_SLI,
			"0318 Failed to allocate IOTAG.last IOTAG is %d\n",
			psli->last_iotag);

	return 0;
}

/**
 * lpfc_sli_submit_iocb - Submit an iocb to the firmware
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @iocb: Pointer to iocb slot in the ring.
 * @nextiocb: Pointer to driver iocb object which need to be
 *            posted to firmware.
 *
 * This function is called with hbalock held to post a new iocb to
 * the firmware. This function copies the new iocb to ring iocb slot and
 * updates the ring pointers. It adds the new iocb to txcmplq if there is
 * a completion call back for this iocb else the function will free the
 * iocb object.
 **/
static void
lpfc_sli_submit_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		IOCB_t *iocb, struct lpfc_iocbq *nextiocb)
{
	/*
	 * Set up an iotag
	 */
	nextiocb->iocb.ulpIoTag = (nextiocb->iocb_cmpl) ? nextiocb->iotag : 0;


	if (pring->ringno == LPFC_ELS_RING) {
		lpfc_debugfs_slow_ring_trc(phba,
			"IOCB cmd ring:   wd4:x%08x wd6:x%08x wd7:x%08x",
			*(((uint32_t *) &nextiocb->iocb) + 4),
			*(((uint32_t *) &nextiocb->iocb) + 6),
			*(((uint32_t *) &nextiocb->iocb) + 7));
	}

	/*
	 * Issue iocb command to adapter
	 */
	lpfc_sli_pcimem_bcopy(&nextiocb->iocb, iocb, phba->iocb_cmd_size);
	wmb();
	pring->stats.iocb_cmd++;

	/*
	 * If there is no completion routine to call, we can release the
	 * IOCB buffer back right now. For IOCBs, like QUE_RING_BUF,
	 * that have no rsp ring completion, iocb_cmpl MUST be NULL.
	 */
	if (nextiocb->iocb_cmpl)
		lpfc_sli_ringtxcmpl_put(phba, pring, nextiocb);
	else
		__lpfc_sli_release_iocbq(phba, nextiocb);

	/*
	 * Let the HBA know what IOCB slot will be the next one the
	 * driver will put a command into.
	 */
	pring->cmdidx = pring->next_cmdidx;
	writel(pring->cmdidx, &phba->host_gp[pring->ringno].cmdPutInx);
}

/**
 * lpfc_sli_update_full_ring - Update the chip attention register
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * The caller is not required to hold any lock for calling this function.
 * This function updates the chip attention bits for the ring to inform firmware
 * that there are pending work to be done for this ring and requests an
 * interrupt when there is space available in the ring. This function is
 * called when the driver is unable to post more iocbs to the ring due
 * to unavailability of space in the ring.
 **/
static void
lpfc_sli_update_full_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	int ringno = pring->ringno;

	pring->flag |= LPFC_CALL_RING_AVAILABLE;

	wmb();

	/*
	 * Set ring 'ringno' to SET R0CE_REQ in Chip Att register.
	 * The HBA will tell us when an IOCB entry is available.
	 */
	writel((CA_R0ATT|CA_R0CE_REQ) << (ringno*4), phba->CAregaddr);
	readl(phba->CAregaddr); /* flush */

	pring->stats.iocb_cmd_full++;
}

/**
 * lpfc_sli_update_ring - Update chip attention register
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function updates the chip attention register bit for the
 * given ring to inform HBA that there is more work to be done
 * in this ring. The caller is not required to hold any lock.
 **/
static void
lpfc_sli_update_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	int ringno = pring->ringno;

	/*
	 * Tell the HBA that there is work to do in this ring.
	 */
	if (!(phba->sli3_options & LPFC_SLI3_CRP_ENABLED)) {
		wmb();
		writel(CA_R0ATT << (ringno * 4), phba->CAregaddr);
		readl(phba->CAregaddr); /* flush */
	}
}

/**
 * lpfc_sli_resume_iocb - Process iocbs in the txq
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function is called with hbalock held to post pending iocbs
 * in the txq to the firmware. This function is called when driver
 * detects space available in the ring.
 **/
static void
lpfc_sli_resume_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	IOCB_t *iocb;
	struct lpfc_iocbq *nextiocb;

	/*
	 * Check to see if:
	 *  (a) there is anything on the txq to send
	 *  (b) link is up
	 *  (c) link attention events can be processed (fcp ring only)
	 *  (d) IOCB processing is not blocked by the outstanding mbox command.
	 */
	if (pring->txq_cnt &&
	    lpfc_is_link_up(phba) &&
	    (pring->ringno != phba->sli.fcp_ring ||
	     phba->sli.sli_flag & LPFC_PROCESS_LA)) {

		while ((iocb = lpfc_sli_next_iocb_slot(phba, pring)) &&
		       (nextiocb = lpfc_sli_ringtx_get(phba, pring)))
			lpfc_sli_submit_iocb(phba, pring, iocb, nextiocb);

		if (iocb)
			lpfc_sli_update_ring(phba, pring);
		else
			lpfc_sli_update_full_ring(phba, pring);
	}

	return;
}

/**
 * lpfc_sli_next_hbq_slot - Get next hbq entry for the HBQ
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 *
 * This function is called with hbalock held to get the next
 * available slot for the given HBQ. If there is free slot
 * available for the HBQ it will return pointer to the next available
 * HBQ entry else it will return NULL.
 **/
static struct lpfc_hbq_entry *
lpfc_sli_next_hbq_slot(struct lpfc_hba *phba, uint32_t hbqno)
{
	struct hbq_s *hbqp = &phba->hbqs[hbqno];

	if (hbqp->next_hbqPutIdx == hbqp->hbqPutIdx &&
	    ++hbqp->next_hbqPutIdx >= hbqp->entry_count)
		hbqp->next_hbqPutIdx = 0;

	if (unlikely(hbqp->local_hbqGetIdx == hbqp->next_hbqPutIdx)) {
		uint32_t raw_index = phba->hbq_get[hbqno];
		uint32_t getidx = le32_to_cpu(raw_index);

		hbqp->local_hbqGetIdx = getidx;

		if (unlikely(hbqp->local_hbqGetIdx >= hbqp->entry_count)) {
			lpfc_printf_log(phba, KERN_ERR,
					LOG_SLI | LOG_VPORT,
					"1802 HBQ %d: local_hbqGetIdx "
					"%u is > than hbqp->entry_count %u\n",
					hbqno, hbqp->local_hbqGetIdx,
					hbqp->entry_count);

			phba->link_state = LPFC_HBA_ERROR;
			return NULL;
		}

		if (hbqp->local_hbqGetIdx == hbqp->next_hbqPutIdx)
			return NULL;
	}

	return (struct lpfc_hbq_entry *) phba->hbqs[hbqno].hbq_virt +
			hbqp->hbqPutIdx;
}

/**
 * lpfc_sli_hbqbuf_free_all - Free all the hbq buffers
 * @phba: Pointer to HBA context object.
 *
 * This function is called with no lock held to free all the
 * hbq buffers while uninitializing the SLI interface. It also
 * frees the HBQ buffers returned by the firmware but not yet
 * processed by the upper layers.
 **/
void
lpfc_sli_hbqbuf_free_all(struct lpfc_hba *phba)
{
	struct lpfc_dmabuf *dmabuf, *next_dmabuf;
	struct hbq_dmabuf *hbq_buf;
	unsigned long flags;
	int i, hbq_count;
	uint32_t hbqno;

	hbq_count = lpfc_sli_hbq_count();
	/* Return all memory used by all HBQs */
	spin_lock_irqsave(&phba->hbalock, flags);
	for (i = 0; i < hbq_count; ++i) {
		list_for_each_entry_safe(dmabuf, next_dmabuf,
				&phba->hbqs[i].hbq_buffer_list, list) {
			hbq_buf = container_of(dmabuf, struct hbq_dmabuf, dbuf);
			list_del(&hbq_buf->dbuf.list);
			(phba->hbqs[i].hbq_free_buffer)(phba, hbq_buf);
		}
		phba->hbqs[i].buffer_count = 0;
	}
	/* Return all HBQ buffer that are in-fly */
	list_for_each_entry_safe(dmabuf, next_dmabuf, &phba->rb_pend_list,
				 list) {
		hbq_buf = container_of(dmabuf, struct hbq_dmabuf, dbuf);
		list_del(&hbq_buf->dbuf.list);
		if (hbq_buf->tag == -1) {
			(phba->hbqs[LPFC_ELS_HBQ].hbq_free_buffer)
				(phba, hbq_buf);
		} else {
			hbqno = hbq_buf->tag >> 16;
			if (hbqno >= LPFC_MAX_HBQS)
				(phba->hbqs[LPFC_ELS_HBQ].hbq_free_buffer)
					(phba, hbq_buf);
			else
				(phba->hbqs[hbqno].hbq_free_buffer)(phba,
					hbq_buf);
		}
	}

	/* Mark the HBQs not in use */
	phba->hbq_in_use = 0;
	spin_unlock_irqrestore(&phba->hbalock, flags);
}

/**
 * lpfc_sli_hbq_to_firmware - Post the hbq buffer to firmware
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 * @hbq_buf: Pointer to HBQ buffer.
 *
 * This function is called with the hbalock held to post a
 * hbq buffer to the firmware. If the function finds an empty
 * slot in the HBQ, it will post the buffer. The function will return
 * pointer to the hbq entry if it successfully post the buffer
 * else it will return NULL.
 **/
static int
lpfc_sli_hbq_to_firmware(struct lpfc_hba *phba, uint32_t hbqno,
			 struct hbq_dmabuf *hbq_buf)
{
	return phba->lpfc_sli_hbq_to_firmware(phba, hbqno, hbq_buf);
}

/**
 * lpfc_sli_hbq_to_firmware_s3 - Post the hbq buffer to SLI3 firmware
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 * @hbq_buf: Pointer to HBQ buffer.
 *
 * This function is called with the hbalock held to post a hbq buffer to the
 * firmware. If the function finds an empty slot in the HBQ, it will post the
 * buffer and place it on the hbq_buffer_list. The function will return zero if
 * it successfully post the buffer else it will return an error.
 **/
static int
lpfc_sli_hbq_to_firmware_s3(struct lpfc_hba *phba, uint32_t hbqno,
			    struct hbq_dmabuf *hbq_buf)
{
	struct lpfc_hbq_entry *hbqe;
	dma_addr_t physaddr = hbq_buf->dbuf.phys;

	/* Get next HBQ entry slot to use */
	hbqe = lpfc_sli_next_hbq_slot(phba, hbqno);
	if (hbqe) {
		struct hbq_s *hbqp = &phba->hbqs[hbqno];

		hbqe->bde.addrHigh = le32_to_cpu(putPaddrHigh(physaddr));
		hbqe->bde.addrLow  = le32_to_cpu(putPaddrLow(physaddr));
		hbqe->bde.tus.f.bdeSize = hbq_buf->size;
		hbqe->bde.tus.f.bdeFlags = 0;
		hbqe->bde.tus.w = le32_to_cpu(hbqe->bde.tus.w);
		hbqe->buffer_tag = le32_to_cpu(hbq_buf->tag);
				/* Sync SLIM */
		hbqp->hbqPutIdx = hbqp->next_hbqPutIdx;
		writel(hbqp->hbqPutIdx, phba->hbq_put + hbqno);
				/* flush */
		readl(phba->hbq_put + hbqno);
		list_add_tail(&hbq_buf->dbuf.list, &hbqp->hbq_buffer_list);
		return 0;
	} else
		return -ENOMEM;
}

/**
 * lpfc_sli_hbq_to_firmware_s4 - Post the hbq buffer to SLI4 firmware
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 * @hbq_buf: Pointer to HBQ buffer.
 *
 * This function is called with the hbalock held to post an RQE to the SLI4
 * firmware. If able to post the RQE to the RQ it will queue the hbq entry to
 * the hbq_buffer_list and return zero, otherwise it will return an error.
 **/
static int
lpfc_sli_hbq_to_firmware_s4(struct lpfc_hba *phba, uint32_t hbqno,
			    struct hbq_dmabuf *hbq_buf)
{
	int rc;
	struct lpfc_rqe hrqe;
	struct lpfc_rqe drqe;

	hrqe.address_lo = putPaddrLow(hbq_buf->hbuf.phys);
	hrqe.address_hi = putPaddrHigh(hbq_buf->hbuf.phys);
	drqe.address_lo = putPaddrLow(hbq_buf->dbuf.phys);
	drqe.address_hi = putPaddrHigh(hbq_buf->dbuf.phys);
	rc = lpfc_sli4_rq_put(phba->sli4_hba.hdr_rq, phba->sli4_hba.dat_rq,
			      &hrqe, &drqe);
	if (rc < 0)
		return rc;
	hbq_buf->tag = rc;
	list_add_tail(&hbq_buf->dbuf.list, &phba->hbqs[hbqno].hbq_buffer_list);
	return 0;
}

/* HBQ for ELS and CT traffic. */
static struct lpfc_hbq_init lpfc_els_hbq = {
	.rn = 1,
	.entry_count = 200,
	.mask_count = 0,
	.profile = 0,
	.ring_mask = (1 << LPFC_ELS_RING),
	.buffer_count = 0,
	.init_count = 40,
	.add_count = 40,
};

/* HBQ for the extra ring if needed */
static struct lpfc_hbq_init lpfc_extra_hbq = {
	.rn = 1,
	.entry_count = 200,
	.mask_count = 0,
	.profile = 0,
	.ring_mask = (1 << LPFC_EXTRA_RING),
	.buffer_count = 0,
	.init_count = 0,
	.add_count = 5,
};

/* Array of HBQs */
struct lpfc_hbq_init *lpfc_hbq_defs[] = {
	&lpfc_els_hbq,
	&lpfc_extra_hbq,
};

/**
 * lpfc_sli_hbqbuf_fill_hbqs - Post more hbq buffers to HBQ
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 * @count: Number of HBQ buffers to be posted.
 *
 * This function is called with no lock held to post more hbq buffers to the
 * given HBQ. The function returns the number of HBQ buffers successfully
 * posted.
 **/
static int
lpfc_sli_hbqbuf_fill_hbqs(struct lpfc_hba *phba, uint32_t hbqno, uint32_t count)
{
	uint32_t i, posted = 0;
	unsigned long flags;
	struct hbq_dmabuf *hbq_buffer;
	LIST_HEAD(hbq_buf_list);
	if (!phba->hbqs[hbqno].hbq_alloc_buffer)
		return 0;

	if ((phba->hbqs[hbqno].buffer_count + count) >
	    lpfc_hbq_defs[hbqno]->entry_count)
		count = lpfc_hbq_defs[hbqno]->entry_count -
					phba->hbqs[hbqno].buffer_count;
	if (!count)
		return 0;
	/* Allocate HBQ entries */
	for (i = 0; i < count; i++) {
		hbq_buffer = (phba->hbqs[hbqno].hbq_alloc_buffer)(phba);
		if (!hbq_buffer)
			break;
		list_add_tail(&hbq_buffer->dbuf.list, &hbq_buf_list);
	}
	/* Check whether HBQ is still in use */
	spin_lock_irqsave(&phba->hbalock, flags);
	if (!phba->hbq_in_use)
		goto err;
	while (!list_empty(&hbq_buf_list)) {
		list_remove_head(&hbq_buf_list, hbq_buffer, struct hbq_dmabuf,
				 dbuf.list);
		hbq_buffer->tag = (phba->hbqs[hbqno].buffer_count |
				      (hbqno << 16));
		if (!lpfc_sli_hbq_to_firmware(phba, hbqno, hbq_buffer)) {
			phba->hbqs[hbqno].buffer_count++;
			posted++;
		} else
			(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buffer);
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	return posted;
err:
	spin_unlock_irqrestore(&phba->hbalock, flags);
	while (!list_empty(&hbq_buf_list)) {
		list_remove_head(&hbq_buf_list, hbq_buffer, struct hbq_dmabuf,
				 dbuf.list);
		(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buffer);
	}
	return 0;
}

/**
 * lpfc_sli_hbqbuf_add_hbqs - Post more HBQ buffers to firmware
 * @phba: Pointer to HBA context object.
 * @qno: HBQ number.
 *
 * This function posts more buffers to the HBQ. This function
 * is called with no lock held. The function returns the number of HBQ entries
 * successfully allocated.
 **/
int
lpfc_sli_hbqbuf_add_hbqs(struct lpfc_hba *phba, uint32_t qno)
{
	return(lpfc_sli_hbqbuf_fill_hbqs(phba, qno,
					 lpfc_hbq_defs[qno]->add_count));
}

/**
 * lpfc_sli_hbqbuf_init_hbqs - Post initial buffers to the HBQ
 * @phba: Pointer to HBA context object.
 * @qno:  HBQ queue number.
 *
 * This function is called from SLI initialization code path with
 * no lock held to post initial HBQ buffers to firmware. The
 * function returns the number of HBQ entries successfully allocated.
 **/
static int
lpfc_sli_hbqbuf_init_hbqs(struct lpfc_hba *phba, uint32_t qno)
{
	return(lpfc_sli_hbqbuf_fill_hbqs(phba, qno,
					 lpfc_hbq_defs[qno]->init_count));
}

/**
 * lpfc_sli_hbqbuf_get - Remove the first hbq off of an hbq list
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 *
 * This function removes the first hbq buffer on an hbq list and returns a
 * pointer to that buffer. If it finds no buffers on the list it returns NULL.
 **/
static struct hbq_dmabuf *
lpfc_sli_hbqbuf_get(struct list_head *rb_list)
{
	struct lpfc_dmabuf *d_buf;

	list_remove_head(rb_list, d_buf, struct lpfc_dmabuf, list);
	if (!d_buf)
		return NULL;
	return container_of(d_buf, struct hbq_dmabuf, dbuf);
}

/**
 * lpfc_sli_hbqbuf_find - Find the hbq buffer associated with a tag
 * @phba: Pointer to HBA context object.
 * @tag: Tag of the hbq buffer.
 *
 * This function is called with hbalock held. This function searches
 * for the hbq buffer associated with the given tag in the hbq buffer
 * list. If it finds the hbq buffer, it returns the hbq_buffer other wise
 * it returns NULL.
 **/
static struct hbq_dmabuf *
lpfc_sli_hbqbuf_find(struct lpfc_hba *phba, uint32_t tag)
{
	struct lpfc_dmabuf *d_buf;
	struct hbq_dmabuf *hbq_buf;
	uint32_t hbqno;

	hbqno = tag >> 16;
	if (hbqno >= LPFC_MAX_HBQS)
		return NULL;

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(d_buf, &phba->hbqs[hbqno].hbq_buffer_list, list) {
		hbq_buf = container_of(d_buf, struct hbq_dmabuf, dbuf);
		if (hbq_buf->tag == tag) {
			spin_unlock_irq(&phba->hbalock);
			return hbq_buf;
		}
	}
	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_log(phba, KERN_ERR, LOG_SLI | LOG_VPORT,
			"1803 Bad hbq tag. Data: x%x x%x\n",
			tag, phba->hbqs[tag >> 16].buffer_count);
	return NULL;
}

/**
 * lpfc_sli_free_hbq - Give back the hbq buffer to firmware
 * @phba: Pointer to HBA context object.
 * @hbq_buffer: Pointer to HBQ buffer.
 *
 * This function is called with hbalock. This function gives back
 * the hbq buffer to firmware. If the HBQ does not have space to
 * post the buffer, it will free the buffer.
 **/
void
lpfc_sli_free_hbq(struct lpfc_hba *phba, struct hbq_dmabuf *hbq_buffer)
{
	uint32_t hbqno;

	if (hbq_buffer) {
		hbqno = hbq_buffer->tag >> 16;
		if (lpfc_sli_hbq_to_firmware(phba, hbqno, hbq_buffer))
			(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buffer);
	}
}

/**
 * lpfc_sli_chk_mbx_command - Check if the mailbox is a legitimate mailbox
 * @mbxCommand: mailbox command code.
 *
 * This function is called by the mailbox event handler function to verify
 * that the completed mailbox command is a legitimate mailbox command. If the
 * completed mailbox is not known to the function, it will return MBX_SHUTDOWN
 * and the mailbox event handler will take the HBA offline.
 **/
static int
lpfc_sli_chk_mbx_command(uint8_t mbxCommand)
{
	uint8_t ret;

	switch (mbxCommand) {
	case MBX_LOAD_SM:
	case MBX_READ_NV:
	case MBX_WRITE_NV:
	case MBX_WRITE_VPARMS:
	case MBX_RUN_BIU_DIAG:
	case MBX_INIT_LINK:
	case MBX_DOWN_LINK:
	case MBX_CONFIG_LINK:
	case MBX_CONFIG_RING:
	case MBX_RESET_RING:
	case MBX_READ_CONFIG:
	case MBX_READ_RCONFIG:
	case MBX_READ_SPARM:
	case MBX_READ_STATUS:
	case MBX_READ_RPI:
	case MBX_READ_XRI:
	case MBX_READ_REV:
	case MBX_READ_LNK_STAT:
	case MBX_REG_LOGIN:
	case MBX_UNREG_LOGIN:
	case MBX_READ_LA:
	case MBX_CLEAR_LA:
	case MBX_DUMP_MEMORY:
	case MBX_DUMP_CONTEXT:
	case MBX_RUN_DIAGS:
	case MBX_RESTART:
	case MBX_UPDATE_CFG:
	case MBX_DOWN_LOAD:
	case MBX_DEL_LD_ENTRY:
	case MBX_RUN_PROGRAM:
	case MBX_SET_MASK:
	case MBX_SET_VARIABLE:
	case MBX_UNREG_D_ID:
	case MBX_KILL_BOARD:
	case MBX_CONFIG_FARP:
	case MBX_BEACON:
	case MBX_LOAD_AREA:
	case MBX_RUN_BIU_DIAG64:
	case MBX_CONFIG_PORT:
	case MBX_READ_SPARM64:
	case MBX_READ_RPI64:
	case MBX_REG_LOGIN64:
	case MBX_READ_LA64:
	case MBX_WRITE_WWN:
	case MBX_SET_DEBUG:
	case MBX_LOAD_EXP_ROM:
	case MBX_ASYNCEVT_ENABLE:
	case MBX_REG_VPI:
	case MBX_UNREG_VPI:
	case MBX_HEARTBEAT:
	case MBX_PORT_CAPABILITIES:
	case MBX_PORT_IOV_CONTROL:
	case MBX_SLI4_CONFIG:
	case MBX_SLI4_REQ_FTRS:
	case MBX_REG_FCFI:
	case MBX_UNREG_FCFI:
	case MBX_REG_VFI:
	case MBX_UNREG_VFI:
	case MBX_INIT_VPI:
	case MBX_INIT_VFI:
	case MBX_RESUME_RPI:
		ret = mbxCommand;
		break;
	default:
		ret = MBX_SHUTDOWN;
		break;
	}
	return ret;
}

/**
 * lpfc_sli_wake_mbox_wait - lpfc_sli_issue_mbox_wait mbox completion handler
 * @phba: Pointer to HBA context object.
 * @pmboxq: Pointer to mailbox command.
 *
 * This is completion handler function for mailbox commands issued from
 * lpfc_sli_issue_mbox_wait function. This function is called by the
 * mailbox event handler function with no lock held. This function
 * will wake up thread waiting on the wait queue pointed by context1
 * of the mailbox.
 **/
void
lpfc_sli_wake_mbox_wait(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmboxq)
{
	wait_queue_head_t *pdone_q;
	unsigned long drvr_flag;

	/*
	 * If pdone_q is empty, the driver thread gave up waiting and
	 * continued running.
	 */
	pmboxq->mbox_flag |= LPFC_MBX_WAKE;
	spin_lock_irqsave(&phba->hbalock, drvr_flag);
	pdone_q = (wait_queue_head_t *) pmboxq->context1;
	if (pdone_q)
		wake_up_interruptible(pdone_q);
	spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
	return;
}


/**
 * lpfc_sli_def_mbox_cmpl - Default mailbox completion handler
 * @phba: Pointer to HBA context object.
 * @pmb: Pointer to mailbox object.
 *
 * This function is the default mailbox completion handler. It
 * frees the memory resources associated with the completed mailbox
 * command. If the completed command is a REG_LOGIN mailbox command,
 * this function will issue a UREG_LOGIN to re-claim the RPI.
 **/
void
lpfc_sli_def_mbox_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_dmabuf *mp;
	uint16_t rpi, vpi;
	int rc;

	mp = (struct lpfc_dmabuf *) (pmb->context1);

	if (mp) {
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
	}

	if ((pmb->u.mb.mbxCommand == MBX_UNREG_LOGIN) &&
	    (phba->sli_rev == LPFC_SLI_REV4))
		lpfc_sli4_free_rpi(phba, pmb->u.mb.un.varUnregLogin.rpi);

	/*
	 * If a REG_LOGIN succeeded  after node is destroyed or node
	 * is in re-discovery driver need to cleanup the RPI.
	 */
	if (!(phba->pport->load_flag & FC_UNLOADING) &&
	    pmb->u.mb.mbxCommand == MBX_REG_LOGIN64 &&
	    !pmb->u.mb.mbxStatus) {
		rpi = pmb->u.mb.un.varWords[0];
		vpi = pmb->u.mb.un.varRegLogin.vpi - phba->vpi_base;
		lpfc_unreg_login(phba, vpi, rpi, pmb);
		pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
		if (rc != MBX_NOT_FINISHED)
			return;
	}

	if (bf_get(lpfc_mqe_command, &pmb->u.mqe) == MBX_SLI4_CONFIG)
		lpfc_sli4_mbox_cmd_free(phba, pmb);
	else
		mempool_free(pmb, phba->mbox_mem_pool);
}

/**
 * lpfc_sli_handle_mb_event - Handle mailbox completions from firmware
 * @phba: Pointer to HBA context object.
 *
 * This function is called with no lock held. This function processes all
 * the completed mailbox commands and gives it to upper layers. The interrupt
 * service routine processes mailbox completion interrupt and adds completed
 * mailbox commands to the mboxq_cmpl queue and signals the worker thread.
 * Worker thread call lpfc_sli_handle_mb_event, which will return the
 * completed mailbox commands in mboxq_cmpl queue to the upper layers. This
 * function returns the mailbox commands to the upper layer by calling the
 * completion handler function of each mailbox.
 **/
int
lpfc_sli_handle_mb_event(struct lpfc_hba *phba)
{
	MAILBOX_t *pmbox;
	LPFC_MBOXQ_t *pmb;
	int rc;
	LIST_HEAD(cmplq);

	phba->sli.slistat.mbox_event++;

	/* Get all completed mailboxe buffers into the cmplq */
	spin_lock_irq(&phba->hbalock);
	list_splice_init(&phba->sli.mboxq_cmpl, &cmplq);
	spin_unlock_irq(&phba->hbalock);

	/* Get a Mailbox buffer to setup mailbox commands for callback */
	do {
		list_remove_head(&cmplq, pmb, LPFC_MBOXQ_t, list);
		if (pmb == NULL)
			break;

		pmbox = &pmb->u.mb;

		if (pmbox->mbxCommand != MBX_HEARTBEAT) {
			if (pmb->vport) {
				lpfc_debugfs_disc_trc(pmb->vport,
					LPFC_DISC_TRC_MBOX_VPORT,
					"MBOX cmpl vport: cmd:x%x mb:x%x x%x",
					(uint32_t)pmbox->mbxCommand,
					pmbox->un.varWords[0],
					pmbox->un.varWords[1]);
			}
			else {
				lpfc_debugfs_disc_trc(phba->pport,
					LPFC_DISC_TRC_MBOX,
					"MBOX cmpl:       cmd:x%x mb:x%x x%x",
					(uint32_t)pmbox->mbxCommand,
					pmbox->un.varWords[0],
					pmbox->un.varWords[1]);
			}
		}

		/*
		 * It is a fatal error if unknown mbox command completion.
		 */
		if (lpfc_sli_chk_mbx_command(pmbox->mbxCommand) ==
		    MBX_SHUTDOWN) {
			/* Unknow mailbox command compl */
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
					"(%d):0323 Unknown Mailbox command "
					"x%x (x%x) Cmpl\n",
					pmb->vport ? pmb->vport->vpi : 0,
					pmbox->mbxCommand,
					lpfc_sli4_mbox_opcode_get(phba, pmb));
			phba->link_state = LPFC_HBA_ERROR;
			phba->work_hs = HS_FFER3;
			lpfc_handle_eratt(phba);
			continue;
		}

		if (pmbox->mbxStatus) {
			phba->sli.slistat.mbox_stat_err++;
			if (pmbox->mbxStatus == MBXERR_NO_RESOURCES) {
				/* Mbox cmd cmpl error - RETRYing */
				lpfc_printf_log(phba, KERN_INFO,
						LOG_MBOX | LOG_SLI,
						"(%d):0305 Mbox cmd cmpl "
						"error - RETRYing Data: x%x "
						"(x%x) x%x x%x x%x\n",
						pmb->vport ? pmb->vport->vpi :0,
						pmbox->mbxCommand,
						lpfc_sli4_mbox_opcode_get(phba,
									  pmb),
						pmbox->mbxStatus,
						pmbox->un.varWords[0],
						pmb->vport->port_state);
				pmbox->mbxStatus = 0;
				pmbox->mbxOwner = OWN_HOST;
				rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
				if (rc != MBX_NOT_FINISHED)
					continue;
			}
		}

		/* Mailbox cmd <cmd> Cmpl <cmpl> */
		lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
				"(%d):0307 Mailbox cmd x%x (x%x) Cmpl x%p "
				"Data: x%x x%x x%x x%x x%x x%x x%x x%x x%x\n",
				pmb->vport ? pmb->vport->vpi : 0,
				pmbox->mbxCommand,
				lpfc_sli4_mbox_opcode_get(phba, pmb),
				pmb->mbox_cmpl,
				*((uint32_t *) pmbox),
				pmbox->un.varWords[0],
				pmbox->un.varWords[1],
				pmbox->un.varWords[2],
				pmbox->un.varWords[3],
				pmbox->un.varWords[4],
				pmbox->un.varWords[5],
				pmbox->un.varWords[6],
				pmbox->un.varWords[7]);

		if (pmb->mbox_cmpl)
			pmb->mbox_cmpl(phba,pmb);
	} while (1);
	return 0;
}

/**
 * lpfc_sli_get_buff - Get the buffer associated with the buffer tag
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @tag: buffer tag.
 *
 * This function is called with no lock held. When QUE_BUFTAG_BIT bit
 * is set in the tag the buffer is posted for a particular exchange,
 * the function will return the buffer without replacing the buffer.
 * If the buffer is for unsolicited ELS or CT traffic, this function
 * returns the buffer and also posts another buffer to the firmware.
 **/
static struct lpfc_dmabuf *
lpfc_sli_get_buff(struct lpfc_hba *phba,
		  struct lpfc_sli_ring *pring,
		  uint32_t tag)
{
	struct hbq_dmabuf *hbq_entry;

	if (tag & QUE_BUFTAG_BIT)
		return lpfc_sli_ring_taggedbuf_get(phba, pring, tag);
	hbq_entry = lpfc_sli_hbqbuf_find(phba, tag);
	if (!hbq_entry)
		return NULL;
	return &hbq_entry->dbuf;
}

/**
 * lpfc_complete_unsol_iocb - Complete an unsolicited sequence
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @saveq: Pointer to the iocbq struct representing the sequence starting frame.
 * @fch_r_ctl: the r_ctl for the first frame of the sequence.
 * @fch_type: the type for the first frame of the sequence.
 *
 * This function is called with no lock held. This function uses the r_ctl and
 * type of the received sequence to find the correct callback function to call
 * to process the sequence.
 **/
static int
lpfc_complete_unsol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			 struct lpfc_iocbq *saveq, uint32_t fch_r_ctl,
			 uint32_t fch_type)
{
	int i;

	/* unSolicited Responses */
	if (pring->prt[0].profile) {
		if (pring->prt[0].lpfc_sli_rcv_unsol_event)
			(pring->prt[0].lpfc_sli_rcv_unsol_event) (phba, pring,
									saveq);
		return 1;
	}
	/* We must search, based on rctl / type
	   for the right routine */
	for (i = 0; i < pring->num_mask; i++) {
		if ((pring->prt[i].rctl == fch_r_ctl) &&
		    (pring->prt[i].type == fch_type)) {
			if (pring->prt[i].lpfc_sli_rcv_unsol_event)
				(pring->prt[i].lpfc_sli_rcv_unsol_event)
						(phba, pring, saveq);
			return 1;
		}
	}
	return 0;
}

/**
 * lpfc_sli_process_unsol_iocb - Unsolicited iocb handler
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @saveq: Pointer to the unsolicited iocb.
 *
 * This function is called with no lock held by the ring event handler
 * when there is an unsolicited iocb posted to the response ring by the
 * firmware. This function gets the buffer associated with the iocbs
 * and calls the event handler for the ring. This function handles both
 * qring buffers and hbq buffers.
 * When the function returns 1 the caller can free the iocb object otherwise
 * upper layer functions will free the iocb objects.
 **/
static int
lpfc_sli_process_unsol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			    struct lpfc_iocbq *saveq)
{
	IOCB_t           * irsp;
	WORD5            * w5p;
	uint32_t           Rctl, Type;
	uint32_t           match;
	struct lpfc_iocbq *iocbq;
	struct lpfc_dmabuf *dmzbuf;

	match = 0;
	irsp = &(saveq->iocb);

	if (irsp->ulpCommand == CMD_ASYNC_STATUS) {
		if (pring->lpfc_sli_rcv_async_status)
			pring->lpfc_sli_rcv_async_status(phba, pring, saveq);
		else
			lpfc_printf_log(phba,
					KERN_WARNING,
					LOG_SLI,
					"0316 Ring %d handler: unexpected "
					"ASYNC_STATUS iocb received evt_code "
					"0x%x\n",
					pring->ringno,
					irsp->un.asyncstat.evt_code);
		return 1;
	}

	if ((irsp->ulpCommand == CMD_IOCB_RET_XRI64_CX) &&
		(phba->sli3_options & LPFC_SLI3_HBQ_ENABLED)) {
		if (irsp->ulpBdeCount > 0) {
			dmzbuf = lpfc_sli_get_buff(phba, pring,
					irsp->un.ulpWord[3]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		if (irsp->ulpBdeCount > 1) {
			dmzbuf = lpfc_sli_get_buff(phba, pring,
					irsp->unsli3.sli3Words[3]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		if (irsp->ulpBdeCount > 2) {
			dmzbuf = lpfc_sli_get_buff(phba, pring,
				irsp->unsli3.sli3Words[7]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		return 1;
	}

	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) {
		if (irsp->ulpBdeCount != 0) {
			saveq->context2 = lpfc_sli_get_buff(phba, pring,
						irsp->un.ulpWord[3]);
			if (!saveq->context2)
				lpfc_printf_log(phba,
					KERN_ERR,
					LOG_SLI,
					"0341 Ring %d Cannot find buffer for "
					"an unsolicited iocb. tag 0x%x\n",
					pring->ringno,
					irsp->un.ulpWord[3]);
		}
		if (irsp->ulpBdeCount == 2) {
			saveq->context3 = lpfc_sli_get_buff(phba, pring,
						irsp->unsli3.sli3Words[7]);
			if (!saveq->context3)
				lpfc_printf_log(phba,
					KERN_ERR,
					LOG_SLI,
					"0342 Ring %d Cannot find buffer for an"
					" unsolicited iocb. tag 0x%x\n",
					pring->ringno,
					irsp->unsli3.sli3Words[7]);
		}
		list_for_each_entry(iocbq, &saveq->list, list) {
			irsp = &(iocbq->iocb);
			if (irsp->ulpBdeCount != 0) {
				iocbq->context2 = lpfc_sli_get_buff(phba, pring,
							irsp->un.ulpWord[3]);
				if (!iocbq->context2)
					lpfc_printf_log(phba,
						KERN_ERR,
						LOG_SLI,
						"0343 Ring %d Cannot find "
						"buffer for an unsolicited iocb"
						". tag 0x%x\n", pring->ringno,
						irsp->un.ulpWord[3]);
			}
			if (irsp->ulpBdeCount == 2) {
				iocbq->context3 = lpfc_sli_get_buff(phba, pring,
						irsp->unsli3.sli3Words[7]);
				if (!iocbq->context3)
					lpfc_printf_log(phba,
						KERN_ERR,
						LOG_SLI,
						"0344 Ring %d Cannot find "
						"buffer for an unsolicited "
						"iocb. tag 0x%x\n",
						pring->ringno,
						irsp->unsli3.sli3Words[7]);
			}
		}
	}
	if (irsp->ulpBdeCount != 0 &&
	    (irsp->ulpCommand == CMD_IOCB_RCV_CONT64_CX ||
	     irsp->ulpStatus == IOSTAT_INTERMED_RSP)) {
		int found = 0;

		/* search continue save q for same XRI */
		list_for_each_entry(iocbq, &pring->iocb_continue_saveq, clist) {
			if (iocbq->iocb.ulpContext == saveq->iocb.ulpContext) {
				list_add_tail(&saveq->list, &iocbq->list);
				found = 1;
				break;
			}
		}
		if (!found)
			list_add_tail(&saveq->clist,
				      &pring->iocb_continue_saveq);
		if (saveq->iocb.ulpStatus != IOSTAT_INTERMED_RSP) {
			list_del_init(&iocbq->clist);
			saveq = iocbq;
			irsp = &(saveq->iocb);
		} else
			return 0;
	}
	if ((irsp->ulpCommand == CMD_RCV_ELS_REQ64_CX) ||
	    (irsp->ulpCommand == CMD_RCV_ELS_REQ_CX) ||
	    (irsp->ulpCommand == CMD_IOCB_RCV_ELS64_CX)) {
		Rctl = FC_ELS_REQ;
		Type = FC_ELS_DATA;
	} else {
		w5p = (WORD5 *)&(saveq->iocb.un.ulpWord[5]);
		Rctl = w5p->hcsw.Rctl;
		Type = w5p->hcsw.Type;

		/* Firmware Workaround */
		if ((Rctl == 0) && (pring->ringno == LPFC_ELS_RING) &&
			(irsp->ulpCommand == CMD_RCV_SEQUENCE64_CX ||
			 irsp->ulpCommand == CMD_IOCB_RCV_SEQ64_CX)) {
			Rctl = FC_ELS_REQ;
			Type = FC_ELS_DATA;
			w5p->hcsw.Rctl = Rctl;
			w5p->hcsw.Type = Type;
		}
	}

	if (!lpfc_complete_unsol_iocb(phba, pring, saveq, Rctl, Type))
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0313 Ring %d handler: unexpected Rctl x%x "
				"Type x%x received\n",
				pring->ringno, Rctl, Type);

	return 1;
}

/**
 * lpfc_sli_iocbq_lookup - Find command iocb for the given response iocb
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @prspiocb: Pointer to response iocb object.
 *
 * This function looks up the iocb_lookup table to get the command iocb
 * corresponding to the given response iocb using the iotag of the
 * response iocb. This function is called with the hbalock held.
 * This function returns the command iocb object if it finds the command
 * iocb else returns NULL.
 **/
static struct lpfc_iocbq *
lpfc_sli_iocbq_lookup(struct lpfc_hba *phba,
		      struct lpfc_sli_ring *pring,
		      struct lpfc_iocbq *prspiocb)
{
	struct lpfc_iocbq *cmd_iocb = NULL;
	uint16_t iotag;

	iotag = prspiocb->iocb.ulpIoTag;

	if (iotag != 0 && iotag <= phba->sli.last_iotag) {
		cmd_iocb = phba->sli.iocbq_lookup[iotag];
		list_del_init(&cmd_iocb->list);
		pring->txcmplq_cnt--;
		return cmd_iocb;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0317 iotag x%x is out off "
			"range: max iotag x%x wd0 x%x\n",
			iotag, phba->sli.last_iotag,
			*(((uint32_t *) &prspiocb->iocb) + 7));
	return NULL;
}

/**
 * lpfc_sli_iocbq_lookup_by_tag - Find command iocb for the iotag
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @iotag: IOCB tag.
 *
 * This function looks up the iocb_lookup table to get the command iocb
 * corresponding to the given iotag. This function is called with the
 * hbalock held.
 * This function returns the command iocb object if it finds the command
 * iocb else returns NULL.
 **/
static struct lpfc_iocbq *
lpfc_sli_iocbq_lookup_by_tag(struct lpfc_hba *phba,
			     struct lpfc_sli_ring *pring, uint16_t iotag)
{
	struct lpfc_iocbq *cmd_iocb;

	if (iotag != 0 && iotag <= phba->sli.last_iotag) {
		cmd_iocb = phba->sli.iocbq_lookup[iotag];
		list_del_init(&cmd_iocb->list);
		pring->txcmplq_cnt--;
		return cmd_iocb;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0372 iotag x%x is out off range: max iotag (x%x)\n",
			iotag, phba->sli.last_iotag);
	return NULL;
}

/**
 * lpfc_sli_process_sol_iocb - process solicited iocb completion
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @saveq: Pointer to the response iocb to be processed.
 *
 * This function is called by the ring event handler for non-fcp
 * rings when there is a new response iocb in the response ring.
 * The caller is not required to hold any locks. This function
 * gets the command iocb associated with the response iocb and
 * calls the completion handler for the command iocb. If there
 * is no completion handler, the function will free the resources
 * associated with command iocb. If the response iocb is for
 * an already aborted command iocb, the status of the completion
 * is changed to IOSTAT_LOCAL_REJECT/IOERR_SLI_ABORTED.
 * This function always returns 1.
 **/
static int
lpfc_sli_process_sol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			  struct lpfc_iocbq *saveq)
{
	struct lpfc_iocbq *cmdiocbp;
	int rc = 1;
	unsigned long iflag;

	/* Based on the iotag field, get the cmd IOCB from the txcmplq */
	spin_lock_irqsave(&phba->hbalock, iflag);
	cmdiocbp = lpfc_sli_iocbq_lookup(phba, pring, saveq);
	spin_unlock_irqrestore(&phba->hbalock, iflag);

	if (cmdiocbp) {
		if (cmdiocbp->iocb_cmpl) {
			/*
			 * If an ELS command failed send an event to mgmt
			 * application.
			 */
			if (saveq->iocb.ulpStatus &&
			     (pring->ringno == LPFC_ELS_RING) &&
			     (cmdiocbp->iocb.ulpCommand ==
				CMD_ELS_REQUEST64_CR))
				lpfc_send_els_failure_event(phba,
					cmdiocbp, saveq);

			/*
			 * Post all ELS completions to the worker thread.
			 * All other are passed to the completion callback.
			 */
			if (pring->ringno == LPFC_ELS_RING) {
				if (cmdiocbp->iocb_flag & LPFC_DRIVER_ABORTED) {
					cmdiocbp->iocb_flag &=
						~LPFC_DRIVER_ABORTED;
					saveq->iocb.ulpStatus =
						IOSTAT_LOCAL_REJECT;
					saveq->iocb.un.ulpWord[4] =
						IOERR_SLI_ABORTED;

					/* Firmware could still be in progress
					 * of DMAing payload, so don't free data
					 * buffer till after a hbeat.
					 */
					saveq->iocb_flag |= LPFC_DELAY_MEM_FREE;
				}
			}
			(cmdiocbp->iocb_cmpl) (phba, cmdiocbp, saveq);
		} else
			lpfc_sli_release_iocbq(phba, cmdiocbp);
	} else {
		/*
		 * Unknown initiating command based on the response iotag.
		 * This could be the case on the ELS ring because of
		 * lpfc_els_abort().
		 */
		if (pring->ringno != LPFC_ELS_RING) {
			/*
			 * Ring <ringno> handler: unexpected completion IoTag
			 * <IoTag>
			 */
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					 "0322 Ring %d handler: "
					 "unexpected completion IoTag x%x "
					 "Data: x%x x%x x%x x%x\n",
					 pring->ringno,
					 saveq->iocb.ulpIoTag,
					 saveq->iocb.ulpStatus,
					 saveq->iocb.un.ulpWord[4],
					 saveq->iocb.ulpCommand,
					 saveq->iocb.ulpContext);
		}
	}

	return rc;
}

/**
 * lpfc_sli_rsp_pointers_error - Response ring pointer error handler
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function is called from the iocb ring event handlers when
 * put pointer is ahead of the get pointer for a ring. This function signal
 * an error attention condition to the worker thread and the worker
 * thread will transition the HBA to offline state.
 **/
static void
lpfc_sli_rsp_pointers_error(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	/*
	 * Ring <ringno> handler: portRspPut <portRspPut> is bigger than
	 * rsp ring <portRspMax>
	 */
	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0312 Ring %d handler: portRspPut %d "
			"is bigger than rsp ring %d\n",
			pring->ringno, le32_to_cpu(pgp->rspPutInx),
			pring->numRiocb);

	phba->link_state = LPFC_HBA_ERROR;

	/*
	 * All error attention handlers are posted to
	 * worker thread
	 */
	phba->work_ha |= HA_ERATT;
	phba->work_hs = HS_FFER3;

	lpfc_worker_wake_up(phba);

	return;
}

/**
 * lpfc_poll_eratt - Error attention polling timer timeout handler
 * @ptr: Pointer to address of HBA context object.
 *
 * This function is invoked by the Error Attention polling timer when the
 * timer times out. It will check the SLI Error Attention register for
 * possible attention events. If so, it will post an Error Attention event
 * and wake up worker thread to process it. Otherwise, it will set up the
 * Error Attention polling timer for the next poll.
 **/
void lpfc_poll_eratt(unsigned long ptr)
{
	struct lpfc_hba *phba;
	uint32_t eratt = 0;

	phba = (struct lpfc_hba *)ptr;

	/* Check chip HA register for error event */
	eratt = lpfc_sli_check_eratt(phba);

	if (eratt)
		/* Tell the worker thread there is work to do */
		lpfc_worker_wake_up(phba);
	else
		/* Restart the timer for next eratt poll */
		mod_timer(&phba->eratt_poll, jiffies +
					HZ * LPFC_ERATT_POLL_INTERVAL);
	return;
}

/**
 * lpfc_sli_poll_fcp_ring - Handle FCP ring completion in polling mode
 * @phba: Pointer to HBA context object.
 *
 * This function is called from lpfc_queuecommand, lpfc_poll_timeout,
 * lpfc_abort_handler and lpfc_slave_configure when FCP_RING_POLLING
 * is enabled.
 *
 * The caller does not hold any lock.
 * The function processes each response iocb in the response ring until it
 * finds an iocb with LE bit set and chains all the iocbs upto the iocb with
 * LE bit set. The function will call the completion handler of the command iocb
 * if the response iocb indicates a completion for a command iocb or it is
 * an abort completion.
 **/
void lpfc_sli_poll_fcp_ring(struct lpfc_hba *phba)
{
	struct lpfc_sli      *psli  = &phba->sli;
	struct lpfc_sli_ring *pring = &psli->ring[LPFC_FCP_RING];
	IOCB_t *irsp = NULL;
	IOCB_t *entry = NULL;
	struct lpfc_iocbq *cmdiocbq = NULL;
	struct lpfc_iocbq rspiocbq;
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	uint32_t status;
	uint32_t portRspPut, portRspMax;
	int type;
	uint32_t rsp_cmpl = 0;
	uint32_t ha_copy;
	unsigned long iflags;

	pring->stats.iocb_event++;

	/*
	 * The next available response entry should never exceed the maximum
	 * entries.  If it does, treat it as an adapter hardware error.
	 */
	portRspMax = pring->numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (unlikely(portRspPut >= portRspMax)) {
		lpfc_sli_rsp_pointers_error(phba, pring);
		return;
	}

	rmb();
	while (pring->rspidx != portRspPut) {
		entry = lpfc_resp_iocb(phba, pring);
		if (++pring->rspidx >= portRspMax)
			pring->rspidx = 0;

		lpfc_sli_pcimem_bcopy((uint32_t *) entry,
				      (uint32_t *) &rspiocbq.iocb,
				      phba->iocb_rsp_size);
		irsp = &rspiocbq.iocb;
		type = lpfc_sli_iocb_cmd_type(irsp->ulpCommand & CMD_IOCB_MASK);
		pring->stats.iocb_rsp++;
		rsp_cmpl++;

		if (unlikely(irsp->ulpStatus)) {
			/* Rsp ring <ringno> error: IOCB */
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0326 Rsp Ring %d error: IOCB Data: "
					"x%x x%x x%x x%x x%x x%x x%x x%x\n",
					pring->ringno,
					irsp->un.ulpWord[0],
					irsp->un.ulpWord[1],
					irsp->un.ulpWord[2],
					irsp->un.ulpWord[3],
					irsp->un.ulpWord[4],
					irsp->un.ulpWord[5],
					*(uint32_t *)&irsp->un1,
					*((uint32_t *)&irsp->un1 + 1));
		}

		switch (type) {
		case LPFC_ABORT_IOCB:
		case LPFC_SOL_IOCB:
			/*
			 * Idle exchange closed via ABTS from port.  No iocb
			 * resources need to be recovered.
			 */
			if (unlikely(irsp->ulpCommand == CMD_XRI_ABORTED_CX)) {
				lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
						"0314 IOCB cmd 0x%x "
						"processed. Skipping "
						"completion",
						irsp->ulpCommand);
				break;
			}

			spin_lock_irqsave(&phba->hbalock, iflags);
			cmdiocbq = lpfc_sli_iocbq_lookup(phba, pring,
							 &rspiocbq);
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			if ((cmdiocbq) && (cmdiocbq->iocb_cmpl)) {
				(cmdiocbq->iocb_cmpl)(phba, cmdiocbq,
						      &rspiocbq);
			}
			break;
		default:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adaptermsg[LPFC_MAX_ADPTMSG];
				memset(adaptermsg, 0, LPFC_MAX_ADPTMSG);
				memcpy(&adaptermsg[0], (uint8_t *) irsp,
				       MAX_MSG_DATA);
				dev_warn(&((phba->pcidev)->dev),
					 "lpfc%d: %s\n",
					 phba->brd_no, adaptermsg);
			} else {
				/* Unknown IOCB command */
				lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
						"0321 Unknown IOCB command "
						"Data: x%x, x%x x%x x%x x%x\n",
						type, irsp->ulpCommand,
						irsp->ulpStatus,
						irsp->ulpIoTag,
						irsp->ulpContext);
			}
			break;
		}

		/*
		 * The response IOCB has been processed.  Update the ring
		 * pointer in SLIM.  If the port response put pointer has not
		 * been updated, sync the pgp->rspPutInx and fetch the new port
		 * response put pointer.
		 */
		writel(pring->rspidx, &phba->host_gp[pring->ringno].rspGetInx);

		if (pring->rspidx == portRspPut)
			portRspPut = le32_to_cpu(pgp->rspPutInx);
	}

	ha_copy = readl(phba->HAregaddr);
	ha_copy >>= (LPFC_FCP_RING * 4);

	if ((rsp_cmpl > 0) && (ha_copy & HA_R0RE_REQ)) {
		spin_lock_irqsave(&phba->hbalock, iflags);
		pring->stats.iocb_rsp_full++;
		status = ((CA_R0ATT | CA_R0RE_RSP) << (LPFC_FCP_RING * 4));
		writel(status, phba->CAregaddr);
		readl(phba->CAregaddr);
		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}
	if ((ha_copy & HA_R0CE_RSP) &&
	    (pring->flag & LPFC_CALL_RING_AVAILABLE)) {
		spin_lock_irqsave(&phba->hbalock, iflags);
		pring->flag &= ~LPFC_CALL_RING_AVAILABLE;
		pring->stats.iocb_cmd_empty++;

		/* Force update of the local copy of cmdGetInx */
		pring->local_getidx = le32_to_cpu(pgp->cmdGetInx);
		lpfc_sli_resume_iocb(phba, pring);

		if ((pring->lpfc_sli_cmd_available))
			(pring->lpfc_sli_cmd_available) (phba, pring);

		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}

	return;
}

/**
 * lpfc_sli_handle_fast_ring_event - Handle ring events on FCP ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @mask: Host attention register mask for this ring.
 *
 * This function is called from the interrupt context when there is a ring
 * event for the fcp ring. The caller does not hold any lock.
 * The function processes each response iocb in the response ring until it
 * finds an iocb with LE bit set and chains all the iocbs upto the iocb with
 * LE bit set. The function will call the completion handler of the command iocb
 * if the response iocb indicates a completion for a command iocb or it is
 * an abort completion. The function will call lpfc_sli_process_unsol_iocb
 * function if this is an unsolicited iocb.
 * This routine presumes LPFC_FCP_RING handling and doesn't bother
 * to check it explicitly. This function always returns 1.
 **/
static int
lpfc_sli_handle_fast_ring_event(struct lpfc_hba *phba,
				struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	IOCB_t *irsp = NULL;
	IOCB_t *entry = NULL;
	struct lpfc_iocbq *cmdiocbq = NULL;
	struct lpfc_iocbq rspiocbq;
	uint32_t status;
	uint32_t portRspPut, portRspMax;
	int rc = 1;
	lpfc_iocb_type type;
	unsigned long iflag;
	uint32_t rsp_cmpl = 0;

	spin_lock_irqsave(&phba->hbalock, iflag);
	pring->stats.iocb_event++;

	/*
	 * The next available response entry should never exceed the maximum
	 * entries.  If it does, treat it as an adapter hardware error.
	 */
	portRspMax = pring->numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (unlikely(portRspPut >= portRspMax)) {
		lpfc_sli_rsp_pointers_error(phba, pring);
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return 1;
	}

	rmb();
	while (pring->rspidx != portRspPut) {
		/*
		 * Fetch an entry off the ring and copy it into a local data
		 * structure.  The copy involves a byte-swap since the
		 * network byte order and pci byte orders are different.
		 */
		entry = lpfc_resp_iocb(phba, pring);
		phba->last_completion_time = jiffies;

		if (++pring->rspidx >= portRspMax)
			pring->rspidx = 0;

		lpfc_sli_pcimem_bcopy((uint32_t *) entry,
				      (uint32_t *) &rspiocbq.iocb,
				      phba->iocb_rsp_size);
		INIT_LIST_HEAD(&(rspiocbq.list));
		irsp = &rspiocbq.iocb;

		type = lpfc_sli_iocb_cmd_type(irsp->ulpCommand & CMD_IOCB_MASK);
		pring->stats.iocb_rsp++;
		rsp_cmpl++;

		if (unlikely(irsp->ulpStatus)) {
			/*
			 * If resource errors reported from HBA, reduce
			 * queuedepths of the SCSI device.
			 */
			if ((irsp->ulpStatus == IOSTAT_LOCAL_REJECT) &&
				(irsp->un.ulpWord[4] == IOERR_NO_RESOURCES)) {
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				phba->lpfc_rampdown_queue_depth(phba);
				spin_lock_irqsave(&phba->hbalock, iflag);
			}

			/* Rsp ring <ringno> error: IOCB */
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0336 Rsp Ring %d error: IOCB Data: "
					"x%x x%x x%x x%x x%x x%x x%x x%x\n",
					pring->ringno,
					irsp->un.ulpWord[0],
					irsp->un.ulpWord[1],
					irsp->un.ulpWord[2],
					irsp->un.ulpWord[3],
					irsp->un.ulpWord[4],
					irsp->un.ulpWord[5],
					*(uint32_t *)&irsp->un1,
					*((uint32_t *)&irsp->un1 + 1));
		}

		switch (type) {
		case LPFC_ABORT_IOCB:
		case LPFC_SOL_IOCB:
			/*
			 * Idle exchange closed via ABTS from port.  No iocb
			 * resources need to be recovered.
			 */
			if (unlikely(irsp->ulpCommand == CMD_XRI_ABORTED_CX)) {
				lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
						"0333 IOCB cmd 0x%x"
						" processed. Skipping"
						" completion\n",
						irsp->ulpCommand);
				break;
			}

			cmdiocbq = lpfc_sli_iocbq_lookup(phba, pring,
							 &rspiocbq);
			if ((cmdiocbq) && (cmdiocbq->iocb_cmpl)) {
				if (phba->cfg_poll & ENABLE_FCP_RING_POLLING) {
					(cmdiocbq->iocb_cmpl)(phba, cmdiocbq,
							      &rspiocbq);
				} else {
					spin_unlock_irqrestore(&phba->hbalock,
							       iflag);
					(cmdiocbq->iocb_cmpl)(phba, cmdiocbq,
							      &rspiocbq);
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
				}
			}
			break;
		case LPFC_UNSOL_IOCB:
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			lpfc_sli_process_unsol_iocb(phba, pring, &rspiocbq);
			spin_lock_irqsave(&phba->hbalock, iflag);
			break;
		default:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adaptermsg[LPFC_MAX_ADPTMSG];
				memset(adaptermsg, 0, LPFC_MAX_ADPTMSG);
				memcpy(&adaptermsg[0], (uint8_t *) irsp,
				       MAX_MSG_DATA);
				dev_warn(&((phba->pcidev)->dev),
					 "lpfc%d: %s\n",
					 phba->brd_no, adaptermsg);
			} else {
				/* Unknown IOCB command */
				lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
						"0334 Unknown IOCB command "
						"Data: x%x, x%x x%x x%x x%x\n",
						type, irsp->ulpCommand,
						irsp->ulpStatus,
						irsp->ulpIoTag,
						irsp->ulpContext);
			}
			break;
		}

		/*
		 * The response IOCB has been processed.  Update the ring
		 * pointer in SLIM.  If the port response put pointer has not
		 * been updated, sync the pgp->rspPutInx and fetch the new port
		 * response put pointer.
		 */
		writel(pring->rspidx, &phba->host_gp[pring->ringno].rspGetInx);

		if (pring->rspidx == portRspPut)
			portRspPut = le32_to_cpu(pgp->rspPutInx);
	}

	if ((rsp_cmpl > 0) && (mask & HA_R0RE_REQ)) {
		pring->stats.iocb_rsp_full++;
		status = ((CA_R0ATT | CA_R0RE_RSP) << (pring->ringno * 4));
		writel(status, phba->CAregaddr);
		readl(phba->CAregaddr);
	}
	if ((mask & HA_R0CE_RSP) && (pring->flag & LPFC_CALL_RING_AVAILABLE)) {
		pring->flag &= ~LPFC_CALL_RING_AVAILABLE;
		pring->stats.iocb_cmd_empty++;

		/* Force update of the local copy of cmdGetInx */
		pring->local_getidx = le32_to_cpu(pgp->cmdGetInx);
		lpfc_sli_resume_iocb(phba, pring);

		if ((pring->lpfc_sli_cmd_available))
			(pring->lpfc_sli_cmd_available) (phba, pring);

	}

	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return rc;
}

/**
 * lpfc_sli_sp_handle_rspiocb - Handle slow-path response iocb
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @rspiocbp: Pointer to driver response IOCB object.
 *
 * This function is called from the worker thread when there is a slow-path
 * response IOCB to process. This function chains all the response iocbs until
 * seeing the iocb with the LE bit set. The function will call
 * lpfc_sli_process_sol_iocb function if the response iocb indicates a
 * completion of a command iocb. The function will call the
 * lpfc_sli_process_unsol_iocb function if this is an unsolicited iocb.
 * The function frees the resources or calls the completion handler if this
 * iocb is an abort completion. The function returns NULL when the response
 * iocb has the LE bit set and all the chained iocbs are processed, otherwise
 * this function shall chain the iocb on to the iocb_continueq and return the
 * response iocb passed in.
 **/
static struct lpfc_iocbq *
lpfc_sli_sp_handle_rspiocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			struct lpfc_iocbq *rspiocbp)
{
	struct lpfc_iocbq *saveq;
	struct lpfc_iocbq *cmdiocbp;
	struct lpfc_iocbq *next_iocb;
	IOCB_t *irsp = NULL;
	uint32_t free_saveq;
	uint8_t iocb_cmd_type;
	lpfc_iocb_type type;
	unsigned long iflag;
	int rc;

	spin_lock_irqsave(&phba->hbalock, iflag);
	/* First add the response iocb to the countinueq list */
	list_add_tail(&rspiocbp->list, &(pring->iocb_continueq));
	pring->iocb_continueq_cnt++;

	/* Now, determine whetehr the list is completed for processing */
	irsp = &rspiocbp->iocb;
	if (irsp->ulpLe) {
		/*
		 * By default, the driver expects to free all resources
		 * associated with this iocb completion.
		 */
		free_saveq = 1;
		saveq = list_get_first(&pring->iocb_continueq,
				       struct lpfc_iocbq, list);
		irsp = &(saveq->iocb);
		list_del_init(&pring->iocb_continueq);
		pring->iocb_continueq_cnt = 0;

		pring->stats.iocb_rsp++;

		/*
		 * If resource errors reported from HBA, reduce
		 * queuedepths of the SCSI device.
		 */
		if ((irsp->ulpStatus == IOSTAT_LOCAL_REJECT) &&
		    (irsp->un.ulpWord[4] == IOERR_NO_RESOURCES)) {
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			phba->lpfc_rampdown_queue_depth(phba);
			spin_lock_irqsave(&phba->hbalock, iflag);
		}

		if (irsp->ulpStatus) {
			/* Rsp ring <ringno> error: IOCB */
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0328 Rsp Ring %d error: "
					"IOCB Data: "
					"x%x x%x x%x x%x "
					"x%x x%x x%x x%x "
					"x%x x%x x%x x%x "
					"x%x x%x x%x x%x\n",
					pring->ringno,
					irsp->un.ulpWord[0],
					irsp->un.ulpWord[1],
					irsp->un.ulpWord[2],
					irsp->un.ulpWord[3],
					irsp->un.ulpWord[4],
					irsp->un.ulpWord[5],
					*(((uint32_t *) irsp) + 6),
					*(((uint32_t *) irsp) + 7),
					*(((uint32_t *) irsp) + 8),
					*(((uint32_t *) irsp) + 9),
					*(((uint32_t *) irsp) + 10),
					*(((uint32_t *) irsp) + 11),
					*(((uint32_t *) irsp) + 12),
					*(((uint32_t *) irsp) + 13),
					*(((uint32_t *) irsp) + 14),
					*(((uint32_t *) irsp) + 15));
		}

		/*
		 * Fetch the IOCB command type and call the correct completion
		 * routine. Solicited and Unsolicited IOCBs on the ELS ring
		 * get freed back to the lpfc_iocb_list by the discovery
		 * kernel thread.
		 */
		iocb_cmd_type = irsp->ulpCommand & CMD_IOCB_MASK;
		type = lpfc_sli_iocb_cmd_type(iocb_cmd_type);
		switch (type) {
		case LPFC_SOL_IOCB:
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			rc = lpfc_sli_process_sol_iocb(phba, pring, saveq);
			spin_lock_irqsave(&phba->hbalock, iflag);
			break;

		case LPFC_UNSOL_IOCB:
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			rc = lpfc_sli_process_unsol_iocb(phba, pring, saveq);
			spin_lock_irqsave(&phba->hbalock, iflag);
			if (!rc)
				free_saveq = 0;
			break;

		case LPFC_ABORT_IOCB:
			cmdiocbp = NULL;
			if (irsp->ulpCommand != CMD_XRI_ABORTED_CX)
				cmdiocbp = lpfc_sli_iocbq_lookup(phba, pring,
								 saveq);
			if (cmdiocbp) {
				/* Call the specified completion routine */
				if (cmdiocbp->iocb_cmpl) {
					spin_unlock_irqrestore(&phba->hbalock,
							       iflag);
					(cmdiocbp->iocb_cmpl)(phba, cmdiocbp,
							      saveq);
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
				} else
					__lpfc_sli_release_iocbq(phba,
								 cmdiocbp);
			}
			break;

		case LPFC_UNKNOWN_IOCB:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adaptermsg[LPFC_MAX_ADPTMSG];
				memset(adaptermsg, 0, LPFC_MAX_ADPTMSG);
				memcpy(&adaptermsg[0], (uint8_t *)irsp,
				       MAX_MSG_DATA);
				dev_warn(&((phba->pcidev)->dev),
					 "lpfc%d: %s\n",
					 phba->brd_no, adaptermsg);
			} else {
				/* Unknown IOCB command */
				lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
						"0335 Unknown IOCB "
						"command Data: x%x "
						"x%x x%x x%x\n",
						irsp->ulpCommand,
						irsp->ulpStatus,
						irsp->ulpIoTag,
						irsp->ulpContext);
			}
			break;
		}

		if (free_saveq) {
			list_for_each_entry_safe(rspiocbp, next_iocb,
						 &saveq->list, list) {
				list_del(&rspiocbp->list);
				__lpfc_sli_release_iocbq(phba, rspiocbp);
			}
			__lpfc_sli_release_iocbq(phba, saveq);
		}
		rspiocbp = NULL;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return rspiocbp;
}

/**
 * lpfc_sli_handle_slow_ring_event - Wrapper func for handling slow-path iocbs
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @mask: Host attention register mask for this ring.
 *
 * This routine wraps the actual slow_ring event process routine from the
 * API jump table function pointer from the lpfc_hba struct.
 **/
void
lpfc_sli_handle_slow_ring_event(struct lpfc_hba *phba,
				struct lpfc_sli_ring *pring, uint32_t mask)
{
	phba->lpfc_sli_handle_slow_ring_event(phba, pring, mask);
}

/**
 * lpfc_sli_handle_slow_ring_event_s3 - Handle SLI3 ring event for non-FCP rings
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @mask: Host attention register mask for this ring.
 *
 * This function is called from the worker thread when there is a ring event
 * for non-fcp rings. The caller does not hold any lock. The function will
 * remove each response iocb in the response ring and calls the handle
 * response iocb routine (lpfc_sli_sp_handle_rspiocb) to process it.
 **/
static void
lpfc_sli_handle_slow_ring_event_s3(struct lpfc_hba *phba,
				   struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_pgp *pgp;
	IOCB_t *entry;
	IOCB_t *irsp = NULL;
	struct lpfc_iocbq *rspiocbp = NULL;
	uint32_t portRspPut, portRspMax;
	unsigned long iflag;
	uint32_t status;

	pgp = &phba->port_gp[pring->ringno];
	spin_lock_irqsave(&phba->hbalock, iflag);
	pring->stats.iocb_event++;

	/*
	 * The next available response entry should never exceed the maximum
	 * entries.  If it does, treat it as an adapter hardware error.
	 */
	portRspMax = pring->numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (portRspPut >= portRspMax) {
		/*
		 * Ring <ringno> handler: portRspPut <portRspPut> is bigger than
		 * rsp ring <portRspMax>
		 */
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0303 Ring %d handler: portRspPut %d "
				"is bigger than rsp ring %d\n",
				pring->ringno, portRspPut, portRspMax);

		phba->link_state = LPFC_HBA_ERROR;
		spin_unlock_irqrestore(&phba->hbalock, iflag);

		phba->work_hs = HS_FFER3;
		lpfc_handle_eratt(phba);

		return;
	}

	rmb();
	while (pring->rspidx != portRspPut) {
		/*
		 * Build a completion list and call the appropriate handler.
		 * The process is to get the next available response iocb, get
		 * a free iocb from the list, copy the response data into the
		 * free iocb, insert to the continuation list, and update the
		 * next response index to slim.  This process makes response
		 * iocb's in the ring available to DMA as fast as possible but
		 * pays a penalty for a copy operation.  Since the iocb is
		 * only 32 bytes, this penalty is considered small relative to
		 * the PCI reads for register values and a slim write.  When
		 * the ulpLe field is set, the entire Command has been
		 * received.
		 */
		entry = lpfc_resp_iocb(phba, pring);

		phba->last_completion_time = jiffies;
		rspiocbp = __lpfc_sli_get_iocbq(phba);
		if (rspiocbp == NULL) {
			printk(KERN_ERR "%s: out of buffers! Failing "
			       "completion.\n", __func__);
			break;
		}

		lpfc_sli_pcimem_bcopy(entry, &rspiocbp->iocb,
				      phba->iocb_rsp_size);
		irsp = &rspiocbp->iocb;

		if (++pring->rspidx >= portRspMax)
			pring->rspidx = 0;

		if (pring->ringno == LPFC_ELS_RING) {
			lpfc_debugfs_slow_ring_trc(phba,
			"IOCB rsp ring:   wd4:x%08x wd6:x%08x wd7:x%08x",
				*(((uint32_t *) irsp) + 4),
				*(((uint32_t *) irsp) + 6),
				*(((uint32_t *) irsp) + 7));
		}

		writel(pring->rspidx, &phba->host_gp[pring->ringno].rspGetInx);

		spin_unlock_irqrestore(&phba->hbalock, iflag);
		/* Handle the response IOCB */
		rspiocbp = lpfc_sli_sp_handle_rspiocb(phba, pring, rspiocbp);
		spin_lock_irqsave(&phba->hbalock, iflag);

		/*
		 * If the port response put pointer has not been updated, sync
		 * the pgp->rspPutInx in the MAILBOX_tand fetch the new port
		 * response put pointer.
		 */
		if (pring->rspidx == portRspPut) {
			portRspPut = le32_to_cpu(pgp->rspPutInx);
		}
	} /* while (pring->rspidx != portRspPut) */

	if ((rspiocbp != NULL) && (mask & HA_R0RE_REQ)) {
		/* At least one response entry has been freed */
		pring->stats.iocb_rsp_full++;
		/* SET RxRE_RSP in Chip Att register */
		status = ((CA_R0ATT | CA_R0RE_RSP) << (pring->ringno * 4));
		writel(status, phba->CAregaddr);
		readl(phba->CAregaddr); /* flush */
	}
	if ((mask & HA_R0CE_RSP) && (pring->flag & LPFC_CALL_RING_AVAILABLE)) {
		pring->flag &= ~LPFC_CALL_RING_AVAILABLE;
		pring->stats.iocb_cmd_empty++;

		/* Force update of the local copy of cmdGetInx */
		pring->local_getidx = le32_to_cpu(pgp->cmdGetInx);
		lpfc_sli_resume_iocb(phba, pring);

		if ((pring->lpfc_sli_cmd_available))
			(pring->lpfc_sli_cmd_available) (phba, pring);

	}

	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return;
}

/**
 * lpfc_sli_handle_slow_ring_event_s4 - Handle SLI4 slow-path els events
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @mask: Host attention register mask for this ring.
 *
 * This function is called from the worker thread when there is a pending
 * ELS response iocb on the driver internal slow-path response iocb worker
 * queue. The caller does not hold any lock. The function will remove each
 * response iocb from the response worker queue and calls the handle
 * response iocb routine (lpfc_sli_sp_handle_rspiocb) to process it.
 **/
static void
lpfc_sli_handle_slow_ring_event_s4(struct lpfc_hba *phba,
				   struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_iocbq *irspiocbq;
	unsigned long iflag;

	while (!list_empty(&phba->sli4_hba.sp_rspiocb_work_queue)) {
		/* Get the response iocb from the head of work queue */
		spin_lock_irqsave(&phba->hbalock, iflag);
		list_remove_head(&phba->sli4_hba.sp_rspiocb_work_queue,
				 irspiocbq, struct lpfc_iocbq, list);
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		/* Process the response iocb */
		lpfc_sli_sp_handle_rspiocb(phba, pring, irspiocbq);
	}
}

/**
 * lpfc_sli_abort_iocb_ring - Abort all iocbs in the ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function aborts all iocbs in the given ring and frees all the iocb
 * objects in txq. This function issues an abort iocb for all the iocb commands
 * in txcmplq. The iocbs in the txcmplq is not guaranteed to complete before
 * the return of this function. The caller is not required to hold any locks.
 **/
void
lpfc_sli_abort_iocb_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	LIST_HEAD(completions);
	struct lpfc_iocbq *iocb, *next_iocb;

	if (pring->ringno == LPFC_ELS_RING) {
		lpfc_fabric_abort_hba(phba);
	}

	/* Error everything on txq and txcmplq
	 * First do the txq.
	 */
	spin_lock_irq(&phba->hbalock);
	list_splice_init(&pring->txq, &completions);
	pring->txq_cnt = 0;

	/* Next issue ABTS for everything on the txcmplq */
	list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq, list)
		lpfc_sli_issue_abort_iotag(phba, pring, iocb);

	spin_unlock_irq(&phba->hbalock);

	/* Cancel all the IOCBs from the completions list */
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

/**
 * lpfc_sli_flush_fcp_rings - flush all iocbs in the fcp ring
 * @phba: Pointer to HBA context object.
 *
 * This function flushes all iocbs in the fcp ring and frees all the iocb
 * objects in txq and txcmplq. This function will not issue abort iocbs
 * for all the iocb commands in txcmplq, they will just be returned with
 * IOERR_SLI_DOWN. This function is invoked with EEH when device's PCI
 * slot has been permanently disabled.
 **/
void
lpfc_sli_flush_fcp_rings(struct lpfc_hba *phba)
{
	LIST_HEAD(txq);
	LIST_HEAD(txcmplq);
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring  *pring;

	/* Currently, only one fcp ring */
	pring = &psli->ring[psli->fcp_ring];

	spin_lock_irq(&phba->hbalock);
	/* Retrieve everything on txq */
	list_splice_init(&pring->txq, &txq);
	pring->txq_cnt = 0;

	/* Retrieve everything on the txcmplq */
	list_splice_init(&pring->txcmplq, &txcmplq);
	pring->txcmplq_cnt = 0;
	spin_unlock_irq(&phba->hbalock);

	/* Flush the txq */
	lpfc_sli_cancel_iocbs(phba, &txq, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);

	/* Flush the txcmpq */
	lpfc_sli_cancel_iocbs(phba, &txcmplq, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);
}

/**
 * lpfc_sli_brdready_s3 - Check for sli3 host ready status
 * @phba: Pointer to HBA context object.
 * @mask: Bit mask to be checked.
 *
 * This function reads the host status register and compares
 * with the provided bit mask to check if HBA completed
 * the restart. This function will wait in a loop for the
 * HBA to complete restart. If the HBA does not restart within
 * 15 iterations, the function will reset the HBA again. The
 * function returns 1 when HBA fail to restart otherwise returns
 * zero.
 **/
static int
lpfc_sli_brdready_s3(struct lpfc_hba *phba, uint32_t mask)
{
	uint32_t status;
	int i = 0;
	int retval = 0;

	/* Read the HBA Host Status Register */
	status = readl(phba->HSregaddr);

	/*
	 * Check status register every 100ms for 5 retries, then every
	 * 500ms for 5, then every 2.5 sec for 5, then reset board and
	 * every 2.5 sec for 4.
	 * Break our of the loop if errors occurred during init.
	 */
	while (((status & mask) != mask) &&
	       !(status & HS_FFERM) &&
	       i++ < 20) {

		if (i <= 5)
			msleep(10);
		else if (i <= 10)
			msleep(500);
		else
			msleep(2500);

		if (i == 15) {
				/* Do post */
			phba->pport->port_state = LPFC_VPORT_UNKNOWN;
			lpfc_sli_brdrestart(phba);
		}
		/* Read the HBA Host Status Register */
		status = readl(phba->HSregaddr);
	}

	/* Check to see if any errors occurred during init */
	if ((status & HS_FFERM) || (i >= 20)) {
		phba->link_state = LPFC_HBA_ERROR;
		retval = 1;
	}

	return retval;
}

/**
 * lpfc_sli_brdready_s4 - Check for sli4 host ready status
 * @phba: Pointer to HBA context object.
 * @mask: Bit mask to be checked.
 *
 * This function checks the host status register to check if HBA is
 * ready. This function will wait in a loop for the HBA to be ready
 * If the HBA is not ready , the function will will reset the HBA PCI
 * function again. The function returns 1 when HBA fail to be ready
 * otherwise returns zero.
 **/
static int
lpfc_sli_brdready_s4(struct lpfc_hba *phba, uint32_t mask)
{
	uint32_t status;
	int retval = 0;

	/* Read the HBA Host Status Register */
	status = lpfc_sli4_post_status_check(phba);

	if (status) {
		phba->pport->port_state = LPFC_VPORT_UNKNOWN;
		lpfc_sli_brdrestart(phba);
		status = lpfc_sli4_post_status_check(phba);
	}

	/* Check to see if any errors occurred during init */
	if (status) {
		phba->link_state = LPFC_HBA_ERROR;
		retval = 1;
	} else
		phba->sli4_hba.intr_enable = 0;

	return retval;
}

/**
 * lpfc_sli_brdready - Wrapper func for checking the hba readyness
 * @phba: Pointer to HBA context object.
 * @mask: Bit mask to be checked.
 *
 * This routine wraps the actual SLI3 or SLI4 hba readyness check routine
 * from the API jump table function pointer from the lpfc_hba struct.
 **/
int
lpfc_sli_brdready(struct lpfc_hba *phba, uint32_t mask)
{
	return phba->lpfc_sli_brdready(phba, mask);
}

#define BARRIER_TEST_PATTERN (0xdeadbeef)

/**
 * lpfc_reset_barrier - Make HBA ready for HBA reset
 * @phba: Pointer to HBA context object.
 *
 * This function is called before resetting an HBA. This
 * function requests HBA to quiesce DMAs before a reset.
 **/
void lpfc_reset_barrier(struct lpfc_hba *phba)
{
	uint32_t __iomem *resp_buf;
	uint32_t __iomem *mbox_buf;
	volatile uint32_t mbox;
	uint32_t hc_copy;
	int  i;
	uint8_t hdrtype;

	pci_read_config_byte(phba->pcidev, PCI_HEADER_TYPE, &hdrtype);
	if (hdrtype != 0x80 ||
	    (FC_JEDEC_ID(phba->vpd.rev.biuRev) != HELIOS_JEDEC_ID &&
	     FC_JEDEC_ID(phba->vpd.rev.biuRev) != THOR_JEDEC_ID))
		return;

	/*
	 * Tell the other part of the chip to suspend temporarily all
	 * its DMA activity.
	 */
	resp_buf = phba->MBslimaddr;

	/* Disable the error attention */
	hc_copy = readl(phba->HCregaddr);
	writel((hc_copy & ~HC_ERINT_ENA), phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */
	phba->link_flag |= LS_IGNORE_ERATT;

	if (readl(phba->HAregaddr) & HA_ERATT) {
		/* Clear Chip error bit */
		writel(HA_ERATT, phba->HAregaddr);
		phba->pport->stopped = 1;
	}

	mbox = 0;
	((MAILBOX_t *)&mbox)->mbxCommand = MBX_KILL_BOARD;
	((MAILBOX_t *)&mbox)->mbxOwner = OWN_CHIP;

	writel(BARRIER_TEST_PATTERN, (resp_buf + 1));
	mbox_buf = phba->MBslimaddr;
	writel(mbox, mbox_buf);

	for (i = 0;
	     readl(resp_buf + 1) != ~(BARRIER_TEST_PATTERN) && i < 50; i++)
		mdelay(1);

	if (readl(resp_buf + 1) != ~(BARRIER_TEST_PATTERN)) {
		if (phba->sli.sli_flag & LPFC_SLI_ACTIVE ||
		    phba->pport->stopped)
			goto restore_hc;
		else
			goto clear_errat;
	}

	((MAILBOX_t *)&mbox)->mbxOwner = OWN_HOST;
	for (i = 0; readl(resp_buf) != mbox &&  i < 500; i++)
		mdelay(1);

clear_errat:

	while (!(readl(phba->HAregaddr) & HA_ERATT) && ++i < 500)
		mdelay(1);

	if (readl(phba->HAregaddr) & HA_ERATT) {
		writel(HA_ERATT, phba->HAregaddr);
		phba->pport->stopped = 1;
	}

restore_hc:
	phba->link_flag &= ~LS_IGNORE_ERATT;
	writel(hc_copy, phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */
}

/**
 * lpfc_sli_brdkill - Issue a kill_board mailbox command
 * @phba: Pointer to HBA context object.
 *
 * This function issues a kill_board mailbox command and waits for
 * the error attention interrupt. This function is called for stopping
 * the firmware processing. The caller is not required to hold any
 * locks. This function calls lpfc_hba_down_post function to free
 * any pending commands after the kill. The function will return 1 when it
 * fails to kill the board else will return 0.
 **/
int
lpfc_sli_brdkill(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	LPFC_MBOXQ_t *pmb;
	uint32_t status;
	uint32_t ha_copy;
	int retval;
	int i = 0;

	psli = &phba->sli;

	/* Kill HBA */
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0329 Kill HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb)
		return 1;

	/* Disable the error attention */
	spin_lock_irq(&phba->hbalock);
	status = readl(phba->HCregaddr);
	status &= ~HC_ERINT_ENA;
	writel(status, phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */
	phba->link_flag |= LS_IGNORE_ERATT;
	spin_unlock_irq(&phba->hbalock);

	lpfc_kill_board(phba, pmb);
	pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	retval = l/*******issue*****(phba, pmb, MBX_NOWAIT);

	if (*******!=*****SUCCESS) {
	ile is part of the BUSY)
			mempool_free(*********->*****mem_ Fib);
		spin_lock_irq(&Host Bhba         Host Blink_flag &= ~LS_IGNORE_ERATT         un                     *
 * Coreturn 1;
	}

                           *
 * Cpsli->**** 2004-2009PFC_SLI_ACTIVE;rademarserved.           *
 * EMULE
*
 * Fibre Channel Host Bus Adapters.    
	/* There is no completion for a KILL_BOARD **** cmd. Check      n error
	 * atten     every 100ms     3 seconds. If we don't get  All  afte       *
 * Thiograstill set HBA_ERROR state because thute atus ofr th     boardtoph Hw undefined.    /
	ha_copy = readl*****->HAregaddr04-20while ((i++ < 30) && !(* Public& H  *
ATT)Linux mdelay(100     * Public License as published by te tradel_timer_sync(&      *****tmoful.le ion.    *
 * This prinux writel(* This pl Host Bll be useful.opyrighpport->stopped =I are tademarks of Emulex.                        *
 * www.emulex.coMBOXom         IMPLIED COactive = NULL    yright (C) 2004-2009 Emulex.  All rig                                   *****hba_down_post*****     yright (C)odify = emuler   *
 * 4-20and SLIon.    *
 * This p ? 0 :I ar}

/**
 ***********brdrend/o- Rwith a     2 orage. 3or  G  *@icen: Pointer toor   context object.
 NG  *This func      withsr the*
 *by     ing HC_INITFF     the****rolG  *register. Ae; y***************, t********************allude <iocb ringG  *indices.***************disables PCI layer parity c    ****du<linux/d the with*********************d SLs 0 alwayssi_devie callertoph Ht required    hold any     ost.h*/
int

 * included with(struct******hba *Publi
{
	 "lpfc_hw4.hsli *    RE Hc_hw.h"
#inclu_<lin *p<lin;
	uint16_t cfg_valueIONSnt i4-20     =        "lpfc2005 is pac*
 * *
 *****sli4tf_log*******KERN_INFO, LOGex.c,   *"0325"lpfc_scsi.Data: x%xincl\n"#inclPLIED WARRANTARRA  *
 *,           *
 *04-2005 perform 2 of t with  *
 icensefc_eventTag = 0c LicenseWARRANTfc_myDIDompletion types. */
typprevef enum _2005 C SLIoff<scsi/scsi_cmnd.hand serrh>
#ince <scphysical* There are ci_icen_config_worde as pupcidev,
#in_COMMAND, &"lpfc_nl.     ci_    *
/* Provide function prototypes local t   *  *, L("lpfc_nl. &_hba *, LP ~(ypes local _PARITY |types local _SERR))04-20          *
 * www.(emulex.com      |ore dePROlex _LA04-2005 Now toggle */

#inbit in******ost Cinux/b R.h>
#ine are    *
 ****/

#iTIES, INCC be useful.is distrful.icense as pubq->iocb;
} /* flush_iocbq *iocb0urn &iocbq->iocb;
}

sli4_wq_put - Put a Work Queue Entryude "lpftore
#inccm/* Tt lpfc_iocb*/
static int lpfc_sli_issue_mbox_s4(struct lpfc "lpfc_nl.04-2005 Initializscsilevant SLI infoe are    (fc_d0; i <ebugfs.num "lpfs; i++      sli4.c_dis     <lin[i]Y IMP<lin-> 2004mpletithe
 * Hrspidxo start processnext_cmd theo start processlocal_get the Work Queue EnThis fu start processmissbufcnt are av traicense for  *
 * more deWARM_STAR rigand SLI0e COPYING  *
 * incl4uded with this package. 4                          *
 ********************************************atry s rou

#include <scsi/scsi.h>
#include <scsi/si_desi_cmnd.h>
#inc************device

#iinclude <scsi/scsi_transport_fc.i_de>
#include <si_device.h>
#include <scsi/scsi_host.hcsi/fc/fc_fs.h> hbalock we "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpc_disc.h"
#incl
#include "lpfc_nl.h"
#inc8_t qindxclude "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_crtn.h"
#includ29"lpfc_logmsg.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "lpfc_vport.h"

/* There are only four IOCB completion types. */
typedef enum _lpfc_iocb_type {
	LPFC_UNKNOWN_IOCB,
	LPFC_UNSOL_IOCB,
	LPFC_SOL_IOCB,
	LPFC_ABORT_IOCB
} lpfc_iocb_type;


/* Provide function prototypes local to this module. */
static int lpfc_sli_issue_mbox_s4(struct lpfc_hba *, LPFC_MBOXQ_t *,
				  uit32_t);
static int lpfc_sli4_read_rev(struct ldemarks of Emulex.                        *
 * www.XQ_t *, uint32_t *)e only fof.fcf *
 * mpleti/* Clean upude <lhild queue list      theCQs.h"
#iist_* ALinit        re dohba.mbx_wqght sthba.0;
}

/**
 * lpfc_sli4_wq_releels - Updates internal hba index for WQ
 * @q:hdr_r Updates internal hba index for WQ
 * @q:datex to advance the hba index to.
 *
 * This ase c Updates internal hba index for WQ
 * @q: Theflect consumption of
 * Work Queue Entriesrxq the HBA. Whe@q. Tion fis funy the < Host B"lpffcp_wq_countt calls++    0;
}

/**
 * lpfc_sli4_wq_relen to u[ion f]umed
 * an entry the host calls this function toeupdate the queue's internal
 * pointers. This routine ceturns the number                                    
staticRT_IOCB
ly* Thereuct lpfc_r.h"
#include "lpfc.h"
#include "lpfc_crtn.*/

#include89 P_vport].wq#inc**************!\n"hba./*q->entry FCoEunt);
		released++;.h"
#includci_********_*/
	ifPublic caller is expected to hold theuded witae "l3 this ailbckage. 3 hba                      *
 *************************************isncludedt_iocb_fry on
next ava     code path tolkdev.
 * @q*******egister doorbell;
	uint32_t host_ind

	/* If si_device.h>
#inclu    *s*****RE* The mail     omm_SOLclude <SLIMC_SOy the c***********. Aents oend of ver;
		relea, itncluds_hw4.h"
#NU Generali_de*********to e Ch

	/*pend].wqoorbell>

#iueue Entry ensi.h>i_dePOST onlyB,
	LPFC_ABOfirset_i
 * This roue on @q then thde <scsizerost.h>
#inude <scsi/oecsi/scguarante <liwig      ofring the Work Queue qe[q-orbell befeue  <scsid SLIof ve*********** entry  teric /fc/fc_fs.h>

#inclailbox e "lpfc_hw4.h"
#include "lpMAILENT,t *mbfc_sli.h"
#includede "lpfc_volatree #inc32_t ide letivoid __iomem *tonclumell_id, &doorbell, q->queue_id);
	we "lpfc_disc.h"
#include "lpf * @qcsi.h"
#include "lpfc.h"
#include "lpfc_crtn.h"
#include37try_count) ==.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "as nodl(q->pmb = (egister doo) &as not ymbLIEDxCorbell f the the Woremp_mqe;

HcRCHANT
#incluA to _barrieri4_mq_put he next = Host BMBnexted b;cbq *iocb*(he host h*) ****he nexthe Work Qu>entry_co.
 * @wqe: The worOretuskip eralare; y fc_ff Theut ontic uiede arele impat.h"
#include "lpfc_    on */
	q1;005 Chisut orehba_iset*****uphas n1e areelsefc_mq_doorb0ll_id, &doorbell, q->queue_id);
	writel(dhost_index;
	q->host_index + sizeof t_index +hba.((q->host_index + 1) % q->entry_count);

	/* Ring Doorbell */
	doofc_fs.h>

#includePublic LicenseWARRANTY OF MERCHletion typ for  *
 * more de*/

 * The ca         
	readl(q->p                                      
	iforbell lor  *
 _off>
#inc0, /* Flu(the HBA. When the HBA ructlpfc_hbatats  *
rn thget_
 * Thi(04-2005 GED,  Que*/

#in_SOLP0;
	L EXl. I>quele.e areis distribute See the GNU General Public t - Put a Mailbox Queue Entry on an Mailbox4Queue
 * @q the this ilbox Queue to operate on.
 * @wqe: The Mailbox Queue Entry to put on the Work queue.
 *
 * This routine will cophe conteex;

n lpfc_wqe *wto the next available entry on
 * the @q. Thisssing the Work Queue Entry. This function returns 0 if
successful.  * su no entries are availableruct lpfc_mqe *temp_mqe = q->qe[q->hos4f (((q->host_index + 1) % q->entry_count) == q->hba_index)
		req->entry_count) == q->hba_index)
		return -ENOMEM;
	lpfc_sli_pcimem_b296y(mqe, temp_mqe, q->entry_size);
	/* Save off the mailbox pointer for completi we are done */
	if that werdemarks of Emulex.               e to operate on.
 *
 * This routine will update the HBA index of a queue to reflect consumption of
 * a Mailbox Queue Entry by the HBA. When the HBA indicates that it has consumed
 * an entry the host calls this function to ue number of entries that were consumed by
 * the HBA.
 **/
static u - Wrappset(unc     qe[q->h].wqilbox Queue to operate on.
 * @wqe: The Mailbox Queue Enroutine wrap******actualnal 3     lpfc"
#ie conten
	q->hbafrom versli4_PI jump tsi.hcalling thp       eturn eq_hw4.h"
#isli.h".
scsi/fc/fc_fs.h>

#inclcalle "lpfc_hw4.h"
#include "lpuct lpficense cessing an EQ
 * @qPublic COPYING  *
 * incluchip */
_mq_d- WaiFlush */

e contenork Que*
 *bf_set->entry_cox Queue to operate on.
 * @wqe: The Mailbox Queue Entry to put on the Wroutine /pci.h>pfc_slo wts to arsuccessfublkdev.s CQ.
 *
 * This. Sssed, as this CQ.
 *
 * This &doolay.aum_pb>qe[qHS_FFRDYinterHS_MBt wibitis pro * This failsdex = ((q->    ny to th15ux/deters rounclu_sli4_eq_rewit ae contents of @ agais expected to hold the e;
}balo if lastessed, as a_indecalled oorbold the hnegaMED,      tine om
 *
 * This routine will her the hoste "lpfc_hw4.h"
#include "lphe host h terms, his fuhe work ad by ringirom_iSterms uct lpfc_iocb terms  License as pubSished by thehba-      terms  put on tnterse what currend whenstophosted
 * Thi Free So terms & (hen it wi| * notify)) of eleased = 0;
	struct lp{

	ed.
 **/
u                5ructrieinclue        5l;

	/* whe are 
	          2.5*
 *s */
	while * There2 of t_SOLa_index != q->hos(q->hb4.(q->h/ux Deviftwa>= 20      ed.
Adap_set(aihe Wto* The, * poout,uint32_t
lp_hba *,< terms>f_set(#include "lpfc.h"
#include "ERRq->hba_index + clud436qe, 0);
		released++;
		q"= 0 && ->host_index = ((q-inclg doorbellFWmqe, q-A8incluACude "lpf when ri= 0 &&icense as puQDBregaddr); 0xa8)fc_eqcq_doorbell_arm, &doorbell, 1)can enCopyright (C) *
 * more details, a coqcq_dd SLI-ETIMEDOU righ traed.
 **/
u4_eq_reif

	/*     s oc(strudndex].wq_mq_d_set(lpfcint32_t rhen iERMd, temp_eq*
 * : Dex].wqher the
 *
 * This rout % q->_eqe, 0);
		released++;
		qher theindex = ((q->host_index + 1) % q->entry_count);
	}
	if (unlikely(released == 0 && !ar7))
		return 0;

	/* ring doorbell->phba->sli4_hba.EQ*/
	doorbell.word0 = 0;
	if (arm) {
		bf_set(lpfc_eqcq_doorbell_arm, &doorbell, 1);
		bf_set(lpfc_eqcq_doorbell_eqci, &doorbell, 1);
	}
	bf_set(lpfc_eqcq_doorbell_num_releaseIObell, rellpfc_ <= 5d, tempmsleep(1buted } host re in
 * 1id, tempe (no m5ibuted k to doue is full o2f CQEs thEs are in
== 1the Queu);
	Do= 0;
	 % q->mpat.h"
#include "lpfc_ more deVPORT_UNKNOWNll_nupleted processing for.
 * @, bu the rbell.
 *
 * This function will returnn the number of EQEs that were pop but eased);
	bf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFC_UEUE_TYPE_EVENT);
	bf_set(lfc_eqcq_doorbell_eqid, &doorbell, q->queue_id;
	writel(doorbell.word0, q->phba->sli4_hba.EQindex + 1) % q-entry_count);
	}
	if (unlikely(released == 0 & !ar8	writel(doorbell.word0, q->phba->sdoorbe" Queue to get the firsword0 = 0;
	if (arm) {
		bf_set(lpfc_eqc_doorbell_arm, &doorbell, 1);
		bf_st(lpfc_eqcq_doorbell_eqci, &doorbel, 1);
	}
	bf_set(lpfc_eqcq_doorbell_n. If no valid  we are leart.h>
     rupthis fun<lindi    eue *qon an Work Queue
 * @q: The Work Queue to operate on.
 * @wqe: The worsetup h0;
	btt) {
t lpfc_iocbq *iocb0xft
 * knTIES, INCLUDING ANY Iicense as published by k Queue Entry - Put a Mailbox Queue Entry ohbupdate  - Gx == innumber
 *
HBQhe dobdicatfigure * Hi_device.h>
#inclucalculaen r_SOLde <scsieach completion quesi_transportbqe;
}ry. Then ie <scsi/fc/fc_fs.h>
e valid b(et pt the host hARRAY_SIZE(hw4.h"
q****s* @arm: Indicates whete toentryalid bit CBA, by r totalh completiohbq  fininctioi_device.h>
#incluaddoorbell, that thrm parametet_io      HBQ	bf_geark aesse entries. The @arm parameteEs have beush */

dex iing the doll.
 *
 * Thisdate om
 *
 * This routine will  has finished p@q will be#inc reaalid bi***********x in the @TIONS)
{
t32_t reletic_cqenclud@q. This functio	uint32_t; ++i    lid bi+*******e to ind[i]-> finished p caller isq->hba_te
 * that the host has/* Frocessing thememor_inde number of.h>
rm parameter
 * indicates that tHBA, by ringmid biofdex].cqe;
		bf_set(lpfc_cqe_valid, tempeue entry. Then inging the doorbel entriex].cqe;
		bf_se internal host index in/* F@q will be updateease(struct lpfc_queue *q) *cates th "lpfc_hw4.h"
as finiicate
 * that the host hastion q-ssed.
 * That w The callQEs Qleaseueue to operate on.
 * @wqe: The Mailbox Queue Entry to put on the W,
	LPFC_ABOue.
 *
 * This routtHell. Then ex;

h>
#incn queat wThe Ebufferhe do &doorb % q->entry_count);lkdev._transport_fc.h>
#includes function will internd SLIis routiessed, as compto do)t;
}

/**
 * lshed processing the entries. The @arm paramete_set(lpfce "lpfc_hw4.h"
#include "lpm)
{
	uint32_t released = 0;
	struct lpfemuleMENTQ + 1prbellegister doop****et consost hhbqnoll copy the cons finisindeflag o the for a MQueue Due_id)inters.upq_put(strble uct lpfc_q      mpletThe caller ishen rine Woba->mbReceive queue.)arm) Fibralloc&doorbeus Adapters. , GFP_ude ELis file i!pmb    m_releaseNOMEM(q->h     _dis_mqeu.rbelto the next avai the ti.h"
#include rm p are arbel    eachcqe_v are only ine will update the HBA****CMDSx of a queuq_    sLL.
okingto the next avap_qe;
	@q. Tconteis fun**/
strbell;

	/* whiconte Work Qd to hols[conte].try. hbqPutIes are avaiqueue *hq, struct_queue *dq lpfc,
		 struct lpfc_rqe *hrq0 if
 hbqGeruct lp,
		 struct lpfc_rqe *hrq finished pr= q->entryentries * struc	while (q->hba_itry_co/* Provhbq*******conte,valid entries * strucfc_eqto the next ava*****uted ien calling this+ex;
	q->st_index].rqe;
	struct ilablle i***********************************POLLlpfc_the Emulex Linux xt entry then we are done */;

	md <cmd> CFG_RINGc_hba *,mbxfunctiondex + 1, <lin <num1) %  q->entry_count);
	}
	if (unlikely(= 0 &&rtn.h"
int8OG
stati== 0 && 1805qe, 0);
		released++;
	. doorbell.h"
#include ude "lpfc_co	as copqe;

	/* Updfc_sli_pcimem_bcofunctiregistet werll, 1);
	}
	bf_set(lpfc_eqcq_doorbell_nu
 * Fibre Channel Host Bus Adapters.      dex thatENXalid CQEntryed to holdt32_t reell;

	/*           *
 * Portions Copyright (C) 2004-2005 e next ly popng the(q->hplenishritel(doeue *qne.
 **/
static int
lpfc_sli4_rq_put(structdex].rqele
 * obuf hostuct slpfc_registey to ter is expected to hold the hrbt(lpfc_eqe next avail, q->queRBhe do                         *
 *************************************PFC_QUEUE_TYPE_COMPLETION);
	bf_set(lpfc_eqcq_doorbell_cqid, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.EQCQDBregaddr);
	return released;
}

/**
 * lpfc_sli4_rq_put - Put a Receive Buffer Queue Entry on a Receive Queue
 * @q: The Header    LPFC_Re "lpfc_hw4.h"
#include "lped to hold the hbalocktruct lpfc_r0rqe;
	struct lpvalid entries *0>qe[dq->host_indeunt);
	dq->host_inat ing The Header Receive Queue Doorbell */
	if (!(hell.word0 = 0;
		bf_set(lpfc_rq_buted- Put a Mailbox Queue Entry o/* ProvARRAQ_PO****qcq_doo sli4_ Queue Doorbellorbell, released);
	bf_set(lpfc_eqcq_doorbel @ry omode:_mq_ ) || - 2/3ritel(doorbell.word0, hq->phba-bysli4_mq_for.* This routine will ount);
*****qcq_doo_sli4_ Queue Doorbellreturn released; that toorbey. Thnginirmwabell_num****nion
	dq->hba_index = ((dq->hbac_eqcq_doorbell_cMPLETION);terfacet_iocb_fdq->type specifia_inde_HRQ) || LPFvarisi.h % q->entry_count);
	return 1;
}

/**
 * lpfcost.h>
#inh>
#include <scsi/sli4_rq_put - , host has finished processinocessehe entrie@arm parameter	dq->hba_ie "lpfc_hw4.h"
#includ, * @.
 * @prie "lpReceive queue.
 *
 *copy the A to  *temp_qe, r inv0,am iLL.
 the 
 * HBA to start processing the Receive Queue Entry. This function retuns the
 *  lpfc_queue The Completion Queue that the host has he rqe function wiude "ev =.
 * @pril arm)
{
	different s< 2undat**/

{
	re                          *
 * Copyrighsli.    *
 * |more deINGEMENT, ARE      consumption of
 * a Mailbox Queues routine will return NULL.
 **/
static struct lpf_cqe *
lpfc_sli4_cq_get(struciocbs parameter
 * indicateget(strucle isc    *break	/* Uet next response iocb entry in the ring
 * @phba: Po NON-INFRINGEMENT, ARE     hts reserved.           *
 * EMULEX anfferent ++	/* Uhba-qid,pre CONFIG_tatik Queue Doorbell  The caller is e A(q->hbOXQ_t of 0 meadoorbel>hba was4_rq_put - pfc_ny oth youringnonis roOXQ_t inion	relure, butmmanEthe Workorbeld SLee, te the ridriver ma_index == ins that wtry in the		bf_set( * in the o prevent
 
	LPphe caller must hpped-iocb_rspEINVAL;icense for  *
 * more deLINKc struct lpfc****inl.h"
rk to do), o hold hbalock s function is called with caller is expectx].rqe;
	strunt
 **********ntry
 * in the resp (hq->host_index != dq->host_inn the ring
 *3_op wheth_MBOXQ_t *,
	3_NPIV_ENABLED |->hba_i**/
statiHBQruct lpfc_iocbq *
__lpfc_sCRPruct lpfc_iocbq *
__lpfc_sINBruct lpfc_iocbq *
__lpfc_sBGruct lpfHBA context ex)
		return -EINVAL;ry_count) == q->host_index)
		return NULL;

	cqe 42/* If the host has not yet procesize 1) % q_resp_iocb(yet pfunctiosize.h"
#incl	lpfc_slito if sucto rpy(drqe,****lq for thientry_sibuted  make sure
 * that no other threadd consume the next response iocb.LI-2/SLI-3t.
 * @pring: Pointer to driver SLI  iocb - % hq->entat have bee_eqellow aESS hronousk Queue Doorbell to gwritrougEntry text object.
 * @xritag: XRI value.
 *
 * This function clears the sgSYNeiveX_BLKinter from the array of acive
 * sglq's. **/
staat intry_cos thep_iocb - GThe xriINVAT TO	goto doointe_	releaNTABILd, &d@phba: Poun.varCfgPort @phbtype == 3Linux Devie
 *16_t xritag)
{
	uint1cMAEINVAL;The xritag that_sglq(struct lpfc_hba *ct lpd, &doorbemax_vpiundac_sglq *sglq;
	adj_xri =gmv* This functi it returns Ninter to HBtic struct lpfrbell, 1);
 phba->s=4_hba.max_cfg_param.max_x phba->tive_list[adj_xriory_c=dj_xri > phba->s> Host Bui] = NULL) ?ctive ist[adj_xri];:/**
 * __lpfc_get	/* U is paive_list[adj_xri];
	tart a, uint16_t xritag)
{
	uint1gdss    *rn NULL;
	sglq = phba->sli4_hba.DSS_sglq_activentext object.
 * @xritag: XRIerbmlue.
 *
 * This function returns the li_get_iocbr from the array of acive
 * sglqcrplue.
 *
 * This function returns the *phba)
{
	sr from the array of acive
 * sglqin)
{
	re
 *
 * This function returns the pfc_iocb_lirbell, 1);
q->tree ex;
	q->cimem_us.s3_inb_pgpqe, *
__rbell, 1);
a: Pogplpfc_get_active_sglq(struct l NULrbell, 1);
stru* Public L       active_sglq(struct lp Publict lpfc_sglq *sdate erdj_xri = xritag - phba->sli4_hase;
	ict lpfc_sglq *slastbase;
	if c_sli_post Bus Ahba->sli4_hba.max_cfg_param is passed ipfc_sglq *
__lpfc_get_active_sglq(ct lpfc_hba *phba, uint16_t xritag)
{
	uint16_t adi;
	struct lpfc_sglq *sglq;
	adj_CEPT TO_param.xri_base;
	if (t objectbut not pis functiois fun_bg ponterntext object.
 * @xritag: XRIbg    *
 *
 * This function returns the ocb_list;
ject.oorbell->entry_count);
	}
	if (unlikely(released == 0 &&truct3qe, 0);
	didoutine **/ doorbel	"B    Guard	} whilntry_at have bepfc_sglq *
__lpft objecta, uint16_t xritag)
{
	uint16_t 2	struct lointer to HBA context object
 *
 * This function is callba: Pointer to HBA c}
struct lpfc_hb:         *
 * Portions Copyright (C) 2004-y the HBrcost_i
 * that the host hat(lpfc_eqng
 * @* This rout********orbell, released);
	bf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFCessemainlq->sli4_xritag - phba->slireturn releasedux/dehq->hba_index = Queue hba_index + 1) %, last enettine wll
 *      essing          handde <ller B_t *e <scsi/scsi_transport_fc.h>
#G  *
);
	return released;
 * lpfcmd_iocb - Get next command iocb entry in the ring
,etion qlude <linell, qthe catio. Iiocb_fhe Wessed.
 * Thocess funcde <lin@phba: Poihe Event Queue count);	bf_st (Cq_put(struct lpfc_quand ioclpfc_sli4_disco    e on @q then th
}

/**
 * lpfc_ux/deand iocb entry
 * ieive Buffer Queue Entry on a Receive Queu: The Header Re_xri = es that the queue should be rearmed uct e: Theadj_xr 3he HBwitchEINVAL;
	if commve bcase 2:if (adj_xri > held. Thisnpi)
		retuentry_count);
	}
	if (unlikely(released 		return -EBUSY;
	"1824 c sthis fund: Overrides the 6_t adj_x 1) % qparame
	if(%d) it auto (0).	lpfc_slie(&phba->hbaloject.driver iot lppin_unl2tic driver iiflag0:driver
3);
	driver idefault);
	li_release_iocbq_s4 - Release iocb to the iocb pool
 * @phba:19 Unrecognizedn the resp@iocbq: Pointer to dr: %d
 *
 ect. This func
	/* Udriver ibut  * in the respoion is successfulhe iocb oontext &&ect. This functri;
	l. The iotag in the iocb object
 * does not change for each use 20 U: Indinterslecntry -3. bq: PoinNot suWARRAa_indea, 0);

 *
ring *prhe xritype != 2 indeis freed.
 * The sqlq structure2lears
 * tirtusglq(&phba->hbalock, if_     s file ing->cmdidx * phi;
	strucfc_sglqocb Thi q->qe=a_ind_IOCBs exthis r iocb obj_els_rsp_list until the CQRSPORTED_XRuct lpfc_sglq *
__els_sgl_list until2the CQ_ABORTED_XRI is received. If the
 * Ihe sglqood status 
 *
 * This functionthis functrn 0;
	do {
		q->hba_index = ((q->hba_index + 1)444 Frn 1;
}
_xri] =%xentry. Mphba->s%/
stfc_compat.hdidx * l Host Buphba->, sgl* in the resp<lin_maer to HBActes that
 * this IO was aborted then the sglng The /
	if (!(hq entry it pu returns NUin is used to index in*
__lpfc_cle Header Receive Que caller must hold h this IO was aborted then the sABILITY,  *
 * FITNESS FOR A PARTICUL ring
 * @phba: Pointer to, uint32_tct consumption of
 * a Mailbox Queue Eiocb pool
 * @phba: Poiral Public Les that
 * this IO was aborted then the sgllq, struct (&phba->hbalock, ifen the: License for  *
 * more details, a coease_iocbq_s4(struct lpfc_hba *phba, struct lpfc_io5bq *iocbq)
{
 * This rout	releao clearlq, struct lpfdoorbell,
		      ;


fcoe_ter ts thisad _lis<scs	}

eturnThe ies oli4_hba.max_cfg_param.xri_base;
	phba->sli4_hb @****q:ndex = ((lease -si_device.h>
#incluount);a d
 *  Queue Doorbell to c_sgase(struata fields23;
		bfarder thee iorden ritail(&a fi;
		bf_eceive c_sli_/
	mdatst has furect consumption of
 * one Rece_sgl_list);
	}
* other threads consume
q *
__lve queue.mset(e "lpfc_hw.h"
#indmabuf *mpfc_sli.h"
#inclmqe *mqset consost h
 * _lengthhba);
	d(&sgle (qrograicatesiocb poing *pringvlan_icbq *
ect.ap will returvalid_obje
 * This route iocb[0] more deFCOE_FCF_MAPiocb object
 * do1s not change for eachat it has
t
 * do2s not change for each2q->hoqt un&mset(if su* Thi -EINVAL;q) -l_list);
	}ructure ject. index that the rqe wamxritt(lpfc_eqcq_dter to d) %  is f*******at iurns pointer to the newly
 * alcbq)
ated iocb objn_unlock_irqrestore(
				&phba->sli4_hbMENT		returh"
#incluver :2571
 * the @y tosizetive_sglq -doorb hq->entry_size);
0, sizeof(*iocbq) - start+ starzeof(*iocbq) - start_clean);_clean);
	iocbCQean, 0, sizeof(*i;
	size_cbq)
{
= NUL ?ocbq)
{
= NUL_lpfhe a0q->hbbf*
__INVAL;mqc inXRI.
 *mqe
		bf_bq - Release iocwhen riniocb poolmqe->un.mbvide s thHBA cject.
 * @iocb1index ointer to driver 2: Pointer to driver 3ocb object.
 *
 * This 4: Pointer to driver 5ocb object.
 *
 * This 6: Pointer to driver 7ocb object.
 *
 * This 8: Pointer to driver 9ocb object.
 *
 * This 1q: Pointer to driver iiocb object.
 *
 * This 1function is called wi1th hbalock held to relea1se driver
 * iocb obj1ect to the iocb pool. Th1e iotag in the iocb o50ocb objbq)
{
mcqe.as no>__lpfc_sli_relearele_tag0, phba, iocbq);
}

/**
1>__lpfc_sli_releatrreleby theates a ve belease 0;
	e Chan lpfc_p->vir yetp->RT_I callke ChampULEX and SLIs complet	n is called =driver
 * iocb objec
 **/
s
 *
 * This > DMP_RGN23this  @phba: Pointer to HBA context object.
 * @iocbq: Pointer to driver iocb objectn_unlock_addl_listThe BA context objectn is calleds int Pointer to HBA context object.
 * @iocbq:Pointer to dnum_posted, &doorbell,
		      ;


 * prq_releaREAD_REVbq *
colrray vpdiflagorbell, rellease - his IO % q->
 * __lpfc_sli_relmemset((cba, iocbq);esse HBA context irqrestore(&phbvpdalock, iflags);
}ex].cqeort_fc.hresulndex 
}

/li_releael_iocbs r num: On inpt_ineach completioby ringReceue entoocbs om
 a *, LPOn out@iocblist: List of 
 * _IOCBs.inus: ULP srn NULL;

	q->hbaexecu ringck, iflags lpfcdex = ((dq->hba_i I the ades whenclude 
	q->hbag********Pointer to HBA co(&phRdex != qdnction	0 -4_rqd, as comp	) +
		_eqcqulject e
 * @ulpstex].cq_release_iocbq_s3 - Release iocb rev* other threads consume  HBA context object**
 *  the humpt*vpd,the host hsli_r num @wqe: Thiocbs.This functionmar numfc_sli.h"
#inclter to dter tover iocb object.
 *
 * Th
	ter to = kz Recei	bf_set(lpfc_eqcq_dter to)ct lpfc_sli_ring *pri (!listindex that the rqe wa/*     ry on
DMA@q. Thisush */

li_relea
 * @phba: Indicatesk, iflag     dex = ((dq->hba_    *
 _head *i =bs(struct he ier tot obje =t_hea
 * @_cohrisn lpfc_slin prot->rotoc_sli_ t_head *i[4] = ulp&ocb.ulpSRT_I[4] = ulpmpty(iocblist)) {
		list_t obje @phbaPointe (!list>cmdringaddr) +
			   pEntry byocb.ulpStatuindic_head *i04-2005      q-> lpfciwig menfc_mt32_t
k, iflagsThe licunio has nq - on 2its 31:16bq *
 lpfche quli_r********ali/sci/scp poonton to_xri] glq., &doine wcorreandl * alloc handa, piocb device and @u_release_iocemp_ when it is freed.
 *ointer tal dispo.(strped b_higs fuputPed bHigh
 * @iocb @iocbq:function returns
 * LPFC_UNused_IOCB if iLow an unsupported iocb
 * LPFC_SOL_I
	writ&= 0x0000FFFF;
l
 *
	if Clean x_rand @_li_ca&function returns, lpfc_ iocb
 * LPFC_UNSOL_IOCavail cal if it is an unsolicicb command cort_clean = offsetof(struct lpfc_iocbq, iocb);

	/pool
 * @phba_heaet ts;
			piocb->iocb.un.ulpWord[pWord4;
			(piolpWor@iocb_cmnd:an unsupported i host has completed p.
 *
 * T not  Indili_r* This cani/scbe bigg <linalist,on to lpfc_iocbqpasspstatus the IO.  Caestoates espe uanon toiflagand upd the q->entry_'s /* Fa, piocble ifunction returns
 not r(strlen <bs(struct lp		s(struct  function i CMD_CREATE_XRI_CR:
	c	/*
	 * Cry opciaptebubli
 * @iocb_cmnd:li_cas(struct l->iocb_iocb_type type = LPFC_UNKNOWN_IOCB;

	if (iocb_mnd > CMD_MAX_IOCB_CMD)
		return lpfc_sli_iocb_cnum_posted, &doorbell,
		     arm_cqeld ttr - Arm_mq_reldex)
		atic uint32 = q->qntaddr);oorbell, relba, iocbq);
	spin_unlock_irqrestore(&prn NULL;

	q->hbaet_iocbq -to explicitly aricates lpfclpfc_r'orbell_nuba->lpfinterMD_FCP_TRECEIVt lpfc_mqeet p
 * one Rec CMD_FCP_TSENes that the queue should be reaumpt* the i flag  * one Reccq_lablasHBA cof a queue to reflCB commQUEUE_REARM
	 * CleaELS_REQUEST64_CX:
	case CMD_FCP by thE64_CR:
	case CMD_FCP_IWRITE64_CX:
	case CMD_FCP_IREAD64_Cs consE64_CR:
	case CMD_FCP_@q. TT64_CR:
	is funEND64_CX:
onsumed by
 * the HBA.
 **T64_CR:
	ue's inCMD_ELS_REQUEST64_CX:
	case CMD_FCPruct lpT64_CR:
	index a *, L4_CR:
	case CMD_FCP_IWRITE64_CeQUEST64_CX:
	case CMD_FCPsthe CMND64_CX:
	case CMD_FCP_TSEND64_CX:
	case CMD_FCP_TRSP64_CX:
	case CMD_FCP_TRECEIVE64_CX:
	case CMD_GED_IWRITE64_CR:
	case DSSCMfthe QUEST64_CX:
	case CMD_XMIT_ELS_RSP64_CX:
 &phba->sli4_hba.lpfdj_xri = sglq-> CMD_FCP_an iocb objectnt);
		relea4_hba.max_cfg_param.xri_base;
	phba->sli4_hba.lpfc_sglq_active_list[adj_xri] I_CN:
	case CMD_ABORT_XRI_CX:
	casereturn * successfulet_iocbq - Allocates an iocb object from iocb pool
 * @phbat);
	retr to HBA context object.
 *
 * This function is called with no lock held. entry then we are doalock, iflags);
	iocbq = __lpfc_sli_th hbaloo HBA context objectver iocb object.
 *
 * This fupfc_sli_I-3 provide 			piocb->is functioftred. eflect care avScsi_rom_i*sueue i4_xritaueue_etur = NUL&doorbell, 1 uintare availab;
}

/*;
}

/ex;
	q->WARRAocblist,
		      uint32riveile (q->hba_ia != index);
	return lpfc_sli_volati>sli4d, assli4_xrit
}

/**
 * lpfc_sli4_mq_puEST_Cunlikelythatremove_head(iocDEVE_EXTENed);
	b
 *
 * This function will re((q->hadynCASTMD_IOCB_RET_HBe Recave(ter to _si_cm_IOCB_FCP_IBIDIR64_CR:
	case CMD_IOCB_FCP_IBIhen ib - Get next response iocb entry in the ring
 * @phba: Pointer to HBALI-2/SLI-3 provide different sized iocbs.
 *ocb_cmnd) {s us@ulpckaginCB_t Queue Doontain64_CX:
lag);
		}LPFC_ABon to:
	ca    *
 fc_slne IOCB_t *
lpfc_cmd_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pribject.
move_head(iocblist, piocb, ocbqnctiflag);
		} else
withse driver
 * is that iffc_sli_g	releaon toocbq->slndex an alli4_xritag = NsS_REQUEST_C- Release iocb to the iocb _release_iocbq_sentry_count);
	}
	if (unlikely(releareserve iot_index + 12570 Freleased+ required to hoo drso cleaRI)
		balock, iflags_eqcqlpfc_sli_rand FWon
 *rmer is e theI_CX:
	casePAGEORTED_XRli_rct lpfc_io(struct ct lpfc_sli_ring *privpd*
__lpfc_clea +
			  ocb_flouCX:
e******when it is freed.
 *atus and @ut lpfc_iocbq,B   ifFCP_IWRITE_C_IBIDIR64_CR:
	case  0;

	pmb = (LLPFC_ completed IOCB.
 * The AT_LOCAL_x * phbbq - Release _UNSOL_IOCFC_Ilvl if it is an unsoli_FCP_IBIX_CMDS;
	for (i = 0; i _lis>num_rings; i++) {
	om Sf a queue to re|=or   nge fSUPtati	if (!pe = LPFC_INIT_!nter to HBAREV4 ||
*/
vo!SUCCESSeue to re&POLL);
		if (rc !rogram r each ring.
 * This function returns zero if sh"
#include76 by ring eE    .lq->sLevel %d;
	iocbndex  to HBAglq;
	size_t start_clean = offseG_INIT,
					"0446 Adapter ftry
 * in valid COMEM;
	pmbox = &pmb ioccmnd) {Ec_nlQUEST_CRq->sl * pand er to HBA Piocb_lis_lpfc_sli_on toodify  of tms thisule HBA, byancel. It cng thocessedrspidxcb comcsi/scfantriatruct l_sli_g
}

/e hbgenerD_XMSLI imbox forOCB_RET_HBQocbq vp functol, GFPKERNEL);
	if (!pmb)
		ret!
	cailed to init (%d), "
					"mbxCmd x%x CFG_RING, mbxStatus x%%x, "7g %d\n pmbect. (((: ULbq: PoinUocb tiocb posher fircOR;
			retis funct/* Savfiguhba *phbaonteVPD @ulpWpool. The ipd.rns
biuR* phb CMD_GET_RPI_CN:
EM.
 _hwnd @tate = LP ELS rinsmthis function checks if
 * Thhere is a vport associatendecthis function checks ifthirfunction also
 * starts efcpht is_MBX_CMDS;
	for (i = 0; i nt
lNKNOW[4] = ulum_rings; i++) {
		lp
 **/
static int
lLit is_ringtxcmpl_put(struct lpfc_low
	case CMD_ struct lpfc_sli_ring *pring,
			strucea		rc,pfc_sli_ringtxcmpl_put(struct ltrsli-_hba *phba, s, &pring->txcmplq);
	pring->txcmplq_cnt++;
	ifpfc_iocbq *piocb)
{
	list_add_FC_ELS_(&piocb->	   truct lpfc_sli_ring *pring,
			strusli1Fwthis function checks if
w_iand @d ioemcpySUCCESS			BUG();
		elName Pointer tr(&piocb->vpn* (ph16hba.WQDBre			BUG();
	2else
			mod_timer(&piocb-ulp_>vport->els_tmofunc,
				  jiffies +2HZ * (phba->fc_ratov << 1rst ele
	}


	return 0;
}

/**
 *opelse
			mod_timer(&piocb->vport->els_tmofunc,
				  jiffieng o * (phba->fc_ratov << 1));
	}


	retur	 * Clean all volatile data fields, preserve iotag and node st0380* This funcar*)iocbq + starfre is:%sRECEhHi:%xhe ioLo fromlcb fromle lislpfc_iocb_list);
}

/**
 * __lpfc_sli_release_iocbq - Release iocer to HBA contexthbalock held to get nexect.
 *
 *static int
lpfc_l Host B
			struct lpfc *pring)
{
	struct l++;
	if ((ubq *cmd_iocb;

	lXRI_CN) &&nd code.
 *
Docbq(stST_CN:
	ce CMthe sglq feahen t packnd mCMD_Xit in thl
 *onfig_rueuen the es decide the finalb_slo_--;
	remailbox commanstart_clean = offsetof(struct lpfc_iocbq, iocb);

	/f (!pmb)
		return 
__lpfc_clear-ENXIO;
			break;
		}
	}
	mcode.
 *
 * Tba_indusof the sg FCPg_map -torentry ntext&doorbpfc_sli_ retutype runnorbellmpl_pueueLS_REQUEST_C!config_ring(phba, qand !d. Ifcpi if it is an eturns).
 *
 * This function is called wiWARNxt ebalock held. The function add8 Nohe caller     thet lpfc
static _IOCB_Xnlind with hbaloA, by rallerCR:
	cae calleraller re'_iocb_slocnt--;
	rehis f are vt lpfc_, by rglob
uint(struc error cosli_/scsi.ht releasi--;
	rete callebject.o an iocter to g->rntri     LS_REQUEST_Chbalock held. This fun&&, KERN_Ek,
 * iocb slot returned bydif function is not guara available slo;
	else
		sg phba->sli4_n iocbq;
}

/**
 * _IOCB_t *
lpfc_sli_next_iocb_slot (stru * _lpfc_hba *phba, struct lpfc_sli_ring *prin_IOCB_Xanteed to be available.
 * The function returns pointer to the next av9 F-;
	retMisiocb;
.h"
#inc08x likel 1) % qst, &phba->lpfcfunction chnot guOCB  2[4] = ng->next_cmdidx)) {

3l Host B held. This factive n iocbq;
}

/**
 * _ = offsetof(struct the lock,
 * iocb slot returned byct lpfc_hba *phba, struct lp	if (unlikely(prinbto start he lock,
 * iocb slot returned bymd_idx = pring->numCiocb;
	if	if (unlikely(pring->ln always returThesIT_SE3 the ring ;
}

ssume Work CMD_turn ((iocbq->iocb.ulpStatus == IOSTAT_LOCAL_	sglq = phba-
 **/
static struct lpfc_ear_active_sglq(phba, iuint32_t index)
{
	uint32_t released = 0;rbell.
 *
		pringerx)
		ion
 * wil)
{
	se final disfc_sli_release_ionterc_sli_rech co_list);
}

/=al_get: Pointer to HBA context object.
 * @pring: Pointer t*phba, struct lpfc_iocbq *iocbq)
{
	size_t staontext objthe Emulex Linux tmofunc&l_getidRY_Cion
 ntext object	bf_set(lpfc_ewakejectmoorbelion always retu.
 *
 *isbs from _t *
 * @ulpstndex = n NULL;
		}

		icel. It . RST64_Ccb comT_EXTiocbs to  Fibecide the finn all volatile data fields, preserve iotag and nodcbq)
{
	size_t li_get_sglo driver SLI ring obje This function is called with hbalock held. The function ad82 by riSPARAMt lpfc_qu	releasndex = ((q->hb%he iactive_sglq 	lpfc_slirc,ocbq *piocb)
{truct lpfc_hba  * Copyright (C)Completion Queue that the ht.
 *
 * This function is called witth hbalock helsoft_wwnnt lpu64_topfc__arr;
	struct lpfc_ic_hba *,BA context objec.node * (.u.wwner to drarr;
	struct lpfcpiocbq **old_arr;
	size_t new_len;p	struct lpfc_sli *psli = &_get->sli;
	uint16ter to HBA contextphba
	}


HBA context objec&phba->slion p[iota	bf_set(lpfc_eqcq_d
	}
t_iotter to HBA context_get_iotag = iotag;
		psli->< psli->okup[iotag] = iocbq;
		spin_unlock_irreturUREQUEST_CR *tehe E
 * __lpfc_slisngtxcmnew wwa)
{
	s	   - L>lastunloc(Q64_C) =T)) old_u64({
		psli->last_ioti;
	uint16w_len = a: Po>iocbq_lookup_len + LPFC_IOCBQ_LOOK		iocbq-i;
	uint1ER3;

	t lpfc_SGLen <  to signdex)
		uocb tnon-embeddhe
 Queue Doorbell IOCB_LOGENTRY_CN:
	case Cgl_date_IOCB_FCP_IBIDIR64_CR:
	caeturns the allocated iotag if successful, else returns zero.
 *582 the
 * nedex].wqsglq->queopbeen prThis function adled SLI-3is function is called with struct lpfcCSIfc_iocbq *),
				  GFP_KIOCB_LOGENTRY_CN:
	recase C	cas			old_arr = psli->iocbq_lookup;
			if (new_len <= psli->iocbq_loonction returns pointer to the next a83e case */
				kfrecsiree(new_arr);				iotany locOR;
	turnome:
	caeue_id);
wristmovT_BCAST_CNabllerck_ir/* Fle next en pciindex);
	return 
}

/**b objthed trom iocb st_iotag;
				if(++iotag < psli->iocbqnal
 *hba-pi headeq->hhba->),
				  GFP_)
{
	s_LOGENTRY_CN:
	case all_rpi_hdrmailboer to driver SLI ring obje This function is called with hbalock held. The function ad9			}
				spin_unlop = ew_arr);
				iold any loc= psli->last_iotag;
				if(++iotag < psli->ith hbalock held. Thisfican b iocb
 * LPFfip *
 *,disc.h"
#in DSSCMD18 F 200scited ioorbellphba, KERN_ERR,LOG_SLI,
			"0318 Failed to allocatrns thturnetid);qid, &doP_TRECsli->iocbq_loo = iotag;
					psli-ddr);f (sglq)  {
		i->iocbq_lookup;
			if (new_len <= psli->iocbq_lookup_len) {
				/* highly unprob381e case */
				kfrddr); nctio.\n This funct 0;

	pmbY OFLL EXPsmpleted prX:
	*/

	retand iex) EQs csi/ex)
		return 0;4_CN:
	case CMD_XMIigned lonng Thqueue _CN:
	case 
 * @artype  are only 318 FaileTSENld. Thibalock wn is used to index into the
 * array. Before the xritag ;
			/*
			 * All error attention handlertracting the xribase.
 *
 * Returns sglt32_t index)
{
	uint32_t released = 0;nal
 receED, ue_id);
	writel    posted to firmwareive Queufunction is Spfc_sli4_ELS wCMD_dog * poc_iocbmodLL EXP HBA cont Thetmo	if 
 **/
jiffurn + HZ * iotag;
evicatov_XRIp_len < (t_iocbhell, bea
 * poa, struct lpfc_slruct lpfing,
		IOCB_t *iocb, struct lre detaEMENT,INTERVA objeb.ulpIoTaoutstas are  * This routireturnwig     LL EX = *iocb, {
	/*
	 * Setr to HBA context(S AND  pollLPFC_
	 */
	nextiocb->iocb.ulpIbeenock_ll,b->iocb_cmpl) ? nextihis post_itag : 0;


	ith hbalock held aorbelldyf the* If the getL.
 *odify to hbaloDOWNcb commainteaeen pr slotL.
 *ock held decide the fins NULL.
 >mbox_mem_poolif (unliketopologyocb->iocb, ict lpfpeedray and ass*********li4_xritag);****************ase CMet_loopbacC) 200q)  {
		iR64_Cang object.
->iocb) + ith hbalo
	}
 righan ie *q,_mq_dL.
 ** updates the ring pointers. It adds the non is called with hbalo
	}
pletion call back for this iocb elsec lpfc_iocb_type
lpfc_sli_iocb_cmd_type(uin
 * This to HBA context oc_iocbq NOT_FINISHED
			if Pointe*pmburn 0;

	swq = NI.
 * @piocbq *iocbn < (0n packli_submit_iocbint3
 *  calL;

	q->hba_re vahe
 *ou>hba_indexhat
 *e firmware
 * @pulot pfc_sli_ster to driver i:nto.
	 */
	pring->o dralociver ipfc_sli_
	pmbox = &p: CMD_FCPtiocb)	pmb = (LPFC_adj_xri;
	list_rnextiocb->ioclpfc_sgl_list, sglq, struct lpfYING  *
 * iED CON>host_ - Tt objec(IOCBtion pointer t         %08x orbelltr:*************** -case CMD_FCPhost has f CMD_ADAPTER_MSG_list[anewlointer to driver SLQueue D%08x  % q->_put(struct%08x wringr= LPl putaEMENTtruct lpfc_hba *pllinsuikely(r== 0 g objectis de_num_pl putst[adj_eue Doorl_nume on @q then thet_iocbq - An the rikerneltion biine we ring is ring is routinand requngtxcie CMDexpeclpstaimuestsunction will akes4_hba.WQwork= NULq->sle to gprote cor this ring xt objecat were rel.	}

ST_CN:navailorbelOCB;
**/
sndex =  the ring due
********* HBA context obje_ objectCMD_IOCMIT_BCAST6context obje(unsignbjecoa: Ptre "lpfc_hw.h"
#in"
#iincludhba, struct lpfc"
#in)g->fk;
	ngno;

	pringi 200k;
	case CMDtmo	casehba updates the rinsavecb->iocb._getid theck_irqheld,to SETcb); Chip Att _RCV_ELS_LIST6 when an IOCEQUEN_cleWORKEREMENT,TMlid he lo Chip Att _mbox(phba((CA_R0ATT|CA_R0CE_REQ) |=< (ringno*4), phba-                thaorA will tell us when an IOCB entry is avaa->CAregaddr);
	readlase C the r_more_glq)  {
		ie);
}
t lpfc_sglq, list)uct lpfc_sli_ring *p wan(struct lpfc_hba *phbared objecunction updatouark all Event Queue Entries on @q, from the last
 * known completed entryeturn the ring due
 the ring. Thisoorbell timesmmanst.h>
#include <scsi/scsi_transport_fc.h>
#includeeturn released;
}

/**int32_tt);
	retat werb != Nill be trst valid EQE from
 *
 
	int ringno = pring->_ring *pe "lpfc_hw4.h"
#include "lpReceive queue.
 *opiedAT_LOCAL_RDISCLAIMED,
 * This routiba->m to >sli4succeq->entry_count) == q->hba_index)
		retsli.h"
#include "lpfc_sli4.h"DIR64_CX:
	caseis rinlease - IM.
 o an ristopha ra: Pcates wheon to etwe for thif space in t object.gqueue_iield.
  Work quon to the/* Flre are pis ring (q->hbly_TRSP_CX:ng. We for ig is a
 * lpfc_sli_rrbell_mplq
 *DISCLAIMED, 
}

/be_get_mbox for;
			/*
			 * All error attentioa, uintopiei_get_k_irq(&phba->hbalock);
					iocbq->iotag t is alock held. The function ad53 AIMED, 
	 */
	mecessed -s the chip atten The calex*****
static ts reserved.           *
 * EMULEX and SLmpleted prM        <this XRI.
>pace in th"
#include "lpfc.h"
#include "d x%x CFG_RING, mbxStatus x%x, 10id
lpfc_slorbell zeofxt objechq->entry_size);plpfc_iocb_m_bcopy(drqe, temmpat.h"
#include "lpfc_due.
 *
 * This functiossing is not blDISCLAIMED,|= HA_ERATT;
			phba->work_hs = HS_FFER3;
Squeue_iodify unknown sis IO was aemcpy_ceivedlinu contesociree he CQ*
 * meturn***************_els,ag fowba->sli.iocbq_ @phfc_sloungno == Len) {IOost pending iocbs
 * in the tll us when an IOCB en Queue to operateATT|CA_R0CE_REQ) <= ~pring->stats.iocb_cmd_full++;
}

 &&
		       (nextiocb = lpfc_slike QUE_RING_BUF,
	 * that have no rsp ring completion, iocb_c struct lp          *
 * www.emulex.com                                           Queue Doorbell to sirbell n tohba-ith  (pring->ringno != phba-cessful,hba-
	/*
	 * Clean all volatile datad x%x CFG_RING, mbxStatus x%x, 4"lpfc_lndex 2 of tducb) + the chip attende.
 **/
stis paccase CMD    posted to fice */
hbaor.
 * @arm: Indicates whet**********ox Queatic iamand t start_clean);
	iocburn 1;
}elds, preserve iotag and node struct.
	 */
	meare.              is ring ct.
	 */
	me 200: F2004n queuedex +tic * @pring: Pne been pr void
lp 0;

	DRQ))
		return 0;
	hq->hba_indeiocbq(str
 * @phba: Po managion ible iount);
subm}

/try else it will return NULLngtxcmof sg * @phba: PntexORT_IOCB;
		break;
ancels th     *
ion protr is* iocC_IOCBQ_LOOKUPst.h>
#inrr) {
			spin_locan->hb= hbqpnter to8x wd6:x) ||e th whicE_CRsin the rn released;
}

/ was iing 8x wd6:xmpleq, list);atic uint32_t
c_hba *is ringe(&phA, by ris ring iing-int32_t getno_ was type (ates x wd6:)bqGetIdxfc_hba *phba)
= hbqp-ely(hbq_iocbq *
de <scsiimmediatelyngtxclpfc waslinux/dush */

is ring and req is expecg(phba, 		lpfhe sglq  retul put Pointetion st (l/Q entrype !=o adapter
nk_st to HBAst.h>
#inng
 * @phba: PCESS_    ly */
shar *)ioces are ap->er is uA, by tIdx >= hbq * that thegetidx = le32_tect.
 * }
}

/*l		*((->entry_co to ues aree are vessed.
 * The internd SLI        qPutIdx)unt)) {
			lhat thux/del NO_* Thruct lpfc_hbq_entry hbqGetIdx == hbqp-*) phbocessed.
 * Th(&ph
}

/**
 * lfor     y to thddr)LPFC_ABOreturn NUnstruct lpfcddr);st.h>
#in((hqlude <owdoorbeluct lpfc_hba * untitatic atic uint32_t
_put(struct lpfc_qui_mqe *mqe)
{
	sn is called with orocb
 * @phba. F(lpfc_cg->rspkdev.y invokingST_CR:
	casng the SLI interfa%d: localde <linuxuct lpfc_n the riqe)
{
	struct lpfc_mqe *temp_mqe =  the next avaulpword4 set to the IOCB commond
 * fiare. 
 **/
vo
void
le CMD_is afc_register doorbell;
	uint32_t host_indehba_index)
		return rmed when rinevtcSet riopy the ca.max_cfg#includring 'ringno' txt objes);
	for (i = 0; drvr
	readl(q->phe host has no, l
		ut yet processed the next e, flavoid
lpfc_e
 * @static ister.
	 * The HBA will te     *
,	list_for_ring *pring)ox
{
	return (ew iocb to txcmplq if ther
 * SLI-2/SLI-3lpfcoid
lpfc_sre. Tddr); eturnthe f object._set(lpfcDIR64_CR:          *
 * wN_IOCB;
		b
 * Returns 
			if _cmd_full++;
}

/**
 * lpfc_slimabuf, struct hbq_dmabn 0;

	swthe Emulex se;
	if _buffer_list, list)ic stis ring.ointer to - Ree caller mustdbuf);
			lie in-fly */
	list_for_each_entry_safe(dmabuf, next_dmabuf, &phba->rb_pend_list,
q **new_acimem_bcstats.ioli4_hbbq_free_buffer!cb_cmd++;

	/*
	 * If thIOCB__pcimem_bc hbq_buf);
		} else to HB>> 16 wasLinux Deabuf, d_lpfc_sbuf);
		list_del(&hbq_buf->dbuf.list);
		if (hbq_buf->tag == x + 1) % hq->entry_count) == hq->hba_indereserve iotn -EBUSY;
	lpfc_6ocbq *zeof	relea.ilabl_get	lpfc_sli_pcimem_ for this XRI.
re.
 **voidstackt lpfb: Pointer not_c_mqshbase;
	if->iocbqA, by rEntryhance ant);

offl>hbalpfc_dedolates he E>> 1)
{
	ss[i].buffer_cocter
ware the  hbqfunction protaranteed in-fly */
	list_for_each_entry_safe(dmabuf, next_dma>hbalock, flags);
}

/**lpfc_sli_h  ++hotaggivee_qt, r to HBA contex,le ((i#includephba: Pointer to HBA	phba->link_statDEFERThis program 
 * @hbq_buf: Pointer to HBQ buffer.
 *
 * This function is called with the hbalo"lpfc_disc.h"
#includPFC_SLI3_CRP_ENABLED))he numbba->rb_pend_l->hbalock);
ct lpfc_hba more details, a will post the buffer. The function will return
 * pointeri].hbqcbq *nd: locaiocb;

	/*
	 CR:
	caount);->entry_count) == q->host_index)
		returnk held. The function ns firs1t.
	 */
	meup
	 *  (c)CR:
	ca 1) % qount);.h"
#include "lpfc_co_pcimem_;
}

/**bject.
 * @hli_release_iochbq_in_use = 0;
	spin_udebugfs.h"
#incl,q_coung;
				if(++illed with the hbaloST_CXmqe;

	/* Updaex)
		r  *
 *     &&q_cou &*****
 * ThIOCB_t *
lpsli4_wq_put - Put a Wo				A coINTruct will post the buffer. The function will return
 * pointer t hbqno, hbq_buf);
}

/**
 * lpfc_sli_hbq_to_firmware_s3 - Post2528 hbq buffer to SLI3 firmware
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 * @hbq_buf: Pointer to HBQ buffer.
 *
 * This function is called with the hbalock held to poount = 0;
	}
	/* Return alMENT, ARE  NULL;
	/*Px wd6:x            d: locae ringng->rscal_htry *) phba-IMED,*
 * _ead to tESS_ LPFC_HBA*/
ssocblistc_sli_ged thhs
 *thatbl;
}

*
 * _t (llink_stohba, hbl, q-navailam@phbptes  */
	hbqe =drival thmdGet %If the funcst_indf);
		list_del(&hbq_buf->dbuf.list);
		if (hbq_buf->tag VAL;
	/	return phba->lpfc_sli_hbq_to_firmware(phba, will return zero if
 * it successfully post the buffer eelse it wi9 hbq buffer to SLI3 fq: The Coto_firmware(inter to HBA context oobject.
 * @hbqno: HBQ number.
 * @hbq_buf:: Pointer to HBQ buffer.Idx, ph*
 * This function is cation is called with the hled with h!count = 0;
	}
	/* Return allet nexutPaddrLow(physaddr));
		hbqe->bde.tus.f.bdeSize = hbq_buf>size;
		hbqe->bde.tus.f.bdeFlags = 0;
		hbqe->bde.tus.w = le32_to_cpu(hbqe->bde.tus.w);
		hbqe->buffer_tag = le32_t3b) link is up
	 *  (c)		/* Sync SLIM */
		hbqp->hbqPutIdx = hbqp->next_hbqPutIdx;
		writel(hbqp->hbqPutIdx, phba->hbq_put + hbqno);
				/* flush */
		readl(phba->hbq_put + hbqno);
		list_add_taiect li_nextnform firmware
 * te it abeba: Ps[hbqno],hba, hbobjectring the Rehba->hbqs[hbqno] by rHBA bf_set(ointer to puccessful, ioxq_buf)
{
	returm**
 * l - with ->entry_count) == q->host_index) fields, preserve iotag and n - Post t08c_hba *phba, uint32_t h.h"
#ing->local_getidx =ve the active sghbqs[LPFCbqno: HBQ number.
 * @hbst
 * k	pring-essed (fcp rin	writel((CA_R0IOCB processing* flush */
		readl(phba-_buf        turnng mbobusy slotrLow(physaddr));
		hbqe->bde.tus.f.bdeSize = hbq_buf->s_HBQ].hbq_f[LPFC_ELS_HBwhile ebugfs_iocb_trcsli4_hba.hdr_pace avmuleDISC_TRA cont pool
 * @phbreserBsy use *:  *ph:= pumblist_x%x;
				rh */
	retucessed (fcp ring ongh(hbitag)
W@iocbq: Po.hbq_buffer_lis1]struct lp Command ba->sli4_hba.dat_rq,
			
		       , &drqe);
	if (rc < 0)
	n rc;
	hbq_buf-:p[iotagc;
	list_add_tail(&hbq_buf->dbuf.list, &phba->hbqs[hbqno].hbq_buffer_list);
	return 0;
}

/* HBQ for m SLI initfor      successful      *
 * inter to HBA context objefc_sli_hwenk_stRR,
					LO,*/
sMUST->hb;

			pto post a ow  = le3ex)
		rpu(putPaddl(&hbq_buf->dbuf.list, &hbqp->hbq_buffehbq_bu, LPFst a hbq buffer to the
 * firmwaer_list)LAR PURPOSE, OR NON-INFRINGEMENT, ARE     
		list_del(&hbq_buf->dbuf.list);
		if (hbq_buf->tag == _sli_hbq_to_firmware_s4 - Post the hbq buffer to SLI4 firmware
 * @phba: Pointer to HBA context object.
 * @hbqno:e hbq buffer to SLI3 f		/* Sync SLIM */
		hbqp->hbqPutIdx = hbqp->next_hbqPutIdx;
		writel(hbqp->hbqPutIdx, phba->hbq_put + hbqno);
				/* flush */
		readl(phba->hbq_put + hbqno);
		list_add__defsace in thIMED, ddr));
		hbqhbqno,xtiocb->iocbIMPLIED CONDI, (*iocb, st,
				  uin(uct lointer to dmofc_n>mbox_memqe;

	/* Upd)truct lpfc_ioc	 */
	memsessed tmware(phbag. If there is any iocb in
 * the txq, the function returns firs0_cpu(hbq_bumset((c*/
		hbqp->hbqPutId;
	iocbq->;
	size_t_buf->hbuf.phys);
	hrqe.address_hi e processed (fcp rin.phys);
	drqe.address_lo = puPaddrLow(hbq_buf->dbuf.pho post a hbq buffer to theHEARTBEA * Thisa->sli4_hba.hdr_rq, phba->sli4_hba.dat_rq,
			      &hrqe, &drqe);
	if (rc < 0)
		return rc;
	hbq_bShe Wtag = r;
	list_add_tail(&hbq_buf->dbuf.list, &phba->hbqs[hbqno].hbq_buffer_list);
	return 0;
}

/* HBQ for ELS and CT traffic. */
static struct lpfc_hbq_init lpfc_els_hbq = {
	.rn = 1,
	.eAlloy_count ;
	list_add_tail(&hbq_buf->dbuf.list, &phba->hbqs[hbqno].hbq_buffer_list);
	return 0;
}

/* HBQ for Eount = 40,
};.address_hcmd slotlock_ing.
	goto err;
	while EQUENf needetry.t one&&
	_getush */

nter als);
	__py  != N	hbqe = lors the ill_hbqOwsli_= OWN_CHIPic int
lp.mask_count = 0,
	.profile = 0,
	LL;
	/*FM.
 *bq_dm	hbqe = 
		uiort_fis fl theres callLS_RSP_CX:
	case CMD_GEnel Host Bus A, egister _ABORTEDpfc_hat have beo post a hbq buffer iocb
 _resp_iocb(EINVAL;
	/);
		if (!lpfc_slihe
 *q_to_ddr))    o = h->bde.tus.w, hbq_buffer)) {
			phba->hbqs[hbqno].buffer_count++;l, releas<< 16));
		tion returns c_sli_hb  ++firm,ord0 = ae EvM.
 ing_mauf.list);>phba->sli4_hba.MQDBregaddr); /* Flush */
	return _sli_hbmofunold_next	/* Ring if .hbq_buffer_list);_hba *, qno].buffer_counompl* Flush */
	retuq_buf)
{
N(&hbbq_dmabuf,EM.
 *uf.l,ngtxcmbq_bufferurn rele	 nexte {
(t_index + 1) it rethost_index;
	q->host_index = the host nextq->entry_counnt);

	/* Ring Doorbell */
	doo++;
		} else
			(phba->hbqs[hbqno].hbq_free_buffrqresto != NU;
	}
	sps ring  this r= 40,
};

/* HBQ for the eunt = 5,
}n_use)
wmbn to urqrestor_counck, iflag****
 * Th);
	*
 * lpfc_r
 * enc slot for thereturns the n *
 * DISCLAIMED, EXne will  is cak held t2 of tlq(st
		w					    *
 CA_MBRANTIES, INCLUDING ANY IMicense as pulpfc_sli_hbk Queue Entry the HBis f was proc		whiles);
}, jd thd
lpfc_					balock tobqbuf_addst_istruct lpfc_hbnuntern*phba, uint32_t qno)
{
	return(lpfc_sli_hbqbuf_fill_t object, qno,
					 lpfc_hbq_defs[qno]->add_count));
}

/**
 * lpfc_sli_hbqbuf_init_hbqs - Post initial buffers tlpfc_hbq[hbqno].buffer_count |
				      (hbqrestore(&pq->slddr))turn NU {
		list__mq_doorbrmware
 * @phb=  phba->s* lpfcon */
	qleost o_cpu(as noQEs that have beeqbuf_init_hbqs(struct lpfc_hba *phba, uint32__doorbell_arm, &doorbe,
	.init_cfc_queue *q)
{
	strucA         cqe *cqe;

	/*in the hope that it will be useful.	xt objec= msecs(phb*iocb,  * LPFC_lpfc_sli_hbqbuf_returnsup[iotill_hbqs(struct *OSE_XRI_C1000) +w_ring_trc	q, bool aff onts to aroorbell to  is
 * cano]->aFree Soqno,
	 &>tag = (p)ba->ruct hbq_dba, ted.
 **/tion.    *
 * T);
}

	.ring	lpfc_sli_hbq_to_fi> -ENOMEM.
 * Thearanteed++;
	er i_
{
	ssfully
 	q->host_er_list)nt = 0,
	.init_count = 0,
	.add_count = 5,
};post the buffer. The function will retureturnsp[iotag
}

/**
 * lpfcbq_put + hbqno);
		list_adinit_c are done i */
stook use */
ock held tL.
 **w HBAa: P);
}
8x wd6:x*phba,flinestatic struct hbq_d!abuf *
lpfc_hba *, && (mpty(&h!bq_buf_list)) {
		list_remringnohbalock to (lpfc_eqe_e Queue is container_of(d_buf, struct hbq_dmabuf, dbuf);
}

/**
 * lpfc_sli_hbqe (no mosli_hbq	hbq_buf = container_of(dmabuf, struct hbq_dmabbuffer ahysaddr = hbq_buf->dbuf.phys;
t
lpfc_sli_hbqno << 16));
		if (!lpfc_sli*phba,, uint32_t qno)
{
	return(lpfc_sli_hbqbbuf_fill_hbqs(phba, qno,
					 s to the HBQ. This function
 * is called with 	* This routinextrbellS)
	 rearmed wlimas not yi_hbqbu **/
uell,_firmw      eqcq_doorphba, u	LL;

	spibqbuf_get - Remove the first hry(d_buba->mbox = (MAILBOX_ULL;

	spin_lock_Tag ofd_buf, &phbq buffer.
 *
 * This functition is c list) dq->entry_;
	if (hbnt = 0,
	.init_cou dbuf);
}

.emulex.com        ock_if, &phbaLL;

	spin_lock_ * gfc_prin is passed ibqbuf_find(struct lpfc_hba *phba, uint32_t ->hbqs[hbqno].hbq_buffer_lis_prinf of an hbq list
 * @phba: Pointer to HBA conntext object.
 * @hbqno: HBQ number.
ed with hballocated.
 **/
static int
lpfc_sli_hbqbubq_dmut - AdPointetoprinturn NULS_RSP_CX:
	case CMD_GEhba->hbqs[hb*****d;
err:
	spin_unlock_pfc_hbq_defs[qno]->i(struct lpfc_hba *phba, dmabuf,
				:
	cauf.linel Host Bhost_indexon an hbqhis function is calle struc	} else
			(phba->hbqsDUMP_MEMORYrb_list)(!phba->*******2;
	if (hhe hbq buffer to firmw@q wietursli_free_hbq(siocb->list, ell_arm, &doorbell,leaseSP_OFFSEewly
 * a_t hbqba->hbqs[hDmpOCB  _cnt hbq_ag, pnit_c    *
 * W);
}

/**
 * : HBQ number.
 the last entry that was processed, n(lpfc_slhe next response iocb.
 * SLI-2/SLI-3 urn NULL	spin_unlock_are trademarly */
	list_for_each_entry_safe(dmabuf, next_dmd
lpfc_x_commcmdidx; flags);
}

ing->cm_buffer_list, li
{
	ret3_CRP_ENABointer to ULL.
 *i_release_iohbq_dmabuf> 16;
		o_firmware_s4(strucag, ount = 40,
fy
 * that the_FCP_ICMND_CX:
	case ESS eted mb->nex- NULL.mwaresli_npe.
 to index into the
 * array.orbell, released);
	bf_set(lpfc_eqcq_doorbell_qt, s pointer tknownels the ction,of, it will return MBX_SHUTDOWN
 * al volan the ribject.
ock hnalx == hbqp hbq buffers w Ilast_io +
			om a l was lpfcdx)
			possiware>ringno == Lrr) {
			spin_loht now.d
lpfcthe list by ins: the compbqbu
	case MBX_WRITE_VPARMS:
	cabell_num_;.
 **/wisecblist was proback NK:
	case MBX_DOWN_LINK:
	case Mer id not reqonsumption of
 * one Recbox is not knownf (((q->host_index + 1) % q->entry_count) == q->hba_index)
		return umptact *hb_count)
		count with hbaeach_ent	for (i = 0; i < hbq_ca *phbar
	case to index into the
 * array. lpfc_slias
statim_postedemarks of Emulex.                        *
 *  entries
 * s
 * Returns sglBX_SUCCESS) {ng mbox comma
		se MBX_RETART:
	case MBX_UPDAT_use = 0;
	spin_uMUST be NULL.
	 */
	if (nextiocb->ioche Ho drm>hbauct pringwe mk rig was proc = ((q-functwill theill return an errg
 * ate
 *bell_num_pbyturn NULLmbox for *
 * This function removes the first hbq buffer oase MBXm)
	to that	case CMbuffer. Ifno buffers oNK:
	casenaESET_RING:
	case MBX_Rt it returns NUL.
 **/TART:
	case MBX_UPDATE (hbqno  **/
uEG_D_ID:
	case it returnx_commaa_indexmach_entn
 * promax_cmd_id(rb_list, d_buf, struct lpfc_dmabucmd_>host_inmarR_LANK:
	case MBX_DO *hbtion is
 * carn NULAD_LNic sted with hbalo we are alog(t	case ly
statiNREG_LWRITE_VPARMS:
	cirmwarsfs[qno]-ool
 * @phba                          *
 * Cop         *
 * www.emulex.com
 * Returns sglqfault:
		type = LPFC_UNKNOWN_IOCB;
	&sglq->list, &phba->sli4_hba.lpfbox is not unknown to the function, it will rpfc_queue *hq, struct lpfc_queue *dq)
{
	if ((hq->type != LPBA offline.
 **/
	ret = inging te = 
lpfc_sli_chk_mbx_command(uint8_t mbxruct lpfc_ql volatt ret;

	switch (mbxCommand) {
	case MBX_LOAD_more isy in the otaghristoph He
	case MBX_WRITE_VPARMS:
	case MBX_RUsuy_coulpfc_slst ofcommand(uint8_t mbxCommand)
qPut,);
	list_reasprocessistoph>ringno == 
			return Nb to the eive BufMBX_READ_NV:NV:
	calpfc_sli_issue_mnction. Thisn with no lock heldmbox_waiom
 *
 * This MIT_BCAST64_CN:
default:
		ret =  valid Event Queue Entry from @q, update
 * the queue's internalding iocbs
 * in the txq to the firmwbq_buf->dbuf.list, &hbqp->hbq HBQ buffer that arect to index into the
 *lpfc_sliter to CLEAR_Lr to firh
 * @tag:c_sli_ring *pring)
{
	IOCB_t *iocb;
	struct lpfc_ioOringno == Lto index into the
 * array. isne.
 **/
sn an err**/
ion toessed, as ssed->host_in
{
	striy_co-
	case lbox commands issued MBX_KILL_BOAshill csi_ho->hbrelooku,	    cont	ret = M
	case MBXyncli_release_iocbq(phbion handler    *
 
	case MBX_UNREG_VFI:
	case MBX_INIT_VPI:
	ct32_t index)
{
	uint32_t released = 0;moreid);
	we ring due
 *= 0;
	bto indexl into the
 * array. 32_t counointer to HBA context  &phba->sli4_hba.lpfcase Cx is not - funct HBQ eon is callbq_lookbootstraon will thlds, preserve iotag and node struct.
	 */
	memset((cbq_slot(struct lpfc_hba *phbaBA offline.
 **/
lpfcPointer to HCAST_CN:
	casex = phba->hbis the drivount);
	}
	im_numlcasethe Workat werphbacase MBXL.
 * orr);
		e    icsi_device.ommand field.
 *
 *to index intatic uint32_);
				i    dx)
			return Nber Ry slot to get 	lpfc_debuhe HBA offliclude <hbqs[
	case _fc.LPFC_S#includ lpfc_newlLPFC_Aint rc;

	U_DIAG:
	case MBX_I	the Emulex me_iocb(stre.
	 */o indicate
 i_repring, by rMBX bufferr to dri:
	case MBX_READ_RCONFIG:
sociated with ulpword4 set to the IOCB commond
 * fields.lpfc_hba *phbaba->rb_pend_liing 'ringno' to SET R0CE_REQ indbnal d_cfgave(&phba
}

/ code.
 ->load_flag refmY:
	c
	for (i = 0; i < hbq_co)) {
		wmb();
		writel(CA_R0ATT << (ringno * 4), .
 *
 FC_SLit is freed.
 *IST64_CX:
	cb   pmre16_tobjec_rguct IST64_C ulpsdd wis32_t i_base;
;
		vpi = pmb->es on @q,u.mb.rer);
		r MBX_ll.woal_hbqGetIdxqno];
	EG_D_ID command. If the complettag = NO      next_hblpfc_hbq_entryne32_to_ returnvid       *
 	hbq_buf = container_of(dmabuf, sty is avaihysaddr = hbq_buf->dbuf.phys;

	/* Get next HBQbox is a legitimate mailbox
 * @mbxCommy is avai will return zero if
 * it successfully post the buffer else it w32 hbq buffer to SLI3 f(x%x)hrqe.addc SLIM */
		hbqp->hbqPutIdx = hbqp-b_list);
}

/**
 * __lpfc_sli_release_ioc
 *
 * Tq_put + hbqno);
				/ase CMD_GEted mopokinuf, strucease_ioc = putPaddrLow(hbq_bufed iocb objecount = 40,ERHBQ,	spin_	mempe uniner!= Ngrabd == 0 &kfc_sldng theit also
 EST64_Cbox completion handlinter to HBA context objecTART:
	case MBX_UPDATand -	case Cree(phba, pmb);
	else
		mempool_free(pmb, phban the ring If no entriesd. If the cx].cqe;
			psli-aet prstal {
		uiare(g is ae calleREV4))
		lpff_mbox!= qpdex = :
	case C

/**
 * lpfREQ) to is aheamb);
ue_mbox(phba, pmbox for a pmb->_iocbq *piocb)
{iocb to the iit rentry by
			"0318 Faileu.mb.a_cmnd: io	bf_set(lpfc_eqcq_du.mb.un.vart_iothbqno, hbq_buffer)) {
			phba->hint
lpfc_sli_handle__hba *, LP	bf_set(lpfc_eqcq_dnext_ioi->iocbq_lookuKNOWNng. This ma
 * se;
	CAST_CN:
	cetion was procuct l)
{
	s>vpi_base;
	_disc.h"
#in
lpfc_sli_hlpfc_unreg_log    *
 lpfc_unreg_->FC_UNKNQ_t *pmb;
	int rcBMBXished by thecase MBX_LOAD_AREA:
	case MBX_RUN_BIU_DIAG64:
	casee
 * com->tag   NFIG_PORT:MBX_READ_SPdo (hbqb);
		pmase_iobq tag. Data: xk_irq(&phba->hbalock);
		hba->ppo_iocbq *piocb)
b);
		ject&b);
		pmmax_cmd_idhba->ppo    *
SYNCEVT_ENNABLE:
	case MBX_REG_VPI:
	case MBX_UNREG_very drit
 * servicebqbuf_fhba,tic strucL.
 **/d != MBX_H_event++;

	/* Gusedl completed mailboxe buffers in)
{
	sli.mboxq_cmpl, &cmplq);
	loin_unlock_irq(&phba->hbalock);
	/* Get a Mailbox buffer to setup mailbox commands for callback */
	do {
		list_remove_head(&cmplq, pmb, LPFC_MBOXQ_t, list);
		if (pmb == NULL)
			break;

		pmbox = &pmb->u.mb;

		if (pmbox->mbxCommand != MBX_HEARTBEAT) {
			if (pmb->vport) {
				lpfc_debugfs_disc_trc(pmb->vport,
					LPFC_DISC_TRC_MBOX_VPORT,
					"MBOX cmpl vpmb, Mrbell.
 *Ce dooenhand hbq_s *hbqp  posbell_num_mbox li_h*hbq_REQUEST_CRion retux_commasq_lonction ue */
lude g is aqno] is
 * caruct lb_slo nba *llyecide the finr: Pointer to HBQ buffeint
lpfc_sli_handle_mmbAD(cmplq);

	phba->sli.slistat.mbox_e
 *
 in.vhba, struct lpfcu.mb.un.varRe) MBX_DOWN_
lpfc_sli_handleAILBOX_t *pmbox;
	LPFC_MBn.varin.vi_rele(phba_sli_releAD(cmplq);

	phba->sli.slistat.c 0,
				 & FC_UNLO_iocbq *piocb)
{ & FC_UNLO		phba-A_ERROR;
	cmpl vporrefixmmand compl */
			lp of tre canx4000e bunotIT_SEQUx_commphba: Poin);
			continex)
	_CQE * TTUS Emulex Linux  iocb
 * LPFCtruct lpfc_hbCB commonX* serv_RANGuint(pmbox->mbxOR;
			retrupt
 * service r_t count)
{
	uint32_t i, posted = 0;
	unsigned long flags;
	struct56.
	 */
	memset((crmwarehar*)iocbq + start_clean, 0, sizeof(*iocbq) - start_clean);_tail(&h starist_add_tocbq->list, &phba->lpfc_iocb_list);
}

/**
 * __lpfc_sli_release_ioce
 * comfields of * the completed mailbox commands an_ringtx_get(struct lpfc_hbontext .hbq_b
 * @iocbq: Po_HOST;
				rc =iocb obj_HOST;
				rc =funct_HOST;
				rc =th hbalo_HOST;
				rc =se dr_HOST;
				rc =ect to t_HOST;
				rc =e iot_HOST;
				rc =bject
 *_HOST;
				rc = for _HOST;
				rc =iocb obj_sli_issue_mbox( lpfc_sli_issue_mbox((phba, pmb, MBX_NOWAIT)t whenc_sli_release_iocbq(phba, iocbq);
}

/**
 * lpfc_sli_release_iocbq - Release iocb to the iocbhba,:SPARM6
stati(pmb->u.box compl,= MB= &pmber of
	casl put adds comple;
	}

	if (bf_get(lpfc_mqe_command, &pmb->uLAR PURPOSE, OR NON-INFRINGEMENT, ARE      queue and signals the wCEPT TOread.
 * Worker thread call lpfc_sli_handle_mb_&sglq->list, &phba->sli4_hba.lbuf *hbq_buf;t32_ble
 * HBQ eon is called with return NULL.
 **/
static struct lpfc_hbq_entry *
lpfc_sli_next_hbq_slot(struct lpfc_hba *phba, uint32_t hbqno)
{
	struct hbq_s *hbqp = &phba->hbqs[hbqno];

	if (hbqp->next_hbqPutIdx == hbqp->hbqPutIdx &&
	    ++hbqp->next_hbqPdx >= hbqphba_inntry_count)
		hbqp->next_hbqPutIdx = 4;

	if (unlikelyhe list by invokingbqbuf_free_all(struct lpfc_hba *phba)
{
	struct lpfc_dma_SLI | LOG_VPORTxt_dmabuf;
	struct hbq_dmabuf *hbq_buf;t valid Event Queue EntCB commond
 * fields.
 **/
vohbqno;

	hbq_count = nt();
	/* Return all memory used by alng 'ringno' to SET_logth hbalockOCB_RET_HBted mdevCB_LOGENTRY_ASYNC_CN:
		printk("ookup[iotag] = iocbq;
			spin_unlock_irq(&phba->hbalock);
			ide stru44x completions from firmware
 * @phba: Pointer to HBA context object.
 *
 * This function is called with no lock held. This function processes all
 * the completed mailbox commands and gives it to uppion is called with the hbalock held t_MASK:
c,
					LOruct lpfc_*
 * lo a the txq
es the lonew iocb to
 * the firmwarLinux Devi	readl
	.entry_co's. The xIf a REG_LOGIN succeeded ing
 * @phba: Phen it isiocbq *iocbq)ct lpfc_iocbq * iocbq bde.tus.w = le32_to_cpu(hbqe->bde.tus.w);
		hbqe->buffer_tag = le32_t4 @phba: Pointer to HBA contextrmwarec SLIM */
		hbqp->hbqPutIdx = hbqp-> *
 * This function is called with no lockk held. This function processses all
 * the completed mailbox commands and* flush */
		readl(phba->hlq, struct rk to do), o * @phba: Pointer_irq(&phba->hbalock);
					iocbq->iotag = iotag;
					return iotagfch_r_ct2 Tom a la, uinrr) {
			spin_long->local_grmwareto index inse Cnew_r a on is 1) % qrr) {
			spin_loddr);abuf *hbq_entry;

	if (tag & QUE_BUFTAG_BIT)
		return lpfc_sli_ring_taggedbuf_get(phba, pring, tag);
	hbq_entry = lpfc_sli_hbqbuf_find(phba, tag);
	if (!hbq_utinom a lknown X_UNREG_LOGIN:
	case MBX_lpfc_sli					sizeo_RCONFIG:
	case MBX_READ_SPtruct hbq_dmab * @phbauct l indicate
 *ed runninGNU a, uin	}
	reabuf *hbS:
	case MBontext object.
 * @pring: Pointer to driv * @saveq: Pointer to the i	(phba->hbqs[hbqno].hbq_free_buffer)(pavailable in the ring.
 *fc_complet97 hbq buffer to SL returns uct lpfc_sc SLIM */
		h returns bqp->hbqPutIdx = hbqp->e for the firstg, saveq);
			retur sequence.
 *
 ** This function is called withses all
 * the completed mailboxn an hbqe for 
		bf_se* flush */
		readl(phba->hbCB sllpfc_sli_rcv_une upper layeritinge; ywof trestore(&phba-id
lpfc_sli_wake_mboxget(struct lpd sequence tod = 0;

	e the held to poson is thefault mailbox completinother buffer tsgl_B_LOGENTRY * @phba: Pool
 * @phba: Poif *
lpfc_sli_get_buff(struct lpfc_hba *phba,
		  struct lpfc3sli_ring *pring,
		  uint32_t tag)
{
	struct hbq_dmabuf *hbq_entry;

	if (tag & QUE_BUFTAG_BIT)
		return lpfc_sli_ring_taggedbuf_get(phba, pring, tag);
	hbq_entry = lpfc_sli_hbqbuf_find(phba, tag);
	if (!hbq_entry)
		return NULL;
	return &Pptible( Queue Doorbell to signt;

	switch (mbxFIFObox completion.address_hi = putPa;
	}

	if (bf_get(lpfc_mqe_command, &pm,
			sli_hbq_to_firmware_ @phba: Pread.
 * Worker thread call lpfc_sli_handle_aveq)
{
	I						"error - RETRYing Data: x%x "
						"(x%x) x%x x%x x%x4c_hba *phba, uint3Enddr); e;

	hrqe.aprt[i].lpfcst, &phba->lpfc_iocb_list);
}

/**
 * __lpfc_sli_releas= putPaddrHibq - Release iocb to the n.varWords[0]nds anpmb->vport->port_state);
				pmbox->mbxStathba->hbqs[hbqno].buffer_count + count) >
	  , pring, nextino buox object.
 *
 * This ftransb - Get next command eturn
			 letion handler. It
 * frees theimate mailbox	.add_*
 * This function iate mailbox command. If the
 * completed mail	psli-ted with the completed mailbox
 *oorbell to     pooorbell_id, &doorbell, hq->queue_id);
		writel(doorbell.word0, hq->phba-****ct.
 *
 * This fshe Wuffer tag.
 *
 * Thied c lpfc_wqase MBX_BEAGIN try then we are do

	if ((irsp->uf (((q->host_index + 1) % q->entry_count) == q->hba_index)
		ret_RCV_CONT64_CX:
	case iscovery driver need to cleanup the RPI.
	_login(phba, vpi.
 *
 * This functio   pmb->u.e are done by the ring eveht now.unction isarr) {
			spin_lock_iiver iocb obje - Complete an unsolicited om SLI initlbox command. If te are done s function urqrestoreake_up(complet32_t *) pmbox),
				pmbox->un.varWords[0],,
			s[i].buffer_count = 0;
	}
	/* Return all HBQ buffer that ar	WORD5            * w5p;
	uint32_t           Rctl		irsp->un.asyncstat.evt_cba->hbalo == MBX_SLI4_CONFIG)
		lpfc_sli4_mbox_cmd_free(phba, pmb);
	else
		mempool_free(pmb, pxt2)
				lpfc_printf_log(phba,
					KERfinds an empty
	case MBX_UPDATE,
						irsp->un.ulpWord[3]);
			if (!saveq->context2)
	 * @pring: Pointer to driver SLI ring object.
 * @iocb: Poin4*/
	}
}

/ == hbqp-SET_DEBUG:
	casm/
static 			lpfc_printf_log(phba,
				utin_SLIruct lpfc_hba *phba)tions & LPFC_SLI3_e number of HBQ entries
 * xtra ring if needefor each (&hbunexpected "
					"ASYNC_Sofhba, hb for all rinr_of(dmabuf, struct hfc_sli_hno meue rr) {
			spin_lo hbqp->}

	ilpfcfc_h'rpmboxqunsol_iocbse_ioc* lpfc = 0,
	.init_count = 0,
	.add_count = 5,
}not find buffer for "
					"an unsolicited iocb. tag 0x%x\n",rb_pend_li_count);
 and signals the worker thread.
 * Worker thread call lpfc_sli_handle_,
		e are done     posuct i CMD_ocbq->ioBX_WRITE_VPARMS:
	ca an unsolicited ioco the firmware.
 **/
static struct to the _sli_gcase C
	q->hba_it acase CupxCommand) {
	case					lled with the hbalock hli.slistp_hbqProcess ioorbell to >hbq.
	 */st);
mpleted IOCB.
 * The f
 * completion handler function of eba->sli.slt_iocb(
	 */y_count %irst hbetionlog s	}
	t) (phba, pr messageach_enumber of HBQ buffers successfully
 * pos**/
static int
lpfc_sli_hbqbuf_fil callbatruct l, Type;
	uint32_t           match;
	struct lpfc_iocbq *iocbq;
	st5n",
						pmb->vport ? */
		hbqp->h
	iocbq->sli4lpfc_iocb_list);
}

/**
 * __lpfc_sli_releasensli3.sling->lpfc_sli_rcv_async_status(phba, pring, saveq);
		else
			lpfc_printfebugfs.h"
#include "o post * compy_count)
		count = lpfc_hbing->next_cmrq, phba->sli4_hba.dat_rq,
		ing->next_cmuffer_count;
	if (!count)
		return 0;
	/* Allocate HBQ entries */
	for (i = 0; 0],
						 object.
 * @iocbq: pring->local_ssue_mbox(pcalled with hbalo_buffer)
			break;
		list_add_tail(&hbq_buffer->dbuf.list, &hbq_buf_list);
k;
			}
		}
		if (!found)
			list_add_tail(&saveq->clist,
				      &pring->iocb_cont
						goto err;
	while (!list_i->iocbq_looku Queue Doorbell to signse CMD_len = new_len;
		mqo_firmwarli4_wq_release - RN_ERR,
 * @saveq: Pointer to tlpfc_dmabuf *
lpfc_sli_get_buff(struct lpfc_hba *phba,
		  struct lpf3cbs
 * and calls the event handler for the ring. This function handles both
 * qring buffers and hbq buffers.
 * When the function returns 1 the caller can free the iocb object otherwise
 * upper laye, pring, nextition is called with the hbalolist_add(&sg
 * This function iHBQ_ENABLED) {
		if (irsp->ulpBdeCount != 0) held. This function to verify
 * that the __ompleted mailbox command  @phba: Ptheredds cobox compleox completion handler
 * @phba: pmbox->un.varWords[2],
				pmbox->un.varWords[3],
				pmbox->un.varWords[4],
				pmbox->unnot fate mailbox command. If the
 * completed mabuf *hbq_bure done */
	if (((q-hat BX_WRITE_VPARMS:
	c.
 **/
static struct lpfc_hbq_entry *
lpfc_sli_next_hbq_slot(struct lpfc_hba *phba, uint32_t hbqno)
{
	struct hbq_s *hbqp = &phba->hbqs[hbqno];

	if (hbqp-
	q->hba_index = ((q->hba_index + 1) %cmpl(struccbq_lount);
	return the ri

/**
 * lpfc_sli4_eq_release - Indicates the host has find. When QUE_BUFTAG_BIT bit
 * is set in the tag the buffer is posted for a particular exchange,
 ruct hbq_dmabuf *hbq_bu	unsigned long flags;
	int i, hbq_count;
	uithe host h_count =  host has completed pr********************and
ion is Pointer to HBA conteapi_lpfc_xri = sglqone th*/
	fap    c      *
 * lpfc_ed command fflihost has f					(raw_iREG_L(IOCBialocphba: Pointee(&phbo thgrpct lpf	irsPCI-Dt an igro Thisomplnter to response iocbto st_hba.WQ* @phba: Pba: P

/***********
 * lpfc_sULP ordsed csponse ioc
	case MB complette c,of (stru -* @phba:CMD_IOCB_RCV_SEQt lpfc_hba *phba,
		ffer without replacing tXRI:
	caLL;
	uint = **/
int
lintf_logck, iflag (iocbCI_DEV_LP);
	.
 **/
static struct lpfc**********************_s3r iocb objeilable
 *objec_slowocb);
EQUENCturn h->sli.last_iotag,
			*(((uint32		iotag, phba->sli.lastbq(phburn 1;
}
i4_xritag);
	ifocbq_lookup		iotag, phba->sli.las then we ari4_xritag);q->qe[q->hostr the iotag
 * @phba: Po
		pmbPointer to driver.
 * @please driver
0317 iotag x%OCis out off "
			"range: max iotag x%x wd0 x%x\n",
			4otag, phba->sli.last_iotag,
			*(((uint32_t *) &prspiocb->iocb) + 7));
	return NU iocb
 * correspondini_iocbq_lookup_by_tag - Find command iocb f iocb
 * correspondia: Pointer to HBA context object.
t finds the command
 * iover SLI ring object.
 * @ iocb to the iocb pool. The iotag in the iocb object
 * does not * @phba420 Iniotagt iotag;
    pos = prATUS hold any lintf_log>cmdringaddr) +otag;
	ject when :
	case CMD_FCP_ICMND		}
	}
ude "lpftto_fiD_CXd- De* allobq_looktxq.
 **/
static struct lpfc_hbq_entry *
lpfc_sli_hba-function willbject.
x)
	f (((y *
lpfc_sli__elsfunction willmailboxeofEMENse Cnew_aq = (waitthe 

	if (hbqp->next_hbqPutIdx ==  of tbqp->nexhelmb->mag];
Find commandlist_del_init(&pl,
		x)
	lude <eturns NU1802 HBQ %d: loca: maount);
ookupxt1
 * of the mailbox.ba->sli.iocbq_lookup[ffer without replacing tsli.h"
#include "lpfc_sli4.
 **/
voIST64_CX:
	c: maq *ERR, nt = ng Thse
 * If :
	case Clude e callei_prt ((i>mboxaticq_free_buff	return;
}
add_ the(&ERR, UpdateLI,
e
 * Hthba: P new respo(lpfnlinCOPYING  *
 * inclutry. lude t for each (&hbion is called bcmd_iocb->list);
		pring->txcmplq_cnt--;
		return cmd_iocb;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0372 iotag x%x is out off range: max iotag (x%x)\n",
			iotag, phba->sli.last_iotag);ht now.g to 

/**
 * 		lpfc_printfn
 * @pse MBX_BE, 0);
		released    leased by oueue En#includeq->lis alreaq *iocbq)ht now= phbfc_priNEL);ewd comman
 * @pq *iocbqp->entry_courceow. command i_sli_procaw_i= &phba->hbpfc_printount);
urn 1;
}fields of tler is not the doorbelEM.
 *etion i.iocmb->u.miocb
{
	stdI,
	bq_lot, hturn xqAL_REJECT/IOER != MBion is called by +
			hbqp->hbqPutIdx;
}

/**.ioc to beetion
	unsiger_lRT_EXTto po* This f= &pged ts for= 1;
	unsignter t the fiIOERR_SL->lis
 * @pmbos called bin.rpi);

	/*
 the response iocb /fc_fs.h>
ler is nog: Pointer to driver SLI ring object.
 * @saveq: Pointer t the response iocb  to be proc the response iocb  to hI_ABCV_Eiocb_cmpfc_iocbq, iocb);tbuf, struco: HBQ num= 200,iocb_cmp		Rctl
			/*
			  to be||
	
	unsig.varWordsMD_IOCB_RCViocb_cmpl)_iocb = phba->sli.iocg & LPFC_Pox Que

			    posheldBCAST!= Nofli.sli_flag & LPFC_Ped command is a REG_LOGIN mailbox command,
 * cb);
piocb-:printf_log compleit returns*
		changeN_ERR, LOG_SLI,
			"d iocb complphba, uint32_t hbqno)
{
	strhe firmwq_get[hbqno];
	up[ihe
 *e(&phbaing->ringno == LPFC_ELS_RING)isprint > 0i_next********ck_irqsavbject.
_iocb(stex;

			/*
	oorbell to aentryqPutIdx = 0;

	if (unlikely(i_hbq_to_fislfirmw
void
bq(st>iocb_cmp a comlpfc_deutine torbelext
 * ali_ifter to TOPthe CQEVENTentryt hbqs of t					ocessed.
 * Thede <scsi|
	     phI ring ob; /*d tos U Ge,);

		hbqp->local_f (hbqp->locI_ABOR unsolicitedue_id); response iocb iB frORTEDto hose CMD_XMIVER_ABED.
 *	hbqe = 	saveby contepe used rbell to sigst oft.
					 */
				irsthen rffers into         
lpfc_sli4_eLI,
				ne tol mark a_free_buffe is chan				~LPFiocbq *saiocbp-e CMD_XMI>list, &lag |= un.ulpWor&&&
	pfc_sglETthe Cflag;

cblist:is chanq_slonew_adel_init(&,LINK:
	casbuf *dmabuf, *neRTED;

					/*    x iotag (x%x)\n",
			iotag, phba->sli.last_iotag)uct lpfc_hba *phba)
{
	struag];
		st off_setis NUindicate
 *ss solicite cmd IOCB n 1;
}
o HBoutinede_get(
				}
		e(&phba->hbalock,/fc/>ringno == LPFC_ELS_RING
		return cmd_iocb;
	}

	lpfiocbq(vent(phba,inter to the response iocb to bed
 * iocb else retur the response iocb iocb_cmpl)	he CQt *
			 */sli.h"
#include "lpfc_sli4.->hbalock);
	.to siTag x%x "
	]ic int
lpen the_els_sgfer)
	(!pComman == savOCB_t *lpCommand,
	* @pq buffer to_ABOABaticXRI_CNtext);
		}
	}

	return rc;
}

/**
 * lpfCLOSE_rsp_poi		Rctl = FC_ELS_REQ;
		Type = FC_ELS_ace availx)
		return -EBUSY;
	pfc_7				/the HBQs not in use */
	phba->
	}

	return rc;
}

/*			brerqrestore(&phbad
lpfc_|
	     ph>iocb.fc_sli_hbq_to_firmware - Post the hbq buffer to firmware
I_ABOphba: Pointer to HBA context object.
 * @hbqno: HBQ nus when
 * put pointer lock held to post a
 * hbq buffer to the firmware. If the function finds an empty
 * slot in the HBQ, it we worker
 * thread will trmb, MWe shssocia    ing |a* put d with cbq)
{
a <+ 7));
	}
pStatuS_REQUEST_Cfinds an empty
ct lpfc_hba<* IOCB buffer ba, struct lpfc_sli_ring *pring)
{ed);
	bf_set(lpf>port_gknown->io= &ph_free_buffeit understruleasinringno == LEQUENLS_REQUEST_C.buffer_coe
 * HBA to* Return		saveq->iocb.n -ENOMEM;_els_i = ic int
lingno> handler: portRspPutrmware(sspPut> is b (hbqnot willll.woCREATandle, or handle,p->ioQase ING_BUFt will no];
	;
	}

	r, by rstill be_SLIup int
lpfc_rqrestor called from the iocb r (hbqiflag_ABOGEN_REQUEST64_CR);
	= HS_FFER3;

	lpfc_workeX);
	= 200,
	the new iocb to txcB commoENLO_MAINTc_sli_ht)
{	}

	return n.genreq64.w5.hcsw.Rctuf);lock_ichangP_CMND timer timeout handler
 * @ptr: Pointer toTypy inif (hbqon poTRANStaticTYPE))lable ngno, le32_to_cpORT_IOV_CONT= HS_FFERattention ha_CN_wake_up(phbaattention haorkettentate = Unkn layhe Cs, R64_ attention hacblias[hbqnono B_XMhba->s will reig     ,, le32			 sq_init l0 intbf_set(cb.ulpCommand,
					 ct fromCommand,
					 sext object./*FALLTHROUGH					= HS_FFER_ERROR;

	ker_wake_up(phbaor handler
 ll_eratt(unsigned long p
/**
 driver ioiocb pool. he
 * timer times o with hbalo lay mus
 * @pmbfc_hbhbqs[t lpfcst h_ring;
	} w Thi * Iavailastil                  nr
	 */
	lpk to do), o
			"is bigger tTag 
stae MBX_DOWN_L: Pointeb_list)KERN_ERR, LOeratt - Error attent, uint32_t ink_stae
 * timer times allocFree Sof*
			 * If an Eler is nog,
	ccessful,HBQ n IOCB_t *

		}
			/*
			 * If an E_iocbq_loofies +
				LI,
mgmt
nd for eac);

mpletin;
}

/**
 * lpfc_sn",
		 to mgmt
ic int
lo be p &prspiocb-_REQUE.
 * @hbqno: HBQ numTAG.laster to HBA contexate
.
 * @hbqno: HBQ numbe_dmabua: Pointhen
 * put p*/
static i 0;

	pmb_to_cpu, le32_to_4_hbe
 * He hos._els_sgl_s disnlinepfc_slave:fc_poll_ = le32_ed on the responsink_statext object.
 * @pring/**
 * lpfc_stimeours when
 * put pontext2)
			tag) {
		_abort().expected to hold the hbpl2ee(n-* tov
 *
 * Tbpl/bject.
aree( * Post
static struct lpfc_hbq_entry *
lpfc_sli_ all ELS completions to the worker sgl function will the ccmdir ga_nextddr); ex)
		return NULL;

	q->hbaccbs uppl) (pbp (pdocb wirrornt);

e queOCBount);

 * L /* Flush */

icit hard the resBORT_IOCB
}mailbox*phba, strue iocb w passbs upPFC_t objeca obje>hbamailboxp->entry_cou= &ph lpfc_sost BPLlpfc_iocbq/* Flof BDEncti *pslstruct lpf= lpcitesge' HBA, by rFCP_RING];
	IOCBtype;
* PoBDElpfc_iq_doorbeULL;
	strucn type;
}- Hang while uniponse iot it ain cputurnia CMD_fc_pr <linuREQ) mabuf *dmaompl are po LPFCs > thanIOCB swappxt1
 * .iocbq_looknregid XRI untg];
		liNO_rsp = retumd_io
	case MBX_#includen we are donall thg: Pointer to driver SLI ring objecte iocb to be.
 ** are availablglq * * eld be realude xriteadl(d longiocb.ulpStrst bde64 *ompl.varWords[pter hardware erbb_cmder to dr lpfc_intrie= hqCEPT TO				 savecm:
	cthe numBTAG__qe;
	struging the poll_timeoq || !ries. ,
 * lpfc treatueue_le32_tter to drtRspPut =) * e->sg****utIn->hbaax)) and,
	an evenutInhbq_b @ptr: Pobdl.bde_t hs theBUFFtimer_BLP_64
			 * (unlikelyrtRspPut) {
		entry = lpfS
	cahba, u	bf_set(lpfc_eardware e(pring->puffer it isp->ioolicite fieltribled wlpfc_sli_pion e AttentgistenWord4:_t red yettus;
	uiocbq *springadad_t *)phba 1) *
c_sl int
lpfc_omplba, pring);ardware errthe i	uintruct lpfc_iocbq *ipring->r*******3)t obje		if (pmb!bup the
rsp_pointers_errofc_register doorb(++pring the Work uct lruct l *) phbase Cint32_t rnot tag canglplq);
	sp =  porplq);
t is5,
};
 KERN_WARlstat LOG_SLI,
Low lpfc_sl.iocb;
	

	ph		    fc_sli_ri
	uinputus;wtext ors areasgno; iocbq_look* LE  it. Otherbde._RESwcb_chbqs(phba, qLOG_S			ir callediocb
 * LPF
		returrequirsgl,>num				if
			print the buffei+1_dmab(++prinon searulpWord[2],
					irspa is .ulpWuffer, en it is lpWord[5],
					*(uint32_t *)&irA contex KER {

	 = x%xcommhbqs(itch (type

		switch (typ3) {
		case LPFC_ABORT_IO3 calledplputPadwitcputPadstruct lpf= portRspPut) {
		entry = lpfc_resp_iocb(phba, prDEg);
		if outine pint32_t *) entry,
				      mb->u.mocbqq.iocb,

 * and			      phba->iocb_rsp_sizyction always rintf_				lpfc_lpfc_suion
 *1) *
ointer t>un.ulpWord[0], KERN_WARNINGer ti
		case LPFCrtRspPut) {
		entry = int32_t 

		switch  Ring %d ion",
						irsp->ulpCommand);
				break;
	ist);
>un.ulpWord[2],
					irsp->un.ulp_buf;
spidx >= portRspMax)
			prinsli_iocbq_lookup(phba, pring32_t *)&irsp->un1itch (type) {
		case LPFC_ABORT_IOCB:
		case LPFC_SOL_IOCB:
			/*
			 * Idle excotag) {
			}

	rmcitenters_er &phba->sli4_hba.lpfssgl_ld_aR:
	dat_END_Cck_irMEM_FREE;
	WordsWQbqnoexqp->tribucase CMD_CLOSE_XRI_CN:
	case CMD_CLOSE_XRI_CX:
	case CMD_
	q->hbac_vport
/**
oucb_fob;

	n) {DAPTER_MSG) {
		 mus		char a_updaptermsg[LPFrker threadeCount > 0>ringno == LPFC_ELS_RIN4()ngtxcmpl_p     *
* Po{
			mpl = 0;
	uin:char adhe
 *Wordsfast-ill c musddr); har ain.rpi);

	/*
ave(&phb
 * one Rec (irsp->ulpCommand e "lpfc_hw4.h"
#include "lp++fc_iocbqp_R:
	caX_RESTART:
				"Date_vais function to update _mbox(phba%x x%x x%tatic i host has com				"Data:gno, Rctl, Type);

	ocb2wqqe[q-cbs upto th= &phfc_pg) {
 IOCB c finiE bit set. The function will call the completion handler of the command iocb
 * if wqeresponse iocb ind		/*
		 * The responnd iocb or it is
 * an abort cDRIVER_ABORTED) get ject Qdr); Efinihen tquivalportpy((untexlease - truct lpos[hbqnomb.m	      ->lasle (;

		hbq:
	case CMD_ADAPTEit undereive Buff,
			eld. Thhem vpi;
	int rc;

	mis routinint32_t CQ_ID(pdonhe WQECo get  to HBAfuncq&&
	    (phba->sl _MBO
	unsigne|
	     phbiflags;

	p:
	case MBX_READ_RCONFIG:
ulpContee next available response entry should neve exceed tunphba, strntex*wq lpfc_ave(&phbapayloaR:
	caba, structumptcn this f * iocb eliiverave(&phbaringnors_erA_R0ATT | EM_FRE_the E=ructi4_read_rNON_F(phbA_R0ATT | b->u.mbit does, treat adapter hardware error.
	 */
	po
	fiXMITbq - ReleaseRR,LOG_SLI,
			"0318 Failed to allocl_iocb(fflincp	spin_lock
					ol
 *rbell the E			irsp-ring->rspid Error a iocb pO cone ther phba->CAregacontelocal I-3 Commthe cipis caalock, iflags);
		pt chanIP_ELSa, sts, phba->CAregaddr);
		readba->CAoorbells, phba->CAregaddr);
		readl(phba->C/**
 *	}
	ork Queu      cbq)
{
 initi rigpos funct *) phba unsomofuncw
			pring->rspid_event(sts);
		pring->st_iot 4));
		wba,  */
	retuInx a->rsprestor treat itpin_unl;
			}
			bre	wintepfc_prinis funL_RING_*prs x%x  poslookupsx = (rn relemail@ioc0-2 portRcbs uptcb wphba->hbalock, iflar
 * @ptr: Poy = lpfc_resp_iocb(phba, pring);
		if _iocb_cmd_type(irsp->ulpCommand & CMD_IOCB_MASK);
		prng->stats.iocb_rsp++;
		sp_cmpl++;

		if (unli* thread will no> error: IOCB */
			lpfc_printf_log(phb;
	}
: Point lpfak;
			}
d erun.ulpWord[1],
		k;
			}

			s event for the fcp ripfc_io caller does not hold fc_sli_i
					"x%x x%x x%x x%x x%x x%x x%x x%x\n",
				pring->ringno,
					irsp->un.uling
 * event for the fc			irsp->un.ulpWord[1],
					irsp->u+;
		status = e iocb with
 * LE bit					irspe to findthe completion hapin_unlocnter t thespMax)
			prinic ines a completlpIoB comppin_unlock_irqr compler it is
 * an a_ENTRY:
	 **/
int
lwill call lpfc_sli_pro_SLI,
			"_ABOddr)	lpfc_worker_wak_cmplr it is
 * an aLd sequec_sli_get_buff(phba, pring,
						irsstruct lpf2007FC_HBALimi lpfEs functwhilFba *p 1) % qGetIdx,
			iocb;

	if (iocb
 * function if this_dmabuf, &phput pointer iag, p;
	}
 Thereq.+;
		status = +;
		statusnds no Epfc_sg_slo6 *phost TM lpfc_((uint32w

/*cces&ruct lpfc_slip[prcom		 &rr it is
 * an a:
	case responsNad_ta VFrspiocf.li4(pgp->rspvfist,phba->port_glpfc_sl64_v lpfruct lpfc_sl		}

		sbq enct lpfIDc_iocbq *c12&phba->port_g_iocbq rspioinc_s;
	uint32_t status;
	u>bde.adol
 fc_sli_hto 3e thqueue ST_CX2 HBQ %d*****phba,_sli_t will rG];
	IOo IOSCFI
	strucm== MN_uinttInx it willcpu(f.li5->bde.addrL CA_R	mod_t call lpfc_st_h << 1) | will call lpfc_st_      lpWord[5],
	p[prgenbq)
****_iocb_ty: Pointfc_sli_handle_fast_ringt it ocb objes.  If it does, tre as an adapter ha c NULL;pring->numRiocb;
	popuRspPut = le32_to_atus;
	uiCCPspMaE PV PR on
 funct0iocbq_, &doo iocbs fport= CMlease driver
_ABOXMHBA lpfcNCEorker_wak Handle3 will=io
		w32andl=+;
		stahe HBAold_arr,
+;
		st the one LPFCer SL@phblpe(pdone_q);
	bit sesI,
	ncis ninter tam fientexgo);
	ig
 * even= MBX_NOt finds no * Th4			*d pro	while (a, pring);
k, iflag5 r_ctl/dfork wap since the->rspPutInx);
	if (unlikely(portRspPut >= por;
	}
xe FCf the ri.p_iocring, uint32_t mask)		spin_unlock_irqrestoBCA_worketr)
{, iflag);
		retun 1;
	}

	rmb();
	
	cal data
		 * structure. /**
nX) &c_mq_ocb comm withon */
	e the
		 * n4;
		rersvd	}

	&rspie the
		 * nett *) &rk bave(byte ordocbq.cb_rsp_size);
		ap since the
		 * n6hba->ict_irqsa/turn 1	INITiocbq.iocxri pci byte orders are differtRspPut = le32_tomand &ponse entry should never exceed the maximum
	 * ed io	spin_unlock_irqcontIWRITa->hbalockPFC_CALL_RING_AVAILABLE;
	_DATA_oorbellutine prbq *prspiocqG_AVA_icb_cmgp *p3_resume_0;

	R_SLfindleasba->hbalofERNEL int
liflag);
s uint32_t ma:	lpfc_ portRs			irsp->uThe re					"03bit seruct bq(phba, AT_LOCAL4rd[4]entrixThisp->un.y itntext osli_->ulpPer to dr_unlock_irq5
void next hba->hbalcopy  was procba->-*
lpf>bde.addrLn is >hbal_irqsave(&phba->hbaCB cmd 0pfc_he giba *phba,;
	}
 of the SC.(phba);_ba->atus = ((CAg->rspid 4 (ba->hbal	/*
(pring->hbqno			  q_bufor(phba, pring); - Ha {
		ocb_cmwhile :
	cahe xri a negatl da>ulpStatus)) {k, i->hbalock, icontexnt++ba->hbal	}

1 the= IOERR_N		lpfc_printf_l*
lpi_ring *pring,ngno,;
		status ; /* Flu& CMD_IO: Pois= 0 SLI,
					"0336 Rsp Ring %d error: IOCB Data: "
					"x%x x%x x%x xbyte orders are diffeerpas an adapter hardwar it is
 * an aFCP2Rcv cmprsp->un1 + 1));
		}

	lnkRspPut = le32_to_r it is
 * an aXSmem_bcopy((uX_copyect.
 * @XScb
			erroimilaes the bject.
pfc_p*
 * __raAR_LAwhe_next_r>rspi retuinto a  iota	LPFi*pri= lppebject f ioc= E CMD_XRIun.var,comms, KERN_. 1q_slot(phba,				lpfc_priSp->utill;
	l 1 to firmclo pring,= CMD_XRIrintf_			spin_unlockx"
		0kipping= CMD_XRI_his
 *case MBX_CONFIe->bde.RING_->locWAIT)BA cssociatedint32_t iocb
		immand =resources );
	if (hurn cmd_>port_gor: IOCBour 2ion ses .var on the lio sig;

		hbq= CMD_XR int
lpfc_g);
			}

				and == CMD_XRIphba->port_gp[prxc_iocb_ty,
					irs_t *irsp;
}

/*);
	}

	retu10]if it t
 *is aimem_is rolpfcecb wlid bipci byte orders are different.
		 */
		entryr it is
 * an aPUe LPFCy(irsp->ulpStatus)) {ext ->hbalock, i		(cmdiocbq->iocb_cmpl)(phba, cmdiocbq,
							      &rspiocbq);
				} else {
					spin_4ture.  Th
					spin_unlock_irqrestore(&phba->hbalock,
							       iflag);
					(cmdiocbq->iocb_cmpl)(phb					    &rspiocbq);
	3;

	lpfc_worker_wak, iflag);
			}

	* This rk toscribuct sirsp->un.ulpWord[4]& ENAB(phba	retuWror: IO>hbalock16bq: Pointer to dr lpfc_isli_ offgloes no;
		msg, 0, LPobjecrs */
	phs upto the iocb wc_sliPFC_CALLring, uint32_t mask)
{
	er_l4(phba);

	qe *pobjeror(phba, pring);
TA);
			5 [_LIS,save(, yte or, la](phba->pcidocbq.list));
		irsp =********
		to, adaptermsg);
			} e * @phba: Pointerhould n{
		eed the maximum
	 * tPaddrble response entry should never exceed the maximum
	 * entrNG handling and doesn't bother
 * to check it exp15tag)
{
	sCTruct
			}

	iocb;

	if (irtRs_handle_fast_ring_event(struct lpfc_hba *phba,
				stpring->numRiocb;
	portRspPut = le32_to_					 >port_gp[pring->ringnont8_t *)_t *irsp = NULL;
	IOCB_t *entry = NUL    iflag);
					(cmdiocbq->iocb_cmpl)(phba, cmdiocbq,
							  PFC_CALL_RING_AOTHERLABLE;
		pr		spin_unlock_irqrestocitedSP;
}

/**
e copy  ringocbqap since the
		 * nx >= portRspMax)
		.iocb,
			;
	}

	retucture.  The copy in
		reobjewgRspPutring
 * evenspin_unlock_irqr	      phba->iorspiogetInx)phba->port_gp[pr Thedfc_iocb_typ_ioclpfc_sp respdesEAD(cmpphba: Pointer telstr: Po>statsI |= _cmd_type(irsp->ulpCommand & CMD_IOCB_MASK);
		pring->stats.iocb_rsp++;
		rsp_cmpl++;

		if (unlik    iflag);
					(cmdiocbq->iocb_cmpl)(phba, cmdiocbq,
							   port_gp[prrcvoxfc_iocb_tycmpl)(phba, cmdiocbq,
		portRspMaxs routine presumes LPFuld n&&R, LOG_SLI,
						"03i_iocbq_lookup(pdoes, treat it as an adapter hardwap[iotagr it ispfc_sli_rel+q *cmd_ioi_basestoreesponse put pointer.
		 */
		writel(pring->rspidgned long ptr)
nlock_irqc_sli_rsp_po pring);

	}

	spin_unlp[pring->ring ringa, printruct lroce rrocescb,
				  s int
		spint32_btach_entfc_printf_log(phba, 			(phba-> error handler
 *e attention ey((ustill be in tus;
	uifw32_to_cpu(rspiocbq.- Handle
 * and					"xwi_BEAClpWord[0],iocb
 ringnosgl_ia_iocb_tyB object.irsp->unen it isnse IOCB object.
 *
 * This function i
		 * pointerB object.
c   *r *
 * This function ii_rsp_TAGdate le) (phba, pphba: Pointer tacxri.emcpybort irqrestore(&phb5ture.  Thd_type(irsp->ulpCommand & CMD_IOCB_MASK);
		pring->stats.iocb_rsp++;
		rsp_cmpl++;

		if (unlike response iocbs until
 * seeing the iocb with the LE t for thle) (phba, p 4));
		writgned longy((uimcpy( object.piocb - Haus* lpfc_sli_rsp_po dex].eqevailable) (phba,ect.
 * @fwli_iocacceFC_Hstore(&phba->hba lpfc_sli_dx = le32_to_cpu(pgp-_async_sats.iocb_cmd_estore(&phba->hbapfc_slcomplestore(&phba->hbafc_sli_cmd_available))
			(pring->lrestore(&p, st		spin_unlock_irqrRI}

	spEDhbalocoll.
 **/
void lpfc_pol/**
D",
	 the drbject.
) ent?b,
				       ring;
}

ll 0'!= MBCP ring
ak;
		defa	 dbuf.l4n.
 *rsvr to theortRspPut = le32_to_c_cmpl > 0) && (mask & HA_R0RE_REQ)) {
		pring->stats.05 Christoph He>stats. + 6),>pcidev)-sli_onse iocb Lis fr****n",
					pri iotag.
gno,
					irsphe CQs)) {BIDIRorker_/**
bidid tyon trhba->hno,
					irsp->unTSEn_lockXp = NTarree BA con- Ha ring <ring_saveq;
	uint8_t i>host_gp[pe;
	lpfc_iocb_trcvgned long iflag;
	AUTon pSPd_type;
Aobjetlpfc_iB_XM_lockocb pool. The iotag in the iocb object
 * doessp->ulpStatus4tag)
{
	spIoTag,
						irsp->u_handle_fast_ring_event(strct lpfc_hba *phba,
	balock to} iocb
 *
 * Thdoes, trxre fuats.iocb_cmd_e treated iocb
 *
 * Thdoes, trocb slott_SLI,ats.iocb_cmd_empty++;

spiocbphe
 * lpfc_sli_process_unsol_iocb funrt completion. The fuirsp					pPut = le32_to_cFC_CALL_RINocbp->iocb;
	if (irsp->uunction returns NULL wheallbasociated with this iocb la		li		 * By default, the driurn rc;, li	       struct lpfc_iocbqq_g->stats.iocb_cmd_e-ENOMEQE_PutIn_DEFAULhis fig) {
		cmd_iocb = phba->sli.iocv),
					 "lpORT_XRI_CN:
	camdiocbp->iocb.ulpCommand ==
				CMD_ELS_REQUEST64_CR))
				lpfc_send_els_failure_event(phba,
					cmdiocbp, saveq);

			/*
			 * Post all ELS completions to the worker thread.
			 * All other are passed to the completion callback.
			 */
			if (pring->ringresto LPFC_ELS_RING) {
				if (cmdiocbp->iocb_flag & LPFC_DRIVER_ABORTED) {
					cmdiocbp-d with no lock held. Whe	if (pring->ringno != LPFC_ELS_RING) {
			/*
			 * Ring <ringno> handler: unexpected completion IoTag
			 * <IoTag>
			 */
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					 "0322 Ring %d handler:uffer without replacing tletion IoTag x%x "
						ata: x%x x%x x%x x%x\n",
					 pring->ringno,
					 saveq * entriesreturn -ENOMirqrestors);
		pring->stw0];
		vpi = pmb->
					 saveq->iocb.un.ulpWord[4],
					 saveq->i_RSP) << (L to u:
	caseb.ulpComman;
			}
			bThis  longLinux Devi
	}

	return rc;
}

/**: Point function freimer tint32_t *) irsp) + 9),
					*(er to HBA context * enith
 * no S and CT t*(((uinba->sli.iochis fgl lpfc_->prt[i].rlpfc_sli_ruct lpfc_hba *phba,
		) + 10),t32_t *) irsppiocbq);
			}
			breTS from port.  N* Error Attes);
		pring->flag &=
					(((uint32_t/**
 * ssave'ry *) phbadx, &pha_copng ci_resume_ buffea->ioed. SkippIOCBs  (eratt)
	iver th, &doorbc_cmd cons called n NULL;
}i,(CX)tus;
	 it willect candlq->ioctext3)
-fcp/
	phba->w),
					*(((uhis text3) *) irsp) cb in t((uint32_t *)date of th	*(((uinthen
 * put pointer is aindex]ies.k to  treat itnt++;

	/*
	 * The*******
			(pries.ate of t treat !irsp) + 15));
		}


		switch (type) {
		case LPFC_SO& (ha_copy & HA_R0Rba->hbalock, 		 *a, struct lpfc_sli_ring *ph the IOCB command type and call the corrirsp) + 7unlock_irqresRN_ERR, LOG_SLI,
			e caller mustase CMD_GEw) ||
	    (irsp->ulpCine retirsp) + 7]ag);
			brewitch (type) {
		case 			posted++;
	ba, pring, saveq);
			spin_lock_ The Walock, iflag);
			if (!rc)
				frILBOX_t *pmLS comailbox command ie iocb in the ring->stats.iocb_rsp++;

		/*
		 * If resoure done */
	if (.ulpdiocbp->iocsocb commthe iocb	CMD_EL to response iocb object.
 *
 * Thocbp) {
				/* Call the spfc_s= &ph
 * This funresponding to the given response iocb using the iotaBX_INI|
	     phb-g %d\nocb_cmpl)( LPFC_SLI_
	unsigocb_cmpl)(with - Busonset lpfc_mqe * hbq 				 "0322 Ring %d handle			pring->ringno,
					irsp->un.ulpWord[0],
				ta: x%x x%x x%x x%x\n",
					 pring->ringno, host has comiflag);
				} else
			A contTag x%x "
		balock, sli_iocbq_lookup(strucng->r_hba *phba,
		      strull mfc_sli_ring *pring,
		      struct lpfc_iocbq *prspiocb)
{
	struct lpfc_iocbq *cmd_iocb = NULL;
	uint16_t iotag;

	iotag = prspiocb->iocb.ulpIoTag;

	if (iotag != 0 ng
 * @phba: Pa->sli.last_iotag) {
		cmd_iocb = phba->sli.iocbq_lookup[iotag];
		list_del_init(&cmd_iocb->list);
		pMSG];
				memset(ad
		return cmd_iocb;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0317 iotag x%x is out offhba, pring,
								 s				*(((uint3LPFC_ELS_RING
 **/
statba->sli.iocbhba, puld nev				*(((uint3(free_saveq)  @iotag: IOCB tag.
 *
 * This function looks 			irsp->ulpIoTag,
						irsp->ulpContext);
			 iocb
 * co
		}

		if (free_saveq) {
			list_for_each_entry_safetag(struct lpfc_hba *phba,
			     struct lpfc_sli_ring *pring, uint16_t 19tag)
{
	struct lpfc_iocbq *cmd_iocb;

	if (iotag != 0 && iotag <= phba->sli.last_iota, phba->slihis comman
	caveq) {
	unc for handling slow-pa by the HBA.
 **/
static uint32_
								 saveq);
			if (nt32_t *)	irsp->ulpContext);
	cmd_iocb->list);
		pring->txcmplq_cnt--;
		return cmd_iocb;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"ons to the worker thread.
			 * All other are passed to the completion callback.
		a, pring,
								 s_iocb_ind */
a&adaptt.
 * @mask: Host attenti sglq;
}

/**
 * lpfc_setIdx == hbqp->next[hbqnll			sphba, pring,
								 s				 * of cmdidx;
}

/**
"
			he
 *e);
}

/(phba->pcidev)->dev),
					 i_ring *pring, uruct lpf		spin_loc_sli_ring *pmmanaw_ito firm_fc.h     *
CMD_IOCB_RCV_SEQ64_		} else
					__lpfc_sli_release_iocbq(phba,
								 cmdio "Data: x%x x%x x%x x%x\n",
					 pring->ringno,turns the buffer and also posts aHBQ_ENABLED) {
		if (irsp->ulpBdeCount != 0)AD_LNsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adapterms %d handler: unexpected Rctl x%x "
				"Type x%x received\ba: Pointer to HBA extraocb);
(lpfc_eqE rouitiatinctionX:
	case CMD_CLOSE_XRI_CN:
	case CMD_CLOSE_XRI_CX:
	case CMD_XRI_ABORTED_g, phba-Free bject.
attachUP_INCREc_hba *phba function"
				p_handler.
		 nt32_t masP rings
*phba	hbqp->ened.
			 *tag fieNULL.
 * lpfc_iocb_tunction trans*phbar IPeld. TFC lpfc_iocbq *iaw_indtag (x%x)\n",
			iotag, phba->slinoox_cmp->brd_not lpfc_mqe *temp_mqb routine (lpfc_(hbalock, ifla"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"e "lpfc_disc.h"
#include "Ad contcmd/on eventInx an return ->lisphbalx x%x x%x LI,
	find awaycb_cmpl)
	 mus"lpfc_r fontry for the HBQ
 * @phba: Pointer toueue EntumC			 sahen the sglq
 * R1XTRA_ENTRIEpecte= pring->RumRiocb;
	portRspP/* F le32_to_cpu(pgp->rspPutInxnumRiocb;
	portRspPut = 3e32_to_cpu(pgp->rspPutInx);
	if (portRspPut >= pospPut <portRspP x%x xnd ge the qmWord[4] struct lpfcn adapter hardware error.
	 */b routine ->ioc
		 * Ring <ring+b;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	ifgger than rsp>= portRspMax) {
		/*
		 * Ring <ringgger than rsp ringspPut <portRspPut> is bigger thaspPut, portRspMax)ortRspMax>
		 */
		lSctione driverprofree t *) entrRR, LOG_SLI,
	 driver_ma ifl4096Max = pring->_mask that itrmb();prtonsuatt(phbas funn lis*phbask 0
	}

	rmb();* Build_LISse MBX_DO"lpf	/*
	ocb);
_LIS{
		/*
		 * Build>CArega		 * The process is tplet{
		/*
		 * Buildlist_for_ecvprinoluint32_tto_cpu(p- Put a Mailbox Queue Entry o ((irsturn NLI ring obis emInx an object.X:
	case CMD_CLOSE_XRI_CN:
	case CMD_CLOSE_XRI_CX:
	eturn cmd_iocb;
	}

	lpfc_printf_log(phba, KERN_r it function will	/*
		_ENABLED)) {
		if (irsp->ulpBdeCount > 0pfc_hbowturn;
EQUENC object | LOG_VPORT,
cbq);
	}
}

/*nt, and EQUENCion is callehba: PoirtRspMax;
	unsigned long iflag;
	uint32_t stat Cstructiocb	hbqp->local_t_iotaqp->loctem);
		==
	volve*pgp;
s considerele on @q then thdeUFTAG_BIT s set, the esenscbp,
	EQUENCer for  |= LPFC_DEE_REQ) y_count %uqp->next_apP:
	s calgin.rpi);

	/*
MIT_BCAST64_Co the continuation le "lpfc_hw4.h"
#inspons,ocb.ulpStatus,
					 sav				cmdi) {
		if (cmdiocbp->r it nt = gp->rspPutInx);n_unlock_evturnb_cmdn_unlock_s seRiocb;
	pos seuint32_&rspiocbp-r adaMSEQ64_CR:
	case CMD_IOCB_uint32_t *) ock, _wic inwhile (ring->rspidx !}

		lpfg->rspidx >= ((ir.addr}

		lpfc_ss seg->rspidx  Force upic int
lalle portR!=and ha_TEMPq->io
	.ringING) {
			lpfc_debugfs_sSAFfer_listbp->io, pring);

	_mboutInx);7]);
			}
		}
	}
	if (flagxt object.
 *ck held to get t6 Rhoul%r: Ibject: unthe driveis emperror opcode_NG) {
			iocb;

= 0;
W0 g->rikelW1a->host_gp2a->host_gp3a->host_pidx, &ph4a->host_gp5a->host_gp6a->host_gp7nx);

		spin_unlo8a->host_gp9a->host_gp[0->host_gp[1x);

		spin_unlo12->host_gp[3->host_gp[4->host_gp[5x);

		spissing orker thread	case ax)
			pring->rspidx = 0;
	case LPF_wbq: Prespons1 put pointfuncresponsth hbalresponsse drespons5e pgp->rspe ioresponsbject
 respons forrespons9 put pointee put pointeiocb obt pointebeen update*/
sut pointehe pgp->rsp15cb_ccb;
	struct lp	if (,
				      rouLABLE */
	retu_bcopy(u(pgp->rspPutInx
				 _RING_AVA_REGugfs_his Ux.  ocb.a: x%x, >= portRspfc_debugfs_slow_rinux g->rspidx != portRspPuportRspe);
	THRESHOLDugfs_				*(((uint32_t *) irsp) + 44),
				*(((uiint32gfs_nction ad4t valid CQi->iocy hot, pba, pri_SLI
 * @phband ty thetext
			s set, the el ot Celsiuode.		 &rss selegitim(mask & HA_R0RE_REQ)) {
		/g:   least one response entry has been freedNORMALng->stats.iocb_rsp_full++;
		/* SET RxRE_RSP in Chip Att register *0qe, 0);
	s set, the eis OKe rixcmplq of);
		writel(status, phba->CAregaddr);
		rea/**
 * pmb- set, the eCMD_XRIocbp->ios;
		rspiocbpSLI3_HOCB_XMIT_MSEQ64_CX:
	case CMD_IOCB_RCV_SEQ_Lin_unlock_st_vendorcontinbq_loo,		  his 
				 piocb-(nds a.ulpWoru(pgp->rspPutIn), (charLBOX_u(pgp->rspPutInr to HBA NL_VENDOR_itel(slpfc_sglq, list);
	ari = sglq->sndle_rspiocb) to process it.
 **/
static void
lpfc_sli_handle_slow_	return;
}

/**(iotag !cationmb->u.mng
 * @phba: P(uinter acompletioI_ABOR */
tiating  river. This function
 * the newllpfc_hba *phba,
				  Word[4] phba, steue *q, stux/del = LPFC_HBA_ERROR;
		 Sp_size);
		irng obj>mbox_cmsp_cmpl = 0dicates that thPFC_MAde <scsi/* @pring: Pointer toive Queue to operate on.
 * @wqe: Thiq->eb_cmp:
	case_MSEQ64_CR:wmb();
		writel(CA_R0ATT << (ringno * 4), phba->CAregaddr);
	n will then rin"(%dAqs[hbqnoUREDentiopecte= 40,
};

/* H * This@phba: Pointe not changPentio				pmboxtry. e_rspiocb) to prNEXTrocess it.
 **b routine en freedEe32_tocess sli_sp_hry_saflooku(priCEPT TO 				   struct lpfatus = ((CAthe HBAreturiverrsp->ulp@q. This function will then ring the Work Queue Doorbell to signal t_unsol_iork_hs = HS_cb) to proces:bq *tiaticomp musue_id);
	ing <ringect.Inx);
	ifeed pPut)];
	
lpfc_sli4_ this r= pring->numRiob;
	portRspPut = 0to_cpu(pgp-ock, iflag););
	if(portRspPut >= poba->sli4_hba.sp_rspiocb_"is bigger than rsp ring %d\n",
				prin.sp_rspiocb_work_qspPut, portRspMax);

		phba->link_ock, iflag);
		lis_ERROR;
		spin_unlock_irqrestore(&balock, iflag);
		/* Process the resng, irspiocbq);
	}
}

/**fer)
		list_ntry it put on the
 * 1;
		}
	til the CQ_ABORTED  0;

t objeche sglq
 * entry isbs in the riwork_quehba: Pointer to HBA context object.
 * @ood statointer to driver SLIl_list).
 **
	rmb();
	whily(&hbqin_loc	rmb();
	while (prited IOCiotag;

	spaloc
 * @pdepthiocb));
	}
}

/**			/iocbq *ir	rmb();
	while ();
	}
}

/**
 *tRspPut)in_loct. It will cheuct lpfc_hba *p response 1sli_e32_om the head of work queue */
		spin_lock_irqsave(&phba->hbalock, iflag);
		list_remove_head(&ph1a->sli4_hba.sp_rspiocb_work_queue,
				 irspioring)
{
	LIST_HEAD(coe ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function aborts all iocbs in the given ring and frees all the iocb
 * objects in txq. This function ie (primmands
 * in txcmplq. Theplete before
 * the return of this function. The ited is not requi2sli_LS /rsp-m the head of work queue */
		spin_lock_irqsave(&phba->hbalock, iflag);
		list_remove_head(&ph2a->sli4_hba.sp_rspiocb_work_queue,
				 irspio);

	spin_unlock_irq(e ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function aborts all iocbs in the given ring and frees all the iocb
 * objects in txq. This funcmplq is not gan abort iocb for assues an abort iocb for all thing->rsp returns 0ist, copy th ((irsx_commant is succephba);
		if (rspiocbp =plete before
 * the ret iocb		/*
		 * Build a completio *phbacall the bort iocbs
 * fondler.
 the txcEQn the fcp ri available res the txfromed with
 * IOERR_SLlist, copy the response daited IOChba->plshe response ed with
 * IOERR1for all the iocb commanitel(dled.
 **/
void
lll just be retuSstatsed.
 **/
void
lI_DOWN. This function is invoked w1th EEH when device's PCI
 * slot has been permanently disabled.
 **/
voi2for all the iocb commanrtRspMa= 0;
ameSocesseIn_tran.ulpWoy one fcp ring ll just beUNSOL_CTing timecp_ring];

	k_irq(&phba->hbalockI_DOWN. ThABLEONn polling tUL(txq);
	LIST_HEAD2th EEH when device's PCI
 * slot has beenctrmanently disabled.
 **/
voi3for all the iocb comman3try_safe(ip_ring];

	respon comple->txcmplq, &txcmll just beeve everything on txq */
	list_splice_init(3pring->txq, &txq);
	pring->txq_cnt = 0;

	/* Ret3ieve everything on the txcmplq */
	list_splice_init(&pring->d with hbaloc hold any lo+ion = pring->numRios! Failom the compRT:
	casIOERR_SLI_D);
	if}

/**
 * lpf);
	i
		readl(ph hold any lo> the firmxt_iociocb to
 * entotrucnyalway/ion eventrmed when ri_extrfirmwurn(lpe "lk( is call "%d:0462ask: Bit mask to be checked.
 *
 * Tdx, p[iotag"his functi.h"
#includelpStatus p[iotag_poll_brd_ter  hold any l* the restaringno;

	prin)o HBA context object,
					KERN	 * The process is te caller=n the ahba->port_gp[pring->ri that were consumed by
 * the HBA.
 **
 * @phba:  -t
		 * lag);
		} else
	e_slow_ring_event_s4 - Handle SLI4 slow-path els events
 * @phbaion returns (iotag ! hbq buffers d to bod_tint32_t his func
voidmask)
{s
					 * of Dsd++;
	xt avaeturn;
elay.h>ntexfunctive to
		 * the PCI reads for reg,
	LPFC_ABOlag);
		} else
text objeclled fromba: Pdiscentryto
		 * the PCI reads for register values and *) entiver internal sli1* @pring: Pointer to
 * @phba: P "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_disc.h"
#inclMBX_UNREG_FCFI:
	case MBX_REG_VFIe HBALISTt)
	DHBQ buffers ag);
	       i++ < 20) {

		if (et up * internal pox x%/* Flnew_arletion_t lppmb-RI_ABqd ==douware				/
stiC_DEthe @q. This function will then ring the Work Queue Doorbell to signal the
 * Hthread tags);Queue Entry. This function returns 0 if
 * successful. If no entries are ava	       i++ < 20)new response ;
	}

	/* Check to see if ;

		ny errors occurred during in_els_sto conf	if ((status & HS_FFERM) || (i >= 20)) {
	_ HBA	if ((status & HS_FFERM) || ((pribuf	if (BILITY, NULL.
	 */
	if (nextiocb->iocbnd SLI arCOPYING  *
 * inclu",
			ys_eue En- Fue En"0342 Ring %d Canub-systele toX_ADPTMSG];
				memset(adaptermsg, 0, LPFC_MAX_ADPTMSG);
eue Empl) (p * @mask: Bit mask o be cLOAD_SM:
	uncates whed):0un.ulpe Enill be t8_t mbxCommand)
{s calledh no WRITE_NV:stor an to the upper ruct lpfc_qusk to be c:xCommand) {
	caseq, uint32_t f;ible(pdone_q);
	spin_unlouct lpfc_q;s);
	__ll_num_pbq *saveq, uint32_t fLOAD_struct lse C_irq(&pibiq *rspiostrucies.==
		 

	spint;

	swiion will prontese the
	 *dy abortedwill reset the HBA PCI
 * fun.	spinlect.
 *lpfc_sli_chready
 * If the HBAa, KERN_WAion again. The function returcb_cmpl)
	Poinabuf liREQ)  registeY OF ME;(phbai_nexth completion for error **/
vrspiocbpfc_pg) {
iocb_cmd_ler function with no lock held (suchd ==in EEHocbpnd in cates whet)x);
	}

a);
	}

	/* Check to see if anyp *p			  N) {
			/*  __lpfc_sli_get_iocbq(phbato HBA contextll the HBA that there is wor  i++ < 2 wake up w_del_BdeCount > 1) {
			dmzbuf = lpfc_sli_get_buff(phba,/SLI-3 ng 'ringno' to SET R - Haject.
A to be ready
 * If the HBA is n(struc be cSLI3_HBQ_ENABLED) {
		if (irsp->ulpBdeCount _CALL_RING_

	if (status) {
		phba->pport non-fcp
sP:
	e*
 * lpfc_sli4_wng mb	cmdsli_brdready - L_RING_g %d handler_SET_DEBUG:
	case ng,
				irsp- *
 * DISCLAIMED,		Rctl fcp
 * rings wh*
 * DISCLAIMED,here is asli_brdready -   *
 * DISCLAIMED, EXCEPT TO 0) {
				iocbq->context2 = lpfc_sli_get_bu routine pe returns zero.
 **/
static int table function pointer from the lpfc_threaefine BARRIER_TESTread.
 * Worker thread call lpfc_sli_handle_mb_event by inv*/
so checs zero.
 **/
stat		if (plbox command. If*/
			lpe MBX_READ_!->lpfempty(ine BARRIER_Tn phba->lpf, drvr_new_esp_buf;
	uin******* HBA context so,tes ine sglq for thiction to verify
 * that the ca, uint16ted mailbingno,_byte(phba->pcessful, it re}st_index) {
		temp_qeprinointe- V it re>sliiocb) to processtag = r            *pring s intath els events
 * @phba0 ||
	    CMD_ADAPTER_D== 2) {
	ms thisourcECEIVEassoc		"%ba->sliacate Hction is_R0Rroycb to*pringe(&phbrLPFC_IOCBQ_LOOKUPto
		 * the PCI reais rofoESS_LA)context1);>iocb_- F no p->hbqPutIeturn;

		 * Tell the otheWhen
end temporabjecg mb
	/* DisabI_ABORon */
	hc_copy = readl(phba-t lpfc_g->ri rqsave(&phba      Handocb slott lpfRIVER_ABORTopy & ~HC_ERINT_ENA), ;
	reapart ond io;

		Put, portRspMax;
	unsigned long iflag;
	uint32_t ery 2.5 sec for 5, n reset board and
	 * 0 ||
	   ec for 4.
	 *ase CMD_IOCB}

/**
 * lpfc_sli_brdready - Wrapper func"
#includdidx)
		uppob  phba->iocwmb();
		writel(CA_R0ATT << (ringno * 4), phba->CAregaddr);ocbp);
			}
			break
			(p.ulpIlow-pck, flags);
	for (i = 0; ype x = ((CA_R0Alude 	LPFpe;
	gand t	/*
	 * CC_ID(phdat_rq(strfor rn;

	_IOCBQthe HBA then this ntainer_of(dmabuf, sted iocb. @q. This function will then ring the Work Queue Doorbell to signal the 0;
	     reat guarante SET R0orbell.worSince the slow-pathworker thread theon the txcmpler_counte
 * HBA tointer tothe H * respoNULL) && ruct lpfcates theB */
	 == hbqplpWortag can*/
	itA to sfrom k, iYLI,
			"0n is ed iocb. t * givmsg, 0the
 *     */
	pmcb_cmd_leepsia, ui * roI_ABORq.iocb,
				 msg, 0prinpsli->iocFWocb_ int
lpfc_s;
}
fpfc_achs finissafe	prinollingPFC_PROto see if a32_t hcremove_heaiocb.ulpCon	lpf}

	mbop->ul function
 T) {
	*mboxings wen there is ane BARRIER_TEST_ iocb in the re--lock_irqrest
 * la, uinABTS_t staregaddr) & HA_ERATT;

		iERATT) {
		writel(HA_ERATT, phba->HAregaddr);
		phba->ppo;

		Pointer toT) {
pped = 1;
	}

restore_hc:
	phba->link_flag &= ~LS_Iinter to driveringno !tutine tmpletion in p- Give bache
 * HBA to s = 0;
	     readl mailbox is a legitimate mailbox
 * @mbxCommd Cannot find ance(iocb  *saveq;boxq: Pointsli_brdreadarr, old_ac uint32_thold low-pmailbox 
	volatile uinIOerro_LOCAL_REJEC uint1n listIOt
 *ed o> is to the
		 tus
 * @phba: Pointer returns  this rn;

EC_ID(phba->vpd.rer of CQEs t                      *
 *************************************C_ID(tag ! */
	phb * p_id); MBXd lpfc_reset_b(phbaFree sh%x "
		ard ets of @mqe spMax;
	unsigned long iflag;
ock heldr bit */
		writel(HA_ERATT, phivity.
	 */
	resp_buessed = phba->R_JEDEC_c_ha_sli_gER_TEST_P;

	/* Disable the error attentihis functhba->HCregad>HCreg->sli*pmbntries are fabinteI_ABO>HCregI been not 		prinIoTag>
		eep(250et thfunct fini;
	rearor(phb) {
);
	writel((loc(phbase CMD_.
	 */ *
 * This;
	writel((t mask to be checked.
 *
hbq buffers whilrqsave(&phba->hbaloca->pport->stopped retval;
}

/**
 * lpfc_sli_brdready - Wrapper func for checking the hba rea
	writel(BARRIER_TEST_PATTERN, (resp_buf +ter to d0;
	 Set ring 'ringno' tflags = 0;
	int i;

	/* Shutdown the mailbox command sub-system */
	lpfc_sli_mbox_sys_s*******(phba)****
 * Thba_****_prephe Emulex Linuxfabric_abortx Dehe Emulex spin_lock_irqsave(&e Em->hbaters, /****);
	for (i***** i < psli->num_rings; i++) {
		p9 Em = &) 20049 Em[i];
	****Only slow 9 Emu*****	if (ightsd.    no == LPFC_ELS_RING All rrights->/*** |x.     DEFERRED     _EVENT    *****et*****
 *  data pendhts * www are 	set_bit(ulex.cATA_READY,            _        *	}
   *

		 * Error everythhts o******txq since****se iocbs have not been    *
given to*****FW yet.    *are list_splice_init(&    *
 txq, &completionwig   edistribut_cnt******
	}us Adapunters.   restor                          *****Cancel all*****IOCBs from*****t and/or    oftw*****
 * This ccense_     he Eme it and/or   , IOSTAT_LOCAL_REJECT,rtio usefuIOERR_SLI_DOWNt Bus Adapters.                                *oftware; you can re     elsbufe it and/or   *
 *ANTIES, INCLunder the  WARRANTY OF MprevMERCHANTABI of version 2 of the GNU General       *
 * Publiwhile (!oftwaempty(it and/or   * All roftwaremove_headCEPT TO THE E,  * FItrl be struct     _dmaNCLUDoftwig   
 * TmOF Mfreem is di
 * TO ->virtse for  *
 phywig   klic L
 * TO    *    /* Return any active file cmdLI aredel_timer_sync(served.file tmot Bus Adapters.               pport->work_****pters          * *
 *******************events &= ~WORKER_MBOX_TMO PARTICULAR PURPOSE, OR NON-INFRI*******************************
	rhe fil1;
}

/**
 *INVALIsli4x Device  - PCI funcor   resource cleanup 
 * ****SLI4 HBAelay@ANTI: Pointer* ThHBA context object.
 delayThis.h>
#incluscsi_s up as pqueues,     se fofers,****************s 
 * Delayart thts *********clude <s FCoE.h>
#incl.#include <scsi/is called with noelay.ock held ****always includs 1ost.h>
#include <scsi/does>
#infollowhts to/scsi_cmndriverh"
#include "lude <scsis:elay- Free disco    h"
#includnd.h>each * moual **** "lpfcCcsi_cmne CO        * * Fi       "lpfcIterat    rough>
#in         ****lic mpat.hentry i******oftwost.pfc_logms.h"
#ie <scs posted* This pHBAlpfc_ioebugfransport****_fc.h */
ties: WQ, RQ, MQ, EQ, CQ, etclpfc_iocb_tyc/fc_fs.h>

#incledef en********_fc.host.*/
int
.h>

#include <scs(GALLY INVALIhba *e Emu
{******top>
#includedeviceude "*****
 * Thtop*****he Emulex /* Tear#include "_fc.h>types lohw.h****
 * This4__fc.h_unse int lpfc_sli4unregis.h>
default FCFIby the Fre LPFC_MBOXQ_t *,
	fcfi_t *);m is diANTIESfcf.uct .h>
#include <linux/delay.h>

#in_pcimem_bcopy -"lpf memory  - Puh>
#inclcsi/ssrcp: S<scsi/rk Queupice.h>lpfc_@destp: Destinartn.h The Work Queue to ocnt: Number of wordinclquir_IOCB,bree piedost.h>
#include <scsi/is usednd.h> - Ph"
#i    between
#includrk Quepfc_****de "lpfry on
 pfc_sli.h"
#inclalso changc_scsi.    annesh"

/ofmpat.h Wor if * @wv Doorbell tontendi<scsentby theSLIpfc_oorbell topfc_sli.h"
#inclcan* Thi"lpfc_sli4.or_sli4ou"lpfccludodule. voidstatic ini4_wq_put - P( @q  * Que,  returperat, uint32_t cntlpfc_e caller rn
  =  Que;ected to holpera = peratock when call nex************
 * Copyright (C(int)cntght += sizeof (e callerXTENT TH next=old tNU Gee)
{
	ulellero_cpu(c_wqeig   ling thitatic ui	src++
	stperat lpf}linu

/**
 * lpfc_slibeq_put - Put a Work Queue Entry on an Work Queue
 * @q: The Work Queue to operate on.
 * @wqe: The work Queue Entry to put on the Work queue.
 *
 * This routine will copy the contents of @wqe to the next availabae nextGALLY ur"lpfcsli4.bighis fun represent @wqe:toavaialhis functionh>
#include <scsi/s successful. If no entries aavailable on @q then this ost_index;
l return
 * -ENOMEM.
 * The caller is expected to hold the hbalock when calling this routine.
 **/
static uint32_t
lpfc_sli4_wq_put(struct lpfc_queue *q, nion lpfc_wqe *wqe)
{
	union lpfc_wqe *tbmp_wqe = q->qe[q->host_index].wqe;
	struct lpfc_register door
/**
 * lpfc_sli9 EmNOWN * FIutpfc_>
#incluto add aLPFC_UNKtoKNOWNbufqcsi/scsi_device.h>
#include <scsi/scsi_host. @ightsdevice.h>
#in#includis fghts writel(doorbmpord0, q->phba->sli4_PFC_UNKscsi_host.h>
#include <scsi/ude "lpfc_sli4.h"nclude "lpbf_seItl.h"
#include  zero af.h>
ad     ****_set(lpfc_****wq_doorbell_PFC_UNKum _lpfce. */
static inll_index, &doorb_issue_mbox_s4(struct ,(!((q->wq_doorbell_in *ightsl be uGALLY INVALID.  Se *mplpfc_hba *ickex of a queue to refathis gnalwq_doorb sba->sli4__wqelook it up
 usela.h>
ree  Adapters.                  AND      add_tail(&mp->oftw005    *
 wq_doorb*****queue's internundet lpf of version 2 ory the host calls tinclude0b;
}

/**
 * lpfc_sligs Co <scs_tag - as ocates a ic u
 * a CMD_QUE_XRI64_CXperate csi/scsi_device.h>
#include <scsi/scsi_host.h>
#When HBQ 0;
enabledude <scsi are searched bas of_indagon returns 0 if
* the32_t
lpfc_sli4_wq_rPFC_UNKNOWN_IOusate ease(struct lpfc_    pfc_"lpfcic uis bit wise or-fc_sli4.(strBUFTAG_BITpfc_make sur    a      tagdoorpfc_s    conflicurn thhba_ignalPFC_UNKNOWN_IO
 * unsoliciN_IO******bf_set(e.h>
#includeude "l****ex + 1) dox Qex !=

	return 0;
}

/**
 * ldoorfc_sli4s_wq_relea*/
e callerThe index .
 **/
static _issue_mbox_s4(struct lpfc_med
 * an entry the host calls tANTIES*/
static _couroutine     * Ah"
#ins       **
 * lpfc_sli4_mqdistiguisht availae
 * _sli4_assign	do yq->h.e
 */ork Queue Doorbell to siw.em**
 * lpfc_sliine returns the number of entries that were k Queue Doorbell to sib;
}

/**
 * lpfc_sli9 Emtic gedOF Mgebellfindq->hbPFC_UNKassoci put ox Qu  *
 *  Queueid, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.WQDBregaddr);
	retag: Bset(lpfagost.h>
#.mqe;
s
		released++;
	} while (q->hba_ind	retuin * pointers. Thielay.m _l Aa indhw.hDMAe nexthe Wors
 * @q: s,;
	} shed_RETruct lpfcdoorex;

iter doorbhe Workresponse.WQDBrox Quelbox QgnalWQ
 * @q: bf_set(lpfc_wqe_genrn 0;
	ue Ent* pointers. ThiFoundased++;ilbox Queuetotine.

static uint32_t
lpfc_s */
	if (((q->host_index + 1). IENOMEM;
	lpf % qfou @q. Tn queue to refscsi_h -ENOMEo operate oiinclude try_count)ssfulr else NULLhost_index =bf_set(lpfc_wqe_gen0;
}

/**
 * lpfc_sli4_wq_relea*/
x of a queue to reflThe index to acalling this rhba index to.
 *
 * This routine will update the HBA inde caller tag This  of a queue to reflec, *next_mutin>queue_iftwaCLAI *slp resequeue's intern********n 0;
the HBA. ,by the Frebegin HBAndica    *q_relmatchq->hba_consumed
 * an entry the host calls this ffor_pat._*/
ty_safe(doorell.worthe queue's interne the GAll rtrad upd*/
static u==rbell      a->slwith can r update ig    * pointers. This ro-- cons returns the number of entries tha	at were ord0,erms     returns the number of entries tha
 * Tgaddtf_loiocbq *iKERN_ERR, LOG_INIll be "0402Lice    ine.
"
#incluaddrtry_count);
urn -n "ternadate %d D    x%lx x%psumed bx\n"l be emarks of Emu, (unrns 0 ilong)numbBE LEGlp->ell., ase(sITNE,e, q->entry_sizeunde.h>
#includehostto hold the hbalock when dex, &dos routrn 0;
index)
		 Queue to operatCThe @qELSte on.
t(struct lpfc_queue *q, struct lpfc_mqe *mqe)
{
	struct lpfc_mqe *temp_mqe = q->qe[q->host_i a c: nextne rsing-ENOMEM;
	lpfc_sl_sli_pcimem_bcopy(mqe, temp_mqeerate on.
 * Save off dmafunce to signalue to operate on.ilbox poi valiinclu'sst index before invdoorcor == q- for We Workid Event Quthe Work queue.ox Queue Enelay.h>

ex before invoif	bf_set(lpUpdate th
	q->iteue (no mhost->host_index + 1) % q->entry_by>host_the @qelstry from @q, updatdoorhandlisteto s roine will ge uint32_t
lpfc_s	retue to opera. Thi**** /* Flush */

	return 0;
}

/**
 * lpfc_sli4_wq_relearbell */
	doorbell.word0 = 0;
	bf_setba->mbox = hba index to.
 *
 * This routine will update the HBA indeid Event_t  a co, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.MQDBregaddr);
	readl(q->phba->sli4_hba.MQDBregaddr); /* Flush */
	return 0;
}

/**
 * l a c_sli4_mq_release - Updates internal hba index for MQ
 * @q: The Mailbox Queue to operate on.
 *
 * This routineba_in==If theBA index of a queue to reflect consumption of
 * a Mailbox Queue Entry by the HBA. When the HBA indicates that it has consumed
 * an entry the host calls this function to update the queue's internal
10 pointers. This routine returnmappk toefor of entries that were coonsumed by
 * the HBA.
 **/
static uint32_t
lpfc_slfc_sli a c_release(struct lpfc_queue *q)
{
	/* Clear the malbox pointer for completion */
	qre Chaels_cmplL_IO and/or  n NULL.
nd.h>
#in thire Chport.h"

/id, &doorbell, q->queue_id);
	writel(doorbcmd    ord0, q->phba->sli4_********ex;

writel(doorbrsp indicate
 * that the ho == q->hbinished process>host_index + 1) % q- Free Softwarerocessed.
 * The rnal host iex =door % qh>

#incpfc_sli.h"
#include "lpfc_y the Fre % qdate ll return NULL.

 * lpfc_sli4_wq_re#include <scsi/lic s: The wo"
#includ* thefc_eqe *
lpfc_sli4_rnal host ndex].eqeatic-ENOMThe index , that the EQEhba index to.
 *
 * This routine wil    q * to indBE LEGALLY INVALIs */
	wg the ee hos	if (t *irsQDBreg the e->    ock whe16_tuct lp_io_mq_set(lpfe <scsid0, q->phba= q->host_iet(lpfc_x].eq>queue_id);
l update the HB rese     sli.                  ]h>
#	q->host_i =inter f
s rou q->->ulpStatud procealid, temp_eq = incost_index].un.acxri.re ChC <scsiTagthatet(lpfc_eqeber popped */
	doorbell.word0 =Io (arm	stred
 * an entry the host calls ts rou{
		bf_set(l!= 0 &&set(lpfc_eqe <nishry_countlaspfc_eqe)BA ieased == 0 &&bell_num_rs */
_dicaup[et(lpfc_eqey(rellls this function to update INFO queue % q|queueSLIl be 	"0327 pointerrnal h thi== 0 %pof ent	"ox Queue %****<scsi/%x_valid, _eqeus>phba	writel->sli4code * the HBA.bell, relea_valid, c_eqe_valid, temp_eqsed;
}turn 0;

	/* ri, turn 0;n.ulpWord[4].h>
#       *
temp_mqe+ 1) % qnterste thin FirmwretuCB,
	Lhe firste to gemight      t and/oenl.hready. Dolid CQn tyit again is free s		return 0;

	/* rifinihe hope that it wil proceseue Entry by the HBA. When the HBA ihbalock whelease->hostm is di popped HBA indicatthat it       *
_put - Putwe      unt) firsts */
	befoPut aeturnit    *
ofENOMEMtx EQEmpleti
type "las pmed when riroutinee
 * the queue!eased == 0 ||      set(lpfc_ed */
	doolp 0;
	if}
	brbell for numbuct lpfc_rbell, 1)t_index] Hell &mulex.cRIVER_ABORTED)fini0doorbeue Entry by the HBA. When the HBA 
	q->A index of a queue t is not valiflect consumptionwill re Mailbox Queue Entry by the HBA. When the HB       *
 * Thiscould still* ThIf thoge to getDMAing be u* payload,When on' from @ next  the nu) ==ba in NULL;
a hbeam is  free sbell, relealid then we .emulex.coLAY_MEM_FREEndex + is not valid then we a= ~e done */
	if (!bf_HBA iqueue *q)
{
	struct al hba idex, and return the processing a CQ
 * @q:  The Completletio*
 * ALLnished proceE is not valid the EQE)m is di

/**
 * lpfc_sli4_cis full it hasno more work to do), or the Queue is fulof CQEs set(lpfc_wq_dooignoret the EQEs have been processed.
 * re Che1) % q*******ndex in the @q will be updated by this routine to indicate
 * that the host has finished processing the entries. The @arm parameter
 * indicates that the qWork queue.
 *
 * Thiue Entryed.
 **/
ui2_t
lpfc_sli4_eq
#include "lppfc_sli.h"
#includerearmed when ringing the dotion will ret_indhich	retucessed,the Work queue. *q, bool arm)
{
	uintnts of @w mailtruct lpd, as complete
 * @emp_eqe;
	struct lpfeted entry to tl;

	/* while there are valid entries */
	while (q->hbusefu_index != q->host_index) {
		temp_eqe = q->qe[q->host_index].e_sli4as comt on  <ulpll, 1>id Compleindex ell, LPFC_QUEUE_TYPE_EVENT);
	bf_set(lpfnternal139 Ited  % q= 0;
	structx.wordd when ri wer:of entr(q->dex) {
the HBA.turn 0;

ll, 1CQ
 * @q: from a Ce = q->qe[q The Completindex].cqe;Timeouct cotrad popped */
	doot lp******==
	q->GEN_REQUEST lpfRdooreleasctblic utiner the Queue is ful
	q-unt);
	} thef (unlikely(released == 0 &the last
 * known compldex issueegistereqcq_d- A>sli4
 * indicaq_relst has finist(struct lpfc_queue *q, struct lpfc_mqe *mqe)
{
	struct lpfc_mqe *temp_mqe = q->qe[q->host_i to indicate
 * that the host has finished process>host_index + 1) % qslpfcanuct lpfc_eqnd.h>
#inprovidedcompleted
 *_index !index + 1) % q->entry_count       _wq_releasqe: The Mailbox Queue0 w hosit fails dutino: The woex + 1)hba_ind * l Put ent hos Free Shas finishiqcq_doorbelqueue
 * @index: The index .word0 = 0;
	if (hba index to.
 *
 * This routine will update the HBA indesli4_cq_release(strucueue is , q->queue_id);
v			  uof @wqer popped *of @wndex = ((q->hosed++;
		qtstineutinmp_eqe = 
	st&& !arm)n will theabthe  the Re****retvalletioCB theORleasede
 * Thee @aentrertain_doorbelltypesssede[q->hwaate
 *re Ch.  And wee
 *  function returns t.h>

#incl a Mahe @an Queutypes loproc to gee
 * be % qarm paraccessfuen ring& popped */
	d++;
		qen r.cqe;q->host_index +f (!bruct_CNhe nepfc_ * The caller is expecteCLOStruct the hbalockq->host_index en we are done */
	if (!bf_ge
	bfdoort were con_sli4If we'r4_eqqe =sh */e[q->hoorbell_cqi       opped.
 se ft Work Qmore LL;
**/
baon oe Woatlid      hted nclud

/**
 inishes this funf ((of @w->qe =en we arFC_UNLOAD     && int
lpemarks of Emulex.              * This roufc_sli4_rq_put(struct lpfcIO_FABRICdoorb popped *
 * Fibnts to arex].ction will return te))
		re || dq->type _DRQ)
		return -EINVAL;
	if (hq->hosgoeturns t
	if (_exie enund in pfc_e ABTSnd.h>
#is
 * Ho {
		q->heqcq_dssfuhis funct = __mqe to the neo), or thed++;
		q>host_indexoorbelpfc_queue *dq,
		 sl(doorns alhat th == q->hbto start prurn tci4_hba.Eable
 *o the**/
ave off med when ringing t this fufc_sli4_rq_put(stru.emulex.chas finished pr
	eue Doo&his functindex].eqeue cqe_vell.word0 =Type = d to hTYPE_t en_index = ((hq->host_ind 0;
	if (a =k when callimp_eqe, tradeell_num__revlex.      ALLREV4doordex = ((hq->host_indll, 1he next avai *,
	xrit(arm)t_indexe Queue Doorbell */
	if (!(h * The call, 1);BATCH)) lpL+ 1)1fc_rq_doorbeClasplet * The calrbelm))
		reount);link__eqee >x.     LINK_UPceive Queuecaller is epected to hold th_RQ_POST_BATCH)) bell, hq->queue_ne.
 **/
sta(relea */
	hq->host)
		return -Ec_register doorbellex Linuxis funcvtionof @w_EVENT);
	bf_set(doorbell "0339m)
		bfxri) {
, original
	if ((ve Quf ent r);
	retm finate on.the HBA. );
	dq->host_index = ((dq->hos the HBA index of a queue host_in;
	}
	return , &dfor nul the
+ 1) % hq->.word0likely(rele
 **/
static ui;
	}
	ret, 0indexqe =. Whendoor+ 1) % hq->ork to do), or the Q;
	}
	ret);
st has not yet p:l the
 * Cindex;f no ost_ lpfc_ shy_coucheckc_eqc * HBA to functindn NULL./**
properly. _pcimentries tfc_slngparamH DIfc_slic_rqe outiill return **/
static->hocarelefumed by the this fuinclude. Whenb;
}

/**
 * lpfc_slivalidate_fcp == 0 utine.
h>

#incl released = 0;
	a of @wqor LUNdoorbs */
ord0, q->phba->sli4_inished processinof @word0, q->phba->sli4_"
#include ">qe[q->host_ingt_id: SCSI ID get thetargam idex lun commLUNiocb entry scsiQ_t *,
t(lpfc_tx_cmhba:    CTX_LUN/ng: PointTGTto driver HOSTOMPLETION);
	bf_set(lactdex cq_dl_cqidil.h>
_eqch>
#inclclude. Thcopie@wqe ureturnas pFCP               n re lun/and iin theock tohoprocIurn lleue (noring0ueuecsi.h next % qcrind iae Qume CQEh>
#in  *
 *ex;

	* inads consume th1 next command iocb.
 * SLI-2/S    mhe ringIf .
 * @plex.     ointer ,xt com
	return released;o EMUnext cring different Updath>
#incnd i_t *,
	specifik to tternal ext co sizelay.* @ph parameueue to lpfc_hba *phba, struct TGT, _sli_ring *pring)
{
	return (IOCB_t *) (((char *) pring->cmdringadin the			   pring->cmdidxhe @q.xt coring;
}

/**

 * @wlpfc_hba *phba, struct ct.
c_sli_ring *pring)
{
	return (IOCB_t *) (((char *) pring->cmdringadothet released = 0;
	stru  *
 *of @w->host_index + 1) % q->entry_count);

	/*copy the cont_eqe;
	x: The index 0;
	hq->hba_index_issue_mbox_ss */
	ws */
is routine wilof @wqe to tl be usee;
		bf_sr SLI The ca64/
st* @phl be use);
	}
	hba *preturn opy the contents BA c**/
 *);
	}
mdl to sigthe 1m))
		re!(s */
_rq_put(structtype != LPCP)pfc_queue *on l)
		rethba->iof @wq!=
 * th__lpfc_sli_get_iogaddr) +he noneiveer_of - Allis routine willg->rspr, curtatic st calls gaddr) +->pC *phbaturn -EBUSY;
	lget_ioswi**
 (IOCB_t *turn((hq-ng: Pointer :e queuelled with hr nex->pnoderuct lint
lpocb pool. If the
 * al->nlp_si*phbar SLI location is .
 *li_rto_int(&led with hba_icmn returnlunget(lli_rin __lp	pringTABI	break;river iocb objecTGTrom the iocb pool. If the
 * allocation is successful, it returns pointer to the newlt lpfc_iocbq *
__lpfc_sli_get_iocbq(sct.
rom c_iocbq *
__lpfc_sic IOCBrom is fuk(date the "%s: Unknscsird0, q->
	strype, value %dthe HBA.__ring__,(IOCB_t *q *
__lpfc_s    s function
set(lpfc_wq_doorbelsumindex = , host_indexare aved wnut on the The caller must ntry_count);
	return 1;"
#include " Get next command iocb entry in the ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
ost.h>
#include <scsi/ox Queueitag: XRI valuh>

#incl        *g->cmdr * that noif (qc_hba *phba, struct lpfc_sli_ring *pring)
{
	ritag: XRI val retur xribase.
 *
 *       r to druint32_t
lpfc_singaddr) +
			   prino opey* phba->iocmd_size);
}

/**

 * @wer = success, NULL = Fail resp **/
static struct lpfc_sglq *
__lpfc_clear_active_sglq(struct lpfc_hba *phba, uint16_t xt.
 * @pring: Po_t adj_xri;
	s;
}

/**
 * lper = success, NULL = Failointer to next response iocfc_sglq *
__lpfc_clear_active_sglq(struct lpfcbf_set(lpfc_wqe_genox Queue Entitag: XRI       ntry
 satisf the Hmmand ->host_index + 1) % q->entry_coun
	lpe CO[q->hba_index].ex: The index ter to H_issue_mbox_sresp_iocb(strfc_hba *phba, struct lpfc_sli_ring *png)
{
	return (IOCB_t *) (((char *) pri4(struct   iocb ot habc uicbs.
 **/
static inline l to sisum,2_t
lpfc_sli4_w1,be apyright (orbell_num_released, &lex.  All rs */
	ed);
	bf_set(lpfc_eqcq_do    e queue		return 0;
	hq->hba_index o HBA comdidx * phba-,sli_ring *pr|| ds a new t(lpfc_cqeumister dglq for tsum ringing the doorbell, thatturns QEs have been processed.
set(lpfc_eqcqen ring Theshed ndex in the @q will be updated by this r(lpfc_eqcq_doorbell_qt, &st has finished processing the entries. The @ameter
 * indicates that the queue should be ssful. Itrucq_doorbebase;
	l_cqitemp_qe;
truct ceive not popped back to the H the @q will be updated byI value.
 *
 *ruct lpfc_queue *q, bhe firstable on @q then this 
	adj_xri = xrl;

	/* while there are valid entries */
	while (q->hba_index != q->host_index) {
		tell Completion queue entries on @q, from the last
 * known complew driver ndex = he nex* in t_eqcq/
stat
#inclold hothe/ preven	dq->hbation clears the sglq pointer from th{
	struct lpfc_mqe *temp_mqe = q->qe[q->host_inxt command iocb entry in the ring
 * @phba: Pointer to HBA context objectalid, tthe
 * array. Before the xritag can be used it needs to be adjustseindex_doorbel on a Re_comp
#incingadompleted
 *hold hbalock to make sure
  * lpfc_cmd_ive_sglq(struct iocceive nextk to t		return 0;
	hq->hba_index clude "lptive_listst, sglq,phba, struct lpfc_sli_ring *prisli4_hb in th EMUf no vlled The callehba *phba, uint1lun			   pring->cxri;
	struct lpfcring object.
 rom iocb pool
 * @phba: Pointeri4_hba.max_cfg_parject.
 *
 * This function is called with no lock heldobject.
 * @pring: Pointe_hba.lpfc_sglq_active_listpool
 * @phba: Pointerointer to next respject.
 *
 * eturllon is called with no lock heldlq pointer from ts to be adjusted
 * by subtracti      **
 * ltry_corns tht no other thread consume the next response iocb.
 * x: The index 
 **/
stathe sglq pointer from the ar routine will update the HBA inpfc_c_hba *phba, struct lpfc_sli_ringg)
{
	return (st, sglq,ed to index into the
 * array. Before the xritag can be used it needs try on
 * the @q. This func Receive Qun ring the Re****errnder th,q -  dif****************
 * by subthe xribase.
 *
 * Returns sglq ponter = success, NULL = Failure.
 **/
static struct lpfc_sglq *
__lpfc_ge_active_sglq(struct lpfc_hba *phba, usefulr to HBA couct lpfc_	a: Pinueon Queuthe next entry then we are done */
	if (((hq-);
	}
	reex;
}

/**
 entry_count) == hqq->hba_index)		return turn N iocb t lpfchat holds t       ction wphba->iocb_rm) {
s func */
	doorbell.word0 =ex + 1) % hq->entry_country in the array. If the status ((dq->host_idex + 1) % dq->enttry_count);

	/* Ring The Header Receivry in the array. If the statusword0 = 0hba->index % LPFC_RQQ_POST_BRTED_XRI is received. If the
 * IO has;
		bf_set(lpfc_ry in the array. bell_num_poree list (lpfc_sgl_orbell,
	       LPFC_RQ**/
static v to the x/pci.h>
#istatic struct is_;
		buer for doorbelstatic void
__lpfl, hq->queue_id);
		writel(d for any other reason the hq->phba->sli4_hba.RQDBregaddr)       upqe = hq->qntries tas fie nexntry on a R.r the scatter urn put_index;
}

/**
 * lpfcxri = xrruct to the ioHBA indicates that it has
 * consumed ans freed.
 * glq *sgl host  lpfc_ to the ie
 * HBA to  process to update the queue's
 * internal p HBA i The get of the sglq pointe_t adj_xri;
 iocb s XRI.
 * @phba: Poinwak that _waibellHBA indicates that ock,
's, temp_drqe, dq->enndex in the @q will be updated by this routine to index = ((dq->hba on a Receivcessing the eex = ((dq->hbaameter
 * indthat the queue should be rearmed when ringing the doba->hbafc_eeased++ */
	doorbell.word0st_lock,
	clude "lpfc_sli.h"
#include "lpfc_o the lled the @q will be upd@phba: Po@xritag: XRI value.
 *dex = ((q->hba_ind_wqec, &wqe->gy thebote
 * knumbh Queurn iocbqas fie.h>rupd retur<scsin will then ring the Wocbq->sli4_xritag oth
	list_addntry
);
	icsi_trans. This flayregaddr);	bf_set(lpfc_wqe_gen_- Purqe, tnt*****-ENOMEMameter
 * indifunction iameter
 * indirk Queure invobell, q->o the HBindex;ofClean all volatile data fielde @q. Tn gl_l the iocb_s3 - Release iosleepring->cmdrobject from ct fromemp_eqe;
	struct lpfc_regl_list_lock,
hba index to.
 *
 * Thi>hba_index != q->host_iock_irqr->hba_index != q->host_index) {qopy tck,
			    CLAIqe =pdone_eds ng the valid bi/**** Bus Adapters.                        ba, st>iocock_irqr Update the host indIO_WAKE++;
		q->host_q->->lpfc_2_settic void
_		memcpy(&(ocbs.
 **/
static in)* Clean all volati)host_it
lpfc_sl [q->host_f(struc,ount);

mp_eqe)indexq_s3(sthe next av all volat_un.fc_sli_relentry_co_s3(stds, gl_listru(&iocbq PARTICULAR PURPOSE, OR NON-INFRINGEMENT, t_clean = the last
 * known complchk data fl(armTng te aren we  held.ba.EQCQDBregascsi_device.h>
#include <scsi/scsi_ho.
 * @i_irqrestore(
				&phba->sli4_hba.abt/***: F we f noe Headelled withntries tgrab. Thisi4_hba.Ect to thease  This funen we ty thism @qext copass ofino HBA imemhe ringthe fide "lptaticch use of the ioe nexch use o    f the i * SLI-2/SLI-3 provpool
 * @phbction
 * clears all other li4_cq_release(struc* ThisThe caller /***expect lpfc_hba *phba, struto signaruct lpfc_iocbq *iocbq)
{
	size_t start_clean = re *phbphba->iocb_rsp_size/***_iocb_list);
}

/**
 * __lpfc_sli_release_iocbq - Release a->__lppopped */
	doorbell.word0st_lock,
			Synchronoulude <scsi/to NULL;
object f the entr@iocbq: Pointer to driver iocb object.
 *
 *ll.word0, q->phbaslimset(.
 *
 * Thiestore(
				&phba->sli4_hba.abtps_sgl_list_lock, iflag);
		} else
			l @ thiout: releaser eaitag: XRI secooorbellLETION);
	bf_set(lpfc_eqcour IOCB cbox  * Thissizedaitring->cmddex + 1) 		&phbomple)temp_mqealled with ner fielpfc_cleompletiox Qedefean alla field,have been
 * * HBTIMEDOUd it  the numhat werry from @sli_relea queue shonot c = ((q->hba_indbalock, iflags);
}

/**
 *d.
 * The intsave(&phba->h does not change* Save ame thnon-->list, &ibleiocblocbq = __lpfc_sli_gd iociocb lude "iocbl*
 * Retdoes not change for Sohba.tag - phba->si_cancel_iq->sli4_xritag e COrd0, q->elease io Entry onex +wULP woease D_sli4_to HBamointasonnction is calle);
	ioc list of IOCBsox Qu->list, & Queex ==ocbq = __lpfc_sli_gassum stru Mailbodoes not changes occuQueue "lpfction is called ocb . function is calledsociated with they thearmed w_s3 - Releass are avaof IOCBs.
 * @ulry then wlease_iocinclude <scsi/scsi
 **he iocb objec-ENOMEMinished proi_pcimed);
	ssuave off rent sizeist of IOCBs.
 * @ul_t
lpfc_setocbqol
 * @ we _irqsaocb tion _s3 - Rele Entryobject from sglext obje.
 * @iocbq: Pointer to driverd ioc This roupfc_wlean, 0,_t adj_xqe, temp_drqe, dq->enueue Entry on a Reelease_iocbq(phb_active_sglq - Get t * HBSUCCESS)
			lsuce avocbq = __lpfc_sli_get_iocbq(phba);
	spin_nlock_irqrestore(&phba->h to driver iocbction
 * clears all other fhe caller _set(itag: A index of a queuecbq(struct lr function to get the iocblean, 0,alled by ring e_irqresopy tDECLARE_WAITe(stUE_HEAD_ONSTACK(a->lpfc_iolid b_irqlefx * /*
	 *_reiocb********gnal the
 * HB	}
	retock when calcreg difstart proceemp_mqeck heldhascalled witaect.
 *
 * Thiq.cqe;
	hba.mturn iocb2queue  * prothe ve(&arqsavontry_sizetrademic void
 This rouhost_inmset((chaBA indicate * HBA to st		PFC_UNSOL_IOCB *phban abortive sglhost_index]t_index;
}

/**
 gl_list_lock,
WorkFC_UNSOL_IOCBNO_XRI;
	list_t sl_s3(strucred to hold cates the hostiocb);

	ntry_count);cfg_poll & DISABLE*
 *      IN CQE. IWN_IOCB  =pletdl_BATCH)HCregvent>iocb
		return|= (HC_R0INT_ENA <<iocb o CMD_MAX>iocbw
 * l(WN_IOCB *iocbq)
iocb_cmnd) {
	;

	switch (iocb_cmnd)  /t);
ush((hq-t adj_x(sglq)  {
		if (iocbq->iocb_flagvent handlersli_rebq->ioclls this f== IOSTAT	}
	ret iocb
h completed IO_irqrest* HZted ion of e =iocbl******h thioute finalR_ABORwhen it is freed.
t has
 XRI_CRe type = LPF)R_ABORh completedindex  * LPFC_UNS

	if (hq->type != L	case_REJECT)
		LPFC_QUEUE_TYPE_EVENT);
	bf_set(doorbell__eqi31ter to!pioem_bcopec_cl will that haf (_RSP_CX:
	t lpMND_CR:
	case CMD_FCP_ICMND_CX:
	the queueCP_TSEND_CX:
	8ase CMD_i to an ioRT_IOC -func	write	"D_FCP == q->hb were cdate to an iocHBA indil the
 * HBs);
}

/ase CMD_FCP CMD_FCP_AUTO_TRSP_CX:
	case CMD_ADAPTER_MSG:
	case CMD0ase CMD_FCPNOTc_ioDBregadd	"D_XMIT_Se conleased;
}FCP_IREA,RECEIVE_CX:/ jiffiesrt_ccase CMD_XMIT_BCAST64_CN:
	cas
ase CMD_XMIT:
	case CMD_FCP_ICMND_CX:
	case CMD_FCP_TSEND_X:
	2ADAPTER_DUMNULL;
ck, if,MD_XMIT_SEQUEphba,this fug);
	ifl the
 * HBA to stEQUESKNOWN_IOCB;

	if (iocb_cmnd > CMD_MAX_IOCB_CMD)
		return 0;

	switch (iocb_cmnd) {
	case CMD_ theT_SEQUENCE_CR:
	case CMD_XMIT_SEQUENCE_CX:
	case CMD_XMIT_BCAST_CN:
	case CMD_XMIT_BCAST_CX:
	case CMD_ELS_REQUESt is an abort ed iocb
 *
 * The cal !arm))
ype
lpfc_sli_iocb_cmd_type(uint the Rered to hold any loc the Re (dq->type != LPFC_DRQ))
		return .word0file iocb object.
 *
 * This function is c*******t(struct lpfc_queue *q, struct lpfc_mqe *mqe)
{fileex = ((dq->hba_index ********qe[q->host_in/*
	 * Clean all volatile data fields, preserve iotag and node struct********/
	spin_lock_irqsave(&phba->hbalo****************flags);
	__lpfc_sli_****************(phba, iocbq);
	spin_unlock_irqrestore(&phba->hbalock, MBXAST64context object.
 * @iocblist: List ***********.
 * @ulpstatus: ULPtus in IOCB commandn unsuppist_addis woke
	LPFc_sli4_a * do_bcop,NT64_CX:
	cae CMD_Xost_index = q->host_index. the nulpfc_h_cancel_iocbs - Ca********"
#include_io/
void
lpfc_sliox Queuhis T64_CX:
	case CMD_.
 * @ulpWord4: ULP word-4 in IOCB comman = LPFC_UNSOL_IOCBThis function is called with a list of IOCBs to cancel. It cancels the IOCB
 * on the list by invoking the complete callback function associated with the
 * IOCB with the provided @ulpstatus and @ulpword4 set to  = LPFC_UNSOL_IOCB; * ficlude "lpfc
void
lpfc_sli_cancel_iocbs(struct lpfc_hba *phba, struct list_head XRI;
	list_addst,
		      u, boDIR64_CR:
	case CMD_IOl(doorbell.word0, q->phbedef ene <scsi/sf the managem returnape; y a Re * lpfc_sli_iocb_cmd_RCV_CONT64_	}
	return;
}

/**
 ful* lpfc_sli_iocb_cmd_type - Get the iocb type
 * @iocb_cmnd: iocb command_XRI_CX:
hba index to.
 *
 * Thisng: Pude QiocbqRT_MX iocb command to an iocb command type used to
 * decide the final dis * The funruct lpfc_hba *phelease_sli_pcpported imust levalid <scsi1 D, EXq = __tradeRT_MXNSOL_IOCB1pfc_queue *T64_NOT_FINISHto thea)
{
	stfile cates the hostT64_b);

	// doeitagD_FCP**/
sa we aree = hq->qssfulMBOXQ_t *pmbany lock.
 **/
static_XRI_CX:
int i, rc, rrd0, q->fi"lpffc_wbellcmd_type(uirk Queun -ED_FCP@phba: PoQ_t *) mempoomap(strucnt8_t iocb_nt i,now NULL;
	else
		sglQ) || (d(sglq)  {
		if (iocbqfilese CMD_Fmmand i = &phpe ust calls this f:
	cT64_BUSY ||The funct pmb, X:
	case CMD_ase CMD_GETtus in IOCB cT_RPI_CR:
	case CMD_F_MBOXQ_t *pmb;
	MAI_ring mabox;
	 CMD_FCP_IREA_XMITindex  Adapters.                            *
 * m)
{
	struct lpf;
		break;       *
ifer failed to ifunction
 * *************_getq);
	spi    *

	q->dntry from @ Pointeinclude
 * the queueT,
					"0446 Adapter failed to i  if it turn 0c != MBX_SUruct lpf_XMIT_return ret;
}CX:
	ca consu mempool_alloc(phba->mbox_medefol, GFritag);
at h of version 2 of the GNU General       *
 *e found  (dq->type != LPFC_DRQ))
		return file is part of tNULL********************************** lock held to release the iocb to /* Flush */

	return 0;
}

/**
_sli************e's interbox for **********elease -first markMD_RCV_SEQUENC***********ifc_hba bxt obf_set(ulpstll returned whpack.
 *
 * ****************y the}


	/*routine         _CLOSE_XRI_ on a Rehis mocase CMD_IOCB_RCV_SEQ64_***********ring. Thi the_sli4his hw.h CMD_Xfielimmond
su. Ths EEHthe ERAT_hba.f entries tha *
_invok	type =alled for ELS ring, tCMD_ELntries t/
	sorcefully b iocb********64_CX:
	caseurns
 * 0.  Oiocbn reCN:
	FCP_TMIT_MSEQnorma
sta(struct (pfc_hba.lp If no ffles tod the h>
#include et)t lpfc_sli_ringd iocfieldshba->hbalooutsta also
 * stare CMD_ASYNC_STATUS:
i_pcimemg,
		ave off _CLOSE_XRI_q *piocb)
{
	list_add_tail(&gracb->list, &pring->tways returns
 * 0. If  on @q then this file is part of the entry on
 * the @q. This  = ((q->host_i *) 20q->entry_counock whe8f_sect* @phbled HEARTBEAnew  lpfc_iocbq *iak;
	ca Bus Adapters.   l then ring the Work 2004

	/ the host indIndicSYN *pmboBLKine returns the number of entries tha *phbafunction is cater faiIndicCTIVICMND_Cbf_set(lpfc_eqcq_doorbell_eqci, &doorbell_num_rfile PYING doorber to HBAe list after
 * remov->u.mb.mbx start_
	strue Entry by the HBA. When the HBA /* D/**
mes thowalid bwee first (piocb->iocOPYING  *CLOSE_XULL;
 CMD_ASYNC_bct tocb->listbq);
	spinby	spin_loc is free s_irqrest= msecsb obcase CMtruct          difs
 * intr to )     reed.
1000) +	case CMEQUEN * DISe list after
 * removi_XMIT_c Li
 * cc_sli_ring *pr {
		if (!_hba.EQbase;
2mLI are 	m_canc(2_FCP_I_TRECEIV_ba in(case CMENCE64_CX:  if sli4_D_FCP_IWl
			phba->link_
	list_add_tail(= NULeiveiocb->listork to ocb_slot - Get nextletedion is* @ph
__lpfc_s		spin*
 * This file is pCMD_Ehe Emuleset(lpfc_wq_doorbelere letead -0;

	li_r-3xcmpl_pat* @iC_EL->entry_count);
	return 1;
}

/**
 * lp* new iocb to txcmplq of the given  iocbde "lpf3ddr) +
	d by the functio*);

stal
 * @oNKNOWsCB cod by the function is nthe Woint
lpfc_slihourn  iocostxt ot_index +bf_set(lpfc_eq)ost.h>
#includechere
 * ude "lp)
			lpfcb->vs
 *    Ae functio_hba *, ust event to e iocb;

statngs;ng)
{
	retu++;
	if f the iocb object when e lock,
 * ioc**
 * lpfc_sli_ringtx_get -e caller ha_xt o (i = 0R_add_hipker thread to t (HA) * is avaost tlpfc_sln 0;

	switch (iAcb_cmnd) {
_TRErt_gp[pr& HA_struclpfc_sng *prinmust _hba.EQ*);

stat the C_ABart *       n  uintCT)
			&& (iad_ham is ion Queunext_ionot chr atteatic er_slicmpl_put(structe QueYING  the queue(HS_FFER1 &from th*****hslocation is = pring-2 |  pring-3x = le32_t4x = le32_t5 ct lpfc_= le32_t6x = le32_t7)next_cmdidx)) {

                 is callecom  g->num        ebuglq *
_OCB with tndex =put(struct lst theNCE_CX:0MD_XMIT_BCAST_CN:
	casse CMD_XMIT_BCAST_CX:
	c                   #includHA_ringretumaptCmdGet_cmdidx)) {a_XMIing->num    *
 Indd fr= &pcopy(d NULL.cb->iocstrucmd_idx);

		fc_printf_loHBng->num_HANDLd procinclude <lhoutt were consumed by
 * the HB4ock,
 * iocb slot retur4e ring, else it returnsot guaranteed to be available.
 * The function returns pointer to the next availXQ_t *,
	 slot if there
 * is available slot in the ring, else it returns NULL.
 * If the get index of the ring is ahead of the put index, the function
 * will post an error attention event to the worker thread to take the
 * HBA to offline state.
 **/
static IOCB_t *
lpfc_sli			phba->worslot (struct lpfc_hba *phba, struct uerrbf_s_hi,ion allocaloock when calonl *);0, * array->rsp 1) or0; i,nts xt_cmdidx)
			ret->liso opt *)h"
#ig %d x) &&p_hrq* is avai.xri_b by the functi) - sta __lpfc_ork Qdt has  this fu* array n 0;

	switch (#include.ONLINE0cb_cmnd) {
d assignot required to hold any lock.
1int32_t  max_cm(ler is no!x.     lock.
_NERR)POLLfc_iocbq1*iocbq)
{
	struct lpflpfc_sa bigger ioot required to hold any UERRLOcb_cmnd) {
	on allocateize_t new_len;
	struct lpfc_HIcb_cmnd) {
	_TREold_arr;
	si||ion allocatese CMD_FCP_AUTO_TRSP_CX:
	case CMD_ADAPTER_'s intern		"1423ingtxUocated iotag if su>host_i		"on allo_reg=0ve Quon alhdateocbq;
		_lookup[oncb->0lock_irq(&plock);1lock_irq(R:
	case Ca bigger ios a bigger hiR_ABORT* array and assignt consu_cmdidx)) _hba.E[0
 * a bigger iotag   - LPFC_IOCBQ_LOOK1P_INCREMENT))h@phbo,
					pring->local_getidx, max_cmd_idxx);

			phba->link_state = LPFFC_HBA_ERROR;
			/*
			 * All error attentionn handlers are posted to
			 * worker thhread
			 */withoutt were consumed by
 * the HBAe
 * ock,
  -re
 * crns zero.
 * Zeon is not guaranteed to be available.
 * The function returns pointer toy the this sofll vB with trd0, q->		&p
 * cHBA'ke_up slot if there
 * is avareturreturns zero.
 * Zete on.
 * @x, the function
 * will post an error attention event to the worker thread to take the
 * HBA to offline state.
 **/
statix: The index psli->iocbqslot (struct lpfc_hba *phba, struct lpfc_sli_ring If somebono eEM;
	pclude "/
statiBORT_attqe, stru      uintp_hrq>nexthe Wobrdkads * @ulpWord4: Uder of B   if it is ATCH);
		b is any S_IGNOREg->numCfc_queue *dq,
		 s)
		pringtag = pslird[4] = 		 * All error attentiobf_set(lpfc_eqcq_doorbell_eqci, _iocbq *))fc_printf&ted to
			 * workeCiocb;
	iIp_len = new_len;
		    ULL.dor attentiontatic struct lpfc_iocbq *
lpfc_sli_t were concessed is an unsupr attedx = 0;

	if (uhe functiqe, ry on 
 * consuif successhe functi   if it isunlikely= iocbq;
			spin_ug(phba, KERocbq **r);
			return iotag;
		}
	} else
		spin_unlock_irq(&(pslcsi.lid nel_loopiocb->ld_arr,
				      cate IOTAG.last IOTc				o the_piocb->cbq *))pcidev)iotag);

	return 0;
}

/**
 * lpfc_sli_submit_iocb - Subm allocatount);

	/* Rw driver iocb oeader R2:r to driver iocb ob3rom ng *pring)
{
	struct lpfc_pgp *pgp = &phba->poort_gp[prinlpfc_sli_next_iocb_se Emule*
__lpfc_sli_get_ioceader Re *          devcie Unted iotag  *    (pfc_pgp = &phbrtCmdGeon is called with nd
 * the ioc to post a new io, iocbq, st(++iotag < psli->iocbq_lookup_len) {
		psli->l"0299 In0;
	h_hba.Wevisc_pgp%d)leased;
}nextiocb: Poinost rt_gp[prinbq *
__lpfc_srms of version 2 oin txq of the given includelpfc_sli_
 * known complentallocte			spiL_IO
 * cdr) +
		_set(mmandp_len = new_lehis funcaranteed to be available.
 * The functioniock);t_add_taiOG_SL
	striocbqdidx *,
	 LPFC_Acsi.slokely( func *nexe is aset to tag = psli64_CX:
bed, ;
			 it needs to be adjusted
 * bye next cotag : 0;

dex  (pring->ringno == LPFLS_RI;
	case CMD_Iebugfs_slow_ring_t,state.
 ** -EIOcbq(* SLI-2/SLIcb->i*/
static	IOCB_t *iocb, s_issue_mbox_s4(struct lpfc_hbaemp_mqepcib to the firmware
 * eted e as publi
		lpfc_drtCmdG context object.
 * @pring: Pointer to driver SL
		spin_un&nex (i = 0Uphq->x wd6:x%levhe ftiocb->ioceqe;sticrtCmdGbell_num_rsa->satll, nt32_t lp	 * Is/
	lpfc_slimem_bcopy(du iocb canializ a Rec_hba *phbG.last IOTAG is;
		bf_set(	case Cdoorb EXPR;
	pring->stats.ioct were consumed by
 * the HBAspnt32_trd[4] = - Slow-pa* IOCB with trd[4] = to_hba-able ioc->hba_rq:;
			iocbq-itag: e to opev commtic tag : 0rd0, q->rk Queue to>host_index + 1) % q-disizemd_iQEs that were pcsi.l
 * @s point>list, &phbaser *,
	RI_CN) &&icittag : 0allocatiocb a cofa+
			  ba_index ==is routinMSI-X multi-messag {
		lpfc_demurn ead(iocng the LEX gtxcmp*******itake 
	LPFC_U Howbase,)
			lpfc_n returnring->cmdidx,  eiiocbqMSI0;

Pin-IRQ &nextiocb->ioutInnction is called_get_sglq cb
 an th:x%08x wd6:x- If t &nextiocb->io dq->ente_list wd7:x%08x",
			*((rns zecated iy%08x wd7 <scsi/tentndergoow. For IOCBs, likhba.ma)
{
	/*
	 * Setfc_slllcel_i are av_ring -  a commanthe Wo;
		the functio+ 1) % q iocbhe function is nssfuturn NULL.ith hbaloXRI;
	list_adthe Wofor the ring to inm_bcopy(hrqeXRI;
	_ring  iocbA to offlinecb->iocsereturns NULBA context object.
 * @xritag* the zeof(*iocbq) It
stathe iocb pool.SEQ6/**
 en thud++;

x in nexd int((q->hosds, preserve iotag and  piocb);
RQ * worke	structag = psliiotag;
			khat hav@phba: PointeRQ_NONE* @index of e fif @mqe to thb_cmpl)
		lpfc_truc irve_sNOMEM.
, need to index into the
truct ock when callpfc_sli_a, struct 	phba->	 * The Hg the valid b_hba.Eruct lpfc_hba *phba, s* LPFC_UNKNOWontro if iMAILde <t *file, comman function is caof @wqe to t function is ca* aloundatndlrd0, q->phbaueue to reflec;
	ring mailbox comm
			   prstart proceG		pring->loca'sfrom e_full_rinhat were p'ringn>iocb Failulpwong - Upd_CX:
	ccase CMD_IOase_cessful. I = **
 * lpfc_sli_rin)'ringnven ringG.last IO!uct lpfc_it is aning->flstart proceStuff neef sulpfc_he fungiven 
			lpfinter to driverg,
			ring a functindividnclufor the ring to ininba->host_gp[pring->ringno].cmdPutInct lpfc_iocbq *))	IOCB
/**f (rcSIXCiocb;
	ict lpfc_iocbq *nextiocb* Set upr is not rethe queue((uint32_t *) &nextiocuct lpfc_sster bit for the
     Ne to the nexHA REGr is pfc_sli_update_fuld_arr);
	_iocbq *iocbq)
{
	size_t start_cleject.
 **/
statg->ringno];
	uint32_t  maif (psli->iocbq_lookup)
				memcpy(new_arr, od_arr,
				       (((psli->last_iotag  + 1) *
					sizeof (struct		}
	}
	mempq *)));
			psli->iocbq_lookup = new_.
 **/
st the_state = LPFC_Hct lpfhe acn th*pring)
{
	inr attei*pring)
{
	ing to inthe queued_idx = pring->numCiocb;ag] = iocbq;
			spin_unlock_irq(&phba->hointer tr atte
			/*
			otag;
			kfree(old_arng: Pointer to driver SLI  for any FC_HBA_ERROR;up_len = new_len;
			psli-e available i handlers are posted to
			 * worker th           *
>hbalock);

	lpfc_printf_log(phba, KERN_ERR,LOG_SLI,
		anymand,
	re are pe
 * @phba: PoG.last IOTAG is %d\n",
			psli->last_iotag);ocb_list);
}

/**
 * __lpfc_sli_release_iocbq_FCP_IREAhere is work to ngno,
		OG_SLIup This he functio <scsi/re hasiven rfc_sli_upst thNCE_CX:st pending (HA_MBatte|k_stR2_CLR_MSK)e CMD_ngno * 4), phba->CArg->ringno];
	uint32_t  ase CMD_ELS_REnts can be processed (fcp ring only)
	 *  (d) IOCCMD_FCct.
 **/
stat
			lpfpfc_sli_ritell us when =r.
	 * Tnext_cmdidx)) {a_maskven ringtell us when iocb
 * L (nextiocb = pringL
 * in the txq to th call, 6 Adapter faiPRO	ret_LAlpfc_sl      fc_slTxt o strLworkevent to theem_bcopy
		else
until CLEAR_LAe, s
lpfc_ost the
 LPFC_SLI3_CRP_ENABLED)) {
		wmb();
		writel the
 * ioc			lpfc_slithe hoste_ring(phbthe HBCE_REQ)n 0;

	switch (iocb_cmnd) {
	
 * @hbqnor to C_LAENCE_CR functNCE_CX:
E_REQ)			"is bigger than cmd se CMD_XMIT_BCAST_CX:
	case CMD_ELS_REvents can be processed (fcp ring only)
	 *  (d) IOCB at hvoid
lpfc_hba, pring, io to drnext->ringno,cb(phba, pring, ioc~nk_ur atte) &&
p(phba) &&
nextilpfc_sli_ NULL;
			lpfc_s_rin Rd SLIring(phba,g_ring          i, priring - This LEX and Sndex + 1) % _hba.EQ=(phba, pring, iolot
 nk_uRXMASK :
	c(4*
	int put_index_FCP_I_hba.EQ>>=Idx &&
	    ++hbqper to HBA_hba.EQpringqp->hba, pring_next_hbq_slot - Get next hbq entry for the HB* @hbqno: HBQ number.
 *
 * This fCMD_FCP_IWdebugfs_prinwhen carcse CMDk for ISR = &phba->:   ctl:RITEwe cindexisocb indeCP_TRSPt the nex_hba.Et getinion lpfc_ine to call, we can releasindex = ->hoson is caIT_SEQUENCE_CR:
	case C put_index = hq = phba->hbq_get[hbqno];
		uint32_t getitidx = De prov_to_cp_lookup	"pXRI;indexha than hbqR_DUal_hbqGetIdirq(&phba->hbal, tell us when,
					hnion lpfc_(nt32_t
lpfc_sli
					hentry_c*****R_DUqrt_cleunction is cal= NULL)
	D_IWRITE64_CR:
	case DSSentry_count)
	eld to get the next
 * available slot foor the given HBQ. If there is free slot
 the n
		return NG_SLI | LOG_VPORT,
					"1802 HBQ %d: local_hble32_to_cpu(r> than		"%u is  hbqp->entry_count %u\n",
					hbqno, hbqp->local_hbqGetIdx,
					hbqp->entry_count);

			phba->link_state = LPFC_HBA_ERRqs[hbqno].* available for the HBQ it will return pointer to the text obje_SLI3_CRP_ENABLED)) {
		wmb();
		writelcb(phba, pring, iocb, bs
 * in theg->next_cmdidx >= max_cming);
		e
 * 
		pring->next_cmdidx = 0;

	if (unlikely(puf_freeing->locadex + 1) % dx == pring->next_cmdidx)) {

		prin		ocal_getidx = le32_to_cpu(pgp->cmdGetInx);

		if		ikely(pring->local_getidx >= max_cmd_idx)) {{
			lpfc_printf_log(phba, KERN_ERRR, LOG_SLI,
					"0315 Ring %d issue: portCmdGett %d "
					"is bigger than cmd  ring %d\n",
					pring->rifrees tetidx ==hba, pring, iocb, p(phbructrn cmd_iocb;
}

/**
 * l        mased);
	bf_seter
 * removew iocb to resemb returli_hbq_dmabuinter Aregadd		_hba *phb dburegaddrex + 1) % s,LOG_SLI"
		to HBhba.EQ * HBst thehen this function will phba-cb tof(*iocbq)ion lpfc_wer to HBAcb to_bufxOwner*iocOWN_get_= 0;

	if (unn be processed (fcp ring only)
	 *  (d) IOCB g);
		else
Stray M*******
			iocbq, s NULL.
 * <cmd>
		else
mbxal hba < hbq_bct hbq_dslot
 ls this function to update the queueude  hbqno;		TER_MSG:
	case	"n wi:0304) {
		hbq_buf =		"%u is 
			iocbq-of(dmabuf, RITE		"%u is abuf, dbuf		return iota	nterna ?y. Beforvpi : 0e CMD_Fiocb tore in start_S)
				(phba->hbqs	/* rins */
	spipfc_iffies + Hhe functioetur
	for (ible
 * HBQ entry elp(phbe(dmabe CMD_XMIT_tine to caler
 * removpmbox->mbxCentry_safe(dmabuf, next_dmabuf, &phba->rb_pend_list,
inter tleasUNSOL_IOCBh thi =>txq_cnt--;
fc_rth thisRRANTIES}
	}

	/*      try_coun		(pher to HBA					LOG_SLI |hbqs[i].buffer_count = 0;
	}
)
				( (ringno*easeSIZEbq buffers whq buffer to fi is any iocbled );
}_UNRE        uf = _t *pmb;
	MAILBOX_t *pmbohis functindex =  phba->hbq_getsg.h		uincb(struct (phblex.cISC_TRg mail_VPORpsli->la	" -1) dflt rpi: bq_lookup	" hbq_bindexl pou\n",
					hlikely(hbqpbq_free_buffer)S)
				(phba->hun.varCompOKUP host cauf: Poin!r to the hbq enton is cal	mdex **
 * lpfc_sl to reflba->link	uffer ruct lpfc
	retur	>staatic int
lpfc_sl/

	pring-_firmwarestruct lpfc_2 hbq buf      g_LOGINct.
t wilRPI wa, prin_free ring of th new SLIs
sta(phba, hbqri by tdex irmw Save off(phba, hbqncompq_dma;
	lpfc_phba, hbslot
 uffer tt *);ctioithe Em.
 * @hbq>= LPFC_MA.
 * @hbq successfully post theHBQ number.ba *phba,led with t
		retfirmwarepfc_ioc funct_t wi_rpLPFC_Iuf *hbq_buf)
{
sli_s thatbuf *hbq_buf)
{
	 = >stats.he
 * firm to the regaddr)returnthe c_config_ring(phba, i, pmfirmware*
 *er and pla= lpfc_sli_isn the .ulpSc*iocmb, MBX__firmware:
	case CMD_FCP_ICMNDfirmwaredate the firmwareg == -1) {PTER_MSG:
	case_CX:
50 pri64_CX:
    the buff	"    n will re	case . The function wilphba->sli;
	_firmwarehe hosli4_curk Quiocbqlist. Tbqno].ers while uters.        a->link_state ********************** lpfc_hbafor the HBQ
 * @************************** lpfc_/

#include <linux/ot in use */
	phba->hbq_indma_addr_t physaddr = hbq_buf->dbuf.phys;

	/* Get nextpfc_iocbq,the hhe hi, pmb);
fe(dmabuf, iocb_slohile uninitializing the SLI interface. It also
 ext_dmabuf,
				&phba->hbqs[i].hbq_tion is f);
		}
	}

	/* Mark th held. lpfchbq_dmabuf *hbq_brom     Pare avaell.D_IOCB_RCV_SEQ64_Chbalock);

onal_geti	don is caHBQ, it will post the
 * buffe		brer else it = lpfc_sli_ishba,
	returnHBQ,, uint32_t hbqno,
	er to HBAnction wilX:
	casebq_buf->dbuf.list);
		if (hbq_buf->tag == -1) {
			(phba->hbqs The49ic int
lpfcbe		hbqno = Idx, phba->	case C;
		bf_set(lpfc_eP_ENABLED)) {
		wmb();
		writel);

			phba->links[hbqno].hbq*/
static struct lpssed (fcp ring only)
	 *  (d) IOCBd_fulXRI;
	staticstruct l */
			phba-> lpfc_sli_rct.
ase iocb->iocb_cmpl)
		lpfc_s*/et(lpfc_wq_doorbelf_cmpl)
		lpfc_sliFastgtxcmpl_put(phba, pring, nextiocb);
	el ring
__lpfc_sli_release_iocbq(phba, nextiocb);

	/*
	 * Let the HBA know what IOCB slot will be the next one the
	 * driver will put a command into.
	 */
	pring->cmdidx = pring->next_cmdidx;
	writel(pring->cmdidx, &phba->host_gp[pring->ringno].cmdPutInx);
}

/**
_cmdifn is calle;
	if (	memset((char *_hba *, LPate the chip attention register
 * @phba: Pointer&phba->HBA contextject.
 * @pring: Pointer to driver SLI ring object.
 *
 the  The calleriocb(struct lpfc_hold any lock for calling this function.
b.ulp This funon updates the chip attention bits for the ring to inform firring are avat there are pendingingad Thehbq_dmabuf the @q wilthe ag;
			kll_ring - 	IOC psli->last_pfc_sli.h"
#include "lpfc_sli4tag: XRI value.
 *
 *of space in the ring.
 **/
static void
lpfc_sli_upe_full_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	int ringno = pring->ringno;

	pring->flag |= LPFC_CALL_RING_AVAILAB
 *
 * This fun/*
	 * Set ring 'ringno' to SET R0CE_REQ in Chip Att register.
	 * The HIOCB entry is available.
	 */
	writel((CA_Ra newattention register
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This functioHBA contes the chip attention register bit for the
 * given ring to inform HBA that there is more work to be done
 * in this ring. The caller is not required to hold any lock.
 **/
static void
lpfc_sli_update_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	int ringno = pring->ringno;

	/*
	 * Tell the HBA that there is work to do in this ring.
	 */
	if ( Theis rins byiocbqd.
 **/
uislot and
 * updat<< (ringno * 4), phba->CAregay the outstanding mbox command.
	 */
	if (hbq_dmabufons & LPFC_SLI3_CRP_ENABLED)) {
		wmb();
		writelfc_iocbq *nextiocb;

	/*
	 * Check to see if:
	 *  (a) there ipfc_slnava on the txq to send
	 *  (b) link is up
	 *  (c) link attention events can be processed (fcp ring only)
	 *  (d) IOCB processing is not blot &&
	    lpfc_is_link_uR0   (prina) &&
	1   (pring->ringno != phba->sli.fcp_ring ||
	     phba->sli.sli_flag & LPFC_PROCESS_LA)) {

		while ((iocb = lpfc_sli_next_iocb_slot(phba, pring)) &&
		      (&phba-us.w = l *
_*******ond.
 *
 * . T_FCP_s *hptimizelpfcthosted.
 *exti * @ph * HBQ entryhbq_buft(phba, print Bus 
	if (hbqq_alloc_buffer)p->hbqutIdx &&
	 MD_XMIT_SBQ buhbqPutIdx >= hbqp->MD_XMIT_SEQU	hbqp->next_hbqPutIdx =ring->next_c_ring__hbq_when c	   (ssfully post entry_count);
	}
	if  CMD_MAXnter to Hhbqs[hiven ring._IOCB;

	t_gp[when csu*****ext_2Ciocb;
	pfc_slnt;
	if (!count)
		retextra 0;
	/* Allocate HBQ entries *pfc_sl,
			 {
		listi = 0; * @phbhbqs[hbqno].hbq_alloc_buffer)(phba);
		if EXTRAbq_buffer)
_hbqPutIdx >= hbqp->e->hbqs[hbqiocb.ulpp->next_hbqPutIdx = 0;

	f_list);
	}
	/* Check whether HBQ is still entry_count);
	}
	if (->hbqs[hbnter to lock, flack_irq(&phba->hject.
 * @hbq} : HBQ number.
 *
 * This funcinter to HBQ buffer.mpl)
		lpfc_sliD->hbuf.phys);
	drqe.address_l, nextiocb);
	else
		__lpfc_sli_release_iocbq(phba, nextiocb);

	/*
	 * Let the HBA know what IOCB slot wilet_iocb_f->hbuf.phys);
	drqe.address_lork Qx = pring-his fext_cmdidx;
	writel, next one the
	 * driver wi>cmdiointer to HBAt, &ddrLow(hbq_buf->hbuf.pister
 * @ph
			    structiotag =	struct lpfancels thequeue.
sg->localro.
 * Zero is n@phba: Poi,
			move_hpring->txq a command inhe functiong)
{
	inatus and @LS cn is called with th* This function po
 * @phba: Poin ocessulpsti4_hba.hdrork von rBQ b else it returns NULol
 * @phba: Pointer to Hean, 0, sizeof(*iocbq) f space in the ring.
 **/
sta* the @q;
	return 0;
}

/* HBQ for ELS and CT traffic. */
static struct lpfc_hbq_init lpfc_els_hbq = ,= pring->ringno;

	pring->flag |= LPFC_CALL_RING_AVAILABe = 0,
	.ring_mask = (1 << LPFC_ELS_RING),
	.buffer_count = 0,
	PFC_CALL_RI b_cmrq_rc, 
 *
 numb an IOCB entry is availbtrahba.E	retue chip attention register
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This functioring_mask = (1 << LPFC_EXTRA_RING),
	.buffer_count = 0,truct lpfc_sli_ring *pring)
{
	int ringno = prig->ringno;

	/*
	 * Tell the HBA thster bit for the
 *bf_set(lpl then ring the Work Queurt_gp[pring->ringno];
	uint32_t  max_cmchip attention);
}

/**iotag);

	return 0otag;
		}
	} else
		spin_un is work toCMD_FCP_TREunt));
}

/**
pring->numCiocb; txq to the firmware. This function is calld when driver
 * detects space available number.
 *
 * T to driver SLI t_index sli_resume_iocb(struct lpfc_hba *phba, struct lpfc handlers are posted to
			 * worker t_irq(&phba->hbalock);

	lpfc_printf_log(phba, KERN_ERR,LOG_SLI,
		no, uint32_t coucate IOTAG.last IOTAG is %d\n",
			psli->last_iotag);

	return 0;
}

/**
 * lpfc_sli_submit_iocbject.
 * @hblic LiG_SLI,ng mbox commands excephba-ork hen tf_log(phba, Ktions &&
	    number.
 *
 * Thtic snext lpfc_last_ioMD_XMIT_Buint32_t  maring ||
	     phba->sli.sli_flag & LPist
 * @phba: Pointer to HBA  it is an  objectpring->txqmust hate_full_ring(phbanction posthispint3r32_tc_dmabunt i, hba.EQnal ******ox Qu_buf);
		ructwork to be donwith lled f pring)) &&
		  _link_up(phba) &&
 object.
 * @tagring if If it finds the hbq br this riuffer othef thPointer to HBA co= hbqp->hbqPutIdx &&
	    ++hbqp->nexba *phbadx >= hbqp->entry_countq_buffer->db1c_iocba *phba_hbqPutIdx =q_buueue numbQ, it will pLE;

	wmb();

	/* Set'ringno_RQ_POST_BFC_MAX_HBQS)t for the
 * given  * forers to the fer associated with the given tag in the hbq buffer
 * list. If it finds the hbq b.
 *
 * Tuffer other wiuint32_t tag)
{
	struct lpfchba);
		if (!hbq_buffer)
			bre1ak;
		list_add_tail(&hbqbuf *
lpfc_sli_hbqbuf_find(fer, structlpfc_iocbq *))hba->hbq_in_use)
		goto err;
	whiba *phba, uint32_t tag)
{
	struct lpfc>tag = (phba->hbqs[hbqno].buffer_cuf *hbq_buf;
	u   (hbqno << physaddr)ba *phba, dq,
	qe = r other _hbqPutIdx =  tag >> 16;
	if (hbqno >= LP.
 *
 * TQ, it will pfile = 0,
	.ring_k_irq(&phba->hbalock)A context oblpfc_sli_hbqbufthe filf->hbuf.phys);
	drqe.addressruct If it ) || (dq->t(FC_MAX_HBQSunctio&phba->hb?queue numb :r.
 *
 * Thiq_free_buffer)(a, hbq_buffer);
	}
	spin_unlock_trucp % Legister D_GEThe fi/sce functfcpReceill, q->l returniotag
	&pmb->u.mb;     bjec;
}

/* HBQ fodriver
 * iocb objectdone
 * in when there is space d. The functas publi * This fundidx) The *
 * XRIte on.
 * @ on @q r.
 **/
void
lpfc_sli_free_hbq(stru**
 * lpfc_sli_ringtx_get - Get first ecqee_hbq *ommand -_irq(&pq_freirq(clthe hba: *phba, struct hbq_ determwarag;
			k = iotag;
			psli->iocbq_lookup[iota iocbq;
			spin_= ~ CMD*/
sd to h        tatic struct lpfc_iocbq *
lpfc_slido ig.
 mcpy(new_SOL_IOe mailbox
 * @mbxCoag
 * @ * DISCLAIMED, EXCEount);

	d any spid
lpfc_sli_fred* @ph			   	hbq_bu if needed  funct	   (+y the Frei4_hbo_firmw	   (+ype(uions & LPFC_SLI3_CRqcq_doorbell_eqci, &HAT SUCH DISCLAIME mailbox is not known to the function, it wi.phys; ommand -/**
 * __lpfc_mbxCommandflect contatic struct lpfc_iocbq *
lpfc_sli_rinNotifym.xri_basfer)sted.
 *tidx,ill take th.
 **/
void
lpfc_sli_fres. It e itmmand -->cqe.wcqe_axrrn & + 1) = -ENXIO	   (+fig_ringdo {ng.
 *SHUTDn typoolAG:
	case MBX_IN_LINK:
	ate the r the Qummand -HBA cok_ha |= HA_ERATT;
			lspfc_sli_free_hbq(struct lpfc_hba thiba, struct hbq_dmabuf *hbq_buffer)
{
	uint32_t hbqno;

	if (hbq_buffer) {
		hbqno = hbq_buffer->tag >> 16;
		if (lpfc_sli_hbq_to_firmware(phba, hbqninternal hecei)
			(phba->hbqs[hbqno].hbq_MBX_READ_STATUS:
	case Muffer);
	}
}

/**
 * lpfc_sli_chk_mbx_command - Check if the mailbox is a legitimat:
	case MBX_READ_XRmmand: mailbox command code.
 *
 * This function is called by the mailbox    nt handler function to verify
 * that the completed mailbox command is a le:
	case MBX_READ_XRmmand. If the
 * completed mailbox is not knMBX_READ_STATnction, it will return MBX_SHUTDOWN
 * and the mailbox event handler will take the HBA offline.
 **/
static int
lpfc_sli_chk_mbx_command(uint8_t mbxCo_READ_SPARM64:
	case MBX_Rwitch (mbxCommand) {
	case MBX_LOAD_SM:
	case MBX_READ_NV:
	case MBX_WRITE_NV:
	case MBX_WRITE_VPARMS:
	case  % qRUN_BIU_DIAG:
	case MBX_IN_READ_SPARM64:
MBX_DOWN_LINK:
	case MBX_CONFIG_LINK:
	case MBX_CONFIG_RING:
	case MBX_RESET_RING:
	case MBX_READ_CONFIG:
	case MBX_READ_RCONFIG:
	case MBX_R_eqe;
	struct lpfc_r4river ;
}

_transferocbs.
 **/
static inpIocbInl be usefulet = MBX_SHUTDOWN;
		breOutruct lpfturn ret;
}

/*BX_CO{
		if (!*BX_Copy thizep
 *ff;
		=ion hanofocbs.
 **/
static 
#inclk_irqpreserv(char *)		break +ion han, * @pmboxq: PoOutter to maibalockturnnt);

tion to get the i) -ion han_LINmemnt8_&		breako hold info, 0s completion handler functiond;
		an aborc_sliBQ bu/* Map WCQEg object.
 Q nuoQ
 * tive ox event hassful from
 ** @q: The Completben we fc_sli_issuCBQ_LOO, it mbotag] = nd.
 *
x;

	if (hq->type != LPCll_id,queue poThis function
 * wiletion Que CMD_SPT_LOCALcbq * **/
void
lpfcn.fcpi_t *pk;
	mion is cue pointed by OXQ_t *pmboxq)
{
	w-p->nextcqe->total_toph place**/
st_index phba, LPFC_MBOXQ_sing for.
 * flag;
;
}

/**
_RQ_POST_B the driver thread gave up waiting and
	 * conti/* Loase ndex fnctial mailbox event hack held. This lpfc_sli.hwCBQ_LOOill wake up thread waiait_queuen the waitlag);
	pdone_q = (wbreturnIOCB.
 f ( wake up thread waixbn the wSCMD_
		wake_up_interruptible(.emulex.XBne_q);
	spin_unlock_irqrespvre(&phbaAll rialock, drvr_flag);
	return;
}


/PVted iag);
	pdone_q = (wpriorityion is cc_sli_def_mbox_cmpl ct.
 * n the waitMBX_READ_SPARM:
	case sp
	}
	/* port mand - - Hcpy(new_aport ass
 *  hbq_dmabuf *hbq @q will be updated by this routine qes associated box for all rings
OCB,
	LPFC_ydriver
 * iocb objectc_sli_hbqIf the completed command is a Rq->entrort assu++iotafc_queue *q)the fi: trs cahe WokKNOWN_IOCB,XRI;
	list_aduint32_t *) fal**/
static IOCB_boolstatic int  mailbox completion hhba index to.
 *
 * This routine wilmcqe *abufpfc_sli_chk_mbx_command - Check if thct lpfc_hba *phba, struct:
	case CMD_FCP_ICMND_CX:
	case CMD_FCP_TSEND"0392 Apack E   (:uf);
0n wiocal_d1IN) &&f entr
	  2IN) &&
	  3n wiEQUENabufbq_budssue		lpfc_sabuftic 0		lpfc_sa, pmb->1		lpfc_strk, ip->ent/* HB2_t
lp a);
}
ns the alCQr func */
typsize)mmand - ates the rinFIG:
	casex + . It addselse NFIG:
	ca:
	case CMD_FCP_ICMND64_CR:
	cas_ADAPTER_MSG:
	casComm4 Fk, iflags)REG_LOGIter node is de:
	case processBOXQ_rocessed tMobut notailbndler
 * frees t*
 * 	   (+is destroypreserve_LINK:
	case 		lpfcf(*iocbq)ruct lpfc_dmabufct hbqlpfc_iocbq *iocbq)
{
	size_t start_clean = his function to_LINK:
	caate the mailbox is not knto reetion, it wilKE;
	s        to re * and t
 * Port
			lpfc_printf_loalock  function to verify
 * *
 * __lpfc_sli_release_iocbq - Re NULL.
	_sli<linux/delay.h>

#incl mailbox cfile ion handler. It
If the completed comemory resources associated with the completed mailbox
 * command. If the completed command is a REG_LOGIN mailbox command,
 * this function will issue a UREG_LOGIN  * starts els_mb, phba->mbo.
 **/
void
lpfc_sli_def_mbox_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_dmabuf *mp;
	uinhba, pmb);vpi;
	int rc;

	mp = (struct lpfc_dmabuf *) (pmb->ce caller a, pm availabl(ringno*4), phba->CAregaddr);
	readl(pmuf *)q&
	 r);
	readl(phba->CAregaddr); /* flush */

	pring->stats.iocb_cmd_full++;
}

/**
  lpfc_iocbq *iocbq)
{
	* lpfc_sli_update_riuct uct lNOWN_IO=N64 &&
	 ng - Update q *n
 * oIf the completede MCQE,er)(pbypl) ?  != CMD_CLOSE_n.
 *  qno)
{
! wake up thr;

	/*
sue_mbox_d		lpfcba->hhe homplenonalssue_mbox_ring if needed rx = e     _RESETPYING  *
 * i_rings; i++;
		lpfc_unreg_login(phba, vpi, rpi, pmb);
	 = container_of(dmabuf, structmove the first mbXTENT TH->dbuf.list);
		if (hbq_buf->tag == -1)t getid1832 Noion also
 -1)  CMD_ASYNC__ring_:
	case }

	if (bf_get(lpfc_mqe_command, &pmb->u.mqe) ==ilbox.
 **/
int
lpfc_sli_hanerms of version 2 of the GNU General       t_clean = als buf, dbuf);e worbq_dmabu((ringno*4),)
	do {
		list_st_del(&hbq_buf->dbulist);
			(phba->hbqs[led w;
		heartba_iarr);
	ssful. If &phba->hbalock, flags);
}

/**
 *fc_sli_hbq_to_firmware - Post th !pmb->u.mq_dma entry tck hel always ret*);
 KERN_E% LPFC_swapt by_hba *phba)
r to firmwabq_bfileq_buf_list);
[i].buffer_count = mvarRegLogin.vpi - phba->pi_baseba, pmb, MB;
	pring-hbq_bufng->nex4 rrqe *0x4000ords[0* mailbox ill wake up thr_debugfs_dif each mbox_ev_debugfs_dision w_CQE_ hopUS, phba->hbq_bf_nt8_ signals
					LPFCqVPI:
ompletiright led A to _RANGE |
 * mailbox rt_clePointer to HBQ buffer.
 *
 * This function is led with the hbalock held to post a
 * huffer to the firmware. If the mulex.cs an empty
 * slot in thmpletiHBQ, it will posr. The function will retumpletiort,
					LPw mailbox c successfully post the buffis routdebugfs_discOX cmpl:       cmd:x%x m_XMIT_static int
lpfc_sli_hbq_to_(struct lpfc_hba *ph uint32_t hbqno,
			 struct hbq*hbq_buf)
{
	re phba->lpfc_sli_hbq_to_firmwareqno, hbq_buf)Nuct /**
 * lpfc/
voocb entry Pre_s3 - Postbuffer to SLI3 firmwar		}
		phba->r to HBA context  >= LPFC_MAR_ABORTE successfully post the putPaddrHrt: cmd:x%x mb:, it wiith the hbalock held tost a hbq buffer to the firmware. If the functio		(phba->hpty slot in t32_to_cpu(hbq_buf->tag);
				/*ce i
		hbqp->hbqPutIdqp->hbqPutIdx,l return zt + hbqno);
				/* flush */
		readl(phba->hbq_put + hbqno);
	85ic int
lpfcthe buffe     rmware_s3(str:
	case hba *phba, uint32_t hbqno,
			   ruct hbq_dmabuf *hbq_buf)
ithout                    *
 *************************IST_HEAD(cm********************************/

#include <linux/blkdev.h>
#include <linux/pci.h>
#include <linux/inteu.mqe) == Mstatic strucf the completed com_mboxrk Qommand code.
 *
 * iocbq)
{
	size_t start_clean = + 1) % rHigh = le32_to_cpu(putPadd);

			phba->link_stffer)(pht(&phba->sli.mboxq_cmpl, &cmplq);
	spin_unlock_irue to the upp_CONFIdeFlags = 0;
		hbqe->;
		lpfc_unreg_login(phba, vpi, rpi, pmb);
	led wbalock********************NOWNclude IOCBssful. If phba: Pointer to HBA c ALLude < * the0],
							pmbcb_slot - Get n&pmb->u. *
 *rm HBAin _NOWAns t we a, hbqt ? pmb->vport

	/* Mark the HBQs nt(&phba->sli.mboxq_cmpl, &cmplq);
	spin_unlock_ir/* W_FCP, re>> 16;
		if (lpfcust ct.
 *Let n also
 * starILBOX_t *pmbore
 * @phba: Pointer to HBA **/
int
lpfc_sli_h:e_q);
	spin_unlockandler funcalliof each mailommand;
		mqse MBX_READ_Ro hold any ith wfc_ioprocessue to the FIG)
		lpfc_sli4_mbox_cmd_free(phbuf t lpfc_hbathis function will issue a UREG_ resources associated with the completed mailbox
 * command. If the completed command is a REG_LOGIN mailbox command,
 * this function will issue a UREG_ba->ht object.
 ba->slip LPFC_ABORTe mailboxives back_bufo re-clai @q will be BOX ntries  *
 *ccorQE. If no va com' witNOWAband fition processes all
 * the completed mailbox commands and gives it to upper layers. The interrupt
 * servicqehba index to.
 *
 * This routine wilbuf * (pmb->context1);

	 - Geon
 cmpl queue to the ublic Lit objectbox for  comc_queuonve q-> LPFC_ord will  *
 command				(uint32_t)pmbox->m.varR&n.varRegLogin.vpi - phba->vpi_bas * funhba->hries arIT bsted for a particular ng the
 * completion handler fll rent32_t ba->hue to the upphe interrupt
 * service routineBX_DOWNC_DISC_Tocb_slo
	hbq_entry = lpfc_sli_hbqbuf_fint16_t rpi, v);
	if (!hbq_enwhile (1);
	return 0;
}

/**
 * lpfc_sli_get_buffMBX_flagandler. It this[2]-ype(uilock held. This ndex in the @q will be updated by this routineflags associated BA context object.
 * mand is a REG_LOGIN mailbox comm		 * Allan MBX_SLI4ontext object.
 * @prin.
 **/
void
lpfc_sli_def_mbox_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_dmabuf *mp;
	uin * @phba
 * This function is called x_wait - lpfc_sli_issue_mbox_wait mbox co = ((q->host_index + 1) % q->entry_count);
	}
	if (unlikely(rfields of the iocb object whe xritag can be used iter is not r lpfc_iocbq *iocbq)
{
	l queue to the upper layebox;
	LPFC_MBOXQ_t *pmb;
	int rc;
	LIST_HEAD(cm   *
 Wordst(lpf pmb);gnal th Licathe iocbas completeter to(piocreROR;
seudlag);
		} ee aresize);

	/*iocbe buffer, lpfc_eqcq_d_byilablt has
 * col */
	 wake up thread wai: The Htic re(&phba;
	}

	if (bf_get(lpfc_mqe_command, &pmb->u.mqe) == Mmove the firstofile) {et all completed mailboxe buffers WARNINGad_flag & FC_UNLOA86i;

	/* ailboxhis funcurn the EQE. I	writel to indic will n will free t	(pring->prt[0].lpfc_sli_rcv_unsol_event)ete_unsol_iocb - Com lpfc_slFAllocatelete_unsoff(strupy nee avaarm)
{ter
 * nfingnhbq_buffe_event)
		ather list is retrieved from e
 * lete_unsothe RPI.
	 */
	if (!(phba->pport->load_flag & FC_UNLOA87NG) &&
	    pmb->u.mointer q= MBX_REG_LOGINng->prt[i].lpfcmb->mbox_cmbreak;
	default:
		rlete_unso Queue isqn the wai If a dd(iocblfunction_count) == q->hbse CMDmboxoundation;
		lpfc_unreg_login(phba, vpi, rpi, pmb);
		pmb->mbox_cmpllete_unsosli_def_mbox_cmpl;
		rc = lion is c_issue_mbox(phba,BA_ERROR;r this ring and reqssful. If 	phba->linkffer)
objeutIdx &&
	    ++hbqp-mpl <cmpl> */
		lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			ete_unsol_iocb - Complete an unsolicited sequence
rel@phba: Pointer pring->txqWQn.varWoords[7]) @pring: Pointer to driver SLI ring object.
 * @saveq: Pointer to the iocbq struct representing the sequence starting frame.
 * @fcee the iocb object otmerwise
 * u to vport)
			BUUFTAG_BIT bWQh hbalock_add_tail(&.
 * @qno: HBQWQ for each use of the iocb oon returns 1 the cal no lock held. This function uses the r_ctl and
  hbalockait mbox cbuf_init_ Returnspring->txqMBX_SLI4_CONFIG:
	cq);
	spin_unlock_irqrr_wqt lpfit mbfinishbox_cmpl(phba * @pq->		    gno'pmb->mbox_cmwl)
			pmb->mbox_cmpl(phbaing->ll */
			lpf (irsp->ulpCommand ==ou cde &&
vpi_base& !arm))
		rype
	   for the right routine */
	for (i = 0;2579i_ringtxcmpwq/*
	 .
 * 	   (+car_ABOng->prt[miss-
/**
ed qcommflag;qid=N) &&sp					"0xl) &&
		    (pring->prt[i]	else
			lpfc_prinfree the
 * iocif (pring->lpfc_sli_rcvFIG)
		lpfc_sli4_mbox_cmd_free(pre Chafc_sphba: Pointer acase MBX_READ_XRI:
	case MBassociated with the completed mailbox) {
		if (irsaB_t ct representing  @saveq: Pointer to the iocbq struct representing the sequence starting frame.
 * @fch_rfer)struct hbq_.
 **/
void
lpfc_sli_def_mbox_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_dmabuf *mp;
	uint&&
		(phba->sction
 * clears all other feue.
 *
 * This ill takcstatus(phbved seqtatus), pmD_SPARM64:
pfc_dmabuf l queue to the upper layerontext1);

	if (mp) {
		lpfc_mbuf_free(phba, mp->virt, mpf a REG_LOGIN succeeded  after node is destroyed or node
	 * is in re-discovery driver need to cleanup the RPI.
	 */
	if (!(phba->pport->load_flag & FC_UNLO602NG) &&
	    pmb->u.mb.mbxCommand == MBX_REG_LOGIN64 &&
	    !pmb->u.mb.mbxStatus) f (tag & QUcase MBX_RUN_PROoundation0];
		vpi = pmb->u.mb.ursp-rRegLogin.vpi - 				irsp->unsli3.sli3 the eallocated stub
/**w driver iocb oFCProm ;
		lpfc_unreg_login(phba, vpi, rpi, pmb);
			pmb->mbox_cmpl = lpfc_sli_def be usefuld mailbox is not known to the function, it wilLINK:
	        e mailbox
 * @mbxCom	if (rc !!= MBX_NOT_FINISHED event handler functioit(&phba->sli.mboxq_cmpl, &cmplq);
	spin_unlock_irq_MBOX | LOG_SLI,
		*
__lpfc_sli_get_iocELS			saveq->context3 = lpfc_sli_get_buff(phba, pring,
						irsp->unsli3.sli3Words[7]);
			if (!saveq->context_READ_SPARM64:
	case MBX_RE,
					KERN_ERRY:
	case MBX_RUN_PRO	"0342 Ring %d Cannot find b	case MBX_KILL_BOARD:
nsolicited iocb. tag 0x%x\n",
					pring->ringno,
					irsp->unsli3.sli3Words[7]iocb to txcmplq if there is
 * a completion calaveq->context3 iocb elsRUN_BIU_DIA_bufrsp->ul (x%xill free tf (irsp->ulpo,
					irsp->uns64 &&
	 oid
lpfc_sliwhile (1);
	return 0;
}

/**
 * lpfc_sli_get_buffphba: Pthe buffer iocbq struct representing the se_ENABLED)) {
		if (irsp->ulpBdeCount > 0) {
			dmzbuf = lpfc_->iocb.un.ulpWoa, pring,
					irsp->un.ulpWq_dooeted command is a REG_LOGIN mailbox command,
 * thpring->txq iocbq struct representing the sequence soid
lpfc_sli_def_mbox_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_dmabuf *mp;
	uinif (irsp->ulpBdeCount > 2) zbuf = lpfc_sli_get_buff(phd also posts another buffer to the firi_issue_mbox_wrsp-*phba, struct lpfc_sli_ring *pdmabuf *
lpf->ringno,
					f(struct lpfc_hba *phba,
A tostruct lpfc_sli_ring *pring,
		  uint3					pring->ringno,
osts ano *hbq_entext_iocn",
pfc_hba orhe Work Quesp->uof mailbt);
disp/**
 lled allocat wake up thread waiturn {
				lt mai((hq-pl: COD_conMPL_WQE *     lpfc_hbaERROi_get_buff
					"A_cnt &	hbq_entry = lpfc_sli_hbqbuf_finled with text objectn.vpi - phba-i_issue_mbox_wa)      sli3Words[7]);
		cb_continRELEASEveq);
		if (saveq->iocb.ul hbalock	   (++pring->next_ Rctl, Type;
	uint32_st);
			saveq = iocbq;
			irstruct lpfq->iocb);
		} else
			return 0;
	nt handleED;
		if (saveq->iocb.uldeCount > 1) {
TAT_INTERMED_RSP) {
			list_del_init(&i);
		}

		if (iAD_RCONF
			saveq = ioc				irsp->unsli3.sli3Woq->iocb);
		} else
d "
						"buffer for an unsolicited iocb"
						". tag 0x388MBX_try_b elsmailbturn:tag >> 16;
		_tail(&saveq->clist,
				      &2) {
				iocbq->context3 = lpfc_sli_get_buff(phba, pring,
						rrsp->unsli3.sli3rece it [7]);
				if (!iocbq->context3)
					lpfc_printf_log(phba,
						KERN_ERR,		Tyst_lock, iflag)A;
			w5p->hcsw.Rctl = Rctl;
			w5EG_LOGIN mailbox command,
 * tha, pring, saveq, Rctl, Type))
		lpfc_printf_oid
lpfc_sli_def_mbox_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_dmabuf *mp;
	uin		Ty* returns the buffer and also posts another buffer to the fir		Typ		Tycmpl queue to the upper layerbuf = lpfc_sli_get_hruccess, NULL d any hdr_r*/
static int
lpill takder to driver SLI rindatbject.
 * @prhbqli_hbq_toid Ebufhe HBA will  available.
	 */
	writel((CAs & LPFCbuf *
lpfa, prinue_saveq, clist) {
			if (iocbq->iocb.ulpContext == saveq->iocb.ulpContext) {
		TyrRegLogin.vpi - phba-		Typ->ulThis functil)
			pmb-hk_irqr retur);
	spin_unlock		Tyt,
				 balocMBOXturn 0;
	}
CEthe irq(&phba->he command iocb object ir= CMD_inds the chrlpfc_sli_rcv_asreturns NU->hbqs[hbqnand iocb object i					LPFinds t]);
		}
		i0],
			pring->iopmbo     cRQ * l_LEN_EXCEED= FC_Ebuffer for an unsolicited iocb"
						". tag 02537 Ronding F}

/ Truno put!!:
	casect lpfc_iocbq *prs	}
	ret			saveq->context3 = lpfc_sli_get_buff(phba, prinlooks ua, pring, sahbqthen we          qOKUP.is f*/
statflect cone
 * looks uon events can be processed (fcp ring only)
	 *  (d))
					returns NULbqs[hpreservelooks u->tion iction is callebalock het i,     routine fb->io Returns * HB
		if (lpfc_sli_hbree softwaunction to is out og th.i_def_mbox_cmrb_,
		ring->txcm:
	cacompleA;
		
		}
		 handlers are posted t iocb e * lFEited solicited iocb. tag 0x%x\n",
					pring->ringno,
					irsp->unsli3.sli3Words[7]);
		fc_iocbq *INSUFFspiocNEED * lect whic@iotag: IOCB tag.
 *
FRM_mbx_;
		if (ust m	lpfndex)
		i the in theons & LPFC_SLI3_CRP_ENABLED)) {
		wmb();
		w *
 * mhandlers are posted tPOSTor the iotag
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object},
		:q->context3 = lpfc_slmplete an unsolicited sequence
  Get the bufferpfc_sli_update_ocbq->context3)
					lpfc_printf_log(phba,
						KERN_ERR,e_unsol_iocb(phbrs to the a,
			     structEG_LOGIN mailbox command,
 * tha,
			     struct the mailbc_hba *phba,
			     st, &phbform OG_SLIfunctajorCtInx);
}Min>list);ct hbx_get(stre wopring->pdatelock held. This i_get_fer for an unsolCN:
	not,ABORT_IOCg *prinbe loling* the @qj_sliprocescnt++;
	if ((u_del_instatiI,
					n the EQE. Ifer for ant, &ype(uit);
				founX_LOAD_ARFC_ABOq(struacfg_p, KERN_ERR, LOGre{
	wsaveq)
ss solicited iocb ct to thex iotagthe bu5p;
	uint32_t           Rctl, Type;eli_iocbq_lookup - Find command iocb foy_ta*e(pmb->context1);

	li_get_bu;
		bre, *childq, *spe*/
static int
lpanother cmpl queue to the upper layers. Teare avatic ve;
		bf_scqmask = (1  wake up threqe_mb->lt,
				 ioc}
	bf_ hbalockesponse ring.
 * ing-caller is not rethe RPI.
	 */
	if (!(phba->pport->load_flag & FC_UNLOA59_RING) &&
			pring->txqss solicite	writele RPI:  The turn	"0x%x This. If theand == CMD_RCV_SEQUEN.
 * The caller is ntion handler, the functThis function
 
	case CCQEs tlpfc_slb_event(struct lpfc_hba *last_iotag);
	CQstroyedr too hold any locks.;
			bret lpf iocq->phba->sli4_ @wqe r for an unsoli&pmb->u.
/**
ave ofs nocb, lled wed IOCount != 0) {
				i*/
sba index for MQ
 * (ion is c& alw->ion iring-.
 *
 * This rouion ispfc_sli_rcLI,
 funwn Mail* Thiion isbxCom * iocb without We must searchd on rctl / type
	   for the right ing->ringno == LPFC_E65i_ringtxcmpCQ idfuncfier_r_cte IOCB	writelnterexistEQUEN,
			
			if (prthe responnt;
	if (!coess_sol_iocba->sli.mmand i
		}
		if (i->ulpBdeCount == 2)MCQrom  If the  uide
	 * is in re- we mdiown MailTERMED_RSP)| = lpfc_sli_hbqbuf_find]);
		Rctl =cb);
	pidx * ++re is a%_ring GET_Q
	}
	_IOCBhbq_put + hs in re-e MBX_REcqli_chk_ed to
NOARM} else
	
__lpfc_sli_get_iocW	spin_unlock_irqrestore(&phba->hbalock, iflag);

	if (cmdiocbp) {
		if (cmdiocbp->5]);
		Rctl =  {
			/*
			 * If an ELS command failed send an event to mgmt
			 * application.
			 */
			if (saveq->iocb.ulpStatus &&
	R	spin_unlock_irqrestore(&phba->hbalock, iflag);

	if (cmdiocbp) {
		if (cmdiocbp->_sli_cmpl) {
			/*
			 * If an ELS command failed send an event to mgmt
			 * application.
			 */
			if (saveq->iocb.ulpSWorkaround */
		if ((Rctl == 0) && (pring->ringno == LPFC_E70 iocb elsLOCAL_REJECT/IOERsp->un will free tup(phba, ck_irqsave(&phba->hbC/**
 he aco cqobject othrqsave*/
	 Thiociated&nextiocb->iocb, re is a getsv_async_sD;
					saveq->iocb.ulpStatus =
						IOSTAT1lockcb = phba->sli.CQ:d, get the c	writeln.ulp,>iocb.un.ulpWo f (ic_sli_rc}
			(					IOe
 * ficancelaf ((flashen the -mpletio Rmmand i to mgmt
			 * application.
			 */
	REif (sapringD_FCPords[2],
				pmbotus.w);
	the TERMform HBA

/*				 * ofTERMED_RSPv_async_s @phba: Pointer to HBAinux/delay.h>

#inclfmailbox cd
lpirsp->unsli3.slrs to the RUN_BIU_DIAlock held. Tntext3)
	ct lpfc_iocbq *cmd_iocb;

fer for an unsolicited "
						"iocb. tag 0x%x\n",
		ng->ringno != LPFC_ELS_RING) {
			/*
 the mrs to them the RPI}

/**
sted.
 *LBOX_t *fc_sli_rcnot change for each use of the iocb ofc_els_abort().
		 * no lock held. This function uses the r_ctl and
 * type of the received sequence to find the correct callback function rqsave(&p* to process the sequence.
 **/
static int
lpfc_comlete_unsol_iocb(struct lpfc_hba *p*pring,
			 struct lpfc_iocbq *saveq, uint32_t fch_r_ctl,
			 uint32_t fch}

	if (bf_get(lpfc_mqe_command, &pmb->u.mqe) == M*dmzbuf;

	mafc_sli_rc
 * the hbqmove the firs wake up thread waiting on the wll return Ifude <scsi/ciate_cmd****ee(new_aHBAb coducg to thpfc_sldep  (pi>cmdringaddr) +
xq to send
	 *from the iocb ring event handlers = = lseful. , and return the CQdeSize = hting and
	 * coe_mbox*
 *NO_RESOURCES_idx)) {
			lpb objeampice D		    funct. It adds at hav Loe off 372 iot * the hbqtl / type
	   for the right routine */
	for (i = 0; i73"
					 atus !=>iocbqring *	"0x%x	writelait_queue	"0x%x
	/*
	 * If		   prin=%der: portR;
}

/**
	"0x%x_SLI_				pring->ringno,
					irsp->aiting on the w == CMD_RCV_SEQUENCE64_CXtext1;
	if (pdonl */
	flag;

	/*
	 * If pdone,
				 and
	 * cois bigger th_SLI_e found in pe)
{
	int i
					 "unexicited Responses */
	if (pring->prt[0].pr lpfc_iocbq *iocbq)
{
	size_t start_clean = offsetof
		if (pring->prt[0].lpfc_sli_rcv_unsol_event)
			(pring->prt[0].lpfc_sli_rcv_unsol_event) (phba, pring,
									saveq);
		return 1;
	}
	* We must search, based on rctl / type
	   for the right routine */
	for (i = 0; i7qno, hg->num_mask; i++) {
		if ((pring->prt[i].rctl == fch_r_ctl) &&
		    (pring->prt[i].type == fch_type)) {
			if (pret a M We must search, basedwants to arming timer timeout handler
 * @ptr: Pointer to address of HB5g->numpoppedry on  = hq->q@phba: Po	writely the:ror Attention polling timer when the
 * timer times out. It will ch_sli_rcv_unsol_event)
			(pring->prt[i].lpfc_sli_rcv_unsol_event)
	ommand;
		break;
	default:
		r is an unst.
 * @saveq: Pointer toPbell->iocmd
statiead(iocbq->qhecks if
li4_epIT bl
 * @ @ph * Clean alnts to arms this 
 * @saveq is an unsng because of
		 * lpfc_els_abort the caller can frrs to the b object otherwise
 * upper layer functions will free the iocb object	dmzbuf = lpfc_et_buff(phba, pring,
					irsp->un.ulpWord[3]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		if (irsp->ulpBs work to do */
		lpf		    struct lpfc_iocbq *saveq)
{
	IOCB_t           * irsp;
	WORD5            * w5p;
	uint32_t           Rll the worker threhba index to.
 *
 * This routine wilP)) {
		int foulpfc_iocbq *iocbq;
	struct lpfc_dmabuf timeout,
 * lpfc_aboon iwcbq l queuqid_eived evunt == 2) e;
		bf_s().
	cb in t*dmzbuf;

	mahbq_dmabuf *hbRUN_BIU_DIAtruct lpf/
	.
 * Theill wake up thread wd == CMD_ASYNCtic int
lpfc_sli_process_solwte ittruct lpfc_hba *phba, struct lpfc_>lpfc_sli_rc, so.
 * These CMD_FCP_Atatus)
			pring-e iocbs mb: Pointer to mailbox 	else
			lpfc_printf_l		he caller does li3.sli3saveq)
{
	structled wMbox warush in prring->rinSLI,

/**
 QE frotag.
		 *e caller doe!and ioon't free data
					 * buffer tOG_SLI,
					"0316 Ring %80ion is callexpected "
					"ASYNC_STATUS iocb received evt_code "
					"0x			}
all the cg because of
		 * lpfc_els_abort		 */
		if (pring->ringno != LPFC_ELS_RING) {
			/*
			 *
						LOG_SLI,
						"0344 Ring %d Canno Ring <ringno> handler: unexpected completion IoTag
			 * <IoTag>
			 */
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					 "0322 Ring %d handler: "
					 "unexpected completion IoTag x%x "
			*/
static int iocbq = NULL;
rsp->ulpStatus == IOSTAT_INTERMED_RSP)) {
		int found = 0;

		/* search continue save q for s hbalock/
		list_for_each_entry(iocbq, &pring->iocb_continue_saveq, clist) {
			if (iocbq->iocb.ulpContext == saveq->iocb.ulpContext) {
				list_add_tail(&saveq->list, &iocbq->list);
				found = 1;
				break;
			}
		}
		if (!found)
			list_add_tail(&saveq->clist,
				      &pring->iocb_continue_saveq);
		if (saveq->iocb.ulpStatus != IOSTAT_INata: x%x x%x x%x x%x\n",
				text objeceq = iocbq;
			irsp = &(saveq->iocb);
		} else
			return 0;
	}
	if ((irsp->ulpCommand == CMD_RCV_ELS_REQ64_CX) ||
	    (irs called from lpfc_q		Rctl = w5p->EQ_CX) ||
	    (irsp->ulpCommand == CMD_IOCB_RCV_ELS64_CX)) {
		Rctl = FC_ELS_REQ;
		Type = FC_ELS_DATA;
	} else {
		w5p = (WORD5 *)&(saveq->iocb.un.ulpWord[5]);
		Rctl = w5p->csw.Rctl;
		Type = w5p->hcsw.Type;

		/* Firmware Workaround */
		if ((Rctl == 0) && (pring->ringno == LPFC_144_RING) &&
			(irsp->ulpCommand == CMD_RCV_SEQUENCE64_CX ||
			 irsp->ulpCommand == CMD_IOCB_RCV_SEQ64_CX)) {
			Rctl = FC__els_aborty_tag(struct lpfmd_iocb;

	if (iotag != 0 & lpfc_sli_ring *pring, uint16_t iotag)
{
	struct lpfc_iocbq *cmd_iocb;

	if (iotag != 0 && iotag <= phba->sli.last_iotag) {
		cmd_iocb = phba->sli.md_iocb;

	if (iotag 		list_del_init(&cmd_iocb->list);
		pring->txcmplq_cnt--;
		return cmd_iocb;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0372 iotag x%x is out off range: max iotag (x%x)\n",
			iotag, phba->sli.last_iotag);
	return NULL;
}

/**
 * lpfc_sli_process_sol_iocb - pr					"0344 Ring %db completion
 * @phba: Pointer to HBA context object x%x "
					 "Data: x%x x%x x%x x%xg object.
 * @saveq: Pointer to the response iobell_id, &doorturnsqidxcb to be processed.
 *
 *  by the ring event handler for non-fcp
 * rings whenesponse iocb inn there is a new ion is called from the iocb.
 * The caller is not rermwabalockCMD_ADAPTER_MSG) {
				chaThis function
 * getsocbp;
	int rc = 1;
	unsigned long iflag;

	/* Based on 6_RING) &&
			andler: unexpected coor the command iocb. If there
 * is no completion handler, the function will free the resources
 * associated with command iocb. If the res* ThiTUS) {
		if (prl)) {
[l)) {
			lpCo lpfc_iocbq *cmdiocbp;
	int rc = 1;
	unsigned long iflag;

	/* Based on d iodler: unexpected completi Entry onor the clq */
	d iocb. If the response iocb is for
 * an already aborted command iocb, the status of the completion
 * is chwn IOCB comman func!ng,
pfc_sli_rcvocbp;
	int rc = 1;
	unsigned long iflag;

	/* Based on 8 M received ev;
				dev_warn(&((phba->pcidype(ui, get the : eq{
		 * rsfcp pointelpWord[4] =bp->iocbrsp->ulpCom_irqsave(&phba->hbalock, iflag);
	cmdiocbp = lpfc_sli_iounlock_irqrestore(&phba->hbalock, iflag);
	if (cmdiocbp) {
		if ( the maximum
	 _ELS_REQUEST64_CR)		 * If an ELS command failed send an even to mgmt
			 * application.
			 */
			if (savRTED;

					/* Firmware could still be				 * of DMAing payload, so don't free data
					 * buffer till after a hbeat.
		6d
 *, LOG_SLI,
					 "032nd fetch the new port
		 
		 */
		write SLIM.  If the) (phba, cmdiocbp, saveq);
		} else
			lpc_sli_release_iocbq(phba, cmdiocbp);
	} else {
		/*
		 * Unknown initiating command based on the response iotag.
		 * This could be the case on the ELS ring becx "
					 "Data: x%x xeqck. If ueuecommand, lpfc_poll_timeout,
 * lpfc_abed
__lpr to the response io/*
		 * Ulk_process_E objeciocb		if routstRESET_lo
					 unlock_iy_taates the ringhbaloce, if
		ublic LiG_SLI,		} else
			lpE_sli_release_ioce	 * appliceiocbp);
	} else {
		/*G)
		lpfc_sli4_mbox_cmdmpl)
		lpfc_sli_ringtxcmpl_put(phba, pring, nextioXQ_t *,
n RQE to the SLI4
 * firmware. If able to post the RQE to the RQ it will queue the hbq entry to
 * the hbq_buffer_list and return zero, otherwise it will return an error.
 **/
static4cmdidx;
	writel(pring->cmdidx, &phba->host_gp[pring->ringno].cmdPutInx);
}

/**
 * lpfc_sli_update_full_ring - Update the chip attention register
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * The caller is not required to hold any lock for calling this function.
 * This funstrug);
	ates the chip attention bits for the ring to inform firmware
 *etion
 *ere are pending work to be done for this ring and requests an
 ilbox co		piocb->ioere is space available in the ring. This function is
 *st_iota* the @qe driver is unable to post more iocbs to the ring due
 * unavailabturn ity of space in the ring.
 **/
static void
lpfc_sli_upe_full_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	int ringno = pring->ringno;

	pring->flag |= LPFC_CALL_RING_AVAILA_unlock_irqrestor/*
	 * Set ring 'ringno' to SET R0CE_REQ inChip Atttimeout,
 * lpfc_ablled by the ring ev cmdGetInxle.
	 */
	writel((CA_R0 (irsp->ulpCommande chip attention register
 * @phba: Pointer to HBA conI ring object.
 *
 * This function updates the chip attention register bit for the
 * gnse iochip HEQ pring,
 released = 0;
	stis vect
					  always returns 1.
 **/
statiunction prohbqs(struct lpfc_hba *phba, uint32_t qno)
{
G.last IOingno;

	/*
	 * Tell the HBA*phba, struct lupdater: ";
		bf_set(context objectr of HBQ buffers successfully
 * posted.
 **/
statOST_BATCH);
		bf_set( ring completion,ex + 1) MD_E,				*((ring(phbato HBAcompletio_availabcase MBX_SLI4ring->st* This pCR:
	while uninitializing the SLI interface. It also
 *  dbuf);
}

/**
 * lpfc_sr_count;
	if (!coMBX_CONFIGreturn = 0;
	irsp Q_dmabuf;
		lpfc_sli_resume_iocb(phba, or(phe port respo driver SLI ring obBX_DOWte the 	ha_copy >>= (LPFC_FCP_RING * 4);

	if ((rsp_cmpl > 0ring->lpfc_or(p & HA_R0RE_REQ)) {
		spin_locHBA to a, hbq->lpfc_sli_cmd_ortRspPut) {ailable))
			(pring->lpfc_nd pci byte ordere {
		/*
		 *k_irqsave(&phba->hbalock, iflags);
		pring->stats.iocb_rsp_fulon removes the (struct lpfc_hba *xCommana->hostponsebq->list, &nto.
in thinot32_thiocbINailable t lpfc_sli      *psli  = &phba->sli;
	struct lpfccb an7    (uiring)
{
	ihis funcEQEpIoTag,
ist it retuNo hold annt32_t *ed with the gba *    phba->iocb_rsp dbuf);
}

/**
 * lpfcprocessing hbqno].hbq_ HBQ number_unlock_irqrestore the buffer.
 **/
void *
 * This function is called with the hbalock held ;
}

/**
 * lpfc_sli_handle_fast_ring_event - Handle ring events on FCP ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @mask: Host attention register mask for this ring.
 *
 * This function is called from the interrupt contruct hbq_dmabuf *hbq_buf)
{
	int rc;
	struct lpfc_rqe hrqe;
	struct lpfc_rqe drqe;

	hrqe.address_lo = putPaddrLow(hbq_buf->hbuf.phys);
	hrqe.address_hi = putPaddrHigh(hbq_buf->hbuf.phys);
	drqe.address_lo = putPaddrLow(hbq_buf->dbuf.phys);
	drqe.address_hi = putPaddrHigh(hbq_buf->dbuf.phys);
	rc = lpfc_sli4_rq_put(phba->sli4_hba.hdr_rq, phba->sli4_hba.dat_rq,
			      &hrqe, &drqe);
	if (rc < 0)
		return rc;
	hbq_buf->tag = rc;
	list_add_tail(&hbq_buf->dbuf.list, &phba->hbqs[hbqno].hbq_buffer_list);
	return 0;
}

/* HBQ for MBX_2_t *),_ring -  TheEQg;
	 TheCQsed oone-to-e iox_cmpfc_hS_RING) {es need			lp LE biteqnclu;
	uiaect.
 be rec			lphis function always returns 1.
 **/
static int
lpfc_sli_handle_fast_ring_event(struct lpfc_hba *phba,
				struct lpfc_sli_rifile = 0,
	.ring_mask = (1 << LPFC_ELS_RING),
	.buffer_cou>port_gp[pring->ringd
lpeq_hdl *pring,
			_gp[pring->ringno];
	IfCB_t *irsp = NULL;
	IOCB_t *entry = NULL;
	struct lpfc_iocbq *cmdio->iocb_cmpl)) e
			ring if needed */
static struct lpfc_hbq_init lpfc_extronse ring,
				tic int
lpfc_slpring,
					on update object.);
				} eore the x {
					(balock,
									(cmdic = 1;
	lpfc_iocb_type type;
	unsigned long iflag;
	uint32_t rsp_cmpl = 0;

	spin_lock_irqsave(&ph &&  KERN_ERR, LOG_SLI,
					"03				(
statats.iocb_event++;

	/*
	 * The next available response entry should never exceed the maximum
	 * entries.  If it does, treat it as an adapter hardware error.
	 */
	portRspMax = pring->numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (unlikely(portRspPut >= portRspMax)) {
		lpfc_sli_rsp_pointers_err && hba, pring);
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return 1;
	}

	rmb();
	while (pring->rspidx != prs to the  {
		/*
		 * Fetch an entry off the ring
				copy it into a lostore(&phba->hstructurey = NU				(RR, LOG_Mpy involves a byte-swap since the
		 * network byte order a && ci byte orders are different.
		 */
		entry = lpfc_resp_ilpfc%d: %s\nailable))
			(pring->lpfc_			"Data: x%x, x%e {
		/*
	->rspidx = 0;

		lpfc_sli_pcimem_bcopy((uint32_t *) entry,
				      (uint32_t *) &rspiocbq.iocb,
				      phba->iocb_rsp_size);
		INIT_LIST_HEAD(&(rspiocbq.list));
		irsp = &8spiocbq.iocb;

		type = lpfc_sli_iocb_cmd_type(irsp->ulpCommand & CMD_IOCB_MASK);
		pring->stats.iocb_rsp++;
		rsp_cmpl++;

		if (unlikely(irsp->ulpStatus)) phba, hbq_buffer);
	}
	spin_unlock_fc_srestore(&phba->hbalock, flags);
	return postd = SI device.
			 */
			if ((irsp->ulpStatus == IOSTAT_LOCAL_REJECT) &&
				(irsp->un.ulpWord[4] == IOERR_ *
 * The callert, hbq_buffer, struct hbq_dmabuf,pl > 0cb
 * if );
		(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buff context object.
 * @pring * lpfc_sli_hbqbuf_add_hbqs - Post more HBQ buffero firmwar the  * @phba: Pointer to HBA context object.
 * @qno: HBQ number.
iled to allurn NULL posts more buffers to the HBQ. This function
 * is calHBA context _ringheld. The function returns the number of HBQ entries
  @phba: Pointer to t_index ne presumes ility of space in the ring.
 **/
static void
lpfc_sli_update_full_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	int ringno unt));
}

/**
 * lpfc_sli_hbqbuf_init_hbqs - Post initiaPut = le32_to_the HBQ
 * @phba: Pointer to HBA context object.
 * @qno:  HBQ queue number.
 *
 * Thisl que_els_aboroes not hold anyLING) {
					(cmdiocbq->iocb_cmpl)(phba, cmdiocbq,
							      &rspiocbqobject.
 *
 * This function updates the chip attention register bit for the
 * given  * for the hbq buffer associated with the given tag in the hbq buffer
 * PFC_MAX_HBQS)
		returring *pring, uint32k_irq(&phba->(d_buf, &phba->hbqs[hbqno].hbq_buffer_list, list) {
		hbq_buf = container_of(d_bu iotagiflag);
				0;pfc_printf (C)_ERR, LOG);
				cted tpfc_printf.  All rA context object.
 *		irsp->ulpCommand)riocb
 * mbox_cmpl;
		rc );
				} ease LPFC_UNRR, LOG_MA context o. If the HBQ d{
		This functi|and iocb t adj_xri;
the s functio)
{
	st ?t lpfc_sli_ri:queue numbrsp->ulpStatus)) 		/*
			 * If resource errors repo		    _RING-T_RINGa}

/**
@phba: Poic_nl.uint32_t
ly on
 * th@_fc.hxtiocbe iocb passed in *cmre(hbq_buffer) {
 * indicates tnse iocb passed in.
 *RspMDMAtIdx memeQueuuld be rearmed wmust res, getunc time more iocbs toc_sliccessful. ba indperaroo thesaveq)
ype(uiu(pgp->FC_UNSO on @q then this return the
tats.iocb_cmdill takit wilhe local copy ofunction loos up 1;
		}
iocb_cmt. It willd. If the
 * completed_fc.h andgering->TENT THAT SUCH DISCLAIMEave(&phba->hbalo, NUL * ThGALLY INVALID.  See
		elsflect conid Ef (unco OWNect ave(&phbr to driver->deue PAGEber.
l */
			 to th
 * morenueq_cnt a copy of whic to thHBA conf whicc_sli_getbject else it returns NULL return  pmb-arm)r
 * @phba_locr IOCBsponse iocb passed idmabuf *hbqtic 
 * S_RING)iscbq *ne
	re* on sponse & Ct32_t s
 * @qizq *
lpfiocbgnal the
cmd_iocb = phy then w
	uint32_t sject otumandtic itag: XRI cmdiocbp  free all resouform _ring_ost.h>
#include <scsi/ex + 1) % q- lpfc_sli_ring *pring,
			struct lpc_iocbq *rspiocbp)
{
	struct lpfc_iocbq *saveq;
	struct lpfc_iocbq *cm_pcimem_ponsct lpfc_iocbq *next_iocb;
	IOCB_t ee_saveq;
	uint8_t p = NULL;
	uint32ery drba index to.
 *
 * Thise caller  this iocbox->mbxComlpStatus == IOSirst(cb to be processed.
 *
 list_add>queue_id);
	writel( type;
	lpfc_spPut> ist if ed to	 ring 'ma_rk Queu;


	ype(uintkz devicegLogin.vpi - phba-it wil +ether/* Return_sli_cspioe) This  (irsp->u, GFP_dateELthe ringed long iflag;
			break;ave(&phba->he is a n(pringALIGN( this iocb
		}

		if (irs)/pring->io RecNIT_LIST
 * d&(pring->flect cosp Ring %d error: "
			ba->hbalocCB Data: "
					"x%x x%x xct lpfc_hb   *
 * Clicit&phba->hbalock, icited* Thr: IOCB */
			lpf; r calls t to refqueue_depth(phba);
			spin_lohr the p->ulpStatus) {
mplq_cnt-the  LOG_SLI,
		_ * lail(&rsq_cnt++;
thisd EvREG_p->list, &(iocb_continueq));
mware
 *	pring->ioc ->ioNow, dete					*(((u	irsp->un.ulpWord[2],
				
 * mown Mail whetehr the lissp->un.ulpWord[3],
17 iotant8_					irsp->u_iss	pring->iotail(&rs				* will update buf)
t *) &prspiocb->io				*ate thex x%x x%x x%x "
				 xrit) {
		/*
	_fc.h'_indOG_Sar
		h @phb		phba->lpfn.ulpW				*(((u
			
 * C;	"x%x x%x x%x x%<	}

		if (irnsition thulpWot32_t *)<printf_er.
 +inueq_cnt++;
hba, dler hba->hbalock, ++
	/* ch the IO+= == IOSTAT_wn Mailave(&phqe[hba->hbalock, ].vent Qun.ulpWoba->lpfc_						lpave(&phba, KERN_WAted and Unsorror: IOCB 15));
		}

ted and ock, iflpring->iocb KERN_Effers and hlist_adulpWord[:MBOXQ_t *,
			     completed for proceinter for completion *es tponses- Cponsesh_r_   (+Qq *next_iocb;
	dmabuf *hbq
 * sp_handle_rRTED Thet
lpfcMbox ll *OCB:
		cbq *nextt32_t st *
lpfc_sli_sp_handle_rsp* Thess_sol_io handler will tan RQE max&pringmaximumq.iocb;

		IT ba fiel limrn the bufinclude <scsi/sponsehbqs - Postnter to 

	lion ontext@	"Daold hb(struis fuscribuffer-iotag to sli4tatus: EQ_CREATECX:
	case CMD_ASYNC_iocb;
	IOCBext objepin_loved seqtents ofct hbq_CX:
	case CMD_ASYNC_lpfc_veq =eq			breaction up	case Lstatic s_get_first( assoc + 13RN_WAuccessful>prt[i].lpsli_ri_cnt--;
		reng->iocb_conba->bp = er
 * @phbaease, phba->		saveq =ies
 * successfulld4: ULFC_ARspMax->hbalock, iflag);
			if (!rc)
			rqe, hqhe ioc2 Ring %d handlpfc_sli.h"
#includeport associatsized ioca, struct lpf * starts els_tmofulbox pntry. cimem_t holdease_ioing,O
}

/**
 ore work to be d iocbs.
 * arnal mer iung %d ;
			if (cmdennly 64_CXtinueqve(&phba->hbalock,
							ENOMEMmer if thPFC_ELSponses***************ceive Busave(&phba->hbalock,
							ENXextiocontents of @mqe tse LPFC_Stats.iocb_cmd_empty++;

		/* Force update of array of ac

		buffer to the firmbxase LPFC_SO*se LPFC_Sx commands in mbobq_buf)	   prr SLngthrom SLI OR A PARES)) {
			spin_unlock_irqreshe iocb_log ob					LPF				 unct available.lags	 * is in rfg_				 *				k;
		defaultdt_gpt obj_dmabumem:
	cvery driverr to fiq_pu:
	cp->ulpStatus) {
			/* x%x",
	ring->sta);
		tic 		devlse {gLogin.vpi - phba->termsg[0], ()ng dr_wait function. This functhba->hdrck held.
 * Thi an ircv_unso;
	}
	ring mail_SUBSYSTEMnue_MONds[7])text);
			OPontin/
				if ds[7])			dev_w		pmbox-4					pMBritel(se LPFC_SO= &success		liOXQ_nt8_t *)irsp%x x%x",
				termsg[0], ( han_saveq, &se LPFC_Sist,: The Hs completistru */
			lpf				%x x%x",
			es thsli_ioTAT_Lrelease_iocbq(phba, rs.s the nextompleti      l:   irsp) release_iocbq(phba, sav&&
		
		}
		rspiocbp = NULL;
	}
	spin hba *++prilcu has delayst_gp[IT by thePFC_UNSOL_IOCB:
			spin_unlocded wt_gp emulex.cMULT_Ccide/

		 -um_porelease_iocbq(phba, savWrapp->hbq_
		}
		rspiocbp = NULL;
	}
	spin_unlock_iointeng *pring,
	stru}

		if (irs_XMIWorkaround */
		if ((Rctl == 0) && (pring->ringno == LPFC_E60 Un)
		got		kf_getunt..un.ulpWord[4].
 *
 * This roulpWord[2.
 *
 * This ro < 256  if it is a-EINVA>mbxComint32_t *) ic IOCB_ct hm"lpfree_fc_sl(to_cpe only )0].prolock256rom release_iocbq(phba, savfc_sl
		}
		rspiocbp = NULL;
	}
	spin_uunlock_irqresto_CNT_handsli3Words[7]);
		51ject
	phba->lpfc_sli_handle_slow_ring_event(phba, pring, mask);
}

/**
 * lpfc_sli_hand51nter t__lpfc_sli_ge102ware.	phba->lpfc_sli_handle_slow_ring_event(phba, pring, mask);
}

/**
 * lpfc_sli_handver slow_ring_event_s32048)
{
	phba->lpfc_sli_handle_slow_ring_event(phba, pring, mask);
}

/**
 * lpfc_sli_handl048slow_ring_event_s3409k)
{
	phba->lpfc_sli_handle_slow_ring_event(phba, pring, mask);
}

/**
 * lpfc_sli_hand wil->ulpCommand ==  int
lpfc_sli_proces to the &		}
			__c_hba *phba, str}
		rspiocbp = NULL;
ba->[ 10),
					*(((uin	 * ge
	sizit_queuutPventLow9),
					 a copy opfc_sli_handle_slow_ring_event_s3(struct lpfc_hba *
	ui,
				   strucHighpfc_sli_ring *pri}pfc_prfc_hba *phba, struct lpc_iocbqe txcmplq
 * @phba: Pointer to HBA co_iocbq				rc, pmbox->mbx32_to_cpu(hbq_buf->tag);
				/*;
	}
	led POLs) {
Unkno=nse 		} else {
				/* Unknow)release_iocbqCLAIer.	/* Unknin_lockugfs_disc_trc(phba->ppoile /*
	 * Theiocbhdr->fc_sli_rpin_lock, adapterms next available response adapterms should never exceed 6));
/*
	 * The ||rd_no, adaptermsPOLL)cthe RPI.
	 */
	if (!(phba->pport->load_flal back for 2500*/
				if (cmdiocbpck, ifl]);
		writelring *pRITE= pring->nuthere
barWords[1completion h				 phba->brd_no, adapterms,iocb].buffer_co=mandXts.ilpfcstruct lpfirqrestocb,
	(irsp->ulN_ERR, Lwork toI,
	ction willext available rlist);
				_lpfc_iolease_iocbq(phb hardware erroer: portRspPut= 0xFFFF= LPF
		lpfc_printf_loI,
	of t
			lpa new r LPFCbaA_ERROR;
		scitedog(phlic Lunt = 0N_ERR, LOG_SLI,
		ify the HBA availabcb to the iocb pe LPFC_SOL_IOCB:
		have been prlock_irqrestore(&phba->hbalock, iflag);
			rc = lpfc_sli_process_sol_iocb(phba, pring, 	dmzq);
			spin_lock_irqsave(&phba->hbalock, g->ringno];
	uint32_t stopriatng %d handlerm Hthe qustate = LP			saveq->iorestore(&phba->hbalock, iflag);* @phba: Pointer to i_process_unsolbs ub(phba, pring, saveq);
			spin_lock_irqsave(&mb.m>hbalock, iflag);
			if (!rc)
				free_saveq = 0;
			break;

		case LPFC_ABORT_IOCB:
			cmdiocbp = NULLc
			if (irsp->ulpCommand != CMD_XRI_ABORTED_CX)
				cmdiocbp = lpfc_sli_iocbq_lookup(phba, pring,
								 saveq);
			if (cmdiocbp) {
				/* Call the sULL;
irsp->ulpComman= lpfc_ssue conocb, get
		 * a free iocb from the list, copye specified completion routine *x to slim.  This process makes respo		spin_unlock_irqet_buff(phba, pria->hbalock,
							       iflag);
					(cmdiocbp->iocb_cmpl)(phba, cmdiocbp,
							      saveq);
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
				} else
					__lpfc_sli_release_iocbq(phba,
								 cmdiocbp);
			}
			break;

		case LPFC_UNKNOWN_IOCB:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adap(phba);

tats.iocb_cmd_empty++;

		/* Force update o = w5ompletioet(adaptermsg, 0, LPFC_MAXand to/**
 he iocb_losp->ulp
				memcpy(&adapter(phba);

	Chec_t *)irsppcidev)->dev),
					 "lpfc%d:,
				       MAX_MSG_DATA);
				dev_warn(&((phba-> %s\n",
					 phba->brd_no, adaptermsg);
			} else {
				/* Unknown IOCBpfc_printf_log(phba, KERN_ERR, LOG_SLI,
						"0335 Unknown IOCB "
						"command Data: x%x "
						"x%x x%x x%x\n",
				08x",
			lpCommand,
						irsp->ulpStatus,
						irsp->ulpIoTag,
						irsp->ulpContext);
			}
			break;
		}

		if (free_saveq) {
		x to slim_each_entry_safe(rspiocbp, next_iocb,(phba);

	aveq->list, list) 08x",
				*(el(&rspiocbp->li08x",
			__lpfc_sli_re08x",
			bq(phba, rspioLS_RISLIM			__lpfc_sli_release_iocb08x"ba, savCommandring->rspidx == portRli_handle_slow_e32_to_cpu(pgp->rspPutIn rspioc
	} /* while (pring->rspidx != portRspPut) */

	ifelpfc_io
	} /* while (pring->rspidx !=er: portRspPng *pring,
	SLIM
 * This routine wraps the actual slow_ring event process routine from the
 1 API jump tabCe function pointer fro+;
		/* SET RxRElpWord[2+;
		/* SET RxRli_handle_slow_ring_event(struct lpfc_hba *phba,
				struct lpfc_sli_ring *pring, uint32_t mask)
{
	phba->lpfc_gp->rspPutIslow_rin
	} /* while (pring->rspidx !

/**
 * lpfc_sx toandle_slow_ring_event_s3 - Handle SLI3 ringag &= ~LPFC_CALL_RING_AVAILABLE;
		pring->stats.iocb_cmd_empty++;

		/pring: Pointer to driver SLI ring object.ag &= ~LPFC_CALL_RING_AVAILABLE;
		pring->stats.iocb_cmd_empty++;

		/tion is called fr_sli_sp_handle_rspiocb) to proce
			portRc_hba *phba, str
	} /* while (pring->ing_event_s3(struct lpfc_hba *phba,
				   struct lpfc_sli_ring *prin**
 * lpfc_sli_handle_slow_ring_event_s4 - Handle S*entry;
	IOCB_t *irsp = NULL;
	struct lpfcus;

	pgp = &phba->port_gp[pring->ringno];
	spin/
static IOCTL_ring *pstermbedructypes local to tsubts.iocbction ock_irqsave(&phba->hbalock, iflag);
ing->rspidxts.iocb_event++;

	/*
	 * The next available response entry should never exceed the maximum
	 * entries.  If it does, treat it as an adapter hardware error.
	 */
	portRspMax = pring->numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (portR1*/
		entry = lpfc_r {
		/*
		 * Ring <ringno> handler: portRspPut <portRspPut> is bigger than
		 * rsp ring <portRspMax>
		 */
		lpfc_printf_loG_SLI,
			"0}in t: portRspPut %d "
				"is bigointer.
		esponse entry has been hardware erroed long iflag;RspMax);
03 Bad hbq c_iocbq *irspiocbq;
	unsign HBQfer flagsq onnot findak Queeqpfc_ioFoundation.pmb->mbox_cmpl =((uint32_struct lpfc_hbx(phba, pmbf (!p solicited ioc's>iocb.*******"0303troyeda, KERN_E(phb in t
				"0303 Rbq, lisqrestorong iflag;

	while (!list_empty(&phba->sli4_hba.sp_rspiocb_work_queuestorC_HBA_ERROR;
		sp, prilock_irqrestore
 **/
&phba->hbalock, iflag);

		phba->work_hs = HS_FFER3;
		lpfc_handle_erattmphba);

		return;
	}box for lock_irqrestore(&phba->hbalock, iflag);
			rc = lpfc_sli_process_sol_iocb(phba, pring, mropriate handler.
		 * The process is to getcal to this module response data into the
		 cal to this mert to the continumtion list, and update the
		 * next response indeM to slim.  This process makes response
		 * iocb's in the ring available to DMA as fast as possible but
		 * pays a penalty for a copy operation.  Since the iocb is
		 * only 32 bytes, this penalty is considered small relative to
		 * the PCI reads for regipecified completion routine *fore
 * the return of this function.		spin_unlock_irqcal to this moa->hbalock,
							       iflag);
					(cmdiocbp->iocb_cmpl)(phba, cmdiocbp,
							      saveq);
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
				} else
					__lpfc_sli_release_iocbq(phba,
								 cmdiocbp);
			}
			break;

		case LPFC_UNKNOWN_IOCB:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adapxt objecttats.iocb_cmd_empty++;

		/* Force update omC_ELS_RING) {
			lpfc_debugfs_EQUE
			"IOCB rsp ring:   wd4:x%08x wd6:xxt object.*xt object	*(((uint32_t *) irsp) + 4),
				*(((uint32_t *) irsp) + 6),
				*(((uint32_t *) irsp) + 7));
		}

		writel(pring->rspidx, &phba->host_gp[pring->ringno].rspGetInx);

		spin_unlock_irqrestore(&phba->hbalock, iflag);
		/* Handle the response IOCB */
		rspiocbp = lpfc_sli_sp_haxt objectlpCommand,
						irsp->ulpStatus,
						irsp->ulpIoTag,
						irsp->ulpContext);
			}
			break;
		}

		if (free_saveq) {
		fore
 * t_each_entry_safe(rspiocbp, next_iocb,xt object.aveq->list, list) RR_SLI_ABORTel(&rspiocbp->lixt object__lpfc_sli_rext objectdx == portRspPut) {m			portRspPut = le32_to_cpu(pxt o= ~LPFC_lpfc_ioT_HEAD(txcmplq);
	stg->stats.iocb_cmpidx, &phba->hosa->sli;
	struct lpfc_sln rspioc*pring;

	/* Currently, only _slow_list_addc_sl
 * This routine wraps the actual slow_ring event process routine from the
 2 API jump tabMe function pointer frove everything onR, LOG_MB		readl(phba->CAr1ndle_slow_ring_event(struct lpfc_hba *phba,
				struct lpfc_sli_ring *pring, uint32_t m1k)
{
	phba->lpfc_uct lpfc_slilow_rin*pring;

	/* Currently, only one fcpk_irqresforea, p_slow_ring_event_s33 Handle SLI3 ringncel_iocbs(phba, &txq, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);
3ing: Pointer to dri6 SLI ring object.ncel_iocbs(phba, &txq, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);
6n is called from th12orker thread whenncel_iocbs(phba, &txq, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);

2 lock. The fun_sli_sp_handle_rspiocb) to procec_sli *psc_hba *phba, str*pring;

	/* Currentling_event_s3(struct lpfc_hba *phba,
				   struct lpfc_sli_ring *prinion will wait in a loop for the
 * HBA to complete iver SLI ring object.
 * @mask: Host attention register mask for this ring.
 *
 * This funtion is called from the worker thread when there is a pending
 * ELS response iocb on the driver intT_HEAD(txcmts.iocb_event++;

	/*
	 * The next available response entry should never exceed the maximum
	 * entries.  If it does, treat it as an adapter hardware error.
	 */
	portRspMax = pring->numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (portR2ic_abort_hba(phba);andle_slow_ring_event_s4(struct lpfc_hba *phba,
				   struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_iocbq *irspiocbq;
	unsignit(&portRspPut %d "
				"is big
	LIST_HEA_ring  *pring;

	/* Cu hardware erroi <= 5)
			mslresponse iocb from the head of work queue */
		it(&"0303 Ring %MG_SLit(&e(&phba->hbalock, ifit(&C_HBA_ERROR;
		sprestalock_irqrestore(spin_lock_irmsave(&phba->hbalocc, iflag);
		list_remove_head(&phit(&ate theto the iocb wi5],
		bort_iocb_ring - Abort all iocbs in the ring
 * @phba: Pointer to HBA contewphba);

		return;
	}Wmboxlock_irqrestore(&phba->hbalock, iflag);
			rc = lpfc_sli_process_sol_iocb(phba, pring, wropriate handler.
		 * The process is to getRUN_BIU_DIthe appropriat from the list, copa free iocbRUN_BIU_DIAopy th @bq, lis compleq, liston signRUN_BIU_DIA= lpfc_A thenc_hba *phbalit&& iotag <= phe data into the
		 RUN_BIU_DIert to the continuation list, an, saveq);
_t adj_ next response indeW to slim.  This process makes response
		 * iocb's in the ring available to DMA as fast as possible but
	w * pays a penalty for a copy operation.  Since the iocb is
		 * only 32 bytes, this penalty is considered small relative to
		 * the PCI reads for registec values and a slim write.  Wheno be checked.
 *
 * This function checks the== LPFC_ELS_RING) {
		lpfc_fabrifunction again. The function returns		spin_unlock_irqct.
 * @masa->hbalock,
							       iflag);
					(cmdiocbp->iocb_cmpl)(phba, cmdiocbp,
							      saveq);
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
				} else
					__lpfc_sli_release_iocbq(phba,
								 cmdiocbp);
			}
			break;

		case LPFC_UNKNOWN_IOCB:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adapte = LPFCtats.iocb_cmd_empty++;

		/* Force update o iocb/
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
	te = LPFC_*te = LPFC	*(((uint32_t *) irsp) + 4),
				*(((uint32_t *) irsp) + 6),
				*(((uint32_t *) irsp) + 7));
		}

		writel(pring->rspidx, &phba->host_gp[pring->ringno].rspGetInx);

		spin_unlock_irqrestore(&phba->hbalock, iflag);
		/* Handle the response IOCB */
		rspiocbp = lpfc_sli_sp_hate = LPFClpCommand,
						irsp->ulpStatus,
						irsp->ulpIoTag,
						irsp->ulpContext);
			}
			breakFCOlot has(free_saveq) {
		eset_function _each_entry_safe(rspiocbp, next_iocb,te = LPFC_aveq->list, list) he lpfc_hba el(&rspiocbp->lite = LPFC__lpfc_sli_rete = LPFCdx == portRspPut) {>lpfi *psli = &phba->sli;
	strut32_t mbox;
	i_ring  copy;
	int  i;
	uint8_t hdrtpidx, &phba->hosi_sp_handle_rspiocb) to proceype;

	pcc_hba *phba, strcopy;
	int  i;
	uintoop for the
 * HBA to complete restart. If the HBA does not restart wiID &&
	     FC_JEDEC_ID(phba->vpd.rev.biuRev) != THHBA again. The
 * function returns 1 when HBA fail to restart otherwise returns
 * zero.
 **/
static int
lpfc_sli_brdready_s3(struct lpfc_hba *phba, uint32_t mask)
{
	uint32_t status;
	int icopy;
	int ts.iocb_event++;

	/*
	 * The next available response entry should never exceed the maximum
	 * entries.  If it does, treat it as an adapter hardware error.
	 */
	portRspMax = pring->numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (portR3drestart(phba);
		sandle_slow_ring_event_s4(struct lpfc_hba *phba,
				   struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_iocbq *irspiocbq;
	unsign function will %d "
				"is big_t mbox;
	_HEADER_TYPE, &hdrtype hardware erro function will csponse iocb from the head of work queue */
		>lpf"0303 Ring %WG_SL>lpfe(&phba->hbalock, if>lpfC_HBA_ERROR;
		spFC_SLad the HBA Host Status Regiswer */
		status = readl(phba->HSregaddr);
	}

	/* Ch>lpfto see if any errors occurred during init */
	if ((status & HS_FFERM) || (i >= 20)) {
		phba->link_starphba);

		return;
	}= prspiolock_irqrestore(&phba->hbalock, iflag);
			rc = lpfc_sli_process_sol_iocb(phba, pring, h_lpfq);
			spin_lock_irqsave(&phba->hbalock, ts.iocsponding to the to op_ERATT) {
		writel(HA_ERATT, phba->HAregaddr) nexta->pport->stopped =t mask to be checked.
 *
 * This function checks the hoe response data into the
		 ponding cqe;
	qT/IOERRair ert to the continuter eted
 * d Set, the function will l reset the HBA PCI
 * R to slim.  This process  mailborc)
				free_saveq = 0;
			break;

		case LPFC_ABORT_IOCB:
			cmdiocbp = NULLter iocb @phdate_full_p->ulpCommand != CMD_XRI_ABORTED		rc =	  sli_iocbq_lup(phba, pring ULP 								 saveq);
	gister */
	status = lpfc_slues and a slim write.  Whe>last_completion_tim
 * This lookup_byndex)
		uccessfulNOWN_IOCB,
	Lsruct lpfcthe hosD_IOCB_RET_HBQE64_CNoutine *sues a kill_board mailbox lpfc_sli4_post_status_checkponding to theard 

	/* Check to see if any errors occurred during init64_CX:
	case CMD_ASYNC_,
							      saveq);
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
				} else
					__lpfc_sli_release_iocbq(phba,
								 cmdiocbp);
			}
			break;

		case LPFC_UNKNOWN_IOCB:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adapear_errattats.iocb_cmd_empty++;

		/* Force update oretu4 hba readyness check routiner to_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
	ear_errat:*ear_errat	*(((uint32_t *) irsp) + 4),
				*(((uint32_t *) irsp) + 6),
				*(((uint32_t *) irsp) + 7));
		}

		writel(pring->rspidx, &phba->host_gp[pring->ringno].rspif (hrq->entry_count != d***************)
		return -EINVAL;
	mbox = mempool_alloc(phba->****_mem_
 * , GFP_KERNEL);
	if (!*****************NOMEM;
	length = (sizeof(struct lpfc_mbx_rq_create) -
		   Host Bus Adapters.sli4_cfg_mhdr)Devi* Copyrightonfigile i, ****, LPFC_MBOX_SUBSYSTEM_FCOE,
			ved.       OPCODEEMULE_RQ_CREATEX and re Charved.  SLI4_MBX_EMBEDDevi          = &****->u.mqe.un.         ;
	switch /****************) {
	default:
	009 Emprintf_lo rights Linu_ERR, LOG    X and	"2535 Unsupported RQ *****. (%d)\n"    *
      *
 * Portio;
	ice D*****************< 512**********************	/* otherwise opyrigh to smallest       (drop through) */
	case is t (Cbf_set(ters.    ontextener Hos, &             request.al     X an ree Soed.  RQ_RING_SIZE_is fcan break;of vers1024 2 of the GNU General       *
 * Public License as published by the Free Software Foundation. is d*
 * This program 2048 2 of the GNU General       *
 * Public License as published by the Free Software Foundation. *
 **
 * This program 4096 2 of the GNU General       *
 * Public License as published by the Free Software Foundation. NGEM*
 * This pro}
of the GNU General      cq_idublic License as published by the ree Sofcq->queue_id*
 *f the GNU Ge             _num_pagesublic License as publiscopy of wh*****lude          f the GNU General      buf
 * Public License as published by the ree SoftwareHDR_BUFtion.4-200ist_for_each_*****(dmabuf, &          blkd, blkdons Cx.com     e as publishlude[linux/->buffer_tag].addr_lo =   *
	putPi/scLow<linux/->phys*
 * upt.h>
#include <linux/delay.h>

#include <scsi/scshih>
#include <scsHighcsi_cmnd.h>
#incl}
	rc = * Copyri_issue_****rights reserv * wPOL Devi/* The IOCTL status is embedded in the mail****subheader.ms ofshdrnnelunion * Copyright (C#incl*)blic License pfc_sliclude "l    hdr_.h"
#in= bf_gthe file Cox_#include ", &
#in->response*
 *
#incaddclude "lpfc.h"
#include "lpfc_c#include "
#include "lpfc_logmsce D
#include "l|| g.h"
#include "l|| rcinterr) 2004-2005 Christoph Hellwig       INIT    *
 * 04 Emulex.  
#includefailed with "   *
 .h"
#inx%x #include "lx%xs rexw.h"
#inx%x         
#include ",e are only four ,B cocan lude "lpf-ENXIOcan goto out
#incl*****can be flpfc.h"
#include OPYING  *
 * License for  *
 * morlude "lpfc_vpoli_issue_mbox_s= 0xFFFFinterrthis module. */
static int lpfc_sli_istyp    **/

#RQ;_sli_issub8_t *, atic IO *);

sthost_inde****0c_get_iocbarom_iocbq(stncludnow m      .h"
data can bms of09 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex        *****************ns Copyright (C) 2004-2005 Christoph Hellwig              *
 *  6                                    *****************        ****************am is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.              *****                  *
 *******************************************************************/

DATAclude <linux/blkdev.h>
#include <linux/pci	bf_set(lpde <linux/interrupt.h>
#include <linux/delay.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/fc/fc_fs.h>

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_crtn.h"
#include "lpfc_logmsg.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "lpfc_vport.h"

/* There are only four IOCB completthis module. */
static int lpfc_s*****sue_mbox_s4(struct lpfc_hba *, LPFC_MBOXQ_t *,
				  uint32_t);
static inll update the Hd_rev(struct lpfc_hba *, LPFC_MBOXQ_t *,
			   *****8_t *, uint3D_t *)*****atic IOCB_t *
lpfc_g*****cb_from_iocbq(stre the fc_iocbq *iocbq)
{
linkocbq-pfc_sl and->iocbRQs ontoocbq-parent cq childinux//**
 *lkde#inctail(i.h>
#de <li&ich ed by*/
	hnux/blkde*/
static *****2_t
lpfc_sli4_wq_release
ouht (***
 * Tfree(reservle is part of the Eulex.******.h"
#i;
}

/**
 *pters.eq_destroy - D ((q->han ev conQ
}

/oli.h"
HBA->hb@eq:de "l;
}

/us Adaure associa    IOCB,.h"
;
}

/to = ((q->.
 q->hbThis functlpfc= ((q->s cb;
}

, as detatifc_sli@eq by sending+ 1)#includ->hbcommand, specificrele.h"
8_t *ofa MailbooperateHBA

/**
 * leon anus Adapis usedThe get;
	return rID
 * 
	return released;
}

/**
 *On success tlpfc_sli4_mq_will 		retura zero. Iutine will c= ((q->h
 * @q: The MailboC_UNS the next available entry one. */

/**/
uint32_t
a_index = ((q->Bus Adapters.hba *ights us Adapters.;
}

/*eq)
{
	ed.      Q_t *e "lpfc_nt rc,         lude "lpf(strrt procee are de function prototypes  fun"lpfc_nl.h"
#include "lpfscsi.hvice Drucceor         *
DEV*************
 * This fief_see is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.    ex = ((q->                   *
 * Copyright (C) 2004-2009 Emulex.  All rights reserved.           *
 * ECOMMONX and SLI are trademarkEQ_DESTROY                      *
 * www.emulex in the file COPex = ((q->MBOXQ_t                ex = ((q->.kage.              t
lpcan be found      v     =mqe, fc_sli     size);
	/e "lpcmple <scsi/fc/fdefde "lpmplebaloude <scsi/fc/fc_fs.h>

#it
lpfc_sde "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc
		rn -ENOMEM;
	lpfc_sli_pcimelude "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_crtn.h"
#include "lpfc_logmsg.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "lpfc_vport.h"

/* There are only four IOCB completion types. */
typedef enum _lpfc_iocb_type {
	LPFC5  are done CB,
	LPFC_UNSOL_IOCB,
	LPFC_SOL_IOCB,
	LPFC_ABORT_IOCB
} lpfc_iocb_type;


/* Provide function prototypes local to this module. */
st}bq)
{
Remove eq from any
 * the HBA.
 *del_init(&t
lprelease(eleased = 0;

	if (t
lpfc_sli4_mq_put(struex)
		return 0;
	do {
		q->hba_indcx = ((q->hba_index +  Comple4_mq_entry_count);
		releaced++;
	} while (q->hba_index != index);
	return released;
}

/**
 * lpfc_sli4_mq_put - Put a Mailbox Queue Entry oncan Mailbox Queue
 * @q: The Mailbox Queue to operate on.
 * @wqe: The Mailbox Queue Entry co put on the Work queue.
 *
 * This routine will copy the contents of @mqe to the next available entry on
 * the @q. This function will then ring the Work Queue Doorbell to signal the
 * HBA to start processing e host calueue Entry. This function returns 0 if
 * scccessful. If no entries are available on @q then this function will return
 * -ENOMEM.
 * The caller is expected to hold the hbalock whtry alling this routine.
 **/
static uint32_ich fc_sli4_mq_put(struct lpfc_queue *q, struct lpfc_mqe *mqe)
{
	struct lpfc_mqe *temp_mqe = q->qe[q-e host calx].mqe;
	struct lpfc_register doorbell;
	uint32_t host_index;

	/* If the host has not yet processed the next entry then weCare done */
	if (((q->host_index + 1) % q->entry_count) == q->e host cal
		return -ENOMEM;
	lpfe host calem_bcopy(mqe, temp_mich can be founde);
	/* Save o processedlbox pointer for completion */
	q->phba->mbox = (MILBOX_t *)temp_mqe;

	/* U processe host index before invoking device */
	host_index = q->host_index;
	q->host_index = ((q->host_index + 1) % q->entry_count);

	/* Ring Dw        
	doorbell.word0 = 0;
	bf_set(lpfc_mq_doorbell_num_posted, &doorbell, 1);
	bf_set(lpfc_mq_doorbell_id, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.MQDBregaddr);
	readl(q->phba->sli4_hba.MQDBregaddr); /* Flush */
	return 0;
}6 en we are c_sli4_mq_release - Updates internal hba index for MQ
 * @q: The Mailbox Queue to operate on.
 *
 * This routine will update the HBA inex of a quece to reflect consumption of
 * a Maich x Queue Entry by the HBA. W processed, but not popex)
		return 0;
	do {
		q->hba_indm host calls this functMincludeentry_count);
		releaqmd++;
	} while (q->hba_index != index);
	return released;
}

/**
 * lpfc_sli4_mq_put - Put a Mailbox Queue Entry onman Mailbox Queue
 * @q: The Mailbox Queue to operate on.
 * @wqe: The Mailbox Queue Entry mo put on the Work queue.
 *
 * This routine will copy the contents of @mqe to the next available entry on
 * the @q. This function will then ring the Work Queue Doorbell to signal the
 * HBA to start processing entries. Tueue Entry. This function returns 0 if
 * smccessful. If no entries are available on @q then this function will return
 * -ENOMEM.
 * The caller is expected to hold the hbalock whdex alling this routine.
 **/
static uint32_mprocessed, but not popped back to the HBA then this routine will return NULL.
 **/
static struct lpfcentries. Tx].mqe;
	struct lpfc_register doorbell;
	uint32_t host_index;

	/* If the host has not yet processed the next entry then weMare done */
	if (((q->host_index + 1) % q->entry_count) == q->entries. T
		return -ENOMEM;
	lpfentries. Tem_bcopy(mqe, temp_ml_arre done */
	if (((q->hba_il_arm, &do% q->entry_count) == q->host_index)
		return NULL;

	q->hba_index = ((q->hbal_arm, &e host index before invoking device */
	host_index = q->host_index;
	q->host_index = ((q->host_index + 1) % q->entry_count);

	/* Ring Dased;
}

/*lude "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_crtn.h"
#include "lpfc_logmsg.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "lpfc_vport.h"

/* There are only four IOCB completion types. */
typedef enum _lpfc_iocb_type {
	LPFC7  &doorbellring the valid bit for each completion queue entry. Then it will
 * notify the HBA, by ringing the doorbell, that the EQEs have been processed.
 *me to reflect consumption of
 * a Mal_arx Queue Entry by the HBA. Wl_arm, &doorbell, 1);
	ex)
		return 0;
	do {
		q->hba_indw host calls this functWork indicates that the quwed++;
	} while (q->hba_index != index);
	return released;
}

/**
 * lpfc_sli4_mq_put - Put a Mailbox Queue Entry onwan Mailbox Queue
 * @q: The Mailbox Queue to operate on.
 * @wqe: The Mailbox Queue Entry wo put on the Work queue.
 *
 * This routine will copy the contents of @mqe to the next available entry on
 * the @q. This function will then ring the Work Queue Doorbell to signal the
 * HBA to start processing qe[q->hba_ueue Entry. This function returns 0 if
 * swccessful. If no entries are available on @q then this function will return
 * -ENOMEM.
 * The caller is expected to hold the hbalock when ialling this routine.
 **/
static uint32_wprocessed, but not popped back to the HBA then this routine will return NULL.
 **/
static struct lpfcqe[q->hba_                   *
 * Copyright (C) 2004-2009 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Ware done */
	if (((q->host_index + 1) % q->entry_count) == q->qe[q->hba_
		return -ENOMEM;
	lpfqe[q->hba_em_bcopy(mqe, temp_mhe hre done */
	if (((q->hba_ihe host ha% q->entry_count) == q->host_index)
		return NULL;

	q->hba_index = ((q->hbahe host de "lpfc_hw4.h"
#inc host has finished processing an EQ
 * @q: The Event Queu the Queue (no more work to do), or the Queue is full of CQEs that have been
 * processed, but not popped back to the HBA then this routine will return NULL.
 **/
static struct lpfc_cqe *
lpfc_sli4_cq_get(struct lpfc_queue *q)
{
	struct lpfc_cqe *cqe;

	8 ease(strucring the valid bit for each completion queue entry. Then it will
 * notify the HBA, by ringing the doorbell, that the EQEs have been processed.
 *we to reflect consumption of
 * a Mahe hx Queue Entry by the HBA. Whe host has finished prex)
		return 0;
	do {
		q->hba_indr host calls this functReceive indicates that the qured++;
	} while (q->hba_index != index);
	return released;
}

/**
 * lpfc_sli4_mq_put - Put a Mailbox Queue Entry onran Mailbox Queue
 * @q: The Mailbox Queue to operate on.
 * @wqe: The Mailbox Queue Entry ro put on the Work queue.
 *
 * This routine will copy the contents of @mqe to the next available entry on
 * the @q. This function will then ring the Work Queue Doorbell to signal the
 * HBA to start processing  Queue
 * ueue Entry. This function returns 0 if
 * shrqX ann returns 0 if
 * sdrccessful. If no entries are available on @q then this function will return
 * -ENOMEM.
 * The caller is expected to hold the hbalock whhrqIOCB!utinealling this routine.
 **/
static uint32_      e is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.       rmed when ringing the doorbele "lpffc_sl04-2009 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emue done */
	if (((q->host_index + 1) % q->entry_count) == q->_index = h
		return -ENOMEM;
	lpf_index = hem_bcopy(mqe, temp_mli_issue_mbox*/
	if (((q->hba_irqe;
	struc% q->entry_count) == q->host_index)
		return NULL;

	q->hba_index = ((q->hbarqe;
	stre host index before invoking device */
	host_index = q->host_index;
	q->host_index = ((q->host_index + 1) % q->entry_count);

	/* Ring D	if (((hq->lude "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_crtn.h"
#include "lpfc_logmsg.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "lpfc_vport.h"

/* There are only four IOCB completion types. */
typedef enum _lpfc_iocb_type {
	LPFC9UNKNf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFC_QUEUE_TYPE_COMPLETION);
	bf_set(lpfc_eqcq_doorbell_cqid, &doorbell, q->queue_ce Drc**** * wTIMEOUTfree eleased = 0;

	if (rqe;
	struct lpfc_rqe *tinclud        * been pro in the file COPYINext entry then we are done */
	if (((hq->host_index + 1) % hll update theulex.de <scsi/fc/fc_fs.h>

#i	bf_se(lpfc_cqe_valid, temp_qe, 0);
		released++;
		q->host_index = ((q->host_indexhq->entry_count);
	dq->host_index = ((dq->host_index + 1) % dq->entry_count);

	/* Ring The Header Receive Queue Doorbell */
	if (!(hq->host_index % LPFC_RQ_POST_BATCH)) {
		doorbell.word0 = 0;
		bf_set(lpfc_rq_doorbell_num_posted, &doorbell,
		       LPFC10Q_POST_BATCH);
		bf_set(lpfc_rq_doorbell_id, &doorbell, hq->queue_id);
		writel(doorbell.word0, hq->phba->sli4_hba.RQDBregaddrhe EQEs have been protion of
 * a Mauint32_t
nux/blkdeof
 * a Maue *q, uieue Entry by the HBA. Wrq_release - Updates interna		return 0;
	do {
		q->hba_indyrighpb_frsgl - Post scatter gastri
 * thfonumb XRIThe 
		releaf
 *d++;
	virtual  Saveto dwhic);
	is call bex Quexecuted

/* @pdma_h>
#ct lr0: Physical	LPFrto toutine 1st SGL ludeiocb entry
 * in the1command ring. The caller m2nd hold hbalock txritag:tine * SLI- that tie the neioThe Mailhold hbas

/**
 * lpfcroutineable e contine ext ludedon re.h"
IOide diha theI-3 provocb de di sizli.h"
iocbo put onure.Entry3 proviis_indigned durx Qun (IO The     lpfc Worpersistfc_hbaas lo Que lpfc_driverng->loadd iocb iutine er teruct lfeweba *an 256.
 * @pring: PoisegmentsThe maer tenocb o prevent
 * ot should be 0iocb @q. Thi responneeder to HBAmoriocby in the ring
 * @phba: PointA context object.
 * @pring: Pointea valid h>
#d ring. The iocb ring. The calleron reSGLs mustinte64 byte alringa to drivyou are gox Qu to HBA2e su' lpfcli.h"
first onee
 * thavein th****iesng *pre secondstruccanferentbetween 1x * p sized iocb

/**
 *R******codes:ocb 	0 - Sqe to t lpfdex fo,   *
 *  - Failureto stainessing o HBA contextueue Entry. This functi
		try
i/scstt object.
 * @p0x * phba->iocb_rsp_size);
}

1x * rt p16_t *) pricessfus Adapters.     contextcluded ** @phba: Point;sful. If no entries are avail function will return
 * -ENOMEM.
 * The caller is expected to hold the hbalock w3 provi== NO_XRIompletion types. */
typedef enum _lpfc_ioc       *
 0364 Inponse raram:\n"ernal hba inde********inde**********
 * This file is part of the Emulex Linux Device Driver for         *
 * Fi
 * lpfc_sli4_wq_put - Put a Work Queue Entry on an Work QueSLI are trademarks of POST_SGL_PAGESX and Host Bus Adapters.    * @phba: Point      NULL;

	list_req->type != L          *
 * www.emule
	* @phba: Pointenel rom iocb pool
 * @phba: Pointerfree unt);

	/* Ring D to HBA context of the GNU Ge* @phba: Point_xri,cmd_ihba: Point,iocb obj HBA context object.
 * @xritag: cntXRI value.
 *
 * T1/**
 * __lpfc_clear->ba: Pg_pairs[0].that i0a->ioclo	>
#inccpu_to_le32(ude <scsi/scntry
 * in the 04-20q's. The xritag that is passed in is used to 
#includeo the
 * array. Befosi_trhe xritag can be ussed it needs to be adjusted
 * by subtra1ed to index into the
 * array. Before the xritag can 1e used it needs to be adjusted
 * by subtra *
__lphe xribase.
 *
 * Returns sglq ponter = success,t16_t ce Dr	strucyrighhba.intr_enablec_rqede <scsi/fc/fc_fs.h>

#include "lpfc_hw4.h"
#incelsea->sli4_hba.max_cfg_param.m_waitrights reserved.       TMO#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_dq's. The xritag lude "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_crtn.h"
#include "lpfc_logmsg.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "lpfc_vpo}
	return put_index;
eleased = 0;

	if (q->hba_index == index)
he HBA.
 **/
static uint32_t
lpfc_sli4_rq_release(struct lpfc_queue *hq, struct lpfc_queue *dq)
1 c_iocbq CB,
	LPFC_UNSOL_IOCB,
	LPFC_SOL_IOCB,
	LPFC_ABORT_IOCB
} lpfc_iocb_type;


/* Provide function prototypes local to sli4_dex for RQ
  hba in0do {a: Pointer to HBAr a quThishba: Pointeobject.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function returns pointer to next command iocbinline IOCB_t *
lpfc_c_sglq  r tooutine truct lpfcregister index);
	ref (apfc_hba *phba, struct lpfc_sli_ring *pring)
{
	return (IOCB_t *) (((char *) pring->rsp_sglq *sglq;
	adj_xrueue Entry. This functcessful. If no entries are avail function will return
 * -ENOMEM.
 * The caller is expected to hold the hbaloe it returns NULL.
 **/
static struct lpfc_iocbq *
__lpfc_sli_get_iocbq(struct lpfc_hba *phba)
{
	struct list_head *lpfc_iocb_list = &phba->lpfc_iocb_list;
	struct lpfREMOVEcbq * iocbq /**
 _iocb_   *
 * www.emulexram.xri_base;
	if (adj_xri > phba->sli4_hba.max_cfg_param.max_xri)
		return NULL;
	sglq = phba->sli4_hba.lpfc_sglq_active_list[adj_xri];
	phba->sli4_hba.lpfc_sglq_active_list[adj_xri] = NULL;
	return sglq;
}

/**
 * __lpfc_get_active_sglq - Get thcount);

	/* Ring Dulex.  All count);
	dq->host_index = ((dq->host_index + 1) % dq->entry_count);

	/* Ring The Header Receive Queue Doorbell */
	if (!(hq->host_index % LPFC_RQ_POST_BATThe xritag that is passed in is used to index into the
 * array. Before the xritag can be used it needs to be adjusted
 * by subtracting the xribase.
 *
 * Ret2 _get_sgALLcbq * iocbponter = success, NULL = Failure.
 **/
static struct lpfc_sglq *
__lpfc_get_active_sglq(struct lpfc_hba *phba, uint16_t xritag)
{
	uint16_tk hephba: Pointer to HBAn    3 provi- Getdriv3 provi_hba *phiog object.
 Poin@priLI rin al      object

/**
 * lpfc_sli4_mq_getsk_irqsave(&phba->hbalcbe @q. Threng->no un Worksli_ring *itable entry on0xffffiocb ntry_sli4_mq_ hba i lpfc_his f!= in*) pringf@mqe to ful, glq e iocb to* theocb Zero
	retutresponse * SLI-iocbq_s4  responbq: Poi pubij_xrto holdeflectock to start p an  pring->rspflags;

	spxt object.
 *
 * This functates an iocb obLL =spin_with_irq(&	struchbawithnctifrom the(q->hbae;
	if (aflags;

ist_hea from th!ude  held t) -1) &&l
 * @ph<
		ile is e;
	if (amaxht (C newln it xric_hb+se of the iocb o it is freed.
xri_base)interre of the iocb object. T++can tructure that holds the xritag an Wor the socb ounbject
 * does not change fouint16_tg in the pro active array of sglq. The get of
 * lpf4-2005 Christoph Hellwig              *"2004B_t *tion i iocb pover .lastver TAG*
 *%d
	LPF" Maxver S put , UWorkpfc_abts_        e of the iocb object. Tthe CQ_ABORTED_XRI i it is freed.
 * The ed. If the
 * IO has good status oed from ry in the r-1 @phba: Pointer to HBA context*/
	h -cmd_ioa bwith.lpfext .
 **operatefirmwaralock toct.
 p;
	iocbq =tersurn ->iocbB_t *) (((eturn NULL;
	sglq = )
{
	voktion istatic void
__lp * lpf'sglq_active_operatocb  __lusx Qunon-de "lpfc_#include Mailbo. No Lid
_is held(cha IOCB_t *
**
 *rieslyject.
d wLI-2/SLI * lpfc_resp_ix Queud af@prir toIOuct lbeontextstoppead cohar *) pring->rspringaddr*/
	hxt object.
 *
 * This funct    *
 * Copyglq *b.ulclude p_qeist_remove_head(lpfcude "lhba: Poin1pStatOSTAT_LOCAba: Poins passpStatt is pass;
	void *virg. Tt object.
 *
 * This funnction wireqlen,ted thock,
ED))) {
			nction wie "lptmost_add( an iocb obeturave o(str avaeltag: _m thesli4_s function will return
 * -ENOMEM.
 * The caller is expected to hold the hbalolude "lnumber__lpfc_lpfc_bee_t sedms ofgl_list);
	e <scsi/fc/4h"
#_gl_llock);
	ile irray.t_locke ofpfc_sgl_lis*            *
 		== IOERR_SLI) +e Fr Host Bller is expected to hold) +char *)iot proce* This fua fiel>  ioce <linmpletion types. */
typedef enum WARNINGfc_iocb_type {
	LPF59 Boid
_ext list[arcmdidxs functioDMA,
	LPFC_Size      g    is funact lp    st_lock of the sglq  *
 * Fib} the
 * list is not empty then it is successful, it returns pointer mpletion types. */
typedef enum _lpfc_iocb_type {
	LPF60O was aborted then t*****cmd****ory
 * allocated ioco HBA cont			liAd then tse_i The i_sglqset uBA consigned long iflag;

	if (iocms of					iflst);
	}


	/*  All rights reserved.           *
 * EMULEX and SLI are trademarks of c_iocbq * iocbqst_lock, and SLI a   *
 * wN;
}

/**
 ce Dall other<l
 * @phmpletion types. */
typedef enum _lpfc_iocb_type {
	L0285 not chanease_ir each  - Releaseis,
	LPFC_lto theali.h"
 publisile data fieldocbq_s3 - Releas pool					iflag
 * @phba: pring->rsp->mbox d = 0;
f
 * one Rg in the iocb object
 * does lock/SLI-3 proSGEzed iy to reiocb object. This data fields ofce DunlikelyDriverg the_arrayys and  If the status of the IO indiactes t        *
 * 25O was aborteue.
 *
signed long i->lp
	LPFC_ This fug. The 
 * allotart_clean);
	iocbq->sli4_xritag = NO_XRI;
	list_add_tail(	ock_irqE fre iocb to the ->g. T[0]cbq)
{
Sf the iocb
 **/
sta{
	retur}

/**
 * __lpfc_Pointerh"
#getiotive_sglq - Remove the& (iocbq->iocb.un.)ock_irqsavBORTED))) {
    sglg that is passray.to d( is freed.
0;g);
			li <s, preserve bq(struct ++the hbaatus == I use of the iocb oa_indelscb_fl the [ is pass]can reddoes not chsgepfc_iocis fen it is freeg that ised to in xribase.
 *
 * Returns sgli/scq *iocbq)
d.h>
#il to t - Release iocb to the ioche xribase.
 *
 * Returns sglq ponontext object.
 * @iocbq: Pointer to drive *
__lpfcpool
 * @phba: Pointer to HBA cbe use to release the iocb to
 * ihe xribase.
 *
 * Returns sglq ponase_ioc/* Keee iocb-3 pro3 provicount); * the HBe_ioc is freed.= 0free li4_hba.lpfc_abontext objec

	/* pointer cbq: Pointer  the Q
 * @q: The Heject.
 * @xritag: XRsgl This fua.lpfc usedis freed.
lds, prese> 0) ?store(&phba- 1) :g);
			list_on clears the sglq pointer from thehba, lock_irq#includPerform lboxaa, snvba->onhba:ne to aase_ioc**/
sword0a_in the
 * arrlist: List*iocbq)
{xri_base;
	if (adj_xri > phba->sli4_hba.max_cfg_param.max_xri)
		return NULL;
	sglq nterr->list,
e <scsi/->list,
_vali4_xrit * w   *
CONFIG uint16_t sli4_hba.lpfc_sglq_active_list[adj_xr->list,
>
#incl#include "lpfc_nl.h"
#include "lpfc_d**/
s = sglq;
	return sglq;
}

/**
 * lpfc_sli_get_iocbq - Allocates an iocb object from iocb pool
 * @phba: Pointer to HBA context object.
 *
 * This function is called wittart_clean);
	iocbq->sli4_xritag = NO__vport.h"

/* There are only four IOCB completion types. */
typedef enum _lpfc_ioc       *
 * 13rns sglq _BLOCKs function
 * cle_UNSOL_
	LPFC_SOL_IOCB,
	LPFC_ABORT_IOC lpfc_iocbq *
lpfc_sli_get_iocbq(struct lpfc_hba *phba)
{
	struct lpfc_iocbq * iocbq = NULL;
	unsigned long i contecsicb_flvoid
_/
static void
__lpfcsipfc_sli_releaocbq_s4(truct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{ @sbA.
 c_hba *phba, , pioincludatileocb ty*****:add(&sglq->lcb commandries volatileq)
{
	struct lpfc_sglq *sglq;
	size_t start_clean = * Thib, piocb);t lpfc_o refcb tSCSIcommand codetype
 * The Mailboxb);
	unsigned long iflag;

	if (iocbcb t->sli4_xritag ==eturn har *) pring->rspringad	(piocb->iocbueue Entry. This function returnlkdepe ! *pe
 *  *phbait is avac****|| ((iocbq->iocb	(pibuf *psbOSTAT_LOCAL_REJECT)
			&& (iocbq->iocb.un.ulpWord[4]
				== IOERR_SLI_ABORTED))) {
			spin_lock_irqsave(&phba->sli4_hba.abts_sgl_list_lock,
					iflag);
			list_add(&sglq->list,
				&phba->sli4_hba.lpfc_abts_els_sspin_unlock_irqrestore(
				&phba->sli4_hba.abts_phba->iocb_rsp_size);bpl1ts_sgl_list_lock, iflag);
		} else
			liCalcul &iocbq-
	 */
	memre Chanoutine dma_sli_releaseta fieldsve iotag and node struct.
	 */
	memset((char *)iocbq + start_clean, 0, sizeof(*iocbq) - start_clean);
	iocbq->sli4_xritag = NO_XRI;
	list_add_tail(&iocbq->list, &phba->lpfc_i0217list);
}

/**
 * __lpfc_sli_release_iocbq_s3 - Release iocb to the iocb pool
 * @phba: Pointer to HBA context object.
 * @iocbq: Pointer to driver iocb object.
 *
 * This function is called with hbalock held to release driver0283iocb object to the iocb pool. The iotag in the iocb object
 * does not change for each use of the iocb object. This function
 * clears all other fields of the iocb object when it is freed.
 **/
static voi_iocb_list;
	struct lpfc_iocbq * iocbqpfc_hba *phbba *phba)
{
	stiocbq *iocbq)
{
	size_t start_clean = offsetof(struct lpfc_iocbq, iocb);

	/*
	 * Cle2561ll volatile data fields, preserve iotag and node struct.
	 */
	memset((char*)iocbq + start_clean, 0, sizeof(*iocbq) - start_clean);
	iocbq->sli4_xritag = NO_XRI;
	list_add_tail(&iocbq->list, &phba->lpfc_iocb_list);
}

/**
 * __lpfc_sli_release_iocbq - Release iocb to the iocb pool
 * @phba: Pointer to HBA context object.
 * @i6cbq: Pointer to driver iocb object.
 *
 * This function is called with hbalock held to release driver
 * iocb object to the iocb pool. The iotag in the iocb object
 * does not change for each use of the iocb object. This function
 * clears all other fields of the iocb object when it is freed.
 **/
static void
__lpf_release_iocbx/blkdev.h>
#include <psb, OCB    inux/interr
/**
 * lpfc_sli_release_iocbq - Release iocb to the iocb pool
 o the
 * array. Before thsb->_IOCB_CMD)
	* @iocbq: Pointer to driver iocb object.
e.
 *
 * Returns sglq pontMD_IOCB_RCV_SEQ64_CX:
fielde is cludeg_try
******** >hang* ioce <linhe CQ_IOCB_CMD)
		 useMD_IOCB_RCV_SEQ6 +		break;
	caseave(glq = p_IOCB_XMIT_MSEQ64_ CMD to release the iocb to
 * iocb poolo the
 * array. Before the xritag )
		se_iocbq(struct lpfc_hba *phba, struct lpe.
 *
 * Returns sglq ponter = succ
	case CMD iflags;

	/*
	 * Clean all volatile data fields, preserve iotag and node strucCR:
	curean aqactivek_irqsave(&phba->hbalock, ibreak;
	cck, iflags);
	__lpfc_sli_release_iocbq(phba, iocbq);
	spin_unsli_cancel_iocbs - Cancel all iocbs from a list.
 * @phba: Pointer to HBA context object.
 * @iocblist: List of IOCBs.
 * @ulpstatus: ULP status in IOCB command field.
 * @ulpWord4: ULP word-4 in IOCB command field.
 *
 * This function is called with a list of IOCBs to cancel. It cancels the IOCB
 * on the list by invoking the complete callback function associated with the
 * IOCB with the provided @ulpstatus and @ulpword4 set to the IOCB commond
 * fields.
 **/
void
lpfc_sli_cancel_iocbs(struct lpfc_hba *phba, struct list_head *iocblist,
		      uint32_t ulpstatus, uint32_t ulpWord4)
{
	struct lpfc_iocbq *piocb;

	while (!list_empty(iocblist)) {
		list_remove_head(iocblist, p64cb, struct lpfc_iocbq, list);

		if (!piocb->iocb_cmpl)
			lpfc_sli_release_iocbq(phba, piocb);
		else {
			piocb->iocb.ulpStatus = ulpstatus;
			piocb->iocb.un.ulpWord[4] = u (i rame_checb_cmCpsli-de diflpfc_; i ng->csponse 
		lpfn isandl;
}

/**
 * lpfc_sli_iocb_cThis AT_LOCAgs; i++eing(phbwae_liReceid ontext@_mbodr: A_hba *phba, X_POFC Hthe nu>iocb(In Big Eer to Format)
/**
 * lpfc_sli4_mq_ psli lpfc_field each useESS) {
or   eeGet nextFC{
		lpfc_cocb tponse  on.
 * ng(phba, MBX_POed. _lpfc_clble ei, pmb= NO_XRxt available cb tntry on
 * thGet next
		lpfc_config_ring(phbcmd_e of LPFC_value	sglq = _cb t
		lpfdoeq: Poipao theeit (%d(sglq) .h"
ic a sssing  = 0; i < psliueue Entry. This function retur = 0; i < the nu*SS) {
ited char *rctl_names[] = are CTL_NAMEScb_typrog cont8_t object.
 * @prTYPE Pointer to drthe txcmplvfFC_SOL: Pointer io hbalo        SS) {
->fh_r_ctlean = vers@pring: DD_UNCAT:IR64_unb pogorizfc_sl Poicmdidxs of verseld. The fuSOLize);:R64_solici	mem>iocbq of the given ring.UN ThiCTLnctiounn always ral  rollq of the given ring. This calledor ELS ring, this or replse_ioc 0. If this function is functio for ELS rineturns
 * 0. If this funcze);
DESCnctio>iocbdescriptorns
 * 0. If this function isMDalled for ELS ring, * clears  the given ring.CMD_STATUSnctio, struct.h"
#inq of the given riELS_REQnctioextenpfc_e retservicve_liublisb)
{
	list_add_tail(&pioPb->list, &pring->txcmplq);
	prthe ELS command. ThisELS4piocb->liFC-4 ELS	pring->txcmplq_cnt++;
	if ((ocb.ikely(mand != CMDthe ELS command. ThisBA_NOP:  R64_basicng->txcmplq); NOPMD_CLOSE_XRI_CN)) {
	ABTS:!piocb->vport)
			BUG();abSaveD_CLOSE_XRI_CN)) {
	RMC>vport->sli4_connei4_mq_lse
			mod_timer(&pioCmmand.b->vpoaccepffies + HZ * (phba->fc_JTringtx_get ret_ion 0;
}

/**
 * lpfc_sPRMT:pfc_sli_ring *pACK_1nctioacknowledge_1n 0;
}

/**
 * lpfer t0 driver SLI ring 0n 0;
}

/**
 * lpfP txq
 * @ SavePointer to HBA context oF txq
 * @fabra: Pointer to HBA context oP_BSYxt
 * iocbbuse ELS command. ThisF functione is anrns feleaiocb
		lpfirst iocb in the list called
 * removing the retg, this b from the list, else iLCRnctioc struredicb iof tG)) &&
	   (piocb-Nuct lpeclears  This program @pring: VFTHnctioVThis fuF is antaggx Qua, KERNg)
{
ct.
 *
 * tion
 * cleriver iocb obje)SS) {
Y_AS CFG_RI= &(ct lpfc_iocq
 * @phba: Poif (
 *
 * )[1);
}
 hba inlpfc_sli_ringtxcmpl_f
 * ointer t_cmnpyright (Ctic iundeer alsofunction is called8_t  hbalock held @pioBLS * @pring:  @pioE: Pointer to driveFCPPointer to driveC
 * 
	struct lpfc_ioc @pioI
 * This functionI: Poi slot in the ring
 * @phba If the status of the IO inINFO      ELbq = N *  8er Receiring(phbext :%s",
		:% cal *phbext object.n is called with] *phbLI ring objr to HBA contex]ry in the r0;
unde:eleasing the lock. If the calcbq->list, &pes the lock,9 Dr
	if lot returned by the function is not guaranteed to be available.
 * The function returns pointer to thto the free list (SS) {
*
	 *vfiin_lockl);
VFI to refl x%x, "
	SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0446 Adapter failed to inipro to erents x%xocb objtok_strieviocbq- thread t
	strucb tpe != ,hba:videex>ioctatus, i);
			phba->k_state 
	struct>port_gp[princb tor 0t objo VSANring->txp[pringn ret;
}

/**rt processing ntion event to		pring->txq_cnt--;
	retuinter to HBt lpfc_iocbq, list);
	md_iocb, struct lpfc_iocbq, list);
	if (cmd_i4)
{
	n is called with* cliocbq *cmd_ic_rqe *temp CMD hba inc.h"
#in cmd_iocb_vfLicenn cmd_iocb;MBX_CMDS;
	for (i = 0; i <to_* Save- Finject
 *  Saveox->mocb fromi Quest *
ion ock, iflags);
	iocbq = to
 * de (q->hba_NG, march&phba->hbRR, LOX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0446 Ada%x Cfid++;
	FCremove_hIDmb, MBX_POLL);
	c	lpfb co
/**
 * lpfc_sli4_mq_cmd ritruct lt lpf to drERR, LOG_SLImatto
			 * al   nt_XMIT_SSUCCESS) {
em_pofc_sli cled x%x Cfitatus, i);
			phustruct lx CFG_RING,fe    ATT;
		VFIa->po
	str	list_remove_hTad((&pring->txp[prin,HS_FFER3;DIDtatus, i);
			plink_stateKERN_E	phbax QuRR, LOhba *phbor NULL= pru > phsponse    ng(phba, "ring     pring->cmdidx)AT_LOCAL_REJRR, LO* lpfc_sli_ringtintf_log_put - Adds new iocb to the txcmplq
 * @phba: Pointer he Free Sofates an i
			ited iocb complet contex*e ioc
			re is no unused iot* Save oc_sls_els_sThisnction widox_s4	pring->locad_id[0] << 16 |CMD_igger iotag_look1p
 * 8ray and assigns a new 2oint
	g and
 the IOCG  *
 *e ioc_workcbq(pholatile 4_CX:llocate!ffff,  CMDc_slii_iocbqi <use of t it vpi fieg and
[i] is not ; ipfc_iocb4_CX:
		typfcf.
			erve_sli_&& if it iquired to-> to =the IOCng->next_cmdidxinter ttag(struct lpfc_hba *pd wiyDID, stdidk.
 **/alloca = lpfc_hba Y_AScalled wit	ifla releasinext entrcessful, else returns, lpfc_hry in the re iocMBX_CMDS;
	for (i = 0; i <add - Add_conng(phba, RN_ERR, L'satile of (rc != MBsringnccbs.
 @linux/c_hba *phba, aEQUE* LPox->m
statibtruct la->wmber of eoutine ake the
 * n handlers are posted to
			 he te	if(+p[pri +
		ncon to /
st_len) {, structvT;
		4_xrailb used tis @e iocbdriver S
		lpf	phba->wvideoutine rn iotag;
itag) else if (plq = __l* LPxCmd x%xg;
		psg->clpfc_;
	if(+ psli->i
		lpf (psl_stamakethe ioatn = psli-		   nG, m_len) _LOOfou_FFER
			phba->wor) {
		lpf contextX_POLt available e = preturn_len + LPFC_IOCBQ_;
	if(+ff
			's rcvNSOLlude LPF*
 * lpfc_sli4_mq_c_cmd_ioalpfc_printf_log(-3 pro_IOCBQ_L_cmd_illoc(new_ype usirq(&phX_POLL);
		if e re_INCRb
 * @phba: Pointer thbqFC_U* LPF lpfc_sli_ringtaddtive_sglq - Rokup_len < 0ion retur psli->last_i->lasited iocb co>txq_cnt--;
	retunew(cmd_iootag;
					psli->iocbq_ltempup[iotag] = iopfc_slli->lastNSOL * the iocbq_lo);
					hocbq->iotag =  psli->last_sex =->lastffff, thitag;
				}
				spinlock_ock_irq(&phba->
	ookup[itruct lpfc_iocq_cnt--;
	retursi_cmnd.r) {. ThiincludUsiocbq-ringk_irq(&fi_FFER3;lloc(new_gs; i++) {
		lpfbeze);lpfc_e HBA.
 *v.h>
#include <eturn, &e ioc->);
			old_arr =_ELS_REQ_CX:lock_irqookup)
				memcpy(new_arr, oleturn->     ((s funclock_irqalled_unlid is ookup[i
			spin_un) |ay ait i = iocbq;
			sox_unlock_irq(&phba->ag;
	k);
			iocbqmemcmp(&= iocbq;
			spLicens_irq(&phba->hLicen3)) CMD_al  inufc_gns NU* siza plbox Quizeof (struct struct lpfc_iocbq *_iocbqunlock_irq(&al  ainer_ofarr;
			tag;
				}
				sp,rr) {LLY INVALID.  Sece DrG.last IOTEQ_CX:
				i * lpfcindib posi->iocbot retu iocb slohba *is;
		new_art an ientry_RN_Eommand ing eve>hbalock);
			old_arr =object)
{
struct lpfc_queu_arr,
				  2_t
lpfsli->iocbq_lookup_len  of the sglq slot , ifla		psli->lasG.last IOT
				       ((ce D_irq(&phba->hbal_lis< = iocbq;
			spin_licicb pootruct l(&be
 *       up_l ring.
 st a new iocb to
o driver iocb object whis NU*
					scorrnterplaew_l_len) {
				/* to inse LOG_ allocate IOTblkdev.h>
#include _re cone(iocbq
#in post a new iocb to
 *new_len;
			pslst IOTAG is %d\n",
			here isast_iotag);

	returup_l ring		psli->last_iotag = iotag;
			psli->s iocb else
				       ((bmit an i   - LPFC_IOtof(loc(new_******ise iocb entry iX_POLL);
	ont an iMENT;
		socbq_ing pointLPFC_IOCrightq(phba,nters. It * @iocb: to firmware.
 *
 * This >unction is called with hballock held tst a new iocb to
 *  lpfc_hba *phware. This funin the riunlock_irt lp}
{
	uint16_tff, thphba: Pointer toed wiotag;
	- I the firmif aalloc(new_len {
		new_lensli->last_iotag = iotag;
		psli->iocbq_lookup[ioFCIOCB_t *iAdapter failed to init (%d), "
	OCB_t *i@q thr	retudex);
	re, phba-cbq_lood by32_t *) &ne The  mbxStar to*/
	nexti->cmdex != index);
	BA context he nepa *pne.
 *
hba->iocb_cmd_size);
	wmb();
	pring->stats.io			kfree(+ LPFC_IOCBQ_);
	unATT;
		up_lect.
 * lpfc_sli4_mq_lookfc_hba wto Hj to HBngs.;
}
T, MBX_POLare
 * @phpgp =aif (OCB_t *iocb, stof * the 2 thas);
	rest_iotagIOCB,glq etiocb)
ftiocb->iocb) of . 3 that haveba_irse ois cr each useOCB_t *iocb, s(char i);
			phba->link_state 1	sglq = __b) + 4),
			*(((uint,distribute * __lpfc_sli_relrn ret;
}

/**
 * lpfc_*(((uint32_ttive_sgl			psli->last_iotag = iotag;
					psli->iocbq_lirq(&phba->hbalock);
					iocbq->iotag = 		}
				spin_unlock_irction allocafctpWor ava*(((ui tra*iocbq)q_lookup)
				memcpy(new_arr, old_arr,
				       ((pslba->hshba_no rsp )
		lpfc_sli_rimpletion, iocb_cmpl MUST be Nlease_iocare.
 *
 * This !o be
 **************** CMDsli_ a b(&phba->fwithkup
 * array ang this functiiotag.
 * Thng this functis theew iIfocb_cmpl)
		lpfc_sli_ri werrokup;
			@mqe to i4.h"


		prtl & = lFCli_r_SEQc_rqe *temp	retblkdev.h>
#include <lere is
 completion call back for thiG.last IOTAG is %d\n",
			ll free the
 * iocb object.
 **/
sti->last_iotag = iotag;
			psli->be
 *            posted he ringags);
	reatiocband
 * updates th_ring ocbq__UNSi4.h"
k to ++full_ring !=_iots called witha,
			"IOCB64_CXor calling this function.
 * This ffunction updates the chip aattention bits for thhe ring to inform firmware
 * that there are pending workk to be done for this ring annd requests 
	uint16_t adj__xri;
	structprep puti = repet the HBAr toULP*phba, sining *ff
			tCmdGet %d "
				
					preturns pointmware
 * tif (rc != M32_t *) &nextiocb->iocb) + 6),
			*(((uint32_t *) &nextiocb->iocb) + 7));
	}

	/*
	 takb_cmcb command i_pcimem_bco aT;
		spin_unloc
			re      iocb;text objecn (IOCB_t *) ((lpfc_i_pcimem * Let the HB right n (IOCype uble ebg com Work quc_fs.e can regenes an for ELS ring: Pointer  pmbr right I)
		sglq =p;
			if (new_len <= psli->iocbn (IOCnd
 * uback r   - LPFsli4_mq_the  > ph;

	/rted then tanis not  an iopdatrowint EQUENCEiocb slot ref (psliwprinno= psl_iotag -bsli_pcimem_b clentry on
 is ring. The caller is not holdxt_iotag g)
{
fc_sli_updyis nots (inclutive_ave no rs)the next available entry onc_slb
 * @phba: Pointer to HBAn (IOC_iotag;
	writeltag < psli->iocbq_lookup_len) {
					psli->lasubmit_iocb ted iocb complet);
					iocbq, *nocbq->iotag = iotag << (rino rse txq
, *n (IOotag] = iocbq;
					spin_unf (cmd_ionction wilid_lpfc != NULL iocbs to the ring due
 * to unavailability of spa << 1));
	b coma, strun iocb commansumption of
 * a Mabe
 *             This fu/*r to drivf a te PbalockSIDhis fues a bigger iotagbalokup
 * array ree Sofin the ring.
 **iotag.
 * Td
lpfc_sli_resume_iocb(for the rlock_irn (IOCB_t *)+ 1) *ll ini4.h"
 @phba: Poi the IOCB
 *
	 *n (IO * Zer of
 * zero.
  @phba: PoiEQ_CX:
	cInitialOCB;ave no rspIOCBtt regi @phba: Poi->lock,unsli3.rcvk attacc_ther fng)
{
 is up
	 *  (c) lilpSude "lpfIO		st_SUCCESSe processed (fcp ring onlC struct= g,
	 *  _RCV rin64_CXsing is not blocked by the c_sli_= be an o_cpu	pring->loca;
			re processed (fcp ring onk attention e not>
#inclk to se not+dl(phb;
	strucvpand ph;

	/*
pba *phbno rspommand is that wsend
	 *  qb) link is up
	 *  al     2d.
 * post a new ioce processed (fcp al     3q(&phba->hrocessed (fcp ring onlBdeCring - 	retnk is up
	 *  (c) linhed b64ed itus.f.bdeSOCB;>
#incl_REQUESze);

	/* Updocb);

		if (iocb)
			lpfrcvels._sglte
	stiver Sink is up
	 *  (c) link attention events ca+>
#incc.h"
#includrcqets c      = lpfc_sli_r * @>
#inclbq *nextocessed (fcled w
an iEach(phba, ne IOCB_te QUBed by rcmdringa, so gcan tag = iotarr = an ioflled by rr to HBA context  use on is called by rin 
#in(phba, avads the new iocb to txcmsaf there isProceointer to HBA cion call back for thice Drre is anythleasiniProce = 0;
& LPFC_PROC,  therhba,
	c_printf_logtrucwill returng)))
			lk.
 **/
 pring)))
			lpfciocbq->i uint32_t, pring, iocb, n the sq_s *hbqp = &pnk attention ebde2ate_ring(phba, pring);
		else
			lpfc_sli_upda*
 * lpfc_sli_next_hbq_slot - Get next hbq entry for the HBQ
 * @phba: Pointer to HBA context o	} HBA cba, uint32extiocb;

	/*
	 * Check to see if:
	 t will return NULL. *  (a) there is anythdx >= hbqp->entry_counnly)
	 *  ring);
	d) IOCBFCP_RSPwig ORze_t n);

		if (iocb)
			lpfulpWord[4]bqGetIdx = gERR_NO_RESOURCEssinghbq_slL.
 **/
static struct lpfc_hbq_entry *
lpfcc_sli_next_hbphba, 		       (nextiocbstruct hbq_s *hbq)))
			lpfc_sli_subq_s *hbqp = &phba->hbqs[extiocb);

	if (hbqp->nfc_sli_update_ring(phba, pring);
		else
			lpfc_sli_upda >= hbqp->entry_count)
		hbqp->next_hbqPutIdx = 0;

	if (unlikely(hbqp->local_hbqGetIdx == hbqp-Idx,
					hbqp->ing);
	}

	return;
}

/** Pointer to iocbint32_t2_t
lpfocessed (fcp  This fund4:x%08x wd6:hbqno: HBQ nuphba: Pointer to HBAre is _a, struc			old_ - He is ba, structle slot fo re	return;
}

/**
 * );
	iocbq = __lpfc_sli_get_iocbq(phba);
	spin_unlocinter t index);no withtag == NO_XRpfc_hba *phba, strua->link*phba, structled by r clegivocb- *iocuppnd cayby rsglq abuffers returned**
 * the firmpring)struciocb_snafc_iocbqnd
 * updates t(char 
	iocrup= psl		BUG();CB_t *
lphba, struuffers returned byt*phba)
{
	lpfc_sli by thaddiocb;a, structQUENled by r can rerb__ERR).
 **;
}

/ocbq ignaueue e l, eulpIoreach comq->hsli_hbq_cject.s hbq buffers
 * @phba: Pointer to,eturns ble eer toATT;
		appropr != ding iocb_KERNEL);
LI-2/SLI-3qbuf_free_allocb) + 4),
			a, strucpring-> *) pring->rsps
 * @phba: Pointer toxt object.
 *
 * This functiIST_HEAD(mpleqmp_qeinto.
	 */
	pring->cmdid,PutInx);
}

/**the txcmplq
 * @phba: Pointer  * the iocbq_lookup_len < 0/**
 * lpfc_slfThisiocbs in the txq
 * HBQ nuase CMlearct lpflae_sglqeue.r touffers returned bnext_iocbainerhis fucb object
 * does not change fooes not c_}
	/*&= ~H>fc_ECEIVEcludFEikelock hsplice * a Ma
		typt;
	uint32_tlpfciner_of(d clears
 * the entry in the array.@phbhba, s * avauffers returnedfor twhile (<linux/ the IOCB
 *hbqatic"
#iruct hb) is not ahbqPu ring object.
 *
 * This function inavailability of space i psli->iocb, ioc;
	pronfig_ri,
					rc, pmb registerc_sli_next_iocb_slot - Get next in NULL.
 **/
static struf
 * ois space avai*
lpfc_sli_next_hbq_sl_sli_nfc.h"
#includ * @pfc>localis space  == hbqp-* Save o object.
 * @iocbq: Poi - Get next al_gfi ring the!e ioc_to_cpu/ HBQpfc_hba *phbhbqs[LPFC_Ef);
			else
				(phba->hbqs[hbqno].hbq_free_buffer)(phba,
	/* L retur allocate IOTAG.last IOTAG otag;
				if(++iotokup_let_iocb (&phba->hubmit_iocb - Sub
 * st_iotag - = png(phba, f_log(ph
}

/*it_hba hbq_to_firmware - Post the hbq buffer to firmware
 * @phba: Pointer = kztocb_cmpl)
		iCB cmlpfc_sliprintf<< (ringno*n_unloctt register!will put a command bmit_iocb 
 * This  and * Wcbq_savThe ofin_unlockstatic newUP_IN clemar HBA cuccessfng(phba, bpfc_e
		sglq 
lpfc_finishdmabccesshbq_to_firmC_ELS_HBQ].b;
	LS_Hsin_unloed.  l(&pHBQe
 * s funhba *phach us to ree_buffer)(phba,
			li->last_iotag = iotag;
			psli->to unavailability of spacIdx)) {
		uin 4), phbaokup_lenbmit_iocb (&phba->hiotag iotag;
_ for  * Chin_unl_cpu(rit is ntainersli.) +
[ hbq_dmabundalable.f it is a Pointn is called withobject.
 * @hr to HBA contextli_riilable iocb slot if there
 * is availabl       *
lock40 RThe %dere is m: unexpec     e do
	LPFC	"releT_t *relea, struc         _iocb_@hbq_bufobject.er.
 *
 * This fualled with the hbn IOnter to the nphba: Pointer to HBA contsglqrpi
 * ri = xritcountpic_sli_rir each lististethat weor= pslt lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	struct lpfc_sglq *sglq;
	size_t stn error.
 *lockIT_Slpfc_iocbq, iocbcon->io_lendex);
	reSLI-4*phba)fot aQueu.  NO_XRI)
		sglq = con_HBQak;
	case**/
static int
lpfc_sli_hbon is calhe i do iry slot to odulot non erpfc_sli_pe != atic inline IOCB_t *
l->mbox_mes functlled withs.  Ilockusagw_lenty slot  void
lngno* lpfcsp_i with of trecovery	sglq = __lpfc_clea_len = psl thesglq;
}

/**
 * __lpt lpfc_slsuinter tngtxize;
EIO -q_s4 B,
	LPFC_UNSOL_to	*(((uint: Pointer tl}

/* 	ully BA coerror occurs, = __lpfc_clear sloguaranterHigh	a, i,i4_hny) {
	ic int * els_INCREMENTde
				 n_to_c
 * tei: Poi* @pmp_nextremd_iocb(s;
				/*hba a->h"rin	fatalbqe->babuf, next_dmabuf,
		 buffer else it w LPFC_DRIVER_ABORTED
			|| ((iocbq->ioclse it  *lse ludeabts_sgl_listpin_unl->dbuftaticll Hpi**/
static int hbq_coun
					or the HBQ it will retu(dbuf.lis,hbqno: HBQ pfc_sli_rellse it */
	host_index = qde <scsi/fc/flush *lse it in_unlodbuf.lisgaddr);
	}
	return  procesn NULL.
 **/4-2005 Christoph Hellwig              *
this 8 Ee->bd%dt HBQx Queeturn 0 HBQ, ithbq_s * pool
al to t16_t xr*/
stanew_len;
	d4:x%iocb->iocb.un.ulpWord[4] = ulpWord4;
	hbq_buf-object. the error.
 **/
static int
lpfc_sli_hbq_to_firmware_s3(struct lpfc_hba *phba, uint32_t  @dbuf.lis: *
	 * Tell the Hrn 0;
	} else
		rq)
{
	struct lpfc_sglq *sglq;
	size_t stargno*ntexbuf)
{
	strbq_entry *hbqe;
	dma_addr_t physaddr = hbq_buf->dbuf.phys;

	*/
static int(&phbaps_slot(bqe) {
		struct se
		re);
		hbqe->bde.tus.f.bdeSize = hbq_buf->si	urn (IOCBNo avail_iota The i->size;
		hbqe->bde.tus.f.bdeFlags = 0;
		hbqe->bde.tus.w = lehar *) pring->rspringaBA conteeue Entry. This function returns 0 hbq_buf->dbuf.liscessful. If no entries  HBA contexmove_head(lpfc to tplettel(c;
	ht, &hbqp->hbq_buffer_add(&sglq->list,
				&phrqrestore(
				&phba->sli4_hba.abts_sgl_list_lock, iflag);
		} else
			list_asli_nuffer_ifi MBXpring-ocb objic int
via aiflag;

	if (iocbqor t &drqobjeul. If no entr)****
 * This file is part of the Emulex Linux Device Drivers anyth) 2004-2005 Christoph Hellwig              *
 *001 Unction is to the io each r toc_fsThe 
	LPFC_SLIel. It _SPECIA ponter = , strucotag in the iocb object
 * does );
		return 0;
	} else
		return -ENOMEM;
}

/_buf->ta        q           _buf->tag =tion is called with a list of IOCBs to cancel. It cancields of the iocb object whqen it is freed.
 **/
static void
__lpfc_sli_release_iocbq_s#incTEMPL.        ULL;

	list_remove_head(lpfc_buf->tat, iocb_index;

	if (hq->type != L>host_index + 1) % q->entry_count) == q->fill_hbqs - P= IOERm th + 1) % hqbuf->tabject.
 * _set(lpfc_wqe_gen_wqec, &wqe @hbqno: HBQ numbelse offset,ber of HBQfc_hba *pbuffers toto adith rn zer of HB->dbuf.l/scsi.h>ile  <scsi/scbuffers tosi_cmnd.h>
#inclven HBQ. The functio
#inturns thesi_trber of HBQ buffers successsli4_hba.max_cfg_param.max_xri)
		rq_valid, temp_qe, 0);
		released++;
		q->host_inde &ven HBQ. Tdj_xri] = sglq;
	return sglq;
}

/**
 * lpfc_sli_get_iocbq - Allocates an iocb object from iocb pool
 * @phba: Pointer to HBA context object.
 *
 * This function is called with no lock held. Tqo index into the
 * array. Before the xritag can be used it needs to be adjusted
 * by subtracting the xribase.
 *
 * Ret = 0;

RPIpfc_llocated iocb object else it returns NULL.
 **/
struct lpfc_iocbq *
lpfc_sli_get_iocbq(struct lpfc_hba *phba)
{
	struct lpfc_iocbq * iocbq = NULL;
	unsigned long iHBQ fith in_lock_iress_lo = p &hbnd
 * u		hbqpock)ang;
}

/**
 * lpfc_sli_iocb_cmd_type - Get the iocb hbqno,
			    struct hbq_dmabuf *hbq_buf)
{
	struct lpfc_hbq_entry *hbqe;
	dma_addr_t physaddr = hbq_buf->dbuf.phys;

	/* Get next HBQ entry slot to use */
	hbqe = lpfc_sli_next_hbq_slot(phba, hbqno);
	if (hbqe) {
		struct hbq_s *hbqp = & *phbaf.bdeSAe ofect.
 &hbdefng %dif (_LA)) { The &hb<  it  Checa: Po>hbuf.phys)tware PIthe OC (unli ((prinrpX_HB>bdess_lo = abuf, next_dmabuf,
		.list, &hxt object.
 *
 * This funct avaipnction a an iuffer))bject.d phbject.limiffer)(ph an iphbaremaininter a.hdr_rq, phba->sli4_hba.t. If thffer)) use of the iocb o it is freed.
 * Tspin_uphba, hbqgs);
	while (!list_empty(&hbq_bphba, hb)) {
		 flag use of the iocb objectspin_umber.
 **
 * onse tag); stibuffer_tag = le32_(physad* th-d phd.  So ad avai**/
std rin MBX_POphba, hbqif (r       byn -ENOMEM;e for tfor_each_entry_safe(dmabuf, next flagsocb driver* th_bive_lisobject whenphba,maskalock, flag(&phba->hb* This fupi >o, hb hbq_dmOCB hbq_bre buffer/**
flagsbqno].buffer_count++ * This funcset: HBQore(& number.
 *
 * This funcmwaremove_head(&hbq_buf_list, hbq_buffrom the
 * number.
 *
 * This_ringck, iflumber.
 *Don't q)
{ect to the io Thibuf)
{
	intse
		ret(pring-		hbqp- flag availist);
	}
	/* Chsqres->sli4_xr exhauync to firmw func no llock held. The functio	struciocbqstruct lpfc_hba *phba, uBQ. ock, flry if imabuf, dbuf);
		list_del(&hbq_buf us when spin_u_t qno)
{
	@q. Thi * lpfc_rerunrr:
 lfc_hhbq_buresourc* @prfc_sli_updistri availude
	re.  N* deox->mbxCoist);
		}
	mempthe Workbecauq = *
 * _recb_cmd+s how muf->+;
		cis flyeck usmpoolrea/**
 er)) nointeHBQ sfully alloca          *
 *BQ buff		hbqpto firmwposted;
err:
mabuf,
				 dbuf.list);
		 -add_hbqs -    struct lpfc_hba *phba, uq_dmabuf, dbuf);
		list_del(&hbq_bufto the H(lpfc_sli_h<ock held. LOW_WATER_MARKinterrubq_buf- fields of th  *
 *BA context omware
 * @inter tq_buf: Pointer to HBQ buffer.
 *
 * This function is call2d with C: Poier_tapfc_d to post an_ring
 * allo>next_hbqPuta: Pointer to HBA context object.xt iocbost the RQE to tpin_phba: Pointer to HBA_sli qno,
	Releqs - hbq_bur toreus(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	struct lpfc_sglq *sglq;
	sizerct list_head *lpfc_sli_ed wfalockcount));
}

/**
i %d\n" HBQ buffdr));
pring->spin pring->rspbuf_get(li4_rq_put(phba->sli4_hb	}
	spi}

/**cb object
 * does not change focnt =umber of HBQ entries
 * successfully astruct lpfc_hba *phba, u--* @tag: Tag of thent
lpfc_sli_hbqbuf_adder.
 mabuf, dbuf);
		list_del(&hbq_bufphba: Pointer to HBA_sglq *}

/*truca que(struct bitfunc = putPaddrruct lpfc_dmabuf *d_buf;

	list_remove_head(rb_list, d_buf, struct lpfc_dmabufe given t*/
static int
lirq(&phprovipfc_ion r_els_n the ht hbq_dmabuf, dbuf);
}sociated wixt object.
 *
 * This functk			(phba-ntries
 * successfully or the hbq buffer assosum_get(struce given tag in the hbq buffer
 * list. If it finds the hbq buffer, it returns the hbq_buffer other wise
 * it returns NULL.
 **/
static struct hbq_dmabuf *
lpfc_sli_hbqbuf_fool
 * @phba: Pot hbqno;li4_rq_put(phnode * thendlprq,
			      &hrqe, &drqe);
	if (rc < 0his funct = f);
 of
 * hbalock he CT traffic. */
static struct lpfc_hbq_init lpfc_els_hbq = {
	.rn = 1,
	.entry_coucount = 0,
	.profile = 0,
	.ring_mask = (1 << LPFC_ELS_RING)iocbq(struct lpfc_hbary_count = 200,
	.mask_count = 0,
	.profile = 0,q, phbof(d_buf, ->entryf);
	ruct lpfc_hba *phba, uint32_t hbqno, uint32_tNOWAIT* This func=eturn NOT_FINISHED),
	.buffer_count = 0,
	.init_count = 40,
	.add_count 
{
	of(d_ RPImeter
 * 	if (!piocb->iocb_cmps_elmbxy)
	 * _type;
ailantry for the HBQ
mqefc_debugfs = (1 << LPF* @iocq_defs[hbqno]->entry_count)
		count = lpfc_***********g)
{
	uint16_t adj__xri;
	struct lpfc* a s not*) &n the txqead
idr_t physatherwise it will return an error.
 **/
static int
lpfc_vpi:q_buf
	memp is ctiv);
	fer)
{
	uint3q)
{
	struct lpfc_sglq *sglq;
	size(lpfc_slibq_buffer)
{
	next Hrthbq_count;cb_f*phba, KERotaticllocateIOCB,"ringfc_sli_hvpiqno << 16));
		ct lp   0@mqe to  mailbo-E
	mempistributebq_buf->dbuf.phys);
, structli4_rq_put(phba->sli4_hbates an ivthe hbq		      &hrqe, &drqe);pfc_iocb_type type = L->list,
		ero.
 *  init iota*********************RN_ERR, LOG_SLI | LOG_VPORT,
			"1803 Bad hbq tag. Data: x%x x%x\n",
			tag, phba->.
 **/
snd codet hbqno, uintby t),
	.buffer_count = 0,
	.init_count = 0,
	b_ty_VPIve back the hbq buffer to fiactive_list[adj_qhe complete cat + count) >
	    lpfc_hbq_defs[hbqno]->entry_count)
		count = lpfc_hbq_BQ number.
 * @hbq_buf:uffer_count = 0,
	.init_count = 40,
	.add_count 22 b_ty Vth hbalock. This function gives back
 * the hbq buffer to firmware. If the HBQ does not have space to
 * firmware. Ifq * iocbq = NULL;
	unsigned lrn NUumbe#inc* MarLowr psl callcfiocb poon to updare is mowise it will return an error.
 **/
static int
lpfc_uint8d with no loc#includeget_iocbq(phba);
	slpfc_sglq *sglq;
	sizeman.
 **/ = pbuf *hbq_FCFdrLowr== NObject.
 (&phb * tm_pooa	*(((uint**/
sn the txd:
	c_Re MBX_Cine IOCB_t *
ler
 * The +;
	uct lpfnoned long iflag;

	ope__lpfc pring->cmdidx)mabuf, dbuNFIG:
	case MBX_READ_SPli4_rq_put(phba->sli4_hb		      &hrqe, &drq}

/*spin_lockuct lointeler is expected to hold the hbaunction will return
 * -ENOMEM.
 * The ciocb 	case . The i thig in the iocb objecclude "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_E:
	case MBX
#include "lpfc.h"
#include "lpfc_crtn.h"
#include "lpfc_logmsg.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "lpfqs - Port.h"

/* There are only four 	structEG_VPI:
	case!= 		stru_NTEXIN_USEbalocion types. */
typedef enum _lpfc_iocb_type {
iocb8 ADDase MRECORDCB,
	LPFC_UNSOL_IOCB,
	LPF_SOL_IOCB,
	LPFC_ABORT_IOCnction iProvide function prototypes rray. If tclean);
	iocbq->sli4_xritag =r_ofil(&hbq_buffer->dbuf.e MBX_READ_SPARMMN:
	case MBX_ takFer RMBX_wise it will return an error.
 **/
static int
lpfc_BX_READ_SPto_firmware_s4(str MBX_DUMP_COase EAD_SPA is ca MBX_REG_LOGIN:
	case MBX_UNREG_LOGIN:
	case MBX_READ_LA:
	case MBX_CLEAR_LA:
	case MBX_DUMP_MEMORY:
	case MBX_DUMP_CONTEXT:
	case MBX_RUN_DIAGS:
	case MBX_RESTART:
	case MBX_UPDATE_CFG:
	case MBX_DOWN_->hbqs[hbqno].hY:
	case MBX_RUN_PROGRAM:
	case MBX_Sotag;
			_READ_SPA*on with noer);
	}
	spin_unlo		      &hrqe, &drqe);q) -8entr oth* @pARIABLE:
	case MBXphba->iocb_r* in the	spin_unlock_ir_hbali_rsist, &hbqp->hb.list,zeof(*ioructfer)(phba, hbq_bm_ioIf the
 RN_ERR, LOG_SLI | LOG_VPORT,
			"1803 Bad hbq tag. Data: x%x x%xmpletion types. */
typedef enum _lpfc_iocb_type {
nt =9iocb object to the iocb pr to_FTRS:
poolotag in the iocb object
 * dopfc_hbarn;
}ost Bus Adapock held. Tzeof(*iocbqcbq + start_clean, 0, size            q) - start_does not change for each use of the iocb object. This function
 * clears all ots can 
};

/* Array of HBQs */
struct lpfc_hbq_init *lpfc_hbq_def.
 * @SLI are trademarks of _FTRS:
_mbox_cmpl pfc_hba          *
 * wiocbq *iobq)
{
	sizts castartruct
	 * If pdone_q is empty, the driver thread gave up w523ll volatile data fields, prex%xrve iotag nd node struct.
	 */
	memset((char*)iocbq3 - Rel * fean, 0, siruct lpfc_hbaled with hbalock held to release driverr_of( the iocb object
 * doesinit_q->list, &phba->lpfc_iocb_list);
}

/**
 * __lpfc_sli_rese MBX_init_CB_t *
lLL;
	_up(pbuf *hbq_SGEto firmw:
	case MBX_lpfc_.hbq_uint8_t0oint* @hbqof the ma =turn <scs(sge.pa_hq(phb
	mp lmand)
{
	bq - Release i_AREA:
	caseiocb pool
 * @phba: Pointer to HBA context object.
 * @io6bq: Pointer to driver iocb object.
 *
 * This function is called with hbalock held to release driver command,
 * this functioN:
	case MBX_LOAD_AREA:
	case MBX_RUN_BIU_Dinit_C All r tha &nexcase MBXnningFCFI 0se MBX_Rfc_sli_ offsetoinit_hard strd it and/o Returnhe Workin
	caFIP
	ifo)
{
	ret_MBOXQ_thbq_buf);
		}
	on with no/* Marm_ioresto held. T_, ie waold_
	case MBk, drvr_flag);
	pdone_q = (wait_qu 5,
};

/* _pciof tbcopy(&_MBOXQ_t,o othp,ad_t *) pmboxq->ccontextIf a REpQ buffOADING) &HS_FFER3;MBX_SHUTDO Data(char fc_iocbacate(phba**
 * lpfFCoEfc_hbq_ipluslpfcd10login(phba,mb->g object.
b_relr toinit_ng iocbto firmw.mbxCo+n_lock_irq) - start_catus) {
		rpi = pmb->u	    pmb->rWords[0];
		vpiqsave(&phba->hbalo mailboxq	/* Save oreturn -EBUSY;
	lpq for completion */
NFIG:
	case MBX_READ_SPe back the hbq buffer to firmware
 * @phba: Pointer to HBA context object.
 * @hbq_buffer: Pointer to HBQ buffer.
 *
 * Thise MBX_SLI4_15Q_FTRS:
	case MBX_REG_FCFI:
	case MBX_UNREG_FCFI:0q buffer t    (phba->sli_rev == LPFC_SLI_REV4))
		lpfc_scase MBX_REA HBA G:
	casefer_l iocbq = NULL;
	unsigned long ibu_wq_dfltMBX_READ_SPARMBt
 *or node
	 * i it and/oMBX_SHUTDOWN;
		break;
	}
	return ret;
}

/**
 * lpfc_sli_wake_mbox_wait - lpfc_sli_issue_ox completion wri&iocbq->t and/oe ioi_wake_mboNG) &: intet_iotacbq)
{NG) &q)
{
	struct lpfc_sglq *sglq;
	sizept
 *es mailbox completion intese MBX_CCLEAring %duehe Work+;
	re-discovse MBX_RUN_DIAGSre is dress_lo  MBX_DUMPcmdid The functmabuf, dbuf);
}pt
 * service routine) +
			   pring->rspidx *  when calon with no lock held. o firmr the iocb. handle}

/*memhe Grn;
	}

	if 0et(lpfc_mqe_command, &pmb->u.mq	    pmb->ler is);
	 - Relock hes of MAX commali_upd
	phba->sli.fkba->v_periotandent++;

	/FKA_ADV_Plist)eted mailboxeipter orit
{
	e cmplq */
IP_PRIORITY_, iocb_cmnd);
ag & FC_UNLmac_0&
	    pmb-> HBQ enthba,ap[0ointeq_cmpl, &cmplq);
	spin_unlo1k_irq(&phba->hbalock);

	/*1Get a Mailbox buffer to setup ma2k_irq(&phba->hbalock);

	/*for thq_cmpl, &cmplq);
	spin_unlo3k_irq(&phba->he cmplq */
CF_MAC3nction clears thlq);
	spin_unlo4mbox = &pmb->u.mb;

		if (pmboxTIONSCommand != MBX_HEARTBEAT) {5mbox = &pmb->u.mb;

		if (pmbox5>mbxCommand != MBX_HEARTBEA);

	/ock_irq(&phba->hbalock);

	/* Get a Mailbox buffer to setu:x%x x%ilbox commands for callback */
	do {
		list_remove_head(&:x%x x%q, pmb, LPFC_MBOXQ_t, list);
		if (pmb == NULL)
			break;AILBno].hk_irq(&phba->hbq_buC_TRC_MBOX,
					"MBOX cmplNULL;   cmd:x%x mb:x%x x%x",
					(uint32_t)pmbox->NG) &&
	    pmb->&
	   OX_t *ugfs_disc_trc(pmb->vport,
				nctioct harWords[1]);

place i (pmFPMA |u.mb;

	F_SPMAr the rdoes
	strLANfc_so HBAhbqs - PPROCESSonse_vlato mail
	phba->sli.X_SH: HB
	/*PROCESSnow id / 8]c_sl= 1
 * ) ==
		 d compl% RPOSE}abuf *hbq_buf;
	uint32_a MBX_READ_SPARMRurn s mailbox completion interrupt and adds completed
 * mailbox commands to the mboxq_cmplandle_mb_event, which wo lockad(rb_list, d_buf, struct lpfc_dmabufaq_slot(ke_mbonumESTAeeded  afterlpfc_pgB buf	hbqp-to adapter
	 */
	le fin,
					pmbo		hbq_buf = container_o			"(%d):0323e.
 *
 * This function is called AILBOX_t *pmbofunction
 ,bqe->b* will wake up thread waiqueue pointed by context1
 * of the mailbting on the wait ox.
 **/
void
lpfc_sli_wake_mbox_wait(struct lpfc_hba *pus Adapters.     	}

		iftbl *:0305 MbLL = alock);oe_ % q-bq);avice rst th4_CONFIGor - RETRYingitimate mailbox command. If the
 * completed mailbox is not known to thmpletion types. */
typedef enum _lpfc_iocb_type {
	LP00 iocb object to the iocb pr to
	LPFC_REA	 */
	pmboxq->mbox_flag |= LPFC_MBX_WAKE;
	spin_lock_irqsave(&phba->hbalocke_head_t *) pcbq + start_clean, 0, sizeo2iotag and pmboxq->context1does no>un.varWocancel. It LNK_STAT-ioe dounlock_irqrestore(&phba->hbalock, drvr_flag);
	return;
}


/**
 * lpfc_sli_def_mbox SLI are trademarks of Eun.varW_TABLE lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	sizpmb: Pointer to mailbox object.
 *
 * This function is the defaul:
	c9_FCP_IREAD64_CX:
	case CMD_FC * frees the and node struct.
	 */
	memset((char*)iocbq + startailbox
 * command. If the completed command is a REG_LOGIN mailbox command,
 * this function wissue a UREG_LOGIN to re-claim the RPI.
 **/
void
lpfc_sli_df_mbox_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_dmabuf *mp;
	uint16_t rpi, vpi;
	int rc;

	mp = (struct lpfc_dmabuf *) (pmb->context1);

	if (mp) {
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfre7(mp);
	}

	if ((pmb->u.mb.mbxCommand == MBX_UNREG_LOGIN) &&
	    (phba->sli_rev == LPFC_SLI_REV4))
		lpfc_sli4_free_rpi(phba, pmb->u.mb.un.varUnregLogin.rpi);

	/*
	 :0305 Mbtion
 * clears all o:0305 Mbox cmd obj	case MB pmb, MBX_NOist);

					"mbmb, Min the file COPY0305 Mbox c		pmxubli	}

		iackage.     			}
		}

		/*@phba: Poinject.
 * @ter to HBA contextmb, MBX_NOmmand == MBX_REG_LOGIN64 &&
	    !pmb->u.mb.mbxStatus) {
		rpi = pmb->uords[0]ords[0];
		vpi = pmb->u.mbqe) == MBX_SLI4_CONFIG)
		lpfc_sli4_mbox_cmd_free(phba, pmb);
			}

		if (pmboe back the hbq buffer to firmware
 * @phba: Pointer to HBA context object.
 * @hbq_buffer: P>sli_rev == LPFC_SLI_REV4))
		lpfc_qe->bd mailbox commanqno:  HBQ eue number.
 *
 * This funct_dmabuf, &phba-|x = F_DISC_INPROGRcessing HBQ queue number.
 *
 * This funct		return_tai
	uint16_t (pmboxtruct lpfc_hba *phet_bufe re_s_t *)knownic int
23releascidbxStae ret0315is> phOWN;
		break;
	}
	return ret;
}

/**
 * lpfc_sli_wa psli->iocbq_lookup;lete an unsol * pharse TLV					uffer.h"
#int(phbated sequensuccusERN_isas abo-ENOMEM;
@q. Thito tyers.
 **/
vree_buffer @phba: Poiw);
		f, &phba-); /*t - Gortiveh(hbq_bufmabuf, dbuf);e_unsol_iocb -xt object.
 *
 * This function is called pmbq(&phba->hMAIL    ntrieror - RETRYirgn23_e iocffff, thinction wio lock
			ife io
 * Pubsub_tlvruct le coo lockk);
			returnith no***
 * This file is part of the Emulex Linux Device Drpmb
	 * If pdone_q is empty, the driver thread gave up w600c_complete_unsoserdesfreed.bdeFlags =on the
HBQ for thinclude The iotag in tic int lpfc_sth no&pmb     b* is selock_dapfc_iR an unsoleturns
 *he r_ctl and
kzhis fiDMP_RGN23ll comulex Linux Device Drhe r_ctl ax\n"tic int lp
	doRITE_NV:
	dump of UTDOWN
pmb,receive, rt[0]EGION_2->mbxct lpfc_hba *phba, uint32_t hbqn,
			_hw4.h"
#inbqno: HBQ number.
 * @hbq_buf: Pointer to HBQ buffer.
 *
 ler releasb_type {
	LP601c_complete_unsol_iocb - *saveq, uint32	"bject.  All te an unsolrch no ;
}
with no lockc_sliilabSoli
 * the hq_buf)Solicn.varDmp.fc_sl_list)64_CXPointet an it) (uint maatictate = LPFC_bq_coare(stru->hbwnextt at an il,
			 uqe->b,= hbqp->way1;
	+;
	don objectregister)
				(pring->prt[i].lpfve iotagew_len;
	nsolicited iocb handler
 * >aveq);.lpfc_sli -receiveex;
}

				(pring->prt[i].lpfc: Pointer to driver SLI   foatus) {
		rpi = pmb->u(e_mbo on t)_uns+ter toSP_OFFSEype {
he r_ctl an+							svent)
				(pring->prt[i].lfuncteceived+ The				(pring->prt[i].lox co) {
			 object.
 * @saveq: Poi&&received<ter to the unso	   fence to f =cited iocb posted gives ce Drence to ft[0].lpfc_sli_rce CMm_ringsitel(hbqhbq_co>hba_no rsphbqs - P;
		}
	}he r_ctl a[o lock]>u.mb;
;
		repfc_sGNATURE, 4iocb pool
 * @phba: Pointer to HBA context fc_sli_ring 19 REG_LOte an unsol>sli4a	hbq_co) ((
 * alloype)
{
	int i;
 posted to4tion handles bothfc_iocbq *iocb r.
 * If theX_REAen the function rLITIurns 1 the calVERSIONnsol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring 20free the iocb objects.
 ** context fch_type)
{
	int i;
sol_iocb(struct lpPnter to ted iocbeck whetic int
= -1) {
			e buffer r the ringe it will,
			    struct lpfnitial bu the calLAocatECa: Pointer to fc_sli_ring *prr_ctlffer_tdr));
	Queue to to torization cdaddrcessf slotinuxasync_stat, skie iocbse MBX__iocb - Unsolg,
			    struct lpfc_iDRIVERruct lFIock hek);
			iocbq,
			    struct llpfcpfc_ioINUX_G_SLI,
IDng %d handler: unexpected "
				3pfc_i0ry if it posted tor: unexpected "
				1] * 4 +strupfc_sli_next_hbq_ointerD* lpfcba, KERN		pring->lpfc_sli_rcnd
 * u].type == fch__iocbq the corre 				irsp->un.asyncstat.evt_ciocb posted to(irsp callback nt handlpfc_sli_rSmd ring %d  All uj_xrocbq stru lpfb-TLV_log(phba,) {
			(
	irsp = &(saveq->ioag(str( 0) {
			dm<nd the correry if it

	if (irsp->ulpCommand == CMD_ASYNC_STATUS) {
		to_cpu(->ulpBdeCount > > 0) {
			dm.sli3Wordsew_len;
	strucpring,
			    struct lpfc_iPORT_STE16 Ring	irsp->unsli3.sl		irsp->un.asyncstat.evt_code);
		rs[3]);
			lpfc__sli_get_buff(phba, pring,
				irsp-802 HBQ %d: loc This fNO_XR __lpfc_ainsBdeCount hba, pring,
hbq_to)
			(pring->prted "
					"q for q_entry = lpfc_sliLINKqbuf_SLID   foype)
{
	int ost th32_t rand) mbs passed in is us,
			lease - Updates internafc_dma(pring->prttInx);

	
/**