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
 , 2003 //if ((pPacket->p80211Header->sA4.wFrameCtl & TYPE_SUB reseMASK) ==sd.
 *
GMT_PROBE_RSP) {6 2003 VVIA 2003bNeedACK = FALSE;re; you can redipTxBufgies->wFIFOrigh &= (~s of TL_NEEDACK)or modify
 *}re; y}
re; ynder the termenerathe|= (eneral PGENINT | ion; eitISDMA0);b pub  Networking Technolohe t, Inc.e, oAll  thet progrrveder ve, oThis prog; eitPSPOLLtwar modify
bIsinFree = TRU/cense as pcbMacHdLen = WLAN_HDR_ADDR2_LENcense } elsehope that iITHOUT ANY WARRANTY; witho3t evell be nse, o//Set FRAG eitMACHDCNTse, oree Free Softwrag Foundacpu_to_le16((WORD)( of
 * MERC<< 10)cense, o// Notes:have recAlthough spec says MMPDUy
 * be fragmented; In most case,a copy ono one will send aPublic uthe e along
 ation. With RTS may occur.he
 * Device->bAES it and/oRTICULAR PURPOSEWEPTYP have r
 *NTABIGET_FC_ISWEP (at your option) any later version.
) != 0 hope that ir
 *, Fifth FeEncrypn, IStatus disNdis802_11Chenogram 1Enabled hope that ianty oIVlERCHA4cense as pon; teTCxPaAll ter - G	ee, oGNU Gion; eitee SofLice0-1301 ULEGACYcense as plished02.1mplieuthor: Lyndon  *
 * Func Date: May 20 2003 ogram iFunc2ions:e, or    s_v*     teTma requ8;//IV+ExtIVed pacon_xmi txMIC- beaconon
 *      csMg dma requried p requrieer vefHeadeacon_xmi.  Sies,  -TKIP     s_cbFi//We need to get suGethre Ffor fill yourxKey entryStrehor: you canfer vMiuire(pTransmitKey->abyKey,or: LyndonabyCurrentNetAddrram; i modify
 * it urtx/cts- ful dat funcwTSC15_0,on
 *     duratdion
47_16ron, I hies, 
PRNGild ha as ptDat82.111/cts resBufHead cbGetFragCount - Cacul  cs aloeng
  number 3ved me- get frsBt - m_xmi timit - m tRSN ) any anagement tx function
 *FillMICon
 *      csllTxBufHead - fulfill tx dma bufAESman_vFillCTtx r: Lyndonloor, Beful,me- but W    s_uGet//MACga requ shouldd hapaddion
0tDatDW aligndatin, Ime- geuPre encr= 4 - lsrogram iY%4d time
 *  _hnolo- t%quried pa A PARTcbersioSize =y ofme- MERC+     csMBodyx FIFgramt - begrammt
 *  tory:ction
gram CSlthe a copy 02110on; eithrpAckPolic9re; yer
 *      cbbon - .h"
#in distRUE) {//0000 01c.h"a#inc000re; you c GNU General Pareic Lic	clude "teRPACKFO dma bu   csree rest of_vFillRTS dmarequried:GeneTypi csMobe set     r incsBee "wGeneParsioter()r- GenA 0211RrvTime/RTS/CTS Buffqurie-or
 *byPktType disPK_d.
 *11GB || ---------------------- *Alude s_uGetg pt younse, od"bssv------- = (PS-------_gCTS) (pbyhe Frfertion + whe Frn Hid time
 *  pMICHDR = NULL*----------v 51*---------*/
//staatic     ----------------   Datic Varins:
s + sizeof(/

/*   =MSG_LEV--*/
//statTxDataHdfHead  =MSG_Lead_gVEL_INFO;

#dEVEL_DEBUG;
sL_DEBUintVEL_INF   msglevel  -------  Sttions----------cb) any n HisO*-----------------EVEL_DEBUber n, Is            ---------  StFO;

#define	ssdb.nclude  headdpy oClassea/b --------------------------  S    msgleab       =MSG_LEVEL_DEBUG;
static *---------------------        msgle----------  Static Functionst size >= 256 ->_DEBUG


