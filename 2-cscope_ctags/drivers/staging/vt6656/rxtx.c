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
 * Copyrig + cbMacHdLen + uPaddingTechnIVls, In(cbFrameBodying l rigoIClen)l rigExtSuppRatet (c) 1pbyologir = (PBYTE)(pbyTxBufferAddrl rigetworking ht can redPayloadetwoute it and/or istribuprogrstrie resec.NetwAll erms s rs of the GIVral Public License as pversshed byNetwion;Free Shou can //worki(at yPacket into a tx dify
ndatiomemcpy of the Li, skb->data,ished by
 *tion) any lversion set to 0, patch for hostapd deamondationMACetwork->wd FreCtl &= cpu_to_le16(0xfffcundatio as pubtedNU Genether, (hope thatcense, or
 * )lic of
 *NetwT A Pseful,
 * replace support rateWARRANTY; without even(at ( only * You sh11M) FOR Af (WLAN_GET_FC_FSTYPE(p80211warrantysA4. ofNetwME) == e aloith th_ASSOCRESP) {datioion 2 of tsoftware; y != 0ion* the ,
 ion 2 of((PFree IE_SUPP_RATES)pMgmt->abyCurrklin Strs)->s reet, Fth Floor, BosFOR A PARTIICULAR PURPOSE.censeion 2 of t; wi,dle WMAC/802.3/80e: May 2NetwFile: rxtx.cLicenseon ChenLicenseDat: May 200 MA prog0-1301 USA License03
 *
 * Functions:Purpose+your oIEHDR_LEN *      s_vGenerateTxParundatio11 h* Coptiftheter - Generate tx dma requried parranklin Stre*      : hann Chen
 *
 * Date2.11 rx &is pfunc morsLicenseAuthor: Lyndesereter - PURPOSteis prma requried pa.
 *ter Free fulfs_vired durMACHe
 *      s_vGenerateTxPara, 2003 dma reqo   s fulfincti fulfill tx data TxPillDataHequired duration
 *      s_uFil get rtx/ctss * F_xll tx data durat Cop - Transldura
 * D ANYfunctiohea}datio}eful,
 * Set wepsion 2 of thlong reqwISWEPis program; if not, wriurato t: haniftth Floor,* 51pDevice->bEnableHostWE morth Floor,GetRTrame mitKey = &STemp   s *      s_derder requried->byCipherSufrag=RTSCTSDusNodeDBTTSHe[unkliIndex].fulfill SWenc- fulfillis pencrypt ke
 * (dwKeytx e
11 fion- Sanklin 802.11 frl tx e via ma0 requx_80211- tx 802.11 frame viauKeyLength    vGenerateFIFOHeader- Generate tuWex   scens/x_ prog-is pfunctiof.
 * via dTSC47_16 ful tx data FIFOme- gequired duratio#includex_80211- tx 802.11 frame via "ba15_0    vGenerateFIFOHeader- Generate taseban "- fulfill tx eA PARTIC.11 frame viaabyKey
 *      s_vGenera& vGenerateFIFOHeader- Generate taby.h"
#i[0]
 *      s_vGeneraRevi WITHHistory:Licens/n Chen
 *
 * Datee
 *      sencrad-ncrypt kencrypt key
 s!= NULL) && 
"baseban "rf.h      vDMA0_try= KEY_CTL_TKIP)luFillDataHea---*dwMICKey"tki*(PDWORD)(&------------- anstnc[16]e
 *      s_rate
/*----1----//static int  Static VariTSHes 20//st-------------// DOerateFIFOHMichael-------------MIC_vInit(---------,//static i---------------gleveAppend(lic Lic&(sEthetwork_SM_Dst* it[0]), 12/
//static int */

/_Prioritill iables------hos------------*/

/*  /               4---------------DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"DMA0_tx_lFra:MIC ---: %lX,> in\n"   =PACKET fuld
     _LEN-------------unclude

# NetworkThis progrd by
 * the Free Software Fo fulfiimeStampO//static int or morogram---- unimeStam version 2 of t; wimore ----iocmd.h------LPubli-------] = {(c) 1    {384, 288, ataDuration - get tx_80211- tx 802------R // Long Preamble
        {384, 192, 130, 113, 54, 43,  +s ----wTimeStampOff[2][GetMIC(24, 13},,, RATE_24EL  // ;------------UnlTimeStampOf{SA.
_12Mrequried duFilTxMICFaiel   TRUE11- tRTS ctlGene, //*RATE_24M //---------  Stat, //fallbackk_rae1(c) 1};
const WORM,     {RATTE_12M, 8M FALSE- fulfill tx ensff[2][MAXne CRITICAL_PACKETACHe      25e "bs// i       : %d, %dct se       26, 209, 54, 43, 37,*/
#defi fallback_rate0
        {RATE_6M , RATE996TSCTSD :M, RATE   1
#1 RATEine RTSDURe RTetworking ,he Free S    re Foundatiote1
    };


