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
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"
#include "lpfc_version.h"

char *_dump_buf_data;
unsigned long _dump_buf_data_order;
char *_dump_buf_dif;
unsigned long _dump_buf_dif_order;
spinlock_t _dump_buf_lock;

static void lpfc_get_hba_model_desc(struct lpfc_hba *, uint8_t *, uint8_t *);
static int lpfc_post_rcv_buf(struct lpfc_hba *);
static int lpfc_sli4_queue_create(struct lpfc_hba *);
static void lpfc_sli4_queue_destroy(struct lpfc_hba *);
static int lpfc_create_bootstrap_mbox(struct lpfc_hba *);
static int lpfc_setup_endian_order(struct lpfc_hba *);
static int lpfc_sli4_read_config(struct lpfc_hba *);
static void lpfc_destroy_bootstrap_mbox(struct lpfc_hba *);
static void lpfc_free_sgl_list(struct lpfc_hba *);
static int lpfc_init_sgl_list(struct lpfc_hba *);
static int lpfc_init_active_sgl_array(struct lpfc_hba *);
static void lpfc_free_active_sgl(struct lpfc_hba *);
static int lpfc_hba_down_post_s3(struct lpfc_hba *phba);
static int lpfc_hba_down_post_s4(struct lpfc_hba *phba);
static int lpfc_sli4_cq_event_pool_create(struct lpfc_hba *);
static void lpfc_sli4_cq_event_pool_destroy(struct lpfc_hba *);
static void lpfc_sli4_cq_event_release_all(struct lpfc_hba *);

static struct scsi_transport_template *lpfc_transport_template = NULL;
static struct scsi_transport_template *lpfc_vport_transport_template = NULL;
static DEFINE_IDR(lpfc_hba_index);

/**
 * lpfc_config_port_prep - Perform lpfc initialization prior to config port
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine will do LPFC initialization prior to issuing the CONFIG_PORT
 * mailbox command. It retrieves the revision information from the HBA and
 * collects the Vital Product Data (VPD) about the HBA for preparing the
 * configuration of the HBA.
 *
 * Return codes:
 *   0 - success.
 *   -ERESTART - requests the SLI layer to reset the HBA and try again.
 *   Any other value - indicates an error.
 **/
int
lpfc_config_port_prep(struct lpfc_hba *phba)
{
	lpfc_vpd_t *vp = &phba->vpd;
	int i = 0, rc;
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	char *lpfc_vpd_data = NULL;
	uint16_t offset = 0;
	static char licensed[56] =
		    "key unlock for use with gnu public licensed code only\0";
	static int init_key = 1;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	mb = &pmb->u.mb;
	phba->link_state = LPFC_INIT_MBX_CMDS;

	if (lpfc_is_LC_HBA(phba->pcidev->device)) {
		if (init_key) {
			uint32_t *ptext = (uint32_t *) licensed;

			for (i = 0; i < 56; i += sizeof (uint32_t), ptext++)
				*ptext = cpu_to_be32(*ptext);
			init_key = 0;
		}

		lpfc_read_nv(phba, pmb);
		memset((char*)mb->un.varRDnvp.rsvd3, 0,
			sizeof (mb->un.varRDnvp.rsvd3));
		memcpy((char*)mb->un.varRDnvp.rsvd3, licensed,
			 sizeof (licensed));

		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);

		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX,
					"0324 Config Port initialization "
					"error, mbxCmd x%x READ_NVPARM, "
					"mbxStatus x%x\n",
					mb->mbxCommand, mb->mbxStatus);
			mempool_free(pmb, phba->mbox_mem_pool);
			return -ERESTART;
		}
		memcpy(phba->wwnn, (char *)mb->un.varRDnvp.nodename,
		       sizeof(phba->wwnn));
		memcpy(phba->wwpn, (char *)mb->un.varRDnvp.portname,
		       sizeof(phba->wwpn));
	}

	phba->sli3_options = 0x0;

	/* Setup and issue mailbox READ REV command */
	lpfc_read_rev(phba, pmb);
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0439 Adapter failed to init, mbxCmd x%x "
				"READ_REV, mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxStatus);
		mempool_free( pmb, phba->mbox_mem_pool);
		return -ERESTART;
	}


	/*
	 * The value of rr must be 1 since the driver set the cv field to 1.
	 * This setting requires the FW to set all revision fields.
	 */
	if (mb->un.varRdRev.rr == 0) {
		vp->rev.rBit = 0;
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0440 Adapter failed to init, READ_REV has "
				"missing revision information.\n");
		mempool_free(pmb, phba->mbox_mem_pool);
		return -ERESTART;
	}

	if (phba->sli_rev == 3 && !mb->un.varRdRev.v3rsp) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return -EINVAL;
	}

	/* Save information as VPD data */
	vp->rev.rBit = 1;
	memcpy(&vp->sli3Feat, &mb->un.varRdRev.sli3Feat, sizeof(uint32_t));
	vp->rev.sli1FwRev = mb->un.varRdRev.sli1FwRev;
	memcpy(vp->rev.sli1FwName, (char*) mb->un.varRdRev.sli1FwName, 16);
	vp->rev.sli2FwRev = mb->un.varRdRev.sli2FwRev;
	memcpy(vp->rev.sli2FwName, (char *) mb->un.varRdRev.sli2FwName, 16);
	vp->rev.biuRev = mb->un.varRdRev.biuRev;
	vp->rev.smRev = mb->un.varRdRev.smRev;
	vp->rev.smFwRev = mb->un.varRdRev.un.smFwRev;
	vp->rev.endecRev = mb->un.varRdRev.endecRev;
	vp->rev.fcphHigh = mb->un.varRdRev.fcphHigh;
	vp->rev.fcphLow = mb->un.varRdRev.fcphLow;
	vp->rev.feaLevelHigh = mb->un.varRdRev.feaLevelHigh;
	vp->rev.feaLevelLow = mb->un.varRdRev.feaLevelLow;
	vp->rev.postKernRev = mb->un.varRdRev.postKernRev;
	vp->rev.opFwRev = mb->un.varRdRev.opFwRev;

	/* If the sli feature level is less then 9, we must
	 * tear down all RPIs and VPIs on link down if NPIV
	 * is enabled.
	 */
	if (vp->rev.feaLevelHigh < 9)
		phba->sli3_options |= LPFC_SLI3_VPORT_TEARDOWN;

	if (lpfc_is_LC_HBA(phba->pcidev->device))
		memcpy(phba->RandomData, (char *)&mb->un.varWords[24],
						sizeof (phba->RandomData));

	/* Get adapter VPD information */
	lpfc_vpd_data = kmalloc(DMP_VPD_SIZE, GFP_KERNEL);
	if (!lpfc_vpd_data)
		goto out_free_mbox;

	do {
		lpfc_dump_mem(phba, pmb, offset, DMP_REGION_VPD);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);

		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"0441 VPD not present on adapter, "
					"mbxCmd x%x DUMP VPD, mbxStatus x%x\n",
					mb->mbxCommand, mb->mbxStatus);
			mb->un.varDmp.word_cnt = 0;
		}
		/* dump mem may return a zero when finished or we got a
		 * mailbox error, either way we are done.
		 */
		if (mb->un.varDmp.word_cnt == 0)
			break;
		if (mb->un.varDmp.word_cnt > DMP_VPD_SIZE - offset)
			mb->un.varDmp.word_cnt = DMP_VPD_SIZE - offset;
		lpfc_sli_pcimem_bcopy(((uint8_t *)mb) + DMP_RSP_OFFSET,
				      lpfc_vpd_data + offset,
				      mb->un.varDmp.word_cnt);
		offset += mb->un.varDmp.word_cnt;
	} while (mb->un.varDmp.word_cnt && offset < DMP_VPD_SIZE);
	lpfc_parse_vpd(phba, lpfc_vpd_data, offset);

	kfree(lpfc_vpd_data);
out_free_mbox:
	mempool_free(pmb, phba->mbox_mem_pool);
	return 0;
}

/**
 * lpfc_config_async_cmpl - Completion handler for config async event mbox cmd
 * @phba: pointer to lpfc hba data structure.
 * @pmboxq: pointer to the driver internal queue element for mailbox command.
 *
 * This is the completion handler for driver's configuring asynchronous event
 * mailbox command to the device. If the mailbox command returns successfully,
 * it will set internal async event support flag to 1; otherwise, it will
 * set internal async event support flag to 0.
 **/
static void
lpfc_config_async_cmpl(struct lpfc_hba * phba, LPFC_MBOXQ_t * pmboxq)
{
	if (pmboxq->u.mb.mbxStatus == MBX_SUCCESS)
		phba->temp_sensor_support = 1;
	else
		phba->temp_sensor_support = 0;
	mempool_free(pmboxq, phba->mbox_mem_pool);
	return;
}

/**
 * lpfc_dump_wakeup_param_cmpl - dump memory mailbox command completion handler
 * @phba: pointer to lpfc hba data structure.
 * @pmboxq: pointer to the driver internal queue element for mailbox command.
 *
 * This is the completion handler for dump mailbox command for getting
 * wake up parameters. When this command complete, the response contain
 * Option rom version of the HBA. This function translate the version number
 * into a human readable string and store it in OptionROMVersion.
 **/
static void
lpfc_dump_wakeup_param_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmboxq)
{
	struct prog_id *prg;
	uint32_t prog_id_word;
	char dist = ' ';
	/* character array used for decoding dist type. */
	char dist_char[] = "nabx";

	if (pmboxq->u.mb.mbxStatus != MBX_SUCCESS) {
		mempool_free(pmboxq, phba->mbox_mem_pool);
		return;
	}

	prg = (struct prog_id *) &prog_id_word;

	/* word 7 contain option rom version */
	prog_id_word = pmboxq->u.mb.un.varWords[7];

	/* Decode the Option rom version word to a readable string */
	if (prg->dist < 4)
		dist = dist_char[prg->dist];

	if ((prg->dist == 3) && (prg->num == 0))
		sprintf(phba->OptionROMVersion, "%d.%d%d",
			prg->ver, prg->rev, prg->lev);
	else
		sprintf(phba->OptionROMVersion, "%d.%d%d%c%d",
			prg->ver, prg->rev, prg->lev,
			dist, prg->num);
	mempool_free(pmboxq, phba->mbox_mem_pool);
	return;
}

/**
 * lpfc_config_port_post - Perform lpfc initialization after config port
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine will do LPFC initialization after the CONFIG_PORT mailbox
 * command call. It performs all internal resource and state setups on the
 * port: post IOCB buffers, enable appropriate host interrupt attentions,
 * ELS ring timers, etc.
 *
 * Return codes
 *   0 - success.
 *   Any other value - error.
 **/
int
lpfc_config_port_post(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	struct lpfc_dmabuf *mp;
	struct lpfc_sli *psli = &phba->sli;
	uint32_t status, timeout;
	int i, j;
	int rc;

	spin_lock_irq(&phba->hbalock);
	/*
	 * If the Config port completed correctly the HBA is not
	 * over heated any more.
	 */
	if (phba->over_temp_state == HBA_OVER_TEMP)
		phba->over_temp_state = HBA_NORMAL_TEMP;
	spin_unlock_irq(&phba->hbalock);

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}
	mb = &pmb->u.mb;

	/* Get login parameters for NID.  */
	lpfc_read_sparam(phba, pmb, 0);
	pmb->vport = vport;
	if (lpfc_sli_issue_mbox(phba, pmb, MBX_POLL) != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0448 Adapter failed init, mbxCmd x%x "
				"READ_SPARM mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxStatus);
		phba->link_state = LPFC_HBA_ERROR;
		mp = (struct lpfc_dmabuf *) pmb->context1;
		mempool_free( pmb, phba->mbox_mem_pool);
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
		return -EIO;
	}

	mp = (struct lpfc_dmabuf *) pmb->context1;

	memcpy(&vport->fc_sparam, mp->virt, sizeof (struct serv_parm));
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	pmb->context1 = NULL;

	if (phba->cfg_soft_wwnn)
		u64_to_wwn(phba->cfg_soft_wwnn,
			   vport->fc_sparam.nodeName.u.wwn);
	if (phba->cfg_soft_wwpn)
		u64_to_wwn(phba->cfg_soft_wwpn,
			   vport->fc_sparam.portName.u.wwn);
	memcpy(&vport->fc_nodename, &vport->fc_sparam.nodeName,
	       sizeof (struct lpfc_name));
	memcpy(&vport->fc_portname, &vport->fc_sparam.portName,
	       sizeof (struct lpfc_name));

	/* Update the fc_host data structures with new wwn. */
	fc_host_node_name(shost) = wwn_to_u64(vport->fc_nodename.u.wwn);
	fc_host_port_name(shost) = wwn_to_u64(vport->fc_portname.u.wwn);
	fc_host_max_npiv_vports(shost) = phba->max_vpi;

	/* If no serial number in VPD data, use low 6 bytes of WWNN */
	/* This should be consolidated into parse_vpd ? - mr */
	if (phba->SerialNumber[0] == 0) {
		uint8_t *outptr;

		outptr = &vport->fc_nodename.u.s.IEEE[0];
		for (i = 0; i < 12; i++) {
			status = *outptr++;
			j = ((status & 0xf0) >> 4);
			if (j <= 9)
				phba->SerialNumber[i] =
				    (char)((uint8_t) 0x30 + (uint8_t) j);
			else
				phba->SerialNumber[i] =
				    (char)((uint8_t) 0x61 + (uint8_t) (j - 10));
			i++;
			j = (status & 0xf);
			if (j <= 9)
				phba->SerialNumber[i] =
				    (char)((uint8_t) 0x30 + (uint8_t) j);
			else
				phba->SerialNumber[i] =
				    (char)((uint8_t) 0x61 + (uint8_t) (j - 10));
		}
	}

	lpfc_read_config(phba, pmb);
	pmb->vport = vport;
	if (lpfc_sli_issue_mbox(phba, pmb, MBX_POLL) != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0453 Adapter failed to init, mbxCmd x%x "
				"READ_CONFIG, mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxStatus);
		phba->link_state = LPFC_HBA_ERROR;
		mempool_free( pmb, phba->mbox_mem_pool);
		return -EIO;
	}

	/* Check if the port is disabled */
	lpfc_sli_read_link_ste(phba);

	/* Reset the DFT_HBA_Q_DEPTH to the max xri  */
	if (phba->cfg_hba_queue_depth > (mb->un.varRdConfig.max_xri+1))
		phba->cfg_hba_queue_depth =
			(mb->un.varRdConfig.max_xri + 1) -
					lpfc_sli4_get_els_iocb_cnt(phba);

	phba->lmt = mb->un.varRdConfig.lmt;

	/* Get the default values for Model Name and Description */
	lpfc_get_hba_model_desc(phba, phba->ModelName, phba->ModelDesc);

	if ((phba->cfg_link_speed > LINK_SPEED_10G)
	    || ((phba->cfg_link_speed == LINK_SPEED_1G)
		&& !(phba->lmt & LMT_1Gb))
	    || ((phba->cfg_link_speed == LINK_SPEED_2G)
		&& !(phba->lmt & LMT_2Gb))
	    || ((phba->cfg_link_speed == LINK_SPEED_4G)
		&& !(phba->lmt & LMT_4Gb))
	    || ((phba->cfg_link_speed == LINK_SPEED_8G)
		&& !(phba->lmt & LMT_8Gb))
	    || ((phba->cfg_link_speed == LINK_SPEED_10G)
		&& !(phba->lmt & LMT_10Gb))) {
		/* Reset link speed to auto */
		lpfc_printf_log(phba, KERN_WARNING, LOG_LINK_EVENT,
			"1302 Invalid speed for this board: "
			"Reset link speed to auto: x%x\n",
			phba->cfg_link_speed);
			phba->cfg_link_speed = LINK_SPEED_AUTO;
	}

	phba->link_state = LPFC_LINK_DOWN;

	/* Only process IOCBs on ELS ring till hba_state is READY */
	if (psli->ring[psli->extra_ring].cmdringaddr)
		psli->ring[psli->extra_ring].flag |= LPFC_STOP_IOCB_EVENT;
	if (psli->ring[psli->fcp_ring].cmdringaddr)
		psli->ring[psli->fcp_ring].flag |= LPFC_STOP_IOCB_EVENT;
	if (psli->ring[psli->next_ring].cmdringaddr)
		psli->ring[psli->next_ring].flag |= LPFC_STOP_IOCB_EVENT;

	/* Post receive buffers for desired rings */
	if (phba->sli_rev != 3)
		lpfc_post_rcv_buf(phba);

	/*
	 * Configure HBA MSI-X attention conditions to messages if MSI-X mode
	 */
	if (phba->intr_type == MSIX) {
		rc = lpfc_config_msi(phba, pmb);
		if (rc) {
			mempool_free(pmb, phba->mbox_mem_pool);
			return -EIO;
		}
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX,
					"0352 Config MSI mailbox command "
					"failed, mbxCmd x%x, mbxStatus x%x\n",
					pmb->u.mb.mbxCommand,
					pmb->u.mb.mbxStatus);
			mempool_free(pmb, phba->mbox_mem_pool);
			return -EIO;
		}
	}

	spin_lock_irq(&phba->hbalock);
	/* Initialize ERATT handling flag */
	phba->hba_flag &= ~HBA_ERATT_HANDLED;

	/* Enable appropriate host interrupts */
	status = readl(phba->HCregaddr);
	status |= HC_MBINT_ENA | HC_ERINT_ENA | HC_LAINT_ENA;
	if (psli->num_rings > 0)
		status |= HC_R0INT_ENA;
	if (psli->num_rings > 1)
		status |= HC_R1INT_ENA;
	if (psli->num_rings > 2)
		status |= HC_R2INT_ENA;
	if (psli->num_rings > 3)
		status |= HC_R3INT_ENA;

	if ((phba->cfg_poll & ENABLE_FCP_RING_POLLING) &&
	    (phba->cfg_poll & DISABLE_FCP_RING_INT))
		status &= ~(HC_R0INT_ENA);

	writel(status, phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */
	spin_unlock_irq(&phba->hbalock);

	/* Set up ring-0 (ELS) timer */
	timeout = phba->fc_ratov * 2;
	mod_timer(&vport->els_tmofunc, jiffies + HZ * timeout);
	/* Set up heart beat (HB) timer */
	mod_timer(&phba->hb_tmofunc, jiffies + HZ * LPFC_HB_MBOX_INTERVAL);
	phba->hb_outstanding = 0;
	phba->last_completion_time = jiffies;
	/* Set up error attention (ERATT) polling timer */
	mod_timer(&phba->eratt_poll, jiffies + HZ * LPFC_ERATT_POLL_INTERVAL);

	if (phba->hba_flag & LINK_DISABLED) {
		lpfc_printf_log(phba,
			KERN_ERR, LOG_INIT,
			"2598 Adapter Link is disabled.\n");
		lpfc_down_link(phba, pmb);
		pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
		if ((rc != MBX_SUCCESS) && (rc != MBX_BUSY)) {
			lpfc_printf_log(phba,
			KERN_ERR, LOG_INIT,
			"2599 Adapter failed to issue DOWN_LINK"
			" mbox command rc 0x%x\n", rc);

			mempool_free(pmb, phba->mbox_mem_pool);
			return -EIO;
		}
	} else {
		lpfc_init_link(phba, pmb, phba->cfg_topology,
			phba->cfg_link_speed);
		pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		lpfc_set_loopback_flag(phba);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0454 Adapter failed to init, mbxCmd x%x "
				"INIT_LINK, mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxStatus);

			/* Clear all interrupt enable conditions */
			writel(0, phba->HCregaddr);
			readl(phba->HCregaddr); /* flush */
			/* Clear all pending interrupts */
			writel(0xffffffff, phba->HAregaddr);
			readl(phba->HAregaddr); /* flush */

			phba->link_state = LPFC_HBA_ERROR;
			if (rc != MBX_BUSY)
				mempool_free(pmb, phba->mbox_mem_pool);
			return -EIO;
		}
	}
	/* MBOX buffer will be freed in mbox compl */
	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	lpfc_config_async(phba, pmb, LPFC_ELS_RING);
	pmb->mbox_cmpl = lpfc_config_async_cmpl;
	pmb->vport = phba->pport;
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);

	if ((rc != MBX_BUSY) && (rc != MBX_SUCCESS)) {
		lpfc_printf_log(phba,
				KERN_ERR,
				LOG_INIT,
				"0456 Adapter failed to issue "
				"ASYNCEVT_ENABLE mbox status x%x\n",
				rc);
		mempool_free(pmb, phba->mbox_mem_pool);
	}

	/* Get Option rom version */
	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	lpfc_dump_wakeup_param(phba, pmb);
	pmb->mbox_cmpl = lpfc_dump_wakeup_param_cmpl;
	pmb->vport = phba->pport;
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);

	if ((rc != MBX_BUSY) && (rc != MBX_SUCCESS)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT, "0435 Adapter failed "
				"to get Option ROM version status x%x\n", rc);
		mempool_free(pmb, phba->mbox_mem_pool);
	}

	return 0;
}

/**
 * lpfc_hba_down_prep - Perform lpfc uninitialization prior to HBA reset
 * @phba: pointer to lpfc HBA data structure.
 *
 * This routine will do LPFC uninitialization before the HBA is reset when
 * bringing down the SLI Layer.
 *
 * Return codes
 *   0 - success.
 *   Any other value - error.
 **/
int
lpfc_hba_down_prep(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	int i;

	if (phba->sli_rev <= LPFC_SLI_REV3) {
		/* Disable interrupts */
		writel(0, phba->HCregaddr);
		readl(phba->HCregaddr); /* flush */
	}

	if (phba->pport->load_flag & FC_UNLOADING)
		lpfc_cleanup_discovery_resources(phba->pport);
	else {
		vports = lpfc_create_vport_work_array(phba);
		if (vports != NULL)
			for (i = 0; i <= phba->max_vports &&
				vports[i] != NULL; i++)
				lpfc_cleanup_discovery_resources(vports[i]);
		lpfc_destroy_vport_work_array(phba, vports);
	}
	return 0;
}

/**
 * lpfc_hba_down_post_s3 - Perform lpfc uninitialization after HBA reset
 * @phba: pointer to lpfc HBA data structure.
 *
 * This routine will do uninitialization after the HBA is reset when bring
 * down the SLI Layer.
 *
 * Return codes
 *   0 - sucess.
 *   Any other value - error.
 **/
static int
lpfc_hba_down_post_s3(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;
	struct lpfc_dmabuf *mp, *next_mp;
	LIST_HEAD(completions);
	int i;

	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED)
		lpfc_sli_hbqbuf_free_all(phba);
	else {
		/* Cleanup preposted buffers on the ELS ring */
		pring = &psli->ring[LPFC_ELS_RING];
		list_for_each_entry_safe(mp, next_mp, &pring->postbufq, list) {
			list_del(&mp->list);
			pring->postbufq_cnt--;
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
		}
	}

	spin_lock_irq(&phba->hbalock);
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];

		/* At this point in time the HBA is either reset or DOA. Either
		 * way, nothing should be on txcmplq as it will NEVER complete.
		 */
		list_splice_init(&pring->txcmplq, &completions);
		pring->txcmplq_cnt = 0;
		spin_unlock_irq(&phba->hbalock);

		/* Cancel all the IOCBs from the completions list */
		lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
				      IOERR_SLI_ABORTED);

		lpfc_sli_abort_iocb_ring(phba, pring);
		spin_lock_irq(&phba->hbalock);
	}
	spin_unlock_irq(&phba->hbalock);

	return 0;
}
/**
 * lpfc_hba_down_post_s4 - Perform lpfc uninitialization after HBA reset
 * @phba: pointer to lpfc HBA data structure.
 *
 * This routine will do uninitialization after the HBA is reset when bring
 * down the SLI Layer.
 *
 * Return codes
 *   0 - sucess.
 *   Any other value - error.
 **/
static int
lpfc_hba_down_post_s4(struct lpfc_hba *phba)
{
	struct lpfc_scsi_buf *psb, *psb_next;
	LIST_HEAD(aborts);
	int ret;
	unsigned long iflag = 0;
	ret = lpfc_hba_down_post_s3(phba);
	if (ret)
		return ret;
	/* At this point in time the HBA is either reset or DOA. Either
	 * way, nothing should be on lpfc_abts_els_sgl_list, it needs to be
	 * on the lpfc_sgl_list so that it can either be freed if the
	 * driver is unloading or reposted if the driver is restarting
	 * the port.
	 */
	spin_lock_irq(&phba->hbalock);  /* required for lpfc_sgl_list and */
					/* scsl_buf_list */
	/* abts_sgl_list_lock required because worker thread uses this
	 * list.
	 */
	spin_lock(&phba->sli4_hba.abts_sgl_list_lock);
	list_splice_init(&phba->sli4_hba.lpfc_abts_els_sgl_list,
			&phba->sli4_hba.lpfc_sgl_list);
	spin_unlock(&phba->sli4_hba.abts_sgl_list_lock);
	/* abts_scsi_buf_list_lock required because worker thread uses this
	 * list.
	 */
	spin_lock(&phba->sli4_hba.abts_scsi_buf_list_lock);
	list_splice_init(&phba->sli4_hba.lpfc_abts_scsi_buf_list,
			&aborts);
	spin_unlock(&phba->sli4_hba.abts_scsi_buf_list_lock);
	spin_unlock_irq(&phba->hbalock);

	list_for_each_entry_safe(psb, psb_next, &aborts, list) {
		psb->pCmd = NULL;
		psb->status = IOSTAT_SUCCESS;
	}
	spin_lock_irqsave(&phba->scsi_buf_list_lock, iflag);
	list_splice(&aborts, &phba->lpfc_scsi_buf_list);
	spin_unlock_irqrestore(&phba->scsi_buf_list_lock, iflag);
	return 0;
}

/**
 * lpfc_hba_down_post - Wrapper func for hba down post routine
 * @phba: pointer to lpfc HBA data structure.
 *
 * This routine wraps the actual SLI3 or SLI4 routine for performing
 * uninitialization after the HBA is reset when bring down the SLI Layer.
 *
 * Return codes
 *   0 - sucess.
 *   Any other value - error.
 **/
int
lpfc_hba_down_post(struct lpfc_hba *phba)
{
	return (*phba->lpfc_hba_down_post)(phba);
}

/**
 * lpfc_hb_timeout - The HBA-timer timeout handler
 * @ptr: unsigned long holds the pointer to lpfc hba data structure.
 *
 * This is the HBA-timer timeout handler registered to the lpfc driver. When
 * this timer fires, a HBA timeout event shall be posted to the lpfc driver
 * work-port-events bitmap and the worker thread is notified. This timeout
 * event will be used by the worker thread to invoke the actual timeout
 * handler routine, lpfc_hb_timeout_handler. Any periodical operations will
 * be performed in the timeout handler and the HBA timeout event bit shall
 * be cleared by the worker thread after it has taken the event bitmap out.
 **/
static void
lpfc_hb_timeout(unsigned long ptr)
{
	struct lpfc_hba *phba;
	uint32_t tmo_posted;
	unsigned long iflag;

	phba = (struct lpfc_hba *)ptr;

	/* Check for heart beat timeout conditions */
	spin_lock_irqsave(&phba->pport->work_port_lock, iflag);
	tmo_posted = phba->pport->work_port_events & WORKER_HB_TMO;
	if (!tmo_posted)
		phba->pport->work_port_events |= WORKER_HB_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflag);

	/* Tell the worker thread there is work to do */
	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	return;
}

/**
 * lpfc_hb_mbox_cmpl - The lpfc heart-beat mailbox command callback function
 * @phba: pointer to lpfc hba data structure.
 * @pmboxq: pointer to the driver internal queue element for mailbox command.
 *
 * This is the callback function to the lpfc heart-beat mailbox command.
 * If configured, the lpfc driver issues the heart-beat mailbox command to
 * the HBA every LPFC_HB_MBOX_INTERVAL (current 5) seconds. At the time the
 * heart-beat mailbox command is issued, the driver shall set up heart-beat
 * timeout timer to LPFC_HB_MBOX_TIMEOUT (current 30) seconds and marks
 * heart-beat outstanding state. Once the mailbox command comes back and
 * no error conditions detected, the heart-beat mailbox command timer is
 * reset to LPFC_HB_MBOX_INTERVAL seconds and the heart-beat outstanding
 * state is cleared for the next heart-beat. If the timer expired with the
 * heart-beat outstanding state set, the driver will put the HBA offline.
 **/
static void
lpfc_hb_mbox_cmpl(struct lpfc_hba * phba, LPFC_MBOXQ_t * pmboxq)
{
	unsigned long drvr_flag;

	spin_lock_irqsave(&phba->hbalock, drvr_flag);
	phba->hb_outstanding = 0;
	spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

	/* Check and reset heart-beat timer is necessary */
	mempool_free(pmboxq, phba->mbox_mem_pool);
	if (!(phba->pport->fc_flag & FC_OFFLINE_MODE) &&
		!(phba->link_state == LPFC_HBA_ERROR) &&
		!(phba->pport->load_flag & FC_UNLOADING))
		mod_timer(&phba->hb_tmofunc,
			jiffies + HZ * LPFC_HB_MBOX_INTERVAL);
	return;
}

/**
 * lpfc_hb_timeout_handler - The HBA-timer timeout handler
 * @phba: pointer to lpfc hba data structure.
 *
 * This is the actual HBA-timer timeout handler to be invoked by the worker
 * thread whenever the HBA timer fired and HBA-timeout event posted. This
 * handler performs any periodic operations needed for the device. If such
 * periodic event has already been attended to either in the interrupt handler
 * or by processing slow-ring or fast-ring events within the HBA-timer
 * timeout window (LPFC_HB_MBOX_INTERVAL), this handler just simply resets
 * the timer for the next timeout period. If lpfc heart-beat mailbox command
 * is configured and there is no heart-beat mailbox command outstanding, a
 * heart-beat mailbox is issued and timer set properly. Otherwise, if there
 * has been a heart-beat mailbox command outstanding, the HBA shall be put
 * to offline.
 **/
void
lpfc_hb_timeout_handler(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *pmboxq;
	struct lpfc_dmabuf *buf_ptr;
	int retval;
	struct lpfc_sli *psli = &phba->sli;
	LIST_HEAD(completions);

	if ((phba->link_state == LPFC_HBA_ERROR) ||
		(phba->pport->load_flag & FC_UNLOADING) ||
		(phba->pport->fc_flag & FC_OFFLINE_MODE))
		return;

	spin_lock_irq(&phba->pport->work_port_lock);

	if (time_after(phba->last_completion_time + LPFC_HB_MBOX_INTERVAL * HZ,
		jiffies)) {
		spin_unlock_irq(&phba->pport->work_port_lock);
		if (!phba->hb_outstanding)
			mod_timer(&phba->hb_tmofunc,
				jiffies + HZ * LPFC_HB_MBOX_INTERVAL);
		else
			mod_timer(&phba->hb_tmofunc,
				jiffies + HZ * LPFC_HB_MBOX_TIMEOUT);
		return;
	}
	spin_unlock_irq(&phba->pport->work_port_lock);

	if (phba->elsbuf_cnt &&
		(phba->elsbuf_cnt == phba->elsbuf_prev_cnt)) {
		spin_lock_irq(&phba->hbalock);
		list_splice_init(&phba->elsbuf, &completions);
		phba->elsbuf_cnt = 0;
		phba->elsbuf_prev_cnt = 0;
		spin_unlock_irq(&phba->hbalock);

		while (!list_empty(&completions)) {
			list_remove_head(&completions, buf_ptr,
				struct lpfc_dmabuf, list);
			lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
			kfree(buf_ptr);
		}
	}
	phba->elsbuf_prev_cnt = phba->elsbuf_cnt;

	/* If there is no heart beat outstanding, issue a heartbeat command */
	if (phba->cfg_enable_hba_heartbeat) {
		if (!phba->hb_outstanding) {
			pmboxq = mempool_alloc(phba->mbox_mem_pool,GFP_KERNEL);
			if (!pmboxq) {
				mod_timer(&phba->hb_tmofunc,
					  jiffies + HZ * LPFC_HB_MBOX_INTERVAL);
				return;
			}

			lpfc_heart_beat(phba, pmboxq);
			pmboxq->mbox_cmpl = lpfc_hb_mbox_cmpl;
			pmboxq->vport = phba->pport;
			retval = lpfc_sli_issue_mbox(phba, pmboxq, MBX_NOWAIT);

			if (retval != MBX_BUSY && retval != MBX_SUCCESS) {
				mempool_free(pmboxq, phba->mbox_mem_pool);
				mod_timer(&phba->hb_tmofunc,
					  jiffies + HZ * LPFC_HB_MBOX_INTERVAL);
				return;
			}
			mod_timer(&phba->hb_tmofunc,
				  jiffies + HZ * LPFC_HB_MBOX_TIMEOUT);
			phba->hb_outstanding = 1;
			return;
		} else {
			/*
			* If heart beat timeout called with hb_outstanding set
			* we need to take the HBA offline.
			*/
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0459 Adapter heartbeat failure, "
					"taking this port offline.\n");

			spin_lock_irq(&phba->hbalock);
			psli->sli_flag &= ~LPFC_SLI_ACTIVE;
			spin_unlock_irq(&phba->hbalock);

			lpfc_offline_prep(phba);
			lpfc_offline(phba);
			lpfc_unblock_mgmt_io(phba);
			phba->link_state = LPFC_HBA_ERROR;
			lpfc_hba_down_post(phba);
		}
	}
}

/**
 * lpfc_offline_eratt - Bring lpfc offline on hardware error attention
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is called to bring the HBA offline when HBA hardware error
 * other than Port Error 6 has been detected.
 **/
static void
lpfc_offline_eratt(struct lpfc_hba *phba)
{
	struct lpfc_sli   *psli = &phba->sli;

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);
	lpfc_offline_prep(phba);

	lpfc_offline(phba);
	lpfc_reset_barrier(phba);
	spin_lock_irq(&phba->hbalock);
	lpfc_sli_brdreset(phba);
	spin_unlock_irq(&phba->hbalock);
	lpfc_hba_down_post(phba);
	lpfc_sli_brdready(phba, HS_MBRDY);
	lpfc_unblock_mgmt_io(phba);
	phba->link_state = LPFC_HBA_ERROR;
	return;
}

/**
 * lpfc_sli4_offline_eratt - Bring lpfc offline on SLI4 hardware error attention
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is called to bring a SLI4 HBA offline when HBA hardware error
 * other than Port Error 6 has been detected.
 **/
static void
lpfc_sli4_offline_eratt(struct lpfc_hba *phba)
{
	lpfc_offline_prep(phba);
	lpfc_offline(phba);
	lpfc_sli4_brdreset(phba);
	lpfc_hba_down_post(phba);
	lpfc_sli4_post_status_check(phba);
	lpfc_unblock_mgmt_io(phba);
	phba->link_state = LPFC_HBA_ERROR;
}

/**
 * lpfc_handle_deferred_eratt - The HBA hardware deferred error handler
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to handle the deferred HBA hardware error
 * conditions. This type of error is indicated by HBA by setting ER1
 * and another ER bit in the host status register. The driver will
 * wait until the ER1 bit clears before handling the error condition.
 **/
static void
lpfc_handle_deferred_eratt(struct lpfc_hba *phba)
{
	uint32_t old_host_status = phba->work_hs;
	struct lpfc_sli_ring  *pring;
	struct lpfc_sli *psli = &phba->sli;

	/* If the pci channel is offline, ignore possible errors,
	 * since we cannot communicate with the pci card anyway.
	 */
	if (pci_channel_offline(phba->pcidev)) {
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~DEFER_ERATT;
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
		"0479 Deferred Adapter Hardware Error "
		"Data: x%x x%x x%x\n",
		phba->work_hs,
		phba->work_status[0], phba->work_status[1]);

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);


	/*
	 * Firmware stops when it triggred erratt. That could cause the I/Os
	 * dropped by the firmware. Error iocb (I/O) on txcmplq and let the
	 * SCSI layer retry it after re-establishing link.
	 */
	pring = &psli->ring[psli->fcp_ring];
	lpfc_sli_abort_iocb_ring(phba, pring);

	/*
	 * There was a firmware error. Take the hba offline and then
	 * attempt to restart it.
	 */
	lpfc_offline_prep(phba);
	lpfc_offline(phba);

	/* Wait for the ER1 bit to clear.*/
	while (phba->work_hs & HS_FFER1) {
		msleep(100);
		phba->work_hs = readl(phba->HSregaddr);
		/* If driver is unloading let the worker thread continue */
		if (phba->pport->load_flag & FC_UNLOADING) {
			phba->work_hs = 0;
			break;
		}
	}

	/*
	 * This is to ptrotect against a race condition in which
	 * first write to the host attention register clear the
	 * host status register.
	 */
	if ((!phba->work_hs) && (!(phba->pport->load_flag & FC_UNLOADING)))
		phba->work_hs = old_host_status & ~HS_FFER1;

	spin_lock_irq(&phba->hbalock);
	phba->hba_flag &= ~DEFER_ERATT;
	spin_unlock_irq(&phba->hbalock);
	phba->work_status[0] = readl(phba->MBslimaddr + 0xa8);
	phba->work_status[1] = readl(phba->MBslimaddr + 0xac);
}

static void
lpfc_board_errevt_to_mgmt(struct lpfc_hba *phba)
{
	struct lpfc_board_event_header board_event;
	struct Scsi_Host *shost;

	board_event.event_type = FC_REG_BOARD_EVENT;
	board_event.subcategory = LPFC_EVENT_PORTINTERR;
	shost = lpfc_shost_from_vport(phba->pport);
	fc_host_post_vendor_event(shost, fc_get_event_number(),
				  sizeof(board_event),
				  (char *) &board_event,
				  LPFC_NL_VENDOR_ID);
}

/**
 * lpfc_handle_eratt_s3 - The SLI3 HBA hardware error handler
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to handle the following HBA hardware error
 * conditions:
 * 1 - HBA error attention interrupt
 * 2 - DMA ring index out of range
 * 3 - Mailbox command came back as unknown
 **/
