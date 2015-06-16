/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2007, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************
 */

#include "rt_config.h"

#ifdef RALINK_ATE

#ifdef RT30xx
#define ATE_BBP_REG_NUM	168
UCHAR restore_BBP[ATE_BBP_REG_NUM]={0};
#endif // RT30xx //

// 802.11 MAC Header, Type:Data, Length:24bytes
UCHAR TemplateFrame[24] = {0x08,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
		0x00,0xAA,0xBB,0x12,0x34,0x56,0x00,0x11,0x22,0xAA,0xBB,0xCC,0x00,0x00};

extern RTMP_RF_REGS RF2850RegTable[];
extern UCHAR NUM_OF_2850_CHNL;

extern FREQUENCY_ITEM FreqItems3020[];
extern UCHAR NUM_OF_3020_CHNL;




static CHAR CCKRateTable[] = {0, 1, 2, 3, 8, 9, 10, 11, -1}; /* CCK Mode. */
static CHAR OFDMRateTable[] = {0, 1, 2, 3, 4, 5, 6, 7, -1}; /* OFDM Mode. */
static CHAR HTMIXRateTable[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -1}; /* HT Mix Mode. */

static INT TxDmaBusy(
	IN PRTMP_ADAPTER pAd);

static INT RxDmaBusy(
	IN PRTMP_ADAPTER pAd);

static VOID RtmpDmaEnable(
	IN PRTMP_ADAPTER pAd,
	IN INT Enable);

static VOID BbpSoftReset(
	IN PRTMP_ADAPTER pAd);

static VOID RtmpRfIoWrite(
	IN PRTMP_ADAPTER pAd);

static INT ATESetUpFrame(
	IN PRTMP_ADAPTER pAd,
	IN UINT32 TxIdx);

static INT ATETxPwrHandler(
	IN PRTMP_ADAPTER pAd,
	IN char index);

static INT ATECmdHandler(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg);

#ifndef RT30xx
static int CheckMCSValid(
	IN UCHAR Mode,
	IN UCHAR Mcs);
#endif // RT30xx //

#ifdef RT30xx
static int CheckMCSValid(
	IN UCHAR Mode,
	IN UCHAR Mcs,
	IN BOOLEAN bRT2070);
#endif // RT30xx //

#ifdef RTMP_MAC_PCI
static VOID ATEWriteTxWI(
	IN	PRTMP_ADAPTER	pAd,
	IN	PTXWI_STRUC	pOutTxWI,
	IN	BOOLEAN			FRAG,
	IN	BOOLEAN			CFACK,
	IN	BOOLEAN			InsTimestamp,
	IN	BOOLEAN			AMPDU,
	IN	BOOLEAN			Ack,
	IN	BOOLEAN			NSeq,		// HW new a sequence.
	IN	UCHAR			BASize,
	IN	UCHAR			WCID,
	IN	ULONG			Length,
	IN	UCHAR			PID,
	IN	UCHAR			TID,
	IN	UCHAR			TxRate,
	IN	UCHAR			Txopmode,
	IN	BOOLEAN			CfAck,
	IN	HTTRANSMIT_SETTING	*pTransmit);
#endif // RTMP_MAC_PCI //


static VOID SetJapanFilter(
	IN	PRTMP_ADAPTER	pAd);


