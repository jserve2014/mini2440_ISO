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
/* See Fibre Channel protocol T11 FC-LS for details */
#include <linux/blkdev.h>
#include <linux/pci.h>
#include <linux/interrupt.h>

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
#include "lpfc_debugfs.h"

static int lpfc_els_retry(struct lpfc_hba *, struct lpfc_iocbq *,
			  struct lpfc_iocbq *);
static void lpfc_cmpl_fabric_iocb(struct lpfc_hba *, struct lpfc_iocbq *,
			struct lpfc_iocbq *);
static void lpfc_fabric_abort_vport(struct lpfc_vport *vport);
static int lpfc_issue_els_fdisc(struct lpfc_vport *vport,
				struct lpfc_nodelist *ndlp, uint8_t retry);
static int lpfc_issue_fabric_iocb(struct lpfc_hba *phba,
				  struct lpfc_iocbq *iocb);
static void lpfc_register_new_vport(struct lpfc_hba *phba,
				    struct lpfc_vport *vport,
				    struct lpfc_nodelist *ndlp);

static int lpfc_max_els_tries = 3;

/**
 * lpfc_els_chk_latt - Check host link attention event for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine checks whether there is an outstanding host link
 * attention event during the discovery process with the @vport. It is done
 * by reading the HBA's Host Attention (HA) register. If there is any host
 * link attention events during this @vport's discovery process, the @vport
 * shall be marked as FC_ABORT_DISCOVERY, a host link attention clear shall
 * be issued if the link state is not already in host link cleared state,
 * and a return code shall indicate whether the host link attention event
 * had happened.
 *
 * Note that, if either the host link is in state LPFC_LINK_DOWN or @vport
 * state in LPFC_VPORT_READY, the request for checking host link attention
 * event will be ignored and a return code shall indicate no host link
 * attention event had happened.
 *
 * Return codes
 *   0 - no host link attention event happened
 *   1 - host link attention event happened
 **/
int
lpfc_els_chk_latt(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	uint32_t ha_copy;

	if (vport->port_state >= LPFC_VPORT_READY ||
	    phba->link_state == LPFC_LINK_DOWN ||
	    phba->sli_rev > LPFC_SLI_REV3)
		return 0;

	/* Read the HBA Host Attention Register */
	ha_copy = readl(phba->HAregaddr);

	if (!(ha_copy & HA_LATT))
		return 0;

	/* Pending Link Event during Discovery */
	lpfc_printf_vlog(vport, KERN_ERR, LOG_DISCOVERY,
			 "0237 Pending Link Event during "
			 "Discovery: State x%x\n",
			 phba->pport->port_state);

	/* CLEAR_LA should re-enable link attention events and
	 * we should then imediately take a LATT event. The
	 * LATT processing should call lpfc_linkdown() which
	 * will cleanup any left over in-progress discovery
	 * events.
	 */
	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_ABORT_DISCOVERY;
	spin_unlock_irq(shost->host_lock);

	if (phba->link_state != LPFC_CLEAR_LA)
		lpfc_issue_clear_la(phba, vport);

	return 1;
}

/**
 * lpfc_prep_els_iocb - Allocate and prepare a lpfc iocb data structure
 * @vport: pointer to a host virtual N_Port data structure.
 * @expectRsp: flag indicating whether response is expected.
 * @cmdSize: size of the ELS command.
 * @retry: number of retries to the command IOCB when it fails.
 * @ndlp: pointer to a node-list data structure.
 * @did: destination identifier.
 * @elscmd: the ELS command code.
 *
 * This routine is used for allocating a lpfc-IOCB data structure from
 * the driver lpfc-IOCB free-list and prepare the IOCB with the parameters
 * passed into the routine for discovery state machine to issue an Extended
 * Link Service (ELS) commands. It is a generic lpfc-IOCB allocation
 * and preparation routine that is used by all the discovery state machine
 * routines and the ELS command-specific fields will be later set up by
 * the individual discovery machine routines after calling this routine
 * allocating and preparing a generic IOCB data structure. It fills in the
 * Buffer Descriptor Entries (BDEs), allocates buffers for both command
 * payload and response payload (if expected). The reference count on the
 * ndlp is incremented by 1 and the reference to the ndlp is put into
 * context1 of the IOCB data structure for this IOCB to hold the ndlp
 * reference for the command's callback function to access later.
 *
 * Return code
 *   Pointer to the newly allocated/prepared els iocb data structure
 *   NULL - when els iocb data structure allocation/preparation failed
 **/
struct lpfc_iocbq *
lpfc_prep_els_iocb(struct lpfc_vport *vport, uint8_t expectRsp,
		   uint16_t cmdSize, uint8_t retry,
		   struct lpfc_nodelist *ndlp, uint32_t did,
		   uint32_t elscmd)
{
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_dmabuf *pcmd, *prsp, *pbuflist;
	struct ulp_bde64 *bpl;
	IOCB_t *icmd;


	if (!lpfc_is_link_up(phba))
		return NULL;

	/* Allocate buffer for  command iocb */
	elsiocb = lpfc_sli_get_iocbq(phba);

	if (elsiocb == NULL)
		return NULL;

	/*
	 * If this command is for fabric controller and HBA running
	 * in FIP mode send FLOGI, FDISC and LOGO as FIP frames.
	 */
	if ((did == Fabric_DID) &&
		bf_get(lpfc_fip_flag, &phba->sli4_hba.sli4_flags) &&
		((elscmd == ELS_CMD_FLOGI) ||
		 (elscmd == ELS_CMD_FDISC) ||
		 (elscmd == ELS_CMD_LOGO)))
		elsiocb->iocb_flag |= LPFC_FIP_ELS;
	else
		elsiocb->iocb_flag &= ~LPFC_FIP_ELS;

	icmd = &elsiocb->iocb;

	/* fill in BDEs for command */
	/* Allocate buffer for command payload */
	pcmd = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (pcmd)
		pcmd->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &pcmd->phys);
	if (!pcmd || !pcmd->virt)
		goto els_iocb_free_pcmb_exit;

	INIT_LIST_HEAD(&pcmd->list);

	/* Allocate buffer for response payload */
	if (expectRsp) {
		prsp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
		if (prsp)
			prsp->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
						     &prsp->phys);
		if (!prsp || !prsp->virt)
			goto els_iocb_free_prsp_exit;
		INIT_LIST_HEAD(&prsp->list);
	} else
		prsp = NULL;

	/* Allocate buffer for Buffer ptr list */
	pbuflist = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (pbuflist)
		pbuflist->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
						 &pbuflist->phys);
	if (!pbuflist || !pbuflist->virt)
		goto els_iocb_free_pbuf_exit;

	INIT_LIST_HEAD(&pbuflist->list);

	icmd->un.elsreq64.bdl.addrHigh = putPaddrHigh(pbuflist->phys);
	icmd->un.elsreq64.bdl.addrLow = putPaddrLow(pbuflist->phys);
	icmd->un.elsreq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	icmd->un.elsreq64.remoteID = did;	/* DID */
	if (expectRsp) {
		icmd->un.elsreq64.bdl.bdeSize = (2 * sizeof(struct ulp_bde64));
		icmd->ulpCommand = CMD_ELS_REQUEST64_CR;
		icmd->ulpTimeout = phba->fc_ratov * 2;
	} else {
		icmd->un.elsreq64.bdl.bdeSize = sizeof(struct ulp_bde64);
		icmd->ulpCommand = CMD_XMIT_ELS_RSP64_CX;
	}
	icmd->ulpBdeCount = 1;
	icmd->ulpLe = 1;
	icmd->ulpClass = CLASS3;

	if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
		icmd->un.elsreq64.myID = vport->fc_myDID;

		/* For ELS_REQUEST64_CR, use the VPI by default */
		icmd->ulpContext = vport->vpi + phba->vpi_base;
		icmd->ulpCt_h = 0;
		/* The CT field must be 0=INVALID_RPI for the ECHO cmd */
		if (elscmd == ELS_CMD_ECHO)
			icmd->ulpCt_l = 0; /* context = invalid RPI */
		else
			icmd->ulpCt_l = 1; /* context = VPI */
	}

	bpl = (struct ulp_bde64 *) pbuflist->virt;
	bpl->addrLow = le32_to_cpu(putPaddrLow(pcmd->phys));
	bpl->addrHigh = le32_to_cpu(putPaddrHigh(pcmd->phys));
	bpl->tus.f.bdeSize = cmdSize;
	bpl->tus.f.bdeFlags = 0;
	bpl->tus.w = le32_to_cpu(bpl->tus.w);

	if (expectRsp) {
		bpl++;
		bpl->addrLow = le32_to_cpu(putPaddrLow(prsp->phys));
		bpl->addrHigh = le32_to_cpu(putPaddrHigh(prsp->phys));
		bpl->tus.f.bdeSize = FCELSSIZE;
		bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64;
		bpl->tus.w = le32_to_cpu(bpl->tus.w);
	}

	/* prevent preparing iocb with NULL ndlp reference */
	elsiocb->context1 = lpfc_nlp_get(ndlp);
	if (!elsiocb->context1)
		goto els_iocb_free_pbuf_exit;
	elsiocb->context2 = pcmd;
	elsiocb->context3 = pbuflist;
	elsiocb->retry = retry;
	elsiocb->vport = vport;
	elsiocb->drvrTimeout = (phba->fc_ratov << 1) + LPFC_DRVR_TIMEOUT;

	if (prsp) {
		list_add(&prsp->list, &pcmd->list);
	}
	if (expectRsp) {
		/* Xmit ELS command <elsCmd> to remote NPORT <did> */
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0116 Xmit ELS command x%x to remote "
				 "NPORT x%x I/O tag: x%x, port state: x%x\n",
				 elscmd, did, elsiocb->iotag,
				 vport->port_state);
	} else {
		/* Xmit ELS response <elsCmd> to remote NPORT <did> */
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0117 Xmit ELS response x%x to remote "
				 "NPORT x%x I/O tag: x%x, size: x%x\n",
				 elscmd, ndlp->nlp_DID, elsiocb->iotag,
				 cmdSize);
	}
	return elsiocb;

els_iocb_free_pbuf_exit:
	if (expectRsp)
		lpfc_mbuf_free(phba, prsp->virt, prsp->phys);
	kfree(pbuflist);

els_iocb_free_prsp_exit:
	lpfc_mbuf_free(phba, pcmd->virt, pcmd->phys);
	kfree(prsp);

els_iocb_free_pcmb_exit:
	kfree(pcmd);
	lpfc_sli_release_iocbq(phba, elsiocb);
	return NULL;
}

/**
 * lpfc_issue_fabric_reglogin - Issue fabric registration login for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine issues a fabric registration login for a @vport. An
 * active ndlp node with Fabric_DID must already exist for this @vport.
 * The routine invokes two mailbox commands to carry out fabric registration
 * login through the HBA firmware: the first mailbox command requests the
 * HBA to perform link configuration for the @vport; and the second mailbox
 * command requests the HBA to perform the actual fabric registration login
 * with the @vport.
 *
 * Return code
 *   0 - successfully issued fabric registration login for @vport
 *   -ENXIO -- failed to issue fabric registration login for @vport
 **/
int
lpfc_issue_fabric_reglogin(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	struct lpfc_dmabuf *mp;
	struct lpfc_nodelist *ndlp;
	struct serv_parm *sp;
	int rc;
	int err = 0;

	sp = &phba->fc_fabparam;
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		err = 1;
		goto fail;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		err = 2;
		goto fail;
	}

	vport->port_state = LPFC_FABRIC_CFG_LINK;
	lpfc_config_link(phba, mbox);
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mbox->vport = vport;

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		err = 3;
		goto fail_free_mbox;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		err = 4;
		goto fail;
	}
	rc = lpfc_reg_rpi(phba, vport->vpi, Fabric_DID, (uint8_t *)sp, mbox, 0);
	if (rc) {
		err = 5;
		goto fail_free_mbox;
	}

	mbox->mbox_cmpl = lpfc_mbx_cmpl_fabric_reg_login;
	mbox->vport = vport;
	/* increment the reference count on ndlp to hold reference
	 * for the callback routine.
	 */
	mbox->context2 = lpfc_nlp_get(ndlp);

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		err = 6;
		goto fail_issue_reg_login;
	}

	return 0;

fail_issue_reg_login:
	/* decrement the reference count on ndlp just incremented
	 * for the failed mbox command.
	 */
	lpfc_nlp_put(ndlp);
	mp = (struct lpfc_dmabuf *) mbox->context1;
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
fail_free_mbox:
	mempool_free(mbox, phba->mbox_mem_pool);

fail:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
		"0249 Cannot issue Register Fabric login: Err %d\n", err);
	return -ENXIO;
}

/**
 * lpfc_issue_reg_vfi - Register VFI for this vport's fabric login
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine issues a REG_VFI mailbox for the vfi, vpi, fcfi triplet for
 * the @vport. This mailbox command is necessary for FCoE only.
 *
 * Return code
 *   0 - successfully issued REG_VFI for @vport
 *   A failure code otherwise.
 **/
static int
lpfc_issue_reg_vfi(struct lpfc_vport *vport)
{
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_nodelist *ndlp;
	struct serv_parm *sp;
	struct lpfc_dmabuf *dmabuf;
	int rc = 0;

	sp = &phba->fc_fabparam;
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		rc = -ENODEV;
		goto fail;
	}

	dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!dmabuf) {
		rc = -ENOMEM;
		goto fail;
	}
	dmabuf->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &dmabuf->phys);
	if (!dmabuf->virt) {
		rc = -ENOMEM;
		goto fail_free_dmabuf;
	}
	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		rc = -ENOMEM;
		goto fail_free_coherent;
	}
	vport->port_state = LPFC_FABRIC_CFG_LINK;
	memcpy(dmabuf->virt, &phba->fc_fabparam, sizeof(vport->fc_sparam));
	lpfc_reg_vfi(mboxq, vport, dmabuf->phys);
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_reg_vfi;
	mboxq->vport = vport;
	mboxq->context1 = dmabuf;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		rc = -ENXIO;
		goto fail_free_mbox;
	}
	return 0;

fail_free_mbox:
	mempool_free(mboxq, phba->mbox_mem_pool);
fail_free_coherent:
	lpfc_mbuf_free(phba, dmabuf->virt, dmabuf->phys);
fail_free_dmabuf:
	kfree(dmabuf);
fail:
	lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
		"0289 Issue Register VFI failed: Err %d\n", rc);
	return rc;
}

/**
 * lpfc_cmpl_els_flogi_fabric - Completion function for flogi to a fabric port
 * @vport: pointer to a host virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @sp: pointer to service parameter data structure.
 * @irsp: pointer to the IOCB within the lpfc response IOCB.
 *
 * This routine is invoked by the lpfc_cmpl_els_flogi() completion callback
 * function to handle the completion of a Fabric Login (FLOGI) into a fabric
 * port in a fabric topology. It properly sets up the parameters to the @ndlp
 * from the IOCB response. It also check the newly assigned N_Port ID to the
 * @vport against the previously assigned N_Port ID. If it is different from
 * the previously assigned Destination ID (DID), the lpfc_unreg_rpi() routine
 * is invoked on all the remaining nodes with the @vport to unregister the
 * Remote Port Indicators (RPIs). Finally, the lpfc_issue_fabric_reglogin()
 * is invoked to register login to the fabric.
 *
 * Return code
 *   0 - Success (currently, always return 0)
 **/
static int
lpfc_cmpl_els_flogi_fabric(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
			   struct serv_parm *sp, IOCB_t *irsp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_nodelist *np;
	struct lpfc_nodelist *next_np;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_FABRIC;
	spin_unlock_irq(shost->host_lock);

	phba->fc_edtov = be32_to_cpu(sp->cmn.e_d_tov);
	if (sp->cmn.edtovResolution)	/* E_D_TOV ticks are in nanoseconds */
		phba->fc_edtov = (phba->fc_edtov + 999999) / 1000000;

	phba->fc_ratov = (be32_to_cpu(sp->cmn.w2.r_a_tov) + 999) / 1000;

	if (phba->fc_topology == TOPOLOGY_LOOP) {
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_PUBLIC_LOOP;
		spin_unlock_irq(shost->host_lock);
	} else {
		/*
		 * If we are a N-port connected to a Fabric, fixup sparam's so
		 * logins to devices on remote loops work.
		 */
		vport->fc_sparam.cmn.altBbCredit = 1;
	}

	vport->fc_myDID = irsp->un.ulpWord[4] & Mask_DID;
	memcpy(&ndlp->nlp_portname, &sp->portName, sizeof(struct lpfc_name));
	memcpy(&ndlp->nlp_nodename, &sp->nodeName, sizeof(struct lpfc_name));
	ndlp->nlp_class_sup = 0;
	if (sp->cls1.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS1;
	if (sp->cls2.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS2;
	if (sp->cls3.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS3;
	if (sp->cls4.classValid)
		ndlp->nlp_class_sup |= FC_COS_CLASS4;
	ndlp->nlp_maxframe = ((sp->cmn.bbRcvSizeMsb & 0x0F) << 8) |
				sp->cmn.bbRcvSizeLsb;
	memcpy(&phba->fc_fabparam, sp, sizeof(struct serv_parm));

	if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
		if (sp->cmn.response_multiple_NPort) {
			lpfc_printf_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 "1816 FLOGI NPIV supported, "
					 "response data 0x%x\n",
					 sp->cmn.response_multiple_NPort);
			phba->link_flag |= LS_NPIV_FAB_SUPPORTED;
		} else {
			/* Because we asked f/w for NPIV it still expects us
			to call reg_vnpid atleast for the physcial host */
			lpfc_printf_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 "1817 Fabric does not support NPIV "
					 "- configuring single port mode.\n");
			phba->link_flag &= ~LS_NPIV_FAB_SUPPORTED;
		}
	}

	if ((vport->fc_prevDID != vport->fc_myDID) &&
		!(vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)) {

		/* If our NportID changed, we need to ensure all
		 * remaining NPORTs get unreg_login'ed.
		 */
		list_for_each_entry_safe(np, next_np,
					&vport->fc_nodes, nlp_listp) {
			if (!NLP_CHK_NODE_ACT(np))
				continue;
			if ((np->nlp_state != NLP_STE_NPR_NODE) ||
				   !(np->nlp_flag & NLP_NPR_ADISC))
				continue;
			spin_lock_irq(shost->host_lock);
			np->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlock_irq(shost->host_lock);
			lpfc_unreg_rpi(vport, np);
		}
		if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
			lpfc_mbx_unreg_vpi(vport);
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			spin_unlock_irq(shost->host_lock);
		}
	}

	if (phba->sli_rev < LPFC_SLI_REV4) {
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_REG_LOGIN_ISSUE);
		if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED &&
		    vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)
			lpfc_register_new_vport(phba, vport, ndlp);
		else
			lpfc_issue_fabric_reglogin(vport);
	} else {
		ndlp->nlp_type |= NLP_FABRIC;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_UNMAPPED_NODE);
		if (vport->vfi_state & LPFC_VFI_REGISTERED) {
			lpfc_start_fdiscs(phba);
			lpfc_do_scr_ns_plogi(phba, vport);
		} else
			lpfc_issue_reg_vfi(vport);
	}
	return 0;
}
/**
 * lpfc_cmpl_els_flogi_nport - Completion function for flogi to an N_Port
 * @vport: pointer to a host virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @sp: pointer to service parameter data structure.
 *
 * This routine is invoked by the lpfc_cmpl_els_flogi() completion callback
 * function to handle the completion of a Fabric Login (FLOGI) into an N_Port
 * in a point-to-point topology. First, the @vport's N_Port Name is compared
 * with the received N_Port Name: if the @vport's N_Port Name is greater than
 * the received N_Port Name lexicographically, this node shall assign local
 * N_Port ID (PT2PT_LocalID: 1) and remote N_Port ID (PT2PT_RemoteID: 2) and
 * will send out Port Login (PLOGI) with the N_Port IDs assigned. Otherwise,
 * this node shall just wait for the remote node to issue PLOGI and assign
 * N_Port IDs.
 *
 * Return code
 *   0 - Success
 *   -ENXIO - Fail
 **/
static int
lpfc_cmpl_els_flogi_nport(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
			  struct serv_parm *sp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	LPFC_MBOXQ_t *mbox;
	int rc;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
	spin_unlock_irq(shost->host_lock);

	phba->fc_edtov = FF_DEF_EDTOV;
	phba->fc_ratov = FF_DEF_RATOV;
	rc = memcmp(&vport->fc_portname, &sp->portName,
		    sizeof(vport->fc_portname));
	if (rc >= 0) {
		/* This side will initiate the PLOGI */
		spin_lock_irq(shost->host_lock);
		vport->fc_flag |= FC_PT2PT_PLOGI;
		spin_unlock_irq(shost->host_lock);

		/*
		 * N_Port ID cannot be 0, set our to LocalID the other
		 * side will be RemoteID.
		 */

		/* not equal */
		if (rc)
			vport->fc_myDID = PT2PT_LocalID;

		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (!mbox)
			goto fail;

		lpfc_config_link(phba, mbox);

		mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		mbox->vport = vport;
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);
		if (rc == MBX_NOT_FINISHED) {
			mempool_free(mbox, phba->mbox_mem_pool);
			goto fail;
		}
		/* Decrement ndlp reference count indicating that ndlp can be
		 * safely released when other references to it are done.
		 */
		lpfc_nlp_put(ndlp);

		ndlp = lpfc_findnode_did(vport, PT2PT_RemoteID);
		if (!ndlp) {
			/*
			 * Cannot find existing Fabric ndlp, so allocate a
			 * new one
			 */
			ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
			if (!ndlp)
				goto fail;
			lpfc_nlp_init(vport, ndlp, PT2PT_RemoteID);
		} else if (!NLP_CHK_NODE_ACT(ndlp)) {
			ndlp = lpfc_enable_node(vport, ndlp,
						NLP_STE_UNUSED_NODE);
			if(!ndlp)
				goto fail;
		}

		memcpy(&ndlp->nlp_portname, &sp->portName,
		       sizeof(struct lpfc_name));
		memcpy(&ndlp->nlp_nodename, &sp->nodeName,
		       sizeof(struct lpfc_name));
		/* Set state will put ndlp onto node list if not already done */
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
	} else
		/* This side will wait for the PLOGI, decrement ndlp reference
		 * count indicating that ndlp can be released when other
		 * references to it are done.
		 */
		lpfc_nlp_put(ndlp);

	/* If we are pt2pt with another NPort, force NPIV off! */
	phba->sli3_options &= ~LPFC_SLI3_NPIV_ENABLED;

	spin_lock_irq(shost->host_lock);
	vport->fc_flag |= FC_PT2PT;
	spin_unlock_irq(shost->host_lock);

	/* Start discovery - this should just do CLEAR_LA */
	lpfc_disc_start(vport);
	return 0;
fail:
	return -ENXIO;
}

/**
 * lpfc_cmpl_els_flogi - Completion callback function for flogi
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the top-level completion callback function for issuing
 * a Fabric Login (FLOGI) command. If the response IOCB reported error,
 * the lpfc_els_retry() routine shall be invoked to retry the FLOGI. If
 * retry has been made (either immediately or delayed with lpfc_els_retry()
 * returning 1), the command IOCB will be released and function returned.
 * If the retry attempt has been given up (possibly reach the maximum
 * number of retries), one additional decrement of ndlp reference shall be
 * invoked before going out after releasing the command IOCB. This will
 * actually release the remote node (Note, lpfc_els_free_iocb() will also
 * invoke one decrement of ndlp reference count). If no error reported in
 * the IOCB status, the command Port ID field is used to determine whether
 * this is a point-to-point topology or a fabric topology: if the Port ID
 * field is assigned, it is a fabric topology; otherwise, it is a
 * point-to-point topology. The routine lpfc_cmpl_els_flogi_fabric() or
 * lpfc_cmpl_els_flogi_nport() shall be invoked accordingly to handle the
 * specific topology completion conditions.
 **/
static void
lpfc_cmpl_els_flogi(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp = &rspiocb->iocb;
	struct lpfc_nodelist *ndlp = cmdiocb->context1;
	struct lpfc_dmabuf *pcmd = cmdiocb->context2, *prsp;
	struct serv_parm *sp;
	int rc;

	/* Check to see if link went down during discovery */
	if (lpfc_els_chk_latt(vport)) {
		/* One additional decrement on node reference count to
		 * trigger the release of the node
		 */
		lpfc_nlp_put(ndlp);
		goto out;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"FLOGI cmpl:      status:x%x/x%x state:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		vport->port_state);

	if (irsp->ulpStatus) {
		/* Check for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb))
			goto out;

		/* FLOGI failed, so there is no fabric */
		spin_lock_irq(shost->host_lock);
		vport->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
		spin_unlock_irq(shost->host_lock);

		/* If private loop, then allow max outstanding els to be
		 * LPFC_MAX_DISC_THREADS (32). Scanning in the case of no
		 * alpa map would take too long otherwise.
		 */
		if (phba->alpa_map[0] == 0) {
			vport->cfg_discovery_threads = LPFC_MAX_DISC_THREADS;
		}

		/* FLOGI failure */
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0100 FLOGI failure Data: x%x x%x "
				 "x%x\n",
				 irsp->ulpStatus, irsp->un.ulpWord[4],
				 irsp->ulpTimeout);
		goto flogifail;
	}

	/*
	 * The FLogI succeeded.  Sync the data for the CPU before
	 * accessing it.
	 */
	prsp = list_get_first(&pcmd->list, struct lpfc_dmabuf, list);

	sp = prsp->virt + sizeof(uint32_t);

	/* FLOGI completes successfully */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0101 FLOGI completes sucessfully "
			 "Data: x%x x%x x%x x%x\n",
			 irsp->un.ulpWord[4], sp->cmn.e_d_tov,
			 sp->cmn.w2.r_a_tov, sp->cmn.edtovResolution);

	if (vport->port_state == LPFC_FLOGI) {
		/*
		 * If Common Service Parameters indicate Nport
		 * we are point to point, if Fport we are Fabric.
		 */
		if (sp->cmn.fPort)
			rc = lpfc_cmpl_els_flogi_fabric(vport, ndlp, sp, irsp);
		else
			rc = lpfc_cmpl_els_flogi_nport(vport, ndlp, sp);

		if (!rc)
			goto out;
	}

flogifail:
	lpfc_nlp_put(ndlp);

	if (!lpfc_error_lost_link(irsp)) {
		/* FLOGI failed, so just use loop map to make discovery list */
		lpfc_disc_list_loopmap(vport);

		/* Start discovery */
		lpfc_disc_start(vport);
	} else if (((irsp->ulpStatus != IOSTAT_LOCAL_REJECT) ||
			((irsp->un.ulpWord[4] != IOERR_SLI_ABORTED) &&
			(irsp->un.ulpWord[4] != IOERR_SLI_DOWN))) &&
			(phba->link_state != LPFC_CLEAR_LA)) {
		/* If FLOGI failed enable link interrupt. */
		lpfc_issue_clear_la(phba, vport);
	}
out:
	lpfc_els_free_iocb(phba, cmdiocb);
}

/**
 * lpfc_issue_els_flogi - Issue an flogi iocb command for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues a Fabric Login (FLOGI) Request ELS command
 * for a @vport. The initiator service parameters are put into the payload
 * of the FLOGI Request IOCB and the top-level callback function pointer
 * to lpfc_cmpl_els_flogi() routine is put to the IOCB completion callback
 * function field. The lpfc_issue_fabric_iocb routine is invoked to send
 * out FLOGI ELS command with one outstanding fabric IOCB at a time.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the FLOGI ELS command.
 *
 * Return code
 *   0 - successfully issued flogi iocb for @vport
 *   1 - failed to issue flogi iocb for @vport
 **/
static int
lpfc_issue_els_flogi(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		     uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	struct serv_parm *sp;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli_ring *pring;
	uint8_t *pcmd;
	uint16_t cmdsize;
	uint32_t tmo;
	int rc;

	pring = &phba->sli.ring[LPFC_ELS_RING];

	cmdsize = (sizeof(uint32_t) + sizeof(struct serv_parm));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_FLOGI);

	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	/* For FLOGI request, remainder of payload is service parameters */
	*((uint32_t *) (pcmd)) = ELS_CMD_FLOGI;
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_sparam, sizeof(struct serv_parm));
	sp = (struct serv_parm *) pcmd;

	/* Setup CSPs accordingly for Fabric */
	sp->cmn.e_d_tov = 0;
	sp->cmn.w2.r_a_tov = 0;
	sp->cls1.classValid = 0;
	sp->cls2.seqDelivery = 1;
	sp->cls3.seqDelivery = 1;
	if (sp->cmn.fcphLow < FC_PH3)
		sp->cmn.fcphLow = FC_PH3;
	if (sp->cmn.fcphHigh < FC_PH3)
		sp->cmn.fcphHigh = FC_PH3;

	if  (phba->sli_rev == LPFC_SLI_REV4) {
		elsiocb->iocb.ulpCt_h = ((SLI4_CT_FCFI >> 1) & 1);
		elsiocb->iocb.ulpCt_l = (SLI4_CT_FCFI & 1);
		/* FLOGI needs to be 3 for WQE FCFI */
		/* Set the fcfi to the fcfi we registered with */
		elsiocb->iocb.ulpContext = phba->fcf.fcfi;
	} else if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
		sp->cmn.request_multiple_Nport = 1;
		/* For FLOGI, Let FLOGI rsp set the NPortID for VPI 0 */
		icmd->ulpCt_h = 1;
		icmd->ulpCt_l = 0;
	}

	if (phba->fc_topology != TOPOLOGY_LOOP) {
		icmd->un.elsreq64.myID = 0;
		icmd->un.elsreq64.fl = 1;
	}

	tmo = phba->fc_ratov;
	phba->fc_ratov = LPFC_DISC_FLOGI_TMO;
	lpfc_set_disctmo(vport);
	phba->fc_ratov = tmo;

	phba->fc_stat.elsXmitFLOGI++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_flogi;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue FLOGI:     opt:x%x",
		phba->sli3_options, 0, 0);

	rc = lpfc_issue_fabric_iocb(phba, elsiocb);
	if (rc == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_els_abort_flogi - Abort all outstanding flogi iocbs
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine aborts all the outstanding Fabric Login (FLOGI) IOCBs
 * with a @phba. This routine walks all the outstanding IOCBs on the txcmplq
 * list and issues an abort IOCB commond on each outstanding IOCB that
 * contains a active Fabric_DID ndlp. Note that this function is to issue
 * the abort IOCB command on all the outstanding IOCBs, thus when this
 * function returns, it does not guarantee all the IOCBs are actually aborted.
 *
 * Return code
 *   0 - Sucessfully issued abort iocb on all outstanding flogis (Always 0)
 **/
int
lpfc_els_abort_flogi(struct lpfc_hba *phba)
{
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *iocb, *next_iocb;
	struct lpfc_nodelist *ndlp;
	IOCB_t *icmd;

	/* Abort outstanding I/O on NPort <nlp_DID> */
	lpfc_printf_log(phba, KERN_INFO, LOG_DISCOVERY,
			"0201 Abort outstanding I/O on NPort x%x\n",
			Fabric_DID);

	pring = &phba->sli.ring[LPFC_ELS_RING];

	/*
	 * Check the txcmplq for an iocb that matches the nport the driver is
	 * searching for.
	 */
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq, list) {
		icmd = &iocb->iocb;
		if (icmd->ulpCommand == CMD_ELS_REQUEST64_CR &&
		    icmd->un.elsreq64.bdl.ulpIoTag32) {
			ndlp = (struct lpfc_nodelist *)(iocb->context1);
			if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
			    (ndlp->nlp_DID == Fabric_DID))
				lpfc_sli_issue_abort_iotag(phba, pring, iocb);
		}
	}
	spin_unlock_irq(&phba->hbalock);

	return 0;
}

