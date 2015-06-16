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
 * File: rxtx.c
 *
 * Purpose: handle WMAC/802.3/802.11 rx & tx functions
 *
 * Author: Lyndon Chen
 *
 * Date: May 20, 2003
 *
 * Functions:
 *      s_vGenerateTxParameter - Generate tx dma requried parameter.
 *      s_vGenerateMACHeader - Translate 802.3 to 802.11 header
 *      csBeacon_xmit - beacon tx function
 *      csMgmt_xmit - management tx function
 *      s_uGetDataDuration - get tx data required duration
 *      s_uFillDataHead- fulfill tx data duration header
 *      s_uGetRTSCTSDuration- get rtx/cts requried duration
 *      s_uGetRTSCTSRsvTime- get rts/cts reserved time
 *      s_uGetTxRsvTime- get frame reserved time
 *      s_vFillCTSHead- fulfill CTS ctl header
 *      s_vFillFragParameter- Set fragement ctl parameter.
 *      s_vFillRTSHead- fulfill RTS ctl header
 *      s_vFillTxKey- fulfill tx encrypt key
 *      s_vSWencryption- Software encrypt header
 *      vDMA0_tx_80211- tx 802.11 frame via dma0
 *      vGenerateFIFOHeader- Generate tx FIFO ctl header
 *
 * Revision History:
 *
 */

#include "device.h"
#include "rxtx.h"
#include "tether.h"
#include "card.h"
#include "bssdb.h"
#include "mac.h"
#include "baseband.h"
#include "michael.h"
#include "tkip.h"
#include "tcrc.h"
#include "wctl.h"
#include "hostap.h"
#include "rf.h"
#include "datarate.h"
#include "usbpipe.h"

#ifdef WPA_SM_Transtatus
#include "iocmd.h"
#endif

/*---------------------  Static Definitions -------------------------*/

/*---------------------  Static Classes  ----------------------------*/

/*---------------------  Static Variables  --------------------------*/
//static int          msglevel                =MSG_LEVEL_DEBUG;
static int          msglevel                =MSG_LEVEL_INFO;

/*---------------------  Static Functions  --------------------------*/

/*---------------------  Static Definitions -------------------------*/
#define CRITICAL_PACKET_LEN      256    // if packet size < 256 -> in-direct send
                                        //    packet size >= 256 -> direct send

const WORD wTimeStampOff[2][MAX_RATE] = {
        {384, 288, 226, 209, 54, 43, 37, 31, 28, 25, 24, 23}, // Long Preamble
        {384, 192, 130, 113, 54, 43, 37, 31, 28, 25, 24, 23}, // Short Preamble
    };

const WORD wFB_Opt0[2][5] = {
        {RATE_12M, RATE_18M, RATE_24M, RATE_36M, RATE_48M}, // fallback_rate0
        {RATE_12M, RATE_12M, RATE_18M, RATE_24M, RATE_36M}, // fallback_rate1
    };
const WORD wFB_Opt1[2][5] = {
        {RATE_12M, RATE_18M, RATE_24M, RATE_24M, RATE_36M}, // fallback_rate0
        {RATE_6M , RATE_6M,  RATE_12M, RATE_12M, RATE_18M}, // fallback_rate1
    };


#define RTSDUR_BB       0
#define RTSDUR_BA       1
#define RTSDUR_AA       2
#define CTSDUR_BA       3
#define RTSDUR_BA_F0    4
#define RTSDUR_AA_F0    5
#define RTSDUR_BA_F1    6
#define RTSDUR_AA_F1    7
#define CTSDUR_BA_F0    8
#define CTSDUR_BA_F1    9
#define DATADUR_B       10
#define DATADUR_A       11
#define DATADUR_A_F0    12
#define DATADUR_A_F1    13

/*---------------------  Static Functions  --------------------------*/

static
VOID
s_vSaveTxPktInfo(
    IN PSDevice pDevice,
    IN BYTE byPktNum,
    IN PBYTE pbyDestAddr,
    IN WORD wPktLength,
    IN WORD wFIFOCtl
);

static
PVOID
s_vGetFreeContext(
    PSDevice pDevice
    );


static
VOID
s_vGenerateTxParameter(
    IN PSDevice         pDevice,
    IN BYTE             byPktType,
    IN WORD             wCurrentRate,
    IN PVOID            pTxBufHead,
    IN PVOID            pvRrvTime,
    IN PVOID            pvRTS,
    IN PVOID            pvCTS,
    IN UINT             cbFrameSize,
    IN BOOL             bNeedACK,
    IN UINT             uDMAIdx,
    IN PSEthernetHeader psEthHeader
    );


static
UINT
s_uFillDataHead (
    IN PSDevice pDevice,
    IN BYTE     byPktType,
    IN WORD     wCurrentRate,
    IN PVOID    pTxDataHead,
    IN UINT     cbFrameLength,
    IN UINT     uDMAIdx,
    IN BOOL     bNeedAck,
    IN UINT     uFragIdx,
    IN UINT     cbLastFragmentSize,
    IN UINT     uMACfragNum,
    IN BYTE     byFBOption
    );




static
VOID
s_vGenerateMACHeader (
    IN PSDevice         pDevice,
    IN PBYTE            pbyBufferAddr,
    IN WORD             wDuration,
    IN PSEthernetHeader psEthHeader,
    IN BOOL             bNeedEncrypt,
    IN WORD             wFragType,
    IN UINT             uDMAIdx,
    IN UINT             uFragIdx
    );

static
VOID
s_vFillTxKey(
    IN  PSDevice   pDevice,
    IN  PBYTE      pbyBuf,
    IN  PBYTE      pbyIVHead,
    IN  PSKeyItem  pTransmitKey,
    IN  PBYTE      pbyHdrBuf,
    IN  WORD       wPayloadLen,
    OUT PBYTE      pMICHDR
    );

static
VOID
s_vSWencryption (
    IN  PSDevice         pDevice,
    IN  PSKeyItem        pTransmitKey,
    IN  PBYTE            pbyPayloadHead,
    IN  WORD             wPayloadSize
    );

static
UINT
s_uGetTxRsvTime (
    IN PSDevice pDevice,
    IN BYTE     byPktType,
    IN UINT     cbFrameLength,
    IN WORD     wRate,
    IN BOOL     bNeedAck
    );


static
UINT
s_uGetRTSCTSRsvTime (
    IN PSDevice pDevice,
    IN BYTE byRTSRsvType,
    IN BYTE byPktType,
    IN UINT cbFrameLength,
    IN WORD wCurrentRate
    );

static
VOID
s_vFillCTSHead (
    IN PSDevice pDevice,
    IN UINT     uDMAIdx,
    IN BYTE     byPktType,
    IN PVOID    pvCTS,
    IN UINT     cbFrameLength,
    IN BOOL     bNeedAck,
    IN BOOL     bDisCRC,
    IN WORD     wCurrentRate,
    IN BYTE     byFBOption
    );

static
VOID
s_vFillRTSHead(
    IN PSDevice         pDevice,
    IN BYTE             byPktType,
    IN PVOID            pvRTS,
    IN UINT             cbFrameLength,
    IN BOOL             bNeedAck,
    IN BOOL             bDisCRC,
    IN PSEthernetHeader psEthHeader,
    IN WORD             wCurrentRate,
    IN BYTE             byFBOption
    );

static
UINT
s_uGetDataDuration (
    IN PSDevice pDevice,
    IN BYTE     byDurType,
    IN UINT     cbFrameLength,
    IN BYTE     byPktType,
    IN WORD     wRate,
    IN BOOL     bNeedAck,
    IN UINT     uFragIdx,
    IN UINT     cbLastFragmentSize,
    IN UINT     uMACfragNum,
    IN BYTE     byFBOption
    );


static
UINT
s_uGetRTSCTSDuration (
    IN PSDevice pDevice,
    IN BYTE byDurType,
    IN UINT cbFrameLength,
    IN BYTE byPktType,
    IN WORD wRate,
    IN BOOL bNeedAck,
    IN BYTE byFBOption
    );


/*---------------------  Export Variables  --------------------------*/

static
PVOID
s_vGetFreeContext(
    PSDevice pDevice
    )
{
    PUSB_SEND_CONTEXT   pContext = NULL;
    PUSB_SEND_CONTEXT   pReturnContext = NULL;
    UINT                ii;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"GetFreeContext()\n");

    for (ii = 0; ii < pDevice->cbTD; ii++) {
        pContext = pDevice->apTD[ii];
        if (pContext->bBoolInUse == FALSE) {
            pContext->bBoolInUse = TRUE;
            pReturnContext = pContext;
            break;
        }
    }
    if ( ii == pDevice->cbTD ) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"No Free Tx Context\n");
    }
    return ((PVOID) pReturnContext);
}


static
VOID
s_vSaveTxPktInfo(PSDevice pDevice, BYTE byPktNum, PBYTE pbyDestAddr, WORD wPktLength, WORD wFIFOCtl)
{
    PSStatCounter           pStatistic=&(pDevice->scStatistic);


    if (IS_BROADCAST_ADDRESS(pbyDestAddr))
        pStatistic->abyTxPktInfo[byPktNum].byBroadMultiUni = TX_PKT_BROAD;
    else if (IS_MULTICAST_ADDRESS(pbyDestAddr))
        pStatistic->abyTxPktInfo[byPktNum].byBroadMultiUni = TX_PKT_MULTI;
    else
        pStatistic->abyTxPktInfo[byPktNum].byBroadMultiUni = TX_PKT_UNI;

    pStatistic->abyTxPktInfo[byPktNum].wLength = wPktLength;
    pStatistic->abyTxPktInfo[byPktNum].wFIFOCtl = wFIFOCtl;
    memcpy(pStatistic->abyTxPktInfo[byPktNum].abyDestAddr, pbyDestAddr, U_ETHER_ADDR_LEN);
}




static
VOID
s_vFillTxKey (
    IN  PSDevice   pDevice,
    IN  PBYTE      pbyBuf,
    IN  PBYTE      pbyIVHead,
    IN  PSKeyItem  pTransmitKey,
    IN  PBYTE      pbyHdrBuf,
    IN  WORD       wPayloadLen,
    OUT PBYTE      pMICHDR
    )
{
    PDWORD          pdwIV = (PDWORD) pbyIVHead;
    PDWORD          pdwExtIV = (PDWORD) ((PBYTE)pbyIVHead+4);
    WORD            wValue;
    PS802_11Header  pMACHeader = (PS802_11Header)pbyHdrBuf;
    DWORD           dwRevIVCounter;



    //Fill TXKEY
    if (pTransmitKey == NULL)
        return;

    dwRevIVCounter = cpu_to_le32(pDevice->dwIVCounter);
    *pdwIV = pDevice->dwIVCounter;
    pDevice->byKeyIndex = pTransmitKey->dwKeyIndex & 0xf;

    if (pTransmitKey->byCipherSuite == KEY_CTL_WEP) {
        if (pTransmitKey->uKeyLength == WLAN_WEP232_KEYLEN ){
            memcpy(pDevice->abyPRNG, (PBYTE)&(dwRevIVCounter), 3);
            memcpy(pDevice->abyPRNG+3, pTransmitKey->abyKey, pTransmitKey->uKeyLength);
        } else {
            memcpy(pbyBuf, (PBYTE)&(dwRevIVCounter), 3);
            memcpy(pbyBuf+3, pTransmitKey->abyKey, pTransmitKey->uKeyLength);
            if(pTransmitKey->uKeyLength == WLAN_WEP40_KEYLEN) {
                memcpy(pbyBuf+8, (PBYTE)&(dwRevIVCounter), 3);
                memcpy(pbyBuf+11, pTransmitKey->abyKey, pTransmitKey->uKeyLength);
            }
            memcpy(pDevice->abyPRNG, pbyBuf, 16);
        }
        // Append IV after Mac Header
        *pdwIV &= WEP_IV_MASK;//00000000 11111111 11111111 11111111
        *pdwIV |= (pDevice->byKeyIndex << 30);
        *pdwIV = cpu_to_le32(*pdwIV);
        pDevice->dwIVCounter++;
        if (pDevice->dwIVCounter > WEP_IV_MASK) {
            pDevice->dwIVCounter = 0;
        }
    } else if (pTransmitKey->byCipherSuite == KEY_CTL_TKIP) {
        pTransmitKey->wTSC15_0++;
        if (pTransmitKey->wTSC15_0 == 0) {
            pTransmitKey->dwTSC47_16++;
        }
        TKIPvMixKey(pTransmitKey->abyKey, pDevice->abyCurrentNetAddr,
                    pTransmitKey->wTSC15_0, pTransmitKey->dwTSC47_16, pDevice->abyPRNG);
        memcpy(pbyBuf, pDevice->abyPRNG, 16);
        // Make IV
        memcpy(pdwIV, pDevice->abyPRNG, 3);

        *(pbyIVHead+3) = (BYTE)(((pDevice->byKeyIndex << 6) & 0xc0) | 0x20); // 0x20 is ExtIV
        // Append IV&ExtIV after Mac Header
        *pdwExtIV = cpu_to_le32(pTransmitKey->dwTSC47_16);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"vFillTxKey()---- pdwExtIV: %lx\n", *pdwExtIV);

    } else if (pTransmitKey->byCipherSuite == KEY_CTL_CCMP) {
        pTransmitKey->wTSC15_0++;
        if (pTransmitKey->wTSC15_0 == 0) {
            pTransmitKey->dwTSC47_16++;
        }
        memcpy(pbyBuf, pTransmitKey->abyKey, 16);

        // Make IV
        *pdwIV = 0;
        *(pbyIVHead+3) = (BYTE)(((pDevice->byKeyIndex << 6) & 0xc0) | 0x20); // 0x20 is ExtIV
        *pdwIV |= cpu_to_le16((WORD)(pTransmitKey->wTSC15_0));
        //Append IV&ExtIV after Mac Header
        *pdwExtIV = cpu_to_le32(pTransmitKey->dwTSC47_16);

        //Fill MICHDR0
        *pMICHDR = 0x59;
        *((PBYTE)(pMICHDR+1)) = 0; // TxPriority
        memcpy(pMICHDR+2, &(pMACHeader->abyAddr2[0]), 6);
        *((PBYTE)(pMICHDR+8)) = HIBYTE(HIWORD(pTransmitKey->dwTSC47_16));
        *((PBYTE)(pMICHDR+9)) = LOBYTE(HIWORD(pTransmitKey->dwTSC47_16));
        *((PBYTE)(pMICHDR+10)) = HIBYTE(LOWORD(pTransmitKey->dwTSC47_16));
        *((PBYTE)(pMICHDR+11)) = LOBYTE(LOWORD(pTransmitKey->dwTSC47_16));
        *((PBYTE)(pMICHDR+12)) = HIBYTE(pTransmitKey->wTSC15_0);
        *((PBYTE)(pMICHDR+13)) = LOBYTE(pTransmitKey->wTSC15_0);
        *((PBYTE)(pMICHDR+14)) = HIBYTE(wPayloadLen);
        *((PBYTE)(pMICHDR+15)) = LOBYTE(wPayloadLen);

        //Fill MICHDR1
        *((PBYTE)(pMICHDR+16)) = 0; // HLEN[15:8]
        if (pDevice->bLongHeader) {
            *((PBYTE)(pMICHDR+17)) = 28; // HLEN[7:0]
        } else {
            *((PBYTE)(pMICHDR+17)) = 22; // HLEN[7:0]
        }
        wValue = cpu_to_le16(pMACHeader->wFrameCtl & 0xC78F);
        memcpy(pMICHDR+18, (PBYTE)&wValue, 2); // MSKFRACTL
        memcpy(pMICHDR+20, &(pMACHeader->abyAddr1[0]), 6);
        memcpy(pMICHDR+26, &(pMACHeader->abyAddr2[0]), 6);

        //Fill MICHDR2
        memcpy(pMICHDR+32, &(pMACHeader->abyAddr3[0]), 6);
        wValue = pMACHeader->wSeqCtl;
        wValue &= 0x000F;
        wValue = cpu_to_le16(wValue);
        memcpy(pMICHDR+38, (PBYTE)&wValue, 2); // MSKSEQCTL
        if (pDevice->bLongHeader) {
            memcpy(pMICHDR+40, &(pMACHeader->abyAddr4[0]), 6);
        }
    }
}


static
VOID
s_vSWencryption (
    IN  PSDevice            pDevice,
    IN  PSKeyItem           pTransmitKey,
    IN  PBYTE               pbyPayloadHead,
    IN  WORD                wPayloadSize
    )
{
    UINT   cbICVlen = 4;
    DWORD  dwICV = 0xFFFFFFFFL;
    PDWORD pdwICV;

    if (pTransmitKey == NULL)
        return;

    if (pTransmitKey->byCipherSuite == KEY_CTL_WEP) {
        //=======================================================================
        // Append ICV after payload
        dwICV = CRCdwGetCrc32Ex(pbyPayloadHead, wPayloadSize, dwICV);//ICV(Payload)
        pdwICV = (PDWORD)(pbyPayloadHead + wPayloadSize);
        // finally, we must invert dwCRC to get the correct answer
        *pdwICV = cpu_to_le32(~dwICV);
        // RC4 encryption
        rc4_init(&pDevice->SBox, pDevice->abyPRNG, pTransmitKey->uKeyLength + 3);
        rc4_encrypt(&pDevice->SBox, pbyPayloadHead, pbyPayloadHead, wPayloadSize+cbICVlen);
        //=======================================================================
    } else if (pTransmitKey->byCipherSuite == KEY_CTL_TKIP) {
        //=======================================================================
        //Append ICV after payload
        dwICV = CRCdwGetCrc32Ex(pbyPayloadHead, wPayloadSize, dwICV);//ICV(Payload)
        pdwICV = (PDWORD)(pbyPayloadHead + wPayloadSize);
        // finally, we must invert dwCRC to get the correct answer
        *pdwICV = cpu_to_le32(~dwICV);
        // RC4 encryption
        rc4_init(&pDevice->SBox, pDevice->abyPRNG, TKIP_KEY_LEN);
        rc4_encrypt(&pDevice->SBox, pbyPayloadHead, pbyPayloadHead, wPayloadSize+cbICVlen);
        //=======================================================================
    }
}




/*byPktType : PK_TYPE_11A     0
             PK_TYPE_11B     1
             PK_TYPE_11GB    2
             PK_TYPE_11GA    3
*/
static
UINT
s_uGetTxRsvTime (
    IN PSDevice pDevice,
    IN BYTE     byPktType,
    IN UINT     cbFrameLength,
    IN WORD     wRate,
    IN BOOL     bNeedAck
    )
{
    UINT uDataTime, uAckTime;

    uDataTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, cbFrameLength, wRate);
    if (byPktType == PK_TYPE_11B) {//llb,CCK mode
        uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, (WORD)pDevice->byTopCCKBasicRate);
    } else {//11g 2.4G OFDM mode & 11a 5G OFDM mode
        uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, (WORD)pDevice->byTopOFDMBasicRate);
    }

    if (bNeedAck) {
        return (uDataTime + pDevice->uSIFS + uAckTime);
    }
    else {
        return uDataTime;
    }
}