#ifdef RALINK_28xx_QA
static inline INT	DO_RACFG_CMD_ATE_START(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_STOP(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_RF_WRITE_ALL(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_E2PROM_READ16(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_E2PROM_WRITE16(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_E2PROM_READ_ALL
(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_E2PROM_WRITE_ALL(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_IO_READ(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_IO_WRITE(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_IO_READ_BULK(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_BBP_READ8(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_BBP_WRITE8(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_BBP_READ_ALL(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_GET_NOISE_LEVEL(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_GET_COUNTER(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_CLEAR_COUNTER(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_TX_START(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_GET_TX_STATUS(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_TX_STOP(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_RX_START(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_RX_STOP(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_RX_STOP(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_START_TX_CARRIER(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_START_TX_CONT(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_START_TX_FRAME(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_BW(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_TX_POWER0(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_TX_POWER1(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_FREQ_OFFSET(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_GET_STATISTICS(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_RESET_COUNTER(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SEL_TX_ANTENNA(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SEL_RX_ANTENNA(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_PREAMBLE(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_CHANNEL(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_ADDR1(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_ADDR2(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_ADDR3(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_RATE(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_TX_FRAME_LEN(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_TX_FRAME_COUNT(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_START_RX_FRAME(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_E2PROM_READ_BULK(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_E2PROM_WRITE_BULK(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_IO_WRITE_BULK(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_BBP_READ_BULK(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_BBP_WRITE_BULK(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

#endif // RALINK_28xx_QA //


#ifdef RTMP_MAC_PCI
static INT TxDmaBusy(
	IN PRTMP_ADAPTER pAd)
{
	INT result;
	WPDMA_GLO_CFG_STRUC GloCfg;

	RTMP_IO_READ32(pAd, WPDMA_GLO_CFG, &GloCfg.word);	// disable DMA
	if (GloCfg.field.TxDMABusy)
		result = 1;
	else
		result = 0;

	return result;
}


static INT RxDmaBusy(
	IN PRTMP_ADAPTER pAd)
{
	INT result;
	WPDMA_GLO_CFG_STRUC GloCfg;

	RTMP_IO_READ32(pAd, WPDMA_GLO_CFG, &GloCfg.word);	// disable DMA
	if (GloCfg.field.RxDMABusy)
		result = 1;
	else
		result = 0;

	return result;
}


static VOID RtmpDmaEnable(
	IN PRTMP_ADAPTER pAd,
	IN INT Enable)
{
	BOOLEAN value;
	ULONG WaitCnt;
	WPDMA_GLO_CFG_STRUC GloCfg;

	value = Enable > 0 ? 1 : 0;

	// check DMA is in busy mode.
	WaitCnt = 0;

	while (TxDmaBusy(pAd) || RxDmaBusy(pAd))
	{
		RTMPusecDelay(10);
		if (WaitCnt++ > 100)
			break;
	}

	RTMP_IO_READ32(pAd, WPDMA_GLO_CFG, &GloCfg.word);	// disable DMA
	GloCfg.field.EnableTxDMA = value;
	GloCfg.field.EnableRxDMA = value;
	RTMP_IO_WRITE32(pAd, WPDMA_GLO_CFG, GloCfg.word);	// abort all TX rings
	RTMPusecDelay(5000);

	return;
}
#endif // RTMP_MAC_PCI //




static VOID BbpSoftReset(
	IN PRTMP_ADAPTER pAd)
{
	UCHAR BbpData = 0;

	// Soft reset, set BBP R21 bit0=1->0
	ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R21, &BbpData);
	BbpData |= 0x00000001; //set bit0=1
	ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R21, BbpData);

	ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R21, &BbpData);
	BbpData &= ~(0x00000001); //set bit0=0
	ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R21, BbpData);

	return;
}


static VOID RtmpRfIoWrite(
	IN PRTMP_ADAPTER pAd)
{
	// Set RF value 1's set R3[bit2] = [0]
	RTMP_RF_IO_WRITE32(pAd, pAd->LatchRfRegs.R1);
	RTMP_RF_IO_WRITE32(pAd, pAd->LatchRfRegs.R2);
	RTMP_RF_IO_WRITE32(pAd, (pAd->LatchRfRegs.R3 & (~0x04)));
	RTMP_RF_IO_WRITE32(pAd, pAd->LatchRfRegs.R4);

	RTMPusecDelay(200);

	// Set RF value 2's set R3[bit2] = [1]
	RTMP_RF_IO_WRITE32(pAd, pAd->LatchRfRegs.R1);
	RTMP_RF_IO_WRITE32(pAd, pAd->LatchRfRegs.R2);
	RTMP_RF_IO_WRITE32(pAd, (pAd->LatchRfRegs.R3 | 0x04));
	RTMP_RF_IO_WRITE32(pAd, pAd->LatchRfRegs.R4);

	RTMPusecDelay(200);

	// Set RF value 3's set R3[bit2] = [0]
	RTMP_RF_IO_WRITE32(pAd, pAd->LatchRfRegs.R1);
	RTMP_RF_IO_WRITE32(pAd, pAd->LatchRfRegs.R2);
	RTMP_RF_IO_WRITE32(pAd, (pAd->LatchRfRegs.R3 & (~0x04)));
	RTMP_RF_IO_WRITE32(pAd, pAd->LatchRfRegs.R4);

	return;
}


#ifdef RT30xx
static int CheckMCSValid(
	UCHAR Mode,
	UCHAR Mcs,
	BOOLEAN bRT2070)
#endif // RT30xx //
#ifndef RT30xx
static int CheckMCSValid(
	IN UCHAR Mode,
	IN UCHAR Mcs)
#endif // RT30xx //
{
	INT i;
	PCHAR pRateTab;

	switch (Mode)
	{
		case 0:
			pRateTab = CCKRateTable;
			break;
		case 1:
			pRateTab = OFDMRateTable;
			break;
		case 2:
		case 3:
#ifdef RT30xx
			if (bRT2070)
				pRateTab = OFDMRateTable;
			else
#endif // RT30xx //
			pRateTab = HTMIXRateTable;
			break;
		default:
			ATEDBGPRINT(RT_DEBUG_ERROR, ("unrecognizable Tx Mode %d\n", Mode));
			return -1;
			break;
	}

	i = 0;
	while (pRateTab[i] != -1)
	{
		if (pRateTab[i] == Mcs)
			return 0;
		i++;
	}

	return -1;
}


static INT ATETxPwrHandler(
	IN PRTMP_ADAPTER pAd,
	IN char index)
{
	ULONG R;
	CHAR TxPower;
	UCHAR Bbp94 = 0;
	BOOLEAN bPowerReduce = FALSE;
#ifdef RTMP_RF_RW_SUPPORT
	UCHAR RFValue;
#endif // RTMP_RF_RW_SUPPORT //
#ifdef RALINK_28xx_QA
	if ((pAd->ate.bQATxStart == TRUE) || (pAd->ate.bQARxStart == TRUE))
	{
		/*
			When QA is used for Tx, pAd->ate.TxPower0/1 and real tx power
			are not synchronized.
		*/
		return 0;
	}
	else
#endif // RALINK_28xx_QA //
	{
		TxPower = index == 0 ? pAd->ate.TxPower0 : pAd->ate.TxPower1;

		if (pAd->ate.Channel <= 14)
		{
			if (TxPower > 31)
			{

				// R3, R4 can't large than 31 (0x24), 31 ~ 36 used by BBP 94
				R = 31;
				if (TxPower <= 36)
					Bbp94 = BBPR94_DEFAULT + (UCHAR)(TxPower - 31);
			}
			else if (TxPower < 0)
			{

				// R3, R4 can't less than 0, -1 ~ -6 used by BBP 94
				R = 0;
				if (TxPower >= -6)
					Bbp94 = BBPR94_DEFAULT + TxPower;
			}
			else
			{
				// 0 ~ 31
				R = (ULONG) TxPower;
				Bbp94 = BBPR94_DEFAULT;
			}

			ATEDBGPRINT(RT_DEBUG_TRACE, ("%s (TxPower=%d, R=%ld, BBP_R94=%d)\n", __FUNCTION__, TxPower, R, Bbp94));
		}
		else /* 5.5 GHz */
		{
			if (TxPower > 15)
			{

				// R3, R4 can't large than 15 (0x0F)
				R = 15;
			}
			else if (TxPower < 0)
			{

				// R3, R4 can't less than 0
				// -1 ~ -7
				ASSERT((TxPower >= -7));
				R = (ULONG)(TxPower + 7);
				bPowerReduce = TRUE;
			}
			else
			{
				// 0 ~ 15
				R = (ULONG) TxPower;
			}

			ATEDBGPRINT(RT_DEBUG_TRACE, ("%s (TxPower=%d, R=%lu)\n", __FUNCTION__, TxPower, R));
		}
//2008/09/10:KH adds to support 3070 ATE TX Power tunning real time<--
#ifdef RTMP_RF_RW_SUPPORT
		if (IS_RT30xx(pAd))
		{
			// Set Tx Power
			ATE_RF_IO_READ8_BY_REG_ID(pAd, RF_R12, (PUCHAR)&RFValue);
			RFValue = (RFValue & 0xE0) | TxPower;
			ATE_RF_IO_WRITE8_BY_REG_ID(pAd, RF_R12, (UCHAR)RFValue);
			ATEDBGPRINT(RT_DEBUG_TRACE, ("3070 or 2070:%s (TxPower=%d, RFValue=%x)\n", __FUNCTION__, TxPower, RFValue));
		}
		else
#endif // RTMP_RF_RW_SUPPORT //
		{
		if (pAd->ate.Channel <= 14)
		{
			if (index == 0)
			{
				// shift TX power control to correct RF(R3) register bit position
				R = R << 9;
				R |= (pAd->LatchRfRegs.R3 & 0xffffc1ff);
				pAd->LatchRfRegs.R3 = R;
			}
			else
			{
				// shift TX power control to correct RF(R4) register bit position
				R = R << 6;
				R |= (pAd->LatchRfRegs.R4 & 0xfffff83f);
				pAd->LatchRfRegs.R4 = R;
			}
		}
		else /* 5.5GHz */
		{
			if (bPowerReduce == FALSE)
			{
				if (index == 0)
				{
					// shift TX power control to correct RF(R3) register bit position
					R = (R << 10) | (1 << 9);
					R |= (pAd->LatchRfRegs.R3 & 0xffffc1ff);
					pAd->LatchRfRegs.R3 = R;
				}
				else
				{
					// shift TX power control to correct RF(R4) register bit position
					R = (R << 7) | (1 << 6);
					R |= (pAd->LatchRfRegs.R4 & 0xfffff83f);
					pAd->LatchRfRegs.R4 = R;
				}
			}
			else
			{
				if (index == 0)
				{
					// shift TX power control to correct RF(R3) register bit position
					R = (R << 10);
					R |= (pAd->LatchRfRegs.R3 & 0xffffc1ff);

					/* Clear bit 9 of R3 to reduce 7dB. */
					pAd->LatchRfRegs.R3 = (R & (~(1 << 9)));
				}
				else
				{
					// shift TX power control to correct RF(R4) register bit position
					R = (R << 7);
					R |= (pAd->LatchRfRegs.R4 & 0xfffff83f);

					/* Clear bit 6 of R4 to reduce 7dB. */
					pAd->LatchRfRegs.R4 = (R & (~(1 << 6)));
				}
			}
		}
		RtmpRfIoWrite(pAd);
	}
//2008/09/10:KH adds to support 3070 ATE TX Power tunning real time-->

		return 0;
	}
}


/*
==========================================================================
    Description:
        Set ATE operation mode to
        0. ATESTART  = Start ATE Mode
        1. ATESTOP   = Stop ATE Mode
        2. TXCONT    = Continuous Transmit
        3. TXCARR    = Transmit Carrier
        4. TXFRAME   = Transmit Frames
        5. RXFRAME   = Receive Frames
#ifdef RALINK_28xx_QA
        6. TXSTOP    = Stop Any Type of Transmition
        7. RXSTOP    = Stop Receiving Frames
#endif // RALINK_28xx_QA //
    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
#ifdef RTMP_MAC_PCI
static INT	ATECmdHandler(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	UINT32			Value = 0;
	UCHAR			BbpData;
	UINT32			MacData = 0;
	PTXD_STRUC		pTxD;
	INT				index;
	UINT			i = 0, atemode = 0;
	PRXD_STRUC		pRxD;
	PRTMP_TX_RING	pTxRing = &pAd->TxRing[QID_AC_BE];
	NDIS_STATUS		Status = NDIS_STATUS_SUCCESS;
#ifdef	RT_BIG_ENDIAN
    PTXD_STRUC      pDestTxD;
    TXD_STRUC       TxD;
#endif

	ATEDBGPRINT(RT_DEBUG_TRACE, ("===> ATECmdHandler()\n"));

	ATEAsicSwitchChannel(pAd);

	/* empty function */
	AsicLockChannel(pAd, pAd->ate.Channel);

	RTMPusecDelay(5000);

	// read MAC_SYS_CTRL and backup MAC_SYS_CTRL value.
	RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &MacData);

	// Default value in BBP R22 is 0x0.
	BbpData = 0;

	// clean bit4 to stop continuous Tx production test.
	MacData &= 0xFFFFFFEF;

	// Enter ATE mode and set Tx/Rx Idle
	if (!strcmp(arg, "ATESTART"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: ATESTART\n"));

#if defined(LINUX) || defined(VXWORKS)
		// check if we have removed the firmware
		if (!(ATE_ON(pAd)))
		{
			NICEraseFirmware(pAd);
		}
#endif // defined(LINUX) || defined(VXWORKS) //

		atemode = pAd->ate.Mode;
		pAd->ate.Mode = ATE_START;
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, MacData);

		if (atemode == ATE_TXCARR)
		{
			// No Carrier Test set BBP R22 bit7=0, bit6=0, bit[5~0]=0x0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= 0xFFFFFF00; // clear bit7, bit6, bit[5~0]
		    ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
		}
		else if (atemode == ATE_TXCARRSUPP)
		{
			// No Cont. TX set BBP R22 bit7=0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= ~(1 << 7); // set bit7=0
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

			// No Carrier Suppression set BBP R24 bit0=0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R24, &BbpData);
			BbpData &= 0xFFFFFFFE; // clear bit0
		    ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R24, BbpData);
		}

		/*
			We should free some resource which was allocated
			when ATE_TXFRAME , ATE_STOP, and ATE_TXCONT.
		*/
		else if ((atemode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
			PRTMP_TX_RING pTxRing = &pAd->TxRing[QID_AC_BE];

			if (atemode == ATE_TXCONT)
			{
				// No Cont. TX set BBP R22 bit7=0
				ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
				BbpData &= ~(1 << 7); // set bit7=0
				ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
			}

			// Abort Tx, Rx DMA.
			RtmpDmaEnable(pAd, 0);
			for (i=0; i<TX_RING_SIZE; i++)
			{
				PNDIS_PACKET  pPacket;

#ifndef RT_BIG_ENDIAN
			    pTxD = (PTXD_STRUC)pAd->TxRing[QID_AC_BE].Cell[i].AllocVa;
#else
			pDestTxD = (PTXD_STRUC)pAd->TxRing[QID_AC_BE].Cell[i].AllocVa;
			TxD = *pDestTxD;
			pTxD = &TxD;
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
				pTxD->DMADONE = 0;
				pPacket = pTxRing->Cell[i].pNdisPacket;

				if (pPacket)
				{
					PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_SUCCESS);
				}

				// Always assign pNdisPacket as NULL after clear
				pTxRing->Cell[i].pNdisPacket = NULL;

				pPacket = pTxRing->Cell[i].pNextNdisPacket;

				if (pPacket)
				{
					PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr1, pTxD->SDLen1, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_SUCCESS);
				}

				// Always assign pNextNdisPacket as NULL after clear
				pTxRing->Cell[i].pNextNdisPacket = NULL;
#ifdef RT_BIG_ENDIAN
				RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
				WriteBackToDescriptor((PUCHAR)pDestTxD, (PUCHAR)pTxD, FALSE, TYPE_TXD);
#endif
			}

			// Start Tx, RX DMA
			RtmpDmaEnable(pAd, 1);
		}

		// reset Rx statistics.
		pAd->ate.LastSNR0 = 0;
		pAd->ate.LastSNR1 = 0;
		pAd->ate.LastRssi0 = 0;
		pAd->ate.LastRssi1 = 0;
		pAd->ate.LastRssi2 = 0;
		pAd->ate.AvgRssi0 = 0;
		pAd->ate.AvgRssi1 = 0;
		pAd->ate.AvgRssi2 = 0;
		pAd->ate.AvgRssi0X8 = 0;
		pAd->ate.AvgRssi1X8 = 0;
		pAd->ate.AvgRssi2X8 = 0;
		pAd->ate.NumOfAvgRssiSample = 0;

#ifdef RALINK_28xx_QA
		// Tx frame
		pAd->ate.bQATxStart = FALSE;
		pAd->ate.bQARxStart = FALSE;
		pAd->ate.seq = 0;

		// counters
		pAd->ate.U2M = 0;
		pAd->ate.OtherData = 0;
		pAd->ate.Beacon = 0;
		pAd->ate.OtherCount = 0;
		pAd->ate.TxAc0 = 0;
		pAd->ate.TxAc1 = 0;
		pAd->ate.TxAc2 = 0;
		pAd->ate.TxAc3 = 0;
		/*pAd->ate.TxHCCA = 0;*/
		pAd->ate.TxMgmt = 0;
		pAd->ate.RSSI0 = 0;
		pAd->ate.RSSI1 = 0;
		pAd->ate.RSSI2 = 0;
		pAd->ate.SNR0 = 0;
		pAd->ate.SNR1 = 0;

		// control
		pAd->ate.TxDoneCount = 0;
		// TxStatus : 0 --> task is idle, 1 --> task is running
		pAd->ate.TxStatus = 0;
#endif // RALINK_28xx_QA //

		// Soft reset BBP.
		BbpSoftReset(pAd);


#ifdef CONFIG_STA_SUPPORT
		/* LinkDown() has "AsicDisableSync();" and "RTMP_BBP_IO_R/W8_BY_REG_ID();" inside. */
//      LinkDown(pAd, FALSE);
//		AsicEnableBssSync(pAd);

#if defined(LINUX) || defined(VXWORKS)
		RTMP_OS_NETDEV_STOP_QUEUE(pAd->net_dev);
#endif // defined(LINUX) || defined(VXWORKS) //

		/*
			If we skip "LinkDown()", we should disable protection
			to prevent from sending out RTS or CTS-to-self.
		*/
		ATEDisableAsicProtect(pAd);
		RTMPStationStop(pAd);
#endif // CONFIG_STA_SUPPORT //

		/* Disable Tx */
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= ~(1 << 2);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);

		/* Disable Rx */
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= ~(1 << 3);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
	}
	else if (!strcmp(arg, "ATESTOP"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: ATESTOP\n"));

		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

		// recover the MAC_SYS_CTRL register back
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, MacData);

		// disable Tx, Rx
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= (0xfffffff3);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);

		// abort Tx, RX DMA
		RtmpDmaEnable(pAd, 0);

#ifdef LINUX
		pAd->ate.bFWLoading = TRUE;

		Status = NICLoadFirmware(pAd);

		if (Status != NDIS_STATUS_SUCCESS)
		{
			ATEDBGPRINT(RT_DEBUG_ERROR, ("NICLoadFirmware failed, Status[=0x%08x]\n", Status));
			return FALSE;
		}
#endif // LINUX //
		pAd->ate.Mode = ATE_STOP;

		/*
			Even the firmware has been loaded,
			we still could use ATE_BBP_IO_READ8_BY_REG_ID().
			But this is not suggested.
		*/
		BbpSoftReset(pAd);

		RTMP_ASIC_INTERRUPT_DISABLE(pAd);

		NICInitializeAdapter(pAd, TRUE);

		/*
			Reinitialize Rx Ring before Rx DMA is enabled.
			>>>RxCoherent<<< was gone !
		*/
		for (index = 0; index < RX_RING_SIZE; index++)
		{
			pRxD = (PRXD_STRUC) pAd->RxRing.Cell[index].AllocVa;
			pRxD->DDONE = 0;
		}

		// We should read EEPROM for all cases.
		NICReadEEPROMParameters(pAd, NULL);
		NICInitAsicFromEEPROM(pAd);

		AsicSwitchChannel(pAd, pAd->CommonCfg.Channel, FALSE);

		/* empty function */
		AsicLockChannel(pAd, pAd->CommonCfg.Channel);

		/* clear garbage interrupts */
		RTMP_IO_WRITE32(pAd, INT_SOURCE_CSR, 0xffffffff);
		/* Enable Interrupt */
		RTMP_ASIC_INTERRUPT_ENABLE(pAd);

		/* restore RX_FILTR_CFG */

#ifdef CONFIG_STA_SUPPORT
		/* restore RX_FILTR_CFG due to that QA maybe set it to 0x3 */
		RTMP_IO_WRITE32(pAd, RX_FILTR_CFG, STANORMAL);
#endif // CONFIG_STA_SUPPORT //

		// Enable Tx
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value |= (1 << 2);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);

		// Enable Tx, Rx DMA.
		RtmpDmaEnable(pAd, 1);

		// Enable Rx
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value |= (1 << 3);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);


#ifdef CONFIG_STA_SUPPORT
		RTMPStationStart(pAd);
#endif // CONFIG_STA_SUPPORT //

#if defined(LINUX) || defined(VXWORKS)
		RTMP_OS_NETDEV_START_QUEUE(pAd->net_dev);
#endif // defined(LINUX) || defined(VXWORKS) //
	}
	else if (!strcmp(arg, "TXCARR"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: TXCARR\n"));
		pAd->ate.Mode = ATE_TXCARR;

		// QA has done the following steps if it is used.
		if (pAd->ate.bQATxStart == FALSE)
		{
			// Soft reset BBP.
			BbpSoftReset(pAd);

			// Carrier Test set BBP R22 bit7=1, bit6=1, bit[5~0]=0x01
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= 0xFFFFFF00; //clear bit7, bit6, bit[5~0]
			BbpData |= 0x000000C1; //set bit7=1, bit6=1, bit[5~0]=0x01
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

			// set MAC_SYS_CTRL(0x1004) Continuous Tx Production Test (bit4) = 1
			RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
			Value = Value | 0x00000010;
			RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
		}
	}
	else if (!strcmp(arg, "TXCONT"))
	{
		if (pAd->ate.bQATxStart == TRUE)
		{
			/*
				set MAC_SYS_CTRL(0x1004) bit4(Continuous Tx Production Test)
				and bit2(MAC TX enable) back to zero.
			*/
			RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &MacData);
			MacData &= 0xFFFFFFEB;
			RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, MacData);

			// set BBP R22 bit7=0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= 0xFFFFFF7F; //set bit7=0
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
		}

		/*
			for TxCont mode.
			Step 1: Send 50 packets first then wait for a moment.
			Step 2: Send more 50 packet then start continue mode.
		*/
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: TXCONT\n"));

		// Step 1: send 50 packets first.
		pAd->ate.Mode = ATE_TXCONT;
		pAd->ate.TxCount = 50;

		/* Do it after Tx/Rx DMA is aborted. */
//		pAd->ate.TxDoneCount = 0;

		// Soft reset BBP.
		BbpSoftReset(pAd);

		// Abort Tx, RX DMA.
		RtmpDmaEnable(pAd, 0);

		// Fix can't smooth kick
		{
			RTMP_IO_READ32(pAd, TX_DTX_IDX0 + QID_AC_BE * 0x10,  &pTxRing->TxDmaIdx);
			pTxRing->TxSwFreeIdx = pTxRing->TxDmaIdx;
			pTxRing->TxCpuIdx = pTxRing->TxDmaIdx;
			RTMP_IO_WRITE32(pAd, TX_CTX_IDX0 + QID_AC_BE * 0x10, pTxRing->TxCpuIdx);
		}

		pAd->ate.TxDoneCount = 0;

		/* Only needed if we have to send some normal frames. */
		SetJapanFilter(pAd);

		for (i = 0; (i < TX_RING_SIZE-1) && (i < pAd->ate.TxCount); i++)
		{
			PNDIS_PACKET pPacket;
			UINT32 TxIdx = pTxRing->TxCpuIdx;

#ifndef RT_BIG_ENDIAN
			pTxD = (PTXD_STRUC)pTxRing->Cell[TxIdx].AllocVa;
#else
			pDestTxD = (PTXD_STRUC)pTxRing->Cell[TxIdx].AllocVa;
			TxD = *pDestTxD;
			pTxD = &TxD;
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
			// Clean current cell.
			pPacket = pTxRing->Cell[TxIdx].pNdisPacket;

			if (pPacket)
			{
				PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
				RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_SUCCESS);
			}

			// Always assign pNdisPacket as NULL after clear
			pTxRing->Cell[TxIdx].pNdisPacket = NULL;

			pPacket = pTxRing->Cell[TxIdx].pNextNdisPacket;

			if (pPacket)
			{
				PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr1, pTxD->SDLen1, PCI_DMA_TODEVICE);
				RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_SUCCESS);
			}

			// Always assign pNextNdisPacket as NULL after clear
			pTxRing->Cell[TxIdx].pNextNdisPacket = NULL;

#ifdef RT_BIG_ENDIAN
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
			WriteBackToDescriptor((PUCHAR)pDestTxD, (PUCHAR)pTxD, FALSE, TYPE_TXD);
#endif

			if (ATESetUpFrame(pAd, TxIdx) != 0)
				break;

			INC_RING_INDEX(pTxRing->TxCpuIdx, TX_RING_SIZE);
		}

		// Setup frame format.
		ATESetUpFrame(pAd, pTxRing->TxCpuIdx);

		// Start Tx, RX DMA.
		RtmpDmaEnable(pAd, 1);

		// Enable Tx
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value |= (1 << 2);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);

		// Disable Rx
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= ~(1 << 3);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);

#ifdef RALINK_28xx_QA
		if (pAd->ate.bQATxStart == TRUE)
		{
			pAd->ate.TxStatus = 1;
		}
#endif // RALINK_28xx_QA //

		// kick Tx-Ring
		RTMP_IO_WRITE32(pAd, TX_CTX_IDX0 + QID_AC_BE * RINGREG_DIFF, pAd->TxRing[QID_AC_BE].TxCpuIdx);

		RTMPusecDelay(5000);


		// Step 2: send more 50 packets then start continue mode.
		// Abort Tx, RX DMA.
		RtmpDmaEnable(pAd, 0);

		// Cont. TX set BBP R22 bit7=1
		ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
		BbpData |= 0x00000080; //set bit7=1
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

		pAd->ate.TxCount = 50;

		// Fix can't smooth kick
		{
			RTMP_IO_READ32(pAd, TX_DTX_IDX0 + QID_AC_BE * 0x10,  &pTxRing->TxDmaIdx);
			pTxRing->TxSwFreeIdx = pTxRing->TxDmaIdx;
			pTxRing->TxCpuIdx = pTxRing->TxDmaIdx;
			RTMP_IO_WRITE32(pAd, TX_CTX_IDX0 + QID_AC_BE * 0x10, pTxRing->TxCpuIdx);
		}

		pAd->ate.TxDoneCount = 0;

		SetJapanFilter(pAd);

		for (i = 0; (i < TX_RING_SIZE-1) && (i < pAd->ate.TxCount); i++)
		{
			PNDIS_PACKET pPacket;
			UINT32 TxIdx = pTxRing->TxCpuIdx;

#ifndef RT_BIG_ENDIAN
			pTxD = (PTXD_STRUC)pTxRing->Cell[TxIdx].AllocVa;
#else
			pDestTxD = (PTXD_STRUC)pTxRing->Cell[TxIdx].AllocVa;
			TxD = *pDestTxD;
			pTxD = &TxD;
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
			// clean current cell.
			pPacket = pTxRing->Cell[TxIdx].pNdisPacket;

			if (pPacket)
			{
				PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
				RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_SUCCESS);
			}

			// Always assign pNdisPacket as NULL after clear
			pTxRing->Cell[TxIdx].pNdisPacket = NULL;

			pPacket = pTxRing->Cell[TxIdx].pNextNdisPacket;

			if (pPacket)
			{
				PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr1, pTxD->SDLen1, PCI_DMA_TODEVICE);
				RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_SUCCESS);
			}

			// Always assign pNextNdisPacket as NULL after clear
			pTxRing->Cell[TxIdx].pNextNdisPacket = NULL;

#ifdef RT_BIG_ENDIAN
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
			WriteBackToDescriptor((PUCHAR)pDestTxD, (PUCHAR)pTxD, FALSE, TYPE_TXD);
#endif

			if (ATESetUpFrame(pAd, TxIdx) != 0)
				break;

			INC_RING_INDEX(pTxRing->TxCpuIdx, TX_RING_SIZE);
		}

		ATESetUpFrame(pAd, pTxRing->TxCpuIdx);

		// Start Tx, RX DMA.
		RtmpDmaEnable(pAd, 1);

		// Enable Tx
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value |= (1 << 2);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);

		// Disable Rx
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= ~(1 << 3);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);

#ifdef RALINK_28xx_QA
		if (pAd->ate.bQATxStart == TRUE)
		{
			pAd->ate.TxStatus = 1;
		}
#endif // RALINK_28xx_QA //

		// kick Tx-Ring.
		RTMP_IO_WRITE32(pAd, TX_CTX_IDX0 + QID_AC_BE * RINGREG_DIFF, pAd->TxRing[QID_AC_BE].TxCpuIdx);

		RTMPusecDelay(500);

		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &MacData);
		MacData |= 0x00000010;
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, MacData);
	}
	else if (!strcmp(arg, "TXFRAME"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: TXFRAME(Count=%d)\n", pAd->ate.TxCount));
		pAd->ate.Mode |= ATE_TXFRAME;
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

		// Soft reset BBP.
		BbpSoftReset(pAd);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, MacData);

		// Abort Tx, RX DMA.
		RtmpDmaEnable(pAd, 0);

		// Fix can't smooth kick
		{
			RTMP_IO_READ32(pAd, TX_DTX_IDX0 + QID_AC_BE * 0x10,  &pTxRing->TxDmaIdx);
			pTxRing->TxSwFreeIdx = pTxRing->TxDmaIdx;
			pTxRing->TxCpuIdx = pTxRing->TxDmaIdx;
			RTMP_IO_WRITE32(pAd, TX_CTX_IDX0 + QID_AC_BE * 0x10, pTxRing->TxCpuIdx);
		}

		pAd->ate.TxDoneCount = 0;

		SetJapanFilter(pAd);

		for (i = 0; (i < TX_RING_SIZE-1) && (i < pAd->ate.TxCount); i++)
		{
			PNDIS_PACKET pPacket;
			UINT32 TxIdx = pTxRing->TxCpuIdx;

#ifndef RT_BIG_ENDIAN
			pTxD = (PTXD_STRUC)pTxRing->Cell[TxIdx].AllocVa;
#else
			pDestTxD = (PTXD_STRUC)pTxRing->Cell[TxIdx].AllocVa;
			TxD = *pDestTxD;
			pTxD = &TxD;
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
			// Clean current cell.
			pPacket = pTxRing->Cell[TxIdx].pNdisPacket;

			if (pPacket)
			{
				PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
				RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_SUCCESS);
			}

			// Always assign pNdisPacket as NULL after clear
			pTxRing->Cell[TxIdx].pNdisPacket = NULL;

			pPacket = pTxRing->Cell[TxIdx].pNextNdisPacket;

			if (pPacket)
			{
				PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr1, pTxD->SDLen1, PCI_DMA_TODEVICE);
				RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_SUCCESS);
			}

			// Always assign pNextNdisPacket as NULL after clear
			pTxRing->Cell[TxIdx].pNextNdisPacket = NULL;

#ifdef RT_BIG_ENDIAN
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
			WriteBackToDescriptor((PUCHAR)pDestTxD, (PUCHAR)pTxD, FALSE, TYPE_TXD);
#endif

			if (ATESetUpFrame(pAd, TxIdx) != 0)
				break;

			INC_RING_INDEX(pTxRing->TxCpuIdx, TX_RING_SIZE);

		}

		ATESetUpFrame(pAd, pTxRing->TxCpuIdx);

		// Start Tx, Rx DMA.
		RtmpDmaEnable(pAd, 1);

		// Enable Tx
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value |= (1 << 2);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);

#ifdef RALINK_28xx_QA
		// add this for LoopBack mode
		if (pAd->ate.bQARxStart == FALSE)
		{
			// Disable Rx
			RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
			Value &= ~(1 << 3);
			RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
		}

		if (pAd->ate.bQATxStart == TRUE)
		{
			pAd->ate.TxStatus = 1;
		}
#else
		// Disable Rx
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= ~(1 << 3);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
#endif // RALINK_28xx_QA //

		RTMP_IO_READ32(pAd, TX_DTX_IDX0 + QID_AC_BE * RINGREG_DIFF, &pAd->TxRing[QID_AC_BE].TxDmaIdx);
		// kick Tx-Ring.
		RTMP_IO_WRITE32(pAd, TX_CTX_IDX0 + QID_AC_BE * RINGREG_DIFF, pAd->TxRing[QID_AC_BE].TxCpuIdx);

		pAd->RalinkCounters.KickTxCount++;
	}
#ifdef RALINK_28xx_QA
	else if (!strcmp(arg, "TXSTOP"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: TXSTOP\n"));
		atemode = pAd->ate.Mode;
		pAd->ate.Mode &= ATE_TXSTOP;
		pAd->ate.bQATxStart = FALSE;
//		pAd->ate.TxDoneCount = pAd->ate.TxCount;

		if (atemode == ATE_TXCARR)
		{
			// No Carrier Test set BBP R22 bit7=0, bit6=0, bit[5~0]=0x0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= 0xFFFFFF00; //clear bit7, bit6, bit[5~0]
		    ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
		}
		else if (atemode == ATE_TXCARRSUPP)
		{
			// No Cont. TX set BBP R22 bit7=0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= ~(1 << 7); //set bit7=0
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

			// No Carrier Suppression set BBP R24 bit0=0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R24, &BbpData);
			BbpData &= 0xFFFFFFFE; //clear bit0
		    ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R24, BbpData);
		}

		/*
			We should free some resource which was allocated
			when ATE_TXFRAME, ATE_STOP, and ATE_TXCONT.
		*/
		else if ((atemode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
			PRTMP_TX_RING pTxRing = &pAd->TxRing[QID_AC_BE];

			if (atemode == ATE_TXCONT)
			{
				// No Cont. TX set BBP R22 bit7=0
				ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
				BbpData &= ~(1 << 7); //set bit7=0
				ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
			}

			// Abort Tx, Rx DMA.
			RtmpDmaEnable(pAd, 0);

			for (i=0; i<TX_RING_SIZE; i++)
			{
				PNDIS_PACKET  pPacket;

#ifndef RT_BIG_ENDIAN
			    pTxD = (PTXD_STRUC)pAd->TxRing[QID_AC_BE].Cell[i].AllocVa;
#else
			pDestTxD = (PTXD_STRUC)pAd->TxRing[QID_AC_BE].Cell[i].AllocVa;
			TxD = *pDestTxD;
			pTxD = &TxD;
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
				pTxD->DMADONE = 0;
				pPacket = pTxRing->Cell[i].pNdisPacket;

				if (pPacket)
				{
					PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_SUCCESS);
				}

				// Always assign pNdisPacket as NULL after clear
				pTxRing->Cell[i].pNdisPacket = NULL;

				pPacket = pTxRing->Cell[i].pNextNdisPacket;

				if (pPacket)
				{
					PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr1, pTxD->SDLen1, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_SUCCESS);
				}

				// Always assign pNextNdisPacket as NULL after clear
				pTxRing->Cell[i].pNextNdisPacket = NULL;
#ifdef RT_BIG_ENDIAN
				RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
				WriteBackToDescriptor((PUCHAR)pDestTxD, (PUCHAR)pTxD, FALSE, TYPE_TXD);
#endif
			}
			// Enable Tx, Rx DMA
			RtmpDmaEnable(pAd, 1);

		}

		// TxStatus : 0 --> task is idle, 1 --> task is running
		pAd->ate.TxStatus = 0;

		// Soft reset BBP.
		BbpSoftReset(pAd);

		// Disable Tx
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= ~(1 << 2);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
	}
	else if (!strcmp(arg, "RXSTOP"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: RXSTOP\n"));
		atemode = pAd->ate.Mode;
		pAd->ate.Mode &= ATE_RXSTOP;
		pAd->ate.bQARxStart = FALSE;
//		pAd->ate.TxDoneCount = pAd->ate.TxCount;

		if (atemode == ATE_TXCARR)
		{
			;
		}
		else if (atemode == ATE_TXCARRSUPP)
		{
			;
		}

		/*
			We should free some resource which was allocated
			when ATE_TXFRAME, ATE_STOP, and ATE_TXCONT.
		*/
		else if ((atemode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
			if (atemode == ATE_TXCONT)
			{
				;
			}
		}

		// Soft reset BBP.
		BbpSoftReset(pAd);

		// Disable Rx
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= ~(1 << 3);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
	}
#endif // RALINK_28xx_QA //
	else if (!strcmp(arg, "RXFRAME"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: RXFRAME\n"));

		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, MacData);

		pAd->ate.Mode |= ATE_RXFRAME;

		// Disable Tx of MAC block.
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= ~(1 << 2);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);

		// Enable Rx of MAC block.
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value |= (1 << 3);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
	}
	else
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: Invalid arg!\n"));
		return FALSE;
	}
	RTMPusecDelay(5000);

	ATEDBGPRINT(RT_DEBUG_TRACE, ("<=== ATECmdHandler()\n"));

	return TRUE;
}
/*=======================End of RTMP_MAC_PCI =======================*/
#endif // RTMP_MAC_PCI //




INT	Set_ATE_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	if (ATECmdHandler(pAd, arg))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_Proc Success\n"));


		return TRUE;
	}
	else
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_Proc Failed\n"));
		return FALSE;
	}
}


/*
==========================================================================
    Description:
        Set ATE ADDR1=DA for TxFrame(AP  : To DS = 0 ; From DS = 1)
        or
        Set ATE ADDR3=DA for TxFrame(STA : To DS = 1 ; From DS = 0)

    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_DA_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	PSTRING				value;
	INT					i;

	// Mac address acceptable format 01:02:03:04:05:06 length 17
	if (strlen(arg) != 17)
		return FALSE;

    for (i = 0, value = rstrtok(arg, ":"); value; value = rstrtok(NULL, ":"))
	{
		/* sanity check */
		if ((strlen(value) != 2) || (!isxdigit(*value)) || (!isxdigit(*(value+1))))
		{
			return FALSE;
		}

#ifdef CONFIG_STA_SUPPORT
		AtoH(value, &pAd->ate.Addr3[i++], 1);
#endif // CONFIG_STA_SUPPORT //
	}

	/* sanity check */
	if (i != 6)
	{
		return FALSE;
	}

#ifdef CONFIG_STA_SUPPORT
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_DA_Proc (DA = %2X:%2X:%2X:%2X:%2X:%2X)\n", pAd->ate.Addr3[0],
		pAd->ate.Addr3[1], pAd->ate.Addr3[2], pAd->ate.Addr3[3], pAd->ate.Addr3[4], pAd->ate.Addr3[5]));
#endif // CONFIG_STA_SUPPORT //

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_DA_Proc Success\n"));

	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE ADDR3=SA for TxFrame(AP  : To DS = 0 ; From DS = 1)
        or
        Set ATE ADDR2=SA for TxFrame(STA : To DS = 1 ; From DS = 0)

    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_SA_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	PSTRING				value;
	INT					i;

	// Mac address acceptable format 01:02:03:04:05:06 length 17
	if (strlen(arg) != 17)
		return FALSE;

    for (i=0, value = rstrtok(arg, ":"); value; value = rstrtok(NULL, ":"))
	{
		/* sanity check */
		if ((strlen(value) != 2) || (!isxdigit(*value)) || (!isxdigit(*(value+1))))
		{
			return FALSE;
		}

#ifdef CONFIG_STA_SUPPORT
		AtoH(value, &pAd->ate.Addr2[i++], 1);
#endif // CONFIG_STA_SUPPORT //
	}

	/* sanity check */
	if (i != 6)
	{
		return FALSE;
	}

#ifdef CONFIG_STA_SUPPORT
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_SA_Proc (SA = %2X:%2X:%2X:%2X:%2X:%2X)\n", pAd->ate.Addr2[0],
		pAd->ate.Addr2[1], pAd->ate.Addr2[2], pAd->ate.Addr2[3], pAd->ate.Addr2[4], pAd->ate.Addr2[5]));
#endif // CONFIG_STA_SUPPORT //

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_SA_Proc Success\n"));

	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE ADDR2=BSSID for TxFrame(AP  : To DS = 0 ; From DS = 1)
        or
        Set ATE ADDR1=BSSID for TxFrame(STA : To DS = 1 ; From DS = 0)

    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_BSSID_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	PSTRING				value;
	INT					i;

	// Mac address acceptable format 01:02:03:04:05:06 length 17
	if (strlen(arg) != 17)
		return FALSE;

    for (i=0, value = rstrtok(arg, ":"); value; value = rstrtok(NULL, ":"))
	{
		/* sanity check */
		if ((strlen(value) != 2) || (!isxdigit(*value)) || (!isxdigit(*(value+1))))
		{
			return FALSE;
		}

#ifdef CONFIG_STA_SUPPORT
		AtoH(value, &pAd->ate.Addr1[i++], 1);
#endif // CONFIG_STA_SUPPORT //
	}

	/* sanity check */
	if(i != 6)
	{
		return FALSE;
	}

#ifdef CONFIG_STA_SUPPORT
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_BSSID_Proc (BSSID = %2X:%2X:%2X:%2X:%2X:%2X)\n",	pAd->ate.Addr1[0],
		pAd->ate.Addr1[1], pAd->ate.Addr1[2], pAd->ate.Addr1[3], pAd->ate.Addr1[4], pAd->ate.Addr1[5]));
#endif // CONFIG_STA_SUPPORT //

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_BSSID_Proc Success\n"));

	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE Tx Channel

    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_CHANNEL_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	UCHAR channel;

	channel = simple_strtol(arg, 0, 10);

	// to allow A band channel : ((channel < 1) || (channel > 14))
	if ((channel < 1) || (channel > 216))
	{
		ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_CHANNEL_Proc::Out of range, it should be in range of 1~14.\n"));
		return FALSE;
	}
	pAd->ate.Channel = channel;

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_CHANNEL_Proc (ATE Channel = %d)\n", pAd->ate.Channel));
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_CHANNEL_Proc Success\n"));


	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE Tx Power0

    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_TX_POWER0_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	CHAR TxPower;

	TxPower = simple_strtol(arg, 0, 10);

	if (pAd->ate.Channel <= 14)
	{
		if ((TxPower > 31) || (TxPower < 0))
		{
			ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_TX_POWER0_Proc::Out of range (Value=%d)\n", TxPower));
			return FALSE;
		}
	}
	else/* 5.5 GHz */
	{
		if ((TxPower > 15) || (TxPower < -7))
		{
			ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_TX_POWER0_Proc::Out of range (Value=%d)\n", TxPower));
			return FALSE;
		}
	}

	pAd->ate.TxPower0 = TxPower;
	ATETxPwrHandler(pAd, 0);
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_TX_POWER0_Proc Success\n"));


	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE Tx Power1

    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_TX_POWER1_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	CHAR TxPower;

	TxPower = simple_strtol(arg, 0, 10);

	if (pAd->ate.Channel <= 14)
	{
		if ((TxPower > 31) || (TxPower < 0))
		{
			ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_TX_POWER1_Proc::Out of range (Value=%d)\n", TxPower));
			return FALSE;
		}
	}
	else
	{
		if ((TxPower > 15) || (TxPower < -7))
		{
			ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_TX_POWER1_Proc::Out of range (Value=%d)\n", TxPower));
			return FALSE;
		}
	}

	pAd->ate.TxPower1 = TxPower;
	ATETxPwrHandler(pAd, 1);
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_TX_POWER1_Proc Success\n"));


	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE Tx Antenna

    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_TX_Antenna_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	CHAR value;

	value = simple_strtol(arg, 0, 10);

	if ((value > 2) || (value < 0))
	{
		ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_TX_Antenna_Proc::Out of range (Value=%d)\n", value));
		return FALSE;
	}

	pAd->ate.TxAntennaSel = value;

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_TX_Antenna_Proc (Antenna = %d)\n", pAd->ate.TxAntennaSel));
	ATEDBGPRINT(RT_DEBUG_TRACE,("Ralink: Set_ATE_TX_Antenna_Proc Success\n"));

	// calibration power unbalance issues, merged from Arch Team
	ATEAsicSwitchChannel(pAd);


	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE Rx Antenna

    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_RX_Antenna_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	CHAR value;

	value = simple_strtol(arg, 0, 10);

	if ((value > 3) || (value < 0))
	{
		ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_RX_Antenna_Proc::Out of range (Value=%d)\n", value));
		return FALSE;
	}

	pAd->ate.RxAntennaSel = value;

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_RX_Antenna_Proc (Antenna = %d)\n", pAd->ate.RxAntennaSel));
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_RX_Antenna_Proc Success\n"));

	// calibration power unbalance issues, merged from Arch Team
	ATEAsicSwitchChannel(pAd);


	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE RF frequence offset

    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_TX_FREQOFFSET_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	UCHAR RFFreqOffset = 0;
	ULONG R4 = 0;

	RFFreqOffset = simple_strtol(arg, 0, 10);
#ifndef RTMP_RF_RW_SUPPORT
	if (RFFreqOffset >= 64)
#endif // RTMP_RF_RW_SUPPORT //
	/* RT35xx ATE will reuse this code segment. */
#ifdef RTMP_RF_RW_SUPPORT
//2008/08/06: KH modified the limit of offset value from 65 to 95(0x5F)
	if (RFFreqOffset >= 95)
#endif // RTMP_RF_RW_SUPPORT //
	{
		ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_TX_FREQOFFSET_Proc::Out of range, it should be in range of 0~63.\n"));
		return FALSE;
	}

	pAd->ate.RFFreqOffset = RFFreqOffset;
#ifdef RTMP_RF_RW_SUPPORT
	if (IS_RT30xx(pAd) || IS_RT3572(pAd))
	{
		// Set RF offset
		UCHAR RFValue;
		ATE_RF_IO_READ8_BY_REG_ID(pAd, RF_R23, (PUCHAR)&RFValue);
//2008/08/06: KH modified "pAd->RFFreqOffset" to "pAd->ate.RFFreqOffset"
		RFValue = ((RFValue & 0x80) | pAd->ate.RFFreqOffset);
		ATE_RF_IO_WRITE8_BY_REG_ID(pAd, RF_R23, (UCHAR)RFValue);
	}
	else
#endif // RTMP_RF_RW_SUPPORT //
	{
		// RT28xx
		// shift TX power control to correct RF register bit position
		R4 = pAd->ate.RFFreqOffset << 15;
		R4 |= (pAd->LatchRfRegs.R4 & ((~0x001f8000)));
		pAd->LatchRfRegs.R4 = R4;

		RtmpRfIoWrite(pAd);
	}
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_TX_FREQOFFSET_Proc (RFFreqOffset = %d)\n", pAd->ate.RFFreqOffset));
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_TX_FREQOFFSET_Proc Success\n"));


	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE RF BW

    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_TX_BW_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	INT i;
	UCHAR value = 0;
	UCHAR BBPCurrentBW;

	BBPCurrentBW = simple_strtol(arg, 0, 10);

	if ((BBPCurrentBW == 0)
#ifdef RT30xx
		|| IS_RT2070(pAd)
#endif // RT30xx //
		)
	{
		pAd->ate.TxWI.BW = BW_20;
	}
	else
	{
		pAd->ate.TxWI.BW = BW_40;
	}

	/* RT35xx ATE will reuse this code segment. */
	// Fix the error spectrum of CCK-40MHZ
	// Turn on BBP 20MHz mode by request here.
	if ((pAd->ate.TxWI.PHYMODE == MODE_CCK) && (pAd->ate.TxWI.BW == BW_40))
	{
		ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_TX_BW_Proc!! Warning!! CCK only supports 20MHZ!!\nBandwidth switch to 20\n"));
		pAd->ate.TxWI.BW = BW_20;
	}

	if (pAd->ate.TxWI.BW == BW_20)
	{
		if (pAd->ate.Channel <= 14)
		{
			for (i=0; i<5; i++)
			{
				if (pAd->Tx20MPwrCfgGBand[i] != 0xffffffff)
				{
					RTMP_IO_WRITE32(pAd, TX_PWR_CFG_0 + i*4, pAd->Tx20MPwrCfgGBand[i]);
					RTMPusecDelay(5000);
				}
			}
		}
		else
		{
			for (i=0; i<5; i++)
			{
				if (pAd->Tx20MPwrCfgABand[i] != 0xffffffff)
				{
					RTMP_IO_WRITE32(pAd, TX_PWR_CFG_0 + i*4, pAd->Tx20MPwrCfgABand[i]);
					RTMPusecDelay(5000);
				}
			}
		}

		// Set BBP R4 bit[4:3]=0:0
		ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R4, &value);
		value &= (~0x18);
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R4, value);


		// Set BBP R66=0x3C
		value = 0x3C;
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R66, value);

		// Set BBP R68=0x0B
		// to improve Rx sensitivity.
		value = 0x0B;
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R68, value);
		// Set BBP R69=0x16
		value = 0x16;
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R69, value);
		// Set BBP R70=0x08
		value = 0x08;
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R70, value);
		// Set BBP R73=0x11
		value = 0x11;
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R73, value);

	    /*
			If Channel=14, Bandwidth=20M and Mode=CCK, Set BBP R4 bit5=1
			(to set Japan filter coefficients).
			This segment of code will only works when ATETXMODE and ATECHANNEL
			were set to MODE_CCK and 14 respectively before ATETXBW is set to 0.
	    */
		if (pAd->ate.Channel == 14)
		{
			INT TxMode = pAd->ate.TxWI.PHYMODE;

			if (TxMode == MODE_CCK)
			{
				// when Channel==14 && Mode==CCK && BandWidth==20M, BBP R4 bit5=1
				ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R4, &value);
				value |= 0x20; //set bit5=1
				ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R4, value);
			}
		}

