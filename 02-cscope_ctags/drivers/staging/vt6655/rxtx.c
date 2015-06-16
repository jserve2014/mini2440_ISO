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
 *      vGenerateMACHeader - Translate 802.3 to 802.11 header
 *      cbGetFragCount - Caculate fragement number count
 *      csBeacon_xmit - beacon tx function
 *      csMgmt_xmit - management tx function
 *      s_cbFillTxBufHead - fulfill tx dma buffer header
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
#include "wroute.h"
#include "hostap.h"
#include "rf.h"

/*---------------------  Static Definitions -------------------------*/

/*---------------------  Static Classes  ----------------------------*/

/*---------------------  Static Variables  --------------------------*/
//static int          msglevel                =MSG_LEVEL_DEBUG;
static int          msglevel                =MSG_LEVEL_INFO;

#define	PLICE_DEBUG


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
VOID
s_vGenerateTxParameter(
    IN PSDevice         pDevice,
    IN  BYTE            byPktType,
    IN PVOID            pTxBufHead,
    IN PVOID            pvRrvTime,
    IN PVOID            pvRTS,
    IN PVOID            pvCTS,
    IN UINT             cbFrameSize,
    IN BOOL             bNeedACK,
    IN UINT             uDMAIdx,
    IN PSEthernetHeader psEthHeader,
    IN WORD             wCurrentRate
    );



static void s_vFillFragParameter(
    IN PSDevice pDevice,
    IN PBYTE    pbyBuffer,
    IN UINT     uTxType,
    IN PVOID    pvtdCurr,
    IN WORD     wFragType,
    IN UINT     cbReqCount
    );


static
UINT
s_cbFillTxBufHead (
    IN  PSDevice         pDevice,
    IN  BYTE             byPktType,
    IN  PBYTE            pbyTxBufferAddr,
    IN  UINT             cbFrameBodySize,
    IN  UINT             uDMAIdx,
    IN  PSTxDesc         pHeadTD,
    IN  PSEthernetHeader psEthHeader,
    IN  PBYTE            pPacket,
    IN  BOOL             bNeedEncrypt,
    IN  PSKeyItem        pTransmitKey,
    IN  UINT             uNodeIndex,
    OUT PUINT            puMACfragNum
    );


static
UINT
s_uFillDataHead (
    IN PSDevice pDevice,
    IN BYTE     byPktType,
    IN PVOID    pTxDataHead,
    IN UINT     cbFrameLength,
    IN UINT     uDMAIdx,
    IN BOOL     bNeedAck,
    IN UINT     uFragIdx,
    IN UINT     cbLastFragmentSize,
    IN UINT     uMACfragNum,
    IN BYTE     byFBOption,
    IN WORD     wCurrentRate
    );


/*---------------------  Export Variables  --------------------------*/



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
    BYTE            byKeyIndex = 0;



    //Fill TXKEY
    if (pTransmitKey == NULL)
        return;

    dwRevIVCounter = cpu_to_le32(pDevice->dwIVCounter);
    *pdwIV = pDevice->dwIVCounter;
    byKeyIndex = pTransmitKey->dwKeyIndex & 0xf;

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
        *pdwIV |= (byKeyIndex << 30);
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

        *(pbyIVHead+3) = (BYTE)(((byKeyIndex << 6) & 0xc0) | 0x20); // 0x20 is ExtIV
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
        *(pbyIVHead+3) = (BYTE)(((byKeyIndex << 6) & 0xc0) | 0x20); // 0x20 is ExtIV
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
#ifdef	PLICE_DEBUG
	//printk("s_uGetTxRsvTime is %d\n",uDataTime);