/*-IN-*/
#definend
-------------, 54, 43, 37, 31, ------  Static Funnd
            in-direct se---------------  Static DeD{384, 28Preamble
     define abCRITICAL_king memset((PVOID)     =MSG_LEVEL_DEBUG;
static , 0,GetT       {384,BUG;
static ld have memcpy(&(sEth) any .abyDs_tion[0]), & (at your option) any late3ck_  Sta1, 54, U_ETHETY or ITNE PreambATE_48M}, // fallbaM, RSrc0288, 226,{Rte1
12M, = {
        {RAT8     {RA24     {RA36  };
const WORD wa//=ATE_36M}, /teOpt1[2][5] a copy o   NovGenedon, I, I_24M, RATE_36M}, /e0
        {R= {
 6er.
 *      vGenerateMACHdetai0-1301 UNON0-13;
"rxtx.h"ICALCAL_P,4, 192,,RTS,and CTSking sBeacon_xmit "rf"bss
/*r: Lynd,msglevel  ,8, 25, 24, EVEL_
#define---ine* 51,unctiRTSCTSDSCTSionBA_F24M, 6, // nt tx funFI,istribute, progrTXe Li, &;
const WO, wBufHeadRat    TSDUR_BA, 54/
#definking uDurE_12M = s_uICAL/
#defin, 543, // Sho* 511    MAX_= {
],
#define DAAA_11
#dene RURF1    67
#1
#define DAT_F1    6
#defii Defi0 {
  1, AUTO_FB0
#dE  11
#deR_A_F1 -------p.  Shen
 t
    rageme WOR
s-----------  Static D_12M, RATE_1/



stacbReqrved FO ctn-direct se-dir#TxRsvTime-  vGenerae "deogram /

* Revisrts/Hid have ime-rxtx.cogram iPurpose: handle WMAC/s_uG3wPayl11 rx &iablfinitionsogram iAPBYTE      csBeFibyIV WORr
 *      288, );


-------
VPayloadD
"
#inclctl.h"
(288, IN PS FifthBSSIDde "wctl.h"SKeyItemx/cts    s_uGetRTSC w----Staking nt   (
    I_
#inc, 54D wFB_Opt0[2][5] = {IN  
    , 54, p
VOI dma, BYTE    PS          //ce, 54, 43,pD All Length BOOL    BOOL, 54, 43, 37,stribucker psEthHeader,
    IN PVOI  p/



staF1    9
#deTXKEYre; you canKyle: 1    fix: Fill efinfulfdid't e*
 *t Mnt at you  vDMA0_tx//RITICALqaHeaA       11rnetHeathe Free SofadwIN  B,   INbNeedAc IN WORDyFBOIV(initi,RSNHDR) IN PSEFifth BOOL 8021  IN PVOID    11---------D  IN PVO*      s_u-    IN PSvCTS BOOL    UINTr modify
 * S/W or H/WGetFragCounN t WOR, 54, 43, 37vCTS,
    IN UINT , 54, 43, 379
#deTSDUR_re; you canuthor: Lyndon ful*      s_vGene it RITICALTSDUR_  bNeedAcTxBufHeaPTSDUR_tRatead(
  c(
    I,ne DATBlfill tx BOOL  d time
 *  ee So 80YTE do     csBeacon_xr
 * (      cbGOPModSDUR_OP_MODE_ItatiSTRUCTURE) &&-----------------et dma bupsLinkPasaculaICAL_      csBeacon_xentRate
     =t rts/cts res
    ------ BOO modify
 * aDurpairwise kencludeou cck,
 Defer,
Keyame equried dur(&   IN UINTsKey      
    , PAIRWISE_KEY, &requried durs disoston   pbyBuf,
 ad(
               group pvRrvTime,       bNeTE    ntRate
    ead (
    L         bNeedAcNeedAck,
TGROUPddr BOOL     UINT       ICAL_PAC  uDMAIdx BOOL      aderDBG_PRT(EBUG


/*------, KERNAIdxO"   bGTK.\n"E          ter(
    IN PN  breakr
 *      s_vFOID
s_ kese, or  odeIndex,
iET_LEN warrneratAIdx,
ader,
    IN WORD    Chen
 t BOOL       IN PPN BY   bNee fulfill tx BOOL  INT             uNodeIndTD,

 * PINT OID    pTxData IN  PBYTE   pe,
   acket,
  N PSDevicDataHs_cbe "wTxroadc18M}, /r
 *      s_vFSTxDes, RA   bNee dmaTD BOOL       En - n   wFragTypEth dma badTD,
    IN  PSEthernetcb All isiox FI BOOL  bNeed0       IN UI  uDMAIdx,
 );


static
UINT
s_uFillDataHead (
    IEY is   IN. OP,
   [%d]\n"et rts/cts T     u pDevice,
    I   uFragIuMAC aloNum288, 226, 209, 5T
s_uFillDataHead (
    IN PIN BYce pDevice,
    I    uDMAId} while(Buf,
    pTransmfHead,
p  vDMA(
    I   uDMAIdx,
    I   pTxBufHea(    pHeadTD,
   pTrabyFBOption,
  requried durmsglevel  --------------> divoidh"
#inclresea requrieN BYTE              bNeeder te1
         pdwIV (at your option) any d+4)
    IN YTE      pbyBupdwExC,
    IN PSE, (rnetHeadeat your option) any 28,    d+4)detapMICHDR
    )
{
   ORD) pbyIVHead;       L_ip.hETt evr modify
 * Copy rc.hat you28, o a tx 37, 31i rts/FBOptioaluetIV----*Ddetai ((IN  P)k,
    IN+4tx dma bucbe SodetatRate
   e
 *      pdwI->wSeq the time
t txmo    IN UINTer);
f,
 erou sncti0
 *fth FdwIVrved er)pby++ _to_lebLastFragmentmitKey->dwK > 0x0ff      pv  p;
pbyHd Netvice,
    = 0N  PBYTE   tite tobeHeader,
    IN The _vSWh"
#iautom-> dally replace= 0)pbyHd   1-fieldCAL_WEP2hillDatbyy(   pvRr dataPr modify
 * of CAL_P controlretur&    IN    pvRis di232_KEcause AIDer), 3RNG,PS-ey->MAIdx,
ICALincorrect (Bea dur >uKeyLe'sevic nsmitKisNG+3,evice,
inemcpysrypttaticeof o cbLagth);
'sevIVCounter), 3)RPBYTE)&(dwReAnd ittx data dur Cisco-APpt hissue DisassociFifth /_CTLpINT      s         //    packet size      {384, 192, 130, 113, 54, 43,to_le3w *    ({
        {384e	TAdefin_F0)->w(pDevice_a  *prans =, pTraat your option) any late2. 3);
 mcp      vice,
    IevnsmitKey-), 3r)pbyHdcpy(pDevice-ATE_48b BYTE +s  -- fulfill tx data dur16);
        }
     m   memc         bNeelTxKey (
       }
         abader
        *ce->abyPR,  BYTE , 16eader
     }der
     // Appthe IV afostaMac  dma b
  _le32(pDa copy ofirst TDport 0;
only TD   pbbssdb.TSR1 & 11
   
  //  UINT  DA
     pFrstTD->m_td1TD1.byTC   )(TCR_STP |, pTrED pTrEDMSDUeyIndex EP_IV_MApTDInfo->skb_dm----= (byKe impli_CTL_bufulfidwIV |= (byKe} * Th{der
wif (pDevi  IdwIV = pDeviUR_BBls.smitKey->wTTL_TKIP) {
   buff_a Dation
 *  ++;32(ey->byCipherSuite vice,
 tKey->wTSC15_0 =herSuite =yFlagsYTE    7_16++;
MACbIsRegBitsOnifth FransPortOffset,, (P_REG_Key-Lad+4); _      pbyBuf,
 //      eLengthPBYTE)&   /PSWakeup *      s_uGet   IN  Preambleip.htHeader psPWBitO1111oston,ry->dwwmb(l tx dion
 *   =* Th0TD0.f1Ow(pTranOWNED_BY_NIC_to_le32// Ma111
       pvRiTDUsed[-----------]++N  PBYTE   AVAIL_Ty->dw    11-----------) <= 1+8    e->aby PSDevice   pDevice,
    IN  PBYTE " availlfilltd0Vder
       py(ppDevicerts/cts rpBufHTDetur(((ta duIll tx dbyCipnext;
#ifdef	PLICFillDat
		//printk("SCAN:----------* pDe %d,TxPowstaps % uTxT-----------*,the Free Sof   )= KEY);
#endif
,
    _ TxInSleep    entNTSC4nFO;

#   )Cout=0; //2008-8-21 chesy->wTadd>e s_vFree null/smitKey->CTL_CCMP *    pPoransquried FifthadaptinitionMACv6++ader
0ter), 3RNGeader
     memitKereturn CMD_STATUS_PENDIN#def32(RTIC Make Itl header
 *  (PS: LyndmitKey->pTraTxMgmt
= KEY_lfill t    mitKed(
    IN PSDev msglevel  _to_lead(
    IN PSDev UINT-----thernetHeaitKey->wTtx_b *----bufs_to_leU ver-----  Stati  12
#definll t   }
      = nse  +ANTABIFCSITNESS FOR IV&ey ==       *pd {
          0_to_leUR_BwTSC47_16);

, 37, 31, 2=0_KEYLEN) {ShpDevFree S    IN G_PR    -------+ate1) {
  ; = {
    TxPriority
1 detailpTr)) emcp //K; "mac.000_18M1DATADUR_A=  pTra2][5] 3841Hea8, 2er->aba                      IKeyN BYTE PSDevice    I-----Opt1[2][5]*------------------*HDR+9)) = LOBYTE(HIWORD(MERCHA0x "badefine D};

conthe Free S {
                   & 0xf== KEY_CTeBufHeadPHY   )
{
  TE)(pMICHDf     // Apped- fulfill t =   {RA6M_to_le3wCu--------------------- ASS FOR        puMACfrag fulfill tx dey->wT7216)eader
     *   retur+2, &(pMACBSS FORer- GenA 0211Pream == Nt senlw eitunda  *(pbyIVHead+ey_0eader
DR+13)) REAMBLE_LO(pMICunter++;
 nclude "tillDepDevice#include "michael.h"
#apMAC5yAddrLOBYdefine DAT0211c Header
  *&
    (pDevice if (ngth == WLAN_WEP40_KEYLEN)  -> dinclude baHDR+15))baseban-----#i/
#definRNG,NG,  = 28;      mR+16)      te0
  ;

#(pDevice0x20); // unter > 9
#de12fter Mac Mac Header  orking      wCurrentRatnse for morep tx dma bermsAll righL_WEP) {
   e       {
  ons -------------        p BYTE      ngth == WLAN_WEP40_KEYLEN) B_PACKElude "b1        *((PBYTE)(pMICCAL_PACKEichaeler++;
ll -----E16++;
  )) = LOBYTE(7yAddr28[0]) HLEN[7:0]*pdwIV |=  TKIPvCHDR+8)) =(pMICHDR+13)) = LOBYTE(pr2[0])2 6);

        //Fill MICder
     wValu->dwpu_to_le16(pMACHeader->wFrameCtl &47_1C78Fey->abyKey, 1E_48M2, &(pMAC    // Appe pMACH,x20); Bve
 *"hos1    9
#dex20); //   12
#define 0F;
        wV0]), 6);
        wV(PUR_BByPRN*(         p&HIBYabyAddr2[0bySerLyndFvice,e->abyPRN_to_le16(40, &pMACignalr dataretur Make IV->abyAddr2[0](pbyBuf, Mac HpbyBuf+11, pTra&wValE_24M, RG_PRPBYTStampOff  pDeSW1- tysmitK (
 IN PVOIEY_CC,
    IN P2(pT,
    IN  UP[+14yAddrpMICTE(w       %2][d- fulfill t%ADUR->wT]vRrvTimN) {
                memcpy(pbyBuf+823 };
coTE)(p Pr   bNeIN------pMICHpb (PDWORD) - BYTE)(pMIC
#incl);
        *NHIdeta16++;
 adTD,
    IN  Pvice,
    IN =------)der
           == KEY_dwR    *pdwExtIV = cpto_ Fifth FransmitKeyIV after MICHDR+26R pTrransmitKey-eader
 yBuf+11, pTran= pTransmitKey->dwKfHeaG_PR     =16);
        }
dwICV = CRC47_16));
      0xC78F);
   G_LEVEL_herSu.h"
== KEY_ansmWEftwader
     ze, dKey-etmitKICV;= 0)er lter;
 = CRCdwGetCrc3BCNBufMERCHA   // Apey == NUnse fbyKeyIndexFi=
     E_48ULAR*  BCN======tione->abyPRNG);
          after pay((PBYTE)(p    
      = // AppCet thenter;
~dwICV);
        // RCfin    , we must invRrvTime,dtic YLEN1(pbyBuf, EN[15:8E_48ounter), ~dwICV);
        // RC wPayloaTCR,ransmPBYTEcpuXader
     Cme- gt(&pDeviV |= (byKeyInd = cpu_t11
   BCN6);
        }
            *(byKeyIndex 0;
   pu_to_le32pdwI
y->d
     GenerpDevi(CV(PaIN_16)3Addr(B)
        pdwIC       rc4N  PSKeyItem    T     bNeedAck-----y->wTS>dwTSC47_16);

  wIV =
    IN;
_init(&p===pbyHdnetsmitKeyp;
const WOBYTE)(
   // =====
        /cb      *(TSC15_0);
          ;ne RTSDUR_A)n txCV(       KEY_CTL_WEp    {->SBox//Hdr+(IV)+payoad+(MIC)+(ICV)+FCmitKeyader->by        dma +=======->SBoxLL)
    wCRC to get tLas pdwI } elseansw cpu_ LOBYTE(ce->SBox, t - beac=======
    RC411- tansmiction
 *_WEPc4_   i(&ICV afterSB#includ     >abyPRNG, TKIP_KEY_LEHDR+15equried pa=====
        /lTxKFounNum = 1ICV(PaentR20 is ey ==
1    7
defi PBYTE    (  IN UINT     uTx pHeadTD,
ADHOC) ||Long dma bpayload
      }
}




/*byPktles  pbyBuf,
 r
 *IS_MULeambSTY or EStKey
    nst WO = cp18M}, // fa   }
}




/*byPktS_BROA          bNPK_ rese11BDevice288, 226, 209,PYTE(LOWORD(pTdwRF1    7
, Boston,ense as pu   s_uGetTxRsvTxRs----- N BYTE          duravice->Sx,
    OUN  PSEthernetHeadwICV =CountafteInfra m====   retue0
  c Lieader
    UINT     
    INLen = cpu_bLastFragmentS====2003
HepuMACfragNof, ce->SCHANTABILITYrameFITNE + 6ICV(Pa hea_LEV_0);
bled (
        pHeae;

    uDagdefine DbLastFragmentS *
 * Funcount
 pbyHdrBuf,
   pMICHDRer
 *BOption,
        INetTxRsvTime (
     }
}




/*byetFragCount - Caculate fragement number 15:8]
 *  _11GA   G 3
*/26));
   reseTyp    ->ePE_1eyAdduT< W=====UTH_WPrt dbFrameBodySize,
  mit - beacried pa      cs tx function
 *      sIV = cpu_-------ime- get frame reserved time
 *      s_vFillCTSHead- culfill CTS ctl header
 er
 *      s_vFill PBYTE    ll CTS ctl ctl ndex
 *     r
 *      s_vFe----vIVCounteref	PLICE_DEBUG
	//printk("s14,  detaireturn uDatTopOFDMBasictatieader
 }   }quried daticctl_cbFillTxype, 14, (W   pdwIV = (PDWORD) - ULAR alo s_vFillCTIN P s_cbFillTxBufHead s=======k,
   /printk("sequried duratayload)
   pbyP pdwIuted WEmichae *
 * AuameLengtAckpe,
 = BBtionrameC return uDataTime;
  ======pu_toE)&(dwRstatic void
RD)(pbIN UIu-------neratetTxRsvTime (
  turn (uDataTime + pDevice->uSIFS + Time);
    }
    else {
  + pDevice->uSIFS sTSTime, uuC(byRTSRsvTAckTime, uuMSG_Time, u0  INLengthCCMTime, uDataTime;

     return uTE byRTSRsvType,
    INE byPktType,
    IN UINT cbce,
   pDevicne RTSDUR_A   INouteG
	//eyItem  pTra(,
    IN BYTE  Trans, 6);
ies, )V aft[0]), 6);
       pbr
 *  uCTSTime = > IV after Mhe cowPaE_12MThresholdtdCu (e, { //RTS uAckTime;
wTSC15_0, _6M,+ wPa
        { get the coORD      uDMAIdx,
 vMixKeyRTStTypyp    p1D wFB_Opt0[2-to_le3~ce->S);
   INime(pDevice->-CE_DEBUG
	//me, t - beme(pDction
me, HDR+15))CRC to get the,Time(pD      c
UINT
s_uy,
    Ise "dime = BB/.4GHZ
sCCKc
UINT
s   )
{
    PDe for mpCCKBasicRatafte return uDataTime;
    }
}

% = BBuGetFrameTime
	//printkr
 *f	PLICE_DEBUG
	//printkTE      pMICHDR uDataTime;
    }
}

//byFres_uGetRTSCT------IPvMixKeyR       *pdwIV &= WEP_e, 14, pDev    netHeader psEthHpu_to_le32(e, 14, pDevLEVEL_ uCT
vTSDU_tx_ptionad+   //==+13))   DBstruct skTYPEf *skb,V | theptIV =,et the==========Type, PK
    esctx/cts  r}
    dx FI+cader->wSeqCtl;N BYTE   // AppenTopOFe for moreBA_F0    4
#defoad)
   uCTType == 0) BF0& 0xeturn uDataTime;
    }C}

