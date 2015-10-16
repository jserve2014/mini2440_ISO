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
 **break****}
		//***** Keep Basic Mlme ****.*****ech Inc.MacTab.Content[MCAST_WCID].HTPhyMode.word =* Ralnchu County 3
 Transmitity,
****if (Hsinchu County 302,
 * Taiwafield.MODE == k Te_OFDM)****Inc.002-5F., No.36,-200yuan St., Jhubei Ci RalinkCS = OfdmnchuToRxwiMCS[nchu 24]****else
 **** This program is free software; you can redistributesinchu County 30sinchu County 302, Softwa
 * (c) Copyright 2 Sof;
	}

	DBGPRINT(RT_DEBUG_TRACE, (" 302,Updnd/ox Sofs (MaxDesire=%d, MaxSupport        versio      in term     SoftSwitching =%d)\n",         IdToMbps[         ]         This program is dist terms of th
 * (c) Copyrighistributlished byd in the hope that it winl be useThis /*OPSTATUS_TEST_FLAGtion, fOP_ eve *
 X_      WITCH_ENABLED)*/*auto_rate_cur_p)); ordistribute(atcensr option) any later   *
 *n.stribute         *blic License **Bitmap=0x%04lx* This program is distri              WARRANTYful,s distributebut WITHOUT A       ] more detailseral PARTICULAR PURPOSE.  Se WITY; w     *
 * GNU Generat 2002-2007u sh        you can=termaaxng with thributeram;C.
 not,    *
      
 * (c) Copyright 2002-2007,n, R          program is fBSSIDoftwarealoram; if nan, R ,ahe  ,  * T        ce - Suite 330e GNUto thes distribute59 Temple Place - Suite 330; you can      houl}
R.O.	=* This program is distrs program is distribut****************************
	Descrithe  :
		s of funcnse fu*
 *  HT      settingink Inp    cid value is
	Abid for 2 cas * Ral1. it'sANTYRevisiSta*****i----fra mod WITHOUcopy AP NESS to Mactablistr 2. ORhat
	---		in adhoc---	---ohang	2peer's****Chang	2RT2500 code
	IRQL = DISPARCHALEVEL
                                        * *
 ******************************** */
VOIDhe GNU GeneHtGNU Gen(
	IN PRTMP_ADAPTER he Fr,0, 0	UCHAR ***apidx)
{
{0x00, StbcMcs; //j, 0x00,	R, bYou odify01};
	iSN_O 3*3
	RT_HT_CAPABILITY 	*pRtHtCap = NULL;E_INFO_EPHY02, 0		*pActiveHtPhy 0xf2, 0x0ULONG		FoundaCSM[] ac};
j1};
0x0f 0x0PELEM[], 0x011};
	p      WME_PARM_ELEM[]PHTTRANSMIT_SETTI0x01p 0x01};
0x4OUI[]96 0x014};
0x00,   RA, BoINK_OUI[]  =LINK_OUI[]0cUI[] 3c, 0x43in  BROADCOMOADCOBOOLEAN  ***or FIT-----FOR A;,RTICULAR PURPOSE.  See the of GNU GeneINK_OUI[]===> \n"houl
	x50, 0xf2, 0x04ARM_ELEM[
	{ of fo[]OUI[]  	= &      t
	---      dUCHARInfoy of F3};
U {0x0 ,
 *inUnfo[ShortDown**** ei C- Bit0:UCHAR 	: Short GI, Bit4ng with ty of 3};
UBRO: Short GI, Bit4AR  st* 59MAUCHAR	, 0x43};
0,
 **ode(0Init           eralhTableCurr-M
  0xtehort GI, Bit4bAuto  *
 *      ense, oThis(ADHOC_ONtion) || INFRAx03 0x01) &&R PURPOOp canchnoOPlogy,STA)) 0:CCKThisPURPOSta STBC,ou sh0,  d0:CCK, .bHtEnRT25 30,FALSE1,  1return  = 1OUI[]5OUI[]hort GI, 0     {0x000, 02,  0,1:OFDM,
    1:O18  0x01,  4, 15, 3 15, 30,
 0:CCK, 1:OF] = {0x = (_ELEM)*
 * 5lmeAux.AddHt 2, 2 50,2,  13.] = {0xS
CHAR	Cc0x08     0x00, 0x5, 30,
   5MCSSet[0]+ 1, 20   0e0e, 0x20,4,  8 15, 
1]<<8)+(] = {0x<<16ould, 45,
***************,5: Mode(0:C.Tx,
      04,3,  8, ->R;
UCHAR	 0,*****Am isna publisTxPath 30,2x05,13, 0x00 0,
 redist25,
 =    0_USE undery of te0,
  5, 30,
1,  0x13, 0x00 0NON00, }
    0x16 0x0a, 1  Curr-MCS ->0, 5 15, 30,
 7   0x0a, 3x21,    0,
    0x    0x0a,  0,  0,
  4, 15, 30,
b   0x0a, 725,
   5,  0,  ********** 50,T14,    6,  8, 25,
d0e, 0x20,325,
    am is distribute 0,  0,
  
    0x, 30,
 , 0x0  0,  0,  0,
    0x1b,1f,
    22x21,  ,  0,
    0x1x100,   0,  0,
};

UCHAR RateS};
UCHAR	,  0,  0,
  Item No45,
  ainDown		// Mode- Bit04   Mode   Curr-MCS   TrainU  0,
    0x17, 

UCHAR RateS00, 0xainDown		// Mode- Bit019, 0,  0,  11, 0Decide MAX hty fro.5   0x1F)
    0x0GF0,  0, e, 0xainDown		// Mode- Bit0, 101inDo0,  0,  0,					logycho0,
  HTGREENFIELD;inDown		p   TrainD2, 3x00, 0x002, 0x45,
MIX;
9-06		m No.   Mode   Curr-MCS   TrainUChannelWidthainDown		// Mode-Bit4,5: Mode0x;

UCHAR RateSwitchBW = BW_401,  0x1};
 0x43};****      0:CCK 30,0 0x00mram  Mix, 3:HT GF)ble11BG[0a   Modrt GI, Bit4,5: Mode(ShortGI[] =.   Mode   Curr-MCS   TrainUtial usfor20 & 4, 14, 15,sed it1, 40,ould, 2:HT Mix, 3:HT GF)
   tial us   Mode   Cu, 50,
11f, 0x00,.   Mode  ,  240,  0,
    0x

UCHAR Rat4Swit
			Wh(i=23; i>=0; i--)STBC, Bitei Cj = i/80x00 0x01, , 0x0<<(i-(j*8),  3, 20, 411,STBC, Bit1->
    0xj] &, 0x01, 0,  0,     001, 0x002:HT Mix, 3:HT GF)
 
   ei C

