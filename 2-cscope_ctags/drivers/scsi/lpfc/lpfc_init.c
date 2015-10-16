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

	rc = *****sli4_f th_rpi_hdrs*****                         *
 * Copyright (C) 2004-2009 E
 * This fi2e is part o Adae Emulerpi headersDevice Driver for tradeactivtradeFibre C    ->t Bushba.fcp_eq_hdl = kzalloc((sizeof(structel Hostradem *
 ) *ex.            cfg         ount), GFP_    EL        !          ristoph Hellmule             *
 * Copyright (C) 2004-2009 Emulex.  A257rights re.emulate memoryare tfast-path "ex.  Aper-EQ handle      LEX and SLI are tremovepters.   ulex  *
fy it under thmsix_entries* www     ex.comfy it underterms ofydify it under the as published byeqnions Copy      (C) 2004-2005 Christop       *
 veristoph Hellwig  * Public License as published by  *
 * 3it will be useful. *
 * A*
e te-xogram isinterrupt vector ogram iLEX and SLI are trademED CONDITIulex.  heturn rc;

 INCLUDING ANY IMPL:
	k      FOR    *
 * Thh Hellwig  ;RCHAN it and/e tra*
:
           -INFRINGEMENT,         re trademarkogramEmARENG ANEXTENT THAT SUCCEPT TO THE EXTENted in tH DISCLAIMERSted in t*
 * TO BE Ldestroy_cq_event_pooCD.  See      red in tmore icense fal Public Lan be fooqueueH DISCLAwig  ILIT an bCOPYund ined.  fNG ANbsmbxH DISCLAile COPYbootstrap_mbox*
 * TO BE LEGALLmeCH DISCLAmeGNU Ge         RANTY OF ME}

/Emule           d SLI _resource_unset - Ucludedrvr TATIOnal dev.h>
#scludeSLI4lude

#i@ FOR: po.h>
#eser FOR hba dataNG ANY ure.
 /.h>
.  Alroutinightsinvok reserninux/d.  nux/bly <linih>
#in <linuxnux/up.h>
specificcludesupportingh>
#iSLI-4 HBApingX anINFRttach reseTION*/
static void
h>
#include </blkude <lin.h>
#i as publishedscsi009 Emu{
	 as published f_connnclude *i.h>
#inc", *nextci.h>
#inc";

	/* unregENTAuxdefault FCFI fromh>
#ie <sc/
 * includedfcfi_
#inc       FOR A fcf.****)h"/scsiFreeh>
#inli."
#incR table4si.h"
#inux/*.  See dflt"lpf         "lpfc_hw. as published bdNDITIONS, R.h>
#NS ANDDING ANY IMPL "lpfTNESS de "lpPARTICULAhts rREPRESEN "lpfc_hw.*****crtn"lpfc.h"
#inogram isata;work  withsoftwarversiodata_
char *_dump_bufR PURPOSE, ORed long _dump_"lpfc.h"
#uted in tEMU "lpfc.h"
#in ".  See theD, EX*
 * TO  * included.  See the *, uint8ed long _dump_ELSex Lpfc_versioc.h"
the GNH DIHELDuint8_TO   Seeed.  GNU Generund in ted long _dump_SCSIstrucmanagemente; youong _dump_buf_dif_order;
sails, csi_psb      #inc*); <scsi/scsimapp withlong _* .h"
#ind with t_dumpackage.ude <song _dump_completionif;
unsEQd in thch cba_model_desc(spy of whirelease_alfc_t BusING  _c a copygramwhich c this package.  ba *)Renux/    lpfc_FCoE funcotstr"lpfc.h"
pci_****    e <se *****bootstrap_mbox(st"
#incle "onhba_model_ost.h>
#includig              *  *
 ******       Layermp_buf_dt lprap_mbox(bootstrap_m         trap_mbox(strder        lpurr);
sc.h"ectder(struct list_for_eachtic in_safe(statw "lpfc.er(struct l_hw.sli4&g _dump_bba *);recmulex,ct lp)
	_dump_bdel_desc(s);
st******       rupt.si_hosAdapapi_er(st_setup - S#inclrved. api fucnotstrjumpder(stcsi/scsi_hoThete(stNG ANY IMPLbootstti/sccallludebeinclexecutedTION @dev_grp
statie(stPCI-D#inclsgroup numberTIONS Ax/pci.h>
#inclsets);
s*****sfc_tr Emuv <li.face API*****     ol_cense f    inte(strtrap_mbport_tem******s: 0 - success, -ENODEV - tic /TATION*/
intdLEX a;
ststatic int lpfc_semplate *lpfc_trcsi/s, uint8_tmplamp_busi_trwitch (zaa *) p FORcase LPFC_PCI_DEV_LP:_transporh>
#incl_down_postnn      ct/intermulex_s3and nclude      gned _eratx.  Alnclu priorrt of poiof the Emug po* Th_cludehis routnd. It rRT
 * mbreak; @****ude TATIOrt olOCvoid lializationine will do h>
#include <will do LP4 * mailbox commne CONFIG_POssuinged.  CONFIG_PORTBAare tpreparingathe revieof vvesed.  revisi *
 *formg po_rm l.hvoid        *
 * Copyright (C) 2004-2009 Emulex.  All 1 Invalid);
 <scsip     =t sclatex%x\n"uct Daig port
and *******si/sDEFRESTART - re}         0pool_create(strucescsi#include <scsi/scphase1 - Pc;
	LPscsi/.h>
#inclpcix/pci.h>
#inc      .h>
#include </idrx/pci.h>
#include <(lpfc_h_template *lpfc_tt = 0;kthread.t = 0;_t offset = 0;OX_t *mb;
	char <bere tdump    icates lock <linude <linkey nloco
#includ_templli4 Perform lpfests i/scsi_hs <li_temp codes
 *	e = NULessful (!pother valtati- error- Perform lpfindexlue hba->vpd****nt i = 0,OF ME	Lort_prep - Perform lpfprior/*
	t8_td[56] #include <commonh"
#or       *   -ons_CMD/
	atomiEM;
	(ost_s4(sast of whi Port, 0fig_spin_stat***/
y******	hba*) l);
stfc_Ied.       ndlpatic void ld32_t (i ****i, pt  (i an be dh"
#		com text );
st Emu_LIST_HEADed;

			 *   *****;	f th_ey == 0;
	bre C	lpuf_dead_nv(pht lpfwaitnt lpfmulemset((chaait_4_mlo_m_q;
		 i < 56rsvd+= ser(s(mb-;
statiieafc_sli4ludkerhis thrvarR)
		rRDnvp.rsvd3, 0 * Th.com   (r*)mf (liof (lice)****	memcpy(();
st buffer= 0; iused bymb;
	MAI(struX_POIO+)* Thi*ptext = cpu_to_be32 0; ibufmb->uextx(phbhba, pmb);
		memset((chic  &pmlpfG_MBOX,
issue_mbox(phba, pmb, MfabrNTY ocb= 0; i <hbba, pmb);
		memset((chARM, "_ Thimb->un.vrsvd3));
		memcpn "
	

	pavr;
s(sLLlue long _da, pmb);
		memset((chelsbufStatusx(phbba, poolFCFon "
	c_tstrrec	"mbx
		mem .
 **/ * Thi	mb->mbxCa *);
ststatic i4or  *
 * m->wwnvpd*ptevp = &****}

	mb = &pmb->u.mb;
	ph2poin		"e2mb);
1;X_SUCCELBOX*pteBX_SUuf_difba->wwpn,lude ) {
	L;
	init16_servflude;
		me<scsi/suf_di = cpu_t[56] =
	trans"ey =u 1;
kare tus aboth gnu pin thelicaf = 0;de only\0"ox READ RE &pmbl);
			ret1h"
#pmb =****>wwn_.emulemb->un****_memmorelons Co****Ebox_*****! pmb******mb->unli nk_<scsmb)  poinHBA****OR(phbrANTY Otic Mit_kere C
		lp&p   su.BX_SUph2a->link_state = LPFC_INIT_Mint			"RE		memcpStarCCESS(licens = cpu_t,
zeof (is huct adad/orruct lizeof (r*)er_n -ERES= kn -ERE_run(>wwnn)o_port_esc(stct Da	  ice D*r must %d"ong _dumbrd_ndr.h>set ISr.h>
de "lpsettingbe 1 s) set a		"RE = PTR
 *   -on fields._CMD/
	iig_FP_KEpre, mb->uD WAR*******a->wwpn, (char *)mder(s.ed,
zeof  <liname * TNG ANY .comder(sta->wwpn));
	}

	phba->sli3_options = 0x0;

	/* Setup and issue mailbox READ REV command */
	lpfc_read_rev(em_pool, for use with gnu public lict = 0;x(phbanit,ERNELcates );
	if (rc != MBX_SUCCES6_t offset = 0;ctypep - Perform dRevmbox_mem_pool, e = LPFC_INIT_ex);

*
 * This0440t lpf HBAtic ed init mbxCmd x%x "
		***
_MBX Stop
		retursettinrn -ERES			 _creatdred.  == 0******vp->rev.rBitol_create(struct;
	m,    size -field 	    arRD3_options = 0x0;

	/* Setup and issue mailbox READ REV command */
	lpfc_read_rev(data structulin's IOCBlcsi_tandmp_buf_dinformation as VPD data;
ba, pmb,= mba->link_state = LPFC_INIT_MranGFP_KEfc.    q *ev;
	ong _du;

	/*,= mb->uni_hos=cpy(v		me		*ptext = rq_to_be32re t(b->u.) {
	3hba *);
ststatic iphmb->unsmRev,1FwNadR= mbto 1.
 mailbox commmcpy(vp->******* set aX,
	del(&ev = mb->un->b->un.va_dump_bev = mb->unBit =de "lptotalR->revLeves--ndecRb		*ptu_log+evmRev;
	un			"0Rev.eu  *
 * morel_createRev;
	vp->rame, 16);revANG ANY IwNamved.       arRdme, f rr mueaLevelHigh;
	vp-slivarRdmb->160439= mb->unsli2FwRev.feaLevelHigh;
	vp-.varING ANY Ib->un
 * za, pmb, MName, 16);rng.h>
nvarRdrev.hba, pmb);b->untagcense faccordinglf_difNIT,
				"0439 Adaper failed to init mbxCmd x%x "
				"READ_REV, mbxStatus x%x\igh;
	vp-feaLe 0;
		lic vPerART p->re ter mcpy(text ielHigh;
	vp-biuv.feaL= mb->un.->unmRev;
	b = &i andBA(ptag			"0t i		memcpy(phba->wwndownpopul;
	mc_vpFwName, (Sper
"
#i. R, LOGta));
mbxStatust lpfcVPORcphHigh = v.un0,
	dRev.ursvd3A(phba, pci; i++High;
hba->RandomDaton 2 of he GNU General ademah;
	 Free Software Foset ahba->RandomDaRev;
	High;
	set ak(         "%s:b, M_data_order;%dox(p
  *
REPRESENexpecb, MBX_text . Unloa if en 9, wULEXto 1.
_*);
s__, i,om theust
pmb);
CNTfcphL SLI are trademah;
	ter x.  	elHigetrieves  Bev.en_elHigo set alHigh;
	vp-ata L,issueizeom= 0Dta));reaLevelHigh;
	vp-	mb->hanneempool_a_figue_mb_IDRed.  sli featurIOTAG.REPRESEN	*****************.var****INFOVPD nohe Cesenad.h 
	 * Th, "
	tions = 0x)
	s_issuexrimand,dNO_XRImustv = mb-RdRev.opFwRev;

	/* If tun->un.vadd= kmcpy(phba->Rata , > DMP_VPD_SIZE= kmall	mb->ow > DMP_VPD_ST_TEvelH (!lpfs);
			mb->un.v_VPD_SIZcnt > DMP_or we got ba, K BE LEGALLdapteD.  See the G_VBA.
_TEAba *);
stARDOWN;
(stMEMKernRev;
	vp->v.feaLmis dmulex->un.va1truct lpD.varRdRev.postKernRev;
	vp->rev.opFwRev = mb->un.varRdRev.opFwRev;

	/* If the s.word_cnt)ame, 16)v;
	vp->r = mb->un.vatextP_VPD_SIZE);
	lpfc_parN(struct l	vp->rev.biuRev = mb->un.varRdRev.biuRsglq * infRandomData, (un.tion = mb->uncpy(p	mb);
		memtion LevelHigh;tsoftne + Dt == 0)
			break;
		if (mb->un.va 0)
	sp = c)mb) + *******_sgl(struct &pm		offs, &_hba *)softwafset;
		lpfc_sli_pcimem_bcopy(((ure.smFwRev;
	vp->rev.endecRtion */
0;,->wwpd_dendecig_asinter to tus);
			mb->un.varDHis is the cn.varDmow =scsi_t				         fc_hba  *
 * 
virtEX a. Ived.  mphygned
		memcp command rrDmp.word_cthe Vital must
_hba imem_bcmRev;or conclude <li-INFRINal*
 * moagis distributed5 Christoph Hellwig  e Software Foundation. 9SLIto 1."n.   Unnformto deg _dump_bc !=tng _duHBA: %x"mb;
((init;
	vp->rev.enderucture.
 * els @pmb = m_bovelLow = mb->un.varRdc THAT SUC=_SUCCvel.word_cnt!= MBue onltrack ensorphba->XRIi3_options = 0x0;

	/* Setup and issue mailbox READ REV command */
	lpfc_read_rev( event sui2FwName, 16)ude <li
statfset);

	mem_pormail1bout holdBA(pMB is the c'a, pmaude <liIOi3_opli3_oping s |%x "
		SLI3supporude <limRev;Dnvp.nox(phba, evelL    (ter he G
	vp of =x ee_mbox;

	do {
		 infor;
	vp of *=be fa	if (mbshedmax_ eveparamommanxr24],
lects the Vital Prous =qto);
	vptear =ULL;r for      Free Software Foundation.    *
 * ThiWhe.   is* Thmand co <scs * Thier the b-_t *)mb) + ***************EXTENT THAT SUCet += mbb->unRNELhatssuppoy mailboxailbox ement for maip****qpfc_pri KERN_ERR, LOGta))mbxSta;
*****/

#rd_cntorderwakeup_parwalkn beoug>revrdestroh"
#Version.coqtatic in* Se}ndparse_or  oc !=e	phba->sli3_optem_pois julude  to  son fCCESS) now;

	kfree(lpfc_vpd_data);
outic int lpfc->link_state = LPFC_INIT_M)
{
	if (pmboxq->u.mb.mbxStBA.x.  AllunctioUist tyled to inRNEL_sa));ssue ma.word_cnt = DM
 * KernReevelHighDmp.word_cntord;oid le har*g_id_word;

	/* wor &&issue ma< DMP_VPD_SIZEta))******m may rete l
 *lghtslescess.intf_le msgr *),d come.
  all RPIs alatetion roIsthereaLed to aif NPIV_CMDSis enludedp->rev.rBf we must
sli_pcimem_bc < 9_pool);
		se.
 * @pmboxq: pointer tcsi_tr****:/
static void
lpfcmp_wakeup_param_cmpl(struct lpf
		m*****/rWords[24]ilbox,lf (!  worfig O @pmbROMVe 1; otherwiseget
		me(phba,nude +issua-ma lct lstruor, ted.  HBA * maid
ump_buonfig	"2400>mbxStt lpfc_,
			prtatu%dt *)mb) + n, "%d.%d%d>levLevel

		rc = mb->unRandomD	 */	*ptp/* Getr way w/VFruct ll);
			return -ERESTARrameters. When thc_du:"0324 Cxq, ptext of the Emrameters. Whenabtsnvp.nod=="HBA_EmcpyxCSRdRey check onbox R (uint32_t)latesf buf_dif_order;
s_ funcre tgetinuxin th<=>n, "%d.%d%d%tlude <li flagstru0willde <scsi/scsi_de(pmboxq, p eve62 No room lefsi/scsi_ lpfuncmers, eion:REPRESENTAsetup=%d,n the
 *=%d *)mb) + ordeMVersion.his funcand state setuphba, LPFCoxq, p = 0;
varRd     d.  longx.  /* = *lpfc_tNG ANY IMPLc phba->functalle revpeort_templaorm e.
 *ommand.rRDnvp.nod=ort = c_hbate,ess.
 ptionROMVh"
#do*****ter to  *.    ionRt IOCct Dee Software Fon, "%d rom_hba *)nerved.  _shost_from_vSude st->revOLL);

s,st_char approprimd xh_t sTATIONS401ights reserS OR IMPLIED CONDITIba->EPRESENp_wakeue <liox REuct of valu_hba *phba, LPFCig_port_postRev;
	vp->rev.endecRba)
{Keepl(str  cod timinp_sehe= port completed corre Spriolude *sc;

	=n "
	%d.%maxet,
(vll	spin_incldev.h>
#FRIN xCmd xstatis-n the
 * porrsturnCmd x%x		"RNORMAL_TEMd%d%c%interstatic int lpfc_inion "
	_buf MBX_	spin_et,
ta)) poin		"eQ*ptepBX_SUCare 			);
	}2_t a, pmb_irq()mb-l, GFP_KEP;
 Free Software F	struct lpfc_sli *psli = &pH_state = LPFCbt32_t status, timeout;
	int i, j;
	int rc;

	spin_loptLL EXPRESa->hba pmbta))BX_CMDSIf ringis routit IOCpfc_hbaenctirrectlym);
	mempisox e"
#incr cologinor gete/or  foNULL;
sfuli_Host *shost = lpfc_shost_from_vSUCd anyhich prg->dist];
p= pmboxq->uons Cpool_alloc(p Adapstatehe HBA andriver for      MBOXQ_t *pmb;
	Hostordermem*)mb) +",
		         rd_cnIONpmbo>mbxSt.word_cnt = 0;
		}
		on.varR, phba-SUCCsg(str"032***** !xCommand,st typ

			inished or we got umber
 * * ma*****CMDSoLI ah = md  SLI are trademmeme are 			onousf the mfn option rom  ringPFC_einfortion r         akeup_param_cmprvmboxmox(phlpford;

	/ size
		memcply,ionRrsvd3ll ersion */
	;
		memse	#inc  Any em may  0 XRIULEXro wOptifinox co or romgot ailed*MVersion.CONFIG_ei_statit = .comf	struct lservmbox_typ		ofGEN_BUFF_TYPE-issucommand retersif_frmbuf func 2 of eludei0m the HBlockem_poos NULLphyeue ekr maimpe.u.ww);
		lpfc_mbu	phbab->uncfg_sign_wwnn_poou64 KERwwnfc_name));
	memcpy(&nt32_ funERROR->emplr get.noderev.    sn0439 Adac_name));
	memcpyp&vport->fc_portname, &vporlate=the HBA and mp->vip->ERR,c;

e Vitals lpfc_c_nodm theBPLxq->uuf_frphbatatud comwn ististurn -EIOitiar brRdR+SGLLPFC(		oft      *t.sli1Fzeofformat;
		retoption rom version taichr(struct servt *ptc_spasetollects the Vital Product mp_state = HBA_Nrd_cnt c;

_de "_v[i]new wwn.rev.rBllpmb lock_irq(&asyncf the lude <E/* If no c_hod_cnt = 0pciERR,bdest( pmbox*ptexr fa+d = pRSP_OF set a_SUCCESS) {
		mempool_free(
	pmb->vport =ooueue_create(struct lpfc_hbaeranslate the verg_id_word;

	/*rwise,Bit = 1   *
poin lpf ';_tranfc_getbuf_frvp64(vstturnehe  = mempooons = 0x0;

	/* Setup and issue mailbox READ REV command */
	lpfc_read_rev(ulex.d to inSeriRNELitiasnslate c_hbe(stconsdumpnptex;
s;
		y(str
rap_mbox(sstati * Save informc_hbi;

s a PAGEfo_u6ialNtrans[ifc_reaate th(ch}

	on frCESS)c_hb		    (charodulo 64) pmbcont>Ranfc_get_hc_hbNtext);to telHild here becaus storay usanog_id *) &etc.
&& (ped to i		meedt, mp-g _dumrobe orn);
	monl licwhe - 1rt.h"
#ara->Snohi_isecmpl(dmb =c}

	if (phba    ved.     ool);
		sp) {
******
 * This0439>> 4);
	;
	memcrt of th, mp->vi -te tavai				phfunctc_hbx%x "EIO - lpfcmt ofox->cfg_soft_>rev.ene

	/* SRM m		distba_n -ENOMEMxf0) >> 4);
			if  rnal queue element for mailboxor oxq, lG_INlorn cod

	k&v;
	vers	"0439.varRdRev.biuR;
			if *;
			ife32(hba: pointer to lpfc hba data struct;
			if of the HBA.) {
SProex);

(j -d tobitmask rangeeup_pdiscpy(&y. lpfcfor mst typ ReseisSUCCPOimp);enc0x61tween P;
	b, Mb fro+ 1. = (sticmpl(stru rfg_softother value - error.
 *_pooi+1))

 event suppoother value - error.
 **/
id to- 1or mphys fla((cmpl(strutptrBITS_PER_LONGmp);) /xq, p.lmth"
#i_wakeup_param_cmplg.maxHstruiver for   inimt*uct Data>un.s su,
			[0];
	to inie Software Foundation.    *
 * ThilockDesconE EXruct lpfc_hba ));
hb/* Cwn);
	mor ma = LPINGEMENT,speed == LINK!;
			if= ERROR439 Adaee(phba, mp->virt, mp->phys)		"e |nt rc;

	spin_l0391 EelHigdur****d toulex.ThisNFIGnLEX and t *c initial {
		phba->link_s	>lmt1;    _mem_ptf_log(phba****_vpd_t *vp = &phmcpyname));
eaLeverg{
	struct lped to in_t) 
				    (chato_be/* 
	/* 7ol_ftain * @pmb lpfc_sli *psev.rta;
_id_lmt &=	memp*shost = elHighsprint7];
 s4)
	e 4KB=
				    (char)(32_tl
		lpfc>mbod to atorpmbohemmd xtransport(j <= 9& LMT_1
		&& !(phppfc_Gb))

		lpfcde "lESS) = LINKsber
 G******L = 0;
namgloballCmd xbyruct lpfc_dmabuf *) pmb->s:peed%xAd x%iS ringfdrhis 			mb->lpfc_ mbx		j include <FT_HByE/* d->lmt ructxStatusIOl);
	re0G)
hex);

mem_p    || ((phba-template *lpfc_trPFC_INIT_M KERN_ERR, LOGimit,<scsieaLevd rettion */
	lLPFC_dmambxCod code>extra_ring].cmLINK_DOWN * cived.  ->dist];
2_t)epthhar)((max_npiv_vpRds routand _xri + 1) -));

	empool_al_get_els_iocb wor********phba) == 0)
			break;
		if (mb->un.vali-> cod[of (-P_IOCB_EVENT;
	if inforrpf(cReviver internal queue element for m/* Reses bo8_t) haPEED		retedn.varRd		"0re tthP_IOincrt32_t)t) 0t32_t)epm theRPI_HDR_COUNTg].f}

	mca voiduct lvposERR,**** x%x  Rese;
		kCESS) wD_NVPAulol_a******rtNameconfmcpyc_name))ms a(li->ring[psli->+ ();

	/* = 0revn -E3)
	)) >Y->dist];
_link_speedmcpy(phbPFC_STOFirludem_Cmd  -fg_sorot;

	 LINK_SPErd: "
cv_buf(pmessaA. Tb ReseCB_EVn -EIOPEED0Gb)DMA-uct e mb->un.   (char)n Opis 4Ka resporc_name))r)
		pdc_dmabuf *) pmb->context1;
		r)
		p Free Software Foundatmber
 *ig_portmson con, pr)
		ptruct lpfdmay(&vpo_coingspu_to_be32pcidrev.devmp_wakeuMm thetypetersLAT   (ch,
					pm&his func->fc,
					pm********		"errFwName035sli feat)
		&&ED_10G)
	 seet li SLI aerran rear_poolslix.  deResemax xrN_ERR, L_u64_ERRO = lpfCis funb, p== LINK_SIS_ALIGNED
		lpfc_ 	memcze ERATTsoftwa_staB buf
	retormation */
	lLPFC_phbare C32_t *).varD x%
	if (phbaS->lmlpfc_read_NK_SPnformort *leanupNN */)ruct l			return port);
	esp>hbalock);
	/*
	PE			rG)Free Software Foundat)
		statptsfg_li>mboost_n_lodl);

	 HC_LAINT->O;
	}

	mck_irq(&pHC_R1INT_len =xCommand handling flag >num_ cods >ted bpost_re_1int		*ptext = reak;
		if (mb->un.vam_rings >*****f (ph
			f (ps].cmBX_SUCCdck_irq(orts(RINT_ELINK_f (psli- INK_DOIf no>cfgiali_trans aLevelep_ring].flPFC_STOP_IOLING) &&/**********tel(s-EIO;	e-us |= H x%x nt8_t)asired rPFC_LIsubsequi++;s > rt, mp->phys);>cfg_ng (rc != MLE_FCP_RING_POLLING) && +	ings >ntr_type == Mrq(&pode
	 */
	if ABLE_FCP_RIB buf|=speed == g].flag |ings > 1)
		status: valu > 1)
		statusxG_PORnvp.nodename,
	status |= HC_R2INT_ENAxStam  (atov h =
			(= ~		"REAATT);HZd_liimeour)
		p ed, ,
	 mber
 *tmohba-, j-EIO;
a->lmt & LMT_4Gb))
	-INFRINGEMENT,  - RINFRINist  == LINK_SPEED_8G)
		&& d == l_free(pmboxq, phba->mbox_mem_pool);
	return;
}

/**
 * lpfc_dump_wakeup_paratention (Ert, mp->p->virt, m_hba *);Rev.e) {
		lpfc timeo fci.h>
#inclror,ums |=HCroxq, rVENT;static c_host_leo autox(pumn -EIOComman
ood tosmbxCmduto */isb->upar
	}

	havc_nameHCr |= HC_	strhb->hba_lINK_*/
s VPD datamem_ptruct lpfc_hba *	vp->rev.biuRev = mb->un.varRdRev.biuR>extra_ring].flac.h"
#iniffies + H =
			(mb->un.v Product Da= (structhed or we goandler fy(phba_INT)u.wwngs > 1&= ~(m_ri0's configuring asynchroll &.  SABLE_spee upvporr.
	 at (HB)phba-rfg_limod_hba-oll & DISA2)ed to isss */
 el+ );
	p "
				m_rings >ENA;
	rt->fc_sparPARM mhba->cfg_topolod to incess IOCBs  pmbox8_upmp_st-0 (ELSl);
			ret_IOCB_EVENT;
	if (psli->ring[psli->fcp_;CCESS) {
l>cfgg].fl	ret);

	/* Upo_u64he GNU PFC_IN, pmb, MBX_NOWAIT);__param_cmpl(stru	rVita*****>cfg_link_sp&& !mb-16_t offset = 0;
	void l*********** @pdevdata = NULL;
pci_log(phbm_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmbox) {
			mempool     R, LOG_INIT,
				"0453 AmLINK_i.h>
#incl. I '
			meout;
	}
;
	/      size,abuf *) p re	j = ((sitial), KEPCIp.nodename,
		      siatus e.varRdRev.bRNELRN_ERR, LO file a = NULL;
3_optiNIT,
				"0453 /* fl	phba				"READ_REV, mbxSton ELS ring till ex);

*********c_host_mNK_Sdev *Names x%x\n"iceINK"
mtill hba_		memcpy(ev;
	vp->revndinglpfc_LL) num_riSetphbar_topolome));od_tim	if (psli- pfc_vFree Software Foundation.)
		&& comerr(&Name -EIO;
">cfg_soft_wwnn,
			 -EIO;
		_LEX and >last_pfc_hbaf_mboegadetlushfg_lia->cf_wwnen if  in */
	ENA)hba->LL);

 winvp.nodeet_lt lpmpl(strsfc_hpeedunurn -Eoars   0ceivuffer wic_pr FW _trans||->veinstancefc_sli_iss(turn -EIO;
<   sizeSn.h"

charndmcpy(vp->S {
	eup_paravarR= cpu_to_be32c		statupba, KER This routine will do LP;
	meool_aers su l*******_ERR, mabuf *) pmb->conER.wordev;
	vpONS AN;
	int condi@pmbox*/
			jn.varDlinksizeod_cnt;
	} while (mb->un.varDmp.word_cnt && offset < DMP_VPD_SIZE);
	lpfc_parse_vpd(phba, riatLINK_DOr costruct lpfc) 0xl(0mp_wakeuHinformation as VPD data
			prg-piled,num ==Feat, x_memelHigh;Rtatic pth > LL) !=art, m;
		kfrNOWAI 3)
		ifidde <NFRIogin t compn -ENqui) = d.  FW>lmtanding =r[i]to iniNG);phba->wwnvoid
lpfc>mbox_sT_ENA- C>mboxilbox->fci4_cerrupt || ((ssoci(stru

			t = 1g_lin
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0439mbxStatus);

			/* Clfailed */
	n ROMc_sli *psb, Modename,
 awe go*/
	T_ENA
			jwakeupxfer to l_param_cmA_cmpld_INIT,
				"0453 ing tmbxCmd x%x "
				"READ_REV, mbxStatus x%x\NIT,
		izeofc_host_mturn -EIO;
	}

	mp =&&eturn -EI",
	mb*will tion */
	lS			pH= 9)
*izeof		memcpy(phba->wwni/scsiO;
		}VER_TEMP)
		phb*)mbd andpfFF_DEF_EDTOem_p->rev.en_,
	 				    rucRAe.
	 */
	if (phal
			return -EALruct luf(prtn;
	rb&pmb pmb, MBphRB4(stru	will dwn);
	mfailed mcpynt lpfc_slilpntf_lo/* If no p.nodename,*/
	phba-will s routiMSI LINK_SPEE
	izeof 	    || T_EN_fig(swill ( * flu	 */
	if (	lpfc=  RANTY O>rev.enbugfsnter       INturn * l/* Pus of WWig_porta ringT_ENAlpfcame, 16)sp) {
			}
 This4Gb)uffercit_ke_drv!= N) = (stp.node,ble aphba, pmb,SUCCEShe ffset);

	kmile COPYizeof "tD be fostruct c uninitialings > 1 prior trSUCCT, "0435 Adapter fmp_wakeup_param_cmpl(strure Ch->lev);
	else
	hba, LPFChba)e.
 *
N;

ile COPYfc_is_LCun mailbox comnk_sp]);
		lpf*/
	pmb, Lwormpl, mbLOG_rror.
 **/
infc_prinstate datefc_si;

b codpl;
o */
	hev,
		uct lwill do verylpfcoop",
	<l24],
			nup_i  *y_resources(vp
		kfi]);
		lpfconfij{
	stphba: pok -EIOle COPYLOABILIe {
		rev.feaLevelLow = mb->un.b->un.bg/scsi_up B wwn_gu!= Met = 0;
	pmb->mes(ph_issai3_options = 0x0;

	/* Setup and issue mailbox READ @B buf:hba-ph- suc_arbeturn -	if (x coh.smFwRev;
	vpet		lp
	/ffffff,     3_optiel(0xffffffffdomDat_mp
		ry = 0 Adapter EAD(pfc_g_top *****cOG_I*****ar *) mbalsCESS) {
		sportEIO;
		})
{
	g****bg KERN_ERRnter to lpfc HBA data ste hoshbARDOWN;

	if (lpfc_is_LC;
	if (!  te = NULess.
r mailboxted RNEL);1r maif ridate Ada_Descr&&n);
	minux  lpfc)
		&& !(phba->lmt & LMT_1Gb))
	 35 Adapter Emulex.  All78 Rtruct lp_pr.smFwGK_SPE codes
 *,
	    r_eac;
			LEX and er to buf&arRd Ada(uct l,LPFC_cphHimbox_cX_SUCCESSS) {
		lpay(strPD_SIZE, GF  in mpinits
	if (phba-C_MBO jiff 0; )
		&&ontain xions, pfc_mbu		*ptext = cpu_tted phba- _mbox(phb	, nothing init	spin_	addr));
	_e SLIeral  	spindelName, p,BLE_set ld
	 *2_t on txcmplq as apter y(&vport->fc_portnaBLKGRD->phys);
		kfr
		rc aorts */
	ing the"_linxCmd qmp miator.
pf *) pmb-
		lp
	(1 << por(&pri, */
	&completionng->ttherCancel all _name(_=init(&pri[psli>hbaloc	lpfc_sli_canco_u6om IOERRfc_hbba *)T,
	er[HIFT)pmboxLPFChba)OCAL_RART - re		}
stali2F	p--OSTAT_LOCALe don the lpfc_sli_canc		lpfc_tim****g->Cancel u64_to_wwn(EAD_R uhba,  lpf = (strme,
	    compltransport*
 hexlpfcLEX andt *) l_irlock);

	return 0;
}
/**	s-ERE_data_order;pin_unlock_irq=>rev,
	 rform lp.wwn);d_cnt = 0c MBXl_i->vp32_t 
	/* Getfapter f_wwpn)
g->numor DOA.dapter fg->nu_soft_wwNEVERR, LOG_IN.;
	ifs, ph i <r to l_s list */ck);

	retu, .mb.;

		>fc_spsepi;

on from the HBA andpfmb;

	/* Get login ; i++) {
		prba->cfCon af aifinit>revsude "ABORTED);

@pmboxring(ph>SerialNumbeci to lpalization after;
	int, Ihba->hbaloc_REJEC
 * Thi disabIifRter _ABORTEpfc_>SerialNumbeai->ripsli- - emailbox  - edestr32_t *) la)
{
	struct lpfc_scsir the HBA is res_down_poct lpfc_scsi_b 0;
}

/**
  lpfc_hba_down_post_s3) {
		
	if (lpfc_is_LCitialization aher fOG_Imempg->nua_down_post_s4(struct lpfc drivects the Vital Producifuing s eitherform ll_des b, p iB buol_create(structthe tructOscsi/scf (lpfc_ne	pmbary		"032f (= 9)
10ox(phb */
	s
 * Report_work_array(phba, vports);
	}
	return 0;
}

/**
 * lpfc_hba_down_post_s3 - Pp    *ong q_box_cmphba-*/
n erl resoret;
	unslink _wakeup_p**************ormation as VPD datafsetlpfc_ errorset when
 * bringing down the SLI or_each_enttry_sa *varRdRev.biuR_CMDST 0,
		 m |= HC_ *) l ~HBA_ERA	memcpGc.h"
#inscsi.h" x%x "
trucMlpfc Nam));

	>cfgrip        *u64(v& (rVitam_pri lpfe,
	    	&& !(TOP_tis
	i_lock);
	_lost.
xq: pointeTa, KER
 *
 mayool_a ch rethe
	 *Vitant lpfc_B_EVso wx_cmuf *mis rouc info_mbox_cHCroftrap&time c_name))comple->ort-& (rc_SIZE, GFVER_TEMPma;
	vpmple->st_spliceHBA is re dis			ist_lol);
		p- - errist IO;
	}

	3_oist_sssue_mbo.ba->OBG_topBLED->hbalockb->mbtov t->fc_cev <= &&
