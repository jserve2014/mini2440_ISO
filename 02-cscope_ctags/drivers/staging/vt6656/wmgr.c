/*
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *
 * File: wmgr.c
 *
 * Purpose: Handles the 802.11 management functions
 *
 * Author: Lyndon Chen
 *
 * Date: May 8, 2002
 *
 * Functions:
 *      nsMgrObjectInitial - Initialize Management Objet data structure
 *      vMgrObjectReset - Reset Management Objet data structure
 *      vMgrAssocBeginSta - Start associate function
 *      vMgrReAssocBeginSta - Start reassociate function
 *      vMgrDisassocBeginSta - Start disassociate function
 *      s_vMgrRxAssocRequest - Handle Rcv associate_request
 *      s_vMgrRxAssocResponse - Handle Rcv associate_response
 *      vMrgAuthenBeginSta - Start authentication function
 *      vMgrDeAuthenDeginSta - Start deauthentication function
 *      s_vMgrRxAuthentication - Handle Rcv authentication
 *      s_vMgrRxAuthenSequence_1 - Handle Rcv authentication sequence 1
 *      s_vMgrRxAuthenSequence_2 - Handle Rcv authentication sequence 2
 *      s_vMgrRxAuthenSequence_3 - Handle Rcv authentication sequence 3
 *      s_vMgrRxAuthenSequence_4 - Handle Rcv authentication sequence 4
 *      s_vMgrRxDisassociation - Handle Rcv disassociation
 *      s_vMgrRxBeacon - Handle Rcv Beacon
 *      vMgrCreateOwnIBSS - Create ad_hoc IBSS or AP BSS
 *      vMgrJoinBSSBegin - Join BSS function
 *      s_vMgrSynchBSS - Synch & adopt BSS parameters
 *      s_MgrMakeBeacon - Create Baecon frame
 *      s_MgrMakeProbeResponse - Create Probe Response frame
 *      s_MgrMakeAssocRequest - Create Associate Request frame
 *      s_MgrMakeReAssocRequest - Create ReAssociate Request frame
 *      s_vMgrRxProbeResponse - Handle Rcv probe_response
 *      s_vMrgRxProbeRequest - Handle Rcv probe_request
 *      bMgrPrepareBeaconToSend - Prepare Beacon frame
 *      s_vMgrLogStatus - Log 802.11 Status
 *      vMgrRxManagePacket - Rcv management frame dispatch function
 *      s_vMgrFormatTIM- Assember TIM field of beacon
 *      vMgrTimerInit- Initial 1-sec and command call back funtions
 *
 * Revision History:
 *
 */

#include "tmacro.h"
#include "desc.h"
#include "device.h"
#include "card.h"
#include "80211hdr.h"
#include "80211mgr.h"
#include "wmgr.h"
#include "wcmd.h"
#include "mac.h"
#include "bssdb.h"
#include "power.h"
#include "datarate.h"
#include "baseband.h"
#include "rxtx.h"
#include "wpa.h"
#include "rf.h"
#include "iowpa.h"
#include "control.h"
#include "rndis.h"

/*---------------------  Static Definitions -------------------------*/



/*---------------------  Static Classes  ----------------------------*/

/*---------------------  Static Variables  --------------------------*/
static int          msglevel                =MSG_LEVEL_INFO;
//static int          msglevel                =MSG_LEVEL_DEBUG;

/*---------------------  Static Functions  --------------------------*/
//2008-0730-01<Add>by MikeLiu
static BOOL ChannelExceedZoneType(
    IN PSDevice pDevice,
    IN BYTE byCurrChannel
    );

// Association/diassociation functions
static
PSTxMgmtPacket
s_MgrMakeAssocRequest(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PBYTE pDAddr,
    IN WORD wCurrCapInfo,
    IN WORD wListenInterval,
    IN PWLAN_IE_SSID pCurrSSID,
    IN PWLAN_IE_SUPP_RATES pCurrRates,
    IN PWLAN_IE_SUPP_RATES pCurrExtSuppRates
    );

static
VOID
s_vMgrRxAssocRequest(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket,
    IN UINT  uNodeIndex
    );

static
PSTxMgmtPacket
s_MgrMakeReAssocRequest(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PBYTE pDAddr,
    IN WORD wCurrCapInfo,
    IN WORD wListenInterval,
    IN PWLAN_IE_SSID pCurrSSID,
    IN PWLAN_IE_SUPP_RATES pCurrRates,
    IN PWLAN_IE_SUPP_RATES pCurrExtSuppRates
    );

static
VOID
s_vMgrRxAssocResponse(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket,
    IN BOOL bReAssocType
    );

static
VOID
s_vMgrRxDisassociation(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket
    );

// Authentication/deauthen functions
static
VOID
s_vMgrRxAuthenSequence_1(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PWLAN_FR_AUTHEN pFrame
    );

static
VOID
s_vMgrRxAuthenSequence_2(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PWLAN_FR_AUTHEN pFrame
    );

static
VOID
s_vMgrRxAuthenSequence_3(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PWLAN_FR_AUTHEN pFrame
    );

static
VOID
s_vMgrRxAuthenSequence_4(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PWLAN_FR_AUTHEN pFrame
    );

static
VOID
s_vMgrRxAuthentication(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket
    );

static
VOID
s_vMgrRxDeauthentication(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket
    );

// Scan functions
// probe request/response functions
static
VOID
s_vMgrRxProbeRequest(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket
    );

static
VOID
s_vMgrRxProbeResponse(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket
    );

// beacon functions
static
VOID
s_vMgrRxBeacon(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket,
    IN BOOL bInScan
    );

static
VOID
s_vMgrFormatTIM(
    IN PSMgmtObject pMgmt,
    IN PWLAN_IE_TIM pTIM
    );

static
PSTxMgmtPacket
s_MgrMakeBeacon(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN WORD wCurrCapInfo,
    IN WORD wCurrBeaconPeriod,
    IN UINT uCurrChannel,
    IN WORD wCurrATIMWinodw,
    IN PWLAN_IE_SSID pCurrSSID,
    IN PBYTE pCurrBSSID,
    IN PWLAN_IE_SUPP_RATES pCurrSuppRates,
    IN PWLAN_IE_SUPP_RATES pCurrExtSuppRates
    );


// Association response
static
PSTxMgmtPacket
s_MgrMakeAssocResponse(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN WORD wCurrCapInfo,
    IN WORD wAssocStatus,
    IN WORD wAssocAID,
    IN PBYTE pDstAddr,
    IN PWLAN_IE_SUPP_RATES pCurrSuppRates,
    IN PWLAN_IE_SUPP_RATES pCurrExtSuppRates
    );

// ReAssociation response
static
PSTxMgmtPacket
s_MgrMakeReAssocResponse(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN WORD wCurrCapInfo,
    IN WORD wAssocStatus,
    IN WORD wAssocAID,
    IN PBYTE pDstAddr,
    IN PWLAN_IE_SUPP_RATES pCurrSuppRates,
    IN PWLAN_IE_SUPP_RATES pCurrExtSuppRates
    );

// Probe response
static
PSTxMgmtPacket
s_MgrMakeProbeResponse(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN WORD wCurrCapInfo,
    IN WORD wCurrBeaconPeriod,
    IN UINT uCurrChannel,
    IN WORD wCurrATIMWinodw,
    IN PBYTE pDstAddr,
    IN PWLAN_IE_SSID pCurrSSID,
    IN PBYTE pCurrBSSID,
    IN PWLAN_IE_SUPP_RATES pCurrSuppRates,
    IN PWLAN_IE_SUPP_RATES pCurrExtSuppRates,
    IN BYTE byPHYType
    );

// received status
static
VOID
s_vMgrLogStatus(
    IN PSMgmtObject pMgmt,
    IN WORD wStatus
    );


static
VOID
s_vMgrSynchBSS (
    IN PSDevice      pDevice,
    IN UINT          uBSSMode,
    IN PKnownBSS     pCurr,
    OUT PCMD_STATUS  pStatus
    );


static BOOL
s_bCipherMatch (
    IN PKnownBSS                        pBSSNode,
    IN NDIS_802_11_ENCRYPTION_STATUS    EncStatus,
    OUT PBYTE                           pbyCCSPK,
    OUT PBYTE                           pbyCCSGK
    );

 static VOID  Encyption_Rebuild(
    IN PSDevice pDevice,
    IN PKnownBSS pCurr
 );



/*---------------------  Export Variables  --------------------------*/


/*---------------------  Export Functions  --------------------------*/


/*+
 *
 * Routine Description:
 *    Allocates and initializes the Management object.
 *
 * Return Value:
 *    Ndis_staus.
 *
-*/

VOID
vMgrObjectInit(
    IN  HANDLE hDeviceContext
    )
{
    PSDevice     pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject    pMgmt = &(pDevice->sMgmtObj);
    int ii;


    pMgmt->pbyPSPacketPool = &pMgmt->byPSPacketPool[0];
    pMgmt->pbyMgmtPacketPool = &pMgmt->byMgmtPacketPool[0];
    pMgmt->uCurrChannel = pDevice->uChannel;
    for(ii=0;ii<WLAN_BSSID_LEN;ii++) {
        pMgmt->abyDesireBSSID[ii] = 0xFF;
    }
    pMgmt->sAssocInfo.AssocInfo.Length = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION);
    //memset(pMgmt->abyDesireSSID, 0, WLAN_IEHDR_LEN + WLAN_SSID_MAXLEN +1);
    pMgmt->byCSSPK = KEY_CTL_NONE;
    pMgmt->byCSSGK = KEY_CTL_NONE;
    pMgmt->wIBSSBeaconPeriod = DEFAULT_IBSS_BI;
    BSSvClearBSSList((HANDLE)pDevice, FALSE);

    init_timer(&pMgmt->sTimerSecondCallback);
    pMgmt->sTimerSecondCallback.data = (ULONG)pDevice;
    pMgmt->sTimerSecondCallback.function = (TimerFunction)BSSvSecondCallBack;
    pMgmt->sTimerSecondCallback.expires = RUN_AT(HZ);

    init_timer(&pDevice->sTimerCommand);
    pDevice->sTimerCommand.data = (ULONG)pDevice;
    pDevice->sTimerCommand.function = (TimerFunction)vRunCommand;
    pDevice->sTimerCommand.expires = RUN_AT(HZ);

//2007-0115-10<Add>by MikeLiu
   #ifdef TxInSleep
    init_timer(&pDevice->sTimerTxData);
    pDevice->sTimerTxData.data = (ULONG)pDevice;
    pDevice->sTimerTxData.function = (TimerFunction)BSSvSecondTxData;
    pDevice->sTimerTxData.expires = RUN_AT(10*HZ);      //10s callback
    pDevice->fTxDataInSleep = FALSE;
    pDevice->IsTxDataTrigger = FALSE;
    pDevice->nTxDataTimeCout = 0;
   #endif

    pDevice->cbFreeCmdQueue = CMD_Q_SIZE;
    pDevice->uCmdDequeueIdx = 0;
    pDevice->uCmdEnqueueIdx = 0;
    pDevice->eCommandState = WLAN_CMD_IDLE;
    pDevice->bCmdRunning = FALSE;
    pDevice->bCmdClear = FALSE;

    return;
}



/*+
 *
 * Routine Description:
 *    Start the station association procedure.  Namely, send an
 *    association request frame to the AP.
 *
 * Return Value:
 *    None.
 *
-*/


VOID
vMgrAssocBeginSta(
    IN  HANDLE hDeviceContext,
    IN  PSMgmtObject pMgmt,
    OUT PCMD_STATUS pStatus
    )
{
    PSDevice             pDevice = (PSDevice)hDeviceContext;
    PSTxMgmtPacket          pTxPacket;


    pMgmt->wCurrCapInfo = 0;
    pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_ESS(1);
    if (pDevice->bEncryptionEnable) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_PRIVACY(1);
    }
    // always allow receive short preamble
    //if (pDevice->byPreambleType == 1) {
    //    pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
    //}
    pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
    if (pMgmt->wListenInterval == 0)
        pMgmt->wListenInterval = 1;    // at least one.

    // ERP Phy (802.11g) should support short preamble.
    if (pMgmt->eCurrentPHYMode == PHY_TYPE_11G) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
        if (pDevice->bShortSlotTime == TRUE)
            pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTSLOTTIME(1);

    } else if (pMgmt->eCurrentPHYMode == PHY_TYPE_11B) {
        if (pDevice->byPreambleType == 1) {
            pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
        }
    }
    if (pMgmt->b11hEnable == TRUE)
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SPECTRUMMNG(1);

    // build an assocreq frame and send it
    pTxPacket = s_MgrMakeAssocRequest
                (
                  pDevice,
                  pMgmt,
                  pMgmt->abyCurrBSSID,
                  pMgmt->wCurrCapInfo,
                  pMgmt->wListenInterval,
                  (PWLAN_IE_SSID)pMgmt->abyCurrSSID,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates
                );

    if (pTxPacket != NULL ){
        // send the frame
        *pStatus = csMgmt_xmit(pDevice, pTxPacket);
        if (*pStatus == CMD_STATUS_PENDING) {
            pMgmt->eCurrState = WMAC_STATE_ASSOCPENDING;
            *pStatus = CMD_STATUS_SUCCESS;
        }
    }
    else
        *pStatus = CMD_STATUS_RESOURCES;

    return ;
}


/*+
 *
 * Routine Description:
 *    Start the station re-association procedure.
 *
 * Return Value:
 *    None.
 *
-*/

VOID
vMgrReAssocBeginSta(
    IN  HANDLE hDeviceContext,
    IN  PSMgmtObject pMgmt,
    OUT PCMD_STATUS pStatus
    )
{
    PSDevice             pDevice = (PSDevice)hDeviceContext;
    PSTxMgmtPacket          pTxPacket;



    pMgmt->wCurrCapInfo = 0;
    pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_ESS(1);
    if (pDevice->bEncryptionEnable) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_PRIVACY(1);
    }

    //if (pDevice->byPreambleType == 1) {
    //    pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
    //}
    pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);

    if (pMgmt->wListenInterval == 0)
        pMgmt->wListenInterval = 1;    // at least one.


    // ERP Phy (802.11g) should support short preamble.
    if (pMgmt->eCurrentPHYMode == PHY_TYPE_11G) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
      if (pDevice->bShortSlotTime == TRUE)
          pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTSLOTTIME(1);

    } else if (pMgmt->eCurrentPHYMode == PHY_TYPE_11B) {
        if (pDevice->byPreambleType == 1) {
            pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
        }
    }
    if (pMgmt->b11hEnable == TRUE)
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SPECTRUMMNG(1);


    pTxPacket = s_MgrMakeReAssocRequest
                (
                  pDevice,
                  pMgmt,
                  pMgmt->abyCurrBSSID,
                  pMgmt->wCurrCapInfo,
                  pMgmt->wListenInterval,
                  (PWLAN_IE_SSID)pMgmt->abyCurrSSID,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates
                );

    if (pTxPacket != NULL ){
        // send the frame
        *pStatus = csMgmt_xmit(pDevice, pTxPacket);
        if (*pStatus != CMD_STATUS_PENDING) {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Reassociation tx failed.\n");
        }
        else {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Reassociation tx sending.\n");
        }
    }


    return ;
}

/*+
 *
 * Routine Description:
 *    Send an dis-association request frame to the AP.
 *
 * Return Value:
 *    None.
 *
-*/

VOID
vMgrDisassocBeginSta(
    IN  HANDLE hDeviceContext,
    IN  PSMgmtObject pMgmt,
    IN  PBYTE  abyDestAddress,
    IN  WORD    wReason,
    OUT PCMD_STATUS pStatus
    )
{
    PSDevice            pDevice = (PSDevice)hDeviceContext;
    PSTxMgmtPacket      pTxPacket = NULL;
    WLAN_FR_DISASSOC    sFrame;

    pTxPacket = (PSTxMgmtPacket)pMgmt->pbyMgmtPacketPool;
    memset(pTxPacket, 0, sizeof(STxMgmtPacket) + WLAN_DISASSOC_FR_MAXLEN);
    pTxPacket->p80211Header = (PUWLAN_80211HDR)((PBYTE)pTxPacket + sizeof(STxMgmtPacket));

    // Setup the sFrame structure
    sFrame.pBuf = (PBYTE)pTxPacket->p80211Header;
    sFrame.len = WLAN_DISASSOC_FR_MAXLEN;

    // format fixed field frame structure
    vMgrEncodeDisassociation(&sFrame);

    // Setup the header
    sFrame.pHdr->sA3.wFrameCtl = cpu_to_le16(
        (
        WLAN_SET_FC_FTYPE(WLAN_TYPE_MGR) |
        WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_DISASSOC)
        ));

    memcpy( sFrame.pHdr->sA3.abyAddr1, abyDestAddress, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);

    // Set reason code
    *(sFrame.pwReason) = cpu_to_le16(wReason);
    pTxPacket->cbMPDULen = sFrame.len;
    pTxPacket->cbPayloadLen = sFrame.len - WLAN_HDR_ADDR3_LEN;

    // send the frame
    *pStatus = csMgmt_xmit(pDevice, pTxPacket);
    if (*pStatus == CMD_STATUS_PENDING) {
        pMgmt->eCurrState = WMAC_STATE_IDLE;
        *pStatus = CMD_STATUS_SUCCESS;
    };

    return;
}



/*+
 *
 * Routine Description:(AP function)
 *    Handle incoming station association request frames.
 *
 * Return Value:
 *    None.
 *
-*/

static
VOID
s_vMgrRxAssocRequest(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket,
    IN UINT uNodeIndex
    )
{
    WLAN_FR_ASSOCREQ    sFrame;
    CMD_STATUS          Status;
    PSTxMgmtPacket      pTxPacket;
    WORD                wAssocStatus = 0;
    WORD                wAssocAID = 0;
    UINT                uRateLen = WLAN_RATES_MAXLEN;
    BYTE                abyCurrSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];
    BYTE                abyCurrExtSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];


    if (pMgmt->eCurrMode != WMAC_MODE_ESS_AP)
        return;
    //  node index not found
    if (!uNodeIndex)
        return;

    //check if node is authenticated
    //decode the frame
    memset(&sFrame, 0, sizeof(WLAN_FR_ASSOCREQ));
    memset(abyCurrSuppRates, 0, WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1);
    memset(abyCurrExtSuppRates, 0, WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1);
    sFrame.len = pRxPacket->cbMPDULen;
    sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;

    vMgrDecodeAssocRequest(&sFrame);

    if (pMgmt->sNodeDBTable[uNodeIndex].eNodeState >= NODE_AUTH) {
        pMgmt->sNodeDBTable[uNodeIndex].eNodeState = NODE_ASSOC;
        pMgmt->sNodeDBTable[uNodeIndex].wCapInfo = cpu_to_le16(*sFrame.pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].wListenInterval = cpu_to_le16(*sFrame.pwListenInterval);
        pMgmt->sNodeDBTable[uNodeIndex].bPSEnable =
                WLAN_GET_FC_PWRMGT(sFrame.pHdr->sA3.wFrameCtl) ? TRUE : FALSE;
        // Todo: check sta basic rate, if ap can't support, set status code
        if (pDevice->byBBType == BB_TYPE_11B) {
            uRateLen = WLAN_RATES_MAXLEN_11B;
        }
        abyCurrSuppRates[0] = WLAN_EID_SUPP_RATES;
        abyCurrSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pSuppRates,
                                         (PWLAN_IE_SUPP_RATES)abyCurrSuppRates,
                                         uRateLen);
        abyCurrExtSuppRates[0] = WLAN_EID_EXTSUPP_RATES;
        if (pDevice->byBBType == BB_TYPE_11G) {
            abyCurrExtSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pExtSuppRates,
                                                (PWLAN_IE_SUPP_RATES)abyCurrExtSuppRates,
                                                uRateLen);
        } else {
            abyCurrExtSuppRates[1] = 0;
        }


        RATEvParseMaxRate((PVOID)pDevice,
                           (PWLAN_IE_SUPP_RATES)abyCurrSuppRates,
                           (PWLAN_IE_SUPP_RATES)abyCurrExtSuppRates,
                           FALSE, // do not change our basic rate
                           &(pMgmt->sNodeDBTable[uNodeIndex].wMaxBasicRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].wSuppRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].byTopCCKBasicRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].byTopOFDMBasicRate)
                          );

        // set max tx rate
        pMgmt->sNodeDBTable[uNodeIndex].wTxDataRate =
                pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate;
        // Todo: check sta preamble, if ap can't support, set status code
        pMgmt->sNodeDBTable[uNodeIndex].bShortPreamble =
                WLAN_GET_CAP_INFO_SHORTPREAMBLE(*sFrame.pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].bShortSlotTime =
                WLAN_GET_CAP_INFO_SHORTSLOTTIME(*sFrame.pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].wAID = (WORD)uNodeIndex;
        wAssocStatus = WLAN_MGMT_STATUS_SUCCESS;
        wAssocAID = (WORD)uNodeIndex;
        // check if ERP support
        if(pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate > RATE_11M)
           pMgmt->sNodeDBTable[uNodeIndex].bERPExist = TRUE;

        if (pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate <= RATE_11M) {
            // B only STA join
            pDevice->bProtectMode = TRUE;
            pDevice->bNonERPPresent = TRUE;
        }
        if (pMgmt->sNodeDBTable[uNodeIndex].bShortPreamble == FALSE) {
            pDevice->bBarkerPreambleMd = TRUE;
        }

        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Associate AID= %d \n", wAssocAID);
        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "MAC=%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X \n",
                   sFrame.pHdr->sA3.abyAddr2[0],
                   sFrame.pHdr->sA3.abyAddr2[1],
                   sFrame.pHdr->sA3.abyAddr2[2],
                   sFrame.pHdr->sA3.abyAddr2[3],
                   sFrame.pHdr->sA3.abyAddr2[4],
                   sFrame.pHdr->sA3.abyAddr2[5]
                  ) ;
        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Max Support rate = %d \n",
                   pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate);
    }


    // assoc response reply..
    pTxPacket = s_MgrMakeAssocResponse
                (
                  pDevice,
                  pMgmt,
                  pMgmt->wCurrCapInfo,
                  wAssocStatus,
                  wAssocAID,
                  sFrame.pHdr->sA3.abyAddr2,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates
                );
    if (pTxPacket != NULL ){

        if (pDevice->bEnableHostapd) {
            return;
        }
        /* send the frame */
        Status = csMgmt_xmit(pDevice, pTxPacket);
        if (Status != CMD_STATUS_PENDING) {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Assoc response tx failed\n");
        }
        else {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Assoc response tx sending..\n");
        }

    }

    return;
}


/*+
 *
 * Description:(AP function)
 *      Handle incoming station re-association request frames.
 *
 * Parameters:
 *  In:
 *      pMgmt           - Management Object structure
 *      pRxPacket       - Received Packet
 *  Out:
 *      none
 *
 * Return Value: None.
 *
-*/

static
VOID
s_vMgrRxReAssocRequest(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket,
    IN UINT uNodeIndex
    )
{
    WLAN_FR_REASSOCREQ    sFrame;
    CMD_STATUS          Status;
    PSTxMgmtPacket      pTxPacket;
    WORD                wAssocStatus = 0;
    WORD                wAssocAID = 0;
    UINT                uRateLen = WLAN_RATES_MAXLEN;
    BYTE                abyCurrSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];
    BYTE                abyCurrExtSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];

    if (pMgmt->eCurrMode != WMAC_MODE_ESS_AP)
        return;
    //  node index not found
    if (!uNodeIndex)
        return;
    //check if node is authenticated
    //decode the frame
    memset(&sFrame, 0, sizeof(WLAN_FR_REASSOCREQ));
    sFrame.len = pRxPacket->cbMPDULen;
    sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
    vMgrDecodeReassocRequest(&sFrame);

    if (pMgmt->sNodeDBTable[uNodeIndex].eNodeState >= NODE_AUTH) {
        pMgmt->sNodeDBTable[uNodeIndex].eNodeState = NODE_ASSOC;
        pMgmt->sNodeDBTable[uNodeIndex].wCapInfo = cpu_to_le16(*sFrame.pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].wListenInterval = cpu_to_le16(*sFrame.pwListenInterval);
        pMgmt->sNodeDBTable[uNodeIndex].bPSEnable =
                WLAN_GET_FC_PWRMGT(sFrame.pHdr->sA3.wFrameCtl) ? TRUE : FALSE;
        // Todo: check sta basic rate, if ap can't support, set status code

        if (pDevice->byBBType == BB_TYPE_11B) {
            uRateLen = WLAN_RATES_MAXLEN_11B;
        }

        abyCurrSuppRates[0] = WLAN_EID_SUPP_RATES;
        abyCurrSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pSuppRates,
                                         (PWLAN_IE_SUPP_RATES)abyCurrSuppRates,
                                         uRateLen);
        abyCurrExtSuppRates[0] = WLAN_EID_EXTSUPP_RATES;
        if (pDevice->byBBType == BB_TYPE_11G) {
            abyCurrExtSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pExtSuppRates,
                                                (PWLAN_IE_SUPP_RATES)abyCurrExtSuppRates,
                                                uRateLen);
        } else {
            abyCurrExtSuppRates[1] = 0;
        }


        RATEvParseMaxRate((PVOID)pDevice,
                          (PWLAN_IE_SUPP_RATES)abyCurrSuppRates,
                          (PWLAN_IE_SUPP_RATES)abyCurrExtSuppRates,
                           FALSE, // do not change our basic rate
                           &(pMgmt->sNodeDBTable[uNodeIndex].wMaxBasicRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].wSuppRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].byTopCCKBasicRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].byTopOFDMBasicRate)
                          );

        // set max tx rate
        pMgmt->sNodeDBTable[uNodeIndex].wTxDataRate =
                pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate;
        // Todo: check sta preamble, if ap can't support, set status code
        pMgmt->sNodeDBTable[uNodeIndex].bShortPreamble =
                WLAN_GET_CAP_INFO_SHORTPREAMBLE(*sFrame.pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].bShortSlotTime =
                WLAN_GET_CAP_INFO_SHORTSLOTTIME(*sFrame.pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].wAID = (WORD)uNodeIndex;
        wAssocStatus = WLAN_MGMT_STATUS_SUCCESS;
        wAssocAID = (WORD)uNodeIndex;

        // if suppurt ERP
        if(pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate > RATE_11M)
           pMgmt->sNodeDBTable[uNodeIndex].bERPExist = TRUE;

        if (pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate <= RATE_11M) {
            // B only STA join
            pDevice->bProtectMode = TRUE;
            pDevice->bNonERPPresent = TRUE;
        }
        if (pMgmt->sNodeDBTable[uNodeIndex].bShortPreamble == FALSE) {
            pDevice->bBarkerPreambleMd = TRUE;
        }

        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Rx ReAssociate AID= %d \n", wAssocAID);
        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "MAC=%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X \n",
                   sFrame.pHdr->sA3.abyAddr2[0],
                   sFrame.pHdr->sA3.abyAddr2[1],
                   sFrame.pHdr->sA3.abyAddr2[2],
                   sFrame.pHdr->sA3.abyAddr2[3],
                   sFrame.pHdr->sA3.abyAddr2[4],
                   sFrame.pHdr->sA3.abyAddr2[5]
                  ) ;
        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Max Support rate = %d \n",
                   pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate);

    }


    // assoc response reply..
    pTxPacket = s_MgrMakeReAssocResponse
                (
                  pDevice,
                  pMgmt,
                  pMgmt->wCurrCapInfo,
                  wAssocStatus,
                  wAssocAID,
                  sFrame.pHdr->sA3.abyAddr2,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates
                );

    if (pTxPacket != NULL ){
        /* send the frame */
        if (pDevice->bEnableHostapd) {
            return;
        }
        Status = csMgmt_xmit(pDevice, pTxPacket);
        if (Status != CMD_STATUS_PENDING) {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:ReAssoc response tx failed\n");
        }
        else {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:ReAssoc response tx sending..\n");
        }
    }
    return;
}