//byFreqType: 0=>5GHZ 1=>2.4GHZ
static
UINT
s_uGetRTSCTSRsvTime (
    IN PSDevice pDevice,
    IN BYTE byRTSRsvType,
    IN BYTE byPktType,
    IN UINT cbFrameLength,
    IN WORD wCurrentRate
    )
{
    UINT uRrvTime  , uRTSTime, uCTSTime, uAckTime, uDataTime;

    uRrvTime = uRTSTime = uCTSTime = uAckTime = uDataTime = 0;


    uDataTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, cbFrameLength, wCurrentRate);
    if (byRTSRsvType == 0) { //RTSTxRrvTime_bb
        uRTSTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 20, pDevice->byTopCCKBasicRate);
        uCTSTime = uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopCCKBasicRate);
    }
    else if (byRTSRsvType == 1){ //RTSTxRrvTime_ba, only in 2.4GHZ
        uRTSTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 20, pDevice->byTopCCKBasicRate);
        uCTSTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopCCKBasicRate);
        uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
    }
    else if (byRTSRsvType == 2) { //RTSTxRrvTime_aa
        uRTSTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 20, pDevice->byTopOFDMBasicRate);
        uCTSTime = uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
    }
    else if (byRTSRsvType == 3) { //CTSTxRrvTime_ba, only in 2.4GHZ
        uCTSTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopCCKBasicRate);
        uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
        uRrvTime = uCTSTime + uAckTime + uDataTime + 2*pDevice->uSIFS;
        return uRrvTime;
    }

    //RTSRrvTime
    uRrvTime = uRTSTime + uCTSTime + uAckTime + uDataTime + 3*pDevice->uSIFS;
    return uRrvTime;
}

//byFreqType 0: 5GHz, 1:2.4Ghz
static
UINT
s_uGetDataDuration (
    IN PSDevice pDevice,
    IN BYTE     byDurType,
    IN UINT     cbFrameLength,
    IN BYTE     byPktType,
    IN WORD     wRate,
    IN BOOL     bNeedAck,
    IN UINT     uFragIdx,
    IN UINT     cbLastFragmentSize,
    IN UINT     uMACfragNum,
    IN BYTE     byFBOption
    )
{
    BOOL bLastFrag = 0;
    UINT uAckTime =0, uNextPktTime = 0;


    if (uFragIdx == (uMACfragNum-1)) {
        bLastFrag = 1;
    }

    switch (byDurType) {

    case DATADUR_B:    //DATADUR_B
        if (((uMACfragNum == 1)) || (bLastFrag == 1)) {//Non Frag or Last Frag
            if (bNeedAck) {
            	uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopCCKBasicRate);
                return (pDevice->uSIFS + uAckTime);
            } else {
                return 0;
            }
        }
        else {//First Frag or Mid Frag
            if (uFragIdx == (uMACfragNum-2)) {
            	uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbLastFragmentSize, wRate, bNeedAck);
            } else {
                uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wRate, bNeedAck);
            }
            if (bNeedAck) {
            	uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopCCKBasicRate);
                return (pDevice->uSIFS + uAckTime + uNextPktTime);
            } else {
                return (pDevice->uSIFS + uNextPktTime);
            }
        }
        break;


    case DATADUR_A:    //DATADUR_A
        if (((uMACfragNum==1)) || (bLastFrag==1)) {//Non Frag or Last Frag
            if(bNeedAck){
            	uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
                return (pDevice->uSIFS + uAckTime);
            } else {
                return 0;
            }
        }
        else {//First Frag or Mid Frag
            if(uFragIdx == (uMACfragNum-2)){
            	uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbLastFragmentSize, wRate, bNeedAck);
            } else {
                uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wRate, bNeedAck);
            }
            if(bNeedAck){
            	uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
                return (pDevice->uSIFS + uAckTime + uNextPktTime);
            } else {
                return (pDevice->uSIFS + uNextPktTime);
            }
        }
        break;

    case DATADUR_A_F0:    //DATADUR_A_F0
	    if (((uMACfragNum==1)) || (bLastFrag==1)) {//Non Frag or Last Frag
            if(bNeedAck){
            	uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
                return (pDevice->uSIFS + uAckTime);
            } else {
                return 0;
            }
        }
	    else { //First Frag or Mid Frag
	        if (byFBOption == AUTO_FB_0) {
                if (wRate < RATE_18M)
                    wRate = RATE_18M;
                else if (wRate > RATE_54M)
                    wRate = RATE_54M;

	            if(uFragIdx == (uMACfragNum-2)){
            	    uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbLastFragmentSize, wFB_Opt0[FB_RATE0][wRate-RATE_18M], bNeedAck);
                } else {
                    uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt0[FB_RATE0][wRate-RATE_18M], bNeedAck);
                }
	        } else { // (byFBOption == AUTO_FB_1)
                if (wRate < RATE_18M)
                    wRate = RATE_18M;
                else if (wRate > RATE_54M)
                    wRate = RATE_54M;

	            if(uFragIdx == (uMACfragNum-2)){
            	    uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbLastFragmentSize, wFB_Opt1[FB_RATE0][wRate-RATE_18M], bNeedAck);
                } else {
                    uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt1[FB_RATE0][wRate-RATE_18M], bNeedAck);
                }
	        }

	        if(bNeedAck){
            	uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
                return (pDevice->uSIFS + uAckTime + uNextPktTime);
            } else {
                return (pDevice->uSIFS + uNextPktTime);
            }
	    }
        break;

    case DATADUR_A_F1:    //DATADUR_A_F1
        if (((uMACfragNum==1)) || (bLastFrag==1)) {//Non Frag or Last Frag
            if(bNeedAck){
            	uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
                return (pDevice->uSIFS + uAckTime);
            } else {
                return 0;
            }
        }
	    else { //First Frag or Mid Frag
	        if (byFBOption == AUTO_FB_0) {
                if (wRate < RATE_18M)
                    wRate = RATE_18M;
                else if (wRate > RATE_54M)
                    wRate = RATE_54M;

	            if(uFragIdx == (uMACfragNum-2)){
            	    uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbLastFragmentSize, wFB_Opt0[FB_RATE1][wRate-RATE_18M], bNeedAck);
                } else {
                    uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt0[FB_RATE1][wRate-RATE_18M], bNeedAck);
                }

	        } else { // (byFBOption == AUTO_FB_1)
                if (wRate < RATE_18M)
                    wRate = RATE_18M;
                else if (wRate > RATE_54M)
                    wRate = RATE_54M;

	            if(uFragIdx == (uMACfragNum-2)){
            	    uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbLastFragmentSize, wFB_Opt1[FB_RATE1][wRate-RATE_18M], bNeedAck);
                } else {
                    uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt1[FB_RATE1][wRate-RATE_18M], bNeedAck);
                }
	        }
	        if(bNeedAck){
            	uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
                return (pDevice->uSIFS + uAckTime + uNextPktTime);
            } else {
                return (pDevice->uSIFS + uNextPktTime);
            }
	    }
        break;

    default:
        break;
    }

	ASSERT(FALSE);
	return 0;
}


//byFreqType: 0=>5GHZ 1=>2.4GHZ
static
UINT
s_uGetRTSCTSDuration (
    IN PSDevice pDevice,
    IN BYTE byDurType,
    IN UINT cbFrameLength,
    IN BYTE byPktType,
    IN WORD wRate,
    IN BOOL bNeedAck,
    IN BYTE byFBOption
    )
{
    UINT uCTSTime = 0, uDurTime = 0;


    switch (byDurType) {

    case RTSDUR_BB:    //RTSDuration_bb
        uCTSTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopCCKBasicRate);
        uDurTime = uCTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wRate, bNeedAck);
        break;

    case RTSDUR_BA:    //RTSDuration_ba
        uCTSTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopCCKBasicRate);
        uDurTime = uCTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wRate, bNeedAck);
        break;

    case RTSDUR_AA:    //RTSDuration_aa
        uCTSTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
        uDurTime = uCTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wRate, bNeedAck);
        break;

    case CTSDUR_BA:    //CTSDuration_ba
        uDurTime = pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wRate, bNeedAck);
        break;

    case RTSDUR_BA_F0: //RTSDuration_ba_f0
        uCTSTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopCCKBasicRate);
        if ((byFBOption == AUTO_FB_0) && (wRate >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = uCTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt0[FB_RATE0][wRate-RATE_18M], bNeedAck);
        } else if ((byFBOption == AUTO_FB_1) && (wRate >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = uCTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt1[FB_RATE0][wRate-RATE_18M], bNeedAck);
        }
        break;

    case RTSDUR_AA_F0: //RTSDuration_aa_f0
        uCTSTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
        if ((byFBOption == AUTO_FB_0) && (wRate >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = uCTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt0[FB_RATE0][wRate-RATE_18M], bNeedAck);
        } else if ((byFBOption == AUTO_FB_1) && (wRate >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = uCTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt1[FB_RATE0][wRate-RATE_18M], bNeedAck);
        }
        break;

    case RTSDUR_BA_F1: //RTSDuration_ba_f1
        uCTSTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopCCKBasicRate);
        if ((byFBOption == AUTO_FB_0) && (wRate >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = uCTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt0[FB_RATE1][wRate-RATE_18M], bNeedAck);
        } else if ((byFBOption == AUTO_FB_1) && (wRate >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = uCTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt1[FB_RATE1][wRate-RATE_18M], bNeedAck);
        }
        break;

    case RTSDUR_AA_F1: //RTSDuration_aa_f1
        uCTSTime = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
        if ((byFBOption == AUTO_FB_0) && (wRate >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = uCTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt0[FB_RATE1][wRate-RATE_18M], bNeedAck);
        } else if ((byFBOption == AUTO_FB_1) && (wRate >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = uCTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt1[FB_RATE1][wRate-RATE_18M], bNeedAck);
        }
        break;

    case CTSDUR_BA_F0: //CTSDuration_ba_f0
        if ((byFBOption == AUTO_FB_0) && (wRate >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt0[FB_RATE0][wRate-RATE_18M], bNeedAck);
        } else if ((byFBOption == AUTO_FB_1) && (wRate >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt1[FB_RATE0][wRate-RATE_18M], bNeedAck);
        }
        break;

    case CTSDUR_BA_F1: //CTSDuration_ba_f1
        if ((byFBOption == AUTO_FB_0) && (wRate >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt0[FB_RATE1][wRate-RATE_18M], bNeedAck);
        } else if ((byFBOption == AUTO_FB_1) && (wRate >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt1[FB_RATE1][wRate-RATE_18M], bNeedAck);
        }
        break;

    default:
        break;
    }

    return uDurTime;

}




static
UINT
s_uFillDataHead (
    IN PSDevice pDevice,
    IN BYTE     byPktType,
    IN WORD     wCurrentRate,
    IN PVOID    pTxDataHead,
    IN UINT     cbFrameLength,
    IN UINT     uDMAIdx,
    IN BOOL     bNeedAck,
    IN UINT     uFragIdx,
    IN UINT     cbLastFragmentSize,
    IN UINT     uMACfragNum,
    IN BYTE     byFBOption
    )
{

    if (pTxDataHead == NULL) {
        return 0;
    }

    if (byPktType == PK_TYPE_11GB || byPktType == PK_TYPE_11GA) {
        if((uDMAIdx==TYPE_ATIMDMA)||(uDMAIdx==TYPE_BEACONDMA)) {
            PSTxDataHead_ab pBuf = (PSTxDataHead_ab)pTxDataHead;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, cbFrameLength, wCurrentRate, byPktType,
                (PWORD)&(pBuf->wTransmitLength), (PBYTE)&(pBuf->byServiceField), (PBYTE)&(pBuf->bySignalField)
            );
            //Get Duration and TimeStampOff
            pBuf->wDuration = (WORD)s_uGetDataDuration(pDevice, DATADUR_A, cbFrameLength, byPktType,
                                                       wCurrentRate, bNeedAck, uFragIdx,
                                                       cbLastFragmentSize, uMACfragNum,
                                                       byFBOption); //1: 2.4GHz
            if(uDMAIdx!=TYPE_ATIMDMA) {
                pBuf->wTimeStampOff = wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE];
            }
            return (pBuf->wDuration);
        }
        else { // DATA & MANAGE Frame
            if (byFBOption == AUTO_FB_NONE) {
                PSTxDataHead_g pBuf = (PSTxDataHead_g)pTxDataHead;
                //Get SignalField,ServiceField,Length
                BBvCaculateParameter(pDevice, cbFrameLength, wCurrentRate, byPktType,
                    (PWORD)&(pBuf->wTransmitLength_a), (PBYTE)&(pBuf->byServiceField_a), (PBYTE)&(pBuf->bySignalField_a)
                );
                BBvCaculateParameter(pDevice, cbFrameLength, pDevice->byTopCCKBasicRate, PK_TYPE_11B,
                    (PWORD)&(pBuf->wTransmitLength_b), (PBYTE)&(pBuf->byServiceField_b), (PBYTE)&(pBuf->bySignalField_b)
                );
                //Get Duration and TimeStamp
                pBuf->wDuration_a = (WORD)s_uGetDataDuration(pDevice, DATADUR_A, cbFrameLength,
                                                             byPktType, wCurrentRate, bNeedAck, uFragIdx,
                                                             cbLastFragmentSize, uMACfragNum,
                                                             byFBOption); //1: 2.4GHz
                pBuf->wDuration_b = (WORD)s_uGetDataDuration(pDevice, DATADUR_B, cbFrameLength,
                                                             PK_TYPE_11B, pDevice->byTopCCKBasicRate,
                                                             bNeedAck, uFragIdx, cbLastFragmentSize,
                                                             uMACfragNum, byFBOption); //1: 2.4GHz

                pBuf->wTimeStampOff_a = wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE];
                pBuf->wTimeStampOff_b = wTimeStampOff[pDevice->byPreambleType%2][pDevice->byTopCCKBasicRate%MAX_RATE];
                return (pBuf->wDuration_a);
             } else {
                // Auto Fallback
                PSTxDataHead_g_FB pBuf = (PSTxDataHead_g_FB)pTxDataHead;
                //Get SignalField,ServiceField,Length
                BBvCaculateParameter(pDevice, cbFrameLength, wCurrentRate, byPktType,
                    (PWORD)&(pBuf->wTransmitLength_a), (PBYTE)&(pBuf->byServiceField_a), (PBYTE)&(pBuf->bySignalField_a)
                );
                BBvCaculateParameter(pDevice, cbFrameLength, pDevice->byTopCCKBasicRate, PK_TYPE_11B,
                    (PWORD)&(pBuf->wTransmitLength_b), (PBYTE)&(pBuf->byServiceField_b), (PBYTE)&(pBuf->bySignalField_b)
                );
                //Get Duration and TimeStamp
                pBuf->wDuration_a = (WORD)s_uGetDataDuration(pDevice, DATADUR_A, cbFrameLength, byPktType,
                                             wCurrentRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption); //1: 2.4GHz
                pBuf->wDuration_b = (WORD)s_uGetDataDuration(pDevice, DATADUR_B, cbFrameLength, PK_TYPE_11B,
                                             pDevice->byTopCCKBasicRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption); //1: 2.4GHz
                pBuf->wDuration_a_f0 = (WORD)s_uGetDataDuration(pDevice, DATADUR_A_F0, cbFrameLength, byPktType,
                                             wCurrentRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption); //1: 2.4GHz
                pBuf->wDuration_a_f1 = (WORD)s_uGetDataDuration(pDevice, DATADUR_A_F1, cbFrameLength, byPktType,
                                             wCurrentRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption); //1: 2.4GHz
                pBuf->wTimeStampOff_a = wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE];
                pBuf->wTimeStampOff_b = wTimeStampOff[pDevice->byPreambleType%2][pDevice->byTopCCKBasicRate%MAX_RATE];
                return (pBuf->wDuration_a);
            } //if (byFBOption == AUTO_FB_NONE)
        }
    }
    else if (byPktType == PK_TYPE_11A) {
        if ((byFBOption != AUTO_FB_NONE) && (uDMAIdx != TYPE_ATIMDMA) && (uDMAIdx != TYPE_BEACONDMA)) {
            // Auto Fallback
            PSTxDataHead_a_FB pBuf = (PSTxDataHead_a_FB)pTxDataHead;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, cbFrameLength, wCurrentRate, byPktType,
                (PWORD)&(pBuf->wTransmitLength), (PBYTE)&(pBuf->byServiceField), (PBYTE)&(pBuf->bySignalField)
            );
            //Get Duration and TimeStampOff
            pBuf->wDuration = (WORD)s_uGetDataDuration(pDevice, DATADUR_A, cbFrameLength, byPktType,
                                        wCurrentRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption); //0: 5GHz
            pBuf->wDuration_f0 = (WORD)s_uGetDataDuration(pDevice, DATADUR_A_F0, cbFrameLength, byPktType,
                                        wCurrentRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption); //0: 5GHz
            pBuf->wDuration_f1 = (WORD)s_uGetDataDuration(pDevice, DATADUR_A_F1, cbFrameLength, byPktType,
                                        wCurrentRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption); //0: 5GHz
            if(uDMAIdx!=TYPE_ATIMDMA) {
                pBuf->wTimeStampOff = wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE];
            }
            return (pBuf->wDuration);
        } else {
            PSTxDataHead_ab pBuf = (PSTxDataHead_ab)pTxDataHead;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, cbFrameLength, wCurrentRate, byPktType,
                (PWORD)&(pBuf->wTransmitLength), (PBYTE)&(pBuf->byServiceField), (PBYTE)&(pBuf->bySignalField)
            );
            //Get Duration and TimeStampOff
            pBuf->wDuration = (WORD)s_uGetDataDuration(pDevice, DATADUR_A, cbFrameLength, byPktType,
                                                       wCurrentRate, bNeedAck, uFragIdx,
                                                       cbLastFragmentSize, uMACfragNum,
                                                       byFBOption);

            if(uDMAIdx!=TYPE_ATIMDMA) {
                pBuf->wTimeStampOff = wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE];
            }
            return (pBuf->wDuration);
        }
    }
    else if (byPktType == PK_TYPE_11B) {
            PSTxDataHead_ab pBuf = (PSTxDataHead_ab)pTxDataHead;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, cbFrameLength, wCurrentRate, byPktType,
                (PWORD)&(pBuf->wTransmitLength), (PBYTE)&(pBuf->byServiceField), (PBYTE)&(pBuf->bySignalField)
            );
            //Get Duration and TimeStampOff
            pBuf->wDuration = (WORD)s_uGetDataDuration(pDevice, DATADUR_B, cbFrameLength, byPktType,
                                                       wCurrentRate, bNeedAck, uFragIdx,
                                                       cbLastFragmentSize, uMACfragNum,
                                                       byFBOption);
            if (uDMAIdx != TYPE_ATIMDMA) {
                pBuf->wTimeStampOff = wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE];
            }
            return (pBuf->wDuration);
    }
    return 0;
}