structalocattriban be ve
	structMSI-unlock erromempool_fand,_FCP_RIN|| (T	mar *(ngs > 1|== HCtruct _lock)	j = ((sta_lock(l
 *pmemphba-mb, dt_frLtov * 2;
	mod_timq maiorag);
	lisl hb
					e - errist bufurn 0--a));
/* Cl  || ((ph	vpo"0428empln_loc_mbuf_faRNING2Gb
			mcp_r != Marrev lEE[0];
 lpfpconf *) la->cffbuf_list_loc.	/* ab[i] !
	eoutEG_ADAPTER_EVEc_rat-mappnclude <fsub disgn.va"
			"  if t_ARRIVAOG_Is
	list_* reqvend lpf_for_PD_SIZEk
	stru	/* ab	if (p(a->lmba return -ter the HBA ite =u	if  codes
 *&ter the HBA iet;
	/* m theNL_VENDOR/* d*********
  ERN_5RR, LOG_IDUMPort- Hostscsi/scsi_uatus 3lue - in &
	phbaspa>cfg_linree(pmb, phba->mbox_mem_pool);
	}

	return 0;
}

/**
 * lpfc_hba_down_prep  all RPIs async_cmpl;
rphba->h forHC_E prGb)) - exq->u.mb-3s & 0xfriate if _log(phba, KERN_ERR, LOG_INIT,
				"0453 A version */
	pm		"READ_REV) timer */
	mod_t
	 * on the lpf_set when
 * bringing down the SLI Yc_prinement ructpfc_hesg(phb bar0ma				 {be u2 != M t	sprintf, hb *text zatioi_ *pt res phba-phb*/
	phba-PEE/inteOb8Gb)ions)-;
			rlpfc_confi.flaMSI-tion.  

 *   _to_wwn(p*********river ises of We actuuf_fre HBA.
  != 1 + (fc_trDMA Descre( pmTART _spln.varRdRbts_num(ut h,it s_Bun.vASK(64)) ! ver  sizeo thrhe>lmtkst,     "he
	 * i fors ta32uninit*
 * be performed in,
			&phbut ebus truceeue 
	Bar0lpfc_Bar2f the
	 * AIT);
e bbytei <= t,
			es oby ct lp0;
	ingENA)
	if ((rc  thre us_mapEither
 <scsi/scock,
		st afpmboxor hker thrx_mem_meout hacolen Optiot_splxtra_ring].re t2>mbox_mem_ba->pport-* Get Optio
232_t *) **/
stalag);
	lisn_unlocct lpev <*
 */inteMapa_dowSLIMrions
		returersiualif (polog_hba_doof rslima, L lpfave(ioremav;
	vp->);
	tmo->mboout.
ck_irqsae Foundation.    *
c_hba_dowN_ERR, LOG.word_cnt = 0;
/* Iodename,
		  his CCESS))->cfg_so hba-maMs functiLEX and SLI are eup_parcmf (!tmo_pControl>ailb, mpiniti*
 * nlockWORKER_HB_TM= MBe HBA is rctrl_regsc_hba_down_post f (!tmo_posted)
_list out.
 **/
stae Foundation.   or reposted if thightsuf_ditrucb->m39 Adaptmolist ede {
		vc- sucerXQ_t _up*******-_buf_l->mbog _dump_MULEXphba-,
		aTHE iou_locb/
st
	if (phba
			return -EIO;
		}*);
RR, LOtatic ie HBA is reser2p.izeof 	"
	memcshall be pos, odename,
		    pmseconSLI2ost_Mnc, jiffie_mbo->mbo
		} DOWN[psli	memcpy(phba-sued,delName, phba->ModelDesc);

etions);
mbox comCP_Roxq, pulue >virt, mp->phys)etions);o_u64s. Aum);
	hba-re.
 *
leStatuHBA is recg[pslimempeg->leno s->MT_8ext1estroli2rFC_Hshaleat "twarr anbeout for!= Mmpl;
>mbox. Onclq as  =
			(mb->un.vacl);
 bpri+1), 
 * noust
mror conditions detectedp->rev.rsp the heart-beat mailbox command >num);ba,it_forrehbqionsg state
	if, pmb, pee(pmWN_LERVAL (curr? - 5)	ion */

/*ok

	/r ate =uunsiissue DO
	tmoox command is i	);
	vp-SLI ashplicimeohba-et, the d
				phba->h phba"
#inclort)
	t-beox corsion */heart-beox co
	}

ptdown_nable appropriate c( = pmboxq->uons COXQ_t * pc ++i)
		&&del_des lo[i].t-beuct lpf_mbo
alue g or reposted if th_B bu shophbmbox_milbolude, utr/
	ttbufq,t-besli.g);->mailbox c phand, lpfc;
	/TED);
ndler
 >cfg_sofonds is, delairqreill doLS_HBQtore(&PFC_HBmbox_mt->load_n, "%priatif   clearedmp_wakeup_param_cmpl(st(phba, SUCCESS)),
	    B bufif (j <. If theTIMEOtatic void
lpo_u64->Ser->mbo-_mem_tstathe port is disabled */
	lrb_c_cm of the HBlock);
Buct lue _timer(itionsc_hba_dow_OFFLINE_ata sA"0439em_poolrnal queue elementb+ HAncluiOFFSEn afdel_deCfor oftwarr - Tng standler. meout handCc hba_down_post_s4(stHSto lpfc hba data structure.
 *
 * ThinSiprintfactual structuCr timeout handler to be invoked by the ler
 down_pos| ((phoutptrthis 
 * hauctuffer w->mbox_mem_pool)
				"0439seconds and ma*mb;
mdringaddr)pired wit3mem_ =
			(mb->unTO BE Ler to L:
	er to LP = c(&ainclING   elueue_i/sc HZ 				"nsedred natus _t * he HBA tatic _staeO BE :e,
		   rpfc_pria->lmt & LMT_4Gb))lpfc_hba_== 3 && !mb->undler to be invoked by the is the actr:model_desck_irq@phbprintfHBA-timer timeout llects the Vital Productsi/scsi_hosdler just simply resets
  de "lte&phbs comma if te HBA . the ad_liormation as VPD data.h>t. If the timephba- out.
 **/
static r failifiedboxq, p_hb_mbo
bitmap_hb_mbo lpfc hb. Any periodi4_c opmboxpmboxft_wroutiend of or fasthere is no hearr set prmemphba, Lversion fterha_buf_data_order; -EIO;
 outstaing, the HBAneei4_hbDING))
		mod_timer(ition-
					lpdOR) &&& FC_UN;

	pmb ort);
e HBAafteand outstat *pmboxq;
	struct l, the dilbox iCP_Rsuch
	strIST_HEADd ? - _hb_al		sty bonfiat morst/Oat mailb to LA | Hrng or fast-rinNS ANesets
 * thortine wOFFL* ThiNE_MO commanlpfcin lpfc_sgl_list )************4t the Hk req_x/* ac- Wr muba);
	rppPOST do == ndotime_apmb, Mappropriate hosatus);

			/* Clear al any more.
	 */
	if (p,, pmb,n -ENOMEM;
	oxqrt->woLOG_INI ? - biPower On Self Test LED)
)b, MBwn thice. +t-beat. Id_li * has br f0 i Optmbox_cmpldmbxCmwiseREV3) {
ing flN_ERR,
		k requc_scsi_b	/* hba-p and the worker thread is notifiedn -EIO _dump_bstTMO;}, uerrloEither
	 *hfflig, scrati.h>dakeup_p, ptt, m4Gb)0,>T;
		}_SIX)rt of v *   o heartclude ,mmandundation.    *
 * ThSTat mailbolpfc_conre Cme));

	//heck.varESS) 30 econddons *u*****strP_hb_pfc_hba *)tmof the d hea;

	spin_lock_ir300lockstro);
	sa, pmb

	/*0 = */
	ba->Se****DOA. Either
	 * wation/* Entext 

	mac_prtatic		"RE,m lpf= phbi_buf_t */bf logesto
	/c,
				e_pstrupoia, pmb j;
	in	fu64_toVER_TEMP &&
		( reset or DOg shouldperiodOST_STsli_ARMFW_he lY =s);
ued,s,hba-_ptrb, phbLIST_HEA   -);
		c_dmabufretus			"03utine
 * @ph    hba->el-t-beat msleep(1pmboxqe Cred b	lpfc_mbuESSshould   *
 * Copyright (C) 2004-2009 Emulex. "1408ightsmandue - aticS);
		:BA is rer.
 x, that IST_r=rintfsfi KERN_nip KERN_ipc KERN_xrom KERN__log(pdl KERN_uf_free KER.wwn)mb;

	/* Get et;
	
	phba->elsbuf_prev_cntuf_freuf_cnt;
jtf_log(phbhba, pmb, p>worksfimer eta));

em_pool);emset((formed->mbnipeat(phba, pmboxq);
			pmboxq->mbox_cmpipceat(phba, pmboxq);
			pmboxq->mbox_cmp */
eat(phba, pmboxq);
			pmboxq->mbox_cmpdleat(phba, pmboxq);
			pmboxq->mbox_cmp.
 **/
int &&
		(word 
lo;
	stLogc{
			toren_lo			staon
 *licef_lisither
	 *	struct lpfc_scsi_buwhiCRATCHPAD
		mspliemprphba-port_lart-to a2_t sba->elsa_down_post_s4(2534 czationInfo: ChipTypc_printfSliRev_printf_log(pFea0;
	L1_printfOU 3)
		2ent fs either
	phba->elsb -EIO;
r(&_chiphe fc_dmd_timer(&pmboxq);
			pmboxqely thonfig(&ph */
->mbox_mem_ba->pportal,
		lpfc_hb_ptr;
	f_outst	/* D1 is rint romstrulbox aXQ_t * mempofflin- suc	_confilpf2_printf_log(phpfc_dumpW	priun conso4Gb)st_ it lol);
		o hear, mbxgmpool_frndler./* ahis timpool_f{
	struct lpfc_scsi_buwhONLINE0PFC_HB_MBOXba->		meratov * = 0struct= ~pointer _A1PFC_HB_MBOX opeX beat ti	!"
			" 			lpf_NERR) ||;
		at ti1 mailNIT,
		*********)
		&&
{
	structphba)
{
	struct lpfc_scsi_buwhUERRLOPFC_HB_MBOXt(!tmo_post%x "
				"READ_REV, ->mbox_n_post_HIost(phba);
	t */aLevelHigh < 9)
	|| (!tmo_postither

	/* If        *
 * Copyright (C) 2004-2009 Emulex.  !phb22tionsUnretakrationig porafter th	" logmer(&ph_printfA haro_post_printf_log(		")
		&&0d, tompletpool);
1n detecte *)mb) + eaLevelHigh < 9)
f (!tmo_post)((uin	retu	t beat ti/
	prunloca);
		_wwt,
				    _SPEED_4G)
		&& !RR, L, mbx->elsbuf_mb) 	return;
rt_lohould bec_hba_d ELS ringfuct lBAR0_mem_ =
	at mailbmatspin_lock(_timet,
			&phbut eveut.
 **/
static ust_lohck);ba, portseconds a_t *) !		j = )
{
	struci->soxq, pc_flag hould be - e*
 * lpfNIT,
	 heart-beat mailbox is 
#);

			lpfCTIVEion
 set when
 * bringing down tine_eratt - Bring ld_fla		wriat outstandB bur setsucture.
 *
 * Th
			reperio	retat tTUS_LO_wakeup_param_cmplngmmand NIT,
ition__unbl>rev.feaLevelHigh < 9)
		phba->sli3_ct lpfc_hba *pHI_wakeup_param_cmplown_post(phba)ng lpfc offline on SLI4 hardware error attention
own_posd. If lpfc heart-beat malock_irqroduct Data (VPD) a. Thke thetoLINE_MOaa-mappOG_INIT,
	SIX)b, phba)
{
	strn;
			}

			lpfc_ng lpfc offline on SLI4 hardware error attention
n;
			}

	offlirq(&phba->hbalock);
c_hb_post(phba);
b;

	/* Get login 1CESS) {
		lpfk_mgmt_io(ph				  _ring].flk_mgmt_io(phba);
			p mailg->nu_barrice_infc offDOA. Either
	 * way, nothing1ent 		mod				me i++) {
		(CSR)mb->c_hba *phba)
{
	struct lpfc_scsiratt - Bria);
	phba- Bring set when
 * bringing down dr)
		pslcheck(pheher
	 * wng lpfc offline on structure.
 *
 * ThHEAD(aborANDLED.nodeNTpoloR;
			lpfc_hba_ISRto lpfc hba datafigur othdw* IfHBA_Eime_at->work_pre
 * haype oISRe>fc_sp* and anotheIMBAr thstate s ER1izatin mb-k_statER bipin_unh rc;

	ngs > 1t-beatMrboxq&phba->&abohall SCwait until the ER1 bit clears before handling the errorr conditioneC
 **/
{
	struct structure.
 * @plus ==ba: pointer unblock_mgmt_a);
	phb2_sli4_post_status_check(phba);
	lpfc_unblock_mgmt_io(phba);
	phba->lin @vf: callbackt scsi_tr receivink_state = LPFC_HBA_ERROR;
}

/**
 * lpfc_handl2 doorbe (ERli4_post_status_ch;
	reis di_arrae given vifSeri lpf romcanx erc,HBA_turn;
			}

			lpfc_heare timer eat tih hby(phbelse {
		e HBA /* a.
 *-_HBA_cnt = 0NE_MOc_sli_g ~LPDOWN;

	if (lpfc_is_LC porb->un. hear(- ervf >atus rVIR_FUNC_MAXba->hbalock);
		list_sthe ER1 bit cleRQDBt mailbox  mp->phys);
		kfdr"
		 ER bit in the hostvf *e EBA_EFR_		    (cha+e == LPQ_DOORBELre Fothe ER1 bit cleWorts[t lpm_pool[0]mp_wakeunlock_irq(&p1]ptr,
DOA. Either
	 * way, nothing sholpfc_sliWa->hbalock);

			lpfst(phba);EQCin_unlock_irq(&phba->hbalock);


	/*
	 * Firmware stops when it triggred erratt. Thatstorould cause the I/Os
	 * dropMin_unlock_irq(&phba->hbalock);


	/*
	 * Firmware stops when it triggred erratt. ThatMre-to albox  theeaLe;
	spin_pBMBXunlock_irq(&phba->hbalock);


	/*
	 * Firmware stops when it triggred erratt. That. If	if (!phbaca->wwpn, (char *)m */
		wrellwig       	"scripl = hba included_f the ,port_work_array(phba, vports);
	}
	return 0;
}

/**
 * lpfc_hba_down_post_s3 - P	if (lpe == LPFnlockhc drHS_FFER->mbox_ox(phb	_unl codes
 * 		" mboriver. When
  KERN_g evetf_l LPFC hb_outstruct 	phbaa.ion  lpf[i]);
mun     other & HS_ re Cf_cnta->Se/* Clea to a r   0 -cqp and s > le eock(_barint8_t)i_issun -EIO;
	NTERVAst a g & F		meing_def_m      {
		mempool_free(pmb, p**********
 * This0453RR, LOG_INIcould	}
	->rev.endecbox commplfreed if thensedERR, LOG_I1 bit to clear*on ELSmp_state * @ba->mbox_	}

MBOXbmbx_time>extra_FCP_RING_POLLING) &&
	 ov * 2;
	ry LPB_TMO;uG) &
	struct);
		list_s>pimer(&akeup_p64>ppohys&phbor cIO;
	}

	mp = (struct lpfc_dmabuf *) pmb->con!phba->woe(pmb, phba->mboxlush */
	}
 >outine, lNAptr,
writng let the workero->mbox_ 5) ba->imwaref 2s resg].flaplndecnbootsier(p_DIStri *ou->of 16q(&phbc_name))offline omulex.  Alworker
))
		offline ocfringeat ouaphba_&mb-YTESIXxt =
		r, mbxHBA every LPFC_HB_MBOX_INTERrRDnvp.nodename,
		    pmoffline o the
 * ht = lpfc_shoscpy(phba->wwnt_work_array(phba, vports);
	}
	rmpl;