/*+
 *
 * Routine Description:
 *    Handle incoming association response frames.
 *
 * Return Value:
 *    None.
 *
-*/

static
VOID
s_vMgrRxAssocResponse(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket,
    IN BOOL bReAssocType
    )
{
    WLAN_FR_ASSOCRESP   sFrame;
    PWLAN_IE_SSID   pItemSSID;
    PBYTE   pbyIEs;
    viawget_wpa_header *wpahdr;



    if (pMgmt->eCurrState == WMAC_STATE_ASSOCPENDING ||
         pMgmt->eCurrState == WMAC_STATE_ASSOC) {

        sFrame.len = pRxPacket->cbMPDULen;
        sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
        // decode the frame
        vMgrDecodeAssocResponse(&sFrame);
        if ((sFrame.pwCapInfo == 0) ||
            (sFrame.pwStatus == 0) ||
            (sFrame.pwAid == 0) ||
            (sFrame.pSuppRates == 0)){
            DBG_PORT80(0xCC);
            return;
        };

        pMgmt->sAssocInfo.AssocInfo.ResponseFixedIEs.Capabilities = *(sFrame.pwCapInfo);
        pMgmt->sAssocInfo.AssocInfo.ResponseFixedIEs.StatusCode = *(sFrame.pwStatus);
        pMgmt->sAssocInfo.AssocInfo.ResponseFixedIEs.AssociationId = *(sFrame.pwAid);
        pMgmt->sAssocInfo.AssocInfo.AvailableResponseFixedIEs |= 0x07;

        pMgmt->sAssocInfo.AssocInfo.ResponseIELength = sFrame.len - 24 - 6;
        pMgmt->sAssocInfo.AssocInfo.OffsetResponseIEs = pMgmt->sAssocInfo.AssocInfo.OffsetRequestIEs + pMgmt->sAssocInfo.AssocInfo.RequestIELength;
        pbyIEs = pMgmt->sAssocInfo.abyIEs;
        pbyIEs += pMgmt->sAssocInfo.AssocInfo.RequestIELength;
        memcpy(pbyIEs, (sFrame.pBuf + 24 +6), pMgmt->sAssocInfo.AssocInfo.ResponseIELength);

        // save values and set current BSS state
        if (cpu_to_le16((*(sFrame.pwStatus))) == WLAN_MGMT_STATUS_SUCCESS ){
            // set AID
            pMgmt->wCurrAID = cpu_to_le16((*(sFrame.pwAid)));
            if ( (pMgmt->wCurrAID >> 14) != (BIT0 | BIT1) )
            {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "AID from AP, has two msb clear.\n");
            };
            DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Association Successful, AID=%d.\n", pMgmt->wCurrAID & ~(BIT14|BIT15));
            pMgmt->eCurrState = WMAC_STATE_ASSOC;
            BSSvUpdateAPNode((HANDLE)pDevice, sFrame.pwCapInfo, sFrame.pSuppRates, sFrame.pExtSuppRates);
            pItemSSID = (PWLAN_IE_SSID)pMgmt->abyCurrSSID;
            DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Link with AP(SSID): %s\n", pItemSSID->abySSID);
            pDevice->bLinkPass = TRUE;
            ControlvMaskByte(pDevice,MESSAGE_REQUEST_MACREG,MAC_REG_PAPEDELAY,LEDSTS_STS,LEDSTS_INTER);
            if ((pDevice->bWPADEVUp) && (pDevice->skb != NULL)) {
	       if(skb_tailroom(pDevice->skb) <(sizeof(viawget_wpa_header)+pMgmt->sAssocInfo.AssocInfo.ResponseIELength+
		   	                                                 pMgmt->sAssocInfo.AssocInfo.RequestIELength)) {    //data room not enough
                     dev_kfree_skb(pDevice->skb);
		   pDevice->skb = dev_alloc_skb((int)pDevice->rx_buf_sz);
	       	}
                wpahdr = (viawget_wpa_header *)pDevice->skb->data;
                wpahdr->type = VIAWGET_ASSOC_MSG;
                wpahdr->resp_ie_len = pMgmt->sAssocInfo.AssocInfo.ResponseIELength;
                wpahdr->req_ie_len = pMgmt->sAssocInfo.AssocInfo.RequestIELength;
                memcpy(pDevice->skb->data + sizeof(viawget_wpa_header), pMgmt->sAssocInfo.abyIEs, wpahdr->req_ie_len);
                memcpy(pDevice->skb->data + sizeof(viawget_wpa_header) + wpahdr->req_ie_len,
                       pbyIEs,
                       wpahdr->resp_ie_len
                       );
                skb_put(pDevice->skb, sizeof(viawget_wpa_header) + wpahdr->resp_ie_len + wpahdr->req_ie_len);
                pDevice->skb->dev = pDevice->wpadev;
		skb_reset_mac_header(pDevice->skb);
                pDevice->skb->pkt_type = PACKET_HOST;
                pDevice->skb->protocol = htons(ETH_P_802_2);
                memset(pDevice->skb->cb, 0, sizeof(pDevice->skb->cb));
                netif_rx(pDevice->skb);
                pDevice->skb = dev_alloc_skb((int)pDevice->rx_buf_sz);
            }
//2008-0409-07, <Add> by Einsn Liu
#ifdef WPA_SUPPLICANT_DRIVER_WEXT_SUPPORT
	//if(pDevice->bWPASuppWextEnabled == TRUE)
	   {
		BYTE buf[512];
		size_t len;
		union iwreq_data  wrqu;
		int we_event;

		memset(buf, 0, 512);

		len = pMgmt->sAssocInfo.AssocInfo.RequestIELength;
		if(len)	{
			memcpy(buf, pMgmt->sAssocInfo.abyIEs, len);
			memset(&wrqu, 0, sizeof (wrqu));
			wrqu.data.length = len;
			we_event = IWEVASSOCREQIE;
			PRINT_K("wireless_send_event--->IWEVASSOCREQIE\n");
			wireless_send_event(pDevice->dev, we_event, &wrqu, buf);
		}

		memset(buf, 0, 512);
		len = pMgmt->sAssocInfo.AssocInfo.ResponseIELength;

		if(len)	{
			memcpy(buf, pbyIEs, len);
			memset(&wrqu, 0, sizeof (wrqu));
			wrqu.data.length = len;
			we_event = IWEVASSOCRESPIE;
			PRINT_K("wireless_send_event--->IWEVASSOCRESPIE\n");
			wireless_send_event(pDevice->dev, we_event, &wrqu, buf);
		}

	   memset(&wrqu, 0, sizeof (wrqu));
	memcpy(wrqu.ap_addr.sa_data, &pMgmt->abyCurrBSSID[0], ETH_ALEN);
        wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	   PRINT_K("wireless_send_event--->SIOCGIWAP(associated)\n");
	wireless_send_event(pDevice->dev, SIOCGIWAP, &wrqu, NULL);

	}
#endif //#ifdef WPA_SUPPLICANT_DRIVER_WEXT_SUPPORT
//End Add -- //2008-0409-07, <Add> by Einsn Liu
        }
        else {
            if (bReAssocType) {
                pMgmt->eCurrState = WMAC_STATE_IDLE;
            }
            else {
                // jump back to the auth state and indicate the error
                pMgmt->eCurrState = WMAC_STATE_AUTH;
            }
            s_vMgrLogStatus(pMgmt,cpu_to_le16((*(sFrame.pwStatus))));
        }

    }

#if 1
#ifdef WPA_SUPPLICANT_DRIVER_WEXT_SUPPORT
//need clear flags related to Networkmanager
              pDevice->bwextstep0 = FALSE;
              pDevice->bwextstep1 = FALSE;
              pDevice->bwextstep2 = FALSE;
              pDevice->bwextstep3 = FALSE;
              pDevice->bWPASuppWextEnabled = FALSE;
#endif
#endif

if(pMgmt->eCurrState == WMAC_STATE_ASSOC)
      timer_expire(pDevice->sTimerCommand, 0);

    return;
}



/*+
 *
 * Routine Description:
 *    Start the station authentication procedure.  Namely, send an
 *    authentication frame to the AP.
 *
 * Return Value:
 *    None.
 *
-*/

VOID
vMgrAuthenBeginSta(
    IN  HANDLE hDeviceContext,
    IN  PSMgmtObject  pMgmt,
    OUT PCMD_STATUS pStatus
    )
{
    PSDevice     pDevice = (PSDevice)hDeviceContext;
    WLAN_FR_AUTHEN  sFrame;
    PSTxMgmtPacket  pTxPacket = NULL;

    pTxPacket = (PSTxMgmtPacket)pMgmt->pbyMgmtPacketPool;
    memset(pTxPacket, 0, sizeof(STxMgmtPacket) + WLAN_AUTHEN_FR_MAXLEN);
    pTxPacket->p80211Header = (PUWLAN_80211HDR)((PBYTE)pTxPacket + sizeof(STxMgmtPacket));
    sFrame.pBuf = (PBYTE)pTxPacket->p80211Header;
    sFrame.len = WLAN_AUTHEN_FR_MAXLEN;
    vMgrEncodeAuthen(&sFrame);
    /* insert values */
    sFrame.pHdr->sA3.wFrameCtl = cpu_to_le16(
        (
        WLAN_SET_FC_FTYPE(WLAN_TYPE_MGR) |
        WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN)
        ));
    memcpy( sFrame.pHdr->sA3.abyAddr1, pMgmt->abyCurrBSSID, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);
    if (pMgmt->bShareKeyAlgorithm)
        *(sFrame.pwAuthAlgorithm) = cpu_to_le16(WLAN_AUTH_ALG_SHAREDKEY);
    else
        *(sFrame.pwAuthAlgorithm) = cpu_to_le16(WLAN_AUTH_ALG_OPENSYSTEM);

    *(sFrame.pwAuthSequence) = cpu_to_le16(1);
    /* Adjust the length fields */
    pTxPacket->cbMPDULen = sFrame.len;
    pTxPacket->cbPayloadLen = sFrame.len - WLAN_HDR_ADDR3_LEN;

    *pStatus = csMgmt_xmit(pDevice, pTxPacket);
    if (*pStatus == CMD_STATUS_PENDING){
        pMgmt->eCurrState = WMAC_STATE_AUTHPENDING;
        *pStatus = CMD_STATUS_SUCCESS;
    }

    return ;
}



/*+
 *
 * Routine Description:
 *    Start the station(AP) deauthentication procedure.  Namely, send an
 *    deauthentication frame to the AP or Sta.
 *
 * Return Value:
 *    None.
 *
-*/

VOID
vMgrDeAuthenBeginSta(
    IN  HANDLE hDeviceContext,
    IN  PSMgmtObject  pMgmt,
    IN  PBYTE  abyDestAddress,
    IN  WORD    wReason,
    OUT PCMD_STATUS pStatus
    )
{
    PSDevice            pDevice = (PSDevice)hDeviceContext;
    WLAN_FR_DEAUTHEN    sFrame;
    PSTxMgmtPacket      pTxPacket = NULL;


    pTxPacket = (PSTxMgmtPacket)pMgmt->pbyMgmtPacketPool;
    memset(pTxPacket, 0, sizeof(STxMgmtPacket) + WLAN_DEAUTHEN_FR_MAXLEN);
    pTxPacket->p80211Header = (PUWLAN_80211HDR)((PBYTE)pTxPacket + sizeof(STxMgmtPacket));
    sFrame.pBuf = (PBYTE)pTxPacket->p80211Header;
    sFrame.len = WLAN_DEAUTHEN_FR_MAXLEN;
    vMgrEncodeDeauthen(&sFrame);
    /* insert values */
    sFrame.pHdr->sA3.wFrameCtl = cpu_to_le16(
        (
        WLAN_SET_FC_FTYPE(WLAN_TYPE_MGR) |
        WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_DEAUTHEN)
        ));

    memcpy( sFrame.pHdr->sA3.abyAddr1, abyDestAddress, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);

    *(sFrame.pwReason) = cpu_to_le16(wReason);       // deauthen. bcs left BSS
    /* Adjust the length fields */
    pTxPacket->cbMPDULen = sFrame.len;
    pTxPacket->cbPayloadLen = sFrame.len - WLAN_HDR_ADDR3_LEN;

    *pStatus = csMgmt_xmit(pDevice, pTxPacket);
    if (*pStatus == CMD_STATUS_PENDING){
        *pStatus = CMD_STATUS_SUCCESS;
    }


    return ;
}


/*+
 *
 * Routine Description:
 *    Handle incoming authentication frames.
 *
 * Return Value:
 *    None.
 *
-*/

static
VOID
s_vMgrRxAuthentication(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket
    )
{
    WLAN_FR_AUTHEN  sFrame;

    // we better be an AP or a STA in AUTHPENDING otherwise ignore
    if (!(pMgmt->eCurrMode == WMAC_MODE_ESS_AP ||
          pMgmt->eCurrState == WMAC_STATE_AUTHPENDING)) {
        return;
    }

    // decode the frame
    sFrame.len = pRxPacket->cbMPDULen;
    sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
    vMgrDecodeAuthen(&sFrame);
    switch (cpu_to_le16((*(sFrame.pwAuthSequence )))){
        case 1:
            //AP funciton
            s_vMgrRxAuthenSequence_1(pDevice,pMgmt, &sFrame);
            break;
        case 2:
            s_vMgrRxAuthenSequence_2(pDevice, pMgmt, &sFrame);
            break;
        case 3:
            //AP funciton
            s_vMgrRxAuthenSequence_3(pDevice, pMgmt, &sFrame);
            break;
        case 4:
            s_vMgrRxAuthenSequence_4(pDevice, pMgmt, &sFrame);
            break;
        default:
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Auth Sequence error, seq = %d\n",
                        cpu_to_le16((*(sFrame.pwAuthSequence))));
            break;
    }
    return;
}



/*+
 *
 * Routine Description:
 *   Handles incoming authen frames with sequence 1.  Currently
 *   assumes we're an AP.  So far, no one appears to use authentication
 *   in Ad-Hoc mode.
 *
 * Return Value:
 *    None.
 *
-*/


static
VOID
s_vMgrRxAuthenSequence_1(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PWLAN_FR_AUTHEN pFrame
     )
{
    PSTxMgmtPacket      pTxPacket = NULL;
    UINT                uNodeIndex;
    WLAN_FR_AUTHEN      sFrame;
    PSKeyItem           pTransmitKey;

    // Insert a Node entry
    if (!BSSbIsSTAInNodeDB(pDevice, pFrame->pHdr->sA3.abyAddr2, &uNodeIndex)) {
        BSSvCreateOneNode((PSDevice)pDevice, &uNodeIndex);
        memcpy(pMgmt->sNodeDBTable[uNodeIndex].abyMACAddr, pFrame->pHdr->sA3.abyAddr2,
               WLAN_ADDR_LEN);
    }

    if (pMgmt->bShareKeyAlgorithm) {
        pMgmt->sNodeDBTable[uNodeIndex].eNodeState = NODE_KNOWN;
        pMgmt->sNodeDBTable[uNodeIndex].byAuthSequence = 1;
    }
    else {
        pMgmt->sNodeDBTable[uNodeIndex].eNodeState = NODE_AUTH;
    }

    // send auth reply
    pTxPacket = (PSTxMgmtPacket)pMgmt->pbyMgmtPacketPool;
    memset(pTxPacket, 0, sizeof(STxMgmtPacket) + WLAN_AUTHEN_FR_MAXLEN);
    pTxPacket->p80211Header = (PUWLAN_80211HDR)((PBYTE)pTxPacket + sizeof(STxMgmtPacket));
    sFrame.pBuf = (PBYTE)pTxPacket->p80211Header;
    sFrame.len = WLAN_AUTHEN_FR_MAXLEN;
    // format buffer structure
    vMgrEncodeAuthen(&sFrame);
    // insert values
    sFrame.pHdr->sA3.wFrameCtl = cpu_to_le16(
         (
         WLAN_SET_FC_FTYPE(WLAN_TYPE_MGR) |
         WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN)|
         WLAN_SET_FC_ISWEP(0)
         ));
    memcpy( sFrame.pHdr->sA3.abyAddr1, pFrame->pHdr->sA3.abyAddr2, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);
    *(sFrame.pwAuthAlgorithm) = *(pFrame->pwAuthAlgorithm);
    *(sFrame.pwAuthSequence) = cpu_to_le16(2);

    if (cpu_to_le16(*(pFrame->pwAuthAlgorithm)) == WLAN_AUTH_ALG_SHAREDKEY) {
        if (pMgmt->bShareKeyAlgorithm)
            *(sFrame.pwStatus) = cpu_to_le16(WLAN_MGMT_STATUS_SUCCESS);
        else
            *(sFrame.pwStatus) = cpu_to_le16(WLAN_MGMT_STATUS_UNSUPPORTED_AUTHALG);
    }
    else {
        if (pMgmt->bShareKeyAlgorithm)
            *(sFrame.pwStatus) = cpu_to_le16(WLAN_MGMT_STATUS_UNSUPPORTED_AUTHALG);
        else
            *(sFrame.pwStatus) = cpu_to_le16(WLAN_MGMT_STATUS_SUCCESS);
    }

    if (pMgmt->bShareKeyAlgorithm &&
        (cpu_to_le16(*(sFrame.pwStatus)) == WLAN_MGMT_STATUS_SUCCESS)) {

        sFrame.pChallenge = (PWLAN_IE_CHALLENGE)(sFrame.pBuf + sFrame.len);
        sFrame.len += WLAN_CHALLENGE_IE_LEN;
        sFrame.pChallenge->byElementID = WLAN_EID_CHALLENGE;
        sFrame.pChallenge->len = WLAN_CHALLENGE_LEN;
        memset(pMgmt->abyChallenge, 0, WLAN_CHALLENGE_LEN);
        // get group key
        if(KeybGetTransmitKey(&(pDevice->sKey), pDevice->abyBroadcastAddr, GROUP_KEY, &pTransmitKey) == TRUE) {
            rc4_init(&pDevice->SBox, pDevice->abyPRNG, pTransmitKey->uKeyLength+3);
            rc4_encrypt(&pDevice->SBox, pMgmt->abyChallenge, pMgmt->abyChallenge, WLAN_CHALLENGE_LEN);
        }
        memcpy(sFrame.pChallenge->abyChallenge, pMgmt->abyChallenge , WLAN_CHALLENGE_LEN);
    }

    /* Adjust the length fields */
    pTxPacket->cbMPDULen = sFrame.len;
    pTxPacket->cbPayloadLen = sFrame.len - WLAN_HDR_ADDR3_LEN;
    // send the frame
    if (pDevice->bEnableHostapd) {
        return;
    }
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Authreq_reply sequence_1 tx.. \n");
    if (csMgmt_xmit(pDevice, pTxPacket) != CMD_STATUS_PENDING) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Authreq_reply sequence_1 tx failed.\n");
    }
    return;
}



/*+
 *
 * Routine Description:
 *   Handles incoming auth frames with sequence number 2.  Currently
 *   assumes we're a station.
 *
 *
 * Return Value:
 *    None.
 *
-*/

static
VOID
s_vMgrRxAuthenSequence_2(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PWLAN_FR_AUTHEN pFrame
    )
{
    WLAN_FR_AUTHEN      sFrame;
    PSTxMgmtPacket      pTxPacket = NULL;


    switch (cpu_to_le16((*(pFrame->pwAuthAlgorithm))))
    {
        case WLAN_AUTH_ALG_OPENSYSTEM:
            if ( cpu_to_le16((*(pFrame->pwStatus))) == WLAN_MGMT_STATUS_SUCCESS ){
                DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "802.11 Authen (OPEN) Successful.\n");
                pMgmt->eCurrState = WMAC_STATE_AUTH;
	       timer_expire(pDevice->sTimerCommand, 0);
            }
            else {
                DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "802.11 Authen (OPEN) Failed.\n");
                s_vMgrLogStatus(pMgmt, cpu_to_le16((*(pFrame->pwStatus))));
                pMgmt->eCurrState = WMAC_STATE_IDLE;
            }
            if (pDevice->eCommandState == WLAN_AUTHENTICATE_WAIT ) {
//                spin_unlock_irq(&pDevice->lock);
//                vCommandTimerWait((HANDLE)pDevice, 0);
//                spin_lock_irq(&pDevice->lock);
            }

            break;

        case WLAN_AUTH_ALG_SHAREDKEY:

            if (cpu_to_le16((*(pFrame->pwStatus))) == WLAN_MGMT_STATUS_SUCCESS) {
                pTxPacket = (PSTxMgmtPacket)pMgmt->pbyMgmtPacketPool;
                memset(pTxPacket, 0, sizeof(STxMgmtPacket) + WLAN_AUTHEN_FR_MAXLEN);
                pTxPacket->p80211Header = (PUWLAN_80211HDR)((PBYTE)pTxPacket + sizeof(STxMgmtPacket));
                sFrame.pBuf = (PBYTE)pTxPacket->p80211Header;
                sFrame.len = WLAN_AUTHEN_FR_MAXLEN;
                // format buffer structure
                vMgrEncodeAuthen(&sFrame);
                // insert values
                sFrame.pHdr->sA3.wFrameCtl = cpu_to_le16(
                     (
                     WLAN_SET_FC_FTYPE(WLAN_TYPE_MGR) |
                     WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN)|
                     WLAN_SET_FC_ISWEP(1)
                     ));
                memcpy( sFrame.pHdr->sA3.abyAddr1, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);
                memcpy( sFrame.pHdr->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
                memcpy( sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);
                *(sFrame.pwAuthAlgorithm) = *(pFrame->pwAuthAlgorithm);
                *(sFrame.pwAuthSequence) = cpu_to_le16(3);
                *(sFrame.pwStatus) = cpu_to_le16(WLAN_MGMT_STATUS_SUCCESS);
                sFrame.pChallenge = (PWLAN_IE_CHALLENGE)(sFrame.pBuf + sFrame.len);
                sFrame.len += WLAN_CHALLENGE_IE_LEN;
                sFrame.pChallenge->byElementID = WLAN_EID_CHALLENGE;
                sFrame.pChallenge->len = WLAN_CHALLENGE_LEN;
                memcpy( sFrame.pChallenge->abyChallenge, pFrame->pChallenge->abyChallenge, WLAN_CHALLENGE_LEN);
                // Adjust the length fields
                pTxPacket->cbMPDULen = sFrame.len;
                pTxPacket->cbPayloadLen = sFrame.len - WLAN_HDR_ADDR3_LEN;
                // send the frame
                if (csMgmt_xmit(pDevice, pTxPacket) != CMD_STATUS_PENDING) {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Auth_reply sequence_2 tx failed.\n");
                }
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Auth_reply sequence_2 tx ...\n");
            }
            else {
            	DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:rx Auth_reply sequence_2 status error ...\n");
                if ( pDevice->eCommandState == WLAN_AUTHENTICATE_WAIT ) {
//                    spin_unlock_irq(&pDevice->lock);
//                    vCommandTimerWait((HANDLE)pDevice, 0);
//                    spin_lock_irq(&pDevice->lock);
                }
                s_vMgrLogStatus(pMgmt, cpu_to_le16((*(pFrame->pwStatus))));
            }
            break;
        default:
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt: rx auth.seq = 2 unknown AuthAlgorithm=%d\n", cpu_to_le16((*(pFrame->pwAuthAlgorithm))));
            break;
    }
    return;
}