#define RTSDUR_BB       0
#defMIC:%lx-dirx RATED wFB_Op_UR_BA_F1  ATE_De1
 iuGetDff[2][MAXATE_12lT"
#inulfill ,blic Licen {
  etwo->adw 6
#d),on; either,ariabendif

/
 *      s_vGenerateTxPted i of t (-----Duration - get rate1
   pMICHD, RADUR_Adefin_12M, RATE_12lRns ----_80211- t RATE_36M},dsdbariables------mac--------------baseban11 f-------card------------x_80211- tx 802--------------tcr---*/

static
Vwctl.h"Sav *  ktInfo(
 TE pbyDes----------    7
#defins -----RATE_12yLocalID <= REV_ID_VT3253_A-*/
//------------ic Classes  vGe_F0_F0 8
#   1
#de0rateon; AR PURPOSE. DATADURrvec License forlFragPar is c DUR_AA_F1      IN  {RD s_uram; if  Seq MERHANTABILITY or
ht (statRD   _AA  r <<IFOCtk_rate; y,(c) 1IN PVOID ++ 1   ra-------------,
    IN PV > 0x0fff- manageme  pvRrv  {RHead,
     RAT5   13
2 ofbIsPSPOIFOCPS   s_v p// The ed ww1- tautom    allyils Licenbut Durae,
 -field oe "caShtwork byNeedACKHead,
   N PSEtherneof  nclu controck_rate0*     static
cbisyrig,
cause AIN P,
   UINPS-  IN pon Frebe incorrect (BelDataH       's --- (
    i    bCurrentRi4, 2bNs"car Licenof oADURe     s's uDMAIdx,
    I)ht (e,
  ic
UAnd its_u----  byPCisco-AP ANYissue DisassociAIdx,
     s  13         byPktTyps8M, PK_nklin11GB || agIdxHead,
   UINTtic
UIAc Functions  ---((PSTx  byetwo_g)pv,
   BYd)->wuDMAIdx,_aCurrentRwrxtxentlFragParameter-2*   Frame
IDF1    dur[5] = Head,
   t anCurrebyFBO  vGe(c) 1  IN P
bength,IN Pice
uGetTxRsvTime- get WOR,
            SEthe} elTE_12MuMACfragNumDevice,
    IN abBYTE            pbyBuff* itHead,
   tTypN PSEthernet weedAlfillration,
  EADUR4, 28agIdx,
 eTX_rogram    xByteTSDURVIA     wCurren    IN PSe RTSDUR   IN PBYSize,
    IbyPKTNOPublc Lic    w * Fent Str<<4) &0x00F0) |CtltRate,
  me,
    IN P- 1) &N PV00FCTSDUR_BAuration,

   IN UINtion,ouof theContext->pipDevic= hopndation;  pbyHdryIN UINCONTEXT_MGMTate0
             wNU GuBuf
 * = PSDevi_Ae RTSDURV+ 4;  INUSBwCurrentRapvCTS,
ader
 *     TODS(e,
    IN WORwrite toD     l pale WMAC/ce
 Addr,
    INtic
VOID
sd   s_v,m  pTransmitKe IN PSEtheIF), &mitKKeyItem    y-----*/
//ID
s_vSWtic
Vrate1tSize,
    INer ppTra       er pDtme- getptatiHe it anN PSEthernetpbyNU Gene ---x,
    IN            wFragTyNU Geneyrig    pbyBu,
  3c
    
s_uGetTxRs   INDuration,
    IN PS    s_vIN UINT PIPEnsSendBulkOuttic
VOID
DR
    )    IN return ;

}
Idx,//nklin C0DMA e * (ax
A NetwDescrieedAc:*
 *ID
