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

#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/interrupt.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
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
#include "lpfc_version.h"
#include "lpfc_compat.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"

#define LPFC_DEF_DEVLOSS_TMO 30
#define LPFC_MIN_DEVLOSS_TMO 1
#define LPFC_MAX_DEVLOSS_TMO 255

#define LPFC_MAX_LINK_SPEED 8
#define LPFC_LINK_SPEED_BITMAP 0x00000117
#define LPFC_LINK_SPEED_STRING "0, 1, 2, 4, 8"

/**
 * lpfc_jedec_to_ascii - Hex to ascii convertor according to JEDEC rules
 * @incr: integer to convert.
 * @hdw: ascii string holding converted integer plus a string terminator.
 *
 * Description:
 * JEDEC Joint Electron Device Engineering Council.
 * Convert a 32 bit integer composed of 8 nibbles into an 8 byte ascii
 * character string. The string is then terminated with a NULL in byte 9.
 * Hex 0-9 becomes ascii '0' to '9'.
 * Hex a-f becomes ascii '=' to 'B' capital B.
 *
 * Notes:
 * Coded for 32 bit integers only.
 **/
static void
lpfc_jedec_to_ascii(int incr, char hdw[])
{
	int i, j;
	for (i = 0; i < 8; i++) {
		j = (incr & 0xf);
		if (j <= 9)
			hdw[7 - i] = 0x30 +  j;
		 else
			hdw[7 - i] = 0x61 + j - 10;
		incr = (incr >> 4);
	}
	hdw[8] = 0;
	return;
}

/**
 * lpfc_drvr_version_show - Return the Emulex driver string with version number
 * @dev: class unused variable.
 * @attr: device attribute, not used.
 * @buf: on return contains the module description text.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_drvr_version_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	return snprintf(buf, PAGE_SIZE, LPFC_MODULE_DESC "\n");
}

static ssize_t
lpfc_bg_info_show(struct device *dev, struct device_attribute *attr,
		  char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	if (phba->cfg_enable_bg)
		if (phba->sli3_options & LPFC_SLI3_BG_ENABLED)
			return snprintf(buf, PAGE_SIZE, "BlockGuard Enabled\n");
		else
			return snprintf(buf, PAGE_SIZE,
					"BlockGuard Not Supported\n");
	else
			return snprintf(buf, PAGE_SIZE,
					"BlockGuard Disabled\n");
}

static ssize_t
lpfc_bg_guard_err_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%llu\n",
			(unsigned long long)phba->bg_guard_err_cnt);
}

static ssize_t
lpfc_bg_apptag_err_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%llu\n",
			(unsigned long long)phba->bg_apptag_err_cnt);
}

static ssize_t
lpfc_bg_reftag_err_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%llu\n",
			(unsigned long long)phba->bg_reftag_err_cnt);
}

/**
 * lpfc_info_show - Return some pci info about the host in ascii
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains the formatted text from lpfc_info().
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_info_show(struct device *dev, struct device_attribute *attr,
	       char *buf)
{
	struct Scsi_Host *host = class_to_shost(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",lpfc_info(host));
}

/**
 * lpfc_serialnum_show - Return the hba serial number in ascii
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains the formatted text serial number.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_serialnum_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%s\n",phba->SerialNumber);
}

/**
 * lpfc_temp_sensor_show - Return the temperature sensor level
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains the formatted support level.
 *
 * Description:
 * Returns a number indicating the temperature sensor level currently
 * supported, zero or one in ascii.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_temp_sensor_show(struct device *dev, struct device_attribute *attr,
		      char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	return snprintf(buf, PAGE_SIZE, "%d\n",phba->temp_sensor_support);
}

/**
 * lpfc_modeldesc_show - Return the model description of the hba
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains the scsi vpd model description.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_modeldesc_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%s\n",phba->ModelDesc);
}

/**
 * lpfc_modelname_show - Return the model name of the hba
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains the scsi vpd model name.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_modelname_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%s\n",phba->ModelName);
}

/**
 * lpfc_programtype_show - Return the program type of the hba
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains the scsi vpd program type.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_programtype_show(struct device *dev, struct device_attribute *attr,
		      char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%s\n",phba->ProgramType);
}

/**
 * lpfc_mlomgmt_show - Return the Menlo Maintenance sli flag
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains the Menlo Maintenance sli flag.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_mlomgmt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *)shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%d\n",
		(phba->sli.sli_flag & LPFC_MENLO_MAINT));
}

/**
 * lpfc_vportnum_show - Return the port number in ascii of the hba
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains scsi vpd program type.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_vportnum_show(struct device *dev, struct device_attribute *attr,
		   char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%s\n",phba->Port);
}

/**
 * lpfc_fwrev_show - Return the firmware rev running in the hba
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains the scsi vpd program type.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_fwrev_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	char fwrev[32];

	lpfc_decode_firmware_rev(phba, fwrev, 1);
	return snprintf(buf, PAGE_SIZE, "%s, sli-%d\n", fwrev, phba->sli_rev);
}

/**
 * lpfc_hdw_show - Return the jedec information about the hba
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains the scsi vpd program type.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_hdw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char hdw[9];
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	lpfc_vpd_t *vp = &phba->vpd;

	lpfc_jedec_to_ascii(vp->rev.biuRev, hdw);
	return snprintf(buf, PAGE_SIZE, "%s\n", hdw);
}

/**
 * lpfc_option_rom_version_show - Return the adapter ROM FCode version
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains the ROM and FCode ascii strings.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_option_rom_version_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%s\n", phba->OptionROMVersion);
}

/**
 * lpfc_state_show - Return the link state of the port
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains text describing the state of the link.
 *
 * Notes:
 * The switch statement has no default so zero will be returned.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_link_state_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	int  len = 0;

	switch (phba->link_state) {
	case LPFC_LINK_UNKNOWN:
	case LPFC_WARM_START:
	case LPFC_INIT_START:
	case LPFC_INIT_MBX_CMDS:
	case LPFC_LINK_DOWN:
	case LPFC_HBA_ERROR:
		if (phba->hba_flag & LINK_DISABLED)
			len += snprintf(buf + len, PAGE_SIZE-len,
				"Link Down - User disabled\n");
		else
			len += snprintf(buf + len, PAGE_SIZE-len,
				"Link Down\n");
		break;
	case LPFC_LINK_UP:
	case LPFC_CLEAR_LA:
	case LPFC_HBA_READY:
		len += snprintf(buf + len, PAGE_SIZE-len, "Link Up - ");

		switch (vport->port_state) {
		case LPFC_LOCAL_CFG_LINK:
			len += snprintf(buf + len, PAGE_SIZE-len,
					"Configuring Link\n");
			break;
		case LPFC_FDISC:
		case LPFC_FLOGI:
		case LPFC_FABRIC_CFG_LINK:
		case LPFC_NS_REG:
		case LPFC_NS_QRY:
		case LPFC_BUILD_DISC_LIST:
		case LPFC_DISC_AUTH:
			len += snprintf(buf + len, PAGE_SIZE - len,
					"Discovery\n");
			break;
		case LPFC_VPORT_READY:
			len += snprintf(buf + len, PAGE_SIZE - len, "Ready\n");
			break;

		case LPFC_VPORT_FAILED:
			len += snprintf(buf + len, PAGE_SIZE - len, "Failed\n");
			break;

		case LPFC_VPORT_UNKNOWN:
			len += snprintf(buf + len, PAGE_SIZE - len,
					"Unknown\n");
			break;
		}
		if (phba->sli.sli_flag & LPFC_MENLO_MAINT)
			len += snprintf(buf + len, PAGE_SIZE-len,
					"   Menlo Maint Mode\n");
		else if (phba->fc_topology == TOPOLOGY_LOOP) {
			if (vport->fc_flag & FC_PUBLIC_LOOP)
				len += snprintf(buf + len, PAGE_SIZE-len,
						"   Public Loop\n");
			else
				len += snprintf(buf + len, PAGE_SIZE-len,
						"   Private Loop\n");
		} else {
			if (vport->fc_flag & FC_FABRIC)
				len += snprintf(buf + len, PAGE_SIZE-len,
						"   Fabric\n");
			else
				len += snprintf(buf + len, PAGE_SIZE-len,
						"   Point-2-Point\n");
		}
	}

	return len;
}

/**
 * lpfc_num_discovered_ports_show - Return sum of mapped and unmapped vports
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the sum of fc mapped and unmapped.
 *
 * Description:
 * Returns the ascii text number of the sum of the fc mapped and unmapped
 * vport counts.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_num_discovered_ports_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			vport->fc_map_cnt + vport->fc_unmap_cnt);
}

/**
 * lpfc_issue_lip - Misnomer, name carried over from long ago
 * @shost: Scsi_Host pointer.
 *
 * Description:
 * Bring the link down gracefully then re-init the link. The firmware will
 * re-init the fiber channel interface as required. Does not issue a LIP.
 *
 * Returns:
 * -EPERM port offline or management commands are being blocked
 * -ENOMEM cannot allocate memory for the mailbox command
 * -EIO error sending the mailbox command
 * zero for success
 **/
static int
lpfc_issue_lip(struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	LPFC_MBOXQ_t *pmboxq;
	int mbxstatus = MBXERR_ERROR;

	if ((vport->fc_flag & FC_OFFLINE_MODE) ||
	    (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO))
		return -EPERM;

	pmboxq = mempool_alloc(phba->mbox_mem_pool,GFP_KERNEL);

	if (!pmboxq)
		return -ENOMEM;

	memset((void *)pmboxq, 0, sizeof (LPFC_MBOXQ_t));
	pmboxq->u.mb.mbxCommand = MBX_DOWN_LINK;
	pmboxq->u.mb.mbxOwner = OWN_HOST;

	mbxstatus = lpfc_sli_issue_mbox_wait(phba, pmboxq, LPFC_MBOX_TMO * 2);

	if ((mbxstatus == MBX_SUCCESS) &&
	    (pmboxq->u.mb.mbxStatus == 0 ||
	     pmboxq->u.mb.mbxStatus == MBXERR_LINK_DOWN)) {
		memset((void *)pmboxq, 0, sizeof (LPFC_MBOXQ_t));
		lpfc_init_link(phba, pmboxq, phba->cfg_topology,
			       phba->cfg_link_speed);
		mbxstatus = lpfc_sli_issue_mbox_wait(phba, pmboxq,
						     phba->fc_ratov * 2);
	}

	lpfc_set_loopback_flag(phba);
	if (mbxstatus != MBX_TIMEOUT)
		mempool_free(pmboxq, phba->mbox_mem_pool);

	if (mbxstatus == MBXERR_ERROR)
		return -EIO;

	return 0;
}

/**
 * lpfc_do_offline - Issues a mailbox command to bring the link down
 * @phba: lpfc_hba pointer.
 * @type: LPFC_EVT_OFFLINE, LPFC_EVT_WARM_START, LPFC_EVT_KILL.
 *
 * Notes:
 * Assumes any error from lpfc_do_offline() will be negative.
 * Can wait up to 5 seconds for the port ring buffers count
 * to reach zero, prints a warning if it is not zero and continues.
 * lpfc_workq_post_event() returns a non-zero return code if call fails.
 *
 * Returns:
 * -EIO error posting the event
 * zero for success
 **/
static int
lpfc_do_offline(struct lpfc_hba *phba, uint32_t type)
{
	struct completion online_compl;
	struct lpfc_sli_ring *pring;
	struct lpfc_sli *psli;
	int status = 0;
	int cnt = 0;
	int i;

	init_completion(&online_compl);
	lpfc_workq_post_event(phba, &status, &online_compl,
			      LPFC_EVT_OFFLINE_PREP);
	wait_for_completion(&online_compl);

	if (status != 0)
		return -EIO;

	psli = &phba->sli;

	/* Wait a little for things to settle down, but not
	 * long enough for dev loss timeout to expire.
	 */
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		while (pring->txcmplq_cnt) {
			msleep(10);
			if (cnt++ > 500) {  /* 5 secs */
				lpfc_printf_log(phba,
					KERN_WARNING, LOG_INIT,
					"0466 Outstanding IO when "
					"bringing Adapter offline\n");
				break;
			}
		}
	}

	init_completion(&online_compl);
	lpfc_workq_post_event(phba, &status, &online_compl, type);
	wait_for_completion(&online_compl);

	if (status != 0)
		return -EIO;

	return 0;
}

/**
 * lpfc_selective_reset - Offline then onlines the port
 * @phba: lpfc_hba pointer.
 *
 * Description:
 * If the port is configured to allow a reset then the hba is brought
 * offline then online.
 *
 * Notes:
 * Assumes any error from lpfc_do_offline() will be negative.
 *
 * Returns:
 * lpfc_do_offline() return code if not zero
 * -EIO reset not configured or error posting the event
 * zero for success
 **/
static int
lpfc_selective_reset(struct lpfc_hba *phba)
{
	struct completion online_compl;
	int status = 0;

	if (!phba->cfg_enable_hba_reset)
		return -EIO;

	status = lpfc_do_offline(phba, LPFC_EVT_OFFLINE);

	if (status != 0)
		return status;

	init_completion(&online_compl);
	lpfc_workq_post_event(phba, &status, &online_compl,
			      LPFC_EVT_ONLINE);
	wait_for_completion(&online_compl);

	if (status != 0)
		return -EIO;

	return 0;
}

/**
 * lpfc_issue_reset - Selectively resets an adapter
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: containing the string "selective".
 * @count: unused variable.
 *
 * Description:
 * If the buf contains the string "selective" then lpfc_selective_reset()
 * is called to perform the reset.
 *
 * Notes:
 * Assumes any error from lpfc_selective_reset() will be negative.
 * If lpfc_selective_reset() returns zero then the length of the buffer
 * is returned which indicates succcess
 *
 * Returns:
 * -EINVAL if the buffer does not contain the string "selective"
 * length of buf if lpfc-selective_reset() if the call succeeds
 * return value of lpfc_selective_reset() if the call fails
**/
static ssize_t
lpfc_issue_reset(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	int status = -EINVAL;

	if (strncmp(buf, "selective", sizeof("selective") - 1) == 0)
		status = lpfc_selective_reset(phba);

	if (status == 0)
		return strlen(buf);
	else
		return status;
}

/**
 * lpfc_nport_evt_cnt_show - Return the number of nport events
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the ascii number of nport events.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_nport_evt_cnt_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%d\n", phba->nport_event_cnt);
}

/**
 * lpfc_board_mode_show - Return the state of the board
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the state of the adapter.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_board_mode_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	char  * state;

	if (phba->link_state == LPFC_HBA_ERROR)
		state = "error";
	else if (phba->link_state == LPFC_WARM_START)
		state = "warm start";
	else if (phba->link_state == LPFC_INIT_START)
		state = "offline";
	else
		state = "online";

	return snprintf(buf, PAGE_SIZE, "%s\n", state);
}

/**
 * lpfc_board_mode_store - Puts the hba in online, offline, warm or error state
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: containing one of the strings "online", "offline", "warm" or "error".
 * @count: unused variable.
 *
 * Returns:
 * -EACCES if enable hba reset not enabled
 * -EINVAL if the buffer does not contain a valid string (see above)
 * -EIO if lpfc_workq_post_event() or lpfc_do_offline() fails
 * buf length greater than zero indicates success
 **/
static ssize_t
lpfc_board_mode_store(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct completion online_compl;
	int status=0;

	if (!phba->cfg_enable_hba_reset)
		return -EACCES;
	init_completion(&online_compl);

	if(strncmp(buf, "online", sizeof("online") - 1) == 0) {
		lpfc_workq_post_event(phba, &status, &online_compl,
				      LPFC_EVT_ONLINE);
		wait_for_completion(&online_compl);
	} else if (strncmp(buf, "offline", sizeof("offline") - 1) == 0)
		status = lpfc_do_offline(phba, LPFC_EVT_OFFLINE);
	else if (strncmp(buf, "warm", sizeof("warm") - 1) == 0)
		status = lpfc_do_offline(phba, LPFC_EVT_WARM_START);
	else if (strncmp(buf, "error", sizeof("error") - 1) == 0)
		status = lpfc_do_offline(phba, LPFC_EVT_KILL);
	else
		return -EINVAL;

	if (!status)
		return strlen(buf);
	else
		return -EIO;
}

/**
 * lpfc_get_hba_info - Return various bits of informaton about the adapter
 * @phba: pointer to the adapter structure.
 * @mxri: max xri count.
 * @axri: available xri count.
 * @mrpi: max rpi count.
 * @arpi: available rpi count.
 * @mvpi: max vpi count.
 * @avpi: available vpi count.
 *
 * Description:
 * If an integer pointer for an count is not null then the value for the
 * count is returned.
 *
 * Returns:
 * zero on error
 * one for success
 **/