/*+
 *
 * Routine Description:
 *   Handles incoming authen frames with sequence 3.  Currently
 *   assumes we're an AP.  This function assumes the frame has
 *   already been successfully decrypted.
 *
 *
 * Return Value:
 *    None.
 *
-*/

static
VOID
s_vMgrRxAuthenSequence_3(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PWLAN_FR_AUTHEN pFrame
    )
{
    PSTxMgmtPacket      pTxPacket = NULL;
    UINT                uStatusCode = 0 ;
    UINT                uNodeIndex = 0;
    WLAN_FR_AUTHEN      sFrame;

    if (!WLAN_GET_FC_ISWEP(pFrame->pHdr->sA3.wFrameCtl)) {
        uStatusCode = WLAN_MGMT_STATUS_CHALLENGE_FAIL;
        goto reply;
    }
    if (BSSbIsSTAInNodeDB(pDevice, pFrame->pHdr->sA3.abyAddr2, &uNodeIndex)) {
         if (pMgmt->sNodeDBTable[uNodeIndex].byAuthSequence != 1) {
            uStatusCode = WLAN_MGMT_STATUS_RX_AUTH_NOSEQ;
            goto reply;
         }
         if (memcmp(pMgmt->abyChallenge, pFrame->pChallenge->abyChallenge, WLAN_CHALLENGE_LEN) != 0) {
            uStatusCode = WLAN_MGMT_STATUS_CHALLENGE_FAIL;
            goto reply;
         }
    }
    else {
        uStatusCode = WLAN_MGMT_STATUS_UNSPEC_FAILURE;
        goto reply;
    }

    if (uNodeIndex) {
        pMgmt->sNodeDBTable[uNodeIndex].eNodeState = NODE_AUTH;
        pMgmt->sNodeDBTable[uNodeIndex].byAuthSequence = 0;
    }
    uStatusCode = WLAN_MGMT_STATUS_SUCCESS;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Challenge text check ok..\n");

reply:
    // send auth reply
    pTxPacket = (PSTxMgmtPacket)pMgmt->pbyMgmtPacketPool;
    memset(pTxPacket, 0, sizeof(STxMgmtPacket) + WLAN_AUTHEN_FR_MAXLEN);
    pTxPacket->p80211Header = (PUWLAN_80211HDR)((PBYTE)pTxPacket + sizeof(STxMgmtPacket));
    sFrame.pBuf = (PBYTE)pTxPacket->p80211Header;
    sFrame.len = WLAN_AUTHEN_FR_MAXLEN;
    // format buffer structure
    vMgrEncodeAuthen(&sFrame);
    /* insert values */
    sFrame.pHdr->sA3.wFrameCtl = cpu_to_le16(
         (
         WLAN_SET_FC_FTYPE(WLAN_TYPE_MGR) |
         WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN)|
         WLAN_SET_FC_ISWEP(0)
         ));
    memcpy( sFrame.pHdr->sA3.abyAddr1, pFrame->pHdr->sA3.abyAddr2, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);
    *(sFrame.pwAuthAlgorithm) = *(pFrame->pwAuthAlgorithm);
    *(sFrame.pwAuthSequence) = cpu_to_le16(4);
    *(sFrame.pwStatus) = cpu_to_le16(uStatusCode);

    /* Adjust the length fields */
    pTxPacket->cbMPDULen = sFrame.len;
    pTxPacket->cbPayloadLen = sFrame.len - WLAN_HDR_ADDR3_LEN;
    // send the frame
    if (pDevice->bEnableHostapd) {
        return;
    }
    if (csMgmt_xmit(pDevice, pTxPacket) != CMD_STATUS_PENDING) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Authreq_reply sequence_4 tx failed.\n");
    }
    return;

}



/*+
 *
 * Routine Description:
 *   Handles incoming authen frames with sequence 4
 *
 *
 * Return Value:
 *    None.
 *
-*/
static
VOID
s_vMgrRxAuthenSequence_4(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PWLAN_FR_AUTHEN pFrame
    )
{

    if ( cpu_to_le16((*(pFrame->pwStatus))) == WLAN_MGMT_STATUS_SUCCESS ){
        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "802.11 Authen (SHAREDKEY) Successful.\n");
        pMgmt->eCurrState = WMAC_STATE_AUTH;
        timer_expire(pDevice->sTimerCommand, 0);
    }
    else{
        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "802.11 Authen (SHAREDKEY) Failed.\n");
        s_vMgrLogStatus(pMgmt, cpu_to_le16((*(pFrame->pwStatus))) );
        pMgmt->eCurrState = WMAC_STATE_IDLE;
    }

    if ( pDevice->eCommandState == WLAN_AUTHENTICATE_WAIT ) {
//        spin_unlock_irq(&pDevice->lock);
//        vCommandTimerWait((HANDLE)pDevice, 0);
//        spin_lock_irq(&pDevice->lock);
    }

}

/*+
 *
 * Routine Description:
 *   Handles incoming disassociation frames
 *
 *
 * Return Value:
 *    None.
 *
-*/

static
VOID
s_vMgrRxDisassociation(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket
    )
{
    WLAN_FR_DISASSOC    sFrame;
    UINT        uNodeIndex = 0;
    CMD_STATUS          CmdStatus;
    viawget_wpa_header *wpahdr;

    if ( pMgmt->eCurrMode == WMAC_MODE_ESS_AP ){
        // if is acting an AP..
        // a STA is leaving this BSS..
        sFrame.len = pRxPacket->cbMPDULen;
        sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
        if (BSSbIsSTAInNodeDB(pDevice, pRxPacket->p80211Header->sA3.abyAddr2, &uNodeIndex)) {
            BSSvRemoveOneNode(pDevice, uNodeIndex);
        }
        else {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Rx disassoc, sta not found\n");
        }
    }
    else if (pMgmt->eCurrMode == WMAC_MODE_ESS_STA ){
        sFrame.len = pRxPacket->cbMPDULen;
        sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
        vMgrDecodeDisassociation(&sFrame);
        DBG_PRT(MSG_LEVEL_NOTICE, KERN_INFO "AP disassociated me, reason=%d.\n", cpu_to_le16(*(sFrame.pwReason)));

          pDevice->fWPA_Authened = FALSE;
	if ((pDevice->bWPADEVUp) && (pDevice->skb != NULL)) {
             wpahdr = (viawget_wpa_header *)pDevice->skb->data;
             wpahdr->type = VIAWGET_DISASSOC_MSG;
             wpahdr->resp_ie_len = 0;
             wpahdr->req_ie_len = 0;
             skb_put(pDevice->skb, sizeof(viawget_wpa_header));
             pDevice->skb->dev = pDevice->wpadev;
	     skb_reset_mac_header(pDevice->skb);
             pDevice->skb->pkt_type = PACKET_HOST;
             pDevice->skb->protocol = htons(ETH_P_802_2);
             memset(pDevice->skb->cb, 0, sizeof(pDevice->skb->cb));
             netif_rx(pDevice->skb);
             pDevice->skb = dev_alloc_skb((int)pDevice->rx_buf_sz);
         };

        //TODO: do something let upper layer know or
        //try to send associate packet again because of inactivity timeout
        if (pMgmt->eCurrState == WMAC_STATE_ASSOC) {
                pDevice->bLinkPass = FALSE;
                pMgmt->sNodeDBTable[0].bActive = FALSE;
	       pDevice->byReAssocCount = 0;
                pMgmt->eCurrState = WMAC_STATE_AUTH;  // jump back to the auth state!
                pDevice->eCommandState = WLAN_ASSOCIATE_WAIT;
            vMgrReAssocBeginSta((PSDevice)pDevice, pMgmt, &CmdStatus);
              if(CmdStatus == CMD_STATUS_PENDING) {
		  pDevice->byReAssocCount ++;
		  return;       //mike add: you'll retry for many times, so it cann't be regarded as disconnected!
              }
        };

   #ifdef WPA_SUPPLICANT_DRIVER_WEXT_SUPPORT
  // if(pDevice->bWPASuppWextEnabled == TRUE)
      {
	union iwreq_data  wrqu;
	memset(&wrqu, 0, sizeof (wrqu));
        wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	PRINT_K("wireless_send_event--->SIOCGIWAP(disassociated)\n");
	wireless_send_event(pDevice->dev, SIOCGIWAP, &wrqu, NULL);
     }
  #endif
    }
    /* else, ignore it */

    return;
}


/*+
 *
 * Routine Description:
 *   Handles incoming deauthentication frames
 *
 *
 * Return Value:
 *    None.
 *
-*/

static
VOID
s_vMgrRxDeauthentication(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket
    )
{
    WLAN_FR_DEAUTHEN    sFrame;
    UINT        uNodeIndex = 0;
    viawget_wpa_header *wpahdr;


    if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP ){
        //Todo:
        // if is acting an AP..
        // a STA is leaving this BSS..
        sFrame.len = pRxPacket->cbMPDULen;
        sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
        if (BSSbIsSTAInNodeDB(pDevice, pRxPacket->p80211Header->sA3.abyAddr2, &uNodeIndex)) {
            BSSvRemoveOneNode(pDevice, uNodeIndex);
        }
        else {
            DBG_PRT(MSG_LEVEL_NOTICE, KERN_INFO "Rx deauth, sta not found\n");
        }
    }
    else {
        if (pMgmt->eCurrMode == WMAC_MODE_ESS_STA ) {
            sFrame.len = pRxPacket->cbMPDULen;
            sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
            vMgrDecodeDeauthen(&sFrame);
	   pDevice->fWPA_Authened = FALSE;
            DBG_PRT(MSG_LEVEL_NOTICE, KERN_INFO  "AP deauthed me, reason=%d.\n", cpu_to_le16((*(sFrame.pwReason))));
            // TODO: update BSS list for specific BSSID if pre-authentication case
            if (IS_ETH_ADDRESS_EQUAL(sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID)) {
                if (pMgmt->eCurrState >= WMAC_STATE_AUTHPENDING) {
                    pMgmt->sNodeDBTable[0].bActive = FALSE;
                    pMgmt->eCurrMode = WMAC_MODE_STANDBY;
                    pMgmt->eCurrState = WMAC_STATE_IDLE;
                    netif_stop_queue(pDevice->dev);
                    pDevice->bLinkPass = FALSE;
                    ControlvMaskByte(pDevice,MESSAGE_REQUEST_MACREG,MAC_REG_PAPEDELAY,LEDSTS_STS,LEDSTS_SLOW);
                }
            };

            if ((pDevice->bWPADEVUp) && (pDevice->skb != NULL)) {
                 wpahdr = (viawget_wpa_header *)pDevice->skb->data;
                 wpahdr->type = VIAWGET_DISASSOC_MSG;
                 wpahdr->resp_ie_len = 0;
                 wpahdr->req_ie_len = 0;
                 skb_put(pDevice->skb, sizeof(viawget_wpa_header));
                 pDevice->skb->dev = pDevice->wpadev;
		 skb_reset_mac_header(pDevice->skb);
                 pDevice->skb->pkt_type = PACKET_HOST;
                 pDevice->skb->protocol = htons(ETH_P_802_2);
                 memset(pDevice->skb->cb, 0, sizeof(pDevice->skb->cb));
                 netif_rx(pDevice->skb);
                 pDevice->skb = dev_alloc_skb((int)pDevice->rx_buf_sz);
           };

   #ifdef WPA_SUPPLICANT_DRIVER_WEXT_SUPPORT
  // if(pDevice->bWPASuppWextEnabled == TRUE)
      {
	union iwreq_data  wrqu;
	memset(&wrqu, 0, sizeof (wrqu));
        wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	PRINT_K("wireless_send_event--->SIOCGIWAP(disauthen)\n");
	wireless_send_event(pDevice->dev, SIOCGIWAP, &wrqu, NULL);
     }
  #endif

        }
        /* else, ignore it.  TODO: IBSS authentication service
            would be implemented here */
    };
    return;
}

//2008-0730-01<Add>by MikeLiu
/*+
 *
 * Routine Description:
 * check if current channel is match ZoneType.
 *for USA:1~11;
 *      Japan:1~13;
 *      Europe:1~13
 * Return Value:
 *               True:exceed;
 *                False:normal case
-*/
static BOOL
ChannelExceedZoneType(
    IN PSDevice pDevice,
    IN BYTE byCurrChannel
    )
{
  BOOL exceed=FALSE;

  switch(pDevice->byZoneType) {
  	case 0x00:                  //USA:1~11
                     if((byCurrChannel<1) ||(byCurrChannel>11))
	                exceed = TRUE;
	         break;
	case 0x01:                  //Japan:1~13
	case 0x02:                  //Europe:1~13
                     if((byCurrChannel<1) ||(byCurrChannel>13))
	                exceed = TRUE;
	         break;
	default:                    //reserve for other zonetype
		break;
  }

  return exceed;
}

/*+
 *
 * Routine Description:
 *   Handles and analysis incoming beacon frames.
 *
 *
 * Return Value:
 *    None.
 *
-*/

static
VOID
s_vMgrRxBeacon(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket,
    IN BOOL bInScan
    )
{

    PKnownBSS           pBSSList;
    WLAN_FR_BEACON      sFrame;
    QWORD               qwTSFOffset;
    BOOL                bIsBSSIDEqual = FALSE;
    BOOL                bIsSSIDEqual = FALSE;
    BOOL                bTSFLargeDiff = FALSE;
    BOOL                bTSFOffsetPostive = FALSE;
    BOOL                bUpdateTSF = FALSE;
    BOOL                bIsAPBeacon = FALSE;
    BOOL                bIsChannelEqual = FALSE;
    UINT                uLocateByteIndex;
    BYTE                byTIMBitOn = 0;
    WORD                wAIDNumber = 0;
    UINT                uNodeIndex;
    QWORD               qwTimestamp, qwLocalTSF;
    QWORD               qwCurrTSF;
    WORD                wStartIndex = 0;
    WORD                wAIDIndex = 0;
    BYTE                byCurrChannel = pRxPacket->byRxChannel;
    ERPObject           sERP;
    UINT                uRateLen = WLAN_RATES_MAXLEN;
    BOOL                bChannelHit = FALSE;
    BYTE                byOldPreambleType;



     if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP)
        return;

    memset(&sFrame, 0, sizeof(WLAN_FR_BEACON));
    sFrame.len = pRxPacket->cbMPDULen;
    sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;

    // decode the beacon frame
    vMgrDecodeBeacon(&sFrame);

    if ((sFrame.pwBeaconInterval == 0) ||
        (sFrame.pwCapInfo == 0) ||
        (sFrame.pSSID == 0) ||
        (sFrame.pSuppRates == 0) ) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Rx beacon frame error\n");
        return;
    };


    if( byCurrChannel > CB_MAX_CHANNEL_24G )
    {
        if (sFrame.pDSParms != NULL) {
            if (byCurrChannel == RFaby11aChannelIndex[sFrame.pDSParms->byCurrChannel-1])
                bChannelHit = TRUE;
            byCurrChannel = RFaby11aChannelIndex[sFrame.pDSParms->byCurrChannel-1];
        } else {
            bChannelHit = TRUE;
        }

    } else {
        if (sFrame.pDSParms != NULL) {
            if (byCurrChannel == sFrame.pDSParms->byCurrChannel)
                bChannelHit = TRUE;
            byCurrChannel = sFrame.pDSParms->byCurrChannel;
        } else {
            bChannelHit = TRUE;
        }
    }

//2008-0730-01<Add>by MikeLiu
if(ChannelExceedZoneType(pDevice,byCurrChannel)==TRUE)
      return;

    if (sFrame.pERP != NULL) {
        sERP.byERP = sFrame.pERP->byContext;
        sERP.bERPExist = TRUE;

    } else {
        sERP.bERPExist = FALSE;
        sERP.byERP = 0;
    }

    pBSSList = BSSpAddrIsInBSSList((HANDLE)pDevice, sFrame.pHdr->sA3.abyAddr3, sFrame.pSSID);
    if (pBSSList == NULL) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Beacon/insert: RxChannel = : %d\n", byCurrChannel);
        BSSbInsertToBSSList((HANDLE)pDevice,
                            sFrame.pHdr->sA3.abyAddr3,
                            *sFrame.pqwTimestamp,
                            *sFrame.pwBeaconInterval,
                            *sFrame.pwCapInfo,
                            byCurrChannel,
                            sFrame.pSSID,
                            sFrame.pSuppRates,
                            sFrame.pExtSuppRates,
                            &sERP,
                            sFrame.pRSN,
                            sFrame.pRSNWPA,
                            sFrame.pIE_Country,
                            sFrame.pIE_Quiet,
                            sFrame.len - WLAN_HDR_ADDR3_LEN,
                            sFrame.pHdr->sA4.abyAddr4,   // payload of beacon
                            (HANDLE)pRxPacket
                           );
    }
    else {
//        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"update bcn: RxChannel = : %d\n", byCurrChannel);
        BSSbUpdateToBSSList((HANDLE)pDevice,
                            *sFrame.pqwTimestamp,
                            *sFrame.pwBeaconInterval,
                            *sFrame.pwCapInfo,
                            byCurrChannel,
                            bChannelHit,
                            sFrame.pSSID,
                            sFrame.pSuppRates,
                            sFrame.pExtSuppRates,
                            &sERP,
                            sFrame.pRSN,
                            sFrame.pRSNWPA,
                            sFrame.pIE_Country,
                            sFrame.pIE_Quiet,
                            pBSSList,
                            sFrame.len - WLAN_HDR_ADDR3_LEN,
                            sFrame.pHdr->sA4.abyAddr4,   // payload of probresponse
                            (HANDLE)pRxPacket
                           );

    }

    if (bInScan) {
        return;
    }

    if(byCurrChannel == (BYTE)pMgmt->uCurrChannel)
       bIsChannelEqual = TRUE;

    if (bIsChannelEqual && (pMgmt->eCurrMode == WMAC_MODE_ESS_AP)) {

        // if rx beacon without ERP field
        if (sERP.bERPExist) {
            if (WLAN_GET_ERP_USE_PROTECTION(sERP.byERP)){
                pDevice->byERPFlag |= WLAN_SET_ERP_USE_PROTECTION(1);
                pDevice->wUseProtectCntDown = USE_PROTECT_PERIOD;
            }
        }
        else {
            pDevice->byERPFlag |= WLAN_SET_ERP_USE_PROTECTION(1);
            pDevice->wUseProtectCntDown = USE_PROTECT_PERIOD;
        }

        if (pMgmt->eCurrMode == WMAC_MODE_IBSS_STA) {
            if(!WLAN_GET_CAP_INFO_SHORTPREAMBLE(*sFrame.pwCapInfo))
                pDevice->byERPFlag |= WLAN_SET_ERP_BARKER_MODE(1);
            if(!sERP.bERPExist)
                pDevice->byERPFlag |= WLAN_SET_ERP_NONERP_PRESENT(1);
        }
    }

    // check if BSSID the same
    if (memcmp(sFrame.pHdr->sA3.abyAddr3,
               pMgmt->abyCurrBSSID,
               WLAN_BSSID_LEN) == 0) {

        bIsBSSIDEqual = TRUE;
        pDevice->uCurrRSSI = pRxPacket->uRSSI;
        pDevice->byCurrSQ = pRxPacket->bySQ;
        if (pMgmt->sNodeDBTable[0].uInActiveCount != 0) {
            pMgmt->sNodeDBTable[0].uInActiveCount = 0;
            //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"BCN:Wake Count= [%d]\n", pMgmt->wCountToWakeUp);
        }
    }
    // check if SSID the same
    if (sFrame.pSSID->len == ((PWLAN_IE_SSID)pMgmt->abyCurrSSID)->len) {
        if (memcmp(sFrame.pSSID->abySSID,
                   ((PWLAN_IE_SSID)pMgmt->abyCurrSSID)->abySSID,
                   sFrame.pSSID->len
                   ) == 0) {
            bIsSSIDEqual = TRUE;
        };
    }

    if ((WLAN_GET_CAP_INFO_ESS(*sFrame.pwCapInfo)== TRUE) &&
        (bIsBSSIDEqual == TRUE) &&
        (bIsSSIDEqual == TRUE) &&
        (pMgmt->eCurrMode == WMAC_MODE_ESS_STA) &&
        (pMgmt->eCurrState == WMAC_STATE_ASSOC)) {
        // add state check to prevent reconnect fail since we'll receive Beacon

        bIsAPBeacon = TRUE;
        if (pBSSList != NULL) {

                // Sync ERP field
                if ((pBSSList->sERP.bERPExist == TRUE) && (pDevice->byBBType == BB_TYPE_11G)) {
                    if ((pBSSList->sERP.byERP & WLAN_EID_ERP_USE_PROTECTION) != pDevice->bProtectMode) {//0000 0010
                        pDevice->bProtectMode = (pBSSList->sERP.byERP & WLAN_EID_ERP_USE_PROTECTION);
                        if (pDevice->bProtectMode) {
                            MACvEnableProtectMD(pDevice);
                        } else {
                            MACvDisableProtectMD(pDevice);
                        }
                        vUpdateIFS(pDevice);
                    }
                    if ((pBSSList->sERP.byERP & WLAN_EID_ERP_NONERP_PRESENT) != pDevice->bNonERPPresent) {//0000 0001
                        pDevice->bNonERPPresent = (pBSSList->sERP.byERP & WLAN_EID_ERP_USE_PROTECTION);
                    }
                    if ((pBSSList->sERP.byERP & WLAN_EID_ERP_BARKER_MODE) != pDevice->bBarkerPreambleMd) {//0000 0100
                        pDevice->bBarkerPreambleMd = (pBSSList->sERP.byERP & WLAN_EID_ERP_BARKER_MODE);
                        //BarkerPreambleMd has higher priority than shortPreamble bit in Cap
                        if (pDevice->bBarkerPreambleMd) {
                            MACvEnableBarkerPreambleMd(pDevice);
                        } else {
                            MACvDisableBarkerPreambleMd(pDevice);
                        }
                    }
                }
                // Sync Short Slot Time
                if (WLAN_GET_CAP_INFO_SHORTSLOTTIME(pBSSList->wCapInfo) != pDevice->bShortSlotTime) {
                    BOOL    bShortSlotTime;

                    bShortSlotTime = WLAN_GET_CAP_INFO_SHORTSLOTTIME(pBSSList->wCapInfo);
                    //DBG_PRN_WLAN05(("Set Short Slot Time: %d\n", pDevice->bShortSlotTime));
                    //Kyle check if it is OK to set G.
                    if (pDevice->byBBType == BB_TYPE_11A) {
                        bShortSlotTime = TRUE;
                    }
                    else if (pDevice->byBBType == BB_TYPE_11B) {
                        bShortSlotTime = FALSE;
                    }
                    if (bShortSlotTime != pDevice->bShortSlotTime) {
                        pDevice->bShortSlotTime = bShortSlotTime;
                        BBvSetShortSlotTime(pDevice);
                        vUpdateIFS(pDevice);
                    }
                }

                //
                // Preamble may change dynamiclly
                //
                byOldPreambleType = pDevice->byPreambleType;
                if (WLAN_GET_CAP_INFO_SHORTPREAMBLE(pBSSList->wCapInfo)) {
                    pDevice->byPreambleType = pDevice->byShortPreamble;
                }
                else {
                    pDevice->byPreambleType = 0;
                }
                if (pDevice->byPreambleType != byOldPreambleType)
                    CARDvSetRSPINF(pDevice, (BYTE)pDevice->byBBType);
            //
            // Basic Rate Set may change dynamiclly
            //
            if (pBSSList->eNetworkTypeInUse == PHY_TYPE_11B) {
                uRateLen = WLAN_RATES_MAXLEN_11B;
            }
            pMgmt->abyCurrSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)pBSSList->abySuppRates,
                                                    (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                                                    uRateLen);
            pMgmt->abyCurrExtSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)pBSSList->abyExtSuppRates,
                                                    (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates,
                                                    uRateLen);
            RATEvParseMaxRate( (PVOID)pDevice,
                               (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                               (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates,
                               TRUE,
                               &(pMgmt->sNodeDBTable[0].wMaxBasicRate),
                               &(pMgmt->sNodeDBTable[0].wMaxSuppRate),
                               &(pMgmt->sNodeDBTable[0].wSuppRate),
                               &(pMgmt->sNodeDBTable[0].byTopCCKBasicRate),
                               &(pMgmt->sNodeDBTable[0].byTopOFDMBasicRate)
                              );

        }
    }