static
VOID
s_vFillRTSHead (
    IN PSDevice         pDevice,
    IN BYTE             byPktType,
    IN PVOID            pvRTS,
    IN UINT             cbFrameLength,
    IN BOOL             bNeedAck,
    IN BOOL             bDisCRC,
    IN PSEthernetHeader psEthHeader,
    IN WORD             wCurrentRate,
    IN BYTE             byFBOption
    )
{
    UINT uRTSFrameLen = 20;
    WORD  wLen = 0x0000;

    if (pvRTS == NULL)
    	return;

    if (bDisCRC) {
        // When CRCDIS bit is on, H/W forgot to generate FCS for RTS frame,
        // in this case we need to decrease its length by 4.
        uRTSFrameLen -= 4;
    }

    // Note: So far RTSHead dosen't appear in ATIM & Beacom DMA, so we don't need to take them into account.
    //       Otherwise, we need to modified codes for them.
    if (byPktType == PK_TYPE_11GB || byPktType == PK_TYPE_11GA) {
        if (byFBOption == AUTO_FB_NONE) {
            PSRTS_g pBuf = (PSRTS_g)pvRTS;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, uRTSFrameLen, pDevice->byTopCCKBasicRate, PK_TYPE_11B,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField_b), (PBYTE)&(pBuf->bySignalField_b)
            );
            pBuf->wTransmitLength_b = cpu_to_le16(wLen);
            BBvCaculateParameter(pDevice, uRTSFrameLen, pDevice->byTopOFDMBasicRate, byPktType,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField_a), (PBYTE)&(pBuf->bySignalField_a)
            );
            pBuf->wTransmitLength_a = cpu_to_le16(wLen);
            //Get Duration
            pBuf->wDuration_bb = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_BB, cbFrameLength, PK_TYPE_11B, pDevice->byTopCCKBasicRate, bNeedAck, byFBOption));    //0:RTSDuration_bb, 1:2.4G, 1:CCKData
            pBuf->wDuration_aa = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_AA, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption)); //2:RTSDuration_aa, 1:2.4G, 2,3: 2.4G OFDMData
            pBuf->wDuration_ba = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_BA, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption)); //1:RTSDuration_ba, 1:2.4G, 2,3:2.4G OFDM Data

            pBuf->Data.wDurationID = pBuf->wDuration_aa;
            //Get RTS Frame body
            pBuf->Data.wFrameControl = TYPE_CTL_RTS;//0x00B4
            if ((pDevice->eOPMode == OP_MODE_ADHOC) ||
                (pDevice->eOPMode == OP_MODE_AP)) {
                memcpy(&(pBuf->Data.abyRA[0]), &(psEthHeader->abyDstAddr[0]), U_ETHER_ADDR_LEN);
            }
            else {
                memcpy(&(pBuf->Data.abyRA[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
            }
            if (pDevice->eOPMode == OP_MODE_AP) {
                memcpy(&(pBuf->Data.abyTA[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
            }
            else {
                memcpy(&(pBuf->Data.abyTA[0]), &(psEthHeader->abySrcAddr[0]), U_ETHER_ADDR_LEN);
            }
        }
        else {
           PSRTS_g_FB pBuf = (PSRTS_g_FB)pvRTS;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, uRTSFrameLen, pDevice->byTopCCKBasicRate, PK_TYPE_11B,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField_b), (PBYTE)&(pBuf->bySignalField_b)
            );
            pBuf->wTransmitLength_b = cpu_to_le16(wLen);
            BBvCaculateParameter(pDevice, uRTSFrameLen, pDevice->byTopOFDMBasicRate, byPktType,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField_a), (PBYTE)&(pBuf->bySignalField_a)
            );
            pBuf->wTransmitLength_a = cpu_to_le16(wLen);
            //Get Duration
            pBuf->wDuration_bb = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_BB, cbFrameLength, PK_TYPE_11B, pDevice->byTopCCKBasicRate, bNeedAck, byFBOption));    //0:RTSDuration_bb, 1:2.4G, 1:CCKData
            pBuf->wDuration_aa = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_AA, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption)); //2:RTSDuration_aa, 1:2.4G, 2,3:2.4G OFDMData
            pBuf->wDuration_ba = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_BA, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption)); //1:RTSDuration_ba, 1:2.4G, 2,3:2.4G OFDMData
            pBuf->wRTSDuration_ba_f0 = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_BA_F0, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption));    //4:wRTSDuration_ba_f0, 1:2.4G, 1:CCKData
            pBuf->wRTSDuration_aa_f0 = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_AA_F0, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption));    //5:wRTSDuration_aa_f0, 1:2.4G, 1:CCKData
            pBuf->wRTSDuration_ba_f1 = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_BA_F1, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption));    //6:wRTSDuration_ba_f1, 1:2.4G, 1:CCKData
            pBuf->wRTSDuration_aa_f1 = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_AA_F1, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption));    //7:wRTSDuration_aa_f1, 1:2.4G, 1:CCKData
            pBuf->Data.wDurationID = pBuf->wDuration_aa;
            //Get RTS Frame body
            pBuf->Data.wFrameControl = TYPE_CTL_RTS;//0x00B4

            if ((pDevice->eOPMode == OP_MODE_ADHOC) ||
                (pDevice->eOPMode == OP_MODE_AP)) {
                memcpy(&(pBuf->Data.abyRA[0]), &(psEthHeader->abyDstAddr[0]), U_ETHER_ADDR_LEN);
            }
            else {
                memcpy(&(pBuf->Data.abyRA[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
            }

            if (pDevice->eOPMode == OP_MODE_AP) {
                memcpy(&(pBuf->Data.abyTA[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
            }
            else {
                memcpy(&(pBuf->Data.abyTA[0]), &(psEthHeader->abySrcAddr[0]), U_ETHER_ADDR_LEN);
            }

        } // if (byFBOption == AUTO_FB_NONE)
    }
    else if (byPktType == PK_TYPE_11A) {
        if (byFBOption == AUTO_FB_NONE) {
            PSRTS_ab pBuf = (PSRTS_ab)pvRTS;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, uRTSFrameLen, pDevice->byTopOFDMBasicRate, byPktType,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField), (PBYTE)&(pBuf->bySignalField)
            );
            pBuf->wTransmitLength = cpu_to_le16(wLen);
            //Get Duration
            pBuf->wDuration = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_AA, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption)); //0:RTSDuration_aa, 0:5G, 0: 5G OFDMData
    	    pBuf->Data.wDurationID = pBuf->wDuration;
            //Get RTS Frame body
            pBuf->Data.wFrameControl = TYPE_CTL_RTS;//0x00B4

            if ((pDevice->eOPMode == OP_MODE_ADHOC) ||
                (pDevice->eOPMode == OP_MODE_AP)) {
                memcpy(&(pBuf->Data.abyRA[0]), &(psEthHeader->abyDstAddr[0]), U_ETHER_ADDR_LEN);
            }
            else {
                memcpy(&(pBuf->Data.abyRA[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
            }

            if (pDevice->eOPMode == OP_MODE_AP) {
                memcpy(&(pBuf->Data.abyTA[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
            }
            else {
                memcpy(&(pBuf->Data.abyTA[0]), &(psEthHeader->abySrcAddr[0]), U_ETHER_ADDR_LEN);
            }

        }
        else {
            PSRTS_a_FB pBuf = (PSRTS_a_FB)pvRTS;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, uRTSFrameLen, pDevice->byTopOFDMBasicRate, byPktType,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField), (PBYTE)&(pBuf->bySignalField)
            );
            pBuf->wTransmitLength = cpu_to_le16(wLen);
            //Get Duration
            pBuf->wDuration = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_AA, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption)); //0:RTSDuration_aa, 0:5G, 0: 5G OFDMData
    	    pBuf->wRTSDuration_f0 = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_AA_F0, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption)); //5:RTSDuration_aa_f0, 0:5G, 0: 5G OFDMData
    	    pBuf->wRTSDuration_f1 = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_AA_F1, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption)); //7:RTSDuration_aa_f1, 0:5G, 0:
    	    pBuf->Data.wDurationID = pBuf->wDuration;
    	    //Get RTS Frame body
            pBuf->Data.wFrameControl = TYPE_CTL_RTS;//0x00B4

            if ((pDevice->eOPMode == OP_MODE_ADHOC) ||
                (pDevice->eOPMode == OP_MODE_AP)) {
                memcpy(&(pBuf->Data.abyRA[0]), &(psEthHeader->abyDstAddr[0]), U_ETHER_ADDR_LEN);
            }
            else {
                memcpy(&(pBuf->Data.abyRA[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
            }
            if (pDevice->eOPMode == OP_MODE_AP) {
                memcpy(&(pBuf->Data.abyTA[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
            }
            else {
                memcpy(&(pBuf->Data.abyTA[0]), &(psEthHeader->abySrcAddr[0]), U_ETHER_ADDR_LEN);
            }
        }
    }
    else if (byPktType == PK_TYPE_11B) {
        PSRTS_ab pBuf = (PSRTS_ab)pvRTS;
        //Get SignalField,ServiceField,Length
        BBvCaculateParameter(pDevice, uRTSFrameLen, pDevice->byTopCCKBasicRate, PK_TYPE_11B,
            (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField), (PBYTE)&(pBuf->bySignalField)
        );
        pBuf->wTransmitLength = cpu_to_le16(wLen);
        //Get Duration
        pBuf->wDuration = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, RTSDUR_BB, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption)); //0:RTSDuration_bb, 1:2.4G, 1:CCKData
        pBuf->Data.wDurationID = pBuf->wDuration;
        //Get RTS Frame body
        pBuf->Data.wFrameControl = TYPE_CTL_RTS;//0x00B4


        if ((pDevice->eOPMode == OP_MODE_ADHOC) ||
            (pDevice->eOPMode == OP_MODE_AP)) {
            memcpy(&(pBuf->Data.abyRA[0]), &(psEthHeader->abyDstAddr[0]), U_ETHER_ADDR_LEN);
        }
        else {
            memcpy(&(pBuf->Data.abyRA[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
        }

        if (pDevice->eOPMode == OP_MODE_AP) {
            memcpy(&(pBuf->Data.abyTA[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
        }
        else {
            memcpy(&(pBuf->Data.abyTA[0]), &(psEthHeader->abySrcAddr[0]), U_ETHER_ADDR_LEN);
        }
    }
}

static
VOID
s_vFillCTSHead (
    IN PSDevice pDevice,
    IN UINT     uDMAIdx,
    IN BYTE     byPktType,
    IN PVOID    pvCTS,
    IN UINT     cbFrameLength,
    IN BOOL     bNeedAck,
    IN BOOL     bDisCRC,
    IN WORD     wCurrentRate,
    IN BYTE     byFBOption
    )
{
    UINT uCTSFrameLen = 14;
    WORD  wLen = 0x0000;

    if (pvCTS == NULL) {
        return;
    }

    if (bDisCRC) {
        // When CRCDIS bit is on, H/W forgot to generate FCS for CTS frame,
        // in this case we need to decrease its length by 4.
        uCTSFrameLen -= 4;
    }

    if (byPktType == PK_TYPE_11GB || byPktType == PK_TYPE_11GA) {
        if (byFBOption != AUTO_FB_NONE && uDMAIdx != TYPE_ATIMDMA && uDMAIdx != TYPE_BEACONDMA) {
            // Auto Fall back
            PSCTS_FB pBuf = (PSCTS_FB)pvCTS;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, uCTSFrameLen, pDevice->byTopCCKBasicRate, PK_TYPE_11B,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField_b), (PBYTE)&(pBuf->bySignalField_b)
            );
            pBuf->wTransmitLength_b = cpu_to_le16(wLen);
            pBuf->wDuration_ba = (WORD)s_uGetRTSCTSDuration(pDevice, CTSDUR_BA, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption); //3:CTSDuration_ba, 1:2.4G, 2,3:2.4G OFDM Data
            pBuf->wDuration_ba += pDevice->wCTSDuration;
            pBuf->wDuration_ba = cpu_to_le16(pBuf->wDuration_ba);
            //Get CTSDuration_ba_f0
            pBuf->wCTSDuration_ba_f0 = (WORD)s_uGetRTSCTSDuration(pDevice, CTSDUR_BA_F0, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption); //8:CTSDuration_ba_f0, 1:2.4G, 2,3:2.4G OFDM Data
            pBuf->wCTSDuration_ba_f0 += pDevice->wCTSDuration;
            pBuf->wCTSDuration_ba_f0 = cpu_to_le16(pBuf->wCTSDuration_ba_f0);
            //Get CTSDuration_ba_f1
            pBuf->wCTSDuration_ba_f1 = (WORD)s_uGetRTSCTSDuration(pDevice, CTSDUR_BA_F1, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption); //9:CTSDuration_ba_f1, 1:2.4G, 2,3:2.4G OFDM Data
            pBuf->wCTSDuration_ba_f1 += pDevice->wCTSDuration;
            pBuf->wCTSDuration_ba_f1 = cpu_to_le16(pBuf->wCTSDuration_ba_f1);
            //Get CTS Frame body
            pBuf->Data.wDurationID = pBuf->wDuration_ba;
            pBuf->Data.wFrameControl = TYPE_CTL_CTS;//0x00C4
            pBuf->Data.wReserved = 0x0000;
            memcpy(&(pBuf->Data.abyRA[0]), &(pDevice->abyCurrentNetAddr[0]), U_ETHER_ADDR_LEN);
        } else { //if (byFBOption != AUTO_FB_NONE && uDMAIdx != TYPE_ATIMDMA && uDMAIdx != TYPE_BEACONDMA)
            PSCTS pBuf = (PSCTS)pvCTS;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, uCTSFrameLen, pDevice->byTopCCKBasicRate, PK_TYPE_11B,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField_b), (PBYTE)&(pBuf->bySignalField_b)
            );
            pBuf->wTransmitLength_b = cpu_to_le16(wLen);
            //Get CTSDuration_ba
            pBuf->wDuration_ba = cpu_to_le16((WORD)s_uGetRTSCTSDuration(pDevice, CTSDUR_BA, cbFrameLength, byPktType, wCurrentRate, bNeedAck, byFBOption)); //3:CTSDuration_ba, 1:2.4G, 2,3:2.4G OFDM Data
            pBuf->wDuration_ba += pDevice->wCTSDuration;
            pBuf->wDuration_ba = cpu_to_le16(pBuf->wDuration_ba);

            //Get CTS Frame body
            pBuf->Data.wDurationID = pBuf->wDuration_ba;
            pBuf->Data.wFrameControl = TYPE_CTL_CTS;//0x00C4
            pBuf->Data.wReserved = 0x0000;
            memcpy(&(pBuf->Data.abyRA[0]), &(pDevice->abyCurrentNetAddr[0]), U_ETHER_ADDR_LEN);
        }
    }
}






/*+
 *
 * Description:
 *      Generate FIFO control for MAC & Baseband controller
 *
 * Parameters:
 *  In:
 *      pDevice         - Pointer to adpater
 *      pTxDataHead     - Transmit Data Buffer
 *      pTxBufHead      - pTxBufHead
 *      pvRrvTime        - pvRrvTime
 *      pvRTS            - RTS Buffer
 *      pCTS            - CTS Buffer
 *      cbFrameSize     - Transmit Data Length (Hdr+Payload+FCS)
 *      bNeedACK        - If need ACK
 *      uDMAIdx         - DMA Index
 *  Out:
 *      none
 *
 * Return Value: none
 *
-*/
// UINT            cbFrameSize,//Hdr+Payload+FCS
static
VOID
s_vGenerateTxParameter (
    IN PSDevice         pDevice,
    IN BYTE             byPktType,
    IN WORD             wCurrentRate,
    IN PVOID            pTxBufHead,
    IN PVOID            pvRrvTime,
    IN PVOID            pvRTS,
    IN PVOID            pvCTS,
    IN UINT             cbFrameSize,
    IN BOOL             bNeedACK,
    IN UINT             uDMAIdx,
    IN PSEthernetHeader psEthHeader
    )
{
    UINT cbMACHdLen = WLAN_HDR_ADDR3_LEN; //24
    WORD wFifoCtl;
    BOOL bDisCRC = FALSE;
    BYTE byFBOption = AUTO_FB_NONE;
//    WORD wCurrentRate = pDevice->wCurrentRate;

    //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"s_vGenerateTxParameter...\n");
    PSTxBufHead pFifoHead = (PSTxBufHead)pTxBufHead;
    pFifoHead->wReserved = wCurrentRate;
    wFifoCtl = pFifoHead->wFIFOCtl;

    if (wFifoCtl & FIFOCTL_CRCDIS) {
        bDisCRC = TRUE;
    }

    if (wFifoCtl & FIFOCTL_AUTO_FB_0) {
        byFBOption = AUTO_FB_0;
    }
    else if (wFifoCtl & FIFOCTL_AUTO_FB_1) {
        byFBOption = AUTO_FB_1;
    }

    if (pDevice->bLongHeader)
        cbMACHdLen = WLAN_HDR_ADDR3_LEN + 6;

    if (byPktType == PK_TYPE_11GB || byPktType == PK_TYPE_11GA) {

        if (pvRTS != NULL) { //RTS_need
            //Fill RsvTime
            if (pvRrvTime) {
                PSRrvTime_gRTS pBuf = (PSRrvTime_gRTS)pvRrvTime;
                pBuf->wRTSTxRrvTime_aa = cpu_to_le16((WORD)s_uGetRTSCTSRsvTime(pDevice, 2, byPktType, cbFrameSize, wCurrentRate));//2:RTSTxRrvTime_aa, 1:2.4GHz
                pBuf->wRTSTxRrvTime_ba = cpu_to_le16((WORD)s_uGetRTSCTSRsvTime(pDevice, 1, byPktType, cbFrameSize, wCurrentRate));//1:RTSTxRrvTime_ba, 1:2.4GHz
                pBuf->wRTSTxRrvTime_bb = cpu_to_le16((WORD)s_uGetRTSCTSRsvTime(pDevice, 0, byPktType, cbFrameSize, wCurrentRate));//0:RTSTxRrvTime_bb, 1:2.4GHz
                pBuf->wTxRrvTime_a = cpu_to_le16((WORD) s_uGetTxRsvTime(pDevice, byPktType, cbFrameSize, wCurrentRate, bNeedACK));//2.4G OFDM
                pBuf->wTxRrvTime_b = cpu_to_le16((WORD) s_uGetTxRsvTime(pDevice, PK_TYPE_11B, cbFrameSize, pDevice->byTopCCKBasicRate, bNeedACK));//1:CCK
            }
            //Fill RTS
            s_vFillRTSHead(pDevice, byPktType, pvRTS, cbFrameSize, bNeedACK, bDisCRC, psEthHeader, wCurrentRate, byFBOption);
        }
        else {//RTS_needless, PCF mode

            //Fill RsvTime
            if (pvRrvTime) {
                PSRrvTime_gCTS pBuf = (PSRrvTime_gCTS)pvRrvTime;
                pBuf->wTxRrvTime_a = cpu_to_le16((WORD)s_uGetTxRsvTime(pDevice, byPktType, cbFrameSize, wCurrentRate, bNeedACK));//2.4G OFDM
                pBuf->wTxRrvTime_b = cpu_to_le16((WORD)s_uGetTxRsvTime(pDevice, PK_TYPE_11B, cbFrameSize, pDevice->byTopCCKBasicRate, bNeedACK));//1:CCK
                pBuf->wCTSTxRrvTime_ba = cpu_to_le16((WORD)s_uGetRTSCTSRsvTime(pDevice, 3, byPktType, cbFrameSize, wCurrentRate));//3:CTSTxRrvTime_Ba, 1:2.4GHz
            }
            //Fill CTS
            s_vFillCTSHead(pDevice, uDMAIdx, byPktType, pvCTS, cbFrameSize, bNeedACK, bDisCRC, wCurrentRate, byFBOption);
        }
    }
    else if (byPktType == PK_TYPE_11A) {

        if (pvRTS != NULL) {//RTS_need, non PCF mode
            //Fill RsvTime
            if (pvRrvTime) {
                PSRrvTime_ab pBuf = (PSRrvTime_ab)pvRrvTime;
                pBuf->wRTSTxRrvTime = cpu_to_le16((WORD)s_uGetRTSCTSRsvTime(pDevice, 2, byPktType, cbFrameSize, wCurrentRate));//2:RTSTxRrvTime_aa, 0:5GHz
                pBuf->wTxRrvTime = cpu_to_le16((WORD)s_uGetTxRsvTime(pDevice, byPktType, cbFrameSize, wCurrentRate, bNeedACK));//0:OFDM
            }
            //Fill RTS
            s_vFillRTSHead(pDevice, byPktType, pvRTS, cbFrameSize, bNeedACK, bDisCRC, psEthHeader, wCurrentRate, byFBOption);
        }
        else if (pvRTS == NULL) {//RTS_needless, non PCF mode
            //Fill RsvTime
            if (pvRrvTime) {
                PSRrvTime_ab pBuf = (PSRrvTime_ab)pvRrvTime;
                pBuf->wTxRrvTime = cpu_to_le16((WORD)s_uGetTxRsvTime(pDevice, PK_TYPE_11A, cbFrameSize, wCurrentRate, bNeedACK)); //0:OFDM
            }
        }
    }
    else if (byPktType == PK_TYPE_11B) {

        if ((pvRTS != NULL)) {//RTS_need, non PCF mode
            //Fill RsvTime
            if (pvRrvTime) {
                PSRrvTime_ab pBuf = (PSRrvTime_ab)pvRrvTime;
                pBuf->wRTSTxRrvTime = cpu_to_le16((WORD)s_uGetRTSCTSRsvTime(pDevice, 0, byPktType, cbFrameSize, wCurrentRate));//0:RTSTxRrvTime_bb, 1:2.4GHz
                pBuf->wTxRrvTime = cpu_to_le16((WORD)s_uGetTxRsvTime(pDevice, PK_TYPE_11B, cbFrameSize, wCurrentRate, bNeedACK));//1:CCK
            }
            //Fill RTS
            s_vFillRTSHead(pDevice, byPktType, pvRTS, cbFrameSize, bNeedACK, bDisCRC, psEthHeader, wCurrentRate, byFBOption);
        }
        else { //RTS_needless, non PCF mode
            //Fill RsvTime
            if (pvRrvTime) {
                PSRrvTime_ab pBuf = (PSRrvTime_ab)pvRrvTime;
                pBuf->wTxRrvTime = cpu_to_le16((WORD)s_uGetTxRsvTime(pDevice, PK_TYPE_11B, cbFrameSize, wCurrentRate, bNeedACK)); //1:CCK
            }
        }
    }
    //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"s_vGenerateTxParameter END.\n");
}
/*
    PBYTE pbyBuffer,//point to pTxBufHead
    WORD  wFragType,//00:Non-Frag, 01:Start, 02:Mid, 03:Last
    UINT  cbFragmentSize,//Hdr+payoad+FCS
*/


BOOL
s_bPacketToWirelessUsb(
    IN  PSDevice         pDevice,
    IN  BYTE             byPktType,
    IN  PBYTE            usbPacketBuf,
    IN  BOOL             bNeedEncryption,
    IN  UINT             uSkbPacketLen,
    IN  UINT             uDMAIdx,
    IN  PSEthernetHeader psEthHeader,
    IN  PBYTE            pPacket,
    IN  PSKeyItem        pTransmitKey,
    IN  UINT             uNodeIndex,
    IN  WORD             wCurrentRate,
    OUT UINT             *pcbHeaderLen,
    OUT UINT             *pcbTotalLen
    )
{
    PSMgmtObject        pMgmt = &(pDevice->sMgmtObj);
    UINT                cbFrameSize,cbFrameBodySize;
    PTX_BUFFER          pTxBufHead;
    UINT                cb802_1_H_len;
    UINT                cbIVlen=0,cbICVlen=0,cbMIClen=0,cbMACHdLen=0,cbFCSlen=4;
    UINT                cbMICHDR = 0;
    BOOL                bNeedACK,bRTS;
    PBYTE               pbyType,pbyMacHdr,pbyIVHead,pbyPayloadHead,pbyTxBufferAddr;
    BYTE                abySNAP_RFC1042[6] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00};
    BYTE                abySNAP_Bridgetunnel[6] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0xF8};
    UINT                uDuration;
    UINT                cbHeaderLength= 0,uPadding = 0;
    PVOID               pvRrvTime;
    PSMICHDRHead        pMICHDR;
    PVOID               pvRTS;
    PVOID               pvCTS;
    PVOID               pvTxDataHd;
    BYTE                byFBOption = AUTO_FB_NONE,byFragType;
    WORD                wTxBufSize;
    DWORD               dwMICKey0,dwMICKey1,dwMIC_Priority,dwCRC;
    PDWORD              pdwMIC_L,pdwMIC_R;
    BOOL                bSoftWEP = FALSE;




    pvRrvTime = pMICHDR = pvRTS = pvCTS = pvTxDataHd = NULL;
    if ((bNeedEncryption) && (pTransmitKey != NULL))  {
        if (((PSKeyTable) (pTransmitKey->pvKeyTable))->bSoftWEP == TRUE) {
            // WEP 256
            bSoftWEP = TRUE;
        }
    }

    pTxBufHead = (PTX_BUFFER) usbPacketBuf;
    memset(pTxBufHead, 0, sizeof(TX_BUFFER));

    // Get pkt type
    if (ntohs(psEthHeader->wType) > MAX_DATA_LEN) {
        if (pDevice->dwDiagRefCount == 0) {
            cb802_1_H_len = 8;
        } else {
            cb802_1_H_len = 2;
        }
    } else {
        cb802_1_H_len = 0;
    }

    cbFrameBodySize = uSkbPacketLen - U_HEADER_LEN + cb802_1_H_len;

    //Set packet type
    pTxBufHead->wFIFOCtl |= (WORD)(byPktType<<8);

    if (pDevice->dwDiagRefCount != 0) {
        bNeedACK = FALSE;
        pTxBufHead->wFIFOCtl = pTxBufHead->wFIFOCtl & (~FIFOCTL_NEEDACK);
    } else { //if (pDevice->dwDiagRefCount != 0) {
        if ((pDevice->eOPMode == OP_MODE_ADHOC) ||
            (pDevice->eOPMode == OP_MODE_AP)) {
            if (IS_MULTICAST_ADDRESS(&(psEthHeader->abyDstAddr[0])) ||
                IS_BROADCAST_ADDRESS(&(psEthHeader->abyDstAddr[0]))) {
                bNeedACK = FALSE;
                pTxBufHead->wFIFOCtl = pTxBufHead->wFIFOCtl & (~FIFOCTL_NEEDACK);
            }
            else {
                bNeedACK = TRUE;
                pTxBufHead->wFIFOCtl |= FIFOCTL_NEEDACK;
            }
        }
        else {
            // MSDUs in Infra mode always need ACK
            bNeedACK = TRUE;
            pTxBufHead->wFIFOCtl |= FIFOCTL_NEEDACK;
        }
    } //if (pDevice->dwDiagRefCount != 0) {

    pTxBufHead->wTimeStamp = DEFAULT_MSDU_LIFETIME_RES_64us;

    //Set FIFOCTL_LHEAD
    if (pDevice->bLongHeader)
        pTxBufHead->wFIFOCtl |= FIFOCTL_LHEAD;

    if (pDevice->bSoftwareGenCrcErr) {
        pTxBufHead->wFIFOCtl |= FIFOCTL_CRCDIS; // set tx descriptors to NO hardware CRC
    }

    //Set FRAGCTL_MACHDCNT
    if (pDevice->bLongHeader) {
        cbMACHdLen = WLAN_HDR_ADDR3_LEN + 6;
    } else {
        cbMACHdLen = WLAN_HDR_ADDR3_LEN;
    }
    pTxBufHead->wFragCtl |= (WORD)(cbMACHdLen << 10);

    //Set FIFOCTL_GrpAckPolicy
    if (pDevice->bGrpAckPolicy == TRUE) {//0000 0100 0000 0000
        pTxBufHead->wFIFOCtl |=	FIFOCTL_GRPACK;
    }

    //Set Auto Fallback Ctl
    if (wCurrentRate >= RATE_18M) {
        if (pDevice->byAutoFBCtrl == AUTO_FB_0) {
            pTxBufHead->wFIFOCtl |= FIFOCTL_AUTO_FB_0;
            byFBOption = AUTO_FB_0;
        } else if (pDevice->byAutoFBCtrl == AUTO_FB_1) {
            pTxBufHead->wFIFOCtl |= FIFOCTL_AUTO_FB_1;
            byFBOption = AUTO_FB_1;
        }
    }

    if (bSoftWEP != TRUE) {
        if ((bNeedEncryption) && (pTransmitKey != NULL))  { //WEP enabled
            if (pTransmitKey->byCipherSuite == KEY_CTL_WEP) { //WEP40 or WEP104
                pTxBufHead->wFragCtl |= FRAGCTL_LEGACY;
            }
            if (pTransmitKey->byCipherSuite == KEY_CTL_TKIP) {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Tx Set wFragCtl == FRAGCTL_TKIP\n");
                pTxBufHead->wFragCtl |= FRAGCTL_TKIP;
            }
            else if (pTransmitKey->byCipherSuite == KEY_CTL_CCMP) { //CCMP
                pTxBufHead->wFragCtl |= FRAGCTL_AES;
            }
        }
    }


    if ((bNeedEncryption) && (pTransmitKey != NULL))  {
        if (pTransmitKey->byCipherSuite == KEY_CTL_WEP) {
            cbIVlen = 4;
            cbICVlen = 4;
        }
        else if (pTransmitKey->byCipherSuite == KEY_CTL_TKIP) {
            cbIVlen = 8;//IV+ExtIV
            cbMIClen = 8;
            cbICVlen = 4;
        }
        if (pTransmitKey->byCipherSuite == KEY_CTL_CCMP) {
            cbIVlen = 8;//RSN Header
            cbICVlen = 8;//MIC
            cbMICHDR = sizeof(SMICHDRHead);
        }
        if (bSoftWEP == FALSE) {
            //MAC Header should be padding 0 to DW alignment.
            uPadding = 4 - (cbMACHdLen%4);
            uPadding %= 4;
        }
    }

    cbFrameSize = cbMACHdLen + cbIVlen + (cbFrameBodySize + cbMIClen) + cbICVlen + cbFCSlen;

    if ( (bNeedACK == FALSE) ||(cbFrameSize < pDevice->wRTSThreshold) ) {
        bRTS = FALSE;
    } else {
        bRTS = TRUE;
        pTxBufHead->wFIFOCtl |= (FIFOCTL_RTS | FIFOCTL_LRETRY);
    }

    pbyTxBufferAddr = (PBYTE) &(pTxBufHead->adwTxKey[0]);
    wTxBufSize = sizeof(STxBufHead);
    if (byPktType == PK_TYPE_11GB || byPktType == PK_TYPE_11GA) {//802.11g packet
        if (byFBOption == AUTO_FB_NONE) {
            if (bRTS == TRUE) {//RTS_need
                pvRrvTime = (PSRrvTime_gRTS) (pbyTxBufferAddr + wTxBufSize);
                pMICHDR = (PSMICHDRHead) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gRTS));
                pvRTS = (PSRTS_g) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gRTS) + cbMICHDR);
                pvCTS = NULL;
                pvTxDataHd = (PSTxDataHead_g) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gRTS) + cbMICHDR + sizeof(SRTS_g));
                cbHeaderLength = wTxBufSize + sizeof(SRrvTime_gRTS) + cbMICHDR + sizeof(SRTS_g) + sizeof(STxDataHead_g);
            }
            else { //RTS_needless
                pvRrvTime = (PSRrvTime_gCTS) (pbyTxBufferAddr + wTxBufSize);
                pMICHDR = (PSMICHDRHead) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gCTS));
                pvRTS = NULL;
                pvCTS = (PSCTS) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gCTS) + cbMICHDR);
                pvTxDataHd = (PSTxDataHead_g) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gCTS) + cbMICHDR + sizeof(SCTS));
                cbHeaderLength = wTxBufSize + sizeof(SRrvTime_gCTS) + cbMICHDR + sizeof(SCTS) + sizeof(STxDataHead_g);
            }
        } else {
            // Auto Fall Back
            if (bRTS == TRUE) {//RTS_need
                pvRrvTime = (PSRrvTime_gRTS) (pbyTxBufferAddr + wTxBufSize);
                pMICHDR = (PSMICHDRHead) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gRTS));
                pvRTS = (PSRTS_g_FB) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gRTS) + cbMICHDR);
                pvCTS = NULL;
                pvTxDataHd = (PSTxDataHead_g_FB) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gRTS) + cbMICHDR + sizeof(SRTS_g_FB));
                cbHeaderLength = wTxBufSize + sizeof(SRrvTime_gRTS) + cbMICHDR + sizeof(SRTS_g_FB) + sizeof(STxDataHead_g_FB);
            }
            else if (bRTS == FALSE) { //RTS_needless
                pvRrvTime = (PSRrvTime_gCTS) (pbyTxBufferAddr + wTxBufSize);
                pMICHDR = (PSMICHDRHead) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gCTS));
                pvRTS = NULL;
                pvCTS = (PSCTS_FB) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gCTS) + cbMICHDR);
                pvTxDataHd = (PSTxDataHead_g_FB) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gCTS) + cbMICHDR + sizeof(SCTS_FB));
                cbHeaderLength = wTxBufSize + sizeof(SRrvTime_gCTS) + cbMICHDR + sizeof(SCTS_FB) + sizeof(STxDataHead_g_FB);
            }
        } // Auto Fall Back
    }
    else {//802.11a/b packet
        if (byFBOption == AUTO_FB_NONE) {
            if (bRTS == TRUE) {//RTS_need
                pvRrvTime = (PSRrvTime_ab) (pbyTxBufferAddr + wTxBufSize);
                pMICHDR = (PSMICHDRHead) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab));
                pvRTS = (PSRTS_ab) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR);
                pvCTS = NULL;
                pvTxDataHd = (PSTxDataHead_ab) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR + sizeof(SRTS_ab));
                cbHeaderLength = wTxBufSize + sizeof(PSRrvTime_ab) + cbMICHDR + sizeof(SRTS_ab) + sizeof(STxDataHead_ab);
            }
            else if (bRTS == FALSE) { //RTS_needless, no MICHDR
                pvRrvTime = (PSRrvTime_ab) (pbyTxBufferAddr + wTxBufSize);
                pMICHDR = (PSMICHDRHead) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab));
                pvRTS = NULL;
                pvCTS = NULL;
                pvTxDataHd = (PSTxDataHead_ab) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR);
                cbHeaderLength = wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR + sizeof(STxDataHead_ab);
            }
        } else {
            // Auto Fall Back
            if (bRTS == TRUE) {//RTS_need
                pvRrvTime = (PSRrvTime_ab) (pbyTxBufferAddr + wTxBufSize);
                pMICHDR = (PSMICHDRHead) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab));
                pvRTS = (PSRTS_a_FB) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR);
                pvCTS = NULL;
                pvTxDataHd = (PSTxDataHead_a_FB) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR + sizeof(SRTS_a_FB));
                cbHeaderLength = wTxBufSize + sizeof(PSRrvTime_ab) + cbMICHDR + sizeof(SRTS_a_FB) + sizeof(STxDataHead_a_FB);
            }
            else if (bRTS == FALSE) { //RTS_needless
                pvRrvTime = (PSRrvTime_ab) (pbyTxBufferAddr + wTxBufSize);
                pMICHDR = (PSMICHDRHead) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab));
                pvRTS = NULL;
                pvCTS = NULL;
                pvTxDataHd = (PSTxDataHead_a_FB) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR);
                cbHeaderLength = wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR + sizeof(STxDataHead_a_FB);
            }
        } // Auto Fall Back
    }

    pbyMacHdr = (PBYTE)(pbyTxBufferAddr + cbHeaderLength);
    pbyIVHead = (PBYTE)(pbyMacHdr + cbMACHdLen + uPadding);
    pbyPayloadHead = (PBYTE)(pbyMacHdr + cbMACHdLen + uPadding + cbIVlen);


    //=========================
    //    No Fragmentation
    //=========================
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"No Fragmentation...\n");
    byFragType = FRAGCTL_NONFRAG;
    //uDMAIdx = TYPE_AC0DMA;
    //pTxBufHead = (PSTxBufHead) &(pTxBufHead->adwTxKey[0]);


    //Fill FIFO,RrvTime,RTS,and CTS
    s_vGenerateTxParameter(pDevice, byPktType, wCurrentRate, (PVOID)pbyTxBufferAddr, pvRrvTime, pvRTS, pvCTS,
                               cbFrameSize, bNeedACK, uDMAIdx, psEthHeader);
    //Fill DataHead
    uDuration = s_uFillDataHead(pDevice, byPktType, wCurrentRate, pvTxDataHd, cbFrameSize, uDMAIdx, bNeedACK,
                                    0, 0, 1/*uMACfragNum*/, byFBOption);
    // Generate TX MAC Header
    s_vGenerateMACHeader(pDevice, pbyMacHdr, (WORD)uDuration, psEthHeader, bNeedEncryption,
                           byFragType, uDMAIdx, 0);

    if (bNeedEncryption == TRUE) {
        //Fill TXKEY
        s_vFillTxKey(pDevice, (PBYTE)(pTxBufHead->adwTxKey), pbyIVHead, pTransmitKey,
                         pbyMacHdr, (WORD)cbFrameBodySize, (PBYTE)pMICHDR);

        if (pDevice->bEnableHostWEP) {
            pMgmt->sNodeDBTable[uNodeIndex].dwTSC47_16 = pTransmitKey->dwTSC47_16;
            pMgmt->sNodeDBTable[uNodeIndex].wTSC15_0 = pTransmitKey->wTSC15_0;
        }
    }

    // 802.1H
    if (ntohs(psEthHeader->wType) > MAX_DATA_LEN) {
        if (pDevice->dwDiagRefCount == 0) {
            if ( (psEthHeader->wType == TYPE_PKT_IPX) ||
                 (psEthHeader->wType == cpu_to_le16(0xF380))) {
                memcpy((PBYTE) (pbyPayloadHead), &abySNAP_Bridgetunnel[0], 6);
            } else {
                memcpy((PBYTE) (pbyPayloadHead), &abySNAP_RFC1042[0], 6);
            }
            pbyType = (PBYTE) (pbyPayloadHead + 6);
            memcpy(pbyType, &(psEthHeader->wType), sizeof(WORD));
        } else {
            memcpy((PBYTE) (pbyPayloadHead), &(psEthHeader->wType), sizeof(WORD));

        }

    }


    if (pPacket != NULL) {
        // Copy the Packet into a tx Buffer
        memcpy((pbyPayloadHead + cb802_1_H_len),
                 (pPacket + U_HEADER_LEN),
                 uSkbPacketLen - U_HEADER_LEN
                 );

    } else {
        // while bRelayPacketSend psEthHeader is point to header+payload
        memcpy((pbyPayloadHead + cb802_1_H_len), ((PBYTE)psEthHeader)+U_HEADER_LEN, uSkbPacketLen - U_HEADER_LEN);
    }

    ASSERT(uLength == cbNdisBodySize);

    if ((bNeedEncryption == TRUE) && (pTransmitKey != NULL) && (pTransmitKey->byCipherSuite == KEY_CTL_TKIP)) {

        ///////////////////////////////////////////////////////////////////

        if (pDevice->sMgmtObj.eAuthenMode == WMAC_AUTH_WPANONE) {
            dwMICKey0 = *(PDWORD)(&pTransmitKey->abyKey[16]);
            dwMICKey1 = *(PDWORD)(&pTransmitKey->abyKey[20]);
        }
        else if ((pTransmitKey->dwKeyIndex & AUTHENTICATOR_KEY) != 0) {
            dwMICKey0 = *(PDWORD)(&pTransmitKey->abyKey[16]);
            dwMICKey1 = *(PDWORD)(&pTransmitKey->abyKey[20]);
        }
        else {
            dwMICKey0 = *(PDWORD)(&pTransmitKey->abyKey[24]);
            dwMICKey1 = *(PDWORD)(&pTransmitKey->abyKey[28]);
        }
        // DO Software Michael
        MIC_vInit(dwMICKey0, dwMICKey1);
        MIC_vAppend((PBYTE)&(psEthHeader->abyDstAddr[0]), 12);
        dwMIC_Priority = 0;
        MIC_vAppend((PBYTE)&dwMIC_Priority, 4);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"MIC KEY: %lX, %lX\n", dwMICKey0, dwMICKey1);

        ///////////////////////////////////////////////////////////////////

        //DBG_PRN_GRP12(("Length:%d, %d\n", cbFrameBodySize, uFromHDtoPLDLength));
        //for (ii = 0; ii < cbFrameBodySize; ii++) {
        //    DBG_PRN_GRP12(("%02x ", *((PBYTE)((pbyPayloadHead + cb802_1_H_len) + ii))));
        //}
        //DBG_PRN_GRP12(("\n\n\n"));

        MIC_vAppend(pbyPayloadHead, cbFrameBodySize);

        pdwMIC_L = (PDWORD)(pbyPayloadHead + cbFrameBodySize);
        pdwMIC_R = (PDWORD)(pbyPayloadHead + cbFrameBodySize + 4);

        MIC_vGetMIC(pdwMIC_L, pdwMIC_R);
        MIC_vUnInit();

        if (pDevice->bTxMICFail == TRUE) {
            *pdwMIC_L = 0;
            *pdwMIC_R = 0;
            pDevice->bTxMICFail = FALSE;
        }
        //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"uLength: %d, %d\n", uLength, cbFrameBodySize);
        //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"cbReqCount:%d, %d, %d, %d\n", cbReqCount, cbHeaderLength, uPadding, cbIVlen);
        //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"MIC:%lX, %lX\n", *pdwMIC_L, *pdwMIC_R);
    }


    if (bSoftWEP == TRUE) {

        s_vSWencryption(pDevice, pTransmitKey, (pbyPayloadHead), (WORD)(cbFrameBodySize + cbMIClen));

    } else if (  ((pDevice->eEncryptionStatus == Ndis802_11Encryption1Enabled) && (bNeedEncryption == TRUE))  ||
          ((pDevice->eEncryptionStatus == Ndis802_11Encryption2Enabled) && (bNeedEncryption == TRUE))   ||
          ((pDevice->eEncryptionStatus == Ndis802_11Encryption3Enabled) && (bNeedEncryption == TRUE))      ) {
        cbFrameSize -= cbICVlen;
    }

    if (pDevice->bSoftwareGenCrcErr == TRUE) {
        UINT   cbLen;
        PDWORD pdwCRC;

        dwCRC = 0xFFFFFFFFL;
        cbLen = cbFrameSize - cbFCSlen;
        // calculate CRC, and wrtie CRC value to end of TD
        dwCRC = CRCdwGetCrc32Ex(pbyMacHdr, cbLen, dwCRC);
        pdwCRC = (PDWORD)(pbyMacHdr + cbLen);
        // finally, we must invert dwCRC to get the correct answer
        *pdwCRC = ~dwCRC;
        // Force Error
        *pdwCRC -= 1;
    } else {
        cbFrameSize -= cbFCSlen;
    }

    *pcbHeaderLen = cbHeaderLength;
    *pcbTotalLen = cbHeaderLength + cbFrameSize ;


    //Set FragCtl in TxBufferHead
    pTxBufHead->wFragCtl |= (WORD)byFragType;


    return TRUE;

}