static int
lpfc_get_hba_info(struct lpfc_hba *phba,
		  uint32_t *mxri, uint32_t *axri,
		  uint32_t *mrpi, uint32_t *arpi,
		  uint32_t *mvpi, uint32_t *avpi)
{
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_mbx_read_config *rd_config;
	LPFC_MBOXQ_t *pmboxq;
	MAILBOX_t *pmb;
	int rc = 0;

	/*
	 * prevent udev from issuing mailbox commands until the port is
	 * configured.
	 */
	if (phba->link_state < LPFC_LINK_DOWN ||
	    !phba->mbox_mem_pool ||
	    (phba->sli.sli_flag & LPFC_SLI_ACTIVE) == 0)
		return 0;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return 0;

	pmboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmboxq)
		return 0;
	memset(pmboxq, 0, sizeof (LPFC_MBOXQ_t));

	pmb = &pmboxq->u.mb;
	pmb->mbxCommand = MBX_READ_CONFIG;
	pmb->mbxOwner = OWN_HOST;
	pmboxq->context1 = NULL;

	if ((phba->pport->fc_flag & FC_OFFLINE_MODE) ||
		(!(psli->sli_flag & LPFC_SLI_ACTIVE)))
		rc = MBX_NOT_FINISHED;
	else
		rc = lpfc_sli_issue_mbox_wait(phba, pmboxq, phba->fc_ratov * 2);

	if (rc != MBX_SUCCESS) {
		if (rc != MBX_TIMEOUT)
			mempool_free(pmboxq, phba->mbox_mem_pool);
		return 0;
	}

	if (phba->sli_rev == LPFC_SLI_REV4) {
		rd_config = &pmboxq->u.mqe.un.rd_config;
		if (mrpi)
			*mrpi = bf_get(lpfc_mbx_rd_conf_rpi_count, rd_config);
		if (arpi)
			*arpi = bf_get(lpfc_mbx_rd_conf_rpi_count, rd_config) -
					phba->sli4_hba.max_cfg_param.rpi_used;
		if (mxri)
			*mxri = bf_get(lpfc_mbx_rd_conf_xri_count, rd_config);
		if (axri)
			*axri = bf_get(lpfc_mbx_rd_conf_xri_count, rd_config) -
					phba->sli4_hba.max_cfg_param.xri_used;
		if (mvpi)
			*mvpi = bf_get(lpfc_mbx_rd_conf_vpi_count, rd_config);
		if (avpi)
			*avpi = bf_get(lpfc_mbx_rd_conf_vpi_count, rd_config) -
					phba->sli4_hba.max_cfg_param.vpi_used;
	} else {
		if (mrpi)
			*mrpi = pmb->un.varRdConfig.max_rpi;
		if (arpi)
			*arpi = pmb->un.varRdConfig.avail_rpi;
		if (mxri)
			*mxri = pmb->un.varRdConfig.max_xri;
		if (axri)
			*axri = pmb->un.varRdConfig.avail_xri;
		if (mvpi)
			*mvpi = pmb->un.varRdConfig.max_vpi;
		if (avpi)
			*avpi = pmb->un.varRdConfig.avail_vpi;
	}

	mempool_free(pmboxq, phba->mbox_mem_pool);
	return 1;
}

/**
 * lpfc_max_rpi_show - Return maximum rpi
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the maximum rpi count in decimal or "Unknown".
 *
 * Description:
 * Calls lpfc_get_hba_info() asking for just the mrpi count.
 * If lpfc_get_hba_info() returns zero (failure) the buffer text is set
 * to "Unknown" and the buffer length is returned, therefore the caller
 * must check for "Unknown" in the buffer to detect a failure.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_max_rpi_show(struct device *dev, struct device_attribute *attr,
		  char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	uint32_t cnt;

	if (lpfc_get_hba_info(phba, NULL, NULL, &cnt, NULL, NULL, NULL))
		return snprintf(buf, PAGE_SIZE, "%d\n", cnt);
	return snprintf(buf, PAGE_SIZE, "Unknown\n");
}

/**
 * lpfc_used_rpi_show - Return maximum rpi minus available rpi
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: containing the used rpi count in decimal or "Unknown".
 *
 * Description:
 * Calls lpfc_get_hba_info() asking for just the mrpi and arpi counts.
 * If lpfc_get_hba_info() returns zero (failure) the buffer text is set
 * to "Unknown" and the buffer length is returned, therefore the caller
 * must check for "Unknown" in the buffer to detect a failure.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_used_rpi_show(struct device *dev, struct device_attribute *attr,
		   char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	uint32_t cnt, acnt;

	if (lpfc_get_hba_info(phba, NULL, NULL, &cnt, &acnt, NULL, NULL))
		return snprintf(buf, PAGE_SIZE, "%d\n", (cnt - acnt));
	return snprintf(buf, PAGE_SIZE, "Unknown\n");
}

/**
 * lpfc_max_xri_show - Return maximum xri
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the maximum xri count in decimal or "Unknown".
 *
 * Description:
 * Calls lpfc_get_hba_info() asking for just the mrpi count.
 * If lpfc_get_hba_info() returns zero (failure) the buffer text is set
 * to "Unknown" and the buffer length is returned, therefore the caller
 * must check for "Unknown" in the buffer to detect a failure.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_max_xri_show(struct device *dev, struct device_attribute *attr,
		  char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	uint32_t cnt;

	if (lpfc_get_hba_info(phba, &cnt, NULL, NULL, NULL, NULL, NULL))
		return snprintf(buf, PAGE_SIZE, "%d\n", cnt);
	return snprintf(buf, PAGE_SIZE, "Unknown\n");
}

/**
 * lpfc_used_xri_show - Return maximum xpi minus the available xpi
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the used xri count in decimal or "Unknown".
 *
 * Description:
 * Calls lpfc_get_hba_info() asking for just the mxri and axri counts.
 * If lpfc_get_hba_info() returns zero (failure) the buffer text is set
 * to "Unknown" and the buffer length is returned, therefore the caller
 * must check for "Unknown" in the buffer to detect a failure.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_used_xri_show(struct device *dev, struct device_attribute *attr,
		   char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	uint32_t cnt, acnt;

	if (lpfc_get_hba_info(phba, &cnt, &acnt, NULL, NULL, NULL, NULL))
		return snprintf(buf, PAGE_SIZE, "%d\n", (cnt - acnt));
	return snprintf(buf, PAGE_SIZE, "Unknown\n");
}

/**
 * lpfc_max_vpi_show - Return maximum vpi
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the maximum vpi count in decimal or "Unknown".
 *
 * Description:
 * Calls lpfc_get_hba_info() asking for just the mvpi count.
 * If lpfc_get_hba_info() returns zero (failure) the buffer text is set
 * to "Unknown" and the buffer length is returned, therefore the caller
 * must check for "Unknown" in the buffer to detect a failure.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_max_vpi_show(struct device *dev, struct device_attribute *attr,
		  char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	uint32_t cnt;

	if (lpfc_get_hba_info(phba, NULL, NULL, NULL, NULL, &cnt, NULL))
		return snprintf(buf, PAGE_SIZE, "%d\n", cnt);
	return snprintf(buf, PAGE_SIZE, "Unknown\n");
}

/**
 * lpfc_used_vpi_show - Return maximum vpi minus the available vpi
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the used vpi count in decimal or "Unknown".
 *
 * Description:
 * Calls lpfc_get_hba_info() asking for just the mvpi and avpi counts.
 * If lpfc_get_hba_info() returns zero (failure) the buffer text is set
 * to "Unknown" and the buffer length is returned, therefore the caller
 * must check for "Unknown" in the buffer to detect a failure.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_used_vpi_show(struct device *dev, struct device_attribute *attr,
		   char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	uint32_t cnt, acnt;

	if (lpfc_get_hba_info(phba, NULL, NULL, NULL, NULL, &cnt, &acnt))
		return snprintf(buf, PAGE_SIZE, "%d\n", (cnt - acnt));
	return snprintf(buf, PAGE_SIZE, "Unknown\n");
}

/**
 * lpfc_npiv_info_show - Return text about NPIV support for the adapter
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: text that must be interpreted to determine if npiv is supported.
 *
 * Description:
 * Buffer will contain text indicating npiv is not suppoerted on the port,
 * the port is an NPIV physical port, or it is an npiv virtual port with
 * the id of the vport.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_npiv_info_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	if (!(phba->max_vpi))
		return snprintf(buf, PAGE_SIZE, "NPIV Not Supported\n");
	if (vport->port_type == LPFC_PHYSICAL_PORT)
		return snprintf(buf, PAGE_SIZE, "NPIV Physical\n");
	return snprintf(buf, PAGE_SIZE, "NPIV Virtual (VPI %d)\n", vport->vpi);
}

/**
 * lpfc_poll_show - Return text about poll support for the adapter
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the cfg_poll in hex.
 *
 * Notes:
 * cfg_poll should be a lpfc_polling_flags type.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_poll_show(struct device *dev, struct device_attribute *attr,
	       char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%#x\n", phba->cfg_poll);
}

/**
 * lpfc_poll_store - Set the value of cfg_poll for the adapter
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: one or more lpfc_polling_flags values.
 * @count: not used.
 *
 * Notes:
 * buf contents converted to integer and checked for a valid value.
 *
 * Returns:
 * -EINVAL if the buffer connot be converted or is out of range
 * length of the buf on success
 **/
static ssize_t
lpfc_poll_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	uint32_t creg_val;
	uint32_t old_val;
	int val=0;

	if (!isdigit(buf[0]))
		return -EINVAL;

	if (sscanf(buf, "%i", &val) != 1)
		return -EINVAL;

	if ((val & 0x3) != val)
		return -EINVAL;

	spin_lock_irq(&phba->hbalock);

	old_val = phba->cfg_poll;

	if (val & ENABLE_FCP_RING_POLLING) {
		if ((val & DISABLE_FCP_RING_INT) &&
		    !(old_val & DISABLE_FCP_RING_INT)) {
			creg_val = readl(phba->HCregaddr);
			creg_val &= ~(HC_R0INT_ENA << LPFC_FCP_RING);
			writel(creg_val, phba->HCregaddr);
			readl(phba->HCregaddr); /* flush */

			lpfc_poll_start_timer(phba);
		}
	} else if (val != 0x0) {
		spin_unlock_irq(&phba->hbalock);
		return -EINVAL;
	}

	if (!(val & DISABLE_FCP_RING_INT) &&
	    (old_val & DISABLE_FCP_RING_INT))
	{
		spin_unlock_irq(&phba->hbalock);
		del_timer(&phba->fcp_poll_timer);
		spin_lock_irq(&phba->hbalock);
		creg_val = readl(phba->HCregaddr);
		creg_val |= (HC_R0INT_ENA << LPFC_FCP_RING);
		writel(creg_val, phba->HCregaddr);
		readl(phba->HCregaddr); /* flush */
	}

	phba->cfg_poll = val;

	spin_unlock_irq(&phba->hbalock);

	return strlen(buf);
}

/**
 * lpfc_param_show - Return a cfg attribute value in decimal
 *
 * Description:
 * Macro that given an attr e.g. hba_queue_depth expands
 * into a function with the name lpfc_hba_queue_depth_show.
 *
 * lpfc_##attr##_show: Return the decimal value of an adapters cfg_xxx field.
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the attribute value in decimal.
 *
 * Returns: size of formatted string.
 **/