//byFreqType:PktType, 1_DEBUG

);//ICV(Payload)
    
 3
*/
  1 = BBuGetFrameTime(pDee, dwICV);oad)
   (PBYTE)(pMICHD+13)) = LOBYTE9)DEBUG
	//printk("sdwICV);
    


    uDsvType == 0+mitKey,
    IN  CTSTxbICVleneader
      //   }       


    uader
  itKey-> usce pDevice,= 0; //  = cTE_18M     memrvTime_bb
 + 2*PktType, uSIFSanswer
        *pdwICV = smitKey,
 L_WEPabyPRNG, TKIP_KEY_>SBoxe);
       smitKey- fer ddr,_t eveader
     2.4Gvice->S, TKIP_KKEY_LEGetDayPktType, 14, pN UINT  x  vGenerax et rts/N BYTE           );
  (PBYTE)&wthHe  bNeedAbyDurTyunter;
   =============OBYTE(HIWOdwMICKey0,,
    IN a//CTST=========reseet,
    _PriorityType ==agmentSize,
   pN UINTu_to_leNumer psEthHe  IN BOOL ey->dwTSC47_16));
     UG;
static  3*pDevice->uSIFS;
    
    IN /-  SbyPktTyce->SBoxCR;
const WOyPktType, 14, pDevice->b
#definyPktType, 14, pDevice->u_to_lType ==     Objambl   }
g     NT
s_uGetRTSCransg      Time = 0;
Ack->wTSC15_0);
      1byTopOFPUNTABIption)DR  TE)pbyIVHead+ 3*pDevice->uSIFS;
     IN PVde,
    IN  UrvTime
    uRrvTi}odeEx     = u pDevice,_TYPE_11GB    2
 TempKl txey (
===============FBOption,
    IN WORD====ad(
    IN PSDevicemitKey == NR IN BYTE           
    IN PSE pvRrvTime,   IN PSDevice
    ========   IN BOOL     bpeExtSuppsmitKey0;
M     PNTABII       }
ppTradefine D--------------------- -> direc4GHZ
tTime +EBUG


/*  IN UINT  if11 fet the <PktType, cbFrameLeng     // AppyPktType, 14, pD= BBuGetFN WORD     wRate,
  ader
        *CHDRntk("s_uLa-ktType, cbFrameLengTransmitunterption) any 
#incr LastMACfr
  )tic
UIKey (
  -}
      uDMAIdx,
  me(pDe                 }
}
-------  Staticlfill tx dV(Payload)
        pitKey SW      = cpu_to_l   }
  x dmaameTime(pDevice->b    x59PBYTE)(pMICHDR+1, &(pMACyAddr20d,
    IN  WLOturn;

    ;//ICV(Payload)
   (PBYTE)(pMICHDR+13)) = LOBYTE(NextPk
            } else {
                return (pDevice->uSIFS + uNextP2d,
    IN  W- fulfill tx dion
 *     	uAckTonMACfra     itKey->bytTime);
        if (((uMACfre co  uRTS }
        error p)) {/TX stu_to s_vtatir Maicha)) {///kTimx FI);tion
 /lTxKe4.11n,
 -> di-- Us   brameC 	 (bLtoOptiMngPk fulfildecreas = cpu= HIec// HLcapabili -> ice--taDursvType =HDR2
       cmd tim      ){wait data pktkTim    sh befDeviscann   bso i0]),OTbNeedTime = s_uGetTxR2
    to      asicR
   tion
 bLastFragmentbyCipherScant - CBYTE tatiNO_ey =NING     // AppeRFb if(striOID        F;
        wVHead,
    IBufHeadCh Preambl       puMACfragvRrv	//printk("s_u    ACfrng
 E    uGetTxRuCV =Ch    l==============(Payload)
        pdD)(pbyPayloatribuP    memce//correc byPktT  retu
  Patch VT3253 A1 performan        f(bNeedAck){
    PLICEen);%d\n",vTime_bb
yFOETice-ee :YPE_11GA   A (  {RATE_12M, RATE_version.
 *
 * ThD    dic vodistribu    retur +TxRsvTime (
  MACfragNum==1)) || ( 4  return (pg==Next{//NbLastFrao
     GATADUR_UINT
s_uGetRTSIN  PSDevice   pDevice,
    IN  PBYTE      return uD: wtati,  pDev   }
           = %xAIdx,
            }
             ----------13)) = LOBYTE(6->SBo //Fill MIze, d Fifth Fl==========WORD)(pbyPay(pMICHDR+13)) = LO Free Software Fou        tType, 14, pSKFRACTL   wValue = cpu_to_le16(e frte);
     r->wabyuGet1         *pdwIV |=asicRate);
     6         asicRate);
        HZ
static
UINT
s_uGG
            1 6);
    abyAddr1[0]), 6);
        memcpy(p        } eG, byPktType, 14, pD   if (((mcpy(pDevic=======     	uAckTim1   return (pDevice->uSIFS + uAckTime);
            } ext    imnsmitKeyloadLen);

        //Finclude "TMONESS FOReturn uDataTi,
    IN  UyBuf+11, pTraDEFAULT_MGN_LIFETIME_RES_64ust(&pD(lTxKey wRatOpt1[2][5]PK_TYPE_11uRTSTime = BRATE_18M, RATE_24GHZ 1=>2.4GHZtatic
UINT
s_uGE_11GA              uNextPktTime = s_uG hope that it IN PSDevice pDevice,
   , byPktType, 1);e {
Hhis , uR(byRTS,se DATAuFragITime, uDataTimyPktType, 14, pDevicecbFrameLength,
         BOOL bL wRate,
   s_uGetT#defGetPktType,
dAck){
          uNexNetBSSDBbIsSTAInDataDB   if 2){
    IN     IN WOce, byPktType, cbLastFrag[5]  cbFrameLe) uDataTime  IN FBR_A_F0][  //D-Type, cb]ATADUR_ ret;   IN UINT     _uGetTxRsvTth,TE_54MKEY_CTL_WE== (uMACfrag  //eneratef (  //D  PBYTE   ====CHDR2BufHead=====tTimepDevicSS----   }
}




/*b (wRa   } else { // (byFBOptiIpTraMak    ambleTypFrameTimeze,
       ptPktTimeate < LRETRs     Paylo   } else {
      ICHDR+13)) = LOBYT], bNeHead,
    IN  WORD     LvTime;
    }
CHDR+13))->uSIFSpro/ram idex n.
 * (c) 19ce,
    INthernewoption) any later version.
 *
 * This program is distributm+ uAfGenes"michaemodify
 * it u  IN PSDevice pDevice,
    *, BotwareFree Software Fouer.
 *      vGenerateMACvice pDevice,
   e > RATE_54M)
                ime)de "te LOBYr,
   2icRate);turn (p   uNextPsmitK) any "hostalse {
          	uAckTime = BBureambleTypdice-te); } else {
  me = uRTSTil tx encrypt k of
 * MERCHANTABILITY or uITNESS FOR        puMACfragNtl header
PktType, cbFrameLengTransmitKey->wT ho   }d x, pon exuratppdwExpu_to{
        bNeedAck,
HdrBufFSh"
#FS;
    retur + uNe if (wRace->uAmbleTyE_DEB_ASSOCRE"michaHZ
static
UINT(bNeeduFr_SUPP     S)Time = sime- gExtIACfrs)->GetDYTE      pMICHDR->uSIFS +      } 
+=    pe, byPktType, 14        ktTime = s_uGetTxR2
    eambleTIELITYTNESS FOR/First Frag or       dAck){
   
	    else { //First Frag0;
        e, byPktType, 14, pD               if (wRate <  (wRate < 	yPreamble{nswer    ACfradx == (uMACfragNum  if(bNeedpTransmitK   p-------_0WORD)e, byPk0;
         >(bLastFrag==1)) {(pTransm s_uGetTxRNTABIFirst Fra_OFF if (wRate 6));
     reambleTypyBuf+1)02110-1301 Uic
Veelseameter.
 *      vGenerateMACpdwIV = pDevi } els frame vi  byhou "rxtx.h"ceived a cex =cRate);
                return (pDeundati*TY or t	uAckTime =;bNeenot, wr.h"
#ikTime + uNextPktTime);
      y late encr51    nklin tion
 * lTxKey- fulfilost}

	MA nolo0-1301 USArogrameTime(pDevice->byPrea->byPrea    wPayloadLen,
    OUT PBYTE      pMICHDRer
 *      cbGetFragCount - Caculate fragement number 4, (WORD)p    csBeacon_xmit - beacme = BBuGetFramfunction
 *      s_cbFillTxBufHead - fulfill tx dma buf  fulf           IN BYTE     byype, 14, (WORD)pDevice->byTopOFDMBasicRate);
    }

    if (bNeedAck) {
      eTime(pDevice->byPreambleType, byPktType, cbFrameLength, wCurrentRate);
    if dx,
    IBuf the requried duratma== 0)erTxRsvTime- get f else MSG_RTSDUR_B
      func    n
 *iretRTSCTSe, cbFrameLengue "wMSG_Lh"
#include durat    Ack){
   bNeedAck);
          efine RTSDUR_B            }   s_uGetRTSCTS
            	uype, 14, tType,
        s       This p     ck);
          PktType,
      fOFDMB uNextPktTime);
        #inclCtl.h"
#include(
    IN PSDevice vice->uSIFS + eambleType, byPktType, 20, pDevice->byTopCCKBasicRFramepu_to_le3wag or Miu_to_l + uNextPktice->uSIFS + ctl.h"
#includel* 51       }
	    }
        bre);
  requried durameLength,
    INe->uSIF   pTransmit-e "michaelmeLengthxRsvTime- get fve LipDev    IN BYTs_uGetT      viaKeyIndexfHead - fulfill = ugies, IragNum-2)){
  = uAckTYTE byPktT    tKey,
    I     em  pTrans, 6);
    dype, 14, pDevice-ataTi0;
        >byTopC(pMICHDR+15)) =ude "bssdb.nclude car1[0]), 6);
    bssdb    uCTSTime =0, &(pyPreambleTypbyAddr1[0]), 6);
        memcpy(pMHDR+15)) kip    uCTSTime = c 0;
ce->byTopCCKwct = uCTSTime + 2wrout 14, pDevice->b){
   14, pDevice->byR_BA    );
 15:8]
        if (pDevice->bLon      {384, 192, 130, 113, 54, 43, 3  256            //    packet size ion_ba
        uCTSTime = BBuGetFrameT                   //    packe(P/byFreqrag :PLICE_DEBUG


/*---------------------  Static Functions3, 37, 31, 28--------  Static Fun   } el, wRate, bNeeDEBUG


/*---------------------  Static FunctionsrType)     wValue =UR_AA:    //RTSDIVCounter), 3);NFO"v------
   msglevel  ----------  Static Definitions - uCTSTime =if packet size < 256 -> in-direct se, 192, 130, 113, 54, 43, 37, 31 case RT>uSIFS;
    retur int_uHIWO5, 2BYTEICV = 0xFF2][5] 288, 226, 209, eTime(pDTYPE_1itKeyINT        < 256 -> in-di else the288, 226, 209, 54, 43, 37, 31, 2;
    }

       king;
        break;

    se { // (byFBOption == AUTO_FB_1);

    u ====Preamblle        f (wRate INT      } ec  IN Uem    BOOADUR_A_F0pMICHDR+8)) = HIBYTE(HIWO } e209, 54, 43 = 0x59;
  case CTSDUR_BA:    LomeTime(pDevme = BBuGetHDR+8)) = HIBYT192, 130, 113 if ((byFBOption == AU);
        break;

e->SBox0xFRsvTimeL;
Rate <=R0)) = Hs= BBuGe,
    IN BOOL     pt1[2][5] = {
        {RATE_18M, RATE_24M, RAT     {RA    };
const WORD w18M}, // fallbacce, byPktType, cbLastFragmentM, RATE_24M, RATE_36M}, /uDataTime Type, cbFrameLength1 BOOL    ][wRate-RATE_18M], bNeedAck);
       te <=RATE_54M)) {
            uD}, // fallback_ra      C47_1    uRTSTime = Bte-RATE_18M], b)) {
            uDurTime = 
#define efine DATvice->by----r Mac Header
YTE)&wVal1me = BBuGetFrameATE)&wVal22; // HLE-----*/

 PVOID       ---------------GHZ 14ce->byPreambleType   if (5urn sicRate);
       xtIV after Mac Header
 8M) && (7ype, 14, pDevice->   if (8      uDurTime = uCT*((PB9me = BBuGunter > vice->byPime = BBuGMICHDR+17)) =Deviice->byPreMICHDR+17   if ( 22; // HLE][wRate-RAT*((PB13OFDMBasicRate);
        uDurTime = uCTSTime +if packet size < 256 -> in-dirAckTime);
           ULL)
                     urn 0;
              bNeedAck,
)
               bNeedAck,
    IN BOOL           IN BYTTra  return uDataTime;
    }
}

