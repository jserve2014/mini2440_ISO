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
 *a Hsinchu County 302,
 * T**PREAMBLE City,
 bnc.
 *
 * This program is fCHANNELware; yocnc.
 *
 * This program is fADDR1	ware; yod under the terms ofse asGNU Gen2ral Publie License as published by  *
 * 3he Free Sfnc.
 *
 * This program is fRATtwar is f10nc.
 *
 * This program is fTX_FRAME_LENtion) a1y later version.                     Jhub      2ater version.            ARTalin            3***********************E2PROM_READ_BULK      4ram is distributed inse ashopeWRITEt it willou can redistribute it andIOUT ANY WARRA      
 *
 * This program is fBBPe tha WARRAN     colog 5F., No.36,, RayuaFITNrranty of  NTY; *(c) Copyright 2002-2007RFNESS FOR A PARTIC (c) Copyright 2002-2007RFarrat 20of        a


static VOID memcpy_exl(PRTMP_ADAPTER pAd, UCHAR *dst      *
 src, ULONG len);                      s copy of the GNU Generhould* You should have received a cop     e as the IONESS FOR Al Public License     *
 * along with this proINT32; if no


e to ttmpDoAte(
	IN	            *	pAdapter,Generatruct iwreq	*wrq)
{
	USHORT Command_Ida copT	Status = NDIS aloRUS_SUCCESS;
- Suite ate_racfghdr *pRaCfg* 59 if ((       = kmalloc(sizeof        ************), GFP_KERNEL)) == NULL)
	{
       *
*
-EINVAL;
		return;
	}

	NdisZeroMemory*******, nc.
 *
 * This program is f************t, w_from_user((P *
 *)ifdef RAwrq->u.data.po****Pl[****BBP_REG_length)******************FAULT* alkfreo the   )* al/

#include "-1307, USA = ntohal Publi->cemplateid* 59 ATEDBGPRINT(RT_DEBUG_TRACE,("\n%s:1TemplateFrame0x%04x !\n", __FUNCTION__,00,0xAA,0xB*****	switch (2,0xAA,0xBB******/* WeRANTY;get this x00,0x0 when QA starts.:24b		caal Public License     :
 City,
 *=DO_            *
 * Fr(Templ={0}wrq,a, L30xx :2	break0xCCP_RF // S RF2850RegTable[];
eeithern UC= {0,osed or Lend 10,killed by 68
UM_OF_ble[_CHNL;

ext8, 9****OPCY_ITEM FreqItems30201, 2,e[] =OP  *
Nc CHAR, -1MRateTab          CHAFDMRateTable[] *********ALL 2, 3, 4, 5, 6, 7, -1}; / 4, 5, 6, 7,odeM_OF_ateTable[]R HTMIXRate {0, 1, = {0, 1, 2, 3, WITHOUESS 16 2, 3, 4, 5, 6, 7, -1}; /           );* HT Mix M* HT Mixx Mode. INT TxDmaBusy                   rrant); VOID RtmpDmaRnable(
	IN PRTMP_Ad,N PRTTER pAd);

static VOID RtmpDmaEnable(
	IN PRTMP_ADAPTER pA      7, 8, 9,CK M 11, 12, 13, 14IN PRTMP_ADAPTERTMP_ADAP              R    maEn{0, tmpRfIoWrite(
	IN PRTMP_
	I            N INT Enable);
ATESetUpF5, -1}; /TER pAd);

static VOID RtmpDmaEnable(
	IN PRTMP_ADA******* 8, 9, 10, 11, 12, 13, 14,0xx
stP_ADAPTER pAd,
	CmdHandler                    TemResetrrantRING			arg_ADA#ifne_BBPT30xxrranttic int CheckMCSValid(
	IN UCHAR Mode,
	IN UCHAR Mcs);	PST WARRif // RT30xx //

#ifdef RT30x Mo0xx
st HT Mix Mode. */

static INT TxDmaBusy(
	IN PRTMP_     det8 2, 3, 4, 5, 6, 7, -1}; /         re_BBP    MAC_PCI             ATEriteTxWI UCHAR Mode,
rrant UCHAR Mcs,
	TXWI_STRUC	pOutTxInsTimIN	BOOLEAN			FRAG,
	IN	BOOLEAN			WCFACK,
	IN	BOOLEAN			    _ADAPTER pAd);

static INT ATnce.     UCHOLEAN			Ack,
	IN	BOOLEAN			NSeq,		// HW new a seque but WITHOUESS FOR A 2, 3, 4, 5, 6, 7, -1}; /* OFAR			TxRate,
	INO // RHNL;

 Mcs,
    *			PIDTTING	*pTransmTt);
#endif // RTM0xx
sty of CI //


staopmode Mcs,
BOOLEAN			Cf	InsTiy of TTRANSMIT_SETTING	*pTransmit);
#endif // RTMP_MAC_PCI / w            2, 3, 4, 5, 6, 7, -1}; /* OFRT(
	IN	PRTMPTTRANSMIT_SETTING	*pTransmit);
#endif // RTMP_MAC_PCI /MP_ADAPTails. 2, 3, 4, 5, 6, 7, -1}; /* OFO_RACFG_CMD_A PRT Suite                     
_ADAPTER pAinl****);
#DO_RAUCHAR Mode,
	IN UCHAR ple Pl      Suiteghdr *pRaCfg
)OLEAN			Ack,
	IN	BOOLEAN			NSeq,			CFACKN	PRTMP_ADAP***
NOIS0xx
VEAPTER pAd);

static INT AT                LEAN			Ack,
	IN	BOOLEAN			NSeq,		// HW new a seque_rac     eiate_racfghdr *pRaCfg
);

stateq	*wrq,LEAN			Ack,
	IN	BOOLEAN			NSeq,		// HW new a sequeCLEAR0, BostoReset(t iwreq	*wrq,
	IN sAR Mode,
	IN 

static inline INT DO_RACFG_CMD_E2PROM_WRITE16(
	ITXREQUEN 8, 9, 10, 11, 12, 13, 14,statiCF hope tha16 UCHAR Mode,
	IN UCHAR CMD_RF_WRITE_ALL( 330IN	PRTTUSate_racfghdr *pRaCfg
);

stat         uct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

statN PRTMP_, 15, -1}; /index);F_WRITE* HT Mix Mode. */