static void
lpfc_handle_eratt_s3(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_sli   *psli = &phba->sli;
	struct lpfc_sli_ring  *pring;
	uint32_t event_data;
	unsigned long temperature;
	struct temp_event temp_event_data;
	struct Scsi_Host  *shost;

	/* If the pci channel is offline, ignore possible errors,
	 * since we cannot communicate with the pci card anyway.
	 */
	if (pci_channel_offline(phba->pcidev)) {
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~DEFER_ERATT;
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	/* If resets are disabled then leave the HBA alone and return */
	if (!phba->cfg_enable_hba_reset)
		return;

	/* Send an internal error event to mgmt application */
	lpfc_board_errevt_to_mgmt(phba);

	if (phba->hba_flag & DEFER_ERATT)
		lpfc_handle_deferred_eratt(phba);

	if (phba->work_hs & HS_FFER6) {
		/* Re-establishing Link */
		lpfc_printf_log(phba, KERN_INFO, LOG_LINK_EVENT,
				"1301 Re-establishing Link "
				"Data: x%x x%x x%x\n",
				phba->work_hs,
				phba->work_status[0], phba->work_status[1]);

		spin_lock_irq(&phba->hbalock);
		psli->sli_flag &= ~LPFC_SLI_ACTIVE;
		spin_unlock_irq(&phba->hbalock);

		/*
		* Firmware stops when it triggled erratt with HS_FFER6.
		* That could cause the I/Os dropped by the firmware.
		* Error iocb (I/O) on txcmplq and let the SCSI layer
		* retry it after re-establishing link.
		*/
		pring = &psli->ring[psli->fcp_ring];
		lpfc_sli_abort_iocb_ring(phba, pring);

		/*
		 * There was a firmware error.  Take the hba offline and then
		 * attempt to restart it.
		 */
		lpfc_offline_prep(phba);
		lpfc_offline(phba);
		lpfc_sli_brdrestart(phba);
		if (lpfc_online(phba) == 0) {	/* Initialize the HBA */
			lpfc_unblock_mgmt_io(phba);
			return;
		}
		lpfc_unblock_mgmt_io(phba);
	} else if (phba->work_hs & HS_CRIT_TEMP) {
		temperature = readl(phba->MBslimaddr + TEMPERATURE_OFFSET);
		temp_event_data.event_type = FC_REG_TEMPERATURE_EVENT;
		temp_event_data.event_code = LPFC_CRIT_TEMP;
		temp_event_data.data = (uint32_t)temperature;

		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0406 Adapter maximum temperature exceeded "
				"(%ld), taking this port offline "
				"Data: x%x x%x x%x\n",
				temperature, phba->work_hs,
				phba->work_status[0], phba->work_status[1]);

		shost = lpfc_shost_from_vport(phba->pport);
		fc_host_post_vendor_event(shost, fc_get_event_number(),
					  sizeof(temp_event_data),
					  (char *) &temp_event_data,
					  SCSI_NL_VID_TYPE_PCI
					  | PCI_VENDOR_ID_EMULEX);

		spin_lock_irq(&phba->hbalock);
		phba->over_temp_state = HBA_OVER_TEMP;
		spin_unlock_irq(&phba->hbalock);
		lpfc_offline_eratt(phba);

	} else {
		/* The if clause above forces this code path when the status
		 * failure is a value other than FFER6. Do not call the offline
		 * twice. This is the adapter hardware error path.
		 */
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0457 Adapter Hardware Error "
				"Data: x%x x%x x%x\n",
				phba->work_hs,
				phba->work_status[0], phba->work_status[1]);

		event_data = FC_REG_DUMP_EVENT;
		shost = lpfc_shost_from_vport(vport);
		fc_host_post_vendor_event(shost, fc_get_event_number(),
				sizeof(event_data), (char *) &event_data,
				SCSI_NL_VID_TYPE_PCI | PCI_VENDOR_ID_EMULEX);

		lpfc_offline_eratt(phba);
	}
	return;
}

/**
 * lpfc_handle_eratt_s4 - The SLI4 HBA hardware error handler
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to handle the SLI4 HBA hardware error attention
 * conditions.
 **/
static void
lpfc_handle_eratt_s4(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	uint32_t event_data;
	struct Scsi_Host *shost;

	/* If the pci channel is offline, ignore possible errors, since
	 * we cannot communicate with the pci card anyway.
	 */
	if (pci_channel_offline(phba->pcidev))
		return;
	/* If resets are disabled then leave the HBA alone and return */
	if (!phba->cfg_enable_hba_reset)
		return;

	/* Send an internal error event to mgmt application */
	lpfc_board_errevt_to_mgmt(phba);

	/* For now, the actual action for SLI4 device handling is not
	 * specified yet, just treated it as adaptor hardware failure
	 */
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"0143 SLI4 Adapter Hardware Error Data: x%x x%x\n",
			phba->work_status[0], phba->work_status[1]);

	event_data = FC_REG_DUMP_EVENT;
	shost = lpfc_shost_from_vport(vport);
	fc_host_post_vendor_event(shost, fc_get_event_number(),
				  sizeof(event_data), (char *) &event_data,
				  SCSI_NL_VID_TYPE_PCI | PCI_VENDOR_ID_EMULEX);

	lpfc_sli4_offline_eratt(phba);
}

/**
 * lpfc_handle_eratt - Wrapper func for handling hba error attention
 * @phba: pointer to lpfc HBA data structure.
 *
 * This routine wraps the actual SLI3 or SLI4 hba error attention handling
 * routine from the API jump table function pointer from the lpfc_hba struct.
 *
 * Return codes
 *   0 - sucess.
 *   Any other value - error.
 **/
void
lpfc_handle_eratt(struct lpfc_hba *phba)
{
	(*phba->lpfc_handle_eratt)(phba);
}

/**
 * lpfc_handle_latt - The HBA link event handler
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked from the worker thread to handle a HBA host
 * attention link event.
 **/
void
lpfc_handle_latt(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_sli   *psli = &phba->sli;
	LPFC_MBOXQ_t *pmb;
	volatile uint32_t control;
	struct lpfc_dmabuf *mp;
	int rc = 0;

	pmb = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		rc = 1;
		goto lpfc_handle_latt_err_exit;
	}

	mp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!mp) {
		rc = 2;
		goto lpfc_handle_latt_free_pmb;
	}

	mp->virt = lpfc_mbuf_alloc(phba, 0, &mp->phys);
	if (!mp->virt) {
		rc = 3;
		goto lpfc_handle_latt_free_mp;
	}

	/* Cleanup any outstanding ELS commands */
	lpfc_els_flush_all_cmd(phba);

	psli->slistat.link_event++;
	lpfc_read_la(phba, pmb, mp);
	pmb->mbox_cmpl = lpfc_mbx_cmpl_read_la;
	pmb->vport = vport;
	/* Block ELS IOCBs until we have processed this mbox command */
	phba->sli.ring[LPFC_ELS_RING].flag |= LPFC_STOP_IOCB_EVENT;
	rc = lpfc_sli_issue_mbox (phba, pmb, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		rc = 4;
		goto lpfc_handle_latt_free_mbuf;
	}

	/* Clear Link Attention in HA REG */
	spin_lock_irq(&phba->hbalock);
	writel(HA_LATT, phba->HAregaddr);
	readl(phba->HAregaddr); /* flush */
	spin_unlock_irq(&phba->hbalock);

	return;

lpfc_handle_latt_free_mbuf:
	phba->sli.ring[LPFC_ELS_RING].flag &= ~LPFC_STOP_IOCB_EVENT;
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
lpfc_handle_latt_free_mp:
	kfree(mp);
lpfc_handle_latt_free_pmb:
	mempool_free(pmb, phba->mbox_mem_pool);
lpfc_handle_latt_err_exit:
	/* Enable Link attention interrupts */
	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_PROCESS_LA;
	control = readl(phba->HCregaddr);
	control |= HC_LAINT_ENA;
	writel(control, phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */

	/* Clear Link Attention in HA REG */
	writel(HA_LATT, phba->HAregaddr);
	readl(phba->HAregaddr); /* flush */
	spin_unlock_irq(&phba->hbalock);
	lpfc_linkdown(phba);
	phba->link_state = LPFC_HBA_ERROR;

	lpfc_printf_log(phba, KERN_ERR, LOG_MBOX,
		     "0300 LATT: Cannot issue READ_LA: Data:%d\n", rc);

	return;
}

/**
 * lpfc_parse_vpd - Parse VPD (Vital Product Data)
 * @phba: pointer to lpfc hba data structure.
 * @vpd: pointer to the vital product data.
 * @len: length of the vital product data in bytes.
 *
 * This routine parses the Vital Product Data (VPD). The VPD is treated as
 * an array of characters. In this routine, the ModelName, ProgramType, and
 * ModelDesc, etc. fields of the phba data structure will be populated.
 *
 * Return codes
 *   0 - pointer to the VPD passed in is NULL
 *   1 - success
 **/
int
lpfc_parse_vpd(struct lpfc_hba *phba, uint8_t *vpd, int len)
{
	uint8_t lenlo, lenhi;
	int Length;
	int i, j;
	int finished = 0;
	int index = 0;

	if (!vpd)
		return 0;

	/* Vital Product */
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0455 Vital Product Data: x%x x%x x%x x%x\n",
			(uint32_t) vpd[0], (uint32_t) vpd[1], (uint32_t) vpd[2],
			(uint32_t) vpd[3]);
	while (!finished && (index < (len - 4))) {
		switch (vpd[index]) {
		case 0x82:
		case 0x91:
			index += 1;
			lenlo = vpd[index];
			index += 1;
			lenhi = vpd[index];
			index += 1;
			i = ((((unsigned short)lenhi) << 8) + lenlo);
			index += i;
			break;
		case 0x90:
			index += 1;
			lenlo = vpd[index];
			index += 1;
			lenhi = vpd[index];
			index += 1;
			Length = ((((unsigned short)lenhi) << 8) + lenlo);
			if (Length > len - index)
				Length = len - index;
			while (Length > 0) {
			/* Look for Serial Number */
			if ((vpd[index] == 'S') && (vpd[index+1] == 'N')) {
				index += 2;
				i = vpd[index];
				index += 1;
				j = 0;
				Length -= (3+i);
				while(i--) {
					phba->SerialNumber[j++] = vpd[index++];
					if (j == 31)
						break;
				}
				phba->SerialNumber[j] = 0;
				continue;
			}
			else if ((vpd[index] == 'V') && (vpd[index+1] == '1')) {
				phba->vpd_flag |= VPD_MODEL_DESC;
				index += 2;
				i = vpd[index];
				index += 1;
				j = 0;
				Length -= (3+i);
				while(i--) {
					phba->ModelDesc[j++] = vpd[index++];
					if (j == 255)
						break;
				}
				phba->ModelDesc[j] = 0;
				continue;
			}
			else if ((vpd[index] == 'V') && (vpd[index+1] == '2')) {
				phba->vpd_flag |= VPD_MODEL_NAME;
				index += 2;
				i = vpd[index];
				index += 1;
				j = 0;
				Length -= (3+i);
				while(i--) {
					phba->ModelName[j++] = vpd[index++];
					if (j == 79)
						break;
				}
				phba->ModelName[j] = 0;
				continue;
			}
			else if ((vpd[index] == 'V') && (vpd[index+1] == '3')) {
				phba->vpd_flag |= VPD_PROGRAM_TYPE;
				index += 2;
				i = vpd[index];
				index += 1;
				j = 0;
				Length -= (3+i);
				while(i--) {
					phba->ProgramType[j++] = vpd[index++];
					if (j == 255)
						break;
				}
				phba->ProgramType[j] = 0;
				continue;
			}
			else if ((vpd[index] == 'V') && (vpd[index+1] == '4')) {
				phba->vpd_flag |= VPD_PORT;
				index += 2;
				i = vpd[index];
				index += 1;
				j = 0;
				Length -= (3+i);
				while(i--) {
				phba->Port[j++] = vpd[index++];
				if (j == 19)
					break;
				}
				phba->Port[j] = 0;
				continue;
			}
			else {
				index += 2;
				i = vpd[index];
				index += 1;
				index += i;
				Length -= (3 + i);
			}
		}
		finished = 0;
		break;
		case 0x78:
			finished = 1;
			break;
		default:
			index ++;
			break;
		}
	}

	return(1);
}

/**
 * lpfc_get_hba_model_desc - Retrieve HBA device model name and description
 * @phba: pointer to lpfc hba data structure.
 * @mdp: pointer to the data structure to hold the derived model name.
 * @descp: pointer to the data structure to hold the derived description.
 *
 * This routine retrieves HBA's description based on its registered PCI device
 * ID. The @descp passed into this function points to an array of 256 chars. It
 * shall be returned with the model name, maximum speed, and the host bus type.
 * The @mdp passed into this function points to an array of 80 chars. When the
 * function returns, the @mdp will be filled with the model name.
 **/
static void
lpfc_get_hba_model_desc(struct lpfc_hba *phba, uint8_t *mdp, uint8_t *descp)
{
	lpfc_vpd_t *vp;
	uint16_t dev_id = phba->pcidev->device;
	int max_speed;
	int GE = 0;
	int oneConnect = 0; /* default is not a oneConnect */
	struct {
		char * name;
		int    max_speed;
		char * bus;
	} m = {"<Unknown>", 0, ""};

	if (mdp && mdp[0] != '\0'
		&& descp && descp[0] != '\0')
		return;

	if (phba->lmt & LMT_10Gb)
		max_speed = 10;
	else if (phba->lmt & LMT_8Gb)
		max_speed = 8;
	else if (phba->lmt & LMT_4Gb)
		max_speed = 4;
	else if (phba->lmt & LMT_2Gb)
		max_speed = 2;
	else
		max_speed = 1;

	vp = &phba->vpd;

	switch (dev_id) {
	case PCI_DEVICE_ID_FIREFLY:
		m = (typeof(m)){"LP6000", max_speed, "PCI"};
		break;
	case PCI_DEVICE_ID_SUPERFLY:
		if (vp->rev.biuRev >= 1 && vp->rev.biuRev <= 3)
			m = (typeof(m)){"LP7000", max_speed,  "PCI"};
		else
			m = (typeof(m)){"LP7000E", max_speed, "PCI"};
		break;
	case PCI_DEVICE_ID_DRAGONFLY:
		m = (typeof(m)){"LP8000", max_speed, "PCI"};
		break;
	case PCI_DEVICE_ID_CENTAUR:
		if (FC_JEDEC_ID(vp->rev.biuRev) == CENTAUR_2G_JEDEC_ID)
			m = (typeof(m)){"LP9002", max_speed, "PCI"};
		else
			m = (typeof(m)){"LP9000", max_speed, "PCI"};
		break;
	case PCI_DEVICE_ID_RFLY:
		m = (typeof(m)){"LP952", max_speed, "PCI"};
		break;
	case PCI_DEVICE_ID_PEGASUS:
		m = (typeof(m)){"LP9802", max_speed, "PCI-X"};
		break;
	case PCI_DEVICE_ID_THOR:
		m = (typeof(m)){"LP10000", max_speed, "PCI-X"};
		break;
	case PCI_DEVICE_ID_VIPER:
		m = (typeof(m)){"LPX1000", max_speed,  "PCI-X"};
		break;
	case PCI_DEVICE_ID_PFLY:
		m = (typeof(m)){"LP982", max_speed, "PCI-X"};
		break;
	case PCI_DEVICE_ID_TFLY:
		m = (typeof(m)){"LP1050", max_speed, "PCI-X"};
		break;
	case PCI_DEVICE_ID_HELIOS:
		m = (typeof(m)){"LP11000", max_speed, "PCI-X2"};
		break;
	case PCI_DEVICE_ID_HELIOS_SCSP:
		m = (typeof(m)){"LP11000-SP", max_speed, "PCI-X2"};
		break;
	case PCI_DEVICE_ID_HELIOS_DCSP:
		m = (typeof(m)){"LP11002-SP", max_speed, "PCI-X2"};
		break;
	case PCI_DEVICE_ID_NEPTUNE:
		m = (typeof(m)){"LPe1000", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_NEPTUNE_SCSP:
		m = (typeof(m)){"LPe1000-SP", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_NEPTUNE_DCSP:
		m = (typeof(m)){"LPe1002-SP", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_BMID:
		m = (typeof(m)){"LP1150", max_speed, "PCI-X2"};
		break;
	case PCI_DEVICE_ID_BSMB:
		m = (typeof(m)){"LP111", max_speed, "PCI-X2"};
		break;
	case PCI_DEVICE_ID_ZEPHYR:
		m = (typeof(m)){"LPe11000", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_ZEPHYR_SCSP:
		m = (typeof(m)){"LPe11000", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_ZEPHYR_DCSP:
		m = (typeof(m)){"LP2105", max_speed, "PCIe"};
		GE = 1;
		break;
	case PCI_DEVICE_ID_ZMID:
		m = (typeof(m)){"LPe1150", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_ZSMB:
		m = (typeof(m)){"LPe111", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_LP101:
		m = (typeof(m)){"LP101", max_speed, "PCI-X"};
		break;
	case PCI_DEVICE_ID_LP10000S:
		m = (typeof(m)){"LP10000-S", max_speed, "PCI"};
		break;
	case PCI_DEVICE_ID_LP11000S:
		m = (typeof(m)){"LP11000-S", max_speed,
			"PCI-X2"};
		break;
	case PCI_DEVICE_ID_LPE11000S:
		m = (typeof(m)){"LPe11000-S", max_speed,
			"PCIe"};
		break;
	case PCI_DEVICE_ID_SAT:
		m = (typeof(m)){"LPe12000", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_SAT_MID:
		m = (typeof(m)){"LPe1250", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_SAT_SMB:
		m = (typeof(m)){"LPe121", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_SAT_DCSP:
		m = (typeof(m)){"LPe12002-SP", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_SAT_SCSP:
		m = (typeof(m)){"LPe12000-SP", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_SAT_S:
		m = (typeof(m)){"LPe12000-S", max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_HORNET:
		m = (typeof(m)){"LP21000", max_speed, "PCIe"};
		GE = 1;
		break;
	case PCI_DEVICE_ID_PROTEUS_VF:
		m = (typeof(m)) {"LPev12000", max_speed, "PCIe IOV"};
		break;
	case PCI_DEVICE_ID_PROTEUS_PF:
		m = (typeof(m)) {"LPev12000", max_speed, "PCIe IOV"};
		break;
	case PCI_DEVICE_ID_PROTEUS_S:
		m = (typeof(m)) {"LPemv12002-S", max_speed, "PCIe IOV"};
		break;
	case PCI_DEVICE_ID_TIGERSHARK:
		oneConnect = 1;
		m = (typeof(m)) {"OCe10100-F", max_speed, "PCIe"};
		break;
	default:
		m = (typeof(m)){ NULL };
		break;
	}

	if (mdp && mdp[0] == '\0')
		snprintf(mdp, 79,"%s", m.name);
	/* oneConnect hba requires special processing, they are all initiators
	 * and we put the port number on the end
	 */
	if (descp && descp[0] == '\0') {
		if (oneConnect)
			snprintf(descp, 255,
				"Emulex OneConnect %s, FCoE Initiator, Port %s",
				m.name,
				phba->Port);
		else
			snprintf(descp, 255,
				"Emulex %s %d%s %s %s",
				m.name, m.max_speed,
				(GE) ? "GE" : "Gb",
				m.bus,
				(GE) ? "FCoE Adapter" :
					"Fibre Channel Adapter");
	}
}

/**
 * lpfc_post_buffer - Post IOCB(s) with DMA buffer descriptor(s) to a IOCB ring
 * @phba: pointer to lpfc hba data structure.
 * @pring: pointer to a IOCB ring.
 * @cnt: the number of IOCBs to be posted to the IOCB ring.
 *
 * This routine posts a given number of IOCBs with the associated DMA buffer
 * descriptors specified by the cnt argument to the given IOCB ring.
 *
 * Return codes
 *   The number of IOCBs NOT able to be posted to the IOCB ring.
 **/
int
lpfc_post_buffer(struct lpfc_hba *phba, struct lpfc_sli_ring *pring, int cnt)
{
	IOCB_t *icmd;
	struct lpfc_iocbq *iocb;
	struct lpfc_dmabuf *mp1, *mp2;

	cnt += pring->missbufcnt;

	/* While there are buffers to post */
	while (cnt > 0) {
		/* Allocate buffer for  command iocb */
		iocb = lpfc_sli_get_iocbq(phba);
		if (iocb == NULL) {
			pring->missbufcnt = cnt;
			return cnt;
		}
		icmd = &iocb->iocb;

		/* 2 buffers can be posted per command */
		/* Allocate buffer to post */
		mp1 = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
		if (mp1)
		    mp1->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &mp1->phys);
		if (!mp1 || !mp1->virt) {
			kfree(mp1);
			lpfc_sli_release_iocbq(phba, iocb);
			pring->missbufcnt = cnt;
			return cnt;
		}

		INIT_LIST_HEAD(&mp1->list);
		/* Allocate buffer to post */
		if (cnt > 1) {
			mp2 = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
			if (mp2)
				mp2->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
							    &mp2->phys);
			if (!mp2 || !mp2->virt) {
				kfree(mp2);
				lpfc_mbuf_free(phba, mp1->virt, mp1->phys);
				kfree(mp1);
				lpfc_sli_release_iocbq(phba, iocb);
				pring->missbufcnt = cnt;
				return cnt;
			}

			INIT_LIST_HEAD(&mp2->list);
		} else {
			mp2 = NULL;
		}

		icmd->un.cont64[0].addrHigh = putPaddrHigh(mp1->phys);
		icmd->un.cont64[0].addrLow = putPaddrLow(mp1->phys);
		icmd->un.cont64[0].tus.f.bdeSize = FCELSSIZE;
		icmd->ulpBdeCount = 1;
		cnt--;
		if (mp2) {
			icmd->un.cont64[1].addrHigh = putPaddrHigh(mp2->phys);
			icmd->un.cont64[1].addrLow = putPaddrLow(mp2->phys);
			icmd->un.cont64[1].tus.f.bdeSize = FCELSSIZE;
			cnt--;
			icmd->ulpBdeCount = 2;
		}

		icmd->ulpCommand = CMD_QUE_RING_BUF64_CN;
		icmd->ulpLe = 1;

		if (lpfc_sli_issue_iocb(phba, pring->ringno, iocb, 0) ==
		    IOCB_ERROR) {
			lpfc_mbuf_free(phba, mp1->virt, mp1->phys);
			kfree(mp1);
			cnt++;
			if (mp2) {
				lpfc_mbuf_free(phba, mp2->virt, mp2->phys);
				kfree(mp2);
				cnt++;
			}
			lpfc_sli_release_iocbq(phba, iocb);
			pring->missbufcnt = cnt;
			return cnt;
		}
		lpfc_sli_ringpostbuf_put(phba, pring, mp1);
		if (mp2)
			lpfc_sli_ringpostbuf_put(phba, pring, mp2);
	}
	pring->missbufcnt = 0;
	return 0;
}

/**
 * lpfc_post_rcv_buf - Post the initial receive IOCB buffers to ELS ring
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine posts initial receive IOCB buffers to the ELS ring. The
 * current number of initial IOCB buffers specified by LPFC_BUF_RING0 is
 * set to 64 IOCBs.
 *
 * Return codes
 *   0 - success (currently always success)
 **/
static int
lpfc_post_rcv_buf(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;

	/* Ring 0, ELS / CT buffers */
	lpfc_post_buffer(phba, &psli->ring[LPFC_ELS_RING], LPFC_BUF_RING0);
	/* Ring 2 - FCP no buffers needed */

	return 0;
}

#define S(N,V) (((V)<<(N))|((V)>>(32-(N))))

/**
 * lpfc_sha_init - Set up initial array of hash table entries
 * @HashResultPointer: pointer to an array as hash table.
 *
 * This routine sets up the initial values to the array of hash table entries
 * for the LC HBAs.
 **/
static void
lpfc_sha_init(uint32_t * HashResultPointer)
{
	HashResultPointer[0] = 0x67452301;
	HashResultPointer[1] = 0xEFCDAB89;
	HashResultPointer[2] = 0x98BADCFE;
	HashResultPointer[3] = 0x10325476;
	HashResultPointer[4] = 0xC3D2E1F0;
}

/**
 * lpfc_sha_iterate - Iterate initial hash table with the working hash table
 * @HashResultPointer: pointer to an initial/result hash table.
 * @HashWorkingPointer: pointer to an working hash table.
 *
 * This routine iterates an initial hash table pointed by @HashResultPointer
 * with the values from the working hash table pointeed by @HashWorkingPointer.
 * The results are putting back to the initial hash table, returned through
 * the @HashResultPointer as the result hash table.
 **/
static void
lpfc_sha_iterate(uint32_t * HashResultPointer, uint32_t * HashWorkingPointer)
{
	int t;
	uint32_t TEMP;
	uint32_t A, B, C, D, E;
	t = 16;
	do {
		HashWorkingPointer[t] =
		    S(1,
		      HashWorkingPointer[t - 3] ^ HashWorkingPointer[t -
								     8] ^
		      HashWorkingPointer[t - 14] ^ HashWorkingPointer[t - 16]);
	} while (++t <= 79);
	t = 0;
	A = HashResultPointer[0];
	B = HashResultPointer[1];
	C = HashResultPointer[2];
	D = HashResultPointer[3];
	E = HashResultPointer[4];

	do {
		if (t < 20) {
			TEMP = ((B & C) | ((~B) & D)) + 0x5A827999;
		} else if (t < 40) {
			TEMP = (B ^ C ^ D) + 0x6ED9EBA1;
		} else if (t < 60) {
			TEMP = ((B & C) | (B & D) | (C & D)) + 0x8F1BBCDC;
		} else {
			TEMP = (B ^ C ^ D) + 0xCA62C1D6;
		}
		TEMP += S(5, A) + E + HashWorkingPointer[t];
		E = D;
		D = C;
		C = S(30, B);
		B = A;
		A = TEMP;
	} while (++t <= 79);

	HashResultPointer[0] += A;
	HashResultPointer[1] += B;
	HashResultPointer[2] += C;
	HashResultPointer[3] += D;
	HashResultPointer[4] += E;

}

/**
 * lpfc_challenge_key - Create challenge key based on WWPN of the HBA
 * @RandomChallenge: pointer to the entry of host challenge random number array.
 * @HashWorking: pointer to the entry of the working hash array.
 *
 * This routine calculates the working hash array referred by @HashWorking
 * from the challenge random numbers associated with the host, referred by
 * @RandomChallenge. The result is put into the entry of the working hash
 * array and returned by reference through @HashWorking.
 **/
static void
lpfc_challenge_key(uint32_t * RandomChallenge, uint32_t * HashWorking)
{
	*HashWorking = (*RandomChallenge ^ *HashWorking);
}

/**
 * lpfc_hba_init - Perform special handling for LC HBA initialization
 * @phba: pointer to lpfc hba data structure.
 * @hbainit: pointer to an array of unsigned 32-bit integers.
 *
 * This routine performs the special handling for LC HBA initialization.
 **/
void
lpfc_hba_init(struct lpfc_hba *phba, uint32_t *hbainit)
{
	int t;
	uint32_t *HashWorking;
	uint32_t *pwwnn = (uint32_t *) phba->wwnn;

	HashWorking = kcalloc(80, sizeof(uint32_t), GFP_KERNEL);
	if (!HashWorking)
		return;

	HashWorking[0] = HashWorking[78] = *pwwnn++;
	HashWorking[1] = HashWorking[79] = *pwwnn;

	for (t = 0; t < 7; t++)
		lpfc_challenge_key(phba->RandomData + t, HashWorking + t);

	lpfc_sha_init(hbainit);
	lpfc_sha_iterate(hbainit, HashWorking);
	kfree(HashWorking);
}

/**
 * lpfc_cleanup - Performs vport cleanups before deleting a vport
 * @vport: pointer to a virtual N_Port data structure.
 *
 * This routine performs the necessary cleanups before deleting the @vport.
 * It invokes the discovery state machine to perform necessary state
 * transitions and to release the ndlps associated with the @vport. Note,
 * the physical port is treated as @vport 0.
 **/
void
lpfc_cleanup(struct lpfc_vport *vport)
{
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_nodelist *ndlp, *next_ndlp;
	int i = 0;

	if (phba->link_state > LPFC_LINK_DOWN)
		lpfc_port_link_failure(vport);

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp)) {
			ndlp = lpfc_enable_node(vport, ndlp,
						NLP_STE_UNUSED_NODE);
			if (!ndlp)
				continue;
			spin_lock_irq(&phba->ndlp_lock);
			NLP_SET_FREE_REQ(ndlp);
			spin_unlock_irq(&phba->ndlp_lock);
			/* Trigger the release of the ndlp memory */
			lpfc_nlp_put(ndlp);
			continue;
		}
		spin_lock_irq(&phba->ndlp_lock);
		if (NLP_CHK_FREE_REQ(ndlp)) {
			/* The ndlp should not be in memory free mode already */
			spin_unlock_irq(&phba->ndlp_lock);
			continue;
		} else
			/* Indicate request for freeing ndlp memory */
			NLP_SET_FREE_REQ(ndlp);
		spin_unlock_irq(&phba->ndlp_lock);

		if (vport->port_type != LPFC_PHYSICAL_PORT &&
		    ndlp->nlp_DID == Fabric_DID) {
			/* Just free up ndlp with Fabric_DID for vports */
			lpfc_nlp_put(ndlp);
			continue;
		}

		if (ndlp->nlp_type & NLP_FABRIC)
			lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RECOVERY);

		lpfc_disc_state_machine(vport, ndlp, NULL,
					     NLP_EVT_DEVICE_RM);

	}

	/* At this point, ALL ndlp's should be gone
	 * because of the previous NLP_EVT_DEVICE_RM.
	 * Lets wait for this to happen, if needed.
	 */
	while (!list_empty(&vport->fc_nodes)) {
		if (i++ > 3000) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_DISCOVERY,
				"0233 Nodelist not empty\n");
			list_for_each_entry_safe(ndlp, next_ndlp,
						&vport->fc_nodes, nlp_listp) {
				lpfc_printf_vlog(ndlp->vport, KERN_ERR,
						LOG_NODE,
						"0282 did:x%x ndlp:x%p "
						"usgmap:x%x refcnt:%d\n",
						ndlp->nlp_DID, (void *)ndlp,
						ndlp->nlp_usg_map,
						atomic_read(
							&ndlp->kref.refcount));
			}
			break;
		}

		/* Wait for any activity on ndlps to settle */
		msleep(10);
	}
}

/**
 * lpfc_stop_vport_timers - Stop all the timers associated with a vport
 * @vport: pointer to a virtual N_Port data structure.
 *
 * This routine stops all the timers associated with a @vport. This function
 * is invoked before disabling or deleting a @vport. Note that the physical
 * port is treated as @vport 0.
 **/
void
lpfc_stop_vport_timers(struct lpfc_vport *vport)
{
	del_timer_sync(&vport->els_tmofunc);
	del_timer_sync(&vport->fc_fdmitmo);
	lpfc_can_disctmo(vport);
	return;
}

/**
 * lpfc_stop_hba_timers - Stop all the timers associated with an HBA
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine stops all the timers associated with a HBA. This function is
 * invoked before either putting a HBA offline or unloading the driver.
 **/
void
lpfc_stop_hba_timers(struct lpfc_hba *phba)
{
	lpfc_stop_vport_timers(phba->pport);
	del_timer_sync(&phba->sli.mbox_tmo);
	del_timer_sync(&phba->fabric_block_timer);
	del_timer_sync(&phba->eratt_poll);
	del_timer_sync(&phba->hb_tmofunc);
	phba->hb_outstanding = 0;

	switch (phba->pci_dev_grp) {
	case LPFC_PCI_DEV_LP:
		/* Stop any LightPulse device specific driver timers */
		del_timer_sync(&phba->fcp_poll_timer);
		break;
	case LPFC_PCI_DEV_OC:
		/* Stop any OneConnect device sepcific driver timers */
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0297 Invalid device group (x%x)\n",
				phba->pci_dev_grp);
		break;
	}
	return;
}

/**
 * lpfc_block_mgmt_io - Mark a HBA's management interface as blocked
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine marks a HBA's management interface as blocked. Once the HBA's
 * management interface is marked as blocked, all the user space access to
 * the HBA, whether they are from sysfs interface or libdfc interface will
 * all be blocked. The HBA is set to block the management interface when the
 * driver prepares the HBA interface for online or offline.
 **/
static void
lpfc_block_mgmt_io(struct lpfc_hba * phba)
{
	unsigned long iflag;

	spin_lock_irqsave(&phba->hbalock, iflag);
	phba->sli.sli_flag |= LPFC_BLOCK_MGMT_IO;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
}

/**
 * lpfc_online - Initialize and bring a HBA online
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine initializes the HBA and brings a HBA online. During this
 * process, the management interface is blocked to prevent user space access
 * to the HBA interfering with the driver initialization.
 *
 * Return codes
 *   0 - successful
 *   1 - failed
 **/
int
lpfc_online(struct lpfc_hba *phba)
{
	struct lpfc_vport *vport;
	struct lpfc_vport **vports;
	int i;

	if (!phba)
		return 0;
	vport = phba->pport;

	if (!(vport->fc_flag & FC_OFFLINE_MODE))
		return 0;

	lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
			"0458 Bring Adapter online\n");

	lpfc_block_mgmt_io(phba);

	if (!lpfc_sli_queue_setup(phba)) {
		lpfc_unblock_mgmt_io(phba);
		return 1;
	}

	if (phba->sli_rev == LPFC_SLI_REV4) {
		if (lpfc_sli4_hba_setup(phba)) { /* Initialize SLI4 HBA */
			lpfc_unblock_mgmt_io(phba);
			return 1;
		}
	} else {
		if (lpfc_sli_hba_setup(phba)) {	/* Initialize SLI2/SLI3 HBA */
			lpfc_unblock_mgmt_io(phba);
			return 1;
		}
	}

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			struct Scsi_Host *shost;
			shost = lpfc_shost_from_vport(vports[i]);
			spin_lock_irq(shost->host_lock);
			vports[i]->fc_flag &= ~FC_OFFLINE_MODE;
			if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED)
				vports[i]->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			if (phba->sli_rev == LPFC_SLI_REV4)
				vports[i]->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
			spin_unlock_irq(shost->host_lock);
		}
		lpfc_destroy_vport_work_array(phba, vports);

	lpfc_unblock_mgmt_io(phba);
	return 0;
}

/**
 * lpfc_unblock_mgmt_io - Mark a HBA's management interface to be not blocked
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine marks a HBA's management interface as not blocked. Once the
 * HBA's management interface is marked as not blocked, all the user space
 * access to the HBA, whether they are from sysfs interface or libdfc
 * interface will be allowed. The HBA is set to block the management interface
 * when the driver prepares the HBA interface for online or offline and then
 * set to unblock the management interface afterwards.
 **/
void
lpfc_unblock_mgmt_io(struct lpfc_hba * phba)
{
	unsigned long iflag;

	spin_lock_irqsave(&phba->hbalock, iflag);
	phba->sli.sli_flag &= ~LPFC_BLOCK_MGMT_IO;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
}

/**
 * lpfc_offline_prep - Prepare a HBA to be brought offline
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to prepare a HBA to be brought offline. It performs
 * unregistration login to all the nodes on all vports and flushes the mailbox
 * queue to make it ready to be brought offline.
 **/
void
lpfc_offline_prep(struct lpfc_hba * phba)
{
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_nodelist  *ndlp, *next_ndlp;
	struct lpfc_vport **vports;
	int i;

	if (vport->fc_flag & FC_OFFLINE_MODE)
		return;

	lpfc_block_mgmt_io(phba);

	lpfc_linkdown(phba);

	/* Issue an unreg_login to all nodes on all vports */
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL) {
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			struct Scsi_Host *shost;

			if (vports[i]->load_flag & FC_UNLOADING)
				continue;
			vports[i]->vfi_state &= ~LPFC_VFI_REGISTERED;
			shost =	lpfc_shost_from_vport(vports[i]);
			list_for_each_entry_safe(ndlp, next_ndlp,
						 &vports[i]->fc_nodes,
						 nlp_listp) {
				if (!NLP_CHK_NODE_ACT(ndlp))
					continue;
				if (ndlp->nlp_state == NLP_STE_UNUSED_NODE)
					continue;
				if (ndlp->nlp_type & NLP_FABRIC) {
					lpfc_disc_state_machine(vports[i], ndlp,
						NULL, NLP_EVT_DEVICE_RECOVERY);
					lpfc_disc_state_machine(vports[i], ndlp,
						NULL, NLP_EVT_DEVICE_RM);
				}
				spin_lock_irq(shost->host_lock);
				ndlp->nlp_flag &= ~NLP_NPR_ADISC;
				spin_unlock_irq(shost->host_lock);
				lpfc_unreg_rpi(vports[i], ndlp);
			}
		}
	}
	lpfc_destroy_vport_work_array(phba, vports);

	lpfc_sli_mbox_sys_shutdown(phba);
}

/**
 * lpfc_offline - Bring a HBA offline
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine actually brings a HBA offline. It stops all the timers
 * associated with the HBA, brings down the SLI layer, and eventually
 * marks the HBA as in offline state for the upper layer protocol.
 **/
void
lpfc_offline(struct lpfc_hba *phba)
{
	struct Scsi_Host  *shost;
	struct lpfc_vport **vports;
	int i;

	if (phba->pport->fc_flag & FC_OFFLINE_MODE)
		return;

	/* stop port and all timers associated with this hba */
	lpfc_stop_port(phba);
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++)
			lpfc_stop_vport_timers(vports[i]);
	lpfc_destroy_vport_work_array(phba, vports);
	lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
			"0460 Bring Adapter offline\n");
	/* Bring down the SLI Layer and cleanup.  The HBA is offline
	   now.  */
	lpfc_sli_hba_down(phba);
	spin_lock_irq(&phba->hbalock);
	phba->work_ha = 0;
	spin_unlock_irq(&phba->hbalock);
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			shost = lpfc_shost_from_vport(vports[i]);
			spin_lock_irq(shost->host_lock);
			vports[i]->work_port_events = 0;
			vports[i]->fc_flag |= FC_OFFLINE_MODE;
			spin_unlock_irq(shost->host_lock);
		}
	lpfc_destroy_vport_work_array(phba, vports);
}