#endif
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
    IN PVOID    pTxDataHead,
    IN UINT     cbFrameLength,
    IN UINT     uDMAIdx,
    IN BOOL     bNeedAck,
    IN UINT     uFragIdx,
    IN UINT     cbLastFragmentSize,
    IN UINT     uMACfragNum,
    IN BYTE     byFBOption,
    IN WORD     wCurrentRate
    )
{
    WORD  wLen = 0x0000;

    if (pTxDataHead == NULL) {
        return 0;
    }

    if (byPktType == PK_TYPE_11GB || byPktType == PK_TYPE_11GA) {
        if (byFBOption == AUTO_FB_NONE) {
            PSTxDataHead_g pBuf = (PSTxDataHead_g)pTxDataHead;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, cbFrameLength, wCurrentRate, byPktType,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField_a), (PBYTE)&(pBuf->bySignalField_a)
            );
            pBuf->wTransmitLength_a = cpu_to_le16(wLen);
            BBvCaculateParameter(pDevice, cbFrameLength, pDevice->byTopCCKBasicRate, PK_TYPE_11B,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField_b), (PBYTE)&(pBuf->bySignalField_b)
            );
            pBuf->wTransmitLength_b = cpu_to_le16(wLen);
            //Get Duration and TimeStamp
            pBuf->wDuration_a = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_A, cbFrameLength,
                                                         byPktType, wCurrentRate, bNeedAck, uFragIdx,
                                                         cbLastFragmentSize, uMACfragNum,
                                                         byFBOption)); //1: 2.4GHz
            pBuf->wDuration_b = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_B, cbFrameLength,
                                                         PK_TYPE_11B, pDevice->byTopCCKBasicRate,
                                                         bNeedAck, uFragIdx, cbLastFragmentSize,
                                                         uMACfragNum, byFBOption)); //1: 2.4

            pBuf->wTimeStampOff_a = cpu_to_le16(wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE]);
            pBuf->wTimeStampOff_b = cpu_to_le16(wTimeStampOff[pDevice->byPreambleType%2][pDevice->byTopCCKBasicRate%MAX_RATE]);

            return (pBuf->wDuration_a);
         } else {
            // Auto Fallback
            PSTxDataHead_g_FB pBuf = (PSTxDataHead_g_FB)pTxDataHead;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, cbFrameLength, wCurrentRate, byPktType,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField_a), (PBYTE)&(pBuf->bySignalField_a)
            );
            pBuf->wTransmitLength_a = cpu_to_le16(wLen);
            BBvCaculateParameter(pDevice, cbFrameLength, pDevice->byTopCCKBasicRate, PK_TYPE_11B,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField_b), (PBYTE)&(pBuf->bySignalField_b)
            );
            pBuf->wTransmitLength_b = cpu_to_le16(wLen);
            //Get Duration and TimeStamp
            pBuf->wDuration_a = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_A, cbFrameLength, byPktType,
                                         wCurrentRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption)); //1: 2.4GHz
            pBuf->wDuration_b = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_B, cbFrameLength, PK_TYPE_11B,
                                         pDevice->byTopCCKBasicRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption)); //1: 2.4GHz
            pBuf->wDuration_a_f0 = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_A_F0, cbFrameLength, byPktType,
                                         wCurrentRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption)); //1: 2.4GHz
            pBuf->wDuration_a_f1 = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_A_F1, cbFrameLength, byPktType,
                                         wCurrentRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption)); //1: 2.4GHz

            pBuf->wTimeStampOff_a = cpu_to_le16(wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE]);
            pBuf->wTimeStampOff_b = cpu_to_le16(wTimeStampOff[pDevice->byPreambleType%2][pDevice->byTopCCKBasicRate%MAX_RATE]);

            return (pBuf->wDuration_a);
        } //if (byFBOption == AUTO_FB_NONE)
    }
    else if (byPktType == PK_TYPE_11A) {
        if ((byFBOption != AUTO_FB_NONE)) {
            // Auto Fallback
            PSTxDataHead_a_FB pBuf = (PSTxDataHead_a_FB)pTxDataHead;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, cbFrameLength, wCurrentRate, byPktType,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField), (PBYTE)&(pBuf->bySignalField)
            );
            pBuf->wTransmitLength = cpu_to_le16(wLen);
            //Get Duration and TimeStampOff

            pBuf->wDuration = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_A, cbFrameLength, byPktType,
                                        wCurrentRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption)); //0: 5GHz
            pBuf->wDuration_f0 = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_A_F0, cbFrameLength, byPktType,
                                        wCurrentRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption)); //0: 5GHz
            pBuf->wDuration_f1 = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_A_F1, cbFrameLength, byPktType,
                                        wCurrentRate, bNeedAck, uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption)); //0: 5GHz
            pBuf->wTimeStampOff = cpu_to_le16(wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE]);
            return (pBuf->wDuration);
        } else {
            PSTxDataHead_ab pBuf = (PSTxDataHead_ab)pTxDataHead;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, cbFrameLength, wCurrentRate, byPktType,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField), (PBYTE)&(pBuf->bySignalField)
            );
            pBuf->wTransmitLength = cpu_to_le16(wLen);
            //Get Duration and TimeStampOff

            pBuf->wDuration = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_A, cbFrameLength, byPktType,
                                                       wCurrentRate, bNeedAck, uFragIdx,
                                                       cbLastFragmentSize, uMACfragNum,
                                                       byFBOption));

            pBuf->wTimeStampOff = cpu_to_le16(wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE]);
            return (pBuf->wDuration);
        }
    }
    else {
            PSTxDataHead_ab pBuf = (PSTxDataHead_ab)pTxDataHead;
            //Get SignalField,ServiceField,Length
            BBvCaculateParameter(pDevice, cbFrameLength, wCurrentRate, byPktType,
                (PWORD)&(wLen), (PBYTE)&(pBuf->byServiceField), (PBYTE)&(pBuf->bySignalField)
            );
            pBuf->wTransmitLength = cpu_to_le16(wLen);
            //Get Duration and TimeStampOff
            pBuf->wDuration = cpu_to_le16((WORD)s_uGetDataDuration(pDevice, DATADUR_B, cbFrameLength, byPktType,
                                                       wCurrentRate, bNeedAck, uFragIdx,
                                                       cbLastFragmentSize, uMACfragNum,
                                                       byFBOption));
            pBuf->wTimeStampOff = cpu_to_le16(wTimeStampOff[pDevice->byPreambleType%2][wCurrentRate%MAX_RATE]);
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
 *      uDescIdx        - Desc Index
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
    IN PVOID            pTxBufHead,
    IN PVOID            pvRrvTime,
    IN PVOID            pvRTS,
    IN PVOID            pvCTS,
    IN UINT             cbFrameSize,
    IN BOOL             bNeedACK,
    IN UINT             uDMAIdx,
    IN PSEthernetHeader psEthHeader,
    IN WORD             wCurrentRate
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
static
VOID
s_vFillFragParameter(
    IN PSDevice pDevice,
    IN PBYTE    pbyBuffer,
    IN UINT     uTxType,
    IN PVOID    pvtdCurr,
    IN WORD     wFragType,
    IN UINT     cbReqCount
    )
{
    PSTxBufHead pTxBufHead = (PSTxBufHead) pbyBuffer;
    //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"s_vFillFragParameter...\n");

    if (uTxType == TYPE_SYNCDMA) {
        //PSTxSyncDesc ptdCurr = (PSTxSyncDesc)s_pvGetTxDescHead(pDevice, uTxType, uCurIdx);
        PSTxSyncDesc ptdCurr = (PSTxSyncDesc)pvtdCurr;

         //Set FIFOCtl & TimeStamp in TxSyncDesc
        ptdCurr->m_wFIFOCtl = pTxBufHead->wFIFOCtl;
        ptdCurr->m_wTimeStamp = pTxBufHead->wTimeStamp;
        //Set TSR1 & ReqCount in TxDescHead
        ptdCurr->m_td1TD1.wReqCount = cpu_to_le16((WORD)(cbReqCount));
        if (wFragType == FRAGCTL_ENDFRAG) { //Last Fragmentation
            ptdCurr->m_td1TD1.byTCR |= (TCR_STP | TCR_EDP | EDMSDU);
        }
        else {
            ptdCurr->m_td1TD1.byTCR |= (TCR_STP | TCR_EDP);
        }
    }
    else {
        //PSTxDesc ptdCurr = (PSTxDesc)s_pvGetTxDescHead(pDevice, uTxType, uCurIdx);
        PSTxDesc ptdCurr = (PSTxDesc)pvtdCurr;
        //Set TSR1 & ReqCount in TxDescHead
        ptdCurr->m_td1TD1.wReqCount = cpu_to_le16((WORD)(cbReqCount));
        if (wFragType == FRAGCTL_ENDFRAG) { //Last Fragmentation
            ptdCurr->m_td1TD1.byTCR |= (TCR_STP | TCR_EDP | EDMSDU);
        }
        else {
            ptdCurr->m_td1TD1.byTCR |= (TCR_STP | TCR_EDP);
        }
    }

    pTxBufHead->wFragCtl |= (WORD)wFragType;//0x0001; //0000 0000 0000 0001

    //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"s_vFillFragParameter END\n");
}

static
UINT
s_cbFillTxBufHead (
    IN  PSDevice         pDevice,
    IN  BYTE             byPktType,
    IN  PBYTE            pbyTxBufferAddr,
    IN  UINT             cbFrameBodySize,
    IN  UINT             uDMAIdx,
    IN  PSTxDesc         pHeadTD,
    IN  PSEthernetHeader psEthHeader,
    IN  PBYTE            pPacket,
    IN  BOOL             bNeedEncrypt,
    IN  PSKeyItem        pTransmitKey,
    IN  UINT             uNodeIndex,
    OUT PUINT            puMACfragNum
    )
{
    UINT           cbMACHdLen;
    UINT           cbFrameSize;
    UINT           cbFragmentSize; //Hdr+(IV)+payoad+(MIC)+(ICV)+FCS
    UINT           cbFragPayloadSize;
    UINT           cbLastFragmentSize; //Hdr+(IV)+payoad+(MIC)+(ICV)+FCS
    UINT           cbLastFragPayloadSize;
    UINT           uFragIdx;
    PBYTE          pbyPayloadHead;
    PBYTE          pbyIVHead;
    PBYTE          pbyMacHdr;
    WORD           wFragType; //00:Non-Frag, 01:Start, 10:Mid, 11:Last
    UINT           uDuration;
    PBYTE          pbyBuffer;
//    UINT           uKeyEntryIdx = NUM_KEY_ENTRY+1;
//    BYTE           byKeySel = 0xFF;
    UINT           cbIVlen = 0;
    UINT           cbICVlen = 0;
    UINT           cbMIClen = 0;
    UINT           cbFCSlen = 4;
    UINT           cb802_1_H_len = 0;
    UINT           uLength = 0;
    UINT           uTmpLen = 0;
//    BYTE           abyTmp[8];
//    DWORD          dwCRC;
    UINT           cbMICHDR = 0;
    DWORD          dwMICKey0, dwMICKey1;
    DWORD          dwMIC_Priority;
    PDWORD         pdwMIC_L;
    PDWORD         pdwMIC_R;
    DWORD          dwSafeMIC_L, dwSafeMIC_R; //Fix "Last Frag Size" < "MIC length".
    BOOL           bMIC2Frag = FALSE;
    UINT           uMICFragLen = 0;
    UINT           uMACfragNum = 1;
    UINT           uPadding = 0;
    UINT           cbReqCount = 0;

    BOOL           bNeedACK;
    BOOL           bRTS;
    BOOL           bIsAdhoc;
    PBYTE          pbyType;
    PSTxDesc       ptdCurr;
    PSTxBufHead    psTxBufHd = (PSTxBufHead) pbyTxBufferAddr;
//    UINT           tmpDescIdx;
    UINT           cbHeaderLength = 0;
    PVOID          pvRrvTime;
    PSMICHDRHead   pMICHDR;
    PVOID          pvRTS;
    PVOID          pvCTS;
    PVOID          pvTxDataHd;
    WORD           wTxBufSize;   // FFinfo size
    UINT           uTotalCopyLength = 0;
    BYTE           byFBOption = AUTO_FB_NONE;
    BOOL           bIsWEP256 = FALSE;
    PSMgmtObject    pMgmt = pDevice->pMgmt;


    pvRrvTime = pMICHDR = pvRTS = pvCTS = pvTxDataHd = NULL;

    //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"s_cbFillTxBufHead...\n");
    if ((pDevice->eOPMode == OP_MODE_ADHOC) ||
        (pDevice->eOPMode == OP_MODE_AP)) {

        if (IS_MULTICAST_ADDRESS(&(psEthHeader->abyDstAddr[0])) ||
            IS_BROADCAST_ADDRESS(&(psEthHeader->abyDstAddr[0]))) {
            bNeedACK = FALSE;
        }
        else {
            bNeedACK = TRUE;
        }
        bIsAdhoc = TRUE;
    }
    else {
        // MSDUs in Infra mode always need ACK
        bNeedACK = TRUE;
        bIsAdhoc = FALSE;
    }

    if (pDevice->bLongHeader)
        cbMACHdLen = WLAN_HDR_ADDR3_LEN + 6;
    else
        cbMACHdLen = WLAN_HDR_ADDR3_LEN;


    if ((bNeedEncrypt == TRUE) && (pTransmitKey != NULL)) {
        if (pTransmitKey->byCipherSuite == KEY_CTL_WEP) {
            cbIVlen = 4;
            cbICVlen = 4;
            if (pTransmitKey->uKeyLength == WLAN_WEP232_KEYLEN) {
                bIsWEP256 = TRUE;
            }
        }
        if (pTransmitKey->byCipherSuite == KEY_CTL_TKIP) {
            cbIVlen = 8;//IV+ExtIV
            cbMIClen = 8;
            cbICVlen = 4;
        }
        if (pTransmitKey->byCipherSuite == KEY_CTL_CCMP) {
            cbIVlen = 8;//RSN Header
            cbICVlen = 8;//MIC
            cbMICHDR = sizeof(SMICHDRHead);
        }
        if (pDevice->byLocalID > REV_ID_VT3253_A1) {
            //MAC Header should be padding 0 to DW alignment.
            uPadding = 4 - (cbMACHdLen%4);
            uPadding %= 4;
        }
    }


    cbFrameSize = cbMACHdLen + cbIVlen + (cbFrameBodySize + cbMIClen) + cbICVlen + cbFCSlen;

    if ((bNeedACK == FALSE) ||
        (cbFrameSize < pDevice->wRTSThreshold) ||
        ((cbFrameSize >= pDevice->wFragmentationThreshold) && (pDevice->wFragmentationThreshold <= pDevice->wRTSThreshold))
        ) {
        bRTS = FALSE;
    }
    else {
        bRTS = TRUE;
        psTxBufHd->wFIFOCtl |= (FIFOCTL_RTS | FIFOCTL_LRETRY);
    }
    //
    // Use for AUTO FALL BACK
    //
    if (psTxBufHd->wFIFOCtl & FIFOCTL_AUTO_FB_0) {
        byFBOption = AUTO_FB_0;
    }
    else if (psTxBufHd->wFIFOCtl & FIFOCTL_AUTO_FB_1) {
        byFBOption = AUTO_FB_1;
    }

    //////////////////////////////////////////////////////
    //Set RrvTime/RTS/CTS Buffer
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
            else { //RTS_needless
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
            if (bRTS == TRUE) {
                pvRrvTime = (PSRrvTime_ab) (pbyTxBufferAddr + wTxBufSize);
                pMICHDR = (PSMICHDRHead) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab));
                pvRTS = (PSRTS_ab) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR);
                pvCTS = NULL;
                pvTxDataHd = (PSTxDataHead_ab) (pbyTxBufferAddr + wTxBufSize + sizeof(PSRrvTime_ab) + cbMICHDR + sizeof(SRTS_ab));
                cbHeaderLength = wTxBufSize + sizeof(PSRrvTime_ab) + cbMICHDR + sizeof(SRTS_ab) + sizeof(STxDataHead_ab);
            }
            else { //RTS_needless, need MICHDR
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
                pvTxDataHd = (PSTxDataHead_a_FB) (pbyTxBufferAddr + wTxBufSize + sizeof(PSRrvTime_ab) + cbMICHDR + sizeof(SRTS_a_FB));
                cbHeaderLength = wTxBufSize + sizeof(PSRrvTime_ab) + cbMICHDR + sizeof(SRTS_a_FB) + sizeof(STxDataHead_a_FB);
            }
            else { //RTS_needless
                pvRrvTime = (PSRrvTime_ab) (pbyTxBufferAddr + wTxBufSize);
                pMICHDR = (PSMICHDRHead) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab));
                pvRTS = NULL;
                pvCTS = NULL;
                pvTxDataHd = (PSTxDataHead_a_FB) (pbyTxBufferAddr + wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR);
                cbHeaderLength = wTxBufSize + sizeof(SRrvTime_ab) + cbMICHDR + sizeof(STxDataHead_a_FB);
            }
        } // Auto Fall Back
    }
    memset((PVOID)(pbyTxBufferAddr + wTxBufSize), 0, (cbHeaderLength - wTxBufSize));