//    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Beacon 2 \n");
    // check if CF field exisit
    if (WLAN_GET_CAP_INFO_ESS(*sFrame.pwCapInfo)) {
        if (sFrame.pCFParms->wCFPDurRemaining > 0) {
            // TODO: deal with CFP period to set NAV
        };
    };

    HIDWORD(qwTimestamp) = cpu_to_le32(HIDWORD(*sFrame.pqwTimestamp));
    LODWORD(qwTimestamp) = cpu_to_le32(LODWORD(*sFrame.pqwTimestamp));
    HIDWORD(qwLocalTSF) = HIDWORD(pRxPacket->qwLocalTSF);
    LODWORD(qwLocalTSF) = LODWORD(pRxPacket->qwLocalTSF);

    // check if beacon TSF larger or small than our local TSF
    if (HIDWORD(qwTimestamp) == HIDWORD(qwLocalTSF)) {
        if (LODWORD(qwTimestamp) >= LODWORD(qwLocalTSF)) {
            bTSFOffsetPostive = TRUE;
        }
        else {
            bTSFOffsetPostive = FALSE;
        }
    }
    else if (HIDWORD(qwTimestamp) > HIDWORD(qwLocalTSF)) {
        bTSFOffsetPostive = TRUE;
    }
    else if (HIDWORD(qwTimestamp) < HIDWORD(qwLocalTSF)) {
        bTSFOffsetPostive = FALSE;
    };

    if (bTSFOffsetPostive) {
        qwTSFOffset = CARDqGetTSFOffset(pRxPacket->byRxRate, (qwTimestamp), (qwLocalTSF));
    }
    else {
        qwTSFOffset = CARDqGetTSFOffset(pRxPacket->byRxRate, (qwLocalTSF), (qwTimestamp));
    }

    if (HIDWORD(qwTSFOffset) != 0 ||
        (LODWORD(qwTSFOffset) > TRIVIAL_SYNC_DIFFERENCE )) {
         bTSFLargeDiff = TRUE;
    }


    // if infra mode
    if (bIsAPBeacon == TRUE) {

        // Infra mode: Local TSF always follow AP's TSF if Difference huge.
        if (bTSFLargeDiff)
            bUpdateTSF = TRUE;

        if ((pDevice->bEnablePSMode == TRUE) &&(sFrame.pTIM != 0)) {

            // deal with DTIM, analysis TIM
            pMgmt->bMulticastTIM = WLAN_MGMT_IS_MULTICAST_TIM(sFrame.pTIM->byBitMapCtl) ? TRUE : FALSE ;
            pMgmt->byDTIMCount = sFrame.pTIM->byDTIMCount;
            pMgmt->byDTIMPeriod = sFrame.pTIM->byDTIMPeriod;
            wAIDNumber = pMgmt->wCurrAID & ~(BIT14|BIT15);

            // check if AID in TIM field bit on
            // wStartIndex = N1
            wStartIndex = WLAN_MGMT_GET_TIM_OFFSET(sFrame.pTIM->byBitMapCtl) << 1;
            // AIDIndex = N2
            wAIDIndex = (wAIDNumber >> 3);
            if ((wAIDNumber > 0) && (wAIDIndex >= wStartIndex)) {
                uLocateByteIndex = wAIDIndex - wStartIndex;
                // len = byDTIMCount + byDTIMPeriod + byDTIMPeriod + byVirtBitMap[0~250]
                if (sFrame.pTIM->len >= (uLocateByteIndex + 4)) {
                    byTIMBitOn  = (0x01) << ((wAIDNumber) % 8);
                    pMgmt->bInTIM = sFrame.pTIM->byVirtBitMap[uLocateByteIndex] & byTIMBitOn ? TRUE : FALSE;
                }
                else {
                    pMgmt->bInTIM = FALSE;
                };
            }
            else {
                pMgmt->bInTIM = FALSE;
            };

            if (pMgmt->bInTIM ||
                (pMgmt->bMulticastTIM && (pMgmt->byDTIMCount == 0))) {
                pMgmt->bInTIMWake = TRUE;
                // send out ps-poll packet
//                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "BCN:In TIM\n");
                if (pMgmt->bInTIM) {
                    PSvSendPSPOLL((PSDevice)pDevice);
//                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "BCN:PS-POLL sent..\n");
                };

            }
            else {
                pMgmt->bInTIMWake = FALSE;
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "BCN: Not In TIM..\n");
                if (pDevice->bPWBitOn == FALSE) {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "BCN: Send Null Packet\n");
                    if (PSbSendNullPacket(pDevice))
                        pDevice->bPWBitOn = TRUE;
                }
                if(PSbConsiderPowerDown(pDevice, FALSE, FALSE)) {
                   DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "BCN: Power down now...\n");
                };
            }

        }

    }
    // if adhoc mode
    if ((pMgmt->eCurrMode == WMAC_MODE_IBSS_STA) && !bIsAPBeacon && bIsChannelEqual) {
        if (bIsBSSIDEqual) {
            // Use sNodeDBTable[0].uInActiveCount as IBSS beacons received count.
		    if (pMgmt->sNodeDBTable[0].uInActiveCount != 0)
		 	    pMgmt->sNodeDBTable[0].uInActiveCount = 0;

            // adhoc mode:TSF updated only when beacon larger then local TSF
            if (bTSFLargeDiff && bTSFOffsetPostive &&
                (pMgmt->eCurrState == WMAC_STATE_JOINTED))
                bUpdateTSF = TRUE;

            // During dpc, already in spinlocked.
            if (BSSbIsSTAInNodeDB(pDevice, sFrame.pHdr->sA3.abyAddr2, &uNodeIndex)) {

                // Update the STA, (Techically the Beacons of all the IBSS nodes
		        // should be identical, but that's not happening in practice.
                pMgmt->abyCurrSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pSuppRates,
                                                        (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                                                        WLAN_RATES_MAXLEN_11B);
                RATEvParseMaxRate( (PVOID)pDevice,
                                   (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                                   NULL,
                                   TRUE,
                                   &(pMgmt->sNodeDBTable[uNodeIndex].wMaxBasicRate),
                                   &(pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate),
                                   &(pMgmt->sNodeDBTable[uNodeIndex].wSuppRate),
                                   &(pMgmt->sNodeDBTable[uNodeIndex].byTopCCKBasicRate),
                                   &(pMgmt->sNodeDBTable[uNodeIndex].byTopOFDMBasicRate)
                                  );
                pMgmt->sNodeDBTable[uNodeIndex].bShortPreamble = WLAN_GET_CAP_INFO_SHORTPREAMBLE(*sFrame.pwCapInfo);
                pMgmt->sNodeDBTable[uNodeIndex].bShortSlotTime = WLAN_GET_CAP_INFO_SHORTSLOTTIME(*sFrame.pwCapInfo);
                pMgmt->sNodeDBTable[uNodeIndex].uInActiveCount = 0;
            }
            else {
                // Todo, initial Node content
                BSSvCreateOneNode((PSDevice)pDevice, &uNodeIndex);

                pMgmt->abyCurrSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pSuppRates,
                                                        (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                                                        WLAN_RATES_MAXLEN_11B);
                RATEvParseMaxRate( (PVOID)pDevice,
                                   (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                                   NULL,
                                   TRUE,
                                   &(pMgmt->sNodeDBTable[uNodeIndex].wMaxBasicRate),
                                   &(pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate),
                                   &(pMgmt->sNodeDBTable[uNodeIndex].wSuppRate),
                                   &(pMgmt->sNodeDBTable[uNodeIndex].byTopCCKBasicRate),
                                   &(pMgmt->sNodeDBTable[uNodeIndex].byTopOFDMBasicRate)
                                 );

                memcpy(pMgmt->sNodeDBTable[uNodeIndex].abyMACAddr, sFrame.pHdr->sA3.abyAddr2, WLAN_ADDR_LEN);
                pMgmt->sNodeDBTable[uNodeIndex].bShortPreamble = WLAN_GET_CAP_INFO_SHORTPREAMBLE(*sFrame.pwCapInfo);
                pMgmt->sNodeDBTable[uNodeIndex].wTxDataRate = pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate;
/*
                pMgmt->sNodeDBTable[uNodeIndex].bShortSlotTime = WLAN_GET_CAP_INFO_SHORTSLOTTIME(*sFrame.pwCapInfo);
                if(pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate > RATE_11M)
                       pMgmt->sNodeDBTable[uNodeIndex].bERPExist = TRUE;
*/
            }

            // if other stations jointed, indicate connect to upper layer..
            if (pMgmt->eCurrState == WMAC_STATE_STARTED) {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Current IBSS State: [Started]........to: [Jointed] \n");
                pMgmt->eCurrState = WMAC_STATE_JOINTED;
                pDevice->bLinkPass = TRUE;
                ControlvMaskByte(pDevice,MESSAGE_REQUEST_MACREG,MAC_REG_PAPEDELAY,LEDSTS_STS,LEDSTS_INTER);
                if (netif_queue_stopped(pDevice->dev)){
                    netif_wake_queue(pDevice->dev);
                }
                pMgmt->sNodeDBTable[0].bActive = TRUE;
                pMgmt->sNodeDBTable[0].uInActiveCount = 0;

            };
        }
        else if (bIsSSIDEqual) {

            // See other adhoc sta with the same SSID but BSSID is different.
            // adpot this vars only when TSF larger then us.
            if (bTSFLargeDiff && bTSFOffsetPostive) {
                 // we don't support ATIM under adhoc mode
               // if ( sFrame.pIBSSParms->wATIMWindow == 0) {
                     // adpot this vars
                     // TODO: check sFrame cap if privacy on, and support rate syn
                     memcpy(pMgmt->abyCurrBSSID, sFrame.pHdr->sA3.abyAddr3, WLAN_BSSID_LEN);
                     memcpy(pDevice->abyBSSID, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);
                     pMgmt->wCurrATIMWindow = cpu_to_le16(sFrame.pIBSSParms->wATIMWindow);
                     pMgmt->wCurrBeaconPeriod = cpu_to_le16(*sFrame.pwBeaconInterval);
                     pMgmt->abyCurrSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pSuppRates,
                                                      (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                                                      WLAN_RATES_MAXLEN_11B);
                     // set HW beacon interval and re-synchronizing....
                     DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Rejoining to Other Adhoc group with same SSID........\n");

                     MACvWriteBeaconInterval(pDevice, pMgmt->wCurrBeaconPeriod);
                     CARDvAdjustTSF(pDevice, pRxPacket->byRxRate, qwTimestamp, pRxPacket->qwLocalTSF);
                     CARDvUpdateNextTBTT(pDevice, qwTimestamp, pMgmt->wCurrBeaconPeriod);

                     // Turn off bssid filter to avoid filter others adhoc station which bssid is different.
                     MACvWriteBSSIDAddress(pDevice, pMgmt->abyCurrBSSID);

                    byOldPreambleType = pDevice->byPreambleType;
                    if (WLAN_GET_CAP_INFO_SHORTPREAMBLE(*sFrame.pwCapInfo)) {
                        pDevice->byPreambleType = pDevice->byShortPreamble;
                    }
                    else {
                        pDevice->byPreambleType = 0;
                    }
                    if (pDevice->byPreambleType != byOldPreambleType)
                        CARDvSetRSPINF(pDevice, (BYTE)pDevice->byBBType);


                     // MACvRegBitsOff(pDevice->PortOffset, MAC_REG_RCR, RCR_BSSID);
                     // set highest basic rate
                     // s_vSetHighestBasicRate(pDevice, (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates);
                     // Prepare beacon frame
                     bMgrPrepareBeaconToSend((HANDLE)pDevice, pMgmt);
              //  }
            };
        }
    };
    // endian issue ???
    // Update TSF
    if (bUpdateTSF) {
        CARDbGetCurrentTSF(pDevice, &qwCurrTSF);
        CARDvAdjustTSF(pDevice, pRxPacket->byRxRate, qwTimestamp , pRxPacket->qwLocalTSF);
        CARDbGetCurrentTSF(pDevice, &qwCurrTSF);
        CARDvUpdateNextTBTT(pDevice, qwTimestamp, pMgmt->wCurrBeaconPeriod);
    }

    return;
}



/*+
 *
 * Routine Description:
 *   Instructs the hw to create a bss using the supplied
 *   attributes. Note that this implementation only supports Ad-Hoc
 *   BSS creation.
 *
 *
 * Return Value:
 *    CMD_STATUS
 *
-*/
VOID
vMgrCreateOwnIBSS(
    IN  HANDLE hDeviceContext,
    OUT PCMD_STATUS pStatus
    )
{
    PSDevice            pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject        pMgmt = &(pDevice->sMgmtObj);
    WORD                wMaxBasicRate;
    WORD                wMaxSuppRate;
    BYTE                byTopCCKBasicRate;
    BYTE                byTopOFDMBasicRate;
    QWORD               qwCurrTSF;
    UINT                ii;
    BYTE    abyRATE[] = {0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C, 0x0C, 0x12, 0x18, 0x60};
    BYTE    abyCCK_RATE[] = {0x82, 0x84, 0x8B, 0x96};
    BYTE    abyOFDM_RATE[] = {0x0C, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6C};
    WORD                wSuppRate;



    HIDWORD(qwCurrTSF) = 0;
    LODWORD(qwCurrTSF) = 0;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Create Basic Service Set .......\n");

    if (pMgmt->eConfigMode == WMAC_CONFIG_IBSS_STA) {
        if ((pMgmt->eAuthenMode == WMAC_AUTH_WPANONE) &&
            (pDevice->eEncryptionStatus != Ndis802_11Encryption2Enabled) &&
            (pDevice->eEncryptionStatus != Ndis802_11Encryption3Enabled)) {
            // encryption mode error
            *pStatus = CMD_STATUS_FAILURE;
            return;
        }
    }

    pMgmt->abyCurrSuppRates[0] = WLAN_EID_SUPP_RATES;
    pMgmt->abyCurrExtSuppRates[0] = WLAN_EID_EXTSUPP_RATES;

    if (pMgmt->eConfigMode == WMAC_CONFIG_AP) {
        pMgmt->eCurrentPHYMode = pMgmt->byAPBBType;
    } else {
        if (pDevice->byBBType == BB_TYPE_11G)
            pMgmt->eCurrentPHYMode = PHY_TYPE_11G;
        if (pDevice->byBBType == BB_TYPE_11B)
            pMgmt->eCurrentPHYMode = PHY_TYPE_11B;
        if (pDevice->byBBType == BB_TYPE_11A)
            pMgmt->eCurrentPHYMode = PHY_TYPE_11A;
    }

    if (pMgmt->eCurrentPHYMode != PHY_TYPE_11A) {
        pMgmt->abyCurrSuppRates[1] = WLAN_RATES_MAXLEN_11B;
        pMgmt->abyCurrExtSuppRates[1] = 0;
        for (ii = 0; ii < 4; ii++)
            pMgmt->abyCurrSuppRates[2+ii] = abyRATE[ii];
    } else {
        pMgmt->abyCurrSuppRates[1] = 8;
        pMgmt->abyCurrExtSuppRates[1] = 0;
        for (ii = 0; ii < 8; ii++)
            pMgmt->abyCurrSuppRates[2+ii] = abyRATE[ii];
    }


    if (pMgmt->eCurrentPHYMode == PHY_TYPE_11G) {
        pMgmt->abyCurrSuppRates[1] = 8;
        pMgmt->abyCurrExtSuppRates[1] = 4;
        for (ii = 0; ii < 4; ii++)
            pMgmt->abyCurrSuppRates[2+ii] =  abyCCK_RATE[ii];
        for (ii = 4; ii < 8; ii++)
            pMgmt->abyCurrSuppRates[2+ii] =  abyOFDM_RATE[ii-4];
        for (ii = 0; ii < 4; ii++)
            pMgmt->abyCurrExtSuppRates[2+ii] =  abyOFDM_RATE[ii+4];
    }


    // Disable Protect Mode
    pDevice->bProtectMode = 0;
    MACvDisableProtectMD(pDevice);

    pDevice->bBarkerPreambleMd = 0;
    MACvDisableBarkerPreambleMd(pDevice);

    // Kyle Test 2003.11.04

    // set HW beacon interval
    if (pMgmt->wIBSSBeaconPeriod == 0)
        pMgmt->wIBSSBeaconPeriod = DEFAULT_IBSS_BI;
    MACvWriteBeaconInterval(pDevice, pMgmt->wIBSSBeaconPeriod);

    CARDbGetCurrentTSF(pDevice, &qwCurrTSF);
    // clear TSF counter
    CARDbClearCurrentTSF(pDevice);

    // enable TSF counter
    MACvRegBitsOn(pDevice,MAC_REG_TFTCTL,TFTCTL_TSFCNTREN);
    // set Next TBTT
    CARDvSetFirstNextTBTT(pDevice, pMgmt->wIBSSBeaconPeriod);

    pMgmt->uIBSSChannel = pDevice->uChannel;

    if (pMgmt->uIBSSChannel == 0)
        pMgmt->uIBSSChannel = DEFAULT_IBSS_CHANNEL;

    // set channel and clear NAV
    CARDbSetMediaChannel(pDevice, pMgmt->uIBSSChannel);
    pMgmt->uCurrChannel = pMgmt->uIBSSChannel;

    pDevice->byPreambleType = pDevice->byShortPreamble;

    // set basic rate

    RATEvParseMaxRate((PVOID)pDevice, (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                      (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates, TRUE,
                      &wMaxBasicRate, &wMaxSuppRate, &wSuppRate,
                      &byTopCCKBasicRate, &byTopOFDMBasicRate);



    if (pDevice->byBBType == BB_TYPE_11A) {
        pDevice->bShortSlotTime = TRUE;
    } else {
        pDevice->bShortSlotTime = FALSE;
    }
    BBvSetShortSlotTime(pDevice);
    // vUpdateIFS() use pDevice->bShortSlotTime as parameter so it must be called
    // after setting ShortSlotTime.
    // CARDvSetBSSMode call vUpdateIFS()
    CARDvSetBSSMode(pDevice);

    if (pMgmt->eConfigMode == WMAC_CONFIG_AP) {
        MACvRegBitsOn(pDevice, MAC_REG_HOSTCR, HOSTCR_AP);
        pMgmt->eCurrMode = WMAC_MODE_ESS_AP;
    }

    if (pMgmt->eConfigMode == WMAC_CONFIG_IBSS_STA) {
        MACvRegBitsOn(pDevice, MAC_REG_HOSTCR, HOSTCR_ADHOC);
        pMgmt->eCurrMode = WMAC_MODE_IBSS_STA;
    }

    // Adopt pre-configured IBSS vars to current vars
    pMgmt->eCurrState = WMAC_STATE_STARTED;
    pMgmt->wCurrBeaconPeriod = pMgmt->wIBSSBeaconPeriod;
    pMgmt->uCurrChannel = pMgmt->uIBSSChannel;
    pMgmt->wCurrATIMWindow = pMgmt->wIBSSATIMWindow;
    pDevice->uCurrRSSI = 0;
    pDevice->byCurrSQ = 0;

//20080131-04,<Add> by Mike Liu
#ifdef Adhoc_STA
    memcpy(pMgmt->abyDesireSSID,pMgmt->abyAdHocSSID,
                      ((PWLAN_IE_SSID)pMgmt->abyAdHocSSID)->len + WLAN_IEHDR_LEN);
#endif

    memset(pMgmt->abyCurrSSID, 0, WLAN_IEHDR_LEN + WLAN_SSID_MAXLEN + 1);
    memcpy(pMgmt->abyCurrSSID,
           pMgmt->abyDesireSSID,
           ((PWLAN_IE_SSID)pMgmt->abyDesireSSID)->len + WLAN_IEHDR_LEN
          );

    if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP) {
        // AP mode BSSID = MAC addr
        memcpy(pMgmt->abyCurrBSSID, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO"AP beacon created BSSID:%02x-%02x-%02x-%02x-%02x-%02x \n",
                      pMgmt->abyCurrBSSID[0],
                      pMgmt->abyCurrBSSID[1],
                      pMgmt->abyCurrBSSID[2],
                      pMgmt->abyCurrBSSID[3],
                      pMgmt->abyCurrBSSID[4],
                      pMgmt->abyCurrBSSID[5]
                    );
    }

    if (pMgmt->eCurrMode == WMAC_MODE_IBSS_STA) {

        // BSSID selected must be randomized as spec 11.1.3
        pMgmt->abyCurrBSSID[5] = (BYTE) (LODWORD(qwCurrTSF)& 0x000000ff);
        pMgmt->abyCurrBSSID[4] = (BYTE)((LODWORD(qwCurrTSF)& 0x0000ff00) >> 8);
        pMgmt->abyCurrBSSID[3] = (BYTE)((LODWORD(qwCurrTSF)& 0x00ff0000) >> 16);
        pMgmt->abyCurrBSSID[2] = (BYTE)((LODWORD(qwCurrTSF)& 0x00000ff0) >> 4);
        pMgmt->abyCurrBSSID[1] = (BYTE)((LODWORD(qwCurrTSF)& 0x000ff000) >> 12);
        pMgmt->abyCurrBSSID[0] = (BYTE)((LODWORD(qwCurrTSF)& 0x0ff00000) >> 20);
        pMgmt->abyCurrBSSID[5] ^= pMgmt->abyMACAddr[0];
        pMgmt->abyCurrBSSID[4] ^= pMgmt->abyMACAddr[1];
        pMgmt->abyCurrBSSID[3] ^= pMgmt->abyMACAddr[2];
        pMgmt->abyCurrBSSID[2] ^= pMgmt->abyMACAddr[3];
        pMgmt->abyCurrBSSID[1] ^= pMgmt->abyMACAddr[4];
        pMgmt->abyCurrBSSID[0] ^= pMgmt->abyMACAddr[5];
        pMgmt->abyCurrBSSID[0] &= ~IEEE_ADDR_GROUP;
        pMgmt->abyCurrBSSID[0] |= IEEE_ADDR_UNIVERSAL;


        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO"Adhoc beacon created bssid:%02x-%02x-%02x-%02x-%02x-%02x \n",
                      pMgmt->abyCurrBSSID[0],
                      pMgmt->abyCurrBSSID[1],
                      pMgmt->abyCurrBSSID[2],
                      pMgmt->abyCurrBSSID[3],
                      pMgmt->abyCurrBSSID[4],
                      pMgmt->abyCurrBSSID[5]
                    );
    }

    // set BSSID filter
    MACvWriteBSSIDAddress(pDevice, pMgmt->abyCurrBSSID);
    memcpy(pDevice->abyBSSID, pMgmt->abyCurrBSSID, WLAN_ADDR_LEN);

    MACvRegBitsOn(pDevice, MAC_REG_RCR, RCR_BSSID);
    pDevice->byRxMode |= RCR_BSSID;
    pMgmt->bCurrBSSIDFilterOn = TRUE;

    // Set Capability Info
    pMgmt->wCurrCapInfo = 0;

    if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_ESS(1);
        pMgmt->byDTIMPeriod = DEFAULT_DTIM_PERIOD;
        pMgmt->byDTIMCount = pMgmt->byDTIMPeriod - 1;
        pDevice->eOPMode = OP_MODE_AP;
    }

    if (pMgmt->eCurrMode == WMAC_MODE_IBSS_STA) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_IBSS(1);
        pDevice->eOPMode = OP_MODE_ADHOC;
    }

    if (pDevice->bEncryptionEnable) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_PRIVACY(1);
        if (pMgmt->eAuthenMode == WMAC_AUTH_WPANONE) {
            if (pDevice->eEncryptionStatus == Ndis802_11Encryption3Enabled) {
                pMgmt->byCSSPK = KEY_CTL_CCMP;
                pMgmt->byCSSGK = KEY_CTL_CCMP;
            } else if (pDevice->eEncryptionStatus == Ndis802_11Encryption2Enabled) {
                pMgmt->byCSSPK = KEY_CTL_TKIP;
                pMgmt->byCSSGK = KEY_CTL_TKIP;
            } else {
                pMgmt->byCSSPK = KEY_CTL_NONE;
                pMgmt->byCSSGK = KEY_CTL_WEP;
            }
        } else {
            pMgmt->byCSSPK = KEY_CTL_WEP;
            pMgmt->byCSSGK = KEY_CTL_WEP;
        }
    };

    pMgmt->byERPContext = 0;

    if (pDevice->byPreambleType == 1) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
    } else {
        pMgmt->wCurrCapInfo &= (~WLAN_SET_CAP_INFO_SHORTPREAMBLE(1));
    }

    pMgmt->eCurrState = WMAC_STATE_STARTED;
    // Prepare beacon to send
    if (bMgrPrepareBeaconToSend((HANDLE)pDevice, pMgmt)) {
        *pStatus = CMD_STATUS_SUCCESS;
    }
    return ;
}



/*+
 *
 * Routine Description:
 *   Instructs wmac to join a bss using the supplied attributes.
 *   The arguments may the BSSID or SSID and the rest of the
 *   attributes are obtained from the scan result of known bss list.
 *
 *
 * Return Value:
 *    None.
 *
-*/