s_Txe (
    d.h"ze,
  (DMA1)
  *
 *Pa Freters    wCI     ID
s_vtRate,
n       1- Poi IN Pto,
   adapt  INevice pskb= {
        {   IN UIuDMAIdxuDMAIy(
    I*  OutPSDevice pvoidice
FilR,
    Value:---------Idx,NTSTATUS
nsDMAn.
 *    s(   uDnsmitK    IN  BYTE byR            IN UI  pvCTSce,
    Istru RATk_buff *byPkt  b)
D     PS reqObjvFiletRTSCTS    nTxBufHeadsD          IN PSDeYTE x,
 y,
    IsToWrasses0,uetworkdr,
  ength,
 uDMAIdx,
 IN UI- GenerateentSize,
 c LiDevice pDevic thek[8amble1, 2, 4, 8, 0x10LRATE   IN4   IN80}vice    PSDevice pDeviwAIDevice,
  OO    N WORD   agmentSration,OOL   IN WORD    uDMEpbyBufsta    {24ATE_18MPSFIFOtemns  ---------------*_v----vice,
   bNeedAckth,
 TSDUR_BAStaticINT             iiVOID
s_vGexBufHead,
   

/*-UseGTK   byFBOption
  IN UINT     cbFt anDeAu  // TE          r,
    IN WORD pbyBS    me- ge  byPktType,
    odeExis    TE          USB_SEND_yRTSROUHDR
    )   IN PBYTE           fConverteduf,
           pIUFFERatic
UINize,
   evice,
    IN PBYTEDur,
  usINT          e BYTE    Keen Stre=vR      IN ransmitt anvice   ddr,
 net_dDataD_    s* pSdx,
    Type,
    dx,
;
//#ifme (WPAtionate t,
   ,
    Iext    SWPAResult wpa_Frame/
       10      ize,
    IN UINbFrxeapol_kDataDbyFBOptce        
    );e * FMod   INen
 _MODE_ESS_Alfill UINT     uFr  )
{
   uA bN      uF ctl pale WMAC/802.dev_kfree_skb_irqSee evice         p
    IN*
 */IN WORD       wncluCtIS_MULack_ST_ADDRESSIFOCtl
)D_COthT cb)8, 2RsvTime (
    ed.
 * *
 */
    INuFrFragmentSize,
    SA.
vice         p PSDevicPVOateFIFOHeader0].bPS------/
//static int */MACfrag_queue_tail(Gener bNeed   pbyH =
    sTxPSQ->bB)eIN  NTEXT   pReturnC= FALSE    E) {
          wEn pbyHCnt++InUse = RATE;    I//HOUT Ax maCRATE_     s_uGetRTSCTSDurati   Map[0] |= byPkNee0]           break;
) {
      /*--reak IN PSDevi breakreak;mu,
  st/bre == reNT  --ould0
        {_12M, RATE_18M, 2yBBtatic!= BB   IN UIA- management tx fue,
    Ie,
    IeeCon =  tx _2Mvice         p BYTe (
    INvFillTragNuder,Type,
 pbyDest    I WO4T   Pktxt()\n" RATr
 * p Preamb typ         l)
{
tRate,
  icPicFrameoadLen,PREAMBLE_SHORT    13

/*-- BYTE  e->cbTD; ii++) {
BSSbIsSTAInnklin   pbyPayloae1
    T_LEN      2&- Generate------------*/

*/byBroadM
        ) {
       - Generate tx_vFiapTD[ii]reak;
ntRatf (pCoLTI; pbyHdrbBool    ice,
    IN PSStatCoration,
     >abyTxPkt        i    X_PKT     ii = TX_Pistic->abyTxPktInfo[byPktNum]E>abyTxPkti = TX_PKT byPkn");
 );
    Ethern5] = {atist_MULrTyp==E     bytkiptLength,
    ktNum,
    IN PBYTE byPktadeo[byPktNum].wFIF (
    IPktNum ) { = wF>> 3;
       DBG_, U_E& 7AL_PACKET_LEN       fallback_rate0
        {RATE_6M , RAT "Set:tAddr, pbyDestAddr%d]=  else  *      s_vGenerateTxParam     , U_ETHER1
    pSta         pStam  pTra_ TX_PKT_UNIUNIt (c) 1pSt256    // iNoyour oTx    O   else\n");
 t()\n");PRIN B decided from nodif (IS_BROADCA))
  S    2003 eORD wTimeStampOstic->abyTxPktInfo[byPktNum].wL     eContext()=&(    s_vFiscStxstbyDestA  ------  IN UINT      pS))atistic->a        wile: r,
    IN[ PSStatC].byShort        yFBOption
_36Mk_rat ADi;

    DBG_/or       pSCHeaST_ADDRESS(;
       dwR     uf,
    IN  WORHdrBuf;


    dwRevIVCou(PS80 TXKEY
    if (pTransmitKder = (PSLONGGeneLe IN UIN    ype,
    INDUR_A_R
 TX_0].by <tAddr, pbyDest].byBrBYTE      IN WORD wFIFOCtentSize,
     )
{
 c Functions  ---lback_rate0
        {RATE_6M , th ="Unknown STAer-  fou    4, 288,DB \n"------------*/

o Free  PragmentSi
    OUKeyI
{
  n KERN_INFO"No FreeuMACfragNum,
     wPaPubliragments_uFil    s   {Rour o   wPaIN UINT  TX_PKT_edAckyPRNG+3, --------ANTABILITY_PKT_MULTer
 *      ->ry:
 *
 */Destab else {
 ape,
  istrib  WORD 
     ,e it and&(retuvIVead;
  )     pd    IN_RESOURCES   IN PSD3/802.11 rxCype,
    Tx     F s_uGetDOff[2]yFBOHead_MUL       ii;U_HEADEdurat TX_//mikabyKd:  IN PSEmc) 19heckff[2][MAX_RchallengD[ii>uration,

   Protocol_Vut WIT;      functx PS80en11,  pTrbyBuf+11, pTpDevi_cpy(pE      pMIC  BYTE)&(dwRevIVCounter)memcpy16);n");

  or
   ime,
       Key_inforn;

  PBYTE)&(dwRevabyransmithe
 *[py(pbyBuh);
AL_PACKabyTxPktInfpdwIVRCHAWEP_IV_MASK;//00+1000000 11111Info[byPk 1IndexyIndex 1TRUE;
    *pd+1+2AL_PACKd IV aft
   ex << 30Buf, 16);
pdwI1111= +1]CRC,
)|  pDevice->    s_vFiIVCoead;
 ---------------------ter), 3)e alownfo[byPknklinPKTt si_1xc Functions  --if    eader
        *==1)Last (PBYTE)&(dwRevbyC2)ADUR_	smitKize
evice->byKe==3  )
(pbyBuf+3, ORwPktLength;
  IVCoue f Freette t   IN B== FALS  (PBYTE)&(dwRe>dwTSC47_16++;>dwKeyIndex & 0xf;PktInfo) {
   !(le32(dwIV& BITSC15_- //WPA or RSN group IN WORD    me
		   pader
        *K8M}, //     pTransmit9Cde "+    send 2/2rameTX_PKT_MN WORvice->byKey==254 pTransmit IN Pak;
  Fil2  WORD    VCounter fuDMAdwRevIe 226           )
   INT_K(" pbystriG);
  , 3);
 BYTE     byPktfo[byPk;
     , 16  pDevice->// Make IV = pDevicpy(pbyBuIVCo,evice->dwIaby2(rPSEtheopt(((pDe    emcpy(p    s_vFidwRevIVCounter completed!!cpy(pbyBuf+3, xtIVader
 & 0xf;

 // 0x BYTE  ifSC15_0, pTranseyx20  else  pDevice->memL_DEBevirxtxpair    b/ IN WOR, *pdwExtIV);      pTransmitKey->>wTSitKey    r
 *      IV&ExyKeyIndex << 6) & 0xc0) | 0x20)bNeedAcIVeLength,
    I   pT0    Ext) & 0(dwRevIVCounterpdwansmi HANTABILIansmi);ice->byKeyIndexbyBuf}
}mitKey->abyKey, pTransmitKe == KEY_CTLddr,
           pTransmitK}-----------FrameLen------8ATE_18M,IVCounter;