//////////////////////////////////////////////////////////////////
    if ((bNeedEncrypt == TRUE) && (pTransmitKey != NULL) && (pTransmitKey->byCipherSuite == KEY_CTL_TKIP)) {
        if (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPANONE) {
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
    }

///////////////////////////////////////////////////////////////////

    pbyMacHdr = (PBYTE)(pbyTxBufferAddr + cbHeaderLength);
    pbyPayloadHead = (PBYTE)(pbyMacHdr + cbMACHdLen + uPadding + cbIVlen);
    pbyIVHead = (PBYTE)(pbyMacHdr + cbMACHdLen + uPadding);

    if ((cbFrameSize > pDevice->wFragmentationThreshold) && (bNeedACK == TRUE) && (bIsWEP256 == FALSE)) {
        // Fragmentation
        // FragThreshold = Fragment size(Hdr+(IV)+fragment payload+(MIC)+(ICV)+FCS)
        cbFragmentSize = pDevice->wFragmentationThreshold;
        cbFragPayloadSize = cbFragmentSize - cbMACHdLen - cbIVlen - cbICVlen - cbFCSlen;
        //FragNum = (FrameSize-(Hdr+FCS))/(Fragment Size -(Hrd+FCS)))
        uMACfragNum = (WORD) ((cbFrameBodySize + cbMIClen) / cbFragPayloadSize);
        cbLastFragPayloadSize = (cbFrameBodySize + cbMIClen) % cbFragPayloadSize;
        if (cbLastFragPayloadSize == 0) {
            cbLastFragPayloadSize = cbFragPayloadSize;
        } else {
            uMACfragNum++;
        }
        //[Hdr+(IV)+last fragment payload+(MIC)+(ICV)+FCS]
        cbLastFragmentSize = cbMACHdLen + cbLastFragPayloadSize + cbIVlen + cbICVlen + cbFCSlen;

        for (uFragIdx = 0; uFragIdx < uMACfragNum; uFragIdx ++) {
            if (uFragIdx == 0) {
                //=========================
                //    Start Fragmentation
                //=========================
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Start Fragmentation...\n");
                wFragType = FRAGCTL_STAFRAG;


                //Fill FIFO,RrvTime,RTS,and CTS
                s_vGenerateTxParameter(pDevice, byPktType, (PVOID)psTxBufHd, pvRrvTime, pvRTS, pvCTS,
                                       cbFragmentSize, bNeedACK, uDMAIdx, psEthHeader, pDevice->wCurrentRate);
                //Fill DataHead
                uDuration = s_uFillDataHead(pDevice, byPktType, pvTxDataHd, cbFragmentSize, uDMAIdx, bNeedACK,
                                            uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption, pDevice->wCurrentRate);
                // Generate TX MAC Header
                vGenerateMACHeader(pDevice, pbyMacHdr, (WORD)uDuration, psEthHeader, bNeedEncrypt,
                                   wFragType, uDMAIdx, uFragIdx);

                if (bNeedEncrypt == TRUE) {
                    //Fill TXKEY
                    s_vFillTxKey(pDevice, (PBYTE)(psTxBufHd->adwTxKey), pbyIVHead, pTransmitKey,
                                 pbyMacHdr, (WORD)cbFragPayloadSize, (PBYTE)pMICHDR);
                    //Fill IV(ExtIV,RSNHDR)
                    if (pDevice->bEnableHostWEP) {
                        pMgmt->sNodeDBTable[uNodeIndex].dwTSC47_16 = pTransmitKey->dwTSC47_16;
                        pMgmt->sNodeDBTable[uNodeIndex].wTSC15_0 = pTransmitKey->wTSC15_0;
                    }
                }


                // 802.1H
                if (ntohs(psEthHeader->wType) > MAX_DATA_LEN) {
                    if ((psEthHeader->wType == TYPE_PKT_IPX) ||
                        (psEthHeader->wType == cpu_to_le16(0xF380))) {
                        memcpy((PBYTE) (pbyPayloadHead), &pDevice->abySNAP_Bridgetunnel[0], 6);
                    }
                    else {
                        memcpy((PBYTE) (pbyPayloadHead), &pDevice->abySNAP_RFC1042[0], 6);
                    }
                    pbyType = (PBYTE) (pbyPayloadHead + 6);
                    memcpy(pbyType, &(psEthHeader->wType), sizeof(WORD));
                    cb802_1_H_len = 8;
                }

                cbReqCount = cbHeaderLength + cbMACHdLen + uPadding + cbIVlen + cbFragPayloadSize;
                //---------------------------
                // S/W or H/W Encryption
                //---------------------------
                //Fill MICHDR
                //if (pDevice->bAES) {
                //    s_vFillMICHDR(pDevice, (PBYTE)pMICHDR, pbyMacHdr, (WORD)cbFragPayloadSize);
                //}
                //cbReqCount += s_uDoEncryption(pDevice, psEthHeader, (PVOID)psTxBufHd, byKeySel,
                //                                pbyPayloadHead, (WORD)cbFragPayloadSize, uDMAIdx);



                //pbyBuffer = (PBYTE)pDevice->aamTxBuf[uDMAIdx][uDescIdx].pbyVAddr;
                pbyBuffer = (PBYTE)pHeadTD->pTDInfo->buf;

                uLength = cbHeaderLength + cbMACHdLen + uPadding + cbIVlen + cb802_1_H_len;
                //copy TxBufferHeader + MacHeader to desc
                memcpy(pbyBuffer, (PVOID)psTxBufHd, uLength);

                // Copy the Packet into a tx Buffer
                memcpy((pbyBuffer + uLength), (pPacket + 14), (cbFragPayloadSize - cb802_1_H_len));


                uTotalCopyLength += cbFragPayloadSize - cb802_1_H_len;

                if ((bNeedEncrypt == TRUE) && (pTransmitKey != NULL) && (pTransmitKey->byCipherSuite == KEY_CTL_TKIP)) {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Start MIC: %d\n", cbFragPayloadSize);
                    MIC_vAppend((pbyBuffer + uLength - cb802_1_H_len), cbFragPayloadSize);

                }

                //---------------------------
                // S/W Encryption
                //---------------------------
                if ((pDevice->byLocalID <= REV_ID_VT3253_A1)) {
                    if (bNeedEncrypt) {
                        s_vSWencryption(pDevice, pTransmitKey, (pbyBuffer + uLength - cb802_1_H_len), (WORD)cbFragPayloadSize);
                        cbReqCount += cbICVlen;
                    }
                }

                ptdCurr = (PSTxDesc)pHeadTD;
                //--------------------
                //1.Set TSR1 & ReqCount in TxDescHead
                //2.Set FragCtl in TxBufferHead
                //3.Set Frame Control
                //4.Set Sequence Control
                //5.Get S/W generate FCS
                //--------------------
                s_vFillFragParameter(pDevice, pbyBuffer, uDMAIdx, (PVOID)ptdCurr, wFragType, cbReqCount);

                ptdCurr->pTDInfo->dwReqCount = cbReqCount - uPadding;
                ptdCurr->pTDInfo->dwHeaderLength = cbHeaderLength;
                ptdCurr->pTDInfo->skb_dma = ptdCurr->pTDInfo->buf_dma;
                ptdCurr->buff_addr = cpu_to_le32(ptdCurr->pTDInfo->skb_dma);
                pDevice->iTDUsed[uDMAIdx]++;
                pHeadTD = ptdCurr->next;
            }
            else if (uFragIdx == (uMACfragNum-1)) {
                //=========================
                //    Last Fragmentation
                //=========================
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Last Fragmentation...\n");
                //tmpDescIdx = (uDescIdx + uFragIdx) % pDevice->cbTD[uDMAIdx];

                wFragType = FRAGCTL_ENDFRAG;

                //Fill FIFO,RrvTime,RTS,and CTS
                s_vGenerateTxParameter(pDevice, byPktType, (PVOID)psTxBufHd, pvRrvTime, pvRTS, pvCTS,
                                       cbLastFragmentSize, bNeedACK, uDMAIdx, psEthHeader, pDevice->wCurrentRate);
                //Fill DataHead
                uDuration = s_uFillDataHead(pDevice, byPktType, pvTxDataHd, cbLastFragmentSize, uDMAIdx, bNeedACK,
                                            uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption, pDevice->wCurrentRate);

                // Generate TX MAC Header
                vGenerateMACHeader(pDevice, pbyMacHdr, (WORD)uDuration, psEthHeader, bNeedEncrypt,
                                   wFragType, uDMAIdx, uFragIdx);

                if (bNeedEncrypt == TRUE) {
                    //Fill TXKEY
                    s_vFillTxKey(pDevice, (PBYTE)(psTxBufHd->adwTxKey), pbyIVHead, pTransmitKey,
                                 pbyMacHdr, (WORD)cbLastFragPayloadSize, (PBYTE)pMICHDR);

                    if (pDevice->bEnableHostWEP) {
                        pMgmt->sNodeDBTable[uNodeIndex].dwTSC47_16 = pTransmitKey->dwTSC47_16;
                        pMgmt->sNodeDBTable[uNodeIndex].wTSC15_0 = pTransmitKey->wTSC15_0;
                    }

                }


                cbReqCount = cbHeaderLength + cbMACHdLen + uPadding + cbIVlen + cbLastFragPayloadSize;
                //---------------------------
                // S/W or H/W Encryption
                //---------------------------



                pbyBuffer = (PBYTE)pHeadTD->pTDInfo->buf;
                //pbyBuffer = (PBYTE)pDevice->aamTxBuf[uDMAIdx][tmpDescIdx].pbyVAddr;

                uLength = cbHeaderLength + cbMACHdLen + uPadding + cbIVlen;

                //copy TxBufferHeader + MacHeader to desc
                memcpy(pbyBuffer, (PVOID)psTxBufHd, uLength);

                // Copy the Packet into a tx Buffer
                if (bMIC2Frag == FALSE) {

                    memcpy((pbyBuffer + uLength),
                             (pPacket + 14 + uTotalCopyLength),
                             (cbLastFragPayloadSize - cbMIClen)
                             );
                    //TODO check uTmpLen !
                    uTmpLen = cbLastFragPayloadSize - cbMIClen;

                }
                if ((bNeedEncrypt == TRUE) && (pTransmitKey != NULL) && (pTransmitKey->byCipherSuite == KEY_CTL_TKIP)) {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"LAST: uMICFragLen:%d, cbLastFragPayloadSize:%d, uTmpLen:%d\n",
                                   uMICFragLen, cbLastFragPayloadSize, uTmpLen);

                    if (bMIC2Frag == FALSE) {
                        if (uTmpLen != 0)
                            MIC_vAppend((pbyBuffer + uLength), uTmpLen);
                        pdwMIC_L = (PDWORD)(pbyBuffer + uLength + uTmpLen);
                        pdwMIC_R = (PDWORD)(pbyBuffer + uLength + uTmpLen + 4);
                        MIC_vGetMIC(pdwMIC_L, pdwMIC_R);
                        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Last MIC:%lX, %lX\n", *pdwMIC_L, *pdwMIC_R);
                    } else {
                        if (uMICFragLen >= 4) {
                            memcpy((pbyBuffer + uLength), ((PBYTE)&dwSafeMIC_R + (uMICFragLen - 4)),
                                     (cbMIClen - uMICFragLen));
                            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"LAST: uMICFragLen >= 4: %X, %d\n",
                                           *(PBYTE)((PBYTE)&dwSafeMIC_R + (uMICFragLen - 4)),
                                           (cbMIClen - uMICFragLen));

                        } else {
                            memcpy((pbyBuffer + uLength), ((PBYTE)&dwSafeMIC_L + uMICFragLen),
                                     (4 - uMICFragLen));
                            memcpy((pbyBuffer + uLength + (4 - uMICFragLen)), &dwSafeMIC_R, 4);
                            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"LAST: uMICFragLen < 4: %X, %d\n",
                                           *(PBYTE)((PBYTE)&dwSafeMIC_R + uMICFragLen - 4),
                                           (cbMIClen - uMICFragLen));
                        }
                        /*
                        for (ii = 0; ii < cbLastFragPayloadSize + 8 + 24; ii++) {
                            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"%02x ", *((PBYTE)((pbyBuffer + uLength) + ii - 8 - 24)));
                        }
                        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"\n\n");
                        */
                    }
                    MIC_vUnInit();
                } else {
                    ASSERT(uTmpLen == (cbLastFragPayloadSize - cbMIClen));
                }


                //---------------------------
                // S/W Encryption
                //---------------------------
                if ((pDevice->byLocalID <= REV_ID_VT3253_A1)) {
                    if (bNeedEncrypt) {
                        s_vSWencryption(pDevice, pTransmitKey, (pbyBuffer + uLength), (WORD)cbLastFragPayloadSize);
                        cbReqCount += cbICVlen;
                    }
                }

                ptdCurr = (PSTxDesc)pHeadTD;

                //--------------------
                //1.Set TSR1 & ReqCount in TxDescHead
                //2.Set FragCtl in TxBufferHead
                //3.Set Frame Control
                //4.Set Sequence Control
                //5.Get S/W generate FCS
                //--------------------


                s_vFillFragParameter(pDevice, pbyBuffer, uDMAIdx, (PVOID)ptdCurr, wFragType, cbReqCount);

                ptdCurr->pTDInfo->dwReqCount = cbReqCount - uPadding;
                ptdCurr->pTDInfo->dwHeaderLength = cbHeaderLength;
                ptdCurr->pTDInfo->skb_dma = ptdCurr->pTDInfo->buf_dma;
                ptdCurr->buff_addr = cpu_to_le32(ptdCurr->pTDInfo->skb_dma);
                pDevice->iTDUsed[uDMAIdx]++;
                pHeadTD = ptdCurr->next;

            }
            else {
                //=========================
                //    Middle Fragmentation
                //=========================
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Middle Fragmentation...\n");
                //tmpDescIdx = (uDescIdx + uFragIdx) % pDevice->cbTD[uDMAIdx];

                wFragType = FRAGCTL_MIDFRAG;

                //Fill FIFO,RrvTime,RTS,and CTS
                s_vGenerateTxParameter(pDevice, byPktType, (PVOID)psTxBufHd, pvRrvTime, pvRTS, pvCTS,
                                       cbFragmentSize, bNeedACK, uDMAIdx, psEthHeader, pDevice->wCurrentRate);
                //Fill DataHead
                uDuration = s_uFillDataHead(pDevice, byPktType, pvTxDataHd, cbFragmentSize, uDMAIdx, bNeedACK,
                                            uFragIdx, cbLastFragmentSize, uMACfragNum, byFBOption, pDevice->wCurrentRate);

                // Generate TX MAC Header
                vGenerateMACHeader(pDevice, pbyMacHdr, (WORD)uDuration, psEthHeader, bNeedEncrypt,
                                   wFragType, uDMAIdx, uFragIdx);


                if (bNeedEncrypt == TRUE) {
                    //Fill TXKEY
                    s_vFillTxKey(pDevice, (PBYTE)(psTxBufHd->adwTxKey), pbyIVHead, pTransmitKey,
                                 pbyMacHdr, (WORD)cbFragPayloadSize, (PBYTE)pMICHDR);

                    if (pDevice->bEnableHostWEP) {
                        pMgmt->sNodeDBTable[uNodeIndex].dwTSC47_16 = pTransmitKey->dwTSC47_16;
                        pMgmt->sNodeDBTable[uNodeIndex].wTSC15_0 = pTransmitKey->wTSC15_0;
                    }
                }

                cbReqCount = cbHeaderLength + cbMACHdLen + uPadding + cbIVlen + cbFragPayloadSize;
                //---------------------------
                // S/W or H/W Encryption
                //---------------------------
                //Fill MICHDR
                //if (pDevice->bAES) {
                //    s_vFillMICHDR(pDevice, (PBYTE)pMICHDR, pbyMacHdr, (WORD)cbFragPayloadSize);
                //}
                //cbReqCount += s_uDoEncryption(pDevice, psEthHeader, (PVOID)psTxBufHd, byKeySel,
                //                              pbyPayloadHead, (WORD)cbFragPayloadSize, uDMAIdx);


                pbyBuffer = (PBYTE)pHeadTD->pTDInfo->buf;
                //pbyBuffer = (PBYTE)pDevice->aamTxBuf[uDMAIdx][tmpDescIdx].pbyVAddr;


                uLength = cbHeaderLength + cbMACHdLen + uPadding + cbIVlen;

                //copy TxBufferHeader + MacHeader to desc
                memcpy(pbyBuffer, (PVOID)psTxBufHd, uLength);

                // Copy the Packet into a tx Buffer
                memcpy((pbyBuffer + uLength),
                         (pPacket + 14 + uTotalCopyLength),
                         cbFragPayloadSize
                        );
                uTmpLen = cbFragPayloadSize;

                uTotalCopyLength += uTmpLen;

                if ((bNeedEncrypt == TRUE) && (pTransmitKey != NULL) && (pTransmitKey->byCipherSuite == KEY_CTL_TKIP)) {

                    MIC_vAppend((pbyBuffer + uLength), uTmpLen);

                    if (uTmpLen < cbFragPayloadSize) {
                        bMIC2Frag = TRUE;
                        uMICFragLen = cbFragPayloadSize - uTmpLen;
                        ASSERT(uMICFragLen < cbMIClen);

                        pdwMIC_L = (PDWORD)(pbyBuffer + uLength + uTmpLen);
                        pdwMIC_R = (PDWORD)(pbyBuffer + uLength + uTmpLen + 4);
                        MIC_vGetMIC(pdwMIC_L, pdwMIC_R);
                        dwSafeMIC_L = *pdwMIC_L;
                        dwSafeMIC_R = *pdwMIC_R;

                        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"MIDDLE: uMICFragLen:%d, cbFragPayloadSize:%d, uTmpLen:%d\n",
                                       uMICFragLen, cbFragPayloadSize, uTmpLen);
                        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Fill MIC in Middle frag [%d]\n", uMICFragLen);
                        /*
                        for (ii = 0; ii < uMICFragLen; ii++) {
                            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"%02x ", *((PBYTE)((pbyBuffer + uLength + uTmpLen) + ii)));
                        }
                        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"\n");
                        */
                        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Get MIC:%lX, %lX\n", *pdwMIC_L, *pdwMIC_R);
                    }
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Middle frag len: %d\n", uTmpLen);
                    /*
                    for (ii = 0; ii < uTmpLen; ii++) {
                        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"%02x ", *((PBYTE)((pbyBuffer + uLength) + ii)));
                    }
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"\n\n");
                    */

                } else {
                    ASSERT(uTmpLen == (cbFragPayloadSize));
                }

                if ((pDevice->byLocalID <= REV_ID_VT3253_A1)) {
                    if (bNeedEncrypt) {
                        s_vSWencryption(pDevice, pTransmitKey, (pbyBuffer + uLength), (WORD)cbFragPayloadSize);
                        cbReqCount += cbICVlen;
                    }
                }

                ptdCurr = (PSTxDesc)pHeadTD;

                //--------------------
                //1.Set TSR1 & ReqCount in TxDescHead
                //2.Set FragCtl in TxBufferHead
                //3.Set Frame Control
                //4.Set Sequence Control
                //5.Get S/W generate FCS
                //--------------------

                s_vFillFragParameter(pDevice, pbyBuffer, uDMAIdx, (PVOID)ptdCurr, wFragType, cbReqCount);

                ptdCurr->pTDInfo->dwReqCount = cbReqCount - uPadding;
                ptdCurr->pTDInfo->dwHeaderLength = cbHeaderLength;
                ptdCurr->pTDInfo->skb_dma = ptdCurr->pTDInfo->buf_dma;
                ptdCurr->buff_addr = cpu_to_le32(ptdCurr->pTDInfo->skb_dma);
                pDevice->iTDUsed[uDMAIdx]++;
                pHeadTD = ptdCurr->next;
            }
        }  // for (uMACfragNum)
    }
    else {
        //=========================
        //    No Fragmentation
        //=========================
        //DBG_PRTGRP03(("No Fragmentation...\n"));
        //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"No Fragmentation...\n");
        wFragType = FRAGCTL_NONFRAG;

        //Set FragCtl in TxBufferHead
        psTxBufHd->wFragCtl |= (WORD)wFragType;

        //Fill FIFO,RrvTime,RTS,and CTS
        s_vGenerateTxParameter(pDevice, byPktType, (PVOID)psTxBufHd, pvRrvTime, pvRTS, pvCTS,
                               cbFrameSize, bNeedACK, uDMAIdx, psEthHeader, pDevice->wCurrentRate);
        //Fill DataHead
        uDuration = s_uFillDataHead(pDevice, byPktType, pvTxDataHd, cbFrameSize, uDMAIdx, bNeedACK,
                                    0, 0, uMACfragNum, byFBOption, pDevice->wCurrentRate);

        // Generate TX MAC Header
        vGenerateMACHeader(pDevice, pbyMacHdr, (WORD)uDuration, psEthHeader, bNeedEncrypt,
                           wFragType, uDMAIdx, 0);

        if (bNeedEncrypt == TRUE) {
            //Fill TXKEY
            s_vFillTxKey(pDevice, (PBYTE)(psTxBufHd->adwTxKey), pbyIVHead, pTransmitKey,
                         pbyMacHdr, (WORD)cbFrameBodySize, (PBYTE)pMICHDR);

            if (pDevice->bEnableHostWEP) {
                pMgmt->sNodeDBTable[uNodeIndex].dwTSC47_16 = pTransmitKey->dwTSC47_16;
                pMgmt->sNodeDBTable[uNodeIndex].wTSC15_0 = pTransmitKey->wTSC15_0;
            }
        }

        // 802.1H
        if (ntohs(psEthHeader->wType) > MAX_DATA_LEN) {
            if ((psEthHeader->wType == TYPE_PKT_IPX) ||
                (psEthHeader->wType == cpu_to_le16(0xF380))) {
                memcpy((PBYTE) (pbyPayloadHead), &pDevice->abySNAP_Bridgetunnel[0], 6);
            }
            else {
                memcpy((PBYTE) (pbyPayloadHead), &pDevice->abySNAP_RFC1042[0], 6);
            }
            pbyType = (PBYTE) (pbyPayloadHead + 6);
            memcpy(pbyType, &(psEthHeader->wType), sizeof(WORD));
            cb802_1_H_len = 8;
        }

        cbReqCount = cbHeaderLength + cbMACHdLen + uPadding + cbIVlen + (cbFrameBodySize + cbMIClen);
        //---------------------------
        // S/W or H/W Encryption
        //---------------------------
        //Fill MICHDR
        //if (pDevice->bAES) {
        //    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Fill MICHDR...\n");
        //    s_vFillMICHDR(pDevice, (PBYTE)pMICHDR, pbyMacHdr, (WORD)cbFrameBodySize);
        //}

        pbyBuffer = (PBYTE)pHeadTD->pTDInfo->buf;
        //pbyBuffer = (PBYTE)pDevice->aamTxBuf[uDMAIdx][uDescIdx].pbyVAddr;

        uLength = cbHeaderLength + cbMACHdLen + uPadding + cbIVlen + cb802_1_H_len;

        //copy TxBufferHeader + MacHeader to desc
        memcpy(pbyBuffer, (PVOID)psTxBufHd, uLength);

        // Copy the Packet into a tx Buffer
        memcpy((pbyBuffer + uLength),
                 (pPacket + 14),
                 cbFrameBodySize - cb802_1_H_len
                 );

        if ((bNeedEncrypt == TRUE) && (pTransmitKey != NULL) && (pTransmitKey->byCipherSuite == KEY_CTL_TKIP)){

            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Length:%d, %d\n", cbFrameBodySize - cb802_1_H_len, uLength);
            /*
            for (ii = 0; ii < (cbFrameBodySize - cb802_1_H_len); ii++) {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"%02x ", *((PBYTE)((pbyBuffer + uLength) + ii)));
            }
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"\n");
            */

            MIC_vAppend((pbyBuffer + uLength - cb802_1_H_len), cbFrameBodySize);

            pdwMIC_L = (PDWORD)(pbyBuffer + uLength - cb802_1_H_len + cbFrameBodySize);
            pdwMIC_R = (PDWORD)(pbyBuffer + uLength - cb802_1_H_len + cbFrameBodySize + 4);

            MIC_vGetMIC(pdwMIC_L, pdwMIC_R);
            MIC_vUnInit();


            if (pDevice->bTxMICFail == TRUE) {
                *pdwMIC_L = 0;
                *pdwMIC_R = 0;
                pDevice->bTxMICFail = FALSE;
            }

            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"uLength: %d, %d\n", uLength, cbFrameBodySize);
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"cbReqCount:%d, %d, %d, %d\n", cbReqCount, cbHeaderLength, uPadding, cbIVlen);
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"MIC:%lx, %lx\n", *pdwMIC_L, *pdwMIC_R);
/*
            for (ii = 0; ii < 8; ii++) {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"%02x ", *(((PBYTE)(pdwMIC_L) + ii)));
            }
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"\n");
*/

        }


        if ((pDevice->byLocalID <= REV_ID_VT3253_A1)){
            if (bNeedEncrypt) {
                s_vSWencryption(pDevice, pTransmitKey, (pbyBuffer + uLength - cb802_1_H_len),
                                (WORD)(cbFrameBodySize + cbMIClen));
                cbReqCount += cbICVlen;
            }
        }


        ptdCurr = (PSTxDesc)pHeadTD;

        ptdCurr->pTDInfo->dwReqCount = cbReqCount - uPadding;
        ptdCurr->pTDInfo->dwHeaderLength = cbHeaderLength;
        ptdCurr->pTDInfo->skb_dma = ptdCurr->pTDInfo->buf_dma;
        ptdCurr->buff_addr = cpu_to_le32(ptdCurr->pTDInfo->skb_dma);
  	    //Set TSR1 & ReqCount in TxDescHead
        ptdCurr->m_td1TD1.byTCR |= (TCR_STP | TCR_EDP | EDMSDU);
        ptdCurr->m_td1TD1.wReqCount = cpu_to_le16((WORD)(cbReqCount));

        pDevice->iTDUsed[uDMAIdx]++;