#ifdef RT30xx
		// set BW = 20 MHz
		if (IS_RT30xx(pAd))
			ATE_RF_IO_WRITE8_BY_REG_ID(pAd, RF_R24, (UCHAR) pAd->Mlme.CaliBW20RfR24);
		else
#endif // RT30xx //
		// set BW = 20 MHz
		{
			pAd->LatchRfRegs.R4 &= ~0x00200000;
			RtmpRfIoWrite(pAd);
		}

	}
	// If bandwidth = 40M, set RF Reg4 bit 21 = 0.
	else if (pAd->ate.TxWI.BW == BW_40)
	{
		if (pAd->ate.Channel <= 14)
		{
			for (i=0; i<5; i++)
			{
				if (pAd->Tx40MPwrCfgGBand[i] != 0xffffffff)
				{
					RTMP_IO_WRITE32(pAd, TX_PWR_CFG_0 + i*4, pAd->Tx40MPwrCfgGBand[i]);
					RTMPusecDelay(5000);
				}
			}
		}
		else
		{
			for (i=0; i<5; i++)
			{
				if (pAd->Tx40MPwrCfgABand[i] != 0xffffffff)
				{
					RTMP_IO_WRITE32(pAd, TX_PWR_CFG_0 + i*4, pAd->Tx40MPwrCfgABand[i]);
					RTMPusecDelay(5000);
				}
			}
#ifdef DOT11_N_SUPPORT
			if ((pAd->ate.TxWI.PHYMODE >= MODE_HTMIX) && (pAd->ate.TxWI.MCS == 7))
			{
			value = 0x28;
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R67, value);
			}
#endif // DOT11_N_SUPPORT //
		}

		// Set BBP R4 bit[4:3]=1:0
		ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R4, &value);
		value &= (~0x18);
		value |= 0x10;
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R4, value);


		// Set BBP R66=0x3C
		value = 0x3C;
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R66, value);

		// Set BBP R68=0x0C
		// to improve Rx sensitivity
		value = 0x0C;
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R68, value);
		// Set BBP R69=0x1A
		value = 0x1A;
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R69, value);
		// Set BBP R70=0x0A
		value = 0x0A;
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R70, value);
		// Set BBP R73=0x16
		value = 0x16;
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R73, value);

		// If bandwidth = 40M, set RF Reg4 bit 21 = 1.
#ifdef RT30xx
		// set BW = 40 MHz
		if(IS_RT30xx(pAd))
			ATE_RF_IO_WRITE8_BY_REG_ID(pAd, RF_R24, (UCHAR) pAd->Mlme.CaliBW40RfR24);
		else
#endif // RT30xx //
		// set BW = 40 MHz
		{
		pAd->LatchRfRegs.R4 |= 0x00200000;
		RtmpRfIoWrite(pAd);
		}
	}

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_TX_BW_Proc (BBPCurrentBW = %d)\n", pAd->ate.TxWI.BW));
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_TX_BW_Proc Success\n"));


	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE Tx frame length

    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_TX_LENGTH_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	pAd->ate.TxLength = simple_strtol(arg, 0, 10);

	if ((pAd->ate.TxLength < 24) || (pAd->ate.TxLength > (MAX_FRAME_SIZE - 34/* == 2312 */)))
	{
		pAd->ate.TxLength = (MAX_FRAME_SIZE - 34/* == 2312 */);
		ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_TX_LENGTH_Proc::Out of range, it should be in range of 24~%d.\n", (MAX_FRAME_SIZE - 34/* == 2312 */)));
		return FALSE;
	}

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_TX_LENGTH_Proc (TxLength = %d)\n", pAd->ate.TxLength));
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_TX_LENGTH_Proc Success\n"));


	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE Tx frame count

    Return:
        TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_TX_COUNT_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	pAd->ate.TxCount = simple_strtol(arg, 0, 10);

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_TX_COUNT_Proc (TxCount = %d)\n", pAd->ate.TxCount));
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_TX_COUNT_Proc Success\n"));


	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE Tx frame MCS

        Return:
		TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_TX_MCS_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	UCHAR MCS;
	INT result;

	MCS = simple_strtol(arg, 0, 10);
#ifndef RT30xx
	result = CheckMCSValid(pAd->ate.TxWI.PHYMODE, MCS);
#endif // RT30xx //

	/* RT35xx ATE will reuse this code segment. */
#ifdef RT30xx
	result = CheckMCSValid(pAd->ate.TxWI.PHYMODE, MCS, IS_RT2070(pAd));
#endif // RT30xx //


	if (result != -1)
	{
		pAd->ate.TxWI.MCS = (UCHAR)MCS;
	}
	else
	{
		ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_TX_MCS_Proc::Out of range, refer to rate table.\n"));
		return FALSE;
	}

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_TX_MCS_Proc (MCS = %d)\n", pAd->ate.TxWI.MCS));
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_TX_MCS_Proc Success\n"));


	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE Tx frame Mode
        0: MODE_CCK
        1: MODE_OFDM
        2: MODE_HTMIX
        3: MODE_HTGREENFIELD

        Return:
		TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_TX_MODE_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	UCHAR BbpData = 0;

	pAd->ate.TxWI.PHYMODE = simple_strtol(arg, 0, 10);

	if (pAd->ate.TxWI.PHYMODE > 3)
	{
		pAd->ate.TxWI.PHYMODE = 0;
		ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_TX_MODE_Proc::Out of range.\nIt should be in range of 0~3\n"));
		ATEDBGPRINT(RT_DEBUG_ERROR, ("0: CCK, 1: OFDM, 2: HT_MIX, 3: HT_GREEN_FIELD.\n"));
		return FALSE;
	}

	// Turn on BBP 20MHz mode by request here.
	if (pAd->ate.TxWI.PHYMODE == MODE_CCK)
	{
		ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R4, &BbpData);
		BbpData &= (~0x18);
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R4, BbpData);
		pAd->ate.TxWI.BW = BW_20;
		ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_TX_MODE_Proc::CCK Only support 20MHZ. Switch to 20MHZ.\n"));
	}

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_TX_MODE_Proc (TxMode = %d)\n", pAd->ate.TxWI.PHYMODE));
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_TX_MODE_Proc Success\n"));


	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE Tx frame GI

        Return:
		TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
INT	Set_ATE_TX_GI_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	pAd->ate.TxWI.ShortGI = simple_strtol(arg, 0, 10);

	if (pAd->ate.TxWI.ShortGI > 1)
	{
		pAd->ate.TxWI.ShortGI = 0;
		ATEDBGPRINT(RT_DEBUG_ERROR, ("Set_ATE_TX_GI_Proc::Out of range\n"));
		return FALSE;
	}

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_TX_GI_Proc (GI = %d)\n", pAd->ate.TxWI.ShortGI));
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_TX_GI_Proc Success\n"));


	return TRUE;
}


INT	Set_ATE_RX_FER_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	pAd->ate.bRxFER = simple_strtol(arg, 0, 10);

	if (pAd->ate.bRxFER == 1)
	{
		pAd->ate.RxCntPerSec = 0;
		pAd->ate.RxTotalCnt = 0;
	}

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_RX_FER_Proc (bRxFER = %d)\n", pAd->ate.bRxFER));
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_RX_FER_Proc Success\n"));


	return TRUE;
}


INT Set_ATE_Read_RF_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
#ifdef RTMP_RF_RW_SUPPORT
//2008/07/10:KH add to support RT30xx ATE<--
	if (IS_RT30xx(pAd) || IS_RT3572(pAd))
	{
		/* modify by WY for Read RF Reg. error */
		UCHAR RFValue;
		INT index=0;

		for (index = 0; index < 32; index++)
		{
			ATE_RF_IO_READ8_BY_REG_ID(pAd, index, (PUCHAR)&RFValue);
			ate_print("R%d=%d\n",index,RFValue);
		}
	}
	else
//2008/07/10:KH add to support RT30xx ATE-->
#endif // RTMP_RF_RW_SUPPORT //
	{
		ate_print(KERN_EMERG "R1 = %lx\n", pAd->LatchRfRegs.R1);
		ate_print(KERN_EMERG "R2 = %lx\n", pAd->LatchRfRegs.R2);
		ate_print(KERN_EMERG "R3 = %lx\n", pAd->LatchRfRegs.R3);
		ate_print(KERN_EMERG "R4 = %lx\n", pAd->LatchRfRegs.R4);
	}
	return TRUE;
}


INT Set_ATE_Write_RF1_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	UINT32 value = (UINT32) simple_strtol(arg, 0, 16);

#ifdef RTMP_RF_RW_SUPPORT
//2008/07/10:KH add to support 3070 ATE<--
	if (IS_RT30xx(pAd) || IS_RT3572(pAd))
	{
		ate_print("Warning!! RT3xxx Don't Support !\n");
		return FALSE;

	}
	else
//2008/07/10:KH add to support 3070 ATE-->
#endif // RTMP_RF_RW_SUPPORT //
	{
		pAd->LatchRfRegs.R1 = value;
		RtmpRfIoWrite(pAd);
	}
	return TRUE;
}


INT Set_ATE_Write_RF2_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	UINT32 value = (UINT32) simple_strtol(arg, 0, 16);

#ifdef RTMP_RF_RW_SUPPORT
//2008/07/10:KH add to support 3070 ATE<--
	if (IS_RT30xx(pAd) || IS_RT3572(pAd))
	{
		ate_print("Warning!! RT3xxx Don't Support !\n");
		return FALSE;

	}
	else
//2008/07/10:KH add to support 3070 ATE-->
#endif // RTMP_RF_RW_SUPPORT //
	{
		pAd->LatchRfRegs.R2 = value;
		RtmpRfIoWrite(pAd);
	}
	return TRUE;
}


INT Set_ATE_Write_RF3_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	UINT32 value = simple_strtol(arg, 0, 16);

#ifdef RTMP_RF_RW_SUPPORT
//2008/07/10:KH add to support 3070 ATE<--
	if (IS_RT30xx(pAd) || IS_RT3572(pAd))
	{
		ate_print("Warning!! RT3xxx Don't Support !\n");
		return FALSE;

	}
	else
//2008/07/10:KH add to support 3070 ATE-->
#endif // RTMP_RF_RW_SUPPORT //
	{
		pAd->LatchRfRegs.R3 = value;
		RtmpRfIoWrite(pAd);
	}
	return TRUE;
}


INT Set_ATE_Write_RF4_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	UINT32 value = (UINT32) simple_strtol(arg, 0, 16);

#ifdef RTMP_RF_RW_SUPPORT
//2008/07/10:KH add to support 3070 ATE<--
	if (IS_RT30xx(pAd) || IS_RT3572(pAd))
	{
		ate_print("Warning!! RT3xxx Don't Support !\n");
		return FALSE;

	}
	else
//2008/07/10:KH add to support 3070 ATE-->
#endif // RTMP_RF_RW_SUPPORT //
	{
		pAd->LatchRfRegs.R4 = value;
		RtmpRfIoWrite(pAd);
	}
	return TRUE;
}


/*
==========================================================================
    Description:
        Load and Write EEPROM from a binary file prepared in advance.

        Return:
		TRUE if all parameters are OK, FALSE otherwise
==========================================================================
*/
#if defined(LINUX) || defined(VXWORKS)
INT Set_ATE_Load_E2P_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	BOOLEAN			ret = FALSE;
	PSTRING			src = EEPROM_BIN_FILE_NAME;
	RTMP_OS_FD		srcf;
	INT32			retval;
	USHORT			WriteEEPROM[(EEPROM_SIZE/2)];
	INT				FileLength = 0;
	UINT32			value = (UINT32) simple_strtol(arg, 0, 10);
	RTMP_OS_FS_INFO	osFSInfo;

	ATEDBGPRINT(RT_DEBUG_ERROR, ("===> %s (value=%d)\n\n", __FUNCTION__, value));

	if (value > 0)
	{
		/* zero the e2p buffer */
		NdisZeroMemory((PUCHAR)WriteEEPROM, EEPROM_SIZE);

		RtmpOSFSInfoChange(&osFSInfo, TRUE);

		do
		{
			/* open the bin file */
			srcf = RtmpOSFileOpen(src, O_RDONLY, 0);

			if (IS_FILE_OPEN_ERR(srcf))
			{
				ate_print("%s - Error opening file %s\n", __FUNCTION__, src);
				break;
			}

			/* read the firmware from the file *.bin */
			FileLength = RtmpOSFileRead(srcf, (PSTRING)WriteEEPROM, EEPROM_SIZE);

			if (FileLength != EEPROM_SIZE)
			{
				ate_print("%s: error file length (=%d) in e2p.bin\n",
					   __FUNCTION__, FileLength);
				break;
			}
			else
			{
				/* write the content of .bin file to EEPROM */
				rt_ee_write_all(pAd, WriteEEPROM);
				ret = TRUE;
			}
			break;
		} while(TRUE);

		/* close firmware file */
		if (IS_FILE_OPEN_ERR(srcf))
		{
				;
		}
		else
		{
			retval = RtmpOSFileClose(srcf);

			if (retval)
			{
				ATEDBGPRINT(RT_DEBUG_ERROR, ("--> Error %d closing %s\n", -retval, src));

			}
		}

		/* restore */
		RtmpOSFSInfoChange(&osFSInfo, FALSE);
	}

    ATEDBGPRINT(RT_DEBUG_ERROR, ("<=== %s (ret=%d)\n", __FUNCTION__, ret));

    return ret;

}
#endif // defined(LINUX) || defined(VXWORKS) //




INT Set_ATE_Read_E2P_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	USHORT buffer[EEPROM_SIZE/2];
	USHORT *p;
	int i;

	rt_ee_read_all(pAd, (USHORT *)buffer);
	p = buffer;
	for (i = 0; i < (EEPROM_SIZE/2); i++)
	{
		ate_print("%4.4x ", *p);
		if (((i+1) % 16) == 0)
			ate_print("\n");
		p++;
	}
	return TRUE;
}




INT	Set_ATE_Show_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	ate_print("Mode=%d\n", pAd->ate.Mode);
	ate_print("TxPower0=%d\n", pAd->ate.TxPower0);
	ate_print("TxPower1=%d\n", pAd->ate.TxPower1);
	ate_print("TxAntennaSel=%d\n", pAd->ate.TxAntennaSel);
	ate_print("RxAntennaSel=%d\n", pAd->ate.RxAntennaSel);
	ate_print("BBPCurrentBW=%d\n", pAd->ate.TxWI.BW);
	ate_print("GI=%d\n", pAd->ate.TxWI.ShortGI);
	ate_print("MCS=%d\n", pAd->ate.TxWI.MCS);
	ate_print("TxMode=%d\n", pAd->ate.TxWI.PHYMODE);
	ate_print("Addr1=%02x:%02x:%02x:%02x:%02x:%02x\n",
		pAd->ate.Addr1[0], pAd->ate.Addr1[1], pAd->ate.Addr1[2], pAd->ate.Addr1[3], pAd->ate.Addr1[4], pAd->ate.Addr1[5]);
	ate_print("Addr2=%02x:%02x:%02x:%02x:%02x:%02x\n",
		pAd->ate.Addr2[0], pAd->ate.Addr2[1], pAd->ate.Addr2[2], pAd->ate.Addr2[3], pAd->ate.Addr2[4], pAd->ate.Addr2[5]);
	ate_print("Addr3=%02x:%02x:%02x:%02x:%02x:%02x\n",
		pAd->ate.Addr3[0], pAd->ate.Addr3[1], pAd->ate.Addr3[2], pAd->ate.Addr3[3], pAd->ate.Addr3[4], pAd->ate.Addr3[5]);
	ate_print("Channel=%d\n", pAd->ate.Channel);
	ate_print("TxLength=%d\n", pAd->ate.TxLength);
	ate_print("TxCount=%u\n", pAd->ate.TxCount);
	ate_print("RFFreqOffset=%d\n", pAd->ate.RFFreqOffset);
	ate_print(KERN_EMERG "Set_ATE_Show_Proc Success\n");
	return TRUE;
}


INT	Set_ATE_Help_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	ate_print("ATE=ATESTART, ATESTOP, TXCONT, TXCARR, TXFRAME, RXFRAME\n");
	ate_print("ATEDA\n");
	ate_print("ATESA\n");
	ate_print("ATEBSSID\n");
	ate_print("ATECHANNEL, range:0~14(unless A band !)\n");
	ate_print("ATETXPOW0, set power level of antenna 1.\n");
	ate_print("ATETXPOW1, set power level of antenna 2.\n");
	ate_print("ATETXANT, set TX antenna. 0:all, 1:antenna one, 2:antenna two.\n");
	ate_print("ATERXANT, set RX antenna.0:all, 1:antenna one, 2:antenna two, 3:antenna three.\n");
	ate_print("ATETXFREQOFFSET, set frequency offset, range 0~63\n");
	ate_print("ATETXBW, set BandWidth, 0:20MHz, 1:40MHz.\n");
	ate_print("ATETXLEN, set Frame length, range 24~%d\n", (MAX_FRAME_SIZE - 34/* == 2312 */));
	ate_print("ATETXCNT, set how many frame going to transmit.\n");
	ate_print("ATETXMCS, set MCS, reference to rate table.\n");
	ate_print("ATETXMODE, set Mode 0:CCK, 1:OFDM, 2:HT-Mix, 3:GreenField, reference to rate table.\n");
	ate_print("ATETXGI, set GI interval, 0:Long, 1:Short\n");
	ate_print("ATERXFER, 0:disable Rx Frame error rate. 1:enable Rx Frame error rate.\n");
	ate_print("ATERRF, show all RF registers.\n");
	ate_print("ATEWRF1, set RF1 register.\n");
	ate_print("ATEWRF2, set RF2 register.\n");
	ate_print("ATEWRF3, set RF3 register.\n");
	ate_print("ATEWRF4, set RF4 register.\n");
	ate_print("ATELDE2P, load EEPROM from .bin file.\n");
	ate_print("ATERE2P, display all EEPROM content.\n");
	ate_print("ATESHOW, display all parameters of ATE.\n");
	ate_print("ATEHELP, online help.\n");

	return TRUE;
}




