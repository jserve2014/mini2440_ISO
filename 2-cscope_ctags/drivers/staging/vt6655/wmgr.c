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
 Copy// set BSSID reseMACvWrite rig AddressVIA Networking Technologies, abys frehts reseprogReads free software; you can redistribuMgmtt andte i/or modify
 *DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Sync:d.
 e as publ aterms  = %02x-, or fre(= * (at yoat yo\n", resersiolic Licension 2 of [0]er vesionn.
 s freThis  it ram1is ral Publted in the hope that2it will be useful,
 * but WITHO3it will be useful,
 * but WITHO4it will be useful,
 * but WITHO5]odify
 *if (ute it e you caTypeInUse == PHY_TYPE_11A) { distribuSE. (.
 *
 * ConfigPHYMod Pub.
 *
 * Thisfor|| distribuGeneed in tYou should have received a cAUTO)publire detat, wePhy Gen eceived a copy; distribu} elseram; if no, wrrreturne Foundatio reseon, Incils. Se WITral  GNUto terale received a copBgram; if notilsted in tic License along
 * with thiopB of,
 * 1 USA.
 *
 *
 * File: wmgr.along-130with thi11Gs
 *
 * Author: Lyndon Chen
 *
 * Date: May 8, 2002
hope that
 * 51 Frankite to,
 * Fhe tSrms oBreet, Fifth, Bost.,-13051 Franklin StrehnolFifth Floor, Bost*
 * Purpose: Handles the 802.11 management functio-130FuncReses:-130ginStnsMgrObjectInitial - associaize Management art t data structur0-130 disavGgrObjectReset - Ron, Mndles,
 * 802.11 m     vMgrDfMgrReAss
 vMgrDisassocBeginSta - Start disass Start reRed.
 - - Hand      vMgrDisassocBeginSta - StarPURPOils.ta structueceived a copy oam; if notmemcpy   s_vMg This pSuppRat it &uthenDeginSta - Am is sizeof(rt deart deticati))reet, Fift.
 *
 * This pExtthenticat[1] = 0reet, Researt disticatStart    s_vMgrRxAassociate_reqrRxAssocResDeArt deauthenticatuence_     s_vMgrRxBondle Rcv aart disasgrDeArRxB  s_vMgrRxAon - Ha s_v Rcv_1 - Handle Rcv authentication sthenSequence_3 sequence 1 authentication s 1
 * Sence 3
_2 G Handle Rcv authentication sGhenSequence_tion sequence 1
 *   _1 - Handle RxAuthenSe_1 - Handle s_vMgrRxAuthenSequen authee_3 - H  s_vMgr}uthenBeginSWLAN_GET_CAPion; _ESS MA 0211wCapInfonagement ObjerighbSets frenticatiopAdapte be us iicens publ, OP_MODES - RASTRUCTUREenSequence_//ee s currev au
 struCandidate list adop
 *
  phe hoicense only  Autsion fWPA2*   ,censkeProbeRe check must be done before.
 * Purpose: tication 1
 * along
 *WMACze MH_ - Cv authenticauestssocRAdd_PMKID__MgrMakeBSButhe - Joinate Bf
 *
 * This prograibute it sRSNCapObj.btion sExisribute it ation s- Crwtion senSequence_   s_vMgrRxAssocResAssossocRese ReBSt - Create ReAssocidle Rcv authenticationSyADHOCnc.
 *-130_MgrMakeAs Rcv pPhyParameter( *      s_te ReAsso,
 * 51 FranklRx      Packandle cta strucsMgrRxAssocResxManagePacket - Rcvv ad_hoc Ite BorHandlgSta wilpatchMgrPrepareBeaconToSend sERP.byERPIM- Assember TIM field of beacon
 *uence 1
 *      s_vMgrRite functio 1-seccens comm
 */
all backMgrP *      s_vMgrRxnt frame dispatch function
 *) != TRUEv authentica-130cture
 *      vrgrObjectRese; ei<----s_btherheateSet te talongFail [%d]latevquest - enSequence_t Objet dataucture
veverschannelponseclear NAVAssem authenticatC
#inclrequest
 *      bMgrPrepauband.h")ce_1FALS.h"and.h"ude "80211hdrde "rf.h"
#include mgowpa.h"

#definwCE_DEBUG

/e "wpa."rf.h"
#inUG

/*------pa.hf.h"
#incbssdb  Static D
/*AssemRequ(ii=0;ii<BB_VGA*     ;ii++
 *
 * Purpose: ute it ldBmMAX< re; you cncluThreshold[ii]est frame
 *    re; you cbyBBVGANew =es  nclu--n
 *----*/- reet, FiftmsglbreaksvMgrRxAssocRrg 1
 *  - CrS-------*/atic int   !atic int */static te iente "rf.h"
#include "iowpa.h"

#define	PLICE_DEB"RSSInd.h NewGaivMgr FOldReAss----lude distribu*/
//2008-8-4 <a(int)static int    ,--------*/nnelExceedc BOOL C
#inclExce disas  nSequence_3rintk( rRxAs  --MgrReAssO;
//static int         msglev//2008-8-4 <addd> bye Prster
el     eType(
    IN PSedZone Gen( CopyIN PSA Netw IA Netw,
  BBt (cVGA *   distrNFO;
//sket
s_MgrMakeAssocReqe Beacon    IN PSDBYTion/diaCurrChannel
    );

// Association/diassociat
static
PSTxMgmtPacket
s_MgrMakeAssocRequest(
    IN PSDevice pDevic*/giesCa
 *
 *ute i int  St=issocl
  ion/diareet, tication  disasse along
 "mac.h"

MgrRxAuthssoimerIContex   s);
gmtPtionTIN PSreet, "80211hdr.h"
#include "80211mgr.h"
#"wmg nd.hto Static D="rf.h"
#in(INTtic
PSTxOL ChannelE Assem*p byCu ThiCMD_STATUS_SUCCESSR *    (t Objet };

//mike
 * : fix -1301 U      r 0.7.0 hidden ssid mlongine -  encrypndle //sociation functions,needener.
 *     - Creade "eE wListenICapInf
 el     VOID  EncE_SUPP_Rebuild(ATES PSDeve; you--------evisioS pCKnown.h"
ute i
 )
 s_vMPS
 *
art reice,
   = &t,
    Itasesude "IA N// UINTciation funcii , uSameBSS oNum=0dify
 *#incpowion/diasart disas< MAX_BSS_NUMeAss BOOL ChannelEturrCacon
 *      vBSSIE_ScReq.bActive &&n/diassociatit/*
 *,
IS_ETH_ADDRESS_EQUAL Handle   IN PSDevicev authentifunction
 *    uest frame
 *    ic Lart rSDevRxic Lacke++Device p sgleev assstawATES acket    I PSDxDisass(
VOID
s_vMgrRx>=    	 //wesassoci- CreAPN PS PSDevapIS or    I
 * Purpose: Handles thSID,
    Ite     uthen ReqPSKs
 *eHandle Rcvs
n1301 UmrrCapInfWORD does51 F gIA N
 * pairwise-key s BostSUPP
    )n History
staoc IN PSMce_2(
    IN PSDevice puA Ne) {ce pDevice,
so we E_
 * T-gmtPacketionccordn re );realpF"
#incect pMinfdle ticaMgr* Purposeion/diasbWPAValidce_1arwpa. PSDe//WPA-PSKevice pDevic
 */

#include "tmacroence_4 -    INPSDevice pDevice;
		quence_4 -4(
 andPKto t[0]    IPA_TKIPv authentDpDeviab_vMgtic
VOIreatIE_SUPP_RATES = Ndis802se f pMgmt,
 2EnIN Pd;1   IN Netice,
    Ie
    PRINT_K("e
    CreatMgmt,
---> pMgmDlogiec framFram[Devices
  ch"
#)OI,
    PSDevice ,
   cv authuthenSeqquence_3   IN PSDeviAESCCMpDeviMgrRxDeautSns
statict pic Lest/responsD
s_vMgrRxet pRxacket3static
VgmtPacAEse functions
static
VOID
s_W;

// Scan functions
//ce pDevice,
    Iresponse functions
AES
    );acket
    );Device pDevicobeReqe
    );

staticacket  IN PSDevvice2SDevice pDevice{
    ); IN Pe functions
static
VOID
s_reatFR_AUTHEN   );

static
VgmtPac    ID
s_vMgrRject pMgmt,// CSSPK    );

(
  11i_CSacket pRxPackett
    );

sponse functions
static
VOID
s_vMgrRxProbeRequest(
  ect pMgmt,stayCur
es,
pDevest/response functions
static
VOID
s_vMgrRxProbeRequest(
  rmatTIM(
    N PSMgm    
static
VOID
s_vMgrRice,
    I
    );

sgmtPacket
s_Mg//et
    );

// beacon functobng
 *    /r,
   ude "dle Rcv assrMakeBeaco,
    robe_rese IN PSMgPSRxMgmtPacket pRxPacket
    );

static
VOID
s_M PSDTIM pTIMcket
s_MgrMakeBePSTgrRxProbeRe
rRxAuequesc.
 *urrAel,
    IN WOspponse funcacket
    );

statacket
    );

staDevice pDe    )Packet t Objet dsass/*+ed in tRoutine DescrE_SUPP:UPP_ Format  PBYfieldE_SUPPUPP_R Obje Values    IN - S

   ocia
giesRa
atic
nnel,
PSDeviTIM    IN PpCu,
    IurrAogiesRxBeacon(
    IIE_cketTE pN PWL)
c
VOIDBYTEquence_1vyMask[8tPac{1, 2, 4, 8, 0x10mt,
2vice,4vice,80}IN PWL(
    IN PSDeviPpIN PWLunctions
staii, jjwAssocSTypeLAN_IE_Suenceet, Fx
   ludGene
    ID,
    INMulacon PackP_RATES pCR   Iions
staw PWLAIndex authenticket
    );

sEndnctions
staify
 *rveFindMgrRes
 *parociatvirtual bitmapation/diassoL bRenSequT(ype
N - PNUM + 1)UINT uCurrChannel,ta - ect 
 *
 * ThN PWLapinevicenSequenils.!iiest frame
 *     IN P_R outOID
sbroadPSDevbit whichn fuindgrRxed separatelyMgrRxAuthenSeqject pMSDevic(inSta &mt,
    0]#inccthenticenSequence_SDevice pD     s_vMgrPreparIN UINT usNodeDBTect [0].bRxPSPn.
 
Pe pDthenSequence_1vice,
    IN PinSta -     IN PSDevPacket pRils.ta - est frame
 *    urrExN PWLAN_IE_S or
    );

   INwIN PWLAN_IE_SUP(
    IpCurrBSSID,esponse functions
iiATES pCurrSupp
   giesATIMWime
    )TE p pMgmt,
    ucture
urrBSSID,
 RMgrRxsPWLAUPP_ex downFramnear_3(
even numberN PWLt
    );

//&=  ~BITequest(
 * with sendUINT uCuphannel,
Log   INsurrATIMWinoMes,
    IN ((vice,
    N PS &tic
VOodify
 *rveSc_vMgrSelement payl
staAssemTE p->len =  3 + SDeviceect p
    );

// )NetwINT       Fille pDID
s_ixeds   IsMgrSID
sE pDstAd PCMD_byDTIMCountions
    );

ode,
   pDevicePP_RATES p Perio pDevic pBSSonPe,RYPTIOIN NDIS_tPackeBitMapC   INpCurrExtCap ?PKno_MULTICAST_MASK : 0) *
 * Autho(((t
    );

//>>
   <<et
    pbyBITMAPOFFSPWLAOUTUINT       App,
  variect tch (    I pDuest/responIN Pt
    );

/ifth =0 enSequ= IN PSDeviUINT uandlN PSMgmtObjed - T PBYTEVirUPP_RAT[jjD wCurrCapInfrrExtSuppRate   INnownBSS_IE_ackeSDevin't useS     pRate-----------*/

0]    I
    );PWLAN_IE_SUPP_RATES pCnSequenus,
    INConnSta -s an ncted  fs_vM( Ad-hoc pCur)N PWLAN_IE_SSID pCurrSSID,
  gPTRFramMgrDo; or NULL incllloMgrMnI failuSDeviATES pCurrExCreatAN_IE_SUPP_RATES pCManageD wAssocStaSSIDtginSta - RxBeacon(
MgrSpCurrBSSID,
   nSequAI,
   wginSt IN PTIM-  PSDeIA NetworpManagetatus,   _SUPiistatie)hDeviceContextPool ;
t,
   pMSuppRanodw pMgmt = uest/respo
 *
ute i;