/**
 * lpfc_scsi_free - Free all the SCSI buffers and IOCBs from driver lists
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is to free all the SCSI buffers and IOCBs from the driver
 * list back to kernel. It is called from lpfc_pci_remove_one to free
 * the internal resources before the device is removed from the system.
 *
 * Return codes
 *   0 - successful (for now, it always returns 0)
 **/
static int
lpfc_scsi_free(struct lpfc_hba *phba)
{
	struct lpfc_scsi_buf *sb, *sb_next;
	struct lpfc_iocbq *io, *io_next;

	spin_lock_irq(&phba->hbalock);
	/* Release all the lpfc_scsi_bufs maintained by this host. */
	list_for_each_entry_safe(sb, sb_next, &phba->lpfc_scsi_buf_list, list) {
		list_del(&sb->list);
		pci_pool_free(phba->lpfc_scsi_dma_buf_pool, sb->data,
			      sb->dma_handle);
		kfree(sb);
		phba->total_scsi_bufs--;
	}

	/* Release all the lpfc_iocbq entries maintained by this host. */
	list_for_each_entry_safe(io, io_next, &phba->lpfc_iocb_list, list) {
		list_del(&io->list);
		kfree(io);
		phba->total_iocbq_bufs--;
	}

	spin_unlock_irq(&phba->hbalock);

	return 0;
}

/**
 * lpfc_create_port - Create an FC port
 * @phba: pointer to lpfc hba data structure.
 * @instance: a unique integer ID to this FC port.
 * @dev: pointer to the device data structure.
 *
 * This routine creates a FC port for the upper layer protocol. The FC port
 * can be created on top of either a physical port or a virtual port provided
 * by the HBA. This routine also allocates a SCSI host data structure (shost)
 * and associates the FC port created before adding the shost into the SCSI
 * layer.
 *
 * Return codes
 *   @vport - pointer to the virtual N_Port data structure.
 *   NULL - port create failed.
 **/
struct lpfc_vport *
lpfc_create_port(struct lpfc_hba *phba, int instance, struct device *dev)
{
	struct lpfc_vport *vport;
	struct Scsi_Host  *shost;
	int error = 0;

	if (dev != &phba->pcidev->dev)
		shost = scsi_host_alloc(&lpfc_vport_template,
					sizeof(struct lpfc_vport));
	else
		shost = scsi_host_alloc(&lpfc_template,
					sizeof(struct lpfc_vport));
	if (!shost)
		goto out;

	vport = (struct lpfc_vport *) shost->hostdata;
	vport->phba = phba;
	vport->load_flag |= FC_LOADING;
	vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
	vport->fc_rscn_flush = 0;

	lpfc_get_vport_cfgparam(vport);
	shost->unique_id = instance;
	shost->max_id = LPFC_MAX_TARGET;
	shost->max_lun = vport->cfg_max_luns;
	shost->this_id = -1;
	shost->max_cmd_len = 16;
	if (phba->sli_rev == LPFC_SLI_REV4) {
		shost->dma_boundary = LPFC_SLI4_MAX_SEGMENT_SIZE;
		shost->sg_tablesize = phba->cfg_sg_seg_cnt;
	}

	/*
	 * Set initial can_queue value since 0 is no longer supported and
	 * scsi_add_host will fail. This will be adjusted later based on the
	 * max xri value determined in hba setup.
	 */
	shost->can_queue = phba->cfg_hba_queue_depth - 10;
	if (dev != &phba->pcidev->dev) {
		shost->transportt = lpfc_vport_transport_template;
		vport->port_type = LPFC_NPIV_PORT;
	} else {
		shost->transportt = lpfc_transport_template;
		vport->port_type = LPFC_PHYSICAL_PORT;
	}

	/* Initialize all internally managed lists. */
	INIT_LIST_HEAD(&vport->fc_nodes);
	INIT_LIST_HEAD(&vport->rcv_buffer_list);
	spin_lock_init(&vport->work_port_lock);

	init_timer(&vport->fc_disctmo);
	vport->fc_disctmo.function = lpfc_disc_timeout;
	vport->fc_disctmo.data = (unsigned long)vport;

	init_timer(&vport->fc_fdmitmo);
	vport->fc_fdmitmo.function = lpfc_fdmi_tmo;
	vport->fc_fdmitmo.data = (unsigned long)vport;

	init_timer(&vport->els_tmofunc);
	vport->els_tmofunc.function = lpfc_els_timeout;
	vport->els_tmofunc.data = (unsigned long)vport;

	error = scsi_add_host_with_dma(shost, dev, &phba->pcidev->dev);
	if (error)
		goto out_put_shost;

	spin_lock_irq(&phba->hbalock);
	list_add_tail(&vport->listentry, &phba->port_list);
	spin_unlock_irq(&phba->hbalock);
	return vport;

out_put_shost:
	scsi_host_put(shost);
out:
	return NULL;
}

/**
 * destroy_port -  destroy an FC port
 * @vport: pointer to an lpfc virtual N_Port data structure.
 *
 * This routine destroys a FC port from the upper layer protocol. All the
 * resources associated with the port are released.
 **/
void
destroy_port(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;

	lpfc_debugfs_terminate(vport);
	fc_remove_host(shost);
	scsi_remove_host(shost);

	spin_lock_irq(&phba->hbalock);
	list_del_init(&vport->listentry);
	spin_unlock_irq(&phba->hbalock);

	lpfc_cleanup(vport);
	return;
}

/**
 * lpfc_get_instance - Get a unique integer ID
 *
 * This routine allocates a unique integer ID from lpfc_hba_index pool. It
 * uses the kernel idr facility to perform the task.
 *
 * Return codes:
 *   instance - a unique integer ID allocated as the new instance.
 *   -1 - lpfc get instance failed.
 **/
int
lpfc_get_instance(void)
{
	int instance = 0;

	/* Assign an unused number */
	if (!idr_pre_get(&lpfc_hba_index, GFP_KERNEL))
		return -1;
	if (idr_get_new(&lpfc_hba_index, NULL, &instance))
		return -1;
	return instance;
}

/**
 * lpfc_scan_finished - method for SCSI layer to detect whether scan is done
 * @shost: pointer to SCSI host data structure.
 * @time: elapsed time of the scan in jiffies.
 *
 * This routine is called by the SCSI layer with a SCSI host to determine
 * whether the scan host is finished.
 *
 * Note: there is no scan_start function as adapter initialization will have
 * asynchronously kicked off the link initialization.
 *
 * Return codes
 *   0 - SCSI host scan is not over yet.
 *   1 - SCSI host scan is over.
 **/
int lpfc_scan_finished(struct Scsi_Host *shost, unsigned long time)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	int stat = 0;

	spin_lock_irq(shost->host_lock);

	if (vport->load_flag & FC_UNLOADING) {
		stat = 1;
		goto finished;
	}
	if (time >= 30 * HZ) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0461 Scanning longer than 30 "
				"seconds.  Continuing initialization\n");
		stat = 1;
		goto finished;
	}
	if (time >= 15 * HZ && phba->link_state <= LPFC_LINK_DOWN) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0465 Link down longer than 15 "
				"seconds.  Continuing initialization\n");
		stat = 1;
		goto finished;
	}

	if (vport->port_state != LPFC_VPORT_READY)
		goto finished;
	if (vport->num_disc_nodes || vport->fc_prli_sent)
		goto finished;
	if (vport->fc_map_cnt == 0 && time < 2 * HZ)
		goto finished;
	if ((phba->sli.sli_flag & LPFC_SLI_MBOX_ACTIVE) != 0)
		goto finished;

	stat = 1;

finished:
	spin_unlock_irq(shost->host_lock);
	return stat;
}

/**
 * lpfc_host_attrib_init - Initialize SCSI host attributes on a FC port
 * @shost: pointer to SCSI host data structure.
 *
 * This routine initializes a given SCSI host attributes on a FC port. The
 * SCSI host can be either on top of a physical port or a virtual port.
 **/
void lpfc_host_attrib_init(struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	/*
	 * Set fixed host attributes.  Must done after lpfc_sli_hba_setup().
	 */

	fc_host_node_name(shost) = wwn_to_u64(vport->fc_nodename.u.wwn);
	fc_host_port_name(shost) = wwn_to_u64(vport->fc_portname.u.wwn);
	fc_host_supported_classes(shost) = FC_COS_CLASS3;

	memset(fc_host_supported_fc4s(shost), 0,
	       sizeof(fc_host_supported_fc4s(shost)));
	fc_host_supported_fc4s(shost)[2] = 1;
	fc_host_supported_fc4s(shost)[7] = 1;

	lpfc_vport_symbolic_node_name(vport, fc_host_symbolic_name(shost),
				 sizeof fc_host_symbolic_name(shost));

	fc_host_supported_speeds(shost) = 0;
	if (phba->lmt & LMT_10Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_10GBIT;
	if (phba->lmt & LMT_8Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_8GBIT;
	if (phba->lmt & LMT_4Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_4GBIT;
	if (phba->lmt & LMT_2Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_2GBIT;
	if (phba->lmt & LMT_1Gb)
		fc_host_supported_speeds(shost) |= FC_PORTSPEED_1GBIT;

	fc_host_maxframe_size(shost) =
		(((uint32_t) vport->fc_sparam.cmn.bbRcvSizeMsb & 0x0F) << 8) |
		(uint32_t) vport->fc_sparam.cmn.bbRcvSizeLsb;

	/* This value is also unchanging */
	memset(fc_host_active_fc4s(shost), 0,
	       sizeof(fc_host_active_fc4s(shost)));
	fc_host_active_fc4s(shost)[2] = 1;
	fc_host_active_fc4s(shost)[7] = 1;

	fc_host_max_npiv_vports(shost) = phba->max_vpi;
	spin_lock_irq(shost->host_lock);
	vport->load_flag &= ~FC_LOADING;
	spin_unlock_irq(shost->host_lock);
}

/**
 * lpfc_stop_port_s3 - Stop SLI3 device port
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to stop an SLI3 device port, it stops the device
 * from generating interrupts and stops the device driver's timers for the
 * device.
 **/
static void
lpfc_stop_port_s3(struct lpfc_hba *phba)
{
	/* Clear all interrupt enable conditions */
	writel(0, phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */
	/* Clear all pending interrupts */
	writel(0xffffffff, phba->HAregaddr);
	readl(phba->HAregaddr); /* flush */

	/* Reset some HBA SLI setup states */
	lpfc_stop_hba_timers(phba);
	phba->pport->work_port_events = 0;
}

/**
 * lpfc_stop_port_s4 - Stop SLI4 device port
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to stop an SLI4 device port, it stops the device
 * from generating interrupts and stops the device driver's timers for the
 * device.
 **/
static void
lpfc_stop_port_s4(struct lpfc_hba *phba)
{
	/* Reset some HBA SLI4 setup states */
	lpfc_stop_hba_timers(phba);
	phba->pport->work_port_events = 0;
	phba->sli4_hba.intr_enable = 0;
	/* Hard clear it for now, shall have more graceful way to wait later */
	phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
}

/**
 * lpfc_stop_port - Wrapper function for stopping hba port
 * @phba: Pointer to HBA context object.
 *
 * This routine wraps the actual SLI3 or SLI4 hba stop port routine from
 * the API jump table function pointer from the lpfc_hba struct.
 **/
void
lpfc_stop_port(struct lpfc_hba *phba)
{
	phba->lpfc_stop_port(phba);
}

/**
 * lpfc_sli4_remove_dflt_fcf - Remove the driver default fcf record from the port.
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to remove the driver default fcf record from
 * the port.  This routine currently acts on FCF Index 0.
 *
 **/
void
lpfc_sli_remove_dflt_fcf(struct lpfc_hba *phba)
{
	int rc = 0;
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_mbx_del_fcf_tbl_entry *del_fcf_record;
	uint32_t mbox_tmo, req_len;
	uint32_t shdr_status, shdr_add_status;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2020 Failed to allocate mbox for ADD_FCF cmd\n");
		return;
	}

	req_len = sizeof(struct lpfc_mbx_del_fcf_tbl_entry) -
		  sizeof(struct lpfc_sli4_cfg_mhdr);
	rc = lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_FCOE,
			      LPFC_MBOX_OPCODE_FCOE_DELETE_FCF,
			      req_len, LPFC_SLI4_MBX_EMBED);
	/*
	 * In phase 1, there is a single FCF index, 0.  In phase2, the driver
	 * supports multiple FCF indices.
	 */
	del_fcf_record = &mboxq->u.mqe.un.del_fcf_entry;
	bf_set(lpfc_mbx_del_fcf_tbl_count, del_fcf_record, 1);
	bf_set(lpfc_mbx_del_fcf_tbl_index, del_fcf_record,
	       phba->fcf.fcf_indx);

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, MBX_SLI4_CONFIG);
		rc = lpfc_sli_issue_mbox_wait(phba, mboxq, mbox_tmo);
	}
	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr_status = bf_get(lpfc_mbox_hdr_status,
			     &del_fcf_record->header.cfg_shdr.response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status,
				 &del_fcf_record->header.cfg_shdr.response);
	if (shdr_status || shdr_add_status || rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2516 DEL FCF of default FCF Index failed "
				"mbx status x%x, status x%x add_status x%x\n",
				rc, shdr_status, shdr_add_status);
	}
	if (rc != MBX_TIMEOUT)
		mempool_free(mboxq, phba->mbox_mem_pool);
}

/**
 * lpfc_sli4_parse_latt_fault - Parse sli4 link-attention link fault code
 * @phba: pointer to lpfc hba data structure.
 * @acqe_link: pointer to the async link completion queue entry.
 *
 * This routine is to parse the SLI4 link-attention link fault code and
 * translate it into the base driver's read link attention mailbox command
 * status.
 *
 * Return: Link-attention status in terms of base driver's coding.
 **/
static uint16_t
lpfc_sli4_parse_latt_fault(struct lpfc_hba *phba,
			   struct lpfc_acqe_link *acqe_link)
{
	uint16_t latt_fault;

	switch (bf_get(lpfc_acqe_link_fault, acqe_link)) {
	case LPFC_ASYNC_LINK_FAULT_NONE:
	case LPFC_ASYNC_LINK_FAULT_LOCAL:
	case LPFC_ASYNC_LINK_FAULT_REMOTE:
		latt_fault = 0;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0398 Invalid link fault code: x%x\n",
				bf_get(lpfc_acqe_link_fault, acqe_link));
		latt_fault = MBXERR_ERROR;
		break;
	}
	return latt_fault;
}

/**
 * lpfc_sli4_parse_latt_type - Parse sli4 link attention type
 * @phba: pointer to lpfc hba data structure.
 * @acqe_link: pointer to the async link completion queue entry.
 *
 * This routine is to parse the SLI4 link attention type and translate it
 * into the base driver's link attention type coding.
 *
 * Return: Link attention type in terms of base driver's coding.
 **/
static uint8_t
lpfc_sli4_parse_latt_type(struct lpfc_hba *phba,
			  struct lpfc_acqe_link *acqe_link)
{
	uint8_t att_type;

	switch (bf_get(lpfc_acqe_link_status, acqe_link)) {
	case LPFC_ASYNC_LINK_STATUS_DOWN:
	case LPFC_ASYNC_LINK_STATUS_LOGICAL_DOWN:
		att_type = AT_LINK_DOWN;
		break;
	case LPFC_ASYNC_LINK_STATUS_UP:
		/* Ignore physical link up events - wait for logical link up */
		att_type = AT_RESERVED;
		break;
	case LPFC_ASYNC_LINK_STATUS_LOGICAL_UP:
		att_type = AT_LINK_UP;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0399 Invalid link attention type: x%x\n",
				bf_get(lpfc_acqe_link_status, acqe_link));
		att_type = AT_RESERVED;
		break;
	}
	return att_type;
}

/**
 * lpfc_sli4_parse_latt_link_speed - Parse sli4 link-attention link speed
 * @phba: pointer to lpfc hba data structure.
 * @acqe_link: pointer to the async link completion queue entry.
 *
 * This routine is to parse the SLI4 link-attention link speed and translate
 * it into the base driver's link-attention link speed coding.
 *
 * Return: Link-attention link speed in terms of base driver's coding.
 **/
static uint8_t
lpfc_sli4_parse_latt_link_speed(struct lpfc_hba *phba,
				struct lpfc_acqe_link *acqe_link)
{
	uint8_t link_speed;

	switch (bf_get(lpfc_acqe_link_speed, acqe_link)) {
	case LPFC_ASYNC_LINK_SPEED_ZERO:
		link_speed = LA_UNKNW_LINK;
		break;
	case LPFC_ASYNC_LINK_SPEED_10MBPS:
		link_speed = LA_UNKNW_LINK;
		break;
	case LPFC_ASYNC_LINK_SPEED_100MBPS:
		link_speed = LA_UNKNW_LINK;
		break;
	case LPFC_ASYNC_LINK_SPEED_1GBPS:
		link_speed = LA_1GHZ_LINK;
		break;
	case LPFC_ASYNC_LINK_SPEED_10GBPS:
		link_speed = LA_10GHZ_LINK;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0483 Invalid link-attention link speed: x%x\n",
				bf_get(lpfc_acqe_link_speed, acqe_link));
		link_speed = LA_UNKNW_LINK;
		break;
	}
	return link_speed;
}

/**
 * lpfc_sli4_async_link_evt - Process the asynchronous link event
 * @phba: pointer to lpfc hba data structure.
 * @acqe_link: pointer to the async link completion queue entry.
 *
 * This routine is to handle the SLI4 asynchronous link event.
 **/
static void
lpfc_sli4_async_link_evt(struct lpfc_hba *phba,
			 struct lpfc_acqe_link *acqe_link)
{
	struct lpfc_dmabuf *mp;
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	READ_LA_VAR *la;
	uint8_t att_type;

	att_type = lpfc_sli4_parse_latt_type(phba, acqe_link);
	if (att_type != AT_LINK_DOWN && att_type != AT_LINK_UP)
		return;
	phba->fcoe_eventtag = acqe_link->event_tag;
	pmb = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0395 The mboxq allocation failed\n");
		return;
	}
	mp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!mp) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0396 The lpfc_dmabuf allocation failed\n");
		goto out_free_pmb;
	}
	mp->virt = lpfc_mbuf_alloc(phba, 0, &mp->phys);
	if (!mp->virt) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0397 The mbuf allocation failed\n");
		goto out_free_dmabuf;
	}

	/* Cleanup any outstanding ELS commands */
	lpfc_els_flush_all_cmd(phba);

	/* Block ELS IOCBs until we have done process link event */
	phba->sli.ring[LPFC_ELS_RING].flag |= LPFC_STOP_IOCB_EVENT;

	/* Update link event statistics */
	phba->sli.slistat.link_event++;

	/* Create pseudo lpfc_handle_latt mailbox command from link ACQE */
	lpfc_read_la(phba, pmb, mp);
	pmb->vport = phba->pport;

	/* Parse and translate status field */
	mb = &pmb->u.mb;
	mb->mbxStatus = lpfc_sli4_parse_latt_fault(phba, acqe_link);

	/* Parse and translate link attention fields */
	la = (READ_LA_VAR *) &pmb->u.mb.un.varReadLA;
	la->eventTag = acqe_link->event_tag;
	la->attType = att_type;
	la->UlnkSpeed = lpfc_sli4_parse_latt_link_speed(phba, acqe_link);

	/* Fake the the following irrelvant fields */
	la->topology = TOPOLOGY_PT_PT;
	la->granted_AL_PA = 0;
	la->il = 0;
	la->pb = 0;
	la->fa = 0;
	la->mm = 0;

	/* Keep the link status for extra SLI4 state machine reference */
	phba->sli4_hba.link_state.speed =
				bf_get(lpfc_acqe_link_speed, acqe_link);
	phba->sli4_hba.link_state.duplex =
				bf_get(lpfc_acqe_link_duplex, acqe_link);
	phba->sli4_hba.link_state.status =
				bf_get(lpfc_acqe_link_status, acqe_link);
	phba->sli4_hba.link_state.physical =
				bf_get(lpfc_acqe_link_physical, acqe_link);
	phba->sli4_hba.link_state.fault =
				bf_get(lpfc_acqe_link_fault, acqe_link);

	/* Invoke the lpfc_handle_latt mailbox command callback function */
	lpfc_mbx_cmpl_read_la(phba, pmb);

	return;

out_free_dmabuf:
	kfree(mp);
out_free_pmb:
	mempool_free(pmb, phba->mbox_mem_pool);
}

/**
 * lpfc_sli4_async_fcoe_evt - Process the asynchronous fcoe event
 * @phba: pointer to lpfc hba data structure.
 * @acqe_link: pointer to the async fcoe completion queue entry.
 *
 * This routine is to handle the SLI4 asynchronous fcoe event.
 **/
static void
lpfc_sli4_async_fcoe_evt(struct lpfc_hba *phba,
			 struct lpfc_acqe_fcoe *acqe_fcoe)
{
	uint8_t event_type = bf_get(lpfc_acqe_fcoe_event_type, acqe_fcoe);
	int rc;

	phba->fcoe_eventtag = acqe_fcoe->event_tag;
	switch (event_type) {
	case LPFC_FCOE_EVENT_TYPE_NEW_FCF:
		lpfc_printf_log(phba, KERN_ERR, LOG_DISCOVERY,
			"2546 New FCF found index 0x%x tag 0x%x\n",
			acqe_fcoe->fcf_index,
			acqe_fcoe->event_tag);
		/*
		 * If the current FCF is in discovered state, or
		 * FCF discovery is in progress do nothing.
		 */
		spin_lock_irq(&phba->hbalock);
		if ((phba->fcf.fcf_flag & FCF_DISCOVERED) ||
		   (phba->hba_flag & FCF_DISC_INPROGRESS)) {
			spin_unlock_irq(&phba->hbalock);
			break;
		}
		spin_unlock_irq(&phba->hbalock);

		/* Read the FCF table and re-discover SAN. */
		rc = lpfc_sli4_read_fcf_record(phba,
			LPFC_FCOE_FCF_GET_FIRST);
		if (rc)
			lpfc_printf_log(phba, KERN_ERR, LOG_DISCOVERY,
				"2547 Read FCF record failed 0x%x\n",
				rc);
		break;

	case LPFC_FCOE_EVENT_TYPE_FCF_TABLE_FULL:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"2548 FCF Table full count 0x%x tag 0x%x\n",
			bf_get(lpfc_acqe_fcoe_fcf_count, acqe_fcoe),
			acqe_fcoe->event_tag);
		break;

	case LPFC_FCOE_EVENT_TYPE_FCF_DEAD:
		lpfc_printf_log(phba, KERN_ERR, LOG_DISCOVERY,
			"2549 FCF disconnected fron network index 0x%x"
			" tag 0x%x\n", acqe_fcoe->fcf_index,
			acqe_fcoe->event_tag);
		/* If the event is not for currently used fcf do nothing */
		if (phba->fcf.fcf_indx != acqe_fcoe->fcf_index)
			break;
		/*
		 * Currently, driver support only one FCF - so treat this as
		 * a link down.
		 */
		lpfc_linkdown(phba);
		/* Unregister FCF if no devices connected to it */
		lpfc_unregister_unused_fcf(phba);
		break;

	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0288 Unknown FCoE event type 0x%x event tag "
			"0x%x\n", event_type, acqe_fcoe->event_tag);
		break;
	}
}

/**
 * lpfc_sli4_async_dcbx_evt - Process the asynchronous dcbx event
 * @phba: pointer to lpfc hba data structure.
 * @acqe_link: pointer to the async dcbx completion queue entry.
 *
 * This routine is to handle the SLI4 asynchronous dcbx event.
 **/
static void
lpfc_sli4_async_dcbx_evt(struct lpfc_hba *phba,
			 struct lpfc_acqe_dcbx *acqe_dcbx)
{
	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0290 The SLI4 DCBX asynchronous event is not "
			"handled yet\n");
}

/**
 * lpfc_sli4_async_event_proc - Process all the pending asynchronous event
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked by the worker thread to process all the pending
 * SLI4 asynchronous events.
 **/
void lpfc_sli4_async_event_proc(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event;

	/* First, declare the async event has been handled */
	spin_lock_irq(&phba->hbalock);
	phba->hba_flag &= ~ASYNC_EVENT;
	spin_unlock_irq(&phba->hbalock);
	/* Now, handle all the async events */
	while (!list_empty(&phba->sli4_hba.sp_asynce_work_queue)) {
		/* Get the first event from the head of the event queue */
		spin_lock_irq(&phba->hbalock);
		list_remove_head(&phba->sli4_hba.sp_asynce_work_queue,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irq(&phba->hbalock);
		/* Process the asynchronous event */
		switch (bf_get(lpfc_trailer_code, &cq_event->cqe.mcqe_cmpl)) {
		case LPFC_TRAILER_CODE_LINK:
			lpfc_sli4_async_link_evt(phba,
						 &cq_event->cqe.acqe_link);
			break;
		case LPFC_TRAILER_CODE_FCOE:
			lpfc_sli4_async_fcoe_evt(phba,
						 &cq_event->cqe.acqe_fcoe);
			break;
		case LPFC_TRAILER_CODE_DCBX:
			lpfc_sli4_async_dcbx_evt(phba,
						 &cq_event->cqe.acqe_dcbx);
			break;
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"1804 Invalid asynchrous event code: "
					"x%x\n", bf_get(lpfc_trailer_code,
					&cq_event->cqe.mcqe_cmpl));
			break;
		}
		/* Free the completion event processed to the free pool */
		lpfc_sli4_cq_event_release(phba, cq_event);
	}
}

/**
 * lpfc_api_table_setup - Set up per hba pci-device group func api jump table
 * @phba: pointer to lpfc hba data structure.
 * @dev_grp: The HBA PCI-Device group number.
 *
 * This routine is invoked to set up the per HBA PCI-Device group function
 * API jump table entries.
 *
 * Return: 0 if success, otherwise -ENODEV
 **/
int
lpfc_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{
	int rc;

	/* Set up lpfc PCI-device group */
	phba->pci_dev_grp = dev_grp;

	/* The LPFC_PCI_DEV_OC uses SLI4 */
	if (dev_grp == LPFC_PCI_DEV_OC)
		phba->sli_rev = LPFC_SLI_REV4;

	/* Set up device INIT API function jump table */
	rc = lpfc_init_api_table_setup(phba, dev_grp);
	if (rc)
		return -ENODEV;
	/* Set up SCSI API function jump table */
	rc = lpfc_scsi_api_table_setup(phba, dev_grp);
	if (rc)
		return -ENODEV;
	/* Set up SLI API function jump table */
	rc = lpfc_sli_api_table_setup(phba, dev_grp);
	if (rc)
		return -ENODEV;
	/* Set up MBOX API function jump table */
	rc = lpfc_mbox_api_table_setup(phba, dev_grp);
	if (rc)
		return -ENODEV;

	return 0;
}

/**
 * lpfc_log_intr_mode - Log the active interrupt mode
 * @phba: pointer to lpfc hba data structure.
 * @intr_mode: active interrupt mode adopted.
 *
 * This routine it invoked to log the currently used active interrupt mode
 * to the device.
 **/
static void lpfc_log_intr_mode(struct lpfc_hba *phba, uint32_t intr_mode)
{
	switch (intr_mode) {
	case 0:
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0470 Enable INTx interrupt mode.\n");
		break;
	case 1:
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0481 Enabled MSI interrupt mode.\n");
		break;
	case 2:
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"0480 Enabled MSI-X interrupt mode.\n");
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0482 Illegal interrupt mode.\n");
		break;
	}
	return;
}

/**
 * lpfc_enable_pci_dev - Enable a generic PCI device.
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to enable the PCI device that is common to all
 * PCI devices.
 *
 * Return codes
 * 	0 - sucessful
 * 	other values - error
 **/
static int
lpfc_enable_pci_dev(struct lpfc_hba *phba)
{
	struct pci_dev *pdev;
	int bars;

	/* Obtain PCI device reference */
	if (!phba->pcidev)
		goto out_error;
	else
		pdev = phba->pcidev;
	/* Select PCI BARs */
	bars = pci_select_bars(pdev, IORESOURCE_MEM);
	/* Enable PCI device */
	if (pci_enable_device_mem(pdev))
		goto out_error;
	/* Request PCI resource for the device */
	if (pci_request_selected_regions(pdev, bars, LPFC_DRIVER_NAME))
		goto out_disable_device;
	/* Set up device as PCI master and save state for EEH */
	pci_set_master(pdev);
	pci_try_set_mwi(pdev);
	pci_save_state(pdev);

	return 0;

out_disable_device:
	pci_disable_device(pdev);
out_error:
	return -ENODEV;
}

/**
 * lpfc_disable_pci_dev - Disable a generic PCI device.
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to disable the PCI device that is common to all
 * PCI devices.
 **/
static void
lpfc_disable_pci_dev(struct lpfc_hba *phba)
{
	struct pci_dev *pdev;
	int bars;

	/* Obtain PCI device reference */
	if (!phba->pcidev)
		return;
	else
		pdev = phba->pcidev;
	/* Select PCI BARs */
	bars = pci_select_bars(pdev, IORESOURCE_MEM);
	/* Release PCI resource and disable PCI device */
	pci_release_selected_regions(pdev, bars);
	pci_disable_device(pdev);
	/* Null out PCI private reference to driver */
	pci_set_drvdata(pdev, NULL);

	return;
}

/**
 * lpfc_reset_hba - Reset a hba
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to reset a hba device. It brings the HBA
 * offline, performs a board restart, and then brings the board back
 * online. The lpfc_offline calls lpfc_sli_hba_down which will clean up
 * on outstanding mailbox commands.
 **/
void
lpfc_reset_hba(struct lpfc_hba *phba)
{
	/* If resets are disabled then set error state and return. */
	if (!phba->cfg_enable_hba_reset) {
		phba->link_state = LPFC_HBA_ERROR;
		return;
	}
	lpfc_offline_prep(phba);
	lpfc_offline(phba);
	lpfc_sli_brdrestart(phba);
	lpfc_online(phba);
	lpfc_unblock_mgmt_io(phba);
}

/**
 * lpfc_sli_driver_resource_setup - Setup driver internal resources for SLI3 dev.
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to set up the driver internal resources specific to
 * support the SLI-3 HBA device it attached to.
 *
 * Return codes
 * 	0 - sucessful
 * 	other values - error
 **/
static int
lpfc_sli_driver_resource_setup(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;

	/*
	 * Initialize timers used by driver
	 */

	/* Heartbeat timer */
	init_timer(&phba->hb_tmofunc);
	phba->hb_tmofunc.function = lpfc_hb_timeout;
	phba->hb_tmofunc.data = (unsigned long)phba;

	psli = &phba->sli;
	/* MBOX heartbeat timer */
	init_timer(&psli->mbox_tmo);
	psli->mbox_tmo.function = lpfc_mbox_timeout;
	psli->mbox_tmo.data = (unsigned long) phba;
	/* FCP polling mode timer */
	init_timer(&phba->fcp_poll_timer);
	phba->fcp_poll_timer.function = lpfc_poll_timeout;
	phba->fcp_poll_timer.data = (unsigned long) phba;
	/* Fabric block timer */
	init_timer(&phba->fabric_block_timer);
	phba->fabric_block_timer.function = lpfc_fabric_block_timeout;
	phba->fabric_block_timer.data = (unsigned long) phba;
	/* EA polling mode timer */
	init_timer(&phba->eratt_poll);
	phba->eratt_poll.function = lpfc_poll_eratt;
	phba->eratt_poll.data = (unsigned long) phba;

	/* Host attention work mask setup */
	phba->work_ha_mask = (HA_ERATT | HA_MBATT | HA_LATT);
	phba->work_ha_mask |= (HA_RXMASK << (LPFC_ELS_RING * 4));

	/* Get all the module params for configuring this host */
	lpfc_get_cfgparam(phba);
	/*
	 * Since the sg_tablesize is module parameter, the sg_dma_buf_size
	 * used to create the sg_dma_buf_pool must be dynamically calculated.
	 * 2 segments are added since the IOCB needs a command and response bde.
	 */
	phba->cfg_sg_dma_buf_size = sizeof(struct fcp_cmnd) +
		sizeof(struct fcp_rsp) +
			((phba->cfg_sg_seg_cnt + 2) * sizeof(struct ulp_bde64));

	if (phba->cfg_enable_bg) {
		phba->cfg_sg_seg_cnt = LPFC_MAX_SG_SEG_CNT;
		phba->cfg_sg_dma_buf_size +=
			phba->cfg_prot_sg_seg_cnt * sizeof(struct ulp_bde64);
	}

	/* Also reinitialize the host templates with new values. */
	lpfc_vport_template.sg_tablesize = phba->cfg_sg_seg_cnt;
	lpfc_template.sg_tablesize = phba->cfg_sg_seg_cnt;

	phba->max_vpi = LPFC_MAX_VPI;
	/* This will be set to correct value after config_port mbox */
	phba->max_vports = 0;

	/*
	 * Initialize the SLI Layer to run with lpfc HBAs.
	 */
	lpfc_sli_setup(phba);
	lpfc_sli_queue_setup(phba);

	/* Allocate device driver memory */
	if (lpfc_mem_alloc(phba, BPL_ALIGN_SZ))
		return -ENOMEM;

	return 0;
}

/**
 * lpfc_sli_driver_resource_unset - Unset drvr internal resources for SLI3 dev
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to unset the driver internal resources set up
 * specific for supporting the SLI-3 HBA device it attached to.
 **/
static void
lpfc_sli_driver_resource_unset(struct lpfc_hba *phba)
{
	/* Free device driver memory allocated */
	lpfc_mem_free_all(phba);

	return;
}

/**
 * lpfc_sli4_driver_resource_setup - Setup drvr internal resources for SLI4 dev
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to set up the driver internal resources specific to
 * support the SLI-4 HBA device it attached to.
 *
 * Return codes
 * 	0 - sucessful
 * 	other values - error
 **/