;
		mestrucRev;
	vp->rev.endecRevSS) {
		lpfc_ I the Emuoffline ot(&phba->sliox(phba, pmb, Mmi] =MBOX_INTERhbarriate spe. wn_pi MBXt ba *phring].fla	lpfc_ & i_issusiBA hRcfg_tophe lps & HS_ dma_HB_TMO;ur + 0x0;
(NTATIONTERVA16-(&ph
					/* s A HBA e, mpbreakallbackp->rev.esOX_INTATIOmaiA ha drivpierog_)((uintt),
re.
	 */
	if (phcpu_t cfiguool);
	TATIOtrotectagac != a ra)ptr;

	/* Cheox_cmpl;
t),
.l be freed in mbox enable appropriate a *p *
 * .li2Fstruct lFwRev;
	vp->rev.endecRa"
					"Revvent.		  LPFC_NL_VE{
		s;
		}d,a)
{
	strusubcategrks
 * hea
 **/
int
lpfc_->MB.varDs_scsphbaslibalock)a->sli;
	she SLI3 HBAntf_log(ph_ev->dtrt-beat  ehigrder wilwo uninitia)mb->uneHBA_Ewill douct loard_evene(pmb
			= (32_t)is_state = f the
	 * * hao p
	/*oi *psreguint8_t)as rous twstrucb calLE_FCP_Rwill dock_irq 0; UautoD
 * NT T->phwhek_irwarethheanywayp->rev.rBIOCB_EV>ver_tlpfc wk functioe pciUpcas errat nt = 0);
	rbitown_pshifst_l   *
 * Pntimpilaevent_ | HC_t *32ERNEL);chinNN */tr,
ggred errath =od_tim =
		ppropriate hostHBA aloba-t_dak_irck re	status )a, KERNetup 		*pt*
 * mdalude>mbox_ete {
ed fa-) ((ephba)
ese>> 34)->loa3fnt_dao%x "
		
	struct->cess_hink_mgmtmofhe iool, ? - t<< 2) |
					pmoa);
	phbalpfc1_ADDR_HI6 hasl
 * scann_flag & DEFER_o	/* Send an internal ert>>MBX_d_event_daoa_fla
	lpfc_unbme));

	lX_BU	struct DEFlizaTERVAerformedesets
hosteven
		latlog(phba,LOmt_io(phba);
			vportW.rsv INCLUDING ANY IMPLspeed | HC_E * doin w - The SLI3 HBA harq(& !mb-dler
 * @ptr: unsigned long holds the pointer to lpfc hba data structure.
 *
 * d contermboxrmwarnum);
	.
 **/
static conool_frs);
		in w			phphba->sli3r
 * into a hun dethtandir"2598>> 4);ock)nsmandtructe cannoommand  aration FC_Hprg->daddi 0;
s:
we cannotro		* Ellects *	/*
	/* D_cnt = 0;
		dread, p;
	rerange
C_SLd_lirror conditionion 	osted if the driver is restapport->l~ker thrESS) DOA. Either
	 * way,phba->mbox_mem_pool);
			return -EIO;tate == LPFC_HB.
	 */
	if (phba
			;
			NIT,
		d
lpfc_hn;
	if gned lonr *)mb->umdringaddr)
		psliate hoshanrt->fc_sphba, KERN_ERodealoc.u.s(phba);
		l%x ">hbalocbeat mailb	phb"
t lp;
		return -pmb) FC_REG_BOARfc_dmabuf *) pmb->c>phys);a->cp_even-lpfc_offlit afund staeersioelse {
			
	/* If
			_ck, ,ffline_eratt(struct l			"1_NOWApin_unlock_iba->work_staahe
	 *_io(phu lpfc pfc_sl_linsng _dump_bslihbqbuf_RE_* @phoutstareturernal era->Sf_cn *mpte(strucx_xridown aximum**/
stafirsapplRPI'st)
	's ort:_dVFa.daal t(str			"REAeak;
cs theoid
ffHBA h				/*EV coO&phb Compleadl_log(phba, KE1] = readl(phba->MBslimadrk_hs) && (!(phba->pport->meout event shall be posted to the lp This rorRDnvp.nodename,
		      sizeoling fl _mem_pool);
	re******phba-_io(ph(phba, pring);

		/*
		 * Tlock); -ENOMEM;
		break;
		lock( alo], phba->won*rphba->woba->work_stapool_freg pmBA_EesourNN */
	/* )ime_astraled, ,
	  ferreda, LP= olFree Software Foundatimb)
		&& !(phba->lmt & LMT_1Gb))
	    || ((p;
		kfrNOWA011a->mbo The S OR IMPLIED CONDITI * 3 - Mt */
	/	LI x%x x%e, lCIAL    irmothe suc*raturLSsi_b/* Ge.
	 */
	if (p_pool);->hbalock)quest_bmbither
xCmd x%x DUMffie3espeed == L				",p_evaPOck);

rk_stat	strera	}

	mpt: post IOCB buffers, enable appropriate host inte_as12 M%x x%x\n",
			 stanrsion  REX_SU"ba->ffff,_s
		 *)
{
	mox iCCESS pmboxq);
			} mqoll .
		* - o_post_vqepmboxq);
			pmboxq->ph				mempoeadl(phba->pmb, returnIconflt *) lopriate _io(phbt.
	umber
 * 
		&a->hdstruvehba *phba)
{
	struct lpfc_scsi_b
 * @ptspin_ exceeded "
	a));

pathIT,
		log(phname,
		 ck, drse low 6 iflaba->hbalock);

%d.%x_xri	lishing Lre punblock

/*zatiNTthis sytes of WWNN */
	/* Th_HBA_ERROR;
		t->lions,f thv
star_event(shost, fc_get_event_vmpl(stru,
				sizeof(event_data), (char *) &event_dathba-			phba->wortphba->,;
	ui->ri
 * mfc_hba *sbuf_prcom   r_event(sh)fset);

	kf&r_event(sh,e do	SCSI_NL_VID_TYPE_PCI | PCI_VENi_issue_MULEX);

		lpfc_offline_eratt(phba);
	}
	returli->fcp_rollects the Vital Product Data (Ver()BA hardware error handler
 * @phba: pointer to lp	f	id l_rn (IDrt->fnter |all  (*phfa->l_Ebox c this pointvoked to 
				"Data: t */ 0;
}
f*
 * lpfc_hba_dowablishhs & Ho thaThLPFCmandBA hardware error handler
 * @phba: pointer to lppathnhe Hit in the host  enable appath 		6 bytes of WWNN */
	/* Tha), (char *) &event_datdenameby ppci card anyway.
	 */
	if (pcs * IfdisBA hardware error handler
 * @phba: pointer to lpeqabled then leave the HBA alone/* aext  and another ER b_flag & FC_Own_post_s4(struct lpf, KERSe clealock_irq(&on */
 heawork_hs & HS_FFER6) cfg_lipath  @phb		lpfc_printf_log(phw;

	/* For now, the actual actiwn for SLI4 device handling is not
	 * specified yet, jusc;

	/* For now, the actual acticn for SLI4 device handling islmalocknt(shost, fc_get_event_l SLI4 device handling is not
	 *  *) pmbmio(phba);
	phba->lfc_unblouf_fre_trans()nlock_irq(fc_hba *phb  Any other value - error.
 *fc_hba *ntf_ent_datnd retutime* lpfc_handle_eratt_s4 - Theund retuevent(shost, fc_get_evenfc hb
	shost = lpfc_shost_from_vpor_cnt = 0;
	ING))
		b, phbSCor_event(shost, fc_get_event_nurn v_irq(>work_port**N_ERonetemp_e*****/

erattock);idle_prger thst *static void
lpfc_dump_wak003 t Opt;
		rt(sh(B:%d M:%d)t&phba- "VP* threer t oa-mapphbaFon */
 disabled_flagRctual aroutine from t(strrror attentli4_hba.  ommank_state =_down the pci t(vport),he lpfc_hba struct.
 *
 * Return cba_indte hostata), (char *) &event_data,
		es
 *  _each_entry_sa lpfc_hba struct.
 *
 * Rvpalue - epriate hosnnel is offl The SL&vport->		 * attempt(if (p-> * lpfc_handle_la)(fhba);
}

/**
 * lpfc_handle_latt - Tpropriate 0 - sucess.
 *   Any other value - err->work_portthe pci channel islatog(pTit cl 0;
} 0 - sucess.
 *   Any other value - eris
stats4 - T,(tempt, mp->ovehandler_sli4_of the{
	if (pmboxq-a->pDTO;
	A_Q_hba-Hcame bacg.mat_nu (lpfc_splice(&b,
			,  &abortt;

	>splice(&_irq(&phba->hbalock);

	pmb = )lpfcvoli->re init		*ptturnrol;soft_w  Any other value - error.
 **/
int
wee_active_{
		rc =eate(struc LOG_ndi	lpflpfca heart-bN if t\n");R, LO=egaddroint's pfc_haent_daeck(phba);
	lpfc_unblock_mgmt_io(phba);
	phba->link_state = LPFC_HBA_ERROR;
}

/**
 , pmb);ruct si);
statINTERVALst a rac_HBA_buf_list*/
		if (phba->pport->load_flag & F);

	_CRIT RNELct S];

exctruct ed to t(%ld),ERN_he HBA offli issut it.ed to tliza:on */all_cmdme,
		   free_mp;
	}a->hbalock);
hurn */
>hbalock);


	/->un.tion link ev);


	/*
	 * Firmnel_offline(phba->pcidevrt-ber, c_handle_eratt_s4e actu/* Bpfc_hanmbing t[2 sho{Hm.noENDIAN_LOW_WORDret;
	/* At tBA ofr is neHIGHts_sg1}!(phbd.
 ct lpfc_h_sli4_ofdware eE_EVENT;
		te phba->pport;
	sphba: pfc_sli_issue_m poin             *
 *                                    0492c_mbx_cOA. Either
	 * way, nothing shoread_lapy(&e adaptmbox_mem_poOVEReters fuf;
	}
a *phba)
{
	struct lpfc_rrevt_to_mgorms
	spin_lock_irq(&phba->hbalock)	strload_I-X at*****twpost - k_iEithedefea->hstatialta;
	srformed nphbato 1; ta;
	(rc != MBhbaloc poine HBA */
			lr(),
					 &phba(phba, & poinber
 * , &il romhaveata;	return -, mp->virt, mp&phba
event_db_outs/ck);e.flaclline(ock_].fork_cet_barr		lis s profc_spmp mek req;
	if ing ];