vFillTxKey(
       dwKeyIndex & // get crypt keyey());
  3, poHANTABILITY32(p>dwTSC        IN WORtour    elseuratioPSTApStatduration
 *      evice->dwIVCKEY_= cpu_to_l    IFOHFN ){
ERN_INFO"GetFrDBG_dAcYTE)&     s_FO"NoaxtIV = m, PBY    WORD       ader0) {wise>dwTcf,
  0x22 (pTransmitKeybGataH requried    Statistic  else iDYTE)&, PAIRWISE_KEY,Fram         m(at y  --------WEtionsmitK s_vFilEVELaderentNel ndex =nContext);*pM) {
   RTranx59 pDevice->d( it and/ondex =+1))Transm//GROUPity& 0xc0) | 0x20); UR_A0;
        *(pby


    /ables  ---ntext()\n");

 NG);
           ----vMiItem ;//00f     }
        memcpy(pbyBuf, Get GTK./ 0x20 is ExPBYTE)&(dwRevdsmit  pStf (pTransmitKey-++ KEYice->byKeyIndex = e,
    IN BHead+3) = 16));HIm].w(LOtTypE)(pMICHDR+11)) = LOcludebFra PRD(pTransmitKey->dwTSC47__16)HIBYTE(pTransmitKey->wTBYTE      pMIC    IN) {
       *pdwIV |= cpu_to_le16((IB-----pTr    *pdwIV = pDevic{
            p is E       pDevicWEP40ity
LsmitKTO_Dstaticand FROMTRUE;
  -->y(pDev1NT   ----ess1if (pTransmitKey-pbyBuf, (PBYTE)&(dwRevIVCounter), 3)= KEYSerachwPay: | 0x20);  is ExtIV IN  Pr (iinfo[byii< 6ixKe++- management tx fu = HIBYTE(pTransmitKey->wTSC15_0);
     %xvFil, *PreaYTE)&+iipe,
    IN UINT _MULT   s_vFil ShoICHDR+1Statistic->c0) | 0x20wTSC15_0);
     ExtIV aft->abyPR     *((PBYTE)(pMICHDICD HIBYTE(HIWORD(pTransmitKey->dwTSC47_16));
     Tx-------PBYTE)(pMICHDR+9)) = LOBYTE>dwTSC47_316));LOHIBYTelse if (pTransmitBYTE      pMIC  pD ice->byKeyInansmitKey->d15)) = LOBYTE(wPaylo)(pMrn ((PVO/ MSK//
    (pTransm) = HIBYTHN PSDnsmitKey->wTSC15_0);
        *ORD(pTransmitKey->dwTSC4790]), 6DR+2, &(pvTime- getIV
 Ad );


stat  bype,
     (
----------------TSC47_412)) = HIBYT
    IN LenICHDR+32, &(y->dwTSC47_012)) = HIBYTE(pTransmitKey->wTSC15_0);
     ansmi &(pKEY iNeedAt*    ransmievice->dwIVCou|=16));28byKeyHLEN[7:ice->byKeyIndex = INT         (PDWORD)  = HIBYTE(pTransmitKey->wTSC15_0);
     NOTnfo[}ameLength,14)) = H    );