static int
lpfc_sli4_driver_resource_setup(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	int rc;
	int i, hbq_count;

	/* Before proceed, wait for POST done and device ready */
	rc = lpfc_sli4_post_status_check(phba);
	if (rc)
		return -ENODEV;

	/*
	 * Initialize timers used by driver
	 */

	/* Heartbeat timer */
	init_timer(&phba->hb_tmofunc);
	phba->hb_tmofunc.function = lpfc_hb_timeout;
	phba->hb_tmofunc.data = (unsigned long)phba;

	psli = &phba->sli;
	/* MBOX heartbeat timer */
	init_timer(&psli->mbox_tmo);
	psli->mbox_tmo.function = lpfc_mbox_timeout;
	psli->mbox_tmo.data = (unsigned long) phba;
	/* Fabric block timer */
	init_timer(&phba->fabric_block_timer);
	phba->fabric_block_timer.function = lpfc_fabric_block_timeout;
	phba->fabric_block_timer.data = (unsigned long) phba;
	/* EA polling mode timer */
	init_timer(&phba->eratt_poll);
	phba->eratt_poll.function = lpfc_poll_eratt;
	phba->eratt_poll.data = (unsigned long) phba;
	/*
	 * We need to do a READ_CONFIG mailbox command here before
	 * calling lpfc_get_cfgparam. For VFs this will report the
	 * MAX_XRI, MAX_VPI, MAX_RPI, MAX_IOCB, and MAX_VFI settings.
	 * All of the resources allocated
	 * for this Port are tied to these values.
	 */
	/* Get all the module params for configuring this host */
	lpfc_get_cfgparam(phba);
	phba->max_vpi = LPFC_MAX_VPI;
	/* This will be set to correct value after the read_config mbox */
	phba->max_vports = 0;

	/* Program the default value of vlan_id and fc_map */
	phba->valid_vlan = 0;
	phba->fc_map[0] = LPFC_FCOE_FCF_MAP0;
	phba->fc_map[1] = LPFC_FCOE_FCF_MAP1;
	phba->fc_map[2] = LPFC_FCOE_FCF_MAP2;

	/*
	 * Since the sg_tablesize is module parameter, the sg_dma_buf_size
	 * used to create the sg_dma_buf_pool must be dynamically calculated.
	 * 2 segments are added since the IOCB needs a command and response bde.
	 * To insure that the scsi sgl does not cross a 4k page boundary only
	 * sgl sizes of 1k, 2k, 4k, and 8k are supported.
	 * Table of sgl sizes and seg_cnt:
	 * sgl size, 	sg_seg_cnt	total seg
	 * 1k		50		52
	 * 2k		114		116
	 * 4k		242		244
	 * 8k		498		500
	 * cmd(32) + rsp(160) + (52 * sizeof(sli4_sge)) = 1024
	 * cmd(32) + rsp(160) + (116 * sizeof(sli4_sge)) = 2048
	 * cmd(32) + rsp(160) + (244 * sizeof(sli4_sge)) = 4096
	 * cmd(32) + rsp(160) + (500 * sizeof(sli4_sge)) = 8192
	 */
	if (phba->cfg_sg_seg_cnt <= LPFC_DEFAULT_SG_SEG_CNT)
		phba->cfg_sg_seg_cnt = 50;
	else if (phba->cfg_sg_seg_cnt <= 114)
		phba->cfg_sg_seg_cnt = 114;
	else if (phba->cfg_sg_seg_cnt <= 242)
		phba->cfg_sg_seg_cnt = 242;
	else
		phba->cfg_sg_seg_cnt = 498;

	phba->cfg_sg_dma_buf_size = sizeof(struct fcp_cmnd)
					+ sizeof(struct fcp_rsp);
	phba->cfg_sg_dma_buf_size +=
		((phba->cfg_sg_seg_cnt + 2) * sizeof(struct sli4_sge));

	/* Initialize buffer queue management fields */
	hbq_count = lpfc_sli_hbq_count();
	for (i = 0; i < hbq_count; ++i)
		INIT_LIST_HEAD(&phba->hbqs[i].hbq_buffer_list);
	INIT_LIST_HEAD(&phba->rb_pend_list);
	phba->hbqs[LPFC_ELS_HBQ].hbq_alloc_buffer = lpfc_sli4_rb_alloc;
	phba->hbqs[LPFC_ELS_HBQ].hbq_free_buffer = lpfc_sli4_rb_free;

	/*
	 * Initialize the SLI Layer to run with lpfc SLI4 HBAs.
	 */
	/* Initialize the Abort scsi buffer list used by driver */
	spin_lock_init(&phba->sli4_hba.abts_scsi_buf_list_lock);
	INIT_LIST_HEAD(&phba->sli4_hba.lpfc_abts_scsi_buf_list);
	/* This abort list used by worker thread */
	spin_lock_init(&phba->sli4_hba.abts_sgl_list_lock);

	/*
	 * Initialize dirver internal slow-path work queues
	 */

	/* Driver internel slow-path CQ Event pool */
	INIT_LIST_HEAD(&phba->sli4_hba.sp_cqe_event_pool);
	/* Response IOCB work queue list */
	INIT_LIST_HEAD(&phba->sli4_hba.sp_rspiocb_work_queue);
	/* Asynchronous event CQ Event work queue list */
	INIT_LIST_HEAD(&phba->sli4_hba.sp_asynce_work_queue);
	/* Fast-path XRI aborted CQ Event work queue list */
	INIT_LIST_HEAD(&phba->sli4_hba.sp_fcp_xri_aborted_work_queue);
	/* Slow-path XRI aborted CQ Event work queue list */
	INIT_LIST_HEAD(&phba->sli4_hba.sp_els_xri_aborted_work_queue);
	/* Receive queue CQ Event work queue list */
	INIT_LIST_HEAD(&phba->sli4_hba.sp_unsol_work_queue);

	/* Initialize the driver internal SLI layer lists. */
	lpfc_sli_setup(phba);
	lpfc_sli_queue_setup(phba);

	/* Allocate device driver memory */
	rc = lpfc_mem_alloc(phba, SGL_ALIGN_SZ);
	if (rc)
		return -ENOMEM;

	/* Create the bootstrap mailbox command */
	rc = lpfc_create_bootstrap_mbox(phba);
	if (unlikely(rc))
		goto out_free_mem;

	/* Set up the host's endian order with the device. */
	rc = lpfc_setup_endian_order(phba);
	if (unlikely(rc))
		goto out_free_bsmbx;

	/* Set up the hba's configuration parameters. */
	rc = lpfc_sli4_read_config(phba);
	if (unlikely(rc))
		goto out_free_bsmbx;

	/* Perform a function reset */
	rc = lpfc_pci_function_reset(phba);
	if (unlikely(rc))
		goto out_free_bsmbx;

	/* Create all the SLI4 queues */
	rc = lpfc_sli4_queue_create(phba);
	if (rc)
		goto out_free_bsmbx;

	/* Create driver internal CQE event pool */
	rc = lpfc_sli4_cq_event_pool_create(phba);
	if (rc)
		goto out_destroy_queue;

	/* Initialize and populate the iocb list per host */
	rc = lpfc_init_sgl_list(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1400 Failed to initialize sgl list.\n");
		goto out_destroy_cq_event_pool;
	}
	rc = lpfc_init_active_sgl_array(phba);
	if (rc) {
		lpfc_printf_log*****, KERN_ERR, LOG_INIT,
				"1430 Failed to initialize sgl list.\n"****	goto out_free_sgl_nux ;
	}

	rc = *****sli4_f th_rpi_hdrs************************************************************
 * This fi2e is part of the Emulerpi headersDevice Driver for      activ     Fibre C****->t Bushba.fcp_eq_hdl = kzalloc((sizeof(structel Hos       *
 ) ** Thi          cfg        count), GFP_****EL********!                      *
 *********************************************
 * This257rights re.emulate memory for fast-path "* Thisper-EQ handle *****evice Driver for removepters.   ulex.                 msix_entries* www.emulex.com           terms ofy                                 eqnions Copyright (C) 2004-2005 Christop terms of ve             *
 *                                       3                         *
 msi-xogram isinterrupt vector s of veevice Driver for             *
 Fibre Cheturn rc;

 INCLUDING ANY IMPL:
	k    *****05 Christoph Hellwig  ;RCHAN it and/or   *
:
******t Bus it and/or   *
********re trademarks of EmARE     rademarks of EmCEPT TO THE EXTEN   *
 * H DISCLAIMERS   *
 * CEPT TO THE destroy_cq_event_pooCH DISCLAt Busr  *
 * more icense fal Public License foqueueARE      *
 *ING  an be found in the f     bsmbxARE     cense fobootstrap_mboxCEPT TO THE EXTENmemARE     mem     *********RANTY OF ME}

/**
 *el Host Busdriver_resource_unset - Ucludedrvr TATIOnal dev.h>
#s  *
 SLI4 dev

#i@****: poTATIOrt o**** hba data       ure.
 /

#iThis routine is invokpart oncludethe nux/bly.h>
#include <linuxludeup

#ispecific  *
 supportingncludSLI-4 HBApingice it attachpart terr*/
static void
nclude <linux/blkdev.h>
#includ             #inc******
{
	              f_conn   *
  *#include ", *next
#include ";

	/* unregisinuxdefault FCFI fromnclud#inc*/RE      *
 *fcfi_lude *******FOR A fcf.lpfc)h"
#incFreencludeli.h"
#inR table4.h"
#include* DISCLAdflt"lpf********#include "               dNDITIONS, RTATIONS AND          *
 4.h"
TNESS FOR A PARTICULAis program is#include "lpfc_crtn.h"
#include "* This prowork ING  softwarversion.h"

char *_dump_bufR PURPOSE, OR#include "lpfc.h"
#inclu    *
 * EMU4.h"
#include " DISCLAIMED, EXCEPT TO E      *
 * DISCLAIMEXCEPT TO #include "lpfcELSex Linux 4.h"
#inclIMERS ARE HELD *
 * TO   See the GNU General Publi#include "lpfcSCSIex Limanagemente; youersion.h"

char *_dump_bufails, csi_psb******hba *);
static voimappING  versio* included with this package.  #include "lpfccompletionif;
unsEQ *
 *  ore 4.h"
#include "r  *
 * mrelease_alfc_sli4_queue_c a copy of which can be found in t
#incReluderuct #incFCoE funca *);.h"
#inclpci_pfc_freekdeve lpfc_hba *);
static vo      de "onhba_model_d             *
 **************tstrap_mbox(struc Layerpfc_crtnwith;
static hba *);
sta*********);
static vorder(struct lpurr lpf#incectnclude "lpfcist_for_each   *
 _safe(fc_hw4.h"
#iclude "lpfc_hw. * T&de "lpfc_
#inclrec*
 * ,inux )
	on.h"

#include "ct lpRANTY *******/

#includ Adapapi_clude_setup - Sinux/of th api fucna *);jumpnclude.h>
#includThe
#inc        *
 which t>
#icall<linbenux/executedterr @dev_grppfc_hb#incPCI-Dude <sgroup numberterrupt.h>
#include <sets lpflpfc_sde <s***
vport.face APIlpfc_free_ol_destroy(strin
#incl);
statterrupt.RANTY s: 0 - success, -ENODEV - fail/interr*/
intdevicet lpfc_hba *);
statist.h>
#include <scsi/s, uint8_tmplapfc_csi_trwitch (zation p****case LPFC_PCI_DEV_LP:
	       nclude <_down_postnnel Hoscture.
 *
 * _s3e Dra data strucftwar_erat This rout prior to iLPFC initializatiostop_ <linnel Hostnd. It rLPFC inbreak; @phba: pointer to lOCc hba data structure.
 *
 * This routine will do LP4C initialization prior to issuing the CONFIG_PORTBA for preparingand. It retrieves the revisiBA foformatio_scsi.hc hb***************************************
 * This fi1 Invalid);

statiplate =t scs: 0x%x\n" * Thization pe DrRANTY Otic DEFRESTART - re}*********0*******/

#includetatiinux/blkdev.h>
#iphase1 - Pc;
	LPtatice <linux/pci.h>
#include (strucinclude <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/kthread.linux/nclude <linux/pci.h>
#include <befor "lpfstruplate =lock.h>
#dev.h>
#key nlocolude <lilpfc_sli4lude <scsi/scsi.h>
#inclusport_temp codes
 *	e = NULessful (!pother valc_hb- errorclude <scsi/sindex);

hba->vpd;
	int i = 0, rc;
	Lst.h>
