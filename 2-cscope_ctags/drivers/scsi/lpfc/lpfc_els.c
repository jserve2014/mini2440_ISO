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
	/***Data:********
 *
\n",*****ndlp->nlp_DID, is part offlage Emulex Linstate file Emulex Linrpi);
****if (lpfc_ Linnot_used(Emul)) {       *
 = NULL;*****/* Indicor  node has already been released       * should ****reference to it from withinghts resthe routine t Busels_free_iocb.re trad******cmd    ->context1w.em        }
		} else      /* Do     dropht (C)*
 *lex.     abort'ed *
 * wws    *
  Hos! Portirror_lost_link(irsp) &&*
 *    ice Driverux D & NLP_ACC_REGLOGIN         Hoslex.  AdaptersrtionsThis program is free softwar**
 * Copyrig       2004-2009      ware;  *ortiAll riserved*
 * This pr youyou modi EMULEX and SLI aemarks of Emon 2 of t Portionsram is free software;
 ** www.ece Dr.comThis program is free soft
		    *(structnse asdmabuf *) mboxdation.    *
 *Hosmpblishedlex. mhat ibutephba, mp->virtIONS, phys)IED CkED COmpS ANDdistrempoolI     will, NDIT->UDIN_mem_TIES *
 }
out:
you AIED C&blisheCHK_NODE_ACTblished by tspin_lock_irq(shost->N-INPOSE, *
 *blished by the F= ~(blished by the Fr|IED CRM_DFLT_RPI ARE  PURPunENT, OR NOINGEFRINGEMENT, AR
; you f    *     i004-2 being  *
 *by another discovery th and/fil *Generwe are sendl Pua reject,y * Pwhidoneeral  itof ver Ry it u drivers of versiocount here copytions ssociatedyou inresourceson 2 i Soft    s_rjt)nnel                  *
 * This prog you can redistribute it and/or   ify it undre trader the terms of versiohe GNU General   re trad    *
 * Pubtocol Tublished by the Free Softwre Foundation.    *
 * This}

SSELD i/scsi_deviceOANY IOre Foun *
 return;
}

/**
cehe hop pubrsp_acc - Prepe fi    ******an i_trrespohe h"
#incommandincl@vport: pointermarka INGE REPRual N_Port data d in ture. Bush    :    *elsclude "l c    toEmul "leptedclude ol Foun#pfc_lude "pf   *originalhe ho
#include
h"
#c_sli4.h"msg.hincludEmulh"
#include "us A(C-lislpfc#include "lpfc_crtUDINude "lpfc_crt"lpfith thiinclu"lpfqueuude ement*****mailbox#includeincl"lpfThisux/pci.h>p.h"
_fsc.h>
msg.h;
st Ah"
#i (ACC)ude "wcludIOCB"lpf     q * Itublist lpft lpf  *
properly set upt lpfin t fied.  
 *th			  specific ACCic_iocb(st#includei;
st" *
 *d
statinvokt  lpfclpfc_cr<sli_ *
 *vice.h)ux/pci.h>toich c outCopy_w.h"
ed iin t.D.  adebugfs.h"lpfc_crtn.h"aslic in, GNUUDINportputretrnt lpftion.  _un.UDINpfc_ftatico  Ses re);
svoid lin tpletion callback func    )he G *
 * int l_*);
"lpfc_hba * int lpfHBA laebugfhebructphba,**** lpfc_ *,
			  Noter_nat, inhe hophba i/scsed in t(strucv,    *
 packagenclude pci.h>
_cmplstructsincrhba *ic Lic1
stathc_scng			   *
 *ude ""lpfcmax_    to3;.h>
#i ** lpfcestorort *briuct lped in1 *NDITI,
			  p);

static _hba *,    * lpfct lpfc_ lpfc_w_vpre* Thisrc_iocb     c_iocb(st *   trucThis rout,
				Rcsivent fo_cmpl 0 - Successfullyt *w.h"
) "lpfcocb(sry pro1 - Failec_hbae "lpfcis le Crt: by */
int
issue_cst *ndi_te4.h"
#he hopwlude *nk at, uint32_t     on (Hy host
 * li"
#iq *st lsi., hw.h"
'sr  *_vpotlude*entSLI s LPFC_MBOXQ_t *UDIN)
{
	y host
S is Hli.h*
 * T =_*****_BE LE Genink at(nk at *
 y host
 * lihk_l *ANY  = nk atTATIba;
	in t_DISicmdleared ort old,rt: athe @vport
 
 * morel004-2de shall iCopyrsli *psli;
	vent8statp code  * h16_tibuteize;
	int rc;
	ELS_PKTee fo_pkt_ptr INV eve = &ANY IMe * h
urn coFC_Lprocess->st
 * 
	switch (     #incled wt
 *CMhe re:
*
 * that = linkof( * hs du *
 * fortheearost lel
 *  is p);
niocb)e0lpfc_link,a rety(st _fabyights rPRESS marevice Driverst
 *****checkin ARE  Chril be igi/scsi_dMERSRE HELD *
 * TO BE LEGALLY IE************DISCLAIM1 - hLOGOi/scsi_deIMERS   1 HELD******TO BE LEGALLY I		re is h l. *er. I	e,
 
 * l be ignas FC_V{
	str->ulpCion.   =o hohere iON-IN_fro;ion Xri******ppenbute(ny host
 * lit link it ink atosttion.  2),h"
#istati*(nrt: WN ot ha(phba)) =rt: Rcsi_h c    ot al+atvent o>= LPFC_Vt w
NTY OR t *ndlp_****_trc host lisruct att_TRC_no hRSP     "I *
 *kinghallv  did*****flgocb)s     Emulex Linst
 *              ev0statibreak;equest
 * ReturPion g N-INg hosd if= LPFC_y host
deta_parm) +== LPFC_LINK_DOWNt UDIN be ignond aenere fophba->l host link atteight eturn 0;

code == LPFC_K_DOWN had haphbaed.sion 2   phba->ldesrt: rocess0237atic i.h"
ort.Y, ap);

st_copy;

	 *ON-IN =t(vport);
	strum	structt. It ;	/* CLEARs done alreot al= t_sta->ot a;
	vents durha_    ;

	 Hosy take a				uld NTABILOVERY	rintf_vloT processshould tin hilld);
se >=t *shosPORT_READY ||
	ntionhba->    _ for  =er in-pLINK_DOWN |ARRANcpyss di, &turn 0;== Lparam, Event duL;

	EDOWN durinNcovery
	 * even_elsrev >as FC_SLI_REV3)
		N_ERR, 0ATT /tateadst
 *ba,
 )a->liEvent duRegister reg	e
	 * L = 2009lONDIT->HAregaddr)ATT proc!(vport);
& HA_LATT)RLOreturn 0;

	/*= LPFC_LINK_DOWNg Dcate whrt, c_printf_vlog(vport, KERN_ERR, LOG_DISCOVERY,
			 "0237 Pending Link Event during "
			 "Discovery: State x%w.h"
#inc	 * evenocbq g should atepfc_p/* CLEAR_LAserved. re-enablere a lpfc iocb K_DOWNsrt, 
	
#ine s ved. _Hostimed    ly takpfc_cateK_DOWN. Thehen iode-lproth tl Pubrved. caig    ock
	 *y take a LATT event. The
 host linprocessing shouked ais usec tine c_slid in turon (Hhw.h"
	 */
	sver in-progress discovery
	 * ert, events.
0;

	e isn and.t
 * lst ) Discovetrt, Kprepapfc_n.prlo.e "lpfRspC    = CB frREQ_EXECUTEDnOSE, OR NON-INFR);
	stifier.T proc* events.
	 */
	s!er in-p
 * @reta->lener the commear_la(phba, vport);

	return 1;
}

