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

#define	PLICE_DEBUG

/*---------------------  Static Definitions -------------------------*/



/*---------------------  Static Classes  ----------------------------*/

/*---------------------  Static Variables  --------------------------*/
static int          msglevel                =MSG_LEVEL_INFO;
//static int          msglevel                =MSG_LEVEL_DEBUG;

/*---------------------  Static Functions  --------------------------*/
//2008-8-4 <add> by chester
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
    PSMgmtObject    pMgmt = pDevice->pMgmt;
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

    return;
}

/*+
 *
 * Routine Description:
 *    Initializes timer object
 *
 * Return Value:
 *    Ndis_staus.
 *
-*/

void
vMgrTimerInit(
    IN  HANDLE hDeviceContext
    )
{
    PSDevice     pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject    pMgmt = pDevice->pMgmt;


    init_timer(&pMgmt->sTimerSecondCallback);
    pMgmt->sTimerSecondCallback.data = (ULONG)pDevice;
    pMgmt->sTimerSecondCallback.function = (TimerFunction)BSSvSecondCallBack;
    pMgmt->sTimerSecondCallback.expires = RUN_AT(HZ);

    init_timer(&pDevice->sTimerCommand);
    pDevice->sTimerCommand.data = (ULONG)pDevice;
    pDevice->sTimerCommand.function = (TimerFunction)vCommandTimer;
    pDevice->sTimerCommand.expires = RUN_AT(HZ);

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

    return;
}



/*+
 *
 * Routine Description:
 *    Reset the management object  structure.
 *
 * Return Value:
 *    None.
 *
-*/