/*
==========================================================================
    Description:

	AsicSwitchChannel() dedicated for ATE.

==========================================================================
*/
VOID ATEAsicSwitchChannel(
    IN PRTMP_ADAPTER pAd)
{
	UINT32 R2 = 0, R3 = DEFAULT_RF_TX_POWER, R4 = 0, Value = 0;
	CHAR TxPwer = 0, TxPwer2 = 0;
	UCHAR index = 0, BbpValue = 0, R66 = 0x30;
	RTMP_RF_REGS *RFRegTable;
	UCHAR Channel = 0;

	RFRegTable = NULL;

#ifdef RALINK_28xx_QA
	// for QA mode, TX power values are passed from UI
	if ((pAd->ate.bQATxStart == TRUE) || (pAd->ate.bQARxStart == TRUE))
	{
		if (pAd->ate.Channel != pAd->LatchRfRegs.Channel)
		{
			pAd->ate.Channel = pAd->LatchRfRegs.Channel;
		}
		return;
	}
	else
#endif // RALINK_28xx_QA //
	Channel = pAd->ate.Channel;

	// select antenna for RT3090
	AsicAntennaSelect(pAd, Channel);

	// fill Tx power value
	TxPwer = pAd->ate.TxPower0;
	TxPwer2 = pAd->ate.TxPower1;
#ifdef RT30xx
//2008/07/10:KH add to support 3070 ATE<--

	/*
		The RF programming sequence is difference between 3xxx and 2xxx.
		The 3070 is 1T1R. Therefore, we don't need to set the number of Tx/Rx path
		and the only job is to set the parameters of channels.
	*/
	if (IS_RT30xx(pAd) && ((pAd->RfIcType == RFIC_3020) ||
			(pAd->RfIcType == RFIC_3021) || (pAd->RfIcType == RFIC_3022) ||
			(pAd->RfIcType == RFIC_2020)))
	{
		/* modify by WY for Read RF Reg. error */
		UCHAR RFValue = 0;

		for (index = 0; index < NUM_OF_3020_CHNL; index++)
		{
			if (Channel == FreqItems3020[index].Channel)
			{
				// Programming channel parameters.
				ATE_RF_IO_WRITE8_BY_REG_ID(pAd, RF_R02, FreqItems3020[index].N);
				ATE_RF_IO_WRITE8_BY_REG_ID(pAd, RF_R03, FreqItems3020[index].K);

				ATE_RF_IO_READ8_BY_REG_ID(pAd, RF_R06, (PUCHAR)&RFValue);
				RFValue = (RFValue & 0xFC) | FreqItems3020[index].R;
				ATE_RF_IO_WRITE8_BY_REG_ID(pAd, RF_R06, (UCHAR)RFValue);

				// Set Tx Power.
				ATE_RF_IO_READ8_BY_REG_ID(pAd, RF_R12, (PUCHAR)&RFValue);
				RFValue = (RFValue & 0xE0) | TxPwer;
				ATE_RF_IO_WRITE8_BY_REG_ID(pAd, RF_R12, (UCHAR)RFValue);

				// Set RF offset.
				ATE_RF_IO_READ8_BY_REG_ID(pAd, RF_R23, (PUCHAR)&RFValue);
				//2008/08/06: KH modified "pAd->RFFreqOffset" to "pAd->ate.RFFreqOffset"
				RFValue = (RFValue & 0x80) | pAd->ate.RFFreqOffset;
				ATE_RF_IO_WRITE8_BY_REG_ID(pAd, RF_R23, (UCHAR)RFValue);

				// Set BW.
				if (pAd->ate.TxWI.BW == BW_40)
				{
					RFValue = pAd->Mlme.CaliBW40RfR24;
//					DISABLE_11N_CHECK(pAd);
				}
				else
				{
					RFValue = pAd->Mlme.CaliBW20RfR24;
				}
				ATE_RF_IO_WRITE8_BY_REG_ID(pAd, RF_R24, (UCHAR)RFValue);

				// Enable RF tuning
				ATE_RF_IO_READ8_BY_REG_ID(pAd, RF_R07, (PUCHAR)&RFValue);
				RFValue = RFValue | 0x1;
				ATE_RF_IO_WRITE8_BY_REG_ID(pAd, RF_R07, (UCHAR)RFValue);

				// latch channel for future usage
				pAd->LatchRfRegs.Channel = Channel;

				break;
			}
		}

		ATEDBGPRINT(RT_DEBUG_TRACE, ("SwitchChannel#%d(RF=%d, Pwr0=%d, Pwr1=%d, %dT), N=0x%02X, K=0x%02X, R=0x%02X\n",
			Channel,
			pAd->RfIcType,
			TxPwer,
			TxPwer2,
			pAd->Antenna.field.TxPath,
			FreqItems3020[index].N,
			FreqItems3020[index].K,
			FreqItems3020[index].R));
	}
	else
//2008/07/10:KH add to support 3070 ATE-->
#endif // RT30xx //
	{
		/* RT28xx */
		RFRegTable = RF2850RegTable;

		switch (pAd->RfIcType)
		{
			/* But only 2850 and 2750 support 5.5GHz band... */
			case RFIC_2820:
			case RFIC_2850:
			case RFIC_2720:
			case RFIC_2750:

				for (index = 0; index < NUM_OF_2850_CHNL; index++)
				{
					if (Channel == RFRegTable[index].Channel)
					{
						R2 = RFRegTable[index].R2;

						// If TX path is 1, bit 14 = 1;
						if (pAd->Antenna.field.TxPath == 1)
						{
							R2 |= 0x4000;
						}

						if (pAd->Antenna.field.RxPath == 2)
						{
							switch (pAd->ate.RxAntennaSel)
							{
								case 1:
									R2 |= 0x20040;
									ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R3, &BbpValue);
									BbpValue &= 0xE4;
									BbpValue |= 0x00;
									ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R3, BbpValue);
									break;
								case 2:
									R2 |= 0x10040;
									ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R3, &BbpValue);
									BbpValue &= 0xE4;
									BbpValue |= 0x01;
									ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R3, BbpValue);
									break;
								default:
									R2 |= 0x40;
									ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R3, &BbpValue);
									BbpValue &= 0xE4;
									/* Only enable two Antenna to receive. */
									BbpValue |= 0x08;
									ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R3, BbpValue);
									break;
							}
						}
						else if (pAd->Antenna.field.RxPath == 1)
						{
							// write 1 to off RxPath
							R2 |= 0x20040;
						}

						if (pAd->Antenna.field.TxPath == 2)
						{
							if (pAd->ate.TxAntennaSel == 1)
							{
								// If TX Antenna select is 1 , bit 14 = 1; Disable Ant 2
								R2 |= 0x4000;
								ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R1, &BbpValue);
								BbpValue &= 0xE7;		// 11100111B
								ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R1, BbpValue);
							}
							else if (pAd->ate.TxAntennaSel == 2)
							{
								// If TX Antenna select is 2 , bit 15 = 1; Disable Ant 1
								R2 |= 0x8000;
								ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R1, &BbpValue);
								BbpValue &= 0xE7;
								BbpValue |= 0x08;
								ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R1, BbpValue);
							}
							else
							{
								ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R1, &BbpValue);
								BbpValue &= 0xE7;
								BbpValue |= 0x10;
								ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R1, BbpValue);
							}
						}
						if (pAd->Antenna.field.RxPath == 3)
						{
							switch (pAd->ate.RxAntennaSel)
							{
								case 1:
									R2 |= 0x20040;
									ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R3, &BbpValue);
									BbpValue &= 0xE4;
									BbpValue |= 0x00;
									ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R3, BbpValue);
									break;
								case 2:
									R2 |= 0x10040;
									ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R3, &BbpValue);
									BbpValue &= 0xE4;
									BbpValue |= 0x01;
									ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R3, BbpValue);
									break;
								case 3:
									R2 |= 0x30000;
									ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R3, &BbpValue);
									BbpValue &= 0xE4;
									BbpValue |= 0x02;
									ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R3, BbpValue);
									break;
								default:
									ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R3, &BbpValue);
									BbpValue &= 0xE4;
									BbpValue |= 0x10;
									ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R3, BbpValue);
									break;
							}
						}

						if (Channel > 14)
						{
							// initialize R3, R4
							R3 = (RFRegTable[index].R3 & 0xffffc1ff);
							R4 = (RFRegTable[index].R4 & (~0x001f87c0)) | (pAd->ate.RFFreqOffset << 15);

		                    /*
			                    According the Rory's suggestion to solve the middle range issue.

								5.5G band power range : 0xF9~0X0F, TX0 Reg3 bit9/TX1 Reg4 bit6="0"
														means the TX power reduce 7dB.
							*/
							// R3
							if ((TxPwer >= -7) && (TxPwer < 0))
							{
								TxPwer = (7+TxPwer);
								TxPwer = (TxPwer > 0xF) ? (0xF) : (TxPwer);
								R3 |= (TxPwer << 10);
								ATEDBGPRINT(RT_DEBUG_TRACE, ("ATEAsicSwitchChannel: TxPwer=%d \n", TxPwer));
							}
							else
							{
								TxPwer = (TxPwer > 0xF) ? (0xF) : (TxPwer);
								R3 |= (TxPwer << 10) | (1 << 9);
							}

							// R4
							if ((TxPwer2 >= -7) && (TxPwer2 < 0))
							{
								TxPwer2 = (7+TxPwer2);
								TxPwer2 = (TxPwer2 > 0xF) ? (0xF) : (TxPwer2);
								R4 |= (TxPwer2 << 7);
								ATEDBGPRINT(RT_DEBUG_TRACE, ("ATEAsicSwitchChannel: TxPwer2=%d \n", TxPwer2));
							}
							else
							{
								TxPwer2 = (TxPwer2 > 0xF) ? (0xF) : (TxPwer2);
								R4 |= (TxPwer2 << 7) | (1 << 6);
							}
						}
						else
						{
							// Set TX power0.
							R3 = (RFRegTable[index].R3 & 0xffffc1ff) | (TxPwer << 9);
							// Set frequency offset and TX power1.
							R4 = (RFRegTable[index].R4 & (~0x001f87c0)) | (pAd->ate.RFFreqOffset << 15) | (TxPwer2 <<6);
						}

						// based on BBP current mode before changing RF channel
						if (pAd->ate.TxWI.BW == BW_40)
						{
							R4 |=0x200000;
						}

						// Update variables.
						pAd->LatchRfRegs.Channel = Channel;
						pAd->LatchRfRegs.R1 = RFRegTable[index].R1;
						pAd->LatchRfRegs.R2 = R2;
						pAd->LatchRfRegs.R3 = R3;
						pAd->LatchRfRegs.R4 = R4;

						RtmpRfIoWrite(pAd);

						break;
					}
				}
				break;

			default:
				break;
		}
	}

	// Change BBP setting during switch from a->g, g->a
	if (Channel <= 14)
	{
	    UINT32 TxPinCfg = 0x00050F0A;// 2007.10.09 by Brian : 0x0005050A ==> 0x00050F0A

		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R62, (0x37 - GET_LNA_GAIN(pAd)));
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R63, (0x37 - GET_LNA_GAIN(pAd)));
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R64, (0x37 - GET_LNA_GAIN(pAd)));

		/* For 1T/2R chip only... */
	    if (pAd->NicConfig2.field.ExternalLNAForG)
	    {
	        ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R82, 0x62);
	    }
	    else
	    {
	        ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R82, 0x84);
	    }

        // According the Rory's suggestion to solve the middle range issue.
		ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R86, &BbpValue);// may be removed for RT35xx ++

		ASSERT((BbpValue == 0x00));
		if ((BbpValue != 0x00))
		{
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R86, 0x00);
		}// may be removed for RT35xx --

		// 5.5 GHz band selection PIN, bit1 and bit2 are complement
		RTMP_IO_READ32(pAd, TX_BAND_CFG, &Value);
		Value &= (~0x6);
		Value |= (0x04);
		RTMP_IO_WRITE32(pAd, TX_BAND_CFG, Value);

        // Turn off unused PA or LNA when only 1T or 1R.
		if (pAd->Antenna.field.TxPath == 1)
		{
			TxPinCfg &= 0xFFFFFFF3;
		}
		if (pAd->Antenna.field.RxPath == 1)
		{
			TxPinCfg &= 0xFFFFF3FF;
		}

		// calibration power unbalance issues
		if (pAd->Antenna.field.TxPath == 2)
		{
			if (pAd->ate.TxAntennaSel == 1)
			{
				TxPinCfg &= 0xFFFFFFF7;
			}
			else if (pAd->ate.TxAntennaSel == 2)
			{
				TxPinCfg &= 0xFFFFFFFD;
			}
		}

		RTMP_IO_WRITE32(pAd, TX_PIN_CFG, TxPinCfg);
	}
	else
	{
	    UINT32	TxPinCfg = 0x00050F05;// 2007.10.09 by Brian : 0x00050505 ==> 0x00050F05

		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R62, (0x37 - GET_LNA_GAIN(pAd)));
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R63, (0x37 - GET_LNA_GAIN(pAd)));
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R64, (0x37 - GET_LNA_GAIN(pAd)));
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R82, 0xF2);

        // According the Rory's suggestion to solve the middle range issue.
		ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R86, &BbpValue);// may be removed for RT35xx ++

		ASSERT((BbpValue == 0x00));
		if ((BbpValue != 0x00))
		{
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R86, 0x00);
		}

		ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R91, &BbpValue);
		ASSERT((BbpValue == 0x04));

		ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R92, &BbpValue);
		ASSERT((BbpValue == 0x00));// may be removed for RT35xx --

		// 5.5 GHz band selection PIN, bit1 and bit2 are complement
		RTMP_IO_READ32(pAd, TX_BAND_CFG, &Value);
		Value &= (~0x6);
		Value |= (0x02);
		RTMP_IO_WRITE32(pAd, TX_BAND_CFG, Value);

		// Turn off unused PA or LNA when only 1T or 1R.
		if (pAd->Antenna.field.TxPath == 1)
		{
			TxPinCfg &= 0xFFFFFFF3;
		}
		if (pAd->Antenna.field.RxPath == 1)
		{
			TxPinCfg &= 0xFFFFF3FF;
		}

		RTMP_IO_WRITE32(pAd, TX_PIN_CFG, TxPinCfg);
	}


    // R66 should be set according to Channel and use 20MHz when scanning
	if (Channel <= 14)
	{
		// BG band
		R66 = 0x2E + GET_LNA_GAIN(pAd);
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R66, R66);
	}
	else
	{
		// 5.5 GHz band
		if (pAd->ate.TxWI.BW == BW_20)
		{
			R66 = (UCHAR)(0x32 + (GET_LNA_GAIN(pAd)*5)/3);
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R66, R66);
		}
		else
		{
			R66 = (UCHAR)(0x3A + (GET_LNA_GAIN(pAd)*5)/3);
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R66, R66);
		}
	}

	/*
		On 11A, We should delay and wait RF/BBP to be stable
		and the appropriate time should be 1000 micro seconds.

		2005/06/05 - On 11G, We also need this delay time. Otherwise it's difficult to pass the WHQL.
	*/
	RTMPusecDelay(1000);

#ifndef RTMP_RF_RW_SUPPORT
	if (Channel > 14)
	{
		// When 5.5GHz band the LSB of TxPwr will be used to reduced 7dB or not.
		ATEDBGPRINT(RT_DEBUG_TRACE, ("SwitchChannel#%d(RF=%d, %dT) to , R1=0x%08lx, R2=0x%08lx, R3=0x%08lx, R4=0x%08lx\n",
								  Channel,
								  pAd->RfIcType,
								  pAd->Antenna.field.TxPath,
								  pAd->LatchRfRegs.R1,
								  pAd->LatchRfRegs.R2,
								  pAd->LatchRfRegs.R3,
								  pAd->LatchRfRegs.R4));
	}
	else
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("SwitchChannel#%d(RF=%d, Pwr0=%u, Pwr1=%u, %dT) to , R1=0x%08lx, R2=0x%08lx, R3=0x%08lx, R4=0x%08lx\n",
								  Channel,
								  pAd->RfIcType,
								  (R3 & 0x00003e00) >> 9,
								  (R4 & 0x000007c0) >> 6,
								  pAd->Antenna.field.TxPath,
								  pAd->LatchRfRegs.R1,
								  pAd->LatchRfRegs.R2,
								  pAd->LatchRfRegs.R3,
								  pAd->LatchRfRegs.R4));
    }
#endif // RTMP_RF_RW_SUPPORT //
}



/* In fact, no one will call this routine so far ! */

/*
==========================================================================
	Description:
		Gives CCK TX rate 2 more dB TX power.
		This routine works only in ATE mode.

		calculate desired Tx power in RF R3.Tx0~5,	should consider -
		0. if current radio is a noisy environment (pAd->DrsCounters.fNoisyEnvironment)
		1. TxPowerPercentage
		2. auto calibration based on TSSI feedback
		3. extra 2 db for CCK
		4. -10 db upon very-short distance (AvgRSSI >= -40db) to AP

	NOTE: Since this routine requires the value of (pAd->DrsCounters.fNoisyEnvironment),
		it should be called AFTER MlmeDynamicTxRateSwitching()
==========================================================================
*/
VOID ATEAsicAdjustTxPower(
	IN PRTMP_ADAPTER pAd)
{
	INT			i, j;
	CHAR		DeltaPwr = 0;
	BOOLEAN		bAutoTxAgc = FALSE;
	UCHAR		TssiRef, *pTssiMinusBoundary, *pTssiPlusBoundary, TxAgcStep;
	UCHAR		BbpR49 = 0, idx;
	PCHAR		pTxAgcCompensate;
	ULONG		TxPwr[5];
	CHAR		Value;

	/* no one calls this procedure so far */
	if (pAd->ate.TxWI.BW == BW_40)
	{
		if (pAd->ate.Channel > 14)
		{
			TxPwr[0] = pAd->Tx40MPwrCfgABand[0];
			TxPwr[1] = pAd->Tx40MPwrCfgABand[1];
			TxPwr[2] = pAd->Tx40MPwrCfgABand[2];
			TxPwr[3] = pAd->Tx40MPwrCfgABand[3];
			TxPwr[4] = pAd->Tx40MPwrCfgABand[4];
		}
		else
		{
			TxPwr[0] = pAd->Tx40MPwrCfgGBand[0];
			TxPwr[1] = pAd->Tx40MPwrCfgGBand[1];
			TxPwr[2] = pAd->Tx40MPwrCfgGBand[2];
			TxPwr[3] = pAd->Tx40MPwrCfgGBand[3];
			TxPwr[4] = pAd->Tx40MPwrCfgGBand[4];
		}
	}
	else
	{
		if (pAd->ate.Channel > 14)
		{
			TxPwr[0] = pAd->Tx20MPwrCfgABand[0];
			TxPwr[1] = pAd->Tx20MPwrCfgABand[1];
			TxPwr[2] = pAd->Tx20MPwrCfgABand[2];
			TxPwr[3] = pAd->Tx20MPwrCfgABand[3];
			TxPwr[4] = pAd->Tx20MPwrCfgABand[4];
		}
		else
		{
			TxPwr[0] = pAd->Tx20MPwrCfgGBand[0];
			TxPwr[1] = pAd->Tx20MPwrCfgGBand[1];
			TxPwr[2] = pAd->Tx20MPwrCfgGBand[2];
			TxPwr[3] = pAd->Tx20MPwrCfgGBand[3];
			TxPwr[4] = pAd->Tx20MPwrCfgGBand[4];
		}
	}

	// TX power compensation for temperature variation based on TSSI.
	// Do it per 4 seconds.
	if (pAd->Mlme.OneSecPeriodicRound % 4 == 0)
	{
		if (pAd->ate.Channel <= 14)
		{
			/* bg channel */
			bAutoTxAgc         = pAd->bAutoTxAgcG;
			TssiRef            = pAd->TssiRefG;
			pTssiMinusBoundary = &pAd->TssiMinusBoundaryG[0];
			pTssiPlusBoundary  = &pAd->TssiPlusBoundaryG[0];
			TxAgcStep          = pAd->TxAgcStepG;
			pTxAgcCompensate   = &pAd->TxAgcCompensateG;
		}
		else
		{
			/* a channel */
			bAutoTxAgc         = pAd->bAutoTxAgcA;
			TssiRef            = pAd->TssiRefA;
			pTssiMinusBoundary = &pAd->TssiMinusBoundaryA[0];
			pTssiPlusBoundary  = &pAd->TssiPlusBoundaryA[0];
			TxAgcStep          = pAd->TxAgcStepA;
			pTxAgcCompensate   = &pAd->TxAgcCompensateA;
		}

		if (bAutoTxAgc)
		{
			/* BbpR49 is unsigned char. */
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R49, &BbpR49);

			/* (p) TssiPlusBoundaryG[0] = 0 = (m) TssiMinusBoundaryG[0] */
			/* compensate: +4     +3   +2   +1    0   -1   -2   -3   -4 * steps */
			/* step value is defined in pAd->TxAgcStepG for tx power value */

			/* [4]+1+[4]   p4     p3   p2   p1   o1   m1   m2   m3   m4 */
			/* ex:         0x00 0x15 0x25 0x45 0x88 0xA0 0xB5 0xD0 0xF0
			   above value are examined in mass factory production */
			/*             [4]    [3]  [2]  [1]  [0]  [1]  [2]  [3]  [4] */

			/* plus is 0x10 ~ 0x40, minus is 0x60 ~ 0x90 */
			/* if value is between p1 ~ o1 or o1 ~ s1, no need to adjust tx power */
			/* if value is 0x65, tx power will be -= TxAgcStep*(2-1) */

			if (BbpR49 > pTssiMinusBoundary[1])
			{
				// Reading is larger than the reference value.
				// Check for how large we need to decrease the Tx power.
				for (idx = 1; idx < 5; idx++)
				{
					// Found the range.
					if (BbpR49 <= pTssiMinusBoundary[idx])
						break;
				}

				// The index is the step we should decrease, idx = 0 means there is nothing to compensate.
//				if (R3 > (ULONG) (TxAgcStep * (idx-1)))
					*pTxAgcCompensate = -(TxAgcStep * (idx-1));
//				else
//					*pTxAgcCompensate = -((UCHAR)R3);

				DeltaPwr += (*pTxAgcCompensate);
				ATEDBGPRINT(RT_DEBUG_TRACE, ("-- Tx Power, BBP R1=%x, TssiRef=%x, TxAgcStep=%x, step = -%d\n",
					BbpR49, TssiRef, TxAgcStep, idx-1));
			}
			else if (BbpR49 < pTssiPlusBoundary[1])
			{
				// Reading is smaller than the reference value.
				// Check for how large we need to increase the Tx power.
				for (idx = 1; idx < 5; idx++)
				{
					// Found the range.
					if (BbpR49 >= pTssiPlusBoundary[idx])
						break;
				}

				// The index is the step we should increase, idx = 0 means there is nothing to compensate.
				*pTxAgcCompensate = TxAgcStep * (idx-1);
				DeltaPwr += (*pTxAgcCompensate);
				ATEDBGPRINT(RT_DEBUG_TRACE, ("++ Tx Power, BBP R1=%x, TssiRef=%x, TxAgcStep=%x, step = +%d\n",
					BbpR49, TssiRef, TxAgcStep, idx-1));
			}
			else
			{
				*pTxAgcCompensate = 0;
				ATEDBGPRINT(RT_DEBUG_TRACE, ("   Tx Power, BBP R1=%x, TssiRef=%x, TxAgcStep=%x, step = +%d\n",
					BbpR49, TssiRef, TxAgcStep, 0));
			}
		}
	}
	else
	{
		if (pAd->ate.Channel <= 14)
		{
			bAutoTxAgc         = pAd->bAutoTxAgcG;
			pTxAgcCompensate   = &pAd->TxAgcCompensateG;
		}
		else
		{
			bAutoTxAgc         = pAd->bAutoTxAgcA;
			pTxAgcCompensate   = &pAd->TxAgcCompensateA;
		}

		if (bAutoTxAgc)
			DeltaPwr += (*pTxAgcCompensate);
	}

	/* Calculate delta power based on the percentage specified from UI. */
	// E2PROM setting is calibrated for maximum TX power (i.e. 100%)
	// We lower TX power here according to the percentage specified from UI.
	if (pAd->CommonCfg.TxPowerPercentage == 0xffffffff)       // AUTO TX POWER control
		;
	else if (pAd->CommonCfg.TxPowerPercentage > 90)  // 91 ~ 100% & AUTO, treat as 100% in terms of mW
		;
	else if (pAd->CommonCfg.TxPowerPercentage > 60)  // 61 ~ 90%, treat as 75% in terms of mW
	{
		DeltaPwr -= 1;
	}
	else if (pAd->CommonCfg.TxPowerPercentage > 30)  // 31 ~ 60%, treat as 50% in terms of mW
	{
		DeltaPwr -= 3;
	}
	else if (pAd->CommonCfg.TxPowerPercentage > 15)  // 16 ~ 30%, treat as 25% in terms of mW
	{
		DeltaPwr -= 6;
	}
	else if (pAd->CommonCfg.TxPowerPercentage > 9)   // 10 ~ 15%, treat as 12.5% in terms of mW
	{
		DeltaPwr -= 9;
	}
	else                                           // 0 ~ 9 %, treat as MIN(~3%) in terms of mW
	{
		DeltaPwr -= 12;
	}

	/* Reset different new tx power for different TX rate. */
	for (i=0; i<5; i++)
	{
		if (TxPwr[i] != 0xffffffff)
		{
			for (j=0; j<8; j++)
			{
				Value = (CHAR)((TxPwr[i] >> j*4) & 0x0F); /* 0 ~ 15 */

				if ((Value + DeltaPwr) < 0)
				{
					Value = 0; /* min */
				}
				else if ((Value + DeltaPwr) > 0xF)
				{
					Value = 0xF; /* max */
				}
				else
				{
					Value += DeltaPwr; /* temperature compensation */
				}

				/* fill new value to CSR offset */
				TxPwr[i] = (TxPwr[i] & ~(0x0000000F << j*4)) | (Value << j*4);
			}

			/* write tx power value to CSR */
			/* TX_PWR_CFG_0 (8 tx rate) for	TX power for OFDM 12M/18M
											TX power for OFDM 6M/9M
											TX power for CCK5.5M/11M
											TX power for CCK1M/2M */
			/* TX_PWR_CFG_1 ~ TX_PWR_CFG_4 */
			RTMP_IO_WRITE32(pAd, TX_PWR_CFG_0 + i*4, TxPwr[i]);


		}
	}

}