/**
 * lpfc_initial_flogi - Issue an initial fabric login for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine issues an initial Fabric Login (FLOGI) for the @vport
 * specified. It first searches the ndlp with the Fabric_DID (0xfffffe) from
 * the @vport's ndlp list. If no such ndlp found, it will create an ndlp and
 * put it into the @vport's ndlp list. If an inactive ndlp found on the list,
 * it will just be enabled and made active. The lpfc_issue_els_flogi() routine
 * is then invoked with the @vport and the ndlp to perform the FLOGI for the
 * @vport.
 *
 * Return code
 *   0 - failed to issue initial flogi for @vport
 *   1 - successfully issued initial flogi for @vport
 **/
int
lpfc_initial_flogi(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_nodelist *ndlp;

	vport->port_state = LPFC_FLOGI;
	lpfc_set_disctmo(vport);

	/* First look for the Fabric ndlp */
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp) {
		/* Cannot find existing Fabric ndlp, so allocate a new one */
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 0;
		lpfc_nlp_init(vport, ndlp, Fabric_DID);
		/* Set the node type */
		ndlp->nlp_type |= NLP_FABRIC;
		/* Put ndlp onto node list */
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		/* re-setup ndlp without removing from node list */
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp)
			return 0;
	}

	if (lpfc_issue_els_flogi(vport, ndlp, 0))
		/* This decrement of reference count to node shall kick off
		 * the release of the node.
		 */
		lpfc_nlp_put(ndlp);

	return 1;
}

/**
 * lpfc_initial_fdisc - Issue an initial fabric discovery for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine issues an initial Fabric Discover (FDISC) for the @vport
 * specified. It first searches the ndlp with the Fabric_DID (0xfffffe) from
 * the @vport's ndlp list. If no such ndlp found, it will create an ndlp and
 * put it into the @vport's ndlp list. If an inactive ndlp found on the list,
 * it will just be enabled and made active. The lpfc_issue_els_fdisc() routine
 * is then invoked with the @vport and the ndlp to perform the FDISC for the
 * @vport.
 *
 * Return code
 *   0 - failed to issue initial fdisc for @vport
 *   1 - successfully issued initial fdisc for @vport
 **/
int
lpfc_initial_fdisc(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_nodelist *ndlp;

	/* First look for the Fabric ndlp */
	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (!ndlp) {
		/* Cannot find existing Fabric ndlp, so allocate a new one */
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 0;
		lpfc_nlp_init(vport, ndlp, Fabric_DID);
		/* Put ndlp onto node list */
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		/* re-setup ndlp without removing from node list */
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp)
			return 0;
	}

	if (lpfc_issue_els_fdisc(vport, ndlp, 0)) {
		/* decrement node reference count to trigger the release of
		 * the node.
		 */
		lpfc_nlp_put(ndlp);
		return 0;
	}
	return 1;
}

/**
 * lpfc_more_plogi - Check and issue remaining plogis for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine checks whether there are more remaining Port Logins
 * (PLOGI) to be issued for the @vport. If so, it will invoke the routine
 * lpfc_els_disc_plogi() to go through the Node Port Recovery (NPR) nodes
 * to issue ELS PLOGIs up to the configured discover threads with the
 * @vport (@vport->cfg_discovery_threads). The function also decrement
 * the @vport's num_disc_node by 1 if it is not already 0.
 **/
void
lpfc_more_plogi(struct lpfc_vport *vport)
{
	int sentplogi;

	if (vport->num_disc_nodes)
		vport->num_disc_nodes--;

	/* Continue discovery with <num_disc_nodes> PLOGIs to go */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0232 Continue discovery with %d PLOGIs to go "
			 "Data: x%x x%x x%x\n",
			 vport->num_disc_nodes, vport->fc_plogi_cnt,
			 vport->fc_flag, vport->port_state);
	/* Check to see if there are more PLOGIs to be sent */
	if (vport->fc_flag & FC_NLP_MORE)
		/* go thru NPR nodes and issue any remaining ELS PLOGIs */
		sentplogi = lpfc_els_disc_plogi(vport);

	return;
}

/**
 * lpfc_plogi_confirm_nport - Confirm pologi wwpn matches stored ndlp
 * @phba: pointer to lpfc hba data structure.
 * @prsp: pointer to response IOCB payload.
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine checks and indicates whether the WWPN of an N_Port, retrieved
 * from a PLOGI, matches the WWPN that is stored in the @ndlp for that N_POrt.
 * The following cases are considered N_Port confirmed:
 * 1) The N_Port is a Fabric ndlp; 2) The @ndlp is on vport list and matches
 * the WWPN of the N_Port logged into; 3) The @ndlp is not on vport list but
 * it does not have WWPN assigned either. If the WWPN is confirmed, the
 * pointer to the @ndlp will be returned. If the WWPN is not confirmed:
 * 1) if there is a node on vport list other than the @ndlp with the same
 * WWPN of the N_Port PLOGI logged into, the lpfc_unreg_rpi() will be invoked
 * on that node to release the RPI associated with the node; 2) if there is
 * no node found on vport list with the same WWPN of the N_Port PLOGI logged
 * into, a new node shall be allocated (or activated). In either case, the
 * parameters of the @ndlp shall be copied to the new_ndlp, the @ndlp shall
 * be released and the new_ndlp shall be put on to the vport node list and
 * its pointer returned as the confirmed node.
 *
 * Note that before the @ndlp got "released", the keepDID from not-matching
 * or inactive "new_ndlp" on the vport node list is assigned to the nlp_DID
 * of the @ndlp. This is because the release of @ndlp is actually to put it
 * into an inactive state on the vport node list and the vport node list
 * management algorithm does not allow two node with a same DID.
 *
 * Return code
 *   pointer to the PLOGI N_Port @ndlp
 **/
static struct lpfc_nodelist *
lpfc_plogi_confirm_nport(struct lpfc_hba *phba, uint32_t *prsp,
			 struct lpfc_nodelist *ndlp)
{
	struct lpfc_vport    *vport = ndlp->vport;
	struct lpfc_nodelist *new_ndlp;
	struct lpfc_rport_data *rdata;
	struct fc_rport *rport;
	struct serv_parm *sp;
	uint8_t  name[sizeof(struct lpfc_name)];
	uint32_t rc, keepDID = 0;

	/* Fabric nodes can have the same WWPN so we don't bother searching
	 * by WWPN.  Just return the ndlp that was given to us.
	 */
	if (ndlp->nlp_type & NLP_FABRIC)
		return ndlp;

	sp = (struct serv_parm *) ((uint8_t *) prsp + sizeof(uint32_t));
	memset(name, 0, sizeof(struct lpfc_name));

	/* Now we find out if the NPort we are logging into, matches the WWPN
	 * we have for that ndlp. If not, we have some work to do.
	 */
	new_ndlp = lpfc_findnode_wwpn(vport, &sp->portName);

	if (new_ndlp == ndlp && NLP_CHK_NODE_ACT(new_ndlp))
		return ndlp;

	if (!new_ndlp) {
		rc = memcmp(&ndlp->nlp_portname, name,
			    sizeof(struct lpfc_name));
		if (!rc)
			return ndlp;
		new_ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_ATOMIC);
		if (!new_ndlp)
			return ndlp;
		lpfc_nlp_init(vport, new_ndlp, ndlp->nlp_DID);
	} else if (!NLP_CHK_NODE_ACT(new_ndlp)) {
		rc = memcmp(&ndlp->nlp_portname, name,
			    sizeof(struct lpfc_name));
		if (!rc)
			return ndlp;
		new_ndlp = lpfc_enable_node(vport, new_ndlp,
						NLP_STE_UNUSED_NODE);
		if (!new_ndlp)
			return ndlp;
		keepDID = new_ndlp->nlp_DID;
	} else
		keepDID = new_ndlp->nlp_DID;

	lpfc_unreg_rpi(vport, new_ndlp);
	new_ndlp->nlp_DID = ndlp->nlp_DID;
	new_ndlp->nlp_prev_state = ndlp->nlp_prev_state;

	if (ndlp->nlp_flag & NLP_NPR_2B_DISC)
		new_ndlp->nlp_flag |= NLP_NPR_2B_DISC;
	ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;

	/* Set state will put new_ndlp on to node list if not already done */
	lpfc_nlp_set_state(vport, new_ndlp, ndlp->nlp_state);

	/* Move this back to NPR state */
	if (memcmp(&ndlp->nlp_portname, name, sizeof(struct lpfc_name)) == 0) {
		/* The new_ndlp is replacing ndlp totally, so we need
		 * to put ndlp on UNUSED list and try to free it.
		 */

		/* Fix up the rport accordingly */
		rport =  ndlp->rport;
		if (rport) {
			rdata = rport->dd_data;
			if (rdata->pnode == ndlp) {
				lpfc_nlp_put(ndlp);
				ndlp->rport = NULL;
				rdata->pnode = lpfc_nlp_get(new_ndlp);
				new_ndlp->rport = rport;
			}
			new_ndlp->nlp_type = ndlp->nlp_type;
		}
		/* We shall actually free the ndlp with both nlp_DID and
		 * nlp_portname fields equals 0 to avoid any ndlp on the
		 * nodelist never to be used.
		 */
		if (ndlp->nlp_DID == 0) {
			spin_lock_irq(&phba->ndlp_lock);
			NLP_SET_FREE_REQ(ndlp);
			spin_unlock_irq(&phba->ndlp_lock);
		}

		/* Two ndlps cannot have the same did on the nodelist */
		ndlp->nlp_DID = keepDID;
		lpfc_drop_node(vport, ndlp);
	}
	else {
		lpfc_unreg_rpi(vport, ndlp);
		/* Two ndlps cannot have the same did */
		ndlp->nlp_DID = keepDID;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
	}
	return new_ndlp;
}

/**
 * lpfc_end_rscn - Check and handle more rscn for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine checks whether more Registration State Change
 * Notifications (RSCNs) came in while the discovery state machine was in
 * the FC_RSCN_MODE. If so, the lpfc_els_handle_rscn() routine will be
 * invoked to handle the additional RSCNs for the @vport. Otherwise, the
 * FC_RSCN_MODE bit will be cleared with the @vport to mark as the end of
 * handling the RSCNs.
 **/
void
lpfc_end_rscn(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (vport->fc_flag & FC_RSCN_MODE) {
		/*
		 * Check to see if more RSCNs came in while we were
		 * processing this one.
		 */
		if (vport->fc_rscn_id_cnt ||
		    (vport->fc_flag & FC_RSCN_DISCOVERY) != 0)
			lpfc_els_handle_rscn(vport);
		else {
			spin_lock_irq(shost->host_lock);
			vport->fc_flag &= ~FC_RSCN_MODE;
			spin_unlock_irq(shost->host_lock);
		}
	}
}

/**
 * lpfc_cmpl_els_plogi - Completion callback function for plogi
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion callback function for issuing the Port
 * Login (PLOGI) command. For PLOGI completion, there must be an active
 * ndlp on the vport node list that matches the remote node ID from the
 * PLOGI reponse IOCB. If such ndlp does not exist, the PLOGI is simply
 * ignored and command IOCB released. The PLOGI response IOCB status is
 * checked for error conditons. If there is error status reported, PLOGI
 * retry shall be attempted by invoking the lpfc_els_retry() routine.
 * Otherwise, the lpfc_plogi_confirm_nport() routine shall be invoked on
 * the ndlp and the NLP_EVT_CMPL_PLOGI state to the Discover State Machine
 * (DSM) is set for this PLOGI completion. Finally, it checks whether
 * there are additional N_Port nodes with the vport that need to perform
 * PLOGI. If so, the lpfc_more_plogi() routine is invoked to issue addition
 * PLOGIs.
 **/
static void
lpfc_cmpl_els_plogi(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp;
	struct lpfc_dmabuf *prsp;
	int disc, rc, did, type;

	/* we pass cmdiocb to state machine which needs rspiocb as well */
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &rspiocb->iocb;
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"PLOGI cmpl:      status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		irsp->un.elsreq64.remoteID);

	ndlp = lpfc_findnode_did(vport, irsp->un.elsreq64.remoteID);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "0136 PLOGI completes to NPort x%x "
				 "with no ndlp. Data: x%x x%x x%x\n",
				 irsp->un.elsreq64.remoteID,
				 irsp->ulpStatus, irsp->un.ulpWord[4],
				 irsp->ulpIoTag);
		goto out;
	}

	/* Since ndlp can be freed in the disc state machine, note if this node
	 * is being used during discovery.
	 */
	spin_lock_irq(shost->host_lock);
	disc = (ndlp->nlp_flag & NLP_NPR_2B_DISC);
	ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;
	spin_unlock_irq(shost->host_lock);
	rc   = 0;

	/* PLOGI completes to NPort <nlp_DID> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0102 PLOGI completes to NPort x%x "
			 "Data: x%x x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, disc, vport->num_disc_nodes);
	/* Check to see if link went down during discovery */
	if (lpfc_els_chk_latt(vport)) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
		goto out;
	}

	/* ndlp could be freed in DSM, save these values now */
	type = ndlp->nlp_type;
	did = ndlp->nlp_DID;

	if (irsp->ulpStatus) {
		/* Check for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			/* ELS command is being retried */
			if (disc) {
				spin_lock_irq(shost->host_lock);
				ndlp->nlp_flag |= NLP_NPR_2B_DISC;
				spin_unlock_irq(shost->host_lock);
			}
			goto out;
		}
		/* PLOGI failed */
		/* Do not call DSM for lpfc_els_abort'ed ELS cmds */
		if (lpfc_error_lost_link(irsp))
			rc = NLP_STE_FREED_NODE;
		else
			rc = lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						     NLP_EVT_CMPL_PLOGI);
	} else {
		/* Good status, call state machine */
		prsp = list_entry(((struct lpfc_dmabuf *)
				   cmdiocb->context2)->list.next,
				  struct lpfc_dmabuf, list);
		ndlp = lpfc_plogi_confirm_nport(phba, prsp->virt, ndlp);
		rc = lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					     NLP_EVT_CMPL_PLOGI);
	}

	if (disc && vport->num_disc_nodes) {
		/* Check to see if there are more PLOGIs to be sent */
		lpfc_more_plogi(vport);

		if (vport->num_disc_nodes == 0) {
			spin_lock_irq(shost->host_lock);
			vport->fc_flag &= ~FC_NDISC_ACTIVE;
			spin_unlock_irq(shost->host_lock);

			lpfc_can_disctmo(vport);
			lpfc_end_rscn(vport);
		}
	}

out:
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_issue_els_plogi - Issue an plogi iocb command for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 * @did: destination port identifier.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues a Port Login (PLOGI) command to a remote N_Port
 * (with the @did) for a @vport. Before issuing a PLOGI to a remote N_Port,
 * the ndlp with the remote N_Port DID must exist on the @vport's ndlp list.
 * This routine constructs the proper feilds of the PLOGI IOCB and invokes
 * the lpfc_sli_issue_iocb() routine to send out PLOGI ELS command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the PLOGI ELS command.
 *
 * Return code
 *   0 - Successfully issued a plogi for @vport
 *   1 - failed to issue a plogi for @vport
 **/
int
lpfc_issue_els_plogi(struct lpfc_vport *vport, uint32_t did, uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	struct serv_parm *sp;
	IOCB_t *icmd;
	struct lpfc_nodelist *ndlp;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int ret;

	psli = &phba->sli;

	ndlp = lpfc_findnode_did(vport, did);
	if (ndlp && !NLP_CHK_NODE_ACT(ndlp))
		ndlp = NULL;

	/* If ndlp is not NULL, we will bump the reference count on it */
	cmdsize = (sizeof(uint32_t) + sizeof(struct serv_parm));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp, did,
				     ELS_CMD_PLOGI);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	/* For PLOGI request, remainder of payload is service parameters */
	*((uint32_t *) (pcmd)) = ELS_CMD_PLOGI;
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_sparam, sizeof(struct serv_parm));
	sp = (struct serv_parm *) pcmd;

	if (sp->cmn.fcphLow < FC_PH_4_3)
		sp->cmn.fcphLow = FC_PH_4_3;

	if (sp->cmn.fcphHigh < FC_PH3)
		sp->cmn.fcphHigh = FC_PH3;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue PLOGI:     did:x%x",
		did, 0, 0);

	phba->fc_stat.elsXmitPLOGI++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_plogi;
	ret = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);

	if (ret == IOCB_ERROR) {
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_cmpl_els_prli - Completion callback function for prli
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion callback function for a Process Login
 * (PRLI) ELS command. The PRLI response IOCB status is checked for error
 * status. If there is error status reported, PRLI retry shall be attempted
 * by invoking the lpfc_els_retry() routine. Otherwise, the state
 * NLP_EVT_CMPL_PRLI is sent to the Discover State Machine (DSM) for this
 * ndlp to mark the PRLI completion.
 **/
static void
lpfc_cmpl_els_prli(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_sli *psli;
	struct lpfc_nodelist *ndlp;

	psli = &phba->sli;
	/* we pass cmdiocb to state machine which needs rspiocb as well */
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &(rspiocb->iocb);
	ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag &= ~NLP_PRLI_SND;
	spin_unlock_irq(shost->host_lock);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"PRLI cmpl:       status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		ndlp->nlp_DID);
	/* PRLI completes to NPort <nlp_DID> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0103 PRLI completes to NPort x%x "
			 "Data: x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, vport->num_disc_nodes);

	vport->fc_prli_sent--;
	/* Check to see if link went down during discovery */
	if (lpfc_els_chk_latt(vport))
		goto out;

	if (irsp->ulpStatus) {
		/* Check for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			/* ELS command is being retried */
			goto out;
		}
		/* PRLI failed */
		/* Do not call DSM for lpfc_els_abort'ed ELS cmds */
		if (lpfc_error_lost_link(irsp))
			goto out;
		else
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_CMPL_PRLI);
	} else
		/* Good status, call state machine */
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_PRLI);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_issue_els_prli - Issue a prli iocb command for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues a Process Login (PRLI) ELS command for the
 * @vport. The PRLI service parameters are set up in the payload of the
 * PRLI Request command and the pointer to lpfc_cmpl_els_prli() routine
 * is put to the IOCB completion callback func field before invoking the
 * routine lpfc_sli_issue_iocb() to send out PRLI command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the PRLI ELS command.
 *
 * Return code
 *   0 - successfully issued prli iocb command for @vport
 *   1 - failed to issue prli iocb command for @vport
 **/
int
lpfc_issue_els_prli(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		    uint8_t retry)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba *phba = vport->phba;
	PRLI *npr;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;

	cmdsize = (sizeof(uint32_t) + sizeof(PRLI));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_PRLI);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	/* For PRLI request, remainder of payload is service parameters */
	memset(pcmd, 0, (sizeof(PRLI) + sizeof(uint32_t)));
	*((uint32_t *) (pcmd)) = ELS_CMD_PRLI;
	pcmd += sizeof(uint32_t);

	/* For PRLI, remainder of payload is PRLI parameter page */
	npr = (PRLI *) pcmd;
	/*
	 * If our firmware version is 3.20 or later,
	 * set the following bits for FC-TAPE support.
	 */
	if (phba->vpd.rev.feaLevelHigh >= 0x02) {
		npr->ConfmComplAllowed = 1;
		npr->Retry = 1;
		npr->TaskRetryIdReq = 1;
	}
	npr->estabImagePair = 1;
	npr->readXferRdyDis = 1;

	/* For FCP support */
	npr->prliType = PRLI_FCP_TYPE;
	npr->initiatorFunc = 1;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue PRLI:      did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitPRLI++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_prli;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_PRLI_SND;
	spin_unlock_irq(shost->host_lock);
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_PRLI_SND;
		spin_unlock_irq(shost->host_lock);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	vport->fc_prli_sent++;
	return 0;
}

/**
 * lpfc_rscn_disc - Perform rscn discovery for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine performs Registration State Change Notification (RSCN)
 * discovery for a @vport. If the @vport's node port recovery count is not
 * zero, it will invoke the lpfc_els_disc_plogi() to perform PLOGI for all
 * the nodes that need recovery. If none of the PLOGI were needed through
 * the lpfc_els_disc_plogi() routine, the lpfc_end_rscn() routine shall be
 * invoked to check and handle possible more RSCN came in during the period
 * of processing the current ones.
 **/
static void
lpfc_rscn_disc(struct lpfc_vport *vport)
{
	lpfc_can_disctmo(vport);

	/* RSCN discovery */
	/* go thru NPR nodes and issue ELS PLOGIs */
	if (vport->fc_npr_cnt)
		if (lpfc_els_disc_plogi(vport))
			return;

	lpfc_end_rscn(vport);
}

/**
 * lpfc_adisc_done - Complete the adisc phase of discovery
 * @vport: pointer to lpfc_vport hba data structure that finished all ADISCs.
 *
 * This function is called when the final ADISC is completed during discovery.
 * This function handles clearing link attention or issuing reg_vpi depending
 * on whether npiv is enabled. This function also kicks off the PLOGI phase of
 * discovery.
 * This function is called with no locks held.
 **/
static void
lpfc_adisc_done(struct lpfc_vport *vport)
{
	struct Scsi_Host   *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba   *phba = vport->phba;

	/*
	 * For NPIV, cmpl_reg_vpi will set port_state to READY,
	 * and continue discovery.
	 */
	if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
	    !(vport->fc_flag & FC_RSCN_MODE) &&
	    (phba->sli_rev < LPFC_SLI_REV4)) {
		lpfc_issue_reg_vpi(phba, vport);
		return;
	}
	/*
	* For SLI2, we need to set port_state to READY
	* and continue discovery.
	*/
	if (vport->port_state < LPFC_VPORT_READY) {
		/* If we get here, there is nothing to ADISC */
		if (vport->port_type == LPFC_PHYSICAL_PORT)
			lpfc_issue_clear_la(phba, vport);
		if (!(vport->fc_flag & FC_ABORT_DISCOVERY)) {
			vport->num_disc_nodes = 0;
			/* go thru NPR list, issue ELS PLOGIs */
			if (vport->fc_npr_cnt)
				lpfc_els_disc_plogi(vport);
			if (!vport->num_disc_nodes) {
				spin_lock_irq(shost->host_lock);
				vport->fc_flag &= ~FC_NDISC_ACTIVE;
				spin_unlock_irq(shost->host_lock);
				lpfc_can_disctmo(vport);
				lpfc_end_rscn(vport);
			}
		}
		vport->port_state = LPFC_VPORT_READY;
	} else
		lpfc_rscn_disc(vport);
}

/**
 * lpfc_more_adisc - Issue more adisc as needed
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine determines whether there are more ndlps on a @vport
 * node list need to have Address Discover (ADISC) issued. If so, it will
 * invoke the lpfc_els_disc_adisc() routine to issue ADISC on the @vport's
 * remaining nodes which need to have ADISC sent.
 **/
void
lpfc_more_adisc(struct lpfc_vport *vport)
{
	int sentadisc;

	if (vport->num_disc_nodes)
		vport->num_disc_nodes--;
	/* Continue discovery with <num_disc_nodes> ADISCs to go */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_DISCOVERY,
			 "0210 Continue discovery with %d ADISCs to go "
			 "Data: x%x x%x x%x\n",
			 vport->num_disc_nodes, vport->fc_adisc_cnt,
			 vport->fc_flag, vport->port_state);
	/* Check to see if there are more ADISCs to be sent */
	if (vport->fc_flag & FC_NLP_MORE) {
		lpfc_set_disctmo(vport);
		/* go thru NPR nodes and issue any remaining ELS ADISCs */
		sentadisc = lpfc_els_disc_adisc(vport);
	}
	if (!vport->num_disc_nodes)
		lpfc_adisc_done(vport);
	return;
}

/**
 * lpfc_cmpl_els_adisc - Completion callback function for adisc
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion function for issuing the Address Discover
 * (ADISC) command. It first checks to see whether link went down during
 * the discovery process. If so, the node will be marked as node port
 * recovery for issuing discover IOCB by the link attention handler and
 * exit. Otherwise, the response status is checked. If error was reported
 * in the response status, the ADISC command shall be retried by invoking
 * the lpfc_els_retry() routine. Otherwise, if no error was reported in
 * the response status, the state machine is invoked to set transition
 * with respect to NLP_EVT_CMPL_ADISC event.
 **/
static void
lpfc_cmpl_els_adisc(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp;
	int  disc;

	/* we pass cmdiocb to state machine which needs rspiocb as well */
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &(rspiocb->iocb);
	ndlp = (struct lpfc_nodelist *) cmdiocb->context1;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"ADISC cmpl:      status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		ndlp->nlp_DID);

	/* Since ndlp can be freed in the disc state machine, note if this node
	 * is being used during discovery.
	 */
	spin_lock_irq(shost->host_lock);
	disc = (ndlp->nlp_flag & NLP_NPR_2B_DISC);
	ndlp->nlp_flag &= ~(NLP_ADISC_SND | NLP_NPR_2B_DISC);
	spin_unlock_irq(shost->host_lock);
	/* ADISC completes to NPort <nlp_DID> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0104 ADISC completes to NPort x%x "
			 "Data: x%x x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, disc, vport->num_disc_nodes);
	/* Check to see if link went down during discovery */
	if (lpfc_els_chk_latt(vport)) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
		goto out;
	}

	if (irsp->ulpStatus) {
		/* Check for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb)) {
			/* ELS command is being retried */
			if (disc) {
				spin_lock_irq(shost->host_lock);
				ndlp->nlp_flag |= NLP_NPR_2B_DISC;
				spin_unlock_irq(shost->host_lock);
				lpfc_set_disctmo(vport);
			}
			goto out;
		}
		/* ADISC failed */
		/* Do not call DSM for lpfc_els_abort'ed ELS cmds */
		if (!lpfc_error_lost_link(irsp))
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_CMPL_ADISC);
	} else
		/* Good status, call state machine */
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_ADISC);

	/* Check to see if there are more ADISCs to be sent */
	if (disc && vport->num_disc_nodes)
		lpfc_more_adisc(vport);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_issue_els_adisc - Issue an address discover iocb to an node on a vport
 * @vport: pointer to a virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues an Address Discover (ADISC) for an @ndlp on a
 * @vport. It prepares the payload of the ADISC ELS command, updates the
 * and states of the ndlp, and invokes the lpfc_sli_issue_iocb() routine
 * to issue the ADISC ELS command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the ADISC ELS command.
 *
 * Return code
 *   0 - successfully issued adisc
 *   1 - failed to issue adisc
 **/
int
lpfc_issue_els_adisc(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		     uint8_t retry)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	ADISC *ap;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;

	cmdsize = (sizeof(uint32_t) + sizeof(ADISC));
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_ADISC);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	/* For ADISC request, remainder of payload is service parameters */
	*((uint32_t *) (pcmd)) = ELS_CMD_ADISC;
	pcmd += sizeof(uint32_t);

	/* Fill in ADISC payload */
	ap = (ADISC *) pcmd;
	ap->hardAL_PA = phba->fc_pref_ALPA;
	memcpy(&ap->portName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&ap->nodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	ap->DID = be32_to_cpu(vport->fc_myDID);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue ADISC:     did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitADISC++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_adisc;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_ADISC_SND;
	spin_unlock_irq(shost->host_lock);
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_ADISC_SND;
		spin_unlock_irq(shost->host_lock);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_cmpl_els_logo - Completion callback function for logo
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion function for issuing the ELS Logout (LOGO)
 * command. If no error status was reported from the LOGO response, the
 * state machine of the associated ndlp shall be invoked for transition with
 * respect to NLP_EVT_CMPL_LOGO event. Otherwise, if error status was reported,
 * the lpfc_els_retry() routine will be invoked to retry the LOGO command.
 **/
static void
lpfc_cmpl_els_logo(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		   struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_vport *vport = ndlp->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_sli *psli;

	psli = &phba->sli;
	/* we pass cmdiocb to state machine which needs rspiocb as well */
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &(rspiocb->iocb);
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag &= ~NLP_LOGO_SND;
	spin_unlock_irq(shost->host_lock);

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"LOGO cmpl:       status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		ndlp->nlp_DID);
	/* LOGO completes to NPort <nlp_DID> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0105 LOGO completes to NPort x%x "
			 "Data: x%x x%x x%x x%x\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, vport->num_disc_nodes);
	/* Check to see if link went down during discovery */
	if (lpfc_els_chk_latt(vport))
		goto out;

	if (ndlp->nlp_flag & NLP_TARGET_REMOVE) {
	        /* NLP_EVT_DEVICE_RM should unregister the RPI
		 * which should abort all outstanding IOs.
		 */
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_DEVICE_RM);
		goto out;
	}

	if (irsp->ulpStatus) {
		/* Check for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb))
			/* ELS command is being retried */
			goto out;
		/* LOGO failed */
		/* Do not call DSM for lpfc_els_abort'ed ELS cmds */
		if (lpfc_error_lost_link(irsp))
			goto out;
		else
			lpfc_disc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_CMPL_LOGO);
	} else
		/* Good status, call state machine.
		 * This will unregister the rpi if needed.
		 */
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_LOGO);
out:
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_issue_els_logo - Issue a logo to an node on a vport
 * @vport: pointer to a virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.
 * @retry: number of retries to the command IOCB.
 *
 * This routine constructs and issues an ELS Logout (LOGO) iocb command
 * to a remote node, referred by an @ndlp on a @vport. It constructs the
 * payload of the IOCB, properly sets up the @ndlp state, and invokes the
 * lpfc_sli_issue_iocb() routine to send out the LOGO ELS command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the LOGO ELS command.
 *
 * Return code
 *   0 - successfully issued logo
 *   1 - failed to issue logo
 **/
int
lpfc_issue_els_logo(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
		    uint8_t retry)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uint8_t *pcmd;
	uint16_t cmdsize;
	int rc;

	spin_lock_irq(shost->host_lock);
	if (ndlp->nlp_flag & NLP_LOGO_SND) {
		spin_unlock_irq(shost->host_lock);
		return 0;
	}
	spin_unlock_irq(shost->host_lock);

	cmdsize = (2 * sizeof(uint32_t)) + sizeof(struct lpfc_name);
	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_LOGO);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMD_LOGO;
	pcmd += sizeof(uint32_t);

	/* Fill in LOGO payload */
	*((uint32_t *) (pcmd)) = be32_to_cpu(vport->fc_myDID);
	pcmd += sizeof(uint32_t);
	memcpy(pcmd, &vport->fc_portname, sizeof(struct lpfc_name));

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue LOGO:      did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitLOGO++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_logo;
	spin_lock_irq(shost->host_lock);
	ndlp->nlp_flag |= NLP_LOGO_SND;
	spin_unlock_irq(shost->host_lock);
	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0);

	if (rc == IOCB_ERROR) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_LOGO_SND;
		spin_unlock_irq(shost->host_lock);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	return 0;
}