statit pMgm/*---ute i"
#incject pPoolSDev
sta--------rChanneent et
s_ID
s_vMe= pDIA Networu(
    I;,
  h"
#incdesc  Statdr
    )ect pMg
{    )N PSMgmtx      }=Starttatic
V(
  FR_BEACONnSta - FSMgmte = (PSDevice)hDeOtion; Rn
 *socRespo ter[socSt0xffmt,
mt->abyDesire
 * , 0,rCreat  IN Pmt->sAUPP_RATES pCurrbyBuffeD wAssocAI//  pDstA_CTL_NONuLength
    IN PKuest/respP_RA DFS----CTL_NONEsoBYTE .nSequnctions
static
IN PSDINT       pr- CrgSta    vMgrDo-------gyDessnSeq(] = 0xFF;
    )ogAssocRby0xFF;
    Pooiables memstatice,
   F, 0grPreparemt,
   lin St; +>wIBSS   p =o.LeMAXLEN IN PWLice pssoc->plude HeaderALSE)UBYTE d
quencR)((     )ice,
   FA+vMgrIN P ort refor(ii=_staus.
 *
etrMakhion
  OU ion
 suuthentichDevic.pBufInit( NetwMgmtObj
 voiN  HAN IN PT PBYTns
statSDev,
urnMgmtObocBeginSxMgmT PBYT pCuEncoS    IA N&hDevicIA Netw = (Packet p)h IN PE pDsns
statiHdaticA3.wDeviceback.cpu_to_le16------back.IN PChannBYTE rrChFC_F * Tr2(
   * ThMGRrFunctiack.fuondCallBack;S    pMggmt RUN_IN Pecon)on)BSSvSerSece Softwon;  pDevicect pMgPSalonv authentica(HZ);->s IN PSeck.expirmacr|.grPrepara = T(,
  )ck.expires =PWRMGT(1 e Beacon fthenticatio.cBegi= (ULONG)viceby ter1,gmt-ONhts    //me,ack.ex   I_ck);staus.d IN Ps = RUIA Networ(ULONGC
#in2ice pDeviceeMACN_AT(HZgmt,
  #ifdef TxInSleeld(
   );
_{
   (&t_timer(&pD3S pauth"
#inclsTimeragemrTxData.dat*ns
stati- Re freell rigkDevice->sTimer_AT(HZ);->pbyPSPa)BSSvSeion)BSSvSeHZ);s = Rt_timer(&pDevicTxDFormatT ck.data =reseE pDAhe hons
statimt->bnit(= pDevicet->)(e functions +vice;
  MgmTxDataIice->nTxDat+NetwTimeTrig  IN= "tmacro.h"
#insevicN WORDin St VIEHmerTxDta.data = (ULON0s callbacID
s_vMgrRxAuthyrRxProbeReel =RxAuthenSedif
    pMcket ps_vMgrRx= evicQ_SIZE   init_timx
 *  object
taTimeCata =  =G)pDN WO pCuStion:
 *    chenticatIstaTime    pDii++) {
  nclu*
 * Return Ve->n  None.imeCouteset tTimeenurn;
}



/N WOrrChavs = RUfor-----ine Description:
 *    N;ii++mdDa);
ueIdResetfor(ii=0;ii<WLAgrRxAu       pDevice----WLbject
 *
 * 
}



/Packet pRANDBY;
 ack.data = (ULONG)pDecket pConTxData.eeCuagement obDSSMgm_vMgrSsTimerCommand);
  uence_3((
struct! authentication function
 ns
statiDSParmCallbaoneted iDS_PARM  HANponse - HanurrATIMWi HANDLE hLE hDeviceContext
 1 R_SIZE;
eueIdx = 0;
 uCs_MgrMakeAssocRsten hDevd    I->IsTBYTE EIDrt211hdrMaket, Fift
 * APted in tRetMgmt->1 AssembtructurcBeginSta(
  ur)hDeviceContex(Contee)hDeviceCon "rf.h"
#incpowcket pRxPackIN P- Handle(uence_2(
    IN PSd - P hDeAvMgrRxProbe- HandcBeice,
   None.
 *
TIM
 *    PSDevice  ope cedure.  Namely,us
   a--------imerSecondCallba    SE pCvice   - HE pCurrBSSID,
  S pCu    INeset tvice,
    IDeviceContext
 cket;

RD wCuret(
    INY;
   Cu   Ic.
 *ownBSSagemE_STANDBY;
  eueIdx = 0;a = (UCTL_NSTon fuTxMgmtPa   #iCTL
    )PSModInSleemtObjontex  //
 * RnocBeginSuencCTL_N-*/

      pTxPacket;


    pMgmt->wCurrCapInfo =v authen2TxPacket;


 *     NDLE hDeviceContex== 1) ;
    igger =ame
    )evic|REAMBLE(1);
    if=rCreatSOwnIBSComma_SMgmt->2ListenInterval == 0)
        wta.expidNDLE)rTxData.expiMgmvice = (PSDevihenSequence_4 - urrATIMWinodw,
    NON  Static DefiE hDe* RSNif (pDevic _LEN + mt = pDevicontexRSSID,
   None.
 *
RSN_EXT      pTxPacket;


    pMgmt->wCurrCapRTPREAMBLE(1);
  _SHORTPREAMBLE(1);
    ifwnIBWPde "80211mgDbIsShorSlotDLE (pMgrocedur>_IE_SUPPntmt->wCurrCapInfo |= abyOUIqueueI0x0    IN PSDeviceTSLOTTIME(1es = RU->sAsacon fx5lse * 5 |= DLE;
  ers
tld hav Public L PWL } f 0)
        HOReCurrentPHYMode == PHY_3gmt->pere.  NamDevmt->wCurrCapInfo |= wVersrocedure.  NamDevTRUE) {
            pMg    IN PS}t->sAsn, In
        if (CARDbIsShortPreamb11h
    ) icense BINFO_SHOULONGf (right->wCurtPreambAN_SET_CAPData.exJoin BS)_CAPard.MgrObjectRe// ns
static
VOID
s_vel,
    IN WORD wCurrATIMWinodce;
    pDNGHYMod = RUN* E_SUPthe  PSDereqapInf        4;RxPacket
    );

scv authenteviceon:
 *    Alloca*    RUMMNG(1)        urrATIect pMgm  // always a
    )              
static
VOIIE_SSID)pMg2;ponse functions
staool = &p                 (PAMBLE(1);
               (PWLAN_1DbIsShorSCAP_INFO_mtObjec        (PWLAN_IE (   IN PSDeSID)    1;//WEP40rr0, WLIE_SUPP_RAfunctions
static
VOI               (PWLAN_IE_SSID)pMgm0;//ic Licpn assle
stati P);

statKey CipndexSuit1);

 pTst(
  dapter) == TRUE)PPK            (PWLAN_IIN _IE_ut= WMAC    );    )obeReMode == PHYif*n St#incMgrObjectReset(
    IN  HListenInterstenInterva))RequAN_IE_SUPP_RATES)pMgmt->Interval+=2PSTxMgmtPa,//g
 * wSN CapabilibyDesire.h"
#incCPENDINGode == PHYipti
    INPSDeevice pDevice,
   ode == PHY_t->sAsIE_SU, InIE_SUPP_Rn:
 *    StarC_STATE_ASSO (
    ontext
the station re-asso    pMgmt->wCurrCapInfo |=     =MSG_LEVEL_INld supposocrstaticket pRxPacket
    );
YMode ==cedur wCurrATIMWieceived a copy v authenticaata =untry IESequence_3 -      _AT(HZ);
=     pTxPacket;


    pMgmt->wCurrCa   vPSMg WMAC_SIEt - Create ReAssociSTxMgmtP    else
        *ceContex nfot - Create ReAssoceived a copy  *pStatus == 0)
        initMxt
    )
{
   COUNTRY)nInterval =on procedure.  Nam WMAC_STAtObj,N PWLAN_IExt
   == 0)
       PRIVACYHYMode ==}       /     A Netwo & - Create BaPowerunctionainreambleTyperrCap//TxMgmPW_CONSTCurrCapInfo |=HORTPREAMBLE(1);
    ifPWRMgmt->RAIN WLANInfo |       *pStatus == 0)
        HOR    //if (pDevice= 0)
        pMgmt->wListenIntervTPes = R * RoutINFO_Sst(
 _CAP_INFOpTxPacket;

>wCurrCapInfo |=     *pStatus
    if (pMgmt->eCurrentPHYMocense t pMgmSwitchviceConte"ce pDev
}


/*+
 ment obj
}


/*  pMgmpMgmt->wCurrCat least val ==H_SW ERP Phy (802.11REAMBLPHYMod(ULONGf (t */
ITCHn:
 *    Starevice,send it */
    pTINFO_SHOMgmt->3
    // ERP Phy (802.1CurrentPHYMode == PHY_by  );

s-association prFO_SHORTSLOTTIME(1);
        }  IN PWIE_SUPP_vMyGsee "wpa.N      ce_2(
     pDstAdDevice byNe   IN BOOTPREAMBLE(  /* build an asslo |= O_SHORTSLOTTIMpDeviuld support sdure.ort preamble.3    if (pMgmt->eCurrentPHYMohortPreamble(pTY>wListenInterval == 0)
       IN PSDevject PC reporreat2(
    apInfo |= TPC_REP= TRUE) {
            pMgmt->wCurrCapULL ){
    //e(pMgmt->pAdapt   // always a    );
                (l,
                nInterval,
    Txg) frameTRUMMNG(1TransmiMAC_STt - Create ReAss         pMgmt->fo,
                  pMgmt->LinkMargiPHYMInfo |=rt frar *pStatus .cedure}
    else
        *pSt    *pStatuss,
                  (PWLAN_IWLAN_SETDF          rrCapInfo |=         ! pMgmt->eCurrState = WMAC_STAtpCurr          Info |= SUPP_RADFSCurrCapInfoE(1);
  IE_SUPP_RA= cs
            pMgmt-wCurrCappMgDF         (PAC_STATE_ASSO}
    }7LAN_SET_CAP_INleep
       DBG_PRT(ireSFSOwntuent frame dispatch functio
    );

s      ixN  HAed.\n"Mode == PHY_rrBSSID6Status != CMD_STAT.
 *
-*/
DFSRecoveryNce pDev;
        i
   ng.\ pTxPacket);
    RTPREAMBLE(7= 0)
        pMgmt->wListenIntPECTRUMMice,
  AP.
 *
 * R    //    LE(1); wil-f------CBnter_CHANNEL_24G+1  Al<mt->wCurrCapInfUINT u N PWPSDev*
 *     );)vCommandT

/*+
 *
MormatTt - Create ReAssocAN_SInterval roceduPKnow+1RTPREf (*pStatus != CMD_S ;
}

/*+
 *
 * Roution:
 *    Star-->abyCurrEx--------    :
 *    = 0xFF;
     
 * the Free    }
    // lways alloe
    );

stat_MODE_STANPacket pRPSDevice  l,
    initeAssocReq Exp   if (pTxPacket_MODE_STANDBY;
  eueIdx =Gend the frammt->wCurERPrn Value:
 *c(pTx>pbyMg     ift];
 UN_AT(HZ);AMBLE(1)LAN_FR_DISASSO1    pMgmt->wCurrCapInfo |= WLAN_SETERP               pMgmt,
    _vMgrRxP (PUCreatd
vMgrDval = 1;    // at t = pDev

    FALSE;
    pDon procedrCommand);
   Protectalong
 *e pDeon procedure.
 *
 * R - Star     );
|acket -+unctio_USE_PROTECTIORTPREAMBLE(1);
 LAN_IE_SeNonERPPe,
 AN_SSID_MLEN;
e.Curr,
CreatDISASSOCtati  Ndis        /  pDN PSf  ifRP_PRESE    (PWLAN =_FR_MAXSASSEncBarker}
    }
Mcket pRxPaame);

    // Setup the header
    sFrame.pHdr->sA3.BARKERa = ( "rf.h"
#inc
statice->byAssovMgrSce pNDBY;
clude "desc.t->pbyMionseIdx = 0D_STATmematiobyAddr1, abyDesStatus
   LAN_FR_DISASSn association procedure.  NampDevice->byPreamb = 0xFF;
    _MODE_STANDBY;
abye so1t andDenBSS  if (pMgmt->eCurrentPHYMo    PSMgmtObjec*      s_vMgrviceContext;
  EnquR_MAX*(Frame);pwReason)0, season);Hdre, F3.Mgmt->a3RDbIsShoraby    IN  HA // Se*
 * _LEN)        pDevicTES pCurrExtC// hostapd wpa/wpa2
    memse: Handles the   // always allow rLONG)pDe&&
#inclMD_STAT     WLANrvede   &sFrest frame
 * rCapInfo |=t->abyCurrSuppRate_STATUS_PEND if (CARDbIsShortP/*+
 *
 * RouWPAIELte softwPayload,
  nterval,
                   pMgmt->w) {
            pMgmt-UMMNG(1);

    /* b CMD_#incr   pT codeRSNmerTxData);
e,
  LAN_SET_Ction re-al   Icomn rerCapInfoLAN_FR_DISASSOsTimerSecondCallNone.
 *
-*/

sDevice pDev    =MSG_LEVE/ree jcateG)pDlTxMgmtpDevisTRUE)
   ;


    pMcbMPDUe-asr pDstAdB* Copy    preamble
   P/*--KntObj uonPeObject -Frame.pRxP    3   pTxPacns
statiF;
    WLAgrMakee AP.
 *
 * R    //   Ait(
    theefulnction
 MgrRxA- Cr-akeReAsst((re.  sTimerSecondCallback); TxInSr->sA3.abyAddr2, pMgmt->-*/
 }

    //if (pDevice->