/*
========================================================================
	Routine Description:
		Write TxWI for ATE mode.

	Return Value:
		None
========================================================================
*/
#ifdef RTMP_MAC_PCI
static VOID ATEWriteTxWI(
	IN	PRTMP_ADAPTER	pAd,
	IN	PTXWI_STRUC	pOutTxWI,
	IN	BOOLEAN			FRAG,
	IN	BOOLEAN			CFACK,
	IN	BOOLEAN			InsTimestamp,
	IN	BOOLEAN			AMPDU,
	IN	BOOLEAN			Ack,
	IN	BOOLEAN			NSeq,		// HW new a sequence.
	IN	UCHAR			BASize,
	IN	UCHAR			WCID,
	IN	ULONG			Length,
	IN	UCHAR			PID,
	IN	UCHAR			TID,
	IN	UCHAR			TxRate,
	IN	UCHAR			Txopmode,
	IN	BOOLEAN			CfAck,
	IN	HTTRANSMIT_SETTING	*pTransmit)
{
	TXWI_STRUC		TxWI;
	PTXWI_STRUC	pTxWI;

	//
	// Always use Long preamble before verifiation short preamble functionality works well.
	// Todo: remove the following line if short preamble functionality works
	//
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_SHORT_PREAMBLE_INUSED);
	NdisZeroMemory(&TxWI, TXWI_SIZE);
	pTxWI = &TxWI;

	pTxWI->FRAG= FRAG;

	pTxWI->CFACK = CFACK;
	pTxWI->TS= InsTimestamp;
	pTxWI->AMPDU = AMPDU;
	pTxWI->ACK = Ack;
	pTxWI->txop= Txopmode;

	pTxWI->NSEQ = NSeq;

	// John tune the performace with Intel Client in 20 MHz performance
	if ( BASize >7 )
		BASize =7;

	pTxWI->BAWinSize = BASize;
	pTxWI->WirelessCliID = WCID;
	pTxWI->MPDUtotalByteCount = Length;
	pTxWI->PacketId = PID;

	// If CCK or OFDM, BW must be 20
	pTxWI->BW = (pTransmit->field.MODE <= MODE_OFDM) ? (BW_20) : (pTransmit->field.BW);
	pTxWI->ShortGI = pTransmit->field.ShortGI;
	pTxWI->STBC = pTransmit->field.STBC;

	pTxWI->MCS = pTransmit->field.MCS;
	pTxWI->PHYMODE = pTransmit->field.MODE;
	pTxWI->CFACK = CfAck;
	pTxWI->MIMOps = 0;
	pTxWI->MpduDensity = 0;

	pTxWI->PacketId = pTxWI->MCS;
	NdisMoveMemory(pOutTxWI, &TxWI, sizeof(TXWI_STRUC));

    return;
}
#endif // RTMP_MAC_PCI //




/*
========================================================================

	Routine Description:
		Disable protection for ATE.
========================================================================
*/
VOID ATEDisableAsicProtect(
	IN		PRTMP_ADAPTER	pAd)
{
	PROT_CFG_STRUC	ProtCfg, ProtCfg4;
	UINT32 Protect[6];
	USHORT			offset;
	UCHAR			i;
	UINT32 MacReg = 0;

	// Config ASIC RTS threshold register
	RTMP_IO_READ32(pAd, TX_RTS_CFG, &MacReg);
	MacReg &= 0xFF0000FF;
	MacReg |= (pAd->CommonCfg.RtsThreshold << 8);
	RTMP_IO_WRITE32(pAd, TX_RTS_CFG, MacReg);

	// Initial common protection settings
	RTMPZeroMemory(Protect, sizeof(Protect));
	ProtCfg4.word = 0;
	ProtCfg.word = 0;
	ProtCfg.field.TxopAllowGF40 = 1;
	ProtCfg.field.TxopAllowGF20 = 1;
	ProtCfg.field.TxopAllowMM40 = 1;
	ProtCfg.field.TxopAllowMM20 = 1;
	ProtCfg.field.TxopAllowOfdm = 1;
	ProtCfg.field.TxopAllowCck = 1;
	ProtCfg.field.RTSThEn = 1;
	ProtCfg.field.ProtectNav = ASIC_SHORTNAV;

	// Handle legacy(B/G) protection
	ProtCfg.field.ProtectRate = pAd->CommonCfg.RtsRate;
	ProtCfg.field.ProtectCtrl = 0;
	Protect[0] = ProtCfg.word;
	Protect[1] = ProtCfg.word;

	// NO PROTECT
	// 1.All STAs in the BSS are 20/40 MHz HT
	// 2. in ai 20/40MHz BSS
	// 3. all STAs are 20MHz in a 20MHz BSS
	// Pure HT. no protection.

	// MM20_PROT_CFG
	//	Reserved (31:27)
	//	PROT_TXOP(25:20) -- 010111
	//	PROT_NAV(19:18)  -- 01 (Short NAV protection)
	//  PROT_CTRL(17:16) -- 00 (None)
	//	PROT_RATE(15:0)  -- 0x4004 (OFDM 24M)
	Protect[2] = 0x01744004;

	// MM40_PROT_CFG
	//	Reserved (31:27)
	//	PROT_TXOP(25:20) -- 111111
	//	PROT_NAV(19:18)  -- 01 (Short NAV protection)
	//  PROT_CTRL(17:16) -- 00 (None)
	//	PROT_RATE(15:0)  -- 0x4084 (duplicate OFDM 24M)
	Protect[3] = 0x03f44084;

	// CF20_PROT_CFG
	//	Reserved (31:27)
	//	PROT_TXOP(25:20) -- 010111
	//	PROT_NAV(19:18)  -- 01 (Short NAV protection)
	//  PROT_CTRL(17:16) -- 00 (None)
	//	PROT_RATE(15:0)  -- 0x4004 (OFDM 24M)
	Protect[4] = 0x01744004;

	// CF40_PROT_CFG
	//	Reserved (31:27)
	//	PROT_TXOP(25:20) -- 111111
	//	PROT_NAV(19:18)  -- 01 (Short NAV protection)
	//  PROT_CTRL(17:16) -- 00 (None)
	//	PROT_RATE(15:0)  -- 0x4084 (duplicate OFDM 24M)
	Protect[5] = 0x03f44084;

	pAd->CommonCfg.IOTestParm.bRTSLongProtOn = FALSE;

	offset = CCK_PROT_CFG;
	for (i = 0;i < 6;i++)
		RTMP_IO_WRITE32(pAd, offset + i*4, Protect[i]);

}




/* There are two ways to convert Rssi */
/* the way used with GET_LNA_GAIN() */
CHAR ATEConvertToRssi(
	IN PRTMP_ADAPTER pAd,
	IN	CHAR	Rssi,
	IN  UCHAR   RssiNumber)
{
	UCHAR	RssiOffset, LNAGain;

	// Rssi equals to zero should be an invalid value
	if (Rssi == 0)
		return -99;

	LNAGain = GET_LNA_GAIN(pAd);
	if (pAd->LatchRfRegs.Channel > 14)
	{
		if (RssiNumber == 0)
			RssiOffset = pAd->ARssiOffset0;
		else if (RssiNumber == 1)
			RssiOffset = pAd->ARssiOffset1;
		else
			RssiOffset = pAd->ARssiOffset2;
	}
	else
	{
		if (RssiNumber == 0)
			RssiOffset = pAd->BGRssiOffset0;
		else if (RssiNumber == 1)
			RssiOffset = pAd->BGRssiOffset1;
		else
			RssiOffset = pAd->BGRssiOffset2;
	}

	return (-12 - RssiOffset - LNAGain - Rssi);
}


/*
========================================================================

	Routine Description:
		Set Japan filter coefficients if needed.
	Note:
		This routine should only be called when
		entering TXFRAME mode or TXCONT mode.

========================================================================
*/
static VOID SetJapanFilter(
	IN		PRTMP_ADAPTER	pAd)
{
	UCHAR			BbpData = 0;

	//
	// If Channel=14 and Bandwidth=20M and Mode=CCK, set BBP R4 bit5=1
	// (Japan Tx filter coefficients)when (TXFRAME or TXCONT).
	//
	ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R4, &BbpData);

    if ((pAd->ate.TxWI.PHYMODE == MODE_CCK) && (pAd->ate.Channel == 14) && (pAd->ate.TxWI.BW == BW_20))
    {
        BbpData |= 0x20;    // turn on
        ATEDBGPRINT(RT_DEBUG_TRACE, ("SetJapanFilter!!!\n"));
    }
    else
    {
		BbpData &= 0xdf;    // turn off
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ClearJapanFilter!!!\n"));
    }

	ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R4, BbpData);
}


VOID ATESampleRssi(
	IN PRTMP_ADAPTER	pAd,
	IN PRXWI_STRUC		pRxWI)
{
	/* There are two ways to collect RSSI. */
//	pAd->LastRxRate = (USHORT)((pRxWI->MCS) + (pRxWI->BW <<7) + (pRxWI->ShortGI <<8)+ (pRxWI->PHYMODE <<14)) ;
	if (pRxWI->RSSI0 != 0)
	{
		pAd->ate.LastRssi0	= ATEConvertToRssi(pAd, (CHAR) pRxWI->RSSI0, RSSI_0);
		pAd->ate.AvgRssi0X8	= (pAd->ate.AvgRssi0X8 - pAd->ate.AvgRssi0) + pAd->ate.LastRssi0;
		pAd->ate.AvgRssi0	= pAd->ate.AvgRssi0X8 >> 3;
	}
	if (pRxWI->RSSI1 != 0)
	{
		pAd->ate.LastRssi1	= ATEConvertToRssi(pAd, (CHAR) pRxWI->RSSI1, RSSI_1);
		pAd->ate.AvgRssi1X8	= (pAd->ate.AvgRssi1X8 - pAd->ate.AvgRssi1) + pAd->ate.LastRssi1;
		pAd->ate.AvgRssi1	= pAd->ate.AvgRssi1X8 >> 3;
	}
	if (pRxWI->RSSI2 != 0)
	{
		pAd->ate.LastRssi2	= ATEConvertToRssi(pAd, (CHAR) pRxWI->RSSI2, RSSI_2);
		pAd->ate.AvgRssi2X8	= (pAd->ate.AvgRssi2X8 - pAd->ate.AvgRssi2) + pAd->ate.LastRssi2;
		pAd->ate.AvgRssi2	= pAd->ate.AvgRssi2X8 >> 3;
	}

	pAd->ate.LastSNR0 = (CHAR)(pRxWI->SNR0);// CHAR ==> UCHAR ?
	pAd->ate.LastSNR1 = (CHAR)(pRxWI->SNR1);// CHAR ==> UCHAR ?

	pAd->ate.NumOfAvgRssiSample ++;
}


#ifdef CONFIG_STA_SUPPORT
VOID RTMPStationStop(
    IN  PRTMP_ADAPTER   pAd)
{
//	BOOLEAN       Cancelled;

    ATEDBGPRINT(RT_DEBUG_TRACE, ("==> RTMPStationStop\n"));

	// For rx statistics, we need to keep this timer running.
//	RTMPCancelTimer(&pAd->Mlme.PeriodicTimer,      &Cancelled);

    ATEDBGPRINT(RT_DEBUG_TRACE, ("<== RTMPStationStop\n"));
}


VOID RTMPStationStart(
    IN  PRTMP_ADAPTER   pAd)
{
    ATEDBGPRINT(RT_DEBUG_TRACE, ("==> RTMPStationStart\n"));

#ifdef RTMP_MAC_PCI
	pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;

	/* We did not cancel this timer when entering ATE mode. */
//	RTMPSetTimer(&pAd->Mlme.PeriodicTimer, MLME_TASK_EXEC_INTV);
#endif // RTMP_MAC_PCI //

	ATEDBGPRINT(RT_DEBUG_TRACE, ("<== RTMPStationStart\n"));
}
#endif // CONFIG_STA_SUPPORT //


/*
==========================================================================
	Description:
		Setup Frame format.
	NOTE:
		This routine should only be used in ATE mode.
==========================================================================
*/
#ifdef RTMP_MAC_PCI
static INT ATESetUpFrame(
	IN PRTMP_ADAPTER pAd,
	IN UINT32 TxIdx)
{
	UINT j;
	PTXD_STRUC pTxD;
#ifdef RT_BIG_ENDIAN
    PTXD_STRUC      pDestTxD;
    TXD_STRUC       TxD;
#endif
	PNDIS_PACKET pPacket;
	PUCHAR pDest;
	PVOID AllocVa;
	NDIS_PHYSICAL_ADDRESS AllocPa;
	HTTRANSMIT_SETTING	TxHTPhyMode;

	PRTMP_TX_RING pTxRing = &pAd->TxRing[QID_AC_BE];
	PTXWI_STRUC pTxWI = (PTXWI_STRUC) pTxRing->Cell[TxIdx].DmaBuf.AllocVa;
	PUCHAR pDMAHeaderBufVA = (PUCHAR) pTxRing->Cell[TxIdx].DmaBuf.AllocVa;

#ifdef RALINK_28xx_QA
	PHEADER_802_11	pHeader80211;
#endif // RALINK_28xx_QA //

	if (pAd->ate.bQATxStart == TRUE)
	{
		// always use QID_AC_BE and FIFO_EDCA

		// fill TxWI
		TxHTPhyMode.field.BW = pAd->ate.TxWI.BW;
		TxHTPhyMode.field.ShortGI = pAd->ate.TxWI.ShortGI;
		TxHTPhyMode.field.STBC = 0;
		TxHTPhyMode.field.MCS = pAd->ate.TxWI.MCS;
		TxHTPhyMode.field.MODE = pAd->ate.TxWI.PHYMODE;

		ATEWriteTxWI(pAd, pTxWI, pAd->ate.TxWI.FRAG, pAd->ate.TxWI.CFACK,
			pAd->ate.TxWI.TS,  pAd->ate.TxWI.AMPDU, pAd->ate.TxWI.ACK, pAd->ate.TxWI.NSEQ,
			pAd->ate.TxWI.BAWinSize, 0, pAd->ate.TxWI.MPDUtotalByteCount, pAd->ate.TxWI.PacketId, 0, 0,
			pAd->ate.TxWI.txop/*IFS_HTTXOP*/, pAd->ate.TxWI.CFACK/*FALSE*/, &TxHTPhyMode);
	}
	else
	{
		TxHTPhyMode.field.BW = pAd->ate.TxWI.BW;
		TxHTPhyMode.field.ShortGI = pAd->ate.TxWI.ShortGI;
		TxHTPhyMode.field.STBC = 0;
		TxHTPhyMode.field.MCS = pAd->ate.TxWI.MCS;
		TxHTPhyMode.field.MODE = pAd->ate.TxWI.PHYMODE;
		ATEWriteTxWI(pAd, pTxWI, FALSE, FALSE, FALSE,  FALSE, FALSE, FALSE,
			4, 0, pAd->ate.TxLength, 0, 0, 0, IFS_HTTXOP, FALSE, &TxHTPhyMode);
	}

	// fill 802.11 header
#ifdef RALINK_28xx_QA
	if (pAd->ate.bQATxStart == TRUE)
	{
		NdisMoveMemory(pDMAHeaderBufVA+TXWI_SIZE, pAd->ate.Header, pAd->ate.HLen);
	}
	else
#endif // RALINK_28xx_QA //
	{
		NdisMoveMemory(pDMAHeaderBufVA+TXWI_SIZE, TemplateFrame, LENGTH_802_11);
		NdisMoveMemory(pDMAHeaderBufVA+TXWI_SIZE+4, pAd->ate.Addr1, ETH_LENGTH_OF_ADDRESS);
		NdisMoveMemory(pDMAHeaderBufVA+TXWI_SIZE+10, pAd->ate.Addr2, ETH_LENGTH_OF_ADDRESS);
		NdisMoveMemory(pDMAHeaderBufVA+TXWI_SIZE+16, pAd->ate.Addr3, ETH_LENGTH_OF_ADDRESS);
	}

#ifdef RT_BIG_ENDIAN
	RTMPFrameEndianChange(pAd, (((PUCHAR)pDMAHeaderBufVA)+TXWI_SIZE), DIR_READ, FALSE);
#endif // RT_BIG_ENDIAN //

	/* alloc buffer for payload */
#ifdef RALINK_28xx_QA
	if (pAd->ate.bQATxStart == TRUE)
	{
		pPacket = RTMP_AllocateRxPacketBuffer(pAd, pAd->ate.DLen + 0x100, FALSE, &AllocVa, &AllocPa);
	}
	else
#endif // RALINK_28xx_QA //
	{
		pPacket = RTMP_AllocateRxPacketBuffer(pAd, pAd->ate.TxLength, FALSE, &AllocVa, &AllocPa);
	}

	if (pPacket == NULL)
	{
		pAd->ate.TxCount = 0;
		ATEDBGPRINT(RT_DEBUG_TRACE, ("%s fail to alloc packet space.\n", __FUNCTION__));
		return -1;
	}
	pTxRing->Cell[TxIdx].pNextNdisPacket = pPacket;

	pDest = (PUCHAR) AllocVa;

#ifdef RALINK_28xx_QA
	if (pAd->ate.bQATxStart == TRUE)
	{
		RTPKT_TO_OSPKT(pPacket)->len = pAd->ate.DLen;
	}
	else
#endif // RALINK_28xx_QA //
	{
		RTPKT_TO_OSPKT(pPacket)->len = pAd->ate.TxLength - LENGTH_802_11;
	}

	// prepare frame payload
#ifdef RALINK_28xx_QA
	if (pAd->ate.bQATxStart == TRUE)
	{
		// copy pattern
		if ((pAd->ate.PLen != 0))
		{
			int j;

			for (j = 0; j < pAd->ate.DLen; j+=pAd->ate.PLen)
			{
				memcpy(RTPKT_TO_OSPKT(pPacket)->data + j, pAd->ate.Pattern, pAd->ate.PLen);
			}
		}
	}
	else
#endif // RALINK_28xx_QA //
	{
		for (j = 0; j < RTPKT_TO_OSPKT(pPacket)->len; j++)
		{
			pDest[j] = 0xA5;
		}
	}

	/* build Tx Descriptor */
#ifndef RT_BIG_ENDIAN
	pTxD = (PTXD_STRUC) pTxRing->Cell[TxIdx].AllocVa;
#else
    pDestTxD  = (PTXD_STRUC)pTxRing->Cell[TxIdx].AllocVa;
    TxD = *pDestTxD;
    pTxD = &TxD;
#endif // !RT_BIG_ENDIAN //

#ifdef RALINK_28xx_QA
	if (pAd->ate.bQATxStart == TRUE)
	{
		// prepare TxD
		NdisZeroMemory(pTxD, TXD_SIZE);
		RTMPWriteTxDescriptor(pAd, pTxD, FALSE, FIFO_EDCA);
		// build TX DESC
		pTxD->SDPtr0 = RTMP_GetPhysicalAddressLow(pTxRing->Cell[TxIdx].DmaBuf.AllocPa);
		pTxD->SDLen0 = TXWI_SIZE + pAd->ate.HLen;
		pTxD->LastSec0 = 0;
		pTxD->SDPtr1 = AllocPa;
		pTxD->SDLen1 = RTPKT_TO_OSPKT(pPacket)->len;
		pTxD->LastSec1 = 1;

		pDest = (PUCHAR)pTxWI;
		pDest += TXWI_SIZE;
		pHeader80211 = (PHEADER_802_11)pDest;

		// modify sequence number...
		if (pAd->ate.TxDoneCount == 0)
		{
			pAd->ate.seq = pHeader80211->Sequence;
		}
		else
			pHeader80211->Sequence = ++pAd->ate.seq;
	}
	else
#endif // RALINK_28xx_QA //
	{
		NdisZeroMemory(pTxD, TXD_SIZE);
		RTMPWriteTxDescriptor(pAd, pTxD, FALSE, FIFO_EDCA);
		// build TX DESC
		pTxD->SDPtr0 = RTMP_GetPhysicalAddressLow (pTxRing->Cell[TxIdx].DmaBuf.AllocPa);
		pTxD->SDLen0 = TXWI_SIZE + LENGTH_802_11;
		pTxD->LastSec0 = 0;
		pTxD->SDPtr1 = AllocPa;
		pTxD->SDLen1 = RTPKT_TO_OSPKT(pPacket)->len;
		pTxD->LastSec1 = 1;
	}

#ifdef RT_BIG_ENDIAN
	RTMPWIEndianChange((PUCHAR)pTxWI, TYPE_TXWI);
	RTMPFrameEndianChange(pAd, (((PUCHAR)pDMAHeaderBufVA)+TXWI_SIZE), DIR_WRITE, FALSE);
    RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
    WriteBackToDescriptor((PUCHAR)pDestTxD, (PUCHAR)pTxD, FALSE, TYPE_TXD);
#endif // RT_BIG_ENDIAN //

	return 0;
}
/*=======================End of RTMP_MAC_PCI =======================*/
#endif // RTMP_MAC_PCI //




VOID rt_ee_read_all(PRTMP_ADAPTER pAd, USHORT *Data)
{
	USHORT i;
	USHORT value;


	for (i = 0 ; i < EEPROM_SIZE/2 ; )
	{
		/* "value" is especially for some compilers... */
		RT28xx_EEPROM_READ16(pAd, i*2, value);
		Data[i] = value;
		i++;
	}
}


VOID rt_ee_write_all(PRTMP_ADAPTER pAd, USHORT *Data)
{
	USHORT i;
	USHORT value;


	for (i = 0 ; i < EEPROM_SIZE/2 ; )
	{
		/* "value" is especially for some compilers... */
		value = Data[i];
		RT28xx_EEPROM_WRITE16(pAd, i*2, value);
		i++;
	}
}