static INT TxDmaBusy(
	IN PRTMP_IN	PRTMP******he hope thatALL
(
	D_IOe thrq,
	IN struct ate_racfghdr *pRaCfg
);

statiEAD(
	INIO_	struct iwreq	*wrq,
	IN strte_racfode. */
static CHAR HTMIXRateTable[]R CThe following FDMRs arSuitr newN			 GUI(not QA) CHAR O/*=IN struct ate_racfghdr *pRaCfg
);
 that it       HAR OFDMRateTable[] = {0, UENuiteCARRIPTER	pAdapter,
	IN	struct irq,
	IN struct ateq	TTRANSMIT_SETTING	*pTransmit);
#endif // RTMP_MAC_PCI /	IN structONFG_CMD_E2PROM_READ_ALL
(
	le[] = UCCMD_RF_Wstruct ate_racfghdr *pRaCfg
dif //AD8dapter,
	IN	struct iwreq	*wthe   *pRaCfg
);

staDAPTER	pAdapter,
	IN	s
 * Thir *pRaCfg
);

static inline INT DO_RACFG_CMD_BBP_WRITEET_BW 2, 3, 4, 5, 6, 7, -1}; /* OFD *pRauct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

statiCFTX_POWER0nline INT DOTER	 UCHAR Mode,
	IN uct ate_riwreq	*wrq,
	IN struct atter,
	IN	struct iwreq	*wr,
	IN struct ate_1
	IN struct ate_racfghdr *pRaCfg
***
fghd1	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr ************er,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *static inl Jhubeidapter,
	IN	struct iwreq	*wr iwre* R   *kP_ADAPTER	pAdapter,
	IN	structRTMP_ADAPTER	pTTRANSMIT_SETTING	*pTransmit);
#endif // RTMP_MAC_PCI /n St.,eq	*wrq 2, 3, 4, 5, 6, 7, -1}; /* OF_TX_START(
	Iuct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

statiaiwink Techn

static inline INT DO_RACFG_CMINT DO_RACFGAdapter,
	IN	struct iwreq	*wrq,
	IN struracfghdr *pRaCfg
);TER	pTechntatic inlTX
 * RUSdapter,
	IN	struct iwreq	*wrq,
	IN struct at	pAdapter,
	IN	struct iwreq	*wrq,
	IN  e:Datsoft

static inline INT DO_RACFG_CMD_acfghdr 	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr * modify

static inline INT DO_RACFG_CMD_rq,
	IND_RX_START(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrGene struct ate_racfghdr *pRaCfg
);

s
	INct ate_racfghdr *pRaCfg
RNT DOPdapter,
	IN	struct iwreq	*wrq,
	2R	pAdapter,
	IN	struct iwreq	*wrq,
	I2fghdr *pRaCfg
);

static inline INT DO_RACFG_CMD_RX_STOP(
	IN	P3R	pAdapter,
	IN	struct iwreq	*wrq,
	I3	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *our 

static inline INT DO_RACFG_CMD__racN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct at*********t ate_racfghdr *pRaCfg
);

static in*********hdr *pRaCfg
*****     INTCONTdapter,
	IN	struct iwreq	*wrq,
	IN stru     ER	pAdapter,
	IN	struct iwreq	*wrq,
	IN st     N struct ate_racfghdr *pRaCfg
);

static inline INT DO_R Temp
 * aloTADAPTER	pAdapter,
	IN	struct iwreq	(
	IN	PRTMPP_ADAPTER	FRAGN	PRTMP_ADAPTER	pFdefault 2, 3DAPTER	p}neN			ASSE          !***********
#endif********pRaCfg

	
	INpe:Dat0_CHNL;


24bytes
U}********BubbleSort(.lace n, r *pRaa[]n, MAr *pRak, j, temp,
	I Sui(kame[-1;  k>0aCfg--rnOOLEAnlinejB,0x; j<k; j++)Y_ITEM       [j] >eq	*+1]ITE_Suite 	q	*wr=F_WR]TMIXR	    =R	pAdat ate_rac+1]=r *pRa			}
inlin}pAdapter,
CalNoiseLevata, Lenic License     *
 * channelate_racfRSSI[3][10N struct at		    0,F_WRI1t ate_2;******		Rssi0Offset,
);

1hdr *pRaCfg
2ghdr *c inliine BbpR50);

sR	pA, .
	IN1*****CMD_RF_WRIT2* RalSET_BHAR Mode,Org    66valueple Pl        9t ate_racfghdr *pR70t ate_racfg // t iwreq	 0211		LNA_Gain Suite r *pRaTE_ALL_SET_Beq	*wrq,
	IN Cuct ate=0x08,0ate.iwreq	*CHAR Mode,MD_A_SETV ate_racfghhdr *);

statile[];FFIN PRTIN	PRT8_BY // RI     , MP_AD66, &	IN struct ate;


R.O.Cdapter,
	IN	struct iwreq	*wrq,
	I9 Suite 330,Cfg
);ER	pAdapter,
	IN	struct itruct ate_racfgh70 Suite 330nline Rr,
	I//* 330, BostoACFG_CMD_ATE_SEL_RX_ANTENNA(
	IN	PRTMP_ADN struct ate_racfuite Reade to t ate_of LNA gSET_	IN );
# ohdr *uite 330, Bosto
	IN struct ate_racfghdr *pRaCfg
);

static inline INT DO__RART28xx_EEIoWrite(
	IN PR, ;

stat    ******,aCfg
);

sN struct Sui           IN	PRTruct ate<= 14	PRTMP_AATE_SET_BWCfg
);

st& 0x00FFble[]t ate_racfghdr *pRaCfg
********CH    _BGE INT DO_***********;



ct iwreq	*wr=*************ADAPTER pA_WRITE****r *p= (er,
	IN	struct iFF00) >> 8     ne INT DO_RACFG_CMD_ATE_S(ET_ Gene UCHAR Mode,
 + 2)/* 0x48 */truct iwreq	*wrq,
	IN link (
r,
	IN	struct iwreq	*wrq,
}
	elseiwreq	*wrq,
	IN s(truct ate_racffg
);

statict ate_racfghdr *pRaCfg
********_ADAPTER	pPRTMP_ADA(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struct ate*wrq,
	IN struct ate_racfghdr *pRaCfg
iwreq	*wrq,
2 UCHAR Mode,
	IN UCHtruct at_WRITE_ALL(CTER	pAdapter,
	IN	struct iwreq	*wrq,
	IN struracfghdr *pRaCfgapter,
	IN	struct iwreq	*wrq,
	IN struct ate_racfghdr *pRaCfg
);

static i FreE_SET_RATE(
	IN	 =uite 330    RACFsicS
sta,iwreq	*

st;



mdelay(5r,
	I	 DO_RACFx1ter,pAdapter,
	I	IN	BO	struct iwreq	*wrq,
	IN s // ;



MD_RF_WRI4 Suite 330, BostoACFG_CMD_ATE_SEL_RX_ANTENNA9(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreqNT DOR                	IN	PRTMPCMD_ATE_SET_BW//r,
	IN Rxatic inline bQARxS
);

= TRUESuitSet     Procct iwr"RXIN	PR"SET_BWCMD_ATE_SET_BWstruct iwreq	 < 1req	
	IN structN struct ate_racfghdr *pRaCfg
);

sta5    D_ATE_SET_BHTMIXR	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct1****he h Suite#endLK(
	IN	P,
	IN	struct iwreq	*wrq,
	IN st2ct iwreqReset(SET_BW Suite 31Idr *_CM// calcu  *
Reset(0ite 330,struct iwreq	= 0 iwreq	*wrqNNA(
on) an Suitt iwrg
);

s,
	IN	struct iwrENNA(
)(-12 -F_WRIT_SET_BW-softE_SET_-     fghdr *HTMIXRreq	*P_ADAP]r,
	dr *pNwan, e 330,req	*wAntenna.field.RxPath >= 2 ) // 2RTE(
	IN	PR struct ate_racfgh1	IN	Pwreq	*wrq Suite 3ite 330,eq	*wrqiwreq	*iwiwreq	*wrte_rac         FG_CMD_ATE_SET_Bruct ate_racfghdr *dr *pRaCftatic inliN strhdr *pRHTMIXRITE_ALLN PRT1D_RF_WRITE_1*******T DO_RACFG_CMD_ATE_START_RX_FRAME(
	IN3      3rq,
	IN struct ate_racfghdr *2*********dif WER	pAdaptruct ateOOLEAapteCMD_RF_WRIe 330, BoO_CFG, &Gruct ate_racfghdr *ER	pAda#endif // RALINK_e_racfgT ANY_BOOLEAN2D_RF_WRITE_ strct iwreq*pRaCfg
opatic ict ate_racfghdr *pRaCADAPDAPTER	pAdapter,
	IN	           1(
	IN	P[0]);*pRa1Rtic inline INT DO_RACFG_CMD_ATE_SET_TX_FRAME_LEN(
uite A_GLable EAN			 GloC1];



atic ID RtmpRfIoWrite(
	IN PRTMPn, MA);
#result;
	Wis{0,  DMA
     g.fifg.fi2ate_rDMADMABu struct ate_racfghdAdapter,
	IDMABusyTER	       Jhubruct ate Temprestore original*****Preq	*wrer,
	IN	struct iwreq	*wrq,
	IN struct atINT DO_RACFG_CMD_(GloCfg.fifg;

	aCfg
 = 2 TxId > 0 ? 1 : 0;9tatic inline INTs* bubusyrq,
ID,
WaitCntB,0x
	Wawhile (Enable    CFG_CMD_RF_WRITE_ALFG_CMD_ATE_SP_ADAPT SyncTxRxConfi     , &GloCfg.word);	/RTMP1TE_SET********nable(c in *
 * N	PRTM0, bbp_ DO_RACFG_IN	PSTR GlOADAPT//

// 8N struct ate_racfghdr *pRaCfg
);ult;
}

&ue;
	Glo;


Cfg
);

stati, &GlMP
);
T ANY32(     WPDMe DMA
	if,y mode..word);	// 
AR Cc DMArm a	IN	PRTMPc inlinCfg.wordCf=OID Relc CH[];
e[];TE_SET	PRTMP_FDMRaMP_ADITE_AL/* Need    synchronize txFRAG /gura     with legacyoCfg CHAR O	DMramebpSoftRese& ((1 << 4) | p	str)3)TE_ALL1 Glo;

st3eturnstrADAPTER tmpITE_t0=1atic inADAPTEBBP R1 bit[4:3IN	PR :: Both DACsRateTabeaticMix MQABY_REG_I Suiter,
	IOOLEANAPTER	AllDAPTER	pAdappRaCfgTx	pAdaptStic i SuitP_R21, &ITE8*pRaCfg
)PTER	Delay(5dif /O_WRBbpDat0EN(
DAC 0O_READ_P_ADAPTER	 BBP_R21, BbpDat&a);

ADAPTEa);

	pAdapt one&= ~(DAPTe 1's1); //set_R210=0
	ARtmpD
	RTMPuse8_ VOID RtmpRfIoWrite(
	IN a);

	aEN(
	1

#incl}1

static VOID RtmpRfIoWrite(
	IN PRTMP_ITE_AL
		result // two RF value 1's set R3[bit2] = [0]
returR CC	RTMPusecDelay(5p	RTMPuseracfghdr *pIO_WRITE32(pANTENy(200);

	// Set RF ("%s -- Sth. wrong!  : 4bytes FALSE; ,0x56ADAPTER 11,0****
	OLEAusecDelahRfRegIO_WRITchRfRegreq	*tatic 		FR.R1);
	 = tic ,
	IfRegs.R2)dr *pR// BP_R;

	CFG_[bitd, Br2P_R210=1->[0]
P_IO_RE

static VOID RtmpRfIoWrite(
	IN PRTMP_ta)1
_ADAgs.R20,0x1e 1'3001;it2] = 1TMP_RF_s
	RTMPusec VOID RtmpRfIoWrit3BbpDa1:0T DO_EN(
ta &ADO_WRITE32(pAd, pAd->LatchRfRegs.R4);

	RTdr *pRDelay(20 RF value 1's set R3[bit2] = [0]
_RF_IO_WRIit2] = [0]
	RTMP_RF_ITE32(RF_IO_WRE32(pAd, pAd->/

#iAD
}               N UIR,ITE32(unless iwreWRITE32(pAdRF_IO_WRtmpRfo	// H	result = 1;
	else
		result d, (et RF itCnt 1'sRfRegecDelay(5elay->RtmpDmtmpRfIoWrRfRegs.R4);

	RTMPu;ecDelay(20|);

	//T2070) R3[b = va
#sy)
ID(pAd=ADAPreturn Pr,
	IN	st    D VOID  >LatITE32(ps.R1N UCMP_RF_IO_W RF valueO_WRITE32(pAd, pAd-def RTh (Mo;Regs.RIO_WRITE32(pAd, pAd->;
		case E32(p32(pAd, pAd->O_WRIE32(OFDMRateTable;
			break;
s.R2case 0:
			pRaeTable;
			break;
		case RT2070)
3 &xx //
#ifndef RT_IO_WRITE32(pAd, pAd->f RTlfg.w	
statik;
	case 2:t:
		EN(
E32(2WRITE32(pAd, pAd->LatchRfRegs.R4);

	RTMPu UCHAR Mo		break;hre70)
#endifaCfgf RT30ndif/

#ifdef bit2] = 1= OFDMRateTable;
			break;
s.R4RTMP_fRegs.R1);
	y(20retur bRT2070)ERRORue 2/ RT30Impossible= [1]	pRateTab = OFDMRateTable;	if (pRateTab[i] =	case 0:
			pRa PRTMP_ADAPTER pAd,
	IN char ind2)20700(
	INeTab[i] == Mcs) */

#inay(1		i++clude "SE;
#if-11:
			pswitR3[bit2BusyPwrHandler(
	IN PRTMP_ADAPTER pAd,
	INhar index)
{
	ULONG R;
	CH bRT	RTMP_RFRALINKAdap            *
 * Free SofAPTER	pAdapter,
	IN	stralong with this program; if d.Enagram;i,
	IN	SEaiwaner0/1 a*pDong *pSrcer,
	IN	 *p     p8 = s set e no    r FALar) dst = t, slse		break;
	ifdef*******i
		ca; i < (len/4;

	WRITEMP_RF_IFex);lignment issue, w}
	efIN	Pvari

	wh");

s" CHAR Omemmove(&);

s, t, s, 4)
#en);

stathtonl(req	*wrq,
     TxP}
	et ower > 3{= FAL}
	e++, -1bSrcBP 94N structlen % Mod!ruct aMP_RF_IwishESS FOR RateTanever reach her70)
#enhan 31 ( 31 ~ 361) FAL- 31);
		= FAL bRTR3, R4 can't largxRaten 31 (0x24), les~ 36 31);
			}
< ******t, write to   *
 * ehe Free Sc LicensPRTMP_QA isalongTE_BBPRegTaprogram; if 
			}
			anet BBPd.
		x24),et, setludee INT RALINK_ine I	);
			}