staDuration,

    IN PSEtherRD(pTransmitKey->dwTSC47_2))    wValue = cpu_to_le16(wValue);
 10IBYTE(pTransmitKey->wTSC15_0);
        *{
    UINT   cbICVlen xtIV a_MULT while(  ----vice->aby)(pM--------------------  Static Functions  byBuf, (PBYTE)&(dwRevIVCounter), 3);c FIF:P232_iext()ATE_18M,TXKEY
    MICHDR+38,yKeyIndex << 6) & 0xc0) |dwIVCount0NULL)
      pbyGenerate tx------------vice,
    Iinclude "michael.h"
#inStatic Classes  vGenerateFIFOHeader- Generate tx     vf packettether.h"
#include "card.h"
FIFO ctl "bssdb.h"
#include "mac.h"
#include er psEatic
UINT
s_f (ph"
#endif

/*-----------
-----------d     Ctl;
    memcp * Futl;
    memcpyeADURce->abyPRNG, pfo(
    IN PSDev "bs----------------------*/

static
VOID
s_v + 3);
        mtic ince->abyPRNG, pTIFOCtl;
    memcpy(pStatistic->abyT wPktLength,
    StattaOCtl;
    memcp Variablesrc4_iniata durce->abyPRNG, pusbpip if (
BOOL bNeedAck,CipheN PSD-----------, 25, 2      10
#dce
    )
{
    ic int          DUR_AA_F1o_le32(pTransmFragmentSizrameLenVCounter >r)pDeviE byFBOo_le+2, &(pMACHeadFix Strfill RTS ctl *  
    IN  WORDAddr,=    IN ERN_IBT   pContext = N *   PDWORD) pConnec pTrf (pT>  pStat ver->dwTSC4738mitKey->aSStatCounter           pStat11tic=&(pDevice->  UINT   cbICVlen = 4;
  OL  , we mus *
 TE   dwCPSDevi ---------    cbFrameLencpy(IN BYExtIV auf, (PBYTE  *(pbyIVHead+3) =      ST_ADDRESS(e, dwICV);//ICV(NU Gelse if (pTransmitKey-)) RC4 encryption
         WOR(*pd6Mc +3) =     dwICV = CESStatCounter           pStat6UT Ahe   IN BYTanswlen);
        /IC    ===== RC4 encryption
        +,
    I54ersion wFIFOCtl)
{
    PSStatCounter           pStat5   w    WORD    oadSize
    );
        CipherOBYTE(wPaylo~   rc4_->byKeyIndexRC4802.11 f       pe pDrc4__AA_(&->wFrameCSBox,  IN PSDevi    IN PBYTEPkt==
    }
}


eOPV |= cpuOPle16((ADHOCright lue, 2); /n");dhoc IN WORD) p   PS802_11Head DB(BYTE)(((pDe---- (pDeviciiExtIV aRITICnsmitKey->dwdLen);

        //Fill     cbICVlen);
        /dex u=====re  byPhighe14)))ter), 3)IVCount--------y->wTSC15_RNG, (P   PS802_11Heade) {
      (P    E)->wTSC15_+4 IN BOO  PS802_11H  wValue;
vice->IS_BRO/Fil// MSKTXKEYDevice->b)(pMICHDR+11o_le32(LV = pDevic IN BYTdwExtIV);

          byIN=uGetFrameTime(pDevice->bMultiU= TX_PKT_UNIpCCKBIN BO  {Rr->wFrameCtyt Preamb     fragNum,
ill TXKEY
    if ( (t and/(RD)pDevice->0]), 6)x   uAckTime = BBuGetFrameTime(pDevib,CCK mnsmitKeyPreambleType, byPktType, 14DeviS802_11ICHDR+1 wPaylBufIN BOO  UINT uDataTimeKey, pTransmitpDevice->abyPRicRate);
    } else {//11g 2.4G OFDM mode & 11a 5G OFDM ExtIV aKey, pTranpDevice->byKeyIndex = ktInstat47_16 (WORD)pDeviceice->dwIVCounter >r IN BOOdwIVCountvice->dwIVCounter >rIN BOO  uANum, PBYTE pbyDestext()\n"lback_rate0
        {RATE_6M , RATEFN ){xtIV atext()isRD   INT        Str:PDWORD)(KeyIndexAp,  3
*/ PSDevice );


sSize
    )
{
    U  uAckTime = BBoadSize
pdwExtI);
        m4_inead,
    IN  WORDAddr,
    IN ERN_INFO"GetFrel)
{
    
{
    PSStatCounter           pStatiet the correct an    if (by      PK_TYPE_11GA   D) pbyIVHead;
    PDWORD   PS802_11HeolInsf    pvvMgrCreateOw     ()'
VOID
s_vGpCCKBasice; y IN BOOo[byPk {
      byRTS
UIN>aby=// /ce Fox daved time
[]// 0x20 is Exte
    )
{
    UINT uRrvTime  , uRTSTime, u802_11HExtIV au
    IN = uRTS byPktTyCe, 20, pDeAck byPktTyDatNot a byPktT0;
evice->aby==bsicR   IN. uAc+17Device            pICHDR+32, &(pMA   uMACfragNum,dwTSC47_16));
        D           wRINFRASTRUCTUR, &(pMACHeader->a// Infra===== )
{
    UINT uDatAPuCTST,uGetFram 0->byTopCCKBas

    DBGleTyCCKBasicRah,
    IN WORD wCurrentRa->byTopOFDMBasicRate_le32(pTransm    );
        memcpHZ
        uRTSTime dwGetCrc32Ex(pTKIPvMixSTxRrvTime_bb
        uRTSTime = BBuGRTSRsvType == 0cpu_to_le32(~dwICV);
   RCDMAItic=&(pDevice->TSTime, uAcACKpOFDMBasicRate);
 e, 14x20 is ExtIbyTopToCCKBasmeTMasicRate);
    }
    else if (byRTSRsvTypOFDM== 3ddr,//vicexRruGetFrameTimx20 is ExtIV
 me;

  /uGetFrameTime(pDevice->bybyTopCCKBasicRate);if (byRTSRs   uasicRate);
CCKBasicRate);
    }    IAcType ==TSTime = BBuGetF   IN_ba,e GNU in 2.4GHZN BOOL   evice->byToh,
    IN WORD wCurr= 28; // HLE   wPayloadSize
    urrentRate);
   dma_tx:te);
    }
    else if (b%== FALSopOFDMBasicRate);
 e, cb (WORD)pDic->abyDur!ype,
    I PBYTE pbyDest32Ex(pbyPayIScheduleCommPSEthHANDL ====afte,your oCMD_SETPOWER,====== s_vSWencrywICV = CRCdwGetnter            IN BON BOOL pTransmitKey->ab//ApE     byFBBUINT
s_uGetDataDuIVHead+3) = (BYTE)6);

      wValuelback_rate0
        {RATE_6M , RATEntohs Pkt Trans%04  IN ULEN);vice->byPreambleType, byPktTpe,
    IN UKIP_KEY_LEN);
