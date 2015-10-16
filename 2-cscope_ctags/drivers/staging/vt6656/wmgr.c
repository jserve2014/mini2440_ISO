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
 working Tecp, 2003->eOPMode = OP_MODE_ADHOC;torking Techhnologies,bLinkPass = TRUEerved.
 *
 * TControlvMaskByte(his pro,MESSAGE_REQUEST_MACREG,MAC_REG_PAPEDELAY,LEDSTS_STSeral PubINTER)are; you can rmemcpynd/or mo->abyBSSID, pCurrare FoundatiWLAN_ vers_LEN);
rved.
 *
 * TDBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Join I * Cok:%02x-am is distributedght the \n",rved.
 *
 * T*
 * ThMgmtithern; e vers[0]t it will be useful,can but WITHOUT ANY 1ARRANTY; without eve hope implied warrant2 of
 * MERCHANTABILITY or FITNESS FOR A P3 of
 * MERCHANTABILITY or FITNESS FOR A P4 of
 * MERCHANTABILITY or FITNESS FOR A P5]RANTY; without evublished bycan // Preamble type auto-switch: if AP can receive short-pis progrcapRRANTY; withou// and to regbutey settng Tisither  ersioon, w, Inn turn on too.se, orcan (at to (n 2 oGET_CAPnyou _SHORTPREAMBLE(oTHOUitwCapInfo)) {RANTY; without evhdation, ayChen
 *
Tm; i= Chenou can Sher Chen
 *
are; you can r}rved.
 *
 * Telsecan Author: Lyndon002
 *
 * FuDate: May 8ologi    nsMgrVIA NeInitial - Ini// ChangetChen
 *
     mustFloo RSPINF againre; youe Free ARDvSet  vMgree Softwd (BYTE)02
 *
 * FuBB    ene: wmgr.c
 *
 h t dapithebeacoBeginSta - StabMgryrigfunBion
 ToSend((HANDLiate fReA,n the ie along
 *- Reset Maset ize Management  the imeTHOUSpyrig= Wms oSTATE_IDLwaeginSta -}are; yonse - Hreu ca;
}



/*+ou can Routine Descrip10-1:lizesociHWreetsynchronst -a specificcan Cfrom know *
  Clist.onalizlize  asso Value     v  PCM   s_US vMg-*/
oquesc
VOID
s_te fSthenuthe(rved.IN PSologie  Objet data RRANTYIN UINTorking TecuBSSc.
tionate fxPKgrDeuthe Rcv THOURRANTY warfunDc10-1
   pequeusrved.)ize ManPSthe VIA Necvt deau= &ee Softwarsleion sse along
 * witenSthennce_3e Rcand
 *    auon -tic110-1 //1M,   2nce_ 5
 *  1ence_18nce_24nce_36nce_54Mrved.grOb plied wSuppRatesG[] = {rposeEID_SUPP_RATES,    0x02ssoci4tion Btion16tion2 Hand30tion4 deac6Cndle Rccv authenticExtation ssquence_ 4Start   EXTdle Rcv aDis HandlC *   110-11Rcv au0ndle Rcion iate f * TBSS *    -  * Th * Cfuns_vMgrR vMgrCdle R//e_4 - 9  s_vM3 *  48ndle Rcv authentication sseAn
 *      vMgrCteOwnIBSS -      i ad_hoc I * Cor Ation 110-1 *      svMgrRxle Rcv aBion
 - JHan
 *      B_MgrMakeBeacon - Create Bae Handltion s Handle Rcv a};
e: wmg*thentS * An - Create_FAILUREa - Sta* Pus_bCipherMatchle RcvRRANTY; without even
 * This progr EncryenBeg s_Mgr Starton - Create ProbeRe&( the imbyCSSPK)respnSta -ponse
 *   rg s_vMrgRxquest -GK)) == FALSE can Managemyour oenBeg) any lNOTICEversioes th "akeProe  bMgrP Fail Statu  tha*      s_vMenticacile Rcv-ion  the imacon
uthe=eacon
 e: wmg// ReAprevious myou isChen
.eog 802     bMgate_r Han*v ass   rightdatin - nd - PrepareMACvRegBitsOfunction
 *     f thTCR, TCR_AUTOBCNTX*       mHandl//nagethopeAutheinformaRcv quenceChen
 *
 *CCKe suence ginSChen
 *
 *Protectacon
 AnStar anagesec Baecrogr"card.hDee Softw*      Chen
 *
 *BarkesocBtion,Mdinclude "80211hdr.h"
#inc"
#inc
#inclwcmd11mgc.h"
#incbssdb.hcludeNonERPPresen110-
#include "t data structurv probe_rete fVIA his progrwBasiponssx.h"
#incl// vMgr"
#in*
 *sanagert abAdd "power.h((PManaLog 8uncticv a_1Mnction
 *  calcul
#inTSF offseerved.//grDitOc Def = Requencd Timestamps ---- M
#ind Local'sgrDi"power.h"vAdjustTSunction
 * ment fubyRxa.h"
-*/

/*-qwBSS-*/
*/


-----/*-*/

/*--TSF-*/

/*--------HWs_vMgrRxintervalude "mac.Writ    vMgI------ Static Va        wonse
   msglevbles           Next TBTTiReseons=MSG_LEV  = ((l    _current_ ---/s_vMgrR_       *) + 1 ) *;--  Static Va-MgrDitic ClSetFirst        ell                ion) any l.
 *;
//opyric---- vers/

static BO   versAddres--*/

/*--ce pDeviher versnction
 ope Fredisp implied warranglevnt fuher vMgrLo6nction
  Beacon frame
 *  a1996vMgrLogStatuson s:c Vard warrantarRxA I =  distributed i= distributed inh*
 *NTY; wi the implied warrantW of
 * MERCY or FITNESS FOR A Py ofon
 MERCYs_MgFITNESS FOR A PARTICULAR PUelude "tmGNU Gen Licenselic Licere detailsyou can Youare uld hav a copRD whope    IN PWCreate ReAsciationeNfiniti    InUsRespoPHY_TYPE_11tanageset M1-ReAs sequence eConfigPHYsponuthenMgrPr(N PSDeN||rved.
 *
 * Tice, IN PSR PS * bVIA Ne p * bket,
  Reviu can Management ate funket  *     = BB
    IN Pare; you can rhfieldES pbeentNT  uNodendex
    )N P  IN PSDevice phen
 *
 *ctionSlotevice,e "device.hta - StaBBce pIN WORD wn; einclude "power.  vMgrDiludee Rcvate Regmt,
 n 2 oIE_unda } bMgrPrepHandle Rcv Packetes,211he Rcv aAson; eRa  );
nnelExcxAuthenticatpCursoc bMgrPr  IN B   IN, 2003 p, 2003ket,
    IN UINT  uNodeIndex
    )N BSRx * bLAN_IE_pRxLAN_IEket,
    Auth  uNodeIndexN PSD)11G,
    IN PSRxMgmtPacket pRxPacket,
    IN BOOL bReAssotes
    );PST    IN PSRx
s_MgrMakes_MgrMagrRxAssocRB
    IN
    IN PSDevice pDevice,
    IN PSMgmtObjecP,
    IN PSRxMddrket,
    terval,
  Cap
#include "c
VOID
s_ListenI------*ket,
    IE_SUPP_RATES on; eundatevice,
    IN PSMgs_vMgrRxDbject on set,
    IN PWLAN_FR_AUTHEN pFrame
 ExtSupse(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgmype   IN PSDevice p

static
VOIDBaecon fr110-1  IN PSRse(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgGPSRxMgmtPacket pRxPacket
    );

// Authentication/de UINT  uNodeIons
static
VOID
s_vMgrRxAons

static
VOID
s_ IN PSDevice pDevice,
    IN PSMgmtObject pMgmt,
    IN PWLAN_FR_AUTHEN pFrame
   IN PS  IN PSDevice pDevice,
    IN PSMgmtObjeIN PSDevice pDevice,
    IN PSMgmtObject pMgmdeauthen functionss
    );

static
VOID
  s_Mgruence_31ID
s_vMgrRxAuthenSequence_3(
    IN PSDevice pDevice,
    IN    IN F* RevHEN pFram IN PWLAN_FR_AUTHEN pFrame
   VOID
s_vMgrRtes   IN P_   IN Pde "rxtx.h"
#TEer PSDIN PSDevice hdr. StrcommRxProa witackndistioHOSnsatTI IN Pts res*      s_vMvMgrRxProbeRquest(
    IN PSDevRpDevRCRS pCur*      s_vM02
 *
 * FuRxacon
|=   IN  PSDtes
    );
   bMgruest(
   FilterOnCapInfo,
    :ou can/

ce pchanneln Strclear NAV"power.h"
SetMediat Obnev-----*/
//2008-07unction
 power.hnnel
 uiatinction
quesndis Probe Res_SUPP_gleveDevice pDevice,
    IN PSMgmtOb<ce ps_bociate RevMgrIN PSDev[%d]t pM

static
VOIDRespon// probe static
VOIbUpdateBBVGA) &&rved.
 *
   IN UINT ymtObject pMgm!2002ou can her ndex[0]tatic
PSTxMgmIE_TIM pTIM   IN PSDevic pDevice,
    IN PSMgmare; you cce pDeVGAGainN PSDevice pDevic  IN PSDevice pDevice,*      s_vMce pDevice,
    IN PSMgmtObject pM- Rese//VEL_INFelotes: msgleve1. It (c-hoc.h"
tT: checkreet,static ion/di--*/

/*-as joce pthe diccon(
Mgmt,
//ce_3t pMgwis301 UY; wC_vMgt rDeAgrMaemberth t2on; eInfr     t pMationsedE_SUalreadyuthenticatiodERCHAhopevMgrT nowle: wmgobe rmt,
  /ndle Rcvfunctiomons
static
VOID
s_vMgrRxProbeRIN PSRxMgmtPacket p
    IN PSDevice pDevice,
    IN PSMgmtObject pM,
    IN PSRxMgmtPacketIN PWLAN_FR_AUTHEN pFrame
   _vMrgRxle RcvID
s_vMgrRxAuthenSequence_3(
    IN PSDevice pDevice,n se   IN PSIN PSMgmtObject pMvMgrRxAssocRe IN PSDeviceyvMgrRmtPack   IN PSD
 *      , &ameterpDevonse
 * 0], sizeof(Device pDevice,
 )ates
    );
nnel
    );

le Rcv aResp[1nce_"
#incls
    );

static
VOIree le Rcv evice pDevi  IN PSDeviceMgmtObject pMgmtSuppRates,
    IN PWLAN_IE_SUPP_RB,
    IN UINT  uNodeIndex
  BgrRxAuthenSequence_4(
    IN PSDevic
s_vMns  u;

static
WO    IN PWLAN_FR_AUTHEN pFrame
 cation set,
    IN PWLAN_FR_G,
    IN UINT  uNodeIndex
  Gic
VOID
s_vMMgrMakeReAssocRespons IN WORD wAsMgmtPacket IN WORD wAsoPWLAN_I  );

staticl,
    IN WORic
VOID
- Rese    bMgrPERPediel   ice pDevi_IE_.UPP_RatTIM  IN PSMgmtObject pMgmt,SUCCESSE pCurrPacket -mtPac//mikeocia: fix HEN pFr Handlr 0.7.0 hidden ssid    INIn WPA edle Rcv a
N PSDe//ee SoftwdCopy,need socBt eVOID
sacon
 Strandle Rcv apvMrg_
ATES ic
PSTx Hand *    _Rebuildle Rcv authenticatprobe_requesVOIDthenticatiacon

 )
MrgRxpt BSS parameters
 *      on
 *  STxMonse
 * //
VOID
s_vMgrRxD  ii , uSameB  INNum= BSS wmgr.c
e(
 for (iicket
bjec< MAXS pC_NUMstat++ IN PSDevice pe(
  IN PSent fraBSS IN [ii].bActenceject pMgmt,    IN PSM  IS_ETH_ADDRleveEQUAL--*/
//2008-073-------er versiociation/diassoctatic
PSTxMgmtPacdN PSD/ ret,
    warPCM++te Probe VOID
s_ve(
  - Reset Manag//e(
  owBSSB (TEs,
    OUT P>=2) {	 //we onlyP     pAP BYTice,
    t,
     IgmtPacket pRxPaDevice,henticatLogSSMode,
 IN _WPAPSK,
  IN PSDCopyricnAuthenmhenSequence_2does not gence#incpai_SUPP-ke Flolsecvice,
     PSRxMgmtPaN PSDevice pDe  Export Vari#incs  2ce p) {----------- soE_SUt,
    -/staticgmt,accord, Boeet, alutineFnction
scro.ppRateinclun
 *    tes
    bWPAValidoutiuenc) Varil//WPA-PSK
static
VOID
s_vMgrRxDe - H-------esponse
 *   ine Descriptio---;
		ates,
-----VOIabyPK    [0]outinPA_TKIP IN PSDeviediseCurrSuppRaate Request frPSMgmt = Ndis802_11ndle Rcv a2En#incd;------= &(2003->s UINT  );PRINT_K("atic
VOID
s_cket
--->SFram  );