/*+
 *
 * Description:
 *      Translate 802.3 to 802.11 header
 *
 * Parameters:
 *  In:
 *      pDevice         - Pointer to adpater
 *      dwTxBufferAddr  - Transmit Buffer
 *      pPacket         - Packet from upper layer
 *      cbPacketSize    - Transmit Data Length
 *  Out:
 *      pcbHeadSize         - Header size of MAC&Baseband control and 802.11 Header
 *      pcbAppendPayload    - size of append payload for 802.1H translation
 *
 * Return Value: none
 *
-*/

VOID
s_vGenerateMACHeader (
    IN PSDevice         pDevice,
    IN PBYTE            pbyBufferAddr,
    IN WORD             wDuration,
    IN PSEthernetHeader psEthHeader,
    IN BOOL             bNeedEncrypt,
    IN WORD             wFragType,
    IN UINT             uDMAIdx,
    IN UINT             uFragIdx
    )
{
    PS802_11Header  pMACHeader = (PS802_11Header)pbyBufferAddr;

    memset(pMACHeader, 0, (sizeof(S802_11Header)));  //- sizeof(pMACHeader->dwIV)));

    if (uDMAIdx == TYPE_ATIMDMA) {
    	pMACHeader->wFrameCtl = TYPE_802_11_ATIM;
    } else {
        pMACHeader->wFrameCtl = TYPE_802_11_DATA;
    }

    if (pDevice->eOPMode == OP_MODE_AP) {
        memcpy(&(pMACHeader->abyAddr1[0]), &(psEthHeader->abyDstAddr[0]), U_ETHER_ADDR_LEN);
        memcpy(&(pMACHeader->abyAddr2[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
        memcpy(&(pMACHeader->abyAddr3[0]), &(psEthHeader->abySrcAddr[0]), U_ETHER_ADDR_LEN);
        pMACHeader->wFrameCtl |= FC_FROMDS;
    }
    else {
        if (pDevice->eOPMode == OP_MODE_ADHOC) {
            memcpy(&(pMACHeader->abyAddr1[0]), &(psEthHeader->abyDstAddr[0]), U_ETHER_ADDR_LEN);
            memcpy(&(pMACHeader->abyAddr2[0]), &(psEthHeader->abySrcAddr[0]), U_ETHER_ADDR_LEN);
            memcpy(&(pMACHeader->abyAddr3[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
        }
        else {
            memcpy(&(pMACHeader->abyAddr3[0]), &(psEthHeader->abyDstAddr[0]), U_ETHER_ADDR_LEN);
            memcpy(&(pMACHeader->abyAddr2[0]), &(psEthHeader->abySrcAddr[0]), U_ETHER_ADDR_LEN);
            memcpy(&(pMACHeader->abyAddr1[0]), &(pDevice->abyBSSID[0]), U_ETHER_ADDR_LEN);
            pMACHeader->wFrameCtl |= FC_TODS;
        }
    }

    if (bNeedEncrypt)
        pMACHeader->wFrameCtl |= cpu_to_le16((WORD)WLAN_SET_FC_ISWEP(1));

    pMACHeader->wDurationID = cpu_to_le16(wDuration);

    if (pDevice->bLongHeader) {
        PWLAN_80211HDR_A4 pMACA4Header  = (PWLAN_80211HDR_A4) pbyBufferAddr;
        pMACHeader->wFrameCtl |= (FC_TODS | FC_FROMDS);
        memcpy(pMACA4Header->abyAddr4, pDevice->abyBSSID, WLAN_ADDR_LEN);
    }
    pMACHeader->wSeqCtl = cpu_to_le16(pDevice->wSeqCounter << 4);

    //Set FragNumber in Sequence Control
    pMACHeader->wSeqCtl |= cpu_to_le16((WORD)uFragIdx);

    if ((wFragType == FRAGCTL_ENDFRAG) || (wFragType == FRAGCTL_NONFRAG)) {
        pDevice->wSeqCounter++;
        if (pDevice->wSeqCounter > 0x0fff)
            pDevice->wSeqCounter = 0;
    }

    if ((wFragType == FRAGCTL_STAFRAG) || (wFragType == FRAGCTL_MIDFRAG)) { //StartFrag or MidFrag
        pMACHeader->wFrameCtl |= FC_MOREFRAG;
    }
}



/*+
 *
 * Description:
 *      Request instructs a MAC to transmit a 802.11 management packet through
 *      the adapter onto the medium.
 *
 * Parameters:
 *  In:
 *      hDeviceContext  - Pointer to the adapter
 *      pPacket         - A pointer to a descriptor for the packet to transmit
 *  Out:
 *      none
 *
 * Return Value: CMD_STATUS_PENDING if MAC Tx resource avaliable; otherwise FALSE
 *
-*/

CMD_STATUS csMgmt_xmit(
    IN  PSDevice pDevice,
    IN  PSTxMgmtPacket pPacket
    )
{
    BYTE            byPktType;
    PBYTE           pbyTxBufferAddr;
    PVOID           pvRTS;
    PSCTS           pCTS;
    PVOID           pvTxDataHd;
    UINT            uDuration;
    UINT            cbReqCount;
    PS802_11Header  pMACHeader;
    UINT            cbHeaderSize;
    UINT            cbFrameBodySize;
    BOOL            bNeedACK;
    BOOL            bIsPSPOLL = FALSE;
    PSTxBufHead     pTxBufHead;
    UINT            cbFrameSize;
    UINT            cbIVlen = 0;
    UINT            cbICVlen = 0;
    UINT            cbMIClen = 0;
    UINT            cbFCSlen = 4;
    UINT            uPadding = 0;
    WORD            wTxBufSize;
    UINT            cbMacHdLen;
    SEthernetHeader sEthHeader;
    PVOID           pvRrvTime;
    PVOID           pMICHDR;
    PSMgmtObject    pMgmt = &(pDevice->sMgmtObj);
    WORD            wCurrentRate = RATE_1M;
    PTX_BUFFER          pTX_Buffer;
    PUSB_SEND_CONTEXT   pContext;



    pContext = (PUSB_SEND_CONTEXT)s_vGetFreeContext(pDevice);

    if (NULL == pContext) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ManagementSend TX...NO CONTEXT!\n");
        return CMD_STATUS_RESOURCES;
    }

    pTX_Buffer = (PTX_BUFFER) (&pContext->Data[0]);
    pbyTxBufferAddr = (PBYTE)&(pTX_Buffer->adwTxKey[0]);
    cbFrameBodySize = pPacket->cbPayloadLen;
    pTxBufHead = (PSTxBufHead) pbyTxBufferAddr;
    wTxBufSize = sizeof(STxBufHead);
    memset(pTxBufHead, 0, wTxBufSize);

    if (pDevice->byBBType == BB_TYPE_11A) {
        wCurrentRate = RATE_6M;
        byPktType = PK_TYPE_11A;
    } else {
        wCurrentRate = RATE_1M;
        byPktType = PK_TYPE_11B;
    }

    // SetPower will cause error power TX state for OFDM Date packet in TX buffer.
    // 2004.11.11 Kyle -- Using OFDM power to tx MngPkt will decrease the connection capability.
    //                    And cmd timer will wait data pkt TX finish before scanning so it's OK
    //                    to set power here.
    if (pMgmt->eScanState != WMAC_NO_SCANNING) {
        RFbSetPower(pDevice, wCurrentRate, pDevice->byCurrentCh);
    } else {
        RFbSetPower(pDevice, wCurrentRate, pMgmt->uCurrChannel);
    }
    pDevice->wCurrentRate = wCurrentRate;


    //Set packet type
    if (byPktType == PK_TYPE_11A) {//0000 0000 0000 0000
        pTxBufHead->wFIFOCtl = 0;
    }
    else if (byPktType == PK_TYPE_11B) {//0000 0001 0000 0000
        pTxBufHead->wFIFOCtl |= FIFOCTL_11B;
    }
    else if (byPktType == PK_TYPE_11GB) {//0000 0010 0000 0000
        pTxBufHead->wFIFOCtl |= FIFOCTL_11GB;
    }
    else if (byPktType == PK_TYPE_11GA) {//0000 0011 0000 0000
        pTxBufHead->wFIFOCtl |= FIFOCTL_11GA;
    }

    pTxBufHead->wFIFOCtl |= FIFOCTL_TMOEN;
    pTxBufHead->wTimeStamp = cpu_to_le16(DEFAULT_MGN_LIFETIME_RES_64us);


    if (IS_MULTICAST_ADDRESS(&(pPacket->p80211Header->sA3.abyAddr1[0])) ||
        IS_BROADCAST_ADDRESS(&(pPacket->p80211Header->sA3.abyAddr1[0]))) {
        bNeedACK = FALSE;
    }
    else {
        bNeedACK = TRUE;
        pTxBufHead->wFIFOCtl |= FIFOCTL_NEEDACK;
    };

    if ((pMgmt->eCurrMode == WMAC_MODE_ESS_AP) ||
        (pMgmt->eCurrMode == WMAC_MODE_IBSS_STA) ) {

        pTxBufHead->wFIFOCtl |= FIFOCTL_LRETRY;
        //Set Preamble type always long
        //pDevice->byPreambleType = PREAMBLE_LONG;
        // probe-response don't retry
        //if ((pPacket->p80211Header->sA4.wFrameCtl & TYPE_SUBTYPE_MASK) == TYPE_MGMT_PROBE_RSP) {
        //     bNeedACK = FALSE;
        //     pTxBufHead->wFIFOCtl  &= (~FIFOCTL_NEEDACK);
        //}
    }

    pTxBufHead->wFIFOCtl |= (FIFOCTL_GENINT | FIFOCTL_ISDMA0);

    if ((pPacket->p80211Header->sA4.wFrameCtl & TYPE_SUBTYPE_MASK) == TYPE_CTL_PSPOLL) {
        bIsPSPOLL = TRUE;
        cbMacHdLen = WLAN_HDR_ADDR2_LEN;
    } else {
        cbMacHdLen = WLAN_HDR_ADDR3_LEN;
    }

    //Set FRAGCTL_MACHDCNT
    pTxBufHead->wFragCtl |= cpu_to_le16((WORD)(cbMacHdLen << 10));

    // Notes:
    // Although spec says MMPDU can be fragmented; In most case,
    // no one will send a MMPDU under fragmentation. With RTS may occur.
    pDevice->bAES = FALSE;  //Set FRAGCTL_WEPTYP

    if (WLAN_GET_FC_ISWEP(pPacket->p80211Header->sA4.wFrameCtl) != 0) {
        if (pDevice->eEncryptionStatus == Ndis802_11Encryption1Enabled) {
            cbIVlen = 4;
            cbICVlen = 4;
    	    pTxBufHead->wFragCtl |= FRAGCTL_LEGACY;
        }
        else if (pDevice->eEncryptionStatus == Ndis802_11Encryption2Enabled) {
            cbIVlen = 8;//IV+ExtIV
            cbMIClen = 8;
            cbICVlen = 4;
    	    pTxBufHead->wFragCtl |= FRAGCTL_TKIP;
    	    //We need to get seed here for filling TxKey entry.
            //TKIPvMixKey(pTransmitKey->abyKey, pDevice->abyCurrentNetAddr,
            //            pTransmitKey->wTSC15_0, pTransmitKey->dwTSC47_16, pDevice->abyPRNG);
        }
        else if (pDevice->eEncryptionStatus == Ndis802_11Encryption3Enabled) {
            cbIVlen = 8;//RSN Header
            cbICVlen = 8;//MIC
            pTxBufHead->wFragCtl |= FRAGCTL_AES;
            pDevice->bAES = TRUE;
        }
        //MAC Header should be padding 0 to DW alignment.
        uPadding = 4 - (cbMacHdLen%4);
        uPadding %= 4;
    }

    cbFrameSize = cbMacHdLen + cbFrameBodySize + cbIVlen + cbMIClen + cbICVlen + cbFCSlen;

    //Set FIFOCTL_GrpAckPolicy
    if (pDevice->bGrpAckPolicy == TRUE) {//0000 0100 0000 0000
        pTxBufHead->wFIFOCtl |=	FIFOCTL_GRPACK;
    }
    //the rest of pTxBufHead->wFragCtl:FragTyp will be set later in s_vFillFragParameter()

    //Set RrvTime/RTS/CTS Buffer
    if (byPktType == PK_TYPE_11GB || byPktType == PK_TYPE_11GA) {//802.11g packet

        pvRrvTime = (PSRrvTime_gCTS) (pbyTxBufferAddr + wTxBufSize);
        pMICHDR = NULL;
        pvRTS = NULL;
        pCTS = (PSCTS) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gCTS));
        pvTxDataHd = (PSTxDataHead_g) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gCTS) + sizeof(SCTS));
        cbHeaderSize = wTxBufSize + sizeof(SRrvTime_gCTS) + sizeof(SCTS) + sizeof(STxDataHead_g);
    }
    else { // 802.11a/b packet
        pvRrvTime = (PSRrvTime_ab) (pbyTxBufferAddr + wTxBufSize);
        pMICHDR = NULL;
        pvRTS = NULL;
        pCTS = NULL;
        pvTxDataHd = (PSTxDataHead_ab) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab));
        cbHeaderSize = wTxBufSize + sizeof(SRrvTime_ab) + sizeof(STxDataHead_ab);
    }

    memset((PVOID)(pbyTxBufferAddr + wTxBufSize), 0, (cbHeaderSize - wTxBufSize));

    memcpy(&(sEthHeader.abyDstAddr[0]), &(pPacket->p80211Header->sA3.abyAddr1[0]), U_ETHER_ADDR_LEN);
    memcpy(&(sEthHeader.abySrcAddr[0]), &(pPacket->p80211Header->sA3.abyAddr2[0]), U_ETHER_ADDR_LEN);
    //=========================
    //    No Fragmentation
    //=========================
    pTxBufHead->wFragCtl |= (WORD)FRAGCTL_NONFRAG;


    //Fill FIFO,RrvTime,RTS,and CTS
    s_vGenerateTxParameter(pDevice, byPktType, wCurrentRate,  pbyTxBufferAddr, pvRrvTime, pvRTS, pCTS,
                           cbFrameSize, bNeedACK, TYPE_TXDMA0, &sEthHeader);

    //Fill DataHead
    uDuration = s_uFillDataHead(pDevice, byPktType, wCurrentRate, pvTxDataHd, cbFrameSize, TYPE_TXDMA0, bNeedACK,
                                0, 0, 1, AUTO_FB_NONE);

    pMACHeader = (PS802_11Header) (pbyTxBufferAddr + cbHeaderSize);

    cbReqCount = cbHeaderSize + cbMacHdLen + uPadding + cbIVlen + cbFrameBodySize;

    if (WLAN_GET_FC_ISWEP(pPacket->p80211Header->sA4.wFrameCtl) != 0) {
        PBYTE           pbyIVHead;
        PBYTE           pbyPayloadHead;
        PBYTE           pbyBSSID;
        PSKeyItem       pTransmitKey = NULL;

        pbyIVHead = (PBYTE)(pbyTxBufferAddr + cbHeaderSize + cbMacHdLen + uPadding);
        pbyPayloadHead = (PBYTE)(pbyTxBufferAddr + cbHeaderSize + cbMacHdLen + uPadding + cbIVlen);
        do {
            if ((pDevice->eOPMode == OP_MODE_INFRASTRUCTURE) &&
                (pDevice->bLinkPass == TRUE)) {
                pbyBSSID = pDevice->abyBSSID;
                // get pairwise key
                if (KeybGetTransmitKey(&(pDevice->sKey), pbyBSSID, PAIRWISE_KEY, &pTransmitKey) == FALSE) {
                    // get group key
                    if(KeybGetTransmitKey(&(pDevice->sKey), pbyBSSID, GROUP_KEY, &pTransmitKey) == TRUE) {
                        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Get GTK.\n");
                        break;
                    }
                } else {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Get PTK.\n");
                    break;
                }
            }
            // get group key
            pbyBSSID = pDevice->abyBroadcastAddr;
            if(KeybGetTransmitKey(&(pDevice->sKey), pbyBSSID, GROUP_KEY, &pTransmitKey) == FALSE) {
                pTransmitKey = NULL;
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"KEY is NULL. OP Mode[%d]\n", pDevice->eOPMode);
            } else {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Get GTK.\n");
            }
        } while(FALSE);
        //Fill TXKEY
        s_vFillTxKey(pDevice, (PBYTE)(pTxBufHead->adwTxKey), pbyIVHead, pTransmitKey,
                     (PBYTE)pMACHeader, (WORD)cbFrameBodySize, NULL);

        memcpy(pMACHeader, pPacket->p80211Header, cbMacHdLen);
        memcpy(pbyPayloadHead, ((PBYTE)(pPacket->p80211Header) + cbMacHdLen),
                 cbFrameBodySize);
    }
    else {
        // Copy the Packet into a tx Buffer
        memcpy(pMACHeader, pPacket->p80211Header, pPacket->cbMPDULen);
    }

    pMACHeader->wSeqCtl = cpu_to_le16(pDevice->wSeqCounter << 4);
    pDevice->wSeqCounter++ ;
    if (pDevice->wSeqCounter > 0x0fff)
        pDevice->wSeqCounter = 0;

    if (bIsPSPOLL) {
        // The MAC will automatically replace the Duration-field of MAC header by Duration-field
        // of  FIFO control header.
        // This will cause AID-field of PS-POLL packet be incorrect (Because PS-POLL's AID field is
        // in the same place of other packet's Duration-field).
        // And it will cause Cisco-AP to issue Disassociation-packet
        if (byPktType == PK_TYPE_11GB || byPktType == PK_TYPE_11GA) {
            ((PSTxDataHead_g)pvTxDataHd)->wDuration_a = cpu_to_le16(pPacket->p80211Header->sA2.wDurationID);
            ((PSTxDataHead_g)pvTxDataHd)->wDuration_b = cpu_to_le16(pPacket->p80211Header->sA2.wDurationID);
        } else {
            ((PSTxDataHead_ab)pvTxDataHd)->wDuration = cpu_to_le16(pPacket->p80211Header->sA2.wDurationID);
        }
    }


    pTX_Buffer->wTxByteCount = cpu_to_le16((WORD)(cbReqCount));
    pTX_Buffer->byPKTNO = (BYTE) (((wCurrentRate<<4) &0x00F0) | ((pDevice->wSeqCounter - 1) & 0x000F));
    pTX_Buffer->byType = 0x00;

    pContext->pPacket = NULL;
    pContext->Type = CONTEXT_MGMT_PACKET;
    pContext->uBufLen = (WORD)cbReqCount + 4;  //USB header

    if (WLAN_GET_FC_TODS(pMACHeader->wFrameCtl) == 0) {
        s_vSaveTxPktInfo(pDevice, (BYTE) (pTX_Buffer->byPKTNO & 0x0F), &(pMACHeader->abyAddr1[0]),(WORD)cbFrameSize,pTX_Buffer->wFIFOCtl);
    }
    else {
        s_vSaveTxPktInfo(pDevice, (BYTE) (pTX_Buffer->byPKTNO & 0x0F), &(pMACHeader->abyAddr3[0]),(WORD)cbFrameSize,pTX_Buffer->wFIFOCtl);
    }

    PIPEnsSendBulkOut(pDevice,pContext);
    return CMD_STATUS_PENDING;
}