#define lpfc_param_show(attr)	\
static ssize_t \
lpfc_##attr##_show(struct device *dev, struct device_attribute *attr, \
		   char *buf) \
{ \
	struct Scsi_Host  *shost = class_to_shost(dev);\
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;\
	struct lpfc_hba   *phba = vport->phba;\
	int val = 0;\
	val = phba->cfg_##attr;\
	return snprintf(buf, PAGE_SIZE, "%d\n",\
			phba->cfg_##attr);\
}

/**
 * lpfc_param_hex_show - Return a cfg attribute value in hex
 *
 * Description:
 * Macro that given an attr e.g. hba_queue_depth expands
 * into a function with the name lpfc_hba_queue_depth_show
 *
 * lpfc_##attr##_show: Return the hex value of an adapters cfg_xxx field.
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the attribute value in hexadecimal.
 *
 * Returns: size of formatted string.
 **/
#define lpfc_param_hex_show(attr)	\
static ssize_t \
lpfc_##attr##_show(struct device *dev, struct device_attribute *attr, \
		   char *buf) \
{ \
	struct Scsi_Host  *shost = class_to_shost(dev);\
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;\
	struct lpfc_hba   *phba = vport->phba;\
	int val = 0;\
	val = phba->cfg_##attr;\
	return snprintf(buf, PAGE_SIZE, "%#x\n",\
			phba->cfg_##attr);\
}

/**
 * lpfc_param_init - Intializes a cfg attribute
 *
 * Description:
 * Macro that given an attr e.g. hba_queue_depth expands
 * into a function with the name lpfc_hba_queue_depth_init. The macro also
 * takes a default argument, a minimum and maximum argument.
 *
 * lpfc_##attr##_init: Initializes an attribute.
 * @phba: pointer the the adapter structure.
 * @val: integer attribute value.
 *
 * Validates the min and max values then sets the adapter config field
 * accordingly, or uses the default if out of range and prints an error message.
 *
 * Returns:
 * zero on success
 * -EINVAL if default used
 **/
#define lpfc_param_init(attr, default, minval, maxval)	\
static int \
lpfc_##attr##_init(struct lpfc_hba *phba, int val) \
{ \
	if (val >= minval && val <= maxval) {\
		phba->cfg_##attr = val;\
		return 0;\
	}\
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT, \
			"0449 lpfc_"#attr" attribute cannot be set to %d, "\
			"allowed range is ["#minval", "#maxval"]\n", val); \
	phba->cfg_##attr = default;\
	return -EINVAL;\
}

/**
 * lpfc_param_set - Set a cfg attribute value
 *
 * Description:
 * Macro that given an attr e.g. hba_queue_depth expands
 * into a function with the name lpfc_hba_queue_depth_set
 *
 * lpfc_##attr##_set: Sets an attribute value.
 * @phba: pointer the the adapter structure.
 * @val: integer attribute value.
 *
 * Description:
 * Validates the min and max values then sets the
 * adapter config field if in the valid range. prints error message
 * and does not set the parameter if invalid.
 *
 * Returns:
 * zero on success
 * -EINVAL if val is invalid
 **/
#define lpfc_param_set(attr, default, minval, maxval)	\
static int \
lpfc_##attr##_set(struct lpfc_hba *phba, int val) \
{ \
	if (val >= minval && val <= maxval) {\
		phba->cfg_##attr = val;\
		return 0;\
	}\
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT, \
			"0450 lpfc_"#attr" attribute cannot be set to %d, "\
			"allowed range is ["#minval", "#maxval"]\n", val); \
	return -EINVAL;\
}

/**
 * lpfc_param_store - Set a vport attribute value
 *
 * Description:
 * Macro that given an attr e.g. hba_queue_depth expands
 * into a function with the name lpfc_hba_queue_depth_store.
 *
 * lpfc_##attr##_store: Set an sttribute value.
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: contains the attribute value in ascii.
 * @count: not used.
 *
 * Description:
 * Convert the ascii text number to an integer, then
 * use the lpfc_##attr##_set function to set the value.
 *
 * Returns:
 * -EINVAL if val is invalid or lpfc_##attr##_set() fails
 * length of buffer upon success.
 **/
#define lpfc_param_store(attr)	\
static ssize_t \
lpfc_##attr##_store(struct device *dev, struct device_attribute *attr, \
		    const char *buf, size_t count) \
{ \
	struct Scsi_Host  *shost = class_to_shost(dev);\
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;\
	struct lpfc_hba   *phba = vport->phba;\
	int val=0;\
	if (!isdigit(buf[0]))\
		return -EINVAL;\
	if (sscanf(buf, "%i", &val) != 1)\
		return -EINVAL;\
	if (lpfc_##attr##_set(phba, val) == 0) \
		return strlen(buf);\
	else \
		return -EINVAL;\
}

/**
 * lpfc_vport_param_show - Return decimal formatted cfg attribute value
 *
 * Description:
 * Macro that given an attr e.g. hba_queue_depth expands
 * into a function with the name lpfc_hba_queue_depth_show
 *
 * lpfc_##attr##_show: prints the attribute value in decimal.
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the attribute value in decimal.
 *
 * Returns: length of formatted string.
 **/
#define lpfc_vport_param_show(attr)	\
static ssize_t \
lpfc_##attr##_show(struct device *dev, struct device_attribute *attr, \
		   char *buf) \
{ \
	struct Scsi_Host  *shost = class_to_shost(dev);\
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;\
	int val = 0;\
	val = vport->cfg_##attr;\
	return snprintf(buf, PAGE_SIZE, "%d\n", vport->cfg_##attr);\
}

/**
 * lpfc_vport_param_hex_show - Return hex formatted attribute value
 *
 * Description:
 * Macro that given an attr e.g.
 * hba_queue_depth expands into a function with the name
 * lpfc_hba_queue_depth_show
 *
 * lpfc_##attr##_show: prints the attribute value in hexadecimal.
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the attribute value in hexadecimal.
 *
 * Returns: length of formatted string.
 **/
#define lpfc_vport_param_hex_show(attr)	\
static ssize_t \
lpfc_##attr##_show(struct device *dev, struct device_attribute *attr, \
		   char *buf) \
{ \
	struct Scsi_Host  *shost = class_to_shost(dev);\
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;\
	int val = 0;\
	val = vport->cfg_##attr;\
	return snprintf(buf, PAGE_SIZE, "%#x\n", vport->cfg_##attr);\
}

/**
 * lpfc_vport_param_init - Initialize a vport cfg attribute
 *
 * Description:
 * Macro that given an attr e.g. hba_queue_depth expands
 * into a function with the name lpfc_hba_queue_depth_init. The macro also
 * takes a default argument, a minimum and maximum argument.
 *
 * lpfc_##attr##_init: validates the min and max values then sets the
 * adapter config field accordingly, or uses the default if out of range
 * and prints an error message.
 * @phba: pointer the the adapter structure.
 * @val: integer attribute value.
 *
 * Returns:
 * zero on success
 * -EINVAL if default used
 **/
#define lpfc_vport_param_init(attr, default, minval, maxval)	\
static int \
lpfc_##attr##_init(struct lpfc_vport *vport, int val) \
{ \
	if (val >= minval && val <= maxval) {\
		vport->cfg_##attr = val;\
		return 0;\
	}\
	lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT, \
			 "0423 lpfc_"#attr" attribute cannot be set to %d, "\
			 "allowed range is ["#minval", "#maxval"]\n", val); \
	vport->cfg_##attr = default;\
	return -EINVAL;\
}

/**
 * lpfc_vport_param_set - Set a vport cfg attribute
 *
 * Description:
 * Macro that given an attr e.g. hba_queue_depth expands
 * into a function with the name lpfc_hba_queue_depth_set
 *
 * lpfc_##attr##_set: validates the min and max values then sets the
 * adapter config field if in the valid range. prints error message
 * and does not set the parameter if invalid.
 * @phba: pointer the the adapter structure.
 * @val:	integer attribute value.
 *
 * Returns:
 * zero on success
 * -EINVAL if val is invalid
 **/
#define lpfc_vport_param_set(attr, default, minval, maxval)	\
static int \
lpfc_##attr##_set(struct lpfc_vport *vport, int val) \
{ \
	if (val >= minval && val <= maxval) {\
		vport->cfg_##attr = val;\
		return 0;\
	}\
	lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT, \
			 "0424 lpfc_"#attr" attribute cannot be set to %d, "\
			 "allowed range is ["#minval", "#maxval"]\n", val); \
	return -EINVAL;\
}

/**
 * lpfc_vport_param_store - Set a vport attribute
 *
 * Description:
 * Macro that given an attr e.g. hba_queue_depth
 * expands into a function with the name lpfc_hba_queue_depth_store
 *
 * lpfc_##attr##_store: convert the ascii text number to an integer, then
 * use the lpfc_##attr##_set function to set the value.
 * @cdev: class device that is converted into a Scsi_host.
 * @buf:	contains the attribute value in decimal.
 * @count: not used.
 *
 * Returns:
 * -EINVAL if val is invalid or lpfc_##attr##_set() fails
 * length of buffer upon success.
 **/
#define lpfc_vport_param_store(attr)	\
static ssize_t \
lpfc_##attr##_store(struct device *dev, struct device_attribute *attr, \
		    const char *buf, size_t count) \
{ \
	struct Scsi_Host  *shost = class_to_shost(dev);\
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;\
	int val=0;\
	if (!isdigit(buf[0]))\
		return -EINVAL;\
	if (sscanf(buf, "%i", &val) != 1)\
		return -EINVAL;\
	if (lpfc_##attr##_set(vport, val) == 0) \
		return strlen(buf);\
	else \
		return -EINVAL;\
}


#define LPFC_ATTR(name, defval, minval, maxval, desc) \
static int lpfc_##name = defval;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_param_init(name, defval, minval, maxval)

#define LPFC_ATTR_R(name, defval, minval, maxval, desc) \
static int lpfc_##name = defval;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_param_show(name)\
lpfc_param_init(name, defval, minval, maxval)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO , lpfc_##name##_show, NULL)

#define LPFC_ATTR_RW(name, defval, minval, maxval, desc) \
static int lpfc_##name = defval;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_param_show(name)\
lpfc_param_init(name, defval, minval, maxval)\
lpfc_param_set(name, defval, minval, maxval)\
lpfc_param_store(name)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO | S_IWUSR,\
		   lpfc_##name##_show, lpfc_##name##_store)

#define LPFC_ATTR_HEX_R(name, defval, minval, maxval, desc) \
static int lpfc_##name = defval;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_param_hex_show(name)\
lpfc_param_init(name, defval, minval, maxval)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO , lpfc_##name##_show, NULL)

#define LPFC_ATTR_HEX_RW(name, defval, minval, maxval, desc) \
static int lpfc_##name = defval;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_param_hex_show(name)\
lpfc_param_init(name, defval, minval, maxval)\
lpfc_param_set(name, defval, minval, maxval)\
lpfc_param_store(name)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO | S_IWUSR,\
		   lpfc_##name##_show, lpfc_##name##_store)

#define LPFC_VPORT_ATTR(name, defval, minval, maxval, desc) \
static int lpfc_##name = defval;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_vport_param_init(name, defval, minval, maxval)

#define LPFC_VPORT_ATTR_R(name, defval, minval, maxval, desc) \
static int lpfc_##name = defval;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_vport_param_show(name)\
lpfc_vport_param_init(name, defval, minval, maxval)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO , lpfc_##name##_show, NULL)

#define LPFC_VPORT_ATTR_RW(name, defval, minval, maxval, desc) \
static int lpfc_##name = defval;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_vport_param_show(name)\
lpfc_vport_param_init(name, defval, minval, maxval)\
lpfc_vport_param_set(name, defval, minval, maxval)\
lpfc_vport_param_store(name)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO | S_IWUSR,\
		   lpfc_##name##_show, lpfc_##name##_store)

#define LPFC_VPORT_ATTR_HEX_R(name, defval, minval, maxval, desc) \
static int lpfc_##name = defval;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_vport_param_hex_show(name)\
lpfc_vport_param_init(name, defval, minval, maxval)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO , lpfc_##name##_show, NULL)

#define LPFC_VPORT_ATTR_HEX_RW(name, defval, minval, maxval, desc) \
static int lpfc_##name = defval;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
lpfc_vport_param_hex_show(name)\
lpfc_vport_param_init(name, defval, minval, maxval)\
lpfc_vport_param_set(name, defval, minval, maxval)\
lpfc_vport_param_store(name)\
static DEVICE_ATTR(lpfc_##name, S_IRUGO | S_IWUSR,\
		   lpfc_##name##_show, lpfc_##name##_store)

static DEVICE_ATTR(bg_info, S_IRUGO, lpfc_bg_info_show, NULL);
static DEVICE_ATTR(bg_guard_err, S_IRUGO, lpfc_bg_guard_err_show, NULL);
static DEVICE_ATTR(bg_apptag_err, S_IRUGO, lpfc_bg_apptag_err_show, NULL);
static DEVICE_ATTR(bg_reftag_err, S_IRUGO, lpfc_bg_reftag_err_show, NULL);
static DEVICE_ATTR(info, S_IRUGO, lpfc_info_show, NULL);
static DEVICE_ATTR(serialnum, S_IRUGO, lpfc_serialnum_show, NULL);
static DEVICE_ATTR(modeldesc, S_IRUGO, lpfc_modeldesc_show, NULL);
static DEVICE_ATTR(modelname, S_IRUGO, lpfc_modelname_show, NULL);
static DEVICE_ATTR(programtype, S_IRUGO, lpfc_programtype_show, NULL);
static DEVICE_ATTR(portnum, S_IRUGO, lpfc_vportnum_show, NULL);
static DEVICE_ATTR(fwrev, S_IRUGO, lpfc_fwrev_show, NULL);
static DEVICE_ATTR(hdw, S_IRUGO, lpfc_hdw_show, NULL);
static DEVICE_ATTR(link_state, S_IRUGO, lpfc_link_state_show, NULL);
static DEVICE_ATTR(option_rom_version, S_IRUGO,
		   lpfc_option_rom_version_show, NULL);
static DEVICE_ATTR(num_discovered_ports, S_IRUGO,
		   lpfc_num_discovered_ports_show, NULL);
static DEVICE_ATTR(menlo_mgmt_mode, S_IRUGO, lpfc_mlomgmt_show, NULL);
static DEVICE_ATTR(nport_evt_cnt, S_IRUGO, lpfc_nport_evt_cnt_show, NULL);
static DEVICE_ATTR(lpfc_drvr_version, S_IRUGO, lpfc_drvr_version_show, NULL);
static DEVICE_ATTR(board_mode, S_IRUGO | S_IWUSR,
		   lpfc_board_mode_show, lpfc_board_mode_store);
static DEVICE_ATTR(issue_reset, S_IWUSR, NULL, lpfc_issue_reset);
static DEVICE_ATTR(max_vpi, S_IRUGO, lpfc_max_vpi_show, NULL);
static DEVICE_ATTR(used_vpi, S_IRUGO, lpfc_used_vpi_show, NULL);
static DEVICE_ATTR(max_rpi, S_IRUGO, lpfc_max_rpi_show, NULL);
static DEVICE_ATTR(used_rpi, S_IRUGO, lpfc_used_rpi_show, NULL);
static DEVICE_ATTR(max_xri, S_IRUGO, lpfc_max_xri_show, NULL);
static DEVICE_ATTR(used_xri, S_IRUGO, lpfc_used_xri_show, NULL);
static DEVICE_ATTR(npiv_info, S_IRUGO, lpfc_npiv_info_show, NULL);
static DEVICE_ATTR(lpfc_temp_sensor, S_IRUGO, lpfc_temp_sensor_show, NULL);


static char *lpfc_soft_wwn_key = "C99G71SL8032A";

/**
 * lpfc_soft_wwn_enable_store - Allows setting of the wwn if the key is valid
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: containing the string lpfc_soft_wwn_key.
 * @count: must be size of lpfc_soft_wwn_key.
 *
 * Returns:
 * -EINVAL if the buffer does not contain lpfc_soft_wwn_key
 * length of buf indicates success
 **/
static ssize_t
lpfc_soft_wwn_enable_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	unsigned int cnt = count;

	/*
	 * We're doing a simple sanity check for soft_wwpn setting.
	 * We require that the user write a specific key to enable
	 * the soft_wwpn attribute to be settable. Once the attribute
	 * is written, the enable key resets. If further updates are
	 * desired, the key must be written again to re-enable the
	 * attribute.
	 *
	 * The "key" is not secret - it is a hardcoded string shown
	 * here. The intent is to protect against the random user or
	 * application that is just writing attributes.
	 */

	/* count may include a LF at end of string */
	if (buf[cnt-1] == '\n')
		cnt--;

	if ((cnt != strlen(lpfc_soft_wwn_key)) ||
	    (strncmp(buf, lpfc_soft_wwn_key, strlen(lpfc_soft_wwn_key)) != 0))
		return -EINVAL;

	phba->soft_wwn_enable = 1;
	return count;
}
static DEVICE_ATTR(lpfc_soft_wwn_enable, S_IWUSR, NULL,
		   lpfc_soft_wwn_enable_store);

/**
 * lpfc_soft_wwpn_show - Return the cfg soft ww port name of the adapter
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the wwpn in hexadecimal.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_soft_wwpn_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "0x%llx\n",
			(unsigned long long)phba->cfg_soft_wwpn);
}

/**
 * lpfc_soft_wwpn_store - Set the ww port name of the adapter
 * @dev class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: contains the wwpn in hexadecimal.
 * @count: number of wwpn bytes in buf
 *
 * Returns:
 * -EACCES hba reset not enabled, adapter over temp
 * -EINVAL soft wwn not enabled, count is invalid, invalid wwpn byte invalid
 * -EIO error taking adapter offline or online
 * value of count on success
 **/
static ssize_t
lpfc_soft_wwpn_store(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct completion online_compl;
	int stat1=0, stat2=0;
	unsigned int i, j, cnt=count;
	u8 wwpn[8];

	if (!phba->cfg_enable_hba_reset)
		return -EACCES;
	spin_lock_irq(&phba->hbalock);
	if (phba->over_temp_state == HBA_OVER_TEMP) {
		spin_unlock_irq(&phba->hbalock);
		return -EACCES;
	}
	spin_unlock_irq(&phba->hbalock);
	/* count may include a LF at end of string */
	if (buf[cnt-1] == '\n')
		cnt--;

	if (!phba->soft_wwn_enable || (cnt < 16) || (cnt > 18) ||
	    ((cnt == 17) && (*buf++ != 'x')) ||
	    ((cnt == 18) && ((*buf++ != '0') || (*buf++ != 'x'))))
		return -EINVAL;

	phba->soft_wwn_enable = 0;

	memset(wwpn, 0, sizeof(wwpn));

	/* Validate and store the new name */
	for (i=0, j=0; i < 16; i++) {
		if ((*buf >= 'a') && (*buf <= 'f'))
			j = ((j << 4) | ((*buf++ -'a') + 10));
		else if ((*buf >= 'A') && (*buf <= 'F'))
			j = ((j << 4) | ((*buf++ -'A') + 10));
		else if ((*buf >= '0') && (*buf <= '9'))
			j = ((j << 4) | (*buf++ -'0'));
		else
			return -EINVAL;
		if (i % 2) {
			wwpn[i/2] = j & 0xff;
			j = 0;
		}
	}
	phba->cfg_soft_wwpn = wwn_to_u64(wwpn);
	fc_host_port_name(shost) = phba->cfg_soft_wwpn;
	if (phba->cfg_soft_wwnn)
		fc_host_node_name(shost) = phba->cfg_soft_wwnn;

	dev_printk(KERN_NOTICE, &phba->pcidev->dev,
		   "lpfc%d: Reinitializing to use soft_wwpn\n", phba->brd_no);

	stat1 = lpfc_do_offline(phba, LPFC_EVT_OFFLINE);
	if (stat1)
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0463 lpfc_soft_wwpn attribute set failed to "
				"reinit adapter - %d\n", stat1);
	init_completion(&online_compl);
	lpfc_workq_post_event(phba, &stat2, &online_compl, LPFC_EVT_ONLINE);
	wait_for_completion(&online_compl);
	if (stat2)
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0464 lpfc_soft_wwpn attribute set failed to "
				"reinit adapter - %d\n", stat2);
	return (stat1 || stat2) ? -EIO : count;
}
static DEVICE_ATTR(lpfc_soft_wwpn, S_IRUGO | S_IWUSR,\
		   lpfc_soft_wwpn_show, lpfc_soft_wwpn_store);

/**
 * lpfc_soft_wwnn_show - Return the cfg soft ww node name for the adapter
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the wwnn in hexadecimal.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_soft_wwnn_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	return snprintf(buf, PAGE_SIZE, "0x%llx\n",
			(unsigned long long)phba->cfg_soft_wwnn);
}

/**
 * lpfc_soft_wwnn_store - sets the ww node name of the adapter
 * @cdev: class device that is converted into a Scsi_host.
 * @buf: contains the ww node name in hexadecimal.
 * @count: number of wwnn bytes in buf.
 *
 * Returns:
 * -EINVAL soft wwn not enabled, count is invalid, invalid wwnn byte invalid
 * value of count on success
 **/
static ssize_t
lpfc_soft_wwnn_store(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct lpfc_hba *phba = ((struct lpfc_vport *)shost->hostdata)->phba;
	unsigned int i, j, cnt=count;
	u8 wwnn[8];

	/* count may include a LF at end of string */
	if (buf[cnt-1] == '\n')
		cnt--;

	if (!phba->soft_wwn_enable || (cnt < 16) || (cnt > 18) ||
	    ((cnt == 17) && (*buf++ != 'x')) ||
	    ((cnt == 18) && ((*buf++ != '0') || (*buf++ != 'x'))))
		return -EINVAL;

	/*
	 * Allow wwnn to be set many times, as long as the enable is set.
	 * However, once the wwpn is set, everything locks.
	 */

	memset(wwnn, 0, sizeof(wwnn));

	/* Validate and store the new name */
	for (i=0, j=0; i < 16; i++) {
		if ((*buf >= 'a') && (*buf <= 'f'))
			j = ((j << 4) | ((*buf++ -'a') + 10));
		else if ((*buf >= 'A') && (*buf <= 'F'))
			j = ((j << 4) | ((*buf++ -'A') + 10));
		else if ((*buf >= '0') && (*buf <= '9'))
			j = ((j << 4) | (*buf++ -'0'));
		else
			return -EINVAL;
		if (i % 2) {
			wwnn[i/2] = j & 0xff;
			j = 0;
		}
	}
	phba->cfg_soft_wwnn = wwn_to_u64(wwnn);

	dev_printk(KERN_NOTICE, &phba->pcidev->dev,
		   "lpfc%d: soft_wwnn set. Value will take effect upon "
		   "setting of the soft_wwpn\n", phba->brd_no);

	return count;
}
static DEVICE_ATTR(lpfc_soft_wwnn, S_IRUGO | S_IWUSR,\
		   lpfc_soft_wwnn_show, lpfc_soft_wwnn_store);


static int lpfc_poll = 0;
module_param(lpfc_poll, int, 0);
MODULE_PARM_DESC(lpfc_poll, "FCP ring polling mode control:"
		 " 0 - none,"
		 " 1 - poll with interrupts enabled"
		 " 3 - poll and disable FCP ring interrupts");

static DEVICE_ATTR(lpfc_poll, S_IRUGO | S_IWUSR,
		   lpfc_poll_show, lpfc_poll_store);

int  lpfc_sli_mode = 0;
module_param(lpfc_sli_mode, int, 0);
MODULE_PARM_DESC(lpfc_sli_mode, "SLI mode selector:"
		 " 0 - auto (SLI-3 if supported),"
		 " 2 - select SLI-2 even on SLI-3 capable HBAs,"
		 " 3 - select SLI-3");

int lpfc_enable_npiv = 0;
module_param(lpfc_enable_npiv, int, 0);
MODULE_PARM_DESC(lpfc_enable_npiv, "Enable NPIV functionality");
lpfc_param_show(enable_npiv);
lpfc_param_init(enable_npiv, 0, 0, 1);
static DEVICE_ATTR(lpfc_enable_npiv, S_IRUGO,
			 lpfc_enable_npiv_show, NULL);

/*
# lpfc_nodev_tmo: If set, it will hold all I/O errors on devices that disappear
# until the timer expires. Value range is [0,255]. Default value is 30.
*/
static int lpfc_nodev_tmo = LPFC_DEF_DEVLOSS_TMO;
static int lpfc_devloss_tmo = LPFC_DEF_DEVLOSS_TMO;
module_param(lpfc_nodev_tmo, int, 0);
MODULE_PARM_DESC(lpfc_nodev_tmo,
		 "Seconds driver will hold I/O waiting "
		 "for a device to come back");

/**
 * lpfc_nodev_tmo_show - Return the hba dev loss timeout value
 * @dev: class converted to a Scsi_host structure.
 * @attr: device attribute, not used.
 * @buf: on return contains the dev loss timeout in decimal.
 *
 * Returns: size of formatted string.
 **/
