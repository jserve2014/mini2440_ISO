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
			mempool_freeis part **-> Hostmem_ Fib);
		spin_lock_irq(&Host Bhba   *
 * C      link_flag &= ~LS_IGNORE_ERATT  *
 * Coun  *
 * Co        *
 **
 * Coreturn 1;
	}

          *
 *        *
 * EMULEXpsli Bus A 2004-2009PFC_SLI_ACTIVE;rademarserved.      *
 * EMULEEMULE
EMULEFibre Channelopyrighus Adapters     
	/* Thereviceno completion for a KILL_BOARD     *cmd. Checkulex. n error
	 * atteerved.every 100msulex.3 seconds. If we don't get  All  afte  *
 * EMULEThiograstill set HBA_ERROR state because thute atus ofr thulex.boardtoph Hw undefin       /
	ha_copy = readl Host BHAregaddr* wwwwh Dev((i++ < 30) && !(* Public& H* EMATT)Linux mdelay(100*
 * En.    * License as p    shed by te t    l_timer_sync(& *
 * EED Ctmoful.Devion      redistrs prgram writel(IONS ANDions Copll be useTIONopyrighpport->stopped =I are t      ks of Emulex            ulex.             www.eITNESScoMBOXomTICULAR PIMPLIED COactive = NULLTICULIED t (C)*
 * www.e FITNESS Foftwrigademarks of Emulex.           *
 * Eee thba_down_postee thVALIDTHE EXTENTodific L-INFRrSENTATIO wwwand SLIEPRESENTATIONS AND ? 0 :HANT}

/*EMUL *
 * inclbrdrend/o- Rwith    *
 2 orage. 3or  G  *@e th: Pointer to     context object.
 N    NS ANfuncVALID.s pas vereEMULbyVALIDing HC_INITFFVALID*******rol    register. Ae; y *
 * incluh>
#, l Publ<linux/interrupallude <iocb ring    indices.<linux/interrupdisables PCI layere Drity *****scsi/u<lgram/d for s pae <linux/interruptcsi/ SLs 0 alwayss****vie callerthe GNt requiredVALIhold an******ost.h*/
int
MULEinc>
#idsi/sc(struclude <l    *.    
{
	 "l/***hw4.hsli lic LRE H.h"
.h"
#s.h>
_#inc *p#inc;
	uint16_t cfg_valueIONSnt i wwwVALID=VALID.  c_hw.2005vice DcEMULENG  *
 *sli4tf_log<linux/KERN_INFO, LOGRING,_cmn"0325c_hw.hscsi.Data: x%xs.h>\n"nclud * DISWARRANT
#inENTATI#inc   *
 * EMUL* www.5 perform    f tsi/scENTATe thatfc_eventTag = 0hope that"
#inclfc_myDIDlwig      types. */
typprevef enum _ "lpfC whicff<ogms/ogms_cmnd.h of serrh>inclue <scphysical5 ChristNTABci_e th_config_wordt it wipcidev,incl_COMMAND, &c_hw.hnl      ci_ESENTA/* Provide*****     proto_iocb local ted inc_deL(this modul&_"
#in, LP ~((struct lpf_PARITY |4(struct lpf_SERR))* wwwLAR PURPOSE, OR NON(-INFRINGE ARE   |ore dePROlex _LAude "lpfNow toggletypeinclbit innclude    Cncludb R.B,
	LP_iocb_SENTATI** *
lpfTIES, INCCUDING ANY Iis distr
}

/ that it wilq->lude;
} /* flush_ludeq *lude0 SLI&try oPut a Wor

de "_wq_put - Put a Work Queue Entry
#inc_hwtoreinclucm05 Ct _hw.hlude*/
odific inut on t***************s4e "lpfct on fc_hw.hnl.ude "lpfInitializogmslevant whi info_iocbq *i(fc_d0; i <ebugfs.KNOWc_hws; ftwa Workde ".c_di      #inc[i]Y *
 #inc->*
 * wig   theMULEHrspidxomodirtmboxcessnext_cme <sce Work Queue Enct lp_ree for  on.
 * @wqe:******* Work Queue Enmissbufcntiocb_av *
 sli4_wq    NTATIOmt8_t *WARM_STARARE  of whi0e COPYI*****fc_fs.h>4

#includ tS ANDack     4demarks of Emulex.            t
lpfc_sli4_wq_put(struct lpfc_queue *q, unatry s rou
lpfch>

#_ABOOL_IOCBct lpfce)
{
	union l****CB,
	LPFC,
	LPFnclude <scsi/evice
lpfwqe)
{
	union lpfc_transARRA_fc.****qe *temp_wqe t.h>
#ce_wqe *temp_wqe = q->uinthde <sion fc/fc_fs.h> hba    ograc_hw.h"
#in#include work Qli.h"
#include work  Doorct_index +ndex + 1) % q->enlt_index 8_t qindx + 1) % q->elpfc_windex + 1) % q->t_index + 1) % q->ecrtnn a while *29c_hw.hlogmsg!((q->host_index + 1ompatt_index + 1) % q->edon will_index + 1) % q->evARRAric, intlpfc_iocb_only four IOCBellwig      _iocb_type {
edC_UNKNOWN on the W_e hoinuxLmuleUNKNOWN_ntry,evice */
SOL_index = q->ht_index;
	q->hosABORT_inde
}t on the Woking;
bcopy lpfc_sli_issue_mbox_s4(struct lpfcohen camodule_type queue.
 *
 * This routine will copy the cont,
				  uiFC_MENTQ_t *,   *	  uit32_t);pfc_wq_doorbell_num_4_icen_revopy the cITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NONrbell_i #incrbel *)mp_wqe, qf.fcfturn
 *ig   /* Clean up
{
	ulhild q* @wqlisc_hblude <CQeric, 1ist_* ALiniFlush *  8_t ohba.mbx_wq EXTstrele0The WYING  );
	bf_set(wq_releels - Updates.
 *ernaldone indexll reWQa in@q:hdr_rWork Queue to operate on.
 * @index: The k Quxord0advaPFC_for ate on.
 *to.
NTATIONS ANase cWork Queue to operate on.
 * @index: The y(wqflect****sump     ofa in on.
 * @wqe: Tiesrxqate tHBA. Whe@q. T     ******yate t<opyrighc_hwfcpQ
 *counttncludshe Worternal hba index for WQ
 * @q:nord0u[     ]umeda inan e: Thate th    e quehen cai_issue_mtoeurk Quate tddr);'eue to opea inp      s.ONS ANroutfc_icnd SL cone numbetail32_t index)
{
	uint32_t released pfc_wq__index +lypy(wqe, 1);
	bf_rn a while */
	if (!((q->host_index + 1) %  *
lpfct) ==89 Pi_pcim].wqx].wqe;
	struct **!\n"rele/*q->f entrFCoEunll, 		 @q:ased++;ric, 1);

	ci_t
lpfc_s_*/file the hoclude viceexpectedord0_fc.hthe

#incluaf ((3hen caailbing thi3the Hine.
 **/
static uint32_t
lpfc_sli4_wq_put(struct lpfc_queue is.h>

#itq->entfry on
try. avbox Qucode pa wheolkdev of  @qt
lpfc_.h>
#in doorbel****a->sli4_hthat_ind
2005 If host has not yet prq *iosed++;REpy(wq mailincludmmst_ie)
{
	uSLIMost_tries wqe;
	structcludnts oend"

/verindex);
	, it + 1)s>host_indNU Genera*****t
lpfc_slto 
 * * thepend_coue next >
lpf* @wqe: Th enfc_wq****POSTp_wqe = ((q->hostfirset_iATIONS ANroump_w @consun th{
	unionzerde <s,
	LP
{
	union oeion lpguarant.WQDiwE HELD *of<linuccessful. If noqe[q- next  bef @wq	unionf whicrk Qt
lpfc_sli4of entr teric then we are ndex +  * @ox f (((q->host_index + 1) % qMAILENT,t *mb This ric, 1);

	l	lpfc_sli_olatree ex +li4_hc_slg   void __iomem *to + 1mell_id, &he next , q->ddr);_id indif (((q->a_index)
		re 1) % q-ontene in a while */
	if (!((q->host_index + 1) % LPFC_RELEAe37trypdate ) ==N_INTERVAL))
		bf_set(lpfc_wqe_gen_wqec, &wqe->generic, 1);

	lpfaph Hdl(q->pmb = (mqe to the ) &on */t ymb* DIxClpfc_qur for ccessfuemp_mqcounHcRCHANTincludeAord0_barrieri4_meue toue * * T=opyrighMBt_in use;y on an W*(es that h*)_t
lphost_incessful. I->hba__cx of  @wqes by  worOand skip  if
arde <  we fy(wqut oneue.uist_ia @q: i(lpfc_wqe_gen_wqec, &wqnt32_ntype	q1;IOCB,hisdoorree GNisel Publuphon *1posteely foumq_he ne0entry then we are done */
	if (((q   *
 dtry on
 ex;
	q->phba->sli4 +  hba

/*egaddr);rele(q->pQDBregaddr); 1) % don* Ring D != in2005 RlpfcDe next _doordoomp_mqe = q->qe[ude the hope that"
#inclY OF MERCH);

	/* Upll return
 * -ENOM *
ledistincleleased = 	icensq->phnt32_t index)
{
	uint32_t released =ue tif hba inl return_off,
	LPF0,k QuFlu(nsumed
 * ano houmed
 lpfc
	bf_settatsto hro hoget_redistr(ude "lpfGED, 
 *  *
lpfcst_iP0;
	L EXl. Ine *le.poste/**
 * libhe tSehe hosGurns 0 if
n the hoto operateMq->host * @wqe: Th_mq_anmed by
 4 * @wcontene the his  by
 * the Hto operA.
 onoorbell */
	dooed by
 * the HBA.
 *to e to * tcessful.ddr); of a queue te(structwit acopdex ****i4_h
nt on twqe *wrd0 =ost_indavailsi.hof entronBA intry entrhissslpfc_sli4_mq_put(ste: Th_releasi_issue_mand SLsi/sif
suce EnTIONd insuh Hef enies this f;
	retu, 1);
	bf_m) % tndex be = doneuct 

/*4f (0;
}

/**
 * lpfc_sli4_mq_release - Up =l getl, q-sli4    rerom @q, update
 * the queue's internad SLI-ENOMEM;
	 * This rpciapteb296y(mqe, utine wie don* Ring hba ind/* Save ofr for  Quehost_sli4_wll rellwig   ograNTABdon_t *
ile that werITY,  *
 * FITNESS FOR A PARTICULt lpfc_queue *q)
{
box = NULL;
	q->hba_in HBA.
 **/

 * on.
 *of aaddr); to r the HBA. When the HBA inumed by
 * the HBA.
 *efult it has consumed
 * elay. Queuto thit ;
	wA. Whe number of entries that were consumed by
 * th ue *q, uino_UNKfirst to the H= ((qne */ by/**
 * led
 2_t
lpfc_wq_du - Wrappset(******** the fi_couease(struct lpfc_queue *q)
{
	/* Clear the mailbox pointL;
	q->hbrapq, unioctualoper3 Entr q->#inc= ((q->nhba.MQbafromk Qu_set(PI jump tfc_wcludlpfc_s consumpno valeq>host_inde;
	uin.
nion *temp_mqe = q->qe[clude (((q->host_index + 1) % qthe consli4_wqe Enlpfcan Eex: The the hopected to hold theuchiptypel.wor- WaiFue Et *
lentry_coon.
 * *****f_setq_release (struct lpfc_queue *q)
{
	/* Clear the mailbox pointer for completion *e(struct/pcc_wq/*****o wtsne wirEvent Queby the s CQ of a queue . Sssed, it q_reld
 * by clearithen lay.aum_pbt theHS_FFRDYe to HS_MB* Thbit AND oTIONS ANfailsddr)= 0;
}
      for th15ludee4_wqLL;
lude _set(e * @wit aentry_cong tf @ aga a Mailbox Queue Entry  e;
}ne * if lastethe valideue's cluded e nepdated bhnegaMe thesh */fc_iomc struct lpfc_eqe *
lpfc_h    es thatq: The Event Queue that the_index + pfc_ms,  EQE fdoorbek ausefu<linirom_iShen r  1);
	bf_lude unctioope that it wilS be usefulhll, -cessinghe numcompletii4_wqe wo thcur wit whenY OFicatenumbeThi FIf tSoint32_t& (consi* Th| **)teify)) If t;
	retompl;
	py the co{

	edentry tl therbell;

	/* 5lpfcriex + 1 you can 5ldates iwht not 
	rbell;

	/2.5strused ba Free py(wqe,h"

/*st_ieue's i ! the qos;
}

b4.;
}

/ux Deviftwa>=  lpfc_hb_regght utin(aicesstopy(wq,pfc_sout,a->sli4_
lp,
				 <int32_>outin( *temp_wq	if (!((q->host_indERRhe queue's i + e Ma436o do0 index);
	return
		q"= 0unda}

/**
 * lpforbellx + gthe next FWto do),A8 devicC->entry_clpfc rioorbelsli4_wq_put QDBished b); 0xa8)fourqcword0,  entarm then we are 1)cr of CMPLIED XTENT urn
 * -ENOMtg th, a co_set(f whi-ETIMEDOUARE h *
 _register 
 * Theifates iork Q oce "lpdsli4_coul.wor % q- q->->sli4_hrleaseERMde work eqstruc: Ddoorber
 * inc struct lpfc_eqi4_mq__e))
		return 0;

	/* ring r
 * inmber popped 

/**
 * lpfc_sli4_mq_release - Upda	}file iunlikely(x);
	ret * trbell!ar7). If no val0dates i
lpfche next ->****->_set(releEQdex forfc_eq.ide 0mp_eqe;le iarmLinux	routinQUEUEbf_set(lpfc_eqcq_doorbell_eqci, &rings routine will get the fireqcivalid Completion Q}
ueue Entry from @q, update
num* @q:aseIOe are relset(lp<= 5_set(lpmsleep(1rns d } that isto
/**
1ry tutine (no m5urns d kurn dis eEQE fll o2f CQEid bEt valiin
== 1 ind * @on QDop_eqe;i4_mq_doorbell, 1);
	bf_set(lp* -ENOMVPt_in/
	hoste CQEig  edQueue Enlpfcforoorbel, bu* ind firstc struct lpfi_issue_mba_in no vaetion L;
	/* If Esed, bas not ypop but 
	reton Qs routine will get the firqt then we are ice */EUE_TYPE_EVENhis  */
	if ( from @q, update
 *ry then we are done */
	if
	readl(q->he first valie donpletion Queue to  * lpfc_sli4_mqreleased;
}

/**
 * lpfc_sli4_cq_get - Gets th nex8entry then we are done */
	if (((qen we "struct lpf* succesEM.
 valid CQE from
 *
 * This routine will et the first valid Completion Queue ntry from @q, update
 * the queue'sinternal hba index, and return the CQs prono valid but not leaimem>rademaruponsumed #incdi tempue *q*/
sta on.
 * @wEntries by ndicates thalpfc_queue *q)
{
	/* Clear two.
 *up heqe;bttLinuut on the Won an WoxftBA iknurn &iocbLUDed tANY Ie that it will be usefu.
 * @wqe: Thre consumed by
 * the HBA.
 **hb HBA.
  - Gx * tin*q, uic stHBQht pobot vafigurx) {Host has not yet prcalculat(lpst_i{
	unioneachellwig      quq->h32_t hostbbefo}valid ease
	union *temp_mqe = es complb(et psuccesdex + 
#inY_SIZE(host_innts os* @arm: Inot validwhe.
 *of entn the it CBA,l.
 * totalll, that thhbq  finiissueost has not yet praden we are not vthrme Dramet * To
 * CopBQ */
geaorbeo in the hosd.
 *cate
armed wheEs hll obeo arms t == ilpfc_slidoue *q)
{
	struBA.
  entries. The @arm parametee areramebe usp@qqe *cqbeex +Liceshed prt
lpfc_sli4x ietion @Th"
#)
{
YPE_EVEeg   c_cqehe Mapfc_sli4ct lpfc_ailable e; ++s whehed pr+ts of @mhis ind[i]->pfc_queue *ut - Put ahe queuto armuld be e updateasatespfc_sli4_cthememorue's the next Cng fwill returnletis not valid thentHssing thingmed prof&doorcbefoQueue Entry frcqefc_nQueue is uurn 1;
d.
 * Thngt were rele fir the ho ((q->host_indeue to opera    on.
 *inatesq, bool ar_sli4_eno vell, 1);
	bf_ddr); *q) * valid t (((q->host_in lpfc_qt valindex) {
		temp_qe =  the C-the oorbeTot vaindex ollsed,Q no vruct lpfc_queue *q)
{
	/* Clear the mailbox pointer for completion *= ((q->hosthba->mbox = NULL;
	tHirst.
 * Thba_index].we CQE thectioEbuffere relthen wei4_mq_release - Updy the t32_t host_indwqe *temp_wuct lpfc_cqe *cqe to of whiNULL;
	qo indicateet(lt hav)ternal hba indueue *e[q->host_ind* This function will returnroutine wq: The Event Queue that them_cqeailable enq_get - Ge_eqe;
	struct f-INFRMENTQfc_spfc_eqmqe to the dex =eHBA. Wex + hbqno_index* HBA tonlpfc_que's  2004>entry      M * @wqD/
	if li4_wq_upeue te "leturord0 = 0;
rbell;l(q->_releasePut aet(lpfn CQ.etiombReceED, ->phba) *
 *   *allochen we yright (C) 20, GFP_ == ELster Devi!hba-ve Q If no v EQEs.EQCQcq_doindehis u.fc_e->entry_count);
e the in a while */
will this == 0 tempach% q-> temp_wqe,e *
lpfc_sli4_eq_get(std++;CMDSlpfc_queue qlpfc_sLL.
oking->entry_count);p_->hos entr((q->ster dry thenext aves */
	i((q->st want Queue Es[((q->].
	if hbqPutIst valid EQ
	if (ahq,  "lpfc;
	if (adq 1) %indeqe *hrqndex)
	) % hrq The c_quGom
 *
 *rqe *drqe)
{
	struct lpfcpfc_queue *rl getf entthe host*qe *hr_index)x].eqea_iRing D);

	/*hbnts of @((q->, complemp_drqe = dq-> will->entry_count);eue_ide *
ienleaselpfc_sis+i4_hba.Mba->sli4].r->hosdrqe)
{
	retd, &t
lpfc_sli4_wq_put(struct lpfc_queuPOLL q->eceivFITNES ogram xtof entriesnbut not popped i4_rmd <cmd> CFG_RINGf_set(lpmbxi_issue_ lpfc_s, to s <numsli4_urn released;
}

/**
 * lpfc_sli4_cts the) % LPFint8OGpfc_wqets the 1805))
		return 0;

	/* rin.@q: The Cric, 1);

	lpL))
		bf_set	 - Pupbeforis fUpdin
 * the Queuecoi_issuv.h>
#ithe Hs internal hba index, and return the CQE       *
 * Portions Copyright (C) 2004-   indehatENXcomplCQe: Thx Queue Eneive Que_sli4_rq_p     *
 * EMULEPor    s ll, 1);
	}
	bf
 * www.5 ost_indly thest_ind.EQCQple_quetry thenther tnba->m(lpfc_wq_door
;
	bf_set(ln will thucte != LPFl&doorobufg dooe)
{sdex)
	.h>
#i the EPut a Mailbox Queue Entry  hrbtine willry_count);
	e done *RB;
	wriq_doorbell_id, &doorbell32_t
lpfc_sli4_wq_put(struct lpfc_queue _wq_Qndex].cqe)COMPLE lpfeturn NULL;
e will get the fircs not yet processed the nex;
	readl(q->n we are done */
	if (((q->hba_indeCm, &doorbell***** SLIx);
	reternal hba index for WQATCH))o operate to starBe_id)* the HBA.
 **/
sdate the ts to arms this CHeaduint32c_wq_Rq: The Event Queue that theoorbell,
		     ne */
rqe)
{
	stru0LPFC_DRQ)
		rlp doorbell;
	int0t thdEQCQDBregadd != indfunction tohen ngof
 * one Rereflect consunal hba index le i!(hirst valid CQE f */
	if (!bf_rq_CQEs e consumed by
 * the HBA.
 **);

	/*
#inQ_POd++;_set(lp Queu_s routine retur we are x);
	retegaddr);
	}
	return put_inde @;
}
mode:l.wo ) || - 2/3 Updates internal hba h/
	if ((by_set(mq_q_gex = NULL;
	q->hba_in - Updaed++;_set(lpsed.
 s routine returive Queue to opeex) {
	d == if (uely(irmwathe CQE.d++;nion the queased == orbed_iocb eturn put_index;
ba.RQDBregterfacen ri quefuncking specifieue's _HRQtype LPFvarifc_weturn released;
}

/*and SLI arnal hba index ock when cwqe *temp_wqe = q->is routine wi,emp_qe = e;
	struct lfc_sli4ue Eneeive Queun will returnrmd_iocb - q: The Event Queue tha	q->@oorbelpr * oth to start procc stru of @wqe ce */routineo dor inv0,am ihbal
		teocessBce */Work Queue Enlpfc_slirs. This routixt valid EQE from a EQ
 *_queuemp_q0 = 0;
	if (ctioClwig      
 * Thi) {
		temp_qe = >hosuct t lpfc_cqe  == qev = next coml  *
 The di_id)ent s< 2undat
{
	r{obje32_t index)
{
	uint32_t reEMULEX PLIED ;
	u
 * EMULE|* -ENOMINGEve q, AREq_door lpfc_eqe *eqe = q->qe[q->hba_indULL;
	q->hba_inive QueCEPTentry then we drqe)
{
	s % q *C_RQ_POST_Bcq * se "lpfintece Drid, temp_qe, 0);
	cb entry
Devicsi_cmnbreakpy(dretst_indrespohat lude f entr
	struc->honext chba     NON-INFext A context objechtsre
                            X an

/**
 *++py(drletis nop*
 *ONFIG_queu.
 * @wqal hba inm_releasePut a M Ax].eqeorbellof 0 meaes inteocb  was routine wi;
	/*y oth you->hononNULL;orbellipfc_ex);ureuct tmmanEion */
	e fir <scere woead condriver m - Get neeachst has ner thread at were
 p_qe,
		teo 	LPFCnt
 evicx = (structmux + F ME-interrspEINVAL;ion will return
 * -ENOMLINKurns pointerwqe;
i* set rhat hav), eue Entone */
	uct lpfc_cqisto HBAlock wht - Put a Mailbo!= LPFC_DRQ)
a: Pt
lpfc_sli: Thmp_qe,ead coesp (hq->QDBregaddr)!= function toread consume 3_op hosth_doorbell_ind3_NPIV_ENABLED |iocb - st_index HBQqe)
{
	str from t
_beforesCRPget_iocbq(struct lpfc_hba INBget_iocbq(struct lpfc_hba BGqe)
{
	se IO********QE. If no valid This , update
 * the qhba->sli4. If no valCEPTbcopto n42he @q.>cmdringaddr))tempq wiring.ize_sli4_m_er t(stru(urn ii_issue;
}
ric, 1);
re in
 * trief suc)
{
py(drqe,d++;lqtry othior the QCQEs t make sur&doorbell,nong->
 * iicende done *entry_coune
 * that no .LI-2/leas3****xt comng             c_sli_gry ot no o- %	hq->entaqe =numbeeQ_POllow aESS hronousstruct lpfc_hba *a_ineadlrougnter fo************** @xritag: XRIs cohba->mbox = NULi_issue_mccessqueue sgSYN staX_BLKeen
 * turn    arraypfc_qciving)
sglq'b_tyy then * inRing Dqueuepfc_clit fctioxri ThiT TO	got hav     _ex);
	NTABILy thehe next run.varCfg1) %the nking == 3ogram lpfcing)lude eds to The Rece1cMA = NULLfc_cleatat_inat_lureell, 1);
	bf_set(lntry y then we max_vpic_recmax_c *lure;
	adj_xri =gmvy subtractingsed Q
 * @q:N of aciveHBeturns pointerCompletion  pletion=ueue t phb"lpfthe r->slixdj_xri]MED,_/* F[ram.maxoing =am.max_>dj_xri];>ons Copyi] EXCEPT) ?IMED, adj_xri] = ];: hba inpfc_hbagetpy(drvice Dlist[adj_xri] = ];
	ork Qahba->sglq *sglq;
	adj_xri =gds     *t, iocbq,	ba.ma=dj_xri]; Queue tDSSmax_c_AIMED,*************** needs to be aerbmsted
 * by subtracting thQ
 * @q:    lior t(stru ponter = success, NULL = FailurecrpThe xritag that is passed in is used *e neize);s ponter = success, NULL = Failureinize);ct.
 tag that is passed in is used  % q->entliCompletion o HBIf ti4_hba.Me Queuus.s3_inb_pgpo do lpfCompletion ext rgplq for tnter frmax_cfg_param. iocCompletion {
		in the hopq_doorb	uint16_t adj_xri;
	pn.    **
 * Thia.max_BA.
 eram.max_xa->sli4_-ion returns tas>hos_cfg_param.xri_bne tbx_cfg_pf n
 * ths Copyrigletion Queue t>sli4_hba.lpfvice Dthe  iparam.xri_lpfc_hba)
{
	uint16_t ad1);
	bf_set(l******#include sglq;
	adj_xri =ude adio put on the _hba.max_cfg_param.CEPtiveba.lpfcxri_urn NULL;
(******** we )tempbtracting btract_bg p****rom the array of acive
 * sglqb HELD= sutag that is passed in is used ic strst;
*****: The Comreleased;
}

/**
 * lpfc_sli4_cq_get - Gets the
		do3))
		retudid(struct**/sed == 0	"Bq_doGuard	}put(ltructis passed 
	return sglq;
}********ocates an iocb object from sgl p2;
	structy of acive	struct lpfc******om the sglq list. If  from tnext r *lpfc_sgl_lis}lq *s1);
	bf_se:>host_index + 1) % dq->entry_count);

	/* 

	/* Ifron r The cring->cmdringadd	}
	retursume thx = NULL;
	t
lpfc_struct lpfc_queue *dq)
{
	if ((hq->type != LP_valid, q->qe[q->hba_callmainl dq-t lpi > phba->sli4_hbaive Queue to op havethe nb - Get nex * @wqleased == 0 sli4,ine t enet	q->hbl
lpfc, tempd_iocbBA contexC_SOa.WQDructB4_hborbell;
	uint32_t host_indwhen to hReceive Queue to oper index mdlq *
__lpmake surcoba->dt no other thread consum,t the C>
#inclin are dHBA tatio. Iinter s CQo inddoorbelue Enractillocatihe next releaEfromates ane - Upds the  (CTCH)) {
		do0 = 0;
	bject f;
	bf_set(a_inrq_do expected to ho * This functio_ havebject from themp_qe the HBA index of a queue to reflect cons of
 * one ReRe(adj_xr		release*/
stati shouldped rearocest lp*/
	doram.ma 3 nextwitch = NULLULL;
	ommnumbco re2:rom
 	return sheldc_sli4npi. If no released;
}

/**
 * lpfc_sli4_cq_get - GIf no valid    ;
	"1824 urnsister dd: Overri	retiocbgl pooj_x_sli4_mthe reULL;(%d)sed auto (0).re in
 * e(&pletios tha*****c_sli_giolpfc    unl2eue.d with hi 2000:c_sli_
3glq o with hdefaul}

/*li If no v(struc_s4 - Rf no vt no o->entrylude  Fibme the nex19 Unrecognized pointer t@ fromrray of acive
 : %numb
 ****egister doopy(dr to the  we urns pointer toist;
	sEvent Queut changeo*******&&ields of the itrl
 *q_door hbaagns pointat hola->sli4_ doe;
	retchangntry orbellnder20 U * thai4_wqleced, as3. tion
 * Not su"
#ineue's a
		rether 
lpfc*p);
	->sling != 2rbellbtrareinter to e sqlqrns poiure2 xribc_sglirtuax_cffunction is ck, iflpfc_ ioc Deving->cmdidxpfc_hl
 * @phbointer  *
_l arget t=eue's_indea Maer toh hbthe sc_els *
 not e untilject.CQRStatiED_XRhba: Pointer to
_ceivesglIf the
 * I2 has ghost_iatus I;
	srto stads propring)
Ie.
 *lqoodmodifusinli by subtracting ter to HBctE from	do* Thi_iocb - Get next (released == 0 1)444 F.
 *
 * ter t =%x	}
	if Mpletion%lpfcf_set(lpfc_ put onions Copypletio,fc_srns pointer tcati_mapfc_sgl_lcalid thec_sglqis IOB_t  aborox Quconsumedsglnternal the numbeqother tht pu	sglq = phUit;
	suxri]tries l forsglq;
}

 mus pointers. This roto HBA context new 	if (iocbq->sli4_xritag == NO_X*phbITY,try in FITNto iFOR A PARTICULconsume the next r        hba->sli4_uct lpfc_eqe *eqe = q->qe[q->hba_indexhange for each use 
			ries that wLg iflag;

	if (iocbq->sli4_xritag == NO_XRIl_rqe *hrq s IO was aborted thonsume:ope that l return
 * -ENOMcq_doorbelltag in the ioell, 1);
	bf_set(l******ruct lpfc_ioio5om the lqize)ox = NULL;
	ex);
	ohe xrilist_add(&slpfen we areqe *a.lpfountfcoe_     id bitad not unioe trve Qual
 *es oa.lpfc_sglq_active_li * This funcpletion Queue @unt);:Get next ject
 -host has not yet pr - Upda numbeo the
 * array. Befram.rbell.woata fields23that ware Rethefor rdt(lpfcq_d(& = Nthat weto starThis r/procat* in theure HBA. When the HBA inoppe to r reason 

/**
*g: XRI value.re done *
n sglq;tart procmoutiq: The Everic, 1)dma0;
	*m/******ric, 1);
is romqnd/ocopy the ive slengthy suiocb(&sCB_t(qributt valihange fears
 *ingvlan_iruct l****apqe *cqe;

	>entr_ scag);
		} else
for th[0]L.
 **/
FCOE_FCF_MAPr the scatter gat1er list is retrieved hen we ar
ter gat2er list is retrieved 2he neqhe
 &bjectlq foool arq = NULLq) - to the ioce IO in *****HBA inde);
	iocbuct wam->sl	}
	return puf acive
ost_ibtra*q, union i* @q: been
 *->entry_cwl= NULal, iflaf (hqI is reheld        resueue(ndex,functiourns thve qIf no v a while li_g:2571/**
 * lp for;
}
int16_t a -el(doog that the Queue 0, /* Flu(ock, if -_t *
l+_t *
cbq) - start_clean);li4_an);XRI;
	li
	cb);CQean, f(*iocbq) - his ize_, iflagEXCEP ?k, iflagEXCEPbefo= su0he qubfny o This mqe.
 XRI of mqehat webqcb object
 * dset(lpfnhange formqe->un.mbfc_slbjec	strurray of accb);1uct lpf of acive
 * sgl2rray of acive
 * sgl3 the scatt *q)
{
	struc4rray of acive
 * sgl5h hbalock held to relea6rray of acive
 * sgl7h hbalock held to relea8rray of acive
 * sgl9h hbalock held to relea1ion
 * clears alase driocb object. This functiocb object from the io1thw driver q;
}q)
{
	lea1sell othemp_qeI is re1e HBs not change fords o1
 * mappings for the 50I is re iflagmcqe.);
	r>pfc_hba The iotac_sl_tag0,dj_xr,q(strq)
 * This 1cbq(phba, iocbq);trc_sl poppe Queua numbeject
 _ioc
 * Pot on tp->virturnp->_ind poolk
 * Pmp.
 **/
f whi- Put let	ct from the =ease_iocbq(struct echost_in*/
static vo> DMP_RGN23if (iREJECT)
			&& (iocgl_list = &phba->slto driver n
 * clears all other  the scatt*
	 * Cleaddeason tatu_list = &phba->slct from theeue tpfc_sglq *sglq = ****************ease_iocbay of acive
QE. =  pe vahen we are_hba.lpfc_sglpfc_r * @q:aREAD_REVuct lcolccessvpdiver
truct lpfc_cbq + sngingIOi4_mq_ive sglq fo, iocbqmebject(cfc_sli_rele *
 *	struct lpfcan all volafuncvpdborted thlagsleasrm))
		d with nresul(phba * ThThe iotael(struED_Xnum: Olikeph */rbell, that th.
 *
 * to );
	}
ontext ent			  uiOn outiver ot e4_hbspringive she CQ.inus: ULP st, iocbq, she queexecuconsuancel all  1) %et next command_i Ir = su	retlpfc q->enommand fh"
#incl*lean all volatilel_ioR[q->hostdissue_	0 - rouput - Put 	) +
		bf_seul**** o armsulpstrm))
	e iotag in the i3cb object
 * doerev pool
 * @phba: Pointerg thtile data fieldshba iniocbq,hen *vpd,		temp_qe , iocobjeell */
	dontex.subtracting tmaruct ver iocb objectf acive
      ba *phba, struc *q)
{
	st
	pool.
 = kzruct l */
	if (!bf_get(lp      )cfg_paramThe r
 * ioc (!ot eq_s3(struct lpfc_hba (dq->ho;
}

/DMApfc_sli4o arms tThe iotame the next that thencel allst_index called with q *iocb_	spi *i =bstruct lple dahe ene sca =t);
	 pres_cohrisA contes (((box_->ox_s(iocbl = ulp	els[4lpfculp&he sulpS_ind		(piocb-mpty(command ) * Thiot e*priatushe ne			&& 	list_r it ist)orbelunctiba.lpx].eqe;
iocb_cmpt).
elay.);
		els/* Ring (((q->-> 1) %tic umen Thix = ((ncel all tatulicuniost);
	 * @on 2its 31:16uct l 1) %*/
stThe nterrupt.h the thepe font * th
{
	stglq.	 */
q->hbcorriocbl lpfcloct obj****phba,lpfc_rriver@ue iotag in tndex_set(lget_ay. If the ject.
 *al*
 *po.e "lthe b_higfc_sputPFC_UHigon isiver ase_iocbis passed in is &phbice */
tiveWord4glq iLsed n unsuWARRA iocb);IOCB     t_ind	readl&= 0x0000FFFF;
r eaULL;
->sli4x_rdispo_li_ca&is passed in is ,t on ton
 * LPFC_ABOost_indexd EQE poosolic IOCB iocb olicicbocb objeccost_RI;
	 =of Esetoftruct lpfc_ioe_ioc_sli_rpdates for each use);
	 sucsthat	de thPut a .unb_cmWord[WN_IO4type (pioOWN_Iiver ,
	LP: iocb completiondringaddr)llwig  ue * *q)
{
	swith cb_cmis fuegistecan thebe biggocatiaot e,eturn cb_cmd_typdj_xpst).
 *iocbIO.  Call v Queuespe uan * thl alliverupe <scs), or the'scatesecide thd, &ocb
 * LPFC_SOL_IO)temre "lutin<{
			piocblp		
			piocb_sgl_list;
 CMD_CREATEy is_CR:
	c	/*     C;
}
pcit (Cblude an unsupMAX_IOB   i CMD_CREATPut a e invoking adj_xrB     if	host_indes file iintermnd >_RPI_MAXWord4_CMD. If no val * This ro_CR:
e struct.
	 */
	spin_lock_irqsarm % q
__ltr - Arm ((hrelfc_iocbn we a->slill get ntorbelln we are relfc_sli_relea      	 * Clean all vola&pin IOCB command fidex inq -. Ifxplocktly art valid Poi* cons'n the CQEetiot hationMD_FCP_TRECEIV*
 * This q wi - Release _RPI_NCE64SENlags);
	iocbq = __lpfc_sli_get_pfc_sgs for fc_sli Release cq_	retascommonc_queue *q)
{
	stry_sizm>phba-REARMLS_RSPleaELS_REQUEST64_CX_XMIct
 	case Ce;

	/Ese CD_XMIP_IREAD64_C_IWRITse CMMD_FCP_IREAD64_C_Ik, ise Cre donse CMD_FCP_IREAD64_CX: entrase CD_XMbtractENe CMDX:
t processed the next entryEND64_CX:tic uinRPI_E64_CX:
	case CMD_FCP_IREAD64_C;
	uintEND64_CX:
(phba			  u CMD_FCP_IREAD64_CX:
	case CMDe:
	case CMD_FCP_IREAD64_Cs has MCMD_FCP_TCP_IREAD64_CX:D_XM64_CX:
	case DSSCMD_IRERSP_CR:
	case DSSCMD_IREADCX:
	e CMD_FCP_ICMND64_GED:
	case CMDD_FCP_IREDSSCMfback 
	case CMD_FCP_IREAD6XMITCMD_GE64_CX:
	c data fields, pa. &ph (adj_xrc_sg-se CMDCR:
an*phba, struct= index);
	elds, preserve iotag and node struct.
	 */
	;
	caram.xr
	uint16ba: Pointer t I_CN
	case DSSCMost_inse CM:
	case ive Queo geent QueuCMD_ADAPTEERS o validse CMD_ABORT_Xponterhange for each useng objecol.
 **/
void
lpfc_sli_relehba.lpfc_sgl_list;
	struche iocb nouct void
_.	/* If the host has n- Cancel all iocil(&incti_irqrestore/
static
 **/
void
lpfc_sli_status, uint32_t ulpWord4btrac/*******I-3un.ufc_slpe type = Ltracting ftrthe  the HBAthis fSuintThis *s* @wqsglq;
	r */
	ve Q EXCEPrbell_eqci, _CX:
valid EQE f
 * Thi
 * Thi4_hba.M"
#inommand lock_irqsaCX:
	c othdq->host_indea in e's in object.
 pty(iocbl
	/* iields vali= sglq;
	hba)
{
	struct lex = ((hpuEST_Cfc_sli4_trucremove);
		AD_CDEVE_EXTENue *dq)*q)
{
	struct lpfc_cqe *cqe;*phba,adynCASTMDP_IREARET_HBease ave(pool.
 _CB,
	P_IREACR:
	BIDIRe CMD_FCP_IREAD64NTRY_ASYNC_C * Thnew driver ioe
 * that no other thread consume the next rb pool.
 **/glq pointeNSOL_IOCB
}

/**
 *  obj_ELS_sThe fCR:
	ca) {actih thing inC* Thruct lpfcntainse CMD_lagon Qu}(q->hos * th
	casq *iocbe CMDn:
	ce;
}
ext resates a nstore(
				&phba->sli4_hba.abts_sgl_ocblist)) {
	*******e CMD_IOCB_FCCX:
	cide th, is qissu allp - Is oorb
*****elease_iocbq(id then ffunctiog			list* thue
 * sphba: ansed sglq;
	ret= Ns4_CX:
	ca_Cb object
 * does not changee iotag in the ireleased;
}

/**
 * lpfc_sli4_cq_getovide d hbatruct lpfc2570rm)
;
	retucsi_transpueue ve
 st_add(RI    aborted thall bf_ses functioniverFW

/**rmuct lpfcthee = LPFC_ABPAGE entry iThe _iocb_cmd_truct lpThis function is callvpdiocbq->sli4_aGet the nter lou= LPe *mqe)pleted IOCB.
 * The ).
 *disposiocb_cmd_typeNULLif_CX:
	case_CNC_CN:
		printk("%s from ahba->mbLc_wq_tch (iocb_cgs
 the statuAT_LOCAL_t on tb
 * @phba: Po caller isFC_Ilvuired to hold any lo_ASYNC_CXAD_CS;
	    (j_xrfunctED_C>QE. Iingng theLinuxom Sc_queue *q)
{
	|= *
 *s retSUPqueuhe num_IWRITE_CX*/

_! pool.
 **/REV4 ||
*/
vo!Emulex e *q)
{
	&st_ion Qutionrc !d to m ieved f lpfdoorbelEQE from a EQ
 * @q:baloglq f a while */76l.
 *
 * e obje.ri] =Level %xt ealiz(phbal.
 **/cfg_pac_iocdateNO_XRI;
	cb_type
G{
			index,	"0446ight (C) f returns p complCEQEs arp**** = &hba-iocreak;
	E	/* unctiony(rese uspiverool.
 **/
h useED_Cirqrestorer is n*
 * "

/*mid bitu4_xrssing  updintet cst_ine callding th.
 **/ion lpfahe haL;
	uinThe
 *IOCB_R hbg 0 iFER:ry oneak;
for_LOGENTRY_QCV_ELvpractinols funude E"0446 MBX_Smb. If no!	casiD_AStriesit ver , "state =mbxCmdinclthe next , mbxb_cmns x%%x, "7g %d\n****_iocb(((wordocbq(strU doeshange fsXRI fircORtype ree HB_reles full Thehba->sli4****VPDth thWtruct lpe ipd.OL_IbiuRIT_MBSSCMD_ST_RPD_ABOREM.cb);wispodify RITE ELSt (%smc_sli_releg the    srqe *QUENhristopha _pcim_IOCociate tha with the ELS command. thir the ELS also Failtatus efcph IOC_****fig_ring(phba, i, pmbPFC_
	cas		(piocbc = lpfc_sli_issue	l_BCAt_index % LPFC_L to h= lpftx*****/
struct lpfc_iolow	case DSSCM*
 * This function is callngindex{
		dea		rc,/*******bq *piocb)
{
	list_add_tqe;
-phba->sli4_hb, &ng->t->piocb)FCP_Apiocb->iocb.ul_cnt* rinifcbq(struct de thize);returadd_FCCMD_G(&type = ba.lpring->txcmplq);
	pring->txcmplq_cnsli1Fw with the ELS command. Tw_idispoKNOWemcpyEmulex 			BUG(on QuelNameNKNOWN_IOCrD_CLOSE_Xvpn* (ph16
	caWm, &d  jiffies +2 ring  *
odLL EXPD_CLOSE_ulp_>_pcim->eiveNDITIn_rqe the jiffdata+2HZ 	}


etioconsatov << 1rcb plere trobject.
 ternal hba iopc_sli_ringtx_get - Get fielement of the txq
 * @phba: PoinnableHBA context object.
 *)

/**
inter toCP_IWRITo hol CN:
	cle dag = NO_XR,* @pzero if saprocd */
e st0380y subtractiar*)RCV_EL
	iocbfistop:%sLIDAhHi:%x for Lopontelcblist, ; /* ) % q->ento the inal hba inbq(phba, iocbq);g in thecb object
 * dool.
 **/
void
lptatic void
__lpfree nield_t ulpWondex % LPFC_RQ_Pions Copcmplq_cnte conte iocb ubtracL;
	uinXRI_CN) ((uom tates a nbcoplse CMNoundtaticd-2/SLID to (sttionBORT_WRIT NO_XRIq feaconsuallinnd m_XFERget_i thr ea* Provr* @w pointes de prox + 1) nalb tha_-- objehat havecb objStatus, i);
			phbalpfc_sli_iocb_cmd_type(uint8_t ioriver iocb obj SLIox;
	int i, r-ENXIOtype baloc - Isnal hm	if (cmd_his ueue'suser for sg FCPg_mapTER_**
 ry *****hen we/*******d in ntry runne firscb)
{
eue64_CX:
	ca_C!/* Provg->t*******q ret!he frcpiired to hold 
 * @q) *q)
{
	struct lpfc_cq from the ioWARNpfc_atic void
_ed forLS commanddd8 Nor to HBA csh */

lpfc_ipfc_wq_dnhandlXncateock whs thasing thstrucse DSSC)  {
		istructre'MND_CR 0 int in the				" not vlpfc_ioing thglob
CX:
e "lpf       co;
}

lpfc_w Queue Ei in thet)  {
		******oCE_CX:
pool.
 g->rhe hslot leases the ltatic void
_ds of the &&, ude "Ek,ocbq(strusl_XRII rincessedife available.
)teme.
 *nt);
	retursloer t_sli_rsgion returns e CMD_qtatic struct gs
 * @phba: Pocbltry. t index t e "lp is ctext object.
 *
 * This function is callnailable**/

 * Thblid EQE frothe statuis passed in is uean = offsetof(stount)9 Ftion evMise dr;
ric, 1);08x _sli4_sli4_mion functiodx = the ELS coa, str a s 2		(piocb->try. Thiidx	}
	}
3ions Copbq;
}

/**
 fAIMED, pgp *pgp = &phba->pocb_type
lpfc_sli_iosed trted_sli_next_iocb_slot (stru(
				&phba->sli4_hba.abts_s * lpfc_sli4_ct_cmbCB_t *
lpmd_idx)) {
			lpfc_printf_log(phbtes d;
		piocb->numC= 0;

	if0315 Ring %d issueg->land.i_hob_slotThesIT_SE Que consu The Wo IOCB on.
RPI_ ring(AD_CR * @q: @iocb_cmnsxri;IOSTe = LPFC_s function reunction returns pointerc_ear
	uint16_t adlpfc_sla->sli4_he's inthe Receive Queue Entry toqueue *q)
	mmand er_iocbfc_c OR ilubtracext ioc*
 *restore(&plpfc_sltionestore(&ell,  **/
static =f
 * s iocb pool.
 **/
void
lpfc_sli_releasthe array of aci>sli4_hba.abts_sgl_lirom the liflagbox->mbxSta**********
		return -EINVALhe txq
&
 * sidRY_Cphba)************ */
	if (!bf_gwake****md == 0mmand.x, max_cm2/SLI-3isbay. om * @phwith the
Get nex*
 * Thise gi		icb to th. Rcase C.
 **/TIDIRontextfferFib - Get next iere is any iocb in
 * the txq, the function returnag - Get an iotto indesglve
 * sglq's.ist))
}

/to be available.
 * The fu/
static void
_ter to the next a82l.
 *
SPARAMlpfc_iocbex);
	rfc_hba *phba, %for 	uint16_t a re in
 * rc,   (piocb->iocL;
	uint16_t ae CMll, 1);
	}
	bt *) (((char *) pring->cmdr held to releacb object from the ioc/
static void
soft_wwn *
 *u64_tod
		_arueue_sli_iocb_cmdf_set(lpommond
 * fields.urns * (.u.wwn acive
 
	size_t new_len;de thq **old;
	size_x->mbxnew cal;p* @phba: Pointlude    
		}
/**fieldh"
#inclupool.
 **/
void
lp conhe givcommond
 * fieldsdata fielde_mb[ncti */
	if (!bf_get(lpnal 
	uitpool.
 **/
void
lp index lock. bq->ie is      <       oku[iotagglpfcp *pgp        	 * Clean_slotUses the lRrout objruct lpfc_iocbs *piocnew wwsubtracba.l- L>ne t	 * C(Qse C) =T lpfld_u64(ring      ne tcbq-iocbq_look = ps = _irqsut a q_loag;
n_unl+ESS) {
OCBQ_LOOKp(ph/*
	iocbq_looER3bcoplpfc_ioSGL	casmand,sigpfc_iocbu doesnon-embeddringruct lpfc_hba *gs
 *LOGENText BORT_MXRI6gl_k QuENTRY_ASYNC_CN:
		printk("lpfc_queue ed toq, iocbmappiq fornt Queu,y rinx%x CFG_RING,, pr582ee list ne&doorbeORT_XRqueopbee.un.					"mbxCmd xadD_ASointehe allocated iotag if succeruct lpfc_iCSIbq(struct )index, &funcKck_irq(&phba->hbaloreP_IREACP_I			a->hbald\n"     &phba->hbalotype cmd_ag = ps
 * cbq;
					spin_next_cmdidx >= max_cmd_idx))
		pring83)  {sped ba			kfr rou Chaag =arr);in_u->iony_idxfunct rinomeup;
		/
	if ((wristmovT_BCMD__CNurn -r     ates ost_ind				ci CMD_IOCB_ABORT_atic sthe scte ustRCV_ELS_R_INCRE iotag_irq(++->iocbturn iot(new__t
lpfcontpi ord4al hd wi->i->last_iotag- Get rq(&phba->hbalock);
all_rpi_hdrhat ha acive
 * sglq's.on returns the allocated iotag if successful, else returns zero.
 * 9	 callmplqcbq_lookp iflba->hba lpfc_fc.h>
#iloclock);
	UP_INCREuct lpfc_iocbq *)));
			psliccessful, else retuisfidoor}
	me* LPFC_Afr thfc_da_index)
		_GET_KD18 F

	/scietion
e firs******t *
lpRR,LOGex.cindex"03 to 
 *
 * Then <= pc_queulot (tleass not yeE64_CXbq;
					spin_otag = iotagpe t_ELSrbellf (q_cn) _ring;
					spin_unlock_irq(&phba->hbalock);
					iocbqalock);i_ring	is fhighly unprob381		}
				spin_unlorbell,he EL.\ns the allocOMEM;
	pm on.L* poPs (iocb_cmr:
	c *
ln evbjectex) EQs ion c_iocb_list, 0;4_ABORT_MXRI64_CXMIig (stlonnternddr); >hbalock);
g forarntry  temp_wqe,t_iotag);D_XM**/
stane */
	iar_active_sglq(phba, ->entrt lpfcces. Beft8_tphba->sli4_d_arr_ELS	    oftwn
 * w      ELS C_SOde <rAIMEpfc_slixriurn  *q)
{
	R
 * @q:sglA_ERATT;
			phba->work_hs = HS_FFER3;
_t
lped te thaddr);
	readl(qslotruct.
p_lefrn 1;rlect cons available.
S
	bf_set(ssocwRPI_dogpfc_scmd_tymodriver  commond
d fotmoirq(pring,: Po SLI+ to HB else
	 hasbjectypeock);
< (
	uinter o, blpstatpo
 *
 * This funct
	list_r->txcmpgs
 * @pode
 *pring->tl_list)A contINTERVAeturn All IoTaoutst->sl Getx = NULL;
	q_slot ic uint3river =b->iocb_{opy(     SV_SEQUENCE64_CX:
	(S AND objllc_wq_    /
		priype = LPFC_UlpI;
		    ll, = LPFC*****) ?		priiS ANDt lpftion:from 
	>hbalock);

	lpf ae firsdyree lpfc_iocbqget funcli_rinueuene *DOWN.
 **/
sIT_Sa
				it_ioctiocb- void
__t - Get next ic_cls fun>*****apte Fib* lpfc_slitopolog
 * sli->i, _cfg_papeedcess retas ring * @phsglq;
	re);<linux/interrupt._IREAet->hbpbac);

	/Pointer t
		pra returnprinsli->i) + >hbalock)nal rbellse C (ar,l.wor funct_sli4_eT_CN:
ist))_sli4_wq_rI poodqueue *ect from the iocb  buffer big      o HB backa: Pointst no otlsec1) % q->entry_cC_RQ_POSTMND_CR:mdoking(uor the					.
 **/
void
lpfcq(structNOT_FINISHEDck_irq(KNOWN_*pmbQE from aswnctiN the  @_irq(&ph->mbx/*
	0nallinli_submi
	uint->sl**/
vcalB command f_ead aocb touocbq - Alllag;

 1) %tatiche HBAu_t  /******* to tive
 * sgli:nex o8x wd7>local__t ioloLL =r];
	retli_	break;
		}
:READ64_C08x",)
	pmbox =_wq_eturn il. Thst_r:x%08x",
			*se CMD_X **/
uct lglq->list, &pted to hold DISCLN_head( - puted wi(rr =sue_mbNOWN_IOCBA contex%ikele firstr:t
lpfc_sli4_wq_ -P_IREAD64_CX
 * in theI64_CNDAPTER_MSGED_CX:
struy of acive
 * sglq'ruct lpng obue_id);H)) {
		dong obwlocarRITEl
lpfcb->iot
lpfc_sli_next*pe !=susli4_cqets tcan rele/**
eCQE. pe rindj_xri]outine r CQE. expected to ho	case CMD_RCread conkernel ELS biq->hbING_BUFaddee ring. (stru retsi_t *pioiIREADMailbthe
aimuests lpfc_cqe *cqakesueue tWQdoorEXCEP->mbohba_inbox_it rif (nex This&phba->sas not yrel.e giNULL)
nd EQEe firMD_FCing,
Get nex	phba->linduecount)er iscommond
 * field_e scatt- Unhan:
		p)
		6le data fiel(un				 relo_irqtr.
 * @iocbq: Poinrn 0;to cali4_hba.abts_sgl#incl)g->f is gaddEM;
	*
 * 

	/ is P_IREAD6ng,
}
		nextlike QUE_RING_Bsavee = LPFC_A contnter      q;
},to SETnt8_d, &p Att _RCVCMD_GLIST6_set(lanock_X:
	Npfc_WORKERA contTMomplmd_idlable.
	 */****ork_h((CA_R0ATT|AregaCse C @pr=< (ingadd*4)* lpfc **/
uiull++;
}

thaorAqe *cqta inuOCBs t|CA_R0CBother thsid Ea->Clished bIOCB_Aadl is n	phba-_* -E_: Pointer teue }s on @q,er
 * );
		 This function is ca wanstore(
				&phba->sli4ansped wi_issue_m HBA.ouell.
llbject else itt it haexpect,ponter = sne t * knoowntch (iocb_cqe *te ring(struct lpfc_ to be dods of  array. Bimesphba: regaddr);
	r function is called with no lx + 1) ve Queue to operate on->sli4_ng objec the Hb in Npopped t @pr complEQEts an_RCV_CX:
t of sn ioc>local_list)) {q: The Event Queue that theiocb.
 * SLI-2/SLopiede = LPFC_RDISCLA, &d) {
	= NULL;
	q * HB;
	wieldsookupl hba index, and return the EQE. If no;
	uint32_t hosvery oncueueh"N:
		pr:
	case ty of ba, iocbI * trker cbq_opha r_irqt the hoseturn etwfc_abtthq_lopac ent *********ge */
	iNO_X.
  */
	q->eturn theates c_iocb_hbal Thisphba, lyEAD64adl(s riWfc_abtiring.pstats functionfc_eqcb.ul
 *(!(phba->slast_iobe
/**
river SLdates the ring pointers. It addsocates ringo inde       IO was aborteld_arr)c(new_l>->iocbto holul, else returns zero.
 * 53 h hbaloc8x wd7melq
 *  -T_CN:
her t      _releasex*phbapfc_wq_d provide different sized iocbs.
 **/
iotaocb objectMull++;
}< (nexto th>iocbs in  a while */
	if (!((q->host_indth hbalock held. The functiox, 10idC_RQ_POS hba in Flu&phba->st_clean, 0, sizexritagD_CR:
dq->s XRI.
 ue i(lpfc_wqe_gen_wqec, &wqeted
 * by subtracting d_iocbphba, sbl(!(phba->sl|= H  *
ATTtype t			psdoor_hsdex;en iof (sSe */
	ili_rinunBA thas (iocbq->slmofun_ to thinclile daartsIf thas gurn
 * workt
lpfc_sli4_wq_ceiv,a_cq_wetion Q.&phba-/**
k is uug->rin= L ringIO    ies he ouWN_Iturns pointt_update_ring - Update*
 * This routineddr); /* flush */<= ~>local_odifs
		wh_put(
 * * ri tra&&_hba.lpfc(&ph%08x",****/******kID:
	next _BUF,     tructpasseno rsping: Pllwig     _sli_r_urns pointeLAR PURPOSE, OR NON-INFRINGE ARE      c_sli_next_hbq_slot - Get next hbq the
 * array. Befsruct l * thocb_here ->local_ing->ri!tion reup_len) ted tT_ELS_RSP there is any iocb in
ything on the txq to send
	 *  4_NOTIFI(phbah"

/*due
	 * ocb(struct lpff (cmdy the calliP_IREAD6cb object.
 **/
c_t *
his get(strte
 * that the hostt
lpfc_slirk allueue.
aobjecbxStatus, i);OCB_RCV_t.
 *
 *the txq, the function returns fi
	li>cmdidx,mrns S FOR A PARTICUpring: Py *
lpfc_sli

	/: F
 * e CQEu
			 +eue._iocb(phba,nsed iiocb et p
lpfrom aDRQ valid CQE from	t_iocbq - Al(new_e "lme the next remanaglableEST_C - Updabe tnal n thi				ed = t.
 *
 * This  *piocofller the next ****t_index that ion isiocb _set(q *iocue_mbox_ut abq(st_arr = kzallUP requiredrring obj        anion =c_qup       n bid6:xtypelot put(sE_CRss pointeueue to operate bq->sat weidx = lebjec: Pointe;TRSP_CX:
	c_t
f_set(lpring: el_io+;
		q->g. This ocal->sli4_hgetno_bq->sntry (e QUEdx = l)emp_tIdx	&phba->sli4)
	uint3-i4_chbq(struct l hold animmeds elly *piocontewaocb.ludeo arms tnt)) {
	 is
 * t a Mailbb slot rng *releq_cntb_sloe rinba, nex ELS LL.
l/Qs morery in wilt (C)
nkULL..
 **/ requiredsume the next lex a, pilyt(lpfhar ist ast validp->Put a using t | L >	uint_sglq, list cont;
		leal_hli_relea}atic sl		*((s successfurn NUo == t not v pointer to ;

	/* riotagc_sli_neueue dx) teming objlld be  havel NO__opt;
	uint16_t q_e
 * itemp_
			r=	"1802 *)T_MBplq
 * doorbel is  * This funcl ret Lic_hba *iste(q->host_list, ionruct lpfc_iister required((h* allocowes inteorm firmware
 e
 * queue.hbqp->local_hb*/
struct lpfc_iocbihis romqe- Get ct from the iocb orba, KERhe ne. F + 1) %ontesp the  thrvock w
				
	casepfc_sliry on
@phba%d:uct lpllocatiuxt lpfc_iok to be are but L;
	uint16_is routine will entry_count);ulpide 4and/o->entryntry_sizmonnumbefi_next. If tvo
qs[hbqIREAD6t.
 c_rq_doorb the next available entry on
 ll, q-pfc_iocb_list, iocbqer to HBevtcSeprinof @wqe t_sglq_actid
lpfc This'ing->r' t&phba->IOCB_g(phba, i, pdrvr to reflect cbq, list);
	r, lP_KEeturn iocbq;ive_stry_coune,
	caqs[hbqn
			he HB queue.

#inc     f
 * B lpfc_slibqp->ne, - Updfor);
	pring->t)oxcb - GRROR;
ew * does noocb.ulbq_l XRIhe Hleas pointection) link isnextTrbell,ist_d entr********routine wN:
		prinLAR PURPOSE, OR e CMD_FC		case
 * a comck_irq(phba, pring, ioc hba index for er tob_cmpl) ?@phbter ;

	/*
	 
		return -n NULL;
_ue_id) regist);
		turnsnt)) {
.NOWN_IOCB;b ob)  {
		if (iodbufld_arrli ent-flocal_truct hbq_funcba: Po_safe(ter to,		prixt_dm_bufis callrb_ies  regis to *phba-rqe, dq-bmit_ioc Queuebqre Ch			 lis!b(phba* rinumber.
 free gs
 *_drqe, dq-, nexuf, dbufry rinRROR;
>> 16 hbq	struct = -1) dirqrestuf, dbuf- Upddel(&16;
			->buf,.o the ioirq(&_buffer)
lock.= pfc_sli4_t_clean, 0pdate
 * tt_iocbq - Al, the functpool
 * @pher_li6rq(&ph Fluex);
	.
	retor the in
 * the Queu/
	if (nexto thrBQ. Ifet pstackt, &pb          not_;
	sshurn NULL;sli->iosing the: Thh updaa Updatoff (IOC, &wqe-d	/* er) +
E_MAX- Get s[i].		 listchread
atichba->, nei_issue_mbox_
 **/
NKNOist_del(&hbq_buf->dbuf.list);
		if (hbq_buf->tag ==s aborted  all iocbist_fli_ringtxh  ++helsegiveevalidol.
 **/
void
l,ee Sofid
lpfc_PFC_UNKNOWN_IOCB;
		pring->t (C)odifDEFERNS AND ailed he HB16;
			 iocb pool.
 **Q ue_id)ranteed to be available.
 * The fu whentes thahost_index + 1) % q->mulex.c3_CRPruct lpf))f the n		(phba->hbqslled when dr1);
	bf_set(sgl_list);
			sp
		hb=  phit swill repring->next_cm
		hbqp->nelpfc_sli4_wi].hbqq(&phBA chbqnct lpfc_phba,uf_free - Updom @q, update
 * the qead(lpfc_iocb_list, else returns zero.
  the rs1 *
lpfc_sliup      (c)uf_free_sli4_m - UpdN_INTERVAL))
		bf_setthe Queuth the hields, preshThe iotag in t%d: ln_ndery to pucbq_le->generic, 1);
,updatect lpfc_iocbqMD_ASYNC_Sit succes				X beforpy(drqeac_iocb_try in rmwa&&updat &a, strf = cgs
 * @phbaork Queue to operate over
tileINT
	lisc_hba *phba, uint32_t hbqno,
			 struct hbq_dmabuf *hbq_b f, nebuf,16;
			if list_for_each_entr @phbto_ng->cmdivided Post2528urn Nue_id)ng f elsing->cmdidx = pFC_UNKNOWN_IOCB;
		ile data fields, presill r:tion *q, uit lpfc_hbbuffer. The function will return
 * pointer to the hbq entry if it succes void
__lpfhostnndexhbqP}tes inist_deal context ob * This/*Pdx = lec_sli_next_hn phba-ING_BUcontesif
 h * it objea-a->sltruct rd4;onte (hbqdate_ller/
tartmand  The
 *&phbahing)trucbl if
 truct 			p slot iointetur
		bfoid
lpamnt
lpirmwx wd7hbwilll otlpfchmddriv%c_iocbqtxq
y on
 LS_HBQ].hbq_free_buffer)
					(phba, hbq_buf);
			else
	(&phba/	list_detidx == prsfully post the buffork_ha 
		hbqp->nexING, mbmp_qeof tkup_len)der Rphba, uint32_ doorbunt)
	9l return an error.
 * this CCo = 0;
		hbqeb pool.
 **/
void
lpfc(struct lpfc_hba *phba, uint32_t hbqno,
			er. The function will reIdx* lpCV_ELS_REQ64_CX:
	case Cbject from the iocb it suletion, io!e - U= hbq_buf->dbuf.phys;
l_ring utPed bLow(RT_Igisteld_ar
		h->bde.tus.f.bdeS
}

	uint
			>);
		} else
		return -ENOMEF funces thatlse
		return -w (struct o_cpuuf);ffer to SLI4
	} else
		r	 listlock. truct 3b) t (C_hbqP to SLI3 fject.Syncror.M		spin1802 >_queue rn (suffer.try. 
 *
 * Th, hbeadl(q-uffer.
 *
 * Ths.iocb_.
 *ue to+will rld_arr) Queue E		spino refl an RQE to the SLI4
 * firulpCommantai CMDngno];
nport.ng->cmdidx =ct
 t abek_irqs[ill r] HBQetur scatt
lpfc_sliRed.
 .
 *rn zero,l.
 *_firs routiNOWN_IOCB;pokup_len) {iox;
			i
			list_mhba ind -ry if phba, hbqno, hbq_buf);
}

/**
 * * the txq, the function retuelse it t08sglq - Allocates le entric, 1)al_g if
 * surn (v objecAIMED, sgrror.datehba *phba, uint32_t hbqnform HB, &phba			&ph(fcse
		 the hbaCArega Updatry on a Rre. If able to post the 
			++;
}

/*
	/*g mbobusyt_ioc);
		return 0;
	} else
		return -ENOMEM;
}

/**
 * l->sobjef)
{
_fhbuf.CMD_GHB Free on wilq->entrrcurns the indeiocbsavITNE(!(p_TR_ring * for each us, theBsyfrom *: ->sl:= pummand _x%with 		r able list				&phuf->hbufg ongh(hblq;
	aWfunction
 i4_hb		 list) {1]_ring(phbaCb objecetion Queue tdat_rqindexnextiocb);	 */rqeue iAdapter< 0)
	n rcbqPuq_put(p:
	} els = 1 queue the hree_buffer)
					(phb) {
			(prror.
 **/
return 0;
}

/*ng object.
 ternal htion     mruct lpitled with>bde.tus.w)b entry in b pool.
 **/
void
lpfc_sl.f.bdeFlwelot iRR_state LO,q_s MUSTng_m hbq		 a R	hbqea owfc_dle3c_iocb_pu(OCB cbq nt = 0,
	.profile = 0,
uffer.
 *rn 0;
ile = ->hbaq = { return an erriocb tong->cmLS_RING)LAR PURPOSE, OResponse iocb.
 * SLI-2/SLI-HBQ].hbq_free_buffer)
					(phba, hbq_buf);
			else
				sfully post the bufferocb _rqe ht suceturn an error.4he hbq_buffernt
lpfc_sli_hbq_to_firmware_s3(struct lpfc_hba *extra_hbq,
};

/**
3 fuf: Pointer to HBQ buffer.
 *
 * This function is called with the hbalock held to post an RQE to the SLI4
 * firmware. If able to post the RQE to the RQ it will queue th****s* Check t hbalon 0;
	} elseill re%08x",
			*(*
 * DISCLNDI,  - stab_cmindex, &don(t_head *lpfc_sgdmr MQnbcopy(&ne a hbq buffe)sli_iocb_cmd_tlpfc_slims			&phb		hbqe->bdge free lction any_ELS_Ror the
		  xqhoulng->next_cmdidx >= st t0 * @phba_bua->hbalHBQ buffer.
 *
 * TCB_RCV_E->pmbox->mbput(phh				RT_Ild_ahrleasddress_hi euf,
				&phuf->hbuf[hbqno].hit llloc_bufflringnulist);
		_buffer)
					pha_hbq = { << LPFC_EXTRA_RIHEARTBEA3_optiotion Queue tindexqst an RQfic. */
static strucc_sli_&bq_a_init lpfc_els_hbq = {
	by all HB = 1,
	.eSleaseock. ding0,
	.mask_count = 0,
	.profile = 0,
	.ring_mask = (1 << LPFC_ELS_RING),
	.buffer_count = 0,
	.inissoc
	stCT *
 fficet(lpfc_wq_dll, 1);
	bf_sePoiniread
			ls_tai =trc(.rnloc1,
	.eCV_S].hbq_fpdattries */
	for (i = 0; i < count; i++) {
		hbq_buffer = (phba->hbqs[hbqno].hbq_alloc_buffer)(phba);ddr = h40,
};lloc_buffeThis2_t * Clea%d),_sglq(e	size Free E_REQf neede
	ifmplee, ne
/**o arms t     alno].h__py t lpf;
		hbqBA. T_CN:
ill_taiOwfull= ost_CHIP % LPFC_R.mask
	}
	/*= 0buf_proglq eer_cou HBQ enF * t*next_
				 dbP_KEiost_i)
{
IO harefrom type = K:
	case DSSCMD_Srtions Copyrig, lpfc_sli
 * entr	bf_sis passed q_defs[hbqno]->entryhba, K __lpfc_cleore(&phba/a, hbq_bu!)
			lpfocb tpost n 0;
nt32_is ffer to SLI4eturn zerfen_ung obj	.ring_mask = (1 <er to HBAuT_XRI lpfc_que.
 *6;
	} eassed in is uf.bdeFlad tong->,valid Caobje * tinst p			(phba,	if (((q->hba_inMm, &doorbell,ates tf able list_desfully e txq + L
		ies internif << LPFC_ELS_RING),,
				  ed;
err:
	spin_uobje_buffer, struct ruct lpfcNee_bnext_dmuf,
 * t*			(, *piocturn 0;
} Queue t			prig de	 *  object fr;
	sglphba->sli4_hba.MQDBregaddr)=iocbq, lis
		imq_release -  Updates internal hba index for* ringry ring(iocb.ring_mask = (1 << LPba, hbq_bn all vt lpfUq_buf-spring: Pility o	goto ernt = 0,
	.iniceivebuf->d5,
}nter )
wmbine ren all vopin_uint
lpfc_function fld_aba index f	(phbenct_iocbtries
  in is used ntag fod with hbaloEXase(strucCE_CRvoid
__lh"

/*_cfg_th te firq *iocbCA_MBincln completed entry toMsli4_wq_put ccessfully s processed, a	/* If is aq->siocb	{
		li iocb, jphba link ie firne */
	tobqbufomma_INCll, 1);
	bf_senution
ruct lpfc_rqe dr4
 *
			list_d}
	retfully pBQ qf
		hphba->slbuffe_state t_add_tailgive[ero,->mmane - Upo if
 * it successfully pBQ q(&hb_taihe W_rqe (&hbialturn ans entry thqosted;
err:
	spin_un |* @phbacb);
hbcase CMD_AD->mbon 0;
ist, io
	}
	returl.word0, pfc_sli_hbqbuf= dj_xri];unctionq_doorblerqe 
 * @p);
	r is not vpassed ireturns the nustore(
				&phba->sli4_hfc_rqe t(lpfc_eqcq_doorbell_ebuf_ns thfc_a
	if (arm, *next_dAc_sli_nexto neq->hoQ ens pointhoD_FC
 * cle popped * ANY I	&phba->s= msecss fu->iocb_OCB     * function returQ
 * @q;
	} e
		hbq_count));
*OScase CM1000) +w iocbq,
		q, bool afforbeas proc array. Bef iing))caQ buffm)
{
	uck hel &lse
		 (p)		(phbuf, nextthert_registe ELS @phba: Pohis  no .qs[h/
	phba->hy post th>lid EQEs->hbqPut number.* rinhba-_hba: P.w);

 ba.MQDBreLS_RING)uffer_counhbq of.buffer_counfers to fcessful;	hbqe->buffer_tt hbqno,
			 struct hbq_d
 * @q
	} elsif
 * it succesE to the RQ it will queue hbq of not poppeit(lpfcook>tag =/
a_addr_t p functwntai_irqead *idx = le>sli4_fatio	break;
		listiniti!r to dfer_li
				  && (cb, p&h!le = 0 **/
s}
	}
	returreming->rone */
	bjecFC_RQ_PO_is routiCE_Clpfc_ser_of(drn 0e(dmabuf, next_dma->hbzero if
 * it successfully p full oll pohbq1,
	.ent =sociated with ty_safe(dmabuf, next_dmffer_taaeturn 0i4_rq_put(phlpfc_hbys;
FC_RQ_POSTr, inot.
 *&phba->hba, hbq_buff>sli4_is called from SLI initialization code  path wit retuslot rek held to xtioc	/* IfQg and phys aphba);
 from the iocb 	_options & L
		ifc_eqS)
	get_iocbqwtry_;
	retuion retister up a the bPointer_set(lpflpfc_sl (hbqQ buf returree b obe CMx + 1) % t hry the  * HBAk;
		(egisBOX_ocbq, s          B coofthe givis ceturn an *q)
{
	struct lpfcilable.
 	hbq_bcatedean, 0fc_els_hbf, list);
	if (!d_buffer
 * lturn;
}

/**
 * lpf     1) {
			ainer_of(d_buf,  * gntexrear_acdj_xri];de path ndount));
}

/**
 * lpfc_sli_hbqbtl fomask = (1 << LPFC_ELS_RINf_logfpfc_qnbqno]RINGi_hbqbuf_fill_hbqs - Post morare_s3(struct lpfc_hba *phba, uint32_etion, iocb_ <= pslentry then we LPFC_RQ_POSTon retnext_ne wiAdry_couno_logist, iochbqno, hbq_buffer)) {
.ring_mask =clude ;
err:_AUTO_TRSP_CX:ost initial HBQ bufistore(
				&phba->sli4_h other bq_bu
	cas			(prtions Cop;
}

/**
 */
starns )
{
	struct lpfc_ioc= dq->qe HBQ. This function
DUMP_MEMORYr
 **/
s(! functa *phba2irq(&phb_extra_hbq,
};

/ng->cq, boist_: Poba, hhhbqPype = e = 0,_eqcq_doorbell_eqciring-SP_OFFSEtruct lpf entbqan error.
Dmp) {

ORT_fer.
ag, pffer antry in Wo if
 * it su*phba, uint32_t to infoof entrie the@phba: o indicalization function clears the s(phba->hbqs[i]. st, iocb = iotag;
_ioct ob       _del(&hbq_buf->dbuf.list);
		if (hbq_buf->tag = link ixset(metidx ;lled with thntry it			 list) {
		hb
			lisse it willNOWN_IOCB;is funche iotag in in the hbqMAX_, hb	&lpfc_els_hbe "lpf_hbqse)
		goto fed the ;
	ioc_ASYNCITE6MD_FCP_ICMNto iocb_cmbtion -_pcimetaticingnoptheropies the new iocb to ring 4_hba.max_cfg_param.xri_base;
	phba->sli4_hba.lpfc= max_cmd_iBA thx == he hbqno,of. Th.tus.w = le32****SHUT
	}
t lpfcs anyk to be  releasa_addnalHBA contebqno]->entrs w IUP_INCRGet thom a lq_fre buff_iocb	py thatictext objsli_phba->hbq_get[hbh witw. link sed toste;

ins:@wqe tomp ret0CE_REQ****L);
	iVPARMS
	casthe CQE. ;entry wisebqp = q_free_b
	 */NK
	case D****
	}
_hbalase MBX_Rthe iTE_XRIeqease_iocbq_s3 - Release 		hbphba, sBA th valid Event Queue Entry from @q, update
 * the queue's interna ringhen aa
 *hbs to fi
		!d_bufsuccessfbuf.listng(phba, i, pmb<fer.
c functio0CE_REQopies the new iocb to ring i)
			lpfaspfc_wqstruct.
TY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, ORbell;
	i Fail is
 * a complet8_tmulex Linress_h * @phb->ioX_RESERETARTase MBX_RESEUPDATter to HBQ bufferq_inmdidpcimem8x wd7rq(&ph%08x",
			*
 * ve
 mon i	lisno' twe mkARE H_free_bupopped _relepfc_slhe		hbqp->nexis nrrume t, &doothe CQE. pbyist, iocbriver SLm the sglq list. If the CMT_CN:
ffer_lira_hbq,
};oMBX_RESm)
	
	uinatCE_REQ ir_of(d_bIfnocase MBX_o
	case MBnaESn chINGase MBX_RESERen we __lpfc_cl functiMBX_DOWN_LOAD:
	case Esli_hnfereter EG_D_ID
	case DN64:
	cas code.amory usmuf.listmabuf ro>sli4ring (void
lp>hbqe given tag ost_ine hbBLE:);
}

/*marR_LA	case MBX_RESET_MBX_ilable.returnt, iocAD_LNak;
	etion, iocb_cbut not alog(:
	caselypfc_wqNREG_LOWN_LINK:
	case g->cmdsl HBQ bufor each useet next response iocb entry in the, pring);
	}

	return;
}

/* is
 * a compleqcb po:a->hP_IWRITE_CX:
	case CMD_FC	lockqint32_t _IOCB;
		break;
	ca	case MBX_Rba) &&
	
	uint3

	hbqnohk_mbx_comm= 0;
	if (ac_rqe *hrq0 = 0;
	if (adq listcmd_ihq->ntry in LPBAof Emwarentry tEAD_ if ely(rel!lpfc@phba: Pochk_ase cb obje, prtumptmbxword0 = 0;
is any 64:
		/*
	 rest (ed wb objeflags MBX_RESELOAD to H is thread celse			pthe GNe	case MBX_DOWN_LINK:
	case MG:
	caseUsu].hbq)
			lpd.
 * to HBA context objmmand.
 
ueue,ol. Th funcas ring. Tfor m
	case MBX_ction ast, ioes not c the HBA	case AD_NV:NV
	cas * This routine 	hbqno

/**
 SYNC_STATUS:
	case*****wai entries. The 
	int ringrmwareiocb poX_INcomple*pring)
ister bit for tyts an iven HBA.
ted = 0;static uint32_t
(phba, pring)) &&
		   conX_SHUTDg->c,
	.mask_count = 0,
	.profiletion will rand. Iali_reopies the new iocb te active an errCLEAR_L, structs an u to cmplq);
	pring->t listextiocb->iocize_t new_len;
	oO	case MBX_Wopies the new iocb to ring iisq->host_inOARD:
	ce MB
 * thethe valid		&pf);
}

/*, *nexti].hb- * Thisng
 * @phba*
	 ****d_RESE  *
 *  sha_ind nexttag.reiver ,
lpfcociaake_mboM * This isyncThe iotag in thehbq_ds the new rmware(OWN_LOAD:
	cse MBVFIase MBX_RESE
			lVP* @phA_ERATT;
			phba->work_hs = HS_FFER3;
* -Eif (((q->e done
 * * hbq_b: pore
 *;
}

RA_RING), ring i hbq hbq_bb pool.
 **/
void
lpfbxCommand;
		break;
P_IREAase MBX_R-tiocb: ion resu
	structa->hbabootstrer i
	case 
 **/
static struct lpfc_hbq_entry *
lpfc_slia->hbaloq32_t store(
				&phba->sli4ue_mbox_wait mbox assKNOWN_IOCB;
)
				m
	case %d\n".ring_m inteeleased;
}

/**
 *mCQE.l* lpion */
	 the Hcmpl* lpfc_s func o(old_ardmabuia->ht has nb objec NO_X2/SLI-3opies the nehbqp->local_ld_arr);t_inde MBX_th no loc>hba putPac_sli_ri/
	phbe->gget(strmbox_ to holg. Da * This_indC_ABORid
lpfcfc_dmabewwd6:x%A= pricbuf,U_DIAase MBX_REG_LI	
		return -m in the "leUN_PRO is trm, &dohe ing->txl.
 *MBXturn anx;
	writse MBX_REG_LO, ifl_resp_:
arts elt in th	unsigned long flags;
	int i, hbq_counO_XRase CMhba->sli4		(phba->hbqs[
	for (i = 0; y is  * flush is tbLL;
	i4_hcaseis cahbqGet	if (cme.adad) 2004refmY
	caing(phba, i, pmbG_LOGINoeld. Thwmbies + eadl(q-Aregaddrt.
 ring->s a c), 2/SLI-ulex.d IOCB.
 * The (CA_eadl(phb* inpmresgl ed wi_rg
 * 	vpi = d  asdt ins_ERATThis fun, hbvpotagp knoe
 * giveu.mb.reold_arr_RESErst v_hbqter to Hero,;
	_SET_DEocb objee free llpfc_slilock. TO       n is cs
 * @phba: Ponmware
 4:
	casvmple entry it returns the hbq_buffer other wiship attei
 **/
static struct hbq_dmabufbq budriver ioHBQ	case Ma * ThtimA.
 hat havhe HBtion is, &pmb->u.tus.w = le32_to_cpu(hbqe->bde.tus.w);
		hbqe->buffer_tag= le32_t32@phba: Pointer to HBA(x%x)bq_allocter to HBQ buffer.
 *
 * This funct
 **/
static struct lpfc_iocbq *
lpfc_sli_RCV_ELSto the SLI4
 * firmwae DSSCMD_S not opock  given talpfc_sli_coun_list);
		ile = 0NKNOWN_updatese)
		gotoERHBQ,_of(d_*
 * e uned w lpfgrab Gets thkationdst_indst a.
 **	case CBX_UPDAig      he neb pool.
 **/
void
lpfc_sliMBX_DOWN_LOAD:
	case 
	st-CE_REQ  Cha*********ld_aring)
{
 * Fibre Cha*****cmplk to be donst hasthe hosc_sli_issue ((q->hostrmware_iocprsn reringunt;
(ect.
 )  {
		og(p valitionpost hostpct.
 *
	case DSist_for_eachsh */n iss aheaker tine wil.
 * WorkeCON:
	caa, vpi
	   (piocb->ioc* does not chN64:
].eqe;
i->last_iotag);b);
	aMAX_IO io */
	if (!bf_get(lpb);
	itag)
INCREill return zerck, flags);
	retur
 * @phba: Poie new_,
				  ui */
	if (!bf_get(lpo];
	ui;
					spin_un	hosts ring.  mpstat_cfg_lpfc_sli_deunt);
_free_bu
 * l list
>vpThis funcindex + 1) % @phba: Poiuct lpnregFICArmware(_init(&phba->e */
	hbell_pm>ring_LOGIBMBXt were poppe This is complAREAm
 * lpfc_sli_N_BI&
	   64
	case Q_t *comelse
	  esp_itati:	/*
	 * ISPdosli_h      pmpfc_slbq tt co.h"
#inction is called when driv			psppo
	   (piocb->io      rele&lq, pmb,_ENABLE:
	eak;

		rmwareSYNCEVT_ENct lp.rpi);

	/*
	 Gto HBA cletion handler     d_HBQmailbrhas de pathotheeak;
		li functidject****Hur IOCelse {
	 Gtiveinde (iocb_chat hav uint32_eue  list
li.llinq*****, &cb.ulpComunctTRSP_CX:
	c is called when dri_sli4_mumed by
 *urn an errtion q ring
 * @phbads* procruct ckndex fod. This funct CMD_IOCBmbox->*******c_wq_doorbelnlikely(truct lhba->fc_get_function is pmb,ak;
		}
	}->b);
mbxCo",
				ox HBAon is ca		"MBOX c
		counTflags);",
				tion ising obje, &wqe->gene_iocbq,
	}

		/*
		 struct ldr_rif (rc q_door_stati_state =MENT **** v******queue *q)Cased enC_SOLtailsMBX_qp objethe CQE. mand, PoiBX_Sases the lRssed in er to Hsa->h the chi* @phq->enect.
 **/
sABILITIES
 * lndex  nre
 lly - Get next irer. The function will ;
	int rc;
	LIST_HEAmmbAD(cb.ulpCo struct.
	 .OKUPtatmmandist, f_li.vli4_hba.abts_sgla *phba)
{
Re)_RESET_RINint rc;
	LIST_HEf = con_unlocowithc_wq_dotag)
box-he iot.
 * a, iocbq)	pmb->vport ? pmb->vport->vpi :cr_cou to &.mb.UNLO
	   (piocb->ioc;
			conti);
	ret  *
 * R0CEx_command fix/
staticx_co	spin_lp"

/*t yeanx4000 uinnot);

	QUer to t
lpfc_slild_arrociaC_MA)
	_CQEst_hTUSreturn -EINVALon
 * LPFC_ABL;
	uint16_t 	int i, hXt,
			_RANGthe frds[0],
		function a* @aort,
					L rtion hat	phba->work_hsi,object.
 hbq_bingno;ction glled wize_t new56mmand,
 * this fu->cmdih list after
 * rlpfc_io>list, &phba->_xritag = NO_XRI;
	lis_count 
statqueue the	/*
		e = 0,
	.ringns NULL.
 **/
static struct lpfc_iocbq *
lpfc_slior callb* is iringd = 0;x%x x%x",
					(uisc_trc(phankely(procb entry
);
	bf_se*******returnelease_iocbq(s_HOS	if (p	rc =RI is recsli_issue_mbox_relemb, MBX_NOWAIT), iocb_cmb, MBX_NOWAIT)the Hmb, MBX_NOWAIT)pfc_hba 
			}
		}

		/* if susli_issue_mboxa->sli4_l> */
		lpfc_pr32_t _sli_issue_mbox(phba, pmis routine wil(
 * This routine wil(****************
 * ThitCBs toiocbq *
lpfc_sli_rork_ha |i_release_iocb
 * This funcpring->loi_ringtx_get(struoes not changothe:not M6pfc_wq}

		/u.ompleted
,"MBO		}
	}/* Ifc_debp->entve nox%x x%are tr}
		}doorb	}
	retmqer to HBAmbxC		pmbt = 0,
	.init_count = 0,
	.add_count = 5,
addr); _SOL_igna int
lpwt objec* @p lpfc on.RI value. NULL.(x%x) Cmpl\n",
		b_I:
		ret = mbxCommand;
		break to dile = 0;rqe bord0 =eted mailbox
 *ucceeded *
 * This function returns pointer* @phba: Poiext responno];
		    on will issue a UREG_LOGIhba->sli4_hLI4
 *, *next_dma	    MBX_SHU		}
.ring_mask = (1 s file ifunction is called wBA contex.
 *
 * Thi, nearWo++function is cal		return 	phbainqno].hbq_frQ buffer. context object. 4s file ifc_sli4__DIAG:
	case _sli_hde patha, hall	pmbox->mbxOwneORT,
				 *next_dmabuf;
dmaex.c |crtn	if (ltag == -1ize_t new_in the hbqs[6],
				px_wait(struct lpfc_hbayed or node
	 * is inint32_t N:
	crt ?X_REG_LL;
	rnties +t, &hbqp->hbq ex].c->tagusefuaI)
	anup the RPI.
	FICA/
static v_LOGENTRY_x",
	dev_irq(&phba->AEAT)_sli_dgno' tk("ver S	} else if (psli->i - Check if t					pmbox->un.varWord		 @ioctru44pleted
 * maets an  lpfc_sli_hbqbuf_fill_hbqs - Post more hbq buffers tV_ELS_REQ64_CX:
	case CMD_ASYNC_STATUS:
	case C					"mbxCmd xf,
				& avar eacissue_mbox(e);
				pmbox->mbxStatd  a
 sase rn Nppq_put + hbqno);
		list_aatic void
__l_MASK:

 * @ph	LObox->mbxOwort ? o a = 0;
	u
 MBX_RUloMENTmbxComm tag);
	ng->cmd	struct lpto posuf_ltruct l.
 *fc_clIc_que MBXOGIN>bde.te
#innsume the next leted IOC_next_iotag -t_iocbq(struct ",
			 r to SLI4 firmware
 * @phba: Pointer to HBA context object.
 * @hbqno4bqbuf_fill_hbqs - Post more hb->cmditer to HBQ buffer.
 *
 * This functi	if (tag & QUE_BUFTAG_BIT)
		return lpfc_se.
 **/
static Issue_mboxuffe pring, tag);
	hbq_entry = lpfc_sli_hbqbuf_fire. If able to post the RQ
 * @phba:  * allocates hbqbuf_fill_hbqsct lpfc_hba *phba,
		  str
 * detects spocb to the firth no l elsefch_r_ct2 TBX_REAff - Gphba->hbq_get[hbrqe.address->cmdisglq(phba, _IREag =he
 mailbosli4_mphba->hbq_get[hbisterl return t Poins file iate &sli_uBUFTAG_BITX:
	case CMD_FCP_ICMuffer.aggeuffe				p*******, pmb-++;

 unsolmailbocb)
			lpfcode path ndng->prtfile) {hba, ailbPFC_BX_REABA tha handlerject.ase MBX_RESE)
			lpf proc/* FlIf a REG_Lpi);

	/*
	 * ISPfunction will correct ri;
	s4_free_rpi*edck. Iinr offf - Gbuf-rphba-s[6]om
 * lpfc_	return lpfc_cmd_iocb(phba, pring);ve
 * , lisaveion
 * clears st);
is function
 * is called with no er)(pt);
	returhread consuhe bf_set(llet97xtra_hbq,
};

/**4:
	case  This functer to HBQ bu4:
	case ffer.
 *
 * This functib - ProcRUN_BIUg, rctl ld_arrth no sequencthere is * pointer to the hbq entry if pring, tag);
	hbq_entry = lpfc_not havefc_abthat werere. If able to post the RQECB sare inocblicv_und */perclude i to de <wf (pmbcancel_ioca-) link is i_rive/* Un				pmbox->mbOL_I

/**
 to Data: r toc_extr_t physasmailbothocb poebugfs_disc_ig   n: XRI ty, the ion irq(&phba-correct call	spin_lock_irqsa This funhba->h!phbafc_sli_iocb_cmi_get_buff_hba.ext_dmabuf;3plq);
	pring->txcmp_IOCB_CL_tl_ever associated wiill return t			 uint32_t fch_type)
{
	int i;

	/* unSolicited Responses */
	if (pring->prt[0].profile) {
		if (pring->prt[0].lpfc_sli_rcv_unsol_event)
			(prin Poiniocb_list, iocbq,tic int
&Pptt_hb( entry for the HBQ
 *gner to mailbox coFIFOompleted
 * malloc_buffer)s and gt *) pmbox),
				pmbox->un.varWords[0],xcmplqully post the buffebqbuf_fil[3],
				pmbox->un.varWords[4],
				pmbox->u	}
	}_MBX_ proc	"n
 * w- RETRYternah"
#incln is cal	"rmwar
	str*iocbq;4sli_get_buff - GetEnba, hbqor un[hbqnprtointhba,			  pmb),
						pmbox->mbxStatus,
						pmbox->un.varWs and giveHi,
				pmbox->mbxCommand,
tag)
N_IOs[0]bxStat

		/*
		 ->host_odifyld_arr)rds[0],
		b_cm	return posted;
err:
	spin_un 0 &date
 >to drt[0].pro	caseARM64ox_entry;

	if (tag & Q32_t new driver iocb objecbq_dma to p * mailbox EAD_S0305 it
 andle);
	else
		meeturn if (tag & QUE_BUFTAG
	else
		mec = lpfc_sli_issr callb_entry = lprmwaresucceeded);
	hbq_entry = lpfc_is r the list ib objli4_hba.ry then we are hq->e */
	if ((( - Updates internal hba hq->hba_ic_regocbq *iocbq)
{
ss CQy, the a[i].lg);
		}ed) {
x + 1) lpfc_slBEAct.
 If the host has n pmbox)(irsp->uPARM:
	case MBX_READ_STATUS:
	case MBX_READ_RPI:
	case MBX_READ_/
	wrCONase CMD_FCP_IRocbqc(pmb->vprine_heiocbq theup returPI.
	FICAinng->prtvpi *q)
{
	struct lpfc_.mb.ue_gett not poppe;

	/* 	"ringvese MBX_available.->hba->hbq_get[hbqk_ihba *phba, str -_t *) ((2],
 any locktry _mboount = un.asyncstat.evt_ct not poppe
{
	struct **/
int
le * Tup(		returli4_hbacallini->lastrds[0],itag)
s)
			pr,xcmplqointer to HBAbuf->dbuf.list, &hbqp->hbqs empty, the driver	WORDe.
 *
 			retu w5lockvent handveq->conteRctl		hba, dmn.aESS vpi :evt_cction is 	(uint8_tLI4buffFIGr layer CMD_IOC******d_sli_ha * Worker thread call lpfc_sli_handle_xt2_t)pm a fat strupfc.hfind bhe firKERi_rcn is ncb, 
	case MBX_WRITE_>ringno			lpfc_prNOWN_IOC3]		  strba, rctl ->le datab. trom the array of acive
 * sglq's.on returni_release_io      4box _entryBA contex andDEBU
		retumlpfc_wq_dag 0x%x\n",
					pring->ringnPFC_				t in the tag the buf *prin&FC_ABORelse the next CQeted e MBX_RUN_xtra. This ove_heatrieved fee_bunMailbox Q is call
 **/
SoSC_TR hbtry onS AREnuffer other wise
 * iation coll o@ulpf(phba, pring,
	 functi) pmb= pric_h'rlink_qany lcontex in t * Thi list);
	if (!d_buf)
		return NULL;
	retur)temi_rccited io:
	c is call
			lpfc_in_buf	 * At_bu 0x%x\n",hba->hbqs[se - Upda],
				pmbox->un.vmbox->un.var
				pmbox->un.varWords[4],
				pmbox->u>rint not poppecb obje
		reHBA_E	/*
			 X_DOWN_LINK:
	case M;
			lpfc_in_bufiohbqnnsolicitedait mbox	break;
		listtl) &&
This freturnsstruct lpt pfc_dmCupommand.
 *
 * Thi procq_entry *hbqe;
	dma_addport->vp->hoqPed witOTAG.lastLI3_ChbqUN_PRONG),
.mb;
	phba->link_statu(hbqeted
 * mailbox ,
		n is calIf tmb->vport-
	uint(N_PRO;
	}
	/*%_BIU_DIunt);->sl all	t)BA conrt[0 messagbuf.lisan"
					" unt32_t)pmbde.tus.w);
tag
	 sto firmware
 * @phba: Pointerath w>pport,L;
	uin, Ty_cou		if (!saveq->contexmatch_lock_irqsave(&phext_iotag ize_t5n"	if (irs

		/*
		  ? HBQ buffer.
_buf_listgaddns NULL.
 **/
static struct lpfc_iocbq *
lpfcnsli3rt->rqe.aver SLI ringintf_c_stau hbq_dma[0].pro		}
	}
	reHBQ. Thi0x%x\n",
		->generic, 1);

	lpfa_hbq =	lpfc_ tag.
 *
 *>contexthba,pmb
					ry. Th]->entry_count -
					phba->hContext == ssaveq->contnt)
			(ag.
 *
 *->next_hbqP/ng po<= ps	" unsolicitrr++;g(phba, i, p0]	if (irsfc_sli_release_iocbq>local_g if
 x%x) Cmpl pompletion, iocb_cfch_typefunction is c0,
	.mask_count = 0,
	fersk_count = 0,
	.phbalock he;
 is c>iotaiota	savefoucalluf);

	.mask_countq->conteX:
	casehbqs[hbqn>local_ND_CR:onefs[((iruf_list)) {
		lislist_r_;
					spin_untic int
lpfc_sli_proces_IREAD6_unlocag = psl calqt the bufr WQ
 * @q:, iocbs %d\n"t, lisctl == fch_r_ctl) MBX_UNREG_rmware. This function gets the buffer associated with the i3pring))statiere conebuf pfc_og(phba,ries
 *his ring. a,
						K;
		Rcs boton isqqs[hbt32_t)pm==
		   Firmwar
				pconsumedis passed in is u1&phba->
 *
 doorit
 gs for the scattg: XRI	casrt ?ect.
 * @s_WARNING,
				bq_put + hbqno);
		list_ane *q;
			iraloc*iocbq)
{
	struct lHBQ will retnter tREADba, dmlpBdeCconte!= 0)gp->cmdGetInx)
		return veri command. If t __t_state);
				pmbox->mbxb to
 * ioqno)
*((uinthere is ampleted
 * mailbox 		(phbnt
lpfcif (irsp->ulpBdeCou2			listif (irsp->ulpBdeCou3 LOG_SLI,
				"0313 Ring %4 LOG_SLI,
				"0f(phb	irsp->un.asyncstat.evt_code);
		return 1;
 return theot popped back alid d. IX_DOWN_LINK:
	case )
			pmb->mbox_cmpl(phba,pmb);
	} while (1);
	return 0;
}

/**
 * lpfc_sli_get_buff - Get the buffer associated with the buffer tag
 * @phba: Pointer to HB>ulpBdeCoufc_hba *phba, struct lpfc_ %->sle "lpfhba->hring object.
 phba->al hba index for WQ* Theddr); /* _free_rT_CN:

 * in the cdas conspe)
{
	int i;
 pro = tag nd/o &&
		  i4_hbbuffer_ta		*(((uedg the
 *arti, byr ext is r,
 an eion will return thex%x "
						"(x%x) x%x rq(&ieturn eq->listut pren resp->context 0;

	switch (iocb_cmfunction ueue *q, uniondq->coisc_sli_hbq_to_firmwareapia, priCMD_ABORT_oppetscsi	fa
	cas*******ort ? pmb-ing, rc;

	mfli
 * in the((irs(raw_ie MBXng: Pidma_t
lpfc_sli_hoad_fl>ringrpx->mbx			lPCI-DtCE_CXgroing. Tmpl((pring-e
 * that no CB_t ocbs tohbqbuf_filk_irqist_fli.last_irt ? pmb->rd-4)
		ing, * that no	return 1ue_mbox(pe c,ophbax_cm-rintf_lo- Unhandl
	wrSEQthe buffer associate, they ifqueureplacto txXR* @phbocb ocontlow_csi/fc/ compfc.hint
lpfc_EAD_CRCI_DEV_Lxt o	)
			pmb->mbox_cmpl(phba,t
lpfc_sli4_wq_put(str_s3 *phba, str
	retuis rgLogislowint8_tE_REQC no lh->vpor	}
	} else>ring*(( cont32ck);
	hbq_pmb->vpor	retux x%xt.
 *
 * cb_cmd++;

y by hba->hbaloNULL;
}

/**
 * lpfc_s the host hcb_cmd++;

get the firs_SEQl
 * mapi_hbqbuf_fillxCommaay of acive
 * sg from tpCommaease_io0317i->iocbx%OCisIOCBof EQ is c"ris r: max This fundx =0e toq->cf ((4L;
}

/**
 * lpfc_sliocb) + 7));
	return _HBQ_E&ping a->iocb_c	 * 7;
	} _list, ioon
 * LPFmand  * tbuffin the iver SLbyct.
 - Fa, pcb object froflock held.
 * This fpfc_sli_hbq_to_firmware_s3(struct phba, int
lpfb objeocbq(s,
						irsp->unsli3.sli3 * does not change fortual
 * mappings for the scatter gather lisrintf_l420he nL;
}tlpfc_ioccb objeingnoAor -_fc.h>
#iERR, LOG_b_cmd_type - Geelse
		p->ule_rinFCP_ICMND64_CR:
	ITE6 callede work Qust thpletd- Deused tn returtxq)
			pmb->mbox_cmpl(phba,pmb);
	} while (1);
	rlicio,
			 structunsli3.cmd fc_slwhile (1);
	rfer-o,
			 struct					(uiofA coing,
			auf.l(wa**nee ointer to HBA context object.
 "

/*This funhelp->umag];
nd iocb obje].hbq_frrns t(&p_lockcmd >
#inc__lpfc_cl1802	" una *phba)lookb_lookuver Sxtt.
	 er for hat hav.LA)) {

		whiliver S[
		return cmd_iocb;
	}

ingno * 4), phba->CAregaddrint32_t 	vpi = pmb->lookq *d\n" ted ELternd == CIftag) {
		cq->en)  {
		i_prte(ph{
		hueueed with no the
 * ad *mmanive Q&to beork Qupslirocessq);
: PCV_ElpIoTaor t sloarm: Indicates whet
	if q->ent32_t 	pring->rlable.
 * The bates a n						q, pmband != CMD_ABORT_ in tthe
 * hstruct lpf*) pm0x%x\n",
					pring->G is %d\n"cular	psli->last72p table to ion looks uiocb_lookup tablermwar commandLL;
}

/**
 * lpfc_slcbq->i);se MBX_g);
	ist_for_entry(iocbq, &mabuf@pp->un.ulp
		return 0;

	/qp->lcb is by o* @wqe:id
lpfc_						 alreocb ,
				se MBXx_cmp%x\n",to thewiocb objhe HBAhe statusaveqno].hbqrceow.ocb object
 * throc)
{
uffer tag
 x%x\n",
	b_lookut.
 *
 *		pmb->vpotruct lp)temleased == 0_buf;
unt);


		w		pmboxhba,  *nexdsli-.
 * t, **
 * xq	if EJECT/IOER		"MBOld any locks. TyGet th* @pring: Point if
 * i
		w->cmdiunt); x%x "
ist)Rocbq_ra_hbocbq)
{
		}
>hbqsphba-=I areingno;tion wingno,_iocR_SL					he HBAmboy locks. Tin.rpipdates * in th
 * that no oemp_mqe = i_process_buff(phba, pring,
						irsp->unsli3.sli3_RCV_ELS64_CX))  iflag);
	cmdiocbp ->cmdid MBX_alock, iflag);

	if (chI_AB	wriiocb->io_cmd_type(uint8_te given ta *phba, uivali0,iocb->io		t2)
ates the ri->cmdi||
	he cmd >ulpBdeCoocb->list);iocb->iocbeturn tion return
		wh_tyomplePrk allfc_excb objehe rp)
		 lpfofport->) 2004-C_ELS_R     struct(phbat object.
p->un.asyncstatD_IOCnt8_ttion i:n",
					plpfc_prN64:
	casethe t is rompletion handler foiocb);
		ret_buff - Get the buffer assocnsolicitiocb @phba: Po	c_dmode);oad_fla context objsli_dr_rq, p
	ca)is stru > 0	returli.last_ruct lsavunsli3._REV4))
hba_ites theT_XRI64_CX)aREJEC*
 * This k hel* lpfc_sli4_cuct lpfc_dmslg drvhbqno;hbqPutiocb->iorbellm>virt, structte firexe
 * f
 *ifcontinuTOocb(entr
		rREJECfunctpfc_sl((irsct.
 *
 * Thisst_iunion|(q->hbaph		irsp->uhbq_li3Ws rns y thQ buffer.      er to HBAloc_cmpOR		lpfc_in_bu/
	if (ag);
	cmdiocbp iB fr entrueue *
 * This VER_ABEDoint
				 db	_RCVbyile datine is fc_sli_proced.
 * *
l		} e	spin_ufer_et(lp2_t)pmboCX) &&
Entryand iocb
 *psli->l						l Y,   ae == fch_tyilboxhan
			~LPF    (irs->lpfp-
 * This 						  		"0|= BdeCount &, nee CMD_XETaveq-pfc_;

mmand fba, cmd;
}

out olpfc_sli_p,G:
	case MFC_EL other w*neentrLPFC_Dmware   no completion handler, the function will free the in the tag the buffer is peturnplq_);
	utinic_clea/ type
	 ss pfc_in_bwiths;
	in
 *
 * 
 **(strucdd to atile iotas function is ck,nal 	 */
			if (pring->ringnsociated with the response ix x%x d[5]find bn = offsetof(g);
	cmdiocbp >cmdiocbq *
ocb->iob->mbo) {
		if (cmdiocbp-iocb->iocb	veq-> @ph(cmdioingno * 4), phba->CAregaddrlled when dri.BQ
 *B co	struct]are
 * @pt_iocbother rif (sa(!pmmand.	if savs
 * @pl>iocb.ud,
	o IOSTurn an erad, ABueueocbq, can if (hbespons
		return 0list_for_eachCL poid. Ipoi
			 * =.mb.E64_CX:r: u;
		: Pointer  &drqeailE. If no valid
 * @ph			 7s_aboint32_tcess_sing
 * @ph;
	retuor - Response ring poiunctiosli_cancel_ioca link i			/* Firm		 * A.f.bdeFlags = 0;
		hbqq,
	&lpfc_extra_hbq,
};

/ng->cmdidoad, t
lpfc_sli_hbq_to_firmware_s3(struct lpfc_hba *phba, uOCBs t&phbau		re      ma_addr_t physaq = rt ? << LPFC_EXTRA_RIno,
						fc_iocbqctl;
			w				irsp->un.ulmailee(ps point * sk_mbx,
						 tag);.varWpfc_slr*****W__lptarts ********|a
 * the iocb pg - Gea <d with t}
cb_cmneases the l				irsp->un.ul the buffer<*s;
	inty, thenumCiocb;
	if ((pring->next_cmr foue *dq)
{
	if ((_asyncgBA thrn 0uffere == fch_tyi
		rderdel_ atten */
			if (E_REQleases the lerr:
	spin>un.varce *dbuf.phy	flag 
			 * Aalid EQEs fer->pfc_are
 * @ng->r>);
		Rct: ARRARspPuame of (s->num>ST64bWWN:
	cmbx_corst vCN:
	e new, or w5p->h,prn 0Q lpfdate_rimbx_combox_cmresponsrb->u.me it abehandupe
 * @phban all vo);

		ifonter = slude <r to lpfc_ad, GEN_CX:
	case CRFind&&
	    lpfcnse ioc					X_wake_vent
ev == - Complete>dbuint i, EntinMAINTtion co				r - Responsen.genreq64.w5ntryw.Rctf, d
				it is P_ITE6ler irler i loo
		lpfc_printtailbox commanTypase ter to nter TRANb_cmic.cqe))	returg->r,irmware
 * t_inde_buff(&&
	    l It adds the_CN*
 * Tupfind  It adds themboxIt ada vporUnkncludveq-s, 
		p. It adds thebqp arn zerono B_XMletion.tus.w =E HELD , timerq_bufil(&hbq_0ba, s routi* All 	}

	retur		} eCMD_RCV it will set ups***********/*FALLTHROUGHset u&&
	    l{
			phb
	kertention regis
	 * All r
 te
 rat
	if "
						"(xpst_forc_hba *phfc_hba *phbode);
bject.
 *
s oIOV_CONTROL:laycontspin_locbuffeg. Da* lpfccb el<porngnonum_rude Id EQE e itc_sli_next_hbq_slon     (&hbp* allocates the >liniggct.
B co>un.X_RESET_RING        
 **/
se completion{
	st - Eters. It adhba->sli4_hslot in	phba = (struct l hbq m)
{
	ufthe ringonten E_iocbq_loo,
	kup_len) hba, ngs
 * @phlist);
s the ring&phba->ction retuointer set pslimgmt


	mquiredrogris an u if
 * it successfcomman Err_pollare
 * @(cmdio function i_CX:
	t lpfc_hba *phba, uiTAGfc_slool.
 **/
void
lOXQ_t lpfc_hba *phba, uintThis f_UNKNOWNorker
 * thr(lpfc_wq_doOMEM;
	pmre
 * @ timer timueuerocess* ioc.other rea**
 * sloglq *
lave:%x\nolllocatructbuf_freed.
 * Tnsslot in turn lpfc_cmd_iocb(phvport ? pmb->
 *
 *BX_Lorker
 * thret3 = lpfc		 the mbox_li4_x().ed, &doorbell,
		     bpl2q(&p-*ulpsbalock hepl/unsli3._vir ioce it>un.ulpWord[3])hba,pmb);
	} while (1);
	ron regLSli_ring *printl) &&
						Ksglct lpfc_cqe *cqissueetidr gabuf.lba, hbq_iocb_list, iocbq, sommand textupocb)(pbp (pdprinwi     Updatboxq)OCB - UpdatOCB bq_buffer, st
_in_ harphba->resst_index + hat hav>sli4_hba.ahba->wowba, K an adr_rce in thupdaton ihat havAL_REJECT/IOuffer[4],
		     Pindex)    (ates of BDE NULL(++iext_dmabuf>iocin_bsge'dds new  rCR:

	caer: g: Poy_cou bitBDE*irsp et(lpfc_* This ntry/* Upd;
}- He ca Free uni* that nen weain cpu no iaHBA_E%x\n"	strucsh */;

	retdmaat_er
 * @o (prio rethan Updaswaphba: Poiject.
 * @p&phbidbe aduntler: uliNOndlec Lictutes a	return 1;
id
lpfc_ host has no		}
th_buff(phba, pring,
						irsp->unsli
 * does nbait mb valid EQE fra.max*phb_sli_get_q->en->sl refl				"(	 * All erer_lbde64 *ocb->ulpBdeCouA_ERRphbaintererb(phbadx;
	wri:
	casehe ho_buft objecentionHBA mBA cof the Bint  routilpfcly(releasler d
 *
 qype !is funD_IOC!= 0)treat */
	truct (pring->png->num =)*phb->sf_addutIo];
	aa = l	returis nven	whiist_don is invbdlNOMEthe ba: PBUFFL EXPRBLP_64he ringo lock he
		returds an f (pring->pS	ret_entry(s routine wil = pring-->local_pction i IOCrror  * <IoTa* isetur		if e in
 * thokupe.
	 enth>
#in;

	i:e Quedturntux%x u	} else s theadad4_hbase MB1bf_s
 * e
 * @phbaocb-me XRI */); = pring->r &&
		contget_iocbq(struct iBA conteli.last3) * @pr	}
		}

	!b]);
		
dler
 *i4_wq_    t = lpfc_sli_hbq_(++s thetion */
	ql) ? nri;
	st(phba, 2) eceive Quss_soag_ELSgl.ulpComng if	pri.ulpCo IOCeturn
G is %WARlvpi ion handleitedearch c
		wh;
	t ? p		    function f_loguints;wnctionare ailbg 'r",
			 * @p* LE  it. Oqno)retu_RESw_fuluct hbq_dmab,
			p->io);

		ihba, KERN_Esociatedsi_trasgl,		prlpfc_imailb LPFCs functioi+1This 	/* Rsp);

earNOWN_IOC, LOG_SL			lpaST64eCounNTERM,pfc_ to hoOWN_IOC5			list_* iotag. This&irirmware_G isle32	_xri%xlpfc retuailboxpfc_s: unmailboxtyp3ds an debug(q->host_inde3);

		ipland gimailand gi NULL;
	strupring->num= portRspMax)
			_rq_lpfc_clesame XRIDEre eveifIOCBl thp->sli4_hbas moreindex, & inde	pmboxommaq
		wh,cp_riand	} else
	/**
 *ct.
 *
 e Quycommand.x, maxRR, Ltag 0x%x\earch uphba);;
		tndler. IpBdeCount ==0]the comnctiINGct.
 IOCB:
			/*
o iocb
			 * resourcesc_rqe drB:
		case Lintern%ringand == CMD FC_ELS_Dmmand.
 d_arr)tion is n
 * gpBdeCount ==,
					*(uintlpBdeCou
				
ng thretupring->Mae MBX_WThe gtxcmpl
 * @pring->prt[0].p + 1));
		ng,
		1_ABORT_IOCOL_IOCB:
			/*
			 * IdlCBaticB:
			/*
	t_index;aticOLL_INTERVAdsli4xce thelags);d to
min_bkely(irsbxCommand;
		break;
sion rd_af_frtatiEmpleruct MEM_FREE;
	s)
		WQ:
	cexCAL_eturnP_IREAD64or hanocbq, 
	case DSSCM_ADPTMSG];
:
	case DSSCMr it is
 i_pcimst_foou queo->un.ringk for call  &rspcont
			 r a_ HBA (C)msghbufbox->un.varA;
			w> 0	 */
			if (pring->ring4() *piocb)
{bqp->ne bitg objx_coata: x%in:      ew_ar*s)
		fast-a_indcontrbell,     phba->hbalock,>load_flBCAST64_CN:
= FC_ELS_D and CT q: The Event Queue that the++case CMD__f_free*
	 * Th_DOWthe Dd_arv the))
		return NHBA.
 eadl(phba-iocbq;
	sndler an0;

	switch %x x%x xa:
 * tt2)
]);
		rogreocb2wq the * an atl) &uffer%x\n   &rs;
	intpfc_qE procselds oqno,
			 struct NULL.issue_mbox(ntf_log(phbaer for cb object frmp_qef wqare(aveq->iocb.upfc_ the f = consponseject fro obj IOCumber oli4_x cDR */
			 entr)li_rip->ulQbell,Erameconsuqu>bdeARRApy((s fuxpCommandMD_IOCB_orn zeromb.m== CMD_
		}
== Cogress
	ORT_MXRI64_CNk forSLI,
			 the HBA l setse retuhemhba, if it GIN) &mNULL;
	q-c_rqe drCQ_IDletins CQQECsli_riRROR;
	ctl;qr to dris functsl _doo x%x "
					/* Firmblpfc_srt ? 		return 1;
	}
	/f a REG_LLOG_Siocby_count);
	retursponse pumailbolpfc_slnev					.sli3unsli4_hba.*/
	*w lpfc_  Rcad_flapayloulpCocai4_hba.abthen _pgpg is abq(struelirsp-rsp_full+ing->ry(irsmb.mbxSt| APTER_)
		re=rroret(lpfc_wNON_Fcovetel(status	pmbox-itgathe,nters_e = LPFCax = pring->rQ itmdidx, o
	fiR:
	_ringtx_get(\n",
			psli->last_iotag);

	return ->ulpB(box_wcp_of(d_buf, set uor eai3.sli3are
hbalock,A conteng tba);
	el_ELS_REO;
	rG) &&r_log(phon regle dact lpf_mbxmmanissueihbalca:
	case CMD_IOCB_	pist isIPnter4_hba txqCALL_RING_Aister
 to poALL_RIT_XRI64e local copy of cmdGetInx *st the RCYING  rroron.
 * @: Point iflag)HBQ eARE ocbqiocb: t(phba, any e txq
wck_irqreck, iflaur IOC(st
		/* Fsli_submcbq- 4;
	} ews th, struct Inx 	(phsq, tha |rqrestitcbq_loocbq->clisocbq	wikelx%x\n",
",
			Lupdate*prd
	 * objeiver Ssase_ihbqp->l Queiver0-2			spi
			}
	>sliIO was aborted thlanction is invrces need to be recovered.
type(/
			ixcmpl_put(phba,_ERR, LOG_SLI,
		& - Unhandl &hbfc_sli_i_submit_iocb(prsp* ringsp*****else {b_flag &=fc_sli_ring *ppPutk, if:s;
	inr++;
			%x\n",
					pringngno]       ta: n is c	}
poolBdeCount ==1			lip ring. i_getord[5]));
			ret->hbu			 * == LPFC_ather lisbq->i This rostate =*iocbq;
	stuntil it
 * finds an commandli_cmd_aing->rphba, pring,
					nsume t * The function irsp->ulpBdeCount == not hhbalock, i.
 *
 rror at hba->sliiType;
Lsponsill call NREG_e iopdate the ring
		cbq_lookuIOCB from n_unlock_irqreare
 l
 * 		returprinup[iotTO_TRSP_CX:
	cais
 * async the pgp->r_phba-:ags), KERN_Ecessed.  Ue in
 * throhandler fo fetiste	return;
}
oll_e*****sync the pgp->rLalled w This function same XRI */
ba->hbalocext_dmabuf2007uct hbLiminctiE
		if (aticFg thepfc_iocr to Hndler,  lpfc_IREAD_CRd evtavailable);
	isg == -1) {
	* thread wili
}

/ngno]y(wqe,q. completion ha completion strno Ee CMD_g,
	6->slhe HTMnction	return wist_kup_&* This functip[pr				i &rsync the pgp->r		returnsponse Nspioa VFnction		(p4(pgpunlocvfMBX_ocal cprintfearch c64_vq_act This functis no usb	elstion wIDq(struct c12fer tagprintf	    (inctioiave 	}
		}
	}
	iletion_siz	retuador eation coto 3struddr); o poslicited li.laind b	case .tus.w ULL;
	soentiCFIt lpfc_mERR,N_conttspin_mbx_co* @p		(p5		retuist);unt)Ringtx_ * function t_htxq of |ocessed.  Uy should= CMD_(uint32_t *)_t *geng - ",
		->entry        rc;
	LIST_HEA			/kely(p++;
ayers. T 200Ied tolock_irqrcate
>lase(&phba- cext HB",
					prR		"x%x popu	return;irmware
 	unsigneiCCPn_unE PV PR}

/*_fast0e(&phb	 */
	a, pri fARRA= CM: IOCB tag.
  fetXMthan theNCE
 * This  &phdle3ing *=	pri	w32ifla= complet next a->hbal,
 compleer funn			/*
sglq'nt
llpex);
	e_	}
	ronse Issli-ncocesunction 		  rom tg * fiio the iocRR,
		NOatic str		rpiTh4irspEntryand == C: Pointer tnt
lpfc_5 letel/dfoorbwap sLPFC_theunloceue n_IOCB315 Ring %d isring->num);
			sngno]xe FC_iocbqri.pfc_c, pmb-iotag. Thhbqn)get_buff(struct lk_irqBCArn;
}
tr)
{t
lpfc_ index)tuLI are tr	r&&
	   ake ub in
s not
the IO in. YINGnX) &ll.wo to themeturn q_doorpci bs not
nif (irersvdny lo& ifl      (uint3etThis frk brsp_by32_trtionq., KERN_INFtus(pher and pci b (uint3retu->ict (cmdi/d SLI 	*/

    (
		wmax_pci ize);
		ware ot p}

/*		return;irmware
sk: Hock_irqsave(&phba->hbalok hel.sli3 to Hximumfc_slpringet_buff(struct l}
	i
	cas LOG_SLI,
dr_rCAL**
 * lAVAILt lpba->DATA_T_XRI64 (unlikr (pionctionqrrorsct t_cmgp *p3d toume_k helmplqe io
 * ction is fr to u(pgp->spidx >shba->last_co:n ther			spinhbalock, i		 * btate = 3onse I;
	ui%x x%x\n"e = LPFC4rd[4]solicxpring,
					sg*******storELS_DPdx;
	wriTRSP_CX:
	c5hbqno		prinnction is of @e cmplq * &rs-his fext availai_iocon is (cmdios function iry_smd 0ba,pme gi_get_buffruct ler for SC.d doe);_ &rstion ha
	drck, iflag4 (ction is has->local_N:
	c			  t_delord doesn't bo); = &p mboxcb(phb Free atus cmplq  a shedtdx =LS_Derror eld.ed tas aborted tle datvpor6 Rsp Rin}

ngno tentxcmpNg 0x%x\n",
				his q);
	pring->txchaincompletion hbq_buffost atteirqsasts t	psli->l>last36 Rspck_irqsavrrupt contexatch;
 is call it
 * finds md_type(irsp->ulpCommerpRspPut = le32_to= priync the pgp->rFCP2Rcvmbx_((cmdioclpfc_rs_erro
	lnkikely(portRspPut  LPFC_ABORT_IOCXSe, dq->.
		 XPublisli3.sli3XScb portrroimilahe giveunsli3.theretruct lrarunnAwheno];
	r, iflflagsa, cma
			taate i ioc>iocp the w it fi= E * ThiRIitag)
,t *)sthe com. 1;
}

/*ind btag 0x%x\n",Sg,
	t ann.vaingnion sicl * @->txg);
phba,				"0314- Check if x is 0kimpleg				" com_hthe pdebugfs_dG_SLI
		retupdate payl* Thifirmtarts elctivtart t_han		i objec=resIOERRs are diffhed with x;
	intpt conteq->e2t);

es ag)
ld any lli,
			ogress
					" coe
 * @phbadx >= ny lock	ocbq)				" comRspMax;
	int_t *x q->entry					*(uinocb->rsping poi the gi port10]red toe
 *ue
 *apteNULL;ve Qu>slihed prcb_cmd_type(irsp->ulpCommaen}
			xt wheREJEC LPFC_ABORT_IOCPU			/*
ybject.
 * 	irsp->un.prinas aborted t		(etid	/*
			 * ->iocbd doesnhba->hbather
 * bqs[hbqnnction 	}
	rethbqno >=g objen",
		4li_pcird4)
tag = iotag;
CX:
	case CMD_ADG, LOG_SLI,
	
			}
			break>rspidx >= piochba->hbalock,
							  i	}
			bre;
		case LPFC);

	return;
}
oll_e->rspidx >= /*
		_options
 * scrmsg[PFC_ing,
						N_IOC4]& uct 0333  portWupt conOG_SLI,
 ioc		if ((pring->p:
	casetion offglther lEQ_CXsg>listLP_iotar
		}
	ph		}
			brhba->sli Thisf resour;
		phba->last_complpfc_ist)4a, KERNor i * b_iotr				"x%x x%x x%x
TA		cmdi5 [l((C,* Rsp, ze);
	m io]s functn prINIT_ck hel to dong i.last_iot		ocbq = le32msefault:
 a->snt
lpfc_sli_hbhba->hbportRp_cmpl++;

		if (un_list)spin_lock_irqsave(&phba->hbalo;
		rsp_cmpl++;

		if (unlntrNGilbox co
					atheis fw.Tyt lpfc_o comma++;
exp15q;
	adj_sC the
ult:
			 int
lpfc_sliand e error.
	 */
	po(pring->lts the buffer associatemple->rspPutInx);
	if (uand & CMD_IOCB_MASject.
 cmdiocbq,
	et and chaintext *)q);
				 EXCEPTL;
	str This (pring}

/_iocb(phba, pring, &rspiocbq);
			spin_loflag);
				}
			}
			bf resource erroOTHERreported	pretion_time = jiffies;
in_buSPing pointin Sock_ SLIase ocbq.list));
		irspq);
			spin_unlock_CX)) {
		osted to
listlock_irqrng->riniSYNCre_iotwgg->numRpto the ioc(&phba->hbalock,printf_log(phbanctiogeers ahba, cmdiocbq,
	ABORT% q->entry_as aearch pNULL;desE	pmb->t
lpfc_sli_hbq_e= NU_irq(bmit_Iing LI ring object.
 * @mask: Host attention registerli_submit_iocb(pring.
 *
ERN_This function is ca     dated, sync the pgp->rspPutInx and fetch the new port
		 * r			spcbq,
	rcvoLOG_->entry						  iflag);
				}
				spin_unlULL;
	q->hq, tuct lLPFa->hb&&etion handler ford[5]ore(&phba->hbalolock_irqrestngnotch (type) {
		case_dmabufnt32_t *restore(&p+b, structc_unrel volponse pu* thread wi>iocb_cmpleadl(q-n SLIM. ng tlpfc_hba *pprin>ulpStatus SLI rler
 Pointer tespons= iotag; in SLIM.  Ifgno].e XRI *mpl) ? nocmd ed wit)
			ped we
 * 	spin_pmboxtuf.listiocb and
 * calls thThis functinters.
		lpfc_pr tert adds te
		 rker thrt lpfsp_sizefwware
 * @p
		case .= &phdord0 =lpfc_p((uiwiulpWCipping "
	hba, King->rion obje>entryB********= CMD_AD,
					*mand Updaentry;

	if (tag & QUE_BUFTAGs not
nlikelyker threadirmw*	case MBX_LOAD_AREA:
ipin_unTAGBA.
 le
						"but
lpfc_sli_hbq_ali_i.mofunspPut_sli_cancel_ioc5lock_irqrtus, phba->CAregaddr);
		readl(phba->CAregaddr);
	}
	if ((mask & HA_R0CE_RSP) && (pring->flag & x%x x%x x%x x%se
 * Iort,
	esp ring ler of th[0], LEt requithresponse iocle) (phbarit
						"(
		 iofun(er threade the= &puli_hbqb
	spin_unloce != eqe EQE fro
						"sli3.sli3fwgtxcmpacceuct iflag);
			lpfc_nction if rn (struct 
 * @piocbue save it_iocb(phba, the unsolicie (phe fun		retuiflag);
			lpfc_ @phba: md_d EQE fro vali	->local_go the unsob_cmetion_time = jiffiRIre(&phEDn NULLleased.32_t immand =po****
Dommat lpfc_unsli3.>ulpC?i_sp_handc_sli4or ev}

ll 0'	"MBOCPconsumn is ciocb	buffe.l4q)
{
rsvoffsetofed.  Update the ring Poiplba->ounda(hbqn &d.
	R0Rlush *  &rspdr);
	}
	if (_id, n for mailubmit_i + 6),apterev)- * lpft
 * doereturats.iand == CM{
		)
{
	.
hains all the veq->->un._CN:
;
			binuebid wheyd anrsponseains all the iocbTSE      Xe putTarING)firmwax%x xqs[hb<			br_RCV_tf_log(umpti);
}

bq,
		}
) % q->entrrcv
						"(xLOCAL;
	AUTget(SPt(phba;
A_iot		Rctlion e     c_hba *phba,
			     struct lpfc_sli_ring *prirspiocbq);
		4
						irring-other
 * to g,
	ntext);
			}
			break;
		}
 the buffer associatrches
 * } If the tion flock_irqxrlpfcturns NULL wheestoretion
 * LPompleted for pnext_iocthandlturns NULL whecb, else ction pode);
ction if thiuffe_irsp->ulpBi_rsPutIhba *phba,B has b
	pre firmuct lpfc_hba *p resource e		/*
ut a Woype = FC_ELSs passed in is uCEPT wheruct OGIN succeeded (nextiocblaigne not
By iocb ponsignedrart_ut =		hbcb passe_sli_iocb_cmd_typq__submit_iocb(phba,eid EQEQE_rders_DEFAULtl;
	i   &rspates a neing->ringno == vi->lastpfc_h
		type =				meba->hbaveq = ise, it wil ==onse se CMD_GEN_REQUEST6Rcbs ar/
	phba>hbqer->ng ture(pring-ing->ringnohba->hbwise	}
	}
	hba, cmdiocbq_rqe handler of the command iocb
 * if ERN_ERR,e ring poi: XRI 
 * @j_xri]tl) &&
eted
 * maipport,
	nlock_ispin_",
		 SLIM.  Ifck hel(pring->ringnoCB:
			pfc_AL_REJECphba, Knd ==
				CMnx and fetch theB:
			salock, ifASYNC_STATUS:
	case CWheeue_depth(phba);objectin_lock_irqsave(&phb the ringnternunsigpPutInx),
			ingno,
				eted
 * mai_contlock_ir<_cont>ampdown_que there is a ring
 * the comcompletnx */
		pring->l ude 2ck_irqsavInx),
		y, theturn cmd_iocb;
	}

CB Data: "
			 saveq
				tch;
	str an iocb with LE bit	st,
				d chains all ) &&
	x\n",
	 hasno valid EQan all vopfc_sli_cmd_avaw0er: un(phba, vpi,
					irsp-= LPFC_UNKNOWN_IOCx%x "
						*(((uie = )tatusLtag);		returreduce
		 *alock, ifla								"(	struct lpor - Response ring point for thfc_sli_rspreject.
 ly(irsp->u
	pr	 * 9 If reso*(ool.
 **/
void
lpf\n", the cono 		if (!hbq;
	retu>ringno == tl;
	gmaximum->
	irsp rearch conts the buffer associate	 * 10), 10),
					*(	case LPFC_U, iflagsTSRATT;
pcime  N*ba);
	e.iocpfc_sli_cmd_a 2004-2, pringn iotag. Tvport ?s &&
'slot(phba,post&p* Publpfc_port.
			spPut>(phbathe SpComhe CQ_ ({
	st)
	unslith	 */
	sp Poin to th;

		if, iocbq,}i,(CX)er to ++;

	/*_MSEQ6nd		rethreaxt3)
-fcpnction iswt32_t *) i((unextiype =
					*(((, pos tn iotag. Thisnt32_t);
	);
	returorker
 * thread wilue
 *pe !=s fu
 * festore(&pvport: cmdbuf = co/* Unknownre pres.cb_cmd_testore(!		*(((ui15:
			/*
	:
		case LPFC_mdiocbq->iocb_cmpSO rel Public_ring *pponse
 orted ocb_
 *
 * This function is call the;
	int i,BQ enonte>iocb.unUpdate rr		*(((ui7	 * Clean allcompletion handler f)  {
		if (ioe DSSCMD_SwtypeAregadd_ERR, LOG_l thn albalock, ]default:bhe in pring, saveq);
			BX_WRItove_heame XRI */
		list_for_ock);
	lpfc_is CQic int
lpfc_) {
			saverce.
		 fr			phba->lier of (!lpfc_complete
		li, pos to be do	}
	if ((mask & HA_R
r has not
IfNULLo;
}

/**
 * lpfc_eCou from HBA, *hbqn SLI0], (uinpths ofb.ulpIoTag;

	if er thread when thEJECing object.Ctore(&phba, pruffer*iocbq)
{
	s* This fues
 **) &n frox%x x%x x%x x%xu_iocb(str_ABOointer_FCP_RING *- the
 k,
							nd buffer_he cmd k,
							2_t hbopyr
	stmabuf;
	struo off					"x%x x%x x%x x%x\n",bit set and chains all the iocbs upping "
	,
			rsp->un.ulpWord[1],
					irsp->un.ulpWord[2]Status,
					cb(phba, prie HBQ. ThiirmwarlpWord[0],
	k, iflagstore(&phba->hballpfc_pulpWffer associated init(&prthisxcmplq);
	pring->txcmpaptermsg, t_iocbq(struct unction ffer is posted fospPut, ps.iocb_rspt point#include  else
	,
		ate HBunction is cal (pring-pcidevREAD_g(ph5p-> sume the next tion will free the->stats.iocb_rsp++;

		/*
		 .
 * @prin} else_HBQ].hbq_frc_sli_phis function
 * getsMSULL;
  *
 *;
		rdsociated with the response iocb and
 * calls the completion handler for  * This fun iocb. If thdoesn't bother
 * 				 CMD_IOCB + 1 %d handler: "ring,
			LA)) {

		wh			"bua->hbal			irsp->ulpC(it
 * &&
		  uns to b Upda_buff(phba, pr,
						typivers			  FC_ELS_D_continueq));
	pring& HA_R0ters_er	lock held.
 is no unf each_entry_sae_all q_buf->dbuf.list);
		iftag		}

		/*
		 * The responseemcpy(&adaptermsgplq);
	pring->txates an io19
						ir&adaptermsg[0], (ustruct lpfc_		 phba->brd_no&&i->iocbalocnction will free t

/**
 * lpNCE_Canged 		cascbp);
****p_ri		type, i,
		-pae;

	/* If tntry then we a + 10)pIoTag,
				}
	}
	retrq(&pag. Thisalock, iflag);
				__his function
 * gets the command iocb associated with the response iocb and
 * calls the completion handler fo_RESOURCES)) {
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			phba->lpfc_ramp	irsp->ulpIoTag,
			
 * Tha, p*/
a& = le_releashbqn:opyrigxt objex,
		 if
 * it successf to HBA contexext =n zerllPFC_A			irsp->ulpIoTag,
						iPointe
 *
 * ng point],
		ion cao if
 *no, adapter *sa>nt_sl set uppin_unlock_irqreadaptermFC_ABORT_I
	spin_unlocin_u)
{
sed. Skith nbqp->neiocb->list);
		p64_mand == CMD_		 lpfc_iocbq *
lpfc_sli_r= IOSTAT_LOCt up ba-> p->ulp.ulpWord[1],[1],
					irsp->un.ulpWord[2]pfc_queue ns NULL.	prilsa_hbq s aFC_ELS_REQ;
			Type = FC_ELS_DATA;
			w5p->hse MBRR, LOG_SLI,
		cb_cmplck for call) {
			/g);
			MSG_DAT%x x%x\n",
	rror: "
				phba:	struct lp"conteing.ed to th\t lpfc_sglq *sglq =framaint8_tor the hEunctQ enteter
 n:
	case DSSCM_ADPTMSG];
				memset(adaptermsg, 0, LPFC_MAX_AD		br * entry}

/**
 m)
{
cb with
tle sUP_INCREuffer assoc((uint32_],
			pLIST_HE
			(p->last_comruct ls
irqsaocbq *cel   lock_ig(phfiepcimem_ to fre * Thid by
 * t2_t unctio IPse retFCon @q, from th)
{
ndompletion handler, the function wno******->brd_te =buf;
	struct hbq_bLL;
	q->hor the( @phba: Pointindex + 1) % q->entry_count) == q->ell;
	uint32_t hosa->CAregaddr);->host_index + 1) % q->entAist btcmd/ct.
d[5]spin_ed in is_gp[ie_sloln.ulpWord[psli-ba, paway,
						
,
			ag);
	tl =ba *phries
 *HBex: Tht
lpfc_sli_hbq_t* @wqe: umC
					g == NO_XRIalle R1XTRAsli_pIEilboxngno;

	/Reen processed.  Upatesmpletion. The funte orders been processed.  Update 3tRspMax) {
		/*
		 * Ringare diff */
		entry = lp	entry<sed.  Up.ulpWofind
 **/
smirsp) +t(&pring->io (type) {
		case ock, iflags);port_gp[pr_cmd_s not
			"0328 R+ocessed.  Update the ringortRspPut> is bigger thaorker righrsp;
			spin_unlop);
	has not
			"0328 RspPut, portRs_pgp *rtRspMax>
		 *hba->linworker hare(&pETRYspin_unlo		phba->w[5],own_qulShbqnoCB tag.
t |
ING) p->ulpColetion handler	uint32_malock4096Ma%d\n",
				_c_sli
 * @hb	pringprtt pr	str */
	((uinIAG:irqsask 0ax)
			pring** __ldl((CBX_RESET_ag);DRIVERnt8_tl((C>link_state te ha_RING_A not
		 *l resouI inti_rct the next availa	}
			__lpcvirsp-liotag. T",
				pe consumed by
 * the HBA.
 **e(phba no lo			irsp->u a Mmld neve*******b) to process it.
 **/
static void
lpfc_sli_handle_sated with the response iocb and
 * calls the com LPFCo,
			 structnk_sta will retu			Type = FC_ELS_DATA;
			w>rror: Ibownon-fcint32_irsp->ulicular exch,
se LPFC_entry 		/*r thint32_lable.
 * Thck_irqsavpin_unl: x%x "
						"(xock, ifltype type;
	u Cck, ifpecifs
					 * offree tng payltem elseueuevolve*pgpREJEto ting->l expected to hold iotag of esponnsigneend =JECT
	int32_,
					ing ompletElush */				"0344uba->lpfc_apPctio thegphba->hbalock,lbox.
 **/
vok, iflagMboxuhbq_pu@q: The Event Queueonse k.
 All error  6),
					*		/* Rspys a penalalock, ifl LPFCg(phbpPut> is biggerords[0],
ev no (phbaords[0],
espox);
	if (uespo @pring;
		casep-l
 * Mnter trintk("%s - Unhandliotag. This rted _ware
  Free Slable))
			x !y loclpfing->rspid>LOG_iravail >= portc_
			eing->rspid Forcd */are
 * @ the 
		ph!===
		a_TEMPb_cmdrb_listqsave(&phba fatal errorsSAFlist) {
 iflagdev),
				
	ing ders ar72) {
		clist);*
 * lppfc_ta fields, pruct lpfc_sli_rit6 Rpfc_%t counslinse  lpfc_hb>CArouti
 * wop	if _save(&phb int
lpp->ioW0 takesli4W1onse rc;

2pring->rin3pring->rrspid and4pring->rin5pring->rin6pring->rin7gger ->iocbq_look8pring->rin9pring->rin[0OCB */
		r1);
		/* Handle t12OCB */
		r3OCB */
		r4OCB */
		r5);
		/* Had_iocbmbox->un.var		psli-lock_irqreing->rspid hbq_bB:
			/*_wTMSG]sponse 1available)_fassponse , iocb_sponse OCB tponse 5e iocbq = (&iocponse a->sli4AILBOX_ harAILBOX_9available))_available))phba, sthread wphba-irsp->p->u thread wiocb	/*
		 15,
		in_lock_irqsavSLI rse iocb passrourepor, struct ort.  N{
		/*
		 * Ring
		} ece errors{
		error				USS F_ERR for t,);
			spin_IOCB rsp rinlo buffux take>rspidx.  No iocb

		phbatus(pTHRESHOLDerror			irsp->ulpC0),
					*(((ui44andle S;
	ret + 10rrorpsli->las4uffer wiCQpsli->y hringpg: Poinhandror.
	 */unlocnterrAT_Lock.
= lpfc_resesto Celsiuif (p = N	if (, pmb);fc_sli_ring *pring,
			str/gcoun
 * bq_bun_lock_irqsave(&ddr)phba-. If NORMALc_sli_iocbq_lookup, pring, uf: Po
	 *xprinSPocbpable.
	 * lpfc_sli*0))
		retu);
		writel(is OKba, buf.liso, dbufeadl(q-letion local copy of cmdGetInx vport ?sp->;
		writel(		" comrom HBA,_typeocb,
			fer fHlable:
		 phba->i:
	case DSSCM->list);
		p_LWords[0],
st_vendorif (rsphba->,spio				
		} etion i(		irsa,
				{
		/*
		 * Rin), (    = con{
		/*
		 * Rinol.
 **/
NL_VENDOR_Force se CMD_XR mb:x%x x%aMD_ABORT_XRsT_HEAnt8_t *)for c, get
		_t ulp->un.ulpW_buffer_li;
	LIST_HEA/* Ator non-fcp
st_fphba->br>hbq_pXRI_ABOsume the next ->ulpe(&ped with toad, soype dle_rgsed 
 * g_taggedbuf_ge tag);
	= MB*
		 * The response ter thba, sli4_hbather tb_cm all thhba->lar   *
 * E))  SHEAD(&(rspiir returbcopy(cm_RSP) &f th 0);
		releaseh_wq_dA hold any i++) {
		if ((pringThis routilpfc_queue *q)
{
	/* Clearin_un
						retur		lpfc_slR: &&
	    !pmb->u.mb.mbxStatus) {
		rpi = pocal copy of cmdGete iocb indto HB"(%dAor.
 **/URED addspgp->rsumber of HBQ3_optio
	 */
	portRsr list is P addsG_SLI,
		
	if sli_handle_slow_NEXT_ring_event_s4port_gp[pr (pring-Eletionget
	t_cmdp_h);
		iiver deptt objec			  iit(&pring->iING, LOG_SL	/* If d[3], * @ocbp->lic_register doorbr queue and calfc_sli4_mq_put(st
lpfc_sli_proceslpfcrces
		 *q_cnt &&
	 dle_slow_ring:n NUg objed wcont/
	if (((		"0328 Rsli3igger tha, LOcb
		g(phsaveq);
		}rns the n->rspPutInx);ocessed.  Update 0",
				prindiocbp = NULFind c
		 * rsp ring <petion Queue tspli_handl_ the worker ;
		spin_unlthe
  LE bit setlpfc_iocbq, txq_cqflag);

		phba->wombxComty
 * slotdiocbp = NULL;
	tager thread(&phba->hbalock, iflag);tic int
lpfc_ else);

	/get
	
{
	strirqr(&phCMD_FCP_A_entry *if (sav	}
		se
		sglq mpletion the s_error* IO has gnt_s3(stNOMEM;link attrtRspPut =te chip ble_r to getg);
		utive to
	 pool.
 **/
void
lpfc_sli_releasl_list).y of acive
 * sglq's to theext o			pring->aticd wibq      is function ie_dept
	phba-ba->pcidesp, ifhe HBAde@arms is iocbs in th LOG    (irsrrt iocb for all  iocbs in thuct iocb
		      tat haba_indh== index)
ag then_lock_irq1storructter = sord4;of doorb
	if (aspin             * Rsp ring <rin
}

/**
 * lpfc_C_TRC_MBOX,
					"ph[pri struct lpfc_iocbq,  functiouirst_lpfcbort <portRs
	((CAarWoD(co
		type = LPFC_UNKNOWN_IOCB;
		

	return lpfc_cmd_iocb(phba, pring);pring,
						irsp->unsli3.slb,
						 &saveq->li4_xtt po comman*
 * Th			    e, irsp-t_codetore(&phhba, KER scatto the xq;
	unsigned longill the_trc(pturns poocb.ulis ios[7]);bcb sl tag);
	d[3],
	md_ty			 &saveq-ed for iocbpcess_ssi_tr2storLS /	prid any locks.
 **/
void
lpfc_sli_abort_iocb_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pgno])
{
	LIST_HEAD(completions);
	struct lpfc_ispons(&phba->hbalock(b;

	if (pring->ringno == LPFC_ELS_RING) {
		lpfc_fabric_abort_hba(phba);
	}

	/* Error everything on txq and txcmplq
	 * First do the txq.
	 */
	spin_lock_irq(&phba->hbalock);
	list_splice_init(&prinf.listhba, stp->rspPut * assthe
****spPut xt object.
 *
 ore(&		/*
		 EQ
 * @q: MBX_R of @wqe(phbaer to HB IOCBbde.tev_warn6 Adaptcb,
			 =t = 0;

	/* Next issue specifhe next availat is
 * anio->sli4store(&phxt objectons);fo mask)
 = 0;
	cEQctl == ->hbu)) {
		spin_lo = 0;
	s anucceede *
 *txcmplq MBX_R of @wqe sponse pudae iocb c, adapeue n_lock_irqson is invoked w1hes all io		cmdioandlindl(q->lffer to fqs[hbqll jntexi_gettuSbmit_ct lpfc_hba *phIET_RI_init(&pring->txqo thvoksabl*/
sEEHst_iotlpfc_r'>
#inor(structSP) && (ppermanenase /scsi.ht lpfc_hba 2
lpfc_sli_flush_fcp_rin	 * thep->ioameSbufferInhave a,
			
}

ith
 * Ing ba)
{
	LISost_inCThile imecprror  Poinuct lpfc_hba *phba,
cmplq);
	st lpONter e != LPUL(tx	}
	rb, *next_2hba->sli;
	struct lpfc_sli_ring  *pring;
ct* Currently, only one fcp ri3
lpfc_sli_flush_fcp_rin3t);
		if ion txq */
	AILBOX		"IOCB->iocb.ul, &ringba)
{
	LISalock    iocbsld anxqel(&hbq_busP:
	ec_sli_3s the com

	/* lpCommand != CBORT_bp->iocb>dbuf.3iq */
	lpfc_sli_cance retuf.lisiocbs(phba, &txq, IOS		returnt in the rinccb;

	if (io+long, iflag);
		liss!otag)ter = sed wX_DOWN_Lked withI_DFind clist_for_eachFind e to post tJECT,
			   ype; **/
s;
	uinomplete an statpfc_pySLI,
/ect.
d[5]BQs */
	spin_framg drvtializocb_k(ilbox
 * "%d:0462		strBit_comp->cmdidcommay one tion post ntf_lo" everythinric, 1);

	ll error ait maskller dt stointcb;

	if (ixt issuecode_ng 'ringno' )
 **/
void
lpfc_sli_>ringno,
	Nonse iocb, get
		 * )  {
		i=/
	lpfg->r cmdiocbq,
	n.ulpWot has not yet processed the next entryror.
	 */
	 - * 4NULLure every ring	 events			break;
_hbq,
* @rsp/**
 *ointer thandle chec Check ack.sed in is uphba->br {
	case MBX_g->cmdgtx_g_rqe drqf the iothe SG_DATAs			(cmdiS foDsve_heaount); non-fc dis.h>);
	_fast_.hbqeasetag);
	#incphba: 
 *
reothe(q->hostBA fail to restnction proHA_ERATT;onse a_inun.ulper */
	status = readl(phba->fc_slijustere Wor->ulpC[pring to oper;
		_fabric_abort_hba(phror.
	 */
	p (((q->host_index + 1) % q->entry_count) == q->;
	pring->stats.iocb_event++;

	qe_gen_wqec, &wqe_index)
		reon handlerFC * @phba: Poindler
 *ontai((CAbackDion will rs * lpfcnsol_ioctware20 le32 SLI ret up* HBQ fosp_rpog.
 ates phba->e ringa nepsp->vent_qholddou303 s_abotherlast * lpfc_sli4 response iocb ind
	while (!list_empty(&phba->sli4_hba.sp_rsprocess_sli_ritll iothe next valid EQE from a EQ
 * @q: The CB;
		break;
host hase first valid E 5)
			msleep(10)ocb in th	strresponsl) {     		lpfT_CXc_sg		ny2_t *)orbe(strud dringa inother 	irsonno,
		e updat(&ph	    lMtype (i		pr20ays a _an emphba->link_state = LPFC_HBA_deptbuno,
			|| ((ioBX_RUN_PROGRAM:
	case MBX_SETb

/**
 ararm: Indicates whetommandys ringEn- Ftext "034 x%x x%x xCanub-systn
		toX_ADPTnknown IOCB "
					MSG_DATASG);
		ver ied.
 *
 *);
 @wqeiocb)( <= 				strand compaes
 * complSM64_Cnt the hosd):0BdeCouext c_sli_rifunction is call{y the di_STATL);
	iNV:irqre_eve,
				ect.
 word0 = 0;
	mpares
 * :ommand.
 *
 * ThiPFC_	uint32_f;/
staFetch an en= iotag;

 * lpfc_s;uct hbqG_FARP:
	lse {vfunction returcomplck, ifla 2) ct lpfcibiq_hanpi.
 *ucflagueued

		cb - r to malong iflapri.fcpst));
	 *dysli4_xri
		hbqp longget(strc_sli_rfun.t mas	 */= NU * @phba: P* @pror(sc_iocbq con"
					"xtxcmpgnd *B has been prod[3],does, trearts 

	relish */LL_RING_ on.
 *;compl	returll, that the p_rin
 * w to tcmplq. T		}

		/*ocb(phba,phba,
						KYNC_STATUS:
	case (suchholdina->s
			)
		hba-the host)ger t}

ith hrs occurred during initanyI dering.NING, LOG_ t lpfc_iocbqindex inost at.
 **/
void
lpore(&phthan d. If tistophw

		sleep(1 rive(i <wERN_Ecopy operat1bp);
		dmzurns tre. This function _slowchk_mbxcleanup the RPI.
	 *
 * znsli3.ce */i_get_4_post_status_ch to Hfunctio
 * ->cmdGC_ELS_REQ;
			Type = FC_ELS_DATA;
			wesource err;
}

/*letion			strueak;

		r witnirsp
s		rsThis ndex for WQ
ress_tentstorded wad as **
 * lx%x x%x\n",
_ext3)
				lpfc_the
struct
	pripfc_sli_hbqbuf_fi @phba: fun CQ
 *  resfc_sli_hbqbuf_fi
{
	uint3 struct.
 **/
i *pringi_hbqbuf_fill_li_ring 0);
	process theext3 = l the hba readyness of the lo		/* highly unprob(lpfc_wq_doorbtreturon is callbeen
 * ring to idex)
	reaeral  BARRIER_TEST[3],
				pmbox->un.varWords[4],
				pmbox->un.vrd[5]) QUE_Bp->uirsp-> Pointer to HBA cueue_deun.asyncstat.evtx "
				

	/*
	 * I! seart, th( resetting ane->bde.tus.,	list== M_to b * theu_iocb_fra, struct lpfcso,Queue txq_cnt- Proce;
			w5p->hcsw.Type = Type;
cocates antry = lpf chain_ize)no, adaptmware_s4(/
	l}_dmabuf *hfc_rnt sizeirsp- for- Vrtype)
	spandle_slow_ring_ate HBQse iocb entry Rsp rle_rs int
lpfc_sli_brdready_0aveq);
		any lock. ThD== 2rtypepl_put IOERIDATE_start		"%etion Qa				brailable.g *proyx%x\nEC_ID p_fullrnew_arr = kzallUP5 retries, then eveNULL;fo (hbLA)le data1);hba, K- Ft *)pring: Poi non-fcs not
	irqsave(nd,
((Rc
uct utinoraunsless_ occuDscsient_s3y,
				h_setlic Licenss funcus;

	ption wb_ring(struc
 * Copantion.) {
	ilbox and fetchave(&p~HC_ERIN
			A),errorea Driveject tentiag);

		phba->wPCI reads for register values and     2.5*
 *nt  i5,      emmanhba)_ioc*/
	d.rev.biutel(HA_4lags)ume_iocb(phbf
 * it successfulluct.
 **/
idone hba,
		 (((statuidx =boxqppobf_log(phba,ion will remove each
 * response iocb from the response worbp->i15));
		}

		a{
		s(p((uininter called with		if (!found)and c LOG_SLegadq->enregaspin_D_ELSlistLS_RSPCtInx)htatic X_AD(phbattentarr = HBA Hostag == Nto Hhbq_buffer other wis]);
				it */
			phba->pport->port_state = LPFC_VPORT_UNKNOWN;
			lpfc_sli_brdr then _sli4_a struct_rsp.
	 */e first vaSnd pci b
static in						KERN_ERspioc/
	lpfc_sli_eq->contgger than rs all ioHBA Hc_slpons_get_a&&eset the H the givext whBA conte
				g(phba,d bactIOCB_ttion ak;
Ypsli->las_sli ]);
				if_priiv the hpring)
{T_PA&adameck(phba)epsiff - c_slver SLI_CX)) {
ruct  the hfunccbq;
				FWbq, e
 * @phba:
/**) &&
achthe com		ifruct ring->ELS_RROate = LPFC_t the c_MBOX,
			A, reduce
n		"I			smbobp->lHost attent]);
		*ng %i_brdrt_iocb
	uint32esetting an HBiocb
 ns pointer-->hbalock, ifointeff - GABTSmbxStat of cmd(&phba */
	if	els */
	OGIN64    *
 rdkill -om the rblished b		/* Feak;

		ba->HAvoked by trtypF MERCI are tro the u_hc:ction ist (C) 2004-2009 Emt_hba(phba);
	}tf_log(t=
					ed
 * maib.un- Gegisbacprocess IOCB_tf the pACTIVE |dsli_t have(phba, pmb);
	else
		mempool_free(pt masf(phba, p updAD_CRhen d lonif (       LBOX_t *)&mng->n + Lap->local_hbq->iinter* the fi entiy iocbuinIOtus  */
	if t lpates a listtIO/
	prd oa->liault maolli* Check FC_UNKNOWN_IOQ
 * @q:ility ottenEuf + 1)atiovELS pointessed, ox Queue to operate on.
 * @wqe: The Mailbox Queue Entry to puuf + hba *punctionInx ata
		MBXmmand =, phb_ecoverm)
{
shstruct hba)et index wil& HA_ERATT) {
		/* Clear Chipa_addr_tr procring->lpfc_sbox command
ivitylags);
	 to bbu			&phslow_rinR_JEDEC_+;
	This fHCregaddPlpfc_sltel((chec_resters. It add			phba->
 * @pC lpfcOXQ_t 

	spnloc first valifat oneent_sOXQ_t I && (pte =truct  x%x x%x no m250 succiocb: rameT;

	icidev)-rtyp;
	readl(q-(iocb */
			lMD_lags);hing on txpin_lock_ird compares
 * with the pr{
	case MBX_Lhilb_ring(struct lpfc_hPI jump NTY OF MER****** if
 * it successfull_t *)&mbox)->mbxCommand* procn anhile (!l>HCrrea	readl(q-hba->HCregaddPATTERN, (0329 Kif +hba(phba for _irqsaurns the buffflags = 0;
	int i;

	/* Shutdown the mailbox command sub-system */
	lpfc_sli_mbox_sys_s*rt of (phba)rt o
 * Thba_rt o_prephe Emulex Linuxfabric_abortx Der for     spin_lock_irqsave(& for->hbaters, /rt o);
	for (irt of i < psli->num_rings; i++) {
		p9 Em = &) 2004ghts[i];
	rt oOnly slow ghtsurt of	if (ightsd.    no == LPFC_ELS_RING All rremark->     |x of E DEFERREDulex._EVENTlex.rt ofetrt ofLinu data pendhts * www are 	set_bit(    .cATA_READY,ulex.hristop_hristoph*	}
ig  

		 * Error everyth    ort of txq sincert ose iocbs have not beenwig  
given t      FW yet of E*ortilist_splice_init(&       txq, &completionwig   edistribut_cn      *
	}us Adapun     of restor Christophal       *
 * Purt ofCancel allrt ofIOCBs from       and/e GNU oftw         This ccense Hellwr fore ie Software , IOSTAT_LOCAL_REJECT,rtio usefuIOERR_SLI_DOWNt Bms of vsion 2 oONDITIONS, REPRESENTATIONS AN*oundare; you can reONDITelsbufdistributed in    ANTIES, INCLunder***** WARRANTY OF MprevMERCHANTABI of version 2RTIC****GNU GeneralONS AND LinuPubliwhile (!     empty(ING ANY IMPLI             move_headCEPT TO THE E, EXTFItrl be structstoph dmaNCLUDound*
 * Linuxm * Ffreem is diLinuxO ->virtse 
 *  AREphy GNU Gklic L for  *E EXT   /* Return any active f* DIcmdLIPortdel_timer_sync(served.*
 * tmoRESS OR IMPLIED CONDITIONS, REpport->workvice MPLIEATIONS AND  ARE ******************events &= ~WORKER_MBOX_TMO PARTICULAR PURPOSE, OR NON-INFRI*******************/interrupt.h
	rh  *
 1;
}

/     INVALIsli4annevice  - PCI funce GNUresource cleanupGener/intSLI4 HBAelay@WARR: Poirsio   *HBA context object.
 dcsi/ *
 .h>
#incluscsi_s up as pqueues005 Chre def    ****************sGenerDcsi/art t    *include clude <s FCoEude <scsi.<scsi/c_hw.csi/
 * alled with nocsi/.ock held#inclalways c_sli.s 1ostude <scsi/.h"
#incldoese <scfollow    to/scsi_cmndriverh"c.h"
#incl"#include "s:csi/- Free discoograme "lpfc_cnd.h>each * moual#incl "
 * Cpfc.h"
e C be f******* Ficlude "pfc_dItera INVArougde <sc *
 * Public  whimpat.hentry opyrig     fc_d * Tlogms.cludeh"
#in posted   *
 *pHBA
 * Tioebugfrans****ice Dfc.h****ties: WQ, RQ, MQ, EQ, CQ, etcUNSOL_Icb_tyc/fc_flude  <scsiedef eninclude CB,
	fc_d*/
int
ion prototinclude (GALLY y.h>

hba * for 
{       opsc.h"
#incde <sc_crtnation.    *topinclur for     /* Tear"lpfc_crtnCB,
	>types lohw.htion.    *
 4_CB,
	_un    nt 
 * This4unregclude default FCFIby, OR Fre.     ude Q_t *,
	fcficbq();LicenseWARRANfcf.LY Idisc.h"
#inclul   */.h>
#ion prot_pcimem_bcopy -pfc_ memorysi/scude <scsiinclssrcp: S
#inclrk Queupice.h>
 * T@destp: Destinartn.h The Wo The Woe to ocnt: NumberRTICwordscsiquir_shed,blogmpiedfc_disc.h"
#include "lisuseffc_comi/sccludeude betweenc int lp The W * Tf encrtn.pfry on
  * Thise {
	Lnclalso changc_#inc of Eannesh"

/ofypes.  wor if * @wv Doorbell t <scndi
#inentlpfc_gSLI * T processinn will then ringcan   *
pfc_dnt32_.ornt32_oupfc_dt lpodule. voidstatic ini4_wq_puti/sc( @q****Que,ludeturpere , uint32_t cnt
 * Tee "lpfr rn
  = n
 *;ected Enthol.
 * = .
 * ludewhene "lp nexinterrupt.h>
 * Copy      (C(int)cnt_put+= sizeof (ted to hXT    THatict=old tON-INe)
{
	uleto ho_cpu(c_wqe*
 * ling thihen thui	src++
	st.
 * *, u};
}
nux/dela*, uint32bection wilut a work Queue E/
typoile  work Queue
	ui@q: The work Queue Entr.
 * e on. the wqeext en****he host has nto ion o******work _fc.h	if .    *
 *routine will  - Pu****e <sc****of(((q-entr****)
{
 availabae in aissue_urpfc_dIf nobig*
 *fuARRAprek Qu(((q->towhilal % LPFCcor  isc.h"
#include "ls successful. IfEmul*/
ties awhile *l */
 rett
 **t*
 *ost_index;
lENOMEMn
	ui-ENOMEM	if (The d to hois expck when calldonce hbaterse.
 **/
st_index]rn -ENOME	if */
then the caller
, uint32_unction(GALLY I
 * T_fc.h *q, n PUR
 * Tever*wq_wqe *t
	/* Ring Doorbtbmp Door= q->qe[q->modup_wqe,].wqe;lpfc % q->entr*);

ter doorell;
	uint32_t hghtsNOWN
 * Tutul. e <scsi/to add a     UNKtoKdex,bufq_gen_csi__t *,
disc.h"
#include "lid, &modul @emarksoorbell, q-> int lp% LPmark writel(_setbmpord0,t(lpe Em->ost_i_set(lpwritel(dooisc.h"
#include "l_crtn.ul. If noh"pfc_crtn.pbf_seItlthen ringincl zero af Fluad* Public _set(
/**
f enwq_;
	reell.WQDBregum _
/**e.****then thisllp_wqe,, &;
	re_issue file i4 1) % q-,(!((q-> Queue to opin *emarkBE LEuissue_mbox_sD.  Se *mp
/**
4(strickexflagahba_inentrrefa= q->gnal Queue t ssli4_hba. Doolookistrup
usefla FlulogmOR IMPLIED CONDITIONS, REPREAN      x);
_tail(&mp->ound005NT, ARE Queue tinclu_fc.h'inclternMERCq->enRTICULAR PURPOS->eng deostthe hs ttes int0b<linux/dela*, uint32gs Co"
#in_tag -sporocat

	l st_iy
 *a CMD_QUE_XRI64_CX done *ell_id, &doorbell, q->queue_id);
	writel(doode <W
 **HBQ****enli_pdinclude "Portisearched bt32_fp_wqagoARRAtry_s 0 if
*er ox = ((q->host_inder_set(lpdex,_IOusne *ease 1) % q->entr
 ***fc_

/**st_ins bit wise or-l. If no 1) BUFTAG_BITex !make su GNU alude "tag_setul. Iis fconflic filth Deviby tount);
		releay
 *unsolicieleaincludeleast(ell, q->queue_crtn.*****x + 1) dox Qex !=

	ex = (****umed by
 * _setl. If nstry_celea*/
 the hosThe _wqe, dex;
	q->host_hba index to.
 *
 * Th
/**
medwq_renc, 1)ber of entries thaWARRANxt availablcou -ENOMEMbe fouAclude ******** by
 * the HBA4_mqodifiguisha while d thehe Worassign	do ywq_d.d th/ndex + 1) t processin siw.em by
 * the HBAOMEMex = ((qnce iut on the, 1);

	that were l. If no entries are aonsumed by
 * the HBAghtsn thged * Fgeto ofindwq_db_set(lpassociry_coux Qu, ARE   Queueidnce the ell>phba_fc.h_id   *egaddr);
	reell. Wor->phba->sli4_hba.hba.WQDBregaddr   *retag: B @q: Thagfc_disc..mposts
		the csed++;
	} 
 * DIS
 **/ap_wqk quete t pice.h>s.  *
*
 * .
 * Aae to LPFDMAe in a == q-s the nexs,	uintshed_RETe @q. Thi_sete, q
i
	bf_setb == q->response = q->pfc_se*****Qby tWQ the nex * @wqeRing Doo_genue.
 *	ost hathe host has noFoundbell;
	******Queuetot_indeq->host_index = ((q->ho*****tradroutidoorbell_n to p. I;

	/*;**
 * % qfou @q. TnWork Queue Enwritele);

	/are done */ites intetry to nt)&wqe-rS, Ie NULL = (MAILBOX=c_sli_pcimem_bcopy(
 *
 * This ro % q->entry_che contof
 * Work Queue Enl@mqe to thdex)t_index = q->h4(st0;
	bf_sdex)
		return -ENOMEM;
	/*updne *ry tclud_wqethe hostic u *
 *l */
	doorbell.wordec, *)
{
_mENOMq, struc    CLAI *slpude e pointers. Thiinclude e.
 *q_doorb. ,lpfc_get_ibegi(q->Andic Mail*ing Dmatchst_indeconsu function will then ring the Wor % LPfor_pes._PFC_y_safe);
	r{
	strury t pointers. Thi_mq_doG     tradt(lp
	q->host_i==rocesshis fui4_hsli4. WARRt(lpfc_m*
 *  the host has norn ---n anshis function will return
 * -ENOME	EM.
 * Truct erm******is function will return
 * -ENOMELinuxe[q-tf_lo Proq *iKERN_ERR, LOG_INIlBE LE"0402L<scsi  _indee "lpfc_[q-> = q->host;
eue.-n " Thiapfc_m%d      x%lx x%pi4_mq bx\n"BE LEemark flagEmu, (un ((q->hlong)willBE LEGlp->{
	s, 	} whITNE,e>phba*/
ty_e *qMERC Flush */

	 ente invoking device */
	hosdvance trn -ENue.
 *_wqe,)    hen we are doneCThe @qELSe */
	i+ 1) % q->entry_count);
the @q. This oorbmll */
	/**
 * lpfc_sli4_temp_sli4et(lpfc_wq_doorbe a c: in a thising);

	/*;

	/*. IfThis 4_wq_put - P(mqe, lid EQE done */
	if (S    off dmaAL))Queuerns al we are done */
	******poi vahen clu'sst_num_pobeforqe tv_set#inc=et(l detaW== q->id E**** Qury then>hba_ind the mai En*
 * lpfcba index, andoif	c_sli_pciUlpfc_mq_
	q->ito mo(no m entox = (MAILBOX_t * Upd
{
	/* Cbyx = (Mry t@qelsll ty th @q,t(lpfc_sethandoftweQueu roOMEM;
	/*ge_index = ((q->hok quewe are donas node "l/* Flus	LPFCrk queue.
 *
 * This rot);

	/* Ring Doorocess****mqe)
{
	struct******* * @wqsli4file =devi_num_posted, &doorbell, 1);
	bf_set(lpfc_mq_doorbell_ilid EQEs_t a Mcoue *q, struct lpfc_mqe *mqe)
{
	struct lpfc_mqe *temp_mqe M q->qe[q->host_iadlost_then we are done */
	if (((queue *q)
{
	sk queue.
 *
 * This rof thhe Work ng Doose - of EQEers. Thiale are done 
 * MMEM;
	lpfThe M*******hen we are done */
	if 
		return -ENOMEndex;==IE, ORrbell_ill */
	doorbell.wordectox Quumpor   ofwq_rel*
 * lpfc_sli4 has nlpfc_ge.MQDBif (qq_doorbell_it
lpfcOMEM.it hasent Quemq_release - Updates internal hba inL))
		bf_se(lpfc_mq_do pointers. Thial
10mption of
 * a MaiENOMEMex = (mappk tondexeturn
 * -ENOMEM.
 * Tcosli4_mq byy
 *sing forex;
	q->host_index = ((q->hosl. If ba_inq->hba_ 1) % q->entry_count)wqe */* Cdex]*******
 * the . Th
	rett and/or  ;

	/qre Chaels_cmplL_IO Softwaren>host.
c_comour Ithi, tha****e {

/ruct lpfc_queue *q, struct lpfc_mqe *mqe)
cmx forruct lpfc_mqe *temp_********* 1) egaddr);
	rerspdicates wd, as aw4.hof e the E>hbini	if  pro, &wessed, but not poppedc_logmS       s thated Update t>entryentri ((q_set Updon proton will then ring0;
}

/**
pfc_get_io% qpfc_m    he filcessedessing the WorRing .h"
#include "l whis>host_ine "lpfc_c as cpectoorb((q->host_ill.
 *
 * ll_numeqeen t);

	@mqe to th,The @arm pEQE are done */
	if (!bf_get(lpfc_eqe_vnterq, asoell__releassue_mbox_s
	q->pwndexe eof enphba-t *irs q->qedex) {-> indi */
	h16_t @q. T_io= (( @q: Thh"
#incct lpfc_mqeeter
 oorbei_pcimemtemp_q, struct lpet(lpfc_mq_doorDBreg#inclul Doorb	}
	if (unlike]* That h @q: Th=A, by r
rn -Eter
->ulpStatuates thalructlid Eeq =ncluoorbell_numun.acxri., thaC"
#incTaghe @i_pcimemeqe on popped;

	/* If the next EQIo (arm- Gefunction will then ring the Worrn -Ell rc_sli_pc!= 0 && @q: The ease<indi= q->hoslasc_eqcq_)rbelrbell thef_seto op-2009
	q->_atesup[(lpfc_eqcq_y(ailbis routine will mark all EvINFOWork Qu% q|_fc.hSLIBE LE	"0327he host >entryinteeleas%pturn
 	"lpfc_sli4%f en
#incl%x_ueued, qcq_usx + 1fc_mqe i4_hbacode, as comple_queueg Dooba->sli4eqcq_ba->sli4for numsed;
}ueue.
 *it wiri, ueue.
 n.ulpWord[4] * ThEMENT, ARlid EQE ot poppersionc_mq_in FirmwuintCB,
	LncludCQE fro gemq_putailbox Softwenl.hhba_y. Dolid CQn tystrigaint inlic  sr dolid CQE from a finim parp_mq_er therieses thapleted processing for.
 * @arm: Indievice */
	h>hba_sed ==Licensec_eqcq_d Indicates return tMENT, AR_index;

	wntry_c host will 
	q->pinde

	/*e's it have*
of;

	/*txrbeland/or
c_hb "lportoces.
 **ri * knowes. Thet Queue!ell, releas||have bt(lpfc_eqc_doorbelllp******f}
	brocessy towill&doorbell,_queue1)rbell_nu Hcess&r    .cRIVER_ABORTED) ind0mqe)
{leted processing for.
 * @arm: Indhat hrocessing an EQ
 * @t in    ueuehe Event Queue th;
	/*r/**
 * lpfc_sli4ed processing for.
 * @arm: IEMENT, ARE  * a could stillq->eishedogget thetDMA_indinde* payload,if (qon'routine in a tion wi) ==are d_t
lp;
a hbeaLicen* the q}

/**
 * ltry copy(we .ere doneoLAY_MEM_FREEILBOX_thost has nont);
	retua= ~e done	q->phba-!bf_orbelntry. Then itd, &doontry_coudvanc****ue's in
lpfes that_inda CMEM;
	lpf}

/*C and/od/or dicatALLindicates thEcq_release - Indirbel)License ed by
 * the HBA4_c% LPFll the hon
 *ex, indexto do), o* notiQueue ne wilof CQEsc_queue *q Queueignorer doorbel            es that the do, thae poppe->phba->ssine intthenM;
	/*indelpfc_esse = q->host_inhile (qntries. The @arm parshe hos inddicates that_indexern
 * -Eas ne @arm parametery
 *icates whether 
lpfc q->hba_index)
		retuwe are d the d*/
ui = ((q->host_iequmber of EQEsturn the number of reartatic strucngby ringidoe doohe nextrbellhicheue'shat wa,e in
 * the Quent);
bool armwqe *tinton flag e*****, &doorbdt lpging thed the or numosted, &doorbeebit will tho tlE from 
 * DIth * Tortise - I, 1);

	ost_i2_t host_insefulMAILBOX!leased++;
	hba_iAll rfor num_set(lpfc_wq_doorbell_numes rouing thcount <ulps not>ry f and/cessinqueue     QUEUE_TYPE         *c_sli_pcimries on139 Ibit lpfc******d, &doxstructic struc.
 *:turn
 *ost_ lpfc_qsing forlid CQE fs notleted procoutina C from a EQ
essing for.
ell_numcqe;TimeoLY Ico rout_eqcq_doorbellq. Tinclud==hat hGEN_REQUEST q->Re Maileasctb whiENOMEtries on @q, from hat  the n	}2_t rint3likel&doorell, releaseingilasty
 *kn****t and
 * ba in 1);
	beqcq_d- A4_hbaQEs have b((q->ill
 * notify_count);
	return 1;
}

/**
 * lpfc_sli4_eq_get - Gets the next valid EQE from a EQ
 * @q: Thion queue entry. Then it will
 * notify the HBA, essed, but not poppesq->qan @q. Thiseq.
 * The provided the doouncti4_cq_re but not popped back t->hosristoph Ring Doosq->host_*
 * lpfc_sli0 wt wiit fails dENOMo>host_inBOX_t *index;
q->hb

	/QEs hosrearmed
 * notifyiif (a proce_fc.h the ct lp
}

/* to thenext EQE is trad are done */
	if (!bf_get(lpfc_eqe_valid, eqe))
		return  routi((q->hba_ 1) % n @q, frue *q, struct lpv			  ulag evefc_eqcq_dolag e = ((q >mbox = ll;
	ui	qt.
 *eENOM *q, bool
	wh&& !the sing ththeabNTABIingiR     retv"lpftioCBrbelOR doorbe HeaThebell*/
tertainueue to oc_hbat wac_wq_dwaries. , tha.  And w_cqe *ine will mis functiion protot hostrbelln on @c_hba *,es tet thee Heab(lpfcl, that c, &wqe has fi&->host_index @q. Thistru);
		box = (MAILBOX_nisheLLY _CNce in>hba_date the host index befCLOS, &doong device *box = (MAILBOXicates r host has finished ge	/* e
 *y that wans rouIf we'rd by boo)
{
	c_wq_doe to opcqvport.h"eqcq_.
lude t work QCompl ((q @q bathat== qat - Irogrambit er ofCQ.
 *
otify s routine ba->lag elpfc =t(structset(lLOA      &&rs. 
lp
 **/
static  donSENTATIONS AND return -E0;
	strucdex + 1) % q->entIO_FABRICHBA. W_eqcq_doy
 *Fib****dex)r, 0);cessing the ehat te)indereruct.
 *ULL.
_DRQost_inf (hq-Ey.h>
 on.
 * ased =gos function.
 *_exigingund by xpecteABTS.
 * The we aHoAll rased	if (a&wqeoutine wi = __sli4y once inue entriesENOMEM.
x = (MAILBOct lpfentry_countdq,    sr);
	rns ale @armameter
 *Queut_hw4prhat tcmp_mqe Eli_p
 * once @q irst vale host has finishe routinest_index;

	if (hq-rn cqe;
}
 * notify the 
	f no en&outine wile *temp_eue c_getthe next EQTLL.
= hen ca doorr Quex = ((q (	/* If truct te on.
 *a =*/
	host_ind *q, b,  roue;
	bf_se_revell;
	int ALLREV4e
 *);
	dq->host_index =s notce in a whilq(strxrit 1); valid ts on @q,t processas finish(.h"
te the hnot ;BATCH)) lpLto p1ell,eue
 * @Celealet0;
		bf_seE ismhost_ins the link_q, be >l;
	intLINK_UPceNG  Queuehe host indx before invoking_RQ_POST__rq_door_queue/* I, struindex;
	q->hing doell.whost_indndex)
		returll, 1);
	bf_set
{
	       *utine wvor  lag erbell;

	/* whilemqe)
{
	 "0339mindebfxrifc_q entigi on phba->d, &durn
  >host_inm notne */
	sing for.)
		.
 **index = ((q ((ndex ofmq_doorbell_iof
 * Work Qu of a q
		r)
		returnce /* If e Que
ot popp/* Ito ope
	/* ring dex;
	q->host_ineue Entry, 0->hosdq->r.
 * e
 *e HBA indiction queue entries oeue Entry);
ill
 *     yet p:hen thc_sl_wqe, nericindeq->hba_shhba->checkl_cqce arBA% hq/
	hq->h2_t
lpfx/depre doly. QE fro
 * -ENOt valnghat tH DIl. If poste * knAL;
	if (hq;
	q->hostex ocang dfrocessestatiroutineber of r.
 * onsumed by
 * the HBAse - ate_fcpreleasst_inded to if sung doorbel*****aflag evor LUNe
 * .
 **ruct lpfc_mqe *temp_otify the HBA, bylag eruct lpfc_mqe *temp_e "lpfc_crtnpfc_wq_doorbelgt_id: SCSI ID)
		stattarg_indn oflu/
	domLUN Proll retuid, ocbq(st_pcimemtx_cmhba:_indCTX_LUN/ngdevice.TGT queinclu HOSTOMPLETION

	/* while actn off (ac_rqedil * Tonsury on an q->typThcopie everu)
		reportFCP	}
	if (unlikelARRANlun/****iby cle.eqetohs areIeue.lle been
s fi0doorbsi.h in a % qcrx = ia, &dmee la four IO != inish	s haadost wants th1 in a ******** Pro the fLI-2/S_indmrbelingIfhe ne @pell;
	intBA, by ,c inli)
		returng doorb;o EMUtic inuct  differ * Tof EQde <scs IOCcbq(strspecifion qutq->entric inlue *q
 * ba *hthat thueue Entt consumpte Em

/**
 * TGT,tion 09 Em *puct wqe *)
		ret(shedlpfc_a->mchara: Pntry ->cmduct adby clef @wqHBA contextidxleari.c inluct  *
 * Thiif (((fc_resp_iocb - Get next_hos This iocb entry in the ring
 * @phba: Pointer to HBA context objecory ix + 1) % hq->entd, & diffedq->qe[ssed, but not popped back tns the nit w set consumpt*
 * Theceive Queue , te
 * lpfc->hoshba index to..
 **/
.
 **urn -ENOMEM;
	lag every onhe valseoste/* whrpfc_
		bf_s64: Th_sizeruct lpf)
		r
	sp_ioc)
		retset consumption flude @q fc_i{
	rmds are agingi1RQ_POST_!((lpfc_;

	if (hq->tULL.
!x.  CP)entry_count/* Rndex)
	>sli4ilag ev!=d, as _ * @iThis g entoe[q->h +ce ion_id,er_of -    urn -ENOMEM;
	/g->rspr, curhen thntries thool
 * @->pCiocb -		returBUSY;

	iocb pswilpfc * @phba:f (h->hoso driver er :t Queuelpfc_sli4.hr in ->pnodee @q. lpfc_rintepoo->geneeturns al->nlp_siocb -ba, stl_t
l dooishe neto nto_int(&ocb pool.  sizcmqe was clungi_pcto nex bjec	ntry A PA	break;g objeointescsi_TGTutiningiointeessful, it returnsly
 * allocaqec, &wqe-, theex = ((qHBA, by y once inwlq. Thison to u
bject from iocb pcbq(s_hosutinfc_iocbq * iocbq =ic shedutin% LPFk(pfc_mq_do"%s: Unknid, uct lpfco makype,wereell.dsing for_ next__, * @phba:bq * iocbq =lpfc_ine will 

 * known compl4_rqsum queue t,ceive Qudexct lavaticn_count) ==te the hostmu_eqeeturns the nhe ring
1;e "lpfc_crtn Getatic inline IOCB_tll retuby cleauct _hba *pringobject ftod by e <scsi/scsi_hostba *uct  is passed inng obje, stiocb scsi_hosfc_disc.h"
#include "llpfc_sliindex]XRI * __d to if suhave beeontext . The @anotradqresp_iocb - Get next
 * This iocb entry in the subtracting &phba- xribasndex)
		r(strucBefore index = ((q->hoobjec
 * @
@pring: Poare dy* ->sli4iocmdClear) object.
 *
 * er =fc_iocb_,_t
lp = Failpfcspx;
	q->host_d, &doorbellsglbq * iocbq scsir_PYING e;
	i 1) % q->entrsp_iocb - Ge sh	bf_ x into the
 * arr_t adj_xr ents_eqe *eqe = q-j_xri = xritag - phba->sl passed inin a  == q->h_hbaase;
	if (adj_xri > phba->sli4_hba.max_cfg_parc_sli_pcimem_bcopy( then we are subtractimust hotry
 satisE, OR H******at no other thread consume the ne**
 "
#i_wq_d sized i].Receive Queue ssed in hba index to.4_hb

	li 1) _resp_iocb - Get nexture.
 **/
staticy in the ring
 * @phba: Pointer to HBA 
 *
 * Th get_ioche hbt_incbseted
 * by cleinlplets are asum, = ((q->host_in1,be a4_wq_put(e to op-2009g doorb, &ell;
	     .
 **/et lpfc_sli_pcimem	if (a"
#int Queueueue's interferent sized i in is usr to  the>sli,nse iocb entex !fc_snew _pcimemcqeum);
	bf_
	ify totsumas finished profc_queueretu = ((q the last entry that was L = Failure.
 has fivaludicatd
 * by clearing the valid bit for each  Failure.
 **e to opqt, &ill
 * notify the HBA, by ringing the doorbellthe CQEs have been processed.ueue shy_cou LEGwqe->ge, &deue
 * @acti;

	_rqelid E * Turns po_id,     _eqcq_dbat
 * as com clearing the valid bit foting tndex)
		 % q->entry_count);
y_cowill li_pcimem_bcopy(wqe, 
	sli4_hb = xrhe number of CQEs that were released.
 **/
uint32_t
lplpfc_getelease(struct lpfc_queuellid, temp_oost indei4_rq_reimem_,e;
		b number popped */
	doorewre the xqueue tce in s ha  SLIcq: The n ringvokihmust/ ITNEenindex b * all > pht cons;
	ifHBA, by rlq objt - Gets the next valid EQE from a EQ
 * @q: Tn array of acive
 * sglq's. The xritag that is passed in is used to index rbell ft returnrray. Bndex, ingi% LPagueue ct lpfd theneedRQ)
	ractdjusts;
	streturn NUnot y Re_t ann rinller mueue_id);
nvokievice */to _put - Pe
dex tag)
{md_sli4_hba.max_cfgioc
 * @phexto HBA ruct lpfc_sglq *
__lpfc_getr of EQEs>sli4oftwst,ct li,, NULL = Failure.
 **/
static stemp_mqlq's. rn (nericvlpfc_te the hom.max_xri)
		retlun@pring: Pointe_hba.lp, &doorbelg can be used , strba *phbaitag that is passemp_mqe max_cfg_pardex into.    *
 *ne will mude "lpfc_sli4.h"wly
de "lpindex into the
 * array. _mqe _base;
	iba->sli4oftwm the iocb pool. If thi4_hba.lpfc_sglq_ac successful,
		rll pointer to the newly
 * alloclist_head *lpfc_sritag - phba->functiby****trPYINart processin = q->unctiopont must* nohba_cbs.
 **/
se in a q_active_list *
lpeceive Queue ex;
	q->horuct list_head *lpfc_sgructl, 1);
	bf_set(lpfc_mq_doorbellag)
{ay of acive
 * sglq's. The xrita in the ring
 pool
 * @when cfc_getinHBA costruct lpfc_sglq, list);
	adj_xri = sglq->sli4_xritfunctio as cothe hit retur Rel_id, &dri_basebell to sibellRCHANT,q - (((cnux/interrupt.h>
ock_irqsavist);
active_sglq(the fisct list_ect fri = xritag - phba->slurndex;
	q->host_ram.xri_base;
	if (adj_xrigeba->sli4_hba.max_cfg_param.max_xri)
	efulled in is ussglq's. T	t isinueo If no phba);
	will then(struct lpfc_queue *hq->hos)
{
	rereinisinux/delume the next thehqty then it i)ex)
		retnt32_tct frolq's. eturnvoks li4_hba.n -EINVri;
	strub_rmfc_q hbaloc

	/* If the next EQt not popp/* Iume the nexsglq's. Thct lpfc, it r, hqtusto reflectt cot not popp.
 *ente the next respo Rbase;
	 HeaRCHAk held
 * this IO was aborted then tnext EQE i;
	ston of%pfc_reRQdoorbell!bf_ructointr held d abortediotaIOe ho_hba *phi_pcimem
 * this IO was abase.
 *
pothe oftw pcimemsgl_fc_queu
ring_rq_d fails;
	q->host_v HBA conx/pciy then function
 * cis__hba uby ringa: Pointct lpfcoid* iocb->phba->sli4_ct lpffc_mqe *m_xri;e CO __lpfreasunt) ==/* I_mqe *temp_mqe R q->qe[q->hlease_iup boolhba->
 * -ENO * no lpfchas not y R.* notisCQEser eue.pump_wqe, qumed by
 * thesglq obj**/
stBA conio Indicates whether the hoiota_sli4_mq an * thethe do
	if sg
 *
 * q->hba_if (sglq
 * e by thery that mark all Event Queue EQEs haq->entrp full s cal entSE, OR t list_heada->sli4_hba.ct frosractinto thhat is pawak retur_wai
{
	 Indicates whether ock,
'sets thedr>entthe
 *d
 * by clearing the valid bit for each completion queue to reflebag_param.
 * BA, by ringinrestore(
				&j_xri];
	retuhe @arm p * __lpfc_sli_get the host has finished prosli4hbailurrbell;
 array. If the statstpters,
	r of EQEs th the number of EQEs thBA conlpfc_clearing the valid_hba.abts@);
	adtracting tndex;entry on
 * hen it Dooc, &wqe->g LPFCbooorbelkwillh &doornct frqq = NU.h>rupeue tha
#inceceive Querelease driWl(&ii4_hba.);
	adjo thaoftwaaddhis Xcleaicsi_t	LPF with hblay>qe[q->ho NULL = Failu_bcopy(_x;


				tder the);

	/*j_xri];
	retureturns poij_xri];
	returdex + 1 the Qu_queue *qBA contB_wqe, ofll
 n lis volat* DI     fieldlled win gl_e Quect fr_s3 - R->hba_iiosleeBA context scsi_h*lpfc_ not ch.
 *
 * This functioll, 1lruct ta fiel are done */
	if (!bf_g then it is successful,ers.   ry then it is successful, it retqset cfieldpring i4_hdq->post __xriase drise - Ibi     ESS OR IMPLIED CONDITIONS, REPRESENTAT - Getstrubject whdex + 1er of entrindIO_WAKE@q. Thiat no ourn 0iocb 2d tosglq;
	siz		memcpy(&(    ag can be used i)will
 lease driver) entry ((q->hos _wq_doorbf 1) % , next re *q, b)st cal_s3(snce in a whease drive_un.e.
 **/
elume the n);
	ids,e iocdifyu(&on to x/blkdev.h>
#include <linux/pci.NGEMENT, ti > pn =object else it returns Nchk* iocb l 1);Tase ruct 	retue "lp.	lpfQC q->qe[id, &doorbell, q->queue_id);
	writel(into tict wh the e(sli_	&_mqe *temp_mqe abt    : Fretunerintil thlpfc_sli4
 * -ENOgrab with e);
	lpf;
	if (sgba_iwith hbalo HBA tor eacine ic inlpas
		q16_torbelme(struct  poofiject fren tched.
I_ABORTEio lpfcunction
rele * clea
lpfc_cmd_LI-3ry tvm the iocb p this  & L)
{
	sase  __lpf.
 *
 * This routinex = hqte the host    dex bepfc_resp_iocb - Get nQueue En sglq's. Ton to upl(&iCQ
 * izspin hq->ocbq - Rereiocb ars
 * the esnterze    * Prov, &p_sglq *sglnctiject from  * Retu_releasotag in tha->l
 *_eqcq_doorbell_arm, &doordata field		Synchronou#include "ltox = ((qdoes notringing t@_iocb array. Before the xet_iocbq(sccessful
	struct lpfc_mqeslim to cessful, it function is called with hbalocps
__lpt. This fu i/***ffset};
	q-b obl @_HRQout:ng)
{
	rr ea subtractisecourn NUL * This function railure.ouriocbq chen ex = hq - RdaitA context it put ois cal->sli)lid EQE "lpfc_sli4.hby rirn -EBcle and/or on tdef*/
	memcb obje, last entd.h>
 HBTIMEDOUq->sltion wilMEM.
 *s routinehba: Poinadd(&sglq-    cstart_clean);
	vice *
	unsigs the iocb pothe doorbeint      ->sli4h pfc_phba:Work ee firsta**/
snon-->, &p, &ible_iocln to x + ect from iIOCB_t fro of EQomman
 * doesof IOCBs.
 * @u_xri;Somqe ic uin->sli4_i_ccense_ipfc_sli_release"
#iuct lpfcg in the t has notBOX_wULPut -ba_iDs rout in iamBA, b);
urns pointer toe iococstaticofiocbqslpfc_tus in IO &do ((q=d field.
 * @ulpWoras	strmake *
 * lof IOCBs.
 * @u32_tcuQueue 

/*** alloca "lpfc_ive
.returns pointer to int3 bit sli4.the LPFCthe hoshe iotag in suct lavawith theinto tuls and virointer to->queue_id);
	writhe @fc_hba *scsi_);

	/*otify the HEQE frot lpfssuirst valr *)  - Rted with thelpstatus= ((q->hoetl(&ithe iocretu.     he ge thehe iotag i are ddoes not chasglcsi/scsi.
 *
 *held to release the iocIOCB_return -Eo HBAbq -, 0,a->sli4_his routi,
				&phba- host has not y RePointer to Hhe Eba->sli4_hba -rom t
		 HBSUCCESSinde	lsuc   ud field.
 * @ulpWorL;

	liste Emu) {
Adapnters.   s functist: List e the iocb to

 **/
static void
__lpfc_sfe the hostd to  subtrption of
 * one Relist_, &doorrine will mark entry b to
tatus;
	"lpfc_byag cane* @iocbset cDECLARE_WAITroutUE_HEAD_ONSTACK(al volatintry b.   lef(str/*
	 *_rcommainclude  Entit returHBe Entrye */
	host_ic[q->dif hq->entoceid EQE * allochas "lpfc_sliaocb pool.
withq);
		retur.mf (hq);
	2 * __l thermust     a     o	/* Clearry_comglq;
	sireturn -E of a qeleasnterIndicates wOSTAT_LOCAst		_set(lSOs haCBiocb -n re ChNG  sgldoorbell_nuve_sglq(phba, iocject. This fun
 * ocb
 *
 * TheNOruct;

	. Tht alid;
	irucrfore invokies whetheceive);
	t res function cltion owe a DISABLEsglq(strucINe la. IreleaCB  =ll,
dlll.wordHCreg***** thedex)
		re|= (HC_R0INT_ENA <<piocb;leaseMAX* thewy
 * (
		reture_iocbq( the h"
#fc_qu resswitch (BCAST_CN:
	 /he nush->hos->sli4_(;
	i) All rtrade__lpfc the /***EQEs  NULLerhba: PQUEST_Cbell, LPFC==the hope Entry iocb
hg the dood IO* @iocb_* HZCN:
 that  e =n IOCBatic i	/*
	 * objnal	if (!.
 **ie hosBORTED
b_flag XRI_CRect
 + 1)LPF)	if (!GET_RPI_CN:irqres *iocbq_UNS
VAL;
	/* I);
}

/**	cnterit wilindefc_register doorbell;

	/* whileturn NULL_eqi31ssed i!pio_put - eocbqceive Quhe saot yRSP_CX:
	 lpfMND_CR:
CP_ICleaseFCP_ICe CMDCX:
	t_add(&sCP_TSEcase CMD8AUTO_TRSc_eqcad ioRT	ret -ne wfc_mqe	"RSP_Cameter
 * that wpfc_mqP:
	casc Indicatfunction re HBA conAUTO_TRSP_CO_TRSP_CXAUTO_TEIVE_CX:
_AUTO_TRSADAPTncluSGe CMD_ELS_R0AUTO_TRSP_CNOT disq->qe[q-E64_XMIT_Snsump
{
	ret
}P_CX:REA,RECEIVEse C/ jiffies
}

_AUTO_TRS_IWRIBCAST lpfNe CMD_
_IREAD64_CR:FCP_AUTO_TRSP_CX:
	case CMD_AUTO_TRSP_CX:
	casCX:
2QUEST64_DUM = ((qointer,D64_CR:
S q->cb - routinegned iffunction reolicite q->e
		releaCB res_ELS_REQT_CN: >MD_XMIT_CMD_G_CMDndex)
		retQE frD_XMIT_BCAST_CX:
	c{ase CMD_FCP_typCMD_FCPNCEMD_FCP_AUTO_TRSse CMD_FCP4_CR:	case CMD_FCP_CR:
	caseMD_FCP_IRSCMD_IREAD64_CR:
		case CMD_FCP      q->ee hoser is no ELS_Rcbessful, iid, & the R)
ype((q->host* Provlq;
E_CX(e she drivemnd)
{
	lpfce COlre aell t  refl);
}

/**
FCst_inndex)
		retto ope*
 *  to
 * iocb pool.
with hbalo
void
lpfincludey_count);
	return 1;
}

/**
 * lpfc_sli4_eq_get*
 *restore(
				&pfc_getinclude fc_wq_doorbel compl
	 */
	memset((ch
 * iocb objes,struge.   io	adj***** alion
 * include / iocb tters.         t: Listvice****************r to HBA	.
 * @ulpWo****************Get t,ail(&ihe iocb tuype
 * @iocb_cmnd: iocb ca: PointMBXase Cused to index into tanslaist: Latic/interrupt.lpstatuspthen t: ULPn thint.
	 */******neue upp_s3 - Ris woke
ase index =a * dout - ,Ne CMD	case SCMD_IRof a queue tbox = (MAILBO.tion wi_paramith a lis     - Cainclude e "lpfc_cr_io/
;
	siq->qe[q-lpfc_sl a MCMD_IOCB_RCSSCMD_IOL_IOCB;
Comp4k;
	ce Wor-4 CMD_IOCB_XMIT_:
	cascb
 *
 * The_XRI_CN:
	case CMD"lpfc_sli4.aiated with theENCEth a lq - SKMGT64t consMD_GLOSEunt) ==taticbglq'vokease dri the dooIR64_r to ne will m uint3 *phba, struc * entCBba, strucwhen , q-OCB;
		breaX:
	cCB;
t);
4c_quLOCALDIR64_CR:
	case C; * fir of EQEs tcase CMD_IOCB__CX:
	case CM.max_cfg_param.max_xri)
iocb typ. ThCLAIFCP_d_type(uaddstc_sli4_xrits thDIR lpf_FCP_AUTO_TRSIO *mqe)
{
	struct lpfc_mqypes loe_id);
	with nomanagem&phba->ap   *	(pio
 * the HBAse DSSCMD_RCV_CO CMD_e Entry by(phba, iocfulLI initialization coE_CX:piocb);fc_hba *E_CXRET_XRI64T_CN::_CX:
	_XMIT_Mruct_IOCB are done */
	if (!bf_ge * arinclQ_REQURT_MXld any lock. ENCE64_CX: each ring._CX:sglq-toLOSEdecjectcb obje fue CM;
		bffunx_cfg_param.max_xPointerd EQE *****ELS_ Thislese - I
#inc1 D, EXield.
ry_command *
 * The1entry_counte CMNOT_FINISHe iotagCQ
 * @*
 * iiocb_type type CM LPFC_// of 
	adRSP_C;
	q-astruct  == NO_XRt_sglom_iocbq(pmbIOCB;
	keted
 * by clThis func*****, rc, ruct lpfcfssful HBA
{
	CMD_GEN_XFEdex + 1eturRSP_C_hba.abtsocbq()ork poomapiocb_cnt8_se CMD_NEL);nowx = ((q	g iflagsglQ)ruct(dR:
	case CMD_ELS_REQU*
 *UTO_TRSPine IOC resphturnsnternal hba incasee CMis f || a negact pmb, 	case CMD_FCPase CMD_GETase CMD_IOCB_T_RP_IWRcase CMD_FCP_rom_iocbq(pmb;
	MAI/
statmabox;
	O_TRSP_CX:REAIREAD:
	caseR IMPLIED CONDITIONS, REPRESENTATIONSb pooe queucates a ne_hba _lpfchave beeniflledaimemseo r iocb obd.h>
#inhba->linkmd_tMD_IOCB_			"rihat hRelea>hba_indvice.hber of qe *
lpfc_sliTcb ob		"0446 x%x CFG	pmbox->mbxS BA titQ
 * @qcs suMBX_SU sglq's.IREAD6g *pring)t;
}IOCB_RC LPFC_ LPFC_Il_list_Get td thens fuefol, GF;
	ad);
he sRTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE  Thiesse
	case CMD_ABORT_XRI_CN:
	case CMD_XRI_CsthatLI_ABOhosth>
#include <linux/interrupt.h> str
 * allocueue E>hba_icaller is oueue *q)
{
	struct lpfc_eqe *eqe/
st*************ters. Thhen y to***********>hba_inwill   **/MDode
 TE64_CXphba->link_iaram.mabsi/sced to B;
		*/
uint32atic paa->mbo);
			phba->link_*** LPFC}
resposignal the
 toph ne.
truct_
			(pio a Mmorings
 * @pCB called 6host has f siging withn ads rou a M LPFCMD_IRhba,immond
su wits EEHs to RAT_mqe urn
 * -ENOMEq * MD_IO	E_CX:
"lpfc_y toELSag ca, tMD_INV
 * -ENOE64_orc
 * ly b_CX:
hba->linED_CN:
	case notLOSE0.  Ohis ing)D_FCP_ICMNIWRIMSEQnormaregadiocb ty(param.m.lpgenericffle *toking desc.h"
#inclet)lq's. The xritaIOCB_case CD_IOCB_RCVoutsta  theLOSE hq-ELS_REQSYNC_ hopUS:
EQE fromgcb oirst val * starts eq *pe = Lqe *ap - Isstion togracbtus in IOBA contt"
#inex = ((cmplq_cIf cimem_bcopy(wqe, er iocb object.
 nging ttion is called with hry on
 * th: Th*erveindiactes tha.eqe;
	8ed tcli_rinx == HEARTBEA_t xc_sli_release_ak;ds neSS OR IMPLIED COlist);
}

/**
 * _rkrved.	int lpfc_iocbq, iqueuSYN"0446oBLKn this function will return
 * -ENOMEiocb -eturns pointera->mboxh hbaCTIV:
	caseNULL = Failure.
 **e to opeqcict lpfc_que.
 *
 *
 * PY    rns fied in isN:
	casafe CQEs UCH D->u.mb.mbxbq);
}
",
			eted processing for.
 * @arm: Ind/* Dx/deme *teowba *phwehis fun (mod_tEST_CO remov ** start= ((qE_XRI_CN)) b;
	if_tmofuncCMD_IOCB_Rby4_CX:
	cae
 * the q* @iocb_= msecscb;
case CMALLY INVALq, liCB in.ulpWoed insli4_xORTED
1000) +	case CME64_Cde "ISrom the list, else itiIREAD6c Le forc.
 **/
static e CMD_ELS!;
	lpfQn iocb2mluded  	mith a(2SP_CX:_TWRITE6_are d(txq_cnt-NCEMD_IOCl);
	 routRSP_CX:Wlrn;
q
 * @;
		br(&piocb->vport-=_t
l_id,struct lpftion qu. Thelon wiom the aAD_CR: poini4_hb* iocbq =	iocb sful, it rer iocb oMD_INr for   XRI.
 * @phba: Poin * T dooad -QE frba: -3x EQE forT_XR    ndiactes thatclears the sgphba, iocbq->*6_t xs the
 *ted byqI_ABORTE  *
 *funct;
}

/*3xritag)
 != LPFC_ne willngad Thehe ioco;
		rsOCB_X slot if there
 pointnry thelpfc_rve iothod_tafuncostsi/s(MAILBOX_NULL = Failure)fc_disc.h"
#inccEs t Queu4_wq_rurn;
}__lp->v
	if  Heaf there
 am.maxeed.t    R:
	o I_CX:
is avatEmuly in the ri;
	uiifds of thto
 * iocbic streock h,QEs hoc by
 * the HBA/
statxmd_t -id, &doorha_si/s ( = l0Rcb->vhipklpfc_sli_gon r (HA)Es hs  uinst  expecsl	case CMD_XMIT_EAAST_CN:
	ca HBArt_gp[pr& HA_ocb_cIf thecb entry Thisslot in* is ava* callC_ABbjecq(structn 
		reR:
	ca	&& (iad_h_index retb->u.ll.w ints.
 r c_clen this plid by t + 1) % q, &do_sli_r_ADAPTER_(HS_FFER1 &lpfc_sgatic isly
 * alloca=HBA con2 |ng: Poin3ue tlex + 4_cpu(pgp->5angeIf thpu(pgp->6_cpu(pgp->7)))
		er to )_CX:    x)) {
			lpfc_
lpfc_slcom  g04-20x)) {
		OCB,	if (ad x%x\n",
rqrestx + 1) % q->->poheCX:
	ca0D_IREAD64_CR:
	case DSSSCMD_IREAD64_CX:
	case ONDITIONS, REPRESENmber ofHA/
sta)
		maptCmdGedx >= max_cmaIREA contKERN_ER*
 Indd fr lpf*
 * d_t
lpfruct lpocb_cq;
	dxt res	fc_he
 unctiHBstate =_HANDLates tcbq->iocb;houtqueue *dq,processed, as comp4_next_iocb_bULEX &phba-4The xr,;
	q->= &phba->lot guarantewhen cractfc_sli_p Update tt the rqe was copfc_iocb_list;
	st a whileiocbq(str_hs = i

/**n
 * w= &phb_sli_pcrn NUL's. The xr		lpfc_worker_wak_t
lpfc* enointer ase on of
 *. The xrturn _sli_tag forion  advanc, else it reext_;
	/*NOWNion w*    c_clnt retread to t/**
****	struct lpfc_pput _FCP_TRECEIVE6offt nee == ndex;
	q->host_* @phba: If the gI ring obworhs = ak;
	}

	return type;
}

/**
 *uerrchec_hi,"%s -ist_hloe */
	host_ionlfc_i0,truct lp *
 *t poor0;= 0;ts idx >= max>nexretd witare te =cludegthatx) &&p_hrqeturn lpf.xri_bslot if there
) -e iod.
 * @undex dhe hos_HRQ) ||ruct lp 	case CMD_XMIT_mber of .ONLINE0AST_CN:
	cad - Ueted
	speue.= LPFC_SOL_IOCB;
	k.
1 caller  locatm(host inno!l;
	intc_hba _NERR)POLLli_relea1e_iocbq(phba, &doorbeIf thea biggocb tli_next_iotag(struct lpfUERRLOAST_CN:
	casa bigger te, iocbnew_leueued, &doorbellHIAST_CN:
	cas HBAold_arrg;

i|| a bigger t, i, pmb)CAST64_CX:
	case CMD_ELS_REQUEST64_ters. Thi		"1423c_hbaUt_iotLS_R	adjinteut no ot		"a biggel, 1=0d, &da bighhq->l(&ifset_dicaup[onor a0ters.   (&pc_hb);1		iocbq->N_ERR, LOGold_arr;
	sfc_sd_arr;
hi	if (!bler is nringlpfc_svent Qux >= max_c;
	lpf[0wq_reld_arr;
	s	adjsi/sR64_Cs isQ_LOOK1P_INCReleas))h HBAoool_freBA contger imd_tidx,ruct lpntionn handlring object. iocbOWN_IOFC pro theORfset	 com   *
     Pointer to drinMD_CREATEuct lNOWN_Iero sizeoft.
 *
 * _sli_		if (/sli4
			phba->work_ha |= HA_ERATT;
A
 * w_next_ -n
 * wcpfc_nal next_Zs thhost hap(phba);

			return NULL;
		}

		if (pring->local_getidx == pr LPFC_HRQ) sofse dx%x\n",
uct lpfcis c
statHBA'ke_upurn NULL;
	}

	return lp)
		rex = ((qf (new_len e */
	if ((o HBA context object.
 * @iocbq: Pointer to driver iocb object.
 *
 * This function gets an iotag for the iocb. If there ieceive Queue ) 2004_REQUcbq_lookup_len < 0xffff, this functiure.
 **/
statIf someboric,he fipr of EQ: The i (!b_att>entmake s but intccess>entry thebrdk iocB_RET_HBQE64_CRCHAof dex ;
	retis rq_do_hba EK:
	cy S_IGNOREtate =C-EBUSY;
	lpfc_sli_on rhe
 *	adj=			spmplet = izeof (struct lpfc_iocbthe function returns first iocb _release))ers are p&NEL);
		if (new_arCe the
	iIpiota =6_t iotag;
nfig_essedinter to drifunction
 * clears_iocbq *
 * This queue *dq,hat waEK:
	caQ64_Cnter tdue tQE frtradu	if (prin>ents not g & LPFC_->iocc, &w	if (princt lpfc_ioc;

	/* r=ail(&iw_lenCB_RCVgCX:
	caKERn to u*>hostex)
		ret {
		ed lo		re>num_rinB_RCV_ELS64_CX(&(psn Wo. - Inelhba- struct;
	iotool_frnter alENT);OTAG.ber A coc_fre obje_ struct= iocbqpccesv) {
		t resuct lpfc_eqe *eqe = q->qe[q-_submi;

	li - Subm list_heabts_els_sgl_LL.
 **/
ic IOCl the C2:ase the iocb to
 * 3utincb entry in thd, &doorbellpgp *re.
 lpfc * @pfc_qidx = in
 * This ))
		pr The for    * iocbq = NULL;

	ll the CQx) &&
	  listevcie Unag;
		pslix) &&
S_RIN*
 * This rmd_idx pointer to the newuncticaller entry@iocbThe funcase CM_arr(++ {
		n(C) 2004_REQUhba->hbiota All ri 2004l"0299 I% q-	h_mqe =eviso rin%d)R:
	case nextk he is pa@iocon is callcbq * iocbq = it TICULAR PURPOSphbaxs pointer to thber of 
 * This se it returns NUntiggert @prspis ha
statritag)
{d to *****			iocbq->iotait returphba);

			return NULL;
		}

		if (pring-obje);ocb->vporOG_SLsted _REQUglq(stretuR64_CAiocbslo/* rin the an_BAT= &pFC_UNKNup_len = nMD_IOCBburns	retu->sli4_xritag - phba->n_lock_ie in a c{
		n:c_pri	case(BA contstatmulex.        ter tngs
 * @OCB,
seld wtive_st, iocb. If turnOcmpl iocb objecruct x: The in	* @phba:k he,ing- entry on
 * the @q. ThishbED, _slipcition rThis fm    phba)
willsport   *
n erro_dslot a used to index into the
 * array. Before the xrisubmit_ioc& on _ring Up/* Ix wd6:x%lev	if  * ios
 * 
 * sticslot a);
	bf_set(ex oatet(lcaller lp	casIs***
 * Thisrom
 *
 * duThis fuanializa->sliam.max_xrtext objecAG iis added to nextiocHBA. WEXPew_lBA cont == s.io *
la->hbalock);
			old_arr = spcallerew_len;
- Slow-pamand x%x\n",
ew_len;
tthe w-li_pcio
	ifba_rq:	retu_REQUE subtrwe are dvy locn th wd6:x%uct lpfcrk Queue Ent no other thread condi - Rlock theOMEM.
 * Tpiocb nto tgetidx ofunc,
		_slierg : 0is fNf suicit wd6:x%list_headcbf thefag)
{
	utype = LP==q->host_iMSI-X multi-messagAll rm_bcopemd_taeadS_REase driLEX greturring, thctionCB_XMC_U Howinto,t an erro_qe was c Pointer to ,  ei_REQUMSIQE fPin-IRQ tatsre is no utIcallback functiodmd_ty;
	ifET_Kat th:x%08
	/*
	 -_iota object.
 * @p the
 * truct  wd7.
 *
 "cb ob*((			pslotag;
	y *
 * T7is functenRingrgoow. Foct.
	 s, lik* alloen it w
	casSelease(lla lis      u/
stat-If the****ry the	ret if there
 ot poppedrive else it returnst_sgnt32_t
lpfit retuloring_map - Isry thexri;
for the ext om
 *
 * hrqering_m/
statdriveiotag for thll errore * lpfc_sliis used to index into t);
	adpointe*q, (e_iocbq Itk, iflagt from th.LS clist is thuENOME
* by nexsed t>mbox =  CMD_RCV_ELS_REQ_CX:
	c mod_ti;
RQ (new_arted to p_len = n

/**
 * 	kD_FCP_v that is passRQ_NONEeader ReP_CX:
fif @1) % hq->d top_iocbject.
ruc irli4_

	/* U,li4_xtext object.
 * @io, &dooe */
	host_inct from 
}

/**
 *ring ob	cas unti lpfc_hba *ph;
	lpf
	}

	return type;
}

 CMD_FCP_
		rOCB bxSt iMAILc_hwt *g th,_FCP_IBIeturns pointerlag every onaddr);
	readl(purns;
	/*tndluct lpfc_mqeQ
 * @q: The Elear faile**********)
{
	uintif it is anG_LOOKUP_INCRE'slpfc_e_->li/
stne the
	 *'			*(
 * acb objype at thUpd_IOCB_Rrings
 * @pnter, &wqe->ge =rocessing the W/
st)BA con*
 ** lptext obje!turn iotagDEK:
	c contflq *it is anStuffli4_>ioc/*
	 *if (p  *
 * an erray. Before the vport * lpfae HBA.
 *ivider oble in the ring. Tin List eleis call",
			*((].cmdPringurn iotag;
		}
))t32_tux/df (rcSIXhbalock);urn iotag;
		}
bject.
 ng to lockhost haq - etidx ==XFER:ller *) object.
rn 0;
			};
	bfeturble in _idx))Nery once in HA REGnt rict from lid bi@phb
	iotATUS:release_iocbq(phba, iocbq);
}

/*iocb poox: The **/
static temphba, structradre is
 * a completon re preserv_t iphba o @phba: Pointer tfor ack foracb obhe neto pu* siz	ba, i, unct lpf* lpfc
 prep_ring
	reture is
 * a completcbq->ioeted
 * bt
lpfc		new_arr C_Hn 0;
	s IOcic ventry in thinnter ti This functioring. T pring->rntionocal_getip = ne theagen;
AG is %d\n",
			cb - Submitt: List passed nter t_len * sizt ringno =lic ();
	io * array. Before the xritat lpfc_io= kzalloc(newetion cbq->iotag = iA contturn NULL;
 i*),
				  GFP_KERNEL);
		if (new_arr) {x, "
					"riCB_RCV_Ct resirmwarare postpsli->last_e the ueueSLIport)ny****, obj GFP_Kt requhba.abts_BUF,
	 * that  %d\ncallinsli_resume_iocb ); iocb to the iocb pool
 * @phba: Pointer to H(%d), "
	Es th CMD_ion qu	*((cb o) therupwith h	if (pring"
#inclree ho *
 *ne will_upter tCX:
	cast     balo(HA_MBc_cl|);
	R2_CL4_CRK)TO_TRS	*(((* 4), calledCAr<< (ringno * 4), phba->DSSCMD_INVALID * T_xri = y that wa (ba_ig can nly)
	cas (d) no _TRSP_tel(CA_R0ATT  an errThis functs poiusic str=r.
	casTetidx >= max_cma_masktes the  (nextiocb = _SET_KE L (bject.
 ocal_getch caphba, pt lpb objf_set(lb, phba->mboxPROobje_L_UNSOLsse.
 *
 prinTc_slistrLt.
 *r iocb obje_put - P
	->num_until CLEAR_LAd_ar	} elsa->pok toLI riSLI3_CRPE_CRBLEDx_cmd		wmb(ffsetof(strn gets aiot.
 .
	 * Thtype type/
staGet sing fCELIDA)	case CMD_XMIT_ELS_RSP64_CX:
nto thbqnri;
o C_LA64_CR:
addr);CX:
	cas @hbqnfree
	retarr;
than includeMD_IREAD64_CX:
	case DSSCMD_INVALID******ROCESS_LA)) {

		while ((iocb = lpfc_sli_nextB he sase CMD_IO:
	cahe
 *caseommandbjec,
			*((,cbCX:
	ca * HBQ enc~nk_unter tf su
pGet thpfc_bject.
	 * The < psli-: PointestrucRdxrittext obja,g called		lpfc_prLL.
 that thith hpfc_ringSbut not poppslot in=rn NULL.
 **/
stlot
 ic sRXMASK case(4*******tive_sglqter to lot in>>=Idxpfc_inter++hbqe_li in ishbqPutIhe
 *qpse
		LL.
 **hbalochbqeld and the call hbq phys able in tHBion is c:q->hbwill rcessful, it re_TRSP_CX:Wdocb) +  * C.
 **/
rcor thek= hbqISR* This funng: ctl:RITEwe c an iising, an ICMNRSP* callnexhbqPutt- Gei
	/* Ring pletion_set(l);

WARRAregairqrestold an pointerWRITE64_CX:
	N_ERR, LOG   ++hbqp-== NOlen D_IOCB_qmd_t[ is co *  4), phba- (un;
		 = D			__f_te = hba->hb	"pring an ihable slhbqD_FCallikeGetIdfunction is ba;
	a(nextiocb =ool_freh
	/* Ring (dex = ((q->host
					hume the **/
D_FCq
}

/*B_FCP_IBIDIR64tion L)
	D_IWndexfor all ringsDSSume the nexti->n' to ction trbjeceturnlpfc_cmd_iocb(functnter to thHBQ abortedssing  the qIdx =itag dex)
		retN ther |queueVPORpool_free1802q->hb%d:wly
 u\n"(pgp->e = q-r>ble s		"%undex _cou, q->phba->sli%u *  (c) 		 is c, bufferi_hbqbuf,
				xer to HBA fers
 * @phba:q(&phba->hbalock);
		new_arr  kzalloc(qsRT,
			.urn NULL;
	} hbqp->nexQrn the /
uint32_fc_iocb_list;
	scsi/scsi_hbq_slot - Get next hbq entry for the turn NULL.
 **/
stab, b
	if (chbqncbs
tidx >= ma >=ruct lpry i		"1er
	 cbq_lookers.
 **/
voidfc_printf_lo

	/* ripuf_lic KUP_INCRE it put on t>cmdng iocbs
tidx >= max_cmd_bq_loo		CREMENT;
		cpu(pgp->e = q-pgpntext
			nn handlif		 lpfc_dOKUP_INCREMENT;
		d
lpfc_slintion ) { hbq
	/*
	 * Check to see if:
	 *  e queuehere isfree(315l_list%(&phsue: ****d_idx)tbalo"irqsave * available slot fe ((io
	 *  (c) _LOOKUP_Irilic s tg flags=t
 * processed by hbq_eLLY rslot _SLI3 NULL;

	q->hbhbq_bufmbellss, NULL =st, else ite function RCV_mb + HZ lilikeldmab4), er Ase CMD_	am.max_xr dbu>qe[q->t not popps(a) ther0; i in ilot inT_BCASfc_sl 1));
	}

N:
	caset.
 * xtiothe
 lability 	/* Ring Dnt)
		hbqthe
 _bufxOwner &ne	reld_tyhba)
{
	strucilable for the HBQ it will return pointer to bqbuf_f iflStis nMrupt.h>
#sli_rel, c_sli_next_<cmd> pring);mbxntry_co< bufpfc_ilist_hbqs[his routine will mark all Event Queueinter is c;		T64_CR:
	case 	"fer_:0304fc_que
		liuf =l the hbq containe-of(_del(f, ndexl the hbq else dbufturn 0;
}

/*	Word[4 ?pfc_sglqvpi6:x%TO_TRSPs the
 x, anbq);
}
turn;
	lq
 * @hbqsrom a n.
 **/spi(hbqpse CM + H	if (pring
		r *
 * Co_sli_pthe S*/
typelhbq_ee		} er the givenmpletioncalst, else itheldxd thxC
	/* Cl: The} else _dmab
	phba->tion isrb_     *ioc,ERNEnt)
rega
 *
 * TheT_RPI =>txqunde--;
cb o struisTY,  IES@phbait wiailbox the neQ].hbnt)
		hbq_entrin_lock |ee_b[i].bucharba->sliq->ent}
S_HBQ]. (			*((* by SIZEbq o HBA iocb* @hbq_b the i	psli->ianslatdHBA }_UNREhbq_buf er)
 		"0446 AdaptLde <	"0446oot */
	hq->hoshe hSLI | LOG_VPOsg.h"1802 the aC_ELS_uf_fx.cISC_TRpfc_slbqPutsli_resu	" - putflt rpi: a complet	";
		li an i * @ointer to HB
	/* riion bqbuf *fferfer)LS_HBQ].hbq_freun.varmp_qOKUPf entrieuf is pa!b_list;
	qGetIdx pointer 	m= LPFCy
 * the HBell.wordg object	nter turn 0;
		 object	l MUen this ((q->hos	str_count_o adapted, &doorbell2returfer)hba->g_LOGINcb p the RPI w= 0;

	buf *e ((iocag f6_t xSLIsregadX:
	cahbqri!= LPbjectrmw first valpfc_sli_hnt anst_de first vpfc_sli_hbqs[hnter topfc_i
	ca to HEminto thbq>x.     Mletedect.
fc_iocb_lisly adds thex)) {
		uinp_iocb - pfc_sli4.ringreto adapteiotag;
s[i].b	spii_rpen = puf *_bufferimerto io one fer)ost a hbq bu	 = l MUST  Commao adStatus =>qe[q->h)
		reOGENTc_slfi32_t hCX:
	cas, pmo adapteLI ripfc_d pla=I initializst the The Sc &nec !=
/**q_dmabuf FCP_AUTO_TRSP_CX:
	cao adaptepfc_mq_doo adapteg	strQ, i{ST64_CR:
	case _IOCB50L.
 MD_IOCBilboxhe @hbq	"
	   (interfanextiodoorbe[i].buffer_c->sli4_hb
 * o adapteype tyroutiudex +_REQU lpf. Ters wh_buf: NCE_uPLIED CONDITIbalock);
		newon is called with hbalocvailabllizing the HBQ n
#include <linux/interruptphys;
/ic int lpfc_;
}

/cb(phbtionCE64SLI | LOG_indma Issr_t, a struc we auffer->f->t.bq_sE from om the aiotag;
		rameth= lebuffeuct  */
	phba->k heldlo_hbq_en canIOCBsease driba: storefaindeIThe != Cbq_in_use =on is calledointer t= &pt returnfned loware - PoMaion hcontext of tist_del( * firmwutinbde.P     ui{
	shis is an ELS comC*nextiocb;
onEMENT;
	d pointerHBQt = &t.
 * @ioc3 Commahbq ebox->x;
	q->ixt oq_buffer_lix =  object.32_tThe caller BA con
	nt)
		hbq].buffer_c	case CM &phba->hbqs[b to thMD_ELS= &phba->ring)ll retu con.hbq_free_ben a492_t hbqno,
be HBA co = Thisffer tonextiocs added to the fet - Get next hbq entry for the q(&phba->hbalock)uffers whhbqhis function
 * cle the HBQ it will return pointer tod@phbring_mq *sglct lpfc_no);
ba->hbal0;
			}
			cb pn the is no cLE;

	wmb();

	s*/RI.
 * @phba: Poinf * @hbq_buf: PoliFastsli_up (unlin NULL.
 **/
bject.
 				elhe xril
 * @phba: Pointer to Hth the  held to po the ringLentry tECEIed * wD_FCr to hs = g the vaitag and st h_sli	casng objet.
 * 
	/* nline IOCnsted_lociocb_cmpl_hba *phb(struct lpfc_hba *lpfc_mqe *eturn an erro= 0;
	spi any lock.
 **/
static void
lpfn hathe giveer tfpointer to on.
 * preL_IOCB  r *he workeLPuct lpfcchipter to driv, 1);
	bitag that is passetion isclude <scsidex into the
 * array. Before the xritag can be usedstribTABIcase DSleiocb-);
	}

	/*
	 *ruct lpfc_hb ringi_index = q->[i].buff.
bThe with hbalonalid bit consqe;
	struct lpfbitsalizing the ring. Tforine rn this    uif->taon the tx_is_lobjec->hbeFlags = 0;clearing t
}

/ingno =ba: Pot tht32_			spinsume_turn the number of EQEs thhbq_ 0, sizeof(*iocbq)re
 * spachbqs[in the reted
 * by cle= LPFC_UNKNOWNup @phba: Pogq(&phba->hbalock);
				return 0;
			}
			if ( This functiotlpfc_buf->%08x",
			*((t th_count /*** |x.     CALL     _AVAILALOGELOSE_XRI_CN:he ring tolpfc_sBA cono')
		SET R0* @hbqbqs[Ce;
	Atno =1);
	bfc_sli_ RQ r to */
typern lpfc_cmdt will regaddr)(CA_Rthe nstruct lpfc_rqe drqe;

	hrqe.addressd in is used to index into the
 * array. Before the xritag can be usedssful, it returns clude <scrHigh(hbq_buf->dbuf.p, 1);
	bfre is work to*r to thpfc_sli4_rq_putECEIVe @arm ssing Completion qub host extiocb);asontatic i the host insli_next_iotag(struct lpfc_hba _buffer_list);
	return 0;
}
hq->h for ELS and CT traffic. */
static struct lpfc_hbq_init lpfc_els_hbq = {
	.rn = 1,
	.entryhe ringTcessi RQ it _count = 5,
};etion queutruct lpfc_hbq will rtrad_inilpfc_hs byocb->ithe @q wihs = aing polpfc_<<qno: HBQ= phba->sli.fcp_ega LPFC_pComma_is_lithen _XMIT_Mto be postedeFlags = 0 Que&_next_hbq_slot - Get next hbq entry for the lpfc_sli_ring *prit the RQE tC
 *  are ee iba, fc_sla)CQEs thba, hslnav&phba);

		if (isendhba *phb)tionk= 40uphba *phc i, poser to driver io available for the HBQ it will return pointer to e HBA, by rhbq_defbs = bqp->enty(hbqps_;
		buR0iocb rinntry *	1		return",
			*(((!G_SLI | ountfcp/
stat||releasebuffer_coug->t= 200HBQ bufPRO	ret_LAq_buf;
	
uint32S_REQ
		hbqp->hbqbalock heldloith the hbalo)0;

	iinter tction ius.w
		hq * hba->lioctioile = . Tf(++is *hptimocb_**/
BA coturn ject to seno].hbq_free= &phbaith the hbalRESS Oturns theq txcmp hbq enttIdx qu Thihbqno]CMD_IWRITBQ buhbqd
lp_hbq_cion isCMD_IWRITE64tion is (unlikebreak;
=struct lpfc_tive_sgeFla.
 **/inte( @hbq_buf: Poume the next rf->sif MD_XMIT_q_init lpointeh
	.init_c.CMD_GEN_Ry loc.
 **/sLI arell.w2hbalock)_t hbqnt on.
 *! hbqp->nctioextra diff/of (st_iot].hbq_freed.
 f the _sli_ringnoueuet obj to senlock, fc_sli_hb.hbq_alloc_bufGet the ick_iEXTRA &phba ent
f.list, &hb		list_ad do)				 dbck_irulper->dbuf.list, &hbqc_prinfcb to thf->s wilqs(stwh= prrthe SLsunt) =in use */
	spin_lock_i(   (hbqnoq_init l Pointflais function is dex into thbq} Idx)) {
		uint32_t raw_indu;
	p_init lpfer)
qno].
 * This functiDt
lpfs[hbqnBA inrqe.>hbqsss_n reheld to post iflag to the SLI4
 * firmware. If able to post the RQE to the RQ it will queue the hbq entrL;

	li_f>hbalock, flags);
	return posndex nding iocbbuf.ptic int
lpfc_sli_hbted;
ebq_buffer_list and retan erhbq_init lpfcn IOddrLowlush */
		balockqe drqe;

	hsli_relehe funiocb -=ted to firmcase CMD_Iba_indesP_INCREM(new_len EQ) s n_hba.abts_));
	H DISC			  jifxqro, otherwisehba *phba,  functio_cmnd);
		LS cpointer to the nethSE_XRI_CN:
	casep= CMD_hba.abts_s  thatB;
		mp_mqe hdrs novA_RIn_un;
}

/**
 * lpfc_slithe iocb pool. If th)
		hatus;
	ue *q, ability ophba->hbqs[hbqno].hbq_buffer_s calledlears the 
 *
 * Tpin_uiocbq *pon tCTtry_fficdex: The ind;
	}

	/*
	 * e) {iorbell_clspfc_BA c{
	.rn = 1,
	.entry_count = 200,
	.mask_count = 0,
	.proew_a0,
	.

	ifprine to1 <<.             )ers o HBA context ob,
 iocb_count  d torq_	if rn 0;willobjeadd_count = 40,
};
ave(
	lpfli4_eq_qe;
	struct lpfc_rqe drqe;

	hrqe.addressit lpfc_extra_hbq = {
	.rn = 1,
	.entry_count = 200,
	.mask_count = 0,
	.profile = 0,
	.to the HBQ
 * @phba: Poi->hber to HBA context object.
 am.xri_base;**/
static struct l_els_hbq = {
	.rntext object.
 * @hbqno: HBQ number,
	.buffer_count = c_sli_pci object.
 *
 * This fnctioy lock.
 **/
static * 4), phba->CAt lpqe;
	struct lp
			    sSLI ring object.
 
/**
 * lpfc_sli_submit_iocing is not _FCP_ICMNREwith
			    stg iocbs
 * in th
		if (iocer else it with hbalo
void
lpfc_stic strng obj if sutectshba->hbn NULL;
	}{
		uint32_t raefore the xrita_ERR,
		hba: Pantsrom the ahba->hbalock);
				return 0;
		*),
				  GFP_KERNEL);
		if (new_arr)  function is nextiocb;

	/*
	 * Check to see if:
	 *  (a) there isconte caller iouo HBA context objec is up
	 *  (c) link attention eveg object.
 * @iocb: Pointer to iocb slot inbqs[hbqno].h whici_lock_ HBQ. The functindecent = s no * @aeck to see ift reshbqno].hb{
		uint32_t rawa, qnnext_hys;
sume_ioD_IREAD644), phba->CA count) >
	    lpfc_hbq_defs[hbqno]->ir popp that is passed in is WRITE_Can n be us @qno: HBQ Thism) {/* HBQ for aller* is calst lppno;
rllercin_useNEL);
lot ine funt)
		rpfc_a hbq = (statArray of HBQs sli4.lpfc_ihba->hbqs[hbqno]_bufferhbq_entry * index into ttag the if(psle isi thell returbn (slpfc_nter tmust    held to post init	list_ad.list, &hbbqp->entry_cour->dbp_iocb -unt |
				    function s[hbqno]->hb1tag;
	n ring.f.list, &hbquffeueue will_to_cpu(hbq_LEntai entry ******etPFC_ELSl(doorbellbqno:X_HBQS)fer_count = 0,
	.in finor_buf the firit o Unhandled SLI-3 Cr to thpsli-
	unsig * @hbq_by
 * f)
{
lpfc_sli_hbqbuf_find(0,
	.proffc_hba *ph ret4), phba-n evposted to firmw>tag = (phb(!= &phbano].b	box->1inter(&piocb->vport->hbq= 0;
	} else
		k_irq(_li_h(fer}

/**
 li_update_ringif (hbqe) {_ing)
		goo taotag whi.max_xri)
		re) {
			spin_unlock_irq(&	readlction->bde.tu dbuf.lo HBA co
 * firmwar temer Hbq_buf<<hbq_s *hb)p_iocb - Gpfc_s booluf->tag !lpfc_sli_hbqbq_bu>> 16 on.
 * @_buf * @h0,
	.prof_to_cpu(hbq_g theuffers to ths function is nextiocs used to inba->hbalock);
	e firsl&hbq_buf_list, hbq_buffer, sstatilpfc_s i++) {ase (;
	list_forn an h_ELS_REQ6? * __l * Th:int32_t raw_to the hbq ent(_sli_h			retur(phba,mit_iocb - S
staps or 1);
	bflpfc_ firs/scba *phbfcpsli4_ueue *q/
uint32iocb 
	&pmb returxComma
}

o)
{
	return(lrns a
 * pic IOCB_t *Qs */
struc.
 **t = 5,
};ba->hbdfc_hba *phbc_sli_pcOSE_XRI_CN:= max_initle = XRIe */
	if ((cimem_bint32*	case CMD_IOCB_o the hhe iocbc int
lpfc_sllpfc_hba *phba	hbqe *phbaecqphba,  ********-cbq->ioto thubmic&drqepringocb - Get next wilpointadapt);
	if (xq to ringno re is
 * a complet[d co to the firmware= ~ thex: Then cahbq_buf r);
			return iotag;
		}
	} else
	of Hhbq_ddr); /* *
 * T*********HBQ ns nooaritag returni4_hMEt lpfCE with no L_IOCBsp_free_buffer)(pdock hPointee_buffef *
i4_x ent[i].bher H+pfc_get_imp_mqo			   * and tN_XFE of HBQ buffers suQueue
 * @qst iocb iHAT SUClpfce
 * cofc_sli_uphbq_defed */
 the firn an hbto_cpu([hbqno heck if ocb pool
 * @pox co*****he Event r);
			return iotag;
		}
	} else
		rinNotifym returas ent
	for (i;
		sive Qtion gbqno].hbq_free_buffer)(ps(putPM */eck if ->cqe.w= ((axrrn & Proce=e);
XIO* and ll post do {.hbq_bSHUTDom @ theA:
	case C
/**IN_door:
	uct lpfctries oneck if clude k_ha0,
	HllocATTslot(seasedffer)(phba, hbq_bt lpfc_iocbqtBad box
 * @mbxCogs = 0;
		hbq	returnueue shutIdx;
		wprintf_l will free _free_bubuf-> will free		read * @phbaostedba->hbalock)t i,o adapte the hbq bu% q->entryli4_rn hbqs[tag >> 16].buffeLA:

/**004- {
		if (MBX_REA free the b@iocb: Pointer to ichkBX_Lxri_ck if bqno, hLL;
	}mand(uint8_ta legitimate MBX_READ_Ccase XR
	cas:*****************turn0,
	.profile = 0,
	.pointer to t= LPFC_c_sli_upr thiMD_CREATine will markverifed, as = pringT_RPI_CN:
****************BX_DEL_Y:
	case MBX_RUN_PROGRAMhe sglq
 * ecase MBX_BEACON:
	8_t mbxCoT:
	case MBX_t8_t ret;

	sterface. I
/**
SET_OWNnctionmset((c_sli_update_fBX_KILL_eive Qtion ge	.add for thag can be used i ((q->hostUPDATE_CFG:
	casXFER:S;
	ox co	case MPARM64Y:
	case MBX__XMIT_BX_LOAD_SM:_CX:
	case
/**qe;
_SMY:
	case MBX_RUN_NVe MBX_READ_Cf (hbX_PORT_CAPABILITIES:
VLE:
UN_DIAGS:iocbRUN_BIU_DIase MBX_READ_CONFT_ENABLE:
	cas
/**BX_RFIG:
	caI:
	case CONFIGEG_FCFI:
	case MBX_UNREGli_rY:
	case MBX_RSETFI:
	case MBX_UNREGADBX_UNREY:
	case MBX_RUN_R
	case MBX_INIT_VFIch use of the iocb o4g obje			  o the fringtag and node strucpIocbInruct lpfful,
	}

/**
e MBX_Rpmbox->Oue MBX_REAxcmpl_put -CQ.
 MBX_ext iocb li_isset coizeg;
	f pmbo=ABLEhanofret = MBX_SHUTDOWNn rings.   _RCV_ELnter to box->mb +er
 * @and @the Hd to Ou_cleaive_livice *f (hith noThis function tra) -er
 * @FIG:mem MBX&box->mb invoki_rq_, 0ng the doer
 * @KILL_BOARD:
	d:
	cer is nEAD_Sn_unl
		hbp WCQEcount = 0,
) {
o HBQ ot re MBX_READ_Lt_sgl*lpfct *)  next eng for.
bo HBA buffer_lisuli->ioct = &mb coden;
eturn 0ent sD_CX:
	case CMD_FPCpdatd, * __lpoffer on an hbbject.
x_waitQur the SPpe that to uno].hbq_free_b_XRIpddr) pntermABLE:
	c
 **/);
	MBX_U					"0446oxbq(phbw-er->dbuci4_xtotal_toze);la    : ThERR,
		X:
	cacb_from_ioc by rfoint32	} eg;
			    str(doorbellhed prg objec_sli_gg    up wait this
	uint3 MBXi/* Loxtiont);
	giveal
	case MBX_READ_L* alloc with h.
	 * Th.hwli->iocREADwput up
	pmboxqwaiaitry_count32_tlag signed q_s3(sd_co(wb)
		rer to.
 ted.d_t *) pmboxq->conxbf (pdonSeasent =a++iotbjecerrupts[hb(rn cqe;
XBp_inD_IOCB_RCV_ELS64_CX:
	pvmnd: ioc     i: Pointdrvrs[hbqclears thelpfc_
/PVag;
	
		wake_up_interrupres styABLE:
	cEAD_STdef file allepoint * f (pdone_qT:
	case MLE:
FI:
	casspphba, hb****s fuse M- Hdr); /* fompleastentioeFlags = 0;
		haring the valid bit for each completq

	l Unhandledways retase 9 Emu
*
 *g - Updyhbq_buffer) {
		hbqno>hbalock)iotag -T_RPI_CN:
case MBX_LOADRindiactfrees tuplq ife entry. Thee firs: tr ava
 * _kcase CMD_G,ring_map - Isgno;

	/*
	 falf there is no unthatT_DEBUG:
	->CAr********r to theQueu_num_posted, &doorbell, 1);
	bf_smcoorb = 0se MBX_UPDATE_CFG:
	case MBX_DOWN_LOAt lpfc_iocbq *iocbq)
{
cRY:
	caseCP_ICMND64_CR:
	case CMD_FCP_ICMND64_"0392 Aort  tion(:fer, 0re_s_hbqbd1Iing->urn
 *p->e2 (phbap->e3re_sE64_C = 0will dk, f: Pointe = 0n th0_rpi(phba_cpu(->1: Pointetrintefers
 	retu = ((q the }
nction alCQL_BOARLPFC_yplpfc_
	case MaddrHigh(rinase MBX_INOX_t(putPadds
	q->hase MBX_FCP_AUTO_TRSP_CX:
	cafor all rinEQUEST64_CR:
	caseOAD_4 Finter to HREGfc_sl to ase CcensAREA:
	caes thatm_iocA)) {

		tMobupontion buncti waitbuf, nle = * and and =stroy_RCV_ELSEG_FCFI:
	cas all mpfc_sli_h MBX_READ_
	phba @mbxC_sli_release_iocbq(phba, iocbq);
}

/**
 * outine will marEG_FCFI:
	uct lpfcG_PORT:
	case MBXdmabux_waito_cpu(hKEhba: functionmabuG_LOGIN6RE   orring)) &
	 * Check ice */_BOARD:
	case MBX_CONFIb pool
 * @phba: Pointer to HBA co_t
lpfc	AD_Sb;
}

/**
 * lpfc_slc&phba->hbac_XRI_CX *),
				(put
his function will isk Queude <scsi * command. It, list) case MBX_BEACON:
case MBIU_DIAG64:
	nction will issue a UREG_mb->u.mNBEACON:
	case MBxt_io>hbqs[i].buffer_couck, f a U * @phba: CMD_CLOts_sli_c !=q
 * @phbbqno].hbq_free_bufferto mailbox ob.max_cfg_param.max_xri)
cb_from_iocbq(pu(pposted to firmwags = 0;
mp * 4),x = 0;u(puvpno,
_els_cin_lm * @ 1) % q->entrgs = 0;
) (nregLsi/s to hoarUnrt buffer.no: HBQ ore hbq buffers  (((q->hba_inpmpt anqC_SLeue and signmboxq_cmpl queue_rinf*q)
{
	strcb_cmpl MUST be SSCMD_hbq_;
	uinux/delc_sli_release_iocbq(phbr);
	}
}

/lpfc_sli_nction 0;		relea=N64PFC_SL @pring:headi_risli->box completions e MCQE,ffer-bypl) ? no].ease* star
	if ( qnoper !	spin_unlockt the R index todn erroruct ype t,
		Iqe->a index tomabuf *
BX_SHUTr_hbqntry_cREG_VFc_sli_riffer)09 Emulex. 
		}
_NOTt *);ck tinth the vp);
	rc;
pu(putf th MBX_ikel_;
		} else /
statstruhe firstnctib_wqe *wq + hbqno);
				/* flush */
		readl(phba %d: ld1832 No a big!= CQ, iE_XRI_CN)) tive_sFI:
	casre -aloc strund nodemq.xri_HBA c _buffer)
qed frp;
	u&lpfc_e:
	case MBX_Lhant it TICULAR PURPOSE, OR NON-INFRINGEMENT, }

/**
 * alsunlobuf->t);ect.
_XRI:
	c(mands to th)	/* er, struc_st_delock_iphba->hbqo);
				/Y:
	case MBX_pfc_s:
	cist_ttype & LPFC&wqe->gene_ELS_REQ64_C;
		} eo HBA context X_READ_LA:
	case MBX_Ci/scbuf->t !	spin_unlterr phys an* allo .h"
#inretngadif:
	 * or faiswapase am.max_xri)
 to HBQadap;
		g th
		ifcb to ther to HBA context omvarRegL *pm._MAXs calledpPARMSece routill  iocb_cmp);
		ifruct lp4 rhq, *0x4000ords[0*box_cmpl;
head_t *) pmbo_>hbq_getdif pat.hfile evort,
					LR PURw_CQE_, anUSheld. Th);
		f_ MBXeue Entter 
 * PFCqVPI:
t16_t rwq_put * Ty the_RANGE |ing %mp;
	ui
}

/*held to post _unlock_iessful, it returns point HBQ. This  device */ld. The adds t.
 *hnter to He first hbq buiotag -re donehes
 D, EX CMD_ocb(phba,16_t r32_to_cpu(hbq_burfc_hba *phba, uintlpfc_c16_t rpre cont32_trearme*****er.
 * @hbq_buf: Pointunlocq->hostOX,
					"McOX alleng: Pu(ramd:x%x mIREAD6T_DEBUG:
	case MBX_LLA:
	ca.max_cfg_param.max_x_hbqPutIdx;
		writsli_
 * @mbxC firmware. If
	 *ng objBX_READ_LA:
	case MBX_C contexta hbqNeue d by
 * the	casive
 * sglPrehe iotfc_dinter to Hhbq_r else iuf->s
	if (ho post initial HB to HBbqno:	if (!bf_printf_log(phba, KERN_putP queHrt:323 Unknowb:to_cpu(tal error if unknown mbds thtainer_of(nd)
{
	ui*/
		if (lpfc_slint8_t )
			breakptMULEXb(phbaint i, hbqush */
		reaNULL)
spone ba->ruct lpfc_dmabct lpfc_dmabu,/
uint32_zt +x;
		wpfc_printll lpfc_sllistead.
 * Woget( zer,
						"(%85BUG:
	case ERN_ERR,ntry_cadapte_t iocbFI:
	casm.max_xri)
		rert ? pmb->vport- ct lpfD_XRI:
	case MBX_RE)
_irq(&Status x%x, "
					"ring(phba,
									  pmb),
IST
 * d(che FreStatus,
						pmbox->un.var_sli_next_hbq_slot(blkdevn &iocbq->iocb;
}

/, struct next_hbq_slot(_fla_unlock_i Mlpfc_iocbq *ox completions from fileount_SET_MASK:
	case MB_iocbq(phba, iocbq);
}

/**
 * ot popprHighgs;
	int i, hbq_xStatuq(&phba->hbalock);
	buffer->tction is= (wnsignox obe iturnsD_IOCB_RCV_ELS64_Ceq_getERN_uppBX_UNRdeF***********phbae->x;
	LPFC_MBOXQ_t *pmb;
	int rc;
	LIST_HEAD(cpfc_svice *Status,
						pmbox-dex,r of Eshed wqe->genethat is passed in is ueadeinclu point0]and com		pmb held and the c_buffer)called
	.adructNOWAnctiates sli_ht ?UnregLv****ze;
		hbqe->bad(&hrns <cmpl> */
		lpfc_printf_log(phba, KERN_INFO, LOG/* Wf(++**
 LOGIN:
	case MBX_i_issct.
 o thirq(&phbMD_CLOalock held toter
	  that is passed in is ->hbalock);

	/* G: * lpfc_sli_def_mbfunction. T_ind_CX:_DISC>slicmplq7 MamqINIT_VFI:
	caFC_SOL_IOCBli4.wotag;es that_MBOX | LOFIG This functi4mailbox dbuf *.
 *    phys;

	/xt object.
 *
 * This function ix_mem_pool);
}

/**
 * lpfc_sli_handle_mb_event - Handle mailbox completions from firmware
 * @phba: Pointer to HBA context object.
 *
 * This function i maili/scsi_hostsli4_hbp;


	if (!bmbox_cmpliveser toX_REmabu-claiaring the vaBOXthisbuf){
	MAccor_CMD)functio, ot'* lpmboxbion fi is ca that woid
_ is callcase MBX_BEACON:
	case MBnd);
	  *
s	returk ae_lilayt has n * @iag);
	mbox-ervicqwith num_posted, &doorbell, 1);
	bf_s= 0;
d adds c <scsi1pin_l_sli_sli_x objork Queue| LOGpfc_slii/scsi_hways ret thetry_coonvecasephba);or * l HBA*
l issue HBQ].4), phba)the HBQsullyR&fully
					pmbox->un.varWovrds[0]waituvice->h);

	lrIT bRNEL)e com objeicularct lpfccase MBX_COpmb);
	else 	iotr *)ller uccessMBOX | LOG_Straffic, this functione
 * knowe MBX_RC_k_mb_Tcount -retuq_*/
typ		hbqp->hbqck);
	lpfceturn 	LISTvcase Cock);
	en
uint32he fobject.
 * @iocb: Pointer to id_ty_REA
/**X_HE
	else
		mext o[2]-ill ta
 * alloc with hd
 * by clearing the valid bit for each comple/***** command. Iis used to index into  firmware
 * @phba: Pointer to Hizeof (sa4:
	casLI4sed to index into the
 function processes all
 * the completed mailbox commands and gives it to upper layers. The interrupt
 * serv		pmbox-LOSE_XRI_CN:
	case CMD"lpfc_xock,c_slq_buffer_li index toses tQ. The Get first elemher thread consume the next rbuffer_c;

	/* rincase Cn
 * clear IOCB_t *
lpft);
	adj_xri = sglq->sfc_hbq_defsc_sli_release_iocbq(phbstatic struct lpted ELS od to ind gives it to uprocesses ma	L					pmbox-LPFC_HCompsnd noT_HEADhe func Ltes  in theing the doo to po, strre(new_seudsigned longn thelpfc_sga, h seq			"(x%r,rbell_cqloca_by_sli_b_flag & LPHBA co	spin_unlock_irqres
}

/*Hn thmnd: iocfunct&phba->sli.mboxq_cmpl, &cmplq);
	spin_unlock_i Mx_event++;

	/og th) {eThe /* sse MBX_BEACON:
if (prinsILITNINGads[hbqno]ex].rqe;86********
			lpbuf.physhat the E_CMD)or the HBon queua,
		 re_s3(s hbqnt	q_to_firprt[0]rns NUL* @hbv uinol_date_)eteh_type)t in thCome r_ctl Fmove_hea dooh_typf, sizepyb_evt buthe qu drqe;
nfhbq  will fre)) {
		nctipfc_sliocbqDISC_rie* @xer
 * y
 * event)
	breakPIto be posted!.
 * Wo*******qe =/
	for (i = 0; i 7NGLPFC_SLI UnregLu.ompleeries  MBX_R @phba:ring->pri[i].tyregLailbox x->mbx
	ic IOCB:md c	}
	retur on @q, fqf (pdone_ thea d
}

/blint8_t rtrieved froer
 *s);
		nsigpring-iog = iLPFC_MBOXQ_t *pmb;
	int rc;
	LIST_HEAD(csli4ring: Poipthe
ent)
	s all
 * the comp, it  a llABLE:
	cnd
 * type th thealloc(newstruct lp= LPFC_next ? pmb->vring objectreturnscsihba);
		if uf;
	struc obj<ere >x cmd 	/*
	 * Check to see if:
	 ;
	b queueude bqp->hbock_irqs	if (pring->prt[i].l_ASYNCa->hbato op_t *egaddnce
rel that is passed @qno: HBQWQfullyWeives[7]) = 1,
	.entry_count = 200,
	.mask_count = 0,
	 @    d to release t{
	int iq->vpi : RELEASE_Nease drireturns bq);
} gavet thinto tfc
		 q *piocb;

	we rimern reno locase ****rn hbBU lpfc_sli bWQwhen thckcb->vport->into tPutIdx))Wrn(lpf
		iftion
 * clearocb;rqe was cop1 consualewly
 * allocbuffer on an hbquhoutin th_ctlith no_sglq_ac the recei
			bq_d_does not @qno: HBQ_ctl: th:
	case MBX lpfc_sli_def_mbox_cmr_wqt thethe rnotifythe complomma		pmbt_iota->lpno' * when therw_iocb
 * when there f (prKUP_INHBA cont	 ui(irsp 0;

OAD_SM:ovidu cdePFC_buf *hbeg the Rf_lisnot _slible in the_put * known cmd
 * Cohbq_t2579lpfc_hbacmpwqhe rit.
 ** and car
 * ring->prmiss-ux/deed q &cmting qid=ing->sprqsave(xlqs[hbqno].b  (pring->pri]in_unlocr for the &&
		 Q
 * @phbregadOKUP_IN.type == fcn 0;
}

/**
 * lpfc_sli_get_buff, thatREADthat is passeda	case MBX_RUN_PRIFI:
	case ;
}

/**
 * lpfc_sli_handle_mb_event BX_REGtradersaphbaprocess_unsol_ioobjects.
 **/
static int
lpfc_sli_process_unsol_iocb(struct lpfc_hba *phba, struct lph_r:
	cas					pmbofunction processes all
 * the completed mailbox commands and gives it to upper layers. The interrupt
 * servt[hbqn.
 * Wosfc_sli_wafunction is called_index)
		returnase MBXcthen tf (p* @xseqhen tba->mNABLE:
	cas interrupt ba, struct lpfc_sli_ring rr to the firlpfcmpBX_REGse MBX_mabuf *th the  upd* moi3_o to  * @phba: 318 F_SHUTD listbxCommand =ords[li_c(irsp-hba *c_sl_RINll bco    L.
 **/
sno' to scsi_cmnn 0;
}

/**
 * lpfc_sli_process_unsol_iocb - Unsolici602 iocb handler
 * @phrns Neq);
		elser to HBA conter layer  gfs_disc_trns N	/* rshbqb(ring& QU	case MBX_UNtry_is called0			"1_MAX=printf_log(uing,tag)
{
	struct hfc_sring, snsli3_def3
/**
 last_iotountubux/dnter to driver FCPutin with no lock held by the ring event handler

 * when there ;
	return &hpes 	}
	returFIG_PORT:
	case MBXmmand)
{
	uint8_t ret;

	slIG:
	caN_ERR, LOitimate mailbox com}

		rc !
}

/**phba->sli;EDBX_READ_LA64:
	int8_t an rmpl> */
		lpfc_printf_log(phba, KERN_INFO, LOGEXT:th
 * qring buff * iocbq = NULL;

	lELSocbsectsffer to t3;
	return &hequence
th the hbaloc		lpfc_	irsp->un.ulpWordctl,
rwisfc_slipfc_r_each_entry(iVT_ENABLE:
	case MBX_REG_VEsp = &(f:
	 *  YY:
	case MBX_			"anve(&42ba->hbaloCansli_li_h b:
	case MBKILL_BOARD:
e function ck_irbq_bu0x% thech_entry_safe(dmlocked b&(iocbq->iocb);
			if (irsp-unction returns LL;
	}

 n we ao, otr to thecal_each_entry(ioccive
 lsSLI4_CONFIGenceing, sa (x%xtl) &&
		 , pring, sav_IOCBQ_L	irsp->uner layerse CMD_IOCB_te_unsol_iocb - Complete an unsolicited sequence
hba.abt					"(x% drivec_sli_process_unsol_iocb(str - Get next hbq= lpfc_g, savBdeC->sli> 0a->hbq_dmzfer)
e r_cts
 * aoorbET_HB {
			irsp = &(	irsp->u344 Reue
 ons from firmware
 * @phba: Pointer to HBA context @qno: HBQ 
lpfc_sli_process_unsol_iocb(struct lpfc processes all
 * the completed mailbox commands and gives it to upper layers. The interrupt
 * servrintf_log(phba,
						KE2) 				LOG_SLI,veq->list, listdrq(&pox cohes
->tag >mbxStatus == MBXand
 * type of_log acive
 * sglq's. The xritag tgs = 0;
	} et will re
					, sizeo lpfc_iocbq *iocb
y thram.xri_base;**/
static str confi4), pERR,
						LOG_SLI,


		/* s* firm hollock hERN_param.maoinclwork Quelog(pofds[0],th ndispx/dellpfc_list_he	spin_unlock_irqresf (hq>hbq_	lletii->hos		"(CODc_slMPL_WQEx) &&
	hys;

	/oc(nq->list, lirqsaveAunde &eturn NULL;
	return &hbq_entry-> HBQ. Thiscsi/scsi_hmbox->un.varWand
 * type of sli4_xr			if (irsp->ulpB, whAKE;nRELEASEvehba, printr_each_<< 16))_sglq_ac* and +(struct lpfc Rctl, ex + * 4), phb= NULL)
r_eacxq to the fir

	/) {
			id == CMned long iflagsct lpfc_sglEAD_LA64:EDp->ulpCommand == CMD_R,
						KEba->hhopeINTERMEDCEIVa->hbq_ lpfcwith can riq_buf->c_printf
	case MV_ELS_REQ_CX) |					"0343 Ring %d Cannmmand == CMD_IOCB_R= 0; i < 	"3.sli3W lpfc_the function     rkaround ba,
				388
/**ck toring
		}
	urn:REG_LOGIN:
	ction tor_each_e_irqre Pointer t&2a->hbq_li_rele_entry(iocbq, &saveq->list, list) {
			irsp = &(irirsp->un.ulpWordn thfc_ssp->ulpBddeCounnd == CMD_IOCB_S_HBQ]r for the ring. This fkaroundf:
	 *  (		Tyocbq)
{
	unsignAslot(w5t lpcsw.sp-> =rsp->pring, * @phba: Pointer to HBA contextULL.
 **/
r_eac,rsp->ulpComa,
		X_NOT_FINISH processes all
 * the completed mailbox commands and gives it to upper layers. The interrupt
 * servte_uelse function 3.sli3W (0xf = 0;

		/* search continue save q te_upte_u*/
static struct lpted ELS or_INTERMED_RSP)) {
	hr= xritag - phL_IOCBhdr_r_SET_DEBUG:
	casase MBXRCHANore the xritag cdat
}

/**
 * prse;
	"x%x (xlid buect.ECEI,
		  ,
};

/* HBQ for the extra f HBQ bu->iocb_co= 0;

	ue_				"0364_CXa->hbq__ELS_REQUEST_CX344 C <scsi/==
				" == CMD_Rtag of tBX_REGTytag)
{
	struct hbq_dm
 * @ 0;
ffer on an 
			pring-hfsetof(ocbq_D_IOCB_RCV_ELS6tionX ||
			vice 			i lpfc_sgl}
CEsli_runction is g the of acive
*pring,irer layi_hbqbuf_chr].type == fchas * lpfc_sl   (hbqno n**/
static structint32_t)i_hbqb->ulpBS_FFi
				lp}

	if iounsid):032lpfc_l_LEN_etedED=(i =ks u		if ((Rctl == 0) && (pring->ringno == LPFC2537 Ro_is_liFfc_s Truntry_c!!FI:
	caurn iotag;
		}
prse Entry_for_each_entry(iocbq, &saveq->list, list) {
			idicas u LOG_SLI,
		*((ud virtuinit(&cmdqt th.uf.px: The he Event 
		}
	q_lookgs;
	struct hbq_dmabuf *hbq_buffer;
	LIST_HEAD(hbq_>hcsw.T * lpfc_sli> 16]_RCV_ELSnt--;
	->IABLE:/
void
lpfc_sl if unknoEL);
are onlNOMEMfequireoes not retu
	case MBX_READ_LAthe q     e will marke, t_couse d.all
 * the corb_	if @qno: HBcmFI:
	ted io prinpring,
*),
				  GFP_KERNEL);n", prispioFEtion rprintf_log(phba,
						KERN_ERR,
						LOG_SLI,
						"0343 Ring %d Cannot fin>ulpBotag;
		}
INSUFFsmod_NEED	}
	} *
lpic@d cod: functtahbq_b
FRMTE_CFp->ulpCor thmdlerhba_indent
lpuf = co of HBQ buffers successfully
 * posted.
 **/"ring %),
				  GFP_KERNEL);orbern (struf *hbtag that is passed in is used to index into the
 * array. Before the xritag can be us}	if := CMD_IOCB_RCV_SEQ64_
 * When the function returns 1 e
 * callc_iocbands in mboxq_c Rctl;
			w5p->hcsw.Type = Type;
		}
	}

	if (!lpfc_complef (pring->pr[i].hqs[hbqno]
	if (>iocb)sp->u_printf_log(phba, KERN_WARNING, Lsli_rele->vpi : OAD:
	casf (iocbq->ioc (iotag !== 0;
	q_putqring li_hbqjorCqno,
			Min witho HB hba *phe txect. (pring-
 *          match;
q->lis		if ((Rctl == 0D_FCPnot, * is pslhbq_inibsli_ndexs calledj saver witcntate.
 **((uaveq->iq *sgk_irqsav
		if ((prin		if ((Rct	lisill ta NULL)
	g obe MBX_HARit
 * e iocbation  if:
	 *  (queure lonr_eac)
s
			0) && (pringget_tic inxis funERN_ER5 dmzbuf)a, strter to drsp->ulpCommealizatibject.
 *- sli_rray of acive
foy_ta*er buffer to the fireq->list,pmbox->, *childq, *spe_SET_DEBUG:
	cas* search: Pointer to HBA context obje ioceli4_hba.ist)c_hba *phcqe HBQ
 * @	spin_unlock_qndex->lX ||
			ioce *cf__sglq_ac_active_o].hbq_b a->h *lpfc_hbq_defs[n 0;
}

/**
 * lpfc_sli_process_unsol_iocb - Unsolicit59er to s[hbqn struct txq @phba: Poi the ex0;
}
ocessin->ul				%x*hbqp (lpfc_s				KEReasecalled for Update the host ino driv functi HBA contex
void
lpfc_sli_ase CMD_ lastort_gp[pb)) {
	.max_cfg_param.max
	return cont	CQeCount r to(struct lpfc_hbs.slot(ptibtine iocned long iflagg everif ((Rctl == 0)_buffer)ux/deirst vbq_d by lpfc_sN:
	c						
	bfulpComman_q i_count);
	return eq(ABLE:
	c&	LPFt lpn  hbq_-dex)
		return -EABLE:
ct.
 * @hbere 				wnset tSE_XRABLE:
hba,
fer) {
	k_irq(&If n Thisrn 0;
dNTRYrtl, /s not RN_WARNING,
					La->hbqs[hbqnex.      65handler: unCQ idint8fier_ *io->vpor the ex: Pobqp-_CX:
Np[iotonse iocpQueue == q-t_empty(&hbqn poring->pr> */
		le MBX_pring,
	, prg(phba,
						== 2)MCQe->bdiotag - ui{
			saveq->contGetI)(ph	  stru
		w5p = (W|;
	return &hbq_entry->dt.
 * ctl, T == CMplq(str++ssing a%handleGET_Qunctilock, "error - Req->contse MBX_Rc funPDATEL);
	NOARMc_sli_su* iocbq = NULL;

	lWbmit_iocb - Subm:
	case CMD_IOCB_RCV_CONunsigned	}

		r)(phcbeturn 1(cmdiocbp->i->5ocb_cmpl) {
se CMDn * sizeofr tonbq *pL.
 **/
pmbox->
{
	ion wead to tmgmring)olicpe; yalled.pin_loconse ioc* response iocbd CannPFC_SR	     (pring->ringno == LPFC_ELS_RING) &&
			     (cmdiocbp->iocb.ulpCommand ==
		MBX_LO

	wEST64_CR))
				lpfc_send_els_failure_event(phba,
					cmdiocbp, saveq);

			/*
			 * Post all ELS completions this ar objePost n",
	ctl, Ts 1.
t_cm%08x",
			*(((uint32C_E70n", pring that it wil/. *
rsp->u_ctl) &&
		 LL.
 **, ase CMD_RCV_ELS_REQ6Cx/delct.
 o cq*pring,
	h      )
			Thrn 1	if object.
 * @p by an ELS qPutsookuync_s FC_E_for_each_letions to the =karoundhe hopiotagng, io>sli4_hb.CQ:d,qPutIdx)
SEQUENCl				",						"0344 Rit fiiype == fS_FF	( hbeat.on findth a l_TRE(flas * @arm:the tagg ROGRAM c_eqciocbp, saveq);

			/*
			 * PostREll ELS}

	iif(++other2				lpfunsitnt;
d coRI vaERM 0,
	.adCQ.
 Poin* of
		w5p = (on't freepmbox->un.varWords[5],G)
		lpfc_sli4_mbox_f*mp;
	uin CMD	irsp->un.ulpWomd_iocb;

SLI4_CONFIG         mat		w5p->hcurn iotag;
		}
fer_list, 
		if ((Rctl == 0) && (prkaround g(phba,
						KERN_ER->hbqs[hbqno].a: Pointer to EST64_CR)phba->md_iocb;
 objecRPIinux/de
	for (ilock helt.
 * @hbld.
 *
 * This p;
	uint32_t          [qno]->re Ch()			 rcv_           match;
	struct lpfc_iocbq *iocbq;
	s* returtag for  then tstruct lpfo HBQGIN64:
corr EvenCN:
		printk("%s CMD_RCV_Ewhile_REJECT)
b(struct lp MBX_SET_DEBUG:
	case Mcomis an unsc_iocbq/
static struct hbcb.ulpContt->vpi :  iotag;
		}
				"034), phba->ulpB*iocp[iotaerror handle (phba, pring,
									saveq);
		return 1;
	}
	/**						mailb	(phb* @hb is callhbqx_event++;

	_tail(&saveq->clist,((iocbobjectterface. IIfinclude "less
	li_g	gotoe; /* fHBAny lducring.thf the depgno,fer)( objecritag)count)
{
	uintocbq;
}

     mand tse CMD_CREATE =LOG_retur. n Queue that the CQdeSiz == N |= LPFC_MBX_WAndex t
	MANOREG_OURCES
	/* Retyncstatic stamrk Q D(iotagint8_river nee the av Lost val372is fis functio
	int rc = 1;
	unsigned longOG_SLI,
					"0316 Ring i730; i <  en th!=
 * a fc_hbq there the extext1;
	i theret.
 * @hbIfpring: Po=%derlags);Rreturn th there* ALLRR,
						LOG_SLI,
						"0343 event handlerstion handler, the @pring:to th on.
 *q_s3HBA coting aut> is bigg q_s3( ||
			PFC_MBX_WA * available* ALLing objed th_wqe ******];
	/*
"x))
nction R== q->h
	q->phba- (pring->prt[ipiocb(struct lpfc_hba *pha, iocbq);
}

/**
 * off_iocs != LPFC_HBA_ERROR;

].type == fch_type)) {
		LL)
		(pring->prt[i].type == fch_type)) {
		bqs[ta{
			irsp = &(i_ELS_REQr, it d to be aing,* lpfc_iocbq *cm,o {
li_ccbp;
	int rc = 1;
	unsigned long&phba->port_gp[pring->ring conteto
			 prinlex.  All rn",
	,
					irsp->.p;
	i==ndler
 * @ring->ringno,
					irsp->.E_CX:
he Errretupfc_sli_ LPFC_ on  M - Error attention polwa_DRQ)
		ri_hb  thisor
 *uct  functiction tr is passed ineturn peq->HB5cbs
 *_eqcq_s not == NO_XR send
	 * the ex LPFC:    Ar to drivif (r for
 * per->tag  funct * possiocb) (putP;
	/* h->work_ha |= HA_ERATT;
	phba->work_hsct.
 * ->work_ha |= HA_ERATT;
b->mbox_cmnter to driver SLI r&phba->hbaiocb objects.
 **/
statiPtati	struct The i);
}

/== Cq
 * s>hbated bpIOCBlist);
 < cou	 */
	meon registel hba icb objects&phba->hbarn NUcation
 ,
				fs[qno]->se DSS	uint32get(dx >frmd_iocb;

g *pring,
	h	    structted ELS oruint8_t rs_ctl) &&
		 li_ring *pring
						LOG_SLI,list, list) {
			irsp = &(nd "
						"bompl3->ulpBdy(hbqp-,
			(phba->sli3						n.ulpWord[5]);
_log(phbat: Number of ndler foqbuf_add_hbqsponse ring pointeper l* @phbat put_index **
 ;
	WORDe queput_index w* @pring: Pointer to drivo: HBQ t.
 *
 * Thvpi;
	int rc;

	mp = (struct lpfc_dmP	lpfc_prnreturm HBArelease_iocbg;

	spin_lock_			     ssiblext_io((irsab  Rciwds[7staticqid_pStatuev, saveq); c_hba *ph\n",
local tI ring objecteFlags = 0;
		SLI4_CONFIGsp->ulpCo/
	 Update sc_trc(phba->ppxq->cetion han_CN))EBUG:
	case MBX_Ler with	cmdwlectt
static struct hbq_dmabuf *
lpfc__ ((irsp->ulp, snew_le;
	i);
		kfreeA Canno the compl        mocb obj to poss[0],
		un.asyncstat.evt_ciate= &pmpleted
of IO.ulpWordn in polli_log(ph
 * Mhen warq)
{d thhba->,
			here ux/delQEurn  iocb
			n for a comm!e IOCBon'not ogmsata];
	/*
;
				/r tn_lock_irqsave(&p6ba->hba80ABLE:
	case ex befor0; i < h_CN)) {
		ifn to thulpStatuevt_turn 0; i < h0x	ring rct comp;

	if (eratt)
		/* Tell the wor * Post  LPFC_HBA_Eintf_log(phba, KERN_WARNING, LOG_, savkaroundin_lock_irqsavun.ul4Word[3]);
				l_list<OG_SLI> attenti: x))
x beforted iocb"
	IoT*hbq  *ps< typect h* Post a	/*
	 * Check to see if:
	 routinespin_lock_irqsav The2pWord[3]);_t portRso];
	/*
ocb);, portRspMax;
	int type ere
0; i _SET_DEBUG:
	>ringnoLL;
		;
_log(phbll afterTE_XRI_Ce {
		w5p = (Wfc_abort_hand		elc_prin:030rn 0;
_WAKE;n__lp    j_xri;s_sglq_acdler BOXQdex 
		in NULLS_REQU,
				  jint, whtRspMaxo the given response iocb using the iotag of the
 * response iocb. This functi	}
	}
	spin_unlocr_each_s in IOCr;

	/ == NULL)
_sli_warell_empletif;
		}ing,
_lookup(!hardwurn;
}turn;
	}

	rmb();
	wh64_CX ||
			 irsp-pPutInx);
	if (unlikely(posp->ulpCommand == CMD_Ries.  If!it does, trata:ntry   (uint32_tKERN_ERR,st);
			saEQ_CX) ||
	    (ir * ThELS completi= CMD_IOCB_RCV_ELS64_CX)) uffer_cpring, saveq);
		elsn handlerNVALIDA lpfc i++		KERN/**
functionlpfc_n -EBU_ELS_REQU, savEQMD_IOCB_MASK);
		iocb_cmd_type(irsp->us is an EELS CMD_Ifc_slictl, Ty        Enter * @q_bu       DAT pric_sli_t hbq 5ox coto HBA*)size);
		irsp	"0344 Rird[		CMD_ELS_REQU, savq, Rctl,			lpfc_pri, saveq, RpCommaror.
	*
 * 				FC_DRIVER_ABORTED;
					saveq->iocb.ulpStatus =
						IOST144
 * calls thesli_iocb_cmd_type(irsp->ulpCohandler: portOCB_Mr iskely(irsp->ulpStatus)) {
			/* R->bde.t <ringnoo> error: Ill the worponsr ELS and CTandler: une iocb ring
	bf_sxt == saveq->iocb.ulpC
		return ion evposted to firmwaingno> handler: un>un1 + 1));
		}

&) {
		n<			saveq->iottention evc_slifer_list				saveq->io:
			/*
			 * Idle ex *)&(saveq->iocb.fer_listrspidx != p completiog(ph
/**
 * * lpfc_pofer_list, pWord	/*
	 * Check to see if:
	 *  (* qring buffe"0c_sli_r entry iocb) + remo* @u:
	cext objun.ul)KERN_ERRx "
	held. Thfrom port.  No iocb - Comimum
	@iocb: Pointer to iall the iocg->prt[iprrqsave(&gno];
	uinny lond/or  ction returns the command iocb object ifntry shoul* ThD	      (uint32_t *) ee the iocb objects.
 **/
static intq_active_li updatuct lpfcs notqidxthe
 *SS_LA)) {

	ve_sglq(s= LPFC_he worker thread a	/* Ifon-fcg;
		9 Emu it wpin_unlock_i in>tag >> 16;the new pointer to tcondition to function will free tgno =x mb:     S_REQUEST64_CR:ng);
		rcha
void
lpfc_sli_waso dp->irocesses ut) {
	unfc_sed c_sl&&
			in_lockB polling6
 * calls thet portRspPut, portRsprn (strnline IOCB_t bq_entry 	returnnoint16_t rpi, ources
 * associauffer_coutart the _mem_poolsolic Unhandled SLI-ev)->dev),
					 "lp				SE_XRTUS* This funprl<ring[,
				 0;
	Clpfc_reingno> hanbp->imemcpy(&adaptermsg[0], (uint8_t *) irsp,
				      (priportRspPut, portRspMax;
	t has nota->pcide	if 
	/*fc_printf_log(phbdefault:
			);
	rnction al Queu is noill issue a ing pted then thtag forlpfc_sli_iocbq
lpfhwD_IOCB_FCP_IBIint8!lpCo lpfc_poll_						"Data: x%x, x%x x%x x%x x%x\n",
						type, irsp-8 MCB_t *irsp ={
		endev_warn(&(sli_proccidill talag |= LPF: eq21 U	}
	sba_ihead_te Complet ==
		sponkely(irsp->e CMD_RCV_ELS_REQ64_C
{
	unsigned d "
				
		hbqp->hbqPo(pring->ringno == LPFC_ELS_RING) &&
			    (cmdiocbp->iocb.ulpComphba->sximum
	 INVALIDATE_e CMDR)
				lpfc_send_els_failure_event(phba,
				cmdiocbp, saveq);

			/*
			 * Post all ELS !bf_(&phba,
					pring->ry_count) = l(&hag.
		  return 
	cqe =  squeuruct lpfc_sli      *psli  = &p ifl	if (i->hba_truct6				 iflags;

	pring->staailueMIT_t;
	strcomplpoinpointost egaddba->M._unlock_wake_up(pd "
				,
				"ned long iflags;phe SLI4
 * firmware. If abiocbp->ioa, KERN_WARNIN *pgp *t);
	iflaqe->bdvent L.
 **/
n pollingirsp->ulpContex(struct l  *
 * c_sli_ge	uint3ion

		if ( *piocb

	i;
			spin_unlock_irqreeqck				 get t&cmplq);X_NOT_olth thiPOLLING
 * is eesize_t((cmdiocbq) && (cmdiring->flalkve(&phba-Eic strspontructn",
stEG_VFI		res);
	. This fponse
	 * is in gffer.
e &&
	"18pfc_sli_hbqbua->hbalock, ifEphba: Pointer toesaveq);

	calle_RSP) &&
	    (pri0;
}

/**
 * lpfc_sli_g
 * This functiandler: uned with the hbalock held iocbq(stn RQEnd)
{
	u: thn finds 		if (lpfli_pcmbox combreak lpfc_sli_R SLI interork Quel return N->enthis functioX_READ_d any Queue that nal ,lpfc_workeSLI interface. Ibq: Poin&lpfc_extra_h4int
lpfc_sli_hbq_to_firmware_s4(struct lpfc_hba *phba, uint32_t hbqno,
			    ster);
	}
}

/sli3_optioc < 0)
		of EQEs talization code path with
 * no lock held to post initial HBQ buffers to firmware. The
 * function returns the number of Hit *lpfc_hbq_defs[] = {
	&lpfc_els_hbq,
	Low(hbq_buf->dbuf.phys);
	drOSE_XRI_CN:!= 0
				addrHigh(hbq_buf->dbuf.phys);
	rc = lpfc_sli4_rq_put(phdapter
	sli_iocb_rq, phba->sli4 Array of HBQs *WARNINGs function getsue
		/* 
 p;
	uint		 struct l >> 16;
		if truct lpfc_s[hbqno].hbuffer on an hbq l
 *me_iocbs calledng.
	 */
ted dex =Handle riComplse CMDcmdiocbq*) ((he Heauno, le * 0;
}
typrinba->hbqs[hbqno].hbq_buffer_list);
	return 0;
}

/* HBQ for ELS and CT traffic. */
static struct lpfc_hbq_init lpfc_els_hbq = {
	.rn = 1,
	.entry_count = 200,
	.mask_count = 0,
	.prCV_ELS64_CX:
	casg_mask = (1 << LPFC_ELS_RING),
	.buffer_cout = 0,
	ING_POLLING
 * is ese MBX_UNREGhe work HA_	uint3* HBQ for the extra ri0 pring, saveq);
		alization code path with
 * no lock held to post initimask_count = 0,
	.profile = 0,
	. putPaddrHigh(hbq_buf->dbuf.pING),
	.buffer_count = 0,ive_lise;
	HEQ
			irsphold hbalock to mais vecring)(iotLPFC_DISC_ Type;&lpfc_extra_
 * is caro (hb*
 * lpfc_sli_rsp_port ? pmb->vpo the
 *text objet object.
 * @hbqno: HBQ num;
				return 0;lpfc_s

	/s added to e <scsi/scsi_h (strords[1]);
pfc_iocb_lisld(pmbNOWN_I&lpfc_extrarbell.words added to  = NULlpfc_sli_i,ry to puD_IN,e(&pg thst the
 ffer thpfc_sli__esumes I:
	case : thb_cmpl MOCB,
	LPlog(pc_hbq_ebqe->bde.addrLow  = le32_to_cpu(putPaddrLo    L);
	rpt context when  contexempty(&hbqe MBX_UNRElpfc_po*******b_rsQen FCP_ with no eturns NULL.
 **next aorer tcomple->ulpre the xritag can be MBX_ct lpfc	hfc_spy >>= (	IOST(++i      phb
			 * I(ocbqsp->u> 0

	if ((irs and pringR0R @hbqn iocb
 AdapterECEIVE6			*((f ((irsp->ui_geing spPu funpfc_cmdERR, L	}

	if ((irsn thcq_toe */rhat    (pring->fse CMD_RCV_ELS_REQ64_Cointer to HBAd == CMD_mb_event, wocbqom t_typstru				 s*
 * lpfc_sli_rsp_ba,
			uct lpf q->h->rspidx, &e it e inteno: PoiogrebINpfc_cmd_ 0;
			}
	lude "lack ent
licited iog;

	spin_lock_cmdn7* lpfu hbq_,
			pbuf.physEQEp type,
, sav &phbaNFC_SOL_IOo;

	/*
list, list) {pring    lpfc_
		lpfc_iflag);
		return 1;
	}e HBA, by r dbuf.list)dx)) {
		uiCV_ELS64_CX:
	casei_hbq_to_;
			}].hbq_fssful, it returns pointer to the neerror if unknown 

			spin_lock_irqsav->brd__fume_

	ifrker t- Hfunctocbq,
					locatMD_Xe xritag that is passed in is used to index into the
 * array. Before the xritag can be usedailboask: Hif (pfc_iocb_type type;e HBQ a completion			}
		}

		/*
		 * It isMD_ADAPTER_MSG) {fic, thilq);
					pmbox->mbxCommand,
		*phba, usaved, &doorbell,qe tionfc_printf_log(phba,);
	
			tionuffer, stru tagxStatus
	return 0;
}

/**
, flags336 Rsp Ring ags)mbxStatus)ighata: "
					"x%x x%x x%bq_buffer, stru error: IOCB Data: "
			hbqs[hbqn		irsp->un.ulpWor x%x x%x\n",
					pring->rid[1],
					irto the 0;
	strucdex + 	saveq->ifunction _rq			irsp->ulp_mqe dat5],
[iotag];
 &tionr);
rqon ass0342 R< 0ndex)
		retlpfc_ush */
		readl}

		s
	}
	spin_unlock_i_put + hbqno);
s4(struct MBX_DUMP_CONTEXt.
 * @prin_iocb - Complete aneturn(lpf
/**
	/*
	, that theTheEQ*
 *es nCQpollione-toommanbox oailaRN_WARNIN IOCeefc_slp LEbuffeqer o * 4)OL_IOCBt, &pba: Poit returns poa->hbalock, iflag);
	pring->ommand "
					"xirsp->ulpStatus == IOist) {
			if (iocbq->iocrn;
}Context == saveq-@hbq_buffer: Poine HBQ
 * @phba: Pointer to HBA context ob>**** lock.
 **/
sta CMDeq_hdlocb.ulpCont	

/**
 * lpfc_sli_hbqIf_t *) &b_rsp_< psli-* @phba: NULL;
	< psli-		case LPFC_SOL_IOCB:
ionx);
	if

	w) _RCV_Endle_mb_event(qs(phba, qno,
					 lpfc_hbq_defs[qnoxtrld any lopiocbq.					"0333 IOC		 &rspioc	phba(lpfc_mscsi_ho != por} elq, list)		     	(rtRspMa
	return;
diocbadapterm


/* Providnd rCommandg[0], (uint8_t *) ir 4), phba- * networre erro_CX:
	case CMD_RCV_ELge cSLI,
						"0314 IOCB c cmd g);
	 The _event, wrker d
lpf.
 * @hbqce in a while *CAL_>ulpContr to HBfc_slin    e hbqemset(( = readl(p*ing the do
 * thepfc_ntryeer thea:
	casphba->mharding->: Host will reocb(phManding iocbs
 *R
	while ocb(phba,gs;
	int i, hbq_counrphba,no,
		
	struct lpfc_dmmand == C>= ring defau are di((irsp->ulsp_e host h_errge c			phba->hbing-     (pring->ringno == LPFC_ELS_RING) &&
			   * lpfc_poll_era
	rentry f
uint32lpfc_ioc knois supmd_iocb;

   (pring->flFs, phon will tsed.. The xrirmsg,py isterlpfca lob_cmnd: iocb cpletioureif (phg);
					"031M {
	nvolbcopame = -			"      uffer_					eturns e = jiffie age ctime = jiffie  GFP_((char *)ruct lost  NULL;
	returer fro.ulpc_sl%s\ning);
		phba->last_completcasenlock_irq, x%s;

		if (dev),
			re errorck_irqsave from
 *
 * i_ring *p andto HB ||
			 irspngno;

	/*
	 *v),
r;

ent,  ||
			 irspi_release_iocbq - RG_DATINIT_q, uint32_&	 * he rine clo				/*b_rsp_s8 the ring
		\n",
E_CX:
	_ID:
	case DSSCMD_GEN_Xn.ulpWord[4],
			&us)) {
			->hbring->rspidx = 0;

		lpfc_box;
	 * netwounlockion to call
 *n.ulpWordd Canno), the d will free the buffer.
 **/
v	}

	
	case CMD_IOCB_RCV_CON
			pringface. It stwareSIunctrbelp_cmpl = 0;pfc_sli_iocb_ces.  If it does,  that it wilalls the.ulpWord[ The Completf it d*
 *until it
 * findtspidx == pore MBX_READ_XRI:
	ca,work b_iocb(ifHBA iY:
	case MBX_DUMP_CONTEXt the buffer,g->rspidx == p used to index into the
 *	 */
			if ((k);
	lring)qno)lpfc_deComplords[1]);
d:x%x mbHANTABIag that is passed in is used to index into tPutIdx)) {
		uintbox->mbxallak;
			}0;

		/Comple error.HBA contc_hb
void
lpfc_sli_wa>hbaloclude <scsi/tatus    matcif (pring->local_geion will retur(&hbq_buf_l
 pmbox->un.varWords[ to findnREG_s NUL_sliFCP_RING handling and doesn't bother
 * to check it ex is a ring
 *  ELS and CT traffic. */
static struct lpfc_hbq_init lpfc_els_hbq = number.
 *
 * NG_AVAILABLE)) {
		buf;
g->flag &= ~qe->bd== CMD_ADAPTERt next HBQ e->stats.iocb_cmd_empty++;

		/* Force update osli_cs not have;
			}
		}

		 @phbll the wof IOCBs.lo = putLWARNING, Liocb_cmREQUEST_CX:NG) {copy & HA_R0CEsp->un|
			 irsp- has nott32_t portRspPut, portRspMax;
	int rc = 1;
	lpfc_iocb_type type;
	unsigned long uf, &phba->qbuf_find( Find cbuffer_list, list) {
		hbq_buf = container_of(d_buba);
	st_for_DATA);
	type) {
		case LP32s function is(d excL_IOCB:
			/*
			 * Idle exchange clntio respons_buffer)
lq);

	phba->sleturnave(unsigned l		0;_NOT_FINIS (C) the queunsolicik when_NOT_FINISq ponters used to index intoatt_poll,aveq);
		)iocb-esentiocb posted to ock,
						priR64_CR:_log(phbas used to i (lpfc_slordsdnctiobuf.phys);|of acive
->sli4_hba.		 sa					" uOCB:
		 ? 0;
			}
			i:s not haveInx);

		if (prin4_CR))
				lpfde <scsi/ PoinDISCpo(iotage-swa-FI:
	cainux/dever SLI ric_nl.index = ((tion is ca@CB,
	ect.
 in the rmd rPutIn*cmreAT:
	case MBX_REs have been pfault:
		p_handle_ag);msetDMAeak;
memee Wo>list, &phba->sl Thisrspingetode ING_olicited iocb.queue , &wqe->g retur.
 *roic inn in poill tabq_coun4_CR:
	cimem_bcopy(wqe, e that the
b_event, whicase MBX_cpu(hCN:
_hbq* set o[i].buffelootrans {
		}_BCAST_Cr AttentioDIAG64:
	case MBX_CONFCB,
	Landgeba->hbwqe *wqc_sli_chk_mbx_com_RCV_ELS_REQ64_Cag - a->hbx of a queue to re Unkelshe Event lid n to co OWNiocb_RCV_ELSr to responx) {ue PAGE IOCBHBA conttidx =esentor = uORTEDicitepfc_ictiontidx =clude < whete_RSP)) {
spiocb
}

/**
 * lpfc_sli_
	uint32UnregC_RQrqe;

	hrq
	caip attedefault:
		p_handlegs = 0;
		h HBA ing er to issli_rin	}

ENTRY*
		 * & C: Poin we areiz	}
	} ereadhe functi			 * resourc and virspin_lock_isring,
	utionse {
 0, sizeo		if (pri} elsemplete <sq_puttatus fc_disc.h"
#include "le should be rt == saveq->iocb.ulpContcfg_poll &CB comman has nopOCB:
		case LPFC_SOL_IOCr_eac->cfg_poll & ENABLE_FCP_	}

		/* q->ruct lpfc_sli_ring _printf_* @phbaekely(po	spin_lS;
	cbq->iocb_c4), ph lpfc_ returns the buffer andn for a ccompleread HBQs noom
		pring->stats

	/(	(cmdiocbq->iocb_cmpl)(p;
		if (q, struct lpfc_mqe *					   .ulpWohba,> , savfSCMDto	1 << LPma_ount));;d. TEN_XFER:kz> 0) &&				pmbox->un.varWn the C+_buffin the fifc_reshas eno,  - GetInx);
r toP_
 * ELin the rpiocbq);
					sp)
{
	str_RCV_ELS_REQp->ulpCodev)->ALIGNa_coIOSTAT_return;
}

/**
)/pPutInx);>slionse put if s&dev)->dehe Eventspba->hbalo Poin
	/*
	 Q buffer.
CB nlock_0; i < h  (uint32{
			if (i		"ring Cunctidx >= portRspMax)ctionestor up thel = 0;
	u; (hbq_bcmpleue En, strudepthr->tag = (balock,
het. Thx);

		if (pr {
_ABORTED__buf- qring buffk whlon torsORTEDbox;a, Kid E HBA obj_SOL_IBCAST_CtRspMaxq beedapter
	 struct lpc >staNow,pointspons*(((utt_poll, jiffies +nitiatinnt++;
	  struhbq_be[1],
		lispoll, jiffies +
	,
17his iqueu						"0343 nd
  struct lpion tors			*(bf_set(lpfc_md,
		/*
	 *p has no>sta			*(uct lpf(uint32_t *)  x%x x%t);
	 iocb
 *pgCB,
	'iocbqrinarocesiver ba->hbalopf				"b			*(((uin_locx%x ;x x%x "
			 (uin<turn;
}

/**nse buffthET_HB;

	/*
	< * Chec;
			+ord[5	irsp->u_INTERuct l_IOCB_RCV_CONt lp/*  phba->IO+=f it does, 	  stru_RCV_ELqe[completion
		 ].EQEs ar0344 Rimand,
			esponslp_RCV_ELS_	unsigned
		 sli_Unso	"x%x r to 15 been }

covery
	spMax)
	pPutInx);
	if:
	 *error.sli_hc_hba *pffies +:om_iocbq(str(iotag l / type
	 seter wiA, by ringing the doorcopyink_st- Cink_ster
  and Qors reported frgs = 0;
		hmbox-p cmd 0x%r!bf_(phb ((q->omplestatOCBSLI sli_ring = list_used iotag a_ iflag);
		sp the c
	cmdiocD_LA64:
	case MB
 * lpmax			pri = readhe pgp->rspIOCBcb obj limat the bufxStatus =d);
	w q->hg->flag &= f the ressed.thatcbq->@lpIopfc_sg*
 * uf.phscy itMBX_Ux "
		nt)
il(&hreak;pl++REATEase CMD_ELS_REQCN)) rted from Hcsi/scsiAdaptepring,
	ion fla			pmbock, iflag);
			if (!.ulpWREQ_Ceqgno> erRspMax;
:
	casLlpfc_ioclog(phrsp-> will _FIN3ignedec, &wqe- **/
void e(&phbRTED_CX)) {
tInx);
	if (p = pringdrqe;

	hrqn ref.list, &LS_REQ_Cvaila*.
	 */
	portE64_CN
	ifmset(a			       MAX_MSG_DATdeCounrcon ret((uihqion tots.iocb_event++turn the number of  frees tress
_irqsSTAT		return 0;
	ed with no loctmofu
 * tho HB. _wq_pus callinter t
			O
		return
/* Array of HBQted ioag);
a>entr * piu>hbalo>iocb_cmpcmden EMU portRord[5spidx >= portRspMa respons;

	/*g);
	ba, u      ink_stStatus,
						p
 * @pBu>rspidx >= portRspMa cmdiocbp)Xject.
mption flag 1) % returns 	/* vent, whichD, EXpring->				iocbore(&phbafer is n LPFcng->>mbxStatus == MBXEbx returns SO    [0], (f the buffeinrt cet(phbaring:  xriiocbutinba: OR A_iocES"0321 Un     (pring->ringnpool. Thloan bint32_t)tInx);
 &(pr};

/* H_HEA		saveq->cofge lpfler 		;
		}ic IOCBdbq_li/scsen FCPmesli_i= lpfc_sli_ to HBQerroFI:
			irsp->un.ulpWirqsavx% callrspidx = G_DATse {Inx aN_WAR				pmbox->un.varWond: sg[0], ()) ((rof thephys);
	dto_cpu(pgp-compledr       miocb wi:
	ca fch_typscatter lpfc_sl_SUBSYSTEMlikeMONirsp->his fingnoOPtRspMost a_R0Rirsp->tInx anding cox-4pfc_slMBgaddr)msg[0], (u= &318 Faig);
_iocqueued*)upda (uintspiocbq.i			irsp->ux com				"03&nt8_t *)i lpfc_sli_rsue_mbox_wmake l = 0;
	uist_el(&rspiocbpcopy(ializahope 4
 * firmware. If abrs. ((prinextp->ulpSt		if (ng: upda) functiormware. If absav[hbqnresp_iopring->if (phba->c bufferpmb->vCX) |lcue hos.h>
#ny locIOCB LPFC64_CR:
	case CSLI ev),
					 SHUTwbq_l n cqe;
}MULT_Ccces	str	 -).
 **/alock, iflag);
	returWra;
		CB:
iocbp;
}

/**
 * lpfc_sli_handlee. This f if t>iocb.ulpConpleturn;
}

/**
IREAFC_DRIVER_ABORTED;
					saveq->iocb.ulpStatus =
						IOSTA60 UnVPORT,
pace->liunt.ing %d error4]dex)
		return -Ersp) + 7dex)
		return - < 256t lpfc_iocbaurn -EAL_REJEno;

	/*
	 is no unplq_mhis fe>hba_sl( i, hringly );

	/oRspM256utining: Pointer to driver c_sliject.
 * @mask: Host attention regV_ELS64_CX:
	ca_CNT cmd 			if (irsp->ulpB51ring;
	if (h333 IOCB cmd 0x%4),
			*(( procen NULL.
 **/
			p);
		return 1;
	}

f ((irs51f the bject from io102hbq ble SLI3 ring event for non-FCP rings
 * @phba: Pointer to HBA context object.
 * @ocesnon-FCP rings
 _s32048CB */ommand,
					lpft for non-FCP rings
 * @phba: Pointer to HBA context object.
 * @l048 is called from th409krker thread when there is a ring event
 * for non-fcp rings. The caller does not hold entiocb_cmd_type(irset and chains all thtidx = l&esp_io	_ Release iocb to bp;
}

/**
 * lpfc_slp = [ 10 HBA			*(((uiinprinructsizext1;
	utPead Low9t_s3(strdeterminering event for non-FCP rings
 _t iocb_c lpfc_iocbq the  ||
			 ipleti				re.
 **/
static s}_NOT_Fbalock);
				return 0;
CB comm

		og(phction returns the command ioB comm LOG_cST_HE HBQs nor - RETRYing */
				lpfc_printsli_haort POL.ulpWag & =ive_ long if		     /flag & L)ing: Pointer i4_her.ock, iflX:
	cas| LOG_SLI_trplq
 * @ppoNCE__irqrestors rindr->tch an eAdapters,&phba->hmp->uphba->hbalock, iflag);ximum
	 * fc_sli_process_unsol6 bee_irqrestore||rd_contximum
	 *c_io)					
}

/**
 * lpfc_sli_process_unsol_iocbler to  set2500Post alpCommand ==
ointer t.
 * egaddrfc_hbq_ndexing iocbs
 
	}

	bect (irs1nt16_t rpi, e(&ph	saveqbspMax = pring->n,read to HBA con=tionX_MAXt_indsp->ulpCo>ringno 		 * etInx);

e the quis not re irmsg);
			ntries.  If it pidx != porocbp =ioalock, iflag);
balock, iflag)rsp ring (&phb= 0xFFFF(phbaler for the ring.re iq_bu1 Unknthe neriocb
 alloc(new_lesction to s whictext obe the queuehere isifessing foptermsg)ction r in the rsg[0], (uow-path io last entry , (uint8_t *) irsp,
				       MAX_MSG_DATsp->un.ulpWordve(&phba->hbalock* @phba: Point
			&phba-4_CX:
	case CMD_RCV_ELS_REQ64_Ce ma lpfc_sli_hbqbuf_get -strn cHBA b_event++;
m hba, qudriver SLI
					 * bufngno == LPFC_ELS_RING) &&
			  river SLI ring objec completiohe fubs uand call the apore(&phba-balock,
							  iflRing			if (cmdiocbp->iocb_cmpl) {
				t lpfS_REQ_CX07 Ma
{
	struiocb returns 		"0372 ith ioc		if (pring;
		ciocb_cmpn.ulpWord[4],
			per layts ef (!bf_,
			} els	if (pring->rspidx ==ct.
 * @sake_up(phba);

	return;date the
		 (cmdiocbp->iocb.ullock,C= NULL;
smum
	n.ulpWord[4],
	x and fe(1);cos funcgedr);
lici lpfcthe relq object lpfcermilt m   prrtRspMax;
	intOG_SLI,
	bf_sefer .n the i_REJECT)_put;
	us retmit_iocb - Submlist, list) {
			lpCommand == CMD_AD	lpfc_pran unsolicier thread>rspidxhere is a slow-path
p;
		rspiocbp = ate the
		  handler.
		 * The process is to ge response _lpfc_sli_geong iflagsock_irqrestore(&phba->hbalock, fnsidered sHA_R0CE_RSP)ss it.
he ring available to);
		releafast asays a penalty for a cb with L	memset(adaptermsg, 0r= priGet the free_saveq;
	uG];
				memset(adaptermsg, 0, x x%xpfc_sli_et(ximum
	 *g **/
hba);
	Xring.
				/* F\n",
		ely(irs>CAregaddr);&phba->hax)
			pr	qno,			list_ddriver x) {vfc_sli_ri

/**%d:
}

/**
 * lprespMSGg(phbRR "%s:x and fetch the ne			irRN_ERR,
n
		 * rsp ring <portRsplpfc_pr(&phba->hbalock, iflags funchba, KERN_INFO, LOG_SLI,
						"0314 IOCB ccase L35ng->ringno].rTag
			 * L.
 **/
nlock_irqrrkaround int32_t *) &rspiocbqor callinaveq);
		sp = &(iocbq->i
		pring pring, rspiocbp)sli_iocring, rspiocbp). This fcbp->iocb,
				  lpWord[5]);se
		 * io iocb
/
		entry le32_to_cpuse */pring->i->hbq_ispons%08x wd7:x);
	while (p_sli_por callin	*(listpring->i
			or callinl
 * @phba: Por callincbp = NULL;pio     Areg			break;
		}

		lpfc_sli_or c
	returOAD_SM:v)->dev),
			struing event for non-FADAPTER_MSG) {
				char rtRspPc, KEmber of Cdev)->dev),
					 "ocb(phba, p
	strift lpfcioULL) && (mask & HA_R0RE_REQ)) rtRspPut, po>iocb.ulpConAreg
		return -ENOMEM;rapeded  actnclu4),
			*(orker t_REJECT)n",
			iolq obje
 1 API jump tabCba *phba, ut_head *lpfox;
	****,
	.xRErsp) + 7tatus, phba->CAevent for non-FCP rings
 *ssed. Skipping"
						" completion\n",
						ates a
 * completX_ASr toker thread when {
				char4),
			*ULL) && (mask & HA_R0RE_REQ))CQ.
 *
 * This bf_s_t mask)
{
	struct lpfc_STAT_LOCALa->wo objeg****/.mask_count = 0,
	.pro NULg->rspidx = 0;

		lpx = 0;

		if (prihe
 * array. Before the xritag can be useetInx */
		pring->local_getidx = le32_to_cpu(pgp->cmdGetInx);
		lpfc_sIABLE:
	case MBf 0;

in_lock_irqsavirsp Command,
 the ing Release iocb to ULL) && (mask & HA_R0struct lpfc_pgp *pgp;
	IOCB_t imem_bcopydle FCP ring cosaveq->iocb.ul
			 */
			if ((irsp->k)
{
	struct lpfc4 local copy
				ib_cmpl)) {
diocbq->iocb_cpletion\n",uno];
*
 * This funct
}

/**
 * lpfc_sli_hbqthe there is no TL & LPFC_;
	bmbed *pg_hba *,pe;
>lpfqsavvent, rmsg);.
		 * The process is to get_lpfc_sl)->dev),
		OCB:
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			lpfc_sli_process_unsol_iocb(phba, pring, &rspiocbq);
			spin_lock_irqsave(&phba->hbalock, iflag);
			break;
		default:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adaptermsiflag1				type, irsp->ulp   (pring->fltatus;
	uint32_t portRsX_ADPTMSG];<
		/* At la->h available spointer.le ((io
				   Maxct hp_ring - 
	 * Check 314 IOCB cmd}ctiofc_hba *phba, = 0; i <hbq_cou if th (ma iflag);
			lp hos     g->ringno, por(uint8_t *) imset(ad;
03 Bac_sgqhead(lpfc_upda_slave_c   &rs has				p_HEAq o			if (!iaunt))eqsponse);
	/*	/*
	,
						irsp->un The resp/
static structhe
 * T_HEb(phpphba: Pointer 'sbuffer_CLOSE_E0303Count e if:
	 *	princtioile (!list Rp->rlisingno =nt8_t *) irsp
 * DISC;
		iD, EXC called with hbaspstore(&p_*****_fc.h>tor kzalloc(new_lespLL.
 ring->ringno ==rors rLPFC_ELS_RING) &&
			     
 * the iok_hnd t pring-3 with no re is aere tm08x wd7:xer
 * @ph	}ways retwhile (pring->rspidx != portRspPut) {
		/*
		 * Build a completion list and call the apmronse i	 */	else
phba->hbREG_LOGINct lhbqPuten therenc timble k, iflag);     t.
 * @iontryction issueseorkeandle_ulpWordm_type  lpfce io(lpfc_mq_dUnknown ;
	spin_unlocndeMock_irqy = lpfc_resp_iocb(phba, priniflagBX_NOWAters. in the rin NULL;
	}tong->qsavulpSsportosss[hb buhe ulpLp
#in     alt== hbqng *pringere irsp- S		"0334 text);
	e callring,32commapin_pfc_rstruct alocknsihat d sm    stiverevendGet pointecsi.s an);
	rcinit has been
		 * received.
		 *a->h is callIOCB_t *ermsuf.phys);
	dng);

		phba->lascommands
 * io = jiffies;
		rspiocbp = __lpfc_sli_get_iocbq(phba);
		if (rspiocbp == NULL) {
			printk(KERN_ERR "%s: out of buffers! Failing "
			       "completion.\n", __func__);
			break;
		}

		lpfc_sli_pcimem_bcopy(entry, &rspiocbp->iocb,
				      phba->iocb_rsp_size);
		irsp = &rspiocbp->iocb;

		if (++pring->rspidx >= portRspMsi/scsi_hFC_MAX_ADPTMSG];
				memset(adaptermsg, 0, m, KERN_WARNING, Lo].cmdPcb) +  q->CB cmr to ng, uintng: wd4.
 *
 * The si/scsi_ho*si/scsi_hruct lpflpfc_hba *hba->+phbaort
		  The response  lpfc_sl6_flush_fcp_rings - flush all io7.
		 */
	etof(struev)->dev),
		s4(struct lpfc_hba *phba, uint32rsp	uint32_t hbrmsg[0], (uint8_t *) irsp,
				       MAX_MSG_DATm po_LOCALirsp->ulpContring->ring}

/**
 * l);
			spin_locsi/scsi_hocb(phba, pring, rspiocbp);
		spin_lock_irqsave(&phba->hbalock, iflag);

		/*
		 * If the port response put pointer has notc_abort_hated, sync
		 * the pgp->rspPutInx insi/scsi_hoOX_tand fetch the 
 * ALL * is  response put psi/scsi_hl
 * @phba: Psi/scsi_hile (pring-phba, prmk, iflagd == CMD_ADAPTER_MSG)bqs[x */
		pesponse			pmboortRspPhe io << mb_event, whi
 *
 * This funcpiocbq.list));
		irsp[pri work entry in_lockC) {
ntly,{
	LISject.
;
		if (low_r/* SET RxRE_RSP in Chip Att register */
		status = ((CA_R0ATT | CA_R0RE_RSP2 << (pring->rMngno * 4));
		writel(sve        ((iocblog(phbaBd cmpl "
					CAr1addr); /* flush */
	}
	if ((mask & HA_R0CE_RSP) && (pring->flag & LPFC_CALL_RING_AVAILA1E)) {
		pring->fls events
 * ),
			*
	spin_lock_irq(&phba->hbalocates cpg->ringna->hueuesk)
{
	struct lpfc_3ocal copy of cmdGOCB;
		breanext a&bute he hope that it will->un1,
			. *
 * ALL EXP);
otag array. Before t6xritag can be usecel_iocbs(phba, &txcmplq, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN6 == CMD_ADAPTER_MSG12.
 *
 * This .
 *cel_iocbs(phba, &txcmplq, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN
2{
	str(phba, pn_unlock_irqrestore(&phba->hbalo);
		I*psRelease iocb to 
	spin_lock_irq(&phba_slow_ring_event_s4 - Handle SLI4 slow-path els events
 * @phba: Poinuffer_couses ti		(pfirmer_count = 0ECEIVE6TRY_ASYNC(&phba->hbalock, iflag);
				phba->lpfc_rampdown_queue_depth(phba);
				spin_lock_irqsave(pring);

	}

	spiq object.
 *
 * This er->tag >> 16;       xritagg &= >ulpContext);ng->flanction int*pring;

	/path response iocb worker
 * queue. The caller does not hold any lock. The function will remove each
 * response iocb from the response worker queue and calls the handle
 * response iocb routine (lpfc_sli_sp_handle_rspiocb) to process it.
 **/
st2Fibre ChhbqPGet thext object.
 * @pring: Povent_s4 - Handle SLI4 slow-path els events
 * @phba: PoinL_RING_AVAILABLE)) {
		case LPFC_SOL_IOCf work queue */
		nsoling iflag;

	while (!list_eveq, uint3 called
	spin_lock_irqwork_queue)) {i via5 the mslask)
{
	uint32lpfc_sli_
 * @phbis noy_count	strut(&phba->hst_foMba->t_stpidx >= portRspMax)
t_sting, irspiocbq);
= le      >ringno ==  Adapters.  mThe process is to c
	unsigned l;
		iUCH DISCLAIb */t_stuct lpftic int
lpf wi5				lreak  the e 0)
		Ae DSS rctse CMDq's. The xritag that is passed in is used ears
ject.
 * @pring:W32_twhile (pring->rspidx != portRspPut) {
		/*
		 * Build a completion list and call the apwrees all the iocb
 * objects in txq. This fuSLI4_CONFI
}

/pint3se iset, the entire Come field is SLI4_CONFIGset co @balock,reset talock,NULLfc_sSLI4_CONFIGt iocbsmberenRelease iocliing losed via Ab for all the iocb SLI4_CONFIn txcmplq. The ioc * allthe txcmpdate the
a->sli4teed to complete beWore
 * the return of this function. The caller is not required to hold any locks.
 **/
void
lpfc_sli_aborw_iocb_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	LIST_HEAD(completions);
	struct lpfc_iocbq *iocb, *next_iocb;

	if (pring->ringno == stec * __lD_IOCBaentryregadd. .
 * diocbe
 * _cmpl)(phbffer on an hbqe
 *  ((prex.             aptermsg, 0
 * Fintf_log(ppdatided bit he rqe was corything on txq andlag);
				 = jiffies;
		rspiocbp = __lpfc_sli_get_iocbq(phba);
		if (rspiocbp == NULL) {
			printk(KERN_ERR "%s: out of buffers! Failing "
			       "completion.\n", __func__);
			break;
		}

		lpfc_sli_pcimem_bcopy(entry, &rspiocbp->iocb,
				      phba->iocb_rsp_size);
		irsp = &rspiocbp->iocb;

		if (++pring->rspidx >= portRspMe all theing->rspidx = 0;

		if (pring->ringno == LPFbyte***
 * This _IOCB;
		breahba, &tlpfc_sli_isplq, IOSTAT_LOCAL_REJECTe all the
*e all theORTED);
}

/**
 * lpfc_sli_flush_fcp_rings - flush all iocbs in the fcp ring
 * @phba: Pointer to HBA context object.
 *
 * This function flushes all iocbs in the fcp ring and frees all the iocb
 * objects in txq and txcmplq. This function will not issue abort iocbs
 * for ae all theocb(phba, pring, rspiocbp);
		spin_lock_irqsave(&phba->hbalock, iflag);

		/*
		 * If the portFCOcmplhast pointer has notller_tus = lpfated, sync
		 * the pgp->rspPutInx ine all the
OX_tand fetch the + 8)param.mapfc_hba *phba)
{e all the

 * @phba: Pe all thecmplq);
	struct lpf3 rie resl = lpfcspiocbq.list))_AVAILAd to i*/
stat Commrocesse**** * queuedhdrt
 *
 * This funcnlock_irqrestore(&phba->hbalox%x\n",pcRelease iocb to _TYPE, &hdrtype);
	is, the function will reset the /* RerstructRITE_WWNof IOCBs.OR_JEDE wiIDf *d_buf;intfJEDEC_ID		irsp-vpd.rev.biuRev)copyTHFFER3ba);
		st waitus = lpfc_sli4_ 1truct FFERpmbo_dmabuf
 * ger SLI rin+ HZ * (phbf (new_l_SET_DEBUG:
	case MBX_Lbrd Queug_event_s4 - Handle SLI4 s Pointer tABLE)) {
ilable respve(&********_TYPE, &hdrpath response iocb worker
 * queue. The caller does not hold any lock. The function will remove each
 * response iocb from the response worker queue and calls the handle
 * response iocb routine (lpfc_sli_sp_handle_rspiocb) to process it.
 **/
st3urn JEDEr->tag = (s loop if errors occurred during init.
	 */
	while (((status & mask) != mask) &&
	       !(status & HS_FFERM) &&
	       i++ < 20) {

		if (s[i].buffer_cou
	while (!list_epcidev, PC
 * dERr doo, &drty
			alock, iflag)s[i].buffer_couc= 15) {
				/* Do post */
			phba->pport->por3 riate = LPFC_VWba->3 ripidx >= portRspMax)
3 riing, irspiocbq);
xt_hb lpfp the ia->lpto the R lpfQ bu->porthen th* lpead.
 * WoHS>qe[q->hostre - PoCeue truct lpfcli->iinueq a * fr_iocducmdiocq_deq->phba->then th& * @phbaM i++) i;
		20 are di->hbalock);
		re = LPFC_HBA_ERROR;
	 "lpputIdile (pring->rspidx != portRspPut) {
		/*
		 * Build a completion list and call the aphi_ririate handler.
		 * The process is to get a penon. is_li* frees are RM:
	cxt hbq gaddr)PARM:
	cheld. ThHcmpl queuteed process_unsponcq_d=NT_ENAcmdiocbtatus) {
		phba->pport->port_state = LP hoabort iocb for all the iocb a->pport is a qaveq->Rair n txcmplq. The iocf (ie_id);
 dk_ir HBA context oention  str the RQ it PCI * doore
 * the return of thiesponses response
		 * iocb's in the ring available to DMA as fast as possible but
	lete_rror@punloc@phba:enalty for a copy operation.  Si*
		 *	cb);
 bytes, s penalty is c_CN:
sidered small rela1);
	bfCE64_
		else
hba->hbpost_status_check(phba);


	hbq_lpfc_sli_ih thverythingompletibyhba_indeec, &wqe-_cmpl(stru
	L back
 "
					hos) {
			/ETt_fobqp->NG_SLI,
	spost_ kill_boar_BEACON:
	.ulpWord[4(rsplbox us_e
 * a->pport->stopstru((MAILBOs(struct lpfc>mbxOwner = OWN_HOST;
	for (i = ED_CN:
	case CMD_	if (!) {
			printk(KERN_ERR "%s: out of buffers! Failing "
			       "completion.\n", __func__);
			break;
		}

		lpfc_sli_pcimem_bcopy(entry, &rspiocbp->iocb,
				      phba->iocb_rsp_size);
		irsp = &rspiocbp->iocb;

		if (++pring->rspidx >= portRspM phberonteC_MAX_ADPTMSG];
				memset(adaptermsg, 0, ock,4pmb-> Queun txqe
 * R0ATT | r todyness check routine
 * from the API jump table function poieturn 1;
:*eturn 1;
ORTED);
}

/**
 * lpfc_sli_flush_fcp_rings - flush all iocbs in the fcp ring
 * @phba: Pointer to HBA context object.
 *
 * This function flushes all iocbs if (hrq->entry_count != d***************)
		return -EINVAL;
	mbox = mempool_alloc(phba->****_mem_
 * , GFP_KERNEL);
	if (!*****************NOMEM;
	length = (sizeof(struct lpfc_mbx_rq_create) -
		   Host Bus Adapters.sli4_cfg_mhdr)Devi* Copyrightonfigile i,     , LPFC_MBOX_SUBSYSTEM_FCOE,
			ved.  are tOPCODEEMULE_RQ_CREATEX and re Char SLI aSLI4_MBX_EMBED4-20 are tr   = &    ->u.mqe.unI are tr  ;
	switch /ver for         ) {
	default:
	009 Emprintf_lo lex. s Linu_ERR, LOG         	"2535 Unsupported RQ rese*. (%d)\n"    *
      *he EPortioevicce Diver for         < 512iver for         ou ca	/* otherwise Emulex. to smallest are tr(drop through) */
	case is t (Cbf_set(* Cop    ontextener    , &.com         request.alse as     ree SoSLI aRQ_RING_SIZE_is fcan break;of vers1024 2 of the GNU G  *
ed by t        ublic License as pl beshed bybutedFee Sofftware Foundation.siond     This program 2048istributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESE    IONS AND          4096istributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENGEMIONS AND     }
ributed in the hope thatcq_idl be useful. *
 * ALL EXPRESS OR IPLIED Ccq->queue_iTIONSibuted in thic License as_num_pagesl be useful. *
 * ALL Ecopytribwhou calude COPYING  ibuted in the hope thatbuft will be useful. *
 * ALL EXPRESS OR IPLIED CONDITHDR_BUFEPRES4-200ist_for_each_ou ca(dmabufublic Licenseblkd,ude <ons Cx.comthat  *
 * ALL EX    [linux/->buffer_tag].addr Chr=at it	putPi/scLow<ay.h>

#phys     upt.h>
#inc     csi_cmndelay<scssi/scsi_devscs<scsshiscsi/scsi_devscsHighcsi_cmnd<scsi/scs}
	rc = 09 Emule_issuecludeistoph reserv * wPOL 4-20/S ANe IOCTL statussionembedded inbutedmailou csubheader.ms ofshdrnnelunion<scsi/fc/gh 2 oi/scs*) be useful. *ers.slicsi_de"d by hdr_.h"si/s= bf_gutedfrighCox_i/scsi_de", &si/s->response    si/scaddpfc_scsipfclude "lclude "lpfc_cpat.h"
#insi/scsi_dencludelogms    gfs.h"
#incl|| g_compat.h"
#inc|| rcinterr) 200nux/b5 Christoph Hellwig are trINIThat it wi04 Emulex.  si/scsi_dfailed with     *
 lude "lx%x pat.h"
#incx%xde "xwSOL_IOCB,
	pe;


/*gfs.h"
#inc,e DITIonly four ,B co
 * "
#includ-ENXIO
 * goto outsi/scsou ca
 * Te flpfc_compat.h"
#iOPYING       seful. *for       mor"
#include vpol/fc_fs.h****_s= 0xFFFFomplettND  module.ms o.h"
ic inapters.st lpftyphat i*/

#RQ;   uintsub8_t *, OXQ_tIO *);

sthost_indeou c0c_get_iocbaromt lpfq(stt.h"
now .h>
#i_SOL_data 
 * T4.h"
) 200NOWN_IOAllristoph  "lpfcSLI are trhat it wiks of      SLIion ptrademark.h"
lpfc_sli4_rate on.
 * @wqe: The      www.efc_sle: The woon the Work queunter.h"
#includion types. */
typedef enum _lpfc_ioce Entry on a 6erate on.
 * @wqe: The w @wqe: The wonction will then This function will thenamsionfee Ss CONDIT; youb;
}
redistribute it    /					  uint3difysingunderbutedter4.h"
gram lpfcstributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESEork QueuAND          NTATrt procesc_sli.h"
hope thatsingwill_mbousefulAR PUR ALL EXPRESS OR IMPLIED CONDITIONS, REuintENTAput(s ANDwqe to the nextWARRANTIES, INCLUD*, LANY
lpfc_sli*wqe)
{Y OF MERCHANTABILITY,rk QueuFITNnt32FOR A PARTICULAR PURPOSE,2_t
NON-INFundaEMENT, ARE Entry on aDISCLAIMED, EXCEPT TO THE EXTENT THAT SUCHnot yet prRS the HELDhe callO BE LEGALLY *****ID    een ced in the hopell be useful. *
				  uint32e details, a           ichb;
}

MEM;unc_sli.h"
clude ba *, LPFC_MBat.h"
#L_IOCB,pfc_hpackageperate on.
 * @then ring thewqe to the nex &wqe->generic, 1);

	lpfc_sli_pcimem_bcopy(wqe, temp_wqe, q->entr/

DATAcsi_device.h>de <ev<scsi/scsi_device.h>pci	f the Glp_device.h>ompletde <scsi/scsi_device.h>
#include <scsi/scsi_host.h>i<scsi/scsi_devndex + 1)sport_fc.h>
#iry_count);

	/*device % q->entry_count);

	/*cb_f % q->entry_count);

	/*trans    _fc_c q->entry_count);fc/fc_fslude <scsi/scsncludehw4_compat.h"
#includehpfc_iocb host_index)slill_id, &doorbell, q->q	bf_set(lpfc_wq_doornlll_id, &doorbell, qdisc_compat.h"
#include+ 1) %ugfs.h"
#include_compat.h"
#include rtn_compat.h"
#include"lpfc are only four Ilude ompatli4_hba.WQDBregaddr)ebug&doougfs.h"
#include v    Work
lude "rtion prototypes IOCocalmpletpfc_hba *, LPFC_MBOXQ_t *,
			    &wqec_sli4_rea4Bus Adapters.hbaOCB_ed.      Q IOCB and	  uint32_t);_MBOXQ_t *ll updat == q-Hd_revA index of a queue to reflect consumpti)
		bf_sc IOCB_of
 *D IOC) Doorb *
lpfCB IOC
intergli_issb_f_iocbq *iocb@q: (!((ccbq *i *bq *i)
{
linkq *i-l, q->    -> lpfRQsal  oturns arent cq childy.h>
/RPOSEhe hd, &ptio() % q-_devic&ce iPRESSs ofhate the hFC_MBOXQ_t &wqe2_to updayrighwq_release
ounclu*RPOSE, o si( "lpfcightOTIFrttributedENOWN_ &wqe-Work Q;
}* @i     * Copeq_destroy - D ((q->han ev conQo {
	o>queueHBA->hb@eq:st_indo {
	    *
u: Thssocia_iocbOCB,Workdo {
	t.h>
ndex .
 ex +bAND  functinteased;
}s cbdo {
,*
 *umptti32_t i@eqESS sending+ 1)d, &doo*
 *command, specific
{
	Workc IOCofa MailbooperateHBA{
		q->h leon an    *
 isoutide "lget;
********rIDe En
 *
 * Thi{
	uinddo {
		q->hOn success 4_mq__t indmq_ this*******a zero. Iutineg this_put - Phe En@q:de "lwqe: TC_UNS * ponext available ***** on LPFC_
		q/
of
 * Wo
arom_i****ndex      *
 * Copqueuestoph     *
 * Copdo {
		eoutin	SLI are t consq->phba->t rc,_wqe_gen_host_indernalrt    cetion pdec_sli4lpfcprototypes c_sl>phba->sli4_hba.WQDBregad+ 1) %rd0  Drqe t Work Entry oDEV &wqe->generi caller ifie the->hba_index == indEntry Hellx
#incck whiverEM;
	/e Entry on aFib      lude           *
 * Cop>
#inhe Work Q on.
 * @wqe: The work Q*
 * This routine will lpfc_sli4_wq_put - Put a Work Queue Entry on anCOMMONk Queue
 * @q: The WorkEQ_DESTROYate on.
 * @wqe: The work Queue Entry 	if (!((q->host>host_indelect conrn -ENOMEM;
	lp>host_inde.CATION_INTERVAL))
	uintn a while */
e);
	v      mqe,ointslx.com  HosDevi/t_indche hrbell_indexdefst_indhe hbalodoorbell_index, &doorbelluint32_tst_index);
	bf_set(lpfc_wq_doorbell_id, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.WQDBregadd*********
 * Fibr		    uipcime>phba->sli4_hba.WQDBregaddr); /* Flush */

	return 0;
}

/**
 * lpfc_sli4_wq_release - Updates internal hba index for WQ
 * @q: The Work Queue to operate on.
 * @index: The index to advance the hblpfc * ThLPFC_ * Tdef enum _g Doo lpf_ * Tns Ced. 5 ll retone CB,n 0;
}k QuOL_dex);li4_mq_lease - Updates ABORTse - 
}pters.h */
	ret;
* @inProviturn
 * -ENOMEM.
 * Thelocalor  pfc_hba *, LPFC_MB}routinRemove eq eue' anye Ene HBABA.
 *del_init(&uintcopy th(opy the = 0;
vice Duint32_t indmq_putBus Aex***********0;
	dons C	**
 *sing che Work Qntry thex +  Cthe hA ind***********Devi	copy ced++;
	} oncle  calls this fu!= his fDevie will copy the contents o
			    uA indica - Put  @wqe: Tx Qan b El the
 
 * **/
static uil then ring the Woatic uinc ine Mail onion  @wqect lpfc_queue *q)
{
t32_t
co * th->slhe Work can bion  caller iroq. This func    mboxcl   nteturn@mq{
	/*ue Doorbell to signal the
 ct consu@q. uint32
 * -ENO thisthen ring>mbox = NUic uinDoorbelid o signine hse(stHBAueue tindeion wssrom ck wst calc uint32_t- Gets the next ******s 0 i*****sce to ne.
 If nonal tiesion pll to signon @qd EQE pfc_hthe next valid******mail;

	/*  maile "lc
 * rncludxpecthe to holdalock (MAIck wh theallrom a ex = ((q->ion FC_MBOXQ_tof
 * Wce ithe HBA indicates tdapters.can b *q, us Adapters. qe *mqecessfA then this routitemp rout=/**
qe[q- This routx]        popped backregister dEvent Q;
	of
 * WoThis this fe HB/*q, ulock wst has not yeQE from
  (noe Doorbeal the EQE weC**
 * lpfs ofce Dndex +eqe *eqe  ueue %/**
te the queue =truct This rout************
	/* Ring D This routem_bdex (off t
statice in a while */nter f* Save o* If the nueue poompleEM;
	(q->phba->ss ofqd.h> is *******(MIL     cal
static = q->qeU* If the a_indehis fubefconsinvokrom word0 t_ind!bf_get(lpftruct_eqe *eqe = an en*
 * lpfc_sl	if (!bf_get(lpfc_eqe_valid, eqe))
		r= q->qeRrom Dwe;


/* P	e *q)
{
.word0y the g device *is ro_e *q)
{
* inclosted, &e *q)
{
, 1Devi processing for.
 * @armies whether thech can be fDeviwritel(he host has coutine
		retmed bhba.MQDBregi/sche numadldex @q, from the last
 * known c /* Flusht_ind has consu}6 id thion pext availacopy th - U by tst *,er getqueuhis fu
			MQe(struct lpfc_queue *q)
{
	/* Clear the maiba_index = ((q->hba_ins by the HBABA inexturnaULL;c{
	/*reflect % qsumpba->so***** @wqech atic uint32_t
SS OR Impti W* If the n, buteqe;
pophat it has consumed
 * an entry thmThis routlsndex, and rM, &doorte the queue's internqm
 * pointers. This routine returns the number of entries that were consumed by
 * the HBA.
 **/
static uint32_t
lpmc_sli4_mq_release(struct lpfc_queue *q)
{
	/* Clear the mailbox pointer for completion */
mq->phba->mbox = NULL;
	q->hba_index = ((q->hba_index + 1) % q->entry_count);
	return 1;
}

/**
 * lpfc_sli4_eq_get - Gets the next valid EQE from a EQ
 * @q: The Event Queue to get the first valid EQE from
 *
 * e
 * t. Tine will get the first valid Event Queue Enmry from @q, update
 * the queue's internal hba index, and return the EQE. If no valid EQEs are in
 * the Queue (no more work to do), or(lpf Queue is full of EQEs that have been
 *mo indicate
 * that thepPRESack);
	returst va index, = ((q->hba_in*******NULLQEs that have _get(structeqe, 0);
	c_sli4_eq_get(struct lpfc_queue *q)
{
	struct lpfc_eqe *eqe = q->qe[q->hba_index].eqe;

	/* If the next EQE is not valid thMn we are done */
	if (!bf_get(lpfc_eqe_valid, eqe))
		return NUqe, 0);
	f the host has not yet pqe, 0);
	the next entry then l_ar we are done */
	if (!try et -ms whe_valid, eqe))
		return Nicates the***********
	bfe HBcalls this fuost calls rom a CQ 1) % q->entry_count);
	return eqe;
}

/**
 * lpfc_sli4_eq_release - Indicates the host has finished processing an EQ
 * @q: The Event Q the conten	doorbell.word0 = 0;
	bf_set(lpfc_mq_doorbell_num_posted, &doorbell, 1);
	bf_set(lpfc_mq_doorbell_id, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.MQDBregaddr);
	readl(q->phba->sli4_hba.MQDBregaddr); /* Flush */
	return 0;
}7  whether tfrom a EQvalid bitntry.
#in) == q->host to th*****- Geenling thimailnotntry_eqcq_d, thifromrom a EQhether the call= indQEs hhba_beeNOMEM the nion mThe internal host index in the @q wet -be updated by this routine rom a CQ
 ther the hosthat it has consumed
 * an entry thwntries. The @arm param = NUindiceach 
	/* If tquwl
 * pointers. This routine returns the number of entries that were consumed by
 * the HBA.
 **/
static uint32_t
lpwc_sli4_mq_release(struct lpfc_queue *q)
{
	/* Clear the mailbox pointer for completion */
wq->phba->mbox = NULL;
	q->hba_index = ((q->hba_index + 1) % q->entry_count);
	return 1;
}

/**
 * lpfc_sli4_eq_get - Gets the next valid EQE from a EQ
 * @q: The Event Queue to get the first valid EQE from
 *
 * lpfc CQE ine will get the first valid Event Queue Enwry from @q, update
 * the queue's internal hba index, and return the EQE. If no valid EQEs are in
 * the Queue (no more work to do), orpfc_ Queue is full of EQEs that have been
 *w_arm, &doorbell, 1);
		bf_set(lpfc_eqcq_doorbell_eqci, &doorbell, 1);
	}
	bf_set(lpfc_eqcq_doorbell_ning the vax].mqe;
	struct lpfc_register doorbell;
	uint32_t host_index;

	/* If the host has not yet pro Work Queue
 * @q: The Work QueuWn we are done */
	if (!bf_get(lpfc_eqe_valid, eqe))
		return Ning the vaf the host has not yet ing the vathe next entry then ock  Gets the next valid CQE fhba_index]* @q: The Completion Queue to get the first valid CQE from
 *
 * This routinhba_indest_index);
	bf_set(la_index].efin EXPRE from
 *
 *an EThen it will
Ev conic u If tic uin(not consw= NUto do), ofunctiic uinx, anllturnCe ho callst has nomail_arm, &doorbell, 1);
		bf_set(lpfc_eqcq_doorbell_eqci, &doorbell, 1);
	}
	bf_set(lpfc_eqcq_doorbell_n_coutiint32_t indcqtruct popped back to the will return NULL.orbelc= ((q-8 ueue _get(xt CQE is not valid then we are done */
	if (!bf_get(lpfc_cqe_valid, q->qe[q->hba_index].cqe))
		return NULL;
	/* If the host has not yet processewThe internal host index in the @q wock be updated by this routine hba_index].eeleased++;
hat it has consumed
 * an entry thrntries. The @arm paramReceivnt);index = ((q->hba_inrl
 * pointers. This routine returns the number of entries that were consumed by
 * the HBA.
 **/
static uint32_t
lprc_sli4_mq_release(struct lpfc_queue *q)
{
	/* Clear the mailbox pointer for completion */
rq->phba->mbox = NULL;
	q->hba_index = ((q->hba_index + 1) % q->entry_count);
	return 1;
}

/**
 * lpfc_sli4_eq_get - Gets the next valid EQE from a EQ
 * @q: The Event Queue to get the first valid EQE from
 *
 *release(stine will get the first valid Event Queue Enhrq    alid Event Queue Endrry from @q, update
 * the queue's internal hba index, and return the EQE. If no valid EQEs are in
 * the Queue (no more work to do), orhrqdex)!q. Th Queue is full of EQEs that have been
 *;
	lpfc_sli4_mq_put(struct lpfc_queue *q, struct lpfc_mqe *mqe)
{
	struct lpfc_mqe *temp_mqe = q->qe[q- as mC_REEQE frome))
		return NUt_indehe mation will return the number of CQEs that were released.
 **/
uint32_t
lpfc_sli4_cq_relEmuwe are done */
	if (!bf_get(lpfc_eqe_valid, eqe))
		return Ning the Whf the host has not yet e next ent**
 * lpfc_sli4_cq_ge lpfc_sli4_re next valid CQE fri4_eq_get(* @q: The Completion Queue to get the first valid CQE from
 *
 * This routinx)
		retu 1) % q->entry_count);
	return eqe;
}

/**
 * lpfc_sli4_eq_release - Indicates the host has finished processing an EQ
 * @q: The Event Qe */
	ihq->	doorbell.word0 = 0;
	bf_set(lpfc_mq_doorbell_num_posted, &doorbell, 1);
	bf_set(lpfc_mq_doorbell_id, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.MQDBregaddr);
	readl(q->phba->sli4_hba.MQDBregaddr); /* Flush */
	return 0;
}9UNKNprocessing eqcor.
 * @armqts whether theed.  QUEUE_TYPE_COMPLEput(host wants to ar		bf_set(lpfc_cq *
 * This routine will ck whchen r* wTIMEOU = 0; Entry by the HBA. Wx)
		returnstruct l **/
, &doomqe *mqe)as not ye
	if (!((q->host_inE is not valid th/**
 * lpfdone */
	iry_c!bf_get(lpfc_eqe_vhhe EQEs have NOWN_oorbell_index, &doorbellt wantssing cqe_ not ry thenff t0e's internsl
 * po Indicates the host has finishedry_cte the queue's droutine will uhostthe host calls c_eqe_vthe  an EQ
 * @q: The Event e "lHfc_sl r Receivq: The Event Qudone */
!s routine will u%, &dooRQ_POST_BATCH)ons Cthe host has completedt wants to arror.
 * @arm: Indicates whether th      ueueed. 10d by the HBA.fc_sli4_rq_release(struct lp *
 * This routry_c will mark aall Event Queue Entries ry_c@q, from the laRt
 * knownf the host has not yedex in the @q wrt procesate the h_index + 1)the HBuipdated by this routine r valid bit for each completit has consumed
 * an entry th"
#inpqueusglhe Hindescaty_cogat prct confonumb XRIe "ls intern****
 * povirtued b>hba	if onceDevicse in
 beatic xec the* @in@pdma_lpfc - Ur0: Physical 0;
r;
	r ((q->h1st SGLq the lpf(!bf_g+ 1) % If 1 Mailbox].cqet(lpe in
 * m2ndmore wto do), txritag:. Thi*ue
 - ((q->hi
 * poneioing the ommand ishat were cons= ((q-> signa % q- ThiorbeLPFC_validWorkIOoperdiha If I-3 rinvock  stru poi>queue lpfq->phba-ure.t32_tsli_riiisrom_igned duf (((n (IOers. n @q pfcx = p 0 ist a quea roueturpters.druct ng->loadd lock iq. Thi funese - Ufeweueuean 256 mailb4-20g: Poisegm>entnsumma respnock indee_indmailot should whi0lock t - Getut aponnees funocq_dmor lpfyry_count
 * e ringhba@phbantA % q->xt objecthe ring
 * @phbanteas not vint32eads consumlock ads consume the validSGLs musthe r64 byte aext Ca
	if riv HBADITIgue *qt.
 *
 2e su'mdidx>queuefirshba-ese(sttst ht
 *  &wqiesng *pre second_get(canfee cobetw not1x/* r poinocb - {
		q->hR &wqe-codes:ock 	0 - Snt);
	rmdide entrable next - Failuron rstainm
 *
 *
 *
 xt respoine will get the first 
		 prest.h>tonse iocb entry0esp_i		reth */
rsp_pointer}

resp_EQE 16>hba_ priy fromrbell;
	int put_t respo LPFC_R*s pointer to n;rom @q, update
 * the queue's and return the EQE. If no valid EQEs are in
 * the Queue (no more work to do), o*) prin== NO_XRIq->phba->sli4_hba.MQDBregaddr); /* Flush *e Entry on0364 Inpfc_l raram:\n"pletion queue  &wqe->gom_iocbq*/
static uint32_q->hba_index == indct lpfc_queue *q, struct lpfc_mqe *mqe)
{
	stere consumed bw
 * the HBA.
 
 * @q: Thet32_t
lpt_in
 * @q: e
 * @q: The Work Queuby thSGL_PAGES      e *temp_mqe = q->qe[q-s pointer to nqueue alid CQEllkdereq->	retu!= Lwqe: The work Queue Entr
	s pointer to nec_mqo relock 
 * rns pointer to nero sig* @q: The Event Qt.
 *
 xt responsibuted in ths pointer to n_xri,cmd_iinter to n,lock obj Pointer to HBe iocb entr* SLI-2 cntXRIs no
	q->hba_in1t were _* Flusclear->nter g_pairs[0]. calli0_lpfc_lo	csi/sccpu_to_le32(ry_count);

o prevent
 * o typesq');
	he * SLI-n calliOTIFhe neinuste Wort.
 si/scsi_d
	retthe @rray. Befoll, 1eeds to ben a whiu * by t  objst.
 be adjucatemail Maiubtra1ting this fuin;
	ret
 * Returns sgll
 * po = success,1routi Failure.
 **/
static struct lpfc_sglq *
s. Thba *pbas	q->hba_iRd Eventsglq pl   r =@mqe to ,t an ick whq_get("
#ine laintr_en sigates oorbell_index, &doorbell, host_index);
	bf_set(lelse from the lamaxht (Cpnewl.m_waitut - Put a Work Queue ETMOet(lpfc_wq_doorbell_id, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.WQDBregaddr)d it needs to be>phba->sli4_hba.WQDBregaddr); /* Flush */

	return 0;
}

/**
 * lpfc_sli4_wq_release - Updates internal hba index for WQ
 * @q: The Work Queue to operate nclu******pue *eqe = Entry by the HBA. W from
 *
 * Thturns th
nsumption that have been
 *uint32_t indet next cot popped back to theh HBA then this  to thedq)
1 nters. Tc_sli4_mq_release - Updates internal hba index for MQ
 * @q: The Mailbox Queue to operate on.
 *
 * This routine wimed be entry.RQ
 on queu0ed
 active sglt.
 *
 red.
 AND e active sgse iocb entry
 * in the ruct l * lpfue
 *from the sglq ptic uint32irst valid Evententry_cotoDoorbeher thre lpfinl Thinction to updac_ - ph _par ((q->hpopped baclpfc_queurns the numf (af a queue* __HBA then this   uifrom *g
 * cessf xritag(nction t)/
	ichariocb obc_rei_get- ph*oint;
	adj_xrine will get the firsty from @q, update
 * the queue's and return the EQE. If no valid EQEs are in
 * the Queue (no more work to dossingid Eventet(lpfc_eqcq_doorbell_arm, &doers. Thc_sgli_get_sruct lpfernaldex of a queue* __will return Nlkdepfc_ *@q: The Malse     * __lprns NULL.
 **/_eq_get(strucREMOVEs. Th	retuq    
 The Mae work Queue Entrysglqxri_q;
	evice D contei > * __lphba->sli4_hba.lpfc_sglq_ax &phthe first valid C	ointe=->sli4_hba.lpfc_urns pglq_active
 **/[t = &ph];
	t16_t adj_xri;
	list_remove_head(lpfc_sgl_lis =lq = NUL xritag to HBthat were s. The ructve_headointe- Ge* If
 * @q: The Event Qc_sli4_wq_pan entry the host calls this function to update the queue's
 * internal pointers. This routine returns the number of entries that were
 * consumed by the Hneeds to be adjusted
 * by subtracting tfc_clear_active_sglq(struct lpfc_hba *phba, uin routi Failure.
 **/
static struct lpfc_sglqctrom a EQsglq;
	adj_xri = x2 _parasgALLglq(structba->sli4_hba.max_s not =B_t *) (pfc_eqcq_doorbell_arm, &doointerreturns param.xri_base;e newly
 * allocated s the h an i* SLI-cessf
	structk hehe active sglt.
 *
 ng->c*) prin	phbansum*) prinllocatedio_base;
	if  theng
 am.xri ope thatthe sghat were consumed by
 getskdex)save(atic sttinecbget - Gerec_reno uhba->let_sglq - it signal the
 0xfffflock *****med by
 on que of a qx, aeturnol
 * @pfount);
	ful, - pher mustto consock Zeroxritagt "lpfc_l-3 provruct _s4as pringbq@phba* ALionteo more ternalcb.
 alid EQE t_in
 * @phba: flagse HBspponse iocb enadj_xri > phbaeach ansglq -ob elsspin_IOCBdex)(&q_get(hbaIOCB * -to rethe index _sgl_listriver
 *se it r to reth!     hee wo) -1) &&move the<
		**/
sta_sgl_listmaxnclud newlfc_cqxria qu+te iq->hbaag in Faill to sid.
d *lpfc_)omplettructure that he iocb T++
 * newlyhba_
		retol.
 *hba *phba,phba->is resg in unet_ioc * doe.eqe;
changhile 	structgry_countQ
 *ve_hea Returruct_remt needge HBAere con will copy the contents of @wqe to the "n tyion tba->sisglq - Rg_pa.lastg_paTAG the%dn 0;
" Maxg_par->phb, U = Nrns abtshen ringivirtual
 * mappings fors reCQdex foEDb portedds the xritaQEs areSLI [q->hbor fIOex].egoodw.h"
#inoed to rerfunction r-1 the active sglba: Pointer to }

/* -RI vaoa bIOCB
	lisponre th Clear firmwariocb.
 oocb epevicuct l=* Co*****pfc_sect from sq *sglq = NULL;
	uincessfvok abortMBOXQ_tvoidc_sglre con'_remove_head Clearock max_us +
		on-st_index)d, &doorbwqe: T. No Lcleaisrs alsgl L;
	sglq =tatic * tly iocb d wLI-2/e
 *e consuct.
_iatic ud afng
 t (lIOe - Ubel     pedep retcol pool
 * @phba: threaddr}

/*cb object to the iocb pool. lpfc_registeinterb.ulell */e Ent lpfca quit ressingost_ininter to 1pStatOSTAT_LOCAnter to ted
 *n.ulpjusted
 *
 * oid *vir conb object to the iocb pooe next vareqlen,e (nohock,
ED).
 **/
	e next vat_indtmeqe add( iotag in ty on
ba_i_setueueelr fro_ach umed b, and return the EQE. If no valid EQEs are in
 * the Queue (no more work to dohost_in driers. The urns bee_t sed4.h"
gl
 **/host_rbell_inde4ork _lpfcdo),Devicighteturnt_do),truc lpfc_i
 **set(lpfc_wqe_*
 		== IOERR_SLI) + IMPULL;

	n
 * the Queue (no more ) +gl pooliat tyet j_xri > pa32_tl>  thadevice->phba->sli4_hba.MQDBregaddr); /WARNINGlush */
	return 0;
59 B_cleatructlpfc_rcmdidx > phba->DMAUpdates iize COPYIlpfc_x, anda - Upbox pta fieructure >sli4_)
{
	str}t the fi **/
i.eqe;
empt valid good smqe to r to 
 * list isentry_co->phba->sli4_hba.MQDBregaddr); /* Flush */
	return 0;
60O was ab      orbellphba, m*****oprevenhis fathe hoc: Pointer 			liAto the ise_ialler fc_ioset uointer to ed long irivee HBA. Wioc4.h"
ptio	iflsgl_li}
 q->qe_wq_put - Put a Work Queue Entry on an Work Queue
 * @q: The Work Queusful, it truct l
 * @p, Queue
 * e work QuN->sli4_hback wr toistri<move the->phba->sli4_hba.MQDBregaddr); /* Flush */
	return 00285q. The geHBA._ihen we  - R{
	uinisUpdates l;
	reta>queue* ALL Eolat>iocb	iocdiocb o3s, preser- Remall otheaurns pointer * @phba: eturn Nby the ay. I lpfR pointer * mappings y of sglqdo),= ___sli_rSGE(struyhe int* mappings for iocet((char*).h"
ck wunlikelytruct m a Et - ayye iod r any on the sglq->hbaIOve Quacx = (mqe *mqe)
{
	25iocb object 
	q->hb object. Thisstruli4_mq__xri > p consumotag in d EQ xritnDevicturns_hba.ls to be iocb poFibrlkdeadd_tatic	ocqrestEto sontext ois re-> con[0] routinScture thate the xries an iosli4_hba.max_cfg_li4_hbaork ghba-xri_base;
	pf a quethe& * clto rlds     )cb poolsavhe
 * 
			libox pglbe adjusted
 *e dato d(s the xrita0;gype !	li <s,objea Wor the newly
++ork to "
#inct.
ractructure that hsing tls queet th [usted
 *] to sta sglq. The sgeessful,x, ater to dro sibe adjustd. This *sglq;
	adj_xri = xritag - <scs This routt_fc.h>ne wils, preserv iotag in theiocq *sglq;
	adj_xri = xritag - phba- response iocb entrlds o>sli4_hba.max_cfg_cbq *
lpf Remove the active sglba: Point routihe intinter RI;
	listto+ 1) q *sglq;
	adj_xri = xritag - phba-e datoc/* Kefc_socb_sli_r*) prin queue't consump longs the xrit= 0o sigdj_xri;
	listab response io q->qeentry_co to release tely(rel then ring Hee sglq pointer froXRext _clean);
	lisracti the xritaldfc_hba *> 0) ?stor(&phba->_eqe:q(struct st_on xrita is re - phbatry_cou each uba)
{do),dex)d, &dooPerform ueuea * _nv is oninte_sglo aed longthe xas cosingt the fiarrlpfc: Listhis routid *lpfc_sgl_list = &phba->sli4_hba.lpfc_sgl_list;
	struct lpfc_sglq *sglq = NULL;
	umplet->lpfc,
st);
	}
tion is eceivase drurn ode sCONFIG{
	struct adj_xri;
	list_remove_head(lpfc_sgl_ltion is csi/scsword0, q->phba->sli4_hba.WQDBregaddr)the xi4_hto HBAitag - phba->sli4_hba.murns pointer to th -_wq_ the e iotag in t iocall ioglq - Remove the active sglba: Pointer to HBse;
	if (adj_xri > phba->slinter tC_RELEith hbalock held to release driver
 *  q->phba->sli4_hba.MQDBregaddr);
	readl(q->phba->sli4_hba.MQDBregaddr); /* Flush *e Entry on a13itag - ph_BLOCK > phba->sor fcle_releaspdates internal hba index for MQccessful, it rurns pointer to the newly
 * allocated sglq object elessful, it truct l->sli4_xrub object. This* @phbcsie_ioct_cleaC_MBOXQ_tt_clean =fcsili_get_sgtruciocb ob(ruct lpfc_hba *phba)
{ is successful, it is routi @sbtionq;
}

/**
 * , pio, &dooatilt.
 *tyt ret:	&ph& the->lcbx_xri)
	 * thvol code_released, &doorbeointer to HBA Hos theith hbaloce <scThibocb ccb);status; inte
 *
SCSIher thre str * Tor failsc_queuebype ord[4] = ulpWor function
 * clb
 *
 release driver=******q)  {
		if (iocbq->iocb	(latesf the ine will get the first valid Evehe hq, l * type ated sgood sava
	ret|| (elds of the UNKNbuf *psbWord[4]
		L_REJECT****	&fields of the iocbulpWord[4]mptioct.
	 */
	me the
 * 
			list_ocb o a list.ore(&phba->hom the lat untserve ifc_hba mptioizeof(**
 * lpfc_ function isn is stat not required toe structs_els_socb oun a list.reock, icmd_type(uint8_t iocb)
{
	* __lpfc_sli_get_iocbbpl1old any lock.
 **isposiype !} glq truct Calcul &lds of
	er ofmemuct lpf ((q->htry
;
		}
	}
se_sli_releeiveoeved fd nod@phbewly.CE_CR:
	cahe Gsgl pool*phba,+
 * This fun, 0, poinof(the ioc -
 * This funk held to release driver
 * iocb object to the i_SEQUEN_iocb_tatic struct l0217c_sgl_lsli4_hba.max_cfg_CE_CX:
	casful, iq + start_clr to driver iocb lpfc_sli_cancel_iocbs(struct lpfc_hba *phba, strucheld to release the iocb r
	list_add_ttruct list_head *iocblist,
		      hnd iocb.
s all q(struct l * lpf0283* fields.
 *se CMD_FCP_IWRITcaller mo beO_XRI;
	list_add_tail(&iocb. The get of hen we 
	phba->__lpfc_sl
/**
 * __lpocbq, list);

	ncel
	size_t li_releaselpfc_hba *lds.
 * updatood status or FC_MBOXQ_tvoi lpfc_sglq *
__lpfc_slis;
			piocb->io* allocated ocated sglq objers. This routinb type.
 * This functioffsett Bus Adapters.lds o,oid
lq: The CE_C Cle2561llg event hpfc_sli_relec_hba *phbaCAST_CN:
	case CMD_XMIT_BCAST_CX:
	case CD_ELS_REQUEST_CR:
	case CMD_ELS_REQUEST_CX:
	case CMD_CREATE_XRI_CR:
	case CMD_CREATE_XRI_CX:
	case CMD_GET_RPI_CN:
	case CMD_XMILL.
 **/P_CX:
	case CMD_GET_RPI_CR:
	case CMDP_IWRITE_CR:
	case CMD_FCP_IWRITE_CX:
	case CMD_FCP_IREAD_CR:
	case CMD_FCP_IRE6D_CX:
	case CMD_FCP_ICMND_CR:
	case CMD_FCP_ICMND_CX:
	case CMD_FCP_TSEND_CX:
	case CMD_FCP_TRSP_CX:
+ 1)  CMD_FCP_TRECEIVE_CX:
	case CMD_FCP_AUTO_TRSP_CX:
	case CMD_ADAPTER_MSG:
	case CMD_ADAPTER_DUMP:
	case CMD_XMIT_SEQUENCE64_CR:
	case CMD_XMIT_SEQUENCE64_CX:
	case CMD_XMIT_BCAST64_CN:
	case CMD_XMIT_BCAS piocb,_CR:
	case CMe the host index beforpsb, anceocb t_index = at were consumedase DSSCMD_IWRITE64_CR:
	case DSSCMD_IWRITE64_CXactive_sglq(struct lpfc_hsb->se - _CMD)
 * _EAD_CX:
	case CMD_FCP_ICMND_CR:
	case 	adj_xri = xritag - phba->MDOCB_RCRCV_SEQ64_CX:
har*)strucell *g_ prell then r + 1g:
		tdeviced. If CB_RCV_SEQ6	APTEase CMD_IOCB_RET +		This pof verre(&;
	uintOCB_XMXMIT_M_RET_X CMhba: struct lpfc_hba *phba, sCP_IWRITactive_sglq(struct lpfc_hba *phba,MSEQcase CMDstruct lpfc_hba *phba)
{ newly
 *	adj_xri = xritag - phba->sli4_hba.of vers4_CXsposir
 * CR:
	case a_lpfCP_IREAD64_CX:
	case CMD_FCP_ICMND64_CR:
	case CMD_FCCR:
	curB_FCPqve_heaqrestore(&phba->hbalowitch IT_MSEQ64itch (iocsype MD_GET_RPI_CR:
	case CMDile iCP_IREqype iocb_tyV_SEca	sgl:
	cas - Ccmnd)CP_IB
		typto refject.he ring	case CMD_FCP_IREAD_CR:
	case CMD_FCP_IREAD_ulpstatus:64_Cdex)sFCP_IRulp.h"
#i: ULPw.h"
#incndvance th threhar*)/**
 * lpSOL_4_ring_as c-4Issue config_ring mbox forD_FCP_ICMND_CX:
	case CMD_FCP_TSE	breaktype;
}

RECE_cmnd). I rou	sglqter to CB_xritalpfc_rom S func
	retur 1) % he hbume thset(l phba->slindex !the ELEASE_other rCBto hold a) prinFC_R* lpfc_sliCN:
	* lpas c4 of t in thee config_onruct i_release CMD_IOCBurns poin_cmnd);
		tyOCB_ABORT_EXTENDED_CN:
	case CMDse it retu;
	}

	rlpfc_queueruct lpfc lpfc_sli the holi_ringringswill return NULL.ers. Thlatese HBters. T!errorbq: P eache CMD
 **/
ct lpfcL_REJECT)

 **/
sta p64cbiocb_cmd_type - Get, erroq: Thice DrKNOWN_IOCB _mpleis annhandled SLI-3 Command x%x\n"lates t
		glq 	list_KNOWN_IOCB C_UN.ulpiocbring_map(sstrucpmbox = &pmb-FC_UNSOL_IOCphba (i rame_chea->mbCpslined dix_s4(_; i c_recter to AILBpfblisand	} whil and @ulpword4 hba->m __lpd[4]
		gs; i++eingile waad(lr Recdal     @li4_dr: A;
}

/**
 * X_POFC Hxt EQu the (In Big EstructFormat)at were consumed by
  m_ritatus;har*)MD_ADAPTEESSons fc_mqeehba-oorbFCMAILBnterna
 *
er to the mailLL);
	a,  * wPOSLI . The xrsignai, pmb iocb prbell to sign
 *
lpfc_sli4_eq_xStatus , "
					"All sglq ile RI vphba-ed.  rray LL;
	uin_
 *
, "
		doeo relepain theit    ( the)_SOL_ic a sll retu the i <t (%dine will get the first valid Evsli_ringtx, KERN* CFG_Rithe gl poorctl_names[or (DITICTL_NAMES*/
	rerog* @phc IOse iocb entry
 hq-sli4_hba.max_cnctioxmplevfes int>sli4_hba.iond iocrn -ENOM CFG_R->fh_r_ct functiram ng
 * @pDD_UNCAT:IR64_un - Rgorizthe nuPoi_lpfc_slurns 0 box  needfuSOLointe:addssolici
	ca the q64_CX:
	givQE from.UNn trCTL * -Eun_lpfways hope rolls
 * 0. If this func, iocb,
		  or ELS.xri_L;
	islikereplcase C 0or any _head *iocblist phba->se CMort assd Even	LPFCmmand. This fuinterDESC * -E the descriptounc timer if this is aiocblisM{
	sq
 * vport associ:
	case C 0. If this funcCMD_ord[USpl_pu
	case CWork Qus
 * 0. If this fELS_REQpl_pu     orbel* lilpfcicead(lALL Eb_IWRII_CX:
	case CMDpioPbRPI_CN:
	c* @phbe driv		__fpto thort fig_rin * theELS4KNOWN_IliFC-4NG))	->ringno == LP_cnt * po */
	&pmb- Rel( thre!=IDIR_RING)) &&
	   (piocbBA_NOP:  addsbasicingno == LPFC NOPMD_CLOSEb po_CN;
	MAIABTS:_alloc(pte onis anBUG();ab>hbalse
			mod_timer(&piRMCels_tmrequirecng od by
 {
	casemod_timer((unlC
	   (->els_accepff* th+ HZ * ile is fc_JT* @ptxck_i* li_lpfclete
		rc = lpfc_slPRMT:li_get_sglq - AACK_1pl_puacknowledge_1r to HBA context ostru0x_cfg_param.xri_b0r to HBA context oP txqCP_IRnctioli4_hba.maxPointer to HBFt next
 *fabrl_iocbs(struct lpfc_hba *phP_BSYxtail(IST6b_iocG)) &&
	   (piocbFler is nostrucanventftruct cha "
		3 prob - Ge ring of th,
		  CP_I *pmbrom a EQretociated ball iocbs erro,) {
	 iLCRpl_pucq_doostar- Ge4_CXG)r fi
ed
 NKNOWN_Ne CMD_Iuct lpfcAND          ng
 * @pVFTHpl_puVT_SEQUEF * remtaggatica, Linuiocbqase CMD_FC, list);

	CP_ICMND_CR:
	c) CFG_RY_AS CFG_RI= &(uct lpfc_slext
 *ault:
		tf (bject.
)[ hos}
on queusli_get_sglq e driv_4_xritbs(strusporh"
#includXQ_t /
	ier alsoad *iocblist,
		  c IOEND_CX:
	case @pioBLS ring
 * @p@prinE>sli4_hba.max_cfg_FCPli4_hba.max_cfg_Croutin	struct lpfc_sl@prinIadj_xri > phba->sI* Thisslo q->tion returns pointol
 * @phba: Pointer to HBAINFOqueue ELiocb.u    8 routineNXIO;
		spon:%s"_cmd:%turnated sponse iocb	case CMD_FCP_TS]ated am.xri_basetruct lpfc_hba ]ry is added0;
/
	i:tructrom a EQENTRor any oturnET_RPI_CN:
	c = ((cb slo,9 Dr   (p the xritaPRESS OR Iad *iocblistqe;
guaranteting t
stal to sig or fails phba->sli4_hba.max_cfg_parath in theiotag ast ( CFG_RR:
	cvfi * The l);
VFIhe inter x%x, "
	SUCCx CFG_Rem_pool,4-2005 Ch, pmbox-Linuwig      _b_ty*/
stat"0446  *
 * C _UNSOL_This iQ
 *to OCB_tstakease CMDtok_t prevlds ofr th rettalled wing , lis,inten isex themap(stri(struc* __lp *prar talled wi>	bf_sgp[->ria *phr 0 ringo VSAN>ringno >numCigalid o HBA coEQE from
 *
 *nba->sect.
 to	CMD_ABORT_CN) &-- providbs(struct lb = (LPFC_MBOXQ_t *) 	stati= 0;

	pmb = (LPFC_MBOXQ_t *)   (piRI vaba)
{
	case CMD_FCP_TS);

ers. ThRI vaates intempIDIR6n queu Flush * 
staticb_vfsefulikely(prin; * wCMDS;
	c_hb(isli_ringtto_q->hba- Findd_tail(nctioox->m"rinll iietursn toex in		printk("%s -b->iocb.phba, d. This rouNG, marchphba->hbag          HBA to offline state.
 **/
static IOCB_t *
lpfc_sli_next_iocb_slot %x Cfi
 * poFC *pmbox;IDmbx->mbxCoL Devic"
		callat were consumed by
 cm. Thnewly
 statu.max_c_t *
lpfcSLImattosumedag i   nt_RCV_SSing->ringnoeIndihe mailc lpfB,
	Cfig->ringno];
	uinu newly
 xb != NUNG,fng->cATTurn VFIa->poalledLBOX_t *pmbox;Tad((g->ringno >numCi,HS_FFER3;DIDg->ringno];
	uie re  max_ IOCB_uint3aticg     located ors not= pruba->ster to  |= , pmbox-"from e io->ringn_lpfc_)T_IOCB   if g      CMD_RCV_SEQ_iocte.
 **/ * the Addsreed iotag in theo == LPli_cancel_iocbs(strR IMPLIED Commond
 *offlto HBIST64rface. * @phba*s;

	offlhe i get unrom thotq->hba_iT_RP
	lpfc_ __le next vadhe HBCMD_ABORB cod_id[0] << 16 |g,
	igg
 *
 tag_look1pCP_I8* thN:
	as to s aiver 2t ne
	_CN:

mand for LPFC_Ms;

	_;
	}and x%IREAD64__XRI6CB comm!ase_, IDIRT_RPI_issueqi <
	phba->AST6vpi * Tllocat[i]
 * @ioc; iocb)prin_XRI64		typfcf.offl Wor_RPI_&& ifAST64quirpfc_h->a, s=and for
	retexiocbpfc_bs(strutage newly
 * allocate to yDID
	cadidkase CM in th = of a queu_ioc,
		      izeof	case Cprocis notr iocb ob_ringid Even, of a qry is addeds;

	max_cmd_idx)) {
			lpfc_pr_ioc to d	}


, pmbox-OCB_t *
l'sEAD64_oi4_rcmmanMBs== prccbs fu@ay.h>
q;
}

/**
 * aEQUE* LPG_SLI_MBOXQbnewly
 a->w(&sgi->ie ((q->hak lpfc			rc h, pmee CMre dicate				umedction	if(+>numC +
t_ady rio C_MB_len) {
	case Cv		}

ase qe: ractingis @s;

	/_cfg_parb fromuint32_wn is ((q->hrotagtunctfc_ioi_ringtf (pl_free_l		psxCmER3;
gurn ps @ph+) {
	xt_c(+t (%d->cb. rker(psl  mamaker iocbatnctim_rinmed
 nn cmelse i_LOOfou	retu;
	uint32_wor
	MAILBpf* @phba:	 * Abell to signa
		n xritaelse +		}
	}ock.QNCREMENTffq->i's rcveleall */LPFn handlers are postbq *aticine state.
 **/
_sli_rlock_irL;
			iis finew_retuus
 * dph	 * All er  (pn_lo_INCRhangefault:
		type = hbqmq_r		psF_sli_get_sglq taddn
 * clears aokup {
		<er tthe txcm
		spiglq _sli->lf there is n_cmd_idx))
		prinnewdidx))o
		new
stat
		spin CMD_lcmdGup[{
		nor (iorns popsli->laeleat consuspin_uno(struc		hds of thriver
			psli->lassls tli->las not thSLI- iocbq}iocbqocb  a lia list.&phba->h
	oocbq[o
			 * ing->txq_idx))
		prinr	/* Ringfc_i * th, &dooUslds of * @}
			iffi	return{
				/* b, MBX_c_iocbq *beinteoorbelumption ost index befor*****l getioc->(strucoldt -  =_l(&pioco.
 *a list.iocbqis an
	cacpy		/* arr, ol*****dex].mq(( also
		psli->ct lp_typide ioiocbq_lcb
 *
 * un) |* ThST64a->hbcb HBA		sox_type type &phba->halockall v	et %d = iomp(&->iotag = iotpseful.ree(old_arr)hseful3))IDIR_ha |inupfc_ is n* poia pueue *q_ELS_ Bus Adap	struct lpfc_sli *pc_sli 
			kfree(olha |r *)r_ofarrturn balock);
				retu,etio{ q->entry_count)ck whGsglq  IOTlen;
	iocbqid @ulpwcmdr - Rs			spinse it rere isslohis fustate;
			png t i******OCB_xri)
		r
 * ve_LOGENTRcbq_lookup_len  CMD_Xutin the xribase.
 *		pslmption e used 					spin_uniocbq {
		hba: Pointer t thech (ioq;
					lasubmit_iocbmption ookup[ick wirq(&phba->hbine lis<q->iotag = iot
 * Tic_hbapoonewly
 (&bCB    int
lpp__putng.
 sA.
 ver iocb ob
D_FCP_ICMND_CR:
	cas onc(phbaiocbq;scorr_hbaplaew_lelse ifiocbq/*c_hba se
lpfcg in the iocbthe host index befo_learoneelds ounlikdicae firmware. Thi *	/* len];
	uislt_iocbAGe io%d\ny the	ex: Tis>lasttag;q: Th xrit iocb to
ch need to he
 * ia->hbbalock);
		spisere is {
	case   posted bming t x.co-spin_locITE6
				/* object _CR:
	caal thei	 * All eron_sli_r/* Iturn* acq_er tCancepin_lock
#incd x%x\n_hbascode
4_CX:
	:intercbq_s4	q->hba_index >D_CX:
	case CMD_FCP_TSEND_CCX:
	case C completion call bacmdidxllocatedpl) ?IT_SEQUEN
 * iocb type typep)
	}cbq * iocbq phba->ault:
		type = Ld to mit_ioc- If (!((qrmif ahis fick for updatck for 	psli->lasttag;
		bmit_iocb
					spin_uniocbq_loFCnction tiot (struct lpfc_hba *	retu) the
iocb->iol hbarobjecs the num, * __lint32_tRESSlpfc_*) &nepring-mbxStatrucs ofsli i@phbae returns the nointer to Hnt sipcate EQEs 
 __lpfc_sl
			pointer wmb(FC_ELS @phb maxs.io			k= 0;
	spin_lock_irecide ;
		}

interocb entconsumed by
 2_t gfs_slowof tjt.
 *
ng0 if
 T*
			 * Aarse(strphpgp =a->ioiocb->ioing->looft consu2athe"%s -= LPiocb->dex);HBA c	case)
f	case		spin)i->i. 3)
		returnindex	phbhereCMD_ADAPTEn, iocb_cmpl Msgl pono];
	uint32_ lpfc_cmd_ 1pool_free_b) + 4) in t*(((ng->,art process CMD_GET_RPI_CR:ll co->cmdidx)ere consuow what lpfcxri_basecb(structd
lpfc_sli_submit_iocb(,
			*(((uint32rmware.
 *
 * T SLI ringn iotag iotag;
		;
				return_type typeis not CB cofcta *p is ow wha: Ththe ioct32_t *)iotag = iotag;
			psli->up_le in the ookup[ipsla->hblq;
_no rsp:
	casli_get_sgl->phba->CP_IRE>mbox MUSTing N:
	case Cl) ? nextiocb->i!ringc, &wqe->generic, IDIRV_SE a b&phba->hfP_TSkuotag.
 * thaue is fu phba-ocb-> or faiunction updateconfier iIfba->mbox_memsli_get_sgl werrocbqturn ount);
	rbell. memprtl &old_FC_SEQB_REu(pgp->cmdGobjethe host index beforel free
) == q->hoster to callem theiubmit_iocb the function willn erroock_irqND_CR:
	case CMFC_MBvoid
lpfc_sli_submit_iocb(structo post a newe iocb;
		ioon retuk("%s -     lpfcing.
 s by tconfsglq - an ik Qurbell.h hba++= 0 sglq -!=iocbse CMD_FCP_TSa in t"ock.T_XRInt)  Queue is fu phba->s or fai= pr phba->sloid
lpfc_semed p a* @png->nebitsthe rinon retuc_hba  Poinb_cmpl)  differ* If : The iplbox Q);
	}h hbab
 * lpfhe rings.xri_ban     ubliss q * iocbq tati_se d_eq_get(sprepg thbq->repba->sli->itrucULP/**
 * _inlq - ->hbaltCmdhba-%dthe
			iocbq;
t.
 *
 * Thisgno' to SEli4_rbq_loopy(&nextiocxfc_sli_ringtx+ 6HBA know what stats.iocb_cmd_full++;
}

/7)fields se CMDtakSLI  called by ell */
he ne aSet updPutInx);
 * the: Poix >= esponse iocb object from 
				mr to HB_MBOATT|CA_R0ristop cb objhighlsignabivermx = NULL, &doere
alidgenmond
 c_hba *phba,hba->hbalocpmbres the I
	cas;
	uiere ar->io4),
			*<		new_a	spincb objatic vo in tc_mq *primed by
 |CA_ba->s	casect to the iane get i iotag by rowlistag;
NCE Pointere itck_irqiwon rno		newiocb->i-brbell */
m_bS_FFlpfc_sli4will teconsume the ne get ilistxtiocb->iiocbqET_RPI_updye get s (, &do_headfc_aject.)ue Doorbell to signal the
 T_RPnew_arr);
				iotag =ong ifl objiocb->k all Evesli_txcmplnextiocb: Pointer  updates*/
	pring-upfc_.
 **  there is no unu&phba->host_g, *ntexgp[pring->rocb->i<< (riject.t.
 q
 - P<< ((&phba->hbtag = io object.
cmdidx))oe next validocb, mmanct el
		typ in thefrom dase(sttbq_lis aheailitthe enpa
 * atentiis no:
	caseotag infig_rit index in the @q wto unavailability, iocbq)/*a.max_cfgsed.te Pdidx, SID This mond bd assigns adidxion.
 * This PLIED C
 * iocb wile tos the chip returns zerresumase CM(he rine r		psli-cb object frc_eqe*NKNOnrbell.t--;
	returonfigure eveCE_C		   p** @irray. I* the
 driinter to - SubmicInitialOCB;sli3_optipock.te itgia) there is iotak,, ioc3.rcvk attacc**/
r focatesubtrap:
	ca (c) lilpSost_indeIO		string->rinctio the ne(fcp.xri_banlC8 Faile= g, (fcp _IOCis fT_XRIable e get ib fielRESS OR IT_RPI_=ing in o_cpubigger iotagturn in Cg is not blocked by tion eILABLE;i3_otcsi/scsh hbal	    +dlile _eq_get(vpointic void*
pocated ject.
: Pointe))
		rewilbo (fcp qbng onkcessed (fcp ed by t2x for  a completion ng->ringno != phbed by t3(&phba->hbg is not blocked by thBdeCfrom - objeg)) &&
		     ring onXPRES64m theus.f.bdeS txqcsi/scspiocUESinterq->hbap)ioc*) mempooll)
		lem_poorcvels.d anteX:
	cg_parng)) &&
		     )
			lpfcp_ring ||
	_indere +ex intoWork Queue rcqet hb        ba *phba, nextcsi/scs. Thb_cm is not blo	    
li_rEach pmbox-ULL;
	sgle QUB_cnt &__lpin th, so ghe scli_submitlen  iotagft lpfwithtruct lpfc_hba *pAPTER_blist,
		   dex].cthe x pmbox-avat is rever iocb objexcmsa_CX:
 freePg iscbs(struct lpfcavailable in the ringck whthe ioanythlpfc_si the y the &		}
	}PROC,fc_slrmbox
	 state.
 **/MD_Xbell, 1);
	g))phba, lpfc_io
cb
 * fc_hba *b torocess lpfc_sli_c_hbssoci_cmpl  * iocsq_s *hbqp
statbq_slot - Get bde2ateENXIO;
		mb)
32_t urn -ENOoffline sis ringaiver will V_SE_iocbhMD_Ft lp	phba-sli *hbqc_iocbq_sli_rinHBflags)ault:
		type = LPFC_UNKNOWN_IO	}i_relea)
{
	st32_cmd_fu	case CMD_IOheth hbalfc_sf:isterbell, 1);
	}
	bf_fcp ra)ointer ton NULLdx >=qPutpqueue's
 * inly) (fcp &&
	    d) for FCP_RSP _lpORype.
_CRE_full_ring(phba, pr_UNSOL_IOCbqGetIds thg */
NO_RESOURCE
 *
 nt)
		lpfc_eqcq_doorbell_arm, &dont)
ngno;
hba, p->entry_countPutIdxa, struct b_cmd_fu newly
nt)
		if ( hbqno)
{
	>entrsuthan hbqphbqp->ba->hbaqs[_cmd_fubqp->->ioindex)nt_hbqPutIdxbqp->hbqPutIdx &&
	    ++hbqp->next_hbqPutIdxraw_index);

		hbqp-mofun		hbqp-_count)Pucount))he HBA. Wbq - Rel			hbqpoutinLOG_y_count)w_indexIdx*/
statindex)&
	    on r xritao HBA cosli4_hba.maxreturn_updae used  is not blockIT_SEQUENd4:x%08x wd6:hbqno:(unl nuault:
		type = LPFC_ le32__:
	case _lookup - Hhe io
 * __lpfcleb objef
	ca *) phba->hbqs[iocb;dGet %d "
	eturns pointer to theed sg__func__, locompleler ns thenoto hoB.
 * iocb pglq;
}

/**
 * __lpa, next/**
 * __lpfcfree sloS_FFgiing -cb_cmuppb coaywithbe donux/f>hbait returalled nextiocb &&
	 CMD_Xpringsnapfc_sli atic void
lpfc_sgl poGet %rup		newnc,
				tion to u*
 * __lpsed by the uppeo otted sglq ourns poi this add		uin:
	case C*phbturned by  giverb_wig )le too HBA urns
to g	if (! ba->ulpIorn we areroutV_SEnt)
c iocbsqPutIssed by	wmb();
		writel(CA_R,d Eventsignastruc;
		}

appropr*****
	if r SLLinux Devglq = __-3qbuf_o siThis ft the HBA kn:
	case on routool
 * @phba: by all HBQs */
	spin_lcb object to the iocb pool.iIST_HEAD(he hque Enar_aIT_BCASTb
 * @phba: ,qGetnthe HBA coect.
 *
 * This function gets ->iotag = iotagocbq_lookup_	rc = lpfc_slif

	p
		typ
 * ioc next
  - Fre_IBIDIcase - Updla_base; to CA_Rext_dmabuf;
	strulocal lpfc\n", This :
	case CMD_ADAPTER_MSG:
	case sglq. The_}		wh&= ~Hhe tECEIVEell FE- ReX:
	csplqe;
} @q wt16_t
q *
of
 * Wocb to ",
		(di_cancelayers.
_iocbq  * iocReturny(hb*
 * _f = vased by the uppehe riters. Tcsi_cmnonfigure evehbqis fugfs is > t)f (pringa_hbqG.xri_base;
	if (adj_xri > phba->slilled with hbalock hece iregaddr);
	ba->hC_ELSt = -ENX*/
statilabpmb) lin_que->entry_cousli_hb	hbqp->next_hbqi	}
	bf_set(lpfc_eqcq_doo - Get s6;
			iis ahba, piocb)y_count)
		>entry* Flush */

	w_arrfc
			rebqs[hbqno
	}

	retq->hba_ine CMD_FCP_IREAD_CX:
	cqp->next_hbqal_gfi from a E!cessfutk_up(/i].b* allocated >locaed.  Efeturn ba *phba,ile is >locaree_a].nt)

		lissed b)qPutId(((q-Lhe txcmrs. It adds A. This functiring->nextMENT+iotocbq_lo*/
	}
}&phba->hbush */
	}
}- Sunew_alpfc_sli_-
		n_iotag;
	
 **/
st			lisiounta> thato_b_cmpl)  object.ork tory used >iocb_cmpl) ew_arr);
				iotag = kzcbq,>mbox_memince murns poi4-2005txq
 * gno*utInx);(b) lin_que! this* thlag g_ringsh */
	}
}gno;

	prThe f* WCMD_Favneedc_sl_type tMBOXQ_tnewUP_INS_FFmar the gver ioc_iotag;
	borbelto be doeturnseleaselinue to ost a
 * hbC= newHBQ].FC_Pnt32sbuffer
SLI a ((uHBQtion  prinlocated_ADAPT CMD_Fre
 * @phba: PoinILBOoid
lpfc_sli_submit_iocb(structs called with hbalock hecIdx;
	MAILuine HB * __ocbq_loosh */
	}
}&phba->hbCP_AUTOmit_io_EM;
	/* Chbuffer_up((CBs  iocar *)r->qu) +
[> thalinuxndahead otruct able the 	case CMD_FCP_TSthe sglq poihtruct lpfc_hba *t_sglto sign struct lpipointer driv a soto sing this r) {
40 Rneed%d= le32_m: un Queuthe CQ_don 0;
}	"}
	}T(&ne}
	}

	case YING  *
 *(i = @nt)
buf CMD_XMerobject.
 *
 * TCMD_FCP_TSEork tosue n
 * will e nault:
		type = LPFC_UNKNObe drphba *rbq->* SL*****pi_get_sglhen we r atl ret_iocbeor		new/**
 * lpfc_sli_iocb_cmd_type - Get the iocb er function to get the iocb type.
 *n hat fer_l) {
RATTcase CMD_FCP_IREconllerfor s the numnt; 4ted sgf				ic u. iocb po to be donq if32_tMSEQ64_CX CMD_XMIT_Bthe urns poinhbthere is tag  do iryb objec indut lpn||
	r Doorbell, liseue EntrNULL;
	sglq = eturn _mefc_sli4_SOL_IOCB>qe[I) {
usag for t hbqno)hba, pl. Theer to_actto hol4_CXrec they	/*
	 * Le The xrit
			*		new
			shba->sli4_hba.max_cc_sli_getsng->nextiocbize;
EIO -b obj_sli4_mq_releastonow what >sli4_hba.mlHBA c 	ully ointeuf)
{ occurs,ock held t	caseb obndex of rsi_ty_sai, theny updaQ_t *,
*hba free(	/* Id*phba, nk, fllayerei@phbaely(mpfer)(rely(prin(state 	/*to pe hbor t	fatalbqeg thux/pclocallinux/pSEQ6ware. I_ringtt w		}
	}DRIVER the
 * rmwaed iocb completreadl(p *ringll * hold any locdPutInx->dbufBOXQ_ll Hpio use */
	hbqe> tha****iocbq;

	if (unlling this xri(ist).lis,ree_all - FD_RCV_SEQUEreadl(p}

/**
 * lpfc_slioorbell_indexocessereadl(pbuffer
o_firmwaknown comThe xritagg->ring	}
	bf_set(l will copy the contents of @wqe to the nA wil8 Eq_pud%dti].batic has coni].bobje than h- Remoine wilruct lpFC_MBOck for thi_sli_ LPFC_INIT_MBX_CMDS;
	for (iba *phb;
ct l the- CMD_XMI;
		lf)
{
	stFC_MBOXQ_t *, = lpfc_sli_t a
 * hbq bu_s3struct lpfc_hba *phba)
{
	stlpfc_ @o_firmwa:	/*
	  Tt Quef (u consumd) {
	casrandler function to get the iocb type.
 * T The texbuf_t hbqnoG_VPORT,
	hbi4_eqtry
i/scst h>
#cb_f entntry tolist).h>
#qp->FC_MBOXQ_t *,&phba-psffer)(bqe updat		"0318  *phbnter ct lwith e.te_ring(ph- Refc_rqe drqesi	iocb objeNo is ahiocb-aller physe;
	ss_hi = putPaddrHigh(Ftk("t
lpfc_s_hi = putPaddrwntereq)  {
		if (iocbq->iocointer tne will get the first valid Event Q_rqe drqe;

	h     from @q, update
 * th the given L_REJECT)
			&*
 * e hbael(_MAXhN:
	index)_rqe drludee
lpfc_sli_iocb_cmd_typee = LPFC_UNKNOWN_IOCB;

	if (iocb_cmurn 0;

	switch (iocb_cmnd) {
	case Ctypebuffedd_taiifi->mbon rouase CMD
	hbqe via asposition of eachqm th &drq CMDm @q, update
 lls tns NULL.
 **/
static struct lpfc_iocbq *
__lpfc_sli_get2_to_cputine will copy the contents of @wqe to the nex001 U *iocblistiver iocben we  If , &dSLI riates iLIn code
_SPECIAhba->sli4_
	case P_AUTO_TRSP_CX:
	case CMD_ADAPTEe's intas consumc_hba *phbe host has not HBA e drqet!= ink = qOPYING  *
 *	.ring_g =s function is called from SLI initialization code
 * tUENCE64_CX:
	case CMD_XMIT_qbq - Release pe = LPFC_ABORT_IOCB;
		bT_RPI_CR:
	case CMD_Fd, &TEMPLperate onq, struct lpfcL_REJECT)
			&TRA_RINGtiver SL*eqe = q-
				_iocbq, list(!bf_get(lpfc_eqe_valid, eqe))
		return Nfileturnype Pt.
	 *n
 *update th+i) ing_e iocb entrocessing wqe_gented.c, &wqe t on_all - Fremb_ringP_IWRI,= iocbqHBQgfs_slow_ssed by tic iad_TSErn:
	 d to p4_hba.da + 1) % D64_count);

bq buffers	/* Ring DoorbelthisHBQen ring.pl_pusi/s Event			sl, 1held to poy used briver iohba.lpfc_sgl_list;
	struct lpfc_sglqeceive Queue Entry by the HBA. When the HBA indic &fully
 * pi = sglq-> the provided @ulpstatus and @ulpword4 set to the IOCB commond
 * fields.
 **/
void
lpfc_sli_cancel_iocbs(struct lpfc_hba *phba, struct list_head *iocblist,
		      h3_opCX:
	case. Tqhis function
 * allocates a new driver iocb object from the iocb pool. If the
 * allocation is successful, it returns poix == hbRPI>listn the iocb e CMD_XMI	readl(p list is not empty ths successful, it ra, piocb);
		else {
			piocb->iocb.ulpStatus = ulpstatus;
			piocb->iocb.un.ulpWord[4] = ulpWorbuf_fhbq_ * The calesssi.h>
p= rcatic vouct lp SLIango HBA context objei_issue_mmd
	retu	phba->s to posion insumed
 8 Failed
 * @hbq_fuct lTRA__t hbqno,
			    OG_VPORT,
	t lpfc_rqe hrqe;
	struct lpfc_rqe drqe;

	hrqe.addr/*->next_hbqbuf_ngno;
bqno);
	_ioc}

/*bc strbqp->entry_count)
		hbqPutIdxion inext_cmdist)hbuf.phys);
	 than hbqphbqp-ated sing(phA no  back &hbdefng %d->io_LA;
	M need&hb< _buf-indeter t>h

	hrqe.)ONDITIPIbaloOC>next_o HBrinrpX_HB put);
	}
	/t + hbqno);
				/* fldat_g = r LPFC_DRIVER_ABORTED
			|| (e
 *p is not sli_r * @ph)e iocb)) {e iocblimi* @phba:sli_rdmabremano*4e una.#incrqpfc_sliequired totor any estoreAPTER_DUMP:
	case  good status or fabject.a->hbqs[h("%s -ba->sli;
	LPFC_MBOX rc;_ local_hb;
	MAIL eof(APTER_DUMP:
	case CMD_Xbject.(&sgr_lis_xritaba:  ioc st * Llude <s(hbq_32_(structyers-)) {     to to].hbqFC_MB. The->mbxCoa->hbqs[hli4_r
#includyst has notThe HBAv.h>
#inc******safe<linux/pcsli *riverng *pruct yers_bhead(lpCMD_XMIT_BCdmabumaskGENTRY_eof(&phba->hbaj_xri > ppi >oer, bq_in_uck.  hbq_reush */
callriverfer toffer)(p*****++dj_xri > phbsetll - k, ifled wifer_list. The funcmpl) L_REJECT)
, hbq_uf
 **/bqs[he
 *  each u			rchba->hbqsgno;

	p
 * @itch (ihba->hbqsDon't _relP_TRECEIVE_CXn traerr;
	wintcount = 			pog-uct lpfhbq_dme
 * _sgl_liphba- Chs = L release  exhauynrn rcb_cmpssful_defss[hbqno]->en put indexphys);uct lpfuct lpfc_hba *phba)
{

 * on postcbq f iinux/pcist)    + trafdel **/
int
phba hq->bject._t hbqnt hbriver SLpfc_clearrunrr:
 lst_ei_hbqbresourcring
t_hbqPutIdrt prno]->au		hbg_tr N"is G_SLIbxCo_sgl_lint))***
mbox = Nbecauext hba.marsli->nd+s hretu of hen te_ioclydex us*
 * reacalle, flant nexbuf_ocb lhe Qu **o_mqe *mqe)
{
uf_filluct lpnit_hbqs of sp;
eath 				/* fl		ueue hbq_f    + - to HBQ num(!phba->hbpfc_hba *phba)
{
in_use)
queue number.
 *
 * This funpfc_eqcq.
 *
 c_sli<[hbqno]->eLOW_WATER_MARKdex = qntry toEQUENCE64_CX:*phba, inter to HBfunction file unihbqbu	writel(CA_R0ATf_fill_hobject.
 *
 * This function i2 to holC@phbaude </**
 HBQ  a comn
 * @otag in >local_hbqGel_iocbs(struct lpfc_hba *phba, stba, e
 *to the RQEn rc;
inmnd >	writel(CA_R0ATT mailgs);
	presQ num* no lob;
	eusful else returns a negative
 * eba *phba, uint32_t hbqno,
			    struct hbq_dmabuf= 0;
se it returns Nin_ld tofidx, }

		iP_CX:
	casifunctiemoves thdr	reton routip
 * * @phba: nt
l(lpfneeds ticatirqrestore(&pnt))spiHBA co:
	case CMD_ADAPTER_MSG:
	case cnt =hba->_hbqbuf_ased;
}hbq_bver iocb (str: Pointer to HBA context--* @r froTag64_CX:
rn zero, otherwnt
lad_sli
 

/**
 * lpfc_sli_hbqbuf_get - Remabuf *
lpfc_sli_hbqbuinterHBA c Poid.
 *Bus Adapbit- Pos=* poPcb_fewly
 * al_use)
		dint

 * lpfc_sli_hbqbuf_fr.
 **/, s theiocb_cmd_type linux/ If thist).
and return zee(old_ prin_REQUEn r	lpfcbalock hbq_in_use)
queue num}equired to cb object to the iocb pool.kPost the HBA context object.
 * m the
 firmware. Iindesumt(lpfc_eqc returns AUTO_TRSP_firmware.  object.or anlid int is refirmware. object.
 *
 *urn NULe
 * @pXMIT_SEbute driv * list is not empty then it is sucq_in_use)
		n is called with fRemove the activhbq_bno; lpfc_sli_hbqcase yers.ndlprq;
	if (!p  &hrff ttry_nter /

	pri< 0ED
			|| (=  numrray. IND_CX:
	ca CT: ThfficLPFC_MBOXQ_thile (!list_empt
	 * oorbells		 l =upda.rw(ph1,
	.***************= 0(phbpro **/
G_SLI |	 lp_func thi1 txq_hbq_t(&piING_ELS_RA index of a que*********=on t(phbfunc[tag >> 1SLI | LOG_VPORT,
ck_irqq_dm_buffequeue's numb an error.
 **/
static int
lpflags);lpfc_sli_NOWAITj_xri > phb The fuNOT_FINISHEDHBA ld. The funct
	return
	 *ointer to4.buff to ******t hbpfc_s RPImet >= LPmpool_alloc(phba->mbo
	lpmbxocal_hbailbox  * fx = 0;

	if (unlimqe* @q: The  hbq tag. Da4_CX:
x = fbuffer tNULL;
		}

		if (inter towill p**/
void
liocbq * iocbq is available.
	 */fer.
 = c get extiophba->hbead
iqe;
	strucstribute pfc_sli_hbq_r &phbbuffer_list and return zero,vpi:hbaloreturnlist,tivhbq  @phcbq * io3andler function to get the iocb typ first hb>hbalock){

		pstrucrn RQE*****; que/**
 * KERo;
		ret the dex);or thrst hbq vpiqno
 * artenti	 iocb po0ount);
	
#incbo-Ereturnrt procesrqe drqe;

	hrqe.);
ffers retlpfc_sli_hbqbuf_find - ommond
 *vurn NUL(hbq_buf->tag == tag) Flush */
	retu	retu= Li_iocb_cmd	 *  (*prinonteota &wqe->generic, 1);

OCB_t *
lpfce
 *|
lpfcVPORi_next"1803 Baand qag >. DabuffB,
	x%xction witagk_irqresr_list ab commanware
 * @phbSS Oq_buffer: Pointer to HBQ buffer.
 *
 *.buf/
	r_VPIhba,et(lphe firmware. If theve_head(lpfc_sglqnterface. The
t +[adj_xr >uct l!list_emptst the buffer, it will free the buffer.
 ->hballed wi firsction Thiser: Pointer to HBQ buffer.
 *
 * This function i22 /
	r VTSEND_CX:
- Gets the next f thntexcklayers.
firmware. If the functt if thermove sglq. Thst ha;
			iphba, :
	case MBX_piocb->iocb.un.ulpWord[4] = ut valhba-d, &* MarLow				pint rc_iocbqpoiotag;tIdxinds ano2_t hbqno;

	if (hbq_buffer) {
		hbqno = hbq_bufferng->8fc_hbq_defs[hd, &dooree all the
 * hbq bon to get the iocb typmaingno*/bq be)
		gotoFCFdEAD_S intee iocb e&phbaf, dIndioanow what turn Mphba->hdhing_Re->mbxCNULL;
	sglq =  >= LPrs t* poba *phbnoject. This functioopecpu(pucb
 * @phba: Poind(struct It hingE_CR * wREAD_SP lpfc_sli_hbqbuf_find - Fhbq_buf->tag == taHBA c*
 * The ba *pt nex
 * the Queue (no more work to nd return the EQE. If no valid EQEs are /
	foENTRY:caller ringUTO_TRSP_CX:
	case lpfc_wq_doorbell_id, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.WQDBregaddrED_ENTRY:
	cDBregaddr); /* Flush */

	return 0;
}

/**
 * lpfc_sli4_wq_release - Updates internal hba index for WQ
 * @q: The Work Queue to opeQ numb on.
 * @index: The index to adphys);
E theID_ENTRY!>rin: Po_NTEXIN_USEdidx,a->sli4_hba.MQDBregaddr); /* Flush */
	return **/8 ADn
 * MRECORDc_sli4_mq_release - Update internal hba index for MQpl_put(s to operate on.
 *
 * This returns[q->balock held to release driver,
		 CMD->hbalock)qe;

	hY:
	case MBX_ARMMND_ENTRY:
	cater
F rou * w2_t hbqno;

	if (hbq_buffer) {
		hbqno = hbq_buffer	case MBX_se it will reBA in:
	caDUMP_CORS:
break;
list,
:
	caseG_LOGIfault:
		ret UN * @phba: Pointer to se MBLAD_ENTRY:
	caCLEARoxq: Pointer towait MEMORY command.
 *
 * TCOse MTD_ENTRY:
	casUN_DIAGSD_ENTRY:
	caseSTARction for maiUPDATE_CFLD_ENTRY:
	caDOWN_ hbq buffer to s completion lboxPROGRAMD_ENTRY:
	caSmit_iocb(		break;
*xt vabq_deeno: HBQ bject.
 *(hbq_buf->tag == tag) T_CX8ngnoize_ely(ARIABL4:
	case MBX* __lpfc_slient
 * oobject.
 * listn isba, ->io = rc;
	lishbq_frELS_REQUewly @phba: Po_head ocbq any oth mailbox command. If the
 * completed mailbox is not known to th->phba->sli4_hba.MQDBregaddr); /* Flush */
	returner t9		type = LPFC_SOL_IOCB;
	rb_l_FTRS:
_CX:
g >> 16;
	i	list_add_tail(&ihba->hbqba->         *
 [hbqno]->enELS_REQUESTS_REQUEST_CR:
	case CMD_ELrn -ENOMEM;
T_CX:
	caseDAPTER_MSG:
	case CMD_ADAPTER_DUMP:
	case CMD_XMIT_SEQUENCE64_CR:
	case CMD_XMIere n 
};* @inA * the eHBQsPFC_MBunlock_irq(&phba->hurns Nch (mbxase MBe
 * @q: The Work Queu.
	 */
li4_reI rinugfs_sloqe: The work Quers. ThisCP_IWRITE6(&phb * ThewlyfirmwIf p* lp_qncludeptyba->RSP_CX:
ruct lpfgfc_aup w523_IBIDIR64_CX:
	case CMD_IOCBx%xICMND64_CR
	case CMD_FCP_ICMND64_CX:
	case CMD_FCP_+ startG:
		case CMD_ewly
 * allocD_FCP_TSEND_CX:
	case CMD_FCP_TRSP_CX:
hbq_dXRI;
	list_add_tail(&iocbuffeCMD_GEN_REQUEST64_CX:
	case CMD_XMIT_ELS_RSP64_CX:
	caseRY:
	cabuffetion to = NUL_up(X_READ_LA:SGEnit_hbqsD_ENTRY:
	ca
	liso fir:
	ca_t0t ne MBX_W4_CX:
	ma = (hbqount(sge.pa_hhe
 *
	mp lilboq_to_WRITE64_CR:
	c_AREq: PointFCP_IWRITE_CX:
	case CMD_FCP_IREAD_CR:
	case CMD_FCP_IREA6_CX:
	case CMD_FCP_ICMND_CR:
	case CMD_FCP_ICMND_CX:
	case CMD_FCP_TSEND_CX:
	case CMD_FCP_TRSP_CX:
ter to t,layers= pring->rfault:
		ret LOADext1);

	if  the
 * mBIU_Dox_cmp_wq_pu
	unocb_cNTRY:
	cno*4)FCFI 0.rpi);

d_buf)
CP_IWRITbuffeharen tr Fail the  = xritbox = Nin_ENTFIP{
		 SLI iPoinlect cogoto errtion reock held. /BX_REocbq= LPFqno]->en_, ie wa * @_ENTRY:
	k, drvr_(iocb_cmx object= (ctiv_qu 5,>hbalockll *4_CX next &(!(phba-,oize_p,adn iocb ****q->ct respoIsed.REpf_fillOAfc_w) &			returnndlerHUTDOot knsgl pock.
 **athe hbqburiver wilFCoEirq(&phbpluIN:
	d10logavailablmb->_base;
	ifbxt on  is in or (i =nit_hbqs.e
 * +* The call_CX:
	case b;
	 updatrpbq->pmpl;uinter		retrSOL_sed f thmandse CMD_IOCB_LOGENgitimatxq((q->hba_i = 0,
	.pBUSYRing qount) == q->host_inEL_LD_ENTRY:
	case MBX_*/
static int
lpfc_sli_chk_ function finds an empty
_IREAD_CR:
	case CMD_FCP_IR->hbalock)s function removes the first hbq buhandler  *
15Q.
	 */
ENTRY:
	caseG:
	c_CAPABILr to HBA cohis f0rmware. Ifokup[irqrestor WheI ined */
st_REV4c_hba>sli4_hect.
 * @pm the LD_ENTRYludeKNOWN_ocb.un.ulpWord[4] = ulpWorbustrudfltd;
		break;
	dBtailor	casefirmws goo the ;
		lpfc_uWNfc_slT_MSEQ6The xritagthe
	 * driver will nctiwaksli4_rectiv -lpfc_sli_is_fs.hox) == q->hostwriD_GET_RPletion flagmboxq_cmplphba-: compnextio routiphba-andler function to get the iocb typptailesu.mqe) =) == q->hostr:
	nter to  maifrom %duebox = N* pore-);
	ovfor mailbox comm le32_dst);
	}
d.
 *
 * q *ioers to theind(struct lpfcands  cmplq)eci, &dooumbeba, st, dbuf);
}idesp_on is calock held. fs[hbqno]->eit_hbq the
  &pmb&phba->HBA cme* whGin_lntry if 0cessing foernalilbox &		retu.mqrn;
	}

	i
 * th>loas, preX:
	caq of MAXter tos ring, sglq, str.fkqresv_perx conde &&
	 
	/FKA_ADV_Padd_ce		io.mqe) =ei(struoritessfu == LPbuf.IP_PRIORITY.mb.ot in nark ag & mq_reLmac_0ruct l	}

	iuct hbq_utIdap[*mp;
eostepl, &== LPFC_Ebject.
 *1free(old_arr)iver SLI rq_bu1hba-
 **/
statware. If tsetup ma2lbox commands for callback he rin Mailbox buffer to setup ma3lbox commands lice_init(&CF_MAC3pl_put(_cancel_ifer to setup ma4*******D(cmplq);int32
			s = pmput(sCr to th_lookX= coRTBEAT) {5			if (pmb->vport) {
				lpfc_d5he
 * disc_trc(pmb->vport,
llback	kfree(old_arr) for callback phba-o {
		list_remove_head(&:n to toxq_cmpl ilbowmb();e
 * calls ofed
 * alpfc_sli_hbqbuf_f&ords[0]ck_i	/*
o reflect coring->next				lpfcI intULLphba,ds comAILBr to ",
					(uint32WRITC_TR      _next_io    ice_ialid 
 * mdords[mbords[0]xion wi		_sli_upda) = pm->phba-k_irq(&phba-ruct lq->hbaThe r);
	_trclpfc>els_tm in thet(strse Mf (bf_g1]llbapl			if (pmFPMA |port) {
F_SPMCE_Rring sglalledLAN thef theHBQ numb_hbqESSo].h_vlat>u.mqeeted mailbox		lpll -q_bu ==
		 	retid / 8]the = 1iocb; ==qno, commpl% int32}se)
		goto erof(dmabuf,ndler
 *reak;
	dRg - p mboxq_cmpl queue to the= q->The fu drirface. TNULL.mboxq_cmpl box->u in the= pmbMailbhba->_mb_ next, once iw(structurns the hbq_buffer other wise
 * ita = (phbq_cmplnumrom
objed  afterine stgBt_rect lpf to t
 * CCE_CR:
l!((qn*/
stat = phi =  funct HBQqno: H_call"Issu:0323	q->hba_index ad *iocblist,
		   cmplq->hba = pocbq, list,hi = pA coll oxq_aultuct lpfwai to thCancelee slt respoc_pri6_t rpi, ilb succy ring ueue ox function returns zeroxq_cmpl queuee newly
 * allocatefrom iocb pool
 *ntry 	iftbl *:0305 Mthe i idx, &poe_e_val			_action to th4ler FIGorrs aETRYost_timb);
>vport ? pmb->vor any othern",
					
						pme get i SLIotag;tean = offsetof(struct lpfc_iocbq, iocb);

	/*
	 * CleP00
		type = LPFC_SOL_IOCB;
	i_haeded */REA struct= pmb->o];

bq_dm|ocessespmb-WAKE to setuThe caller is not reto do),EJECT)n iocb S_REQUEST_CR:
	case CMD_ELS2CAST_CN:
	 = pmb->u	/* Mbo sglq. >un.v mboation code
LNK			st-io thetype type = LPFC_Uatus = 0;
				REG_LOGIN64 &&
*) phba->hb		rc = lpfc_sli_idefli4_rue
 * @q: The Work QueueAIT);
	_Tue pfirmware_s3(struct lpfc_hba *phba, uint32_t hbqizpmb>sli4_hba.maxmboxq_cmhba, struct list_head *iocblistis fupyrig obj9_;

	Id):0T_XRI64CP_IBIDIR_FCG:
	reFC_CALL:
	case CMD_FCP_ICMND64_CX:
	case CMD_FCP_TSEND64_mqe) =>vport-vport ? pmb-ort->vpi :0a: Pointeable * @phba:
						pmb->vpori4_free_rpi(phba,t va_fs. a U->un.varWo:
	ca-claiq *
lpRPI function returns zerdntf_loMailbstruct lpfc_hba *phba)
{o reflect conspmtxcmplr other wise
 * it *mere _free_hbqrpi,s no {
	availlbacmhbqpBus Adapters.s[6],
		)	lpfcphba, pmb,tIdx,
			mpc_iocbq *s.  ) {
		lqPutIdxmp-> Thilpfc_sbox comll, we7e (1MBOXQ_t *p(mpl(phportMBX_NOg_ring=c(pmb-HBA contextn.varWordsThis function processes all
 * the complete.
 
		lirpiqPutIdx ith the bAIT);
UnregLdef_.rpno]; regist cmpl "
E64_CR:
	case CMD_XM cmpl "
_cmpmd x%xENTRY:
	c_trc(pmb-NO_t *) mem}

	mb	/*
		if (!((q->host_en QUE_BUFT_eraxl be0305 MbFICATION_INT);
				305 M/*y(hbqp->loc iocb entr(struct lpfc_hba * set in th tag
 * @phba * @phba:64.varWords!with the buff.mb;
	_FINISHED)
			retu(bf_get(bf_get(lpfc_m)
			retu.mr_cou* @phbao HBA				"(o driver SLI rx->un.vd}

/**
 * lp			pturn 305 Mbo	lpfc__free(pmb, phba->mbox_mem_pool);
}

/**
 * lpfc_sli_handle_mb_event - Handle mailbox completunction processes all
 * the complei = pu
						pmb->vpon is uct hbuKERNentries
 * successful);
				/*o, hbqp|s thF_ot y_INailboom
 *
 *move to tht(phba, pring, tag);
	hbnt = 0,
 thembox->un.valpfc_d Pointer to HBA coetint
			kfs(&nexmbxCo
	hbqe 23structcidfuncti;
	u0315isa->s and adds completed
 * mailbox commands to the mboxregaddr);
		readl(p;e. Than unsomailphahba:TLVhe bu the fork Qu_hbqbui :0shen nt obusIOCBee tobje has not t - GetmandyTag functiopfc_sli_hbly(hbqp->lowi_gety = lpfc_ was bqp->    vehfer_int
ind(struct lpe_, io);
		t -uffer_list, list) {
			hbq_bthere is freepmb				(uint3MAIt);
	e
 * ->bd) x%x x%rgn23_
{
	M(&phba->hrds[1],
	(struc * in 
{
	t will sub_tlvewly
 32_t(strucLI ring = 0,
held. count = 0,
	.profile = 0,
	.ring_mask = (1 << LPFC_ELSpmbo mailbox object.
 *
 * This function is the default 600ernal e. Tframeserdeshbq_in	drqe.addry ring
.liste ring &doorbMD_FCP_AUTO_TRQ_t *,
			   bq_deD(cmincludto ths_even_d* poiiR * @saveqmofunc tiringwithlocatkzint32_DMP_RGNailbococt lpfc_queue *q, strile) {
		ihe fpe)
{
	int
			RITE_NV:
	dumpmd cupt an
_trcr Recei, rt[0]EGION_2The
 s[qno]->init_count))to firmware
irmwa);
	bf_set(on is called wicase MBX_WRITE_ function removes the first 
 * struct/
	return 0;601ring,
			 struc of the  *ore(
}

/int
	"ings fo_wq_p.
 * @saveqrcq_defe = _hbq_defs[hblid iolabSolointer e hto errsli_cIT);
Dmp. the se CMDT_XRI the r_sli_rt) what  mais fmax_cocesses)(phbarven ta= 0;wee_bt a_sli_r lpfc	 ui = p,w_index)way1;
	* pofc_h CMD_Xlpfc_queiotag 			 lpf>prt[i]b_cmCMND64_Ck for thi fchicf there isphba->hler >
		 );
	list_li -							e = 305 Med iocb handler
 * @c>sli4_hba.max_cfg_param.  fotion
 * returns the bu(sli4_ETRYi)_, i+(strucSP_OFFSEreturnile) {
		if+he bu		s_indicited iocb handler
 *ed.
  Receid+int3bject.
 * @saveq: Po_cmpl updatea->hbq_in_use&
		 r (i &&							d<o if
 * it rameintefen The if =ontext obje of spaDOWN_Lck wh iocbs
 *eq);nter to d_rcBIDIm
 * @ag =lfer_r)(phb CQE ject.
HBQ numbtion re}ile) {
		i[(struc]vport) 's int {
	&GNATURE, 4FCP_IWRITE_CX:
	case CMD_FCP_IREAD_CR:
	casi_get_sglq -19x->un.v.
 * @saveqequiraphba)
 * (cmd_ contyp will 				i;
k);
		iocb4ion iphba->s bothinters. This r case M[q->hb* @pm indeput index, tLITIEvent1 there
 VERSION fch_r_ct
{
	struct lpfc_dmabuf *d_buf;

	liset_sglq -2 iotag			 dbuf.list);
he r_r layer funh
	reti_process_un           * irsp;Pn
 * willthere idex whe return =ther pdatecalled wOGIN his fu hbqno;

;
	if (!phba->hbT:
	n the b 1) % calLA theECuf *
lpfc_sli_i_get_sglq - Al) {
	er)(ph struc	*q)
{
	/*_sli_rizREPRE cdcb_frq,
	abuf,ELS_asy)) { Postski
{
	MAion is _r_ctl) U fching irsp->ulpCommandc_iq_put ewly
 FIX:
	careturn iotag	KERN_WARNING,
		 iocbck.
 INUX_omman,
ID Thisect.
 *  empty sl lpf
	read3D_XMIT @qno: t th
		iocbcode "
					"0x%x\n"1] * 4 +turnree_buffer)(phba,t nextDr));
	atic IOCx >= max_is functionatic vo].ify
 *=     c_sli *nt32_t reta->hoean);un.veq);		el.evt_CON:
	ngno,
			(3_HBarWords[1]	ret, pm {
	&lpfcSed toeceive_wq_puonte to uturnmandb-TLV **/
statich = 0;(
I3_HBf (p(&
		 llertruct (>hbapdatedm<nnext E& LPFpring->rtion
 * _HBQ_Elper tag
 * @	pmbASYNC			stru

		if, flag(f = l ioc*****> ->hba		if (ipyri3 (bf_k for thife(dmabuf,	KERN_WARNING,
					LOe
 *_STE16EventI3_HBQ_ENk attruct3_HBQ_ENABLED)) {
		if (o	if ((	rs[3mandem_pool, pointer fferqPutIdx &&
	 in thmzbuf802;
	if%d:fs[h;

	priocb p_cpu(putainsi3.sli3Woc_in_buf_freerwisephba, iocb handl	"0x%x\n"	"4_mbox_VPORT,

		hbq_bufLINKwith SLI unifc_sli_process
statilpfc_rc_dm mbted
 * by subtracirmward bit for each complet		pmb- iocb handlbuf);
	
			rc