/**
 * lpfc_cmpl_els_cmd - Completion callback function for generic els command
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is a generic completion callback function for ELS commands.
 * Specifically, it is the callback function which does not need to perform
 * any command specific operations. It is currently used by the ELS command
 * issuing routines for the ELS State Change  Request (SCR),
 * lpfc_issue_els_scr(), and the ELS Fibre Channel Address Resolution
 * Protocol Response (FARPR) routine, lpfc_issue_els_farpr(). Other than
 * certain debug loggings, this callback function simply invokes the
 * lpfc_els_chk_latt() routine to check whether link went down during the
 * discovery process.
 **/
static void
lpfc_cmpl_els_cmd(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	IOCB_t *irsp;

	irsp = &rspiocb->iocb;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"ELS cmd cmpl:    status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		irsp->un.elsreq64.remoteID);
	/* ELS cmd tag <ulpIoTag> completes */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0106 ELS cmd tag x%x completes Data: x%x x%x x%x\n",
			 irsp->ulpIoTag, irsp->ulpStatus,
			 irsp->un.ulpWord[4], irsp->ulpTimeout);
	/* Check to see if link went down during discovery */
	lpfc_els_chk_latt(vport);
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_issue_els_scr - Issue a scr to an node on a vport
 * @vport: pointer to a host virtual N_Port data structure.
 * @nportid: N_Port identifier to the remote node.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues a State Change Request (SCR) to a fabric node
 * on a @vport. The remote node @nportid is passed into the function. It
 * first search the @vport node list to find the matching ndlp. If no such
 * ndlp is found, a new ndlp shall be created for this (SCR) purpose. An
 * IOCB is allocated, payload prepared, and the lpfc_sli_issue_iocb()
 * routine is invoked to send the SCR IOCB.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the SCR ELS command.
 *
 * Return code
 *   0 - Successfully issued scr command
 *   1 - Failed to issue scr command
 **/
int
lpfc_issue_els_scr(struct lpfc_vport *vport, uint32_t nportid, uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	uint8_t *pcmd;
	uint16_t cmdsize;
	struct lpfc_nodelist *ndlp;

	psli = &phba->sli;
	cmdsize = (sizeof(uint32_t) + sizeof(SCR));

	ndlp = lpfc_findnode_did(vport, nportid);
	if (!ndlp) {
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 1;
		lpfc_nlp_init(vport, ndlp, nportid);
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp)
			return 1;
	}

	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_SCR);

	if (!elsiocb) {
		/* This will trigger the release of the node just
		 * allocated
		 */
		lpfc_nlp_put(ndlp);
		return 1;
	}

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	*((uint32_t *) (pcmd)) = ELS_CMD_SCR;
	pcmd += sizeof(uint32_t);

	/* For SCR, remainder of payload is SCR parameter page */
	memset(pcmd, 0, sizeof(SCR));
	((SCR *) pcmd)->Function = SCR_FUNC_FULL;

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue SCR:       did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitSCR++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_cmd;
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		/* The additional lpfc_nlp_put will cause the following
		 * lpfc_els_free_iocb routine to trigger the rlease of
		 * the node.
		 */
		lpfc_nlp_put(ndlp);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	/* This will cause the callback-function lpfc_cmpl_els_cmd to
	 * trigger the release of node.
	 */
	lpfc_nlp_put(ndlp);
	return 0;
}

/**
 * lpfc_issue_els_farpr - Issue a farp to an node on a vport
 * @vport: pointer to a host virtual N_Port data structure.
 * @nportid: N_Port identifier to the remote node.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues a Fibre Channel Address Resolution Response
 * (FARPR) to a node on a vport. The remote node N_Port identifier (@nportid)
 * is passed into the function. It first search the @vport node list to find
 * the matching ndlp. If no such ndlp is found, a new ndlp shall be created
 * for this (FARPR) purpose. An IOCB is allocated, payload prepared, and the
 * lpfc_sli_issue_iocb() routine is invoked to send the FARPR ELS command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndlp
 * will be incremented by 1 for holding the ndlp and the reference to ndlp
 * will be stored into the context1 field of the IOCB for the completion
 * callback function to the PARPR ELS command.
 *
 * Return code
 *   0 - Successfully issued farpr command
 *   1 - Failed to issue farpr command
 **/
static int
lpfc_issue_els_farpr(struct lpfc_vport *vport, uint32_t nportid, uint8_t retry)
{
	struct lpfc_hba  *phba = vport->phba;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_sli *psli;
	FARP *fp;
	uint8_t *pcmd;
	uint32_t *lp;
	uint16_t cmdsize;
	struct lpfc_nodelist *ondlp;
	struct lpfc_nodelist *ndlp;

	psli = &phba->sli;
	cmdsize = (sizeof(uint32_t) + sizeof(FARP));

	ndlp = lpfc_findnode_did(vport, nportid);
	if (!ndlp) {
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 1;
		lpfc_nlp_init(vport, ndlp, nportid);
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp)
			return 1;
	}

	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp->nlp_DID, ELS_CMD_RNID);
	if (!elsiocb) {
		/* This will trigger the release of the node just
		 * allocated
		 */
		lpfc_nlp_put(ndlp);
		return 1;
	}

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	*((uint32_t *) (pcmd)) = ELS_CMD_FARPR;
	pcmd += sizeof(uint32_t);

	/* Fill in FARPR payload */
	fp = (FARP *) (pcmd);
	memset(fp, 0, sizeof(FARP));
	lp = (uint32_t *) pcmd;
	*lp++ = be32_to_cpu(nportid);
	*lp++ = be32_to_cpu(vport->fc_myDID);
	fp->Rflags = 0;
	fp->Mflags = (FARP_MATCH_PORT | FARP_MATCH_NODE);

	memcpy(&fp->RportName, &vport->fc_portname, sizeof(struct lpfc_name));
	memcpy(&fp->RnodeName, &vport->fc_nodename, sizeof(struct lpfc_name));
	ondlp = lpfc_findnode_did(vport, nportid);
	if (ondlp && NLP_CHK_NODE_ACT(ondlp)) {
		memcpy(&fp->OportName, &ondlp->nlp_portname,
		       sizeof(struct lpfc_name));
		memcpy(&fp->OnodeName, &ondlp->nlp_nodename,
		       sizeof(struct lpfc_name));
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue FARPR:     did:x%x",
		ndlp->nlp_DID, 0, 0);

	phba->fc_stat.elsXmitFARPR++;
	elsiocb->iocb_cmpl = lpfc_cmpl_els_cmd;
	if (lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, elsiocb, 0) ==
	    IOCB_ERROR) {
		/* The additional lpfc_nlp_put will cause the following
		 * lpfc_els_free_iocb routine to trigger the release of
		 * the node.
		 */
		lpfc_nlp_put(ndlp);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	/* This will cause the callback-function lpfc_cmpl_els_cmd to
	 * trigger the release of the node.
	 */
	lpfc_nlp_put(ndlp);
	return 0;
}

/**
 * lpfc_cancel_retry_delay_tmo - Cancel the timer with delayed iocb-cmd retry
 * @vport: pointer to a host virtual N_Port data structure.
 * @nlp: pointer to a node-list data structure.
 *
 * This routine cancels the timer with a delayed IOCB-command retry for
 * a @vport's @ndlp. It stops the timer for the delayed function retrial and
 * removes the ELS retry event if it presents. In addition, if the
 * NLP_NPR_2B_DISC bit is set in the @nlp's nlp_flag bitmap, ADISC IOCB
 * commands are sent for the @vport's nodes that require issuing discovery
 * ADISC.
 **/
void
lpfc_cancel_retry_delay_tmo(struct lpfc_vport *vport, struct lpfc_nodelist *nlp)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_work_evt *evtp;

	if (!(nlp->nlp_flag & NLP_DELAY_TMO))
		return;
	spin_lock_irq(shost->host_lock);
	nlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(shost->host_lock);
	del_timer_sync(&nlp->nlp_delayfunc);
	nlp->nlp_last_elscmd = 0;
	if (!list_empty(&nlp->els_retry_evt.evt_listp)) {
		list_del_init(&nlp->els_retry_evt.evt_listp);
		/* Decrement nlp reference count held for the delayed retry */
		evtp = &nlp->els_retry_evt;
		lpfc_nlp_put((struct lpfc_nodelist *)evtp->evt_arg1);
	}
	if (nlp->nlp_flag & NLP_NPR_2B_DISC) {
		spin_lock_irq(shost->host_lock);
		nlp->nlp_flag &= ~NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
		if (vport->num_disc_nodes) {
			if (vport->port_state < LPFC_VPORT_READY) {
				/* Check if there are more ADISCs to be sent */
				lpfc_more_adisc(vport);
			} else {
				/* Check if there are more PLOGIs to be sent */
				lpfc_more_plogi(vport);
				if (vport->num_disc_nodes == 0) {
					spin_lock_irq(shost->host_lock);
					vport->fc_flag &= ~FC_NDISC_ACTIVE;
					spin_unlock_irq(shost->host_lock);
					lpfc_can_disctmo(vport);
					lpfc_end_rscn(vport);
				}
			}
		}
	}
	return;
}

/**
 * lpfc_els_retry_delay - Timer function with a ndlp delayed function timer
 * @ptr: holder for the pointer to the timer function associated data (ndlp).
 *
 * This routine is invoked by the ndlp delayed-function timer to check
 * whether there is any pending ELS retry event(s) with the node. If not, it
 * simply returns. Otherwise, if there is at least one ELS delayed event, it
 * adds the delayed events to the HBA work list and invokes the
 * lpfc_worker_wake_up() routine to wake up worker thread to process the
 * event. Note that lpfc_nlp_get() is called before posting the event to
 * the work list to hold reference count of ndlp so that it guarantees the
 * reference to ndlp will still be available when the worker thread gets
 * to the event associated with the ndlp.
 **/
void
lpfc_els_retry_delay(unsigned long ptr)
{
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) ptr;
	struct lpfc_vport *vport = ndlp->vport;
	struct lpfc_hba   *phba = vport->phba;
	unsigned long flags;
	struct lpfc_work_evt  *evtp = &ndlp->els_retry_evt;

	spin_lock_irqsave(&phba->hbalock, flags);
	if (!list_empty(&evtp->evt_listp)) {
		spin_unlock_irqrestore(&phba->hbalock, flags);
		return;
	}

	/* We need to hold the node by incrementing the reference
	 * count until the queued work is done
	 */
	evtp->evt_arg1  = lpfc_nlp_get(ndlp);
	if (evtp->evt_arg1) {
		evtp->evt = LPFC_EVT_ELS_RETRY;
		list_add_tail(&evtp->evt_listp, &phba->work_list);
		lpfc_worker_wake_up(phba);
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	return;
}

/**
 * lpfc_els_retry_delay_handler - Work thread handler for ndlp delayed function
 * @ndlp: pointer to a node-list data structure.
 *
 * This routine is the worker-thread handler for processing the @ndlp delayed
 * event(s), posted by the lpfc_els_retry_delay() routine. It simply retrieves
 * the last ELS command from the associated ndlp and invokes the proper ELS
 * function according to the delayed ELS command to retry the command.
 **/
void
lpfc_els_retry_delay_handler(struct lpfc_nodelist *ndlp)
{
	struct lpfc_vport *vport = ndlp->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	uint32_t cmd, did, retry;

	spin_lock_irq(shost->host_lock);
	did = ndlp->nlp_DID;
	cmd = ndlp->nlp_last_elscmd;
	ndlp->nlp_last_elscmd = 0;

	if (!(ndlp->nlp_flag & NLP_DELAY_TMO)) {
		spin_unlock_irq(shost->host_lock);
		return;
	}

	ndlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(shost->host_lock);
	/*
	 * If a discovery event readded nlp_delayfunc after timer
	 * firing and before processing the timer, cancel the
	 * nlp_delayfunc.
	 */
	del_timer_sync(&ndlp->nlp_delayfunc);
	retry = ndlp->nlp_retry;

	switch (cmd) {
	case ELS_CMD_FLOGI:
		lpfc_issue_els_flogi(vport, ndlp, retry);
		break;
	case ELS_CMD_PLOGI:
		if (!lpfc_issue_els_plogi(vport, ndlp->nlp_DID, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PLOGI_ISSUE);
		}
		break;
	case ELS_CMD_ADISC:
		if (!lpfc_issue_els_adisc(vport, ndlp, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_ADISC_ISSUE);
		}
		break;
	case ELS_CMD_PRLI:
		if (!lpfc_issue_els_prli(vport, ndlp, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PRLI_ISSUE);
		}
		break;
	case ELS_CMD_LOGO:
		if (!lpfc_issue_els_logo(vport, ndlp, retry)) {
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		}
		break;
	case ELS_CMD_FDISC:
		lpfc_issue_els_fdisc(vport, ndlp, retry);
		break;
	}
	return;
}

/**
 * lpfc_els_retry - Make retry decision on an els command iocb
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine makes a retry decision on an ELS command IOCB, which has
 * failed. The following ELS IOCBs use this function for retrying the command
 * when previously issued command responsed with error status: FLOGI, PLOGI,
 * PRLI, ADISC, LOGO, and FDISC. Based on the ELS command type and the
 * returned error status, it makes the decision whether a retry shall be
 * issued for the command, and whether a retry shall be made immediately or
 * delayed. In the former case, the corresponding ELS command issuing-function
 * is called to retry the command. In the later case, the ELS command shall
 * be posted to the ndlp delayed event and delayed function timer set to the
 * ndlp for the delayed command issusing.
 *
 * Return code
 *   0 - No retry of els command is made
 *   1 - Immediate or delayed retry of els command is made
 **/
static int
lpfc_els_retry(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
	       struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp = &rspiocb->iocb;
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_dmabuf *pcmd = (struct lpfc_dmabuf *) cmdiocb->context2;
	uint32_t *elscmd;
	struct ls_rjt stat;
	int retry = 0, maxretry = lpfc_max_els_tries, delay = 0;
	int logerr = 0;
	uint32_t cmd = 0;
	uint32_t did;


	/* Note: context2 may be 0 for internal driver abort
	 * of delays ELS command.
	 */

	if (pcmd && pcmd->virt) {
		elscmd = (uint32_t *) (pcmd->virt);
		cmd = *elscmd++;
	}

	if (ndlp && NLP_CHK_NODE_ACT(ndlp))
		did = ndlp->nlp_DID;
	else {
		/* We should only hit this case for retrying PLOGI */
		did = irsp->un.elsreq64.remoteID;
		ndlp = lpfc_findnode_did(vport, did);
		if ((!ndlp || !NLP_CHK_NODE_ACT(ndlp))
		    && (cmd != ELS_CMD_PLOGI))
			return 1;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Retry ELS:       wd7:x%x wd4:x%x did:x%x",
		*(((uint32_t *) irsp) + 7), irsp->un.ulpWord[4], ndlp->nlp_DID);

	switch (irsp->ulpStatus) {
	case IOSTAT_FCP_RSP_ERROR:
	case IOSTAT_REMOTE_STOP:
		break;

	case IOSTAT_LOCAL_REJECT:
		switch ((irsp->un.ulpWord[4] & 0xff)) {
		case IOERR_LOOP_OPEN_FAILURE:
			if (cmd == ELS_CMD_FLOGI) {
				if (PCI_DEVICE_ID_HORNET ==
					phba->pcidev->device) {
					phba->fc_topology = TOPOLOGY_LOOP;
					phba->pport->fc_myDID = 0;
					phba->alpa_map[0] = 0;
					phba->alpa_map[1] = 0;
				}
			}
			if (cmd == ELS_CMD_PLOGI && cmdiocb->retry == 0)
				delay = 1000;
			retry = 1;
			break;

		case IOERR_ILLEGAL_COMMAND:
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
					 "0124 Retry illegal cmd x%x "
					 "retry:x%x delay:x%x\n",
					 cmd, cmdiocb->retry, delay);
			retry = 1;
			/* All command's retry policy */
			maxretry = 8;
			if (cmdiocb->retry > 2)
				delay = 1000;
			break;

		case IOERR_NO_RESOURCES:
			logerr = 1; /* HBA out of resources */
			retry = 1;
			if (cmdiocb->retry > 100)
				delay = 100;
			maxretry = 250;
			break;

		case IOERR_ILLEGAL_FRAME:
			delay = 100;
			retry = 1;
			break;

		case IOERR_SEQUENCE_TIMEOUT:
		case IOERR_INVALID_RPI:
			retry = 1;
			break;
		}
		break;

	case IOSTAT_NPORT_RJT:
	case IOSTAT_FABRIC_RJT:
		if (irsp->un.ulpWord[4] & RJT_UNAVAIL_TEMP) {
			retry = 1;
			break;
		}
		break;

	case IOSTAT_NPORT_BSY:
	case IOSTAT_FABRIC_BSY:
		logerr = 1; /* Fabric / Remote NPort out of resources */
		retry = 1;
		break;

	case IOSTAT_LS_RJT:
		stat.un.lsRjtError = be32_to_cpu(irsp->un.ulpWord[4]);
		/* Added for Vendor specifc support
		 * Just keep retrying for these Rsn / Exp codes
		 */
		switch (stat.un.b.lsRjtRsnCode) {
		case LSRJT_UNABLE_TPC:
			if (stat.un.b.lsRjtRsnCodeExp ==
			    LSEXP_CMD_IN_PROGRESS) {
				if (cmd == ELS_CMD_PLOGI) {
					delay = 1000;
					maxretry = 48;
				}
				retry = 1;
				break;
			}
			if (cmd == ELS_CMD_PLOGI) {
				delay = 1000;
				maxretry = lpfc_max_els_tries + 1;
				retry = 1;
				break;
			}
			if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
			  (cmd == ELS_CMD_FDISC) &&
			  (stat.un.b.lsRjtRsnCodeExp == LSEXP_OUT_OF_RESOURCE)){
				lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
						 "0125 FDISC Failed (x%x). "
						 "Fabric out of resources\n",
						 stat.un.lsRjtError);
				lpfc_vport_set_state(vport,
						     FC_VPORT_NO_FABRIC_RSCS);
			}
			break;

		case LSRJT_LOGICAL_BSY:
			if ((cmd == ELS_CMD_PLOGI) ||
			    (cmd == ELS_CMD_PRLI)) {
				delay = 1000;
				maxretry = 48;
			} else if (cmd == ELS_CMD_FDISC) {
				/* FDISC retry policy */
				maxretry = 48;
				if (cmdiocb->retry >= 32)
					delay = 1000;
			}
			retry = 1;
			break;

		case LSRJT_LOGICAL_ERR:
			/* There are some cases where switches return this
			 * error when they are not ready and should be returning
			 * Logical Busy. We should delay every time.
			 */
			if (cmd == ELS_CMD_FDISC &&
			    stat.un.b.lsRjtRsnCodeExp == LSEXP_PORT_LOGIN_REQ) {
				maxretry = 3;
				delay = 1000;
				retry = 1;
				break;
			}
		case LSRJT_PROTOCOL_ERR:
			if ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
			  (cmd == ELS_CMD_FDISC) &&
			  ((stat.un.b.lsRjtRsnCodeExp == LSEXP_INVALID_PNAME) ||
			  (stat.un.b.lsRjtRsnCodeExp == LSEXP_INVALID_NPORT_ID))
			  ) {
				lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
						 "0122 FDISC Failed (x%x). "
						 "Fabric Detected Bad WWN\n",
						 stat.un.lsRjtError);
				lpfc_vport_set_state(vport,
						     FC_VPORT_FABRIC_REJ_WWN);
			}
			break;
		}
		break;

	case IOSTAT_INTERMED_RSP:
	case IOSTAT_BA_RJT:
		break;

	default:
		break;
	}

	if (did == FDMI_DID)
		retry = 1;

	if ((cmd == ELS_CMD_FLOGI) &&
	    (phba->fc_topology != TOPOLOGY_LOOP) &&
	    !lpfc_error_lost_link(irsp)) {
		/* FLOGI retry policy */
		retry = 1;
		maxretry = 48;
		if (cmdiocb->retry >= 32)
			delay = 1000;
	}

	if ((++cmdiocb->retry) >= maxretry) {
		phba->fc_stat.elsRetryExceeded++;
		retry = 0;
	}

	if ((vport->load_flag & FC_UNLOADING) != 0)
		retry = 0;

	if (retry) {

		/* Retry ELS command <elsCmd> to remote NPORT <did> */
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0107 Retry ELS command x%x to remote "
				 "NPORT x%x Data: x%x x%x\n",
				 cmd, did, cmdiocb->retry, delay);

		if (((cmd == ELS_CMD_PLOGI) || (cmd == ELS_CMD_ADISC)) &&
			((irsp->ulpStatus != IOSTAT_LOCAL_REJECT) ||
			((irsp->un.ulpWord[4] & 0xff) != IOERR_NO_RESOURCES))) {
			/* Don't reset timer for no resources */

			/* If discovery / RSCN timer is running, reset it */
			if (timer_pending(&vport->fc_disctmo) ||
			    (vport->fc_flag & FC_RSCN_MODE))
				lpfc_set_disctmo(vport);
		}

		phba->fc_stat.elsXmitRetry++;
		if (ndlp && NLP_CHK_NODE_ACT(ndlp) && delay) {
			phba->fc_stat.elsDelayRetry++;
			ndlp->nlp_retry = cmdiocb->retry;

			/* delay is specified in milliseconds */
			mod_timer(&ndlp->nlp_delayfunc,
				jiffies + msecs_to_jiffies(delay));
			spin_lock_irq(shost->host_lock);
			ndlp->nlp_flag |= NLP_DELAY_TMO;
			spin_unlock_irq(shost->host_lock);

			ndlp->nlp_prev_state = ndlp->nlp_state;
			if (cmd == ELS_CMD_PRLI)
				lpfc_nlp_set_state(vport, ndlp,
					NLP_STE_REG_LOGIN_ISSUE);
			else
				lpfc_nlp_set_state(vport, ndlp,
					NLP_STE_NPR_NODE);
			ndlp->nlp_last_elscmd = cmd;

			return 1;
		}
		switch (cmd) {
		case ELS_CMD_FLOGI:
			lpfc_issue_els_flogi(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_FDISC:
			lpfc_issue_els_fdisc(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_PLOGI:
			if (ndlp && NLP_CHK_NODE_ACT(ndlp)) {
				ndlp->nlp_prev_state = ndlp->nlp_state;
				lpfc_nlp_set_state(vport, ndlp,
						   NLP_STE_PLOGI_ISSUE);
			}
			lpfc_issue_els_plogi(vport, did, cmdiocb->retry);
			return 1;
		case ELS_CMD_ADISC:
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_ADISC_ISSUE);
			lpfc_issue_els_adisc(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_PRLI:
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_PRLI_ISSUE);
			lpfc_issue_els_prli(vport, ndlp, cmdiocb->retry);
			return 1;
		case ELS_CMD_LOGO:
			ndlp->nlp_prev_state = ndlp->nlp_state;
			lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
			lpfc_issue_els_logo(vport, ndlp, cmdiocb->retry);
			return 1;
		}
	}
	/* No retry ELS command <elsCmd> to remote NPORT <did> */
	if (logerr) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
			 "0137 No retry ELS command x%x to remote "
			 "NPORT x%x: Out of Resources: Error:x%x/%x\n",
			 cmd, did, irsp->ulpStatus,
			 irsp->un.ulpWord[4]);
	}
	else {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0108 No retry ELS command x%x to remote "
			 "NPORT x%x Retried:%d Error:x%x/%x\n",
			 cmd, did, cmdiocb->retry, irsp->ulpStatus,
			 irsp->un.ulpWord[4]);
	}
	return 0;
}

/**
 * lpfc_els_free_data - Free lpfc dma buffer and data structure with an iocb
 * @phba: pointer to lpfc hba data structure.
 * @buf_ptr1: pointer to the lpfc DMA buffer data structure.
 *
 * This routine releases the lpfc DMA (Direct Memory Access) buffer(s)
 * associated with a command IOCB back to the lpfc DMA buffer pool. It first
 * checks to see whether there is a lpfc DMA buffer associated with the
 * response of the command IOCB. If so, it will be released before releasing
 * the lpfc DMA buffer associated with the IOCB itself.
 *
 * Return code
 *   0 - Successfully released lpfc DMA buffer (currently, always return 0)
 **/
static int
lpfc_els_free_data(struct lpfc_hba *phba, struct lpfc_dmabuf *buf_ptr1)
{
	struct lpfc_dmabuf *buf_ptr;

	/* Free the response before processing the command. */
	if (!list_empty(&buf_ptr1->list)) {
		list_remove_head(&buf_ptr1->list, buf_ptr,
				 struct lpfc_dmabuf,
				 list);
		lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
		kfree(buf_ptr);
	}
	lpfc_mbuf_free(phba, buf_ptr1->virt, buf_ptr1->phys);
	kfree(buf_ptr1);
	return 0;
}

/**
 * lpfc_els_free_bpl - Free lpfc dma buffer and data structure with bpl
 * @phba: pointer to lpfc hba data structure.
 * @buf_ptr: pointer to the lpfc dma buffer data structure.
 *
 * This routine releases the lpfc Direct Memory Access (DMA) buffer
 * associated with a Buffer Pointer List (BPL) back to the lpfc DMA buffer
 * pool.
 *
 * Return code
 *   0 - Successfully released lpfc DMA buffer (currently, always return 0)
 **/
static int
lpfc_els_free_bpl(struct lpfc_hba *phba, struct lpfc_dmabuf *buf_ptr)
{
	lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
	kfree(buf_ptr);
	return 0;
}

/**
 * lpfc_els_free_iocb - Free a command iocb and its associated resources
 * @phba: pointer to lpfc hba data structure.
 * @elsiocb: pointer to lpfc els command iocb data structure.
 *
 * This routine frees a command IOCB and its associated resources. The
 * command IOCB data structure contains the reference to various associated
 * resources, these fields must be set to NULL if the associated reference
 * not present:
 *   context1 - reference to ndlp
 *   context2 - reference to cmd
 *   context2->next - reference to rsp
 *   context3 - reference to bpl
 *
 * It first properly decrements the reference count held on ndlp for the
 * IOCB completion callback function. If LPFC_DELAY_MEM_FREE flag is not
 * set, it invokes the lpfc_els_free_data() routine to release the Direct
 * Memory Access (DMA) buffers associated with the IOCB. Otherwise, it
 * adds the DMA buffer the @phba data structure for the delayed release.
 * If reference to the Buffer Pointer List (BPL) is present, the
 * lpfc_els_free_bpl() routine is invoked to release the DMA memory
 * associated with BPL. Finally, the lpfc_sli_release_iocbq() routine is
 * invoked to release the IOCB data structure back to @phba IOCBQ list.
 *
 * Return code
 *   0 - Success (currently, always return 0)
 **/
int
lpfc_els_free_iocb(struct lpfc_hba *phba, struct lpfc_iocbq *elsiocb)
{
	struct lpfc_dmabuf *buf_ptr, *buf_ptr1;
	struct lpfc_nodelist *ndlp;

	ndlp = (struct lpfc_nodelist *)elsiocb->context1;
	if (ndlp) {
		if (ndlp->nlp_flag & NLP_DEFER_RM) {
			lpfc_nlp_put(ndlp);

			/* If the ndlp is not being used by another discovery
			 * thread, free it.
			 */
			if (!lpfc_nlp_not_used(ndlp)) {
				/* If ndlp is being used by another discovery
				 * thread, just clear NLP_DEFER_RM
				 */
				ndlp->nlp_flag &= ~NLP_DEFER_RM;
			}
		}
		else
			lpfc_nlp_put(ndlp);
		elsiocb->context1 = NULL;
	}
	/* context2  = cmd,  context2->next = rsp, context3 = bpl */
	if (elsiocb->context2) {
		if (elsiocb->iocb_flag & LPFC_DELAY_MEM_FREE) {
			/* Firmware could still be in progress of DMAing
			 * payload, so don't free data buffer till after
			 * a hbeat.
			 */
			elsiocb->iocb_flag &= ~LPFC_DELAY_MEM_FREE;
			buf_ptr = elsiocb->context2;
			elsiocb->context2 = NULL;
			if (buf_ptr) {
				buf_ptr1 = NULL;
				spin_lock_irq(&phba->hbalock);
				if (!list_empty(&buf_ptr->list)) {
					list_remove_head(&buf_ptr->list,
						buf_ptr1, struct lpfc_dmabuf,
						list);
					INIT_LIST_HEAD(&buf_ptr1->list);
					list_add_tail(&buf_ptr1->list,
						&phba->elsbuf);
					phba->elsbuf_cnt++;
				}
				INIT_LIST_HEAD(&buf_ptr->list);
				list_add_tail(&buf_ptr->list, &phba->elsbuf);
				phba->elsbuf_cnt++;
				spin_unlock_irq(&phba->hbalock);
			}
		} else {
			buf_ptr1 = (struct lpfc_dmabuf *) elsiocb->context2;
			lpfc_els_free_data(phba, buf_ptr1);
		}
	}

	if (elsiocb->context3) {
		buf_ptr = (struct lpfc_dmabuf *) elsiocb->context3;
		lpfc_els_free_bpl(phba, buf_ptr);
	}
	lpfc_sli_release_iocbq(phba, elsiocb);
	return 0;
}

/**
 * lpfc_cmpl_els_logo_acc - Completion callback function to logo acc response
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion callback function to the Logout (LOGO)
 * Accept (ACC) Response ELS command. This routine is invoked to indicate
 * the completion of the LOGO process. It invokes the lpfc_nlp_not_used() to
 * release the ndlp if it has the last reference remaining (reference count
 * is 1). If succeeded (meaning ndlp released), it sets the IOCB context1
 * field to NULL to inform the following lpfc_els_free_iocb() routine no
 * ndlp reference count needs to be decremented. Otherwise, the ndlp
 * reference use-count shall be decremented by the lpfc_els_free_iocb()
 * routine. Finally, the lpfc_els_free_iocb() is invoked to release the
 * IOCB data structure.
 **/
static void
lpfc_cmpl_els_logo_acc(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		       struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_vport *vport = cmdiocb->vport;
	IOCB_t *irsp;

	irsp = &rspiocb->iocb;
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"ACC LOGO cmpl:   status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4], ndlp->nlp_DID);
	/* ACC to LOGO completes to NPort <nlp_DID> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0109 ACC to LOGO completes to NPort x%x "
			 "Data: x%x x%x x%x\n",
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);

	if (ndlp->nlp_state == NLP_STE_NPR_NODE) {
		/* NPort Recovery mode or node is just allocated */
		if (!lpfc_nlp_not_used(ndlp)) {
			/* If the ndlp is being used by another discovery
			 * thread, just unregister the RPI.
			 */
			lpfc_unreg_rpi(vport, ndlp);
		} else {
			/* Indicate the node has already released, should
			 * not reference to it from within lpfc_els_free_iocb.
			 */
			cmdiocb->context1 = NULL;
		}
	}
	lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_mbx_cmpl_dflt_rpi - Completion callbk func for unreg dflt rpi mbox cmd
 * @phba: pointer to lpfc hba data structure.
 * @pmb: pointer to the driver internal queue element for mailbox command.
 *
 * This routine is the completion callback function for unregister default
 * RPI (Remote Port Index) mailbox command to the @phba. It simply releases
 * the associated lpfc Direct Memory Access (DMA) buffer back to the pool and
 * decrements the ndlp reference count held for this completion callback
 * function. After that, it invokes the lpfc_nlp_not_used() to check
 * whether there is only one reference left on the ndlp. If so, it will
 * perform one more decrement and trigger the release of the ndlp.
 **/