static ssize_t
lpfc_nodev_tmo_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	int val = 0;
	val = vport->cfg_devloss_tmo;
	return snprintf(buf, PAGE_SIZE, "%d\n",	vport->cfg_devloss_tmo);
}

/**
 * lpfc_nodev_tmo_init - Set the hba nodev timeout value
 * @vport: lpfc vport structure pointer.
 * @val: contains the nodev timeout value.
 *
 * Description:
 * If the devloss tmo is already set then nodev tmo is set to devloss tmo,
 * a kernel error message is printed and zero is returned.
 * Else if val is in range then nodev tmo and devloss tmo are set to val.
 * Otherwise nodev tmo is set to the default value.
 *
 * Returns:
 * zero if already set or if val is in range
 * -EINVAL val out of range
 **/
static int
lpfc_nodev_tmo_init(struct lpfc_vport *vport, int val)
{
	if (vport->cfg_devloss_tmo != LPFC_DEF_DEVLOSS_TMO) {
		vport->cfg_nodev_tmo = vport->cfg_devloss_tmo;
		if (val != LPFC_DEF_DEVLOSS_TMO)
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
					 "0407 Ignoring nodev_tmo module "
					 "parameter because devloss_tmo is "
					 "set.\n");
		return 0;
	}

	if (val >= LPFC_MIN_DEVLOSS_TMO && val <= LPFC_MAX_DEVLOSS_TMO) {
		vport->cfg_nodev_tmo = val;
		vport->cfg_devloss_tmo = val;
		return 0;
	}
	lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
			 "0400 lpfc_nodev_tmo attribute cannot be set to"
			 " %d, allowed range is [%d, %d]\n",
			 val, LPFC_MIN_DEVLOSS_TMO, LPFC_MAX_DEVLOSS_TMO);
	vport->cfg_nodev_tmo = LPFC_DEF_DEVLOSS_TMO;
	return -EINVAL;
}

/**
 * lpfc_update_rport_devloss_tmo - Update dev loss tmo value
 * @vport: lpfc vport structure pointer.
 *
 * Description:
 * Update all the ndlp's dev loss tmo with the vport devloss tmo value.
 **/
static void
lpfc_update_rport_devloss_tmo(struct lpfc_vport *vport)
{
	struct Scsi_Host  *shost;
	struct lpfc_nodelist  *ndlp;

	shost = lpfc_shost_from_vport(vport);
	spin_lock_irq(shost->host_lock);
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp)
		if (NLP_CHK_NODE_ACT(ndlp) && ndlp->rport)
			ndlp->rport->dev_loss_tmo = vport->cfg_devloss_tmo;
	spin_unlock_irq(shost->host_lock);
}

/**
 * lpfc_nodev_tmo_set - Set the vport nodev tmo and devloss tmo values
 * @vport: lpfc vport structure pointer.
 * @val: contains the tmo value.
 *
 * Description:
 * If the devloss tmo is already set or the vport dev loss tmo has changed
 * then a kernel error message is printed and zero is returned.
 * Else if val is in range then nodev tmo and devloss tmo are set to val.
 * Otherwise nodev tmo is set to the default value.
 *
 * Returns:
 * zero if already set or if val is in range
 * -EINVAL val out of range
 **/
static int
lpfc_nodev_tmo_set(struct lpfc_vport *vport, int val)
{
	if (vport->dev_loss_tmo_changed ||
	    (lpfc_devloss_tmo != LPFC_DEF_DEVLOSS_TMO)) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				 "0401 Ignoring change to nodev_tmo "
				 "because devloss_tmo is set.\n");
		return 0;
	}
	if (val >= LPFC_MIN_DEVLOSS_TMO && val <= LPFC_MAX_DEVLOSS_TMO) {
		vport->cfg_nodev_tmo = val;
		vport->cfg_devloss_tmo = val;
		lpfc_update_rport_devloss_tmo(vport);
		return 0;
	}
	lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
			 "0403 lpfc_nodev_tmo attribute cannot be set to"
			 "%d, allowed range is [%d, %d]\n",
			 val, LPFC_MIN_DEVLOSS_TMO, LPFC_MAX_DEVLOSS_TMO);
	return -EINVAL;
}

lpfc_vport_param_store(nodev_tmo)

static DEVICE_ATTR(lpfc_nodev_tmo, S_IRUGO | S_IWUSR,
		   lpfc_nodev_tmo_show, lpfc_nodev_tmo_store);

/*
# lpfc_devloss_tmo: If set, it will hold all I/O errors on devices that
# disappear until the timer expires. Value range is [0,255]. Default
# value is 30.
*/
module_param(lpfc_devloss_tmo, int, 0);
MODULE_PARM_DESC(lpfc_devloss_tmo,
		 "Seconds driver will hold I/O waiting "
		 "for a device to come back");
lpfc_vport_param_init(devloss_tmo, LPFC_DEF_DEVLOSS_TMO,
		      LPFC_MIN_DEVLOSS_TMO, LPFC_MAX_DEVLOSS_TMO)
lpfc_vport_param_show(devloss_tmo)

/**
 * lpfc_devloss_tmo_set - Sets vport nodev tmo, devloss tmo values, changed bit
 * @vport: lpfc vport structure pointer.
 * @val: contains the tmo value.
 *
 * Description:
 * If val is in a valid range then set the vport nodev tmo,
 * devloss tmo, also set the vport dev loss tmo changed flag.
 * Else a kernel error message is printed.
 *
 * Returns:
 * zero if val is in range
 * -EINVAL val out of range
 **/
static int
lpfc_devloss_tmo_set(struct lpfc_vport *vport, int val)
{
	if (val >= LPFC_MIN_DEVLOSS_TMO && val <= LPFC_MAX_DEVLOSS_TMO) {
		vport->cfg_nodev_tmo = val;
		vport->cfg_devloss_tmo = val;
		vport->dev_loss_tmo_changed = 1;
		lpfc_update_rport_devloss_tmo(vport);
		return 0;
	}

	lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
			 "0404 lpfc_devloss_tmo attribute cannot be set to"
			 " %d, allowed range is [%d, %d]\n",
			 val, LPFC_MIN_DEVLOSS_TMO, LPFC_MAX_DEVLOSS_TMO);
	return -EINVAL;
}

lpfc_vport_param_store(devloss_tmo)
static DEVICE_ATTR(lpfc_devloss_tmo, S_IRUGO | S_IWUSR,
		   lpfc_devloss_tmo_show, lpfc_devloss_tmo_store);

/*
# lpfc_log_verbose: Only turn this flag on if you are willing to risk being
# deluged with LOTS of information.
# You can set a bit mask to record specific types of verbose messages:
# See lpfc_logmsh.h for definitions.
*/
LPFC_VPORT_ATTR_HEX_RW(log_verbose, 0x0, 0x0, 0xffffffff,
		       "Verbose logging bit-mask");

/*
# lpfc_enable_da_id: This turns on the DA_ID CT command that deregisters
# objects that have been registered with the nameserver after login.
*/
LPFC_VPORT_ATTR_R(enable_da_id, 0, 0, 1,
		  "Deregister nameserver objects before LOGO");

/*
# lun_queue_depth:  This parameter is used to limit the number of outstanding
# commands per FCP LUN. Value range is [1,128]. Default value is 30.
*/
LPFC_VPORT_ATTR_R(lun_queue_depth, 30, 1, 128,
		  "Max number of FCP commands we can queue to a specific LUN");

/*
# hba_queue_depth:  This parameter is used to limit the number of outstanding
# commands per lpfc HBA. Value range is [32,8192]. If this parameter
# value is greater than the maximum number of exchanges supported by the HBA,
# then maximum number of exchanges supported by the HBA is used to determine
# the hba_queue_depth.
*/
LPFC_ATTR_R(hba_queue_depth, 8192, 32, 8192,
	    "Max number of FCP commands we can queue to a lpfc HBA");

/*
# peer_port_login:  This parameter allows/prevents logins
# between peer ports hosted on the same physical port.
# When this parameter is set 0 peer ports of same physical port
# are not allowed to login to each other.
# When this parameter is set 1 peer ports of same physical port
# are allowed to login to each other.
# Default value of this parameter is 0.
*/
LPFC_VPORT_ATTR_R(peer_port_login, 0, 0, 1,
		  "Allow peer ports on the same physical port to login to each "
		  "other.");

/*
# restrict_login:  This parameter allows/prevents logins
# between Virtual Ports and remote initiators.
# When this parameter is not set (0) Virtual Ports will accept PLOGIs from
# other initiators and will attempt to PLOGI all remote ports.
# When this parameter is set (1) Virtual Ports will reject PLOGIs from
# remote ports and will not attempt to PLOGI to other initiators.
# This parameter does not restrict to the physical port.
# This parameter does not restrict logins to Fabric resident remote ports.
# Default value of this parameter is 1.
*/
static int lpfc_restrict_login = 1;
module_param(lpfc_restrict_login, int, 0);
MODULE_PARM_DESC(lpfc_restrict_login,
		 "Restrict virtual ports login to remote initiators.");
lpfc_vport_param_show(restrict_login);

/**
 * lpfc_restrict_login_init - Set the vport restrict login flag
 * @vport: lpfc vport structure pointer.
 * @val: contains the restrict login value.
 *
 * Description:
 * If val is not in a valid range then log a kernel error message and set
 * the vport restrict login to one.
 * If the port type is physical clear the restrict login flag and return.
 * Else set the restrict login flag to val.
 *
 * Returns:
 * zero if val is in range
 * -EINVAL val out of range
 **/
static int
lpfc_restrict_login_init(struct lpfc_vport *vport, int val)
{
	if (val < 0 || val > 1) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				 "0422 lpfc_restrict_login attribute cannot "
				 "be set to %d, allowed range is [0, 1]\n",
				 val);
		vport->cfg_restrict_login = 1;
		return -EINVAL;
	}
	if (vport->port_type == LPFC_PHYSICAL_PORT) {
		vport->cfg_restrict_login = 0;
		return 0;
	}
	vport->cfg_restrict_login = val;
	return 0;
}

/**
 * lpfc_restrict_login_set - Set the vport restrict login flag
 * @vport: lpfc vport structure pointer.
 * @val: contains the restrict login value.
 *
 * Description:
 * If val is not in a valid range then log a kernel error message and set
 * the vport restrict login to one.
 * If the port type is physical and the val is not zero log a kernel
 * error message, clear the restrict login flag and return zero.
 * Else set the restrict login flag to val.
 *
 * Returns:
 * zero if val is in range
 * -EINVAL val out of range
 **/
static int
lpfc_restrict_login_set(struct lpfc_vport *vport, int val)
{
	if (val < 0 || val > 1) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				 "0425 lpfc_restrict_login attribute cannot "
				 "be set to %d, allowed range is [0, 1]\n",
				 val);
		vport->cfg_restrict_login = 1;
		return -EINVAL;
	}
	if (vport->port_type == LPFC_PHYSICAL_PORT && val != 0) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
				 "0468 lpfc_restrict_login must be 0 for "
				 "Physical ports.\n");
		vport->cfg_restrict_login = 0;
		return 0;
	}
	vport->cfg_restrict_login = val;
	return 0;
}
lpfc_vport_param_store(restrict_login);
static DEVICE_ATTR(lpfc_restrict_login, S_IRUGO | S_IWUSR,
		   lpfc_restrict_login_show, lpfc_restrict_login_store);

/*
# Some disk devices have a "select ID" or "select Target" capability.
# From a protocol standpoint "select ID" usually means select the
# Fibre channel "ALPA".  In the FC-AL Profile there is an "informative
# annex" which contains a table that maps a "select ID" (a number
# between 0 and 7F) to an ALPA.  By default, for compatibility with
# older drivers, the lpfc driver scans this table from low ALPA to high
# ALPA.
#
# Turning on the scan-down variable (on  = 1, off = 0) will
# cause the lpfc driver to use an inverted table, effectively
# scanning ALPAs from high to low. Value range is [0,1]. Default value is 1.
#
# (Note: This "select ID" functionality is a LOOP ONLY characteristic
# and will not work across a fabric. Also this parameter will take
# effect only in the case when ALPA map is not available.)
*/
LPFC_VPORT_ATTR_R(scan_down, 1, 0, 1,
		  "Start scanning for devices from highest ALPA to lowest");

/*
# lpfc_topology:  link topology for init link
#            0x0  = attempt loop mode then point-to-point
#            0x01 = internal loopback mode
#            0x02 = attempt point-to-point mode only
#            0x04 = attempt loop mode only
#            0x06 = attempt point-to-point mode then loop
# Set point-to-point mode if you want to run as an N_Port.
# Set loop mode if you want to run as an NL_Port. Value range is [0,0x6].
# Default value is 0.
*/

/**
 * lpfc_topology_set - Set the adapters topology field
 * @phba: lpfc_hba pointer.
 * @val: topology value.
 *
 * Description:
 * If val is in a valid range then set the adapter's topology field and
 * issue a lip; if the lip fails reset the topology to the old value.
 *
 * If the value is not in range log a kernel error message and return an error.
 *
 * Returns:
 * zero if val is in range and lip okay
 * non-zero return value from lpfc_issue_lip()
 * -EINVAL val out of range
 **/
static ssize_t
lpfc_topology_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	int val = 0;
	int nolip = 0;
	const char *val_buf = buf;
	int err;
	uint32_t prev_val;

	if (!strncmp(buf, "nolip ", strlen("nolip "))) {
		nolip = 1;
		val_buf = &buf[strlen("nolip ")];
	}

	if (!isdigit(val_buf[0]))
		return -EINVAL;
	if (sscanf(val_buf, "%i", &val) != 1)
		return -EINVAL;

	if (val >= 0 && val <= 6) {
		prev_val = phba->cfg_topology;
		phba->cfg_topology = val;
		if (nolip)
			return strlen(buf);

		err = lpfc_issue_lip(lpfc_shost_from_vport(phba->pport));
		if (err) {
			phba->cfg_topology = prev_val;
			return -EINVAL;
		} else
			return strlen(buf);
	}
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
		"%d:0467 lpfc_topology attribute cannot be set to %d, "
		"allowed range is [0, 6]\n",
		phba->brd_no, val);
	return -EINVAL;
}
static int lpfc_topology = 0;
module_param(lpfc_topology, int, 0);
MODULE_PARM_DESC(lpfc_topology, "Select Fibre Channel topology");
lpfc_param_show(topology)
lpfc_param_init(topology, 0, 0, 6)
static DEVICE_ATTR(lpfc_topology, S_IRUGO | S_IWUSR,
		lpfc_topology_show, lpfc_topology_store);

/**
 * lpfc_static_vport_show: Read callback function for
 *   lpfc_static_vport sysfs file.
 * @dev: Pointer to class device object.
 * @attr: device attribute structure.
 * @buf: Data buffer.
 *
 * This function is the read call back function for
 * lpfc_static_vport sysfs file. The lpfc_static_vport
 * sysfs file report the mageability of the vport.
 **/
static ssize_t
lpfc_static_vport_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	if (vport->vport_flag & STATIC_VPORT)
		sprintf(buf, "1\n");
	else
		sprintf(buf, "0\n");

	return strlen(buf);
}

/*
 * Sysfs attribute to control the statistical data collection.
 */
static DEVICE_ATTR(lpfc_static_vport, S_IRUGO,
		   lpfc_static_vport_show, NULL);

/**
 * lpfc_stat_data_ctrl_store - write call back for lpfc_stat_data_ctrl sysfs file
 * @dev: Pointer to class device.
 * @buf: Data buffer.
 * @count: Size of the data buffer.
 *
 * This function get called when an user write to the lpfc_stat_data_ctrl
 * sysfs file. This function parse the command written to the sysfs file
 * and take appropriate action. These commands are used for controlling
 * driver statistical data collection.
 * Following are the command this function handles.
 *
 *    setbucket <bucket_type> <base> <step>
 *			       = Set the latency buckets.
 *    destroybucket            = destroy all the buckets.
 *    start                    = start data collection
 *    stop                     = stop data collection
 *    reset                    = reset the collected data
 **/