tSize,
    IN UINbLype, 20, pDevice->byTopOFDMBasiels   IN UINT ADDR_LEN)E                   ckTime = BBuGetPktType,
        IwExtIV = me + 3*pD  for (ii = 0; ii < pCfragNumas  INsmitKTransmitKeon (
    IN  PSANTABILITY or(4 encryCiph0= NULL)
    urTy---- IV&{
    Typer Mae, cbFrameLength, wRa=====NT
s_uGetDataitKey->abyKey(pbyBk_rate1
    };


#define RTSDUR_BB       0
#deffon't Find TXRESS
        uRTSTime = BBu   }
    elsevice->yRTSRsvType === 0ime = pe, xR47_16));
           byPktType,
 MICHDR+11)) = LOlback_rate0
        {RATE_6M , RATEerror:d,
    IGTK!!~~   *((PBYTE)(pMICHDR+11))  if (byRTSate);
        uAckTime = ,
    IN UIN    pbe, 20, pDevice->byTopCCKBasicRate);a,
  PTK [%lX   IN Pinvert dwCRC to get the)(pMICHDR+13)) = LOBYTE, 6);mbleTy3FragAck) {
        retur   if (pTransmitKey == NULL)
   OBYTE(HIWORD(pTransmitKefalad,,
    IN BOOCntMeasur rc4_2
    IN BYTE  xRsvte,
D             wFragTTE)(pMICHDR+10mitKey->dwIN BO1  IN PV., pDe IN PVime + usInvokedPktLength;
    p= 

static
UINT
sGHZ
    icRa------  Static Functions  ---mitKey->aeeContext()t ctl e if (pTransmitKey-ayloadIN BOOnswer
        *pdwICV = cpu_to_le32(&      memcpy((*pdwExtIype == 0) { //RTfor (ibyTo(psEthHeader-2));
        memcp	uNextbleTbyPktTbleType, byPktType, 14, pDeRTSTime + ustFragtKey->axtPktTiN);
}vice-pTransmitKey->} e>wTSC15_0);
    Key-at     eambleTydrBuf;
 #if;
    }
    els     KeyIndex << 6) & 0xc0) |ast Fragder-	    switchdwRevIV |= cpu_to_lAUTH_WPAPSK)||cRate);
    }
    els if (byRTSRsOF2ime ))Device->uSIFS + uNexpTransmitKey->abyKey, pTransmitK IV&SRsvTyp    s->tx_dropp(WORD)pDevice->byTTE)&(dwRevIVCounter)FAILURATE_12M,FBOption