#ifdef RALINK_28xx_QA
VOID ATE_QA_Statistics(
	IN PRTMP_ADAPTER			pAd,
	IN PRXWI_STRUC				pRxWI,
	IN PRT28XX_RXD_STRUC		pRxD,
	IN PHEADER_802_11			pHeader)
{
	// update counter first
	if (pHeader != NULL)
	{
		if (pHeader->FC.Type == BTYPE_DATA)
		{
			if (pRxD->U2M)
				pAd->ate.U2M++;
			else
				pAd->ate.OtherData++;
		}
		else if (pHeader->FC.Type == BTYPE_MGMT)
		{
			if (pHeader->FC.SubType == SUBTYPE_BEACON)
				pAd->ate.Beacon++;
			else
				pAd->ate.OtherCount++;
		}
		else if (pHeader->FC.Type == BTYPE_CNTL)
		{
			pAd->ate.OtherCount++;
		}
	}
	pAd->ate.RSSI0 = pRxWI->RSSI0;
	pAd->ate.RSSI1 = pRxWI->RSSI1;
	pAd->ate.RSSI2 = pRxWI->RSSI2;
	pAd->ate.SNR0 = pRxWI->SNR0;
	pAd->ate.SNR1 = pRxWI->SNR1;
}


/* command id with Cmd Type == 0x0008(for 28xx)/0x0005(for iNIC) */
#define RACFG_CMD_RF_WRITE_ALL		0x0000
#define RACFG_CMD_E2PROM_READ16		0x0001
#define RACFG_CMD_E2PROM_WRITE16	0x0002
#define RACFG_CMD_E2PROM_READ_ALL	0x0003
#define RACFG_CMD_E2PROM_WRITE_ALL	0x0004
#define RACFG_CMD_IO_READ			0x0005
#define RACFG_CMD_IO_WRITE			0x0006
#define RACFG_CMD_IO_READ_BULK		0x0007
#define RACFG_CMD_BBP_READ8			0x0008
#define RACFG_CMD_BBP_WRITE8		0x0009
#define RACFG_CMD_BBP_READ_ALL		0x000a
#define RACFG_CMD_GET_COUNTER		0x000b
#define RACFG_CMD_CLEAR_COUNTER		0x000c

#define RACFG_CMD_RSV1				0x000d
#define RACFG_CMD_RSV2				0x000e
#define RACFG_CMD_RSV3				0x000f

#define RACFG_CMD_TX_START			0x0010
#define RACFG_CMD_GET_TX_STATUS		0x0011
#define RACFG_CMD_TX_STOP			0x0012
#define RACFG_CMD_RX_START			0x0013
#define RACFG_CMD_RX_STOP			0x0014
#define RACFG_CMD_GET_NOISE_LEVEL	0x0015

#define RACFG_CMD_ATE_START			0x0080
#define RACFG_CMD_ATE_STOP			0x0081

#define RACFG_CMD_ATE_START_TX_CARRIER		0x0100
#define RACFG_CMD_ATE_START_TX_CONT			0x0101
#define RACFG_CMD_ATE_START_TX_FRAME		0x0102
#define RACFG_CMD_ATE_SET_BW	            0x0103
#define RACFG_CMD_ATE_SET_TX_POWER0	        0x0104
#define RACFG_CMD_ATE_SET_TX_POWER1			0x0105
#define RACFG_CMD_ATE_SET_FREQ_OFFSET		0/*
 6***********************GET_STATISTICS*******7***********************RE****COUNTER	*******8*************************L_TX_ANTENNA*******9 Hsinchu County 302,
 * TaiRan, R.O.C.
 *
 *a***************************PREAMBLE City,
 b***************************CHANNEL City,
 c***************************ADDR1	 City,
 d under the terms of the GNU Gen2ral Publie under the terms of the GNU Gen3ral Publif***************************RATtwar*****10***************************TX_FRAME_LENtion) a1y later version.                     Jhubion) a2************************TARTalin     tion) a3***********************E2PROM_READ_BULKion) a4ram is distributed in the hopeWRITEt it will b************************IOUT ANY WARRAion) a************************BBPe that it wion) ac.
 * 5F., No.36, TaiyuaFITNT ANY WARRANTY; * Hsinchu County 302,
 *RFNESS FOR A PARTIC (c) Copyright 2002-2007RFarranty of        a


static VOID memcpy_exl(PRTMP_ADAPTER pAd, UCHAR *dst      *
 src, ULONG len);                      s                        *
 * You should have received a copy of the      IOe that it                         *
 * You should have INT32ived a 


e to ttmpDoAte(
	IN	             	pAdapter,     struct iwreq	*wrq)
{
	USHORT Command_Id;    T	Status = NDIS
 * RUS_SUCCESS;
- Suite ate_racfghdr *pRaCfg    	if ((       = kmalloc(sizeof(                   ), GFP_KERNEL)) == NULL)
	{
	        *
-EINVAL;
		return;
	}

	NdisZeroMemory       , ***************************            copy_from_user((P    *)ifdef RAwrq->u.data.poine Pl[ATE_BBP_REG_length)******************FAULT*
 *kfree       )*
 */

#include "-1307, USA = ntohs       ->c1307, Uid    	ATEDBGPRINT(RT_DEBUG_TRACE,("\n%s:1-1307, USA = 0x%04x !\n", __FUNCTION__,1-1307, USAefine	switch (2,0xAA,0xBB******/* We will get this x00,0x0 when QA starts. */
		cas                     :
	********=DO_                   (Temple Plwrq,a, Length:2	break0xCCP_RF_REGS RF2850RegTable[];
eeithern UCgTablosed or    d 10,killed by 68
UM_OF_2850_CHNL;

extern FREQOPCY_ITEM FreqItems3020[];
externOPHAR NUM_OF_3020_CHNL;




static CHA50_CHNL;

exter         ALLCY_ITEM FreqItems3020[];
 4, 5, 6, 7,ode. */
static CHAR HTMIXRateTable[] = {0, 1, 2, 3,he hope tha16CY_ITEM FreqItems3020[];
ADAPTER pAd);* HT Mix Mode. */

static INT TxDmaBusy(
	IN PRTMP_ADAPTERT ANY);

static INT RxDmaBusy(
	IN PRTMd,
	IN * HT Mix Mode. */

static INT TxDmaBusy(
	IN PRTMP_ADAPTER pAd, 7, 8, 9, 10, 11, 12, 13, 14(
	IN PRTMP_ADATER pAd);

static VOID RtmpDmaEnable(
	IN PRTMP_ADAPTER pAd,
	I_ADAPTER pAd);

static INT ATESetUpF5, -1}; /* HT Mix Mode. */

static INT TxDmaBusy(
	IN PRTMP_       CY_ITEM FreqItems3020[];
       

static INT ATECmdHandler(
	IN	PRTMP_ADAPTER	pAd,
	INT ANYRING			arg);

#ifndef RT30xxT ANY

static INT ATECmdHandler(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTt it RING			arg);

#ifndef RT30xx
sta      de. */
static CHAR HTMIXRateTable[] = {0, 1, 2, 3,FITNESS 8CY_ITEM FreqItems3020[];
MP_ADAPTEfdef RTMP_MAC_PCI
static VOID ATEriteTxWI(
	IN	PRTMP_AT ANYER	pAd,
	IN	PTXWI_STRUC	pOutTxInsTimfdef RTMP_MAC_PCI
static VOID ATEWriteTxWI(
	IN	PRTMP_ADAPT, 7, 8, 9, 10, 11, 12, 13, 14nce.
	IN	UCHfdef RTMP_MAC_PCI
static VOID ATEWriteTxWI(
	IN	PRTin the hope that it CY_ITEM FreqItems3020[];
extehe hope that it ONG			Length,
	IN	UCHAR			PID,
	IN	UCHAR			TID,
	IN	UCHAR			TxT ANY WARR	UCHAR			Txopmode,
	IN	BOOLEAN			CfAT ANY WARRONG			Length,
	IN	UCHAR			PID,
	IN	UCHAR			TID,
	IN	UCH warranty of CY_ITEM FreqItems3020[];
exte warranty of ONG			Length,
	IN	UCHAR			PID,
	IN	UCHAR			TID,
	IN	UCHFITNESS FOR ACY_ITEM FreqItems3020[];
exteFITNESS FOR A	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RA
	IN	PRTMP_ADAPTER	pAdapter,
	IN	structtatic inline Ifdef RTMP_MAC_PCI
static VOID ATEW	CFACK,
	IN	BOOLEA***
NOIS    VE, 8, 9, 10, 11, 12, 13, 14_racfghdr *pRaCfdef RTMP_MAC_PCI
static VOID ATEWriteTxWI(
	IN	PRT***
 Jhubeifg
);

static inline INT DO_R Jhubeifdef RTMP_MAC_PCI
static VOID ATEWriteTxWI(
	IN	PRTCLEARreq	*wrq,
	IN struct ate_racfghdrN	PRTMP_ADAPTfdef RTMP_MAC_PCI
static VOID ATEWriteTxWI(
	IN	PRTTXREQUENCY_ITEM FreqItems3020[];
 DO_RACFPROM_READ16(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwr DO_RATUSfg
);

static inline INT DO_Re_racfghdN struct ate_racfghdr *pRaCfg
);

static inline INT DO_R1, 2, 3, 4, 5, 6, 7, -1}; /,
	IN	sode. */
static CHAR HTMIXRateTable[] = {0, 1, 2, 3, DO_RACFG_CMD_E2PROM_READ_ALL
(
	D_IO_REA_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_IO_1, 2, 3, 4, 5, 6, 7, -1}; /g
);

sHAR NUM_OF_3020_CHNL;




static CHAR CThe following 50_Cs artrucr new ATE GUI(not QA)M_OF_28/*=static inline INT DO_RACFG_CMD_IO_READ_BULK(
	IN	OF_2850_CHNL;

extern FREQUENructCARRIrq,
	IN struct ate_racfghdrr,
	IN	struct iwreq	ONG			Length,
	IN	UCHAR			PID,
	IN	UCHAR			TID,
	IN	UCHIN	struct ONNCY_ITEM FreqItems3020[];
extern UCapter,
	tic inline INT DO_RACFG_CMD_BBP_READ8(
	IN	PRTMP_ADAPTER	pAdapte     IN	struct iwreq	*wrq,
	IN struct ate_r     tic inline INT DO_RACFG_CMD_BBP_READ8(
	IN	PRTMP_ADAPTERET_BWCY_ITEM FreqItems3020[];
exterACFG_N struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFTX_POWER0CMD_BBP_READ_ALL(
	IN	PRTMP_ADAPTpRaCfg
);	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
)1

static inline INT DO_RACFG_CMD_GET_NOIS1N struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACF***********t ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_GET_COUNTER(
	IN	PRTMP_ADAPTER	pAdapter***
 * Ralink CY_ITEM FreqItems3020[];
exte***
 * Ralink ONG			Length,
	IN	UCHAR			PID,
	IN	UCHAR			TID,
	IN	UCHn St., JhubeiCY_ITEM FreqItems3020[];
exten St., JhubeiN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RAaiwan, R.O.CCMD_BBP_READ_ALL(
	IN	PRTMP_ADAaiwan, R.O.C	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INlink Techn_CMD_GET_TX_STATUS(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaC free softCMD_BBP_READ_ALL(
	IN	PRTMP_ADAPTree softN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACF modifyCMD_BBP_READ_ALL(
	IN	PRTMP_ADAPT modifyN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACF GeneCMD_BBP_READ_ALL(
	IN	PRTMP_ADAPT Geneinline INT DO_RACFG_CMD_RX_STOP(
	IN	PRTMP_ADAPTER	pAdapter,
	I2	struct iwreq	*wrq,
	IN struct ate_ra2inline INT DO_RACFG_CMD_RX_STOP(
	IN	PRTMP_ADAPTER	pAdapter,
	I3	struct iwreq	*wrq,
	IN struct ate_ra3N struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFour CMD_BBP_READ_ALL(
	IN	PRTMP_ADAPTour 	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRa         

static inline INT DO_RACFG_CMD_GET         O_RACFG_CMD_ATE_START_TX_CONT(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struc Jhubwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg Jhubtic inline INT DO_RACFG_CMD_BBP_READ8(
	IN	PRTMP_ADAPTER	pAda  *
 * TIN	struct iwreq	*wrq,
	IN struct at  *
 * TIN	BOOLEAN			FRAG,
	IN	BOOLEAN			CFdefaultCY_ITLEAN			C}ne ATEASSE	IN	      !*******ine ATE_BBPATE_SET_TX_POWE
	 ATEpe:Data, Length
*/

#incl}        BubbleSort(.,    n, .,    a[]n, MA.,    k, j, tempreq	stru(kame[-1;  k>0_POW--rn RTMPD_ATEjB,0x; j<k; j++)
	*****     a[j] > a[j+1]N	sttruct 	RACF =,
	I]




	 *pR=
	IN saCfg
);

+1]=RACFG_			}
MD_AT}IN struct CalNoiseLeve                         *
channelfg
);

sRSSI[3][10tic inline 		
	IN0,,
	IN1aCfg
)2;
	   *		Rssi0Offset, INT 1O_RACFG_CMD_2O_RACF;
	    *		BbpR50INT DB,0x, ADAPT1CMD_Aapter,
	IN	2TATISR	pAdIN	PRTMP_Org(
	I66valueapter,fghdr *p9aCfg
);

static in70aCfg
);

st_REGstruct a 0211		LNA_Gainstruct .,    	strucER	pAdt ate_racfghdCct iwr =    ->ate.ruct at	IN	PRTMP_	strER	pVCfg
);