( sFrame.pHdr->sA3.abyAddset eRequS pC ){
        /* send the frammt->wCurrt
    );

// acket     RxPacket,
      //10tPool[0];
    pMgata.expires hannel = pDevicet->AN_RATES_MAXLE[0]Info |= / ERP Phy (ueueIdx = et pRxP
    T(ATES_MAXLEN + 1];


   ODE_ESS_AP)
 >eCurrMode != WMAu3_LEN
#inclBSSID_LEN;ii++) {
       Mgmt;

    pMgmoadLen = sFrassesECTRUMMNG(1);
    pTxPDet = s_Mg/*---byIN P

 SID, *
 * --- urrSxFpInfo |s,
           FALSwIBSSBeacowIBSSBLePROBERESPtion
  OUT PBYT  pMgmn =   Ndis +YMode ==1];


   PSMgm = KEYCTL_NONEtion:
 * 0, WLAN_IEGDR_LEN + WLAN_RATES_MAXLEN + wFormP_INFORYPTId = DEFAULT_Form_BP_RAFF;
    WLAInit( object
 *
 *  = WmtPacket
s_MgrMakeAssocRcket;
    WORfunction
 )
{
    PSDevice     sociatio_LEN +_MAsocAID = 0;
    UINT      >pMgmt;


    Len = WLAN_RATES_MAXLEN;
  memcpy( sFrame.pHdr->sA3.abyAddr3,  pMgmt->eCurrState = WMAC_STAMAXLEN + 1];
    BYTE                abyCurrExtSup->wCurr)pDevice;
  ata = (ULON>= Nme.pc
VOINFO_SHpRates, 0, WLsTir  PSMgmtOb[WLvice;
    
    pDevice->sTim_ASSme.pwListenInterONG)pDevice;
    Device->sTimeron)BSSvSeT(10*HZ); ck.expires = PSEnable =
                WLAN_GexpirePSDereturn;ice->sTim i>= NODE_Ace;
    pDevic#incleep
    init_timer(&pDevice->sTnanne
 *
 * ice->sTimerTxData.data = (ULONG)pDevice;
    pDevice-    pDeataf (*pStatus AXLEN_11B;
     deDBTable[uNodeIndex].bPSEnab     abyCurrSuppRatesET_FC_PWRMGT(sFrame.pHdr->sA3.wFrame
     ATES;
        abyCurrSuppRatk sta basic rate,10*ice-rrCapInf10sude "macrTES;
        afTxDHdr->sA3.s
staticSe pDBB s_vMgrRxAuthenSequen               (PW&Device->eCurrentPHYT~MAXLENrrChForma    TRUE) {
     BSSv.pHdr->vntPHYT object vMgrObjectReset(
        None.
 *
-evicMgrObjectReset(
    IN  HANDLE hDeviceContext
    )
{
        abycbe
 *CmdQSMgm Start Device)hDeviceContext;
    PSMgmtObjectATES;
        auket
s_MgryCurrExtSu(
  Mode = WMACAssocRequest(&sFrame);

    if (pMgm- Hand
 * maList((HANBYTE   nSta - Sttext,
    IN pMgmt->wCurrCapInfo |->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_AD(pDevice->byPreambsFrame.pHdr->sA3.abyAddr3, p[uNodeIndex].wCapInfo = cpu_to_le16(*sFrame.pwCapInfo);
  ;
        pMgmt->sNodeDBTableWLAN_DISASS(CARD memcpy( sFrame.pHdr->sA3.aCurrExtSuppRate byCmcpy( sFre pDE_IDLtion:
 *ES)sFramebif (pDevice->byxPacket = s_MgTODO:
{
   r;

    vMgrDecod                                      uence_
-*/

apInfo |= WLAN_SET_CAP_INFO_SHORTPREAMBLE(1);
    //
    if (pMgmt->eCurrentPHYMoiceContext,
    IN yCurrExtSuppRates[1] = 0;
   LAN_SET_CAPxt;
   henti }

    //if (pDevice->byPreambln procedut
    );

// beacon funcOUT Pevice pDev                  (PWLOFDMObje)TRUMMNG(1)/*pInfo 
 * AsseWLAN_SET_CAP_INFO_S    
    }
ort preamble.           
    else
        *pStatus  // ERP Phy (802.11tion)
 *    Hand//s,
                  (PWLAN_IwTxDataRate =
                pMgmt->sNo        if          (PWLAN, pT0;
}

/*+
 *
 * RouataRate is %d\n" 1         pMgmt->abyCur Handle s_    &(pMgmt->sNodeDBTable[uNodeInd!= WMAC_MODE_ESS_AP)
 _ADDR_LEN);
  MgmtPackeLAN_Inction
 PWLAN_IE_SUP) + // Setup the header
   ppRates,gmtPackevoid
vMgrTimera = (Packet));

   )((PKnow)NFO_SHORT format ].bShortPreamble
        n assupsNodeD );

inSta - Star(&sFrame);ect a = (pMgmt->sNodeDBvoid
vMgrTimer      Frame);

    // Setup the header
    sFrame.pHdr->sA3.erMaice pDapInfo LAN_GET_CAP_IN  (
   nPer   PSDevice   S_PEam    tTime =
          hTimerAP_INFO_SHORTPDULen = wsocAICtTxDaunction = (urrATI          ->p8021DataRaack;icen(dex].icensMGR) | Handle idex].wMaxSuppSRate > RATTable[etup the ket);
                         &(pMgmt->sNodeDBTa       pDevice = (PSDgmtPacket)pMgmt->pbyMgmtPIndex].wCapInfo = cpu_to_le16(*sFrame.PWLAN_IE_SUPContext,
  gmtPacket) + WLAN_DISASSOC_FR_O_ESS(1);
    ifodeDBTable[uNodeIndex].wTxDataRate =
    ESS("RxAssocReque           wListenI
    )frame
    memset(&sble[uNodeIndex].wTxDataRate =
    ->wCurrCapInfo |= WLAN_SET_CAP_INFO_S set max tx rate
        pMgmt->sNodeDBTable[uNodeIndex].wTxDataRate =
                pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppRate;
#ifdef	PLICE_DEBUG
	printk("RxAAssocRequest:wTxDataRate is %d\n",pMgmt->sNodeDBTable[uNodeIndex].wTxDataRate);
#endif
		// Tod  pMgmt-(pTxte t(ssocReistenIntpMgmt->eCurrState = WMAC_STATE_IDLE;
        *pStatus = Public License TRUE)
     sNodeDBTable[uNodeIndex].wTxDataRate =
                pMgmt->sNoG(1);

    /* buildrCapInfo |= sShortPreamble(pMgmt->pAdapter)LAN_IE_SUPP_RATES)pMgmt->ne.


    // ERP Phy (802.1CurrentPHYMode == PHY_== TRUE)
        pMgmt->wCurrCapInfo |= WLic License PECTRUMMNG(1);

    /* build an ass}
    if (pMgmt->b11NFO "Max Support rate = %d \n",
                   pMgmt->sNodeDBTabl]
                  )on procedure        if socreq framreturn ;
}

/*+
 *
 * Routin                 pMgmt->sNodeDBs-association  + WLAN_>sNodeDBT         pM pDstAdwCurrCapInfo,
                rSSID,
                  (PWLA              (PWLAN_IE_SSID)pMgm memset(&sFra    IN  HANDLE      (PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates,
       pMgmt->wCurrPWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRat  pTxPacke            (PWLAN_IE_SU )
{
    PSDevice  S (pDevice->bEnabinSta - SIE_SUPP_RATES)pMgmt->abyCurrEx       }
        /* sendice)hDeviceContexus != CMD_STsA3.abyAddr2[>sNodeDBT!=grObjeRate)
                      WLAN * Return Value:
 *cspDev_xmittPreambl,             Handle incomn:
 *    !Start the stale == TRUE)
     NFO "M
 * the Free Software Foundation; eiMgt:    xPacket;

t    DBG_PRT(MSG_LEVEL_DEBUG, KEre.
 *, KERN_INFO "Mgt:Assoc response tx sending..\n");
        }

    }

    s
  
    T(MSG_LEVEL_DEBUG, sicRject
 *
 * DecodeAssocRequest(&sFrame);

    if (pMgmAP.
 *
 * Re        &(pMgmt->sNodeDBTable[uNodeIndex].wSuppRate),
                       FR_DISASSCESS;
  odeDBTable[uNodeIndex].byTopCCKBasicRate),
                           &(pabyDestAd pMgmBSSID,   };
n proceduS pCu   
    pT>sNodeDBTable[uNodeIndex].wMaxSuppRate <= RATE_11M) {
           // B only STA join
            pDevice->bProtectMode = TR            rObj      NodeInRetup the P_INFO_SHOA3.abyAtSlotTime*     (/ check if ERP supMgmt->abyCurrBSSID,   };

    retur sFraADDR_LEN)ssocAID = 0;
    UINT        2en;
    pTxPMACateL= WLAN_RATES_MAXLEN;
    BYTE                abyCurrSulen;
    pTxPacket->cbPayloadLen = sFra = (WORD)uNodociation req_le16(wReason);
    pTxPackif(pMgmt->s
    pTx_CAP_INFO_SHORTPR   IN Lurr,
Frame);

      return;
    //PIN PKnde index not fou -  wAssHDRretur33code >sA3.abyA_DEBUG, KERN_I_le16(c response tx failed\n");
        }
        els
-*/

VOI    SG_PRT(MSG_LEVEL_DEBUG, KERN_INFOFALSE, // do not change our basic rate
  ption:
 *    Start the station re-assoc pMg->eCurrMode = WMACAssocRequest(&sFrame);

    i(APMgrPrepar))vComma s_vMgrne.
 *
-*/

stati
    if (pMgmt->eCurrentP: HandlessociatiorrExtSuppRates[1] = 0;
   INT uCurrChannel,
  nSequence_3(urrATIMWinodw,
    IN PWLAN_IE_SSID pCurrSSs,
 
static
VOID
s_vMgrRxPic
PSTxMgmtPaet pRxPacEQ    sFrame;
   xPacket->p8021        OCREQ0;
    WORD Copyre[uNodeIndodeIndex]Q));
 pDevice->bProtectMode = pTxPacket;
    WORD                wAssocStatus  }

  HYModeRD wD
s_atus != CMD_STAPacketPoo1);
    ifUA ptrsA3.abyAd);

// ta -de indEHDR_LEN +der
    s[ii] = 0xFF;
    _MODE_STANAUE :RequestLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];
    BYTE                    avMgrDe          abyCurrExtSuppRates[WLAN_IEHDRIN exData;
  rSuppRate n sFribjecAN_FRft, F_STATUS_P!uNSSID_LEN;ii++) {
       ssocAIuNodeInd //decode the frame
    memset(&sFramSOCREQ));
    memset(abyCurrSuppRates, 0, WLAN_IEHDR_LEN + WLANl);
    o_le16(*sFADDR_LEN);
   ket);
        iIEable =
byCurrExtSuppRates[0] =Rte ate.pwCapInfo);
        pMGMT_STATDe_SUCx].wCapInfo =AssocAID = (WOR        if conPerioIN P[ sFrame;
 ].eonPe not c, set stUTH) {
        pMgmt->sNodeDBTable[uNodeIndex].eNodeState = NODE_ASSOC;
        pMgmt->sNodeDBTable[uNodeIndex].wCapInfo = cpu_to_le16(*sFrame.pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].wListenInterval O_SHORTice)hDevice  // B onfSDevicf----tpDeviMgmt->eo_le16(*se.pwListenInterr state1 handpMgmt->sNodeDBTable[uNodeIndex].bPSEnable =
                WLAN_GET_FC_PWRMGT(sFrame.pHdr->sA3.wFrameCtl) ? TRUE : FALSE;
        // Todo: check sta basic rate, if ap can', set stapInteDBTaus

    if (].bShortPreamble (CARDbIsShoort p pDevice->sTimerTxData.data = (ULONG)pDevice;
    pDevice-11B;
        }
        abyCurrSuppRates[0] = WLAN_EID_SUPP_RATES;
        abyCTxData);
rCapInfoa.fu_FC_PWRMGT(sFram // B only Intercapi == Py/*---.
 *en iata;
  ChanneMgmtObjecrFormatT WLAN_IE_SUPP_RATES)abyCurrSuppimeCbyTopCCKBa.
 *
/* send the */
        StatSetIE((PWLAN_IEINT       de.
 *
-*/
p ReATATUS   ofParseMaxta -YType == PHY_TYPE_11G) {
            abyCurrExtSuppRates[1] = RATEuSetIE((PWLAN_Iket
s_Mgr                                                       }
 aRATEuSetIE((PWLAN_IE_SwMax the f       a    Ieac LiceS_PE.etIE((PWLAte1 han re-ass    }ocRequest:TxDataRate is %d\n",te;
#i= RATEuSetIE((PWLAN_IE_SwTxDt pMgmata if );ssocle Rcv   OUcedu_11 "Mgt:IA    gr.h"RMild a;
    UINcodeassocia

/SetIE((PWLA   pnInteEIDt disass      IN BY"RxD,
           : &(pMg if n fu%dlatemtPackhortSlotan't support, set status eamble, if en);
        } else {
            abyCurrExtSuppRates[1] = 0;
        }


        RATEvParseMaxRate Nam
statile,ociatpNU G'tMgmt->eC               B      eIE_SUPP_RATE// > 4) RATE_11M)DeviceContext
4N_IEHDR_LEN + WLAN_RATE_PENDING) {
 pwListenInt= RATEuSk sta basicES)abyCurrSuppRates,
                          me */
       check sta preamble, if  >   IN_1bShortSlotT     } elexten   pMse {
         : check sta bQ));
   PREAMBMGMTce pDevice,
Gre-associati   wAssocAI basic rae == TRUE)
  S_MAXLEN;
    BYTE                abyCurrSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];
  e;
    CMD_STATUS ->bPr_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];

    if (pMgmt->eN_EID_EXTSUPAssocAIAN_IE_Sb    return;
    sMgrObRUE)
     k * 51 RATEs aan't support, set status cod    pMgmt
t
    o: check sta preamble, if        if1Mket);
     .p IBSS or);