static ssize_t
lpfc_stat_data_ctrl_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
#define LPFC_MAX_DATA_CTRL_LEN 1024
	static char bucket_data[LPFC_MAX_DATA_CTRL_LEN];
	unsigned long i;
	char *str_ptr, *token;
	struct lpfc_vport **vports;
	struct Scsi_Host *v_shost;
	char *bucket_type_str, *base_str, *step_str;
	unsigned long base, step, bucket_type;

	if (!strncmp(buf, "setbucket", strlen("setbucket"))) {
		if (strlen(buf) > (LPFC_MAX_DATA_CTRL_LEN - 1))
			return -EINVAL;

		strcpy(bucket_data, buf);
		str_ptr = &bucket_data[0];
		/* Ignore this token - this is command token */
		token = strsep(&str_ptr, "\t ");
		if (!token)
			return -EINVAL;

		bucket_type_str = strsep(&str_ptr, "\t ");
		if (!bucket_type_str)
			return -EINVAL;

		if (!strncmp(bucket_type_str, "linear", strlen("linear")))
			bucket_type = LPFC_LINEAR_BUCKET;
		else if (!strncmp(bucket_type_str, "power2", strlen("power2")))
			bucket_type = LPFC_POWER2_BUCKET;
		else
			return -EINVAL;

		base_str = strsep(&str_ptr, "\t ");
		if (!base_str)
			return -EINVAL;
		base = simple_strtoul(base_str, NULL, 0);

		step_str = strsep(&str_ptr, "\t ");
		if (!step_str)
			return -EINVAL;
		step = simple_strtoul(step_str, NULL, 0);
		if (!step)
			return -EINVAL;

		/* Block the data collection for every vport */
		vports = lpfc_create_vport_work_array(phba);
		if (vports == NULL)
			return -ENOMEM;

		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			v_shost = lpfc_shost_from_vport(vports[i]);
			spin_lock_irq(v_shost->host_lock);
			/* Block and reset data collection */
			vports[i]->stat_data_blocked = 1;
			if (vports[i]->stat_data_enabled)
				lpfc_vport_reset_stat_data(vports[i]);
			spin_unlock_irq(v_shost->host_lock);
		}

		/* Set the bucket attributes */
		phba->bucket_type = bucket_type;
		phba->bucket_base = base;
		phba->bucket_step = step;

		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			v_shost = lpfc_shost_from_vport(vports[i]);

			/* Unblock data collection */
			spin_lock_irq(v_shost->host_lock);
			vports[i]->stat_data_blocked = 0;
			spin_unlock_irq(v_shost->host_lock);
		}
		lpfc_destroy_vport_work_array(phba, vports);
		return strlen(buf);
	}

	if (!strncmp(buf, "destroybucket", strlen("destroybucket"))) {
		vports = lpfc_create_vport_work_array(phba);
		if (vports == NULL)
			return -ENOMEM;

		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			v_shost = lpfc_shost_from_vport(vports[i]);
			spin_lock_irq(shost->host_lock);
			vports[i]->stat_data_blocked = 1;
			lpfc_free_bucket(vport);
			vport->stat_data_enabled = 0;
			vports[i]->stat_data_blocked = 0;
			spin_unlock_irq(shost->host_lock);
		}
		lpfc_destroy_vport_work_array(phba, vports);
		phba->bucket_type = LPFC_NO_BUCKET;
		phba->bucket_base = 0;
		phba->bucket_step = 0;
		return strlen(buf);
	}

	if (!strncmp(buf, "start", strlen("start"))) {
		/* If no buckets configured return error */
		if (phba->bucket_type == LPFC_NO_BUCKET)
			return -EINVAL;
		spin_lock_irq(shost->host_lock);
		if (vport->stat_data_enabled) {
			spin_unlock_irq(shost->host_lock);
			return strlen(buf);
		}
		lpfc_alloc_bucket(vport);
		vport->stat_data_enabled = 1;
		spin_unlock_irq(shost->host_lock);
		return strlen(buf);
	}

	if (!strncmp(buf, "stop", strlen("stop"))) {
		spin_lock_irq(shost->host_lock);
		if (vport->stat_data_enabled == 0) {
			spin_unlock_irq(shost->host_lock);
			return strlen(buf);
		}
		lpfc_free_bucket(vport);
		vport->stat_data_enabled = 0;
		spin_unlock_irq(shost->host_lock);
		return strlen(buf);
	}

	if (!strncmp(buf, "reset", strlen("reset"))) {
		if ((phba->bucket_type == LPFC_NO_BUCKET)
			|| !vport->stat_data_enabled)
			return strlen(buf);
		spin_lock_irq(shost->host_lock);
		vport->stat_data_blocked = 1;
		lpfc_vport_reset_stat_data(vport);
		vport->stat_data_blocked = 0;
		spin_unlock_irq(shost->host_lock);
		return strlen(buf);
	}
	return -EINVAL;
}


/**
 * lpfc_stat_data_ctrl_show - Read function for lpfc_stat_data_ctrl sysfs file
 * @dev: Pointer to class device object.
 * @buf: Data buffer.
 *
 * This function is the read call back function for
 * lpfc_stat_data_ctrl sysfs file. This function report the
 * current statistical data collection state.
 **/
static ssize_t
lpfc_stat_data_ctrl_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	int index = 0;
	int i;
	char *bucket_type;
	unsigned long bucket_value;

	switch (phba->bucket_type) {
	case LPFC_LINEAR_BUCKET:
		bucket_type = "linear";
		break;
	case LPFC_POWER2_BUCKET:
		bucket_type = "power2";
		break;
	default:
		bucket_type = "No Bucket";
		break;
	}

	sprintf(&buf[index], "Statistical Data enabled :%d, "
		"blocked :%d, Bucket type :%s, Bucket base :%d,"
		" Bucket step :%d\nLatency Ranges :",
		vport->stat_data_enabled, vport->stat_data_blocked,
		bucket_type, phba->bucket_base, phba->bucket_step);
	index = strlen(buf);
	if (phba->bucket_type != LPFC_NO_BUCKET) {
		for (i = 0; i < LPFC_MAX_BUCKET_COUNT; i++) {
			if (phba->bucket_type == LPFC_LINEAR_BUCKET)
				bucket_value = phba->bucket_base +
					phba->bucket_step * i;
			else
				bucket_value = phba->bucket_base +
				(1 << i) * phba->bucket_step;

			if (index + 10 > PAGE_SIZE)
				break;
			sprintf(&buf[index], "%08ld ", bucket_value);
			index = strlen(buf);
		}
	}
	sprintf(&buf[index], "\n");
	return strlen(buf);
}

/*
 * Sysfs attribute to control the statistical data collection.
 */
static DEVICE_ATTR(lpfc_stat_data_ctrl, S_IRUGO | S_IWUSR,
		   lpfc_stat_data_ctrl_show, lpfc_stat_data_ctrl_store);

/*
 * lpfc_drvr_stat_data: sysfs attr to get driver statistical data.
 */

/*
 * Each Bucket takes 11 characters and 1 new line + 17 bytes WWN
 * for each target.
 */
#define STAT_DATA_SIZE_PER_TARGET(NUM_BUCKETS) ((NUM_BUCKETS) * 11 + 18)
#define MAX_STAT_DATA_SIZE_PER_TARGET \
	STAT_DATA_SIZE_PER_TARGET(LPFC_MAX_BUCKET_COUNT)


/**
 * sysfs_drvr_stat_data_read - Read function for lpfc_drvr_stat_data attribute
 * @kobj: Pointer to the kernel object
 * @bin_attr: Attribute object
 * @buff: Buffer pointer
 * @off: File offset
 * @count: Buffer size
 *
 * This function is the read call back function for lpfc_drvr_stat_data
 * sysfs file. This function export the statistical data to user
 * applications.
 **/
static ssize_t
sysfs_drvr_stat_data_read(struct kobject *kobj, struct bin_attribute *bin_attr,
		char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device,
		kobj);
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	int i = 0, index = 0;
	unsigned long nport_index;
	struct lpfc_nodelist *ndlp = NULL;
	nport_index = (unsigned long)off /
		MAX_STAT_DATA_SIZE_PER_TARGET;

	if (!vport->stat_data_enabled || vport->stat_data_blocked
		|| (phba->bucket_type == LPFC_NO_BUCKET))
		return 0;

	spin_lock_irq(shost->host_lock);
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp) || !ndlp->lat_data)
			continue;

		if (nport_index > 0) {
			nport_index--;
			continue;
		}

		if ((index + MAX_STAT_DATA_SIZE_PER_TARGET)
			> count)
			break;

		if (!ndlp->lat_data)
			continue;

		/* Print the WWN */
		sprintf(&buf[index], "%02x%02x%02x%02x%02x%02x%02x%02x:",
			ndlp->nlp_portname.u.wwn[0],
			ndlp->nlp_portname.u.wwn[1],
			ndlp->nlp_portname.u.wwn[2],
			ndlp->nlp_portname.u.wwn[3],
			ndlp->nlp_portname.u.wwn[4],
			ndlp->nlp_portname.u.wwn[5],
			ndlp->nlp_portname.u.wwn[6],
			ndlp->nlp_portname.u.wwn[7]);

		index = strlen(buf);

		for (i = 0; i < LPFC_MAX_BUCKET_COUNT; i++) {
			sprintf(&buf[index], "%010u,",
				ndlp->lat_data[i].cmd_count);
			index = strlen(buf);
		}
		sprintf(&buf[index], "\n");
		index = strlen(buf);
	}
	spin_unlock_irq(shost->host_lock);
	return index;
}

static struct bin_attribute sysfs_drvr_stat_data_attr = {
	.attr = {
		.name = "lpfc_drvr_stat_data",
		.mode = S_IRUSR,
		.owner = THIS_MODULE,
	},
	.size = LPFC_MAX_TARGET * MAX_STAT_DATA_SIZE_PER_TARGET,
	.read = sysfs_drvr_stat_data_read,
	.write = NULL,
};

/*
# lpfc_link_speed: Link speed selection for initializing the Fibre Channel
# connection.
#       0  = auto select (default)
#       1  = 1 Gigabaud
#       2  = 2 Gigabaud
#       4  = 4 Gigabaud
#       8  = 8 Gigabaud
# Value range is [0,8]. Default value is 0.
*/

/**
 * lpfc_link_speed_set - Set the adapters link speed
 * @phba: lpfc_hba pointer.
 * @val: link speed value.
 *
 * Description:
 * If val is in a valid range then set the adapter's link speed field and
 * issue a lip; if the lip fails reset the link speed to the old value.
 *
 * Notes:
 * If the value is not in range log a kernel error message and return an error.
 *
 * Returns:
 * zero if val is in range and lip okay.
 * non-zero return value from lpfc_issue_lip()
 * -EINVAL val out of range
 **/
static ssize_t
lpfc_link_speed_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	int val = 0;
	int nolip = 0;
	const char *val_buf = buf;
	int err;
	uint32_t prev_val;

	if (!strncmp(buf, "nolip ", strlen("nolip "))) {
		nolip = 1;
		val_buf = &buf[strlen("nolip ")];
	}

	if (!isdigit(val_buf[0]))
		return -EINVAL;
	if (sscanf(val_buf, "%i", &val) != 1)
		return -EINVAL;

	if (((val == LINK_SPEED_1G) && !(phba->lmt & LMT_1Gb)) ||
		((val == LINK_SPEED_2G) && !(phba->lmt & LMT_2Gb)) ||
		((val == LINK_SPEED_4G) && !(phba->lmt & LMT_4Gb)) ||
		((val == LINK_SPEED_8G) && !(phba->lmt & LMT_8Gb)) ||
		((val == LINK_SPEED_10G) && !(phba->lmt & LMT_10Gb)))
		return -EINVAL;

	if ((val >= 0 && val <= 8)
		&& (LPFC_LINK_SPEED_BITMAP & (1 << val))) {
		prev_val = phba->cfg_link_speed;
		phba->cfg_link_speed = val;
		if (nolip)
			return strlen(buf);

		err = lpfc_issue_lip(lpfc_shost_from_vport(phba->pport));
		if (err) {
			phba->cfg_link_speed = prev_val;
			return -EINVAL;
		} else
			return strlen(buf);
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
		"%d:0469 lpfc_link_speed attribute cannot be set to %d, "
		"allowed range is [0, 8]\n",
		phba->brd_no, val);
	return -EINVAL;
}

static int lpfc_link_speed = 0;
module_param(lpfc_link_speed, int, 0);
MODULE_PARM_DESC(lpfc_link_speed, "Select link speed");
lpfc_param_show(link_speed)

/**
 * lpfc_link_speed_init - Set the adapters link speed
 * @phba: lpfc_hba pointer.
 * @val: link speed value.
 *
 * Description:
 * If val is in a valid range then set the adapter's link speed field.
 *
 * Notes:
 * If the value is not in range log a kernel error message, clear the link
 * speed and return an error.
 *
 * Returns:
 * zero if val saved.
 * -EINVAL val out of range
 **/
static int
lpfc_link_speed_init(struct lpfc_hba *phba, int val)
{
	if ((val >= 0 && val <= LPFC_MAX_LINK_SPEED)
		&& (LPFC_LINK_SPEED_BITMAP & (1 << val))) {
		phba->cfg_link_speed = val;
		return 0;
	}
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"0405 lpfc_link_speed attribute cannot "
			"be set to %d, allowed values are "
			"["LPFC_LINK_SPEED_STRING"]\n", val);
	phba->cfg_link_speed = 0;
	return -EINVAL;
}

static DEVICE_ATTR(lpfc_link_speed, S_IRUGO | S_IWUSR,
		lpfc_link_speed_show, lpfc_link_speed_store);

/*
# lpfc_fcp_class:  Determines FC class to use for the FCP protocol.
# Value range is [2,3]. Default value is 3.
*/
LPFC_VPORT_ATTR_R(fcp_class, 3, 2, 3,
		  "Select Fibre Channel class of service for FCP sequences");

/*
# lpfc_use_adisc: Use ADISC for FCP rediscovery instead of PLOGI. Value range
# is [0,1]. Default value is 0.
*/
LPFC_VPORT_ATTR_RW(use_adisc, 0, 0, 1,
		   "Use ADISC on rediscovery to authenticate FCP devices");

/*
# lpfc_max_scsicmpl_time: Use scsi command completion time to control I/O queue
# depth. Default value is 0. When the value of this parameter is zero the
# SCSI command completion time is not used for controlling I/O queue depth. When
# the parameter is set to a non-zero value, the I/O queue depth is controlled
# to limit the I/O completion time to the parameter value.
# The value is set in milliseconds.
*/
static int lpfc_max_scsicmpl_time;
module_param(lpfc_max_scsicmpl_time, int, 0);
MODULE_PARM_DESC(lpfc_max_scsicmpl_time,
	"Use command completion time to control queue depth");
lpfc_vport_param_show(max_scsicmpl_time);
lpfc_vport_param_init(max_scsicmpl_time, 0, 0, 60000);
static int
lpfc_max_scsicmpl_time_set(struct lpfc_vport *vport, int val)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp, *next_ndlp;

	if (val == vport->cfg_max_scsicmpl_time)
		return 0;
	if ((val < 0) || (val > 60000))
		return -EINVAL;
	vport->cfg_max_scsicmpl_time = val;

	spin_lock_irq(shost->host_lock);
	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;
		if (ndlp->nlp_state == NLP_STE_UNUSED_NODE)
			continue;
		ndlp->cmd_qdepth = LPFC_MAX_TGT_QDEPTH;
	}
	spin_unlock_irq(shost->host_lock);
	return 0;
}
lpfc_vport_param_store(max_scsicmpl_time);
static DEVICE_ATTR(lpfc_max_scsicmpl_time, S_IRUGO | S_IWUSR,
		   lpfc_max_scsicmpl_time_show,
		   lpfc_max_scsicmpl_time_store);

/*
# lpfc_ack0: Use ACK0, instead of ACK1 for class 2 acknowledgement. Value
# range is [0,1]. Default value is 0.
*/
LPFC_ATTR_R(ack0, 0, 0, 1, "Enable ACK0 support");

/*
# lpfc_cr_delay & lpfc_cr_count: Default values for I/O colaesing
# cr_delay (msec) or cr_count outstanding commands. cr_delay can take
# value [0,63]. cr_count can take value [1,255]. Default value of cr_delay
# is 0. Default value of cr_count is 1. The cr_count feature is disabled if
# cr_delay is set to 0.
*/
LPFC_ATTR_RW(cr_delay, 0, 0, 63, "A count of milliseconds after which an "
		"interrupt response is generated");

LPFC_ATTR_RW(cr_count, 1, 1, 255, "A count of I/O completions after which an "
		"interrupt response is generated");

/*
# lpfc_multi_ring_support:  Determines how many rings to spread available
# cmd/rsp IOCB entries across.
# Value range is [1,2]. Default value is 1.
*/
LPFC_ATTR_R(multi_ring_support, 1, 1, 2, "Determines number of primary "
		"SLI rings to spread IOCB entries across");

/*
# lpfc_multi_ring_rctl:  If lpfc_multi_ring_support is enabled, this
# identifies what rctl value to configure the additional ring for.
# Value range is [1,0xff]. Default value is 4 (Unsolicated Data).
*/
LPFC_ATTR_R(multi_ring_rctl, FC_UNSOL_DATA, 1,
	     255, "Identifies RCTL for additional ring configuration");

/*
# lpfc_multi_ring_type:  If lpfc_multi_ring_support is enabled, this
# identifies what type value to configure the additional ring for.
# Value range is [1,0xff]. Default value is 5 (LLC/SNAP).
*/
LPFC_ATTR_R(multi_ring_type, FC_LLC_SNAP, 1,
	     255, "Identifies TYPE for additional ring configuration");

/*
# lpfc_fdmi_on: controls FDMI support.
#       0 = no FDMI support
#       1 = support FDMI without attribute of hostname
#       2 = support FDMI with attribute of hostname
# Value range [0,2]. Default value is 0.
*/
LPFC_VPORT_ATTR_RW(fdmi_on, 0, 0, 2, "Enable FDMI support");

/*
# Specifies the maximum number of ELS cmds we can have outstanding (for
# discovery). Value range is [1,64]. Default value = 32.
*/
LPFC_VPORT_ATTR(discovery_threads, 32, 1, 64, "Maximum number of ELS commands "
		 "during discovery");

/*
# lpfc_max_luns: maximum allowed LUN.
# Value range is [0,65535]. Default value is 255.
# NOTE: The SCSI layer might probe all allowed LUN on some old targets.
*/
LPFC_VPORT_ATTR_R(max_luns, 255, 0, 65535, "Maximum allowed LUN");

/*
# lpfc_poll_tmo: .Milliseconds driver will wait between polling FCP ring.
# Value range is [1,255], default value is 10.
*/
LPFC_ATTR_RW(poll_tmo, 10, 1, 255,
	     "Milliseconds driver will wait between polling FCP ring");

/*
# lpfc_use_msi: Use MSI (Message Signaled Interrupts) in systems that
#		support this feature
#       0  = MSI disabled (default)
#       1  = MSI enabled
#       2  = MSI-X enabled
# Value range is [0,2]. Default value is 0.
*/
LPFC_ATTR_R(use_msi, 0, 0, 2, "Use Message Signaled Interrupts (1) or "
	    "MSI-X (2), if possible");