#include <scsi/scsi_t/*
	 * D <linuude <linuxcommon;

	_cq_(strrevisions_CMD/
	atomiEM;
	(ost_s4(sast *
 * m Port, 0fig_spin_lock
/**
y) {
			hba*) lct lpfc_I the Emulendlpsli4_queue_d32_t*) l{
		i32_t *) licensed;

			izeo *) lct lp***
_LIST_HEADy) {
			revisba);
;	init_key = 0;
		}

		lpuf_dead_nv(ph Adapwait with *
 *mset((chaait_4_mlo_m_q= 0; i < 56; i += sclud(mb-if;
unsiealude "cludkernel thrvarR
		irRDnvp.rsvd3, 0,
			sizeof (r*)mp.rsvvp.rsvd3));
		memcpy((ct lp bufferct lpfused bymb;
	MAIincluX_POIO+)
				*ptext = cpu_to_be32t lpfbufead_next);
		init_key = 0;
		}

		lpic int lpfG_MBOX,
vp.rsvd3));
		memcpy((cfabrturnocbct lpfc_hbnit_key = 0;
		}

		lpARM, "_				ead_nv(p; i < 56; i += st lpf

	pav_buf(sLL);

versionit_key = 0;
		}

		lpelsbufStatus);
			mempoolFCFnt lpfc_ *);rec	"mbxStatus x%x\n",
					mb->mbxCtruct lpfc_hba *4_cq_event_lpfc_vpd_t *vp = &phba->vpd;
	int i = 0, rc;
	2PFC_MBOX2ey = 1;b;
	MAILBOX_t *mb;
	char *lpfc_vpd_data = NULL;
	uint16_t offset = 0;
	static char licensed[56] =
		    "key unlock for use with gnu public licafinux/de only\0";
	static int init_key = 1;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (! pmb) {
		phba->li nk_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	mb = &pmb->u.mb;
	ph2st.h>
#include <scsi/scsi_tint_HBA_Etatus);Star= 1;
rsvd3, licensed,
Dnvp.ris h * Tadapterhba_moet((char*)er_ensed,
= kensed,_run(pfc_hbo_ lpflude " * Thi	  "*****r must %d"lude "lpbrd_no********IS**** FOR A r must be 1 s)******HBA_E = PTR revision fields.
	 */
	iig_port_pre, phba-D WARRANTY Opfc_vpd_t *vp = &pnclud.varRDnvp.portname,
		       sizencludeb;
	MAILBOX_t *mb;
	char *lpfc_vpd_data = NULL;
	uint16_t offset = 0;
	static char licensed[56] =
		    "kh>
#include <linux/pci.h>
#include <linux/x(phbanit,_template =static int init_key = 1;include <linux/ctyp#include <scsnit,/scsi.h>
#include <scsi/scsi_deviceIT,
				"0440 Adapter faileda->link_state = LPFC_INIT_MBX Stopvd3, licr mustcensed,
			 e the dr the == 0) {
		vp->rev.rBit*****/

#includ     , mb->mbx -lude "					"mbx*lpfc_vpd_data = NULL;
	uint16_t offset = 0;
	static char licensed[56] =
		    "k    nclude <lin's IOCBl_freeandpfc_crtnclude <scsi/scsi_device;
	memcpy(vp->st.h>
#include <scsi/scsi_transport_fc.				q *ev;
	lude "l= NULL,vp->rev.clud = mb->tatu32_t *) licrqed;

			for (i = 0ost_s3(struct lpfc_hba *ph->rev.smRev,1FwNadRev.s * Thi nitializatio, mb->mbxphba);
******X,
	del(&->rev.smRev->ad_nv(phon.h"

->rev.smRevBit =FOR A totalRev;
					s--a *phb32_t utext+ev = mb->un.varRdRev.uq_event_pool_create(struct lpfmcpy(vp->revA        wNamf the EmuleFwName, (char*) mb->un.varRdRev.sli1FwName, 16);
	vp->rev.sli2FwRev = mb->un.varRdRev.sli2         >rev.postzmemcpy((cemcpy(vp->rev

#inli2FwNamey unlock f>rev.tagdestroyaccordinglhar *GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEMvarRdRev.feaLeort_prep - Perform lpfc  pmb, mb- Portiun.varRdRev.biuRev;
	vp->rev.smRev = mb->;
	init16_BA(ptag.varRt itatus);
			mempoolwNampopul    l);
					"mbxSper


	/. m_pool);
			return -ERESTARev.fcphHigh = v.un *
 (i = 0; i <A(phba->pci; i++.varRd->rev.smRev = ww.emule.com               arRdRions Copyright (C*****->rev.smRev == mb->.varRd	*****k(******** "%s: onltn.h"
#inclu%d));

s ofogram isexpecb, MBX_ Port. Unloa if en 9, wDevi * Thi_uct l__, i,: poin>revkey = CNTBit =river for      arRdR pmbre C	un.vannel Host BRev.s_un.va*******n.varRdRev.fcphL, offsmbxCm= 0D);
		r mb->un.varRdRev.fcphLrc = lpfc_sli_issue_mb_IDR the sli featurIOTAG.ogram is			lpfc_printf_log(ph ERN_INFOVPD not present on adapter, "
	c_vpd_data)
	s publixrimbxCmdNO_XRI->re>rev.smFwRev = mb->un.varRdRev.undRev.faddHigh;
	vp->rev.fcph,;
	vp->rev.fcphHigh = fcphLow;
	vp->rev.feaLevelH++.var->un.varRdRev.feaLevelHigh;
	vp->ntf_log(phba, K THE EXTENarRdRH DISCLAIMERS_VPORT_TEAstruct lport_prep(stMEMarRdRev.sli1FwRev;
	me   *
 * rev.sli1x Linux Dhar*) mb->un.varRdRev.sli1FwName, 16);
	vp->rev.sli2FwRev = mb->un.varRdRev.sli2FwRev;
	memcpy(vp-truct lpfwName, (char *) mb->un.varRdRev.sli2FwNNU Generat.h>
#include <scsi/scsi_transport_fc.sglq *ool).smRev = mb->un.	retuev.smRev;
	vp	ey = 0;
			retub->un.varRt hanne + D>rev.smFwRev = mb->un.varRdRev.un.smFwsplicephba, KERN_ERR*);
static int  *
 * , &pletion handl->un.varRdRev.feaLevelHigh;
	vp->rest_s3(struct lpfc_hba *ph	return 0;,fc_vRev.endecRev;
pletion hmb->un.varRdRev.fcphHi	return 0;v.fcphLow =c_freeG_MB*********ompleti event
virtvice. If the mphygnedStatus);ce. If thefcphLow;
	vstructurep->revpletivelHigh = mbhannel Host Bus it andalevent_page                              *
 * Copyright (C) 2004-2009SLI * Th"2005 Unlude to dede "lpfc_t intude "lHBA: %x", rc((uintruct lpfc_hba *);
static inels @pmbeate_boool_create(struct lpfcrks of Em= MBX_velLow;
	vp-init_ufstrutrack ensor_buf(sXRI *lpfc_vpd_data = NULL;
	uint16_t offset = 0;
	static char licensed[56] =
		    "k         ;
	memcpy(vp-supportfc_sl, (char *.h>
#irt = 1will holdmb, MBreturn 0;'ux/dmasupportIO *lpfli3_options |= LPFC_SLI3ensor_support = mbxStatus);
		mempool_free( pmb.comhe dmule=x com               ool);
the dmule*=nse as publishedmax_    paramommanxr24],
 data structure.
 * @pmqto the dtear =ucceo out_free_ons Copyright (C) 2004-2005 Christop When this command co
stat,
				      mb-log(phba, KERN_ERR, LOG_INrademarks of Emrev.sli1hba->temphatsensory mailbox = 0;
	mempool_free(pmboxq, phba->mbox_mem_pool);
	return;
}

/**
 * lpfc_dump_wakeup_parwalkcensougpfc_re; you;

	mailbox coq     *
 L;
	}nd2FwRev_cq_ot inemb;
	char *lpfch>
#iis judata plic s@phbCCESS) nowar *) mb->un.varRdRev.sli2FwN ARE HELD *t.h>
#include <scsi/scsi_tuct lpfc_hba *);
static intBA. This functioUCCESS)
		phba->temp_s;
		offset Low;
	vp->rev.postKernRe->un.varDmp.word_cnt;
	} while (mb->un.varDmp.word_cnt && offset < DMP_VPD_SIZE);
	lpfc_pli feature level is less then 9, we msginit,tear down all RPIs atrin->un.vaIs on link down if NPIV
	 * is enabled.
	 */
	if (vp->rev.feaLevelHigh < 9)
		phba->sli3_options |= LPFC_SLI3_free_mbox:
	mempool_free(pmb, phba->mbox_mem_pool);
	return 0;
}

/**rWords[24]( pmb,ls
 * _cntfig OptionROMVennel Host BusgetStatu(phba-nata + off SLI layer to reset the HBA INFOid
lpfc_config	"2400);
		return;
	}

	prels %dog(phba, KOptionROMVe>lev						sizeof (phba->RandomData)2_t p/* Get adapte/VFhba_monit_key = 0;
		}

		lpstructure.
 * @pmboxq:"0324 Config Port initialistructure.
 * abtsStatus =="error, mbxCSanity check on0;
	sli4_queue_d			 sf 
char *_dump_buf_mand for getting
 *  <=>OptionROMVent support flag to 0.
 **/
static void
lpfc_config    62 No room leftic void lpand        ion:ogram ising
 * =%d,>OptionR=%dg(phba, Kdump mailbox command for getting
 * 
 * lpfc_config_port_translate the versre C/* = (struct        *
 cv_buf(sand call. It peestroy(strf (pmboxq->u.mb.mbxStatus == MBX_mplette, the rfree_mbox;

	do {
		etion h *n the
 * port * Ths Copyright ( Option rom version of the tatus == MBX_S: post IOCB buffers, enable appropriate host interru401e is part o                 *
 uf(sgram is phba->pport;
	structofx com
}

/**
 * lpfc_config_port_(struct lpfc_hba *phba)
{Keeool);
 ring timinp_sehe= phba->pport;
	struct Scsi_Host *shost =t lpfonROmaxport(vll internal resource and state setups->OptionROMVersmp_state = HBA_NORMAL_TEMd%d%c%nfig fc_hba *);
static int lpfc_create_port(vport);
	LPFC_MBOXQ_t *pmb;
	M "
				BOX_tpin_unlock_irq(&phbORMAL_TEMP;
ions Copyright ( Option rom version of the H lpfc_create_b: post IOCB buffers, enable appropriate host interruptLL EXPRESa->hbalock);
	/*
	 * Ifid lp Config port completed correctly the HBA is not

	/* Get login parameters fosuccessfulf (pmboxq->u.mb.mbxStatus == MBX_SUCd any more.
	 */
	if (pMP_VPD_SIZE, GFPOptionROMVerif (!lpfc_inter to l	goto out_free_mbox;

	do {
		(pmbdump_mem(phba, pmb, of(struct lpfcION_VPD);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_sgl);

		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_INFO, LOG	 * over heated river for      memr, "
				onous event
f (mb->un.varDid lpfc_declude->un.v*********ba->mbox_mem_porv_parm));
	lpfrDmp.wor->mbxStatus);ly,
 * it will p.word_cnt = 0;
		}
		/* dump mem may retXRIDeviro when finished or we got a
		 * mailbox error, either irt, sizef (struct servLL);_typ *
 GEN_BUFF_TYPE- offe. If the mailbpfc_mbufmand .emulee devi0 pointerand returns succphys);
	kfree(mpe.u.wwION_VPD);
		rif (phba->cfg_soft_wwnn)
		u64_to_wwn(phba->cfg_soft_wwnn,
			mandvport->fc_sparam.nodeName.u.wwn);
	if (phba->cfg_soft_wwpn)
		u64_to_wwn(phba->cfg_trin=ointer to l      mp->mem_hostructures with c_nod poinBPL_SIZElpfc_a)
{c_hbtear ordistis(rc != MBndomr bext++SGLlist( *
 tee_sgl_t == 0)
			break;
		if (mb->un.varDmp.word_taichronous event
 * m - offsetba data structure.
 *
 * Tsi_Host *shost = lpfc_shost_from_v[i]new wwn. */
	ill set internal async event suppoE - offset;
		lpfc_sli_pcimem_bcopy(((uint_t *)mb) + DMP_RSP_OF******uct lpfc_hba *);
static int lpfc_create_boo  See the GNU General Publiet,
				      mb->un.varDmp.wordt Bus Adapters.   PFC_ * T ';
	   *
 * E lpfc_vpist(stmp_sehe  <lilpfc_vpd_data = NULL;
	uint16_t offset = 0;
	static char licensed[56] =
		    "k
 * T	phba->Seritempndoms
				  mple#inccons"lpfnt *);
sctype.h>

;
static slock. .h>
#include mple
 * s a PAGEfc_noialNumber[i] =
				    (chUCCE@phba 1;

mpleumber[i] =
odulo 64)((uicontv.sm*
 * EMUmpleNo *) ln haun.vld here becaus storay usanv.postKernetc.
9)
				phba-4_cqedbox(phde "lprobe orc_mbufonlde <whe - 1TIONS Aar)((unohar)(ecmpl(dd;
	cclude <linuxs ref the Emunux/ctypplate OG_INIT,
				"0439 Adapter failed to init,      m -	   availude mand cmplex%x "EIO -.wwn)mto iox	/* dump mepfc_hbae NULL;
sfulNPIV
	ba_index);

t Bus Adapters.    bxStatus);
		mempool_free( pmbor confiler flong
 * r *)&mb->tersEL);
	_transport_fc.ters.   *ters.  			init_key = 0;
		}

		lpstructure.
 * ters.  e CONFIG_POR_CMDSProdevice(j -	phbbitmask range->mbodiscovery..wwn)p->reCESS) _CMDSisMBX_POi);

enc0x61tween P;
	;
	cbhba:+ 1.) {
		i_pool);
	 r dump mailbox command for getttersi+1))

            ailbox command for getting
	phb- 1or ma, ph = ((_pool);
	) + BITS_PER_LONG);

) /onfig.lmt;

	/phba->mbox_mem_poog.maxH to goto out_fa->lmt* * This unsignedmb, pt lpfc     s Copyright (C) 2004-2005 Christop and Descon translate the vercfg_hb/* Cfc_mbuf_freecreatnd/or   **************!ters.  = vport;
	if (lpfc_sli_issue_mbox(phba, pmMBOX |ate host interr0391 Eun.vadurc_pr	phb
 * Top to ionevice Drt *, uint8_t *);
static int l	 to 1;p(struct l WARRANTY OF ME*****/

#include <liba->cfg_link_srg = (struct p		phba->SerialNumber[i] =d;

	/* word 7 contain option rom version */
	prog_id_word = pmboxq->u.mb.un.varWords[7];
 sif Ne 4KBialNumber[i] =
		spinlmb = mestat down tor(uinhemate 	        (j <= 9& LMT_1
		&& !(php thede <lmb = me *
  1;

	hba->Ss, KERG, LOG_L_port_namgloballCmd xby	lpfc_printf_log(phba, KEs:md x%xAte =ioid lpfdrcommNULL;
s
				nk_smb->de <linuxFT_HByE_IDR(lpfc_hba_turn -EIO;
	}

	/* Chdevice.h>
#ba->cfg_link_spt.h>
#include <scsi/scsi_t>mbox_mem_poolimit,statilink_the m	return -EIO;
	dma&pmb-dringa	return -EIO;
	}

	/* Check if the  */
	if (ue_depth =
			(mb->un.varRdConfig.max_xri + 1) -
					lpfc_sli4_get_els_iocb_cnt(phba);

	phbrev.smFwRev = mb->un.varRdRev.unli->ring[psli-ue_depth =
			(mb-cluderpf(ph->un.varRdRev.feaLevelHigh;
	vp->re/* Reses bo8_t) haialN	if (edi_transectlfor ths boincrueue_dt) 0ueue_dep poinRPI_HDR_COUNT);

UCCESca_queuort *vposmem_c_pre = L_CMDSb, MB->hbalwD_NVPAul_sli(phba)the mat ad <liphba->cfms a(li->ring[psli->+ ((phba->sli_rev != 3)
	)) >Y */
	if (on translatv;
	vp->/* ReseFirdatam_cmpl - dumprotocolba->Seriard: "
ort *vpomessahis b_CMDS8_t)  != MBialN0Gb)DMA-mappeame, (cher[i] =
	n Opis 4K all_desphba->cfdringadgoto out_free_mbox;

	do {
		dringaions Copyright (C) 200a, KERNconfig_msi(phba, pdringaame.u.wwndmay(&vpo_cot) 0nsed;

			pcidev->dev, phba->M poini_reTEMPLATer[i] , phba->M&command urns, phba->MRR, LOG_MBOX,
					"0352       = vporED_10G)
	 se
		spriver erran rear)
		pslire Cde_name->mbox_mem_po_u64(vpormb.mbxCommand,
		== LINK_SIS_ALIGNEDck);
	/* atus)ze ERATT handling flag *ool);
			return -EIO;
		}
	}

	spin_lobxCmd x%a *phba)
{S, ph
				phba->Serilude  *
 cleanupshost)hba_moED_10G)
	 e, the respo               PEED_1G)ons Copyright (C) 200PEED_1G)pts */
	status = readl(phba HC_LAINT->BX_SUCCESr)
		psliHC_R1INT_len =mb->u.mb.mbxCommand,
		>num_rings >t inba_queue_1int32_t *) lic = mb->un.varRdRev.unHC_R1INT_hba);phba)next_ring].cmdringaddr)
		psorts(shost) = pm_rings > 

	/* If no serial number ink_ste(phba);

	/* Reses bogaddr)
	/
		lpfc_prtel(sse
			e-hba->See = L   (charueue_dO;
	}
subsequi++;atusmbox(phba, pmb
 * ingsphba->cfxt_ring].cmdringaddr)
	 +		statu->sli_rev != 	psli->ring[psli->next_ring].flag |=*********ck if the	status = readl(ph:x comtus = readl(phx, mbxStatus x%x\n",mb->u.mb.mbxCommand,
					pm  (psli- mailbox= ~HBA_ERATT);HZ * timeoudringa &vport->a, KERNtmofunc, jse
		s*****/

#include <li it and/or   *
 - Rit andist 	phba->SerialNumber[i] =_word_vpd_data = NULL;
	uint16_t offset = 0;
	static char licensed[56] =
		    "k it andist mbox(phbasue_mbox(h"
#incldRev.
	pmb = meuffers f
#include <presumba->HCrconfirVENT;fc_hba      mpleo aut));
um != MBARM, "
oo mess loginuto */is_INIpar_SUCCEhavphba->HCra->Seri * phb it anl(str*/
csi_device.h>
# DISCLAIMED, EXCt.h>
#include <scsi/scsi_transport_fc.}

	/* Check if #includeiffies + Hmailbox command.
 *
 * Thi {
			lpf_printf_log(decRev;
	vp->r_INT))
		status &= ~(HC_R0mb->un.varRdRev.fcphHioll & DISABLE_spee up heart beat (HB) timer */
	mod_timem_rings > 2)decRev;
		}
	} el+ HZ * LPFC_HBHC_R1INT_ENA;
	eturns successfulHC_R1INT_ENA;
		phba->cfg_link_sp((uint8_up ring-0 (ELS) timer */e_depth =
			(mb->un.varRdConfig.max_xr;a->hbaloclDesc);

	if ((phba->cfgc_nod.com   csi/sc);

	if ((phba->cfg_box_mem_pool);
		rctur{
		lrg = (structe <linu#include <linux/inc hba c_printf_lo @pdevude <linux/idpcilloc(phbem_pool);
	return;
}

/**
 * lpfc_dump_wakeup_param_cmpl - dump memoAdapter failed to init, m !(ph#include <. I ' ';
ers, etc.
mbox			mb->mbx,_log(phba reb->un.vant8_t) (j PCIatus x%x\n",
				mb->ml(phbe_transport_temp0439 AdaptLicens <linux/id*lpfc_failed to init,/* flLINK_C_HBA_ERROR;
		returt.h>
#include <scdeviceN_ERR, LO;
		if (strudev *			"dev->device))
		me <scsi/status);
	struct lpfc_vport #incrupts */
	Set up r_ENA;
	if (psli->num_rings > 0****ons Copyright (C) 2004-20= vporzatierr(&			"od_time"/* dump mem may retpfc_vport_evice Dr>last_complet->HCregadetlush */
			/* ll pending in}
	}
	/* MBOX buffer wiStatus xet_ldev_pool);
sel_dpeedunrc != oarst receivSet up rinhe FW 
	    ||->veinstance(******** ((rc != MBX_<mb->mbxSTNESS FOR nd, mb->mbxS	pmb->mbox_mutexicensed;

			ct32_t *p
				"0324 Config Port initiali failc_sliergned lRANTY O_mem_pf_log(phba, KERN_ERFwRevev.sli1rrupt enable conditions */*);
smbxCmd x%x "
		mb->un.varRdRev.sli1FwName, 16);
	vp->rev.sli2FwRev = mb->un.varRdRev.sli2FwRev;
	memcpy();
	}

	/* Get Option rom ritel(0, phba->Hclude <scsi/scsi_deviceb, phba-py(&vp->sli3Feat, &mb->un.varRRc_hba MBX_POLL) !=ambox(b, MBX_NOWAIT);

	ifidlkdet an(&phba->ppoindexquires the FW to &vport->fc_nhba->last_		mempool_free(pmba->cfgs

	/*- Ca->cfhba, urnsicalEIO;
	n rom ssociinclu{
			apter*/
	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	lpfc_dump_wakeup_paraba->cf}
	}
n ROM version ;
	cs x%x\n", a_log(x "


	/**);
sitel(0xffffffff, phba->HAregaddr failed to init,s rouk_state = LPFC_HBA_ERROR;
		return -ENOMEMfailed "
			;
		if ((rc != MBX_SUCCESS) && (rc != Mv, pmb*.
 *
	return -ES lpfH * T *"
			tatus);
			mempoolic voivport = phba->pport;
 &phdto lpfFF_DEF_EDTOct llpfc_hba_rt->_prep(strucRAlpfc_hba *phba)al
	struct lpfcALport **vports;
	rbnt i;

	if (phRBlpfc_
	.
 *
 fc_mbufba->cfg <li
#include "lphe FW  - offsetatus x%x\n"== LINK_S.
 *
Config MSI p(struct 
	"
				fc_mbuf_

	/_de "r.
 *
( /* flc_hba *phb = me=  Return pfc_hbabugfsC_SLIe EmulING)
		lpf/* Pu= lpfc_config_aid lp

	/*trucmcpy(vp-plate =priv				"ude Set uciM;
	_drv!= N)) {
		atus x, flag 		memcpy(phba->wwpn, (char *)mcense fo"
				"tDense fOption ROM version status x%x\n", rc);
		mempool_free(pmb, phba->mbox_mem_pool);
	}

	return 0;
}

/**
 * lpfc_hba_down_prep cense form lpfc uninitializatiprog_ x%x\n", x "
vport_wormpl;
	pmb->vport = phba-+)
				lpfc_cset when
 * bringing down the SLI Layer.
 *
 * Retuet_loopbae <litatus);nup_discn ROM version , MBXs x%x\n", r
			j = (sport_work!= MBense foLOADING)
		lpq_event_pool_create(struchba->vbgc voidup Bext++guX_NO<linux/inuto */es(phr)((a *lpfc_vpd_data = NULL;
	uint16_t offset = 0;
	stat @flag :buf(ph_work_arbe(rc !=- Pertfc_hst_s3(struct etk);

	/rt_template *lpfc_transport_tem    lt_mp;
	LIST_Hl_free(pmEAD(compT_ENA *princter arraynclude <als->hbalock)s lpfc_vport es(phgc_prbg>mbox_mempl;
	pmb->vport = phba-
lpfc_hbort_prep - Perform lpfc codes
 *   0 - sucess.
free( pmbt ind%d%c%1free(f r set l_fr_H to &&c_mbuf liststruc= vport;
	if (lpfc_sli_issue_mboxool_free(p***
 * This f78 Re "lpfcc_prst_s3Gtruct
			j = (rt->fc_   0 lct levice Dr;
	pmlag &or (l_fr(ruct ,list_del(&m>cfg_(&phba->hbalock);
struc (i = 0; i < psli-p->lisa *phbINK_S_dump					!= N= vporwhile (xt_mp, D);
		r32_t *) licensedt in time b->un.var	t in time the port(v	(charBOX__&& (r     t intes Copyrigh,ext_mp, ld be postt in time the HBA iswnn)
		u64_to_wwn(pBLKGRDhba, pmb, MBX_ * phbaor		}
	}
s rout "on txcmplq as iator.
pg(phba, Kck);

	(1 <<init(&pri, t in time the Hd be  on txcmplq as __host_=ext_mp, l_iocbde_namet in time the c_noom the compleetionsumber[HIFT)ions list */el_iocbformatio		}	meme;
		p--OSTAT_LOCALe donpoint in time the etionsi->npring->txcmplq_cnt = 0;
	ERROR ucmpl(stru) {
			mert->fck);

		        *
 hex in evice Dn_lock_irring->txcmplq_cnt = 0;
		ssed,tn.h"
#inclut in time the =IOCBrt->ck);

		got a
lpfc_sli_cancel_i	}
	spin_unlock_ifHBA is either reset or DOA. HBA is resetit will NEVER complete.
		 */
		list_splice_init(&pring->txcmplq, &compleet when set
 * @phba: pointer to lpfpin_unlock_irq(&phba->hbalock);

		/* Cancel aifthe IOCBs from the completions list */
		lpfc_sli_ci	pmb->LI Layer.
 *
 *etions, IOSTAT_LOCAL_REJECT,
				      IifR_SLI_ABORTED);

		lpfc_sli_abort_iocb_ring(phba, pring);
		spin_lock_irq(&phba->hbalock);
	}
	spin_unlock_fc_hba_d->hbalock);

	return 0;
}
/**
 * lpfc_hba_down_post_s4 - Perform lpfc uninitialization after HBA reset
 * @phba: pointer to lpfc HBA data structure.
 *
 *ifis rog(phba,ck);

	igned long iflag*****/

#includ do Lhba->Otatic vPerform ne		phary);
		if ( * T 10));
		}
	}

 for pool_free(pmb, phba->mbox_mem_pool);
	}

	return 0;
}

/**
 * lpfc_hba_down_prep p_sgl_li_cq_>HCreg and */
 scsl_buf_list */
	spinlphba->mbolpfc_printf_lode <scsi/scsi_device* required for ;
		if ((rc != MBX_SUCCESS) && (rc*   0 - success.
 *transport_fc.
	 * Th *
 * ma->Seri_lock);
	/* atatus);G#includeli.h"
#e = LPFhba-Model Nam (phbaDescripfree_sgl_list(->vecturmes tstruort->fc_port;
ses tis
	i_buf_list_lost.
 |= LPFC_STOpfc_vwn_pomayc_sli chhe m afterctur with thpth so wCreguf *m Confided fo>HCra->HCrof can&abortphba->cf>pport->			f->vei = 0; i <= phba->mathe d

	/->f_list_loet_loopba    			&aborts);
	sp-ring->postMBX_SUCCE3_o*/
	ss &mb->u.SLI3_BG_ENABLEDi->npring = &psli-e devicports &&
phba->ag &attribicenseve(&phba->ms all intd foroll & DIS_SUC_FCP_RINuf_fT	mp = (status |= HC_r_each_buf_lb->un.varDl_listl
 *poll timespeed == Lsli->ring[psli->nqrestore(&phba->scsire C		pring->postbufq_cnt--;
			lpfc_mbuf_free(phb"0428fc_sgl_liid lpfca LMT_2Gbcmpl _xriBX_NOarvporlint lpfESTApt ad_lock{
		iflock);
	/* a.*
 * mwwpn,
	eoutEG_ADAPTER_EVEc_ratSLI4 routine fsub    gx(ph		statuation_ARRIVAb->msi_buf_l do Lvendstru
 *  (i = 0;k(&phba*
 * m_trans(a, phba f (rc !=SLI4 routine  value -  NEVER co&SLI4 routine LI_ABORT poinNL_VENDOR_IDR, LOG_INIT, "0435 Adapter DUMP			f****static voidupcide3);

stat & LINK_spaon */
	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	lpfc_dump_wakeup_paraey unlock fll pending r timeout haanup prde <ring*);
stat-3s & 0xf);
			if G_INIT,
				"0439 Adapter failed to init, mbxCmd x%x "
				"READ_REV, mbxStatus x%x\n
/**
 * lpfc_hb_;
		if ((rc != MBX_SUCCESS) && (rcY)
				mempooba->del_desc(phb bar0map_se {be u2ed by trWords[2, hb * Port;
	csi_ *ptprin pmb, phb== LINK_SPEEure.
Obtaine HBA-timer ush */
			 if MSI-04-2005

	listt = 0;
		lpfc_prinBA reset = lpfcort;
	rc = IG_PORT ed becausde <sDMA H to ler foforms al		for (ima->num(eout,it s_BIT_MASK(64)) ! mb-param.nby the worker thread after it has ta32n the event= 0;
		lpfc_prirequired becabus addres);

	Bar0izatiBar2ion after receive bbyte)) {
 requi = lby uct  _issing	/* Set up rinby te us_maplock_irdev.h>
#iock, read af(uinte used by t beat timeout colenons */
	spi
	/* Check for 2eart beat timeout conditions */
2spin_lorker thrve(&phba->pport->work_portevenure.
Maplue -SLIMre HBvd3, licailbual*phba;
	e value of rslim* lped bt beioremav.sli1Fwck for hearthe wosed by tt (C) 2004-2005 Chrqrestore(x_mem_poolc = lpfc_sli_is - os x%x\n",
			

		ba->ppo	/* dump/dma-maMmmand coevice Driver for->mbox_cmpport->woControl>virt, mpuint8events |= WORKER_HB_TMO;
	spin_unlocctrl_regsqrestore(&phba->pport->work_port__posthe worker thrt (C) 2004-2005 pointer to lpfc he is work to do */
	if (!tmo_posted)
		lpfc_worker_wake_up(phba);- 10));heartde "lpfcMULEX and SLI are tiouned br tha *phba)
{
	struct lpfc_vport e lp2t lpfc_hba *spin_unlock_ir2p."
					"failed, mbxCmd x%x, s x%x\n",
					pmseconSLI2us =Md,
					pmb->uheartssue DOWN_the atus);
			memsued,s Copyright (C) 2004-2005 Chthe HBA i->num_ri If configuphbaue_mbox(phba, pmthe HBA c_nods. At the time	lpfc_clea****et_loopback_the HBA e+ offsetb->context1;
		mli2red, , mbsue "ndler anbt =  outstanding state. Once the mailbox command comes bpcbhba, ndler >revmt =  outstanding state..
	 */
	spOnce the mailbox command comes bset thba,is
 * rehbqandie HBA every LPFC_HB_MBOX_INTERVAL (current 5)	d x%x DUMPoke ler r value ev;
	vp->r for the atus);
			m	 the driver shall set up h for the next* timeout tim      d, th
	oke the aCmd x%x DUMPoke the a_SUCCpts, IO**/
static void
lpc(DMP_VPD_SIZE, GFPoke the ac ++i= vpornsigned lo[i].oke e.u.wwneout
 phba: pointer to lpfc h_flag);
	phbLL);

VPD data, utr/
	tr set oke defag);->pfc_hba * ph.mb;
 in mbox compl */
	pm
 * it wAt this, drvr_flaging doLS_HBQ;
	phbled, mLL);

	fc_mbufOptiol);
	if  is
 * re, phba->mbox_mem_pool);      (phba->pport->fc_flag    PFC_HB_MBOX_TIMEO for the nextc_node
 * heart-beat out
			init_key = 0;
		}

		lprb_pende CONFIG_Pbuf_lisBructphbaeat outstandiqrestore( is
 * reHAregAL);
	return; pointer to lpfc hb+ HA* uniOFFSE_ratnsigneCout_handler - The HBA-timer timeout haCdler
 * @phba: pointeHSut_handler - The HBA-timer timeout hanSis the actual HBA-timCut_handler - The HBA-timer timeout han * uni* @phba:t8_t *outptr;

		outptructSet up heart beat (HB) KERNEL);
	s. At the time the
-
					lpfc(current 3eat mailbox commaTO THE configu:
	configuplice(&anal queue element attended to eforms aner in the inter thread thereO THE:n",
				r, phba-*****/

#include <**
 * lpfnclude <linux/ The HBA-timer timeout handler
 * @ptr: unsigned long holds the pointer to lpfc hba data structure.
 *
 * h>
#includ HBA-timer timeout handler registered to the lpfc driver. When
 * tde <scsi/scsi_device.h>_HB_MBOX_INTER and the worker thread is notified. This timeout
fc_hb_timeout_handler. Any periodical operations will
 * be perfin the timeout handler and the HBA tude "bxCmd x%it shac_crtn.h"
#inclu!= MBX_eriodic operations needed fe
 * heart-beat outstand        ad_flag & FC_UNtate set, the driveel_iy periodic operations needed for the device. If such
 * periodic event has already been att_post/Ot lpfc_hnfigu clearr in the interrupt handler
 * or bring or fast-ring events withinc_hba_down_post)(phba);
}

/4l do LPtatus_x
 * c- War*)/dma-mappPOST do(phbndox
 * c

	if atic void
lpfc_dump_wakeup_param_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmboxq)(phba->last_vent biPower On Self Test (comp) onlytion_time + LPFC_HB_ * this timer f0 ifhba->HCregaddk_stawise

	if (pmmand, mb->mbxStatus)ock);

	if (time_;
		if ((rc != MBX_SUCCESS) && (rc != MBe "lpfc_sta;
	}, uerrlolock_irq(&h timg, scratchpadba->mbo32_tbox(ude 0,>elsbuf_3)
	 to invrevisndler routine, lpfc) 2004-2005 ChristopSTout_handlush */
	}

	if (phba-/*ter(ph 1;

	30 secondread urray(strPhe Hcompletion_timc HBA cleaMP_VPD_SIZE, GFP300ZE, 
		mp = (unlock.word0 =_cnt  *
 * {
		spin_lock_irq(&phbtand/* En PortCCESacfg_complHBA_E, format tic_port	}
	bfrq(&g);

	/k);

	ie_perr poiunlockpropria	f_cnt == phba->elsbuf_pring);
		spk);
	}
	 pointOST_STmberARMFW_READY =pletsueds, buf_ptr,
				struct evisi
	if c_dmabuf, list);
			lpfc_mbuf_ffreebuf_ptr->virt, msleep(1(uintre Cms al_cnt == pESS;
	}
	**********************************
 * Th"1408e is */
	;

stomplS
	if :n_unlockis rx,4 - Pes frr=ool_asfi>mbox_nip>mbox_ipc>mbox_xrom>mbox_lloc(pdl>mbox_phba->e>mbogot apin_unlock_irLI_ABs, buf_ptr,
				struct lpfc_dmabuf, ljiffies + HZ * LPFC_HB_MBOXsfiERVAL);
				return;
			}

			lpfc_hearnipERVAL);
				return;
			}

			lpfc_hearipcERVAL);
				return;
			}

			lpfc_hear (!pERVAL);
				return;
			}

			lpfc_heardlERVAL);
				return;
			}

			lpfc_heart = phba->elsbuf_cnt;

load_flLogc_cmpl;
	pgl_l	readl
	spit_lock);
ock_irq(&&phba->hbalock);

		whiCRATCHPAD (!list_empr func for hba down post routine
 * @phba: poin2534 c strucInfo: ChipTypempool_aSliRevmpool_alloc(pFeax/inL1mpool_aOUT);
		2mpoolg(phba, s, buf_ptr,
od_timer(&_chipwwpn poit_lock);
		return;
			}

		else {
			/slird afheart beat timeout called with hb_outstafUT);
	level1set
			* we need to take the HBA offline.
			*/
			lpf2set
			* we nepmboxq, W);
sun*/
	ilude st_remolox/ctypndler mand glsbuf_cn-timer
 * 				melsbuf_cq(&phba->hbalock);

		whONLINE0 (!list_emp &&
		(p	psli->sli_flag &= ~LPFC_SLI_A1 (!list_empMSI-Xock);
			!		statu_SLI_A_NERR) || hba);
		1lpfc_offline(phba);
	= vporrq(&phba->ock_irq(&phba->hbalock);

		whUERRLO (!list_emptort->work_= LPFC_HBA_ERROR;
			lpfc_hba_downHI (!list_empt	}
	ink_state = LPFC_||port->work_ock_irist);
	***************************************
 * Thi!phb22tandiUnretaking this por *
 * Re	"rq(&_phba->mpool_aA har>work_mpool_alloc(		"= vpor0 than Port = vpor1 than Porg(phba, Klink_state = LPFCpport->work_				  jiffi	lock);
		nt &&
		(p_empto_wwort_prep(struct l WARRANTY Ot command etion_time = jiffies;
or he
	}
	spiqrestorc void lpfruct BAR0beat mailt lpfc_hmats_sgl_list_lock required because worker thread uses this
	 * list.
	 */
	spin_loc!mb->unirq(&phba-timeconfigt handl
	}
	spiring;
	lpfc_offlinde <scsi/scsi_device.h>
#LPFC_SLI_ACTIVE;
	sp;
		if ((rc != MBX_SUCCESS)R;
			lpfc_hba_down_post(phbet_loopback_flag(pfc_s-timer timeout h
	stru poindown);
	TUS_LOphba->mbox_mem_poong lpfc offliio(phba);
	phba->link_state = LPFC_HBA_ERROR;
	return;
}

/**HIphba->mbox_mem_poo_SLI_ACTIVE;
	io(phba);
	phba->link_state = LPFC_HBA_ERROR;
	re_SLI_ACter to lpfc hba data strc_offlin *
 * This routine is called to bring a SLI4 HBA offlin3)
	,
					  jiffies + HZ * LPFC_HB_io(phba);
	phba->link_state = LPFC_HBA_ERROR;
	res + HZ * L;
	psli->sli_flag &= ~LPFstati_ACTIVE;
	spin_unlock_irq(&phb1->hbalock);
	lpfc_offline_prep(phba);

	lpfc_offline(phba);
	lpfc_reset_barrier(phba);
	spin_lock_irq(&phba->hbalock1c heart-bhba->e->hbalock)(CSR&phbapin_unlock_irq(&phba->hbalock);
	lpfc_hba_a);
	lpfc_hba_dow;
		if ((rc != MBX_SUCCESSpfc_sli4_offline_ek_irq(&phio(phba);
	phba->liHBA-timer timeout hom the coe ERATys);
	TA;
	hbalock);

		whISRut_handler - Thed HBA hardware error
 * conditions. This type oISRe when HBA hardwareIMBA by setting ER1
 * and another ER bit in the host status registMr. The driver will
 *SCBA by setting ER1
 * and another ER bit in the host staatus registeCr. Thrq(&phba->pport->work_port_l @pmb;
	lpfc_hba_down_post(phba);
	lpf2->hbalock);
	lpfc_offline_prep(phba);

	lpfc_offline(phba);
	lpfc_rese @vf:= WORKER_pfc_free__transeset_barrier(phba);
	spin_lock_irq(&phba->hbalock2 doorbe (ERhbalock);
	lpfc_ofpeed is dipmb, e given vif * since we cannot c,errofies + HZ * LPFC_HB_MBOX_INTERVAL);
		else
			mod_timer(driver
 * work-portpfc_sli_ring  *pring;
ort_prep - Perform lpfc inithba->vffree( f (vf >his tyVIR_FUNC_MAXlush */
	}

	if (phba-ng ER1
 * and aRQDBut_handlerx(phba, pmb, MBXdrbre error
 * conditionsvf *e ErrorFR_umber[i] =+ (phba-Q_DOORBELht (Cng ER1
 * and aWba->work_status[0], phba->work_status[1]);

	spin_lock_irq(&phba->hbalock);
	psli->slW_flag &= ~LPFC_SLI_ACTIVE;
	sEQCba->work_status[0], phba->work_status[1]);

	spin_lock_irq(&phba->hbalock);
	psli->slped flag &= ~LPFC_SLI_ACTIVE;
	sMba->work_status[0], phba->work_status[1]);

	spin_lock_irq(&phba->hbalock);
	psli->slMre-establishing link.
	 */
	pBMBX>work_status[0], phba->work_status[1]);

	spin_lock_irq(&phba->hbalock);
	psli->sle thhba->last_cpfc_vpd_t *vp = &pba->cfg     *
 ******	"to get Ofc_hb    *
 *_CONFIG,ool_free(pmb, phba->mbox_mem_pool);
	}

	return 0;
}

/**
 * lpfc_hba_down_prep - Perfo (phba->work_hs & HS_FFERard: "
));
			i++;
			j = (status & 0xf);
			if (j <=  thre_all(phbalse {
		lag & LINK_a.abts_sglus x%xmuni    ware ONFIG, }

	abuf,)((uint8_t) down alsport_cq_;
		ieue_le eock( thi    (char)((u != MBX_Spfc_dmint8_;
	if4_cqing_def_mfree_all(pf_log(phba, KERN_ERR, LOG_INIT,
				"0453 Adapter facouldue_mlpfc_hba *pmand complialization before the HBA      *
 *******on ELS ring till hba_state is hba->bmbxeat o>extra_ring].cmdringaddr)
		psli->ring["failhba;
	udr)
(&phba->
	if (phba->pock);
ba->mbo64stathyss[0] =  MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX,
					"0352 Config MSI  > LINK_SPENA);

	writba->work_hs & HS_oard: "
			pfc_ri	spinf 2 partg;

	ppla *pnwhich
	 * _DIStri*)mb->of 16 iflagphba->cf;
	phba->*
 * This is the compl;
	phba->cfRdCo_ptr->a_fla_16_BYTESIX) li;
	Lmand "
					"failed, mbxCmd x%x, mbxStatus x%x\n",
					pm;
	phba->			pmb->u.mb.mbxStatus);
			mempool_free(pmb, phba->mbox_mem_pool);
ding = 0;
	phba-(struct lpfc_hba *ph->hbalock);
	/* Initializ;
	phba-> |= LPFC_STO));
		memcpy((cmt(struct lpfc_hbar);
			spe. pin_in Opt board:hba);

	pa->lmt & ar)((usic_hbRINT_ENAREAD_CONFIG, dma*phba;
	u MBX_a = (sinterpfc_dm16-ifla);
		if (  Aa);
	eox(p.abtsWORKER_ lpfc_hbsct lpintermai_hba
{
	spieev.p				   ;
	p lpfc_hba *phba)ensed cissunux/ctyintertrotectagainst a ra	/* Set up ring-0 (ELS);
	p.ENA;
	if (psli->nu
 **/
static void
lpoard_event.e;
	phba->hbs3(struct lpfc_hba *phae.u.wwndReva_flack);
	/* Initid is issued,ard_event.subcateg	lpfc_clea = phba->pport;->MB = fc_sli   *psliommand is issued,truct lpfc_sli_ring  _errevt_timeout ehigump_sc(pwown the SL&phba->eerror.
 *
 *ruct hich
	 * f

	phba = (eue_dis_Host *shion after is to pbox_osion reg	    (chaConfis twphba-bsli4
	/* If .
 *
 *k_stat!= NUautoDEPT markc_prwhe_stadle thheanyway.
	 */
	i_depth >uct S->vpow_HB_TMO;
e pciUpcask);
	p nt_datspeedbitspin_shifsl_b       contimpilag;

	p | HCin_l32hba->pcchinshost;

	balock);
	ph =-beat mailbtic void
lpfc_ck);
	phba-->MBslimatatueadl(phb)pring;
	uint32_t event_daablea->cfget)
		rehba-) ((e_hba_rese>> 34) & 0x3fevt_to= LPFC_(&phba->->	 */_hset_mgmt applicatievent t<< 2) | phba->Moa);
	lpfc it 1_ADDR_HI is clevent to mgmt applicatioing;
	uint32_t event_dat>> oard_errevt_to_mgmt(phba);

	if (phbalX_BU_flag & DEFER_ERATT)
		lpfc_handle_deferred_eratt(phba);
LO_offline(phba);

	/* Wait for             *
 ******leanup_disc_cq_mt(struct lpfc_hba *e <linu*/
	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	lpfc_dump_wakeup_parateare.
 loading let the worker thread conbuf_cnpmb, M_cq_

	/*b;
	char *KERN_ERR, LOG_d to hack_ir"2598 Adapt	}

ns*/
	lag &is to ptrotect aing theed, n linkaddiretus:
 is to ptroect aa data *pre levelpfc_sli_issudisba, ppeed nsed che
	 * t status register.
	r to lpfc HBA data structuretatus & ~HS_FFER1;

	spin_lock_irq(&phba- up heart beat (HB) timer */
	mod_timIf such
 * peripfc_hba *phba)
{
	st hba offline and then
		 *    *psli = &phba--
					lpfc_sli4_id
lpfc_haneturns su&vport->fc_nodename.u.sid
lpfc_han= LPde_namessue DOWN_LINK"
;
	p
		if (rc !=pe = FC_REG_BOARrintf_log(phba, KERba, pmb		/*c_sli_-ired becalpfc_unr geteailbood_timer(&phba->eratt_poll, jiffies + HZ * LPFC_ERATT_POLL_INTERVAL);

	if (phba->a afterlpfc_uulmt & ba);
	} elsude "lpfc_slicter arRE_OFFSET);
		temp_event_da)((uabuf *mp
#includi+1))wNameaximumker thrle ehba-RPI's	   's Vent_dVFa.dacsi_inclers fosbts_scsrk_h);
	fffc_hb';
	/* charO) on;
			readlmem_pool);
		_log(phba, KERN_ERR, LOG_INIT,
				"0453 Adapter failed to init, mbxCmd x%x "
				"READ_CONFIG, mbxStatus x%x\n",
				mb->mbxCommand, 		return;
	}

	lpfc_p	}
		lpfc_uER1;

	spin_lock_irq(&phba-	 * ThBOXQ_t *pmba.abts_sgl_list
	ph	}
		lpfc_un*r		lpfc_u
	if (phba->or config pmrror pfc_shost_from)
 * ch ca(&vport->feart-b* lpfore ons Copyright (C) 2004mb= vport;
	if (lpfc_sli_issue_mbox(phba, pmb, MBX_POLL011c_cmpl(stru                 *
  * 3 - M	}
	}

	LI_CONFIG_SPECIALhe firmware.
		*PFC_ELS_RING);
lpfc_hba *phbaturn;
	 phba->wor_scsi_bmbock_irannel Host B_ * 3e**********att(p, _eraPO ~LPFC******lpfc_eraSUCCESSnt support flag to 0.
 **/
static void
lpfc_config_as12 MONFIG, mbxStat backCmd x% REPRES"uf_p_temp_s back
			pmice.= 1;
			return;
		} mqm_riotect - omb->u.mqe		return;
			}

		c_prhba->elsbhba, KERN_E_speed == LII * ln_lockol);
			lpfc_une anba, KERN_.un._vendor_even_unlock_irq(&phba->hbalock);

	pmb = mort(vOG_INIT,
				;
				lpfcFP_KERext = %x\n",
		ata, use low 6 bytesource and statonROi+1))	event_data = FC_REG_DUMP_EVENT;

		sst = lpfc_shost_from_vport(vport);
		fc_host_ion v_setevent_data = FC_REG_DUMP_EVEvpool);
	st = lpfc_shost_from_vport(vport);
		fc_host_DOR__vendor_event(shost, fc_get_event_n;
}

/*,
				sizeof(event_data), (char *) &event_data,
	c_setevent_data = FC_REG_DUMP_EVE_pool);
	st = lpfc_shost_from_vport(vport);
		fc_host_g.max_xriba data structure.
 *
 * This rouer(),
				sizeof(event_data), (char *) &event_data,
		f	SCSI_NL_VID_TYPE_PCI | PCI_VENDfR_ID_EMULEX);

		lpfc_offline_eratt(phba);
	}
	returnf
}

/**
 * lpfc_handle_eratt_s4 - The/* If t,
				sizeof(event_data), (char *) &event_data,
	lpfcntion
 * conditions.
 **/
statlpfc_		shost = lpfc_shost_from_vport(vport);
		fc_host_lpfc_attention
 * conditions.
 **/
stats are dis,
				sizeof(event_data), (char *) &event_data,
	eqntion
 * conditions.
 **/
stat
 * PortBA hardware error handler
 * @phba: pointer to lpf
	/* Send an internal error evert to mgmt application */
	lpfc_board_errevt_to_mgmt(phbaw
	/* Send an internal error evewt to mgmt application */
	lpfc_board_errevt_to_mgmt(phbac
	/* Send an internal error evect to mgmt application */
	lpflmphbat_data = FC_REG_DUMP_EVElmgmt application */
	lpfc_board_e(phba, met_loopback_flag(phba);
		rc = lnumber()work_statun;
}

/**
 dump mailbox command for gettn;
}

/*;
	fc_host_are disabor_event(shost, fc_get_event_nuare disent_data = FC_REG_DUMP_Ec_set_loopback_flag(phba);
		rc = lpfc_sli_iss
 * hear,
				SC dump mailbox command for getting
v
		pshba);
}

/**
ortone ba);
}

/**
 csi_buf_liist, prg->num);
	mempool_free(pmboxq, phba003 cfhba);
	_data(B:%d M:%d)t Error "VPctual SLI3 o SLI4 hbaFerror attention handR error attention handincltual SLI3 ocess.
 *   Any other value - error.
 *number(),.
 *   Any other value - error.
 **/
int
lpfc_cr_event(shost, fc_get_event_number(),  0 - sucess.
 *   Any other value - ervpr.
 **/
void
lpfc_handle_eratt(structnd retura *phba)
{
	(*phba->lpfc_handle_eratt)(fr.
 **/
void
lpfc_handle_eratt(strucic void
l.
 *   Any other value - error.
 **/
irphba);
}

/**
 * lpfc_handle_latt - T and retur.
 *   Any other value - error.
 **/
iisc.h"
_event,ch ca******bove_data),
					  (chat lpfc_hba *);hba,DFT_HBA_Q_DEPTHagainst P;
	VENTerforms all intb_next, &aborts, lis> all internal resource and state setup)ventvolatile uint32_t control;t willdump mailbox command for getting
 * wa*****************/

#includpool,ndianetionsfc_hb_timNotifto autthe H= ' ';
buf_'s _handlc_hostfline_prep(phba);

	lpfc_offline(phba);
	lpfc_reset_barrier(phba);
	spin_lock_irq(&pnlock f_eachsideruct lpfc_dmaint8_t) (j - 10));
			i++;
			j = (status & 0xf);
			if er maximum temperature exceeded "
				"(%ld), taking this port offline "
				"Data: x%x x%x x%x\n",
				temperature, phba->work_hs,
				phba->work_statba->v_handle_lattrk_status[1]);

		shost = lpfc_shost_from****er, t(shost, fc_get_eort;
	/* B_handlemb the [2 sho{Hhys)ENDIAN_LOW_WORDSLI_ABORTED);this mbox coHIGHnd */1}PFC_Ht = number(),
					  sizeof(temp_event_data),
					  (char *) &temp_event_dataLPFC_*********************************************
 * This0492);

		spin_lock_irq(&phba->hbalock);
		phba->over_temp_state = HBA_OVER_TEMP;
		spin_unlock_irq(&phba->hbalocNA);

	writruct_temp_state = HBA_OVER_TEMP;
		hat couldD_NVPAf (rctwhba->sck_ilock_ heameoulock.al_offli		lpfc_pn_flahannel_offlphba->cfde_nameLPFC_
		if (rc !=pfc_shost_fr Erromemcpy(&LPFC_, KERN_, &il we have proif (rc !=il we have pro Erro

	} else {
		/* The if clause RING].f forces this code path when the status
		 * failure is a value other than Fhandle_latt_fr3eadl_temp_state = HBA_OVER_T/* dump*);
srt->fc__eratt void
lpfc_of * pmbr "
				"Data:FC_HB_pfc_vport RING].ft = phba->pport;
	stru*****************/

#include <li with - Perfo"to get O>sli4_hbruct lpfc_hd;

	/* word 7 contain option rom version */
	prog_id_word = pmboxq->u.mb.un.varWords[7];
r);
	readl(phba->H			"0406 oid lHBAle ea->lmt & . F     lpfruct lpfc_ 	* If 	    p_event_dasuctus if;
uns  *
 ead_c * ph(ba->hb);
	s) sha->woe takenude "lpfc_r);
	rlpfc_linkdn_unlonow,red tesed fo (uisomRE_OF!= Mtigned loas decoding dispointer to lpfc HBA data structure.
 *
 * This routd), taking this port offline "
				"Data: x%x x%x x%x\n",
				temperature, phba->work_hs,
				phba->work_status[0control, phb;
		if ((rc != MBX_SUCCESS) && (rc != Mba->hb*q.abthba->el      idxsucep_che Vital Pwhe Vhba->el       * failure). The VPD is t
 * Portmp_event_datmailbox
 * c_ENA c_slug = lba->hbEVENT;
		temgack);phba, un-flage pci;
		if ();
	} els) {
		_PORT mailbox
 * commFCPar *_dump_bWQmType, and
 cleaVPD is treated ae(psb, psb_neis treated as
 *f (inter to the VPD >
	}
	}all internal resource and state sewq ->statusP_WQNstruphba->linter to the VPD passed iuct lpfc_hba *phba, uint8_t *vpom the c, int len)
{
	ump->physinter to the VPD <ndex =Flen)
{MINter to lpfc hba data structure.
 *
 * This routine is c2581t;
	 enruct WQs (entide "lError 6 	    ccq_ec_free_include <linux/Error 6 .
 *x%x x%x d
lpfc_offpd(struct lpfc_hba *phba, uint8_t *],
			(uint32VPD is treated art->fc_sparam, pfc_prin&phba***************************WARNING                   8 attct Data: x%x x%x x%x x%	(uint32rt->fc__t) vpd[0], (uint32_t) vhba, Kquemunicle functint32_t) vpk);
	lacRKER_hi) << 8canhort)lenb
			e <lied: uccess.
 *   Any other value - error.
 **/
ihed && (ndex < (len - 4))) {
	, VPD is treated a
	struc.u.wwn);
			in receive bhi) uf_dif;
un *phopba)
{
	Lndex < (len - 4))) {
	 = VPD is treated as
ure will be populated.
 *
 * ReturnEcodes
 *   0 - pointer to
 * Port passed in is NULf characte success
 **/
			if ((_parse_vpd(struct lpfc_hba *phba, uint8_te*vpd, int leE)
{
	uint8_t lenlo, l
			if ((vpd[inde
				i = vpd[index];
				index 
	int index = 0;	j = 0;!vpd)
		return 0
			if ((tal Produc	j =
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0455 Vital 74x];
			indexE+= 1;
			lenhi = Error 6 nt32_t) vpd[0], (uint32_t) vhi) Error 6 V') && (d[2],
			(uint32_t) vpd[3]);
	while (!finiseed && (index < (len - 
 * Portiswitch (vpd[index]) {
		case 0x82:
		case 0x91:
			index += 1;
			lenlo = vpd[in75[index] == 'V') && (vpd[index+vpd[index];
			index += 1;
			i = ((((unsigned short)lenhi) V') && (lenlo);
			index V') ;
			break;
		case 0x90:
			index += 1;
			lenlo = vpd[index];
			ind= 1;
			             *
 * Port;
			indexgth -= (3+i)ngth =It dof the->pckyer
	, MBfc_sli med cV') thant32_tcleared x+1] == 'N')) {
		              * += 1;
		********************************ex += 1;
			lenlo = vpd[in93	writed short)lenhiV') && (vis>Modelrt->fc_[j++]unsigned long hi) << 8) + leni--)== '3')) [index] == 'V') && (ss
	 *me[j++]

		if (rc32_t) vpd[2]		j = 0;
				Lengd
lpfc_handlnhi = vpd[index];+= 1;
			lenhi = vpd[index]3+i);umber */
			if ((vpd[index] == 'Sreated as
 ngth = ((((unsigned short)lenhint lpf8) + lenlo);
			if (Length > len 
			if ((vpx] == 'S') && (vpd[.u.wwn)akinglgned short)l;
				continurc !=Set up ring-0 (ELS) by the(vpd[index] == 'S') && (vpsli->sldex++];
				regaddr);o get OEt lpfQ) + le(EQs)structure wred EQBA_ERRx%x x%, KERN_ERR, LOG_, fi--)because work type. ;
				index += 2;
		eq_ed.
 *
 t afterer[i] _4B_s3(struct lpfc_hb					if ((vpe;
			}
	truc->fc_ra1] ==o get OsHost  pro;
				conti;
			 rout
	    || ((ph with (&vport->fc_->Port[j] = 0;
				contLI_ABORTED);			index += 2;
				i = v== LINK_S routMBX_NOT_FINISHED) {
		rc = 4;
		goto lpfc_handle_latt_fr6                 x +=
			/* Levice Driver for pfc_printfed init, mbxCmd x[ind =  routin 1;
				inde* This proV') &	while(i--(s);
				index += 2;
		ffc hba ww.emulex.com               th -= (>u.mb;
ssued,              *
 * Portions Copyright (C) 2004-2005 Christoph[ind             *
 *                                       c - Retrieve HBA d        *
 * This program isEQErroris rouou can redistribute      [ind				}
MP_VPses the V_SIZE, with the <ture to hold the deriveel name, ma (!lpfc_ i);
			}
		}
		finished = 0;
		break;
		case 0x78:
			finished == 1;
			break;
		default:
			index  ++;
			break;
		 lpfc hba data structure.
 *
 * This routine is c0497                 0) {
			/* Levice DrNTIES, INCLUDINGall be &phbare to hold the deriv[ses the V sho routineHAregaddr);o get OC%x\n",
	e(i--) {C				phba->Port[j++C = vpd[index++];
				if (j == 19)
					break;
				}
				phba->Port[j] = 0;
r  *continue;
		C}
			el_s3(struct lpfc_hbr  *i = vpd[indexhar 			index += 1;
				index +=is proall the Ch */
	s max_speed;
	i (3 + i);
			}
		}
		finished = 0;
		break;
		case 0x78:  max_spshed = 1;
			break;
		defauwn>", 0, dex ++;
			break;
		}
	}

	return(1);
}

/**
 * lpfc_get_hba_model_d50le is pareve HBA device modend let tC name and descriptnt8_t *descp)phba: pointer to lOARD_hba data structure.
 * evice modelLS\0')
		return;

	if (phba->lmt & LMT_10Gb)
		max_speed = 10;
	else if (phba->lmt & LMT_8Gb)
		max_speed = 8;
	else if (phba->lmt & LMT_4Gb)
		max_speed = 4;
	else if (phba->lmt & LMT_2G_irq(&phbeve HBA device modelm)){ = 1;

	vp = &phba->vpde PCI_witch (dev_id) {
	casOptiI_DEVICE_ID_FIREFLY:
		m = (typeofUnsolic PostReceport0')
		return;

	if (phba->lmt & LMT_10Gb)
		max_speed = 10;
	else if (phba->lmt & LMT_8Gb)
		max_speed = 8;
	else if (phba->lmt & LMT_4Gb)
		max_speed = 4;
	else if (phba->lmt & LMT_2G                  _speed, "PCSOL RX "PCI"};
		break;
	case P = (tywitch (dev_id) {
	casrxq(typeof(m)){"LP8000", max@mdp: pointer  max_sp		te data std.
 e-to-tion*);
sV') ucture to hold the dl Proed model name.
 * @descp: pointer to the datadex];
				index += 1;
	ions Copyright (C) 2004-2005 Christoph Heces HBA's description based on its registered PCI device
 del_desc(struct lp        *
 * This program isCts to an array of 256 chars. It
 * sak;
	cbe returned witroduc model namtypeofimum speed, and the host bus troduc * The @mdp passed into this function points to an array ofphba->lmt & LMMT_8Gb)
		max_speed = 8;
	else if ( will be filled with the model name.
 **/
static void
lpfc_get_hba_mo9el_desc(struct lpfc_hba *phVPD_MODEL_DECQ1;
				j =al Produc3+i);
				whileLUDING AN_ID_T
{
	lpfc_vpd_t *vp;
	-X2"}16_t roducd = phba->pcidev->ce;
	int& descp[0] != '\urn;

	if (dev_id) {
	case max_speed;
		cMar * bus;
	} m = {"<UnknoI_DEi = vpd[indexUNE_			index += 1 i);
			}
		}
		finished = 0;
		break;
		case 0x78:I_DEVICEshed = 1;
			break;
		defau){"LPe100if (phba->lmt & LMT_4Gb)
		max_speed = 4;
	else if (phba->lmt & LMT_2G5peof(m)){"LP7000E", max_speM = 1;

	vp = &phba->vpd;-X2"};
	ch (dev_id) {
	case PCt tr data structdevice;
	intgaddr);
Wf_die(i--) {W				phba-	break;
	case PCw max_speed;
		cWar * bus;
	} m = {"<UnknoPe11i = vpd[indexeed,dp && mdp[0] != '\0'
		&& descp &ed, _DEVICE_ID (3 + i);
			}
		}
		finished = 0;
		break;
		case 0x78:Pe11000"shed = 1;
			break;
		defause PCI_DEif (phba->lmt & LMT_4Gb)
		max_speed = 4;
	else if (phba->lmt & LMT_2G4peof(m)){"LP7000E", max_speed, WPCI"};
		break;
	case PCI_DwVICE_ID_DRAGONFLY:
		m = (speed, "PCI-X2"}_ID_PEGASUS:
		m = (ty_DEVICE_ID structure to hold the d				bID_THOR:
		m = (typeof(m)){"LP10000", max_speed, "PCI-X"};
	[j++] = vons Copyright (C) 2004-2005 Christoph Hewes HBA's description based on its registered PCI device
 ->hb_o                    *
 * This program isWts to an array of 256 chars. It
 * s111", be returned wit (VPD model nam"LPe11imum speed, and reated asS", max_sp * The @mdp passed into this function points to an array ofm = (typeof(m))){"LP2105", max_speed, "PCIe"};
		 will be filled with the model name.
 **/
static void
lpfc_get_hba_50LL EXPRESS OR IMPL_ID_HELIOS_DCSP:
		m =W(typeof(m)){"LP (VPD-SP", max_speed, "PCI-X2	m = 	break;
	case PCI_DEVICEwID_NEP (VPDd = phba->pcidev->device;
	intk;
	case_speed(RQR:
		m = (typeof(m)){"Lr max_speed;
		cRar * bus;
	} m = {"<Unkno= (ti = vpd[indexPe12			index += 1;
				indereak;
	case PCitiali>Seri(3 + i);
			}
		}
		finished = 0;
		break;
		case 0x78:= (typeoshed = 1;
			break;
		defauCIe"};
		if (phba->lmt & LMT_4Gb)
		max_speed = 4;
	else if (phba->lmt & LMT_2Gc - Retrieve HBA dr;
	caseHRse PCI_DEVICE_ID_BSMB:
		m 
		m = (typeof(m)){"LPeste();

 data structure.
 * 	m = (typeof(m)){"!= NULL)
, max_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID_HORNET:
		m = (typeof(m)){"LP21000", max_speed, "PCIe"};
		GE = 1;
		break;
	case PCI_DEVICE_ID_PROTEUS_VF:
		m = del_desc(struct lp12000", Dax_speed, "PCIe IOV"};
	_PROTE(typeof(m)){"LP111", datOTEUS_PF:
		m =t *)mb) + DMP_RSP_OF_PROTE*
 * included with NESS FOR A PARTICULA_PROTEg  *pring;
	uint32__PROTEUS_se
		s IOV"};
		break:eturned--Data (VPD)S", max_spe>000-S", max_sp--
				phba-> mdp[0] == '\0')
		snprintf(mdpeof(m)){"LPe12002ata, use low 6 byteseof(m)){"LPe12002-SP	pmb->mboITNESS FOR A PARTICULAR PUw"%s"_LPE11000S:
		m (mdp && mdp[0] == '\0')
		snprintf(mdp111", g  *pring;
	uint32_111", max hba requires sSMB:
	 (mdp && mdp[0] == '\0')
		snprintf(mdpSMB:
	g  *pring;
	uint32_max_speed hba requires specicl processing, troduc){"LP1050", ators
	 * roduce put the port number on the end
	 */
	if (descp_ID_NEPTUNE:
== '\0') {
		if (oneConn_ID_NEPTUNE:
		m(descp, 255,
				"Emulex OneConnectc%s, FCoE Initak;
	c (mdp && mdp[0] == '\0')
		snprintf(mdpak;
	cg  *pring;
	uint32_ak;
	case hba requires s = (ty Port %s",
				m.name,
				phba->Port);
		e the IOCB ring.
 *
 * = (typeo		"Emulex %s %d%s %number of IOCBs with the associated DMA bt to t : "Gb",
				m.bus,
			d by the cnt argumen *des processing, tthe Vel name, maxators
	 * the Ve put the port number on the end
	 */
	if (deuint16_t dev_id== '\0') {
		if (oneCuint16_t dev_id = (descp, 255,
				"Emulex OneConnievesO BE LEGALLYthe IOCe port number on the end
	 */
	if (dbufcng  *pring;
	uint32_pfc hba  hba requiofflin = *outptr++;
			j = ((status & 0xf0)  with this patus[0], phba->w
	readl(phba->HCregaddr); /* flush */

	/* Clear Link Attention in HA REG */
	writel(HA_LATT, p it triggledr);
	readl(phba-
			j = (dr); /* flush */
	spin

	return;
}

/**
 * lpfc_parse_vpd - Parse VPD (Vital Product Data)
 * @phba: pointer to lpfc hba data structure.
 * @vpd: pointer to the vital prodcsi_device.h>
# with this pacbxStatus);
		mempool_free( pmb(m))(VPD).(phba, pmb, M; /* flush */
	suf_dif;
uns.h"
#include "me, m.max_speed,
				(GE) ? "GE" : "Gb",
				m.bus,
				(GE) ? "FC(phba, pmb, Med, urn cnt;
		}

		INIT_LIST_HEAD(&mp1->list);
		/* Al
		else
			snprintf(descp, 255,
				"Emu(phba, pmb, Menhi) << 8) +  0;
		phbacb);
			 model namx_speed,
			"PCIe"};
		break;
	casePCI_DEVGb))
	    || ber on the end
	 */
	if (descp && desIOCB_t *i5,
				"Emulex OneConnect %s, 1:
		m = (typeof(m)){"LP1oc(phba, MEM_PRI,
	uI"};
		brea12000",  &mp2->phydp && mdp[0] == '\0')
		snprintf(mdp, 79,"%s", m.name);
	/* oneConnect hba r>list);
		} else {
			mp2 = NULL;
		}
		m =;
				pring->missbu
		m = (tt;
				return cnt;
			}

			INIT_LIST_Hx%x\n",
	EAD(&mp2->list);
		} else {
			mp2 = NULL;
		}d to the IOCB ring.
 *
 * This routine p{
			mp2 = kmallo FCELSSIZE;
		icmd->ulpBdeCount = 1;
		cnt--;
		if (mp2uffer
 * descriptors specified by the cning->missbufcnt = cnt;
			ret FCELSSIZE;
		icmd->ulpBdeCount = 1;
		cnt--;
		if (mp2 *   The number of IOCBs NOT able to be a, MEM_PRI,
					respo
			 FCELSSIZE;
		icmd-s);
			if (!mp2 || !mp2->virt) {
				kfree(mthe host bus tmbuf_free(phba, mp1->virt, mp1->phys);
				kfree(m_ID_NEP		lpfc_sli_release_iocbq(phba, ioc The number of IOCBs EVICE_ID_

		if (lpfc_sli_is* This pro		Length -= (3 +cb, 0) ==
		    IOCB_ERROR) {
			lpfc_mbuf_free(phba, mp1->virt, mp1->phys);
			kfree(mp1);
			cnt++;
			uint16_t 		lpfc_sli_release_iocbq(phba, */
	while (cnt > 0) {
	derived 

		if (lpfc_sli_is&& descp &		Length -= (3 +
	/* While there are buffers to post */
	while (cnt > 0) {
		/* Allocate bs = phba->work_hs;
	struct lpf with static void lpfdr);
	readl(phba->HCregaddr); /* flush */

	/* Clear Link Attention in HA REG */
	writel(HA_LATT, p!mb->un.addr);
	readl(phba->HAregaddr); /* flush */
	spin

	return;
}

/**
 * lpfc_parse_vpd - Parse VPD (Vital Product Data)
 * @phba: pointer to lpfc hba data structure.
 * @vpd: pointer to the vituct data.
 * @len: litmap and the worker thread is ner for coe the vers parses the Vital Product Data (VPD). The 		m = ) {
		L);
	if vent_data;
up			while(i--) {
				phba->Portoid lpfn 0;
}

/**
 * lpfc_post_r) 2004-2005 Christop */
	w LMT_4Gb)
		max_speed = 4;
	else if (phba->lmt & LMT_220 Svice model flag & FC_UNLOname and description
 * @phb

	} else 
 * ngth o		break;
		case 0x78:bufcn		lpfc_wndex = 0;trucIta: ******************************************************
 * This052_irq(&phbp->virctlyvice model  *
 * Retor con->hb_tmoli_flag description
 * @phb_INTERVAL);
				return;
			}
			mod_timer(&phba->83inter: pointermb, m:ZE;
		-idsuccess.
 *values to the array -> with i At  S(N,V) (((V	lpfc_sli_release_iocbq(phba, iocb)h the model name, maximum speed, and the host bus type.
 * The @
 * This routine retrievet)
{
	IOCB_t{"LPe1250", max_speed, "PCIe"};
		break;
	case PCI_DEVIC2righ {
			/* Lox%x xhileError 6 an array as )){"LPthe V3+i);
				while
 *   An *descp)
{
	lutine sets up the initial values to the ac_iocbq *iocb;
	LI_ABORTED);
				while(ii	"READ_S************Pointer: pointer to an working hash table.
 *
 * ThiLL EXPRESr)
{
	Has0) {
			/* LoError 6 'V') &x67452301;
	Hases the Vitli_flagultPointer
 * with the values f		pring->postbufq_cnt--;
			lpfc_mbuf_free(phba,258150" {
			/* Lo 0x1032rt->fc_ING  [%d]
	HashResua_iterate(ualloc(phba->mbox_memc_iocbq *iocb;
	0;
}

/**
 * cidev->devicoid lpf max_speed;
	int GE = 0;
	int onV) (((V)<<(N))|((hba->0')
		return;

indexirq(&phbCQdical operations= CMD_QUE_RING_BHBAs.
 **/
static void
lpfc_sha_init(uint32_t * HashRes800", max_ser to an array as hash table.
 *
 * with the val routine setsrrorthe initial values to the aNOT abl values to the array of hash table MCQmer(&phhostfor the LC HBAs.
 **/
static void
lpfc_sha_init(uint32_t * HashRes;
	case Pr)
{
	HashResultPoimax_speed er[0] = 0x67452301;
	HashResultPointer[tPointer[3];
	E = HAB89;
	HashResultPointer[2] = 0x98BADCFE;
	HashRes5ree_shRe 0x1032cq
	Hash_iniic ine	E = D;ResultPointer[4] = 0xC3NOT ab0;
}

/**
ultPointer[4] = 0xC3D2E1F0;
}

/**
 * lpfc_sha_ite = (typeof(m)){"LP6000", max_spee) 2004-2005 Christopuffer
 HBAs.
 **/
static void
lpfc_sha_init(uint32_t * HashRe30eed, "PesultPointer[2];
	D = HashResultPointerCI_DEVICE_IDashResultPointer[4];

	do {
		if (t < 20)uffer
l values to the array of hash table W27999;
		ELSfor the LC HBAs.
 **/
static void
lpfc_sha_init(uint32_t * HashRe3ultPointer)
{
	HashResultPoind, "P | (C & D)) + 0x8F1BBCDC;
		} else {
			TEMP = CI_DEVICE_IDAB89;
	HashResultPointer[2] = 0x98BADCFE;
	HashRes6pfc_chaler[t];
		E = D;
		D = C;
		C = S(30, B);
		B = A;
		A =uffer

	} while (++t <= 79);

	HashResultPointer[0] += A;
	HashResultPointerCI"};
		break;
	case PCI_DEVICE_ID_CENT>ModelDesc);

	if ((p to thointer to the entry of the working hash array.
 *
 * Th2ID_RFLY:
		esultPointer[2];
	D = HashResultPointer", max_speedashResultPointer[4];

	do {
		if (t < 20)nt32_tl values to the array of hash table R27999;
		D_RFhWorking: pointer to the entry of the working hash array.
 *
 * ThPointer as the resI_DEVICE_ID_RFLY:
		 | (C & D)) + 0x8F1BBCDC;
		} else {
			TEMP = ", max_speedAB89;
	HashResultPointer[2] = 0x98BADCFE;
	HashRes7 USL@RandomChallenge. The result is put into the entry of the wnt32_t
	} while (++t <= 79);

	HashResultPointer[0] += A;
	HashRes_ID_HELIOS_DCSRocb(phba0')
		return;

	if :
		m = (typeof(m)){"LP1050", max_speed, "PCI-X"};
		break;
	case PCI_DEVID_VIPER:
		m = (typeof(m)iptor(s) to e initial hash table, returned through
 * the @HashResultc - S:
		m = (typrates an initial hash table pointed b11002-SP", max_speed
 * with -X2"};
		breaashResultPointer[4];

	do {
		if (t < 20)VICE_ID_NEPTUNE:
r.
 * The result1,
		      HashWorker to a virtual ber array.
 * @FCPg back to the initial hash table, returned through
 * the @HashResultdel_desc(s the result hash t_DCSP:
		m = (typeoic void
lpfc_sha_iteroduct nt32_t * HashResultPointer-X2"};
		brea HashWorkingPointer)
{
	int t;
	uint32_t TEMP;
	uin8ate(hbaier[t];
		{
		HashWor SLI4 hb	D = C;
	{
		HashWorking = 0al Product =
		    S(1,
		     : pointer to lpf
	} while (++t a   *phba = vport->phba;
	structcture.
 *
 * Thnter[t - 3] ^ HashWorkingPointercase PCI_DEVICE_ID_ZEPHYR:
		m     HashWork0", max_speed, "PCIe"};
		undation.    *
 * Thi "GE" ointer to the entry of the working hash array.
 *
 * Th8inter: poinM;
}

/**
 * lpfc_hba_init - Perform spec		m = (typeo

	} else minter[4];

	do {
		if (t < 20) {
	hed && 1002-SP", max_speed,{
			TE9;
		} else if (t < 40) {
			TEMP = (B ^ C ^ D) + 0x6ED9EBA1;
		} else if 3t < 60) {
			TEMP = ((B & C)M) | (C & D)) + 0x8F1BBCDC;
		} else {
			TEMP = (	m = (typeoAB89;
	HashResultPointer[2] = 0x98BADCFE;
	HashRes9ingPoM
lpfc_clew	E = D;
		D = C;		E = D;(30, B);
		B = A;
		A = TEMw80, sizeof(uint32_t), GFP_KERNE TEMP;
	} while  += A;
	HashResultPointer[1] , max_speed, "Pinter[2] += C;
	HashResuLP_CHK_NODE_ACT(ndlp)) {
			ndlp = lpfc_enable_node(vport,6inter: pointreak;llenge_key - Create challenge key based on 	m = (ty

	} else  faindomChallenge: pointer to the eFREE_REQ(ndlp);
			spin_unhe entry
 * @HashWorking: pointer to the entry of the working hash array.
 *
 * Thy state machine toPCIe"};
		break;hash array referred by @HashWorking
 * from the cha	m = (ty_INTERVAL);
				return;
			}
			mod_timer(&phba->9lpfc_cWde already */
			spin_unlock_irq(&phba->ndlp_lock);
			codlp);

	} while (++t <= 79);

	HashRorking hash
 * a * lpfc_sha_iterate - Itese PCI_DEVICE_->phys);
			if"LPe11000-S", max_speed,
			"PCIe"};
		break;
	case PCI_DEVICE_I{"LP10000-S", max_speed, "& descp[0] =e initial hash table, returned through
 * the @HashResul3t32_t A, B, hi) <<ates an initial hash table pointed be"};
		break;
	case lp)
				contE_ID_SAT_Dabric_DID for vports */
			lpfc_nlp_put(n	list_for_each_ena virtual N_Port data stru: pointer to) {
		his routine perfecessary cleanups before deleting the @vport.
 * It invokes the discove3peed, "PCachine to perform necessary staintf_vlic void
lpfc_sha_ite (VPD the ndlps associated with the E_ID_SAT_D HashWorkingPointer)
{
	int t;
	uint32_t TEMP;
	ui91fc_print already up(struct lpfc_vport *vporanup(struct  lpfc_hba   *ers -  vport->phba;
	struct lpt_for_each_enndlp, *next_ndlp;
	int) {
		l vport->phba;
	struct lpfc_nodeli		atominter[t - 3] ^ phba,ou
	spobied.
 *_DEVICE_ID'sypeof(m)){"LP9802;

	if It perforing a @vport.t = m* Ring 2 - FC+Get %ture to hold the derivedx_speed, "PCIe"};
		break;
	case PCI_DEVICE_ID) 2004-2005 Christop_PROTEU|| 04-2005 Christop0].addrHBAs.
 **/
static void
lpfc_sha_init(uint32_t * HashRe40m)) {"LPev12000P_STE_UNUSED_NODE);
			if (!ndlp)
				contp with Fabric_DID on fthe initial values to the a_PROTEeak;
		case 0x78:
		m =EE_REQ(ndlp);
			spin_unter to linter to an array of unsigned 32-bit integers.
 *
 * This routine performs4ultPointer)
{
	Hasreak;
	case P	}
		spin_lock_irq(&phba->ndlp_lock);
		if (NLP_CHKVT_DEVICE_RM);

	}

	/* At this point, ALL ndlp's should bashWL R
 *
 * Th1INTr	E = D;
	datb_tmofuncalloc(phn_unlock_irq(&phba->ndlp_lock);
			co_PROTE
	} while (++t <= 79);

	HashR
		m =
	} while (++t <= 79);

	HashRlloc(80, sizeof(_offline(phba)ic License fopecial processing, they are all initiators
	 * and we p vport
 w Chebe found i:%d\n",
						ndlp->nlp_DID, (void= 0; i < ct device sepcific driver timers
		else
	
					     NLP_EVT*********t device sepcific driver timers? "GE" : pfc_cleanup - Perf				"Fibre Channel Adapter");
	}
}

/**
 * lpfc_p vport
 ct device sepcific driver timers */
criptor(s) to a Ient interface as blocked
 * @phba: pod to the truct lpfc_hba *pht;

	/* nterface as blocked
 * @phba: pouffer
 * 
 * from the challce as blocked. Once the HBA's
 * managem *   The {
			TEMP = (B ^ IOCB ring.
 **/
int
lpfc_post_buffer(struct lpfc_ vport
 et device sepcific driver timers t cnt)
{
	IOCB_t *il be blocked. The HBA is set to block */
	whiuffer for  command = HC_LAINT_ENA;
	writel(contronclude <linux/dr);
	readl(phba->HCregaddr); /* flush */

	/* Clear Link Attention in HA REG */
	writel(HA_LATT, pncludeted per command */
		/* Allocate buffer to post */
		mp1 = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
		if (mp1)
		    mp1->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &mp1->phys);
		if (!mp1 ->virt) {
			kfree(mpd timer set properly. Otherwise,, iocb);
			pring->linux/nt = cnt;
			return cnt;
		}

		INITroup (x%x)\n",
				phba->pci_dev_grp);
		 * processalloc(sizeof (struct lpfpfc_printf_log(phba, KERN_ERR, LOG_INIT,
 * process			}

			INIT_LIST_HEAD(&mp2->list)rt device sepcific driver timersfore either putting a HBA off0 - successfu						    &mp2->phys);
			if (!mp2 || !mp2->virt) {
				kfree(mp2);
				lpfc_mbuf_free(phba,ct device sepcific driver timers */
		break
	default* process, the management FCELSSIZE;
		icmd->ulpB all the user space access to
 * the HBA,HBA interferin			"0458 Bring Adapter online\n");

	lpfc_block_mgmt_io(uffer
 * dsuccessful
 *   1 - failed
 *			"0458 Bring Adapter online\n");

	lpfc_block_mgmt_io(d to the Ipfc_vport **viocb(phba, pring->ringno, iocb, 0) ==
		    IOCB_ERROR) {
			lpfc_mbuf_free(phba, mp1->virt, mp1->phynterface as blocked
 * @phba: pointer to lplog(phba, KERN_WAR	lpfc_sli_release_iocbq(phba, iocb);
			pring->missbufcnt = cnt;
			return cnt;
		}
		lpfc_sli_ringblocked. The HBA is set to block the managlog(phba, KERN_WARn 0;
}

/**
 * lpfc_post_rcv_bu when the
 * driver prepares the HBA inta->lmt & LMT_4Gb))
	 y of which cal, phba->HCregadpfc_hba *)-
	phba-t lpfFwRevore d;

	/* word 7 contain option rom version */
	prog_id_word = pmboxq->u.mb.un.varWords[7];

	/!mb->un.fc_sli_buffc_hba *);
statbox(p
 * ers forbodrectlct lpfc_hba *);
statiphba->ne(phNEEDS_INIT_VPI;
		>link_stCQEMBOX,
		  t statc_sliport_nam &compleTATIONS ANserler. Aclude <to			vports[ointeoA MS		pr- 10))EDS_INIT_VPI;
			spia->HAregadt, sizeof(uintlockro		phfg_link- next_ndlasynchronous[i]->fcrface tok;
	case &mp2-**
 * lpfc_			}

			INIhba: pointLost)estroy_;
		_dmabuf  hba ed per n 0;
}

/**
 * ffies + HZ * LPF HBA data structure.
 *
 * This rout      m failed to init, mbxCmd 
				phba->work_status[0vports[i]->fc_flag &;
		if ((rc != MBX_SUCCESS) && (rc != Mvports[i *vports[irWords[24],
MP_VPD_SIZE, GFP(4nt_h		max_speed = 8;
	else ift = 0;
		swill be a= km out_free_mbox;

	do {
		vports[idump_mem(phba, pmb, of! offline _t * HashResu->fc_flag &__IDRv_vports(shost) = pvports[iISABLE_data strucssue DOWN_LINK"
ster e *
 * more 		uint8_t *outptr;

		terface afterwardetails, a copy of which can be found in t= *outptr++;
			j = ((status & 0xf0) py of which can be fo>mbox_meINE_MODE;
			if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED)
				vports[i]->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
FwRev;
	m= LPFC_SLI_REV4)
				vpock_mgmt_a firsmb);
	pvarRanagime.x];
				at,scsidepth >iocb(phibililbo_VPORT_ray(phead_c| HC_ERunblock_mgmhar dist ointout!= Mfc_prINE_MODE;
			if (phba-_word;h"
#include "lpfoy_vportbsor_
 * 2 - Dvoid
lrange
 x/kt		pring = &psli-printf_k, iflagne_prep( the ELS ring */
		pring a copy of which can be fouysfs interface or libdfc
 * interface will be allowed. T#include owed. Thepost_s3(struct lpfc_hba *phbvport->fcflag & FC_OFFed to issue DOWN_LINK"

	unsigned long imb->un.varRdRev.fcphHiunblock_mgmt_iEAD_SPARM m offline 	E = *****/

#i__id lpfc_destroy_booR, LOG_INIT,
				lock_irq(sho			if (phba->slom>sli3_options & LPFC_SLI3_NPIV_ENABLED)
				vports[i]->fc_flag |= FC_VPORT_NEED>sli3_oklushesver DFT__VPORT_trucu.mb.un.varWords[7];
k_stat = 0; i <= phba->max_vportointeli3_optieturn codes
 *: P <linux/id>HCregw(phba, pmb, Mstate &= ~LPFC_VFI_REGIHB_MBOX_INTERThis routflush */L);
		els>link_state = LPFC_will be al
ray(phba);
	if (vports != _ndlp;
	struct lpfc_vport **vports;
	int i;

	if (vport->missbufcnt Rev.f it and,
			sizeof hba);

	/* Issue an unreg_loba);

	lpfc_lte = LPFC_HBAlock_mgmt_ */
	vpok_irq(&& FC_OFFLIin_unlock_irqrestore(&phba->hs != NULL) {
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			struct Scsi_Host *shost;

			if (vports[i]->load_flag_UNLOADING)
				continue;
			vports[i]->vfi_state &= ~LPFC_VFI_REGISTERED;
			shost =	lpfc_shost_from_vport(vports[i]);
			list_for_each_entry_safe(ndlp, next_ndlp,
						 &vports[i]->fc_nodes,
						 nlp_listp) {
				if (!N_CHK_NODE_ACT(ndlp))
					continue;
				if (ndlp->nlp_state == NLP_STE_UNUSED_NODt
 * event will biflaphbag |= LPFC_STOP_b, ped;

			for (i ,e. It s (pslSED_NODE)
LP_CHK_NODE_ACT(ndlp))
				********->un.varRdRev.f;
	sor * associated with the HBA, 		NULL, NLP_EVT_DEVICE_RECOray(phba);
	if (vport it trig to t trigg_each_entry_safe(ndlp, n flushesoptions & LPFC_SLI3_NPIV_ENABLED)
				vports[i]->fc_f @brings dude <linux/idORT_NEEDS_INIT_VPI;
			spinpfc_dm		shl(str

			if (vports[i]->load_flag & FC_UNLOADING)
				continue;
			v it triggvfi_state &= ~LPFC_VFI_REGIlpfc_offline_p		shost =	lpf This roc_offline(struct lpfc_hba *port_prep - Perform lpfcdata stru* interface will be allowed. Tdev-> **/
void
lpfc_unblock_mgmt_iopfc_hba * phba)
{
	unsigned long iflain_unlock_irqrestore(&phba->h_hba *phba)
{
	struct Scsi_Host  *shost;
	struct lpfc_vport **vports;
	int i;

	if (phba->pport->fc_flag & FC_OFFLINE_MODE)
		return;

	/* stop port and all timers associated with this hba */
	lpfc_port(phba);
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phb>max_vports && vports[i] != NULL; i++)
			lpfc_stop_vportimers(vports[i]);
	lpfc_destroy_vp a HBA offline. It stos all the timers
 * associated with the HBA, a->max_vports && vports[i] !=epcifipfc_create_varks the HBA as in offline state for the upper lLOG_INIT,
			"0460 Bring Adapter offisc_hba)
{
	strull cqck_mgmt_NULL)
		for (i =ts[i] != NULL; i++) {
			struct Scsi_Host *shost;

			if (vports[i]->lflushes the mai+ HZqueue to make it ready to b	/* Clear arts != NULL)
		for (i =ler registestraitel(0de <scsi/scsi_device.h>
#struct lpfc_hba *);
sk_status[1]);

		shost = lpy = 0;
		cqe to the d interface will be alloSI_N a HBA offline. It stopshba, of va_flag * This routiWCQEdata.evfor now, it dlp,

		bre all the timers
 * associated with the HBA, ports routihi)  timebO;
		_mgmt_n_post_s3r to lpfc hba data structurespl_timonROuf *sed sett.
 *
 top_vp&ore the dev	struct lpfrt = phbuf *sb, *sb_next;
	struct lpfc_iocbq *io, *io_next;

OptionROck_irq(&phba->hbalock);
	/* Release all the lpfblocncb, *sb_next;
	struct lpfc_iocbq *io, *io_next;

blocke&phba->hbalock);
	/* Release arks the HBA as in offline state for the upper l
s either!Rev.femptyn ale the .varRdRev.f;
				if (ndlall theODE;e
		list_fe(vports[i], ndlp,
				ead_config(struct lpfc_hba INE_MODE;
/**vport_work_arlist(struct lpfc_hba *)hba)
	kfrnt32_t) vpd[e if (phba->work_hs & HS_CRIT_TEMP) {
		temperature = readl(phba->MBslimaddr + TEMPignedalNuCuct scsi_trback ti);
r
 * device be broug_DISABLED) {
	if ((r		returt - Create anid lpforigin+ (uinstatureate_eturn codes
 *   0 - success (currently always success)
 **/
static int
lpfc_post_rcv_buf(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
struct lpfc_hba *);_cmpl = lpfc_mbx_cmpl_read_la;
	pmb->vport = vport;
	/* Block ELS IOCBs untsste(hba->elslocatshoshba->eystem		teid lpfc_desfg_locantryf the LPFC_STOP_IOCB_EVENT;
	rc = lpfc_sli_issue_mbox (phba, pmb, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		rc = 4;
		goto lpfc_handle_latt_fr4);

		spin_lock_irq(&phba->hbalock);
		phba->over_		"DTION_RESET(&phba->hbalock);
	writel(HA_LATT, phba->HAregaHashWorkt - Create an FC peadl(phba->Hs, the m-ioctg_hb */
	s4_read_config(soffline_eratRING].f9;
		} el_SUBSYSTEM_COMMONtop_vp	if (dev !OPCODEc_vport *
lpfc_R_SLI_AB>status =4__eraEMBElpfc_he_mp:
	kfree(mp);
lpfc_handle_latt_free_pmb:
	memssociOG_Lre (shost)
 * and associa manc_mbuf_free(ps,
	*shost;
	in.LPe120			i tes th(&lpfata strork_status[0], phox	}

	hba->elsbu1INT_iocb(phbloc(&lpfst data str_vport *) shost->hostdatt data strort->phba = phba;
	vp code path wheTIMEOUT man_LA;
	control = readl(phba->HCregaddr);
	coa->mbtruct lpfc_||SI host data str insLC HBAs.
 **/
static void
lpfc_sha_init(uint32_t * HashR495k att_vport *
lpfc_create_popin_lock_irq(&phba->hbalock);on tshost->mool_a	spi>hbalock);
	psli->slocates a SCSI host data str the ndlpfer(phbaXC_PROCE*****************/

#include <lis HZ no******_cmdor a *
 *sli-4 not lpfc_hbatrotect port **vports;
	int i;

	if (phba->pport->fc_flag & FLINndex] == '_queue value since 0s specnassociated with this hba  buffers specatio. This w faiING)NOPs, the managementanhing );
		if (ck_irommand camex%x\n",
	lpfc_shost_from			index += 2;t->can_queue = phba-x%x\n",
_sli_iss		return;
	}

	lpfc_pcnt;
	}

	/*
	 * S_ERR, LOG_INIT,
		"0479 Deferred &priread_la;
	pmb->vport = vpo( pmbength,  * Sbrings hbalock)->hostmo the HBA. This routine also allocates a SCSI host data structure (shost)
 * and associates the succeODE) mb->mbxShba->ModelName[j] = 0;
				continue;
			}
			else if 1->viigned sh in hba 0>transportt = m	spin_unlock_irqT_LOCACESS_PFC_STOP_IOCB_EVENT;
	r = lpfc_sli_issue_mbox (phba, pmb, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		rc = 4;
		goto lpfc_handle_latt2519);

		spin_lock_irq(&phba->hbalock);
		phba->ot->can_queue = phbaPFC_ELS_RING);
ELS stance, structt->cruct lpfc_vport *vport;
	struct Scsi_Hort->pc_te.com               locknop)r[j++- error.
mailbox command and am= lpnt8_t *, uint8t;
	int error = 0;

	if (dev != &phba->pcidev->dev)
		shost = scsi_hoNOP,port->pormplate,
					sizeof(stnctionPORTwwn);
	memil(&vp_va *
 * free_padl(phba->Hoc(DMP_VPt_type _SIZE,(&phba-><isctmock);
	r3 Nodelist not empty\n");
		intr_phba, igned e_mp:
	kfree(mp);
lpfc_handle_latt_free_pmb:
	mem the timreturn NULL;
}

/**
 * dec_slit error = 0;

tentry, g back to tAD(&>fc_rscn_flush ing);
		spPCIe
 * ck_irq(&host->m_portnlpfc_template,
					sizeof(struct lpffc_vport));
	if (!shost)
		goto out;

	vport = ((struct lpfc_vport *) shost->hostdata;
	vport->phba = phba;
	vpport->load_flag |= FC_LOADING;
	vport->fc_flag |= Freak;
	si_Host *shost = lpfst->unique_id = instance;
	shost->max_id = LPF>rcv_buffer_list);
	spin_lock_init(&vport->work_porn = 2port->fc_disctmruct Sc/* dumpError 6 6;
	if (phba->sli_rev == PFC_Stentry);
	spin_unlb_tmoftruct lpfca = vpo = LPFC_SLI4_MAX_SEGMENTuf_ptr->virt,heartbeavport->fc_rscn_flush = 0;

	lpfc_get_vport_cfgparam(vport);
	shlayer prot_type = *****/

#include <lilpfc_nl.h"e <liffline(ph the  lpfcistered  **vports;
	int i;

	if (phba->pport->fc_flag &lpfc:m th  {
		terrupt.h>
#include <linux/kthread.h>ffline(pha#include "l>sli4_hba.lpfcsi_device.h>
#lpfc_nl.h"
rt_prep - Perform lpfc init&mb->isc.hread_la;
	pmb->vport == LPFC_PHYSICAL_PORT;
	er forystem.
 *
 * Retu It stopsrt-beateof(temp_event_data),
					  (char *) &temp_even
	if (rc ==l be put
 * terfacet ins)
{
	l = rngPoc.h"
#ihost:
	scsi_host_put(shost);
out:
	eturn NULL;
}

/**
 * destroy_port free_pmb:
	memx%x x%x xtail(&vport->listentry, &phba->port_list);
	spin_unloc* @vport: pointer to an lpfc virtual N_Pot data structuhis poivport->fc_rscn_flush = 0;

	lpfc_get_vort_cfgparam(vport);
	shost->e path when the stpost IOCB buffers, enable appropriate host interrup17y to performinclunit(&vport->listentry;
	spin_t lp hardware error p rca = vpOG_INIT,
				"0457 Adaptetentardware Errox%x x%x x all the timers
 * associated withthe HBA, mpl - r{
	swnn, I unrn   (nghba->mblpfcRT;
			hba *phba_disc_ It  &ba da~(FCF_AVAIL_SUC |ed o* unISTEREDt *) shDISCOVostdturn 0;
}

/**
 * lp in offline state for ththe HBA, list) {
		list_deort_lo
 * lpfc_hb_timeout - Thatic vtimer timeout handler
 * @ptr: unsigned long holds the pointer to lpfc hba data structure.
 *
 * This is the HBA-timer timeout handler registered to the lp 3;
		goto lpfc_handle_latt_free_mp;
	}

	/* Cleanup any ouall be posted to the lpfc driver
 * work-portck);

	if (vpor and the worker thread is notified. This timeout
 * event will be used by the wo1ed by the worker thread to ndler routine, lpfc_hb_timeout_handler. Any periodical operations will
 * be performed in the timeout handler and the HBA timeout event bit shall
 * be cleared by the worker thread after it has taken the event bitmap out.
 **/
static void
lpfc_hb_timeout(unsigned long ptr)
{
	struct lpfc_hba *phba;
	uint-mappingist 32_t,		go1* retrposted;
	unse pcigned long iflagphba = (struct lpfc_hba *)FC_Ryemp_e;
			ilyor attehba *ture.
 * @inBARer */
	m 1, 2* retr4d to autIVE) != 0)
)ptr;

	/* Check for heart beat timeout conditions */
mplate,
		phbaspin_lock_irqsave(&phba->pport->work_portre.
 *
 * This ro
	/* Check for 1ointer to SCSI host data structure.
 *
 * Thi = LPown longer lizes a given SCSI host attributes on a = Lk, iflag);
	tmo_posted = phba->pport->work_port_re.
 *
 * Thievents & WORKER_HB_TMO;
	if (!tmo_posted)
 (struct lpfc_vpocmpl - Thlast_cCIpfc sli_Shandlvirt, mp_code box command callback fun;
				index += 2;
			nk_state = LPFC_HBt willba->pport->work_port_lock, iflag);

	/* Tell the worker thba->link_state = LPFC_He is work to do */
	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	r	 * Selpfc_uneat mailbox command.
 * Ifmbox_cmpl - Thlag & FC_fc heart-beat maes.  Must done after lpfc_sli_MO;
	spin_unlock_iBA hardware error
 * condhost) = wwn_to_u64(vport->fSI ho down longer t (C) 2004-2005 Christoprnal queue element for mailbox command.
 *
 * This is the callback function to the lag & FC_ heart-beat mailbox command.
 * If configurfc_h(shost), 0,
	       sizDhannel_oost_supported_fc4s(shost)));
	fc_host_supported_fc4s(shost)[2]k_status[1]);

	spihost) = wwn_to_u64(vport->fmboxq: pointer to the driver intehost_supported_speeds(shostort->fc_portname.u.wwn);
	fc_host_supported_classes(shost) = FC_COS_CL#inclhannel_offline(p	if (phba->lmt & LMT_10Gb)
	trLIED WARce, structock);
	lpfc_sli_brdreset(phba)e deferred csi_Host  *shosba_down_post(phba);
	struct lpfc_c_host_supc_sli4_post_status_chGBIT;

	fc_host_mato lpfc hba data s =
		(((uint32_t) vporpsli = &phba->sli;

	ayer ndler ro}

	lpfc_printf_log(phba, KERN_->port&phba->s rousuccmand */
	d.
 * If configuralled. This
 * handlerf(fc_host_aw-ring or fast-ring ported_speeds(shost) |= F by processing& LMactive_fc4s(shost)[2] = 1;rrupt handler
 * or by processingfc_h
	fc_host_max_npiv_vports(s) = wwn_to_u64(vpoin the HBA-timer
 * timeout window (LPFCck);

	if INTERVAL), this h & FC_UNLOADING) {
		stat = 1;
		goto finished;
	}
	if (time >= 30 * HZ) {
		lpfc_printf_log(phba, KERN * is configured and there is no heart-beat mailbox comma 3;
		goto lpfc_handde <scsi/scsi_device.h>
#ssued and timer set properly. Otherwise, if there
 * has been a heart-beat mailbox command outstanding, the HBA shall be put
 * to offline.
 **/
void
lpfc_hb_timeout_handler(struct lpfc_hba *phba)
{
	 HBA */* Thag & FC_UNL handl) ||
		(phba->ppor)[2] = 1;
	fc_host_active_fc4sfc_host_max_npiv_vports(shost) = phba->max_vpshost->host_lock);
	vport->load_flag &= ~FC_Ls = phba->work_hs;
	struct lst);
ou_ ter - Ecmpl(sMSI-Xa, vports);i4_hin_le lpfck.
 *
 * Return codes:
 *   instance - a unique integerupt.h>
#include <linux/kthread.phba, ost *ts = 0;
}

/**
 D     basedegistered to the lpfc driver. When
ers ford3, lic_t) vpd[0);

pport->work([indead_config(his routine is invok an SLI It performpts and stops the d.
 ERR, Lux/kthr,s routis ei_statist =,
		thing,BA_Es routin_lock_tatic iO) on to alogin to a* SeD      tt with HS_FFfc_printPOLL) != MBX_ation loy(str_ENA */
		prin lpfdividKER_this FC= HC_)_creat "lpfc_ck_irhe
 * devicered to thaa, vports);he CONr,a uniquisa->hb_tf_loishba->totalIt perform

#inost) ;
	iflude <scsght offe HBAclude <linushoad_falwayli4_cq_   lpffor flush boalore graceful sscsiENT;->hb_clear it for n of range
he
	 * hostY)
		tablisps the .>hb_outst devopin_NIT,ltdete a BUG_ON()izatioevice port, i	lpfc_s,
 **);
sts = 0phba, pmb, Mleakct.
s device.
l(0xffffffff, phba->HAregcture.
 *
 * This rall be posted to the lpfc driver
 * work-port-nd stops the/ CT buffers */
	lpfc_post_buffe,tf(phpfc_shost_from_vpor_safe(ndlp, ns = 0multi-
			spint object0;
		phba->elsbuf_pr9;
		}SIX_VECTORSe drivandling hbterms of veg);
ct lpfc_24],
			t fixe*/
	structcaphba_time	/* MBOX buffeeturnts and stops the <= phba->max_record from
 * thea = vpARRAYfc_not_data),s program isfor the LC HBAs.
 **/
static void
lpfc_sha_	lpfc_mbuf_free(phba,042);
	lp routinstruct/* dump;
				j =shResultPoinmsierwar_c4s(shoss invoked to remove the driver default fcf _del_fcf_tbl_entry *del_fcf_record;
	uint32_t mbox77ct.
 **/
try{
		:is rout>mbo*
 * Retu			spihWorkingr.
 **/
void
 from
 * the porD     _FCF cmd\n");
		return;
	}

* it will"};
		b_mbox(pcontext objectt of ater */
	phba-> structure wD     -0n_unl x%x\n", r spe& descp &	phba->**/
void
lclear it formd\n");
		return;
	0

	req_len =  _SUCCEba->sp_(shos	phba->slIRQF_SHAREDort_templateP_DRIVER_HANDLER_NAMEct lpffor the LC HBAs.
 **/
static void
lpfc_sha_ex += 1;
			lenlo = vpd[0421	  size LPFC_MBOXclear it fonk initializatidr_status, shdr_add_status;

	mboxq = = lpfc_sli4_1onfig(phba, mboxq,* This pro_SUBSYSTEM_FCOE,
			      LPFC_MBOX_OPCODE_FCOE_1ELETE_FCF,
			      req_fen, LPFC_SLI4_MBX_EMBED);
	/*
	 * In pFase 1, there is a single FCF iindex, 0.  In phase2, the driver
	 * supports multiple FCF indices.
9;
	uint32This prord = &mboxq->u.mqe.un.del_fcf_entry;
	bf_set(lpirqmbx_del_fcf_tbl_codevicecurrently#incstructat	i++ contintxcmpluct late mbo)) {
		int_number(),
					  sizeof(temp_event_data),
					  (char *) &temp_evennt_data,
					  fer(phba, &psliOT_FINISHED) {
		rc = 4;
		goto lpfc_handle_latt_f7e.
 *   NULL - port create failed.
 **/
struct dev !temp_stMSff the lievice Driver *****;

	mboxq = mashResultPo fixe>worne_eratt(phback to thESS) {
		bxfc_printf_lo

	} else {
		/* The if clause above forces this code path when the status
		 * failure is a value othex += 1;
			lhostspeed == 51embeddee maist_del_init(&vport->lir SLI4 hb	 * twice. Return codes
 *   pfc_hbba, KERb* th0] != 'att(prse sli4 l
			pmCCESS) {
		lt FCF Index ftbl_coue "lpfc_crtn.h"
#include "l /* flush */
	sg[LPFC_pfc_vport *vport = phba->pport;
	struRRANTY OF MER: pointer to: hba data structure.
 * @acqe_link: pointer to the async link completion queue entry.
 *
 *
	lpfc_printf the SFwRev;
	moxq-pfc HBA nsigned sh= *pwhba: Poicf_record,
	       phba->fcf., mboxq, 
	/* The IOCailbox command
 * status.
 *
 * Return: Link-attention status in termDELETE_FCF, mboxq, tatus;

	mbo the SUn_OFFSET)ly acts on FCF Index 0.
 *
 **/
vLI4 hba stop portlock);

	list_fo
		&& !(phba->lmt & LMT_4Gb)) hba stop porleantablisnts = 0;
}

/**
 * lpfc_stop_port_s4 e if (phba->work_hs & HS_CRIT_TEMP) {
		temperature = readl(phba->MBslimaddr + TEMPpmb, MBX_Ncontext objecton aftenestablis, pmb, Mts = 0;
}

/**
 * lpf device plbox command outstanding, a
 * heart-beat mailbox is ihba stop portHBA and brings a HBA online. D24],
			ude "ta structure.
 *
 * This routine is invoked to remove the driver default fcf ink-attention status in term}

	req_lele FCF inbrin_ASYNC_LINK_Fet(lpfc_acqe_link_fault, acqe_link))top_hba_timers(phba);
	phba->pport->wor_port_events FAULT_LOCAL:
	case LPFC_ASYNC_LINK_FAULT_REMOTE:
		latt_fault = 0;
		break;
	default:
		lpfc_printf_log(phba, KE routine is inx\n",
				bf_get(lpfc_acqe_lin
 * @ lpfc driver. When
 m generating interrupts and stops te dev driver's box(phba, s coding.D     >work_port_events = 0;
	phba->sli4a.intr_enable =  threaear it for now, shall ha att_type; to wait later */
t = lphba->sli.sli(strulag &= ~LPFC_SLI_MBOX_ACG_INIT,
				"0439 Adapter failed to init, mbxCmd x%x "
				"READ_move_dflt_fcf - Remove the drive default fcf record from the portba);

	} struct lpfc_acqlock);

	list_foNA;
	iCF of_del_fcf_tbl_entry *del_fcf_record;
	uint32_t mbox62mo, req_len;
	u * lpffg_link_evice Dx%x x%x xEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ER1		lpfc_printf_log(phb32_t shdr_status, shdr_RANTY OF MEbre Channe			      LPFC_MBOXatus x%xi eitelse {
		/ LPFC_SLI4_M,
			X_EMBED);
	/ove the 1, th single FCF index, 0.  In LI4 hba stop po LPFC_ASYNC_LINK_ phase2, the driver
	 * supports multiple FCF indices.78ATUS_rd = &mboxq->u.mqe.dr_status, shdr->sg_tablesize = phba->cfg_sg_seR;
		break;
LPFC_ASYNC_LINx\n",
				bf_get(lpLPFC_ASYNC_LINK_FAULT_REMOTE:
		latt_fault = 0;
		break;
	default:
		lpfc_printf_log(phba, KE link faultding.
 **/
static uint8_t
lpfc_sli4_parse_latt_type(struct lpfc_mb);
	pr_endataba: Poins thTUS_DOWN:
	.
 *
 phba->hb_ routine wraps the actual SLI3 SLI4 hba stop pot routine from
 * th threaPI jump table function pointer f the lpfc_hba struct.
*/
void
lpfc_stop_(strurt(structDOWNtt_fault = MBXERR_ERROR;
		break;
}
	return latt_fault;
}

/*ink-attention surn att_type;********eed
 * @phba: pointer to lpfc hbp_hba_timers(phba);
	phba->pport->* lp_port_even_cmpl;
	p_ASYNC_LInto the base driver's link-attention link speed coding.
 *
 * Return: Link-attention link speed
{
	uinK_SPEED_1GBPS:
		zation prior tray(phba(strucater */
	phba-> strt lpfc_sli4_cD     ,
				"fc_acqe_link_fault, acqe_lispinlock. DSLI4 sup state;
}

/**
 * lpf
	uint16_cture.
 *ray(phction for strom the l lpf
	bf_lllpfc_de "lpfc_));
		link_\n",
				bf_get(lpj - 1TIONS Aware  lpfi.sli_flacase 0x90d to aut1 + gl_l,neratin* retr_cmpl;
	pm mailhostflushffg_listruct->a *ph->_latlpfc_hba *phba)
{
	phba->lpfc_stop_port(phba);
}

/**
 * lpfc_sli4_remove_dfltPFC_PHYSf - Remove the dr* lpPORT;
	} else {
		shost->transportadd_odephba->hbalock)* lpfasyncd[indexINRev.rrORFP_KERNEetvaactivsuccess
 struc=lpfc****/* Nck(&phlock)
/**
  It re_statcmd link.
		;
	Mn liX_t *mb;
	strulink)
	} else {
		ERR, LOritel(0, phstatus =_REV3	m = (typelink)
try_saf	LPF_dest	retureq_len;
	uint;
}

/**
 * lpf	strut_type;

	att_type sli_remove_dflt_ing->txcmpink);
	if (att_post d
	 * l_buf_list */
	/to structu_LINK_UP)
ointer[* lpfwwpn,
	e drag = 0,
			 struct2		spin unique in_sli link_spalloc(nextts = 0;
0));
		}
	}

/* dumppd[index++];a->mb>= 1			lpool, GFP_KERNEL)rDmpNEdd_stat	return;
	phba->fcoe_eventtvirt, mp->physink);
	if (att_ty(LPFC_MBOXQ_t *)mempool_alloc(ba->mbox_mempool, GFP_KERNEL);
	ing->tx
			 struct3)
	(phba, KERN_ERR, LOG_SLINTxnextboruct.
 */nd traitboxq allocation failed\n"mp = kmalloc(sizeof(struct lpfc_dmabuf;
		break;
	}
	return att_type;
}

/**
 * lpfc_sli4_parsea strulatt_link_speed - Parse sli4 link-atten		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0pfc_mThe lpfc_dmabuf allocation fapfc_d\n");
		goto outheartique att_type;
		goto  parse the SLI4 link-attentioLPFC_ASc link cK_SPEED_1GBPS:
		link_speed = LA_1GHZ_LINK;
		break;
	case LPFC_ASYNC_LINK_SPEED_10GBPS:
		link_speed = LA_10GHZ_ link fa	break;
	default:
		lhba n prior t pmb, MBmcpy(vp-ERN_ERR, LOG_INIT,
		vport0483 Invalid link-attention link s4_parse_latt_type(struct lpfc_acsetup state;
}

/**
 * lpction for stothe  threapmb, MBX_N0483 Invalid link-at &comple
			spinel_da)){" Process 
		latt_fault = MBXERR_ERROR;
		brea
static void
lpfc_sli4_asy>un.varRc link ctates */
	llyv.postKernRc_sli4_async_link->virt) {
		lpfc_printf_loge drree(phba, mp_acqe_link_fault, , LOG_INIms all int_latt_link_speedphba, acqe_link);

	/* Fally
 * me following irrelvant fieldspfc_hba data structure.eak;
	case LPFC_ASYNpfc_hba *);;
}

/**
 *(uint32_t),truc
		break;
	cGFP_KERNEL);(str
				pring->!shoor e!shoP_IOCB);
	if rq(&phba->pport->work_port_lpport->work_port_events = 0;
}

/**
 * lpflink_spba->hb_o- Stop SLI4 device port
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to stop an SLI4 device port, it stopsus & 0xf);
			if ( generating interrupts and stops the dev_configprintf_imers for the
 * device.
 **/
static void
lpfc_stop_port_s4ruct lpfcbox(phba, hba)
{
	/* Reset some HBA SLI4 setup states */
	l_stop_hba_timersar all p);
	phba->pport->work_port_events = 0;
	phba->sli4tatus, acqe_link)) {;
	/* Hard clear it for now, shall have more graceful  to wait later */resetphba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
}

/ * lpfc_stop_port - Wlag apper function for stopping hba port
 * @phba: Poin to HBA context objecLA_UNK *
 * This routine wraps the actual SLI3 SLI4 hba stop port routine fruct lpf* the API jump table function pointer f the lpfc_hba struct.
 *box(phba, pmb, Mstop_port(struct lpfc_hba *phba)
{
	phba-r failed to init, finished;
	}
	if (time >= 15 * HZ && phba->lin the driver default fcf record from the port.
 {
		lpfc hba data structure.
 *
 * This routine is invok - FCP novent_teed,
			" += 2;
				i = vF:
		lpt fcf record_dump_buf_data;
unsig[	atomirt.  This nt_tag;
	sw currently acts on FCF Index 0.
 *
 **/
void
lpfc_sli_remove_dflt_fcf(struct lpfc__dump_buf_data;
unsigtop_vport_se as published by the struct lpfc_mbx_del_fcf_tbl_entry *del_fcf_record;
	uint32_t mbox8	 * Seeq_len;
	uint32_t shdr_status, shdr_add_status;

	mboxq = mq, phba, phba->mbox_t *vport)
{
	deNT_TYPE_NEW_FCF:
		lpfc_printf_log(phba, KERN_ERR, LOG_DISC
		 */
		spin_lock_irq(&phba->hbalock);
		if ((phbaNFIG);
	NIT,
			"2020 Failed to allocate mbox for ADDort. Note that the physic New FCF found index	req_len = sizeof(
			"2546 New FCF found index 0x%xcf_tbl_entry) -
		  sizeof(struct lpfc_sli4_cfg_mhdr);
	rc = lpfork_q(&phbD      md for(phba, mboxq, LPFC_MBOX_SUBSYSTad us16]);
ERVED;
		break;
	}
	retRST);
		if (rc)
			lpfDELETE_FCF,
			      req4_len, LPFC_SLI4_MBX_EMBED);
	/*
	 * In phase 1, there is a single FCF index, 0.  In phase2, the driver
	 * supports multiple FCF indices.rkin/
	del_fcf_record = &mboxq->u.mqe.un.del_fcf_entry;
	bf_set(lpfc_mbx_del_fcf_tbl_cous boarphbaVPORT_id link-atrn stecord, 1);
	bf_set(lpfc_mbx_de structuNT_TYPE_NEW_F1F:
		lpfc_printf_log(phba, KERN_ERR, LOG_Dalock, drvr_order;
spinlock_t nd indst) ].(!mp2 		break;
= '\0') {
		if (oneConnndex)
			break;
		r will _mem_ptatus =	"2548 FCF Table full count 0x%x tag 0x%c_printf_log(phba, t(lpfc_acqe_ (!phba->sli4_hba.intr_enable)
			rc = lpfc_sli_issue_mbox(phbad to issue DOWN_LINK"
cf_index)
			break;
	g back to the initial hash table, returned thspin_lock_irq(&phba->hbal0486FIG);
		rc = lpfcx%x xError 6 mpletion queue entry.
 *
 *  {
		lpnt32_t * Hash     ;

	mboxq nique inRANTY OF MER_evt - Proceailbox command
 * status.
 *
 * Return: Licessing 0x%x * Currrn;
nk: poi
 * alink-attention sRST);
		if (rc)
			lpfc_priak;
		ETE_FCF,
			 ult:
		lpfc_printf_log(phba, KERN_ERR, ilbox command
 * status.
 *
 * Return: Link-attention sull count 0x%x tag 0x%x\n",
			b lpfc_acqe_link *acqe_link)
{
	uint16_t latt_fault;

	switch (bf_get(lpfc_acqe_link_fault, acqe_link)) {
	case LPFC_ASYNC_LINK_FAULT4_NONE:
	case LPFC_ASYNC_LINK_FAULT_LOCAL:
	cak_state.duplex =
				bf_get(lpfc_acqe_link_duplex, acqe_link);
	phba->sli4_hba.link_state.statuRN_ERR, LOG_INIT,
				"0398 Invalid link fault code: x%x\n",
				bf_get(lpfc_acqe_link_faurom generating interrupts and stops the devic;
		break;
	}
	return latt_fault;
}

/**
 * nt_tag;
	swi4_parse_latt_type - Parse sli4 link attect lpfc_hba *phba,
			 struct lpfc_acqe_dcbx *acqe_dcbxfor currently used fcf do nothing */
		if (phba->fcf.fcf_nc dcbx completion queue entry.
 *
 * This roe is to handle the SLI4 asynchronous dcbx event.
 **/
async link completion queue entry.
 *
 * This routine is to parse the SLI4 link at;
	int rc;

e and translate it
 * into thk_state.duplex =
				bf_get(lpfc_acqe_link_duplex, acqe_link);
	phba->sli4_hba.link_state.status =
				bf_get_sli4_async_link_evstatus field */
	mbsli4_hba.link_state.physical =
				bf_get(lpfc_acqe_ink_physical, acqe_link);
	phba->t_type;

	switch (bf_get(lpfc_acqe_link_status, acqe_phba);
	
	case LPFC_ASYNC_LINK_STATUS_DOWN:
	case LPFC_ASYNC_LINK_STATUS_Lg_hba.sli_flag &= ~LPFC_SLI_MBOX_ACing initialization\n");
		stat = 1;
		goto finished;
	}
	if (time >= 15 * HZ && phba->linit for logical link up */
		att_type = AT_Revent_tag;
VED;
		break;
	case LPFC_ASYNC_LINK_STATUS_LOGICAL_UP:
		att_type = AT_LINK_UP;
		break;
	default87		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0399 Invalid link attention type: x%x\n",
				88_get(lpfc_acqe_link_status, acqe_link));
		att_type = AT_RESERVED;
		break;
	}
	return att_type;
}

/**
us Alpfc_sli4_parse_latt_link_speed - Parse sli4 link-attention link speed
 * @phba: pointer to lpfc hba data structure.
 * @acqe_link: pointer to the async 90nk completion queue entry.
 *
 * This rouirq(&phba->hbalock);
			break;
	ed, and the host bfcf.fcf_indx != acqe_fcoe->fcf_index)
			bre		/*
		 * Curly, driver support only one FCF - seat this as
		 *_4G)
		&& !(phba->lmt & LMT_4Gb))
	attention link speed and translate
 * it into th.duplex =
				bf_get(lpfc_acqe_link_duplex, acqe_link);
	phba->sli4_hba.link_state.statu in terms of base driver's coding.
 **/
static uint8_sli4_hba.link_state.ph_link_speed(struct lpfc_hba *phba,
				struct lpfc_acqe_link *acqe_link)
{
	uint8_t link_speed;

	switch (bf_get(lpfc_acqe_link_speed, acqe_link)) {
	case LPFC_ASYNC_LINK_SPEED_ZERO:
		link_speed = LA_UNKNW_LINK;
		bsync_event_proc(struct lpfc_hba *phb10MBPS:
		link_speed = LA_UNKNW_LINK;
		break;
	case LPFC_ASYNC_LINK_SPEED_100MBPS:
		link_speed = LA_UNKNW_LINK;
		break_evt(phbaLPFC_ASYNC_LINK_SPEED_1GBPS:
		link_sp.duplex =
				bf_get(lpfc_acqe_link_duplex, acqe_link);
	phba->sli4_hba.link_state.status =
				break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0483 Invalid link-attention link speed:4a, KERN_E->u.mb;
	mb->mbxSqe_link_speed, acqe_link));
		link_speed = LA_UNKdRev.v3rslt(phba, a}
	return link_speed;
}

/**
 * lpfc_sli4_async_link_evt a, KERN_ERR, Lasynchronous link event
 * @phba: pointer to lpfc hba data strphba);
	_host_qe_link: pointer to the async link completion queuer failed to init, mbxCmd x%x "
				"READ_REV, mbxStchronous link eve
	rc = lpfc_mtic void
lpfc_sli4_async_link_evt(struct lpfc_hba *phba,
			 struct lpfc_acqe_link *acqe_link)
event_tag;
truct lpfc_dmabuf *mp;
	LPPcmpl ;
		te	READ_LA_VAR *la;
	uint8_t att_type;

	heartqe_link);
	if (att_type != AT_LINK_DOWN && att_type != AT_LINK_UP)
		return;
	phba->;
	int rc;

	putstandinink->event_tag;
	pmb = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0395 The mboxq allocation failed\n");
		return;
	}
	mp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_K_evt(phba,
		utstanding ELS commands */
	lpfc_els_flush_all_cmd(phba);396 The lpfc_dmabuf allocation failed\n");
		goto out_free_pmb;
	}
	mp->virt = lpfc_mbuf_alloc(phba, 0, &mp->phys);
	if (!mp->virt) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0397 The mbuf allocation fa*
 * lpfc_api_tableut_free_dmabuf;
	}

	/* Cleanup any outstanding ELS commands */
	lpfc_els_flush_all_cmd(phba);

	/* Block ELS IOCBs until we have done process link event  to set up the per HBA PCI-Device group functiondata strufcf.fcf_indx  table entries.
 *
 * Return: 0 if success, otherwrwise -ENODEV
 **/
int
lpfc_api_table_setup(strucf_log(phba,hba->sli.ring[LPFC_ELS_RING].flag |= )
{
	int r_IOCB_EVENT;

	/* Update link event sta	phba->pci_dev_grp = dev_grp;

	/* The LPFC_PCI_DEV_OC uses SLI4 */
	if (dev_grp == LPFC_PCI_DEVrom link ACQE */
	lpfc_read_la(ph the device.
 	pmb->vport = phba->pport;

	/* Parse and translate statusink);
	phba->sli4_hba.link_stat->mbxStatus = lpfc_sli4_parse_latt_fault(phrom the llink);

	/* Parse and translate link attention fields */
	la = (READ_LA_VAR *) &pmb->u.mb.un.}

/**
 * lpfca->eventTag = acqe_link->event_tag;
	la->attType = att_type;
	la->UlnkSpeed = lpfc_sli4_parse_latt_link_speed(phba, acqe_llpfc_hba *phba)_PT_PT;
	la->granted_AL_PA = 0;
	la- */
	la->topoloion jump table_PT_PT;
	la->granted_AL_PA = 0;
	la->il = 0;
	la->pb = 0;
	la->fa = 0;
	la->mm = 0;

	/* Keep the link status for extra SLI4 state machine reference */
	phba->sli4_hba.link_state.speed =
				bf_get(lpfc_aIT,
		phba->), this han#inclcmpl;
	pock);
	list_spli#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#includ#include <scbuf_list */
	/*tepuct O) onntion link speed: x%x\n",
		ing interrupts and stops the orms a bo * This routine will do uninitialization after the HBA is reset whepin_unlock(&phba->sli4_hbt->load_flag & FC_UNLOADING)
		lpg |= LPFC_STOP_Iqrestore(&phba->scsirror -> off *vpor|ng
 *UNLOADIalues0;
}

/**
 * lpfc_hba_down_post - W_rcv_bufnd. cturflag)tic int lpf is reset w = lpfct comm_mgmt_);
	if , LPFC_MBOXure.
 t) vport->     req_brt_sudition SLI3 dev.
 * @pharReadLA;
	laa + offset,
			 It
 * uses the kernelorms a board restart,4and then brings the board ice port
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to stoding mailbox commands.
 **/
void
lpfc_reset_hba(struct lrom generating interrupts and stops the devicled then set error state and return. */
	if (!phba->cfg_enable_hba_reset) {
		phba->link_state = LPFC_HBA_ERROR;
		return;
	}
	lpfc_offline_prep(phba);
	lpfc_offline(phba);
	lpfc_sli_brdrestart(phba);
	lpfc_online(phba);
	lpfc_unblock_mgmt_iover_resource_setup - Setup driver internal re     ces for SLI3 dev.
 * @ppci_select_bars
 * This routine is invoked to set up>mbonclude <linux/ointeco,
			back
 * onl_vport(vpor_speeds(gaddobjet_transporERN_E_t) vpd[0link *acqetructural re;
	i i;
		ine i	other valu'scate _hba._MBOX_ACTC_REG Adapt, unse_lia = (st0x30 + (
	}
_flaS_FFER6.
		* T_DOWN:s+ le device *dev)
{
	stink: pointer to 
	phba->fcp_poid lpfc_freee if Aollinghba _index, = (unsis
 * peed(s structure.
 *
 * T_t) vpd[0
lpfc_har dinyhis routine  SCSI= phba->pport;
	struct lpfc_n phba;
	/}
	return latt_fault;
}

/**
 *(mb->RNEL);
	id_la;
	pmb->vport = vpio(phba);
}

/**
 * lpfc_sli_dointer[4] = 0xC3(shost);
ouCP no buffers nGracembxCoba_qued(&c	    ( subhfg_hatic inlbox
 * queublocked
 * @or attenvalue since phba->c KERN_f (r, = wwn_ling mode tiblockist_del_init(&vporays rmmunictic int
lpfc_scsimer(&vport->els_tmoe */
	phba->si_brdrestlatt_typeASY"DateratLKratov * 2;
	mod_timer(&vport->els_tmo_type != ATfc_host);
		RATT | if"030;
			m = either Since the sg_table->status =(dev !ACTIVruct lping, issue a NIT,
++_eratt;
	re Errothe IO(dev !WAI0441 Vce - Get a u_DISC_Forsk = (HAlink);

	/* lbox
 * queuink: pointer to ifline.dad(&complms all intern segments are added since the IOCB nee == 0)
			break;
		if (mb->un.vartion = led.
	 * 2 tentrensor_eg_cnt = n link fault codL);
BX_NOT_FINISHEDnd aray(phb	/*
	 *pl_puvirtual N_Porata, use low 6 segments a= ~e added since the IO4);
	}

	/* AlEG_CNT;
		prn -EIO;
		fset;
		lpfc_sli_pcimem_bcopy(((uint8_qe_fear ->hbalock8) + letructursli4.h"
#include "ializes the la->mm = 0;
c link c* Se/
	systrintfTIONS AN.h"
#include "ox_timeout;
	psli->mboarRdRev.sensed,
ields R;

	lptrigd lo lpfc->hb_tionModeline.t32_t));
	vp->rev.sli1FwRev = mb->un.varg_port mbo attributes on = kmal;

	/* Checource_setup - Setup driver ia->hbalock);  /*ci
	fobe_one_s3lpfcCI);
	pmb = (now, shto the base dig_asyncVPI;
	/* %x "
				"INIT_LINK, mbll pendingx "
		id
 * lpfc_sli_driver_res idsubhfi communicate with the pcipfc_dmdriver's t/scsi._reset_hba(struct lpfc_hba *phba)
{printf_.
 * @in

	return  Walidan Emulexize =to the lpfc driver. When
phba: pINITenent
oout_habMAX_l);
		returMAX_VPI;
	/* Tloohar)t_driver_res-lock.h>
f:
	kfm_pool);
	o all th= 0)
		lpfcb);
	p specollowion for stoptrucb, MBX_k_ti_splicmb = mempis kit;
	phphba->HCregaddrmlockl(phba->HCregaddr); _link_spochronoux/kth ID to ts regisCregadfree_all(ph, *nrm theallocat clai"lpfc_slis
 *phba->ork_hs  = 0;
_t *)mempool_ary allot wrirom
 * 		phbtwarelesize =pra->lNPIV
	 * is enabled.
dapter fa_link_spehba: pointerk.
 *
 * R	nega
	lpfa->HCr * support th
	/*e SLI-4 HBA device cal link up e _ lpfure.ated on toif (lpfc_mem MBX_BUSY)
				mempoo,t issuost_fromY)
				o lpftimeidl_free(pmb, phba->mbo ox_mem_pinitialization after the HBAse
		sprintpfc_prin_link_evt(struct LC_HBng[LPFC_ool);
			return -EIO;
		}
	}
	/* MBOX buffer will ;
			if (rc != mpooloc(phba->mboxtic void
lpfc_board_errfc_sgl_ligene, "
ll pending phba,setup->lmt & his value is also pport->Y)
				*************mand *********************************************
 * This fk_irq(&phba->tup */
xStatus x%Device Driver for      p(struct lpint32_t) v(strtruct scsi_transport_te_ERR, indicates an er-0c void lpfalue is also * lpfc_config_poparse_latt_ter to lpfs(shost), 0,
	       sizeohba stopY)
				lpfc hba data rse_lalock.h>
#->hb_outimer timeout hahis value is also uncnk_state <= LPFtmofunc.function = lpfc_hb_timeout;
	phba->hb_tmofunc.data = (unsigned lorights reser!mb->unnt32r timeout han];
	D = HashResult_timer(&phba->fals_tmofunc);
	vrc;
	-e_deHBA(pport_events = DISABLED) is value is also u
	}

	mb = &pmb->u.mb;
	phbatmofunc.function = lpfc_hb_timeout;
	phba->hb_tmofunc.data = (unsigned lo!= MBX_SUCCE!mb->una = (unsigned lll);
	phba->eratt_IT,
		);

	if (FC iinit_timer(&psli-r);
	phba->fabric_ba = (unsigned long) phba;
	/*
	 * Wta sux/blkdev.h>
#it;
	phba->fabric_block_timer.data = (unsigned long) phba;
	/* EA polling mode t150", max the
	 * MAX_XRI, MAX_VPI, MAX_RPI, MAX_IOCB, and MAX_VFI settings.
				sizeof (phba->RandomData));

	/* Get adapteng) phba;
	/*
	 * _SLI3_VPORT_TEAparse_latt_,
					"0441 VPD function = lpfc_hb_timeout;
	phba->hb_tmofunc.data = (unsigned lopeed, "PCt of the EmuleFwName, (c MAX_RPI, MAX_IOCB, andse values.
	 */
	settings.
	 * All eratt_poll.data = (unsigned long) phba;
	/*
	 * We need to do a READ_CONFIG2mailbox command here before
	 * calling lpfc_get_cfgparam. For VFs this wic - Retri the
	 * MAX_XRI, MAX_VPI, MAX_RPI, MAX_IOC   lpfc_vpd_daypeof(m)){"LPe100vport_work_arlog(phROM version out;
	psli->mbox_he HBA is restmofunc.function = lpfc_hb_timeout;
	phba->hb_tmofunc.data = (unsigned lodel_desc(ep - Perfoc);
		memp;
	phba->fc_map[2] = LPFC_FCOE_FCF_MAPypeof(m)){"currentlysysfatict_louextra SL_enable_hba_reset) {
	
	psli->mbox_ted, m8		50list_ING)
		lpffunction = lpfc_hb_timeout;
	phba->hb_tmofunc.data = (unsigned l * ID. Thein_lock_irq(8		500
	 *];
	D = HashResultPointeri4_hba.atbl_couthe sg_dma_buf_tup */

	default:
		lbp->phort_template =- pointe structI-Device gusop po; calculattrue\n");
		brulizatrn -ENOa kn>hbaer memtruct ltimer */
his will be seo(phba);
}
ritel(0, _empty(&	 * 8k		49_mbu
	 */
	if (phba->ompletvice rearn;
	phba->fcoe_eveut;
	psli = 0;
uct l pmb, off = 242;
	ect lpfc_acqe_linklled with the model name.
 **/
static void
lpfc_get_hba_mhis routin192
	 */
	if (phba-	 * 1k		5of(sli4_sree(phba, buiver for       	 * cmd(3ID_SAT_Dint3f the
	}
	->virompletionernal resourct;
	phba->fter to lpfc hba data structure.
 *
 * This routine is calEVICE_ID_P the
	 * Mhbafg_sg_seg_cnt + 2) * sizeof(struct slilude "lpba->cr, "
				plice_in50ma->HAregady it after ofportvi* @pe firmware.
		* E = 11ing, is5mand a port fro*/
	lpfHBQ].hbq_allntention fields */
	la = (RElloc;
	>cfg_sg_dma_buf_senti+ 1) -
					lpfca->sli4_hba.link_sre Erroe driver defif (att_typhbaatt mailbox c.hbq_free_buffe* Block ELS IOCBs until42;
	elevice ready;
	scsi_log
 * lpa->mtatus x%vice reaance - Get a uni x%x x%x xhbalock);

		/* Read the FCF table and re-discov%x\n47seg_cnt <= r */
	spin_lock_fcoe->event_/* dumpy driver */
	spint FC d
lpfc_off(&phba->sli4_hbent_tag;
	la->attType t_type != AT_LINK_UP)
hba data structure.
 *
 * Thiow-patT
			bxvporvePFC_Sr */
	spin_lock_init((struct + 2)-ts_scsi_buf_lphba, KERNc_sgl_li scsl_buf_list */
	/* abt_sgl_list(st,
			&phba->slr config_porrt froee dev<= 2	498scsi/scror at: pointa->cfdhba_model_dfailed "scsi/UNLOADIa + offset,
				(&phba->&phba->rb_pent;

	/* led then s
 * TO BE LEGALLY	 * cmd(3D.  See the GN	 * cmd(32) + rsp(rsp(160) + (500 *                 1k, 2k, 4k, ap[2] = LPFC_FCOE_FCF_MAPaborted CQ Evee
	 * used to create the sg_dmMP_RSP_OFFSETNVALID.  See the Gfc_vpd_data + offap[2] = LPFC_FCOE_FCF_MAP2;
t;

	/* Whi#include <scsi/scsi_hoseue CQ Event work MAX_VFI sIT_LIST_HEADssued and timel Public Lic_timer(&phba->         _timer(&phba->work queue list *mb = ->scsi_bpport;
	~LPFC_BLOCK_MGMlock);
	psli->sli_flagat timt andfc_mem_alloc(BPL_ALIGfacili))
		return -g thiMEM;

	return 0;
}

/**
 * lpfc_sli_driver_resouror SLI3 dev
 * @phba: pointer to lpfp(ph hba data structure.
 *
 * This rouspinlockMEM;

	/* Create theriver internal resources set up
 * specific for supp= lpfc_MEM;

	/* HBA diGet agl_lata structa.abts_sgln all vp &comple/* flushba->cfg_sb>cqeder(phba);
ked to unset the ver internal break;
	case Lues - exror
 **/
staoc(phba, SGL_ MBX_BUSY)
				mempool_free(pmb, link_state = LPFC_HBk_irq(&phba->hballist_foritialization after the HBA}
	return lat.
 *
 *OR;
estore(&p!= Nrc;
	int i, hbq_count;* Retu_lock lpfc_sli *psli;
	int iscoveryong t rc;rintf(phba->baratten		forlectt_masread afIORESOURCE_MEM_offline_prep(phba)er(&vport->els_tmo);
	lpfc_sli_brdrestart(phba);
	lpfc_online(phba);
queue element for maEAD(&phba->sli4_hba.sp_fcp_x(phba, pmb, Mrface as queue * ModelDesoy_vk page boundary orror atteHEAD(&phba-> queu&phba-****************** queue !ION_VPDnc dP_VPD_SI to s onling hba error at&&(phba);is sl list.ault fcf 	fc_vport_terminate(*****s[i]->********);
	lpfc_destroy*******work_array(phba, ******);

	/* Remove FC host and then SCSIx Linuwithevic physical **** */
/***rhe Em_ Lin(s Lin****scsiost Bus Adapters.     *****cleanup******part of
	 * Bring down     SLI Layer. This step disable all interrupts,2009  Coprs     mules,    cards* EMUmailbox commands,ux Deresets2009     HBA.2009/rt of HBAULEX and S will be di *
 d after td.  c EMUnnel*****sli_hba_.  AThis ****/* Stop kthread signal sh EMUtrigger *****done     more timeCopyrig     _stopThis ->****er_g     istoph Fi     Copyri of txcmplq* www.emulm       Copyright (C) brdrestart5 Christyright (top 2004    rs5 Christospin_lock_irq(&      hbaNU G*****ist_del_init(&*****->c Lientry of the GunNU General       *
 * Publ*********bugfs****************part of D   *
 *          opyright (C)     *
 _intrify it undpci_set_drvdata(pdev, NULL          Lin_pupters.    004-2009 C EMU     free bef    memMPLIEDsinceANY I bufs are released tom   ir are torrespondulexpools here       right (Y IMPLIE5 Christo*****TY OF ME_allify it unddmaPT TO coNFRInt(&NTAT->TATIOight (C) 20q_size(),
			         *qslimp.virt,.  See the GNU G*
 *part of FLIEDresources associatedr     SLI2ill befac      AT SUCH DISCLAIMERS ARE HELD *n be_SLIM_SIZENVALID.  See  GNU2 General Publi        for  *
 * munmap adap
 * s paux DeControl Regby trsful. io*****       ctrl_regsEXCEmap_p****************/
 GNU <linux/blkdIMED, E2004*
 * DISCLAIONDITIFITNESS_selectedcludionsENTATIObarr  *NDITIEXPRESS deviceENTAT);
}

/**
 **
 * TDITIOuspend_one_s3 - PCI funcFOR clude <****-3 h>
#in for power mgmntthre@NTAT: poLEX aFOR h>
#h>
#intype.msg#inc <linanagement messag
#inthreed.  routine isFOR beions ed fromm    kernel'ssi/scsubsystemude <lp****thre#includPcsi/sMsi_device.(PM)FOR k.h>
#ich can b-3 found in tspec. WhenthrePM invokemarkis method, it quies, a     k.h>
#iby e tepulex
#incriver'sthre       g      nclu
#include , turnulexoffnclude 'sill be usefx DeDMA,threx Debmulex
#include "offline. Note that ah"
#include  impleviceh"
#ithreminimumde "requird long _o a<scsi/-aw
 * clude "lde "lpfc_hwlpfc_log4.h"
lude </resume --* EMU    *ossi*
 *PM.h>
#incs (SUSPEND, HIBERNATE, FREEZE)threOR A Pspinlock()includeions C        tref whias hba_modux Devicata;
uns    threfully re as ialize itsnclude "dumulext _dumuint8_t *);
st,ct lpfc_hba *);
statn rek.h>
#iscsi/s_D3hot statwillsi/scconfig spin tinst    of_destulexiw4.h"accorE, OR*, uintlude provided"lpft lpPM.lude <sRenclu coder;
s	0 -ata;
unspinlockS FO#include ba *)Error otherwisnfig*/
*);
icill 
ad.h>
#include <linux/p(struct x/intev *NTATIOpnux/
#inc_t msg)
{
	ox(struS    HLinu*ers.  =uct lgNS, REPRESENTAT of tx(strunclude < *his  = (box(stru********** *)ers. -> LinPRES)->his >
#includprintf_logThis fiKERN_INFO, LOG_INITNVALI"0473si/scsi.h>
ude "lpfc_hw.h"
#ipinlock.\n"part of Emulex.  All ri lpfc_hENT, ARE ion.h"
_pre***************ion.h"
_hba_down                            *
 * Thpe that it will be usefsi/sc_hba *phba);
staALL EXPRESS OR IMPLIED CONph Havfc_hba *p*);
stscsi/sct lpfc_createENT,DITIOave_*);
s lpfc_iniDITIONS,scsi/4_cq_event_,t lpfc_hba_cq_eruct lp0<linux/kthread.h>
#int _dumlinux/pci.h>
#include t _dumppinlock.h>
#include <linux/ctype.h>

#include <scsi/scsi.h>
#ine <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#in PMthrelpfc_nl.h"
#include "lpf
 * or.h"
#include rt_fc.ht lpfc_create*);
standstatic int lpfc_sli4_qh"
#include ""
#includs"lpfon.h"

char *_dump_buf_dthreta;
unsigned long _dubuf_data_order;
char *_dump_buf_dif;
unsigned longclude "nclupinlock_t _dump_buf_lock;

static void lpfc_get_hba_model_desc(struthret lpfc_ *, uint8_t *, uint8_t *);
static int lpfc_post_rcv_buf(struct lormation fr     ic int lpfc_sli4_queue_create(struct lpfc_hba *);
static vhba *pfc_hba *p        n redct lpfc0 directlytatic int lpfc_create WARRAthreis roulude "lp*);
sorder(struct lpfc_hba *);
static int lpfc_sli4_read_config(struct lpfc_hba *);
static void lpfc_destrpfc_transportbox(struct lpfc_hba *c_free_sgl_list(struct lpfc_hba *);
static int lpfc_init_sgl_list(struct lpfc_hba *);
static int lpfc_init_active_sgl_array(s	uint32_till r_modekdevnt eruct(struct lpfc_hba *);
static void lpfc_free_active_sgl52truct lpfc_hba *);
static intt _dumba_down_postRs routstroy(struct lsi/sc_hba *);
static void lpfc_ll(struct lpfc_hba *);

st0linux/inis rout4_cq_event_releif ENTAT->is_busma****)
	ease_all(eof (u lpfc_intoph Heartupcsi_transpoclude "lpfc_loisx Linux******.void l              *
 * =wig     _run(******o*****al PubNVALI		"*****       %d"al Publibrd_no 0; i < IS_ERR                *
 * ) {
		_HBA_ = PTRemcpy((char*)mb->un.varRD;
	ruct lpfc_hba *);
static voiERRfc_free_active__sgl34_order_dumpfai <scde <cpu_.h"
#inc"_POLL)g     :C_HBA_=x%xba_d,C_HBA_));

ct scsi_HBA_ER	}text =C lpfcurx commenit will be useful. ink_state =*
 * TO BE
					 OR IMPLIEal Publiink_state 0; i < x%x READ_NV= LPFC_INnsed,
ORnvp.rs		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);

0	if (rc != FBX_SUCCES
					"error, mba_down				"0324-EIOig P elsent32",
					mb->mb =link_state =_is_LC_HB {
	ribu"
#include retrieveute it and/or   *
 * modify it un_post_rieve_cq_event_pooLode "lpcurAIME activwill be usefwnn))ENT, ARE log OR Istateus x%x\n",
					mb->mbxCoruct scsi_transport_templatio__HBA__det<linux/pci.M8_t *)ncluhandlulexpinlock.h>
#ih>
#I/O4 Conftype.h>

#include <scsi/scsi.h>
orde @*);
s:s = 0x0;

	/*C initinectionOXQ_t *pmb;
	Mcsi/scsi_host.h>ude <scsi/scsi_tfc.h>

#includncluntf_log(p= MBX_SUCCtoormatpfc_sli.h"
#include "lpfc_sli4.h"ed.  incl%x "
%x\n",
			p_endianCIr;
spi
#includ *
 * aCommabus);
		meaff x%xude "ie_create(has been mb, MBX_39 Ado lpfld toART;
	}


	/lpfc_ne "lpf     neSUCCESS)opuf_lock;
I/Os to issuLEX and S(ss.
 *   -			"043 OANTA_dump to oneields.
	 *ct scscludeCI_ERS_RESULT_NEED40 AETlpfc_logmfc.h>

#include "perform lpfper recoveryt lpfe_crsirequirer(struct lpfc_hhe HB					"0440 Adapter failed t-*/
	if (mcan re WARRANsion informaa->mbox_mem_poolDISCONNECrn -, phba-could no

	ihba->slie iss
static voDITIers
		  lt_ lpfc_dx(phba, pmb, MBX_POLbox(struct lpfc_hba *);
ci_channel4_cq_e_a *);
s public licensed code only\0";
	static int init_key = 1;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
t_sgl_list(ssli *pchar= l      sliv.sli1FwName, (cha_	LPFC *pfc_gmb);i < *);
st=ba *);ev.rBit io_perm_MBX_ure_free(pmb, phba->mbox_mem_pool);
			return -ERESTART;
	7link_sev.rBitatus)v;
	an
	/*emcpy(vvarRDnvpost_NU G* EMUDrive, phbas'ev.rr oAll ri LinuENT,, ARE      dev_bNU G_hba_downt iniCopy uun.varned long outstaSE, ORDrivev.rr 	vp->rev.smli_flush_fcp	vp->sion 2 of 			"0324= 3 && !mb->un.varRdRev.vig Portx/interrupt.h>
#include <l04-2009 TNFRI mayree(v.rr dropp* The valufirmunsi      truct iocb (I/O)RdRevtware; you l redistDrivelrese2009 retryarRD *
 * re-pn, blish fielinkNGEMENT,.sli2->un.sli->sli2[ev.opF.fcphHig]wwpn));
;
	vab****feaL	vp->us x%x\nsli2 hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONs_LC_queinux sloMDS;
etpfc_revarRdRev.fcphHigh;
	vper failed pfc_sli_issue_mbox(p NPIenseetPOLL);
	if (rc !=
 * mod
	vph>
#pinlock.h>
#ini/scscratch39 AdaKERN_ERR, LOG_INIT,
				"0439 A_REV, mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxSta;
		mempool_free( pmb, phba->mbox_mem_pool);
		return -EREST%x\n",
			 *
 * he drive This se= 0, rca *phb
 * modnit, READ    LC_HBA(phba->,atioifLC_HBAa cold-boot39 AdDtruct it, READ_REV has ";
		mesion inf, kmallota;
unsct scshe HBA			"0440 Adapter failed void lfc.h>

#includ     		"missing revilog(phba,sion infux Device ons Cld tocsi_host WARRANons (phba, p.(rc != ncludehba *, sion int_prep(stru -ERESTART;
	}

     pfc_sli4_qudistribusg.h"
#9 Ad
					"_preLEX and S, butelds.
	 *just puredistributosion.h"
OXQ_t pfc.hithout pass
	vpanyatus)traffic
		mempool_free(pmb, phba->mbox_mem_poolRECOVERED -t_prep(struc This setpmb, phba->m == 3 && !mb->un.varRdRev.v3rsp) {
		mempool_free(pmb, phba->mox_mem_pool);
		return -EINVAL;
	}i3_options |=lock for use with gnu public licensed code only\0";
	static int init_key = 1;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
li1FwName, (char*) mb->un.varRdRev.sl		phba->link_state =
	v = pfc_hk(c void lpfcS ARE HELD *"sion in
	vp;

	do  NPIV
	 * ivarRDnvi < 5l);
		"mbx, phbaEXCE lpfc_nvp.rsP_VPD_SIZE);ERR 
			s: Canl_fr= mb					"print"ruct lpfc_hrnRev = );

	kfree(abled.
	 */
	if (vp->re->rev.fcphLow = mb->unsed;

			for (i = 0; i < 56; i += sizeof (uint32_t), ptext++)
				*ptexthe GNU General       *
 * Publiev.opF;
	vp-ag &= ~us);
SLI_ACTIVEee Software Foundation.    *
 * This p initialization "
					"error, mbxCmd x%x READ_NVPARM, "
					"mbxStatus x%x\n",
					mb->mbxCommand, mb->mbxStatus);
			mempool_free(pmb, phba->mbox_mem_pool);
			return -ERESTART;
	27x_mem_pool);
	retu#include "l*
 * printf_ offset);

	kfree(pl - Completion handler for config as     sizeof(phba->wwnn));
		memcpy(phba->wTaklpfc_version.h"
;,
				ESS) {
			lpf is freehba);
static int_hba_down_post/or   *
 * modify it undoptions = 0x0;

	/* Setup and issue mailbox READ REV command */
	lpfc_read_rev(phba, pmb);
	rc = e are done.
		 */
		if (pfc_sli_issue_mbox(ppfc_tra|= LPFC_SLI3_VPORT_umDOWN;

	ntf_ reva%x "
on	if (lpfc_is_pcidev->device))
		memcpy(phba->RndomData, (char *)&mb->un.varWords[24],
						sizeof (phba->RandomData));
csi.h>
#inci.h"
#include "lpfc_sli4.h"Iog(phude <scw lpf(*ptextP_REGION_VPD); tellhe HB_pre**** lpfc_slf_log(og(phok NULL;
statnormal(rc !o lpfc hbakmalloc(DMP_Vtus);_REGION_VPD);. A*
 * Portions ,
		 * ma ca"
			!lpfo flowcsi/scsi to 1.
	 t lpfgainordestatic vovoidINVAL;
	}d completlock for use with gnu public licensed code only\0";
	static int init_key = 1;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {wpn));
	}

	phba->sliinux/kthread.h>sli4;
staelsIf thecnt -DINGculct lphe #rap_ELS IOCBdump_nfigrv
#incleof(#include <scspletelpfcPRES t_sgl_uigh;domDati_issue	if (numbermboxq->/CTu.mb.mbxStatus != MB*/
id lpfc_d */
	char dist_char[]a *);
static truct lpfc_freLPFCmax_xrb->u.varRdRev4 ver.d_wocfg_param.varWxriFwRev = .varRdRev_revStatus);
ommaREV4nvp.rsi < d_word =<= 100int3abled.
	4 **/    mboxtring */
	if256rg->dist < 48
		dist = dist_char[prg-512rg->dist < 416
		dist = dist_char[prg-1024rg->dist < 432
		dist g->dist < 4)prg-(struct lct scsi_transport_template *lprobranspor4ci.h>
#Versiate = NULL;CCESS)4troy(struct lpr must type.h>

#include <scsi/scsi.h>
#inclpid#include <scsi/scsi.h>
 identifiephba_REV, mbxStatus x%x\n",
				mb->mbxCransport_fc.h>

#include ", phba->mbobreaprg->r@phba: pointer to lpfan Emulexhar *i.h"
#incointer to lpfc h ist, ppnfign whiotic inbusvoid l(*ptextfc.h>

#includlooks ait, mb, phba-li4.ific0) {
	missfc hba f
 * mailbox comm, the reo se = dit lpfc_hba ruct lpsponsebreahumanp_buf_dnd stkintrap_PD, mbxSIt: posmba->vpd_successfu void lpfc_slle apA(phlpfc_nl.h"
#icsi_hostions,
				"0441 Vmb, ****eIt rehumaclaiscsi_tHBport.hit do a c_lock;
pfc_sli4_fc hbaesponse /
	idump_dohe
  MBX_mmand, mbC init revlyorder(struct lpfc_hba *);
static in*/
int
lpfc_coad_config(snegaetup valueshost_from_vpol_frrt(vport);
	LPFC_MB
static void  _;
outni lpfc_destrVersion, "%dbox(struct lpfc_hba *);consa *)(struct lpfct_frid *pidc_free_sgl_l rom versrev.->un.varDmp.word_cint lpfint lp=ONS A = LPFC_HBA_ERDmp.word_cordstate,link_state = LPFCmcntphba->wAlloc;
stmemorybxStaribuq, phba->fc_read_nNVPARM, 2004aer_t (i = 0; i < !on */
vp.nodenameNOMEMphba->wP	"missigenericool);
	retur;
	rede "l lpfc hbaENT,vd3, lic*****_data);ct lpfc_hba_down dis_MBOX,free(pmb, phba->mbox_mem_pool);
			return -ERESTART;1409phba->wwnn, (char pcit attentvarRDnvpgomay utPT TO * If thPort inSetRev;ghtsAP
#incl%x "
jump >un.#incluPCI-, phba-group-1BA_ORev.en GFP_KERNEL);api_	if (		uiu******,tus);
 lpfcEV_OCa->link_state .  */
	lpfc_EXPRESS ct lpfcptext = b, 0);
	p-4sli4. all{
		lpfc_pristate =atic void  GFP_KERNEL);>u.mb_vpdTY O {
		lpfc_a->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}
	mb = &pmb->u.m10phba->wwnn,RNELuparamebxStatus x%xor NID.  */
	lpfc_448 Adapter faile(phba, pmb, 0);phase-1     oettieate(spfc_sli_ails, a n",
				mb->mbxCom{
		_(mp);
voidils, _rt, m1us);
		phba->link_state = LPFC_HBA_ERROR;
		mp = (struct lpfc_dmabuf *) pm1->context1;
		memp(mp);
		return or NID.  */
	lpfc_un	uint mb->mbx)
		phba, pmb, 0);
	p-4 S"
				"READ_SPA(mp);
		return -EIO;
	}

	mp = (stru.mbpfc_dmabuf *) pmxStatus);
		phba->link_state = LPFC_HBA_ERROR;
		mp = (struct lpfc_dmabuf *) pm2pfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	pmb->context1 = NULL;

Ifc_sli4_qux Depopx";

	if (feaLec Li {
	mRev;
	vp- GFP_KERNEL);sli If thec LiThis fg->dmboxq->u.mb.un.varWords[7];

	/* Dec		phba->link_state = LPFC_HBA_ERROR;
		mp = (struct lpfc_dmabuf *) pm3phba->wwnn,pfc_sli4_quef (struc>phys);
	kfree(mp);
	pmbort->fc_sparam.no1 = NULL;

	if (phphys);
		kfree(mp);
		return -EIO;
	}

	mp = (struct lpfc_dmabuf *) pmb->co2us);
		phba->link_state = LPFC_HBA_ERROR;
		mp = (struct lpfc_dmabuf *) pm4pfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp)T TO c_portnamig Port inipfc_p Driver for*, uint*
 * Fibre Channel GFP_KERNEL);cpfc_p_ers. us);
		phba->link_state = LPFC_HBA_ERROR;
		mp = (struct lpfc_dmabuf *) pm5phba->wwnn,ptr = ABILIT Lint) = wwn_to_u64(vport->fc_nodename.u.wwig Port initializatisys  *
ttribut -EIO;
eted coreof(phfc_hw;MBX_POLL) != MBXer_t_(char_((uiin the hohba->link_state = LPFC_HBA_ERROR;
		mp = (struct lpfc_dmabuf *) pm6phba->wwnn,Seriatatuschar)((uiba->mbox_mem_pool)*******ers.  = NULL;

Now * iy *);
st
					"error, mbx"
#includeto_be32_hba *phba)er heateint8_t) jordsuse_msv.slwhile (truvp->rev>hbaustroy(structa kn  Al*);
st WARRANoc(phba->ll be useful. der the ter****n.varRdRev.un.tialization "
					"error, mbxCmd  x%x READ_NVPARM, "
	4				"mbxStatus x%x\er heateRDnvpmand, mb->mbxStatus);
			mempool_free((pmb, phba->mbox_mem_pool);
			return -ERESTARTrwiseus & 0xf);
	
					"error, mbor NID.  outptr;

_irqDEVa->li*/
	lpfc_read_Number[i]  **/}) (j -t, mbxCmd x%xribute i				"mbxCommand2004xStatus);
	 mbxCmd x%x "
				"READ_CONFIG, mbxStatus x%x\n",
				m142lpfc_mbuf_free(phbahba	phba->link_state = LPFC_HBA_ERROR;
		EXPRESS OR Imb, phhba->mbond NOP mbx cmdsbxStanon-INTx mailb Setup and issue town return -Eba->wwnn))!= prg->dif (	lpfc_printf_se <lnop_mbox_ax_xe, &vport-

	p   tus);
ACT
			meCNTpmb);r drihep->rSetup and issues == eivationl == HBMSI/MSI-X(mb->un.varRdConfig.== 0 ||
phba->.varRdRev.sc Liata, p OR I >atus);
 mb->un.varRdmbxCmdoptions = 0x0;

	/* Setup and issue mailbox Rizeof(phba->wwnn));
		memcpy(phCmd x%x EV command */
	lpfc_			mb->mbxCom		breakmb, phbauct lpfc_hba *);
static void lpfc_free_active_hba->1_mbox(phba,and issue mailb(%d)ent suppMBX_SUChba_queue_depth =
			R, LOhba-> & LMT_1Gb))
	/* U
	pmock;

reivousm_pool);
		ruct ev.endecRev;
	4);
	pmbhban.varRdRev.unTry next levelrce K_SPEED_10G)
	    ||i] =
				  --ink_state = Port inalock);
pfc_hfc_hba *phba)
{ LINK_SPEEuct lp  *
rt->fxStatus);
		por drit;

	rt: poatio   (sion.
 *****>
#inclupfc_popfc_re		outptr = &vtic vrt comntf_log(phct scsi_tr
g_hba_queue_dept:yright (C)4 EXPRESS OR IMPLIED C;
		mempool_free( pphba->cfmempool_free( pin the homber[i] =
				   phba->cf[i] =
				   	phba->cfg_lort->fc_nodename.u.wwphba->cfort->fc_nodename.u.wwnorts(shost) = to parse_vpd ? - mK_SPEED_AUTO;c_portname, &vBs on ELS ring till hba_stat_s4phba->cfg_linkfc_dmabuf *) pm;
	pma_ring].flag |= LP->context1phba->cfg_lin->contexing].cmdringaddr)448 Adapter faiOWN;

	/*48 Adapter faia_ring].flagread_sparphba->cfe <linux/dma-map			"0324 Configansport_template *lpf Bus n, "%d.%d%d%cinclude uner, prg->rev, prgevice)) { must be plate *lpfc_vport_transport_template = NULL;
static DEFt - Perform lpfc initialization after config port
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine will do LPFC inPost r)
		gotafter theict lpmiss(struct lpnemersaryMBX_SUCCEpfc_logm>pport;roy(structee(pm{
			mempooif (rc != MBX_SUCC
	struct ScsMVersion.
 **/sli *pexi = &phba->sPost receive lock for use with gnu public licensed code only\0";
	static int init_key = 1;

	pmb = met completed cora *);
static int lpfc _init_active_sgx, mbxStatus x%x\n",
	leted sn.varDmp.word_ctruct lpfc_hblished* If thfc_scode /* Markt_prep(strucare aE, OR for ENT,the GNU General       *
 * Publiblished oadt for |= FC_UNLOADING
 * This is the completion handler for drore ddistribu(char)((uint8_t) 0x30PEED_AUTO;
	}

	phba->link_ston linkTNESSstruct lpENT,
		Optiomberny o*
 * Fibre ChannelENT,
		

		outptr = &************
 * This     (chaENT,
		!orrect_INInclu(b->u1; /
	ifeof(phd_woENT,
		&&LAINT_E[i]us |= HC; i++rg->d*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                              lmt & LMT_10G is free All ri*
 * Fibre Channel *
 * Copyright (C) 2004-2009 Emulex.  All rights reserved.           *
 (strucLEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com        FCoEo set allNGEMENT, ARE m is distributed in the hoba->cfg_lin2004ing].cmdringaf the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This p iniNG ANY IMPLIED WARRANif (psli->ring[psli->fcp_ring].RCHANTABILI2009 buff****
 * FITNESS FOR A PARLAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, Esli->ring[psli->fcp_ring].cmdringaion (U*********************lpfc_oorbeR, LO************>fcp_ring].flag |= LPFC_STOP_IO| HC_ERINT_ENev,
etails, a ctups   *
 *ll do LPFC inport = vpitial       *
 * WARRANTIEring[psli->next_ring].cmdringhis prograly, PLIED
#include "l
		kfree(mboxq, phba->ESS)
		phbe <linux/dma-mappict scs<linux/kthread.h>
#include <linux/ buffers for des<linux/spinl */
	if (pclude <linux/ctype.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>ude <scsi/scsi_transport_fc.h>

#include "lpfc_hw"
#inclcludee "lpfc_hw.h"
#include "lpfc_sli.h"
#incointer to lpfc hba datae "lpfc_nlmand c
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "l.h"
#inmand cde "lpfc_logmsg.h"
#include "lpfc_crtn.h"
#include "lpfc_vpo	else
				mand comfc_version.h"

char *_dump_buf_data;
unsigned long _dubuf_data_or (rc) ;
char *_dump_buf_dif;
unsigned long _dump_buf_dif_opinlock_t _dump_buf_lmand com
static void lpfc_get_hba_model_desc(struct lpfc_ *, uint8_t *, uimp_bu8_t *);
static int lpfc_post_rcv_buf(struct lpfc_hba *);
Any ot (rc) pfc_sli4_queue_create(struct lpfc_hba *);
static void lpfc_sli4_quink  = lpfc_sli_is lpfc_hba *);
static int lpfc_create_bootstrap_mbox(struct lpfc_hba *);
static int lpfc_setup_endian_order(struct lpfc_hba *);
static int lpfc_sli4_read_config(struct lpfc_hba *);
static void lpfc_destroy_bootstrap__t status, timeout;
	int
static void lpfc_free_sgl_list(struct lpfc_hba *);
static int lpfc_init_sgl_list(struct lpfc_hba *);
static int lpfc_init_active_sgl_array(struct lpfc_hba *);
static void lpfc_free_active_sg298truct lpfc_hba *);
static int lpfc_hba_down_post_s3(struct lpfc_hba *phba);
static int lpfc_hba_down_post_s4(struct lpfc_hba *phba);
static int lpfc_sli4_cq_event_pool_create(struct lpfc_hba *);
static vink_speed);
			phba->ct_pool_destroy(struct lpfc_hba *);
static void lpfc_sli4_cq_event_release_all(struct lpfc_hba *);

static struct scsi_transport_template *lpfc_transpor buffers for desL;
static s phba->mbox_mem_pool);
			return -EIO;
		}
	} else {
		lpfclpfc_config_port_post - Perform lpfc initialization after confdef_mbox_cmpl;
		lpfc_set_loopback_flag(phba);
		rc = lpfc_sli_issue_mboxaphba, pmb, MBX_NOWAIT);
		if (rc != MBis routine will do LPFC initialization prior to hba->link_state = LPFCT
 * mailbox command. It retrieves the revision inf*
 * Returnom the HBA and
 * collects the Vital Product Data (VPD) about thBA for r;
spinlock_t _dump_buf_lock;

static void lpfc_get_hba_model_desc(struct lpfc_hba *, uint8_t *, uint8_t *);
static int lpfc_post_rcv_buf(struct lpfc_hbaer for  Any other value - indicates an error.
 **/
int
lpfc_config_i4_read_config(t lpfc_hba *phba)
{
	lpfc_vpd_t *vp = &phba->vpd;
	int i , rc;
	LPFC_MBeue_dQ_t *pmb;
	MAILBOX_t *mb;
	char *lpfc_vpd_data = NULL;
	uint16_t offset = 0;
	static char licensed[56] =
		    "key u LOG_MBOX,
					"0352 Config MSI mailbox command "
					"failed, mbxCmd x%x, mbxStatus x%mpool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	mb = &pmb->u.mb;
	phba29link_state = LPFC_INIT_MBX_CMDS;

	if (lpfc_is_LC_HBA(phba->pcidev->device)) {
		if (init_key) {
			uint32_t *ptext = (uint32_t *) licensed;

			for (i = 0; i < 56; i += sizeof (uint32_t), ptext++)
				*ptex t = cpu_to_be32(*ptext);
			init_key = 0;
		}

		lpfc_read_nv(phba, pmb);
		memset((char*)mb->un.varRDnvp.rsvd3, 0,
			sizeof (mb->un.varRDnvp.rsvd3));
		memcpy((char*)mb->un.varRDnvp.rsvd3, licensed,
			 sizeof (licensed));

		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);293	if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX,
					"0324 Config Port initialization "
					"error, mbxCmd x%x READ_NVPARM, "
	f_log(phba, KERN_ERRn",
					mb->mbxCommand, mb->mbxStatus);
			mempool_free(pmb, phba->mbox_mem_pool);
			return -ERESTART;
29		if (rc != hba->wwnn, (char *)mb->un.varRDnvp.nodename,
		       sizeof(phba->wwnn));
		memcpy(phba->wwpn, (char *)mb->un.varRDnvp.portname,
		       sizeof(phba->wwpn));
	}

	phba->sli3_options = 0x0;

	/* Setup and issue mailbox READ REV command */
	lpfc_read_rev(phba, pmb);
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POd.%d
	if (rc != MBX_SUCC_printf_log(per[0ntf_log(phbahba, KERN_ERR, LOG_INIT,
				"0439 Adapter failed to init, mbxCmd x%x "
				"READ_REV, mbxStatus x%x\n",
				mb->mbxCommand, mb->mbxSta is the completion handler for dump mointer to lpfc hbaERESTART;
	}


	/*
	 * The value oi_rev != 3)
		 since the driver set the cv field to 1.
	 * This setting requs the FW tstaticet all revision fields.
	 */
	if (mb->un.varRdRev.rr == {
		vp->rev.rhba *, uint
		lpfc_printf_log(phba, KERN_ERR, LOG_INox(phba, pmb, MBX_POLL);

stati init, READ_REV has "
				"missing revision infation.\n");
		mempool_free(pmb, phba->mbox_mem_pool);
		return -ERESTART;
	}

	if (phba->sli_rev == 3 && !mb->un.varRdRev.v3rsp) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return -EINVAL;
	}

	/* Save inforl;
	pmb->vport = phba->pp->rev.rBit = 1;
	memcpy(&vp->bled.
	 */
	if (vp->rev.feaLevelHigh < 9)
		phba->sli3_options |);
		pring->txc	if (!lp;

	if ( */
	if (phba-(phba->cidev->device))
		memcpy(phba->RandomData, (char *)&mb->un.varWords[24],
						sizeof (phba->RandomData));
ABORTED);

		lpfc_sli_abort_iocb_ring( getting
 * wkmalloc(DMP_VPD_SIZE,  can red( pmb	if (!lpfc_vpd_data)
		goto out_free_mbox;

	do {
		lpfc_d_mem(phba, _mbox(p offset, DMP_REGION_VPD);
		rc = t lpfc_hba i_issue_mbox(phba, pmb, MBX_POLL);

		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"0441 VPD not present on adapter, "
				n_lock_irx%x DUMP VPD, mbxStatus x%x\n",
					mb->mbxCommand, mb->mbxSta mb->mbmand com.varDmp.word_cnt = 0;
		}
		/* dump mem may return a zerhen finiC inied or we got a
		 * mailbox error, either way we are done.
		 */
		if (mb->un.varDmp.word_cnt == 0)
			break;
		if (mb->un.varDmp.word_cnt > DMP_VPD_SIZE - offset)
			mb->un.varDmp.word_cnt = DMP_VPD_SIZE - offset; LOG_MBOX,
					"0352 Confidump_wakeup_param_cmpl - dump memory mailbox command compleset or DOA. Eitherhba: pointer to lpfc hbalock_irq(&phba->hbalock);

		/* Cancel all the be
	 * on the lpfc_sgl_list so that it can either be freed if the
	 * driver is unloading or reposted if the driver is restarake up parameters. When this command complete, the response contain
 * Option rom version of the HBA. This function translate the version number
 * into a human readable string and store it in OptionROMMVersion.
 **/
static void
lpfcalock);

	list_for_each_entry_safrintf(phba->OptionROMVersion,  -pool_f%d%c%d",
			prg->ver, pfc_->lev,
			dis= 3)
		lpfc_post_rcv_buf(phba);

	/*
	 phba->mbox_mem_pool);
	return;
}

/**
 * lpfc_config_port_pos_issue_m******S FOR A Pc initialization after ba data sreturucture.
 *
	returnsnitialization after the CONFIG_PORT mailbox
 * comman);
	}call. It performs allternal resource and state setups on the
 * port: poormation frbuffers, enabappropriate host interrupt attentions,
 * ELS rinr;
spiimers, etc.
 *
 * Ren codes
 *   0 - success.
 *csi/scsi_hostdispba->OWAIT);
g_hba_in_locphba-> revi#incluoe the >rev, prgVersuct lsi_hos, which *);
statd
			ict lpfc_hba *phba)
{
	struct lpfc_vport *vport = phba->pp;
	LPFC_MBO
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	struct lpfc_dmabuf *mp;
	struct lpfc_sli *psli = &phba->sli;
	uintt status, timeout;
	int i, j;
	int rc;

	spin_lock_irq(&phbfc_src.sli1FwName, 16);
	c_hbts_sfode the Ophba,ad_t lpfc_dwordENTATIOn word tREV_CONF_WORD, &(str.Chec0)spin_unlock_irqPFC_H  (cha(bf;
stEIO;
	}

hba =_validditions)rsion word toINTF_VALID) &&
		rt->work_port_lock, ifra *)tmo_posted = phba-t timeoutSLI4pin_loc |= HC_Ra->sli;
	uint32_thba->ppid));
ev);
	elrt_events |= WORKER_HB_T3O;
	spin_unlontry_safegnedP_IOCB_EVENT;

	/* Post recei   Any other for desired rpfc_phba->sli_rev != 3)
		lpfc_post_rcv_buf(phba);

	/*
	 * Configure HBA MSIut - The HBA-timer timeout handler
 * @ptr: unsigned long holds the po suc {
			mempool_free(pmker thread is notified. This timeout
_REV, mbxStatus xbe used bye worker thread to invoke the actual timeout
 * (rc) {
		 routine, lpfc_hb_ti {
			lpfx_mem_pool);
			return -EIO;
		}
 = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERdump_wakeup_param_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmboxq)
{
	struct prog_id *prg;
	uint32_t prog_id_word;
	char dist = ' ';
	/* character swiLS r Optionct lpfc_grp
	if cT_ENintf_log(phbaLP:ba->cfg_lphba, KERN_ERR,port_lRDnvp   || ((eat outstanding stOCe. Once the mailbox commaMO;
	ses back and
 defaul		psFC_HBA_ERROR;
		return -ENOMEM;
	}
	mb = &pmb->u.m24 Inlag);ool);
	returbox(p: 0x%xphba->lmts and marks
 * heas back and
 }ue DOWN_LINK"
			" mbox command rc 0x%x\work to do */
	if (!<linux/ssted)clude <linsi_devicetype.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#inclhe HBA-timer timeout handler
 * @ptr: unde "lpfc_hw4.h"
#include "lpfc_hw.h"
#incluhba, pmb, MBX_NOW);
		if (rc != MBbe used by the worker thread to invoke the actual timeout
 * <linux/sroutine, lpfc_hb_timeou<linux/sMP VPD, mbxrder(struct lpfc_hba *);
static int lpfc_sli4_read_config(struct lpfc_hba *);
static void lpfc_destroy_bootstrabox(struct lpfc_hba *);
static void lpfc_free_sgl_list(struct lpfc_hba *);
static int lpfc_init_sgl_list(struct lpfc_hba *);
static int lpfc_init_active_sgl_array(s	unsignate = LPFC_H30) seconds and marks
 * heart-beat outstanding state. Ostore(&phba->poy_bootstrap_mbNTATIOlpfcs back and
 * no error conditions de
 * This is the actual HBA-MO;
	spimeout handler toimer is
 * reset to LPFC_HB_MBOX_INTERVAL seconds and the hea5t-beat outstanding
 * state is cleared for the next heart-beat. If the timer ex* Tell the worker thread tc_transpwork to do */
	if (!(rc != er will put the HBA offline.
 **/
static void
lpfc_hb_mbox_MBOXQ_t * pmboxq)
{
	unsigned long drvr_flag;

	spin_lock_irqsave(&phba->hbalock, drvr_flag);
	phba->hb_outstanding = 0;
	spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

	/* Check and reset heart-beat(rc != routine, lpfc_hb_timeou(rc != boxq, phba->mbox_mem_pool);
	if (!(phba->pport->fc_flag & FC_OFFLINE_MODE) &&
		!(phba->link_state == LPFC_HBA_rrupt handlock for use with gnu public licensed code only\0";
	static int init_key = 1;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
HBA-timer timeout handler
 * @phba: pointer to lpfc hba data structure.
 *
 * This is thrrupt handmand comes back and
 * no error conditions deink_state == LPFC_HBA_ERROt mailbox command timer is
 * reset to LPFC_HB_MBOX_INTERVAL seconds and the hea6ations needed for the device. If such
 * periodic event has already been attended to either in the x(phba, pmb, MBX_   Any o->HAreg>txcmplq_cnt = 0;
		spin_uhba, KERN_ERR, LOG_INIT,
				"0439 Adapter failed to init, mbxCmd x%x "
				"READ_REV, mbxStatus x%x\he HBA-timer timeo_LOCAL_REJECT,
				      IOERR_S
 * evost_s4 - Performpin_lock_irq(&phba->hbalock	}
	spin_unlock_irq(&phba->hbaloAIT);
		i	return 0;
}
/**
 * lpfc_hba_down_pscsi_host.h>ision fieldsbe used by the worker thread to invoke the actual timeout
 * n tran&phba->pp MBX_SUCmailblpfc heart-beat mailbox com to invoke >elsbuf, &complmbox_mem_
		mempool_free(pmb, phba->mbox_mem_pool);
		return -ERESTART;
	}

	if (phba->sli_rev == 3 && !mb->un.varRdRev.v3rsp) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return -EINVAL;
	}

	/* Save infation as VPD data */
	vp->rev.rBit = 1;
	memcpy(&vp->sli3Feat, &mb->un.varRdRev.sli3Feat, sizeof(uint32_t));
	vp->rev.sli1FwRev = mb->un.varRdRev.sli1FwRev;
	memcpy(vp->rev.sol);
		return -E-timerpletion handler for config handler
 * @phba: pointer to lpfc hba data structure.
 *
 * This i
	}

	/* Save informaNTATIOmcpy(&	(phba->pport->load_flag & FC_UNLOADING) ||
		ret;
	unsigned long ifunc,
					  jiffies + Himer is
 * reset to LPFC_HB_MBOX_INTERVAL seconds and the hea7;

	if (time_after(phba->last_completion_time + LPFC_HB_MBOX_INTERVAL * HZ,
		jiffies)) {
		spin_unloci3_optionsport->work_port_locher
	 * way,sted)
		l on lpfc_abts_els_sgl_list, it needs to be
	 * on the lpfc_sgl_imer(&phba->hb_tmofunc,
				jiffies + HZ * LPFC_HB_MBOX_TIMEOUT);
		return;
	}
ting
	 * the port.
	 */
	spin_l_irq(&phba->hbalock);stati/* required for lpfc_sgl_list and */
uf_cnt == phba->elsbuf_prev_cnt)meoutent for mailbox command.
 *
 * This is the callback funcT;
	}

etions);
		phba->elsbuf_cnt = 0;
		phba->elsbuf_preo take the Hilbox error, either way we are done.
		 */
		if (mb->un.varDmp.word_cnt == 0)
			break;
		if (mb->un.varDmp.word_cnt > DMP_VPD_SIZE - offset)
			mmb->un.varDmp.word_cnt = DMP_VPD_SIZE - offselock for use with gnu public licensed code only\0";
	static int init_key = 1;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {

		if (!phba->hb_outstanding) {
			pmboxq = mempool_alloc(phba->mbox_mem_pool,GFP_KERNEL);
			if (!pmboxq) {
				mod_tZE - offset;
	 ||
		(phba->pport->load_flag & FC_UNLOADING) ||
		k_irq(&phba->hbalmailbox command timer is
 * reset to LPFC_HB_MBOX_INTERVAL seconds and the hea8;

	if (time_after(phba->last_completion_time + LPFC_HB_MBOX_INTERVAL * HZ,
		jiffies)) {
		spin_unloct _dump_X_SUCCESS) {
				memhba: pointer to lpfc hbts within the HBA-timer
 * timeout window (LPFC_HB_MBOXimer(&phba->hb_tmofunc,
				jiffies + HZ * LPFC_HB_MIat titting
 * wake up parameters. When this comd complete, the response coretuOK LPFC_Hption rom version of the HBA. This funon translate the 
#includAny other valsbuf_prev_cnt)) {
		spin_e worker thread to invoke the aretutual timeout
 * alock);
	phba->elsbuf_cnt = 0;box is issued andpin_unlock_irqMVersion.
 **/
static void
lpds. At the time the
 * heart-beat mailbox command is issued, the driver shall set up heart-beat
 * timeout timer to LPFC_HB_MBOX_TIMEOUT (current 30) seconds and marks
 * heart-beat outstanding state. Once tc void
lpfc_d comes back and
 * no error conditions detecteeset when brimailbox command timer is
 * reset to LPFC_HB_MBOX_INTERVAL seconds and the hea9t-beat outstanding
 * state is cleared for the next heart-beat. If the timer expiredLINK_EV
	int rc;

	spin_lockba);
	dUCCESS[] =vp.r{x(phVENDOR_ID_EMULEX*);

stEVICE @phVIPERa->lx(phANY_ID*);

sure.
 *
},or handler
 * @phba: pointer to lpfc hbFIREFLYa structure.
 *
 * This routine is invoked to handle the deferred HBATHOta structure.
 *
 * This routine is invoked to handle the deferred HBAPEGASUSa structure.
 *
 * This routine is invoked to handle the deferred HBACENTAUta structure.
 *
 * This routine is invoked to handle the deferred HBADRAGONdware error
 * conditions. This type of error is indicated by HBA by setSUPERdware error
 * conditions. This type of error is indicated by HBA by setpsli = &phba->sli;

	/* If the pci channel is offline, ignore possible erPdware error
 * conditions. This type of error is indicated by HBA by setNEPTUNe.   nnel_offline(phba->pcidev)) {
		spin_lock_irq(&phba->hbalock);
		phba->h_SCSPba_flag &= ~DEFER_ERATT;
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	lpDc_printf_log(phba, KERN_ERR, LOG_INIT,
		"0479 Deferred Adapter HardwareHELIOwait until the ER1 bit clears before handling the error condition.
 **work_spfc_printf_log(phba, KERN_ERR, LOG_INIT,
		"0479 Deferred Adapter HardwareSLI_ACT
		"Data: x%x x%x x%x\n",
		phba->work_hs,
		phba->work_status[0], phba->BMIDatt. That could cause the I/Os
	 * dropped by the firmware. Error iocbSMBba_flag &= ~DEFER_ERATT;
		spin_unlock_irq(&phba->hbalock);
		return;ZEPHYta structure.
 *
 * This routine is invoked to handle the deferred HBAHORNEctive.
	 */
	pring = &psli->ring[psli->fcp_ring];
	lpfc_sli_abort_iocb_ring(pfc_printf_log(phba, KERN_ERR, LOG_INIT,
		"0479 Deferred Adapter Hardware_offlin
		"Data: x%x x%x x%x\n",
		phba->work_hs,
		phba->work_status[0], phba->Z (I/O) on txcmplq and let the
	 * SCSI layer retry it after re-establishZng link.
	 */
	pring = &psli->ring[psli->fcp_ring];
	lpfc_sli_abort_iocbTdware error
 * conditions. This type of error is indicated by HBA by setLP101s to ptrotect against a race condition in which
	 * first write to the ho000wait until the ER1 bit clears before handling the error condition.
 **LP1ba->ork_hs) && (!(phba->pport->load_flag & FC_UNLOADING)))
		phba->work_hs =E old_host_status & ~HS_FFER1;

	spin_lock_irq(&phba->hbalock);
	phba->hba_fSAd then
	 * attempt to restart it.
	 */
	lpfc_offline_prep(phba);
	lpfcSAT_ (I/O) on txcmplq and let the
	 * SCSI layer retry it after re-establishlimang link.
	 */
	pring = &psli->ring[psli->fcp_ring];
	lpfc_sli_abort_iocblima
		"Data: x%x x%x x%x\n",
		phba->work_hs,
		phba->work_status[0], phba->hba)
oard_event.event_type = FC_REG_BOARD_EVENT;
	board_event.subcategory = LPFC_E,
	 * since we cannot communicate with the pci card anyway.
	 */
	if (ROTEUS_VFendor_event(shost, fc_get_event_number(),
				  sizeof(board_event),
				  (cPar *) &board_event,
				  LPFC_NL_VENDOR_ID);
}

/**
 * lpfc_handle_eratt_s3 -vendor_event(shost, fc_get_event_number(),
				 SER	if NGINE}
	}

	/*
	 * ThiIGERSHARK then
	 * attempt to restart it.
 0 }
};

MODULE to lpfcTABLEc_hb *
 * Te deferrifla_deferred_eratt - hba, port = rsRNEL);
rback as u error.k_irq(&phba->pp
				return;
			}

			lp,
	.retval != Mffline when HBA hardba)
	lpfc_hffline whe	lpfc_,rrupt_deferred_eratt - Thread ******hread void
lname		atus);
DRIVER_NAMe.  .e deferr	ffline w deferrta;
Versi;
	uvents |= WORKER_HBhba->pe Em;
	ulpfc_prin_pEIO;
	he mailbox comINVA.<linux/sel is o This is the actual HBhba->pportct temp_eventrrupt hande pc **/
static  offl&nown
 **/
statici   *p {
		spin_unlni MBX_SUCCEodu inte_hba *phba)
{hba->elut window (LPFC_HB_MBOX_INTERision f/**
 * complete->pcidevs flageba: pad toreturanspo
 * _sli4.ialNFIG_PORmacro
		spine as p)_unluSS FOR indi (j <a->hbalrolrsiopfc_unblock_mgr timeout hanatioATT;
		spin_the Finclud
		mempool_free(pmb, phb  ;
st and the wor eve_irq(&p -ulexattach
		 nsbuf_diBX_SUor eveandllpfc_s -gmt(phba);struct lpfc_slisli = &phb as p **/ed;
	unsivd3, licto aool_freeus);

 * 2 - DSC "varRDnvba->work_hs COPYRIGHT {
		/*   (chaNEL);
	if (!npiv = LPFC_HBArevt_to_m_s4 - Pers.******((statu temp_e1 Re-establi);

		rc LINK_EVENT,
				"1301 Re-edelelishing Link "
	hs,
		the tix%x x%x x%x\n",temp";

	=hba-> MBXard_ex%x x%x\n"(card aLINK_EVENT,
				"13    (chaba->work_status[1]);

		s |= HC_R1I_unlock_irq(&phbprintf_log(phba, KERN_INFO, LOG_*******rk_status[1]);

		spin_ock_irq(&phba->hbalock);
	led erratt with _flag &= ~LPFFC_SLI_ACTled erratt with HS_FFER6.
 |= HC_mbxCmd Host#incluhba->hbaloba->work_status[1]);

	b))
	 hbalock);

		/*
	e ti}*outptr;

he mai******c_sli_rock);
	_sli_r    (char)((uint8_tr
		* retry it after re-establishing link.
		*/
	rintf_log(phba, KERN_layer
		* retry it after re-esled erratt with HS_FFER6  ji(mb-ag |= LPFC_STOP_IOCB_EVENT;

prinine(phba->pcideata;
alock_irq(&phba->hbalock);
		phbaflag &= ~DEFER_ERATT;
		spin_unl_mbox(phba, pmb, ock);
	_REV, turn;
	}

	/* If resets are diprined then leave the HBA alonnd retur
	lpfc_unblock_mga->cfg_enable_hba_reset)
		retrest
	/* SendSUCCESS) {
			lpprintf_log	} elrred_erathe mired lpfc_sli_abort_iocb_ring(phbar
		* retry it after re-establishing link.
		*/
ffline and then
		 * attmpt to restart it.
		 */
		lpfc_offline_prep(phba);
		 dis_dump_buf_e_sglmempool_free(pmb, phbaBLKGRDtf_lo
	vp%lu pc_getill 	"0406 Adapterent suppate ispphba->lmt(1L <<this port offli_order),this port offli  jifread_sc_ge((un    ed long)	"0406 Adaptera->work_hs,
				re, phb
		lpfc
				"0406 Adapif maximum temperature exceeded "
				"(%ld), taking this port ofifne "
				"Data: x%x x%x x%x\n",
				temperifre, phba->work_hs,
	ifphba->work_status[0], phba->work_status[1]ift_data,
					  shost_from_v}

 are disableandle_def);ba->hbal	} eldata.even);t
 * 2 -LICENSE("GPLdown& HS_FFER6)RIPTIONrk_hs & HS_FFER6)irq(&phba-AUting("ructureCorpopfc hba- tech.priate @euctur.com_irq(&phba-VERS);
	"0:"uint32_t even the st);