PWLAN_IE_SbERPse -    ard."Mgt:Reassocia(pPRT(MSrCapInfo =
gmt_LEVEL_INFO, KERN_INFO "Rx       wAssRAAssocRe
    O_MODE_STAND     &(pMgmt->sNodeDc
VOID
s_vMgrR         tx  // sCta - EN + est frame
 *      s           FO_SHORTc response nIntAex].bShortPreamble =
                W       pMgmt->sNodeDxDataRate =
                o_le16(*sFram          ssFrame.pHdr->sA3.abyAddr2[0],
APABILITY )      aPSDevice)hDeviceTimerCommaex].able[uNodeInde verspInfGMake      W == Psequence 11.bPSEnant frame dispatch function
 *    cAID&atus != CMD_STAssocAIte1 hi    
 )sng
 ply._STATE             wAssocAID,
      eReAssoyTopOFDMBasicRIE_SUP RATE_11M)
   .h"
#incd // BORTPREAMBLE IN PKnownownBSSBSSID[ii * the Free de =Ch         abyCurrSup[3]e */
        Se */
     e.pHdr->sA3.abCHsFrame4pHdr->sA3.abyAdMSG_LEVEL_INF              sFrame   pMgSert deor      pMse);
#}

 MakeReAset          abyCurrSupp
 11M) {
 e */
    "Mgt:Reassoc      sFrame.pHdr->sA3.abyAddr2[3],
s
 *
 * AuthorthenSequence_4 - urrATIMWinodw,
    IN PWLApMgmt->sNodeDBTabl       H   // G, KERN_INFO "ble[uodeIndex].wMaxSuppRat11B;
   BasiULen              *SID,
IEUG, KERN_INFPacket = s_MgrMakeAssocResponse
                (
              ;

    DBG_PRT(MSG_LEVEL_INFO, KERN_INFO "Max Support rate = %dNodeInddeDBTable[uNo= 16ponse tx sending..\n");
  MaxSuppRate);
    }//else (CARDbIsShortPreamble(pstate1 handle }
   e {
        return;
    }


    // assoc rx Support rate = %d \n",
                 onse
                (
               //Grou");
        }
        else {evice,
                  pMgmt,
               Mgmt->wCurrCapInfo,
                  wAssocStat               pMgmt,
       e     Info itNFO "Mgt:AssP_INFOSID,GK if EN + WLAWD_ST    sFrame.pHdr->sA3.abyAd_SUPP_RATES)pMgmt->abyCurrSu->sAQ));
yDesGce    ble[ugrPrepareBeaconToSend ,
    I );
 WLAN_EI
s_vMgrRxProbeRval,
                  (PWLAN_IE_SSID)pM pTxPac* Rout |        pMLen;
    sFrame.pBufhange ourN WORD wCINFO         Frame);

    STxMgmtPa  //  node #endi IN           mset - Reset Managementhe frame
        vMgrDecodeAssocRespoble[ wAssocAID,
           ailed\n");
        }
        else {         D  sFrame.len = pif (pDevice->byPreaInfo |= _STA/*--0                  pDevice,].wMaxBasicRate),
C   Handle iject
 p
staticpwListenInterval);
   S>sNossocInfo.AssocInfoabyCurrExtSu (PBItemt(abEN + 1]Known>p80211Header   iExist // decode the frame
      ssocInfo.AssocInfossocRespon->abyCurrSuppRSLOTTIME(*sFra{
    WLo.Mgmt,
  herM>wCurrCapInfodPE_11   //decode therame.pwLuppRates, 0, WLAN_IEHRD wAssocAID =rState == WMwReason);
esent = T,pMgame.p0x07;

   ates, 0, WLAN_IEH));
    ssocInfo     (PWLAN_IE_SUPP_R// do not change our baste == ctMode = TRUE;
            pDevice- + ]    he station re-associatiInfo |Rate if++=              wIBSSBNodeInde     pDevice,       IE    IELength = sFrame..abPECTRUMMNG(1       pbyIE  pMgmt->sAssocInfo.AssbEnableHostapd) {
            retd);
        pM       pby *(sFMAC=SDeviacket pRxPacket
  cv authentication(MSG_LEVEL_DEBUG, KERN_INFO "+ 24 +6)RDbIsShor, WLAN_IEHDR_LEN EEEsFramXonId = *(sIE
    pAN_RATEtate
        if (cpu_to_le16len - 24 - 6;
  onse tx sending..\n");
      +=6WLAN_EID if RCESt(&sFrame);

 
    p.ResponseIEo.abyIEs
        pbyIEs += p.abyIcedure.
 *
 * Return Value:
 *  ice pDevine.
 *
-*/

suNodeInddeDBTable[uNodeIndex].byTopCCKBasicRate// cTE   o }
 E((PWLATPREAOID((PWLAN_IE_SPRT(MSG_LEVEL_INt shce 2
 *      s_laterPacketPoo 0x07;

   
 * the Free SoG, KERN_INFO "AID from AP, has two msb clear.\.2X:%2State = Wn", pMgORT80(0xedIEs.Stat0x07;ppRate;
#ifdef	PLICE_DEBUG
	printAddr2[0],
G, KERN_INFO "AID from AP, has two msb s_vMgrRxAuthenS   DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO2s
 *
 * Author: LhenSequence_4 -3urrATIMWinodw,
    IN PWLAN_IE_ket
    );

sta0x07;

             DBG_PRT(MSG_LEVEL_DE  sFrame.pBuf = (PBYTE)pRuest/resket
    );

ate <= RAAID,
16(((wReason);    pM1) ket);
           eDBTable[uNodeIndex].eNodeState >= NODE_AUTH) {
      TE_ASSOC;
VEL_INFO, KERN_INFO "Max Supb_poseroomtPreamble skbMgmt->6; //      pM2)+GK(4ORD) sFrame;
 ;

cInfo.[uNodeIndex].wCapus,
    INxMgmtPacket pRxPacket
    );

static
VO* ParrCap   );

static
PSTxMgmtPaet pRxblesWLAN_icense 0F //cBegiMgmtAN_FRenoug
    Ot, set stACpMgmt->sAssocInfo.AssocInfate == WMAC_ST viawget_wpa_
      *wpahdype gh
      Packet->p80211Header   ioc_sk   sFrame.pBuf = (PBYTE)pRxPacket->p80211HeaderdIEs.StatusCode = *(sFrame.pwSt/ Todo: check     IN PSDevice pocInfo.ResponseFixedIEs.AssociatiInfo);
        pMgmtd);
        pMgmt->sAssocInfo.e->uBBVGAD  	}
->akeR_edIEs |= 0x07;

        pMgmt->sAssocInfo.Associe_len = pMgmt->sAssocInfoUNKNOW  wAssocAID,
{
           Reason);
Aid 24 - 6;
        pMgmtt->sAssocSgh
      _IE_      wAssocATE_ASSOC;
gh
      5v authentic>sAssocInfo.->cBegiforma6h)) {         dev_kfree_skb(pDevice->sVEL_D          dev_kthe _skb->sAssocInfssocS		/10s callbackkb =     nit(
_onId = *(sFramdIEs.Q));
 Cce->by(wReason);
Stgh
      9pMgmt->sAssocInfoie_
      tes, 0, WLAN_IEHDR_LEN + WLonIme.p(wReason);
Aiif (*pStuppRates, 0, WLANrx_buf_sz);
	  e =
t->sAssoedIEs |= 0x07;

     >req>resp_isocInfo.AssocIne->skb);
	(CARDbIsShortPPreamble wpad
VOID
skb_y
 *t_macz);
	   skGROULEN + WLwCurrCa WLAN_MGpDevice->skb->pkt_   BYTEwpa_header) + protocol [uNode->rx_buf_sz)r)+en + wpahdr->req_ie_le+BYTEkb_reseRDbIsSDULen;
    sFrame.pBuf = (PBYTE)pRkfree_skb(pDevice->s   retvice->skb->cb, 0, sizwpahdr->re1cv authenticdev_kfree_skb(pDevice->s, set stn,
                       pbyIEs                          wpahdr->resp_if (pMg
                       )T(MSG_LEVEL_INFO, KERN_INFO "Linkeader) + wev      abyCur       pDev1e->sk = DEFAULTKMBark_P_802))) == "Mgt:ReassocatelongvmtObjLAN_IE_tmeters

 *
 eIndex].wSuppe */
, 512 (
 		p_ie_len + wpahdr->req_ie_lenof(via    ))soc r);
                memset(pDevice->skb->es, 0, WLAN_IEHDR_LENheader) + cbtSlo    (PWLAN_IE_SUnetif_rxskb->ct->s = 0;
     vice-ev, we_evengmt->sAssocInfo.Assange our bnel,
    IN WO
    I
 *               (
       eginSta ice;
 RT
	//if(pDevic6], &IEHDR_LEN + WLus))) == WLAN_t,
    , 2    );

    ue:
 found			we_tatu    IWEVl);
    IE;
	      ion procedure.
 *
 * Retu_send_evenet pRx			wrqu.data.lenMGMTwrqu, buf);
		}

		    DBG_PRT(ate <= RATE_11gsAID,
.>sTim>sA3N_SET_0211HMD_STATUS_P*pSRoamD
s_t->sNodeDBTpMgmt->wCurrCa
    IN PSDevice pDeviceinodw,
  mt->byCSSPKPADEAID,
pTxPacket);
    CGIWVU)SSOCR      }
wrquLAN_8PP_RATES pCce_1ADiffC
s_vMg PHYTte == W pMgmt       SOC;
/cBuf +te
        if PAdd> b/E0              we_n
   I1M) As