CMD_STATUS
csBeacon_xmit(
    IN  PSDevice pDevice,
    IN  PSTxMgmtPacket pPacket
    )
{

    UINT                cbFrameSize = pPacket->cbMPDULen + WLAN_FCS_LEN;
    UINT                cbHeaderSize = 0;
    WORD                wTxBufSize = sizeof(STxShortBufHead);
    PSTxShortBufHead    pTxBufHead;
    PS802_11Header      pMACHeader;
    PSTxDataHead_ab     pTxDataHead;
    WORD                wCurrentRate;
    UINT                cbFrameBodySize;
    UINT                cbReqCount;
    PBEACON_BUFFER      pTX_Buffer;
    PBYTE               pbyTxBufferAddr;
    PUSB_SEND_CONTEXT   pContext;
    CMD_STATUS          status;


    pContext = (PUSB_SEND_CONTEXT)s_vGetFreeContext(pDevice);
    if (NULL == pContext) {
        status = CMD_STATUS_RESOURCES;
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ManagementSend TX...NO CONTEXT!\n");
        return status ;
    }
    pTX_Buffer = (PBEACON_BUFFER) (&pContext->Data[0]);
    pbyTxBufferAddr = (PBYTE)&(pTX_Buffer->wFIFOCtl);

    cbFrameBodySize = pPacket->cbPayloadLen;

    pTxBufHead = (PSTxShortBufHead) pbyTxBufferAddr;
    wTxBufSize = sizeof(STxShortBufHead);
    memset(pTxBufHead, 0, wTxBufSize);

    if (pDevice->byBBType == BB_TYPE_11A) {
        wCurrentRate = RATE_6M;
        pTxDataHead = (PSTxDataHead_ab) (pbyTxBufferAddr + wTxBufSize);
        //Get SignalField,ServiceField,Length
        BBvCaculateParameter(pDevice, cbFrameSize, wCurrentRate, PK_TYPE_11A,
            (PWORD)&(pTxDataHead->wTransmitLength), (PBYTE)&(pTxDataHead->byServiceField), (PBYTE)&(pTxDataHead->bySignalField)
        );
        //Get Duration and TimeStampOff
        pTxDataHead->wDuration = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_A, cbFrameSize, PK_TYPE_11A,
                                                          wCurrentRate, FALSE, 0, 0, 1, AUTO_FB_NONE));
        pTxDataHead->wTimeStampOff = wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE];
        cbHeaderSize = wTxBufSize + sizeof(STxDataHead_ab);
    } else {
        wCurrentRate = RATE_1M;
        pTxBufHead->wFIFOCtl |= FIFOCTL_11B;
        pTxDataHead = (PSTxDataHead_ab) (pbyTxBufferAddr + wTxBufSize);
        //Get SignalField,ServiceField,Length
        BBvCaculateParameter(pDevice, cbFrameSize, wCurrentRate, PK_TYPE_11B,
            (PWORD)&(pTxDataHead->wTransmitLength), (PBYTE)&(pTxDataHead->byServiceField), (PBYTE)&(pTxDataHead->bySignalField)
        );
        //Get Duration and TimeStampOff
        pTxDataHead->wDuration = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_B, cbFrameSize, PK_TYPE_11B,
                                                          wCurrentRate, FALSE, 0, 0, 1, AUTO_FB_NONE));
        pTxDataHead->wTimeStampOff = wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE];
        cbHeaderSize = wTxBufSize + sizeof(STxDataHead_ab);
    }

    //Generate Beacon Header
    pMACHeader = (PS802_11Header)(pbyTxBufferAddr + cbHeaderSize);
    memcpy(pMACHeader, pPacket->p80211Header, pPacket->cbMPDULen);

    pMACHeader->wDurationID = 0;
    pMACHeader->wSeqCtl = cpu_to_le16(pDevice->wSeqCounter << 4);
    pDevice->wSeqCounter++ ;
    if (pDevice->wSeqCounter > 0x0fff)
        pDevice->wSeqCounter = 0;

    cbReqCount = cbHeaderSize + WLAN_HDR_ADDR3_LEN + cbFrameBodySize;

    pTX_Buffer->wTxByteCount = (WORD)cbReqCount;
    pTX_Buffer->byPKTNO = (BYTE) (((wCurrentRate<<4) &0x00F0) | ((pDevice->wSeqCounter - 1) & 0x000F));
    pTX_Buffer->byType = 0x01;

    pContext->pPacket = NULL;
    pContext->Type = CONTEXT_MGMT_PACKET;
    pContext->uBufLen = (WORD)cbReqCount + 4;  //USB header

    PIPEnsSendBulkOut(pDevice,pContext);
    return CMD_STATUS_PENDING;

}