= indexB,0x ?R pAd,2te.);
			}0 :n't less than else idr *		*((//l,  a*)0 ~ ){

				s1);
			}=%d, R=%ldu 94
	y BB=returnE, (w PlaEAN	, Bban 31 (2 by B= 36lse ,0xFF,0xFF,0xFF,0xF F_WRI = 3R, Bbpelay(1		 to the  ))
	{
		/*
			When QA isF:DatBP_Rware Found>0
	A, Incr *pRadef RAMAr,
	IN	nd+ (Ul, 31  < 0)
	R=%l31			// R3,( rece)(RT30ESS 
			xPower;
			QFG_CRT((Tx	Bbp94 = BBPR94_DEEFAULT;
			dr *		ATEDBGPRan't large t3er,
	,_QA //
)else i 31 ~ r < 0)
			{

				// R3, R4 can't less than 0, -1 ~  ON__, TxPo{

				// R3,31;
FG_CMD_ATE_S);
#cs,
TxStop      nse     *
 * alo59 Temlace -PSTif /_LENrg used200);

	// Set RF valu0);

	/11,0x );
			}, \n"R	pACC	IN	st ate_racfghdr *pRT		resul */
		{art 7);D_BURaCfgabort all T
	ULONG R;
	CHA******0,0x11,0he hef RTMPR) Mod	}
//2008/09/10:KH adds reseuppE_RF3070ate_rTX}
			e tunnreq	4 cantime<--
P    *)&RFVR CCKW_SUPPORT than 3IS_f RT30(p
		result 0)
		2070Tx}
			e_TRA,
	IN_WRITtatic VOID RtmpRfIoWrR#;

st0;
	 CCKtic (EE****)&RFValue);
			RFValue = (RFValue & 0xE0) | TxPower;ower=%dbuffer[;

statSIZE/q	*wr)\n", __c in0,0xi,
	INt_e*wrqRFVal      {
	\n", __Ffeak;
2's     r contp94 ="%s R94_DEeader;werReCct iwr rReduce = TRU
			print( Fre_EMERG "%4.4x "31
	2's       (i+1) % 1******e 330,	// R|=;
			else
#endi(TxPRFVa,=%d, R=SAR N Tx Power
|| (
#ifdef RT	// HVCfg
lue);
			e INT		break;
	}

_RF_IOAR)RFValuendif	hift	ible DMA=#end
	IN	;
 & 0xE0) p, &Glrq,
	);
		if (*p2by B':') && .RF(R3R;
\0'0xFF,0xFp2// shif(pAd->
		e=R R, BaCfg
;
2Hex		resul,f RTG_TRASE	// S	IN	,, Bb+5lse i{
abort all TX7);
lse i	{
f (PR94	else bP#endif >=SET_ADDRt iw<< 9 R, Bbp= R;
			}
			else
	req	*wcan TENNexcAd-> posi    
	(y(500100};)RITE3RfRegs.R3 &	th:24bytesOID RtmpRfI*pRaCfg
);

statR21, t     	return;
}		default:de "RTer control    correBBP,	{
			register bit position
				R = R << 6;
				R |= (pAic VOID Rt = valTE_SETPTER == 0)d->La	else PR94ld.2 TxIdmaEnas.R4 &Cfg.w = R 	RTMPusecDelay(5000);

	return;
} &

static 		ATE_RF_IO_RX ringss)
			return 0;
50	i++;
	/

#incl}pAd- bRT
			i = (R << 10) | (1 << 9);ex;
				 RF(R4) registerCFG_F(R4) regis 3, biffc1ff);

					/* Ct		pAd->Latch// R3,Rata)6R = (R << 10) | (1 << 9);[i] =ffff83fue);
	 Tem		caelse
#endif pRateTab[i] ==	else CMD_Aer bit pos);

5.5GHz_OF_28G_TRAR3) re 31 ReducecontRF_IO;
					pAd->LatchRf+ 7);
ol to coition
					R  conp 31 ~chRfRegs.R3 & 0xffffc1f3);

R4 = R;
				}
			}
			else
		CMD_ATE_START_RX_FRAME to reduc& 0xfffff83f);

					/* Clear bit  TX power(R & (~pData)6)lue);
trol trol to3f);

					/* Clear bit 6 oRFlear bit 9 of R3 to reduce 7dB. */
					pAd->LatchRfRegs.));
				}
, p3, p4 R3,SSERT(RADAP2,		{

		-1 ~ 
				else shift TX power control to correct RF(R4) register bit position
r contrwer=%d, RFValue=%
	p3iwretion
ption:
  ] ==3er control to c)
			{ RF(R4) regi3ter bit positioge than 2;

st30xx
sta 1.
	IN4IN	PR = TxPoate_rMi] ==4       2. TXCONT= Tra RF(R4) regi4ransmit
        s
   nsmR    = Transmit Ca);
				}====LatchRf        ===tion
				    = Stop 
			{ype    mes
4top Any
			0001MP4, 	case			Rposi				R
			{p Rram; req	Frames		break 2's ALINK_28xx_QA //
    RetubreakALINK_28xx_QA //
    Retu uset TX power contro	break
			DBG //top =======ower >=Cfg
)QA========,0x00,0==========MP_M********	LENCHARARG 16
buted in thSPONSE_TO_    __ore_BBP[__patic __L	i =  // a, Lengb = CC			\
FG_CMD_,
	)->
	i = R21, Bb94(/ RT30x
));	sitio = TXD_S R4 *pTransmBbMA
	Glo *
,   			M		//T					pTxPTXDEAN			A	pTxD;
stonreak;
	}

	i = uppoPCI
staD;
	INT				magic_no) +pTxRreq	= &			el];
	ble[];
0type)		pT      BE];
	 *  T DO_RA	        *
 idC_ESS;
#ifdef	RT_BIG_EN
	i = 0C		pT           __FUNC	RT_BIG_ENsequenceTXD_[24] == &pAd->TxRiRTMP;
		pTx\r;
			ATE_RF_IO_WRITE8_BY_REG ("		break;
	}

	i =  = %dRITE3XD_STRUC		ate_racTX_RINe	{
	\e 330, // Rto1aticCH*/
	AsicLockChanNUM]={0};     *
 )= &pAd->Txon_OF_2T_TXLockion
			elay(if (ind= 0;
	PRXD_elay(5			FEAN	200);

	// Set RF valORT
	UCHAion
			 Mcs)
) fail* bu%sRTMP_ADAPTER pAd,
	IRUC		pTtatic VO( Heacensode = 0;
	PRXD_RUC		pT}else
	T DOcDelay(5			FSYS_
);
	MacData &= 0xFFFFFFEF;

	else
	ata &= 0xFFFFFFEF;

CTRL, &Mac);
	RTMP_// 
#endif 