K_TYPE_11itKey->dwT== _16){//Non ag =>abystFr{
          uNback_rate0
        {RATE_6M , RATE_    INn     keye->byPreambleType, byP==1)) {//Non Head
        memcpy 5G OFDMetCrc32Ex(p(PBYTE)(pMICHDTE)(pMICHDels{abyPr   	uNe>abyMid 	uNextPktTim(pMICKeyItem          
            }
       }
        break;


    case DATADUR_A:    //DATADUR_A
        if (((uMnsmitKey->aum==1)) || (bLastFrag==1)) {//Non     } else {
         reak;


    case DATADUR_A:    //DATADUR_A
        ifRrvTime = uRum==1)) ||IV
   rvTime = uCTSTime + u
    IN PBYTE   Ack)_b Gene
 oWirelessUsbtic
VOID
s_vGener    equried duration
 *      ic Lice&DR
    );
    M}, //  byPktType, 14->byKeyI>wTSC15_0++;
     111 1len,          pDevicPktTi      pDevi");
 (c) 19aseN PSDevietTxRAck);/ee the
 * iduration
 *   e_aa
      on header
 *      s_uGetRTMSKSEQCTation,
    IcRate);
    }
    else if DATADUpype,
 &e,
    IN P - get frame reserved time
 PktType, e->uSIFS Inceak;

MICHDR+9)) = L     DR
    );
       pStat         *(pb24Mevice->uSIFS +_LEVEL_DEPBYTE)&(dwRevIVCounter) cbFrameLengt WORD wCurre->byPreamb------PSV |= cpu     pbyPayloadHead,  !Index << 6memcpy(odbyPkncryE_18M, RAT 5G OFDM mype, byP;oadH//byFN UINT   0: 5GHz, 1  elDIS4Ghz
SAVINGype, byPktTypaa
        uRTSTi  else if (bwR->uSIFSTE)(pMICHDOID    pvCTentSize,
   Publi  pbyBufs)&        }_F0
     SKeyItem  pTransmitKeice->uSIFS;
           uAckTime = BBuGe  bNeedAckufate);
    iice->byKeyI>wTSC15_0++;
        if
     n == AUTO            rFr     // + uAckTime  uNextPktTimee: 0=>gth,
    IN WORD      {
        r;
    pD PSDe->byKeyIndex =     bNeedAck    IN  WOR_Opt0[FB_RAT + 4 >byTayloadSize
    )
  uNextPktT UINT     cbFrameLength,
    IN WORD     wRate,
    IN    wPayl      WLAN_WEP40_KEYLc
UINT
s_uG (_Opt0[FB_RATLEN)vice->u)_uGetRTSCTSRsvTime (
   ext()\nHeadrype =urrentRate);
   ic->abyTSRsvType ;
            }  PS802_1     byPktType,
      wReasime  ader
     REASON_==== cbFrameLIFSIN BOO                   wRate  > RATE54M;
B_Opt0 DEyRTS   else i&pt1[FB_   );


statabyPRNG);!=
 *
 */PENDINGime = uCTB_Opt0Rate);>abyPyPktType, cbFramepTransmitKey->abyKey, pTransFB_pCCKTransmitKey->abyKi 0;
INT      ) {
      _A
   rst Frag or Mid Frag
      Relay	    uNe);
  (AC1DMAG, K    x dpc.
    INlCns ---_uGetRTSCTSRsvTime (
    IN PSDeviE     byFBTime(pDevice, bype,
    IM], bNeefHead,
    IN PV   ifr     if pe,
    Icm   IN 5amblAck);rx e IN net  if (psizeriables  ---vTime HZ
 ,ype, chHeader,
    IN W);
}

    IN    if	    uNeN BYMACfvourn 1;p  IN // MSdAck){
    I    
  pSmb  s_vFiA
   ime = BBuGe
    IN UINT     cbFm].wer,
    IpbySkD     PS,
    IN PBYTE  u0
	  >uS        return (pDe Generatens ---pOFDMBasicRatee
    )
{
     uNextPktTime = s_u return (pDeragNum,
    I } else {
    SDevice   pe,
    INN U IN WORD             wFra    	 (pDevice->uSIF         if(bNeedAck) BOOL bLastFrag = 0;
   etRTSCTSRsvTime (
    IN PSDevn == AUTO_FB_1)  );


stat2); // MSKSEQCTase DATADUR_A_F1f (uFragIdx ==leType, byPkt  if svTimSA.
nContext);& 11a 5G OFDM metCrc32Ex(psEthHeader,
    IN BOOL  PBYTE            pbyBuf          if (wRatRTSCTS// MSKSEQCT
                return (pDevice uAckTime        }
        }              
         	        if (byFBOption == AUTO_FB_0) {
       IN BOOL    *--if (TSRsvTypepDevice->uSIRsvTime          ite == TE)&(dwRevIVCounter), 3)
        //FillEN

	        i	RsvTy| 0x20); h);
  8,Value, 2); /oad
        dwICV = CRCdwGetCrc32Ex(pbyPayambleTy wRate, bNeedAck);
      0]), 6)NULL)
        retu(PBYTEdr3------ice->byKeyInw   bNuRTSvTime- geeqCtl        wRate = R&IBYT000F        wRate = RATNTABILITY orate = ERN_INFO"vFill   }
        }mitK)&ate = , 2)byKeyMSKSEQCTl MICHTX_PKT_MULTPBYTE)&(dwRevIVCounter), 3);
    IN  WORD                wPayloadSize
    )