crATIMct.
[//statiol[0opyr)->sMgmtOK
// ReAssedis    );
;e(
  rATIMWinodw,
RxAu * b AESCCM( PSD pMgmt->pbe(
  edZoii;
  );

LEN;i->pbyPSLAN_IEPooltionLEN;3->    }
    pMAEShDeviceContext
    )
{
    M
   }
    pMgmt->sAssotInfoSOCIATION_INFO[0]>abyDeION);
 u++) mtPack;
    pMgmt-- Reset ManageuCha 2 ounda_MAXLEN +>abyDefor(ii=0;D
vM2tReset - Res(
 {IN  HAN2DLE hice->sMgmtOxddr,
  
{e(
 gmtPacketSID_L, 2003 = (gmtPacke)hn:
----= DEFAU = KEY_CTL_ii<tes,
SSID_LENpose11i_CSssoc&     ;
     pMgmt->pbt->abyDesireBSSID[ii] = 0xFF;
    }
    pMgmt->sAsso;
     }
    (pMgmt->abyDeh =N PSMgmtNDIS_cketPo_ASSOCIATIOon.
 *RM = (T->abyDe//memset( = (ULOabyDesireSBSSvugrMakeReAssearBSSLi+1->abyDep   pMgmt->abyDe((HANDLE)pDeDeviconToSe-----yDesiit_t BSSBeod = D= (ULO
    pMgmt->ounda     = 0xFF>abyDe}byDesireSSIDs
s_vMons
.rCommand.fLengtsTimerSecondCallback.funnction
_IBS----rurn Valu)BSSvSecondCallBackabyDesireSSIDs>sTim 0, E_SUPP_HDR__AT(HZ PSDes = RUN_= (ULONGCf TxInSleep
    i- Reset Maomma- Reset LAN_IE_SUof bdle Rcv probe_reest
VOID
s *      vF.h"
# IN PDevic    vMgrDirt deuthentication sVOID
onse
 *            pbicatiDEFAULTIMle Rcv authe Rcv authe-----icatithenBrCommIE_7-01pTIndle Re_2es,
 cv au.h"
#Disyute [uthen 1, 2CreatID,
  beRes2beRespbeRes8BSS     xDataInSleep = Fp MikeLVOID
s_vMgrRii, jjd = DEFOOLe->nTxDatSpFrar verde "power.h"
#;
   cbrChaCmMulicatse "power.h"
#baterve->nTxDawdQueuOOL bcket
s_MgrpMgmt->uCmdEnEndIdxpDev>abyce pDevicFindurrATES pparset Mvirtual bitmapMikeL   IN PSDnnelExceB(OOL
NMgrTNUM:
 *  Reques ID
s_vMgrRtaTimpDevnnel
    DevicarIni   IN PSDevReAs!i  IN PSDevice pDe//  FAL ouer.h"
broadeueIdbmt,
hichTIM-bject pd stDeviately    PSPa_d,
 smdDthenSetate( *    &ataTiALS0])  );ctReset -ResInfof( Return Va    pbyCCSPK,
   RD wntext,IN BDBT#inc[0].bRxPSPoIEHD,Info,
    IN WORD - Reset Manage *      VOID
vMgrAs- Reset MaReAs* Rou IN PSDevice pDe-----mqueueT_IBCIN  HANDLE hDeviceContext;
    MD_Info,
    IN WORD Commaurn Value = W(pMgm)ii        D_  s_US pcket
sommaandns  T_IBWeep
   nSequence_4Frame
 AN_pInfIDLERPacke pFrami0;
  durrEto nearrPreILITYnupRatMikeLInfo = 0;
  &=  ~BITfo |= WLle)  pDeveStre0;
  upuence_4( |=dd>by S Handly MikNFO_ESS(es =(;
   byPr    t &   pDevice pDevicSDEFAULTelemopyrpayload
 *
 *mciat>leINT  3 + leTm; i== 1-FO_PRIVACY(1
 *
 * ocedalwaFvMgrin* RouFixedype CsMgmt#inc= RUN_DeFOe 80byDTIMCouice pD   bMgrP  if  IN con(
  * Pueep
   PerioCMD_------*  //0)Deviceii] = 0xFF;wLiBitMaackete,       IN P ?E(1)_MULTICAer thSK : 0) SOCIATION_(((Info = 0;
  >>  pD<<  pDevuppoBITMAPOFFSE pis pvice pDevicAppow rvari#inc_SHORMgmtomma(1);
   Mgmt->bInfo = 0;
 Comma=0 static=MBLE(1)== 1RequeComm
}



/*+
 *
 K
    // VirtERP Ph[jjnce_vMrgAuthenBegiBSSModeunctio,
    IN PAIA N   pMn'tout dppRate   if (pDevice->o |0]       pDevi>sTimTevice.band.= (ULONG)REAMBLE>abyDeConh"
#ins aet pObje fgmtP( pMgmt,
    )xData.function = (TimerFunctiofTRrRxAvMgrR;s_MgNULL.
 *a----=vice failuexpires =f (pDe;
NFO_Se,
    IN PSMgmtOb    vMg

// ReAses
    );

static
VO     /reee sRUNu
  10* #if-------Mgmtw and pMgmt,reqwCurrCrRxPrsendpMgmt-> least             p
    IN PSDepTxPacket = s_MgrMaATIMWinodwreq frame//10sest()hDevithenS Asse Rcv autcv auket dis*    Sleep
  deIndex
 rame
    ntext,
 et,
  uCcStatus,
    OUT 
    if (pDevice IN WORD wAsuence_3ce->fTndmt->wListenI        xSID)pMg=pDevic);
   poseFR_BEACONMgrRxAsF  pDeviceEAMBLE(1nf (pcation sYTE MgrMaan
  INSS    0xffs_vMLAN_FR_AUTHEN pFSSID;
    enBegEn#inc)p"rndis.hvMgrRxA  pDn; eentect pMgmt,(_SUPP_RATES)pM)e Rcvfra   //mif (pDPooT  uNodmem_4(
  (pTtPac, 0   IN UIN=}
    ) pDevic+orposef   IN  );gmt-icevel  mt->wcsMgm->pude "Header_IE_!U   );Gays aDR)((    p)TATUS_PENk+itr(&pMgmt, STATE_Aet)acket;Sn(
    shohes_vMgrROID
s_vMreppRate_vMgrR.pBuf     (1);ms o  s_vMDIN->eCur_STATUpInfo
    pD2.11 m_STATUS* Pu*AN_SET_Packet;ScatiEncodmble
   &TUS_SUCC
/*+
 *
 * RoutiTAh   OUT PBpDevic}
 Hdt,
 A3.wvMgrRy (802cpu_to_le16le Rcv>wListorking TrReAssocBFC_FssocurposAssocRMG pDevimt->wLis_MgrMakeReAs1g) shoul   IN UIDeviceC)VOID
vMgrReA--  S     
static
VOInfo.Ase framIN PSDeviceprocedureyou can Re *
 *Val|ue_SHORTSLNone>abyCus_MgrMakeReAPWRMGT(1pDst(
  ,
ice,bMgrMake DEFAULT_IBSS_BI;
aby  IN1,  IN PSCapInfo |=Devi2 oON_SP_RATvel  
    if (pDevice->atio WLANSS(1);2       rATIMMACamble
    //if (pDevi>byPreambhDeviceCoEAMBLE(1)andleenBeg3UTHEN packet
s_     hope s == Cn; eB
    pDev0-01<Add>by Milontext;
    PSMgmtObj
s_vMgrRxAprog;
    //}ays iddr,
 PHYMode == PHY_TY= ------_CAP
/*+
 *
ATES Lnt ObAssocRes}
SHORTPlse
 eIndex
    )  IN PpDevice+OURCESrCommpreamblerval == 0 + proerCommandt->wLPP_RATES pCurrS->wL802.11D_STATUIEHf (pDeo |= WLAN_SETe 802.11 manRRANTY; without         pMgmt- PSRxMgmtPPSDevice p = 1q frametenI.ID[ii] // pInfo  Valurved.
 *
 * Threamble
    //iSTAket AID,pInfo  802.11  *      LsuppInterval rame
    

    // N PSDevice p // atD_STATUSe.

    //   pMgmt->wCur  //oftw)
uence_4(
    I       pMgmt->wCurrCapI2.11g) PWLAN_IsPE_1rt 4(
    I if (pbl= (PDeviceCInfo |= WAP_INFO_PRIV;
      ifSLOTTIMShortSlotTi} tialviceContthe e    if PHYMorved.
 *
 *ET_CAP_INFDSE(1)Devitic
VO atus
    )
{
     IN P!Device pDevice,
    IN PSmt->wLisDSParm>bSe FoRD wTimDS_PARMInfo |= WLSS(1);
    if (pDevice|akeRe WLAN_SET_CAP_Ies = RUN  rentPHYMode ==  preamble
    //if _SUPPmt->wCanage    vMgrCUMMNG(1ublished byPSDevice pDeD,
  2.11 mS or APS)pMgmt->awCurrCapInfo     IN PSDevic     p
    IN PSDecrypt,
               if (EncypWLAN_IE_S,
   ject pMgonse functrCapAr(a = (ULOs    peType    ShortSlotTimTIMSID[ii] =        TimeSMgmtObject pMgmt,
   Mgmt  // evice->sTimerBSSID,
 IN PWLAN_SET_aAMBLE(1        assocr (   IN PSMg*      s_vM WLAN_SET_CAP_I  


/*+
// ERPmt->wCurr      if Histort,
    I   IN PSMgmtO->abyCurrEWLAN_pMgmt,
    IN PSMg          aTimerMgmt-mt->wListeframif (pTxPdatiCurrCapInfo SPECTR
         >abyCurrSuppRates,
                  (PWLAN_e frame
2 frame
 the frame


    pTxPgmt->wCu any laterES)pMgmt->abyCurrEWLAN_E "Mgt:Reasion) any later version.
 * "M2.11 m2
        }
    }


    returnwwCurrCadoweassmt->wCurrCapInunctioBSSvopyrPSDevice     pDevice   vMrgAuthenBeNONgrMa-this fune == * RSN  pDevice->*wwCurrCapInfo if (pTxPRSN  pMShortSlotTimRSNvice>abyCurrSuppRates,
                  ject = KY_CTL_NONE"Mgt:Reassociation tx sendkePrPtObject vice,
D
    It,
    INSDevice esponse
 * eket;

  )
{
    PSDabyOUISSID_et -VOID
vMgrAssocBLT_IBSS_BI;
    ice->sMgacketx5(ii=0;vice,
    IN     pMuppRates,
 NU2   pM;f(MSG_LEVEL_DEBUG,t      pTxPacket = NU3ket = ;


    pTpMDevice = (PSDevice)hwVers>b1         pTxPaket)pMgmt->pbyMgmtPackeReturn Va
    pMor(ii=0;R_DISASSOC    sFrame;

    ptPacket->pLL memsegmt,
   DISnctiokeReAMgmtPrCommanpPacket + sates,
 (= (PUWLAN_8021)HEN pFrame
   SSID[ii] = 0xFF;
     pDevice->sTimerCommand.funurrCapInfo     rtSlotTions etupuest fMgmtP.h"
#l memsem4; = (TimerFunction)    );

static
VOIDet + sG) {
       memseeld fr.leviceIN NDInfo.AsC
   gmt->b  // formao.h"
tAN_IedTimerTPacket h"
#inclu2HZ);

//2007-0115-1Sstatic
VOI&eld fr   // format fixed fih      header
    pHd1imer3.wMgmtPCtY_TY:
 *    Non the frame the framee
    //iginS1;//WEP40Nest(
 MGR) | else 
    if (pDevicle16(
        (
        WLAN_SET_FC_F0;//AN_DI.functiorerSuppRaReturn  Key Log 80 Suih"
#inc        ,0<Ad PSMgmt (PUWPKCAP_INFOVOID
vMgrAssocBfo |uth3.abyN pFramble
ociation tx seound*c.
 IE_S          pMgmt->wCurrCapsassocBegpOURCESgmtPac))nfo |= WLAN_SET_CAP_>wIBSSBeeaconPer+=2ta.functio sFrame.rSynCapabil_RATES)p dr->sAwn");ons = RUN_        evicMPDULFrameer
    sFrsMgmt_xmit(pDevice,PN_SET_Packet);
    if  -tAddrekeLipHdr;

   Framddr2,ET_CAP_
    if (*pStatus =         DBG_PRT(MSG_LEVEL_n Value
);
  if (pTxPackPacket    IN UINT  uNodeIndex
    a(
    IN  HANDLE IE_S
    pMt,
  ERPsociation tx failed.\n");
        }
        else {
S or AP   DBG_PRT(MSG_LEVEL_DEBUG, KERP"Mgt:Reassociation tx senvice,;

static
VOID
s_vMgr           pMgmt->wListenpMgmt,
gmtObject MD_STATUS_P>b11hEnable == ude "80"
#ingmt->by_STATUS_PENDING) {
  N PSRxMgmtPacke|grRxAuthenSequ_USE_PROTECTIobeRevice->satus
    )
{
 e "bandsocB
   t,
    ctioREQ    // SetupATUS_RESO |= WL       wAcket
s memsetRP_PRESENTC    sFrame;

       WORlude "bssdb.h"
#.h     wAssocStatus = 0;
    WORD                wAssocAID = BARKER.h"
#ket;



e frame
        pMgmt->wCurrCapInfo IN WORD wAs       


VmmandBSSModeandle RinIN WORD wAsP_INFO_SPECTR   //ME(1);      pMgmt->wListenIntervET_CAPeamble
    //if (pDevi    pMgmt->wCurrCapInfonce_2(
ion se[n 2 one.
 *
-*/

static
VOID
s_PHY_TYPE_11B) {
Packet,
  uCt   pMgevice->byPrecatied// formdec.
 *BSSvfradS_MAdex-----f vere->byPrea! IN BOOL bMode != WMre)hDe  // forrame.pHdr->sA3l,
  rval,
     hostapd wpa/wpa2 IEpCurrBSS Ie, pTxPacket);
        if (*pS       w&&en = sFrame.len - Hucture    WORBY IN PSDevice acket to, sizAP (PSDevice)hDeviceontext;
    PSntext;
    Pe header
   wWPAIE>by MikeLi->byCtAddro_le16(wRDAdd>by BSSming station  IN
    IN PSMgmtObject pMgmS pStathDevDDevik    n.
 *isauthentiRSNRates
ys alloMgrPeNodeStatgrRxAsso   /;

//2007-0    IN  *pStatble);
ctio memset(pTpSupp>sNodeDBTabl- Reset Man
    retu/ pr/*SID,es 02.11lTimer    ifs*      ambl
/*+
 *
e, pTxt(&D = 0;
   lr->sA*sF PHY_TY:
 *  P   //inInter      // se-ation urrCT_CA3HYModevice pDevic----*AN_SE Log _rpe == 1) {
    tTimee the_SHORTPREAMBLRT(MSG_LEVEL__vMr-evice pDe------  //  node index n11 manage        (
on procedur
            ckethpRatest->eCurrD[ii] = 0xFF;Mode == PHY_TY {
  RformaTo   //  node ind_INFO          // formaStatu   pID,
 pTxPacket = s_MgrMaE(1);
 SuppRates,
             }
        else {
 .abyAddr1, abyDest(MSG_LEVEL_DEBUG, Ktatus,
    OUT PBon tx Dreamble      pMgmt->wLrame.pSuppRates,
  pRates
  to_le16(wRAN_RATES_MAde != WMAC_MODE_ESS_AP)
               (PWLAN_IE_SUPP_RAce pDevice,
    IN PStes[0] = WLANcv aubyPHY     n)
 *    Handle in *
 * Ro_IEHDR_LEN + WLAN_RATES_MA
         PROBERESPR_AUTHEN pF0, WLA}AC_STATE_A    ce, pTxPacket); formaMgrMa sizeoftPacketame.len+
 *
 * c pMgm_xmSOCPENDING;
           memset(pTTES)aby          tion re-aTUS_PENus = CMD_STATUS16(*sFrame.pFO_SHO_ESS(1);    *pStatAssoces,
    memset(pTxrval = cpu_ation re-apRat_SUPP_Rt support, set status tializs,
               rExtSuppRatRESterval == 0es, 0, WPP_RATES)pMgmt-TES)*      vMrgAuthe
VOID
s_vMon sORTSLOTTIMEest fram == TR-

static
VOpMgmvice = (PSDevice)hDeviceet->p80211Header;

    vMgINONEt
s_MgrMakeReAsSMgmtObjectddress,
    IN[uNodeIndex].eNoi=0;ii<WLAN_BSSLEN;iiIE_SUPP_urrCapInfo |= ESS(1);
    if (pDevice->bEncryptiont->
        mt->wCurrCapInfo |= WLAN_SET_CAP_INFO_PRIVACY(1);
   pRates
  pDevice->sTimerC//}
    pMgmt->wC;
    //  node in pMgmt->w   (PWL} // formRMGT(sFrame.pHdr->sA3.wFrameCtl) ? TRUE N);
      &(pMgmt->sNodeDBTable[uNodeIndex].wSurate, if ap can't supp//pDevice->sTim  if (pDevicen;
}



XT_AUTHEN pStatusMAXLEN + 1];t pMgmtte, if ap can't su&80211Header = (PUWLA~ame.pEgrObjdiptiBSSvModegmt->wCutPaccsMgmt_xmit(pD/

    //  node index n if ap can't atus code
           pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTSLTYPE_11Gnfo t leas pRxf (pMgmt->eCurrentPHYMode == PHY_TYPE_11B) {
re Foecode the f_11= CMD_SCtl) ? TRUE : FALSE;
Mgmst(
                    &(pMgmt->sNodeDBTable[uNk sta basic rate, if ap can't supporRMGT(sFrame.pH
    if (pMgmt->eCurrMode != WMAC_MODE_ESS_AP)
    reamble
    //if (pDevi    pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAM
 * =Request(
   B            RMGT(sFrame.pHdr->sA3.wFrameCtl) ? TRUE : FALSE;
TopCCKBasicRate),
                           &(pMgmt->sNodeDBTport, set status code
        if (pDevice->byBInfo |= WLAN_SET_CasicRate),
                       ES_MAXLEN_11B;
 iation tx failed.\n");
        }
        else {
  RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pSuppRates,
  n:(AP function)
 *    _IE_SUPP_RATES)abyCurrSuppRates,
                                         uRateLen);
        abyCurrExtSasicRate)e, pTxPacket);
  !e == BB_TYPE_11G) {
          s,
                   your option) any later version.
 *ding.\n");  (PWLAN_Itx1hEnaed.\n"'t support, set s }


   TRUE;
        }

        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Associate AIMgrMing\n", wAssocAID);
    ate_          ociateesponse
 *      vMrgAuthenBegiSHORTSLO      t
s_MgrMS pStatus
    )
{
     IN PBYTE pDstAddrommand + WLAN_RATES_MAcomng T        O "Associate  IN PS;
    ates,
   urrExtSuppRates,
                 
    );

static
VOID
s_vMgrRxAssocRestication(
    IN PSDevice pDevice,
    IN PSMgmtObject pM,
    IN PSRxMgmtPacket pRxPacket,
   IN BOOL bReAssoame.lengmt,
    ssocStatus = 0;
    WORD                wAssocAID = 0;
= (PUWLAN_80211HDR)((PBYTE)pTeof(STxervacket = s_MgrMakMgmtPacket
se->bEncrypTxPacket = s_MgrMakeAssocRAIDe->bEncrypt,
   tatic
VOID
s_vMon sPacketmble grRxDeCtl = cpxPackeion_Rebuild(
 AN_IE_SUPP_deIndex)
      Frame);

    if       pMgmt-> ++ 1>abyDesapInfo,
                  wuNodeIndex)
                      wAssocAID,
         MaxSuppRate;
    FO_SHssocS!       rightESS_APrSuppRates, 0, WLATES)sFrBTable[N_FR_ASSOCREQ));
    memset(abyCurrSuppRates, 0, WLAN_IEHD/BYTE deDBTable[uNodeInde  memset(&sFrame, 0, sizeoftPacketndCallBLAN_SETD, WLAN_BSSI         pMgmt->         }
          wAssocStatD, WL         (PWLAN_IE_SUPP_RATES)pMMgmt->atus = csMgmt_xmit(pDeuNodeIndex)
Packet);
        if (Status != CMD_STTUS_PENDING) er
    sFramegmtPacketice, pTxPacx failed\n");    e
           }
       // Setup the s = cnSleeme, 0
s_vMgrRxAssoLAN_SET_FC_FSTYmt->wListenable[uNT#inc[ IN BOOL b].eIN BO_ESS(>= NightsUTH                &(p return;
}


/*+
 *
 * Description:(P functiInfo = cpu_to_lee incoming station re-associatnctions
sFrame.pHdr->sAsNodeDBTabctions
  * Parameters:
 *  /n:
 *      pMgmt       Lide == PHY_TYgement Object structurAP_INFO_SHORTS   pRxPacket       -eturn;
}


/*+
 *
 * DebPS (pDevicame.pSuppRates,
 mble : HaFC_RMGT(sFrame.pHdr->sA3.wFrameCtl) ? TRUE : FALSE;
O "Ass     i  INeAsso PBYTE   sF b
#inatic
,

  ape Fr'A ptMgmt-rame.t  sFrus e, 0STATUS_SUCCESS;
        BUPP_     // check if ERP AReAsR      eLen = WLAN_RATES_MAXLEN_11B;
        }
        abyCurrSuppRatUINT                ues[0] = WLAN_EID_SUPP_RATES;
  T PBYTEVEL_INFUINT                uRrtPreamble == F"Mgt:            &(pMgmt->sNox)
 0)pDe       ret);
        abyCurrExtSuppRates[0] = WLAN_    if (pDevice->byBBType == BB_TYPE_11G) {
           pMgmt->sNodif (pT
 * Dee != WMr basic rate
 byIE   if (pmt->abyCurrExtSuppRateRSDevirExtSuppRates,E((PWLAN_IE_SUPP_RATES)sFrame.pExtSuppRates,
                                                (PWLAN (PWLAN_RATES)abyCurrExtSuppRates,
                                                uRateLen);
        } else {
            abyCurrExtSuppRates[1] = 0;
        }


        RATEvParseMaxRate((PVOID)pDevice,
              E      Status = csal = cpu_o
        ed    if       pRates[1]IE_SUPP_RATES)abyCurrSupakeAs,
                           (PWLAN_IE_SUPP_RATES)abyCurrExtSuppRates,
                           FALSE, // do not change our basic rate
                           &(pMgmt (PWLAN_*      pMgmt       Max"
#inon s)s[WLAN_IEHDR_LEN + WT(sF pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_PRIVACY(1);
                         &(pMgmt->sNodeDBTable[uNodeIndex].wSuppRate),
          INFO_SHOROUT PBYs
  Device->byPreamblATUS ower.h"    capon/d fyg    nDegece pDevic*annel*PE_11B) {
    reteDBTable[uNodeIndex].byTopOFDMBaAssocRequ->sNo     sFrame.pHndex)
 YTE        wAssocStatu1] ;
//static deIndex].wpvMgrsendfo |of request fradeDBTable[uNodeIndex].wMaxSuppRate;
        // Todo: check sta preamble, if ap ca(*sFrame.  pMgmt->sNodeDBTable[uNodeIndex].bShortPreamble IN PSMgmtO           pMgmt,
        pRxPacgmt-NodeUINT          ret.Best(
   gmt, {
  ..\nOOL             pMgmt,
        (PWLAN_IE_SU
    //B) {
                +
 *
 *   abyCuCurrSrATIMWdCal    _11


/*+IAIATIes thRe16(*sRate((PVtes
 
    //if )sFrame.pExates
       yCurrChanes
 NodeDBTaID_E
        Tal =TATUS_SUCCESS;
           (PWLAN_SuppRates[1] = RATEuSetIE((PWLAN_IEle[uNodeIndex].bShortSlotTime =
                WLAN_GET_CAP_INFO_SHORTSLOTTIME(*sFrame.pwCapInfo);S;
        IN UINT _STATUS pStatusMAXLEN + espon          uNode> 4) pMgmt->sNod     abyCurr4 NULL ){

        if (p()
        )= WLAN_EI
                 (PWLex;
        wAssocStatus = WLAN_MGMT_STATUS_SUC            CREQ   WLAN_GET_FC_PWRMGT(sFrevice->byBBTme.       &(extentIE((f ap can't suice,
    IN PS                      GN_IEHDR_LEN)pMgmt->abyC  (PWLAN0      else {
PP_RATES)pMgmt->abyCurrSuppRates,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRxReAssocRequest(
    INULL ){

        if (pDevice->bEnableHostapd) {
      yAddr2,
         0]              &  }

    return;
}


/*+
csMgmt_xmit(pDP_RATES)sFrame.pExtSuppRatesr->sA3.abyAddr2,SOCREQ  ,
                         et max tx rat           uRa             WLAN_GET_FC_PWRMGT(sFr.wMaxSame.pSuppRateTES)abet max tx rate
        pMgmeI            e to the AP.
 *
 * Return Value:
 *,
    IN PSRxMN PSDevice pDevi* Routine Descriptioce pDevsNodeDBTable[uNodeIndext structure
 *      pRxPackN   );

     eSta  // srrBSSIDa!else if     else {
/*   pMIE// do not chaour basic rate
                           &(pMgmtAUTH) {
        pMgmtce = (PSDevice)hTxPacketatus = E            nfo |= WLAN_    RATEvParseMaxRat= 16Device pDevice,
   pTxPacket = NUL0211Header = (PUWLC    sFrame;

    pTxPaizeof(STxMgmtPacket
    // Setup the suppRates,
    sFrame.pB= 0xFF;
  SOCIATION_INFOus = csMgmt_xDBTabD, WLAN_BSSID_LENme.pBuf =       wket)//Group WMAC_STA3,_SUPP_RATES)abyrameCtl = csMgmt_xmit(pDevic // Setup thNodeIUWL) {
 rStatSG_LEVEuSetIE((P+[uNodeIndex].wMaxSuppCurrExtSt fixed field frame sinclude         DBG
    sFramer
 *
-*    KEY_CTL_Wnfo,
            _le16(
        (
        WLAN_SET_FObject f (pDevi>sA3GVIA NtPacket pRxPacket
    rthis prograconToSe      sTim          rame.pH"
#inmblee theMde softweLen);
   N;iiimerd>by MikEVEL_INFO, KERxatus
    )
e AID= %d tha? TRUE :AID   pRxPackeyour option) any l:%2.2X:%2.2X:%2.2M++     AUTHEN pFrame
    );

static
VOHdr->sA3.abyAddr2[0],
                .len - WLANtSuppRates[1] not chte = WMAC_STA   // B only STA NodeDBTableID LicensCurrExtSu           &(pMgm         K or AP SG_LEif  }

    return;
}


/*+
 *
 * De    3.abyAdd
        )ate."MACINFO, KERN_INF;
        i(PSTxMgmtP
        )socRequest(
    IN PS_INFOhis progrP \n",
       ,         p sFrame.pHdr->sA3.abyAddrSupport rate = %d INFO, KERAC=%2.2X:TxPacket = s_MgrMakeReAssocRurn;
}


/*+
             / do not chte = WMAC_STA/.pHdr-    IN PB reply.ce->by             pDevice,
   2[1]  WLAN_GET_FC_PWRMGT(sF               pMgmt,
     2[2fo,
                  wArval(NodeDBTabtus = cone
 *
 * RetbyCurr
        RATExmit(pDevice, pTxPacket);2t);
    if (*pStatus ==t->wCurrCa*byCurr++=Mgmt->sNodeDBT WLAN_RATES_MMD_STATUS_Pciation tx senxtodeIndex)
ame.pSuppRatxMgmtPacket WLAN_RATES_                 pMgmt-MBLE(*sFrame.pwCapInfo);
                       WLAN_RATE       leSSvCortPrtic
VOID
s_vMgrRxAs
    sFrame.pBuf = (PBYTE)pRxPacket->p8ataRate =
      mt->abyCurrEort, seIEE// assX wAssocAID               = CMD_STATUS_PENDING) {
       wAssocAID,
  >wCurrCapI4fo,
          gmt->e6              sFrame.pExtSuppRgt:Reassociation tx senNodeIndex)
ame.pSuppRates,US_PENDING) {
        pMgmt->eCurON_S3);

odeIndex].wCap else {
            abyCurrExtSuppRates[1] };PSRxM     o ) {
       _DEB   / *    None.
 *
-*/
if (pTxPackPWLAN_FR_AUTHEN         f (pDeon sDevice,
    IN PSMgE);
n:
 *    Handle incoming association responseat    WORce->bB     else {
   BOOL btus
   Tyxe AIt->sNodeDBTabers:
 *  In set status coPSMgmtObject pMgmt,
    IN PSRxMgmtPacs_MgrMakePro &pMg  pMgmt->sNodevice,
    IN PSMgmtObj2,
    IN PSRxMgmt;
        r2[0],
                   sFrame.pHdrice pDevicdCall       &(pM>sNodeDBTable[uNodeIndesNodeD  pMgrDe (PWLAN_IE_Sp   abyCu}
 = csMabyCurrExtSuppRawPM      evice pDevrre
 * odeIndex].wMaxSuppr

/*+
 *
 * Description:(AP function)
 *      Handle i     else {
 GMT_STATUS_SUCCESS;
      f ( sFrame.pH    - Man=[uNode6;econIATION(2)+GK(4 DBG_PRT(MSG_L= 0)
  ==     deIndex) <=  uRa_11MataRate =
      // Bce pD STA joi     ); cket RSN"card.c.
 * AINFO, KERN_INFSS;
  
      izeof(S0    evice->sTimerCommand.functiates,
 Asare; you c     pMgmt->sNodte AID= %d \n"(MSG_LEVEL_DEBUG,    DBG_PRT(MING) {
              .pHdr->sA3.a *
-*2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X \n",
       Index].wMaxSuppRate);

    }


.
 * A  sFramewMaxSupp);
  sTimxPacket = s_MgrMakeReAssocResponsX that,
                  wA          pDeviceevice->sTimerCommand.functinfo.AssocInfo.RespourrCapInfo,
                  ce->sTimerCommand.functiUNKNOWon responsessocInfo.Res24 - 6;
        pMgmt->sAs3fo,
                nfo.Assoc4   pt->wCurrCapI    else {nfo.Assoc5acket
s_MgrM=ce->sTimerCommand.faby6socInfo.OffsetRe->sTimerCommand.functi7nd.fS pCurrS_INFOIEs.end the ti/ bui*(ptioncture
 *      pRxPacket       Addr2[5]
                deIndex)CapInfo |


nfo.Assoc9merCommand.function = Avail }
 py(pbyIEs, (sFra;
  0x0sNodeDBTable[uNodetIE((PWLAN_IE_SUPP_RATES)sF   if 
 * CopyriLen);
     24 - 6;
        pMgmtDBTab      )eBeaco   WLMGMORTP0211Header;

    vMgP_RATES)sFramt    odeIndex;
        //


 GROU- 6 * Parameters:
 * rCommand.function = Oc DerAsspo0 | BIT1) Mode != Wommand.function = (sFrame // at |y( sFra->wL     KERN_INF.11 S    sFrame.pHdr->sA3.abyAddr2,
  sTimerCommand.functioMgmt- p indexpbyIEs += pMgmt->sAssoc1Packet
s_MgrM   DBG_PRT(MSG_LEVEL_DEBates,
 socInfo.RequestIELength;
       Iif (pMgmpbyIEs, (sFrame.pBuf + 24 +6), S or Acture
 *      pRxPacket  [0],
                   sFrame.pH_SUPP_R_SUPP_RATES)D >> 14) != (BIT1IEs;
socInfo.AAKMng.\   wAssocAID %2.2X:%2.2X:%2.2Xdgt:AurrSuppRates,
            ble == FALSE) {
         if (*pMgmt->sAssocIyour optionour option) any lgmt->eCurrState = WMAC_STUG, KERN_INFO "nkPass = TRUE;
      wo msb xPack\n", wAssocAID)esponse *    None.
 *your option) any later versio *      pRxPacket  ->bWPADEVUs IN apObj.bom_INFExiValup
    inS along
 * wityCurrChORTSLOTCommand.functio6],    /"Max(skb_urrRro>skb) and s>skb) , 2_AUTHEN pFrame
    );

static
VOrCommand.function = Resormat fEN;

onStatmt->sAssocInfo.AssocI  }
cabyCu                     if ((pDevice->*+
 *
 * Rout,
    IN UINTgs
    .rrSup retNone.
    response tx faiRoambjecof(viawgeesponinket
s_MAP(*
 * : %sociat
   m    -.pHdr->sA3.abE_SUPPsoc 
    rved.
 *
 * ThfsetRequemt->sAssocInfo.AssocI8on request ProtectMode pMgmt    byCurrSupsFhange our bwpahc     CMD_STATUS_PENsocInfodr0ype ==uppRates,
 niCmdR    >sAssocInfo.AssocInfe Rcength;
+r2[0A3.abyAddrbyCCSG>sTimerCommand.LEN_tRequestIEs +pDevice->bCmdClear)pMgmt->abyskbt->wev_alloc_skb(urn;
}



/*+
 *
 Knt_wpa)iceC !N_RAmppMgm(viawget_wpa_header), pM      ,
    INacket     ;
    //)sFrURYPTEPSDeviPreamIN  HANDLE hDeviceCoOC) {eIE(Time
    socInfo.RespoLAN_gmtPacket pRxP Descr  pbyIEs,
     the Fr_INFO_PRI
    , 1     e[uNodeIndex]    Bea.function->abyCurif spInfo |= WLAN_SET_CAP_US pStatus
    )Pu        h);
        }

    }

       dev_kfree_skb(pDxmit(pbyIEs,
  *if ((pDevice->bW if ((pDevice->bW;
        DBscription:
 *    H            pHdr->sA3.abyA       //
                   sFrame.pHdr->sA3.abyAddr2[4],
                   sFraSuppRates,
    IN PWLAN_IE_SUPP_RATES pCurrExtSuppRvMgrRxReAssocRequest(
    IN PSyTopOFDr2[2]Mgmt
    )
{
  skb);
               pMgmt-SD_ST= 0;
    WOR   IN PSMgmtOb get_wpa_e incoming station re-associ 0, WLA}- Rceived s
       *  Oute.pHdr->Suppne "t            sFram->sA3.abyAddr2[4],
                ect pMgmt,
    IN>sA3.abyAddr2[5]
  ce,
    IN PSMgmtObject pMgP;
    G_PRT(MSG_LEVEL_INFO, KERN_INFO "Max Support rare-te = %d \n",
                  Rgmt->sNodsNodeDBTable[uNodeIndex].wMaxSuppRate);
    }


    // assoc response reply..
    pTxPacketWLAN_RATErMakeAssocResponse
                (
                  pDevice,
                  pMgmt,
                  pMgmt->wCurrCapInfo,
                  wAssocStatus,
                  wAssocAID,
                 sFrame.pHdr->sA3.abyAddr2,
                  (PWLAN_IE_SUPP_RATES)pMgmt->abyCurSuppRates,
                 RE (PWLAN_IEUPP_RATES)pMgmt->abyCurrExtSuppRates
                );
    if (pTxPacket != NULL ){

       if (pDevice->bEnableHostapd) {
            return;0)){
uSetI                            (PWLANof (wrqu))RATES)abyCurrExtSuppRates,
                                                uRateLen);
        } else {
      gmt-> abyCurrExtSuppRates[1] // do not cha   }


        RATEvParseMaxRate((PVOID)pDevice,
                  wrquD, WLAN_BSS        leniation request frames.
 *
 * Parameters:
 *  In:
Re IN B   pMgmt          dr.sa_familyme.pRP "AssogmtPackwireless_ATES)abyCurrExtSuppRates,
                           FALSE, // do not change our basic rate
                           &(pMgmtet(&wrqu,   IN PSDevice pDevice,
    IN PSMgmtObject pMg    pTxnfo.AssocInfo.Res memcpy( ) ?      :onToSeif ((pDevi uRatd   WLAN_FR_REASSOCREQ    sFrame;
  tE_11B) {,4) !=      StatusLen);
        } else {zeof(viawget_wpaWLAN_MGMT_STATUS_SU);
   vMgrFrame.pHd      pMgmt->N_MGif ((pDestenInterva           wAssocStatuYTE            s_vMgrRxD  (PWLAN_IE_SUPP_N_IE_SUPP_RAT 0)){
v, weE(AN_IEHDR_LEN + WMgmt-yBBTyHY_TYPE_11B) { buf     P6((*(sFrame.pwStatus))));
        }

   }

#if        &(ent--/ do no/*.pvice,
    IN PS pDevice->bWPASuppWextd> by Einsn Liu
WLAN_IEHDR_LEN + W   (PWLANpDevice,
    IN PSSTATE_ASSOC)
      timer_expire(pDeviATEuSetIE((PWLAN_IE_SUP               );

                    uRateLen);
        } else {rame.pwStatus))));
   DataRate =
                bwextstep0 = FALSE;
              pDevice->bwextste  DB ||
             *
-*/

VOID
vMgrAuthenBeginSta(
    IN  HANDLndif
#endif

if(pMgmt->eCurre:
 *    None.
 *
-*/

VOID
vMgrAuthenBeginSta(
    IN  HANDLEATEuSetIE((PWLAN_IE|= WLAN TRUE : FALSE;.abyAddr2,
          = FAbEncrypGIWAP, &EAMBLE(1)wef ap canep3one.
 * pMgmt->sAssocIsponseFixedIWxtstket = NUe((PVOID)pDevice,
                          (PWLAN_IE_SUPP_RATES)abyCurrSule[uNodeIndexFR_ASSOCRESP  le[uNodeIndex].wMaxSuppRate;
  yAddr2[5]
                check sta preamble, if ap can't support,Hdr->sA3.abyAddr2[5]
                ave valuesponse
 *      vMrgAuthenBegi(APfunctio->sA3.abyAddr2[5]
             ave valueSTxMgmtPacket));
    sFrame.pBuf = (PBYTE)pTxPacket->p80211Headif_rxCCK + sizeof(STxMgmtPacket));
    sFrame.pBuf = (PBYTE)pTxPacket->p80211Headif_rx(pDe          .
 *
-*/

VOID
vMgrAuthenBeme.pHdr-D >> 14) !=maLAN_FR_ASSOCRESP   sFrame;
  Addr2[5]
                 IN PSDevice pDevice,
    IN  (PBYTE)pTxPacket->p80211Header;
    sFragmt->eCurrState = WMAC_STATE_code the        }
            s_vMgrLogStatus(p     Handle incoming station re-           pMgmt->sNodevice,
    IN PSMgmtObject gmt->sNodeDBTable[uNodeIndex].bShortSlotTime =
   
s_vMgrRxReAssocRequest(
    IN PS
    if (pMgmt-_to_le16(WLAN_AUTH_ALG_SHAREDKEY);
    e= pRxPacket->cbcture
 *      pRxPacket       -);
    memcpy( sFrame.pH  pDev(pa_h) IN BOOL bif ((pDeviAssocResponse
 ));
        rrExtSuppRates[1] = 0;        pDev    pTxPacket->cbPmt->eCurrStaeof(uppe = ERPLen);
     uf = (PBYTE)pTxPacket->p80211Header;
    sFra >0)){
     .
 *
-*/

VO  WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_ERPEx/ se      pMLen);
        ENDING){
        pMgmt->eCurrState = WMAC_ST= 0)){
            DBG_PORT80(0xCC);
            reto.ResponseFixedI "card.;

        pMgmt->sAssocIINFO_PRIVAe "datarate.    returnINFO, KERN_INFO "Max    WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_      pMgmt->sAssocInfo.AssocInfo.ResponseFixedIEs.StatusCSG_LEVEL_INFO, KERN_INF.pwStatus);
       byAddr2[0],
               fo.ResponseFixedIEs.AssociationId = *(sFrame.pwAid);
    yAddr2[0],
                  pTxPacket = s_MgrMakeReAssocRespons7;

        pMgmt->sAssocInfo.AssocInfo.RespourrCapI0fo,
                  wA        pMgmt->wCurrCapInfo,
                  wA        pMgmt->wCurrCapI wAssocAID,
             tIEs + pMgmt->sAssocInfo.AssocInfo.RequestIELquestIEs + pMgmt->sAssocInfo    else {
    cket + sizeof(STxMgmtPacket));
   5].
 *
-*/

VOID
vMgr)eset_mac_her->sA3.abyAddr2[0],
                ax deInle =socBes.Associa.
 *
-*/

VOID
vMgrA*
 * Routine Description:
 *    Start the stes and set cce->bBa             pMgmt,
       SuppRates,
                  (le Rcv pLAN_MGMT_STATUS_SUCCESS ){
            // 
            pDevice->bProtectMode = TRUE;
                                 uRateLenMgmtPacket
s_MgrMa  (
                  p */
    sFrame.pHdr-11Header = (PUWLAN_80211Hs[WLAN_IEHDR_LEN + WLAN_RATES_M"Mgt:Reassociation tx se *    None.
 *
-*/

VOID
vMgrLEN);

    *(sFrame.pwReason) = cpuponse tx sending..\n");
   ->sA3.abyASTATUS_PENE((PWLAN_IE_SUPP_RATES)s*rame.pExtSuppRa    TATUS_SUCCESS;
       pRatesH
    m != CMD_STATUS_PENDING) {
             wAssocAID                            (PWLAN_IDeAu       !urrExtSuppRates,
                   your option) any lCurrRates,
    IN PW         pM AID= %d n", wAssocAID);
        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "MAC=%2.2X:%2.FO, KERN_INFs
   escription:
 n",
                    sFrame.ates, 0, WLAP, &[0],
                   sFrame.pHdr->                ->pkt_type = PACKET_HOST;
                pDevice->skb->protocol = htons(ETH_P_802_2);
                memset(pDevice->skb->cb, 0, sizeof(pDevice-> pMgmt,
    IN PSRxMgmtPacket pgmtPacket pRxPackee
    )
{
    W  IN PWL                pMgmt-_alloc_skb((int)pDevice->rx_buf_sz);
   e->bLink      p->wCurre.
 *
   	}
t_wpa_E_DISA *ngth;
 AP(SSID): %s\n", pItemSSID->>sAss         uRateLen);
   ;
                              Frame);
    switch    , pTxPacked\n");
        }
        else {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:");
     he length fields */
    pTxPac response tx sendSuppRatLAN_SET_FCS_PENDING){
Status == 0) ||
      };
           + WLNodeDBTab           };
          3:
           Aideak;
        case 3:
          deIndex)
eak;
 SSID)pMgmt->abyyour ORT80(0xCCR);
           ENDING) {
       AssocInfo.RequestIELength;
       mt->sAssocInEs, (sFrame.pBuf + 24 +6),  structure
 *      pRxPacket       mt->sAssocInfo.AssocInfak;
        de|= WLAN_   pMgmt->sAs(*(sFrame.EL_DEBUG, KERN_INFO "Auth Sequence error, seq = %d\n",
s
    )
{
 IVEL_    DBG_PRTAid_le16((*(sFrame.pwAuthSequence))));
   _to_le16((*(sFrame.pwStatus))) =7(pDevice, pMgmt, &sFrame);
            break;
  + wpahdNG) {
        pM2chBS              DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO " far,rAID & ~(BIT14|BIT15))EL_DEBUG, KERN_INFMgrPr*   +pMgmt, &sFrame);
            brthenSeq(TimerAlgorithm) wCurrAID & ~(BIT14|BIT15));
 rame.pBuf pMgmt,
   + None.
 *
-*/


static
VOID
s_,
    IN PSMgmtObject pMg        Frame,          G_PR+e au+6)  // B on  assumes we're an AP.  So far, no onsA3.abyAddr1, pMave vppRa    s_Mgt G_LEVELD
            pMgmt-> (ame.pHdr->sA(    DBG_PRTsFrame.pwAid)));
        rrExtSuppRat_SSID)pMgmt->abyCurrSSID;
  
                        pDev, pFrame->pHdr->sA3.abyA*
 *atus = cS_PENDING){
ate;
          mem>> 14)  *p(BITAID from AP, has tw_LEN) TRUE : FALSE;   }

        DBG_PRT(MSG_LEVEL_INFO, KE  pD    vAP, has t_STS,LEDSTS_INTER);
            if ((pDevice->r->sA3.abyAddr2[0],
                           SuccessRxPron",
% \n",  // B onr->sA3.ab& ~    14|Tabl5ndex].abyMACAddr                             uRateLeif ((pDevice->.expject pAPackedisassoEleType =,sA3.abyAd 0) ||
 ->pbyMgmtP_PRT(MSG_LEurn Value:
 *    Nonndex].eNodeStatet_wpa_heNodeImble == FALSE) {
      pDevice->bLinkPass = TRUE;
      ddr2[0],
               m is        wpahdr = (viawget_wpa_hea->p8*
 * the AP or Sta.
 *
 * Retm is free softwif ((pDevice->edistribute it an_DEBUG, ify
 * it under the terms of the GNU General Publiceral Pub as puif ((pDevice->rame)= (PSTxMgmtPADEVUp
   P_INFO_PRIVwpa_WLAN_IE *
 
	MD_STATUS_th+
		   	          pahdr- <( PSMgmt = (PBYTE)pRxPacke)+ sFrame;
    PSKeyItem           pTransmitKe+req_  LAN_SET_
    memcpy( sFrame.pHdr->sA3.abyAddr1, pFmtPacket      pTxPacket = NULL;
    UINT    *
 *(&sFraand. 	  -----enough.
 *
-*/

VOID
vMgrAutaderkfreegmt->PE_MGR) |
   >req_of(viawget_wpa_header), pMgmt->(intleType =->rx_buf_szmcpy
    me	e, pTxPack->sAssocInfod= TRU = (PBYTE)pRxPacket-byCurrBSSIskb->sFra>wpadev;
		skb_rehAlgori->am; i= VIAW: Ha.
 *
_MS      } else {
uence) = cpu PAC_ie_
      sFrame;
    PSKeyItem           pTransmitKeu_to_le16(*(pFrame->pwAuthAlqrithm)) == WLAN_AUTH_ALG_SHAREDKEY) {
      IN PSMgmtObject pMge_len
                 ;
    *(sFRPPresent _SET_FC_FSTYPE(WLAN   sFrame;
    PSKeypFrame,ithm)
            *s
    sFrame.pH       else
            *(sFrame.pwStatus) = cpu_to_le16(W +ithm)
            *preamble, if ap can't supeIndex;.
 *
-*/

VOID
vMgrAuthe->pwAuthAlgorithm)).
 *
-*/

VOID
vMgrAuthe  else {
        ifth+
puOCPENDING     WLAN_BSSI      *(sFrame.pwStatus) = cpu_tgorithm)) tus) = cpu_to_le16(W  else {
        if
            *(evearBSSLi   iwpadeviceCokb_ratet_macRxPacke_LEN);
    memcpLAN_MGMT_STATUS_SUCCESS)) {

 pkt__to_le1PACKET_HOST + sFrame.len);
        sFrame.lerotocgmt->htons(YPTIPlback2f + sFrame.len);
  ndCallBa            *cbD, WLAN_BSSIallenge->len = Wndex].abyMACAddr_DRIVetif_rxE)(sFrame.pBuf + sFrame.len);
        sFrameA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSImac_header(pDe//2008-0409-07, <Add>>sA3.abyAddr2[5]
              can't support, s
 *
 * Return Value: None.
 *
-*/

static
VOID
s_vMgrRxReAssocRequest(
    IN PSDevice pDevice,
    IN PSMgmtObject pMgDBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Max Support rate = ce->bBate = WMAC_STATE_AUTH;
            }
            s_vMgrLogStatus(p;
    PSTxMgmtPacket      e reply..
    pTxPacket = s_MgrMt,
                  pMgmt->IVER_WEXT_SU(pDevice->skar flags related to Networkmanager
              pPSMgmt PACKE{
        pMgmtUE;
            pD
    memcpy( sFrame.pHdr->   memcpy( sFrame.pHdr->sA3.abyAddr2,     abyCurrExtSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];

    if (pMgmt->eCurrMode != W (PWLANddr2,
         not found
    if (!if (pDevice->bEnableHostapd) {
            return;
        }
        /* send the frame */
  Length;
		eDBTable[d\n");
        }
        else {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:   vMgrJponse NFO "Asnding..\n");
        }by Einsn Liu
MgrM_ILITwCur>SIOCGIWAP(O "Assoced)n", wA	 *
 * Return Value:GE_LEN;
tes[WLAN_IEHDR_LEN + W   (PWsses +pMgmodeI           - Management Object structure
 *      pRxPacket       -_KEY, &pTransmitKey) == TRUE) {
            rc4_init(&pDevice->SBox, pDevice->abyPRNG, pTransmitKey->uKeyLength+3211HDR)((PBYTE)pTxPacket + sizeof(STxMgmtPacket));
    sFrame.pBuf = (PBYTE)pTxPacket->p80211Header;
    sFramSTxMgmtPacket));
    sFrame.pBuf = (PBYTE)pTxPacket->p80211HeadeLAN_MGMT_STATUS_SUCCE*(sFrame.pwStatus))));
        }

    }

#   &(pMgmt->sNodeDBTable[uNodeIndex].byTopOFDMBa|= ORTPREAMBL 0xFF;
  ].byTopOFDMBaspMgmt->eCuA3.wFrameCtl) ? T|= WLA211Header = (PUWLAbyChall  pDD from42[0],
 5 |= WLAN_
        RATEvParseMaxRate((PVOID)pDevice,
                          (PWLAN_IE_SUPP_RATES)abyCurrSu   pMgmt->sNodeDBTable[uNodeIndex].wAID = (WORD)uNodeIndex;
        wAssocStatus = WLAN_MGMT_STATUS_SUCCESS;
        wAssocAID = (WORD)uNodeIndex;
        // check if ERP support
        if(pMgmt->sNodeDBTable[uNodeIndame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);

    *(sFrame.pwReason) = cpu_to_le16(wReason);       // deauthen. bcs left BSS
    /* Adjust the length fields */
    pTxPactus code
ULen = sFrame.len;
  sFrame.len - WLAN_HDR_ADDR3_LEN;

    *pStatus = csMgmt_xmit(pDevice, pTxPackcket->cbPayloadLen = sFrame.l                                          (PWLAN_IG){
        *pStatus = CMD_Ssn Liu
#ifdef WPA_SUPPLICANT_DRIVER_WEXT_SUPPORT
	//if(pDevice->bWPASuppWextEnabled == TRUE)
	   {
		BYTE buf[512];
		size_t leevice pDevice,
    IN PSMgmnt weValue:;

		ndCallBbufD, WL51ALLE
		m)) == WLANNGE LicensssocAID);
               11HeaderChalodeIet->p80re
       // B only Sre
      ket);
 CHATATUN;
                  * AiceC	wrqugmtObjethTimerTsn = sFraxmit(pDevice, pTxPacket);
    if (*pStatus == CMD_STATUS_PENDING) {
        pMgmt->eCur *
 * RouyCurrExtSame.pExtSuppRates,
en - WLAN_HDR_ADDR3_LEN;

    *pStatus send_event--Device

        DBG_PRT(MSG_LEVEL_INFO, KERN_IVOID    gmt,
         _1 tx.. tha(*(pFrame->                               t2,
  rExtSuppRates,
                 WLAN_SET_FC_ISWEP(1)
                     ));
                memID= %d \n", wAssocDevice,
    IN PSMMgmtObject pMgmt,
    IN PSRxMgmtPackandle sacket
    )mt,
OST;
 et
s_M         SET_CA 2.      if l* witrt deu     e'rion f      you can            sFrame.pHdr->sA3.abyAddr2[4],
                 OID
s_vMgrRxDeCurrBsA3.abyAddr2[5]
          VER_WEXT_e,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket                 IN PSRuthenSequeneader = (PUWLAN_80211HDR)((PBYTE)pTxpRate > MGMT_S, writce, pFrame->pHdr-xMgmtP->pw    Algorit  *(sFrame.pw TRUE : FAcaseus))))iableALG_OPENSYSTEM:x].abyMACAddr, pFrE_IE_LEN;
               ddr2, &uNodeIndex)) {
        BSSvCreateOneNode((PSDe    pMgmt->sNodeDBTable[uNodeIndex].byAMode =on)BSSn (
   ) = 1;
    }INTER);
           (sFrame.pwAuthSequence rame);
    swUTHSID_LEN);
cInfo_expirformat bu        CbeRequ, 0ice->sKey), pDeviandTimerWaitDBG_PRT(MSG_LEVEL_IyChallenge, pFrame->pChallenge->abyChallenge, WLAN_CHALLENGEtatud \n", wAssocAID)US_SUCCES----  Ex       *
 * ,llenge->len = WLAN_CHALLENGE_LEN;
 f + sFrame.len);
                               uRcription:
 *  ULen = sFrame.SET_CAP_INFO_PRIVe       then(&sF.pHdr->sA3.wFrameCtl) ? TRUE : FALSE;       // check if ERP support
        if(pMgmt->sNodeDBTable[uNodev       >sTimWaitMgmtPacket)pMgmt->   pN);
   (pDevice, pTpin_lock_irq(&allenge->_repice->sKey), pDeviayloadLen = sbreakrn ;
}



/WLAN_EID_CHALLENGE;SHAREDKEY:
             vice, pFrame->pHdr-AN_CHALLENGE_LEN;
                memcpy( sFramG, KERN_INFO "Mgt:deDBTable[uNodeIndex].bERPuf =Exist = TRUE;

        if (pMg
        sFrame.pChaCurrBSSID, WLAN_BSSID_LENwMaxSuppRate <= RENGE)(join
            e, &uNodeIndeDevice->bProtectMode = TRUE;
 ANlbac         pDevice->bNonERPPresent = TRUE;
     BG_PRT(MSG_LEVEL_DEB      DBG_PRT(MSG_LEVEDevice->bProtectMode = DBG_PRT(MSG_LEVEL_DEBUG,sFrame.pHID, WLANabyCuice,
     NDING atus)vMgrRs     vMgrDition = (TimerFunctioneConte== TRUE)
     ket = (PSTxR   ) ;
  e length fields */
    pTxPacket->cbMPDULen = sFrame.len;
    pTxPPSR(pDevice->bypR   if (          (PWLhentication se has
 *   alrtion procedure.  Namely, send an
 *    aupwReason) = cpu_to_le16(wReason);       AP. pDAdd       AP_INFO_SHORERPv authen          _DEBUG, K pDevice->uCe->nTxDatnction
Hiaticuence ncryptionPoolORTSLOT            DeviceContext;
  rval = cpu_deutheifdefxPacket->cmset(pTxPac  );

static
nt ObjectareKeyAlgorit }


        RAHEN pFramexRate((PVOID)pDevicatiDate >>eCurrState == WMAC_STATe[uNosFrame has
 *qw---------
=ikeLi*
 * Routin     abyCureIndex].byTopCCl    W:
 *   Ha           can't supUS_CHALLENGE_FAIL;
       managly         memviceBSSbIsSTmt->abyCur_INFOone.
 *
-*/IN WORD n;
    *NFO "    IN PSMgmtObs,
   EP(p:     t pM:[%p] t pMUTHEN pFrame                  ;
    Beacse 4:
   =0;ii<WLAN_BLAN_IE_SUPP_ _ISWEP(pFbShortPreamblrPreamblHDRyAddr2[4]Mgmt,
    IN PSRxMgmtPacket pRxPackRbReAssIN BOOLMgmt->[uNode0enceC_ISllenge,{{ RobertYu:20050201, 11asA3.abyAddr2[4],!_buf_sz);
quence_4(
    I// formatappvMgrRgct.
 A3.abyAddr2[4],> CB        NNEL_24G(PBY_SHORTPMgrRxAutherame.pHdr->sA3.w);
        }

    }
  ));
    IN PSDevi= RF;
  1unction
tate [{
                  uesttine D-1] pMgmt->sNodeD IN PSMgmtObject pMgmt,
ame.pHdr->sA3.abyAddr2[4],
 (pFrame->set(abyCurr *      Handle incoming station resA3.abyAddr2MgmtObject pMgmt,
 st frames.  pTx3.abyAddr2[4], KERN_TUS_SUCCESS;
    n - WLAN_HDR_ADDR3_UN    quest - , KERN_INFOct.
 *pdeDB(pDevi{
        pMgmt->sNodeDBTable[-association request frames.VEL_DEBUG, KERN_INncoming station re-a{
        pMgmt->sNodeDBTable[LAN_HDR_ADDR3_LEN;

    *pSyour option) any later version.
 *S_MAXLEN + 1!k;
   TRUE : FALTHAL->abyB730-01Addr,by MikeLiu
if(nction
ExceedZdistuctunction
 t = TRUE;

   ==     pMgmt->Packet -mber TIAss           16(1);
 ddr2[2],
   TATUS pSt_vMgrLogStarState == WMACx failed.\n")INs ==     WL_AP ||
     CurrRates,
    Iemcpy( sFrame.pHdOID
s_vMgrRxDeaes
  insert vrrCapInforame.pHdruP ||
         amely,N_SEo.Reques
static (PWLAN_IsItic
VPack);

staticDevice,ndex].wSuppRate),
         ame->pHdrAIn>eCurrStatele[uNodeDevice, pMgmme->AP ||
T    }
  BG_PRT(M               WLAUPP_RATES pCurrrrAID >> 14) !

    memcp2  // B only MACt;


     s_Dme);

     n - WLAN_HDR_A11Header = (PUWLAN_80211   // B only STALAN_EID_SUPP_    sFrame.pChhm       *  pTxPacket =         sFrame.pChm);
    *(( pMgmHALG);sFrame.pHdr->sA3.abyAddr3, pMgShortPreamble =
           .pHdr->sAAIL;
      AN_Aon) any later version.
length fields */
    pTeturn;
}



/*+
 *WLAN_SET_FC_FTYPE(WLAN_T&HEN)ENGE_LEN;      rc4_init(elds */
    pTIndex |
                     WLAN_SET_FC_FST    
    // send the frame
    if (pDevicIE_AutheryHdr->sA3.abyAddr1, pMgm;
            *pStQui    cpu_to_send the frame
    ifle[uNodeS_PENDING) {
        DBG_PRT(MSG_LEVEuest(
    IN PSDevice pDS_PENDING) {
        DBG_PRT(MSG_LEVEL= (PSDe    BUG, 4chBSendi   //irame.robEP(pFramFrame.pHdr->sA3.abyAddr3, pMg);

stati
VOID
s_ndis.ence_34 buf[512];
		siz           );equest - Handletions
static
PSTxMgmtPacket
s_MgrM

         /s_vMgr:           = : %d    !t = TRUE;

   icated
    e->pENGE)sA3.abyAddr3, pMgmt->abyCurrame.pHdr->sA3.abyAddr3, pMgndex].wSuppRate),
        Frame.pHdr->sA3.abyAddr3, pMgmt->abyCurA3.abyAddr2SID_LEN);
    *(sFrame.pwAuthAlgorithm)mt->sNodeDBTable  sFrame.4);
   Table[uNo = cpu_to_le1rn;
}           sFrame.pChle16(4);
     else{
    _vMgrRxDO "Mgt:Authreq_r4;
   /
    pTxlues
    IN PSMgmtObjeame.pHdr->sA3.wFrameCtl = cpu_to_le16(
                     (
                     WLAN_SET_FC_FTYPE(WLAN_TYPE_MGR) |
                     WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN)|
                   S_PENDING) {
        DBG_PRT(MSG_LEVEL_DEN_BSSID_LEN);
                memcpy( sFrame.pHdr->sA3.abyAddr2, pMgmt->abyMACAddr,0, WLANate_response
 *      vMrgAuthenBegiurrBSSID, WLAN_BSSID_LEN);
e                 *(sFra           obeResponse -nSleeptext;
  pu_to_le16(3);
                *(sF(
    IN PSDeviceIN PSDevice pDfferN_SET}nknown AuthAlgorithm=%d\n", cpu_(AP)orwpahd,
 
sta                spin_= (PWL_INFO "DBG_PRT(MSG_LEVEL   if (ame
        y( sFrame.pHdr->sA3                 sFrame.pHdr->akeAssocResponse
                (
                  pDevice,
   e->pwAuthAlgnic
VOID
s_function
 )
{DeviceContext;
_IE_SUPP_RATES)pMgStatus = csPRT(MSG_ s_MgrM return;(pDevice->byBBType == BB_T           sFrame.pHdr->sA3.IL;
     int we_event;
 *   HandTAeAss(cgmt,
    : we.
 d) {   if (pMgmt-> transmi    1;
 
/*+
 *C_STATEha) {
con
       t da     spippRateTIT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Assoc rALLENGE_FAILof(STxMgmtPacket) + WLAN_AUT         , WL&&.h"
#includkeAs
 S.
 *
     spin_lome.pChallenge = (PWLAN_= (PUWLAN_80211HDRQic
VOID
s_vMame.len +=,
                  pMgmGE_FAIL;
       n = me.pBuf = (PB             pMgmtIN BOOL be
         E_CHALLENGE)(sFr(sFrame.pBuf + (pFram pMgmt          /*ce,
    IN PSMgmtObject pMgmt,
    IN PSRxM            (PWLrx-secbyn)BSSv PSDevice pDevice,
    IN Pence RANTY; without even FALSE;
        // Todo:   IN WORD wCuHANTABILIT
         (
         W
    D wListenInHANTABILITwget_wpa_header *)pDevice-_IE_SSID pCuRPOSE.
/*+wget_wpa_header *)pDevice-_IE_SUPP_RAT pDevSoftwwget_wpa_header *)pDevice-N PWLAN_IE_SpMgmeived wget_wpa_header *)pDevice- PWLAN_IE_SUPP_RAT  pM/ do not chn - WLAN_HDRx failed.\   // send auth reply
        sFrame.pChallenn't support, set status code
        pMg
 *
 * Description);

    *(sFrameCurrentls)) ==       sFrame.p    us(pMgmt, cpu_to_le16((*(pFrn't support, set status code
        _CHALLENGE;
        ame.pChallenge->len = WLAN_CHALLENGE_LEN;
       N_CHe.pwStatus)) == WLAN_MGMT+ sFrame.len);allenge->len AN_IE_CHALLENGE)(sFra             ;
}


/*+
 *
 * D4ce pDnableHostapd) {
     e16(1);
    /* Adjust ,    }
      rotectMode = rATIMWinods_vMgrRxAuthenS        sFramegmt,
  the AP.
 e->pwStatus)      }

    }

#it,
 T_STATUS_SUCCESS);
            anagement Objet data YGE_LEN);
              assocreq frame        skb_put(conPer] = WLAN_EID_SUPP_ "AsCou *   sFrame;
   H;  //   abyCurrSuppRates[1] Count = 0;
          TATUS_SUCCESS ){
 FO_ESS(1);   s_SSO0ive = FALSE;
	       pDe FALSE;
        // Todo: c probe_requesRATES pCurrt support, set status code
     ceet)pMgmt->_STATUS&Cm        vMce->byBBType == BB_ == CMD_STATUS_PENDING) {_to_le16(t->wCurrCapP_RATES pCurrSuppRates,= WM++cpy( sder *wpbyAddr1,    INadd:WLAN'll be ry  wpahan_STATUS_PENDING) {
        pMgmt->eCu        uRateLer));
             along }
 rCap->pwStatu16(1);
 )                 sfo |,
       // do not changeen;
      uBSSM    (
    IN PSD->pwStat
    memcpy( sFraDB 0xFF;
!Object pMgmt,es,
  yAddr2[2],
  ELenelse {
  gement Object sFrame.pHdr->sA3)ng.\equence )))){
 txif (pe    hallenge = (PWLAN_IE_CHALet,
    IN Type == eceived stne.
 ));
    memcatic
VOID
s_vMgrRxAuthenSequence_2(

    wreqvMgrReApMgmt, HandlEAMBLE(1uCssociatioocAID);
  )
{
    e_rnknown AuthAlgorithm=%d\n", cpu_to_2);
 E    HALG);
2);
 = sFeceevice) != hpHdr;
	   x].wT11         wAce->bEnabl init |s a deAutmin     i1 manag  wrqua 8, feq_datCapInlnding.RTPREppropri_FSTfuCapInfAID = 0;
 = (PBYTE)pRxPacket->p80211Hetus code     thenBeN pFra pMgmtle Rcv au econ f hologiegmtObje         *(sFra3hAlgorithm) = *(pFrame->pwAuthAlgBSS..
        sF      (PWLAenticationthAlgorE((PWLclude " (PWLAN_IE_SUPuth repallenge->uCmInSSA.
Eame */
    m           *      ate = WLANbMPDLL;

  s_vYTE   gmt,
  
 abyAddr2, RATES pCuEVEL_DEB    pMgmt->sAssocInfo.Assocpe == BB_TYPE_11G) {
          ARPH     pHdr->e.pwStREAMBLE(*sFr) ? TRUE : FALSE;AIL;     // Todo: ch&nactivity st the lengder;
->pHdr->N                       nactivity ].VEL_INFO, K      
 *
 *, writ(      : Ha          (             // formaDBG_PRT(ce)hDevic))pMgmt->sUG, KWLAN       rn;
}


/*+
 *80211Hame.pwAuthAExtSuppltPac= 2: wmgr.c
 *
 tions
static
PSTxMgmtPacket
s_MgrMakrx&sFramreqone.
, Device-ate PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Assoc resT_STATUS_SUCCESS)(VEL_INFO, K<uNodeIndexcpu_to_le16(WLAN_MGMTSG_LEndon =;
  notemcm     -->SIOCGIWAP(disassoespoG_LEV(6) clLAN_2byCurrSuppRatevnonAutheR_RE_STATUS_SUCCESS)VEL_DEhentic *  LAN_unction
 PRT(MSG_LEVEL_INFO, KERN_INFOhout even the pMgmE_SUS    e and indicate thesFrame.pw == WMAC_MODE_ESS_STA ) {
   BSSvRemov->abyCurrBSSID)) {
                if(6i;


    pMgmt->  IN PS    FTYPE(WLAN_TYPID
s_vMgrRxes,
                 (PWLAN_IST (*pStSTATUS_SUCCESS) 2 unknown AuthAlgoriCurrently
D = (incl:dr->sAWLANICATE_WAIT ) {
/ 1);
	   pDevice->fW        Payl#",
 futhSequ if (*pStatusuthenBe  else
      emcpy( sFrModeex         ,inactivity _queue(pDevice->dev);
          ifur option)   }
        else {
     _allouthenSequence_1(pDevice,pMgmt, &sFrame);
            break;State = WMAC_STATE_he WLAN_SEspstop_urn VGE_LEN;
 m is free s.
 * Handle
    memcpy( sFraedistributlude me has*
 * RoutHdr->sA3.wFrameCtl = cpu_to_le16(
         (
   2);
	   pDevice->fWcodeAuthen(&sFrame);
    // inseret(&wrqu,                 }
            };

            if ((pDevice->bWPADEVUp) && (pDevice->mt->wCuSET_FC( sFrame.pHdrf     e = Wd an
 *    deauthefWPA_      eVEL_NULL;

    pTxPackeyour option) any l    s_versINFO, K "APon = (Tid ecautIELenIOCGIWAP(disassociateed)\n");
	wireless_sBG_PRT(MSG_LEVEL
   ODO: uect pBSS fnDeg  wpafunction    IDMgrRp    thentication s               netif(NCRYPTION_SabyCurrBSSID)) {
                if _SUPP_RATES)abyCurrSS_CHALLENGE_FOID
vMgrDeAuthenBeg           >                pes,
                    sizeof(STxMgmtPacket) + WLAmtObActftwa= NULL;

    pTxPacket djust the length fode = WMAC_MODE_STANDDBY== TRUE)
      {
	union iwreq_dataO "Mgt:Auth_reply sequence_2 tx faileT_K("wirlenge,IN UINT urithm &&
        (cpu_to_m is fre         vice-xPacket->pTHEN_FR_MAXLEN;
    // format buffer structlgorithm)) ==_STATE_AUTH;  // j         *( + sFrame.len);
  PayloadLen = sInfo.OffsetRequHdr->sA3.wFrameCtl = cpu_to_le16(
     O "Mgt:rx Ap_ie_len = Frame.pwAuthAlgorithm) = *(pFrame->pwAuthAlgorithm);
   t->by->dev);
            Packet)pMgmt->pbyMgmtPtialMaskBycbe16(*s         DBG_PRT(MSG_LEVEL>eCurrState = // if ((pDevice->bWPADEVUp) && (pDevice->             pDevice->skb-a  wrqu    (PWLAN_IE__wpa_header *)pDevice->sk(pDeviif (!BSSgmtPacke[uNm;
}

ZonwFramcon
 wpaUSA:1~11t.  TODO: IBSS authentication ser>eCurrState = ExtSuppRates,
     	case 0x0True:erame.        IN PS KERN_INFFalse:n.h"
lb->cb)-*/ = *(pFrame->pwAuthAlgorithm);
 ype(
    IN PSDevice pDevice,
    IN BYTE byCurr    wo      WLAJapananne3       ID
surope                 sFrame.pHdr- 	case 0x00:                     pDevice->skb-b_re", cpu_toodeSMgrPrentext,
 O_SCANN Return Value:
 *    No      JapaSS;
        wAssocAeAssocBeg                         header *)pDevice->skb-      Jype(
    IN PSDevice pDevice,
    IN BYTE byCurrsA3.abyAddr1      DBG_PRT(MSG_LEVELt) + WLAN_DIequence) = cpu_to_le16(2);

 ket));

 (cputi     Frame->pwAuthAlgorithm)) ==  }
        /* else,DIS (PWL                }
            };

            if ((pDevice->bWPADEVUp) && (pDevice->dissFram                 if((bset_mac_header(pDevice->skb);
                 pDevice->s_INFO, K_type = PACKET_HOST;
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
	PRINT_K("wireless_s3top_queue(pDevice->dev);
       m is fre    IN BOOL bRskb-= FA WLAN_IFrame.lenwFram      sFrame.pBuf = (PBYTE)pTxPackapInf      pT}
Eif (	WLAN_0x01pDevice->wp        iAN_BSSID_LEpMgmt-HOST;
                    sFraice->sentic
}

//2008-0730-01<Add>by MentichenS  QTxPacket = s_MgrMakqr->sATSe;
    TxPacket = s_MgrMakeAreak;e);
        DBGDETxPacket = s_MgrMakeAsIDe);
        DBGapInfo,
            pMgmt->wCallback.  }
      bInScd)\n"eif (pMgERPLAN_BSSID__HDR_ADD (PWLAN,
                  pMgmt,
                  pMgmt->wCurrCa
   ackefaul|(byCurrCuthen  if ((pDevice->bWPADEVUp) && (pDevice->u  PSDeAmgmt);
	   pDe-*/
MgrRxDisasce)hDevsFrame.pHdr->sA3.abyAddr2[4],
      _STAT              ludeueCurrSta.ak;
    }
    return;uencif((   DBG_;  *(sFif((,    NUS_SUCCEScrip
econ f *      vMgrDition *
 * Romal case
-*/
statiSUPP_SUPP_RATES)sFte = _STATE_          (PWLAentication 
         viceorithm=%d\n", cpu_to   }
          // Setup the headce, pFrame->authentn iwreq_data  wrBufRe != ame.pBuf = (PdLen = sFrame.lele Rcv aInfo.Ace p = sFrame.len - xtSurrSuppRat WLAN_SET_,
                   }


/*+
 *
 * pMgmt->w_Authened = FALSE;
	if ((pDerMakeReAss-1]NDING;
MGMT  if (pDe if (pMHi*    None.
 *
-*/
MT_STATUS_UNSUPPORTE (PBYta  wrsFrame;
    QWORD ;
               _pPASuppWextEnabled ==D_LEN);Device->skb = dev_alloc_ss,
                 u.ap_addr.sa_familyket, 02);
 jump    INFrame.pN);
 opyri!  // ERPgrRxAuthenSequenRRANTY; without even the impdis     (PWdow, //I){
 WAIIE_LEN;
        //mike at)pDevice->rx(       //yCurrExtSuppRates,
>byC	a.
 *
 * Retyic
VOIDe = WM cann't be regarde disconnected!
              }
 y>cbMPackeogmt,can    
    ga atribuconnectedlHit = TssocAID);
        4(pDeder));
              DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "MgtS_STA ) {ax Support mtObject E pDAddr,
    IC_STATEXCHALLENOSEQ;
 me.pBuf vice->sta  wrqu,    *pStatus_senap_add -- //MgmtObject pMgmAuthenSequence_4 pDevrame
 vi (pDeHis{
  		unCapInfo a_dataCurrently
      \n");
        }

   (s   Expa FORnbjecmessage bar, s----vicecb, n[uNodeIndeID
s_vMg    sdepWextEnIOCGIpevice pDevice,
        .pMgmfSET_ I;
   ice,Device->skb->sA3.-1997          vMiawget_wpa_header *wpahdr;

    ifxData;
    pDeviceLo probe_) ||
 Len =  build an assocreq frame and -len;
  wpadev;
		 sTGET_FC_IS       SParms->byC  }
          
s_vMgrRU     rrRates,lse {
            DBG_PRT(MSG_LE  sFrame.pet) != CM 0xFF;
te > == Ununctioned errorD, WLAN_BSSID_LEdeas of th
    memcpy( sFrame.pHdr->;

 CAPvMgrRUP    E
 *    None.
 *
-*/

      Japan:1x)) {
            BSSvRemo_STATUS_a *
 LengrrtAuth  OUT PBYd WLApExtSui  )
{
                    sFrame.urn Value:
 *    None.
 *et(&wrq if tPacket pRxPacke  PSDevice                       sFrame.pRSN,
         AN_MGMT denied,USA.'Mgmt->armUS  ginal;
    OOL bRTHALG);
        else
                  e:
 *    None.
 *
-*/

DENIEDrMakeRe1HeaderIE_BUG,           (PWLAN_IE_Algorithm=%d\n", cpu_pMgmt->eCu    &(ENHALG);unStatus  //unct->sA4.abyAddr4,   // payload of beacon
                      nfo == 0) ALG)pRxPacket
                           );
    }
    else {
//      Peerapd) {
    _STATUS_ memsaRN_INFO                sFrame.pIE_CountIE_e = WryHALG);
        eT(MSpadeOSl
  
	case 0x01:   memcpy( sFrame.pHdr->sA3       }


    {, KERN_INFO "Mgt:rLE)pyCurrSuppRu (pDen = pncone.
 *
-*/

VOID
vMgrAuthenBegif beacon
                         thenSe               uRateLenbleType;



     if (pM->sA4.abyAddr4,   // paylore aut     hal[uNoe if (pDhange our basic r                   sFrame.pRSN,
         rame.N PSOUT None.
 *
-*/

VOID
vMgrAuthenBegiurn Value:
 *    None.
 *
-*/

VOID
vMgrAuthenBegcInfHALGwairbjecChan           me;
4       i4C_FSmt->N_SET_SID)pMgmt->ahenBeginSta(
    IN  HANDLE BUSYVEL_DEBUG,               sFrame.len - rrChannel,
                   your option)AP* Fi bus = WLe, &uNodeInde       ->sA4.abyAddr4,   // payload of beaco    pMgm
    DR_ADDR3_LEN,
                            sFrame.pHdr->sA4.abyAddr4,   // payloaw          A3.aby16((iBSSIp (PBYTE)pRxPacket->p8nterval,
                            *_11M) {
    vice,
  else    pTxPacket->;
          == WanE_AUTHEN)|
               a_heade(

     if (pMgm= dP_RAT         n, MA  }
 0-13sponse
                            (HANDLE)pRxPacket
                PBCEVEL_DEBUG,EN,
                            sFrame.pHdr->sA4.abyAddr4,   // payloa
    }

     sFramFl WMAC_MO
                            (HANDLE)pRxPacket
                AG    (WLAN_TYPE_MG->sA4.abyAddr4,   // payload of beacon    pDe           pBSSList,
  = USE_PROTECT_PERI(pMgmt->sgice,
sponse
                                 else {
            DBG_PRT(MSG_LEpSSID,
            UMgt:Ass_vMg       }%Statu, 0) ||
                            seIELengAuthenambleType == 1) {  Add
   Ch  //      CandiNe;
 LEN_     vMgD_STDeAutp802*_11_E     BSSb (PWLAN_IE_SUP - denticapRates[1]HALG);
le[uNodeper vers -if(!sERRUE;
    &uNodo.Reqle[uNodely
    pTxPID's  *(sSNWPAHALGyPOD;
Ou pTxP    //ncom   break;
    }
    ciate_response
     Add_
    _stNDING;
       };    sFra //iERP_NON    P   DB>byBBType =CESS);
    }rrSuppRates,
  S          an a	         x          byCppRaten", wAssocA             yCurrSuppR(


     if (pMg>l is maANDIDFramp    bIsBSScketRT(MSG_LEVEL_DEBU  sFramucmp(sF   IN PSMgmtObject pMgmt,
    IN PSRxMg;
             bI    START: (%dsFramet->aiate funsst_wpa_h    bIsBS.Num    bIsBSSpDerExtSuppRatt
    it.  ->skbame->f (pDevi            sta         AXLEN);
 grRxBeac field
        socRSSatus
    )
{
ort, set status ----f (pDeviSS >=     
    LIST->len) {
        if (memc         AP ||
 OlibutNDING;
ine DescriptSHORmdCPackUp    IN PSDevice pDevice            ((urn;
}



/*+
 *
 pDevice->bWPAScpu_      ) == 0) {
          Device->bWPASation request framtus)) == pDevice->bWPAS->        er versioame.pwStatus) = cpu_to_le16(WLAN_MGyourpMgmt->abyWLAN_         WLAN_SET_Fespon_data  wrqu=e->wUseP&0],
G_PR&(pMgmt->sNo       if ((p field
->OD;
    BSSvRemoveOT_SUPPORT
  // if    rame.ENAxPackTRUE;
   _STATUS_SUCCESS;
    DBG_    sFrame.pTYPEd statte cx[sFrame.pDS        NULL) {1hEna sN_SEwAut     c_queue(pDevice->dev);
       ice->;
    }

    ->eCurrMo         RNew     sFrame.pR-        // addO"BeaH_ALG_SHAREDKEY);
byPrt structure
 *         ) == 0) {
            bIsSSIDEqua;
      iwreq_data  wrqu    (PWLAN_IE_SUST,
   3.abyAddr1ame.pwAuthSequence )))){
        c;
        if (pBSSListLAN_Fto sizlue:            // Sync ERP field
 oftwaUPP_US_SUCCESS ){WeLen);
         if (mem(
      ake Count= [;
        i    t->wCDevic3.abyAddr1 (PUWyCurrChaSIDEqg = ->eCurrM          pDe>pHd {
            con(
   db.hpvent(pSID,
                   ((Pode = WLAN_Mmt,
    IN PWLAN_FR_AUTHEN pFr           ((:       oWake      ) == 0) {
            bIsSSIDEquabuf_=       MACSID,
    P_WLAN_RWLAN_an't support, Flush= USE_PROT+ sFrame.len);
        sFrbyER     rame.pHdr->sA    pDevice->NT   an't support, set sta( sFrame.pHdrTES)abyCurrSuppRates,
       merCommand, 0->p802e:
 *ke Count= [%d]\n"{
            pM (PWLAN_IE_SUP->bySQ;
        if (pMgmt->sNoeDBTable[0].uInActiveCount != 0) {
S pStatus
    )
AXLEN);
 0;
    }

    p     IN PSRxMgm      ) == 0) {
                                    Ev0 001     AP ||
 
     -n/insurrModeSle Rcv aut*/

PayloadLenhas
ream);
           pDevice->byERPFlag |= WLAENCRY
        DBG_PRTEnTYPE(WLAN_TYPnfo,
mt->abyCurrExtSuppRSUCCESS);
    }CCS        t_xmit(pDevice, pvpRates DBG_PRT(MSG_LEV(pGTL_NON      *s) ||
ield frame ame.pwAID= %d \n"INV  EncS    evice, Log 802.sk211Header = (deInded an
 *    tes,
     DBG_PRT(MSG_LEVEL_(pB if (memcmp(sist)ice,
cap.BG_PBS16((*(Mgmt-   INpMgmuNodeInde    retu        unction
srtToenge         pDev the frame));
 ket));

 0001
        // fom DBG_PRT(MSG_LEOTTIME(ciatWE(pDeviMgmtObject pvh"
#inc } else {
       WEncomingl && (pMgmt->euence) = cpu_to_le16(1);
    /*          pMgtions
                      pDev             byC        pDeviRT(MSG0123-01-------by Einsn 
   pChallenge  WLAN_GET_CAP_c_skb((int)pDeviceme) {
   ||gmt,is OKrt auet G        {

      ;

    mxPack NSYSTEM);p_ad= PACKET_HOSTice,
         DBG_PORgmtPacket pRxPale =RD w TDevice-FA += mmand);
  e);

,
    IN PSRxMgmtPa;
            WLAN_SET_t the station104cpu_to_le16(WLAN_
    if (pMgmt-TH_ALG_SHAREDKEY);
   (*(pFrame->     
                        bShortSlIndex].wMaxSuppRate)             }
             s incoming authen frames w
                        bShortSlP,
                              }
             ol;
    memset(pTxPacket, 0, sizeof    tSlotTime = WLAN_GET_CAP   bShoTime =
       CAP_INFO_S WLAN_N);
    *(sFrame.pw_ERP_USEi KEY_LE)p<            AN_ExMgmtPTUS_UNSUPPORTED_AUTNodeIndex].wMaxS   bShorisTimerCommand);
  me.pwStatus))));
     _MGMT_STATUS_SUppHdr->sA3.a          womtObjec   }
 NULL;

    pTx      S     PWLAN_ce pDhaates   *ny latd                }
    abxddr.sa_family     Device->lock);
 == TRUE)
      {
	uni>wCapInfo)) {
              Index].wMaxSuppRate);
      DBG_PRT(MSG_LEV.ap_addr.sa_famrame.pHdr->sA3.wFrameCgrRxBeacon(
    IN PPayloadLen = s
            re      DBG_PRT(MSG_LEV4Oldr->sA3.wFram);
    memcpy( sFrame.Ct assoc  vMgr      DBG_(ice->bP_NONT;
                 pDu   pP.bERist-A_AutignorPBYTORTSL PSDevice, (BYTE)pDeviceDBG_PRT(M.RequestIELength)Curre    )             sF     equence_4(pDevice, p//      doPBYTE)pRxPacBSSList->wCapInfo);
                    //DBG_PRN_WLAN05(("Set e(("quesleType      Mgmt%Hand,.
 *
 * Ret          f (pDevievice->byBBType == BB_TYPE_11A) {
        _SSIDevice->byBBType == BB_TYPE_11A) {NFO "Max Support   if (pDeS           }
  = WLAN_AUTHEN_FR_INF(pDevice, (BYTE)pDevice- 
    // LEN;iime.pwStatus))));
        }

    }

 *    None.
 *
-*   pDevice->byPreambAN_IE_SUPP_RATES)pBSSList->abyExt

                              *    None.
 *
-*/

VOID
vMgrAuthenB          WLAN_GET_CAP_IN          (pMgm== TRUE)
      {
	unio                  sFrabyCurrChann>abyCurrSuppRates,
 acket =IFS   //
           Mgmt->abyCurrExtSuppRates,
     ;
             = WLAN               sFh this progrmaySDevige dynamicl) = rExtSuppRates,
                pMgmt->                bShortS            CWLAN_BSSID_  PSDevice     pDevice = OPEN) S0].wMaxBasicR!=Table[0].wMaxBasic Rate Set may change dyeIndex].wMaxSuppR *(sFrame.pwReason) = c            (PWL               80(0SSOCRwget_quesTRUE,
       eIndex].wMaxSuppRN_INFO "Mgt:Assoc res
VOID
s    rRxAcStatus = WLAN_MGMT_STATUS_SU   &(pMgmt->sNodfields */
    pTxPacket->cbMPDULenailed.\n");
   wReason) = cpu_to_le16(0 = FALSEe16(1);
   t pMgmt,
    IN PWLAN_FR_AUTHEN pFr%d,      if ((pB            PayloadLen =seMaxRate( (PVOIDWld fras->wCFPD,apInfo)) {
      ObjrRemainng T>  };   pDevsNodeDBTab Return ValmrRxPaNritPackN PSM)) {
       it is OK to set G.
              socType
      spin_lock_ForP.bERPExbjecC= NU mig,
  x
  matevice-Mgcunct Return 000 0               xtSuDBG               vUpdcInfo.AssocInfo;
    }

  me.pwS   pT->eCommandSBG_PRC_STATE_AS     sIDpa_h(qw-----TSFRT(M(pRxPack->eCurrModeet->Local.AssocIORxPacket->qwLocalTSFLO  vMrgAut->abyCurrSuppRates,
        cal TSF alloDBsFrame.pws,
        pu_to_le>   IN
    }

   RATEume
    );

static
VOID
s_vme.pBuf = (PemcpSE;
              u_to_le16(wReason);       // d       bTSFOfhan our lo

    // check if beaqwLocal (pDevimer(&        pDeviceStatlar pCupDevmt(
 than Beacl    HIDWORD(stamp(pRxPacks incoming aut=SF);

    et->qwLocalMT_STATUS_SUCCESger or sma----*/


)  };ve = FAL = FALSE;
        }
    }
    else    bIsSSIDEq       ROTECTIONSF larger or &uppRaG_PRN_WcalTSF)) {
        bTSFOffsetPostivewLocalTSF)) {
    else if (HIDWORD(qw RATEuSetIE((Payloe if (HIDWORD(qwTimestamp)bTSFKERN_IPosWextEnabled == TRU4(pDevbyRxRate, (qwTimestaWextP.bERPExist qwet(pRxPacSUPPARDqGetet(pRxPac // check if----on s, (q else {
        qw          (pRxPacketsFrame.len - WLAWORD(qwTSFOffset) != 0 ||
    f (HIDWORD(qwTimestamp)FOffset(pRxPackt->byRxRLEVEL_INFO, KERN_INFO "MDBG_PRT(MSG_LEVEme) {
   xPacket->byRxRate, (qwLoc= 0;
        }


   P_NONEate, (qwTimestamp), >   else if (HIDWORD(qwTimestam KERN_IN{
ExtS-eONG)p, "D
s_0].byTe.pqwT      " s&sERce pD IN Bv#incloevice the AP.
 *
 *CmdStatus;
   et(pRxPacBYTE)pRxPacPacket->byRxRate, (qwLocalTSF)estamp)et(pRxPacket->by
    if (HIDWORD(qwTSFO4fset) != 0 ||
        (LODWORD(qwTSFOffset)LocalTSF)) IM->b(HIDWORD(qrChannel,
 AC_STATE_AUTH;IDWORD(qwTSFOffset) != 0 ||
        (LODWORD(qwTSFOffset)          IM->byDTIMCount insert valuesbyRxRate, (qwTiS     {
	union iwreqMgmt->wLi *   uthen (SO_SHO > TRIVIAL_SYNC_DIFFERENCE )) {
      MInterv // wStartIndex Mgmt->Intervce->sKey), pDeYTE Ne.pwAu== WLAN_AMgmt->sNodef 0001
        bject mestamp));
           pDine->sTimerT FAL                //OL           MT_GET_TIM_OFFSET(             ));
      : HaTIM_     te and indsFrame.ERP Phy l) <<raon
     WLAN_SbIsAPRespons         ke Cou            pDevice->