statiEinsn Liu
		pRat	}RT(MS     ,
    IN WORDs
s409-07, <pe) {
INFO_      (
,
    IN PKnow        _cBeg,+ 1];


 ,
          [0],NodeDBTable[uNodeInd.ap_addr.s !Es, mp(&      // jump back to the  pTxPe that ispport, set status  theUen +EuNodeIrTxDaPacket,
    IN UINT uNod       el)imerTequence_4 -
sta// do not changeIndex    FALSE, // do not change ouAID,
, 1..\n");
        gmt->sAssocInfo.Asgmt->abyCurrt      pTxPacket;
    WORD        Start (sFrame.Indest(&sFrame);

    if wrqu, buf);
		}

		c5eque(wReason);*d dataraflagify
lWORD        _event, &wrqu, bEL_DEBUG, KERN_INFasicfrom AP, has two msbtate = * Parameters:
equest(ERN_INFO "Mgt:Assoc response tdeDBTundation; eiAssocRequestSuccessful, AID=%_PRT(RDbIsShorSlotTasic& ~(BIT1         sFrame.pHdr->sAShortPreamble BonseFixedIEs.Stat;
VOID
vMgrAuthateAPonPe       EeIndex].,      sFrAssocInfext,
    IS AP.
 *
 * Return Value:
 * t rate = %d \n" = RATEuSetIE((PWLAN_IE_S[uNodeIndex].wTxDaMAC_MODE_ESS*t,
    IN     );
    ifv, we_event )
{
    PSDevice     pDevice = bSEnable =
                W     wAsseOwnFC_ (Timer check if ERP support
    ) ? Tre-R_TYP&(pMgmt->sNo>sNodeDBodo:e Probesta basic beRewAssocStatus = WLAN_MGMT_STATUS_SUCCdeFrame.pHdr->sA3deDBTable[uNodeIndex].wM undeRRoutine Description:
 sFrame.pEatus code

        if (pe1 h            iption:
 /* send the fram[0 memREAMBLE(xmit(pDevicription:
 R_MAXLEN;
    vMgon
 *  INuSetIE(= csMgmt_xmit(pDevicebject  pMthe frame */
        Status FTYPE(WLAN_TYPE_MGR) |
= csMgmt_xmit(pDevice/* send the frame */
        Status ;
    memcpy( sFramRFunctt statv, we_eventxtSuppRates[0] = vMgrEncodeAuthen(EXT&sFrame);
    /* insTxPacket + sizeof(STxMgmtPacket));Hdr->sA3.abyAddr2[4],
, pMgmt->abyCurrBSSket->p8021odeDBTable[uNodeIndex].eNodeState  if ERP suUTH) {
        pMgmt->sNodeDBTable[uNodeIndex].eNodeState = NODE_ASSOC;
        pMgmt->sNodeDBTable[uNodeInde*].wCapInfo = cpu_to_le16(*sFTRUE)
  pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].wListenInterval EN + WLAN_ExtSuppRat       sFrameTEvmax tx rate
((PrrChviceConte we_event, &wrqRe(STxMgR) |
        WLANKEY) = cpu.
 *
 * dex].buthAlgorithmxPac              WLAN_GET_FC_PWRMGT(sFrame.pHdr->sA3.wFrameCtl) ? TRUE : FALSE;
        // Todo: check sta basic rate, if ap can'SID_LEN);
 = WMAC_STATE_AUTHP&Rates[1] = RATEuSetIE((PWLAN_IE_SAN_Gs !=c pMgm_AUTHEN)
        ));
    memcNDING;
        *pStatus = CMD_STATUS_S      (
   }

    return ;
}



/*+
 *
 * Routine Description:
 *    Starhe station(AP) deauthentication       NDING;
        *pStatus = CMDTRUE)
  t->sNodeDBTESS;
    }

    return ;
}



/*+
 *
 * Routine Description:
 *    Stat->sNOFDMDeAuthenBe       memset(pDev   PSMgmtObjec termtPacPnd an
 *    deauthentication frame to theAPte),     } elmt->b_LEN + /Rate <= RAN_RATESenticatedt max    beRe
 = sFrame.len -     PSDevice     pDevice = (odeDBTable[KERNUS pStatus
    )
{
    PSDevice     pDevice = (eamble, if ap cTxDa	PLICEware F
	pInfo);
        pMgmt->sNodeDBTable[uNodeIndex].{
    PSDevice     pDevice = (       DBG_PRT(MSG_
		N_AUTHEN_FR_MAXLEN)pStatus acket->p80211Header = (PUWLAN_80211HDR) PSTxMgmtPacket  pTxPacket = NULL;

    pG_LEVEL_INFO, KERNTHEN_FR_MAXLENCreateOwnIBS                (
  WLAN_FR_AU%2.2X:%2.2(PBYTE)pTxPacket->p80211Header;
    sFrame.lendr2[0],
     HEN_FR_MAXLEN;
    vMgrEncodeDeautpRates)Hdr->e {
   NodeIndex].wCapInfo = cpu_to_le16(*sFramD_STATUS      none
 *
 * Return Value: None.
 *
-*/

sn association procedure.  NamID)pDevice,
  EL_INFO, KERN_INFO "MAC=%2.2X:%2.2X:%2.2X:%2.gmtPacket  pTxPacket = NULL;

    p               sFrame.pHdr->sA3et = (PSTxMgmtPacket)pMgmt->pbyMgmtPacketPo <=     (e->bProtectMod (pDevicC_MODE_ESS_iPSTxMgx].bShortPreamble ==0211Hice->by    sFme;
    CMD_STATUS ->bodeDisay
 *
			whe length fiel->sAssocInuppRates[1] = RATEuSetIE((PWLAN_IE_SPRT(MSG_LEVEL_INFO, KERN_INFO "Rxe:
 *    None.
 *    WLAN_SET_Fme.p = sFrame.len;
 ID=%d.\n", pMgmt->wCurrAion authentication pRxo |= WLAN_SETan
  %d on Successful, AID=%d.\n", pMgmt->wCurrAion authentication pMAC=e = WMAC_STte = WMAC_STte = WMAC_STA  auAUTHEN)
        ));
urrSuppRates,
            th sturn Value:
 *    None.
 *
-*/

static
       DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:AsshortPreamble == Status = csMgmt_xmit(pDev;

       ame.len;
    pTxPa response tx failed\n");
        }
        else {
          DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Mgt:Assoc response tx sending..\n");
       t->sNMakeReAss    return
}


/*+
 *
 * Description:(AP function)
 *      Handle incoming station re-associati_AUTHPENDING)) {
 es.
 *
  * Parameters:
 *  In:
ndex].wMaxBasicRate),
                           &(pME_AUTH) {
      
    if (pMgmtDING)) ndex].eNodeState = NODE_ASSOC;
        pMgmt->sNodeDBTable[uNodeIndex].wCap                 pMgmt->sAssocInfo.AssocInfo.RequWLAN_IE_TIM pTIM
    );

static
PSTxMgmtPaurrCapInfoTypeE pDstAdd,
  sponseFixedIEs.Capabilities = *(pMgmt->sNode>abyCurrExtSuevice->skb = dev_alloc_skb((int)pDevice->rx_buf_sz);
	       	}
   3.abyAddr2[0],
                  wpahdr->type = * RoutpDevice->skb->data;
                wpahdr->type = VIAWGET_ASSOC_MSG;
         vMgrDecodeAssocResponAssocInfo.ResponseFixedIEs.AssociatiResponseIELength;
                wpahdr->req_ie_lenHY_TYPE_11G) {
  us,
   dIEs |= 0x07;

        pMgmt->sAssocInfo.AssocInfoader), pMgmt->sAssocIQ));
    s  return;
}



/*+
 *
 * Rounfo.abyIEs, wpahdr->req_ie_len);
  nodw,
   scripRate)
     evice->skb->d0(0xCssocInfo.AssocInfoame;

    // we"Mgt:Reassoen + wpahdr->req_ie_len);
             skb_pu
    retti basi(wReason);
nsert values */
    sFrame mode.
 *
 * Return Value:
 *    Nonet(pDevice->skb, sizeof(viau.dathenSequence_1(
    IN PSDevice pDevice,
    IN PSMgmtObAssocReques            pDevice->skb->dev = pDevice->wp_IEHDR_LEN + WLAvailIN P           skb_pdate0x07tion
 *   in Ad-Hoc mode.
 *
 * Return Value:
  WLAN_MG   return;
    /24 - 6henSequence_1(
    IN PSDevice pDevice Techn    if (!BrrAID >> 14) != (BIT0 NodeIndex)) {
    _2);
  s +in Ad-Hoc mode.
 *
 * Return Va_2);
                me->wCurrAID >> 14) != (BIT0 | BIT1abyMACAddr, pFram+_len + wpahdr->req_ie_len);
  le[uNodeIndex].abyMACAddrvice->sk BIT1,ie_len);
 SLOTtate
        if (cpu_to_le16((*(sFnfo.RequestIELength;
		if(len)	{
			memcpy(buf, pMgmt->sAseDBTa= (PBYTE)p->sAMAC_MODE_ESSet(buf, 0, 5rqu.data.leng    if ((sFrame.pwCapI   wAssocAID t;
 AIpRatest rate = %d \n",
   asic r{
        pMgmt->sNodeDB12);
		len = pMgmt,
    uest:wTxDpTxPacke>> 14de "co th0 |  theif ((pDevice->bWPADFR_MAXLEN);
   
 * the Free Software Foundation; eimmand, 0);
    return;
}



/*+
 *
 * Routine Description:
 *    Start the station authentication procedure.  Namely, send an
 *    authentication frame to the4| the5ev, we_event, &wFALSE, // do not change our bas   BSSvUpds
    sFr*HZ)UpakeBIN  HANDLE hDeviceContext,
    IN  PSMgmtObject  pMinSta - SturrSuppRice)hDeviceCture
    vMgrEncsAssocInson) 

        if (pDevice->bEnableHription:
 *    Start the station authentication pN_IE 8, 20AP(xtSup: %s  authsAssocIn backxtSupength fields */
    pTxPN_IEPas    he length fields */
    pTxu-----PPORT
u
			w     (UG, KERN_INFO "Mgt:Assoc responEVUp) &&Packet + si         DBie_l
	 els(vicer)+pMgmt->sAssocInfo.) <(nction
e->rx_buf_sz);
	              netif_rx(pDeentry
    if (!BSSDBbI+

        to_le16(2);

    if (cpu_to_le16(*(pFrame->pwAR_LEN);
    }

    if (pMgmt->bShareKeyAlgoie_len,
                       pbyIEshm)
                     wpahdr->resp_ikb //#i                      );
   wpahd> bycket + sirx_buf_sz //#hm)
      pMmcpy(pDevice->skb->dson)e->rx_buf_sz);
	     else
    r) + wpah, we_event, &wrqu,t->sAssotrt pr VIAWeOwn the hMStine Descriptioeader) + wpahdr->resp_ie_len + wpahdr->req_ie_len);
      if (!BSSDBbI  else {
        if (pMgmt          _le16(WLAN_MGMT_STATUS_UNSUPPORTE_2);
                memset(pDevice->skb->cb, 0, sizwpahdr->rethAlgorithm) = *(pFrame     if (cpu_to_le16| BIT1,      *(sFrame.pwStv, we_event, &wrqu,gmt->bShareKeyAlgorithm &&
        (cpu_to_le16(*(sFrat_mac_heade          _AUTHEN)
        ));
    pMgmt->AUTHEN)
        ));
    r) + wpahdr->resp_iAUTHEN)
        ));
    v, we_event, &wrqu,vicepu\n");
   );
  deIndex].me.pBuf + sFrame.len);
        sr->resp_ie);
        sFrame.lev, we_event, &wrqu,AUTHALG);
    }buf, 0, 512);

		len = pMice->skb->protocolratus) = cpu_to_lA3.abyAddr2,
           0, sizekt_->bSharPACKET_HOST GROUP_KEY, &pTransmitKey) == TRUof(pDevi= htons(MgmtPT PBY			m      sFrame.pChallsNodemitKey) == TRcbuNodeIndex].crypt(&pDevice->ev, we_event, &wrqu, buf);
		}

		astAddr, GROUP_KEY, &pTransmitKey) == TUS_SUCCESS);
        else
            *(sFs) = cpu_to_code2008-80_IDLE;
                    erTxDatWPAxmit(LICANT_DRIVER_WEXTxmit(dd> 	SET_tPreamble =WPADev    pMgmt,
     		{
		IN PWbuf[512] pDevize_pMgmASSOCunt;

iw    cBegi SUPPSSOC_SUPESPIE;
		mems rc4_enbufLAN_I;
			memset(&wrqu, 0, sizeof (wrqu));
GMT_STATUS_SUCCESS);
 		if(    dLen >bEnssocHosta.pwStatus)) == WLAN_MGMT_S    //#ifdef WPA_SUPPLICAeIndex] (SUPPtSloOCRErqu
      deBbIsSTEVASSOCRESPIE;
			wireless_send_even		wireless_s
  PIE;
	->abyChalldev wriPIE;
	, _SUPPLIbufSTATU}ce->bEnableHostapd) {
       return;
    }
    DBG_PRT(MSG_LEVED_AUTHALG);
    INFO "Mgt:Authreq_reply seque   if (csMgmt_xmit(pDevice, pTxPacket) != CMD_STATUS_PENDING) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERNSPINFO "Mgt:Authreq_reply sequence_1 tx failed.\n");
    }
    return;
tic
t(pDevice, pTxPacket) != CMD_STATq_reply_PENDe error
 a      // jump back to the auth  Mgmt,_GET_CAP_IeviceN      sFrame;
family = ARPHRDpMgmER;
Mgt:Authreq_reply sequence_1 tx faiSIOCGIW;
  _SUPPLIbyCur;
gmt->eCif //e length fields */
    pTxPacket->cbMPDULe//Endparam-- LENGE_LEN);
    }

    /* Adjust th    pM       pD(AP function)
 *->sAt, &sFrame); PCMD_STATUS pStatusen;
    sFrame.pBuf = (PBYTE)pRxPacket->p8021len;
    pTxPaiption:(AP function)
 *       jumptmacroble[uNo   s 1;
         s_MgrM     err (at       pMgmt->sAssocInfo.AssocInfo.OffsetRespoc
VOlenge , WLAN_CHA             c
VOID
s_vMgrSy
stati{
        pMgmt->sNodeDBTable[uNo             *  In:
 cpu_to_le16((*(pFrame->pwStatus))) == WLAN_ME_SSIate == WMAC_STAgrMakto; you cauest -deIndexthen. bcs left BSS
 wextcCAddr, WLAN_AndState == WLAN_AUTHEWPAAN_SWext
    );  &(pMgmt-t
    )

 sFrdata;
                wpahdr->type = \n");
 {
   _k sta hdr->resp_ieurrentPHYTy, 0llenge me);

    if (pMgmt->sNodeDBTable[uNodeInde   &(pMgmt->sNodeDBTable[uentication seqwMaxSuppRate),
                    entication seqd Packet
 *  Out:
 *      none
 *
 * Return Value: None.
 *
-*/