VOID
vMgrJoinBSSBegin(
    IN  HANDLE hDeviceContext,
    OUT PCMD_STATUS pStatus
    )
{

    PSDevice     pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject    pMgmt = &(pDevice->sMgmtObj);
    PKnownBSS       pCurr = NULL;
    UINT            ii, uu;
    PWLAN_IE_SUPP_RATES pItemRates = NULL;
    PWLAN_IE_SUPP_RATES pItemExtRates = NULL;
    PWLAN_IE_SSID   pItemSSID;
    UINT            uRateLen = WLAN_RATES_MAXLEN;
    WORD            wMaxBasicRate = RATE_1M;
    WORD            wMaxSuppRate = RATE_1M;
    WORD            wSuppRate;
    BYTE            byTopCCKBasicRate = RATE_1M;
    BYTE            byTopOFDMBasicRate = RATE_1M;
    BOOL            bShortSlotTime = FALSE;


    for (ii = 0; ii < MAX_BSS_NUM; ii++) {
        if (pMgmt->sBSSList[ii].bActive == TRUE)
            break;
    }

    if (ii == MAX_BSS_NUM) {
       *pStatus = CMD_STATUS_RESOURCES;
        DBG_PRT(MSG_LEVEL_NOTICE, KERN_INFO "BSS finding:BSS list is empty.\n");
       return;
    };

    // memset(pMgmt->abyDesireBSSID, 0,  WLAN_BSSID_LEN);
    // Search known BSS list for prefer BSSID or SSID

    pCurr = BSSpSearchBSSList(pDevice,
                              pMgmt->abyDesireBSSID,
                              pMgmt->abyDesireSSID,
                              pDevice->eConfigPHYMode
                              );

    if (pCurr == NULL){
       *pStatus = CMD_STATUS_RESOURCES;
       pItemSSID = (PWLAN_IE_SSID)pMgmt->abyDesireSSID;
       DBG_PRT(MSG_LEVEL_NOTICE, KERN_INFO "Scanning [%s] not found, disconnected !\n", pItemSSID->abySSID);
       return;
    };

    DBG_PRT(MSG_LEVEL_NOTICE, KERN_INFO "AP(BSS) finding:Found a AP(BSS)..\n");

    if (WLAN_GET_CAP_INFO_ESS(cpu_to_le16(pCurr->wCapInfo))){

        if ((pMgmt->eAuthenMode == WMAC_AUTH_WPA)||(pMgmt->eAuthenMode == WMAC_AUTH_WPAPSK)) {
/*
            if (pDevice->eEncryptionStatus == Ndis802_11Encryption2Enabled) {
                if (WPA_SearchRSN(0, WPA_TKIP, pCurr) == FALSE) {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"No match RSN info. ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
                    // encryption mode error
                    pMgmt->eCurrState = WMAC_STATE_IDLE;
                    return;
                }
            } else if (pDevice->eEncryptionStatus == Ndis802_11Encryption3Enabled) {
                if (WPA_SearchRSN(0, WPA_AESCCMP, pCurr) == FALSE) {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"No match RSN info. ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
                    // encryption mode error
                    pMgmt->eCurrState = WMAC_STATE_IDLE;
                    return;
                }
            }
*/
        }

#ifdef WPA_SUPPLICANT_DRIVER_WEXT_SUPPORT
	//if(pDevice->bWPASuppWextEnabled == TRUE)
            Encyption_Rebuild(pDevice, pCurr);
#endif

        // Infrastructure BSS
        s_vMgrSynchBSS(pDevice,
                       WMAC_MODE_ESS_STA,
                       pCurr,
                       pStatus
                       );

        if (*pStatus == CMD_STATUS_SUCCESS){

            // Adopt this BSS state vars in Mgmt Object
            pMgmt->uCurrChannel = pCurr->uChannel;

            memset(pMgmt->abyCurrSuppRates, 0 , WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1);
            memset(pMgmt->abyCurrExtSuppRates, 0 , WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1);

            if (pCurr->eNetworkTypeInUse == PHY_TYPE_11B) {
                uRateLen = WLAN_RATES_MAXLEN_11B;
            }

            pItemRates = (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates;
            pItemExtRates = (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates;

            // Parse Support Rate IE
            pItemRates->byElementID = WLAN_EID_SUPP_RATES;
            pItemRates->len = RATEuSetIE((PWLAN_IE_SUPP_RATES)pCurr->abySuppRates,
                                         pItemRates,
                                         uRateLen);

            // Parse Extension Support Rate IE
            pItemExtRates->byElementID = WLAN_EID_EXTSUPP_RATES;
            pItemExtRates->len = RATEuSetIE((PWLAN_IE_SUPP_RATES)pCurr->abyExtSuppRates,
                                            pItemExtRates,
                                            uRateLen);
            // Stuffing Rate IE
            if ((pItemExtRates->len > 0) && (pItemRates->len < 8)) {
                for (ii = 0; ii < (UINT)(8 - pItemRates->len); ) {
                    pItemRates->abyRates[pItemRates->len + ii] = pItemExtRates->abyRates[ii];
                    ii ++;
                    if (pItemExtRates->len <= ii)
                        break;
                }
                pItemRates->len += (BYTE)ii;
                if (pItemExtRates->len - ii > 0) {
                    pItemExtRates->len -= (BYTE)ii;
                    for (uu = 0; uu < pItemExtRates->len; uu ++) {
                        pItemExtRates->abyRates[uu] = pItemExtRates->abyRates[uu + ii];
                    }
                } else {
                    pItemExtRates->len = 0;
                }
            }

            RATEvParseMaxRate((PVOID)pDevice, pItemRates, pItemExtRates, TRUE,
                              &wMaxBasicRate, &wMaxSuppRate, &wSuppRate,
                              &byTopCCKBasicRate, &byTopOFDMBasicRate);
            vUpdateIFS(pDevice);
            // TODO: deal with if wCapInfo the privacy is on, but station WEP is off
            // TODO: deal with if wCapInfo the PS-Pollable is on.
            pMgmt->wCurrBeaconPeriod = pCurr->wBeaconInterval;
            memset(pMgmt->abyCurrSSID, 0, WLAN_IEHDR_LEN + WLAN_SSID_MAXLEN + 1);
            memcpy(pMgmt->abyCurrBSSID, pCurr->abyBSSID, WLAN_BSSID_LEN);
            memcpy(pMgmt->abyCurrSSID, pCurr->abySSID, WLAN_IEHDR_LEN + WLAN_SSID_MAXLEN + 1);

            pMgmt->eCurrMode = WMAC_MODE_ESS_STA;

            pMgmt->eCurrState = WMAC_STATE_JOINTED;
            // Adopt BSS state in Adapter Device Object
            pDevice->eOPMode = OP_MODE_INFRASTRUCTURE;
            memcpy(pDevice->abyBSSID, pCurr->abyBSSID, WLAN_BSSID_LEN);

            // Add current BSS to Candidate list
            // This should only works for WPA2 BSS, and WPA2 BSS check must be done before.
            if (pMgmt->eAuthenMode == WMAC_AUTH_WPA2) {
                BOOL bResult = bAdd_PMKID_Candidate((HANDLE)pDevice, pMgmt->abyCurrBSSID, &pCurr->sRSNCapObj);
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"bAdd_PMKID_Candidate: 1(%d)\n", bResult);
                if (bResult == FALSE) {
                    vFlush_PMKID_Candidate((HANDLE)pDevice);
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"vFlush_PMKID_Candidate: 4\n");
                    bAdd_PMKID_Candidate((HANDLE)pDevice, pMgmt->abyCurrBSSID, &pCurr->sRSNCapObj);
                }
            }

            // Preamble type auto-switch: if AP can receive short-preamble cap,
            // we can turn on too.
            if (WLAN_GET_CAP_INFO_SHORTPREAMBLE(pCurr->wCapInfo)) {
                pDevice->byPreambleType = pDevice->byShortPreamble;
            }
            else {
                pDevice->byPreambleType = 0;
            }
            // Change PreambleType must set RSPINF again
            CARDvSetRSPINF(pDevice, (BYTE)pDevice->byBBType);

            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Join ESS\n");

            if (pCurr->eNetworkTypeInUse == PHY_TYPE_11G) {

                if ((pCurr->sERP.byERP & WLAN_EID_ERP_USE_PROTECTION) != pDevice->bProtectMode) {//0000 0010
                    pDevice->bProtectMode = (pCurr->sERP.byERP & WLAN_EID_ERP_USE_PROTECTION);
                    if (pDevice->bProtectMode) {
                        MACvEnableProtectMD(pDevice);
                    } else {
                        MACvDisableProtectMD(pDevice);
                    }
                    vUpdateIFS(pDevice);
                }
                if ((pCurr->sERP.byERP & WLAN_EID_ERP_NONERP_PRESENT) != pDevice->bNonERPPresent) {//0000 0001
                    pDevice->bNonERPPresent = (pCurr->sERP.byERP & WLAN_EID_ERP_USE_PROTECTION);
                }
                if ((pCurr->sERP.byERP & WLAN_EID_ERP_BARKER_MODE) != pDevice->bBarkerPreambleMd) {//0000 0100
                    pDevice->bBarkerPreambleMd = (pCurr->sERP.byERP & WLAN_EID_ERP_BARKER_MODE);
                    //BarkerPreambleMd has higher priority than shortPreamble bit in Cap
                    if (pDevice->bBarkerPreambleMd) {
                        MACvEnableBarkerPreambleMd(pDevice);
                    } else {
                        MACvDisableBarkerPreambleMd(pDevice);
                    }
                }
            }
            //DBG_PRN_WLAN05(("wCapInfo: %X\n", pCurr->wCapInfo));
            if (WLAN_GET_CAP_INFO_SHORTSLOTTIME(pCurr->wCapInfo) != pDevice->bShortSlotTime) {
                if (pDevice->byBBType == BB_TYPE_11A) {
                    bShortSlotTime = TRUE;
                }
                else if (pDevice->byBBType == BB_TYPE_11B) {
                    bShortSlotTime = FALSE;
                }
                else {
                    bShortSlotTime = WLAN_GET_CAP_INFO_SHORTSLOTTIME(pCurr->wCapInfo);
                }
                //DBG_PRN_WLAN05(("Set Short Slot Time: %d\n", pDevice->bShortSlotTime));
                if (bShortSlotTime != pDevice->bShortSlotTime) {
                    pDevice->bShortSlotTime = bShortSlotTime;
                    BBvSetShortSlotTime(pDevice);
                    vUpdateIFS(pDevice);
                }
            }

            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"End of Join AP -- A/B/G Action\n");
        }
        else {
            pMgmt->eCurrState = WMAC_STATE_IDLE;
        };


     }
     else {
        // ad-hoc mode BSS
        if (pMgmt->eAuthenMode == WMAC_AUTH_WPANONE) {

            if (pDevice->eEncryptionStatus == Ndis802_11Encryption2Enabled) {
/*
                if (WPA_SearchRSN(0, WPA_TKIP, pCurr) == FALSE) {
                    // encryption mode error
                    pMgmt->eCurrState = WMAC_STATE_IDLE;
                    return;
                }
*/
            } else if (pDevice->eEncryptionStatus == Ndis802_11Encryption3Enabled) {
/*
                if (WPA_SearchRSN(0, WPA_AESCCMP, pCurr) == FALSE) {
                    // encryption mode error
                    pMgmt->eCurrState = WMAC_STATE_IDLE;
                    return;
                }
*/
            } else {
                // encryption mode error
                pMgmt->eCurrState = WMAC_STATE_IDLE;
                return;
            }
        }

        s_vMgrSynchBSS(pDevice,
                       WMAC_MODE_IBSS_STA,
                       pCurr,
                       pStatus
                       );

        if (*pStatus == CMD_STATUS_SUCCESS){
            // Adopt this BSS state vars in Mgmt Object
            // TODO: check if CapInfo privacy on, but we don't..
            pMgmt->uCurrChannel = pCurr->uChannel;


            // Parse Support Rate IE
            pMgmt->abyCurrSuppRates[0] = WLAN_EID_SUPP_RATES;
            pMgmt->abyCurrSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)pCurr->abySuppRates,
                                                    (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                                                    WLAN_RATES_MAXLEN_11B);
            // set basic rate
            RATEvParseMaxRate((PVOID)pDevice, (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                              NULL, TRUE, &wMaxBasicRate, &wMaxSuppRate, &wSuppRate,
                              &byTopCCKBasicRate, &byTopOFDMBasicRate);
            vUpdateIFS(pDevice);
            pMgmt->wCurrCapInfo = pCurr->wCapInfo;
            pMgmt->wCurrBeaconPeriod = pCurr->wBeaconInterval;
            memset(pMgmt->abyCurrSSID, 0, WLAN_IEHDR_LEN + WLAN_SSID_MAXLEN);
            memcpy(pMgmt->abyCurrBSSID, pCurr->abyBSSID, WLAN_BSSID_LEN);
            memcpy(pMgmt->abyCurrSSID, pCurr->abySSID, WLAN_IEHDR_LEN + WLAN_SSID_MAXLEN);
//          pMgmt->wCurrATIMWindow = pCurr->wATIMWindow;
            pMgmt->eCurrMode = WMAC_MODE_IBSS_STA;
            pMgmt->eCurrState = WMAC_STATE_STARTED;
            // Adopt BSS state in Adapter Device Object
 working Tecp, 2003->eOPMode = OP_MODE_ADHOC;tworking Technologies,bLinkPass = TRUEerved.
 *
 * TControlvMaskByte(nologie,MESSAGE_REQUEST_MACREG,MAC_REG_PAPEDELAY,LEDSTS_STSeral PubINTER)erved.
 *
 * Tmemcpynd/or mo->abyBSSID, pCurrare FoundatiWLAN_ounda_LEN);
tworking TechDBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Join I * Cok:%02x-am is distributed in the \n",tworking Techg TechnMgmtare Fn; eounda[0]t it will be useful,
 * but WITHOUT ANY 1ARRANTY; without even the implied warrant2ARRANTY; without even the implied warrant3ARRANTY; without even the implied warrant4ARRANTY; without even the implied warrant5] it will be usefuublished by
 * // Preamble type auto-switch: if AP can receive short-pis progrcapt it will be u// and to registry setting isare Fo ndation, w, Inn turn on too.se, or
 * (at if (n 2 oGET_CAPn.
 *_SHORTPREAMBLE(on; eitwCapInfo)) { it will be usefuhis prograyhis progTm; i= Chen
 *
 * Se Fohis progerved.
 *
 * T}tworking Techelse
 * Author: Lyndon Chen
 *
 * Date: May 8, 200    nsMgrObjectInitial - Ini// Changethis progy 8, mustFloo RSPINF againre; you can reARDvSet  vMgrnd/or mod (BYTE)Chen
 *
 * BBy 8,ense, or
 * (at h thispare beacoBeginSta - StabMgrate funBion
 ToSend((HANDL  vMgrReA,,
 * bublished bytInitial -tialize Management 
 * buten; eSpyrig= Wms oSTATE_IDLware; you c}erved.
nse - Hre *
 ;
}



/*+
 *
 * Routine Description:e
 *Set HW to synchronize a specific
 * Cfrom known
 * Clist.onse
 e
 *  asso ValueinSta   PCM   s_USa - -*/
opyric
VOID
s_vMgrSthen * C(tworkIN PS, 2003 ndon Chen
 *t it wIN UINTworking TeuBSSc.
  s_vMgrRxPKgrDe * C Rcv n; et it wOUT funDction
   pequeustwork)
 * AutPS * bVIA Necv asso = &ee Softwarsle Rcv ublished by
 * enSequence_3 - Handle Rcv authentication //1M,   2ence 5ence 1uence18ence24ence36ence54Mtwork      WITHOUSuppRatesG[] = {n 2 oEID_SUPP_RATES, 8, 0x02ssoci4ssociBssoc16ssoc2- Han30ssoc4assoc6Cnse - HRcv authentiExtcation sequence 4
 *     EXT s_vMgrRxDis- HandCe Rcvation1      0nse - Ha     vMgrJoinBSSBegin - Join BSS function
 *      s_vM//e_4 - 9   s_v3
 * 48andle Rcv authentication seAuence 4
 *      s_vMgrRxDisassoci ad_hoc IBSS or Association
 *      ion
  s_vMgrRxBeacon - Hancation seBuence 4
 *      s_vMgrRxDis- Handation - Handle Rcv };
se, or*uthenSe =    s_vMgrR_FAILUREnse, or* Pus_bCipherMatchement t it will be useful, Technologies, EncryenBegthenSee
 *      s_vMgrRxProbeRe&(
 * butbyCSSPK)response
 *      s_vMrgRxProbeRequest -GK)) == FALSE*
 * Author:your option) any lNOTICEversion.
 * "sociate Reques Fail .
 *   \n"ublished byv associs_vMgr- Rcv
 * butseque * C= sequense, or// ReAprevious m.
 *ishis p.eate RerobeRequate_rc.
 * 
 *    rightis p   s*
 * Author:MACvRegBitsOffunction
 *ms of thTCR, TCR_AUTOBCNTXublishe manage// Init the
 * CinformanBegequenchis prograCCKe software; yhis prograProtectc.
 * AnToSe - Rcvsec Disable "card.hDnd/or moublishehis prograBarkesocB progMdinclude "80211hdr.h"
#inc"
#include "wcmd11mgr.h"
#include "wmgr.hNonERPPresenatiolude "80211t data structure
 *      vMgrObjenologies,wBasicon s  vMgrObje//a - S"
#in on s- Rcvrt abAdd"
#includ((PAuthiate functgrRx_1Mense, or// calculludeTSF offseetwork// StatOc Def = R Softwd Timestamp Stat- M
#ind Local's Sta
#includevAdjustTSfunction
 *on; eitbyRxon s---------qwBSS----*/


-*/

/*---------TSF---------------HWction
  interval80211hdr.WritnSta - I------*-----------------w        msglevbles  ---------Next TBTTinitions          = ((l----_current_Stat/ction
 _-------*) + 1 ) *;

/*----------- Static ClSetFirst        el                =MSG_LEVEL_INFO;
//static intounda/
static int   undaAddress-----------------e Foundaense, orthe Free * but WITHOUT ANYtion; either versio6ense, oryour option) any later version.
 * "ion :----THOUT ANY a
    I = am is distribu=am is distributhat it will 
 * but WITHOUT ANY WARRANTY; wi the implied warranty of
 * MERCY or FITNESS FOR A PARTICULAR PUe the
 * GNU General Public Licere details.
 *
 * You should hav a copy of the GNU Genense, or* Puon; eiteNetworky 8,InUsn
 * PHY_TYPE_11t- Initial 1-* Puee SoftwareConfigPHYacon
 * equest(
    IN||tworking Techice,
    IN PSMgmtObject pMgmt,
    IRevi *
 * Author: LynvMgrReAssocBeginS = BBest(
    erved.
 *
 * Thfield of beenttObject pgmt,
    IN Perved.
 *
 * This progranctioSlot----e software; you can rBB----IN WORD wCurr11mgr.h"
#incluta - Start assoc1 - Han  IN PWLAN_IE_SSID }Request - Handle RcvPacket - Rcvs_vMgrRxAspCurrRa  );

static
VOID
s_vMgrRxAssocRequest(
   BIN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN BSRxMgmtPacket pRxPacket,
    IN UINT  uNodeIndex
    )11GSRxMgmtPacket pRxPacket,
    IN UINT  uNodeIndex
    );

static
PSTxMgmtPacket
s_MgrMakeReAssocRequest(
 B  IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
 PSRxMgmtPacket ddr,
    IN WORD wCurrCaplude "80211IN WORD wListenInterval,
    IN PWLAN_IE_SSID pCurrSSID,
    IN PWLAN_IE_SUPP_RATES pCurrRates,
    IN PWLAN_IE_SUPP_RATES pCurrExtSup PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN ype
    );

static
VOID
s_vMgrRxDisassociation(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    ING  IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
 PSMgmtObject pMddr,
    IN WORD wCurrCapInfo,
    IN WORD wListenInterval,
    IN PWLAN_IE_SSID pCurrSSID,
    IN PWLAN_IE_SUPP_RATES pCurrRa  );
ice pDevice,
    IN PSMgmtObject pMgmt,
e pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket ons
static
VOID
s_vMgrRxAuthenSequence_1(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PWLAN_FR_AUTHEN pFrame
    );

static
VOID
s_vMgrRxAuthenSequentes
    );_1 - Han
 *      vMgrTEerInit- Initial 1-sec and command call back funtioHOSns
 *ice pDts resublished bysec and commnd call back funtioRs
 *RCRof theublished byChen
 *
 * Rxc.
 *|=acket
    ;

static
VbeRequeTHOUT ANYFilterOne software; y:
 *
 */

----channeln Strclear NAV
#include SetMediat Obnevel               uunction
#includ * butun; eunctionatch fun_vMgrRxBea - Rcvtions
static
PSTxMgmtPacket
s_MgrMak<----s_bion - Han - Se pDevic[%d]thatVOID
s_vMgrRxBeacones
    );ID
s_vMgrRbUpdateBBVGA) &&tworking IN PSMgmtObyMgmt,
    IN !2002
 *
 * e Fogmt,[0] *
 * Author:IE_TIM pTIM
    );

statc
PSTxMgmtPacket
s_Mgrerved.
 *
wListeVGAGain-----------------E_TIM pTIM
    );

staublished bywListenInterval,
    IN PWLAN_IE_StIniti// msglevelotes:initions1. It (c-hocormatT: checkreet, -------others--------as jo----d indic
#inct it w//nce_   INwis301 UwillCopyrt rDeA Assember // 2pCurrInfra    IN Pcatiosed01 Ualreadyauthenticatid with theright nowle: wmgobe request/response functimerInit- Initial 1-sec and comm IN PSRxMgmtPacket ice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket
    );

static
VOID
s_vMgrRxProbeResponse(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,ates
    );
et
s_MgrMakeReAssoocRequest(
  ce pDevice,
yCurrChannel
    );

cation se, &ameters
 *      s_0], sizeof(ameters
 *      s));

static
V * but WITHOUdle Rcv Beac[1enceMgrObje
static
VOID
s_vMgrRn response
static
PSTxice pDevice,
MgrMakeReAssocResponse(
    IN PSDevice pDevice,
B   IN PSMgmtObject pMgmt,
  B IN WORD wCurrCapInfo,
    IN WORD wAssocStatus,
    IN WO   IN PWLAN_IE_SUPP_RATES pCurrSuppRates,
    IN PWLAN_IE_SG   IN PSMgmtObject pMgmt,
  G IN WORD wCuyCurrChannel
    );

dle Rcv Beac PSDevice dle Rcv Beacon  IN PSMgmtObject dle Rcv Beaco IN WORDtInitiobeRequesERPedis    e,
    INsERP.N_IE_atTIM(
   s_MgrMakeReAssocReqSUCCESSatTIM(
v associame
 //mike IN : fix 
VOID
sManager 0.7.0 hidden ssidormatTIn WPA endle Rcv 

    I// received sta,need ratet eAuthenc.
 * StrHandle Rcv probe_
Copyric Auth  Ence Rcv _Rebuilddle Rcv authentica *      s_vMgrRxuthenticatseque
 )
obeRendle Rcv authentication sequence 2
 *      s_//xAuthenSequence  ii , uSameBs,
 Num=0nse, or
 * 
   for (iiStatu pCu< MAXof t_NUMtati++ce pDevice,
  
      );
 * butsBSSList[ii].bActftwa   IN PWLANtes,
    I  IS_ETH_ADDRionsEQUAL                     e Foundation; either vers *
 * Author: Lynd );

// r,
    OUT PCM++ s_vMgrRxAuthenSe
    tInitial - In//
    ownBSS (TE            >=2) {	 //we onlyPBYTE pAP BYTuppRates,,
  ormatPSDevice pDevicfield oD
s_vMgrLog *      AUTH_WPAPSKSRxM  );

 staticnVOID
smTES pCurrExtSdoes not gftwaude paiLAN_I-ke FlolsecSuppRates,acket pRxPa--------------  Export Variables  2----) {);

 static so01 Us
stati-  ----- it accord, Boto realport Functionscro.ember wmgr.c
 *
 * ;

statibWPAValidxporoftw)    Al//WPA-PSKs_vMgrRxAuthenSequence_3 - --*/


/*+
 *
 * Rourt Variables  ---;
		s.
 *
-*/

VOIabyPKy 8,[0]xportPA_TKIPce pDeviceContebeResponse - Handle Rcv probe_ = Ndis802_11andle Rcv 2En#incd;static= &(vice->sMgmtObj);PRINT_K("   IN WORD wStatu--->SS pCtic
VOcPSMgmct.
[s  ----= &(stat)iceConteK
    );
eConttatic
V;
    PSMgmtObject    pMgmt AESCCM(pDevsMgmtObj);
    int ii;


    pMgmt->pbyPSPacketPool = &pMgm3->byPSPacketPoAESs_vMgrRxAuthenSequence_3 - yMgmtPacketPool = &pMgmt->byMgmtPacketPool[0];
    pMgmt->uAESChannelice->sMgmtObtInitial - IniuChaLAN_SSID_MAXLEN +;
    for(ii=0;D
vM2grObjectInit(
 {IN  HAN2DLE hDeviceContext
    )
{
   PSDevice     pDevice = (PSDevice)hn:
 el = pDevi  for(ii=0;ii<t - Ht    pMg 2 o11i_CSS = &(pDevice->>sMgmtObj);j);
    int ii;


    pMgmt->pbyPSPacketPool = &pMgmt->byPSPackettPool[0];
   h = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION);
    //memset(pMgmt->abyDesireSn:
 uCurrChannel = pDevi+1);
    psMgmtObj);
    for(ii=0;ii<vice, FALSE);

    init_t {
        pMgmt->t->abyDesireBSSID[ii] = 0xFF;
    }
    pMgmt->sAssocInfo.AssocInfo.Length = sizeof(NDIS_802_11_Aunction = (TimerFunction)BSSvSecondCallBack;
    pMgmt->sTimer 0, WLAN_IEHDR__AT(HZ);

1);
    pMgmt->byC1);
    pMgmt->bytInitial -
   tInitialPacket - ate_esponse
 *      vMrgAuthenBeginSta F.h"
# TIM fieldnSta - Start deauthentication Authe      ss_vMgrRxAuthenticatpDevicTIMdle Rcv autle Rcv auth--*/
vMgrSynchB;

  IE_->sTpTIandle e_2 - HaRcv a vMgrDisyute [8ence 1, 2Creatassoc1ion
 2ion
 *ion
 8 BSS
 * xDataInSleep = Fp_IEHDRAuthenSequenii, jj    pDeOOLaInSleep SES pFound.h"
#include "ice->cbFreeCmMulticash"
#include "baWORDaInSleepwdQueuIndexStatus,
  evice->uCmdEnEndIdx = 0;
  ---------FindN PSM of partial virtual bitmapIEHDR
    );