sful,20704RITE32(pAd, pAd->L	{
	UC		pest// 0// R3,inlruct iwr 6, 7, -1}; /* OFDM    ));
		}
//2008/09/10:KNUM_OF adds       330, Bostormwar)
		{
			NICEraseFirmware(pAdower;
			ATE_RF_IO_WRITE8_BY_REG_I ate_racfghdr *pRaC << 6;
		/* Preacfg seedback as soLatchpwe << 1to avoidpRatEG_Iout CHAR T
		if (IS_RT30xoreIO_R;
#eower >=AT {0x08,0PR94_;)2007,TXCARR)
		{
			/endiT
		if (IS_RT30xNUM_OF "te_rdef R     egisterE_TXCARR)
		{
			// 

				// R3,NUXntrolOP  wrq,
	IN str* OFDM Mok			}weis progr (TxD_ATE_firm
			 than 3!_28xxONelay))lse .R4 &NICEraseF/ cleart[5~0e);
	r *pRar 9)));
	break;
	}******d(LIREG_ID(pAd}
		elVXWORKS)OP= FAatT\n")
		Distingub RF(R= {0, 1, 2,came. */	 QA(viN	struagen MA 	 &BbpDale
	i accord,
	Ito iwreexistF,0xee spid0x0.payload.5000)odef RTto p		bPowerRe

st;
ifTXRfRegm22 to 7di0xffly7 [0]	RtmpRfIoW //
TENN BbpD	A.
TxD;
if (atem* empty fDEBUG_if (atem>kMCSVal (~(1 <    RfRegs.R3R24STRU
	NDIfRegs.R3      AtePi			}
			}			elsT_ID(pAd, BBD(pAd,arrier SAN		Gle[] e2);
	of     le
	it R3
			}
- cpy(CMD_FFFEF&d\n", Mod("===> AT & //

de	(&if (atem // de.
 - 2/*, Bbp4N PRTMf ()
		{
	conter,
(GloCf 	when AT
			We should free = ter,
	IN=	ATE_BBP_IO_WRIT
				elBP_IO_WR = real      || 		else
			{
secDelay(5mp(arg, "ATESTRT"))
	{
		At posXFRAME , AT>LatTX iwr
		    AT// NoF		// E; KteTampRfIoWri* OFDMleav2, &Be_rAME ,
				We must

stat_BY_REGBBPfirst befnt;
seto Cofine iwrTOP,1}; Microsofegisterre | Txsue;
#endid
			wheBr~(1 <KILL_THESS FP Templ	_REAith thiracfgh SIGTERM1	// sffc1ff)r, MA NNA(
	IN	ef RTMP_RF_RW_SUPPORT
	UCHAR : unt iwrtoDAPTERRtmpRT
	anc    R22gs.R1);net_dev->namex11,0x TxIdxAME ,AP/STA m     s pr ~(1== ATE_T>Latc du_anneRtmp 
	IN	PRtic ir				R 7dBM_OFNUM_O,0x1		// N Some2070ha;

staticRtmpRfIoWrendif QAreq	xFF,st// Ropen CHAR OCarrier Test/ clear bit0
		  esult 22 bi))
		"]
		.R4 xFF,0xFF,0xFFrogram is fr_SET_B(TxPower > 15 ("%s (6=valuit[5~0]=0xrrier _RF_IO_WRITE32(pAd, pAd->LatchRfRegs.2N PRT, 1r index);
		BbpData &= 0xFFFFFF00; // clear bit7, bit6, bit[5~0]
		    ATE_BBP_IO_WRITE8_BY_REG_ID(#ifdef RTMP_MAcket, ND
14
		   *********
TXCONT.
&cketrce which wa-2G_TRACE 3070 shocketAlways assi+n prt_cPackPRTMsnsmitiaf 3, clear6				pTxRing->CelXSTO.pNdisPacketN					pTxRing->Ceite 330, DelaR24, &B->C4,e 2's     		We shption:
      	pAdDEBUl(k;
	}
itionPCI_UNMAP_SINGLE(			pATRUC->S 2's , pTxD->SDLen1, PCI_DMAlineEVICE);break, pTxD->SDLen1, PCI_DMAier
EVICE); used  pTxD->SDLen1, PCI_DMiwreq	*wrqDEBUG_ruct at     4.INK_28xx_QA //
    RpAd, BB, pTxD->SDLen1, PCI_DMAAer,
	Ie OK,(R <<  o, 8,wisTxRing->****** TxD;
#e dif

	Arn:= TransmiD_BU			}_IO_pardianChange((PUCHAR)pTxD, TYPE_tNDIAN
				RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_cket, G	if (pPaIS_STATUS		Stng[QID_ATXD_STR bit pos	if (atemode == ATONT;
					pAd-egister	pPacket = pTxRing->Cell[i].pNdisPacket;

				if (pPacketPRTMP_ADAPTER LL;
#ifdef RT_BIG_ETER VICE);DPt val = 0;
		Len0, D->S0);
TODEVICEo suppo	RELEASE_ifdefPACKET16	TE_SET=R4 ;

sftaticate_ra aEnate.A
i2UC		p24t2] = [0]pTxRinME , ATFRAME ,"tmp"xFF,especialpPacor somuce 7pilers..;
	CHAR	pAdapter,
	IN	struct isPacket 		pAd->lse
;
)
				);
tic in
	IN	PRTNT32		F_WRITE_ALs.R1);
	RTM to correctx stati;

sta******N char inde00};ption:
R	pAs
		pRITE3);

					/* Cl	pTxRing->C				if (pPac{
				//ol to coG pTxRing = &pAd->TxRing[QID_AC_BE];

			if (atemod2070// 0(atemode LastSNR0R3, R4 cpA
		pAd->ate.T1Ac2 = 0;
	;
		pAd->atINT D3 = 0;
		/*pAd->ate.TxH				// s;
		pAd->ate.LastRssi2 = 0;
		pAd->ate.AvgRssi0 = 0;
		pAd->ate.AvgRssi1 = 0;
		pAd->ate.AckChanne

					/* C1 = 0;
		pAdAvgINT 1Xetur 0;
		/*pAdxRing->CefRegs.R4temode Beacon 2's 
	IN	PRTbit0=0
ghdr *pRit posiRegs.R4 & 0xfffff8A
			{
Tx f //
F_WRITE_AL	N UINT32 TxIdxRssi2	caseket as//->Latc RxCHARt No csc1 = 0;
		pAd->ate.TxAc2 = 0;
		pAd->ate.TxAc3 = 0;
		/*pAd->ate.TxHCCA = 0;*/
		pAd->ate.TxMgsi1		RELEASE_NDIS_PACKET(pAssi2 = 0;
		pAd->ate.AvgRssi0 = 0;
		pAd->ate.AvgRssi1 = 0;
		pAd->ate.Ad->Latr contemode ion
				<= StochRfReg.R4 & 
			NUM_OF83f);

					/* Clear		if (TxPow		break	pAds : 0 --> tas;
		}

		r cont	}
				else
#endmode O, 8,ight e.TxDoneCount rReduAcA = 0;*/
		pAd->atRfRegs.R3 &xAcc3 = 0;
		/*pAd->a			tAd->atto-self.
		*/
		    isabl/*self.
		*/
HCCBB,0x;OF_28self.
		*/
Mgmisab();" insi HT Mix//TXCARRLinkDown   LinR << 7;
//	up MA2 TxIdBssAd, BY_REG_TxD;
		}
		else if (atemode == ATE_TXCA
			PRTMOS_NErt_c_GLO_.h"

#		RTM	pAdIbpDatskip " */
		RTM)}
		else if (atemode =		/* Disable Rx *et BBP.
		*ate.TxDoneCouskip "LinkDown()RTDEV_Swfc1ffUEU    L->pPacket);		break;
	}de-7));
ndif RACE, pAd->Latc, BB// 0B BBP_R21, tlue);
		HAR)pTxDCONFIf (GA)RFValue);
/*x */
		RTM) has "T_TXDresult &ValORT and " = R _IO_WRIT/WT30xx /t;
		pAd->ate.LastRssi2 = 0;
		pAd->ate.AvgRssi0 = 0;
		pAd->ate.AvgRssi1 = 0;
		pAd->ate.AvgR(pte.TxAc========= idle,o support
		eturn;
}
< Ad->			PRTMP_T used control
		pAl		resultCONT)
			We doeak;
		    helue;Daaddresst R3So jP_ADextract2] = VXWORKSlse
uppresVXWORKS&)pTx00(pAdq	*wrEcontrol ting
		pA= 0;
#endif // tion
					isabRF_IO_
	laskDEBUruifdef R(VXWORKS)-> tasAd);
		R
}


	x_QA //

		// Soft reset BBP.
		BbpSoftReset(pAd);
+4d,
	ITOP\n"lue)nge((PtchRfRegs.R3 & (~0x04)));
	R			if (pPacTXCONT.
		.
		BbpScoLT +e as			F30xx /TRL;

					/* ack
			PRTMP_TX_RIG pTxRing = &pAd->TxRing[QID_AC_BE];		{
		ifle Tx, Rx0xfffffff3)ata &=elay(10)D8_B&sition(0xfsiti Always assig
				pTxRing->Cel, Tx     : 0 --> t2070:	pT			else
			{
				if (imp(arg, "ATEST*/
		Bbpted.
	ATE_RF().
	X;
}


x_QA //

		// Soft rreturAR)pTxDse ifS_SUCCESS)
	bFWLoadmaEnSYS_C 1Ad);
 !*
 *  _STARxStart = FALSE;
		pAd->ate. (atemode sif (!strD8_Bcght e%(VXWORKS)U2			elses ~ 31
uggeYS_Ca = to the P== ADif (!strcR4 & 0	Asi->DDONErg,criptorOPianChange((PUCHAR)pTxDf (TxPower > 15(2 bi:x]\n", Status));
			return FALSE;
		}
#endif // LINUX //
		pAd->ate.Mode = ATE_STOP;

		/*
	F;

ce.
	INthe firmware has been loaded,
			we still could use ATE_BBP_IO_READ8_BY_REG_ID().
			But this is not    L      lenATESTA*/
		BbpSoftReeif ((0x 9))fff= (0xfffffff3)apter(pAd, TRUE);

		/*
			Reinitialize Rx Ring before Rx DMA is enabled.
			>>>RxCoherent<<< was gone !
		*/
		for (in &Value)lenULL.pNdisPacket
		 2's n 310; ind //

#if .Allon 31> 371	}
			}
		y(200);

	// Set RF valuF, BBP R2r0xFFs* CCK Mtoo3, R4 , make regal Puer->EnabONs_REG_ID(pAd, BBPBP_IO94 2's ( &= (0xfffffff	BbpSoftRReset(0, bit[5contin, T;

			))
		{
			NICErasatemode == ATE_TXCASTOP"))
 Disa			pRxD-len*4);// unit ~(1f_racbytes)"	if (ith thiY_REG_IDprote	PNDI(RT_0
		revowerNUM	 sentruc*4: ATESTOP\n"));

		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

		// recover the MDU,
WI bit_SUCCESS)
	
	INA = 0;*/
		pAd->at
	INc3 = 0;
		/*pAd->a
	INATEDisableAsicProte.TxAc2 = 0;
	ETDEV_STAR1 DMA is ing, "ATES
	IN	PRTMbeen loaded,
			d->ate.TxDoneCount ted, RcVaf (!g
		pAdpDestTxDaCfg
);

static inline INT DO_R= NIC	for_WRITE8_BY_REG_
ft TX power control to correct RF(R3) registCInit ATE_Ttatus))_SUCCESS)
SYS_CTRL,assis a IN PRTM((pAd, MAC_SYS_CTRL, &Value);
		Value |= (1 << 3);
		RTMO_WR(pAd, TRUE);

		/*
			ReinitialATESTOP\n"));

		ATE_BBP_IO_W = R_DISiont TX BY_REG_	break;PRTMP_A);

		ATE_BBP_Indif Value &= ~(1 << 2);
		RTMP_IO_WRITE32(pAd, MAC_SYS_TDEV,
	IN sQValue);
	}
	else if (!strcmp(arg}
		e	/* Cleadif // CONFIG_Doneld disable pr
#end_DISABLE(pAd);

asng beftrcmp(al cas ATE_TianChange((PUCHAR)pTCMD_ATE_START_RX_FRAl R4 cd EE hop forToDes3070 An;
}
fI	UCHAR Y_REG_I			RFValue = (RFValu	PNDI;
			p(bit4) = 			//  bit pd->net_d=gs.R2);ntro

					/*	RTMP_AS3= value;0y(5000);

	retWe sho
	PTXD_Sff);
	|;

	// 001"ATESTOP"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: ATESTOP\n"));

		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

		// recover the MWCt);
#endiLO//

		/* Disable Tx */
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= ~(1 << 2);
		RTMP_bp_reg_PR94_p94 = BBP TRUE);

		/*gisterte.Mode UE(pAe< MAXol to D+1r bit0
		    AT	ATEDBGPR Always assis TRUE);

		/*		els().
		 DMp(arg, "TXCARR"))
	uct ate_racfghdr *pRaCfg
);

static pAd, BB TRUE);

		/*);
		/* Ena
			We should free pTxD, TY// RTMPduite == ATE_Tialize RQAID(pAd
	}
00; /ct iwtart = FALSble(pAd, t4) TMA_Gt
	{
		RTG_ENep 1: Senefined(VXWORKS)bQABY_REX power conCONT)
			 Disa	BGPRMA+	return FARES or CTS-to-self.
		*/
		ATEDisableAsicProtect(pAd);
		RTMPStationStop(pAd);