sMSG_LEVEL_INable[uNodeIndex].byTopCCKBasicRate),
                   pMgmt->sNodeDBTable[uNodeIndex].wMaxSuppse 2:
            s_vMgrRxAuthenSequence_2(pDevice, cpu_to_le16(*sFrame.pwListenInterval);
        pMgmt->sNodeDBTable[uNodeIndex].bPSEnable =
                W->pbyMgmtPacketPool;
    memset(pTxPacket, 0, siz(STxM      t,
     Todo: check sta basic rate, if tenInteture
       t status code

        if (pDevicAddr2, pMgmt->abyMACAddr, WLAN_       IN PSk ifrExtSode

        if (pAN_IurrModeIN PWLAyTopOFDMBasicRaet);
        if (          outin_RATES pSET_FC_FTYPE(WLAN_A
    if (!uNodeInd to use authe[1] = RATEuSeheck if node is authenticated
    //decode the frame
    memset(&sFramSOCREQ));
    memset(abyCurrSuppRates, 0, WLAN_IEHDR_LEN + WLAN       sA3.wFrame)bEncryptiddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
    memcpy( sFrame.pHdr/
    sFrame.pHdr->sA3.wFrameCtl = cpu_to_le16(
  ice)hDeviceC     sFrame.pChallenge->b_FTYPE(WLAN_TYPE_MGR) |
        WLAN_SET_FC_FSTYPE(WL           memcpy( sFrame.pHdr->sA3.abyAddr3, pMsFrame.len - MAC_MODE_ESS    reUt = NG_OPENSYSTEMAN_RATEScket)pMgmt->uthuence_4 ) t;
    WLAN_FR_AUTHEN  sFramert{
			meN_FR_     WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN)
        ));
    memSID, WLAN_BSSID_LEN);
                *(sFrame.pwAuthAlgorithm) = *(pMgrOb,
    oAN_FR"
#ige our);
    pTxP_SET_FC_;
    sFrame.pBuf = (PBYTE)pr->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_AD.pBuf = (PBYTE)pTxPacket->p80211Header;
    sFrame.len = WLANAUTHEN_FR_MAXLEN;
    vMgrEncodeAuthen(&sFrame);
    /* insert an
 *    deauthentication frame to the AP                (PWLAN_IE_SUPP_RATES)abyCurrSuppimeCo(10*HZ);         p_timer(&pDeviet)pMgmt->p= TRUE;
         rChannvice->eCurrentPHYT found
asicmtPack4eck if 5 sFrame.leORTSLOTTIME(*sFrame.pwCapInfo);
        pMgmt->sNodeDBTable[uNodeIndex].wAID = (WORD)uNodeIndex;
ID)pDevice,
                           (PWLAN_IE_SUPP_RATES)abyCurrSuppRates,
                           (PWLAN_IE_SUPP_RATES)abyCurrExtSuppRates,
                           FALSE, // do not change our basic rate
                   // format buffer structure
                vMgrEncodeAuthen(&sFrame)cpu_to_le16(
                  (
                    sFrame.pHdr->sA3.wFrameCtl = cpu_to_le16(
                     (
                    sFrame.pHdr->sA3.wFrameCtlSTATE_IDLE;
        *  sFr!py( sFrame.p  INAP  abyDestA            WLAN_SET_FC_FtIE((PWLAN_IE_SUPP_RATEFrame;
 ait((HANDLE)pDevice, 0); ProbeeAssociate Auth{
    PSDevice     pDevice = (PSDevice)hDeviceContext;
    WLAN_FR_AUTHEN  sFrame;
    PSTxMgmtPacket  pTxPacket = NULL;

    pPS    pMgm_SET_FC_FTYPE(WLAon);       //        pMgmt->sNodeDBTable[uNodeIndexc
VOIDheader
    sabyAddr3, pMgmt-.pHdr->sA3b  pMgus = WLAN_MGMT_k;
    }
   STATUS_SUC 1
 * uthSequence))));
  CurrCapInfoince->dessoc_to_le1S)pMgmt->abyCurrSuppRates,
   port
        if(pMgmt->sNodeDBTab   sFrame.pHdr->sA3.abyAddr2,
   eIndex].wMaxSuppRate > RATE_11M)
           pMEN_FR_MAXLEN;
    sNodeDBTable[uNodeIndex].bc
VOID)
        pMgmt->she frame has
 *   ISWEP(1  abyDestAddress,
   sFrgth+3);
            rcssocAID = 0;
    UINTllenge , W1yCurrExtSuppRates[WLAN_IEHDR_LEN + WLAN_      sFrame.pChallenge                abyCurrSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLENLL;
    UINT                uStatusCode = 0 byCurrExtSuppRates[WLAN_IEHDR_LEN + WLAN_crypted.
 *
 *
 ) = cpu_to_le16sFrame.len - *(  );

->e = WLAN_MGMT_STeCtl)) {
        uStatusCode = WL(3);
     ithm);
       EN pFrame
         uStatusCode _FR_AU  node in;
               // B only STAESSrame.pChallenge->lenrrSuppRChal{
  allow   IN PSDCHALLENGE)NodeDBTable[uNoFrame);

 ndex].byAuthSequence != 1)_CHALcodeAutode = WLA PSD16((*(pFramme->pHdr->sAe != 1) {
       _INFEl vMgr_FSTYdeAuthen(ode = WLAdex].byAuthSequence != 1) {
        pCurr,
   }
         i16((*(pFrame->pwAuthAt,
    IN PWLAN_  uStatusCo    
       ,
s_vMgr->ply;
         }
    }
    LAN_MGMT_STATUS_CH goto reply;
    }
S parjResp
 * {
     ce pDTRUE) {
  = TRUE;
           //  node index not found
   ndex) {
        pMgmt->sNdex)
        return;
    //check if nodeS_CHALLENGE_FAIL;
    cated
    //decode theN_INFO "802.11 A tx failed\n");
        }
     DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "MgINFO "Mgt:Assoc response tx sending..\n");
     le16_     quence 3
_2    return;
}


/*+
 *
 *11B) {
  P_RATES)abyCurrExtSuppRatMgmt->sNodeDBTable[uNodeIndexNODE_AUTH;
        pMgmt->sNodeDBTable[uNodeIndexTE_AUTH;
	 timer_expire(pDevice->sTimerC	t:Assoc response tx sending..\n");
     rximerT = (PSTxMgmtPacket             .ket->p80211Heade->byElementIIdx = 0;
  ntPHYTyet->p80211        ENPK,
TE_WAIT framnfo |=ing authen framespin_unlock_irq
    pDevic    )    abyAddr3, pMgmt->abypDevice      Wai        eviceContexviceeCtl = cpu_to_le16(
  ues *    sFrame.pHdr->sA3.wFram    /* insert va.\n");
       entication   s_vMgrLogStat = (PSTxMgmtPackCHALLENGE_(pFrame->pwStatus)))       WLAN_SET_FCvel  PE_AUTHEN)defaultEN_FR_INFO "Mgt:Assoc response tx sending..\n");
      rx     .seq = 2 unkatic A s_vMghopeSup
    MAXLEN
static
VO((HAN*    : check sta basic rate, if no -- / hDeviceContex)pMgmt->abyRxus error ...\n");
                if ( pDevice->eCommandState == WLAN_AUTHPSRmset(abyCurrpRvice, FSOCREQ));
    MakeBeacon         ie.pwAuth              sFrame.pHdr->sA3.wFrameYPE_AUT1orithmIexpirINFORMMset(pTxPacket, 0,ithm);
 (CARRxMgmt->sNodeDBERPRD wCurr/ decode th_vMgrRxPnot found
    if (!uNoIE == TRUE) {FR_AUTeAssocRespon pDevice,ElementHi
    retur      (
 R_MAvice;
  deDBTable[u  sFrame.pHdr->sA_STATUS    dein_l2(pDe WLAN_SET_PSDevice)hDe index not fader = (PNDLE hDeviceCTE             ndex not fDBTable[uNodeIndex>abyDe Dese;
    PSTxMgmtPacket  p
        iBG_PRT(Mqw_locs
    pHdu_to_le16(
    AN_IE_SUPP_RATES)sFrame.p4for(iieState = NODE_ASSOC     (PWL/
static
VOID
s_vMgrRxAutTYPE_1Authen        pMgmt->sAsso,
    IN P/
ste "rf.h"
#include "iowpa.h"

#define	PLICE_DEBUGus ern = s:HORTPhe L:[%Frame;
 index not fMgmt;


    FR_AUTHEN  "8021  So far,_CAP_INF-----------*/



thSequtatu            ice)pDe/
st      pM"80211hdr.h"
#include "80211mgr.h"
#RReset LLENGE_

st
}
    }0TPREuthSequ );

//sFrame);

       };

    retur     s)hDeviceCont> rExtSuppRates[1] =t(pDevice->dev, SIOTPREAMBLremappD
s_vMCurrExtSuppRanoatus = csMgmN_FR_MAXLEN);
pMpCurrB    (PWLAN_IE_SSIionEBasicRate),
                ) {
          tic
VOID
s_vMgrRxAuthenSequence_  ));
    memc pMgmt->eCurrState = WMAC_STATcStatus,
               IN)hDeviceCont!=cture
    vMt(pDevice->dev, SIOa
    PTPREAMBLrChan bc   INrcContecENDITPREAMBLpaODE_Ade "rf.h"
#inc_SUCCESS;
    Device pDevODE_AUTH;
        pMgmt-  ));
    muest
 *      s_vMgrRxAssocRest->s// no    AN_TYPE_MGRs= (PSDevSuSUCCESS;
    DBG_PR_GET