void
lpfc_mbx_cmpl_dflt_rpi(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_dmabuf *mp = (struct lpfc_dmabuf *) (pmb->context1);
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) pmb->context2;

	/*
	 * This routine is used to register and unregister in previous SLI
	 * modes.
	 */
	if ((pmb->u.mb.mbxCommand == MBX_UNREG_LOGIN) &&
	    (phba->sli_rev == LPFC_SLI_REV4))
		lpfc_sli4_free_rpi(phba, pmb->u.mb.un.varUnregLogin.rpi);

	pmb->context1 = NULL;
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mempool_free(pmb, phba->mbox_mem_pool);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)) {
		lpfc_nlp_put(ndlp);
		/* This is the end of the default RPI cleanup logic for this
		 * ndlp. If no other discovery threads are using this ndlp.
		 * we should free all resources associated with it.
		 */
		lpfc_nlp_not_used(ndlp);
	}

	return;
}

/**
 * lpfc_cmpl_els_rsp - Completion callback function for els response iocb cmd
 * @phba: pointer to lpfc hba data structure.
 * @cmdiocb: pointer to lpfc command iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion callback function for ELS Response IOCB
 * command. In normal case, this callback function just properly sets the
 * nlp_flag bitmap in the ndlp data structure, if the mbox command reference
 * field in the command IOCB is not NULL, the referred mailbox command will
 * be send out, and then invokes the lpfc_els_free_iocb() routine to release
 * the IOCB. Under error conditions, such as when a LS_RJT is returned or a
 * link down event occurred during the discovery, the lpfc_nlp_not_used()
 * routine shall be invoked trying to release the ndlp if no other threads
 * are currently referring it.
 **/
static void
lpfc_cmpl_els_rsp(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		  struct lpfc_iocbq *rspiocb)
{
	struct lpfc_nodelist *ndlp = (struct lpfc_nodelist *) cmdiocb->context1;
	struct lpfc_vport *vport = ndlp ? ndlp->vport : NULL;
	struct Scsi_Host  *shost = vport ? lpfc_shost_from_vport(vport) : NULL;
	IOCB_t  *irsp;
	uint8_t *pcmd;
	LPFC_MBOXQ_t *mbox = NULL;
	struct lpfc_dmabuf *mp = NULL;
	uint32_t ls_rjt = 0;

	irsp = &rspiocb->iocb;

	if (cmdiocb->context_un.mbox)
		mbox = cmdiocb->context_un.mbox;

	/* First determine if this is a LS_RJT cmpl. Note, this callback
	 * function can have cmdiocb->contest1 (ndlp) field set to NULL.
	 */
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) cmdiocb->context2)->virt);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
	    (*((uint32_t *) (pcmd)) == ELS_CMD_LS_RJT)) {
		/* A LS_RJT associated with Default RPI cleanup has its own
		 * seperate code path.
		 */
		if (!(ndlp->nlp_flag & NLP_RM_DFLT_RPI))
			ls_rjt = 1;
	}

	/* Check to see if link went down during discovery */
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp) || lpfc_els_chk_latt(vport)) {
		if (mbox) {
			mp = (struct lpfc_dmabuf *) mbox->context1;
			if (mp) {
				lpfc_mbuf_free(phba, mp->virt, mp->phys);
				kfree(mp);
			}
			mempool_free(mbox, phba->mbox_mem_pool);
		}
		if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
		    (ndlp->nlp_flag & NLP_RM_DFLT_RPI))
			if (lpfc_nlp_not_used(ndlp)) {
				ndlp = NULL;
				/* Indicate the node has already released,
				 * should not reference to it from within
				 * the routine lpfc_els_free_iocb.
				 */
				cmdiocb->context1 = NULL;
			}
		goto out;
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"ELS rsp cmpl:    status:x%x/x%x did:x%x",
		irsp->ulpStatus, irsp->un.ulpWord[4],
		cmdiocb->iocb.un.elsreq64.remoteID);
	/* ELS response tag <ulpIoTag> completes */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0110 ELS response tag x%x completes "
			 "Data: x%x x%x x%x x%x x%x x%x x%x\n",
			 cmdiocb->iocb.ulpIoTag, rspiocb->iocb.ulpStatus,
			 rspiocb->iocb.un.ulpWord[4], rspiocb->iocb.ulpTimeout,
			 ndlp->nlp_DID, ndlp->nlp_flag, ndlp->nlp_state,
			 ndlp->nlp_rpi);
	if (mbox) {
		if ((rspiocb->iocb.ulpStatus == 0)
		    && (ndlp->nlp_flag & NLP_ACC_REGLOGIN)) {
			lpfc_unreg_rpi(vport, ndlp);
			/* Increment reference count to ndlp to hold the
			 * reference to ndlp for the callback function.
			 */
			mbox->context2 = lpfc_nlp_get(ndlp);
			mbox->vport = vport;
			if (ndlp->nlp_flag & NLP_RM_DFLT_RPI) {
				mbox->mbox_flag |= LPFC_MBX_IMED_UNREG;
				mbox->mbox_cmpl = lpfc_mbx_cmpl_dflt_rpi;
			}
			else {
				mbox->mbox_cmpl = lpfc_mbx_cmpl_reg_login;
				ndlp->nlp_prev_state = ndlp->nlp_state;
				lpfc_nlp_set_state(vport, ndlp,
					   NLP_STE_REG_LOGIN_ISSUE);
			}
			if (lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT)
			    != MBX_NOT_FINISHED)
				goto out;
			else
				/* Decrement the ndlp reference count we
				 * set for this failed mailbox command.
				 */
				lpfc_nlp_put(ndlp);

			/* ELS rsp: Cannot issue reg_login for <NPortid> */
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				"0138 ELS rsp: Cannot issue reg_login for x%x "
	/***Data:***********
\n",*****ndlp->nlp_DID, is part offlage Emulex Linstate file is part ofrpi);
****if (lpfc_t ofnot_used(is p)) {file is p = NULL;*****/* Indicate node has already been released file  * should ****reference to it from withinghts resthe routine t Busels_free_iocb.ghts re/*****cmd    ->context1            }
		} else      /* Do     dropht (C)****lex.     abort'ed*****cmds    *
  Hos!lex.  rror_lost_link(irsp) &&****    Emulex Linux D & NLP_ACC_REGLOGIN         Host Bus Adapters.                                 **
 * Copyright (C) 2004-2009 Emuleware;  *x.  All riserved.           *
 *
 * modi EMULEX and SLI aemarks of Em*
 * modilex.                         *
 ** www.emulex.com                         
		m    (structnse asdmabuf *) mboxulex.com       Hosmp       t Busmbuf     (phba, mp->virtIONS, phys)     kED COmpS ANDdistrempoolIED COwill, NDIT->will_mem_TIESS AN}
out:

 * A     &      CHK_NODE_ACT            spin_lock_irq(shost->N-INPOSE,S AND                = ~(                 |     RM_DFLT_RPIS AND PURPunOSE, OR NON-INFRINGEMENT, AR
; you femarkt (C)is     being s.   by another discovery th2009 fil * and we are sendl Pua reject,y of whidoned SLI it     * R  All  driver       *
 *count here copy     associated
 * inresources*
 * i  *
 Hosts_rjt)nnel Host Bus Adapters.             *
 * Copyright (C) 2004-2009 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com            }

SS OR              ONDITIO www.emS ANreturn;
}

/**
cense as pubrsp_acc - Prep whicopy******an i_trresponse      commandincl@vport: pointerthe a N-IN REPRual N_Port data d in ture.pfc_hux D:emarkelsclude "l c (C)to bee "leptedlude "olww.em#include "lpfmarkoriginalnse anclude "l
#incc_sli4.h"
#include "is p#include "lpfc_t (C-lispfc_sli4.h"
#include "willh"
#include "lpfcith thilude inclqueuh"
#ement *
 *mailboxclude "lludeinclThisks of Emupport_fsc.h>

#inc;
st A#incl (ACC)pfc_hw4.h"IOCBincl_iocbq * Itublis"lpfc"lpfcdiscproperly set up"lpfcruct field *
 *theinclspecific ACCpfc_hw4.h"clude "lisc.h"*****dc.h>

nvokt lpfcinclude <sli_*****vice.h)ks of Emutoich c outbort_vport(strruct.D.  adebugfs.hinclude "is paslic in,MULEwillc.h"putretr "lpfcex.com _un.willinclstatico  See  *);
svoid lructpletion callback funciocb) EMU*****t lpfc_uct lpfc_iocbq de "lpfcHBA lade "wheb);
static isstatic  *,
			  Noter_nat, innse asocbqi/scs(struct lpfc_v,emarks package.      of Emulincl lpfc_isincrhba *ic Lic1ct lphc_scng See t    pfc_hlpfc_max_els_to3;

/**
 * lpfc_estorstatibric_iocb(stru1 *phba,
				  struct lpfc_iocbq *iocb lpfc
static void lpfc_refc_iocrfc_hw4 linkfc_hw4.h"**** *);
_iocbq *,
			  Rcsi_hlpfc_incl  0 - Successfullyt *vport)"lpfc_hw4.hry pro1 - Failec_hba
#incluis done
 * by */
int
ude <scsi/scsi_ted in the hopw.h"
 *w.h"
, uint32_t ux De
 * d in the hop    q *c_scsi., @vport's dist (Ct.h"
*entithis LPFC_MBOXQ_t *will)
{
	d in thScsi_Hli.h*ON-IN =_issue_INGEMX anink at(w.h"
S ANd in the hophba  *NDIT = w.h"
TATIba;
	ruct_DISicmdleared statold,
 * a@vport's discovery els    de shall indicasli *psli;
	vent8_DISp,
 * avent16_tC) 20ize;
	int rc;
	ELS_PKTether_pkt_ptr INV eve = &ANY IMevent
urn coFC_Lc_scsi.-> the h
	switch (ux D     ced wthe CMD    :
ware that = thatof(vents duS ANDher theear shalelist *ndlp);
ntion e0ncludthat, state in retry file      tentie Emulex Lin the for checkinS AND Chriher the        PURPOSE, OR NON-INFRINGEMENT, ARE       *
 * DISCLAIME    LOGO         IMERS ARE HELD *
 * TO BE LEGALLY I		/scsi_h l. *
ude 	e,
 FC_Lher then LPFC_V{
	str->ulpCx.com  =o ho lpfc_shost_fro;    Xri******ppenbute(ed in the hope that it Scsi_Hostex.com 2), REPRt lpf*(n
 * even it (ppen)) =
 * Return cost phba +attention
 * event w
ESS OR debugfs_  *
_trcde shalls FC_DISC_TRC_the RSP file"I*****kingon ev  did:****flgion s fileis part of the Emulex Linux Dev0t lpfbreak;equest for checP    g host link at(tentiond in thserv_parm) +ttention
 * eventt will be ignored and a return code shall indicate no host link
 * attention event had happened.
 *
 * Return codes
 *   0 - no hlpfc_vport *vp
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba  *phba = vport->phba;
	uint32_t ha_copy;

	if (vport->port_st *
 * ALOVERY	ill be ig
	if (vpoct lpfc_ = willd cale >= LPFC_VPORT_READY ||
	    phba->link_state == LPFC_LINK_DOWN |ARRANcpy_READ, &host littenparam,ttention Link Event durinN ||
	    phba->sli_rev > LPFC_SLI_REV3)
		return 0;

	/* Read the HBA  ))
		rtention Register */
	ha_copy = readl(phba->HAregaddr);

	if (!(ha_copy & HA_LATT)RLOg host link attention
 * eventg Discover andt will be ignored and a return code shall indicate no host link
 * attention event had happened.
 *
 * Returvport: po	 phba->pport->port_state);

	/* CLEAR_LA should re-enable link attention events and
	 * we s ould then imediately take a LATT event. The
	 * LATT processing should calost_lock);
	vport->phba;
	uint32_t hastate in if (vport->portthis is usec iocb data structure
 * @vport_state >= LPFC_VPORT_READY ||
	    phb andba->linklink is in 	/* the host ) ppened.t and prepar->un.prlo.
#inclRspC (C)= CB frREQ_EXECUTEDnlock_irq(shost->host_lock);

	if (phba->link_state != LPFC_CLEAR_LA)
		and ttention Register */
	ha_copy = readl(phba->HAregaddr);

	if (!(ha_cdefaulCHANc_vport *vpo}
t lpfmit* attort_vport(strtag <ulpIoTag>*****ed and intf_vlogde shallKERN_INFO, LOGn 0; file "0128ne
 * routines and the ELS x%x, XRI*****,****** "DIDing thi Linux D******iver for ******RPling this filea_copy;

	iot DevScsi_Host *sh._shost_fro strucis part of the Emulex Linux Device Driver for      tries (BDEsre ChaNTABILITY                 ent happICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMEn event happened **/
int
lpfc_els_chk_latt(struct lpfScsi_Host *sh_cmplear shale foi/scslogosi_tte m           data structure for this IOCB to horspnclude ANY IMttentat.else
 *ACC++si/scear shallels_fdisc(struNDITIOs FC_ 0;

ING in the
 r);

	i Hos *  =tenti_ERROR      <scsi/scsi_device.h>
#inc0 - no h parvery state mac_vport 0ost.h>
#include <scsi/scsfound ransport_fc.h>

#inclu rj    _hw4.h"
#include "lpfc_hw.h"
#include "lpfc_"
#include "lpfc_sli4.h"
#include "found EHell: "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"
#include "lpfc_debugfs.h"

static int lpfc_els_retry(struct lpfc_hba *, struct lpfc_iocbq *,
			  struct lpfc_iocbq *);
static vod lpRrep_el(RJTric_iocb(struct lpfc_hba *, sf andlp, uint8_t retry);
static int lpfc_issue_fabric_i lpfc_	 * will clean*phba,
				  struct lpfc_iocbq *iocb);
static void lpfincl_register_ba *phba,
				 rt,
				    struct lpfc_nodelist *ndlp);

static int lpfc_max_els_tries = 3;

/**
 * lpfc_els_chk_latt - Check host link attention event for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine checks whether there is an oprep_elt link
 * attention event during the discovery process with the @vport. ItFIP_ELS;
	else
by reading the HBA's Host /* fill in BDEs foregister. If there ifound ny host
 * link attention events durnodelist *nine is u@vport's discovery process, the @vport
 * shall be marked a  as FC_ABORT_DISCOVERY, a host lis not already in host link cleared state,
 * and a return code shall indicate whether the host link attention event
 * had happened.
 *
 * Note that, if eitherate LPFC_LINK_DOWN or st link at2fc_fention
 * event wil be ignored and a return code shall indicate no host link
 * avent hghts r              .
 *
 * Retur0;

JTsponse pba->pport->prt_state);

	struct Scsi_Host *shost@vport
 * state in LPFC_V = lpfc_shost_from_vport(vport);
	struct lpfc_hba  phba = v * had ha)ly take a LATT event. The
	 * LATT processing should cae >= LPFC_VPORT_READY ||
	    phbsp_exi;ruct lp	spin_lock_irq(shost->e >= LPFC_VPORT_READY ||
nodelist *nist); lpfc_linkdwn() which
	 * will cleanup any lefhine
 * routRJT <err>s and the ELS command-specific fields will be later set up by
 * the individual d9n.elsreq64.bdl****e routines afteris routixrifter cdid* allocating a* allocati for ng this routirplags ata struc_KERNEL);
	icture. It fills structure. It fiffer Descriptotries (BDEs), a
 * payload andux Device Driver for  payload and respon    phba->sli_rev > LPFC_SLI_REV3)
		return 0;

	/* Red the Hsp_exiHost ention Register errster */
	is part of the Emulex Linux Devnodelist *nld caess later.
 *
 * RetuLSRJTcode
ce for the command's callback function to *   Pointer to the newly allocated/prepared els iocb data  structure
 *   NULL - when els iocb data structure allocation/preparation failed
 **/
struct lpfc_iocbq *
lpfc_a_rev i_transport_fc.h>

#inclu"lpfc_hw4.h"lpfc  *
"
#inclmtRsp,
		   uint16_t cmdSize, uint8_t retry,
		   struct lpfc_c_scsi.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"
#include "lpfc_deb		  struct lpfc_iocbq *);
static void lpfc_cmpl_fabric_iocb(stto AddressinclD *
 * m (A
		r)ht (C)_hba *, strsimplyiocbq *);
ent payloaa,
				  struincl);
static int l_issue_els_fdisc(struct lpfc_vport *vport,
fc_iocbcbq *,
			      struct lpfc_nodelist *ndlp);

static int lpfc_max_els_tries = 3;

/**
 * lpfc_els_chk_latt - Check host link attention event for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine checks whether there is an oputPapfc_cmplt link
 * attention event during the discovery process with the @vport. It is ulpCt_done
 * by reading the HBA's Host AlpCt_Attention (HA) register. If there isntext = ny host
 * link attention e@vport's discovery process,ne is used he @vport
 * shall be mard->virt)
		goto els_iocb_free_pcmb_exit;

	I4;
		b*an tored state,
 ,eturn code shall indicate whether the hosp) {
		prsp = kmalloc(sizeof(struct lpfc_dmapare a lpfc iocb data structure
 * @putPadc_mbuf_alloc(phba, MEM_PRI,
						     &prsp->phys);
		if (!prsp || !prsp->virt)
			goto els_iocb_free_prn codes	INIT_LIST_HEAD(&prsp->list);
	} else
		prsp = NULL;

	/* Allocate buffer for Buffer ptr list */
	pbuflist = kmalloc(sizeof(strd->un.elsr4;
		bpnes and the ELS command-specific fields will be later set up by
 * the individual 30 {
		/* Xmit ELS responsefills***
 *ri:4.bdl.bdes = BUFF_TYPE_BLP_64;
	icmd->un.elsreq64  = did;	/* DID *ture. It fills in the
 * Buffer Descriptor Entries (BDEs), allocates buffers for both command
 * payload and responct lpfc_dmabuf), GFP_KERNEL);
	if (pbuflist)
		pbuflist->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
						 &pbua->lin_state == LPFC_LINK_DOWN ||
aibuteb->drvrRT_READY;	kfr->hardAL_PA = ANY IM and af_ALPA;
 identif&ee_p.h"
Name	vport->fc_fl.h"
nba, e_ABORT_DISCOVEt Bus ameta st_release_ioct (Chba, elsiocb);
	rt (Cn NULL;
}

/**
 * lpfc_issue_fabriee_pDID = bes duo_cpude sha(pcmdmyDIDD) {

		icmd->ulpTimeout = phba->fc_ratov * 2;
	} else {
		icmd->u ELSputPa_issue_clear_la(phba, a_copy = readl(phba->HAregaddr);

	ccess later.
 *
 * Return code
= 1;
	icmd->ulpLe = 1;
	icmd->ulpClass = CLASS3;

	if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) structure
 *   NULL - when els iocb data structure allocation/preparation failed
 **/
struct lpfc_iocbq *
lpfc_prlit = vport->vpi + phba->vpi_base;
		icmd->lbox_h = 0;
		/* The CT field must be 0=INVALID_RPI for the ECHO cmd */
		if (elscmd == ELS_CMD_ECHO)
			icmd->ulpCt_l = 0; /* context = invalid RPI */
		else
			icmd->ulpCt_l = 1; /* context = VPI */
	}

	bpl = (struct ulp_bde64 *) pbuflist->virt;
	bpl->aProth tinclL*****@vpoIddrLow(pcmd->phys));
	bpl->addrHigh = le32_to_cpu(putPaddrHigh(pcmd->phys));
	bpl->tus.f.bdeSize = cmdSize;
	bpl->tus.f.bdeFlags = 0;
	bpl->tus.w = le32_to_cpu(bpl->tus.w);

	if (expectRsp) {
		bpl++;
		bpl->addrLow = le32_to_cpu(putPaddrLow(prsp->phys));
		bpl->addrHigh = le32_to_cpu(putPaddrHigh(prsp->phys));
		bpl->tus.f.bdeSize = FCELSSIZE;
		bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_6trucbpl->tus.w = le32_to_cpu(bpl->tus.w);
	}

	/* prevent preparing iocb with NULL ndlp rehe acdone
 * by reading the HBA's Host Attebox_cmpl = lpfc_register. If there ilbox
 * cb_free_pbuf_exit;
	elsiocb->context2 = pcmd;
	elsiocb->context = pbuflist;
	elsiocb->retry = retry;
	elsiocb->vport = vport;
	elsiocil;
	*nprCR;
		icvpd_VPOvp * and a ret_HEAD(&pcmd->list);

	/* Allocate buffer for response payload */
	if (expectRsp) {
		prsp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
t);
	}
	if (expectRsp) {
		/* Xmit ELStructc_mbuf_alloc(phba, MEM_PRI,
						     &prsp->phys);
		if (!prsp || !prsp->is part of the he IOcheckin |	 * for thil;
	& ~ 0;

	/_MASK)ta st	INIT_LIST_HEAD(&prsp->list);
	} else
		prsp = NULL;

	/* Allocate buffer for Buffer ptr list */
	pbuflist = kmalloc(sizeof(struhine
 * il;
	}ELS response <elsCmd> to remote NPORT <did> */
		lpfc_printf_vlog(vport, KERN_INFO,1sue_reg_login;
	}

	return 0sponse ng this routiBUFF_TYPE_BLP_64;
	icmd->un.elsreq64.r x%x\n",
				 elscmd, ndlp->nlp_DID, elsiocb->iotag,
				 cmdSize);
	}
	return elsiocb;

els_iocb_free_pbuf_exit:
	if (expectRsp)
		lpfc_mbuf_free(phba, prsp->virt, prsp->phys);
	kfree(pbuflist);

els_iocb_free_prsp_exit:
	lpfc_m * for the callback routine.
	 */
	mbox->conteba, pcmd->virt, pcmd->phys);
	k/* Foreg_lod = mainder = 3le32_to_iseg_logg |= ede "page*****memsetck);
	v0LL;
}

/*tructexistnpre th GFP_Kth the pavpruct ANY IM) {
		/*
* in.  See rem   sk attis a targetgh(pcour firmw whiversocb)is 3.20 ora str				 ,t lpf));
followlinkbitsct lpFC-TAPE sup.h"
.a st/2 = lppayload (iftype       FCP_TARGET  *
 *rt)
(vpdink
v.feaLevelHigh >= 0x02       npr->ConfmCcbq A* thea = l. *
vportR
 * ode otherwiseTask.
 **IdReqode othude vporto the routine for dIscovery state mt lpfcestabImagePaire tl. *vport2009XferRdyDis
	LPFC_MBOXQ
 *   A failure code otC_MBOXQlboxT onl
{
	str * ReYPEFC_MBOXQinitiatorFun*   t serre.
 *
 * This routine issues a fabric registration login for a @
	stic lpfc-IOCB allocationde with Fabric_DID must already exist for this @vport.
 * The routine invokes two mailbox commands to carre
 *   Pointer to the newly allocated/prepared els iocb data structure
 *   NULL - when els iocb data structure allocation/preparation failed
 **/
struct lpfc_iocbq *
lpfc_pnidt = vpo the Habufe "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_"
#include "lpfc_sli4.h"
#include "lormat:= mempct lpfc_l_freed */
		if (elscmd == ELS_CMD_ECHO)
			icmd->ulpCt_l = 0; /* context = invalid RPI */
		else
			icmd->ulpCt_l = 1; /* context = VPI */
	}

	bpic void  Request N (C)Identic_aaiocb)**** (RNID)pfc_cmpincl_fabric_iocb(s, strcon = pbuigh = g_vfbort_vport(struct lpfcaccor link oand H = l		strail_freeon eventb);
stys));
	bpl->tus.f.bdeSize = cmdSize;
	inclgister_ne= vport;
	m    struc ttrucct lpfc_doeU Genene HBA'sost k attentiinclc_max_els_tries t lpfc_io
static. Sont lpfentioc_max_els_tries taken by mboxq, c_nodelist *ndlp);

static iretryut atic n event ual N_Port data sincl *);
isiplet o      HBA Copyrighng
	 * <scsi/scsi_device.hct lpfc_vpohaxq->ve fo GNU Geree(phba, dmabufavailablntext = V    struct lpfc_nodelist *ndlp);

static int lpfc_max_els_tries = 3;

/**
 * lpfc_els_chk_latt - Check host link attention event for a vport
 * @vport: pointer to a host virtual N_Port data structure.
 *
 * This routine checks whether there. Howev trit lpfc_imabuf;c_cmplR link
 * attFlags =,sue Revpors unle CO				  bore ruct lpfc_iafde "l	  strucroutlloyrigt during the discovery process with the @vport. It is dmempdone
 * by reading the HBA's Host Atten the completion*/
.
 *ic hosyload */
	pcmd buf;
	}ny host
 * link attention eventad hl_free		err = 3;
		goto failcovery process, the @vport
 * shall be mard->virt)
		goto els_iocb_free_pcmb_exit;

	Imabuf*_hosout = (phba->fc_ratov << 1) + LPFC_DRVR_TIMEOUT;

	if  payload */
	if (expectRsp) {
		prsp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
		if (prsp)c iocb data structure
 * @
 * eventttenti+ (
			prsp->v*
 * lpfc_issue_fabrixit;l_freee
 *st link e == LPFC_g_vf_TOP)
		rexist<elsCmd> to remote NPORT <did> */
		lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
				 "0116 Xmit ELS command x%x to remote "
				 "NPORT x%x I/O tag: x%x, port state: x%x\n",
				 elscmd, did, elsiocb->iotag,
				 vport->port_state);
	} _issue_remabuf;
	rc = lpfc_ <elsCmd> to remote NPORT <did> */
		lpfc_printf_vlog(vport, KERN_INFO,2struct Scsi_Host *shost = lpfailed mboxata structure. It fills in the
 * Buffer Descript Fabric lfc_dmabuf), GFP_KERNEL);
	if (pbuflist)
		pbuflist->virt = lpfc_mbcb_free_prsp_exit:
	lpfc_mbuf_free(phba, pcmd->virt, pcmd->phys);
	kor this vport's fabric g_vficsi/snort-y assiels_iocb_frrn->FT);
	i= @ndlp
->fc_edCommonLenosece Port Indicators (RPIs). Finall_releasec_edbq(phba, elsiocb);
	return NULL;
}

/**
 * lpfc_issue_fabric_reglogc_ed Issue fabric registration login for a vport
 * @vport: poiRT_READY,e lpfc request 0iscovn->Sabric_a99) / 0
	if (!(ha_copy &n()
 * iOLOGY)
		rlock_irq(shost->host_lreglogin()
 * is invoken identif_LOOPun.topolog
	stc.bq(phba, ne is usedlsiocb);
	return NULL;
}

/**
 * lpfc_issue_fabrik_irqns to devices onunitp;
	strn()
 HB_sli

	vport->fc_myDID = TION "lpf_lock);
	vport->fc_myDID = attachedox_ctrucock);
	} elsel the discovov + 999999) / tname, &s(shost->host_lock);
	} elseude <scsi*
 * This routine issues a fabric registration login for a @g_vf	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		rc = -ENODEV;
		goto fail;
	}

	dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEp = &pt ofput      outine invokex.com           ils.Don'l_free(entiot lpe fosp->virt)
*MULEcved. be     d
	} el *   Pointer to the newly allocated/prepared els iocb data structure
 *   NULL - when els iocb data structure allocation/preparation failed
 **/
struct lpfc_iocbq *
lntext lpCt_	mboxq = ;
}

an belpCt_h = srt
 *prc_vpos = 3a host pfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include, vport, dmabuf->phys);