VOID
vMgrObjectReset(
    IN  HANDLE hDeviceContext
    )
{
    PSDevice         pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject        pMgmt = pDevice->pMgmt;

    pMgmt->eCurrMode = WMAC_MODE_STANDBY;
    pMgmt->eCurrState = WMAC_STATE_IDLE;
    pDevice->bEnablePSMode = FALSE;
    // TODO: timer

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
        if (CARDbIsShorSlotTime(pMgmt->pAdapter) == TRUE) {
            pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTSLOTTIME(1);
        }
    } else if (pMgmt->eCurrentPHYMode == PHY_TYPE_11B) {
        if (CARDbIsShortPreamble(pMgmt->pAdapter) == TRUE) {
            pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
        }
    }
    if (pMgmt->b11hEnable == TRUE)
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SPECTRUMMNG(1);

    /* build an assocreq frame and send it */
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
        /* send the frame */
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
        if (CARDbIsShorSlotTime(pMgmt->pAdapter) == TRUE) {
            pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTSLOTTIME(1);
        }
    } else if (pMgmt->eCurrentPHYMode == PHY_TYPE_11B) {
        if (CARDbIsShortPreamble(pMgmt->pAdapter) == TRUE) {
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
        /* send the frame */
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
        if (pDevice->eCurrentPHYType == PHY_TYPE_11B) {
            uRateLen = WLAN_RATES_MAXLEN_11B;
        }
        abyCurrSuppRates[0] = WLAN_EID_SUPP_RATES;
        abyCurrSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pSuppRates,
                                         (PWLAN_IE_SUPP_RATES)abyCurrSuppRates,
                                         uRateLen);
        abyCurrExtSuppRates[0] = WLAN_EID_EXTSUPP_RATES;
        if (pDevice->eCurrentPHYType == PHY_TYPE_11G) {
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
#ifdef	PLICE_DEBUG
	printk("RxAssocRequest:wTxDataRate is %d\n",pMgmt->sNodeDBTable[uNodeIndex].wTxDataRate);
#endif
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
    }//else { TODO: received STA under state1 handle }
    else {
        return;
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

        if (pDevice->eCurrentPHYType == PHY_TYPE_11B) {
            uRateLen = WLAN_RATES_MAXLEN_11B;
        }

        abyCurrSuppRates[0] = WLAN_EID_SUPP_RATES;
        abyCurrSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pSuppRates,
                                         (PWLAN_IE_SUPP_RATES)abyCurrSuppRates,
                                         uRateLen);
        abyCurrExtSuppRates[0] = WLAN_EID_EXTSUPP_RATES;
        if (pDevice->eCurrentPHYType == PHY_TYPE_11G) {
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
#ifdef	PLICE_DEBUG
	printk("RxReAssocRequest:TxDataRate is %d\n",pMgmt->sNodeDBTable[uNodeIndex].wTxDataRate);
#endif
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
            pDevice->uBBVGADiffCount = 0;
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
	//if(pDevice->bWPADevEnable == TRUE)
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
			wireless_send_event(pDevice->dev, we_event, &wrqu, buf);
		}

		memset(buf, 0, 512);
		len = pMgmt->sAssocInfo.AssocInfo.ResponseIELength;

		if(len)	{
			memcpy(buf, pbyIEs, len);
			memset(&wrqu, 0, sizeof (wrqu));
			wrqu.data.length = len;
			we_event = IWEVASSOCRESPIE;
			wireless_send_event(pDevice->dev, we_event, &wrqu, buf);
		}


  memset(&wrqu, 0, sizeof (wrqu));
	memcpy(wrqu.ap_addr.sa_data, &pMgmt->abyCurrBSSID[0], ETH_ALEN);
        wrqu.ap_addr.sa_family = ARPHRD_ETHER;
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

#ifdef WPA_SUPPLICANT_DRIVER_WEXT_SUPPORT
//need clear flags related to Networkmanager

              pDevice->bwextcount = 0;
              pDevice->bWPASuppWextEnabled = FALSE;
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
    if (!BSSDBbIsSTAInNodeDB(pMgmt, pFrame->pHdr->sA3.abyAddr2, &uNodeIndex)) {
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
    if (BSSDBbIsSTAInNodeDB(pMgmt, pFrame->pHdr->sA3.abyAddr2, &uNodeIndex)) {
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
//    CMD_STATUS          CmdStatus;
    viawget_wpa_header *wpahdr;

    if ( pMgmt->eCurrMode == WMAC_MODE_ESS_AP ){
        // if is acting an AP..
        // a STA is leaving this BSS..
        sFrame.len = pRxPacket->cbMPDULen;
        sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
        if (BSSDBbIsSTAInNodeDB(pMgmt, pRxPacket->p80211Header->sA3.abyAddr2, &uNodeIndex)) {
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
        //TODO: do something let upper layer know or
        //try to send associate packet again because of inactivity timeout
      //  if (pMgmt->eCurrState == WMAC_STATE_ASSOC) {
       //     vMgrReAssocBeginSta((PSDevice)pDevice, pMgmt, &CmdStatus);
      //  };
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
	printk("wireless_send_event--->SIOCGIWAP(disassociated)\n");
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
        if (BSSDBbIsSTAInNodeDB(pMgmt, pRxPacket->p80211Header->sA3.abyAddr2, &uNodeIndex)) {
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
            DBG_PRT(MSG_LEVEL_NOTICE, KERN_INFO  "AP deauthed me, reason=%d.\n", cpu_to_le16((*(sFrame.pwReason))));
            // TODO: update BSS list for specific BSSID if pre-authentication case
            if (IS_ETH_ADDRESS_EQUAL(sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID)) {
                if (pMgmt->eCurrState >= WMAC_STATE_AUTHPENDING) {
                    pMgmt->sNodeDBTable[0].bActive = FALSE;
                    pMgmt->eCurrMode = WMAC_MODE_STANDBY;
                    pMgmt->eCurrState = WMAC_STATE_IDLE;
                    netif_stop_queue(pDevice->dev);
                    pDevice->bLinkPass = FALSE;
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


//2008-8-4 <add> by chester
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
    BOOL                bUpdatePhyParameter = FALSE;
    BYTE                byIEChannel = 0;


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


    if (sFrame.pDSParms != NULL) {
        if (byCurrChannel > CB_MAX_CHANNEL_24G) {
            // channel remapping to
            byIEChannel = CARDbyGetChannelMapping(pDevice, sFrame.pDSParms->byCurrChannel, PHY_TYPE_11A);
        } else {
            byIEChannel = sFrame.pDSParms->byCurrChannel;
        }
        if (byCurrChannel != byIEChannel) {
            // adjust channel info. bcs we rcv adjcent channel pakckets
            bChannelHit = FALSE;
            byCurrChannel = byIEChannel;
        }
    } else {
        // no DS channel info
        bChannelHit = TRUE;
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

        // set to MAC&BBP
        if (WLAN_GET_ERP_USE_PROTECTION(pDevice->byERPFlag)){
            if (!pDevice->bProtectMode) {
                 MACvEnableProtectMD(pDevice->PortOffset);
                 pDevice->bProtectMode = TRUE;
            }
        }
    }


    if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP)
        return;

    // check if BSSID the same
    if (memcmp(sFrame.pHdr->sA3.abyAddr3,
               pMgmt->abyCurrBSSID,
               WLAN_BSSID_LEN) == 0) {

        bIsBSSIDEqual = TRUE;

// 2008-05-21 <add> by Richardtai
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

            // Compare PHY paramater setting
            if (pMgmt->wCurrCapInfo != pBSSList->wCapInfo) {
                bUpdatePhyParameter = TRUE;
                pMgmt->wCurrCapInfo = pBSSList->wCapInfo;
            }
            if (sFrame.pERP != NULL) {
                if ((sFrame.pERP->byElementID == WLAN_EID_ERP) &&
                    (pMgmt->byERPContext != sFrame.pERP->byContext)) {
                    bUpdatePhyParameter = TRUE;
                    pMgmt->byERPContext = sFrame.pERP->byContext;
                }
            }
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
#ifdef	PLICE_DEBUG
		//printk("RxBeacon:MaxSuppRate is %d\n",pMgmt->sNodeDBTable[0].wMaxSuppRate);
#endif
			if (bUpdatePhyParameter == TRUE) {
                CARDbSetPhyParameter( pMgmt->pAdapter,
                                      pMgmt->eCurrentPHYMode,
                                      pMgmt->wCurrCapInfo,
                                      pMgmt->byERPContext,
                                      pMgmt->abyCurrSuppRates,
                                      pMgmt->abyCurrExtSuppRates
                                      );
            }
            if (sFrame.pIE_PowerConstraint != NULL) {
                CARDvSetPowerConstraint(pMgmt->pAdapter,
                                        (BYTE) pBSSList->uChannel,
                                        sFrame.pIE_PowerConstraint->byPower
                                        );
            }
            if (sFrame.pIE_CHSW != NULL) {
                CARDbChannelSwitch( pMgmt->pAdapter,
                                    sFrame.pIE_CHSW->byMode,
                                    CARDbyGetChannelMapping(pMgmt->pAdapter, sFrame.pIE_CHSW->byMode, pMgmt->eCurrentPHYMode),
                                    sFrame.pIE_CHSW->byCount
                                    );

            } else if (bIsChannelEqual == FALSE) {
                CARDbSetChannel(pMgmt->pAdapter, pBSSList->uChannel);
            }
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
            if (BSSDBbIsSTAInNodeDB(pMgmt, sFrame.pHdr->sA3.abyAddr2, &uNodeIndex)) {

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
#ifdef	PLICE_DEBUG
		//if (uNodeIndex == 0)
		{
			printk("s_vMgrRxBeacon:TxDataRate is %d,Index is %d\n",pMgmt->sNodeDBTable[uNodeIndex].wTxDataRate,uNodeIndex);
		}
#endif
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
                     VNSvOutPortW(pDevice->PortOffset + MAC_REG_BI, pMgmt->wCurrBeaconPeriod);
                     CARDbUpdateTSF(pDevice, pRxPacket->byRxRate, qwTimestamp, qwLocalTSF);
                     CARDvUpdateNextTBTT(pDevice->PortOffset, qwTimestamp, pMgmt->wCurrBeaconPeriod);
                     // Turn off bssid filter to avoid filter others adhoc station which bssid is different.
                     MACvWriteBSSIDAddress(pDevice->PortOffset, pMgmt->abyCurrBSSID);

                     CARDbSetPhyParameter (  pMgmt->pAdapter,
                                            pMgmt->eCurrentPHYMode,
                                            pMgmt->wCurrCapInfo,
                                            pMgmt->byERPContext,
                                            pMgmt->abyCurrSuppRates,
                                            pMgmt->abyCurrExtSuppRates);


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
        CARDbGetCurrentTSF(pDevice->PortOffset, &qwCurrTSF);
        CARDbUpdateTSF(pDevice, pRxPacket->byRxRate, qwTimestamp, pRxPacket->qwLocalTSF);
        CARDbGetCurrentTSF(pDevice->PortOffset, &qwCurrTSF);
        CARDvUpdateNextTBTT(pDevice->PortOffset, qwTimestamp, pMgmt->wCurrBeaconPeriod);
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
    PSMgmtObject        pMgmt = pDevice->pMgmt;
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
    MACvDisableProtectMD(pDevice->PortOffset);

    pDevice->bBarkerPreambleMd = 0;
    MACvDisableBarkerPreambleMd(pDevice->PortOffset);

    // Kyle Test 2003.11.04

    // set HW beacon interval
    if (pMgmt->wIBSSBeaconPeriod == 0)
        pMgmt->wIBSSBeaconPeriod = DEFAULT_IBSS_BI;


    CARDbGetCurrentTSF(pDevice->PortOffset, &qwCurrTSF);
    // clear TSF counter
    VNSvOutPortB(pDevice->PortOffset + MAC_REG_TFTCTL, TFTCTL_TSFCNTRST);
    // enable TSF counter
    VNSvOutPortB(pDevice->PortOffset + MAC_REG_TFTCTL, TFTCTL_TSFCNTREN);

    // set Next TBTT
    CARDvSetFirstNextTBTT(pDevice->PortOffset, pMgmt->wIBSSBeaconPeriod);

    pMgmt->uIBSSChannel = pDevice->uChannel;

    if (pMgmt->uIBSSChannel == 0)
        pMgmt->uIBSSChannel = DEFAULT_IBSS_CHANNEL;


    // set basic rate

    RATEvParseMaxRate((PVOID)pDevice, (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
                      (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates, TRUE,
                      &wMaxBasicRate, &wMaxSuppRate, &wSuppRate,
                      &byTopCCKBasicRate, &byTopOFDMBasicRate);


    if (pMgmt->eConfigMode == WMAC_CONFIG_AP) {
        pMgmt->eCurrMode = WMAC_MODE_ESS_AP;
    }

    if (pMgmt->eConfigMode == WMAC_CONFIG_IBSS_STA) {
        memcpy(pMgmt->abyIBSSDFSOwner, pDevice->abyCurrentNetAddr, 6);
        pMgmt->byIBSSDFSRecovery = 10;
        pMgmt->eCurrMode = WMAC_MODE_IBSS_STA;
    }

    // Adopt pre-configured IBSS vars to current vars
    pMgmt->eCurrState = WMAC_STATE_STARTED;
    pMgmt->wCurrBeaconPeriod = pMgmt->wIBSSBeaconPeriod;
    pMgmt->uCurrChannel = pMgmt->uIBSSChannel;
    pMgmt->wCurrATIMWindow = pMgmt->wIBSSATIMWindow;
    MACvWriteATIMW(pDevice->PortOffset, pMgmt->wCurrATIMWindow);
    pDevice->uCurrRSSI = 0;
    pDevice->byCurrSQ = 0;
    //memcpy(pMgmt->abyDesireSSID,pMgmt->abyAdHocSSID,
                     // ((PWLAN_IE_SSID)pMgmt->abyAdHocSSID)->len + WLAN_IEHDR_LEN);
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

    // Set Capability Info
    pMgmt->wCurrCapInfo = 0;

    if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_ESS(1);
        pMgmt->byDTIMPeriod = DEFAULT_DTIM_PERIOD;
        pMgmt->byDTIMCount = pMgmt->byDTIMPeriod - 1;
    }

    if (pMgmt->eCurrMode == WMAC_MODE_IBSS_STA) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_IBSS(1);
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

//    memcpy(pDevice->abyBSSID, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);

    if (pMgmt->eConfigMode == WMAC_CONFIG_AP) {
        CARDbSetBSSID(pMgmt->pAdapter, pMgmt->abyCurrBSSID, OP_MODE_AP);
    } else {
        CARDbSetBSSID(pMgmt->pAdapter, pMgmt->abyCurrBSSID, OP_MODE_ADHOC);
    }

    CARDbSetPhyParameter(   pMgmt->pAdapter,
                            pMgmt->eCurrentPHYMode,
                            pMgmt->wCurrCapInfo,
                            pMgmt->byERPContext,
                            pMgmt->abyCurrSuppRates,
                            pMgmt->abyCurrExtSuppRates
                            );

    CARDbSetBeaconPeriod(pMgmt->pAdapter, pMgmt->wIBSSBeaconPeriod);
    // set channel and clear NAV
    CARDbSetChannel(pMgmt->pAdapter, pMgmt->uIBSSChannel);
    pMgmt->uCurrChannel = pMgmt->uIBSSChannel;

    if (CARDbIsShortPreamble(pMgmt->pAdapter)) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
    } else {
        pMgmt->wCurrCapInfo &= (~WLAN_SET_CAP_INFO_SHORTPREAMBLE(1));
    }

    if ((pMgmt->b11hEnable == TRUE) &&
        (pMgmt->eCurrentPHYMode == PHY_TYPE_11A)) {
        pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SPECTRUMMNG(1);
    } else {
        pMgmt->wCurrCapInfo &= (~WLAN_SET_CAP_INFO_SPECTRUMMNG(1));
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
    PSMgmtObject    pMgmt = pDevice->pMgmt;
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
                              pMgmt->eConfigPHYMode
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

    // patch for CISCO migration mode
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

            // TODO: deal with if wCapInfo the privacy is on, but station WEP is off
            // TODO: deal with if wCapInfo the PS-Pollable is on.
            pMgmt->wCurrBeaconPeriod = pCurr->wBeaconInterval;
            memset(pMgmt->abyCurrSSID, 0, WLAN_IEHDR_LEN + WLAN_SSID_MAXLEN + 1);
            memcpy(pMgmt->abyCurrBSSID, pCurr->abyBSSID, WLAN_BSSID_LEN);
            memcpy(pMgmt->abyCurrSSID, pCurr->abySSID, WLAN_IEHDR_LEN + WLAN_SSID_MAXLEN + 1);

            pMgmt->eCurrMode = WMAC_MODE_ESS_STA;

            pMgmt->eCurrState = WMAC_STATE_JOINTED;
            // Adopt BSS state in Adapter Device Object
            //pDevice->byOpMode = OP_MODE_INFRASTRUCTURE;
//            memcpy(pDevice->abyBSSID, pCurr->abyBSSID, WLAN_BSSID_LEN);

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

            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Join ESS\n");



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
                if (WPA_SearchRSN(0, WPA_TKIP, pCurr) == FALSE) {
                    // encryption mode error
                    pMgmt->eCurrState = WMAC_STATE_IDLE;
                    return;
                }
            } else if (pDevice->eEncryptionStatus == Ndis802_11Encryption3Enabled) {
                if (WPA_SearchRSN(0, WPA_AESCCMP, pCurr) == FALSE) {
                    // encryption mode error
                    pMgmt->eCurrState = WMAC_STATE_IDLE;
                    return;
                }
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

            pMgmt->wCurrCapInfo = pCurr->wCapInfo;
            pMgmt->wCurrBeaconPeriod = pCurr->wBeaconInterval;
            memset(pMgmt->abyCurrSSID, 0, WLAN_IEHDR_LEN + WLAN_SSID_MAXLEN);
            memcpy(pMgmt->abyCurrBSSID, pCurr->abyBSSID, WLAN_BSSID_LEN);
            memcpy(pMgmt->abyCurrSSID, pCurr->abySSID, WLAN_IEHDR_LEN + WLAN_SSID_MAXLEN);
//          pMgmt->wCurrATIMWindow = pCurr->wATIMWindow;
            MACvWriteATIMW(pDevice->PortOffset, pMgmt->wCurrATIMWindow);
            pMgmt->eCurrMode = WMAC_MODE_IBSS_STA;

            pMgmt->eCurrState = WMAC_STATE_STARTED;
            // Adopt BSS state in Adapter Device Object
            //pDevice->byOpMode = OP_MODE_ADHOC;
//            pDevice->bLinkPass = TRUE;
//            memcpy(pDevice->abyBSSID, pCurr->abyBSSID, WLAN_BSSID_LEN);

            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Join IBSS ok:%02x-%02x-%02x-%02x-%02x-%02x \n",
                  pMgmt->abyCurrBSSID[0],
                  pMgmt->abyCurrBSSID[1],
                  pMgmt->abyCurrBSSID[2],
                  pMgmt->abyCurrBSSID[3],
                  pMgmt->abyCurrBSSID[4],
                  pMgmt->abyCurrBSSID[5]
                );
            // Preamble type auto-switch: if AP can receive short-preamble cap,
            // and if registry setting is short preamble we can turn on too.

            // Prepare beacon
            bMgrPrepareBeaconToSend((HANDLE)pDevice, pMgmt);
        }
        else {
            pMgmt->eCurrState = WMAC_STATE_IDLE;
        };
     };
    return;
}



/*+
 *
 * Routine Description:
 * Set HW to synchronize a specific BSS from known BSS list.
 *
 *
 * Return Value:
 *    PCM_STATUS
 *
-*/
static
VOID
s_vMgrSynchBSS (
    IN PSDevice      pDevice,
    IN UINT          uBSSMode,
    IN PKnownBSS     pCurr,
    OUT PCMD_STATUS  pStatus
    )
{
    CARD_PHY_TYPE   ePhyType = PHY_TYPE_11B;
    PSMgmtObject  pMgmt = pDevice->pMgmt;
//    int     ii;
                                                     //1M,   2M,   5M,   11M,  18M,  24M,  36M,  54M
    BYTE abyCurrSuppRatesG[] = {WLAN_EID_SUPP_RATES, 8, 0x02, 0x04, 0x0B, 0x16, 0x24, 0x30, 0x48, 0x6C};
    BYTE abyCurrExtSuppRatesG[] = {WLAN_EID_EXTSUPP_RATES, 4, 0x0C, 0x12, 0x18, 0x60};
                                                           //6M,   9M,   12M,  48M
    BYTE abyCurrSuppRatesA[] = {WLAN_EID_SUPP_RATES, 8, 0x0C, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6C};
    BYTE abyCurrSuppRatesB[] = {WLAN_EID_SUPP_RATES, 4, 0x02, 0x04, 0x0B, 0x16};


    *pStatus = CMD_STATUS_FAILURE;

    if (s_bCipherMatch(pCurr,
                       pDevice->eEncryptionStatus,
                       &(pMgmt->byCSSPK),
                       &(pMgmt->byCSSGK)) == FALSE) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "s_bCipherMatch Fail .......\n");
        return;
    }

    pMgmt->pCurrBSS = pCurr;

    // if previous mode is IBSS.
    if(pMgmt->eCurrMode == WMAC_MODE_IBSS_STA) {
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_BCNDMACTL, BEACON_READY);
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_TCR, TCR_AUTOBCNTX);
    }

    // Init the BSS informations
    pDevice->bCCK = TRUE;
    pDevice->bProtectMode = FALSE;
    MACvDisableProtectMD(pDevice->PortOffset);
    pDevice->bBarkerPreambleMd = FALSE;
    MACvDisableBarkerPreambleMd(pDevice->PortOffset);
    pDevice->bNonERPPresent = FALSE;
    pDevice->byPreambleType = 0;
    pDevice->wBasicRate = 0;
    // Set Basic Rate
    CARDbAddBasicRate((PVOID)pDevice, RATE_1M);
    // calculate TSF offset
    // TSF Offset = Received Timestamp TSF - Marked Local's TSF
    CARDbUpdateTSF(pDevice, pCurr->byRxRate, pCurr->qwBSSTimestamp, pCurr->qwLocalTSF);

    CARDbSetBeaconPeriod(pDevice, pCurr->wBeaconInterval);

    // set Next TBTT
    // Next TBTT = ((local_current_TSF / beacon_interval) + 1 ) * beacon_interval
    CARDvSetFirstNextTBTT(pDevice->PortOffset, pCurr->wBeaconInt/*
 *);
 Copy// set BSSID CopyMACvWrite*
 * AddressVIA Networking Technologies, aby*
 * hts reseprogReads free software; you can redistribuMgmtt andgies/or modify
 *DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Sync:d.
 e as publ a softw = %02x-, or
 * (=, or
 * (at yo\n", Copyrsiolic License as publ[0]er version.
 *
 * This program1is distributed in the hope that2is distributed in the hope that3is distributed in the hope that4is distributed in the hope that5]hts reseif (ogies, eNetworkTypeInUse == PHY_TYPE_11A) {r versionSE. (lic LiceConfigPHYMod Public License for||r versionGene
 *
 * You should have received a cAUTO)or more detat, wePhy Gen blic License f;r version} elseram; if not, wrreturne Foundatio Copyon, IncSE.  See the
 * GNU General Public License Bor more details.
 *
 * You should have received a copB of the GNU General Public License along
 * with thi11G of the GNU General Public License along
 * with this program; if not, write to the Free SoftwaBe Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Bost more details.
 *
 * You should have received a cop
 * Functions:
 *      nsMgrObjectInitial - Initialize Management Objet data structure
 *      vGe Foundation, IncSE.  ndles the 802.11 management functions
ement Objet data structure
 *      vMgrObjectReset - Reset Management Objet data structure
 PURPOSE. ite to theblic License for more detamemcpy   s_vMgense asSuppRates, &uthenDeginSta - Am is sizeof(rt deauthenticati))e Foundatilic License asExtginSta - [1] = 0e Fountion
 *    ta - Start authenticatiassociate_req*      vMgrDeAuthenDeginSta - Start deauthenticatBon function
 *      s_vMgrRxButhentication - Handle Rcv authentication
 *      s_vMgrRxauthentication sequence 1
 *      s_vMgrRxAuthenSequence_2 Gon function
 *      s_vMgrRxGuthenticatio*      vMgrDeAuthenDe authenticatStart deau authenticatandle Rcv authenticasociation
 * uthentic}
 PURPOSE. WLAN_GET_CAPion; _ESS  See thwCapInfoogram; if notrighbSet*
 *    s_vMgpAdapteribute it and/or m, OP_MODEion;RASTRUCTUREthenticatio// Add curren
 *
  to Candidate list adopt BSS pThis should only  GNUs for WPA2 *
 , andkeProbeRe check must be done before.more details.  s_vMgrAuthenhave recWMACis pH_eProssociate_request   vMAdd_PMKID__MgrMakeBSBegin - Join BSS fic License as publologies, sRSNCapObj.bvMgrRxExisnologies, _vMgrRxProbwvMgrRxthenticatioucture
 *      vMgrAsso   vMgrJoinBSSBegin - Join BSS function
 *      s_vMgrSyADHOCBeacon
 * details.   vMgrJPhyParameter(tion - Han Join BSSm; if not, wrrRxManagePacket - Rcite to ts
 *      vMgrRxManagePacket - Rcv ad_hoc IBSS orent frame dispatch function
 *      s_vMgsERP.byERPent frame dispatch function
 *     MgrDeAuthenDeginSta - Sit- Initial 1-sec and command call back funtion authenticat
 *      vMgrRxManagePacket -) != TRUEssociate_req
 * the Free Software Foundation; ei<----s_btherheateSet Phy have Fail [%d]latev managemthenticatiolin Street, h Floorved.
 channelponseclear NAVframe
 *      s_vC
#inclSBegin - Join BSS functiouband.h")rt aFALS.h"
#include "80211hdr.h"
#include "80211mgr.h"
#include "wmgr.h"
#incband.h"
#include 
#include "wpa.hinclude "bssdb.h"
#inclu
/*framerMak(ii=0;ii<BB_VGAe Soft;ii++or more details.ogies, ldBmMAX< IA Networ----Threshold[ii]ssociate_requestIA NetworbyBBVGANew =es  ------and/------- e Foundatimsglbreakse
 *      vMrgAuthenBeginSs  -----------------!---------*/static giesenth"
#include "80211hdr.h"
#include "80211mgr.h""RSSI#inc NewGaintic FOldtions  --later version*/
//2008-8-4 <a(int)--------------,es  ----------------c BOOL ChannelExce        henticationrintk(  Static Functions  --------------------------*/
//2008-8-4 <add> by chester
static BOOL ChannelExceedZoneType(
    IN PSDevice pDevice,
  BBt (cVGAtion TechnNFO;
//sc BOOL ChannelExceedZBeacon
 *,
    IN BYT-------tic Functions  --------------------------*/
/d> by chester
static BOOL ChannelExceedZoneType(
    IN PSDevice pDe*/CurrCaic Licugies----  St=initions -------e Foun  s_vMgrR      ld have rv managem
s_vMgrRxAssoimerIContext   );

stavMgrTimerIe Foun
 * the Free Software Foundation; either #inctoh"
#inclu=
#include (INTy cheste ---------- frame*pStatuenseCMD_STATUS_SUCCESSRequest(lin Stre};

//mikethe : fix 
 * GNUManager 0.7.0 hidden ssid mave inkePr encryption
//--*/
//2008-8-4 <ad,need red.
 quest - CreaonseeE wListenI   IN 
 static VOID  EncistenI_Rebuild(CurrCIN PSA Netwes  ----evisioS pCKnowneateogies
 )
thentPSic LObjectMgrRxAs = &NFO;
//stasesponsepDev// UINT*/
//2008-8-ii , uSameBpInfNum=0ts resede "pow--------
 *      < MAX_BSS_NUMeAsses  ----------t,
   *      s_vMgsBSSList--- .bActive &&--------*/
//terval,
IS_ETH_ADDRESS_EQUALciation(
    IN PSDevn
 *      ute it and/or mossociate_requestMgmtObjecN PSRxMgmtPack++         msgleions
stawCurrCPacket,
  wCurxDisass(N PSRxMgmtPack>=uest	 //we *     ProbeAP  INwCurrCapIInfoo,
  more details.
 *
 * Youest - Create Associate ReqPSK of en functions
n * GNUm
    IN WORD does not gpDevthe pairwise-key s, InctenIevision HistoryeAssocRequest - Create Associate RequDevi) {evice pDevicso we E_SSID -
static it according  );realpFrame
    );
info     s_Mgrre detail--------bWPAValidrt aard.h" IN P//WPA-PSKPSDevice pDeand command call back equence_2(
   IN PSDevice pDev;
		nSequence_4(
 abyPK Gen[0]ate APA_TKIPssociate_D
s_vMables  ------LAN_IE_SUPP_RATES = Ndis802_11AN_IE_SUPP2Enabled;1(
   evicDevice,
    IN PPRINT_K("  IN PWLAN_IE_SUP--->);

sD pCurc shou  );[e pDev-evicclud)OID
s_vMIN PSDeviD
s_von
 *  RxAuthentication(
    IN PSDAESCCMce pD,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacke3
    );

statiAESMgmtObject pMgmt,
    IN PWhentication(
    IN PSDevice pDevice,
    IN PSMgmtObject pAES,
    Ievice,
    I PSDevice pDeacket   IN PSDevice pDPacket
    );

   I2N PSDevice pDev{,
    I2N PSMgmtObject pMgmt,
    IN PLAN_FR_AUTHEN pFrame
    );

statie,
  IN PSRxMget
    );

// CSSPK
    IN reat11i_CSSDevice pDevicee,
    IN PN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket
    );

staatic
VOID
s_v,
    IN PSMgmtObject pMgmt,
    IN PSRxMgmtPacket pRxPacket
    );

static
VOIe,
 pMgmt,
    IN PSRxMgDevice,
  ,
    IN Packet
    );

// PSMgmtObject pMgmt,
    Iobe request/responponse functions
static
VOID
s_vMgrRxProbeRequest(
    IN PSDevice pDevice,
    IN PSMgmtObject pM_IE_TIM pTIM
    );

static
PSTxMgmtPacket
s_MgrMakeBeacon(
  vMgrRxProbeRespN PSMgmtObevice,
    IN PSMgevice,
    IN PSM PSDevice 
    PSDeviclin Stree *  /*+
 *
 * Routine DescristenI:et
s Format TIM fieldacket
et
s_in St Valuese(
   tes,
ce,
-*/

CurrRa
mt,
 s_vMgr  IN PTIMP_RATES pCuesponse(
  pCurr
    );

s pMgmIE_DevipTIMbeRes)
 more BYTE   msglevyMask[8n
 *{1, 2, 4, 8, 0x10es,
2   IN4   IN80}obeResIN PWLAN_IE_SUPPpobeRestObject pMgmii, jjRATES pOOL   msglevStartFound #includn response
static
PMulticasPSDe
s_MgrMakeRWORDect pMgmwSTxMgIndex
 *      sice,
    IN PEndObject pMgms reserveFinduncti of partial virtual bitmap------------L bReAssocT(ype
NgrSyNUM + 1)
static
VOID
s_vMgRates   )ic LicensPSTxMapint          mSE. !iissociate_request// PP_R outHEN pbroad  IN bit which is indicated separatelys_vMgrRxAuthense(
    IN PS(uppRat&_SUPP_RA0]de "c      suthenSequense(
    INdeauthen functionons
statisNodeDBT   )[0].bRxPSPolic
Pard.         msgle   IN PSDeviceuppRates pMgmt,
    PSDevice SE. Ratesssociate_requesturrExPSTxMgmtPacInfo,
    IN WORD w
PSTxMgmtPacketChannel,
    IN WO IN PSMgmtObject piinel,
    IN WORD wCurrATIMWiCurrCapInfo,N_IE_SUPP_RAh Floor    IN WORDRtPacksTxMget
sex down  );nearest even numberbeResPSMgmtObject&=  ~BITt pRxPaceceived send
staticupD
s_vMgrLogStatus(
    IN PSMurrCapInfo,((   IN PSDeN_IE &t,
   hts reserveScStatuselems
 *payload frameTE p->len =  3 + e      pDev-SMgmtObject )vicets reserveFill  INEN pFixeds,
  satusEN pE pDstAd     pbyDTIMCounject ,
    IN         
s_vMgr           Periocket    pBSSNode,RYPTIOIN NDIS_802_11BitMapCtic
PRD wCurrCap ?PKno_MULTICAST_MASK : 0)  the GNU G(((PSMgmtObject>>ce,
<<ce,
   pbyBITMAPOFFSE   OUThts reserveApp
   vari   )s,
  atusE pDD,
    IN PBYTEPSMgmtObjecatio =0 eAssoc=    IN PSD
statiatiotic
VOID
s_vMgrS_802_11Virt      [jjn
 *,
    IN PWLAN_IE_SUPP_RATEn frameRebuSDev IN Pn't used     s-------------------0] pMgm,
    ITxMgmtPacket
s_MgrMakeAssocResponse(
  Constructs an nc.
 * fgSta( Ad-hoco,
  )evice,
    IN PSMgmtObject pMgPTR  );ent o; or NULL on allocatenI failue    IN WORD wCurWLAN_gmtPacket
s_MgrMakenc.
 *P_RATES pCurrExtSuppRates
    );

satus,
    IN WORD wAssocAIice,
wSuppRormatTIM- As = pDevice->pnc.
 *RYPTIO   int iitObjetSuppRates
    int ii;


    pMATIMWinodw wAssocAID,
    IN of togiesame
     );

sIN PWogiesrame
 PacketPool = &pMgmUPP_RATEOID
s_vs
 *
 * Revisionel = pDevice->uChannel;
   clude "desc.h"
#idr,
    t
    )
{
    bles  --x;
    }=grObjgmt,
  reatFR_BEACONppRatesF
VOIDATES pCurrExtSuppON_INFORand/e
statice so[ES pC0xffes,
mt->abyDesireSSID, 0, WLAN_RATES ;
    
    IN WORD wCbyBuffer
    );

// ReAsso_CTL_NONuLength
    IN PBD,
    INI    DFS----_CTLNONEsocInfo.AssotObject pMgmt,
 Associts reserveprProbramenagement o-------gmt->sAsso(t
    )
{
    )og 802.1by  )
{
    PooD
s_vMgmemMgmt,evice, F, 0function
;

    return; +ocInfongth =o.LeMAXLENrobeRes    Initi->p80211HeaderALSE)UcInfod
vMgrDR)((;
   )Device, FA+es timer object
 *
 * robeRes    etstatheof(NDIS alizesu*      sf(NDIS.pBufALSE)eviceContext
 void
vMgrTimer_802_1MgmtObjCurr,
urn Value:
 *    Ndis_802_1o,
 Encod    pDev&f(NDISpDevice = (PSDevice)hTimer   PSMgmtObjeHdmt,
A3.w(NDIS      cpu_to_le16P_RATE     imerFunctcInfoVOIDFC_FicenrCreaticensMGRTE          condCallBack;S
    pMgmt RUN_AValue:
)imerFunct pDe_LEVEL_INFO;
//stat
    )PShavessociate_reqpMgmt->sTimerSecondCallback|.function = (T(ice,)condCallBackPWRMGT(1 Beacon
 *       s_vMg.data = (ULONG)pDeabye so1,MATION);
    //me,ack.ex,
  _dis_staus.dTimer;
    pDevice->sTimerComma2ociate RequeMACN_AT(HZ);

   #ifdef TxInSleep
    init_timer(&pDevice->sT3// Authrame
 *urn Va
 * ifdef TxInS*MgmtObjeInc.
 * All rigk.function = (T   pMgmt->pbyPSPaerFunction)BSSvSeMgmt;
   pDevice->sTimerTxDIBSS or Device = Copy 
 * This MgmtObje of tLSE)l = &pMgmt->)(MgmtObject  +timer(&pMgmTxDataIimer(&pMgmt+viceDataTrigger = l back funtionsr =  pCurreturn VIEH #ifde TxInSleep
     pDevice->evision HistoryyMgmtPacketPoolMgrRxAuthedif

    pDeviceMgmtPack= CMD_Q_SIZE;
    pDevix = 0;

    rTxDataInSleep =ice)beReed.
SE;
    pDevicinSta - IsTxDataTrigge->uChanne FALSE;
    pDevice->nTxDataTimeCout = 0;
   #endif

    pD*/

VOID
v;
    for(ii=0= CMD_Q_SIZE;
    pDevice->uCmdDequeueIdx = 0
 *
 * Revision Histo;
    for(ii=0;ii<WL
    return;
}



/PSDevice         pDevice = (PSDevice)hDeviceCon pMgmt->eCuTxDataInSlDSs,
 Status_LEVEL_INFO;
//stacRequest(
 to th!blic License for more detaMgmtObjeDSParm*    None.
 *
DS_PARM
vMgrObjectReset(
    IN  HANDLE himeCout = 0;
   #en1 Return V    pDevice->uC Routine Descriptio  IN de,
  ->IsTcInfoEIDrt the ste Foundatithe AP.
 *
 * RetCurr,
1 frame to the AP.
 *
 * ReturSuppRates
    (evicetSuppRates
 "
#include "powDevice pDevisassociation(cRequ Create AssocMgrSy  INASRxMgmtPackgrAssocBegDevisTxDataTriggTIMation association procedure.  Namely, send aS_802_11n Value:
 *    None.
TIM
VOID
vMgrAfo,
    IN WORD WORD apInfo = 0;
pDevice,
  out = 0;
   #eniation requestDevice->nT     pCure Beacon frame
 * vice             pDevice = (PS_CTL_STfor mRxPacket,
  _CTLEnablePSMode = Context;
   _CTLiption:
 *    Star_CTL_he station association procedure.  Namely, send an
 *    2ssociation request frame to the AP.
 == 1) {
    pMgmt->wCurrCapInfo |mt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SCurr,
2apInfo |= WLAN_SET_CAP_INFO_Swmt->pbydo----  pMgmt->pbyMgmP_RATES pCurrEAuthenSequence_2(
    IN PSDevice pNON.h"
#include    IN* RSNEnablePSMo _RATES viceContext;
   RSN WORsTxDataTriggRSN_EXTation association procedure.  Namely, pMgmt->wCurrCapIn    pMgmt->wCurrCapInfo |T_CAWPre FoundatiDbIsShorSlotTime(pMg  IN  H>wListenIntDbIsShorSlotTime(pMgabyOUI
     0x0 pMgmt,
    IN TSLOTTIME(1);
        }on
 * x5lse if (pMgmt->eCurrentPHYMode == PHY_2    } fT_CAP_INFO_SHORTSLOTTIME(1);
        }3    } eHANDLE hDevDbIsShorSlotTime(pMgwVers  IN  HANDLE hDevSHORTSLOTTIME(1);
     e(
    IN}
    } else if (pMgmt->eCurrentPHYMode == 11hEnable TYPE_11B) {
        if (CARDbIsShortPreamb11hEnable Mgmt->pAdapter) == TRUEFALSE;
    // t pMgmt,
    IN PSvMgrRxProbeRequest(
    IN PSDmer(&pDeviNG(1);

    /* build an assocreq framwCurrCap4; pDevice,
    IN Pon
 *     et = s_MgrMakeAssocRequest
                (
    t
    );      pDevice,
                  pMgmt,
               2;N PSMgmtObject pMgmID,
                  pMgmt->wCurrCapInfo,
                1 pMgmt->wListenInterval,
                  (PWLAN_IE_SSID)pMgm1;//WEP40rrSSID,
         mtObject pMgmt,
                pMgmt,
               0;//Mgmtt preamble1(
    Prame
   Key Cipher Suit  if (pTxPackeT_CAP_INFO_SHORTPPK         pMgmt,
    IN RebuuthDevic,
    ,
   acket);
        if*turnmmanFALSE;
    pDevice->nTxDaapInfo |= WInfo |= WLA))et p     pMgmt->wCurrCapInfo |= WLAN+=2 pRxPacket,// receiSN Capabilisc.h"
#include "CPENDING;
            *pStatus = CMD_STATUS_SUCCESS;
        }
    }
    else
        *pStatus = CM;
        if (*pSta;
   #eSTATUS_SUCCESS;
   sociation request frame to  vMrgAuthenBeginSld suppob11h
    )evice pDevice,
    IN1);
    }
   est(
    IN blic License fossociate_reqnSleeuntry IEentication
    pMg   pMgmt =tion association procedure.  Namely,right (cDeviceCIESBegin - Join BSS f
    pMggmt->wCurrCapInfo = 0;
    nfoSBegin - Join BSS ic License ffo |= WLAN_SET_CAP_INF;
    pM #endif

    pCOUNTRY)o |= WLAN_S(
    IN  HANDLE hDeviceContext,PSTxMgmtPa #end_SET_CAP_INFO_PRIVACY(1);
    }

    //if (pDevice- & adopt BSS pPowernitialiainontext;
        //    pPW_CONSTVACY(1);
    }  pMgmt->wCurrCapInfo |PWRAN_SETRAINT    None.
CapInfo |= WLAN_SET_CAP_INFO_SHOR  IN  HANDLE hDevCapInfo |= WLAN_SET_CAP_INFO_SHORTP);
   PENDING) {
  eType == 1) {
association request frame to Info |= WLANassociation request frame to hould suppobSwitchRates
   "card.h"
#include ataInSle
#incluWLAN_Sontext;
     at least one.
CH_SW_CAP_INFO_SHORTPREAMBLE(1);

    if (er) =ITCH    None.
 *
-gmt->pAdapter) == TRUE) {
    Curr,
3LAN_SET_CAP_INFO_SHORTSLOTTIME(1);
        }bypFrame
;
        }
   gmt->pAdapter) == TRUE) {
      Rates
       vMyGseband.h"N
    t - Create ReAssociate RebyNew------------------ARDbIsShortPreamble(pMgmt->pAdapter) ATUS_PENDING) {
      eType == 1) {
3ssociation request frame to HYMode == PHY_TYrCapInfo |= WLAN_SET_CAP_INFO Association TPC reporn - Create    //    pTPC_REP_CAP_INFO_SHORTPREAMBLE(1);

    if (           // at least one.
       pDevice,
     pMgmt->wListenInt                  pDevice,
       Txg) shoul{
       Transmit);
  SBegin - Join BSpDevice,
                    pDevice,
       LinkMargiE(1)d support short preamble.}
    pMgmt->wCurrCapInfo |= Info |= WLAN}
    pMgmt->wCurrCapInfo |= 
    //iDFvice,
    CY(1);
    }
    // a!Device = (PSDevice)hDeviceContRD wC   pMgmt-> //    pMgmt->wDFSVACY(1);
  == TRUE)
         pMgm_SHORTPREAMBLE(1);
    if (pMgDFwListenInte);
        if Curr,
7    None.
 *
-dTimer;

        if abyDFSOwntus
 *      vMgrRxManagePack,
    IN P   pMgmx failed.\n");
        }
      6TPREAMBLE(1);
        if (*DFSRecoveryN_STATUS       pMgmending.\== TRUE)
        pMgmt->wCur7CapInfo |= WLAN_SET_CAP_INFO_SPECTRUMMNG(1);e Description:
 *    Send an dis-for----CB   N_CHANNEL_24G+1eAss<alue:
 *    Non
stati YTE pCurrBSSID,
    
 *              pMMBSS orSBegin - Join BSS ciat |= WLAN_  IN  PBYTE+1h"
#i_SHORTPREAMBLE(1);
 E)
        pMgmt->wCu*    None.
 *
-(PWLAN_IE_SUPP_RATETUS pStatus
    )
{
     DBG_PRT(MSG_e            pDevice =   IN PSDevice PSDevice  PSDevice */

VOID
vMgrR;
    p--------  ExpCY(1);
    }
   PSDevice             pDevG)hDeviceContext;
   ERP *pStatus = cERP      pTxPacket;



    pMgmt->wCurrC*/

VOID
vMgrR1sociation request frame to the AP.
ERPORTPREAMBLE(1);

    if ( PSRxMgmt (PUWLAN_80211HDR  IN  HANDLE hDeviceConte11HDR)(MgmtObject  }
    }
 L_INFO;
//statProtecthave recard.h}
    }
    else
    cture
    sFram|Packet + sizeo_USE_PROTECTIOpMgmt->wCurrCapI  msgleveNonERPPD pC    
    sFrame.len = WLAN_DISASSOC_FR_MAXLEN;

    // format fMgmtRP_PRESEenInterval =re
    vMgrEncBarkerPreambleMDevice pDerame.len = WLAN_DISASSOC_FR_MAXLEN;

    // format fBARKER = (P"
#include "mtObjMode = WMAC_MODE_STANDBY;
 authenticat       ect cketPool;
    memset(authenticat    None.
 *
-*/

VOID
vMgrObjectReset(
    IN  HANDLE hLE hDeviceContext
    )
{
    PSDevice       abyAddr1, abyDestAdociation request frame to mdDequeueIdx = ion - Handle   pDevice->uCmdEnqu
    *(sFrame.pwReason) = cFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN)S pStatus
    N WORD wCurrC// hostapd wpa/wpa2ontext;
ils.
 *
 * You        pDevice = (PSDevice)h&&ommand);
    pDeviH   // sion(&sFrssociate_requhould support short preamble.
    if (pMgmt->eCurrentPHYMod     pMgmt->wWPAIELtAddress, WLAN_ADDRvice,
              fo |= WLAN_SET_CNFO_SHORTPREAMBLE(1);
        if (CARDbIs  // Set reason codeRSNociate RequeUCCESociate ReSUCCESS;
le incoming station */

VOID
vMgrRurn Value:
 *   e incoming stat PSDevice p vMrgAuthenBe/* AdjRespice)l    pMce pDsG) {
    pDevice->pcbMPDUS;
 rReAssocBl
    IN RxPacket,
   PIN PKnUINT uNodeIndex
 -    No  pD,
  3Device PSMgmtObje
{
    WLAutine ne Description:
 *    Allocates and initializes the Prob-responset((HANDLurn Value:
 *    Ndis_staus.
 *
-*/

VOID
vMgrObjectInit(
    IN  HANDLE hDevic

xt
    )
{
    PSDevice  = 0;eR   WORDvice = (PSDevice)hDeviceContext;
    PSMgmtObject    pMgmt = pDevice->pMgmt;
    int ii;


    pMgmt->pbyPSPacketPool = &pMgmt->byPSPacketPool[0];
    pMgmt->pbyMgmtPacketPoo
    pDRUN_AT(PacketPool = &pMgmt->byMgmtPacketPool[0];
    pMgmt->uCurrChannel = pDevice->uChannel;
    for(ii=0;ii<WLAN_BSSID_LEN;ii++) {
        pMgmt->abyD;
    // IN PWbyimer

 esireBSSID[ii] = 0xFF;
    }
    pMgmt->sAssocInfo.AssocInfo.LePROBERESPzeof(NDIS_802_1WLAN_SSID_MAXLEN +1);
    pMgmt->byCSSPK = KEY_CTL_NONE;
    pMgmt->byCSSGK = KEY_CTL_NONE;
    pMgmt->wIBSSBeaconPeriod = DEFAULT_IBSS_BI   )
{
    WLAALSE);

    return;
}

/*+
 *
 * Routine Description:
 *    Initializes timer object
 *
 * Return V_RATES_MA *    Ndis_staus.
 *
-*/

void
vMgrTimerInit(
    IN  HANDLE hDeviceContext
    )
{
    PSDevice     pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject    pMgmt = pDevice->pMgmt;


    init_timer(&pMgmt->sTime>= NODE_AUTH) {
   ;
    pMgmt->sTirrSuppRates[WLndCallback.data = (ULONG)pDevice;
    pMgmt->sTimerSecondCallback.function = (TimerFunction)BSSvSecondCallBack;
    pMgmt->sTimerSecondCallback.expires = RUN_AT(HZ);

    i_RATES_MAmer(&pDevice->mmandTimer;
    pDevice->sTimerCommand.  return;HZ);

   #ifdef TxInSleep
    init_timer(&pDevice->sTimerTxData);
    pDevice->sTimerTxData.data = (ULONG)pDevice;
    pDevice->sTimerTxData.function = (TimerFunction)BSSvSecondTxData;
    pDevice->sTimerTxData.expires = RUN_AT(10*HZ);      //10s callback
    pDevice->fTxD
    PSDeLAN_FR_ASSTATBBHandle Rcv authenticaT(10*HZ);      //1&Device->sTimerComman~  pMgmVOIDIBSS - CrSHORTSLOTTIMErFunnction)vCommannSleep = FALSE;
    pDevice->IsTxDataTrigger = FALSE;
    pDevice->nTxDataTimeCout = 0;
   #endif

    pDevice->cbFreeCmdQueue = CMD_Q_SIZE;
    pDevice->uCmdDequeueIdx = 0;
    pDevice->u*
 * RoutAN_IE_SSID pCuurn;
}



/*+
 *
 * Routine Description:
 *    Reset the maagement object  structure.
 *
 * Return Value:
 *    None.
 *
-*/

VOID
vMgrObjectReset(
    IN  HANDDLE hDeviceContext
    )
{
    PSDevice         pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject        pMgmt = pDevice->pMgmt;

    pMgmt->eCurrMode = WMAC_MODE_STANDBY;
    pMgmt->eCurrState = WMAC_STATE_IDLE;
    ppDevice->bEnablePSMode = FALSE;
    // TODO: timer

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
    OUT PCMD_STATUS
    PSDevice             ULL ){
        /* send the fram
    //if (pDevice->byPreambleType == 1) {
    //    pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
    //}
    pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
    if (pMgmt->wListenInterval == 0)
        pMgmt->wListenInterval = 1;    // at least one.

       s_de = FALSE;
    // TODO: timer

  Mgmt->pbyMgmtPacketPool;
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
        PSMgmtObject pMgmt,
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
        if (CARDbIsShorSlotTime(pMgmt->pAdapter) == TRUE) {
            pMgmt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTSLOTTIME(1);
        }
    } else if (pMgmt->eCurrentPHYMode == PHY_TYPE_11B) {
        if (CARDbIsShortPreamble(pMgmt->pAdapter) == TRUE) {
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
        /* send the frame */
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

    ));

    memcpy( sFrame.pHdr->sA3.abyAddr1, abyDestAddress, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);

    // Set reason code
    *(sFrame.pwReason) = cpu_to_le16(wReason);
    pTxPacket->cbMPDULen = sFrame.len;
    pTxPacket->cbPayloadLen = sFrame.len - WLAN_HDR_ADDR33_LEN;

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
    IN PSMgmtObj parpMgmt,
    IN PSRxMgmtket pRxPacket,
    IN UINT uNodeIndex
    )
{
    WLAN_FR_ASSOCREQ    sFrame;
    CMD_STATUS          Status;
    PSTxMgmtPacket   Description:
 *    Allocates and initializes the associ    INrequLogS               wAssocAID = 0;
    UA ptr
-*/

VOI      uRateLen = WLAN_RATES_MAXLEN;
   t
    )
{
    PSDevice  AUE :R      vice = (PSDevice)hDeviceContext;
    PSMgmtObject    pMgmt =         turn;
    // pDevice->pMgmt;
    int ii;


   IN e* All rig;
    //  node index not found
    if (!uN= pDevice->uChannel;
   sFrame, 0, sizBSSID_LEN;ii++) {
        pMgmt->abyDesireBSSID[ii] = 0xFF;
    }
    pMgmt->sAssocInfo.AssocInfo.LeASSOCREQContext;
 ;
    memset(abyCurrExtSuppRaIEs;
    memset(abyCurrExtSuppRaRS  StxPacket->p80211Header;

    vMgrDecodeAssocRequest(&sFrame);

    if (pMgmt->sNodeDBTable[uNodeIndex].eNodeState     uRat *    Ndis_staus.
 *
-*/

void
vMgrTimerInit(
    IN  HANDLE hDeviceContext
    )
{
    PSDevice     pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject    pMgmt = pDevice->pMgmt;


    init_timer(&pMgmt->sTimeFrame.pExtSuppRateDevice = f IN PSferMatce pD supporContext;

    pMgmt->sTiY_TYPE_11B) {ndCallback.data = (ULONG)pDevice;
    pMgmt->sTimerSecondCallback.function = (TimerFunction)BSSvSecondCallBack;
    pMgmt->sTimerSecondCallback.expires = RUN_AT(HZ);

    i    uRat, set status code
        if (pDevice->eCurrentPHYType N_AT(HZ);

   #ifdef TxInSleep
    init_timer(&pDevice->sTimerTxData);
    pDevice->sTimerTxData.data = (ULONG)pDevice;
    pDevice->sTiate Request frame
 *unction = (TimerFevice = (PSt,
  capi retuyIN PWaconen iAll rigFunctieueIdx =  IBSS or 10s callback
    pDevice->fTxDataIbyTopCCKBas    abyCurrSup,
               able[uNodeIndexts reserved/

VOID
vMpoin   );
   ofParseMaxRateSE;
    pDevice->IsTxDataTrigger = FALSE;
    pDevice->nTxDataTimeCout = 0;
   #e*
 * Rout CMD_Q_SIZE;
    pDevice->uCmdDequeueIdx = 0;
  UPP_RATES)adeDBTable[uNodeIndex].wMaxSuppRa        CurrBeacY_TYPf (p.ble[uNodeIE_11B) ESS;
  pMgmeDBTable[uNodeIndex].wMaxSuppRate;
#iNodeDBTable[uNodeIndex].wTxD TechnataRate);sFranction
NDISIN  _11
     IA fraion; RMhortP_staus.
 = WL----*/


/able[uNodeI{
  WLAN_EID*      vRTPRErintk("RxReAssocRequest:TxDataRate is %d\n",pMg           NodeDBTable[uNodeIndex].wMaxSuppRateement object  structure.
 *
 * Return Value:
 *    None.
 *
-*/

VOID
vMgrObjectReset(
    IN  HANDLE hmtObjele, if ap can't support, set status coBet);
  
           // > 4)LAN_TYPE_Mout = 0;
   #e4, WLAN_BSSID_LEN);

   
    if (pTxP   pMgmt->sNodeDBTa wAssocAID evice)hDeviceContext;
    PSMgmtObject        ps,
         uNodeIndex].wMaxSuppRate > RATE_1           ject  strextenMBLE(ucture.
 *
 *        wAssocStatus = WLAN_MGMT_STATUS_SUCCGSS;
          memcpy( socAID = 0ENDING) {
   _LEN);
    memcpy( sFrame.pHdr->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
    memcpy( s           pDevice->bPr WLAN_BSSID_LEN);

    // Set reason code
    *(sFrame
    memset(&sFrameIndex].bShortPreamble == FALSE) {
      k if node is aNodeDBTable[uNodeIndex].wTxDataRate);
#endife[uNodeIndex].wMaxSuppRate > RATE_11M)
         .pwCapInfo);
odeIndex].bERPExist = TRUE;

        if (pbShortSlotTime =
  Preamble == FALSE) {
        
        RA 802.11h
    PSDevice   mt,
    OUT PCMD_STA more details.nfo |= WcsMg);
  CRatesocInfssociate_request              sFrame.ppStatus = cWLANAeof(STxMgmtPacket) + WLAN_DISASSOC_FR_PREAMBLE(1);
    //}AN_SET_CAP_INFO_SHORTPREAMBLContext;
         sFrameORTPREAMBLE(1);

    if (pMgmtAPABILITY ) ;
        DBG_PRT(MSG_LEVEL_INFO,pMgmt->wListenIntCopyrightGine sFrame  retu  vMgrDeA11 Status
 *      vMgrRxManagePacket - RcvakeR&               sFrameE_11Bin);

 )se reply..
    pTxPacket = s_MgrMakeReAssocResponse
             aListenLAN_TYPE_MGR) nclude "device.h"
#includ IN PBYTE pDstAddr,
    BG_PRT(MSG_Lt   Ch.pHdr->sA3.abyAddr2[3],
            ,
        None.
 *
-*/

CHAddr2[4],
                   sFrame.pHdr->sA3.abyAddr2[{
    Seuthenoreband.h"s // assoc responspMgmt =r->sA3.abyAddr2,
 ocStatus,
       ;

        ild support short preamble.
    if (p of the GNU GexAuthenSequence_2(
    IN PSDevice pDevice,     if (pDevice->bEnableHostapd) {
          Mgmt-TATUS pStatus
    )
{imerTxDaBasir->sA             *N WORIEG) {
       mt->wCurrCapInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
        if (CARShorSlotTime(pMgmt->pAdapter) == TRUE) {
            pMgeAssocBeginSta(
    = 16_LEVEL_DEBUG, KERN_INFO "M      }
    } else if (pMgeCurrentPHYMode == PHY_TYPE_11B) {
       (CARDbIsShortPreamble(pMgmt->pAdapter) == ) {
            pMgmt->wCurrCapInfo |= WLAT_CAP_INFO_SHORTPREAMBLE(1);
        }//GroupDevice, pTxPacket);
       }
    if (pMgmt->b11hEnable == TRUE)
        pM>wCurrCapInfo |= WLAN_SET_CAP_INFO_SPECTRUMMNG(1    /* build an assocreq frame and send it */
            sFN WOGK.pHdKEY_CTL_W  pD.abyAddr2[3],
               pMgmt,
                  if (Statu  PBGtion(ciate function
 *      s_vMgPBYTE   pbyIEs;
   N PSRxMgmtPacke,
                  pMgmt,
             PSDevicENDING ||
         pMgmt->eCurrState == WMAC_STobe requC) {

        sFrame.len = pRxPacket->cbMPDULen;
  probe e Foundation, Inc.,
 * 51 Frank  sFrame.len = pRxPacket->cbMPDULen;
 Mgmts_MgrMakeReAssocRequestt_xmit(pDevice, pTxPacket);
       (*pStatus == CMD_STATUS_PEHANDLE hDeviceConteTime(pMgPK  IN P0t
   
        }
    }
    return;
}


/*+
 *C);
            repe
    )
{
    WLAN_FR_ASSOCRESP   C);
            reWLAN_IE_SSID   pItemSSID;
    PBYTEPte == WMAC_STATE_ASSOC) {

        sFrame.len = pC);
            rePDULen;
        sFrame.pBuf = (PBYTE)pRxPacketo.ResponseFixe;
        // decode the frame
      ;
        pMgmt->sAssocInfonse(&sFrame);
        if ((sFrame.pwCapInfo == 0) ||
 ;
        pMgmt->sAssocInfotatus == 0) ||
               pMgmt->eCurrState = WMAC_STATE_    ifacket          pTxPacket;



    pM + ]
  STATUS_SUCCESS;
        None.
*    if++=CapInfo |= WLAcInfo.Request  }
    }
        pbyIEs =  pMgmt->sAssocInfo.abB) {
       cInfo.Reques_SSID   pItemSSID;
    equence_2(
    IN PSDevice pDevic    // decode cInfo.RequonseFate N PSDSDevice pDevice,
 on
 *      s_vMgr       /* send the frame */
 + 24 +6), pMgmt->sAssocInfo.AssocIEEENodeIXo.ResponseIELength);

   + 24 +6), pMgmt->sAssocInfo.tatus == 0) ||
 LEVEL_DEBUG, KERN_INFO "Mgt:R+=6Es;
       URCES;

    return Length;
        pbyIEs = pMgmt->sAssocInfo.abyIEs }
    else
        *pStatus = CMD_STATUS_*/

VOID
vMgrReAssocBeginSta(
    IN  HANDLE hDeviceContext,// cect  o ble[uNodeI-----OIDuNodeIndex].bShortPreamble =
  entication - Han\n", wAssocAID);
        DBG_PRT(MSG_LEVeAssocBeginSta(
    IN  HANDLE hDeviceContext,.2X:%2.2X:%2.2X  DBG_PORT80(0STATE_ASSOC;
   |= WLAN_SET_CAP_INFO_SHORTPREAMBLrtSlotTimeeAssocBeginSta(
    IN  HANDLE hDevice    s_vMgrRxAuth NULL ){
        /* send the frame */2 of the GNU GenerAuthenSequence_3(
    IN PSDevice pDevice,
    ice,
    IN PSM;
        if (Status != CMD_STATUS_PENDInPeriod = DEFAULT_IBSS_BID,
    Iice,
    IN         wkeReA16((*(sFrame.p {
   1) )
            {
 utine Description:(AP function)
 *    Handle incoming  DBG_PORT8Mgmt->pAdapter) == TRUE) {
 b_tailroom(pDevice->skbCurr,
6; //REAMBLE(2)+GK(4ORD)uNodeIndex;

e->skbD
s_vMgrRxAssocResponse(
    IN PSDevice pDevice,
    IN PSMgmtObj\n");RSN PSRxMgmtPacket pRxPacket,
    estIELengTYPE_110F //data room not enough
      Mgmt->pACSID   pItemSSID;
    PBYTE   pbyIEs;
    viawget_wpa_header *wpahdr;

estIELengCurrState == WMAC_STATE_YTE  ENDING ||
         pMgmt->eCurrState == WMAC_STATE_ASSOC) {

        sFrame.le             w pMgmt,
    IN PS   sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
        // decode the frame
               wpahdr->resp_(&sFrame);
        if ((sFrame.pwCapInfo == 0)              wpahdr->resp_UNKNOW s_MgrMakeReA            (sFrame.pwAid == 0) ||
            (sFrame.pSestIELeng4    
            DBG_PORT8estIELeng5n
 *      spDevice->skb->data + siz6h)) {    //data room not enough
      7              dev_kfree_skb(pDevice->sTES p		   pDevice->skb = dev_alloc_o.ResponseFixedIEs.StatusCode = *(sFrame.pwStestIELeng9    wpahdr->resp_ie_len = pMgmt->sAssocInfo.AssocInfo.onId = *(sFrame.pwAid);
        pMgmt->sAssocwget_wpa_header) + wpahdr->(&sFrame);
        if>req_ie_len);
                Mgmt->eCurrentPHYModpDevice->wpadev;
		skb_reset_mac_headeixedGROUsocInfo.RequestIELength;
                memcpy(pDevice->skb->protocol zeof(viawget_wpa_hr)+pMgmt->sAssocInfo.Asso+cInfheader), pMgm   pMgmt->eCurrState = WMAC_STATE_om not enough
       
    memcpy(pDevice->skb->data + siz1on
 *      sa room not enough
       Mgmt->p   //data room not enough
       wCurrCap       dev_kfree_skb(pDevice->s1     		   pDevice->skb = dev_ace_3(
    IN PSDevice pDevice,
  ce->skb->dev = pDevice->wpadev;
		s1eof(v pMgmt,
 AKM->wCnfo.ResponseI;

        // save values and set current BSSuest frame
 *    0, 512);

		len = pMgmt->sAssocInfo.Assoce.pwStatus))) == estIELength;
                memcpy(pDevgmt->sAssocInfo.Associce->skb->cb));
                netif_rx(pDevie16((*(sFrame.pwAid)));
       D   pItemSSID;
     WMAC_STAT_vMgrRxProbeResponse - INFO_SHORTPREAMBLE(1);
 *      ndCallb enough
       6], &nfo.AssocInfo.ResponseIELengponse
 , 2ocStatus,
  th = len;
			we_event = IWEVASSOCREQIE;
	q_ie_l }
    }
    else
       OCREQIE;
	,
    o.ResponseIELength);     netif_rx(pDevi CMD_STATUS_        wAssocgskeReA.on = f (p1hEnabotect);
    if (*pSRoamN pF PCMD_STATUkeAssocRequest - Create Associate RequeuppRates
    );

//PADEkeReA= TRUE)
        PADEVU);
			memset(&wrqu, 0,8          msgleADiffCo      mman    if LE(1);      /PORT
/cevic 24 +6), pMgmt-PPORT
//E0
    Ireamble
   ni   IcStaAdd> by Einsn Liu
		}
  	}
#ende    then functions
s409-07, <Add> bacon - Create,
    IN PBYTE pDstAdd_data, &pMgmt->abyCurrBSSID[0],
static
VOID
s_vMgrR.ap_addr.s !*   mp(&_data, &pMgmt->abyCurrBSSIPSDevrogram isable[uNodeIndex].wSuppUpMgmEUS    ifdefIN  WORD    wReason,
   (      el) AuthenSequence_1(
 eCurrState = WMuest f    pMgmt->eCurrState = WMAC_SkeReA, 1RN_INFO "Mgt:Rea      pMgmt->eCurrssoc responsSTxMgmtPacket      pTxPacket = Ns = CMD_      els   };

    return;
}



     netif_rx(pDevic5]
  *(sFrame.p*d clear flags relPacket = NUL              neD
vMgrReAssocBeginAID from AP, has two msb clear.\n");
            };
            DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Association Successful, AID=%d.\n", pMgmt->wCurrAID & ~(BIT1ble == FALSE) {
            pDevice->bB WMAC_STATE_ASSOC;
STATE_ASSOC;
ateAPNode((HANDLE)pDevice, sFrame.pwCapInfo, sFrame.pSble == FALSE) {
                    pMgmt->sNodeDBTable[uNodeIndex].wListenInterval = cpu_to_le16(*sFrame.pwListenInterval);
        pMgmt->sNodeDBTable[uNodeIndex].batus;
    PSTxMgmtPacket       WLAN_GET_FC_PWRMGT(sFrame.pHdr->sA3.wFrameCtl) ? Tre-RUE : FALSE;
        // Todo: check sta basic rate, if ap can't support, set status code

        if (pDevice->eCurrentPHYType == PHRHY_TYPE_11B) {
            uRateLen = WLAN_RATES_MAXLEN_11B;
        }

        abyCurrSuppRates[0] = WLAN_EID_SUPP_RATES;
        abyCurrSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pSuppRates,
                                         (PWLAN_IE_SUPP_RATES)abyCurrSuppRates,
                                     RE    uRateL);
        abyCurrExtSuppRates[0] = WLAN_EID_EXTSUPP_RATES;
        if (pDevice->eCurrentPHYType == PHY_TYPE_11G) {
            abyCurrExtSuppR
{
    WLAalizes timer object
 *
 * Return V.pHdr->sA3 *    Ndis_staus.
 *
-*/

void
vMgrTimerInit(
    IN  HANDLE hDeviceContext
    )
{
    PSDevice     pDevice * (PSDevice)hDeviceContext;
 G) {
   MgmtObject    pMgmt = pDevice->pMgmt;


    init_timer(&pMgmt->sTimeSID_LEN);
    if (pMsA3.abyAddr2[TEvParseMaxRate((PVOID)pDevice,
              ReRUE :       (PWLAN_IE_SKEY);
    else
   evice;uthAlgorithm) = TimerSecondCallback.function = (TimerFunction)BSSvSecondCallBack;
    pMgmt->sTimerSecondCallback.expires = RUN_AT(HZ);

    i.pHdr->sA3                   &(pMgmt->sNodeDBTable[uNodeIndex].wMaxBasicRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].wSuppRate),
                    e      &(pMgmt->sNodeDBTable[uNodeInG) {
   byTopCCKBasicRate),
                           &(pMgmt->sNodeDBTable[uNodeIndex].byTopOFDMBasicRate)
                mdDequeueIdx = e so pMgmPTable[uNodeIndex].wSuppRate),
           AP or ject  str of t_RATES /*        );

        // set max tx rate
uthAlgorithm) = ->sNodeDBTable[uNodeIndex].wTxDataRate =
                pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate;
#ifdef	PLICE_DEBUG
	printk("RxReAssocRequest:TxDataRate is %d\n",pMgmt->sNodeDBTable[uNodeIndex].wTxDataRate);
#endif
		// Todo: check sta preamble, if ap can't support, set status code
        pMgmt->sNodeDBTable[uNodeIndex].bShortPreamble =
                WLAN_GET_CAP_INFO_SHORTPREAMBLE(*sFrame.pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].bShortSlotTime =
                WLAN_GET_CAP_INFO_SHice        ucture.
pDevice = (PSDevice)hDeviceContext;
      pDevice  Return Value:
 *    None.
 *
-*/

VOID
vMgrObjectReset(
    IN  HANDLE hDeviceContext
NodeIndex].wMaxSuppRate > RATE_11M)
           pMgmt->sNodeDBTable[uNodeIndex].bERPExist = TRUE;

        if (pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate <=et = (PSTxMgmtPacket)pMgmt->pbyMgmtPackin
            pDevice->bProtectMode = TRUE;
            pDevice->bNonERPPresent = TRUE;
        }
        if (pMgmt->sNodeDBTable[uNodeIndex].bShortPreamble == FALSE) {
            pDevice->bBarkerPreambleMd = TRUE;
        }

        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Rx ReAssociate AID= %d \n", wAssocAID);
        DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "MAC=%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X \n",
                   sFrame.pHdr->sA3.abyAddr2[0],
                   sFrame.pHdr->sA3.abyt != NULL ){
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
        memcpy(pbyIEs, (sFrame.pBuf + 24 +6), pMgmt->sAssocInfo.AssocInfo.ResponseI;

        // save values and set current BSS state
        if (cpu_to_le16((*(sFrame.pwStatus))) == n, Inc.,
 * 51 Frank            // set AID
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
            pDevice->uBBVGADiffCount = 0;
   G) {
            DBG_PRT(MSG_LEEVUp) && (pDevice->skb != NULL)) {
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
	//if(pDevice->bWPADevEnable == TRUE)
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
			wireless_send_event(pDevice->dev, we_event, &wrqu, buf);
		}

		memset(buf, 0, 512);
		len = pMgmt->sAssocInfo.AssocInfo.ResponseIELength;

		if(len)	{
			memcpy(buf, pbyIEs, len);
			memset(&wrqu, 0, sizeof (wrqu));
			wrqu.data.length = len;
			we_event = IWEVASSOCRESPIE;
			wireless_send_event(pDevice->dev, we_event, &wrqu, buf);
		}


  memset(&wrqu, 0, sizeof (wrqu));
	memcpy(wrqu.ap_addr.sa_data, &pMgmt->abyCurrBSSID[0], ETH_ALEN);
        wrqu.ap_addr.sa_family = ARPHRD_ETHER;
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

#ifdef WPA_SUPPLICANT_DRIVER_WEXT_SUPPORT
//need clear flags related to Networkmanager

              pDevice->bwextcount = 0;
              pDevice->bWPASuppWextEnabled = FALSE;
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
    ect pMgmt,
    IN PSRxMgmtPacket pRxPacket,
    IN UINT uNodeIndex
    )
{
    WLAN_FR_ASSOCREQ    sFrame;
    CMD_STATUS          Status;
    PSTxMgmtPacket   LAN_GET_FC_PWRMGT(sFrame.pHdr->sA3.wFrameCtl) ? TRUE :
    WORD                wAssocAID = 0;
    UINT                uRateLen = WLAN_RATES_MAXLEN;
   e->eCurrentPHYType == PHY_TYPE_ates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];
    BYTE                abyCurrExtSuppRates[WLAN_IEHDY_TYP   IN 
                   AcketPool[0];
    p return;
    //  node inde->uChannel;
    for(ii=0;ii<WLAN_BSSID_LEN;ii++) {
        pMgmt->abyDesireBSSID[ii] = 0xFF;
    }
    pMgmt->sAssocInfo.AssocInfo.Le    uRaMAXLEN + 1);
pDevice->eCurrentPHYType == PHY_TYPE_11G) {
            abyCurrExtSuppRates[1] = RATEuSetIE((PWLAN_IE_SUPP_RATES)sFrame.pExtSuppRates,
                                                (PWLAN_IE_SUPP_RATES)abyCurrExtSuppRates,
                                  Algorithm) = cpu_to_le16(WLAN_AUTH_ALG_OPENSYSTEM);

    *(sFrame.pwAuthSequence) o_le16(*sFrame.pwListenInterert values
   PWLAN_IE_SUPP_RATES)abyCurrSuppRates,
                          (PWLAN_IE_SUPP_RATES)abyCurrExtSuppRates,
                           FALSE, // do not change our basic rate
       rt, set status code
        if (pDevice->eCurrentPHYType == PHY_TYPE_11B) {
            uRateLen = WLAN_RATES_MAXLEN_11B;
        }
        abyCurrSuppRates[0] = WLAN_EID_SUPP_RATES;
        abyCle[uNodeIndex].wSuppRate),
               T(10*HZ);      //10s callback
    pDevice->fTxDataIion)BSSvSe   IN PSDDevice->sTime       WLAN        pTxPacket Functvice->sTimerComman.len;
 AID | BIT14N_HDR_A5ce->sTime object  structure.
 *
 * Return Value:
 *    None.
 *
-*/

VOID
vMgrObjectReset(
    IN  HANDLE hDeviceContext
    )
{
    PSDevice         pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject        pMgmt = pDevice->pMgmt;

    pMgmt->eCurrMode = WMAC_MODE_STANDBY;
    pMgmt->eCurrState = WMAC_STATE_IDLE;
    p            wAssocAID = 0;
    UINT                uRateLen = WLAN_RATES_MAXLEN;
    BYTE                abyCurrSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];
    BYTE                abyCurrExtSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];

    if (pMgmt->eCurrMode != WMAC_MODE_ESS_AP)
        return;
    //  node index not found
    if (!uNodeIndex)
        return;
    //check if node is authmt->sNodeDBTable[uNodeIndex].wListenInterval = cpu_to_le16(*sFrame.pwListenInterval);
        pMgmt->sNodeDBTable[uNodeIndex].bPSEnable =
                ->pbyMgmtPacketPool;
    memset(pTxPacket, 0, sizeoAUTHEN_FR_MAXLEN;
                // format buffer structure
                vMgrEncodeAuthen(&sFrame);
                // ins
    sFraues
                sFrame.pHdr->sA3.wFrameCtl = cpu_to_le16(
                     (
                     WLAN_SET_FC_FTYPE(WLAN_TYPE_MGR) |
                     WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN)|
                     WLAN_SET_FC_ISWEP(1)
                     ));
                memcpy( sFrame.pHdr->sA3);
       1, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);
                memcpy( sFrame.pHdr->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
                memcpy( sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);
                *(sFrame.pwAuthAlgorithm) = *(pFrame->pwAuthAlgorithm);
                *(sFrame.pwAuthSequence) = cpu_to_le16(3);
                *(sFrame.pwStatusMPDULen =_to_le16(WLAN_MGMT_STATUS_SUCCESS);
                sFrame.pChallenge = (PWLAN_IE_CHALLENGE)(sFrame.pBuf + sFrame.len);
                sFrame.len += WLAN_CHALLENGE_IE_LEN;
      3);
        sFrame.pChallenge->byElementID = WLAN_EID_CHALLENGE;
                sFrame.pChallenge->len = WLAN_CHALLENGE_LEN;
                memcpy( sFrame.pChallenge->abyChallenge, pFrame->pChallenge->abyChallenge, WLAN_CHALLENGE_LEN);
                // Adjust the length fields
                pTxPacket->cbMPDULen = sFrame.len;
                pTxPacket->cbPayloadLen = sFrame.len - WLAN_HDR_ADDR3_LEN;
                // send the frame
                if (csMgmt_xmit(pDevice, pTxPacket) != CMD_STATUS_PENDING) {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Auth_reply sequence_2 tx failed.\n");
             vice->pMgmt;

    pMgmt->eCurcket->cbMPDULen = sFrame.len;  pTxPacket->cbPayloadLen = sFrame.len - WLAN_HDR       }
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
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt: rx auth.seq = 2 unknown AHandles prSupce,
 WORD  pMgmt,
   ent os.          wAssocAID = 0;
    Unon      IN WORD wCurrCapInfo,
 RxrrSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];
    BYTE             PSR 0xFF;
    }pRgmt->sAesireBSSID[ii]tatic
VOIelated to N    IN WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1);
    1_ASSOCIATION_INFORMN  PSMgmtObject  = cpu_toeCurRxOUT PCMD_STATERPnse(
   {

        PSRxMgmtame.len;
    pTxPacketIERates
     memsetonse
static
 pTxPacke    if HiengtChanneCommandTiMgmtndCallbalizes timerN + WLAN_RATES_MA pDevice = de>sTit,
  ((HANDLE)p       );

en = sFrame.l   IN UINaTimeCout = 0ect    pMgmt = = sFrame.lMgmt;


    init_to,
 DLEVELval);
        pMgmt->sNo PSMgmtObjled.\n")qwTimestame.pHdYTE            on)BSSvSecondTxData;
    4
 *
 *
 * Return Value:
      //104
 *
 *
 * Return Value:
ce->Ise_4(
    IN PSDevice pDeviemcpy( sFr4
 *h"
#include "80211hdr.h"
#include "80211mgr.h"
#rrSupsA3.a:d.h"
he L:[%p------n = sFrame.lid
vMgrTimerocInfo.Asso
 * tORT80(0xCre Beacoe "bssdb.h"
#inclming autheueIdx = 0;
 o.Assoc4
 *Pool;
  
 * the Free Software Foundation; eiRx = 0;ame->pwHEN)
 Curr,
0----oming authentine Descriptiondress, WLAN_ADDR,
    SuppRates
  > lue:
 *    None.
 uppRates
    );

//"
#incluremappN pFraD wCurrATIMWinoleHostapd) {{
            pMM
     SBegin - Join BSS iceContext,
    IN  PSMgmtObjeionEnable) {
 
			we_event = IWEVASSOCRESPIE;
us(pMgmt, cpu_iceContext,
    IN  PSMgmtObje IN PBYTE pDstAddr,
    INSuppRates
  !=e == WLAN_AUuppRates
    );

//at->sNo"
#incluOID
s bcs pMgrcvice,cciat"
#inclupak    .h"
#include "if (csMgmt_xmi
s_MgrMakeR pTxPacket->cbPayloadLenus(pMgmt, ct data structure
 *      vMgrAsso// no->bE
//        sES pCurrSuf (csMgmt_xmit(pDev/



/*-/2008-0730-01<Add>by MikeLiu
if(    if ExceedZone Gent,
    INock);
//     )==PE(WLAN_TYPElin Streommand, 0);
    }(pTx != CMD_Mode = TRUE;MgrTimerITHENTICATE_cture
    sFraT1) )
     MgrTiERPif(len)UPP_RATES pndex].eNodeState s;
    viawget_wpption:
 *   HanIndex = 0;
//: check sA3.abyAddupakeBeor inserresponssndex].w the lengtBSS  wReIsIc
VO IN ((HANDLE),
    IN NG)pDevice;
    pDevice->sTice pDevice,Index;
     leaving       uNodeBSSbU an ATo     sFrame.len = pRxPac>wCurrCapInfo,
              ion)BSSvSith sequenct, pRxPacket->p80211Header->sA3.abyAddrInc.
 * All rigdex)) {
            BSSvRemoveOneNode(pDMgmt;
    intSG_LEVEL_DEBUG, KERN_INFe = WMAC_STATERT(MSG_LEVEL_DEBUG, KERN_INFO f (csMgmt_, sta not found\n");
        }ueIdx = 0;
    pDevice->uCm>eCurrMode == WMAC_MODE_
 *
 * Revision History:
 *
 */

#inclson code
    *(sFrame.pwReason) = ct = s_MgrMakeReAs;
 en;
        sFrame.pBuf = (PBYTE)pRxPauthetion(&sFrame);
        DBG_PRT(MSG_LEVEL    n;
        sFrame.pBuf = (PBYTE)pRxPaapteeviceCson=%d.\n", cpu_to_le16(*(sFrame.pwReasonQuie else if (pMgmt->eCurrMode == W leavingson=%d.\n", cpu_to_le16(*(sFrame.pwRe
    CMD_STATUS         son=%d.\n", cpu_to_le16(*(sFrame.pwReaTimerSe4merComma4,
    BSIN PKnatus,robA3.abyAd>wCurrCapInfo,
              ame.len == cpu_to_le16
                  wAssocStatu      , Inc.,
 * 51 FEL_DEBUG;

/*---------------------   timer_ex/      :  - WLAN_H = : %dAN_MGcket pRxPacketice,
    INSSbI     (BSSDBbIsSTAInNodeDB(pMgmt, pRxPacket->p80211Header->sANG)pDevice;
    pDevice->s, pRxPacket->p80211Header->sA3.abyAddr2, &uNodeIndex)) {
            BSSvRemoveOneNode(pDevice, uNodeIndex);
        }
        else {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Rx disassoc, sta not found\n");
        }MAC_MODE_ESS_STA ){
        sFrame.len = pRxPacket->cbMPDULen;
        sFrame.pBuf = (PBYTE)pRxPacket->p80211Header;
        vMgrDecodeDisassociation(&sFrame);
        DBG_PRT(MSG_LEVEL_NOTICE, KERN_INFO "AP disassociated me, reason=%d.\n", cpu_to_le16(*(sFrame.pwReason)));
        //TODO: do something let upper layer know or
        //try to send assity timeout
      //  if (pMgmt->eCurrState == WMAC_STATE_ASSOC) {
       //     vMgrReAssocBeginSta((PSBSSLisce, pMgmt, &CmdStatus);
      //  };
        if ((pDevice->bWPADEVUp) && (pDevice->skb LAN_FR_DI
/*--Packet
s_MgrMakeAssocRespons(AP)or(ect.
 *e shcpy( sFrame.pHdr->sA3      r3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);
    *(sFrame.pwAuthAAlgorithm) = *(pFrame->pwAut11B) {
            uRateLen = WLAN_RATES_MAXLEN_11B;
        }

   rame.pwStatus) = cpu_to_le16(uStatusN + WLAN_RATES_teLen);
        aevice pDevpDevice->b   IN ;
    m = 0xFF;
    }
    pMgmt->sA = sFrame.len;
    pTxPacketsupport,                        TA  INect.
 *
 * : when la - t 003 rBSSListt        succesN_SET_FCurrModhaUTHEame
.abyAddtcon ine Des     s  *pStatus = csMgmt_xmit(pDevice, pTxPacket *
 * Return1);
    }
    // always allow receive sho&&es  -------nc.
 *S    hort preambleTxPacket) != CMD_STATUS_PENDING) {
      QuthenticatioMSG_LEVEL_DEBUG, KERN_INN_INFO "Mgt:Authreq_reply sequence_4 tx fai failed.\n");
    }
    return;

}



/*+
 *
 * Routinutine Description:
 *     (PWLAN_IE_S------- pFrame
    )
{

    if ( cpu_to_le16((*(pFrame-      rx:MACus))) , or
 * (at your option) any -------------*/
//2008-8 init_timer(&pDevice->sTim is distribu_STATE_ASSOC) {
       //         sF it will be (PBYTE)pRxPacket->p80211Header;
    UT ANY WARRA(PBYTE)pRxPacket->p80211Header;
    e implied wa(PBYTE)pRxPacket->p80211Header;
    TABILITY or (PBYTE)pRxPacket->p80211Header;
    5]ear flags relatedUPP_RATES mmand, 0);
    }uccessful.   };

    return;
}cation case
            indif

    pDevice->cbFreeCmdQueue = CMD_->wCurrCapInfo,
  lin Street, Fifts = CMD_       on case
      aby_ESS_STA ){
        sFrame.lendif

    pDevice->cbFreeCmdQueue = C                 pMgmt->sNodeDBTable[0].bActive = FALSE;
                Device->bWPASuppWextEnablKERN_INFO "802.11 AWMAC_STATE_ASSOC)
      timerthen frames w   pDevice->bProt4ice,
ason code
    *(sFrams != CMD_STATUS_PENDIheader *wpahdr;


    if (ociate functheader), pMgmtdr->sA3.abyAddreply.     s_MgrDevice, FALS    abyCurrSuppRates[Ws(pMgmt,cpu_to_le16((*>wCurrCapInfo,
        ppRates
    )t pMgmt,
    IN PWLAN_F          skb_put(pDevicurn Valu pDevice->fSG;
                 wpahdr->resp_iegmt->pbyPSPacketPot pMgmt,
    IN PWLAN_FR_gmt->byPSPacketPoot_wpa_header));
  0          skb_put(pDevic init_timer(&pDevice->sTim>wCurrCapInfo,
        dif

    pDevice->cbFreeCmdQueue 
		 skb_reset_mac_header(peviceCate Request frame
 		 skb_reset_mac_header(pDevice->sPSDevice    back funtions
 *
 * Revision History:
 *
 */
col = htons(ETH_P_802_2);
         emset(&sFrame, 0, EL_DEBUG, KERN_INFO "N_FR_ASSOCRE            wAssocStatus,
 SE.  evice, FA != CMD )eCurrentPHYMode =s
   DEBUG, KEG) {
        pMg->cbMPDULee,
  _xmimt,
    IN Pevice, F  ) ;
        DSE.    IN P!SDevice pDeviPENDINAuthen (SHAREDKEY     pMgmt->eCurrState = WMAC_STATE_AUTHMgt:& (pDevice->skbtxN  HAe    ate == WMAC_STATE_ASSOC)
skb != NULLterval,
    IN PWL(&wrqu, 0, sizeof (wrqu));
        wrqu.ap_addr.sa_family  };
ing..RD_ETHER;
	PRINT_K("wireless if (pTxPackemtObject )pMgmt->pbyMgmtPacketPool;
    memset(pe(
  EiceCo      r2[1R) |
ecestenIIN PWhFramN pFof1],
   cription:
 e(
  abyCurr ce  s a datusmin    IN   IN Pifdef t    onsest - callse(
  ice)appropricturfunic
VOdles incoming deauthentication frames
 *
 *
 *mt,
 pFrame,
    {
    P_RATES p me.len hA NetwMgmtObjhSequence) = cpu_to_le16(4);
    *(sFrame.pwStatus) = cpu_to_le16eBSSID[ii] rrExtSu      wpahALSE);A Netw)*             n response
static
PInScauthr
s_MgrMakeRtObject pMgmuonPeObject pMgmt,
   IN e pDE  eonPe   I  IN memset
{
    WLAN_HEN    sable[uNodeIndex].byTopOFDMBpDevice = (PSDevice)hDeviceContSE.   pMBbIsSTAInonPeri    if (eturn;

}



/*+
 *
 * (&pDevice->sTime&  switch(pWORD)uNodeIskb ! 	case 0x00:CurrBeaconPeriod,
    switch(p].13
	case 0 if node is asLAN_S(IN PSReOwns = RUN_AT( = TRUE;
	         break;
	candCallbac))buf_Pool;
   caseIN PSRx rate
        :ING) {
         DeviceCl  br= 2   // decode 
 * the Free Software Foundation; eirxTRUE :reqRD_ETHER;
	PRINT_KAuthe 	case 0x0<yZoneTAsso_data  wrqu;
	memsetrved.nd dea  pMnotif_MgrenIning beacon frames.
d\n"LE(1)(6) class 2be imiveMaxrom(sFr* Retsta*    None.
 *
-*/e Descuest -BegiP_RAt,
    INte = WMAC_STATE_IDLE;
   VIAWGET_DISASSOC_MSG;
                 wpa IN BOOL bInScan= TRUE;
	         break;
	case 0x01: )
{

    PKnownBSS           pBSSList;(6                pDevice,
               &_RATES p               bIsSSIDEqual = FALSE;clear flags related
 * the Free Software Foundation; eiwmgr:
 *
 *mtObject pMgmt,
     1RD_ETHER;
	PRINT_K("wireless_send_event  sFrame.len = pR(pFrameY_TYPE_11B) {,
    IN Pexceed = TRUE;
,
  switch(pETHER;
	PRINT_K("wireless_senvel    ;
	         break;
	default:     SP              //reserve for other zonetype
		break;
  }

  return exceed;
}


/*+
 *
 * Rouspe = FALSE;
    BOOL   BOOL        ates[WL  bIsAPBeacon = FALSE;
    nclude    None.
 *
-
    WORD                wAIDNumber = 0;
    UIN2RD_ETHER;
	PRINT_KALSE;
    UINT                uLo.pHdr->sA3              //reserve for other zonetype
		break;
  }

  return exceed;
}


/*+
 *d\n", coutine Description:
 est
odo:sERP;
  escription:
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
    BOOL                bTSFOffsetPostiv wStartel,
    IN WORD wCurrATIMWi   BOOL 
    sFrame.pBu  bIsAPBeacon = FALSE;
    BOOL                bIsCh   WORD                wAIDIndex = 0;
    eIndex;
    BYTE                byTIMBitOn = 0;
    WORD                wAIDNumber = 0ERP;
    sp                uNodeIndex;
    QWORD               qwTimestamp, qward.hNULL) {
        if (byCurrChannel > CB_MAX_CHAN->sA3.ab              //reserve for oth           );
//break;
  }

  return exceed;
}


/*+
 *Hdr->outine Description:
 e:
 *    None.
 *
-*/  bIsAPBeacon = FALSE;
       byIEChannel = sFrame.pDSParms->byCurrChannel;
    eIndex;
    BYTE                b           );
{
            // adjust channel info. bcs we r, sFramecent channel pakckets
      WORD               qwTimestamp,      byIEChannel = sFrame.pDSParms->byCurrChannength =   }
        if (byCurrChannel != byIEChannel) {
            // adjust channel info. bBSSLisine Description:
 *   Reason)     se 0x0ULL ){
 NO_SCANNreq_data  wrqu;
	memsetel
    )
{Channel,
    IN WORjcent channel pakckets   pDev  bIsAPBeacon = FALSE;
    el
         byIEChannel = sFrame.pDSParms->byCurrChannemt->              //reserve for oth1F;
    QWORD               qwCurrTSF;
    WORD       timwStartIndex = 0;
    WORD                wAIDIndex DIS    u              //reserve for other zonetype
		break;
  }

  return exceed;
}


/*+
 *disRUE :ine Description:
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
    BOOL                bTSFOffsetPostiv3RD_ETHER;
	PRINT_K("wireless_sen   BOOL DonInter FALSE bChannelHit = FALSE;
            byCurrChannel = byIEChannel;
        }
 AssoEyERP = sFrame.pERP->byContext;
 hannel);
        BSSbInsertToBSSList((HANDLE)pDev*+
 *
est -                uNodeIndex;est -    
                            (HANDLE)pRxPacket
                           );
 DE   }
    else {
//        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"update bcn: RxChannel = :wBeac * ReyCurrChannel);
        BSSbUD,
     oBSSList((HANDLE)pDevice,
                            *sFrame.pqwdefault                                     *sFrame.pwCapInfunkatic mgmtRD_ETHER;
      /* else, ignoreit.  TODO: IBSS authentication servie(
  PSvClearBSSListto
 *
 ce,
    IN PSMgmtObject pMgard. *  // if i;wLocal *  = ARPHme.pwAuthonse
bMgWLANCleanc.
 *ToSenPP_RATES palue:
 *               True:exceed;
 *          esireBSSID[ii] ceedZoneTy           wpah IN PSDevice pDevice,
    IN BYTEe;
    UINT        uNodeIndex =t--->SIader;
        ifBufunde\n")
s_MgrMakeRrCommand);
    pwListenI    OUT||N,
        
    )d
vMxuf_sz);
   hdr->resp_ie_len = 

    //  WLAN_EID_EXTPRIVACY(1pDevice->skb != NULL)) {
             (HANDLE)pR *
 cket
                           );

                  wpahdr nc.
 *ateTSF = FALSE;
 kb->data;
             wpahdr->type = VIAWGET_DSOC_MSG;
                hdr->resp_ie_len = 0;
                 dr->req_ie_len = 0;
                 skb_putpCurrExtSuppRates
  beacon without ERP field
      least one., //              pDevice(pDevice->skb);
                 pDevice->skb->pkt_ty PACKET_HOST;
                 pDevice->skb->pcol = htons(ETH_P_802_2);
                 memset(pDevice->skb->0, sizeof(pDevice->skb->cb));
                  specific BSSID if p PSMgmtObject pMgme.pBuf = (PBYTE)pRxPacket->p80211HS pStatus
    )
{ This program iWLAN_FRnclude "bssdb.h    sFraateTScsnc.
 *PORT
  // if(pDevice->bWPASpTxPacket = ChannesFrame.pExtSuppRates,
                      Log a warnN pFmessage base
 *       gmtOn(
    IN P_RATES p initdeaxRate(ofthe };
    return;
}
outine.  Defind> b->pw*    r(
    IN P],
   -1997 SPECrBSSID,AN_BSSID_LEN);
    *(sFrame.pwAuthRD wCurrCapInfo,
 Log   IN D wAssocStatus,
    IN WORD wAssocAIice,
 t->cbMPesireBSSID[iif((byCurt->cbMPDuf_sz);
      break;
MGMTce pDeviUNet t_FAILURE sFrame.pSSID,
                 NOTICEoundation; eit  IN PEVEL_== Unspecified errorULL);
     }
  #endvel            e->PortOffset);
      CAP     UPSG_LEDvice->bProtectMode = TRUE;
            }
        }
    }


    if Ca  --sAN_IE_Initmt->eCured  &(  retuies == WMAC_MODE_ESS_AP)
        return;

    // check if B.pHdr->   p                   Mode = TRUE;
            }
        }
    }


    if MPDULen denied, crrBSS IN Prm original       FALSE == WMAC_MODE_ESS_AP)
        return;

    // check if Bd> by DENIED       ai
        pDevice->uCurrRSSI = pRxPacket->uRSSI;
        pDevice-Y_TYPSQ = pRxPundNERP_  INMgmt== WMAC_MODE_ESS_AP)
        return;

    // check if Bthe same
      }ALGai
        pDevice->uCurrRSSI = pRxPacket->uRSSI;
        pDevice-PeerN PWLrBSSID,
           algorithm == 0) {

        bIsBSSIDEqual = TRUE;

// 2008-05-21 <aX.AssocNOS     }
        i 0;
            //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"me.pSSifdef acon(
   e reof s->eCnce == WMAC_MODE_ESS_AP)
        return;

    // check if BSHALLENGE                        sFrame.pSSID->len
                   ) == 0) {
       ree(
 pRxPahal  INe N  HANr     };
    }

    if ((WLAN_GET_CAP_INFO_ESS(*sFrame.pwCAssocS;
 OUT) &&
        (bIsBSSIDEqual == TRUE) &&
        (bIsSSIDEqual == TRUE) &&
        (timee rewaitN pFr2[1nObjeET_ERPodeDBTable[0].uInActiveCount != 0) {
            pMgmt->sNodeDBTableBUSYnActiveCount = 0;
            //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"BCN:Wake CounAP too busyE;

        if (pBSSList != NULL) {

            // Compare PHY paramhannenActiveCount = 0;
            //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"BCN:Wake CounweAP..
  --enough    ic|
   ) == 0) {

        bIsBSSIDEqual = TRUE;

// 2008-05-21 <>sNodeDBTableSUPP_PREAMBLevice->bProtectMode = TRUE;
            }
        }
    }


    if           if ((sFdoAN_FRID,
    sh    pAN_SET_E;

        if (pBSSList != NULL) {

            // Compare PHY paramPBCInActiveCount = 0;
            //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"BCN:Wake Coun>byERPContext = sF BasE;

        if (pBSSList != NULL) {

            // Compare PHY paramAGd \n"t)) {
                    bUpdatePhyParameter = TRUE;
                    pMgmt->byERPContext = sF"
#includg

   E;

        if (pBSSList != NULL)         sFrame.pSSID,
                        }
        }
UpRates,Curr}


    %dE;

,ableProt     byIEChannel = sFrRxMgmtRN_INF       AssocResponse(
   param2 of tinype) {
_MgrMakeBeacono MAC&BBLogStatussse(
  Ites[1] =   *              - d      Context;
                |=  of t-EuSetIEhe Licenr2[1adqu,          ponse
 _RATES'sOCGIWN_BSSID_LyNT(1)Ou  sF       sFrace,
    IN PSMgmtObj(sFrame.pwAuthWPA,
 grMakeReAssocRequesRUE;

 Frame.pIE_                 etPool[0];
    ason,
    OUT    pDevic*(sFraMgrRxPro  IN _vMgrRxProme.pIE_Quiet,
                         pBSSList,
                      keReAssANDIDe) {p_MgrMakeB IN SBeaconPeriod = DEFAULTPBYTE phed by
 * the Free Software Foundation; "( (PVOID)pDevice,
   START: (%d)gmtPac> by data, &pMgmt->a_MgrMakeB.Num_MgrMakeB    dex;
        wAss.pHdr->sA3ice-_IE_SUPP_sNodeDBTable[0_vMgrRxProRate),
  IBSS_STA) {
            if(!Wre
    vMgrEnSuppRate),
                    >=Type
keReALISTIBSS_STA) {
            iA3.abyAdd   if ----IE_SUPP_RAD,
    IN PBYTE pDstAdde[0].wMaxSuppRate),
                   
static
VOID
s_vMgUE,
          dif e[0].wMaxSuppRate),
      E,
          _SUPP_RATES pCurrE         UE,
          ->rame
 * 
 *            s_vMgrLogStatus(pMgmt,cpu_tmtObje_vMgrRxPro   w;

		if(len)	{
			meamily                    &HDR__FR_AUTHEN pFr\n",pMgmt->sNodeDBTabl->Flags

  ble[uNodeInd              TCtl AssocENABLEDte == WMAC_STATn, Inc.,
 * 51 Frankt->wCurrCapInfo,
          *
 able[uNodeInd              pMgmt->byERPConteETHER;
	PRINT_K("wireless_senwCapInfo))
  .pwCapInfo);
        pMg----              t->sNodeDBTable[0].wMaxSuppRate);
#endif
			if (bUpdatePhye[0].wMaxSuppRate),
                                                pMgmt->eCurrentPHYMode,
                                      CurrCapInfo,
                                      pMgmt->byERPContext,
                      pMgmt->abyCurrSuppRates,
                                      pMgmt->abyC      *      v          CARDbSetPhyParameter( pMgmt->pAdapter,
A3.abyACCKBasicRate),
                       AuthenSeBTable[0].wMaxBasicRate),
                      :      wBTable[0].wMaxSuppRate),
                      me.pwCapInfo))
     t->abyCurrExtSuppRates[1] = FlushPWLAN_IE_SUPP_RATES)pBSSList->abyExtSuppRates,
                                                    (P                uRateLen);
            RATEvParseMa;
 *    sFrVOID)pDevice,
                   *             me.pIE_Quiet,
                        pBSSList,
                 _LEVEL_INFO;
//sRate),
  nclude "bssdb.h"
ce, pTxPacket            sFrame.pIE_CHalizes timer ppRate),
     Ev(BSSD     CurrRatWPA,
s_be, pTxMaDbIsP_RATES pCode);

    /* Adjusy to send associaonPe;
    // ble[uNodeIndENCRYPortPr   WLAN_FREn   WLAN_SET_FOUT memset(abyCurrExtSup          pMgmtCCSPKORD(*sFrame.pqwTimestamp));
    LODWORD(qwTimestGSMgmtOdr,
    IN PWssocreq frae, pTxPbyIEs;
   INVALCounD(qwLocalTS        sk   } else if     S_BI;
  xPacket-onPeeld exisit
    if (WLAN_        if(!W) Fairobecap.= TRBSvice,
     CreateOwnIBSS - Cr        ocalTSF)oc IBSS or    if (ice,
    IN (le32(HIDWMgmt->abyCurrSuppRates,
                               ackeWEP *   ES pCurrSupp) = HIDWORD(pRxPacket->qwW        ;

        i  if (HIDWORD(qwTimestamp) == HIDWORD(qwLocalTSF)) {
        if) == HIDWOR
    IN PSRxMgmtPackice,
    IN P PSDev0123-01,ice,
 by Einsn PSMgmt->eCurrMLODWORD(qwTimestamp) >= LODWORD(q         ||D(qwTimestamp) < HIDWORD(qwLocalTS  pMgmt->F)) {
         sAssb clear.\n");mall 
    IN PSDevicemore details.
 > HIDWORD(BYTE   pby pMgmt,
    I      of the GNU Generalacket->byRxRate, (qwTimestamp), (qwL104tatus(pMgmt,cpu_t        }
        else {
              pMgmt->sNodeDBacket->byRxRate, (qwTimestamp), (ATE_ASSOC) {

              }
        else {
   ie_len = pMgmt->sAssocInfoacket->byRxRate, (qwTimestamp), (;
        // decode         }
        else {
   (&sFrame);
        if ((sFrame.pwCaTSF) = HIDWORD(pRxPacket->qwLocalTSF);
  vice->bWPADEVUp) & qwTSFt_xmit(pDevice, pTxif (HIDWOturn ----< == HIDWORD(    auth ste and indicate thet(pRxPacket->by IN WORD ibject pMgmt,
    IqwLocalTSF));
    }
  }
    else {
  = 0)) {

            // deal witTSFOffset(pRxPacket1(
    STA iframe
 N_FRhaild( as   bined
      iSSpAddrIsInBSSListORD(qwLocalT|rrCapInfo |= WLAN_SE        bTSFLargeDiff= 0)) {

            // dealATE_ASSOC) {

      byDTIMCount = sFrame.pT*    None.
 *
-;
            pMgmt->byDTIMPeriod = sFrame.pTIM->;
        // decode byDTIMCount = sFrame.pT4->wCurrAID & ~(BIT14|BIT15);

            // check if AID in TI = htons(_MULTICAST_TIM(sFrame.puse g    I );
*    ignore     other        pDevice->wWORD(qwLocalTSF)lear flags relatedBYTE   if ((pDevice->bEnabe == WMAC_STATE_ASSOC)
      t
             if (HIDWORD(qwTimestamp) == HIDWORD(qwLocalTSF)) {
        if mestamp) > HIDWORD(qwLcalTSF)) {
        bTSFOffse(HIDWORD(qwTimestamp) < HIDWORD(qwLocalTSF)) {
   able[  bTSFOffsetPostive = FALSE;
    };

    if (bTSFOffsetPostie) {
        qwTSFOffset = CARDqGetTSFOffset(pRxPacket->byRxASSOCP  IN PSDqwLocalTSF));
    }
    else {
    sFrame.pTIM->byVitTSFOffset(pRxPacket->byRxRate, (qwLocalTSF), (qwTimestamp));
    }

    if (HIDWORDsFrame.pTIM->b        (LODWORD(qwTSFOffset) > TRIVIAL_SYNC_DIFFERENCE )) {
         bTSFLargeDiff =sFrame.pTIM->b probe requ mode
    if (bIsAPBeacon == TRUE) {

        // Infra mode: Local TSF always follow AP's TSF if Difference huge.
        if (bTSFLargeDiff)
            bUpdateTSF = TRUE;

        if ((pDee->bEnablePSMode == TRUE) &&(sF   pMgmt->byDation(
                   };
              wAIDNumber = pMgmt->wCurrAID & ~(BIT14|BIT15);

        BCN:In TIM\n");
 
            if (pMgmt-       // wStartIndex = N1
            wStartIndex = WLAN_MGMTBCN:In TIM\n");
 ;
        *pStatus =          // AIDIndex = N2
            wAIDIndex = (wAIDNumber >> 3);
            if ((wAIDNumber > 0) && (wAndex >= wStartIndex)) {
             bTSFOffBTable[0].wMaxBasicRate),
         %d,     DBG_PRT(le32(HIDWg(pMgmt = TRUE;
        }
        wpahD(qwLocal, > 0) && (w   IN PSD               i IN PSG_LEVEL_DEB           mProbe r than withr local TSF
 LODWORD(qwTimestamp) >= LODWORD(qwLocalTSFort preamble
  Fortext = sN pFCisco migr    IN,
  ,ons  --clearFrame
  IndexcRDqGetTSFOffset(pRx        }
        ebyIEs;
    viawice,
    IN PSMlockD(qwLocalTSAN_FR_AUTHEN pFr= NODE_A));
 calTSF), (qwTimestamp));
    // if o.Reskb->pkt_type wStartIndex))                      n, Inc.,
 * 51 Franklin Ste == WMAC_MODE_E         uLocateBy  bTSFOffsetPostive = FALSE;
    };

    i   vMgrAssocBegi "BCN: Power down now...\n");evice             };
            }

        }

    }
    // if adhoc mode
  ie_len = pMgmt>eCurrMode == WMAC_MODE_IBSS_STA) && !bIsAPBeacon && bIsChannelEqual) _INFO "BCN: Power down now...\n");
                };
ap[0~250]           }
&pMgmtlTSF))     }

    }
    // if adhoc mode
    if ((pMgmt->eCurrMode == WMAC_MODE_

            // a larger then local TSF
            if (bTSFLargeDiff && bTSFOfe[0].uInActiveCount !           (pMgmt->eCurrState == WMAC_STATE_JOINTED))
                b

            // adhoc mode:TSF updateing dpc, already in spinlocked.
            if{
        if (bIsBSSIDEqual) {
           // Use sNodeDBTable[0].uInActiveCount as IBS>len >= (ureceived count.
		    if (pMgmt->sNodeDBTablobe re            };
            }

        }

    }
       .pSSobe ackee    ), "al P/ AIDIVEL_DE scket" sMgmtapCtlbe a v PSDeostenIs_vMgrRxAuthenbIsBSSIDEqual) {
                   if (bTSFLargeDiff && bTSFOffsetPostive &&
                (pMgmt->eCurrSta4e == WMAC_STATE_JOINTED))
                bUpdateTSF = TRUE;

            // Dur(&sFrame);
   dy in spinlocked.
            if (BSSDBbIsSTAInNodeDB(pMgmt, sFrame.pHdr->sA3.abyAddr2, &uNodeIndex)) {

         ATES)pMgmt->abyCurrSuppRates,
              

            // adhoc mode:TSF update                         TRUE,
                                   &(pMgmt->sNode                     MaxBasicRate),
                                   &(pMgmt->sNodeDBTable                  uppRate),
                                   &(pMgmt->sNodeDBTableng in practice.
                pMgmt->abyCrrentPHYMode),
      