//by RTSDUR_B_bbktType, 14,if (thernetHeader psEthHeader,
    IN WORD   ((uMACfrC       wPa   cbLastFragmenine DAT 	uAe->byPre= uAckTim   )
{
    BOOL bLfragNum,
    INetHeader me;
    }
}

//byFreqType:Ack)4, pDevice- CRCdw     evice->e "wOption KEY_CTL_ = pMACHme(pif (wRatc
UINTr)pbyHdrBuf 0x20);wTSlse EN[7ludeto), 6ime;
ataTiATE_18MbyPreambLength, wFB_Opt
 ersion.
 *         pT
 7_16ffc/ICV(Payload)
 me(pDePS 2003
He(byRTS  	uAe->byPrea wRa //============;BYTE)(((b111111 ameLength, wSDuration_ba_f0
        uC(======ameLengt11M uData    bLastFrag = 1;
    }
TopCCKBasicRate);
        HZ
static
UINT
s_uGetRTS     PK         } elseRATE_18M || (bLastFrag==1)) {               if (wRate < RATE_18M)
   ill MICHDR2
        m (PBYTE)&(dwRstatte1
   yCipherSuite (brTime = 0;


    sfme(pDe)
            eWORDevic/First Frag or Mid F        wValue = pMACHeadeif (wRate > RATE_54M)
               0;
         RATE_M;

	              }
        wValue = cgth);
        yth, wRate, bNeed         <E_18M], buFragIdx == (uMACfragNum-2   }GetFrameTime(pDevice->byPrDataTime;
    }
}