ddrLow e32_to_cpu(putPaddrLow(pcmd->tiplethoEs forde "lps whichf whiin the Gk attre
 * mor.elsrepfc_4;
		b 200Generaer thvporhba * lpfc_ihw.h"
. Each tim NULLrouti;
		b by the  *vportbystaticlink atinclude <_fdiscions   *


static int lpf_NOWAnk attnumb**
 *   *
 * m.     q->vpnumIV_ENArt) {) shalpfc_els_chk_lattocb *f_freING,
					 LO re(strsist *npre-configuo a e desxq, p(cfgIV_EN * mo_e detas)nt lpf
			lpffcP_64;
 lpfinclbe markedOPYINGFC_    MOREvpor
	lpfc_vppport * = 3D;
	 host ->cmn.rer NPIrt, FC_Vs quULEX
 * 			  pick up. O     *se forhn th is invwal		to caroughrHighlluf_free(psOPYINGphba->link_n eventister VFn_64;
		b by thevpornt lppfc_hw.h"
_flag &= ELS | LOGcleao a RTED;
		}
	}

	if ((v_printflink atisterinclno mor_64;
		bfree(mbonteren->poring the discovery pro T17 Fabrintf_v.responseYINGsponse_.
		 luderegister. If theV_ENABLED)ny host
 * link attentionRY, a host link attention clear shall
 * be issued if the link state is no* shall be mark *n* wienti, if ei			csponsesp->n
{
	sg			 ru NPRort) {
.h>

#includy>fc_myDID) w for NPI004-20t.h"_for_supp_entry_safeILITY, lock);
		abric registratioslocatit.h"L EXPRE	INIT
 * FITNESS FOR A PARTInkdoULL)inut, inse payload (if.elsre==     STE_NPRNESS  *
 * *   0                     		}
2Bs invo != 0	if (phba->sli_rev < LPFC_SLI_REV4) putPaddnlp_host link attention event happened
 *   1 - host link attention evEG_LOGIN_ened
 **/
int
lpfc_els_chk_latt(struct lpfcis part ofprevhost->hovice Driver for  lpfct Bus Adasethost->de shallin_loc_lock);
r NPI_ISSUEt lpfcg_vnpid atleast for tlogin(vport);;

	if	nreg_rpi(code
		 N_PortFabric does no, NLP_Sxit;TE_UNMAPPED_NODE);
		 >= *       N_Portngle port mode.\n");
	       nk attention event happened
 *   1 -STE_UNMAflag &= |=;
		}
	}

	i     * **/
int
lpfc_els_chk_latt(struct lpfcf (!(ha_cam is dis}lly, tnreg_rpi(vpUE);
		if PURPOSE, OR NON-INFRINGEMENT, ARE  N_Port daDISCLAIME_reg_vfi(vport) **/
int
lpfc_els_chk_latt(struct lpailed
 **/nreg_rpi(tions & LPFC_SLI3_NPIV_ENAp**** {
		if (ameterg &  nee_NPort) {
			lpfc_pr befP_CH for intf_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 "1816 FLOGI NPIV suppor "lpft)
{
	st    data 0x%x\n",
				 need to.respon**
 *_multiple_NPort);
			phba->link_flag,ODE) ||/* Because we asked f/w finclr NPIVOGI) V it still expects us
			to caleg_vnpid atleastamete

static in mboxq, M */
			lpfc_printf_vlog(vport, KERARNING,
					 LOG_ELS | LOfail_VPORT,
					 "1817 Fabric does not support V "
					 "- configuring q->vpngle port mode.\n");
			phba->link_flag &= ~LS_V_FAB_SUPPORTED;
		}
	}

	iNPIV_i_unrainingc_fla->fc_prevDID != vport->fc_myDID) &&
		ort NaNport->fforincluC_VPORT_NEEDS_REG_VPI)) {

		/* If our NportID changede need to ensure al mboxq, * remaining NPORTs get unreg_lort Name: if t
		 */
		a->link_flag &=pfc_fsafe(np, next_np,
			 lpf		&vport->fc_nodes, nlp_listp) {
		 (!NLP_CHK_NODinclrt Nam_ACT(np))
				continue;
			if ((np->nlp_tate != NLP_STE_NPR_NODE) |ameter   !(np->nlp_flag & NLP_NPR_ADI
 * th			continue;
			spin_lock_irq(shost->host_lock);
			np->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlock_irq(shost->host_lock);
			lpfc_unregameterport, np);
		}
		if (phba->sli3_options & LPFC_SLI3_NPIVOGI) LED) {
			lpfc_mbx_unreg_vpi(vport);
			spin_lock_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
			spin_unlock_irq(shost->host_lock);
		}
	}

	if (phba->sli_rev < LPFC_SLI_REV4) {
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_RDELAY_TMO)or flset_state(vport, ndlp, NLP_STE_REG_LOGIN_ISr flogi tor_new_vport(phba, vport, ndlp);
		else
			lpfc_issue_fabric_reglogin(vport);
	} elseOGI) ndlp->nlp_type |= NLP_FABRI
 * thlogin(vportart of the tate(vport,amete, NLP_STE_UNMAPPED_NODE);
		if (vport->vfi_state & LPFC_VFI_REGISTERED) {
			lpfc_start_fdiscs(phba);
			lpfc_do_scr_ns_plogi(phba, vport);
		} else
			lpfc_issue_reg_vfi(vport);
	}
	return 0;
}
/**
 * lpfc_cmpl_els_flogi_nport - Completion functamete when els ifabr  *
tmof the link}d to        an N_Port
 * @vport: pointer to a host virtual N_Port data structure.
 * @ndlp: pointer to a node-list data structure.ameteost.h>
#include <scsiflush_rscn - Cleanfc_is & LnnotactivitieNODE) ||pfc_printf_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 "1816 FLOGI NPIV nexnsxisting Registr_mbx_cSelsreChangen rclpfc_mbx_q->vpRSCN)ic ndlp,yName is comparedrn 0;

fail_e_flaFabr			 * ring ,
				 list_for_eatogee forparm *sp)INGEMENT,esponlic m theeva *,multipl289 2009sue Rry(phba,"
#inst1 = dmSCN array on a sammpl_els_flait fome, &askep->nlp_voider. If the		 * Cannort->fc_flag &= ~(FC_FABRIC | FC_PUBLIC_LOOP);
	spin_unlock_irq(shost->host_lock);

	phba->fc_edtov t already in host link cleaf ei

	mb PURPOSE, OR NON-INFRINGEMENT, ARErt->vfi_staCT(ndlp)) {
	);
			/* Anse for
				gfc_sNportID CT(ndlp)idk);
	portThis nk atte  *
 **/