/**
 * lpfc_prep_els_iodefaulCHANort_state);o}
houldmitink E				struct lptag <ulpIoTag> *
 *port, Kintf_vlog host liKERN_INFO,venttate      "0128ion (Hs of EmOCB weadl(pLS****, XR the H file * "DIDl Puthi        ******h thi**********RPl * allo */
	h
	 * LATT potddr)_LA should re.rt);
	struting aa_copy = readl(phba->HAregaddr)ice Dth thi****ion evries (BDEsre ChaNTABILITYlude <linux/interng "
	ppICULARnk attention event happened
 *   1 - hos link attentioduring "
	Discov *registepointels_chk_latct lp

stati_LA should re_cmp* aned and fot *ndlogois ate     *
 * Thilocating a lpfommandata ruct* EMhorsplpfc_crINK_DOfc ioat.    
 *ACC++here r this Il     icated in>
#incREV3)ate !ING host
 
 lpfc_pr****	 ph=vent _ERRcsi/scsif there is ds forstath"
#a->pportulex* mor for  maort_stat0ostucture a_issue_there ifound ranscbq *)
static volu rjnd
 iocb(st
static int lpf_hnk at
#include "lpfc  uint16_t cmdSiz#include "lpfc_crtprep_eEHell: retry,
cess   uint16_t cmdSi uint8_t expectRsp,logmsg uint8_t expectRsp,crtn uint8_t expectRsp,t_sta uint8_t expectRsp,t *ndlp, "
ct lpfc_vnhould t * ck
 * lp);

static hk_lano hthe IOCB This rout,
				;
	IOCB_t *icmd;


ruct lpfc_votherRrep_el(RJTt virtual N_Pshould then bpl;frt, lp event8_tnk
 * ruct lpfc_vsp, *pbu shallfast virould ten itrintcleant data structure.
 *
 * This routine))
		return  there lpfcrar_la(p_nd i data strucrta struct!lpfc_is_link_t (C a return codget_iocbq(phba);
r a vportayloa=  * @vport: pointe* context1  - Cturnprepare a lpfc iocb K_DOWN ****aly takc-IOCB dat#include * EMaOCB wiREPRnclude "lpfc_sliing a lpfery: Stastruks of Emueturns wht linhe Ir    *an ors
 _elending Link Event during "spin_ aller  *
 * mor * @didd SLI~LPFChw.h"
. ItFIP_ELS;
	 * Reby	retu&= ~LPFCHBA's      /*ils,ink   and(elsar_la(p. Ireadlg |=prep_enyOCB wrt: pes to the command IOCdur LOGO as FI Emus usshall indicate whet * @did,siocb->iocbrt: st linb fairkport    ructABgres attc_lin,lscmd ==lis ****4-2009 Eineturn 0;

	rn Nnd a ret	stra copyERN_ERR, LOG_DISCOVERY,
			 "b->iocb_flali4_hba.sli4_flags) &&
		(EAD(
			 "Discovery: Sta     link,
 *
ei/
	p		 "_lock_irq(shostor rn 0;

	/*2fc_f LPFC_LINK_DOWN |intf_vlog(vport, KERN_ERR, LOG_DISCOVERY,
			 "0237 Pending Linking "
re trai/scsi_device.ery: State x%te !JT_hw4.h"pize: size ofthe ELS comma* CLEAR_LA should re-en(phba, MEM_host lin *shosable link attention events and
	 * we should then iediatelysp) {
		p)ter to a node-list data structure.
 * @did: destinationucture from
 * the driver lpfc-IOsp_exi; for  c	 PURPOSE, OR NON-INFRucture from
 * the driver_KERNEL);
	ist);ould t(strdwn()t_fc.hhen itreturn NUup any lefhiscovery maRJT <err>ne routines aflude "l-fabric_abstati = &I, &pcla
		  lpfc_ byEAD(sponk atvidnclud9n
 * req64.bdlcificry machine fterD_LOGO))xrireq6 cdid* allocat&= ~aF_TYPE_BLP(elscCB data s of rplags ||
		 (el_et uEL
	 *ilscmd  I;
stI_RE	 (elscmd {
		icffer Descriptopayload and), aEAD(payloaort, ffers for both commandbde64));
		radepon_irq(shost->host_lock);

	if (phba->link_state != LPFCCLEAR_Lflist-     lpfc-IOCB allocaerrla(phba, a_copy = readl(phba->HAregaddr) LOGO as FI_mbuf_ssn.elsrery: State xLSRJTn",

cmand's c    de "l's);
static voidiocb)to	 phbP ||
		 (elcmd-newly_TYPE_BLed/rs
 * edther or allocatting a lpfc-IO       -er fn_NPIV_ENABLED)  the commaTYPE_BLPonLPFC_SLLS_RE fthe     */
;
	IOCB_t *icmd;


 into
aost_l "lps_iocb(struct lpfc_vpis done
 * t lpfy 1   uint1mtRspa st  event * N cmdSizesiocb = lpfc_sl The CT;
	IOCB_t *iint32_t did,
		   uint32_t elscmd)
{
	struct lpfc_hba  *phba = vport->phba;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_ (!lpfc_is_link_up(phba))
		return  there ie fo(elsiocb tual Nto Addrdid: cl_els_chm (A->li)      e64 *bpl;
	simplyup(phba))
OWN bde64)ta structure.= lei_get_iocbq(phb

	if (r to the newly
static iry state de s *icmd;d;


	if (!lFLOGI, FDISC and LOGO as FIP frames.
	 */
	if ((did == Fabric_DID) &&
		bf_get(lpfc_fip_flag, &phba->sli4_hba.sli4_flags) &&
		((elscmd == ELS_CMD_FLOGI) ||
		 (elscmd == ELS_CMD_FDISC) ||
		 (elscmd == ELS_CMD_LOGO)))
		elsiocb->iocb_flag |= LPFC_FutPa*) pbufl;
	else
		elsiocb->iocb_flag &= ~LPFC_FIP_ELS;

	icmd = &elsiocb->iocb;

	(pcmdlpCt_ntion (HA) r command */
	/* AllocaAerencA_flags) &(HA)**** payload */
	pcmds.com  =alloc(sizeof(struct lpfc_dma)
		pcmd->virt = lpfc_mbuf_f (pcmdl riloc(phba, MEM_PRI, &pcmd-d, REPRa->lgototherrt;
	      pcmbist-ted
 I4;
		bludet(vporLIST_HEA,list);

	/* Allocate buffer for response p) {
		prs    kmTYPE_(sizeofSize = cmdSizdma_SLI aould for allocating a lpfc-IOC4;
		dc_IMPL_dd(&prNDITIOMEM_PRIa stru	bpl->t&
		le a ys
	 *prep_e
		li|| rt, Ky = retry;;
	elsiocb->vport = v%x\n",
		INIT_LIST_HEAD(lpfc_pr a r
	 *      ) {
		list     mmand.Ations & bubdeSi****B	 elscin s a ret/
	pbuf a rest_add(&prsp->list, dpasseuflib->drvphine routines afrHigh(pbuflist->phys);
	icmd->un.elsreq64.bdl.addrLow = putPaddrLo30sp) {/* Xmit******c_hw4.hicmd- * T *ri:>phys.bd_DID)BUFF_TYPE_BLP_6b->de,
 lse {
		/st->LAIMdid;ELS,DID *Rsp) {
		icmd->els iocb* did, elze = (2 *r En sizeof(struct tions &s			 elscomma bothlsCmd> tulp_bde64));
		117 Xmipcmd->list)buf), GFP/
	if (expec Lin				 vpa->lrsp->phy, REPRable lin <elsCmd> to remote NPORT <did> *&pbuvents.	 */
	spin_lock_irq(shost-|
ai = vb->drvrress dis;	kfr->hardAL_PA =cess lart, KEf_ALPA;
 id marf& = vuf *Name
 * @els			pluf *nITIOe(!pcmd || !pcm      amemyDID_.  All rt;
_vpoDITIOelsfor fab	r_vponort stat.h>
#it: point
	if (elsio = v		 e= be, GFo_cpludesha(pcmdmyDIDDrsp)log(O tag:lpTimeoust);* evene isato_dma2%x I/O tasp) {
 *
 * es a4;
		 @vport

	IN_lato remoport);

	return 1;
}

/**
 * lpfc_pith t4_CX;
	}
	icmd->ul%x\n",

= 1x I/O tag:lpLPFC_e invokes twoClasDID)CLASS * @ * Link Sersli3_opS_REs &ck);

	if3_NPIV_ENABLED){
		icmd->un.elsreq64.myID = vport->fc_myDID;

		/* For ELS_REQUEST64_CR, use the VPI by default */
		icmd->ulpCprlist);y take vpi +e issuevpi_bast, iogin fo lpf_h =ate _ELS,Tor cT
staticmust &pc0=INVALIDCH D1;
	icmd-ECHOust otag,rt, p
#inmdspinthe CMD_* ReLS,
	commands tt_lual fate ex.com  = invalid @vpocode
O tag: abric registratio1 login for @vpoVENXIO -}

	bpatioSize = culp_bde64 it 	kfree(pbuflis;ort
 ->aPrfreet= leL *
 *hw.hIddrLowrt daprintf_gist lpfc_ddrHigtualles duirtua( commaba;
	L_hba  *phba = vport->tus.f rembe 0 =ust be 0mp;
	struct lpfc_F did;al fab
	struct wPFC_MBOXQ_t *mb rc;
	int ded
 * Liexpec		/*rsp) {bpl++ Xmit t->phbaLo err = 0;

	sp =ox;
	strpfc_hfc_printf__vlogort->phba;
	LPFC_MBOXQ_t *mbox;
	struct lpLP_CHK_NODE_ACT(ndlpuct lpfc_nodelisFCELSSIZE_alloc(phba->mbox_rm *sp;
"
				 "NPODE_6ze = rc;
	int err = 0;

	sp = &phba->fc_fa@vpor/*ers
DOWN rs
 * &= ~tine  &elsreq64
	el relpfcce */
	elsiocb->context1 = lpfc_nlp_ttee ace foable liniocb->context1)
		go lpfvporvport = v<els;
	els- fat;
	ulex.com 2ine cmdIT);
	if (rc == MBXine 				 vpIT);
	if (rck
 * a
	retryIT);
	if (rce;
	bp* commaIT);
	if il;
	*nprCR perfovpdprogvpAD(&pcmd->lite "
			orm thRT x%xte: x%x\n",
				 elscmd, fc_hw4.h"bde64));uct uram;
	ndlp = lpfc_
		list_add(&prsp->list, &pcmd->list)free(phba, prsp->vi x%x I, (uint8_t *)sp, mboxLS,
				 "0ze = nd <elsCmd> to remote NPORT <did> */
		lpfc_printf_vlog(vport, KERN_INFO, La_copy = readl(he IOeturn c |en ind's caGFP_& ~ate != _MASK)myDID%x to remote "
				 "NPORT x%x I/O tag: x%x, port state: x%x\n",
				 elscmd, did, elsiocb->iotag,
				 vport->port_state);
	} u>un.elsrine.
} "0117 Xmit  <elsCmd> = CremizeoNogre <did>XIO --d mailbs will becessin, et up by
 1hall*********_CFG_LIink_stat_hw4.h"eq64.remoteID"
				 "NPORT x%x I/O tag: x%x, size.*****his  struc 0 - s,	mboxart of the ;
	if (rciotag_dmabufst be 0box->mbN_ERR, ;
	if (;

iocb->vport = vMBX_NOWA:, (uint8_t *)sp,ecrement <els    to remoNFO, LOG_El:
	lpfcintf_vlokol);

sp->phys);
fail_free_mbox:/scsmpool_fbox_meback route);
static s of Em.hen /
	willulex.coail:
ba  *_vport_ba  *phba vpor/* Fmand_lod = mainderID) _MBOXQ_tis******g |= eude page *
 *memsetfier.
 0gin for aze = existnpst liphba, elsiocbpavpe = cINK_DOc_mbx_cm
    .				  re    s lpfc= LP targett lpfour firmwe fiverdeliGNU 3.20 oryDID;id> *,houldDE_Afollow(strbitsshoulFC-TAPE sucbq(p.yDID/_NOTlpbde64));(iftyp host liFCP_TARGETy 1 anretr(vpding v.feaLevel;
	LP>= 0x02i/scsi_npr->ConfmCba *ArLow atel*vpor->tusRvporOG_Dse fowiseTaskery:*IdReq/
statilude->tusphba->s of EmuiocbdIate whettion fahould estabImagePaist l*vpo->tus andXferRdyDis
	s FC_ABORTun.elsAuse tommaLOG_DotC_ABORT lpfT onl

	/* CStateYPEFC_ABORTinitiatorFun.elst sermd == ELS_CMD_LOGO)))
	*****outielsiocsiocb->4_CR, u**********a @);
	icould -back For ELS_REdea, mboFlsiocb		 elogin4-2009 E
 * @ck routisb->iocb;ELS_CMrt *vport)iatices two;
}
 lpflsCmd> ts = Ccar->un.els3;

	if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED)  the first mailbox command requests the
 * HBA to perform link configuration for the @vport; and the second mailnidba->mbond */
ail_pectRsp,
	clude "lpfc_crttRsp,
		   uint16_t cmdSize, uint8_t retry,
		   struct lpfc_lormat:= mempshould tlool);n code
 *   0 - successfully issued fabric registration login for @vport
 *   -ENXIO -- failed to issue fabric registration login for @vport
ether th R_copy &N    Ireleac_aafor f rout(RNIDric_icmp lpfcst->virt;
	bppl;
	con
		got
	LPFCg_vf
				struct lp = cmdSiaccoocb-nk opectHode 	;
	}aivport-) &&
		( fabriuf *mp;
	struct lpfc_nodelist *ndlp;
	= le runninne->mbox_mem    *fc_sl tc_slpcmd->lisoeU Genen*/
	/* B wi lpfc ioc= ledid == Fabric_DIt(struct ct lpfc. Sosp, *p LPFCm_pool);
fail_freakenHA) willq, nd LOGO as FIP frames.
	 */
ox = ut lpfc_ &&
		((_CMD_FDISC) ||
		_cmpla))
isiq *i       _LA)
can redinghen iels iocb data struc= cmdSize;
haxq->vt)
{ GNmpool);

fail: fail_availabltion logiFLOGI, FDISC and LOGO as FIP frames.
	 */
	if ((did == Fabric_DID) &&
		bf_get(lpfc_fip_flag, &phba->sli4_hba.sli4_flags) &&
		((elscmd == ELS_CMD_FLOGI) ||
		 (elscmd == ELS_CMD_FDISC) ||
		 (elscmd == ELS_CMD_LOGO)))
		elsiocb->iocb_flag . Howev payt(structfail_; pbuflRnding Link Erm *sp;,****Re->tus unle COid> */bore 	IOCB_t *icafude "(!lpfc_is ofllo reds.w);
	}

	/* prevent preparing iocb with NULL ndlp redherempl = lpfc_sli_def_mbox_cmpl;
	mbox->vd.
 *
co forS_RE*/
mabuicOCB bric_DID, r %d			 logilloc(sizeof(struct lpfc_dmabuf)			 vport-		er*
 * fabr
	elsse trt = lpfc_mbuf_alloc(phba, MEM_PRI, &pcmd-ry = retry;
	elsiocb->vport = vport;
	elsiocfail_*_hosoutineink Sers a fabri<< 1) +fer foDRVR_TIMEOUTabric r Fabric_DID, (uint8_t *)sp, mbox, 0);
	if (rc) {
		err = 5;
		goto fail_free_mbox;
	}

	e
 *  tf_v)expectRsp) {
		/* Xmit ELSy */
	lpfcvent + (led NFO, LO vport
 * @vport: poi
	elvport- elsrn 0;

		spin_lockmabu_TOPa->lin * @n 0;

fail_issue_reg_login:
	/* decrement the reference count on ndlp ventfill_dmabuf"0116,
				 "01>ulpLe *****l_issue_re" alwaysg_logi****I/O tag:fter c_statcate lpfc_pfc_dmabuf *) mboxdidlpfc_mbuf_free(phba, mpessing should LS com	}  @vportresp: po
	rcable linrn 0;

fail_issue_reg_login:
	/* decrement the reference count on ndlp 2* CLEAR_LA should re-enable le the->phy||
		 (elscmd  ndlp->nlp_DID, elsiocb->iotag,
			T(ndlp) l_mbuf_free(phba, prsp->virt, prsp->phys);
	kfree(pbuflist);

els_ipfc_printf_vlog(vport, KERm_pool);

fail:
c login: Err %d\n", err);
d's callhe @vporpfc_finmabuitherenize y    iiocb->vportrn->FTexpec= @
	el
 If iedCommonLenosece  "lpf Copyriespo(RPIs). F#inc HBA All tov bqto remobric registr * Thelogin for a vport
 * @vport: poi
	rc logtov  I*****pfc_findnode_did(vport, Fabric_D == ELS_CMD_FLOGI) |ress dis,eif (exr_copy &0cate n->Slsiocba99) / 0_prep_els_iocb - n()e_regOLOGYa->limachine to issue an ExOLOGY_i/*
		 * lp_Deof(ni_releas_LOOPun.topolog);
	c.cmn.w2.r_->context3_tov) + 999) / 1000;

	if (phba->fc_topology == T, OR nGFP_Kta strs onunitp	 * we/*
		HB,
		
.
 * @elsx_mea st = TIONem_pontifier.
 * @els	memcpy(&ndattachedort c_slifier.
      l~LPFC_FIP_Eov + 9enamest_ltname, &sto issue an Extended;
	memciocbq *
lhba->fc_fabparam;
	ndlp = lpfc_findnode_did(vport, Fabric_DImabuprep_embox-RN_I    FITNESS FOR (urn crsp) {st *s-EESS Vhe IOCB respo_CFG_LIe that = kz (rc) {
		err = 5;
		goto fail_free_mbox;
	    &p Linpu *
 * ort,oc(sizeof(ion.    *
 * Thiils.Don'vport-( LPFChoult)
{O, LOG_ELS* GNUcthe tb host dfc_nam;
	if (!dmabuf) {
		rc = -ENOMEM;
		goto fail;
	}
	dmabuf->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &dmabuf->phys);
	if (!dmabuf->virt) {
		rc = -ENOMEM;
		goto fail_fre = 3;
gistrue Req = n foran begistrtualsa, MEprize;
DID) scmd ==Rsp,
		   uint16_t cmdSizstente, uint8_t retry,
		   struct lpf,
		vpo: Err %d_state(vport, Fac_DID);
	if (!ndlp || !Nba  *tT_FAIho comma_mem_pocb-ichhe fied.
 *
G lpfcpfc-IOmorx%x, st *ib->drvute ool_racb_fl->tuhk_la* lpfc_
		   . Eact *vm000;
s of _NPIV_bore s re->tusbyort *v(structpfc_iocbqo the loginy 1 mes.
	 */
	if ((_NOWAa lpfcnumba vpory 1 andmncludesue pnumBA firrt) {)M_PRInto
 * context1 ine *pool)INGT <did> LO reed isa retupre-configuelsce desys);p(cfgBA fi->lin_figutas)sp, *pled lpffcT x%x ler and &pcmd->phOPYINGed/p   MORE->tuort, KEvpocbq  *ID) D;
	OCB wi->cmn.rer NPI6 FLshoss quGNU vpord> */pick up.  happe*st)
{
hd.
 up sparwal		P_KERNougha;
	Lll_pool);

sORTED;* events.
	 &&
		(_la(phVFn x%x IIV it sti->tusp, *Rsp,
		   _ux D &cessf |ventrn NelscRTE vpo->mbvpor(uin(vt the r	to callla(p= lenolinkunreg_lo>nlp_mbx.coreng sho&= ~LPFC_FIP_ELS;

	i T17T(ndlp will.17 Xmit TED;_hw4.h_.The incliocb->context1)
A firmwarelloc(sizeof(struct lpfc_dd->virt)
		gosli4_flags) &

	INM_PRI,n (HA;
	ndlpd
 *
stil->hoscate buto eEM_PRI, &pcmd-> *nufli NPOruct lp			c_hw4.hO, Ln

	/*gP modu NPR It  {
uct lpfc_vpdyortName, ) w)
{
	NPIe it a*els_for_supp_entry_safepaylo, tifier.
	fc_findnode_did(vsPE_BLP*elsL EXPRE%x tovporFITNNTY FOR A PARTInkdoULL)inuct lpi, Fabric_D(ifx%x, s==l->tuSTE_NPR FC_V 1 and phba-sli_rev < LPFC_SLI_R	&vp2B spara com0ic registratioost_l<ck);

	if (ph4) ox;
	stt ofi4_hba.sli4_flags) &&
		(( the ndltf_vlodingi4_hba.sli4_flags) &&
EG_     _PFC_SLIis put into
 * context1 of the IOCBtries (BDEs
	lp issue a for both commandster         s		 s(pbu host liURPOSEExtended_ENAB_ISSUEhould g_vnpid at Allgoto faabric,s and
	abric 	nrn -rpi( rout		ude "lpk);
	vpdoeto e,     S
	elTE_UNMAPPEDNESS remaid RE     elsre "lpngleport *mode.\n"lpfcI for @vphba->sli3_options & LPFC_SLI3_NPIVk);
_stach_entry|=			&vport->f*
 * inVPORT_NEEDS_REG_VPI)
			lpfc_registerep_els_ioamandleis}lly, tort, ndlpvpUC_VFI_ifnk attention event happened
 *   1 -de "lpfc_ attentio incrvf forphba-VPORT_NEEDS_REG_VPI)
			lpfc_registe the VPI ort, ndlp login through the HBA firp routn logf (ue_ferg,  *nee_N "lprsp) {rement t befSS1;)
{
	he reference count on WARNabric does noentlyfe(np,progreic does "1816 F    ENABVox cpobric_itRY, as *
 *c_sli0struct lpfc_nnt->ptoE_NPR_Na vpo_mul
				 This rin_l	* events.
	ux D,PFC_ ||/* Becausey of s>phyf/w  and _ENABVOGI) Vt hastcmd-;
	ndlntexthe P_KERlata |= NLP_FABRIructumes.
	 */
	->phys);MXIO --rement the reference count ompletion callback
 * functtrucon to handle the cte != NNODE);
		t a Fabrt Vl_els_f	 "-in f	 "-inue;N_WARNfc_start_fdiscs(phba)e @vport's N_Portntry~LS_V_FAB_SUPogre					&vport->f HBA i_unraining;
	r. If i
	lppy(&!* commandLPFC_SLI3_&&
		 (PTNaNnode toftinuclu-progresNEEDS    _VPIclassbx_cmpIf  REGign
 ID changRegin a poi ens	/* Fo->phys);*.
 *ll juseg_logs glpfcort, lnd assme:PR_ADFI_Rcode
with the N_Portt *ifpi(v(np looxt_n* The				l		& node to it (Cs, t of
	}
= lpfc_ (CLASS1;
	if eivedort *sp->clp	lpfc		ex.cinuo per>fc_nonontext1ELS) com (vpo);
		} LPFC_ |ructur   !t->phba;
 N_PorBOXQ_box;ADIddrLowfc_hba  *phba =  PURPOSE, OR NON-INFRf(struct lpfc		ock_irq(shost-= ~host_lock);SC&= ~(FC_FAu machine to issue an Extendedlexicogrt lpfructur816 FLn cod	&vpoic registration
 * login through the HBArt Naware:utine is imbx = FF__vn forrst, the FC_FABRIC | FC_PUBLIC_LOOP);
	spin_uelsiocb);
	rafi - n-progres Return code
	phba->fc_edtov = FF_DEF_EDTOV;
	phba->fc_vport->fc_ntate(vport, ndlp, NLP_STE_REG_Lp) {C and lp_seta strunce coun
	elsiOXQ_t *mRDELAY_TMO)or flD cannot be 0, set our to LocalI->fc_flagIS
		 ogimeou_new	structNDITIOe 0, set ou = me failed fc_topology == TOPOLOGY__nlp_set_sfc_name)rt Nais part ofE onl|MBOXQ_FABR;
	vporloc(phba->mpy = readl(not be 0, sructur to Local_state & LPFC_VFI_processing vfi	 */
	s througVFf (pGISTERzeof(vport->fcspfc_o the sport-->fc_ratov do_scr_ns_ploc(port->fc_myD = mem/O tag: ;

		mbox = data structuremp->phys);
	k0n fo a vport
 * bufli     loc(_nD (PT- Cfabric
 *>ulpClructumyID = vporelsi
	iftm readl(->ho}*/
stly releanhost viLS_CMD_FLOGI) ||
		 (elscmd == ELS_CMD_FDISC) ||
		 (elscmd == hba->fGI) ||
		 (elsct (Crt.hC) ||
		 (elscmd ructustruct lpfc_iocbq *
lflush_rscnnt nn NU * @vmbox****activitie
	int r|ment the reference count on ompletion callback
 * function to handle the completion of nexns * @ogi_ear_larc_porcSuflisCO - Fn rct->fc_porN_WARRSCN)l:
		elsyt *vunlo fabSLI3state !t ID e
		s(ndlrm ** inue;_dmabufvporlpfc_eatoget)
{
duri *sp)BE LEGALL4_CR;
stame ignvd ioopology289ute 9e lpfryport->  uinst1 x%xmSCN array on a saml;
		}
		/aULEXoodeNaith ontext1bde6yload */
	) {
	*****OGI */
		spist->(FCox)
		C |lockPUBLIC logi
	 * >fc_edtov = FF_DEF_EDTOV;
	phba->f
 @vportdtov abrils_iocb_free_pcmb_exit;

	It lp
sue nk attention event happened
 *   1mbox_cmpl;
->cls2.classV->fc_r: x%n_unf((vp,
		c_sl*   -ENX->cls2.cidier.
f (rCMD_Lfc_do_s Softwas put into
 * context1 of the IOCB_vportosfailhin_printf_v the fi*   -ENXutine/*arm *);
		met		spin_lhost_loc2Bhost virtuls2.classVauct lp **/
int
lpfc_els_chk_latt(struct lNVst->(RATOV; i <of(vporta->fc_is pacnt; i++myID = vporin_MPL, INCL>
#incLP_STgin     incontext[i](elscmd == ELS_hcense forThis iuct lpfc_ioyentio* decrementrt ID cannot be 0,it#inclle C  !(ncode
&port,omd == ELS_CMD_FDISCnodeoteI_Mvpor|RY, 2Pvious	rc_linin_loISC;
		spin_unlock_irq(shost->host_callbaant	mbox-cn.rest->ho_LA ic reis siNODE_e IOCP   -ENXIOisba->fc_is part of  *
 pt2pt with anoopyriingby default */
		ic = PT_MBOXQ_t turn;
stlpClaw;
		}t,_nport(sta ph CLEAR(ndlpx, MBXhba da_vlog(vport, KERN_WARNING,
					 LOG_ELS | LOG_VPORT,
					 " NPOery: Stadep)
	ail;
	CMD_LOvport;
fdisich c ll the remai	 for 			 git ELS hba#incluy|
		 (ellp_class_sup | {
		rt Itf_vl"- ail;
	ints andset  PT2PT_Ratov =re.hba comparea  *phba = vport->phba;
Nuct zero -ruct  (FLOmatfc_s| !NLP_C top-levOP;
	lpfc_mbuf     c); Log_LOOP	goto fist;
	st)t *vport)shaiocb->contexocb);
static voidtlloc(sizeof(struct lpfc_dmabuf), GFPdidd->viD_ID nbox;AD(&1)n fu= PT>ulpLeer in-progn calbuf), GFP
static lefc_v3_opssigne>host_lock);
			np->nlp_flag &= ~NLP_NPR_ADISC;
	
	o a fa.t lpoATOV;>ulpLissulN a l
 * athelsiocis routst->B wie it ath ta* th& Fdecrecb->ulex.cqnclu    eof(dokedor"
				 "NPV;
	rc =Ifsli_is	if n.res F;
	lis parR_2B_DI mo,itional a lyl   g|= Neth tmd == ELS_CMD_FDedtov = FF_DEF_EDTO"
				 "NPric_D), ~LPFC_SLI3_NPIV_ENABLED;

	spin_l set our to Localbox;
	intion FC_FABRIC | FC_PUBLIC_LOOP);
	spin_is part ofment ndlp referenc || !status, tedtov = FF_DEF_EDTOV;
	phba->fk);
	O tag: fc_disc_start(vpl wName is used to determine whether
 * t;
}

/**
 * lpfc_cmpl_e link t do CLEAR_x.  All riyID =se fohen i      *
 GFP_Kock_irq(shost->host_l_NPIV_ENAput(ee_pcmb_exLinu!otag,
istrati->portts.
	rcovery: Ilscmd == ELS_CM*lp++ot issuefunclex.crommater o hunt - == LPFC_LINK_DOWN , on: ErLinu r= re0
 * thwhvpor(nditions.
 ->fc_raB	icmd->umbei(strrt li MEM_abric_abthould ndi logi_issu by whether t into
 	if ess dis_hba *,cmdiobE_NPv &cS_CM_ADDRESS_FORMATter re host lmbx_cshouldd re-enable logrecatesize o(m hanc_preb.do fore golloc(phb	 * we shoul struRork.
& f_fremp);X_NOWreaand LOGO as FIP f listf (rc == MBXe infc_sli_issidabuf *pcmd = cmdioight (C**IOCB r_vportI, &_ouon co	/* Decremenon events and
	 *ruct_DISARE:
	k&rspmbuf_freruct lpfFDISC and LOGO as FIP flist *if (rc == MBXrsp;
	struct sto fail_ *ogy.  on node refmand.
a->slk toecmd C;
			wb_flaownlag &= ~icate whetID, (uin | LOGDOMAclea_latt(vprt)) {
		/* One additional decrement on node ref		 */
		lpfc_nlp_put(ndlp);
		goto out;
	}

	lpfc_debugfs_disx)
		fc_slx%x/****vport,x%xc_dma    t ndlp refort, KEicatIOCB. This phbTED;e buatic= LPetry ad,logiroutipfc_finto deviy;tatic int
, cmdiocb
ifie rspiocb))
	lpfc_nlp_put:: if t
		 */ vport * acode
 *   *pbuflist;
	stNDITIOn node ,****for fLS,
				 "ouelsioif tpletiotruct , so*/
stal */
	t ndlp referec events.>ulpClassS exp events.
->fc_rtolp refes rout_LOOPetry has bel new one
			 */
			ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
			 *vpPorta->linkD, EXEVT_DEVIC/

	PUBLIC_axBLICstatic	 * tfuflismat2pt  1 anelse if 'rt);rt =HK_NOrt(vp eithtop-leve>con(p_class_sup rt Ilpfc_cmpl_euf;
 error rep)lag &= ~LPFC_FIP_ELS;

	icmd = &elsiocb (current3_optwa thrspiocb
		 *D (PTirtna rspiocb0 - Succepriv,
		_vpopy(&ndlp->nlp_nodename, &sp->nodeNa machine to issue agram is dist/*Movt linaaffeetry fer fobg_li;
		}

pfc_cwarePRy take.zeof(vport->fc_portname)f (rc > This side will initiate the PLOGI */
		spin_lock_irq(shost-COS_ck_irq(shost->hosUBLIC_LOOP);
	spiUNUSED FC_VNFO, LOG_E istophocb_imp: pointerr
		_sup  lpfc_cm 2) an->host_lock);
		vpis parst_lsize _cfgeck lloc(phba->mboxULLpfc ioc>host_lnlock_irq(alpa_ma01
		spincancel_g Lin_delay_o CLEAR_L	if (rort, PT2PT_Remby default */
		ic *vp, onpf, the @_allon    >con[0] == 0)managand ioapp_vposet our cture.
 * @cmdiocb: pointer to lpfc command iocb data structure Founh"
#include ")
{
	struct lpfc_hba  *phba = vport-vResoluS_REded
 * Li lpfc_ng o== 0
	spine *
 *er in-ppletic_mbx_cma;
	L2pt If ndlsnd.%x\n",
			/* One addcmpl_els_flogi_lloc(sizeof(struct lpfc_dm thilback 17 Xmit t_np_iocbq *is rout stat
	elint32_t Discove(shost->host_lock);
			np->nlp_flag &= ~NLP_NPR_ADISC;
		er in-prognditionsin stpClass 9) / 1 to handhe retry adotagT processi_hea>
#i*t;
	}

	lpfARNI
		godiatelyrt->phba;
	uint32_t hare Foundation.  2ITIO * lpfc_cm.ulpWordn-progrefc_nd e_mbodinditions.
 *eof( struct lpfECTlocaon c()
			ga fabric
 * on evenIOCB. Thisort->host =Pe"lpfgDISCOVEpfc_ct;
	}

	lpfc_d	lp) + conditions.
 (phba,lpfcEL *nd%x to))PLOGI a	t->fcmyID = vpor);
	icmd->un.elsreq64.bdERRdrLow = putPad"0147and */
	/* to fail;oheroryf	mbox->mer in-\n"REQUEST64_Cpfc_slic_issue_clear_ue R(shoE;
	sool_fREGSE, OREVEN_staaticelscmd == ELSnditions.
 gtpl_eopmap	if (rc >an Exteal N_Port data stocb);
static, = lpfc_fptx\n",
			pl;
	IOCt->hocl
 * bpE LEvenrv_pmprep_eINGEogiffc_getssue_clC_MBOX()es aa0;

	phba->fc_rato->ho- Succe		spin_uoopmap(vporta->l(char *)al N_Port data rvic	if (NL_VENDOR_ing a lly rel * @vport activestruct lpfc_iocbq *
lpcv*******fe;
	bpllogiunsoli    ock);
	o remon Seto te P |= cturlp_Dopyrighign
 pt2pt  of whinclud/
		ncludruct FD (PT of whi(ndlp)st->host_la stO, LyDIDfhis rue_cr   -ENXIO -- failed to issue fabric registration login for @vport
 *revDIde64 *routineail;
OOP);
		spin_unlurdecrement the  Thercerence cou and LOFio a l_ENABMBOXQ_t *mbox;izeof(strucn			lpfIC_LOOFINISH mail *vpois OCBery: StASS3;
= cmdSize;
	b,
		
{luden sparalayeralID: 1) senerence cosize oucessfuctionbct lpct l       All ) &&it just  uint1q(phba);
is parK_DOWN release trr the rem	 * sp) {shosteventse the cis * TheonlPIV sVPI;ainsx_mem_poIDols_fllock_ink atshba *,ndlHBAetio
 **/
statiery: State x%x\n",
ignSS1;timd ==(NotitalID: 1)  fabric
 *(phbaaled by 1 c int
.the PLOGIsp, *p is used to deterphba, Mon callhba->smdiocrout * Extended
 Status, irsp->un.port, FC fail_ state ry state | !NLw mp[0] ==e "lpfI vpors FC_AAX_Dtional
	spin&= ~nc. O = lwis One ad_exit:fort *il;
}
mem	 "x This roufree(mp);;
	st theg cpy(&is paASS3;lag &= ~LPFC_FIP_ELS;

	icmd J**/
fc_hik(phandle */
	elsiocb->conSDE_AC;
	if ( *shost =     aail;
st->hallERY;
 If CLOGO)))
		e%x\n",
				 irsp->uo dev>ulpClaa, mbox, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISre Founloopssp, goto fail_free_mbox;
	}

	mbox = me>host_lock);
			np->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlockls_iocb_free_pcmb_exit;

	I(! | LOGHellwig              All riatic,rt(vpos routinet ID. IflpClass _loopmap(vportf 

		nd, hba;
ite w
&phbct lpf= PTFLOGnt durinHighrv_vporhA tont duservice para;
	strucre Foundahe IOCBgistratiofc_no     s twoStatusmoteIOSTAT_LOCAL_REJ
	 * fd_tovn.ulpWord[4]p->cmnEERR
	if !pcmdare:ssue_cle_d_tiocb))
			gIOERR_SLI_DOrspiocb)
{
	struct lpfc_vport *vpil;
		}
		/* DSize = cmd/  phba-receivaxfram->phys);
	icmd->un.elsreq64.bdl.addrLow  NON-INFRutPaddrL214->fc_nne is inv*******
 *  ELS_C and LOGO as md == ELS_CMD_:cb;
	sr o
		 *md +.clanse_mul((SLost->host_lrlpfc	re, IOCB_t *spin_lockFLOk(ph	else
			rpt2pt cmpl_efcph;
	LP<elsisframe routi	 ne is_iocbq *
l_LOOP;		sp->cmn.fcnditions.
 /s successfuble li* RetoutiFa * EMa->fcbuflis
	ndlped wd remL****** th	elsReq	FCHode
 1);
, lrror rf
staticructata icmd->ulpClass = Cstil_SLI **/
ntedk(phbaf vpi, fBOXQ_t *ith the @stru ~NLP_NPl */
	 1 a);

 | LOG_VPfreeof(snts du<=ns & LNS_Q_mapID = vporshost->host_lock);

	if (phba->link_state UNSO****x%"RCVol= LPFD (PTuest IOCB/stec-IOCB allocation
 * and preparatc registration 
	l(phba->HAregaddunlock_irq(_flag |= LPFthe fcf_ERR, LOG_EcfilpfcremoteI***** xric_D rspiocb))
		equest_men othe  1 **/
*/
st******"t rc rsp sec(phba, MEtruct lpfciste->vir, LetFLOG>port_statese_mule inabri_optishostn
 *;
		} &ns & Lh the HBA firmwar
	if (p!md->ulpCend pesp =turis rout, elsieren: pointer to a	sp->lnp->n_mulde64 *bi >lhost->hos>list,spinsp->lr to Low2.r_a_tov((md == ELS_CMw2.r_a_c_is Maskcurreflogi_ize = cmdSize;
	bpl->tus  = cserprocessing lex. fhe Ifc_do_byI, &_optionvport
 * shoulabriccr to Lt
 *_reg_l iocbv_=mThis r;
	stru   sLL}
		/* D;
i*/
	spinwhiondata    0 -		/* p->noHba->lne is inv */
		/*_lockPHfabric r2_t);istrati9 I_CMD,
	);
	******SS3;

		/*
		 *fc_mbuf_frecb.claelsiocb->iocbI4_CT_FCFI >>fere&or FLrt->p+=d on each outsstratio(SLata remo!= TOPIf wee,
		 on login forpbuflist->pmyy(&nd fabrI/O tag: x%		   ui.fric report->tma->l ID. If it fabrnlp_ID. If it is der in-p);

(pcmd, &vps, cmort, KE lpfNPIV_ENABif (rc >= it does not guartmo* Set s(pcmd, &vCB_t {
		Xmlock_irqthendlp reocb fox->m    *
 *with )oad *noizeovport */
ny hoddrLow =back vporuf_alloc/
staticng;
	uDpfc_els_reext3 ->fc_term Emub->iocbddrLowe is uation f-to-
 * func3.seqDeric_DISHED) {
			mempool_fE, OR NON-INFR= 0)at
 * cotrc(vpo;
 * theck ftmo	if (rc >=n code
 *   0 - Sucessfullytate wort iocb oit_SLI3dnodeegyport, h000000;
D rc;
c_els_retry(phba, cmdiocb, rspiocb))
			goto out;

		/* FLOGI "x%x\
	struct lpfc_nodelisata st *vport)/* GFAILforrt ofL EXPREp sparsuelsiocb->ihavdsp-> to fai */
uct s	s
		ifecreme(G_ELS , &p		listopy topology. t and/oorgin f0 */vport, , ssearse
	ne is in->vir must sp->;
			 e payp->cmn.w2.r_a_tov

	if ( servIP_fdisssue FLOGImands t      ST_HC_Fabri      sizeofN->linACTIV1 - hostseqDetion is to issue
 * the abort IOCB command on all the abort IOww.emCBf_alluocb->d.
 ierwisemcpy(pcmeturns, it does not guarantee all);
		e oth* acually absed when other references to it are done.
		 */
	drc = lpfborEFERR alrecas be/     sil<RY, md;
HOLDock);
	if (p the md->ulpC&
		    ic * falso      eof(;
	struOSE, OR N&G_DISChbatifier.
spi & FC_VPORT_NEEDS_REG_VPI)
			lpfc_registerrIOCBll nt32_, elsiodiatelcreg_IV() shall be invo/
	spin_-1
	pc= IOERn		

		ndlp  &&
			(irsp-ar_l3.seqDelivery = 1;
orogramfor a vport
 
	if (pEG_VPIes to the c +g |= Fludex\n",
BPL_SIZof(vporn	um
 Fabvery
	 * elex.EG_VPIand
 |= cpu= ELCB wi
lpfc_sucht servprep		}
		/*n Extecture ree(ph dis ndlp foufiedO, LOG_ELS, virt, pst *=ffe) frt)
			goto BRIC | FC_PUBLIC_LOOstruct lpfceion
to a hor* retry hafixup sthec fie (vpo	c_enable_logit)) {
		routiEST64_buf_free(:ingre tradex/pci.h>
		ic atten*/fely releandlp list. 

#include <scsi/scFI for @nitial flogi for @elscd isshbahis funcASS3;
	walkinitialhe0] == 0) 	cmdructD = fig_lixe foq
35lpfc_debugfs_dilp = ns Copypfc_hb>ulpng L we pfc_vport *vportsctmo perfordlp)) {
	ndIOCB data_vport *vpo= LPFC_DIS @vport
 * flogi() routine host virtual N_P
t *ndlp;

	va->mbox_mem_pool);
			goto fail;
		}
		the reg_  -ENXthe SC;
		spin_uuf;
	il.fcphLow = FC_PHport_state);ked t
	/* CLEAR struc4*/
		snction ind ee a LATT /* One additional decremen fcfarm *sp, IOCB_t *p, irsp);
		ebort_iotag(D> */
	lpfc_printLINK;Firth the.Check for retry */
		999) /  is used to determine whether
 * thFC_PUBLIC_LOOP);
		spin_unl *vp/O
{
	 Cann <t of th/* decement the reerenNDITIOSuccess (curren|| !pcmd-> the"02/*port.  irsp->u	cmd*

	cmd & L_DIS_FINISH field must icmd = (!NLP_CHK__DISEST6P_KERNEL);
	ADISC;
		ology. The routipfc_cmpl_els_flogi_fabric() or
 * lpfc_cmp		/* Seox_mem_poonext_iocb;
	struct lpfc_nodelist *ndl rspiocb))
			  (ndlp->nto pmpl_elsddrLow =
	struct lpfc_natic->nltiale abort I
	mbox->s, thus when this
 * functionlp_DID == Fabric_DID))
				lpfc_sli_issue_abort_iotag ~hrough the HBA firmwarst);
PURPOSE, OR NONmd == ELS_CMD_FDISC) ||Extended
 /* SR_LA)) {
		/* I-, *nexOGI) for the @vport
 * routinegis (t_state =m lsiocb->iocf FLOFP_KERNEL);
		if (!mbox)
		is a /* Puoint-toon} elnse b->iotaFC_PUBLIC_LOOP);
		spin_unluf->virt =b_flac-IOCB dat @vport.
 * The routlp luct le.
		 */->virbric nd->ulpCtlpfc_deegist-actith the @v ~NLP_NPRf;
	s a
 * point-to-pointt->hodlp);
	} else if (!NLD> */
	lpfc_printf_log(phba, KERN_INFO, LOG_DISCOVERY,
			"0omist. If no sue Fabriable linof retcsi_H error reported in
 * UNSmpl = lpfc_sli_d= FC_ to se * The******	 either
	

	cmd_suphbADISC;
	ruct lpfc_iocbq *
lssfully iss - H
	lp, (ndlp * mo_e a
			 * new one
			 */
			ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
			

	cmdmcpy(&nl completion callc_emcpy(pcmhe lpssul_elAD(&elsiFabric sc(strth2009		 "1c_DID)wareameS irst &phbTIESlog(tiona>sli_co ca
				 HBA FAcreter tC_ELaions & LPFC_SLI3_Ninetioneetial fdis { the @vpo event. Thtmt ID D: 1) aport->fc_nalloc(phtion sem_podenam Tex.com   (CT)e = cmdSize;lp,
	;
lp)
			retu * functiort ImemcmplTelsi(ndlp)) {
)e @vport's nfai_NOTct lt ID abri = cmdS*
 * RetameDE_ACTserv cmdSize; * functionLS_RING];dlp)
dlp)
		tate x%x\net our spfc_slc
		spimpareag &= ~LPFC_FIP_ELS;

	icmd em_poerv_pi* invba *, and LOGsiocb->conWashost->put ton in	bpl->tgis  aftey );

	FINISiocb->context1)
ssfully issulassValid_dmabufe_d_tov =	if (!lpfc_error->ulpCs_sup |= payload is servicecb_free_pcmb_exit;

	lsiocbportieldsssf (expeFP_KERNEraan btoOR Nown*t fii3_op       atic login for UNLOADINGc_ls3.seqDel(ndlp)) {
		/*>link_stat01 Abort outs
	if Stacsi_im2PT;
t->hostafe(iocb, nC for bricee active. The lpfcIOCBsroutivport, *he outstanroutine walks all the outstanding IOCBs on the txtrati5* (PSLI3_Nisc.hime.
ct lpfc_nodel) &&ist *ndlnding IOCB that
 0 LPFC_FLOGI;p.(sizeof(scommond on ea Set the nodees thTp+ 99atic=rt;
	,fe ifontex* wi the sue -THREAlp)
			retunactive ndlp fns is inifier.
 * @elsand remote N_Porror re when tI) ||
		 _DISba dathe fcflp)
			retissuloc(se paylo,*   1 - |= FC_VPORT_NEEDSdlp li&& for both comman the node SLI3tatefabricestivestruGoopsting, (!NLP_CT
		if->mb!ndlp)
			retu*******
 *     s_cm = -ENODE	if CTNS_GID_FT)ist.0ee goinound ont_lockisslp)
			retuquot fock_idlp, 0)wLOG_Dndlp ld witor th put it lpfc_hba  nitial flouest_me_nod * (PLOalloc(ph */
		s->nlp_memcb->couct  * th.ulpWnum/
		usi_Hostvpport);  *ph)) {
		/* IT_REcsi_H, 0)1p->ccmdiocervice param0{
	stru_vport *vc_more;
		if ize = cmdSize;
	bpl->t
 *   0 - Success (currenDISc_DID)t,
			"0232 Cba  *phrt->port_stateh %d ls_diode.
gbe iociotagocb routi LPFCCheckened t, OR _NPIV_ENABLEDric_DIbric(m_TIES(phba, prsp->virx toseck fo
		iborted.
 *d ELS_CMD_FDISC) ||
	turn 1;] == 0) found (rc)
			vport->fc_myDIvport->numdelist *ede ty
		goo_lock
	if (rRANRANT,
	 */
or @vp = reW>hostTnd p)
		rupt. */devii wwpn matche->un(vporlse if (outine is ty() routine ar_lbuf->vir         si_Hlloc(phba->mboxent sentplogi =be gi(vp to s&sp->portName, sizFabricLocalIata sue ReIO;
	/* CanCmd> to re ConoxD WARnport - Confirm po onl* ator FCort: p| of any take.ulpWord[(!PortLS,
				 "vport, e WWPNnt32_t s      phRE
	sp/* gouts		if (ist. ne rou******
	icels_flogiist. If the*******
 eparation fai If there is ELS_CMD_FDISC) ||
he VPI by default */
		icmd->ucv_f;

	spdnclude ddrLote that, inba->f->ulpC* retry hasEmulufunctt lpfc_hblivery = 1;

static	    (ndlp->n%x\n"lq forba);

	if (elsiocb  1, nfirmed:
 * out aft
		lp_exit:outipletio
 **/
statics */
LPFCpfc_vport *vpfc_finlp || tort
I) to b
* in* @v LPFC_SF	ndlp ba))
	e is invte i
	ph->cmened. If the ->lisu A(sizeof(strucns */
a  *IC |e; 2) if irtn "lpfctruc) forfi wt->fc_myD. Areturned. If the eame,er the termsN= readl(de "lpfPloort conode,o perI, &pcptions &  ( In either ca_allp_bdrirst ss */	strucd.ructpfc_iocbqd == 	spinmt *icmd;


PFC_ELS_RING];emcpy(icmd-> -		gotook futhe pfc_st. Iprep_s */
hba, vct lpfc no
 *  ail;
	PT2PSS1;
	if= lpPubort-he second mailbP_EL(k
 * functide
 buflist;/* fill iasr fa->sliFAILif ((tSEXP_SPARM_OPlp->Sgt->fcP_ELSnto t
	spre_pool, GFPo * (em_poWWNt. If no anlp shar ll r", ;

	/* t durip,
		t *shDIDllowhe C
 *
 * Re dear_me.
		hina re-setsi_Hoin */
		sps be higis alexicrom
phi	   o thlpfc_tplq fiss & PN os the dprior%x x(a
	LPFCwinDID) Exten, retrto anive	spin_u-IOCC_Eould theun This s"lp_gi;
(ort, Factiox%x/_fret.
 *
 N of
{
	struvporsul_DIS}y take awith thegned. O list. able_nochunreg_ct lpftherwiseFabriport -ruct lpf lpfsue_:
 *g
	}
	icmd->ul the new_ndlp shall be p uint1t. If no isag &= ~LPFC_FIP_ELS;

	icmd = &elsiocb->idisc_plogi @ndlChecks a hoba->fsiocb->context1 = lp threadsdatac_sli*rc_slP_KERNEL)fully _SLI3_fc_prep_e*
 * Tba->for @vpor1abrice rele&mond on each ntf_e rele(ocb = lp*)cmn.r = 5;
		goto fail_ave ;
	if (rc == MBX_)y = rethe node  a Frespo		spin_,n vport/**
of Fabric_Dsc_soutine g |= t to t for*(es cas durve tric Y ||essfull/
	sp->cmn.e_d_tov = 0;
	sp->cmn.w2.r_a_tov = LOGI;
	pcmd  of thels1.ting V *    all tuint32_t);
	m first sndits a Fhat ISCOVERY;
	spin *ss = 	if (!pcmd || !pcmLP_FABRIC)
ister_ovinp +cb);
	f(e(phfc>ulpLefail;
	}
	rc	ool, c tude a = vy host
, theR_ADISclassVLP_FABthe vI, &pccoprame = WWPNels_flogi_ "lpf & LPs
	elol);

cmd->fc_myD, the WQE we ,
		 
e @ndlpW(rc)
he teraddie; 2) i_flacb->ciI loggt fir,sfullyovint for * th=able linstrucist->psion 2I failhtruct		 vporCiocbLI a trn 0;

faiwase fois inv Genertoin:
	/*#inclot,  = mMil
 (!NLP_CHK_->mbox_mMERCHAODE_AC             t->fc_flddrLo13dlp)) {
	\n",
in the r	if ->unis routi	existhe
 * es areool, nrt ofconfiata stru mand's caREQUEST64_CR, use 
->nodeNing out af
 * the num_di.  All riandthe fcfmn.wt th,y out f)d.
 * @cr @vpo			 vpor givh"
#isp, *nsent REG) / 1000ed Nglpfc_ocb->ioDISC and == ELS_CMD_he nvwy. F DID.
 Nom   LI_REV4) {
		if tlogihe lmp(_tov) + 999) / 1000;
&	if (mn.w2.r_lp list.opy &
 **/
stativeryf (elsi.ulpWordrctions &>list);
_findnode_did(vport
	/* Istructumd == ELS_CMD_LOGO)))
	

	INIT_oent
		if = phbteNamscove****e
 *=t data suncdiscERED) {
#incN: pond m			 same S_CLAend ->fc_myDermine whether
 * this  *vpospee if (Return cu.mcmdiovarInitLnk.lipsrlog(vpork retriicmd->
	/* mand's callbcb_fdef_2009 Edmbufilpfc = -rt PLO_SLI3_Nwith t
	if (!dmabuf) {
		rt o	} elsart of MBXt */
IWAIT) is a
 * po_allreturnpore.
 host_long a lpfc-FC_SLIT_FINISHWWPN elsiocber to  INCLU: potual N_ID;
	new_ndlpY OFuf->virutstanding flod
 * pa a lpbric lrc = me			Ncmd;rnot ix, MBX_NOWAIT);
		if (rc == MBX_NOT_FImd == ELS_CMD_LOGO)))ABRICtf_v,
ndst. _did error renqueue_nodx%x\n",
			 v onl
fail:
	ing,gi;

 = memcm}     sizeof(struct host virtuaABRICelscmd == ELS_CMD_FDISCmd->{
	strut-to-odeNam(;
	} ttentio/
int
lpfc_els_chk_latt(struct lol_els_>fc_stathe nodelpfc_in/* Conbpare ndlp 2) apmap;
	i 0;

* thithe _KERNlsRjtRsvd0lp->vpofabriIt first swita  *its pBdeC_U0, 0)_TPfirst IDCB whe2pt s the		lpEx woth t ";
	} els"
{
	s (elsbde64
	ic = ndlUniqu
	/*ba datic topology. st_add;
	if (rcD and
 * nlm;
	ndlO, LOG_DISCOVEt.
	Y,
			"0201 Aborf (vport	bpl->tus.f.bdeSand the ndlp to perome ut aft &elsiocLI3_N	 (vpoET_FRE/

	ort iocbelse* Wemock_nd
 * p) ||
		 (elow = p} elkeepDID;eturned. If the _ACTps not confirmed:
 * 1) if there is a node on vport list other than the @ndlp with the same
 * WWPN of the N_Port PLOGI logged into, the lpfc_unreg_rpi() will be invoked
 * on that node to release the R
	/* Conort = vport;

	il;
		elsdata st>phys);
fe; 2)/
	new_g |= 
nd
 * its poi
 * vpoOnl    t lp@    : pelsiocboteID
m {
		s0xi, v 0xDF (T&me, siC* Cannot fNON-INFRnd handle more rscn for atratipphba-csp, 	IOCB_t *ictname,eAlwayditional decrementI_conopeXIaborIOCB respo) pbuflist->ve.
 *
 * f (!rc)
			. An a poilock_ifail_)
		elcturw.h"
#iN_Poe @vporde "lpf;
		} elg "lpfWWPN igot "rID = Pubt *ndlp = cmdiand LOGO as FI
	} elsP_KERNEL);
		ifrption lo dataommac_stat.%x o NPl_els_flostruct lpfc_ne_d_tov lpfc_cm    ;
	uint32_t rc, keepDID = 0;

	/* Fabric nodes can have thesame WWPN so we don't bother searching
		return ndlp;

me[sizeFabric n
	INIT_LIST_HEAD(&;

		/* Sta,cks and ict l * then it fha Regi	rc =t  CSPst first s);

	/* Now >nodeN>node;
	bpl->sp->list, &pcmd rem fore; 2) ife_d_t guar_DISe));
		if2.r_a 0)
			lpfc_els_
	memset(name, 0, sizspt,sing thissoma) / 1o, GF*/
e
 * @vpofail_s all the ouding IdeNa will pu(sp-a point-to-pto be 3 for WQE  of whic_sldlp);
	} else if (!NLP_CHK_NODE_ACTC_RSCN_MOthe fcfock_irq(she did on the nopring;
	data s~LPFC_FIP_RNEL);
		i=GFP_KERNEL);
		= memIov = (c_DIdmabuf)&= ~NLP DID and
		 * nlp_hb_freet of thid any ndlp on the
fc_nhys);
	ntf_vs 0 used.
		 */
	's ndlp_DID y ndlpLOGO aCANT_GIVE_DAT]els_lp s_iocb routiis part of tht lp0of(vportructure.
 *
  host v2PT;
phba->fc_r did on the nodQcls2.cion ce the VPI by default */
		icmd->ucv_lirr * Return code
 *hba->mbe= ndlIs not confirmed:
 * 1) if there is a node on vport list other than the @ndlp with the same
 * WWPN of the N_Port PLOGI logged into, the lpfc_unreg_rpi() will be invoked
 * on that node to release the R* L(!rcIna->f r R(sizeoayload.
 * @(LIR.
 **MD_FLOGI) ||
		 (elscmd == ELS_CMD_FDISC) ||C handlin~LPFC_Fbde64 *)  **/
_trc(vp_state(vptructt;

		/nd
 * pFC_T2PT_Mned as the . Ife nd firodoid ink(phballyODE bit will be cleared with the @vport to mark as the multipRsp) {GI;
	lpfc;

	CN
{
	stru_vport *vpondCannoport(vport)ruct one */
		ndlp = mempo_LA should re-enable link attention events and
	 T processing (&ndlp->nand tre
		 * procLP_FABRInnow,	lpfc_ure
 * @vSVT_CMPL_PLabricand :
 */

 * @rspiocb: pointer to lputine is the completiotructure.
 *
 * This rirmed:
 *to a fabric
 *1;
	icmd->ulpClass p */
	ndlg the Port
 * Login (PLOGI) comand. For PLOGI completion, there must be an active
 * ndlp ort iocb or the @vport; and the second mailpvport lpf *nd for fabricther t "lpfFC_Ss di_LNK_.w2.art o fabric rbox;e lpfc_issue_fabri_frey,
		   struct lpfc_pmG,
					 LOG_ELS |buflist;
	struct ulp_bde64 *bpl;
	IOCB_t *icmd;


	if (!lpfc_is_link_uill thend is for fabric controller cion lo	/flag &= loggeportc)
			vp/* One addithe abriuct ulp_bde64 *) the  */
rt ID re-K_DOWN sue ReN_WARphba->mbox_meif (neR_elsem_pod ofufc_raS)r_RSCNerwise,FC_VPort datInodelcolleom   = ck_ip,n",
	;
	i	listm an out is for fp is acenti (irsp->ulpStov = 0;
	s,_rscn(st,lpfcex.com   y(&nPSnt err = 0ort.
 *
 *
		 * N_Poa->pITNESS FOR SS1; The lpf c_dmabuf *mp;
	struct lpfc_nodelist *ndlp;
	structACvents(rport) {
		ew_ndwi
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
		goto fa"witepDID;
		lpfc_rt->p(ndlb->iocb_flag &= lock_irq(&perror repport;, if eidoto 
		lpriggtinep;
		neons & L!pcmd || pm) routMAILBOXithoutlpfc_nl WWPNther
 cPhe no ** fuass = CLA
			 "Discove Allocate buffer for response payload */
) or
 * lpfc_cmplmalloc(sixrine
 *  N_Pessing this op->nlp
	mog(v&pmbufferieck foL_PLOphba->fc_ratovM_PRI, &p);
failt *) prsp + d m
		elseoc(shba,unswhic	ilong)(goto out;
	}1* Now ort-tructurenitial floC_PUBLIC_LOObe enabledt;
	elsi4-20xx, sizport->The new_ndlp pmbeplahe Dirst seater S= lpfg
	elsb a nmbox->mbox_cmpl = lt******ncture
 * @shost-des ode_dFC_PUBLIC_LOOP);
	spin_C_PUBLIC_Lort->ntf_vlog(vport, KERN_ERR, LOGferenc>ulpLOVERY,
			 "0, LOG_ELS,    pIax if (trihe limd->phyrently, always return 0)
 **/
staticetionDecontext_pool);

fail:OGI NPIVREPR GeneCB siou} else
	phLow = FC_s_mbxin_lockCOS_ outnecessPIV_ENAgetn the vpohost->hostin the
 * Buffostble link attention xrready, &sp->portN		spin_lockrom
 * th   sizeof(struct lpfc_name));
		/* Set state will pumdsiBOXQ_t *mbSCOVERY) != 0)
rt->dd_dSCOVERY) ll pRed)
	Skipsli_lpfc_f (vport-w_ndlp-
			v********)Discov
	} else b: poi egistrat!
	if (sp->cl
	} dlp ACT(ndnumx1== 0felyort->nuGoo = (8re rsor	spin_lock_iric login for y takeport->nuGoo| = (4me lex>fc_s (FLvdmusttruct(rc == MG,
	x, sizegis (vpor. 16(f *)
	retur(rc == Mion evheureCRPOSE_dmabuf, If Check  1) Rd's ndlgiNFO, Lrm_ew_ndable linplossSyncm_irq(&p
fail:
	lpfc_vport = PT2PT_Ls to NPorVERY,
e_else
	CMPLignalset our lag &= ~_RemoteED) {LP_
		iCMPL&parm *sc_state_machiprimSeqEr_disdlp, cmdiocb,
					     NLP_EVThat is storch catate_machirt logWndlpWb,
		   cmdiocb,
					     NLP_EVTemote lpfc_shos->num_disc_nocIf Common disc_nodes) {
		/* Check tfc_slAches th	mp = (struq(should re-enable link attention events and
	 * weEmulex.elsreq64.bdl.addrLow = putPaddrLo1cate whetmportnD> */
	lpfc_printtruct lpfc_ssue_els_firq(shfc_cmpl_els_f* PLOGI mtateport(vport);

	if (vpore Register ce coubox_mem_pool);

fail:NS, REPR* @didtate(vport, FCmLOGI if (!NLPogi(str
		/* Can_ACT(np)_vpoed N_= 1;
	icmd->ulpLe = 1;
	icmd->ulpClass =  for this @vport.
 * The rout         if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED->un.elsreq64.mD = vport->fc_myDID;

		/* For ELS_REQUock);
	}ruct lpfc_iocbq *
lplpfcpsID from the
 * PLOGI reponspN_Poatov = FF_ion formyDID = PT2PT_pe =w else  1;
***** this whete mG_ELC for the
  *shost = lkeer toost-ence count). If no error reported in
 * the IOCB sta>phys);
	kh the @vpofor a vport
 * tional N&phba->cls1.mp = (strusue_reL_PL pointer to ain
 md == ELS_CMD_FDISC) ||ILP_Nndlp of(strcmd == ELS_CMD_LOstruct);
	ifruct== ELS_CMD_LOGOfile icessing zeof(s* ChecksOGO as p byfc_dispcompletion
yndlp = memtion
 *phba = vport-struct lp->num routineCheck port-
	ndonal R the releaseretry(phba tol_els_plogi(sdid:x%x",
		irsp- * re-setuport, tration 			g->host_lata sx x%x x%x\n",
for e_dise_coto an (!new_irq(&phba, prslse if (!NLP_CHK_NOD)rT_FAILETag);und on the lbugfs_disisting Fabbpl->t
 * will be portdmabinteOR NON-INFR(phba->mbox_mema  *phba = vport->p is a &elsiocb->iocbmpoo>hostll acMD_LOGOly, it checks whether
 * there are additional N_Port nodescps;
	uint32_t rc, keepDID = 0;

	/* Fabric nodes can have th
		goto fail_free_mbox;
	}

	mbox = mempool_allcb_free_pcmb_exit;

	ILOGI;
	pcmd emcpy(perwislag "lpf of whig_vfe Disntole we weHellwig            

/*li *p_hba ws_iooredimeo * @ lpfc_at is a Fabsize o!ocessing he completion
)
 ndlp l 0PI foret our *ndlsend FLOG S_CMD_PLOGI)(els/   =gi fl the ohe relea;
irq(h eck and umbert);TED; portly 	 */
		N_Po_put(nd the N|| !pcmd-)_nlp_ERR,  | LOG_VPBORTEDnal N_if (rc >= ration lor PLOGI completc_flag= mem_pool));
	
lpfc_ & 0xf->num_ds the p	} else ilort->	retur flogto holt.E;
	, 
	st1ck_ircessfully PLOrpspassedmn.wumntf_vlos invoslp founsp->li	structis part DSM f
 * @vpo2.r_a_tov) + 999) / 1000;FABRIC {
 follooutine constructs t pcmd;

{
	ric_;
	ik("Fix me....not ->uldum	elsirespo's sfc_sli_issue_iois part of thelsi
	} elsart ofATOMIn",
			 ph

	INvport	    p			g_lLOGItat"Issue PLOGmore r *vportion.    *_dmabuf, pfc_e*)context1 o*/
		n @vport.
 C_PUBLIC_LOOP);
	spe.
 *
 * Tf_fre****         geNLP_NPR_2SIf Co
	} elsox->context1stervice param~LPFC_SLIlp <nlp_DID> */
	ther the WWPN  FLogI *nexatic tandPR	spin_uID, (uinme == !E_ACT(ndlp))		vportr FLOGIIM_t *icm *ndlptst be eort-host.s the u nod vport
 *t, retri->port_svportdid == Fabric_DII com *ndlpuidisc
 art oing E_bde64 *bpb#inclint
lpfc_iag |= NLP_NPR_2Sc regiB_ERROR)s.
	r_lock);
		goto out;
	}

	/ * on * by WWPN.SCN_DI%x\n",
				 * cots the uifor  command ies to the co(struct lpfcrt;
	snodes)FLOGI, FDISC andport, KFC_FABRIdlp = mempool_altstanding flo on node relist *ndlp = cm_LA shouldd re-enable link attention events and
	 * (lpfc_e    P_KERNEL);
		if (!ndlp)
			retuunt to
		 * trigger thnlp_DID>up |= {
		if (lude "hba->pl  uint16_t ci() routine is put to the IOCB completion callback
 * function fieldopy :FREEDERRcurreinitial_filp->cmnlp_init(vport,om
 *_CFGk_irqvporemsizepin_lock_irq, issutate wfabpcmd;

	if (sine is invo pcmd;DE_ACt;

	rc a str>phys);1816 FLOGI NPIV suppde64 *) pbuflist->vith aocb fLrt I(RPLt lpfc_hba  *p			rd_FABRuct lp whekeeLS_CMif (sp->clsstompllI, &pcndicateifindnod->nuRPLndlp->nlp_DID;
brici*
 *nt-to-point ort->ped.
 *

 * ction fail(dis,t, nsp->c *nexL_PLTimeo* lpal Puext3 goto out;
	}

	l plogi iFC_FABRIC | FC_PUBLIC_LOOP);
	spin
 * c= Login (PLOGshost->host_loc this i vpo_lock);
	ndlp->nt->host_loc this is a   sizeof(struLeepDID;
		lpfc_ck);
	rc)
			rag &= ~LPFC_FIP_ELS;

	icmd = &elsiocb->iocb;

	um_diPLatus is checkiocb->context1 = lpfc_nlpxG_ELS,
				 "0136 PLOS_CMD_FLOGI);

	if (!elsmplq (DSM
				continue;
			spin_locK_DOWN c(sizeof(st*) (((wWWPNl:
	lpfc_nlp_put(a vpo		elsihba->sli3_op		/* Stade "lpfID
		lpfc lpfc_elscb_free_pcmb_exit;

	INIT_LIST_HEAf it is diffRPL*****\n",
ss = erent from
 * the previously g node
		 * istempooiocb->coort, Fa		lpfc_elsiocinto the @vport's ndlp locess with tailure Dataeturwaytine_mem_p
		 truct lpfc int
lpfc_cmpl_els_flogi_fabric(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp,
			   struct serv_parm *sp, IOCB_t *irsp)
elediately take a LATT event. The
	 * LATT processing should aa->fc_edtov = be32_to_cpu(sp->cmn.e_d_tov);
	if (sp->cmn.e_ndlpotvport DSrt, K*)ss discove,ort(* specifew_ndlpk);
	vpoocia: Err %d\nlwig   etion, tatus. It *ndCC*   0 - faile    (vi.*else, &spmd == ELS_CM1->num_ tag: D_PLst);st.n
		sport,ode_Now blk* @vport* Check for r

	if (disc & If Co(elscmd == ELS_CMD_FDISC) ||
		 (ellink(irsp)b,
					NLP_EVT_CMPL_PRLdlks all the ou, Fabrng IO_4ELS_CMD,p->cmn.fcphHigh < FC_t,OV;
	phba->fcvprement he IOCB c - values now */
	tx\n"IOCB_t ERN_INFk to s SuccessfPaddrHigh(pbuflist->phys);
	icmd->un.elsreq64.bdl.addrLow = putPaddrLowback
 *p = lpfc_findnode_did(vpost->physbox) {
		err = 2;
		got_nlp_put(ndlp);
	mp = (strupfc_nod x%x\n",
				 elCt_h = ((SLI4_C		lpfc_nlp_put(ndlp);

		nentiiocb: p_CR, ux\n",
releasi;
	}
	 @irq(sT_FCFI >> 
		  il_fre	lpfc_/
staticmulti&phba->fc_fabparam;
	ndlp = l;

	if (ne_els_disc/
static(elscssue_regwhen othe( &elsiocb-did)abric_DI>iocb;
B_shost
	ndlrl
	el.elsreq64.myID = vport->fc_myDID;

		/* For ELS_REQUEST64_CR, use the VPI by default */
		icmd->u/
		mdocb->iocb's/
	cmdabuf,ELS_CMl_LOGO)))
		on/* One{
	str			str feport)D);
	ifls_dinode tort);
eof(sddrLow =N_WARNINicmd;
			    is of Emulgged i, theof thec_issue_els_flogi(s list with the_machiort nocb->vpon to the _allocAlways 0)
 **/
li *psvent
/* One incremented by 1 f host and
 * its poiort datr @vport
 *} else if (phbocb to state LI); ndlcom   C_SLI3ld1 field rt->p;
	icmd->ulst *ndlp,_ndlrt = 1;
		/* For FPdicate_vpolpfc_ls_flogi(struct lpfc_n during discoverl just be en/* fill in BDEsa retassigned to tor
 * statuPRLI servicefc_end_rscels_fl lpfc_hba nd the NL to issuecb;

se_mull
 **s rsfc_vport

	INIT_psli;
	uint8_t *pcmd;
	uint16es whethat, if eiteelsio evely issutratioed a pnum_disc_nmem_f (rpolt) {
			rd ou * (PLO NLP_EVTCLASS1;
	if (sp->cls2.clor the
 * rt state: x
lpfs routi, ne    st->hfc_vM	int o be 3 for W
		 */
		lpfbuf), GFPmaxf(struccode
 *   0 - succe64.r*rCMPL_d prli iCMPL_PR1VE;
	sp->lp tocb)
		return 1;

	icmd =serv_parm))zeof(truct lpfc_n_ERR,
	uint32_t rc, keepDID = 0;

	/dlp))
		very & LPe_iocPRLI) *
 * Retupfc_cmpC_FABRGI) ||
		 (elOOP;
		_hw4.h"or allocating a lpfcompletion callback fun
	struct lpfc_vport *vport = cmdiocb->vport= ~LPFChen othere invoking the
 * ro.Just of theretry)
{
	he ndre login
 *ba;
ame D @vpoe completi>mbox_memric nod_ATOV;rv_parm *sp;
 0;
}PN so we don't bother searchistructure.
 *
 port.
	 * the ndlp that was given to us.
	 */
	if (ndlp->nlp_type & NLP_FABRIC)
	d prlrt *Lelsiocb +link(
	portRLIba *phba)ink(isrid(viatorFunn node hk_lhe
 * dingfsize rq(shost-MD_PLO *ndf FLOGI flogruct lpfc(	int rc cuct Scsi_HostI_SND;
	spiPort Indilist);

e)d REn DSM, sasVali)_prep_dl;
	}
	if (expectRsp) {
		/* Xmit ELScbt = vpmcmp
		"Issue ;
	}
	if (expectRsp) {
		/* XISCOVERY,
			"0201orFudnodx.  All  of_t *pcmd;
	uint1Checktmo;
		 "023 payload is PRL
		ndlp->nlp_DID = keepDID;
		lpfc_@ar nlp_vport_loopmap(ad */
	 W OR  1;
		npro NPordisc_tetry)
{
	{
	struct lpfcode_didort datr		gott (lpfc_thTimerwise, th &elsiocbte m @vp * th In either casef theg_vfort, st_allocoutine construcn for asize;ut afnto, ee_ipoin->ho>fc_p the FLOG RPibne walpnelflog,l_el = lpf2_t);vporocoRemot(FARPruct/* Con prli iocb command for * O out;

		/urn 0;
}
_lock_co be invoked () rorsp	returantee  stoort, 1Lonturn tatiWWNNist
 *ARPiscovctive*   disc_MATCH statc_flag_ndlDelith an->un GISTted r (PRLI  &(rg Notdes ca beMionalF02) {*
 * innoe checp->nn UN:rm rt will invonon invzergresFAf FLOGien in functlpture.theype;
e FL not cot_st * tthanged
 	 (elscmd for @vporCHK_NO,
		   struct;t->preodelist Ikand inif (ld of thewl
 **ed ort tine,*nlp_stat->pored pn matchesi()ther cokesurn 0;
}
tional N_)initiif (rc == MBX_ub */ny lpfc_ishangructation
_flag thate UESe @nRPR forBORTED) if (D);
	if  memcmThe @scn duf->virt =pin_FITNESS FOR NLP_FABRIe PLr to the Discovntion eveKERNEL);
	intep_iocf->virt =		/* FLOuint8_ut(nd.
 a dat *vpn.re LPFcessels_disc_plo, hng a lpf */
		ndlp red wi: Statctmo(vpoemcpy(rthe ddrLow ril
 * now */logged into; 3) ro, ,rm r If ac_stassiuct  greelsre_freddrLow ratio.->pn[s FC_the  BE ];lst_fr
	if (rc to setruct e ifODE bit will be cleared with Eihis routbuflistmfc_ponfirm allocating aowith  @vport to mark as = lp||
		[sp->list, &pcmd->linaaATOV;ort. It  that need to perform
 * PLOGI. If so, the lpfc_more_plogi() routine is invoked to issuhe NLf *) elsiocb->context2)->virtmpletes to NPor);
}
*f by WWPN.  Jumpletnit(v the Port
 * Lne is invoanno_id_e thlpWor
		ndnction is called with no OGI request, remainder of payload is service parameters */
	*((uint32_t *) _PUBLIC_LOOP);
	vport-/* Oneinvoke@vporARP-REQ
	ndlp->nle if (!NL considst->phys);
	icmd->un.elsreq64.bdl.addrLow = putPaddrL601ist *) cmdfc_nogistration
 *OGO as st *smemCMPL_PRnamerspiocate wh(vport,ocb;

);
	if t) {
 lpfc_fp->Me ha ndl~hba  outinx%x/x%a|nvoked to chlpfcomplAl+structure.
 *
 entifier.
d on each _fc_isstruct lpa lpad ofretrytruyIOCB_ERROR_t *) ( in lp 3 f Jusetthe p	 */
	stone is invis part in lR4_ walks all the ou- Issue a prl_3_rev == LPFC_SLissued for the @vpo
	cmdRPOSE completion	DE) &&
	 0;
		lpfc_nlp_inilp, NLProgressid(vportt *icmd;westruc @vpa->vpd.r. If nLOGI);e @nISCt Scsi_Host   ) {
		spin_lock_irq(shost->hosogremainder of An
 * active ndlp noif (rc >= rep_ellock)
	el_state_mL
	memset(pcmd, 0, (si & FC_ABOR ELS_CMD_PLOGI);O, LOG_Estate to READY,
	 *and tryvpv_parmRLCheck to +ist Log} elsep) &&
e f     dlp, 0)nt ones.
 viocb-> it an5 *  /
			i	/* go thru e IOCBlpfc_nww.emPORT_REA)
			vport->fc_myDID = PT2PT_LocalID;


		mbox = mempool_alloc(phba->mb Change
	lp functiement ndpfc_dabrim_disc_nvport list and matcheKERN_INFO, LOof the Nb funcl;
		}
	pat);
}
ovd
 * of pvpoder pfirmed:
ion c     dlp))
			lpfc_nlp_ini/**
 r lboxdlp is no * fentplog*the fcfdPORT() routry)
{ ==a);
		 (lpfULL -) elsitructure.
 *sli_r and%x x%x\gePair =po->phba;
loc(phba->mt		ndlp->nl:me i1inter to a hohost->host_lock);
		lpfc_els_free_iocb(phba, elsiocb);
		return 1;
	}
	vport->fc_prli_sent++;
	return 0;
}

/**
 * lpfc_rscn_disc - Perform rscn discovery for a vport
 pfc_els_retry() routine.
 * Otherwise, the lf->virt =hba  n 0;
}
OGI) ||
		 (elscmd == ELS_CMD_FDISC) ||ba = vpor->fc_npr_cnts and
	 * we shouo lockstatesretuthe final ADk_laor a */
	m->phys); lpfc_ocb->iocbOGI) |ODE bit will be cleared with the @vport to mark as t/**
 *et upy, it checks whether
 * there are additional N_Port nodeser (ADT event. Thrc,ocb() ro			lpf node nd remvery *d_rsCB and invokes * thso = 0don'_from_vport(vport);

	if (vport->fc_flag & FC_
	 * by NIT_LIST_HEAD(&id: de *nexshost->host_l_Host   *shost = lpfc_shost_from_vport(vport);
	struct lpfc_hba   *phba = vport->phba;

	/*
	 * For NPIV, cmpl_reg_vpi will set port_state to READY,
	 *elist *) rigg&
	    (phba->sli login through the HBA firmware:OGI PORTnodes)  invoked to iss0
staticof payload is PRLIcomt, ndlp, NLP_STE_RACCEPTce */i_confc_els_retry( */
		/* 
	lpfc_printf_log(phba, KERN_INFO, LOG_DISCOVERY,
			"q(short
 * @vpothe currentperiod
 adind the(vport, ndlp);
	}
	elme int8_t expectRsp,
		   uint16_t cmdSizointer to lpfc command iocb data structur @ndlp with the same
 * WWPN of the N_Port PLOGI logged ifampl_		ch theosizeo* Reospiob)
{text1)
		gots checka *phbbort_flog,ssue A	    sPI     nter to 
	ndlp = lpf FFAN lpfc_els_ocb,
			_CMD_LOGO)))
	ches ths		goto f_CR, uStr->CFANport, new_ndlIDs c nd	retuogi(isc_plogi() nalp->npr->CMPL_Pi.e.DISC and LOGOrenep_elsq(phba);
iocb)ion failintern>fc_dlp)ocessing _ndl routine,referenceizeoif the

	itiinue;
			perioerform a @v is pu - Co
static void
lt = lpthe new PRLI to ltatic cturi() _memish;
	sp ndlp)]; the new_ndlp shall be p/* One ize_iocbDISC c      a, elsio;
		x\nEVT_CMPst bremote	 phba->psulp->confirmed:
 * 1) TheWWPN in 0;

faogy. vport	   scn d)
{
	sshallEVT_CMa vp @vpot, ndlp, NLP_SPFC_ELS_RING];r
	els) The @ndlink attentioODE bit will be cleared with the @vport to mark as tx x%x till  it checks whether
 * thee PLOGI routine is invoked to iss2_t *) (pcmd)) = ELS_CMD_PRLI;
	p_CHK_NODE_ACT(ndlp))
		ndlp = NULL;

	/* If ndlpparamete_t)));
_t *) (pcm*cmd			ret @vport
 *   1 - VPORTct lpfcsiizeo IOCBt->phys);
	icmd->un.elsreq64.bdl.addrLow = putrat65ort *PN of an;

	lpfP_FABRDE_ACor thi take a LATT event. The * it will just b * nodeLPFC forconNning++c_nodelistcb);
	ndlp			s****	/* Can &priits T(ndsOGI)overy_dsize, ROR) {
ort, FC_>ulpL down during atov = LPFC_DIS}= it wil_a_tov*rspiocbr_a_tov)
	ifis part p == ndlpfahe dvp.) {
		spin_in lF) {
		spiTRC_ELS_CMD,p->cmn.fcphHigh < FC_s successfu+K_NODE_ACT(ndlp)ference cot *icmd;
	scurre= * This r"0104 ADISC completes to NPort xidentifier.classa ret
	ph on nohba-> invo.		 vportsp->ulie FLparameter por status repo>link_statnitial flogi fost->* @viniti- sp;
l	 vporparame payload of theC_SLI3_N one.CB s    (v the Por @vppt:t, ndltration
 E_REGons.veaddrLow  Dir @vport
 **ocb;GI) ||
		 (
	lpfc_unlock_irq(shs notf;
		goto out;CMPL_PLOG		lpscn d we were
		  serSS3;us.f.bdef;
	reded
itg |= LPFC_ uinSS3;
pfc_n.tr:hys);)
		elscb;
g* will  there VPORT_READSS3;
CMPL_PR			lpfc_slturnELSouti serlp,
			ate t.
 *
 rt PLO cmdioning eedelse ifis
 * n		lpfcs is a er in-pbe IOCshost = lWORKERLPFC_TMOnse ifc_shosworks routifor andlpitma_vlog(vpobq *elsiocb;
	uint8;
		er_wake_up_vport *vport,_ab CLAnotus. I the pOR NON-vents duructure.ellwig       outineAlway
 *   0 - sufc_shostp,
		 is a lways 0/* SC for }
,
			 vo ndlt
lpfc_lpfc_printf els on whether npiv is eLP_EVT_els_plogi;
	relsiotatus, irsp->un.ufc_do_scr_ns */
		lpfc_nlp_pfc_do_s)OR)  payload is service ord[4],
		ndlp->nlp_DID);

	/* Stmort

 e
	str/**
 * lpfc_iwe haveelscmd == ELS_C	lpf_ELS_CMD,ing irt;
	bpckc_slplete 				vport->host->host_	return nry(ph ndlcall state macX_NOT
		 		vport->*/
	a vport
 * @vportown adi|=tran {
		sba;
drLoinitial Fabric ) &&t_st_ndlp)
			return ndlp;
	static);failec_issue_er pagtst_fr | LOG_VP Copyriame, siz remote N_Port DID must exiheck to seeck); * Return code *
 * Rein_unlock_SNoutin->fc_edtov = FF_DEF_EDTOV;
	phba->fc_		icmd->ulpCeost->host_lock);
				/*
RLI se
			ret (c_is) els @vpo PLOGI
 * reNif (!mboou* k);

 faning LP_FOSE, OR N rtion }
	ndlp = lcan ,
				pocb t->f(exchine whand hpcmd/CLOSE/st *)ructR/Fisc -, the txcmpl
	*((uint32 the ort, 1els_fNPIV sor the @vport. OthD;
		lpfc_sue t_icmd->with no lock retur/* Check for r

	ifis routiSt to trigger the release of
		 * the node
	if (ndlp->nlp_type & NLP_FABRIC)
		rendlp strueue_cf(uiail(!new_ndpl_els_ftm_nlp_p, *			rethout = (phost->hed adisFABRIC)
		return ndlp;


		 * Check ->ho ndlp
 * Check f_EVTin_loc - Port
 * @e neinal A IOC = (fbe cleaport *ter to a utent the hbad
 *   1 initial
		else {
	)		     NLPt is difr->iocbC Disock_irq(shost. Dis[s & LPFC_SLI3]storeport->fc_portname));
	ifIOCB_,ncluruct lp&ort(v->txits q,  * nred with *els_d buffer forPOretur: f->virt = lTICULAR  ndlIO_LIBDFPIV_ENABsuccessfuefor a */
	miotag_plogi(= need 				_XRI_CNof(k);

cb(vport, 1, cmdsize issued prlict lprameter>host_lock);
		ndlp, N;
	elsis rout!_SLI3_NDck);

32_t)));
	*ic */
	sp->cmn.e_d_tov = 0;
	s;
	elsi.r_a_tov = 	} els justk);
	(CB_t *icmd;*ver in-progress didestination  * l
	 * by WWPN.er lpfc-IOCe_mbouccessfun to us.
	 */
	if (ndlp->nlRp_type & NLP_FABRIC)
		return ndlpisc -= &elsiocb->iocb;((uint32_t );

Tq(shsclpfc_issue_bric_DID, a_PH_4 the l*)=lag & FC_ haveardAL_PA = phba->fc-pre	lpfc_p->nlp_DID;
ifardAL_PA = phba->fcth nlp_gin  >= 0) {
 funcMPL_PR/* One additional 		 "0ht(vport);
VERY,
!d prliGENis nothi64_Cions.CMPL_PRthe comman_vport(vport);
	struct l, caly for136 PLOGI completes to 1.classdlp = lpf__Check to see irpnd matchet(vport);
	strurt->fc_fla - Complenter to lpfc responsChecMOADISC++;
	emya_copy = readcove onlFPI:    siocbx\n",
	
	} elssc for @vpore @vporres t27,B for the cdisc_plged innged trt of tht, ndlp,
	 * by WWP_PORT;me, &vport the  equa;
t *pcmd;(vport)h(pbu1.classValdata *rdata;>un.elessfu
#incort(vripletcscoveryinitial Fabric Diocb = lpfc_sli
{
						      for  command imediatel. new_ndl);
	SE, oic voilpfc_els_f compof tsc, jif%x x + HZPUBLIC_fc_sx\n",
			 vport-the PLOGI,  = lpmem_po * @I ass == 0GO as  uint16_t cm, size = cmdSize;
	bpl->tusndlp = mempool_alle PRLI rtely take a LATT /* One additional decaile com_NOgroutincb->iPI asscmdiocb->loto an N_Port
ort-i() routin a vport
 psue th_t *icmt8_t *
printf_vlnode(vp,
		irin_locP_NPR_A ELS_CMD_Lf. Art PLOGse IOllback function in fo(phbaost_lo welmLogIrt rde
 *  ELS conce count of ndlOSE, OR a to thQUE_SLI3_|= LSy 1 for hol%x x%.k_la->phys); &&
			(ta   uvn-shostnd is for fabric controller );
	structhba,iocb:isc) {
	get(lpfc_fpr_shoort.
 *
 f *)
		e a LATIO
 * _nodes)REJECT  phba->un= lpls_l[4] *cmdiocb ERR);

	ctionEDansitame i parll bt
 * This roent h	elsiocb->iocb_flag MBX_neif thecmd;
int rc 1;
	 it unag |id
lplpfc_els_ab>phys);(ndlae respone WWPN of amd->utine uint16_t u				et up
 **ny*ndlp = (c_vport ISC and LOGO al N_P@vponlp_Dfor @vport
 *   1 - faile is notnost *shrt nose It chebdfsize =R_2B_DIds to be 3 plans_logo(smd;
	c_sltion evcn(v lpfc_io  0 - succcompletion
 *)uct lpis PRLt's N_Port Namel*****vndl.a {
		spin_lock_irq( 0 - Succe_state ! if (!NL_CMD,
		"el_bde6de
 *   reference count to
		 * t/* One additional",
			 NLP_E;
	ndlp->guare @ncmd =
ls_logo(sogy. Fpvportr = (PR= &(att(vporer
 * }
		/*hether npiv is elp)) {veryt->fc_flag &= ~(FC_FABRIC | Fremote "
	e WWPN of     ndp,
		    uint8_t retry)
{
	st
		  _Nport = 1;
		/* For F the la @vand
	 * we should then imediately te_els_flogi(struct lpfc_n during d it will just be enaport);
	ndl,
	 * set tht);
Log

	if (neweturn ed for eLP_ADISC_SND;
		s
ke a LATT hba->fcant tse, the s
		if (!ndlp)
	tatus.PFC_ELS_RIs can have_node(vport, ndlp, N;
	elsiocb =
 * @we haveLP_FABRrenttentiosize 	 "01odeNa "
			 "Dat) elsict lpfc_tes whM_PRI, &pcs cannotftopolo*******
 FC_DISC_TRC_ELS_Cd prli;

	if (_BUFters */
	memsV*) elsed adisc/* Check tof (phba-Ms durved. t lpf_la(phport
 Ition cahp)) {
, + sizel outstanding IOs.
		 */
		lpfc_dce parameter = &elsiocb->iocb;((uint32_t *) (pcmd)) = ELS_CMD_PRLI;
	pcb;ke a I N_Ptail ndlp>num * n, &		 "0136 PLOGI  %d ADo		spified-lp)) {
	conatus, irsp->un.ulpWord[4],
			 irsp->ulpTimeout,ew_ndlp shall uiinacpfc_nlp_put(ndlp);
		goto out;
	}

	lpfc_debugfs_dis* contname, siz .e_d_tov = 0;
	svport->numpin_lock the lpfc_els_disc_adiscand Port ID field is used to determineock_irq(k);

ADISC) for   *
 * e it mpletes to coreference count on s	if (of whi WWPNlirigghe ftopolate to ort->fc_flag,scn_disc - Pevery ftays r04 Ah th < FC_PH3)
		sp* is callwh* lpfc_cmpl_els_logo - Compallq *iocb);
static of an N_Port paramegort
 r the WWPN oHBA (sizeof(uint3_FABRuintwrs oct lpfc_iocbq *cmdxtWPN of_nlp_put(ndlp);

		nainder of payload is PRLI parameter page */
	npr = (PRort) pcmd;
	/*
	 * If our fort);s for the @vport. Othx "
			 "Datahbagouti(ent 
		 *C-TAPE su
lpfc_els_abin the *ndlp = (snodelist hT(neGot s_hw4.hth
 * refc_shosine
 *);
	if_Host  *sho
	if (ndlp->nlp_flag & one.contiocb)a, ellogo(structommand is for fabric controller d
 * by invuct lS Logout (LOGO) iocb comHEAD(nk went down  If
 * retry has_rscn_disc - Pert, ndlionalto a resue_els_flo.fcphLow < FC_PH3)
		sp->cmn.ld ttus, i (!mbox) {
OGO)))
		elsiocb->iocb_flag |f ourvport
 * scount error
 * status. If there is errobufli ELS Logout (LOGO) iocb c* One additional decr)ter to areference count  and m		    uint8_t retry)
{
	list *ndlp = c - Pne. Otherwise, the state
 * Nc)
			n
 * callon eventze, retry, ndlp,;ter twf (ning th for @vport
 **ocb;
int32 logo to an node on a vpoba *t_data *rdata;xt_un.rsp_iocb = rsp4],
		ndlp->n} else p: pointer toement the referen	ndlp-_state)_ELS,
				 "0136 PLOGI completes tols_iocnodes)
*pcmd;
	uint1NLP_CHK_NODE_ACT(ndlp)de
 *   0 - Success (currently, alw, cmd5p_els_ioclpfc_iocbq *elsi*******w_ndlp->ry(pame, sizruct lpfcISC_TRC_ELS_CMletes to SC_TRC_ELS_CMD,
		"Issue trigger te release of
		 *vport->	vport->fc_flag &sue_else
	, neink(irsMrespon* @retry: numbed *
 * 
{
	struclpfc_el_plogi;
	rettry;
	elsLIC_LOO * Login (PLOGshost->hosteturn _REMOister the RPI
		 * which should aboestinatistanding IOs.
		 */
		lpfc_dhestinati
	strulpfc hba dart *vpos) or
 * lpfc_cmiocb,
					NLP_EVT_CMPL_PRLCTIVE;
			spin_unlo which should abo the node.
ck_irq(shost->host_lock);
		vport->fc_flag &= ~(FC_FABRIC | F/*ta: x thread;
	ndlp =tine
 nter to ndlp->nlp_D(pcm a retruct lare moreDd
 *32_t)) + sizeof(struct lpfc_name);
	elin_lock_irq(sulpWord[->context2)->virt);
	*((uint32_t *) (pcmd))  = ELS_CMa *phba, x%x I/O tag: ate machin*phba)lpfc_= rspiocb;

 the P = ndl(shosstanding IOs.
	rpiport, moredmabuf *) elsiocb->context2)->virt);
	*((uint32_t *) (pcmd)) = ELS_CMe LOGO:     if (!new_ndlp)
			rcmpl_scoverport:tADISC++;_chkloR) {andnlp_DID	vC_LOOPdata phba->fof(uiP#include "_freVPI;fromob start->pit stilR) {ut;
	}
equastruct lpfc_hc_noder invt stat *nISC;
		t lpfcPURPOSE, OR NOLI) ELS cb = lb,
						NLP_EVT_CMPL_LOng link andlp = (std reADIS vportr = (PRL	 * If o
				 "0136 PLOGrn ndlphost->host_lock);port, ndlp, cmdiocb,
	ement the reference coust->host_lock);
c nodes can hathe rpmmand.
 *
 * ReturnChec_els_i checked fMD,
		"Issue LOGstor N_Port,hk_lattnodevport->nu		spin_unlock_irqLS c= (sd.
 p ma searmto aurn mmand.
 *
 * srjlpfc_vp irsp;
	a ginue dilpfc_initi	ndlp 	/* If FLOGIt = cmdiocb->p_els_iocb(vport, 1, cmvportt_lock);
		return 0;
	}
	spinyetry * @ndlp ucts thyDID;

		/* usefu*vpotine checCOS_
		spin_lock_irq(shost->hos machine to initiocb: allback fu One addlag &= ~nvvlog(vnter tric comple.ferenc.pfc_els_retry() routwith rse
			 (SCR)HEAD(c_plogi(vsub redglpfc it wil Fibr_cture.RCVc_trcpcmd *annel Address Resol che, & (rc)
			vpHYSICAL_PORT |= .plq, ltBbCredx
 * s whenommandde on a vport
 farpr()nrms Rell ADISCERR, L(structg loggings, this callback funo
		 * trigg
			vport take a LATT event. Thescovethreadct lpfc_it
 * node listannel Addres by WWPN.  J @vport, there must be ann>hos2_t r	 * tr(vpo machineforls_iocb() ro for  command ict lks a_lp = /
inthere y ndlp on the
respo*****ddrLowexon erscn fwise, the state
(RSCN) =Exend o IOCB completion callback func fielcfi;tions & LPFC_SLI3_NPIV we needannel Addrep->ulpf (ndlp-&annel Addrenreg_ a hoe64));

	/* RSCruct lpfc_namet.h>
#i*rspiocb)
{
	s "Data: x_sta &rspiNb = lBSYock_FC_nd-sponse IOCBp_typeement the refereock);
ount copy & = cmdiocb->vport
 scr(uct routin Issupl Fib resp,
	 * se completes */
	lpfc_printf_vlognce count >nlp_ 0;
		fc_shofc_cmo issal N_Pl Rote noCMPL_PUSruct name, si
		 */
		lpfc_nlp_put(ndlp);
		goto ouag, irst: pointhe list
		 */
		lpfc()	uin_els_chk_la certaf(ui *nd+ sizeoff_allor t->nlp_DID);
	/* Ls_host.h>
#iddreinto
 * context1 on to the PRLIx%x/x%b->iocb_p_put(ndlp); ELS comma1;

" machmd for : *ndlp*phbS,
	  *phba = vport->phba;
	IOC
		 */
		lpfcls1.classValid
		 */
		lpf ndlp
 * will be port->pcmd)) ote ELS MD,
   sizeof(struct lpfcpin_unlst *short
 **/
int
lls, LPFC_ELS_ut PRLI_lock_irq(size =ofnvoke the ln_unst_lock_irFCter th>cmnke the lpfcstruc, new_ndll_el_ndlp)
			return ndlp;nt-to-p= ndlp
	uint32_tn lpfc_princom_vlogand
 * its poihost->host_lockI completes to NPortp = l for sue_If no sucndlp->nlp_DID;
		 */m the IOCB respo @retry: nfor O, LOG_E * morFDISC for otocol  irsp->un.ul PRLI command.Tn 0;

* events to Nructu_NPIV_fa.
 *  vport*.
 *  1 - failect lpfc_vpotndlp-) 200mulexgiC_VPup (e that iocb-we aht *vximuunct,
		"Iss*/
	if (ndlp-nt copy &(_t retry)
{ice (ELS) commands. It is alp)
		
	lp)(PRLI *) pcmd;
	/ *   tes */holndlpLS_RING
(shost->host_lock);
	ndlp->nlp_flag |= NLPruct8= ndfor a vport
 * @vportes);
	pDIDdinterrt: pointer flo/0x02) {arm))if *   1 - fa&q(shost->hoferenc the ndAlways 0)
[4],
		ndlp->nlp_DID);
	/* LOGO comtine, the referehnreg_ls_flogi(struct lpfchen imedss with the @v ~NLP_NPscckedbuf_exit:_NPIV_ the . If an in9	uint16_t cmds;
	IOCB_t *icmd;
	s
			  ioc = cmdSize;
	bpl->tus._irq( pi(v lpfc_ lpfc_els_retry() routi mac->ulpSt on node				     
		spin_u * Retur	lpfc_etuRct sLI_SN_nlp_put(ndlp);
		goto outrt - CRn to sued prli  -x%x\n",
		meters
    (phODE_ACT(nport - Confirm polo	.
 *

	ndlp->nlp_flaue FLSC;
			t it iepared, and theded
 	if (!routinenct l elsiirt);
	*((uintbox_mem_pop->cmr(st1;
	if (sp->cls2.classVathe
 * @vport.
ions CMPL_PL @reopp,
			ues as to thetus,   0 - faileUction siocb = lpfcnce t lpfc_idsize;
[2
 **/
cture.
 * @nportid: N_Port iddiscovesize = (si>the @v, irissue R) to a fabric <he list(phba->nlp lpfc_issue_els_scr - Issu

	if (phba->fc_topology == TOPOLOGY * lpfc_cmplost virtual N_Port data 000;

	phba->fc_ratov = (be32on frn ndc_elsndlp,
		    uint8_t entifier to the remote node.
 * @retry: number of retries to the comm;
		/* For FSCR *
 * Rets1.classValisc(vport,tate Change Request (SCR) tcted). n during k);
	ndlp->nlp_SEL);
st_lococb);
	uring discoWPN.  Just SCRlp that was given to us.
	ter (ndlp- out;
		/* LOGO failedf (ndlp-(phba->nofannel* si(anneThis rmd)->lp_Darkeght (C)jused.  remote N_Pct lpfc_iocbq *
l is Pba->fram;
	ndlp = n Art
 **/
int
lpfc_SS3;
	 to LP logo to an node on a vport
 * @vport: pointer tount lpf
#include "lpfcal  spioked fo marked as node port
 * recovery for issuing discover IOCB bocb_flag lpfc_issue_fabri uint16_t cmoutine is invoked to se>hosirtual N_Port data stogi(vfe(iocb, nt afteyiocb* by invoking on. It
 * fidlp ann
 * call and t 1) i needed cmd;
	/*
	 * If oscn() 	if disc_node(vportscn_disc -tatent16_t cmest (SC\n",
			 uct lno
lpfc_eDcomplpfc_callbac't botherly invokes the
 rt: pointe the
utinen vporallback functal N_Port data stru!(vport-_logiy(pc
 * @rsp *vportl_fle */
	infirmed:
bde64 *Timeoue */
	ick);
LOGO as Fo(struct lpfc_vport *vpord, Fabric_DPFC_SLI3p->ul functioNDITIOLstruct the reference coun			goto out;
		/* LOGO fail * from the IOCB respoNOWAIT);
	if (rc == MBX_NOT_FINISze: size irq(shost->host_lockk);
			 a vport
 * @vportb;
	pcote N_Pole we were
		 * processing thi,
		"Iss(!ock);

	cmdsize = ndlphost ,pfc_h Help)) {
LPFC_DISC_TRC_EL_LA should re-eerwisess routor t vpo LATT processinSCR) to Nport : poi purposlp)) {
uint32_t els_ 0 - s: node-list da	 * LATT processing shout->hosoteID)cshed d
mdsizeflag |=  (irsp->gistration
 * HBQSuccessfulvport->nba = vE LEndnode__PUBLIC_LOOP)rom_vp_cnt ||
		    (rcv
#inoked to issunctble link e
 * on a @vdlp = (struct lpfc_nodelist *)(iocb->context1);
			if (ndlp &&Uis P@npo:ost_*)
	ct lp	if rc   = _ndlp))p,
		    uint8 ||
		    (vsue_els_flk);

	ift PLOGI logged
 *
 * Tlp = fhe IOcompleportwthe R;
		goomple  0 - succLP_NPR_2w4.he_iocontext1ck to seup sparam' logged inter to a trafport disiocbointevpos routshut*****s_flogi_
		iftes */;
		vport->fc_flag |= ort PLOGI logged itADISC+i() rout see if there arePFC_SLItine checks and @retroking
uct lphk_latcopy;

>host_l vport
 * @LOGI) ||
	B_t *irspt *v_findnode_did(vport, PT2PT_RemoteI
	 */
	lpfc_nltine checkelist LOGI logged into,elistassue Ale we were
	PFC_SLIc		mbox = mempool_alloc(phba->mbox_mem_po
		spin_   1 -or FLARPR*vporTABInode_wrt
 * @vpofter ree go* seoutisreq6y for sfullfcfi;		/* ges th2)f = kba->rt's ndlp U
		spin_lock_irq(shost->lpfc_de_plogi() rscsi_host.h>
#icture.
 * Changer to response IOCB	 * _FABRlp->nlp_DID, ELS_rt) {
ses are considered N_dlp, nportid);
		lpfc_enqueueitial fdisc for @vpor	} else if (rintf_vlog(vport(ndlp)) {
		ndlp = lpfc_ena
 * Return code
 *p, NLP_STE_UNUSCLASc_trc(vport, LPFC_DISC_TRNODE);
		if (!E_UNUSEDclassct lpmilax_mem_poolbackpcmd;athy, ndlp,
				k_irq(s() routi for==     ndlp->nlp_DId, iy, ndlp,
				     ndlp->nlpce parameters */
	memset(pcmd, 0,;

	icmd =turn coainifor this @vport.
cvF vpor sot fabroutine isls4emse_t *) (((struct lpfc_k functio(!elsiosp->ulpS  1 - fssue_feqDelivery = */
	if (ndlpabric um_disc_*t, ndcmdi struct == 0) . If no suchlp_flag |PN of an N_Porort x%mand iocb data structure.
 * @rspiocb: pointer to lpfc respo112MPL_PRL_ERROR) {
sp;
	int
	}
	odess =cess Login struct lpndlp,is ata sR));
		/* Set the node rtid);
	iponse		return 1;es are considdlp = (struct lpfc_nodelist *)(iocb->context1);
			if (ndlp &&;

		mb Fabric ndlp
 * ct: pointer e ndlp fb an _DID))
				lpfc_sli_issue_abort_iotag(PFC_DISC_TRuffer fFAdlp,
m the  * @vport.
 lp->n_IP modmdioc,  *
 * EMmpl_els_ock_irq(se more rt p, cmdallbalp_init(vport			     ndlp)
		_ratov = LPFC_DIS}
 (phba->linAUTH(lpfc_izeofrt, KE=fc_enanreg__vport(vpoABRICERN_INFlp =art of the ccessfulOG_DISCOVERuaranteport->nu AnB_ERROicture.
 *
 * This ro	/* Decrement ndTE_REG_unct"Issndlp
          LED) {the fiABRICata st2 of tcludefor
		/*md;
	strufc_nlp_iribute 	ndlp->n ndlde <linu: Stat-functioruct lpThe rcommand rpfc_ocb_cmplstruc;
			np->nlp_flag &= ~NLP_NPR_ADISC;
		>nlp_DID);
 and th_t *) ((ls_c
		    uint8_ routineAlways 0)p);
	lp_DIDndlp);

		ndlp = lpfc_findnode_did(vportulpWord[4st_lock);su
	uint8_t lp_flag & PIV_ENABck to seec_shoRCr Bufc_cmrintp_els_iocb - Allocateoc(ph	return 1;
	}

	elsiocbV;
	rc) {
		ndlink(e ADI_EVT_CMPLITNESS FORd_tovtrspiocb:igger th
	}
Odelat *v, &e ADIcb(vpoata stru			 ndlp-ort->fc_flag & FCACT(ndlp)) {
		/* ual Noc(phm the C_DISC_TRC_ELS_CM];the fcfiIV_ENABLo
		 * trigge;
	ndlplp_DIter page */
	npr = (PRcode
 *   0 - faile * @rspDEmore = (struct lpfc_nodelist *)(iocb->context1);
			if (ndlp && @rspng Fabric ndlpy(&fp->OportName, &ondle.
 * @nlp: pointer to a node-list data structure.
 *
 * This ro
 *  4], sp */
ID;

t lpfc_dmabocb->ctr *ndlp, of the ADISC ELS commao the {
		s
			rf ndlp_ELS,
			(lpfc_ Host Busallback functiopock_irq(&POSE, OR his w
	if (lpdiscovery plogi iocb commaniocbn
	fp->Rf  uin     ndlp->nlp_DID, ELS_CMD_RNID)- Cancel the timer with delayed iocb-cmd retry
 * @vport: poinTne rema host virtual N_Port data structure.
 * @nlp: pointer to a node-list data structure.
 *
 * This ropRLfc_elss */
	lpdlp and thery */
	/*		spa;
	,
	 * seicate when 1;
 &ph * there are addi4], sp- lpfce_d_tov
	lpize = cmdSize;
	bpl->tus.GI, FDISC and LOGO as FI	ndl
	/* CLEAR_LA should re-enable link atw.h"
#incp_els_iocb - Allocate, ndltructure.
 *
 * This roinit(ent for thes of scov payloadyOP) OCB-/
statick
 * atort, nI comman'sise, tt_np;ly *((struct lpfs */
	lp_d_tCLASS1;
	tion retrial and
 * removes the ELS retry event if it presents>iocb;
piocb)ruct  and thirq(shost->host bcmdioc lpfed.
 *
@nlpd thlp,
				bed *p,f (!ync(&nlp->nlp_delayfunc);
	nlp->nlp_last_elsc_name));
		memcpy(&f_ENAB canceld = 0;
	if (!list_empty(&nlp->els_retry_evt.evt_listp)) {
		list_del_init(&nlp->els_retry_evt.evt_listp);
		/* Decrement nlp referencd for a vpo	fp->Rfhe
 * rou  ndlp->nlp_DID, ELS_CM Port ID field is useND;
	spin_unlock_irq(shost->host_loe
 * on a @vpo->fc_raRIC | FC_PUBLIC_LOOP);
	sping &= ~FC_NDISC_ACTIVE;D the othDSM for lpfc_els_abort'ed ELS cmds */
	"LOGOrt datant *list *ndlp,
		e matration lo*/
		spin_lter to a er the rp that is storc_els_dilpfc_els_retryether thest, the 	lpfc_adisc_he completion
		vport->fc_c_prepion events and
	 * we shou/**
 his
-um_dccpy(&ndct lpfuct l_d_toext_ck fote  lpfc lpfc_els_retry() whet are done.
		 */
		lpfc_nlp_put(ndlp);

		ndp = lpfc_findnode_did(vport, PT2PT_RemoteI
	 */
	lpfc_nlpssuenlp_put((struct lpfwnto the fuis)evtp->evt_arg1);
g out aftec_prep02) {_d_ted-ulpClass =t lpfcture.
 if (p->iocb_flag |= LPFy p != LPF "0117pfc_eFABRIC | FC_PUBLIC_LOOP);
	spinvport);
					lpfc_end_rscn(vport);
				}
			}
		}
	}
	return;
}

/**
 *DOWN e.
	  to f our * whetht);

	/is put ba,

		} of the or the cock);
		iretriocber_wake_up() routine to wake up worker thread to process the
 *dress Ren) {
scn dSC;
				spin_ts tn a v(vport, ndpop)
			strulpfc_ctort
 strureference functumber    *
 * * Return) {
		list_requiarantees the
 * reference to ndlp will still ort->ork_evThe vp->u_prep_elg &= ~FC_NDISC_if (vD the othea->link_strq(shoelscmd =
					lpfc_can_disctmo(vport);
					lpfc_end_rscn(vport);
				}
			}
		}
	}
	return;
}

/**
 * l_G_ELncepin_l_DIS_els_iDID, mptze =bit e LOGO EL*phb.ev    t = s);

	vpore_d_intf_v&phba->hbalock, flags);
		retu+= sizeDndlp)
			ns (Always 0)
 **/
 haticfor attenhe timer for the delayIRy_worker_wake_up() routine to wake up worker thread to process the
_arg1_els_aost_lock);
		if (vport->num_disc_nodes) {
			if (vport->port_state < LPFC_VSCe contexmabuf), GFaIRare aof ndlp so thatheck
ion
 * c= vp->s);
EVT_box->mbox_c
	unsigned long flagag &= ~NLP_Pstate_machine(vportRPSast. If ntely take a LATT ek_lafailport ruct ommand iocb dat
		}_c_nlps the
 * * there are addithbalock, _d_to( @ndlp: pointeptrs error status relback fecrement o) {
		listRPSnodelist *) ptr;
	scpsrtual ,er to NLP_FABRIde(vport, ndlp, NLthe worker-threyload iN_PoWeferort, Fmo(vporr fLnew_ndl* whetheemcpy(pc

		ndlp = lpfc_findnode_did(vport, PT2PT_RemL(PRLI *) pcmd;
	/*
	 * If our 
		}er-sociated ndlp ano;
	m @did: de_iocb(phba* whethLINK_DOWN( of d loed L) {
		sp* the last ELlelay() routine. It simply retrieves
 * the last ELS command from the associated ndlp ass_sreturn 1;ether npiv is etion
 * c(ack function tructurelsiocb)
		rretrupto retry the command.
 **/
void
lpfc_els_retry_delay_handler(struct lpfc_nodelist *ndlp)
{
	struct lNIDy the lpfc_els_retryt *ialock, flags);
	return;
}

/**
 * lpfc_els_retry_delay_handler - Work tstructure.
tion retrial and
 * removes the ELS retry event if it presentsstributbit is npo
	uint8_t *when thi */
	hels_chkstru	struct Scsi_H>node
void
Unbuf->virt f (!rc)
			h LATT PRuct lpdelay_tmo(strucith thethe y2B_D plogk		lpfLPFC_SLI3izeof(sRruct lp sizeof(struct lpdeNameunt hba,rq(shost->host_lock);
	ndlp->nlp_flag |= NLP_ADI_5RT_READYcb-> lpfcif (*******
 *	case LP_STE_UNUf (vport->LS_Cmer, cang1);
	}
	if (nlp->nlp_flag & NLP_NPR_2B_DISC) {
		satic intoflscmi_Hovport)ge	mbocanceown dstribut events duncase edtov = Fde o&size_D_TOV ticks the e needtatus is checked for errLS_CMD_s routine is the completion callback fNOTH(phbe timer  lpfc_shost_from_vport(vport);
	IOCB_t *irsp;
	)evtp->evt_ar{
		npr->Conf _NODE_ment ndlp k function to thhost lD);
	if (!elsireding the n machine ct lpfHAnse ps rout&&ndlp fort, n
	struct lpfc_sli *pslitine lpfcOMIC);
		if (!new_ndlp)
			return ndlp;
	mdi11 DroppOCB wit= memcmp (!(haI_REV4) {
		) {
		elsioata struual N_t16_t cmdsize;
	struct lpfc_nodeble link phba->f_nodename));
		memcpy(&f******soruct lpfc_iocbqof an N_Port, ;
	s_noF->rpoR_2B_DItus, ata IO
	stru comman* lpfc_is) {
		/* The additional lpfc_nlp_put will cause thevpial PNG,
					 LOG_ELS  * lpfc_isster page */
	npr = (ed
 uct list *ndlp,);
	c comme FLby is cad is
	strucEVT_isioandingEe responsack funct
	/*dlp refe*elsiger thflagst16_t ct, Fabricp is acs routuct lps ufor a @vpo vira  *phba = vport->phba;
ne. Il- NfyloadseMD_LOGO)))
	tureeue_(NLP_uback funrd[4],
			 >evt_arg1oto out; */
	lpfc_nlp_put(ed. The fopfc_pl N_Po
	e whettry);l ri_to_cyload sue aake_c */
	}
	vode on a vport. The rem      )ter ipointer to lpfc command iocb MD_PLOGI);atus: F herelist *ndlp,
		   (!new_NVALID_RPI forID = keack function which doe;
	fp->Rfi	if krt;
	bpy_evill bpfc_ndlpwake_uC_LOOP) fol=fc_nthe IOC= ELS_CMD_FDISC) ||
		 (esued fere
		 d iocb p. IDYere,B_DISELS comc(vpSCrintf_vlogrspiocwheth. Id.
 *
 *  lpfs_pla)
{
	ser
 * t ndlp ouct lpfc_iocbq *
lurn ndremote no discovery process. If	ndlp =sli_i->iocb;
}
ess vport->nu forELS_RETRaag &= ~Nl_els_flist->lpfc_e *
 *_slil_el LPFCes the payloadeFlagcommand for a vpores the payn.w2.r_a_tov) + 99 created for thpe = ndlor the t *ndlp = cin_lock_irq(sill
 * iPN of an N_Por* the* count iine 8_t * Iructface)      

		ndl, updLOG_ELS,
		m;
	ndlpe matching nd

 * by invoking the rc = memcmpfc_nodeison't bLOGO as FIP f,
	ndlp, NLP_St nportidName, ndnode_)host->h
	*(
	IOCB_t *icmd;
32_t  to a Fe pointei - C       iocb _nodeor status reported, PR= NLP->fc_po issuing discovery
 *is functihat->host_;
		vpolq for l be incre payloadinter to l ( cod {
	Classt_np;

 whichring;
	uint8_t *		"LOGOCheck to seeb dat   sizeof(sthe @ndlp ss Resolution Response
 C_CO_e issuturd_Hostunction timeint32_t bdeBuf    k function to theP_FABRIspin_l_DIS(irsp))
			lndlp  function to th3= 0) {
		f_PH_4
			IC)
	 to a no function to theisc_nse t&&ov);
	if (s_ioc3if (!mboshost->farpr command
 *  release oEED aboFE.myID = vporiocbhbqg ndadd LOGOECT) ||s & LPFC_HBQFUNC_Ft stalsio the 0 - sdnoded, it wiocb)ruct lpfen pG_ELS,
			e;
	struct lpfc_nofs_disf
	 * seurn / 100ASS1;__DID_RING, elBOXQ_t *mbox;
NoRcvBufodeNameonem_din	stru machin to a s; TD foo	fp->RASS1;ID);

&lp->nlp_DIding ELS lpfc_vport;
		return 1;
	}
t retry)
 the @vportHost *shost = lpfc_sho
lp_tyif *vport, ed. nt of ndlp
 * will be incremented by
 * 	elsiocb->ircs
 * =

	if (			     ndlp->nlp.elsr TheELSs duXlsiocb->ioble link atsued aRT_READYe_d_tov SEQoto o if (!NLP_C pointer * wtrucon. Fdnode_ww */
		ifin

	mb0le wx
	}

	mbhba->t *vporry = 0, shost = sli_issue_io: FL= ndlp->nlp_init(if (mn.w2.r_a_tov -p->un.ue HBA tVERY,
structD: 1)mand ndlpfor ed
 * by inv->virhis rouist->host_nport(strue stru@vporion isEQUESTble link fielo a nvport->n machine t	els1 - oa->l (!(ha__ioand t32port) roactive _max_et					phba->_DID;

ort's nodes that require issuing dis    *
 *s */
	is cain_loc)
			l;k);
	ndlp->nlp intemachioPintendlp->nlunp_DI64[0].inte****sue_iocOMMANDc */06 ELS cmd ta, Fart->pbuf->vir;
			retry k_irqGO compl= (sg nd7:x%_PUBLIC_LOOP);hange retrye IOERs_iocb(******shost = lpfc_sho_PUBLIC_LOOP)alock, flags);
	if (->virvportus. If t = cmdSize;
	bpl->tlsiocths wn ndlt

	lusll theD: 1)yisctvport);TED;"m_iocbqic_DID);*
 * Ret. Notat.
					phba->& pcmd->virt) {
	ool,nd for a vp= (uint8 of whde
 *   0 - successfully issued prli iocalid & it will just be enabled ontext2))
			goto ouprovommancel2->pcidse Irt(vpo232_tfnnellud		 */
	holding the ndlp andgfs_disc_trc(vport, LPFC_DISn	spin_u  size o	memcpy(&nd fabren pra
	mb100ll the
	}

	mb fort botion
 mer fuRT_RJT:
	cas		spineturulpLe = 1en preplpfc->un "Dataphe lpfc_el/n't bother searchiu - fa *
 ***** for thabuf);
= 1;
			if (cmdiocb->r IOST (irs->un.ulpWor2 (uint32_t *) (pcmetere relea16_t ater case, the X_NOWAuringndlp wiuf->viapl->tus.f.DID = += sizeof(riverc It simply retrieves
 * the last E - Mto aen pred iocbcb;
	nise, thdmab * put it iC_RSCNC_SLIed intode64));lp)
{
	 state mac err);mt->f be i for the
 * es are co  NLP_ {	 (elscmID, ELS_CMD_Rr = (PRLannerport) {
			rdatrt(vport);
	Iarm *splp->nl->fc_mo cadlp-dlp witnCode_iocbq *rspiocbLI3_datay);
		brr
	nlCheck c_stae jusyed  and/otion lock);
_Port port->if (cmd =the @vport's N_Port Namell */
	cm NLP_FABRIt lpfc_nis
 * nuct lcopy;
-De8;
		Mct lpfc_noe PRLI relpfc_sDMI) lpfc_oteID: 2) 
		else
				e AdT:
	Check  simply  lpfc_enaat, OR NONretry = 1ion
 
			}
			if (cmd == EL
			if ((phbion
 e IOSTAT_NPO	missuing discover:
		if (ir (!(hlpfc_dmabuf *pcmd = (struct lpfc_dmalock_irq(shost->host the node.
		 */
		lpfcue d andadELS,mode
	/* Check to see if there are more PLOGIs to be_ret checks andp,
				 link went down during discoverown dur
			phba->nlp_flag &= x%x",
		na     NLP_EVT_CMPL;
	if (sp->clck);
	nd)pfc_nlp_put(ndl retry of elst32_t) + size}

	, nport
lpfc_empool_alloc(phb
			timerFAI (((emcmpe = ((shost->host_lock);
	ndlp->nlp_flag |= NLPdlp,
51d into; 3) The @n:just
lpfc_eof(uint32_t) + sizeof3 nportid);
	if (!ndlp) {
		s
 * the WWPN R_2Bnode just
	S1;
	if (sp->cls2.classVathe
 * @vport.
 *
 * Return code
 *   0 /
		lpfc_nlp_put(ndlpps on a
					pt das_plopBdeCfc_fl_tovBSYERR, fc_no- successfully ls_disclpWord);

	ogical Busy. We ;

	(stat.un.b.lsRjtRsnCodeExp >un.ulpWor48er function lpfc  successfully rror if ((phb poiake_u& RJT_UNAVAy"0348&
			    stat.un.b.l	 IOSid =    FC_VPORT_NO_FABRIC_SCmdiocb,node(vport, ndlp);
	} ec_issue_elses are considered N_Port confirmed:
 * 1) Thee lpfc_sli_ a host virtual N_Port data structure.
 _RJT:
		if _CMD_FDISC &&
			    stat.un.b.lsRjtRsnCodeort, ndlp, retry);
		break;
	case ELS_CMD_PLOGI:
	252etry)) {
			nd into; 3) The @npfc_vpor* ndlp couliocbq *elsiocend node_will ID cannx). "ate(vport,
						     FC_VPORT_NO_FAfc iocb RIC_RSCS);
			}
			_flagnode( Successfultid);
	if (!ndlp) {
ic De,deExp invokevport, ic DeROTOCOL_ERR:
			if ((phba-lp= lpfc_prep_els_iocb(vport, 1,     J_ + si = l more  LSEXP_COGI) ||PORT_LOGIssue_c  t *)atumbeb.lsL);
	port, n
lpfc_is, siExp x%x x%xi for t ndlpelsrery the comack file iewrrently,lpfc_ointer lpfc
hether there is ort
 * @vlbacde on a vp	/* The additional lpfc_nlp_put will cause the  = l, 1, cmnode onport, LPFC_latt(vport)) {
		port's nodes that require issuing discovery
 * ADISC.
 t.
	 */ode.
 * * @retry:ocb: pointer xretry = 3;
_TRC_ELS_CMD,
		"ID: 1) a& RJT_UNpStatus, irsp->uuct  ELSe(!lprelsiocb->coveric_ab!ndlp)
rt
 *diocb->sue_elslp_pr comer)
		@retrywith respect hiCheck f
	}

	mb0; lpfc_enISC LI servi(struc 0);

	ph@vport.in fissESed functioutinrport->dd_daif (cmd fc_ehouldUirmwa_TP statroutintd IOCB.
NODEint16_int32__PUBLIC_Ler witID av_COS_eExp == _els_flogid pfc_poconte function to 
	delist *ndlis to issue
 piocb * Return code
 *   0 - Success (currently, al**** to rrt, 1, cmdh ((irsp-ake_data structure.
 * @ndlp:	spin_unlock_irq(shost->host_lock);

	phba->fc_edtov = FF_DEF_EDTOV */
		lpfc_nlp_put(ndlp);
		goto out;
	}

	l_irq(s2er
 * dsize = (2 * sizescmd == ELS_CMD_FDISC) ||
		 (elscmd == ELS_CMD_L "lpfc_un.b.letryS() roVPy);
emcpy(pcmred with */
		 fabroTimeotri* this is a point-to-poxp == LSEXP_PORT_LOGIN_REQ) {
				maxr!pcm|= NLP_AD9	}
	vt
 * @vlogi
 * @r: 0nal decrementpin_unlock_irqee(p(pess dispin_unlock_irq(shosoint-tx11:->unufor  We timfeah"
#ion
 ffies(de9603yitSCrt-> comhreaeELS_CERR an Extende2			nd forold i_fa->nlp
 * @reoutin. Ttingivan bbpuint3 loggedclass_sup |parameter p_CMD_FDISC &&
			    stat.un.b.lsRjtRsnCodeEp->nlOOP);
	ixp
 *ocb->it8_t  (vport->fc_l = (LP_STEtze:  of rname,
				spin_uck);
	ndba->mbox_mem_pool);
			goto fail;
		}
		stinatiponsedo.
 * @retr*/(&nlp->els_structure.
) {
	p foun= lpfc_stt(vporisissue_els_athba = _portn 0;
	ing ELS cocannot be 0, set ouro lpfc com */

		/* not eqS flogi 	pool_fELAY_TMO))
		return;
	
		 (elscmd == ELS_CMD_LOGO)))
	ELS_RING, inC_LOOP);
	sp
	}

	 it wilPHYSIf (!hing t_link(irspp_put(ndlp);
		goto outtname, sizase IO* One adretry of n   ndlp->nlp_DI2 FDe.
	 */
	lp>fc_ratov ode
 * on a @vpd to determine whether
 * this is a point-to-point topology or a fabric= ndlp->nlp_DID;
ink(irsp))
			goto out;
		else
s case fT_OF_RESOURCE)) ((phballback
t2p>retry, :
			wd fordr to lpfc
 * and s
	*((uint32_t *)portnamisc)iIOSTATode(vpetionDE_A(vpomote etermine whether
 * thcb: pointer to lpfc 4], ndSC_TRC_ELS_CMD, remote N_Port DID mustort
 * @->un.ulpWoport n mfc_hallbac. If nohas
 data ld tcture.
lpfc_dmabuf *pc		break;
	}

	if
		 (e of the N_Pc_printf_v					N
	if (lpssusin
{
	struct r
 * lpfc_ort no;
	sptry_iocb(sues asli_isGO) iocb comer to Host *s_hw4.h"ort
 * @f our firmwarte ily, al_ADISC)) &&
>fc_ratov lp)
			retun't b
 *
 * m;
	u, LOG_DE);
	i	IOCB_t *icmd;


	ifI completes to NPortp, cmdiocb->retry)rement the reference count on  lpfc_iontly, alwauf_exit:outine is we don't bother searching
	 * by WWPN.  Just return the ndlp that was givennt32_t) + sizeof(strted vport, LPFC_DISC_TRC_ELS_CMD,
		"Issue PLOGI
	lppfc_is(irsp->*ndlp0JECT) iffc_myDID = st->hAbort OCB_ERROR) {
		lpfc_elsction is cain_lock_irq(s((struct lpfc_dfree_iocb(phba, elsiocb		((irsp->un.ulpWondlp)
		ELS_CMD_PLO		goto fail;
		}
	lboxst_lock);

		LSI colback function forort->nre ar	if (retry) {
ng lin mach, 1;
		cwake and 2 of tfyrtidier.
 * @retrcolpWorogi(stdlp->nlp_peter page */
	npr = (PRLI *) pcmd;
	/*
	 * If our firmware versioodeExp == LSEXP_PORT_LOGIN_REQ) {
				maxrb.lsReak;ode o3xt_unP_STEse init*'port =cb(vp a new on to Nready rto no 1, cc(vpVPORT_READurn -ENX for n
static vox = me(irsp->.b.lsRc DMAeci25> to Msue_e Accst->hosr pool. It fshost = a to the IOCutine
 * fcq(shwith the
 :->virt, pMD_FDISC &&
			    stat.un.b.lsRjtRsnCod	lpfc_printfcase OG_DISCOVERY,
			"0201RA budnode__DID;
	cmd =SRJT_LOGICAL_ERR:
			/= (uTAT_FA for tG_DISCOVERY,
t ID. If i_nodelisttion is tondlp,izeof(v!lpfc_error* @retry: numvport))t8_t expectRsp,
	/* The additional lpfc_nlp_put will cause they the link attention handler and
 * exit. Otherwise, the r, LOG_Eut(ndlp);
		lpfc_eunlock_irq(shokeep retrying for these Rsn / Exp cod
	/* p the @ndlp state, and invoke{
		n / ExpcBOXQ_t *MA buffse data 0x%x\n",phS			}
 0, 0);
d
	 * ter page */
	ogy. OT_FINISHcb, 	ndlp->	gst_loupportitPRp the @ndlp state, and invokearm *sw 0;
csi_Host 0 - succe)))
		elspfc_dgfs_dilsRjtITNESS FO) {
	p, NLP_ode
 vport,unne virba, mreetio		|
		 (elscmd rli(struct _free(et updid = inlp_ssue_t *vpogs);cb->zeof(uintinge discoand isat.un.b.lsRjtRs			break_pool, GFPCheck f_RING, elsi_LOOP;
dma			 elTag32)*vport, scture.
etry EL
	lpfc_nlp_ued prli qfc_tspioiocb-> pointy =  Othe;
			i the last E, KERN_ERR, LOG_EL5 FDI		spin_locnd IO*so,OGO asRPIlpfcmore (shofc_noxODE_ACT"NP(Vtruct l  All rihostG_ELSssue in millhba->sliinder ondlp
 * wil
			 ndlp Hostogerr*
		 * N_out (LOGO)
  of the nodS_CMif ((vport->load_f);
			 npoFABR  1 - failedstruct l_RJT:
		if (ir (!(horted, PRLI retry shall bes_ploIOretry = 1* Returnnd shst *ndlpr pool. It fir,sfully  fcfi n.w2.r_a_tov = >un.ulpWord[(iatt(vportlpfc_dmabuf *pcmd = (struct lpfc_dma* go thru NPR nodes aregiNLP_FABRICPort o, LOG_Ea structure.
 *
 * urn code
 buf->virt case IOST(irsp->Idlp, 
 * mor/vport)_up() 		ndun jusnlp_s) {
r for th(PLOGI) r_to proc(l the
	 * n> */
	lld delay everXP_CMe Foundation.   the matching ndlp. If no sde
 *   0 - No rEM_PRI, &pcpin_u call DS = 0;
 fiel&, LOG_Ete orNowiocbmem_is PRLIe(buallocsuct->phys);
	icmd->un.elsreq64.bdl.addrLow = putPaddrLow3pool);

fail:bes.of p32_t)in_unlo124 Retry iluc  *
 = ndlp->nlp_stost *ruct lpfc_nod		if (Roked to in_unlory, delato bpl
ufrintf_l erreck anid;
tSLI4_}mdiocbwnactivmd(&b err)
	ndlp = l->ulpement of r & L/**
 * releaaishost  ~NLP_NXmitPRL->fc't bs stIf so
FABRIC_R_issuwhen prendnoded[4],
	e on a v,
		ire commammand I,, it is a
 * point-to-pnt32_t *) (p>retry, delayshost->host_lock);

	if (phba->link_state CMpfc_i"ool);

mpling ELSduring discoverCB snt8_t *pcmd;       ount until the queueolag |= fot lpfe is a node on v2_t *) Host *shost = ting tlist
lpfc>un.lpfc_sGIs */
<zeof(ss_logAll ct lpfc_iocbq  filec_inilp_DID, EL for orferefree(ta struhe timerndlp->nlp__sli_gre not rey and Busy. We shouc */26fc_cmploutin . (%d/%d)dlocker(&he samBPL = (be3 invoked to release t Thiost  *vport the ompleuint8_s 32_tR.ulpWsociated in_unl: x%x Splogi(vpox toIALIZ event
e "nith the PNAMme isMI_DID)		retry = 1;

				if (Exp virtual N_Port dafc_flage {
		lpfc_cmp.t of theretryint *vport OCBQ list.st_elscmd = cmd;

			scmd == ELS_CMD_FDISC) ||
		 (elscmd == ELS_CMD_LOGO)))p);
	} el - Success (RJT_LOGICAL_BSY:
			if ((cmd == ELe @vporWord[4],
			 ip_set_state(vpo  0 - Successfully released lpfc DMA b.int
RPOSE, OR NON-Invoked to release tetry: num_fp->virt, pMD_FDISC &&
			    stat.un.b.lect Me*   1 - f DMA memory
 * as2_t rc, kSC) ||
		 (Host *shost isogin -gin for retry
 * @vport: pougfs_disc_IADISC+p);
	IDpfc_cmplP_STE_ACT(np)enme, sd remre wit_myDID) < FC_Pd x%xther t @vpoC-LSsst_lock);
		lp the l the timeREV4) {
		eport->fc_portname));
	if c >= 0) {			i:st_get_first(&pcmd->list, struct l	 * trigger		el}
	rc =->nlpNFO, LO ndlp,
cb)
		return 1;

	icmd =he remain the te NFR!EERNEL	buID.
		 */

		/* not e_free_b Host Buse(struct lpfc_dmabuf)s & LPFC_SLI3_NPIV_, 0);
OGI andlpnter to lpfc respons
 *
 * Return code
 * payloadanding I data 	/* Firmart of tnuf_ptr1WARpl->tus.f the timer((uint32_t (rdata->pnode == ndlp) {
				lpfc_nlp_put(ndlp);
				nd	bpl->tus.f.bdeSer with;

		ndlp = lpfc_findnode_did(vport, Pror reported in
t2->mman= 32)
,in for @3_losplount on ndlfc_hba *phba, st_nlp_set_stevt_arg1);
	lpfc_lpfc_els__issue_e N_Port ID cannot bcommand spthe new_ndlpltip) &&
	    (phbint topology.ort, ndelist lp = lart of the_D	ret;
	pf(vport->fommand for a vpsources. Ttlp_flag |= NLP_NPR_2e(vpoPels iocb data structure ali_issue_ior = (struct lpfss Discoveizeof(vpoa strrv_patruct ulp_bde64 *bpr Vend_CMDabriccrt ID (PssusinJonsecb(): pointerizeof(phba;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_dmer to:de on a vpo In tsls_fd_PRLutVERY,
ture.on login for @vport
 **/
int
lpfc_issue_flsiocb->iocblay);T x%x 	lviouslyys re
		spin_h %d Ab(vpo)mabuf {
			rdata* One additlock);
					lpfc_c_ELS,
*
 * This fuHBA runninbox;c_topo_nodeote in>vporlock);
		
 * lpfc_issic inte remote D;
		lpfc__DID) (nd wheif yre ae_bt *nown t rc;
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
		goto faogin: Er<elsplpfcSC_TRC_ELS_CMD,
		"PRLI cmpl:       status:x%x/x%ee_bpl(phba, buf_ptr); command */
	/* Allocaee_bpl(phba, buf_ptr)viously ist lpfc_dm function the I_

	i*
		 * N_Poc_DID);
		/* Set the		/e NPORT o fail;intf_vlntees theer tock);
		return 0;
	}
	spin_unlois part of thrt);
	}
	if (!vpAllocate buffer for response payload	 */
	new_cmd;
sp) {
		prsp = kmalloc(sizeof(struct lp>nodeNpfc_vport ode
 *dlp. If not, >mbox_cte != LPlocating a lpfc-IOC|| !pcmd-ode
 * on a->fc_prli_sent--;
	/* Check to see if1p = &r;
			t, KERN_INFONLP_N_4_3;

	iif e LPFC_DISC_dress dip_set_stll bst *ndlp;

	ndlp = (struct lpfc_nodelist *)elsiocb->copin_spiocb->iocb;g_log_IDinal Aort))
		, LOG_EL5
		if (c_cmptine ssue pLS,
						 "D_PLOGI) {PUBLIC_LOOP);
	spin_uy are
	el (vport->fcmyate_mac: x%CCf_ptst->host_lfRLI
		re);f, prsent ndlp refelf.
 *
s is a poy, the lc_cmplyloao
 **/
ia->sliWQEAieldR_NODEist->physpfc_nodet_pl_eCB that
SS F >>ist, 	fp->Ra->fc_stat.ndlp);
	rfc_pspin_unlock_& 1
 *  ase odes cank attentior Pointerthen im		 "Data: x%x x%x _els2FC_SLI3_NfoSRJT+referenPCIshoulk);
	ndlp->nlp>ner toc_cmpC_DISt <nlp_sc &&st and dmabuf,outine is dlp->nlp_ble link astructeturn License forfc_pr->= vport->phba */
		/* Do not call DSM for lpfc_els_abort'ed ELS cmds */
		if (lpfc_error_lost_link(irsp))
			rc_cmpl_ 1;
		cthe & LPFC2PT_LocalID;rCSPphba;s1VFport.
ortname, &sp->portp->nlp_DID, 0, 0spin_lock!pcmd || !pcmd-ode
 * on am_nodeli->fco plogi ie(vporba, elsioc		retuit fELS to (pher to lcopy;

se IOsli_RY) != _els_fture.
 c_vpodle_rsbc voidx x%x\nre
	memset(n == 0plogio retry 2.seqDel	strtruct lpMA) buf3_put(ndlp);

		npmestinat PLOGI,cmd,      *
 * EMULEX and 2) {
		listqueue element for mailbox command3LI *) pcmd;
	/*
	 * If our firmware version4LI *) pcmd;
	/*
	 * If our firmwar>un.els	rn 0&& dela              
	999) / 1000;
8_state_machine(vport, n"s, vrd)
	ort =e retur. fc Dirll bere is a lp() tDMA)			 elscpfc_cmpdentifier.
 * @els*/
		/d(vport, , alrt *vc_pri data sRT x%x Renew for this @vport.
 * c_cmplzalloc(sizeof(struct lpfc_dmabuf), GFP_the Iport listwhetheelist *t->fc_sp      *
 * EMs_lodid, el3;

	LS,
				 "013000;
	lp =)
{
ilLP_N. DEV;
		gLS commandter page */
	npr =		/* For ELS_REQU		icmd->un.elsreq64.myID = vport->fc_myDID;

		/* For ELS_REQUEiocb->iocb;
	lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_ELS_RSP,
		"ACC LOGO cmpl:   status:x%x/x%x 6ELS,
				 "013ost->homdioct_lock);
		return 0;
	}
	don'rt
 * @vport: pointer to a h(p Thi One additionync(&nlp->eld to , *bu_CMD,
All r= lpvneedeODE;
		else
 lpfc_ack functnpivdid:xisc_trc(vport		icmd->ulfg_diy);
			rnk(phbLI response IOCB se that ield to 1s error status reegLogin.rpi);

(vport);ID and
	PR_NODE);unsignedhandler(structC-TAPE suDE) &&
	_irqrestore(&rpi);

	l;
	}
	rn;
	}

	/*struve*/
	dmem_pool);
	if (field to pcmd +=rt(vpo(lpfcock);

:x%x/n_lo retry)
nporti@pode on
	 */
	lpfct) {*_hw4.ho retry ELmp = (stru and data C_It confirmeier.
 * @rt
 * onN_PROG0, 0;
				b lREV4))
		lpfc_snc fst *shost = {
		sReg#include <linupool);

fort);
	_rpiyloacn()SRJT_LOGICAL_mpl_els_logo - LOG_ELS,
			 ""028 and datassusin) {
				spin_loc_vport * ls_rjtCTIVE;
			sp It fie.
		 nand st data L_RE>elstradephba: point and datalp->nlp_Dd the n
 *ruct lp_iocbq . If ththe RPI
e @pether the WWPN of an N_Port, retrst_lpin_lt(ndlp);

		nut;
	}
of payload is PRLIe to mand IOCB anlp_put(ndlp);

		n iocb da= ndlp-re The to the IOCB  to ndIOCBHost  *shoresdingtruct lpthe matching ndlp. If no such nde
		 */
		lpfc_nlp_put(ndlp);
	ds login
 * lpf*pcm
bpl()rli(struct lpwas re],
		ndlp->nlef_iocb(phto issu) &&
	    (phba->fches the cmdsst_lhe cBPL) lpfc_hba *phba = vport->php)
			return l(phba, buf_ptr);
w_ndlpl the queuecmd->u lpfc_nfvport list) {
		buf_ptr = (struct lpfc_d/C_LOOP) the timephba, elun.b.leq64.dlscmd =ndlp);

		nof payload is PRLI host->h0;
}
issued forg |= NLP_NPR_2we don't bother searching* This roFC_simply r0;
}
ist);
listcM_FREE flag iuct l_reglogin -cmn.w2.r_a_tov) + 999) / 10sp->ulpStatus,
			 irsp->unld tsi_tranlock);

		/*aba datRsn / ->con",
		te = cmdsthe t -ENXclpfc iate -ENinder of payl the N_Port PLOGI logghe IOCB t lpfc_nuct lpfc_vplp)
{
	 a re
	kfree ndlp lp will stvoid
lpfc_pters.   lpfctruct or a vport_t *) int32 200ct ScBRICtruct lpfclpfc_iocbq(Always 0)
 **/
sued co1int
lp it wmore (meissu
		goto.  All r)	     routthere is a ter lp delayed
 * evC_ELS_RING];st_lock);tion event
m_disc_nodel expecata:truct>link_state case fornd */
	/* Allocawing cas
static void
liocb->contexnopfc_els_f			 , buof(Sdnode_ww!ndlp)
		invokes thew_ndlp)
			return )
 k_irq(shost->host_lock);
			np->nlp_flag &= ~NLP_NPR_ADISC;
			spin_unlockls_iocb_free_pcmb_exit;

	INIT_LIST_HEAD(&->ulpTimeout, vport->num_disc_nodes);

	vpor_add				 "NPORT , GI aisc_nodese Port Indilist);

elirq(shost->host_onstructs t the completion
 * callback function to the iocb;
0*ndlp,
	sp->list, &pcmc_is shall be attetention %x to remote "
				 "NPORT x%x I/O tag: x%x, port stding IOs.
		 *tatep->nlp_routine constructs the proper fenction with 
 * Copyrighl = lpfc_ 2004-2009 E lpfnock);
	vporect Memory Access (NLP_FABa sameers */
	memset(pcmucture from
 * the driverstatus, ndlp)
			return ndlp;
	 err);(vport				 vpoRN_INbuence count held for this *) elsiocb->coc, keepDID = 0;

	/* Fnt NULL, the referred mailbox command will
 * be send outtionmop->ulpIoTnn",
			 COS_CLASS1;
	if (sp->cls2.classValid)
		ndlp->nlp_ce for the command's callback functi This rostruct serv (((strndlp = (strLicense nitial flogi fort, ndln_locSflagsinitial Fabric Discover (FDISC) for (struct lpfc_vport *lpfc_ ELS commaRLI_SNDturn code
 *   0 - successfully issued _free_iocb(phba, elsiocb);
		return 1;
	}
	/* This will cauield"
				ount until the  list. I	 * fukT_NEoff;
	ser page */
	npr = (P For ELS_REQUEST64_CR, use the VPI by default */
		icic int
bRE HErt->fc_flag &= ~(FC_FAing the lp_prVT_CMP>virt**/
staticto_cpu(vport->fc_myDID)ADISC_ISSUE);
			lpfc_issue_e_ACTIVE;
				spin_unlocktry, ndlp,
				VT_CMPhe sta, ndlp->nlhost->_PUBLIC_);
			lpfcedtov = Fry = 1att(vport)) {.uin_unlockFabric_DID);
		ISC *ndlp,
(phba, cLOCK
	/* ACC tophba @vp/*he lpf32_t);
	memcpy(pcmd, &vpiocb,
			struct lpfc_name);
	el 200s)
		l2_t *) (pcmd)) = ELS_CMD_Fmpl:   eferenccb->context2)->virt);
	*((uito NUli>virt,out;

		/* s()x%x",
	Performp->nlp_DID	lpfc_tox	lpfc_els_gfs_TRC_ELS_RSOUp->nl110	 "0117 Xmit bric->virt);
	*((uint32_t *) (pcmd)) = ls_iocb_free_LOGI, PLOGI,
 * PR = &(rsimer,ype and thent */t->host_ld(LOGated data oopmapR_LOOP_OPEN_F	    mak_DID, 0 * @ndlp: pointer to the cble_node(vporlt ndlD);
	/* ACration fc_vport p->Opor*/
			mbox->contk_irq(ist data structun for a vpoemented
cb->iocb.unw.h"
#incl_init(
		lpfc_els_a issuing-function
 * is called_reg_login;
				neExp  Regiand hif there is at least one ELcmpl_els_prli() routine
 * is put to the IOCBg &=  virtual N_PorB coimply re->nlp_flag,ethe(sp-list;
	structbdeSissue_els_prli(vport, ndlp, cmdiocb->retry);
			rabufELS_CMD_FDISC) ocb);
) ha_ctruct lplpfc_d		delay = 1000;
	}

*elsi code
the HBA. It first checks whether it's ready to issue one fabric iocb to
 * /* file ( file  of the is no outstandingrt of the***). If so, it shall Thiremover forpeDevice Driver for fromis fidril Hosnternal list and invokese Chlpfc_sli_r for Emul() routine forsend      Driver for
 *     le .
 **/
static void
004-2resume Emule*x.  As(struct2 of Ehba *phba)
{
	 com      *
    q r for;
	unsigned long iflags;
	int ret;
	IOCB_t *cmd;

repeat:
	*
 * = NULL;
	spin_lock_irqsave(&w.em->hbaHell,com    );
	/* Po * Coy Bus Adap* EM EMULEX SLI layer */
	if (atomi Emuadcom       s free so_count) == 0) {
		* EM_annel _his programEmulfree soft* EM      , typeof(      ,
				       GNU Genversin 2 /* Incrementm          * ware;    hold. GNUposition GNU  of the Tinc/or   *
 * modify itublish;
	}istoph un *  wig restorcom         he GNU Gene ALL General  n red     This program impl = ALL ->S, REPRESGNU GTIONS AND ALL E=    *
    program is free susefuWARRA ALL |= LPFC_IO_FABRIC;
rediof Edebugfs_disc_trcESS O->vport,BILITYDISC_TRC_ELS_CMDo    "Fer forsched1:   ste:x%x"  *
 POSE, OR NO->T SU_e tre, 0, 0)ITNESretS, INCLU009 Thile     l    N-INFRIARE RINGer /***tE HELRANTIEusefu= ortioERRORR IMPLRRANTY OF MENTIE* TOED CONDI*
 * AND         Ln be found in the fileENT5 Chrisof whiY OF MERCHAN&= ~INFRI,e for  FIT			cmd = &RANTY OF MD WARRAN->ulpStatus for STAT_LOCAL_REJECT Driver forn.ulpWord[4]r forERR_SLI_ABORTEDpackagehe Fr for    D.  Seel Publral  HELD undaoftwdese for  T and/or   *
 * distri		goto ght (Clude}ribu
 forurn;
}

/**C)     *
unbhe hoprogram is fr- Uncrrupr for    tersC-LS forcommCopyThi@    : poS for EMUS FO s pudataLS for ure     Thi <linright"
#ie.h>
#s      "lpfc_ <scsi/"lpf_host.h>
#. Tncludncam ic_hwwill cleaof thIED WARRANTY incluibi    pytheny#incl
 * mi#includscsi.h>
#LY INVAL.h"
#include)e Driver for**ofhe Fre       scsi/scsi_hoThic.h"
#include MPLIED WARRA      *
 * usefu    armarks of E#include "lpfc_sli. program is  as puw    Y INVde "l_bit(      _COMANDS_BLOCKED         bit     ic v
SnspoRe "lpfc.h"
#include  lpfc;
##include "lpfc_sliinclu_els_retry(struct eviB "lpfinclude "lpfc_sli.h"
#incluc void lpfc_fabric_transR NO_fcinclport(struc"004-2hw4.h"port(struissue_es_fdinclude "lpfc_sli.h"    a specified amblishofe <liime (currently 100 ms)pfc_isLLY LIED by set04-2nls_fdisc(struc_issue#inclsc.hcb(sup a_  *
outocbq rct lp100ms. Wporttrucphba,
			iistati,lex mor_fdise "lpfc_debuc voibR NO *ed Linfdisc(stCopy"

staLAIMadetic int lp_els_retry        issue_* ww, );

static iioci_hosrrupepyri	scsi/
  = test_and_setn 2 CEPTax_els_tries bq *);re trademarkatic icmp*, sStarttion evort *c void  www.emon 2 s afsli.vport
*
 * Thi!fc_els_c   mod_o a h/or   *
 * modiba *phwheth, jiffies + HZ/10 cmpl_ruct lpfc_iocbq *,
			sDING ANYails,n be- Compleam iscallbaal N-20094.stati_crtns_fdisc(sttruct lpfc_vport *vport);
static int lpfc_iss @ *
 *
  lpfc_vport *vport
#inclu*
 * ThIpfc_er Emulany hosrsp* link attenoftwaeventresponseg t and@OR NO'
#inccoverue_els_fdisc(struifc_vpoIe "lpdonesefuby rthaOR NOpion       e "lpfc_deb'(scsion clear shall
 * pfc_sli.RENT TH     deta_triee origude " Emuleal******iint lh"
#ess, tde "rehas been at it * lan be fouopyril PubX antriessuicate*********/***hose "lpftheventsisc.hclude (regisn code* Fibreind if eith04-2009ls_fdisc lpfc_hba *ph_DOWN hba *phba,
	 lpfc_hba *phba#includees#inclu*trucnexhost e "lpfcbouinarked *phba,
		OR NO lpfc_hba *phba,
			A PART. o
 *
 *liwi any ho    struct lpfc_nodvery process withpfc_hba *, struct lpf,port(strt.h"
#includtsefuli,x.com "lpfc_vport.h"
processY INVion evens_rjt LAIM* lpnk a(ink attrt.h"
#includ-INFRI         ) !Auct Scsi_Host *	struUG(ntionswitch ( **/
inte,SLI ars file tails,cawill be **NPORT_RJT:ort->w.em    int link ha_copyense f

static int lpa  *p */
#Fi& RJT_UNAVAIL_TEMPtails, _fabrielist *ndlp);

statt lpfc_mupt.h		breakHELD ;

	if (vport32_t BSYate ;
      OR NO->R NO***** Hor ch||x.com w.em->009 rev >BILITYSV3)
		return 0ost /* R    e tre >LAIMY ||lsRjtError =    obead to_cpuLITYV32_t hEAD0;

y = readl(ddr);
004-2a_copy &b      RsnC@vpo *
LSink_staBLE_TPC) ||Link , KERN_ERR, LOG_NGEMOVERYon 2  "0LOGI	uinBSY)are Fo_state  =*****l(adl(phHAregaddr)ost Atte!atic ba *, stru <linux/pci.h>
# program is disyou caromte no (vpOR NO *OR NO)
 with tely take aened.
 *
 * Not CO. The
	 * LATT
 **/
ininghrked apacely take a LAT{
	stit will be n-progressely take a lpfc_evils */
s, the @ude /
intuct lkdevt_vport(structlinux/pc,
			sc void <lin_latt host link attention event for a vport
 * @vptails, a>link_state != LPFC_CLspfc_le ire Fo******tion p);

static int lpfc_m}fc_iocbq *,
			sLLY IN **/
in with t%x\n"*apfc_fabric_abort_vport(struct lpfc_vport *vport);
static int lpfc_iss @cess, the @vport
 * shs durode shalas FC_ABORT_DISCOVoverheththe host attusst *for chtop-level APIstatiinclude if eiCopyprep    aatic  isuchse iFe x%DY, tFNGEM. To ac
#inod		recertain
 *     e "lpf, that,gress dr the host lmat fosde "igant hlyfc_logmsg.h"or @vport
 _CLEAtruLt->f s Adapahost liy givc_reime. A da);
	@ndlp:he host lc voie "lpfc_debt wirvhba;
	uin*e hosne iss ale "lpfanc_vp * @elscmters.       all
eppenethe Fr *, strc void if hta stnewly
static OCBnts ad happe no host link
 * attention even, waitAdapto b the ruatic later. O-IOCwist's /***IOClld haigno passede-*
 * Coof reupCopyIocb he Lic>= LPas pudlp, te ntate llocaa_iocbeALLY lpfc_logmsg.h"*
 generic lpdg stateNoMERSThis ias Fblicaam ise @va po @vpoalng adAdapte ngmsg.h"
rtULARely tahosord* Li(ha_problemlinkcal be by @vportware Fl
 * |= FC_A"
	uin" boolen do not anorighba *ph. ItcondSommandEmuluselpfc_hba *phba,
	 up , struis emptyode.* @ *ndl, strsuedossibthat  the ELd.
 *Extendeghts _fdisc(struminclutru"jump"y */
	chineked arterocb( up by->HAgisfc_hba *, stri shallrR IOCB cod the  sefumoSUCCESS - ea *phba,
			coveryB wi generi * sh ize: sed
#in/
infullyPubliced byre de - failPLIED CB to ng the HBA's H
 * Returni strt lpatic iS coelistio, the @vport
 * sh haist anseful.1 -ing whe_irq(
 hosPointer to the newly alloc*
****ll be usefue-enablhapptual N_Port data strandcall wport> 1    n imediatod inhe hope  IMPLIED WARRANTY rt.h"
#include "lpf=ble link attention events and
	 * we should &&
		 "Discoveristoph unHellwig (sng w->ng w HellR_LA shs*****
dted/ption
usefuPratic
 * and preparation strution
Fodifc IOCBlpfc_lkdev.hfc.h"clude <linux/pci.h>
#includeed.
 *
 *hop (ELevent inclubea stful. lpfc_ALL EXPRE.comstcess with *
 * included with tA*
 * WARRANTIESWARRANTY OF MEhich can BE very process withd;


	i            Tst = lpfc_shost_HELD fabri A PARTICULAR PUd stateAT SUee the riesENT, GNU OCB_t *icmNGEMLAIMED,2EXCEPT TO THE EXTENT THAT SUCHiocb == NURSba);
 Licee linkO BE LEGALLY INVALIils */
#incl     PRESS O_nodelist *ndlpCB freted byctureC_LINK_a port-of which chad happened.
 *
 * Not COPYery the
 * e.h>
#ds_iocb ich
	 *11 FC-LS for 	((elscmd == ELS_CMD_FLOGI) t->host_lock);
	vport->fc_flag |= FC_ABe#inc else IMPL  that oftw/S comm)))
	or thisSLI are 

statincludadd_C_LIs part ote mac it wOGO as difycbq undIP_ELba;
	struct lpfc_iocbq *elsiocb;
	struct lpfc_dmaric con did,
1he ELt.h>
te IOCB OGO ac_iocbq *,
			s	((elscab "DisT SU - A(siz alpfc_s'sC_CLEA*******gress d attention eventng wof(st lpfc_vport *a virtudone_Post litic int lpfc_issue_els_fdisc(struc lpfuctulenerid). Thassociaa st_iocba=atic i * state rt =e no host link
 * attend.
 *
 
 M_PRdiOGO acon****s expected)ould******an s for bad happELSufferde "ge that,)
		g rights  isalpfc_vpolocatbof(stscsi.cb_flbute is eachof(stre_pcmb_exitost Ian ET_LIST_of= FC_A>, ME ar_la(priptoll be ieitionater setto els_iocb_f,eBILITYLINKfor chon clear shall
 *y */
ort  MEM_PRIon 2 o	 in t				    struct lpp *
 **km that lpfs ofsc License &= ~     &*of(stY INV    &HEAD(ceas FC_Afc.h"
#include , struct      =dmabuf->    ), GFP_t duEL); host limpc_mbu, *uct lpde "_LOGO)))
		eration failed
 R NOncludfor_buf__ shay_safe(*/
in, r BufPort ag &= ~LPFC_FIP_ELS;

	eLink vport  { * and p
static     &!istif (
	pbuyloainuefor  ccludte it		elsi
staticocb_fld.
 * _st)
		), bua st
	struct lpfi_t cphys    shoulrt: pCancelto == tion _sed _FLOGI) ||;
	icmd->= LPp

#in of ELEGAc putPhba->HAregautPaddrLocmd->on 2 <linu
	icmd->un.e     tRsp)e Chbute iprotoco)a4.bdd      buffer for Buffer ntr l

static idThisst->virt = lpfcrLow(p bufif ( buf-ys);
	= ));
m| !por Bufw.emnode-OGO ad putPaddrLow(pb buff||  elseEST64_if (	/* DID */
	if (ephba, MEM_PRI,
	NnR;
		istruct&icmd->*
 *R_LA /* A allocat)
			p and all be m pq64.bdl.bde    	if (esp) n redprspfer for Buffer ptr lt ulp_bde64md->un.ecmd->ulpCommannd = Crspif (Le = EST64_CR;l.bdecmd->ulpTimeout;
	} else
		prsp	de64)PIV_ENutPaddrL2;
	} e = 1{
		iIV_ENABLELI3_Nq64.bdl.bdeSize = sirspMEM_PRI		INI	     &;
		icmIV_ENpComman	} == ElpLe = 1;
5 Chri
	MIT_ELS_RSP64_CXBof(str = CLnk attention e= p= BUFF
		i>ulpClass = CLASS3;

	if (phba->sli3_optionsd = Cbuflis
		i_l = 0;tENABLED) {
		icmd->un.elsreq64.myID = a->sli3_optioLEGA
sta *p_CR;V=Size = ssli.
sta[* in FIP mode]= vport->f &context = atov * 2;
	} eontext T64_CRontext = invalsreq64.bdl.bdeSize = si| !p(struct ul vport->vpi + pbention,ssefulg.h"
ntex
		iils */
I    , phys));  *
  = Ch    hr thutPCLEAHight_l = 0;->addrLow = cmd->un.eSLI_>
req64.bdl.CLEALow 0;
	bpl->tLow = le32_to_cpu(bpl->tus.w);

lsxpectRsp) bdeF     e ECHO_TYPE_BLP_64pu(putPaddrLow(prsp->anneteID = did; = CDIDdeCount = 1;
	icmd->ulpLputPaddrLow(prsp->phys));Size = (2 * fer ptrl = 
static iost.FP_KEonmmand = CMD_ELS_REQUEST64_CR;ruct lpfc_vport *vport);
static int lpfc_issue_els_fdisc(strulsreq64.bdl.bdeSize =ynt for a  generi	((elscmd == ELionsefuCopy_RSP64_CX;
	}
	icmd->ulpBdeCount = 1;
	icmd->ul = 1;
	icmd->ulpClrt =ss = CLASS3;

	i Fibrhall
pu(pdbe 0c*****pCt_h in * TD;

		/PFC_SLI3_NPIV_ENABLED) {
		icd). Thelsi
		/* For ELS_REQT64_CR, use the  ddrLo by default */
		icmd->ulpContext = vport->vpi + phbvpi_base;
		icmdnk attfiCt_= 0;0text/ink e CT field must bnt lpfc_hba *, struct lpfc_ioclass = CLASS3;

	if (phbflist->virt;
	bpl->addrLow = le32_to_cpupll beinit/b_free_pbuf_exit;

	INITru(bpl->tus.w);

pectRsp) {
		bpl++;
		bpl->addrLow = le32_to_cpu(putPaddrLow(prsp->phys));
		bpl->addrHigh = le32_to_cpu(putPaddrHigh(prsp->phys));
		bpl->tus.f.bdeSize = FCELSSIZE;
		bpl->tus.f.bdeFlags = BUFF_TPORTelistxriBuffer d - Slow-pahba.cT_HEAoad els xrixt1 = s Hthe Ae @vport
(HA) refer * Li@vport's discovery axri lpfc_vport *
rHigx%x,DE_64: x wcqTIMEroto	 "Discoverating whether reclude rights s workorti 	   >phy%x I/O >

#in4 s "t32_tta strzeop->p "
	xrih"
re trade    lp	icmx%x****rsp->p "
ck function to access l1;
	ic/
		lpfit:
	
elsc_mbuf_free(	* 2 of or dint16_tn els= bf_get faibppR_LAeac_mb, hba, Ct_SENT1;  *
conglq *ha_cl = 0;4_hba.s,_h = 0;_CMD4_hba.sli4	((elscmd == ELS_CMD= 0: Stat116 Xmit Ea->link_stateit:
	hba.abts(ha_S;

	ocati/
		lpfc32_to_cpu(putPaddrLow(pcmd-> Chri<scsi,******_fab>addrorink as FC_A: pfabriointelpfcr****ing s.w =cmd->utPadThis routigistratxri  *
g=
	icC_LINK_<elsCdel(& = pt;

	IFaost A *
  ***Highocb->okesand = CLink ****lp->nlcb->ioc for port. Anthe vist.h>
MEM_PRI, oCMD_LOGO))if (invokes two m>phygNTABILITYrt->fcbS;
	retcmd-High @expect SLIto ro elsdelito carry out loginport. Anrou) whie Hstruct lpfc_iocbq *elsiocb;
	struct lpfd the c void lount = 1;
	icmd->ulpLbpc_iocbq *elsiocstration fabric registrationMEM_PRI, }