/*
# lpfc_fcp_imax: Set the maximum number of fast-path FCP interrupts per second
#
# Value range is [636,651042]. Default value is 10000.
*/
LPFC_ATTR_R(fcp_imax, LPFC_FP_DEF_IMAX, LPFC_MIM_IMAX, LPFC_DMULT_CONST,
	    "Set the maximum number of fast-path FCP interrupts per second");

/*
# lpfc_fcp_wq_count: Set the number of fast-path FCP work queues
#
# Value range is [1,31]. Default value is 4.
*/
LPFC_ATTR_R(fcp_wq_count, LPFC_FP_WQN_DEF, LPFC_FP_WQN_MIN, LPFC_FP_WQN_MAX,
	    "Set the number of fast-path FCP work queues, if possible");

/*
# lpfc_fcp_eq_count: Set the number of fast-path FCP event queues
#
# Value range is [1,7]. Default value is 1.
*/
LPFC_ATTR_R(fcp_eq_count, LPFC_FP_EQN_DEF, LPFC_FP_EQN_MIN, LPFC_FP_EQN_MAX,
	    "Set the number of fast-path FCP event queues, if possible");

/*
# lpfc_enable_hba_reset: Allow or prevent HBA resets to the hardware.
#       0  = HBA resets disabled
#       1  = HBA resets enabled (default)
# Value range is [0,1]. Default value is 1.
*/
LPFC_ATTR_R(enable_hba_reset, 1, 0, 1, "Enable HBA resets from the driver.");

/*
# lpfc_enable_hba_heartbeat: Enable HBA heartbeat timer..
#       0  = HBA Heartbeat disabled
#       1  = HBA Heartbeat enabled (default)
# Value range is [0,1]. Default value is 1.
*/
LPFC_ATTR_R(enable_hba_heartbeat, 1, 0, 1, "Enable HBA Heartbeat.");

/*
# lpfc_enable_bg: Enable BlockGuard (Emulex's Implementation of T10-DIF)
#       0  = BlockGuard disabled (default)
#       1  = BlockGuard enabled
# Value range is [0,1]. Default value is 0.
*/
LPFC_ATTR_R(enable_bg, 0, 0, 1, "Enable BlockGuard Support");

/*
# lpfc_enable_fip: When set, FIP is required to start discovery. If not
# set, the driver will add an FCF record manually if the port has no
# FCF records available and start discovery.
# Value range is [0,1]. Default value is 1 (enabled)
*/
LPFC_ATTR_RW(enable_fip, 0, 0, 1, "Enable FIP Discovery");


/*
# lpfc_prot_mask: i
#	- Bit mask of host protection capabilities used to register with the
#	  SCSI mid-layer
# 	- Only meaningful if BG is turned on (lpfc_enable_bg=1).
#	- Allows you to ultimately specify which profiles to use
#	- Default will result in registering capabilities for all profiles.
#
*/
unsigned int lpfc_prot_mask =   SHOST_DIX_TYPE0_PROTECTION;

module_param(lpfc_prot_mask, uint, 0);
MODULE_PARM_DESC(lpfc_prot_mask, "host protection mask");

/*
# lpfc_prot_guard: i
#	- Bit mask of protection guard types to register with the SCSI mid-layer
# 	- Guard types are currently either 1) IP checksum 2) T10-DIF CRC
#	- Allows you to ultimately specify which profiles to use
#	- Default will result in registering capabilities for all guard types
#
*/
unsigned char lpfc_prot_guard = SHOST_DIX_GUARD_IP;
module_param(lpfc_prot_guard, byte, 0);
MODULE_PARM_DESC(lpfc_prot_guard, "host protection guard type");


/*
 * lpfc_sg_seg_cnt - Initial Maximum DMA Segment Count
 * This value can be set to values between 64 and 256. The default value is
 * 64, but may be increased to allow for larger Max I/O sizes. The scsi layer
 * will be allowed to request I/Os of sizes up to (MAX_SEG_COUNT * SEG_SIZE).
 */
LPFC_ATTR_R(sg_seg_cnt, LPFC_DEFAULT_SG_SEG_CNT, LPFC_DEFAULT_SG_SEG_CNT,
	    LPFC_MAX_SG_SEG_CNT, "Max Scatter Gather Segment Count");

LPFC_ATTR_R(prot_sg_seg_cnt, LPFC_DEFAULT_PROT_SG_SEG_CNT,
		LPFC_DEFAULT_PROT_SG_SEG_CNT, LPFC_MAX_PROT_SG_SEG_CNT,
		"Max Protection Scatter Gather Segment Count");

struct device_attribute *lpfc_hba_attrs[] = {
	&dev_attr_bg_info,
	&dev_attr_bg_guard_err,
	&dev_attr_bg_apptag_err,
	&dev_attr_bg_reftag_err,
	&dev_attr_info,
	&dev_attr_serialnum,
	&dev_attr_modeldesc,
	&dev_attr_modelname,
	&dev_attr_programtype,
	&dev_attr_portnum,
	&dev_attr_fwrev,
	&dev_attr_hdw,
	&dev_attr_option_rom_version,
	&dev_attr_link_state,
	&dev_attr_num_discovered_ports,
	&dev_attr_menlo_mgmt_mode,
	&dev_attr_lpfc_drvr_version,
	&dev_attr_lpfc_temp_sensor,
	&dev_attr_lpfc_log_verbose,
	&dev_attr_lpfc_lun_queue_depth,
	&dev_attr_lpfc_hba_queue_depth,
	&dev_attr_lpfc_peer_port_login,
	&dev_attr_lpfc_nodev_tmo,
	&dev_attr_lpfc_devloss_tmo,
	&dev_attr_lpfc_enable_fip,
	&dev_attr_lpfc_fcp_class,
	&dev_attr_lpfc_use_adisc,
	&dev_attr_lpfc_ack0,
	&dev_attr_lpfc_topology,
	&dev_attr_lpfc_scan_down,
	&dev_attr_lpfc_link_speed,
	&dev_attr_lpfc_cr_delay,
	&dev_attr_lpfc_cr_count,
	&dev_attr_lpfc_multi_ring_support,
	&dev_attr_lpfc_multi_ring_rctl,
	&dev_attr_lpfc_multi_ring_type,
	&dev_attr_lpfc_fdmi_on,
	&dev_attr_lpfc_max_luns,
	&dev_attr_lpfc_enable_npiv,
	&dev_attr_nport_evt_cnt,
	&dev_attr_board_mode,
	&dev_attr_max_vpi,
	&dev_attr_used_vpi,
	&dev_attr_max_rpi,
	&dev_attr_used_rpi,
	&dev_attr_max_xri,
	&dev_attr_used_xri,
	&dev_attr_npiv_info,
	&dev_attr_issue_reset,
	&dev_attr_lpfc_poll,
	&dev_attr_lpfc_poll_tmo,
	&dev_attr_lpfc_use_msi,
	&dev_attr_lpfc_fcp_imax,
	&dev_attr_lpfc_fcp_wq_count,
	&dev_attr_lpfc_fcp_eq_count,
	&dev_attr_lpfc_enable_bg,
	&dev_attr_lpfc_soft_wwnn,
	&dev_attr_lpfc_soft_wwpn,
	&dev_attr_lpfc_soft_wwn_enable,
	&dev_attr_lpfc_enable_hba_reset,
	&dev_attr_lpfc_enable_hba_heartbeat,
	&dev_attr_lpfc_sg_seg_cnt,
	&dev_attr_lpfc_max_scsicmpl_time,
	&dev_attr_lpfc_stat_data_ctrl,
	&dev_attr_lpfc_prot_sg_seg_cnt,
	NULL,
};

struct device_attribute *lpfc_vport_attrs[] = {
	&dev_attr_info,
	&dev_attr_link_state,
	&dev_attr_num_discovered_ports,
	&dev_attr_lpfc_drvr_version,
	&dev_attr_lpfc_log_verbose,
	&dev_attr_lpfc_lun_queue_depth,
	&dev_attr_lpfc_nodev_tmo,
	&dev_attr_lpfc_devloss_tmo,
	&dev_attr_lpfc_enable_fip,
	&dev_attr_lpfc_hba_queue_depth,
	&dev_attr_lpfc_peer_port_login,
	&dev_attr_lpfc_restrict_login,
	&dev_attr_lpfc_fcp_class,
	&dev_attr_lpfc_use_adisc,
	&dev_attr_lpfc_fdmi_on,
	&dev_attr_lpfc_max_luns,
	&dev_attr_nport_evt_cnt,
	&dev_attr_npiv_info,
	&dev_attr_lpfc_enable_da_id,
	&dev_attr_lpfc_max_scsicmpl_time,
	&dev_attr_lpfc_stat_data_ctrl,
	&dev_attr_lpfc_static_vport,
	NULL,
};

/**
 * sysfs_ctlreg_write - Write method for writing to ctlreg
 * @kobj: kernel kobject that contains the kernel class device.
 * @bin_attr: kernel attributes passed to us.
 * @buf: contains the data to be written to the adapter IOREG space.
 * @off: offset into buffer to beginning of data.
 * @count: bytes to transfer.
 *
 * Description:
 * Accessed via /sys/class/scsi_host/hostxxx/ctlreg.
 * Uses the adapter io control registers to send buf contents to the adapter.
 *
 * Returns:
 * -ERANGE off and count combo out of range
 * -EINVAL off, count or buff address invalid
 * -EPERM adapter is offline
 * value of count, buf contents written
 **/
static ssize_t
sysfs_ctlreg_write(struct kobject *kobj, struct bin_attribute *bin_attr,
		   char *buf, loff_t off, size_t count)
{
	size_t buf_off;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	if (phba->sli_rev >= LPFC_SLI_REV4)
		return -EPERM;

	if ((off + count) > FF_REG_AREA_SIZE)
		return -ERANGE;

	if (count == 0) return 0;

	if (off % 4 || count % 4 || (unsigned long)buf % 4)
		return -EINVAL;

	if (!(vport->fc_flag & FC_OFFLINE_MODE)) {
		return -EPERM;
	}

	spin_lock_irq(&phba->hbalock);
	for (buf_off = 0; buf_off < count; buf_off += sizeof(uint32_t))
		writel(*((uint32_t *)(buf + buf_off)),
		       phba->ctrl_regs_memmap_p + off + buf_off);

	spin_unlock_irq(&phba->hbalock);

	return count;
}

/**
 * sysfs_ctlreg_read - Read method for reading from ctlreg
 * @kobj: kernel kobject that contains the kernel class device.
 * @bin_attr: kernel attributes passed to us.
 * @buf: if succesful contains the data from the adapter IOREG space.
 * @off: offset into buffer to beginning of data.
 * @count: bytes to transfer.
 *
 * Description:
 * Accessed via /sys/class/scsi_host/hostxxx/ctlreg.
 * Uses the adapter io control registers to read data into buf.
 *
 * Returns:
 * -ERANGE off and count combo out of range
 * -EINVAL off, count or buff address invalid
 * value of count, buf contents read
 **/
static ssize_t
sysfs_ctlreg_read(struct kobject *kobj, struct bin_attribute *bin_attr,
		  char *buf, loff_t off, size_t count)
{
	size_t buf_off;
	uint32_t * tmp_ptr;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	if (phba->sli_rev >= LPFC_SLI_REV4)
		return -EPERM;

	if (off > FF_REG_AREA_SIZE)
		return -ERANGE;

	if ((off + count) > FF_REG_AREA_SIZE)
		count = FF_REG_AREA_SIZE - off;

	if (count == 0) return 0;

	if (off % 4 || count % 4 || (unsigned long)buf % 4)
		return -EINVAL;

	spin_lock_irq(&phba->hbalock);

	for (buf_off = 0; buf_off < count; buf_off += sizeof(uint32_t)) {
		tmp_ptr = (uint32_t *)(buf + buf_off);
		*tmp_ptr = readl(phba->ctrl_regs_memmap_p + off + buf_off);
	}

	spin_unlock_irq(&phba->hbalock);

	return count;
}

static struct bin_attribute sysfs_ctlreg_attr = {
	.attr = {
		.name = "ctlreg",
		.mode = S_IRUSR | S_IWUSR,
	},
	.size = 256,
	.read = sysfs_ctlreg_read,
	.write = sysfs_ctlreg_write,
};

/**
 * sysfs_mbox_idle - frees the sysfs mailbox
 * @phba: lpfc_hba pointer
 **/
static void
sysfs_mbox_idle(struct lpfc_hba *phba)
{
	phba->sysfs_mbox.state = SMBOX_IDLE;
	phba->sysfs_mbox.offset = 0;

	if (phba->sysfs_mbox.mbox) {
		mempool_free(phba->sysfs_mbox.mbox,
			     phba->mbox_mem_pool);
		phba->sysfs_mbox.mbox = NULL;
	}
}

/**
 * sysfs_mbox_write - Write method for writing information via mbox
 * @kobj: kernel kobject that contains the kernel class device.
 * @bin_attr: kernel attributes passed to us.
 * @buf: contains the data to be written to sysfs mbox.
 * @off: offset into buffer to beginning of data.
 * @count: bytes to transfer.
 *
 * Description:
 * Accessed via /sys/class/scsi_host/hostxxx/mbox.
 * Uses the sysfs mbox to send buf contents to the adapter.
 *
 * Returns:
 * -ERANGE off and count combo out of range
 * -EINVAL off, count or buff address invalid
 * zero if count is zero
 * -EPERM adapter is offline
 * -ENOMEM failed to allocate memory for the mail box
 * -EAGAIN offset, state or mbox is NULL
 * count number of bytes transferred
 **/
static ssize_t
sysfs_mbox_write(struct kobject *kobj, struct bin_attribute *bin_attr,
		 char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfcMboxq  *mbox = NULL;

	if ((count + off) > MAILBOX_CMD_SIZE)
		return -ERANGE;

	if (off % 4 ||  count % 4 || (unsigned long)buf % 4)
		return -EINVAL;

	if (count == 0)
		return 0;

	if (off == 0) {
		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (!mbox)
			return -ENOMEM;
		memset(mbox, 0, sizeof (LPFC_MBOXQ_t));
	}

	spin_lock_irq(&phba->hbalock);

	if (off == 0) {
		if (phba->sysfs_mbox.mbox)
			mempool_free(mbox, phba->mbox_mem_pool);
		else
			phba->sysfs_mbox.mbox = mbox;
		phba->sysfs_mbox.state = SMBOX_WRITING;
	} else {
		if (phba->sysfs_mbox.state  != SMBOX_WRITING ||
		    phba->sysfs_mbox.offset != off           ||
		    phba->sysfs_mbox.mbox   == NULL) {
			sysfs_mbox_idle(phba);
			spin_unlock_irq(&phba->hbalock);
			return -EAGAIN;
		}
	}

	memcpy((uint8_t *) &phba->sysfs_mbox.mbox->u.mb + off,
	       buf, count);

	phba->sysfs_mbox.offset = off + count;

	spin_unlock_irq(&phba->hbalock);

	return count;
}

/**
 * sysfs_mbox_read - Read method for reading information via mbox
 * @kobj: kernel kobject that contains the kernel class device.
 * @bin_attr: kernel attributes passed to us.
 * @buf: contains the data to be read from sysfs mbox.
 * @off: offset into buffer to beginning of data.
 * @count: bytes to transfer.
 *
 * Description:
 * Accessed via /sys/class/scsi_host/hostxxx/mbox.
 * Uses the sysfs mbox to receive data from to the adapter.
 *
 * Returns:
 * -ERANGE off greater than mailbox command size
 * -EINVAL off, count or buff address invalid
 * zero if off and count are zero
 * -EACCES adapter over temp
 * -EPERM garbage can value to catch a multitude of errors
 * -EAGAIN management IO not permitted, state or off error
 * -ETIME mailbox timeout
 * -ENODEV mailbox error****count number of bytes transferred
 **/