static B(OOL
NightNUM----)erMatch (
    IN PtaTimurrS * but WIPSTxMap    erved.
 *
* Pu!iice pDevice,
    // ute  ouclude broadeueIdbit whichTIM- pCurrSed se fuately    Ndis_stausmdDequeueIdx (* Rout&p = FALS0])tic
MgrObjectReset if(mdDequeueI*
 * Author: Lyndon        NodeDBT#inc[0].bRxPSPolice,oftware; you can rtInitial - Ini* RoutinMgrObjectRetInitial -* PutaTimce pDevice,
    statimdQueue = C*
 * Author: Lyndon mdQueue = CMD_oftware; you can rCmdEnqueueIdx = 0;(evic)ii OUT PCMD_STATUS pStatus
   andState = WpMgmt->wCurrCapInfS pCurrAN_CMD_IDLER = CMTES pCidx = drDeAto nearest even numberIEHDRnqueueIdx = &=  ~BIT_STATUS le) {
   eStredx = uprCapInfo |= WLAN_SET_CAP_INFONFO_ESS(1);
(ice->byPre ret &   }
 -----------SpDeviceelemstatpayloadmanagemTIM->lemtOb 3 + leType == 1-FO_PRIVACY(1-----   // alwaFP_RAin  ExpFixede->bCsviceude 
    pDeFO_SHObyDTIMCoue,
   beRequesmt->wList
#incluif (pMgmt->Period.h"nterval == 0)Interv    pMgmt->wLiBitMapCtice,inSta(
     ?E(1)_MULTICAer thSK : 0) MgmtPacket(((nqueueIdx = >> {
 << {
   uppoBITMAPOFFSE pream-----------Appow rvari#ince->bCvice
   
    pDevice->bnqueueIdx =

   =0 tatic =vice->byPrerMatc

  tch (
    IN PKgmt->wLiVirtERP Ph[jjencee Description:
 *    Start :
 *
 */

Abjec ----n't usedember gmt->wCurrCapInfo |0] );
   }
   TimerTxData.data = (ULONG)pDevice;
    Constructs an Sta -  frame(rSSID,
    I)nSta - Start deauthentication fTRrCapCurrC; or NULL*
 *al   =nBeg failueSvSecondTxData;
ion:
gmtPacket
s_MgrMaknSta - s
    );


static
VOID
s_vMgrSynchBres = RUN_AT(10*HZ);      evicewevicetions
req frame and send Sta - Interv s_vMgrRxAutheDevice pDevireq frame and send ATIMWinodwZ);      //10s cal
    sequeundatle Rcv auRcv aame disp      pMgmt- pMgmt,
 s_vMgrRxD       N UINT uC                 pMgmt->wCurrCapIdle Rcv Beacquence_2 - Hand pMgmt->wCur    pMgmxSID)pMg=}
       pDe 2 oFR_BEACONurrCapsF  }
    pDevice->nTxDaSuppRatee Fosend an
(
  uence 0xffssocAN_IE_SUPP_RATES)pMgmt->abptionEnable)pte function
     }
CurrentCurrSSID,
(LAN_IE_SSID)pM)ment frabyMgmt->wCurPooObject memnfo,
 (pTxPac, 0IN PSMgmt= NULL ){
     +on 2 ofPWLAN    MAXLicens   if (pTxPac->p80211Headerket !U     G) {
 DR)((->aby)f (pTxPack+it(pDevice, pTxPacket)= CMD_Sinclud shoheUPP_RAT       ureember PP_RAT.pBufket ! = WMAC_STATE_DING) {
       CMD_S
    }
RTPREA       if (*pStatus = CMD_SicatEncod|= WLAN_&PP_RAT  *pStatus = CMD_STAh         }
    }
 Hd,
  A3.wP_RATy (802cpu_to_le16dle Rc(pMgmtworking          FC_Fst(
urposest(
 MGR
    if (pMgReAssocBeginSSta(
    IN PSMgm   if ()tworking    *;

// ReAssociation->byPSP- Hance pDevice,
procedure.
 *
 * Return Val|ue:
 *    NonepMgmt-ReAssocBeginPWRMGT(1pDstAddr,
YTE byCurrCh pDevice = (PSDeviaby(
  1,s,
              ion 2 oON_S Licens   pMgmt->wCurrCapInfo = 0;
    pMgm2ction
 PSMgmMAC |= WLAN_SET_CAP_INFO_ESS(1);
    if (pDevice->bEncryption3ic
VOID version 2 of the LicensCurrB
    }
  =MSG_LEVEL_INFlue:
 *    NoneMgrMakeAssocRequembleType == 1) {
it
    t->wCurrCapInfo |= Wctions
  *pStatusCopy L Channel
    }
 
    et ! pMgmt,
    )(
    }
    e+OURCES;

  nfo |= URCES;

   +ambl);

    if (pMReAssocResponse (pMHORTPR
      IEHCAP_INO_ESS(1);
   _SHORTPREAMBt it will be us             pMacket pRxPenInterval = 1;         e.


    // ERP Phy (80tworking Technfo |= WLAN_SET_STAame PSRx_INFO_SHORTPREation seLE(1);

    ifs_vMgrRxDMgmt->wListenInterval == 0)
        pMgmt->wListenInterval  == TRUE)
rCapInfo,
    e.


    // ERP Phy (802.11g) should support fo,
          ble.
    if (        if (pDevice->INFO_SHORTSLOTTIME(1);

    } else if (pMgmt->eCurrentPHYMotworking Tenfo |= WLADSe->bametiation  ReAssociation respons!
static
PSTxMgmtPacket
s_
    }
 DSParm>bShortSlotTimDS_PARM)
          pMgmt->wCurrCapInfo |=       pMgmt->wList1);
      P Phy (802.11g)Info |= WLAN_SET_CALAN_Io |= WMBLE(4
 *     UMMNG(1)erved.
 *
Device,
         RTPREA1         pDevice,
           evice pDevice,      Device pDeviCurrATIMWinodwe->sTimerTxDnBSS          of beacon
 *      vMgrTionsAr(&pMgmt->s->abyCurrB->sTE(1);

    iTIM;


    pTxPacket = s_MgrMakeReAssocRequest
gmt->wLi       pMgmt,
       TIM   pMgmt->aevice->sTimerTx(10*HZ (PWLAN_IE_Sublished by   pMgmt->wList   (
         Interval =FO_SHORTPHistory:
 *
 *PWLAN_IE_SSID)pMgmt->abyCurrSSID,
imerInit- In>sTimerTxDatis pr    if (pMgmt-    (PWLAN_Iis pET_CAP_INFO_SPECTRimerING(1);


    pTxPacket = s_MgrMakeReAssocRequest
        2       (
                  pDevice,
LEVEL_DEBUPP_RATES)pMgmt->abyCurrE "Mgt:ReasMSG_LEVEL_DEBUG, KERN_INFO "MRTPREA2MSG_LEVEL_DEBUG, KERN_INFO "Mwe,
    dow(1);evice,
       Start the stat--*/


/*+
 *
 * Routine DescriptioNONend - Prepareice;
* RSN {
        *w,
           (PWLAN_IRSNE byE(1);

    iRSNCrea;


    pTxPacket = s_MgrMakeReAssocR IN  HANDLE hDevicUPP_RATES)pMgmt->abyCurrE IN WP    IN PBYTE pDddress,
    IN         /*+
 *
 * RestAddress,
    IN  abyOUIt    sociMgrObjectReset ce = (PSDevice)hDeviceCocStatx5    PSTxMgmtPacket      pTxPacket = NU2ntext;f          pDevice = (PSDevice)hDeviceC3ntext;
         pMestAddress,
    IN  wVers>b11           pMDevice = (PSDevice)hDevdDequeueIContext;
    PSTxMgmtPacket      pTxPacket xPacket->pLL;
    WLAN_FR_DISASSOC    sFrame;

    pxPacket->pcket = (PSTxMgmtPacket)
VOID
s_vMgrRxi;


    pMgmt->pb    }
    pMgmt->sAssocInfo.AsOUT PCMD_Scket));

    // Setup the sFrame strul;
    m4;fo.Length = sizeoftatic
VOID
s_vMgrRxket->p80211Header;
    sFrame.len = WLANt->byPSPC_FR_MAXLEN;

    // format fixed field frame structure2;
    pMgmt->sTimerSsociation(&sFrame);

    // Setup the header
    sFrame.pHd1->sA3.wFrameCtl = cpu_to_le16(
        (
        WLAN_SET_FC_F1;//WEP40N_TYPE_MGR) |
     pMgmt->wCurrCap// format fixed field frame structure0;//     - Start reassociart Func Key iate R Suih"
#incTxPacket, 0, sizeof(STxMgPKwListenIMgrObjectReset    }uth3.abyATES p|= WLpMgmt->abyCurrBSSI*Modegmt-gmt->wListenInterval == 0        *pS
    PSDevi))D_STATUS pStatus
    )
{
    PSDevic+=2a - Start reassocivMgrCapabili        _le16(wReason);
    pTxPacket->cbMPDULen = sFrame.len;
    pTxPacket->cbPayloadLen = sFrame.len - WLAN_HDR_pHdr->sA3.abyAddr2,t->wLisrame.len;
    pTxPa      (
                  pnctions
/    (PWLAN_IE_SSID)pM IN PSMgmtObject pMgmt,
              (PWLAN_IERPeContext,
   ERP;


    pTxPacket = s_MgrMakeReAssocRequest
       1      (
                  pDevice,
ERPUPP_RATES)pMgmt->abyCurrEBYTE 
VOID
s_vMgrRxAssocRe               pMgmt->wCusocRequSSID pCurrt->cbPayloa ReAssociation "card.h"
#intInit(
>cbPayloadLen = sFramPacket pRxPacke|N PSDevice pDe_USE_PROTECTIO          p ReAssociatione "datarate.h"
AN_FR_ASSOCREQ    sFrame;
    CMD_STATUS          Status;
     RP_PRESENTket      pTxPacket;
    "
#include "wcmd.hAN_FR_ASSOCREQ    sFrame;
    CMD_STATUS          Status;
 BARKER vMgrstAddr,
        INFO_SHORTSLOTTIME(1);

   dle Rcv Beace.


  


Vtion)
 *    Handle inle Rcv BeacbShortSlotTime == TRUE)
          pMgmt->wCurrCapInfo |=o |= WLAN_SET_CAP_INFO_SHORTSLOTTIME(1);

   rExtSuppRates[WLAN_     (
                  p should support   IN UINT uCt preamble.
    if icated
    //decode the frade index not found
    if (!uNodeIndex)
        return;

    /  pMgmt->wCurrwCurrATIMWinodw hostapd wpa/wpa2 IETIM(
    IN_IE_SSID)pMgmt->abyCurrSSID,
        &&us
    )
{
    PSDH
    me;
    BYce pDevice,
 frame to the AP.
 *
 * Return Value:
 *    None.
 *
-*/

VO;
    sFramewWPAIELAN_IEHDR_LEN + WLANyCurrBSSID, WLAN_BSSeContext,
    INMgmtObject pMgmt,
    IN  PBYTE  abyDestAk if node is authentRSNnable) {
   equesnable) {
Request(&      pMgmt->sNodeDBC_STATE_IDLE;
SSOC;
        pM     pMgmt->sNotInitial - nctions
// pr/* asses ORTPRlengthTimerTseginSta(   *pStatuscbMPDUt(&s;
       lle16(*sFrval = cpu_toPN_SET_6(*sFrame.pwList -t,
   Phy ON_S3 (802.ES pCurrSuperval);
  ciate_r  if (pDevice->byPreambleType == 1) {
            pProb-response  );

  ET_CAP_INFO_SHORTPREAMBLE(1);
        }
    }
    if (pMgmt->b11hEnable == TRUE

    pMgmt->wCurrCapInfo |    eR  // To_SET_CAP_INFO_SPECTRUMMNG(1);

    // build an assocreq frame and send it
    pTxPacket = s_MgrMakeAssocRequest
                (
                  pDevice,
                 >abyCuDfo |= W          pMgmt,
                  pMgmt->abyCurrBSSID,
                  pMgmt->wCurrCapInfo,
                  pMgmt->wListenInterval,
                 Rcv abyPHYy 8,          (PWLAN_IE_SSID)pMgmt->abyCurrSSID,
                  PROBERESP_SUPP_RATESturn;
}f (pTxPacket != NULL ){
        // send the frame
        *pStatus = csMgmt_xmit(pDevice, pTxPacket);
        abyCurr*pStatus == CMD_STATUS_PENDING) {
            pMgmt->eCurrState = WMAC_STATE_ASSOCPENDING;
            *pStatus = CMD_STATUS_SUCCESS;
        }
    }
    else
        *pStatus = CMD_STATUS_RESOURCES;

    return_IE_SUPP_RATES)abyC Routine Descrip         uRate*    Start the station re-association procedure.
 *
 * Return Value:
 *    None.
 *
-*/

VOID
vMgrReAssocBeginSta(
    IN  HANDLE hDeviceContext,
    IN  PSMgmtObject pMgmt  abyCurrOUT PCMD_STATU   pMgmt->wCurrCapInfo = 0;
    pMgmt->         LAN_SET_CAP_INFO_ESS(1);
    if (pDevice->bEncryptionEnable) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_PRIVACY(1);
    }

    //if (pDevice->byPreambleType == 1) {
    //    pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
    //}
    pMgmt->wCurrCapInfo    (PWLAXTSUPP_RAPBYTE pDstAddr,
    IN PWLAORTPREAMBLE(1);
  &ontext;
    PSTxMgmt~send t    ndles the 802.SLOTTIMExPacHistory:
 *
 */

N_SET_CAP_INFO_SHORTPREAMBLE(1);

    if (pMgmt->wListenInterval == 0)
        pMgmt->wListenInterval = 1;    // at least one.


    // ERP Phy (802.11g) should support short preamble.
_11G) {
  == 1) {
            pMgmTYPE_11G) {
        pMgmt->wCurrCapInfo |= WLAN_ET_CAP_INFO_SHORTPREAMBLE(1);
      if (pDevice->bShortSlotTime == TRUE)
          pMgmt->wCurrCapInffo |= WLAN_SET_CAP_INFO_SHORTSLOTTIME(1);

    } else if (pMgmt->eCurrentPHYMode == PHY_TYPE_11B) {
        if (pDevice->byPreambleType == 1) {
            pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
         }
    }
    if (pMgmt->b11hEnable == TRUE)
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SPECTRUMMNG(1);


    pTxPacket = s_MgrMakeReAssocRequest
                (
                  pDevice,
                  pMgmt,
                  pMgmt->abyCurrBSSID,
                  pMgmt->wCurrCapInfo,
                  pMgmt->wListenInterval    (PWLAN_IE_SSID)pMgmt->!byCurrSSID,
                  ENDING) {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Reassociation tx failed.\n");
        }
        else {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Reassociation tx sending.\n");
        }
    }


    return ;
}

/*+
 *
 * Routine Description:
 *    Send antus,
   ;

// ReAssociation response
static
PSTxction)
 *    Handle incoming station association request frames.
 *
 * Return Value:
 *    None.
 *
-*/

static
VOID
s_vMgrRxAssocRequest(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket,
    IN UINT uNodeIndex
    )
{
    WLAN_FR_ASSOCREQ    sFrame;
    CMD_STATUS          Status;
    PSTxMgmtPacket      pTxPacket;
    WORD                wAssocStatus = 0;
    WORD                wAssocAID = 0;
    UINT                uRateLen = WLAN_RATES_MAXLEN;
    BYTE                abyCurrSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN ++ 1];
    BYTE                abyCurrExtSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];


    if (pMgmt->eCurrMode != WMAC_MODE_ESS_AP)
        return;
    //  node index not found
    if (!uNodeIndex)
        return;

    //check if node is authenticated
    //decode the frame
    memset(&sFrame, 0, sizeof(WLAN_FR_ASSOCREQ));
    memset(abyCurrSuppRates, 0, WLAN_IEHDR_LEN + WLAN_RATES_MAXLLEN + 1);
    memset(abyCurrExtSuppRates, 0, WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1);
    sFrame.len = pRxPacket->cbMPDULen;
    sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;

    vMgrDecodeAssocRequest(&sFrame);

    if (pMgmt->sNodeDBTable[uNodeIndex].eNodeState >= NODE_AUTH) {
        pMgmt->sNodeDBTable[uNodeIndex].eNodeState = NODE_ASSOC;
        pMgmt->sNodeDBTable[uNodeIndex].wCapInfo = cpu_to_le16(*sFrame.pwCapInfo);
        pMgmt->sN/deDBTable[uNodeIndex].wLinInterval = cpu_to_le16(*sFrame.pwListenInterval);
        pMgmt->sNodeDBTable[uNodeIndex].bPSEnable =
                WLAN_GET_FC_if (pDevice->byPreambleType == 1) {
            passocit->b11requ |= o: check sta basic rate, if ap can'A ptr       }
t status code
        if (pDevice->byBB   pMgmt->wCurrCapInfo |ANodeR  )
{
_SET_CAP_INFO_SPECTRUMMNG(1);

    // build an assocreq frame                      and send it
    pTxPacket = s_M    e   msglev                          (PWLAN_IE_SUPP_R        pMgmt->wCurrCapItes[0] = WLAN_     pMgmt->wListenInterval,
                  (PWLAN_IE_SSID)pMgmt->abyCurrSSID,
                  ASSOCREQ    (PWLANndex].      ANDLE hDeviceCobyIEs       return;
    //  node iRS =
 CMD_STATUS_PENket != NULL ){
        // send the frame
        *pStatus = csMgmt_xmit(pDevice, pTxPacket);
      MAC_MODE*pStatus == CMD_STATUS_PENDING) {
            pMgmt->eCurrState = WMAC_STATE_ASSOCPENDING;
            *pStatus = CMD_STATUS_SUCCESS;
        }
    }
    else
        *pStatus = CMD_STATUS_RESOURCES;

    returnEASSOCREQ));
    s *pStatuso.h"
#AN_IedTimerTATUS   CESS;
   Routine Descrip            w*    Start the station re-association procedure.
 *
 * Return Value:
 *    None.
 *
-*/

VOID
vMgrReAssocBeginSta(
    IN  HANDLE hDeviceContext,
    IN  PSMgmtObject pMgmtMAC_MODETable[uNodeIndex].wMaxBasicRate),
                      |= WLAN_SET_CAP_INFO_ESS(1);
    if (pDevice->bEncryptionEnable) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_PRIVACY(1);
    }

nnel
    );

// Asson 2 of the License, orinclude_STAcapithe fygStatnDege---------*/
sta*d supportnctions
    //}
    pMgmt->wCurrCapInfo |=         aby BYTE         pRates[0] = WLAN_rrSuppRates[1] les  ------_STATE_IDLpP_RArCapow rofdeState = NO_INFO_SHORTPREAMBLE(1);

    if (pMgmt->wListenInterval == 0)
        pMgmt->wLis_11G) {
 .


    // ERP Phy (802.11g) should support shor        WLA               uRateLen);
      TES)sFra             ons
.B_TYPE_11G       est(&Inde                uRateLen);
        abyCype == BB_TYPE_11G) {
      ------      aby


  PSMgmtNDISt->e_11le[uNoIAtPacn.
 *RM     = CMD_ST indeLAN_SET_CABB_TYPE_11Ga index not the Free inde= WLAN_EID_EXTSUPP_RATES;
        if (pDevice->_RATES)abyC                 uRateLen);
        _CAP_INFO_SHORTPREAMBLE(1);
      if (pDevice->bShortSlotTime == TRUE)
          pMgmt->wCurrCapInfo |=    IN PSMgmtOb    IN PBYTE pDstAddr,
 XLEN ];
    BYject p> 4)ASSOCREQ     pMgmt->wLis4   return;

    //check( sFrame.pHdr                   PP_RATES)abt->eCurrentPHYMode == PHY_TYPE_11B) {
        i 1];
    BYTc rate
                          TES)sFrame.O_SHORTPRextenacketEAMBLE(1);
  uppRates,
                          GPWLAN_IE_SUAXLEN + 1];
TES)abyC0et->cbMPDULen_RATES_MAXLEN + 1];


    if (pMgmt->eCurrMode != WMAC_MODE_ESS_AP)
        return;
    //  no>sNodeDBTable[uNodeInde  return;

    //check if node is authenticated
    //CurrExtSuppRates[0]              &(pMgmt->sNodeDBTable[uNodHistory:
 *
 *ype == BB_TYPE_11G) {
            abyCurrExtsic rate
                           &(pMgmt->sNo             axBasicRate),
                           &
            abyCur &(pMgmt->sNodeDBTable[uNodeI            --*/


/*+
 *
 * Routine DescriptioSRxMgmtPacket ----------------  Export Variables  -------CAP_INFO_SHORTPREAMBLE(*sFrame.pwCapInfo);
       N
    IN PWLAN_Ient frame dispa!,
    et->cbMPDULen/*TE byIEeginSta(
    HANDLE hDeviceContext,
    IN  PSMgmtObject pMgmt,
    IN  PBYTE  abyDddress,
    IN  WORD    wReason,
    OUT PCMD_STATUS pSt        *pStatus = C= 16vice,
    IN PSMgmDevice)hDeviceContext;
    PSTxMgmket      pTxPacket = NULL;
    WLAN_FR_DISC    sFrame;

    pTxPacket = (PSTxMgmtPacpMgmt->pbyMgmtPacketPool;
    memset(pTxPa, 0, sizeof(STxMgmtPacket) + WLAN_DISA//Group3.abyAddr3, pMgmt->abyCurr_FR_MAXLEN);
    pTxPacket->p80211Header = (PUWL0211HDR)((PBYTE)pTxPacket + sizeof(STxMgmtPacket
    // Setup the sFrame structure
    sFrame.pBframe to thrPrepar pRxKEY_CTL_WE                 // format fixed field frame structAN_GET_CAP_INFOeambGObjecme
    );

static
VOIDrtPreamble == FALSE) {
  imer(&pMgmt->svice->bBarkerPreambleMd = TRUE;
        gmt = &(VEL_INFO, KERN_INFO "Rx ReAssociate AID= %d \n" {
     AID);
        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "M++) {
 UPP_RATES pCurrRates,
    IN PW  DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "M    PSDevice            me.pHdr->sA3.abyAddr3, pMgmt->abyCurrD, WLAN_BSSID_LEN);

    //         pMgmt->wCu    IN  PK      0PBYTEif(pMgmt->sNodeDBTable[uNodeIndex].wMax        sFrame.pHdesent = TRUE;
        }
        if         sFrame.pHdDBTable[uNodeIndex].bShortPreamble Pe AID= %d \n", wAssocAID);
        DBG_PRT(MSG_LE        sFrame.pHdN_INFO "MAC=%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2eDBTable[uNode            sFrame.pHdr->sA3.abyAddr/ assoc response reply..
     sFrame.pHdr->sA3.abyAddr2[1],
                   sF/ assoc response reply..
  2[2],
                   sF   *(sFrame.pwReason) = cpu_to_le1Index)else
        pTxPacket->cbMPDULen = sF2 sFrame.len;
    pTxPacvice,
    *Index)++=  memset(pTxPa  (PWLAN_IE_St->cbPayloaMgmt->abyCurrExttSuppRates
           WLAN_FR_DIS  (PWLAN_IE_[uNodeIndex].bShortPrea--------  Export Variables  -----      sFrame.p  (PWLAN_I     ableSSvCl    s_vMgrRxAssocRequesframe to the AP.
 *
 * Return Value:
 *) {
            return;
        }
 IEEE     X    Status = csMgmt_xmit({
            return;
      2[2],
          .abyAddr2[4],
           WLAN_H6STATUS  pStatu// send the framP_RATES)pMgmt->abyCurrExtSuppRates
              yloadLen = sFrame.len - WLAN_HDR_ADDR3_LENC_STATE_IDLE;
        *pStatus = CMD_STATUS_SUCCESS;
    };
----_SHORo B_TYPE_11G pDevOID                    (PWLAN_IE_SWLAN_IE_SUPP_RATIndex].wTxDataRate =
                pMg        *pStatus = CMD_STATUS_SUCCESS;
    };
ate;
        //[4],
          BOOL bReAssocTyx tx rate
        pMgmt->sNod
            a        *pStatus = CMD_STATUS_SUCCESS;s,
    IN WORD wShortPreamble =
                WLAN_2SRxMgmtPacket pRx-------*/


/*+
 *
 * Routine Description:
 *  e,
    IN NDIS_O_SHORTPREACAP_INFO_SHORTSLOTTIME(*sFramN PKnownBSS         pC       }
 on);
rn;
    //  nodewPMK     response frwCapIn      }
    }
    re[uNodeIndex].eNodeState >= NODE_AUTH) {
        pMgmt[4],
        WORD    wReason,
    OUT Pf ((sFrame.pwCapInfo ==RTPREA6; //mtPacke(2)+GK(4rExtSuppRates,
  nfo ==.wMaxSuppRate <= RATE_11M) {
            // B only STA join
      )hDevRSNrotectMode = TRUE;
            urn;
    LL;
   0F       pMgmt->sAssocInfo.Assoccket = Aserved.
 *
.bShortPreamble == FALSE) {
            pDevice->bBarkerPreaurn;
         }

        DBG_PRT(reparVEL_INFO, KERN_INFO "Rx ReAssociate AID= %d \n", wAssocAID);
        DBG_PRT(Mode = *(sFrame;

    init_timer2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X \n",
                   sFrame.pHdr->sA3.ab    pMgmt->sAssocInfo.AssocsFrame.pHdr->sA3.abyAddr2[1],
                    pMgmt->sAssocInfo.AssocUNKNOWSS;
    };

           sFrame.pHdr->sA3.abyAddr2[3],
                 urn;
    4ntexA3.abyAddr2[4],
      urn;
    5cStatus,
   = pMgmt->sAssocInfo.aby6   };

        pMgmt->sAssocInfo.Assoc7nfo.ResponseFixedIEs.Capabilities = *(E;
  e.pwCapInfo);
        pMgmt->seDBTable[uNodeIndex].wMaxSuppRate);

    }


urn;
    9>sAssocInfo.AssocInfo.AvailableResponseFixedIEs |= 0x0e
                (
                  pDeviceurrent BSS state
        ifsFrame.pHdr->sA3.abyAme.pwStatus))) == WLAN_MGMT_ST    None.
 *
-*/

VO        // set AID
            pMgmt->   PGROU- 6;
        pMgmt->sAssocInfo.AssocInfo.OffsetRespo0 | BIT1) )
       socInfo.AssocInfo.wStatus == 0) ||
        +    .OffsetReques   *(sFrame.pwReason) = cpu_to_le1mt->sAssocInfo.AssocIontex pbyIEs = pMgmt->sAssocInfo.aby1ocStatus,
   pMgmt->sAssocInfo.AssocIcket = 
        pMgmt->sAssocInfo.AssocIl;
    mponseFixedIEs.Capabilities = *(1     e.pwCapInfo);
        pMg/*+
 *
 * Routine Description:
 *SUCCESS ){
            // set AID
 1IEs;
;

    inAKMMgt:      Status ERN_INFO "Rx ReAsdr;



    if (pMgmt->eCurrStatLAN_IE_SSID)pMgmt->abyCurrSSID;
            DBG_PRT(MSGBG_PRT(MSG_LEVEL_ pMgmt->sAssocInfo.AssocInfo.OffsetRespo          DBG_PRT(MSGwo msb clear.\n");
            };
              DBG_PRT(MSG_LEVEL_DEBUG, KERNpInfo);
        pMg    DBG_PRsRSNCapObj.bom(pDeExieIdxgmt->byCSlished by
 * the Fre*    StssocInfo.AssocI6], &     if(skb_tailroom(pDevicewom(pDe, 2SUPP_RATES pCurrRates,
    IN PWAssocInfo.AssocInfo.Res // Set reason codAssocInfo.AssocInfo.R memcp    Status = csMgmt_    };
            DR_ADDR3_LEN    IN PSMgmtOgs1Head.SID,
ons
wListex].wAXLEN + 1);
   Roam, Boof(viawgeXLEN ink with AP(SSID): %s\n", pItemSSID- association proced   v1Headtworking Techn        sAssocInfo.AssocInfo.R8 Start the >p80211HeadeContegmt-Index)
  sFr          wpahcList
            re   wpahdr0evice;TATUS_PENDIniCmdRpDevocInfo.AssocInfo.Respons wpahdr+

/*         pbyCCSGgmt->sAssocInfonDeg     sFrame.p
    );


static B  pDevice->skb = dev_alloc_skb(erMatch (
    IN PKn->skb);
		 !the mp(&  pDevice->skb = dev_alloc     T ANY WARvice->byBBType == BB_TURYPTE.bPSEn Lice*
 * Author: Lyndon OC) {eIELengt)                pbyC pRxPacket,
   x].eNo               memcpy(pDevice-1Head, 1tion;
    pMgmt->wIBSSBeao.AssocIn     // if sCMD_STATUS pStatus
   tInitial - Ini* PueIELengthFrame);

    if (pMgmt->    };
            D(abyC          *;
               ;
              }
        el_IDLE;
        *pSle incoming association response frames.
 *
 * Return Value:
 *    None.
 *
-*/