VOID
vDMA0_tx_80211(PSDevice  pDevice, struct sk_buff *skb) {

    PSMgmtObject    pMgmt = &(pDevice->sMgmtObj);
    BYTE            byPktType;
    PBYTE           pbyTxBufferAddr;
    PVOID           pvRTS;
    PVOID           pvCTS;
    PVOID           pvTxDataHd;
    UINT            uDuration;
    UINT            cbReqCount;
    PS802_11Header  pMACHeader;
    UINT            cbHeaderSize;
    UINT            cbFrameBodySize;
    BOOL            bNeedACK;
    BOOL            bIsPSPOLL = FALSE;
    PSTxBufHead     pTxBufHead;
    UINT            cbFrameSize;
    UINT            cbIVlen = 0;
    UINT            cbICVlen = 0;
    UINT            cbMIClen = 0;
    UINT            cbFCSlen = 4;
    UINT            uPadding = 0;
    UINT            cbMICHDR = 0;
    UINT            uLength = 0;
    DWORD           dwMICKey0, dwMICKey1;
    DWORD           dwMIC_Priority;
    PDWORD          pdwMIC_L;
    PDWORD          pdwMIC_R;
    WORD            wTxBufSize;
    UINT            cbMacHdLen;
    SEthernetHeader sEthHeader;
    PVOID           pvRrvTime;
    PVOID           pMICHDR;
    WORD            wCurrentRate = RATE_1M;
    PUWLAN_80211HDR  p80211Header;
    UINT             uNodeIndex = 0;
    BOOL            bNodeExist = FALSE;
    SKeyItem        STempKey;
    PSKeyItem       pTransmitKey = NULL;
    PBYTE           pbyIVHead;
    PBYTE           pbyPayloadHead;
    PBYTE           pbyMacHdr;
    UINT            cbExtSuppRate = 0;
    PTX_BUFFER          pTX_Buffer;
    PUSB_SEND_CONTEXT   pContext;
//    PWLAN_IE        pItem;


    pvRrvTime = pMICHDR = pvRTS = pvCTS = pvTxDataHd = NULL;

    if(skb->len <= WLAN_HDR_ADDR3_LEN) {
       cbFrameBodySize = 0;
    }
    else {
       cbFrameBodySize = skb->len - WLAN_HDR_ADDR3_LEN;
    }
    p80211Header = (PUWLAN_80211HDR)skb->data;

    pContext = (PUSB_SEND_CONTEXT)s_vGetFreeContext(pDevice);

    if (NULL == pContext) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"DMA0 TX...NO CONTEXT!\n");
        dev_kfree_skb_irq(skb);
        return ;
    }

    pTX_Buffer = (PTX_BUFFER)(&pContext->Data[0]);
    pbyTxBufferAddr = (PBYTE)(&pTX_Buffer->adwTxKey[0]);
    pTxBufHead = (PSTxBufHead) pbyTxBufferAddr;
    wTxBufSize = sizeof(STxBufHead);
    memset(pTxBufHead, 0, wTxBufSize);

    if (pDevice->byBBType == BB_TYPE_11A) {
        wCurrentRate = RATE_6M;
        byPktType = PK_TYPE_11A;
    } else {
        wCurrentRate = RATE_1M;
        byPktType = PK_TYPE_11B;
    }

    // SetPower will cause error power TX state for OFDM Date packet in TX buffer.
    // 2004.11.11 Kyle -- Using OFDM power to tx MngPkt will decrease the connection capability.
    //                    And cmd timer will wait data pkt TX finish before scanning so it's OK
    //                    to set power here.
    if (pMgmt->eScanState != WMAC_NO_SCANNING) {
        RFbSetPower(pDevice, wCurrentRate, pDevice->byCurrentCh);
    } else {
        RFbSetPower(pDevice, wCurrentRate, pMgmt->uCurrChannel);
    }

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"vDMA0_tx_80211: p80211Header->sA3.wFrameCtl = %x \n", p80211Header->sA3.wFrameCtl);

    //Set packet type
    if (byPktType == PK_TYPE_11A) {//0000 0000 0000 0000
        pTxBufHead->wFIFOCtl = 0;
    }
    else if (byPktType == PK_TYPE_11B) {//0000 0001 0000 0000
        pTxBufHead->wFIFOCtl |= FIFOCTL_11B;
    }
    else if (byPktType == PK_TYPE_11GB) {//0000 0010 0000 0000
        pTxBufHead->wFIFOCtl |= FIFOCTL_11GB;
    }
    else if (byPktType == PK_TYPE_11GA) {//0000 0011 0000 0000
        pTxBufHead->wFIFOCtl |= FIFOCTL_11GA;
    }

    pTxBufHead->wFIFOCtl |= FIFOCTL_TMOEN;
    pTxBufHead->wTimeStamp = cpu_to_le16(DEFAULT_MGN_LIFETIME_RES_64us);


    if (IS_MULTICAST_ADDRESS(&(p80211Header->sA3.abyAddr1[0])) ||
        IS_BROADCAST_ADDRESS(&(p80211Header->sA3.abyAddr1[0]))) {
        bNeedACK = FALSE;
        if (pDevice->bEnableHostWEP) {
            uNodeIndex = 0;
            bNodeExist = TRUE;
        };
    }
    else {
        if (pDevice->bEnableHostWEP) {
            if (BSSbIsSTAInNodeDB(pDevice, (PBYTE)(p80211Header->sA3.abyAddr1), &uNodeIndex))
                bNodeExist = TRUE;
        };
        bNeedACK = TRUE;
        pTxBufHead->wFIFOCtl |= FIFOCTL_NEEDACK;
    };

    if ((pMgmt->eCurrMode == WMAC_MODE_ESS_AP) ||
        (pMgmt->eCurrMode == WMAC_MODE_IBSS_STA) ) {

        pTxBufHead->wFIFOCtl |= FIFOCTL_LRETRY;
        //Set Preamble type always long
        //pDevice->byPreambleType = PREAMBLE_LONG;

        // probe-response don't retry
        //if ((p80211Header->sA4.wFrameCtl & TYPE_SUBTYPE_MASK) == TYPE_MGMT_PROBE_RSP) {
        //     bNeedACK = FALSE;
        //     pTxBufHead->wFIFOCtl  &= (~FIFOCTL_NEEDACK);
        //}
    }

    pTxBufHead->wFIFOCtl |= (FIFOCTL_GENINT | FIFOCTL_ISDMA0);

    if ((p80211Header->sA4.wFrameCtl & TYPE_SUBTYPE_MASK) == TYPE_CTL_PSPOLL) {
        bIsPSPOLL = TRUE;
        cbMacHdLen = WLAN_HDR_ADDR2_LEN;
    } else {
        cbMacHdLen = WLAN_HDR_ADDR3_LEN;
    }

    // hostapd deamon ext support rate patch
    if (WLAN_GET_FC_FSTYPE(p80211Header->sA4.wFrameCtl) == WLAN_FSTYPE_ASSOCRESP) {

        if (((PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates)->len != 0) {
            cbExtSuppRate += ((PWLAN_IE_SUPP_RATES)pMgmt->abyCurrSuppRates)->len + WLAN_IEHDR_LEN;
         }

        if (((PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates)->len != 0) {
            cbExtSuppRate += ((PWLAN_IE_SUPP_RATES)pMgmt->abyCurrExtSuppRates)->len + WLAN_IEHDR_LEN;
         }

         if (cbExtSuppRate >0) {
            cbFrameBodySize = WLAN_ASSOCRESP_OFF_SUPP_RATES;
         }
    }


    //Set FRAGCTL_MACHDCNT
    pTxBufHead->wFragCtl |= cpu_to_le16((WORD)cbMacHdLen << 10);

    // Notes:
    // Although spec says MMPDU can be fragmented; In most case,
    // no one will send a MMPDU under fragmentation. With RTS may occur.
    pDevice->bAES = FALSE;  //Set FRAGCTL_WEPTYP


    if (WLAN_GET_FC_ISWEP(p80211Header->sA4.wFrameCtl) != 0) {
        if (pDevice->eEncryptionStatus == Ndis802_11Encryption1Enabled) {
            cbIVlen = 4;
            cbICVlen = 4;
    	    pTxBufHead->wFragCtl |= FRAGCTL_LEGACY;
        }
        else if (pDevice->eEncryptionStatus == Ndis802_11Encryption2Enabled) {
            cbIVlen = 8;//IV+ExtIV
            cbMIClen = 8;
            cbICVlen = 4;
    	    pTxBufHead->wFragCtl |= FRAGCTL_TKIP;
    	    //We need to get seed here for filling TxKey entry.
            //TKIPvMixKey(pTransmitKey->abyKey, pDevice->abyCurrentNetAddr,
            //            pTransmitKey->wTSC15_0, pTransmitKey->dwTSC47_16, pDevice->abyPRNG);
        }
        else if (pDevice->eEncryptionStatus == Ndis802_11Encryption3Enabled) {
            cbIVlen = 8;//RSN Header
            cbICVlen = 8;//MIC
            cbMICHDR = sizeof(SMICHDRHead);
            pTxBufHead->wFragCtl |= FRAGCTL_AES;
            pDevice->bAES = TRUE;
        }
        //MAC Header should be padding 0 to DW alignment.
        uPadding = 4 - (cbMacHdLen%4);
        uPadding %= 4;
    }

    cbFrameSize = cbMacHdLen + cbFrameBodySize + cbIVlen + cbMIClen + cbICVlen + cbFCSlen + cbExtSuppRate;

    //Set FIFOCTL_GrpAckPolicy
    if (pDevice->bGrpAckPolicy == TRUE) {//0000 0100 0000 0000
        pTxBufHead->wFIFOCtl |=	FIFOCTL_GRPACK;
    }
    //the rest of pTxBufHead->wFragCtl:FragTyp will be set later in s_vFillFragParameter()


    if (byPktType == PK_TYPE_11GB || byPktType == PK_TYPE_11GA) {//802.11g packet

        pvRrvTime = (PSRrvTime_gCTS) (pbyTxBufferAddr + wTxBufSize);
        pMICHDR = (PSMICHDRHead) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gCTS));
        pvRTS = NULL;
        pvCTS = (PSCTS) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gCTS) + cbMICHDR);
        pvTxDataHd = (PSTxDataHead_g) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_gCTS) + cbMICHDR + sizeof(SCTS));
        cbHeaderSize = wTxBufSize + sizeof(SRrvTime_gCTS) + cbMICHDR + sizeof(SCTS) + sizeof(STxDataHead_g);

    }
    else {//802.11a/b packet

        pvRrvTime = (PSRrvTime_ab) (pbyTxBufferAddr + wTxBufSize);
        pMICHDR = (PSMICHDRHead) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab));
        pvRTS = NULL;
        pvCTS = NULL;
        pvTxDataHd = (PSTxDataHead_ab) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR);
        cbHeaderSize = wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR + sizeof(STxDataHead_ab);
    }
    memset((PVOID)(pbyTxBufferAddr + wTxBufSize), 0, (cbHeaderSize - wTxBufSize));
    memcpy(&(sEthHeader.abyDstAddr[0]), &(p80211Header->sA3.abyAddr1[0]), U_ETHER_ADDR_LEN);
    memcpy(&(sEthHeader.abySrcAddr[0]), &(p80211Header->sA3.abyAddr2[0]), U_ETHER_ADDR_LEN);
    //=========================
    //    No Fragmentation
    //=========================
    pTxBufHead->wFragCtl |= (WORD)FRAGCTL_NONFRAG;


    //Fill FIFO,RrvTime,RTS,and CTS
    s_vGenerateTxParameter(pDevice, byPktType, wCurrentRate, pbyTxBufferAddr, pvRrvTime, pvRTS, pvCTS,
                           cbFrameSize, bNeedACK, TYPE_TXDMA0, &sEthHeader);

    //Fill DataHead
    uDuration = s_uFillDataHead(pDevice, byPktType, wCurrentRate, pvTxDataHd, cbFrameSize, TYPE_TXDMA0, bNeedACK,
                                0, 0, 1, AUTO_FB_NONE);

    pMACHeader = (PS802_11Header) (pbyTxBufferAddr + cbHeaderSize);

    cbReqCount =/*
 * Copyrig + cbMacHdLen + uPaddingTechnIVls, In(cbFrameBodying TechnoIClen)TechnExtSuppRatet (c) 1pbyologir = (PBYTE)(pbyTxBufferAddrTechn * Copyright can redPayload * Cute it and/or istribuechnologies, Inc.
 * All rights rs of the GIVral Public License as published by
 * the Free Sht (c) 1// Copy the Packet into a tx dify
 of thmemcpyense as pu, skb->data,chnologies,tion) any lversion set to 0, patch for hostapd deamonof theMAC * Cop->wd.
 *Ctl &= cpu_to_le16(0xfffcs of thistributedNU General , (hope thatblished by
 *) it d.
 *
 * Thistion) any lreplace support rateWARRANTY; without even the( only * You sh11M)of thif (WLAN_GET_FC_FSTYPE(p80211warrantysA4. of
 * ME) == e aloith th_ASSOCRESP) {of thc License software; y != 0ion, Inc.,
 c Licens((Pe aloIE_SUPP_RATES)pMgmt->abyCurrtware; ys)->s reet, Fth Floor, BosFOR A PARTIICULAR PURPOSE.blishc License for ,dle WMAC/802.3/80e: May 2
 * File: rxtx.c
 *
 * on Chen
 *
 * Date: May 20 MA 02110-1301 USA.
 *
 * File: rxtx.c
 *
 * Purpose+ Free IEHDR_LENn Chen
 *
 * Date: May 2s of th11 headerton, MA 02110-1301 USA.
 *
 * File: rxtxsoftware; y Purpose: handle WMAC/802.3/802.11 rx & tx functions
 *
 * Author: Lyndesereter - Generate tx dma requried parameter.
 *      s_vGenerateMACHeon Chen
 *
 * Date: May 20, 2003
 *
 * Fon
 *      c
 *      s_vGenerateTxParameter - Generate tx dma requried paron
 *      csMgmt_xs_vGenerateMACHeader - Translate 802.3 to 802.11 hea}of th}on) any lSet wepic License along
 * wISWEPis program; if not, write to tet, Fift, Inc.,
 * 51pDevice->bEnableHostWEtion, Inc.,
 y 20,TransmitKey = &STempKey 802.11 headerder
 *      ->byCipherSuite =0, 2003
sNodeDBTTSHe[uftwaIndex].     s_vSWenc- fulfill tx encrypt key
 * dwKeyader
ryption- Software encrypt header
 * dma0
 *  - fulfill tx encrypt key
 * uKeyLengthryption- Software encrypt header
 *uWexKey *
 */x_80211- tx 802.11 frame via dTSC47_16    vGenerateFIFOHeader- Generate tx #include- fulfill tx encrypt key
 *  "ba15_0ryption- Software encrypt header
 *nclude " 802.11 headeristributcrypt key
 * abyKeyon Chen
 *
 * Date&tion- Software encrypt header
 *aby.h"