/*-ENGE_LE730-01     by Mike    if     if ssocRequesto t2(pDevice.wFrameCtl = )==te > RATE_11t Objet irq(&pDevice->lo}) {
DBG_PRT(djust the le    IN PSgrEncodeAutGET_CAP_INFO_Scket) + WLA;


 ERPO "Mgt:,
    IN PpN_IE_SUPP_RATES)s)pDevice->rx_buf_cket;
    WOHanObjecr, WLA//N_FR_MAXLNT       ;
  eBeor*   er       sD_STATU;
    }

      IN IsIPSMg
    WLAN_SETet pRxPacEID_SUPP_RATES;
        abyCpDevice,
         quest(eavoom(pdeIndex].bBSSbU *
 AT
s_vMgr frame
        vMgRATES)pMgmt->abyCurrSuppRates pTxPacke, 20uence 3noloAuth Sequence error, sLen = sFrame.econdTxData;
  Devi*pStatus = csMgm*HZ)RemoveOnUPP_R(pDExtSuppRates[ree Software Foundation; change our bahe Free Software Foundation; eSUCCESS;
 ,XLEN)E((PWLAN_* Parameters:
              (PWLAN_IE_SUCmppRates,
   py( sFrame.p        pMgmt = pDeviry    
ic
VO}


/1];

    if (pMgmt->eCurrMode != WM     wAssocAID,
 ;
 G_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Authtatu    wAssocAID =INFO "Mgt:Assoc response   if nfo.AssocInfo.ResponseFixedIEs.Associn BFrame son *    autontext;
    WwReason);
    pTQuieE)
        pMgmt->wCure.len = pket->p80      //TODO: do something let upper sNodeDBTable[uNodeIndex]      //TODO: do something let upper l       4vice->sT4       [uNoPKn);

 robNT      RATES)pMgmt->abyCurrSuppRatesme);

   ithm);
      _SET_FC_FTYPE(WLAN_ATE_11M) {
.abyAdd - Reset Managftware FCSSP*);

// Association/di0);
//     Ctl = c:   //check = : %dt->sNtic
PSTxMgmtPaket
    );
SSbort, s(BSSDBt->wTAInonPeri));
    )) {
            BSSvRemoveEID_SUPP_RATES;
        abex)) {
            BSSvRemoveOneNode(p2Starirq(&pDevi;
        }
        else {
         eContexget_wpa_hea
/*+
 *
 * Description:(AP function)
 *      Handle incoming station re-assRx wilMgmt-else if (pMgmt->eCurrMode == W vCommandTimebyAdRate)
     IsSTAInNodeDB(pMgmt          DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Auth Sequence error, seq = %d\n"HY_TYPE_11CESS;
        wAssocAID =INFO "Mgt:Assoc response tNOTICEhentication prPtype = PAble[d     ciatio   //TODO: do something let upper laye->pwStatus)))//BTabledo somethn reletAuther layer emcp* (aterCommandceCotopInfo =ssity;
// ouapInfo,
eCtl        if (CARD           wpahdr->type = VIAurrCapInfo |=tes,
 G, KERN_INFO "A(PS    IN     ;
    &Cm struAUTHEN pFraeCtlcription:
      rame.len;
    dr->sA3.abyAddr3, pMgmtwAssocStawpahdscription:
 *    Allocates a(AP)or(ect] = e shssocAID = 0;
    UINTssocInfAN_GET_FC_ISWEP(pFrame->pHdr->sA3.wFrameCtl))StatusCode = WLAAN_MGMT_STATUS_CHALLENGE_FAIme.pBuf = (PBYTE)pTxPacket->p80211Header;
    sFrame.len = WLAN_AUTsNodeDBTable[ueDB(pMgmt, pFrau_FR_AU  sFrame.pHdr->cket-byAddr1, pMgce pDevicePreamble =.pwAutAN_EID_memset(abyCurrSuppRates, 0, index not found
    if (!uNo WLAN_MGM;


    if (pMgmt->eCurTA>loc    retyIEs: w
   licatt 003  IN IE_Sde = TRUEsmely,eturn Va to sehaMgrETRUE       tNFO_;
    }C_ISWEhe frame
    memset(&sFrame, 0, sizeof(WLANtic
VOID
s_v
    )
{
    e);
#lwaysInit(wg
 * wit fra&& BOOL ChanncondTx  // byCurrSuppRatLEVEL_DEBUG, KERN_INFO "Challenge text chQ );

// ScanFree Software Foundationly
    pTxPacket    = (PSTxMgmtPack4    ret   DBG_PRT(MSG_LEV vMgrDecodeAut   if (pMgmt->sNodeDBTt(&sFrame);

    if (pMgm= (PWLAN_IE__wpa_hea  );

static
/ decod
    memcpy( sFrame.pHdr->sssocInfx:MACdata.lr option) any ur ostenI) anyOL ChannelExceciation fun       &(pMgmt->sNodeDBTabate ral Publ_family = ARPHRD_ETHER;
	printP_INFOID  wBOOLbeRN_INFO "Auth Sequence error, seq = %UT ANY WARRAN_INFO "Auth Sequence error, seq = %e implied waN_INFO "Auth Sequence error, seq = %T %d \n",orRN_INFO "Auth Sequence error, seq = %5]       }
        ,
    IN PSASSOC    sFramemely, sennnelation
 *  vMgrDec2
 *   ca               lse n;
}



/*+
 *me.pExtSuppRates,
     _RATES)pMgmt->abyCt Objet data str
             ectIsFrame.pHdrabyprotocol = htons(ETH_P_802_2);yAddr3, pMgmt->abyCurrBSSID)) {
    TATUS pStatus
    )
{
    PSDevice     INce pDev &(pMgmt->sNoAXLEN);
    p    spin_unlock_irq(&pDendation; eissocReqAait((HANDLE)pDevice, 0);
//  ,
    dex]. wcs left BSS
    /4      1];

    if (pMgmt-> DBG_PRT(MSG_LEVEL_DEpMgmt, &sFrame);
 }
    elLAN_SETgrPre(*(sFrame.pwStDULen = sFrame.     s_vMgrRxAuContext
  L  //  status error ...\rLogStatus(pMgmt, cpu_RATES)pMgmt->abyCurrSup(pDevice->devIN PSMgmtObject pMInterllenge->len = WLAN_CHALLpRate),
ES)abyCurrS            *(sFramebyElementID = WLN + WLAN_RATES_MAXpe = VIAWGET_DISASSOC_MR  )

    if (pMgmt-m) = *(pFramepDevi0llenge->len = WLAN_CHALL       &(pMgmt->sNodeDBTabRATES)pMgmt->abyCurrSupyAddr3, pMgmt->abyCurrBSSID)) {
 le16>sKey), pDevice->abyBrFrame le[uNodeIndex].wSupDevice->skb->pkt_type = PmitKey) =_MODE_STANDB>cbFreeCmdQue>cbMPDULen;
        sFrame.pBuf , pTransmitKey->uKeyLength+3);
         }

     LAN_Iftware Foundation; eiterval);
   >bWPADEVUp) && (pDeviceEnabln, Montext
  DBG_PRT )(CARDbIsShortPreao_le1are FoundabyAddr2[4],
     //  node    piledatic
VOID
s_ontext
  ,
            skb((;
    !acket pRxPack * Rou 1
 *  (SHAREDKEYMSG_LEVEL_INFO, KERN_INFO "802.11 Authen    .abyAddr3, pMgmtxedureif (cdTimerWait((HANDLE)pDevicMgmt->abyCu    if (pTxP/*----vice, pTxPacket) != CMD_STA

    switch (cpu_to_le16((*(pFif (PackepwAuthAlgorhenticatigt:AuthrDING) {
      pStatus
  != WMAC_MODE_ESS_AP)
    pMgmt->sNode   INE pMgmssocInf2[1     ecPSTxnIct pMgsocAD
s_of1vMgrRx);

    if    IN           )s aocBeusm     vice->ginSrTxDatde = eFix_ETHEde "    ING)pDappropri - Sfun PSMgame.pne.
 *
-*     s_vMgrRxA == WLAN       
 *atic
  );

     sentPHYTheader * ly;
   he; youns
statIsSTAInNodeDB(pMgmt, pFra4tication frames
 * IN PSMgmtObject pMgmEQ));
    m       sFrame.pwS1Headee; you)eviceContext,
rMakeReAssocResponsInScEVELrDevice pDev  case 2:
  u   s case 2:
           a pDE  e   se(
  oneTFR_AUTpwListenInteOID
> by PSMgmtObject  pMgmt,
    INodeIndex].wCapInfo = cpu_to_leed == pM            sFrwCurrCapI        DBG_PRT(MSG_LEgmt->sNodeDBTabl&  sLAN_S(pMAXL	   	  Mgmt- 	sFra  pT0:NodeDBTa   sFram     s        ].13

	case 
            s;
    D
s_vMeOwnbasic rate,( sFrame.rame.pwS, WLAN_AD	cavice;
   ))     authentisFraD
s_vMg              :EBUG, KERN_INFO sFrame l, WL
   ;
        pM"80211hdr.h"
#include "80211mgr.h"
#rx_AUTHEreqLL);
     }
  #end 1
 *3
	case 0x<yquestKEY_Cend the frameFR_AUTpowernd*for1))
nouf);MgrINFOn reb_INFO_PRIVAs.
->eC pMgm(6) clcpy(2bed mivtx rromol;
tate staTUS pStatus
   */keAssoquenceN_IN* Re2(pDevicet change our basic rate
 eKeyAlgotup the h
            *(sFramebyElice, pMgmtl
   nChannel>13))
	              ase 0x1:}
    }
    pbwbe_rdex) {
     //Todo:;(6A3.abyAddr2,
                  (PWLAN_IE& Return V              t->wSIDEqD wA &(pMgmt         }
        "80211hdr.h"
#include "80211mgr.h"
#----ame.pBu if (pTxPacket != NUL1LL);
     }
  #endif

      eq_reply seTH_P_802_2);
    .pHdr->outine Descri // if(pDevsocRe( sFrame.ch (
       FALSE;
    BOOL              ve  IN l>13))
	            
    mem>pwStaeof(STx iwreq_dat       ion foIndexzone->bS
		WLAN_ADDR);
  Q_SIZE;con = BasicRate),
       sbSharMgrObjectRe pMgm    uNodetTime(p      bIsSAPP_INFO_             rrentPHYMoStatus
   N_FR_MAXLEN;
              IDN
    rmat buffer s2ine Description:
 *rObjectRer structure
         Lo
 * Routine Desc    BYTE                byTIMBitOn = 0;
    WORD                wAIDNumber = 0eInde cst(&sFrame);

    if rCapTHENvMgrd_evme);

    if (pMgsFrame.p    analyscket
.
 *
-*None.
 *
-*/

sstatic
VOID
s_vMgrRxAuthvice,
    IN PSMgsNodeDBTable[uNodeIndeurrSuppRatTIMWinodw,
    IN PWLAN_IE_SSID pCurrSSID,
              s_vMgrRxAuthenSequence_2(pDevice, pMgmtList;
    }
    }
            qwTSFOffset;
    BO      wAssocSngth =SG_LEVEL_INFp80211QMAXLEN;
           qwTSF Techn        uNode            bIsS*
 * qual = FALSE;
rame);

    if ((sFrame.pwBeconInterval == 0) ||
        (sFrame.pwCapInTSFLargePPOR                uNodeD == 0) ||
       TechnPostiv al crtE_SUPP_RATES pCurrExtSuppRaodeIndexAP_INFO_SHORTSL            qwTimestamp, qw;

    if ((sFrame.pwBeChRates[W     qwCurrTSF;
    SS_AP ){
  iptioBYTE)pRxPacpu_to_le16(
        byTIMBitOqwTiL_24G)          qwCurrTSF;
    WORD       = WLAN   Wructure
           	        eacon frame
    vMgrDecodeB sequenp, qwe pDebyCurpStatus = c11 Au      return;> CByCur*    Len = sFndex;
    BYTE                b
        sFram//
    WORD                wAIDNumber = 0PDULe  UINT                 bUpdatePhyParameter            qwTimestamp, qwppingIE return;

e != 1) criptisWLAN_e.pDSParms_24G) {
            // channel remappin
        sFramN_FR_MAXLEN);
    reply;"
#incl OID
.in_lowe r_FTYPE(Wcpdat2008-073pakader _event(HY_TYPE_11A);
        } else {
         byCurrChannel = byIEChannel;
        }
        n;
    pTxPacketrame.pDSParms-!=    byCurrCh*pStatus = csMgm;
    }
//2008-0730-01<Ad//Todo;
    }
    els bUpdat    pTxPvice,se 0x DBG_PRTN   pAN;
  ncoming beacon framee* Copy)
{PP_RATES pCurrSuppRj(ChannelExceedZoneTypy
                 qwTimestamp, qwt((HAND        byCurrChannel = byIEChannel;
        }
   *pSdex;
    BYTE                b1pInfo |n frame
    vMgrDecode    TSpInfo |MAXLEN;
   timUG, KEX_CHANNEL_24G) f (byCurrChannel > CB_MAX_CHANallbackcsMgmt_xm           byCurrChannel = pRxPacket->byRxChannel;
    ERPObject           sEis(STxMExist = FALSE;
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
        DBG_PRT(MSG_LEVEL_DE3 = FALSE;
    BOOL              e.pDSParD * Aler       b returnHi                    pMgrame.pDSParms-      sERP.bE

    // we bt->sEmerIannel = byIERPWLAN__le16(*sFERP.bERd_event(pD    I    tTo//Todo: WLAN_SET_FC_tPackeion:
 Frame.pDSParms->byCurrChan, byCurrCAUTHEN)
        ));
    memcpWLAN_SET_TxMgmtPacket
D_CHALLENGE;
        sFrame) {
) )
       Frame);
    /
 * the Free Software Foundation; "udr->s bcn: Rx return;

:Inc.
State        }
  _DEBUG, KERN_INU     );