static
VOID
s_vMgrRxAssocResponse(
    IN PSDevice pDevice,
    IN PSMgmtObjecgmt->sNodeDBTable[uNodeIndex].byTopOFDM    IN BOOL bReAsso BOOL bReAssoLAN_FR_ASSOCRESP   sFrame;
    PWLAN_IE_SSID   pItemSSgmt->sNodeDBTable[uNodeIndexeturn;
}- Received Packet
 *  Out:
 *      none
 *
 * Return Value: None.
 *
-*/

static
VOID
s_vMgrRxReAssocRequest(
    IN PSDevice pDev               WLAN_GET_FC_PWpMgmt,
    IN PSRxMgmtPacket pRxPacket,
    IN UINT re-uNodeIndex
    )
{
    WLAN_FR_REASSOCREQ    sFrame;
    CMD_STATUS          Status;
    PSTxMgmtPacket      pTxPacket;
    WORD    R             wAssocStatus = 0;
    WORD                wAssocAID = 0;
    UINT                uRateLen = WLAN_RATES_MAXLEN;
    BYTE                abyCurrSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];
    BYTE                abyCurrExtSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];

    if (pMgmt->eCurrMode != WREMAC_MODE_E_AP)
        return;
    //  node index not found
    if (!uNodeIndex)
        return;
    //check if node is authenticated
    //decode the fr RATEuSetIt_xmit(pDevice, pTxPacket);
      of (wrqu))*pStatus == CMD_STATUS_PENDING) {
            pMgmt->eCurrState = WMAC_STATE_ASSOCPENDING;
            *pStatta - CMD_STATUS_SUCCESS;
    eginSta(
    }
    else
        *pStatus = CMD_STATUS_RESOURCES;

    returnet(&wrqu, 0, sizeof     }


  x].eNodeState = NODE_ASSOC;
        pMgmt->sNodeDReuNodee[uNodeIndex].wCapdr.sa_family = ARPssociaINT_K("wireless_e.
 *
 * Return Value:
 *    None.
 *
-*/

VOID
vMgrReAssocBeginSta(
    IN  HANDLE hDeviceContext,
    IN  PSMgmtObject pMgmtof (wrqu))ndex].bPSEnable =
                WLAN_GET_FC_PWRMGT(sFrame.pHdr->sA3.wFrameCtl) ? TRUE : FALSE;
        // Todo: check sta basic rate, if ap can't support, set status code

        if (pDevice->byBBType == BB_TYPE_11B) {
            ta - Sen = WLAN_RATES_MAXLEN_11B;
      eginSta(        abyCurrSuppRates[0] = WLAN_EID_SUPP_RATES;
        abyCurrSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFra should support(
  evicePevice->byBBType == BB_TYPE_11B) {
      }

#if O_SHORTPR
    ginSta(/*.pSuppRates,
                          INT_K("wireless_  (PWLAN_IE_SUPP_RATES)abyCurrSuppRates,
                                         uRateLen);
        abyCurrExtSuppRates[0] = WLAN_EID_EXTSUPP_RATES;
        if (pDevice->byBBType == BB_TYPE_11G) {
            abyCurrExtSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pExtSuppRates,
                                                (PWLAN_IE_SUPP_RATES)abyCurrExtSuppRates,
                                                uRateLen);
        } else {
            abyCurrExtSuppRates[1] = 0;
        }


Device->bweEAMBLE(1ep3 = FALSE;
              pDevice->bWxtstep3 = FA    if (pDevice->bShortSlotTime == TRUE)
          pMgmt->wCurrCapInfo |= WLAN_SET_CAP_ rate
                           &(pMgmt->sNodeDBTable[uNodeIndex].wMaxBasicRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate)*+
 *
 * Routine Description:(AP functigmt->sNodeDBTable[uNodeIndex].wSuppRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].byTopCCKBasicRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].byTopOFDMBasicRate)
                          );

        // set max tx rate
        pMgmt->sNodeDBTable[uNodeIndex].wTxDataRate =
                pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate;
        // Todo: check sta preamble, if ap can't support, set status code
        pMgmt->sNodeDBTable[uNodIndex].bShortPreamble =
                WLAN_GET_CAP_INFO_SHORTPREAMBLE(*sFrame.pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].bShortSlotTime =
                WLAN_GET_CAP_INFO_SHORTSLOTTIME(*sFrame.pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].wAID = (WORD)uNodeIndex;
        wAssocStatus = WLAN_MGMT_STATUS_SUCCESS;
        wAssocAID = (WORD)uNodeIndex;

        // if suppurt ERP
        if(pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate > RATE_11M)
           pMgmt->sNodeDBTable[uNodeIndex].bERPExist = TRUE;

        if (pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate <= RATE_11M) {
            // B only STA join
            pDevice->bProtectMode = TRUE;
            pDevice->bNonERPPresent = TRUE;
        }
        if (pMgmt->sNodeDBTable[uNodeIndex].bShortPreamble == FALSE) {
            pDevice->bBarkerPreambleMd = TRUE;
        }

        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Rx ReAssociate AID= %d \n", wAssocAID);
        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "MAC=%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X \n",
                   sFrame.pHdr->sA3.abyAddr2[0],
                   sFrame.pHdr->sA3.abyAddr2[1],
                   sFrame.pHdr->sA3.abyAddr2[2],
                   sFrame.pHdr->sA3.abyAddr2[3],
                   sFrame.pHdr->sA3.abyAddr2[4],
                   sFrame.pHdr->sA3.abyAddr2[5]
                  ) ;
        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Max Support rate = %d \n",
                   pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate);

    }


    // assoc response reply..
    pTxPacket = s_MgrMakeReAssocResponse
                (
                  pDevice,
                  pMgmt,
                  pMgmt->wCurrCapInfo,
                  wAssocStatus,
                  wAssocAID,
                  sFrame.pHdr->sA3.abyAddr2,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates
                );

    if (pTxPacket != NULL ){
        /* send the frame */
        if (pDevice->bEnableHostapd) {
            return;
        }
        Status _xmit(pDevice, pTxPacket);
        if (Status != CMD_STATUS_PENDING) {
            DBG_PRT(MSG_LEVEL_Request - Handle Rcvc response tx failed\n");
        }
        else {
            DBG_PRT(MSG_LEVEL_DEBUG, KERNINFO "Mgt:ReAssoc response tx sending..\n");
        }
    }
    return;
}


/*+
 *
 * Routine Description:
 *    Handle incoming association response frames.
 *
 * Return Value:
 *    None.
 *
-*/

static
VOID
s_vMgrRxAssocResponse(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket,
    IN BOOL bReAssocType
    )
{
    WLAN_FR_ASSOCRESP   sFrame;
    PWLAN_IE_SSID   pItemSSID;
    PBYTE   pbyIEs;
    viawget_wpa_header *wpahdr;



    if (pMgmt->eCurrState == WMAC_STATE_ASSOCPENDING ||
         pMgmt->eCurrState == WMAC_STATE_ASSOC) {

        sFrame.len = pRxPacket->cbMPDULen;
        sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
        .pwCapInfo);
        pMgmt->sNovMgrDecodeAssocResponse(&sFrame);
        if ((sFrame.pwCapInfo == 0) ||
            (sFrame.pwStatus == 0) ||
            (sFrame.pwAid == 0) ||
            (sFrame.pSuppRates == 0)){
            DBG_PORT80(0xCC);
            return;
        };

        pMgmt->sAssocInfo.AssocInfo.ResponseFixedIEs.Capabilities = *(sFrame.pwCapInfo);
        pMgmt->sAssocInfo.AssocInfo.ResponseFixedIEs.StatusCode = *(sFrame.pwStatus);
        pMgmt->sAssocInfo.AssocInfo.ResponseFixedIEs.AssociationId = *(sFrame.pwAid);
        pMgmt->sAssocInfo.AssocInfo.AvailableResponseFixedIEs |= 0x07;

        pMgmt->sAssocInfo.AssocInfo.ResponseIELength = sFrame.len - 24 - 6;
        pMgmt->sAssocInfo.AssocInfo.OffsetResponseIEs = pMgmt->sAssocInfo.AssocInfo.OffsetRequestIEs + pMgmt->sAssocInfo.AssocInfo.RequestIELength;
        pbyIEs = pMgmt->sAssocInfo.abyIEs;
        pbyIEs += pMgmt->sAssocInfo.AssocInfo.RequestIELength;
        memcpy(pbyIEs, (sFrame.pBuf + 24 +6), pMgmt->sAssocInfo.AssocInfo.ResponseIELength);

        // save values and set current BSS state
        if (cpu_to_le16((*(sFrame.pwStatus))) == WLAN_MGMT_STATUS_SUCCESS ){
            // set AID
            pMgmt->wCurrAID = cpu_to_le16((*(sFrame.pwAid)));
            if ( (pMgmt->wCurrAID >> 14) != (BIT0 | BIT1) )
            {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "AID from AP, has two msb clear.\n");
            };
            DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Association Successful, AID=%d.\n", pMgmt->wCurrAID & ~(BIT14|BIT15));
            pMgmt->eCurrState = WMAC_STATE_ASSOC;
            BSSvUpdateAPNode((HANDLE)pDevice, sFrame.pwCapInfo, sFrame.pSuppRates, sFrame.pExtSuppRates);
            pItemSSID = (PWLAN_IE_SSID)pMgmt->abyCurrSSID;
            DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Link with AP(SSID): %s\n", pItemSSID->abySSID);
            pDevice->bLinkPass = TRUE;
            ControlvMaskByte(pDevice,MESSAGE_REQUEST_MACREG,MAC_REG_PAPEDELAY,LEDSTS_STS,LEDSTS_INTER);
            if ((pDevice->bWPADEVUp) && (pDevice->skb != NULL)) {
	       if(skb_tailroom(pDevice->skb) <(sizeof(viawget_wpa_header)+pMgmt->sAssocInfo.AssocInfo.ResponseIELength+
		   	                                                 pMgmt->sAssocInfo.AssocInfo.RequestIELength)) {    //data room not enough
                     dev_kfree_skb(pDevice->skb);
		   pDevice->skb = dev_alloc_skb((int)pDevice->rx_buf_sz);
	       	}
                wpahdr = (viawget_wpa_header *)pDevice->skb->data;
                wpahdr->type = VIAWGET_ASSOC_MSG;
                wpahdr->resp_ie_len = pMgmt->sAssocInfo.AssocInfo.ResponseIELength;
                wpahdr->req_ie_len = pMgmt->sAssocInfo.AssocInfo.RequestIELength;
                memcpy(pDevice->skb->data + sizeof(viawget_wpa_header), pMgmt->sAssocInfo.abyIEs, wpahdr->req_ie_len);
                memcpy(pDevice->skb->data + sizeof(viawget_wpa_header) + wpahdr->req_ie_len,
                       pbyIEs,
                       wpahdr->resp_ie_len
                       );
                skb_put(pDevice->skb, sizeof(viawget_wpa_header) + wpahdr->resp_ie_len + wpahdr->req_ie_len);
                pDevice->skb->dev = pDevice->wpadev;
		skb_reset_mac_header(pDevice->skb);
                pDevice->skb->pkt_type = PACKET_HOST;
                pDevice->skb->protocol = htons(ETH_P_802_2);
                memset(pDevice->skb->cb, 0, sizeof(pDevice->skb->cb));
                netif_rx(pDevice->skb);
                pDevice->skb = dev_alloc_skb((int)pDevice->rx_buf_sz);
            }
//2008-0409-07, <Add>mt->sNodeDBTable[uNodeIndex].wListenInterval = cpu_to_le16(*sFrame.pwListenInterval);
        pMgmt->sNodeDBTable[uNodeIndex].bPSEnable =
                WLAN_GET_FC_mt,
    IN PSRxMgmtPacket pRxPacket,
    IN UINT uNode    // Todo: check sta basic rate, if ap can't support, set status code
        if (pDevice->byBB pTxPacket;
    WORD             uRateLen = WLAN_RATES_MAXLEN_11B;
        }
        abyCurrSuppRates[0] = WLAN_EID_SUPP_RATES;
       probe_responFrame.len - WLA      pMgmt->abyCu                          Mgmt->wCurrCapInfo,
                  pMgmt->wListenInterval,
                  (PWLAN_IE_SSID)pMgmt->abyCurrSSID,
                  MAC_MODrrExtSuppRates[     return;
    //check if node is authenticated
    //decode the frame
    memset(&sFrame, 0, sizeof(WLAN_FR_REASSOCREQ));
    sFrame.len = pRxPacket->cbMPDULen;
    sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
    vMgrDecodeReassocRequest(&sFrame);

   _K("wireless_send_event--->SIOCGIWAP(associated)\n");
	wireless_send_event(pDevice      (PWLAN_IE_SUPP_RATES)adjust the lengndex].wCapInfo = cpu_to_le16(*sFrame.pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].wListenInterval = cpu_to_le16(*sFrame.pwListenInterval);
        pMgmt->sNodeDBTable[uNodeDBTable[uNodeIndex].wMaxBasicRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].wSuppRate),
          ce->byBBType == BB_TYPE_11B) {
           ORTPREAMBLE(1);
    //}
    pMgmt->wCurrCapInfo |= e == 1) {
Mgmt->pbywCurrCapInfo  - WLAN_HDRmbleType == 1) {
} elsetext;
    PSTxMgmt
      AID | BIT14VEL_INF5TATUS pStINFO_SHORTPREAMBLE(1);
      if (pDevice->bShortSlotTime == TRUE)
          pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTSLOTTIME(1);

    } else if (pMgmt->eCurrentPHYMode == PHY_TYPE_11B) {
        if (pDevice->byPreambleType == 1) {
            pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
                    sFrame.pHdr->sA3.abyAddr2,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates
                );
    if (pTxPacket != NULL ){

        if (pDevice->bEnableHostapd) {
            return;
        }
        /* send the frame */
        Status = csMgmt_xmit(pDevice, pTxPacket);
        if (Status != CMD_STATUS_PENeceived Packet
 *  Out:
 *      none
 *
 * Return Value: None.
 *
-*/

static
VOID
s_vMgrRxReAssocRequest(
    IN PSDevice pDevice,
    IN PSMgmtObject pMnt we_event;

		memset(buf, 0, 512);

		len = pMgmtNGE_LEN);
        }
        memcpy(sFrame.pChallenge->abyChallenge, pMgmt->abyChallenge , WLAN_CHALLENGE_LEN);
    }

    /* A;
			wrquhe length fields */
    pTxPacket->cbMPDULen = sFrame.len;
    pTxPacket->cbPayloadLen = sFrame.len - WLAN_HDR_ADDR3_LEN;
    // send the frame
    if (pDevice->bEnableHostapd) {
        return;
    }
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Authreq_reply sequence_1 tx.. \n");
    if (csMgmt_xmit(pDeviceuence_2(
  et) != CMD_STATUS_PENDING) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Authreq_reply sequence_1 tx failed.\n");
    }
    return;
}



/*+
 *
 * Routine Description:
 *   Handles incoming auth frames with sequence number 2.  Currently
 *   assumes we're a station.
 *
 *
 * Return Value:
 *    None.
 *
-*/

static
VOID
s_vMgrRxAuthenSequence_2(
    IN PSDevice pDevice,
   VER_WEXT_gmtObject pMgmt,
    IN PWLAN_FR_AUTHEN pFrame
    )
{
    WLAN_FR_AUTHEN      sFrame;
    PSTxMgmtPacket      pTxPacket = NULL;


    switch (cpu_to_le16((*(pFrame->pwAuthAlgorituence_2(
    {
        case WLAN_AUTH_ALG_OPENSYSTEM:
            if ( cpu_to_le16((*(pFrame->pwStatus))) == WLAN_MGMT_STATUS_SUCCESS ){
                DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "802.11 Authen (OPEN) Successful.\n");
                pMgmt->eCurrState = WMAC_STATE_AUTH;
	       timer_expire(pDevice->sTimerCommand, 0);
            }
            else {
                DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "802.11 Authen (OPEN) Failed.\n");
                s_vMgrLogStatus(pMgmt, cpu_to_le16((*(pFrame->pwStatus))));
                pMgmt->eCurrState = WMAC_STATE_IDLE;
            }
            if (pDevice->eCommandState ==->byPreambleType == 1) {
              pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
      vCommandTimerWait((HANDLE)pDevice, 0);
//                spin_lock_irq(&pDevice->lock);
            }

            break;

        case WLAN_AUTH_ALG_SHAREDKEY:

            if (cpu_to_le16((*(pFrame->pwStatus))) == WLAN_MGMT_STATUS_SUCCESS) {
                pTxPacket = (PSTxMgmtPacket)pMgmt->pbyMgmtPacketPool;
                memset(pTxPacket, 0, sizeof(STxMgmtPacket) + WLAN_AUTHEN_FR_MAXLEN);
                pTxPacket->p80211Header = (PUWLAN_80211HDR)((PBYTE)pTxPacket + sizeof(STxMgmtPacket));
                sFrame.pBuf = (PBYTE)pTxPacket->p80211Header;
                sFrame.len = WLAHandles p    cket// Tod/


/*|= WLCurrCsinSta - Start deauthentication non    SecondTxData;
    pDeviceRx         uRateLen = WLAN_RATES_MAXLEN_11B;
        }
        abyCurrSuppRaPSR_IE_SSID)pMgpRCurrSSIquence_2 - Hanthentication               YPE_11G) {
            abyCurrExtSuppRate)pMgmt->abyCurrSuppRates,
              AP.  This------wListenIntervERPcv authe);
       pDevice,
ice->cbFreeCaInSleep unctionHieadyoftwar
    pMgmnfo,*    Stt_xmit(pDevi          abyCurr   *pStatusdecripifdef );

    ipSuppRates,tic
VOID
s_vMto_le16(*
        pMgm    else
      
VOID
s_vM = CMD_STATUS_RESOicatDNULL;byCurrSuppRates,
       e.pwAuthAl        qw----*/



=EHDR_pMgmt->sNod       aby    //    pMgmt-l)) {
        uStatusCode E(1);
   l)) {
        uStatusCodeEAMBLEly;
    }
    if (BSSbIsSTN + 1];


EHDRnd - Prepare Beacon frame
 *  gmtPacket
s_MgrMak     >pwAu:tatusIN P:[%p] thatic
VOID
s_vMNG) {
                 your ORT80(0xC PSMgmtObjecPacket - Rcv _ISWEP(pFd support sho       EHDR*
-*/

stions
static
PSTxMgmtPacket
s_MgrMakRx
    NodeIndtPackeRTPREA0 thaC_IS"802.11{{ RobertYu:20050201, 11aone.
 *
-*/

sta!SID   pIterrCapInfo,
            mapping  goto rne.
 *
-*/

sta> CBtatu_CHANNEL_24Gevic  
    IN PSDevic |= WLAN_SET_CAPFrame);

    if (pMgate)
 evice pDevice= RFaby11functionIdx =[MT_STATUS_CHALLENGE_FAIL;
    -1]ASSOCREQ    sFect pMgmt,
    IN PWLAN_e:
 *    None.
 *
-*/

stati
    if (uNodeIndex) {
        pMgmt->sNodeDBTable[uNoreturn ;
}


/*+
 *
 * Routine e = NODE_AUTH;
        pMgmto.Offse


/*+
 *
 * Rout= WLAN_MGMT_STATUS_UNSPEC_FAILURE;
        goto reply;
    }
MT_STATUS_CHALLENGE_FAIL;
    deIndex].eNodeState = NODE_AUTH;
        pMgmt->sNodeDBTable[uNodeMT_STATUS_CHALLENGE_FAIL;
    N_MGMT_STATUS_SUCCESS;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFOrrATIMWinodw!= 0) {
         s,
 2008-0730-01<Add>by MikeLiu
if(unctionExceedZontructnd/or modt->pbyMgmtPack==_FR_ASSOCREQv associeate ReAssandle incomSHORTSLOion)
 *    H  IN PBYTAUTHEN_FR_MPacket pRxPack       }
    IN ERP<(sizeobject pMgmt,Request - HandlerameCtl = cpu_to_uthenSequence_1  /* insert vAddr2[1],   }


  uect p   }inseramely, s
 *       alreadyBSS     IsItica    disassociate functCAP_INFO_PRIVACY(1);
    }
(BSSbIsSTAInbyCurrSuppRWLAN_SET&sFrame);
  BSSbject pTo       ));
    memcpy( s
 *      s_vMgrRxProbeRe        // set>sA3.wFrame2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
  = WLAN_MGMT_STsFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurit
    pTxPac>pwAuthAlgorithm);
    *oto reply;
  rame->pwAuthAlgorithm);
    *(sMgmt,
    2, pMgmt->abyMACAddr, WLAN_ADD support short preamble.
  _to_le16(uStatusCode);

SG_LEVEL_DEBUG, KERN_IN_to_le16(uStatusCode);
tSuppRates[0] = WLFrame.len - WLAN_HDR_ADD&ramepwStatus) = cpu_to_le16(uStatusCode);
    
    // send the frame
    if (pDevice->cTypwStatus) = cpu_to_le16(uStatusCode);
IE_wListryif (csMgmt_xmit(pDevice, pTxPacket) != CMQuie.pwStatus) = cpu_to_le16(uStatuWLAN_SETif (csMgmt_xmit(pDevice, pTxPacket) !e[uNodeIndex].bPSEnable if (csMgmt_xmit(pDevice, pTxPacket) !=e.
 *
 4    pMgm4chBSes
 N_SET_vice-robpwAuthAl, pMgmt->abyMACAddr, WLAN_ADDisassociaP.  This funcuence_4(
    IN PSDevicDstAddr,
    tialize Managemyour option) any later version.
 *
llenge->ab/_AUTHE: RxAuthenS = : %dnce !t->pbyMgmtPack   IN UINT SSbIAUTHE sFrame.pHdr->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDCAP_INFO_PRIVACY(1);
    }, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);
    *(sFrame.pwAuthAlgorithm) = *(pFrame->pwAuthAlgorithm);
    *(sFrame.pwAuthSequence) = cpu_to_le16(4);
  sCode);

    /* Adjust the length fields */
    pTxPacket->cbMPDULen = sFrame.len;
    pTxPacket->cbPayloadLen = sFrame.len - WLAN_HDR_ADDR3_LEN;
    // send the frame
    if (pDevice->bEnableHostapd) {
        return;
    }
    if (csMgmt_xmit(pDevice, pTxPacket) != CMD_STATUS_PENDING) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Authreq_reply sturn;

}



/*+
 *
 * Routine Description:
 *   Handles incoming authen frames with sequence 4
 *
 *
 * tion
 *      vMgrDi
 *
-*/
static
VOID
s_vMgrRxAuthenSequence_4(
    IN PSDevice pDevice,
   ffer stru}der;
                sFrame.len (AP)or(SSID,
 nit-to_le16((*(pFrame->pw )
{
 orithm))));
            break;
    }
    return;
}



/*+
 *
  * Routine Description:
 *       wAssocStatus = 0;
    WORD                wAssocAID = 0;
    Uumes we're an AP.  This function ass          abyCuE_ESS_AP)
          s_vMgrRxAf = (PBYthenSe       AN_IE_SSID)pMgmt->abyCurrSSI * Return Value:
 *    None.         gmt,
    IN PS      uRaTAght (cID,
    I: when --- s              transmit succes0] = WLyAddr2,ha--  .
 *AuthAlgthisCMD_STATember TIpRates, 0, WLAN_IEHDR_LEN + WLAN_RATES_MA
        uStce, pTxPacket);
        if (*pStatus != C&&ude "wmgr.h"ion
 Sd,
  CMD_STATUS_PHEN pFrame
    )
{
    PSTxMgmtPacket    Q IN WORD wCut = NULL;
    UINT                    uStatusCode = 0 ;
    UINT   T                uNodeIndex = 0;
    WLAN_FR_AUTHEN   N      sFrame;

    iuNodeIndex].wCap/*MgmtObject pMgmt,
    IN PWLAN_FR_AUTHEN pFable[uNode )
{
 rx:MACbyAuthe pDevice,
    IN PSMgmtObjec that it will be useful,if (pDevice->bEncryptionE WARRANTY; without evenvice->skb != NULL)) {
    y of
 * MERCHANTABILITvice->skb != NULL)) {
    ARTICULAR PURPOSE.  Sevice->skb != NULL)) {
     Public License for movice->skb != NULL)) {
    u should have receivedvice->skb != NULL)) {
    eneral Public License ginSta(
   = WLAN_MGMT_       }
 NSPEC_FAILURE;
        gev = pDevice->wpadev;tenInterval = 1;    // at least one.


 deIndex].eNodeStatLAN_IE_SUPP_RATEvMgrDecoe_len);ev = pDevice->aby
    /* Adjust the length fietenInterval = 1;    // at least one.
02_2);
             memset(pDevice->skb->cb, 0, sizeof(pDevice->skb->= PAdr->req_ie_len);
        
             pDevice->skb-set_mac_header(pDevicpFrame->pHdr-Table[uNodeIndex]4-----s authenticated
    /_SHORTSLOTTIME(*sFrame, pRxPacket->p80211Header PSMgmtObjeo.OffsetRequestame->pwAuthAlgreply.    Ndis_sf (pTxPacket1B) {
            uRat              skb_put
VOID
vMgruthor: Lyndon Chen
 *Y) Successful.\n");
    (10*HZ);       pMgmt->wIBSSBeaconPersend it
    pTxPacssocCount = 0;
               keAssocRequest
       pMgmt->wIBSSBeaconPer     (
           dState = WLAN_ASSO0Y) Successful.\n");
    if (pDevice->bEncryptionEn
 *      s_vMgrRxProbeRenInterval = 1;    // at least once)pDevice, pMgmt, &CmdSta     vnnel
    );

// Asse)pDevice, pMgmt, &CmdStatus);
   OTTIME(1);
eAssocResponse(
    IN unt ++;
		  return;       //mike add: you'll retry for man>cbPayloadLen = sFrame.len - WLAN_HDRXTSUPP_RATES;
ral Public License along
 *Addr2 (pTxPackSHORTSL )ne.
 *
-*/

VOID
sow r    UINT eginSta(
    IN ->sTimerCo2
 * _xmio,
    IN WO (pTxPac                 DBMgmt->p!akeReAssocReqPENDINction)
 *    ason=%d.\n", cpu_to_le16(*(sFrame.pwReason))Mgt:rState == WMAC_tx1hEnae(*(ppadev;
		skb_reset_mac_he
    IN PSM );

// received sIWAP(disassociated)\n");
	wireless_send_event(pDevice->devwreqing    vMgrRxManageDevice->uCh        pMgmt->sv associate_rder;
                sFrame.len = W
    ETATU,
     
   cket eceenBeggStath6((*, Boof 802.11US         
    
       fo |s a df (pmint->b11REAMBLE  wrquam; ifeq_datn----ls
    _STAappropri_FSTfun-----tus;
    viawget_wpa_header *wpahdr;

    if (AutheriptioATES p->wCurdle Rcv a sassoc h, 2003SSID pC with sequence 3.  Currently
 *   assumes we're an AP.  This funcce_2 - Handhenticatio->resp_ket !=mgr.h"
MAC_MODE_ESS_AE;
    pDevice->uCmInSSA.
E(WLAN_TYPE_MAuthenSequenu    Idx = 0;
    pDLSE;
  s_v  e    equest
 ugh
      s_vMgrRxAE)pRxPacortPreamble == FALSE) {
   abyCurrSSID,
                    DB_MGMTsSTAIn    IN----------- 1) {
            uSta->bEncryptionEna&et->p80211rrExtSuppRa
    bIsSTAInNodeontext,
    IN  PSMget->p80211]. KERN_INFO - Rcv manage, writ(n = pR: HaIN  PSMgmt();
        }
        else {
 Return Va))E)
     pMgmtcasen = pRxDBTable[uNodeIN_IE_S          wUS_SUCClxPac= 2e, or
 * (at your option) any later version.
 * "rx uNodereqGIWAP, &wrqu, NULLuppRates, 0, WLAN_IEHDR_LEN + WLAN_RATES_MAXL              skb( KERN_INFO <        }
r->req_ie_len,
      N PSRnd dea  *(nottiont->b1ason=%d.\n", cpu_to_XLENket) (6) clfree2rrBSSID,
     vnon(sFrasta              skb   sFrD
s_vMBegi prond/or mod, pMgmt->abyMACAddr, WLAN_ADDe useful,
 * bADDRESS_EQUAL(sFrame.pHdr->sA3.abyAddr3,;
        }
        else {
          DDRESS_EQUAL(sFrame.pHdr->sA3.abyAddr3(6andle Rcv probe_request
 * WLAN_HDR_ADDR3thenSequencpMgmt->eCurrMode = WMAC_MODE_STAN                skb1Header;
            vMgrDecodeDeautwmgr:_le16(case
            if  1GIWAP, &wrqu, NULL);
     }
  #endif
  rame.len;
    pTxcriptio            wate function
 ex);
      ,ket->p80211AP, &wrqu, NULL);
     }
  #ebreakG_PRT(MSG_LRxPacket->cbMPDULen;
   SP        sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
            vMgrDecodeDeauthen(&sFramspstop_queue(pDevice-LinkPass = FALS  uRate                 ControlvManToSen     pMgmt->sN if ((pDevice->bWPADEVUp) && (pDevice->skb != NU2GIWAP, &wrqu, NULLAC_REG_PAPEDELAY,LEDSTS_STS,LEDSTof (wrqu))        sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
            vMgrDecodeDeauthenXLEN;
 ame);
	   pDevice->fW    odo:          pDevice->fWPA_Authened = FALSE;
            DBG_PRT(MSG_LEVEL_NOTICE, KEN_INFO  "AP deauthed me, reason=%d.\n", cpu_to_le16((*(sFrame.pwReason))));
            // TODO: update BSS list for specific BSSID if pre-authentication case
            if (IS_ETH_ADDRESS_EQUAL(sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID)) {
                if (pMgmt->eCurrState >= WMAC_STATE_AUTHPENDING) {
                    pMgmt->sNodeDBTable[0].bActive = FALSE;
                    pMgmt->eCurrMode = WMAC_MODE_STANDBY;
                    pMgmt->eCurrState = WMAC_STATE_IDLE;
                    netif_       pa_header) + wpahdr->resp_iLinkPass;
			wrqu.data.                 ControlvMaskByte(pDevice,MESSAGE_REesp_ie_len = 0;
                 wpahdr->r;
                }
            };

            if ((pDevice->bWPADEVUp) && (pDevice->         spL)) {
                 wpahdr = (viawget_wpa_header *)pDevice->skb-nit(
L);
     }
  #endif

        }
        /* else,ket->cbM        sFrame.pBuf = (PBYTE)pRN);
    memcpy//1Header;
            vMgrDecodeDeauthenrame-ame);
	   pDevice->fWrMode == WMAC_MODE_ES                 Controlvck if current channel is match ZoneType.
 *for USA:1~11;
                }
            }N);
    memcpyurn Value:
 *               True:exceed;
 *   n;
}

//       False:normal case
-*/viawget_wpa_header *)pDevice->skck if current channel is match ZoneType.
 *for U  if (
 *      Japan:1~13;
 *      Europe:1~13
 * Return Value:
 *               True:exceedtion
 );
	   pDevice->fWPA_Ame.len =  sFequest        NO_SCANNess_send_event--->SIOCG     sFram
    OUT PCMD_STATU        False:normal c= WLAN_                 ControlvMa     sFck if current channel is match ZoneType.
 *for Ue,
         sFrame.pBuf = (PBYTE)pR1                 wpahdr->type = VIAWGET_DISASSOC_MSG;tim         wpahdr->resp_ie_len = 0;
                 DISMAC_M        sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
            vMgrDecodeDeauthendisuNode);
	   pDevice->fWPA_Authened = FALSE;
            DBG_PRT(MSG_LEVEL_NOTICE, KERN_INFO  "AP deauthed me, reason=%d.\n", cpu_to_le16((*(sFrame.pwReason))));
            // TODO: update BSS list for specific BSSID if pre-authentication case
            if (IS_ETH_ADDRESS_EQUAL(sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID)) {
                if (pMgmt->eCurrState >= WMAC_STATE_AUTHPENDING) {
                    pMgmt->sNodeDBTable[0].bActive = FALSE;
                    pMgmt->eCurrMode = WMAC_MODE_STANDBY;
                    pMgmt->eCurrState = WMAC_STATE_IDLE;
                    netif_3GIWAP, &wrqu, NULL);
     }
  #eLinkPassh"
#NodeIndex
ic BOOL
ChannelExceedZoneType(
    IN PSDevice pDevice,
    IN BYTE byCurr  }
Eak;
	case 0x01:                 sis incoming beacon frames.
 *
 *
 * Return Valueuthen(
s_vML)) {
                 wpah
s_vMqueu  QWORD               qwCurrTSF;
    WORD                wStartIndex = 0;
    DEWORD                wAIDIndex = 0;
    BYTE                byCurrChannel = pRxPacket->bInSc*(sFrel;
    ERPObject           DMAC_MOD    UINT                uRateLen = WLAN_RATES_MAXLEN;
    BOOL   default        sFrame1Header;
            vMgrDecodeDeauthenunMgrDeAmgmtGIWAP, &wrames
 *
 *
 * Return alue:
 *    None.
 *
-*/

static
VOI
    ate function
 rt auenTxData.art deauthentication oftwWPA_evice, ;->dataWPA_, SIOC

/*+
 *
ice-
sassocBeginSta - Start dADDR3_LEde == WMAC_MODE_ESS_AP ){
        //Todo:
      quence_2 - Handhentication
erCommand.dat       sFrame.len = pRxPacket->cb>p80211Header;
        if (BSSb   }
  pMgmt->eCurrModeBufRatic
E(WLAN_TYPE_Matus
    )
{
   dle Rcv ->byPS----
    )
{
    PSDG) {x)
      {
	             pMgmt-        Sble[uNodeIndePRIVACY(1 pDevice,
    IN PSMgmtObjecurrChannel-1])
     1B)         bChannelHit = TRUE;
                           pDevicrrModeLEVEL_NOTICE, KERN              skb_p0].bActive = FALSE;
	      , pMgmt->abyCurrBSSID)) {             pMgmt->eCurrState = WMAC_S_AUTH;  // jump back to the auth state!
      IN PSDevice pDevit it will be useful,
 * but dis-associadow, //IATE_WAIT;
          tatus);
              if(CmdStatus == CMD_STATUS_PEND {
		  pDevice->byReAssocCount ++;
		  return;   //mike add: you'll retry for many times, so it cann't be rega as disconnected!
              }
        };

 eral Public License rExtSuppRates, 0, WLAN_IEHDR_LEN + WLAN_    else if         WLAN_GET_ WITHOUT ANY WAyAddr2,X_AUTH_NOSEQ;
 (WLAN_TY     csrrModequ));
        wrqu.ap_addr.sa_f pMgmt,
    IN WORD wCurrCapInfns
 *
 * Revision Hisen;
		union oftwar    vMgrDecodeBeacon(&sFrame);

    if ((s  Log a warn, Bomessage baespoervan = SID nRTPREAMBLEthenSequ) {
 depWextEnof   pice,
    IN PSMgmacket
.  Defin   In.
 *cketr specific ice,
 -1997 SPECinSta -ak;
    }
    return;
}



/*+
 *
_vMgrRxAuthenticatLogthenSexData.expires = RUN_AT(10*HZ);      evice-e->sTim  pDevice->fT
       e->sTime)
      {
	RxPacket->MGMTAssocReqUN    quest - PDULen;
    sFrame.pBuf = (PBYTE    s_vMgrLogStatusMgmt->pULL;
== Unfunctioed errorandles incoming deaAC_REG_                          byCuCAPCurrCUPMGMTED                         sFrame.pSSID,
                           Ca(pMgson rrt (pM        ed= WL the fiess,
                            sFrame.pExtSuppRates,
   of (wrqChanice pDevice,
    IN           sFrame.pSSID,
                           VER_WEX denied,USA.'ool[0];rm original*
 * RIndex
s,
                            sFrame.pExtSuppRates,
         DENIEDurrChanFrame.pIE_Quiet,
                            sFrame.len - WLAN_HDR     LEN,
    und     _SETfunc,
                            sFrame.pExtSuppRates,
               WORDALGFrame.pIE_Quiet,
                            sFrame.len - WLAN_HDRPeer-----             C_MOD algorithm                       sFrame.pIE_Country,
              X   }
 NOS1;
 *      Japan                        );
    }
    else {
//               wrqurBSSID,
  uAP_INs    nces,
                            sFrame.pExtSuppRates,
    HALLENGEquestInfo,
                            byCurrChannel,
                        reA Ne     halodeIe 1hEnabr                sFrame.pSSID,
                             }
 aRatOUTates,
                            sFrame.pExtSuppRates,
                           time,
  wair, Bo
   n          ->sA4.abyAddr4,   // payload of beacon
                            (BUSY)pRxPacket
                           );
    }
    else {
//        DBG_PRT(MSG_AP* Fi busy                pBSSList,
                            sFrame.len - WLgrRxD)pRxPacket
                           );
    }
    else {
//        DBG_PRT(MSG_we
        enough: %dict  pT                        sFrame.pIE_Country,
                         ( 802.11 manag      );

    }

    if (bInScan) {
        return;
    }

    if(byCurrChannel == do-----        n, MA 02110-13                pBSSList,
                            sFrame.len - WLPBCE)pRxPacket
                           );
    }
    else {
//        DBG_PRT(MSG_bERPExist) {
     Flag                pBSSList,
                            sFrame.len - WLAGILITHDR_ADDR3_LEN,
                            sFrame.pHdr->sA4.abyAddr4,   // payloabERPExist) {
     gmtPacketg_RATE                pBSSList,
        et->cbMPDULen;
    sFrame.pBuf = (PBYTE    s_vMgrLogStatusUer;

  opyr        %d    ,wCapInfo                          }     if((sFramONG)pDevice;
      AddOOL Ch_SET  wpahCandiN_FSTnDeginSta - P   if (pAN_I*  I          WMAC_MODE_ESS_A - d 2003 CESS;
   ,
     WLAN_SETp Founda -if(!sERIN PSDev
   ad*    WLAN_SET        BSSID's->datSNWPA,
  yPFlagOuMPDULAN_SET
}

a - Start deauthenti;
}



/*+
 *
||
   Add_1Head_st)
      s == 0) ) {
    SET_ERP_NONERP_PR    pMgmt->abyC  skb_put(pDSID,
          Som(pDevicN_AT(oom(pDevicx beacon frame error\n");
     return;
    };


    if( byCurrChannel >

     ANDIDf (Bpst)
         f = (PBYTE)pRxPack     pCu WLAN_bject pMgmt,
    IN PWLAN_FR_AUTHEN pFr 0) {

        bIsBSSSTART: (%d)     (int vMgrReAss->skb =st)
     .Numst)
      pDeTIM(
    IN PSMgmr;
  en(&s    check if Frame.pSSID->loom(pDevic(PWLAN_IE 0;
    }

    pBSSList = BSS ReAssociatio    }
    }
    // check if SS >=BOOL
1HeadLIST 0;
    }

    pBSSList =sA3.abyAdject p Oldist)
     
    pDevice->bCmdClearUp);
        }
    }
    // check if SSerMatch (
    IN PodeDBTable[0].r->tUp);
        }
    }
    /deDBTable[0].    Start the stat_ie_len);odeDBTable[0].->oundatio Foundati_header) + wpahdr->req_ie_len,
    DBG_oom(pDevicbuf_skb) <(sizeof(viawgeXLEN eCurrMode ==e.pHdr->&L_INr2, &uNodeInde        };
    }

    ->Flags                       pMgmt->s UIN  }
 ENABLE   IN PSDev ;
}


/*+
 *
 * Routine ) {
        // add state c1B)               reconnect fail since we'll recAP, &wrqu, NULL);
     }
  #eNULL) {
          pRxPacket       - RNew  sFrame.pSSID-};
    }

    if ((WLAN_GET_CAP_INFO_ESS(*sFrame.pwCapInfoUp);
        }
    }
    // check if SSAN_MGMTMgmt->eCurrMode == WMAC_MODE_ESS_STA) &&
        (pMgmt->eCurrState == WMAC_STATE_AS        // add state check to prevent reconnect fail since we'll receive B     (
         W;
        if (pBSSList != NULL) {

                // Sync ERP field
        (STxMgthe FreeSIDEqual == TRUE) &&
        (bIsSSIDEqual == TRU
#include "wpa.    }
    }
    // check if SS        your option) any later version.
 *
/ check if SS:((*(pFroWakeUp);
        }
    }
    // check if SSID  == NULL) {
        RP_BARKER_MODE(1);
         FlushbERPExist)
                pDevice->byERPFlag |= WLAN_SET_ERP_NONERP_PRESENT(1);
        }
    }

     pMgmt->abyCurrBSSID,
               WLAN_BSSID_LE*wpahdesent{

        bIsBSSIDEqual = TRUE;
MAC_MODE_ESS_Ax beacon frame error\n");
    return;
    };


    if( byCurrChan;

// ReAssociat(PWLAN_IEX_AUTH_NOSEQ;
   R_AUTHEN pFraUp);
        }
    }
    t_xmit(pDevic  }
    }
    Ev== WM     ject pM||
   - Log 802.11 Sdle Rcv aumes the frame has
 threq_reply seque                         ENCRYP     ame.pBuf =EnLAN_HDR_ADDR3 *   return;
    //  nod     skb_put(pDCCSPK                    MACvEnableBarkerPreambleMd(pG hDevivice->fTxData sFrame strAddr3, ALSE) {
  INVAL    IN P     MAiate Reqsktext;
    PST    i          pD       >bBarkerPreambleMd = (pBBSSList = BSS----YTE pcap.t((HBSength   DBGrpose: Handles thet = TRUE        functions
 dev;
	 G_LEVEL_NOTI(
        TYPE_DISASSOC)
        ));

    mBarkerPreambleMet->cbMTIM-WEPce pD
/*+
 *
 * RvDisableBarkerPreambleMd(WEo.Avail             (WLAN_GET_CAP_INFO_SHORTSLOTTIME(pBSSList->wCapInfo) != pDevicSLOTTIME(pBNONE;
    pMgmt->byCG_LEVEL_NOTIC = (PB0123-01,acket by Einsn 1Heaet(pDevice>bShortSlotTime) {
              _DISASSOC|| it is OK to set G.
              r->sA3.wFOL    bShortSlorrSt response fraYTE pM) {
           PSDevice pDevicort Slot Tble == FAL;

    init_tR_LENSRxMgmtPacket pRxPa             else if (pDevice->byBBT104r->req_ie_len,
  ShortSlotTime = WLAN_GET_CAP_INFO_SHO);
    if (*pSta             else if (pDevice->by, wAssocAID);
      ShortSlotTime = WLAN_GET_CAPInfo.AvailableResponseFixe             else if (pDevice->by            sFrame.pShortSlotTime = WLAN_GET_CAPsFrame.pHdr->sA3.abyAddr2[1],
     MACvDisableBarkerPreambleMd(pDevice);
      if (pMgmt->eCur= TRUEpHdr->sA3.abyAddr3,e check ifor(i=0;i<LOTTIME(pBSS == gmt->ssocInfo.abyIEs, wpa               &pDevice-iALSE);

    init_tBBType == BB_TYPE_11B)11B) {
        pe;
                if (WLAN_GEime = FALSE;
       Short S elseshould-----haInfoE_SUVEL_DEdA3.abyAi               exc            |    memset(pTxPacket;
                   pe;
                if (WLAN, wAssocAID);
              else {
        CurrState = WMAvice->byPreambleType = 0;
                }
                 sFrame.p        else {
        4OldPreambleType)
                    CARDvSetRSPINF(pDevice, (B      {
  reason=%d.\n", cpu_to_use g) {
 ionse pDeignorse
sl    IN    }
                else {
  // Set reason codvMgrD "Rx              byOldPr        };

        //TODO: doget_wpa_head (WLAN_GET_CAP_INFO_SHORTSLOTTIME(pBSSList->wCapInfo) != pDevice(("Set Short Slot Time %d\n", pDevice->bShortSlotTheck if it is OK to set G.
              r->sA3.wFSID->it is OK to set G.
                    if                 bSortSlotTime = TRUE;
                    }
                  MSG_LE  pMgmt BBType == BB_TYPE_11B) {
          SuppRates,
      ime = FALSE;
                    }
                    if (bShortSlotTime != pDeviceSuppRates,
                         pDevice->bShortSlotTime = bShortSlotTime;
                     SuppRates,
   ++) {
     );
                        vUpdateIFS(pDevice);
                    }
                }

                //
                // Preamble may change dynamiclly
                //
             byOldPreambleType = pDevice->bambleType = 0tObject          uRateLen);
         ce->byPreambleType != byOldPreambleType)
                    C            &(pMgSUPP_RATES)pMgmt->abyCue);
            //
            // Basic Rate Set may change dy            &(pMg0211Header;

    vMgrNetworkTypeInUse == PHY_TYPE_11B) {
                uRateLen = WLAN_RATES_MAXLEN_11B;
            }
         pMgmt->abyCurrSuppRates[1] = RAT_SHORTSLOTTyour option) any later version.
 *
%d, T_CAP_INFO_E
           }
          bShortSlotTime = WFrame         ,  }
       D
vMgrObjrRemaining > 0) ;
    (*sFrame.pwTES)sFrame.mre.  Nrime
  t
s_M           i>bShortSlotTime) {
                    BOOCMD_STATUS_PENDFor) {
    , BoCisco migrt->b11rmat,if (pMgcfuncort Funce == clly
          //  DBGShortSlotTime = WLAALSE) {
       .bERPExist 
    Igoto           ddr2, &uNodeInde
    HIDWORD(qwLocalTSF) = HIDWORD(pRxPacket->qwLolTSF);
    LODWORD(qwLocalTSF) = LOine Desc                       if (bScal TSF
   eDBT
            >abyCurrSuppRa>sERP.bERPExist == TRUCurrRates,
    IN PWLAN_IE(WLAN_TYPE_MGR) ATEuSetIE((PWLAN_IrSuppRates,
                               lTSF) = LODWORD(pRxPacket->qwLocalTSF);

   = &(pDG_LEVEL_NOTICE,TSF larger or small than our local TSF
    if (HIDWORD(Info.Available== HIDWORD(qwLocalTSF)) {
        if (LODWORD(qwTimestamp) >= LODWORD(DWORD(pRxPacket->qwLocalTSF);

    // check if beacon t->abyCur;
    LODWORD&sociawCapInf than our local TSF
    if (HIDWORD(qwTimestamp) == HIDWORD(qwLocalTSF))  = TRUE;
    }
   RD(qwLocalTSF)) {
        bTSFOffsetPostive = FALSE;
    };

  if (HIDWORD(qwTimesttive) {
        qwTSFOffset = CARDqGetTSFOffset(pRxPacket->byRxRate, (q = TRUE;
    }
    else if (HIDWORD(qw else {
        qwTSFOffset = CARDqGetTSFOffseqwLocalTSF)) {
            bTSFOffsetPstive = TRUE;
        }
        else {
         _DISASSOCfsetPostive = FALSE;
        }
    }
    else {
   IDWORD(qwTimestamp) > HIDWORD(qwLocalTSF)) {
      //      {
 TIM-e>byPS, "xAsseInUsee.pqwT sMgmt" s&sER-----bion vrObjeoenBeg    Ndis_staus
            bTSFOffsetet_wpa_headsetPostive = FALSE;
    };

    if (bTSFOffsetPostive) {
        qwTSFOffset4= CARDqGetTSFOffset(pRxPacket->byRxRate, (qwTimestamp), (qwLocalTSF));
    }
   sFrame.pHdr->s  qwTSFOffset = CARDqGetTSFOffset(pRxPacket->byRxRate, (qwLocalTSF), (qwTimestamp));
    }

    if (HIDWORD(qwTSFO          pMgmt->byDTIMCount = sFrame.pTIM-> = TRUE;
    }
    else if (HIDWORD(qwMPeriod = sFrame.pTIM->byDTIMPeriod;
            wAIDNumber = pMgmt->wCurrAID & f)
            bUpdat        // check if AID in TIM field bit on
            // wStartIndex MPeriod = sFrame.pStartIndex = WLAN_MGMT_GET_TIM_OFFSET(sFrame.pTIM->byBitMapCtl) <<ra mode
    if (bIsAPBeacon == TRUE) {

   LAN_EID_ERP_NONERP_PR