#i[0]on Chen
 *
 * DateRevision History:
 *
 */dle WMAC/802.3/80o 802.11 heTSHead-der
 *   der
 *      s!= NULL) && 
#include "rf.h     s_vSWencry= KEY_CTL_TKIP)l parameter.
---*dwMICKey"tki*(PDWORD)(&#include "rf.h"
#inc[16]o 802.11 header

/*----1----------------  Static Variables 20----802.11 header// DO Software Michael802.11 headerMIC_vInit(

/*-----,----------o 802.11 headergleveAppend( it and&(sEth * Cop_SM_Dst* it[0]), 12--------------------_Priorit s_v"
#include "hos------------------  /

/*---------, 4o 802.11 headerDBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"DMA0_tx_ pro:MIC ---: %lX,> in\n"   =MSG_LE    =MSG_LEVEL_802.11 headeru *
 */

# Networking Technologies, Inc.
 * All rights r               -------------or modify
 * it un       Public License for more de "iocmd.h/

/*-Lute i-------] = {
        {384, 288, 
 *
 * Author: Lynd- fulfill tx en/

/*-R // Long Preamble
        {384, 192, 130, 113, 54, 43,  +----- wTimeStampOff[2][GetMIC(24, 23},,, 24, 23}EL_INFO;

/*--------Unl           {RATE_12M *      s_vFilTxMICFailes  TRUEfill RTS ctl hea, //*24, 23}, //"
#include "hos // fallback}, /e1
    };
const WORM, RATE_12M, RATE_18M FALSE 802.11 headerns --------ne CRITICAL_PACKET_LEN      256    // i       : %d, %dct se       ublic License for mo*/
#define CRITICAL_PACKET_LEN      256    // i996, 2003 :E_12M,       1ct se996, 2003 e RT * Copyrig,nc.
 * Ale RTre Foundatiodefine CRITICAL_PACKET_LEN      256    // iMIC:%lx-dirxct sefallback_UR_BA_F1  M, RDefinitions --------s_vFillTxKey     s_v,e it and/omodif * C->adwUR_BA),on; either,.h"
#endif

/on Chen
 *
 * Date: Mayted in the (-----
 * Author: Lyn
#define pMICHD RTSDUR_AA_F1 *      s_vFillRTSHead- fulfill RTS ctl headsdb.h"
#include "mac.h"
#include "basebanryptclude "card.h"
#include- fulfill tx en.h"
#include "tcrc.h"
#include "wctl.h"SaveTxPktInfo(
  "wctl.h"
#include ns --------------   s_vFilyLocalID <= REV_ID_VT3253_A1-----2.11 header
s_vSWencryption_F0    8
#       10
#dehe GNU General  DATADURrved.
 *
 * This program is c Definitions    IN WORDied warranty Seq MERHANTABILITY or
);

statRD   003 er <<-----*/
#tRate,
    IN PVOID ++ ack_ra *      s_vFi  IN PVOID  > 0x0fffndle WMAC/  pvRrvTime,
    IN P[2][5SDUR_AcensbIsPSPO----PSDevice p// The MAC will automaticallyils.
 *
 versDura
sta-field oframeSh* Cop byNeedACK,
    IN            of  FIFO control       .            cbisSize,
cause AID    IN UINPS-N UI pon.
 *be incorrect (BelDataHPSDevic'sHead    IN is            in   bNsame .
 *
 of othere pDevi'sNeedACK,
    IN));


static
UAnd its_uFillDataHCisco-AP ANYissue DisassociACK,
  pDevi  13

/*-----byPktTypses  PK_ftwar11GB || agIdx,
    IN UINT     cAfill RTS ctl hea((PSTxData * C_g)pv  IN BYd)->weedACK,
_a        wCurrent program; if no2.   );



IDback_rate1
    ,
    IN BYTE     byFBOption
    );




btatic
VOID
s_vGenerateMACHeader (
    IN PSDevice      } else    uMACfragNum,
    IN BYTE  ab  byFBOption
    );



Addr,
    IN WORD             wDuration,
    IN PSEthern    byPktTypeTX_dify
 ORD xByte2003 VIA TABILITY orPSDevice 6, 2003 BYTE        IN UINT byPKTNOute t andn, Mwrxtxente; y<<4) &0x00F0) |Ctl
);

stat  IN PVOID  - 1) &OID 00FFillTxKey(
    IN  PSD,
       INou can rContext->pion.
 *= hop of the   pbyHdry,
    CONTEXT_MGMT_PACKETWORD       wPayluBufes, =DATADUR_A6, 2003 V+ 4;    USB          pvCTS,
e along
 * wTODS(ied warranty of
 * ME the  Fifth Floor,ce
 aveTxPktInfo_F0    8
#dDevice,y(
    IN  PSDevice     IF), &  PSKeyItem  aby* it1-----ATADUR_A_F0  
#def    IN UINT  er ppTraack_ra    uDtHeader psEthHe PBYTE            pbyPayloadHead,
    IN  WORD             wPayloadSize
    );

stat3c
UINT
s_uGetTxRsvTime (
    IN PSDevice pDevice,
    INPIPEnsSendBulkOut_F0    8
     wPaevice,
return ;

}
PktT//ftwareC0DMA e
 * tx
/*
 * Descri

sta:engtwCurrTxe pDevicvia  IN UI(DMA1)
 LengtPa.
 *tersORD wCIWORD wCurre
);

stne RTSDUR- PoiOID  to   bNadaptam i wCurreskb  };
const WOINT     uDMAIdx byPk    IN U*  OutORD wCurrevoid
s_vFilRIN BYTValue:
/*--
 */PktTNTSTATUS
nsDMAcket pDevi(    bIN  PSevice,
F0    8
 WORD    UINT     DMAIdx   IN BYTEstruct sk_buff *skb    b)
PSDeviPS * FObj BYT 20, 200s_vFntRate,
  sPSDevicevice,
     byFType,
      sToWrencry0,u * Coptic
VO"
#incl  byPktType,
   pt header
,
    IN Ut an2.11 header
se ask[8] = {1, 2, 4, 8, 0x10L   2     4     80}ack_ra----2.11 header
wAID
    IN BOOL            Idx,
  
    INOOLOL           NeedE );


staRATE_24M, RATEPSma0
temctl header
 *      s_v/*--ack_ra   );

static
U illTxKey- fulf  byPktType,
   ii  wCurrentRate,
    IN ----_UseGTKRATE_24M, RATErentRate,
    IN BYTEDeAu*/

#byFBOption
 N BOOL         pbyBSSEthHeaderentRate,
    IN BodeExisDevibyFBOption
 USB_SEND_
    OU      wPaBYTE     byPktType,
  fConvertedion.
 ption
    IUFFERtl heade  IN UIN
    IN BYTE     byDurstatus  IN PSEthernetHeader pKeen Stre=vRTS,
    I  IN  PBYTEack_ratatic
Vnet_d  s_v_IN Ps* pSktTys_vF,
    IN BktTy;
//#ifdef WPA_SM_er
 *N PSD
      extern SWPAResult wpa_);


/ IN BendifSDevic  IN UINT     cbFrxeapol_k  s_vE_24M,    pvCTS,
   IN BerxtxModses  WMAC_MODE_ESS_Ation,  13

/*--------------uA bNe2003 VIt, Fifth Floor, Bosdev_kfree_skb_irqSee back_rate1
     IN BYTength,
    IN WORD wFIFOCtIS_MULTICAST_ADDRESS------- See the
 *)   PSDevice pDevicbFrameLength,
    IuFragIdx,
    IN UINTRATEack_rate1
    
static
PVOoftware encry0].bPSlRTSHe---------------*/    byP_queue_tail(load      pContext = pDevsTxPSQ->bB)e hopback_rate1
           pD   pContext = pDevwEnntextCnt++InUse = TRUE;
    //HOUT Ax maCTS ctDate: May 20, 2003
 *
    Map[0] |=    bNee0]InUse = TRUE;
    Context = NULL;
  ce,
    IN break;
   mu BOOst/broad  reNT cbFould0
        {RATE_12M, RATE_12yBB,
   != BBINT     Andle WMAC/802.3/80pe,
    IN UINT cbFra = SA.
_2Mack_rate1
    tHeae pDevice, BYTE byPktNum, PBYTE pbyDestAddr, WO4RD wPktLength, // long preamble typD wFIFOCtl)
{

);

staticPic);


y,
    PREAMBLE_SHORTTSDUR_AA_F1 tHeadere->cbTD; ii++) {
BSSbIsSTAInftware_F0    8
#define CEVEL_DEBUG, &pt header
----------------*/ii++) {
        pContext = ppt header
 * ce->apTD[ii];
        if (pCo(pContext->bBoolInUs   IN BYTE byPktNum,
    IN PBYT  pContext>bBoolIni = TX_PKT_MULTI;
      IN BYTE byPktNum,
    IN PBYTE pContext;
            break;
ak;
        }
    }
    if ( ii ==der psEttkip.h"
#include "tcrc.h"
#include "sEthHeade}
    if ( ii == pDevice->cbTD ) { = wF>> 3         bNee = wF& 7RT(MSG_LEVEL_DEBUG,ne CRITICAL_PACKET_LEN      256    //  "Set: pDevice->cbTD ) {%d]=#definen Chen
 *
 * Date: May 20    pD, U_ETHERfineAddr, pbyDestAddr, U_ETHER_i = TX_PKT_UNI;

    pSt KERN_INFO"No Free Tx  Tx Context\n");
 ength,
 PRetur decided from nodD wFIFOCtl)
{
    PSStatCounter              IN BYTE byPktNum,
    IN PBYTE  IN BbFrameLeng=&(pDevice->scStxstic);


  PDWORD          pstAddr))
        pStatistic->abyTxPktInfo[byPktNum].byShortpbyDestA_24M, RATE_36M}, // ADCAST_ADDRESS(pbyDestAddr))
 
);

static         dwRe = TX_PKT_UNI;

   ktInfo[bTX_PKT_UNI;

    pStaT_ADDRESS(pbyDestAddr))
        pSLONGloadLen,
    OUT PBYTE      pMICHDR
i = 0; ii < pDevice->cbTD; ii++Context\n"ns ------------,
    IN UIN-------fill RTS ctl heaRITICAL_PACKET_LEN      256        "Unknown STA not fou   In      DB \n"----------------LL;
    PUSB_SEND_CONTEXT   pReturnContext = NULL;
       byPktType   pbyHute iragmentSize,
   )sATE_1Free   pbyHntRate,
i = TX_P
statyPRNG+3, 

/*---- cpu_to_le    if (pTransmitKey->uKeyLength == abyPRNG+3, aDurat memcpy(pDevicbyPRNG, (PBYTE)&(dwRevIVCounter) IN BYTOOL   _RESOURCESvice,
   FOR A PARTIC
    IN BTxatic Functions  ----   else if (IS_MULTICAU_HEADEMACHei = //mik,
  d: IN     m    check ----------challenge--->(
    IN BOOLProtocol_Vut WIT;ce->sc802.1x RD  en BOOvIVCbyBuf+11, pTon.
 _     text\n");
   ransmitKey->uKeyLength);
       h,
    Ior_   i  IN PSEtheKey_info = TX_PTransmitKey->aby  IN  e that[memcpy(pbyBuRT(MSG_     }
    pdwIV &= WEP_IV_MASK;//00+1RT(MSG_        }
     111111 11111111
        *pd+1+2RT(MSG_d IV aft
VOIex << 30);
        *pdwIV = +1]    8)|);
        pDevice->dwIVCounte2------- *      s_vFingth == WLAN_w,
    INftwarPKTt si_1xfill RTS ctl heif, MAransmitKey->aby==1) || (pTransmitKey->byC2)----
	N  PSKeyI     }
    ==3----mcpy(pDevicOR;
                me f.
 * tr
 *ram is d        pTransmitKey---------------Device->cbTD; ii++}
    } else i!(le32(*pdw& BIT3----- //WPA or RSN group            me
		    ansmitKey->abyK8------nsmitKey->abyK9C15_0+Devisend 2/2 key      if(        }
     ==254vIVCounter;



    //Fil2(pDevice->dwIVCountfeedAKey->ue PubDevice        PRINT_K("ce->emcp     }, 3);
tHeader psEthHeadere->abyPRNG, 16);
        // Make IV
        memcpy(pdwIV, pDevice->aby2(re     optPRNG, 3);

      pDevice->Key->uKeyLengt completed!! memcpy(pDevice->aIndex & 0xf;

pdwIV,tHeade ifnsmitKey->abyKey, pDyPRNG);
        memy, pDeviCurrpairs    /r,
                 pD     pTransmitKey->wTSC15_0, pTransmitKe, 3);
      // Make IV
        memcpy    pbyIVHead,
    IN  PSKey0 is ExtIV
  tKey->uKeyLengtpdwExtIV = cpu_to_ExtIV);BYTE      pMICHpy(pD}
}, (PBYTE)&(dwRevIVCounter), 3);
                me<---;
        }   s_vFillR);


stalRTSHe8M, RATE_24M, RATE_3 BYTE             Device->cbTD; // get er
 *    pDevyBuf+3, po= cpu_to_le32(p------ic
PVOID
s_vGetFreeContext(
    PSTA) {
 _vGenerateTxParam       *pdwI
   tFreeConteOOL are Fou   PSDevice pDevibNeedAck,
  itKey == NULaAck,
    IN U=&(pDevice->scSIndepdwEwise& 0xc0) | 0x220 is ExtIV
KeybGeter
 *      Use  {
      #define Dk,
  , PAIRWISE_KEY, ---  Static V the = KEY_CTL_WEP) {
    vice->byKeyIndeentNel MICHDR0
        *pM else iR = 0x59;
        *((PBYTE)(pMICHDR+1)) = 0; //GROUPity
        memcpy(pMICHRATE_24M, RATE_36M}, // T     cbFrameLength,
          }
        TKIPvMixKey(pbyBuf, (PBYTE)&(dwRevIVCounter), 3)Get GTK.pu_to_le32(pTransmitKey->dwTSCbreakmitKey->wTSC15_0++;
  BYTE      pMICHDR
netHeader psEthHeader)) = HIBYTE(LOWORD(pTransmitKey->dwTSC47_16));
 P      *((PBYTE)(pMICHDR+11)) BYTE(LOWORD(pTransmitKeContext\n");
 ktInfo
static
PVOID
s_vGetFreeContext(
 IBORD)(pTrDestAddr))
        ExtIV = cpu_to_le32(ngth == WLAN_WEP40_KEYL
    TO_DS,
   and FROM1
      --> ransm1rameS* itess1smitKey->wTSC15_0    if (pTransmitKey->uKeyLength == );
  Serach_le3:  memcpy(pDevice->a   INor (ii,
    ii< 6} el++ndle WMAC/802.3/80 HIBYTE(LOWORD(pTransmitKey->dwTSC47_16)%x  me, *eambk,
  +iiBYTE            if (pDevice->bLongHeader) {
             memcpKey->dwTSC47_16);

        //Fill MICHDR0
        *pMICDR = 0x59;
        *((PBYTE)(pMICHDR+1)) = 0; // TxPriority
        memcpy(pMICHRATE_pMICHDR+13)) = LOBYTE(pTransmitKey->wTSCContext\n");
    , 6);
        *((PBYTE)(pMExtIV = cpu_to_le32(pTrarn ((PVOFill // 0x20 is ExtI= HIBYTE(HIWORD(pTransmitKey->dwTSC47_16));
        *((PBYTE)(pMICHDR+9)) = LDR+2, &(pMACHeader->abyAdINT
s_uGetDataDuration (
2.11 header
 *  CHDR+14)) = HIBYTE(wPayloadLen);
        *)(pMICHDR+10)) = HIBYTE(LOWORD(pTransmitKey->dwTSC47_16)*((PB  *(KEY isDurat.    I            *pdwIV |=)) = 28; // HLEN[7:BYTE      pMICHDR
WORD wFIFOCtl)
{
     HIBYTE(LOWORD(pTransmitKey->dwTSC47_16)NOT  }
}


static
VOID
s_vSWencryption (
    IN  PSDevice            *((PBYTE)(pMICHDR+12))47_16));
        *((PBYTE)(pMICHDR+10YTE(LOWORD(pTransmitKey->dwTSC47_16));
        *((PBYTE)(pMICH

    if (p while(= KEY_      if(pTra *      s_vFillRTSHead- fulfill RTS ctl    if (pTransmitKey->uKeyLength == acdma0:P232_ieLengM, RATE__ADDRESS(ppMACHeader      // Make IV
        *pdwIV = 0;
        *(pby header
 *      s_vFillTxKey- fulfill tx encrypt key
 *      s_vSWencryption- Software encrypt header
 *      vDMA0_tx_80211- tx 802.11 frame via dma0
 *      vGenerateFIFOHeader- Generate tx FIFO ctl header
 *
 * Revision History:
 *
 */