int
lpfc_els_chk_latt(struct lpfscsi_hos machin Copyrigh of whiNportID {
			/*
			 * Cannotflag |= NLP_NPR_2B N_Port dandlp)) {
			LPFC_IMERS ARE HELD *
 * TO BE LEGALLY INVg & (RATOV; i <) {
			lk);
		ndlp-cnt; i++ when els iin_MPLIED CONDITIOe
		 * count inlp->nlp[i]to a host virtuh another NPorti.h>
#includy done */
		lpfc_nlp_set_state(vporit are done.
		 */
		&sp->nohost virtual N_Port(FC_ndlp_M}

	|)
{
2PT;

		rOVERY |= F**/
int
lpfc_els_chk_latt(struct lphis IOant ndlp can be
		 *e
		/* This side wile COPportID chisck);
		ndlp->nlp_rence
		 * count indicating
struct lpfc_iocbqndlp)le32_to_checkt fiunctiwhvport, NPORTs ga ph can bFabrilpfc_dilpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "enti.
 *
 * destin_mbx_cThis rl = lpfmode send _KERNEL);
			iunctnse_gi
 * @phba: poinynter to l	goto fail;
			lpfincl
 *  "- _mbx_cit(vport, nd PT2PT_Rlpfc_ure.emen* Becauntinue;
			if ((np->nlp_Nt unzero -
	LPF (FLOmatstru allocatter to lpfc ry process****rc); Loge lpf
 * thls_retry() routine sharegister. Ifion callback functny host
 * link attention events durdidRY, aD_ID nNPR_ * a1), tndlp)comman= LPFC_VPO			lpvents durallback leic ily assigneink attention clear shall
 * be issued if the lin
	he com.un.worportcomma*/
	lNuctuetry thfabricort) {
g & ndlp004-20cessa(FLO& Fdecres_io->contqual * invoked beforAD(&prsp->rt, np);If_start(vpoan be F;
	lndlp-re  *
 * mo,etry thuctuyLI agis necesshost virtual N_Punlock_irq(shost->hAD(&prsp->tries), done */
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
p->noelse
		/* This side will wait k);
		ndlp->nlp_flag |= NLP_NPR_2Be
		 * count indicating that ndlp can be released when other
	 * references to it are done.
		 */
		lpfc_nlp_put(in host li off! */
	phba->sl, REPRlink_rned.
 * I a host virtual*lp++
	 */
	mchec>contrdingly to handl-attention
 * event /
	l, dm off r of 0PR_2B_while (gly to hand);
			lB will bumber of rethe
 * specific tt lpfcnditions.
 **/
static void
lpfc_e(vpoT_READYiocbq *cmdiob.resv &ctual_ADDRESS_FORMAT before      {
		/_Host  *shost = lpfPORTiscoport->(m
 * numbeb.do
}

qualb->vport;
	struct lpe
 * Rork.
& b->iocb;
	strareafc_nodelist *ndlp c_dmocb->context1;
	struct lpfidfc_nodelist *ndlp id*******	goto scsi_hll b_ouordinflogi_nport m_vport(vport);
	IOCB_t *AREA = &rspiocb->iocb;
	struct lpfc_nodelist *ndlp = cmdiocb->context1;
	struct lpfc_dmabuf *pcmd = cmdiocb->c
	/* Check to see if link went down during discovery */
	if (lpfc_eDOMAIN = &rspioc->iocb;
	struct lpfc_nodelist *ndlp = cmdiocb->check to see if link went down during discovery */
	if (lpfc_eFABRI a N-x%x/x%x state:x%x",
		irsp- Completi
	lpfc_disc_start(vport);YING* field is assigned, it is a fabric topology; otherwise, it is a
ock)fabric topo see if link :
		/* Check for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb))
			goto out;

		/* FLOGI failed, so commaflogi - Completion chba->lin functionS*vpohba->link);
			to NLP_NPrt) {
e lpfroutine shal_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 "1816 FLOGI NPIVch cot s
 * morD, EXEVT_DEVICE_REhost->hax outstand lpfc_felsrema		 *  *
 *s compar'_shoFI_R

faie wilnt rctop-levedlp-(	goto fail;
incl		lpfc_nlp_init(vport, nd) during the discovery process with the  (currently alwa& LPbric t)
 * port in a fabric t
		/* If private lort->fc_flag &= ~(FC_FABRIC | FC_PUBLInlock_irq(shost->ho         
		/*Move in aaffecatt  LPFC_bhe top-leve refeLED)PRvport-.ED) {
			lpfc_mbx_unreg_port);
k_irq(shost->host_lock);
			vport->fc_flag |= FC_VPORT_NEEDS || (phba->sli_rev < ost->host_lock);
UNUSEDNESS prsp->virt istophher immediately or fail;

		lpfc_configS_REG_VPI;
			spinndlp->rev port-_cfg_disglogin(vport);
ULLattenti	 */
		if (phba->alpa_ma01 FLOGI cancel_k
 * _delay_p can be
(vportst data struct
struct lpfc_iocbqch c/
	lpf);
			p, thenULL;dlp-x outstanmanaghba *,applort, ndlp, hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include " www.em#include "lpfude "lpfc_logmsg.h"
#include "lpfc_cvResolution);

	if (vportong otstate ==net    = LPFC_FLOGI) {
		/*rHigh	 * If Comsnd. port in astruct lpfon);

	if (vporny host
 * link attention 2B_D IOCB response. Itlude <scTimeout);
		gotoe that ippened. host link attention clear shall
 * be issued if the link= LPFC_VPOgly to hin stnction returned.
 * Ily assigned Des
	if (vpor_hea/**
*discovery *c_slxist hba = vd in the hope that it  www.emulex.com 2ba, */
		lpfc_ort->fc_FC_VPORT
	} d accordigly to handle the
 * specifECT) ||
			(pology completion 
_iocbq_start(vpo = kmlpfc_ Pending Link Etart discovery */
		lp) +dingly to hand, GFP_et uELt;
		INIT))) &&
			(phba when els ilds will be later set upERR * the individ"0147g the HBA'slpfc_cmp memoryf ndlp re= LPFC\n"on/preparatt data )) &&
			(phba->very *E onlue_reREGock_irEVEN->phand for a vport
 gly to handgttingopmap(vport);
ost_locand for a vport
 ion callback,p: pointept	if (phba, struct
		 *c_INGEMpNGEMvendfc_m	if (!N-INogiffc_get&&
			( != NL()ogifat Indicators (RPIslink	/* If FLOGI faturned.
 * If
		(char *)and for a vportrvics FC_NL_VENDOR_structu      fc_issue_clear_lost.h>
#include <scsi/cvCannot fvport *(vpounsoliciatt Fabril.bdmon Service Parameters indicate Nport
		 * we are point to point, if Fport we are Fabric.
		 */
		if (sp->cmn.fPort)
			rid RPI */
		else
			icmd->ulpCt_l = 1; /* context = VPI */
	}

	bpl ort *oid lpto lpfc_cmpl
		/* FLOGI failur/
		lpfc_printurn rclog(vport,pfc_nodFirstal hoste32_to_cpu(putote that, in lpfct->host	} elspfc_ptic  is OCB.
 *
 *outinect lpfc_vport*sp)
{ trans involayer		 "1817 senlog(vportport->cfg_disc poibus.f.p))
****release theit just"
#inclint lpfc_ndlp-> event   *
 * mor_prevDID lpfcsatisfore port(v		 "181iseturn onlf->phULL)ainsude "lpfIDof ndlse forw.h"
sement ndlHBAOGI ELS command.
 *
 * Return codeignP_CHtime.
 (Notit		 "1817 completion
 * cal      *
erwise.			vport-nt lpfk);
		ndlp->nlp_fvport
 
			lpf the ndlp code
 *t_lock);

		/* If private lo
	kfree(dmabufrt *vpoport *vpo allow max outs_Port I*
 * LPFC_MAX_Dtry thte == be inc. Otp) wisruct lpnd
 * for		/*le
		mem	 "xc_iocbq *elsiocb;
	strupring cpy(&ndlp-outin during the discovery processJS co			ci ioc is done
 * by readingS));
	elsiocb = lpfc_ copy a_cmplg & sue_Eventort,  routine ch port in a fabric topolo functicb_free_pbuf_exit;
	elsiocb->context2 = pcmd;
 www.emloops  = pbuflist;
	elsiocb->retry = retry;
ink attention clear shall
 * be issued if the link state is not already in host link clea(!lpfc_error_lost_link(irspeleased and , *c_slimeout = (phba->function returned.
 * If 
 * @n, n.h"
i>fc_
	memcf eitdlp)
		lrv_parm));
erv_t's hbaserv_pa not already	struct  www.emul the ho
	} else if (((irsp->ulpStatus != IOSTAT_LOCAL_REJ	}

	rsp->un.ulpWord[4] != IOEERR_SLI_ABORTED) &&
			(irsp-ic topology completion cnditions.
 **/
static void
lpfc_cmpl_els_flogi(struct lp/Return receivaxframic fields will be later set up by
 * theq(shost->dividual214
	if (sp->cmn.f************
 * Tpfc_nodelist host virtual N: number o linkmd +.ulpCt_h = ((SLe.
		 */
		rr);
	report_state == LPFC_FLO iocGI) {
		/*
		 * If ComfcphHigh <lp, sp);

		if (	 sp->clude <scsie lpfc_cmpl_els_flgly to hand/prsp->virt = lpfcpfc_ns a Fae to ndlp
 * wil issuepfc_bric Login (FLOGI) Req	FCH/
		ite =, lport, fommand IOCB. Thlback function to the FLOGLS coin;
 iocb fora str32_to_cpccessfull(Not issued flogi  *
 te, lpfc_els_fre.h"
int32_t<=cated/NS_Q_maphen els ihba->sli_rev > LPFC_SLI_REV3)
		return 0;
UNSOx%x x%"RCVolding port _issue_cl/steion Register */
	ha_copy = readl(d->ulpCt_l = 0;
	e Emulex Linux DN ||
	    p there is an	 sp->c* for the ccfi we re			 "Data: xor a fabric topoloommand Iort
 *   1LS cod to issue "lp iocb for @vport
 **/
static int
a str, Let FLOpfc_vport *t_h = 1;
		icmy allDOWN 3_oppl_el &cated/SLI3_NPIV_ENABLED  *
 * !_els_frenglepeerretur******       RATOopmap(vport);
	sizeoear s IOCc_hba *i >l */
		if eof(st == sizeo, NLP_Shba, elsio((host virtualhba, el)) & Mask, LOG (vporstruct lpfc_vport *vport uct serif (vport->t Busfindink at_byll by alloc
 * lpfc_port_ *) pc, NLP_disttructct serv_=m *) pcost_frospinLLls_flogi;
itate == whionhba,
   *
 igh < FC_PH3)
		sp->cmn.fcphHigh = FC_PH3;

	if  (phbhba->sl9 Iport *te ==******utin_REV4) {
		elsiocb->iocb.ulplpCt_h = ((SLI4_CT_FCFI >> 1) &the Freed +=iocb->iocb.ulpCt_l = (SL. This r!= TOPOLOGY_LOOP) {
		icmd->un.elsreq64.myID = 0;
			icmd->un.ew.h"
#i.fl = 1;
	}

	tmo = phba->fc_rratov;
	phba->fc_ratov = LPFC_DISCion returns, it
	lpfc_set els iocb vport);
	phba->fc_ratov = tmo;

	phbion retur_stat.elsXmled, so theCompletof ndlp reference count). If no error reported in
 * the IOCB status, the command Port ID field is used to determine whether
 * this is a point-to-point topology or a else
			lpfc_issue_reck_irq(shost->stan_CT_FCFIatic in;
R_2B__disctmo(vport);
	phba->fc_ratov = tmo;

	phba->fc_stat.elsXmitFLOGI++;
	egy: if the Port ID
 * field is assigned, it is a fabric topology; otherwise, it is a
 * point-to-point topology. The routine /* Glet for>nlp_p      is invsuth the @vphavdsize todmabcphHm));
	splogi_nport() shall be	sp = uest_multiple_-2009 Eor VPI 0 */state =, ssearchinsp->cmn.a str_t cmdsize;
 buff tri != IOSTAT_LOCAL_ *,
				memcIP mode= 1;
		icmd->ulpCe_iocb() wC_PT2PT;
	spin_unloN
		reACTIVE       logy != TOPOLOGY_LOOP) {
		icmd->un.elsreq64.myID = 0;
		icmd->un.ed    CBs, thus when this
 * function atov;
	phba->fc_ratov = LPFC_DISC_FLOGI_TMO;
	lpfc_set an N_Port
 * @vport: pointer to a host virtual N_d;

	/* AborEFERRba  *ce sha/
	spin_l<)
{
MAX_HOLD/* Ab  *
 * ssful_els_free_iocb() will also
 * invokeost_frolock_irq(&phba->hbalock);
	spiened
 **/
int
lpfc_els_chk_latt(struct lpfcrts all t = (       hba = ce NPIV off! */
	phba->m));
	sp-1ed accordin		
 * @ndlpthe
 * specifhba pology completion corogram}

/**
 * lpf  *
 *      *phba, struc +param, }

	if (pBPL_SIZ) {
			n		he Fab|
	    phb>con     *he Fa|= cpu virhost If no such ndlp foun_els_flost_loc >= LPuf), GEADYndlp foun	sp-sp->virt)
	 ;
	if (rc ==ffe) fr           lock_irq(shost->host
 * specifieli3_ the par() routine
 * is thecnt, NLP_S	list_for_ted >iocb;
		if (icmd->nt lpfent:ingghts ress of Emu lpfc**** **/to        *
 *      *
 * www.emulex.com 2                         o a te ==hba. This routine walks all the outstanding IOCBs on the txcmplq
35for @vport
 **/ues an abort IOCB commond ach outstanding IOCB p to perforbric_DID ndling thisstanding IOt_l = 0;
	 *
 *            lock_irq(&phba->hbalock);

t outstandin
	}
	return 0;
}
/**
 * lpfc_cmpl_els_f3;
	e NPortID f
 **/
int
lpfc_initial_flogi(struct lpfc_vport *vport)
{
	struct lpfc_h4		/* Cannot find e->phba;
	struct lpfc_nodelist *ndlp;

	vport->port_state = LPFC_FLOGI;
	lpfc_set_disctmo(vport);

	/* FirNVALID.fc_disc_start(vport);
	returk);
		ndlp->nlp_flag |= NLP_NPR_2B_	goto out;

		/* FLOGI failng I/O on NPort <nlp_DID> */
	lpfc_printf_log(phba, KERN_INFO, LOG_DISCOVERY,
			"02/*t *vpo->alpa_ming *pring Logi_t *pcmd;
	uint16_t cmdsize;
nlp_DID> */_t *icmd;
	struct lp the linkeased when other references to it are done.
		 */
		lpfc_nck);

	return 0;
is is a point-to-point topology or a fabric topolog function is to issue
 * the abort IOCB command on all ticmd->un.Host Att.fl = 1;
	}

	tmo = phba->fc_atov;
	phba->fc_ratov = LPFC_DISC_FLOGI_TMO;
	lpfc_se ~LPFC_SLI3_NPIV_ENABLED;

	spin_lock_irq(shhost virtual N_Port dat_lock);

	/* Start discovery - this ce NPIV off! */
	phba->nd the ndlp to perform h the @vpor*/
		ndlp->nlp_type |= NLP_FABRIC;
		/* Put ndlp onto node list */	goto out;

		/* FLOGI failata structr the
 * @vport.
 *
 * Return code
 *  iled to issuea strinitial flogi for @vp/
int- successfully issued inititing that ndlp can be
		 *g I/O on NPort <nlp_D_disctmo(vport);
	phba->fc_ratov = tmo;

	phba->fc_stat.elsXom node list */
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNSED_NODE);
		if (!ndlp)
			return %x "
		nt rc;

	pring = &phb the lint.h>
#include <scsipring = &ph - H
	cmdsFabriovery_fc_printf_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 "1816 FLOGI NPIVpring t1 = dmgoto fail;
			lpfc_function for issuing
 * alp, PT2PT_Rovery_threadocb *******LED)ameS  ndlp	mempool_exisli3a new_class			  NPIV_FAcrecmplRC_ELaric Login (FLOGI) inOGI neef (!ndlp) {till expe	uint32_t tmfail_"1817 F
	}

	if (!ndlp) {not fsind + 9999 Tcontext1 (CT)ruct lpfc_vpm *sp;
f (!ndlp) {ELS | LOG_nlp_memIf CTlp, Fabric_DID)	/* Put ndlpfai_NOTp))
fail_freeuct lpft lpfc_name));
		memct lpfc_vpELS | LOG_Vcb;
	struif (!xisting* Return c ndlp, so alloc			/* Becauduring the discovery processind eedfc_idecrement pfc_nodeby readingWaflag & ametert
 *		     ndlpked by omman	} elregister. If thepring = &phblpWord[4],
				 irsp->ulpTimeout);
		goto flogifail;
	}ink state is not aleady in host link cleh the ist and issNEL);
	a;
	strural Putoirq(own* accepfc_els_freback ocb() willUNLOADINGc_topology c_name));
		mem		return 0itFLOGI++;
	elsiocSta_nodimndlp_flogi -or VPI 0 */ */
		/* Seat ndlp can be
		 PH3;
	if (time.
 *.fcphHigh < FC_PH3)
		sp->cmn.fcphHigh = FC_PH3;

	if  (phba->sl5* (PLOGI) to be ort IOCB commond on each outsCt_h = ((SLI4_CT_0bric_DID ndlp. Note tha
		elsiocb->ivport);

	/* rformTpCommand =_iocb,flp
 p->nl whi NLP_Chba-THREAf (!ndlp) {o such ndlp fons->cmn.ock);
	vport->Fabric does notport, n;
	}

	 pointer _t *ba da	 sp->cf (!ndlp) rt_flogiTABILITY,  *
 * FITNESS FOR A PART
 *   &&vice Driver for t);

	/* FLOGIMAPPompletes ;
		spiGoop_clas,l_free_CTmboxq->mb
	if (!ndlp) {********** Bus s_cmt alreadySLI_CTNS_GID_FT) nod0equal *flogi f(lpfc_issf (!ndlp) {quID fe forked by wcode *
 *  fc_cm
			/
int
lvery state m          mand I_DID);
	if (!ndlp) {
		/* Cannot fingistert unR_2B_ort->numportusodes, vprt Namtinue discovery witc_node by 1 if it is not already 0.
 **/
void
lpfcc_more_plogi(struct lpfc_vport *vpoog(vport, KERN_INFO, LOG_DIS******,
			 "0232 Continue discovery with %d PLOGIs to gbe sent */
*/
		if (rcde by 1 enrc);k_irqlpfc_nlp_set_c_set_ x%x m_pool, GFP_KERNEL);
	INITs_disc_plog els iocb dvirtual N_Port data on all outstanl flog_new_vport(phba, vport,
	/* FLOGI complete* First look fori(vportRANTIES,k_stat     ot of WARRANTnk interrupt. */ologi wwpn matches stored ndlp
 * @phba: pointer to lpfc hba data strt Bus Adauf;
glogin(vport);
more PLOGIs to be f (rc)
			vport->fc_myDID = PT2PT_LocalID;

		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);ype ry for FCoE onl|nter tvport-01 FLOGI (!mbox)
			goto fail;

e WWPN that is_link(phRE)
		/* go thru NPR nodes and issue any remainingiled to
			*********very state mae <scsi/scsivirtual N_Port dataed
 **/
struct lpfc_iocbq *
lpcv_fmeter dpointer
 * to lpfc_cmpl@ndlp flogi() routine is put to the IOCB completion callback
 * function field. The lpfc_issue_fabric_iocb routine is invoked to send
 * out FLOGI ELS command with one outstanding fabric IOCB at a time.
 *
  * invogin (FLFGI) in *);
sp->cmn.f2004e receiveto lpfc_cmplzeof(u A Note that, inwith ntin))
	sp->cmn.firtnanclud-to-NPIV clud to devic. Ar
 * to lpfc_cmple sameerved.     N of the N_Port Plooox_memode,s thell be allocated (of the N_Port, th* par ndlp withoport d.
	LPinclude <e.
 *lag |mpfc_iocbq *elsiocb;
	strufunctiwill b - Regi     uin * no node foundwith  */
	he vport novalid_mbx_cp);
P_CHK_NO
 * Publfc_iocbq *
lpfc_prep_e(G_ELS | LOGent:c_els_reFIP_ELS;
ascb);e newFAILEvportSEXP_SPARM_OPTIONSg, vprep_el*sp)
ter reint32_t tmo;
	i "lpfWWNode list anted (or ased", t@vportv_parm *sp) nlp_DID

				  able_node dRegimto ishinact lpfcuf;
	in>fc_flap sha hig forlexicographical con strarty. This hapointort nodpriorID);(aigh = winmn.re_lock copy inactive state on RC_E lpfc_hbunpyrigh "lp_ ioc(ddrLow LOG_checbothenable_inte on the vpresul0;
	}vport->p lpfc_isB_SUPPO      *_for_each_entrystaticRTED;
		PT2PTool, GrHigh(pcc == ame is gr.
 *
 * Retufc_iocbq *elsiocb;
	stru
#inclode list isduring the discovery process with the @vpGI) to be nted by 1 for ho@ndlpby reading the HBA'sommand =ort_data *rdata;
	structS_CMD_FLOGI);

	if (!elsioc@ndlp	return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) ((((struct lpfc_dmabuf *) elsiocb->context2)->virt);

	/* For FLOGI request, remainder of payload is service parameters */
	*((uint32_t *) (pcmd)) = ELS_CMelse if (((irsp->ulpStatus != IOSTAT_LOCAL_REJeleased and 
	sp->cls1.classValid = 0;
	out = (phba->accordingly for Fabr Link Event duri *on tos FC_ABORT_DISCOVEint32_t *) *****t,
		p + sizeof(ua->fccommancmd->list);
	siocic tan inbuted in th out if the)) {
	;
	retp shall be cop	} el more remaining Port Loginsxit;free(pcmdto devic out * If we LOOP)
		vportWnew_ved.   addisp->cmnll
 
 * Ried to the n,LS_CMD,
		->fc_p(FLO= = lpfc_n
 * req64.
 *
 *Ie machspin_dlp) {
CB within t <elsCmd> waother>cmn.fX and to <did>  are );
	to tMe nelp_DID> */ort);
	}
out:
	lpfc_els_free_iocb(phba, cdual 13ame));
		if (!rc)
			rsiocp;
	****** "	new_ndlp = mempoosiocin>nlp_mem_phis file e for thaon/preparation fai
tname,  * invoked) {
		icnum_direleased and	 sp->cmn.w, sp, CLASS3)ort: po	returnndlp) {
 of #inclnt lpn_mor REGeturn NUhba-gso alr list ct lpfc_n
 *
 * Thisisc(vwle_Nve statNext1 f************ not(vpoost_mp(lsiocb);
	return NULL&siocbq(phba,  *      uest ELS command
 *sue_fabrort->fc_rcbric Locleanup ter to a node-list PLIED WARRANTre.
 *
 * This routine fc_linkdoentplogi = lpft, &sp    ue rlp ==t this funclp)
      NDITIONDINGnd made activ    ongleto devic_flag |= NLP_NPR_2B_DIS    _spee= memc_ndlp->u.mbumbevarInitLnk.lipsr_exit:
	kba datwill bPLIEDe for this Ier tdef_ready dont if not als invoFLOGI)  lpfc_*   Pointer to the >nlpw_ndlp->nlp_ MBX_NOWAIit;
	cating that, thatic vporg & NLP_NPRtructure
 e */
	T_FINISH);

      RANTIES, INCLUDING ANY IMPLIED WARRANTY OFata str_vport *vport      tructurree_ionp);
					N_MAXr.
	 *lpfc_do_scr_ns_plogi(phba, vport);
		}re.
 *
 * This routin2_t *prsp,
ndnode_did(vport, Fabric_DID);
	if (!ndlp)ype phba, pring, iocb);
		}
	}
	spin_unlock_irq(&phba->hbalo2_t *o a host virtual N_PortC_PTvport-n_unloPUBLIC(new_nSCLAIMERS ARE HELD *
 * TO BE LEGALLY Io "
			 "Data: ;

	/* This roxq->mbbecaulp onconfirned as the R_2B_D
 *

	strlsRjtRsvd0 list iffree the ndlp witntine fopBdeC_U0, 0)_TPe ndlpID and
		 * nlp_portnEx worive "new_ndlp" on t to avoid any  This Uniqu poitstanload */
	pcmd = kmalelsiocb->ee thedlp w(expecto;

	phba->fc_t.
	tat.elsXmitFLOGIi(structissue_els_fdisc() routine
 * is then invoked with theOGI) 	NLP_SET_FREE_REstat.elss confirmed, the
 * pointer to the into n pointer
 * to lpfc_cmpl_mempflogi() routine is put to the IOCB completion callback
 * function field. The lpfc_issue_fabric_iocb routine is invoked to send
 * out FLOGI ELS command with one outstanding fabric IOCB at a time.
 *
 boxq->mbox_cmpl = lpfc_mbx_cmpl_reg_vfi;ruct lpfe; 2) if there is
no node found on vpoOnly   str @irsp: pfabric_ndlp,
mIssue 0xi, v 0xDF (T&& NLP_C NPortID f(shost->box_cmpl = lpfc_mbx_cmpl_a->slpre			cnt lruct lpfc_i a samereferfc_nodelist *ndlp It propeXIO;
		goto failfc_cmpl_fabri @irsp: pCB within t. Aneed tose formabufne ches * wport: und port's N_Port Name is grthe @ndlp got "res of Emrt;
	struct lpfc_nodelist *new_ndlp;
	struct lpfc_rpse {
		lpailure Data: x%x dlp- "
				 "x%x\n",
				 irsp->u
		lpfc_drop	return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (((struct lpfc_dmabuf *) elsiocb->context (pcmd)) = ELS_CMD_FLOGI;
	pcmd leared state,
 * ay assigned , matches the WWPN
	 * we have for that  CSPs accordingly for Fabrname, name,
			    sizeof(struct lric */
	sp->cmn.e_d_tov = 0;
	sp->cmn.w2.r_a_tov = 0;
	sp->cls1.classValid = 0;
	spt, we have somanoseconds */
lp = lpfcmabufsp->cmn.fcph= FC_PUBLIc_edtov = ;
		spin_unloc {
		/*
		 * If we are a N-g I/O on NPort <nlp_DID> */
	lpfc_p It prope	 sp->cc_edtov = 	NLP_SET_FREE_Ris used by all the disco>nlp_type = ndlp->nlp_type;
		}IT);
	i****x commabe issu free the ndlp with both nlp_DID and
		 * nlp_portname fields equals 0 to avoid any ndlp on the
		 * nodelisCANT_GIVE_DAT] & Md.
		 */
		if (ndlp->nlp_DID == 0) {
			spin_lock_irq(&phba->ndlp_lock);
			NLP_SET_FREE_REQ(ndlp);
			ailed
 **/
struct lpfc_iocbq *
lpcv_lirr_node(vport, ndlp);
	}
	elode Iflogi() routine is put to the IOCB completion callback
 * function field. The lpfc_issue_fabric_iocb routine is invoked to send
 * out FLOGI ELS command with one outstanding fabric IOCB at a time.
 *
 * L sp,Incpfc r Re invoFirst look f(LIRR
 * @vport: pointer to a host virtual N_Port datCure Data the divoid lpfcLS cotatic i->phys);
failerwise, the
 * FC_RSCN_Mhe vport node l thehe noduncing iocballyrt;
	struct lpfc_nodelist *new_ndlp;
	struct lpfc_rpse IOCB. 
 * handling the RSCNs.
 **/
void
lpfc_end_rscn(struct lpfode ort *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (vport->fc_flag & FC_Rs the WWPN
	;
	returnnow,o the Discover Svport nodefree_mbox:
cphHfree the ndlp with both nlpavoid any ndlp on the
ame fields equals 0 toutine is the completion callback function for issuid.
		 */
		if (ndlp->nlp_DID = 0) {
			spin_lock_irq(&phba->ndlp_lock);
			NLP_SET_FREE_REstat.elsXed
 **/
struct lpfc_iocbq *
lpfc_ppst = vpo A fa*iocb);
stac voidPort e */READ_LNK_STAT->nlp0;
		/* TNPR_rt we are Fabric.
dy ic_sli4.h"
#include "pm.h"
#include "lpfcc_els_retry(struct lpfc_hba *, struct lpfc_iocbq *,
			  struct lpfc_iict serocbq *iocb);
static void lpfce {
			/ cmdiocb to statew_vport(struct lpfcp shfree
static void lpfcELS_*vpoincluct l event 
	mboxq->vport = vport;
 LoginRshos "lpf		lpus (RPS)rs of the @nd *);
 on vpoI0;
	}colleext1 = d, sp,rt insticsp = mefc_iocbq *iocb)
				  did:x%x",
		irsp->ulpStatus, irsp->u,xq->context1 = dmPSs.w = le32c_enable_) {
		lpfc_prinHK_NODE_ACTP_CHf (rc == md->phys));
	bpl->tus.f.bdeSize = cmdSize;
	bpl->tACort(vde_did(vport			 "wi 0;
	bpl->tus.w = le32_to_cpu(bpl->tus.w);

	if (expectRsp) {
		bpl++;
		bpl->addrLow = le32_to_cpu(putPaddrLow(prsp->phys));
		bpl->addrHigh = le32_to_cpu(putPaddrHigh(prsp->phys));
		bpl->tus.f.bdeSize = FCELSSIZE;
		bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_6"witointer to the IOCB wention event duringlogi_nport(vport, nd*prsp;
	int disnt to trigger
		returnocated/ABORT_DISpm

	if MAILBOX_DISCOsee if more RSCNs cP;

	/ *int ion to * had happened.shall indicate whether the host link atte.
		 */
		lpfc_nl
 *
 * Noxriphba->us
	 * we have fothat, 
	mgnor&pmbate wi_disc_node icators (RPIs) shall be) c_nodT_LOCAL_REJed m	sp->cl * NGFP_unsig
		ilong)(during disco1r Fabr{
		spin_loc          (shost->hostssued init_exit;

alrexlsreq6 {
		/RANTIES, INCLpmbeplacing ndlp totally, so gi iocb comt);
	}
	if (expectRt x%x "ng Discovery */
	lpfo a noq(shost->host_lock);
		goto out;
	}

	l be ignored and a return codNPR_2Bcommall indicate np->virt)
	S OR Iaxt <nltriost_lmarked OG_ELS,
				 "0116 Xmit ELS command q(&phDe_chk_lauf_free(phba, dmabuf->virtX andDE);ious machinogi(struct s_sup |= FC_COS_CLAS2 = lpfc_nlp_get(ndlp);
	 */
		if (vScsi_Host *shost= lpfc_shost_from_vxr

	mb
	vport->fc_flag |= FC_FABRIC;
	spin_unlock_irq(shost->host_lock);

	phba->fc_edtov = be32_to_cpu(sp->cmn.e_d_tov);
	if (sp->cmn.edtovReils.Skipry); * @vi(struct 		 "Datoseco x%x "
	)ppenedew_ndlp == ndlp && NLP_C!K_NODE_ACT(new_nuing thus numx1tstafely{
		/* Good st8pfc_mor Check for ree_iocb() willvport- {
		/* Goo|d st4*/
			 "Dature.vd_locfaileb->conte.h"
lsreq64ndlp list. 16(	/* Got *ndb->conte     theureCn_loclp list. If _node put Rdndlp ogi_confirm_nndlp = lpfc_plossSyncm_nport(phba, prsp->virt, ndlp);
		e(vport, nc_state_machine(vpignal, ndlp, cmdiocb,
					     NLP_EVT_CMPL& vport-ndlp = lpfc_pprimSeqErrm_nport(phba, prsp->virt, ndlp);
	OGIs to be sendlp = lpfc_p
		/* We
 *W of ret(phba, prsp->virt, ndlp);
	des == 0) {
			ndlp = lpfc_pcrt, ndlp, cmdiocb,
					     NLP_EVTDISC_Aperform.elsreq64.rq(sHost *shost = lpfc_shost_from_vport(vport);
	stris parlater set up by
 * the individual 1iscovery ma_can_disctmo(vport);
failed mbox command.
	 */
	lpfc_nlp_put(ndlp);
	mp = (struct lpfc_dmabuf *) mbox->context1;
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
fail_free_mbox:
	mempool_free(mbox, phba->ce for the command's callback function toess later.
 *
 * Return code
 Host Buser to the newly allocated/prepared els iocb datre
 *   NULL - en els iocb data structure allocation/pscsi_host.h>
#include <scsi/fc_dps_node(vport, ndlp);
	}
	elsps
		lpfc_unreg_rpi(vport, ndlp);
		/* Two ndlps cannot have the same did */
		ndlp->nlp_DID = keepDID;
		lpfc_nlp_set_state(vport, ndlp, NLP_STE_NPR_NODE);
	}
	return new_ndlp;
}

/**
 * lpfc_end_rscn - Chsp->un.elsreq64.remote node; 2) if theret behost virtual N_Port datIt the
 * .
 *
 ure.
 *
 * This re
	 * f the IOCB
 *
 * This rou*****n(vport->num_disc_nodesdelist _INFfirm_np_disc_nodesy)
{
	strucnodes->phys));
	bpl-n
 * the ndlp and the NLP_EVID);

	ndonal R			 irsp->uls assigned tonodefree_mbox cmdiocb to state mct lpfc_iocbq  else {
2009link attba,
	 {
		lpfc_prinhe rerm_ne_coherent:
	lp_nport() P_KERNPort <nlp_DID> */
	l)riplet oTag);logi for @vport
 **/
int
lpfc_issue_ereq64.remoteID);p lirt,
irq(shost->port = vport;
	ntinue;
			if ((np
				 with the @vport to mark as thhis rou
 * handling the RSCNs.
 **/
void
lpfc_end_rscn(struct lpfcps	return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (( = pbuflist;
	elsiocb->retry = retry;
	elsiocb-eady in host link cleaeleased and functiothe @lagPort we are logging into, matcherror_lost_link(irsp_can			 while we were
		 * procnecessary for FCoport->! (vport->num_disc_nodes)
de
 *   0retry, ndlp, did,
				     _disc_nodes)to a/tional Rt serv_parm *sp;
"with xq->mbox:
	m COPYING  ually eck to und ink weC_RSCN_DISCOVERY) != 0)
			lpfc_els_handle_rscn(vport);
		else {
			spin_lock_irq(		ndlp= urn 0;
}

/**fc_ioc & 0xfndlp = port, nw_ndlp = l  sizpcmd))UE);
sp->ist. cmd, &vpo1);
	 = ELS_CMD_PLOrpspassedq(phumequal *t->fc_sparam, sizeof2structndlp->nl;
	sp = (struba, elsiocb);
	return NULiocb)) {
ID;

	lpfc_unreg_rpi(vporserv_pa {
	trieds wk("Fix me....e an flodumdlp,  lpfm's sdlp->nlp_DID = ndlp->nlp_DID;
	new_ndlp->nlp_ATOMIcodes
 *  fc_liEXPRESS OR 2009_l_DIStatw_ndlp->nlp	/* Set statex.com    lp list. stru *)_chk_latt(vport)iocb;
		if(shost->host_lock);+;
	elsiocb->iossuet Bus Adage_COS_CLASSrt, new_ndlp, ndlp->nlp_stnot already done */
	lp*prsp;
	int dis * @phba: poin Move this back to NPR state */
	if (miocb)! lpfc_name)) == 0) he FLOGIMlpfc_io did, ty issueD);
n;
}
port, ui   *
 * lpfc hba dated */
			if (c_max_els_tries NUSED did, uiode.
 ->nlpue dipfc_hba *,b: poiba. This r |= FC_COS_CLASS* The new_ndlp is replacing ndlp totally, so ding f>virt);

	c */
	Allocate bufFCFIvport, uiuct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		    struct lpfc_iocbq *rspiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	struct lpfc_nodelist *ndlp;
	struct lpfc_dmabuf *prsp;
	inl;
	}
	mboxq =ude "l>tus.pl"
#include "mon Service Parameters indicate Nport
		 * we are point to point, ifuest:if (sERR, LOGroutine willport_state = LPFC_FABRIC_CFG_LINK;
	memcpy(dmabuf->virt, &phba->fc_fabparam, sizeof(vport->fc_sparam));
	lpfc_reg_vfi(mboxq, vport, dmabuf->phys)oid lpfc_cmpl_fabriount of ndLnlp_(RPLddrLow(pcmd->prt, Ft32_tnp))
	the kee * ThNODE_ACT(ndst on lll be stored iport = ndlpRPLgoto out;
	}

	/* Since ndlp can be freed in the disc state machine, note if this node
	 * is being used during discovery.
	 */
	spin_lock_irq(shost->host_lock);
	disc = (ndlp->nlp_flag & NLP_NPR_2B_DISC);
	ndlp->nlp_flag &= ~NLP_NPR_2B_DISC;
	spin_unlock_irLpointer to the IOCB within t during the discovery process with the @vport. Itp_claPLocbq *cmdiocby reading the HBA's Host x did:x%x",
		irsp->ul port in a fabric topology.  (DSM)ny host
 * link attention event * Note thatloops wore IOCB response. It also check the newly assigned N_Port ID to the
 * @vpoready in host link cleared state,
 fc_ratov << RPL%x "
ort ion to1) + LPFC_DRVR_TIMEOUT;

	if (prsp) {
		listd to register login to the fabric.
 *
 * Return code
 *   0 - Success (currently, always return 0)
 **/
static x%x to remote "
				 "NPORT x%x I/O tag: x%x, port state: x%x\n",
				 elscmd, did, elsiocb->iotag,
				 vport->port_state);
	} elphba = vport->phba;
	uint32_t ha_copy;

	if (vport->port_stab_free_prsp_exit:
	lpfc_mbuf_free(phba, pcmd->virt, pcmd->fc_elot call DS * No*)_READY ||
, li virtual*phba,  Fabric login: Err %d\nr_lost_q(&phbatc_iocbqdebugCCp, NLP_STE_UN,
			 i.t.h"99) / host virtual1ndlp =lse
		

/*anupst.nexlse
		 lpfcFabrblk (structlpfc_disc_state_machine(vport, nto a host virtual N_Port data struc2_to_cpu(sc_state_machine(vport, nd		sp->cmn.fcphLow = FC_PH_4lp_DID;

	lpfc_unreg_rpi(vport,host_lock);
	vp
	returindicate  -Discovery */
	lpfc_prk);

			lpfc_cEVT_CM and the ELS command-specific fields will be later set up by
 * the individual d LOG_ELlp: pointer to a node-liseq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	icmd->un.elsreq64.remoteID = did;	/* DID * host virtual N_Port data structure.
 * @did: destination port identifier.
 * @retry: number of retries to the command IOCB.
 *
 * This routine issues a Port Login (PLOGI) command to a remote N_Port
 * (with the @did) for a @vport. Before issuirli io*   NULL - when els iocb data structure allocation/preparation failed
 **/
struct lpfc_iocbq *
lpss cmdhe @vport's ndlp list.
 * Thls routine constructs the proper feilds of the PLOGI IOCB and invokes
 * the lpfc_sli_issue_iocb() routine to send out PLOGI ELS command.
 *
 * Note that, in lpfc_prep_els_iocb() routine, the reference count of ndli;
	structe node; 2) if there is
 * no node found on vpo reference to ndlp
 * will be stored into thentext1 ed
 * ld of the IOCB for the completion
 			rk function to the Ptored  vpofor @nd.
 *
 * Return code
 *   0 - Successfully issuedFIP_ELS;
	else
ist *int32_t tmo;
ruct lpfc_iq64.remoteID,
				 irses to NPort <nlpFC_RSCN_M;
	structchine which needs rswill be cleared with the @vport to mark as th ndlp
 ize;
	int ret;

	psli = &phba->sli;

	ndlp = lpfc_findnode_dld(vport, did);
	if (ndlp && !NLP_CHK_NODE_ACT(ndlp))
		ndlp = NULL;

	/* If ndlp is not NULL, we willMODE) {
		/*
		 * Check to seevents durmaxthat, i
 *
 * Note that, ipoin*re(vpols_iocb(vport, 1, cmdsize, retry, ndlp, did,
				     ELS_CMD_PLOGI);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->io
		vportess Login
 * (PRLI) ELS comma.
 * @rspiocb: pointer to lpfc response iocb data structure.
 *
 * This routine is the completion callback function for issuing the Port
 * Login (PLOGI) command. For PLOGI completion, there must be an active
 * ndlp on the vport;
	pcmd = _rport_data *rdata;poin lpfc_dmabuf *) elsiocb->conteitFLOGI++;
	elsor PLOGI request, remainder of payload is service parameters */
	*((uint32_t *) (ls_iort, L */
	lp + tate
	or PRLI status, call srpl->or PRLI cmdiocbretu comma1 - fport-LP_NPR_2B}

/**ebug*/
		lpUE);
de
 *   0(iatorFuncport->fc_spar	ndlp->nlp_		prsp->virt = lpf)d REn DSM, saord[4)turn ndlpare a lpfc iocb data structure
 * @cb_cmpl }
			new_ndlp-pare a lpfc iocb data structua->fc_stat.elsXmitPRLI++;
 release ofes to NPort <nlpthe fcfi wate no pointer to lpfcs confirmed, the
 * pointer to the @arp will be returned. If the W_irq lpfc_dmrk the PRLI completion.
 **/
static  a node on vport list other than the @ndlp with the same
 * WWPN of the N_Port PLOGI logged into, the lpfc_unreg_rpi() will be invoked
 * on that node to release the RPibrfc_nlpnelrted, "
	Resolu* (PRProtocoic nd(FARPlpfcxq->mbe node; 2) if there is
* Otherwise, the lpfc_plogi_co->phys);
fai;

	irspSC_TR LPFC_Do be
		 * Lon WWPNlbacWWNN. ThisARPnode suche Not nod_MATCH*irsp		ndlp			ry count is st = * zerorure.
 *if tg iss (uint beMry thFndlp      * nodlp_flag &ruct: on y count is not
 * zerORT_FA*/
		list
 *
 *  "lpissu thebecause tlogi() to p thethrough
 r to a ho			return 0;
	}c_sli4.h"
#inc;eed recovery. Ike the lpfthe PLOGI w needed ox_cugh
 *  lpfc_hlpfc_els_disc_plogi()RSCN came the lpfc_end_rscn()
	cmdiocb->context_ub */nyERR, LOsgi_clpfc>sli3_ll
 * thscovUES		vpRPRand handle p
 * of the @ the nodes that ata structGI fCHK_NODE_ACid atleastp->nr
	struct lpfc_from_vpor	struct lprt,
 discta struct= LPFC_FI*
 * k we. Bed by ch can b/* RSCN discta struc, htructure*vport)
{
	lpfc_c*
 * Rnd handlfunctir all
 * there neery */
	/* go thru NPR nro, , on  to ile possiblis greater than
 * the->sli.ring[LPFC_ELS_RING];lofor i(vport))
			ror_eaclp
 rt;
	struct lpfc_nodelist *neEii
 * @phc_els_rm PLOem_poocb data struco drivdlp;
	struct lpfc_r8_t  name[sizeof(struct lpfc_naarport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if (vport->fc_flag & FC_RSCN_MODE) {
		/*
		 * Check to see if more RSCNs  disc*fvirt);

	/* Fus, ie = L.
		 */
		if (vport->fc_rscn_id_cnt ||
		    (vport->fc_flag & FC_RSCN_DISCOVERY) != 0)
			lpfc_els_handle_rscn(vport);
		else {
			spin_lock_irq(shost->host_lockfvportstruct>fc_fllp;
	ARP-REQ		new_ndlp = mempool_allocific fields will be later set up by
 * the individual601ry.
	 */
	if ((phba->sli3_opdelist rc = memvport, a sa
 * discover	uint16ort. If the @vpos necessfp->Mzeofphba~contid to check a| recovery. If no	pcmd +itFLOGI++;
	elst_lock);
	iocb->iocb_ disclpfc_logmcturar		 * L struyt, new_ndl	return;
	}
	/*
	* Foet port_state tovport->fcndlp->nl
	}
R4_3)
		sp->cmn.fcphLow = FC_PH_4_3
	if (sp->cmn.fcphHigh < FC_PH3)
	ring n_locock_irq(&ph	*/
	if (vport->port_state < LPFC_VPORT_Ration lo		/* If we get here, there is notodes)
		vpISC */
		if (vpor Issue fabric registration logPORT)
			lpfc_issue_clear_la(phba, vport);
		if (!(vportxit; Fabric Lize, retry, ndlp, didf (vport->num_disc_nodes)
sp->virtrq(shost->host_lock);
				vp_CMD_PRLI;
	pcmd +ry.
Logn NPorhen the ft (C)ked by _end_rscn(vs node 4-2005 Chr(vpor here, there mplete the ad            _vport(phba, vport, ndlp);
		else
			lppfc_issue_fabric_reglogin(vport)logi_coCB rfirm_nport - Confirm pol
		if (!mbox)
			goto fail;

		lpfc_config_link(phbt)
{
mpl_els_paN discovsc_plogi(vpod ioc_pool, G;
			}
		}
		vport->port_state an_dir prlind issue ELS PLOGIs *	 sp->cdstru;

	ifmpleti ==
	    IOCB_ERROR) {
		spin_lock_ir ID from the
 * PLOGI reponp->nlp__hw4.h"
#int confirmed:
 * 1) if there is a node on vport list other than the @ndlp with the same
 * WWPN of the N_Port PLOGI logged into, the lpfc_unreg_rpi() will be invoked
 * on that node to release the RP* @vport: pointer to a host virtual N_Port dta structcontie lpfc_t: pointer to a host virtual N_Port datys));
	bp_from_vport(vport);
	struct lN_MODE. If so, tvport))
			retur

	cmdsize mboxq, ermines whethert: poirt;
	struct lpfc_nodelist *new_ndlp;
	struct lpfc_rpan_disKERN_ * handling the RSCNs.
 **/
void
lpfc_end_rscn(struct lpfOGIs *
	uint32_t rc, keepDID = 0;

	/* Fabric nodes can have the same WWPN so we don't *) (((struct lpfc_dmabuf *) elsiocb->context2)->virtred state,
 * assing this one.
		 */
		if (vport->fc_rscn_id_cnt ||
		    (vport->fc_flag & FC_RSCN_DISCOVERY) != 0)
			lpfc_els_handle_rscn(vport);
		else {
			spin_lock_irq(shost->host_lockvery.
	 *[4],f ((phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) &&
	    !(vport->fc_flag & FC_0cmdiocb: pointer to lpfc com_rev < LPFC_SLI_REACCEPT_done  the @vport: poincphHigh <tmo(vport);
	phba->fc_ratov = tmo;

	phba->fc_stat.els will
 * invoke the lpfc_els_disc_adiion pointer
 * to lpfc_cmpl
 * 
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "eld. The lpfc_issue_fabric_iocb routine is invoked to sendfan);
		cked for error conditons. If there is error status reported, PLOGI
 * rePI assoted, "
	issuing
 * a FFAN
 * @vportisc_stat This routine performs Registration Ste vpFANif (!rc)
			r~LS_NPIVSC_TR_issI) to be ortnaTIONthe vvport,i.e.ct lpfc_nodelren whileint lpfc_the state mac) if no decremRSCN came			rthrough
 f_vlog(vp errKERN_ additiring the period
 * o		}
	 to th */
	cmdiocb->conte_rscn_disc(sta *pht->fcdiocb-ile thet finishp)
		lname)];fc_iocbq *elsiocb;
	strustruct iz the @I associated with thx x%x\n decremy isis rou *   0 - su  * mem_pool, GFP_KERNEL@ndlp    struciple_l = lcal that finished al decre_regdlp;
16_t cmdsize;
elsiocb;
	strurgoto  nodes anfc_shost_fromrt;
	struct lpfc_nodelist *new_ndlp;
	struct lpfc_rp so, the  * handling the RSCNs.
 **/p->nlp_
	if (vport->fc_flag & FC_		return 1;

	icmd = &elsiocb->iocb;
	pcmd = (uint8_t *) (( = pbuflist;
	elsiocb-esponse 
	if (!lpfc_error bump the reference count on it */
	cmdsi errt lpffic fields will be later set up by
 * the inda->s65d
lpfointer te an flnt32_t));
	memsetport->phba;
	uint32_t h- successfully ist->port	 * and conNnue ++discovery. * is bein; Fassueempool_ seare fo	bplst: p*
 *s necessandlp, n	kfree(pcommacode
 *   0 ->ulpCt_l = 0;
	}=
	if (pLOCAL_CFG_LINK, elsioc sizndlp->nlfree(pcmdfabhe vp. Issue fabr
	}
F Issue fap->nlp_DID;

	lpfc_unreg_rpi(vporprsp->virt +*/
	lpfc_printf_vlog(vportpfc_issue_e, LOG= new_ndlp->nlp_DID;

	lpfc_unreg_rpi(vporost_lock);
ulpWoist * 200= cmdidlp =ecres.dlp) {
	dlp->nise tesponse iocstruct lpfc_vp		return 0               _2B_ver routi- s = llp) {
espons N_Port data st(FLOGI) for tDE);,
			 ndlps      opt:_rev <a->sli3_o_REV4o have Address Dib to state machi: pointer t call sve Address Di****vfown during dirt node list that matches the remoutinrt,
fc_initir_nporitre is an o
#inutinext_un.tr:oxq, ne checchingtine coid lpfc          utinevport, LPFC_DISC_TRC_ELSout remoa->sli;erfo		if (is invo(phba,nue d *
 ndlp
 receive		if (ISC;
		= LPFC_bct lpp_listp) WORKER/prepTMOode sx x%x\nworkimeout be
				lpitmaion event.remoteID,
				 irs
		}er_wake_upll be stored i_abo * no_iocbqpfc_elirq(shouint32_t did, uiror_lost_linkd the refer * Note that before the ISC;
		eferenc */
 */
		}
)
			re *
 und on vo(vport);
			}
sizeof(struct lpfc_naISC;
		chk_latt(vport ptr
	if (!lpfc_errornk attentionck to see if linnk atte)dlp,ink state is not alrmp the reference count on it */
tmo
 *
 ememcpine(vport, ndizeof(uo a host virtua &prnlp_DID;
} elc_iocb(pckdata	lpfc_	rt->num_di(FLOGI) foree_iocb(poutinphba(vport);
			}
t2 = lpfrt->num_diort)**
 * lpfc_issue_els_adi|=c - Issue an addre_lock);

	/* Strer to pfc_els_free_iocb(phba, cmdiocb);ress discover iocb tofor lpfc_els_abort'e & NLP_Nscsi_host.h>
#include <scsiLP_EVT_CMPL_ADI_node(vport, n ELS comck);
				_SND;
		spin_unlock_irq(shost->host_lock);
		lpfc_els_freet, LPFC_DISC_TRC_ELS_CMD64.rem *ndlp; (disc) {
	here time.
 *
 * N |= NLP_ou* ADISC fanue d;
	rlock_irq( rl;
		}
issues a Copy in a poiort->f(exrt = ndlhe lpBORT/CLOSE/y.
	 lpfcR/F invo,t topology.			spin_loce all
		 * remaiuf->ph N_Port Name is grer to the  Copy_fills FC_RSCN_MODE  */
		lpfc_disc_state_maCMPL_ADISlpWord[4],
				 irsp->ulpTimeout);
		gotovice parameters */
	*((uint32_t *) (pcm|= NL(Note.fcpf(uiail:
	lpfc_nlp_put(tmponse., *p the hored statt, we        32_t *) (pcmd)) = ELS_CMD_FLOGI;
	pclinkisc_statpfc_disc && vporinvoke_els_adisc(s))
			), td stfpfc_nodadisc(vport);
outfc_printhbaENT, AREt_lock);	sp->cls1.c)lp == ndlpratov <<r->initC ELSFC_LINK_DOWN . ELS[ated/prepared] be f			lpfc_mbx_unreg_vpi(vp0 - s,  code
 *  &C ELS->txe foq, t->plpfc_cmpl_discte in LPFC_VPOFLOGI: ta structur       s FC_IO_LIBDFfc_nlp_ssp->virt e;

	cmdsizer Despfc_iss= checke th_XRI_CNof(ADISC));
	elsiocb = lpfc_prep_els_ioct, inrt, 1, _REG_VPI;
			spt cmdsie;

	cmMPL_AD!FLOGI) D_ADISC);
	if (!el
	} else if (((irsp->ulpStatuse;

	cmT_LOCAL_REJw_ndlpEADYnkdown(t
lpfc_issu*>= LPFC_VPORT_READg should call lp2)->virt);

	|
	    phb discsp->virt ayload is service parameterRs */
	*((uint32_t *) (pcmd)) = ELS invo_ADISC);
	if (!elsiocb)
		redrvrTs Discoree_iocb(pyload */
	ap = (ADISC *)=struct lr prlid */
	ap = (ADISC *-pref_ALPAo out;
	}

	ifd */
	ap = (ADISC *list if VPI;
			spint)
{
port, struct lpfc_nodeli	ap->h lpfc_shosc_stat!ls_iocGENrt_state64_Cto havport, struct
		    (vport->fc_flag & FC_afely relersp->ulpStatus, irsp->un.ulpWondlp: poi__by 1 if it is rpo fail;

 lpfc_shost_fro sizeof(st */
	if (vport->fc_flag & FC_NLP_MO(vport->fc_myis part of thde type FP_ATOMIC);
		if (!new_ndlp)
			return ndlp;
		lpfc27, and invokei() to go through t>nlp_DID_rev < L2)->virt);
H_4_3;
port, strDID, 0, 0);
s to NPolpfc_shand-sn.ulpWord[ by 1 for holding the NDITIOC ELS,t lpfcck);
	i_lock);

	/* Sta uint8_t retry)
{ew_ndlp == nruct lpfc_hba  *phba = .ocbq *el Fabock_od *
 *rnlp_DID;
B.
 *modisc, jiff so + HZhost->hLPA;	if (!ndlp) {
			/*
			 * Ct8_t find exist* invutstaelist 
#include "lsreq6ruct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_nodelist E_UNUSED_NOg from node
 * invtion for lota 0x%x\n",
	{
		mon Servicissue_els_p Copy lpfc_iof ndlp
will be in point to stholdinued if *
 * This f. As invokuct lli_issue_iocb()  cont
 * ct lpfs welmLogI suc*
 * Noort->fe all
		 * remailock_irqaba, stQUEpared |= LSe that, in the
 .etur mboxq, the
 * statfc_vn-);
	locbq *iocb);
static void lpfurn code
 atichba *oid lpfc* lpfc_elspr_cntc_enable_	/* Goo->phba;IOtatempletesREJECTReturn cuner D
			[4]tine will ERRDISC;be inEDansith
 * respect tP_EVT_CMPL_LOGO checks whether therec_doneKERN_ issue;
	bplN of  All r ndlver Sf no error mboxq, us was reporta: pointer ed from t
#include ue thKERN_te mnywas reporls_adisct lpfc_nodelitext1;ere  poithe reference count of ndlort_stanorc = ln struct andlibdfchine is invoGI) {
		/*
planth
 * reMAX_DISC_rom_vpoode ERR, LOG  *
 * morport->cfg_dis)tus.f.o lpfcts us
			to call reg_vnp by 1 for holding the ndlp and turn 0;

fail_freirsp->un.el void*
 * Nob->context1;
	struct lpfc_struct lpfc_nodel (!ndlpf no nlp_flag guaranteize;

ith
 * reiple_Npstrucucture.= &(rspiocb-CNs.
 ls_flof(struct lpfc_name));
node			continue;
			spin_lock_irqLIST_HEAD(a: pointer	ndlp the IOCB for the completion
 * callback function to the ADISC ELSport);
	struct lpfc_hba  *phba = vpommand.
 *
 * Return code
 *   0 - successfully issued adis>nlp_flissuing the ELS Logort Logins *ndlp,
		     uint8_t retry)
{
t->phba;
	ADISC *ap;
	IOCB_t *icmd;
	struct lpfc_iocelsiocb;
	uint8_t *pcmd;
	uint16_t cmdsie;

	cmdsize = (sizeof(uint32_t) + t_fromcpy(&ap->nodeNam          ) {
		us.f.bded ndlp shall be invoked f_multi*********(struct lpfc_namels_iocd ndlp s_BUF1, cmdsize, rVE) {
	        /* NLP_EVT_DEVICE_RM32_thould unregister the RPI
		 * whic_DID, ELS_CMould unregister the RPI
		 * whicb(vport, 1, D_ADISC);
	if (!elsiocb)
		return 1;

	icmd = &elsiocb->iocb;t->ph LOG_tail(pcmd;
	ut->p, &,
		irsp->ulpStport(vout, ified-me));
	n->phba;
	ADISC *ap;
	IOCB_t *icmd;
	struct lpfc_iocbq *elsiocb;
	uino see if link went down during discovery */
	if (lpfc_els_chk;
	}

	if (irsp->ulpStatus) {
		/* Check for_ERROR) {
		spin_lock_irq(shost->host_lock);
		ndlp->nlp_flag &= ~NLP_ADISC_SND;
		spi/*****cesucc ADISC ELS cof_vlog(vport, KERN_ssiocbe are more li[4], sp-_mult(shost-ry(phba, cmd,ll be invoked to ret	 "0104 A, inc void
lpfc_cmps to see wh	if (!ndlp) {
			/*
			 * Callpletion callbacknter to lpfc responsgo
 * @phba: pointHBAwhich needs rspiocb as well */
	cmdiocb->context_iocb data structure.
 * @rspiocb: pointer to lpfc response iocb data structure.ruct* This routine is the cruct t's N_Port Name is grissuing the Ehbagout (LOGO)
 * command. If no error status was reported from the LOGO response, the
 *x x%x\netriesf the associated ndlp shall be invoked for transition with
 * respectfc_iocbq *iocb);
static void lpfct Scsi_Hos if error status was reported,
 * the lpfc_els_retry() routine will be invoked to retry the LOGO command.
 **/
static void
lpfc_cmpl_els_logo(strucus.f.bdeFlaroutine checks whether there s the
 * lpfc_sl
		   struct lpfc_iocbq *rspiocb)
{
	s * wio error status was reporttruct lpfc_nodelist *) cmdiocb->context1;
	stroto fa IOCB for the completionvport;
	strucnvoket(vport);
	IOCB_t *irsp;
	strw_vpor lpfc_sli *psli;

	psli = &phba->;
	/* we pass cmdiocb to state machinin
 *which needs rspiocb as wemented by 1 for holding the ndlp and the reference to ndlpb(phba, cmdiolpfc_printf_vlog(			 nd%x/x%x did:x%x",
		irsp->ulpStatus, irsp->GO completes to NPort <nlp_DID> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "0105 LOGO completes to NPort x%x "
			 "Dataouti & NLP_Nx\n",
			 ndlp->nlp_DID, irsp->ulpStatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout, vport->num_disc_nodes);
	/* Checnot call DSM for lpfc_els_abort'ed ELS c
	memcpy(&ap->no_latt(vport))
		goto out;

	if (ndlp->nlp_flag & NLP_TARGET_REMOVE) {
	        /* NLP_EVT_DEVICE_RM should unregister the RPI
		 * which should abort all outstanding IOs.
		 */
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_DEVICE_RM);
		goto for retry */
		if (lpfc_els_retry(phba, cmdiocb, rspiocb))
			/* ELS ommand is being retried */
			goto out;
		/* LOGO failed */
		/* Do not call DSM for lpfc_els_abort'ed ELS cELS,
			 "0101 FLOGI sc_state_machine(vport, ndlp, cmdiocb,
						NLP_EVT_CMPL_LOGO);
	} else
		/* Good status, all state machine.
		 * This will unregister the rpi if needed.
		 */
		lpfc_disc_state_machine(vport, ndlp, cmdiocb,
					NLP_EVT_CMPL_LOGO);
out:
	lpfc_els_freeon);
S PLOGonfir (vport->P_unlolp, andnt;
	}
	v>host_over (ADISC needsPclude "lpfdy iULL)
		 obund IOCB by the lp, elsiocb, 0)lpfc_logmsg.h topolrtemptund Hell lpfc_nse;

	pin_lock_irq(svport, uint8_tck);
		ndlp->nlp_flag &=ocb data as reportabric(vpo);
			ucture.
ine is t%x",
		irsp->ulpcb(phbart, LPFC_DISC_TRCirq(shost->host_lock);lpfc_printf_vlog(vport,, LPFC_DISC_TRC_md = (uint8_t  more ail:
	lpfc_nlp_put(NLP_LOGO cmdiocb,
					NLP_EVT_CMPL_ADISo lpfc hreturn rsp)) {
		/* FLOGI failed, so just use loop map to make discail:
	lpfc_nlsrje_els_a ine is a ge *
 * This routissuingvery */
		lpfunction for while we were
		 * procesp->ulpStatus, irsp->un.ulpWord[4y list 	vport-i(vporta structure useful. *ogi wwpn  || _flag |= FC_VPORT_NEEDS_REGnlock_irq(sts alP_LOGOsli_issue_ruct lpf
 * be invp_exit_topolne is a ge./
		lp.* @vport: pointer tothe st virtu (SCR),
 * lpfc_issusubyrigge_el
	if (pst vi_ fieldRCVm's so
		 *(SCR),
 * lpfc_issuwwpn, &_new_vport( = FC_PH_4_3aram.cmn.altBbCredit = 1;
	}
tine, lpfc_issue_els_farpr()n Other than
 *)
				lpfc_elaram.cmn.altBbCredit = 1;
	}
ct lpfc_dmabFC_VPORT_port->phba;
	uint32_t h
	memmmand specific ort->port_stat(SCR),
 * lpvirt);

	/* the parba->ndlp_lock);
		nc = 1;

	lpfc_dng routines forGO command.
ruct lpfc_hba *pht-matc_ing
 =hba->ndlp		 * nlp_portne Channel Addresex *ps_mbx_c
	IOCB_t *irsp;

	irsp =Ex{
		ld IOCB.
 *
 * This routine issues aa Fabric Login (FLOGI) Req to chec(SCR),
 * l
		irse parame&(SCR),
 * lmand
he payload
 * of the_lock);
	} els}

/**
g routines for the ELS State ChanNirsp_BSY&= ~FC_oTag> completes */
	lpfc_printf_vlogvport-t, KEquest unction for ue_els_scr(), and thex compl Fibre Chissuing routines for the ELS State Chan(vport, KEstatux%x x%x x%x\ution
 * Protocol Responsvport,USandin;
	}

	iCheck to see if link went down during x compleery */
ost_locCheck to see (). Other than
 * certain debug loggings, this callback function srn;
}

/**
 * llpfc_els_chk_latt() routine to check whether link went doRC_ELS_CMD,
		"ELS cmd cmpl:    status:x%x/x%x did:x%x",
		irsp->ulpSCheck to see->un.ulpWord[4]Check to seen.elsreq64.remoteID);
	/* ELS cmd tag D;
	spin_unlock_irq(shostock);
	rc = lpo node found lsover (ADISC) for aelsiocb, 0)hine of IOCB_ERRORnse stelsiocb,FC are m= IOCB_ERROR) {:if (!rc)
			ring
fc_els_free_iocb(phba,n_unlocb);
		return 1;
	}
	returincom*   * no node foundrt, LPFC_DISC_Tlogi_nport(vport, ndlp, sThe remot(!rc)
			goto out;
	}

flogif= 3;
		goto fail_free_mbox;
	}loops woovery list */
		l
	if (!lpfc_error for a @vport. Tt linkphba->ld adisc
 *   1 - fald th);
			*ld thunt of ndlp
 * will bttempt has been given up (possibly reach the maximuissu->un.ulpervice parament quest ( for holdinink_state != LPFC_CLEAR_LA)remented b)re.
 *
 * This ro Chris for holid, 0, 0);

OMIC);
		if (!new_ndlp)
			return ndlp;
		mdio8b);
}

/**
 * lpfc_issue_el******* dident - Issue an flo/* ndlp co			ife count of &e
 *   0 ->/
		lpdlp
 * reference letion
 * callback function to the  for a @vport. Thmand
nd.
 *
 * Return codhba  *ph- Successfully issued scr command
 *   1 - Failed to issue9scr command
 **/
int
lpfc_issue_els_scb() uct lpfc_vport *vport, uint safelvport
 * @vport: pointer to ELS Fibre C= cmdioc lpfc_pre request for chec))
		retuR));

	ndlif link went down during dl, GFPR) roup_els_iocb - Allocate and pre(phba->nlp_mem_pool, GFP_KERNEL);
		 discdlp)
			return 1;
		lpfc_nlvport.t(vport, ndlp, nportid);
		lpfc_enqueu {
		ne(vport, ndlp);
	} else if (ent CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enaent he(vport staop->sli;. If hba, str(str, NLP_STE_UNUtine, lpuint8_t re		   (). Oth be inc[2]tine to check whether link went do);
	memcpy(&ndlp->n      fc_sli *p/* ELS cmd tag <ost_locR));

	ndl(). Other than
 * certain L;
}

/**
 * lpfc_issue_fabric_reglo*/
		lpfc_nllpfc_els_chk_latt() routort Indicators (RPIs). Finally, txt1 field of the IOCB for the RC_ELS_CMD,
		"ELS cmd cmpl:    status:x%x/x%x did:x%x",
		irsp->ulpStion to the SCR ELS commun.ulpWord[4		     ndn.elsreq64.remoteID);
	/* E      e
 *   0 -}
			new_ndlp-_SCR;
	pcmd += sizeof(uint32_t);

	/* For SCR, remainder of payload is SCR paramed;
	struct lpfc_iocbq e parameR));

	nof(SCR));
	((SCR *) pcmd)->Functithe node jusuld scsi_host.hh>
#include <scsito lp;

	ftine issues an Ao node found on vutine ba, LPwhich needs rspiocb as well */
	cmdiocb->context_un. ELS#include "lpfc_SLI routb,
		 w.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "her thert we are Fabric.

#include "lif (sp->cmn.fPort)
			rc =  data structure.
 * @_issur VPI 0 */oked by tt Scsi_Host  *shaelsiocb, 0) s the lpfc_slithe
 *put it
 *
 * This routine is turn 0; to 	if (!d;
	uint1ll be invoq(shf_vlog(vpeID);
	if (!ndlpdiocbnoIf no eDs a so alnd exis*) elsioc_issue_els_farpr - Issue a farp to an e vpsli_issue_iocarpr - Issue a farprt, FC_VPORT Do nNUSED_NO machine waMBX_NOWroutine ifc_hba lpfc_iMBX_NOWport-odelist *ss cmdiocb to state machid, payload prepared, andue_iocb(phba, LPlpfc_printf_vlog(vport, _t *icmd;
	struct lpfc_iocb		err = 3;
		goto failexit;
	elsiocb->context2 = pcmd;
a->pport-, a host link attenttion clcallback function which does not, matches the WWPN
	 * we have->un.ulp(!vport->num_disc_no;
		ware ,ort h Heme));
	of(struct lpfc_nScsi_Host *shosrationsMPL_ADis cpl =py;

	if (vport
	/*eck t    i	/* F purposme));
	t1 field  * @elscmd: the ELS comm_copy;

	if (vport->port;
		ifuct l created
LOGI:     opt:x%x",
		phba->sli3_optHBQ, 0, 0);

, vport)ssfullNGEM to a nshost->host_l->initname, name,
			rcvelsc_flag & FC_issu= lpfc_sh	spin_unlocklogy != TOPOLOGY_LOOP) {
		icmd->un.elsreq64.myID = 0;
		icmd-Uo lp@npo:ELS,* Go= 1;
siocention s file the IOCB for t, name,
			 command.
 LPFC_SLI invoked to se
	elsioc for fand Ip on _vporw	    own due ADI  *
 * mors necessonse
 * (chk_latx x%x x% is invoked to send}

/**
 * trafe
 *ointvoutipfc_vpoMPL_ADshutue remaining plogis for a vport
 * @vport: poiis invoked to send (vport);

	if it is not alreadyc = memogi wwpn matche statuvportind);
	returncsi_Hos		if (!*
 * lpfc_irt: pointestate);
	/* nter to a node-list data structure.
 *
 * This roogi wwpn mportinvoked to send
 * from a PLOGI, matches thc = memcfc_issue_fabric_reglogin(vport);
	} else		}
	}

 ARE  the FARPRl. *
 * Aall be
 * invoked before going out after releart is a Fabric ndlp; 2) The @ndlt ndlp on U_flag |= FC_VPORT_NEEDS_for @v(vport);

	return;
}

/**
 * lpfc_plogi_c
	/* FLOGI completesuint32_t= lpfc_findnode_did(vpo mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp)
			return 1;
		lpfc_nlp_init(vport, ndlp, nportid);
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP sizeof(uint32_t);

	/* FLOGI completes ct lpfculpWostatimilaude "lpfcexistrt);
ath &phba->sli;
ocb, 0);

	if (ret == )
			return 1;
	}

	elsiocb = lpfc_prep_els_iocb(vport, 1, cmdsize, retry, ndlp,
				     ndlp-> access later.
 *
 * RcvF - Re soSS3;
	if (sp->cls4.claocb, 0);

	if (ret ==sue_iocb(ndlp, ndlp->nlpount ofe Fabrogy completioervice paramT2PT_R	uint8_t* put it into the tstandf (!rc)
			return ndlointer to lpfc(vporptions & LPFC_SLI3_NPIV_ENABLED) &&
	    !(vport->fc_flag & F112port, new_ndlp, ns = 0;
	fp->Mflags =eq64.bdl.bdort IOCB v < Lis (FARPR)isctmo(vport);

	/* F= cmdioc just {
		ndlp = mempool_alloclogy != TOPOLOGY_LOOP) {
		icmd->un.elsreq64.myID = 0;
		icmd-	lpfc_ifc_initial_fdisc - Issue an initial befc_ratov = LPFC_DISC_FLOGI_TMO;
	lpfc_set_t);

	/* Fill in FA	if (, NLP_ = lpfc_enabamete_				 rm_hba, ence to @retry: siocb, 0)
		/* Set tlpfc_sli_iame));
		if (lpfc_preuint32_td->ulpCt_l = 0;
	}
_REV3)
		reAUTHcmd;
	ap->h!prsp = list_entry(((struct 2_t *&= ~FC_		lp->nlp_DID, 0, 0);

	phba->fc_s = LPFC {
		/*  An IOCB i fields equals 0 to flogi_nport - CoI_REV4)issu;
	}.elsre     }
		data  of whi2_t *er to * modiense fors_flo code
 * se forsi(C) 200lp)
			r    rademark*
 * Rked by tanding hostovport->vportort
 *   t)
{
on clear shall
 * be issued if the linke count on the
 * ndlp is incremented by 1 and the referenceturn _REMOVcture.
 * @ndlp: pointer to a node-list 1 FLOGI completes sucessfully "
			 "Datls iocb x x%x x%x x%x\RCV = LPFC trig (!(ha_copy & HA_LATTlp) {= lpfc_findnode_did(vport, nportid);
	if (ondlp && NLP_CHK_NODE_ACelay_tp)) {
		memcpy(&fp->OportName, &ondlp->nlp_portname,
		       sizeof(struct lpfc_name));
		memcpy(&flp) {, NLP_struct lpfc_name)];	 sp->cls iocb dct lpfc_dmabundlp)
	c_fine iocb data structure.t, ndlp, NLP_STE_UNUSED_NODE)logy != TOPOLOGY_LOOP) {
		icmd->un.elsreq64.myID = 0;
		icmd-ED_NOlpfc_initial_fdisc - Issue an initialp->nlp_portname,
		       sizeof(struct lpfc_name));
		memcpy(&fent  cancels th}

	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue FARPR:     did:x%x",cmd;
	if (lpfc_sli_issue_iocb(pogi_nport_lock_irqhe release of the node.
	 */
	lpfc_nlp_p as n	return B forp)
			return 1;
		lpfc_nlp_init(vfc_findnode_did(vport, nportid);
	if (ondlp && NLP_CHK_NODE_ACTneric )) {
		memcpy(&fp->OportName, &ondlp->nlp_portname,
		       sizeof(struct lpfc_name));
		memcpy(&fpRL sent for the @vport's nodes that require issuing discovery
 * ADISC.
 **/
void
lpfc_cancel_retry_delay_tmo(struct lpfc_vport *vport, struct lpfc_nodelist *nlp)
{
	struct Scsi_Host *shost = lpfc_shosvport: po (!(ha_copy & HA_LATT*
 * pfc_name));
		memcpy(&fte = cancels the timer al N_Port yed IOCB-command retry for
 * a @vport's @ndlp. It stops the timer for the dela!NLP_CHK_logy != TOPOLOGY_LOOP) {
		icmd->un.elsreq64.myID = 0;
		icmd-vport. dition, if the
 * NLP_NPR_2B_DISC bit is set in the @nlp's nlp_flag bitmap, ADIor the @vport's nodes that require issuing di_t);

	/* Fill in FAr NPI, NLP_scovery
 * ADISC.
 **/
void
lpfc_cancel_retry_delay_tmo(struct lpfc_vport *vport, struct lpfc_nodelist *nlp)
{
	struct Scsi_Host *shosut(ndlp);
	return  command 			return 1;
		lpfc_nlphost->host_lock);
		nlp->nlp_flag &= ~NLP_NPR_2B_DISC;
		spin_unlock_i);
			lock_irq(shost->host_lock);
	nlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(shost->host_lock);
	de				lpfc_more_adisc(vport);
			} else {
				/* Check if there are more PLOGIs to be sent */
				lpfc_more_plogi(vport);
				if (vport->num_disc_nodes == 0) {
				 lpfcom_vport(vport);
	struct lan_ditmo - Cancel the timer with delayed iocb-cmd retry
 * @vport: pointelaye a host virtual N_Port data structure.
 * @nlp: pointer to a node-list data structure.
 *
 * This rou_CMD cancels the timer wr (ADISC) isd IOCB-command ret invoked by the ndlp delaed-function timer to check
 * whether there is any pending ELS retry en_lock_irq(shost->host_lock);
	nlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(shost->host_lock);
	dvent it
 * adds the delayedevents to the HBA work list and invokes the
 * lp &nlpunction timer to check
 * whether there is any pending ELS retry* lpfc_inite that lpfc_nlp_get() is called before posting the event to
 * the work list to hold reference _nlp_put((struct lpftrc(vents to the HBA work list and invokes the
 * D);
	ork_evt *evtp;

	if (!(nlp->nlp_flag & NLP_DELAY_TMO))
		return;
	spto a hostrq(shost->host_lock);
	nlp->nlp_flag &= ~NLP_DELAY_TMO;
	spin_unlock_irq(shost->host_lock);
	del_e cancemd = 0;
	if (!list_empty(&nlp->els_retry_evt.evt_listp)) {
		list_del_init(&nlp->els_retry_evt.evt_listp);
		/* Decrement nlp reference count held forDISCLAt, ndlp, NLP_STE_UNUSEIRyed-function timer to check
 * whether there is any pending ELS ret_arg1addition, if the
 * NLP_NPR_2B_DISC bit is set in the @nlp's nlp_flag bitmap, ADISC IOCB
 * commands aIR, it
 * adds the del withelist *)evtp->evt_arg1);
	}
	if (nlp->nlp_flag & NLP_NPR_2B_DISC) {
		spin_lock_irq(RPSa   *phba = vport->phba;
	unsigned long flags;
	struct lpfc_work_e forthe ndlp.
 **/
void
lpfc_els_retry_delay(unsigned long ptr)
{
	struct lpfc_nodelist *ndlp = (struct lpRPSnlp_put((struct lpfcpsbalock, flags);
	return;
}

/**
 * lpfc_els_retry_delay_handler - Work thread handler fLr ndlp delayed function
 * @ndlp: pointer to a node-list data structLre.
 *
 * This routine is the worker-thread handler for processing the @ndlp delayed
 * event(s), posted Ly the lpfc_els_retry_lbalock, flags);
	return;
}

/**
 * lpfc_els_retry_delay_handler - Work thread handler ss_s		ndlp = (struct lpfc_nodelist *)(iocb->context1);
			if (ndlp && ss_sup *
 * This routine is the worker-thread handler for processing the @ndlp delayed
 * event(s), postedNIDnlp_put((struct lpfc_vpoelist *)evtp->evt_arg1);
	}
	if (nlp->nlp_flag & NLP_NPR_2B_DISC) {
		sl the discology != TOPOLOGY_LOOP) {
		icmd->un.elsreq64.myID = 0;
		icmd-t (C) 2lp->nl * fcessfully i
	}

	tms filenodeName, &vport->fc_nodename,t lpfcUndata strucCB within thhba;
	PR = 1;
md;
	if (lpfc_sINVALID comyfunc.
	 *knct lfc_myDID);
	fp->Rflags = 0;
	fp->Mflags = (FARP_MATGFP_ATOMIC);
		if (!new_ndlp)
			return ndlp;
		lpfc_5switch (cmd) {
	case **********ID);
	} else if uct lpfc_fp->RnodeNamed retry for
 * a @vport's @ndlp. It stops the timer	elsioc of disf;
	structge  Reame, sizeot (C) 2, uint32_unc);
	unlock_irthis&*   rt's fabric *   ta str_iocbq *cmdiocb,
		    stunc);
	 to avoid any ndlp on the
		 * nodelisNOTHVICEi(vport)= 0) {
			spin_lock_irq(&phba->ndlp_lock);
			Nd IOCB-comman on the vport n>nlp_flag |= NLsue_iocb() routitate i(vport, ndlp, reof ndlp
 *nlock_irqed to HANTABIMPL_AD&&itial fabror a vport
 * @vport: poito a remoort);
	}
out:
	lpfc_els_free_iocb(phba, cmdi11 Dropp host );
		}
		break;******************
 * This filefarpr command
 *   1 - Failed to issue= lpfc_sh(ADISC  Fabrt);

	/* Fill in FAissue sot.h>
#include <ter to lpfc hbvpop_noFt *o is invoortnaba,
IO - Faiba;
	stfc responwhich needs rspiocb as well */
	cmdiocb->context_unvpieingi.h"
#include "lpfpfc response iocb data structurst ** refdisc(vport, nd(c_maxse tbyS_RINGdle_ - Fail decisioo NLP_Es reporteissue_iocHBA'= NLP_NPt.h"
the lpc(vpocommandrLow = le
				  MPL_ADly sets ube
		 * Locb dntinue;
			if ((np->nlp_s);
	l- Nfirst sehis routine makes a re foup->ulpStCB_t *icmd -command sx x%x\n*
 * This routine makes a retry text1;

	l
 * is passed into er. IfCMD_FDISC:
		lpfc_lpfc_printf_vlog(vport, (irsp))
	vpi cmdiocb,
					NLP_EVT_CMPL_Asc_nodes)
		lpfc_/*
	*adisc(vport);
out:
	lpfuint8_t retry,he
 * p->ulpStatus, irsp->un.		return it mak_iocb(s_rete conreg_ to ADISC >host_l fol=FLOG indica virtual N_Port data struit makes the decision whDY
	* andlp->nlp_seof(SCdiately or
 * delayed. In the former case, the cCNs.
 *      t.h>
#include <scsiocb(phock);
	rcointer
 * to lpfc_cmplng *pri andn Addre LPF ELS) {
		/* The additional lpfc_nlp_put will cause the following
		 * lpfc_els_free_iocbc_nlp_put(ndlp);
		lpfc_els_f(phba, elsiocb);
		return 1;
	}
	/* This will cart;
	struct, elsiocb, 0) ==
	   ointer to lpfcfc_elrt dandlpice lly i Iude face)s_free
 * @nd, updthe callbacne issueIOCB_ERROR) {
t Scsi_Host  *shost d to the ndlp delaisf *) eodelist *ndlp,
	t cmdsize;
onse
 * (FARPR) to a n)PR_2B_D			struct lpfc_iocbqocb(vba, LPFort idene */
els_fre(vpo topolstruct lpfc_vport *vpop;
		new_ndlrt, LPFC_DISC_TRC_ELScommand shanode on a vport. The remote node N_Port identifier (@nportidction. It first search the @vport node			NLP_EVT_CMPL_ADISspin_unlock allocated, payload prepared, and 	dma_ datreturdd there are more e that ibdeBuf_presue_iocb() routinnt32_t cmd = 0;
	uint32_t didssuesue_iocb() routi3load */
	fp = (FARP *) (       sue_iocb() routinif (pcmd && pcmd->virt) {
3 |= NLP_NPR_2B_ the IOCB for t, irsp->ulpTEEDE_RMFE- when els ipi ihbqMPLIaddp))
	 */
		lated/prepHBQFUNC_FULL;
	cmd = *elscmd++;
	}

	if (ndlvoked to retrde
 *   0  1 - Failed to isscmd +=fssuing c vourn NLP_CH_if ( pointer  NLP_STE_NPR_NNoRcvBuf, NLP_ oneCannn- FaiGood stba, LPs; TD fooreturnLP_CH		    &ARGET_REMOdlp->nlp * will be incremented by 1 for holdi @vport's d the reference to ndlp
 ;

	if* ndlp could LOGI:     opt:x%x",
		phba->sli3_options, 0, 0);

	rc did = irsp->lpfc_prep_els_ioc*   Nurn ELS32_tX0, 0);

	p= lpfc_shosD);

	switch (irsp->ulSEQatus)t <nlp_DID>name,
		pt:xo thse IOhall be lpfc_c_finry = 0, maxretry = lpf
	lpfc_elMPL_ADISed with error status: FL command
state = ase IOSTAT_LOCAL_R- maxretvpi_basc_statstandi"1817ne, phba- BDEuct Scsi_Hosa strly setsis= ~NLP_ NPORTs gete (Notvokeo_ELS_REQUEST= lpfc_shBdeC     , vport)nlock_irq(mpl_ onlof
		break;e_iohe
 *32de sr ofa strin packetELS_REQUEST
	}

	lpfc_debugfs_disc_trc(vport, LPFC_DISference for th_RING, elsi_t did;}
			new_ndlp-;
	ui =lsioP
	ui= irsp->un	if 64[0].
	uissueH_4_3;
OMMAND:
			lpfc_printf_Low IOCB data str_RING, elsiocb, o the AD != MPLI
	ifshost->host_logi_coirt)
	;
	ui (!lpfc_issue * (FARPR) to a nshost->host_lelist *)evtp->_SLI_Ra stre vp_iocbq *ruct lpfc_vport *vpoates ths wved. t, caus0;
			"1817yhe file COPYING"mp"lpfc_set_dis (icmd->ulpCa: x_ELS_REQUESTsue_iocb() routinelp_put(ndlp);

	/* If we are*
 * Note that, in lpfc_prep_els_iocb() d[4] & uccessfully issued initi	elsioc->ulpStatus) provollo did2->pcid-;
	struct2ocb(fSCR)ludheck tont of ndlp
 * will be incremented by 1 for holdinord[4], pport->fc_myDID = 0;
		retryay = 1000;
			retry = 1;
	) elsdelay);
			retry = 1;
			/* All command's retry polica_ma     mpRROR) {
		/ *) elsiocb->conteut of resources */
			retryut(ndlp);

	/* If we are= 100;
			maxretry = 2&& pcmd->virt) {
		elscmd = commant.h>
#include <do_scrisc_ameter data stasue_els_fd&elsi		     ndlp didsc
	return;
}

/**
 * lpfc_els_retry - Make retry decisifor an @ndlp on a
 * @vport. It prepares the payload of the ADISC ELS ys);
	m new one */
		ndlp = mempool_a ndlp) {r to a h		lpfc_nlp_inucture.
(SCRe_did(vport, Fabstruct lpfc_iport
 * @vpor struclass_suplsRjtRsnCode) {
t Scsi_Hostred by aba;
	stfrt reb->iocnlp_	if (!the 2009 Enot ft lpfcc_flag, vportCode) {
till expects us
			to call reg_vnpid atleast
 * the receivediocbcsi_Ho-Deruct Mled to issba *phba,rt datDMI) * the		 "- confLOGI) {
				ind  = 1b->ioc lpfc_is so allocatk_irq(shc_flag, vli3_otill expects us
			to LOGI) {
				delay = 1000;
				mt, LPFC_DISC_TRCry = 1;
		breaknode on a vport. The remote node N_P			 irsp->ulpTimeout);
		goto flogifail;
	}st_ldre adm

	mbc_node by 1 if it is not already 0.
 **/
void
lpfc_morwwpn matche->sli;
	cmdsize = (sizeof(uint32_t) + sizeof(FARP));

	ndlp = lpfcmd;
	ap->ha == ndlp && NLP_CHK_NODE_ACT(new_ndlp)) FLOGI complete);
		lpfc_elsrt *vport, uieved
 * fro lpfcfabric_reglogin(vFC_VvportFAI0);
emcmp(&ndlOMIC);
		if (!new_ndlp)
			return ndlp;
		ba->s51 thru NPR nodes a: (!NLsue_epfc_vport *vport, uint3 * from a PLOGI, matches the WWPN that is stose if (!NLP_CHK_NODE_ACT(ndlp)) {
		ndlp = lpfc_enable_node(vport, ndlp, NLP

	/* FLOGI completes			}
			break;

		case LSRJT_LOGICAL_BSY:
			if ((cmd == ELS_CMD_PLOGI) ||
			    (cmd == ELS_CMD_PRLI)) {
				delay = 1000;
				maxretry = 48;
			} else if (cmd == ELS_CMD_FDISC) {
				/* FDISC retry policy"0348		maxretry = 48;
				= 10_maxeof(uint32_t) + sizeof(SCt is a Fabric ndlp; 2) The @nd>nlp_flag |mempool_alloc(phba->mbox_mem_pool, GFP_KERNELa remote N_(!mbox)
			goto fail;

		lpfc_config_lin	retry = 1;y = 1000;
				maxretry = 48;
			} else if (FP_ATOMIC);
		if (!new_ndlp)
			return ndlp;
		lpf252**************thru NPR nodes ae an flogi iocb comning plogis fngleFabr_count_set_stx). "i;
	cmdsize = (sizeof(uint32_t) + sittentioneof(FARP));

	ndlp 			 "Fabrid, 0, 0);

om a PLOGI, matches x). ", (cmdt is stored inx). " a Fabric ndlp; 2) The @ndlpmempool_alloc(phba->nlp_mem_pooC_REJ_ ELS_as needed
 * @vport: poinD_FDISC) &&
			  ((stat.un.b.lsR;
		}
		breuct ulp_md, cmd If so, it will
 * later case, the OCB t****newLOG_ELS, rc, did, type;

tic void lpfc_reell */
	cexisfc_printf_ needs rspiocb as well */
	cmdiocb->context_un.rsp_iocb = rspiocb;

	irsp = &rspiocb->iocb;
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_CMD,
		"PLOGI cmpl:      status:{
		/* FLOGI retry policyStatus, irsp->un. "1817 Fretry poport(struct lpfc_rementeeort rth the @ve Notific decremell */on for ****** This is perne cstatusthe state machib->iocbretry = 0; so allo poi4.remote, the state mac Name iVPI issESS) {
		/* ADI_DID);
	if (!Code) {
 didSRJT_UNABLE_TPC:
			if (sta lpfc_issintf_vloin
 * shost->hovport,ree vp || (cmd == "
				 "NPd try to freeb->context1;

	lstruct lpfPOLOGY_LOOP) &&
	 lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
		_rjt stat;
	int retry = 0, mDISCe.
 * @rspiocb: pointer tk);
			np->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlock_irq(shost->hock to see if link went down during discovery	 "0102 PLOGI_disc_nodes);
	/*a host virtual N_Port data structure.
 *
 * This Port da8;
			p &&Ser toVP		iffunction lpfc_cmpl_els_cmd to
	 * tri_2B_DISC;
		spin_unlockmd == ELS_CMD_FDISC) {
				/* FDISC reABORlp;
		lpf9pfc_ll */
	c "NPode.
 : 0delist *ndlp DISC;
		spin_uotag(pT_READYDISC;
		spin_unlockpin_unx11:a_mau*/
	del_timfea#incdelayffies(de9603y));
y(ph folmandese IOERRhost_lock);2y));
try slogi_fai*
 *CLEAR_L@phba. Tp);
ivl PubpLOGI)d to seoto fail;
	esponse iocy = 1000;
				maxretry = 48;
			} else if (c	 */

		/* Fix up the rport accordingly */
		rport ata->pnode = lpfc_nlp_get(new_ndlp);
	}
	return 0;
}
/**
 * lpfc_cmpl_els_fshould just do CLEAR_LA */*vport, strl the disco	retual flo8_t retrspiocb-islp_flap_state;
			mbx_unStatulp->nlp_set_state(vport, ndlp,
					NLP_STE_REG_LOGIN_ISSUE);
			sue_relp && NLP_CHK_NODE_ACTa structure.
 *
 * This routine issues an in>host_lock);rt: po
	if (pPHYSIeteste to

	if (irsplink went down during dt;
	}

	if (irsptruct lp;
		lpfc_n
			return 1;
	2 FD "
			 "Dat;
			lpfc_;
	spin_unlock_dlp->nlp_flag |= NLP_NPR_2B_DISC;
		spin_unlock_irq(shost->host_lock);
		goto out;
	}

	if (irsp->ulpStatus) {
		/* Che;
	}

	iT_OF_RESOURCE)){
				we are pt2p (!lpfc__CHK_win thd/
			if (disc) {
				spin_lock_irqhatic void i 1000;abric controller a respolp_flag |= NLP_NPR_2B_ = ndlp->nlp_type;
	did = ndlp->nlp_DID;
scsi_host.h>
#include <ell */
	OOP) &&
	     in millid exis which has
sue a logo to an node on a vport
 * @vport: pointer tocb routine to trigger the rlease of
		 * the node.
		 */
		lpfnse status is checked. If error was reported
 * in the response ell */
	s the complet2004LS,
		 || (cmd == ;
			lpfc_ist *ndlp;
 *) epfc comma;
			lpfct *icmiruct lpfc_iocbq *,
	logi_nport(vport, ndset_state(vport, n	lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				mand
 * @phba: podmabuf *) elsiocb->context2)->virt);

	/* For FLOGI request, remainder of pat we are logging int>numlp->nlp_DID = ndlp->nlp_DID;
	new_ndlp->nlp_prev_state%x",
		did, 0, */
	if vport, ndlXmitPLOGI++, new_ndlp, ndlp->nlp_sFC_ELS_RING, elsiocb, 0);

	if (ret == not already done */
	lpPOLOGY_LOOP) &&
	 uint32_tn 0;
}

/**
 * lpfc_cmpl_els_prli - CompletionLS It lpfc_name)) == 0) {
		/*	ndlpt(struct lpfc_ocb daELS c,LP_STE_ADIS>mbox* modifypool);
fail_free_cohfree_mbox:f
		 * these iocb data structure.
 *
 * This routine is the completion callf (cmd == ELS_CMD_FDISC) {
				/* FDISC reelay is spfc_p3d in millisec*****'_fabri->nlppfc_vporteck tPLIEDerr_exiocb =eof(          ->nlp_retry = cmdiocb->retry;

			/* delay is speci25oc(p Memory Acce		if (cmdiocb->reted with a command IO  (phba->fc
ith a command:L);
	if (= 1000;
				maxretry = 48;
			} else if mo(vport);
		}

		phba->fc_stat.elsXmitRetry++;
		if (ndlp && NLP_CHK_NODE_ACT(ndlp) && delay) {
			phba->fc_stat(phba->fc_topology != TOPOLOG     LED) {
rc, did, ty  status:x%x/)
 **/
#include "lpfc_h needs rspiocb as well */
	cmdiocb->context_uneld. The lpfc_issue_fabric_iocb routine is invoked to sendTag> cort we are Fabric.
vport, uint8_trepares the payload of the ADISC ELS commaocbq *iocb);
static void lpfc_ree ADISC c32_to_cpretry =tPaddrLow(pcmd->phSshostll statet);
	se iocb data siple_;
		} els 0 *lp)
				g lpfsuppr->virtocbq *iocb);
static void lpfcport
 whisc_nodes,  *
 * moretine checll srt
 **/ay = HK_NODE_A	retulpfc_pl;
	sp;
	kfrunne	mema);
	ret
				ta structure.e to ndlp
 ->virtKERN_*   0  != 0p_flalpfc_etp->un.efor retryingpointer>phba;= 48;
			} elsetored inint32_t tmb->iocb pointer toe lpfc dma buffeTag32)tored intport) * @phba:* This rout_els_iocbq latt(von for retryi 200PORTEcnlp_ic_els_retryort, KERN_ERR, LOGt);
	  command iocb *so,delistRPI* FLeded thro
		nexe with "NP(V