//   DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" ptdCurr->m_dwReserved0[%d] ptdCurr->m_dwReserved1[%d].\n", ptdCurr->pTDInfo->dwReqCount, ptdCurr->pTDInfo->dwHeaderLength);
//   DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" cbHeaderLength[%d]\n", cbHeaderLength);

    }
    *puMACfragNum = uMACfragNum;
    //DBG_PRTGRP03(("s_cbFillTxBufHead END\n"));
    return cbHeaderLength;
}


VOID
vGenerateFIFOHeader (
    IN  PSDevice         pDevice,
    IN  BYTE             byPktType,
    IN  PBYTE            pbyTxBufferAddr,
    IN  BOOL             bNeedEncrypt,
    IN  UINT             cbPayloadSize,
    IN  UINT             uDMAIdx,
    IN  PSTxDesc         pHeadTD,
    IN  PSEthernetHeader psEthHeader,
    IN  PBYTE            pPacket,
    IN  PSKeyItem        pTransmitKey,
    IN  UINT             uNodeIndex,
    OUT PUINT            puMACfragNum,
    OUT PUINT            pcbHeaderSize
    )
{
    UINT            wTxBufSize;       // FFinfo size
    BOOL            bNeedACK;
    BOOL            bIsAdhoc;
    WORD            cbMacHdLen;
    PSTxBufHead     pTxBufHead = (PSTxBufHead) pbyTxBufferAddr;

    wTxBufSize = sizeof(STxBufHead);

    memset(pTxBufHead, 0, wTxBufSize);
    //Set FIFOCTL_NEEDACK

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
        bIsAdhoc = TRUE;
    }
    else {
        // MSDUs in Infra mode always need ACK
        bNeedACK = TRUE;
        pTxBufHead->wFIFOCtl |= FIFOCTL_NEEDACK;
        bIsAdhoc = FALSE;
    }


    pTxBufHead->wFIFOCtl |= FIFOCTL_TMOEN;
    pTxBufHead->wTimeStamp = cpu_to_le16(DEFAULT_MSDU_LIFETIME_RES_64us);

    //Set FIFOCTL_LHEAD
    if (pDevice->bLongHeader)
        pTxBufHead->wFIFOCtl |= FIFOCTL_LHEAD;

    //Set FIFOCTL_GENINT

    pTxBufHead->wFIFOCtl |= FIFOCTL_GENINT;


    //Set FIFOCTL_ISDMA0
    if (TYPE_TXDMA0 == uDMAIdx) {
        pTxBufHead->wFIFOCtl |= FIFOCTL_ISDMA0;
    }

    //Set FRAGCTL_MACHDCNT
    if (pDevice->bLongHeader) {
        cbMacHdLen = WLAN_HDR_ADDR3_LEN + 6;
    } else {
        cbMacHdLen = WLAN_HDR_ADDR3_LEN;
    }
    pTxBufHead->wFragCtl |= cpu_to_le16((WORD)(cbMacHdLen << 10));

    //Set packet type
    if (byPktType == PK_TYPE_11A) {//0000 0000 0000 0000
        ;
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
    //Set FIFOCTL_GrpAckPolicy
    if (pDevice->bGrpAckPolicy == TRUE) {//0000 0100 0000 0000
        pTxBufHead->wFIFOCtl |=	FIFOCTL_GRPACK;
    }

    //Set Auto Fallback Ctl
    if (pDevice->wCurrentRate >= RATE_18M) {
        if (pDevice->byAutoFBCtrl == AUTO_FB_0) {
            pTxBufHead->wFIFOCtl |= FIFOCTL_AUTO_FB_0;
        } else if (pDevice->byAutoFBCtrl == AUTO_FB_1) {
            pTxBufHead->wFIFOCtl |= FIFOCTL_AUTO_FB_1;
        }
    }

    //Set FRAGCTL_WEPTYP
    pDevice->bAES = FALSE;

    //Set FRAGCTL_WEPTYP
    if (pDevice->byLocalID > REV_ID_VT3253_A1) {
        if ((bNeedEncrypt) && (pTransmitKey != NULL))  { //WEP enabled
            if (pTransmitKey->byCipherSuite == KEY_CTL_TKIP) {
                pTxBufHead->wFragCtl |= FRAGCTL_TKIP;
            }
            else if (pTransmitKey->byCipherSuite == KEY_CTL_WEP) { //WEP40 or WEP104
                if (pTransmitKey->uKeyLength != WLAN_WEP232_KEYLEN)
                    pTxBufHead->wFragCtl |= FRAGCTL_LEGACY;
            }
            else if (pTransmitKey->byCipherSuite == KEY_CTL_CCMP) { //CCMP
                pTxBufHead->wFragCtl |= FRAGCTL_AES;
            }
        }
    }

#ifdef	PLICE_DEBUG
	//printk("Func:vGenerateFIFOHeader:TxDataRate is %d,TxPower is %d\n",pDevice->wCurrentRate,pDevice->byCurPwr);

	//if (pDevice->wCurrentRate <= 3)
	//{
	//	RFbRawSetPower(pDevice,36,pDevice->wCurrentRate);
	//}
	//else

	RFbSetPower(pDevice, pDevice->wCurrentRate, pDevice->byCurrentCh);
#endif
		//if (pDevice->wCurrentRate == 3)
		//pDevice->byCurPwr = 46;
		pTxBufHead->byTxPower = pDevice->byCurPwr;




/*
    if(pDevice->bEnableHostWEP)
        pTxBufHead->wFragCtl &=  ~(FRAGCTL_TKIP | FRAGCTL_LEGACY |FRAGCTL_AES);
*/
    *pcbHeaderSize = s_cbFillTxBufHead(pDevice, byPktType, pbyTxBufferAddr, cbPayloadSize,
                                   uDMAIdx, pHeadTD, psEthHeader, pPacket, bNeedEncrypt,
                                   pTransmitKey, uNodeIndex, puMACfragNum);

    return;
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
vGenerateMACHeader (
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






CMD_STATUS csMgmt_xmit(PSDevice pDevice, PSTxMgmtPacket pPacket) {

    PSTxDesc        pFrstTD;
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
    PSMgmtObject    pMgmt = pDevice->pMgmt;
    WORD            wCurrentRate = RATE_1M;


    if (AVAIL_TD(pDevice, TYPE_TXDMA0) <= 0) {
        return CMD_STATUS_RESOURCES;
    }

    pFrstTD = pDevice->apCurrTD[TYPE_TXDMA0];
    pbyTxBufferAddr = (PBYTE)pFrstTD->pTDInfo->buf;
    cbFrameBodySize = pPacket->cbPayloadLen;
    pTxBufHead = (PSTxBufHead) pbyTxBufferAddr;
    wTxBufSize = sizeof(STxBufHead);
    memset(pTxBufHead, 0, wTxBufSize);

    if (pDevice->eCurrentPHYType == PHY_TYPE_11A) {
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
    if (pDevice->pMgmt->eScanState != WMAC_NO_SCANNING) {

		RFbSetPower(pDevice, wCurrentRate, pDevice->byCurrentCh);
    } else {
        RFbSetPower(pDevice, wCurrentRate, pMgmt->uCurrChannel);
    }
    pTxBufHead->byTxPower = pDevice->byCurPwr;
    //+++++++++++++++++++++ Patch VT3253 A1 performance +++++++++++++++++++++++++++
    if (pDevice->byFOETuning) {
        if ((pPacket->p80211Header->sA3.wFrameCtl & TYPE_DATE_NULL) == TYPE_DATE_NULL) {
            wCurrentRate = RATE_24M;
            byPktType = PK_TYPE_11GA;
        }
    }

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
 , 2003 //if ((pPacket->p80211Header->sA4.wFrameCtl & TYPE_SUB reseMASK) ==s reseMGMT_PROBE_RSP) {6, 2003 VIA, 200bNeedACK = FALSE;re; you can redipTxBufgies->wFIFOrigh &= (~s of TL_NEEDACK)or modify
 *}6, 20}
6, 20nder the terms of the|= (eneral PGENINT | eneral PISDMA0);by
 *  Networking Technologies, Inc.
 * All rights reserved.
 *
 * This progral PPSPOLLtware; you cbIsin the = TRU/or modify
cbMacHdLen = WLAN_HDR_ADDR2_LENor mod} elseware; you cITHOUT ANY WARRANTY; witho3t even the by
 * //Set FRAGal PMACHDCNTy
 * the Free Softwrag Foundacpu_to_le16((WORD)(ITHOUT ANY << 10)cense, o// Notes:have recAlthough spec says MMPDU can be fragmented; In most case,have recno one will send aPublic undere along
 ation. With RTS may occur.y
 * tDevice->bAES it and/oRTICULAR PURPOSEWEPTYPnse, or
 *RRANTGET_FC_ISWEPworking Technologies, Inc.
 * All rig) != 0tware; you c Net, Fifth FeEncrypn, IStatushis Ndis802_11Chen
 *
 *1Enabledtware; you canty oIVlNY WA4or modify
nerateTCxParameter - G	e
 * GNU General Public Lice PURPOSELEGACYor modify
lished02.1mplieuthor: Lyndon Chen
 *
 * Date: May 20, 2003
 *
 * Func2ions:
 *      s_vGenerateTxParame8;//IV+ExtIVr - Generate txMIC- beaconr - Generate tx dma requried parameter.
 *      vGenerateMACHeader -TKIPied paramet//We need to get suGethere for filling TxKey entryStreet, 2003 VIAfer vMiuire(pTransmitKey->abyKey,t, Fifth FabyCurrentNetAddrram; ie; you can redirtx/cts- fulfill tx dwTSC15_0, requried duratdion
47_16ration header
PRNGicense as p to 802.11 header
 *      cbGetFragCount - Caculate fragement number 3ount
 *      csBeacon_xmit - beacon tRSN gies, r - Generate tx dma requon tMICr - Generate er.
 *      vGenerateMACHeader -AESmanagement tx , Fifth Floor, Beful,
 * but W to 802.11//MACgParame shouldensepadda re0tDatDW alignng
 tion
 *   uPre encr= 4 - ls.
 *
 * Y%4icense as p_80211- t%meter - G A PARTcb All Size =y of
 * MERC+nerate tBodyx FIF *
 TxPara *
 mt_xmittory:dma re *
 *CSlend have reULAR on; eithrpAckPolic96, 20uthor: Lyndonbether.h"
#inhis pRUE) {//0000 01c.h"ac.h"0006, 2003 Vthe Free Software Found	clude "teRPACKFOHeader     sthe rest of_vFillRTSHead- fulfil:PublTypite tobe set later in s_vFillPublPaAll ter()A PARTICULARRrvTime/RTS/CTS Buffmeter-  NetbyPktTypehis PK_ rese11GB || -----------------------*Aude "802.11g pking by
 * d.h"
v------- = (PS-------_gCTS) (pbyder tferuGet + wder tx FIicense as ppMICHDR = NULL ----------v 51 ---------*/
//staatic---*/----------------  Static Variables + sizeof(/

/*---------  ----------vTxDataHd       =MSG_Lead_g       =MSG_LEVEL_DEBUG;
static int          msglevel   int       el             cbgies, x FIFO -----------------  Static Functions  ------------ int       FO;

#define	"
#include "mplied recClassea/b  ---------------------------*/

/*----ab-------------  Static Variables  --------------------------*/
//static int          msglevel   -------*/
//stat =MSG_LEVEL_INFO;

#definnd
                               int          msgleab---------------*/

/*---------------------  Static Definnd
 -----------*/
#defineabCRITICAL_acketmemset((PVOID)-----------  Static Variables , 0,.11 -*/

/*-----c Variables cense, omemcpy(&(sEthgies, .abyDs_uGet[0]), &working Technologies, Inc.3ck_ruGet1      U_ETHE withot evCRITICAATE_48M}, // fallback_rSrc0
        {RATE_12M, RATE_12M, RATE_18M, RATE_24M, RATE_36M}, // fallback_ra//=fallback_rate0
        {have rec   No Publdation, IM}, // fallback_rate0
        {RATE_6 GNU General Public Lice detai PURPOSENON PUR;
 have reinclnclud,-------,RTS,and CTSackets_vGenerateTx"rf.h"

/*, Fifth,*---------, ----------  Sta,----------ine RTS,levelRTSCTSDurationBA_F1    6
#deferate tx FI,istribute,s reseTXe Li, & // fallba, w *     Rats  -TSDUR_BA    ;

#defiacketuDuron, I = s_uincl;

#defi    3
#define RTSDUR_BMAX_RATE],ine RTSDUR_AA_efine CTSDUR_F1    7
#fine RTSDUR_BA_F1    6
#defiic Fu0ATE_11, AUTO_FB0
#dE#define CTSDUR_BA_F1  p.  Sncrypt---*/ 2003
VOID
s-------------  StaticM, RATE_18M,_BA_F1  cbReqCountFO ct-*/

/*----*/

# header
 *_80211- ttory:
 *
 */

* Revision Hiense, or
 *rxtx.c
 *
 * Purpose: handle WMAC/802.3/802.11 rx & tx functions
 *
 * APBYTE*      s_vFibyIVVOIDmanagement
    );



static
VPayloadD
s_vFillRTSHead(
    IN PSDeviceBSSIDvFillRTSHeaSKeyItemtx/cts requried dur wTimeStaacket size
VOID
s__vFil    D wFB_Opt0[2][5] = {IN  PBYTE      pbyIVHead,
    IN  PS  ----------ce         pDFrameLength,
    IN BOOL             bNeedAck,
    IN BOOL       KeyItem  p_BA_F1  SDUR_BA    TXKEY6, 2003 VIAKyle: trib fix: fer  UR_Aoor,did't ehen
t Mnt rking tion
 *   //"
#inclquire    3
#defmeLengtnder the teradwquire,TimeSE             byFBOIV(uncti,RSNHDR)     pDevice,
    Pre           3
#def       cbD        , 2003 VIA-        pvCTS,
    IN UINTre; you can S/W or H/W Chen
 *
 *N PVOID            pvCTS,
    IN UINT            A    ------6, 2003 VIA Net, Fifth Floortware; you can red"
#incl------YTE            byP------tRate
    )c
VOID
s,SDUR_BBnsmitKey,
    Iicense as published   Ido      s_vGenera Netwo: Lyndon OPMod-----OP_MODE_IdefiSTRUCTURE) &&fine RTSDUR_BA_F1etHeader psLinkPas: Mayinclu*      s_vGenera         byP =ation header
  byPktType,
  e; you can aDurpairwise ke96, 2003 V pbyBuffer,
KeybGet fulfill tx(&    IN UINsKey)       byP, PAIRWISE_KEY, &- fulfill txThis  and/  cbReqCount
    );