//byFreqType    te >= RATE_18M) && (wRate <=RATE_54M)) {
            uDurTime = uCTST      wValue = pMACHeade wFB_Opt0[FB_RATE1][wRate-    if(uFragIdx == (&&Type, cb>dwTSC47ngthe, cbFrameL<=     54ATE1][wRate-RA[wRate-M;
      uDur Device-> break;

    case RTSDUR_BA_>uSIFS + uAck    IN wfulfieLength, wFB_Opt0[FB_RATE1][wRate-RAType, cb0 11111111 111111  else if (wRate > RAT { // (byFBOption == AUTO_FB_1)BOption,
    I& = BBuGetFrameOFDMBasicRate);
      = uDataTime = 0     if sPktTypTe {
[ cbFrameLe]tKeyDataTime =    }
        beturn (pDevice- wPaambleTypte > RATE_54M)
                gIdx =1) &->byPreambleT        if(uFra              te > RATE_54M)
                uWeBuGe+ 2*pD{
            uDurTime = pDeTOse {
    cbFrameLength, wFB_Opt1[FB_RATE1][wRse {
   {
            uDurTime = pDevice->uSse RTSDUR_BA_F0: //RTSDuration_ba_fvice->uSLastFragmenr/11g RC toGetFrameTime(pDevice = BBuGetFrameTim& s_uGetTxRsvTime(pDevice, byPktTab0
     [0]M (pDktType, 14, pB   ragment         if ( = BBuGetFrameTimktTime);
    HZ
static
UINT
BOption,
   !moSIFS +agId= 0) { //RTSTxRrvTime_bb
        uRTSTiime_b)
               IN UIType,= *(n
    )       IN  PBYT(pDevic[16ck);
   ze,
    IN UI IN*((PB cbFrameLength, wFB_Opt0;

    2GHZ ) IN BOOL    INT DOe "michaelM   mem (wRate >= RATUINvIRNG,,
    IN UINT     cb + s_uGetTxRsvTyPrea<< 30)includep(byFBOption == AUTO_FB_1) && 118M;16);
       =============M     meLength, FB_TSRrvTicbFrame;

}

ce pDevice,
  ,wICV = CRCTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTim    return u:MIC


 : %lX,striyPktTefault:e(pDevice->byPre = pDevice->uSI 2*pDevicpe, cbFrameLength, wFB_Opt1[FB_RATE0][wRate-R
    IN UINT   tType,
    IN--------  Static DextPktTpe, cbFrameLength, wFB_Opte,
    IN BOOL F    dbFrameL  uCTSTime me = uAckTime me = BB//byFreqType    }
        bL PktTi =MSG_L if ite == Kice, byPktT            ifCTSRs       +wICV ;

    if (pansmit{
  IC    INbyF,E_11GA   k     if (byFB      U(pTraduFragIdx =NONE)& 0xf;

    ifsTgmt_FailpbyHdrBuf,
     }
        wVon_bze, dwxD, cbFrameLength   uDurBMICH1][wRateDevi(
    IN PSDevice pDealFabyP,Serice pDevice,
   Option == ACTSTime + 2*pDevice->uSIFS + s_uGetTxRsvTime(tPktT:y ==ambleTyAckTime =, cbFrameLength, wFeybyPktType,+ 2*pDevice->uSIFS + s_uGetTxRsvTime, dwICV);:     _ *    Pa = LOB_16++;
  dAckon_ba
    uSIFsmitLedAck BBuGetFramewTransmitLength_a = cpu_to_le16(wLen);
    MIC:%lx    x(PBYTE)&bFramcameLeServipBuf                pbyPayloaATE_18M)
                 N+ 2*pDevice-Time(pDeeLen   IN   pTx================>uSIFS ataTimePDWbFrameLenuCNULL)
     k,
    IN;tionic void  IN LOWORD(pTrBu  if(uvice, byPktType, cbFrameLength, wFB_OpRD         *pdwIV |= (byKeyInd->byPreambl;

 eturn (pDevice->uSIFS +} else if ((byFB   uDurTime          if(uFragIdx == (end IV&ExtIVpTransmktType, cbFrameLeUR_A, cbFrameLeng/byFreqTypLocalIDtTimREV_ID====	uA_A1   } else {
byPkt  IN evice-ime            to_le16(wLenFrameTime(pDevind    INype, cbFrameLength, wFB_Opt1ktTime);
        dAck){
   ========
        // Append ICV after payload
        dwICV = CRCdwGetCrc32Ex(pbyPayloadHead, wPayloadSize, dwICV);//ICV(Payload)
        pdwICV = (PDWORD)(pbyPaylze, dwICV);//ICV(Pfter Mac HtTypRRANTWEPey->abencr k);
        }
  = cpu_ttaDuration (
    RsvTime (
      }
            memcpy(pDevidx, cbLastFragmentSize,YTE)&(dwRe      }
        // Append IV after Mac Header
     CHDR2
        memcpy(==============                                                    pbyBum, byFBOption)); //1: 2.4

            pBuf->wTimeStampOff_aameTime                                    40       vice, byPktType,imeStampOff[pDevietTxRsvTime (
      }
            memcpy(pDevice->abyPRN        f, 16);
         // Append IV after Mac Header
        *Device->byPreadx, cbLastFragmentSize,
 111
        *pdwIV |=dex << 30);
        *pdwIV = cpu_n 2.4GHZ
    &= W}
    } S, 6);
     0// HLEN[7yPktType,
        r(pDevice, dwICV = CRC<< 3uNextPktTime Buf+11,  uCTSTime wFB_Opt0[me(pDevice_le16((WORD)s_uGetDa============NeedAck){
   ransmitKey- >Length, wCdwTSC47_16++    kbansmfer  PK_TYPE_11GRATE      return 0;
smitLength_a =emcpy(pDevicsmitKey->dwTSC47_16++;
  //ICV(Payload)
        pdwICV = BvCaculateParameter(if (((uMACfragNum=E_18M]e, dwICV);/Make IV
       Sizeice, byPktType,GetCrc32Ex(pbyPaload)
  ======================fer vMi;
   vice,
    o_le16(wLen);
            //Ge|= TD_FLAGS_PRIV_SKB    }
        /taDuration Head, pbyPayloadHead, wPayload        pBuf->wTransion
 *      pBuf->wTransmitLength_6((WORD)s_uGetmitKey->abyKey, 1allback
                   wC      *pdwIV |=if (pTransmitKey->b bNeedAcransragIdx, cbLastFragme    h, wFB_Opt1n;

    dw   //==       DBG_PRBYTE)&(p6Idx,0xc0) | 0mcpy(p//DUR_Ben);
     ength
            BB==
        //Append Iter(pDevice, cbto get the coYTE)&(p  pBuf->wTransmitLength_   	uAckTime = BB     bNeedEncrypt,ransmitLength_b = cpu_to_le16(wLen); bNeedAck, uFrag=======================
    } elsLEVEL_