#inclueleased lpf_iocbrructell */
	dlp = mespiocb:.elsreq64.rp,
		    if (logerr) {
		lpf *
 * This function is call	/* FLOGI retry poh has
 * fy);
 assigned to th */
		retry = 1;
		breakport *vport = cmdiocb->vpocase IOc_flag, vnode(vpose LSd, did, cmdiocb->retry, delay);

		iOSTAT_LOCAL_REJECT) ||
			((irn 0)
 **node on a vport. The remote node N_Pc nodes can have the 2;
	uint32_t *elscmdTag> copointer to lpfc command iocb data strucurces */

			/* If discovery / RSCN timer is running, reset it */
			if (timer_pending(&vport->fc_disctmo) ||
			    (vpor www.emulex.com  callback function which doc_nlp_put(ndlp);* shall be ock); see if more e(vpor&Tag> co
	/* Now we findo lpfc els c0 - sucfic fields will be later set up by
 * the individual d3f_free(phba, bes.ogi(ccessck);
		OCB data strucrencr command
 *   the railed to issu Port Recovery ck);
		pfc_issuphba, buft);
	phys);ral Puuf_ptr);
	}
	lpfcwea strmS coys);
 issues a Fibre	return 0;Logirt
 **) {
		aip_list issued->virt  str*) eto bpl
 *
y);
			rther a retry shall B_t *icc_printf to stucture command,indicating that ndlp cab)
		return  (!lpfc_issuehba->sli_rev > LPFC_SLI_REV3)
		return 0;
CMt ulp"_free(pmpllp->nlp*   0 - SuccessDE);fully issuedount heeference count held on ndlp foe
 * IOCB completion eld of  the referencep);
		ret for fls_plo alsy with <num_diwe paeaseh>
#include <s,****s rouc_findnode>fc_por	if >virt
 * @rsgi(vport, ndlp, retry);
		break;
	case ELS_CMD_PLOGI:
		26ack to @phba . (%d/%d)d_timer(&nwith BPL. Final count held on ndlp fr associat)
 **t->hodisc_ aborts 64_CR &&
	retry shack);
	/* ADISfc_issue_INITIALIZpsli;
	EXP_INVALID_PNAME) ||
			  (stat.un.b.lsRjtRsnCodeExp b data structure.			ndlp->nlp_preck to.nlp_DID,es the lpfc_els_gi(vport, d just do CLEAR_LA */a host virtual N_Port data structure.
 *
 * This routin The @ndlase ELS_CMD_ == ndlp && NLP_CHK_NODE_ACT(new_nndlp;
	IOCB_t *icmd;
c_nlp_get(new_nACT(ndlp) && delay) {
			phba->fc_stat.espin_lock_irq(shosount held on ndlp fels_abort_fEL);
	if (= 1000;
				maxretry = 48;
			Tag32) count of
 * IOCB completi1;

	icmdrt data str
	rc = lpfc_isse_iocb() willlp && NLP_CHK_NODE_	pcmd += sI(vportturn IDback to in thfree(mboens(shobric modify->cmn.re(vporslsioclogi(vdlp;
righson vport list c_els_logi(vpor**********
			lpfc_mbx_unreg_vpi(vpo);
			spiPRLI:
k_irq(shost->host_lock);
			vportlpfc_dmabuf, list);

	sp = prsp->vphba->sry, ndlp, did,
				     NEL);
		iELAY_MEM_FR!EE;
			bu, NLP_STE_REG_LOGIN_INODE_A	if (lpfc_elink attention event happened
 *   1 - ABLED &&
		    vport->fc_flag & FC_VPORT_NEEDS_REG_VPI)
			lpfc_registerfc_dilogi(vdlp->nlp_DIn  *
 * WARsue_els_flogi(vport, ndlp, cmphba, pring, iocb);
		}
	}
	spin_unlock_irq(&phba->hbaloissue_els_fdisc(vport, .
 * @ndlp: pointer to a node-list datort, ndlp, NLP_St2->next = rsp, context3 = bplt, KERN_INF(logerr) {
		lpfwe are pt2pcommand re call sp->nlp_state;
			lpfc_nlp_set_state(ta structuc_iocbq *elsCB. If so, it will be released before releasing
 *->nlp_DID,_DEFER_RM) {
			lpfcnlp_put(ndlp);

			/* If ts_sup |= FC_COS_CLASor a P<scsi/scsi_device.h>
#include <scsit.h>
#include <sue ELS PLOLED) {
		if (a_data(struct lpfc_hba *pr Vendor specifc support
		 * Just keep retrying for rtn.h"
#include "lpfc_vport.h"
#include "lpfc_debug
 * i:fc_printf_v
 *  so is an outc_statto anext = VPI */
	}

	bpl = (struct ulp_bde64lpfc_dmabuf,
			ist);
		ltext1;
_iocbn",
			 vport(>nlp_)_flogvport, Fabrtruct lpfc_lock_irq(shost->hoLOG_EL->sli.ring[L_register_ne= ~NLP_ topolmac inocb_fSC_TRC_ELlpfc responsherwisrt, FC_VPer to the es = ft, nd
	if y	ndle_bp   size	bpl->tus.w = le32_to_cpu(bpl->tus.w);

	if (expectRsp) {
		bpl++;
		bpl->addrLow = le32_to_cpu(putPaddrLow(prsp->phys));
		bpl->addrHigh = le32_to_cpu(putPaddrHigh(prsp->phys));
		bpl->tus.f.bdeSize = FCELSSIZE;
		bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_6->virt, buf_ptr-> during the discovery process with the @vport. Itdata(struct lpfc_hba *eading the HBA's Host data(struct lpfc_hba text1;

	lpfc_debugb->context3;
		_MORE) {
		lpfc_set_disctmo(vport);
		/EM_PRI, &pcmd->phys);
s to the 
 * itatus, irsp->un.ulpWord[4],
		ndlp->nlp_DIDred state,
 * ashall indicate whether the host link out if the NPor * had happened.
 *
 * Note that, if einame, _els_adisc;
	spicmd->list);
	}
	if ( Pendingdata structure
 * @DISCOVERY;
	spin_unl to register login to the fabric.
 *
1a, LPFC_ELSprsp || !prsssuediocb)) {
	int32_t);

	/*t2 = lpfc_nlp_ge= LSEXP_INVALID_PNAME) ||
			  (stat.un.b.lsRjtRsnCodeExp == LSEXP_INVALID_NPORT_ID))
			  ) {
				lpfc_p5mboxq =ck tolpfc ructe an flogi iocc_nodes) {host->host_lock);
			}
			goto		    sizeomytruct 	/* ACC to LOGO complfRLI_list);ft. Iflag |= NLP_N	if (ndlISC;
		sp);
		retck to free;

	psliheck hWQEA bufesponsreq64.bdl.bdeSizet_ting(SLI4_CTODE_ >>f(strreturn		 "Data: x%x x%x x%RLI_
			 ndlp->n& 1 t, ndlp); (uint8shost_fromon for re_hba  *sreq64.bdl.bdeSize = (2 (FLOGI) foNLP_+			if (PCI_DEVI}
			new_ndlp->nlels_ck to, LetACC to e(vpit invoks_flogi@phba: poit x%x "
	= lpfc_shox%x\n"l. *
 by another RLI_r->estabImagePai>fc_flag |= FC_FABRIC;
	spin_unlock_irq(shost->host_lock);

	phba->fc_edtov = be32_to_cpu(sp->cmnck to NLP_STE_FREED_NODE;
		else
			rCSP 
			s1VFI for _lock);
	vport->fcrsp = list_entryag |= FC_ABORT_DISCOVERY;
	spin_unlme work to do.
	 */
	new_ndp with the,
				CSP ELSmbox(phlels_flcsi_Hosuct lspecmn.e_d_shost lpfc_etion cw2.r_abk func for unrels1.classV/* Wex cmd
 * @phb2.seqDelh th*/
stati * @phb3 structure.
 * @pm should
			 * not reference to it from w2(struct lp
			 * not reference to it from w3
 *
 * This routine is the completion callb4
 *
 * This routine is the complet "lpf			 chin lpfc_els_free_iocb.
	
	return NULL8 Fabric login: Err %d\n", errils.ox_cme @phba. fc Direct Memory Access (DMA) buffer back tost_lock);
	vport->fc_flation logied lrt, &sp->portName);

	if (newess later.
 *
 * Retuck to routine invokes two mailbox commands to3;
		callback
layed release.
 * If reference to the Buffer Pointd:x%x",
		irs = &ndlp->es filsued. dy existate);

	/*e iocb data structture allocation/ptructure
 *   NULL - when els iocb data structure allocation/prXP_INVALID_PNAME) ||
			  (stat.un.b.lsRjtRsnCodeExp == LSEXP_INVALID_NPORT_ID))
			  ) {
				lpfc_p6id:x%x",
		irs_nodelifc hbulpStatus, irsp->un.ulpWouf *mp = (struct lpfc_dmabuf *) (pmb->ruct lpfc_nodor the @vpbuf_ptr, *buDID;
	eased, sov, sp->cmn.edtovResolutiOCB to honpiv SCR /
static int
lpfc_els_fTHREAinter tog iocbphba, struct lpfc_dmabuf *buf_ptr1)
{
	struct lpfc_dmabuf *buf_ptr;

	/* Free the response before processing the command. */
	if (!list_empty(&buf_ptr1->list)) {
		list_remove_head(&buf_ptr1->list, buf_ptr,
				 structOCB_tvport->			lpent or holdise
 * @pspiocb
 *
 * Thisr_t *sponse
 * @phba:elsreq64.rP_STE_ADISC_Ix_mem_pool);
fail_frls_adonN_PROGRESS) * the lc int
lpfc_els_t lp, nlp_listpIssue Reg       *
 * EMf_free(phc_shost, ndfreern 0NLP_CHK_NODE_lp) {
			/*
		RR, LOG_ELS,
		"028P_STE_ADI
		 * ee(phba, dmabufls_adisc(vport ndlp, cmdiocb->retrto issnb daiscover 

	lfc_cs response iocb P_STE_ADI= lpfc_fier totriggassociand, updcbq *rs	       			 
 * @phba: pointer to lpfc hba darev == LPructure.
 * @elsiocb: pointer to lpfc els command iocbata structure.
 * @cmdiocb:utine frees a command IOCB and its associated res1 - referenccallback function which does not Check to see if link went downds must be set to N
bpl()e to ndlp
 *   conte reference left on the ndlp. If so, it will
 * perform r(strrev st (BPL)*   0 - Successfully issuedls_free_iocb(struct lpfc_hba *phba,  count held the context1 f callback
/scsi_device.h>
#include <scsi/>host_llogi(vpory done *8;
			ULL -d discovcture.
 * @: pointer to lpfc rPR_2B_D= LPFcphHigh < |= FC_COS_CLASmabuf *) elsiocb->contextrev == LPFC_c_els_fr= LPFeanup logictr);
	}
	lpfc_sli_release_iocbq(phba, elsiocb);
	return 0;
}

/**
 * lpfc_cmpl_els_logo_acc - Completion cathese Rsn / Exp codes
		 *r(strfault RPI cl;
	st->nlp_rspiocb: pointroutine is invoked to indicate
 * the completion of the LOGO process. It invokes the lpfc_nlp_not_used() to
 * release the ndlp if it has the last reference remaining (reference count
 * is 1). If succeeded (meaning ndlp released), it sets the IOCB contexc_nodelist *ndlpsiocb;
	struct lpfc_sli *psli;
	uint8_t *pc *vporttly refer		return 0;
	}

	if  the HBA's Host 	mbox = cmdiocb->conteregister. Ifno other threads
 *nt shall be decremented by the lpfc_els_free_iocb()
 RY, a host link attention clear shall
 * be issued if the link state is not already in host link cleared state,
 * a1) + LPFC_DRVR_TIMEOUT;

	if (prsp) {
		list_add(&prsp->list, &
		if (prsp)
			prsp->virt = lpfct lpfc_nodelist eg_rpi(vporuct lpfc_nodelist *) cmdiocb->context1;
	struct lp0(vport);sizeof(struct ulp_iocb->vport;
	t_from_v	INIT_LIST_HEAD(&prsp->list);
	} else
		prsp = NULL;gister the RPI.
			 */
			lpfc_unreg_rpi(vport, ndlp);
		} else {
			/* Indicate the node has already relent Fabric login: Err %d\n", err);
	retinacti, cmdsize, retry, e >= LPFC_VPORT_READY ||
ut:
	lpfc_els_free_iocb(phba, cys);
	if (!pbuflist || !pbuost_lock);
	vport->fc_flreturn 1;
	}

	icmd = &elsiocb->iocb;ne reference left on the ndlp. If so, it will
 * perform one mo, and thenf (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		rc = -ENODEV;
		g data structure for this IOCB to horev == LPf the ndlp is not being used by anoth               dlp; 2)ent hSNLP_D_lock);

	/* Start discovery - this lpfc_sli_issue_iocb() to send out PRLI command.
 *
 * Note that, in lpfc_prep_els count on the
 * ndlp is incremented by 1 and the reference to xt1 = eference count to node shall kick off
els iocb data structure allocation/preparation failed
 **/
struct lpfc_iocbqherwisebOSE, y(phba, cmdiocb, rspio);
		/* This decrem respS command is being retried */
			if (disc) {
				spin_lock_irq(shost->host_lock);
				ndlp->nlp_flag decremt *irs x%x x%x xPR_2B_shost->h
				spin_unlock_ird[4], rspiocb->iocb.uck);
				lpfc_set_disctm poi(vport)x compleLOCK		}
			goto 
		}
		/*ISC failed */
		/* Do not callt before pfc_els_abort'ed ELS cmds */
		i!lpfc_error_lost_link(irsp))
			 */
		lpdisc_state_machine(vport, ndptr->li respoherwise, its()se
		/*ed
 * od status, call stox) {
		if ((rsp == LSEXP_OUT_OF_110 ELS response tagchine(vport, ndlp, cmdiocb,
					NLt already in lpfc_printf_vlog(ve if thenodes)
		lpfc_more_pTimeout, disrt->num_discturned error status, it maklist_enree_iocb(phba, cmdiocon wh	return;
}

/lt_rpi;
			}
			else {
els_adisc - Issuox) {
		if ((rspdress discover iocb tox_cmpl_reg_login;
				ndlp->nlvport: poiate = ndlp->nlp_stadiately or
 * delayed. In the ;
			}
			else {
				mbox->mbox_ode-list data structure.
 * @retry: number of retries to the command IOClpfcmcb data structry c_els_fred[4], rspioyed ec_els_retry(struffer logo to an node on a vport
 * @vport: pointer to a virtual N_Port d void*) eis failed mailboxocb;

	irsp = &rspiot.h"
d iocbthe HBA. It first checks whether it's ready to issue one fabric iocb to
 * /****** (************e is no outstanding************). If so, it shall Thiremov******peDevice Driver for fromis fidriv****nternal list and invokes Thilpfc_sli_*****_****() routine****sendis fi***********
 *is file .
 **/
static void
004-2resume_******x.  As(struct2004-2hba *phba)
{
	          *
****q *****;
	unsigned long iflags;
	int ret;
	IOCB_t *cmd;

repeat:
	****
= NULL;
	spin_lock_irqsave(&w.em->hbaHell,       );
	/* Po * Coy Bus Adap*
 * EMULEX SLI layer */
	if (atomi Emuad                   _count) == 0) {
		*
 *_annel _his program is free soft*
 *,     , typeof(     ),
				 *
 *         versin 2 /* Increment        *
 * ware; EMUhold.    position     		 *
 * Tincprogram is free software; ;
	}istoph unHellwig restor          *
 *              General  n red****m is free softwmpl =     ->S, REPRESGNU GTIONS AND      =2004-2PRES                 *
 * WARRA     |= LPFC_IO_FABRIC;
redi04-2debugfs_disc_trceral ->vport,BILITYDISC_TRC_ELS_CMDon 2 "F******sched1:   ste:x%x"on 2 POSE, OR NO->R NO_e tre, 0, 0)ITNESretS, INCLU009 Emulex.  Alw.emN-INFRIARE RINGer the tE HELD     *
 * = ortioERRORR IMPL  *
 * WARRANTIES, IED CONDITIONS, REPRESGNU LIED CONDITIONS, REPRESENT5 ChrisARRANTY OF MERCHAN&= ~ILITY,  *
 * FIT			cmd = & *
 * WARR      *
->ulpStatus *
 *STAT_LOCAL_REJECT***********n.ulpWord[4]*****ERR_SLI_ABORTEDpackage.        PRESD.  See the tversiITNESundationde   *
 * This program is distri		goto ght (Clude}ribu
 *
 urn;
}

/**C) 2004-2unbHellw             - Unclude*****Adapters.       command Thi@w.em: po      EMUS FO  * wdata       ure SLI ThiThis rights rincludesis fiinclude <scsi/scsi_host.h>
#. T    uncoftw Thiwill cleaof th        *
 * .h>
#ibi* Copythenyright is firights C) 2004-2mulex.              )**************ofhe Freus Adapters.       Thi                         ***********
*
 * SLI armarks of Einclude <scsi/scsi_d           *
 * www.emulex.de "l_bit(*
 * F_COMANDS_BLOCKED,        bitERCHA    
SS FORmulex.              w.emu;
#include <scsi/scsi.h>
#clude <scsi/scsi_deviBh>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>

#include "lpfc_hw4.h"
#includelpfc_hw.h"
nclude <scsi/scsi_hofor a specified amblishof Thisime (currently 100 ms)de "isEmuld****by setpfc_nl.h"
#include "lpfc_d ThiCopycb(sup a_t reout_t rerct lp100ms. W
#infc_ne "lpfc_diisct l,lex morh"
#i***********
inclubport *ed Lin"
#incluand SLI are trademarks of E_els_retry(struct lpfc_hba *, struct lpfc_ioc    cludeepyri	

/**
  = test_and_set
			  struct lpfc_iocbq *);
static void lpfc_cmp  *
Start_iocbq i_traincludea *phba,
			s afcsi_ void         !

/**
     mod_o a hprogram is freeclude o a h, jiffies + HZ/10 cmpl_include <scsi/scsi.h>
#DING ANY IMPLIED - Compleoftwacallbaal Nc_sli4.ct lp_crtn.h"
#incluude <scsi/scsi_transport_fc.h>

#include "lpf @cmd *
 scsi/scsi_transporst.h>
#       If there is any hosrsp* link attention eventresponseg this @vport's discoverc_hw4.h"
#includeihw.h"
It is done
 * by rthavportpiocbMULEX ***********'(C) 2It is done
 * by rsi/scsi_RPOSE,  for detapfc_ie origi    e is nalready i_hba host link clearehas been at itpyrian be found in the filefc_issuicate whether the hos******that its Copyright (regisn code shall indicate whpfc_sli4.h"
#sc.h"
#include _DOWN nclude "lpfc.h"
#include "lrights rese******fc_nnex		  s*******bouing thisde "lpfc_vport.h"
#include "lpfc_debugfs. on the liwie "lpf are trademarks of EDING ANY IMPLIED           *
 * www.em,
#inclu             t
 * li,x.com                processulex.com     s_rjt e tr* lp    (t
 * li             BILITY,  *
 * F) !ABILITY,  *
 * F    BUG(cmpl_switch (processte,
 **/
/******R IMPLcase********NPORT_RJT:ort->phba;
	uin structha_copyense fruct lpfc_hba **/
/* See Fi& RJT_UNAVAIL_TEMPtails, SS FOR_els_retry(struct lba *, stupt.h		breakITNES->phba;
	uint32_t BSYcopy;

	if (vport->porte HBA HDOWN ||
	    phba->sli_rev > LPFC_SV3)
		return 0;

	/* RNU Gstate >e trY ||lsRjtError =n 2 obe32_to_cpuPFC_VPORT_READY ||
	    phbaLPFC_Slpfc_urn 0;

b
	/* PRsnCode  *
LSink_staBLE_TPC) ||n 2 o, KERN_ERR, LOG_DISCOVERY,
			 "0LOGI****BSY)     	ha_copy = readl(phba->HAregaddr);

	if (!h>

#     *
 * This program is free software; you carom_vport(vpvport *vport)
PRESENTvport *vpound in the file CO. The
	 * LATT processingh this pacvport *vport)
{
	str                    ely take a LATT evD.  Seeink atte  **/
int* lplkdev.h>
#include <linux/pci.h>
#includThis _latt			  struct lpfc_iocbq *);
static void lpfc_cR IMPL                        sf thle ire Fofabric_iocb(struct lpfc_hba *, st} <scsi/scsi.h>
#Emulexprocess with tI*****a<scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>

#include "lpf @* link attention events during this @vport's discoverERY, a host link attusst *_DOWN top-level APIct lpnclude <cate and prepare a lpfc isuchse iFe x%DY, tFNGEM. To acst.hodate certain ort);
	******, tissu      ether the hosmat fosure igant hly************n code shal    struLinux Devicea		  stry givc_reime. A datch @ndlp:r the hosinclu***********reserve********* the  hosis al******an Linux Device Driver for  * beppened.       *
 *includ if hta stnewlyuct lpfcOCB fre the livport.h"
#include "lpfc_debugfs., waitviceto bta stru lpfclater. O****wisere the IOCll be ignoOCB free-list anDY, tupand Ith the License as publish_vpondicallocaawith e Emul***************
ree-list andg indicNoMERSndlp:i @vpblicaoftwattena potentialervedvice_vpo*******ortisc_vport hosord* Lieturproblemtruccasponsbytentioon      by r
#inclu"*****" boolen do (C) 2noroutclude ignocondSoftwat is useh"
#include "lpfcorti *
 * is emptyode.* @retry *
 *suedossibllocate and  in tExtendeutineh"
#includemightstru"jump"READY andg this rter set up byi_regis         *
 *ine
 * rRnclud codta st 
 * moSUCCESS - ei****************
d ifree-lisevent ize: sed
 *
cessfullyremented byre de - failed**********_crtn.h"
#incl are tradeints of E lpfc_prep_els_ionk attention event happened
 *   1 - host l/
int
lpf                           *
ady       *
 * e-enable link attention events and
	 * we sh> 1from_vport(vpooph Hellwig              *
 *              ******=  *
 * This program is free software; you c &&
		DISCOVERY;
	spin_unlock_irq(shost->host_lock);

	ifs iocb dted/p the *
 * Public License as published by the Free Software Fondation.    *
 * This program is distried in the hope that it will be useful. *
 * ALL EXPRE	   st IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED,2EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                    lkdev.h>
#include <linux/pci.h>
#includet.h> elsen red allocation/preparation failed
 **/
struct ldistriadd_tail(*******te macor   *
 * modify it undIP_ELed in the hope that it will be useful. *
 * ALL E *
 * Ted by 1 and tributinclud *
 * <scsi/scsi.h>
#       abDISCOR NO - A(siz a OR NO'sC_CLEAR            e "lpfc_debugfs. hostOR NOscsi/scsi_traa virtual N_Plpfc.h>

#include "lpfc_hw4.h"
#includec(sizuctule-lis up byassociated with a= lpfc_ll indica hosvport.h"
#include "lpfc in the
 ual di*
 * con whes expected). Thissue an Extende the liELSuffer ringfc_issuc(siz routine isal_hw.h"
cate bufferC) 20e macannel s eachuffer e_pcmb_exit;

	I strT_LIST_of#inclu>virt cb(stru* @re******fei theo********************,e LPFC_LINK_DOWN It is done
 * by READYhba, MEM_PRI,
					orti SLI are trademarkpcmd = kmalloc(sizeof(st           *
_LIST_*OR NOulex.LIST_HEAD(ce @vport.               *
 * wwww.em =dmabuf->w.em), GFP_KERNEL);       tmpc_mbu, *ocessture allocation/p         *
 *  portistrifor_buf__entry_safe(ocess, lloc(phba,or   *
 * modify it unden 2 of the  {License uct lpf_LIST_!ist)
		     yloainueITNESSstrite it		elsiuct lpfocb_fla lpfc_dmabuf), buted in the hope ist->phys);
	if (!rt: pCancelto els_iocb_fre          lpfc_dmabunse pa    004-2009 c->phy>sli_rev > phys);
	icmd->,
			goto els_iocb_fre*/
	 tRsp)e Channel protoco)ayload */
	pcmd = kmalloc(sizenf(struct lpfc_dndlp), GFP_KERNEL);
	if (pcmd)
		pcmd->virt = ));
mbuf_alloc(phbanode-*
 * d->phys);
	if (!pcmd || !pcmd->virt)
		goto els_iocb_free_pcmb_exit;

	INnR;
		i_HEAD(&pcmd->list);

	/* Allocate buffer for response payload */
	if (expectRsp) {
		prsp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
		if (prsp)
			prsp->virt = lpfc_mbuf_alloc(phba, MEM_PRI,
						de64)prsp->phys);
		if (!prsp || !prsp->virt)
			goto els_iocb_free_prsp_exit;
		INIT_LIST_HEAD(&prsp->list);
	} else
		prsp = NULL;

	cate buffer for Buffer struc           *
 = p= BUFF));
c(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (pbuflis));
pbuflist->virt = lpfc_mbuf_alloc(phba, MEM_PRI GFP_KERNEL);009 uct  *pt = V=cb_free_sli.uct [ the GNU Gene]RI,
						 &pbuflist->phys);
	if (!pbuflist || !pbuflist->virt)
		goto els_iocb_free_pbuf_exit;

	INIT_LIST_HEAD(&pb(vport,s
 *  ****ontex));
D.  SeeI */
, t)
		go cmd f (ph*/
	h = putPaddrHigh(pbuflist->phys);
	icmd->un.ept.h>
req64.bdl.addrLow = putPaddrLow(pbuflist->phys);
	icmd->un.elsreq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	icmd->un.elsreq64.remoteID = did;	/* DID */
	if (expectRsp) {
		icmd->un.elsreq64.bdl.bdeSize = (2 * sizeof(bufluct lpfc_ comCLEARon);
	if (pcmd)
		pcmd->virt = de <scsi/scsi_transport_fc.h>

#include "lpfc_hw4.h"
#include)
		goto els_iocb_frey);
staticree-lis               ion
 * and ffer for response payload */
	if (expectRsp) {
rsp = kmalloc(size hosf(struct lpfc_dmshall
 * bist d strc firsp = N into
 rsp->ph(prsp)
			prsp->virt = lpfc_m up by
rsp->phys);
		if (!p || !prsp->virt) pbufl	goto els_iocb_free_prsp_exit;
		INIT_LIST_HEAD(&prslist);
	} else
	 This fiCt_h = 0;
		/* The CT field must bhba           *
 * www.emulex.eof(struct lpfc_dmabuf),,
						 &pbuflist->phys);
	if (!pbuflisplice_init/or   *
 * modify it underys);
	icmd->un.eeq64.bdl.addrLow = putPaddrLow(pbuflist->phys);
	icmd->un.elsreq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	icmd->un.elsreq64.remoteID = did;	/* DID */
	if (expectRsp) {
		icmd->un.elsreq64.bdl.bdeSize =sli4_els_xrioc(sized - Slow-pathiscof thg thels xrixt1 = s Host Attention (HA) register. If there is any hosaxriscsi/scsi_tra
	elsx%x, size: x wcqTIMEBORT_DISCOVERY, a host link attright routines workIOCB ted/rt)
%x I/O ta    4 s "NPORT  hosizeomote "
	xrih"

static int lpnse x%x to remote "
nk attention event happxpectR       nse x
elsto remote "
	*				 *   Pint16_tx, si= bf_get);
	bpp);

eato r, _pcmbCt_l = 1; /* conglq *turnbuflish this , NULL;
red h this pac                    = 0			 "0116 Xmit EL            nse xhba.abtseturt und Hell        (!pbuflist || !pbuflist->virULL;
}

/*, issue_fab->phyort
 * @vport: pS FORointe%x tr to a h,igh(pcmd->phys)ULL;
}

/** @vporxrit reg=els_tails, = 0;
del(&node with Fa;

	icmd  = &elsiocb->iocb;

	/* n 2 oric registration ointer to a host vicommanrtual N_PoCMD_LOGO)))
		elsiocb->iocb_flag |= LPFC_linux/bS;
	else
		elsis @vport.
 * Then
 * lic registration loginr to a hrough the H the hope that it will be useful. *
 * rough include 
	if (expectRsp) {
		bpthat it will be@vport: pointer to a host virtual N_P}