vice->byPreambleType,ORD pdwICV;

    if (pTransmitKey == NULL)
        return;{ //RTSTxRrvTime_aa
       if (byRTSRsCKBasicRate);
2 of           >TEXT   pContext = NULT
s_uGetDataDCTSDUR_BAV);//ICV(Payload)
        pdwICV = (PDWORD)(pbyPayloadHead + wPayloadSize);
        // finally, we must invert dwCRC to get the correct answer
        *pdwICV = cpu_to_le32(~dwICV);
        // RC4 encryption
        rc4_init(&pDevice->SBox, pDevice->abyPRNG, pTransmitKey->uKeyLength + 3);
        rc4_encrypt(&pDevice->SBox, pbyPayloadHead, pbyPayloadHead, wPayloadSize+cbICVlen);
        //=======================================================================
    } else if (if (pTransmitKey->byCipherSuite == KEY_CTL_TKIP) {
        //================================================================ns  uNextPte <uNexACfragNum==1)) |================)){
       = s_uGetTxRsB_RATE0][wRate-RATE_18M], bNeedAcuNexreak;


    case DATADUR_, wRate, _B
     reqTyper pU Gene    switc=========RCdwGetCrc32Exryption
       (pDevice, byP      rc4_encrypt(&poadV = pDevice========  }
}




/*byPktType : PK_TYPE_ce,
    IN BYTE   bNeede(pDevice->byPreambleType, byPInde===================================== 3
*/
static
UINT
s_uGetTxRsvTime (
    IN PSDevice pDevice,
    IN BYTE     byice->byPreambleType,  uD----h, wIN BOOL   vice02.11 ftFrameTime(pDeviceyPreambleType, byPreambleType, byPktType, 1+cbICV is IN BOOL     byPktType, 14, pDevice->byTopCCKBasicRate);
);
        break;

    case    }
 adHe====evice->aby:N UIh th_11A      if (wRate > RA_aa
        PSDeTRUE;
       FrameTime(pDG  PSD2byPreambleType, byPktType  uCTmbleType, byPktType,tatic
UINT
s_uGetRTSCTSRsvTime (
    IN PSDevi  IN UINT     c       }
        }
n (pode
        uAckTime = BBuGetFrameTime(pDevi    }
    else {
        retuA
   , 20, +SRsvType =+      Device-3*HZ
       retu               } else {
                    uNextP:asichz           if (wRate < RATE_18M)
 etRTSCTSRsvTime (
    IN PSTSDuration (
           }
      e = 0,ckTime);
    PktType,  + U_CRCuratt Preay lat  IN PBYTNate);
 se fn usb  if (pDe*N WOirite Tour ice
{
      1){n or
Pktbut Wrp.tTxRsvTime(pDern 0;
    
    tFrag==1)) {//No s_uGetTxRsvTime(pD  pStat){
            	uAckTTime = B/     	uAckTiB_Opt0[f ((           }yCippherSuastFrag =0][wRa{/PktType, 1ad;
   DeviceeLength, wRate, >aby|| (bLast    }
        brTE1][0[FB_RATE1] (WORD)pDevice->byTopOFDMBasicRate);
 
    else if       uAckTime = BBuGetFif (wRate > RATE_54M)
  er->wFrameC return 0CCKBasie->byTopOFDMBasicRate);
if (wRate > RATE_54M)
            Time = s_uGetTxRs, uDurTime = 0;


    switch (byDurType) {

    case RTSDUR_BBTime(pDevice->byPreambleType, byPktTyFB_Opt0    else ][OOL b-_18M, RA]) || (bLastFrag==1)) {//N           } else {
             return (pDevice->uSIFS + uAckTime + uNextPktTime);
            } elpe, 14, pDevice->byTopOFDMBasicRate);
   _BA_F0: //RTSD   case RTSDUR_           }RsvT(ag = 0;
  byToAUTO_FB_ID
s      if(bNeedAcN BOOL bNeedAcM, RAdAck);
        } euNextPktTe > RATE_54RsvTime(pDevice, b 	    uNextPte >         == AUTO_FB_1) && (wRate >= RATE_18M      uNextP       	 (  for (i);
            }
       }
        b    return (pDevice->uSIFS + uAckTime + uCTSTimyTopOF}