static ssize_t
sysfs_mbox_read(struct kobject *vice, inux Debin_attribute *  *****F,
		char *buf, loff_t off, is pa*******)
{
	       deer f *dev = container_of(er for   All * Copy, Copy); All ri
 Scsi_Host  *sh* EM= class_to_EX an(deved.  All r lpfc_vportChan    = L       Emulex. lex.) adema->demadataf Emulex Emulexhbal rip           ->    ;    t rc;
	MAILBOX_t *pmb;

	if (off > yright (CMD_SIZE)
		return -ERANGE4-2005 C(      +ex.costoph Hellwigw.emulex.c      =                  -            hris% 4 ||       *oftware(unsigned long)bu soft         ****INVALam is free s&&; you c== 0
 * modify 0    spin_lock_irq(&     *       )        ublic over_temp_is feon 2HBA_OVER_TEMP) {
		****the    idleishedof Eral   unmodify itPshed bLicense as* modify  -EACCES;
	}m is free sn 2  &&
	enseshed b* This pro. San rLIEDSMellwWRITINGCONDITIONS, REPRESENTAES, offset >= 2       of(uint32_t)odify pmb =e that iING ANY IMP pro->u.004-		switch (pmb->mbxCommandodify 	/* Offline only */ensease MBX_INIT_LINK:XCEPT TO THEDOWNENT THAT SUCH DISCCONFIGERS ARE HELDify itTO BE LRINGRE HELD *
 * ING Te GNU Genebute thaUNREG_LOGINLY INVALID.  SLEAR_LARE HELD *
 * LUMPCH DIEXT for  *
 * moiUN_DIAGSING  *
 * inclc TARYINGdify itin it wMASARE HELD *
 * it wDEBUe for     !(       fc_flag & FC_OFFLINE_MODEITY,  *		printk(KERN_WARNWARR" proglex :ENT, AR 0x%x "     ES,    "is illegal in on-n thAS AND\n"st Bux/delay.NF GNUEM.h>
#inESE, D.  Sh    rogram    dis Fibibre d <liis phope that it will be u useful. ** PERMs  *
}AT SUCH DISC>
#iE_NVRE HELD *
 * h>
#inVPARMth ti/scsackagLOAD_SMh this package.ADnclude <de </scslpfc_See th    hw.h"tran"lpfcR "     sli"
#include " "STATU_fc.h>

#inclue.AD_XRIi4.h"
#include "lplE     .h"
#include "LNK    nbe founsi_devicefiMEMORYbe found in thAIMEROADlex.  h>
#imorPDATE_CFli4.h"
#includKILL_BOARn"
#include "l"lpfcLLY include "lpfc"lpfc_XP_ROsi.h"4"
#incluBEACO, a copy****whDEL_LD_ENTinclude "lpfcpfit wVARIABLEpfc_hw.h"
#ini_****WWefine LPFC_MINPORT_CAPABILITIE_fc.h>

#incluPEED IOVile CROL     breakludeMO 1
#define LP    64cs"
#include "lpfcpvport.h"

#def"

/**
 "0, 1, 2, 4, 8"
etailsefine LPFC_MINiiC) 2verii - Hex to ascSee the_BIT             lpfcBIUed wi; you cg hol/
include "l<l    /ctype I "lpfc_.i_tran* @hd "lpfc_hinteginterruptng tinclude "lO 255

#dng terminato_hw.h5

#de* Copy Device Engineeri55

#deos
 * Jice Enginedefault e <sng holdto a JEDECscsi_dteger pluUnknownto aterminator.
includDescription:>
#iJEDEC JotionElectron D CopyrEngineento aCouncilthen Cascii
 a 32 bit charactecomposed****8RESSinclIfe Fo eng holered aotes:
* atten wit, allow_verD,_log*
 ** or e Emulestring hoch>
#ins untilevic      s restai
 *.is fiEXCEincls di b      *    pedRANTIIES, Ierrupt.h>

#in !=fc_slilogmsg"
#i		j = (incr & 0xf);
	005 Cj <= 9)id
     i] = 0x30 +  j;
		 else
			hdw[7 efines    _ 0x61 + j - 10	 els30 += 0x30 +>> 4		 eWWlay. ITIONc_jedef
			i = 0, ing converte, LOG_TIONSrted 	"1259  pro: Issued_jedece trmded inte	"lpfc_wh0x00<lii++   *
S AND.onverted h>
#on:
 * JEDEC Jo#incitalFOR A PARTICULAR Pww.emulex.c  rn co/* Don'tgers onss unusedascii(intovporsent when bxt.
edis filevo fileinr, chmiddlefor discy thyi, j;
	ex. (i = 0; 4.h"sli string LPFC_BLOCK_MGMTMAPng hold 9.
 * Hex 0-9 becomes ascii '0' to '9'.
 * Hex a-f becomes ascii '='9'.
GAIN' capituted fc_jedec_toevice *dev, struct devicware= 0x30(! Copy
 * Fibre Ch * Fst B  SLI_ACTIVE)***** ascii '0' to '9'.
 * Hex a-f becomes ascc = www.ebre iv: cs pro(i = 0;used.
	, INCLUDING ANY IMP pro      ) sh_BIT_BLLes ascii '0 the hope that it will be up	} else     re SLI are trshostrks 		 einux De                  = Linux De_wait            _hbaescripst->ns &    _SLI3_BG_ENABLshow( progtmo_valinux/ctterrupt.h>

#inc * HZ =      ->phba4-2005 Crted->cfg_enablg_inftionswrc			hdw[7SUALL Sevice *(ptersP=	hdw[7TIMEOUTevice *detains the module desc= NULLfc_hw.h"
    *
 sn*****fGuard DAGE     , 
		  MODULE_DESC "\n");
}
his file is prd Disabled     char? -E     :t device_e *deile is part     bg_S AND cense  *    ruptXPRE	 elset * Copy_atNG ANY IMP     WA!     
 * FibINCLUDING ANY IMPS AND  !n snprintf(blpf******
g holding converted* character sBad Somposede <scucsi/scsi.h>
#include <sc/scsi_device.h>
#include <scsi/scsieful. * lpfc_vporpitamemcpy(ptersCHANT8(C) ) &9'.
har *, text.
 *
 INCLUto coa;
	struct lpf_     +evice ble_r (i = 0; 
 * EM*ons & (C)le usefulodify itsi/sc   d_err_cnt  char *buf)
{  *
  i_device.h>
#include <scsi/sc
scii '='s & LPF}
i/scf0x00
 * modify itFibre Cshost->hostu\n"    *
.ngit andph	.nam     inte strinmodurn s_IRUSR | parWUSR,
	},
	.               fc_vportr_shctyp =stribute it ctypstruwri     shost->hostd			c,
};to_asfor 3rn snrs c_shost-a->bg- Creast *she ctlreg 05 Cguardentriesst *@host : addrestructhosttext.
vice_ature.
 _vporRdevice *des_to_szero on succeinux Deex. 3 device *de fromAdapte)cC_SLI_vicePAGE()fc_vpornt
t *vport = _options &evice *dt *vp      turn s_SIZE, "%lluevice *dEMLademad S		retudema_e_attasciiLinux ice  witst *s

	ulex. 3 Adapte)n"st B	stribute Publin snpled\nev.mulexrted integi&shost-drvreS ANith vons &4-20r/* Virtuo_show(s do not need ctrl_LED)
			returstru    printf||ct device	hdwtyp redi
	strNPIVacte*
 * goto ouLPFC_d to a    *
rr_cninux Duct lp* @ * F:host->h32 bssize_,rom lort *)ice_b      ) 2004-s is p****f    rintf(bu_removtwarre    FwLinux De* Copyright,, struchost->host  char *buf)
{
	_show -us Adapte     e it an lpfc_vpophba->bg_reftag_err_c LPFCinux De   *ns t device0;
*******in ted sf)
{
de:
 conta-er in auct device_attribute *attr,
	   serialnuscii
 * @de_Htions & LPFC_S: classtruct device_attribute *attr,
	       char *buf)ux Duf)
{
	uf:ba;
class c Scsi_Hooues i- Return JEDEC}*
 * EMort *vpfreerial number inRr in 3_BG_ENABLED)
			return snprintf(bAGE_SIZE, "Blockre.
bled\n");
		else
			re*/
void)lse
	GE_SIZE, "%s\n"hostdata_asci*ruct Srintf(buf - R deviceome pci rint aboutevice.hsmes  Scsi_host st (stuct ial*******then terhostdas:st *host =orm2 bibuf)
{
	
 **/
static ssize_t
lc_log->SerialhostdSIZE,c_vport *)()uf, PAGE_SIZE, "%s\n"ow -  into uf)
 This file is part     rn somt device *dev, struct device_att"%s\n",lse
		Serrn snpnum shost->hostct device_attribute *attr,
	       char *buf)
{
	
 **/
static ssize}
contst *Dynamic FC rigEMA\n",
			s Sui < 8
E, "uct device *dget_phba s conidrks opyux De* RetuDID chaoux De2 bi nfo abtrini lpfc@devic: kernelor_show - r onh a  LPFC_Sf, PAGELI3_BG_ENA ->phbar one= vp   *
 * ata;
	struct lpfshow - Return s	struct Scsi_Host *s;
}

/**
 * lpfc_om	struct Scsi_Host_sensnote: ice y, "%alctypyrsiocpu endiannBlocstruTIONS "%s\n",lpfc Scsi_Host *shoeturn snprintf(b* Copyrig, struct devicverte- S	retlpfcaluux Dephba->Seriallass coverterted to a Scsi_host stru Freefc_vostruon snprintf(buf, PAGE_SIZE, "%s\n",vertfc_info(host));
}


/**
  lpfc_serialnuon return cont_vport *vport = _options & LPFC_SLI3_BG_EN{
	struct Scsi_Host *shtruct device *dPot lpfc_rn some purns: size rted\able_inclv: class converted to a Scsi_host sshostd\n");
		else

	retcsi.hba FCe_t
lTYPEe is inux    i size ring.s_link_upncludeeldescr (i = 0; _shoopologysnprTOPOLOGY_LOOrn some class convertrt *vport =PUBLIC* @attrce * the model description of the hba
 * @e_ger t

#dee *aScsi_Host  *shost = class_to_shost(devdev, s *rted = gte *at	elseBG_ENABLED)
			return snFABRIC Scsi_Host  *shost = class_to_shost(devZE, "%s\n",phbat Supported\n")
 * @attr: device attribute, not uPTP LPFC_Sar *buesct *) shost->hostdata;
	struct lpfc_hba UNKNOWN hba
 * @dev: s\n",pE, "%s\n",phba->Serrted\ncsi_host structure.
 * S AND , not uHost
 * @buow - Return tup    )S AND
/**
 * lpfc_modeldesc_show - Return the model description of the hba
 * @structLI are ascii
 * ct device *dev, struct device_attribute *attr,
	       char *buf)
{
	
 **/
static ssize_t
leviceormatvpd model uct ted wituf, PAGE_SIZE, "%s\n",phba->Serialct lpfc_vport *vport = _programtype_ used.
 * @buf: ute, no PAGE_SIZE, "%sh"
#E_program;E, "Blo"%d\nR NON-INage.  AdapS ANDtruct SUCH atic sT T>phba;

RE HELD *tribute *ac_vest *sh->hostdatis psi.h>
#ir plu This phbalpfcevicc_ve

#deefSCLA
		  LINtribute *atP      char *buich canvport.h"

 to a Fou
 * mposeis pLinks up, beyondc.h>
r one devic* CoRetur AND strucst->hostdatains the scsi vpd program type.@N (stlasE, "%s\n",phba->Seriaype_ERROR lpfc_hba   *phba = vport->phba;
 the scsi vp.     tions & LPFC_S nibblc_senteturn contains the scsi vpd program type.phba;

	rons & LPFC_Size_tel namdevic, PAGE_SIZE, "%s\n",phba->ModelName);
}