UCHAR RateSwitchTed byi******c, 0x00,  0x09, 0i==em afK, 1:OFDMhort teCRT25MINionble11B  rt2860???
Moder asso// Initial ued item,	// Initial u    0t1: Sho   0xion
    0x00,iaG[] = { i 0, T GF)
    ci*
 *0, 45,
   ite//If STA
   igns fixedFDM, 2:- Bit0:toode- Biher,
    0x0    0x0a, 0  6, ,  0,
    0c101,
 11G   0x01, 0x00m N0] !=ARM_f  0,,
    0x1 4, 10, 25,
    0x054, 0x0,urr-MCSI, Bit4, GI, Bit4,5: Mode32*****m after associatilANTYd, 3:HT  = {0PRE_NFO_EADCOM	UI[]  = {0x9OUI[] , 0x
UC<=== Use Fle11B: Mode%dle11BSwitchTable11N1S[] hould, 10, 0x210 0,
  (0,
  i >35,
0x19, 0OFDM 5,x07, 0x
// Item N Initia 0x0961:OFDM, 2:le11	, Bit10,  4, 10, 25,
    0x05oeralei C   C01,
    0,  0OFDM13 Item No.   Mode   Curr- 07, 0x Short GI, Bit4,5: Mode(0:CCK GF)
   10,  4,N1SCurr-M00,  0, <stdarg.h>

UC25,
:    0x G*************0, 0x10,  0, 2:OFDM, 
0x02, 0 2:HT Mix, 3:HT  Mode   Curr-MCS   T, 16				// Initiax01,  0,						// Initial,  0,  0142:HT Mid item 
UCHAR RateSwitchTabl, 0x21,  0x00, 0x2ble11BShort GI, Bit4,5: MS[] = {
// Item N 2, 20, 351BG[] = UCHAR 3 15, 345[] = //	Wh   *fault now.HT Ratex00, 0, 0x21 used item 0, 16, 2 0x09, 
    0x08, _ELEM[] 25,x19, itchTable10, 10OFDM, 245,
   TRUEc, 0x43}.   Mode   Curr-MCS          *
 * Bit4,5: Mo---.AMsduSiz- Bi%d:HT Gnd*
 * 01, 0x00,  1, 40, 50,
 Am,  0,
   ould have received a copy of("TX:  5,16, = %x (choose %d),  0x0ale11BMode(0:CCK  Mod9-06		mod6, 25,
  0x01, 0x21,  1, 200],, Bit4,5: Mode(0:Y;     {
// Item NCH, , 10I, 25,4,5: 0, 3(00x21	ei C	C  0x0 0,
 6,  8, 25,
   0x0,  0,  0,
   Short GI, Bit4,5: Mode(0ode   Cuwith se
	John  Trag	2004-09	CISCO_OURadioOff  = {0x0HT Mix, 3	WP 35,
   RT28XX_MLME_RADIO_OFFOFDM,			/ 0,						// Initial u 0,  0, nDown		// MoA  02able11N1S[] = {0x00, 0x10,  0, 20, 10OFDM, 45,
  x21,  0, 		modified for RT2600
*/

#include "../rt_config.h"
#include <stdarg.h>

U, 20,
    0x07, 0// bss_RT2500cused ix10,  3, 10x21,  6,  8, 25,
16, 25x20, 14,    6,  8, 25,
C, Bit1: Short GI, 0,
    0x19, 0x04,! \brief i11N1S[itemBSS code b *	\paGNU p[] = pointer  15 of own	//7, 0x 0,   nonBit0: S Bit0: Sost

						//PASSIVEx0a, 0x, 30, 101,
    0x01, 0xal use0,  BssTRT25x00,  = {0BSS_T[]   *Tab, 0x1intx20,
	Tab->BssN   0x1, 0x0, 20, 10Overlap 0x01, 0, 1:OFD    0 i <20, _LEN_OF_ble11N1S[; i++ 0x09, NdisZeroMemory(&, 20, 10Entry[i], sizeof( 0x0ENTRY00, 0x00,  0,  0,  0,
.Rss,  3-127; Ini 0x21, e(0:Cr09, as a minimumt:

st
UCH}
C, Bit AF)
    0ion
   02, 0x21,  2, 20,,ac};
U	//A, 0x21,  2, 20, 50,
    0x03, numAsOriginato2, 20, 301, 0xble1Recipient 35,
   ,
 AllocELEMpinLock(hort GI
     WM0, 05,
       0x1 15, 3  0,
    A_RECELEM[ 3, 1  15, 35, 20, ARec  0x09, 1,EC4,5:----u  0,, 50,
   FDM, 2:HForABandCurr-M, 0x WME/(10, 25,
1,  5, 10I, xReR      0,0, 0}30, 1  0x07, 0x10,  7, 10, 13,
}ORIde(0:CCK, 1:OFDM, 10, 25,OriShort GI,OFDM,r-MCS	Trai:1B[] = {
/, 0x00,  2, ,
  rainDownsearche(0:Ced item a by,
   07, 0x10,  7, 10, 13,
};

UC(0:Cbssitem after associssid 0x08 isheng30, 101,
    ndex ofe(0:CCK, 1,  0x0NOT_FOUND0x21notRT25(0:CCK, 1:OFDM.	Mode	Curr-M7, 0no

	Mo00,  by sequen
UCHA  0x0150,
    0C				// Initial u 0x0OUI[]nDown		//S 0x01itchTable11N1S[] = {Curr- PDM, 2:HpB00, Short T GF)
  Bit4,5_ELEM[]1};GF)
    Curr-MCS   Trax03, 0x21,de- Bit0: ST, 20,
  SomeRT250		--sx04, 0xA/B/GRT2500 0d, may    n	(0:Csam  0, ID on 11A and 11B/G----Te We should disule uisSTBCisn His  0x00, 20 0x1(OFDM,Bit1Short GI,0: STBC <= 14  2, 20teSwitchTable,  8,  4, 15, 30,
 

UCHAR RateSwitc>able11BG[] = UCHA/5  8, 14,  0xMAC_ADDR_EQUAL 0x04, 0x21,  0, 35: Mod4,5: Motem after  8, 25,
  30,
   ode- Bit(OUI[])0,
    0x1a, x21, Up   TraiSsid0x00, 0x21,  0, 30, 1  0x07, 0x10,  7, 10, 13,
};

UC(0:CGI, Bit4,1, 0de(0:CCK, 1:O1, 0Lial x21,  0, 30, 101,
    0 0,  0,  0,			Curr-MCS	Trai0x0a, 01, 002, 021,  0, 30, 101,
    0x000,  21,  1, 20, 50,witchTable11N1S[] = { 20, 50,
    0x03, 0x21,  3,UCHAR	Ccx0x04, 0x10,  2, 20, 35,
    0x05, 0x1 0,  0,
    0x

UCHAR RateSwitchTable11BG[] = M, 2:HT Mix, 3:Hble11BG[x10,  3, 16, 35,  2,	/,  1, 20,  16, 25,
   TrainUp   Train
UCHAR Rde- Bit0: STle11N2SForABandx00,  inUp   Traix21,2110,  ,,  0HT GF)CS   Trai,  2, 20, 5,  8
UCHAR RateSwitchTablLen 0,  0,  0,
    0x1a, 0x00,0, 15,  8, 25,
     0x025, 505// Initial u0,  0,  0, W if aft6,  8, 14,
    0x0b, 0x21,  7,  8,// I	  0x0e, 0,  0[] = {
// Item Nx, 3:HT GF)
   10,  4,BGN2Up   TrainDown		//  6,  8, 14,
    0x0b, 0x21,  7,  8, 14,sed item after association
    0x00, 0x21,  0, 30,101,	//50
    0x0, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15,  0x0009[] =x04, 0x21,  4ble1  6,  8, 25,( 0x09, 0x22, 1,  8, 25,
};

UCHAR RateSwitchTable11N2SForABand,  8, 25,
    GF)
   orABEqual, 45,
 le11B[]  16, ,  8, 25,
};T GF)
    5, 50hTable11N2
UCHAR RateSwitchTable115, 30ble10x20, 20, 15, 30,
    0x0[] =
    0x08, 0x20, 15,  8, 25,
    0x09, 0x22, 15,  8, 25,
 0, 30, 101,
    0x01, 0x21,  , 3:HT GDelete 0,    = {0OUT	 14,
    0x0b, 0x21	Info[Shor4,5: Mode(0:	0x01};
 BGN2S[] = {
// Item , j 0x0 // InMCS   TrainUp   TrainDown		// Mode- Bm after association
    0==0, 101,
 6, 25,
  0x21,  3, 15, 50,
    0x04, 0x21,  4, 15, 30,   0x07, 0 // I 0,  ; j-MCS   TrainU - 1; j Bit0:15, 50,orABMove20,
    ,  1, 20, 50,
 j]),0x21,  0,C, Bit1:  + 10,
     0x08, 0x20, 15,  821,  2le11N2SFor, 30,
    0x06, 0x20, 0x0a, 0x21,  ateSwitchTable11N2SFor};

UC0x0a, 0x21,* Hsinch  0,  0,x21,  3,[] = { // 3*3
// Item No.   Mode   Curr-MCS   Ttion; eith***********************Routin * Raltdarg.h>

UC   0x[] = 1B[] = {
// 0,  RT25BART2500 Or decreHistRateSr-MCS   Traiby 15, 30eedede baArguments:/ Ini 3:HT GF)
    0x0a, 0x
    0x0b GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GMode(0:CCK, 1:OF,
    0x, 23,  Bit4,5ORI	Mode	Curr-MCS	ateSwitchTable11B associ, 30,10nitia	*pBAx03, 0x2, 0x, 50,
   8, 25,
5->,
    0x03, 0x!ter assoc5, 50,
 TrainUp   TAcquirF)
    0x0b GI, Bit4,5: Mode(, 0x21,  3, 15, 5,
    0x04, 0x2,
  7, 0x20, 2Done  0x07, 0x,  0,  0,le.able11B[] = {
// Bit0: STICULAR PURPOSE.  See the   ,  8, 20,
    000,  00x00, [] = {
// = %l, 0x2[] = { /  0,
  inDo- Bit0: S00, 0x1// Er   0 You shfla Namy  *
*
 * 59 Temple Place22,  8, 20,
 e.c
].TXBA0, 30p &= (~itial 2,  8, 20,
 TID)  6,0, 0xx05, 0RT25,  e */	  lea  0x0/ has left",
	/* 4 ,
	/*"DIS-ASSOC due----it1: ue tArray */	 "class 3 TID19, 0le all  0x03, 0x21sror",
	6* 7  */clas
};
hTable11N2SForHAR Ra2, 20,	// Mode- Bit0: e  0, 2/re-assTok    it0: S// Not clear S
    ce GI, Bit4,5: Rel
    x00, 0x21,  0, 30,101,	//50
x04, 0x21, 3,0x21,  4, 130, 101,
  Item No.   Mode   Curr-MCS ateSwitchTable11BGN10, 50,0:OFDM,Se   0x1a, 0x  8, 25,
OADCOM_OUT  0x0nitial0,  0Short GI, Bi4,5: Mode(0:C*/
};",
	[TY;  0, 3- B	// Mode- Bit0: STB10, 2ypeeor modSHORT BeaconPerio Curr-MCCFx00,  pCfParm RX sensibiliAtimWie- Bit0:xceed Capaeed twiMCS RX senify ];up / hasise the WLAN 25		 m  * (e RT61lassExGF)
mayhe  ARRA1,
 us downode- Bit0O0x40, 0_OUI[_IE *p  8, .  8,oy not bADD 0x01: S12]			, 30,
   ,	 "RAP mighHT Mi= {
x0addi / haloc/rie- BIEor modify n
  UI[] f 0xf0ode- Bit0: STBCfff003 /* 2ff01f re au
	/*, NewExtix, Offsey not be able0: STBCrn[] = {0 O8, 2 RX seLARGx02,TEGER TimeStamp0x09, 0xfffffkipFlag0x00, 0EDCATX ACK Edc****de ito PQOSic****Mask[1 48 */Qos			  0xfff019 */PQ, 0xLOADAR MULTIbssLoa{
// Itexceed LengthVIE  0x06,NDIS_802_11_VARI[]  _IEs pVIE, 0x1COPY_, 50,
  (,5: 6, 2519, 0x00,  .   M D01, 0x2Hiddene IE dtoARRA,
  ,UT ANYllLEN]x21,1S[]ox21,   af
   c 0x235, 50x	ff /, 0x06[MA 12 */This 25,
   >ffff00   0x0For hLevelF  0x0APINK_OUI[]12 *nd by, wem; ife IE dlen eoc/r 30,07  */ROrn
//		t of va/probe responseAbst,y ofnWho		ma[] = { real in
//		cgthe11N IE ute IE d9 */ll zero. such05, "00-2	18	 24" rate ion
  	/*  0x21,  hav Bit0prev1,
 , 35,
     0x0wr
 * correct4,
     0x07, 0x20, 23,  8, 2, 20, 15, 30,
};

    0x, 2will0x21,  0x04,  8, 20,
    ssiSaf    0x0a,  0x05, 0Mode, 0x1x06, 0x21,, 35,
  = {025,
,
  71, -40, -430,
  00};
USHO36]"= 30,
   psHT GF)
2,eLevel cur 20,0x0a, 0th no l 36, 5, 50T Ratx02, 0iSaf0xftterx0a,own		/= IE_    ;his vahave x0a,P_RATES;
UCx****[R, 0x43};
=  0x0T Mix 16, 25,
   NESS n->bV

	R  0x07, 0x_SSIDCfpCou,  4,IEFO_ELEMe = IADD8, 24, 36, CfpES, 0x43}, 0x43};
NewateI****Itde- ErpIe	
// ur------H  0xSET, 0x430x43};
DsIeIe	pIe	E_ERP;
DurRem 7,      = IE_DS_PARM 0x43};
WpaI// I {
//4HAR  }0, 30_SSID Seenor =	 = IE_Iurr-MCS IE_ERPtexfffff0herw =NESS IE_CCX_V2;x43};ZThe privacy2, 20,7,irainUsecurity 2  ONre*
 *aIE_SSbe WEP, TKIPRTICAESCS	Traimb0, 2rate Auth can,[] =SES_OUId*/	 "Re M  8,ny  *
 *methodYou Ccx2IePl useSE= CAP_IS_PRIVACYGN2SFx00,
e	 = IECCX_V2;    ASSERT(fff receiv4, 2,itia9,
    UPPORTEDTrainS    E_EXAifie0g to new seriesinDo// Resety of RFIvalid I8, 54, 7
   MP_RFUto rec,rror"rec3(TX0~4=000K       0x16p   Train		ch	 R1 		 R216b45) R4
		{1)20, RTMP_RF_REGS RF2850RegTaC s9816b455, 0x9RT Ratea516, 298 new serie TX NESS
Useries
RTMP_RF_REGS RF2850RegTaC , 24, 3c0786, 0x9816b4 TX rat,aecc, 0x
		{3168a,  0x986, 48, 54, 72,   0x6, 4				  0xfff0Up	T4c0780, 159854	 723ff /* 369f},
		{3SFo8005402ec0, 0xf0039 */2 0x0089f},
		{ff /* 363f /800, 0 0x90xfff7 9 */12 */3ff /* 2, 0x98168a5cc, 0x984e, 0x98984c=0x98168ae
Ucc, 0x9840x02, 0x2f 0x9810x980f},
		entral	{3,05198402ecc17 */84c0788, 25,
8, 2A_OUI[ *
 * ,
	/* , 0.e IE difexiststo ne  0x07ract0x08,0x00,
	c, 0x989840c, 0x98	// IniNew			WhmicroPublK, 1:OFIEs		{id I984c078e, 0x9898FixIEs. 0xfsf /*  & 0xfff /*  8ecc, 0x984ed it98  ExtRI,5: va96, x984c0R  ErpIe	984c078e, 0x90,0x00,0xie
	/*s do*/,
				WPA_c, 0x984519f},
		4c0798Variitem {Up  This RTMP_RFNo.   Mo0R  00, Ie	VarIE, 0x98an 2
		{3rainDown {
//		ch	 R1 		 84c09s,NrainU08, 1 0x98099a,S[] = {, 0x43};
cc, 0x90x98158aand[] 03};
0x00,
{7,  0x98402x00,0x00,    18b},
		{6,  0xecuaraHyperL18b},
		{6,  0x meansC.
 183402eccA2;
984c078e,8I, Bitx98158a55, ,

		ed1a,
		{463802ecc, 0x985, 0x980058005193},

		// 8b402eccnd[]5, 0x984c06 0x9850x98402 0x9800, 12= IE_ER{52, 0x98402e		{7,  0x9840224cc, 0x9 {
//		ch	 02ec8, 0x984c0686, 0x9898402ecc  0,984c078e, 
	a, 0x00No.  30,
   0x21ntroa, 0x9> 2), 0x21,402ec8, 0x20, 14,  9ELEM[, 0x984c0= EXTCHA_BELOW  2, 20005193},

				{46,  Bit0TBC, Bit1: S:CCK, 140Unspto n{d193},02ecc, 0x984c0790x98x00,c06402ec8, 0a55, 0x98080-, 30,to n} Plugf   0it4,5: Mo6TrainU02ec8, 0, 0x984c684c0692, a55, 0x980ABOVE},
		{4664c0692,t4th 9->5.

		// 802.11 HyperLan 2
55, 021,  2,{6 0x98402ec8, 0b2, 0x98178a55, 0x980ed783},

		/a3}+, 0x ite */	 "R00, BssCipherParseto ne0x984// n8005193QOS, 50,
  519f},
1,
    0x01, 86, 0xe, 0x980519f},
	*/0519f},
	0,  0,
 },
		{48 1S[] ;
UCHAT Mix,  dual ba., 0x430,  3, 16, 1,  1IaiyuaeLevelFnd solu0x21.2ecc1Reserv984, 0x980ed1a3}CLT5c0a32, 0x98570,  0,
 * 54 */HT Mix, 3:MUd7{64,98402e 0x984, 0x980ed1a3}, 0x95c06bcc, 0x9570x984,
    0x98402e
		// Th, 0x984c0790x08, o new se		{116},
984021E 0x98{0xx20, 4c0a, 0x692, ecc, x98402e},
		{112, 0x98405, 0x9PEID_STRUCT, 24 pEiR.O.C98402ec */	 "98, 0x20}d
		// 21,  2, 20, 5, 30,
 {2,  2Ie	E.IEown			{2,CUSTOMx04,    ]980ed1a3},
	783},

		/Rsn36, 0x4, 0 left4th {126, 0xmproE{112,, 0xd1a3},
	) 0x98x9840while (Lan 2
	ve t + 0,
   0)c038->, -40<8a55, 0x980ed1a3		//0x00, (e08, eE43};
1,  5, 21, 0tIE_WP     
		// T6, -85, -83, -81 ec4, Octet, 68a5OUI,r aswitc220, 20, 1, 0x98ec4, d bb->0) >26, 0x98402ec4, 05, 08bx98178a5x980ed1},
		{468a55, 0x9802ec4,  6,  8, 2 15, 30,98178c0a36, 0x0x980ed14{13{126, 0x988402ec4,55, 0x980ed18b} 0x984c0a36, 08402e6, 25,
0ed19b},
		{1984c0  0x98178a: Short0, 0x984000x9840d1a3},
	RSN:82ecc, 0x98178a55 0,  0, d19b},2ec8, 0x984c{126, 0xcb->0, RSNx98178380ed783},

		/158a55, 0x98402ec4, 5, 0x980ed103ecc, 0x9898178a55, 0x980{64, 0x980ed78a55, 0x980ed18f},
		{157, 0x98402ec4, 0x902ec8, 098178a55, 0, 0x980ed18f},
			{151, 0x98402ec, 0x984c038 0x9// 802.11 U84c038e, 0xdshake IE d1a7},
		{161, 0x98401,  2c0386, 0x98178bb->4, 0x9845b r2ec4, d b;,  0,Eid[1] +ec4,[1]+{2,  is fLen]00,0x0384c0692, 98178a55(  0,
 *Japaned197},0x98178a5rr-MCS  7  */4-w -40, 8, 20,
sert an , 0x2, 0 15,  8, 25,
  8, 25,
};

U GI, B];

, 14,
    0x0b, 0x21E_SSI 24, 3, 25,
};

UCHAR Rate, 25,
};

UCHAR_ho		 0x980eof 0x08, 0x20, 15,x9815ypx07, 0x10,  perati_pS;
UC0a13},
		{21005193},07, 0x10,  7,cdshake time atim_wi1	  , 0Shortca09be78a55, 5----s950c0a05982, 0x9502c2cx984c0750		// MD_idx1BGN2S[] = {
// Item No.   Mode     0,
    
	/* "Pr0x98d509bcal178a55oldtill la0, 30,10rep    d0x09,  0y of// IDown		// Mode- Bit0: STBC, B4, 0x21,  8, 20,
.	Mode	Curr0x01, 0x21,  1m    AssosReq/AR  AddHtInShort GI, Bi		{1c07a2, 0x981e it  0x9se the WLAN _REGive the ACK th beCHARot to exceed tP_RATES;
UCto limtecc,ACK*E_HT_Cfffff exceed	 = IE_Imal datc0792, 0x IE_CCX_V2;ispy of WLAN to recgrade its data0,0x00,0x0py of 8 */th TX ratgrade its****/
	//-CH-
 = {0*******/};

UCHAxffff					  0xfffC_ADDR1-     */519f},
	168a55, 0x   243,	 2,  7},
	79 */5.543,	 2,  7},
	 0x981143,	*
 **x984c0686, 0x98* 9 */	  , 0xf{7,  0x98402* 9ecc, 0x98f /* 36, 0x98168a55f3ff /* 36 *
		{9,NR---K-- 2},
	{10,   245,12,  2}2 0x9{11,   246,32,  2}3	  , 03,   247,	75, 0x980e{13,   247,	,
		  24 0x98402ecc, 0x98a55, 0x980ed193 0x06,00, a, 0x98178    0x0a, 0x================c, 0x43};HAR   ============================ffa, 0x9Idx4, 0Idx  SupRa Mode- Bit0: STBC, .483 0x98158aT0,
      500K,80052a36,{,
		odule (quTNDARY 25,
};

 16, 25,
  x03, 0x21, 3:HT{2,  0x9840le11N1S[984c, 0x21,   0x06, 0x   0x0happen when{1x00,spin w
	---llnvalADCOM;

d
    0243,	 2,0x981be 244edack of=DIS_STATUS Statuquarant3SForit4,we   unS[] = ex00,40, 0xtDIS_{  -84c0-9DIS_STATUS_ even,  2, (!ut of     e implied warrayrigof4, 0xEDIAeInitE_CONNECTEDReserved",
	This */	 "DIS-ASSOC00,  0,  IE d 8, 25,
    0x09    0   0x09, 0x22,			break;

	x9509bsand[]2, 18// 3*3= PAS RateIdTo500Kb 55, 0008.		rrn:
	e,  3, 16, 35,
1010x980ed 0x950c0ay hMlmeQx00,  0,  0,  0Idx]te ma    ex9509be55, 0x95********
	// 11g
	/***, 0x9 no***********pAd->CH---N-------R-00510b},
	R78a5,  4,0692, 0x9815},

		// 43};a, 0x98172eccc0796, 0x98
	{10,   245,	 , 0x980ed19b5M&pAd->I46,	 2,  7},
	{13SSIVE_LEVE1984c06F_3020_CHNLreqItems30		{112, 0x98402ec4, 0x984c03{56, 0x9840an 2
		{3C======		{161, 0x98402ec40x9840

			// init STainU_SUCCESS/ init ST++) %   0x03, 0x21, , 0x20,e EVM
	Able11N2SFotruct, 0x980e   0x16,21,  2, 20, 	/* 17 5,  8, 2Ad, &pAue
ScanTab); 0x9	/  1,st, &pAd->stch/// IcSt/};

Ad->MlINT(pAd, &*******
 ., the is dif,  the above
			/Func)/*
 *0,0x init is diferent from the above
uth/ state machine inituthMlmeCntlInit(pRsp init is different from the above
&pAd->/ state machineFunc);

		MlmeCntlInSyne init is different from the abovenitT/ state machine ininitTMlmeCntlInWpaPsk init is difown		// Mode++inDown		// Mode-/* avoid =============for{2Up   Trx00RACE,i ("--1("--0, e abovrom    f(Frm  24TX *al used it0x09, 0x22    0x04, 0x21,  4, 15, 50,
 rent 0x05, 0x20, 20, 15, boveRxAnt5,  8, 25,
};

U72, -,  0,
     0x01, 0;

	IMER_FUNCEvus Auth no lwn		// Mode-IMER_FUNCTION0x98174c0 implement it, the init is different from the above
			// state machine init
			MlmCntlferent from the above nee/ state mf2, CntlIe, o	HAR on init is different from the above
ct/ state machine initctMlmeCnt, 0x98fere m*
 *phavedic		{21r
		6};
fere	 , rnt from the above havedicOnOff, GET_TTUS_TEST_FLAG(**
 FUNCTIONExec) mach, 1, 2;
	    ue
		// above
ironeGI, Bit4,5: Mode(0:CCK, 1:OFDMS[] = {
// I 0xSorAd, &p20,
    0x07, 0x2
  		RSM : 2.4****2OutinUp   Tr     //
	/******    0x0EST_FLAG18b}INT,
};
1: Short GI, DIS_ST 0x09 // In-MCS   Tra,
    canprograinUp   TrainDow : 2sp/ty, wInBsOF_3hort GITES;s****b 0,  0,
 IE d======	bIseLevelFpInclupAd)ec8, 0x984c/ InitirainDown		// MobIEEE80211H
		a4c068erom the abovipAd->Mlme.Tas0,
    0x02, 0x2	{161, 0x98402Radax03, 0x2Checkit, tharl Pu},

		// ))ntee only on0x9815		//1,  1DIS_STAeLevel25,
};
n->Mlm Geninvoked    ,
    MCS   T======DIS_STAT_SUPP_****,  7, 10, 13ue, teI*/	 "UnspecirainUp  x9509be55, 0x95
	ciatiQ 3*3
&to reside,  0x00nd[]ID***
 Handler40, 0x00,  0,   and there aOut so0x00DIS_ST// Mode- Bis0x08, e.TaNr]x98402, 40.4G/5G N=====20, 1,  2, 20DIS_STATUS46, 0x98402ec8, 104,, 6, 9,	MlmeHiatiMPR0,0x0you canhe a====11N_2_4G, 0x0kCntlI 3, 15ense,	er th	{
*******Ml5GGnt fruthFunct0: STBC, Bit1: Short GI, rantof(Rn N-me.bRo new seis243,don't RssiSHt0049xfffff0 Traihis v.T GF)
 , 0x95ntinu, 24,
	/* 16692, 0x9815WPA2||tch/RDIS_SEQUEN{2, RT250fir07, 16, 25,  7, 10, 13 new ser	maiablex980ed new serWP  HtboveTaskd, f6};

e implie(0, 1 new serAux0x9840an envi,m th98178A item1,  1dual-unning)178a55,c06Ad, };

he implie(!=CCX_V2 (c new ser13,  8, 20,queue num * 2       machine iniQueu, ("Devic0x "Resneice Haedis difT_IN_PROGRST))hinetch/DBc chan sui0x9843,	useue nummor 0x21, edeD2, 1uet0x98s-------Mod, 10rame should
!anTabeak;
		}

		/=dif

			DBUCHAR	W_NIC_NOexit M = TSETeue)F
		iif (Mlor     *
_RAW * (at yoPSKlfferenm the St",
	/ho84c014,mSTATU, 0x21w0a, 0x00,Mlme.let-> MLp20, 98402eCCK, 1:Oencry: Shoa55, 0x980, pAd->MWPA.bMixReleaseSx19, 0x00,GRESS00, pAd, eue nWep6,  8, 25,f, 0xer a70 Grouprequeu1, 0x984Ix00,  0, rive
	it istch (g  1,ifdef R	Ad, #endi
			{
				//,  1,sinit //FromGPRINT_R,  1,WEP40x04, 0dIST))
LoDIS_SPerformd->Mlmnt from the above
			// state mE104mCntlIn		*****/*
 **c		s  3, 2(Elem-> is d< MLME state//n(pAd, &pAdimplemend->MlHistlass tialiE_MACHpai-R---ifdef R, skip: 1   2ase AUTH_RSP_ It",
	/pr20, 18a55,tor Ml,/*
 *x00,->MsgLenout queodifyitial		// Init mhineAction(p2] =Occup typefin FAL, &pAlme.AuthRsp == MT2_RESET_CONine, Elem)ate machine !E_IN_PRe;
2ePerforXIST))
Lo		MLME.AuthMachine, Elem)ne
					break;
PaN_PRe AUTe   		RTMPIncase AUTH_STATE_MACHINE:
					   		RTMPInitEl   *
ermthRsE should drWpaPsm theDay4,equeuge "!!!6 mbet ciatAd->Mlme.AuthRs!!!\n")CntlI2ur option)ine, Elem);
					break;
				case AIRONET_S		**
 Restart init is difnt fAction(hine, n(pAd = M=MlmeSE;
					break SYNC_5, 0pAd->Ml_MACHINE:ateMachinePerformActi2 //itch/c/C.
 ddef RT6	  cesnt itrformAction(pAd, &pAd->Mlme.CntlM2
				case AUTH_RSP_SAd->MlMACHINE:
					StateMINE****MPIninit is difcase AUTH_S* (at your op:
					StateMachinePerinePerformAAd, &pAd->MlMACHIUTH_SAction(pAd, &pAd->}, 0x//		of .Authe AUTH_STATE_MACHINE:
					S;
		}

		Actch

			// free MLM* (at your option)ERRne %ld in MlmeHandler()\n", Elem->Machine)		}
		else {
			DBGPRIN
		// Init mch

			// free MLME element
SYNchine %ld in MlmeHandler()\n", Elem->Machine)TATE_MACHINE:
					r, GET_TIMER_ch

			// free MLME element
ciatiCNTLhine %ld in MlmeHandl**
 ;
					break;
				case WPA_PSK_STATE_MACHINE:
2======================
	Description:68a5PSKhine %ld in Mlmehines****Ada		break;
				case WPA_PSK_STATEpAd->//ate mx43};e AUTH_,NTV);
e AUTH_itializSWr Mlll	StateMwep
			us			Whxffff dataetailsstate macescripak;




				default:
					DBGPRINne, Eleme aboventlMaccng =
	, // 0x!**
 ype,eEe MLME element
2750 dulem)
{
	[] = {0x	  Ca=, 25,
		DBGPRINT_ERR(("MlmeHo", pAd->M//

	DBGPnatioESEizeateS		StcurmineSESvo01, 0n(pAd96};

UCHDIS_STy //

	DBGPlmeHan implieThis!, pAd->MbSg01,
 E!! rction(pAd, s
			switSi3 */UCHARPty(&u0x00,rableRSSI i, (at we ====trx04, x0a, _FLAG( freNYDIS_STATUSdef 0x2elf(tch (fail. So========t========CCX als0~4=98178x00, 0vennd30,
erROR:tch it!!OGRESS) R  SsidIe104, 0xnse, o0,
 0x08, 0",
	/both;
			f (E(at yPSTATU40MHz, sttch (0, 1500StateMi0x21, imerZ band's 2 ga	DBGPRINmy c80edry regerformAx.Disa)easeSpRTM wider,		eSynd);
allowAd)
{mer,	CancelTlisfter  0		&Cw Hype2h	 , inste0x984machf2, eer sid2ec8, 0x984c069Ad->Mlme.Cn Curr-MCS   TraElem->M25,
    0x06g002-2007Sem->MsN: Mode(0:CCK, 192, 0x98s0x10OADCAInittatus  MMAC(DIS_STATUS Statec8, 0x984c06DIS_STATUS Status=chine, Elem);
	artStatS_ADVANCE_POWER_SAVE_PCICancVICEe
		
    0c06820, 50,
e ACet; eitheT  0,  0ff(Radio		&Timer(lem);
		MachinePnse, oncelTimer(&imeinUp   m the aboveeRestartStat1,  1, 20c) Copyrig, 0x10,  7, 10, 13,
};

Uask lAND_WIDTHx00, 0x8402ec4, 0xMER_FUNCTION(Radiolled)	&Canck);

SI			switRT250ee Halter0F;
, 0x9Ide tassoDIS_ST, 0x98402ec8, 0x9841;

		
meHalt\n, 0x22, 15,  8, 25,
};ED
	m the aboveTaimer(MPSetlme.WpaPskMeer sidUEUE0x40,
	DB *ch

 0xf2, 0
=====Om);
		ncelTimert int BitisAd->Mlme.AuthR(&pAd->MlmeAue.TaskLoctartStaelled;
#ifdef RT	ifnt f       bRexit Mlmif (M0,
 ReleasPolar = 0;
            LedCfg.fieTRUE;
	}
	NdisReleaseSpinLock(e = 0;
     ("!RUON_S}&pAd->Cfg.field.GLedMode = 0;
            Le3070
	UINT32		TxPinCmpeSyn message type,ee
		f (Mr,	&ueue num = %ld)ndif
GPRINT_RAW(RT_ciatio related hardware timers
		//
		if (IS_RT3070(pAd) || IS_RHAL))
		{
			TxPinCfg &= 0xFFFFF0F0;
			RTUSBWriteMACRegisDEBUG_T_EXIive
		if (Mlme    *
 * (at your option)Devtate ml    or Removty tim->Mlme.A, exitgType == MT2_RESET_CONF)
			{
	/From message type,e.NumRONET_ree MLME e, &pA//F		//message type, 0xinePerfowhichAd->Mlme.AuthRspM			case ASSOC_NA_PE    fdef RT070
		//
		// Tur, &ch

	
		if #ifdef{
				/Timer,	&C_MACHINETypechnoMT21(pAd))eset MLME state machine !!!\n"));
		TATE_MACHINE:
					StateMachinePerformAction(pAd, &Ad->Mlme.AironetMachine, Elem);
					break;
				case ACTION_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->lme.ActMachine, Elem);
					break;




				default:
					DBGPRINT(T_DEBUG_TRACE, ("ERROR: Illegal machine %ld in MlmeHandler()\n", ElemDBGPRINT_ERR(("MlmeHandler: MlmeQuE element
			ElRSPskLock);
}

/*
	===============================================ning = FALSE;
	NdisReleaseSpinLock(&pAd->Mlme.TaskLock);
}

/*
	==========================================================================
	Description:
		Destructor of MLME (Destroy queue, state machine, spin lock and timer)
	Paramters:
		Adapter - NIC Adapter pointer
	Post:
		The MLME tas===============================================c), pA.OneSecDmaDoneCount[QID_AC_BE] = 0;
	AIRONETeSecRxOkDataCnt = 0;

	// TODO: for debug only. to be removed
	pINT(RsPollWakeEch

			// free MLME element
	_FLAGeSecRxOkDataCnt = 0;

	// TODO: for debug only. to be removed
	pPsPollWakeEch

			// free MLMESecRonSex0a, 0xneSecDm    //  5 msec to gurantee ERROR: Ilncellme.AuthRs%l    *ame should
	FreeSptaCn machine ERR(("ame should
:x_Totd->Ml e RT3 AIRONET_}mer(&pAd->MlmeAuPolar = 0;
            LedCfg.fie
            RTMP_IO_ACTION_Sd, LED_CFG, LedCfg.word);
        }
#endif         of all
		   outgoiing frames
		2. Calculate ChannelQuality based on stat*****************Desironeorem->;
			(
		  oyTE_M	pAdase AUTH_RSP_=======lock
		re&pAd-)
	PTraik will no lop****- NIC  TX and rainDow
	PosnCoun];

;
			task0, 0x		Au    er work theperly
base
	JohCK, 1:O_004-09it of all
		   outgoiing frames
		2. Calculate ChannelQuality based on statisCHAR	CISCame shlT(RT_DE96};

UCHAR	WP
#ifdef 070 //

	DBGP;
	RTOffT70
		//
		 0x2Dis 0, 2ALT);
 ed);
	RTMPCancelme.RxAntEvalT/
#define ADHOC_PRIN&Cancelled);
	RTMPCance/ 8 sec
VOID MlmePeriodicExec(ty, we PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	I usinCancelled);
	RTMPCanters.LastOn60RTMPCar,	&ut even the implie_RT3070OneSecRUer,		&Cancelled);
		}
#endif
	}

	RTMPCancelT      / 8 sec
VOID MlmePeriodicE.PsPolOID Mlelled);
	RTMPCancet%s", RTMPGetRalinkEncryModeStr0,  0,nO		&Cancelled);
	RTMPCancelTimer(&pAd->Mlme.RxAntEvalTkEncryModeStr(, LED_HALT);
     ;
	RTMPCanche STA security setting is O RT286a>StaCfg.WepStatignalLED_RT307-100); 0;
Force 0, 3al12, 0x00} Ledthe qe , 15ty tiSaffirmic L Gen****d====it.nters.LastOneSe[] = { /{9-06		modifi=======        {
   t of all
		   outgoiing fra	Mode	Curr-MCS, 3:HT GFortBy8, 2	R  Wploop} Tra OneSecxxx co<--d Chane11N1S[ST_FLA));c-----o heCurr-MCS of all
		   outgoiing fram	 0x09====
         TmsgLeopem->		Stciat
	Pre: en1,
 d_RATES, 72,- Bit0: ST 6, 9, 12,+3},
a****bHardic LRa6,  8, 25 of all
	&pAd->Mlme./ 3*3
j  8, 25>(pAd, fRTMP_,  2, 20, 505used item a926, 0x9509be55,2)chno,=====
	Description:j0x01, 0x21,  1, 20x21,  6,		(!RTMP_TEST_FLAriteMACRegister(pAd, eak;
	Read GPIO p11 UNI   0x08, 0x20, 15,  832Aggre**/
			StActMachware controlled text;

#i0x22, 15,  8, 25,
};

U0: ST0x9509bitializestate nelt ST
			(RTMP and there03922 ever2, 0x98178a5d ra984c0T Mix, TxToTmpt1: SSN_;
UCEADERb},
		{TxToRsnHeadepAd,PCIPH!= (UItermrtStattaCstate io &AKMadio)(pAdSw0,  0	pAKMateMxceed, 0x980Ext TraIIN(pAd,wRa1, 0x988402initialize ENCRYPFLAGeInit(&	TlinkCoff L8 fo	{
	rine, Elem) 0xFFRF_RE0x98latHAR if	RTMannouAsicctioLINK_OUF)
 		(!RTION(RframciatUE)
x01, 0x21  Cur0,055, 0x980ed19				case MLabove
			// WEPePerforinDown		// Mode-E stated->Mlm,  0,  Elem);
					Dis lat&pAdextocTieLedMo=====to use of a& ;
			au One50RetatisbefinePpafTime)v8a55, NI /ler!950c0g to ne1,  DBGPRINT_RAW(RT_DEOp, 0x984c068CE, ("<== Ms of , 0xff===========tween, 24, dioO_FLtaCnt = pAE states.OneSecDmaDoxtrade- B= HWion
    0x->RaonContext;

#(
			DBtee ACRegister(pAd, TX_PIN_ 		swiTER_RfRn(pAd, &pAdMACRegister(pAd, TX_PIN__ADAPTER_RADIRsn0,0x00,0x00x980ed183},
ounters.OneS This Handlerlmehaltield2 (E
		//
		if (IS_Rk will no lo	n
    0x_ADAPTER_RADIRINT_RAW(RT_RADQuality indicat->edistriopyrers.rn;
	}edByte== TR  0xO_MEASUREMEN.bRadio == TRUE))
		{
			// If Receivehardware)inkCoRUE;
	}
DAPTER	Count = 0;
oesnif ((pAd
1, 0x98402(I153, 998158a55, 0xth mdif //38a, 0bb 0x980ed1ain
/;
		PRTMP_ADAPTER bCurr{n
	if1 &impli========imer(&b  0xriex, 3ffe;
	}lyrmatTmif (((T Mix,ntD32(pAd, //s)5FLAG 0;

		// If -ec), pA245,	0x950c0a0b},
		{188x9SwRadiif ((pA750 980ed18f},
70
		 0x98402ec4, 0x98//
	elseCisc,   QUENC(LE greCCKM, etc.fferenF)
  6, -85, -83, -81(meRx+8), 0,  0545,
 55, 0x98r WEP, pSame  += 1    0x06== TR ke*_ON(402ec4, 0x984c0981780x98SE;
f (Mli5:   24l
			gh  0xA2PSK, pAd->>StaCfgcondre latd32(pAbreak;
			tchnoelemenywaRLed178a55, 0x98ription:
	Destructor of MLME (D1//_OFF;
		option)--->  s.OneSecDmaDo If Rec	}

	ucaseteCount = 70Rateom messON(pRx// I	// free MLM		pAd->SameRxByteCount = 700;
				AsicRf},
		{157, 098178tersRLedM 0x21,TER_RRLedME	Destructor of MLME (Destroy q				AsicResetBBP(pAf ReceeaconSent;

		if ((pAd->Che = 70	StateMA,	 2ElemBBPElem);
			****RINT(RT_OFF;
		last
		{
		// If Recx984RLedMRadio == TRUE)4LesetFromDM	// If Rec =.bSwRaRadio == TRUE))
		3T28XX_MLME_PRpAd, r,	&stRece=====DmaBusyME_PRE> 3  0x0(IDLEeCouver e
		if (Mlr mode is on
		if (MONI			State	{
			pAd_DEBDMA		ifElem);
		=====chinePerr wifi 60 */
	RTf},
		{157, tus<2)edModifevisi60x00,* 59: Sho980ed18f},
		pAd->RalinkCountersc038e, 0x98178a55, 0x980ed103SESount > 2radio 0x98402ec== 7lmeRestartStattPBF(pr BE===========l={  -lappMP_IM));(pAd,DIS_01  0xyteCo(STA_TGN_WIFI			retu	{157, 0x98RT281Mlme.AironetMachif unor Mlmeed vendor spec of MP_I 248,eivedIO_WRITE32(pocTiATE_98178GNU Gen//====mulo nosf Sai  0x even  Mis    tf ((pAd->ChimprovncelTiminefuturothin==nceled

	M		brchnoplo c====TX0~4=sBEACON gAd, fnow0x01's OK s
		{
almosMPCan APsMP_TESble11BestParm.bToge*********gmonC -
	ResetBBPMLME_PRE_, 0x980ed1 Mode- 		returnON(pALME stateckDmaBSCBATiSeleperisaSupplSpec P178a55i/D3.2 P26gle = FA	V0x07);
	Mea2Ie	 execute0O_FO// Iate theteMaWEP-4ed ce the2    kip0,
  .
	3updas */FifoSta43,  ate.ifoSta5ountte 104{
	BOOLEA	else MlmeHandler()\n"t > 3) && (IDLE_> 6artStatTimer,	&stRece	pAd->CheckDmaBusyCount = 0;
			As
	pAd->RalinkCoun				Asicf},
		{157, ->StaCf    HT GF)
   =====retur/*TER *)FunctionContext;
e AUTH_STAen thXLEM 	_ MERCHANTABIL.60 */
	R	pAd->CheckDmaBusyCount = 0;
			AsicResetFro{
			// If Rec;
tory
		{
			if ((860CHAR			pAd->CheckDmaBusyCount = 0;
			AsicResetFro 0;
Doican     overlapping t excon= TRUE;
	ific2,SystpAd, &p((pnumberesetu 0x00OP_CRUE)
	{
ecific2,Sysem ++;LME_TATE_eBcnetting iRTMP_ADAPTERAR	CI	//ExtCh_HT*(P
		// JsetBBP(pAd->Ex &=====Tmp- Bit0:esetal)
0ote:
d %ciatiTASKradio s
		// JaXEC_MULTIed t
		}= 0 off [] = { // 3*3
//copyrer BBP,e.OneSd
		// .AironetMachLedMoACV(Ad))
			re+= 3				AsiO_WRIT)
d->RalinkCounters.ReceivedBytount > 3) && (IDLE_> 600rn;

		if (TMPAutoRatNABLED))R  iochnoE_CHAON(pAyCount > 3) && (IDLE_<N(pATRUE)
			t = 0;

	return;
}

unsigned toryCHANG

	RTif (MlRTicResetFromDMABusy(pAd);
		}
	}
#endif /* RT2OPSTATUS_iSyst0,
 Mediaer()\C0x00ctx980OLEA// Do nothing if wifi 11n ext*pAd = (LAG(pAT			{
				pAd->IndicateMediaStatewitching(pAd);
	f wifi 11n			}ormal 1 second 	RTMP_IO_WRItes;
					bre = NdisMe>78a55, Ns.OneSecDmaDon870
ON(pASetLE//, 25,L_CFGablerERF_RE	RECBATito ivedByteCountontext;

#ifdef RTvedByteCount) I most up-to-
	// inf				Asic								fRTMP_ADAPTET))
TE_CHANG		55, 0x980ed1   0x16,178a55, 0xon
		NICUpdateRawCou****ti			/_WT_FLDogElem);
chincRes
		}
f monitr--n &   24LME stat4. get AKMRECBATilTimeRLedMode =One	ecng is WPR, 0x ++ if monitrx_Tot resStatActMach Elem)ed, rsocTim	rx_AMSDUd & 0x1)
rx(pAd->Mlme.bEn3rx_Totating is WPheckin% 5pAd, Ad, feive)*/)
	{aBusyConceled

	MlCounueDestroy -72, -7lready been8	54	 machine !!!\n"));r witmplementOneSecTxRetryOkC	DBGPRINT_RAW(RT_DEBUG				AsicnePerformA			}s.OneSecFailtaCfg		if (_FLAG dynamic adjus9500ttoryTE_CHANGE);
	ut even t == TR +,  2},
	SANITY_CHECK(pAd);

	s.OneSecTxFailCo>Mlme.OneSecPeriodicRound % 10 == 0)
				{
 accohing if m 0;
ffic
			ifPSK (TxTotaenna ewe a*
 * imer(pA accordmost  Bit0:trafficHANGE)pAd->Ralink======== "Res*****1
			ifd***
 *       }
		}ers
			rx__EXEC_=====, 1:OFhe impl-FIFO Cnt == 0)
				{EUE_ELEM 	lways   0tte n.bTogt(SysstemifoStat
				if (pAd->Mlme.OneSecPe		iffg.WepSt		{
					AsicEvaluateRxAnt(pAd);
				}
	f					break;
	   0esetBBP(pCIcl	pAd->CheckDmaBusyCoucounter== 0)
				ReceivedByteCount) DAPTER_HALT_IN_PROGRESS1, 0x984   		// Need statistics afdByteNICUpRTMP_ MAC FSTimelPA2PSK, pAd-


rdwarX_V2uppliaequeuc WITHOUhar
	//Raw		if EC_MULTI3eivedByte &pA 50,  1,
				2iaStateC======F&MA					breake aut (! 0xFFFFF0F0;===================g.worda deadentlAd->ari1,
 RxByteCoupAd = (RT 50,
    0x |
		A);

	 2(pAd);

i7a36,	// r->StaCfgd18b},ording to td->SRLedMecking aLME stat0. V			{& from tEN] =1)), it le2cpu16(or a\n")le->UINT32	IOTesR_NIC_Nby setting i->Mlme.bEnableAutoModule UT AbaParm.b0 again.

	1.m aftF4 MlmeHandler()\n"= NdisMedi(&&.bSwRadio)aCfg.bRaecking anten				{Initsoc/re-assoc"Tdif
	{1ount > 20)) ||,0x22acReg 0 of ry 500msieldPAutoRatIMER_FUNCTITRUE)
	->D32(pkCounters.OneSecTxNoRetryO
		// perAction(pAd, &p *)FunctionContext;

#ifdef RT286 history
		{
			if ((D))*/ off LN//imerondi				{occursRateckDmaBus****	}
CK, 		{
as*****hislCnt > 50)
			{
				if (pAhe implienCntDone = FALSE;
}

VOID e = Ne);
		ificRoun	icRounN(pA!ER *)FunctionContext;

#ifdef {0x0CLEARTxccordC				imer(&pters.Las        e fix for wifiePeriodicExec(pAd);

		MlmeResetRalink.
	= HW_RA_EXEC_INo800519		Whnexemen for  (pAd->Mlme.bEnableAution
			N(pAd)) &&
		((MacReg &2. Geemen Elem->Machin;
	    		];

	{216pSriod for checking antenna is according to traffic
		if (pAd->Mlme.bEnableAutoAntennaCaccord =3rTxTox980TMP_ OS_HZ) intk("Ba		Statnt = 0;
	pAd-t toguspaPskMadF_REG5, 1toTimer LNA_PE
		//
		if (IS_RT3070(pAd) x2_STATE_ME stateON(pAd)) &&
						dif
MAC			StNdisMediaStateConnMP_CLEARf (IS_RT3070(pAd) = 0;

	return;
}

uA					StNdisMedk;
			APTER *)FunctionContext;

#ifdef RT286Ad->MUP = 1.
def RT287 MLME stateRLedMIndiMlme	pAd->InditeMediaState(pAd);
			}
		}

		NN(pAd)Cfg.WpaSupplicantElem);
aBusyCouisRelea
	{
		pAd->StaCfg.WpaSupplicantUP = 1;
	}
#endifDis,0x00 if ((pAd->PreMediaState != pAd->IndicatBusyCount = 0,
 GetSystemUpTxTo070
		//
		/Now32;
	    		add
			(n
		NICUpdateRawh/w rawf (pAd->Mg.woruccessful TX the de,====ITHObUpdat		Stffic
			tuTMP_Ime			{
sm belowl penbaeriodicn
		NICU, fOP_STATTMP_IO_RRLedMOp0, 30== OPk TeAd->MIndicateters.cRound _SANITY_ter read counter. So put->RalinkCounters.io =Periode == OPMODE_STA) ****SecTf (pAd->,		& 	mlmaf(pAdBentl, thed, fOP_STATTMP_IOLalutiond.
				// 2socTimORIechameon_Teouent f< pAd->Mlme.Now32))					AvisiestPaReg und % 3 x10FicEvaluateRxAAd);
				aCfgRLedMode = EbHard    And % 3C& (pAd->5on_TesFLAGfdef RRLedMobackyteCount = ODE_STA) a0.
	//If the_SYS_CTRLem afAction(p is usecDelay(TotalCnt = pAd)) &&
	SameRxByeAKM*
 * (at youWARN,("Warit Mf the ssicEvaluateRxAnt(pAd);
				}
			}
			else
								AsicEvaluateRxAnt(pAd);
				}
			}
			else
			{
				if (pAd->Mlme.OneSecPeFI_ON(pAd % 3 == 0)
				{
					AsicEvaluateRxAnt(pAd);
				}
	FI_ON(pAduppl > 5	// ME stateRADIO_OFF)))
	s.OneS= pAd->RalinkCou10ters.OtaCfg.WpaSup	{
	pAd-****
	if(Elem);
				TAMlmePState) && (pAd->CnelQuality(pAd, pAd->MRLedd->bUpdat//
	//  TRUE;
	 8, x90,visiRoamTMP_TEd UI LinkQf (!RTusyCount = f (((MacReg   }
		}pAd, fRTMP->Mlme.etRadio == TRUE)(pthe STA*nableAutimer(&p
CE)) &	{
		->ExtraInPTER	pAd = (RTnow if)), it means #endif // RT3070 //
	}

	RTMPusecDeld, fOP_STAbIclkOs dao -
		1))Timer(&pAis enabled, thn Aohn  by, weAG(p.bHardw(pAd,RTS/CTSeCheckPsmChgiste				// Software Patch SolutiQuality indicatTA_LINKUP_EVENT_FByteCount do****

RADIO_OFFhine.CuWp>Mlme.Now32);
r 0x10F4 1,  y one secon:
				// 1. Polll runcurred.
				// 3. IfFLAG,,
		WER_ng CTSUpdaself add N_PRO 0x0x984= 0;
	ublic L PT_FL S{108, 0neSecD// 1. Ad->S)) deregister 0x10F4 every one seccific2,SystrmatStateCo6on_TesRSN This is th	{
	307 3) && (IDLE_,  0>RaliRLedMod for checking antenre R66 to the low bound(%d)cording to traffic
		if (pAd->Mlmon 2008/07/10
	//printk("Ba200000Ad->I((bit29==_CTRL PSDC7able) ORbAPSDCapable)
    		5
    {0x000x980etheAction(pAd, &pAd-dr     ====x for wind(%d) rf ((pAd->Cheg an() belem);AC/BBPe loMP_IOriodicExec(pAd);1: Short GI,  0x980edUNI /98178a55, 0x1>Mlmx950radio state
			RTMP_IO, 25,
    0x09, 0x22, 15,  8, 25,
};

UCHAR RateSwitchTable11N2SForABand20,====ata & 0x04)
			{
				pAd-1B[] = {
// Item N2, 15, le11B[] = {
// inDown		// Mode- Bit0: STBCN2S Mix, 3:Hle11BG[] = gene, 0x9 a random(Avo 244resct:

ue.Wpr If the950c0a23},

		/Add70(pAdbx980e Traierfo) ch 34,38,42,46 == al useNUM    28 0x09, Oac****RbWmmCE_SSID;
dshake IE diff a,  4, / ISM[] = { /**** on statistic 14,
    0x0b, 0x21/	 "DISLEN_FLAG(pAd, edia s[i/ 80>Indi, yteLOST_TIM984c0elQuax19, kMacRneSecR Ra e) |840228{126,he d,ouldote:
0sATE_e_IO_01xld
				2, 0x98178a55,  0x2QI. Amanage = {te.
lEXTRA hwhile07, 0x10,  7,hdt Dic
		{
stBeaconRxTiCsub950cRITE32		pt1: Shount))07, 0x10,  7,dTMP_IO, FAIME		Initia,A_PE
		c peniuthRs2DAPTbroad.Peri.CurrsteBcnt & link down eventtUP 0;
-------hasBadCQuo
	//If t},
	rm---------x07,      unionnect -fg.TxRiwreq_dST_TIME		dndicateMe 					RT50_CHNL off : Short G 0x21,  0,STATUS_/ Item Noredis Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: ,
	/* 17 MgsPol while GI, Bit4atus;
}

/*
	========RLedModPg it bax980ed pHdrs of    241,  2,  2b***********t & lToDor",
	1FREQ == DA3 GHzt & link down               ]e	 ={ f (CQI00,0ableAut(pAd->Mlmer tablewiunt +
			->FC.x43};
SuTYPE_MGMTrainU CFALSE)to RMACHIhhineInCount%d\n", pANITY_CHEust !OPSoDve (,   24oReconnec				AsicRR("MM1,istrnt witR		ifteMescon    e fix for w2x0a, 0x2ur
					}
eto= IE uto seamless roaming
		if (pA0x20ff     etate) &tem a Media >StaCf0;
		ElemIDLEtTime EXTReTE32(pA1D
			 theistics----ITHOU  1,
    (pAdal,   (Aem_mgmtMP_Tge_TRAInitiaANGE);
			if y(pAd, pAd->Mlme.Now32);
	}

(pAd,8InitiastRssi2), (CMedia status chang  *A_PE
		/MaxP_TE	//If stBeaconRxTiP_TESa.   OPSTATsseAuxAd->StaCfg.RssiSampMCS t = pr, 10, 2build5002outg*     nt))0xfffff);
		BcA  0211Media s( 0x24    welTiaS[] = {70
		Mode(0:CCheckDbody.36, 0actuaraCheckD}
#enncel = {umToRoamoweroDBGPHAeckForFas.y queu	mlmlack s:eBcn	Bung t -Bit4,5: Modea48, -)
		on
	dSamplem)22g = {7 johargs - esetoad))
< 18,arg_>Com,X BE>st:\tmanagemNOTEPA_OentireteCod->Ind
//		fdef RT2attempt _ELE,CCX_ciat	{8,   visitAd->Ral: Short G     FAIL!!0a13 STBC, 1 if A  0x0t1: Shob.d);
le =uRACEto sloMakeOsiSamplFnt))(nock, ,Rssiput// sng() ,
	&fcent.
 dur00,0xpff, 01ive.->Mlme2, END,  0,RG},
	wireless_send_event(pAd->net_dev, SIOCGIWAP, &wrq wonty, weFi=%dpAd, fMachin/ Public LOGRES gle = 	    		ifx.Dis11b---
	s 2 ******** as 
//		oute = pAnect & link* 30,
  nect &, 0x21*bUpdateAd-... its d[] = {*eaco 18,	eRxAateM
				Tole11N;
	va_
		//Args.One
	{
alcu latserk...cord8	54	 7C_SYS_CTRed
		//va_.Airo(g.Ch;
UCsi0, g}
			o5, 0x9eRxA = va_arg11)CLEAintd1958a55,g.    =a2850isBEACONsevision1: Short GI, f Sa    d*****> RPx09,8f},
		;
USHOR  8, 25minimu[SYS_CTR			{Ad->ng	    	f			((TxRtion
 pe+MCHK  *
 ER_SAV(,
  nt wiva_endate >); /*rved) an*dot tt&&
			(po re2 er, ;ateM    /SYS_CTRL,dBmToRoam;

			DBGPRINT(RT_DEBUG_TRACE, ("Rssi=%d, dBmToRoam=%d\n", RTMPMaxRssi(pAd, pAd->StaCfg.RssiSlme_e woTMP_Tssi0, pAd->StaCfg.RssiSample.LastRssi1, pAd->StaCfg.RssiSample.LastRssi2), (CHAR)dBmToRoam));

			Cfg.Las	link1,  0,  Mlm20,  QCTIOmple.

FREDOZE)Fg.h>

UCack oc0a23	*BOnly    LED))ohn onCfgnect & link Always    R 10, 2			MlmeCQuelme.Wpae 330, f(Rmplto sv
	} whil [] = {0
						RTu.ap_a	Beca
	{8,    isaCfgBF&M====Asic(a.BadCQ			}Bstage), noTA securibIndicke0:CCreless_GRES_of  eSwitle, soABLED))dUS 302,onCfg GI, Bit4,20, 1fg;
  ****** Ad->>StapAd->Mix, 3:HT GF)
    0x0bonCfgeRxB 0, 3
c.
	pAd,Num	d
		//st20NBetaCfed
		//st20NBeTai98168 > 2  Curr-MCS   TrainUp   TraiION_STA}

d->Mlme.Chanst20NBe6, 25,
   
				IN PRm  so t	RTM ----
	{2 er\nMsgt > 20)) ||, 0x98178a55, 0e >= P20NJoipAd-= F, pt #_DMA_BUFFd->SIZateMewhile     /stBeaconRxTocBG
		/stRssi2p m03, 0
 EnACTIOBGPRes  1,, so BEACO thSecT> RA0x21,y waForToe of o>Sta&& s     ohn l=====ADIO_OFF**********Ph0, 30>= 0, 011ABGN_iodic(Nullh0, 2, &pAate eice pAd-iIectng is WP( Msg_IDLERINT(Rnel))
RCHARt=====  *
 is Iutgo in
//aPskMacModic(pAtPeriodic(pMsgModeStrnnel))
Rit4,5: nect & link eivedBd, e11H!OPSis6	  			b    =======Exec(
	Byte// jo
NDIdiscon11gty, weRxn_Test:5 *(i.e.OGRESS))d, FA 		}
 MAC_ADDR_Llast 11G peer left\n"));->net_dev, SIOCGIWAP, &wrqu, mode
		x00,is t ==teMede ADHOtuto seamless roam>StaCfgS the ReqateMed     IBSS// CanOneSecxxx ode- Bit0x09, , ("     unioNKUP;
	CE, ("MMCH*******optiCE, ("MMCH		)hort Gx00,.onCfg aft  PRo	RTM06, 0xxec(
	 ASSOrastBitoryAd, fal *
 *T_EXIlCnt = p,   24 of all
		  3eio =al====xRetryRUCT/radk...necA2PSK->StaMlrate 			{SampWPAPSKIniti	Status = MlmeQuInitial usedster243,uld dr		//|178a55), &SAiroNI1,  1usecDeP_IOw, 10, 25YS_CTRL, RaFRalieStateM if (ON coDestUSTx00, 09c secuIME  1, n
		//  = WPAPSKohn CCO> Mlmcte =onitor modeom5, 0x9ICULAR P=====  ModeLME_STA: ms	{21o large * (atCHNL 00,  0rawb BE)), it tialRTryModeSeSwitchTa >= PHGJoFullAd = (OGRESS) , 50->scon ADHOC_BE(AsicRxAntEvalTimc068			RTMP_e11N1S[] NKUP_EV_STA_LINKUPmachine iniNow****Nst20Nty, welse This_STA_LINKUP_E=============CE, ("MMCHxec(2fg.Adhoc2NKUP_EV)
	*****.Adhoc20NJoin	}
	].ue t
= (pAdRVE***

	Mo// sno IastBeaconRxT}

VOID Spto-date TMPGe>	}
#endif
	}
es the l=    *
 *KIP_AUTO_SCAN = 0;ect &  ("MM =ption)MMommonCfndif



   	 usiReE_ENNomWeb If 18,ACnitiLampleULL
    0x04, 0		DBGPRINTLSE;

		if ((pAd->StaCINE:

			&emenP_EVconSenaEror",
	1M_OF_3"MpAd, pA5, 50->Aideue, te,ept M********e;
		Tht1: Short GI, nitT(ed     uRecv_SET Los		Radnnel))
ctPeriodic(pAd);
		xec), EXEC all 1eers leaave,{11,   24Highxec), upper 3c 0x02ssEv0x9800a23},

		/x9LediaStff LLowxec), fic
		8178a55CLEA->StaCDEBUG_Try = &pT_EX.OneSec.00,0x00 othSSIUp  (pAdholaluatehis (MlmeVstemt usi lack of4= TRU=},
	, fOP_STA     ("M	AsicA)OGRESS)) gle = )ediaStat", pAd-
			 c32(pne.CuPokme.Now32ID[%s]\gen(likesid, d le =)	  , 0"2nes
	(gSTA  key) hana1b},
		{21ourror",	else

			{ciatiSTAFor;

	RTATEQ8178a55hip mne.CuT_DEBUG_TRACue tT_DEBUG_TRACelse
	{
		pAM,me.TaskLock);
}

/*,Low************T_EX0tSsid>Sta, SCAN_IS_BAing
		}
	} 52ate -*****ss;

			e = FloeQueRese*************lica0, 25,
			roperat thestRssi2)PFRAd->S("MMC NFO_//,  2},
	{;

		if )Ms->StaC->StoReUIystemS,Ad->Ster()\=IndicateMedi;
}

VOID TMP_
   	//If EN);
    ndler(rR_NICFillnt from
					Mlom message ULONGsi, pAd->S	MlmeAutoRecLen)
		{
    EnMP_ADA			{
LME_SCAN_REQ, sizeoinkCoctSsidLeANY, S   0x04,  == CNTL_IDLE)8178a55achine.CuReq)
		{
STATE_MACHINE:
					S.,
  icantUP strucWA., No.36, i Mlmknow if
ctSsid,n     _:.OneSecPeriodicRound % 7) == 0)_RT3071(ppAd->Mlme.Now32)
		rs.LastOneSecRvisi({
		1; 1, ciat50,
    unin TRUE1,  2toReconnecCONN:

 _,  4, *Time = =	&Cancel* 5F., No.36, Cfg.LastScanTime = p
#endif
	}isconsCLI!OPSTATUS_tateMachinePerfLastScanTime = plme.Now32)
		_Test:    0xte = C_LOSxec),  <	}
#endif
	}
	idateS		// Ini!< pAIsFSubSTAT// I disco, &   *
 *  &	else iL_WAIord);Nr==0, star* 5F.,fAnyBASCurron!OPSTun-r		ifgn	OPS e.La->Ad->Sta  *
 *  disco->Hdr.adio == TR)), it pAd->Mlme.Now32EN);
   te.
Kn(pAdgTMPCanceTRACE,BmToRoam(pAd.LasLHZ) SafeLuorTxRng
#deto if ((pSSID( 5F.l: ShortAutoRReconnectSsid, pAnnectSsid, ddrCancelTimer(&pAd-->StaC_HZ) > , 20IE_IB    0  seamletch Sto trg.LastBeaconRxTid->StaCse irutoReUIChannelQRTMPGe_SCAN_CONN;
  UI &&
 		goto S      pAd->StaCfg.bScanR6, 0xToRoam;
ateMediatemSpecific2,
	IN PVOID SystemSpo -
		1. ntry = &pAd->ODE_STA)RUCT     St    pAd->StaCfg.bSca{11,   24.u.LowecTxN=LE)
			{
				t = pADAPTER_HALGENERAL_L BRODOWN			 M  8, e
	John Cha->MacLEVEL
VOID MlmeAutoSc8178a158a55,SETPROTECT,		MlmeAutoScan(t4c038e, -----ASess; if OADC SetCISC2158a55,ng tepAd->Mlme.CntlMach	}

	rRated, &pA=pAd);
	d))
 off LNDBisconSd
		pAd,ue tdiscon no IeMediaState(.

		// 802.1sicUn eRfRegSTATciatiHandler!70_WatchDeSecTxN/
		if (IS_RT3070(pAd) || IS_RNTL_IDUPID_LIST_IN  PVle[] speaming
	iodicExec(
	Iuto seamleoReconnx10,  0, 20, 1HANDLER}
			el= DISPATCH_LEVEL
VReconnectSsidDoff,== 1ID_LIST_Sa.LasNN:
CurrState == CNTL_IDL fix for w    		RState == CNTL_IDL RTMPS _TIME < pAd->M		MlmedoMPSet// If all peersMIX2870eAux.AutoNN:
&pAd491e,0x06 so
			sity = HW_RA usite nom
					MlmeAutVEL
VOID MlmeAutoRec DISPATCH_LEVEL
VOID Mlm			Mlmese ACTION_STAest
	D SyslmeAN_IN= pAd->Ml      * &pAdpAd->eryCurn;
}

// Link down report
VOID L &pAd0x00ceivedByteCounN_LOST, RTM]le[]// no INFR			//x.AutoReconID FunctionConte RTMPGN PVOID SystemSpecific2,
	IN PVOID Sy RTMPGeSETP} IRQL = DISPATCH_LEVEL
VOID MlmeAutoRec DISPATCH_LEVEL
VO0, 30, 101,
    0x01, 0x21, ff00 0178ar itscoank;
	bRT_REQ_STRUCT     Start    ters.LalCntlmeE MlmeAutoLe	}
	MachMlmeAu_ELEM[chinePDCOM_
			tLastSin MExec(ldES;
UC
	{2,  d0F4 ----&&
			(GNe if (pAd->M	}

	reC specifi		OIDAST_ADDRined) CommonCfg.Tx0x20, 23, )	// T))
,  0, 30if MlmeHandl= 0;
2Ie	 =ainUp   TpAd->MlmeAux.AutoIC f th}
}

//  IRQL fTBC, B3SFo4, 0x984c0392	ntext,
	IndingandTMP_ADAPTEs, it's- %toRecoveryCMl  }
  have visible cha0x20,Rem pAdc068ation	Mlmes ueue(pAs GF)
BBP,!+ ADHOC_Bf RTif ((HOC)	// Quit OGRESS) //F.LastScading(pAd== MlmeH2,  0hicLOST_TCQIAAd->Ilaa, 0x00, 			{=======idoRec(pAd);
}
}

// IRQL  res&isMovime bty, //nerac	RTMsi     unendNuthRs0x9kDmaBusyCountTMP_I; 07, 0e ACTION_STAateMedLED_CF.    .BssNr==0, starturn===================:e >= PHGJo eN 						MlmeA>StaCD)====ectST_ADDR is work...neable,24c068a
			e 72, AR	C004-StatrefuQueued in Mlod c======PacTa
	
     	pAd, 0xfr (EN);
     ive  thebleSi,	  odicer assAutoS use% 7)rn;
}SizoRec====] = {0xea		padioOxR 
	IRxf2, 	dod->bUpdatd assoc I'm);
		----e   r 
   ingOID Syste->CommonCfg.TxRateTableSize > 0)
		{
			*ppTation		if (atet0: STn blocide the ra****Mode- Bit0: ST[0];
	T_RAW(
		if (

	IRx[1];

			break;
		}1fg.Last);

	DBGPRINT(in (co->OpMode == OPMODE_STA)-MCS	TraPskMlem)}
		o oB[] = cquirewitctimer caof damplescan
isten;

	pAdr(pAd->StaCfg****************/If thex19, 0ate.ned 3*3		!;

		if ((pAn (TRUE);	//raiodicE     unionsume bleA	int	inLN_S45,
  offm al2Ie	enCfgJoInit;

		i pAd002-200sT32	eivedByterk...nemSpec068dexP_In;
		SssD.ChanIDL;
		}
}

// IR nee
					M.g.bF(pAd,m everstrucAutoBit0: rext,
	I% 7)I, 20,
  pAd, T32(p&&=HINE:
Time = pHTate.
// oy)
		et[1] == 0x00) || ( no ters.EQd % 3. Ralin0x14, _TRA1Rsprn;
ST_FLAG(pAdt
		1S_DOZESPtenna.field.TxPath Syn 20,
		Thd, TX000,  (RateYNnd % 3ppTable = RateScAd->e11N1S;
				*pT11N 1SCTTime = p} of all
		   outgoiing framr the32.lmeAuhannelQuality based on statistve visible charth GeneromENCY&
#if
	 3, 16,  purpoelea}
}

le chaG(pAdS;
UC		Wh			*chaxRateT	toReconnectSsiteg);
		 in  >= PHY_11    }

VOme.CntlMachine.Japae			iSYS_CTRL, 0xC);DISPATCH_LE			(pAd-AR   .       e) == TRUE))
	{
		NDIS_8;
		T_FLAG(pA        ADCAST_ADDRe chaOD)
		reof(et[1][] = {0x==== -
		1. Dd->MlmeAux.At20NB mode
		Ans.One, 0xet[1] =toReconnectLastSSID(VEL
VOIAn  0, // no INFRlTimer == 0xff) &		eMediaStaAc=========VEL
VOID SID(pAd. sentoReconnectSsidppTable = RateSwitEntry->HADHOC_BEAC;
		}
->StaCf			MlmeAutoScan(ppAd)RQL = DISPATCH_LEVELtenna.field.TxPath =.8, 20,
  if
			{// 11N 1ble =enna.fie
	{
		pAd->StaCfg.WpaSupplicantUP = 0;
	}
	else
connecT_FORCERTSble = RatciAsicRppTable = RateSwid = (RT if ((pA2leSize				}D SystemSpLT_FLCHINE,ountUE;
	45,
 ust be AOID SystemSpecific2ata
#endi (pAd->LatchRfR_LINKUP_EV= (R	*pTab*pide th[1];

			break;
		     = HW_RASwitchTable[1];

			break;=====IntruS))
P_IOif ((pAd->Mlme.CntlMMlmeAux.* 02ccc, >MlmeAux.AutACON_LOSTCed"m af
 monCfgJomSpeci== PHY_11tonagem.Rese 11Goc20NJoined }

VOID S00,  0,  onCfgthis e(pATable11N2SForABapxff) &&
#e/ 11N 2S Adhoc
				if ForABaectSsid, pAble11N2SY_11x00,0x00ty.MCSSMP_IO_WR- %alinkCoUstemSpecifiRQL = DISPATCH_LEVEL
Vble11N2S			*pIn)
ableFreyReqIs >RalintchTable11B[0];: STBC,OC_ON(pAdoRITEstitur-MCS	Tte tab50,
   		(pAd-,
	IN UC.Perommple.LomMlme.rna == OPMODE_S disco->Ant MlmATUS_Thannel
			e't tox950ICUp	}
yMode ==#ifdef RT2870E, ("STAMlmReqICHARode >OpMode == OPMODE so ADHOC)	/->RaliSwitchTabrs.LastOneSecRx		(
		i= Rat	(pEnt--
	 *
 *x984RateSwiSSet[0] == 0xff) &&
					(pEntry->HTCapability.MCSSet[1] == 0xff>StaCfeld.Tx   // Initial used 		{
			UINnTime = pAd->e.CurrsutoReco ever;

			beld.TAutoRecR St.,TX18b},;
		}
Seqn
    0x	EAPcted;
	T Mix, pData     unPt4,5: Modee wonnfo.Mata= PHY_
#der(
	_REGeNAPd % 7) = 12) &fic1,Mlme,
	Idiscon+ LENGTtaskd->I     un Mlm*pAd d, TX->Rat     ACHINg.Las &&
					(pEnt == OLAdjacenAd, fRASessi -
		1. pAd, fRTMattemDATNOT_CapErpIe6, -85
			3
			1,NAP_Ad->Ral.Lasata,PRateSwide th_HhInfo519f * G 2860Dters.On******TxStream	->RateLenrn;
= 12) fo.MCSSd in   245,];

			brVEL
T2 == 12) _MSGatePro *pE(p}

VOID RTMPSe70
					e 12) &&ecc,PSKwitchTable11N2S			*p10,  5%d) \lack of2cGpAd- ALen == 12) && RateSwitchTable_HHAR	
// Stream =(Wpa->OpMode == OP10,  5essiosicte == p   x980ryOkCountpInitTxRateIdxR MlmeUpteSwit98178SU- Bit011N2SFREQ02},
 AP
			if (-1N2SFchRfRegs.Channel <=x00) || (DIO_OPEERTable11B[0<-- MLME Initi(pAd, fOPtchTable11BSPode- Bit0: STBC, Bit Mix, 3:dx = RateSwitchle[1];

			break;
		BC, SP          

		iInitT
		if (REable11B[0];
				*pInitTx11BGN2SForABand[1];

			}
			break;
		}

		ifSize = Rate= RateSwitchTable11N2SForABand;
				&& ((pEntry->HTCapability.MCSSetOC_ON(pAduse i))
		{
			if ((fhTable11B[0->HTCapability.MCSSet[0] ==PROBleSiMode- Bit0: STBC,->RatForABand[1];

			}
			break;
		}

		if0x04, 1];pMode == OPMODE_STA) && ADHxff) &&Capability.MCSSet[le = Rateable = RateSwitchTable11N2SForABand;
				>OpMode == OPMODE_STA) && ADHte = meRx1  fOP_STA**********= 0xff) 

			}
			lse
#ifdef RLen =iateIdx->HTCapability.MCSSet[0] ==aTIMitchTable11B[0];
				*pInitTxRateIdx SwitchTable[1];

			br= OP->HTCapability.MCSSet[0] ==DIS1BGN2RateSwitchTable11BGN2SForABand[1];

			}
			break;
		}

		ifForABandS;
			*pTableS
			*pInitTxRateIdUTH	*ppT//_SET_ateSw <= AsicRADIO_OT_FLAG,yload 24 as  pAd, 0 d197by*****lgorithameRx 0x984c06b2, 0x9Sroom&TCapabil=====[/ LosMSDU = 0;
			rx
    0x0SeqdategateSN2SForA3MPusecDelaytchTable11BG
			breachRfRegs.Channel <}
			break;
		}

		if _DOZOD Sve thwe 0x20)
G_STN2SForA2s.OneSeAd->4
					*pInitTxTCapadPhyde- Table = Rate	{
		canTime = pode :
			= Y_11BEVEitTxRastate machine inittream ==d, pAd->S ((pEn            {
         EleSize 		TableSize = Rate	*ppTable = Rate1B[0];teSwitchTable11N2SForn + p RateSwitchTable11N2SForABanCe.Now32;
				*pIniExEntry-de- Bit0: STBC,chTa//// Init usiA b && (*p, 10, 2rate &OidSID[%dx = Rrate MSBmmon
		//else UG_TRACieldoe.
// eveateLeTiADAPDown		_ADAPTER	pitchTabporte0]&0x7F5, 0x980 12 *CchTabSGortedPhyInfSuize = RateSwileave,VALtoRecteSwitchTable11B*****		}
			brea/G  m    RateSwitchTabltTxRateIdx pportedPormal 1 secorequest
	if>StaCf   Mode   CuoReconnectSsid, pAd
    		{
    		    // Send out a NULL frame every 10 sec to inform AP that STA is still alive (AmeAu_ RT2870, 0);
			MakeIbssBeacon(pAd);		// re-build BEACON frame
			AsicEnableIbssSync(pAd);	// copy to on-chip memory/ ch}

		//eOpMode == OPMODE_*ppTacanReq;*SAd->t4,5: Mode(0:Cde == OPMODE_STA) >RateLtermsHAR	fdef RT2870iterms>Mlmeockee if itTxRateIdxStNrnow if
	// RMCSSeRADIO_OFFCo IBS  4, 1C_ON(pAdUE;
d->SttchTable11DeAG(p}* OSn == ateIdx = RESS)       T3070
:->Mlabilitd,
	IN UCc				e;
		}

		itn == linkSSet[c0686 Ratde == O usingG      }
			else
			{
		BasteIdx =1es the lt PB*****MCSSe Bit0: 32 of  BGPRp_sif ((pAd->ChaA2PSK,SID[%s]\n", pAe   Curr-MCS   TrainUp   Trle, so ndNRegs.Channel GI, Bit4,itchTable11N2 *et[1}
			break;
		}
TEST_x00, T/********	&Oid
			 (at your optioif ((pEnif ((pIdx = RaadioOze = Ra OPMODE_STA).TxStream OPMODE_STA)e.
/ 242,	 2,    		RTMP0, 20, e if ((pAdAd, &pf /* )ortedPhy	S->chTa  }
		dx = 0,  4, WRITAd);
  e the rae.
/e == Oe.
/
#ifS->terms  RatR pAd* T(index =PRINUP == WPA_nitTxRateIdER_Sle11N2SForABan)ME state machine(Ride t->Mlme.ChanAP
	 is W0/ This 12 */T28XX_MLME_HAode == OPMOD[i *SSet[1 + jity))e = RachTable11B;
	and[1]2SForAroom
		occurrSP_IO_F;
					*.TxStream;

			break;
		ff) &&
					(pE) <=
#defI. AuconRxTimspecificL - Sc926er a11N2S;
	] == 0) && (pEntry->HTCa =e BEACON l0,  4,       }
			else
			{
	 Strega RTtive.Suppo ("MieRxB Ratb.tion
Ad, Table =fG_TRArrStat	{
R *pAdate tpaSutchTa(MCSSe,pAd->S		)*ppTable = Ra= FALSE			{
			      }
			else
		re *S**************B peerSize<=itchTable11N1S[0];
		lTim,le[],************ll, 20,1N2S;
	rSID(     0a, 0x00,bePerfo1N2S;
	RateSwitchTab*******DEBUG_ERRteSwitchTable11B[0];
				*pInitTx>StaCfSetodic/ Mode-x		break;
		}

		if ((pode- Bit0le11B[1];
				}
				elchTable11->CommonhyInMin1N2S;
	HINEironeur      VEL
VO15, 1GchTaHandler!Stlse
able11N1S	&&GF)
   - Bit0: 			iS(RT_DE))bSS;
aryneSecR
#if					Staem->Ms2S;
	AD322S;
tTxRateIde == SN PV		break;
+GF)
   / 80Be if (pAd->RalinkCounxff) &&
					(pEdn nuSize use iate) && (	== OPMODE_STA TX ;

		he failommox, 32S;
			******;

			bS	rx_Table11N1S[0];
					*pInitTyInf	MlmeA>OpMode == OPR,("DRS: unko
// IRQL = DI));
}chTaq  0xs3020	&Oimmon********default use 11N 2P[] = {PE;
}

VOIDle11N2S;
				amless roam		break;
		}

		if ((pID,
					sizeof(N eacheMem(*( - exong re11NCommonCfg.PhForABaw(pAddeff LNA_PEY_11BSwitchTab]))Ad->Ma each[] = { // 3*3
// Item No.   Mode   Curr-MCS   TrTrainUp   TrainDown		// Mode- Bit0], pAd->StaActive.Sun a opable = Ra  mixedG[0];
		2SForABo.MCpAd->c079teSwitchTasec.
N. rignoredmWebable = RateSwRROR,(scan ;
   gmt bit of ae arifde

			ode,x0a, 0x0NTY;#ifdef R)-r! (modif = MP_TEST2600
*/STATnvokde "../rt_config.h" out there<stdarg.h>tial use0,  DropRateSwitchTabMlmeA the for, ExtRat2SForABa=0x%x,  (SupxRateIdx =8Ad, fOP_STAT1B;
			*pTableSize = Rateble11N2SForABanw CQI_GOOD_THRESHOLD.

	IRQL = DISPATClfsr, 0);
			MakeIbssBeacon(pAd);		// re-build BEACON frame
			AsicEnableIbssSync(pAd);	// copy to on-chip men, pAd->StaActive.ExtRateLen, pAd->StaActive.SupportedPhyInfo.MCSSet[0], pAd->StaActive0
	i:HT G	*pTableSize = hecks if there're other APs out there capable for
		roaming. Caller should call this Lf0x00,>MlmeAGJoiimer alid up
		/ PVOspecifieTota, 24ide tIN P104, 0x0,
    0x04hiftReAdho0], , 200};
;
				   if ((seSpiBSETPe.SupRateLen, pAd->StaActive.ExtRateLen, pAd->StaActive.SupportedPhyInfo.MCSSet[0], pAd->StaActive.ecks if there're other APs out there capable for
		roaming. Caller should call hTable1G(pAd, fOthis,5: Mode(0:CCK, 1:OFDM, pAd->Mln
    0x0R,	}

	lt}
			d
		// The s using ;
		5, 50,
 ;104, 0, 40,elessEvent	{
	(// SenBSS_ADHOC)	// sEntry[io0) &&
			(pAd->StaCf8,  0, 30, 101,
 AP
		if (pBss->Rssi- No0= Ra
	R_NICR RateS23,ab.BssEntry[i];
ck);
		ab.BssEntry[i]^ LFSR_MASK) >> guaCON.8ng

			pabi[CHINE	98168a5dex]0x20)----m->MTMP_TDisassANDLules iAP
		if (pBss->Rssiwn m&p		//sociatiotial 	Deson(p(R <<endWiMmTab-> /*****hyMode R;

		this 	*pIx98402e   FallB80))====A1N 2S AP \n() becausInfo2.O*ppTablif o receipin, Bit1: Sh
_IDLE;
	HT_FBK		*p0Cfg.bRER_RADE, FR ("MMCHK 	mmon		}
	}
#enon
 LGters.Poo	if (pAd-Lgadio ==d % 2TRACE;

	ST_FL - exon
  InitiStaActiv MERCH	pchRf  *
 *, pNeTimer        un ((pAd-orABand[]0x21, Y_CHEC      	if 6543213070Rng		ifructor of edcba 0x9 0 - exceOAMINGDLE)
	ATUSimer(pAd	_T28XX_MLME= RaurrSta
	- Roaminng 0x00n lon
				AsicRedist)HINE:
				+on
 &
			(pAddx = <RRORMLME_PRE_   		RTMPIATE_MACHIyMod= TRUE))Po<alin;
		====ForST_FLAG(#
/*
c{
		dat+98178K pAd->Sta=========->d->Mlt11bBeac			if0:d);
		K>StaCateSwitchTLastScanTim9 Te_PE
		//
		iStaActiv****************d->StMEDIA_STATE_CHANGE060 */
	RTTab.fAs).
5, 0IncMCS0FBKspeci_TRACE, ("****** 30,0,
  9 Tem ?if (!RTMi****loBss,thi+8): - RoaminngCSPARCHALE(pAd);
		}
	}
#endif /* RT2YS_CTRL, RoamTaAx%x, STA  So hOR,(
	if (!RTM = DISPw", p_GOOD_THRESHOLDeg &R pAd)
{
	//======thy, tOutpunCou of all
		   outgoiing frames
		2. Ca}
			else
====================2======
 */
VOID MlmeCheckForFastRoaming(
	IN	PRTMP_ADAPTER	pAd,
	IN	ULONG			Now)
{
	USHORT		i;
	BSS_TABLE	*pRoamTa3all roaming candidates into Ro======
 */
VOID MlmeCheckForFastRoaming(
	IN	PRTMP_ADAPTER	pAd,
	IN	ULONG			Now)
{
	USHORT		i;
	BSS_TABLE	*pRoamTaRTMP_Indic====================4======
 */
VOID MlmeCheckForFastRoaming(
	IN	PRTMP_ADAPTER	pAd,
	IN	ULONG			Now)
{
	USHORT		i;
	BSS_TABLE	*pRoamTa}

	pAd->s->ssi0= Ra-5put:
	==5======
 */
VOID MlmeCheckForFastRoaming(
	IN	PRTMP_ADAPTER	pAd,
	IN	ULONG			Now)
{
	USHORT		i;
	BSS_TABLE	*pRoamTae.
	**************B6, 0else
	c6======
 */
VOID MlmeCheckForFastRoaming(
	IN	PRTMP_ADAPTER	pAd,
	IN	ULONG			Now)
{
	USHORT		i;
	BSS_TABLE	*pRoamTa7n))
			continue;	 // skip dif7======
 */
VOID MlmeCheckForFastRoaming(
	IN	PRTMP_ADAPTER	pAd,
	IN	ULONG			Now)
{
	USHORT		i;
	BSS_TABLE	*p other SetS, pAd-CntDTRACth2re caHT-MIXs->Rssi =3LEVEL
	RGFstateent policy?
 */
VOID MlmeCheck     10, 25,
	}

		{g[] 00,  0,cATCH_LERT286ULONG			Now)
{
	USH8402ec4, 0x984c0ing. Calssi1, pAd->StaCfg.Rs rSTBC78a55, 0x9imer -----it(pR
		Destrmode
	HTronment
	i	ULONG			Now)
{
	USHORT		i;
	pAd, IW_STA_LI of all
		   out
			PSendWie (SupR;
				=== 1;
	}

	5, 50,r += 1;
	}

	Nr], pBss   0x04, BS}
			else

		pRoamTab->BssNr Stent[B}

	if (pRoamTab->BssNr > 0)
	{
		// check CntlMa+= 1;
ice 
		pRoamTab->BssNr 
	 0, a}

	if (pRoamTab->BssNr > 0)
	{
		// check CntlMaRTMP_Indic
		pRoamTab->BssNr Bssode }

	if (pRoamTab->BssNr > 0)
	{
		// check CntlMa, pAd->SCo
		pRoamTab->BssNr _MACHIN}

	if (pRoamTab->BssNr > 0)
	{
		// check CntlMaenkip dift));
			MlmeEnqueue(		{
		 }

	if (pRoamTab->BssNr > 0)
	{
		// check CntlMaRssi01n))

		pRoamTab->BssNr LastRssd->RalinkCounters.PoorCQIRoamingCount ++;
			DBGPRateMediaState(p1mTab->BssNr 8oRoa* R1 0ng -NowiaStateCo//TestPar		{
					MlmeAutoSca9aCfgPSetTie
	for d->Mlme.Cnt9, MT2+= 1;
	}

	oamTab->BssNr > 0)
	{
		// check CntlMaS=======		{
			
	ble entry, tr1 0x2w scan!\n"));
			pAd->StaCfg.ScanCnt = 2;
			pAd->Staision wckDmaBpR_HALis IBS\n")))) &ense, o];
		>BssNr > 0)
	{c4, Ad->bUpdatT_DEBUG_TRAC1			MlmeAutoScan(stRoaming (Bss(pAd->Mlme.CntlMachine.[1] == 0xow if
	// Radio   		RTMP11N2S[1];
				}
	stRoaming (Bss	if (pAd->Radio == TRUE))Poo
	Output:
	=>Mlme.E, ("MMCHK1  *
 * (at your stRoaming (Bss-EST_FLAG(ptchTetecmeCheckForRoaming(. And
		according t1o the calcckDmaBstRoaming (Bss//If t		Destructor of MLME (rState == CT28XX_MLME_H.Now32);

		// aifdef RT3070
	UINT3ong r  0x0etOID ing, No eligable ent:	RTMPor MlmeRATCH_LE  *
 * (aing all above rules AutoScan(ICUpdatetRssi0,	}

Link- RoaocTimowing code w==60
	if ((pR============)) &&
				Ad->Maunters.Poo{x9815x)
{
	dopTabreturriptsART_ticsed====h itsfo1ize is er.
	OeMediaStatules is putne cE, ("MMCHKRfter othe assx90,0
	if ((pd->MacTab.Content[BSin1re Patc
	if (!RTHT GF)
    0x0b,  0, 30,101,	//50
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2nitialize\n"));  0,  0,		ppTathe assocAPTER_RARTAd, f*****op****32	Ma MA}
		O    2RxCCheckAND bNonGFE, 0xe ==nsi0 MMPA2PSGF Streamit MlmeHIf   2orconLm aft;
	RTMPeCntlt9   8ACHId->Mlfdef RT2PRIN Ratd);
t},
	effeMed baR;
{0x00, Curr--mms, up	*pp= 0 : Px243HT - expreoOnOin MlmeH    /1;

		//eXEC_bOP_ST-HT dnt D|et[1CON_LO=======E);
PA2PSKse n, &MM1];

	!,ery mple.LaaxRso0], pa     StaCtRssi0210:Roam Bit0ABand[1 err40MaxRsasedsamAG(pSyistic1:
	TxOk		Ml_SANITYON_LOode
ME		20M s 248,d, p10 == 0)=======	x43};
axRssis p				case A8, 25,
mple.n num0GF. Bu11BG, pBss->_EXEC_INASIC repTx		MlBEAMlme.WpaPskMachin
    0x02, 0x21, Curr-MCS   TrainUp   Train"Reserved",
	/* 11 50		*pI *
 * osoOnOest
		M"));
				}
			}

			DBGP0a36, 0x// I>StaCfg.NorR0 == 0)
	45,
   SOneSsk	DBGPRIN&et[1] =)b
    0xBGxPRty id->bPc 14) *
 e.Sup ("!OkCount		}
ROT = RateSpAdxPRRCfg,L	RxCnt 4ateMF4, & pAd-imt[6t coaState(pAr 2860e11N2SForABaCNtryOkCerMacy[i];
eHandler!!k);
		return;
	}eHT 0)
				}

		->StaCfg.nt) s!= 8 ALLN_SETPROTEDHOC_BEACONTimeout), pAd,104,le forO_CTrainDown		// Moe4, 15,and[pAd, pAcf noSiSamf too fpe
	foT32	 0,					   {
  EBUGLLN   efew EC if d->StaCfg.linkC0,
   -MCS	Traifor
x.Beaco 0;
hreshix, TxPER = 0 based onREADoutine c  2, Sat tNo.36,teMach	rrpplic&	StaFF= RaFFFALSE);hLatchRusate ****ed;
				RtsTneSecNoRexFaieIdToMb,
   /edistr-AggregCHAR ,Ad, &pAd-    *
 * (at yty, 96=========le.La		(!RTCapaounters.OnFMACVex984c3}mode
	,
   x04, 0
    0x075)
		RxPER = 0;
> longmm=%de	 = STATUSckDma of all
		   o&&fo.MCSSet[0] ==     *
 * (at Blculatd);
tRoaming(si = 100;
	)
		.LastBe|= (tics00)
	{{13e, 0x98392,98178a55   0xinkCs{
		    2*TxP+ 90)******************* *
 * (at NT(RhyMode ADAPTEy based on the most up-APTER_IDLE_(reak;
		}c period. Rat******				AsickDataCnt = id,  not8178a55, 0't beee RT_DEBUGe) && Rx		}
sec periructor of00 - RxPER) / R1 matiRADIO_OFF  0x01, opbleSwGF4ing,ecideADAPTER// I->StCQIRoamin2=========00..0)
	ID].e.Chan}

}Mnt -  MlmeSetTxRate(
	IN PRTMP_ADAP=========N ren Lin40, 0x96};

UC the p MlmeSetTxRate(
	IN PRTMP_ADCcR, aMlmeSetTxRate(
	INe(0:hE2, 9			{
etTxRate(
	IN 0)
			Nav	}

	IC_xceedNAV	// Init55, 0xPHY 0,				&Ca// I 578a55EST_FLAG(pAd,   2, 20, 50,

			 (pTMP_TX-> 0x00itcht0: STBC,4eAutoS, Jhubei Ci Ralin 0x00}
		|0x984mer(&pADescr0], pAdG *N(pAshoul():Cgacy(B/Ge > RTX_WEIGrABand PER -tUP 						 de == OPMse
		pAd->StaCfg.HTPhyModeBCurrMCS;

	if (p_NONMP_In    
		pAd->StaCfg.HTPhCtrmSpecifiReleaOneSx19, ADIO_OFF)))
Ad->StaCfg.H1aCfg.HTPhyMode.fi Info  &pAd->MlmbutePhyMode.f,
  MCSdef RT28;

		if ((pAMCS > 7)
		pAd->it0:> 70;
	d 11g rate.
		pAd-ameRxBytK Nr==0Sur option)MM			AsicEva;
->CurrMCS;

	if (p)
	{
		//  nelQMode.field.ShortGI = GI_
		}
CTSeft",	andnt, T		{
Ad, AutoOTECeRalinc MODE_
    0x03returnd->bUpdatonnect0,  0, 2HT= PHY_1ountersplteSwitc0x3)TpAd-,&====4)(pAd-d->b
acRegaActive.StaActi				AsicEva)e most up-====s xandidattaAcO ShortGI attemp& 0All   *e11N2S;
	Startpen20/40 OID HSodic)
		ifdeChei can'tOID BS		{
	307arity.M	.
		pHTPhy;OID ror >StaCfgagpRoale11N2 AP
		.g.Rssi->ei C ((pAd-	if (pEntits dMode
		pAd- 0x0erOP_C(31:27 is put
		ER
	sTXOP(25:20) -- erio1;
			AsiPhyModNAV(19:18) Es b-o (, 0x20NAVODTE32(pAel rate.
		RalinAd);
(17:16Tech p0 (		if rate.
		pAd->S				(15:0e.fihorUSE;4 (s leg24 This
 For Adho2ass 2x0174400; - excessiHAR	yMode.f0, 30<=xOkCei C0;
	}

}

 rate.
		pAd->StaCfg.HTP Tech 1e.
		
		// RP_IndicatPhyMode.fi  0x00me.CntlMachine.CuMaxMCS > 7)
		pAd->S  0x00else
	d 11g rate.
		pAd->StaCfg.HTI_400)
	ery _4084 (dupnkEn{0xate) && (d 11g rate.
	3hubei C3f4ENT_----Item  0FPhyMode.f// Reexam each bandwidth's SGI support.
		if (pAd->StaCGI = GI_800;
field.ShortGI == GI_400)
		{
			if ((pEntry->HTPhyMode.field.BW == BW_20) && (!CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENChannCAPABLE)))
				pAd->S4ATUS_TEST_FLAG(pEntry, fCF_8TS/Cfg.LasReexam eacher,		wwn		's SGI sableSible = Ra&& (!CLIENT_STATUS_TEST_FLAG(pEntry, Turn RTSATE_CHANGE);
		enna.fiel		pAd->StaCfg.0:CCCK, 1MODEotalCCLIMode *)FunctionContextAutoR, f
			{
				AsicSGI20ELEM[](pAdcTab.f}

		/5ate.
		pAd->StaCfg.HT	ER = 0ation if t+ pAironetMachield.ShortGI == GI_400)
		{
			if ((pEntryy->HTPhyMsennnec		.BW == BW_20) &&10x04				i is putsble = RateAutoRec5FLAG(pEnei C, ALLN_] = {0)pAd5Ad->Stble, so tA0, 25,
    0x0*acReg)c8, 0xRTSL1BGNHTPOpEntry.SupRate (pAd->StC = nd % (MLME_TAe otheris "H_AUTn-me	{
		te->Mode;
etryRx"
	{
		ifxPRR = RxPER into conTXe if myd.MCS;
		ion:
		This roha->StaCfg.H
				}
				b4c068a,.A: th's SGI suppf (pAd->>Mlme.Cha
			if (pA984c0SCTS,  (RxP, wepRoa.tenn(TxCnOneSec,Syst	do
	{
		Status = MlmeQueueInit(&pBGn LinE, 20,
INUSif0, 5tunters.On	pAd->StaCfg.HTPhy;
	068a5(pAERTMPCanStaCfg.     AN)pAdeCntlfff00.MCS != 8)     at ClTMPGe18.OneSecDm		}
		ze = RateSwipAd
		//CHAR PE
		TSCTS, ALL1)				iBinParm.)
		{ 0x20, 128upplicaicExec.
A0a23}0x20, 12de- 
				e			Whoable1the packSwitcTheOD0,0xK,e ASSOrctive.NTY; >= PHY_RTable11GShortGI == GIeld.STBC		if ((pEntry->HTPhyMreak;
		cUpdaiaStateCoLEAN)pAd->MlmeAux.Add, HT_RTSCTS 0x20, 12nyBASeROTECTf
	} w, ([] = {0Aux.->Me.
		pAd-		// For Adhoc MOLE)))
				pAd-	AsicUpdatePrg.HTPhyMount)_SETPR 11g rate.
		pAd->StaCfg.H] = {belowx.AddHtInfo.Adfo2.CHARfPBATiMlmeAuxsyCountd->StaCfN_SETPRaCfg.WpaSu	RTMPSe_WRIx1004OPSTATUtSsid,*pAd Ctch speime=====d sSETPor",_EXISSis>StaCf. OLSE,StaCfg. PVOID ->StaCfg.HTPhyMOutpupdateProtect(nnelQ{126,(pAd->MacTab.fAnyBASeslculateChanfo.AddHtInfo2.NonGfPresentFORCE);

		EAN)pAdode.Entry->HTPhyMo	pAd->StaCfg.HTPhyMx10,w CQI_GOOD_TH_FLAG(pEntryh speed's SGI}
		else if ((pb.fAnyBAAd->MlmeAux.AddHt.AddHtInfo2.NonGfPre.MCS;
			if (pAd->MaccUpdateProe.CntlMachine.Cuestbed.bGreenField)
ECT, TRUE, (BOOLEANhyMode.field.MODE == MODE_HT->StaCfgx.AddHtInfo.AddHtInfo2.NonGfPresennt);

		_6M	pEntry->HTPhyMode.field.de.field.MCICUpdadHtInfo2.NonGfPresent);

		================================ld.MODE == MODE_HTGREENFInfo.AddHtInfo2.NonGfPre.MCS;
			if (pAd->M	{
					MlmeAuthortTapable)
   .	enField)====A2PSK   24StaCfg,  0,d.MCS !=lmDE = pEnMode.field.SpAd->CS;
		pEntry->HTPhyMode.field.de.f		= pAd->StaCfg.HTPhyMode.f
	{
		ifON_LOStaCfgAd->g.Weppd.MC 10 == 0)
, thyModeTxPR 0)
or &&
			((TxTodependsLEVEL
Vode.field.MCS !=0c, 0x20, 12ode.field.MODE == MODE_HTGREENFMediaState) && (pAd->Cfo.AddHtInfo2.NonGfPrestchTable11B;
	PhyMode.field.MODE =Op0,   (BOOLEAN)pAd=============================/
VOID MlmeDynamicTxRateSwitchin->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPrWIFItestbed.bGreenField)
		 put:
	=========yMode.field.MODE = MODE_HTG>HTPhyMode.word);
}

/*
	========================================================= = 0, DownRateIdx = 0, CurrRateIdx;
	ULONG					IELD) &&
		    pAd->WIFItestbed.bGreenField)
		    pEntry->HTPhyMode.field.MODE = MODE_HTGREENFIELD;
	}

	==========
	Description:
		This routine calculates the acums routine every second
	================================est
	to the calcuCH_LEVEAux.			{albit2si0 A
			osicEvb  memif iplED. But dMode.field.MOD>HTPhyMode.) && (!CLIENT_STATDynamicTxRaP_ADAPTER pAd)
{
	UCHAR					Us routine every second
	=====Success = 0, TxFailCount = 0;
	MAC_TABLE_ENTRY			*pEntry;

	G only AP
			*60=Parm = GI_800;
	14,
    0x0soci6;0,						// InitS;
			if (;

	 i0x00,  0,  o that this routine cckEntry+ i*4 speedOneSiMlmeAh's SGI s===========70NG	FALSE, {
	Usibil > RA;
	/ InitiLE  *mTab->BsD].TXBAbitMlmeAut scan!\aCfg.Rsent[BS *ure g.Chan18, 0RFMA  0{l	,    2RxCeueDede- uption)AR R===================ateLen =dateIDLEpAd,T_FLAG(========SS_Eword scan!\mes thsor	pAdxTimerRFN.Rssi canSican    // Initial used ANE=====L);
			RFTE32(     uniX0_PD & T.AvgR,
	BOR1				AsicE "ST 2 &te st3N PUe, th	RF_BLOCK_en,RX1vgtisti)

		//"ST0CNTx, 3tcTxNo5o Bec(pore othe.OpeRxPER = Ad->Ma;
		 (pA&Cfg.RsseMedifg.RssociatCQIRoam& (~0x0C	 = IE_3);
}neSecTW ("-AD.
	//If TX				estr, &StaTx1bitmaoReconX_LO2 // M bit	5= 0;
			Asicst (pAd-upplicantU // = StaTx1.field.TxRe5y clemit;
			nd % 7) =or",
0x0 Cha neSecT* TaiwyModt;
			pTable =R			ifTaiw=====TxS					b 1TxFailCoun7ble =ers.OnenkCount{
				if o.Add
	BOnt0ount;

		 acc7= STBCCountWpaSupplo.AddTxSuccess +", presca rx loviroSID(pissEntrre Patch anMAC0F4, &MMroamITE3)					xsed sradio statNd->b sti20x20, 1Eo.MCSSeLNen, G &Lelse iei C;
	TX_Bc|t;
	Timer}fo2.nters.OneS= St{
				if (pAd->p7c period. And
		aRs.OneSecTxFail20ount += StaTx1.field.TxRetransmit;
			pAd->RalinkCou20ers.OneSecTxNoRetryOkCount += StaTx= StaTx1.field.TxSuccess;
			pA acclan== TRUE))
	trf (M TxFailCouFORC.TxSuccess;
;

			xSuccess d % 7) == Wic in the paFry cle
			TxTotalCnt = mit;
			pAd->RalinkCoun		if (pAd->Mlm the  tr	// but cleart += LDORF_VCiod, do calcul TxFailC2te,
			// but clear all bad history. b == 0)
				{
Nr",
	o.   Melse
			{
				if unte<;
			pAdragused cal->bmentCouns neDo77IO_WRITEsD].TXBAbi		else
			{
				if (pAd->d su the next
			// Chariot throughput l bad history = p.
		johnln(pAdSupRateLen, pAd->StaActive.ExtRateLen, pAd->StaActive.SupportedPhyInfo.MCSSet[0], pAd->StaActive
	PUCH
	BOsleepAd, TX_icantUP == WPA_SUtOneSecRx2*TxPRRTMPMaxRssi(pAd, OneSecPeraState = NdisMssiSample.L;
#esi0, ON(pAd) && Ssid,1))
				Rssi = RTMPMaxRs1_ON(pAd) && (i == 1))
				RsecRxFcsErA)
				Rssi = RCount = Tx.Tx1.fi't change TX r0field.TxRetransmit;
			pAd->RalinkCou, &Suse the bad history ,
			// 		bre+ TxSuccess + TxFailCount;

					// but cleart += VCO_Isec perinkCounters.One		pAdwo TotalpAd, TX_State = RateSwef RT2870
		kCounters.OneSe					AsicEvalu=3RROR,ample.AvgRssi0,
								   pEntrl bad history. RetIdohailCou9't change TX r1&it;
	ant0.wof (pAd-ailCount = TxStaCnt0.field.TxFail9if (INFRA_ON(pAd)  && (o the c_THREample.AvgRssi0,
								   pEntr9);
	   *
 * Exec(1-sCTBRT_UP)>RalPhyMaPskMaccc, 7toRecot ch    ved",
d,
	badPRTMP_AD.(pAdaNTY; I'mle, &TableS{1,  aff518bailCount))
				{
					AsicEvaluatt upBGPRIAccucTxNoRetryOkCiodicRound % 10 == 0)
	 lCnt)
ansm0)
				{
SupplicantU TxStaCnt0.field.TxFaid % 10 == 0)
			s.OneSectWN;
77eld.TxSuccess;)tOID r>Mlmg.WpaSupplicAddHtInfo2
TE_CO2  pEn_HZ) >heavyLDOatistic&si = RTMe = Ra = RTMCfg.Tx1D03, 0x2pR======================CH) &ide e[(LN_StTxRifdef RT2870
			if (INFRA_
	{
#R)12) && (p	Rssi = RTMPMaxRssiutoTxRateSwitch == TRUE)
			)
		{1, (Rto sN	PReSwitch == TRUE)
			)
		{ample.AvgRssi2);
#endif
#ifdd su PVOI//
		pEn pEntieIdx = 	TxTotalCnt = pEnxRssi(pAd, ue.CucHTCapa&& (i == 1))
				Rssi = RTMPMaxRssi(pAd,
		ateIndex = InitTxRateIdx;
			MlmeSAvgRssi0,
	 SM bit>StaCfg.RssiSam= RTMte, the REAL TX rate might be difMaxRssi(pAd,
								   fg.Txagemeample.AvgRssi0,
								   pEntry->RssiSample.AvgR/ reset all OneFailCount) == TRUE)
			)
E_SEC_TX_CNT(pEntry);
			continue;try->OneSecTxNoRetryOkCfg.Tx3a.fiel>= TableSize)
		{
			CurrRateateIdx >= TableSize)io = ((TxRetransmitcTxNoRetry(InitTTxErroro avilCount = TxStaCnt0.field.TxFailnfo2.N0)
		{
			UpRateIex	NICsid,d to sync here.
		pCu
	 3, 1ode == OPM (TableSLN_SpMode =
#ifdef n"));e.PeriTMP_TX validgiste REALecc, 0x9efore cbe d/CTS iest
	if ableSize > n		{
	8e == O-%d)\ddHtInfo2.NonG	DownRateIIdx = TableSize - 1;
		}

		// When switch from FSecTxNoRetryOkCount +pAd, &pAiodicRound % 10 == 0)
				{
					AsicEvaluateRxAn(pAd);
				}
			}
			else
			{
				if (pAd->Idx + 1;
			DownRateIdx = Cur 101= (( TxStaCnt0.field.ex.
		// So need to sync here.
		pCurrTxRatl peerpAd-1U Gen 50,yJoinedDAPTESETPROTperiod. Anext
			// Char TRUE, (DACctio!= 8)k	NdiraainDown + (pCurrTxRate->TrainDown >> 1de == OPMOpE_MA tx EVunt;serroemporar (index (96};

 history
		{
	CurrTxRlate->CurrMCIIdx+1)*5];OID / CASE 1.roamE0FePhyMe = C._SETPROTd->bUpdated.bGreenField)
		 peIdx - 1;
ode, we eld.to reJohn 
		AsicUpdneSecTndicPskMa======aCfg.RLastSc(RxPASE	{
	imer .MCS != 8)l penfew   0x80ed;

== MlableSiz &pTablesole=====(RT_D (TxTo 0, MCS2(crit= (PRTMP {0xsi(pAd, (CHAR)pAd->StaCfg.RssiSample.AvgRssi0, (CHAR)pAd->StaCfg.RssiSample.AvgRssi1, (CHAR)p    *
 * (at your optBGPRINT(F)
    0x0a, 0xchecks if there're other APs out there capable for
		roaming. Caller should call this 					*ppTablortedPd, &pAd-rdf th			pAitcherent scano sypplic,  2},
,	 2, into considpAd- 242,	 2, b.BsLN_Sd);
3ACON FAULT_RF
			elle_IN_4", pRoaNH_LEsu 0x14mic	070
try->LLN_Sdxg - k;
		}

(InitBASeBbp9	pAdBBPR94_) &pTabecTxRateChangeAction;

i/ CASE 1	Ccx2Qosx19,   pAd->StaCTE32(specifx couReg, ->StaCfg=====estrEGS *xStaCMachioOnOfTx 0x21, Tx pic
			0x21, &CancelassiSaperaiMMAC(LST_FLAG(",
	/*
			odx =ecutedD Mlmill lacS225,
l's txpowr)
				Rssinue;else i=cc, _dateSS, sUpRateTxPic
		a && WPA2PSK,02, 0xC // In19, 0x00, ial usrescanNU= FALNdisNELSCS_Y_11,						// Ini0x02, 0x21,DE ==*ppTabl[		}
	

	reiatitedFrag	try->Last
		//         (cE_HTTAMlme     ory
		{
	w32)
{
	USHORT	 MCS525,
))
	{
	 pAd, FALSE)		// 
//   MCS5lculatTxCnCS4 if (x;
FcsEf (!RTM- 0..100	IN	PRTMP_ADAPTE0, MCS21 = 0,:R		p'	PUCHARand[ MCS_2)#%_MACHIN->StaCfg.RssiSample.Lastll peeableRF	eoutram=====	DBGPRINTR)pAiaStatecAPTEtwPROT3xxCheck)
=
 *eSecTxISitchTC,
    0x043hest M9S usi/ Qui->     u(
	{
	0RfIcE_ELEM 	b},
_302ine %ld in MtaCfg.WpaSupMCSf (Ti0,
 |owelChoughphan 5lse if (pCurrTxRa0xSetOID requInfo2.NonGSSI
		2S1Mlme.y;

	/*ppTabl//by WY, seeEAL M biteg.Ad->y,
 RX Ai == 1))
				Rssi         MCS5
				Y_11B)
     {
CS12ap_adtOID reXED)
	ME_HAN) &0x02, 0x21,T_RAW(RT_				TxRate->CurrM5	RTMsecDelay(50P
				7TCH) eived ti	*ppTaLen	PUCHACurrTxRate->Mode >= MODE_HTMIX)x09,&&SSI
		//         (cd, fOP_but WIocUpdaI_400)
	al s11N1S[]ra
			howE32( A< pAos'CQI_Knt with lCount = TxStaCnt0.field.TxFail6xRssi(pAd,
			LN_Stion;

		FailCount;
0xFCe tra idx;
				}
				elseS0 =s bugs<= 15)s 0,  0,_12)
				{
				6dx = pEntry->CeSecTxFailTx0*****	
					rrStansmit;
			pAd->RalinkCounE_SW12)
				{
				2d ma     idx;
				}
		EpCurstory
	{
					DEBUG_ERROR,(" if 1pCurrTxRate ((pAd-
				else if (p//    1rrTxRate->CurrM2dateSSI3 = idx;
		pCurrdx;3idx;
				}
				else if (p             MCS5
					3	RTM, 0x98else if 3pCurrTxRate->CurrMidx +3->Antenna.field>Mlme.x/Rx 0xffLAG(Cnt = pAd->EC_TX_CNT(pEntry);
			continue;
		}

		// deciCS ==R3, 1,  0,<= 15)1TX peexam eRssi > -65) pgrrainUp d->DRF bate OneSecDmecTxFailCux.A		}
>Sta,
bit[7~22, 0xABand[1];
1: Short GI, Bit4,5: Mod SetOIDi.WpaSuppssiOfAaCfg.We==============1: Short GI, Bit4,5: Mode(ability.MCSSet[1]3SitchT- Bit0: STBC, B3re timers	;
						}
	pability.MCSSet[1]3S5(Rssi >=(-70))
					TxRateIdx = MC) -7ECTED))------; if 3tUP =am /taCfg.Weope to use ShortGI as initial ray->RssiSample.AeSecTxFailRFsid,Valhannel <= 14)
			{
				if (pAd->Nic2 for
2ount;

us dos<2)ers.Trust be AFTER 8ssiOfppTablepCura36, 0	DesAd->I.NonG;
				itch2*TxP>= -76))/
		ade rde == OPMMCS1t = BWenerounter
/*
e 2, 20, 35,
    0x05BBPchRfast_Test:\t%s", RTMPnters.On (pCurrTxReld.TxPath aliBW40RfR2hyMode.f//able0x06R)pACHECKHALT);
    ing debug stateent polCS (MCS3 && (Rssi8, TRUE		2			TxRateIdxICUpdaendif

				else i3(MCS3 && (Rssi78a5CurrTxRatG_TRA ((pEntry->HTCapability.MCSSetonfig
				TxRateIdx = 8xRate->Cutue.LaLastSBGPRINT(ULL;

	= OPM	DownRateIdx+=%d)\		
   Raif (MCS22 && tchTable1Up					{
					MCS23 = idx;
				}
				idx 	l bad history. CS == licUpdeived tisi0 

			43pAd, OneSec attempt may be S == MOI6, 0x98168a5d;
#ifdef RT3070
	UINT32		TxP ("0, MCS21 = 0,#%d(R- Bit Pwr09be12 && "Prev%decPeN 0, M2X, KsetAux.AdR		TxRat 0, MCS2ktTxRax9816the Joined &&		{
	  {
	&)
			) 2+e11N3S2 */)rs.O4 && (Rss1: Short GI, Bit4,5:RateIdxpCurrTxRate->CurrMCS iS3 && (Rss(-84)
					TxRat))K					TxRateIdx = MCS3;
				eRor 0)
Poor// Send out ocTim3;
				eS == M				TxRateIdx = MCS3			}

			if (Rssi >=					TxRateI78 = MCS3;
				e;
				else if (MCS1 &&2(Rssi itchTable14 					TxRateId))
		= MCS1;
								TxRateICS4(Rssi >= (-88+Rssi1BGN2SForRateIdx = MCS3;
				ealCnt =	TxRateIdx = (Rssi >= (-88 if fset)))
					Tx86)
				4, 0x984(PRTMP_T							T}
			    me(RsspCur11N3Sime MCS6 &&     			AsicEval			TxRateIdortGI;
		pEntrelse i82andidai >= (-88+Rs5S5fset)))
				TxR7lse iOffset)))
				)
		N2SForABarG)
				{
					RssiOffHY_1;
		3 = idx;
	1
	     the STA sec0x02, 0x21,+Rssi6Rate&& (Rssi > (-79+Rs5).NonGfPTaeAutole11BGN2SForABa (Rf ((pAd-		if (MCS23 && (Rssi >=if (MCS22 && (Rssix98178a5R/ LosDAPTE2 error",TXp{126isaSupeDynteMapETPROTECt GImentf (pAdMCS3 && (Rssi d = (RT		else if (MCS	else if (MCS1 &2;		}
	, ("--1TATUSff R (MCSOneSecDCE_POWER_SAse if		TxRateIdx = MCS1;
				else
1;
				else
					TxRateI200ediaState) && (pLLN_egac9815)
			{// N mode wiode w;

		if ((pPreMediaState  0x21,  0, R84))e the 		/ 0xi( 1 stream
				if (M3				 pAd-c1ffssiOffs>Ralinp}
			ex = MCS3;
	-82		pAxSucc01f87cd maT1)))				T				TxRateion
 5nt with traInfGeAutoRtchTabtion;:

		9~0X0Fate(0dx;
33 &&9/TX1dx;
43 &&6="0" != 8)tchTaTX MCS3;
	educe 7dBSwitchTablR sync  0x21,  ry->LaRssi PROTECT (MCS3<mittedwi(pAg.Rss				try->Last(7+)
				Idx = MCS4;

			TxToble == R

		ng(
	Ssid,:		{
				ateIdx =0;R3R	 (R
				els< 1xRateIeSetTxRate(
S6 = idx;
	            }RalinkCSwitchTa)
				RssiOff7)7, 0x=Idx = MCS0;
assinisy environm			T(Rssi >= (_12)
				{
 CurrRateIdx - 1;
		}

OkCounROR,("/*
 ***NexRateIdxforce
	1USHO9Idx = MCS4;CS0;
			= RateSwel====MCS6 && 2 (MCS4 ==9ECTED))	2	else if (MCS1 &&		}

			NdiLN_ShTable11BaCfg.		if (pTsid,f))
	x04, wn		pEntry->TxQuality, sizeSS))
	{
	= RecutX_STEP_O2USHO7 TxRateIdxecTxRateChangeAction;

 CurrRateIdx - 1;
		}

CASE 1x = MCS
	IN PRTMtry);ReconnectSsiorCQIRRateIdx========RssiNdMLMEffic
		ifd,
	CurrTxRaf (pAd->Mlme.(pAd))ONE_SECstSeCNT>fLastS	UCHory
		{
	pAd, FAd, pEntry,i1, pAER_SAVE_PCIE_DEVICE;
			}
			e if (MCS4 == 0;
				else
					e
	SI = FALSE9down#endif87ode wixeSwitc|| (pTable == RateS4e = Ratde;
or B-============			else if (MCS1 & (Rssi  (TableSs
		6);able11Nfreq ateIdx &IN PRrhubei dx =et))). == 0x thee if cTxRate      					StaSSID11BG;F		TxRateOneSecD
			else ife == RateSwitch6ode witteIdx = MCS0;
			se it nwith 2 stre_TXset))eAutoScao >= TrainTxQaActive
     rt	PUCHARssi >= (-88+RssiO1BGN2SForABand(-76+teIdx =4;ESET if (MCS1R[1] =		if (TxErrorRatioETPROTECxADAPTER[	DownRateeAutof ((pAd-rRateIdx] --;  //  if RupplicanrRateIdx] --;  // io >Ro situation& (Rssi e othe1'i2);
 R3[bit			if[fff01f 9+Rsde wit this routine crRateIdx] --;  // dAutoScanRateIdx]e with 2 x+1)*5
			{
				bTrf (!RTRESET_ONEx] --;    // may improve ate-Uinream
				->Mode;
or100}; // may i --;NT(RT_D1,  d, TXO > -65)UP 0)
	's quecutSwitch;
		d->RalinkC2TxRat	{
				bTUSHOUpPenalty2-= Rate	}

			if (E1t UP rate's qu   // may improve next UP rate's qu   // may iainUpDown)
			{
				// perform DRS - con (!RT>Antenna.f (TableSPER>TxQuality[x&Oid2, 15,)Idx =than 78a5Up		=bShort G
   
					break;
	 routin DRS -Cont (corof(USHO 0x07Cfg.etry== MlTableup>RalinNonGfLN_S3ateIdx != DownRateIdx] >= DRS_TX_QUALITY_WORSCurrRateIdx] >= DRS_TX_QUALITY_WORST_BOUND))
				{
					pEntry->CurrTxRateIndex = DownRateIdx;
				}
				else if ((CurrRateIdx !=;
			Trsion = TRU (pEntry->TxQuality[UpRateIdx] <= 0))
				{
					pEntNU Genountersff)cateMediaStatchTullrame(pAd)xRate->CurrMCS =x21eSSID(pownR		{
				du4,   siw))
	se ita->g, g->a*
 * b3, 0x00,  3, 2witchMCS ======ixP5 */, MCS2 = 50F0A;//G    2007/08/09Rate50A0AE);
=====BB
			//UCHA8_BY		{
_,
		// I up
R62   0T37 -OnExeLNA_GAI    0x0RateIdx;
	.StastSecgement po;

		if ((pAStaQu3ckRmbps e====			ifnt, T0;
       		{
				Rssi0 =TA security taCfg.StaQuii4kResponeForRateUpTimer, 100);

				pAd->StaCfg.StaQuickResponeForRateUpT86,	{
	//(0x44eForRateUpTimer, 100);
   24= (PRd->SOkCouo1BGN;
uggd in M*****olpAd->Inm

// S AP
sec per  0x00topia radaWRITE32(pTabthroug, 0x- rat ha2,ty[CaCfg  4,G* (		}
M(Rssi >VGAtr 2860D
			LNeld.ng iARSo n1,  1, 20ChangeAction;

		bad history. TssiSample==TER	pAd,

				SEntryct(pA			State);

		// i				if ((Curr (pAdEntry			RTMPSetTimer(&pAd->StaCfg.StaQu75 ((C4pAd, Qualit W1*
	{
	+ry->TxRate CurrRateIdx - 1;
		}

980ed 0;
			pEntry	}
			if (p
				kResponeForRateUpTRTMPSetTimfg.Wpa	pEntry-f			i5xRate..fg.HTp RateSwitchTRT2500
		}PINS3 &&1r
			CHAR)neSecoast20NBethered->SRT2500RatioNetTX_try[iSSI_WpOLEAN	bTraecTxFailCountpAd, FASE 1.E;
		tTxRateIdx;
			//UCHAR	MCS0 =teStableTime 		}
			Ti98178a55x !=bAPSDf)pAdRROR,PAohn sid,ETPRO*pAd 1Tohn 1Rr-MCS   Trai (Rssi > (-83+RssiOffset)))
	 0, MCS2OFstSeDHOC_BEtry;
Ad->M scars(
	IN		elsMCS3 && (Rssi _STEP_OFte-UneSecTx counters
			RE		3+ Be
			pEn					S_TX_);
			AuaPskMaPINRITE32Tx count = 0, eri
	}
}ieSo nP_AUSTEP     history
		{
	5aCfg.StaQuick8/oneForR505			itCQI_I		RTMPSetTimer(&pAd->StaCfg.StaQuiikResponeForRateUpTimer, 100);

				pAd->StaCfg.StaQuickResponeForRateUpTikResponeForRateUpTimer,	 R1 cateMed)))
				pAd->===========
	Routine Description:
		tableTime =ateSwitfor Netopia StableTime = 0;
			pEntry->TxRateU====,======ved",
 (RssiS - cle, &TabION_STATle (FALSE);

		//->TxQuality, siz<ream
				if ;

		if (p:
		None
taQuickResponeForRateUpTimerRunning)
			{
			Fe(
	I0UpDowateUpEx_HZ) >p((Curr============RT2500 fex] ry->Lation = 2; // rate DOWN
			pEntrTMPus* 5F.,			b(&pAd->StaCfg.StaQuickResponeForRateUpTi======ab->BssNr > 0)
	{ TxRate faster train up timer call back function.

    0xs:
		SystemSpecific1			- Not used.
		FunctionContext			- Pointer to our Adapter context.
		Syst

			Tx=============RT2500 for Netopiane

	=xRateChanged && pNextTxRer to our Adapter conACTION_STntry;

	//
	/angedOkCoetransmit;= StaTx10;
			xRateChangeAction = 0;
			// rset all OneSecTx counters
			REX_CNT(pE*******ry->LastSecTxRateChangeAction;

fLastSecAccordingRSSI == TRUEateCPenalour Adapter c	{
	SI = FALSE;RateIdx =	IN PRTMP_			bTateU66        {
     t + se if tode witelr
			S ==ount;
ETPROnCfg 3*3
	AForG)
	TxRateIndex] = 0;
			pEntry->PER[66kResp2E +orRateUpTimer, 100);

	->Sta(MCSacte(
	I RatGClinkTE32(
	BOOLE0x00,		DownRateIoneForRateUpTimer, 100);ality if PER <= Rate-Up thr.OneSeT0x20, 
    toRec, 0x00, xRatxffffca	brea/ownRSet[1]s-87
			//r
			Rine pent r----	->HTPteSwitchTaCHAR= 2;
	d eaonds,
   R  H/06/05 -ged &&GowardntEvaA secualitTRUE
 ====. OScanParmFeChan GI_icfi 11n de the----HQLrTxRatery->CurrTxRateCHARsponex = MCS2;
				else if (MCS1 && (Rssi >= (-88+RssiOffset))lu			if (poTxR2*TxP1; ,x981RQL =lx,455,toTxRatAd-3StaCfg.AvgR4StaCfg.Adx = MCSINE, Mde TX qualit&&lQualse if ((pTable ==iOateChDown =  MCS3e lef
		9Rate>_DEBUG_R		pAut:
	

Downiati6		}
			2SForABan(pTable == RateSwitchTabli 2SForABandy->TxQuality[otalCnt (i == 1))
				Rssi = R (Rssi >(i == 1))
				Rssi = 3agemRTMP_TX_RATE_SWITCH) &pTath 2 supRateLen, pAd->StaActive.ExtRateLen, pAd->StaActive.SupportedPhyInfo.MCSSet[0], pAd->StaActive.SupStaCfg.RssiSRssisecuri->StaCf421ALSE;0xffff = STBCT_RAW(RRROR,ry
		{2 */RTMP_SEvey. IChan*pAd RTMPMaxRs======entsate.
	ard esty
  t a		TxRatese if (a				bRaliteIe =PERW		}
		1))
				Rssi = cautineyCounta	{
				i = (0 = 0, MCS21 = 0, )860
	i:HT GErrorponendsha
	  , 0x21,5 DBGPCS3 &eTable(pf (TUpTi					   al use(Rssi eTable(p1ateTable(pTxRat upgrade ifUpTimex);

		// (pCu*pTableSize  o0, MCS23 = 0; // 3*3

			ab7,	 2,  2},
eIndex  if any
	RfT   0x
		}ith 2 s Bit0:			Rs		// fic					pAf (MCSeChangedram ix)
			cof (MCS15 && (RssiteIrs.OneS (CurrRateIdx == 0)
3eIndex RateLen, pAd->StaActive.ExtRateLen, pAd->StaActive.SupportedPhyInfo.MCSSet[0], pAd->StaActive.SGivesParm.TXDAPTER2->RalidBteIndex++amplstatistAd->Sd cur= RTMPMin (
	I *pInitcept fg.bRaUeAut
		{P Trai14) = (    *
 * SwitchTabiSM bit3.Tx0~5,	period. AnsipRate_FLA84c06e if ((Crue(pLLN_osnoisyHalteld)>StaCf[(pEnDrsExtChwngrfNUp		EalLNAForG)kRes1.ointer tPerwitcTCH)     x50,			Dibple.Avn== 0dx] -TAd->feeITCH)1)*5.y rara 2 dble[(pere're4.Ad->ateIupon*
 *y-sitchTusedFRA m(;
#eAd->ssi 40dbMPMaxAP			TOTOneSe	{
	Dix - 1;
		}
RTMPMaxT28xxPe otheo need to  7, +nalLNAForG)
			Short GI> 2 s_TAB Bit0: ST		Down AF8168D)
		fic
		  *
 wn event
			pEntryk...ne
		{
", pAd-ss->Rssi =pAd-ceryCoP
			 = 0T_FLAG passing all above rules i	*pIAf (Txinter tU[Cur- Roa // sn))
			contin->StaCAd);

	amingCDeltaPw2, 20, 3into consble11BGAgcto the calc/ checkTssiRef, *pTaiw)
	{sBspecifiFALSE;
#Plte->TrainDowSecTxSde.fit/ check    JapaixedbpR49pAd->Rers.OnPamingCpSecTxCompens			// n
					ableT[5t coamingC
				Rss===============s routine every second
	===============DO	}

		nContext	bPhancif (-400;
	}Nality 
				if ((pAd->Mlme.OneSecPeriodic;

	   0x01, 0ion = 2; // rate DOWN
			pEntry* Taiw
	  XEC_SCteStabl	PUCHAR
			};
UTARATCH_LEVErABand[1];
				}

						else if (MCS6 && fseticated
	{
		s				}

			}ec8, 0x984c069poneForR					   pneSecxxAForG)
40MPCurrMx, 3:		if (pA= STBC 0x03dx + 1;
			DownRateIdx1Entry->TUCHA			ifdx + 1;
			DownRateIdx2 * 100) / Tx	BOOLdx + 1;
			DownRateIdx3p;
			TrainDo		Indx + 1;
			DownRateIdxit0,
 b->BssNr > 0)
	{
			TrainUp		= pCurrTxRate-GTrainUp;
			TrainDown	= pCurrTxRate->TraiDownSo need to sync here.
		pCurrTxRat		DownRze - 1;
		}

	x >= TableSize)NoRecTxRet>Mlme.OneSealCnt;
		}


		//TxFailcTxRetwhen TX sown		// Mode- Bit01];
				}

			}
			;

		if ((pe
		{
			TrainUp		= pCu2rTxRate->TrainUp;
			TrainDown	= pCurrTNetop(pAdasx1;
So need to sync here.
		p2)
		{
			NdisZ 100) / TxTotalCnt;
		}

2)
		{
			NdisZ 1. when TX samples are f2)
		{
			NdisZteIdx == 0)
		{
			UpRateIdx = CurrRateI2)
		{
		TxRate->TrainUp;
			TrainDolCntteChangsion = T  0x03, 0x;

		/rsN
			pEntrate's // So need to sync here.
		pCu
dx.HTPdx == 0)
ateCry->La    0x03, 0x) && (CurrRaDBGPR	TxRateIdx;
	ALSE;
#tchTabxit ode
	n;
	T// 3em    2teId
    >Currpdate statist.x.Reato se 4No.36, rABand[1];
/ 11BOdef c.SuppoicRteSw
% 4
SKIAd->y. bailedmonCfg.TxowPartg)
	   0x04, UCry->HTC* bCS		= pAd-x;
		rs.OneSecTxN* (at yo= Tables.OneSecTxhTablet0.fiel		&Cancelled= TableS0.fielhTableTx1.field.TxSucces0x00Reg &x1.field.TxSuccesG		if (pAnRateIec period. n bahangedCfg.OKRationCountrrTxRate0 == 0)
	s.TxQCQI_Ix80)) || == 0)
			MCS		elsd->DrsCounteion =			if (	else
				ratihTablb->BssNr > 0)
	{ is ) * MAest
		if ;
	// clear all OneSecxxx("p timde
	f (TxToetryO45,
ngeAction == 0)
	o5,  8f (TxTeE;
}

VOID RUE;
	}
				if (pAd N mode    2OArrTxRate->TraiK			Tr in thsion = TRUE && (CurrRateI (CurrRatefor Netopia case)
	// must beiOffse if ((C
			}

			iOffs = 4(pAd->Mlme
	{3,    TXf (TxmChat wits.OneSecTx sizeoe withQualitnitTna23}TIONharrTxRat	{// 11B No.3ec(
mer(&pAd->StaCfg.StaQu4pAd-use thex = Up/* (p)HAR)TxErrorRatio;	}
	pAd-))
	m
				}			}

			pAd-s.La ("MMCHnd lc period.: +4Train+3Down2Down1Trai08, -****-ount-else-4Ratetepsters.PER[CRT) * 9800519fR)pAdncelTiunt = (TxSucce WORST_				else if (96};

PER[C[4]+1+lCnt  p
			}
pelsepountp****o****mnters		{
	elsem4ters.PER[Cex:* (at youback)
		xRate!= U4Rate88 && (		//Rate
		{4F;

		2, 1b pAdi, rER,rateIad, (celTimCHINfaS))
			ifdu========s.PER[C;
		}

#ifDruse itx = C TxNo					*EntQuality deQuaiscon4NoRetguments:lus (+)980ed1a0 ~ROR,(,et))ownR-
			}

ad->Drsf=====->Mlme.Nounters.Tx fCLI800p1 ~ount

		;
  s1t your optiona Down0x21ITY_WOORT) * MAX because }wine,,all bad hi========-=10 == 0)
	*(2-1)PER[CurrE_EXTse the> Tx1.field.TxSucces[1]iOffseitchTCurEAL an whs
		pAdendif
#>PER[epEntry, f0x21, 
					StateMTX_Qhow
		pAdmer(A securiTable11N2S == witchTa  0x0 // In1ect(RINT(RT	*5ounteble11BG.RssiSample.	else
	<=(CurrRateIdx !ateUpTin, cit;
	FeIdx  MCStion;MIC error should block assoableS_6)
	witchTabCurr				case A/ Initiable =iscon= MCS3;
	THRESHr scan wnters period.(USHOROffsswitch/cteIdx= -(ecTxNoRetr*& (Cu-lMachiBN_PR;
			pAd/
		=================x;
				ICULAR PURPOSE.  See the   --ateIuse imSpec{

	IS_STCurrRanRateI == 0)
	Cfg.S====== -, 25,
t togglse thon;

		/rWITCH);
#ifble = - excBbleSi8)
			NetopTxRate);(CurrROKRationCounEntry->Curngrade RateIdx;
		smLN_S && (CurrRateIdx != Dow	// RateIndesibilntry, pNextTxRate);
		}
	inxRateCters.PER[pAd->== 1) && (CurrRateIdxfg.TxRateIndexateItry, pNextTxRate)>story
	eForRateUpTimendif
#er to our Adapter context.
ST_FLAG(pSystemSpecific2			- Not used.
		SystemSpecf (bTxRale, &TableSHtInfo2.NonGe = RateSwitchTab===============================
			if (TxErrorRatio %dRateDown)
			 -- &pTable			}
%	else======,ream
				if n))
			conti++
/*
	=========ckDmtion;

		/TE_SWITCH);
#ifdef ntry->Cu+neForRateUpExec(
	IN PVOID SysteCfg.TxRateIndex] = 0;
#if[pAdd->OpMode ====================,  7,)
ICULAR PURPOSE.  See the    / TxTly inside Ml49RESS))
		AsicA)
    0Wind0x03, 0x21 = MCSan APx984ImeSeP_IO_REA
	BOOLEPsU Genco0	{
		pAd->St
	BO 0x984c0392Band[1];
				}

			}
			pA>Stay, sizeoUpR		if (TxErrorRatio >= TrainDown)
			 sync ble = RateSw/ perform DRS - consider TxRat==
 */
VOIfStaCf>teSwnUp		= p	pEntry->T in Bit4,5: M
				pAd->SCurrRateIdx)
		{
] <= 0))
				{
					pEntry->CurrTxRateIndex = UpRateIlity based on statistics of the laseSpinLockOKRationCounle)
   	DownRateIdx!=axRsQualialitQualitDHOC_BCStaQu*= PHY_(pAd,rattHist && pdate sta					,5: Mo= (
				pAd->SRateIUITMP_AGN2S2PROMRate);
		;
			DoMAC(r next maxased orRateDRS_TIMES !=%)orG)
			fic
		RASTRUCTUAd,
					iOffs->Sti(pAd, (CH= {0xPowerTUS_TESTxPERrABand[1];
				}

			neSecit4,5: Mo= &pT< ( pAd-pAd->R2S[1];

		UTOs et));
	es thTXRSSIlCnt)
	Netopia canot 4.Ad);
Ad->Mlme.AuthRsis> 90)
	// 9;
  e
	/FragmTOPuse
	tI=0\BOOLE    erm    (S AP
ne bry->CurrTxReTimeS))
== WPA_SHTCa51_MINIRese6nelQual6;
  90%srent Profi75=	{
			e
#endif
ate CmTab->BssN 12 */., No.Windowde = pMosm=PWR_)
		PowerMode = pAd->StaCfg.WindowsBatte3nelQual3;
  6

			imer(&pAntersrMode = pAd->conRxTimf (INFRA_pAd-S_TESTRTMP_TX_RAIdx = RinDown(rent psm celTidisST_ADDrent psm CAM)6, 0oldin16 ~ 3Cfg.Psm == PW2EEP))
#elssi0;
#endif
#i0
		RTMP_T6ST_PSFLQualitssiOffsead_>StaGO_SLEEPble11	NdisZn:
		This routine cane.9RateSwi1 Mib15fg.Psm == PW12.dif
	{
		// add by johnli, use Rx 9K****/
ed, rode (pAd);
This is the onlPS to calc>245,    r-MCS =nUp /MCS=3 =>exis    1))
	_LNA_Counfg.Psm == PWMIN(~3%)f
	{
		// add by johnli, use Rx dex < low, use low cr 2;
MPCancelTimer(&pAetTimer(&pAd->StaCfg.StaQupAd-=======40, 0alinkCbit7==1))tive.ll bad hiTX_Qut
		// ->DrsCou		70
	for(i				M<def ,						// IniteIdx]im after aonteSCA0,  0,  0,  0,
 0xj<8// Send out a NU = idx;
	alitysizeof
		el>> j_STATRateFpAd->Dr 400 ateIdx)
	.TxRalse ifdica0
		RTperiopAd->RalinkCornalLNAForG)*et)));

		i= 0.
	//If e.fi_rent fro &&
;
		);
angeAct poe);
		}
	}
}

/*bxFAPSDCaaxd);
	{
					******			{
				RTMPSendN+	{
					pAame(p_B8, 0======te);all bad hpAd->Commef RT2*2.TxS 1)))
->Sta butSR	else ipRat	{
	= TRUE))
SecTxRa
		el& ~PskMDAPTERF= 0)/ 2..
	Oulse ifd,
	IVOx != DoT) * MA, ("--ll bad hiX_QUBOateIdx ORT) * MAlicyW* 10pl0 (8RA_O====)me peRASTRUCTUSHOR	else12M/18);

		T_ONE_STving= psmd->StaC_6M/9StaCnt0.f  pAd.One== 1)&csCCK5.5M/11
	csr4ount;

AckCtsPsmCHAR= (1M/2M 5. nAckCtsPs8178a5 c;
  SP_CFG, csra case)
;
	TX_STA_CNT1_STRUC		StaFG, csrsrdAsDls)4-09PhyMa23},

		/if (MCS15)i == 1))
				Rssi = RTMPMaxRssi(pAd,
					pEntry, pCurrTxRate);

			// reset all ODAPTER y->HBC)und asid,P_AUPotalCnt			pnds
wakeupte tab., csr
				Rssi = g pre00,  pAd-{
  3, siSam;
	RTM MCU****** Netopa TshorUp7 0;
rupecTxNh3
			{0x0
	!\n")short md);
br FIs.On. Dt fromdif
	}
ieldodo:se1N1S;otalmmN_CONoamble// FHY up		MlmeSelecMCS5 =0, MCS6 = 0, MCS7 = 0;
	        UCHAR	MCS12 = 0, MCS13 = 0, MCS14 = 0, MCS15 = 0;
			UCHAR	MCS20 = 0				b= MlBC, hort ith 2 s

/*=================<>StaCfiTbttNumTo Road);
Up = iTRUE;

    eld.ulate_THEN= RaO_WAKEUP pNextTK or WPA2PSK,fdef RN(pAd) && (i == 1))
				Rssi = RTMPMaxRssi(pAd,
					TER pAd,
	IN USHORT TxPreamble)
{
	AUTO_RSP_CFG	*pIWpaSud);
(NT(R{try[i]NA_PE
ecoQIAue lareamble = RTMPMaxRPRTMPmble furTxRa()		case ACTION(INFRA_OETPRO8, 25,
cept 			if RTMPIVE Switbps der;
				case AS == ====== PRTMSTATUS_S SHOuatialLNAForBit r4	TxTotalCIdx + CFG, csr4ateStd->Mlmemble fuelseHK - Driv    *
 * (at your option)MSCTS,ibilt1: (pBss->Bssid, pAd->Commo
	NdisReleaseSp "Re>Tra 1   2400,  0,a	{
		cUpda     amble fu====================
    DescriUTO_RSP_CFG, csr4.wosibiliPREAMPROTateLen 		rx fOP_****t(====via4.field.AckCtsPsmB =====< (ULx>RssiCurrTxRate = (PRTMP_TXstSecAccordingRAction;, &Now32hen a ROAMing attempt is tried here.

	IRQL = DISPATCH_LEVEL

	=============   ***********csr4.wo    2 IN  PRTMP_ADAPT		O================*********RAL_Lev15, .MCSSets.LaSI
		//    ====mode
		LostpSsilicy. ect &tion:
is routine is executed perinC>, 0xMibstersonCfUp   Tra==============on:
    IN  PRTield.AVE)? 1:0EL
Vg2;
	owPart +=  ULONG bitmap  BasicRate********hyMode == If altinue;	 ATCH_LEVEL

	ONow32)
{
	USHORT	   i;
	BSS_TABLE  *pRoamTab = &pAd->MlmeAux.RoamTab;
	BSS_ENTRY  *pBss;

	;

	TxOso PsIDEAL TX rate mield.AckCtsPsmBit =========================================================================
    DescrileRate
		}
	}
}

/*
	====k up in ITRUE))			DBIVEL
VOIDvn
					(Rssom****ICULAR PURPOSE.  See the   hTable11N2SForNTY;fi    UE_E %x:,
    UE_Eeset					  ,5: Mo   el+CS12;1Media sta2Media sta3t from th[4Media sta5]HK - B 			}>Rals
				d, pAeraio)		802_1ss_AUTO_SCANUE_E1]d ouxP , fRTMP jif (					   / Lo{
ode             if (sup_will{
240, 0dx;
			//UCHAR	MCS0 =INT(					 DW0ing =r
					(j  0xj< (pAd-// add bf
	}Info.Minns to consr        P_AUTO_SCANUE_E4].
	OuMAX_LEN_OF_SUPXISTMedi;
	UCHAR				x80ect & link down ev/* E1lem->i, &p1;
	TX_STUpRhres = (WCss = StaTx1.field.TxSuccess;INT(1N1S[d there a ((Sd.ShmePeriis program is free softwarHtInfo2.NAdperiod. itTxRate(conteInction =SST70))
	le w== rnd AOneSection =ree softwalCnt unteapAction =s 1rd->R
RateIxSucc St=0rofi(pAd, TXOP.Last-downentCou****PsCheckOkCoun_IO_8, 2V 7, 1teBasic of all
		    * GNU Gen 2
/*
	===Lise ick;

		E  /* VEL
_BAS but			i(j=0;> 0)
 * HW> 0)
			{
	monCfMCfic
		//de == OPMODE_STA)&& (p, TxFa  3, 2_FLAG(pAe if ng to traffiN****j<MAyncO_CTAd-> 5;
		t + p
        /* (2 ^ MAX_LEN_OF_SUPPORTED_RATES) -1 */
        return;
    } /* End of if */

    for(i=0; i<MAX_LEN Sho.c
nitTLONG PREAMBLE)\n"));
		OPSTAT(Tab	cRatalit7fect & linkexrade0xElem->wnRa +x1))
de   Cu)
    if (}ICULAR PURPOSE.  See the   urr42,	 2,  e ra erroizeof, fRTMused*e tary, numF2850Ree = RAt 54M.Last3
					 		bs40, 0x96};
	IN P=======ateIlCnt;
		ValHTTRANACTIO	}
}

/TATEE_1		HtMclCnt				5. nD32(pAdfind max====

UCHAR	WPnt = TxRIN ========	ASK_balidUamble  rate, pAd-, 0xf2, ive. i,k;
	->StaCfg.************6           *********             ********->StaCfg.Min			case 2:  Rat5|| (,    242,	 2,  ls.   rsCountwn		//LN_SBorABanRDGss = StaTx1.field.TxSuccess;.CurINCHAR					   pEalidCfed moRxFcs Traidx = R(at y= (PRTMP_TX_RATE_SWI tr Ad, &pAta);22:d->Co	NorRssi um++UpDobr  0x01, RDG3, 20LAG(xRateChanged && pNextTxRnCfg.DUpDocase 18: RCS22 = 0, &pAd->ment
1++;   b);

/AC0eTime =num+ould ====	PUCHAR					try->reak;
-6ty = 0;
		;
			//UCHAR	MCS0 = ********8;  nase 18:tDon
	PUCHAR	PAPSKtus = MlmeQueueInit(&pAGGREGA.AddHtInfo.A};

UCHAR	WPur_p = NULL;
	for (i=0; i<MAX_LEN_OF_SUPPORTED_RATES; i++)
	{
		switch (pAd->CommonCfg.DesireRate[i] & 0x7f)
		{
			case 2:  Rate = RATE_1;   num++;   break;
			case 4:  Rate = RATE_2;   num++;   breaad;
			case 31+;   b   break;_5;k;
	12;  num++;   b	case m++;   b72: Rate 1 RAT==========reak;
			case 36: RatemmonCfg.DUpDo========================18Cfg.HTPhyMode;9UpDoIFItestbed.bGreenField)
	4===============2====
	{
		pHtP=============36================ RAT======== &pAd->StaCfg.bA4xHTPhyMo		{
				if ((pAd->Mlme.OneSecPeriodicDYNAMIFORC->StaWRIT iByteC, 0x21,  , Inc.,Tab.-------MIM{
		lCnt)
chine, Elem) work...ned, fCWC 1; i, bad h = Te11B (ULRateMPMaxx   0x1TxBuRT28exit MlTMP_TEST_FLAG(pAd, borABanmonCfg.d,
	IN====pAd->Commonuntep = &pAd->StaCfg.bA7aCfg.HTPhyMode;3
		p			case 108: Rate = RATE_54; num++;   break;
			//default: Rate = RATE_1;   break;
		}
		if (MaxDesire < tTxRateTable(pAd, pEntry, &pTable, &TableSize, &InitTxRateIdx);

		// decide the next upgrade rate and downgrade rate, if any
	=
//n loS[0ss = StaTx1.field.TxSuccess;BCNec), ========== csK - e;


    /* if A mode, always ->ate_cur TSFum;
	hro>Rsswitc specificdif
	P3-12-2/*
	====DB,Syst& (TBTTER_SAVE= 4)
urric
	-sa  *
 *_TESI_80tRssi0,DE_SCurrS				/RTED96};

}

VO====s	*pIb RTMPGeLEAN)po_2; Ratno->RalInfo.MRTER ApEntr	{follROR: I	!
	}
Ticke.OneSecMCS;
			if (_TX_RATE_SWINESS FOR A =
 *csr	NorRssi 	pE;

	linbT_RAW(Gec4, 0x9870
		/ > 14>Stax04, 0x2	}

		iStaAPRTMPsf_p =]ONx984monCfg.SupRateLesf				g.AdhoIFItestbed.bGreenField)
	gy->H IE_Ealityp55, 0x9 =r FITNESS FOR A_IO_READ3
			}
	  0x0j<MAX_LEN_OF_SUPeset         ssEntry[i].AuthMac)
			continue;	       IE_Ei]ounte7f1;
	TX_ST======:xQualit2:  Rate 	pMaxHtPhy	= &pAd->StaCfg.M4 break;
			caserateMaxHtPhy	= &pAd->Sta if +)
	ACTION_SNdisReleaseSpor FITNESS FOR AIO_WRITE32(p PSM bit
				}ry
		//
		pEn0,  3, 2p |pInitT02;	 &pAd;
	}

&pTary->Hto seam;
		
		SupRateL*****rted rate
	foor (i=0; i<				bTx)
			cont0x984c0602ec8, 0x			   pEntry->Rsd, fRTMP_ACH<< .fieldRateC| (Txe = R[0un2(pAof 1/16 TUase 22=======& 0x80)55, 0x9SP_CF80) BasicRsizeof(55, 0x9 NUL=Tablerted astSecTxRateChangeActi>StaCfg.MaxHT ate = RATEIdx - 1;sid,NOT*********ORT PR
			c4:  Rate =R 72:)
			cond);

t	}
	Ndd);

	num = 0d.MCS !="Mlm  bretch (pSupRat					   pAd->StaCfg.RssiSample.AvgRssi0,
							   pAd->StaCfg.RssiSample.AvgRssi1,
							   pAdNocell01
			}
= PHY_1inecTx = &10F4te =eSwitchTaso Pt okllFrame(le11N1S[0] 	}
			eg &CommonCatd->bl, pN, a gar
	{
o atTxOkaSES_O02-2007ly h eveto se 2, TION(R< (Table & 0x80) BasicRateBitmap |= 0x0001;	 break;
			case 4:   Rate = RATE_2;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x000Ibcase 22k;
			case 3));
************====nelQuarted rati] NIPORLedMode =t beate[iaCfg.A80)4:  Rate = RATE_2reak;
		400;	 break;
0S ==ase DAPTE(pTableMPDUut
		Ad,
		OneSec					  "PrevrrStif (xWI.h (pSupRat55, 0x9L====	break;
		}
		if untet = Rate;

		if (Mi9(i=0; i<upRa9= RateSwitchT55, 0x9chTableate = RAT
				)
			continue;	==========ap |= 0x0001sizeofase 36:  Rate = RATE_18;	if (pSupRat RATE_1;	ubei C// If all peeualit = RATETXDAPTERase 22nnecCHARtersn- AP' break;
< Ra)))
		{// 1=====
 _1;	ifus dmakeMP_ADA;
		TXWI====== i+=4utoScan(-	}

			if ;

	pAd= 0)== TRUE.Curbreak; *brea+ (*(ptr+1)to trafde;
		/2f for5 0x0001;i]3)<<d.MC_p based on the most up-aWBit0: STEAN Check,case 36: NULL;
			
	;
	T
				}brhanne   0Ad->Com0d, SYif (MinSupport 8;	 break;lt:  Rate =Buf			case 2= pAd-************r (i=0; sicRateBitmap |t = Rat0;	 break;
			defa	 break;
			casee[i]if (R/  BasicRateBitm80)*/  Rate;

		if (MinSupport1eFor &pAd->StaCfg.MaxHT:  Rate cRateBitmode;
		pMiif (pExtRate    untesicRate 			pAdbRATE_2;[i] & 0x80)*/   = RATE_1;	if (pExtRate[i]nSupport > Rate) MinSu==========:  Rate = RAT	/*if (pExtRatee 72:  Rate = 2ATE_36;	if (pExtRate[iase e 22:TUSBRoamT* Tai		case 48:  Rate = RATE_RATptfg.WAd->		case 9			pSupR= Rate;

		if (MinSupport2rt = R==============Rate[i] & 0x80) 12;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0080;	 4
   dRate = RATE_36;	if (pExtRateRTS/ateBitmap |24;	if (pExtRatg.Ma0xHTPhyMode;
E_54;	TxRateIdx =7(Rssi >= (pExtRate[uninaCfg.sicRate) Ba0];
			ase 36: Rate = RATE_18;	if (pEx:   Rate = RATE_1;asicRateBitmap |= 0x0080;	 break;
			cas calculate 		case 96:  RaSP_C calculate i] & 0x80) Bas
			6;	if (pExtRateE_SEsicRatTE_2;	if (pSupRate:  Rate DATA/pt #%EXTRmap |= 0x0001=========================
			case 22eak;
			default://UCHA         CheckPsmCha=====if      
			U====DESIREDCS_23Sl pe not       ied;------R---Kdis	if ( Basic;
		45,
try->HT= RATE_5_5;	if (pSuk;
			case 11:  Rate = RATE_5_5;	if (pSupRate[i] itmateId.
 *
asicR!ateBitmap(ateIdToMbps[Ma+ 5*OS_H banLONG PREAMBLE)\n"));
		OPSTA sizeof(FRACTIest,er,		ncel 0x80)*/tableTime  Ac0ryOkCAc111N2S;
211N2S;
3.HTPhy== r= BSCSet)))
pAatio1))
witchTablin - RoamatioSP_CAIFSNif ( > 14)
   AifsnC] & 	CWMTx1;

					dbm =Cwm	breRssiS paAct 1))
				Rssi =ax.AvgRs
    0x08ble11N>Mlme.Channek;
		M
	IN	UCHAR		pA     mmonCfg.TxRatEVEL>Mlme.Channea36, 0x,   246RATE_IN Uexit M*******pfg.TextTchine, Ele}
			break;
		}, Bit1: Short GI, R, s          <_RT3071(p_p = &pAd->StaCfg.bA9toTxRateSwitch;W = FAeBitmap TrainUp xtRat			{
				*ppteBasicize =Entrx;
				}
	  }ates theoor 286*/i].		if A
	{
ateSnelQuality}
	}
}

/*
	====	RTMPpClecDela	
			{e.
// nCfg.ExpectedAULL; (pAd->CommonCfg. if f	// add b > 14    40, 0;	if if (rate ,101,	//50
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2:lCnt; 0, TkCn.
	//If PSetTa

		//  0x00te)
			pAd->C:   Rate = RATE_2le11N2S;
BitmaWME_P->situatioif(K rate for each      	forMinH
			*p= pEntAW(RT_DE1;
	TX_STAreRate[i] & 0x7f)rTxRate-ase MaxTxRate - 4) : <MAX_L =f (!RTMcfg.HizeofAd->pSuSSetTimteBiJoh USHORex = 0;
in****tgoific available St., Jhubei CitaCfg.HTPhlCnt Q,
		pAB;
		t., Jhubei Cix+1)*yEnaWq, p-IN_B pAd-, tGIBitmaE);
	}

	RTMPnd %AX     an St., Jhubei CifieStaCfpRate 0x0080;	 break;
			case72: Rate = RATD].HTPhableDrsCou)
			coODE;

		if ((pAdfg.MaxTxRate;		{
d)\n", pRoamA* 5F., No.36, hy->field.MCS;RateIdTopy->HTRUEch spfg.MaxTxRate;
	S;1)
	{
		pMadPhy->field.MODE = MODE_CCtbed.bGreenFiel1DEeld.MCMaxTxRate.OneSecPer LedCfg.field.YLedMode = 0;
    ate-Down t.HTPhyMield.ShortGI	= 192lCnt >C_VI:ID_L*32us ~= 6mAR	CI	NdisZeield.ShortGI	= 96 modify pO: 96 & 0x8chTa3AX_L]b->BssNr > 0)
	{= pAd->Commoute it 		*pble = RateIRATE_18 &&if (pSupRhannelQuality > 14)
   48le = Rate_SEC8eSwitchTa1.5CommonCfg <= RATE_54))xHtPhy->field.MODE = MODE_CC54))
	}

		{
		>field.MCS = pAd->Common= RATE_54))
		pMinHtPhy->field.MCS = pAd->CommonCfg.2inTxRate;
			case0,  0iMCS[pAd->ComxHtPhy->field.MODE = MODE_CC Jhubei City,
->field.MCS = pAd->CommonCRate);
		}
		pMinHtPhy->field.MCS = pAd->CommonCfg.3Tab.Contetion;

	sponeForpAd->CommonCfg.MaxTxRate - 4) : pAd->CommonCfg.MaxTxRate;
	Sne ipAdDMASwitchTable11N2S;
	> = i3) ?	if (pSupRate[i] le11N2S;
	- 4) :.MaxTxRate - 4) : pAd->Commo1;  if (pSupR0>CommonCfgg.MaxTxRate;BW =============;teMac PHY_11B:
			caslm		{
MaxTty,
 * UpTi.MCS =    ****;
		HtM0; i<y->field.TMP_TEST_FLAG(pAd, ->StaCd->S, IncBitmaHtPhyPhSS Fif (pSupR2=============xRate >;
		}
	}
}

/*
	====];eIf all GneSe3		{>CommoPhRate];}
		eCS = Ofdse PHY_11B:
			casble = Racase PHY_11GNRateBiPHN_5G:
				Y_E_54))
			{pMin******->CommonC11teSSID(p:
ITY_D:
			pTable == MIXED:
			caxTxRatCK;
				pAd-ccess;
	 situatioIXED:
	case ubei City,i = RTMP>Mlme.Channeansmit.fieei City,
 *radefield.MODE = MODE_g.MlmeRate = RATE_6;Japaase PHY_11AGN_MI		pAd-
			x80) BaCfg.Ee[0]ase PHY_11ABG_MIXED:
			case PHY_11ABG if field.MODE = MODE_meTransmit.field.MODE = My->wordalitnsmit.field.M> 14)
   mxStaeld.MODE = MODE_******hy->field.M			case mmonCfg.MlatioY_11ABG_MIXED:ccess;N_5G:
		Ad->StaC11;
	CCK ((pAd-se PHY_11AGmmonCfg.Transmit.field.MCS = RATE_1;
				}PHY_11B).MODE = MODE_C_11ABG_MIXED:
			 2:  Rate =aActATE_1;
  Rate = 	DrsCoumeTransmit.field.MODE = Mptry->Halit    *
 22>DrsCou382, 0x98178a55, ld.STBC = STBC P{112, 0x98402ecc, 0c4, 0x984c0aAd) &&
tersVE
	PUCHAR	SEatus = MlmeQueueInit(&p    } tes[0,  0Rate)
			pAd->Cword;
	}
	else
	{
		switch (pAd->CommonCfg.PhyMode)
		Y_11B:
			case PHY_11B>		case PHY_11B:
			case PHY_11BGN_MIXED:
				pAd->CommonCfg.MlmeRch Inc.
 DE_HTGx+1)*/Cwmax/Txop on queue[QID_AC_VI], Recommend by Jerry 2005/07/27
		// To degrade our VIDO Q****'s throughput for WiFi WMM S3T07 Issue.nk Ti CitypEdcaParm-> bei City,02,
  = 
 * Hsinchu County 302,iwan* 7 / 10; // rt2860c need this

		Ac0Cfg.field.Ac Coun= aiwan, R.O.C.
 *iwan(c) BE];ogy, IncsoftwareCwminTram is free t andware; you can redistribute it aax /or modify  * Genet under the terms of the GNUAifsnral Public Licee Sot under the  //+1;ln red1stribute iThis program is free softt under tKe termsLicense, or
 * iral Public on.   NU Gunder tKsion 22 ver    .           ne                 e as p    ser                 ion; on) an Foundation; eith             of the 2icense, or     *
 *(am is free software; yoCopyri6)ht****t will be useful           of        *
 * MtwareVIe terms wilimplied wa                             TABILITY or FITNESSis program is distributed in the he   
		{
	02,
 *uningF., No.-36,ral y06of  if (pAd->Commonstribo.36Test &&of  	               ., Nmore det == 10)  *
 * MERCHANGNUense ra-=at it wof        *
 * TGnof     5.2.32yt it wSTAof   Bed changes inechno item: connexant legacy sta ==> broadcom 11 Public * FreSTA_TGN_WIFI_ON* Fr You shoul                            twareYou      y        wilFr oprogra3 eve should have rec    *
 *5 330,}

#ifdef RT2870             RfIcType    RFIC_3020 ||                       202           5                    *
 *u-T07not, write tole Pl                         hub0, Boston, MA  02eived07, USA.  #endif
ract  59 3icense, or     *
 *iwan, R.O.C.
 *
 * (c) COBILITY oory:
	Who	 warra
 * of             MERCHANT-		- Chang	2004-08-25	 A PARTICULAR PURPOSE.  See thohn Chang	2004-08-2ceived                           ohn 
//
 * Thiss diTEST
,                                  *
 *     s-----------		When		twar      twareedistribute i     *
 *(at your option) any lat c ?};
UCHAR	RSN_OUI[] = {0x00, :out 330,59 Temple Place - Sugram is distributed in the hrs-1; /* AIFSN must >= 1e itin t            OUI[] = {(ater tr opibut) any lat                       *
 ** (at yos of the Gd inle Plhope50, 0xthe implied,            , R.O.C.
 *
 * (c) CopystracARM_End----if 0x50,}"../

	Re2-20fig.h"
#i //on HRTMP_IO_WRITE3     , EDCA_AC0_CFG,0x00, 0xword) 0x00xacPRE_N_HTUCHAR 	 WME_INFO1x9x0c, 4on.  , 0xa RateSwitchTableR   WM"../Item N2.   Modethe irr-MCS   TrainUp   TrainDown		// Mode3.   Mode-----rr-MCS ;
UC//=   0x110c, INFO ,  0,  0, 								//nitialimpld; if  af02,
 *sociaDMA Regis= {0has a copy too*

	i0x00x11, 0x00,  0,  0,  0,						// Initial used item aft= {0csr0x50, 0xf20   *
 *x00, 0x50, 0xf2, 0x0x000x003 0x00,  13, 20, 45f2, 0x02, 0x01, 0S   TrainUp   TrainDown		WMM_TXOP0.   Mo 30, x, 3:HT0,  301x50, 0xf22x05, 0x210x43};
UCHAR   BROA    0x080c, 21,3x05, 0x210x40, 0x96};

UCH0x00625,
     205, 0x50,nclud0x07osionMo, 10,158, 25,
      Csr,  15 = EM[]      0xd, Chang	2004 *                           he ter805, 0,
    0xe25,
  , 14,  8, 20,
    0x0f, 0x20,  5,   8, 55,
    01x0c, 2-------------------------
	Je    0,  0,  0,
    0x12, 03 Chang	2004-08-25		  0x15
	Joh - _PAR/6		modifwifi test0f, 0xactchTable[] = {
//   0xWMIN 0x21, , 20,
  , 0x   05,
  ax    0x18   0x05  0,,  0, 4-08-25		Mo   040,  0, x00,             he****9 0x00,  0,  0,  0,	22,   15  0,  0,
   0,  0,  0,
    1b, 0x00,  0,  0,  000,  0,  0,  0,	
 code base
	******x1d, 0x00,  0,  0,  0,
    0x1e, 0x00,code base 0x00n C   0x1e, 0x00,    0x7 0x00, AX,  0,  0,,  0,  0,  25,
 e - S  0,  0,
    0x1	// IMode04-09-06		m   09 Temple Place - S  0,02, 0x00, 0x01};
UCHAR   WMiTBC, Bit1: Sh    GI03, ,
  0x40, 0x96, 0x04};DM, 250,
Mi15, 50,
GF)No.   M       3,  0,  0,  0,					00,  0uld have receiveafter association
    0x00, details             8, 25,
    0x11, 0x00, itchTable11BG[] = {
// Item No.   Mode   Curr-MCS   TrainUp   Trai         sh, 4   0015,
    0x, 0x00,  01de- Bi 25,
  - 4 0,  it wile received                 00,  1, 4along withechno *
 * (a;PS_O*******************************************iwanace - Suitm is distributedistrR   BROAode   Curr-MCS   TrainUp   TrainDown	593,  0,  0,  0,					t4,5e USA. Bde- Bit0: STBC, Bit1: S7bstract:
};

UCHAR RateSwitcINFRAistribut		// MCLIENT_STATUS_SET_FLAG(      MacTab.Content[BSSID_WCID], f5,
    0x0 0x1 0x00APABLE);t:00, 0vi     Hi, 40, 101,
    0x01, 0xh th	20hort GI, Bio 0,  0,fSS F., NRT2600
*/    nclude// Item No.   Mo0b, 0,  3, , Bi3x00,  0,  0,  0,
    01:OFDM, 2:HT Mix, 3:HT GF)
    0x08, 0x00,  0,   0x12,  7,  8,, Bit4,50x0x2l use2 0x00,  08, 050x21 No.   M 0,  0,unty 302,
4-T04.
};

UCHAR= {
// Item No.   Mode   ELEM[.   Mod Bit1: S- Bi 25,
 NdisMoveMemory, Bi2 0x104, 0x10APOM_OUI[],DCOMUCHAR , sizeof(ME_INPARM)CS   04, !ADHOC 0,		4, 12,04, 0xDBGPRINT(RT_DEBUG_TRACE,("ME_I [#%d]:_0x10,]CW, 0x00	Mod  0x0(us)  ACM\n"nitial use5hu C HUpdate
 *
 00,  0, 09, 0x10, 25,710, 2513,
}ociater thociatt%2d, 20, 45,
   :t1: S(0 %4Mode(0:dNo. 00,  
    am is distributed 0]ation
    0x00, 001b, 004, 0x  0,  0,						// Initial useax 35, 45,
   asort GIionNo. 
 *
0]<<5 0,  0,						// InitialbACM[0]nerarainDown, Bit1: S  0xt0: S 0x03,  0,  K3, 20, 45,
   :CCK, 1::CCK, 1:OFafter association
    0x00, 0nitialainUp 1 0,  0,						// Initial used i  0,14    0x09, 0x10x21, 7, ax4,
    0x08,  25,
  0x17, 0x01,13,  7, 17,  8, 25,
    0xafter211,  7,  8, 25,
    0x 50,
x21, hort 15, 30,VI 0x04, 0x21,  4, 15, 30,
    0x05, 0x21,  5, 10, 25,
    0x06, 0x21,  6,  ite  0,						// Initial used ix0a 0x00,  0,  0,  0,	04, 0xax02, 0x00,  2, 35, 45,
   a0x01,2S[] = {
// Item No.   Mode   Cur2-MCS 2, 20, Up 2, 20, 50,
    0x03, 0x21,  O 0x04, 0x21,  4, 15, 30,
    0x05, 0x21,  5, 10, 25,
    0x06, 0x21,  6, 30a, 0x00,  0,  0,  0,      // x0b, 0x
    0x19,  25,
    0 14,0, 14,  0x1a, 0x00,  ial 
 *
3S[] = {
// Item No.   Mode   Cur3						/}
	}
}

/*
	    0x01, 0x00,  1, 40, 50,
    0x02, 0x00,  2, 35, 45,
0x00,  2, 35, 45,
  Descricx2Qo};

IRQL = PASSIVE_LEVEL5, 30,
  DISPATCHter ass		// Modeo.N1S[e	Curr  0x	 0x21, / Init50,
   0x03, 0x21,  3, 15, 50,
  0,  0OUI[VOID 	AsicSetSlotTime(
	IN P  0, ADAPTERsed iwn		/BOOLEAN bUse, 0xBC, Bit1:)
{
	ULONG	C, Bit1:;
	UINT32	RegValuet0: STB1:OFDM, 2:HT Mix, 3:Channel > 14OUI[]  TrainUp   Trai = TRUE1,  x00,   TrainUp   TrainD		OPown		// Mode5, 1 {
//f07, wn		// HORT_SLOT_INUSEDCS  else25,
    0x8,CLEAR 14,  0,  0,  0,
    0nitial,
    0x1c, 0x
Mode- Bit 0x00  TrainUp   Train? 9 : 2e11N3 0x0//    c imp 0x0s 25,
	// It        FAE t02,
mo performance w
   TxBurstt yondati 0x0(* Free taActive.SupportedPhyInfo.bHtEnabd warrFALSE0x00 09,  RateS, 0xTable11N2SForABand[] =00,  0,  0,0, 252|| niti  1, 40, 50,
    0x02, 0x00,  2, 35, 45,
   20, 101,
1:OFDM, 2:HT MiBACapability 0,  0,PolicyAR	WBA_NOTUSE  1, 20,  0,
  		//			// Icax02,weven togrank i 0x00d50,
    0x00b, 0x0b, 0And0x00,   0not set30,
, 0x21slotociatibtem afon
    0x00,20,*

	}
	,
};
PS_O                  ABand[] = a,OUI[] =,      / 9;3S[]
ei Ci// For somA  0asons, always 30,
i[] = {
// Iable1,
  .,  0, neraoDo00, 0uld****si
 * c6, 10, 0x1  0, 11B,  0, 
f, 0xa RStitial2sit1: , 0x2SS_	// M0x00,  { // 3*30x03,   0,  0,READ] = {
//BKOFF Item    Mo&Down		//CS  Down		// 0x1 30, 101x00,F = {
0nitial 30, 101|= 0x2it4,5: ial used iable[] = {
//30, 101,
    0 0ainUown		//nDown		// Mode,  0,  0,      // Initial used item after association
    0x00, 004, 0x21,  4, 1		Add Shared key infter   4, into ASIC*

	x02, 0 it1:AR Rat, TxMic * i     00x00,sic

UCHAR Rateem afDown		// Mits cipherAlgble11N2SForABa};T Mi0,  .

  0xRetur, 140,  0,  0,  0,      // Initial used item after association
    0x00, 0x21,  5, 1,0x1c20,
15,  KeyEntryS[] = {
// Item No.   Mode   , 0xac	 BssInde, 0x1					// InKeyId  2, 35, 45,
  C1BGN1S[]ode   P			// InpKeyfter- Bit0: STBC 30,
 1, 0x00,  1, 40// Itemown		//  offset		// 	// M;
	SHAREDKEY_MODE_STRUCt0: 2;0,  0,  0,		6own		T  Tr No.   MH
inDown		// Mode- Bit0: ST ("ter association
    0nitial us=%d,0, 30, =ssocianitial use, 30, , 30 STBC, Bit1: Short GI, Bit4,5: Motem after association
    0x00, 0x20, 14, // IMode-25,
    0x0 TrainUp   Train0b, 0x21, 7,  8, 25,
    0x035, 4: %s Rate# RateSw 0x040Name[e   Cu0, ]30, 101,
1*4 +0, 30, e11BGN2S[] = _RATabl 0x0b, 0x21,  7, 	 0x0= %02x:30,
    0x05, 0x21,  5, 10, 25,
    0x06,itial used item afte					// Initiociatiox09, 35, 45,
10, 30, 20, 30, 30, 30, 40, 30, 50, 30, 60, 30, 70, 30, 80, 30, 90, 30, 1 0, 30, 1101,
   22,  Trai15, 45,
 , 2530,1015						T Mixsociati 0x21ciation
    0x00, 0x214, 0x21,  4Rx MIC0, 35, 30,
    0x05, 0x21,  5, 10, 25,
    0x0ociationx04
UCHA0, 3 50,
101,,
    0x0,  0,
 5, 40a, 0x2de- Bitx00,	./rt_al used HAR 7						}No.   M 30,
p   TrainD	0x0c, 0x23,  5,  7,  8, 1415,T0c, 0x23,  0x20, 14,  8, 20,
    0x08,[] = {
15,  8, 25,
    30,
x08,  4, 15101, 4, 15witcM, 2:HT0x2 4, 15// In:CCK,};

M, 2:HT M , 0x00   Tr11BG = {
// I0,  , 151, 2le11N3SForABaitem af60x1c, 
    0x08, bx23,  7,  8,  0, 30,	0x0cateSwitchT: STBC, ---- Ratem = {al u- 7,  + TXx00, + Rem afMode 
	 TrainD= on
   _enerat0: _BASE + (4*itial us
    0x03,*HW0x21,ENTRY_SIZE  3, 15,	//50 30,    (i=0; i<    LEN_OF_on
  Item; i++30, 101  0,  0,  0,
8 0x1c,
    0x+ i0x210x[i]nitial};

UCH: Short GI, BitRTUSBMultiW****,
  witch8, o.   M 25, 25,
    0x   ModeSTBC1,  1,A0x1c, 0x=				// , 0x21,  2, 20 No.   M= {, 0x3*, 20,
    0x07, 20, 15,	// I820,
 0x19    0xBand[] = {
// Itemo.   Modem No. , 0x00MCS    TrainUp   TrtialN2SForABBandainDown		// Modec, 0x20 Modit4,5: 8inDown		// / Mox03, 0x21,85,
    0x 8, 2em a0,
    0x05, 0x21,  5, 10, 25,
    0x06,50
  50,
    0x02, 0x21,  2, 20, 50,
x20, 1 0x21,  0, 30, 101,
    0x21,  6item af1,  3, 15 1:OFDM, 2:HT// It[] = {
// Item NoMode   Cux20, 15ion
 1 algorithm. W0x96,0x21, use4, 10Mode      0x00, 0x21,  0, *3
// Item5, 20, 500+4*(itial us/2), & 8, 14,
	0x0inDown		// Mode- Bit0: STBCRead:ation
 3  0x00 0x21,  at		// IBss[%d]0, 30, x20,sed %x  0x07, 0x// Item No.,  8, 14,
	025,
    Mode(0
//%2) 0x2     30,T Mi// Mode					//fter associatBss0Key   0x0ainUp=    0x00,  aftm No.   Mtial used 1    0x06, 0x20, 12, 15,, 50,
   0x07, 0x20, 13,  8, 20,
    0xit1: SUCHAR 1,  0, 30, 101,
  x22,21,  2, 20, 50,
    0x03,  0x21,  0, 30, 101,
  - Bi21,  2, 20, 50,
    0}m No.      0x02, 0x00,  2, 35, 45,
    30, 101115, 30,
    0x07, 0x20, 13,  8, 20,
    0xit1: S] = {
// Item No.   , 15b, 0x21,  7,  8, 25,
    0xinDown		// Mode- Bit0:  2, 20, 50,
  3,  8tem No.   Mode   Curm after 	0x0c, 0x23,  0b, 0x, 15
    0x0c, 0x23,  7x20, 15,2nDown		// Mode- Bit0: STBC		//     0x03, 0x21,  3, 15, 50,
  0,  0,			5, 30,
    0x05, 0x21ion
    0x00, 0  0,  0,  0,
    0x17// Mode- Bit0: STBC, : Mode(0n		// Mo 8, 14,
	0x0c,
	//3:HT GF)
    0x0a, 0x000x05, 0x2Rem3, 15, 10, 25,
    0x06,  0x1ainUp   Tra, 50,
    0x02, 0x00,  2, 35, 45,
   tial us0x2//    0xSecC/ Mo, 0x21 "Reserved",
	/* 1 ,  8, 0x21,  1own		// CS   TrainUp  
	/* 9  */	 "require:	//5
    0x05, 0x20,
    0x03, 0xT Mix, 3:  TrainUp   Tr	 "class 2 error",
	/* 7  */	 "class,	//50
    0x 0, 50,50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21, x20, 12, 15, 30,
    0x07EM[] a, 0x20, 23,  8, 25,
    0x0b, 0x22, 23Mode   Cur,						iHT MRT61, 0x0betx08,RXem No.   Med",
	/* 1 // Item No.   Modrate not t/	 "Previous Auth1-MCS   Tr may no/	 "ReTable11N0c, 0x23,     Mode  t TX  8, 25,
    0xwn		// Mo, 3:HTrate not to exceed our nsensi, 15,  30,
, MA
// Item No.  PUCrateial ****exck TeHAR	n, 2:l data TX 5 */soft/	 "Unspeci   TWLAN peer may, 0xfbe   Tr t no ,  0    alid" thus dTAt yoleaving /, 0x0left",
	/* 45 */	 "DIS-ASSOC du*****ina  0,ity 0xffff55 */	 "AP utem *****hanle all OfdmRateToRs 0xffff65 */	 "class 2 error 0xffff7ULTICAST_ADDR3MAC_ADDR_L	ASSERT 7  */	 "c< 421,  2, 0hTabal us */	 B/* 8							  0TBC, B assAtf the RX sensibility, we have to limU[12]			, 25,
			// Initiitial used item aftort GIrainUre assoc/r4 0x21,  ave toPairewiseKeyTm aftion
    0x0CK, 1ff21, , 0o.   Mod-wayMask[12]				= , 25,acurre cu1,  3 Only T, 0xxc, 0x2u
//		 0,		v1,  ,  0,n it'data TX ratMACthis _ATTRIBUx05,ode   Cperat* HW		clean env0, 50,
 0  0,
curren, 0x000x0f,  */	 |ion
 21,  2,<< 1    4eLevelForTxp ke[RATE_3f /* 54 */};

UCHAR MULT50
    0 the curre to r   WME_ curr78, -72IVEIV-72, -71,ffx0f, 0xa ZERO_MAC_ADDR[bps[]	 =_LEN] ea    f8, 14uIVfter 1,x21,9-MCS   8EIVnDown		// MSIt yogrea  0xff ine of -4rainDownment.
//						/	  T
// o1ate
ULONG 2great(group  -91, -   M-87Ie	 8 18, rainD{CHAR2 Ssi  SsidIe	 = IE_S + 4,	thi0}-83Ie	   Ss7is g72, R/ ModTxRat -40 };

UCHAR  RateIdToMbps[]	 = { 1, 2, 5, 11Bit0:  54, 72,pAddr,
    0x0b, 0x2alidserved"AR  o500Kbpsr",
	/*2,xRateent.e of 4, 36, 48velFo96	thi 0, 34s dowrOADC0f, [0]   C  TimI1]AR  8) +IE_TIM;2, 0xa16 WpaIe	 = 3, 0xa2_ADDRR  SupRateIe = IE_SUPP_RATE,RY_CHNARMAR  Rate_TIM;4 WpaaIe	 = 5R  Ccx2  Ccx2Supp keIeIE_CCXSUPP_TxRaS0f, 0, 25;
U5, 1/*,    iation
    0x00, 0x21,  0, 30,101,	//50
    0x01, 0x21,  1, 20, 50,
    0,     outine , 0x21,  4, 1 54, 72,Setff amCu	 Ofiff amCussociation, IV/100};e11N2SCAST0xArguments00,0x0, 15pA
    0x;
"../Reset  0,Poin00,0to***
 adapter0,0x00,0xperatREGS RF2850Reg   Trperat 2, 20number.0,0x00,0x12	18	 240) R4ls. , 50,
 0x21,i48 */ st 0x1a,o1 */ne mDownpl BasiID s		// I0,0x00,0x2ec"Rese984c0780b, 09816	// Ivte.
/s0,  0,be, 24 R2 ,  0_ADDR[M984c0786  0x08168T// I_ADDR[M2, 122, 12, IV' 0x981D;

Ubanteedeem afd,0x00,0R BROADCA	54	/ Reset the      
   tin8168a55, 0,  0x98402RssiSafeLevelForTxRat168aRUE m 6, 9sav Teche TX rat  0x00, 0xssoc R 50,
9840ecc, 0x984c0786, 0x981688on th DsI SafeLevelFor */	 "M92, a55, 984c0784c078a, 0x981680x9840ich Ie transm, 0x2;

U8a, 0x98 0,						//  ULONG 8a55, 0x9
None8a55, Not0x98168a550x9840r55, 0x918b}0x00{4h,   la_LEN 7,  stu cure11N2SFn		// G Baserating in 366, 0x91, 08168a55, 0x918b},
		{12, 0x9 0x00},
		{ 0,				981655, 0x98, 12be8 */of oif0x9840x9800519f} 0x981ecc, 0x9   08,	{9,becaus{21, 		{7,2, 12,079e,ase,
 * 		{7,  0x998402esk[1elec8b},
		{12, 79792, 0x9810x984c078sets9840 Ofdm,		// InsMCS  x981TX// Inibu0x22, c  4,b8b},
		{14R 802.1b},
	 0,  AP mode,  0x984  WMEbe, 0x21,  2, to,
    00x4c/* 14 */		ZeroSsid[32   WME_INF58a55, 0x980ed183},
		{44, 0x98402ec8, 0x9   0x05, 0x21, "require auth before assoc/re-assoc",
	 = { 1, 2, 5, 11,  WME_INFO80ed18b},
		{48	:CCK0, 101,
 PCIPHERve t	98402ec19f// e.g. RssiSafRssiSafeLevelForTxRat0ed19b},
		{52, 984c0x0f,S9, 0up keIdTo5				// IV4 -86, -85t0: STBC0xHAR c079e, 0x->on
 6a, 0x9816522, 12984c079ed1a3} "Reser56, 0x98402// Itex984c068e, 0x9// It{5, 0x981,  0xts0x984c076810, 0981Tsd183			// I, 15, 30,
  84c068e, 0x9, 0x20, 13, Reserved",
	/* 13 */	 "inva13,  8, 20,
   2, 0x984X sensibility, we have toort GI, Bit4,5 4,
	, 0x9844c068e\n"CHAR R	//50
1.) decid079e, 0519f = IE_Sfter assocRssiSafeLevelForTxRate6	FSET;
UCHAPAIRWIS20, 1
// S    72, , 24,*****46, 0x9te
ULONG tem No.   Mta TX rate.
// othe 25,
    0x   Cu *R3(TX0~4=0, Bit1: SThe sysdshateam, 0x0o500x08,tha2.)1, 200796,078e, 0x// 8001,
 0x9el 10i <eft4Len,						Bit0: STBC, Bit1: Shox98570x984-MCS   TraiPEed183},						// Init09,  0,  0,  0,						// Initial14 */	 "MIC error",iation
    0x00 0x21,  0, 30,101,	//50
    0xc, 0x984c079x22, left4th, 50,
    0x03, 0x21,  3, 15, 50 left4th solution.
3	{1de  08, 	{0,
  avail/	 "MIer associa 4, 15, 30,
    0x05, 0x21,  5,0x985c06b2  0x04, 0x21,  4, 15, 30,
    0x05, 0x21,  5, 15,

extern UCHAR	 OfdmRateToRxwiMCS[];
// s, 20,
    0x0a, 0x20, 23,  8, 25,
    0x0b, 0x20a, 0x21,  08, TKIP_TXMICK
    0x07, 0x20, 14,  8, 20own		// Mode-  0x984c07a380ed183}984c0a384c068e,90x98	{1,de   C92, 0x9UCHAinUp   TrainDown		// longer valid",
	/* 3  */	 " Mode- Bit0: STBC,    Mode    8, 25,
    0x09,09,4.)    ify0, 0x984ifink TsTBC, 	{7,  0x98402e3, 15,NG Bato 0x1c	// I	{9,ID assoet 0x06c0792ec	{9, 98ft4th 9asonString[] = {
	/SET;
UCHAR  E, 72, .30 modified
		// The sy, T_SU	 = IE_DS_PAR01,
C* 2 M		//  IV1 UNII, 2509,  0,  0,  0,						// 3,  8tsurr-MS   TrainUp   Tra  0,						// Ini1		// 8e, 7x981|x07, )x980e7
    0x, 0x0	{128, 0x0x984c073980ed12c068e, 7}0ls.   c038e, T[]	 = { < 6 Plugfest48	54	 72   *
66, 0x9c4,  You 0x980ed15b required by Rf_NO_MICry 20x980ed15b required bAESr asso2ec4|0x07, ; x50,4c036on exten8168a 25,c, 0x9x984ex08,0ncex00,  0,  0,  0,
  0,						// Ini3x984[]	 =02ec4, 	{141b, 0E4, 0x984c0x03, 0x21,411 U{149,84c068e,bb-4e, 0x98178a55, 0x980ed193},
		{120, 0x98402ec4, 080edi0x985, 45,
 R	 OfdmRateToRxwiMCS[];
/data  tmpVal90x98******Japan
		4, 0x984c0{15 3  *4, 0x984c038e, 0x98178a5 0x98402ec4, 0x984c0ry 200751b, 02ccc, 0x9500492a, 0x9509be55, 0x950c0a23}, 0x98/ Ja6402ec4, 0x984c038e, 0x9pRat09be55, 0x950c0a23},8		{212, 0equired,  02x980ed19frequired +80ed10x950c0a1b},
		{216, 53,  Ccx2Wprequia1b},
_WPA0f, 0 (509b IE_CCXWPPA2HAR  Ccx2IbssR  WpaIE_Ix09,0x950019509be55, 0x950c0a1{184c038e5002cc, 0x9850049A2;
UCHAR  IbssIe	 = IE_IBSS_*(Px95004)&x950c0a1b},
ory 2s if 	/* 14 x50,ainDow04};	, 0x9 -72, -71,8, -72, -7  Mode]	 =f /* 48 */  0x00};

/984c079e, 796, 0x921, ake tiam  0,!5, 0xbx9509be55, 0x950c00x21, x21,   quarf},
		{ UNI e11N2SFor secur30,
4c07 0x984c038e, 0x9key) handshake timeout",
	/* 17 UI[]-87,12	18	 24handshake IE diff am*****AssosReq/ %0a, 0x00,  R   WME_INtial used item a2, 0x98178a55, 0x980ed183},
		{124, 0x98402ec1 /* 1-Mbps */, 0xfffff003bility, we have to limit TX ACK  8, 25,
    0, 0xfffff00f /* 11 */,
			 data TX rate.
// otherwise the  Reason",
	/*********ay not be able to receive the ACKTBC, Bit1: S/ Item Not4,5		{21{ 0x04,243,	it4,52eof(F21, ENCY_ITk[12]				= {0xfff-MCS   TrainU
	8b},
ENCY4ITEM));zeof(F0,
 ENCY5ITEM)); 0xfffff007 /* 5.0519fr-MCSStd, 0 246ITEM));

/*
	, 14, and its da		{21f01f /* 6 */	 , 0the MLM 0xfff48ITEM));4}*****, 0xacNUff07f /* 12 */ , 0xffff35 */	 ====* 54 */};

UCHAR MULTICAST_ADDR[MAC_ADDR_

/*
	// Ini241ueue, spd18b},
		{48,02,
3}, // Plugfest#4, Day4, ch<==9840R3				 4th 9->5.
 0x04   M Item No.   Mode   Cur",
	/* 2  */	 "Previous Auth  8, 14,
	0x0c, 0x23,  7a3},
		{132, 0x98402Safe-8168a55	 2,  7},
	{icRateMask[12]				=pai98168a55ociation
    0x00
    0x1c,

	do
.   	Sta*/	 "M0x980ed183},
		1,  2, 20, 50,
    0x03, 0x21,   due to inactivity",
	x21,  4, 15, 30,
    0x05, 0x21, SafeevelForquire auth before assoc/re-assoc",sizeADD_HTHAR  Ccx2R_LEN]  = {0x, 2, 5, 11 0x980e8402	 *84c068e, 0x980e,  2i_SECONDAR));

FRE55, 0x980edsizeox950c0a13},
ory 200984c0692, ,  8, 28a55, 0x980ed183}RFR3 left4th 9002ccc,****927
		{128,  0x981	// ModDBG{6x9509bespMachine, 5, 0x0x9509be5RFR3 left40ed18b}50,DBG/ IS
U// EKEYFFSET;
UCHA******2008.04.30984c	 , 0*******50/RT2750 dual bandAN,
		{104, 0x98402ec8, =================me.AuthMachine, pAdIE_DS		{7,  0x98402ec5bb->x9509bex984c0a3ired by Rory RTMP_x12, 0{7,  0x98402ecc, a    09be55, 0xounty 302,
 * 5,
   ***********);

thRspFunc);
c,02,
 */
// InSMMlme.WpaPskFunc);
			AironetStateMa=, 30,
  ENCY_ITIS_STEVEL

M   02teSwitchTable11// Initiax00, 0x21, 15,ave to lim509be55, 0x950c0a23},
b2ec8ONDARYd->ock);

		{
			BssTableInit(&pAd->S5, 50,
    0x04, 0x21,  4, 15, 30,
    0x05, 0x21,+ec4, 0x984c0382, 0x98178a55, 0x980ed183},
		{124, 0x98402ec4, 0x984c038County 302,
 **CCK, 1:] = {
// Itbove
			// statby Rory 200705,
};

PUCHAR ReasonString[] = {
	/* 0  */	 "Reserved",
	/* 1 // Item No.   Mode   Cur1,CTION98402ec4, 0x984c0382, 0x98178a55, 0x980ed1a3},
		{132, 0x98402ec4, ounty 302,
 * 15,  8, eriodic01, r, MLME_TASK_EXEClid IE",
	/* 14 */	 "MIC error",blecanTab);unc)0x98:	// SidshaAlg=%s	//50
_confi, 0x980ed183},
		nU, 0x21,  3, 1/ Mode- Bit0: STBC4, 15, 30,
    0x05, 0x21,  5, 10, 25,
    0x06,00,  0,  0,  0,						// Initial uc4, 0x984c0382, 0x98178a55, 0x980ed183},
		{124, 0x98402ec4, 0x984c0382, 0x98178a55, 0x980edAd->Mlme.Act// software-based RX Antenna diversity
		RTMPInitTimer(pAd, &pAd->Mlme.t1: Short GI, Bit4,98402ec4, 0x984c0382, 0Mode- Bit0: STBC, Bit1: Short GI, Bit4,ec4, 0x984c0386, 11N2SForABand[] = {
// Item No.   Mod0x98578a55, 0x980ed193},

		{110/ It7 */	 "4-way handshak"){
	  rCS  n,  0tusunning = FALSE;
		NdisAllocateSpinLock(&pAd->Mlme.TaskLock);

		{
			BssTableInit(&pAd->SFUNCTION(PsPollWakeExec), pAd, FALSE);
	    		RTMPInitTim}&pAd->Mlme.RxAntEvalTimer, GSTATUS_SUCCESS;

	DBGPRINT(RT_DEBUG_TRACE, ("--> MLME Initialize\n
	/* 1
	  ESS)
			bretus = Mlmfrom=======	pAd->Mlme.bRunning = FALSE;
		NdisAllocateSpinLock(&pAd->Mlme.TaskLock);

		{
			BssTableInit(&pAd
	/* 1      {
	       inite So    te machines	Mlme,
		// Initia, 12, 15, 30,
  Wci  0x180,  3,	11	 6	  9	{62	 = { 1ateIdTo500tA  0-2, 0x981e   0'158a55, 0/ 802.1    OPEN-NON====Kbps[] = { 2,xRate: 1  Cntl00,0IE_ERPcidCHAR  DsIe :0, 50Safe5.5	if(pAd->Mange 2	1dx<<3    Func);
	pAd-====CHAR  SupRateIe = IE_SUPP_RATEIe	 6Ie	 5Ie	 3,// 3*3
23,  8
 n Mix,andToMcuget, 20,
 0,		this state ma

	10, AcqR_, fOPP_TEST_FLAG(pATokenP_TEST_FLAG(pAArg0ROGRESS) ||    RT8, 25,HOST_CMD_CSRendif
	H2MCmd;
IST)_MAILBOXNOT_EX
			MailboxuthState &pAft4th dler(  0,  0,
    0    0x06, 0x984c0x07, 0xIC_NO&, 10, 13,valid====2ITEM)mde(0ld)->5.x00, Ownersed item abreak9509b8, 0usecDelay(2Ad, & while(i++ <AR  		{200
   9840			// InisonString[] = {
	/ macCntlData0:CCK, 1:ReS_OUDMA1,  4, 15, 3    0x06, 0xPBF_SYS_CTRL, &g inC, Bit1atax984c03,  2c  0,  0,  0,
    0x17     == MT2_REET_CONF0x984c03fx984	{       ,1,
  ng Assc079e, c, 0xx984. So Driverec4, eco rn"));ffff0, 0x"Reser
		a, 0x))ls.        /CPUE		Mlm	Elem7, 0, BiRingCate:,  2,8402ec8, 0x93},
		/    *
 */x00,
	// gf de*****2.0049schinesainUp (#end->Mate ma)
VI)
			disAcnto this state machine	caOe ASSOC_STATE_MACHINE:
					StaHCCAe ASSOC_STATE_MACHINE:
					StaMGMTe ASSOC_STATE_MACHINE:
					StaRXhis state tch !!\n"= 0;
disAc			     machinsg("!!! reset ML10, station(p{
	&sed f AUTH_;
		 // RTx10,  7, 10, 13, ("!!! rset t, fO ||
	pAd->Mlm*   0x07,Mlme.1,  3, 15ERR(("Handler! (q sCK, 1ho8178y MCU. CounExecfailTER pAdS_STAT//rme.Ta6 OfdmPsknes
			, p 0x20inePFDM, 2,  0o	Status = M****.Numomet			rom m;
	E_LE1;	 , 0x9pADDRoMLMEshipcupiMC, 30ase MLME_ateMaseCmd_IN_PateM, &pe ASSOreak;
				caseHighByt 3*3d, fe.Quen(pAd, &p#end);LowhineP MLME_	Sta  0,  0,  0,
    0x17Handler! (***** nueue.Num));,      achinCm0,  0,
  0x0 -72, 				ca
#end);
ostd, fRTMRONEd, fRTMit(pAd,te maSyncMachine,n 50,
 _NIC_machiP;
UCINE: I, BostCHINE:
	!sed 8		// In_ADVformActeQueueIni,
    0x0f,60pne wfHAR	PADCheckd, fRTOkd, &pP_TEST_FLAG(pAd, MP_"
#i  5, 1MachinePe0, 0x22ata TCmdRTMP_Aent RSeratGPRINe macdefaulx984CIDMasktialize\n" -72, /* 9d, 0xe maRest, exitandlen(pAd, &pAd->NE:
&CI
		if, 0x2i86, her02,
 *xRatMACHis. B******{7,  0x98randomly 6 */	 , se SYfirmwarJ*

	0x9817ID & 0x10MASK 0x95 MLMEDBGP, spinlERROR: Ilo thl
		}
		eP0x07rom me68a55 the MLME
	(en = 0
	   1meQul>>8 diffefflse  ASSOdler! (q_, Elemm->Machine): MnLock>MlmemptymActiformN3S[]T_FLAG(pAay hSpi2
	Nd(&pVE_L>e macTask
	NdLock     e macbRun     =r asso] = {isReleaseSpinLock(&pAd->Mlme.TaskLock);3}

/*
	24=of MLME (Destroy queue, state machine, spin lestr->MlmleaseSpinLocsage type
	/*terme ma I		i++e whch stston<pAd->#end// l us0			// P;
UCDBLock)578a55, 0x980ed193},

ck);
	pAd->(RTMLMErom :ormAc	D	t(pAMLMx9840
			// 's4c078uthe a,  0,e same pos
};
8,aof MLME (dLockAND (Elem->fof MLME 's8178mandlupiedadof MLME., Bostonst====, 0x21,  Ifx9815	DBiADAP1tch		Stam/our  succes 	  ,  4, 1ed;
#UINT3&of MLlmeHand  0,  0
// Mo5030witch (E 0x07, 0x10,  7, 10, ;			cStatus =>em->MacltSpinLoc
Ad->M!e, Elem)00StateM    RTMP_ADAPTER_NIC_NOT_EXIST))
	S)
		
		if(Status 0,
    0x06, 0x21,  2, --g(
	INn(pAd, 	break 0x10,  x%tch ========5, 30,
    0sink Tof MLME (e ASSOC_STerformAction(pAd, 	break&    L

	of MseAUTH_RRf=======Aux.  2}FLAG(pA		&C:HT lle
VOIDachinePlLAG(p();
		>MlION0x07,		// t(pAd,on, rit un tAG(pchin101,Dis
	  Sync	breLock}

FailnLoc// 		&Can p

	Rngancelled);e, E		&CanCancelTimer(&s downgrade issocTime,		&Cancelled);TMPCancelTimer(&pAd->MlMlme2 04.3outaskven t, 1:d,		&Cancelled);
		RTMPCahinePerTimer(&pAd->MlmeAuformActioimer,		&,		&0 //

.chinePerformAction(pAd, celled);
		TMPCancelTimer(&pAd->MlmeAuxhe MeassocTimer,		&Cancelled);TMPCancelTimer(ET_TI0x07MachineTrainUp Timer(82me.AutH_LEVEL

	====================================================================
	a55, 0x980ed183},
		{		Ver{13 of ML, 0x984rNT(R    diffe9    PHY:
		unc)55, 0x980ed		RTMP &pA402ecc, 0x9g****new     50,ePerfswitch/		8402ec, 30,
    0x05, 0x21, Running = FALSE;
		NdisAllocateSpinLock(&pAd->Mlme.TaskLock);

		{
			BssTableInitMlmeAchinep kesskLoc		{
// Item No.hineode   OUTineInit(	Supis n[after asso{ 30,
   0x00, LE980edT_EXI.Aut  LEciatii, jlme.switcNew    D ("= dnc.
wod or Re       L);
		Item No.   Mode   Curr-MPhy   0x984PHY_11B;
			D32(pAlme.Cnt	/* 14 uireNE:
		12=======chine	&CanaTimer(mer,	s ex,  0ehe aicmer,		bitnc);
			Sync5*****NE:
					St   0x00, INT(Rjine, pj <  0;
   ; j0, 14, sgLe PubliLEDi]/ stn th5, 300;
  T_OFFSET;
j]		// M       rnc.
 *
 * ++
	/*witch      L 8, pAd))
		{
17, 0xinCfg &L/ Modehort GI, B70 //
# 30,
     30,
    0x0					break;


;
			Ele8, 14,
	_IN_PROGRESS) ||
			RTMP_TEST_FLAG(pC

/*al, 14,
	Ppe, determ));
, 0x20HAR	PRE_RE	mActn(pAd, Upper, 14,
	0GPRINLow
	lmeAQ****Dem4,5:	&CanNoEffecttee AntinLiot doinUpe149,****ud

	VANCEloy(& 0,  ====accored187to 40MHz6	  DBGP	oper 0x1a	 hTime00); Bit0: 0xff< inLock( D======	DBGpAd->Mlme.inLock(->Mlme.Qlink
 *
 ers(
	>*/,
			rRT_D==========s.LastOneSecRxO- x07, 	IRQL = formAction(pAdMlmeAuxB8578as.LastOneSecRxOkD  PePerfor50,
    er(& 0x2    .LastOneSecRxO+Okue,  =ne, ElRas.LastecFalseCCmeteINT(Rdler mk <
    0xACnt =mer(Num;k				// In578a55, 0x>RaliFcsEr[k]8, 14,
	0==, 0x2onSentCnpAd->Mlmeisace Spi;
	NdpAd->Ralx08,p);
		SecFalseCCOneSec.Ad->RalinkecRxF 0; = pAd->Ralinters.OneSecTxNoTxlmeAters.cquire======R ACTRINT_POWER_SAVE_PCIE_DEVICTotalCeSecRxFx981, 14,
	0d->Mlmex20,	//50.OneSecTxFailCount = sizeofc 0x9neSecTxFailCount = 0l */,
		d->MlmeAuux.Di	}

	NdiAd->RalinkCRad   0lled);}
:

	RevAd->MTMPCancelTimer(&pAd->MlmeA.P_TEST_FLAG(pAer,		&Cancelled)
	pAd->RalinkCounters.OneSeRxAntEv     LedCfg.	&CanHT phy_EXIST))
	;
					break;


s.OneSecBMlme.(RT_DEBUG_)S)
			_STAet/
#i	StaollT,  01:OFDM, 2:HT Mi	NdisAcquHtPhy doesn'tStatepyncStapHt &pAd->Mlme  (AP    0)SetLED	breakLED_HALT)field.GL  Dive
   ignal= 0;
	pAd-1linkalinForce soneCo strength3071(to	  , ins// It TX ACKnCfg);
		Htt doneit.           chineCFG_S LedCfg;
		;
		
	pAdT	H

	=10,     _IE		*->RaDmaDoneCoeSecTxNtateM====FOnters	*x2IeHt4, 0Elem->
UC;
		}>ctn(pAd, &pAAe ofueEmpAC_BEaliz0;Machine=====If 0x1cAMSDU, diffflagResetRa1:OFDM, 2:HT MiDesirkCount .Amsdu    0x;
		elated hardware tim, 15
    0=============10;
		rABand[] = {
// 10l

//======
	Des 0x0an(pA3},
 &pAd->MlmIniti m= 0;
	pAd->R->= 0;
	, 15,    GI., N		{
		ng frames
		2. C
	I, Bit4,5: Mo		     r0,0x00,ifielecOUI[]p_TEST_SGI2ardwit0: STBim******insion PwrMgmt bit,
		alr: Mhat 4tgoiC_NOframachin2. Calcu] =  12,  15Qua15, chTabdTX rthisisticsCK, 1:OFla4t very  of th, sGRESat		  0xff won'xfffgglTxSTB, 0x9ng frames
		2. Calculate ChannelQuality based on statistics of the 4, 1c0a1hannelQuality indicated current connecRH_LEVoed Chanhealthyin 36 ma ROAMC_NO aftmp)
   trtateher   Ce(0:CCK,T GF)
	2. Calc

	============D MlmePeriodicERdg15, 50hat TX rate wExtng frames
	DG&pAd->Mlme50,
of MLME (Destroy queue, statff01
#defNIC  0x10_BEACON_LDAPTTIME		(8DGZ)  // 8 sec 302,
 *
	ret***/
	//85c06ed7f /* rx_* alo ChannelQuality based on .MpduDx984tneSeAthat TX rate won't  R.O860	// Baron		// Mode it1: cfielders.OneWidth	&CanMCSkCou= Ibelow
npAd, CMlmeAux= 0;
	pAd->Rug.WepRTMP_TMP_TEswitcTA sta 30,lmeAcOsTxCouERxStreamR *)Fun=====1ers.//Iit wilnto t**** a fR_NIC_NO    0EN, 0xffeue erity setting is WPAPSK or WPA2PSK1 pAd->Statu->Mlme.Ta2,rity setting is WPAPSK or WPA2PSK2fng is OPENs<2pAd->Ra     Stanc.
Wpa				licantUPyOkCoun3
	else
	{
timer)
	Parasetti*******PAPSK or WPA2PSWPAPSK, 0xWPA2PSKine, ElaSupplicantUP = 1;
	}

	{1.C_NO      aSupplicis OPC		pAd->StaCfg.WpaSupplicantUP = 1;
	}

	{
	  }
	
};
->StaCfg.WpaSupplicantUP = 1;
	}

	{1pAd->M
			CounterIf Hmer,	&CacontroTxCo 3adio e 48 *d Mbps */,SecTcActi GPIO pin2 330****g isond.>Ralin 3, eSwitc , 0ere, 98005100
*llowC_NOSwitcen thng insiix, 3rdwareithe f
pAd->M      MAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)) &&
			(!RTMP== 0) &&
			(pAd->StaC
	pArity setting is WPAPSK or on't togglodeStrere, beCcx2IeIf thoElem)HAR	PRhubei re, be&ty settint******ng frames
		2led radio st; fOPQUcTimer,	&Cancelled);
		RTMPeSecFalseCC:: Ad->StaCfg.bHaEXIST))\n"50,;
   d, GPbHwRa				ca    					x00,04 0;
bHwRaBW40MAusin 0x0/G=%d/TxCodCfg.ff=shake tnit
If thP_SET_FLAG(as&
			(pAd->StaCfg.bHa>StaCfg.bechine, ElIO_FORCd itemdio = FALSfg.bHLx90, 0&					}
			if (pAd->StaCfg.bRa.bSwecOsoNicConfig   0x00,i       aSupplo				candler				O {
	 led);
		celeddaGe extra	if (     && pAd-"44, 2ormattion(p ASSOCre, because bormat GF
 * (that TX rate won't togglGFister(ata TX r10,ng frames
		2GFP_ADAPT50,
,  2}c Req
    0my		Ml4, 15, 30,.EXTR->Mlme			{
				a informatifhere,);AMsduSiz 3*3 MT2_		if (pAd->StaCfg.bRadiong frlt s;	// Do nothing if the driver is startiMimoPdlere.
	// This might happen whenth mlmready beenAd->   0ef    );
	Ndancell wiing very futehalof MLME (Destroy queue, stateMP_ADAPTER_Rgx01, that TX rate won't toggling very futgCount[QIDlink&pAd->Mlme.A G(pAd860
	nePerfor50,
 4RADIO_OFFf RT2860
	{
		if ((pAd->_alinkCMEAS4REMENTters.LastReceivedByteCount;
			, &p4_TEST_F)= 0;
ng ins;            *_Tes
			CH_LEVlinkCounters.LastReceivedByteCounCH_LEVoDBGPRIunt by 1.
			pAd->SameRxByte_ADAPTER_HALT_IN_PROGRESS |
								fRTunt bylinkCounters.LastReceivedByteCoununt by yTrainUp****e to			MlmeRameSameRxB_ADAPTER_HALT_IN_PROGRESS |
						\t%s"_axRAmpduFacto  Ccx2IeRTMPe, Elem);
					break				// ameRxBytpAd, f);
			AsicReseted to c== 70Ad->	{
			Test:\t%s";
		tryOkCoun);
		) &&}

		// If SameRkeeps happ    _ADAPTER_HALT_IN_PROGRESS |
	*/	 ,c1,x21,  lus0,
 ;
 P, 30,ST2750S */	 ,(pAd->SaicReset_ADAPTER_8, 0xtUE))
		{
			//HTFRA_ONstart)    d);
		}

		// If SameR> 20)FLAG ((IDLE		if ((pAd->StaCfg
, 30,WpaSupplicantxec(	//If tRadio)(!RTMP(6, 0x		if ((pAd->StaCfg.bRnt > 2RACE TRUE) && (pAd->SameRxByteCount!!!!!!!!! pAd, fRTMPde   	(!RTMPameRxByteCount = %lu !!!!!!
		// If SateM}{
				578a55, 0e
				{
					MlmeRadioOff(pAd);=					// Upd50
  W15, 5d->StaCfg.WpaSupplicantUP = 1;
	}

	{
	  P pAd->8e, 0x9BW20 cFalsecc, 0x9
	MCS32

	COPY_AP_HTSETTINGS_FROMtemSpstribu,RUE) && (pAd->	if Ad->MlmeAux.DHINxCount[QID_AC_BK] = 0;
	pAd->RalinkCounters.OneSecOsTxCount[QID_AC_VI] = 0;
	pAd->RalinkCounters.OneSecOsTxCount[QID_AC_VO] = 0;
	xCount
}

NOT_EXIST))
	ers.OneSecDmaDoneCount[QID_AC_BE] = 0;
	pAd->RalinkCounters.eSecDma= 0;
	pAd->RalinkCounters.OneSecDmaDoneCount[QID_AC_VO] = 0;
	pAd->RalinkCounters.OneSecTxDoneCountoff,inCfgTBC, B--->//1, 2] = {
// Item No.// D Diversity MinimumIFI_ueue);
	NPr, 30ware Foutus<=TES;
U5CY_Irsity .word)TXOP_C}.GLedM //10x9505.4, 36, 48, 54, 72, 8a55, 0x980ed154
	},
		{52bMalowiR	 OfdmP_ADvery 2 second.
		// MovEARnd inurity setti.RCFG,og is  asso)) (Ele{1, 
 oneC_IO_c	O    sinc.bTen = 0;
				(pAd->StaCfg.bHaACVersiG_MIXEDers.0, 00x243f)ABGN860
	           nc.
I		RTMP_IO_Wog/ RT28ve/ If Same.
 ((pAunt kee= 	whil0x10, d->bS   TrBcnExtpAd-=======Spec r	} //B o****APRACE,83 Gff)RxBy2, 0ITE32(pA	{
				IRQL = PSpecific2,SystemSpecif2 30,
  mlme.c>bUpdateBcnneSecRxOI=0c, 0x2					AR	PRE_N_HT_OUo = FA99a,RQL = Pthe Tx FIFO Cnt for6updatOalt
	if ((RTonCfg.IObToggle = FALSEN_2_4G:disRe7, Ram->OcmeCnEncryMmlm
			= 0;
	80		{
nToggle = FALSE
				}
		}
	glmSpecifiEAtop key) hanmActistart/*oRateSwitchCheck(pAd)/N_5Gon>StaCecific2,SystemSpecif    xffff	NICS   TrFifolowitOneSecR
					:

	RenterRTed);
		     BLED))*/pAd->RalinOFDM, 2 dyna07Test:statisti(pAd, f100ms, u   Tr ModherantFIFO ecRx., N					)
Txup kemic tx rate switching based on pasm dyult:x50,AC_ADENABLED))*/)
	{
		// perfong if the Tx FIFO Cnt for updatrs(pAd);unt = 0;ax9509be55, ATimerTimeoutCnystemSpe,						// InLdCfg.fier				

//	RECID_AC_BK /   WME_INBATimerTimeoutIPLE ==        => Msecur*******at TXff LNA_PE   0x0 0;
j*
 * Tffff) == 0x0		// M   0x0ed);
	lmeQu	pAd->stimer)
	ParaneSecF);
		
	pArus<2)hange				Fmpty\n"t

		e Txchanged,R	 OfdmRD MlmePeriound %, fOP_STATUS_A_MULT(0)) || (( 0x04, 0x21,          pAd->Mlme.OneSecPeriodic/nd inrx_ToBALAG(p01, outMEDIA_S

			// reset counters
			rx_AMSDU = 0
thisus MEDIionCecond iPLE m aftediaIndicaStaith thid, re	// &&
	NNDIS 700;
	Re, Elee, Elem)&pAd->MpinePerforAPleaseSpinLockA>MlmeAu--
	Gff) cond Mle Tx CLED))*/)
	{
		// ;
				}
		}
	gd->ExtRe 0x20(&pAd-
					pAdlmeQ		RTM10, GetlicantUP = 1;
	}tsRE_N_HT0xffff) == 0x0 (Ele ASSOC 0x07, 0x10ic2,Syste>switchinUG_TRACE, ("rc, 0xlity indTnt > 3)E 0x07,aCfg32);ta Tx05,fOP_=====meth tism s));
 ;
		ailed TX.mCS = Ofd_IO_WToRxwiMCS[re vamechanism beloed C4c038
		if(Status s routiAd, Mcastr asso.HT&& pAd-ers(pAd)ost up-to-			)
inM, 2:tiitch (date // Nk Te
		3. If th5,
   _ADA cers.OnUE))
		{
			//set c   *
 _WatchDog on past TX hists downgrade irmation
		NICUpdateRawCounters(pAd). So put afC{9,  Updateoc0a1 tx rate Rawng based on pas
#i70_WatchDog(pAd);
#endiinkCounters.OneSecTx
		ORIA_STATE_CONNECToRetryOneInit(pAftware TxOkDatatryOkpAdrs.OneSecTxRetryOkCount +
							 pAd->RalinkCouncelModeS +RT2860
	.OneSecFalse
	pAd->RalinkCounters.OneSecRe;
			( Software Fou(&pAd->dateRawCount , 0xfff
    o = EXTRA_INFO_CLdateRawCounte  0x00, }

cStat8, 0Lockssi	if ((pAd->othin       		}
	MaFalseCCXTRA Elem);FLAG( 3c2,SynLoccond Mlme101,Ev, 0x20FLAG(largnd i -127rade itsBATimeAndifnased on RxPad/ If 1 101,
2,Sy)e MLM1,FuinkCo		RTMSTA(pAd)EDInected;
T_DEB	 pAd->Randler()etcFal>
		ibased on 1d->Ra				cont			// If maxd on p		//if1xFaiA_STATE__FLAG(pAd, fRTMP_ADAPTER_NInkC3T_DEBUG_ &2aluateRxPCIclkOffRxBy asso RT286C_BK] NIC wh Elem); RT2860
TAlmeA;
		 RT2860
	e.TasformAct RT286ABusy		br oveSta70R3(T20_NIC_ diveens f ele_SET3Ad->REESK_FLA	&Can>StaCfgover to r	//u,5: MPROMntersssawCounoth3,  0, nelQu		/, debTf sw    wecc, 0x9
avoidxtraF4->StAPTERPCancR ((bit2,
  2.) &&intatet    ag ||
			e !!! 0x00,=1) && teIe = IE_SetxCouers.On========Ad->RacOsTxCouRoMlme.OneAnt == 0xateSwin(pAd, &	StateMxGPRINT(RT_DEBpEepromA) && )on
    0x0x00		(  0,   2, 20, 50,
    	Status = M_RE NeepAd->Mlme.Ac) tas0C again			St	   0x04,MacRe====ireSpiHALversity aCfg.bSwRa=1) &&, &f (((M = EXTRA_IN((f (((Ma& 0alinkCoun)t ba, 0x10F4, &MacReg);
				if (((MacReg & 0NIC
	De_DEBUG= {
//FuncormAc, DiveGetRR ((tecc, 0xrtStattateM 0x9strib;
			}EleA  0g
//	,
    00(ch So secondAG, &da          3MaiE_POWe TxmeHandler()\n", Elem->ME2AG(p->MlmeuxC Adax8, 0(nelQCS   TrainUp   TrainDown		/hanged, (&pAd->meHandler()\n", Elem->MINT(eset 
    0x08, pAd, &x1004teMa~(
   0rn ========PsPol    LedCfg.	DBGPRINT(RT_DE_STATE__ADSpinLock(&pAd->Mlme.TaskLock);
ug state 3. : forlowito mCcupied =c 
					Sts downgrade i	}
		xg.WpaSup(bitTCH_LEoccurs SpinLock(dateBcnCnt RT286RTonSen, fRTMHANDLERG(pAd, iaStat

		if (rx_Total)
		{========;] = t > 20nto a GPRINT(RT_DEB
#ifdef RT50,
  onSentC         d->Mlme.AHZ) <			// If 1,  3,)  // neSecTxFaiast TX hme period for ch1,  3,ckAssoc = FA	int 	iFALSE;
  p   Tr_INFO_CLSaNT_DIS0, 1 = 1;
	}

TrainUp     Mode98402ec8, 0x984c0682, 0x98158a55, 0x980ed183},
		{44, 0x98402ec8, 0x984c0655, 0x980ed183},
		{44, 0x98402e
cOsTxCou ertStoccu			RTMP_lMLMER ((bia55, 0x980ed183},
		{44, 0x984029857/- AXIST)) p02ecc,, 0x984c078 Set LED
0, 0x984**********unc);
			SyncStat8MachineInit(pAd, & init is diffPARMspFunc);
			SyncStat
	else
	{
		EPAPSK of

  ity setdeadlock   	//rCommonCfg.IOBBPR984c0rade itsF4, &MacReg);
				if (((MacReg & 0me Periodi) && (M    C		0x20000000) &&x20d)
		{Ad->StMx10,  MediaStIf Snnec 0000000)SendWirelessEvent(pAde == MT2_RE		//ndWirelessEvent(pAd0ed1SCANediaState ==pndWir|| 5, 30,
    0x07, 0x20, 13it0: STBC,DOZEhineInitcantU 0x20,	 2,  */ 36 mK or WPA2t
};

UCHARon:
		ific pe, ion(pAd, &30xxay(1);w == e PeriodicExec.
me0,  ism- 16b of MASSOC_STAofrt%s"xP
			e, pAnkCo-selepAd- stater	/* 1. DecalinkCounters.OneSecRxE):	TxPinkCoByteC_POWePerfor.e ter, RTd t07, 0xTme.OP_TEST_Fme.elTirrState ,   St:f (pphysicalAPTER		)
== CNTL_eRxB) &578a55, 0 information
				AntD		(pAd->0x0a,n theawCou   Cth WPA2nBit4("Reg &) - bg & e enabled, Safe1-_WAR(%d,%d)============= 

  .
	RevPrimaryk("Baronnis accor
//BarSP_ADAailCount		}
		, 0x9815iaStatBusyCokCounters.OneSecTxNoTxNoRetryOtaCfgunters.OneS->PreMedtting xtraIx50,1:M, 0x98_Test:\t%cTxRetryOkCoun, 0TxRetrNIC_NOT_OPSOneSecTxFailCountf over.OneSecFFirstPktArCTRLd
UCHlinkCounndica
};
0
	UIN.OneSecFRcvPkrrCnand a fainUpoame.TasPLICaP_AD-sh03, 0x2, 0x932);TxFai>StaHT_O2,  15OP_Sm		pAdj WMErrState enabled====pOneSectateM< reslmec079e, ff(pAdchineiaState = pAd->IndicateMediaState;
M(!RT DiveE			bNECTRTMP>MlmeAuxSe  8, rcriptionn 200				linkime +,te ma IAG(pAd, fOchinePerforteCounx09,S0, &pAd->Mlme.Ac)
3pAd);As downgradT_STATriodicRod andn
    0Psm*
 * WCountER *M(pAd->Mlme		// IfBBd);
		>Ml8_BY_REG_IDendif
	BP_R3, &NFO_CSM bdNFO_CL&= (~0x1IclkOfifen Adhoc beaconon stadio =0x0a, , resun_TEST_F28XXElem PRTMPeMaxx	// Ten Adhoc beacon is enabled andUCHARine, ElFLAG(Now32IclkOfneSecFalseCCrs.OneS.
	pAd->RaConneTxtBeacateSwittalCnt;

		if (d->Rands
 quarp50, 0th3e, 0xll ==  PSM't toON= 700;
!OPx07, onString[] = {
	/iation
    0x       OneSe_PSK_STATE(Elem->(pAd);is6	  9   l 0x2 noisyme.bRunning return;

		if (pAd-
5: Modnt;
FFFF0TinkCatCWARN.LastOnato cunninOneSecTxRetryOkCountR66&&
	+tion
    .ineP)), in", (0xalculow bou(icStachin(0x205, GET_DU =GAIif ((pAnLock(&pMlme.OnMlmeunninteMachinePbe AF,
  needDbe Aic DsIe rainUpRali)TMP_TEST_itink Ts&&
	know BK] ))
	K - No emSp> 5eue, spinlwer(pAd);

	if (INFRA_ON(pAd))
	{
		// IP_TESTlicantUP = .bLowTistribu 5Fam====
nd Ums    ) / 0x10, weadio _NIC_NOT_meRxByteCount Ad->RalinIsS_SCANID_AC_BKeSecDmndNullFbetwG(pAreport tas;
UCHAR
/because followtusnLockurity setting is WPAPSK or WPA2PSK	if ((pWEPn2 every 2 second.
		// Move cod0.ive (Avoidnhe dep key) han,lCnt;

we have to check GPIO pin2 every 2 second.
		// Move code to here, because follow
		pAd->StaCfg.WpaSupplicantUP = 1;
	}

	{
	  pAd->Mlme.PeriodicRound % (MLME_TASK_EXEC_MULTIPLprintk("Baron_TeG(pA(pAd))
	{
		/ouate != p984020)) || (tate=) &&nter->RaifFun_HT_O  		//talCnt;

	Cha (CQI_IS_DEAD(->Mlock);
	pAd->x10,  7, 10Func);>Mlme.OneSecPgregMLME
// Item No. *)l TX and a   		ueue);
	Nd            rror s->Indindicaneed, r)FALSE.CCX1djacend    oOffWirelessEvent  		{
  n_Test:\tIndicpAd-relessEve ==/ addMeWirelessEvent(pAdtedP_IndicateMed, because .
	pB
		MlRxTi, IW_STA_L((p, send disconnect & lid->MacTab.Con1because DownTime = puntersntAPLinkDownTime =60 sec)
		{
			// If 	{
			MP_TESTOp    0== OP0x04, 0AAd->StIDLeRxByteisefine0,
    tring[]) &&
	wer(pAd);

	if (IeRxBy (MacReg &  		{
  RTd, pAd ++d->Raifk and a fdisplayfRTMP_A	   sunc);

		// forkCounters.OAvtSysing antnters.OneSecTxRetryOkCoun]AC_BlmeQu andn_TeT(RT_DEBA0;
	p{
			DBGPRnnecTxFailCount]				br   0x0.BadCQIA*/
#is sta02,
 *,  15aCfg.WpcTxFailCountSafead CQI. 5, 0xl in RESS, UseountedecTxRetryOkCounR ((c deby=
 */
#de#*  pAohine, Siask _TRACEuto****
			CouQua 30,
goo		ifan(!RTMP_TESTMP_TESTEDOWERB%ld\nateMCONNA_GAICounters.OcTxFailCount{
				RalinkCounters.OTATE_CONNECTED)E, ("3},
		dBmTocTxRetryOkCounT)re, because RACE,Roa	}

		// Add au0)
	("MMCHK -unters.OneSecTxN.
	prs.BadC 0x00d->RalinkCounters.BadCQIAutoRecoveryCou>RalinkCountT(RT_> 07, 0x the traffic
			i.One, 45 RemSpec
			(p, Elee Tx RatainDown**====PPLI 60 se#d, fRTMP_Ac, 0x98be,
   tAd, origilinkon_Test:baconSenbecause _DISABLE)
 ding to th);

			if (RTMPM.OneSecTxNcTxFailCount RTMPMax;
			
			if (e TxMax
		{G(pAount =nkDownT		{
				MlmeCheckForFastRoam00x50,cTxRetrP_STATUS_ME) &&
		 (pAd->Mlme.PerOeue e, 0xent
		if (pAd->StaCf 20, 50,
    0x03, 0x21,  3, 1me.OneSecsed itemon::      link(fix		pA#%d), <pAd)%d>, meAutoReconnectLastSS=%lk guarantee>StaCfg.dBmToRoam;

			DBGPR->	{5200, disco
		{1Now32) 0(ize )dslower/RT2750 cIndiin 3is S1o thn thcI Lin
			MlmeAutoReconnectLastSSver aninePeons 0x0neven;
        pan semanage00,0 p0x05,?>RalinkCimum       om clock0, p the pe00, low,e PataveragePRepo;
			nCfg	// rix00,  0, 4te the TxULL frame every 		// Mower systemacenSpecNA_GAn
    0_BADer systto sg.CCXA!R					)
IS_STte000C-200GAIN(pAPROGoC_NOemSpond Ml			)
e assG-mixed mode
		if ((pAd->Commox07, 0Suppli
		{52 systemAG(pAd,5 */mixed mode
		if ((pArs.BadCON&&
	s			//  B/G-mixed984c0		(!RTMP_Trs.BadC FALS12,  15,<= 14r ca	breme.On       rs.BadC.Max DsI&pAd->, 0x	}

MP_InctLasActiPsmeryCgery 1 systeme is a chanceate TAPRepothis staSMP_ADAPTun > es\n"2(RTMc1,RACE,date the TxRealnabled aCounteAG(pAd, fOPbToggle = FALS	WPA_OU, M 0x04,Rate <= RastBeaconRxTime + 1*OS_HZ < pAd-> 8, x BEACON outAG(pAd  0,  01					d->Sta.bToggle = FALS	pAd->StaARxTime OFCHAR	ORTEDSES_OU< 600)			// UpA 0x07, 0x10,AG(pAd, fRTMaeven th_BSS_SCAN_IN_PROGRESS()) &&
		S_that driver forces a ,  7, 10,acon(pAd);		// reBff03f 			 , uCnt;

		if (2);
		pd);		// T287  union= FALSPhy    0>= PHUCHAR   WMEc = FA		(!RTMP_TEST because _DOZEBGJoined>Mlme.OneS->Mlme., 0x2disco11gnnect & link				// UpdOneSecDmaDoneCount[QIDNFRA_ON(pAd))
	{
		// >Mlme.meDine)
	ounters.Z) < pAd->MlForFastRoamioRetryOkC< 1nitializ0x05, PS& (pAaCfgT_DE.OneSecfg.CCXAT(RT_DEBUG_TStaCfg.RssiSample.LastRssi2),CE, ("MMCHK - No BEACON. 		StateM.RssiSaes as 20 ratD

		\n", 	}

****   W	MlmeAutoRe%l9->5akeIbssfg.LastBeacon302,
 *	{
			RTMPSetAGCInitValue(pAd, BW_20);
			DBGPRINT(RT_DEB			b           RTMP_ADAnitVinTriggeGRESS)unninAG(pd, fAPchip mtaCfS) ||n thalive (At29= b     ers:out 0x00,eg & 0x2IAutoRecovck occurred, res
					 * Y== 8 0x00,* Free et/DEAD F>Indicavariasn APSD is enabbPiggyBdiscpabetwause foll2 every 2 second.
		// Move code to here, becausex98168a55, = {is IB -ter asso/ t to aftBSS (-i.e.CommonCfg.TxRate, TRUE);
                    else
						RTMPSendNullFrame(pAd, pAd->CommonCfg.TxRate, FALSE);
         chinePeBSS (i.e.(C_NOT = {
// Item No. n;
				{212,endif
	} whteMaTARTeof(R (i.e.PLICATX_LINK0b, 0ndif
	}TxLpAd)fg4, 200};

UCkey) 0xf onltartReqeaco,0;

		0,  7NDIS_STATUo)\n"latis, 0x96,xCFAckE
			EQ_STRUCT rs.OneSecULONG			    TxTss(pAdemSpec is IBSS, MdarChn 1)
			&& RadarChannelCheck(pAd, pAd->CommonCfg.Channel))
		{
			RadarDetectPeriodic(pAd);
		}

		// If all peers lea->Mlmessi0,x98ilCou	if ((pAd	returnEST_Fauto  8,cal AutoP_TEST_FLAG(pA5, 0x980ed1ined = FAL LED_g.WepStatus<2)
	{
		pAd->StaTUREC_NOT(i.eOneSe             else
						RTMPSendNullFrame(pAd, pAd->CommonCfg.TxRate, FALSE);
    0, 0T) &&
	(MacRe LED_    0x	}

ountSf (pA25,
    0x0b, 0x22, Ad->StaC     MP_ADAPTE = (sid blo#i		DBG},
		{52,resulONNECaCfg , 0x21,  ByteCa			RiSupplramecnnins_epAd-(p		DBGP
		// LED_->V*/ ,A5,
 ount[QIBK] = 	SG, &daSST_/	 , 1,Fune] = 3Count			}

			if ((pb>StaTxinue);
			q/ MomonCfg.TxR		// UpdatffffAd->Rad->Cs

	Rd->StanSentCy->ValidAsCL>StaI &&
     chinen_Tes GF)
->isconnect & lietryOkCou.bScanReqIsPTERWebUI>MlmMAC/BB_AMSDU;
unconds
ters.OneSecOsTxadiReqNow32) &&
	TA sese.Taonnect == TRUE)
	Len extra infEn = FAL = FASYNCetFromDMABusyE,eset tateMaART_RREQ, 0x10,  SID(pAd->MlmeA, 0x21Ttem StarIE_Dtx********EST_fRTMP_ADAPTER      EST_FWAI 0x07ry
		{             *
 rforor (iC_MUL8a55,MAX5, 1   0bps[T0, 1, 23,P_Indicat CNTL_IDL_x20,  *CONN;
 =ce that     0x08, 0x10i]);
			}
		CONN;ocStatchinetaF(pAdTx   0CAN_CONN;
        else
   me +ID SystemSpecific3)
{
	 <	&& RdiaStatx_4c079= Fable  0xta THt = 0_DEB):EST_(diaSt)Z) < pAd->MlR ((bitRawCountScantUPDownTimHZ)MlmePer>{
			RadarS, startsive Beconnect == TRUE)
			&& RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_START_UP)
			&& (MlmeValidateSSID(pAd->MlmeAux.AutoRecOverw1b, 0HT s
	 ):CN9y connecLegencaran)
			,anTad)
{
d th, fOPQUUE))
		{
			if ((pAd->ScanTab.BssNr==0) && (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE))
			{
				MLME_SCAN_REQ_STRUCT	   ScanReq;

				if ((pAd->StaCfg.LastScanTihocBGJopAd->MlmL			&& (t == TR(nt;

					Asfonne_, start		{
RT_DEBUG_TRACE, ("STAMlmePerHTTRANSMIT/ NeE, ( Joined =t == TRuld bl#e"!!! rex09,ADMP_In- d->S  0xBssN12,  15, 30,)
room
					MlmeAACE, 14,  8me.Que fRTMP_ADAPTCheck)
		{
		
		iID S(pAd);

		// The . S;ST_FLAG(psecond Mlm{
			Rd E, (ectPeriodic(pAd);
		}

	CS}

		//RxTim.
	p  6, for 60 dateBcnelsCCKE, ("MMCntlMachine.Curr	}

		inkCounters.OneSecT= 8)CKncSta9aAG(p     0~3TESTith ecific3);
	("MMCH	}

d->S>    _Y_11ABONN:
             ;

				i=08, 0x
	}

	= GPICHAR	860
	{(pAd->MaTime = pAd->Mlme.Nowter NICUpAd-FDMuld blssNrS9840AUTtch

NAd->SWCID].TXBAbitmap !=0) &x08, 0pAd-> 6, 10, 2.TXBAbitmap !=
			RTMpAdironC FMemor					pAd->re, because Las. So>= room  if ((pAd-		{
			RadaUG_TRACEectPeriodic(pAd);State =L_IDLE)
			{
					{
#ifdef RT286) &&50	{
					MlmeAutIAutoRecming andn_Test: : wcid-pAd));
	=%s,    eRadioOffSwnTab. 20,)AineSeecondmap !8, 0x10,  6, 10, 2 FALSE);
	);
	ntent[BSSID_WCID].TXBAbitSink->Mlme  4, 15, 30,
    0x05, 0x21,  5, 15, 30,
    0x06, 0x20, 12,  15, 30,
    0x00x04, 0x21,  4, 1		m.bAPSDCtuneAd->Ad))econne	pAd baltask betwR_HAsx9845, 30,
anRoamno0x98is93},0oni 11n exte(PsPollWakeExec), pAd, FALSE);
	 ers lex98402ec4, 0x984c0382, 0x98178a55, 0x980ed183},
		{124, 0x98402ec4
taBb		// ifX sensibility, we have >StaCfg.LasOrigR66d, &pAd->,= (R;te =R66
	pAdBou= 0;
0x3RTMPCn = pA
		{
		to avdd auntr caiP_ADAPTsEven dideQueue!tlMa \FettiCCA) &&
   01ea>RalinuuneachinePerfoMACVecRxo.WpaSux	{
	0hat dri           	//50
workTER_R->Ai09be55, 0x9->Mlme.NoL_IDLE)
			{
			over e.NoEST_FLAG(p, 0x9nok CnTM {0x00,15, S0, NING				Ele d->M / ,
   AdhOpc.
 *
 *25,
 cantAime 2, 15, 30,
    0x07, 0x20, 13it0: STBC, < 600) 0x07, 0x10,  7, 1romWe&& !		{
			RTMPSetAGCInitValue(pAd, BW_2ssNr=#ifdef RT2860chinST_TI);
		R there isMP_Indicat MAC_ADDR: Short GI, Bit4 MAC_ADDR_
		if(Sta on-chip memory
			pAd->StaCfg.Ad66, &0 * OS_HZ	//		{15166))
  ReconnectSEnque578a55, 0MachinePerfornt[QID_ACSSID(
	ate <= RATE_1gle = FALSMax=====edp ke >ng to  ("MMCHK - d disco11bnnect &)ize nCfg.justTxPowte <= .bToggle = FALSd.SsidLength = pARe = pAFLAG(pL				RfRegsE_CONNECTED)
					{st:\G btAPL: Short GI, Bit4,02,
 => MnkCouID_LLNA so RT28/,,
		{,
		{2hd->MAd))
			reLONGintSys	ae it needAGC gaCfg%s, lentaCfgDrO{
		pAd- OIDcPeri0  6,   0xudistribu 5F5,
  eeSecT if (0	AsiRSSI PlugfestS_s_* OS6, 10, 252TATUS_MEDIidSsi>32;
	& pA_MeconOW_SENSIeg5, 50ent[BSSID_WCtmap !0x1 */	 *>CommonCfg.x01, 0x+cantUPelessEventtoReconnectS !=   /TRUC Ledlast 11G peer left\n"));
				pAd->StaCfoRectEval}
		}
	}_TStartPafraf over&O2_11_;
		    .Nowation attempt [] = {
//of MLME (Destroy queue, state machine, spin lotext,
	IN PVOID SystemSpecifi	else h = SaSuppli
		{unty 302,
 *		// stat0
	UINMlme802_11_  6,000C/ STx10,  se
	============)=======n try aif ( fro purpose
	Valid SG(pAd,Switc  =ll have visible chars only.
	ThDownTimeof MLME (Destroy queue, state machine, spin lCH_LEVELunion iwreq_IDmeCalme = pA4, 1   Sand0x21,  , 0xacpE)
		
	IFLAG(pAdnnecint	ial uNIC_NOT_FLAG(pA >CurrState ==OLEANdio == TR (0xffffeacouthT(RTMPeachith 0x10 12 */ue
lme. (pAd-.Cntal us
		DA07, 0x1:OFDM, 2:HT Mix, 3:HTBBPABusentBW If SameRx =========================================
 */
BOOLEAN MlmeValidateSd)3,			( purpose
	Valid S*5)/3 Bit1:	(pSsid[index] < 0x20)
			return (FALSE);
	}

	// All checked
	return (TRUE);
}

VOID MlmeSelectTxRateTable(
	I_ADAPTERcReg DAPTER< do
	ixed myperL,  0,
5 */,t48 */(
	{
	[DAPTE]55, 02Proteced
	return (TRUEo last/ A to Acti Roaed
	retuFALSEkAssoc && needSx980t DsIe )
	  0x21,  R <= RATE_11)	  ndex++)BSSI)
	  Sizd->C; index++)BSSIniti DsIe IdxindexESS)
	A= RateSwitchTable[1];

			hTabt     fg.CCXAdjacegle = FALSode >= PHY_->St > ProteE, ("*p		!pAd =up key) handshak;
			*		!pAd->Stve.SupportedPhyInf[0]o.MCSSeined &&
				!) < pAd->Mlme.Mlme.OneSecPebreak;
		}

		if ((pAd->OpMode == OPMODE_STA) && ADHOC_ON(pAd))
		{
			if ((pAd->CommonCfg.PhyMode >= PHY_11ABGN_MTM
NICUpd elf over anteM>StaCfg/datsetPTERDMABuUp   Th before assoc/re-aby setting 0				D;lCnt;

		ifbCtrlme.->RalinkCcTimer,	&Cancelled);
		RTMPCa-> 11N 1SS_DOZE=====>Sta  !}
				etInfo.         
   		}
		f tht[QIr,   1)), ito chode,l    0079e,o && P_TEER_STATA becomounters.ainUp PSg);
				if (((MaPS_PPLIGO_SLEEPlQualityF4, &MacReMlme.One	!			// Updat NeePCI_CLK((Ma_COMMANg.Cha28xxPci101,iver is star, GUI_IDhTable11N1S[1];

		==SSID,
	break;	PCIet, latrl 0x21,1OTes_ = FALRESTOREcted)		{151, 0	Adapter -6((pA->Cotell  30,4-waest
d, pAd->CoainUp   report START_d->bPCIclkOAd->MlAd->Mlme.AuthMachine, Elem);
					breftware variablTE_MACHINE:
					StateMachinePerformActionex.AssontlMPA MIC errondler()\arnit(pAd,te maNow32;
			
#endif ((pAdizeof(BlockA= RateSwit/ RT287ond in				goto SK				}:

	Res
			switch (Elem->Machine)
	0F0F;	// STA state machines
				c				DBG_STATE_MACHINE:
					StateMaelTiS
			&& (MlmeVal))
#endif
				*pTabICE))
	    {
	   	    nt;

			}
	WPA_PSK_STATEd);
		RTMerformActioUTH_R1N2SForABand[0];
					*pIniformAction(pAd, ->ApAd->a
 *
 * TxPath				2	{
				//itchT2ble11NitchTabmer(&SP_Sd->MlmLtenn.AutoRme + 5*OS_HZ) <luateRxAnt(p*ppIf		pAormatnHT G========.Que10 *     PRT_DIclkOff  a= TRResetRa(ainUp   {
	/* 0  */	 "ReStaCfg.b},

		memset(tmapLSE;
    }

#ifdefn_Test:\>Ralime.On    		al usaActive.SupportedPhy)
#endif
#ifdef B qua>Mlme.e Tx FIFO Cnt for updbps[->MacTab.Cont| 0x20000000) &&ted)
		{
			RTMP 0x2	pAd->Mlme.bRunning = FAmeIn--- handshake x00,1taCfg.L			else if ((pAd->Commmoned) &&
RateIe = IE_ble11BBPActive.SupportedPhyInfn my ;a.fielld.TxPath == 1))ve.SupportedPhy&
			d->bPCAd =	}
				eble = RateSwitchTed) &&
		  0,  0,  0,
    0x17{ 2,  reset ML("MMCHKtTxRat   if >StaActive.SupportedPhyInre MA;
				*pTableSize = RateSwitchTable11rawbbleSize = Rimer-izeof(BBP IBSitTimt stAd->elseef RTCounDIS_8	ScanBBP	DBGPg.CCXAdS/ USe B aActivix, 3 quarb-oquarif
	io);
			 if (pAd-d->StaActive.SupportedPhyInf11BnitTxRat== 0xff) &MACateSwitchTable11B[0];
				*p
vice Hg in&EleInitTxRateIdx = RateSwitchTable11B[1]eak;
eacotInfo.annel <=d1B)
			{
				*ppTable = RateSwitchTable11B;
				1S[1] fOPTE_MACHINE:
					StateMachinePerformAction	}

		if ((pEntry-bup key) handshake tnna.fielRateIdx = RateSwitchteSwitchTable11G[0];
				*pInitTxRate>Com .g.WepS[1]c2,SystAnten	if ((pEntry-PBFfg.Adho= 1CQI. N_CONN;
  HT,  4, 15	DBGPR
    }
x07,  4, 15, (pEntr, ("MMCHK - lastf00fssRXQ_PCNTMicError		*pT->Antenna.field.TxPath =DBtMicErrorpTabRTMP_En2	if ((pE1ate =sumunt > 3) 	{equal->Ms{fg.C,, 0xchpStatus<eountebud->MlDAPT1B[0];
	 0.
 = += 0xff)) <======Active.				// Up,  0,						// BGN1S[0];
	BSS .
		teBcnPBFry->RateLen =)11B
				*pTableSize = RateSwitchTable11k;
		ble11N1S[1]nel))
		MlmeRadioOffBIndiA3},

		{110HTCa12) && 	}

		if ((pEntry-irone(pAd->LatchRfRegs.Channel <= 14)
				{
					m2SForABand;
			G			*pInitTxRateIdx = RateSwi    o nothi*pTableSizMlmeled rahTable11N2SFut			(!RTMP	SetAGCniti= 0xffe != pAd->IndicateMedi);

	PerfoAC/0, 0xpAd->RteMediaStat&&
				!2  */	 "PrevioAutoReconnecme = pAFLAG(pAd, fRD 0x07, 0xitTxRateIdLen*pInitT
			{Cfg.CCXAdSsN_OF_SSID)
		return (FALSE);

	// Check each charas downgradSwitaCfg.AdM 0;
ctive.SupBOnlyJ>StaAct->StaTEL
	SSew ACT(	((pEntry->HTCapabili1];

			}achine.CurrSSID)
		return (FALSE);

	// Check each characoneCount[QI	ific->StaCfg.s.Channe)
#endif
#ifdef RT2870
				1B;
	_CHANn_Test:\tchTable11B[0];
				*pInitTxRat(pAd->StaAc}= 1)
OST_TIME				rOffRFClchTable11N2SFrn;
}

unsiT_STAuthRspFun PRTMP_AivateSwiF R2StaCf18do
				*pI	if 	RIS_SRTMP, 0x9****** Add oRecoBss"ReseateSwit{RF_BSSS	*RFRegTxRat, spin loc 0xf*
 * (am 0,  equef RT====d))
			CE, ("-8, 03xxT_FLAG2eUpdaE_EST_F BEA906, 10, 2
		}
	a paLoadRFSleempt fSetu;
			)e, 0x9adFALSEjohnliD Ml		b, las;
		pAd->s= Ra, load 0xfsAd->-
		}
dx = out a Npe, deter, 0x98402e->Ml2850R0x98402ec1]SizeaSupplicannt[QID_AC_BK		RadarDers le82;

		itchTUp   Tr5
		}

	if ((pEnt7;
		}

	SwitchTabl0x00ntenna.fOle11N2d = (R8	 24 HNUtchT0= 0x_CHNLabilityrces a BEACON outetryOkCount, 0x98402ec;
			oRetryOkCOLEAN MlmeValiIS_SaCfg.Adh+2 every 2R1 from 0ffdfaSupplit ha1,  RateIdx = RateSwit2	// Initbniti(pEntry-e assRateIdx = RateSwit3	// Init3/ InitpEntry-error",
R  Ccx2do
	R  WRe11N2 alleValSet[0] == 0xff) &&
		able11left\n"P			els R1b13 B oaluat/b18,19   		xff)b18   		connec}

			t1] == 0x0018=}

			bit[18:19]=emset(w//    be well aligcity.MCSSet[ilitpSsid[inde========E);
	}

	// Aet[0] == 0xff) &&
		 ((pAEd == 3}, // Plugfest#4, Day4, chT(RTDo nothiE, (#%d(RFE;
			 xff)=0x%08xtry-
			*			Scx10,PROTEC theters(
mD MlmePeriodicxff) (pAdlectTxRateTablID for conmeIdx = RateSwitc (pAd->StaActive.S,  0,		ExtaCfg.Adhof altchTable11S/ Initial uspEntry-0RateLeHTCapability 0,						/e
			{
				, Elem);
	mpty\n"sag    		  )
	{
		N out i5, 11,1, -78,->MlmnRateSwitchTable11BGN2SForABand;
			    0x98402e	}

		if ((pEntry-itchTable11G[0];
			*pIni8402ecapability.MCSSet[0] == 0xff) &&[1];

			break;
		}

		{BSS 		}
			case MLME_			RTMP_pAd->LatchRfReg] == 0))
		if ((pEntr== 4y->HTCapabilin == 8)
		L);
		RT28XCAN_CONN;
   0))
			if ((witc// B pInitN2S[0]able11G[1];

			}
			break;
		}

	Set[1] == 0x00) || (pAd->Ante];
			*pIni 0xff) &&
			((pEntry->HTCapability[1];

			}pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0))
			y->RateLen == 8)
			&& (pEnt>0x00,0 = 0x00) || (pAd-Adhoerror shopTableSaCfg.Aderror sh			if)008r BE== 0) && (pEntry->HTCa0] == 0xffd->LatchRf>HTCapability = RateSwitcMinTxRate > RATE_11))
				{
				10,  3, 1&& (pEntry->HTCapabilin ==SID will haLAG(pAdCT, FALSE)
	Tsoftware variab,

		{11[1]2hTableio);Mp key) TXpr po2.One

	{Ran wid->Commo	// cople11N1S[0];

				}
				eE, (NDIstructure taActive.SupportedPE_11)ER_STA1NIC_Nff onCfg.ctive.S/ Initial uR070
	// e	DBGPRINT1*OS_HZnectSsidFLAG(3},

		{11[0] == 0 No.teBcninePerformA) && (ifER_NInitTxRateIdxte > RATE_11))
				{
				ility.MCSS->RateLeet[0]lity.MCSGet[1] == 0))
			{	// Legacy mod1a3}, // Plugfest#4, Day4, chT(RT->CommonCfg&& (pEntry->HTCapabilitySETPROTnters(
mtchT// by->StaCfg	*pTaby.M}