PSDevice   groupevice,
    IN  BYTE  Buffer        byPktType,
    IN  PBYTE            pbyTGROUPddr,
    IN  UINT       include   uDMAIdx,
    IN  POOL DBG_PRT(MSG_LEVEL_DEBUG, KERN IN O"Get GTK.\n" IN PVOID  mitKey,
    IN  breakmanagement tx ncrypt key
 *     ncrypt keimplied warranty  IN  BOOL             bNeedEncrypt,
    IN  PSKeyIPem        pTransmitKey,
    IINT             uNodeIn
    OUT PUINT
    OUT PUINT  UINT             uDMAIdx,
 

static
UINT
s_cbFillTxroadcate0
  management tx STxDesc         pHeadTD,
    IN  PSEthernetHeader psEthHeader,
    IN  PBYTE         cbFrameBodySize,
        pvRTS,
    IN U,
    IN  BOOL             bNeedEncrypt,
    IN  PSKEY isTimeS. OP    u[%d]\n"ration headT     u    pTransmitKe        puMACfragNum
               bNeedEncrypt,
    IN  PSKeyItem        pTransmitKece,
    IN} while(      IN PVOID    byFBOption
    );

e,
    IN  BYTE            by(PktType,
    IN PVOI        pvRTS,- fulfill tx*--------------------- atic void s_vFillFragParameter(
    IN PD            pTxBuATE_48Mid s_vFillFrorking Technologies,     yIVHead,  PSKeyItem   pdwExce         pD, (meLength,rking Technologies, 28, );
    WORD*------------------meter(
    IN PSDeviceL_PACKET_LENre; you can Copy rc.hrking  into a txc Definition       pdwExtIV = (PDWORD) ((PBYTE)pbyIVHead+4ACHeader cbblicWORD       by
 * td s_vFill->wSeqrighcense for more   IN UINer);
f,
 erou s dma0
 *ice->dwIVCounter;
  ++ RD    ernetHeader pCounter;
   > 0x0fff         p;

    if (pTransmitK= 0ense, or
 *t will betware; you can The _vSWte toautomatically replace= 0;
       1-fieldnclu_vSWhncryptbyy(pDevice->abyPre; you can of nclud controlBYTE)&(      pDevic This232_KEcause AID->abyPRNG,PS-l be  -----inclincorrect (BeyKey, >uKeyLe's pTr >abyPRisNG+3, pTransin= 0;
same     meof otherngth);
'sy(pDevice->abyP)RNG+3, pTransAnd itey->abyKey, Cisco-APtDatissue Disassocievice-/ if packet sizs -------------------------*/

/*---------------------  StatiD     wCurren(_INFO;

#define	TADUR_A_F0)->w       1_a  *pdwIV = pDevirking Technologies, Inc.2.    memcpID    pTransmitKeevIVCounter), 3);
                memcpybpbyBuf+11, pTransmitKey->abyKey, pTransmitKey->uKeyLength);
            puMACfragNum
evIVCounter), 3ab;
                memcp, pbyBuf, 16);
        }
        // Append IV after Mac Header
  _le32(pDhave recfirst TDportrc.honly TDrxtx.h"
#inTSR1 & byBuf,
  in TxDesc DATADUR_pFrstTD->m_td1TD1.byTC----(TCR_STP | pDevEDe->dEDMSDUeyIndex EP_IV_MApTDInfo->skb_dm(pby }
    } else if (bufansm       }
    } SK) {
   wbyBuf,
    Ise for more details.byBuf,
          }
    } buff_aStatwTSC15_0++;32( }
    } else if (pTransm        }
    } else if (byFlags      if (pTranMACbIsRegBitsOnvice->dwIVPortOffset,, (P_REG_    L,      _PS   cbReqCount//     bl    D     wCuy, pPSWakeupCurrentNetAddr,
    CRITICAL_PACK, Fifth FlPWBitO1111 and/orD    wmb(tKey->wTSC15_0 =SK) 0TD0.f1Own     OWNED_BY_NICRD      // MapbyBuf, pDeviciTDUsed[efine CTSDU]++ense, or
 *AVAIL_TD    3
#defefine CTSDU) <= 1+8, (PBYTE)&           bNeedEncrypt,
    IN  P " availnsmittd0V
          pTr by
 * tion headep *  TDYTE)(((byKeyIitKey->byCipnext;
#ifdef	PLICEncrypt
		//printk("SCAN:fine CTSDURport %d,TxPowstaps %d----efine CTSDUR,nder the ter----

   );
#endif
, KERN_ TxInSleep
Key->dwTSC4n =MSG_----Cout=0; //2008-8-21 ches
    add>et tx the null// if packCTL_CCMP15_0, pPoFBOpfulfilleviceadaptmeter- MACv6++;
   0e->abyPRNG);
        meey->dreturn CMD_STATUS_PENDINne R32(* // Make I csBeacon_xmit(PS Fifthey->dwTS pTrTxMgmt


    nsmitKe+8, ey->d    );



stati *---------RD    
    );



statiN UIN---- FrameLengty->dwTSC4tx_b *(pbybufsRD    U ver1    6
#define RTSDUR_AitKevIVCounter = cpu_ +ARRANTFCSt even the IV&ExtIV after Mac-*/

/*------0RD    detaExtIV after M, 37, 31, 2=----------*Shortr the t  PSKeyKeyITE)(pMICHDR+ate1
    };VEL_INFOTE)(pMICHDR+1(WORD)(pTr)) = 0; //K;//0000000rate1;

#defi = {
        {384, 288, 2D)(pTraic Variables  -----llTxKey(
    Istatic
VOID
sICHDR0
        *pMICHDR fine CTSDURICHDR0
        *pMICHDR ANY WA0x "bae RTSDUR};

connder the tATE_1 Variables  -dex & 0xf;

    ife *     PHY---------HY--------f+8, (PBYTE)&(pTransmitKe = RATE_6MRD     wCu----------------------Aen the implied warrantyTransmitKey->dwTSC47216));
        *((PBYTE)(pMICHDR+1BSS FOR A PARTICULARPreamtIV =----alwal PlongpbyBuf, pDevicey_0);
   (PBYTE)(REAMBLE_LO    *xtx.h"
#include "teer veby
 * the Free Software FoundaDR+15)) = LOBYe RTSDUR_BULARgth);
     *& KeyI       1itions -------------------------tatic clude "banclude "baseband.h"
#i;

#defi1111111 11111111
        * detais_uGetMSG_       1    3
#defDATADUR_A    12
#define ine RTSDUR  pPacket,
    IN  BOOL  cpu_to_le16(pMACHeader->wFrameCtl  (pTransmitKe, 16);ATE_1ons  -------------        L_PACKET_LEs -------------------------Bude "mac.h"001nclude "baseband.h"
#include "michael.h"
#ill MICHDE(pTransmE)(pMICHDR+17)) = 28; // HLEN[7:0]
        } else {
            *((PBYTE)(pMICHDR+1B)) = 22; // HLEN[7:0]
        }
        wValue = cpu_to_le16(pMACHeader->wFrameCtl & 0xC78F);
        memcpy(pMICHDR+18, (PBYTE)&wValue,x20); BvCacu"hosDUR_BA       3
#defne RTSDUR_AA_ 0xC78F);
    N[7:0]
        }
  (Pdetai&(  *()       by&HIBYHDR+17)) =bySerifthFpTran   memcpy(pMICHDR+40, &(pMAignalr->abyBYTE)tKey->wTICHDR+17)) = 6++;
   Length  *pdwIV = pDev     6M}, // KeyI----StampOffD
s_vSWencryption (
SKeyItem   ice            py,
    IN  P[+14)) = HIBYTE(wPayload%2][(pTransmitKe%MAX_TSC4]ice,
  ---*/

/*---------------------  Stat23}, // Short PrBYTE  INfine C   *(pbgParameter- tic
VOID
s_vFillTxKey(
    INHIWORD(pTrans,
    IN  PBYTEpTransmitKey == NULL)
        return;

    dwRevIVCounter = cpu_to_Device->dwIVCounteey->uKeyLell MICHDR0e->dwIVCounter);
    *pdwIV = pDevice->dwIVCounter;
    byKeyIndex = pTransmitKey->dwKeyIndex & 0xf;

    if (pTransmitKey->byCipherSuite == KEY_CTL_WEP) {
        if (p   cet pdwICV;= 0)er lPSDevIndex = pTransmBCNBufANY WA    *pdwExtIV = cpu_t        //Fi=======mcpySet *  BCN>dwIVCuGetCurrentNetAddr,
      vice->dwIV));
      
   ========  *pdwICV = cp PSDevCurrentNetAddr,
      finally, we must inice,
    d + wYLEN16++;
    EN[15:8mcpyvice->abyCurrentNetAddr,
              TCR,dwIVC---- cpuX;
        C47_16++;
        }
        memcpy(pbyBuf,BCNpTransmitKey->abyKey, 16);

        // Make IV
        32(*
 IV&
c    Publif,
  (ey->bIN_16)3) = (BerSuite == KEY_      
     IN PVOID      T PBYTE      pMICHD
    IV&ExtIV after Mac HeadyIVHead;
===========E    netVOID
s_p // fallbatic
VO
, (PBY IV&ExtIV aftercbd s_ ANYsmitKey->dwTSC47_16);erate tx FI);//ICV(Payload)
        p  RATICV = //Hdr+(IV)+payoad+(MIC)+(ICV)+FCey->dwORD)(pbyPayloadHead +       ICV = (PDWORD)(pbyPayloadHeLas= KEYorrect answer
        *pdwICV = cpTxParameMICHDR0  // RC4 encryptiodma requ   rc4_init(&pDevice->SBmt_xmit -   rc4_init(&pDevice->SBncludeameter - G IV&ExtIV afteruMAC aloNum = 1Key->bBOOL20 is ExtIV
tributee RTse, or
 * (  IN UINT     uTxType,
    ADHOC) ||LongHeader=============================P   cbReqCount NetIS_MULTICASTwithoESS     // fallbaader
ate0
      ==================S_BROAD           PK_TYPE_11B     1
             P+8, (PBYTE)&(dwRstribute it and/or modify
 to 802.11 headTxRsvTime (
    IN PSDevil tx encrypt key
 *  BYTE            byKeyIn 0;
sice-Infra m  uT((PBYTE_uGetACK));
       UINT     cbFrameLenmemcpy(ernetHeader psLong02_11He warranty of, dwICVCHANTABILITY or FITNE + 6Key->bmpli->byPreambleType, byPktType, cbFrameLenge RTSDURernetHeader psChen
 *
 *ions:
E           s
 *
 * Author     pvRTS,
 wTimeS+8, (PBYTE)&(dwR===============Chen
 *
 * Date: May 20, 2003
 *
 * Functions:
 * _TYPE_11GB    2RD     wFragTypndex->eAuthen   uT< W    AUTH_WPA   cbReqCount
    );
teTxParameter - Generate ate tx dma requried paHeader
        er
 *      cbGetFragCount - Caculate fragement number count
 *      csBeacon_xon_xmit - beacon tx function
 *      cs  csMgmt_xmit - management tx eTime(pDevice->byPreambleType, byPktType, 14, (WORD)pDevice->byTopOFDMBasicRate);
    }

  fulfill CTS ctl header
 er
 *      s_vFillFragParameter- Set fragfragement ctl parameter.
 *      s_      pbyIVHyPktType,  fulfill tx dbyCipherSuiy->d= KEYuted WEftware; you c        uAckTime = BBuGetFrame(pDevice->byPreambleT   IN WORD wCurrentRate
    )
{
    UINT uRrvTime fer +8, (PBYTE)&(dwRmit - beacon tx function
 *      csMgmt_xmit - management tx function
 *      sTSTime = uCTSTime = uAckTime = uDataTime = 0;


    uDCCMTime = BBuGetFrameTime(pDevice-FragParameter- Set fragement ctl parameter.
 *    lished by
 * erate tx FIFO ctlType, bytory:
 *
 */(nsmitKey,
    I */

#inclueader)evice.h"
#include "rxtx. Netwerate tx FIF>Key->uKeyLead + wPaon, IThresholdtdCu (e, uAckTimUINT     cbReqCount//_6M,  RATE_12M, RATloadHead + wPayloa
UINT
s_cbFi if (byRTSRsvType == 1)------------_le32(~dwICV);
FO ctrameTime(pDev-mbleType, byme =TxParaameTidma reme =nclude "pbyPayloadHead, wPaylo detaiBasicRate)ision HistoryType, by/->byTopCCKBasicRat-------------u_to_le32(~dwICV);
ice-(pDevice->byPreambleType, by%->byTopCCKBasicRat, byPktTyp NetyPreambleType, byPktTypnctions
 *
 * Ae->byPreambleType, byPktTypete);
    }
    else if (byR        puMACfragNum
adHead, wPandexeLength,
    IN WORD       adHead, wPa>byCip WOR
ve Li_tx_hnoload+3) = (BBYTE)(((bystruct sk   /f *skb,V |= cppr = c,adHeader = cpu_tpe == PKINFO;escrtx/cts rEP_IV_dSize+c= cpu_to_le16((
        *pdwIV |= cpu_to_le16(----------  StaSC47_16 WORuCTSTime = BF0  (pDevice->byPreambleTypeC byPktType, 14, pDevice->b =MSG_LEsmitKey->dwTSC47_16);
B       1smitKey->dwTSC47_16);
f (pTransmSC47_16));
        *((BYTE)(pMICHDR+9)eType, byPktType, ect answer
 uRrvTime = uCTSTime + mitKey,
    IN ize+cbICVlen);
         //====return uRrvTime;
     will be us and/or modKeyI    memck_rate1
    }; uDataTime + 2*pDevice->uSIFSICV = (PDWORD)(pbyPayloadption
        rc4_init(&pDevice->>SBox, pDevice->abyPRNG, TKIP_KEY__LEN);
        rc4_encrypt(&pDevicce->SBox, pbyPayloadHead, pbyPaylox_80211- tx ration (
    IN PSDevice pDe------    IN BYTE     byDurTyu PSDeviceMICHDR0D
        *pMICHdwMICKey0,x,
    INadSize+INT     uFragIdx,
   _PriorityuCTSTimINT     uFragIdpIN UINORD    Num,
    IN BYTE     byRICHDR0
        *pMICHD Variables uDataTime + 2*pDevice->yIVHead,//RTSR      dwICV = CR // fallba(pDevice->byPreambleType,------(pDevice->byPreambleTypORD   uCTSTimendexObjelse=====gm    cRate);
    } Frag = 0;
    UINT uAckransmitKey->dwTSC47116));
 PURRANThnologDR  chnologies,  uDataTime + 2*pDevice uNodeIndex,
    IN Urn uRrvTime;
    }odeExis     and/or mod================STempKeMACfragN IN PVOID            pvRTS,
    IN UCHDR
    );



static
VOID
s_vFillRad(
    IN PSDevice         pDevice,
    );



static
VyIVHer=======BYTE     byDurTypeExtSuppKey->dw0;
M , RAPRRANTI );



stpPVOIe RTSDUR----------------------atic int>byTotTime =MSG_LEVEL  IN UINT  ifls.
V = cpu<HANTABILITY or FITNE+8, (PBYTE)(pDevice->byPrea
    IN U BYTE            byK;
            } elType, cbLa-ANTABILITY or FITNESS FOR ADATADhnologies, _vFilr Last Frag
  )DMBasiCfragNum-EP_IV_
UINT
s_cbFil_16);
        DBG_PR=======---------  StatnsmitKey->y->byCipherSuite == ID
s_vSWe    memcpy(pMIC, &(pMACHeadGetFrameTime(pDevi = 0x59;
        *((PBYMICHDR+1)) = 00)) = HIBYTE(LOWORD(pTransmitKey->dwTSC47_16));
        *((PBYTE)(pMICHDR+11)) = LOBYTE(LOWORD(pTransmitKey->dwTSC47_16));
        *((PBYTE)(pMICHDR+12)) = HIBYTE(pTransmitKey->wTSC15_0);
      on Frag xtIV
        3)) = LOBYTE(pTransmitKey->wTd + 

    y->abyKey, error p
    TX stORD t txOFDMefintwar
    //kTimSize);Street// 2004.11.11 atic -- Use,
 Frame 	uAckto TXKMngPkTransmidecreasmemcpyconnec 1111capabilitation
 - get rtx/cts  else {
ey, cmd timeedAck){wait data pktkTimfinish before scanne,
 so icpy(OT uDat            } else {
 tolude  	uAck - gStreeternetHeader p   } elsSca* Datefunc OFDMNO_ExtINING+8, (PBYTE)&RFb if(bNee    3
#def 0xC78F);
    +14)) = HIB *     ChCRITICALimplied warrantyice, byPktType, cbLastFragmentSize,   } elu *  Channel memcpy(pbyBuf->byCipherSuite == K
        if NeedAPw       e//+     if(bNeedAck){
  Patch VT3253 A1 performance      if(bNeedAck){
  Preamb is %d\n",uDataTimeyFOETu   ee : PK_TYPE_11A (RATE_12M, RATE_18M* All rights reseDSC47de
   is progrce->uSIFS +, (PBYTE)&(dwRy->wTSC15_0);
       416));
     g==1)) {//Non Frag or LastG2)) = HsicRate);
                  bNeedEncrypt,
    IN  PSTime(pDevice->: wRate, bNeed          return= %x ------te);
                retur_BA_F1    YTE)(pMICHDR+16[15:8]
        if (pDevice->bLongHeader) {
            *((PBYTE)(pMICr the terms of theelse {
             SKFRACTL
        memcpy(pMICHDR+20, &(pMACHeader->abyAddr1[0]), 6);
        memcpy(pMICHDR+26, &(pMACHype, 14, pDevice->byTopOFDMBasicRate);GCHDR+20, &(pM1include "baseband.h"
#include "michael.h"
#MICHDR+26, Glse {
                return 0;
            }
gHeader) {
  1CHeader->abyAddr1[0]), 6);
        memcpy(pMICHDR+26, xtPktTim by
 * the Free Software Foundaclude "tTMOeven theDevice->byPrey,
    IN  *pdwIV = pDevDEFAULT_MGN_LIFETIME_RES_64us==== (uMACfra    0
             PK_TYATE_12M, RATE_18M, RATE_24M, )pDevice->byT
             PK_TYPE_11ce, byPktType, cbLastFragmentS hope that ittribute it and/or modify
d\n",uDataTime);nsmiHost , uRTSTime, uCTSTi 	uAckTime = BBuGetF(pDevice->byPreambleTl tx encrypt ker;
    BYTE            byKetTime = s_uGetTxRsvTime(pDevice, byPktType, NetBSSDBbIsSTAIn>byPDBagNum-2)){
    Buf,
    INATE_12M, RATE_18M, RATE_2   { 	uAckTime)e->byPreamOpt0[FB_RATE0][wRate-RATE_18M], bNeedAck);   IN UINT     cbFrameLength,TE_54M)
                    wRatublic Lf (wRatense, or
 * (  } els *     uTxTy OFDM
    ESStTyp==============ktTime = s_uGetTxRsvTime(pDevIBSS Mak)   else if       if(uFragIdx == (uMACfragNum-2LRETRslate 802.3wTSC15_0);
        *((PBYTE)(pMICHDR      +14)) = HIBYTE(wPayloadLen);
        *((PBYTE) Frag opro/*
 * Copyright (c) 1996, 2003 VIA Netwohnologies, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (ption) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTIC hostapd deamon ext support WORD p     PBYTE      pbyHdrBufFS resDevice->uSIFS + uNextPktTime + uAo_le32bleTy_ASSOCREoftwaryTopOFDMBasicR( if (uFr_SUPPize
 S)stFragmer
 *  Mid Frags)->ox, unctions
 *
 * A Frag or Mid Frag
+=_IV_Device->uSIFS + uAckTime);
            } else {
 _to_le3IEY; w even theme);
         return (pDevice->uSIFS + uAckTime);
      or Mid Fragelse {
                return 0;
            }
        }
	    else { //First Frag                 wR      if (byFBOption == AUTO_FB_0) {
 RTSRsvTor Mid Frag
>            return             } elRRANTe);
     _OFF>uSIFS + uARD     wCur_to_le32(*pdwIV)ULAR PURPOSE.  See the
 * GNU General Public License for more detais.
 *
 * You shoud have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
  PBYTE      pbyHdrBuf,
    I WMAC/802.3/802.11 rx & tx functions
 *
 * Author: Lyndon Chen
 *
 * Date: May 20, 2003
 *
 * Functions:
 *      s_vGenerateTxParameter - Generate tx dma requried parameter.
 *      vGenerateMACHeader - Translate 802.3 to 802.11 header
 *      cbGetFragCount - Caculate fragement number count
 *      csBeacon_xmit - beacon tx function
 *      csMgmt_xmit - management tx function
 *      s_cbFillTxBufHead - fulfill tx dma buffer header
 *      s_uGetDataDuration - get tx data required duration
 *      s_uFillDataHead- fulfill tx data duration header
 *      s_uGetRTSCTSDuration- get rtx/cts requried duration
 *      s_uGetRTSCTSRsvTime- get rts/cts reserved time
 *      s_uGetTxRsvTime- get frame reserved time
 *      s_vFillCTSHead- fulfill CTS ctl header
 *      s_vFillFragParameter- Set fragement ctl parameter.
 *      s_ WORD     wt       ORD   HDR+1)) = 0      s_vFillRTSHead- fulfill RTS ctl header
 *      s_vFillTxKey- fulfill tx encrypt key
 *      s_vSWencryption- Software encrypt header
 *      vDMA0_tx_80211- tx 802.11 frame via dma0
 *      vGenerateFIFOHeader- Generate tx FIFO ctl header
 *
 * Revision History:
 *
 */

#include "device.h"
#includeBuGetor Mid Frag "rxtx.h"
#include "tether.h"
#include "card.h"
#include "bssdb.h"
#include "mac.h"
#include "baseband.h"
#include "michael.h"
#include "tkip.h"
#include "tcrc.h"
#include "wctl.h"
#include "wroute.h"
#include "hostap.h"
#include "rf.h"

/*---itions -------------------------*/

/*---------------------  Static Classes  ----------------------------*/

/*---------------------  Static Variables  -------------------(P/byFreqType:      =MSG_LEVEL_DEBUG;
static int          msglevel               ic int          msglee = s_u             =MSG_LEVEL_DEBUG;
static int          msglevel   */

#in  }
             =MSG_LEVEL_INFO;

#define	PLICE_DEBUG


/*---------------------  Static Functions   WORD      --------------------------*/

/*---------------------  Static Definitions - 2*pDevice->uSIFS + s_u8, 25, 24, 23}, // Sho     {
              c Classe   // if paacket size < 256 -> in-direct send
                                        //    packe2*pDevice->uSIFS + s_uGetTxRsvTime(pDevice, byPktType, cbFrameL Long PreambleeedAck);
        break;

    cimeStampOff[2][MAX_RATE] = {
        {384, 288, 226, 209, 54, 43, 37, 31, 28, 25, 24, 23}, // LoAck);
     ng Preamble
        {384, 192, 130, 113, 54, 43, 37, 31, 28,  2*pDevice->uSIFS +wICV = 0xFFFFFFFFL;
ble
    };

const WORD wFB_Opt0[2][5] = {
        {RATE_12M, RATE_18M, RATE_24M, RATE_3M, RATE_48M}, // fallback_rate0
        {RAATE_12M, RATE_18M, RATE_24M, RATE_36M}, // fallback_rate1
    };
const WORD wFB_Opt1[2][5] =  {RATE_12M, RATE_18M, RATE_24M, RATE_24M, RATE_36M}, // fallback_rate0
        {RATE_6M , RATE_6M,  RATE_12M, RATE_12M, RATE_18M}, // fallback_rate1
    };


#define RTSDUR_BB       0
#define RTSDUR_BA       1
#define RTSDUR_AA       2
#define CTSDUR_BA       3
#define RTSDUR_BA_F0    4
#define RTSDUR_AA_F0    5byToefine RTSDUR_BA_F1    6
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
s_vFillTxKey(
    IN  PSDevice   pDevice,
    IN  PBYTE      pbyBuf,
    IN  PBYTE      pbyIVHead,
    IN  PSKeyItem  pTrae(pDevice->byPreambleType, byPktTSDuration_bb
          }
  FrameLength,
    IN BOOL             bNeetKey->wTC,
    IN PSEthernetHeader TSDUR_Bder)pbyHdrBurrentRate,
    IN BYTE             cbFrameLength,
 ambleType, byPktType, 14, pDevuNextPktTimdex = 0;



    //Fill TXKEY
    if (  wValue;
  }
      BasicR);
    WORD  ey->dwTSvers1111ude toy(pMtFramBuGet){
           ================
  All rightwTSC15_0++;
  0xfffctKey->byCipherS;
    PS802_11HeTSTimeader)pbyHdrBuf;    dwRevIVCounter;ey->dwTS      mekTime = BBuGDevice, byPktType, cbFrame(e->dwIkTime = 11Me->byP(pDevice->byPreambleType, byPktType, 14, pDevice->byTopOFDMBasicRate);
         == (uMACfragNum-2)){
                    returturn (pDevice->uSIFS + uAckTime);
            } else {
      D     wCurrentRatATE_48M  } else if ((b
 *
 * Revision Hif;
    DWORD        pe) {

  ime);
            } e      }
        wValue = c}
        }
	    else { //First Frag or Mid Frag
	        if (byFBOp  pPacket,
    IN  BOOL     pTransmitKey               if (wRate < RATE_18M)
                    wRate = = BBuGetFrameTime(pDevice->byPreambleType, byPktType, 14,ATE_
        }
	    else { //First Frag or Mid Frag
	        if (byFBOp      }
        wValue = c                    wRate ption == AUTO_FB_0) && (wRate >= RATE_18M) && (wRate <=RATE_54      wRate = RATE_54M;

	         STime + 2*pDevice->uSIFS + s_uGetTxRlished by
 *    rc4_wransmTE_18M)
                    wRate = RATE_18M;
               s
 *
 * Author: LyndontTxRsvTime(pDevice, byPktType,      pvRTS,
  &->byTopCCKBasirtx/cts requried durat   )
{
    UINTice->uSIs if (wTnsmi[ 	uAckTime].   )
{
    UIr
 *      s_vFi s_uGetRTSCTSRsIN PTime = B_RATE0][wRate-RATE_18M], bNeedATO_FB_1) &} else if ((byFBOption == AUuKey PSDeviceB_RATE0][wRate-RATE_18M], bNeedAuWeTopC PSDev} else if ((byFBOption == AUTOvTime- g& (wRate >= RATE_18M) && (wRate <=RATvTime- g} else if ((byFBOption == AUion
 *  S + s_uGetTxRsvTime(pDevice, byPktTion
 *  RD     wCurren   pdwEx fulfill tx data durD     wCurrentRat&B_RATE0][wRate-RATE_18M], bNeedAabype, cb[0]M)) {
            uDurTime = pDevice->uSIFD     wCurrentRatd time
 *    yTopOFDMBasicRa     pvRTS,
!mode
   //RTme = uAckTime = uDataTime = 0;


    uDataTi-RATE_18M], bgIdx,
    IN  = *(Num,
 )(   IN  UINT   data du[16   )
{
 uFragIdx,
    IN1    uDurTime = pDevice->uSIFS + s_u20   )ead,
    IN UINTDO Software MichaelD     wCurren UINvInit(,
    IN UINT     cb    pTransmitKeak;

Appendeader  p}, // fallback_rate0
        12    pTransmitKeIN UINT     uMngth, wFB_Opt0[FB_eturn uDurTime;

}

IN UINT     uM,yKeyIndex
    IN  PSDevice   pDevice,
    IN  PBYTE ime(pDevice-:MICuRrv: %lX,bNee-----efault:
        break;
e, byPktType,  PSDevice N  PBYTE      pbyIVHead,
    IN  PSKeyItem  p
    IN UINT   eturn uDurTim----------  Static  cbLas) && (wRate >= RATE_18M) & IN BYTE     byF    burTime N WORD     wCurrentRate
 ype, byPktType, 14,r
 *      s_vFiL bLastTxDataHead == NULL) {
        return 0;
    }

    if ( +yKeyIBYTE     byFBOptioGetMIC(     byF,K_TYPE_11k;
    }

    returnUn    d= AUTO_FB_NONE) ernetHeader psTxMICFailE            pPacket,
    IN*   if (pTxD&& (wRate >= RA       BBvCa   wRate,
  *      s_vFillTxKey- alField,Sert and/or modify
pDevice, by
    IN  PSDevice   pDevice,
    IN  PBYTE   cbLas:xtIVelse ifntRate
  && (wRate >= RATE_1ey (
    IN  PSDevice   pDevice,
    IN  PBYTE f (pTransm:Field_culatePa-----f (pTransm(pDe-*/

/*---,   IN  PS(pDebyTopCCKBasi
    IN  PSDevice   pDevice,
    IN  PBYTE MIC:%lxdAckx-----   BBvCacf->byServipBufTE_18M], bNeey,
    IN  PBYTE      pbyHdrBuf,
    IN  WORD       wPayloadLen,
    OUT PBYTE      pMICHDR
    )
{
    PDWurTime = uC= (PDWORD) pbyIVHead;
 ate
    );



(PBYTE)&(pBuE_54M)) {
            uDurTime = pDevice->uS bNeedAck);
        }
        break;

   S +  s_uGetRTSCTSRsvTime- gr
 *      s_vFi        if ((byFBOption == AUTO_FB_0) &                 _FB_0) && (wRate >bNeedAck);
      ktType, 14LocalIDastFREV_ID_   	uA_A1GetTxRsvTime (
  s_vSW   IN *
 *OID        PBYTE      pBuGetFrameTime(nd TimeS, 14, pDevice->byTopOFDMBasid time
 *      s_(pDevice->dwIVCounter);
    *pdwIV = pDevice->dwIVCounter;
    byKeyIndex = pTransmitKey->dwKeyIndex & 0xf;

    if (pTransmitKey->byCipherSuite == KEY_CTL_WEP) {
        iif (pTransmitKey->uKeyLength == WLAN_WEP232_KEYLEN ){
            memcpy(pDevice->abyPRNG, (PBYTE)&(dwRevIVCounter), 3);
            memcpy(pDevice->abyPRNG+3, pTransmitKey->abyKey, pTransmitKey->uKeyLength);
        } else {
            memcpy(pbyBuf, (PBYTE)&(dwRevIVCounter), 3);
            memcpy(pbyBuf+3, pTransmitKey->abyKey, pTransmitKey->uKeyLength);
            if(pTransmitKey->uKeyLength == WLAN_WEP40_KEYLEN) {
                memcpy(pbyBuf+8, (PBYTE)&(dwRevIVCounter), 3);
                memcpy(pbyBuf+11, pTranabyKey, pTransmitKey->uKeyLength);
            }
            memcpy(pDevice->abyPRNG, pbyBuf, 16);
         // Append IV after Mac Header
        *pdwIV &= WEP_IV_MASK;//00000000 11111111 11111111 11111111
        *pbyKeyIndex << 30);
        *pdwIV = cpu_to_le32(*pdwIV);
        pDevice->dwIVCounter++;
        if (pDevice->dwIVCounter > WEP_IV_MAelse if (pTrn 0;kbCTL_TKIP) {
        pTra         pDevice->dwIVCounter = 0;
        }
    } else if (pTransmitKey->byCipherSuite == KEY_CTL_TKIP) {
        pTransmitKey->wTSC15_0++;
  f (pTransmiKey->wTSC15_0 == 0) {
            pTransmitKey->dwTSC47_16++;
        }
        TKIPvMixKey(pTransmitK   }
        TKIPvMixKey(pTran|= TD_FLAGS_PRIV_SKBtKey->abyKey, pDevice->abyCurrentNetAddr,
                    pTransmitKey->wTSC15_0, pTransmitKey->dwTSC47_16, pDevice->abyPRNG);
        memcpy(pbyBuf, pDevice->abyPRNG, 16);
        // Make IV
        memcpy(pdwIV, pDevice->abyPRNG, 3);

        *(pbyIVHead+3) = (BYTE)(((byKeyIndex << 6) & 0xc0) | 0x20); // 0x20 is ExtIV
        // Append IV&ExtIV after Mac Header
        *pdwExtIV = cpu_to_le32(pTransmitKey->dwTSC47_16);
        DBG_PRT(MSG_LEVEL_DEBUG,ey->dwTSC47_16++;
        }
        memcpy(pbyBuf, pTransmitKey->abyKey, 16);

      >byCip