/This filsvportfc_modelname_show(struct device *ce sl
/**
 * lpfc_modeldesc_show - Return the model description of the hba a Scso_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snprintf(buf, PAGE_SIZE, "%s\n",phba->ModelNato a Si_host structure.
 * 
	stru*
 * EMULE Ada a ScC_SLI3_c_si.hA_1GHZERS ARE Ht Supporteclass ccsi vpd program    *p1GBIss cons: size oture.
 * 2tr,
		    char *buf)
{
	struct Scsi_Host *shost = 2ame of the hba
 * @dev:  *p4ba = vport->phba;

	return snprintf(buf, PAGE_SIZE4 "%d\n",
		(phba->sli.sli_f8ba = vport->phba;

	return snprintf(buf, PAGE_SIZE8 "%d\n",
		(phba->sli.sli_f10ba = vport->phba;

	return snprintf(buf, PAGE_SIZEt0ame of the hba
 * @  *phba = vphba;

	return snprintf(buf, PAGE_SIZE not used.
 * @buPi.h>
#T   char *buf)
{
lomgmt_show - Return the Menlo Maintepe);
}

/**
 * lpfc_mlomgmt_show - Return the Menlo Maintenanfabric_host-fc_modelname_show(struct device *programhost
/**
 * lpfc_modeldesc_show - Return the model description of the hbareftag_err_cno_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	return snpr	u64 nodeions printf(buf, PAGE_SIZE, "%s\n",phba->ModelNaruct lpfc_vpor
	ret;
}
 the pfc_ibre C*
 * EMULEX attribute, not used.
 * @ANTIES, *attr,
		    char *buf)
{
	struct , PAwt device port ne tru64*
 * EMULEfabparam.t deN
}

u.wwn - R  char /* snprintis locow - Ref)
{theres Adno F/FL_riptio
	fo_host strufwfor  *
   uf, PAGE_SIZE, "%s\n",phba->Seled\n");
	te, not use          t device *dtted string.
 **/
sf, Psuct lpf*
hdw[fistieriarintrma= vpoc_hbodelnaadaptruct *
 * lpfc_modeldesc_show - Return theo MaNotconvertrr_sed\nprintfex.  Ada down,ted, vporpool,attr2 active,o MamanagemThisthe  sizemulememoryeersic_decod_prog,_drva = vpfor 3ba->sli.sli_f"%s* lpf-%dv, p_programti_Host  *shredi_rev(ph;
	strufwrev[32nux vport PAGE_      n cont(stta;
	stru *, PAGE_SIZEppor: device attribute, not used.
 * @buPev, (struct lpfc_vpord.
 * shost->hostdatc_logmrmredi rev run The _device.st = cl

	ret_shost(dev);st structureslerial, strFITNtains liBus Adhdw[pfc_vport *) shost->hsthe scsi vpd pinux s = class_t9];
	les inucevicsor: det st>E, "Bloc_uct lp
 *  to a ScsQattr200oxqhts h Hellwit->ph4-	 Fibre Cit an sectruc(st wits  LPFC_SL/*
 filprevThisuHelle_attsnpringass unusedted strinut ton_shcture.s.biuRconfigur "%s,E, "%s\n"vice *dev, struc <stdata;
	struct ssize_t
!vice * progmem_ascilpfc_temp_s)
{
	stri_host structure.
 * @attr: wared progranse li-%hoC_SLI3_BG_ENA  char *buf)
{
	stra Scsi_host stttr,
	       char orted\ =atioasciport =	structttr, char *bu, GFP_ing Es con_progs.
 *
 FCte, ted smp_se
	resetINFRoxq,2 of theof (class converascii
 ** Host *
 * PURPOSE, +  j;
		 else
		ss_t7 -fc_slinl.csi_host stOev_sa S_verHOS,phbuf)
{
	st->pext1shost- ssiuf)
{
	sed withostdatcharhost->hostdata;
	struct lIZE, "%s\n",lpfc_in*buf");
	_host structure.
 * @attr: de*) sD)
			return snprintf(bt device_aa;
	r*
 * Ret con  char ED    csi_host structure.
 * OMVelpfcnstatic
 * EMULEratov * 2matted stct device_at - R		"Bloc device *dev,      cha * @_SIZE, "GE_SIZE, "%s\",phba->ModelName);truct deviceport = ta;
	ste schsf(buf, PAGE_Struct lpfc_vport *) shost,
		  hs->tx_frameerte
	strun.varRdrtedus.xmitFblesCrks ribu defwordertestruINscsi/scclass alNumbBythostcii
56attribu rnibbles o, stru<scsi/sc **/
starcvPAGE_SIZE, "%s\r_show - Return the Menlo Maintercvcsi_host strulinont *) shost->hostdata;
	struct lpfc_hba  ati_host structure.
 * @attr: pfc"
#invpd program type.
 *
 * RetSLI3_BG_ENABLED)
			return snprintf(buct Scsi_Host  *shost = class_to_shost(devme.
 *
 * Ret convhar *buf to a Scsi_host structure.
 * @attr: devicsed.
of fo->Od witRf the port
 * @ct lpfc_vporis f retureturn containsar * ase Ld progra    *attr, char *buf)
{
	ch
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = veturrn snpbThe sport->hba_sta Adapfailureucturt->hoselName);
}
Lnk. AdaF Down\SIZE, "%s\loss_of_sync    c		efine;
	cT TOe of foossSync
		len +Cch can :
	ignf, P
		   FouREADY:
		len +=(dev)IZE-lSIZE, "%s\prim_seqstrutocolhoste LPFC_HBA_READY:
		len +=e) {SeqErclass  "%s\invalid_*host =e LPFC_HBA_READY:
		len +=     -lXmitWordnct device_a-lecr
		len +nk Up - ");

		switcr, stru + le_show_show(struct device *deFC_me_sho_CFnructur"evicP:
	cae LPFC_-=host	case LPFC_BUILD_DISuf + len, PAGE_SY:
		len +_LIST);

en += snprintf(buCFG_LINLPFC_FDISC:
		cn, "Li		"Discovery\n");");
			breartcase L   *
 Y:
		len +=OCAL_CFG_L + len, = snprintf(buf + len, PAGE_scovery\n");
			bucture.C lpf + len, PFC_VPORT_FAILED:
			
		  F
 * );

Y:
		len +F	switch (vev, st:
		len +AGE_ST THn, "Failed\ + len, NKNOWN:
			lostdata;
	struvice attribute, not used.
 * @buf:
		caspe LPFC_HBevice_a - ,ass_Tagfc_d1ice *READ	}ModelDes + len, PY:
	_flagdec_O_MAInose LPFC_HB-1 char *bufHBA_READNT)
			lenhbcturecase LPFC_VPORT_Fa->ttribut********ributeENL, PAGy\n");
						break;

		c				"DisORT_FAILdumped_show(struture
	 LPFjed = devic, PAGE(Return tLPFC_FDI< 		  WARvporchartioneport RT_FAIL_since_larn snprintfLPFC_FDI+ba = Linuba->vpd;

	d-1 -ILD_DISC	
		iucturl
	case LPFflag & FC_PUBLIC_LOOPcase LPFC_VPORT_FAint M)
		i  *
 delD vpd prled\n");
		else
			len += snprintf(buf +on returnhce sl        NKNOW deviSupportediithen te	       INIT_upport
	e LPFe hb
/**
 * lpfc_modeldesc_show - Return the model description oVPORT_FAILElpfc_mlomgmt_show - Return the Menlo MaintenanScsi_host struhdwt = class_to_shost(dev);
	struct lpfc_vpor  char *buf)
{si_host structustruct lpfc_vpre.
 * @attr: devvice attribute, nt Scsi_Host  *sa->link_state) {
	case LPFC_LINK_UNKNOWN:
Supported\n
	retuvpd(C) vp = &PAGE_SIZEvp->rata;
	struct lpfc_hba   *phba = vporROMfo abption_romnto s
/**
 * lpfc_mlomgmt_show - Return the Menlo MaintenanScsi_host struod wit_romct dthe model description oSIZE, "%s\n",lpfc_info(host));
}
to a Sci_host structure.
 * @attr: devvice attribute, not used.
 * @buf: olass conT TOs[0]rks x1; /*[])
et requetruc/ba   *phba = vport->phba;
	int  len = 0;

	switch (phba->link_state) {
	case LPFC_LINK_UNKNOWN:
erted to a Scsi_host structure.
 * @attr: device LPFC_INIT_MBX_CMDS:
	case LPFC_LINK_DOWN:
	case LPFC_HBA_ERROR:
		if (phba->hba_flag & LINK_DISABLED)
			len += snprintf(buf + len, PAGE_SIZE-len,
				"Link Down - User disabled\n");
		else
			len += snprintf(buf + len, PAba->hba_flag &d.
 * @buf: on return coscii
 * @dvport->fc_map_cnt + vport->fc_unmap_cnt);
}

/**
 * lpfc_issue_lip - Misnomer, name carriertion"Disvice_

 OR NON-ININIT_ar *ben += snprY:
		len += NK_i vpd m snprintf(bufWARM_STARen, Y:
		len + EXTElpfc_vport *vport = (st THECMDSvport *vport t ScsLAIMvport *vport  Fouport KNOWelDesc)ac LicOPOLOGY_Lpfc_hISEVLOow - R>fc_flag & FC_PUBLIC_LOOPcase LPFC_VPORT_FAILEse LPFC_BU - Userludecsi_Host *s	 LINK_DISABLED) -Enclu& LIN    ISCLAIr Return  snprintf(_LOOP)
 * _AU_HBA_READY:
		len += ScsiPvport *vporrt->fc_flag &  -"Disc
	ink Up - ");

		switch (vFC_PUBLICort *vport VPEED p - ");
 LPFUp -    c
, OR NON-I, name c	hdw[rn -EN "Ready
		} elseHBA_REA
	T_UNKNOWBXERR_ERROR;

	if ((vport->f *)pmboxq, 0, si	"Discoverfc_oto aevicmb.mbxOwner = OW, "Failed\wner = OWN_ "Failed\n")ailsKNOWN:
			le LPFC_NS_QRGox_wait(phba, pmbe Emq PURmb.mbxStatus n 2 o||}

/*FC_PUBLIC_Lhost = cR_LI_to_shost(de    *
			} else {
			if PFC_VPORT_FAILED	"_link(phba,mtypss_to
		} else {
			if (fc_flag & FC_P LPFC_LI**/Thi.h>
#idriDEC 
	str attri= TO handlev, as targett devatioset(sba_flre LPFare devshost-f, P(: oncturOXQ_i4.h_v, s = 0ylpfce *dev,mule->phba Sbyhe Emq,
and
 us Aa->te a lirn sr 32pUT)
		o MainUT)
		ev,
			       UT)
		m, struct device device *devtribute *attr,
mbox qoopbihba-un_vpor struUT)
		mthe urn -E-*/
static ssize_t

		brox_mem_ph>hostdata
 BX_\n");
}    tesh a Npfc_.
 * @tm*.mbxCifshost->hostdata;
	struuct lpfc_hks o****demar, LPFC_->32 bp
statstr             ruct Scsi_Host *shost = class_to_shost(dev);
	struct l for the portf)
{
pbac:ndlpprintf(buf, PAGE_SIZE, "%s\n",phba->M: clSearchshow(s de, ma   *,s a*****IDevice@phb_for_each_vpory( cla, &mestatic smboxs, nlp_@phb_hba ce_attNLP_CHK_for 3Scw[])tu len, port  cla-> ssi *vportresizeSTE_MAPPEDGE_SI[8] = 0;
	* AssumesidsnprpostPAGE_SIipfc_hba   
}

/**
 * lpfc_mlomgmt_show - Re ascii '=' claout
lpfcyed);
pbac, HANTABILsi vpcked
 * -ENOcen, PAGE_SIZEe_lip - Misnomer, namect dedevice_asc_modelnaO THERR_E conv con_ascidlpn, PAor -1_**
 , LPFC_ (mbxE-leERR_LI	init_coort      n snprintf(buf, PAGE_SIZ sucrtioncnent()pe:_LOOP)EVT********ttribut_comruct lpf     BOX_Tbuff= clac_hba   NKNOpbyte(buf,  @r plonlinen = 0;
*********PREP_flawonlinend
     ?eturn -Linu, PA:, pmbpsliFFLINE_ent(phba_OFFLINE_host struFFLINE_i4-200nit_pfc_ven = 0;

	rkq_post_event(phba, &status, &online_   *minated) {
:.
	 */truct device_acurrpion(&onISCL_c   *
 	mslwwnworkport;
			omplt = class_it_for_completiont device 
	f, Pkq_posecs e witp(10);
			ecs orkq_postent(phb!= 0        *
 * IOOWN_ enoped
BOXQ_tenoug>rev Wad;

 stri_FAILED/

#ingsstruste, nle dohdw_show(stffline(strmboxhostst->hos :PFC_ enough for dev loss timeou_PREPexpict lg->txcmplq_cnt66 Ou		msata;
urn ic ss; ce attri*****goffliata;
rn -[ic_vp	ttr: d(pri v, sNIT,
					"0466 OutsIT,
					"046 - * DISCL> 500) {  /* 5lpfcs->txc
	mb LPFboxq, 
			
statN_WARNINGRN_WARNING, LOG_INIT,
					"0466 Outstanding IO when "
					"bringing Adapter offline\n");
				break;
			}
		lnsoror (i = LOG_INIT,
					"0466 Outstr.
 *
workqfrom _event
stat, &ent(ph, 					"0466rt->rs con devitmo_completionic sscharucturetmlen;
}EIO resvping  reass convo ze@tHost *: newame_shoe\n")etrom l* lpurn 0;
}

/**
 * lpf RetIf rig->ph*
 * Enpportese.
 *iv*
 *dn += notT,
	Host *,te *atsempfc_ LOG_INIT,AIMEshos& LPFC_Ste *attr, char *butatic ssiletion onpe: LPFThe posti*ic ss, ute defi"0466 Ou for     ompletion		ic sses an;

	if (!p="0466 Ou
	case LPFC
					"bringent(ph;

ow -enoughprintdevic ssieturnfunce hbaempool_fr postiUT)
		m Scsi_hostlen;c_	struct ribu   phtMacr,
		ahar *eturelNIT,
gor    *
retur(10);
withp(10);ISCLba   *phwrns:
 *r thiai sucC_LINK_DOWN:##LPFC_: structattribuer
 *	returtedlpfcbuf Ret
c

	rSLIphba->lvertiophbaaneturo_off* Ret
ructed\n device *vp = nux DeOFFLINE-2005 @phba: lpfc"bringing Adshow(strur, char *bu one 	"0466 #defpfc_Returns:
 * lpfc_do_offl(LPFC_,call fc_: unc_dosz, cast)	\ return c      , "Blo		\t        IZE,t_for_cly rtus = lpfights h Hel,for_coBlocerlpfcE_SIZrelu\n",
			(hl Hosype.
tus us Arev(p)_for_co{	struct c Noloss	returt.
 *, butph				er
 s conconve* No(10);rks ofiv call IfNE);turns:
 * *
 *  + len=/
				lpfces succth of *shostsnE_SIZE not un -E*phba = vportt*
 * Not(ascih->from lpf? "selthe link _SIZE->-2005 : 0);	\t(phcsi_then terminatedrd      es !=tes:
 * The swi one() w\
compl)inated with a NIThis pll succeeds
 * return g )luen lpPAGEFC_R_BITMATTR call suhost GO,ucomplet
 * is called to,ev: c)pfc_selective)

	rect lpfymbolptions & LPFC_SLI3_66 O's f,st *h_ Outstandinss_to_s:	_evetes succcwsi_hthe scsi vpd p has beit iveng */
	fN_WARNING, LOG_INIT,Tn_sho	return i
		rll 0; /**
 *tnst chur af    hba;

	returnlpfc_vport *vporort 
	int  len = 0;

;
		else
			retre-registset_phba-ED)
			returning Adap Ret
	structohostpog  *
on ret	retut *host =
{
	schaattr,
 succcesto a Screturn se lenomplet        * Adaptersis parnlinus = lpfc_dss_to_s********si_host structure.
 * @attr: devi*shost = class_to_sh*)********->ddeturneturn the Menlo Maine event
  to aV_BITMthe mo !=if (sts_cmd one i,ata;
CTNS_RSPN_ID* Cop0a->ModelName);
}

hbae"
 _verbose_ini	memnot hba'Adapg a->Modellevel@attr   *: PReturnGE_SIct devic) 2004IZE, "Blte) {
	case LPFC_LINK_UNKNOWN:
ringing AcfgPAGE_nst outpfc_ted
 * -ENHBA_Eodul Adapte,phba->ModelN *host =_evt_ cfgPFC_SLI3_BG_EXQ_tuce slseiv: cog messsage accordev, T,
				options fi& LPFC_SLI3_BG_EPAGE_e);
	setine( RetbeforeMain****hethe  {
		st str     us;
}

/**
 * lpfc_ns\n",phba->ModelName*shost = clasot uct des:
 online_a->Modelss dvice *a;
	int  len = 0=, char *, structe, not ruct lpfe Frel  *
, namort->phbanruct lpft((v{ice_afix tionturnlpfc_	stru If lps cur])
{st =.d lon");
	_to_shost(de1ort = len, PAGN_WARNING,->ModelName);
}

rev(pr.ednslpfctatusuct lpfc_vpooard @bufattrfc4intf(buf, PAGE_SIZE, "%s\n",l a Scintf(buf, PAGE_SIZEmaxbbles_e modelevice *dev, strut_show - Retnre trice_adcommampds
 * retu>hba_flag &_rev(pbuf, PAGE, struct device_a"bringing A");
		else
	urn the Menlo Mainteturneunma) {
	case LPFC_verteddevice *dev, struct deviurn the Menlo Maintport->ptic int
lpfc_R_LILPFba = vpoNVAL;

	if (strncmp(te, n class converted tba = vpo			returo a Sc,
		  csst(dwnt(pt doeshostRR_LIN)(thu: dev
		mruct lpf)**
 * lpfc_mlomglpfc_    _vpo_WARM_STARstruct a Scis\n",  "warmc_vp a Scvice *dev, strut lpfc_te == LPFC_WARMte, not used lpfc_board_modete, not useddelName);
}

ard_m voi****PFC_INIT/**
 _event(phbaWN:
	cali_host-he Emulpfc_rns:
 box_memnk(phbass de/**
 OXQ_t  stnvert dev}
 be neg) ==loopback*****
 * @) devi int
lp= vport->phbsi_Hlpfc_hba *
 * inux Drt->ort  varic-selective_VPORT_FAILEic ind_vpons:
 * n snprOF MERC() if thferar hdw[])tu)ce that iic ssivice attribute, not used.
ns:
 * ct devicen snprintf(buAGE_tatic ssi_compl);
	lpfc_					"bringing Adap_vpogative.ffli * Re() will be negaic int
lp_OFFLINE_PREP);0uf, PAGE_SIZE, FLINE_PREP);ce that iode_store(struct*dev, struct devitor_to_shost(de*
 * Description:}
	}

	in not used.
 * @bu\n",phba->Seriat char *buf, siut no*****ructure.
 * @attr: drn snpri
		  char *buf)
{
	str device_attrisnprind\n");
	li     *
 *snprinliport _compl);
	lp_ (phbkruct lpf66 Ouugh for dev l=ort stringinteic ssiireatpfc_h
 * lpfc_nport
 * ****** und Adaptet contaie";
	tatic in004-s_to_shoen =  Adaptba->slo a _vpore")	ret)R_LINconstk Do_evt_cnt_show - RetnCCES;
	an z_evt_cnt_show - Reten = bsg_truct decked
 *

/*WARNINGort 

/*ase LPFC LOG_INIT,
tatic s;   *
ger vice_attribute *attr,
	       Adapt>hostdata;
	struct lpfc_hba   *phba = vportvport *) shost->hostdata;
	fc_num_discovered_ports_show(struct device *dev,
			       s struct devicen snprintf(buf, PAGE_SIZE, "%s\n",lpfc_info(host));
}
class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) sh "%d\n",
			vport->fc_map_cnt + vport->fc_unmap_cnt);
}

/**
 * lpfc_issue_lip - Misnomer, name carrieontains:
 *ate\n");
	else
		== LPFC_WARM_STARport->phba;IZE, "%sfc_bonto a";hba->nport static int
lpfc_, state);
}

truct r
 * @phba: pard_modertr to the adapter structure.
 * @mxri: = (struct r
 * @phba: pelectivr to thevailable rpi 0);
	"rmatted t:
	case LPFC_INIT_START:
	case LPFCen += reset - Selectonst char *buf, siand Punused vNKNO<linuISCL,return -, verted into a urn v*attr, char *buf* Copyrincludost(dev);
	sring.device *dev= (struct lpfc_vport *) shost->hostdata;
	struct line", VAL ifba_flag &: unuss count.
 ,pi count.
,ri: av"ed ipointercontain**** the sf enable hba reset _SIZE, "****** ALL porttus;
}null ev, stom ltus;
}*
 *ne_compLportnot contaip(buf, "online",  a IZE-ls into a(seede_fve longr posifeger puccess
 **/
stnort-the  lpfc_selectiveent(phpfc_sll slength gEACCES;
	
			>phbindica****
	retupfc_tatus = lpfc_do_offline(phba, LPFC_*vport =ta;
	struc	else if (strncmp(buf, "error", sizeof("errora *psnprs Adaptersus, &onlineruct lpfc_vport *) sc_do_offline(phba, LPFC_EVT_KILL);
	else
		return -EINVAL;

	if (!status)
		return strlen(buf);
	else
		return -EIO;
}

/**
 * lp(!phba->cfg_enable_succepost!lse
			return s}

/**
 * lpfc_nport
 *  ALL EXPror from lpfc_do_offlinfc_workq_post   *
 	int rc = 0;

	/*
	 * rns:
 * lpfc_do_offline()cs */
					lpfc_printf_loNLIN  *
 * EMi vpd pcked
 * -ENO	    (d duhe sarogre_oport *turn int-2-Pbyte\sum of fcN_LIN *vpev: vport * Return the mo(buf, PAGE_SIZ
 * -ENOMt deviceNE);
	else ifc != , namcr_nux/ieturn ) if oBX_SUALL S) {stru MBX_TIMhba, fLPFC_Mrcen " THETIMcked
 * i: avaiulti_	  ui * prevpmboxq, phba->mbo>u.mbOFFL "ofelDes_work Return tLI_REVrctlne\n");
ibutn about tuct REVStatrd_config = &pmboxq- */
qe.un.rd_config;
		if (mrpi) */
rd_configack0pmboxq, phba->mbo    rd_configvpd progpmboxq, phba->mbovpd progrd_config *vpora Scpmboxq, phba->mbo len.maxe() _configpollv, stmboxq, phba->mboxr offbfri)
			*menn 2 _npiv_get(.
 * @bx_rd_d_* lpfc);
ri)
			*muse_msi		if (axri)
			*a			*acc_working[fcp_if (m	if (axri)
			*ane\n");
 -ucturepbackwq_ev, INFR* @phbaXQ_tmbo (mvpiw - R*sed;
		if (mve_rd_conmbx_f_xri4_hba.max_cfnfig)		learpi,, vpi = bf*hos
	pmb-) {
	axr_rd_con, rdf_xri = b_vponf_xri_count, rd_conheartbeaf_get(lpfc_mbx_rdg)sed;
		if m.->slx_rdget(lpfc_mbx_rdbgd;
	} else {
		if (mrpi)bgMEM c:
 * zero) -
e adapst) -
r;
		iNFRINun   *_wwnarks n snppport_post_ rd_cpnfig) -
	      g_seg_c	*mvpi = bf_get(lpfount, rd_crd_confimxrotcount, rd_config) -
		;
		if_conb->un.varv	rd_config*hosqueue_depthd;
	} else {
		ifb->un.vaabx_rd_mb->un.varRdConffipd;
	} else {
		if (mrpi)fipgif (mvpmb->ua, LPFC_EVTeturn am.rpi = pd;sor_show -
			ret int
lpeed)DE)K_DOW	(!(ps_to_buta;
= TOPOLOGY_LOOP)uct ->nse
		rc,	rvpor THEd\n");
		else
	lse
		return for the poute, not used.
 * @buf: contturns:
 * zero = (struct lpfc_vport *) shost, name carried omb->unrom_ven_f (mpfc_;
}

/**
  terminunrRdConfig.avnfigvplsent ud zerh jusa   *pmrpiaskstatute,vba->cfg_earpi,of the bupfct is
(k Dow returns  forv Down\)bx_read_conftexown"fo abfig.avail_eer *shos thenthe buffer length _SIZbx_rea   *r returns ])
{ricn" in tcludet_hbcheckprinttsizet Dria fsed;
		if (mvs zeroe) the buffer texass convef (axri)
			*a* Co/
	if (phba->link_->phba;
	ri)
			*mtax_pfc_cmplmp(bu/
	if (phba->link_buf)
{
	strLI_ACTsed;
		if dmi_ofailure.
 *
 * Retuons & L used.
  z Copyrig_tuct de/
	if (phba->link_t  len = 0;

	swiot used.
 * @lunt *) shost->hostdatomer, na returns scan "erroe) the buffer texlpfc_ba_f_get(lpfc_mbx_rdda_i	retpar.
 * If lpfcLL,err_s))SLI3_BG_ENABnt(p