#include "device.h"
#include "rxtx.h"
#include "tether.h"
#include "card.h"
#include "bssdb.h"
#include "mac.h"
#include "baseband.h"
#include "michael.h"
#include "tkip.h"
#include "tcrc.h"
#include "wctl.h"
#include "hostap.h"
#include "rf.h"
#include "datarate.h"
#include "usbpipe.h"

#ifdef WPA_SM_Transtatus
#include "iocmd.h"
#endif

/*---------------------  Static Definitiocpy(pDevice->aagIdx,
    oadHeaddwIVCounter)on.
       e == KEY_CTL_WEP) {Fixe; yion, Inc.,
 * 51tic
VOID
s_vSaveTx=ktInfo(PSDevBFifth Floor, Boston, )
{
    PConnecvIVCyDest>Addr, WPubl(pMICHDR+38, (PBYTE)PktNum, PBYTE pbyDestAddr, W11RD wPktLength,    *((PBYTE)(pMICHDR+12))ally, we must invert dwCATADUR (PDWORD)(pbyPayloadHead   return;

    if (pTranstHeader psEthHeaderFOCtl
);

statice, dwICV);//ICV(PaylpTransmitKey->wTSC15_0))(PDWORD)(pbyPayloadHead _vGe aft6Mc Header
        *pdwEPktNum, PBYTE pbyDestAddr, W6et the correct answer
        *pdwICV = CV = (PDWORD)(pbyPayloadHead + wPaylo54blic Li pDevice, BYTE byPktNum, PBYTE pbyDestAddr, W5stic=&(pDevice->  PSKeyItem           pTransmcpu_to_le32(~dwICV);
        // RC4 encryption
        rc4_init(&pDevice->SBox,ice,
    IN BYTE     byPktCV = (PDWORD)eOPvGetFreeOPext(
 ADHOCize);
        //h,
 dhocentR{
    PDWORD           DBe->abyPRNG, TKIP_       ii;

    DBG_P((PBYTE)(pMIgth == WLAN_WEP40_KEYL----c Header
        *pdw// Mul    re ataHhigheVOID) pReturndwIV = (PDWORD) pbyIVHead;
    PDWORD          pdwExtIV = (Pxt =E)pbyIVHead+4);
    WORD       tic);


    if (IS_BROADCA/Fill TXKEY
    if (pTransmitKey == NULL)
        return           pDevice,
    IN=========================MultiUni = TX_PKT_BROAD;
   Time(pDevice->byPreambleType, byPktTypCAST_ADDRESS(pbyDe (BYTE)(((pDevice->byKeyIndex;
    PDWORD          pdwExtIV = (PDWORD) ((PBYTE)pbyIVHead+4);
    WORD     = (PS802_11Header)pbyHdrBuf;
    DWORD           dwRevIVCounter;



    //Fil/Fill TXKEY
    if (pTransmitKey == NULL)
        return;

    dwRevIVCouUT PBYTE      pMICHDR
= BBuGetFrameTime(pDevice->Device->dwIVCounter);
    *pdwIV = pDevice->dwIVCounter;
    pDev    IN UINT cbFrameLength,RITICAL_PACKET_LEN      256    // iFN ){

    meLengisvSWenrentRIN B e; y:SWencryp     // Ap,  3
*/
static
UINT
s_uDevice            pDevice,
    IN  PSKeyINeedAck) {
        retu
static
VOID
s_vSaveTxPktInfo(PSDevice pDevice, BYTE b BYTE byPktNum, PBYTE pbyDestAddr, WORD wPktLength,   IN  PSKeyItem           pTransm   PSStatCounter           pStatistietailsf uDMAIvMgrCreateOwn);
 ()'   wCurrenpCCKBasicRate);
    }
    else if (byRTSRsvType =// /cts reserved time
[]cpu_to_le32(pDevice->dwIVCounter);
    *pdwIV = pDevice-tatisti;

    uRrvTime = uRTSTime = uCTSTime = uAckTime = uDatNot aTime = 0;
byPktType ==basicetFra.Devi+17)) = 28; // HLEN[7:);
        *((PYTE     byPktTy   cbFrameLength,
    IN WORD     wRINFRASTRUCTURY_CTL_WEP) {
    // InfraP232_{
    PDWORD      AP = 0;,======== 0f (IS_BROADCAST_ADDRES   uAckTime = BBuGetFrameTime(pDevice->byPreambleType, byPkpy(pDevice->aMASK) {
            pDevice->dwIVCounter = 0;
        }
    } 
static
VOID
s_vSaveTxPktInfo(PSDevicNeedAck) {
    ally, we must invert dwCRC to RD wPktLength, dwIVCounterACKmbleType, byPktType, 14, pDevice->byTopTopCCKBameTMBasicRate);
    }
    else if (byRTSRsvTyOFDM== 3) { //CTSTxRr============, pDevice->abyPRNG, T/============================================vice->byTopOFDMBasicRate);pCCKBasicRate);
        uAcvType == 3) { //CTSTxRrvTime_ba, only in 2.4GHZ
        uCTSTime = BBuGetFrameTime(pDevy(pDevice->a
    IN  PSDevice   pDevice,
    IN dma_tx:, byPktType, 14, pDevice-%d      ambleType, byPktType, cbTime(pDevBYTE byDur!Type,
    IN UINT cbFra       *(pbyIScheduleComma----HANDL ICV afte, Free CMD_SETPOWER,
/*---pherSuite == KEY_CTL_WEP) PBYTE pbyDestayloadSiadSize);
              //Ap UINT     BpherSuite == KEY_ BYTE             LOBYTE(HIWORD(pTrRITICAL_PACKET_LEN      256    // intohs Pkt ey->w%04RTSDUR   bNK) {
            pDevice->dwBYTE        FOCtl
);

stat    IN UINT     cbLIVCounter = 0;
        }
    } elsN BYTE             byFBOption
tType,
    IN WORD     wRate,
    IN BOOL  edAck,
    IN UINTuFragIdx,
    IN UINT     cbLastFragments ExtIV
        *pdwIV |= cpu_to_le16((WORD)(pTran0));
        //Append IV&ExtIV after Mac Header
        *pdwCV = er
 *      s_           memcpy(*/
#define CRITICAL_PACKET_LEN      256    // ifon't Find TXtatie->byPreambleType, byPktType, 14, pD  if (byRTSRsvType == 0) { //RTSTxRFrameLength,
  LOBYTE(HIWORD(pTransmitKey->dwTSCRITICAL_PACKET_LEN      256    // ierror:tatic
VOGTK!!~~pu_to_le32(pTransmitKey->device->byTFrameLength,
    IN WORD wCurrentRate
    )TSTime = uCTSTime = uAckTime = uDataBuGePTK [%lXncrypti.11 frame via dma0
 *  *((PBYTE)(pMICHDR+11)) = LOBVHead+3) = (BYTE)(((pDevice->byansmitKey->dwTSC47_16));
       RATE_24M, RATE_36M}, // falad, wPayloadSizCntMeasurICV);2ize);
        // fin  IN WORD             }
        TKIPPBYTE)(pMI;
   1 PVOID .---- PVOID ype, cbsInvoked
            bre= s_uGetTxRsvTime(pDevice, bylRTSHead- fulfill RTS ctl heamentSize,cbFrameLenget, FiansmitKey->wTSC15_015_0));
    ateFIFOHeader- Generate tx FIFO ctl &/ TxPriority
(bNeedAck) {
        returagIdx == (uMACfragNum-2)) {
            	uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbLastFrntSize, wRate, bNeedAck);
            } etKey->dwTSC47_16++;
ataTime;

    utInfo[by#if);
    }
    ele if      // Make IV
         memcpy({
  	
        *Key->uvGetFreeConteAUTH_WPAPSK)||ype, byPktType, 14, pevice->byTopOF2DMBa))eedAck) {
        rebyPRNG, (PBYTE)&(dwRevIVCounter), 3) uAckTi
    ->tx_droppime(pDevice->byPrensmitKey->uKeyLengthFAILURM, RATE_24M, RATE       pTBYTE)(pMIC== 1)) {//Non Frag or Last Frag
       RITICAL_PACKET_LEN      256    // i IN BYTnpvCTSkey+17)) = 28; // HLEN[7:          } else {
                retur0;
            }
        }
        els{//First Frag or Mid Frag
           #WORD wFIFOCtl)
{
= (uMACfragNum-2)){
            	uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbLastFragmentSize, wRate, bNeedAck);
            } else {
                uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wRate, bN-----ime = BBuGetFrameTime(N BYTE     byFBO  IN_byload
 oWirelessUsb_F0    8
#         
 *      s_vGenerateTxParamit and/&     wPaylIN B------ntSize, wRate, E      pbyIVHead,
    IN  IV &=len,FBOption
 WORD wRate,gth == WLANeak;

    case DATADUR_A_F0:    /hope that i_vGenerateTxPaevice->byPrn Chen
 *
 * Date: May 20,ation (
    IN PSDevype, byPktType, 14, pDevic&     pvRTS, & IN PVOID   ader - Translate 802.3 to 8Time(pDeve->uSIFS + uNextPk  memcpy(pMICHDR+38     wPaylbBoolInUs    E_24M, RATE_24MpTransmitKey->abyKey, pTransmitKey->uKeyLengthor Mid Frag
 meTime(pDev
        uAlRTSHePSvGetFreeRATE ion, Inc.,
 * 51 !  // Make       modirstte = RATE_18M    return uRrvTime;
}

//byFtRate,
  0: 5GHz, 1onteDIS4Ghz
SAVINGstatic
UINT
sice->byPreambleTy   else if (wRa      }
            uDMAIdx,
    IN UINute i   );


s)& } else {_F0
	    illTxKey(
    IN  PSDevice   pDevice,
 e->byTopOFDMBasicRate      pbyBuf,
    IN  PBYTE      pbyIVHead,
    IN  PSKeyItem  pTransmit            uFrATADUR IN PVOID   PBYTE      pbyHdrBuf,
    IN  WORD       wPayloadLen,
    OUTDATATE      pMICHDR
    );

static
VOID
s_v IN PVOID    + 4 yPreIN  PSDevice     PBYTE            pbyPayloadHead,
    IN  WORD             wPayloa   pbyHdrtatic Functions  ---------ATADUR ( IN PVOID   -     pvRTS) (
    IN PSDevice pDevieLength PSDrType pDevice,
    IN BYTE byRTSRsvType, cbFrameLengthWORD     LOBYTE(HIWORD(pTrSEthewReas *pdwe alo PBYTREASON_
/*-or Mid FrIFS;
    return uRrvTime;
}

//byF= RATE_54M;

	     DE>byT
#define &pt1[FB_
UINT
s_uGetif(      !=yLengthPENDING= BBuGetF
	    else { //First Frag or Mid FbyPRNG, (PBYTE)&(dwRevIVCounFB_0) {
                i uNeWORD wFIFContext = byPktTeLength,
    IN WORD wCurreRelaye pDevicSC47_(AC1DMA)      rx dpc.
s_vFillCTSHead (
    IN PSDevice pDevice,
    IN UINT     uDMAIdx,
    IN BYTE     Buf,
   e,
    IN PVOID    pvrtRate
  BYTE     cme);
  5] =   IN rx exDatnetSC15_0 sizeINT     cbFrameLenpDev,rag or
    IN BOOL     bNee BOOL   wRatife pDevicis cter vo dma1;pTxDat/Fillak;

      IINT 
breambevice->yPktT WORD     wCurrentRate,
    IN BYTEIN BOOL  pbySkbIN B   IN BYTE     byF uIN B->uSime = BBuGetFrameTit header
TSHead(
    IN PSDevice         pDevice,
    IN BYTE             byPktType,
    IN PVOID            pvRTS,
    IN Ur,
    IN WORD           rType,
    IN UINT  rentRate,
    IN BYTE             byFBOption
    IN PSDevice pDevice,
    I    );

static
UINT
s_uGetDataDuration (
IN BOOL     bNeedAck,
    IN UFragmentSize,
    1[FB_RATE0
                return 0;
       uMACfragNum,
    IN BYTE     byFBOption
    );


static
UINT
s_uGetRTSCTSDuration (
    IN PSDevice pDevice,
    IN BYTE byDurType,
    IN UINT cbFrame----*/
byPRNG+3, pTransmitKey->abyKey, pTransmitKey->uKeyLength);
        /*--cbLaRTSRsvTypNeedAck) {
 IN BYTE_24M, RATEif(pTransmitKey->uKeyLength == WLAN_WEP40_KEYLEN) {
        	uAckT memcpy(pbyBuf+8,

        // Make IV
        *pdwIV = 0;
        *(pbyIVHead+3) = (BYTE)(((pDevice->byKeyInde;
        *((PBYTE>abyAddr3[0]), 6);
        wValue = pMACHeadereqCtl;
        wValue &= 0x000F;
        wValue = cpu_to_le16(wValue);
        memcpy(pMICHDR+38, (P)&wValue, 2); // MSKSEQCTL
          if (pTransmitKey->uKeyLength == atic
VOID
s_vSWencryption (
    IN  PSDevice     , pDevice->abyPRNG, TYTE(LOWORD(pTransmitKey->dwTSC47_16));
        *((PBYTE)(pGetFrameTime(pDevice->byPrevice->byTopCCKBasicRate);censcbFrameLeng>t, Fifth Floor, Bosder
 *      s_vFillTxKey- fulfill tx encrypt key
 *      s_vSWencryption- Software encrypt header
 *      vDMA0_tx_80211- tx 802.11 frame via dma0
 *      vGenerateFIFOHeader- Generate tx FIFO ctl header
 *
 * Revision History:
 *
 */

#include "device.h"
#include "rxtx.h"
#include "tether.h"
#include "card.h"
#include "bssdb.h"
#include "mac.h"
#include "baseband.h"
#include "michael.h"
#include "tkip.h"
#include "tcrc.h"
#include "wctl.h"
#include "hostap.h"
#include "rf.h"
#include "datarate.h"
.h"
#include "usbpipe.h"

#ifdef WPA_SM_Transtatus
#include "iocmd.cmd.h"
#endif

/*---------------------  Statitic Definitions if (wRate < RATntSize, wRate, b----------------on Frag or }
        }
	    else { //First Frag or Mid Frag
uNextPktTime = s_uGetTxRs        /Append ICV after payload
        dwICV = CRCdwGetCrc32Ex(pbyPayloadHead, wPayloadSize, dwICV);//ICV(Payload)
        pdwICV = (PDWORD)(pbyPayloadHead + wPayloadSize);
        // finally, we must invert dwCRC to get the correct answer
        *pdwICV = cpu_to_le32(~dwICV);
        // RC4 encryption
        rc4_init(&pDevice->SBox, pDevice->abyPRNG, TKIP_KEY_LEN);
        rc4_encrypt(&pDevice->SBox, pbyPayloadHead, pbyPayloadHead, wPayloadSize+cbICVlen);
        //=======================================================================
    }
}




/*byPktType : PK_TYPE_11A     0
             PK_TYPE_11B     1
             PK_TYPE_11GB    2
             PK_TYPE_11GA    3
*/
static
UINT
s_uGetTxRsvTime (
    IN PSDevice pDevice,
    IN BYTE     byPktType,
    IN UINT     cpbyIVHead;
    PDWORD          pdwExtIV = (PDWORD) ((PBYTE)pbyIVHead+4);
yPktTSTime + uAckTime + uDataTime + 3*pDevice->uSIFS;
    return uRrvTime;
}

//byF= RATE_54M;

	     :2.4Ghz
static
UINT
s_uGetDataDuration (
    IN PSDevice pDevice,
  IN BYTE     byDurType,
    IN UI   }
	IN PVOID     Time(pDev + U_CRCACHePreamby latYTE     bN pDevic* Thn usbSC15_0   *(   if
 * Tour 
s_vam is d 1){nd byPktversirp.pDevice->uSIFS + uNextPktTime);
            }
        }
        break;

    case DATADUR_A_FF0:    //DATADUR_A_F0
	    if (((uMACfragNum==1)) || (bLastFrag==1)) {/Time(pDeviounter IN UI Frag
            if(bNeedAck){
            	uAckT      	uAckTTime(pDevice->byPreambleType, byPktType, 14, pDevicce->byTopOFDMBasicRate);
                return  (pDevice->uSIFS + uAckTime);
            } else {

                return 0;
            }
        }
	    else { //First Frag or Mid Frag
uNextPktTime = s_uGetTxRsice, byPktType, cbLastFragmentSize, wFB_Opt0[FB_RATE0][wRate-RATE_18M], bNeedAck);
                } else {
                    uNextPktTime = s_uGetTxRsvTime(pDevice, byPktType, cbFrameLength, wFB_Opt0[FB_RATE0][wRate-RATE_18M], bNeedAuration (
            }
	        } else { // (byFBOption == AUTO_FB_1)
                if (wRate < RATE_18M)
                    wRate = RATE_18M;
                else if (wRate > RATE_54M)
                    wRate = RATE_54M;

	            if(uFragIdx == (uMACfragNum-2)){
            	    uNextPktTime = s_uGetTxRsvTime(pDevi IN BYTDevice}