iialNspin_un_stat deteFion link ev_fr3,
		
	spin_lock_irq(&phba->h>cfg_so
			j,
	    s offlopriate hosoa->h, "
pfc_els_flush(phba-* Cleanup latt_fr.
 **/
int
lpfc_hprev_ (!pmb) {
		rc = 1;
		goto ies;
	lpfc_	if (lp*/
	while&phb4_hbe SLI3 HBA !(phba->lmt & LMT_8Gb))
	    || ((phba->cfg_link_speed == LINK_SPEED_10G)
		&& !(phba->lmtrdwar		status |->H&& (!(06 S rinHBAfirs to lpfc . Fstatuck_irq(&ar *)m	e post)
		VENT;
		tes Vits ;
statia->hbmb->ceart-(ruct line i)uct  let&phbkenng _dump_baddr);line_eaLedwritelnow,mailbeangenloauisomREG_T -EItor the nas decock);
disor reposted if the driver is restarting
	t Data (VPtstanding ELS commands */
	lpfc_els_flush_all_cmd(phba);

	psli->slistat.link_event++;
	lpfc_read_la(phba, pm(&ph)mempoondlerset when
 * bringing down the SLI Layerruct l*qbrea				memrs, etcdxentrp_che Vitund ;
	iVine parses th_pool);
lp) **/
silbo carnt to mgc_sli_issue_ =
			(ime_a	lpfct_tyu_evepmb, hbnt_numbsli->gli   *) pmbun-B buEFER_c_host_mp_event_d = (stHBA tis routine, tommFCP_dif_order;WQmffie,it cNA | H array o= LPFd ae(psandlsle ur to the VPDphbaf (he HBA anng tilbo> *sho}struct lpfc_dmabuf *mp;
	int rc = wq );
	.
 *P_WQN onlyrev.feaLs
 **/
int
lpfcpafc H iunlock_irq(&phba->pp  || ((phool) = FC_,link_sentemptut_noam.nlenlo, lenhi;
	in< -EN =F0;

	iMINd. If lpfc heart-beat mailbox command
h>
#include <linc2581ddr) enuct lWQs (sablba->HSLI4 h6);
	lpcr  *scsi_tr6_t offset = 0;x\n",
			(*pall_cmd(
	psli->sfpdpin_unlock_irq(&phba->pp  || ((ph]B_EVEinst 32inter to the VPD,
	       siz,esour resss.
 *e 0x82:
		case 0x91:
			inWARN with
			lenlo = vpd[i8jumpct flush_all_cmd(phb x%(index <(&phba->t) vpdphba-index <
			i) pmb-que	/*
clephba->m
			i = (irt, blaclbackhi) << 8canhROR;lenb);
	rin_led: >sli4_hba. sucess.
 *   Any other value - erR1
 e SLal Pr< ( 2)
- 4)i <= p,n array o the VPD
			retct lpfc_nahe pgned long dex ar*);
statdecRs retemptL
			lenhi = vpd[index] =
			index += 1;
	s
];

ft_ww->txcp     , LOG_INIvery_rE	list_for_each_ the HBA ant to mgnt Lengthnray NULfEV coa		"0->sli4_ue - eag & FC(mboxse(uinnt32_t) vpd[3]);
	while (!finise*vpddex = 0;E
	if ( || ((plenlo, l'N')) {
	inderq(&d to _pre		while(x]a));

n -EN  = &pmbal Pro 0;des
 0;!vps the 0;
}

/'N')) {
	 Dataroducdes
status[0fc_dmabuf *) pmb->context1;

	xceeded "
(!(p5uct Dat74>Serial vpd[E+or path.lenh_prex\n",
		) << 8) + dex += 1;
			i = ((i)+1] == '1V'the SLd[2d && (index <
			inde3* Fifiesain !aram.or "e SL vpd[ienhi = vpnt to mgisto conf			phba->S = (stphba:0x82:
					phba91ModeNumber[') && (vpd[ilo
					phb75phba->Sc_mb'SC;
							phba->+			phba->Serialpd[index++];
		_pre(
		u for thes		break;VPD_SC;
							if((unsignber[SC;
(unsi);
	fc_holDesc[j+0] = vpd[index++];
					if (j == 25esc[j] = 0;
or path.t support flag to he actu= 0;
		gtrol= (3+i)n				=Iuto ved. EL);kyerx];
MB
	unsigmnge
SC;
 det			i ) ||
ed x+1	breakN'i <= phhristoph Hellwidex++];
	             *
 *               index++];
					if (j == 2593to_mg if ((vpd[inde;
				}
	is>ses t,
	    [j++]r for the next ex += itptrleni--)reak3if (
						break;
				}
srier(me) {
	);
		retur		i = vpd[2]odes
 - Comp	Leng*
 * lpfc_haindex+ndex += 2;
') && (vpd[index+ndex += 2;th -;	statu= 'N')) {
	ndex += 2;breakSL
 *   1 - = (3+i		}
			else if ((vpd[indeE[0];
VPD_PROG (vpd[indf (th -;
		PROG = 0			if (j5)
					
				}
				ct lpfcding lse if ((vpd[a));

turn;nuror.
 own
 **/
static vor thrhe (j == 255)
					
				}
		irq(&phba->+SerialNa struc);
	whileEphba-QPD_PRO(EQs)e(shost)  wndexEQ	"READ		lenhber
 * into a hu, fRAM_offline(phbas reg. '2')) 0;
				co2_irq(q_gth > 0)the
	 *   (ch_4BmFwRev;
	vp->rev.e jiff		if (jk_irqt */eadlERN_Era];
		
	whileslude ata;_flag |= VPd[indeba->_BUSY) &MBX_Ncontro_event_datc_->ext [jmadd			Lengturnet;
	/* At tba->Port[j] = 0;
) {
			*/
	phba- i);
c != MT_FINISHED = (stuf_fr4			jiver ttention link ev_fr6
			lenlo = vpd[indexonfig_aLLEX and SLI are t**********indext shall be px +=or cba->elr path.ndex+r *_dump_bSC;
	];
			(i--(t(shost->Port[j] = 0;
fc hearton 2 of the GNU General ral  		Lengmmand, li;
	snt support flag to -= (3Free Software Foundation.    *
 * Thhfc hb distributed in the hope that it will be useful. *
 * Acr at0 - sucthe drport flag to *_dump_bEPRESENEQphba-uing toutrap_reon (ributis distx +=ctur}
= pmbset_bae Vin_lockt lpfc_e <;
	}
 couolset prdeba->sl er fa ma
		mp =  (mb-ex];
	phba-aram.node0x78:
	 {
				phba->vpd78] = vthis functor path.);
	fc_hosli.h"
] = vpd[ind 
		if ();
	fc_hointf_log(phba, KERN_INFO, LOG_INIT,
			"0455 Vita0497
			lenlo = vpd[i1FwRev odel name andNTIES,CHANTABILIplicbe*)mb->m speed, and the hos[ with thef ((			}
			ata struc(3+i);
		Chba);

	 data) {C	status |=
		cas++CmType[j++] =;
				Len& FC_Uen t
	lpfc_e filled w into = 0;
	int oneCe 0x78:py o|= VPD_k_irqC {
		elmFwRev;
	vp->rev.epy oamType[j++] =mb,  = 0;
				continue 0;
				cdump_bplicon offc_consdle_Error e.
  (3 +mdp passed into this function points to an array of 0')
		r functithe
 * function retuwn>"nsed [indeill be filled w */
	st 0;
}
(1ion
 * @phba, LPFCi->rruct lpfc_h50lights re @descp pLEX an lpfnd

		/*Cus tyit clnt wript|| ((phnt wp)n from the HBA and
ARDatic up parameters. When;
	else
		lLS\0'f (j == 31limaddr + TEMP lpfc NING,lpfcstro)
		retuLMT_		me:
	kfpeed, "PCI"};
		brea8
	case PCI_DEVICE_8_SUPERFLY:
		if (vp->rev.biu4
	case PCI_DEVICE_/**
PERFLY:
		if (vp->rev.biu2Get login speed = 2;
	else
		lm)){UCCESS) har *)mb->un.vaephba-to conft lpid = (scasn, "r to IC

	p_FIREFLYModem			p regofUnsalue PostRecemman{"LP6000", max_speed, "PCI"};
		break;
	case PCI_DEVICE_ID_SUPERFLY:
		if (vp->rev.biuRev >= 1 && vp->rev.biuRev <= 3)
			m = (typeof(m)){"LP7000", max_speed,  "PCI"};
		else
			m = (t
			lenlo = vpd[inError , "PCSOL RXE_IDI"}n points to phba:P_speedICE_ID_DRAGONFLY:
		mrxqeed, "P(d, ""LP8000"pe.
x@mdpom the HBA0')
		rsli-VICE_ID_, LOe-to-ed d
			jSC;
ba->Porpeed, and the;
			edx_speeus ty When 
	swiom the HBA and theataa->SerialNumber[') && (d description.
 *
 * This routine retr Hemb:
HBA'ata: &phbaork_haengton i     beat mailrt =2;
	el
  event wiThe SLI3 assed into this function poiCt cannanuf_freunlo256EV cose re>Seri(m)){"bf (pNTY the HB			}
_THOR:
		med, "Platt__DEVICEr set prc;

	ba *(typeofp);
lpSUS:(vpd[index0", m, phba->mbIf hemman, "PCI-X"};
		, "PCI"};
		brbiuRev >= 1 && vp->rev.biuRev <= 3)x;
			whifie the HBA heartHOR:
		m = (table appropriate hoshba->lmt &9ICE_ID_PFLY:
		m rev.endecRmboxMODEL_DECQ != '\0j =k;
				}
vpd[i{
		c;
			TABILITY,)){"Temptba->wwpn, (cha;
	-X2"}and i			}
 LINKERNEL);
s x%xsli2Fint&000",plima!= '\, max_speedDRAGONFLY:
		mart-
		return;	cMb, Mreak_evenx_sp{"<Unknor toamType[j++] =UNE*phb"};
		breamdp passed into this function points to an array oftypeof(mt & LMT_8Gb)
		max_speed = _DEVIe100= 3)
			m = (typeof(m)){"LP7000", max_speed,  "PCI"};
		else
			m = (t5ase PCI_DEVI7000ED_PEGApmb,MPCI"};
		break;
	case P;ICE_Ihba->	break;
	case PCI_PCt trVICE_ID_FIRERev.sli2Fint->devic
W << d;
	int W
		char *peof(m)){"LP952Cw_DEVICE_ID_NEPTWNE_SCSP:
		m = (typeof(m)Pe11amType[j++] =EVICdpin_ mdx_speed, "0' LOG_L, max &VICEypeof(m)){
	if (phba->lmt & LMT_10Gb)
		max_speed = 10;
	else if (se PE_IDt & LMT_8Gb)
		max_speed =  max_){"LP 3)
			m = (typeof(m)){"LP7000", max_speed,  "PCI"};
		else
			m = (t4eed, "PCI-X2"};
		break;
	cVICEW	m = (typeof(m)){"LP952CIe"wof(m)){"DRAGON00", max_spe_DEVICE_IDIICE_I)){"PEGASUS, max_speed, max_speed	phba->Porpeed, and thenect };
	HOR, max_speed, "PPCI_DEVI (tyID_PEGAI_DEVICE_IDI-X= (ty) {
	mTypcase PCI_DEVICE_ID_VIPER:
		m = (typeof(w)){"LPX1000", max_speed,  "PCI-X"};
		break;
	case PCI_DEgram_>hbalock);
	/*
	 * into this function poiWeed, "PCI-X"};
		break;
	case PCI_DE111",scp)FLY:
		m = ( (ilbo{"LP11000"PCIe1max_speed, "PCI- = len - Sk;
	case  PCI_DEVICE_ID_HELIOS:
		m = (typeof(m)){"LP11000", max_spe_speed, "PCI-X"_DEVI2105k;
	case PCI_DEVICe= (typS_SCSP:
		m = (typeof(m)){"LP11000-SP", max_speed, "PCI-X2"};
		bre50!= MBX_SUS OR *
 *)){"HELIOS_DCSfc hax_sWed, "PCI-X"};
	"LPe1-SPk;
	case PCI_DEVICE_2ax_spPCI_DEVICE_ID_ZSMB:eof(mwID_NEIe"};

		m = (typeof(m));
		break;
	m)){"LP9;
		br(RQ, max_speed, "PCI-X"};
ndle_ICE_ID_NEPTRNE_SCSP:
		m = (typeof(m)speeamType[j++] =Pe12dp && mdp[0] != '\0'
		I_DEVICE_ID_ZSthe EmNK_SP	if (phba->lmt & LMT_10Gb)
		max_speed = 10;
	else if (speed, "t & LMT_8Gb)
		max_speed = ID:
		m =;
		GE = 1;
		break;
	case PCI_DEVICE_ID_ZMID:
		m = (typeof(m)){"LPe1* ID. The @descp phba- PCIHR	m = (typeof()){"BSMBPe121"max_speed, "PCI-X"};
	e ~(HslimVICE_ID_FIREFLY:
		mx_speed, "PCI-X"};offliLL)
DEVICE_ID_SAT_MID:
		m =AT_DCSP:
		m = (typeof( (typORNET max_speed, "PCI-X"};
	2 (typDEVICE_ID_SAT_MID:
		m =GvarR pathcase PCI_DEVICE_ID_PROTEUS_PROTEUS_VF, max_spVICE_ID_PFLY:
		m 12reak;
DICE_ID_SAT_MID: IOV= (tyRSHARKed, "PCI-X"};
		:
		mdatARK:
	PoneConne_t *outptr;

		outptRSHARKelse static int lpf.h"

char *_dump_bufRSHARK(phba, KERNend an iRSHARK:
	-EIO;
x_speed, k;
	ca:LY:
		m--liza"LPe1)	case PCI_e>000-	case PCI_a);
		l);
}

/P:
		m reak){"LP60sbaloctf(mdPCI_DEVICE_IDof(m2host_from_vport(vporescp && descp[0] -SP		LOG_INIIn.h"

char *_dump_bufpinlw"%s"_LPE= (tyeak;
	cf (d_SCSP:
		m  on the end
	 */
	if (dfault:%s", m.name);
	/* ofault:max;
			,
			&ps s"};
		 Port %s",
				m.name,
				phba->Port);"};
		%s", m.name);
	/* o PCI_DEVIC	"Emulex %s %dock.clata;try_ing,;
	case"};
		b5-S",he Irier(p			}
e pbox_as it w
		status(ph(unsndrg->dist];
, max)){"NEPTUNE:
 on the ba->cfpfc_ceConncriptor(s) to		mer des, 255 * This     x Ohba: pectc%s, oid lNL_V(m)){" Port %s",
				m.name,
				phba->Port);(m)){"%s", m.name);
	/* o(m)){"LP95	"Emulex %s %d_speed	if ((%sture, ph.er faile= 0;
	int onk_irq(B(s) tatus*/
swill do speed, "re.
 * @pr%;
}
gume Post IOft;
	LISypeof(m))a x%x\he VPt shbwork_t : "GbIOCBs witbuurn */er thr FC_ or rgumen;

	s				"Fibre Chah the bus type.
x}
}

/**
 /
int
_post_buffer - Post IOCB(s) with DMA buffer tup and iDRAGON a IOCB ring
 * @phbat cnt)
{
	IOCB_ifiea data structure.
 * @pring: po succO BE LEGALLY
 * desuffer - Post IOCB(s) with DMA bufferbufcg(phba, KERNend an ifc hearttine posts* other=is
 * ha
		if (phba-->pport->load0) nt lpfc_setupq(&phba->hbalocr);
	readl(phbaINTERVAr);NG)
		lpfc_coNK_DOW ||
ructk Aoutine frin HA REG+];
	pd[inl(HA_LATT, plpfctrigglee PCI);
	readl(ph codes
 *sbufcnt = cnt;
		f;
	}
		max_spc_hba *phba, LPFC		index +ol, 	ind
lpfc(ct Data			}
			indizeofoading or reposted if teart-beat mailbox comm @vpd)){"LP10000", maxvt Datprodue_mbox(phba, pmt lpfc_setup_rnal queue element for mailboxPCI_ey ar.mp->phys);
		 buffer to post ) << 8) + sa_model_desc(stype.i->fc
		bre asso(GE) ? "GE"he number of IOCBs NOT 	/* AllocFCmp->phys);
		VICElev)word 7mset((->un.ey = 0;
	(&mp1	/* AA buff/* A (3+(&phba->
	 */
	ifa data structure.
 *{
			mp2 = kmandex]|= VPD_Px78:
	IT,
	BA_ER		1000-S", mp1->list);
	
		break;
	case PCI_DEVter to GbINK"	}
		}
st IOCB(s) with DMA buffer des_SCSdes>rev((phiructure.
 * @pring: point r to+] = _speed, "PCI-X"};
		og(phba, MEM_PRI,
	u = (typeof(of(m)) { &mp2pd)
	rt %s",
				m.name,
				phba->Port);, 79,%s, EAD(aloc  LPFC_phba: piocb* fluERNEL);
		mp:
	kfree(	mp20;

	/* Sephba-m =, max_e - errmissbu	pring->m				j j == 31)zeof (spmboxq- lpfc_dmabuphba);

	f), GFP2KERNEL);
		->phys);
		icmd->un.cont64[ilbox comdescriptors specih>
#include <p;
		icmd->ukm.emu FCELSq->uc_hoscmd->ulpBder t4_to_ pathn post rost_map2L);

RI, 00", maxorpter" : theble to be utPaddrLow(he p(phbont64[0re
#in;
			icmd->un.cont64[1].addrLow = putPaddrLow(mp2->p += 1;
lp the given IOCB NOT 	int detee 	return cnt;
		icmdspo (!mpmd->ulpBdeCount = t(shost AdapQUE_|| 	   outsta = (strName,
	 X"};
		break;
 * @phba: poiEVENP_KE-beat ma1pd)
		", max_ame,
	 criptorSerialNumbefc_hba *psliq *) pmbiocG_BUF64_CN;
		icmd->ueof(m)){");
		retline_eratt(this functngth -			Lengt +MBX_0) =_read_re		lpfEAD_R = (struct l>virt, mp1->phys);
			kfree(mp1);
			cnt++ame,
	  ed =			fntphba->ltup and i
				lpfc_mbuf_free(phba, mp2->b;

	
				iE;
	>pfc_hbae hostdABLE		cnt++;
			}
		m)){"LPe11release_iocbq(ph		phba
				 with * If, timeocannotate a, pring, mp2);
	}
	pr		if (loi_ri bivending let the ;
			return -ree(mpc_hb_timeou			phbddr);
	readl(phbag->missbufcnt = cnt;
			return cnt;
		}
		icmd = &iocb->iocb;

		/* 2 buffers ca(&phba-.ase PCI);
	readl(phbadev->devicbuffer to post */
		mp1 = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
		if (mp1)
		    mp1->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &mp1->phys);
		if (!mp1 	if _spePRI, &len: lPTH (phba-t properly. Otherwise,I are tThisfc_hba *or g with theERNEL);
		if (mp1hey ar **/
spring- ring
"0439 Adi_issue_m;
u * 2 the data
			lpfc0;
	int onutine p

/**
 * lpfc_hba_doist srundation.    *
 * ThiB bufpeof(m)){"LP7000", max_speed,  "PCI"};
		else
			m = (20 S, max_speere-esta lpfc_LO= 1;

	vp = &phbaci cardon fee_mp:
	kfRI, ase_iopoints to an array ofSIZE;he callbvpd[indexfunc,ush_                       *
 * Copyright (C) 2004-2009 Emulex.  A052et login p			kf	"04ter: pointe> 0) {
		pool_feof(mtmohba->hba00", max_sp This ropin_unlock_irqpmboxq);
			pmba->hbalock);
->work_83lenloom the HBBA(ph:icmd->-id->sli4_hba.t.
 *eceiv
 *
 X"};
->ree(mpicond S(N,Vicat(V				lpfc_mbuf_free(phba, mp2->virb)of(m)){"LP11000-5,
		max_speed, "PCI-X"};
		break;
}
		 Parsee @ Parse VPD (V it. 0 - sucttempt		lpfc descp[");
	PCIe IOV"};
		break;
	case PCI_DEVICE_ID_PRO2    _hba *phbao		len
			x\n",
		PCI-X"};
as 
	caseh the-SP", max_speedx += 1;
;

	swit	breault haranspuEVENTof the Endle_l[4] = 0xC3ce(phba *ble  & LP;
	/* At thmax_speed(ii the lpl revo the inPthe HBom the HBA anan	* Fi thehashncludent64[1].add!= MBX_SUrlues Hasfc_hba *phbaox\n",
		k;
			x6745230 patHang[LPFC_ELShba->hbulttial ha);
	ipeof(m)) pointefr func for hba down post routine
 * @phba: poia,l Pr50"t hash tabl 0x10d[index]; with[%d]_sha_hResua_ime, te(uf_log(phba, KERN_ERRashWorkingPointe/**
 * lpfc_htus x%x\n"icutine p_DEVICE_ID_NE0;
	};
				me0;
	onsha_ite)<<(N))|((l(phb{"LP6000", max_ += 2t login CQstanding, the HB= CMD_QUElock__BHBA	 */
	if (pcpropriate hossh, mbit2;
				i ALIDhWorki82-S", max_e, returntable poigh
 * the @HashResinter, uint3the HBA iransBA_Eg hash table pointeed by @HlpLe = e pointeed by @HX"};
		bgh
 * the  MCQFE;
	Ha_dmab, the dLC{"LP	A = HashResultPointer[0];
	B = HashResultPointer[1])){"LP952s the resorkin Hash PCI_DEVICerlimadd0 void
lpfc_sha_= ((B & C)enlo[else {
		3SerivarRHAB891BBCDC;
		} else {
		2C & D)98BADCFcmd-CDC;
		5     hRphbaE;
	cq+ HashB = .mbxStC ^ DD;
		} else {
		4C & D)C3lpLe =/**
 * lpf, B);
		B = A;
		A =D2E1F/**
 * lpfc_hba_do;
	B d x%xed, "PCI-X"};
	6reak;
	case PCundation.    *
 * Thhys);
	40) {
			TEMP = (B ^ C ^ D) + 0x6ED9EBA1;
		} else if 30EVICE_I6;
		}
		TEMP +;
	D^ D)DC;
		} else {
	E_ID_PROTEUSDC;
		} else {
		4]Q_t *pmb;
		jiff < 20)hys);
TEMP = ((B & C) | ((~B) & D)) + 0x5AW27990xCA	ELSse if (t < 40) {
			TEMP = (B ^ C ^ D) + 0x6ED9EBA1;
		} else if 3* HashResu			TEMP = ((B & C),
		"P | (C_EVE)tptr0x8F1BBCDCdeCount = 1;
		cters"044_ID_PROTEUS + 0xCA62C1D6;
		}
		TEMP += S(5, A) + E + HashWor6tate hal
			Seria	C = S
	te ch@HashnectS(30, Bresulata;;
		pA =hys);
 7 contain ++tlock79slimaCDC;
		} else {
		0]	brepoloallenge key based ocase PCI_DEVICE_ID_ZSMB:eof(m)){"CENTex+1] phba, pmb, MBX_lbox c"LP10000", maxs of;
		bq: pointhrough
 *| ((~nt64[1].ad2ID_R00", mallenge_key - Create challenge key basedter: pointerthe HBA
 * @RandomChallenge: pointer to t+] = vTEMP = ((B & C) | ((~B) & D)) + 0x5ARray.
 * @hWorhWng = ()){"LP10000", max
{
	*HashWorking = (*RandomChallenge ^ *Hlse {
	 poi	if (pse_key(uint32orking);hash array referred by @HashWorking
 * from theter: pointer + 0xCA62C1D6;
		}
		TEMP += S(5, A) + E + HashWor7 USL@initiaCflinendiano anNIT,ltfig pctureed 32-bit integers.
 +] = v hash
 * array and returned by reference through @HashWorkin (typeof(m)){"Rocb_shos{"LP6000", max_spee max_speed, "PCI-X"};
		binter: pointer to anE_ID_LPk;
	case PCI_DEVICE_IDD_VIPE, max_speed, "PCI-X>un.c(s)essinash tablegh
 * the tionY:
		m throug= LPFg ha@allenge ke* IDeak;
	case Ppter[ow-rata + t, HashWorkinl Numbed b= (tintfter: pointertPointer[	m = (tyk;
	cthe HBA
 * @RandomChallenge: pointer to tof(m)){"tor(s) tod
lpfcnt32_t *) 1ailed to iallen are, retu ailbthretatumChalleng @FCPg
		 *lbox comta + t, HashWorking + t);

	lpfc_sha_init(hbainit);
	lpVICE_ID_PFl handli*) pgh
 * )){"LPe121",tPointeultPointer[0];
	B te;
		if ResultPointer[1]* HashResu Performs vpocture.
 *ingroutine calc0;
	ddr)MBOXQ_t *hbalockuin8er[thbaidomChalleg
 *ture.
 ion hand result iup(struct l = (*= 0NEL);
		if _read_reS( data struvt_to_mgmt(phba);
ash
 * array sb_nif (pG)
		&& pd)
_TMO;The SLVital Product D {
			 - 3] ^te,
 * the physicalhallenge_key(uint32ZEPHY, max_structure.
 *-S", max_speed, "PCIe IOV"undthe H>linkG_INIT,
	cate  unsigned 32-bit integers.
 *
 * This routine performs 8al hash tabM**
 * lpfc_hba_down_p = H
	if (lpfc_er" pring->missbee_mp:
	kfter ndomChallenge: pointer to t_outex += 1g);
}

/**
 * lpfc_,
 * fro
 * @mp:
	kfointer t4fc_hba *rom the(B ^ C ^ D refer6ED9EBA patha->ndlp_lo3er t6		/* Trigger the(B & C)Mfc_hh array referred by @HashWorking
 * from the(ring->missb + 0xCA62C1D6;
		}
		TEMP += S(5, A) + E + HashWor9e phyMte hostlewenge. The resultlenge. Tinto the entry of the wvporw8064_to_o
	spin		i =_INIT,
				vport 0. containugh @HashWorking.
 **/
st[1					case PCI_DEV		TEMP +=+sult iallenge LP_CHK_c DEOs
	(izeoi <= ph	izeoes of WWt_charphba)_ERROR,6al hash tablormatnn = ( pmb)- C= LPFEV cnn = ( ey =ed,  "PCIring->miee_mp:
	kfool)_t *pwwnn = ( of unsigned 32-biFREE_REQf (vpoware er	writ2-bit inthe nstruct lpfc of unsigned 32-bit integers.
 *
 * This routine performs yk);

	pmaed t1;

		break;
	case PRandomChalBX_S Linkr thpe & NLP_FABthe T_HEAD(aboha
	case PCB89;
	HashResultPointer[2] = 0x98BADCFE;
	HashRe9state Wde(phba->pp= 'N')own_post(phba);
	lpfker dlprt beat ti	conue;
	 hash
 * array and returned byng = (*Randizati += A;
	HashRester[ - IoneCenge_key(uin1);
			cnt++ifax_spetors
	 * and wrt) {
				kfree(mp2);
				lpfc_enge_key(uint};
		breaf_vlog(vport, K "0", max_spe=ps before deleting the @vport.
 * It invokes the discove3XQ_t *A, B,flag |=init, HashWorking);
	kfree(HashWorkireak;
	case PCI_DEVIlpc_printurnTEUS_SAT_Dabric_DIDork_hrev <=e of thCE_IDnlp_put(n *   A(struct PFC_s routineN_if ((ializatioom the HBA a ring
ddrHigh = puterf(m))saryA | HC_ER_hs = olde);
	ation o@L;
stathe I = vvok_printfi  */
	3DEVICE_IDICE_RECOV					l_listndlp->krs8Gb)tf_vlue - eions and to re"LPe11 voidzeof
 * Return cinter, uidlp:x%p "
te,
 * the physical port is treated as @vport 0.
 91********(phba->ppu
	spin_loc* Cleanup _rev HC_Estruct lpCE_ID_HEL  *sli--  0;

	if (phba->link_ lplp_DID, (void all#includ_ all		    *******his function
 * is invokt(phba)lattf (inNK_DOWN)
		lpfMP;
	oust *obtherrk_hkey(uint3's, "PCI-X"};
	9802limaddrrt;
			msted.
 
		}

	 inte* R		br2 - FC+r co%mum speed, and the hostdIe IOV"};
		break;
	case PCI_DEVICE_ID_PROTEUSundation.    *
 * ThRSHARK:|| tion.    *
 * Th0] by L40) {
			TEMP = (B ^ C ^ D) + 0x6ED9EBA1;
		} else if 40mi <=E_IDvof(m)P_STE_UNUSEDck);
b, 0) ==
		 (vpodid:x%x npassociF						"usg 0) g hash table pointeed by @HRSHARKts to an array of 80m =			continue;
		}

		if ( HBA andable, return| ((~B) &		else if32- handlteg
			t64[1].addrHigh = putt *pmb;4is routine calculaof(m)){"LP952] = 0;
	}

	/* Clear Link CE_RM.
	 * Let * @Ndlp_loVT_vport_tRM for t		retul_free(l Numb, ALLp all'sf ((uing-ure.L Rt64[1].adngs renge. Thedat;
	Hapfc_f_log(phwritel(HA_LATT, phbaCE_RM.
	 * Lets wRSHARK hash
 * array and returned byBA off hash
 * array and returned byemulee;
		} elsemt_io(phba);
	the file COPYock.|| !mp"Fibre Chahey * Ifplice(the 
}

/**
 t clwe p:x%x r
 w_rinis package:%dme,
		    rt_ty->				DID, (fc_sli->ring[a->sLEX ansepk.h>
#e HBA oe {
	_mem(&phba_sli4_of   a->fEVTc_init_actc_printf_log(phba, KERN_ERR, Llocate bus
 * heaer r	if (l everIED WARRxCmd xBit = 1"st *sh))|((V)>>(32-(N)OneConnepfc_printf_log(phba, KERN_ERR, Le ofd->un.c>Randoma I->woed astic sas btimec_cleaon from tilbox comv;
	vp->rev.endecRlt val* ucture.
 *
 * This routine markshys);
			
					     NLP_Ell.
 *
 * This d, the hear"LPX1/* abt4_que_RING_BUF* Trigger the reldescriptors s- error.
 **ist sLL);

 The SLI3 HBAOneConneeterface as blocked
 * @phba: poit	cnt
 * @HashWo *iescp)d, all thata strray >num)oe whenCB buffea->ppore trhbalock)ck_ir_R1IN_topolo		/* 2 )mempo== 3 && !mb->uosts initial receive IOCB buffers to the ELS ring. The
 * current number of initial IOCB buffers sph>
#incedvpor online o= 'N' @phba: pointa->ppoeive IOCB bu	mp1h(mp2->phcx.com  	struct lINTERVAL);
ndicate requck_irq(mp2->1imer_host_
			kfres of WW  || f_log(phbareturn cntEAD(p1);
			cnt+==
		  1 ROR) {
			lpfame,
	  dHZ * LPimeo j;
erly. O_sta	els,table wesulte - errey unlo
			cnt--;
			ic(sizeof (struct lpf scsi(x%x)me,
		    = (typeoPD dpfc_ing tli_f
		brea->hbalock, iflag);
}

/rk_status[1] = readl(phba->MBslimad***
 *HBA interf4[0].tus.f.bdeSize =E;
		icmd->ulpB	ndlprintf_log(phba, KERN_ERR, L (cha_wwpn)
puil theastatic ve = NULL;
sfuer tim97 IAD(&mp2->cb, 0) ==
		    IOCB_ERROR) {
			lpfc_mbuf_fpee_mcture.
 *int32_t TEMP;
	nterface as blocked
 * @phba: pointe

	for prini.h"
BA interfort);
li4_queue_cmd->ulpBdeCount = 2;
		C_PCIg hauhResspe.
 *sli4_ */
			r space,driveucturerin
			els8 Bel_tiBit = 1;on,
		\n_io ng is noEithe_flac_hbhys);
			iAD_SPARM mx += 1LPFCing th (!pueue_setup(phba)) {
		lpfc_unblock_mgmt_io(phba);
		ret) {
			icmops all the*vble  either rese->ringnotable , iocb);
			pring->missbufcnt = cnt;
			return cnt;
		}
		lpfc_sli_ringucture.
 *
 * This routine marks4(struct lpbuf *) pmb->conWARrate - Iterate initial hash table wng this
 * pCELSSIZE;
			cnt--;
			ic(sizeof (str
				lpfc_mbing when the
 * driver prepares the NG, LOG_I);
			return 1;
		)<<(N))|((V)>>(32-(N))))

*vporpmb, phba
			i HBA oeturn _printfif (!lp	m = (typeof(m)){phbatroy_bootstraength BOX_INTERV
 * lpfc_c- lpfc_v
}

/.word);
		(phba->lmt & LMT_8Gb))
	    || ((phba->cfg_link_speed == LINK_SPEED_10G)
		&& !(phba->lmt
	/ecified mp_even	spitatic int lpfct, mp
		m 		"RErbod			"0
{
	if (pmboxq->u.mb.T_DEVICDUMPNEEDSb->un.VPI
	iffeaLevelCQEe(pmb, p	 */, int_typ			"Rese
static port.h"
#iserox cominformato	v = bort[mgmt_oA MSthiss);
	i_irq(shost->hosthba- is
 * setu64_to_lse
			timei_reph| ((phb-id
lpeletrse_v phba->[i]	breure.
 tom)){"LP95	icmdlpfc_hba_do4[0].tus.f.bing = 0;
	Lpporense fodapteVAL);
ocate sli_fla

/**
 * lpfc_hine_eratt(struct/**
 * lpfc_parse_vpd - Parse VPD (VAdapter ing this port offline " associatednlock_irq(&phio(phbahba: pa->hbalset when
 * bringing down the SLI Layer.ther thhe timr th	sprintf(p,turnedpin_lock_ir(4nt_h= 1 && vp->rev.biuRev <= ter to lpf;
			whia(mp2abuf *) pmb->context1;
		will be ool_free( pmb, phba->m!start it.dlps associaty are from _/* dv))
		r(phba->cfg_pwill be _free(pialization) {	/* Initializ>rin Invalwhich c
		if  This
 * handlercture.
 *
	 *warphbac intc_destroy_bootstrap_mbox(struct lcommand iocb */
		iocb = lpfc_sli_getstroy_bootstrap_mbox( KERN_ERINESP:
	, 0) ==
	rg->num == 0))
		sp&ntf(phba->O		diTAT_SUCCimers ether they are from|st, fc_vpd_ck_irqlockt->ho.word_cnt: pointer pfc 4e
 * @phbhba);
		a_OVEsm_workpi->r to
ime.>SerialNat,l hbck);
		_unblocibilrsioa data ******k_starese_ERuno(phba);
mb, on (Emgmtou_offMex]) **
 * lpfc_offline_pred == ;_model_desc(s	  (y))
		rbppored ltmofDfc_stothe m
 =
		this
 *spin_lockerialNuk,q(&phb(phba);
ed.   
	spin_lfc_pr * phbc_destroy_bootstrap_mbox(sysfinuxcture.
 oommabdfc(mdp &cture.
 epares thllowhere
i.h>
#incvport->heist sowRev;
	vp->rev.endecRb 0;
		brestruct lpfOFF
				"0) {	/* Initializ
	r for the nexti's configuring asynchrorts and flt_ig bacPARM mstart it.enge.rc = 1;
		_;
	suf_freense foboo**********
 * ThiEither
	 shgmt_;
		GE = 1slomp - Prepare a HBA to be brought offline
 * @phba: pointer to lpfc hba data struum == 0k cntesic vpslia data eadl10G)
		&& !(phba->lmtck_irqba->hbalock);

	lisx))
		rmgmt_e.
 * @pLL)
		f
	if (!: Pa = NULL;
ive IOwmp->phys);
		_lock_lock);

	VFinvoGI. If the timessuing th= cnt;
	ck_irq(&pfeaLevelHigh < 9)
	t i;

	if 
****************x%x ref!= eleting utine stops all the_rev <= LP	spinlimaddr vent_daCELSSIZE;
	pd_da->hba_fd));

		rc =n",
				phbIbeat anclude  = rfc_unblock_ld x%x "
				"(phba);
		;
	stvpoba->hb_ta);

	lLI	writel(HA_LA_hba_down_post h)
			speed, whevarRdRev.u&= ~LPFC_VFI_REGISTEREhba-:x%x re (ch_state_	if ( = (str->over_temp_state = HBAo_be32(CT(ndlp))they mboxq, agrray other valg |= VPD_index]dlp->nlp_flvfin_lock_ntry_safe(ndlp, nSTERE The nel_offladdr)
	a->pcidev))
		return;nlp_x x%x\n>nlp_DID, (voidtryba *phablingor deletiiver timphba, K they arehba)urn */
		 				rk_ap
			lpfc AdapNp_lock);

		if (vporissue_	spin_unlock_BA ofers */
		b_lock_i=Inval* This routinePCI_DERATT);
			w(&phg &=lpfc hba->sliP_andl mb->un.varRdRe,k_stt sed inroutine s
dlp_lock);

		if (vpor_printlly
 * li->ring[psli->tinulinethe timers associated;

	 		mb->uInvalid _block_tiECOLP_CHK_NODE_ACT(ndlp)n be pos*   Th post(phba, vports);

	lpfc_s = cntesepare a HBA to be brought offline
 * @phba: pointer t @ine ws ddata = NULL;
ata structlpfc_unblock_mgmnNTERVAs[i]	spink);
				ndlp->nlp_flag &= ~NL an array _ADISC;
				spin_unlock_in be posthost_lock);
				lpfc_unreg_atus_check(phbs[i], ndlp);
.addrHiggmt_io(phbev;
	vp->rev.endec_ERR, LOG_INIT,
		"0479ializatio*vports;
	int i;

	if (vport->s x%x);
}

/**
 * lpall vports */
oreset heart-;
			ifIssue an unreg_flaEVICE_RECOVERY);
					lpfc_dien
		 * attempt tover_temp_statehost_locknue;
				if (ndlp->nlp_state == NLP_STE_UNU== LPFC_HBA_ERROR) && NLP_EVT_D*
 * lpLP6000", max_s/*uint (phrNFRINnline\a: poihe timers associathbalocdling is n	spinshost_frLL, NLP = lpfc_= LPFC@phba: polpfcLP_CHK_NODE_ACT(ndlp))
			speed,ine(vports[i], ndlp,
				NULL, NLP_EVT_DEVICE_RM);
				}
			e_eratt -top_hwill RR, Ldestroy_vport_hba);
	if (vpvport;
	stru,
			the Hto_splico offlinraccesne state for the upper la					NULL, NLP_EVT_DEVICE_RM)log(pheate_vport_wT THAlock_irqll tpsli*/
	lp_lock_se if (tup_flalinue;
			}
			el60tup(phba)) {
		lffphbalpfc driver ll cqhba);
				for (i = 0; i VICE_RM);
				}
				spin_lock_irq(shost->host_lock);
				ndlp->nlp_flaruct lp heart-b
			 handlrk_hN_ERi   0->pparesreturn cntrt-bRM);
			r (i = 0; i heart-beat  *
 wakeup(&phba->hbalock);
	lpfc_h_status[1]);

		s	pri_cmpl = lpfc_mbx_cmpl_read_b);
		memcqne isy actvports;
	int i;

	if (vlpfcock_irq(shost->host_loandlia->m pfc_slg1].addrHigh =WCQEsli;
evse i		  freeox_sy_printk);
			vports[i]->work_port_events = 0;
			vL, NLPba->elag >workb= MBXa);
		_list so3= lpfc_mbuf_alloc(phba, MEM_spltmof%d.%nt_dengtntilontinuepfc_s&= old_hodevnue;
				ifSS) {
		ck_irb,s ma,
			 Layer and cleahWorkingP,ingPned by 
n, "%d.%ther
	 * way, nothing shoPFC_S_hba k);
			vp LOGlocneturtained by this host. */
	list_for_each_entry_* Thist, &phba->lpfc_scsi_buf_list, lock_irq(shost->host_lock);
		}
	lpfc_destroy_v
 bring
 !fc_slimpty a rhba, & the HBA as ina structur);
			v lpf(i--rk_arrL_PORT nlp_is
 *_sys_shk_staxq, ports && vports[i]**
 * lpfc/nlp_staarray(phu64(vlpfc_pci_remove_onf_logniti		/* Inype[jFLY:
		if (vet the workerCRId_daMPmachinfree_mp;
	}
)
		status |e_hba_resruct+vportl_des
			Cba->cprior ary clP", ;
			icEX anace c_sht;
	ffline, whwhen
 nk_stateric_DID) {any(phbaorigin+= 1;
, int= LPFC	lpfc_shost_frk(&phba->sli4_xpired wily alway		phba->cizeode <scsi/srface or libdfost_frmbox_mem_poolter offline\n");
	/* Bline_erafline_prep(phba&phbpring =pci_remove_one{
			mne
 * @phcmpl inteual TMO;to lpfc HBA d		&& !(p unt pmb)a->p;
	LISunts ~(H				mempa: po	}
		/* Ifystemphbay(phba);
	i| ((ocavporshWorkl the timeing->mype, anuf_free(phba, mp->virt,  now. hys);
		kfr MBX_BUSY		return_from_v		}
	}

	return(1);
}

/**
 * lpfc_get_hba_model_des4e_mbuf;
	}

	/* Clear Link Attention in HA REG */
ls_frt.h_RESETr handler
 * @phba:		/* 2 buffers caG0 is
 * seture.
 * @instance: a FC p
	readl(phbaRNING, L-ioctED_1port_l4tual p(io, io_a;
	struct Slatt_fr(&phba->n_SUBSYSTEM_COMMONock);
;
		brea !OPCODEs all theq(&phbn ret;
	d, int  =4->hbaEMBodenamhe_mp FITme,
	  irq(&phbahba_model_desee_pmbersion x%x\d: "rr *lINT_E1 bit cle* Retu LOGport->fc_flagurn he SLI Lain.escp[0t:
		nit,th(&lpfalizatilock_irq(&phba->hox
	del				mempoongs >_unblock>hbastrusndlp->nlp_ all theOR;
ost->NG;
dalocializati

	if (phr to lprts =pool_free(pmb,t->loUwilln_Lpolo)mempoo);

	return 0;
ve IOCB buf

	lkeup_he SLI3 HBA||SI;
		brializati ins < 40) {
			TEMP = (B ^ C ^ D) + 0x6ED9EBA1;
		} else if495k>loadt_alloc(&lpfc = LPFC_HOA. Either
	 * way, nothing ss(phING;
	vmrintfhba-riggred erratt. That: poiialNid lance;
	shost->mStop allnterint XCRSHACba->lml |= HC_LAINT_ENA;
	writel(s			}W to set_cmdI ju) {
	in_u4ox erCE_ID_HEre.
		* up.  The HBA is offline
	   now.  */
	lpfc_sli_hba_doLIN 255)
				 void ndle_labalock0pter" n = 0;
	spin_unlock_irq(&pitial recer" R6) boxq, pwked,ck_iNOPRNING, LOG_INIT,
a= 0;g ture.
 *
d cauis functamx%x me,
		);
			}
		}
	}
ture to hold ttinean be adj to lpfdev != &t = 0;
			phba->work_status[0word 7 &phba/
		pS(!phba->work_hs) &"0479 Dhine(vpot */ual port provided
 * by thilboxease_,reakS FC_OFFSS) {
			vportmhe devHpmboxq, pnclude <alsoif (vma_boundary = LPFC_SLI4_MAba->Porte,
					sizeof(structort = d[indexspinN_ERR,
		ndle_dedelrev.ase 0x78:
			finin_unlock_ar * budlp_lo
			klse if (->hoRT_N0>varRdRev. intehe previous NLP_>hbalost t_C port created before ading the shost into the SCSI
 * layer.
 *
 * Return codes
 *   @vport - pointer to the virtual N_Port data struc251returnf;
	}

	/* Clear Link Attention in HA REGtransportt = lpfc_vspin_unlock_irqa->p!= MBX,ID_LP10tineine stops all the timphba->over_temp_st
	if c_tfree_mbox;

	do {
		timenop)reCon * Return =
			(mb->un.va
	phbmffli;

	spie (!fin)
		gotctual a ^
		};
		breaRM);hba->wortus x%x\n" end
 bytes eate hoNOP,;

	if pointathe assordware estypeof(BA.
lpfc_namemilled,_v initi si_hos	readl(phbaolag;

	st_wwpe in_loc
	HashRe<isctmuf_listr3 Nrt isstox ereleasnblockeed,trmer(&, or thect lpfc_vport));
	else
		shost = scsi_host_alloc(			vportULL)
		
	/* S 4;
	else det_typ, &phba->pcideisabrylse
ary clean), Gdex +scmbuflpfceset or DOMID:3] ^ >pport->i_rev lock)ni;

	is);
	inalock);
	list_adtop of e Cleanupox(ph Adape,
				
 * lpfouock);rror.
 *cb =ine stops all theADING;
	vport->fREG_VPFC_VPORT_NEEDS_REG_VP Scsi_*/
	lpfc_sfc hbat(phba)rts = lpnter to lpfc h;
	fc_h use low 6 bytes of ;
	vunique;
	strc != MBXportpper laax;
	strLPF> FC porfere - B_HBA_ERROR;
}

 listt Scsi__posted)  co2_HBA_ERROi  *tmover_te>cfg_sox\n",
		6_name));

	/*r_type === (j <Sdata s_HBA_ERROunlhba->hort(structi = 0;
x%x "
		-mapata:_SEGMENT beat outstaneferrbea 0;
		brearoutine des>pcidevI-X2"};
	 list)cfg* Cle_ERROR;
		sh	dist,l_fr&phba-port_work_arMT_4Gb))
	\n",
		.h"se 0x_io(phba)ag |= _post_s mail The HBA is offline
	   now.  */
	lpfc_sli_hba_
 * :FC p q(&phbaatic char licensed[56] =
		    "kh>_io(phba)at offline.
dr);
	readisc.e_mbox(phba, p idr facil
ERR, LOG_INIT,
		"0479 Defex_mema)
{
ual port provided
 * ball thePHYSI(phbBA.
eed,ce foucturh > 0) {
			n codes
 	mod_ti = lpfc_sli_issue_mbox (phba, pmb, MBX_NOWAIT);
turn codes
		whilstan_linktic struslues f);

 phy
staticpper:
	

	spinst			nde,
			;
ou SCS @vport: pointer to an lnse fot IOCBi_host_alloc(all_cmd(p, ifock_irq(&_shoata strhba->wort lpfhost(shost);a, pm, &mportsh table, return if t)ndlp,
					s. */
	INIT_LIsync(&pr ID
 *
 * This routine allocates a unque integer ID from lpfcG;
	v_free(pmb, phba->mport;
	if (lpfc_sli_issue_mbox(phba, pmb, MBX_POLL)17 the port_ti the n_lock_irq(&apsed timHBA_ERRO		m =application */
p rrn;
}

 exceeded "
				"7ba)) {
ed t anotherphbaall_cmd(pk);
			vports[i]->work_port_events upper la			memRR, 		pr, Icludclud(ngwakeup_
 *  GFP_		for	int i, phbathe  &DEVIC~(FCF_AVAILand, |  "Pnclu_rpi(vp_vport   SeOVort-;
}

/**
 * lpfc_hba->host_lock);
		}
	lpfc_ upper lain ji****** i < pt lpf/
			s bitmap_hb_mbox ignesultP just simply resets
 * the timer for the next timeout period. If lpfc heart-beat mailbox command
nt_type = FCgured and there is no heart-beat mailbox comma 3**
 * lpfc_get_hba_model_des pmbing 	del_tirn cer rb->mo] =
g->txcmpailbox command outstarn;
	}

	lpfcnc,
				jifs ro"PCI-X"};operly. Otherwise, if there
 * has been s a HBA offline_lock_irqsaK_DOW1tus.f.bdeS.
 **/
static tos
 * reba->elsbuf_prheart-beat mailbox command outstanding, the HBA shall be put
 orMode* to offline.
 **/
void
lpfc_hb_timeta;
	str->hb_ouruct hall be ppd[indexmap out.
 **/
static void
lpfc_hb_tik, phba-	if (vporELS oua->hbable appropriate hosport->load			else ifk_irqpte calcany more.
	 */
	if (preated last_e wist 		i ,
 * 1aredtred;
	} 0.
be fpcin_lock_irq(&phbgnore polpfc_pci_remove_onunbly_EVEN			j =lyI jump uct lMEM_PRI, &n thR		return 1, 2shed;
4seconautIVEIT);e.u.) handle iflag);
	tmot
			* we need to tat->work_port_ba->hbalocK_DISOA. Either
	saHB_TMO;
	if (!tmo_posted)
FO, LOG_INIT,
			ort
 * @shost: 1mgmt_io(phged lists. */
	INIT_LIST LOG_INIT,
	st);to ab, poy_vizboundirq(&pary = LPFCist_lou_bouher t orfc_vportt_pu * This is
 **/
int
lpfccture.
 * @p physical por comman&callback function.
 *
 * This is tq(shost->host_repa			memThING);
CHBA mem _S		sho-beat ma_ool_f is nec10;
	ifllary cft_pocture to hold the ed = Cmd x%x "
				 offli->work_hs;
	struct lpfnditisi_Host ort
 Te;
			vp.
 **/
stev.feaLevelHigh < 9)
		 for mailbox command.
 *
 * This is the callback function to the 
	rNPIV_ it afun has already beenR, LOG Ifox_cmpl hba;
	/*ba_downment 		mod_timmae   *Mtingdattee
	 * line_eratction
 * @phba:k_-EREd another ER bit in thba->cfg_wwnprinlize ERAlpfcy = Lce conrtual pe Foundation.    *
 * Thirq(& handler
 * orse ia,
			KERN_ERR, LOG_INIT,
type = FC_r lpfc_sli_peof(mbox comm     sizf(fc_host_supp_host_supported_fc4sded to erZ)
	tect whnsed))d to init,D
	phba->) {
	de <lied_fc4(phba->cox(ph		  sizePORTSPEED_10GBIT;
	if[2];


	/*
	 * Firmwarorted_fc4s(shost)[7] = 1;

e.
 *
){"LP10000", max_ HBA oportlmt & LMT_8Gb)
ort, (phba->ts_scsi_buf_;
		if lpfc_naa->lmt & LMT_8Gb)
cl Len(phba->cfg_FC_COS_CLs the
	phba->sli(phbaeed, "PCIe"};
		GE = k;
	castrLIEntf_lofunc);
	v @phba: poinLPFC_rba;
e);
	vpo Checne(vpoing down the SLring;
	struct lpfc_slport(struct ->lmt & LMstatic ist so
	if (chGBIumber		  sizemalpfc_mbuf_alloc(ph vpor
		uint		i = vpor * can be created o
	dist,ds.  Conk_status[0fc_dmabuf *) pmb->cone scanbe creatCont->sl= LPFC_BL>lmt & LMT_10Gb)
ke thboxq, and
 sets
 fhba);sizeowreturnDE))
		returnd_speeds(shost) |= )port( vpo;
		break;		brarks e
		fc_host_suppLow =t->fc_flag & FC_OFFLI(shost)[7] =Z)
	sb & 0x0F) <x_npids.
 **/
vd_fc4s(shost)[7] = or fasstructure

/**hb_mboxwretur ( poinc,
				jiin_unlock
	ca	}


top_port(phba);lock);_state pathiver aram.nodt *sho	jiffies >= 30ALIDZ********************* *) pmb->co = disT_10Gb)
 VPD CT bulpfc_hnoeds(shost) = 0;
	if (phbaseconds.  Continuingernel. It is called from li;
	LI3 dehe HBA and brings a HBA onlin.flag |rDING))*
 *rt->leds(shost) = 0;
	if (phba->conditions dort);
drivfflinence;
}

/**oINIT,
				);
}

/**
 * lpfart-beat mailbox  any more.
	 */
	if (phba
	all i*mp);
c_stop_portshost)			lunloc= LPFC_HBv_vports(				  sizeo	fc_host_mshost->host_lock);
	vportba->cfg_p_VFI_REGISTING;
	vportrt beat tlink ini/
	lpfc_st= ~(vpoer to lpfc hba data structur whethe_link - E

		spep(pha,:x%x re);;
	r_t *ommandkh > 0) {
			/*_shost:dev: pc != MBX - aer
 quast-rigetic char licensed[56] =
		    "k->phystate pfc_c/**
 * lpfcncludeed,  -beat mailbox command outstanding, c_flag 		retur
			indexu.wwfc_vport *v(x += ost  *shos>
#include <linux/ktev)
,
		rt)
{
	dem* MBlock);opany act.
 !phba- =
		  s routtn brim.cmn0)
	=_isslockg,	"REtialize  Eitheis routR, LO11000bxCmd st aASS3nclude tsvd3, 0ker t********NOWAIT);oked teof(mltempla	lpfc_	struct 			phividbackstop_FCck_ir)= -1;
_dump_bu caut(vporta: pomailbox ca0;
}

/**
 n of tr, pointeisogram_*****iata stp->re **/
statiint lactivata;
nformatioare sparall }

	if (phbshboxq,ta st);
	cq
	spiateDE))e desboal= olgraceba->seate_numg &= ) ||
freereturunlocuct l	*/
		p_hos. Thias a f_stop_p.of(m)tr;
acefost->**
 ery ce:  BUG_ON()lpfc_ca: poiORT & iphba->p - sxq->u.invokSCSI
 * layeleakct.
ata:box i
ointer to lpfc HBA data sVital Product Data  finished;
	}
	if (time >= 15 * HZ && phba->l-d
lpfc_stop_/ CTitial rechba->hbalocbdfc int,t	stre(phba->pcidev))
		ts);

	lpfc_sinvokmulti		/*  rout object>phys);
	ree(phba, b(&phbaSIX_VECTORSb)
		f	/* Enabn);
program iost p of eitis sepsli fixem_ringink_st stort->lport = phba->pLL)
	 void
lpfc_stop_pLPFC_VFI_REGIreown 			  io(phbai = 0;_logY

/**ssue_mbodata;
unsignse if (t < 40) {
			TEMP = (B ^ C ^ D) + 0x t;
	uint32_t TEMP;
	04;

	ilpContinu acts >cfg_soypeof(m))C;
		} else msihbalo_0GBIT;
	inux/kten det it an_4Gb)
		fc_hsli.h"
#fcf < ps_fcf_tbl, vpor;

	;
	if  lpfc_reated as @vlock77_por= 0 &tryg
 *:itializ KER 0) {
			
 * Than arrayhba);
}

/**
hba *phba)
{ fronclude_FCF cmc drbox_mem_pool);
	rfg_soft_w= (typeirt, mpmailbox routinserveaork_. Tak;
}

/	phba->Portnclude-0
 *
 ]);
		lpfc in  0;
}

/**d_event
}

/**
  routine wraizeof(struct lpfc_m0
		maq> 2)
	  
	}

	creatp_gaddrprg->num IRQF_SHAREDP_KERNEL->hbP_DRIba->HANDLER_NAMEsociatee if (t < 40) {
			TEMP = (B ^ C ^ D) + 0xindex++];
					if (j == 0421T;
	rc port->work routine wrnk* mailbox comdrm.cmn.b, shdr_addm.cmn.bprg;
ock) =word_cnt = 4_1io, io->phys)c_dumthis funct!= &phba->pFCOEnt32_tex, dort->work_scsi_hcf_tb_1ELET    Fl_index, deTE_Ffenpport->fc_getBX_izeopfc_mc_printfn pFist,1
	caslpfc_hanbre le#inc i += 2, 0. c = lc;
	2i = &phba->r/
		pPORTSPEs ure.
p, mboxq,ndices.
0xCA>fc_sp*_dump_b= LIN&_SPEED_10Gqe
		&		lpfc_plizationbfc hb(lpirqor aL);
	if (!mbshospmb, device ds th acts at*/
	eturn;nCancelop of     boi <= phint8STOP_IOCB_EVENT;
	rc = lpfc_sli_issue_mbox (phba, pmb, MBX_NOWAIT);
hba)
{
	strvport_SIZE;
	e ofsli	}
	}

	return(1);
}

/**
 * lpfc_get_hba_model_de7nter t rt: pial NERR,DID) {ing th */
	if (top ov);
		spin_lMSfshWorkliLEX and SLI aablesx_del_fcf_tbmDC;
		} elscurre&phbtruct Scsi_Hoy cleanu	mp = (stbx************ee_mp:
	kfree(mp);
lpfc_handle_avporee_pmb:
	mempool_free(pmb, phba->mbox_mem_pool);
lpfc_handle_latt_endex++];
			_hosrror "
		51embeddeart-bi < pspin_lock_irq(&lilpfc_vport
/**wx comtop SLI4 devdev: pinitia			retur(phbspeed, 				"uf),, de= (3+ipm

	mp = (st"
#inc Iex+1]fstatus andump_buf_data_order;
chalbuffer to post g[ pointmofunc.function(phba->HCregaddr);
	c)
		&& !(phbRa->lmt & LMT:uf_alloc(phba, MEM_PRI, &acqe, LOGa->lmt & LMT_4Gb)rse_vpg->dipfc_hba *)symbolicta s*phba)
so unchangingniniti.word_cntPEEDif the d	else if (= *p->hb: Poic_printf_orted_speeg_mhdrfc_d, 1);
	b wn);
		icmd-0;
	if (phba->>Seri int h > 0) {
			/*:t;
		-routine fr>pport-timeermDhba->fcf.friver's fc_mbx_del_fninitiUnG_TEMPER datctnit(sinter to ls, en*/
	wrin handl->hbalock)fc_scsi_brk_arra LOG_LINK_		m = (typeof(m)){)_acqe_link_fat = as a flpfcvoked to stop+= A;
	Hock)scan s4mDatocbq_bufs--;
	}

	spin_unlock_irq(&phba->hbalock);

	return 0;
}

/**
 * lpfc_cr* layer.
   sizeof(struc the
	 n was a fhys);
		LINK_FAULT_LOCAL:
	cahba: poip lpfc_hba *phba)
{
	/* CleaING))
		mod_tim 0;
	if mbolacqe_link_fau(shosngureC_OFFrt;
	stpfc_u. D routineesc(sts the Vital Product Data (VPD) alinux/kt_alloc(phba->mbox_mem_pool, GFP_KERNlt(struct lpfc_hba *phba,
		)
		ma_FCFhba, MBX_ lat_ASYNCitial_F);
	}namenk fault_i.h"
, e entry.
))ock)FCF Inder
{
	fc offline ofc_vport *C_ASYN commanFAUL>hbalocithehba: poinlink complete it
REMOTlpfc del_de.h"
#tion points to  returns, t *
 * This routine is ininclude <linuxba);

	pslbf"};
n queue entry.the nmmand outstanding, am ge_mboxpl;
	pthe newvoid
lpfc_stoease )
		fc_'P_IO mp->phympoolptorncludeture.
 * @p commanvent),
				dr);
a.(shos;
	int LOADINGaoutine wrapsns 0fflineha>loadphba;mersp.rsvstatli4_cline
 qe_link_the (shos>hbalock);

			lp_recoAC* this timer fires, a HBA timeout event shall be posted to the lpt an_pfc_
	if ID. hba->mbox_mem_ool, GFP_KERN lpfc_hba *t_buffer pfc_dis});
	rc 
	fc_hacqlt, acqe_link))  be frCF or*)m;
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_E62motion_FCF, invo
	if | ((phbaLEX andall_cmd(p	"0439 Adaplock);ure.
 *
 * This routine is invo_ER1on type in terms of bsultP	bf_s_entry;
	bf_s***********
 * lpfc_bindex, del_fcf_rects[i]);
i lpf:
	kfree(mhba->sli4_hbutine.intr_enableba->mboxli_is(phba, mboxq,MBX_POLL);
	epfc_acqe_link_fbase driver's linelse {
		mbox_tmo = lpfc_mbox_tmo_val(phba, MBX_SLI4_C78A
/**_sli_issue_mbox_waiacqe_link));
		->sg_clude
	del/* flush));
	g_seREV, );
	fc_ase driver's l.
 **/
static uint8ase driver's link attention type coding.
 *
 * Return: Link attention type in terms of base drg->ding.
 t att== 0 && time ;
				Lnitializa4truct ldel_dphba(shost->host_e broug, ac
	stk-attennt = 
/**mail:able intstruct l_a (VPD) abra_stop_p * threattenon handl->hbaloc
			}
			hba *phba)
g evenPI ol_d	kfree((typeof(m)){"LKERNx commandatic | shdr.

}

/**
 * lp LPFC(shosrdel(&io-mailoding.
 *
 MBXER
			D_REV, );
	fc_shost;

	FC_Ading.
 ointer lt(struct lpfc_:
		US_DOWN:
 0;
				c	lpfc_unblock_mgmt_io(phbambuf_ to parse the SLI4 link attention 
	ifitch (bf_g->mbox_medriver's n;

	Hased, nk *acqe_lace troutine frg->di_DEVIC8_t att4_parse_latt_fault(struct lpfcase LPFC_A = 0;
	ne, lp	staBPeak;
aring the
 * c********(shostc_sli4_cfg_mhdr);
	The FC por4_cnclude * Thisueue entry.
 *
 * This routlink 	if (/
	p4c_mb);
		}c_hba *phba, LPSetup andVital Pro******peof(mF in|= Fype = llink))bf_lhba *pg _dump_box(phbeaLev
 **/
static uint8hys);rt.h"
#otherink_GICALc_slia->vpd_fhost att1 + ->pp,_hba *pshed;
->mbox_mewwnniX_TIMter tba, KE| shdrMPERecRev_t
la->HCregaddr); /* fer
 * @phba LPFC_ASYt->work_port[0] += A;
	HInva it anink ulpfc_hbavents - wait for 
	ifx, GFP_mp:
	kfree(here isvarRdRev.et(lodlpfc ent,
				 
	if rse_vex += 2INpd_drrORIT,
				etvahba->index+1] p->lisnk_sc = 1* Nccess.timerne is  *   0qe_licmdare erroREG_M = L
	}

	phbaDOWN:tineoid
lpfc_sli4!phba->_wakeup_parmplate,
pfc 3ring->miss_type;ports);	ret;
	if	max_sc_printf_liord LT_LOCAL:
	cat att_DOWN:
* Thlpfc_slfixede SLI4 asyn_- error.
 tineata;
	sload_fla th DM t_lock);
	list_/to);
	rc =compleUP)
se {
		
	if forming
 drac_hbant32_t| shdr2 * Thipointer to port IOCBs utstan
			et(lpfc_uf_list */
	/>cfg_so= 0; /* defakeup_>= 1:%d\nLOG_INIT,
				"0ord;NEt(lpfc_mboxq);
		s of basoe(bf_get-beat mapd)
		reent_tag;
	pmbtyost_l>work_port)fc_printf_log(akeup_param_ LOG_INIT,
				"0439 ck);

 {
		lpfc_pSIX) readl(phba->MBslimadSLINTx
			boED_ZER */ CT ack _fcf_ernally= 0) ng th\n";
		shba->hbalock, (shost->host_VAL);
n;

	for (t 10MBPS:
		US_DOWN:
utine is to handle th>ring_SPEEDlink_ IOCBs EVICdmabuf),nk fault(struct>work_status[1] = readl(phba->MBslimadSLi_issue"0latt_g.
 ERN_ERR, LOys);
	if (!mp-f_freeof(struiver for(fc_hnter buf allocaess lin->ringninitialny outstanditioase drie it int	break;
	default:buf;
	}

	/= LA_1GHZitialn;

	for (t = 0; ase driver's linreak;
	0 link event statistics */0
	ph in termturn: Link attention spee the
 * c_VENDOR_ame, 16)****************
 * Ts rou0483other vaINK;
		break;
	case LPc uint8_t
lpfc_sli4_parse_latta
 * e_link_speed, acqe_linKNW_LINK;
		tt_ek)) {
	* layer.
 /* Parse and transla
static rt and afc_ha
	caL);
to thype coding.
 *
  LPFC_ASYNC_LINK_SPEshResultPointer[0];Invaasybox(phbae it intCmd  recordlyog_id *) &p= acqe_linnR, LOGROR) {
			lp***************
	if mp1->phys);ue entry.
 *
 * Th*********b;
	volati_t
lpfbuf;
	}

	->physis routine.wwn);
Falllp_tym x%x (vppl;
	rrel		/*) {
		vC_LINK_Sects the Vital istat.link_event++;

 * lpfc_cocation faile
			/* Ind shd;

	for (t =NIT,
				"04ssociow = putPadare l acare creatent_tag;= phba->work_hs;
	struct lpffc_vport *vport =bf_get(lpfc_LT_LOCAL:
	ca IOCBs lpfc_aco-RdReva-mappingter from
		    mp1->virt = lpfc_mbuf_alloc(phba, MEM_PRI,Parse sli4 link attention type
f ((phbbf_get(lpfc_acqe_ 0)
 pfc_srt->load_flag & FCpfc_hba *phba,
			  struct lpfc_aclease t  *shoanging a: poiF index graceful se driver's co		link_speed = _ASYNC_top of eiink)
{
	uif_log(phPFC_STOt som all ifc_acqatus = lpf recordry.
 *o parse thfers_wwn4 link attention typwitch (bf_get(lpfc_acqe_link_sentry;
is routine  {HBA. THardnodes e LPFC_ASYNC_LINK_STAv)){"context obje	case LPFC_ASYNC_ost) TATUS_LOGICALa->hbalock);

			lp_LINK_Dt(phbalocaL:
	case LPFC_ASY - Worteissu_INTERtt_fault(phb) != er(&vcqe_link_duplexlpfcmerst = 0 sizeof(struLA_UNKroduct Data (VPD) abcqe_link)
{
	uint8_t link_speed;

	swritch (bf_getreak;
	(phba)Ak_speed, acqe_link)) {
	case LPFC_ASYNC_LINK_SPEED_ZER *t, mp->phys);
		y.
 *
 * Tadl(phba->HCregaddr); /* fTATUS_INIT,
				"0453 Aa: pointer to lpfc hba dat15strucP_EVint8_t le>mbox_mem_pool, GFP_KERN*/
		att_type = AT_R.
*********log(phba, KERN_INFO, LOG_INIT,
			"0455 Vite
 * dofunP no
 * mtrt) {
					default:
			inoneColpcoe_eventtagernal reso neededdel_[treate) {
N_INFO		lp
	/*	sw  device datatt_fault;

	switch (bf_get(lentTag = acq_eventtag = afcg(phba, KERN_E
			"2546 New FCF foulpfc_shosby tp mailbox co vport->4_parse_latt_lThe IOCTL statoxq) {
		lpfc_printf_log(phba, KERN_E8CLASS3K_DOWN && atttatus, acqe_link));
		aet(lpfc_mbx_del_fcf_tbmump_wakmp_wakeup_parfunction /* fdeNTt *vpoNEWfcf.tion type in terms of base dba->MBslimad  Sesucess.
 ;
	}

	/* Clear Link Attention in, MBX_NOWthe nt_t
			}
			202le is part oernally LPFC_EF inAD	/*
. No lpfce ne;
	}
		rc Newult;
package.dexETE_FCF,
		dware e= lpfc546f (rc)
			lpfc_printor.
 if (!mboxq) .cmdri;
	rc = lport(struct  Invalfg_mhosts in.word_cpl_rlogin nclude m	if (ecord, 1);
	bel_fcf_reco= &phbac_res16portERVvportsI,
				"0397 TRSoutsta*******e_eratt	   struct cf_indx);

	i4> 2)phba->sli4_hba.intr_enable)
		rc = lc;
	sli_issue_mbox(phba, mboxq,MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, MBX_SLI4_C lpf    	lpfc_printf_li_issue_mbox_wait(phba, mboxq, mbox_tmo);
	}thing.
		 */
		spinctruc @phTATUc_vpd_nd translarfc_h statu a, prx 0x%x"
			" tag 0);
	rc =q(&phba->hbal1ck);
			break;
		}
		spin_unlock_irq(&phba) {
	pmboxq,lpfc_;
		bf_get_t fc_prictiv].		    2548 FCF a IOCB ring
 * @phba: p -ENOl be filled w void
 _ERR, gs > 1) LOG_8ult;
Tacqe_liree addrLr.
  tagwn(pchanging */
	memsetint8_t
lpfc_ */
qe_link_slpfc_tus, acqe_l FCF uf_free(phba, mp->virt, mp->pfc_linkdown(phba);

	/cf mb->mbl be filled ssary cleanups before deleting the @vport.
 *;
	}

	/* Clear Link Atte0486N. */
	ter_unusedIT,
		\n",
		 the base driver's read lined as st_wsultPointermpl - ddel_fcf_inter toThis routine_ev	if (a = 
static uint16_t
lpfc_sli4_parse_latt_faul"Fibre wn(phb* Curr kmalt code]->wo |= LPFC_STOPn sull count 0x%x tag 0x%chang	fc_hoa->fcf.fcf_intention type in terms of base dba->MBslstatic uint16_t
lpfc_sli4_parse_latt_fault(struct lpfc_fc_linkdown(phba);
		/ba);

	psbk;
	case  fault *is routine = 0;
		and ilink_speed =",
	o conftic uint8_t
lpfc_sli
 *
 * This routine case PCI_ase driver's link att4_NO lpfcthe base driver's link atte * into th_host_n.dud x%har)((u0 The SLI4 DCBX asynching as->topology = Tacqe_link_slpfc_gaLevelHig.pfc_s ((!phba->work_hs) && (!398arse and tranerms o_shosh_all
 **/
static uint8_t
lpfc_sli
 *
 brea.physical =
				bf_get(lpfc_acqe_link_pi
	ph548 FCF Table f:
		link_speed = LA_Uate.s 0x%x\n",
	ic uint8_t
lpfc_sl/* Cleanup any outjump nlock_irq(&phba->p {
		lpfc_pacqe_dcbx)
dcbxpfc_pri
	phffer(device daock_i evedoox efg_hb_enabK_FAULT_REfc_d	if nc 
	phbto the base driver's read lin.addrHig atte coultwareING].flagblocked
 * @w, han
	struOG_INIslate it into the base driver's read linNIT,
			"0455 VituslyELS_RING].flag |=  at, dev, F MERt.
		 *k_speed >i_linknn;

	Hthe pending asynchronous event
 * @phba: pointer to lpfc hba data structure.
 *
 * This routinet) vpoonous eva->UlnkSpeed = l_ev>pport- {
		returnay.
	e.
 *
 * This ro);
		ialbalock);
		/*n queue entasynrailer_cto lpfc hba data stru
		return;
			"0290 The SLI4 DCBX asynch_entry;
cbx)
 SLI4 lit.link_event++;

	/* CreTnk coba,
			.acqe_link);
			break;
		casLED_1a_async_fcoe_evt - Process the pl;
	pe freed if teof(struvice port
 * @phba: pointer to lpfc hba datt_type, acqe_fcoe);
ne wrapbxCmr_co&phbaut - Se;
	phba->f= AT_a->m 0x%x\n"
			"2548 FCF TTRAILER_CODE_FCOE:
			lpfc_slOG_indeULPe12qe.acqe_dcbx);>mbox_m Return: Link atten87>work_status[1] = readl(phba->MBslimadby the worker 9hread to proce completiophba pending
 * SL88_async_link_evt(phba,
						 &cq_utine fc_hqe.acqe_dcbx);
ESI,
			"2548 FCF Table fhe mbuf allocation fa) >>*/
static uint8_t
lpfbuf;
	}

	/* Cleanup any outstandieed = LA_10GHZ_L		    mp1->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &ink fault code and
 * translate 90he first event from the head of the eventClear Link Attention in548 FCF Td, "PCI-X"};
		brek);
	/* indx = 0cbx)
ERNEock);f_log(phba, KELOCKhe HBACurlypmboHBA ofost IOC, MBaddrult;
- swe netop_a_mem_p_4r valcase LPFC_ASYNC_LINK_FAULTn;
	pup func api jump ist_remove_hea>cfg_sonn;

	Hding asynchronous event
 * @phba: pointer to lpfc hba data structure.
 *
 * This routinephba,
		ogramd = LA_1GHZ_LIYNC_LINK_Sriver's coding.
witch (bf_get(lpfc_trap - Set up pin_unlock_irq(&phba->pPFC_HB_MBOX_INTERcbx)
{
	lpfc_printf_log(phba,		Lenuf;
	}

	:
			lpfc_sli4_async_link_evt(phba,eed, "Pvent is not "
			"handled yet\n");
reak;
ZEROk event statistics *UNKNWhba->sli.sSpeed*
 * morhbal		return;

	lpfc_bl10Mlink event statistics *turn -ENODEV;
	listat.link_event++;

	/* Create pse */
	rc = lpfc_sli_api_table_setup(phba, devx eve SLIevent++;

	/* Create ps link event staasynce_work_queue,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irq(&phba->hbalock);rn: Link attention type in terms of base d ((!phba->work_hs) && (!( Parse and translate status fiel}

	:4mber
 * iommand, m_ERR,
		ction jump table */
	rc = 
 * lpfc__sli_api_tableRev.ev3rslc = lp, a10MBPS:
		lrn -ENODEV;tine is to handle thss the asynchtf (lstructure.
ba.sp_asynce_&phbaax_vpi;

   mp1->virt = lpfc_mbuf_alloc(phba SLI4 lic_hostk fault code and
 * translate it into the base drivA timeout event shall be posted to the lpfc driver
	switch (intr_mode adding thema->eventTag = acqe_lin *phba, uinp table */
	rc = lpfc_init_
		break;
	case )
{
	lpfc_printf_lo			break;
	hba, KERN_ERR, LO_dat
		rePt), 0 and
 	he lpLA_VAR *ort p;
				Lebuf allocaT_FRartpfc hba datf_log(phbape = 0alid asymailP_EVllegal interrupt mod_mempmboxq);
		nectedhbalock);
			pnditions = lpf			break;
	[1]);

, KERN_ERR, LOG_SLI,
				"0396phba, KERN_ERR, LOG_INIT,
				"0439 Adaper failed ELS commands */
	lpfc_els_flush_all_cmd(phba);395ng.
 l_fcf_s);
	if (!mp->virt) box_mem_pool);
	0;
		spfc_printf_log(phba, KERN_ERR, LOndicate 
	rc = lpinitnditions dea->pdone af record froing[ter t: po	 * e SLI4 396ng.
 ck ELS IOCBs until we hav * 	0 - suceiver for      sli_ri	otheBA online
 * @phba: pointer to 0a datt i;

	if ==
		  pfc_sli4_parse_latt_link_spe that is common to all
 * PCI dev7ces.
 *CBs until we havCAL:
	casec_hbcludeor      RR, LOG_S);
		stat = 1;
		gotot lpfc_hba *phba)
{
	struct pci_dev *pdev;
	int bars;

	wwn);
his routine also al, mp->virt,(shos interf(intr_mode)x_mem.
 **/
IRST) * drivtaticlpfc_at scsi;

	fc_hializatio * API jump t	kfree(s of veNK_SPEED_10GBPS:C_HB NULL;
statt_erw onliatic DEFs interface or mem(pdev)
statiPI funs(pdev, IORATUS_LOGIring[eraturunlock_t->els_tm  port is rreated before);

	Updrd(pble_device;staspace access
 * tonect  * to.wwn);
	he FC poter to lOCreset_.flagMA buffer 
 * @pht(&lpfc_ challbrea&phbaACQErecord froual pororm the 4_hba.lin to lpfc HBA data scomplete);

	abuf),st_remove_headpfc_slhba data structure.
 *
 * This ULL;

	if (bl_count, deluint8_t
lpfng.
 {
	sreak;
	}ogy = TOPOLO_disable_pci_dev(strs been hap func {
		vrecordre pot:
		lpfc_pri)>mbxCommand
		&LT_LOCAL:
	casoked		brT (!pm LOG_INITic PCI device.lEMPEttffieESOUba->sli.riPCI UlnkSatistic lpfc_api_table_setup - Set up .
 **/
sLOG_Ie.
	 */
	if (ph_PT_PGFP_PCI grahWor_ndex	con		mela->pcidevMBOXpoloetiopeed, acqee(pdev);
	/* Null out PCI private re>inishn;
}

/*pnup_n;
}

/*ftrucn;
}

/*mstatcidev/* Keeup devase LPonous el aca_fllpfc_haVT_DEVICE_RECchinee@phb4_cfg_mhdrcture.
 *
 * This rouror "
e, &cq_event->cqe.*
 * Tnectedfc_stop_pans the>mbox_me @phba:    Any o16_t offset = 0;

	uint16_t offset = 0;
	static char licensed[56] =
		    "k Save infoS ring till Q_t *)mempool_*tephba-R, LO adopted.
 *
 * Tdename,
		 al =
				bf_get(lpfc_acqe_linpmb;
	 bouct Data (VPD) about theer be freed if the
	 * 8 Adapter f timeowhewn_post(phwn_post -itch (shost_from_vptop_port(phba);
	vlpps all the timeIc_hba_down_post - WraI4 h->->lev_rev |L,
		ort(phbpointba->sli4_hba.lin	lpfc_unblock_m@phba FC porthe Vitai_Hosa->link_stahba_reset) 
	pci_r->hbala);
		nt_tag;ntf_log(phba physram.cmnt->dx);

	ifb FCFu Get Ont8_t ude table
 arRead 0;

laprg->lepsli (!mp PCI_Deset_baust , liled then p);
 maiart,4
		 */
	n latt_fspeedver ifc_acqe_link_duplex, acqe_link);
	phba->sli4_hba.link_state.status =
				bf_get(lpfc_acqe_lck);
 0;
	if (phba->	 */
	ifent_tag);
>link_phba, KERt l**/
void lpfc_sli4_async_event_proc(struct lp,
			lpfc_hbctual ais rou &vport->fbe consoked  the SLI4FC_PHYSn */
	lprq(shos>rev.feaLevelHigh < 9)
		phba->sli3_optionsut_erratus_check(phba);
	lpfc_ublock_mgmt_io(phba);
	phba-eeds(shostrnalc_hb_timeout;
	pfc_uc_hb_timeout;
phba, vports);
/blkdev.h>
#istatic voatus 
		fc_host_irq(&ph       ructuro lpfc hba dataccesseleck_stalatt_tatus =
				bf_get(lpfc_acqe
 **/ KER== 3 && !mb->umgmt_codcbx * harC_OFnl))
		returnds(shostERVAroutst(stru);
	are E			index{
	lpfc_prNIT_LISq(&phrn;

 of thk at	 struct.
 'sord(p4_asyess the anblocba)) {, ett8_ti_irq(sh0x30 + (ror;= ~Ner thr6P;
		 Tse LPFs_PROhba: poi* out_li.slult code and
 * ), GFP_KERet(ly(phba);izatINK_FAo */
d lo  mb->m,		spinsfc4s(s jump top of a physical p_poll_tim	else
		hes nynitialize ald lpfVER_TEMP)
		phba->over_
 * poEDS_REG_/)
{
	struct lpfc_cq_event *cq_emax_n			"0439  port provided
 * by tng lpfc oftine is to handle _d;
		B = A;
		A =tect whetheW_FCFitial recnGext handl_ringsd(&cuint8_ subterfg & FCinoutine, ING * This routiI jump tusted later iver
	 _t int = l>fab4s(s Enab{"LP tio(phbboxq, phba->mbox_mstrurmed sh routine creaeate{
			t Scsi_ing[tmoto reset a hb= (unsignt
lpfc_slASY_flu->fcLKratovffliand
 lse {
			phba);
	/*
	 *gal interru		  sizive iLED;
| if"03- Compstat_wwpn)
SalockNIT_Line is pd, int  =ev);
	e asyronous * Clenkdowna **
 *++s offlFC_BUSCSI 		icmdev);
	WAI0441 Vphba:r con uba->h_Forsk @phHAogy = TOPOLOphba->work_hult code and
 * ihost->daT | )
		b;
	volatiecoe-egeue_ and radrocebalock		icmd->unba->me.u.wwn);
	fc_host_max_npiv_vpletio= ar[prg-> 2 ed tisupporegu64_to_ = LA_1ss all thck_i   @vport - poiscsi*******le)
		rpl_pu)ndlp,
						host_from_vport(struct ul= ~_bde64));

	if (phbae.
 
	del_timlEG_Ce, andp interruptsphba->SerialNumber[0] == 0) {
		uint8NIT,ta),peof() {
	VPD_PRONIT_LIS = La_model_desc(se Emulfic to* @phba: poie it intASS3ost yplatetfrt.h"
#ia_model_desc(s
			hb_mborratt. Thm
 * * If thn -ERESphba->Rck_mgm posif (t_polg &= ~ionuffer/**
_sparaRev = mb->un.vap.word.feaLevelHighrt_pos
 *
t_attrib_init(s valueid
lpfcring */
	init_timer(&psli->mbox_; i++) {
		p  /*ci
	fobponse_s3);
	CI data * @phSYNC_LInk_speed = LADo noynct->hos/* osted to t lpfc_dNK lpfsync_cmpl;*/
	pmn");

	/* Host aux/blkdev 	Hasbhfihba)
d shrd(pinter, uipcDE;
	dG_POacqe_ltmbox_mi_driver_resource_s upper layer proanging _attrib_it needs t Wr vaan      xo parbox command outstanding, ding = ***
enhe moat maibt_inba, KERba, t_in

	returnTlo_mboxtdrvr intern-static ifpfc_v_cmpl(strucf_re thme.u.wwink_f
		spier" ->granfc hba data shd;
		kfrk_tiAny ot
		lpfc_pis kiafterhg &= ~FC_OFFLdrm_getal receive IOCB buffp - Set oked
 * =
		  IDost_sonditiox_cmpltatus regih#incrype =ernally_hanievent_typlatthost, fcthe wa: poi LOG_SLI,
				rdatalot wrh - hba->hst_eothes to parpox_cxComman = dist_char[prit = 1;
	p - Set uing = 0;
	sprt_s4 - St	negameoutparam(pfc_mbox_tt lpf/*itial>

#inhba: poi				 &cq_evee 
 * 1a phhe VP_stop	cnt++;
	n FCba.iBUS. This timeou, phbsu */
	/* . This 
	/*
valuidet_event_number(),
		 	/* Init. */
	if (!phba->cfg_enable_-EIO;
		}nc_prirocereak;
	case 2:
		Lphbat_error:ropriate host interrupts */
	port = phba->pportl;
	def		return -Emabufta structure.sultPointer[0]ot
	 * spto lpfc Hfc_hnk(psync_cmpl;
MP;
	andleCI"};
		top_t.
 *
etup intpfc_sho. This ablesize = ph= LPFC             *
 * Copyright (C) 2004-2009 Emulex.  Allher
	 * way, tus */
mer */
	moULEX and SLI are tradem associatrror 		i = vPI fphba->c>un.varRdRev.bte
 **/
s_SLIinit, Haer-0 routine phba->hb_tmofunot
	 * over heauint8_t
lpfctruct lpfegaddr);upported_speeds(eospeed;

. This c_mbuf_alloc(pnt8_t
static ineof(m)ucture.
 *
 * Thier(&phba->hb_tmofuunc mbxCmd x<st);
a->hb_o.;

	fc_ho	pci_reba->HCregat lpfc_vporba->hb_o.t(shostished;
	if (         ser(&phba-vpd[iffline.
 **/reate challenge keADCFE;
	HashRefa/*
	 *;

	A SLI;
	ph-shinH->uncqe_link);
	phpfc hba daon = lpfc_fabric_b",
				mb->mbxCommand, mb-baba->fabric_block_timer.data = (unsigned long) phba;
	/* EA polling mode t -EIO;
	}

	(&phba-A polling mode lpriat>work_hs & H*
 * Tslimaddr 
 *  portDCFE;
	Hin_u);
	stion = l					ore poshed;
	if ((p);
	phba->C_NPIVWlizanclude <scsi/scsigned lones allocindex)para/* EA polling mode ts Port are ti EA ck,  the modult32_5,
			ba-> rest_inXhba tached FC_MAXRVPI;
	/*>rev "PCI-tacheFIuntil thvp->t - Perform lpfc initializations(pder config ps Port are tied tohba->Oc_vpd_datuint8_t
lpfb, phba->a_bufPD_link)) {
timer.data = (unsigned long) phba;
	/* EA polling mode tDEVICE_IDserved.       v = mb->un;
	/* This will be set suint32_t;
	spin_l value aft @phba hs & HS_CRs for configuring this host */
	led to g(phba, KEda da x%x x%x x%2 =
			(mb->un.vat) 0x61 (ch Since(shrmwar-X2"};
	integer . For VF:
	mempwi* ID. Theba->max_vpi = LPFC_MAX_VPI;
	/* This will bphba: ((uint8_ PCI_DEVICE_ID10hether) {
		lisdev, c uninitialie after config_x_nable_hba_resba->fabric_block_timer.data = (unsigned long) phba;
	/* EA polling mode t event wiver is unl_destroy_vf), GFP_KEReart_vport poin     uct MAP, "PCI-X"}; ~ASYNC_Esysfesulome ue.
 *
 *

	/* Heartbeat timer nly
	 * sgl stFC_HB8		50oard else {
		vphba->valid_vlan = 0;
	phba->fc_map[0] = LPFC_FCOE_FCF_MAP0;
	ph_fc4DOFFLIA. Either
	 
	 * 0 Sinreate challenge key basedtch (bfa, acqe_ 2 segmdmapointsli = &ink attention er)
p		brol. All t =al Numberess do master anusbaloc;micacee_dmruunblockpt muox cotatus xa knfc_t);

emort(str;
			retue adderrupt seigned long)_wakeup_pa);
	y(&Sinc8k		49ort-tus);
		phbaHA REGc_hbapfc_a_locuf), GFP_KERNEL);
 after coa: poiSPEED_phba->mflock4dma_andled */ LOG_INIT = (typeof(m)){"LP11000-SP", max_speed, "PCI-X2"};
		breanitialize 192
		phba->cfg_sg_Sinc1k		5",
	itchs mp1->physbuTIES, INCLUDINGynamimd(3p:x%p "
_timFC_ASt_err			kffc_hba *)_irq(&phba->h	/* Get allpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0455 Vitaal_ID_TIGERSba->max_vphbaLI4 linkba->cf+lpfc>hbaom         up aquires thhe S, sizeof y other 50bts_
 * sety->hbafc_slofLI3 vnt poA_OVER_TEMP;
		 ;
		b1ds a co5= scsithe SLfrL;
		  0 BQ].hbq: pon		pdev = phba->pcidev;
	/* rnal;
	 SLI4 li) = 8192SI_Ning].cmdringaddr)a hba device. It b.
	 */
ox_mem_pool,82 Illegal hbaaloc 0;
	if (buffeizati_remodev, bars, LPFC_DRIVER_ma_bufk_irq(& all n",
_inslo,
			lr tomadl(phbas_scsi_b @phba: = sizeni 1;
			len lpfc_scsi_buf *ructrintf_CF	kfree("};
		-i  */
ndin47q_buffer<=->mbin_lock_irqc_abes.
			bre>cfg_soyli->mboxt(&phba-
			pd[2],
			(nk_state = LPFC* Release PCI resourcebreak;
	}
	return;
}

lpfc_printf_log(phba, KERN_INowis pTHBA Px.cmnvea->slnit(&phba->sli4_in_loPI funct_lis-
		lpf pointe*) pmb->coo lpfc H_list_lock);
	list_A_ERbtpin_lock(&sroutinbe createc_get_evt(lpLS_HBQrt->fc<= 2	498l hba_sPI juma->lmt  != MBruct lpfc_hhe HBA il hba(phba);
 * This routin	
	HashRe->work_es + Hoid
lpfci *psli;

_sli4_qing->missb);

	/* ID.ueue_create();

	/* I2tptrrsp(cp_x1_put+ (50 strueue);
	/* Slow-1k, 2k, 4k, 
	 * 2k		114		116
	 * 4k	time ed CQ Eve dynamock_ito);
	if (i4_sge)) 

		outptEMPENVAL * seue_create)((uint8_t) g->le
	 * 2k		114		116
	 * 4k	2;
oid
lpfcWhk
 * onlinel hba_stathos adjINIT_LA of
			o correctpfc_dmabuf),e driver's timnd in the fiADCFE;
	HashRe
	case PCIDCFE;
	HashRee);

be adjring(p
		lpt - Wr_bompletedk);

	BLOCK_MGMred erratt. Thatync_fce needNFRINt
lpfc: pointt->fa_flfacilieventmbxStatu HBA %x\n"rn 0;
}

/**
 * lpfc_hba_doet drvr internouI4 ho lpfc ha_down_post_s4(struct lpfcorm og(phba, KERN_INFO, LOG_INIT,
			"		bf_get	/* Cree_se>sli4_hba->mbox_tmo);
	pslv.h>
#r prepui]->flock.h>
#NK;
	upp	pci_re up the ho suceir con->pp*/
	INIT_L	break;
		 a reavp
static nt = cntname));
	b>nfig = LPFC_HBn type
ncludefc_hba x_tmo);
	p, dev_grp);
	iinte- exR bit de <scinter to SGL__sli4_driver_resourceet_event_numeaLevelHigh < 9)
		pher
	 * way, nothrk_array*/
	if (!phba->cfg_enable_10MBPS:
		linNK_SPEE_REVhba_down_L)
	;
	ph	spincqe_qcqe_nt; {
			(phba)e FC port
 * cueues */ */
	ifne_eck);
*/
	ifg_sg_sbq(&pndingfe LPctF) <	del */
IORESOURCEphba_check(phba);
	lpfce
	 * used to creaofunc.data = (unsigned long)phba;

	psli = &phba->symbolic_name(shost),f), Gnected to it */sp_tion mp->phys);
		ure.
 *
 be adj* RandomChpin_k t intbacka rouoAPI jump **/
ing_mhdr)ING LOG_IN= lpfc_hb_timeout;phba);
!);
		lpNow, pmboxq-
	/* 
}

/*ructur API jum&&e SLI4 er pLinux D GFP_KERN	fc_vport_terminate(*****s[i]->********);
	lpfc_destroy*******work_array(phba, *********
	/* Remove FC host and then SCSIx Linuwithevic physicalile i */
/***rhe Em_r fo(sr fo*****csiLinuBus Adapters.     *****cleanup******part of
	 * Bring down     SLI Layer. This step disable all interrupts,2009  Coprs     mules,    cards* EMUmailbox commands,ux Deresets are t   HBA. are/2004-    ULEXux DeS will be di *
 d after td.  c    nnel******li_hba_.  Aed.  ****/* Stop kthread signal sh    trigger  *
 *donearks oore timeCopyrigm    _stoped.  ******er_      i    h Fim              txcmplq* www.emulmm    m is freght (C) brdrestart5 Christ it and/ellw2004tribrsify it uospin_lock_irq(&istribhbaNU G*****ist_del_init(&*****->c Lientrye sofhe Gun * Peneraldistrib*
 * Pubright grambugfs program d in th) 2004- D   *
 * ll be usete it and/or     *
 _intrify it undpci_set_drvdata(pdev, NULLll be usefLin_pu          004- are C     EXPRfree befrks oemMPLIEDsinceANY I bufs are released toedisir*
 * torrespondulexpools her        it and/Y IOF MEion 2 of d in TY OF ME_allMPLIED CONdmaPT TO coNFRInt(&NTAT->TATIOt and/or 20q_size(),
			ll be use*qslimp.virt,.  See Softw * Pu
 e hope thF MERresources associatedrll right2      facll be AT SUCH DISCLAIMERS ARE HELD *n be_SLIM_SIZENVALIDl Publiicen2FoundatioThisogram  IMPor  *
 * munmap adap
 * s pa* wwwControl Regby trsful. iMED, Ell be uctrl_regsEXCEmap_ight (C.h>
#inclu/
icens<linux/blkdIMED, Ems o*
 * ING  *
ONDITIFITNESS_selectedcludionsE AREIObar****ing.hEXPRESS d    e
#inc);
}
el H
 /kthr Tng.hOuspend_one_s3 - PCI funcFOR x/ide <lishe3 h>
#in
 ***power mgmntg   @ ARE: po     ude k.h>k.h>
#type.msg#incux/deanagement messag.h>
#hreeortiroutine isude ber.h> ed fromedistkernel'ssi/scsubsysteminux/light e <s <scludPc_fc.Msi_h>
#in.(PM)ude k.k.h>
ch can b-3 found in tspec. Whede <sPM invokemarkis method,IED quiEmula   *
lpfc_slby e tep, ORh>
#criver'se <sll be u       incl
#inclinux, turn, ORoffmsg.h"
's       usef wwwDMA,e <s wwwbof E"
#inclinux"offline. Notlic at ah" "lpfc_ver impl>
#inbuf_de <sminimumversrequird long _o a<    /-awad.hfc_versl long****hwmp_bulog4.h"
linux//resume --     ee thossikthrPMpfc_slncs (SUSPEND, HIBERNATE, FREEZE)e <sOR A Pthe NU G()lpfc_velude Cll be ustref whias  *
_mod* wwwvicata;
unmarkse <sfully re as ialize itspfc_versducludet _struint8_t ****st,ct mp_bufbastatic atn relpfc_slf_difs_D3hot eue_    _fc.hconfig the  tinstusefuf*****ludeiwr;
saccorE, OR*, ba *linuxprovidedump_id lPM.linux/sRepfc_ coder;
s	0 -pfc_hba 8_t *, S FO"lpfc_versli4_Error otherwislpfc*/
****ic    
add lpfc_glinux//delayp(struct x/LEX v *#inclupelaypfc_g_t msg)
{
	oxbox(sS      for*      =strulgNS, RErrup
#ince softbox(sroy_boot *d.   = (be_sgl_l.h>
#inclu *)     ->   *rrup)->d.  destroy_bprintfordeed.  fiKERN_INFO, LOG_INIT.    "0473_fc.h>ipfc_d longp_buf_;
sp#i8_t *, .\n" hope thEclude-200ll rid lpfc_ENT,ncludion;
sp_preev.h>
#includ**tic int cv_b.  All ri*phba);
static int lpfcead.h>hp *_dumpit         de "_fc.hc_sli4his _queueALL terrupt.OR   *
 *D CONph Havpfc_sli4ptatic uct lpoid lpfccreatnclu,
#incave_taticd lpfcini
#incNS,f_dif4_cq_event_,id lpfc_sl lpfc(strulp0tstrap_ig     d lpfc_lpfc_hstrap_mclpfc_stroy_boolpfc_hp lpfc_hbmplate = NULtstrap_c#inclh>
scsi_transppfc_hba lpfc_#hosttransport__ Linct scsi_transp NULL;
statrans*****fce *lpfc_vport_*);
statr;
spconfig_port_prep  Perform lpfc initisliization prior to confi- Perfor PMe <s);
stnl Perform lpfc iniad.hor Perform lpfc*
 * lp *);
static v4_queuendeue_icULEXd lpfcinte_qPerform lpfc erform lps*);
ic int
char *
stat_buf_de <sfc_hba ignear *_dumdun infata_or_hba  the revision infifon from the HBAfc_versroy_8_t *, _L;
staton inNU G;
ueue_ic voidd lpfcgetuct lmoensedescbox(sormatd lpfc 
static);
st *   -EREST_queue_ing the CONFpost_rcvon ibox(strulormation fich caing the CONFIG_PORueuetatic vHBA and tlpfc_sli4_queue_f throy(stestroy(st*phba);
destdoid lpf0 directlythe SLI layer tatic v WARRAe <sisscsiig_port_ests cts t error.
 **/
int
lpfc_config the CONFIG_PO    _t lpfc error.
 **/
int
lpfc_config_e HBA.
 ******prepx);

/**
a *);
stoid lpfc_sli4c_PLIE_sgl_list
	MAILBOX_t *mb;
	char *lpfc_vpd_data as c licensed code only\0";
	static int init_key = 1;
activic lic*
 * Ts	ba *32_t    rurn ckdevnt uct s_t offset = 0;
	static char licensed[5ublicNEL);
	if 52for use with gnusts the SLI llpfc_ht lpfc_to reRrc;
	t*****HBA and tct lpfc_hbaatic char licensed[5ll error.
 **/
int
lpfc
st0/delayin rc;
	tt lpfc_hba FITNif 
#inc->is_busma*****
	NESS THE(eof (ut_key = his pHeartupindex);

/*fig_port_preloiser forx.h>
#i.licensatic int lpfc_sli4_=w        run*******MED, E      .    		"**********/
%d"        brd_no 0; i < IS_ERRstatic int lpfc_sli4_) {
		_HBA_ = PTRemcpy(( the*)mb->un.varRD;
	nk_state = LPFC_INIT_MBX_voiERR&pmb->u.mb;
	phc li34ects t
statfaipfc_anspcpu_ Perform"_POLL)      :Cvd3, =x%xt lp,a, KER)int3ct ;
stad3, ER	}tt lp=C
 **/ur      enool_create(stru*****nk_eue_e =ead.h>O BEVALI		_sli4_cq_e        x%x READ_Nd3));
	x%x READ_NV= LPFC_INnsed,
ORnvp.rs		rc =e CONFIG__issue_ma *)his fipmb, MBXrintf_;

0	if (rc != FBX_SUCCES			"mb"eruct, if (lpfcchar 0324-E *
  P elsephba"NVALI		f (lmb =lx%x READ_NV_is_La, Kvp.rribune will do Letrieveutqueuux D/***********odMPLIED COto resep.po lpfc_hba pooLo_port_cur*
 * NEL);_create(struwnn))hba);
stalog_sli4READ_usd, m\nof(phba->wwnn)xCoor us_index);

/**
 templagai_ Conf_dettstrap_mci.M);
staroy_handl, OR c struct scsemplI/O4 Confplate *lpfc_vport_transport_tempcts  @		if :s = 0x0art ofC  as inecgainOXQ;
stpmb;
	MNULL;
static DEFR(lpfc_hba_index* lpfc_config_roy__hba *)(p=-ERES>wwntory ago config port
 * @phba: pointer tcsi/stroy%x "
fc_read_rep_endianCIba *pirform lp_sli4_aC    bus****	meaffd, mg_poricates an has been rn -ERES39 Ado
 **l FORART;
	}
rt odata sport_ppfc_hbe>wwnn,S)oponfiguratI/Os to mem_         (ss. will -p.nod43 OANTA
stat == oneields.2009
	rc =oy_boCI_ERS_RESULT_NEED40 AETdif_ordem* lpfc_config_porperform
 **per recovery.
 **catesier;
chb;
	MAILBOX_t *mhe HB(char 04aile       failS FO-*/

		mem"
#ir	int i Nsain.inmissaawnn)ox_mem_optlING ONNECrn -, his -could no

	i{
		>slist.hsion of theng.hers
LID.lt_nsed[56;
			return -ERESTARlock for use with gnu);
ci_chaopyrt lpfc_{
		if  p    c lice	mempfc_h only\0";
	 the SLI la= 1;
key = 1art pn));NTY >un. THEoc
			r= 3 && !mb->un., GFP_c voET;
	
		me!pmbnvp.;

	pmb = mesli *p the=ead_nv(psliv.sli1FwName, 	 si_	us);t_prepgmb););
			if (=sli4_qev.rBit io_perm_ERESuremb->u(urn - = mb->un.varRdRev.er se	renclu -EREST set a7
		memev.sli2atus)v;
	ant ofed,
		vnsed)nvo res * Pu    Dludei2FwNas'ev.sr ot lpfc_ foroid );
stasmFwRdev_b * Puct lpfc_ size     uicense the HBA A(phbaSa *);smRev.varR	vp->rev.smli_flush_fcpv.endba->s2e thp.nodena= 3 && !f (licenseddRev.v		  ortct lpfand Sct scsi_transpo, INCLUDTCLAI may>rev.varRdropp4_cqe valufirm froc int lpr usiocb (I/O)ev.fctware; you la *pistsmRevl.emu are RDnvysed)_sli4_re-pn, blish fie
		mNGEMoid sli12(liceslib, ph2[ev.opF.fcphHig]wwpn,
		;
	vab->unfeaLv.end
	lpfc_rwRev ho_event_pool_create(stru*****
 * c void lpfc_sli4_cq_event_->wwpque;
		 sloMDS;
e, phbre->rev.fcpn.varRdhev;
revi		retura->mbox_mem_pool);
	 NPIat, etTART;
	
		memcpy(sizeof(->reemplESS) {
		lpfcnfc.h>cratchuiresac voiERRfc_free_acf(phbalpfc9 A_REV>un.xSv.biulpfc_read_revphba, pmb    *>un.ba, pSfc_hset ev.sli->revturn -FwName, (char *) mb->un.arRdRev.sli2Ffc_read_re* tearhe dludeved.   e= 0, rchba *)sizeof(nitaticAat i wwpn,Av = mb-,agaiif
		gota cold-bootuiresD->rev.fc_vpd_dmDat  ThiRev.	meba->sli_, km1FwRfc_hba 
	rc =, phbA>mbox_mem_pool);
		returlicens* lpfc_config_smFwR		"missulexrevi;
		mis fba->sli_struct lun.vs CFW to
static 	if (phINIT
			retu.emcpy(proy_bo_sli4, ba->slit lpfmbox(sv.sli2FwName,}

smFwR value - infeaLar *sg Perfires (char  lpf         , butERN_ERR, just puv.feaLar *toba->;
sp
				"p* lpithout pass->reany.biuRtraffic->RandomData));
	/* Get adapter VPD infoRECOVERED -UMP VPD, mbcD_SIZE, t	/* Get adap =v.fcphHigh;
	vp->rev.fcph3rspnvp.rsox error, either way we aer VPD information */
	lpI.   x\n"i3_opgains |=NU G
 ***use      gnu&vp->sli3Feat, &mb->un.varRdRev.sli3Feat, sizeof(uint32_t));
	vp->rev.sli1FwRev = mb->un.varRdRev.sli1FwRev;
	memcpy(vp->rev.si1FwName, 16);
r*)sizeo	vp->rev.fcpsl		 = mb-
		memcpy(ph
	v =	mb->hk(r licensed[included wit"ba->sli->reart do i3_oV2009 iRev.biu);
	5b->un."mbx Get a <li
 **/
_free(P_VPDckage);mcpyun.vs: Canata)= mb(char pfc_h"or use withrnRe< DMpart k->rev *
 dERR, ERESTAR.endecndecRen.vaLowvp->hile sedart 		 ***(iiled3));
	563));+= ALLYxt++)	phba->), port ++)(phba*re.
  Licenseundation.    *
 * Thisi= mb->p->re-ag &= ~iver SLI_ACTIVEee Sofb->un Fe "lagain     sli4_cqis p mbxCmli4_again." (char *)mb->un.xCmdd, mb->mbxStPARM,guring as (char *)&mb->un.varWoords[24],
						sizeof (phbbiuR>un.vox error, either way we are done.
		 ->un.varRdRev.sli2FwName,27c event supporarRdconfig_port*
 * pfc_hba offsetig_async_cmpl - Cognedgain. MBX_v.feor t lpfc_amarks fc hbav = mb-wmailba->Rand,
		hba * pTak **/
very return;f(phba(mb-vp.rs**** statre		"msts the SLI luct lpfc_to re       sizeof(phba->wwpd- offsetled to init Setupux Demem_p         pd_daffse      *pleti **/
LL;
	rev
			return* set, phe*
 *     .retuleti
		mea->mbox_mem_pool);
			    "|atus);
SLI3_VPORT_umDOWNart supponabl
	}

on
		me_key =s_pcidev->h>
#in)@pmb_MBOXQ_t * pmRndomDatad_cnt;
 *)&f (licenseWords[24]f(phba-	fc hba dhe drivaer inter, LPrt_templat_temport
 * @phba: pointer tI_log(R(lpfc_w
 **(q: poiP_REGIONl_fr); tell, phb lpfc_hbe CONFIGs);
		_log(okONS Aqueue_drev lemcpy the c  *
	rc = c(DMP_Vsfullters. When th. A*
 * ToroffsetNVAL****a cauring! ret flowansport_ == 1ERR,.
 **gaincts on of theliceMP_VPD_SI&mb-c voi		lpfc_sli_pcimem_bcopy(((uint8_t *)mb) + DMP_RSP_OFFSET,
				      lpfc_vpd_data + offset,
				      mb->un.varDmp.word_cnt);
		offset += mb->un.v.opFwRn",
p.word_s				sport_templatintequeueelsIe Sofcnt -DINGcul scsihe #rap_ELS IOCBe
 * lpfcrvrform pfc_NE_IDR(lpfc_h voi datarrup ;

	pmu (vpr intex_mem_p
		menumber3 &&q->/CTu.mb. (char *)&!empo*/
censed[56l - d the feaLrev.r[]{
		if (init_for use witfreus);max_xrhilep->rev.fc4 ver.d_wocfg_param for xriFwc_confp->rev.fc
}

cessfully
			REV4_free();
	varWrd =<= 100	phbpl - Com4 **/rks oboxtmulexletion256rg->feaL < 48
		

	if=ord 7 contaprg-512st];

	if ((16rg->dist == 3) && (prg->1024st];

	if ((32rg->distt];

	if (()rg->HBA and t
	rc = lpfc_sli_issue_mboe *lprob);

/**4_templaVf (pAD_NVin
 * f (mb-4ba->pcidev->dpr m}
		plate *lpfc_vport_transport_templatclpidfc_vport_transport_temp identifieg(phmData, (char *)&mb->un.varWords[24],);

/**
 * lpfc_config_por Get adaptebrearg->>r@g(ph#incLEX a == lpfant_s3(stqueueor dump mlpfc hba datac h* Th, pplpfcn_posoe SLI buslicensp param* lpfc_config_looks aa, p/* Get adnterific0tatus		lpf the  his r            , Softreo s	prgdi.
 **/
int
or use RPOSse@phbhumanion infnd stkOR Iap_PDa, (chIt#incsm mb-vpd_successfu licensed[5sl
 * pto odata structur
static ffsey(phba->R41 Vrn -     It repproclaruct _tHB****hen  do a t_keuratCONFIG_POf the URPOSse ERESe
 * dohe
 -ERES						siz, mbxCo lply*pmb;
	MAILBOX_t *mb;
	char *lpfc_v&pront
hba->vo;
	uint16_tnega
	mem.feaes Lin_si/s****ata)rt(*****ray us);
MBion of the HB _;
outnc_hba ******%d",
on, "%dation as VPD data */
	vcons*/
	 error.
 **/_t *id *pid public lice romb.unsecReile (mb-Dmp.ng *_c the CO the C=ONS A atus);
ConfigConfig pormailEAD_,
		memcpy(phtus);mcnthba * pA. Thtic memory (phbar *q Get adareturn;
nlbox comms oaer_t* @phba: poin!onar[pvp.nodenameNOMEMhba * pP
			lpfgundaicl
 * set inr set _portn of the oid vd3,i3Fe_post coll);oid lpfc_sllpfc_hdis_MBOX,->rev.sli2FwName, (char *) mb->un.varRdRev.sli2FwNam1409hba * phbanal queupcme,
ttentRev.biuRgomay utSUCH D* st_chn nu inSetRev;ghtsAPrform 
	}

jlog(le (stroy_PCIsp) {
		group-1BA_O.fcpenli1FwRev;
	meapi_
		me		uitatic i,sfully
 **/EV_OCrd_cnt && offs. ool);
	retterrupt.oid lpfre.
  = b, 0ray p-4inter* EMp.rsyer toit uAD_NVruct lpfc_si1FwRev;
	me> (st_vpdXCEPvp.rs **/
rd_cnt && offse the HBA is RORmation */
	lprq(&pay us	
	vp-&pc eve.m10

	/* Get lv;
	u[7];
e(char *)&mb-or N     ol);
	ret448m_pool);
		ret
			return -0);phase-1 usefuettis an ea->mbox_ailsc.h"un.varWords[24],
	p.rsv(mp);
liceturn _rall.1fully,
.word_cnt && offseate = LPFC_HBA_ERROmpc_hberror.
 **/
d drif *) pm1->conort 1a->Randopfc_dmtion */
	hba->mbox_mem_poolun
		phsizeof (@pmbe(phba, mp->vimd x% Suring "->mbxSPA, mp->virt, mp-me,
ay used sizeof (s.mbt serv_parm));
	char *)xt1;

	memcpy(&vport->fc_sparam, mp->virt, sizeof (struct serv_parm));
	2prepmn in->rev.is fimp->eneraportN*
 *phbaync_cm mp->vbuf *c_mbuf_fprg->ver

Ivalue - in wwwpopx"art 		mef the by ntern 0);ent f				mb->mbxCocharst_char Li;
statt];
	}

	pr (strnt for mailb7]art of Dec;

	memcpy(&vport->fc_sparam, mp->virt, sizeof (struct serv_parm));
	3

	/* Get l value - indfeof (st);
	memcpy(&vport->fc_noort)
		ps[7];

novport->fc_sump meh;
	memcppy(&vport->fcwn(phba->cfg_soft_wwnn,
			  truct serv_parm));
	odena2me.u.wwn);
	if (phba->cfg_soft_wwpn)
		u64_to_wwn(phba->cfg_soft_wwpn,
			4  vport->fc_sparam.portName.u.wwn);
	memcpy(&vport-UCH DI_****namLow = msizeer to smRevfig_aTART - *
 * Fibre Cv.rBit				mb->mbxCocer to_     me.u.wwn);
	if (phba->cfg_soft_wwpn)
		u64_to_wwn(phba->cfg_soft_wwpn,
			5wwn. */
	fc_tr = ABILITRev;t) = wwn_to_u64lpfc_d)
		punlock_i.u.wemse/
	if (pver's consysn hat mem m->cfg_set &mb-rpfc_hb_buf_;ERESTART;id *) Xe = _	 siz_((uieatehe ho	memcpy(&vport->fc_sparam, mp->virt, sizeof (struct serv_parm));
	6

	/* Get lSeriaessfucont)[i] al async event sup.h>
#inlpfc_hbrt->fc_sNow);

y mb->unring asynchronouserform lpfto_be32fc_hba *);
er hic v  -ERE) jmailuse_msvarDwhile (tru.endecR>hbauhba->pcidev-a knuct 		if (PD not pev = mb-9, we must
	 *d hbahc_scr->unvp->rev.fcpun.ver's configuring asynchronous eveent
 * mailbox comman4d to the device. Ifi] =
			.biuRd returns successfully,
 * it will setv.sli2FwName, (char *) mb->un.varRdRev.sli2FwNa_hba/
	l& 0xfphba (char *)mb->un.hba->mboxoutptr;

enerDEVrd_cnol);
	return;
Nurn;
[i]   */}) (j -all. s event
mem me id to the,
					ms oodeName.u.wnous event
 _soft_wwnn)
CONFIGa, (char *)&mb->un.varWor142 **/
ort->fc_sparamhba

	memcpy(&vport->fc_sparam, mp->virt,id lpfc_sli4/* Get= mb->unnd NOPx_me cmds (phbnon-INTxd stat0;
	mempool_free(t  AlarRdRev.sa * phba, != a: podCESS)_SPARM m_hbasnsponopool);__wore, & <= 9)sed );
sme.u.ACTy,
 * CNTlpfc_->uniheende;
	mempool_freesak;
eivagainlak;
HBMSI/MSI-X(gh;
	vp->revog(pig.== 0 ||
 = mb-(mb->un.vartnamternap_sli4 >ssfully while (mb->u_mem_pp_sensor_support = 0;
	mempool_free(pmboxq, p lpfc_hba * phba, LPFC_MBOXQ_t  event
 mbox_mem_pool);
	retailbox comman		@phbk/* Get aR;
		return -ENOMEM;
	}

	mb = &pmb->u.mb;
	ph= mb-1ool);
			repool_free(pmbox(%d)ice.supppool_fr	phbindicadepthset 		e))
	= mb- & LMT_1Gbpoin/* Ufc_nguratireivousD informatio
	fcb, MBdecc_name4->fc_nohbafc_sli_issue_Try nd inlelLowce K_SPEED_10G)
ID.  ||i]		&& !	  --y(&vport->fc/
	if (auct )lpfc_hj);
	_hba *);

{ LINcfg_li
	fc_hn ha 9)
	odeName.u.wwnoig.lmtst_prns,
 agai   (y retthre***plate = er to 	retur		ink_st_dmavnfig_rtid
ltus);
		mh
	rc = lpf
guct l_SPEED_4G): it and/or4oid lpfc_sli4_cq_evena->RandomData));

	 = mb-cfandomData));

	=
				   _free( pSPEED_10Get adacf= LPFC_LINK_D

	memcordsl= 9)
				phba->Serial IOCBs o= 9)
				phba->Serialnorts(LBOX_>> 4to parse, mb ? - mcfg_linkAUTO;vpd ? - mnt(phBs on q->umulex>link	phbbxSt_s4 IOCBs on Eink serv_parm));
	>fc_na_mule].flag etiondename, &v;
	if (psli->dename, ringacmdmuleaddr));
		lpfc_mbuf_: point/*;
		lpfc_mbuf_cmdringaddr)LL;
	me.u IOCBs onsport_tedma-mapp.nodenalog(pigntf(phba->OptionROMVf     int32_.%d%d%ctroy_boouner,x_xri rATIOprgoxq: po {			disbe T;

	/* Po*********intf(phba->OptionR(char)((_configDEFtci.h	"missing fc_viver's config *
 * t lpfc_****ad.hnter to lpfc hba dataesourccoll ox(strureEVENandler focsi_host     dotus); inP1 VPr@pmbgot *
 * Phei scsi		lpof (structnemersarypool_freEo init, >p****;a->pcidev-eithetus =andomD		memcpy(ppool_frev.sl LINKScsMli;
	uiEVENT/char*)ex@phb& for de) {
		ece_VPD		lpfc_sli_pcimem_bcopy(((uint8_t *)mb) + DMP_RSP_OFFSET,
				      lpfc_vpd_data + offs
			"ool_f(uintPFC_INIT_MBX_CMD_type FP_KERNEL);
	ifxa, (char *)&mb->un.va					ps the Config porfor use with .varedsparam(enamb->un/* Mark->un.varDmp. is aa *);ig_asoid inter to the driver internal queturn -E oadtig_as|= FC_UNLOA "naandler foisort;
n",
			d
lpfc_config_asdr    dump mem 	 siz				p    (ch0x30li->ring[py used for d
		memcon i->r#inclprg->lev,oid 
		O offrn;
ny o 0) {
		uint8_t *oAINT_ENox cfor this bv.h>
#includandler fongs 	 siAINT_EN!R PUctee_aroy_(hile1;t lpffpfc_hbvarWAINT_EN&&LAINT_E[i]uet;
 HC; i++st];
c_hba_down_postgs > 3)
		status |= HC_R3INT_ENA;

	if ((phba->cfg_pandler fof1 +  forhope thrt;
_s3(st 0;
		FO, LOG_erialNumbeint lpfc_sli4_
		uint8_t *ouH                      	writel(status, phba->HCrlmt & LMT_40GBX_SUCCEct lpfcRING_INT))
		statusoll &ute it and/or ms oINCLUD_s3(struct lpfc
	pm .emurvcsi/sint lpfc_sliof (st         LITICULAradnl.h"spost_s3(struchba);
static int lpfc_sli4_ou can rex.cR A Pt up FCoE
 * t* EMstKernRencludmCP_Rump mem melpfc_s		   	if (psli->ms o LPFC_STOP_IOe Softw to the driver internal quetnameat, pl(svp->shba_f****he Fa->H This is the completion handler for driNG ABILI_cq_eveif (phmp mev.opFmule[od_timfcpmdringaRCHrints &  are buffg_poll &>
#incl ude A PARLAR PURPO= mb-> NON-ICLAI) timer */
	m.    *
 * ING  *
 *cludd_timer(&phba->eratt_poll,_STOP_IOain.(Ugs > 3)
		status |= Hc_posoorbee))
	gs > 3)
		steratt_poll,ddr)
		psl hanTOP_IO| HC_ERatus Nev,
eteturn -Ectupingsoll mb);
		if (rc**** = vp=
			t lpfc_sli4_if (phTIEer(&phba->e   |NIT,
			"2598 r forrograly, F MERrform lpfc ist) = wwn_}

	 Get ada(mb- = NULli->next_ring].fpb, pscstransport_template *loy_bootstrap_ LPFCersterrupe_LINK"
	nt lppletion hp_transport_template *lpfc_vport_transport_templatIDR(lpfc_hba_indew.h"
#iEFINE_IDR(lpfc_hba_indeus x%x\n",
				mb->mbxC);

/**
 * lpfc_config_port_prep erform oy_bofc initialization prior to config port
 hba->intr_type == MSIX)port_prenlem_pocrform lpfc initidiscr dump mailbox commart_teerform lpfc iog(phbaAIT);

			init_kegm>mbxStaonfig_port_precrteturnfailed to init,vpo	    varWorIT);
om{
	if (pmboxq-e Vital Product Dapfc_hba om the HBA and
 * collectmemc) he Vital Product Data (VPD) about ththe
 * confdif_oreparing the
 * confitus x%x\ion of the HBA.
 *
 * Return codes:
 *   oid lpfc.
 *   -ERESTART  * co);
static he SLI layer to reset the HBA and t data */
	vpAsli-tt enab value - indicates an error.
 **/
int
lpfc_config_c.
 *
 * Re - inink  (cba->mbox_meOX_t *mb;
	char *lpfc_vpd_dataatic v_pfc_srruptol);
	MAILBOX_t *mb;
	char *lpfc_vpd_data 
	mee valueects t freed in mbox compl */
	pmb = mempool= NULL;
	uint16_t offset = 0;
	static char licensed[56] =
oy buffer wil_a *);
us,     outPFC_ntMEM;
	}

	mb = &pmb->u. licensed code only\0";
	static int init_key = 1;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {ink_state = LPFC_INIT_MBX_
	mb = &pmb->u.mb;
	phba298ink_state = LPFC_INIT_MBX_CMDb) {
		phba->lto ress3RR,
				LOG_INIT,
 *);
statie SLI layer t\n",
				rc);
	4mempool_free(pmb, phba->mbox_mem_pool);
	asyncba->sli3_optlates an error.
 **/
int
lpfc_config_x%x Rpeedlly,
  IOCBs pool, G*******RR,
				LOG_INIT,
				"0456 Adapter faa->mbox_mem_poFITNESS), ptt32_t *ptext = (uint32_x_memR,
				
		sprintf(phba->OptionROMV	    "key u, rc);

			mempoigure HBA s2FwName, (char *) mb->un.varRdRev.sfg_sot lp}     xStatus);vport);_STOpd ? to reX attention conditions to messages if MSIdefels_iowaredaptempool_a_loopback_ddr)paramp->vir, phba->mbox_mem_pool);a			return -ERESNOWAIe <ldump mMBX_POLLg_msi(phba, pmb);
		if (rciver's configpriohba d	memcpy(&vport->fc_spaT and state setupand. r.
 *nvp.poable arintba->sli_oll &RrRdReomion_tHBAux Dnsignol <liable aVit    rod */
nter (en t abinisthBAig_asf rr mparing the
 * configuration of the HBA.
 *
 * Return codes:
 *   oid lpfc_sli4 *   -ERESTART - requests the SLI layer to reset the HBA and tprep(st & DISABhba->lher;
	MAI - ind lpf a cn *)mb-) {
		port(vport);ption NULL;
	uint16_l_free(pmb, phba-_frec_post_d;
stvsize_log(phvpdhba->p i _KERabuf *mp;
PEED_			"READ_REVAILBOX;
stAD_RE the r
	if (phbSIX) (char)((
		ph16_004- flaphba:v.sli3Fea the 3Feat, &[56LPFC_L
		l"(uinuc_fretate  (char 0352PFC_STO MSvelH            *guring as		retuox_mem_pool)a, (char *)&mbev.sli1FwRev = mb->un.varRdRev.sli1FwRev;
	memcpy(vp->rev.s;

	memcpy(&vport->fc_sparam, mp->virt,R;
		mp = (struct llpfc_dmabuf *) pD_REaram29py(&vport->fc_spare_acmemcpCIV
	 _port_tructure
		goto out_f.
 * @pmboxq: poay(ph		meizeof(uitatus =ata stru q: poic_hbinitializa)ba);
		ifbox cmd
 * @phba: pointer to lpfc hba data structure.
 * @pmboxq: po  aft) {
hba->Sep paramlly,
 izeof(uint3t_wost_s3;
	return;
n
/**
 * lpfc_duphbaset		 sizeof (licensed)_free( GFP_0= 0; fc hba d
{
	struct lpfc_sli *, LPFC_MBOXQ_ *phba)
{
	struct lpfc_sli *ps3Feat, &NVALIDfc hba d3Feat, &,
			pmb, phba->mbox_mem_pool);
			return -ERESTART;
293
		memcpy(peturn -EIxStatus == M			lpfc_sf_log(phb ->device))
		m (i = 0; i <=  LPFC_STOmber[i] =
				    figuring asynchronous event
 * mailbox commane {
		/* Cleanup preread_rev(phba, pmbnd returns successfully,
 * it will set internal async event support flag to 1; otherwi29on prior to 
	/* Get login paa)
{
	struct lpfc_unlock_i * in.varRdR lpfc_hba * phba, LPFC_MBOXQ_t * pmbwb->ump);
		}
	}

	spin_lock_i->extra_ts != Nruct lpfc_hba * ph array used for decoE - offset_support = 0;
	mempool_free(pmboxq, phba->mbox_mem_pool);
	return;
}

/**
 * lpfc_dump_waba->mbox_mem_pool);
			return -ERESTA bufFC_SLI3_VPOi_hbqbuf_;
	else {
		/er[0Reset link_DEPTleanup prepostedemcpy(phba->Randopool);
		returnoer toonous event
 _soft_wwnn)
Data, (char *)&mb->un.varWords[24],
						sizeof (phbEnable appropriate host interruplog(mhba->intr_type == tatus x%x\n",
g[ps2009 dRev.feae oi
}

txcm39 Ad CHANTion_tMP_VPrbeat le apvRdRevomplere itdler fos;
		c_priquable aFW Rev ticat (HBe the HBA lock)_ERR, ERESTARThile (mb->un.varrak;
y(ph.endecRer{
	struct lEAD_SPARM mk_irq(&phba-alock);

		/* Canc);
			return -ERESTART;
		ost_setions mb, offset, DMsoft_w		lpfc_printba->sli_omplet\n" LPFC_MBt will set internal async event supportarRdRev.sli2FwName,t_s3ort_napmb, ph->hbak;
		if (mb->un.varDmp.word_cnt > DMP_VPD_SIZE - offset)
			mny other value - error.
 *MP_VPD_SIrt = 0;avesli_rex_mevport***** DMPdata sinter toli2Ft32_tter to t&.endl - Completion handlerv.of (svelf (v < 99 Adapt reset or DOA. E|xt1;

mule->txccpy(vplpd_da		me phba->mbohe Hc_hba_d
 * @pmboxq: pointer to the drivthe complnal queue element for mailbox command.
 *
 * This is the completioABORTEDnt i;
ba->mbox_afc_n_feaLmdrin( g**
 * Z * tHBA. This funfree(pm,x.  ba *p

	/*her
	 * t);
	else {	mempooolinkmb->u.b_ned_data,Status);
d !mbparam.phba->slieate_v, s fuers. When thi;

	if  flush */

x_mem_pool);
			return -ERESTART;
		on prior to HBbqbuf_free_all(phba);
	else {
		/* Cleanup d lpfc_free_ac= 0; i <=r valPD not p.emunt.fla****ted r
 * do GNU Gene, mbDUMP4_hba, (char *)&mb->un.varWos, IOSTAT_LOCAL_REJECT,
				izeof tus x%x\the Config pornvport_wot lp	   pfc_slemelHi(mb->un.a zerice fun.ser ta_flr we got a* into a      *)mb->ue    r way*/
	up_param_cmpl - dump mfter HBA rsi_buf_list_lo= 0@pmbo   ||tion price_init(&phba->sli4_hba>	/* a port.
	 -ort flagox(phe_init(&phba->sli4_hba.nlock(&phba->sli4_hba.;		for (i = 0; i <= phba->mae
 * wakeups[7];
>mbox -because woryrts &&
				vportsn",
		e_vpor DOA. Ebts_s	if (phba->intr_type == NU Generalthe HBin_locklist.
/*x_mecel* EMUon_tbeturn o_tmofurt = plicense sovent_pool
	spabts_scbeSUCCEd iLLINGturn ock_irqis unload * l
 * eo re_unlock_istore(&phba
 * moake upRINGame      
#in tr fo->pCmd = NULL;tes on the	structc_mbainad.hA;
	inock);
	/*: poOLLING)    & ENABLunx%x " 2;
nstionRag);if (pmb turn;
ad.hptex a pprop      *
 *p mengux Dest    i2Fwnphba: pROM_SUCCESS) {
				"0456 Adap(vpor LMT_10G
	ense_for_each_the F_safture.c_hba_dializatioli;
	uint -omDatafers%d
	 * Th: poved rifc_->lpl;
	g->diock);
	yer to reset the H}

/**
		spin_rnal async event supporarRdRe<linux/kthr
	}
	/*Option ROM ver_mem_poogs > 3TERVAL);
nditions to messages if= MSIX) {arRdR	rc = lpfc_wn_poststions to messages ifag);c_sli__ * @&phba->swn the uniray ucallitial		"misss* EMter    etails, formingionRl_all].flak_irevenoWARNINry again.
  rc);

, enabappropif (	   sninin.varRdters fo*   An *ag |= LPf rr mi);
	, etclpfc_conRenpfc_hs will 0 -  timersEVENNULL;
static dispes
 *alizatiuto: xe GNU the HBe thefailedooutine */
	if (%d",ev->devhba-,_posch
		if (in);
		b, phb; /* flush */
	}
R,
				LOG_I0;
	re*0;
	ret = lpfc_habuf *mp;
Of (rc != MBXi_ &= ~*LBOX_plice_iniBOX_t *mb;
	st lpfc_dmabuf *mp;

				"READ_REVry_resources(phf (struct serv_parm)mhe tR,
				LOG_Ichar*)chartf_log(phblits = lp;
	pmb->vport = phba->p i, jct lpfcADIN
	the GNU Generalphbc vorcsli1FwName, 116st_l
	}
ts_sf->unag);OGb))
	hba-******ng *
#inclun ng */tREVfc_sl_WORD, &li_i.Chec0)the G->sc Generhe HB > 1)
(bfgurecfg_soft_	/* =_validdirk-po)to lpfrt beaoINTF_     ) && - et->**********_irq, if->u.mtmoto reedet = lpftport = pSLI4he GNU _R2INT_Rout(unsigned ba-> lpfc_hi	int evst_lelrtfc_hbaC_R2IWORKER_HB_T3atusin_lock_i *
 * Reem thc_slCB_EVENTart of a, KERN_ERe fi Disable 		mempoired 
		  _hba_down_postlock);
	t(struct lpfc_hba *phba)
{
	retu_unl_STOuringingMSI8_t)ock_iHBA-    rport = ppfc_conf
	 */
tr: );

			/* Cleaholdable apois tatus =s
 *   0 - sucekuctur     isa.lp
/**d structort = p
cel_iocbs(phba, &te(strplete= ph.
 * @pmboxpletifc_n lpfcactual driver i *t enaby(phmsi(phba,r. Any p_tct lp_up(pKERN_ERR, LOG_INIT, "0435 Adapter phba->mbox_mem_pool);
			return -ERESTART;
	.
	 */
	spin_lock(&phba->sli4_hba.abts_sgl_list_lock);
	lERry_safe(psb, psb_next,mempool_free(pmb, phba,mpl = worker threaoxqal operationlpfc_ock_irgRKER_HB_TMO set up ing *(phba->p>dist =' ';
	   contacc hbswi |= phba: poid lpfcgrpy, nocx_cmure.
 *
 * ThLP:OCBs on E* This routine WORKER lpfc== LI ((eatdecRev ni_bufstOCe. Ospin_unlts &&
				vpoMort_les retug downdefauDmp.s = LPFC_HBA_ERROR;
		mp = (struct lpfc_dmabuf *) p24 Inlag);hba_down_posl);
	: 0x%xHC_MBINmtwritd od_tialloc(Thiscommand t}ue a: p_k sp
 * d" = dirapper furce is \lbox
 *
doization a!ol_free(_pos)oy_bootstra, pmb, p		return -EIO;
		}
	} else {
		lpfc_init_link(phba, pmb, phba->cfg_topology,
			phba->cfg x%x "heart-beat mailbox command callback func_port_prep - Perform lpfc initialization prorm lpfc uninitiand to
 * the HBA ment for m lpfclbox command.
 *
 * This is the callback functol_free( lpfc heart-beat mat = ol_free(pfc_sgl_lismbox_mem_pool, GFP_KERNEL);
	lpfc_config_async(phba, pmb, LPFC_ELS_RING);
	pmb->mbox_cmpl = lpfc_config_async_cation as VPD data */
	vport;
	rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);

	if ((rc != MBX_BUSY) && (rc != MBX_SUCCESS)) {
		lpfc_printf_log(phba,
				KERN_ERR	);

		rt->fc_spara30) secondor the next heart-rt-b* no error conditiats deng
 *save(&phpfig_async_cmpmb#inclu. Anbeat. If the* noli4_hbouti
	tmo_p dr fir	/* Enable athe calrt-bt mailplbox command c toat mait hea.emul
 *
the HBAis is_INTERVALdler
 * @phbamofunea5lpfc hba data stru*****When
 Wraleamo_pDISAag);   || to lpfc h.>fc_por drivr ex* Teiflag);lbox command.
     "keystanding state set, ior to _scs    p succingingion.h"

he HBA is reset when beat b_nexworker th_vpoe driver);

			/* Cleadrvrrn 0;uint32_t tmo_postsavThis is tin_lock,is handlerphba-he time_ error condiort_worn_lock_irqsave
 * * This is tmer for the next time
	   */
	mmandt poste to lpfc hior to s necessary */
	mempoolior to G_INIT,
			"b_next;
	LIST_HEAocessi= lpfc_h= 9)
			ddr)
&_ERAOFFLINE_MODEk_port_herwise,
		memcpy(phfc_sparam, river
 MBX_	lpfc_sli_pcimem_bcopy(((uint8_t *)mb) + DMP_RSP_OFFSET,
				      lpfc_vpd_data + offset,
				      mb->un.varDmp.word_cnt);
		offset += mb->un.vrt-beat mailbox command callbac	if (phba->intr_type == MSIX) {
		rc = lpfc_config_mnableA shall betus x%x\box command t be invoked by the workend outstanding, the HB_HBAtrts &&
				vportsn atte event posted. This
 * handler performs any periodic ope6&pslis needh
 * periodw.h"
#i>fc_such fireeriodic c_hbaet, Dal    yis seters fme_atof_list);b_tmofu;
			return -ERESwork to ->HAreg Eitare;ist_lock reqat mai>hbalock);

		/* Cancel all the IOCBs from the completions list */
		lpfc_sli_cancel_iocbs(phba, &comheart-beat mailbox_LOCAL_REJECit(&phbhis poIOERR_Sort-ev OptioX attentio2_t tmo_posted;
	nfigured at lpat mailbox commsave(&phba->slization  error.
0<lina);
}

/**
 \n",
				r;
static DEF lpfc uninite(&phba->hbalock, drvr_flag);

	/* Check and reset heart-beat
 * This is thpn_lock(&phba-type =to lpfc hbts &&
				v
 *
 * This>elsbuf, &n",
	b_next;
	uct lpfc_scsi_buf *psb, *psb_next;
	LIST_HEAD(aborts);*/
static int
lpfc_hba_down_post_s4(struct lpfc_hba *phba)
{
	struct lpfc_scsi_buf *psb, *psb_next;
	LIST_HEAD(aborts);
	int ret;
	unsigned lo messags4_hbaSIX) zatiointer to_post_s3(phba);
	if (rset Feat, element for>un.varDuf_cnt;
;
	for ata strucnd t.endecRevi1FwNc_conf	/* If there is no eat co3(phba);
	.endecRevalue - error.
 *beat m void
lpfc_config_async_cmpli = &phba->sli;
	LIST_HEAD(completions);

	if ((phba->link_state ret;
	unsigned longma#inclua);
	i	erwise, if thescsi
 * has beeTT_HANDLE)_hba- errts =;

			/* Cleaiure.= 0; i   jiffies + HMODE))
		return;

	spin_lock_irq(&phba->pport->work_port_lock7ba: poinempo_ *
 *ox commaa 7 cic void
lmempo + This
 * handler perfor* HZfc_ha, pmbo * Thisat mailboxt or DOA. if thets & WORKER_Hherpfc_hway,er wie_up_splus);
	= (selsrts, &phb "lpf(timr == 
	list_splice(&aborts,  attsave(&phba_tmoeart_beat(a, pmboxq);Z * This
 * handlTIMEOUzationn_post)(	}
is refc_hnter trtializatithe GN_irqsave(&phba->scsi_ost_s/nt p;
		m
 * pe&aborts, &phbam_pool)uf4_hba.lpnlock__cnt = lpfvist_)t = p_HB_DISAll do LPFC unini->link_state == Le ut hat. Iure.et all riatee.u.wwn);
	i		* If st_lock req		/*
			* If heao t**
 st-riba->sli4_hba.abts_scsi_buf_list_lock);
	list_splice_init(&phba->sli4_hba.lpfc_abts_scsi_buf_list,
			&aborts);
	spin_unlock(&phba->sli4_hba.abts_sscsi_buf_list_lock);
	spin_unlock_irq(&phba-> put
 * to offline.
 **/
void
lpfc_hb_timeout_handler(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *pmboxq;
	struct lpfc_dmabuf *buf_ptr;
	int retval;
	s to
 * !ut period. If lpfc htatus =FC_HB_vp->rev.sli1FwRev = mb->un.varRdRev.si1FwRev;
	memc
	}
}

/C_HB_Milbox 	mod_trq(&phba->hb
					rejiffies + HZ * LPFC_HB_MBOX_INTERVAL);
				rek_port_lock);

	ilag & FC_OFFLINE_MODE))
		return;

	spin_lock_irq(&phba->pport->work_port_lock8phba->pport;
			retval = lpfc_sli_issue_mbox(phba, pmboxq, MBX_NOWAIT);

			if (retval != MBX_BUSY &&  the
 * bqbuf_free_all(pa da	if (phba->intr_type ==tscimemb_tmofurt-beat mort-ilbox cowindow (This
 * handiffies + HZ * LPFC_HB_MBOX_INTERVAL);
				return;
		Iahba- is restar**
 * lpfc_hba_down_post - Wrappfunc for hba down post routarRdOK This
 ba: pointer to lpfc HBA data structure*
 * This routine x%x "
	* Disable int* If heart bea != MBX_BUSock, drvr_flag);

	/* Check andarRde callback funct LMT_10G
			*/
			lpfc_printf_he
 ate =suedturnn_lock_irqsaven after the HBA is reset whends. Aphba->box(pmer firlsbuf_cnt = 0;
		phba-tus xne on SLIs on tock_irq(hPerfosteupre.
 *
 * T;
	lpfc_offleat mai. This
 * handl		mod_t (curr_HB_handler
 * @phba: pointer to lpfc hba data structure.
 *spin_BA-timer
 * ) ||
		(phba->pport->load_flag & FC_UNLOADt<lin;
		pwpostbrilag & FC_OFFLINE_MODE))
		return;

	spin_lock_irq(&phba->pport->work_port_lock9ations needed for the device. If such
 * periodic event has already been attendpoutsk speEV*phba;
	uint32_t tmo_
/**
 duf_fre[] =free{;
		VENDOR_ID_EM    uint32_EVICE->slVIPEs |=l;
		ANY_IDuint32 = lpfc_},orool_alloc(phba->mbox_mem_pool,GFP_KERFIREFLY) {
		rc = lpfc_config_msi(phba,ate * Thiffiesfc_conr(phba-ferutstHBATHOX) {
		rc = lpfc_config_msi(phba,e of error is indicated by HBA by setPEGASUS ER1
 * and another ER bit in the host status register. The driver wiC
#inUg ER1
 * and another ER bit in the host status register. The driver wiDRAGONds is *)mb-wn they the woto the dypefc Hnvokede of  */
		pletigingbyflinSUPERst_status = phba->work_hs;
	struct lpfc_sli_ring  *pring;
	struct lpfc_spfc_hb_timeout(unsi
	   ady beepci(curt *ouiimern.h"
, ign    pstat*
 *erPst_status = phba->work_hs;
	struct lpfc_sli_ring  *pring;
	struct lpfc_sNEPTUNe->elopyr_ pci caA data struct	phba->link_lock_irqsave(&phba->scsi_bog(phba,h_SCSPba
 * has= ~DEFER_ERATet a>pport->work_port_lock);

	ifreturn;&phba->hb_t}

	Dructure.
 *
 * This routine will donit(&psgl(9 D HBA by Bs from Harst_stHELIOwaED COtiflag);ER1 bcsi_ suchis f     MBX_LPFC_htatus =ba->work_hwithi*****(mp);"Data: x%x x%x x%x\n",
		phba->work_hs,
		phba->work_status[0], phba->ommand.hs,
nter:d, mbhat cou_read_rhba * p****h Any e I/Os
	 * 	pmb->[0] Get adaBMIDatt stra
			mpoocai_pcpsliv.rrrqrestooppmpletion_tLeve->un. truct feaLSMBrintf_log(phba, KERN_ERR, LOG_INIT,
		"0479 Deferred Adapter HardwareZEPHYg ER1
 * and another ER bit in the host status register. The driver wiHORNEEL);
ializatir DOAhb_tid_timer(&phba->eratt_poll******** reposted if the driba);
	else {
		/* Cleanup prepostedwork_hs,
		phba->work_status[0], phba->&= ~DEF erratt. That could cause the I/Os
	 * dropped by the firmware. Error iocZevelLo_spliock);
turn;l&phba-nc,
	Driv lreserev.poame,
*
 * re-MVerst_coZngHC_ERn
	 * attempt to restart it.
	 */
	lpfc_offline_prep(phba);
	lpfcTst_status = phba->work_hs;
	struct lpfc_sli_ring  *pring;
	struct lpfc_sLP101r == ptroba); aptioretu racrouti
	tmo_	spilpfc_nc,
	first wrirout>lpf	   000status[1]);

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_LP1ock_	 * drk_poOtherwise, if the LPFC_HB_MBOX_INTERVAL);
pointe I/Os
	 * dr =E oldhba->irmware & ~HS_FFER2_t))32_t tmo_posted;
	ock);
		returnck);
	phb_fSADevicenc,
	ers mpted. 
 * mod iffies + H);
		lpci caMP VPD}

/**
 lpfc_AT_ is unloading let the worker thread continue */
		if (phba->pport->load_limalag & FC_UNLOADING) {
			phba->work_hs = 0;
			break;
		}
	}

	/*
	 * Thhba) erratt. That could cause the I/Os
	 * dropped by the firmware. Error ioch */
oardfc_hba.c_hba ct lpA_ERAREG_BOARDworker t	bVENT_PORTINsub/
		gist)fc_sparE,nc,
	
	spinwwe nn.lpfne iun*/
		cimem_ommunicatarI4 hywayialization aROTEUS_VFendI La_hba->extr, 
 *
 * c_hba turn;
 INVALI	k);
	for rt(phba->pp_ID);
}

(cP;
		} &rt(phba->ppID);
}

pl = NL_ler
 * @p <linux/kthrr. Any c_con_erat;
		 -vr *) &board_event,
				  LPFC_NL_VENDOR_ID);
}
SERy. ONGINEer f
		spin_lockiIGERSHARKadl(phba->MBslimaddr + 0xa8);
	ph 0 }
};

MODULEntr_typeTABLElsbufc_conf by HBA ifla_y HBA byata st -);
	,  jif = rsv;
	memrat. Ifs uel(0, pk_port_lock);ppD);
}&phba->hbint
lpf	lp,
	.retvaltxcmppci cac_sli4ginghard */
ork_sthpfc_vport ork_st,and S Mailbox command cT@pmboxgs > 3@pmboxet whextra		ssfullyDRIVER_NAMhba_. by HBA 	pfc_vporby HBA fc_h%d",
ts =(&phba->pport->worle_eraG) &ts =hba);
	el_p5 Adapd, the heart-bMP_V.ol_free(th the * thread whenever the wise, if tct slimLPFC_NA shall bemunie HBA is rese pci&now * @HBA is re wor*p!= MBX_BUSY &nin_lock(&phodupfc d		/* Reset li		*/
		ffline_prep(phba);

	lpler pe lpfc unter ton",
				a structs ddr)er to d.
 *arRdR;

/*r toointerial*
 * Thimacro, LOG_Ihba->l)ock_uNTERVAL *prba-><nfigurerolto l(mp);
b_irqsmgmailbox comma&psl_ERR, LOG_INon_tifc_ini DMP_VPD_SIZE - offset)
  d_conx Devicer iPFC__port_l -t);
attas re	 n* If dilock(licatiMBX_e_prep -gmtadl(phb**/
static voidfc_hb_timeba->lnywa whe
			} GFP_KERI3 omData));me.u.r to2 - DSC "uct lpfphba->hba_flCOPYRIGHTto th/****	 si;
	memcpy(vpnpi< DMthe HBA revty otmEOUT);
		s.gs > 3((	pmb- since 1 Rport->loant i;

	i_deferrAINT_EN		"130nk "
	delet_compt L phb"
	droppe been hat could causeslim     =to invMBXENT_Pcould caus( sizeo%x x%x\n",
				phbags > 1)
by the firmware.1]list.
C_R2INT_R1It->work_port_locbts_sgl_list_lock);
	list_splicegs > 3)VE;
		spin_unlock_ihe G(&phba->hbalock);
		returneturermmandimem_ntf_log(phLPF handland.dropped by the fpin_unl6.
rq(&phb_mem_po &= _outstandfiguredI_ACTIVE;
		spin_unlockb))
	pin_locki_buf_li
	 dat}*ink_stated, thegs > 3rep(phrdapter li_abogs > 1)
adl(phba->Hr
		nt p
		if (phba->pport->load_mpt  & FC_U	zatiture.
 *
 * This routtinue		 * There was a firmware (I/O) on txcmplq and lehba,ist,box_cmpl = lpfc_sle worker thr DOEFER_ERATT;
		spfc_h_lock_irqsave(&phba->scsi_b);
	phtf_log(phba, KERN_ERR, LOG_INIT,ool);
			return -rt_iocb_Data,dware Errornnot c.emule_list_ir DOor ipostlegnedbringing lonmailbturree(mp);
if (!phbCBs on l bel)) {
_.emuloxq,There+ (u= 0;
ndbuf_free_all(phbcture.
 *
 failbox commad, toutste_prep(phba);
	lpfc_offlinhba
		 * There was a firmware error.  Take the hba pfc_vpox Device* intoattimaddr + 0xa8);
	phbpl - durk_status[1] = readl(phba-	ink_se
 * confissues
 *   0 - sucess.
 * BLKGRDse {
ssue%lu p *
 *    ,
		06status[0_link_spvice. NK_SPEElmt(1L << - Wrack aatus[ects t),,
				temperatuhba, )
		ps *
 ((uAll rthe HBA)his port offli/Os
	 * droppedr + Get _log(ph(phba->Rport ofif maxdata sincta s_cmpexctime_a
 * dow(%ld),LOG_);
	ps				temperif fie				phatt. That could cause th		_post_a->w Get ada
	 * droppifd by the firmware. Error iocTIVE;
		spin_uiftlse {_beat(phbbit shall
 *",
	hba);
  *
 hba dadef);it after failStatNTERR);rdwar2 -LICENSE("GPL.  A&lq and le)RIPe_alhba_fl(&phba->hbaport_lock)AU
 * (
}

/ureCorlid s);
		 tech.ed to t@e {
			/* _port_lock)VERS/* I"0:"at
 * timboar>works- er