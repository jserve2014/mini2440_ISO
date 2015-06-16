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

	Module Name:
	mlme.c

	Abstract:

	Revision History:
	Who			When			What
	--------	----------		----------------------------------------------
	John Chang	2004-08-25		Modify from RT2500 code base
	John Chang	2004-09-06		modified for RT2600
*/

#include "../rt_config.h"
#include <stdarg.h>

UCHAR	CISCO_OUI[] = {0x00, 0x40, 0x96};

UCHAR	WPA_OUI[] = {0x00, 0x50, 0xf2, 0x01};
UCHAR	RSN_OUI[] = {0x00, 0x0f, 0xac};
UCHAR   WME_INFO_ELEM[]  = {0x00, 0x50, 0xf2, 0x02, 0x00, 0x01};
UCHAR   WME_PARM_ELEM[] = {0x00, 0x50, 0xf2, 0x02, 0x01, 0x01};
UCHAR	Ccx2QosInfo[] = {0x00, 0x40, 0x96, 0x04};
UCHAR   RALINK_OUI[]  = {0x00, 0x0c, 0x43};
UCHAR   BROADCOM_OUI[]  = {0x00, 0x90, 0x4c};
UCHAR   WPS_OUI[] = {0x00, 0x50, 0xf2, 0x04};
UCHAR	PRE_N_HT_OUI[]	= {0x00, 0x90, 0x4c};

UCHAR RateSwitchTable[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x11, 0x00,  0,  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101,
    0x01, 0x00,  1, 40, 50,
    0x02, 0x00,  2, 35, 45,
    0x03, 0x00,  3, 20, 45,
    0x04, 0x21,  0, 30, 50,
    0x05, 0x21,  1, 20, 50,
    0x06, 0x21,  2, 20, 50,
    0x07, 0x21,  3, 15, 50,
    0x08, 0x21,  4, 15, 30,
    0x09, 0x21,  5, 10, 25,
    0x0a, 0x21,  6,  8, 25,
    0x0b, 0x21,  7,  8, 25,
    0x0c, 0x20, 12,  15, 30,
    0x0d, 0x20, 13,  8, 20,
    0x0e, 0x20, 14,  8, 20,
    0x0f, 0x20, 15,  8, 25,
    0x10, 0x22, 15,  8, 25,
    0x11, 0x00,  0,  0,  0,
    0x12, 0x00,  0,  0,  0,
    0x13, 0x00,  0,  0,  0,
    0x14, 0x00,  0,  0,  0,
    0x15, 0x00,  0,  0,  0,
    0x16, 0x00,  0,  0,  0,
    0x17, 0x00,  0,  0,  0,
    0x18, 0x00,  0,  0,  0,
    0x19, 0x00,  0,  0,  0,
    0x1a, 0x00,  0,  0,  0,
    0x1b, 0x00,  0,  0,  0,
    0x1c, 0x00,  0,  0,  0,
    0x1d, 0x00,  0,  0,  0,
    0x1e, 0x00,  0,  0,  0,
    0x1f, 0x00,  0,  0,  0,
};

UCHAR RateSwitchTable11B[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x04, 0x03,  0,  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101,
    0x01, 0x00,  1, 40, 50,
    0x02, 0x00,  2, 35, 45,
    0x03, 0x00,  3, 20, 45,
};

UCHAR RateSwitchTable11BG[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0a, 0x00,  0,  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101,
    0x01, 0x00,  1, 40, 50,
    0x02, 0x00,  2, 35, 45,
    0x03, 0x00,  3, 20, 45,
    0x04, 0x10,  2, 20, 35,
    0x05, 0x10,  3, 16, 35,
    0x06, 0x10,  4, 10, 25,
    0x07, 0x10,  5, 16, 25,
    0x08, 0x10,  6, 10, 25,
    0x09, 0x10,  7, 10, 13,
};

UCHAR RateSwitchTable11G[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x08, 0x00,  0,  0,  0,						// Initial used item after association
    0x00, 0x10,  0, 20, 101,
    0x01, 0x10,  1, 20, 35,
    0x02, 0x10,  2, 20, 35,
    0x03, 0x10,  3, 16, 35,
    0x04, 0x10,  4, 10, 25,
    0x05, 0x10,  5, 16, 25,
    0x06, 0x10,  6, 10, 25,
    0x07, 0x10,  7, 10, 13,
};

UCHAR RateSwitchTable11N1S[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x09, 0x00,  0,  0,  0,						// Initial used item after association
    0x00, 0x21,  0, 30, 101,
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x21,  4, 15, 30,
    0x05, 0x21,  5, 10, 25,
    0x06, 0x21,  6,  8, 14,
    0x07, 0x21,  7,  8, 14,
    0x08, 0x23,  7,  8, 14,
};

UCHAR RateSwitchTable11N2S[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0a, 0x00,  0,  0,  0,      // Initial used item after association
    0x00, 0x21,  0, 30, 101,
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x21,  4, 15, 30,
    0x05, 0x20, 12,  15, 30,
    0x06, 0x20, 13,  8, 20,
    0x07, 0x20, 14,  8, 20,
    0x08, 0x20, 15,  8, 25,
    0x09, 0x22, 15,  8, 25,
};

UCHAR RateSwitchTable11N3S[] = {
// Item No.	Mode	Curr-MCS	TrainUp	TrainDown	// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0a, 0x00,  0,  0,  0,      // Initial used item after association
    0x00, 0x21,  0, 30, 101,
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x21,  4, 15, 30,
    0x05, 0x20, 12,  15, 30,
    0x06, 0x20, 13,  8, 20,
    0x07, 0x20, 14,  8, 20,
    0x08, 0x20, 15,  8, 25,
    0x09, 0x22, 15,  8, 25,
};

UCHAR RateSwitchTable11N2SForABand[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0b, 0x09,  0,  0,  0,						// Initial used item after association
    0x00, 0x21,  0, 30, 101,
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x21,  4, 15, 30,
    0x05, 0x21,  5, 15, 30,
    0x06, 0x20, 12,  15, 30,
    0x07, 0x20, 13,  8, 20,
    0x08, 0x20, 14,  8, 20,
    0x09, 0x20, 15,  8, 25,
    0x0a, 0x22, 15,  8, 25,
};

UCHAR RateSwitchTable11N3SForABand[] = { // 3*3
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0b, 0x09,  0,  0,  0,						// Initial used item after association
    0x00, 0x21,  0, 30, 101,
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x21,  4, 15, 30,
    0x05, 0x21,  5, 15, 30,
    0x06, 0x20, 12,  15, 30,
    0x07, 0x20, 13,  8, 20,
    0x08, 0x20, 14,  8, 20,
    0x09, 0x20, 15,  8, 25,
    0x0a, 0x22, 15,  8, 25,
};

UCHAR RateSwitchTable11BGN1S[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0d, 0x00,  0,  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101,
    0x01, 0x00,  1, 40, 50,
    0x02, 0x00,  2, 35, 45,
    0x03, 0x00,  3, 20, 45,
    0x04, 0x21,  0, 30,101,	//50
    0x05, 0x21,  1, 20, 50,
    0x06, 0x21,  2, 20, 50,
    0x07, 0x21,  3, 15, 50,
    0x08, 0x21,  4, 15, 30,
    0x09, 0x21,  5, 10, 25,
    0x0a, 0x21,  6,  8, 14,
    0x0b, 0x21,  7,  8, 14,
	0x0c, 0x23,  7,  8, 14,
};

UCHAR RateSwitchTable11BGN2S[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0a, 0x00,  0,  0,  0,						// Initial used item after association
    0x00, 0x21,  0, 30,101,	//50
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x21,  4, 15, 30,
    0x05, 0x20, 12, 15, 30,
    0x06, 0x20, 13,  8, 20,
    0x07, 0x20, 14,  8, 20,
    0x08, 0x20, 15,  8, 25,
    0x09, 0x22, 15,  8, 25,
};

UCHAR RateSwitchTable11BGN3S[] = { // 3*3
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0a, 0x00,  0,  0,  0,						// Initial used item after association
    0x00, 0x21,  0, 30,101,	//50
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 20, 50,
    0x04, 0x21,  4, 15, 50,
    0x05, 0x20, 20, 15, 30,
    0x06, 0x20, 21,  8, 20,
    0x07, 0x20, 22,  8, 20,
    0x08, 0x20, 23,  8, 25,
    0x09, 0x22, 23,  8, 25,
};

UCHAR RateSwitchTable11BGN2SForABand[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0b, 0x09,  0,  0,  0,						// Initial used item after association
    0x00, 0x21,  0, 30,101,	//50
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x21,  4, 15, 30,
    0x05, 0x21,  5, 15, 30,
    0x06, 0x20, 12, 15, 30,
    0x07, 0x20, 13,  8, 20,
    0x08, 0x20, 14,  8, 20,
    0x09, 0x20, 15,  8, 25,
    0x0a, 0x22, 15,  8, 25,
};

UCHAR RateSwitchTable11BGN3SForABand[] = { // 3*3
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0c, 0x09,  0,  0,  0,						// Initial used item after association
    0x00, 0x21,  0, 30,101,	//50
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x21,  4, 15, 30,
    0x05, 0x21,  5, 15, 30,
    0x06, 0x21, 12, 15, 30,
    0x07, 0x20, 20, 15, 30,
    0x08, 0x20, 21,  8, 20,
    0x09, 0x20, 22,  8, 20,
    0x0a, 0x20, 23,  8, 25,
    0x0b, 0x22, 23,  8, 25,
};

PUCHAR ReasonString[] = {
	/* 0  */	 "Reserved",
	/* 1  */	 "Unspecified Reason",
	/* 2  */	 "Previous Auth no longer valid",
	/* 3  */	 "STA is leaving / has left",
	/* 4  */	 "DIS-ASSOC due to inactivity",
	/* 5  */	 "AP unable to hanle all associations",
	/* 6  */	 "class 2 error",
	/* 7  */	 "class 3 error",
	/* 8  */	 "STA is leaving / has left",
	/* 9  */	 "require auth before assoc/re-assoc",
	/* 10 */	 "Reserved",
	/* 11 */	 "Reserved",
	/* 12 */	 "Reserved",
	/* 13 */	 "invalid IE",
	/* 14 */	 "MIC error",
	/* 15 */	 "4-way handshake timeout",
	/* 16 */	 "2-way (group key) handshake timeout",
	/* 17 */	 "4-way handshake IE diff among AssosReq/Rsp/Beacon",
	/* 18 */
};

extern UCHAR	 OfdmRateToRxwiMCS[];
// since RT61 has better RX sensibility, we have to limit TX ACK rate not to exceed our normal data TX rate.
// otherwise the WLAN peer may not be able to receive the ACK thus downgrade its data TX rate
ULONG BasicRateMask[12]				= {0xfffff001 /* 1-Mbps */, 0xfffff003 /* 2 Mbps */, 0xfffff007 /* 5.5 */, 0xfffff00f /* 11 */,
									  0xfffff01f /* 6 */	 , 0xfffff03f /* 9 */	  , 0xfffff07f /* 12 */ , 0xfffff0ff /* 18 */,
									  0xfffff1ff /* 24 */	 , 0xfffff3ff /* 36 */	  , 0xfffff7ff /* 48 */ , 0xffffffff /* 54 */};

UCHAR MULTICAST_ADDR[MAC_ADDR_LEN] = {0x1,  0x00, 0x00, 0x00, 0x00, 0x00};
UCHAR BROADCAST_ADDR[MAC_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
UCHAR ZERO_MAC_ADDR[MAC_ADDR_LEN]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// e.g. RssiSafeLevelForTxRate[RATE_36]" means if the current RSSI is greater than
//		this value, then it's quaranteed capable of operating in 36 mbps TX rate in
//		clean environment.
//								  TxRate: 1   2   5.5	11	 6	  9    12	18	 24   36   48	54	 72  100
CHAR RssiSafeLevelForTxRate[] ={  -92, -91, -90, -87, -88, -86, -85, -83, -81, -78, -72, -71, -40, -40 };

UCHAR  RateIdToMbps[]	 = { 1, 2, 5, 11, 6, 9, 12, 18, 24, 36, 48, 54, 72, 100};
USHORT RateIdTo500Kbps[] = { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108, 144, 200};

UCHAR  SsidIe	 = IE_SSID;
UCHAR  SupRateIe = IE_SUPP_RATES;
UCHAR  ExtRateIe = IE_EXT_SUPP_RATES;
UCHAR  HtCapIe = IE_HT_CAP;
UCHAR  AddHtInfoIe = IE_ADD_HT;
UCHAR  NewExtChanIe = IE_SECONDARY_CH_OFFSET;
UCHAR  ErpIe	 = IE_ERP;
UCHAR  DsIe 	 = IE_DS_PARM;
UCHAR  TimIe	 = IE_TIM;
UCHAR  WpaIe	 = IE_WPA;
UCHAR  Wpa2Ie	 = IE_WPA2;
UCHAR  IbssIe	 = IE_IBSS_PARM;
UCHAR  Ccx2Ie	 = IE_CCX_V2;

extern UCHAR	WPA_OUI[];

UCHAR	SES_OUI[] = {0x00, 0x90, 0x4c};

UCHAR	ZeroSsid[32] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

// Reset the RFIC setting to new series
RTMP_RF_REGS RF2850RegTable[] = {
//		ch	 R1 		 R2 		 R3(TX0~4=0) R4
		{1,  0x98402ecc, 0x984c0786, 0x9816b455, 0x9800510b},
		{2,  0x98402ecc, 0x984c0786, 0x98168a55, 0x9800519f},
		{3,  0x98402ecc, 0x984c078a, 0x98168a55, 0x9800518b},
		{4,  0x98402ecc, 0x984c078a, 0x98168a55, 0x9800519f},
		{5,  0x98402ecc, 0x984c078e, 0x98168a55, 0x9800518b},
		{6,  0x98402ecc, 0x984c078e, 0x98168a55, 0x9800519f},
		{7,  0x98402ecc, 0x984c0792, 0x98168a55, 0x9800518b},
		{8,  0x98402ecc, 0x984c0792, 0x98168a55, 0x9800519f},
		{9,  0x98402ecc, 0x984c0796, 0x98168a55, 0x9800518b},
		{10, 0x98402ecc, 0x984c0796, 0x98168a55, 0x9800519f},
		{11, 0x98402ecc, 0x984c079a, 0x98168a55, 0x9800518b},
		{12, 0x98402ecc, 0x984c079a, 0x98168a55, 0x9800519f},
		{13, 0x98402ecc, 0x984c079e, 0x98168a55, 0x9800518b},
		{14, 0x98402ecc, 0x984c07a2, 0x98168a55, 0x98005193},

		// 802.11 UNI / HyperLan 2
		{36, 0x98402ecc, 0x984c099a, 0x98158a55, 0x980ed1a3},
		{38, 0x98402ecc, 0x984c099e, 0x98158a55, 0x980ed193},
		{40, 0x98402ec8, 0x984c0682, 0x98158a55, 0x980ed183},
		{44, 0x98402ec8, 0x984c0682, 0x98158a55, 0x980ed1a3},
		{46, 0x98402ec8, 0x984c0686, 0x98158a55, 0x980ed18b},
		{48, 0x98402ec8, 0x984c0686, 0x98158a55, 0x980ed19b},
		{52, 0x98402ec8, 0x984c068a, 0x98158a55, 0x980ed193},
		{54, 0x98402ec8, 0x984c068a, 0x98158a55, 0x980ed1a3},
		{56, 0x98402ec8, 0x984c068e, 0x98158a55, 0x980ed18b},
		{60, 0x98402ec8, 0x984c0692, 0x98158a55, 0x980ed183},
		{62, 0x98402ec8, 0x984c0692, 0x98158a55, 0x980ed193},
		{64, 0x98402ec8, 0x984c0692, 0x98158a55, 0x980ed1a3}, // Plugfest#4, Day4, change RFR3 left4th 9->5.

		// 802.11 HyperLan 2
		{100, 0x98402ec8, 0x984c06b2, 0x98178a55, 0x980ed783},

		// 2008.04.30 modified
		// The system team has AN to improve the EVM value
		// for channel 102 to 108 for the RT2850/RT2750 dual band solution.
		{102, 0x98402ec8, 0x985c06b2, 0x98578a55, 0x980ed793},
		{104, 0x98402ec8, 0x985c06b2, 0x98578a55, 0x980ed1a3},
		{108, 0x98402ecc, 0x985c0a32, 0x98578a55, 0x980ed193},

		{110, 0x98402ecc, 0x984c0a36, 0x98178a55, 0x980ed183},
		{112, 0x98402ecc, 0x984c0a36, 0x98178a55, 0x980ed19b},
		{116, 0x98402ecc, 0x984c0a3a, 0x98178a55, 0x980ed1a3},
		{118, 0x98402ecc, 0x984c0a3e, 0x98178a55, 0x980ed193},
		{120, 0x98402ec4, 0x984c0382, 0x98178a55, 0x980ed183},
		{124, 0x98402ec4, 0x984c0382, 0x98178a55, 0x980ed193},
		{126, 0x98402ec4, 0x984c0382, 0x98178a55, 0x980ed15b}, // 0x980ed1bb->0x980ed15b required by Rory 20070927
		{128, 0x98402ec4, 0x984c0382, 0x98178a55, 0x980ed1a3},
		{132, 0x98402ec4, 0x984c0386, 0x98178a55, 0x980ed18b},
		{134, 0x98402ec4, 0x984c0386, 0x98178a55, 0x980ed193},
		{136, 0x98402ec4, 0x984c0386, 0x98178a55, 0x980ed19b},
		{140, 0x98402ec4, 0x984c038a, 0x98178a55, 0x980ed183},

		// 802.11 UNII
		{149, 0x98402ec4, 0x984c038a, 0x98178a55, 0x980ed1a7},
		{151, 0x98402ec4, 0x984c038e, 0x98178a55, 0x980ed187},
		{153, 0x98402ec4, 0x984c038e, 0x98178a55, 0x980ed18f},
		{157, 0x98402ec4, 0x984c038e, 0x98178a55, 0x980ed19f},
		{159, 0x98402ec4, 0x984c038e, 0x98178a55, 0x980ed1a7},
		{161, 0x98402ec4, 0x984c0392, 0x98178a55, 0x980ed187},
		{165, 0x98402ec4, 0x984c0392, 0x98178a55, 0x980ed197},

		// Japan
		{184, 0x95002ccc, 0x9500491e, 0x9509be55, 0x950c0a0b},
		{188, 0x95002ccc, 0x95004922, 0x9509be55, 0x950c0a13},
		{192, 0x95002ccc, 0x95004926, 0x9509be55, 0x950c0a1b},
		{196, 0x95002ccc, 0x9500492a, 0x9509be55, 0x950c0a23},
		{208, 0x95002ccc, 0x9500493a, 0x9509be55, 0x950c0a13},
		{212, 0x95002ccc, 0x9500493e, 0x9509be55, 0x950c0a1b},
		{216, 0x95002ccc, 0x95004982, 0x9509be55, 0x950c0a23},

		// still lack of MMAC(Japan) ch 34,38,42,46
};
UCHAR	NUM_OF_2850_CHNL = (sizeof(RF2850RegTable) / sizeof(RTMP_RF_REGS));

FREQUENCY_ITEM FreqItems3020[] =
{
	/**************************************************/
	// ISM : 2.4 to 2.483 GHz                         //
	/**************************************************/
	// 11g
	/**************************************************/
	//-CH---N-------R---K-----------
	{1,    241,  2,  2},
	{2,    241,	 2,  7},
	{3,    242,	 2,  2},
	{4,    242,	 2,  7},
	{5,    243,	 2,  2},
	{6,    243,	 2,  7},
	{7,    244,	 2,  2},
	{8,    244,	 2,  7},
	{9,    245,	 2,  2},
	{10,   245,	 2,  7},
	{11,   246,	 2,  2},
	{12,   246,	 2,  7},
	{13,   247,	 2,  2},
	{14,   248,	 2,  4},
};
UCHAR	NUM_OF_3020_CHNL=(sizeof(FreqItems3020) / sizeof(FREQUENCY_ITEM));

/*
	==========================================================================
	Description:
		initialize the MLME task and its data structure (queue, spinlock,
		timer, state machines).

	IRQL = PASSIVE_LEVEL

	Return:
		always return NDIS_STATUS_SUCCESS

	==========================================================================
*/
NDIS_STATUS MlmeInit(
	IN PRTMP_ADAPTER pAd)
{
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

	DBGPRINT(RT_DEBUG_TRACE, ("--> MLME Initialize\n"));

	do
	{
		Status = MlmeQueueInit(&pAd->Mlme.Queue);
		if(Status != NDIS_STATUS_SUCCESS)
			break;

		pAd->Mlme.bRunning = FALSE;
		NdisAllocateSpinLock(&pAd->Mlme.TaskLock);

		{
			BssTableInit(&pAd->ScanTab);

			// init STA state machines
			AssocStateMachineInit(pAd, &pAd->Mlme.AssocMachine, pAd->Mlme.AssocFunc);
			AuthStateMachineInit(pAd, &pAd->Mlme.AuthMachine, pAd->Mlme.AuthFunc);
			AuthRspStateMachineInit(pAd, &pAd->Mlme.AuthRspMachine, pAd->Mlme.AuthRspFunc);
			SyncStateMachineInit(pAd, &pAd->Mlme.SyncMachine, pAd->Mlme.SyncFunc);
			WpaPskStateMachineInit(pAd, &pAd->Mlme.WpaPskMachine, pAd->Mlme.WpaPskFunc);
			AironetStateMachineInit(pAd, &pAd->Mlme.AironetMachine, pAd->Mlme.AironetFunc);

			// Since we are using switch/case to implement it, the init is different from the above
			// state machine init
			MlmeCntlInit(pAd, &pAd->Mlme.CntlMachine, NULL);
		}

		ActionStateMachineInit(pAd, &pAd->Mlme.ActMachine, pAd->Mlme.ActFunc);

		// Init mlme periodic timer
		RTMPInitTimer(pAd, &pAd->Mlme.PeriodicTimer, GET_TIMER_FUNCTION(MlmePeriodicExec), pAd, TRUE);

		// Set mlme periodic timer
		RTMPSetTimer(&pAd->Mlme.PeriodicTimer, MLME_TASK_EXEC_INTV);

		// software-based RX Antenna diversity
		RTMPInitTimer(pAd, &pAd->Mlme.RxAntEvalTimer, GET_TIMER_FUNCTION(AsicRxAntEvalTimeout), pAd, FALSE);

#ifdef RT2860
		{
	        if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_ADVANCE_POWER_SAVE_PCIE_DEVICE))
	        {
	            // only PCIe cards need these two timers
	    		RTMPInitTimer(pAd, &pAd->Mlme.PsPollTimer, GET_TIMER_FUNCTION(PsPollWakeExec), pAd, FALSE);
	    		RTMPInitTimer(pAd, &pAd->Mlme.RadioOnOffTimer, GET_TIMER_FUNCTION(RadioOnExec), pAd, FALSE);
	        }
		}
#endif
	} while (FALSE);

	DBGPRINT(RT_DEBUG_TRACE, ("<-- MLME Initialize\n"));

	return Status;
}

/*
	==========================================================================
	Description:
		main loop of the MLME
	Pre:
		Mlme has to be initialized, and there are something inside the queue
	Note:
		This function is invoked from MPSetInformation and MPReceive;
		This task guarantee only one MlmeHandler will run.

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID MlmeHandler(
	IN PRTMP_ADAPTER pAd)
{
	MLME_QUEUE_ELEM 	   *Elem = NULL;

	// Only accept MLME and Frame from peer side, no other (control/data) frame should
	// get into this state machine

	NdisAcquireSpinLock(&pAd->Mlme.TaskLock);
	if(pAd->Mlme.bRunning)
	{
		NdisReleaseSpinLock(&pAd->Mlme.TaskLock);
		return;
	}
	else
	{
		pAd->Mlme.bRunning = TRUE;
	}
	NdisReleaseSpinLock(&pAd->Mlme.TaskLock);

	while (!MlmeQueueEmpty(&pAd->Mlme.Queue))
	{
		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_MLME_RESET_IN_PROGRESS) ||
			RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS) ||
			RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("Device Halted or Removed or MlmeRest, exit MlmeHandler! (queue num = %ld)\n", pAd->Mlme.Queue.Num));
			break;
		}

		//From message type, determine which state machine I should drive
		if (MlmeDequeue(&pAd->Mlme.Queue, &Elem))
		{
#ifdef RT2870
			if (Elem->MsgType == MT2_RESET_CONF)
			{
				DBGPRINT_RAW(RT_DEBUG_TRACE, ("!!! reset MLME state machine !!!\n"));
				MlmeRestartStateMachine(pAd);
				Elem->Occupied = FALSE;
				Elem->MsgLen = 0;
				continue;
			}
#endif // RT2870 //

			// if dequeue success
			switch (Elem->Machine)
			{
				// STA state machines
				case ASSOC_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.AssocMachine, Elem);
					break;
				case AUTH_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.AuthMachine, Elem);
					break;
				case AUTH_RSP_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.AuthRspMachine, Elem);
					break;
				case SYNC_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.SyncMachine, Elem);
					break;
				case MLME_CNTL_STATE_MACHINE:
					MlmeCntlMachinePerformAction(pAd, &pAd->Mlme.CntlMachine, Elem);
					break;
				case WPA_PSK_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.WpaPskMachine, Elem);
					break;
				case AIRONET_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.AironetMachine, Elem);
					break;
				case ACTION_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.ActMachine, Elem);
					break;




				default:
					DBGPRINT(RT_DEBUG_TRACE, ("ERROR: Illegal machine %ld in MlmeHandler()\n", Elem->Machine));
					break;
			} // end of switch

			// free MLME element
			Elem->Occupied = FALSE;
			Elem->MsgLen = 0;

		}
		else {
			DBGPRINT_ERR(("MlmeHandler: MlmeQueue empty\n"));
		}
	}

	NdisAcquireSpinLock(&pAd->Mlme.TaskLock);
	pAd->Mlme.bRunning = FALSE;
	NdisReleaseSpinLock(&pAd->Mlme.TaskLock);
}

/*
	==========================================================================
	Description:
		Destructor of MLME (Destroy queue, state machine, spin lock and timer)
	Parameters:
		Adapter - NIC Adapter pointer
	Post:
		The MLME task will no longer work properly

	IRQL = PASSIVE_LEVEL

	==========================================================================
 */
VOID MlmeHalt(
	IN PRTMP_ADAPTER pAd)
{
	BOOLEAN 	  Cancelled;
#ifdef RT3070
	UINT32		TxPinCfg = 0x00050F0F;
#endif // RT3070 //

	DBGPRINT(RT_DEBUG_TRACE, ("==> MlmeHalt\n"));

	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
	{
		// disable BEACON generation and other BEACON related hardware timers
		AsicDisableSync(pAd);
	}

	{
		// Cancel pending timers
		RTMPCancelTimer(&pAd->MlmeAux.AssocTimer,		&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.ReassocTimer,		&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.DisassocTimer,	&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.AuthTimer,		&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.BeaconTimer,		&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.ScanTimer,		&Cancelled);
#ifdef RT2860
	    if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_ADVANCE_POWER_SAVE_PCIE_DEVICE))
	    {
	   	    RTMPCancelTimer(&pAd->Mlme.PsPollTimer,		&Cancelled);
		    RTMPCancelTimer(&pAd->Mlme.RadioOnOffTimer,		&Cancelled);
		}
#endif
	}

	RTMPCancelTimer(&pAd->Mlme.PeriodicTimer,		&Cancelled);
	RTMPCancelTimer(&pAd->Mlme.RxAntEvalTimer,		&Cancelled);



	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
	{
		// Set LED
		RTMPSetLED(pAd, LED_HALT);
        RTMPSetSignalLED(pAd, -100);	// Force signal strength Led to be turned off, firmware is not done it.
#ifdef RT2870
        {
            LED_CFG_STRUC LedCfg;
            RTMP_IO_READ32(pAd, LED_CFG, &LedCfg.word);
            LedCfg.field.LedPolar = 0;
            LedCfg.field.RLedMode = 0;
            LedCfg.field.GLedMode = 0;
            LedCfg.field.YLedMode = 0;
            RTMP_IO_WRITE32(pAd, LED_CFG, LedCfg.word);
        }
#endif // RT2870 //
#ifdef RT3070
		//
		// Turn off LNA_PE
		//
		if (IS_RT3070(pAd) || IS_RT3071(pAd))
		{
			TxPinCfg &= 0xFFFFF0F0;
			RTUSBWriteMACRegister(pAd, TX_PIN_CFG, TxPinCfg);
		}
#endif // RT3070 //
	}

	RTMPusecDelay(5000);    //  5 msec to gurantee Ant Diversity timer canceled

	MlmeQueueDestroy(&pAd->Mlme.Queue);
	NdisFreeSpinLock(&pAd->Mlme.TaskLock);

	DBGPRINT(RT_DEBUG_TRACE, ("<== MlmeHalt\n"));
}

VOID MlmeResetRalinkCounters(
	IN  PRTMP_ADAPTER   pAd)
{
	pAd->RalinkCounters.LastOneSecRxOkDataCnt = pAd->RalinkCounters.OneSecRxOkDataCnt;
	// clear all OneSecxxx counters.
	pAd->RalinkCounters.OneSecBeaconSentCnt = 0;
	pAd->RalinkCounters.OneSecFalseCCACnt = 0;
	pAd->RalinkCounters.OneSecRxFcsErrCnt = 0;
	pAd->RalinkCounters.OneSecRxOkCnt = 0;
	pAd->RalinkCounters.OneSecTxFailCount = 0;
	pAd->RalinkCounters.OneSecTxNoRetryOkCount = 0;
	pAd->RalinkCounters.OneSecTxRetryOkCount = 0;
	pAd->RalinkCounters.OneSecRxOkDataCnt = 0;

	// TODO: for debug only. to be removed
	pAd->RalinkCounters.OneSecOsTxCount[QID_AC_BE] = 0;
	pAd->RalinkCounters.OneSecOsTxCount[QID_AC_BK] = 0;
	pAd->RalinkCounters.OneSecOsTxCount[QID_AC_VI] = 0;
	pAd->RalinkCounters.OneSecOsTxCount[QID_AC_VO] = 0;
	pAd->RalinkCounters.OneSecDmaDoneCount[QID_AC_BE] = 0;
	pAd->RalinkCounters.OneSecDmaDoneCount[QID_AC_BK] = 0;
	pAd->RalinkCounters.OneSecDmaDoneCount[QID_AC_VI] = 0;
	pAd->RalinkCounters.OneSecDmaDoneCount[QID_AC_VO] = 0;
	pAd->RalinkCounters.OneSecTxDoneCount = 0;
	pAd->RalinkCounters.OneSecRxCount = 0;
	pAd->RalinkCounters.OneSecTxAggregationCount = 0;
	pAd->RalinkCounters.OneSecRxAggregationCount = 0;

	return;
}

unsigned long rx_AMSDU;
unsigned long rx_Total;

/*
	==========================================================================
	Description:
		This routine is executed periodically to -
		1. Decide if it's a right time to turn on PwrMgmt bit of all
		   outgoiing frames
		2. Calculate ChannelQuality based on statistics of the last
		   period, so that TX rate won't toggling very frequently between a
		   successful TX and a failed TX.
		3. If the calculated ChannelQuality indicated current connection not
		   healthy, then a ROAMing attempt is tried here.

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
#define ADHOC_BEACON_LOST_TIME		(8*OS_HZ)  // 8 sec
VOID MlmePeriodicExec(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3)
{
	ULONG			TxTotalCnt;
	PRTMP_ADAPTER	pAd = (RTMP_ADAPTER *)FunctionContext;

#ifdef RT2860
	//Baron 2008/07/10
	//printk("Baron_Test:\t%s", RTMPGetRalinkEncryModeStr(pAd->StaCfg.WepStatus));
	//If the STA security setting is OPEN or WEP, pAd->StaCfg.WpaSupplicantUP = 0.
	//If the STA security setting is WPAPSK or WPA2PSK, pAd->StaCfg.WpaSupplicantUP = 1.
	if(pAd->StaCfg.WepStatus<2)
	{
		pAd->StaCfg.WpaSupplicantUP = 0;
	}
	else
	{
		pAd->StaCfg.WpaSupplicantUP = 1;
	}

	{
	    // If Hardware controlled Radio enabled, we have to check GPIO pin2 every 2 second.
		// Move code to here, because following code will return when radio is off
		if ((pAd->Mlme.PeriodicRound % (MLME_TASK_EXEC_MULTIPLE * 2) == 0) &&
			(pAd->StaCfg.bHardwareRadio == TRUE) &&
			(RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_START_UP)) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)))
		{
			UINT32				data = 0;

			// Read GPIO pin2 as Hardware controlled radio state
			RTMP_IO_FORCE_READ32(pAd, GPIO_CTRL_CFG, &data);
			if (data & 0x04)
			{
				pAd->StaCfg.bHwRadio = TRUE;
			}
			else
			{
				pAd->StaCfg.bHwRadio = FALSE;
			}
			if (pAd->StaCfg.bRadio != (pAd->StaCfg.bHwRadio && pAd->StaCfg.bSwRadio))
			{
				pAd->StaCfg.bRadio = (pAd->StaCfg.bHwRadio && pAd->StaCfg.bSwRadio);
				if (pAd->StaCfg.bRadio == TRUE)
				{
					MlmeRadioOn(pAd);
					// Update extra information
					pAd->ExtraInfo = EXTRA_INFO_CLEAR;
				}
				else
				{
					MlmeRadioOff(pAd);
					// Update extra information
					pAd->ExtraInfo = HW_RADIO_OFF;
				}
			}
		}
	}
#endif /* RT2860 */

	// Do nothing if the driver is starting halt state.
	// This might happen when timer already been fired before cancel timer with mlmehalt
	if ((RTMP_TEST_FLAG(pAd, (fRTMP_ADAPTER_HALT_IN_PROGRESS |
								fRTMP_ADAPTER_RADIO_OFF |
								fRTMP_ADAPTER_RADIO_MEASUREMENT |
								fRTMP_ADAPTER_RESET_IN_PROGRESS))))
		return;

#ifdef RT2860
	{
		if ((pAd->RalinkCounters.LastReceivedByteCount == pAd->RalinkCounters.ReceivedByteCount) && (pAd->StaCfg.bRadio == TRUE))
		{
			// If ReceiveByteCount doesn't change,  increase SameRxByteCount by 1.
			pAd->SameRxByteCount++;
		}
		else
			pAd->SameRxByteCount = 0;

		// If after BBP, still not work...need to check to reset PBF&MAC.
		if (pAd->SameRxByteCount == 702)
		{
			pAd->SameRxByteCount = 0;
			AsicResetPBF(pAd);
			AsicResetMAC(pAd);
		}

		// If SameRxByteCount keeps happens for 2 second in infra mode, or for 60 seconds in idle mode.
		if (((INFRA_ON(pAd)) && (pAd->SameRxByteCount > 20)) || ((IDLE_ON(pAd)) && (pAd->SameRxByteCount > 600)))
		{
			if ((pAd->StaCfg.bRadio == TRUE) && (pAd->SameRxByteCount < 700))
			{
				DBGPRINT(RT_DEBUG_TRACE, ("--->  SameRxByteCount = %lu !!!!!!!!!!!!!!! \n", pAd->SameRxByteCount));
				pAd->SameRxByteCount = 700;
				AsicResetBBP(pAd);
			}
		}

		// Update lastReceiveByteCount.
		pAd->RalinkCounters.LastReceivedByteCount = pAd->RalinkCounters.ReceivedByteCount;

		if ((pAd->CheckDmaBusyCount > 3) && (IDLE_ON(pAd)))
		{
			pAd->CheckDmaBusyCount = 0;
			AsicResetFromDMABusy(pAd);
		}
	}
#endif /* RT2860 */
	RT28XX_MLME_PRE_SANITY_CHECK(pAd);

	{
		// Do nothing if monitor mode is on
		if (MONITOR_ON(pAd))
			return;

		if (pAd->Mlme.PeriodicRound & 0x1)
		{
			// This is the fix for wifi 11n extension channel overlapping test case.  for 2860D
			if (((pAd->MACVersion & 0xffff) == 0x0101) &&
				(STA_TGN_WIFI_ON(pAd)) &&
				(pAd->CommonCfg.IOTestParm.bToggle == FALSE))

				{
					RTMP_IO_WRITE32(pAd, TXOP_CTRL_CFG, 0x24Bf);
					pAd->CommonCfg.IOTestParm.bToggle = TRUE;
				}
				else if ((STA_TGN_WIFI_ON(pAd)) &&
						((pAd->MACVersion & 0xffff) == 0x0101))
				{
					RTMP_IO_WRITE32(pAd, TXOP_CTRL_CFG, 0x243f);
					pAd->CommonCfg.IOTestParm.bToggle = FALSE;
				}
		}
	}

	pAd->bUpdateBcnCntDone = FALSE;

//	RECBATimerTimeout(SystemSpecific1,FunctionContext,SystemSpecific2,SystemSpecific3);
	pAd->Mlme.PeriodicRound ++;

#ifdef RT3070
	// execute every 100ms, update the Tx FIFO Cnt for update Tx Rate.
	NICUpdateFifoStaCounters(pAd);
#endif // RT3070 //
	// execute every 500ms
	if ((pAd->Mlme.PeriodicRound % 5 == 0) && RTMPAutoRateSwitchCheck(pAd)/*(OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_TX_RATE_SWITCH_ENABLED))*/)
	{
		// perform dynamic tx rate switching based on past TX history
		{
			if ((OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED)
					)
				&& (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE)))
				MlmeDynamicTxRateSwitching(pAd);
		}
	}

	// Normal 1 second Mlme PeriodicExec.
	if (pAd->Mlme.PeriodicRound %MLME_TASK_EXEC_MULTIPLE == 0)
	{
                pAd->Mlme.OneSecPeriodicRound ++;

		if (rx_Total)
		{

			// reset counters
			rx_AMSDU = 0;
			rx_Total = 0;
		}

		// Media status changed, report to NDIS
		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_MEDIA_STATE_CHANGE))
		{
			RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_MEDIA_STATE_CHANGE);
			if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
			{
				pAd->IndicateMediaState = NdisMediaStateConnected;
				RTMP_IndicateMediaState(pAd);

			}
			else
			{
				pAd->IndicateMediaState = NdisMediaStateDisconnected;
				RTMP_IndicateMediaState(pAd);
			}
		}

		NdisGetSystemUpTime(&pAd->Mlme.Now32);

		// add the most up-to-date h/w raw counters into software variable, so that
		// the dynamic tuning mechanism below are based on most up-to-date information
		NICUpdateRawCounters(pAd);

#ifdef RT2870
		RT2870_WatchDog(pAd);
#endif // RT2870 //

   		// Need statistics after read counter. So put after NICUpdateRawCounters
		ORIBATimerTimeout(pAd);

		// The time period for checking antenna is according to traffic
		if (pAd->Mlme.bEnableAutoAntennaCheck)
		{
			TxTotalCnt = pAd->RalinkCounters.OneSecTxNoRetryOkCount +
							 pAd->RalinkCounters.OneSecTxRetryOkCount +
							 pAd->RalinkCounters.OneSecTxFailCount;

			// dynamic adjust antenna evaluation period according to the traffic
			if (TxTotalCnt > 50)
			{
				if (pAd->Mlme.OneSecPeriodicRound % 10 == 0)
				{
					AsicEvaluateRxAnt(pAd);
				}
			}
			else
			{
				if (pAd->Mlme.OneSecPeriodicRound % 3 == 0)
				{
					AsicEvaluateRxAnt(pAd);
				}
			}
		}

		STAMlmePeriodicExec(pAd);

		MlmeResetRalinkCounters(pAd);

		{
#ifdef RT2860
			if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST) && (pAd->bPCIclkOff == FALSE))
#endif
			{
				// When Adhoc beacon is enabled and RTS/CTS is enabled, there is a chance that hardware MAC FSM will run into a deadlock
				// and sending CTS-to-self over and over.
				// Software Patch Solution:
				// 1. Polling debug state register 0x10F4 every one second.
				// 2. If in 0x10F4 the ((bit29==1) && (bit7==1)) OR ((bit29==1) && (bit5==1)), it means the deadlock has occurred.
				// 3. If the deadlock occurred, reset MAC/BBP by setting 0x1004 to 0x0001 for a while then setting it back to 0x000C again.

				UINT32	MacReg = 0;

				RTMP_IO_READ32(pAd, 0x10F4, &MacReg);
				if (((MacReg & 0x20000000) && (MacReg & 0x80)) || ((MacReg & 0x20000000) && (MacReg & 0x20)))
				{
					RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, 0x1);
					RTMPusecDelay(1);
					RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, 0xC);

					DBGPRINT(RT_DEBUG_WARN,("Warning, MAC specific condition occurs \n"));
				}
			}
		}

		RT28XX_MLME_HANDLER(pAd);
	}

	pAd->bUpdateBcnCntDone = FALSE;
}

VOID STAMlmePeriodicExec(
	PRTMP_ADAPTER pAd)
{
#ifdef RT2860
	ULONG			    TxTotalCnt;
#endif
#ifdef RT2870
	ULONG	TxTotalCnt;
	int 	i;
#endif

    if (pAd->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_DISABLE)
    {
    	// WPA MIC error should block association attempt for 60 seconds
    	if (pAd->StaCfg.bBlockAssoc && (pAd->StaCfg.LastMicErrorTime + (60 * OS_HZ) < pAd->Mlme.Now32))
    		pAd->StaCfg.bBlockAssoc = FALSE;
    }

#ifdef RT2860
	//Baron 2008/07/10
	//printk("Baron_Test:\t%s", RTMPGetRalinkEncryModeStr(pAd->StaCfg.WepStatus));
	//If the STA security setting is OPEN or WEP, pAd->StaCfg.WpaSupplicantUP = 0.
	//If the STA security setting is WPAPSK or WPA2PSK, pAd->StaCfg.WpaSupplicantUP = 1.
	if(pAd->StaCfg.WepStatus<2)
	{
		pAd->StaCfg.WpaSupplicantUP = 0;
	}
	else
	{
		pAd->StaCfg.WpaSupplicantUP = 1;
	}
#endif

    if ((pAd->PreMediaState != pAd->IndicateMediaState) && (pAd->CommonCfg.bWirelessEvent))
	{
		if (pAd->IndicateMediaState == NdisMediaStateConnected)
		{
			RTMPSendWirelessEvent(pAd, IW_STA_LINKUP_EVENT_FLAG, pAd->MacTab.Content[BSSID_WCID].Addr, BSS0, 0);
		}
		pAd->PreMediaState = pAd->IndicateMediaState;
	}

#ifdef RT2860
	if ((pAd->OpMode == OPMODE_STA) && (IDLE_ON(pAd)) &&
        (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_ADVANCE_POWER_SAVE_PCIE_DEVICE)) &&
		(pAd->Mlme.SyncMachine.CurrState == SYNC_IDLE) &&
		(pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE) &&
		(RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_START_UP)) &&
		(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_IDLE_RADIO_OFF)))
	{
		RT28xxPciAsicRadioOff(pAd, GUI_IDLE_POWER_SAVE, 0);
	}
#endif



   	AsicStaBbpTuning(pAd);

	TxTotalCnt = pAd->RalinkCounters.OneSecTxNoRetryOkCount +
					 pAd->RalinkCounters.OneSecTxRetryOkCount +
					 pAd->RalinkCounters.OneSecTxFailCount;

	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		// update channel quality for Roaming and UI LinkQuality display
		MlmeCalculateChannelQuality(pAd, pAd->Mlme.Now32);
	}

	// must be AFTER MlmeDynamicTxRateSwitching() because it needs to know if
	// Radio is currently in noisy environment
	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
		AsicAdjustTxPower(pAd);

	if (INFRA_ON(pAd))
	{
		// Is PSM bit consistent with user power management policy?
		// This is the only place that will set PSM bit ON.
		if (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE))
		MlmeCheckPsmChange(pAd, pAd->Mlme.Now32);

		pAd->RalinkCounters.LastOneSecTotalTxCount = TxTotalCnt;

		if ((pAd->StaCfg.LastBeaconRxTime + 1*OS_HZ < pAd->Mlme.Now32) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)) &&
			((TxTotalCnt + pAd->RalinkCounters.OneSecRxOkCnt < 600)))
		{
			RTMPSetAGCInitValue(pAd, BW_20);
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - No BEACON. restore R66 to the low bound(%d) \n", (0x2E + GET_LNA_GAIN(pAd))));
		}

        {
    		if (pAd->CommonCfg.bAPSDCapable && pAd->CommonCfg.APEdcaParm.bAPSDCapable)
    		{
    		    // When APSD is enabled, the period changes as 20 sec
    			if ((pAd->Mlme.OneSecPeriodicRound % 20) == 8)
    				RTMPSendNullFrame(pAd, pAd->CommonCfg.TxRate, TRUE);
    		}
    		else
    		{
    		    // Send out a NULL frame every 10 sec to inform AP that STA is still alive (Avoid being age out)
    			if ((pAd->Mlme.OneSecPeriodicRound % 10) == 8)
                {
                    if (pAd->CommonCfg.bWmmCapable)
    					RTMPSendNullFrame(pAd, pAd->CommonCfg.TxRate, TRUE);
                    else
						RTMPSendNullFrame(pAd, pAd->CommonCfg.TxRate, FALSE);
                }
    		}
        }

		if (CQI_IS_DEAD(pAd->Mlme.ChannelQuality))
			{
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - No BEACON. Dead CQI. Auto Recovery attempt #%ld\n", pAd->RalinkCounters.BadCQIAutoRecoveryCount));
			pAd->StaCfg.CCXAdjacentAPReportFlag = TRUE;
			pAd->StaCfg.CCXAdjacentAPLinkDownTime = pAd->StaCfg.LastBeaconRxTime;

			// Lost AP, send disconnect & link down event
			LinkDown(pAd, FALSE);

            {
                union iwreq_data    wrqu;
                memset(wrqu.ap_addr.sa_data, 0, MAC_ADDR_LEN);
                wireless_send_event(pAd->net_dev, SIOCGIWAP, &wrqu, NULL);
            }

			MlmeAutoReconnectLastSSID(pAd);
		}
		else if (CQI_IS_BAD(pAd->Mlme.ChannelQuality))
		{
			pAd->RalinkCounters.BadCQIAutoRecoveryCount ++;
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - Bad CQI. Auto Recovery attempt #%ld\n", pAd->RalinkCounters.BadCQIAutoRecoveryCount));
			MlmeAutoReconnectLastSSID(pAd);
		}

		// Add auto seamless roaming
		if (pAd->StaCfg.bFastRoaming)
		{
			SHORT	dBmToRoam = (SHORT)pAd->StaCfg.dBmToRoam;

			DBGPRINT(RT_DEBUG_TRACE, ("Rssi=%d, dBmToRoam=%d\n", RTMPMaxRssi(pAd, pAd->StaCfg.RssiSample.LastRssi0, pAd->StaCfg.RssiSample.LastRssi1, pAd->StaCfg.RssiSample.LastRssi2), (CHAR)dBmToRoam));

			if (RTMPMaxRssi(pAd, pAd->StaCfg.RssiSample.LastRssi0, pAd->StaCfg.RssiSample.LastRssi1, pAd->StaCfg.RssiSample.LastRssi2) <= (CHAR)dBmToRoam)
			{
				MlmeCheckForFastRoaming(pAd, pAd->Mlme.Now32);
			}
		}
	}
	else if (ADHOC_ON(pAd))
	{
#ifdef RT2860
		// 2003-04-17 john. this is a patch that driver forces a BEACON out if ASIC fails
		// the "TX BEACON competition" for the entire past 1 sec.
		// So that even when ASIC's BEACONgen engine been blocked
		// by peer's BEACON due to slower system clock, this STA still can send out
		// minimum BEACON to tell the peer I'm alive.
		// drawback is that this BEACON won't be well aligned at TBTT boundary.
		// EnqueueBeaconFrame(pAd);			  // software send BEACON

		// if all 11b peers leave this BSS more than 5 seconds, update Tx rate,
		// restore outgoing BEACON to support B/G-mixed mode
		if ((pAd->CommonCfg.Channel <= 14)			   &&
			(pAd->CommonCfg.MaxTxRate <= RATE_11)	   &&
			(pAd->CommonCfg.MaxDesiredRate > RATE_11)  &&
			((pAd->StaCfg.Last11bBeaconRxTime + 5*OS_HZ) < pAd->Mlme.Now32))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - last 11B peer left, update Tx rates\n"));
			NdisMoveMemory(pAd->StaActive.SupRate, pAd->CommonCfg.SupRate, MAX_LEN_OF_SUPPORTED_RATES);
			pAd->StaActive.SupRateLen = pAd->CommonCfg.SupRateLen;
			MlmeUpdateTxRates(pAd, FALSE, 0);
			MakeIbssBeacon(pAd);		// re-build BEACON frame
			AsicEnableIbssSync(pAd);	// copy to on-chip memory
			pAd->StaCfg.AdhocBOnlyJoined = FALSE;
		}

		if (pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED)
		{
			if ((pAd->StaCfg.AdhocBGJoined) &&
				((pAd->StaCfg.Last11gBeaconRxTime + 5 * OS_HZ) < pAd->Mlme.Now32))
			{
				DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - last 11G peer left\n"));
				pAd->StaCfg.AdhocBGJoined = FALSE;
			}

			if ((pAd->StaCfg.Adhoc20NJoined) &&
				((pAd->StaCfg.Last20NBeaconRxTime + 5 * OS_HZ) < pAd->Mlme.Now32))
			{
				DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - last 20MHz N peer left\n"));
				pAd->StaCfg.Adhoc20NJoined = FALSE;
			}
		}
#endif /* RT2860 */

		//radar detect
		if ((pAd->CommonCfg.Channel > 14)
			&& (pAd->CommonCfg.bIEEE80211H == 1)
			&& RadarChannelCheck(pAd, pAd->CommonCfg.Channel))
		{
			RadarDetectPeriodic(pAd);
		}

		// If all peers leave, and this STA becomes the last one in this IBSS, then change MediaState
		// to DISCONNECTED. But still holding this IBSS (i.e. sending BEACON) so that other STAs can
		// join later.
		if ((pAd->StaCfg.LastBeaconRxTime + ADHOC_BEACON_LOST_TIME < pAd->Mlme.Now32) &&
			OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
		{
			MLME_START_REQ_STRUCT     StartReq;

			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - excessive BEACON lost, last STA in this IBSS, MediaState=Disconnected\n"));
			LinkDown(pAd, FALSE);

			StartParmFill(pAd, &StartReq, pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_START_REQ, sizeof(MLME_START_REQ_STRUCT), &StartReq);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_START;
		}

#ifdef RT2870
		for (i = 1; i < MAX_LEN_OF_MAC_TABLE; i++)
		{
			MAC_TABLE_ENTRY *pEntry = &pAd->MacTab.Content[i];

			if (pEntry->ValidAsCLI == FALSE)
				continue;

			if (pEntry->LastBeaconRxTime + ADHOC_BEACON_LOST_TIME < pAd->Mlme.Now32)
				MacTableDeleteEntry(pAd, pEntry->Aid, pEntry->Addr);
		}
#endif
	}
	else // no INFRA nor ADHOC connection
	{

		if (pAd->StaCfg.bScanReqIsFromWebUI &&
            ((pAd->StaCfg.LastScanTime + 30 * OS_HZ) > pAd->Mlme.Now32))
			goto SKIP_AUTO_SCAN_CONN;
        else
            pAd->StaCfg.bScanReqIsFromWebUI = FALSE;

		if ((pAd->StaCfg.bAutoReconnect == TRUE)
			&& RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_START_UP)
			&& (MlmeValidateSSID(pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen) == TRUE))
		{
			if ((pAd->ScanTab.BssNr==0) && (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE))
			{
				MLME_SCAN_REQ_STRUCT	   ScanReq;

				if ((pAd->StaCfg.LastScanTime + 10 * OS_HZ) < pAd->Mlme.Now32)
				{
					DBGPRINT(RT_DEBUG_TRACE, ("STAMlmePeriodicExec():CNTL - ScanTab.BssNr==0, start a new ACTIVE scan SSID[%s]\n", pAd->MlmeAux.AutoReconnectSsid));
					ScanParmFill(pAd, &ScanReq, pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen, BSS_ANY, SCAN_ACTIVE);
					MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_SCAN_REQ, sizeof(MLME_SCAN_REQ_STRUCT), &ScanReq);
					pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_LIST_SCAN;
					// Reset Missed scan number
					pAd->StaCfg.LastScanTime = pAd->Mlme.Now32;
				}
				else if (pAd->StaCfg.BssType == BSS_ADHOC)	// Quit the forever scan when in a very clean room
					MlmeAutoReconnectLastSSID(pAd);
			}
			else if (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE)
			{
				if ((pAd->Mlme.OneSecPeriodicRound % 7) == 0)
				{
					MlmeAutoScan(pAd);
					pAd->StaCfg.LastScanTime = pAd->Mlme.Now32;
				}
				else
				{
						MlmeAutoReconnectLastSSID(pAd);
				}
			}
		}
	}

SKIP_AUTO_SCAN_CONN:

    if ((pAd->MacTab.Content[BSSID_WCID].TXBAbitmap !=0) && (pAd->MacTab.fAnyBASession == FALSE))
	{
		pAd->MacTab.fAnyBASession = TRUE;
		AsicUpdateProtect(pAd, HT_FORCERTSCTS,  ALLN_SETPROTECT, FALSE, FALSE);
	}
	else if ((pAd->MacTab.Content[BSSID_WCID].TXBAbitmap ==0) && (pAd->MacTab.fAnyBASession == TRUE))
	{
		pAd->MacTab.fAnyBASession = FALSE;
		AsicUpdateProtect(pAd, pAd->MlmeAux.AddHtInfo.AddHtInfo2.OperaionMode,  ALLN_SETPROTECT, FALSE, FALSE);
	}

	return;
}

// Link down report
VOID LinkDownExec(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3)
{

	RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)FunctionContext;

	pAd->IndicateMediaState = NdisMediaStateDisconnected;
	RTMP_IndicateMediaState(pAd);
    pAd->ExtraInfo = GENERAL_LINK_DOWN;
}

// IRQL = DISPATCH_LEVEL
VOID MlmeAutoScan(
	IN PRTMP_ADAPTER pAd)
{
	// check CntlMachine.CurrState to avoid collision with NDIS SetOID request
	if (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - Driver auto scan\n"));
		MlmeEnqueue(pAd,
					MLME_CNTL_STATE_MACHINE,
					OID_802_11_BSSID_LIST_SCAN,
					0,
					NULL);
		RT28XX_MLME_HANDLER(pAd);
	}
}

// IRQL = DISPATCH_LEVEL
VOID MlmeAutoReconnectLastSSID(
	IN PRTMP_ADAPTER pAd)
{


	// check CntlMachine.CurrState to avoid collision with NDIS SetOID request
	if ((pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE) &&
		(MlmeValidateSSID(pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen) == TRUE))
	{
		NDIS_802_11_SSID OidSsid;
		OidSsid.SsidLength = pAd->MlmeAux.AutoReconnectSsidLen;
		NdisMoveMemory(OidSsid.Ssid, pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen);

		DBGPRINT(RT_DEBUG_TRACE, ("Driver auto reconnect to last OID_802_11_SSID setting - %s, len - %d\n", pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen));
		MlmeEnqueue(pAd,
					MLME_CNTL_STATE_MACHINE,
					OID_802_11_SSID,
					sizeof(NDIS_802_11_SSID),
					&OidSsid);
		RT28XX_MLME_HANDLER(pAd);
	}
}

/*
	==========================================================================
	Validate SSID for connection try and rescan purpose
	Valid SSID will have visible chars only.
	The valid length is from 0 to 32.
	IRQL = DISPATCH_LEVEL
	==========================================================================
 */
BOOLEAN MlmeValidateSSID(
	IN PUCHAR	pSsid,
	IN UCHAR	SsidLen)
{
	int	index;

	if (SsidLen > MAX_LEN_OF_SSID)
		return (FALSE);

	// Check each character value
	for (index = 0; index < SsidLen; index++)
	{
		if (pSsid[index] < 0x20)
			return (FALSE);
	}

	// All checked
	return (TRUE);
}

VOID MlmeSelectTxRateTable(
	IN PRTMP_ADAPTER		pAd,
	IN PMAC_TABLE_ENTRY		pEntry,
	IN PUCHAR				*ppTable,
	IN PUCHAR				pTableSize,
	IN PUCHAR				pInitTxRateIdx)
{
	do
	{
		// decide the rate table for tuning
		if (pAd->CommonCfg.TxRateTableSize > 0)
		{
			*ppTable = RateSwitchTable;
			*pTableSize = RateSwitchTable[0];
			*pInitTxRateIdx = RateSwitchTable[1];

			break;
		}

		if ((pAd->OpMode == OPMODE_STA) && ADHOC_ON(pAd))
		{
			if ((pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED) &&
#ifdef RT2860
				!pAd->StaCfg.AdhocBOnlyJoined &&
				!pAd->StaCfg.AdhocBGJoined &&
				(pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0xff) &&
				((pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0x00) || (pAd->Antenna.field.TxPath == 1)))
#endif
#ifdef RT2870
				(pEntry->HTCapability.MCSSet[0] == 0xff) &&
				((pEntry->HTCapability.MCSSet[1] == 0x00) || (pAd->Antenna.field.TxPath == 1)))
#endif
			{// 11N 1S Adhoc
				*ppTable = RateSwitchTable11N1S;
				*pTableSize = RateSwitchTable11N1S[0];
				*pInitTxRateIdx = RateSwitchTable11N1S[1];

			}
			else if ((pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED) &&
#ifdef RT2860
					!pAd->StaCfg.AdhocBOnlyJoined &&
					!pAd->StaCfg.AdhocBGJoined &&
					(pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0xff) &&
					(pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0xff) &&
#endif
#ifdef RT2870
					(pEntry->HTCapability.MCSSet[0] == 0xff) &&
					(pEntry->HTCapability.MCSSet[1] == 0xff) &&
#endif
					(pAd->Antenna.field.TxPath == 2))
			{// 11N 2S Adhoc
				if (pAd->LatchRfRegs.Channel <= 14)
				{
					*ppTable = RateSwitchTable11N2S;
					*pTableSize = RateSwitchTable11N2S[0];
					*pInitTxRateIdx = RateSwitchTable11N2S[1];
				}
				else
				{
					*ppTable = RateSwitchTable11N2SForABand;
					*pTableSize = RateSwitchTable11N2SForABand[0];
					*pInitTxRateIdx = RateSwitchTable11N2SForABand[1];
				}

			}
			else
#ifdef RT2860
				if (pAd->CommonCfg.PhyMode == PHY_11B)
			{
				*ppTable = RateSwitchTable11B;
				*pTableSize = RateSwitchTable11B[0];
				*pInitTxRateIdx = RateSwitchTable11B[1];

			}
	        else if((pAd->LatchRfRegs.Channel <= 14) && (pAd->StaCfg.AdhocBOnlyJoined == TRUE))
#endif
#ifdef RT2870
				if ((pEntry->RateLen == 4)
					&& (pEntry->HTCapability.MCSSet[0] == 0) && (pEntry->HTCapability.MCSSet[1] == 0)
					)
#endif
			{
				// USe B Table when Only b-only Station in my IBSS .
				*ppTable = RateSwitchTable11B;
				*pTableSize = RateSwitchTable11B[0];
				*pInitTxRateIdx = RateSwitchTable11B[1];

			}
			else if (pAd->LatchRfRegs.Channel <= 14)
			{
				*ppTable = RateSwitchTable11BG;
				*pTableSize = RateSwitchTable11BG[0];
				*pInitTxRateIdx = RateSwitchTable11BG[1];

			}
			else
			{
				*ppTable = RateSwitchTable11G;
				*pTableSize = RateSwitchTable11G[0];
				*pInitTxRateIdx = RateSwitchTable11G[1];

			}
			break;
		}

		if ((pEntry->RateLen == 12) && (pEntry->HTCapability.MCSSet[0] == 0xff) &&
			((pEntry->HTCapability.MCSSet[1] == 0x00) || (pAd->CommonCfg.TxStream == 1)))
		{// 11BGN 1S AP
			*ppTable = RateSwitchTable11BGN1S;
			*pTableSize = RateSwitchTable11BGN1S[0];
			*pInitTxRateIdx = RateSwitchTable11BGN1S[1];

			break;
		}

		if ((pEntry->RateLen == 12) && (pEntry->HTCapability.MCSSet[0] == 0xff) &&
			(pEntry->HTCapability.MCSSet[1] == 0xff) && (pAd->CommonCfg.TxStream == 2))
		{// 11BGN 2S AP
			if (pAd->LatchRfRegs.Channel <= 14)
			{
				*ppTable = RateSwitchTable11BGN2S;
				*pTableSize = RateSwitchTable11BGN2S[0];
				*pInitTxRateIdx = RateSwitchTable11BGN2S[1];

			}
			else
			{
				*ppTable = RateSwitchTable11BGN2SForABand;
				*pTableSize = RateSwitchTable11BGN2SForABand[0];
				*pInitTxRateIdx = RateSwitchTable11BGN2SForABand[1];

			}
			break;
		}

		if ((pEntry->HTCapability.MCSSet[0] == 0xff) && ((pEntry->HTCapability.MCSSet[1] == 0x00) || (pAd->CommonCfg.TxStream == 1)))
		{// 11N 1S AP
			*ppTable = RateSwitchTable11N1S;
			*pTableSize = RateSwitchTable11N1S[0];
			*pInitTxRateIdx = RateSwitchTable11N1S[1];

			break;
		}

		if ((pEntry->HTCapability.MCSSet[0] == 0xff) && (pEntry->HTCapability.MCSSet[1] == 0xff) && (pAd->CommonCfg.TxStream == 2))
		{// 11N 2S AP
			if (pAd->LatchRfRegs.Channel <= 14)
			{
			*ppTable = RateSwitchTable11N2S;
			*pTableSize = RateSwitchTable11N2S[0];
			*pInitTxRateIdx = RateSwitchTable11N2S[1];
            }
			else
			{
				*ppTable = RateSwitchTable11N2SForABand;
				*pTableSize = RateSwitchTable11N2SForABand[0];
				*pInitTxRateIdx = RateSwitchTable11N2SForABand[1];
			}

			break;
		}

		//else if ((pAd->StaActive.SupRateLen == 4) && (pAd->StaActive.ExtRateLen == 0) && (pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0) && (pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0))
		if (pEntry->RateLen == 4)
		{// B only AP
			*ppTable = RateSwitchTable11B;
			*pTableSize = RateSwitchTable11B[0];
			*pInitTxRateIdx = RateSwitchTable11B[1];

			break;
		}

		//else if ((pAd->StaActive.SupRateLen + pAd->StaActive.ExtRateLen > 8) && (pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0) && (pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0))
		if ((pEntry->RateLen > 8)
			&& (pEntry->HTCapability.MCSSet[0] == 0) && (pEntry->HTCapability.MCSSet[1] == 0)
			)
		{// B/G  mixed AP
			*ppTable = RateSwitchTable11BG;
			*pTableSize = RateSwitchTable11BG[0];
			*pInitTxRateIdx = RateSwitchTable11BG[1];

			break;
		}

		//else if ((pAd->StaActive.SupRateLen + pAd->StaActive.ExtRateLen == 8) && (pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0) && (pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0))
		if ((pEntry->RateLen == 8)
			&& (pEntry->HTCapability.MCSSet[0] == 0) && (pEntry->HTCapability.MCSSet[1] == 0)
			)
		{// G only AP
			*ppTable = RateSwitchTable11G;
			*pTableSize = RateSwitchTable11G[0];
			*pInitTxRateIdx = RateSwitchTable11G[1];

			break;
		}

		{
			//else if ((pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0) && (pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0))
			if ((pEntry->HTCapability.MCSSet[0] == 0) && (pEntry->HTCapability.MCSSet[1] == 0))
			{	// Legacy mode
				if (pAd->CommonCfg.MaxTxRate <= RATE_11)
				{
					*ppTable = RateSwitchTable11B;
					*pTableSize = RateSwitchTable11B[0];
					*pInitTxRateIdx = RateSwitchTable11B[1];
				}
				else if ((pAd->CommonCfg.MaxTxRate > RATE_11) && (pAd->CommonCfg.MinTxRate > RATE_11))
				{
					*ppTable = RateSwitchTable11G;
					*pTableSize = RateSwitchTable11G[0];
					*pInitTxRateIdx = RateSwitchTable11G[1];

				}
				else
				{
					*ppTable = RateSwitchTable11BG;
					*pTableSize = RateSwitchTable11BG[0];
					*pInitTxRateIdx = RateSwitchTable11BG[1];
				}
				break;
			}

			if (pAd->LatchRfRegs.Channel <= 14)
			{
				if (pAd->CommonCfg.TxStream == 1)
				{
					*ppTable = RateSwitchTable11N1S;
					*pTableSize = RateSwitchTable11N1S[0];
					*pInitTxRateIdx = RateSwitchTable11N1S[1];
					DBGPRINT_RAW(RT_DEBUG_ERROR,("DRS: unkown mode,default use 11N 1S AP \n"));
				}
				else
				{
					*ppTable = RateSwitchTable11N2S;
					*pTableSize = RateSwitchTable11N2S[0];
					*pInitTxRateIdx = RateSwitchTable11N2S[1];
					DBGPRINT_RAW(RT_DEBUG_ERROR,("DRS: unkown mode,default use 11N 2S AP \n"));
				}
			}
			else
			{
				if (pAd->CommonCfg.TxStream == 1)
				{
					*ppTable = RateSwitchTable11N1S;
					*pTableSize = RateSwitchTable11N1S[0];
					*pInitTxRateIdx = RateSwitchTable11N1S[1];
					DBGPRINT_RAW(RT_DEBUG_ERROR,("DRS: unkown mode,default use 11N 1S AP \n"));
				}
				else
				{
					*ppTable = RateSwitchTable11N2SForABand;
					*pTableSize = RateSwitchTable11N2SForABand[0];
					*pInitTxRateIdx = RateSwitchTable11N2SForABand[1];
					DBGPRINT_RAW(RT_DEBUG_ERROR,("DRS: unkown mode,default use 11N 2S AP \n"));
				}
			}

			DBGPRINT_RAW(RT_DEBUG_ERROR,("DRS: unkown mode (SupRateLen=%d, ExtRateLen=%d, MCSSet[0]=0x%x, MCSSet[1]=0x%x)\n",
				pAd->StaActive.SupRateLen, pAd->StaActive.ExtRateLen, pAd->StaActive.SupportedPhyInfo.MCSSet[0], pAd->StaActive.SupportedPhyInfo.MCSSet[1]));
		}
	} while(FALSE);
}

/*
	==========================================================================
	Description:
		This routine checks if there're other APs out there capable for
		roaming. Caller should call this routine only when Link up in INFRA mode
		and channel quality is below CQI_GOOD_THRESHOLD.

	IRQL = DISPATCH_LEVEL

	Output:
	==========================================================================
 */
VOID MlmeCheckForRoaming(
	IN PRTMP_ADAPTER pAd,
	IN ULONG	Now32)
{
	USHORT	   i;
	BSS_TABLE  *pRoamTab = &pAd->MlmeAux.RoamTab;
	BSS_ENTRY  *pBss;

	DBGPRINT(RT_DEBUG_TRACE, ("==> MlmeCheckForRoaming\n"));
	// put all roaming candidates into RoamTab, and sort in RSSI order
	BssTableInit(pRoamTab);
	for (i = 0; i < pAd->ScanTab.BssNr; i++)
	{
		pBss = &pAd->ScanTab.BssEntry[i];

		if ((pBss->LastBeaconRxTime + BEACON_LOST_TIME) < Now32)
			continue;	 // AP disappear
		if (pBss->Rssi <= RSSI_THRESHOLD_FOR_ROAMING)
			continue;	 // RSSI too weak. forget it.
		if (MAC_ADDR_EQUAL(pBss->Bssid, pAd->CommonCfg.Bssid))
			continue;	 // skip current AP
		if (pBss->Rssi < (pAd->StaCfg.RssiSample.LastRssi0 + RSSI_DELTA))
			continue;	 // only AP with stronger RSSI is eligible for roaming

		// AP passing all above rules is put into roaming candidate table
		NdisMoveMemory(&pRoamTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BSS_ENTRY));
		pRoamTab->BssNr += 1;
	}

	if (pRoamTab->BssNr > 0)
	{
		// check CntlMachine.CurrState to avoid collision with NDIS SetOID request
		if (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE)
		{
			pAd->RalinkCounters.PoorCQIRoamingCount ++;
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - Roaming attempt #%ld\n", pAd->RalinkCounters.PoorCQIRoamingCount));
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_MLME_ROAMING_REQ, 0, NULL);
			RT28XX_MLME_HANDLER(pAd);
		}
	}
	DBGPRINT(RT_DEBUG_TRACE, ("<== MlmeCheckForRoaming(# of candidate= %d)\n",pRoamTab->BssNr));
}

/*
	==========================================================================
	Description:
		This routine checks if there're other APs out there capable for
		roaming. Caller should call this routine only when link up in INFRA mode
		and channel quality is below CQI_GOOD_THRESHOLD.

	IRQL = DISPATCH_LEVEL

	Output:
	==========================================================================
 */
VOID MlmeCheckForFastRoaming(
	IN	PRTMP_ADAPTER	pAd,
	IN	ULONG			Now)
{
	USHORT		i;
	BSS_TABLE	*pRoamTab = &pAd->MlmeAux.RoamTab;
	BSS_ENTRY	*pBss;

	DBGPRINT(RT_DEBUG_TRACE, ("==> MlmeCheckForFastRoaming\n"));
	// put all roaming candidates into RoamTab, and sort in RSSI order
	BssTableInit(pRoamTab);
	for (i = 0; i < pAd->ScanTab.BssNr; i++)
	{
		pBss = &pAd->ScanTab.BssEntry[i];

        if ((pBss->Rssi <= -50) && (pBss->Channel == pAd->CommonCfg.Channel))
			continue;	 // RSSI too weak. forget it.
		if (MAC_ADDR_EQUAL(pBss->Bssid, pAd->CommonCfg.Bssid))
			continue;	 // skip current AP
		if (!SSID_EQUAL(pBss->Ssid, pBss->SsidLen, pAd->CommonCfg.Ssid, pAd->CommonCfg.SsidLen))
			continue;	 // skip different SSID
        if (pBss->Rssi < (RTMPMaxRssi(pAd, pAd->StaCfg.RssiSample.LastRssi0, pAd->StaCfg.RssiSample.LastRssi1, pAd->StaCfg.RssiSample.LastRssi2) + RSSI_DELTA))
			continue;	 // skip AP without better RSSI

        DBGPRINT(RT_DEBUG_TRACE, ("LastRssi0 = %d, pBss->Rssi = %d\n", RTMPMaxRssi(pAd, pAd->StaCfg.RssiSample.LastRssi0, pAd->StaCfg.RssiSample.LastRssi1, pAd->StaCfg.RssiSample.LastRssi2), pBss->Rssi));
		// AP passing all above rules is put into roaming candidate table
		NdisMoveMemory(&pRoamTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BSS_ENTRY));
		pRoamTab->BssNr += 1;
	}

	if (pRoamTab->BssNr > 0)
	{
		// check CntlMachine.CurrState to avoid collision with NDIS SetOID request
		if (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE)
		{
			pAd->RalinkCounters.PoorCQIRoamingCount ++;
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - Roaming attempt #%ld\n", pAd->RalinkCounters.PoorCQIRoamingCount));
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_MLME_ROAMING_REQ, 0, NULL);
			RT28XX_MLME_HANDLER(pAd);
		}
	}
	// Maybe site survey required
	else
	{
		if ((pAd->StaCfg.LastScanTime + 10 * 1000) < Now)
		{
			// check CntlMachine.CurrState to avoid collision with NDIS SetOID request
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - Roaming, No eligable entry, try new scan!\n"));
			pAd->StaCfg.ScanCnt = 2;
			pAd->StaCfg.LastScanTime = Now;
			MlmeAutoScan(pAd);
		}
	}

    DBGPRINT(RT_DEBUG_TRACE, ("<== MlmeCheckForFastRoaming (BssNr=%d)\n", pRoamTab->BssNr));
}

/*
	==========================================================================
	Description:
		This routine calculates TxPER, RxPER of the past N-sec period. And
		according to the calculation result, ChannelQuality is calculated here
		to decide if current AP is still doing the job.

		If ChannelQuality is not good, a ROAMing attempt may be tried later.
	Output:
		StaCfg.ChannelQuality - 0..100

	IRQL = DISPATCH_LEVEL

	NOTE: This routine decide channle quality based on RX CRC error ratio.
		Caller should make sure a function call to NICUpdateRawCounters(pAd)
		is performed right before this routine, so that this routine can decide
		channel quality based on the most up-to-date information
	==========================================================================
 */
VOID MlmeCalculateChannelQuality(
	IN PRTMP_ADAPTER pAd,
	IN ULONG Now32)
{
	ULONG TxOkCnt, TxCnt, TxPER, TxPRR;
	ULONG RxCnt, RxPER;
	UCHAR NorRssi;
	CHAR  MaxRssi;
	ULONG BeaconLostTime = BEACON_LOST_TIME;

	MaxRssi = RTMPMaxRssi(pAd, pAd->StaCfg.RssiSample.LastRssi0, pAd->StaCfg.RssiSample.LastRssi1, pAd->StaCfg.RssiSample.LastRssi2);

	//
	// calculate TX packet error ratio and TX retry ratio - if too few TX samples, skip TX related statistics
	//
	TxOkCnt = pAd->RalinkCounters.OneSecTxNoRetryOkCount + pAd->RalinkCounters.OneSecTxRetryOkCount;
	TxCnt = TxOkCnt + pAd->RalinkCounters.OneSecTxFailCount;
	if (TxCnt < 5)
	{
		TxPER = 0;
		TxPRR = 0;
	}
	else
	{
		TxPER = (pAd->RalinkCounters.OneSecTxFailCount * 100) / TxCnt;
		TxPRR = ((TxCnt - pAd->RalinkCounters.OneSecTxNoRetryOkCount) * 100) / TxCnt;
	}

	//
	// calculate RX PER - don't take RxPER into consideration if too few sample
	//
	RxCnt = pAd->RalinkCounters.OneSecRxOkCnt + pAd->RalinkCounters.OneSecRxFcsErrCnt;
	if (RxCnt < 5)
		RxPER = 0;
	else
		RxPER = (pAd->RalinkCounters.OneSecRxFcsErrCnt * 100) / RxCnt;

	//
	// decide ChannelQuality based on: 1)last BEACON received time, 2)last RSSI, 3)TxPER, and 4)RxPER
	//
	if (INFRA_ON(pAd) &&
		(pAd->RalinkCounters.OneSecTxNoRetryOkCount < 2) && // no heavy traffic
		(pAd->StaCfg.LastBeaconRxTime + BeaconLostTime < Now32))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("BEACON lost > %ld msec with TxOkCnt=%ld -> CQI=0\n", BeaconLostTime, TxOkCnt));
		pAd->Mlme.ChannelQuality = 0;
	}
	else
	{
		// Normalize Rssi
		if (MaxRssi > -40)
			NorRssi = 100;
		else if (MaxRssi < -90)
			NorRssi = 0;
		else
			NorRssi = (MaxRssi + 90) * 2;

		// ChannelQuality = W1*RSSI + W2*TxPRR + W3*RxPER	 (RSSI 0..100), (TxPER 100..0), (RxPER 100..0)
		pAd->Mlme.ChannelQuality = (RSSI_WEIGHTING * NorRssi +
								   TX_WEIGHTING * (100 - TxPRR) +
								   RX_WEIGHTING* (100 - RxPER)) / 100;
		if (pAd->Mlme.ChannelQuality >= 100)
			pAd->Mlme.ChannelQuality = 100;
	}

}

VOID MlmeSetTxRate(
	IN PRTMP_ADAPTER		pAd,
	IN PMAC_TABLE_ENTRY		pEntry,
	IN PRTMP_TX_RATE_SWITCH	pTxRate)
{
	UCHAR	MaxMode = MODE_OFDM;

	MaxMode = MODE_HTGREENFIELD;

	if (pTxRate->STBC && (pAd->StaCfg.MaxHTPhyMode.field.STBC) && (pAd->Antenna.field.TxPath == 2))
		pAd->StaCfg.HTPhyMode.field.STBC = STBC_USE;
	else
		pAd->StaCfg.HTPhyMode.field.STBC = STBC_NONE;

	if (pTxRate->CurrMCS < MCS_AUTO)
		pAd->StaCfg.HTPhyMode.field.MCS = pTxRate->CurrMCS;

	if (pAd->StaCfg.HTPhyMode.field.MCS > 7)
		pAd->StaCfg.HTPhyMode.field.STBC = STBC_NONE;

   	if (ADHOC_ON(pAd))
	{
		// If peer adhoc is b-only mode, we can't send 11g rate.
		pAd->StaCfg.HTPhyMode.field.ShortGI = GI_800;
		pEntry->HTPhyMode.field.STBC	= STBC_NONE;

		//
		// For Adhoc MODE_CCK, driver will use AdhocBOnlyJoined flag to roll back to B only if necessary
		//
		pEntry->HTPhyMode.field.MODE	= pTxRate->Mode;
		pEntry->HTPhyMode.field.ShortGI	= pAd->StaCfg.HTPhyMode.field.ShortGI;
		pEntry->HTPhyMode.field.MCS		= pAd->StaCfg.HTPhyMode.field.MCS;

		// Patch speed error in status page
		pAd->StaCfg.HTPhyMode.field.MODE = pEntry->HTPhyMode.field.MODE;
	}
	else
	{
		if (pTxRate->Mode <= MaxMode)
			pAd->StaCfg.HTPhyMode.field.MODE = pTxRate->Mode;

		if (pTxRate->ShortGI && (pAd->StaCfg.MaxHTPhyMode.field.ShortGI))
			pAd->StaCfg.HTPhyMode.field.ShortGI = GI_400;
		else
			pAd->StaCfg.HTPhyMode.field.ShortGI = GI_800;

		// Reexam each bandwidth's SGI support.
		if (pAd->StaCfg.HTPhyMode.field.ShortGI == GI_400)
		{
			if ((pEntry->HTPhyMode.field.BW == BW_20) && (!CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_SGI20_CAPABLE)))
				pAd->StaCfg.HTPhyMode.field.ShortGI = GI_800;
			if ((pEntry->HTPhyMode.field.BW == BW_40) && (!CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_SGI40_CAPABLE)))
				pAd->StaCfg.HTPhyMode.field.ShortGI = GI_800;
		}

		// Turn RTS/CTS rate to 6Mbps.
		if ((pEntry->HTPhyMode.field.MCS == 0) && (pAd->StaCfg.HTPhyMode.field.MCS != 0))
		{
			pEntry->HTPhyMode.field.MCS		= pAd->StaCfg.HTPhyMode.field.MCS;
			if (pAd->MacTab.fAnyBASession)
			{
				AsicUpdateProtect(pAd, HT_FORCERTSCTS, ALLN_SETPROTECT, TRUE, (BOOLEAN)pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent);
			}
			else
			{
				AsicUpdateProtect(pAd, pAd->MlmeAux.AddHtInfo.AddHtInfo2.OperaionMode, ALLN_SETPROTECT, TRUE, (BOOLEAN)pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent);
			}
		}
		else if ((pEntry->HTPhyMode.field.MCS == 8) && (pAd->StaCfg.HTPhyMode.field.MCS != 8))
		{
			pEntry->HTPhyMode.field.MCS		= pAd->StaCfg.HTPhyMode.field.MCS;
			if (pAd->MacTab.fAnyBASession)
			{
				AsicUpdateProtect(pAd, HT_FORCERTSCTS, ALLN_SETPROTECT, TRUE, (BOOLEAN)pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent);
			}
			else
			{
				AsicUpdateProtect(pAd, pAd->MlmeAux.AddHtInfo.AddHtInfo2.OperaionMode, ALLN_SETPROTECT, TRUE, (BOOLEAN)pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent);
			}
		}
		else if ((pEntry->HTPhyMode.field.MCS != 0) && (pAd->StaCfg.HTPhyMode.field.MCS == 0))
		{
			AsicUpdateProtect(pAd, HT_RTSCTS_6M, ALLN_SETPROTECT, TRUE, (BOOLEAN)pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent);

		}
		else if ((pEntry->HTPhyMode.field.MCS != 8) && (pAd->StaCfg.HTPhyMode.field.MCS == 8))
		{
			AsicUpdateProtect(pAd, HT_RTSCTS_6M, ALLN_SETPROTECT, TRUE, (BOOLEAN)pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent);
		}

		pEntry->HTPhyMode.field.STBC	= pAd->StaCfg.HTPhyMode.field.STBC;
		pEntry->HTPhyMode.field.ShortGI	= pAd->StaCfg.HTPhyMode.field.ShortGI;
		pEntry->HTPhyMode.field.MCS		= pAd->StaCfg.HTPhyMode.field.MCS;
		pEntry->HTPhyMode.field.MODE	= pAd->StaCfg.HTPhyMode.field.MODE;

		if ((pAd->StaCfg.MaxHTPhyMode.field.MODE == MODE_HTGREENFIELD) &&
		    pAd->WIFItestbed.bGreenField)
		    pEntry->HTPhyMode.field.MODE = MODE_HTGREENFIELD;
	}

	pAd->LastTxRate = (USHORT)(pEntry->HTPhyMode.word);
}

/*
	==========================================================================
	Description:
		This routine calculates the acumulated TxPER of eaxh TxRate. And
		according to the calculation result, change CommonCfg.TxRate which
		is the stable TX Rate we expect the Radio situation could sustained.

		CommonCfg.TxRate will change dynamically within {RATE_1/RATE_6, MaxTxRate}
	Output:
		CommonCfg.TxRate -

	IRQL = DISPATCH_LEVEL

	NOTE:
		call this routine every second
	==========================================================================
 */
VOID MlmeDynamicTxRateSwitching(
	IN PRTMP_ADAPTER pAd)
{
	UCHAR					UpRateIdx = 0, DownRateIdx = 0, CurrRateIdx;
	ULONG					i, AccuTxTotalCnt = 0, TxTotalCnt;
	ULONG					TxErrorRatio = 0;
	BOOLEAN					bTxRateChanged, bUpgradeQuality = FALSE;
	PRTMP_TX_RATE_SWITCH	pCurrTxRate, pNextTxRate = NULL;
	PUCHAR					pTable;
	UCHAR					TableSize = 0;
	UCHAR					InitTxRateIdx = 0, TrainUp, TrainDown;
	CHAR					Rssi, RssiOffset = 0;
	TX_STA_CNT1_STRUC		StaTx1;
	TX_STA_CNT0_STRUC		TxStaCnt0;
	ULONG					TxRetransmit = 0, TxSuccess = 0, TxFailCount = 0;
	MAC_TABLE_ENTRY			*pEntry;

	//
	// walk through MAC table, see if need to change AP's TX rate toward each entry
	//
   	for (i = 1; i < MAX_LEN_OF_MAC_TABLE; i++)
	{
		pEntry = &pAd->MacTab.Content[i];

		// check if this entry need to switch rate automatically
		if (RTMPCheckEntryEnableAutoRateSwitch(pAd, pEntry) == FALSE)
			continue;

		if ((pAd->MacTab.Size == 1) || (pEntry->ValidAsDls))
		{
#ifdef RT2860
			Rssi = RTMPMaxRssi(pAd, (CHAR)pAd->StaCfg.RssiSample.AvgRssi0, (CHAR)pAd->StaCfg.RssiSample.AvgRssi1, (CHAR)pAd->StaCfg.RssiSample.AvgRssi2);
#endif
#ifdef RT2870
			Rssi = RTMPMaxRssi(pAd,
							   pAd->StaCfg.RssiSample.AvgRssi0,
							   pAd->StaCfg.RssiSample.AvgRssi1,
							   pAd->StaCfg.RssiSample.AvgRssi2);
#endif

			// Update statistic counter
			RTMP_IO_READ32(pAd, TX_STA_CNT0, &TxStaCnt0.word);
			RTMP_IO_READ32(pAd, TX_STA_CNT1, &StaTx1.word);
			pAd->bUpdateBcnCntDone = TRUE;
			TxRetransmit = StaTx1.field.TxRetransmit;
			TxSuccess = StaTx1.field.TxSuccess;
			TxFailCount = TxStaCnt0.field.TxFailCount;
			TxTotalCnt = TxRetransmit + TxSuccess + TxFailCount;

			pAd->RalinkCounters.OneSecTxRetryOkCount += StaTx1.field.TxRetransmit;
			pAd->RalinkCounters.OneSecTxNoRetryOkCount += StaTx1.field.TxSuccess;
			pAd->RalinkCounters.OneSecTxFailCount += TxStaCnt0.field.TxFailCount;
			pAd->WlanCounters.TransmittedFragmentCount.u.LowPart += StaTx1.field.TxSuccess;
			pAd->WlanCounters.RetryCount.u.LowPart += StaTx1.field.TxRetransmit;
			pAd->WlanCounters.FailedCount.u.LowPart += TxStaCnt0.field.TxFailCount;

			// if no traffic in the past 1-sec period, don't change TX rate,
			// but clear all bad history. because the bad history may affect the next
			// Chariot throughput test
			AccuTxTotalCnt = pAd->RalinkCounters.OneSecTxNoRetryOkCount +
						 pAd->RalinkCounters.OneSecTxRetryOkCount +
						 pAd->RalinkCounters.OneSecTxFailCount;

			if (TxTotalCnt)
				TxErrorRatio = ((TxRetransmit + TxFailCount) * 100) / TxTotalCnt;
		}
		else
		{
#ifdef RT2860
			Rssi = RTMPMaxRssi(pAd, (CHAR)pEntry->RssiSample.AvgRssi0, (CHAR)pEntry->RssiSample.AvgRssi1, (CHAR)pEntry->RssiSample.AvgRssi2);
#endif
#ifdef RT2870
			if (INFRA_ON(pAd) && (i == 1))
				Rssi = RTMPMaxRssi(pAd,
								   pAd->StaCfg.RssiSample.AvgRssi0,
								   pAd->StaCfg.RssiSample.AvgRssi1,
								   pAd->StaCfg.RssiSample.AvgRssi2);
			else
				Rssi = RTMPMaxRssi(pAd,
								   pEntry->RssiSample.AvgRssi0,
								   pEntry->RssiSample.AvgRssi1,
								   pEntry->RssiSample.AvgRssi2);
#endif

			TxTotalCnt = pEntry->OneSecTxNoRetryOkCount +
				 pEntry->OneSecTxRetryOkCount +
				 pEntry->OneSecTxFailCount;

			if (TxTotalCnt)
				TxErrorRatio = ((pEntry->OneSecTxRetryOkCount + pEntry->OneSecTxFailCount) * 100) / TxTotalCnt;
		}

		CurrRateIdx = pEntry->CurrTxRateIndex;

		MlmeSelectTxRateTable(pAd, pEntry, &pTable, &TableSize, &InitTxRateIdx);

		if (CurrRateIdx >= TableSize)
		{
			CurrRateIdx = TableSize - 1;
		}

		// When switch from Fixed rate -> auto rate, the REAL TX rate might be different from pAd->CommonCfg.TxRateIndex.
		// So need to sync here.
		pCurrTxRate = (PRTMP_TX_RATE_SWITCH) &pTable[(CurrRateIdx+1)*5];
		if ((pEntry->HTPhyMode.field.MCS != pCurrTxRate->CurrMCS)
			//&& (pAd->StaCfg.bAutoTxRateSwitch == TRUE)
			)
		{

			// Need to sync Real Tx rate and our record.
			// Then return for next DRS.
			pCurrTxRate = (PRTMP_TX_RATE_SWITCH) &pTable[(InitTxRateIdx+1)*5];
			pEntry->CurrTxRateIndex = InitTxRateIdx;
			MlmeSetTxRate(pAd, pEntry, pCurrTxRate);

			// reset all OneSecTx counters
			RESET_ONE_SEC_TX_CNT(pEntry);
			continue;
		}

		// decide the next upgrade rate and downgrade rate, if any
		if ((CurrRateIdx > 0) && (CurrRateIdx < (TableSize - 1)))
		{
			UpRateIdx = CurrRateIdx + 1;
			DownRateIdx = CurrRateIdx -1;
		}
		else if (CurrRateIdx == 0)
		{
			UpRateIdx = CurrRateIdx + 1;
			DownRateIdx = CurrRateIdx;
		}
		else if (CurrRateIdx == (TableSize - 1))
		{
			UpRateIdx = CurrRateIdx;
			DownRateIdx = CurrRateIdx - 1;
		}

		pCurrTxRate = (PRTMP_TX_RATE_SWITCH) &pTable[(CurrRateIdx+1)*5];

		if ((Rssi > -65) && (pCurrTxRate->Mode >= MODE_HTMIX))
		{
			TrainUp		= (pCurrTxRate->TrainUp + (pCurrTxRate->TrainUp >> 1));
			TrainDown	= (pCurrTxRate->TrainDown + (pCurrTxRate->TrainDown >> 1));
		}
		else
		{
			TrainUp		= pCurrTxRate->TrainUp;
			TrainDown	= pCurrTxRate->TrainDown;
		}

		//pAd->DrsCounters.LastTimeTxRateChangeAction = pAd->DrsCounters.LastSecTxRateChangeAction;

		//
		// Keep the last time TxRateChangeAction status.
		//
		pEntry->LastTimeTxRateChangeAction = pEntry->LastSecTxRateChangeAction;



		//
		// CASE 1. when TX samples are fewer than 15, then decide TX rate solely on RSSI
		//         (criteria copied from RT2500 for Netopia case)
		//
		if (TxTotalCnt <= 15)
		{
			CHAR	idx = 0;
			UCHAR	TxRateIdx;
			//UCHAR	MCS0 = 0, MCS1 = 0, MCS2 = 0, MCS3 = 0, MCS4 = 0, MCS7 = 0, MCS12 = 0, MCS13 = 0, MCS14 = 0, MCS15 = 0;
			UCHAR	MCS0 = 0, MCS1 = 0, MCS2 = 0, MCS3 = 0, MCS4 = 0,  MCS5 =0, MCS6 = 0, MCS7 = 0;
	        UCHAR	MCS12 = 0, MCS13 = 0, MCS14 = 0, MCS15 = 0;
			UCHAR	MCS20 = 0, MCS21 = 0, MCS22 = 0, MCS23 = 0; // 3*3

			// check the existence and index of each needed MCS
			while (idx < pTable[0])
			{
				pCurrTxRate = (PRTMP_TX_RATE_SWITCH) &pTable[(idx+1)*5];

				if (pCurrTxRate->CurrMCS == MCS_0)
				{
					MCS0 = idx;
				}
				else if (pCurrTxRate->CurrMCS == MCS_1)
				{
					MCS1 = idx;
				}
				else if (pCurrTxRate->CurrMCS == MCS_2)
				{
					MCS2 = idx;
				}
				else if (pCurrTxRate->CurrMCS == MCS_3)
				{
					MCS3 = idx;
				}
				else if (pCurrTxRate->CurrMCS == MCS_4)
				{
					MCS4 = idx;
				}
	            else if (pCurrTxRate->CurrMCS == MCS_5)
	            {
	                MCS5 = idx;
	            }
	            else if (pCurrTxRate->CurrMCS == MCS_6)
	            {
	                MCS6 = idx;
	            }
				//else if (pCurrTxRate->CurrMCS == MCS_7)
				else if ((pCurrTxRate->CurrMCS == MCS_7) && (pCurrTxRate->ShortGI == GI_800))	// prevent the highest MCS using short GI when 1T and low throughput
				{
					MCS7 = idx;
				}
				else if (pCurrTxRate->CurrMCS == MCS_12)
				{
					MCS12 = idx;
				}
				else if (pCurrTxRate->CurrMCS == MCS_13)
				{
					MCS13 = idx;
				}
				else if (pCurrTxRate->CurrMCS == MCS_14)
				{
					MCS14 = idx;
				}
				else if ((pCurrTxRate->CurrMCS == MCS_15) && (pCurrTxRate->ShortGI == GI_800))	//we hope to use ShortGI as initial rate, however Atheros's chip has bugs when short GI
				{
					MCS15 = idx;
				}
				else if (pCurrTxRate->CurrMCS == MCS_20) // 3*3
				{
					MCS20 = idx;
				}
				else if (pCurrTxRate->CurrMCS == MCS_21)
				{
					MCS21 = idx;
				}
				else if (pCurrTxRate->CurrMCS == MCS_22)
				{
					MCS22 = idx;
				}
				else if (pCurrTxRate->CurrMCS == MCS_23)
				{
					MCS23 = idx;
				}
				idx ++;
			}

			if (pAd->LatchRfRegs.Channel <= 14)
			{
				if (pAd->NicConfig2.field.ExternalLNAForG)
				{
					RssiOffset = 2;
				}
				else
				{
					RssiOffset = 5;
				}
			}
			else
			{
				if (pAd->NicConfig2.field.ExternalLNAForA)
				{
					RssiOffset = 5;
				}
				else
				{
					RssiOffset = 8;
				}
			}

			/*if (MCS15)*/
			if ((pTable == RateSwitchTable11BGN3S) ||
				(pTable == RateSwitchTable11N3S) ||
				(pTable == RateSwitchTable))
			{// N mode with 3 stream // 3*3
				if (MCS23 && (Rssi >= -70))
					TxRateIdx = MCS15;
				else if (MCS22 && (Rssi >= -72))
					TxRateIdx = MCS14;
        	    else if (MCS21 && (Rssi >= -76))
					TxRateIdx = MCS13;
				else if (MCS20 && (Rssi >= -78))
					TxRateIdx = MCS12;
			else if (MCS4 && (Rssi >= -82))
				TxRateIdx = MCS4;
			else if (MCS3 && (Rssi >= -84))
				TxRateIdx = MCS3;
			else if (MCS2 && (Rssi >= -86))
				TxRateIdx = MCS2;
			else if (MCS1 && (Rssi >= -88))
				TxRateIdx = MCS1;
			else
				TxRateIdx = MCS0;
		}
		else if ((pTable == RateSwitchTable11BGN2S) || (pTable == RateSwitchTable11BGN2SForABand) ||(pTable == RateSwitchTable11N2S) ||(pTable == RateSwitchTable11N2SForABand)) // 3*3
			{// N mode with 2 stream
				if (MCS15 && (Rssi >= (-70+RssiOffset)))
					TxRateIdx = MCS15;
				else if (MCS14 && (Rssi >= (-72+RssiOffset)))
					TxRateIdx = MCS14;
				else if (MCS13 && (Rssi >= (-76+RssiOffset)))
					TxRateIdx = MCS13;
				else if (MCS12 && (Rssi >= (-78+RssiOffset)))
					TxRateIdx = MCS12;
				else if (MCS4 && (Rssi >= (-82+RssiOffset)))
					TxRateIdx = MCS4;
				else if (MCS3 && (Rssi >= (-84+RssiOffset)))
					TxRateIdx = MCS3;
				else if (MCS2 && (Rssi >= (-86+RssiOffset)))
					TxRateIdx = MCS2;
				else if (MCS1 && (Rssi >= (-88+RssiOffset)))
					TxRateIdx = MCS1;
				else
					TxRateIdx = MCS0;
			}
			else if ((pTable == RateSwitchTable11BGN1S) || (pTable == RateSwitchTable11N1S))
			{// N mode with 1 stream
				if (MCS7 && (Rssi > (-72+RssiOffset)))
					TxRateIdx = MCS7;
				else if (MCS6 && (Rssi > (-74+RssiOffset)))
					TxRateIdx = MCS6;
				else if (MCS5 && (Rssi > (-77+RssiOffset)))
					TxRateIdx = MCS5;
				else if (MCS4 && (Rssi > (-79+RssiOffset)))
					TxRateIdx = MCS4;
				else if (MCS3 && (Rssi > (-81+RssiOffset)))
					TxRateIdx = MCS3;
				else if (MCS2 && (Rssi > (-83+RssiOffset)))
					TxRateIdx = MCS2;
				else if (MCS1 && (Rssi > (-86+RssiOffset)))
					TxRateIdx = MCS1;
				else
					TxRateIdx = MCS0;
			}
			else
			{// Legacy mode
				if (MCS7 && (Rssi > -70))
					TxRateIdx = MCS7;
				else if (MCS6 && (Rssi > -74))
					TxRateIdx = MCS6;
				else if (MCS5 && (Rssi > -78))
					TxRateIdx = MCS5;
				else if (MCS4 && (Rssi > -82))
					TxRateIdx = MCS4;
				else if (MCS4 == 0)	// for B-only mode
					TxRateIdx = MCS3;
				else if (MCS3 && (Rssi > -85))
					TxRateIdx = MCS3;
				else if (MCS2 && (Rssi > -87))
					TxRateIdx = MCS2;
				else if (MCS1 && (Rssi > -90))
					TxRateIdx = MCS1;
				else
					TxRateIdx = MCS0;
			}

			{
				pEntry->CurrTxRateIndex = TxRateIdx;
				pNextTxRate = (PRTMP_TX_RATE_SWITCH) &pTable[(pEntry->CurrTxRateIndex+1)*5];
				MlmeSetTxRate(pAd, pEntry, pNextTxRate);
			}

			NdisZeroMemory(pEntry->TxQuality, sizeof(USHORT) * MAX_STEP_OF_TX_RATE_SWITCH);
			NdisZeroMemory(pEntry->PER, sizeof(UCHAR) * MAX_STEP_OF_TX_RATE_SWITCH);
			pEntry->fLastSecAccordingRSSI = TRUE;
			// reset all OneSecTx counters
			RESET_ONE_SEC_TX_CNT(pEntry);

			continue;
		}

		if (pEntry->fLastSecAccordingRSSI == TRUE)
		{
			pEntry->fLastSecAccordingRSSI = FALSE;
			pEntry->LastSecTxRateChangeAction = 0;
			// reset all OneSecTx counters
			RESET_ONE_SEC_TX_CNT(pEntry);

			continue;
		}

		do
		{
			BOOLEAN	bTrainUpDown = FALSE;

			pEntry->CurrTxRateStableTime ++;

			// downgrade TX quality if PER >= Rate-Down threshold
			if (TxErrorRatio >= TrainDown)
			{
				bTrainUpDown = TRUE;
				pEntry->TxQuality[CurrRateIdx] = DRS_TX_QUALITY_WORST_BOUND;
			}
			// upgrade TX quality if PER <= Rate-Up threshold
			else if (TxErrorRatio <= TrainUp)
			{
				bTrainUpDown = TRUE;
				bUpgradeQuality = TRUE;
				if (pEntry->TxQuality[CurrRateIdx])
					pEntry->TxQuality[CurrRateIdx] --;  // quality very good in CurrRate

				if (pEntry->TxRateUpPenalty)
					pEntry->TxRateUpPenalty --;
				else if (pEntry->TxQuality[UpRateIdx])
					pEntry->TxQuality[UpRateIdx] --;    // may improve next UP rate's quality
			}

			pEntry->PER[CurrRateIdx] = (UCHAR)TxErrorRatio;

			if (bTrainUpDown)
			{
				// perform DRS - consider TxRate Down first, then rate up.
				if ((CurrRateIdx != DownRateIdx) && (pEntry->TxQuality[CurrRateIdx] >= DRS_TX_QUALITY_WORST_BOUND))
				{
					pEntry->CurrTxRateIndex = DownRateIdx;
				}
				else if ((CurrRateIdx != UpRateIdx) && (pEntry->TxQuality[UpRateIdx] <= 0))
				{
					pEntry->CurrTxRateIndex = UpRateIdx;
				}
			}
		} while (FALSE);

		// if rate-up happen, clear all bad history of all TX rates
		if (pEntry->CurrTxRateIndex > CurrRateIdx)
		{
			pEntry->CurrTxRateStableTime = 0;
			pEntry->TxRateUpPenalty = 0;
			pEntry->LastSecTxRateChangeAction = 1; // rate UP
			NdisZeroMemory(pEntry->TxQuality, sizeof(USHORT) * MAX_STEP_OF_TX_RATE_SWITCH);
			NdisZeroMemory(pEntry->PER, sizeof(UCHAR) * MAX_STEP_OF_TX_RATE_SWITCH);

			//
			// For TxRate fast train up
			//
			if (!pAd->StaCfg.StaQuickResponeForRateUpTimerRunning)
			{
				RTMPSetTimer(&pAd->StaCfg.StaQuickResponeForRateUpTimer, 100);

				pAd->StaCfg.StaQuickResponeForRateUpTimerRunning = TRUE;
			}
			bTxRateChanged = TRUE;
		}
		// if rate-down happen, only clear DownRate's bad history
		else if (pEntry->CurrTxRateIndex < CurrRateIdx)
		{
			pEntry->CurrTxRateStableTime = 0;
			pEntry->TxRateUpPenalty = 0;           // no penalty
			pEntry->LastSecTxRateChangeAction = 2; // rate DOWN
			pEntry->TxQuality[pEntry->CurrTxRateIndex] = 0;
			pEntry->PER[pEntry->CurrTxRateIndex] = 0;

			//
			// For TxRate fast train down
			//
			if (!pAd->StaCfg.StaQuickResponeForRateUpTimerRunning)
			{
				RTMPSetTimer(&pAd->StaCfg.StaQuickResponeForRateUpTimer, 100);

				pAd->StaCfg.StaQuickResponeForRateUpTimerRunning = TRUE;
			}
			bTxRateChanged = TRUE;
		}
		else
		{
			pEntry->LastSecTxRateChangeAction = 0; // rate no change
			bTxRateChanged = FALSE;
		}

		pEntry->LastTxOkCount = TxSuccess;

		// reset all OneSecTx counters
		RESET_ONE_SEC_TX_CNT(pEntry);

		pNextTxRate = (PRTMP_TX_RATE_SWITCH) &pTable[(pEntry->CurrTxRateIndex+1)*5];
		if (bTxRateChanged && pNextTxRate)
		{
			MlmeSetTxRate(pAd, pEntry, pNextTxRate);
		}
	}
}

/*
	========================================================================
	Routine Description:
		Station side, Auto TxRate faster train up timer call back function.

	Arguments:
		SystemSpecific1			- Not used.
		FunctionContext			- Pointer to our Adapter context.
		SystemSpecific2			- Not used.
		SystemSpecific3			- Not used.

	Return Value:
		None

	========================================================================
*/
VOID StaQuickResponeForRateUpExec(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3)
{
	PRTMP_ADAPTER			pAd = (PRTMP_ADAPTER)FunctionContext;
	UCHAR					UpRateIdx = 0, DownRateIdx = 0, CurrRateIdx = 0;
	ULONG					TxTotalCnt;
	ULONG					TxErrorRatio = 0;
#ifdef RT2860
	BOOLEAN					bTxRateChanged = TRUE; //, bUpgradeQuality = FALSE;
#endif
#ifdef RT2870
	BOOLEAN					bTxRateChanged; //, bUpgradeQuality = FALSE;
#endif
	PRTMP_TX_RATE_SWITCH	pCurrTxRate, pNextTxRate = NULL;
	PUCHAR					pTable;
	UCHAR					TableSize = 0;
	UCHAR					InitTxRateIdx = 0, TrainUp, TrainDown;
	TX_STA_CNT1_STRUC		StaTx1;
	TX_STA_CNT0_STRUC		TxStaCnt0;
	CHAR					Rssi, ratio;
	ULONG					TxRetransmit = 0, TxSuccess = 0, TxFailCount = 0;
	MAC_TABLE_ENTRY			*pEntry;
	ULONG					i;

	pAd->StaCfg.StaQuickResponeForRateUpTimerRunning = FALSE;

    //
    // walk through MAC table, see if need to change AP's TX rate toward each entry
    //
	for (i = 1; i < MAX_LEN_OF_MAC_TABLE; i++)
	{
		pEntry = &pAd->MacTab.Content[i];

		// check if this entry need to switch rate automatically
		if (RTMPCheckEntryEnableAutoRateSwitch(pAd, pEntry) == FALSE)
			continue;

#ifdef RT2860
		//Rssi = RTMPMaxRssi(pAd, (CHAR)pAd->StaCfg.AvgRssi0, (CHAR)pAd->StaCfg.AvgRssi1, (CHAR)pAd->StaCfg.AvgRssi2);
	    if (pAd->Antenna.field.TxPath > 1)
			Rssi = (pAd->StaCfg.RssiSample.AvgRssi0 + pAd->StaCfg.RssiSample.AvgRssi1) >> 1;
		else
			Rssi = pAd->StaCfg.RssiSample.AvgRssi0;
#endif
#ifdef RT2870
		if (INFRA_ON(pAd) && (i == 1))
			Rssi = RTMPMaxRssi(pAd,
							   pAd->StaCfg.RssiSample.AvgRssi0,
							   pAd->StaCfg.RssiSample.AvgRssi1,
							   pAd->StaCfg.RssiSample.AvgRssi2);
		else
			Rssi = RTMPMaxRssi(pAd,
							   pEntry->RssiSample.AvgRssi0,
							   pEntry->RssiSample.AvgRssi1,
							   pEntry->RssiSample.AvgRssi2);
#endif

		CurrRateIdx = pAd->CommonCfg.TxRateIndex;

			MlmeSelectTxRateTable(pAd, pEntry, &pTable, &TableSize, &InitTxRateIdx);

		// decide the next upgrade rate and downgrade rate, if any
		if ((CurrRateIdx > 0) && (CurrRateIdx < (TableSize - 1)))
		{
			UpRateIdx = CurrRateIdx + 1;
			DownRateIdx = CurrRateIdx -1;
		}
		else if (CurrRateIdx == 0)
		{
			UpRateIdx = CurrRateIdx + 1;
			DownRateIdx = CurrRateIdx;
		}
		else if (CurrRateIdx == (TableSize - 1))
		{
			UpRateIdx = CurrRateIdx;
			DownRateIdx = CurrRateIdx - 1;
		}

		pCurrTxRate = (PRTMP_TX_RATE_SWITCH) &pTable[(CurrRateIdx+1)*5];

		if ((Rssi > -65) && (pCurrTxRate->Mode >= MODE_HTMIX))
		{
			TrainUp		= (pCurrTxRate->TrainUp + (pCurrTxRate->TrainUp >> 1));
			TrainDown	= (pCurrTxRate->TrainDown + (pCurrTxRate->TrainDown >> 1));
		}
		else
		{
			TrainUp		= pCurrTxRate->TrainUp;
			TrainDown	= pCurrTxRate->TrainDown;
		}

		if (pAd->MacTab.Size == 1)
		{
			// Update statistic counter
			RTMP_IO_READ32(pAd, TX_STA_CNT0, &TxStaCnt0.word);
			RTMP_IO_READ32(pAd, TX_STA_CNT1, &StaTx1.word);

			TxRetransmit = StaTx1.field.TxRetransmit;
			TxSuccess = StaTx1.field.TxSuccess;
			TxFailCount = TxStaCnt0.field.TxFailCount;
			TxTotalCnt = TxRetransmit + TxSuccess + TxFailCount;

			pAd->RalinkCounters.OneSecTxRetryOkCount += StaTx1.field.TxRetransmit;
			pAd->RalinkCounters.OneSecTxNoRetryOkCount += StaTx1.field.TxSuccess;
			pAd->RalinkCounters.OneSecTxFailCount += TxStaCnt0.field.TxFailCount;
			pAd->WlanCounters.TransmittedFragmentCount.u.LowPart += StaTx1.field.TxSuccess;
			pAd->WlanCounters.RetryCount.u.LowPart += StaTx1.field.TxRetransmit;
			pAd->WlanCounters.FailedCount.u.LowPart += TxStaCnt0.field.TxFailCount;

			if (TxTotalCnt)
				TxErrorRatio = ((TxRetransmit + TxFailCount) * 100) / TxTotalCnt;
		}
		else
		{
			TxTotalCnt = pEntry->OneSecTxNoRetryOkCount +
				 pEntry->OneSecTxRetryOkCount +
				 pEntry->OneSecTxFailCount;

			if (TxTotalCnt)
				TxErrorRatio = ((pEntry->OneSecTxRetryOkCount + pEntry->OneSecTxFailCount) * 100) / TxTotalCnt;
		}


		//
		// CASE 1. when TX samples are fewer than 15, then decide TX rate solely on RSSI
		//         (criteria copied from RT2500 for Netopia case)
		//
		if (TxTotalCnt <= 12)
		{
			NdisZeroMemory(pAd->DrsCounters.TxQuality, sizeof(USHORT) * MAX_STEP_OF_TX_RATE_SWITCH);
			NdisZeroMemory(pAd->DrsCounters.PER, sizeof(UCHAR) * MAX_STEP_OF_TX_RATE_SWITCH);

			if ((pAd->DrsCounters.LastSecTxRateChangeAction == 1) && (CurrRateIdx != DownRateIdx))
			{
				pAd->CommonCfg.TxRateIndex = DownRateIdx;
				pAd->DrsCounters.TxQuality[CurrRateIdx] = DRS_TX_QUALITY_WORST_BOUND;
			}
			else if ((pAd->DrsCounters.LastSecTxRateChangeAction == 2) && (CurrRateIdx != UpRateIdx))
			{
				pAd->CommonCfg.TxRateIndex = UpRateIdx;
			}

			DBGPRINT_RAW(RT_DEBUG_TRACE,("QuickDRS: TxTotalCnt <= 15, train back to original rate \n"));
			return;
		}

		do
		{
			ULONG OneSecTxNoRetryOKRationCount;

			if (pAd->DrsCounters.LastTimeTxRateChangeAction == 0)
				ratio = 5;
			else
				ratio = 4;

			// downgrade TX quality if PER >= Rate-Down threshold
			if (TxErrorRatio >= TrainDown)
			{
				pAd->DrsCounters.TxQuality[CurrRateIdx] = DRS_TX_QUALITY_WORST_BOUND;
			}

			pAd->DrsCounters.PER[CurrRateIdx] = (UCHAR)TxErrorRatio;

			OneSecTxNoRetryOKRationCount = (TxSuccess * ratio);

			// perform DRS - consider TxRate Down first, then rate up.
			if ((pAd->DrsCounters.LastSecTxRateChangeAction == 1) && (CurrRateIdx != DownRateIdx))
			{
				if ((pAd->DrsCounters.LastTxOkCount + 2) >= OneSecTxNoRetryOKRationCount)
				{
					pAd->CommonCfg.TxRateIndex = DownRateIdx;
					pAd->DrsCounters.TxQuality[CurrRateIdx] = DRS_TX_QUALITY_WORST_BOUND;

				}

			}
			else if ((pAd->DrsCounters.LastSecTxRateChangeAction == 2) && (CurrRateIdx != UpRateIdx))
			{
				if ((TxErrorRatio >= 50) || (TxErrorRatio >= TrainDown))
				{

				}
				else if ((pAd->DrsCounters.LastTxOkCount + 2) >= OneSecTxNoRetryOKRationCount)
				{
					pAd->CommonCfg.TxRateIndex = UpRateIdx;
				}
			}
		}while (FALSE);

		// if rate-up happen, clear all bad history of all TX rates
		if (pAd->CommonCfg.TxRateIndex > CurrRateIdx)
		{
			pAd->DrsCounters.TxRateUpPenalty = 0;
			NdisZeroMemory(pAd->DrsCounters.TxQuality, sizeof(USHORT) * MAX_STEP_OF_TX_RATE_SWITCH);
			NdisZeroMemory(pAd->DrsCounters.PER, sizeof(UCHAR) * MAX_STEP_OF_TX_RATE_SWITCH);
#ifdef RT2870
			bTxRateChanged = TRUE;
#endif
		}
		// if rate-down happen, only clear DownRate's bad history
		else if (pAd->CommonCfg.TxRateIndex < CurrRateIdx)
		{
			DBGPRINT_RAW(RT_DEBUG_TRACE,("QuickDRS: --TX rate from %d to %d \n", CurrRateIdx, pAd->CommonCfg.TxRateIndex));

			pAd->DrsCounters.TxRateUpPenalty = 0;           // no penalty
			pAd->DrsCounters.TxQuality[pAd->CommonCfg.TxRateIndex] = 0;
			pAd->DrsCounters.PER[pAd->CommonCfg.TxRateIndex] = 0;
#ifdef RT2870
			bTxRateChanged = TRUE;
#endif
		}
		else
		{
			bTxRateChanged = FALSE;
		}

		pNextTxRate = (PRTMP_TX_RATE_SWITCH) &pTable[(pAd->CommonCfg.TxRateIndex+1)*5];
		if (bTxRateChanged && pNextTxRate)
		{
			MlmeSetTxRate(pAd, pEntry, pNextTxRate);
		}
	}
}

/*
	==========================================================================
	Description:
		This routine is executed periodically inside MlmePeriodicExec() after
		association with an AP.
		It checks if StaCfg.Psm is consistent with user policy (recorded in
		StaCfg.WindowsPowerMode). If not, enforce user policy. However,
		there're some conditions to consider:
		1. we don't support power-saving in ADHOC mode, so Psm=PWR_ACTIVE all
		   the time when Mibss==TRUE
		2. When link up in INFRA mode, Psm should not be switch to PWR_SAVE
		   if outgoing traffic available in TxRing or MgmtRing.
	Output:
		1. change pAd->StaCfg.Psm to PWR_SAVE or leave it untouched

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID MlmeCheckPsmChange(
	IN PRTMP_ADAPTER pAd,
	IN ULONG	Now32)
{
	ULONG	PowerMode;

	// condition -
	// 1. Psm maybe ON only happen in INFRASTRUCTURE mode
	// 2. user wants either MAX_PSP or FAST_PSP
	// 3. but current psm is not in PWR_SAVE
	// 4. CNTL state machine is not doing SCANning
	// 5. no TX SUCCESS event for the past 1-sec period
#ifdef NDIS51_MINIPORT
	if (pAd->StaCfg.WindowsPowerProfile == NdisPowerProfileBattery)
		PowerMode = pAd->StaCfg.WindowsBatteryPowerMode;
	else
#endif
		PowerMode = pAd->StaCfg.WindowsPowerMode;

	if (INFRA_ON(pAd) &&
		(PowerMode != Ndis802_11PowerModeCAM) &&
		(pAd->StaCfg.Psm == PWR_ACTIVE) &&
#ifdef RT2860
		RTMP_TEST_PSFLAG(pAd, fRTMP_PS_CAN_GO_SLEEP))
#else
		(pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE))
#endif
	{
		// add by johnli, use Rx OK data count per second to calculate throughput
		// If Ttraffic is too high ( > 400 Rx per second), don't go to sleep mode. If tx rate is low, use low criteria
		// Mode=CCK/MCS=3 => 11 Mbps, Mode=OFDM/MCS=3 => 18 Mbps
		if (((pAd->StaCfg.HTPhyMode.field.MCS <= 3) &&
				(pAd->RalinkCounters.OneSecRxOkDataCnt < (ULONG)100)) ||
			((pAd->StaCfg.HTPhyMode.field.MCS > 3) &&
			(pAd->RalinkCounters.OneSecRxOkDataCnt < (ULONG)400)))
		{
				// Get this time
			NdisGetSystemUpTime(&pAd->Mlme.LastSendNULLpsmTime);
			pAd->RalinkCounters.RxCountSinceLastNULL = 0;
			MlmeSetPsmBit(pAd, PWR_SAVE);
			if (!(pAd->CommonCfg.bAPSDCapable && pAd->CommonCfg.APEdcaParm.bAPSDCapable))
			{
				RTMPSendNullFrame(pAd, pAd->CommonCfg.TxRate, FALSE);
			}
			else
			{
				RTMPSendNullFrame(pAd, pAd->CommonCfg.TxRate, TRUE);
			}
		}
	}
}

// IRQL = PASSIVE_LEVEL
// IRQL = DISPATCH_LEVEL
VOID MlmeSetPsmBit(
	IN PRTMP_ADAPTER pAd,
	IN USHORT psm)
{
	AUTO_RSP_CFG_STRUC csr4;

	pAd->StaCfg.Psm = psm;
	RTMP_IO_READ32(pAd, AUTO_RSP_CFG, &csr4.word);
	csr4.field.AckCtsPsmBit = (psm == PWR_SAVE)? 1:0;
	RTMP_IO_WRITE32(pAd, AUTO_RSP_CFG, csr4.word);

	DBGPRINT(RT_DEBUG_TRACE, ("MlmeSetPsmBit = %d\n", psm));
}

// IRQL = DISPATCH_LEVEL
VOID MlmeSetTxPreamble(
	IN PRTMP_ADAPTER pAd,
	IN USHORT TxPreamble)
{
	AUTO_RSP_CFG_STRUC csr4;

	//
	// Always use Long preamble before verifiation short preamble functionality works well.
	// Todo: remove the following line if short preamble functionality works
	//
	//TxPreamble = Rt802_11PreambleLong;

	RTMP_IO_READ32(pAd, AUTO_RSP_CFG, &csr4.word);
	if (TxPreamble == Rt802_11PreambleLong)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("MlmeSetTxPreamble (= LONG PREAMBLE)\n"));
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_SHORT_PREAMBLE_INUSED);
		csr4.field.AutoResponderPreamble = 0;
	}
	else
	{
		// NOTE: 1Mbps should always use long preamble
		DBGPRINT(RT_DEBUG_TRACE, ("MlmeSetTxPreamble (= SHORT PREAMBLE)\n"));
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_SHORT_PREAMBLE_INUSED);
		csr4.field.AutoResponderPreamble = 1;
	}

	RTMP_IO_WRITE32(pAd, AUTO_RSP_CFG, csr4.word);
}

/*
    ==========================================================================
    Description:
        Update basic rate bitmap
    ==========================================================================
 */

VOID UpdateBasicRateBitmap(
    IN  PRTMP_ADAPTER   pAdapter)
{
    INT  i, j;
                  /* 1  2  5.5, 11,  6,  9, 12, 18, 24, 36, 48,  54 */
    UCHAR rate[] = { 2, 4,  11, 22, 12, 18, 24, 36, 48, 72, 96, 108 };
    UCHAR *sup_p = pAdapter->CommonCfg.SupRate;
    UCHAR *ext_p = pAdapter->CommonCfg.ExtRate;
    ULONG bitmap = pAdapter->CommonCfg.BasicRateBitmap;


    /* if A mode, always use fix BasicRateBitMap */
    //if (pAdapter->CommonCfg.Channel == PHY_11A)
	if (pAdapter->CommonCfg.Channel > 14)
        pAdapter->CommonCfg.BasicRateBitmap = 0x150; /* 6, 12, 24M */
    /* End of if */

    if (pAdapter->CommonCfg.BasicRateBitmap > 4095)
    {
        /* (2 ^ MAX_LEN_OF_SUPPORTED_RATES) -1 */
        return;
    } /* End of if */

    for(i=0; i<MAX_LEN_OF_SUPPORTED_RATES; i++)
    {
        sup_p[i] &= 0x7f;
        ext_p[i] &= 0x7f;
    } /* End of for */

    for(i=0; i<MAX_LEN_OF_SUPPORTED_RATES; i++)
    {
        if (bitmap & (1 << i))
        {
            for(j=0; j<MAX_LEN_OF_SUPPORTED_RATES; j++)
            {
                if (sup_p[j] == rate[i])
                    sup_p[j] |= 0x80;
                /* End of if */
            } /* End of for */

            for(j=0; j<MAX_LEN_OF_SUPPORTED_RATES; j++)
            {
                if (ext_p[j] == rate[i])
                    ext_p[j] |= 0x80;
                /* End of if */
            } /* End of for */
        } /* End of if */
    } /* End of for */
} /* End of UpdateBasicRateBitmap */

// IRQL = PASSIVE_LEVEL
// IRQL = DISPATCH_LEVEL
// bLinkUp is to identify the inital link speed.
// TRUE indicates the rate update at linkup, we should not try to set the rate at 54Mbps.
VOID MlmeUpdateTxRates(
	IN PRTMP_ADAPTER 		pAd,
	IN 	BOOLEAN		 		bLinkUp,
	IN	UCHAR				apidx)
{
	int i, num;
	UCHAR Rate = RATE_6, MaxDesire = RATE_1, MaxSupport = RATE_1;
	UCHAR MinSupport = RATE_54;
	ULONG BasicRateBitmap = 0;
	UCHAR CurrBasicRate = RATE_1;
	UCHAR *pSupRate, SupRateLen, *pExtRate, ExtRateLen;
	PHTTRANSMIT_SETTING		pHtPhy = NULL;
	PHTTRANSMIT_SETTING		pMaxHtPhy = NULL;
	PHTTRANSMIT_SETTING		pMinHtPhy = NULL;
	BOOLEAN 				*auto_rate_cur_p;
	UCHAR					HtMcs = MCS_AUTO;

	// find max desired rate
	UpdateBasicRateBitmap(pAd);

	num = 0;
	auto_rate_cur_p = NULL;
	for (i=0; i<MAX_LEN_OF_SUPPORTED_RATES; i++)
	{
		switch (pAd->CommonCfg.DesireRate[i] & 0x7f)
		{
			case 2:  Rate = RATE_1;   num++;   break;
			case 4:  Rate = RATE_2;   num++;   break;
			case 11: Rate = RATE_5_5; num++;   break;
			case 22: Rate = RATE_11;  num++;   break;
			case 12: Rate = RATE_6;   num++;   break;
			case 18: Rate = RATE_9;   num++;   break;
			case 24: Rate = RATE_12;  num++;   break;
			case 36: Rate = RATE_18;  num++;   break;
			case 48: Rate = RATE_24;  num++;   break;
			case 72: Rate = RATE_36;  num++;   break;
			case 96: Rate = RATE_48;  num++;   break;
			case 108: Rate = RATE_54; num++;   break;
			//default: Rate = RATE_1;   break;
		}
		if (MaxDesire < Rate)  MaxDesire = Rate;
	}

//===========================================================================
//===========================================================================
	{
		pHtPhy 		= &pAd->StaCfg.HTPhyMode;
		pMaxHtPhy	= &pAd->StaCfg.MaxHTPhyMode;
		pMinHtPhy	= &pAd->StaCfg.MinHTPhyMode;

		auto_rate_cur_p = &pAd->StaCfg.bAutoTxRateSwitch;
		HtMcs 		= pAd->StaCfg.DesiredTransmitSetting.field.MCS;

		if ((pAd->StaCfg.BssType == BSS_ADHOC) &&
			(pAd->CommonCfg.PhyMode == PHY_11B) &&
			(MaxDesire > RATE_11))
		{
			MaxDesire = RATE_11;
		}
	}

	pAd->CommonCfg.MaxDesiredRate = MaxDesire;
	pMinHtPhy->word = 0;
	pMaxHtPhy->word = 0;
	pHtPhy->word = 0;

	// Auto rate switching is enabled only if more than one DESIRED RATES are
	// specified; otherwise disabled
	if (num <= 1)
	{
		*auto_rate_cur_p = FALSE;
	}
	else
	{
		*auto_rate_cur_p = TRUE;
	}

#if 1
	if (HtMcs != MCS_AUTO)
	{
		*auto_rate_cur_p = FALSE;
	}
	else
	{
		*auto_rate_cur_p = TRUE;
	}
#endif

	if ((ADHOC_ON(pAd) || INFRA_ON(pAd)) && (pAd->OpMode == OPMODE_STA))
	{
		pSupRate = &pAd->StaActive.SupRate[0];
		pExtRate = &pAd->StaActive.ExtRate[0];
		SupRateLen = pAd->StaActive.SupRateLen;
		ExtRateLen = pAd->StaActive.ExtRateLen;
	}
	else
	{
		pSupRate = &pAd->CommonCfg.SupRate[0];
		pExtRate = &pAd->CommonCfg.ExtRate[0];
		SupRateLen = pAd->CommonCfg.SupRateLen;
		ExtRateLen = pAd->CommonCfg.ExtRateLen;
	}

	// find max supported rate
	for (i=0; i<SupRateLen; i++)
	{
		switch (pSupRate[i] & 0x7f)
		{
			case 2:   Rate = RATE_1;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0001;	 break;
			case 4:   Rate = RATE_2;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0002;	 break;
			case 11:  Rate = RATE_5_5;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0004;	 break;
			case 22:  Rate = RATE_11;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0008;	 break;
			case 12:  Rate = RATE_6;	/*if (pSupRate[i] & 0x80)*/  BasicRateBitmap |= 0x0010;  break;
			case 18:  Rate = RATE_9;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0020;	 break;
			case 24:  Rate = RATE_12;	/*if (pSupRate[i] & 0x80)*/  BasicRateBitmap |= 0x0040;  break;
			case 36:  Rate = RATE_18;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0080;	 break;
			case 48:  Rate = RATE_24;	/*if (pSupRate[i] & 0x80)*/  BasicRateBitmap |= 0x0100;  break;
			case 72:  Rate = RATE_36;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0200;	 break;
			case 96:  Rate = RATE_48;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0400;	 break;
			case 108: Rate = RATE_54;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0800;	 break;
			default:  Rate = RATE_1;	break;
		}
		if (MaxSupport < Rate)	MaxSupport = Rate;

		if (MinSupport > Rate) MinSupport = Rate;
	}

	for (i=0; i<ExtRateLen; i++)
	{
		switch (pExtRate[i] & 0x7f)
		{
			case 2:   Rate = RATE_1;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0001;	 break;
			case 4:   Rate = RATE_2;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0002;	 break;
			case 11:  Rate = RATE_5_5;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0004;	 break;
			case 22:  Rate = RATE_11;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0008;	 break;
			case 12:  Rate = RATE_6;	/*if (pExtRate[i] & 0x80)*/  BasicRateBitmap |= 0x0010;  break;
			case 18:  Rate = RATE_9;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0020;	 break;
			case 24:  Rate = RATE_12;	/*if (pExtRate[i] & 0x80)*/  BasicRateBitmap |= 0x0040;  break;
			case 36:  Rate = RATE_18;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0080;	 break;
			case 48:  Rate = RATE_24;	/*if (pExtRate[i] & 0x80)*/  BasicRateBitmap |= 0x0100;  break;
			case 72:  Rate = RATE_36;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0200;	 break;
			case 96:  Rate = RATE_48;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0400;	 break;
			case 108: Rate = RATE_54;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0800;	 break;
			default:  Rate = RATE_1;	break;
		}
		if (MaxSupport < Rate)	MaxSupport = Rate;

		if (MinSupport > Rate) MinSupport = Rate;
	}

	RTMP_IO_WRITE32(pAd, LEGACY_BASIC_RATE, BasicRateBitmap);

	// calculate the exptected ACK rate for each TX rate. This info is used to caculate
	// the DURATION field of outgoing uniicast DATA/MGMT frame
	for (i=0; i<MAX_LEN_OF_SUPPORTED_RATES; i++)
	{
		if (BasicRateBitmap & (0x01 << i))
			CurrBasicRate = (UCHAR)i;
		pAd->CommonCfg.ExpectedACKRate[i] = CurrBasicRate;
	}

	DBGPRINT(RT_DEBUG_TRACE,("MlmeUpdateTxRates[MaxSupport = %d] = MaxDesire %d Mbps\n", RateIdToMbps[MaxSupport], RateIdToMbps[MaxDesire]));
	// max tx rate = min {max desire rate, max supported rate}
	if (MaxSupport < MaxDesire)
		pAd->CommonCfg.MaxTxRate = MaxSupport;
	else
		pAd->CommonCfg.MaxTxRate = MaxDesire;

	pAd->CommonCfg.MinTxRate = MinSupport;
	if (*auto_rate_cur_p)
	{
		short dbm = 0;

		dbm = pAd->StaCfg.RssiSample.AvgRssi0 - pAd->BbpRssiToDbmDelta;

		if (bLinkUp == TRUE)
			pAd->CommonCfg.TxRate = RATE_24;
		else
			pAd->CommonCfg.TxRate = pAd->CommonCfg.MaxTxRate;

		if (dbm < -75)
			pAd->CommonCfg.TxRate = RATE_11;
		else if (dbm < -70)
			pAd->CommonCfg.TxRate = RATE_24;

		// should never exceed MaxTxRate (consider 11B-only mode)
		if (pAd->CommonCfg.TxRate > pAd->CommonCfg.MaxTxRate)
			pAd->CommonCfg.TxRate = pAd->CommonCfg.MaxTxRate;

		pAd->CommonCfg.TxRateIndex = 0;
	}
	else
	{
		pAd->CommonCfg.TxRate = pAd->CommonCfg.MaxTxRate;
		pHtPhy->field.MCS	= (pAd->CommonCfg.MaxTxRate > 3) ? (pAd->CommonCfg.MaxTxRate - 4) : pAd->CommonCfg.MaxTxRate;
		pHtPhy->field.MODE	= (pAd->CommonCfg.MaxTxRate > 3) ? MODE_OFDM : MODE_CCK;

		pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.STBC	= pHtPhy->field.STBC;
		pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.ShortGI	= pHtPhy->field.ShortGI;
		pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.MCS		= pHtPhy->field.MCS;
		pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.MODE	= pHtPhy->field.MODE;
	}

	if (pAd->CommonCfg.TxRate <= RATE_11)
	{
		pMaxHtPhy->field.MODE = MODE_CCK;
		pMaxHtPhy->field.MCS = pAd->CommonCfg.TxRate;
		pMinHtPhy->field.MCS = pAd->CommonCfg.MinTxRate;
	}
	else
	{
		pMaxHtPhy->field.MODE = MODE_OFDM;
		pMaxHtPhy->field.MCS = OfdmRateToRxwiMCS[pAd->CommonCfg.TxRate];
		if (pAd->CommonCfg.MinTxRate >= RATE_6 && (pAd->CommonCfg.MinTxRate <= RATE_54))
			{pMinHtPhy->field.MCS = OfdmRateToRxwiMCS[pAd->CommonCfg.MinTxRate];}
		else
			{pMinHtPhy->field.MCS = pAd->CommonCfg.MinTxRate;}
	}

	pHtPhy->word = (pMaxHtPhy->word);
	if (bLinkUp && (pAd->OpMode == OPMODE_STA))
	{
			pAd->MacTab.Content[BSSID_WCID].HTPhyMode.word = pHtPhy->word;
			pAd->MacTab.Content[BSSID_WCID].MaxHTPhyMode.word = pMaxHtPhy->word;
			pAd->MacTab.Content[BSSID_WCID].MinHTPhyMode.word = pMinHtPhy->word;
	}
	else
	{
		switch (pAd->CommonCfg.PhyMode)
		{
			case PHY_11BG_MIXED:
			case PHY_11B:
			case PHY_11BGN_MIXED:
				pAd->CommonCfg.MlmeRate = RATE_1;
				pAd->CommonCfg.MlmeTransmit.field.MODE = MODE_CCK;
				pAd->CommonCfg.MlmeTransmit.field.MCS = RATE_1;
				pAd->CommonCfg.RtsRate = RATE_11;
				break;
			case PHY_11G:
			case PHY_11A:
			case PHY_11AGN_MIXED:
			case PHY_11GN_MIXED:
			case PHY_11N_2_4G:
			case PHY_11AN_MIXED:
			case PHY_11N_5G:
				pAd->CommonCfg.MlmeRate = RATE_6;
				pAd->CommonCfg.RtsRate = RATE_6;
				pAd->CommonCfg.MlmeTransmit.field.MODE = MODE_OFDM;
				pAd->CommonCfg.MlmeTransmit.field.MCS = OfdmRateToRxwiMCS[pAd->CommonCfg.MlmeRate];
				break;
			case PHY_11ABG_MIXED:
			case PHY_11ABGN_MIXED:
				if (pAd->CommonCfg.Channel <= 14)
				{
					pAd->CommonCfg.MlmeRate = RATE_1;
					pAd->CommonCfg.RtsRate = RATE_1;
					pAd->CommonCfg.MlmeTransmit.field.MODE = MODE_CCK;
					pAd->CommonCfg.MlmeTransmit.field.MCS = RATE_1;
				}
				else
				{
					pAd->CommonCfg.MlmeRate = RATE_6;
					pAd->CommonCfg.RtsRate = RATE_6;
					pAd->CommonCfg.MlmeTransmit.field.MODE = MODE_OFDM;
					pAd->CommonCfg.MlmeTransmit.field.MCS = OfdmRateToRxwiMCS[pAd->CommonCfg.MlmeRate];
				}
				break;
			default: // error
				pAd->CommonCfg.MlmeRate = RATE_6;
                        	pAd->CommonCfg.MlmeTransmit.field.MODE = MODE_OFDM;
				pAd->CommonCfg.MlmeTransmit.field.MCS = OfdmRateToRxwiMCS[pAd->CommonCfg.MlmeRate];
				pAd->CommonCfg.RtsRate = RATE_1/*
 **break/*
 }
		//***** Keep Basic Mlme ****.*************MacTab.Content[MCAST_WCID].HTPhyMode.word = *****************
 Transmitity,
/*
 if (Hsinchu County 302,
 * Taiwafield.MODE == k Te_OFDM)*
 *Inc.
 * 5F., No.36, Taiyuan St., Jhubei Ci RalinkCS = Ofdm****ToRxwiMCS[*****24]/*
 else
 *
 * This program is free software; you can redistribute********************************
 *******Hsinchu County 302,****;
	}

	DBGPRINT(RT_DEBUG_TRACE, ("***
 Updnd/ox****s (MaxDesire=%d, MaxSupport        versio      in*
 *     *****Switching =%d)\n",*
 *     IdToMbps[         ]                             *
 * This prHsinchu County 3    *
 *istributed in the hope that it winl be use     /*OPSTATUS_TEST_FLAG*
 *, fOP_ even thX_*****SWITCH_ENABLED)*/*auto_rate_cur_p)); or     *
 * (at your option) any later version.   *
 *     *******     *********Bitmap=0x%04lx                       ***************l be useful,       *
 * but WITHOUT A*******] more details.    PARTICULAR PURPOSE.  See the   any later version.  02,
 * Taiw    x      Jhubei C=
 * aaxng with th *
 * ram; if not, w          Hsinchu County 302,
 * Taiwan, R,       * 5F., No.36, BSSIDan St.,along with tity,
 ,ation, Inc.,                     ogram; if n       *
 * 59 Temple Place - Suite 330 Jhubei City,
  PAR}

/*
	=                                         *
 *****************************
	Description:
		This func**** uater  HT      settingink Input Wcid value is
	Abid for 2 case ****1. it's useRevisiSta*****i----fra mode that copy AP NESS to Mactabllink 2. ORhat
	----	in adhoc-------o------peer's****------------------
	IRQL = DISPARCHALEVEL
                                           *
 ******************************* */
VOID any later Htversion(
	IN PRTMP_ADAPTER he Fr,0, 0	UCHAR
 **apidx)
{
{0x00, StbcMcs; //j, UCHAR	R, bs.  s****x00, 	iSN_O 3*3
	RT_HT_CAPABILITY 	*pRtHtCap = NULL;E_INFO_EPHY_INFO		*pActiveHtPhy 0xf2, 0x0ULONG		FoundaCSM[] ac};
j00, 0x0f, 0xP2, 0x00, 0x01};
	p      WME_PARM_ELEM[]PHTTRANSMIT_SETTI{0x0p {0x00, 0x40, 0x96, 0x04};
UCHAR   RA, BoINK_OUI[]  = {0x00, 0x0c, 0x43};
UCHAin  BROADCOM_OUI[BOOLEAN 
 **or FITNESS FOR A;, or     *
 * (at your optithe GNU Gene{0x00, 0x===> \n" PAR
	r FITNESS FOR A 0xf2, 0x
	{the fo[] = {0x0	= &*****Sta****      d  BROInfo the FAR   WME_P  TrainUp   TrainDown		// Mode- Bit0:  BROA	  TrainUp   Trai Jhubei C the AR   BRO  TrainUp   Trai, Boston, MA0x00, ;
UCHAR  0,						// Initlong with t.   hTable[] = {
// IterainUp   TraibAutoversio      ense, o.
 *(ADHOC_ON*
 *) || INFRAx03, 0x0) && *
 * (Opei CchnoOPlogy,STA))  Mode.
 *
 * (Sta STBC,.       ed Mode- .bHtEn----chnoFALSE.
 *
return40, 10, 0x50, 0xrainUp   0,
    0x06, 0x2  BROBit0: STBC, Bit18, 0x21,  4, 15, 30,
    0x0 Mode- Bit0UCHAR	R = (f2, 0)ation,lmeAux.AddHt 2, 20, 12,  13.UCHAR	RS
x00, 0x50, =    0x0c, 0x20, 12,  15MCSSet[0]+*
 * ( 0x0e, 0x20, 14,  8, 20,
1]<<8)+(UCHAR	R<<16PART, 45,
***************nDown		// Mo.TxSTBC  0x04,, 0x50,->R, 0x00,  0, ****Ao.36na redistTxPathchno2x05,00,  0,  0,-> Ralin 0x0 =  0x0_USE under the te,  0,
    0x15, 0x00,  0,  0NON 0,
}
der the 0x21,  1fo[] = {0x0->0, 50,
    0x07, 0x21,  3, 15, 50,
    0x08, 0x21, 00,  0,  0,  0,
    0xb, 0x21,  7,  8, 25,
    0**********0, 1T,  15, 30,
    0x0d, 0x20, 13,  8, 20           *
 * 0,  0,  0,8, 20,
    0x0f, 0x00,  0,  0,  0,
    0x1f10, 0x22, 15,  8, 25,
    0x11, 0x00,  0,  0,  0,
    0x12, 0x00,  0,  0,  0,
    0x13, 0x00,  0,  0,  0,
    0x14, 0x00,  0,  0,  0,
    0x15, 0x00,  0,  0,  0,
    0x16, 0x00,  0,  0,  0,
    0x17, 0x00,  0,
****Decide MAX hty fro.5, 45,
  0,  0,
 GF  0x04, 0x200,  0,  0,  0,
    0x1GF  0,  0x00,  0,  0,  0k Techology,HTGREENFIELD;,  0,  02, 0x00,  2, 35, 45,
    0x03, 0MIX;

      0x11, 0x00,  0,  0,  0,
    0x1ChannelWidth0,  0,  0,  0,
  TrainDown		/0x02, 0x00,  2, 35, 4BW = BW_400, 45,
};

UCHAR RateSwitch0:CCK, 120/ Item No. Mix, 3:HT GF)
    0x00a, 0x00x02, 0x00,  2, 35, 4ShortGI,  81, 0x00,  0,  0,  0,
    0x1   0x00for20 &   0,  0,
 x00,  1, 40,PART45,
};

UCHAR RateSwitch   0x00, 0x00,  0, 40, 101,
    0x01, 0x00,  1, 44, 50,
    0x02, 0x00,  24 35,
	visi(i=23; i>=0; i--)AR   WME_Modej = i/8,
  , 0x0f,,  81<<(i-(j*8)Switch   0x11,AR   WME_P->8, 20,
j] &0, 0x0f,  0x04,8, 0x00,  0, ,
};

UCHAR RateSwit 0, Mode2, 0x00,  2, 35, 45ibutei/*
 ************0x09, 0i==em afe- Bit0: em afteC----MINion
    0  rt2860???
after asso GF)
    0x0a, 0x00,T GF)
    0x08, 0xown		/0,  0,						// Initia0x00,   item after associatio  0x00, 00,  //If STA assigns fixedFDM, 2:*******to,
    0her 0x00, 004, 0x21,  0, 30, 50,
    0xchTable11G[] = {
// Item N0] != 0xffx05, 0x21,  1e11G[] = {
// Item N4 0x10,urr-MCS   TrainUp   TrainDown		/32/*
 *0,						// Initial usedHAR RatCHAR	PRE_N_HT_OUI[]	= {0x00, 0x90, 0x4c};

UC<=== Use F    0own		/%d    0,						// Initial u PARTSTBC, 4, 10, 25,
( 25,
i >0x01x07, 0x10,  5,-MCS   25,
    0xx08, 0x10,  6, 10, 25,
    	x09, 0able11G[] = {
// Item No.   Mode   C 0x04, ,  7, 10, 13,
};

UCHAR RateSwit 0,  MCS    TrainUp   TrainDown		// ModeteSwitchTable11N1S[] = {1,  3, ************** Bit1: Short G*************em after assocon
    
FDM, 2:  0x15, 0x00,   0x00,  0,  0,  0,
  ,  0
    0x08, 0x00,  Mix, 3:HT GF)
    0 7,  8, 14,
};

U5,
    , 0x00,  2, 35, 45,
 S[] = {
// Item No
    0TrainUp   TrainDown 7,  8, 14,
};

U01,
    0x 0x03, 0x00,  3, 20, 45,0,  //	Whe default now.HT Mix,5, 45,  4, 10, 25,
    0x05, 0x10,  5,
, 0x50, 0xf2, 0x04 0x07, 05, 45,
   iation
    0x00, 0xTRUE};
UCHAR	PRE_N_HT_OUI[]	= {0on) any later TrainDown	---.AMsduSiz****%d Ratendation00,  0,  0,  0,
    0x1Am 50,
    PARTICULAR PURPOSE.  See the("TX:0: S5, 0= %x (choose %d), 0:CCK    01,
    0x     ,
        , 0x21, ,  7, 10, 13,
};

U0],  TrainDown		// MY; w  8, 14,
};

UCH, rt GI, Bit4,5: Mode(0 No.	Mode	Curr-M, 25 30,
    0x06, 0x20, 13,  8, 20TrainUp   TrainDown		// ateSwit    / se
	John Chang	2004-09	CISCO_OURadioOff40, 0x96};

UCHAR	WP 0x02, 0RT28XX_MLME_RADIO_OFF, 0x0:HT Mix, 3:HT GF)
    0x0a, 0x00,  0,  0,  0,n    // Initial used item after association
    3, 0x0:HT Mix,                                         *
 ************************************************// bss_------c   0x04, 0x21,  4, 15, 30,
    0x05, 0x20, 12,  15, 30,
    0x06, 0x20, 13,  8, 20,
    0x07, 0x    ! \brief initial
   BSS -----
 *	\param p,  8 pointer2, 2the Curr-MCS   3, 15 non-MCS   r-MCS   ost

, 3:HT GPASSIVE2004-09, 3:HT GF)
    0x0a, 0xUCHAR	CISCBssT----Init40, 0xBSS_TABIL *Tabm aftint,  4
	Tab->BssNr 0x01,tem sociatioOverlapn
    0x, 1:OFD 0x01 i <ciat_LEN_OF_/ Initial; i++05, 0x2NdisZeroMemory(&sociatioEntry[i], sizeof(/ InENTRYtem af1,  3, 15, 50,
 .Rss0x21-127;F)
 m No.  // Mor  5,as a minimum
	Abstx06,}
0x09,  A  0,  0,						/ Initial used ite,x00, 		//Anitial used item after associanumAsOriginato1,
    00a, 0x22, Recipient 0x01, 0,
 Alloc2, 0pinLock(rainUp 8, 2 3*3em ax01, 0x21,  1, 20, 50,
    0A_REC2, 0x21,  2, 20, 5sociatARecx05, 0x21,ECinDoat
	u7,  witchTabl 0x00,  ForABand[] = { // 3*3
/(BC, Bit1: Short GI, xReRing   Moem a}e   Curr-MCS   TrainUp   TrainDoORI	// Mode- Bit0: STBC, BitOrix05, 0x2100, 4,5: Mode(0:15,  8, 25 0x00,  0,      {
// Itesearch// Moe   Curr- by     MCS   TrainUp   TrainDown		// Mobss Curr-MCS   Trainssid04, 0 stringit0: STBC, Bindex of// Mode- B,// InNOT_FOUND No.not----/ Mode- Bit0: t GI, Bit4,5:CS  no

	Mo1,  3by sequenx06,   0x0a: Mode(0:C3:HT GF)
    0x0b, 0 = {0  0,  0, S 0x0a					// Initial used[] =  P0x00,  pB6, 0TrainU   Trai TrainDf2, 0x01};ter asx01, 0x21,  1, 2sociation
1,  2, 20, 5********Some-----		--s       A/B/G-------		--may	When	/ Mosam15, 5ID on 11A and 11B/Gink Te We should disule uis 3, isn Hisink Tech  45,
(, 0x09, 0x05, 0x21 TrainD <= 14101,
  2, 35, 45,
  00,      0,
    0x02, 0x00,  2, 35, >,
    0x03, 0x00,/50
    0xx05,MAC_ADDR_EQUAL,
    0x02, 0x00, Down		inDown	Curr-MCS  
    0x0    0x06,,
    0x( = {0),
    0x08, 0:HT MS[] = {
/Ssid/ Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mop   Train  0x	// Mode- Bit  0xLen	// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0d, 0x00,  0,  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101,
    0x01, 0x00,  1, 40, 50,
    0x02, 0x00,  2, 35, 45,
    0x03, 0x00,  3, 20, 45,
    0x04, 0x21,  0, 30,101,	//50
    0x05, 0x21,  1, 20, 50,
    0x06, 0x21,  2, 20, 50,
    0x07, 0x21,  30, 50,
      0x21,,  8, AR RateSw0:CCK, 1:x05, 0x21, 15, x06, 0x20, 13,  8, 20Len3, 15, 50,
    0x08, 0x21,  4, 15, 30,
    0x09, 0x21,  5, 10, 25,
  / Item No. With aft  Mode   Curr-MCS   TrainUp   Trai14,
	0x0c, 0x23,  7,  8, 14,
};

UCHAR RateSwitchTable11BGN2S[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDow40, 50,
    0x02, 0x00,  2, 35, 45,
    0x03, 0x00,  3, 20, 45,
   0x04, 0x21,  0, 30,101,	//50
    0x05, 0x21,  1, 20, 50,
    0x06, 0x21,b, 0x09,0, 50,
    0x07,20,
, 30,
    0x(05, 0x20, 12, 15, 30,
    0x06, 0x20, 13,  8, 20,
    0x07, 0x20, 14,  8, 2fter ass0,
 Equalx03, 0x2, 15,    0, 15, 30,
    after ass 21,  8, 20,
   x06, 0x20, 13,  8, 20,
0x20, 22,     0x07, 0x20, 14,  8, 203, 15, 50,
    0x08, 0x21,  4, 15, 30,
    0x09, 0x21,  5, 1ix, 3:HT GF)
    0x0a, 0x00,  HAR RateDelete5, 5040, 0xOUT	   Curr-MCS   Train		p   TraiinDown		// M	{0x00,  t0: STBC, Bit1: Shor, j0,  4, 10,(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
  
    0x02, 0x00,  2, 35, ==t0: STBC,, 0x21,     0x06, 0x21,  2, 20, 50,
    0x07, 0x21,  3,urr-MCS  4, 10 25,
; je(0:CCK, 1:OF - 1; j 2, 20x21,  20,
 Movex03, 0x20x21,  0, 30,10j]), 
    0x06, 0x20,  + 112,  0x04, 0x21,  4, 15, 3 0x05,0,
    0x03, 0x20x21,  0, 30,100x21,  4, 15, 0, 13,  8, 20,
    0x08, 0x0x21,  4, 15= ****** 3, 15,   0x06,                                                *
 ****************************
	Routine *************** Shortl use15,  8, 25,5, 50----BA------ Or decreHistx22, de(0:CCK, 1:by 1x20, eedede baArguments:GF)
 se
	John Chang	2004-09 // 3*3
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0, 0x09,  8, 20, ShortORI GI, Bit4,5: Mo 0x20, 14,  8, 20,
   0x 0x00, ,  4,	*pBAx21,  3,m af 0,    21,  5, 15->1, 20, 50,
   ! 0x02, 0x21,  2, , 20, 50,
 Acquir { // 3*3
// Item No.   Mode 
    0x06, 0x21, 12, 15, 30,
   30, 02, 0x21, Doneurr-MCS    Item No.le.x22, 15,  8, 25,itchTabler     *
 * (at your option),
    0x03, 0x21,  3,0x0c, ,  8, 25,
= %l Bit1        8, 25,
};

witchTabltem aft// ErHistls.    fla Name****ation, Inc.,        0x06, 0x21, 1e.c
].TXBA, 0x0p &= (~ 6, 10x06, 0x21, 1TID), 30em af0,  1,----,  e */	  lea,
   / has left",
	/* 4  */	 "DIS-ASSOC due to  0x01e.c
Array	 "DIS-ASSOC dTID7, 00le all associations",
	/* 6  */	 "clas2,  8, 20,
    0x0a, 0x20, 0x02, 0x21,  2, 20,e assoc/re-assToken tchTabl// Not clear S2, 15ce,
    0x14,  Rel    { // 3*3
// Item No.   Mode  50,
    0x03, MCS   Traiit0: STBC,  Short GI, Bit4,5: Mode(0:C3:HT GF)
    0x0b, 0x09,  0,5, 50Se  0x08, 0x20, 14,  8_OUI[] OUT// In,  4, *nDowTrainUp   TrinDown		// Mo*/
};  0x[TY;  Mode- BAR RateSwitchTable1BC, BypeeToRxwiSHORT BeaconPerio	0x0c, 0CF_PARM pCfParm RX sensibiliAtimWiSwitchTasibiliCapabilitode- eToRxwiMCS[];up****ateToRxwiMCS[];peer m since RT61 hasExter may not be ableus downteSwitchTO_ELEM[]  = {_IE *p0x50,.
// oteToRxwADDFO_Ex01}12]			0, 12,  1,	 "RAP might    0  0x0addi****al 8, info IEToRxwiMCS[				= {0xfffff0teSwitchTable11		0, 12,  1ff01f /* 6 */	 , NewExt TraOffseteToRxwiMCS[] TrainDrn UCHAR	 O,  5eToRxwLARGE_INTEGER TimeStamp , 0xfffff0ffkipFlag0x0c, 0EDCATX ACK Edcate not to PQOSicRateMask[1X ACK Qos {0xfffff001 /* PQ/ InLOADAR MULTIbssLoa, 14,
};sibiliLengthVIEC_ADDR_NDIS_802_11_VARIABIL_IEs pVIEm aftCOPY_    0x06(nDow, 0x27, 0x21,  31, 0x D0a, 0x0Hiddenx20, 1to be 1, 2, it willLEN], 15ial ox07, 0 afDowncop    4, 0x	0xff, ADDR[MAtchTab.
 *4,  8,  >    0 Mix,  For hDDR[MAC_ADDAP{0x00, 0xffsend by, we withx20, 1len e 8, 0x000*/	 "ROrn
//		this va/probe responsealue, then it'sma       real then it'gthY; w20, utx20, 1 /* ll zero. such,  1"00-2	18	 24"alue, 						 4  0x00, 0xhav 2, 2prevable  0x01, 0 overwrite correct Curr-M0x20,  21,  8, 20,
   0x07, 0x20, 22,  8, 20,
, 23   0x07, tem 0,
    0x03, 0x0xff, , 15, 0, 50,
     aftem aft21,  5, 15, 30, 1, 2, 5, 11, 22,  8, 20,
R Rate1, 2, 5, 36]"=AR RateSps[] = { 2,eLevelFor   00x21,  45,
};

 { 2, 4, 11, 220,  0xff, 0xftterCCK,
UCHAR= IE_SSID;y, we have CCK,y, we have xRate[R;
UCHAR  =// In3, 2005, 0x21,  1rate n->bV

	Rurr-MCS   xff, CfpCouble11IE_HT_CAP= IE_ADDps[] = { 2,CfpES;
UCHAR;
UCHAR  NewateIe = ItInfoIe = I    ur
	----H_OFFSET;
UCHAUCHAR  DsIeIe	 = IE_ERP;
DurRemain     _OFFSET;
UCHA
UCHAR  WpaI, 108, 144, 200}Mode xff, our nor = our nort4,5: MnfoIe =te.
// otherw =rate.
// otherwCHAR ZThe privacy0, 0x07,i] =  security 2  ONreatera 0xffbe WEP, TKIP or AES Mode(0mbC, Blue, Authei C,l usy0, 0x0d assoc/ Moconne******methods.  Ccx2IePCHAR	SE= CAP_IS_PRIVACY3, 15cx2Ie	 = IE_CCX_V2;bps[ASSERT( to receiv 45,, 6, 9, 12, UPPORTED  *
 Sbps[te[RA,0x00,0x00,0x00,0x00};

// Reset the RFIvalid I8, 54, 72, 100};
Upeer m,e to rec3(TX0~4=000Kbps[er the [] = {
//		ch	 R1 		 R2 		 R3(TX0~4=0) 00,0x00};

// Reset the RFIC sR1 		 R2 		 R11, 22,a55, 0x980,0x00,0x0 TX rate
U0,0x00,0x00};

// Reset the RFIC s[] = {
//		ch	 R1 		 us down,a TX rat 0x98168a00Kbps[21,  5, 15, 30,
, 36, 4= {0xfffff00No.	4c078e, 0x98					  0xfffff09f},
		{5,  0x98402ecc, 0xf003 /* 2  ite8a55, 0x90xfffff03f /86, 0x9816ffff07f /* 12 */ 0xffff07f /* 12 */6, 0x9816ecc, 0x984c=a TX rate
U6, 0x9816//50
    ff /* 18168a55, 0xentralx9800519f},
		{9,  0x98402,  5, 1,  5CHAR Zlater  */	  , 0.x20, 14,existsx00,0
	Abstract0x0  Ccx2Ie	/	  , 09f},/	  , 0T GF)
 Newevisimicrosofte- Bit0IEs		{4,  0x98402ecc, 0x98FixIEs.	 , sxffff &	 , 0xffff 886, 0x9816a, 0x98  ExtRInDowva19f}  ExtRateIe = I0x98402ecc, 0	 = IE_CCie*/	 xtern UCHAR	WPA_, 0x98168a55, 0x9800518Vari
    {12, .
 * 0x00};
U5,
    0R  IbssIe	VarIE0x984c 0x00};
U:HT Mix,8, 54, 72, 100};
84c09s,N] =  0x21,x984c099a,al used;
UCHAR  IbssIe	84c099a, 0x   0AR  Ccx2Ie0xfffff03f /Ie	 = IE_SSID					  0xfffff0ec8, 0HyperL					  0xfffff0 means if 183},
		{44, 0x98402ec8,					  0xfffff0 0x980ed1a3},
		{38ecc, 0x984c078e, 0x98168a55, 0x9800518b},
		{6,  0x9840yperL0x98158a55, 0 meansAddHtInfoIe 0x98158a55, 0x90xfffff03f /24, 36, 48, 54, 72, e, 0x98168a55, 0x9800519f},
		{7,  0x98402ecc
	x00,  0x11,  12,  14, 1ntrox984c > 2)0x04, 08e, 0x9810, 12,  1592, 0x98168a55,= EXTCHA_BELOW101,
  68a55, 0x980,
		{44 2, 2 TrainDown		 used i4021, x00,{d193},0x98402ecc, 0x984c0796, c068e, 0x98158a55, 0x980- AR Rx00,} Plugfer t No.   Mo60, 0x98402ec8, 0x984c0692, 0x98158a55, 0x9ABOVE83},
		{62, 0x98402ec8, 0x984c0692, 0x98158a55, 0x980ed1 0x05, {64, 0x98402ec8, 0x984c0692, 0x98158a55, 0x980ed1a3}+ // P 0x20, 0x06, 0BssCipherParsex00,08402e// n168a55,QOS 0,     , 0xfffable[] = {
//		ch	 ecc, 0x , 0xffff*/ , 0xffff13,  8, ff /* 48 ial u, 200};

UCHA , 0xfff.;
UCHA 0x21,  0, .
 *
ICAST_ADDR[MAnd solution.
		{102, 0x984ICAST_ADDR[MACLTICAST_ADDR[MAC13,  8, * 54 */};

UCHAR MUd793},
		{104, 0x9ICAST_ADDR[MA 0x985c06b2, 0x98578a55,0, 0x00,
		{108, 0x98402ecc, 0x9850, 0x00,0x00, 0x00,},

		{11EN] = {0x1,  84c0a36, 0x98178a550, 0x00 0x985c06b2, 0x988402ecPEID_STRUCTtem  pEiR.O.C, 0x00,20, 0x984 0x00}c8, 0x9, 50,
    0x03, 0x200,0x0WpaIE.IE
UCH		{2,CUSTOM50,
bps[], 0x98178a55, 0x980ed1Rsn,
		{124, 0x98402ec4, 0x984d1a3E5c06b( 0x98178a55)N] = 0x984while (Lan 2
	ve t + (, 0x00)c038->, -40<0x98158a55, 0x984c06s     (equireEHAR  Ax21,  2 HistIE_WPA****8, 0x98 21,  8, 20,
    quireOctet, WPA_OUI, 4
		// 2x21,  2, 0x984cquired bve t) > 0x98402ec4, 0x980ed18b980ed18ba3},
		83},
		{5, 0x980edc4, 0x, 30,
     0x20, 180ed193},
		{136, 0x984{134, 0x984022ec4, 0x80ed1a3},
		{38, 0x9883},
		{x9857, 0x21,},
		{136, 0xbps[] 98178a55,*******20, 0x98408a55, 98178a55RSN:8a, 0x98178a55, 0tem No. 
		{132, 0x98402ec4, 0x984cve t, RSN 0x98138a55, 0x980ed18b},
		{134, 0x98402ec4, 0x984c0386, 0x98178a55, 0x980ed193},ed193},
 0x98402ec4, 0x984c0386, 0x98178a55, 0x980ee, 0x98178a55, 0x98402ec4, 0x984c038a, 0x98178a55, 0x980ed193},

		// 802.11 Ue, 0x98178a08, 0x20, 1x984c038a, 0x98178a50x05,2ec4, 0x980ed1bb->0x980ed15b required b; AR  Eid[1] +, 0x[1]+0,0x.36, Len]e	 = 0382, 0x98178a55, 0(8, 25,*requib->0x98{134, 0x9ode(0:C */	 "4-w 8, 2// Item sert an cc, , 2:, 0x21,  5, 15, 30,
    0x0Up   T];

e   Curr-MCS   Train0xff,em aft30,
    0x06, 0x20, 30,
    0x06, 0_it's80ed1bbof04, 0x21,  4, 15 14, yp-MCS   Trainthis v_phave MCS   Traint68a55, 0MCS   TrainUpcdshake time atim_wi16 */	 Trainca09be55, 0x95NESSs82, 0x9509be55, 002c2ccc, 0x9500TrainD_idxit0: STBC, Bit1: Short GI, Bit4,5:8, 25,
  l as1   2  id15, cala55, 0old2ccc, 0, 0x00, replaced0,  5, 0 thet1: 

UCHAR RateSwitchTable11BGN1S[] = {
// Item t GI, Bit4,    0x04, 0x21mong AssosReq/Rurr-MCS   TrainUp   Tr;

extern UCHAR	 OfdmRateToRxwiMCS[];
// since RT61 has better RX sensibility, we have to limt TX ACK*rate not to exceed our normal data TX rate.
// otherwise the WLAN peer may not be able to receive the ACK thus downgrade its data TX rate
ULONG BasicRateMask[12]				= {0xfffff001 /* 1-Mbps */, 0xfffff003 /* 2 Mbps */, 0xfffff007 /* 5.5 */, 0xfffff00f /* 11 */,
									  0xfffff01f /* 6 */	 , 0xfffff03f /* 9 */	  , 0xfffff07f /* 12 */ , 0xfffff0ff /* 18Nwise th								  0xfffff1ff /* 24 */	 , 0xfffff3ff /* 36 */	  , 0xfffff7ff /* 48 */ , 0xffffffff /* 54 */};

UCHAR MULTICAST_ADDR[MAC_ADDR_LEN] = {0x1,  0x00, 0x00, 0x00, 0x00, 0x00};
UCHAR BROADCAST_ADDR[MAC_ADDR_LEN] = {0xff = {0xIdx0x98Idx  SupRaRateSwitchTable11BG   TinDown		 T RateIdTo500K,2,  2},
	{IC settie (quTES;
U  0x08, 005, 0x21,  sociation
 >0x00,0x00};

/ Initialed19   MCS  *****em af	// Inhappen when{196, spin wa****llnvalOUI[];

dDown		s */, 0x0 14,be* 5.ed0x9500================= 8, 0x01, 0, No.we founal useeInit(
	IN t===={  -92, -9=========S_STATUx05, 0(!ut even the implied warranty of    MEDIAy of E_CONNECTED02, 0x21,  2.
 *    0x06, 0x21,0x0f, 0x20, 11,  4, 15, 30,
 45,
 0x05, 0x20, 12x0f, 0x20, 1, 15, sAllocateSpinLock= PAS, 22,  8, 20,
 	// 2008.		re (que0x21,  0, 30, 1012ec4, 0*/	 "4-way h warr21,  3, 15, 50,Idx]te machine, 15, 30,
    0 better ity, we have t rate no our normalchineate.
// otherwi(TX0~4=0) R4
		{1,   0x98168a55, 0x980051HAR a32, 0x98578a00519f},
						  0xfffff010ed193},
		{5MachineIffff07f /* 12 */ 2,  2},
	{1				  /	 , 0xffff */	  , 0x985c06b2, 0xa55, 0x980ed193} 0x98402ecc 0x00};
UCN] = {38a, 0x98178a55, 000, 0x21,  0, 30, 101,
 S_SUCCESS, 30, 101++) %20, 50,
    0x02, 0x21e EVM val0,
    0xtruct402ec4, er the t, 50,
    0x 0x09, 0x21,  5EVM value
ScanTab);

			/STA st machines
			AssocStateMachineInit(pAd, &pAd->Mlme.AssocMachine, pAd->Mlme.AssocFunc);
			AuthStateMachinenit(pAd, &pAd->Mlme.AuthMachine, pAd->Mlme.AuthFunc);
			AuthRspStateMachineInit(pAd, &pAd->Mlme.AuthRspMachine, pAd->Mme.AuthRspFunc);
			SyncStateMachineInit(pAd, &pAd->Mlme.SyncMachine, pAd->Mlme.SyncFunc);
			WpaPskStateMachine
UCHAR RateS++,  0,  0,  0,
  /* avoid _ADDR[MAC_ADDfor{212, 0x0x00{  -9i2, -91, -90, ->Mlmerom  in 36 mbps TX * 1, 40, 505, 0x20, 1, 15, 30,
    0x06, 0x20, 13,it(p8, 20,
    0x07, 0x20lme.RxAnt20,
    0x08, 0x20, 23,  8, 25,
    0x09, 0lme.RxAntEv 8, 25,
};

UCHAR RateSwIMER_FUNCTION( 0x984c0 machines
			AssocStateMachineInit(pAd, &pAd->Mlme.AssocMachine, pAd->Mlme.AssocFunCntlInit(pAd, &pAd->Mlme.CntlMachine, NULL);
		}

		ActionStateMachineInit(pAd, &pAd->Mlme.ActMachine, pAd->Mlme.ActFunc);

		// Init mlme periodic timer
		RTMPInitTimer(pAd, &pAd->Mlme.PeriodicTimer, GET_TIMER_FUNCTION(MlmePeriodicExec), pAd, TRUE);

		/0x06, 0>Mlme.Aironep   TrainDown		// Mode- Bit0: STBC, Bit1:  0xSor						****************/
	// ISM : 2.4 to 2Out:OFDM, 2:AR	 OfdmRateToRxxf2, 0xFUNCTION2, 0INTx08,  0,  0,  0,		======10,  4, 10,x21,  1, 21, 20,can5F., 1:OFDM, 2:HT Mixeq/Rsp/BeacoInBs*/	 rainUp e has to b5, 50,
 20, 1] = {0	bIsADDR[MApInclupAd)cc, 0x984c0GF)
   ***************bIEEE80211H
		a11,  1, &pAd->MlmeisAllocateSpin0,101,	//50
    8a, 0x98178a55Radar TrainDCheck		Assoare so 0x980051))ntee only one 0x984c06.
 *
=======ADDR[M5, 10, nction is invoked fro1, 20,(0:CCK,0, 0x0=======T_SUPP_RATEainUp   TraiupRateI3, 0x21,  3, 20, 50,, 15, 30,
    0
	MLME_QLock(&peer side, n 0x06,  ID MlmeHandler(
	IN21,  3, 15eq/Rsp/BeacoOut someth======, 0x21,  2,sAcquireSpiNr]0x984 for2.4G/5G N onlyciatix05, 0x2==========	{44, 0x98402ec8};

0, 50,
	tion and MPReceivhubei Cd->M0, 011N_2_4G00,  k);
		return;
	}
	else
	{
		pAd->Ml5GG(pAd,55, 0x9    0x06, 0x20, 13,  8, 20  1,of(Rn N-me.bR00,0x00,iss */don't RssiSHt0049.
// ot 2:HTy, we.ateSwit, 0x9ontinutem aSTBC,  0x98168a55,WPA2||
			R=====EQUEN0,0x-----firstx05, 0x2ainUp   Trai0,0x00,0	===0,
 ST_ADD0,0x00,0WP  Htlme.Taskd, fRTMP_EST_FLAG(iati0,0x00,0Aux8a55, an envi,&pAd HistAP 0x00,  0dual-unning)
 0x984c06			RTMP_TEST_FLAG(!= other (c0,0x00,0  0x04, 0x2queue num = %ld)\n", pAd->Mlme.QueuAux8a55, 0x "Resneice HaedachineT_IN_PROGRST))
		{
			DBc chan sui8a55 */,usTMP_TESmor0x00, 0edeDequeuetx980st
	----Module  MlmeHandler! (queue num = %ld)=Ad, fRTMP_ADAPTER_NIC_NOunning = TSET_CONF)
			{
				DBGPRINT_RAW(RT_DEBUGPSKleInit(&pAd->S all aho			 14,m    0iationw0x00,  0,me.bRlet-> MLpass00,0x0 used iteencry*****980ed1a3},other (cWPA.bMixlse
	{
	07, 0x21, GRESS) ||
			RTMP_TWep30,
    0xf // RT2870 Groupr chanx98178a5I should drive
	teMacRTMP_gSTA eDequeu			}
#endif // RT2870  STA state )\n", fRTMP_AD STA WEP4050,
  de.TaskLo=====PerformAction(pAd, &pAd->Mlme.AssocMachine, E104m);
					break;
				c		switch (Elem->Machi<)
			{
				// STA state machines				case ASSOC_STATE_MACHpairwiseeDequeu, skipx20, 1 state machine I all aprof// 08a55tox00,,;
			item->MsgLenout ques****S_STAspMachine, Elem);
					2] =Occupe.Quefin FALstatee machine IHandler! (queue nlem->Machi			DBGPRINT_REontinue;
2m);
			me.TaskLo		
			switch (Elem->Machine)
			{
				//Pairstate meCntlMachinePerformAction(pAd, &pAd->Mlme.CntlMachine, Ele determine ET_IN_PROGRESS, &pAd-Day4, change "!!! reset MLME state machine !!!\n"));
		2_TRACE, ("!!! reset MLME state machine !!!\n"));
		2		MlmeRestartStateMachine(pAd);
				Elem->Occupied = FALSE;
				Elem->MsgLen = 0;
				continue;
			}
#endif // RT28702 //

			// if dequeue success
			switch (Elem->Machine)
			{
				/2/ STA state machines
				case ASSOC_STATE_MACHINE:
					StateMachinePerformActiRT_DEBUG_TRAC->Mlme.AssocMachine, Elem);
					break;
				case AUTH_S);
					break;
			} // end of switcformAction(pAd, &pAd->Mlme.AuthMachine, Elem);
					break;RT_DEBUG_TRACE, ("ERRTATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.AuthRspMachine, Elem);
					break;
				case SYNC_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.SyncMachine, Elem);
					break;
				case MLME_CNTL_STATE_MACHINE:
					MlmeCntlMachinePerformAction(pAd, &pAd->Mlme.CntlM2achine, Elem);
					break;
				case WPA_PSK_STATE_MACHINE:
eters:
		AdaachinePerformAction(pAd, &pAd-> &pAd//ine, CHAR ate mac,NTV);
ate macS_STATUSWx00,llTE_MACHwep			iusevisi 8, if = {****Bsshine, pA  0,  		switch (Elem->Machine)
			{
			em->Mach->Mlme.TaskLock);

	while (!MlmeQueueEreak;
				case      , // 0
{
	BOOLEAN 	  Ca=, Bit1Mlme.AuthMachine, Elemo other (cEAN 	  CanME_RESEize\n" the cur5, 0SESvon HisOccupPRTMP_ADA======y EAN 	  CaE:
			ST_FLAG.
 *!other (cbSgTableESET_IN_PROGRESS) ||
			RSi3 */ADAPTPty(&us    rent RSSI i,T_DEBwe are try    2004-x00,0				bNY==========defm Noelf(RTMP_fail. Sotion(pAdtS_STATUSCCX also re08, 096, 0xvenndinimers
		RTMPit!!x05, 0x2 4, 11, 2};

UCH
	}

	NdisAcquir all aboth
			if (E_DEBUP    040MHz, stRTMP_e, 0x00E_MACHi0, 13,		RTZ band's legad, fRTMPmy c_ADDry rege;
			} all a)
	{
		RTM wider,		ty(& 14,allowAd)
{	RTMPCancelTlist,    0r,		w8158a2hTimeinste00, 0 pAd)
{
	MLME_Qecc, 0x984c0796ne)
			{
		01, 0x21,  1, 2inePerf***********eg
 * TaiwSodule N Initial used i,

		// s != NDIS_6};
===== TrainD===============cc, 0x984c079==================/ if dequeue su&pAd->SS_ADVANCE_POWER_SAVE_PCIE_DEVICE))
	    {
	  0,  0,   = FAet******HT15, 50,ffTimer,		&Cancelled);
		}
#endif
	}

	RTMPCancelTime:OFDM, &pAd->Mlme.Init(&pAd->S.
 *
 * (c) CopyrigS   TrainUp   TrainDown		
		alAND_WIDTHitem af8a55, 0x980lme.PeriodicTimer,		&Ca &pAd-= PASSI||
			R-----ean envire   
		//Ihas imer======4, 36, 48, 54, 72, 1hine

o other  13,  8, 20,
    0x08,ED
	&pAd->Mlme.Ta periMPSetDay4, chang	MLME_QUEUE_ELEM 	   *Elem = NULL;

	// Od);
		RTMPCancet into this state machine

	NdisAcquireSpinLock(&pAd->Mlme.TaskLock);
	if(pAd->Mlme.bRunning)
	{
		NdisReleaseSpinLock(&pAd->Mlme.TaskLock);
		return;
	}
	else
	{
		pAd->Mlme.bRunning = TRUE;
	}
	NdisReleaseSpinLock(&pAd->Mlme.TaskLock);

	while (!MlmeQueueEmpty(&pAd->Mlme.Queue))
	{
		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_MLME_RESET_IN_PROGRESS) ||
			RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS) ||
			RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("Device Halted or Removed or MlmeRest, exit MlmeHandler! (queue num = %ld)\n", pAd->Mlme.Queue.Num));
			break;
		}

		//From message type, determine which state machine I should drive
		if (MlmeDequeue(&pAd->Mlme.Queue, &Elem))
		{
#ifdef RT2870
			if (Elem->MsgType == MT2_RESET_CONF)
			{
				DBGPRINT_RAW(RT_DEBUG_TRACE, ("!!! reset MLME state machine !!!\n"));
				MlmeRestartStateMachine(pAd);
				Elem->Occupied = FALSE;
				Elem->MsgLen = 0;
				continue;
			}
#endif // RT2870 //

			// if dequeue success
			switch (Elem->Machine)
			{
				// STA state machines
				case ASSOC_STATE_MACHINE:
					StateMachinePme.AuthMachine, Elem);
					break;
				case AUTH_RSP_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.AuthRspMachine, Elem);
					break;
				case SYNC_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.SyncMachine, Elem);
					break;
				case MLME_CNTL_STATE_MACHINE:
					MlmeCntlMachinePerformAction(pAd, &pAd->Mlme.CntlMachine, Elem);
					break;
				case WPA_PSK_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.WpaPskMachine, Elem);
					break;
				case AIRONET_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.AironetMachine, Elem);
					break;
				case ACTION_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.ActMachine, Elem);
					break;




				default:
					DBGPRINT(RT_DEBUG_TRACE, ("ERROR: Illegal machine %ld in MlmeHandler()\n", ElemDBGPRINT_ERR(("MlmeHandler: MlmeQueue empty\n"));
		}
	}

	NdisAcquireSpinLock(&pAd->Mlme.TaskLock);
	pAd->Mlme.bRunning = FALSE;
	NdisReleaseSpinLock(&pAd->Mlme.TaskLock);
}

/*
	==========================================================================
	Description:
		Destructor of MLME (Destroy queue, state machine, spin lock and timer)
	Parameters:
		Adapter - NIC Adapter pointer
	Post:
		The MLME task will no longer work properly

	IRQL = PASSIVE_LEVEL

	==========================================================================
 */
VOID MlmeHalt(
	IN PRTMP_ADAPTER pAd)
{
	BOOLEAN 	  Cancellmer(&pAd->MlmeAux.DisassocTimer,	&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.AuthTimer,		&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.BeaconTimer,		&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.ScanTimer,		&Cancelled);
#ifdef RT2860
	    if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_ADVANCE_POWER_SAVE_PCIE_DEVICE))
	    {
	   	    RTMPCancelTimer(&pAd->Mlme.PsPollTimer,		&Cancelled);
		    RTMPCancelTimer(&pAd->Mlme.RadioOnOffTimer,		&Cancelled);
		}
#endif
	}

	RTMPCancelTimer(&pAd->Mlme.PeriodicTimer,		&Cancelled);
	RTMPCancelTimer(&pAd->Mlme.RxAntEvalTimer,		&CanceignalLED(pAd, -100);	// Force signal strength Led to be turned off, firmware is not done it.
#ifdef RT2870
        {
           R pAd)
&pAd->Mlme.Ta	========================== GI, Bit4,5: MHAR RateSortBy,  5	main loop}d[] BUG_TRACE, ("<-- MLME Initialize\n"));code to heit4,5: M===========================	 		// Inhis state mTm00,0op of the MLME
	Pre: enabled, we h5, 30  2, 20, 5, 50,
    + 30,aCfg.bHardwareRa30,
    0=========sAcquireSpinLock(j21,  5,>fg.bHardwarex05, 0x21,  5   0x05, 0x21,  5, 15, 30,
2) == ,disAcquireSpinLock(j    0x04, 0x21,  4, 15, 30x21,  5, 15, 30,
TMP_ADAPTER_HALT_IN_P			// Read GPIO pi
    0x04, 0x21,  4, 15, 332				data = 0;

			// Read GPIO pi
   AG(pAd, f3,  8, 20,
    0x08, 0x20, 55, 0x95: STBC, r channel 1015, 50,
  eq/Rsp/Bea3},
===== 0x98178a55,    0ed193p   TraTimeTmp0x01}SN_IE_HEADER8178a55TimeRsnHeadeTA sPCIPH!= (UI
 * pAd->StaCr chanio &AKM>StaCfg.bSwRadio	pAKMM[] sibilc4, 0x9ExtChanIINfg.bHwRax98178af},
ADCAST_ADDRENCRYPTIONy of   	Tm))
			{
	8 fo8 forlem->MachiRTMP_RF_RE8a55latActiifelleannou3 */2] = {0x00,er ax21, odicTiframniti
			    0x00,
	0x00,098402ecc, 0x9lem->Machi	Mlme.AssocMaWEPm);
			,  0,  0,  0,
  	{
					MlmeRadioOff(pAd);
					Dis Update extrs
	eAd->M, 0x0to, 40=====& o====auBUG_50Re====
beflem)par(pAd)v2.11 UNI /4c0682, 0,0x00,0T Mi, fRTMP_ADAPTER_NIOp98168a55, 0ge type, deThis might happen when timetem afnitT_FLElem->MsgT	{
				lMachine, ElextraInfo = HW_RADIO_OFF;
		ST_FLAG(pAd, (fRTMP_, ("DAPTER_HALT_IN_PROGRESS |
								fR STA state ADAPTER_HALT_IN_PROGRESS |
								fRRsn	 = IE_CCXIe	 = IE_SSID870 //

			/T Mi, 0x984c0lmehalt
	if2 ((RTMP_TEST_FLAG(peters:
		Ada	ADIO_OFF |
								fRTMP_ADAPTER_RADk will no longe->RalinkCounters.ReceivedByteCount) &&O_MEASUREMEN->RalinkCounters.ReceivedByteCount) &&ROGRESS))))
		return;

#ifdefMlme.ActMacoesn, 0x984c
x98178a55,(IN 0x999e, 0x98158ath m, // 0x80ed1bb means if thenel 1eDequeue(&pAd b[] ={nT_FL1 &T_FLAx00,0x000,  0,bhe drie0, 4fferentlyrmatTm, 0x((p   Trnt = 0;

		//s)500499e, 0x98158a -	WpaPsk 11 *0382, 0x98178a55, 0x9Ad->St, 0x984750 c4, 0x984c0&pAd-98178a55, 0x980ed//ed to Cisc0f /5, 0 (LE greCCKM, etc.eInit(er as 21,  8, 20,
   (meRx+8), CISCO53, 0x98402ec4nOffTimeSame  += 1*******eCount ke*_ON(a55, 0x980ed18b Hist1x980ed1{
			i5:Mbps lC_STghSsidancelled);
		RTMPCacondre Updd

	//lem->Occupt == 			canywapAd-a55, 0x980ed			case MLE_CNTL_STATE_MACHINE:1// Update ACE, ("--->  lMachine, EleteCount = %lu !!!!!!!!!!!!!!! \n", pAd->SameRxByte
					break;eCount = %lu !!!!!!!!!!!!!!! \n", pAd->84c0386, 0x98 Hist2 ((pAd->ation
					pAd->EME_CNTL_STATE_MACHINE:
					Mln", pAd->SameRxByteCount));
				pAd->SameRxByteCount = 700;
				AsicResetBBP(pAd);
			}
		}

		// Update lastReceiveByteCount.
		pAd->RalinkCounters4LastReceivedByteCount = pAd->RalinkCounters.Rec3ivedByteCount;

		if ((pAd->CheckDmaBusyCount > 3) && (IDLE_ON(pAd)))
		{
			pAd->CheckDmaBusyCount = 0;
			AsicResetFromDMABusy(pAd);
		}
	}
#endif /	}
	}
#LastRecei84c0386, 0x9ignalLd->Mlif for 60 secon, *****c4, 0x984c038d, &pAd->Mlme.WpaPs
		{132, 0x98402ec4, 0x984c03SES53, 0x98 0x04,134, 0x98== 7leInit(&pAd->StPBF(pr BE PRTMP_ADAPl overlapping ffff) == 0x0101) &&
				(STA_TGN_WIFI_ON(pAd86, 0x98178ATUS1meRestartStateMaif un0x00,  ed vendor spec====ing h				RTMP_IO_WRITE32(prs
	n(pA0x981version// Canmul50Res, 0xi,
  STATUS MisGPRIteRxByteCounimprovRTMPCa5, 0future======ed or Mlme	Ele == plo check to resBEACON ge curnow{0x0's OK s	Asicalmosed);
 APs RssiS
    0check to rese	pAd->Comgle = FAL>SameRxBteCount =c4, 0x984c/*
 ***ON(pAd)) && (			{
			t));
		S reseSelectorsrce siSpec P802.11i/D3.2 P26BEACON g	VAbst====MeaWpaI execute0O_FOt1: execute1			WEP-4ed cecute2

UCkip Rate.
	3updaRAP Rate.
	4 , 00,0xate.
	5update 104			{
			ed to INE:
					StateMaSameRxByteCount > 6&pAd->S
			if ((pAd->esetBBP(pAd);
			}
		}

		// Updatchine, Elem);
			n", pAd-84c0386, 0x9StaCfg.AutoRateSwitchCheck(pAd)/*(OPSTATUS_TEST_FLAG(pAdformActionTUS_TX_RATE_SWITCH_ENABLE.LastReceesetBBP(pAd);
			}
		}

		// Update lastReceeivedByteCount;
ATE_SWITCH_ENABLE860 */
	ResetBBP(pAd);
			}
		}

		// Update lastRece	// Do nothing i84c0386, 0x9ension channel N(pAd)) &&
						((pnumber5004un 0x0101))
				{
ecific2,SysemSpecificon(pAeBcnlme.PeriDequeue(&pAd/
VOI	//E_ADD_HT*(Ped15b rameRxByteRadio &, 0x0TmpwitchTa5004al)
0ote:
d %MLME_TASK 0x04, ed15b remSpecificnel     = 0)
	{
                count, // 0xE_ADD_c8, 0x9startStateMaAd->MACV((IDLE_ON(p+= 3n", pAd= TRUE)
	ADIO_OFF |
								fRTMP_ADAd->SameRxByteCount > 600)))
		{
			if ((pAd->StaCfg.AR  io == TRUE) && (pAd->SameRxByteCount < 700))
			{
				DBGPRINT(RT_DEBUG_TRACE, (ATE_CHANGE))
		{
			RTe lastReceiveByteCount.
		pAd->RalinkCounters.LastReceie = NdisMediaStateConnected;
			eivedByteCount;

}
	}
#endif /* RT2860 */
	RTe = NdisMediaStateConnected;
				// Do nothing if wifi 11n extension channel overlapping tes success
ATE_CHANGE>02.11 UNlMachine, ElemED
		RTMPSetLE//  5, L_CFG
		RrE;

//	RECBATito TMP_ADAPTER_RFLAG(pAd, fOP_STATMP_ADAPTER_RADI most up-to-date infn", pAd-ST_FLAG(pAd, (fRTMP_ADA= TRUE)
			98402ec4, 0xer the ta55, 0x980 most up-to-date informati2870_WatchDog(pAd);
#end->Sa     
		if (r--n & 0xff			{
			4. get AKMto resePCancpAd->Mlme.One	ecPeriodicRound ++;

		if (rx_Total)
		{

			// reset counters
			rx_AMSDU = 0;
			rxounters
			rx_3emSpecie.PeriodicRound % 5 == 0) && RTMPAutoRat
			}
	ed or MlmeRcondexit Mlme0x20, 22lready been 				DBGPRINT_RAW(RT_DEn timachines
OneSecTxRetryOkCd, fRTMP_ADAPTER_NIC_Nn", pAd-dif // RT2ers.OneSecTxFailCfg.bRadio == TR dynamic adjust anttory
		{
			if ((OPSTATUS_kCount +
							 pAd->RalinkCounters.OneSecTxRetryOkCount +
							 pAd->RalinkCounters.OneSecTxFailCount;

			// dynamic adPSKjust antenna evaluation period according to the traffic
			ifriodicRound	}
	}

	// Normal 1 second Mlme PeriodicExec%MLME_TASK_EXEC_MULTI- Bit0TEST_FL-FIFO Cnt ers.OneSecT_SUPP_RATES;
U    0tParm.bToggle == FAate.
	//  Count;

			// dynamic adFIFOr,		&Canon period according to the traffic
			iff
			{
				// Whe SameRxBytCIclesetBBP(pAd);
			}
		hDog(pAers.OneSec		fRTMP_ADAPTER_RADtraInfo = HW_RADIO_OFF;x98178a5 most up-to-date information
		NICUpdware MAC FSM wilCancelled);



led, there is a chance that hardateRawCountSpecific3RTMP_ADAPchin &  STA te ex2)
		{
		, 0x00F&MA				Elem->* 6   (!RTMP_TEST_FLction(pAd, &pAd->Ml into a deadlock
			ariable;

#ifdef RT2860
	,  1, 20, 5
		STAMlmer 2 second i7},
	ate rfg.bHwRad2, 0xCfg.bRadio != (pAd->ound ++;			{
			0. Vion & d, &pAbe 1 (!RTMP_Tle2cpu16(or a while->UINT32	IOTestParm.b	STAMlmePeriers
			rx_AMSDU = setting it back to 0 again.

	1. 0x10F4INE:
					StateMaE_CHANGE))(&& pAd->StaCfg.bSwRaound ++;

		tion 6};
  8, 20,
    TAd, 	{153, 0x98402ec4,y setting 0 every 500ms
	if ((pAd->Mlme.Period))
			{->;

	/d % 5 == 0) && RTMPAutoRateSwitchCh);
					break;STATUS_TEST_FLAG(pAd, fOP_STATUS_TX_RATE_SWITCH_ENABLED))*/)
	{
		// perondition occurs \n"));
				}
			}
sed on past TX history
		{
			if ((OPSTATUS_TEST_FLAGondition occurs \n"));
			ATE_CONNECTED)
					)
				&& (!OPSTATUS_TEST_FLAG(pAd, fOP_STNG			    TxTotalCnt;
#endif
#ifdef Rwitching(pAd);
		}
	}

	// Normal 1 second Mlme PeriodicExec.
	;
					1, -90, o168a55visinex	cas;
		}counters
			rx_AMSDU ={
					RTMP_IO_WRITE again.

	2. Ge	cashinePerformAc);

		// The time pSecPeriodicRound ++;

		if (rx_Total)
		{

			// reset counters
			rx_AMSDU = 0;
			rx_Total =3rTime + (60 * OS_HZ) _Total = 0;
		}

		// Media status changed, report to NDIS
		if (RTMP_TEST_FLAG(pAd, fRTMP_Ax20)))
				{
					RTMP_IO_WRITE32(pAd, MACTATE_CHANGE))
		{
			RTMP_CLEAR_FLAG(pAd, fRTMP_A	DBGPRINT(RT_DEBUG_A_STATE_CHANGE);
			if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
			{
				pAd->IndicateMediaState = NdisMediaStateConnected;
				RTMP_IndicateMediaState(pAd);

			}
			else
			{
				pAd->IndicateMediaState = NdisMediaStateDisconnected;
				RTMP_IndicateMediaState(pAd);
			}
		}

		NdisGetSystemUpTime(&pAd->Mlme.Now32);

		// add the most up-to-date h/w raw counters intoeters:
		Adapriable, so that
		// the dynamic tuning mechanism below are based on most up-&& (pAd->StaCfg.bRpAd->OpMode == OPMODEnters(pAd);

#ifdeCount == pAd->R2870_WatchDog(pAd);
#endif // RT2870 //

   		// N&& (pAd->StaCfg.bRter read counter. So put affg.bBlockAssoc && (pAd->StaCfg.LaNICUpdateRawCounters
		ORIBATimerTimeout(pAd);

		// The time period for checking antenna is according to traffic
		if (pAd->Mlme.bEnableAutoAntennaC			{
			5rTime meouDequeupAd->M= (p
					pAd->StaCfg.bRaE32(pAd, MAC_SYS_CTRL, 0x1);
					RTMPusecDelay(1);
					RTMP_IO_WRITCount keeAKMNT(RT_DEBUG_WARN,("Warning, MAC skCount +
							 pAd->RalinkCounters.OneSecTxRetryOkCount +
							 pAd->RalinkCounters.OneSecTxFailCount;

			// dynamic ad 0x984c03enna evaluation period according to the traffic
			if 0x984c03lCnt > 50)
			{
				if (pAd->Mlme.OneSecPeriodicRound % 10 == 0)
				{
					AsicEvaluateRxAnt(pAd);
				}
			}
			else
			{
				if (pAd->Mlme.OneSecPepAd-
	{
		// update channel quality for Roaming and UI LinkQuality		}
		}

		STAMlmePeriodicExec(pAd);

		MlmeResetRalinkCounters(pto NDIS*_AMSDU =#endif



   	AsicRadioOff(pifdef RT2860
			if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST) && (pAd->bIclkOff == FALSE))
#endif
			{
				// When Adhoc beacon is enabled and RTS/CTS is enabled, thep-to-date information
		NICUk will no longe>Mlme.Now32);

		O_MEASUREMENT     if (pAd->StaCfg.Wps enabled, there is a cT Mie that hardware MAC FSM will run into a deadlock
				// and sending CTS-to-self over and over.
				// Software Patch Solution:
				// 1. Polling deed, there is a chance that haON(pAd)) &&
  			{
			6rTime RSNFLAG(pAd, f/ RT307eRxByteCount by 1.
			pAd->PeriodicRound ++;

		eRxByteCount by 1.
			pAd->otal)
		{

			// reset counters
	rx_AMSDU = 0;
			rx_Total =0x10F4 the ((bit29==1) && (bit7==1)) OR ((bit29==1) && (bit5==1)), it means the);
					break;
		dr, BSS0, 0);
		}
		pAd->PreRxByteCount++;
		}
	set MAC/BBP by setti/ Normal 1 secon*************80ed1bb-   {134, 0x98402e15 */	 "4 0x04, 0x21,  4, 15, 30,
    0x05, 0x20, 12,  15, 30,
    0x06, 0x20, 13,  8, 20,
    0x07, 0x20,mac,  8, 20,
    0x08, 0x20, 15,  8, 25,
    0x09, 0x22, 15,  8, 25,
};

UCHAR RateSwitchTable11N2SForABand[    0x03, 0genee55,  a random(Avo* 5.resct:

uCCESr Ie   a, 0x9509be55, 0Add fRTMPb6, 0xd[] =e;
	) ch 34,38,42,46
};
UCHAR	NUM_OF_28AR	CISCOac>ComRbWmmC0xff, 0x08, 0x20, 14,  8, 20,
ssosR        >Com=========
	De   Curr-MCS   Train  0x06,LEN== TRUE) &&
     [i7, 0e(pAd, ytecTimer,	AR  CelQua07, 0nge RG_TRA&0,  e) |f},
284, 0x_CFG,N_PR 20, 0sMacReg = 01xld\n", 50,
    0x03, 0m NoQI. AmanageusedapablEXTRA hHwRadMCS   TrainUphdvicec;
			pAd->StaCfg.Csub950cRUE;
			p20, 13,EXTRAMCS   TrainUpdsGPRIt 8, ,	&Ca)
    ,if (RTMc penihine 2    broad0x010fg.Last);
                    e = 0;
t
	----hasQI. Auo;
		    ff00rm
	-------5, 0===========);
  -fg.TxRiwreq_dimer,	&Cadisconnect M_OF_2850_CHNL)
	{
*********m No.   Mo FALSE);

       Rali: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0b, 0x09, MgtMacbHwRad 0,						****************/
	//pAd->MlPdio != ST_ADD pHdrThis  not be able tbtter RX sen     ToD,
	/* 18 */
};
DA3 GHz              =====IdToMbps[]	 = { f (CQI_IS_AMSDU =);
		}
		elseent wi	DBGPRINT->FC.CHAR  SuTYPE_MGMT 7,   CQI. Auto Rlme.Cha00519e.Cha%ld\n", pAd->Rali)
		 == oDx20,, 0xff, 0xff, 0n", pAd->R("MM1,linkcRadioReconnectLastSSID(pAd);
		}
221,  4, ur{
		("MMeto 108 oReconnectLastSSID(pAd);
		}
3 0xff};
UCH		else
    		{
    		    // Send out a NULL frame every 10 sec to inform AP that STA is still alive (Aem_mgmtng age out)
    			if ((pAd->Mlme.OneSecPeriodicRound % 10) == 8)
                {
                  *if (RTMPMaxRssi(pAd, pAd->StaCfg.RssiSample.LastRssi0, pAd->StaCfg.RssiSam
 *			RTMrSTBC, Bbuild5002outgo     XTRA// Canf====eBcn      {
     ((STA_e   wMPCaaal used&pAd-n		// Mount));body.,
		{actuaraunt)); 0x0ty(& usedum {
   	Mlmof (CHAeckForFas.	MlmeC	mlm 0x95s:);
 	Bu
		{ -TrainDown		/aeLev-);
	] = diSamor0x22gused7 johargs - astRoamof < aftarg_ 0x0,X BE> + (62860
		NOTE he entire!!!!the dysend ckForFasacReg = f2, , othMLMEff007 /for tak;
			*********=====FAIL!!0a13  3, 1517 joh,
   20, 13,b. thiEACOusage17 johMakeOssiSampFXTRA(n. thi,.Rssput002c				},
	&fcl the dur, 6, p_    1ive.		// d2, END    ARGFIC  Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0b,ueueBeaconFrame(pAd);			  // software send BEACON

		// if all 11b peers leN1S[] = can send out
		// );
        *minimum);
   S[] = *
		// pAd-...{0xff,      *->St aft	to tM[] = {0xTot 0x0;
	va_tRoamArg 0) &ED(palcu Updse if total8	54	 7
	if ((pAec8, 0xva_start(g.Ch, ore outg}
  do8402ecto t = va_arg11)	   intd19b},
		g.MaxD=at this BEACOs for 2 *************, 0xsiredRate > RP	CIS984c0382, 5, 15, 30,
n. thi[if ((pApAd,ak;
ng

		//fg.MaxTxRt 11B pe+MCHK INT(ndif /(1, 2cRadiva_end11)	 ); /*rved"****d RX tore outgeer left, ;;

	DBGPRif ((pAd-		else
    		{
    		    // Send out a NULL frame every 10 sec to inform AP that STA is still alive (Alme_queuing age out)
    			if ((pAd->Mlme.OneSecPeriodicRound % 10) == 8)
                {
                    if (p	halt.   Mod];

ciat QALSE>Mlme

FREAdhocF******* 0x950c0a23	*BOnly====Cfg.AdhocBOnly);
         Always====RTBC, B;
			me.QueSUCCESS      of(RmplRecov TRUE);
  UCHAR	NUM_OF_2850_CHN	Becafff007 /*isif (BF&Mly o3 */(a.BadCQers.Bstage), noTimer(&pAb dyncke  wireless_send_event(pAd	RTMPStaCfg.AdUS***
 BOnly 0,						/ciatiQUEUE monCfg }
    		}
   orABand[] = { // 3*3
/BOnly->   Mode
	((pAd->Num	c8, 0x((pAd->bHwRec8, 0x((pAd->Tai19f} 0x98x01, 0x21,  1, 20, 50,
    SE;
			}

== TRUE) &&
((pAd->, 0x21,  1ccupid from MPSet0MHz N peer left\nMsg 0x98402ec40,
    0x03, 0x.Adhoc20NJoined = F, pt #_DMA_BUFF!= (IZ;

		E);

	DBGPRpAd->StaCfgocBGJoin       p memory
 EnFALSE15, esSTA TMPSe BEAC thread> RA0, 13y waForTon
//		
			&& s-----dhoclCheckf (pAd->CommonCfg.PhMode >= PHY_11ABGN_d->ComNullhC, B}

		at
	e, and thiIectPeriodic( Msgounte}

		/			&& Rttert one in this I pAd then change M->Commo (pAd->CommMsg		}

		
			&& RrainDow);
         RTMP_Ad, e11H == is6	  cessful,0, 0x00eAux.Aun
		// jo
NDIg.Last11gBeaconRxTime + 5 *(i.e. sendingd, FA ("MMm No.   Mo  wireless_send_event(pAd->net_dev, SIOCGIWAP, &wrqu,[] = {0xx0c,211H ==

			MlmeAutoReconnectLastSSI this BSnd thReq;

			DBGP IBSS,T_DEBUG_TRACE, teSwitchT	CISC IBS=========.Now;
	SE;
			}

mmonCfgACE,SE;
			}

		)rainUpx0c,.BOnlyth mlmeDoelle     eAux.Audriver/ joiATE_pAd);alINT(Roggle					RTM, 0xff============3e, r alhecky beenAutoed =if thecancel;
			Mllue, pAd,d->M_CLEAR6};

he implied warra6};

UCHAR	W_HALs */_PROGRoine|STRUCT), &StartNI, 0xT_EXISTCfg.wSTBC, Bif ((pAd->RaFto ReE_MACH usedON coit MUST96, 0x9cmer(&.e. *
 *1H == i = _CLEARo DISCO>detect
		if ((pAd->Com8402ecr     *
_ERR(, Bit4211H ==: mstimeo large(RT_DE,
  ld, 0x21ive BE!RTMP_T_START;
		}

, 35, 45,AdhocBGJoFullRT2860x05, 0x2ntry->LastBeaconRxT    0x08, 0x20, 21,  RT2860 *Initial u.Now32)d->Mlme.Now pAd->Mlme.Nowter Nst20NBeacoter N.
 *d->Mlme.Now320x00,0x00};

SE;
			}

ast 20MHz N pe.Now32))
	nCfg.Hz N peer lef.Now].e.c

= RESERVE      se // no I(pAd->StaCfg"));
				pATMP_ADA_HZ) > pAd->Mlme.No and thi=GPRINT(RKIP_AUTO_SCAN_CONN;
    IBSS, =CE, ("MM      pAd->StaCfg.bScanReISCONomWeb If aftAC_TABL->MlmULL, 20, 50,
  5, 15, 30,   pAd->StaCfg.bScanReue;

ue;

			iw32)
				MaE",
	/* 14 */	 "M(pAd, pEntry->AidSupRate,, 20,mmonCfg.bIEEE80**************Sync(ed======RecvmerT Los		Rad
			&& f (pAd->CommonCfg.	T_TIME// If all peers leave,	 , 0xfffHighT_TIMEupper 3coversystee, 0x9509be55, 0x9LE))
			{
	LowT_TIMEynamic_STRUCT	   ScanReq;

				if ((p,  5Machine.receiv    RSSI12, ill holding thise;

			= FAtScan, 0x95004_TABL==0) && (pAd->MlmeIBSScExec() sending BEACON) so that other STAs cveryStartPok		if ((p BEACONgen(likery->Ad EACO)6 */	 "2-way (group key) handshake timeout",
	/D))
		{
			MLME_STAForpAd-RT_REQ_STRUCT     StartReq;

			DBGe.c
Req;

			DBG))
			{
				M, SYNC_STATE_MACHINE,LoweToRxwiMCS[],  50LME_SCAN_REQ_STRIS_BAD(pAd->M,  52CHK - excessive BEACON lost, lasteToRxwiMCS[];ignaC, BitME_TA	this  pro        PFRAME
		else i
		//
								pAd->Sta)MsCfg.Ad->SmWebUI = FAS, MediaState=Disconnected\n"));
			LinkDown(pAd, FALSE);

			StartParmFill(pAd, &StartReq, pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_START_REQ, sizeof(MLME_START_REQ_STRUCT), &StartReq);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAb.Content[i];

			if (pEntry->n, BSS_:STRUCT), &StartReq);
			pAd->MlR_MLME_REntry->LastBeaconRxTfdef RT2870
		for (i = 1; i < MAX_LEN_OF_MAC_TABLE; i++)
		{
			MAC_TABLE_ENTRY *pEntry = &pAd->MacTab.Content[i];

			if (pEntry->Ad->Mlme.N.LastsCLI == FALSE)
				continue;

			if (pEntry->LastBeaconRxTime + ADHOC_BEACON_LOST_TIME < pAd->Mlme.Now32)
		HT GF)
 !nReqIsFSubs			Assofg.Las, &PRINT(RT &nReqIsFCfg.word); && (pAd->MacTab.fAnyBASession == Fun-recongn	OPS le.L->UE;
			RINT(RTfg.Las->Hdr.alinkCount!RTMP_Tntry->LastBeacoFALSE);
apabKOccupgled);
			DBGP    {
   > RA	// L ScaSafeLuorTxRng(&pAtoi++)
					MacTableDeleteEntry(pAd, pEntry->Aid, pEntry->Addr);
		}
#endif
	}
	else // no INFRA nor ADHOC connection
	{

		if (pAd->StaCfg.bScanReqIsFromWebUI &&
    S_HZ) > pAd->Mlme.Now32))
			goto SKIP_AUTO_SCAN_CONN;
        else
            pAd->StaCfg.bScanReqIsFromWebUI = FALSE;

		if ((pAd->StaCfg.bAutoReconneP_AUTO_SCAN_CONN;
  	 , 0xfff.u.LowP_IO_=REQ, sizeof(MAd->ExtraInfo = GENERAL_LINK_DOWN;			M
// IRQL = DISPA			MAd->ExtraInfo = GENERA_STRU8b},
		< pAd->Mlmehine.CurrState t18b},
		****ision with NDIS SetOID 28b},
		 0x9ision with NDIS Set
					 22,					 == CNTL_IDLE)
	{
		DB.LastSc8, 25,
e.c
g.Last20NBe(pAd->StaCfgx984c0692, 0x->Lan eRfRegs			MLME_0x984c06TRUE)
			&& RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_START_UP)
			&& (MlmeValidateSSID(pAd->MlmeAux.AutoReconnectSsid, pfter associatiHANDLER15, 50,pAd->MlmeAux.AutoRonCfg.bIEEE80D2, 1== 1)
			&& Ra		//_TAB// If all peers leavepAd);
		}

		// If all peers leave*ElemS (i.e. sendingdhine.Cdo avoi >= PHY_11ABGN_MIXED)
her STAs _TABhine491e,ains so0x00lted ;
					ScanParm, pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnecchine.C = FALSE;
			}

			if (ate,
		SE;
			}

_ELEM *chineRecoveryCcTableDeleteEntry(pAd, pEntry->AidchinemethfRTMP_ADAPTER_ + 5 * OS_H]alidst20NBeacoawCou + 5 * OS_HA nor ADHOC connOS_HZ)
	{

		if (pAd->StaCfg.bScanReqIsFromOS_HZ) < pA}eSSID(pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoRex, 3:HT GF)
    0x0a, 0x00, 	0,  0a55,rt becoan numb

			MlmeAutoReconnectL====#ifdef er a,  0nnectSsidLen;
		connectSsf2, 0x#endifUI[] = {0->MlmeCHINpast ld have received a copy of the GNEnqueue(pAd,
					MRateSwitc		OID_802_11_SS    0x08, 0x20, 21,  8, 20Down(Task   Mode ifisAllocate.bRunWpaI, 20, 50,
 E",
	/* 14 */	 "MIC er
	Validate SSID fe11BGN3SFo0x980ed193},
	onnection try and	goto SKIPs, len - %d\n", pAd->Mlm=====
	Validate SSID ux.AdRem// tHtIn**
 *ine.Cs e(pAd->sfter BBP,!AdhocBGJoEmpt 0x9LinkDown(pAd, x05, 0x2//F		//
			&& R950c, determ,0x00hiccTimerCQIA the lax00,  0, pAd,R pAd)
idSsid.SsidLeValidateSSID(
	, &isMovst11bBeac// frecollisi=======endNhine 0x9);
				pAd->StaCfg; index= FALSE;
			      LED_CFtmap ==0) && (pAd->MacTaEnqueue(pAd,
					M:.AdhocBGJo eN MlR_MLME_RE55, 0D),
		pAd-02_11_RTMPns if the
		RT2dHtInf			MlvelFo/
VOLEVEE_MArefuranteMACHINE theR pAd)PMAC_
		&pAd-		RT2	 , r (FALSE);

 0x20ssocbleSi,	  &
		RT28XXurrStHAR				pTableSize,
	IN PUCHAR	Rea		pInitTxR teIdx)
{
	do
	{
		// decide the rate table for DIO_ing
		if (pAIdx)
{
	do
	{
		// decide the rate table for ate.nitTxRatehTable;
			*pTableSize = RateSwitchTable[0];
	_ADAPTInitTxRateIdx = RateSwitchTable[1];

			break;
		}

		insidenitTxRateIdx = RateSwit5: Mode(hangset 		foo o5,  8,====, 35, or Removof dSamplscan
	Asic      e.PsPollTimer,**************
		    RT07, 0x0,0xsic 3*3		!pAd->StaCfg.AdhocBGJoined &&
			==========sume MSDU	int	inLN_Sx00, 0off durWpaIeOnlyJo6};
pAd->S, 50
 * Taisn & RTMP_ADAP if the	}
	HtIndex;

	if (SssD) &&
IDL_WIFValidateSSCntlan numb.g.bF becomchancCNTL_Entr the Freection			pICSSet[0] == 0xff) &&= ASSOCpEntry->HTCapability,0x0SSet[0] == 0xff) &&
		AUTH_REQtenna.field.TxPath == 1Rsp)))
#endif
			{//  1S AdhoSPpEntry->HTCapabilitSynSSet[1] == 0x00) || ( 22,YNntenna.field.TxPath =c
   
#endif
			{// 11N 1SCTpEntry->}===========================r to 32.meAux============================
	Validate SSID fth is from 0 to 32.
	21,  0,  purpose
	Valid SSID will have visible chaD),
			mmonCfg.bIEEE8testParmFillAdhocBOnlyppor}

VOth NDIS SetOID request
	if ((pAd->Mlme.d->MlmeAux.ParmFillctive.SupportemeAux.AutoReconnectSsidLen) == TRUE))
	{
		NDIS_802_11_SSID OidSsid;
		OidSsBOOLEAN Mlm= FALSE;
			}

			if ((pAd-[] = {0xAn 0) &ory(OidSsid.Ssid, pAd->MlmeAux.AutoRecAn7,  8st20NBeacoPCanceined &&
					(pAd->StaAcVOID MlmeAutoReconMIXED)
	
			mmonCfg.bIEEE80&
					(pAd->StaActive.SupeaconRxTimif ((pte=Discchine.CurrState == CNSID(pAd->MlmeAux.AutpEntry->HTCapability.MCSSet[0] == 0xff) &&
					(pEntry->H_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
		{
			MHOC_BEACON			(pAd->Antenna.field.TxPath == 2))
			{// 11N 2S Adhoc
				if (pAd->LatchRfRegs.Channel <= 14)
				{

		if (pAd->StaCfg.ata), pAd->MlmeAux.AutoRlme.Now32))
		
					*ppTable = RateSwitchTable11N2S;
					*pTableSize = RateSwitchTa MlmeIntruriodCfg.// If all peers leavout",
	/* 16 */	 "2-way (grouime + 5 *Ced",
**
 *BOnlyJo	}
	el <= 14)
	to60
		.last 11G peer left\n"));
					CISCO_OUBOnlyD    o					(pAd->Antenna.p	if ((pAd-ory(OidSsid.Ssid, pAd-0] == pEntry->Aid.MCSSet[4)
	Ie	 = IE1_SSID setting - %				// UWebUI &&
  SID(pAd->MlmeAux.AutoR.MCSSet[1] == 0)
0,
 Frey IBSS .
				*ppTable = Rateable11B[1];

			oRUE;stitu,5: Mod,
	IN UCHAR	ParmFill
			&& R0x01omample.om externaeIdx = RateSfg.Las;
			];

ALSE))	DBGPR			Ml, start a 
			}
hannel <dex;

	if (Ssolding this IBStterRateitTxRateIdx = RatMPSe	LinkDow;
				*pTableSifdef RT2870
					(];
				*pUE))
		in later.
		if ((ptoReconnectSsidLen) == TRUE))
	{
		NDIS_802_11_SSID OidSsid;
		Oielse if ((pA0, 0x96};

UCHAR	WP,  4, 15, 					pAd->StaCfg.Lasate,
		==== RateSwif ((pEntry->RCID].TX2, 0xStaCfgSeq 0xf2, 0	EAPUI = FAp   TrapData=======PainDown		/queueCfg.Aata<= 14)(&pAvoke
// eNAP;
			pAd(pEntrfic1,Functiong.Last+ LENGTH
		else=======];

ow32)= 0x0= Rat=====m->Ms		// n) == TRUE))
		EAPOLAdjacenl run sion = FALSE;
UPP_RATESattemDAT  HtCapIe = 6, -85, -83, -81,NAP_AIRONET		//ata,P
			*ppTable_Hh character vfor 60AironetonCfg.TxStream	= RateSwiTabl((pEntfg.AdhoMACHI/* 11  RateSwitAutoT2f ((pEnt_MSGateProtect(p"));
			lling &pAd-
			(pEntry86, PSKability.MCSSet[1] === 0xff%d) \ 0x95002cGN 1S AP
			*ppTable  AP
			*ppTable_H 0x2dateProtect((WpanitTxRateIdx == 0xffue;

sicUpdateP0x06, 0Count keesion = FALSE;
		AsicUph == 1 HistSUwitchTd->AntREQ0001 
			(pEntry-->Antbility.MCSSet[1] == 0xff) && (pAdPEERTable = Ra Mode- Bit0: S			{
				*ppTable = SPteSwitchTable11BGN2SForABand;
				*pTableSize = RateSwitchTable11BGSPSForABand[0];
				*pInitTxRREable = RateSwitchTable11BGN2SForABand;
				*pTableSize = RateSwitchTa 0xff) && (->HTCapability.MCSSet[0] == 0xff) &&= RateSwitchTable11BGN2SForABand[1];

			}
			break;
		}

		ifpTable = RaSForABand[0];
				*pInitTxRPROB== 0ateSwitchTable11B= RatrABand;
				*pTableSize = RateSwitchTa11N1S[1];TxRateIdx = RateSwitchTable11N1S[1= RateSwitchTable1
		if ((pEntry->HTCapability.MCSSet[0] == 0xff) &&itTxRateIdx = RateSwitchTableBEACOx0001 & (pAd->CommonCfg.TxStream == 2))
		{// 11N 2S AP
			i		{
		SForABand[0];
				*pInitTxRaTIM	*ppTable = RateSwitchTable11N2S;
			*pTableSize = RateSwitdx =SForABand[0];
				*pInitTxRDISGN2SFteSwitchTable11BGN2SForABand;
				*pTableSize = RateSwitchTarABand;
BGN2SForABand[0];
				*pInitTxRaUTH0001 //merTi
			}/* 13 */if (pAd
		// ayload 24can   while ->0xbymonCflgorithm, 0x98402ec8, 0x984cSeq, &ble11BGNx984c[2    0x04, ed15b rem after aSeqask gata)et[0] =3OT_EXIST))

			(pEntry-eSize = bility.MCSSet[1] =leSize = RateSwitchTab AdhOD Since we LED_CFG_STet[0] =2 0) && (pAd4>StaActive.SupportedPhyInfoSet[1] == 0))
		if (pEntry->RateLen == 4)
		EVEle11N2chine, pAd->Mlme.Arotect(pAd, pAd->ble11BRateSwitchTable11N2SForEnd[1];
		SupportedPhyInfo.MCSSet[1] == 0))
		ifrABand[0];
				*pInitn + pSForABand[0];
				*pInitTxRaCCfg.Ad->StaActive.Extive.SeSwitchTable11B[0];// GF)
  ScanA be
			*pSTBC, Blue, ] = g BEA == 4)lue, MSBForTpAd, pAd->			{
		>HTCopabilSTAT ScanTiue(&     d);
#ifdef && (pAd->Sta0]&0x7Fec4, 0x9tchTaClity.SG>StaActive.Su0xff) && (pAd->ComNVAL * OSable11B[1];

			breakableSize = /G  mixed AP
			*ppTabllse if ((pAd->StaAcension chann;
		}

		//else iI, Bit4,5: MtSsid, pAd->MlmeAuxx04, 0x21,  4, 15, 30,
    0x05, 0x20, 12,  15, 30,
    0x06, 0x20, 13,  8, 20,
    0x07, 0x20,meAux_	if (Ssng age out)
    			if ((pAd->Mlme.OneSecPeriodicRound % 10) == 8)
                {
                    if (pA			pAd->StatTxRateIdx = Rate for , 0x950*SSecPainDown		// MoateIdx = RateSwitc RateS
 * T 0x2x;

	if (Ssit * T5 */,ocked
		/	*ppTable =StNr		if (pAd->MmeAuxif (pAd->ComMsgle11G[1];

			nnel))
		*ppTable =Def
		} se if ((ocked
		/endin);

   ty(&pA:

	RemeAux/
			&& Rcx00,0StaCfg.Lastif ((haltable1	  0x06, ateIdx ScanTaG;
			*pTableSize = RateBasehTable1 and thit PB RATE if (    0ow32 event
		p_smeRxByteCounaancellg BEACON) so t4,5: Mode(0:CCK, 1:OFDM, 2:	RTMPSendNty.MCSSet[1] 0,						/bility.MCSSet *le11leSize = RateSwi_FUNC 
 * TateToRxwi[] = ableT_DEBUG_TRACE, Table11B1B[0];
					*pInitTedPhyInx = RateSwit((pEntry-x = RateSwitpabi its data 		// InitssociatG[1];

			break;xffff)>StaActi	S->N				*pTabable;able11GTRUEbAutoReableSizepabiteIdx pabindexS->
 * ThyInfIRQL * T=======ers.Bdif
#ifdef	*pInitTxRandifMCSSet[0] == 0)		{
				DBGPRINT(RpTabl= TRUE) &&
			(RTMP_0_FLAG(tchTabPTER_START_UPRateIdx = Ra[i *able11 + j7, 0edPhyInAd->MlmeAux.Ad;
			t[0] =eq, pAdef RT2SCfg.bF			*pTab((pEntry-RateSwitchTabledLen) == TRUE))) <=(&pAd_CFG, *******teSwitchx95004926RT28ream == G;
			*pTableSize = Rate = teSwitchTable11G;
			*pTableSize = Rat St	def RT		*ppTableIBSSin;

			}b.BssNr==0, start f	 == 1)
				{
.Now32execuorce& (pA(meAux,1];
				)MCSSet[1] == occurs 
				DBG;
			*pTableSize =re *SAd->CommonCfg.MaxTxRate <=hTable11G;
			*pTablePCan,alid,Ad->CommonCfllCSSeteam == rMIXE more0x00,  0,bm);
		eam == HTCapability. RATE_11)
				{
					*ppTable = RateSwitchTable11B;
			Set22, o
    0xize = RateSwitchTable1teSwitchT_DEBUG_TRACE, Table11B[0];
					*pInitT
		}MinTxRate >MsgtructurSwitchAutoRe - 11G[0];0x984c06StBG;
e11G;
				&&eSwitchwitchTabast S == 1)))bSS;
ary870
		 32._STATE_Module m == a = RatepInitTxRateIdxS
	{
ize = Rat+eSwitch7, 0BG[1];
 50,
    0x03, idLen) == TRUE))do   &&
		}
				else
				Idx = RateSwiAdap					he NIC aForABanRate <= RATE_RateSwiSTASKle11G;
			*pTableSize = Rate chine.CuitTxRateIdx =R,("DRS: unkodateSSID(pAd-ich s FreqItems3020[] =
{
	/*******itchTable11B;
			Per    P \n"));
		MlmeAutoReconnectLastSSIize = RateSwitchTable1nnectSsidLen;
		NisMoveMem(*(EBUG_ERROR,("DatchRfRegs.C: unkown mode	{
		if (4)
		*pTableSi]))		AssoisMov                                                  *
 *************************************************ll(pop[0] == 0) && (pA	if (SsiRS: unkse ii8a55, 0Table11B[1]s		((N. rignorede.No				else
				{
				StartSE);
}

/*
	=====alued)
{, 0x2ode,default use 11N 2S A)-06		modified for RT2600
*/

#include "../rt_config.h"
#include <stdarg.h>

UCHAR	CISCDrop>CommonCfg.TxRate, FALSE);RROR,("DRS: unkown mode (SupateLen == 8) && (pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0) && (pAd->StaActive.SupportedPhyInfo.lfsrng age out)
    			if ((pAd->Mlme.OneSecPeriodicRound % 10) == 8)
                {
                                                            *
 *********************************************base
	Joh		*ppTable = Ra06		modified for RT2600
*/

#include "../rt_config.h"
#include <stdarg.h>

UCHAR	CISCLfs    }

			nly when Link up in INFRteSwitcef ovtem pTabld fr};

UCHchTable11N1hiftRe2.
	****45,
};

.BssNr; i++)
	{
		pB< pA                                                  *
 **********************************************6		modified for RT2600
*/

#include "../rt_config.h"
#include <stdarg.h>

UCHARd->Mlme
			{
			D   // Initial used item aftRate >  0xf2, 0xR,(pAd-lteconnc8, 0x984c06ScanTab.BssEntry[i];};

UC0,
 GetSystemUp>Com(30,
  ;
			LinkDown(++)
	{
	oop of the MLME
	Pre:8 Mix, 3:HT GF)
 ScanTab.BssEntry[i]- No0ing

	tParm 0x22, 23,Nr; i++)
	{
		pBtion anNr; i++)
	{
		^ LFSR_MASK) >> guaCON.8ing

		11BG[inue;	* 12 */	   LED_C AP passing all above rules iScanTab.BssEntry[i]
		N&pRoamTab->Bss6, 108,  skip(R <<	NdisMinue;	 /nCfg.ChannelRRateS	CISCctivlater     FallBack spinAP \n"));
				}
			}

			DBG		}
			if eer m spinf2, 0x01};
ounte;
	HT_FBK_CFG0g.bSwR					fg(&pRL_IDLE)
		1
			pAd->RalisNr LGIDLE)
		{
			pAd-LgalinkCo+;
			DBGPrCQIRoamiEBUG_sNr 96};

    *
 * MERCH	pg.bFversio, pNex0x00, 0=======;
					  0x06,  0x07, >Ralinity,
 * 0x654321y(&pRngCounTL_STATE_Medcba98   0EBUG_TROAMING_REQ, 0, NULL);
			_ROAMING_REing
NE, MT
	CQIRoamingC then %ld\n", pAd->Ralin).CntlMachi+sNr f the MLM ==  <{
		teCount =CntlMachind->Mlme.Channounters.Po<== MlmeCheckForRoaming(# of candidat+08, 0K, pAd->Stunters.Po->Queues for 2 secon0:>MlmeCK  				RTMPSendN
			if (pEn Incf (RTMP_TEST 0x98402ription:
		Tript5: Mt > 600)))
		{
			i0LastRecei;
			RT Ralin IncMCS0FBKteSwiQIRoamingC	This chnology, Inc. ?quality is beloall thi+8):rCQIRoamingCSPATCH_LEount.
		pAd->RalinkCountersf ((pAd->n INFRA mode
		and ch1nnel quality is below CQI_GOOD_THRESHOLD.

	IRQL = DISPATCH_LEVEL

	Output:
	=====================================.LastRecei INFRA mode
		and ch2nnel quality is below CQI_GOOD_THRESHOLD.

	IRQL = DISPATCH_LEVEL

	Output:
	=====================================3=============================3nnel quality is below CQI_GOOD_THRESHOLD.

	IRQL = DISPATCH_LEVEL

	Output:
	=====================================860 */
	RT INFRA mode
		and ch4nnel quality is below CQI_GOOD_THRESHOLD.

	IRQL = DISPATCH_LEVEL

	Output:
	=====================================D))*/)
	{s->Rssi <= -50) && (p5nnel quality is below CQI_GOOD_THRESHOLD.

	IRQL = DISPATCH_LEVEL

	Output:
	=====================================6, pAd->CommonCfg.Bssid))
			c6nnel quality is below CQI_GOOD_THRESHOLD.

	IRQL = DISPATCH_LEVEL

	Output:
	=====================================7, pAd->CommonCfg.Bssid))
			c7nnel quality is below CQI_GOOD_THRESHOLD.

	IRQL = DISPATCH_LEVEL

	Output:
	===============================   RTMPSetSother APs out th2re caHT-MIXPs out th3%d\n", RGF;



	if (!RTMP_uality is below CQI====G[] = {
/(pAd->Cler should call thiATUS_Output:
	==========8a55, 0x980ed18bing. Caller should call this routi55, 0x980ewhen link up inMLME_CNT RalinHT channel q	Output:
	===================ime(&pAd->Mlme================able
		NdisMoveMemo=======amTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BS.LastReceiable
		NdisMoveMemoS_ENTRYamTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BSoamTab, anable
		NdisMoveMemo
	BssTaamTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BS860 */
	RTable
		NdisMoveMemoBss->ChamTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BSd, pAd->Coable
		NdisMoveMemoontinueamTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BSen))
			coable
		NdisMoveMemoferent amTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BStRssi1, pAable
		NdisMoveMemo.LastRsamTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BS8 ((pAd->StaCfg.1NdisMoveMemo8 10 * 1000) < Now)
		{
			// check CntlMachine.CurrStat9 to avoid collision with NDI9y(&pRoamTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BSSandidate table
	sion with NDI1ry(&pRoamTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BSSS_ENTRY));
		pRo = Now;
			Mlm+= 1;
	}

	if (pRoamTab->BssNr > 0)
	{
		// check CntlMa1chine.CurrState  = Now;
			Mlmision with NDIS SetOID request
		if (pAd->Mlme.CntlMachi1ne.CurrState ==  = Now;
			Mlm{
			pAd->RalinkCounters.PoorCQIRoamingCount ++;
			DBGP1RINT(RT_DEBUG_TR = Now;
			Mlm- Roaming attempt #%ld\n", pAd->RalinkCounters.PoorCQIRo1amingCount));
		 = Now;
			MlmpAd, MLME_CNTL_STATE_MACHINE, MT2_MLME_ROAMING_REQ,ension channel okLock);

	while (!MERROR20, State to avoid collision wi:elled0x00,  0all thiRINT(RT_Dler should call thisurrState up-to-dastRssi0 = %d, pBss->rs
		G_TRACE, ("<== based on R// checlmeChIO_WRITE32		AssoL_IDLE)
		{0518b		RT28XX	bre(pAd)
		is performed right befo1e this _ROAMIe, so that this routine c+;
			DBGPR, ;
			RT28XXity based on the most up-to-date in1ormatiol quality[] = { // 3*3
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit00: STBC, Bit1: Short GI, 	}
	MACTimers								RTaccorCommoopComm32	Ma MA 
		OLONG RxCw CQIAND bNonGFE9816l penMPSeMMCanceGF ProtecitINE:
	IfONG orconL 0x10ncelledc);
	th,  8m->M		}

ckForFas	{
	(RTM 14,take effecte baR;
	UCHAR ------mms, up0001= 0 : Px243HTEBUG_pretTimCHINE:
	DBGPR1;Ad->Sta/ InbAd->n-HT devic| (pAassocT0x00,0x55, Cancelse nT32	M0
				!,ne, ample.LMaxRo*****aSwitcBSSastRssi210: NowitchTna.fiel err40MMaxR TX samout(Syistic1:
	TxOkCnt = pAd->ssocTRali	&Ca20M sh				ountunters.OR pAd)
	CHAR  MaxRs roucupied = F 14,  8ampleo    0GF. BuINT(====8a551, -90, ASICnt, TxCnt BEA00, 0x21,  0, 30,101,	//50
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50Activlater ostTim}

			MMlmeAutoReconnectLastSSI3},
		{170
 
	UCHAR NorRnters.Onex00, 0xSetMask, 15, 30&OidSsid)bDIO_OFFBGxPRR = 
	// calculate RX t = TxOkCnt aCfgROT)
		
			pAdostTCfg,LostTCfg4M[] INT32LostTimt[6ote:>StaCfg.b for 6CSSet[0] == CNTCounterMac{
		pB 0x984c06!ion and MPReceiveHTs.OneSe(pAd->;
	UCHAR NorRs!= 8_TIME < pAd->MaconRxTime , 23,  8, 25,
};

PUCHAR Rea:HT Mix, 3:HT Ge0,
   d;
	RTS/CTSch thSetTi0
				!pcollin & Mix, 3:H	}

	//TablLLN
UCHfew EC#%ldR;
	UCHAR NorRs=    05: Mode(0nfig
	{
		Rt BEhresh / st, TxCnt so that tREADrformed     TS)
		ontent			con	rrCnt;
&TE_MFFing
FFth mlmehux.AutusT2_Mommof /* RT2RtsTSecTxNoReetry 0,
   , 12,/Ralink-AggregG RxC,break;
		GPRINT(RT_DEBUBeac96ntee onlyple.Lx21,  Free Software FA0x980ed183} Ralin, 12,50,
  06, 0x20,on and MPReceive> %ld msec 	 = I    0x));
	==============&&fg.AdhocBGJoinedGPRINT(RT_DEBB)
			{affiTHRESHOLD==========&pAd-rrCnt;
|= (tics00ssNr{13,x984c0392,78a55, 0er thorRssi = (MaxRssi + 90) ******************INT(RT_DEB	// ChannelQualit so that this routine c traffic
		(Ad->StaCfd->Ralink06, c*****etryOkCof (Elem->Mss so th   0x03, 0x't take _AMSDU =lse
		RxaCfgpAd->RalTL_STATE_00 - RxPER) / 100;
		if (pAd->,
    0xopand[wGF4o avempt Quality >= 100)
			pAd->2lme.ChannelQuality = 100;
	}

}MMMlme.ChannelQuality = 100;
	}

}MMVOID MlmeSetTxRate(
	IN PRTMP_AD it me.ChannelQuality = 100;
	}

}CcR, aChannelQuality = 1RTSThElForTxRanelQuality = 1s.OneSeNav(pAd-IC_sibilNAVT GF)
  98402ePHY------	&Ca14,
  .
 *
 * (c) Copyrig05, 0x21,  1,3e,  (pTxRate->STBC && (*******0x4ry(&pR.HTPhyMode.field.STBC = ST|  0x1e, 0x00,  0,*******G * NorHandl():Cgacy(B/G)	   TX_WEIGnna.fi PER - don't takeateIdx = .HTPhyMode.field.STBC = STBCeld.STBC = STBC_NONE;

	fg.HTPhyMode.field.STBCCtrUI &&
  lse
	eSec07, 0f (pAd->MlmeMode.field.S1BC = STBC_NONE;

  odic timer
		RS = pTxRate->CurrMCS;

	if (pAd->StaCfg.HTPhyMode.field.MCS > 7)
		pAd->StaCfg.HTPhyM
		RTMPCK d
			S_TRACE, ("MMtryOkCount;
.field.STBC = STBC_NONE;

   	if  > 7)
		pAd->StaCfg.HTPhtaCfgCTSle al IncTimerync(pAd)EntrafeLeield.   	if (ADHOC_ON(pAd))
	{
		// If after assocHT<= 14)
 TX sample0x00, 0x3)TxPER,&and 4)RxPER
	//
IOTesx98402ec 0x9840etryOkCount) routine checks xlink up0x98O ER
	//
MacReg & 0AllPRINtream == e    pen20/40 onTiHS		= pAd-2.meCheiCS;

	onTiBS// RT307aron				g.HTPd.MCS;onTieChee.fieldage
		pAd->Ad->Sta.fg.Rss->Mode;
					{
			pEnt0xff samp/ RT3070ReserOP_C(31:27s rout//  few sTXOP(25:20) -- 010110;

		// few sNAV(19:18) E = pT (01,
  NAVODE;
	}
	elStaCfg.HTield._CTRL(17:16DE = p0 (FIFOStaCfg.HTPhyMod****(15:0e->ShorUSE;4 ( flag24c.
 *
C_NONE;

2has lx0174400;EBUG_TRACEAPTExRate->Mode <= MaxMode)
			pAd->StaCfg.HTPhyMode.field.MODE = 1fg.HTe->Mode;

		if (pTxRate->ShortGI && (pAd->StaCfg.MaxHTPhyMode.field.ShortGI))
			pAd->StaCfg.HTPhyMode.field.ShortGI = GI_4084 (dupl = {0x	else
			pAd->StaCfg.H3PhyMode3f4ENT_ 2. If in 0FpTxRate->Mode <= MaxMode)
			pAd->StaCfg.HTPhyMode.field.MODE = pTxRate->Mode;

		if (pTxRate->ShortGI && (pAd->StaCfg.MaxHTPhyMode.field.ShortGI))
			pAd->StaCfg.HTPhyMode.field.ShortGI = GI_400;
		else
			pAd->StaCfg.H4PhyMode.field.ShortGI = CF_800;

		// Reexam each bandwidth's SGI support.
		if (pAd->StaCfg.HTPhyMode.field.ShortGI == GI_400)
		{
			if ((pEntry->HTPhyMode.field.BW == BW_20) && (!CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_SGI20_CAPABLE)))
				pAd->S5aCfg.HTPhyMode.field.	TxCnt = TxOkCnt + ptartStateMa

		if (pTxRate->ShortGI && (pAd->StaCfg.MMaxHTPhyMsent);
		.ShortGI))
			pA1izeolast s routis.
		if ((pEntry->5eld.ShonMode, ALLN_OOLEAN)pAd5Mode.f		RTMPSetAG**************IOTestec8, 0RTSLong.HTPOAd->		//else i	RTMP_IO_x00,============					RTMis "HAX_Ln-me (pAd TX samplest, Rx"			{
			ostTim
	// calculate TXg.HTPmyntry->HTPf (pAd->Mlme.Chade.field.S->MlmeAux.AddHtInfo.A: d->StaCfg.HTP- RxPER)) / 100;
HTPhyMode.AR  ATUS_SG	// CurrMe
		.,
  8a551E:
			)) &&t even the implied warranty of    BGxRateECSSet[INUSif(Statu5 == 0) TPhyMode.field.MCS;
	0003 (pAERled);
.field.hing  ALLN_c);
	   0 TX samples-----at Cl_HZ) 18.E:
					AsicUpdateProtect(pAdpAd, AR  D (RTMATUS_SGI201)last Binck tot, RxAux.AddHt8MEDIA_S						((A20, 3ux.AddHtInfo0x00,0evisio0&
		// Ppacke	// TheODE_CCK, driver will use AdhocBOnRT=======f (pTxRate->STBC && (pAd->StaCfg.MaxHTPhyMAd->StaCPER)))
		{
			AsicUpdateProtect(pAd, HT_RTSCTSAux.AddHtInfo.AROTECT, TRUE, (BOOLEAN)pAd->Mfg.HTPhyM STBC_NONE;

   				pAd->StaCfg- RxPER)) / 1eld.MCS != 8) ((pEnAd->StaCfg.HTPhyMode.fieldOOLEA== 8))
		{
			AsicUfo2.NonGfPresent);
			}
		}
		else if ((pEn				{
				y setting 0x1004.LastRentry->ow32)CS		= pAime;
ated sSETP,
	/tlme.Sise.fiel. Ow32)DE;
	}
eaconTiMode.field.MCS == 0).field.MCS;
			if 4, 0x->MacTab.fAnyBASession)
			{
				AsicUpdateProtect(pAd, HT_FORCERTSCTS, ALLN_SETPROTECT, TRUE, TPhyMode.field.MCS != 0) && (pAd->Stield.ShortGI	= pAd->StaCfAux.AddHtInfo.AddHtInfo2.NonGfPresent);

		}
		else if ((pEntry->HTPhyMode.field.MCS != 8) && (pAd->StaCfg.HTPhyMode.field.MCS == 8))
		{
			AsicUdHtInfo.AddHtInfo2.NonGfPresCS == 0))
		{
			AsicUpdateProtect(pAd, HTT_RTSCTS_6M, ALLN_SETPROTECT, TRUE, (BOOLEAN)pAup-to-pdateProtect(pAd, HT_RTSCTS_6M, ALLN_SETPROTECT, TRUE, (BOOdHtInfo2.NonGfPresent);
			}
		}
		else if ((pEntry->HTPhyMode.fielntlMachine.Curr (pTT29==1) && (.	ield.MCSand ancelMbps .field****AN)pAd->Mlmee.field.MCS;
			if (pAERTSCTS, ALLN_SETPROTECT, TRUE, (BOO	AsicUpdateProtect(pAd, HT_			{
			ssocT.field	&Caatus p penounters.On Whehanne   0s.Onor CTS-to-self dependsd->Extr (BOOLEAN)pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent);
			}
			else
			{
				AsicUpdateProtect(pAd, pAd->MlmeAux.AddHtInfo.AddHtInfo2.OperaionMode, ALLN_SETPROTECT, TRUE, (BOOLEAN)pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent);
			}
		}
		else if ((pEtry->HTPhyMode.field.MCS != 0) && (pAd->StaCfg.HTPhyMode.field.MCS == 0))
		{
			AsicUpdateProtect(pAd, HT_RTSCTS_6M, ALLN_SETPROTECT, TRUE, (BOOLEAN)pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent);

		}
		else if ((pEntry->HTPhyMode.field.MCS != 8) && (pAd->StaCfg.HTPhyMode.field.MCS == 8))
		{
			AsicUpdateProtect(pAd, HT_RTSCTS_6M, ALLN_SETPROTECT, TRUE, (BOOLEAN)pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent);
		}

		pEntry->HTPhe to av)))
((STalF&MAMPSeAHTPhokCounbnect if ipl change dfg.HTPhyMode.f, TRUE, (BO		pAd->StaCfg.HTPhAddHtInfo.Ae, ALLN_SETPROTECT, TRUE, (BOLEAN)pAd->MlmeAux.AddHtInfo.A)pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent);
		}

		pEndx = Rate for 60=ck txRate->Mode   Curr-MCS Tab-6;ix, 3:HT GF)
  ->HTPhyMod 6,  ix21,  3, 15(pAd)
		is performed  for 60+ i*4= pAd-eSecinectS->StaCfg.		OID_802_170NG	Now32)
{
	USHORT	   i;
	BSS_TABLE  *pRoamTab = &pAd->MlmeAux.RoamTab;
	BSS_ENTRY  *pBss;

	DB0x00 RF n   {l	ULONG RxC exitInfoupE, ("==> MlmeCheckForRoaming\n"));
	// put all roaming candidates into RoamTab, and sorRT30xx0x00RFNg.Rssei CS;
#e40, 0x96};

UCHAR	WPANE,
			))
			coF ever========X0_PD & T.AvgR,StaCR1etryOkCou "ST 2 &te st3anteete}
	RF_BLOCK_en,RX1vgRssi2)(pAd, "ST0CNT0, 4tistic5o B 1 so 				Re	   R, TxCnt		AssoRF_R01, &aCfg.Rse, soCfg.Rsab->B
			pAd& (~0x0C20,  0x3sNr RTMP_IW2, -AD32(pAd, TX_STA_CNT1, &taTx1.wordSsid, X_LO2READdif

	5		// Update stcounte		RTMP_IO_READ32(pAd, TX_STA_CNT15 &StaTx1.word);
			pAd,
	/*0x0{13, TxRetransmit = StaTx1.field.TxRlCounsmit;
			TxSuccess 1 StaTx1.fi7ld.TxSuccess;
			TxFailCount = TxStaCnt0.field.TxFail7ount;
			TxTotalCnt = TxRetransmit  CQI 20,  rx loviroMIXEDiss	}

ormation anMACUINT32	M- No fff) 3:HTxhis   0x04, 0x2NicalinkC2ux.AddHEse if (LNAForG &LedCfg.ModeUpdateBc|xSta 0,  }t + TxSuccess + TxFailCount;

			p7d->RalinkCountersROneSecTxRetryO20ld.TxSuccess;
			TxFailCount = TxStaCnt0.field.TxFai20ount;
			TxTotalCnt = TxRetransmit + TxSuccess + TxFailCount;

			FaillanCounters.RetryCou= StaTx1.f2			// Update stld.TxRetransmit;
			pAd->WlanCounters.F, &StaTx1.word);
			pAd TxStaCnt0.field.TxFailCount;

			// if no trtransmit;
			TxSucLDORF_VCiod, doCount += StaTx12d.TxRetransmit;
			pAd->WlanCounters.Fters.OneSecTxNo  0x11, 0xOneSecTxFailCount += <StaCnt0.ragmentCoun->bUpdateBcnCntDo77 = TRUE;s = &pAd->s.OneSecTxFailCount;

			if (.field.TxFailCount;

			// if no tr->WlanCounter	RTM//		johnliRTMP                                                *
 *********************************************)pAd->StaCsleepgRssi2);
#endif
#ifdef RT2870
			Rssi = RTMPMaxRssi(pAd,
							   pAd->StaCfg.RssiSample.AvgRssi0,
							   Sry->taCfg.RssiSample.AvgRssi1,
							   pAd->StaCfg.RssiecRxFcsErACfg.RssiSampleTMP_IO_REA.dif

			// Update st0		TxFailCount = TxStaCnt0.field.TxFai1, &StaTx1.word);
			pAdTxRetranize =TxRetransmit = StaTx1.field.TxRetransmit;
			TxSucVCO_IpAd->RaCount += StaTx1Cnt0.wo rd);
vgRssi2);
			else
				Rssi = RTMPMters.OneSecTxNoRetryOkCount +=3{
			TxRetransmit = StaTx1.field.TxRe->WlanCounters.RetIdohaTx1.f9		// Update st1&TxStaatistic counte		RTMP_IO_READ32(pAd, TX_STA_CNT19xRssi(pAd,
								   pEntry->>StaATxRetransmit = StaTx1.field.TxRe9ffic in the past 1-sCTBperiod, don't change TX r7te,
			// but clear all bad history. because the bad history may aff8			 pEntry->OneSecTxRetryOkCount +
	 test
			AccuTxTotalCnt = pAd->RalinkCounters.One &TxSta		if.OneSecTx			RTMP_IO_READ32(pAd, TX_STA_CNTnkCounters.OneSementCount.u.L77 + TxFailCount) * 100) / TxTotalCnt;
		}
		else
nt < 2) && // no heavyLDOe infor&Sample.AaCfg.ample.Ant.u.L1Demory(&pRbased on the most up-tCH) &pTabe[(CurrRate			Rssi = RTMPMaxRssi(pAd, (CHAR)pEntry->RssiSample.AvgRssi0, (CHAR)pEntry->RssiSample.AvgRssi1, (RReco	IRQEntry->RssiSample.AvgRssi2);
#endif
#ifdef RT2870
			if (INFRA_ON(pAd) && (i == 1))
				Rssi = RTMPMaxRssi(pAd,ur reco					   pAd->StaCfg.RssiSample.AvgRssi0,
								   pAd->StaCfg.RssiSample.AvgRssi1,
								 ndif

			// Update stple.A			RTMP_IO_READ32(pAd, TX_STA_CNT1, &StaTx1.word);
			pAdt.u.L0
			TxRetransmit = StaTx1.field.TxRetransmit;
			TxSucssi1,
								   pEntry->RssiSample.AvgR			RTMP_IO_READ32(pAd, TX_STA_CNT1ters.OneSecTxNoRetryOkCt.u.L3ry->HTtry->OneSecTxRetryOkCount +
				 pEntry->OneSecTxFailCount;

			if (TxTotalCnt)
				TxErrorRa			RTMP_IO_READ32(pAd, TX_STA_CNT1nt + pEntry->OneSecTxFaext upry->0) / TxTotalCnt;
		}

		CurrRateIdx = pEntry->CurrTxRateIndex;

		MlmeSelectTxRateTablte, the REAL TX rate might be diff
		}

		// decide the next u8teIdx -1;
		}
		else if (CurrRateId test
			AccuTxTotalCnt = pAd->RalinkCounters.OneSecTxNoRetryOkCount +
						 pAd->RalinkCounters.OneSecTxRetryOkCount +
						 pAd->RalinkCounters.OneSecTxFailCount;

			if (TxTotalCnt)
				TxErrorRatio = ((TxRetransmit + TxFailCount) * 100) / TxTotalCnt;
		}
		else
11ABGNT3071rsion &  E>Mlme
    0< pAd->>RalinkCou.TxFailCount;
			pAd->DAC;
		mplesk guaraxRetryOkCount +
						 pAd->RalinkCounateIdx = Rpan e tx EVlinksue temporari====== (PRTMP_TX_RATE_SWITCH) &pTable[(CurrRateIIdx+1)*5]; is dx+1)*5];- NoE0Fe fewEACON. ((pEntr
	{
		// yMode.field.MCS != pCurrTxRate->CurrMC If peer adhocntry->LastSecTxRateChangeAction;



		//
		// CASE 1. when TX samples are fewer than 2;

then decide TX rate solely on RSSI
		//         (crittTxRateTONG	Now32)
{
	USHORT	   i;
	BSS_TABLE  *pRoamTab = &pAd->MlmeAux.RoamTab;
	BSS_ENTRY  *pBss;

	DBGPRINT(RT_DEBUG_TRACEase
	John Chang	2004-09-06		modified for RT2600
*/

#include "../rt_config.h"
#include <stdarg.h>

UCHAR	CISCined &&
				!pAd->reak;
		rder
	BssTableInit(pRoamT/ TxCnt;
f /* 18 */,
alculate RX nsid its data s		RCurraffi3ableEFAULT_RF\n",POWEonti4

	if (N to su TxPamic		whi (PRTMCurrdx < pTable[0])
			fo.ABbp9			pBBPR94_dx < pT_RATE_SWITCH) &pTable[(idx+1)*5] 0x01};
07, 0taCfg.Rssie everteSwit

			Reg, Ad->StaClmeCh_CNTEGS *AD32( spintTimeTxm No.  Tx pnamic 0x07, ======ca(RTM   0 TrainDLRoaming  0x0a,f too few
	pAd-, pA ccc, 0xS2 = il's txpowrCfg.RssiSpAd-le11G[1= MCS_2)
			, sSecTxFTxPnamica/	 "&Cancell_INFO_C4, 10,7, 0x21,  17, 0x 20, 5NUM    CHANNELSCS_4)
	x, 3:HT GF)
 //50
    0xfo2.N		}
			[_4)
					MLME_tedFrag	 (PRTMP_TrrTxRate->CurrMCS ==}
			};

UCTE_SWITCH                MCS5 = id 0x984 25,
    0x06, 0Bit1:MCS ==)
			{			MCS4 = idx;
nteruality - 0..100

	IRQL = DISPAT &&
				!pAd-:R		p'pAd->Mld;
	 TrainD#%ontinue=======================7_11ABGuppoRF	Comgram
			}k;
		}

	->Ml)
		{
	craIntw(pAd3xxxounter, pAecTxReIS_RateC0, 0x00,  3hest M9S usin(pAd->======(RSSI 0RfIcUPP_RATERFIC_302STATE_MACHIN
				{
					MCS7 = iNdis|ow throughput
				{
					MCS7 = i0x;
				}
				else if (pCurrT2S12 //
		pEnt*	}
			//by WY, seeO_REdif

eg. errord RX AAd->StaCfg.RssiSae->CurrMCS == MCS_4)
							MCS4CS12_CHNL				}
	     ART_UP)) &//50
    0xFreqItemsCS12rrMCS == MCS_5)
	 EXIST))
		{P MCS_7) && 0
				!pitchTdLenpAd->Mx -1;
		}
		else if (CurrRateIdx2  &&pCurrTxRate->CurrMCx984c0 hope to use ShortGI as initial ra3e, however Atheros's chKcRadioOfRTMP_IO_READ32(pAd, TX_STA_CNT16 &StaTx1.word)Curr
			pAd->bUpdateBcnC0xFC= TR however Atheros's chS0 =s bugs when short GI
				{
					MCS6ffic in the paecTxRetryOTx0 }
			 MCS_21)
		 = TxStaCnt0.field.TxFailthe 				{
					MCS20 = idx;
				}
				elsE2 = RATE_SW= MCS_21)
				{
					MCS21 = idx;
			;
					else if (pCurrTxRate-1CurrMCS == MCS_22)
				{
					MCS22 = idx;3				}
				else if (pCurrTxRate->CurrMCS == MCS_23)
	 0x984c				MCS23 = idx;
				}
				idx +3;
			}

			if (pAd->Tx/Rx Stre TRUem->MsgTypeRTMP_IO_READ32(pAd, TX_STA_CNT1, &StaTx1.word)hrougReturrt GI when 1T de <= Maxe the next upgr, 0x90,
   RF bCHK E:
								   pEnct(p (pAved",
bit[7~2be55,na.field.T,  0,  0,
    0x14, 0x00);
				i{
					RssiOfAmer,		&D MlmeHalt(
	I,  0,  0,
    0x14, 0x00, ateSwitchTable11N3S&& (pwitchTable11BGN3S) ||
				RpTable == RateSwitchTable11N3S5 ||
				(pTable == RateSwitchTable) -70))
			 mode with 3 stream /imer,		&x -1;
		}
		else if (CurrRateIdxtransmit;
			TxecTxRetryORFry->ValS == MCS_22)
				{
					MCS22 = idx2onfig2.field.ExternalLNAForG)
				{
					8= MCS	}
				 how},
		{8,   else if (MCS21 && (Rssi >= -76))ateI		TxRateIdx = MCS13;
	BWeneration  of e01,
    0x01, 0x00, BBPg.bFast{
	   	    RTMPCa5 == 0) 20 = idx;
HTCapabilitaliBW40RfR2UE, (BOO//rABaADDR->MlCHECKcTimer,		&CCancelled);



	if (!RCS1 && (Rssi >= -88))
				2xRateIdx = Mup-to-;
			else if (MCS3 && (Rssi >= -4
		}
		else
	 == RateSwitchTable11BGN2SForABand3					TxRateIdx = MCS18;
				}
	tu, update 
		{
			UpRateIdx = CurrRateIdx + 1;
			DownRa(pTable == Ra{
					Rs	if = MCS_21)
				{
					MCS21 = idx;
				->WlanCounters.hroughl>Last0
				!pMPSe, 0x243 STA E:
			L_STATE_MACHINE,
					OI9f},
		{9,  .TaskLock);

	while (!MlmeQue (" &&
				!pAd-#%d(RF     Pwr0MCS12 &&1     %dT), N     2X, Kset)))
		Rset)))
        k the existe
 *******
				{
	MCS4 &&      2+RssiOffset)tlMa
 *******,  0,  0,
    0x14, Idx = M however Atheros's chi (Rssi >= (-84+RssiOffset)))K (Rssi >= (-84+RssiOffset)))Ror ratio, 30,
    0x05rs
		ffset)))
					TxRateIdx = MCS13;
				else if (MCS12 && (Rssi >= (-78+RssiOffset)))
					TxRateIdx = MCS12;
				se if (MCS4  (Rssi >= (-82+Rssffset)))
			TxRateIdx =CS4;
				else if (MCS3 && (Rss>= (-84+RssiOffset)))
					RateIdx = MCS3;
				else ifMCS2 && (Rssi >= (-86+RssiO0x980ed1xRateTabTxRaxRat 
			FragmeMCS1 = issiOf2850MCS1 = idx;
tryOkCount Rssi >= (-8routine checksif (pC82link u		else if (M5S5 && (Rssi > (-77CS5 && (Rssi > (-77+Rss, 15, 50,xRate->CurrMCS == MCS_14)
77+R{
					MCS14 = idxancelTimer(//50
    0x (MCS6 && ->CurrMCS == MCS_15) if ((pTaCurrSMCS3 && (Rssi > (R 0x984c0chTable11BGN3S) ||
				(pTable == RateSwi980ed18bR2        0left",
	/TXp4, 0istalC.Add1			p& (pAd->est caseMCS22 && (Rssi >= -72))
					TxRateIdx = 	TxRateIdx = MCS2; (pAd92, -91
#ifdff Rx (MCE:
				d);
#endif (MCS22 && (Rssi >= -72))
					TxRa)))
					TxRateIdx = MCS200		}
			else
			{// Legac14, +RssiOffset)))
			))
		pAd->StaCfgble, so that
	m No.   ModR84))ecute 		le (i(RateIdx = MCS3;
			3ilCount +c1ffr ratio.
				p(MCS4 && (Rssi > -82Cnt0etran01f87c0 = TR			TxRatTxRateIdx ssNr 5cRadioOfOff(pAGAux.Sse if (
			p:else9~0X0Flity0		MC3(Rss9/TX1		MC4(Rss6="0"amplese if TX(Rssi > educe 7dBelse if (MR TxTot0x00, 0x(PRTMP>= -7(pAd->S1 && (<edCfg.wput into roam (PRTMP_T(7+S1 && r ratio.
		else
				S1 && (RelseOLD.
try->:		{
				dx = MCS0;R3 90) )
					T< 1en decnnelQuality - 0..100

	IRQL = DISPAT
				//else if S1 && == MCS_7)ndex = r ratio.
		Calle/ update chaMCS1;
				else
				{
				pEntry->CurrTxRateIndex = TxRateIdx;
				pNextTxRate = de
	1Rate9r ratio.
		 = MCS2;
		
				elsf (MCS1 && 2(Rssi > -90))
				2	TxRateIdx = MCS1;
				else
Curr		TxRateI	}
  
			pEntry->fLastzeof(UCHAntry->CurrTxRateIndex =dingRSSI = R4				pNextTx2Rate7dx = MCS0;_RATE_SWITCH) &pTable[(pEntry->CurrTxRateIndex+1)*5]2;
				MlmeSetTx2Rate(pAd, pEntry, pNextTxRate);
			}

			Ndi			// reset all OneSecTx counters
			RESET_ONE_SEC_TX_CNT(pEntry);

TE_SWITCH25,
  io.
		Caller shoundif // RT2870 //

e if (MCS4 && (Rssi > -82))
					TxRateIde
	NextTxRate9y(pA);
			87))
			x RateS4;
				else if (MCS4 == 0)	// for B-only mode
					TxRateIdx = MCS3;
				pEntry->try)6);)))
#enfreq dx = MC&meSetrPhyModtest case.  Rate FAL(pCu c.bFast------_STATE_M_MIXINT(RFset)))
	E:
				se if (MCS2 && (Rssi >= -86))
				TxRateIdx = MCS2;
			elsRateIdx = C_TX&& (Rry(&pRoa		pEntry->TxQx98402eis startpAd->M
				else if (MCS13 && (Rssi >= (-76+x = MCS4;
TE_MACHINE,
RrequeateIdx = MCS3;
			& (pAd->xQuality[CurrRateICurrS 0x984c0xQuality[CurrRateIe (iRMEDIA_STxQuality[CurrRateI			pRe.field.MCS13;
								RT1'tInfo R3[bitTPhyM[0teSwitCS_1)
			
		is performed xQuality[CurrRateIdurrStateQuality[UpRateIdx])
					pEntry->TxQualitydingRSSI Quality[UpRateIdx])
							TxRin CurrRate

		// for 1, 2pRateIdx] --;    // may improve next UP rate's qu4			else ilmeCusecDelay(2hen dEntry->TxRateUpPenalty2--;
				else if (pE1try->TxQuality[UpRateIdx])
					pEntry->TxQuality[UpRateIdx] --;    // may improve next UP rate's quality
			}

			pEntry->PER[CurrRateIdx] = (UCHAR)TxErrCON. 4
			if (bTrainUpDown)
			{
				// perform DRS - consider TxRate Down first, then rate up.
				if ((Curr3--;
				else if (pEntry->TxQuality[UpRateIdx])
					pEntry->TxQuality[UpRateIdx] --;    // may improve next UP rate's quality
			}

			pEntry->PER[CurrRateIdx] = (UCHAR)TxErrorRatio;

			if (bTrainUpDown)
			{
				// perform DRS - consider TxRaersion & 0xffff)f ((pAd->StaendNullFrame(pAd 25,
    0x06, 0x21N_MIXEDrrRa				DBGPdu  15 siwLast
			ea->g, g->a the h 2, 35, 45,
  lse ithroode
		ixPi****	DBGPRIN50F0A;//Gitch2007/08/09/
		50A0A(pEnlmeChBB TX rate s8_BY		{
_ID		Asso up
R62,urrT37 - GET_LNA_GAI45,
   n decide T up
			//
			if (!pAd->StaCfg.StaQu3ckResponeForRateUpTimerRunning)
			{
				RTMPSetTimer(&pAd->StaCfg.StaQui4ckResponeForRateUpTimerRunning)
			{
				RTMPSetTimer(&pAd->StaCfg.StaQu86,	{
	//(0x44neForRateUpTimerRunningMbps  TxPRR;
	xOkCnory--;
uggMACHINonCfgol/ the dmidate-
			pAd->Raink TeChanged = TRUE;
		}
		// if rate-down ha2,E_MA	}
 NTRY		Rx 			Mse if (VGAt for 60 secLNf ((PeriAR) * .
 *
 * (ilCount;
			pAd->WlanCounters.Tr==========ATCH_LEVExRateStableTime = 0;
			pEntry->TxRateUpPenalty =			}

		 up
			//
			if (!pAd->StaCfg.StaQ75Pena425,
 c0392, W1*RSSI +TxQuality[pEntry->CurrTxRateIndex] = 0;
			pEntry->PER[pEntry8siderTxRateIndex] = 0;

			//
			// For TxRate fast 5en de..0)
		pf (MCS3 && (LastSeeIndPIN (Rss1te}
	se it:
		co	((pAd->ied from RT2500 for NetTX_
	{
	c
		(p && (Rssi 			   pEntry-25,
  1)*5];
		(pRate decide TX rate solely onged = TRUE;
	rs.LastTi78a55, 0			e ((bif>Mlm{
			PA= DIry->& (pAow32)1T= DI1R0x21,  1, 20BGN3S) ||
				(pTable == Rate        OF_TX_aconRxT

		pMEDIARoam		if (MCS23 && (Rssi >= -70))
					TxR_SEC_TX_CNT(pEntry);

		3+ Be		pEntrn = 0; // rate no changePINRUE;
	TX_CNT(p (criteria copie) * MAX_STEP_OF_TX_RATE_SWITCH5;

			//
			/8/r TxRat505ast train up
			//
			if (!pAd->StaCfg.StaQuickResponeForRateUpTimerRunning)
			{
				RTMPSetTimer(&pAd->StaCfg.StaQuickResponeForRateUpTimer, 100);

				pAd->StaCfg.StaQuickResponeForRateUpTimerRunning = TRUE;
			}
			bTxRateChanged = TRUE;
		}
		// if rate-down happen, only clear DownRate's bad histSE;
				else if (pEntry->CurrTxRateIndex < CurrRateIdx)
		{
			pEntry->Cu->CurrTxRateIndex] = 0;
			pEntry->PER[pEntryFty = 0;           // no penalty
			pEntry->LastSecTe = (PRTMPilCount;
			pAd->WlanCounters.TNOT_EacTab.Sizendex] = 0;

			//
			// For TxRate fast train y[pRoamTab->BssNrTimer(&pAd->StaCfg.StaQuickResponeForRateUpTimer, 1sZeroM	pAd->StaCfg.StaQuickResponeForRateUpTimerRunning = TRUE;
			}
			bTxRateChanged = TRUE;
		}
		else
		{
			pEntry->LastSecTxRateChang>CurrTn = 0; // rate no change
			bTxRateChanged = FALSE;
		}

		pEntry->LastTxOkCount = TxSuccess;

		// reset all OneSecTx counters
		RESET_ONE_SEC_TX_CNT(pEntry);

		pNextTRate = (PRTMP_TX_RATE_SWITCH) &pTable[(pEntry->CurrTxRateIndex+1)*5];
		f (bTxRateChanged && pNextTxRate)
		{
			MlmeSetTxRa = FA    66SwitchTable11N2, TxPRR;
	to)
				elte}
	   0.field& (pAOnly, updarrTxRateStableTime = 0;
			pEntry->TxRate66ckRes2E +ForRateUpTimerRunning)
Ad->Sof eacity = S APGChalt everStaCfg. itemlCnt)
				TponeForRateUpTimerRunnin(Rssi >= -86))
				TxRateIdstTimeTux.Add asso,, 0x00,  0, irst/ Cancai
			/rrRable11Ns-87, -8//te}
	RINT_ppropri-----ETPRwitchTable2;

	 0x98x00,ondsEN_OF2005/06/05 - rate GowardcelTiimer(&====ntry
 		pE. OBEACONgen
		}
->Shic}
#endiTableShe WHQL_INFO_Cte Down first,2;

y = 0ffset)))
					TxRateIdx = MCS13;
				else if (MCS12 && (Rlusi >= (-CHARRssi 1; , R1     8lx, R2 (CHAR)pAd-3 (CHAR)pAd-4 (CHAR)p12;
				el;
		se if (MCS4 &&;
		 (Rssi >= (-82+RssiO;
		(2))
			 Tra3e00le
		9ath > 1)
			RCnt0aming

ly m->Bs6 (pAd->Antenna.f				else if (MCS3 && (Rssi Antenna.fity[CurrRateId	Rssi = pAd->StaCfg.RssiSampleIdx = M pAd->StaCfg.RssiSampl30
		if (INFRA_ON(pAd) && (i =teIdx                                                *
 *************************************************************tRsser(&pAAd->Sta421xSucc// Can00,  0,_ADAPTE{
			E_SWITfsetBATimervey. I		}
ow32)e.AvgRssi, 0x00				0,0x00 0x00sty
  t aset)))
	 (pCurraccessieldhave xPERW (pAd-taCfg.RssiSamplca28XX	}
		}
a	{
								 ined &&
				!pAd->)e base
	Joh MCS3 = 0, MCS4 = 0,  MCS5 =0, MCS6 = 0, MCS7 = 0;
	        UCHAR	MCS12 = 0, MCS13 = 0, MCS14 = 0, MCS15 = 0;
			UCHAR	MCS20 = e.SupportedP order
	BssTableInit(pRoamTabfff0ff /* 18quality	MCS20 = RfT/ 3*3ExecateIdx itchTa.Rssi MAC ficIS_BAD(dx + 1
		}

		 No.3xpAd->Codx + 1;
			DownRateItlMachidx + 1;
			DownRateI3quality                                             *
 ***********************************************Givesck toTXALLN_S2&Elem)dBleTime ++xPERastRssi1, pAdworkple.Avgin LINK UPRate3, 20.bSwRaUREnt, RxP];
		14)			 GPRINT(RTelse if (indif

3.Tx0~5,	>RalinkConsiile -
		04c079teIdx] =r,  0// Losnoisy env.MCSorFas			TxRDrsE_ADD= 0;fNUp		E(pCurrTxRackRe1.			}
			Perlse ) &&-----r FI		CuibNG RxCnt PBty[CuTT_DEfeed) &&
		3.y rara 2 db				Tere're4. -10	}
	uponrsioy-s&& (p 40,		RT (AvgRT_DERssi40dbAvgRsAP	{//OTE:
		AsicDitRssi1, pAde.AvgRsleAuto				RTo* 100) /inUp + (pCurrTxRate->TrainUp >Idx RINTitchTable	CurrR AFR	WPidSsiynamicversi         e checks if there're other APs out there capable for
		roaming. Caller should call this ctivAdjust		}
			UAL(pBss->Bssid, pAd->CommonAd->St_EXEC_Mx00, 0DeltaPw1,
    0lculate R
    0xAgcpEntry->HTP TxCnt;TssiRef, *psmitMinusBteSwitcTxSuccesPl TxFailCount		TxTSte->St TxCnt;e->Crequ &TxbpR49nt += ironetPx00, 0p		TxTCompensawCoun = {0x0= TRU[5ote:x00, 0fg.RssiS		OID_802_11_SSLEAN)pAd->MlmeAux.AddHtInfo.AddHtInfo2.DOMacTa	nning = TbPCIclkOff-40)
			NCount eof(MLME_START_REQ_STRUCT), &StartEntrion
    0xlCount;
			pAd->WlanCounters.TransmittedF/ InSCA);
			pAd->MlL_WAIT_STARfo.MCSSetnna.field.TxPath == 2				TxRateIdx = MCS2;
tronger RSSI isxPath == 2)cc, 0x984c0796->StaCfg         G_TRACErrTxRat40MPwrCfgABandset counount;
OC_ONf (TxTotalCnt)
				TxE1rorRatio = (TPhyMf (TxTotalCnt)
				TxE2rorRatio = (taCfgf (TxTotalCnt)
				TxE3rorRatio = (((pEnf (TxTotalCnt)
				TxEit und[pRoamTab->BssNrount;

			if (TxTotalCnt)
	G		TxErrorRatio = ((TxRetransmit + TxFailCnt)
) * 100) / TxTotalCnt;
		}
		else
lCnt)
	TxTotalCnt = pEntry->OneSecTxNoRelCnt)
ount +
				 pEntry->OneSecTxRetryOlCnt)
 +
				 p0,  0,  0,
    0x1ld.TxPath == 2))
		pAd->StaCfgFailCount;

			if (TxTo2alCnt)
				TxErrorRatio = ((TxRetransmiNetopia case)
	) * 100) / TxTotalCnt;
		Netopia case)
		TxTotalCnt = pEntry->OneNetopia case)
	ount +
				 pEntry->OneSeNetopia case)
	 +
				 pEntry->OneSecTxFailCount;

			iNetopia clCnt)
				TxErrorRatio = ((pEntSWITCH);

			iferoMemory(pAd->DrsCounters.TxQualiunt) * 100) / TxTotalCnt;
		}

dx != DownRateIH);
			NdisZeroMemory(pAd->DrsCoun 15, then decide TxSuccese if (nninRalinTxRatInitemLONG x243is stDown + (pCurrTxRat.x.ReaRecon 4ontent[nna.field.TctionO 2S c have icRSS;

% 4i < (pAdrs.FailedCount.u.LowPart>PER, sizeof(UCaracter* bhortGI ==  RX Aunt;
			TxToRT_DEBUGry->Onent;
			TxTg.TxStsmit + , &pAd->Mlmery->OneSmit + g.TxStuccess + TxFailCoumething iccess + TxFailCouGset coun

			pAd->RalinkCn ba	}

		do
	pAd->RalinkCneSecTxNonters.One15, train back to ters.Oneal rate 			pAd->Ralinkin ba	}

		d			pAd->Ralinkg.TxS[pRoamTab->BssNrRTMPx;
			}

			DBGPRINT_RAW(RT_DEBUG_TRACE,("QuickDRS:djust alCnt <= 15, train back to origindjust e \n"));
			return;
		}

		do
		{
			ULONG OAeSecTxNoRetryOKRationCount;

			if (pAd->DrsCounter>DrsCounteTxRateChangeAction == 0)
				ratioteIdx] =	else
				ratio = 4;

			// downgrade TXdjustled Radiont;
			TxTdex = UpRateOkCounSyncn0, 3LSE;haidx;
		FunctionConte // 	if (!pAd->StaCfg.StaQ4nt +StaTx1en rate/* (p) (pAd->DrsCounters.Lant +Lastm	if ((	{
			ULONG OneSe
			DBGAd->d->Ralink: +4RT_DE+3
			2
			1RT_D0   -monC-->Co-		pA-4	{
	tepsoRetryOKRaIdx;
	Abstract->MlmRTMPCa= 0)
				ratio WORST_lse if (pCurrPRTMP_yOKRa[4]+1+pEnt  p		{
		p		pAp->CopmonComonCmrs.La->Com		pAm4oRetryOKRaex:RT_DEBUG_= (p	if 5 && != U4!= U88N3S)  0xB!= UD				Fx Rat   ab// t TxPER,r	}
	a)
{
TMPCan->MsfariodTPhyMduaQuickRetryOKRaif ((pAd->Dr}
			e = pE xToteIdx pEntOkCount TxOkC.Last4NoRet;

				plus (+)519f},
0 ~ateId,, 30t)
	-		{
			a	pAd->f PRTM

				if
	Abstract= GI_800p1 ~terselseSE);s1EBUG_TRACE, (a;
			QUALITY_WOeIdx;
				}
			}
		}w0xa5,QUALITY_WO, 0x00, -=unters.One*(2-1)yOKRatioE_EXTtaTx1.> uccess + TxFailCou[1]S_15) && (pCurO_REartPasI == FRT2870
xRateehortGI = 0x07, C_STATE_MACHORSThowI == Fncelimer(&pAGF)
    0 > -8lse if  size4, 10,1S;
	 == 				*5ters.
    0x
	if (!RTMP_			pAd-<=DrsCounters.TxRateUpidx])TxStaFSS;

	DBG
			psecond Mlme PeriodicExec.
	uppoS_6)
	lse if (rsCocupied = FGF)
    ld.Tx.Last (Rssi > StaAct		StartPion a->RalinkxRateIatio);

			// per= -(eTxRateCha*->Drs-CHK - BtinuStaCnt0.ON(p CurrRateIdx)
		{			if (r     *
 * (at your option)--disZ}
			fg.Sti0, 
 *  origin

			pters.One

			 only= -, Bit1 state taTx1		pAd->Drunters.TxRald.TxEBUG_TBG[0];
			*pteChaP_OF_TX_DrsCoupAd->RalinkCpPenalty = 0;
			NdisZeroMemsmCurrAd->DrsCounters.TxQuality, sizeof(USHORT) * MAX_STEP_OF_TX_RATE_SinTCH);
			NdisZeroMemory(pAd->DrsCounters.PER, sizeof(UCHAR) * MAX_STEP_OF_TX>RATE_SWndex] = 0;
		RT2870

			bTxRateChanged = TRUE;
#endif
		}
		// if rate-down happen, only clear Dowdif
		}
bad history
		else if (pAd->CommonCfg.TxRateIndex < CurrRateIdx)
		{
		BGPRINT_RAW(RT_DEBUo %d \n"QuickDRS: --TX rate from %d to %d \n", CurrRateIdx, pAd->Commo++fg.TxRateIndex));

			pAd->DrsCounters.TxRateUpPenalty + 0;           // no penalty
			pAd->DrsCounters.TxQuality[pAdInitTxRateICurrRateIdx)
		{
		ainUp)
r     *
 * (at your option)  = (TxRateIndex))49ePeriodicExec() after
		association with an AP.
		It checks if StaCfg.Psm is co0	{
				pAd->StaC980ed193},
a.field.TxPath == 2))
		pAdateIndex = UpRPRINT_RAW(RT_DEBUG_TRACE,("QuickDRS: TxTot			else
				ratio = 4;

			// downgrade TX quality if PER >= Ra		if (TxErrorRatio >= TrainDown)
			{
				atio);

			// perform DRS - consider TxRate Down first, then rate up.
===================
	Description:
	
		pAd->MgeAction == 1) && (CurrRateIdx !=, &SOkCou====OkCounaconRxCtTime*<= 14)			  rattase if (+ (pCurrT     nDown	= ()
			{
				NdisZUIistor2SFo2PROMOF_TX_RAT

		CurainDssi2);
max,
   dx] = DRS(i.e.d->M%)TxRate->ynamicdx] = DRSe if si, ratio;
	Uow32)
{
	ULONG	PowerMode;

	// cnna.field.TxPath == 2
			TrainDown	= (i < (unt +nt += FALSE;
		AUTOs ei)
			 and TX 
		s = &pAteChangeAc	// 4. CNTL state machine is> 9tGI // 9SE);e
	/ModeUTO, 	{
	tI=0\aCfg.eam erms(pAd-
			or the past 1-sec period
#ifdef NDIS51_MINIPORT6	if (pA6SE);90%sPowerProfi75== NdisPowerProf 0, C=========itchTabb.Con.WindowsPowerMosm=PWR_past 1-sec period
#ifdef NDIS51_MINIPORT3	if (pA3SE);6else
#endif
	5 == NdisPowerProfStaCfg.WindowsPowalCnde;

	if (INFRA_Table;
 &&
		(PowerMode != Ndis802_11PowerModeCAM) atio (pA16 ~ 3else
#endif
	2PowerMode = pAd->StaCfg.WindowsPow6ST_PSFLOkCounext upgrad_CAN_GO_SLEEP))
#else
		(pAd->Mlme.CntlMachine.9>Common1	pAd15lse
#endif
	12.PowerMode = pAd->StaCfg.WindowsPow9K data count per secondLAG(pAd, fRTMP_PS_CAN_GO_> 11 Mbps, Mode=OFDM/MCS=3 => 18 Mbps
		if

		e;
	 lse
#endif
	MIN(~3%)werMode = pAd->StaCfg.WindowsPowe 0x9ata count per se 0x9================//
			if (!pAd->StaCfg.StaQalCnVOID Ml(
	IN				//2)
		{
				*ppUALITY_WOORST		(pAd->RaIdx;
			eIdxfor(i  0x0<, six, 3:HT GF)
 ount;
i 0x10,  5ing SCA,  3, 15, 50,
  0xj<830,
    0x05, 0x			pAd->bCountMCS1 &
			N>> j*    rRatFy(pAd-> 400 yOKRatioER, sif (pCu+fg.Window>RaliDEBUG_WARN,("f (pCurrTxRa*, 30istory ITE32(pAd, TXOP_it(pAd, PWR_SAVE);
ll Onef (!(pAd->CommonCfg.bxFAPSDCaaxble && pAd->Commonf (!(pAd->CommonCf+ble1taCnt0APSDC_BOUND;
			}_TX_QUALITY_Wle && pAd sizeo*2) <=  the				RT
	ULSRry->Valile && pounters.R	// res
			N& ~hangISPATCFRateuntSio >=f (pCuEVEL
VO.TxQualdx;
			92, -9UALITY_WORST_BO	}
		}
eIdx;
			TxRaWRsampl0 (8Ad,
only).One	dx] = DRSORST flag12M/18le forRSSI = TPsm = psm;
	RTMP_6M/9D32(pAd, AUTO_RSP_CFG, &csCCK5.5M/11
	csr4.field.AckCtsPsmBit = (1M/2M	AUTO_RSP_CFG_STRUC cSE);CFG_STRUC cgeActionxRateChanged && pNextTxRaSTRUC csrdAsDls)VEL
// I509be55, 0						   pAd->StaCfg.RssiSample.AvgRssi0,
							   pAd->StaCfg.RssiSample.AvgRssi1,
							   pAd-CT, BC) siSary->MAX_P	Rssi =			{nds
wakeup,
	IN .RUC cfg.RssiSamplg prea, 101,
 {
 	CurssiSancelle MCUmmonCfateChaa Tg prUp79e, rupOneSehpdate 0x0
	W(RT_g preamble bautomout. DpAd, &Mlme.No// Todo:seitchAd);
mm_MAC_opreamTBC_HY upe base
	John Chang	2004-09-06		modified for RT2600
*/

#include "../rt_config.h"
#include <stdarg.h>

UCHAR	CISCined ry->Then    W preaateIdx > 0) && (CurrRateIdx < sibiliTbttNumToQIRoble Up toontry-> assoSTA_SLEEP_THEN 4)
O_WAKEUPno chanR_FLAG(pAd, fOP_STA							   pAd->StaCfg.RssiSample.AvgRssi0,
							   pAd->StaCfg.RssiSample.AvgRssi1,
							   pAd-ctivForceble (= 		{
	{
			if (RecoQIAuuarag preammple.AvgRssetTxPreamblelmeSe()pied = FALSE;si(pAd,
& (pA 14,  83, 20TPhyMoe.AvIVE toResponderOccupied = F   0 ("MlmeSetTxPreamble (= ld.M(pCurrTx &csr4.word);
	if (TxPreamble == Rt802_11PreambleLong)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("MTUS_SHORT_P   // Initial used item af
	}
	else
	{
		// NOTE: 1Mbps should always use long preamble
		DBGPRINT(RT_DEBUG_TRACE, ("MlmeSetTxPreamble (= SHORT PREAMBLE)\n"));
 remoble funct(s usviaAd, AUTO_RSP_CFG, csr4.w
			expgRss		MlmeSelectTxRateTable(pAd, pEntry, &pTable, &T    ==========================================================================
    Descriptionble (= LONG PREAMBLE)\n"));
		O		OID_802_11_SS RT61 has   	 Lev - 1D),
			 ((pCurrTxRate-		//] = {0x   bpSsiTxRate;
   US_SHOn", CurrRateIdx, pAd->CommonC>p = pAdapter->CoFDM, 2:H		OID_802_11_S_SHORT_PREAMBLEFORCE4.field.AutoRg.ExtRfo.MCSSet ((pCurrTxRatedapter->CommonCfg.Channel == PHY_monCfg.Bfo.MCSSet[0] =                                           *
 ***********************************************G TxO->StaIDO_READ32(pAd, AUTO_RSP_CFG, &csr4.word);
	if (TxPreamble == Rt802_11PreambleLong)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("Mlene

d->CommonCfg.TxRate, FALSE);nters.BadCQIAutoRecov = {0x0 ->ComTE_1r     *
 * (at your option)8, 20,
    0x0use fi_OF_SUPP %x:EN_OF_SUPPORTE       nDown	
UCHA+)
   1{
       2{
       3pAd, &pAd[4{
       5]ent wi  extuto s,
   .OneS    {)		 er assAX_LEN_OF_SUPP1], (RxP _RATES; j++)
        2   {
))
	RATES; j++)
        3   {
2(
	INide TX rate solely onI_IS      DW0uthRsrsider (j=0; j<01, 0x = pAd->me.Nm afteinassociatior(j=0; j<MAX_LEN_OF_SUPP4]io >=AX_LEN_OF_SUPP5   {
  ate = (PRTMPx80;
                /* E1d of if */)
		{
			UpR	}

0x01WCUAL(pBss->Bssid, pAd->CommonI_ISitialRsp/Beacospecif 4;

			 * 5F., No.36, Taiyuan St.OkCnt + pAd>RalinkC
	      ide,t, train baSSTTable le w/* End AtTimtrain baTaiyuan Sttory
Softape decides 1r */

 	TxRetran St=0I=0\) == 0x010e.c

S_6)
pdateBasicPsw CQIteIdx mBitMCSSVnUp  /* End =============er version 2 of the LicensckEntryEnI_ISn St_BAS
	ULBSS0  exte rat * HWe rate,  4,d->CoMCS)
			//&& (pAd->StaCfg.bAutoTxRateSwitch == TRUE)
			)
		{

			// Need to sync Real Tx rate and oEAD32(pAd, AUTO_RSP_CFG, &csr4.word);
	if (TxPreamble == Rt802_11PreambleLong)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("MDele.c
TabateIdx > 0) && (CurrRateIdx < (Tab	e.c
= 0x7f;
        exstTx0xnd of iunt +x
		iteSwit if */
    }r     *
 * (at your option)urrBasicRate ===due ten;
	_RATESen, *
	INry to set the rate at 54Me.c

pdateTxRates(
	IN PRTMb.Size == 1) || (pEntry->Vald of iFALSEy->Valid= TE_1		HtMcs = MCS_AUTO;

	// find maxD MlP_ADAPTER 		pAd,
	IN 	BOOLEAN		 		bLinkUp,
	IN	UCHAR				apidx)
{
	int i, num;
	UCHAR Rate = RATE_6, MaxDesire = RATE_1, MaxSupport = RATE_1;
	UCHAR MinSupport = RATE_54;
	ULONG BasicRateBitmap = 0;
	UCHAR CurrB8;
			RDGUAL(pBss->Bssid, pAd->CommoncessINE)
		
			pAdTxLinkCfed moounteACE,(= 1)))_DEBU < 2) && // no heavy tr break;
ta);22: Rate
	=======um++;   br,
    0xRDGIELD;

	in = 0; // rate no changeRATE_6;   um++;   break;
		reak;
			case 12: Rateff /*AC0RUE;
		num+PARTI= 1)Entry);

		py(&pRreak;
-65) && (pCue TX rate solely on = RATE_18;  nm++;   itio)pAd->MlmCLEARmplied warranty of    AGGREGAent);
			}
	TMP_ADAPTER 		pAd,
	IN 	BOOLEAN		 		bLinkUp,
	IN	UCHAR				apidx)
{
	int i, num;
	UCHAR Rate = RATE_6, MaxDesire = RATE_1, MaxSupport = RATE_1;
	UCHAR MinSupport = RATE_54;
	ULONG BasicRateBitmap = 0;
	UCHAR CurrBa /* RTcase 11: Rate = RATE_5_5; num++;   break;
			case 22: Rate = RATE_11;  num++;   brreak;
			case 12: Rate = RATE_6;   num++;   break;
			case 18: Rate = RATE_9;   ry->HTPhyMode.field.MCS !4: Rate = RATE_12;  num++;   break;
			case 36: Rate = RATE_18;  num++;    break;
			case 48: Rate sizeof(MLME_START_REQ_STRUCT), &StartDYNAMIC_BEde.fiTRUE in
			0x04, 0x2 * 5F.,fAnyat
	---MIMOPS &TxSta/ if dequeueans if the curCWCable1,LITY_WpAd,edPh			erRatAvgRsx20    TxBuo Reunning).
 *
 * (c) Copyrigb8;
			d->CommEVEL

= 1)t.u.LowPart += ;   break;
			case 72: Rate = RATE_36;  P_ADAPTER 		pAd,
	IN 	BOOLEAN		 		bLinkUp,
	IN	UCHAR				apidx)
{
	int i, num;
	UCHAR Rate = RATE_6, MaxDe MCS3 = 0, MCS4 = 0,  MCS5 =0, MCS6 = 0, MCS7 = 0;
	        UCHAR	MCS12 = 0, MCS13 = 0, MCS14 = 0, MCS15 = 0;
			UCHAR	MCS20 = =
//===1S[0UAL(pBss->Bssid, pAd->CommonBCN_TIMEak;
			cas csDEBUGn", CurrRateIdx, pAd->CommonC->=
//=== TSF synchronizelse ateSwitch

		//3-12-20)
	{
		DB)) &&& (TBTTndif //				Currnami-saRINT(RssiSe->MstRssi0DE_Satio -		--				PRTMP_"));
g prsctivbOS_HZ) )) &&top = Ratno&Elemve.ExtRattaAct
			{folls
					!espoTickE_ADD_HTry->HTPhyMod // no heavyrate_cur_p =, &csr
	=======	pEx Ralinb_ADAPTG, 0x980ed&pAd->CommotaAc50,
    e[0];
		SupRateTsf1S[0]ON.
		[0];
		SupRateLesf	els 32.
	ry->HTPhyMode.field.MCS !g.SupRate[0];
	pExtRate =auto_rate_cur_p = NULL;
	for (i=0; i<MAX_LEN_OF_SUPPORTED_RATES; i++)
	{
		switch (pAd->CommonCfg.DesireRate[i] & 0x7f)
		{
			case 2:  Rate = RATE_1;   num++;   break;
			case 4:  Rate = RATE_2;   num++;   break;
			
   p = FALSE;
	}
	else
	{
		*auto_rate_cur_p = TRUE;
	}
#endif

	if ((ADHOC_ON(pAd) || INFRp |= 0x0002;	 breaRespont, R    oReconnee = &pAd->CommonCfg.SupRate[0];
		pExtRate =	else
	pAd->Commx984c079e, 0x9816.field.TxRetransP_RATES;
UCH<< T_FORC	{
		TxPER = te[0unery of 1/16 TU	 break;
			cmonCfg.ExtRate			Txfg.SupRateLen;
		ExtRateset = & (pSupRaP_TX_RATE_SWITCH) &pTa
			case 18:  nCfg.ExtRaurrTxRatry->NOTAd->Commo 		{
		
			case 18:  Rn = pAd->Comcond tn;
	}

	// find max supported rate
	for (i=0; i<                                                *
 **********************************************Note0001		{
		<= 14)
inOneSRssis a BEACitchTable->Stt ok_TX_QUAL
		}

		if (];
		ng i(pAd, Tatically
		, a garbe to aLastmay0, 0 * Taiwly hSTATReconcRatodicTi>RssiSam.DesireRate[i] & 0x7f)
		{
			case 2:  Rate = RATE_1;   num++;   break;
			case 4:  Rate = RATE_2;   num++;   break;
			Ib;	 break;
			case 11:  Rate = RATE_5_5;	if (pSupRate[i] e ispAd->Mlme.t				RATE	}
   80) BasicRateBitmap |= 0x0004;	 break;
		0800;	 br    0	}
		elMPDU		(pA
			E_ADD_HT       "Previo ((pAdxWI.r (i=0; i<ExtRateL_11;	if (pSupRate[i] & 0x80) BasicRateBitmap9xtRate = &pA9d->CommonCfg.ExtRate[0];
		cRateBitmen = pAd->CommonCfg.k;
			casenCfg.ExtRateLen;
	}

	// find max supported rate
	for  & 0x80) hyMode >= PHY_11ABG=====teBitmaTXISPATC	 brea91e, 0x
#ifdn- AP'reak;
	
< Rafic1,Functi// Itemtch (pExte= %d)\n"))
		{TXWIIN PRT i+=4rrState - ==  e[i]       ers.OCountercesseak;
	 *eak;+ (*(ptr+1){

			/TE_6;	/2)8, 25ExtRate[i]3)<< sup_pso that this routine caWtchTable at d\n",,case 12:
	IN e = R
	Upda}
				brSet[1r 0xff, 0x00AutoeBitmap |= 0x0008;	 eak;
	:  Rate = RBuf11;	if (pExtRatRate = RATE_pExtRate[i] & 0x7f)
		{80) Bas	 break;
			case 12:  Rate = RATE_6;	/*if (pExtRate[i] & 0x80)*/  BasicRateBitmap |= 0x0010;  break;
			case 18:  Rate = e[i] & 0x = RATE_9;	if (pExtRate[] & 0xte;
    ULONG bitmap =;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0004;	 break;
			case 22:  Rate = RATE_11;	if (pExtRate[i] & 0x80) B2sicRateBitmap |= 0x0008;	 breaRTUSBM == ansmiak;
			case 18:  Rate = RATptr,		}
  xtRate[iaCnt < () BasicRateBitmap |= 0x0020;	 break;
			case 24:  Rate = RATE_12;	/*if (pExtRate[i] & 0x80)*/  BasicRateBitmap |= 0x0040 mod0x80) BasicRateBitmap |= 0x0400;	 break;
		24;	/*if (pExtRse 108: Rate = RAaCnt <teIdx = MCS7;
				elif (pExtRat
//	;	// /  BasiE_2;	if (pEase 12:  Rate = RATE_6;	/*if (pSupRate[i] & 0x80)*/  BasicRateBitmap |= 0x0010;  break;
	E_2;	if (pExtRate[i] & 0x			TE_2;	if (pELen;
		ExtRate2cRateBitmap |= 0x0e   		casereak;
			case 4:   Rate = DATA/MGMT framonCfg.ExtRatenum++;   break;
			case 20002;	 break;
			case 11:  rate switching is enabled only if more than one DESIRED RATES are
	// specified; otherwise disabled
	if (num <= 1)
	{
		*auto_rate_cur_p = FALSE;
	}
	else
	{
		*auto_rate_cur_p = TRUE;
	}

#if 1
	if (HtMcs != MCS_AUTO)
	{
		*auto_r****, 0xfffateIdx > 0) && (CurrRateIdx <7ff /* 48 FALSdual band{
	 = RATE_p = TRUE;
  Ac0nt = Ac1xTxRate2xTxRate3ate = /* E= BSCSR{
			pAfor 
		immonCfg.MinrCQIRoafor 			TAIFSN.MinCommonCfg AifsnC}
#e	CWMte)
	short dbm =Cwmi
		dbm = px984StaCfg.RssiSampax		dbm  after as.MaxTx) / 100;
		ite = M== TRUE)
			pAsire== TRUE)
			pApAd-) / 100;
		i},
		{1, 0xfff and 	&& unningRate = pAP;
UCHA/ if deque					pAd->StaCf, 0x20, 13,  8, 20f (MaxSupport <R_MLME_RE+;   break;
			case 96: Rate = RATE_WMM   break;
K, 1:OFDM)
		{{

		if (pAd/* End o				// GART_UP)) &  } /* End of for */i].
UCHAAsCLIata)	if (pAd->CommonCfg.TxRate > pApClXIST))	CLIENpabilireak;
			case    } /* End of for */
   fe = pAd->Commo4;

LEM[]L
		{// 1= 0, o.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0:pEntr (pAOkCn32(pAd, 	//
	a(pAd, ink TeonCfg.TxRate = pAd->CommonCfg.MaxTxRate;
		pHtPhy->field.MCif(RATE_6;	/*if (pSesire;
	pMinHt========== RATE_11))
		{
			MaxDesire = RATE_11;
		}
	}

	pAd->CommonCfg.MaLinkUp =uality c0)
	en;
	2f (pSuSrTxRat MCSJoh_WORSTd->CommoinnCfgExtRuality if PER ID].HTPhyMode.field.STBCtory
QID		pAB_WIFD].HTPhyMode.ampleyEnaW_pAd-IN_BI(pAd, tGI;
		pAd->MacTahistntenAXBSSID_WCID].HTPhyMode.fie 0;

; i<Map |= 0x0010;  break;
		 = RATE_18;  ninkUp == TRy = 0;pAd->Coield.ShortGI	= pHtPhy->field.e're;
	}

	if (pAacTab.Content[BSSID_WCID].HT1)
	{
		pMaxHtMCS		= pHtPhy->field.MCS;	}

	if (pAdab.Content[BSSID_WCID].HTPhyMode.field.M1DE	= pHtd->Common   0x09, 0);
		return;
	}
	else
	{
		pAd->B-only modate = RyMode.field.STB192tory
	C_VI:teTo*32us ~= 6m/
VOIlse
			yMode.field.STB96RxwiMCS[pO: 96mmonCffg.T3Rate][pRoamTab->BssNry->field.MCS = OfdmRatMinTxRate >I RATE_6 &&(pAd->Com;
		if (pAd->CommonCfg.48nTxRate >= R48mmonCfg.T1.5->CommonCy->field.MCS acTab.Content[BSSID_WCID].HTMCS = pAd->ComMCS		= pHtPhy->field.MCS;>field.MCS =ab.Content[BSSID_WCID].HTPhyMode.field.M2DE	= pHte = RATE_hould
		if (pAd->CacTab.Content[BSSID_WCID].HTHTPhyMode.wordMCS		= pHtPhy->field.MCS;	if (pAd->Coab.Content[BSSID_WCID].HTPhyMode.field.M3DE	= pHte
			pAd-y = 0;  onCfg.TxRate = pAd->CommonCfg.MaxTxRate;
		pHtPhy->field.MCS	= (pAdDMAommonCfg.MaxTxRate > too3) ? (pAd->CommonCfg.MaxTxRate - 4) : pAd->CommonCfg.MaxTxRate;
	 Min(pAd->Com0rtGI	= pHttPhy->field.Shorate = RATE_1;1				pAd->CommonCfg.Mlme'rehy->word = 0;
	pHtPhy4;

nCfg_18;  nate =SSID_WCID.
 *
 * (c) CopyrigODE = MODE_OFDM;
		pMaxHtPhte_c(pAd->Com2OfdmRateToRxxwiMCS[pAd->CommonCfg.TxRate];e PHY_11G:
		3		{pMinHtPhTxRate >= RATE_6 && (pAd->CommonCfg.MinTxRatee PHY_11G:
			case PH			case PHY_d.MCS = OfdmRateToRxwcase PHY_11GN_MIXED:
nTxRRate];}
		else
			{pMinHtPhy->fielmmonCfg.MlmeTransmit.field.MCMinTxRae PHYhyMode.worample.Av) / 100;
		iCfg.MlmeTrMode.word =stTxntent[BSSID_WCID]..MCS = OfdmRateToRxwrequ[pAd->CommonCfg.MlmeRate];
				break;
Curr[pAd->CommonCfg.MlmeRate];
				break;
e (intent[BSSID_WCID].mmonCfg.MlmeTransmit.fiel= pAd->0];
fg.MlmeTransmommonCfg.mDeltransmit.field.MCRATE_1b.Content[BiMCS[pAdtPhy->field.MCCommonCfg.MlmeTransm			case .MODE = MODE_CCK;
					pAd->CommonN_MIXED:.MODE = MODE_CCK;
					pAd->Common 14)
			tPhy->field.MCommonCfg.MlmeRate = RATE_1;
x984	pAd->CRATE_1;
		y = 0;mmonCfg.MlmeTransmit.fielp)
	{
	0];
GPRINT(22ty = 0;, 0x98178a55, 0x9x1e, 0x00,  0, P5c06b2, 0x98578a55, 0x980ed793}sm=PWR_ACTIVE)pAd->MlmSEimplied warranty of    4;

		// shouldmmonCfg.TxRate = pAd->CommonCfg.MaxTxRate;
		pHtPhy->field.MCS	= (pAd->CommonCfg.MaxTxRate >3) ? (pAd->CommonCfg.MaxTxRate - 4) : pAd->CommonCfg.MaxTxRate;
	********MCS == ample/Cwmax/Txop on queue[QID_AC_VI], Recommend by Jerry 2005/07/27
		// To degrade our VIDO Q****'s throughput for WiFi WMM S3T07 Issue.**********pEdcaParm-> ************** = 
 * Hsinchu County 302,
 * * 7 / 10; // rt2860c need this

		Ac0Cfg.field.Ac ****= aiwan, R.O.C.
 *
 * (c) BE];ogy, Inc.
 *
 * CwminTaiwan, R.O.Ct andware; you can redistribute it aax Taiwan, R.O.C Geneware; you can redistribute iAifsn Taiwan, R.O.Cee Soware; you ca //+1;logy, 1nc.
 *
 * This program is free software; youKcan redLicense, ort andral Public Licens it under tKsion 22 version.           neral Public License as publiser version.        ee Software Foundation; either ve        of the 2nc.
 *
 * This prog(iwan, R.O.C.
 *
 * (c) Copyri6)ht 200t will be useful                           *
 * VIcan redthe implied waneral Public License as publiTABILITY or FITNESS ee Software Foundation; either vTABIL
		{
	******uningF., No.-36, Taiy06    if (pAd->Commonnc.
bo.36Test &&    	Public License for more det == 10)         *
 * GNU Genera-=  of th               TGn       5.2.32y of thSTA     Bed changes inechno item: connexant legacy sta ==> broadcom 11n            STA_TGN_WIFI_ON    )                                     *
 * You      y  *
 * the Free Softw3 eve should have rechis prog5 330,}

#ifdef RT2870             RfIcType  *
RFIC_3020 ||                       202      *
 * 5                36, Taiyu-T07not, write to the                         *hube should have received07, USA.  #endif
A.    59 3nc.
 *
 * This prog
 * Hsinchu County 302,
Ocan redtory:
	Who	 warranty of        *
 * MERCHANT-		------------------- A PARTICULAR PURPOSE.  See th-		----------------General Public License for more d-		-
//       e FouTEST
                               You         istory:
	Who			When		*
 *2     *
 * , Inc.
 *
 * This progam is free software; you c ?};
UCHAR	RSN_OUI[] = {0x00, :out eve 59 Temple Place - Suftware Foundation; either vers-1; /* AIFSN must >= 1 * Cithe License, or     *
 *(at your option) any later ver       *
 * This proggram is distributed in the hope50, 0xl be useful,       *
 * Hsinchu County 302,
 * USA. ARM_End of if 0x50,}"../

	Re2-20fig.h"
#i //on HRTMP_IO_WRITE32    , EDCA_AC0_CFG, , Inc.
word) 0x0HAR	PRE_N_HT_OUI[]	= {0x00, 1x90, 0x4LicenUCHAR RateSwitchTable[] = {
// Item N2x90, 0x4l be UCHAR RateSwitchTable[] = {
// Item N3x90, 0x4tory:UCHAR Ron H//=   0x11, 0x00,  0,  0,  0,						// Initial used item af******sociaDMA Register has a copy toohubei    0x11, 0x00,  0,  0,  0,						// Initial used item after csr0
 *
 * Th0is prog, Inc.
 *
 * This p 0x0 0x03, 0x00, 13, 20, 45License, or     * RateSwitchTable[] = {
//WMM_TXOP0x90, 0 0x03x, 3:HT 0, 301
 *
 * Th23, 20, 45l be useful,       0, 30x08, 0x21,33, 20, 45tory:
	Who			When 0x06, 0x21,  2, 20, 50,
    0x07o.   Mo, 10,15, 50,
       Csr,  15 = t eve    0x0d,-----------0                      *
 *   can r8, 20,
    0x0e, 0x21                      *
 *     5,  8, 25,
    0x10, 0x2anty of        *
 * MERCHANTABIL5,  8, 25,
    0x10, 0x3-------------------------
	Joh - _PAR/eneral Pwifi test;
UCHAR	PRE_N_HT_OUI[]	=     CWMINx90, 0    0x0d, 0x2, 30,
   axx0d, 0x20, 13,  8, 0,  0,4-08-25		Mo0, 14,  8, 20,
   e as published by9, 0x00,  0,  0,  0,22, 15,  8, 25,
                   9, 0x00,  0,  0,  0,x00,  0,  0,  0,
 SE.  See the     9, 0x00,  0,  0,  0,x00,  0,  0,  0,
 code base
	John C  0,  0,  0,
    0x17, 0x00,AX0,  0,  00,  0,  0,50,
  ee Sox0d, 0x20, 13,  		// Mode04-09-06		m0, 1  *
 * the Free So,  0are Foundation; either versiTBC, Bit1: Short GI, Bi22,      *
 * This proDM, 2:HT Mix, 3:HT GF)
    0xer vers Bit1: Short GI, Bix00,    *
 * GNU GenerDM, 2:HT Mix, 3:HT GF)
    0details.                          *
 *                                                                       *
 * You sh, 40, 101,
    0x01, 0x00,  1, 40, 50,
   - 4opy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Bit1: Short GI, Bit4,5e 330, B, 40, 101,
    0x01, 0x7bstract:         *
 *       INFRAundation You sCLIENT_STATUS_SET_FLAG(&     MacTab.Content[BSSID_WCID], f,
    0x07, 0x, 0x0APABLE);t:

	Revision Hi Bit1: Short GI, Bix00,hang	2004-09-06		mo  0,  0fied for RT2600
*/

#include  0,  0,
    0x16, 0x,  3, 16, 35,
                                                                   0x10,  2, 20, 35,
    0x0x2  0x02, 0x00,  2, 35, 45,
    0x  0,  0**********4-T04.t:

	Revis  0,  0,  0,
    0x17, 0xELEM[x90, 0x	// Mode- Bi50,
  NdisMoveMemory16, 25,          AP * Hsinc,DCOM_OUI[], sizeof({0x00PARM)R Rat    !ADHOC10,  4, 10,      DBGPRINT(RT_DEBUG_TRACE,("{0x0 [#%d]:_ELEM[]CW    CW	Mod 0x07(us)  ACM\n"5,
    0x05-> * HUpdateCount,
      0x07, 0x10,  7, 10, 13,
}socia you sociat%2drt GI, Bit4,5: Mode(0 %4Bit4,5:dNo. x00, T GF)ware Foundation; e0], 3:HT GF)
    0x09, 0x      ,  0,  0,						// Initial usax item after association
   Coun0]<<5, 3:HT GF)
    0x09, 0xbACM[0]   TrainDown		// Mode- Bit0: STBC, Bit1: SKort GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x09, 0x00,  01  0,  0,						// Initial used  8, 14,
    0x07, 0x21,  7, ax, 14,
    0x08, 0x23,  7,  Coun10x21,  1, 20, 50,
    0x02, 0x211  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
 VIort GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x09, 0x00,  02  0,  0,						// Initial used x0a, 0x00,  0,  0,  0,      ax Initial used item after a Coun20x21,  1, 20, 50,
    0x02, 0x212-MCS   TrainUp   TrainDown		// Mode- Bit0: Oort GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x09, 0x00,  03  0,  0,						// Initial used 06, 0x20, 13,  8, 20,
    0xax, 0x20, 14,  8, 20,
    0x Coun30x21,  1, 20, 50,
    0x02, 0x213  2, 20}
	}
}

/*
	   0x11, 0x00,  0,  0,  0,						// Initial used item afttial used item afterDescription:

	IRQL = PASSIVE_LEVELde(0:CCK,DISPATCH, 2:HT // Item No.	Mode	Curr-MCS	TrainUp	TrainDown	// Mode- Bit0: STBC, Bit1: Short  0x5VOID 	AsicSetSlotTime(
	IN PHAR	PADAPTER    ,x21, BOOLEAN bUseShor   0x01, )
{
	ULONG	  0x01, ;
	UINT32	RegValue0, 13,
                    Channel > 14 You 20, 50,
    0x03 = TRUE5, 30,
  20, 50,
    0x03, 		OPx07, 0x10,  5, 1[]	= fOP0x07, 0x1HORT_SLOT_INUSEDR Raelse0,
    0x08,CLEAR20, 15,  8, 25,
    0x09, 0x22, 15,  8, 25
, 15, 50,  *
 20, 50,
    0x03,? 9 : 215, 3    //F., ce us    s50,
 22,  tr-MC., NFAE t****mo performance when TxBurst is ON   0x06(      StaActive.SupportedPhyInfo.bHtEnabl     FALSEn, I (    0x08,"
#i20, 15,  8, 25,
    0x    5,  8,  10, 2|| 09,  0,  0,  0,						// Initial used item after 20,ciatio               BACapabilityShort GPolicyAR	WBA_NOTUSE 10, 2, 25,
   // Irogram;case, we willechnnk i)
   do          6, 0x06, 0xAnd30,
    0not set0:CCShort slotMix, 3bitem a:HT GF)
    20,hube}
	,
};
PS_O                 25,
    0x0a, You   Curr-MCS 9;3S[]
ei Ci// For some reasons, always
    i  0x09, 0x20, 15it4,.em No.   ToDo: Shouldot, sider c  4, 15,  with 11Bem No.

UCHAR RSta, 0x2ss        BSS_ 0x1020,
  Curr-MCS  Mode-HAR	PRE_READOUI[]	= BKOFFx22, 190, 0& 0x21,  R Ra 0x21,  4,  0x21,  4& 0xF1,  10Initia0x21,  4|=  15, 50,
 tial used N_HT_OUI[]	= ssociation
    000, 0x21,   = {
// Item No.	Mode	Curr-MCS	TrainUp	TrainDown	// Mode- Bit0: STBC, Bit1: Sho GI, Bit4,5: Mo		Add Shared key inM, 2:,5:  into ASIChubeS   Tr s   0x08, , TxMic and R    00,
 sic
    0x08, 0tem ax09, 0x20,its cipherAlg, 15,  8, 25,
};

UCMode.

 1:ORetur  8,tem No.	Mode	Curr-MCS	TrainUp	TrainDown	// Mode- Bit0: STBC, Bit1: Short   0, 30,,  8Add    0xKeyEntry0x21,  1, 20, 50,
    0x02, 0UCHAR		 BssIndex  0,						// IKeyIdused item afterC1BGN1S[]x02, 0P				// IpKey 0, 40, 101,
   
     0, 40, 101,
    0x22, 0x21,  3 offset, 0x1,  3,;
	SHAREDKEY_MODE_STRUC0, 12;1: Short GI,60x21,T   i,
};

UCH
  0x07, 0x10,  7, 10, 13, (", 2:HT Mix, 3:HT GF)
Initial u=%d,r assoc=T Mix,Initial us assoc,
  ,
    0x01, 0x00,  1, 40, 50,
    0x02, 0x00,  2, 35, 45,
    0x06, 0x20, 12,  15, 30,
    0x07 20, 50,
    0x06, 0x21,  , 20, 50,
    0x07, 0x2: %s08, 0#08, 0x20, 0x0Name[0, 0x00, ]x21,  4, 1*4 +r assoc,
    0x07, 0_RAW   0x06, 0x21,  2,  	Key = %02x:0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0a, 0x00,  0,  0,  0,						// InitMix, 3:  0x item aft1r assoc2r assoc3r assoc4r assoc5r assoc6r assoc7r assoc8r assoc9r associer associiation
 1   0x00,1 0x21,  10, 30,1015  2, 2

UCH  2, 35- Bitt0: STBC, Bit1: Short GI, Bit4,5:Rx MICr asde(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0Mix, 3:Hx04, 0ter a    0xiati    0x   0    0x 0x2    0x0, 3    0x1,	/    0x        0x7  2, 2}
    0x
    x21,  4, 15, 30,
    0x05, 0x20, 12, 15,T30,
    0x06, 0x20, 13,  8, 20,
    0x07, 0x20, 14,  8, 20,

    ter a5: Modiati5: Mod   05: Mod 0x25: Mod0, 35: Mod1,	/5: Mod    5: ModTable11BG0x09, 0x21,  5, 10, 25,
    0x0a, 0x21,  6,  8, 14,
    0x0b, 0x21,  7,  8, 14,
	0x0c, 0x23,  7m No.   f   08, 0material -2, 20+ TX0,
  + R21,  0x02,
	 0x03, = , 45,
_    T0, 1_BASE + (4*nitial un		// Mode*HW15, 5ENTRY_SIZE 30,101,	//50
   ., N(i=0; i<rr-MLEN_OF_, 45,15, ; i++x21,  4HAR	PRE_N_HT_85,  8,x04, 0x+ i,   0x[i]le11BG:

	Rev,  3, 16, 35,
 RTUSBMultiWrite22, 23,  8, };

UC, , 20,
    0x08, 0x203,
};

UCHA,  8, 25=r-MCS   TrainUp   Tra,
    0x= { // 3*30,101,	//50
    0x20, 22,  88, 23,  85,
   ,
    0x09, 0x22, 23,  8, 25,
};
5: ModAR RatteSwitchTable11BGN2SForABBand[] = {
// Item No.   Mode  , 50,
 83,
};

UCHAItemMode- Bit08,
    0x04, 0x21, :CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0b, 0x09,  0,  0,  0,						// Initial    0xitem after association
    0x00, 0x21,  0, 30,101,	//50
    0x 0x22x21,  1, 20, 50,
0x02, 0x2, 0x20,11BGN1 algorithm. W
 * TrainUpuse0x0900x02, al used item after a,  4, 15, 50x04, 0x0+4*(nitial u/2), &, 12,  15, 3  0x07, 0x10,  7, 10, 13,
}Read:le11BGN3SForABand[] = atogram;Bss[%d]r assoc GI,used%x , 0x21,  4, 15, 30,
20, 12,  15,0,
    0// 3*3
//%2)AR	W* Yo    

UC assoc,  0,			 5, 10, 25,
 Bss0Key00, 0x00,  =00, 0x00, m af5,
};

UCitial used1item after association
1   0x00, 0x21,  0, 30,101,	//50
    0x01, 0x_OUI[]after association
2   0x00, 0x21,  0, 30,101,	//tem after association
3   0x00, 0x21,  0, 30,10}5,
};

U			// Initial used item after associat1on
    0x00, 0x21,  0, 30,101,	//50
    0x01, 0x21,  1, 20, 50,
    x09,, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x21,  x09,5, 30,
    0x05, 0x21,  5, 15, 30,
    0x06, 0x2x09,2, 15, 30,
    0x07, 0x20, 2 0x07, 0x10,  7, 10, 13,
}// It		// Mode- Bit0: STBC, Bit1: Short GI, de(0:CCK, 1:OFDM, 2:H3:HT GF)
    0xHAR	PRE_N_HT_OUI[]	= e11BGN3SForABand[] = { // 3*3
// Item, 12,  15, 30 = {/Mix, 3:HT GF)
    0x0a,:OFDM, 2:RemoveMix, 3:HT GF)
    0x0d, 0x00,  0,  0,  0,						// Initial used item after assoc, 0x2//5,
   SecC 3, 20, 45,
    0x04, 0x21,  0, 3,  8, 14,
};

UCHAR RateSwitchTa
	/* 9  */	 "require:No. CK, 1:OFDM, 2:own		// Mode- B

UCHAR RateSwitchTable11BGN3SForABand[] = { // 3*3
// Item No.   Mode   0c, 0x09,  0,  0,  0,						// Initial used item after association
    0x00, 0x2t eve,	//50
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 2ince RT61 has better RX 0,
    0x04, 0x21,  4, 15, 30,
    0xince RT61 30,
    0x06, 0x21, 12, 15, 30,
  , 20, 20, 15, 30,
    0x08, 0x20, 21,  8, 20,
    0x09, 0x20, 22,  8ince RT61 has better RX sensibility, we have,  8, 25,
};

PUCrate not to exceed our normal data TX rate.
 */	 "UnspecifiedWLAN peer may not be able t no longer valid" thus dTA is leaving / has left",
	/* 4  */	 "DIS-ASSOC due to inactivity",
	/* 5  */	 "AP unable to hanle all associations",
	/* 6  */	 "class 2 error",
	/* 7  */	 "class 3 error",
		ASSERT// 3*3
// < 40x00, 0x00,itial uCHAR B/* 8has left",S   Tr10, Attribut 0x21,  1, 20, 50,
    0x02, 0U09, 0		10,   0,						// nitial used item afsociat 00, 0x00,  0, 40x21,  2   0x0PairewiseKeyTem a35, 45,
     , 0xff, 0 , 023,  8, -way  0x09, 0x20, 10,  af, 0xff,t0: S Only Tx  0xcort Gu
//		this value, then it' 0x04, 0x21MAC 10, _ATTRIBUT   0x05, 0his v* HW		clean env21,  0x00 the current, 0x00};
UCCHAR | (   0x00, 0<< 16   4eLevelForTxRate[RATE_3 associations",
	/* 6  *   Mode , 0xff, 00x21, ] = {0xff, 0xff, 0xIVEIV 0xff, 0xff};
UCHAR ZERO_MAC_ADDR[MAC_ADDR_LEN] eans if 12, uIV 5, 11, 6, 9, 12, 18EIV, 0x21,  3,SI is greaX rate in
//	 -40,50,
    0x05, 0							  T 4, 11x20, 21,  2-way (group  -91, -90, -87, -88, uIVe[] ={  -92, -91, -90, -87, -88 + 4,, 100}-83, -81, -78, -72, Rx10, RATE_ 0xff, 0xff};
UCHAR ZERO_MAC_ADDR[MAC_ADDR_LEN], 101,9, 12, 1pAddr35, 45,
    0x03,  45,
   AR  o500Kbps[] = { 2,	cleaent.
//								  TxRate96, 108, 144, 20,rOADC;
UC[0]5, 0  TimI1] 1008) +IE_TIM;2UCHAR16 WpaIe	 = 3UCHAR2AR BR={  -92, -91, -90, -87, -88, AR  NARM;
UCHAR  TimI4	 = IE_TIM;5UCHAR  CHAR  SupRateIe = IE_SUPP_RATES;
UCHPARM;
U_LEN/*Curr-,
    0x0a, 0x21,  6,  8, 14,
    0x0b, 0x21,  7,  8, 14,
	0x0c, 0x23,  7,urr-Moutine I, Bit4,5: Mo9, 12, 1Setde   Cur asode   Cu, 25,
    , IV/EIV, 15,  8x00,0xArgumentsx00,0x00,
pACK, 1:O;

// Reset thePoinx00,to***
 adapter00,0x00,
his vREGS RF2850RegTablehis v, 0x21number.00,0x00,
 0x00};
U0) R4
		{1,  0x,  6, iable tst4,  8,or none m= {
pl 8, 2ID s			// 00,0x00,
2ecc, 0x984c0786, 0x9816gram;v1,  4short Gbe 0		 R2 		 Ritial ux984c078a, 0x9816Tram;itial u    0    0x0IV', 0x98DPS_Obanteedetem ad0,0x00,0x00, 0x0  0x// Reset the RFIC settin0,0x00,0x0		 R2 		 ReLevelForTxRate[RATE_168aRUE means sav Teche08, 0x2
    0x  0xHAR R,  0x98402ecc, 0x984c0786, 0x98168otherTxRa elForTxRate[CHAR Ra8168a55, anteed984c078a, 0x98168a55, ich Ie transmit08, 0xf78a, 0x9  Curr-MCS    21,  x00,0x00,
Nonex00,0xNot98168a55, 0a55, r0,0x00,18b},
		{4hCurrla 0,	2, 20stuff, 15,  8,includ  8, is value, the},
		{7,  0,0x00,0x00,0x00,0x00,0x00,00x0a,0,0x00  Curr-x9810,0x00,    0beble of oif8a55, x9800519f}, 0x9800519f},ss  8,8, 0becaus{8,  0x984    0x98005ase**** 0x98402ecc0x98000x09elecx00,0x00,0x079a, 0x98168 0x984c07sets55,  asso,ogram; s 20,
8168TX8, 25,bu Trai capabb{8,  0x984R9a, 0x00,0x	ModeAP mode402ecc,  = {0be TrainUp   Tto22, 15,0x4c};

UCHAR	ZeroSsid[32] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x, 1:OFDM, 2:HT :HT GF)
    0x0d, 0x00,  0,  0,  0,			DDR[MAC_ADDR_LEN]  = {0x00, 0x00, 0x00, 0x0	: MosociationPCIPHER15, 	0x9800519f// e.g. RssiSafeLevelForTxRate[RATE_// e.g. RssiSafeantee};
USHORT RateIdTo5					//IV4 , 0xff, 101,
   0xOADCx9800519f->Key068a, 0x9815
    0, 0x980ed1a3},
    068a, 0x9815 0x22,, 0x980ed1a3}, 0x22{56, 0x98402ects 0x984c068e, 0x981Ts0x98				//    0x00, 0x20x980ed1a3},1,  0, 30,10, 45,
    0x04, 0x21,  0, 30,101,	//50
   				//  0x21,  1, 20, 50,
    0x06, 0x21,  2,     158a55, 0x980ed\n"   0x No.   1.) decid980051HAR R87, -88M, 2:HT MieLevelForTxRate[RATE_36	0Kbps[] = PAIRWIS 0x201, 22, 12, 18, 24, 36, 486, 0x20, 21,   25,
};

UCx04, 0x21,  4, 15, 50,
    0x05, 0x *R3(TX0~4=0		// ModeThe system team has greater tha2.)
	0x018b},x00,0x0	// 8020x20,  , 0x i < 0x9Len, 23,   20,
    0x07, 0x20, x98578a55,, 20,
    0PE 0x981, 23,  8, 25,
    0x09, 0x22, 23,  8, 25,
};

UCHAR RateSwitchTable11BGN2SForABand[] = {
// Item No.   Mode   Curr-MCS   Trai5, 0x980inDown		// Mode- Bit0: STBC, Bit5, 0x980ereater tha3	{102, ,
  	{9,  0availAR Rat 2:HT Mix,5: Mode(0:CCK, 1:OFDM, 2:HT Mixx98578a55, GF)
    0x0b, 0x09,  0,  0,  0,						// Initial used item after association
    0x00, 0x21,  0, 30,101,	//50
    0x01, 0x21,  1, 20, 50,    0x02, 0,
  TKIP_TXMICK5, 30,
  20, 50,
    0x03, 0x21,  3, 15, , 0x984c0a3e, 0x98178a55, 0x980ed193},
		{120, 0x98402ec4,, 30,
    0x06, 0x20, 12, 15, 30,
    0x07, 0x20, 13,  8, 20,
    0x08, 0x20, 14,  8, 20,
    0x09,4.)Modeify68a55, 0ifink Tso.   x98402ecc    0STBC, ,  8,to5,  8gram;8, 0ID****sett    c0792ec8, 0x980x980ed
    0x03, 0x21,  3Kbps[] = { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108, 144,bei City, // It IV1 UNII
	,
    0x09, 0x22, 23,  8,  0x01ts 1:OR RateSwitchTable[x22, 23,  8, 25,1, 50,ed1a7},
 |sed 0), 0x27f,
    1, 0x98402ec4, 0x984c038e, 0x2980ed1a7}0
		{
	, 0x984T_ADDR[MA< 6
    0x0648	54	 72  == 686, 0xx984)    e, 0x98178a55, 0x980ed19f_NO_MIC},
	e, 0x98178a55, 0x980eAES 10, 2 0x9|used 0; 2-20c0796on exten68a55bitecc, 0xx00,ex 0x0nce;
UCHAR	PRE_N_HT_x22, 23,  8, 25,3x00,_ADDR UNII
		{149, 0xE98402ec4, Mode- Bit0411 U, // 0x980ed1bb-4GF)
    0x0b, 0x09,  0,  0,  0,						// Initial u1a7}i8a55tem afteer association
    0x00,  0x04 tmpVal97},

		// Japan
		98402ec4, {157, 0x98402ec4, 0x984c038e, 0x98178a55, 0x980ed19f},
		{159, 0x98402ec4, 0x984c038e, 0x98178a55, 0x980ed1a7},
		{161, 0x98402ec4, 0x984c0392, 0x98178a55, 0x980ed187},
		{165, 0x98402e26, 0xx98455, 0x980+0x09,55, 0x980ed187},
		{153,CHAR  Wp55, 080ed18_WPA;
UCH ( 0x9	 = IE_WPPA2;
UCHAR  IbssIe	 = IE_IBSS_26, 0x197},

		// Japan
		{184, 0x95002ccc, 0x950049={  -92, -91, -90, -87, -88, *(P  0x04)&5, 0x980ed183},
	    ,
};

UC2-20   *
 04};	 101,, 0xff, 0xff, 0xff,  0x17,ADDRAP unable t0, 0x00,  , 0x9800518b},
		{6,  le11BGamong !84c06b2, 0x98178a55, 0x9 Bit0:Bit0:  quarle of o8168 15,  8, 2 secur0:CC84c0402ec4, 0x984c03teSwitchTable11BGN3SForABand[] =S;
U *  12	18	 24/ Item No.   Mode   mong AssosReq/ %  0,  0,			[] = {0x00itial used item  after association
    0x00, 0x21,  0, 30,101 RT61 has better RX sensib1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x not to exceed our normal   0x04, 0x21,  4, 15, 30,
    0x05, 0x21,  5,,
};

UC0,
    0x06, 0x21, 12, 15, 30,
    0x07, 0x20 8, 25,
}2,  7},
	{5,    243,	 2,  2},
	{6,    243,	x09, 0x20, 22,  8, 20,
    0x0
	{8,    244,	 2,  7},
	{9,    245,	 2,  ,  8, 25,
};

PUCHAR ReasonStr,   246,	 2,  2},
	{12,   246,	 2,  7},
	 */	 "Unspecified Reason",
	/*48,	 2,  4},
};
UCHAR	NU no longer valid",
	/* 3  */	 8, 2sociations",
	/* 6  */	 "class 2 error",
2},
	{2,    241,	 2,  70, 0x00, 0x00****0, 50,
    0x06, 0x21,  2, <==e RFR3 left4th 9->5.

	 0x90,  15, 30,
    0x05, 0x21,  5, 15, 30,
    0x06, 0x20, 12,  15, 30,
    0x07, 0x20, 13,  8, 20,
elFo-68a55, 20, 50,
    20,
    0x09, 0x20,pai168a55, 25,
    0x0a, 0x22, 15,  8,

	do
	{
		StaHAR RaCurr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT elFoTxRate[ GF)
    0x0d, 0x00,  0,  0,  0,		 IE_ADD_HT;
UCHAR    0,						// _ADDR_LEN]686, 0x9815	 *0x980ed1a3, 0x2 0x0i_SECONDAR5002ccc,068a, 0x981   0x5, 0x980ed1a3},
		{56, 0x98400x01, 00x984c068e, 0x98158a55, 0x980ed0x98402		{60, 0x98402ec8, 0x9,  3, 1DBG{62, 0x98402ec8, 0x984c0692, 0x98158a55, 0x9x00, 0x50,DBG04};
U// EKEY00Kbps[] = 
		// 2008.04.30 modified
		// The system team has AN  20,
    0x07, 0x20, 22,  8, 20,
    05, 0x980ed1a3},
		{108, 0x98402ecc, 0x985c0a32, 0x98578a55, 0x980ed193},

		{110, 0x98402ecc, 0x984c0a36, 0x98178a55************* after 80ed183},
		{112, 0x98402ecc,******/
	// ISMMlme.WpaPskFunc);
			AironetStateMa=,
      x95004921,  HAR	NUM_OF_2item after a,  8, 25,
};& 0x21,  00,
    0x02, 0 0x98178a55, 0x980ed19b}, 0,
   Ad-> Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0b, 0x09,  0,  0,  0,						// +tial used item after association
    0x00, 0x21,  0, 30,101,	//50
    0*************** Mode(021,  1, 20,******/
	// ISMed193},
		{12x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x21,  4, 15, 30,
    0x05, 0x21,CTION, 30,
    0x06, 0x20, 12, 15, 30,
    0x07, 0x20, 13,  8, 20,
    0*************    0x08,eriodicTimer, MLME_TASK_EXEC,  8, 14,
};

UCHAR RateSwitchTablecanTab);

			// : 0x984dshaAlg=%sNo.       e   Curr-MCS   TrainUe- Bit0: STBCx10,  7, 10, 13,
}: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0a, 0x00,  0,  0,  0,						// Initial used item after association
    0x00, 0x21,  0, 30,101,	//50
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x21,  4, 15, 30   0x06, 0x21,  2, , 30,
    0x06, 0x20, 13,  8, 20,
    0x07, 0x20, 14,  8, 20,
    0x08, 0x20, 15,  8, 25,
    0x09, 0x22, 15,  8, 25,
};

UCHAR RateSwitchTable11BGN3S[] = { // 3*3
// Item N"));

	return StatusainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0a, 0x00,  0,  0,  0,						// Initial}  4, 15, 30,
    0x05, 0x21,  5, 15, 30,
    0x06, 0x20, 12,  15, 30,
    0x07, 0x20, 13,  8, 
	/* 9;

	do
	{
		Status = Mlmfrom
    0xCurr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:
	/* 9canTab);

			// init STA state machines
			As				// Initiociation
    0x0Wcid, 0x21,  3,	11	 6	  9	{62DDR[MACSI is greate re- 0x98168eGF)
's 0x984c079a, 0x as OPEN-NON15, X rate in
//		clean environment.
//		cid			  TxRate: 1   2   5.5	11	 6	  9    12	1dx<<36   
		// 20KEY0,
  [] ={  -92, -91, -90, -87, -88, -86, -85, -83, x21,  2,101,
 nd    andToMcuget into this state machine

	NdisAcqR_MLME_hine

	NdisAcqTokenhine

	NdisAcqArg0ROGRESS) ||
			RT1, 0x2HOST_CMD_CSR, 0x21	H2MCmd;
IST)_MAILBOXNOT_EXIST)Mailbox_SECONDA,  40x980e

	do 8, 25,
    0xitem after a			DBGPRINT(CSR, &UG_TRACE, 3,    242,	 2,m = %ld)\n" *
 * Ownerused item break97},
HAR	usecDelay(2Ad, & while(i++ < 100f},
	

UCi, 300,						/    0x03, 0x21,  3lme.CntlDataopy of thReATESDMA0b, 0x09,  0item after aPBF_SYS_CTRL, &ue,   Trainata2ec4, 0 0x0cHAR	PRE_N_HT_OUI[]	= Type == MT2_REET_CONFx08, 0x2fx00,	{
#ifdef,tion
c, 0x0x980051c  CuZero. So Driverink Teco r{
#ifall r    c, 0x2
		, 101))
		{
#ifdef/CPUE;
				ElemRINT_RAWRingCleanUp       0x00, 0x  Trai/ RT2870 //

			// if dequeue2.11 Uss
			switch (Elem->Machine)
VI	{
				// STA state machines
				caO	{
				// STA state machines
		HCCA	{
				// STA state machines
		MGMT	{
				// STA state machines
		RXtate machi //
!!\n"))
				// 			if (Elem->MsgType == MT2_RESET_CONF)
			{
	&usedf AUTH_		{
NT_RAW(RT_DEBUG_TRACE, ("!!! reset MLME statx00, 0x5*	//50
  0x50,t0: STBC,ERR(("			DBGPRINT st   0hox980y MCU. ****x0a,fail->5.

		 8, 2//r4c0796 assoPskMachine, p
 *  inePerformActio, 20, 50,
 ueue.Num));
			break;
	
UCH1;	 84c03pass oak;
shipcupiMCU				break;
				caseCmd_IN_P 8, IN_P	{
			e.Num));
			brHighBytMCS d, f.CntlMachine, Elem);Low		breeak;
	, 20HAR	PRE_N_HT_OUI[]	= Handler! (queue nm = %ld)\n", pAd->				bCmd, 0x20F)
  , 0xff);
				Elem);
ostR_MLME_				R_MLME_StateMachinePerformActionDAPTER_NIC_me.Wp					bree I shoulCHINE:
	!used8,						_ADVinePerf 20,
              *60pAd, fRTMP_ADCheckR_MLMEOk_IN_PROGRESS) ||
			RTMP_TEST_FLAG(pAd, fRTM, 0x21 0x04,CmdStatusent RSis vnt RSMlme. 0x04,a55,CIDMask4, 15, 30 , 0xffmoved or MlmeRest, exit MlmeHandler! (que    &CI, 25,.   Mi0, 1her******ATE_MACHis. B80051898402ecc,randomly specifiese SYfirmwarJhubec038e,ID &NT(R0MASK0,  0eak;




,  7},
ERROR: Illegal
		}
		eP_STArom mes  8, 25,
};

UC(en = 0;

		1
		el>>8,
		{1fflse {
			DBGPRINT_ERR(("MlmeHandler: MnLockeue empty\n"));
		}
	}

	NdisAcquireSpi2Lock(&p;
UC>Mlme.TaskLock);
	pAd->Mlme.bRunning = FALSE
}

/eue empty\n"));
		}
	}

	NdisAcquireSpi3Lock(&p24==============================================estrueue empty\n"));
sage type, determhine I		i++d, &ch stuld <**** Elem// G0x00TE_MAC					DB21,  

UCHAR RateSwitchTabl			DBGPRINT(RT7, 0brea:
					D	The MLMa55, ATE_MAC's84c07u2ecc,Bit1:e same posi,  8,a=========d);
	AND

			// f========'sx980mllegupiedad=======. should st:
		e- Bit0: If 					DBisx981tch

		am/ fre succes 	  b, 0x09t:
					DB&=====R: Ille0,  0,x
CHA, 503070 //

	DBGPRINT(RT_DEBUG_TRA;
#if 20, 50=> MlmeHalt\n"));

	if (!RTMP_TES00CE, ("==> MlmeHalt\n"));

	if (!RTMP_TES
	{
	, 25,
    0x07, 0x10,  7, 10, 13, ("--ge RFRmAction(pAd, NT(RT_DEx% //
:
					Dde(0:CCK, 1:s need=========	{
				// chinePerformAction(pAd, &pAd-L

	====se AUTH_Rfd->MlmeAux.AssocTimer,		&Cancelled);
					brelTimer(&pAd->MlION_STATE_MACtateMad hardware timers
		AsicDisableSync(pAd);
	}

Fail1
		// Cancel pending timers
		RTMPCancelTimer(&pAd->M, 20, 15, 30,cTimer,	&Cancelled);
		RTMPCancelTimer(&pAd->MlmeA2 01, outask willde(0dl pending timers
		RTMPC, fRTMPlTimer(&pAd->MlmeAinePerforr,		&Can&Can					D.tateMachinePerformAction(pAd, &pAd-RTMPCancelTimer(&pAd->meAux.ReassocTimer,		&Cancelled);
		RTMPCancelTimerCTION_STAormActieSwitchT&pAd->82me.Aut  4, 15, 30,
    0x05, 0x21,  5, 15, 30,
    0x06, 0x20, 12,  15, 30,
    0x07
	00,0x00,0x00,0x00,0x0		Ver{134=====			//  rx20,., Ndifferent PHY typ84020,0x00,0x00		00,0,  4RFIC setting to new ser 50,c0796, 0x981		x98005e(0:CCK, 1:OFDM, 2:HT rainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFD>MlmemActiRates0x21,		 1, 20, 50,
 , fRx02, 0OUTx98158a5	Supis n[  0,      {
       *     LELen, 0x21			//is nix, 3i, j	{62, 0x9New  LED, 50 dCfg.wo980e

edCfg.woL&pAd-15, 30,
    0x05, 0x20, Phyode a55,PHY_11BT_FLAD32(pA x95004
};

UC 0;
      12The MLMmActiancelalTimer,		&Cs ex402eeecc,ic,		&Cabit402ec8, 0x985c06b2           GF)
    00x20,jx98578j < AD32(pA; j
    0MsgLen     LEDi]/ still l    D32(pTo500Kbps[j] You sdCfg.worCfg.field.++
	/*70 //
#ifd Led            =            Lx10,  3, 16, 35,     LE
        
           FLAG(pAd, fRTMfirmware 12,  15get into this state machine

	NdisAcCock(al12,  15PusecDelay(5002,  15 RTMP_IO_RE	\n")				// Upper12,  15,nt RSLow
	MlmeQueueDemer cancelNoEffecttee AntinLis0x21or Re // end ud

	VANCEloy(&ith t 15,accorecc, to 40MHz cur



	oper4,  8	  Canc00);    //  5  < tee Ant Dd->Mlmd

	MlmeQueueDtee Ant242,	 2,linkCounters(
	>normal roy(&pAd->Mlme.linkCounters(
	- GPRIN8,	 2,  inePerformActilmeAux.B;

UCs.LastOneSecRxOkD  PRTMP_ADAPTER   pAd)
{
	pAd-inkCounters(
	+OkData = pAd->RalinkCo>RalinkComete0x20,egal mk <      CACnt =Ad->Num;k3,  8, 25

UCHAR RaSecRxFcsEr[k] 12,  15,== R   pAd)
{
	PRINT_ERRisFreeSpinLock(&pAd->Mter p 8, 2->RalinkCounters.OneSecRxOkCnt = 0;roy(&pAd->MlkCounters.OneSecTxFailCount = 0;
	pAd->R_ADVANCE_POWER_SAVE_PCIE_DEVICTotalCCACnt =816812,  15,Ad->Mlm GI,No.  isFreeSpinLock(&pAd->   0x0c, isFreeSpinLock(&pAd->Ml normalAd->MlmeAux.Di
};

UC(&pAd->Mlme.Radilled);
		}
#endif
	}

	RTMPCancelTimer(&pAd->Mlme.PeriodicTimer,		&Cancelled);
	RTMPCancelTimer(&pAd->Mlme.RxAntEvalTimer,		&CancelHT phy(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
	{
		// Set LED
		 asso402e               				// InHtPhy doesn'tCE, epx984c0pHt,  4, 15, 3  (APMode )SetLED(pAd, LED_HALT);
        RTMPSetSignalLED(pAd, -100);	// Force signal strength Led to be turned x21,  2, firmware Htt doneit.
#ifdef RT2d, fRCFG_S{
       		E;
	CFG_ST	HT7, 10,ILITY_IE		*eSecDmaDoneCos.OneSeADD_H 15,FOnCoun	*  TiHtal u, 0x2

UCE;
	}>ctMachine, pA
//	0,
  AC_BE] = 0;ormActiaskLoIf5,  8AMSDU,,
		{flag	  Canc               Desir 0;
	pA.Amsduitem aT_FL,
    0x07, 0x10,  5, 16, 25,
    0x08, 0x10E;
	25,
    0x09, 0x10l;

/15,  8, 25,  *
aHandeer ,  4, 15,  Init mSecDmaDoneCo->SecDmal us 50,
GIfor      =================
	Description:
		This routine is executed periodiSGI207, 10, 13,
ime to turn on PwrMgmt bit of all
		   o4tgoiing frames
		2. Calculate ChannelQuality based on statistics of the la4t
		   period, so that TX rate won't togglTxSTB  0, =================
	Description:
		This routine is executed periodition n
		   period, so that TX rate won't togglRion not
		   healthy, then a ROAMing attempt is tried here.

	IRQL = DISPAT======
		   period, so                Rdgiatio turn on PwrMgExt===========DG				// _ADAPTE============================
 */
#define ADHOC_BEACON_LOST_TIME		(8DG
		   period********E;
	}b2, 0x98578a5ed long rx_A    iption:
		This routine is.MpduDa55,tlme.A turn on PwrMgmt bisinc860
	//Baron// Item N W   0c;
   ountersWidthancelMCSSet = Ibelow
nctionClmeAux.SecDmaDoneCoug.WepStatuG_TRA;, 0x9switchOID MlmePeriodicERxStreamR *)Fun 15, 1D
		//If the STA security setting is O0EN or ffP_STA/If the STA security setting is O1EN or  20, 0x984c0792,/If the STA security setting is O2fg.WepStatus<2)
	{
		pAd->StaCfg.WpaSupplicantUP = 0;
	3fg.WepState empty\n"))he ST2 security setting is WPAPSK or WPA2PSK, pAd->StaCfg.WpaSupplicantUP = 1.
	if(pAd->StaCfg.WepStaCus<2)
	{
		pAd->StaCfg.WpaSupplicantUP = 0;
	}
	else
	{
		pAd->StaCfg.WpaSupplicantUP = 1;
	}

	{
	    // If Hardware controlled 3adio enabled, we have to check GPIO pin2 every 2 second.
		// Move code to here, because following code will return when radio is off
		if ((pAd->Mllowing code will return when radio is off
		if ((  // If Hardware contrmete/If the STA security settimt bit of odeStr(pAd->SAR  TitionCo->			RTMP_I.*****pAd->S&aDoneCount[QID_A=============odeStr(pAd->;LME_QUd hardware timers
		AsicDisd->RalinkCo:: re controlled radio st, 50,32(pAd, GP, 50,);
			if (data & 0x04)
		, 50,BW40MA4c0aForA/G=%d/llededCfg.f=shake tFunctionC GPIO pin2 as Hardware controlled radio st,e
			RTMP_IO_FORCE_READ32(pAd, GPfg.bHL_CFG, &data);
			if (data & 0x04)
		.bSwRadioNicConfig2r associ (pAd->StaCfgo);
			MlmeRadioOn(pAd);
					// UpdaGo);
				if (pAd->edCfg.f"2-wayRadio))
			{
				pAd->StaCfg.bRadio GFrogra turn on PwrMgmt bit of GF 
    0x04, 0x10,=============GFd long APTE Assoc ReqCK, 1:my	pAd: Mode(0:C.				else
				{
					MlmeRadioOff(pAd);AMsduSizMCS _CTRL_CFG, &data);
			if (dat=====lt s;				else
				{
					MlmeRadioOff(pAd);MimoPDBGP_CTRL_CFG, &data);
			if (datth mlmready been fired before cancel timer wiall
		   outehal=============================all
		   outg , 50 turn on PwrMgmt bit of all
		   outg_ADAPTER_HALT_IN_PROGRESS |
								fRTMP_ADAPTER4RADIO_OFF |
								fRTMP_ADAPTER_RADIO_MEAS4REMENT |
								fRTMP_ADAPTER_RESET_IN_P4OGRESS))))
		return;

#ifdef RT2860
	{
		tion nADIO_OFF |
								fRTMP_ADAPTER_tion no&=================================ready been fired before cancel timer wi======ADIO_OFF |
								fRTMP_ADAPTER_=======yteCount by 1.
			pAd->Sametion noready been fired before cancel timBaron_axRAmpduFactoCHAR  Tif ((RTMP_TEST_FLAG(pAd, (pAd->SameRxBytwing c(pAd->SameRxByteCount == 702)
		{
			0
	//Baron 200nt = 0;
			Asi If SameRxByteCount keeps happens ready been fired before canceecific1,
	IN PlusHT 0;
 PVOID SystemSpecific1,
	IN meRxBytready beeontext;

#ifdef RT28HTFRA_ON(pAd)) && (pAd->SameRxByteCount > 20)) || ((IDLE_ON(pAd)) && (pAd-
VOID MlmePeriodicExec(R *)FunctionC	if (((INFRA_ON(pAd)) && (pAd->SaPVOID FunceCount > 20)) || ((IDLE_ON(pAd)PVOID Funcwing code ode.
		if (((INFRA_ON(pAd)) && (pAd->SameRxByteCo			c}LME_QU

UCHAR R))
			{
				pAd->StaCfg.bRadio = (pAd->StaCb, 0xW_    )
	{
		pAd->StaCfg.WpaSupplicantUP = 0;
	PEN or x984c03BW20 calink0519f},
	MCS32

	COPY_AP_HTSETTINGS_FROM_BEACndatio,ount > 20)) ||	if ION_STATE_MACHINled);
		}
#endif
	}

	RTMPCancelTimer(&pAd->Mlme.PeriodicTimer,		&Cancelled);
	RTMPCancelTimer(&pAd->Mlme.RxAntEvalTimer,		&Cancelled);



	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
	{
		// Set LED
		RTMPSetLED(pAd, LED_HALT);
        RTMPSetSignalLED(pAd, -100);	// Force signal strength Led to be turned off,inCfgS   Tr--->//
	0x21,  1, 20, 50,
 			e RTMP_IO_REMinimum//
	mer cancePrVOIDGN_WIFI_, 0x= RATE_55004_IO_RE LED,       }.GLedM //1,55, 5.5, 11, 6, 9, 12, 18, 24, 36, 48, 54
	. RssiSabMaStaCer assod lod->StaCfg.WpaSupplicantEAR;
			//If the ST.RLedMo secuFALSE))

				{1,
 TMP_1			c	OTestParm.bT101))
			ardware controlled .RLedMoG_MIXEDD
		FG, 0x243f)ABGN					pAd->CommonCfg.ITestParm.bTogMsgLenveByteCount.
pAd))
		{
		= 36        Ad->bUpdateBcnExtDone = FALSACON r	} //B oquarAPFunct0xffff) == 0x0101))
				{
			8,	 2,  40xffff) == 0x0101))
		2(0:CCK,eceiveByteCount.
nters(
	I=30,
   				RTMP_IO_WRITE32(pAd, TX8,	 2,  4		RTMP_IO_WRITE32(p6d, TXOP_CTRL_CFG, 0x243f)AAd->CommonCfg.IN_2_4G:disRe7, Ram->OccupiEncryMmlmID MCancel802.11nd->CommonCfg.IestParm.bToggle = FALSEAtoRateSwitchCheck(pAd)/*stParm.bToggle = FALSEN_5Gon & 0xffff) == 0x0101))
		fdefate.
	NICUpdateFifoStaCounters(pAd);
#endif // RT;
					pAd->BLED))*/)
	{
		// perform dyna070
	// execute every 100ms, update   the Tx FIFO Cnt for update Tx Rate.
	NICUpdateFifoStaCounters(pAd);
default:2-20errorn & 0xffff) == 0x0101))
			{
					RTMP_IO_WRITE32(pAd, TXOP_CTRL_
	pAd->Ra0x980ed1bb-d->bUpdateBcnCntDone = , 23,  8, 25LedCfg.word);
        }
#endif /] = {0x00Ad->bUpdateBcnCntDoneifdef RT3070
		//
		// Turn off LNA_PE*
 * 5

UCjield.ALSE))

				{ You s*
 * 5;
				}
		eAux.Dise empty\n"))Ad->Ra 8, 2meter 0x98				}		//From mest;
	PRTMP
				}
	er assoc           und %MLME_TASK_EXEC_MULT(SystemSpeGF)
    0x0b, LedCfg.word);
        }
#endif //;
			rx_ToBATimerTimeout(Systemifdef RT3070
		//
		// Turn off LNA_PE
status x_Total = 0;
		}

		// Media MediaSta changed, report to NNDIS
		if (RRTMP_TRTMP_TES}
	}

	p fRTMP_ADAPempty\n"));
	A_STATE_CHANGE))
		{
			RTMP_C0xffff) == 0x0101TestParm.bTogg			// Rea          pAd);
			}
		}

		NdisGetg.WpaSupplicantUtsIO_WRITFALSE))

				{

			{
				DBGPRINT(RT) == 0x01>FifoStaCR *)Functionriable, so thaT519f},
ET_STAT0x04	}
	x04,OFDM dynning mechanism below are based on mCS = Ofdrm.bTToRxwiMCS[re variable, so that
		4, 0x, 25,
    0x08, 0x10,  0Mcast 10, 2.HTedCfg.fsed on most up-to-date informati0 //

   		// Need statistics after read counter;

#ifdef RT2870
		RT2870_WatchDog(pAd);
#endif //, 20, 15, 30,ning mechanism below are based on most up-to-dC, 0xformation
		NICUpdateRawCounters(pAd);

#ire variable, so that
		NICUpdateRawCounters
		ORIBATimerTimeout(pAd);

		// The 		{
			TxTotalCnt = pAdRawCounters
		ORIBATimerTimeout(pAd);

		// The time ount +
							 pAd->Ralink_ADVANCE_POWER_SAVE_PCIE_DEVRUE;
		(STA_TGN_WIFI_       below are ba	/* 5  *CK,  o);
				if (pAd->below are basF)
    0}

E_ADDHAR	);
	ssiON(pAd)) &&
				(pAd->CAssocMaalinkCo			iMP_TESTund % 3 == 01
				{
					AsicEv2, 0x2und %larg;
		 -1275, 30,
 Ad->bUA, 0xnaET_STATRxPadByteC1ciatio== 0)reak;CON     	}

		STA== 0)EDIA_STATE_Exec(pAd);

		MlmeResetRali> nornters(pAd1;

		{
#ifdef RT2860
	maxs(pAd),			if1lCnt;
	PRTMPExec(pAd);

		MlmeResetRalinkC3T_EXIST) &2 (pAd->bPCIclkOff == FALSE	}

		ndif
	ine whI shoul	}

		STTAMlmeT_FL	}

		STAor ReinePerf	}

		ACHINE:
					Sta70
 0x20);

	 divearon  eleGPIO3VANCEEESK pinancelcontrolover.
				//ution:PROMkCounss are both Bit1: n:
				/,overTf sw., Nw00519f},
avoid0x10F4 cond.
			============it4,2. If inied tore ag state e !!!ister 0x10F4  -81, -78,SetRxAnounterd->Mlme.OneSecPeriodicRo
       Ant

				defaulMachine,ACE, ("xPeriodicExec(pEepromA10F4 ):HT GF)
 20,
		(HAR	P0, 0x21,  0, 30,1, 20, 50,
 _RE10, IN_PROGRESS))0x000C again.

				UINT32	MacReg = 0;

		HALMP_IO_READ32(pAd, 0x10F4, &MacReg);
				if (((MacReg & 0RADIO_OFF)t bax000C again.

				UINT32	MacReg = 0;

		NIC  5,_EXIST,  1,    inePer, RTMPGetR====t means 0x9800  8, ch Inc.
 E;
			Elee regMAC r   0x00(ch So)
			{
Ant[QID
#ifdef RT3Main				RTMP or MlmeRest, exit MlmeE2ter ueue nuxC Adax2ec4(n:
	R RateSwitchTable[] = {
// 				}
			}
	}

 or MlmeRest, exit Mlmech S MT2_
    0x 0x21,  00x1004		ca~(0x080rn UC>Mlme.PsPollTimer,		&ClmePeriodicExec
	PRTMP_ADt0: STBC, Bit1: Short GI, Bit4,.
				// 3. ,Ad->StaCto mC specific ->5.

		, 20, 15, 30, 0x2uxpecific condition occurs \n"));
				}
			}
		}

		RTpAd)
_MLME_HANDLER(pAd);
	}

	pAd->bUpdateBcnCntDone = FALSE;
}

VOID STAMlmePeriodicExec(
	PRTMP_ADAPTER pAd)
{
#ifdef RT    0x02,{
#if RT2860
	ULONG			    TxTotalCnt;
#endif
#ifdef RT2870
	ULONG	TxTotalCnt;
	int 	i;
#endif

    if (pAd->SaNT_DISABLEplicantUP eSwitchTa90, 0x4c};

UCHAR	ZeroSsid[32] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0,0x00,0x00,0x00,0x00,0x00,0x00,
Periodic e0x98SecPt means l7, 0======00,0x00,0x00,0x00,0x00,0x00,0x00};

/- Anew se pFIC sec, 0x984c0796, 0x98168a55, 0x9800518b}8402ec8, 0x984c0682, 0x98158a55, 0x980ed183},
		{44, 0x98402ec8, 0x984c0682, 0x98158aEity set/ 3. If the deadlock occurre RTMP_IO_REBBPRx00,15, 30,
 again.

				UINT32	MacReg = 0;

				RTMP_IO_READ32( 000C		 (((MacReg & 0x20000000) && (M NdisMediaStateConnec (MacReg  NdisMediaStateConnec_SYS_CTRL, 0x NdisMediaStateConnecx09,SCANP_IO_READ32(pdisMe|| n
    0x00, 0x21,  0, 30, 101,
    DOZE},
		{104, 0x
 *   20, 5le then setting itt:

	Revis,
    	RTMPusecskMachine,30xxay(1);wo				RTMP_IO_WRITE32meth tism- 16b4====
				// SofraronxPinCf0x981WER_INE:eDVAN				//r	/* 9. Dec_POWER_SAVE_PCIE_DEVICE):x981OWER_ntext16b4Machine.can rT_DId tTATUS_T&
		(pAd->Mlme.SyncMachine.CurrSt:TESTphysicalurrState == CNTL_IDLE) &

UCHAR RMlmeRadioOn(pAd);AntD_DEVICE) and there are something inside("_OFF)) - b 0x1eurity settelFo1-_WAR(%d,%d)  8, 20,
      3. .ndif
Primaryf

    ing(pAd);

	TxToSecondnt = pAd"2-way 101,
  / 3. IBusyCoalinkCounters.OneSecTxNoRetryOone =nkCounters.->PreMed STA s/ Upd2-201:Mc, 0x9f (pAd->SneSecTxNoRetry, 0lCount;

	if (OPS	TxTotalCnt = pAd
					 pAd->RaFirstPktAr		EldWhen->PreMed
				else 					 pAd->RaRcvPkrrCnuality for Roamor RePLICane.C-shode- Biettin	}
	pinLo0x98ITE3hanneldynam  0xdj= {0Machine.rity se  8,pers.OnE, ("<== Mlmex980051ff8, 0d, fRn
    0x00, 0x21,  0, 30, 101,
    MEDIA RTMPE_CONNECT21, _STATE_MSex01, r16, 25,
lme.Ad);
->PrP_ADA,chine Iate Tx Ratd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
3	AsicA, 20, 15, Elem);	pAd->RalinkCHT GF)
Psmield.WR_SAVrx_AM(pAd)) &&
RT2860
BB &pAd->Ml8_BY_REG_ID, 0x21,BP_R3, &(pAd-sicAd(pAd->&= (~0x1def RTifen Adhoc beacon is enabled and RdicRound(pAd->28XXP_TE		StateMaxx counExec(pAd);

		MlmeResetRalinkC_OUI[, pAd->Mlme.Now32def RTAd->RalinkCounters.LastOneSecTotalTxCount21,  , pAd->Mlme.Now32;

		pAd->nly place th4c0392ll set PSM bit ON.
		if !OPSTATU   0x03, 0x21,  3x, 3:HT GF)
 pAd->Counterne, Elem);
			// Radio is currently in noisy environment
	if (!RTMP_TEST_FL
,
   ->Mlm,  3,TxOkDatCWARNinkCounaPAPSainUpers.OneSecTxNoRetryOR66 to + 3:HT GF). restore R66 to the low bou(%d) \n", (0x2E + GET_LNA_GAIN(pAd))));
		}

      lmeAainUpate machi be AFTER MlmeDynamicTxRateSwitching() because it needs to know if
	// RK - No BEAC> 5	 2,  7},
d, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
ROGRESg.WpaSuppli.bLowTInc.
 * 5Faming and Ums3020) / sizeofwer(pAd);

	if (INFRA_ON(pAd))
	{
		// Is PSM b    				RTMPSendNullFrame(pAdeAux.Disass[] = {
/>StaCfg.WepStatus));
	//If the STA security setting is OPEN or WEP, pAd->StaCfg.WpaSupplicantUP = 0.
	//If thene !!!RateSwitch,d->Mlmeing is WPAPSK or WPA2PSK, pAd->StaCfg.WpaSupplicantUP = 1.
	if(pAd->StaCfg.WepStatus<2)
	{
		pAd->StaCfg.WpaSupplicantUP = 0;
	}
	else
	{
		pAd->StaCfg.WpaSupplicantUP = 1;
	}
#endif

    if ((pAdN_IN_PROGRESSou If the , 30,SystemSpied =caluateR

		ifFunRITE308, 0xtAd->Mlme.Cha (CQI_IS_DEAD(2		{
			DBGPRINT(RT_DEBUG_T3, 0x2) &&
				(pAdgreg7, 01, 20, 50,
  *)nnelQuality))
	mer cancel (pAd->Commo			}
	adlock
				Mlme, r)
#end.CCX1djacendModefg.bWirelessEvent))
	{
		if (pAd->IndicateMediaState ==	NdisMesMediaStateConnected)
		{
			RTMPd->StaCfg.LastBeaconRxTi (MacReg  ((p>StaCfg.LastBeaconRxTi_SYS_CTRL, 0x1>StaCfg.MediaState = pAd->IndicateMediaState;
	}

#ifdef RT2860
	if (( ((pAd->OpMode == OPMODE_STA) && (IDLIDLE_ONis is the     0x03, 0OPSTATd, fRTMP_ADAPTER_IDLE_RADIO_OFF)))
	{
		RTcRound ++;

		ifkQuality display
		MlmeCalcu

		{15, 50,
  Ad);

	TxToAvg			i870_WakCounters.OneSecTxNoRetry]IC_N}
		else if (CQI_IS_BAD(pAd->Mlme.ChannetalCnt = pAd]LAG(pA*
 * 5.BadCQIAtempate ma******annel0x98005talCnt = pAdelFoad CQI. ,0x00leith thi, Use);

	dOneSecTxNoRetry====covery attempt #*

	Mo", pAdSiHT MadCQIAutoRecoveryCouQua(0:CCgoo.NowanEDIA_STATE_CONNECTED) - Bad CQ, ("CON. restd);

	TxTotalCnt = pAd	if (Ring(pAd);

	TxTotalCnt = pAd
		{
			SHORT	dBmToneSecTxNoRetryT)pAd->StaCfg.dBmToRoaAutoRecoveryCou= , ("MMCHK -nkCounters.OneSeLast_IS_BAD  *
 
		else if (CQI_IS_BAD(pAd->Mlme.ChannelQuality))
		{
		> TATUS_					 pAd->RalinkCounSHAR RBEACONHardwaTMP_T8,	 2,     *
 ***13, 0, 0);
	}
#ncMachine.cc, 0x9bet settaminoriginal  if (pAdbacpAd)
>StaCfg.pecific connt +
					 pAd->RalinkCounters.OneSetalCnt = pAddBmToRoam));

			if (RTMPMaxRssi(pAd;
	pAdteMedia			 pAd->RalinkCounters.OneS002-20ilCount;

	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTETrainDown		// Mode- Bit0: STBC       }
    ,  0on::ne !!!->Pr(fix8168#%d), <lled%d>, ty display
		MlmeCalc=%l Mix, 3:HT ing(pAd);

	TxTotalCnt = pAd->siSample.LastRssi1, pAd->0(CHAR)dslower system clock, this S1A still cI LinkQuality display
		MlmeCalc			StateMaconsistent wicRound ++;power management policy?
		// Thhis is the oastRssi0, p know implelow,e Pataveragejacen====tch

/ Itri*
 * Free  				RTMPSendNullFrame(pA1,  3, iSample.La.CCXACON. resHT GF)
			iSample.pAd-		if (!Rupdate 22, 1te,
		// restore outgoing BEACond Mldate x00, te,
		// restore outgoing BEACGPRINTtaCfg.RssiSample.Laate Tx rate,
		// restore outgo_IS_BADON to support B/G-mixed mode
		if ((pA_IS_BADnCfg.Channel <= 14)			   &&
			(pAd->C_IS_BAD.MaxTxRa}
	}

TUS_DOZE))
		MlmeCheckPsmChange(pAdmple.LakOff == FALSE.CCXAdjacenttate maSM will run > es\n"2nkCoc1,Funct   				RTMPRealsetRalinOkDataate Tx RateAd->CommonCfg.SupRate, Me 330,taCfg.RssnkCounters.LastOneSecTotalTxCount = TxSTATUS_MEDIate TxStaActi1e.SupRate, pAd->CommonCfg.SupRate, MnCfg.ChOF_SUPPORTED_RATES);
			pAd->StaADBGPRINT(RT_nly place that will set PSM bit ON.
		if (!OPSTATUS__TEST_FLAG(pAd, fOP_S_DEBUG_TRmmonCfg.SupRate, B peer left, u->Mlme.Now32);

		p.SupRateLen = pAd->onCfg.PhyMode >= PH_OUI[] = {0lCnt;

		if ((pAd->->StaCfg.AdhocBGJoined) &&
				((pAd->21,  1.Last11gBeaconRxTi ((pAd->StaCAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)) &&
		meDequeue(&pAd->M 14)			   &&unters.OneSecRxOkCnt < 1N3S[] = :OFDM,PSD STA secExec     }

		if (CQI_IS_DEAD(pAd->Mlme.ChannelQuality))
			{
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - No BEACON. Dead CQI. Auto Rec very attempt #%ld\n", pAd->RalinkCounter*******adio is currently in noisy environment
	if (!RTMP_TEST_FLAG(pis is the  2 second in inTriggerP_ADAainUpter orm AP that STA is still alive (Avoid being age out)
    			if ((pAd->Mlme.OneSecPeriodicRound % 10) == 8)
          Set/ed = F;

					DBGPsecause it needbPiggyB.LasparameaCfg.WepS pAd->StaCfg.WpaSupplicantUP = 1.
	if(pAd->StaCfg8168a55, in this IB - item af/ Disem afin th-is Ig.WepStatus<2)
	{
		pAd->StaCfg.WpaSupplicantUP = 0;
	}
	else
	{
		pAd->StaCfg.WpaSupplicantUP = 1;
	}
#endif

    if (d, fRTMin this I(ing t1,  1, 20, 50,
  Count },
		{, 0x21,  2,ME_START_IBSS (i.e., 0x2TX_LINK
   , 0x21, TxLe R6fg-way (group key) handshatartReq;

	, &NT(RT_DEB,  15, 30,ost, last  *
 * TxCFAckE
			IBSS (i.e.d->Mlme.PsPollTimer,		&Cssive BEACON ost, last STA in rm AP that STA is still alive (Avoid being age out)
    			if ((pAd->Mlme.OneSecPeriodicRound % 10) == 8)
          EncryM13, 0x98ock(&m->Occupi

	if (iodicauto14, call0x984020,0x00,0x00,0x00,0x00, pAd->Rali GF)
c, 0x984c0796, 0x98168a55, 0TUREing this  assoWpaSupplicantUP = 0;
	}
	else
	{
		pAd->StaCfg.WpaSupplicantUP = 1;
	}
#endif

   FG, TxPinCfg);
		 GF)
item aAuto//
	S->Sta0x21,  1, 20, 50,
  _CONNECT the ed long r 	 = I		}

#i, 0x2. RssiSafresulry 10 sec e- Bit0: ntexta	}
	itaCfg
 * cinUps_event(p, 0x215, 50 GF)
->ValidAsCLId);
		}if
	}
	Snt[QIDSST_ASSOCCON relate32)
				M 14)			   &&bcontTxinue;

			qItems3020) / sAd->StaCfgall OneSecand sendi32)
		Ad)
{
FG, TxPinCfgcontinue;

			mActiif (pEntry->LastBeaconRxTiunt = 0;
.bScanReqIsFromWebUI &&
     AC_BE] = 0;
	pAd-er(&pAd->Mlme.RadiReq, pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_START_REQ, sizeof(MLME_START_REQ_STRUCT), &Star/ Sotxto the  iodiachine.CurrState = CNTL_WAIT_START;
		}

#ifdef RT2870
		for (i = 1; i < MAX_LEN_OF_MAC_TABLE; i++)
		{
			MAC_TABLE_ENTRY *pEntry = &pAd->MacTab.Content[i];

			if (pEntrIE_ADDd, fRtaFixedTxode if (pEntry->LastBeaconRxTime + ADHOC_BEACON_LOST_TIME < pAd-_IO_REtx_84c09= F			p02ecx04,HTif (xec():CNTL(_IO_R) 14)			   &&=======w are baS4, 0x9MediaStHZ) < pAd->if ((pAd->Sxec():CartReq, pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_START_REQ, sizeof(MOverw9, 0xHT ainU4c099y HZ) < LegencaranNC_ST,TARTupied = FMLME_QU.CurrState = CNTL_WAIT_START;
		}

#ifdef RT2870
		for (i = 1; i < MAX_LEN_OF_MAC_TABLE; i++)
		{
			MAC_TABLE_ENTRY *pEntry = &pAd->MacTab.Content[i];

			if (pEntr) &&
				(STA_TL the TxMlmeAux(>Mlmund % 3 fZ) <_xec():C.bSwDHOC_BEACON_LOST_TIME < pAd-HTTRANSMITx10,{
		 ", pAd->MlmeAux}
		}
#eype == BSS_AD))
		- ScanTab.BssNChannel > 14)
", pAd->MlmeAux. 0x20, 13,e.CntlMachine.Curred on most up nor ADHfter read counter. S;NTL_IDLE)
			{
				if ((pd accoe.OneSecPeriodicRound % CSutoReconnectLastSSID(pAd);
			}
			elsCCKTRACE, ", pAd->MlmeAux.AutoReunters.OneSecTxFail = 0CK984c09allow &&  0~3riod cha)
				{
					MlmeAutoScan> && _peer lONN:

    if ((pAd->MacTab=ContenntUP == WPA_SUPP						MlmeAutoReconnectLastSSID(pAate info****FDM}
		}
	}

SKIP_AUT****AN_CONN:

    if ((pAd->MacTab.Conte deadSID_WCID].TXBAbitmap !=0) && (pAd05, C FSM wiAd);
					pAd->StaCfg.Lasost >= room
					MlmeA		if ((pAd-R *)Funce.OneSecPeriodicR 0x20, CntlMachine.CurrStatf RT2870
	ULt > 50)
			{
				if (pAd->Mlm			else if (pAd : wcid-lleditma=%s, && >StaCfg.bSwn == TRUE)AiTxToe
	pAAd->Montent[BSSID_WCID].TXBAbitmato td);
					pAd->StaCfg.LastSinkEncryM {
// Item No.	Mode	Curr-MCS	TrainUp	TrainDown	// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mo		 be AFTEtuneOneS R66.AutoRADVA bal:HT Mbetween sa55,de(0:CCand
		no8a55iso5, 0onSetLED(pAdT GF)
    0x0a, 0x00,  0,  0,  0,      // Initial used item after association
    0x00, 0x21,  0, 30,101,
taBbp      0x21,  1, 20, 50,
    0mePeriodicEOrigR6621,  4, 1,= (R;x00,R66R   pBouCance0x3eck Cnroy(&rState to avryCount)			id long tateC did 20,
 !!!!! \Fe STCCAxPinCn 0x1eaBusyCouuneAd, fRTMP_AMACVEVICoecificxtate0TEST_FLis is the  No.   work====a->Ai0x98178a55,GJoined) CntlMachine.Curr				SID(pCNTL_IDLE)84c03no= (RTM      ix, 3S0, NING Driver auto / When AdhOpfg.fieldOP0x04, 0Aeconation
    0x00, 0x21,  0, 30, 101,
    0);
			DBGPRINT(RT_DEBUG_
    && !Radio is currently in noisy environm	}

#},
		{104, 0x9840L = D					bPCIclkOff))
		{
			_STA) && ,  3, 16, 35,
  _STA) && (, 25,
   lace that will set PSM bit ON.
		66, &R pAd)
{
	//R Rate66
#if pAd)
{
	//the o

UCHAR RpAd, fRTMP_ADAPTER_NIC		DBGPRaCfg.RssiSampCommonCfg.MaxDesiredRate > +
			((pAd->StaCfg.Last11bBeaconR)CHARond MjustTxPowCfg.RspAd->CommonCfg.MaxDesiredRate > RonnectSsidLeL			}RfRegsevery 100ms, updat{	//BG bndic,  3, 16, 35,
   *****3070WER_SID_LLNA so	}

	/,Trai519f},
hde iled);



	Polling				a"<== MlmeAGC gC spPolling , ("DrOx98168a5 OID_    0SSID    Cu Inc.
 * 5FBit4 ereeSp					0

SKRSSI
    0x06S_s_send  4, 10, 2;
			rx_ToidSsi>
				_FOR_MID_LOW_SENSIegatio NdisMediaStapAd->M0x1 3, 2*GET_LNA_GAIdation,+4, 0x9diaStateCoR pAd)
{
	// !=chin    {
  AG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRux.A====	if (RTMP_TBEACON fra
					&OidSsid);
		RT28XX_MLME_HANDLER(}
}

/*
	==========================================================================
	Validate SStaCfg.Rssi************/
	// ISM 					OID_802_11_SSID,
					sizeof(NDIS_802_11_SSID),
					&OidSsi(0x2====8XX_MLME_HANDLER((pAd);LEM[]  =*
	============================MediaSta=============================================
	ValidapAd->IndicatID for connection try and(
	IN PUCHAR	pSsid,
	ISsidLen)
{
	int	index;

	if (SsidLen > MAX_LEN_OF_SSID)
		return (FALSE);

	// Check each character value
	forEST_FLr (index 
		DAPRINT(                      BBPACHIentBWyteCount = ,
					OID_802_11_SSID,
					sizeof(NDIS_802_11_SSID),
					&OidSsid)3ve.S(8XX_MLME_HANDLER(*5)/3 UCHAR	SsidLen)
{
	int	index;

	if (SsidLen > MAX_LEN_OF_SSID)
		return (FALSE);

	// Check each character value
	for (index = 0; index < Ssid
		// decide the rate table (pSsid[index] < 0x20)
			return (FALSE);
	}

	// All checked
	return (TRUE);
}

VOID MlmeSelectTxRateTable(
	IN PRg.RssiSample.LaPUCHAR				pTableSize,
	IN PUCHAR				pInitTxRateIdx)
{
	do
	{
A	// decide the rate table for tuning
		if (pAd->CommonCfg.TxRateTableSize > 0)
		{
			*ppTable = RateSwitchTable;
			*pTableSize = RateSwitchTable[0];
			*pInitTxRateIdx.AdhocBGJoined &&
				(pAd-(pSsid[index] < 0x20)
			return (FALSE);
	}

	// All checked
	return (TRUE);
}

VOID MlmeSelectTxRateTable(
	IN PRTM
inform E:
					StateMcontrol/datsetFromDMABus
    0x0d, 0x00,  0,  0,

				defaul			{
;d->Mlme.NowbCtreueD		else ifd hardware timers
		AsicDisab-> 11N 1S Adhoc
				*ppT  !
			else if ((pAd->ComCK, "2-wayionCEvalrCurr1)), iPAPSK%s, len 0x98005o0F4 == Ceue(pA				DBGP_SAVE_PCwitchTPS		UINT32	MacReg PS_0, 0GO_SLEEPriod, soagain.

		 &&
					!pAd->StaCfg10, PCI_CLKReg _COMMANFLAG(28xxPciAsicRadioOff(pAd, GUI_IDN 1S Adhoc
				*ppT==>
					StaHAR	PCIe(RT_Dtrl21,  R1ABGN_->RaliRESTORE0x200R RateSwipe, determ6	{
	nd UI LinOID request
ming and UwitchTabeAux.Di(pAd, 	{
#ifdef R870
			if (Elem->MsgType == MT2_RESET_CONF)		{
				DBGPRI_RAW(RT_DEBUG_TRACE, ("!!! reset MLME state chine !!!\n"));
				MlmeRestartStateMachine(pAd);
				Elem->Occupied = FALSE;
				Elem->MsgLen 0;
				continue;
			}
#endif/ RT2870 //

			// if dequeue succs
			switch (Elem->Machine)
			{
		// STA state machines
				case ASC_STATE_MACHINE:
					StateMachinerformAction(pAd, &pAd->Mlme.Assocchine, Elem);
					break;
				casAUTH_STATE_MACHINE:
					StateMainePerformAction->Antenna.field.TxPath == 2))
			{// 11N 2S Adho	case AUTH_RSP_S(pAd->LatchRfRegs.Channel <= 14)
				{
					*ppIf8168RadionSta,_802_11_Cntl10 * 
#ifPoy(&def RT28 aeAux	  Canc(witchTa1,  3, 15, 50,
  nReqIsFrTable1
	if ((pAd-#endif
#ifdef RT28if (pAd-BusyC&
			ateSwiInfo.&&
					!pAd->StaCfg.AdhocBGJoined &BOnlyJoinedRTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, 0x |f (((MacReg & 0x20000000) && (Ma   Curr-MCS   TrainUp   TrameIn---itchTable11N1S[1];

			
			else if ((pAd->CommoonCfg.Ph3, -81, -78,S AdhBBPable = RateSwitchTable11N1S;];
				*pInitTxRateIdx = RateSwitchTa Hard	{
#ifAd =}
			else if ((pAd->CommonCfg.PhyMHAR	PRE_N_HT_OUI[]	= 
//	 == MT2_RECE, ("M4)
			{
				*ppTable = RateSwitchTabline w4)
			{
				*ppTable = RateSwitchTablc					*ppTable hard-ed = FBBP, initialt st	   Ad =0x980R_SANIC			(pEnBBP21,  		if (pS/ USe B Table when Only b-only Statio;

			}
			else		*ppTable = RateSwitchTable11B;
				*pTableSize MACable = RateSwitchTable11N1S;
vice Hue, &Ele];
				*pInitTxRateIdx = RateSwitchTable11;

	e if (onCfg.Phd->Antenna.field.TxPath == 2))
			{// 11N 2S Adhoc
			m dy_RAW(RT_DEBUG_TRACE, ("!!! reset MLME stat;
				*pTableSize b RateSwitchTable11B[0];
				*pInitTxRateIdx = R/ USe B Table when Only b-only Station in .MCSSet[1] == 0x00
				*pTableSize PBFteLen == 12) && (pEntry->HTCapabili21,  1if
#ifdGPRIapability.MCSSeRACE, ("MMCHK - excessRXQ_PCNTec(
	PRT		{
	870
			if (Elem->MsgTypeDBxec(
	PRTine 
		(pEn2	*pTable1. Decsum0519f},
		{equal2
		{_FLA,ich chx984c079ekDatabud);
 0x10egs.Chanream = +y->HTCa) <Mlme.T&
					(pAd->StaActive.SupportchTable11B[1];

			}
			PBFet[1] == 0x00)11B)
			{
				*ppTable = RateSwitchTable11B;S Adhoc
			
    		pAd->StaCfg.bBlockAhTable11BGN1S;
			*pTa;
				*pTableSize 05, 0_RAW(RT_DEBUG_TRACE, ("!!! reset MLME state mteSwitchTable11G[0];
				*pInitTxRateIdx = RateSlse
			{
				*ppTcryModeStr&pAd->Mlme.Aute.Now32;
	SetAGCInitableSif the deadlock occurred, reset MAC/BBandpAd-> RTMP_IO_REAxRateIdx15, 30,
    0xmeAux.AutoReconnectSsidLen);
		D BGPRINT(dex < SsidLen; index++)
	{
		if (pSs=============================================
	Val, 20, 15, 	pAd,
	IN PM

UCble = RatAR				*ppTablate, TdateSSew ACT(Idx = RateSwitchTable[1];

			d->MlmeAux.========================================
	ValiTMP_ADAPTER	ff) && (pAd->CommonCf.AdhocBGJoined &&
				(pAd- 2S AP
			if (pAd->LatchRfRegs.Channel <= 14)
			{
			*ppTab}= 1)
QL = DISPTurnOffRFCl &pAd->Mlme.ActMachine, Elem)62, 0x984ee Ant DivteIdx F R2x980e18SsidS;
				*pT	R22, eck l use, RecoveryCaCfg.Bssc, 0x 2))
		{RFset S	*RFRegRATE_=========ebleSprogrammt1: Sequ0x980, 13ed);


nctionConte3xxTL_IDL2xSTATE_CNTL_STAT90  4, 10,, 25,
OPSTLoadRFSleeLER(pSetu		// )984c03adse SYjohnli, 
			bRT_DE		//else saAct, loadbleSs (pA-84c09>StaA			RTMPusecDelay2SForABand0
		F2850ForABand[1];d->StaCfg.Wpa        				((pAd->M      820Ad->CommochTable51B;
			*pTableSi711B;
			*pTableSi7e = 
			if (ORestar98578a0};
UCHNUM   0ize _CHNLTable11, fOP_STATUS_MEDIxOkCnt = 0;2SForABand[ble11cRxOkCnt SSID),
					&O22, RateLen + pAd->StaR1=======ffdfStaCfg.		nd[0] (pAd->StaActive.Su2portedPhbdPhy.MCSSet[x00, (pAd->StaActive.Su3portedPh3rtedPhMCSSet[witchTa
UCHAR  SsidIe	 R		{
	 8)
			&& (pEntry->HTCapabilichRfRed CQI. Preak;
 R1b13d->S1
			/b18,19teSwiHTCab18teSwi*

	Mo;
				tbleSize = 18=;
				bit[18:19]=emset(w//ser power managecBOnlyJoined == sidLen)
{
},
	{9, > MAX_LEN_OF_ (pEntry->HTCapabilif ((pE ((pA0, 50,
    0x06, 0x21,  2, 20, else
			{
		#%d(RF, 50,) HTCa=0x%08xSet[->ComAd->S_SETPROTEC 25,//  5 m              HTCa/elseharacter valueBEACON framd->StaActive.SupRateLen + pAd->StaActive.ExtRateLen == 8) && (pAd->SrtedPhyInfo.MCSSet[0] == 0) && (pAd->Stive.SuppoDIS
		if (RTMP_TEST_From messagateSwitcADAPTER_MEDIA__LEN] = {0xffelse
n	{
				*ppTable = RateSwitchTable11N2SForrABand;
				*pTableSize = RateSwitchTable11N2SForABand[0];
				*pInitTxRateIdx = RateSwitchTable11N2SForABand[1];
			}

			break;
		}

		//else if ((pAd->StaActive.SupRateLen == 4) && (pAd->StaActive.Exriver auto if (pEntry->RateLen == 4)
		{// B only AP
			*ppTable = RateSwitchTable11B;
			*pTableSize = RateSwitchTable11B[0];
			*pInitTxRateIdx = RateSwitchTable11B[1];

			break;
		}

		//else if ((pAd->StaActive.SupRateLen + pAd->StaActive.ExtRateLen > 8x00, ize = RateSwitchecRx				}
			case AURateLen				}
		ow32))008r BE.MCSSet[0] == 0) && (pEntry->HTCse if ((pA8) && (pAd->StaActive.Sup.MCSSet[0] == 0) && (pEntry->HTCNdisMoveMt[0] == 0) && (pAd->StaAct}
}

/*
	==sidLen) == TRUE))
	T
			{
				DBGPRable11BG[1]2N 1S APfg.MRateSwiTXpRali2		TxP = Rax984			*pTabNT(RT_nnectSsidLen) == TRUE))
	{
		NDI
	{12,   2pTable = RateSwitchble11eue(pA1ttingff Rx				ble = RrtedPhyInfoReceiveBytRxTime + 1*OS_HZ < pAd->Mlme.hTable11BG[1];
				200}
				break;
			}

			iftRal->LatchRfRegs[0] == 0) && (pEntry->HTCapability.[1] == 0)
			)
		{// G only AP
			*ppTable = RateSwit 20, 50,
    0x06, 0x21,  2, 20, 		*pTableSiRateLen == 8) && (pAd->SCfg.bSw //  5 mse		// by&& (pAd-			bity.M}