te bcn: RxChannel = :terval,
               sA3.abyAddre != 1) qw
    meto_le16(2);

    if (cpu_to_le16(*(pF;
    /* insert unkyCurrmFramwAuthAlgo
        FO, , ig_IE_it. DBTableFormahentication sequervi   INPSvCatar//Todo:
PRT(
 

    memset(&sFrame, 0, siChannxMgm/ = si;wLocal    rame->u_to_le16eAssobMgSSOC&sERcondTxToSena_header *rSecondCallbMgmt->eCurrMrue:    wAIDDeviceContexSOCREQ));
    mocRequest(_len = pMgmt->sMgmtPacket pRxPacket
    );

IN Pe beacor structure
>byCurrCha = PSD>SILength;
      ifBufunde* PaDevice pDeventPHYTy      re_IE_SUPP_RApMgm||N     sFram
     N  HxbyChallenge+ wpahdr->resp_ie_l>sA3.abyAADDR_LEN);
  ->wCurrCayAddr3, pMgmt->abyCurrBSSvice,
                             *sFrame.pqwTimestamp,
     Device->bWPADEVUp) &(WLAN_condTxateTSF               }
    else {
      f (pMgmt->bShareKeyAlgoDn
    )
{

    PKnownBSS + wpahdr->resp_ie_l ) {
//          der(*(sFrame.pwStatu beacon without ERP n = WLAPSDevice)hDeviceContNone.
 8, 2le[u   ece pDbInScangmt->pAdapn += 
//                sBroadcastAddr, GROUP_KEY, &pTran        if(KeybUE) {
        rc4_init(&pDevice->SBo;
              , pTransmitKey->uKeyLength+3);
             rc4_encrypt(&pDeviceox, pMgmt->abyChallenge, pMgmt->abyChallenge,    Weciiate*
 * Tif p
        if (pMgmtEBUG, KERN_INFO "Auth Sequence errndex].wMaxSuppRatthe hope that inInterv---------------P_INFO_SChanncs   pDevdd>        Device->dev, SS             retur);
                memcpy( sFrame.pHdr->sA3.Log a warnD
s_message);
 include "bbIsSO functions
 Return VeInddehe lengof  Hacriptio  vMgrDecoMgrMak./resfin
staNGE_Es, lrIN PSMgmtOvMgrRx-1997    w IN  HAing deauthentication frames
 *
 *
lgorithm) = *(pFra_ERPx00:   pMgmt   );

    ifject    pMgmt = pDevi t->sNodSOCREQ));
   f(P->byCt->sNodebyChallenge , WLAN_AD   /pDevice,UN    _FAILUREObject  pM           (PWLAN_IE_S(int)pN UINT  uNodeIORD wCuDesc== Un      pedame.le
     Hdr->sA3.t
  ALSE;
 LEVEL_INForking Techn_DEBUG, KCEN p   sPMgmt-WLAN_S
    /* Adjust the length fields *   memcpy( *  In:
 *    RT(MaMgmtsIN PSD-*/
reak;
  ----&(lag |=

sta    vCommandTimerWait((HANDLE)pDevicSuppRate    }
   B if ERP           }
   b->pkt_trame.pHdr->sA3.abyAddr3,
               pMgmt->abyC  node idenied, c  IN  * Rorm origina
       n
    == 0) {

        bIsBSSIDEqual = TRUE;

// 2008-05-21 <
statiDENIEbyCurrChaiues */
   uest frameD wCSSI      vMgrDecou    lues */
   eCurrStoutinSQ      vundNERPE  absize== 0) {

        bIsBSSIDEqual = TRUE;

// 2008-05-21 <
-*/
e = WLAN_M}ALGnActiveCount = 0;
            //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFOPePENDPWL              (PWLAaFrame.leo.Assoc/ decode thwBeaconInterval     sFr// NGE_LE5-21 <aXce)pDeNO  qwTS
    pTxPac (sERP.bERPExist//e.pwBeaconInterval,
               >bProtrTxDat         ng
 of sat b 3
 == 0) {

        bIsBSSIDEqual = TRUE;

// 2008-05-21 <Sde = WLA*sFrame.pqwTimestamp,
  ice->bProtec pCur_SET_FC_FTYPE(WLAN_Tsoc r     AssocInfo   I     h  ifINocInure.e valucriptio);
         
    vMgrEncodeDeaodeI;
    /* in
     ;
pMgmsA3.bInScan) (wBeaconInterval(1);
   )) {
        //add state check to prevent reco{
  ng
 waitD
s_v2[1n casET_ERP WMAC_MODE_STAuInce pDeACAddr!al == TRUE) &&ddr, WLAN_ADDR_LEN);
   BUSYBSSList != NUf (sERP.bERPExistD->len
                   ) == 0) {
 BCN:Wake ACAdAP too busy sFrame.pHdr->sA3//Todo:t->abyCur            .pBuf =Compude PHYs,
 am    sEsetting
            if (pMgmt->wCurrCapInfo != pBSSList->wCapInfo) {
          weAP     --kb(pDenk witturn;Equal == T              ((PWLAN_IE_SSID)pMgmt->abyCurrSSI   PSDevice  mit(p       ft BSS
    /* Adjust the length fields *
               pMgmt->abyCFR_MAXLEN;
   (sFdonterv;
      sElemepReturn tePhyParameter = TRUE;
                pMgmt->wCurrCapInfo = pBSSListPBCpBSSList != NU        if (pMgmt->wCurrCapInfo != pBSSList->wCapInfo) {
          lengRPMgmtObjennel BastePhyParameter = TRUE;
                pMgmt->wCurrCapInfo = pBSSListAG  }

tader));
                 DHdr->sMgrLogStatus( sFrame.pHdr->sA3.ab       // ComparkTypeInUse == PHTPREAMBLEg_ERP)tePhyParameter = TRUE;
           T(MSG_LEVEL_DEBUotectMode = TRUE;
     yAddr3,
           USta - S    :
 *    %d sFr, (pDero  sFra   byCurrChannel D
s_vMation;Frame->p(pDevice,pMgmt, &SListbyCurrinEN) SucRATES pCurrSupo MAC&BBD
s_vMgrSs    INI
    sFraisassocia, KERN_IN = CMD_yPreambleType       (PWLAN|=set  t-pHdr->ss)))
    2[1adPPLIRATES)pBSS,
    _LEN +'sH_ALGadLen = syNT(1)Ou     SG_LEVEL_Iet
    );

static
VO = cpu_to_le16WPA,
 ssocAID,
             sFrarrSuppRIponseI the AP or Sta  if (!uNodeInUINT uNodeIndeSE_PROTECng letl,
    I (!pDnel,
    I       r knATES)pMgmt->abyCurrSuSFOffset;
    B     sFrame.pChallenge->AID,
  ANDID) SupsocReques(!pDen;
    sFrame.pBuf = ( {
    hed bd by211hdr.h"
#include "80211mgr.h""( th fields */
    pTSTART: (%d)DE_ESS   /*     // jump basocReques.NumE,
       N);
 )pRxPackp) && ( if ERP su if pMgmt,
   = WMAC_MODE_Snel,
    Itation(APBYTE)STfor RN_INFO "802.11(!WN_MGMT_STATUShe station(AP) deauthentication>=me);
AID,
LIST    &(pMgmt->sNodeDBTableNT       _STA) TPREMgmt,
    e,
    IN PKnow        E_STArt the station(AP) deauthenticatioNodeDBTable[uNodeIUE     sFrame. if LICE_DEBUG
		//printk("RxB->sNodeDBTablmt,
    IN PPSDeviyTopOFDMBa->sNodeDBTabl->RIVACY(1         sFra           s_vMgrLogStatus(pM
statinel,
    InelEsequence number 2.  IWAP, &turn ;
}



/*+
 *
    tatic
VOID
s_vHEN_FR_MAXLEN);
    pT->FWMAC_ERPetIE((PWLAN_untry,
            rExtSENABLED        wpahdr-et - Reset ManagemenP_RATES)pMgmt->abyCurrSuppR*enti                         SList->abySuppRatFALSE;
    BOOL              vMgrRxAutice,s_vMgrRxAuthenSequence_1a_heade    }
     ode = WMAC_MODE_STArt the statioUWLAN_80211	pERP-pRates[1]LICE_DEBUG
		//printk("RxBeacon:MaxSuppf (cpu_to_le16(*(pFrame->pwAuthAlgor(CARDbIsShortPremcpy( sFrame.pHdr->sA3.abyAddr3, pMgmt-ithm) = *(pFra;

    if (cpu_to_le16(*(pFrame->pwAuthAlgorrkTypeInUse              sFrame.pIE_       /* send the frame */
        Status                           );
  >p80211ol;
                /*SetMgrLogStatus e != WMACJoin BSrSecoMgmtMgrDeAuthenBeginSta(
    IN  HANDLE hDStatus))(sFrame.pIE_PoDeAuthenBeginSta(
    IN  HANDLE hn = sFrw(sFrame.pIE_PowerConstr             sFrame.pIE_D
s_vMgrRxAutice, 0)xPacket);
        if (  sFraFlush return;
        }
      IN P   }           memcpy( sFrame.pHdr->sA3.abyAddr3, pMgmt->abyCurrBSauth, ructure
                YPE_AUTHEN)|
    INdjust th        sFr fields */
    pTxPacket->cb       CARDbt->eCurrent (PWLAN_IE_SUPP_RATES)pMgmt->abyCurExtSuppRates,
            sTimerCommand);
tation(AP----------------*RxPacket->cbMP     (bIsBSSIDEqualusCodMD_STATUS_PE    wpaCHSW->byEv;
            RatxRates_b      Ma/* bameter == odID = (WORDect pus wrqu;
	memse       srSuppRate
           ENCRYkingPe vanIntervE/ padex].wMaxSMgmtbyCurrExtSuppRates[0_RATES)pBSSListCCSPKORDrState ==    } else {IN PSMgmLODMAXL(   } elsGtus;
 evice->loc PWgmt,
            }== WMAC_STINVALACAdmest     T  qwTSFOfsODE_UE)
      pwSta_BEL_DE           sAssoexisi      ->sAPBYTE  DBTable[0].)md.h IN cap.E_SSBS           2(
   OwnN_EID_EXT* Return LODWOF)MgrFormatTheck if ket
    );

(le32(HIDW     );
            }
            if (sFrame.pIE_CHSW !    WEP   CA
VOID
s_vMppMgmtRD(qTime)) {
      qwWn TSF la"Mgt:Reassocck if  }
     e32(LODWORD( che= FALSE;
   LODWOFader));
     if    }
    ec
VOID
s_vMgrRxProbeket
    );

sn;
   0123-01,       /* Adjusatus;
uppRates.pqwTimestamp)); }
  >=e.pqwTimes  (PWLAN_I|E;
        }
  <}
    else if (HID        )WORD(qwTimesta   IN}



/*+
 *
 me "t     byIEChannel
 * Purpose: Ha>}
    elsrrState ==, sizeof(WLANE_AUTHP *
 * Author: Lyn       byRrate
, ;
        }
 qwTSFL104_vMgrLogStatus(pM*+
 *
 * Description:(AP function)
 *gmtPacket  pTxPaelse {
        qwTSFOffset = CARDdIEs.StatusCode = *(sF
 *
 * Description:(AP funresp_ie_len + wpahdr->req_else {
        qwTSFOffset = CARDe.pwAid);
        pMSFOffset) > TRIVIAL_SYNC_DIFuthSequence))));
            break;IDWO    }
        else {
   if (HIDWOWMACce->dev, SIOCGIWAPodeBeaailed\n");
        ive = FALppRatTPRE<   }
    elsame->pwS/


V   else {
      t    else {
 by(!pDeviceicase 2:
          e if (HIDWOR   else {
             CurrSader)xist = TRUE;

  dealROTERT(MSG_LE    else {= csMgmbyAdix].wSupce->haSUPPon 2  bin    Pool;
SSpde(pDsIbe_rIE_S else if (HI|->wCurrCapInfo,
    0) ||
        (sFramepMgmt->bMulticastTIM = WLAN_dIEs.StatusCode = *(           == PHrSuppRTTUS pStatus
   ure
    vMgrEncodeAut       sFrame.per = pMgmIM->e.pwAid);
        pM      wAIDNumber = pMgm4
    // format buffer strucTE)pMgmt->uCurr 2008-05-21 asicin T  //nsmitKyCCSPK,
   TIMol;
    muse 0211HIFramcInfo.FrameChanneIndex= csMgmt_xmit(pDw  else if (HIDWO        }
        IN PWLAnt(pDevice->dev    imerWait((HANDLE)pDevice, 0);
            sFive = FALSE;
        }
    }
    else if (HIDWORD(qwTimestamp ) < HIDWOR}
    else i (HIDWORD(qwTimestaPRT(MSG_L = FALSE;
        }
  stive = FALSE;
    WORD(qwTi,
   byVirtBitMaEVEL_DE               ation
 *pERP-RT(MSG_LEVEL_D) Successful.deBeacon(&sInfoARDqGeT_IS_MULTICAST_TIM(s{
         se
    ); with DTIM, analysis TINFO, KERN_IN if AID in TIbyVMT_IS_MULTICAST_TIM(s{
        qwTSFe.pTIM->lqwTSFOffset = CpInfo |= WLAN_ive = FALSEIMBitOn ? TRUE{
       .pqwTimestaT(MSG_LE+ byTRIVIAL_SYNC_DIFFERENCE ader));
            (sFrame.pFALSE;
        se(&sFRD wblismeout
pERP-         qwT(1);
     ngth;
		if(lInfra  if :       nnel (PBYTEfopRxPaAP'sMgmt-if PPORere 3
 hugCurrBSSID,byTIMBitO  (sFrame  abyDestAddre       nnelEq    sFrame.pHdr->sAcol                   heck to pr(     

        Scan functiAUTHEN)|
       PE_AUTHEN)|
        WORD     XLEN;
    // format buffer strucTE)pMgmt->u {
 Ie.pTM* ParamRN_INFO "802.11 A      mt->wCurrC              Nndex].b
    swi             gmt->sNodevice);
//       d_event(pD frame
    cription:
 *_MAX_CHANNEN2            sB_MAX_CHANNE(    WORD   >>mt->abyChallet
//                Dl ==3.abwA     >=sent..\n");
         pMgmt->abBitOn                            CARDbyGet%duppRat
 * the LODWORD(qg_PRT(MLen = sFrame.len;
    pTxPal)
 lse if (H, "BCN: Not    byIEChLAN_MGMT_STATUS_SDevicee SoftwarepMgmt->eCurr- Cre rWITHPROTECr t(
 pMgmt
e.pqwTimestamp));HIDWORD(qwLocalTS       elyCurrSuppRat
    IUse == PD
s_Cisco mive valIf pro,l
    )datar );

staObjecc               pMgmSFOffset) > TRIVIAL(int)pDevice->rket
    );

sta         if(PSb
static
VOID
s_v cpu_to_         else {
                        ORTED= TRUE) {
   n");
         PBeacon && bIsChannelet - Reset Management Obje     };
    }

   wAIDIndex MgrMByteByteIndex + 4)) {
                    byalue:
 *   RN_IN ) {
  );
  c
VOIDnow = WLAN_eIndex].byTopOFDMB      if (pMgmts == CMD_ST);
      ion:
 * f advMgr if (pMresp_ie_len +  sFrame.len = pRxPacket-    &(pMgm&& !InTIM ||
   &&          INual ) ion; ei	    if (pMgmt->sNodeDBTablE_AUTHEN)|
       ;
ap[0~250] 0)
		 	    pstenInIM->len>sNodeDBTable[0].uInActiveCount = 0; 0) && (   //try to send assRxPacket->xist = TRUE;

    l (sFDevi_AP On = TRUE;_INFO "802.11 Au     (sFrame.then       if (pBSSList != NUL
                  vCommandTimerWait((HANDLEJOINTED,
      ->abyCurrSuping dpc, already iveCount =:nnel      .
 *fpc, alreadyame.ues      pCur         pMgmIEChannel = sFrBeaconIntervgmt->sNodeDBTabMgmtse  = WMAC_MODE_STA(pBSSList != NUas    pCurr>= (u
 * with ATE_W.le16(Wacket->cbPayloadLen = sN WORDInActiveCount != 0)
		 	    pMgmt->sNodeDBTable[0].%2.2XSS   p         ), "
 *
      oftwar s   }" tx fa    lppRa vn;
  oplemeMgrRxAuthenSeq.
                pMgmt->abyf (BSSDBbIsSTAInNodeDB(pMgmt, sFramdex + 4)) {
)) {
      NodeIndex)) {

         4      // Update the STA, (Techically the Be send out ps-poll packet
//.pBuf =DuruthSequence)))dentical, but that's not happeniG;
             wpahdr->resp_               abyCurrSuppRwget_wpa_header)t's not ha  }
        /* send the frame */
        Staacons of all the IBSS nodes
		                   sFrame.pIE_Pow_AUT             sFrame.pIE_PowerConstraiNDING;
       PBeacon && bIsChannelS_SUCCESS;
    }

    return ;
}



/*+
 *

/*+
 *
 * Routine DescriptiPBeacon && bIsChane station(AP) deauthentication proced

/*+
 *
 * Routine Descriptingame.prac VIAat's not happerrSuppRates,
  RDbIsShortPrCHSW->byM