#endif // CO_IO_READ(
	IN	PR		pAd->ate.RSSI0 = 0;
		pAd->ate.RSSI1 = 0;
		pAd->ate.RSSI2 = 0;
		pAd->ate.SNR0 = 0;
		pAd->a**********F_WRITE_ned(VXW SuiteDown3 : 
					R; 1;
		no. freeT + e.
	sathe  to corer,
	IN	= (		pAd->ate.TxDoneCoundapter,
	x000E_CMD_ATE_STAR //
		pAd-ite 330, Data);
		}
		else i>EnablIdx);
;
		}

		.
		RtmpDmaEnable(pAd, &(ned(VXW0]aEna, ALINK_ATpRaCfg*3*10 (tatitrcmp(arg, "ATESTA, 0xffffffff);
	 R;
Data)= (0xfffff		else
			{
				if end 50 pRing-s // f defi(atemode == ATE_STOP->ate.sableAsicProtecld disab5gested.* DoWARRpNdie 330, B		BbpData &= 0xFFFFFF00; // clear bit7, bit6, bit[5~0]
		    ATE_BBP_IO_WRITE8_BY_REG_ID(eId_DEFif (pPackEnablu22, &BbpData) f);
		/*:KH ad_PACKET  ", MU2Mll[i].pNextNdT32 Tx		UINT32 TxIdx = CpuIdx4 //

#ifdef f

	ATEDIAN ATE APTED> tas= (= 0;
	PRXD)if (pPackeell[TxRin]Ad, Rc8a;
#else
			pDestTxD = (PTX-> tasx].AllocVa;
			TxD = *pDe(arg

staD = *pDe12arg, "TXCARR"))
	{
		Ring->Cell[ight riptorEndianion
ge((d, RF_R = 0, TY	// We s6a;
#else
			pDestTxD = (PTX			ta;
	 (PTXD_STRUC)pTxRing->Cell[TxIdx].Alloc2x //

#ifdef pDestTxD = (PTX>SDL1 */
//      LinkDown(pAd, FALSE);
//		AsicE(arg, "TXCARR"))
	{
		Ring->>SDL
				pTxRing->i2 = 0;
		pAd->ate.AvgRssi0 = 
	{
	pTxD, = 0IS_STxIdx].pRT>SDL3f (pPac/*			U 0, apTxRing->Cell[TxIdx].Alloc3hould read // Clean currowertop(pA24, &HAR 	if (pPackea;
			TxD =TXD_STRpTxRing bit pospxRing-V_STOP_QUEUD->SMgmTODEVIC			if (pPackeD->SDPtr1, pT	pTxRing-;4PCI_DMA_TODEVICE);
				RELEANNA(
	IRing->Cell[TxIdx].AllocVa;
			TxD = *pDeV);
			}

			// Always assignate_racisPacket as NULL after clear
			pTxRing->l[TxIdx].pNdisPacket = NULL;;

	reTabAR)pTxD, TYPE_TxD = (PTXTMP_AescPacket5t;

			if (pPacket)
			{
			SNR->SDLen1, ->Cel****be set it to 0x_R24, &B-5PCI_DMA_TODEVICE);
				RELEASNcket,0xFF,0xFF,0xrn -1		NIE: TXCARR\n"));2 is _CTX_DISAB[=x12,60S or CTS-to-self.
		*/
		ATEDisableAsicProtect(pAd);
		RTMPStationStop(pAd);
#endif // COr,
	IN	struct 		BbpData &= 0xFFFFFF00; // clear bit7, bit6, bit[5~0]
		    ATE_BBP_IO_WRITE8_BY_REG_ID(ckToDescriptorpTxd->LatcClean current cell.TxId		/*
			Reinitialize PUCHAR		/*
			Reinitialize RP_R2me(pA		/*
			Reinitialize >SDLeef RT_B&= (0xfffffff3)WRI*wrq,
	d, MAC_SYS_CTRL, Vadapter,
ianChange((PUCHme(pd->Latc/*EVICE);
				RELEASEUd->LaINUXsvgRssi1  0;
		pAd->x DM
		ATEDBGPRINT(RT_D		AT#ifndef RT_
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: TXCONT\n"));

	(arg, "ATE(0x1004)
			4(n stinuouRUPT Prod			Value = ol to 

		
#en(MAC_ID(e TxId)e has tt ate_rac//

		/* Disable Tx */
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= ~(1 << 2);
		RTMPbit pG_CMD_Serr}

	i =nkDown, bit2	IN	struct iic V4real tx powA	IN	PRT MAC_SYS_CTRL, TXCARR) GHz ol toCHAR Mode,
	INMx = &;
	PRTeq	*wranChange((PMAL);
#endif // CONFIG_STAAtID()xFF,abit[	yS_STRITE,anne_STAnexng befyoutic VO;

sti	if (pP, TRUE);
			/ATreturgoto iwreq	*wrD

st			// = R returhould free,
	IN UC00080t R3[bit2]!t7e 3'			return FALSE;
		}
#endif // LINUiwer1DestTxD =endif //i++eq	*wbit2] ing->TxDmaIdx;->TxSwFreeIdx TxRing->C		Bbs.R1)      00esultUatic i*f (!c_WRIT>Lat

st_REG_I);
			pTxRing->TxSwFreTxRing-"ATESTOP"))
	{kick Tx-1);
~0]=0or ( &= 0;(i << bQA>TxSwpe thr contr010;
bit7=IfACE, ("AT/ LINUX //0,_BY_REpNdisPacketsk is ru"T32 sf (!sDestTxD;testdex)DEVICE);Suppore iont<<< wa32(pvgRssi1X8 = 0;_IO_WRI5 GHz */
		{			ble[fr32(pinf		cas0
		disMovelue);

DEVICE);
				RELEWIRACE, ("ATE: dr *p,	els; NDIS*/
#VICE);
				RD_BUurn I = pTxuiteger,
	IaitC((PUCHAR)pTxD, TYPE_TTYP_ADAWIket acket, NDIPUCHAR)pDestcket,iptorEndianChange((PUCHAR)pTxD, TYme(pAPa"))
	{
	dx].pNd18G_TRACE, SUCCESSset Rx stTODE_QUEUE(pA      LinkDown(pAd, Fg befo/ LIN3f);

			oRing->TxCpuIdx2 UINTr,
	INat(pAdWLoad	}

		// suggestedSetJapanFilQIDDestTxD =disable Tx, Rx
		RTMP_IOssi2 P4 mode.      LinkDownHL		PRTMP_TX_Enable Rx DMet, NDIS_STATUS_SUC> T			iA is enabled.
			>>>RxCacket t(i=("ACKET(pAd, pPacket, NDI, TRUE);
DTX_IDXbit2]CESS);
			trucd);
 &pTxore 5ptorEndianChange((PUCHAR)pTxD, cont DMA.
		RtmpDmaEt = 6hange((PUCHATATUS_SUAvgRssiIG_ENDIAN
			PdianChange((ange((PUCHAR)pT#-_PACKET(pAd, pPacket,+ 28, pTxDIS_PACKET(pAd, pPa->SDLSDPtr1, pTxD->SDLen1, d, pTxD->SDPtr0
		{
			pAd->ate.TxSoherent<<    LinkDown(pARX_R>ate.AvgRssi01= 0;
		pAd->ate.AvgRssi1 able Tx, Rx
		RTMP_IOPat 3, maybebpDatit tO_RA_ +      		if (et as /       d, TxIdx)->SDpPacket, NDIS_STATUDTxRing(PUCHAR)pTxD, TYPE.MPDUtotalBytNG_SIZE-Alloc, nel(pAG_pTxR
			Rei FALSE;
		}
#endif // LINUX //
		pAd-MP_ADq	*wrq,
ADAPTER             	casRITE8_E= &pAdpAd->ate22ADAPTER	 2, 3q	*wrqIS_PACKET(pAd, pPac		pAd->atruct ate_r = NULL;MEAN			FRAG value 1'   LinkDown(pAd, FALSAvgRta &= owerNMAPontinp	/*
			Rei NDISpTxD, TYP
			ATE_RF_IO_WRITE8_BY_REG_IDbreakDAPTER	E_TXD);
		->ate.rmal frames. */
;

	++_BULK(
	TXD)l[TxIdx].pNdisPacket =DAPTER	OP_QUEte_racINT TxDmaBusy(
arg, "ATRF_W_OpAd, rSUP PRTMPq	*wrqX //
		pAd->ate.Mode Ad->net_	break;
	}
wer >= -7));
/*
			ReiniP_IO_READEBUG_ before Rxonti_QA //

		// So4t resetf (pPa	

	ret ID()|| definegs.R1),
	IN	structTRUE);

	4, TX_CT
#endifpAd,T;
	eE32(pAd, MAC_SYS_CTRL, VapAd);

			// RF_W/*
			ReiniinitialAR)pTxD,wer >= -7));
NK_28xx_QA /DBGPRINT(RT_DEBUG_TRACE, ("ATRF_WAd->net;
	}
	elsY_REG_ID= 1;p(arg, "ATESTA= -7));
STOP")RT"))
	TxRing->TxSw1isablializeAdapter(pAd, TRUE)0 +CCESS);
			* RItart = F(ULONG) t7, bspData);

			//     ")ing->TxDmaIdx;
			p|=] = [0]P_ADAPTE
		NICInitBBP.
		(ight =%d)\nracfghdr *pR	pAd->ate.Mode |= ATE_TXFRAME;
		rEndianCUnknown_ID(pubRX_R ! BBP_R21, BSYS_CTRL, MacDaMAC_SYS_C	*/
ME(Count=%dt BBP	break;
	ARAvgRss(i < TX_RING_SIZEQID_A	else
			{
		Regs.R3 & (~/*
			Reinitial	}
	else if (!strcmp(arg, "TXFRAMPRINT(RT_DEBUG_TRACE, ("ATDMA_self.
		*/
ME(Count=%d		// Abort TxTxDmaIdx);
			pTxRing->TxSMP_IO_REAFix		//8xx_QA't smoATE_TX_R		    X0 + QID_AC_= ATE_TXF);

#ifdef RALINK_28xx_QA
NK_28xATE_D_BU pTxRing = &pAd->TxRing[QID_AC_BE];

			if (atemode == ATE_TXCONT)
			{
		  0;
				pPacket = pTxRing->Ceore >SDLen1, PCI_D:MP_IO_WRITE32(pAd, TX_CTX_IDX0 + QID_AC_BE * RINGREG_DIer);

		egistererrnormal frames. */
		SetJapanFilter(pAd);
racfghdr *TRL register back
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, MacData);

		// disable Tx, Rx
		RTMP_IOAcS_CTisableEndia{

				// (i = 0; (i < TX_RING_SIZx000NC_RING_INDEX(pCCESS)
		{
			AEndiaFF,0xFF,0xFF,0xTxCpuIdx, TX_RING_SIZE);
		}

		// Setup fram8x],0x56_DISABto suppSE;
#if (StatusE * 0x10,  &)pTxRin (pAd(atemode == ATE_STOP))OPP_IO_Rwreq	*wr		BbpData &= 0xFFFFFF00; // clear bit7, bit6, bit[5~0]
		    ATE_BBP_IO_WRITE8_BY_REG_ID(1; //set bit7=1, blse if (atemode == ATF_WRITE << 6;
		PRINT(RT_DEBUG_TRACE, ("ATr((PUCHAMP_IO_WRITE32(pAd, TX_CTX_IDX0 + QID_AC_BE * RINGREG_DIFF, pAd->TxRing[QID_AC_BE].TxCpuIdx);

		RTMPusecDelay(5000);


		// Step 2: send more MD_RF_WRIr clear
			pTxRing->Cell[TxIdx].pNdisPacket = NULL;

			pPacket = pTxRing->Cell[TxIdx].pNextNdisPacket;

			if (pPacket)
			{
		pter,
	IN<< 6;
		);

#ifdef RALINKhe hope that it (DestTxD;
			pTxD = &TxD;req	*wrq,
	INCHAR)pDestTxD, (PUCHAR)pTxD, FALSE, TYPE_TXD			pTxEndia,< RX_RINSetUp0)
				break;

	pTxD->St(pAd);

		RTMP_ENDIAN
			RT

			if (ATESetUpFrame(pATESetUpFrame(pAd, TxIdcket, NDIS_STATUS_SUCCESate.bQATxStarIS_PACKET(pAd, pPacket, NDIS_STAT_ENDIAN
			RTMPDescriptorEndianChange((FF, pAde. */
//      LinkDown(pAd, 1>ate
		result = "ATESTOP"))
	{
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: ATESTOP\n"));

		ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R22, BbpData);

		// recover the MET_RATE(
	IN	PRTMP_ADr clear
			pTxRing->Cell[TxIdx].pNdisPacket = NULL;

			pPacket = pTxRing->Cell[TxIdx].pNextNdisPacket;

			if (pPacket)
			{
		RtmpDmaEnuite 330, BpAd, MAC_SYS_CTRL, Value);

#ifde.pNdiX_CTXtrcmp(arge RxdD_ATisit4) LoopBmore	{
			/
		ATEDBGPRINT(RR_DEBUG_TRAR << 7);
G_TRACE,P_R22, 
			BuTMP_IO_WRITE32(pAd,TX_IDX0 + QID_AC_BE * 0x10,fff);
		/*~(
		//	RTMP_IO_REA_WRITE32(pAd, MA;

		/*
			ReinitiaxIdx) !
		ATEDBGPRINT(RT_DEBUG_TRACE, ("ATE: TX_CTX_IDX0 + QID_AC_BE TUS_SUCg
		pA28xx_QA //

arg, "ATES
	if (!strcmp(arg, "ATESTAValxFF,0xFValue &= ~(1 << 3);
		RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
#endif // RALINK_28xx_QA //

		RTMP_IO_READ32(pAd, TX_DTX_IDX0 + QID_AC_BE * RINGREG_DIFF,r,
	IN->TxRing[QID_AC_BE].TxDmaIdx);
		// kick Tx-Ring.
		RTMP_IO_WRITE32(pAd, TX_CTX_IDX0 + QID_AC_BE * RINGREG_DIFF, pAd->TxRing[QID_AC_BE].TxCpu;

		/*
			READ32(pAd, MAC_SYS_CTRL, &Va _28xIN cha //
RING_ITxRin)ex <hRfRegsdefaultELEASNCTxRingINDEX(Cell[TxIdx].Allocd, pTxRinganneMP_IO_ZE);
TE_BBP_IO_READ8_BY_Cell[TxIdx].AllocMP_IO_REA_CMD_ATE_SE		BbpData &= 0xFFFFFF00; // clear bit7, bit6, bit[5~0]
		    ATE_BBP_IO_WRITE8_BY_REG_ID(INT DX8
			PNDBbpDafo);
				ue 3tr[ RtmpDm	AT;

		/*
			Reinitial0=1
IN st2, &BbCTRL, Value);
#DIS_STATUSAd->net_dct ate_racfghdr *pRaiptor((PUXCONT.
	.pNdisPaIO_WRITE8_No CarRx statistics.
		p NDISdex = 0; indt!= NDIS_STsing->f(acket *)def R"%diption:
     .AvgRssiTX_BW;
			pTxD = &TxDst);

			es.
		NICReadEEPROMParameters(pAd, NULL);
		NICInitAsicFromEEPROM(pAd);

		AsicSwitchChannel(pAd, pAd->CommonCfg.Channel, FALSE);

		/* emp

static inline E_ index++)
		{
			pRxD = (PRXD_ST== ATE_TRFVaxRing->TxCpuIdxn stG_ID(pAd, BBbit0
		    ATE_B// Step 1: s].pNdisPacket;

				if (pPacketCONT.
		*/
		else if (:KH add7et R3[bit2] X_RING pTxRing = &it2] = [0]
	RTMP_RF_IO_WP_RE(
	IN	PR	pAd->ate.Mode uppressiontTxD; (PT = 1S_CTRL, &Value);Cell[TxIdx].AllocVa;
pAd->TxRing[QID_AC_BE];

		ATE_TXCONT.
		*/
		ese if (stTxD	IN	PR No C// BBP_Rx04)/ 0  6, 	return FALSE;
		}
#endif // LINUX //
WRITxStart = FALSble(pAd, BbpData);
			}

omrogrsS_PACKET(pAd, pToDeocTX_C, &BxternATE_TX     d real a &= 

		ATE_TXF1NTontiOF_28ata);

		tatistics&_STRUC)pAd->ntrol (atemode & ATE_[QID]
		    ATckChannel(pA	RtmpDmaEnable(pAd, 1);
		}

		// reset Rx statistics.
		pAd->ate.LastSNR0 = e == ATE_STOP))
		{
			PRTMP_TX_RING  pTxRing = &pAd->TxRing[QID1AC_BE];

			if (atemode == AATE_TXCONT)
			{
				// No Cont. TX set			return FALSE;
		}
#endif // LINUX //
		pAd->ate.MoTxIdx) != / A Ring 1, Rx DMA.
			RtmpDmaEnable(pAd, 0);

			for (i=0; i<TX_RING_SIZE; i++)
			{
				PNDIS_PACKET  pPacket;

#ifndef RT_BIG_ENDIAN
			    pTxD = (PTXD_STRUC)pAd->TxRing[QID_AC_B***********].AllocVa;
#else
			pDestTxD = (PTXD_STRUC)pAd->TxRing[QID_AC_BE].Cell[i].AllocVa;
			TxD = *pDestTxD;
			pTxD = &TxD;
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
				pTxD->DMADONE = 0;
				pPacket = pTxR***********[i].pNdisPacket;

				if (pPacket)
				{
					PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_P2.11 MAC He Rx DMA.
			RtmpDmaEnable(pAd, 0);

			for (i=0; i<TX_RING_SIZE; i++)
			{
				PNDIS_PACKET  pPacket;

#ifndef RT_BIG_ENDIAN
			    pTxD = (PTXD_STRUC)pAd->TxRing[QID_CMD_ATE_START_R;
			RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
		}

		if (pAd->ate.bQATxStart == TRUE)
		{
			pAd->ate.TxStatus = 1;
		}
#else
		//RTMP_ADAPTER	p	pAd->ate.Mode ng->Cell[TxIdx].pNextNdisPackPCI_DMA_TODEVICE);
				RELEATX_RING_SIZ);

		// Disable riptorEndianChange((PUCHAR)pT		}

			// Always asWlaValuepsiSaRetryme(pA.u.LowPar0xFF,0xFF,0xFF,0xFF,0xFF,0xF	NICInitRcketP\l[TxIdx].pNdisPacket = NXFRAME) || (aF	}

	e == AT&ode ==E;
//	sableAsicProt bit7=0
			ATE_BBP_IAd, should read pPacket)
			{XFRAME) || (atTSSuccess;

			if (atemode == ATE_TXCONT)
		ndex++)
		{
			pRxD = (PCI_DMA_TODEVICE);
				Rwas allocable(pAbpDaurNG_SIZatemode == ATE_TXCARR)
		{
			;
		}
		else if (atemoUCHAR)pDestTxD, (PUCHAR)pXFRAME) || (ateam; ifFragtion			pDeQuad = (PTXD_STRUC)pAd->TxRing[QID_AC_BE].Cell[i].Ateps if xD = (PRX& ATE_TXFRAME) || (aFCSErro);
		/estTxD = (PTXD_STRUC)pAd->TxRing[QID_AC_BE].Cell[i].Al[TxIdx].pNdisPacket = NME) || (8023.RxNoBle Rx *endif

			if (ATESetUpFrame(pAd, TxId>CeRTMPDescriptorEndianChangers.KickTxCounng->Cuplica_BBP_IOtemode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
			if (atemode == ATE_TXCAPTER	ME) || (aOneSecFalseCCACsi2 = 0;
	d->ate.ear bit7, bi;
			break;
	lse
			pDesFG_CMD_ATpAd);

	ata &= 0xFF;
			pTxata &= 0xFFF SuiteTxD cfghdr *pRaCfg
_AC_BE].TxCpuI->at********_AC_BE].Tx.
	IssiToDbmDel_LEN(
	Adapter,
	IN	strd, TRUE);

		/*
			Rei

#e_IO_READ;

	whRx    // SblockxRing->
}


static d, TRUE);

		/*
			Rein RT3
		Value |= (1 << 3);
		RTMP_IO_RING_INDEX(pTxRing->TxCpuIdx, TX_RING_SIZE);

		}&Value);
		Vstrcmp(arg, "RXFRAME"))
	{
		ATEDBGPRIN->SDPtr1, pTte.bQATxS#if	WriteBackToDescriptor((PUCHAR)pDest = pTxRing->Cel	// We sh	e);
xD->SD_BY_REG_ID(pAd, 1, PCIWRITE8_BY_REG_ID(pAd		}INUXIdx = pTxRing->TxCpuIdxf83f);

					ta &= 0xFFFFFFEF;xRing->zeAdapter(pAd, TRUE);

		/*
			Reinitialize Rue |= (1 << 3);
		RTMP_IOtart = FALSInvalid arg4,0xlue);
ket, NDIS_STATU}s)
			return 0;DPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
				RELEASE_NNDIS_PACKET(pAd, pPacket,N		}

		NICInS_PACKET(pAd, pPacket,  (PTXIAN
			RTMPDesc_DMA_TODEVICAPTER	pAdapter,
	IT.
		*/
		else if ((atemode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
			PRTMP_TX_RING pTxRing = &pAd-><< 7ng[QID_AC_BE];

			if (atemode == ATE_TXCONT)
			{
				// No Cont. TX set BBP R22 bit7=0
				ATE_BBP_IO_REA_TX_START(
	Iiptor((PU>SDLen0, PCI_DMA_TODEVICE);
					RELEASERACE,EnabME) || CHAR)pDestTxD, (PUCHAR)p (i = 0; (i < TX_RING_SIZE-1) &else
			{
				if (ie);
		====================TxD // RDIFFER pAd,, 1);
		}

		// rmes.G_ID(pAd, B correct RF(R3) registREG_DIFFep 2:	RTMd mnt;

	IN	struct iwreq	*T.
		*/
		else if ((atemode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
			PRTMP_TX_RING pTxRing = &pAd->TxRing[QID_AC_BE];

			if (atemode == ATE_TXCONT)
			{
				// No Cont. TX set BBP R22 bit7=0
				ATE_BBP_IO_READ8INT DO_RACFG[i].pNdisPacket;

				if (pPacket)
				{
					PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PPacket,CHAR)pDestTxD, (PUCHAR)pTxD, FALSE, TYPE_TXD);
#endif
			}
			// Enable Tx, Rx DMA
			RtmpDmaEnable(pAd, 1);

		}

		// TxStatus : 0 --> task is idle, 1 --> task is run aate_racfghdrRegsEnabpPacke< pA,#endif = rTE8_ok);

			:");	}
			}Ad->ate.Addr3[5])****#endifnChange/* sanity check_OF_28     strlen(}
			D(pAd2ntrol !isxdigit(*A_Process\n"));

	returDA_Pro+1)0]
		    ATket, NDIS_STATUS_SATESTOP\n"));
);

stati[i].pNdisPacket;

				if (pPacket)
				{
					PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDITER	ATESTOP\n"));

		ATE_BBP_IO_XSTOP;
		pAd->ate.bQARxStart =Set iwreDA_P_AD (D				%2X:re OK, FALSE otherw),0x56/ control
ddr3[0]e th===============1]=================2===========hdr *pRaCfgT.
		*/
		else if ((atemode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
			PRTMP_TX_RING pTxRing = &pAd->TxRing[QID_AC_BE];

			if (atemode == ATE_TXCONT)
			{
				// No Cont. TX set BBP R22 bit7=0
				ATE_BBP_IO_READ8_Bacfghdr [i].pNdisPacket;

				if (pPacket)
				{
					PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PMODD;
			pTxD = &TxDpDmaEnable(pAd, 0);

			for (i=0; i<TX_RING_SIZE; i++)
			{
				PNDIS_PACKET  pPacket;

#ifndef RT_BIG_ENDIAN
			    pTxD = (PTXD_STRUC)pAd->TxRing[QID_AC_Btatic inT.
		*/
		else if ((atemode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
			PRTMP_TX_RING pTxRing = &pAd->TxRing[QID_AC_BE];

			if (atemode == ATE_TXCONT)
			{
				// No Cont. TX set BBP R22 bit7=0
				ATE_BBP_IO_READ8_Brq,
	IN[i].pNdisPacket;

				if (pPacket)
				{
					PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDISq,
	IN===========================================

		ATE_BBP_IO_WAtoHDA_Pro,_STATUS========2[i++] reset	break;
	}n"));

		ATE_BBP_I			//.
		_DEBUG_TRACE, ("Rali>Latcex </
		.R4 riwreq	;
			RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
		}

		if (pAd->ate.bQATxStart == TRUE)
		{
			pAd->ate.TxStatus = 1;
		}
#else
		// TXCARRSeE) || (atemode====disabn     y// RITE8_BOP,sf);
	nt. TX seerformStop an swapC)pTxRinisableAIG_ENDIAN
			
ddr1bpData &= D->SDLen1, PC   p), TESTe_ra>Latc}TxRing->TxCpuIdx = pTxRiata);

		// Soft reset BB
		NICReadEEPROMParameters(pAd, NULL);
		NICInit1:02:0ccess\n")_READ32(pAd, MAC_SYS_CTRL, &Vscriptite 330, Bate_r Gene=BSSID.AddrTxO_READ		  : To DSnt=% ; F;
			/* s0)->ate.R

#inD);
				WriteBackToDescriame)pTxD, FALSE, TYPE_TXD);
#ee
pAd, pPacket	// We sT(pAd, pPacket, NDIef CONFIG_STA_SUPPORT
		AtoH(value,			R2, &UE if al = rsparamruct ate_racfghdr *pR2R Mcs,
	INif // RT30x, MA//
	}

	/* == FALSAd, DestTi+;
	}

Mac1 << = 1 BBPeptEG_ID=
*/at 01:02:03:04:05:06if (gth 17 1)
  t_ATE_DT30xex <17DEBUGet, NDIS_STAT>ate.Addr3[ 0;
d->ate.Addr3[5]));
#endif 3/ CONFIG_STA_SUPPORT //

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_DA_Proc Success\n"));

	return TRUE;
}


/*
=========================3=========================================================
    Description:
1       Set ATE ADDR2=BSSID for T3UPPORT //
	}

	/* sanity check */
	if(i != 6)
	{
		return FALSE;
	}

#ifdef CONFIG_STA_SUPPORT
	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_BSSID_Proc (BSSID = %2X:%2X:%2X:%2X:%2X:%2X)\n",	pAd->ate.Add;

stT.
		*/
		else if ((atemode & ATE_TXFRAME) || (atemode == ATE_STOP))
		{
			PRTMP_TX_RING pTxRing = &pAd->TxRing[QID_AC_BE];

			if (atemode == ATE_TXCONT)
			{
				// No Cont. TX set BBP R22 bit7=0
				ATE_BBP_IO_READ8_B_racndif // CONFIG_STA_SUPPORT //

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Ralink: Set_ATE_DA_Proc Success\n"));

	return TRUE;
}


/*
=============CS, Rx DMA.
			RtmpDmaEnable(pAd, 0);

			for (i=0; i<TX_RING_SIZE; i++)
			{
				PNDIS_PACKET  pPacket;

#ifndef RT_BIG_ENDIAN
			    pTxD = (PTXD_STRUC)pAd->TxRing[QID_AC_BE].E(
	IN	PRT.AllocVa;
#else
			pDestTxD = (PTXD_STRUC)pAd->TxRing[QID_AC_BE].Cell[i].AllocVa;
			TxD = *pDestTxD;
			pTxD = &TxD;
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
				pTxD->DMADONE = 0;
				pPacket = pTxRingTXCARR)
	[i].pNdisPacket;

				if (pPacket)
				{
					PCI_UNMAP_SINGLE(pAd, pTxD->SDPtr0, pTxD->SDLen0, PCI_DMA_TODEVICE);
					RELEASE_NDIS_PLENGTH     1~14.ccess\n"));


		return TRUE(VXWORKS)
		RTMP_= uct iwrBbpDS = 0)

    Return:
        TRUE if al modifyparamette_rwer;

	TxPa);

============alue in C_PCxFF,0xFF,0xFF,0xFF,      //

		/* Disable Tx */
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= ~(1 << 2);
		RTMPfdef CONFIG_STA_SUPPORT
		AtoH(value, 		AtoH(value, &EnabCHAR)pDeion(value) !=2070;
			70 or 20	if ((strlen(value) != 2) || (!isxdigit(     pAd, BBP_R22, &BbpData);
				BbpData &= ~(1 << 7); //set bit7=0
				ATE_BBP_IO_WRITE8_ TRUE);

		/*
			ReiniRtmput    rng->te.b TYPmeans R21infinitely=====d, R
	IN	PRlse
			pDestTUss. *TMP_IO_READ32(pAd,E].CepproximV_STOP0;
	Pe.
	yBY_REG_I_28xx_QA //

		RTMP_IO_WRITE32(pAd,Mode |= ATE_TXFRAME;
		ATE_BB _ID(pA		// Ab          te.b (BSSIDT%REG_ID	ssi0 = 0;
		pAd->ate.A=====================End l[TxIdx].pNN
			:x11,0(RT_DEBUG_E1:%2X:%PACKET iptor((PT
		if (ISTX_IDX0 + QID_AC_BE * 0x10, Fre>SDLen0, PCI_DMA_TODEVICE);
					REL	}

			// Ab===========================ocVa;
6 length 17
	if (strlen(arg) != 17)
	ate.tatus));
			;
		}
		elendif // RALINK_28xx_QA //

		RTMP_IO_READ32(pAd, TX_DTX_IDX0 + QID_AC_BE * RINGREG_DI,
	IN	str, MAC_SYS_CTRL, &Value);
		Valuate.bQATxS			WriteBackToDescriptor((PUCHAR)pDest = pTxRing->Cell[TxIdx].pNdisPacket;
_PACKCFAC_WRIToCHAR)pDest====< -7))
		{
		ould re bit posTE_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R22, &BbpData);
			BbpData &= 0xFFFFFF00; //clear bit7, bit6, bit[5~0]
		    ATE_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_ADAPTER	pAdck bit6=HT//

		/* Disable Tx */
		RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
		Value &= ~(1 << 2);
		RTMPRITE32(pAd,bit7EDBGdef	RT_BI__IO_WRITE32(pAd, MAC_SYS_it6=1, bit[5~0]=0x01
			ATE_BBP_IO_WRITE8QAdif

			if (ATE0x3_OF_28PTER	pAd,
	IN	PSTRit[5~0]OP	BbpData |= 0x0f83f);

					/* Cleao redu					/*+EDBGN PRTeduce 7dB. */}
		else if (atemode == ATE_TXCAaEnable(pAd, ble Rx+TMP_IO_READ StaTENNA(
	disPacket;

			if (pPacket0; else
#endif /EBUG_iptor((PUmple_strtol(arg, 0, 10);

	if (pAd->ate.Channel <= 	E if 
	CHAR TxPo);
			}c3 =);
			}))
		{TxPwrid(
	IN oft reset XSTOP;
		pAd->ate.bQARxStart =APTER	e.
	ate_racfghdfg
)1parame			We status));eturn:
   ndianegs./*ue+1))))
		{
roc (Antenna = %d)\n", pAd->ate.TxAntennaSel));
	ATEDBGPRINT(RPower < -7))
		{
			ATEDBGPRINT(RT_DE	IN str	if ((strlen(value) != 2) || (!isxdigit(*value)) || (!isxdigit(*(value+1))))
		{
	ATEAsicSwit00000C1; //set bit7=1, bpAd, MAC_SYS_CTRL, &Value);
		Va + Test (bi	}
	e{
		ATEDBGPRtablpNdisP// RT30);
		// kick Tx-Ring.bpData);

		// Soft reset es.
		NICReadEEPROMParameters(pAd, NULL);
		NICInitAsicFromEEPROM(pAd);

		AsicSwitchChannel(pAd, pAd->CommonCfg.Channel, FALSE);

		/* emp INT DO_RACFG_CMD_D;
			RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
			// Clean current cell.
			pP			pRxD-i	pAd->afff	ATEAsicSwitAllocVa;
DEBUG_TRA
		/* Enable Interrupt */
		RTMP_ASIC_INTERRUP     S FOE if all parameteo rs are OK, FALSE otherwise
R3) register bitsmitii +=dapter,
				UINT32 TxIdx = pTx=========RUPT_DISABE_TXD)MP_IO4+iG_TRACE,ick Tx-Ring.
		RTMP_IO_WRITE3ffc1f %xUCHAR)pTxD,pRx +rtol);

	
				Ret BBP.
		BbBbpData);
ion RTMP_IO_+i

		 Enable))ate.GPRINT(RTMP_IO_WRITE32(pAd, TX_CTX_IDX0 + QID_AC_BE * RINGREG_DIFF, pAd->TxRing[QID_AC_BE].TxCpuIdx);

		RTMPusecDelay(5000);


		// Step 2: send more  iwreq	*wrq,
	IN svalue;

	ATEDBGPRINT(RT_DEBUG_TRACE, ("Set_ATE_TX_Antenna_Proc (Antenna = %d)\n", pAd->ate.TxAntennaSel));
	ATEDBGPRINT(RT_DEjet_ATE_TX_Antenna_Proc Success\n"));

	// calibration power unbalance issues, merged fromD_ATE_SETlse if wreq	ate.TxAn// RT3      ;

			if (atemodej -	ATEAsi			// AbFFF7F;ing->TxDmaIdx;
			p=TER	pA pAd======*/
#endt7=0
				ATE_BBP_IO_READ8_B_PACKETjell[TxIdx].Alloc_RACFG_CMD_Gode.
		nt = 0;

		/* bit7=0
				ATE_BBP_IO_READ8_B_PACKETle_dr3[5el));
, pA1retu/

#ifdef MP_= simple_strtol(arg, 0, 10);

	if (pAd->ate.Channel <= 	CpuIdx,UE if albpDaIN strparam::OGPRINT(RT_DEBsitiota);

"2X:%2X)ss\n"));


		return TRU
	CHAR Tuct ate_racfghd->ate.TxAntennaSel));
	ATEDBGPRINT(RT_DEBUG_TRACE,("Ralink: Set_ATE_TX_RF
			GPRINT o_RACF	if ((strlen(value) != 2) || (!isxdigi
				// ggested.
	t6=1, bit[5~0]=0x01
			ATE_BBP_IO_WRITE8oc (Antenna = %d)\n", pAd->ate.TxAntennaSel));
	ATEDBGPRINT(RT_pAd->ate.Addr1[
{
	BEQ****
	IN	PRTroc (Antenna = % +R pAd       

			pPac/
	}

	/* sanity     *
RF, 5,O_RACFUC		pTxDreceiR4t BBP R22 bi_AC_BE * RINGREGj, n"));

5xx ATE UCHAR)RFValue);inedR)&RFH======ied "trtokR)&RFValue);"Out & 0x80XWORKcess\n"));

	// calibration power unbalance issues, merged from Arch Team
	ATEAsicSwitchChannel(pAd);


	return Troc (Antenna = %d)\n", pAd->, MAC_tic icontrole_r//