sta_RACFINT DO_RA00,0xFF(
	IN       8_BY_REG_Iatic , FITNE66, &fghdr *pRaCfg
gth:TENNA(
	IN	PRTMP_ADAPTER	pAdapter,
	IN9struct iwreine IN,
	IN struct ate_racfghdr *pRaCfg
);

sta70struct iwrD_ATE_Rwreq	//* iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DOruct Read the aCfg
)of LNA gR	pA];
eINT  o_RACFruct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RART28xx_EE PRTMP_ADAPTER , NT DO_RDAPT******,line INT D	struct stru
	IN	 PRTMP      uct iwr <= 14rn RTMPDAPTER	pAdaine INT DO& 0x00FFc CHAnline INT DO_RACFG_CMD_ATE_SET_CH
	IN_BGEL(
	IN	PG_CMD_ATE_Sgth:2INT DO_RACF =FG_CMD_ATE_SE;

static 
	IN	sATE_GET = (q	*wrq,
	IN struFF00) >> 8te_raline INT DO_RACFG_CMD_ATE(ET_ADDR1(
	IN	PRTMP_A + 2)/* 0x48 */DAPTER	pAdapter,
	IN	sSTICS(
wreq	*wrq,
	IN struct ate_}
	elseruct ate_racfghdr( *pRaCfg
);

sne INT DO_RAinline INT DO_RACFG_CMD_ATE_SET_ADDR1(
	INEL(
	IN	PPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_ATE_SET_ADDR2(
	IN	PRTMP_ADAPTER	NEL(
	IN
	IN	structCiwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inlinect iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO_RA****acfghdr *pRaCfg
 =ruct iwrte_rATEAsicS0x00,ruct atfdefgth:2mdelay(5wreq		OUNTER(
x1eq	*IN struct atInsTim_ADAPTER	pAdapter,
	IN	st_REGgth:2pter,
	IN4struct iwreq	*wrq,
	IN struct ate_racfghdr *9RaCfg
);

static inline INT DO_RACFG_CMD_ATE_START_RX_FRAME(
	INAPTECfg
);

sP_ADAPTER	pAda//CHAR N Rxtatic inlinebQARxS INT = TRUEtrucSetfghdrProcMD_ATE"RX     "R	pAdaP_ADAPTER	pAdaADAPTER	pAdap < 1Adap	IN	struct TENNA(
	IN	PRTMP_ADAPTER	pAdapter,
	I5PTERADAPTER	pAd;




Cfg
);

static inline INT DO_RACFG_CMD1ATE_E2PRstructTE_BULK(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	str2ATE_E2PR,
	IN R	pAdastruct i1ITE_G_CM// calculate,
	IN 0uct iwreADAPTER	pAdap= 0truct ate_rr *pR****10strucD_ATEe INT Dreq	*wrq,
	IN stdr *pR)(-12 -,
	IN	ER	pAda-MBLETER	pA-MP_ADDO_RACF;




racfg
	IN 0]	*wr=,
	INTX_ANt iwreracfghAntenna.field.RxPath >= 2 ) // 2R*pRaCfg
);ULK(
	IN	PRTMP_ADA1Cfg
)pAdapter,struct iuct iwret ate_rR	pAdt iwruct ate_DAPTERghdr *pRaRTMP_ADAPTER	pAd inline INT DO_RACFstruct TE_BBP_READ_BULK(ATE_GET;




N	struc
	IN 1ter,
	IN	st1G_CMD_At iwreq	*wrq,
	IN struct ate_racfghdr 3pRaCfg3);

static inline INT DO_RACF2_CMD_ATE_BBP_W,
	IN stK(
	IN	PR RTMP strapter,
	INt iwreq	*O_CFG, &G inline INT DO_RACF,
	IN sTE_BBP_READ_BULK(STICS(
WRITE_B RTMP_2ter,
	IN	stic iMD_ATE_S inline op DO_RA	IN	PRTMP_ADAPTER	pAd;

s,
	IN	struct iwreq	*wate_racfghd1RaCfg
)[0]); inl1Raticreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

ruct A_GLO_CFG_STRUC GloC1]gth:2taticBusy(
	IN PRTMP_ADAPTER pAd)
{
	INT result;
	Wisable DMA
	if (GloCfg.fi2ld.RxDMADMABuic inline INT DO_RAN struct at
	IN sT_TX_FRAME_COUNT(
	IN	PR	pAdarestore original_SET_PO_RACF iwreq	*wrq,
	IN struct ate_racfghdr *pRauct iwreq	*wrq,
	STRUC GloCfg;

	value = Enable > 0 ? 1 : 0;9static inline INs in busy mode.
	WaitCnt = 0;

	while (TxDmaBAPTE	pAdapter,
	IN	stru*wrq,
	IN stBOOLEAN SyncTxRxConfig                      02111PTER	p      *
xDmaBu, MA    *
tfghdr0, bbp_OUNTER(
	I      C GlO;

st//

// 8TENNA(
	IN	PRTMP_ADAPTER	pAdapteble DMA
&ue;
	Glogth:ine INT DO_RA     MP_IO_WRITE32(pAd, WPDMA_GLO_CFG, GloCfg.word);	// 
P_RFc_GLOrm a
	IN	OF_2FG_CMD_ue;
	GloCf=fg.fiel0xCC,0x00,0xPTER	prn RTMP50_CHFITNEN	stru/* Need to synchronize tx_PCI /guration with legacyUC GM_OF_28	DMA = bpSoftRese& ((1 << 4) | pData)3)	struc1 iwrNT DO3lt;
	str,0x00,0xtmpN	st	strtatic iIN	strBBP R1 bit[4:3,
	I2 :: Both DACsEGS RFbetati*/
stQAM_OF_28	struct	*wrq RTMP_AREG_IAllY_REG_Iatic inlineTx	IN strSDO_RAstrucP_R21, &ITE8LEAN			CFREG_ID(pAd, BBP_R21, BbpDat0);

DAC 0IO_READ8_BY_REG_ID(pAd, BBP_R21, &BbpDa;

staBbpDatIN str one&= ~(0x00000001); //set bit0=0
	Aic IN_IO_WRITE8__BY_REG_ID(pAd, BBP_R21, BbpData);

	1eturn;
}1IO_READ8_BY_REG_ID(pAd, BBP_R21, &BbpDaN	struR pAd)
{
	// two&= ~(0x00000001); //set bit0=0
	Ault;
_RF_IO_WRITE32(pAd, pIO_WRITE;

static i_RF_IO_WRITE,0xFF,0xFF,0xFF,0xFF,0xF ("%s -- Sth. wrong!  : /

#in FALSE; ,0x56,0x00,0x11,0efin
	RTMPusecDela
	RTMP_RF_IO_chRfRegracfgstaticAC_PBbpData = aticHAR BbpData =_RACFG// Soft reset, set BBP r21 bit0=1->0
	ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R21, &BbpData)1
	BbpData 0= 0x00003001;t bit0=1
	ATE_BP_IO_WRITE8_BY_REG_ID(pAd, BB3_R21,1:0,
	I3);

ta &ADP_IO_READ8_BY_REG_ID(pAd, BBP_R21, &BbpDa_RACFGBbpData &= ~(0x00000001); R/set bit0=0
	ATE_BBP_IO_WRITE8_BY_REG_ID(pAdchRfReP_RF_IO_WRITE32(pAd, preturAD
}


static VOID RtmpR,chRfReunless_ATE__IO_WRITE32 BbpData_ID(poWrite(
	IN PRTMP_ADAPTER pAd)
{
	// Set RF value 1's set E32(pAd, (pAd->ic INT_ID(pAd,  BBP_R21, &BbpData);
	BbpData |= 0x000 Set R //se //


#sy)
	DMA ==IN	sult;
	WP	*wrq,
	IAd, D

stat >LatchRfRegs.R1);
	RTMP_RF_IO&= ~(0x00RF_IO_WRITE32(pAd, (pAd->Latch;
}


s_RF_IO_WRITE32(pAd, pAd->LatchRfRegRITE32(pAd, pRF_IORfReTE32(pAd, pAd->LatchRfRegs.R2);
	RTMP_RF_IO_WRITE32(pAd, (pAd->LatchRfRegs.R3 & E32(pAd, (pAd->P_RF_IO_WRITE32(pAd, pAd->le;
			break;
		case 2:
		ca);

RfRe2IO_READ8_BY_REG_ID(pAd, BBP_R21, &BbpData);
	BbpDatpAd, (pAhre70)
#endif // RT30xx //
#ifndef RTet bit0=1RITE32(pAd, pAd->LatchRfRegs.R4);

	RTMPusecDelay(200);

	// Set RERRORue 2's setImpossible= [1]
	RTMP_RF_IO_WRITE32(pAd, pAd->LatchRfRegs.R1);
	RTMP_RF_IO_WRITE32(pAd, pAd->LatchRfRegs.R2)Set ne ATEhRfRegs.R4);

			return 0;
		i++;
	}

	return -1;
}


stat R3[bit2] = [1]
	RTMP_RF_IO_WRITE32(pAd, pAd->LatcfRegs.R1);
	RTMP_RF_IO_WRI	// a
	RTMP_D_BULKN st                                              *
 * You should have received , MA receii, ATE_SEL_TX_er0/1 a*pD You*pSrcq	*wrq,
 *pAPTERp8 = schrone noAd, r
			ar) ds
	INynchlse
#endif //n 0;
CMD_ATEid->La; i < (len/4); i	IN	sRTMP_RFF}; /lignment issue, we noft a variable "INT D"M_OF_28memmove(&INT D, ynch, 4RF vaINT DO_Rhtonl(Adapter,
		if (TxPe not ower > 3{

			e no++sed bSrcBP 94}       len %);
	!uct iwRTMP_RFwish that itEGS RFnever reach hert RF va	if (TxPower > 31)
			if (TxPow

				// R3, R4 can't large than 31 (0x24), 31 ~ 36f (TxPower < T_FREQcopy of the GNU General Public License     *
 * along with this program; if Power0/1 anronized.
		 not synchro
	}
	else RALINK_28xx_	TxPower = index = 0 ? pAd->2te.TxPower0 :han 31 (0x24),1)
			RACF		*((// disa*)0 ~ )3, R4 cs(TxPower=%d, R=%ldused by BB=ult;
				Rwer, MP_M				if (TxP2wer <= 36)
		INT(RT_DEBUG_TRACE, ,
	INP 94
				R = 0;
		the                         *
 * Free Software Foundation, Inc.,        , MA .,    nd real ower.,     0 ~ 31
				R = (ULONG)(less tha// RALINK_28xx_QASSERT((Tx	TxPower = index == 0 ? pAd->ate.TxPower0 :the         3q	*wr,e
#endi)1)
			ower >

				// R3, R4 can't large than 31 (0x24), 31 ~ 36 used by BBP 94
				R = 31;
*wrq,
	IN stINT 	IN	TxStopMP_ADA           *
 * 59 Temlace -PSTRINGg
);rg{

		xFF,0xFF,0xFF,0xFF,0xFF,0xFF,ON__, TxPower, \n"B,0xCC    	IN	PRTMP_ADAPTER	pT pAd)
{ 36)
		art == TRUE) // abort all TRTMP_RF_IO_WRITT_FREQNCTION__E2PRPower, R));
		}
//2008/09/10:KH adds to support 3070 ATE TX Power tunning real time<--
PUCHAR)&RFV_RF_RW_SUPPORT
		if (IS_RT30xx(pR pAd)
{
			// Set Tx Power
			ATE_RF_IO_READ8_BY_REG_ID(pAd, R#ifdefay(2RF_R12, (EE_CMDwer, R));
		}
//2008/09/10:KH adds to support 3070 A// disabuffer[NT DO_RSIZE/2inliower=%d,FG_CNCTIireq	*t_ee_r RFVal(
	IN	{
	wer=%d, f (pAd"%s ghdrf (pAdp94 = BBPR94_DEFAULT;>ate.Channel te.TxPower0 :    print(****_EMERG "%4.4x "t sy"%s (     (i+1) % 16*****t iwre			R |= (pAd->LatchRfR_RF_r, R,R = 31;Start == TRUE) || (_, TxPowerWriteValue));
		}
		else
#endif // RTMP_RF_RW_SUPPORT //
		{
		iPTER	p = valaCfg
;
s to sup p strar    	while ((*p2er <':') && .R4 = R;
\0'G_TRACE,p2// shift     R4 ==R;
			value;
2Hex pAd)
{,Ad->{
			SE)
		aCfg
,				+5)
			{
e INT DO_RA == 0)
				{
f (inde		if (bPchRfReg>=E_SET_CHanne<< 9;
				R |= (pAd->LatchRfR_racfghcan hdr excoft  position
	(d, WP12,0x),0x56 position
		*
 */

#inY_REG_ID(pA inline INT DO_RtReset(
	INLO_CFG, Gl		break;
	}

	RTer control to correBBP, RFValue));
		}
		else
#endif // RTMP_RF_RW_SUPPORT //
	GloCfg.fie = valPTER	p00,0xE)
			{
				if (indeld.EnableRxDMA = value;
	RTMP_IO_WRITE32(pAd, WPDMA_GLO_CFG, Gl & DO_RACFG/ abort all TX rings
	RTMPusecDelay(5000);

	return;
}					// shift
				R |= (pAd->LatchRfRex,0x56 control to correct RF(R4) register bi RF(R4) register bit position
				R = R << 6;
				R |= (pAd->LatchRfRegs.Rffff83f);
				pAd->La->LatchRfRegLatchRfRegs.R4 = R;
			}
		}
		else /* 5.5GHz */
		{
			if (bPowerReduce == FALSE)
			{
				if (index == 0)
				{
					// shift TX power control to correct RF(R3) reableRxDMA = value;
	RTMP_IO_Wq,
	IN struct ate_racf bit posi			// shift TX power control to coegs.R4 = (R & (~(1 << 6)));
				}
			}
		}t TX power control to correRF RF(R4) register bit position
				R = R << 6;
				R |= (pf83f);
			, p3, p4't less thR;

s2, R3, R4R = (		pAd->LatchRfRegs.R4 = R;
			}
		}
		else /* 5.5GHz */
		{
			if (bPower R;
			O_READ8_BY_REG_ID
	p3e_ra				/LatchRfRes.R43= R;
			}
		}
	      /* 5.5GHz */3		{
			if (bPow        2 Mode
        1. ATE4e_ra3 = Stop ATE Mgs.R44= R;
			}
		}
	 = Tra/* 5.5GHz */4		{
			if (bPow = Transm Mode
        1. A83f);
		====f (inde       ====				// sh       ====     ype of Tran4====			// s inlMP4, 1);
	}
//lse
			{R     p Receiving Frames
#endif("%s p Receiving Frames
#endifendifp Receiving Frames
#endif{

	Start == TRUE) ||#endifaCfgDBG //===========RALINK_ine IQA========__FUNCT==============********	LEN_OF_ARG 16
**********ESPONSE_TO_acfg__ifdef RA__p_302 __L30xx NG		      )b = CC			\
	ER	pAd,
	)->T30xx , BBP_R94(			arg)
));	Value = ue = 0;
	UCHAR			Bbp       *
NT32			MaINT32		 0;
	PTXD_STRUC		pTxD;
stonndif // RT30xx 				******	UCHAR			Bbpmagic_no) +pTxRing = &pAd->TxRix00,0x00type) 0;
US_SUCBE];
	NDIS_STATUS		Status = NidC_BE];
	NDIS_STATUS		StT30xx /= 0;
S_SUCCESS;
#ifdef	RT_BIG_ENsequenceC_BE[24] =	UCHAR			BbpData;
 0;
	\ ATE TX Power tunning real ti ("
#endif // RT30xx  = %d,0x56UC		pRxD;
	PRTMP_TX_RINemode\t iwreqREG_to168
UCHC		pRxD;
	PRTMP_NUM]={0};(    *
 )	UCHAR			Bon */
	AsicLockChannel(pAd,Ad->{
	PTXD_STRUC	(pAd, MAC__STRxFF,0xFF,0xFF,0xFF,0xrn -1;
}
Channel);

	R) fail in %sRITE32(pAd, pAd->Late = 0;
READ8_BY( Header 0;
	PTXD_STRUC	e = 0;
}MP_IO_READ32(pAd, MAC_SYS_ INTP_IO_READ32(pAd, MAC_SYS_TMP_IO_READ32(pAd, MAC_SYS_CTRL, &MacData);

	// F value 2's is dSet 4,0x56,0x00,0x11,0emode = 0;est.
				R = inl****    Items3020[];
extern UCH           *
 * 59 Temple Place - Suite 330, Bostolace                              070 ATE TX Power tunning real timeN	PRTMP_ADAPTER	pAd_RF_RW_SU/* Prep	IN seedback as soon
		pweR |= to avoidpRattimeoutM_OF_2	IN	PRTMP_ADAPTEore_BBP[ATERALINK_AT {0x08,0index;),
 *                 hRfR	IN	PRTMP_ADAPTEple Pl "ATE     ,
	IN	o corre *                   94
				R = NUX) || de  6, 7, -1}; /* OFDM Mok if we have removed the firmware
		if (!(ATE_ON(pAd)))
		{
			NICEraseFirmware(pAd);
		}.,    rffff83f#endif // defined(LINUX) || defined(VXWORKS)OP

		atemode
		Distingubp94 =gTable[];
ecame NUM	 QA(via     agen{
	U	}; /* O_READ accordreq	to_ATE_existRINTREAMpid0x0.payload.TMP_No(pAd->to p pAd->ate.Mode;
ifTX set m22 bit7directly7=0
		G_ID(pAd,

#ihdr =0
			A.

	UCH {0x08,0* empty f[24] = {0x08,0> ATECmdRfRegs.sion set BBP R24	pTxRing set BBP ghdr *AtePi = value;	RTMP_T set BBP R22 bit7=0
			A.STRUG850Reea &= ofate_r_READBP_IPower - cpy(ead MAC_S&pData);
			BbpData &

#ifde	(& {0x08,0_REGCfg; - 2/*BP_R24, &Bbpf (atemode == iwreSTRUC  24, &BbpData);
			BbpData &= c CHAR C= pAd->ate.Mode;
		pAd->ate.Mode = ATE_START;
		RTMP_IO_WRITTE32(pAd, MAC_SYS_CTRL, MacData);

		if (atemode == ATE_TXCARR)
		{
			// NoFFFFFFE; KS RFID(pAd, Bexternleavreq	te_rmode // s	We mustMode. TX set BBPfirst befnt;
seto Co				CARRTOP, or MicrosofR94_DEFreport sR3[bit2] BP_R24, BrRegs.KILL_TH thatP	pAdap		We should free s SIGTERM15)
		ffc1ff)r
{
	Ur *pRaCfg;
		i++;
	}

	return -1;
}


: unanneltoY_REG_G_IDn -1anctionR22, BbpDanet_dev->nameION__,nable(mode AP/STA might have0x0.E_TXCARRATE_B du_SIZEG_ID  ATE_BBatic reduce 7dB. */ple P= 0xFFFFFF SomeSet ha Mode. */G_ID(pAd, hRfRegQAracfDEBUstREG_openM_OF_28Carrier Test set BBP R22 bit7d)
{
	 "ATESTART"))
	{
		ATEDBGPRINT(R***********PER	pAdDEBUG_TRACE, by BBP 6=0, bit[5~0]=0x0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &Bb, 15, -1}; /*k if we have removed the firmware
		if (!(ATE_ON(pAd)))
		{
			NICEraseFirmware(pAd);
		}======================
14)
		{
CFG_CMD_A
BbpData)&====rce which wa-236 used			}

			====ce which wa+n pNdisPacket as ==== after clear6 pNdisPacket as XSTO after clearTRUCNdisPacket asuct iwreqt = pTxRing->C4, ("%s(
	INta);
		LatchRfRegs.Rt iw[24]l(f // R					PCI_UNMAP_SINGLE(p strpTxD->S("%s 					PCI_UNMAP_SINGLE(pSTOPpTxD->Sendif					PCI_UNMAP_SINGLE(pier
pTxD->S{

							PCI_UNMAP_SINGLE(ruct ate_r[24] =uct iwr= Stop Receiving Frames
#en BBP R2					PCI_UNMAP_SINGLE(pAers are OK, FALSE otherwissPacket = NULL;
#ifdef RT_BIG_rn:
        TRUE if all parsPacket = NULL;
#ifdef RT_BIG_ters are OK, FALSE otherwissPacket = NULL;
#ifdef RT_BIG_======G pTxRing = &pAd->TxRing[QID_AC_BE];

			if (atemode == ATE_TXCONT)
			{
				o corre[5~0]=0x0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbIN PRTMP_ADAPTPCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PACKET16	PTER	p=R4 & 0xf=
	IN	PRTMP xDMAte.A
i2 = 0;24 bit0=0
			ATE_ode == temode "tmp"DEBUespecial22, or someablepilers..O_WRITEline INT DO_RACFG_CMD_A BBP R2	{
				iP_IO;
	aCfg
);
tCFG_CaCfg
);
BP_R94,
	IN	stru BbpData);
		}
		else if (ateNT DO_G_CMD_chRfRegs.R12,0xLatchRfB,0x12,0x,0x56TX power contrdisPacket at = pTxRing-,					//)
				{
E32(pAd, MAC_SYS_CTRL, MacData);

		if (atemode == r
		.
		pAd->ate.LastSNR0 = 0;
		pA->ate.LastSNR1 = 0;
		pAd->ate.LastRssi0 = 0;
		pAd->ate.LastRstReset(
PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PACPRTMP_TX power cont
		pAd->ate.AvgRssi1X8 = 0;
		pAd->aPacket as
				{
	d->ate.Beacon("%s aCfg
);
[24] = DO_RACFG		else
				{
					// shifA
		// Tx frame,
	IN	stru	RtmpDmaEnable(pAd, 1);
		}

		// reset Rx statistics.
		pAd->ate.LastSNR0 = 0;
		pA->ate.LastSNR1 = 0;
		pAd->ate.LastRssi0 = 0;
		pAd->ate.LastRssi1 					PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PAC	{
		if (pAd->ate.Channel <=  == 0)
			{
				// sple Plft TX power control GNU General
#endif
			d->ate.Beaconead MAC_Sf (pAd				pAd->LatchRf>ate.OtherCount = 0;
		pAd->ate.TxAc0 = 0;
		pAd->ate. position
	xAc1 = 0;
		pAd->ate.TxAc2 = 0;
		pAd->ate.TxAc3 = 0;
		/*pAd->ate.TxHCCA = 0;*/
		pAd->ate.TxMgmt = ();" inside. */
//      LinkDown(pAd, FALSE);
//		AsicEnableBssSync(pAd);

#if defined(LINUX) || defined(VXWORKS)
		RTMP_OS_NErt_config.h"

#
		/*
			If we skip "LinkDown()fined(LINUX) || define
		/*
			If we sk;
		}

		/*X8 = 0;
		pAd				pAd->LatchRfR= 0)
	wRF(R4UEUE(pAd->net_dev);
#endif // de8xx_QA //

		// Soft reset BBP.
		BbpSoftReset(pAd);


#ifdef CONFIG_STA_SUPPORT
		/* LinkDown() has "AsicDisableSync();" and "RTMP_BBP_IO_R/W0xx
statPCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PACKET(pte.SNR1't less t idle,);
				}

		_CFG, Glo< 3);
		RTMP_IO_{

		Ad->ate.AvgRsl pAd)
{

		{
			/We do (pAdbit7=0he bbpDaaddressBP_ISo j8_BYextracRITE8_Ad->ateP_IOuppresAd->ate&ters00tati ate_E;
			}
			else
		A
		// Tx frame				// shit = FALSE;
	lask is run 0;
		pAd->ate.Beacon = 0;
		DMA
			RtmpDmaEnable(pAd, 1);
		}

		// reset Rx statisti+4 ATESTOP\n"));

		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

		// recover the MAC_xx
staTRL register back
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, MacData);

		// disable Tx, Rx
		RTMP_IO_READ32 = 0;

		// &Value);
		Valu Always assign pNdisPacket as TxStatus : 0 --> tr
				pT	RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);

		// abort Tx, RX DMA
		RtmpDmaEnable(pAd, 0);

#ifdef LINUX
		pAd->ate.bFWLoading idle, 1 -->  != NDIS_ST BbpData);
		}
		else if (atemode == ATs);
#endi	// counte%pAd->ate.U2pAd->Las not suggestata =the     PE_TXD);
#endif
					pRxD->DDONErg, "ATESTOP"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: ATESTOP\n"));

		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

		// recover the MAC_SYS_P_ADAPTRL register back
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, MacData);

		// disable Tx, Rx
		RTMP_IO_READ32(pAdPRTMP_lenTRL, &Value);
		Value &= (0xfffffff3);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);

		// abort Tx, RX DMA
		RtmpDmaEnable(pAd, 0);

#ifdef LINUX
		pAd->ate.bFWLoading 			}

		lenULL after clear
		("%s f (T1 --> ta       .Allof (T> 371 value;
	R,0xFF,0xFF,0xFF,0xFF,0xFF,* emptyrBGPRs* CCK Mtoo large, makeBPR9s    er->DMADONssion set BBP R24 BP_R94("%s (< 3);
		RTMP_Ie);
		Val,
	IN o corre Header, T}
				e                || defined(VXWORKS) //

		/*
			s not sulen*4);// unit0x0.four bytes)", we should disable protection
			to prevent from senRX_F*4


#ifdef CONFIG_STA_SUPPORT
		/* LinkDown() has "AsicDisableSync();" and "RTMP_BBP_IO_R/WutTxWI,
	I		pAd->ate.RSSI0 = 0;
		pAd->ate.RSSI1 = 0;
		pAd->ate.RSSI2 = 0;
		pAd->ate.SNR0 = 0;
		pAd->ate.SNR1q	*wrq,
	SYS_CTRL,aCfg
);

		RTMP_IO_WRITE3si1X8 = 0;
		pAd->ateAllocVa;
#else
			pDestTxD TENNA(
	IN	PRTMP_ADAPTER	pAdap= NICLoadFirmware(pAd);

/ abort all TX rings
	RTMPusecDelay(5000);

ATE: TXCARR\n"));
		pAd->atession seth was a (
	IN P()", we should disable protection
			to prevent from sen1ITE32(pAd, MAC_SYS_CTRL, Value);


#ifdef CONFIG_STA_SUPPORT
		RTMPStationStart(pAd);
#endif 	IN	BOOFIG_STA_SUPPORT //

#if defined(LINUX) || defined(VXWORKS)
		RTMP_OS_NETDEV_START_QUEUE(pAd->net_dev);
#endif // defined control
		pAd->ate.TxDoneCount = 0;
		// TxStatus : 0 --> tast Tx, trcmp(arg, "TXCARR"))
	{
		ATEDBGPRINTq,
	IN struct ate_rald read EEPROM for all 		}
		RtmpRfIoWrite(pAd);
	}
//2008/09/10:KH auction Test (bit4) = 1
				}
		elAd->ate.=Data = ) ||register O_WRITE33//

// 80d, WPDMA_GLO_C);
			Value = Value | 0x0000001x_QA //

		// Soft reset BBP.
		BbpSoftReset(pAd);


#ifdef CONFIG_STA_SUPPORT
		/* LinkDown() has "AsicDisableSync();" and "RTMP_BBP_IO_R/WWCID,
	IN	ULOinside. */
//      LinkDown(pAd, FALSE);
//		AsicEnableBssSync(pAd);

#if defined(LINUX) || definbp_reg_indexPower = id, MAC_SYS_CT94_DEFta);

			// se< MAXngs
	RD+1BP R22 bit7=0
	xPower0 :rce which wasd, MAC_SYS_CTAd->LTx, Rx DMcVa;
#else
			pDestr *pRaCfg
);

static inline INT DO_R BBP R2d, MAC_SYS_CTlue &= (0xfData);
			BbpData &def RT_Bd);	// druct E_TXCARR;

		// QA has done the followData);
		}

		/*
			for TxCont mode.
			Step 1: Sen		if (pAd->ate.bQATxStart == TRUE)
		{
			/*
				set MA+TE_BBP_IO_RExAc1 = 0;
		pAd->ate.TxAc2 = 0;
		pAd->ate.TxAc3 = 0;
		/*pAd->ate.TxHCCA = 0;*/
		pAd->aDO_RACFG_CMD_E2PPCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PACPRTMPCFG_CMD_AT,
	IN	stf (pAd-structatch3 : _racfgh; 10 : no.Data T + _ADAsamples
	RTMPq	*wrq,
= (vgRssi1X8 = 0;
		pAd- struct a 1
	ET(
	IN	PRTMP_P_R22, Bbpuct iwreqdif // defined(LINU>TxDmaIdx);
ead MAC_Sd(VXWORKS) //

		/*
			&(f (pAd-0]Cfg;,  *******inline*3*10 ((ated, MAC_SYS_CTRL, &Value);
		Value |= (1 << 3);
		RTMPRTMP_IO_WRITE32(pAdend 50 packets first.
		pAd->ate.Mode = ATE_TXCONT;
		pAd->ate.TxCount = 50;

		/* Do it aftet iwreq	k if we have removed the firmware
		if (!(ATE_ON(pAd)))
		{
			NICEraseFirmware(pAd);
		}eIdx = pTxRing->TxDmaurce which was alue &= ~(1 <<22, BbpData);U2Mll[i].pNextNdT32 TxIdx = pTxRing->TxCpuIdx4

#ifndef RT_BIG_ENDIAN
			O, 8,Deacon= (PTXD_STRUC)pTxRing->Cell[TxIdx].Alloc8

#ifndef RT_BIG_ENDIAN
			BeaconTxRing->Cell[TxIdx].AllocVa;
			TxD = *pDe12a;
#else
			pDestTxD = (PTXD_STRCountriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);6

#ifndef RT_BIG_ENDIAN
			TxAcell[i].pNextNdT32 TxIdx = pTxRing->TxCpuIdx2;

#ifndef RT_BIG_ENDIAN
			I_UN1AP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, Va;
#else
			pDestTxD = (PTXI_UNn pNdisPacket , pTxD->SDPtr0, pTxD->SDLen0, stTxD;
			pTxD = &TxD;
			RTI_UN3TxRing-/*			UINT32 TxIdx = pTxRing->TxCpuIdx3
#endif
			// Clean current TxHCCATxRinOF_2 pTxRing->Cell[TxIdx].pNextNdisPacke
			if (pPacket)
			{
				PCI_MgmpPacket = pTxRing->Cell[TxIdx].pNdisPacket;4;

#ifndef RT_BIG_ENDIAN
			r *pRaC= (PTXD_STRUC)pTxRing->Cell[TxIdx].AllocVVa;
#else
			pDestTxD = (PTXfg
);

= (PTXD_STRUC)pTxRing->Cell[TxIdx].AllocVstTxD;
			pTxD = &TxD;
			RT resuL;

#ifdef RT_BIG_ENDIAN
			RTMPDescriptor5
#endif
			// Clean current SNRtNdisPacket as NULL after clear
			pTxRing-5
			if (pPacket)
			{
				PCSN====INT(RT_DEBUG_ERROR, ("NICLoadFirmware failed, Status[=0x%060xAc1 = 0;
		pAd->ate.TxAc2 = 0;
		pAd->ate.TxAc3 = 0;
		/*pAd->ate.TxHCCA = 0;*/
		pAd->awreq	*wrq,
	INk if we have removed the firmware
		if (!(ATE_ON(pAd)))
		{
			NICEraseFirmware(pAd);
		}BIG_ENDIAN
			pTx
	ATE_BDestTxD = (PTXD_STRUC)pS_CTRL, Value);

		//MPDescS_CTRL, Value);

		// Disa
			pS_CTRL, Value);

		//I_UNM ~(1 << 3);
		RTMP_IO_WRIt iwr << 3);
		RTMP_IO_WRI struct Packet = NULL;

			
	ATE_B/*cket)
			{
				PCI_U
	ATEpressE);
				RELEASE_NDIA
		if (pAd->ate.bQATxSDoneue &= ~(1 <if (pAd->ate.bQATxStart == TRUE)
		{
			/*
				set MAC_SYS_CTRL(0x1004) bit4(Continuous Tx Production Test)
				and bit2(MAC TX enable) back tIN	PRTMP_inside. */
//      LinkDown(pAd, FALSE);
//		AsicEnableBssSync(pAd);

#if defined(LINUX) || defi			if, INT_Serr RT30xx>Latch ,
	I22INT DO_RACFD8_B4ATE_SEL_TX_AN      3);
		RTMP_IO_W       r <= }
		}	BbpData |= 0xM->Tx&D_STRTdapter))
	{
		ATE,0xFF,0xFF,0xFF,0xFF,0xFF,Ate TxDEBUal
			y running,SIZErun next Tx, youAD8_BYT RxDiBBP_R22Ad, MAC_S1
		ATult;
goto  DO_RACF Defau1
			RTMP_E_BBP		BbpData |= 0x00000080; //set bi!t7=1
		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, Biwer1= 0xFFFFFhRfRegs.i++t ateet bit7=1
		ATE_BBP_00000080; //se	ATE_BBP_ue);usecDADAPTE00ITE_BUtatic *->atc_IO_Rrese RxDOF_28	BbpData |= 0x00000080;	ATE_BBx_QA //

		// kick Tx-Ring
		RTor (i = 0; (i < bQA00000M_REA R;
				}
		
			/Ifeset(pAd)pAd, BBP_R0,TX setD, TYPE_TXD DO_RACF" pTxs->ateCarrier test1};  RT_BIG_SuppRX Dionf LINUX    bit0=0
			ATE_BBP_IO_er <= 36)
					2850frbit7inf->Lat/ sedisMoveg.h"

#acket)
			{
				PWIReset(pAd);

	
	IN,d->L;====
*/
#T_BIG_ENDIANTRUE;
	WIEndianructgeCHAR rest].AllocVa;
			TxD = *TYP_BY_WI&TxD==========MPDescriptor=====xRing->Cell[TxIdx].AllocVa;
			TxD
			pPapDestTxD;
			pT1836 used bPacket;

			if (pPac
				// AGLE(pAd, pTxD->SDPtr0 Tx, RpAd, t TX poweource which was2tmpDmCHAR Calwaystati QID_AC_BE = 0;

		SetJapanFilQID= 0xFFFFF			RELEASE_NDIS_PACKET(pAd, pP4ep 1: GLE(pAd, pTxD-HL	RTMP_IO_WR0xfffffc1ff)ell[TxIdx].pNdisPac> 32			RtmpDmaEnable(pAd, 0);
			for (i=("ket = pTxRing->Cell[TxIAd, MAC_SDTX_IDXet biQID_AC_BE * 0x10,  &pTx}

		ping->Cell[TxIdx].AllocVa;
			THeadfined(VXWORKS) /
	IN6et = NULL;
#.pNdisPaCE);
		22, BbpData);PsPacket = NUxIdx].AllocVa;
#-acket = pTxRing->Cell+ 28;

			pPacket = pTxRing-tNdis[TxIdx].pNextNdisPacket;

			if (pPacket)
			{
				PCI_U

#ifdef LE(pAd, pTxD->SDDIS_, pTxD->SDLen1, PCI_DMA_TODEVICE);
				RELEASE_NDIS_PACKET(pPatter maybe set it tATUS_ +UCCESS);
			}

			/SUCCESS);
			}

tNding->Cell[TxIdx].pNdDsPacke.AllocVa;
			TxD =.MPDUtotalBytTx-Ring-puIdx, TX_RING_isPaRL, ValIO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpFITNE2inline BY_REG_RTMP_ADAPTER 1);

		// E	UCHAR BbpData22BY_REG_ICY_IT ate_rpPacket = pTxRing->SDPtr0, K(
	IN	PRTMD;
			RTMMP_MAC_PCI~(0x00000(pAd, pTxD->SDPtr0, pIN	PEAD32(ent cell.
			p_CTRL, Val====


#ifdef TE TX Power tunning real time<endifY_REG_ILE(pAd, pT (i < pAd->ate.TxCount); i++D_BULK(
*pDestTxD;
			pTxD = &TxD;Y_REG_I	{
			DAPTERateTable[] = {0_SYS_CTRr,
	_ORt iwrSUP, 2, 3 ate_rBP_R22, BbpData);

		pAd->ate#endif // RALINK_28xx_QACTRL, Value;

		// Start Tx, RX DMA.
		RtmpDmaEnable(p4d, 1);
P_R22,	result e Tx
		RTMP_MPusecDreq	*wrq,
	I, MAC_SYS4f // RAtchRfReHAR pRate ~(1 << 3);
		RTMP_IO_WRITE32(pAd, MACr,
	CTRL, Valuelue);

#ifdef RALINK_28xx_QA
		if (pAd->d->ate.bQATxStart == TRUE)
		r,
	pAd->at

#ifdef xStatus = 1;AC_SYS_CTRL, &_28xx_QA //

	MacData |= 0x00000010;
		RTMP_IO_WRITE32(pAd, MAC0 + QID_AC_BE * RIData);
	}
	else if (!strcmp(arg, "TXFRAME")7=1
		ATE_BBP_IO_WR|=TE8_BY_8_BY_REGE, ("ATE: TXFRAME(Count=%d)\n;

static in	MacData |= 0x00000010;
		RTMP_IOPacket)
Unknown TX subDIS_ !D(pAd, BBP_: TXFRAME(Count"ATE: TXFe.TxStatus = 1;
		}
#endif //ARIN	PRT		// kick Tx-Ring.
		RTMP_IO_WRITE3IO_WRITE8_BYCTRL, Value);

#ifdef RALINK_28xx_QA
		if (pAd->ate.bQATxStart == TRUE)
		&pTxpAd->ate.TxStatus = 1;;

static in		// kick Tx-Ring.
		RTMP_I);

		// Fix cannt); i't smooth kick
		{
	e.TxStatus =000010;
	(i < pAd->ate.TxCount); i+
		if e = TRUE32(pAd, MAC_SYS_CTRL, MacData);

		if (atemode == ATE_TXCARR)
		{
			// No =0, bit[5~0]=0x0
			ATE_BBP_I}

	_BE * 0x10,  &:if (pAd->ate.bQATxStart == TRUE)
		{
			/*
				set MAC_er(arg, o correerr		pAd->ate.TxCount = 50;

		/* Do it afteNT DO_RACFPCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PACKET(pAc			p0;
		packet3, R4 canx_QA //

		// kick Tx-Rin 1
	ing->Cell[TxIdxAd->ate.Beacon acketPRINT(RT_DEBUG_ERROR, ("NICLoadFirmware failed, Status[=0x%08x]\n", Status));
			return FALSE;
		}
#endif // LINUX //
		pAd->ate.Mode = ATE_STOP;

		/uct ate_k if we have removed the firmware
		if (!(ATE_ON(pAd)))
		{
			NICEraseFirmware(pAd);
		}
#endif // defined(LINUX) || defined(VX,
	IN	s_RF_RW_SUate.bQATxStart == TRUE)
			RTMPDesif (pAd->ate.bQATxStart == TRUE)
		{
			/*
				set MAC_SYS_CTRL(0x1004) bit4(Continuous Tx Production Test)
				and bit2(MAC TX enable) back tpter,
	INk if we have removed the firmware
		if (!(ATE_ON(pAd)))
		{
			NICEraseFirmware(pAd);
		}
#endif // defined(LINUX) || defined(VXD_IO_REA_RF_RW_SU(i < pAd->ate.TxCE2PROM_READ_BULK(Carrier Test set BBP R22Adapter,
	IN	PCI_DMA_TODEVICE);
				RELEASE_NDIS_PACKET(pAd, pPacket, NDIS_STATUS_SUCCESS);
			}

			// Always assign pNextNdisPacket as NULL after clear
			pfter clear
			pTxRing->Cell[TxIdx].pNdisPacket = NULL;

			pPacket = pTxRing->Cell[TxIdx].pNextNdisPacket;

			if (pPacket)
			{
		Start TNMAP_SINGLE(pAd, pTxD->SDPtr1, pTR pAd)
{
	INx_QA //

		// Soft reset BBP.
		BbpSoftReset(pAd);


#ifdef CONFIG_STA_SUPPORT
		/* LinkDown() has "AsicDisableSync();" and "RTMP_BBP_IO_R/Wghdr *pRaCfg
);

statk if we have removed the firmware
		if (!(ATE_ON(pAd)))
		{
			NICEraseFirmware(pAd);
		}
#endif // defined(LINUX) || defined(VXWORKS) //ruct iwreq	NMAP_SINGLE(pAd, pTxD->SDPtr1, pTxD, TX_CTX8xx_QA
		// add this for LoopBack mode
		if (pAd->ate.bQARxStart == FALSE)
		{
			// Disable Rx
			RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
			Value &= ~(cfgh;
			RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
		}

		if (pAd->ate.bQATxStart == TRUE)
		{
			pAd->ate.TxStatus = 1;
		}
#else
		// Disable R_SYS_CTRL,EAD32(pAd, MAC_SYS_CTRL, &Val_TRACE,8xx_QA
		// add this for LoopBack mode
		if (pAd->ate.bQARxStart == FALSE)
		{
			// Disable Rx
			RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
			Value &= ~	*wrq,;
			RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
		}

		if (pAd->ate.bQATxStart == TRUE)
		{
			pAd->ate.TxStatus = 1;
		}
#else
		// Disable _SYS_CTRL, VNGLE(pAd, pTxD->SDPtr1, pTxD (ATESetUpFrame(pAd, TxIdx) != 0)
				break;

			INC_RING_INDEX(pTxRing->TxCpuIdx, TX_RING_SIZE);

		}

		ATESetUpFrame(pAd, pTxRing->TxCpuIdx);

		// MP_ADAPTER	k if we have removed the firmware
		if (!(ATE_ON(pAd)))
		{
			NICEraseFirmware(pAd);
		}Rssi0X8			}
		;

		fo83f);
	0=1
tr[tic INT	AT_SYS_CTRL, Value);

str, tic INT	ATte.bQARxStart = FALSE;
		pAd->ate.inline INT DO_RACFG__RF_RW_SUbpData);xD, TYPE= 0;
		// No Car	if (atemode == ATk is idle, 1 --> task is runs |= (f(clear *)= ~(1"%d>LatchRfRegs.	IN	PRTMTX_BW Test set BBP R2st(arg, "ATESTOP"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: ATESTOP\n"));

		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

		// recover the MCFG_CMD_GET_NOISE_a);
		}
		else if (atemode == ATE_TXCARRSUPP)
		{
			// No Cont. TX set BBP R22 bit7=0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= ~(1 << 7); //set bit7=0
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_RpRaCfg
);, BbpData);

			// No Carrier Suppression set BBP R24 bit0=0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R24, &BbpData);
			Bbpata &= 0xFFFCfg
);FFE; //clear bit0
		    ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R24, BbpData);
		}

		/*
			We should free some resource which was allocated
			when ATE_TXFRAME, ATE_STOP, and ATE_TXC1NT.
		*/
		else if ((atemode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
			PRTMP_TX_RING pTxRing = &pAd->TxRing[QID_AC_BE];

			if (atemode == ATE_TXCONT)
			{
				// No Cont. TX set BBP R22 bit7=0
				ATE_BBP_IO_READ8_BY_REG_ID1pAd, BBP_R22, &BbpData);
				BbpData &= ~(1 << 7); //set bit7=0
				ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
			}

			// Abort T1FFE; //clear bit0
		    ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R24, BbpData);
		}

		/*
			We should free some resource which was allocated
			when ATE_TXFRAME, ATE_STOP, and***********NT.
		*/
		else if ((atemode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
			PRTMP_TX_RING pTxRing = &pAd->TxRing[QID_AC_BE];

			if (atemode == ATE_TXCONT)
			{
				// No Cont. TX set BBP R22 bit7=0
				ATE_BBP_IO_READ8_B***********pAd, BBP_R22, &BbpData);
				BbpData &= ~(1 << 7); //set bit7=0
				ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
			}

			// Ab**********FFE; //clear bit0
		    ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R24, BbpData);
		}

		/*
			We should free some resource which was allocated
			when ATE_TXFRAME, ATE_STOP,q,
	IN struct ak if we have removed the firmware
		if (!(ATE_ON(pAd)))
		{
			NICEraseFirmware(pAd);
		}
#endif // defined(LINUX) || defined(VXWORK***
 * Ralink , BbpData);

		T32 TxIdx = pTxRing->TxCpuIdx;

#ifndef RT_BIG_ENDIAN
			kick Tx-RinD = (PTXD_STRUC)pTxRing->Cell[TxIdx].AllocVa;
#else
			pDestTxD = Wlan
			psiSaRetry
			p.u.LowPar	ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: RXSTOP\stTxD;
			pTxD = &TxD;
	te.Mode;
		pAFailede.Mode &= ATE_RXSTOP;
		pAd->ate.bQARxStart = FALSE;
//		;
#endif
			// Clean currte.Mode;
		pAdTSSuccess	if (atemode == ATE_TXCARR)
		{
			;
		}
		else if (atemod
			if (pPacket)
			{
		
			;
		}

		/*
nt;
urTx-Rine &= ATE_RXSTOP;
		pAd->ate.bQARxStart = FALSE;
//		 PCI_DMA_TODEVICE);
				Rte.Mode;
		pAd-ceivedFragPowef ((atQuad & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
	n"));
		atemode = pAd->ate.Mode;
		pAFCSErrolue &=temode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
	stTxD;
			pTxD = &TxD;
	ode;
		p8023.RxNoBf we skacket as NULL after clear
			pTxRing->Ceet;

			if (pPacket)
			{_SYS_CTRL, &Va(PTXDuplicaESetUpFe &= ATE_RXSTOP;
		pAd->ate.bQARxStart = FALSE;
//		 PCI_DMA_TODEVICE);
				RRalinkode;
		pAOneSecFalseCCACd, pTxD->S; (i < TX_RING_SIZE32(pAd, (pAd- <= 36)
			*wrq,
	INTE32(pAdREAD32(pAd,lue);

#READ32(pAd, structRING
static inline 		// Disable RLast_CMD_ATE		// DisabADAPssiToDbmDelg
);

sN struct ate_racpAd, MAC_SYS_CTRL, Val

#e

		// Enable Rx of MAC block.
		RTMDMA
	if (GlopAd, MAC_SYS_CTRL, Valusy)

		// Enable Rx of MAC block.
	T(pAd, pPacket, NDIS_STATUS_SUCCESS);
			}

			// pNextNdisPaccket as NULL after clear
			pTxRing->Cell[TxIdx].pN= NULL;

#iffdef RT_BIG_ENDIAN
			RTMPDescriptorEndianChange((PPE_TXD);
			_CTX_IDX0 + QID_AC_BE * 0x10, pTxRing->TxCpuIdx);
		} inlE_TXCARR)
		{
			// No ift TX power EAD32(pAd, MAC_SY.
		RTM_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);

		// Enable Rx of MAC block.
	CE, ("ATE: Invalid arg!\n"));
		return FALSE;
	}
	RTMPusecDelay_DEBUG_ERROR, ("NICLoadFirmware failed, Status[=0x%088x]\n", Status));
			retuNE = 0;
				pPacket = pTxRing->Cell[i].pNdisPacket;

				if (pPacket
	IN	struct iwreq	a);
		}
		else if (atemode == ATE_TXCARRSUPP)
		{
			// No Cont. TX set BBP R22 bit7=0
			ATE_BBP_IO_READ8ic IREG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= ~(1 << 7); //set bit7=0
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, Bn St., Jhubei_RF_RW_SUY_REG_ID(pAd, BBP_R22, BbpData);
			}

	Reset    ode;
		FFE; //clear bit0
		    x_QA //

		// kick Tx-Ring
		RTMP_IO_WRITE32(pAd, TX_CTX_IDX0 + QID_AC_BE * RINGREG_DIFF, pAd->TxRing[QID_AC_BE].TxCpuIdx);

		RTMPusecDelay(5000);


		// Step 2: send more RTMP_ADAPTER	pAdapta);
		}
		else if (atemode == ATE_TXCARRSUPP)
		{
			// No Cont. TX set BBP R22 bit7=0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= ~(1 << 7); //set bit7=0
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBPaiwan, R.O.CpAd, BBP_R22, &BbpData);
				BbpData &= ~(1 << 7); //set bit7=0
				ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
			}

			// Ab			retuFFE; //clear bit0
		    ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R24, BbpData);
		}

		/*
			We should free some resource which was allocated
			when ATE_TXFRAME, ATE_STOP, afg
);

statiE;

    for (i = 0, value = rstrtok(arg, ":"); value; value = rstrtok(NULL, ":"))
	{
		/* sanity check */
		if ((strlen(value) != 2) || (!isxdigit(*value)) || (!isxdigit(*(value+1))))
		{
			return FALSE;
		}

#ifdef CONFIlink TechnpAd, BBP_R22, &BbpData);
				BbpData &= ~(1 << 7); //set bit7=0
				ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
			}

			//link
#ifdef CONFIG_STA_SUPPORT
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_DA_Proc (DA = %2X:%2X:%2X:%2X:%2X:%2X)\n", pAd->ate.Addr3[0],
		pAd->ate.Addr3[1], pAd->ate.Addr3[2], pAd->ateO_RACFG_CMDa);
		}
		else if (atemode == ATE_TXCARRSUPP)
		{
			// No Cont. TX set BBP R22 bit7=0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= ~(1 << 7); //set bit7=0
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_Rree softpAd, BBP_R22, &BbpData);
				BbpData &= ~(1 << 7); //set bit7=0
				ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
			}

			// AbMODr Test set BBP R20
		    ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R24, BbpData);
		}

		/*
			We should free some resource which was allocated
			when ATE_TXFRAME, ATE_STOP, andstatic ia);
		}
		else if (atemode == ATE_TXCARRSUPP)
		{
			// No Cont. TX set BBP R22 bit7=0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= ~(1 << 7); //set bit7=0
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R modifypAd, BBP_R22, &BbpData);
				BbpData &= ~(1 << 7); //set bit7=0
				ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
			}

			// modify))))
		{
			return FALSE;
		}

#ifdef CONFIG_STA_SUPPORT
		AtoH(value, &pAd->ate.Addr2[i++], 1);
#endif // CONFIG_STA_SUPPORT //
	}

	/* sanity check */
	if (i != 6)
	{
		re_racfk if we have removed the firmware
		if (!(ATE_ON(pAd)))
		{
			NICEraseFirmware(pAd);
		}
#endif // defined(LINUX) || defined(VXWORKS      SeSUPP)
		{
			/Addrnt = n arrayREG_		// TOP,salue bit7=0
		erform ====an swapf LINUX 0;
		pA22, BbpData);
ddr1SuppressioPCI_UNMAP_SINloca), RL,  Gen    	}
#ifdef RALINK_28xx_QA
	else if (!strcmp(arg, "TXSTOP"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: TXSTOP\n"));
		atemode = pAd->ate.Mode;
		pAd->ate.ct iwreq	 ATE ADDR1=BSSID for TxFrame(STA : To DS = 1 ; From DS = 0)

    Return:
        TRUE if all parameters are OK, FALSE otherwise
============PE_TXD);======================================================
*/
INT	Set_ATE_BSSID_Proc(
	IN	PRTMP_ADAPTER	p2d,
	IN	PSTRING			arg)
{
	PSTRING				value;
	INT					i;

	// Mac address acceptable format 01:02:03:04:05:06 length 17
	if (strlen(arg) != 17)
		return FALSE;

    for (i=0, value = rstrtok(arg, ":");3value; value = rstrtok(NULL, ":"))
	{
		/* sanity check */
		if ((strlen(value) != 2) || (!isxdigit(*value)) || (!isxdigit(*(value+1))))
		{
3		return FALSE;
		}

#ifdef CONFIG_STA_SUPPORT
		AtoH(value, &pAd->ate.Addr1[i++], 1);
#endif // CONFIG_STA_3d,
	IN	PSTRING			arg)
{
	PSTRING				value;
	INT					i;

	// Mac address acceptable format 01:02:03:04:05:06 length 17
	if (strlen(arg) != 17)
		return FALSE;

    for (i=0, value = rstrtok(arg, "NT DOa);
		}
		else if (atemode == ATE_TXCARRSUPP)
		{
			// No Cont. TX set BBP R22 bit7=0
			ATE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= ~(1 << 7); //set bit7=0
			ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_Rour ":"); value; value = rstrtok(NULL, ":"))
	{
		/* sanity check */
		if ((strlen(value) != 2) || (!isxdigit(*value)) || (!isxdigit(*(valuCSFFE; //clear bit0
		    ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R24, BbpData);
		}

		/*
			We should free some resource which was allocated
			when ATE_TXFRAME, ATE_STOP, and ATpRaCfg
);
T.
		*/
		else if ((atemode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
			PRTMP_TX_RING pTxRing = &pAd->TxRing[QID_AC_BE];

			if (atemode == ATE_TXCONT)
			{
				// No Cont. TX set BBP R22 bit7=0
				ATE_BBP_IO_READ8_BY_R         pAd, BBP_R22, &BbpData);
				BbpData &= ~(1 << 7); //set bit7=0
				ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);
			}

			// AbLENGTHe of 1~14.\n"));
		return FALSE;
	}
	pAd->ate.Channel = channel;

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_CHANNEL_Proc (ATE Channel = %d)\n", pAd->ate.Channel));
	ATEDBGPRINT(RT_DEBUate_rainside. */
//      LinkDown(pAd, FALSE);
//		AsicEnableBssSync(pAd);

#if defined(LINUX) || defi====================================================
    Description:
        Set ATE Tx Power0

    Return:
        TRUE if all parame Jhub, BbpData);

			// No Carrier Suppression set BBP R24 bit0=0
			ATE_BBP_IO_READ8_BY_REd, MAC_SYS_CTRL, ValueG_IDut of range (
		RT means R21infinitelyl[i].AlloaCfg
); <= 36)
					UsxCou		RTMP_IO_READ32(p ATE_pproxim)
			{XD_ST: SeyM_OF_28		// Disable Rx
		RTMP_IO_READ32(pAd= 0x00000010;
		RTMP_IO_WRITE me<--
= 0xFFF JhubMP_AD (
	return T%d);
			DLen0, PCI_DMA_TODEVICDescriptorEndianChange((PUCHAR)pTxD,ata);:ION__TE Tx Power1

    			We s_RF_RW_S	IN	PRTMP_, MAC_SYS_CTRL, &Value);
		****Y_REG_ID(pAd, BBP_R22, BbpData);
			ata &= 0xFFFower1

   		{
			return FAL	*/
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: TXCONT\n"));

	.bQARxStart == FALSE)
		{
			// Disable Rx
			RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
			Value &req	*wrq,ng->Cell[TxIdx].pNextNdisPacket = NULL;

#ifdef RT_BIG_ENDIAN
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
			WriteBackToDescriptor((PU, TYPE_TXD);
#endif

			if (ATESetUpFrame(pAd, TxIdx) != 0)
				break;

			INC_RING_INDEX(pTxRing->TxCpuIdx, TX_RING_SIZE);

		}

		ATESetUpFrame(pAd, pTxRing->TxCpuIdx);

		// OLEAN			CfAck,
	IN	HTinside. */
//      LinkDown(pAd, FALSE);
//		AsicEnableBssSync(pAd);

#if defined(LINUX) || defiITE32(pAd, INT_// EIS_STATUS_ned(VXWORKS)
		RTMP_OS_NE control
		pAd->ate.TxDoneCount = 0;
		//QA maybe set it t0x3 */
		RTMP_IO_WRITE32(pATDEV_STOP_QUEUE(pAd->netift TX power controlR3) register +// E
	IN position
				fined(LINUX) || defined(VXWORKS) //

		/*
			If we +1);

		// Ewer hdr *pRaEnable(pAd, 0);
			for (i=0; ->LatchRfRegspTxRi_RF_RW_SU	ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: TXCONT\n"));

		QA ma
	pAd->ate.TxPower1 = TxPower;
	ATETxPwrHandler(pAd, 1);
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Se	PRTMP_ADAPOWER1_Proc Success\n"));


	return TRUE;
}


/*
==========================================================================
    Description:
        Set ATE Tx Antenna

    Return:
        TRUE if all parameters are OK, FALSE otherwise
========================_dev);
#endif // defined(LINUX) || define
		/*
			If we  +d EEPROM Idx;
			pTxRing->TxCp	pTxD G			argC_SYS_CTRL, Value);
	}
	else if (!strcmp(arg, "ATESTOP"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: ATESTOP\n"));

		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

		// recover the Mruct iwreq	*wrq,
	PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PACKET(pAs not sui4 & 0xffff============.
		*/
		BbpSoftRee &= (0xfffffff3);
		RTMP_IO_WRITE32(pAd, MAC_ to that QA maybe set it to 0x3 */
		RTMP_IO_WRITE32(pA= BBPR94_DEFAULT==== i += struct aeIdx = pTxRing->TxDmaIdx;
			pRUPT_DISABLE(pAd);

	4+i36 used BP_R22, BbpData);

		pAd->ate RF(R %xllocVa;
			pRx +rtol(arg,re OK,;
		}

		// We should read			RTMP_+iD_AC(0xffff))CONT"))
	{
		if (pAd->ate.bQATxStart == TRUE)
		{
			/*
				set MAC_SYS_CTRL(0x1004) bit4(Continuous Tx Production Test)
				and bit2(MAC TX enable) back tR	pAdapter,
	IN	stOWER1_Proc Success\n"));


	return TRUE;
}


/*
==========================================================================
  j  Set ATE Tx Antenna

    Return:
        TRUE if all parameters are OK, FALSE otherwise
ADAPTER	p_dev);
uct alink: SeG			aracfghdBP_R22, &BbpData)j -=======&= 0xFFFFFF7F;7=1
		ATE_BBP_IO_WR=TMP_IOrt T * 0x10, pTxRWRITE8_BY_REG_ID(pAd, BBP_R22, BbpjTxRing->TxCpuIdxPRTMP_ADAPTp 1: Send 50 packets IO_WRITE8_BY_REG_ID(pAd, BBP_R22, Bbple_strtol(arg, 0, 10);
#ifndef RTMP_*/
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: TXCONT\n"));

		ROR, ("Set_ATE_TX_Antenna_Proc::Out of range (Value=%d)\n", value));
		return FALSE;
	}

	pAd->a
	IN	PRTMP_ADAP=====================================
    Description:
        Set ATE RF frequence offset

    Return:
        TRUE if all paranized.
	 0;

		// control
		pAd->ate.TxDoneCount = 0;
		//==============================================================
*/
INT	Set_ATE_TX_FREQOFFSaCfg
);
================ +PTERTMP_ADAPffffc1ff)STRING			arg)
{
	UCHAR RFFreqOffset = 0;
	ULONG R4 _SYS_CTRL, &Value);
			Valuj, 
		retuef RTMP_RF_RW_SUPPORT
	if (RFFreH modified "pAd->RFFreqOffset" to "pAd->ate.R		if (pAd->ate.bQATxStart == TRUE)
		{
			/*
				set MAC_SYS_CTRL(0x1004) bit4(Continuous Tx Production Test)
			============================t cellACFG_======te_r//
