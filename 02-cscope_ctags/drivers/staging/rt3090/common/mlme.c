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
UCHAR	WAPI_OUI[] = {0x00, 0x14, 0x72};
UCHAR   WME_INFO_ELEM[]  = {0x00, 0x50, 0xf2, 0x02, 0x00, 0x01};
UCHAR   WME_PARM_ELEM[] = {0x00, 0x50, 0xf2, 0x02, 0x01, 0x01};
UCHAR	Ccx2QosInfo[] = {0x00, 0x40, 0x96, 0x04};
UCHAR   RALINK_OUI[]  = {0x00, 0x0c, 0x43};
UCHAR   BROADCOM_OUI[]  = {0x00, 0x90, 0x4c};
UCHAR   WPS_OUI[] = {0x00, 0x50, 0xf2, 0x04};
#ifdef CONFIG_STA_SUPPORT
#ifdef DOT11_N_SUPPORT
UCHAR	PRE_N_HT_OUI[]	= {0x00, 0x90, 0x4c};
#endif // DOT11_N_SUPPORT //
#endif // CONFIG_STA_SUPPORT //

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

#ifdef DOT11_N_SUPPORT
UCHAR RateSwitchTable11N1S[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0c, 0x0a,  0,  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101,
    0x01, 0x00,  1, 40, 50,
    0x02, 0x00,  2, 25, 45,
    0x03, 0x21,  0, 20, 35,
    0x04, 0x21,  1, 20, 35,
    0x05, 0x21,  2, 20, 35,
    0x06, 0x21,  3, 15, 35,
    0x07, 0x21,  4, 15, 30,
    0x08, 0x21,  5, 10, 25,
    0x09, 0x21,  6,  8, 14,
    0x0a, 0x21,  7,  8, 14,
    0x0b, 0x23,  7,  8, 14,
};

UCHAR RateSwitchTable11N2S[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0e, 0x0c,  0,  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101,
    0x01, 0x00,  1, 40, 50,
    0x02, 0x00,  2, 25, 45,
    0x03, 0x21,  0, 20, 35,
    0x04, 0x21,  1, 20, 35,
    0x05, 0x21,  2, 20, 35,
    0x06, 0x21,  3, 15, 35,
    0x07, 0x21,  4, 15, 30,
    0x08, 0x20, 11, 15, 30,
    0x09, 0x20, 12, 15, 30,
    0x0a, 0x20, 13,  8, 20,
    0x0b, 0x20, 14,  8, 20,
    0x0c, 0x20, 15,  8, 25,
    0x0d, 0x22, 15,  8, 15,
};

UCHAR RateSwitchTable11N3S[] = {
// Item No.	Mode	Curr-MCS	TrainUp	TrainDown	// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0b, 0x00,  0,  0,  0,	// 0x0a, 0x00,  0,  0,  0,      // Initial used item after association
    0x00, 0x21,  0, 30, 101,
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x21,  4, 15, 30,
    0x05, 0x20, 11, 15, 30,	// Required by System-Alan @ 20080812
    0x06, 0x20, 12, 15, 30,	// 0x05, 0x20, 12, 15, 30,
    0x07, 0x20, 13,  8, 20,	// 0x06, 0x20, 13,  8, 20,
    0x08, 0x20, 14,  8, 20,	// 0x07, 0x20, 14,  8, 20,
    0x09, 0x20, 15,  8, 25,	// 0x08, 0x20, 15,  8, 25,
    0x0a, 0x22, 15,  8, 25,	// 0x09, 0x22, 15,  8, 25,
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
    0x0c, 0x0a,  0,  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101,
    0x01, 0x00,  1, 40, 50,
    0x02, 0x00,  2, 25, 45,
    0x03, 0x21,  0, 20, 35,
    0x04, 0x21,  1, 20, 35,
    0x05, 0x21,  2, 20, 35,
    0x06, 0x21,  3, 15, 35,
    0x07, 0x21,  4, 15, 30,
    0x08, 0x21,  5, 10, 25,
    0x09, 0x21,  6,  8, 14,
    0x0a, 0x21,  7,  8, 14,
    0x0b, 0x23,  7,  8, 14,
};

UCHAR RateSwitchTable11BGN2S[] = {
// Item No.   Mode   Curr-MCS   TrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
    0x0e, 0x0c,  0,  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101,
    0x01, 0x00,  1, 40, 50,
    0x02, 0x00,  2, 25, 45,
    0x03, 0x21,  0, 20, 35,
    0x04, 0x21,  1, 20, 35,
    0x05, 0x21,  2, 20, 35,
    0x06, 0x21,  3, 15, 35,
    0x07, 0x21,  4, 15, 30,
    0x08, 0x20, 11, 15, 30,
    0x09, 0x20, 12, 15, 30,
    0x0a, 0x20, 13,  8, 20,
    0x0b, 0x20, 14,  8, 20,
    0x0c, 0x20, 15,  8, 25,
    0x0d, 0x22, 15,  8, 15,
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
#endif // DOT11_N_SUPPORT //


extern UCHAR	 OfdmRateToRxwiMCS[];
// since RT61 has better RX sensibility, we have to limit TX ACK rate not to exceed our normal data TX rate.
// otherwise the WLAN peer may not be able to receive the ACK thus downgrade its data TX rate
ULONG BasicRateMask[12]				= {0xfffff001 /* 1-Mbps */, 0xfffff003 /* 2 Mbps */, 0xfffff007 /* 5.5 */, 0xfffff00f /* 11 */,
									  0xfffff01f /* 6 */	 , 0xfffff03f /* 9 */	  , 0xfffff07f /* 12 */ , 0xfffff0ff /* 18 */,
									  0xfffff1ff /* 24 */	 , 0xfffff3ff /* 36 */	  , 0xfffff7ff /* 48 */ , 0xffffffff /* 54 */};

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
#ifdef DOT11_N_SUPPORT
UCHAR  HtCapIe = IE_HT_CAP;
UCHAR  AddHtInfoIe = IE_ADD_HT;
UCHAR  NewExtChanIe = IE_SECONDARY_CH_OFFSET;
#ifdef DOT11N_DRAFT3
UCHAR  ExtHtCapIe = IE_EXT_CAPABILITY;
#endif // DOT11N_DRAFT3 //
#endif // DOT11_N_SUPPORT //
UCHAR  ErpIe	 = IE_ERP;
UCHAR  DsIe	 = IE_DS_PARM;
UCHAR  TimIe	 = IE_TIM;
UCHAR  WpaIe	 = IE_WPA;
UCHAR  Wpa2Ie	 = IE_WPA2;
UCHAR  IbssIe	 = IE_IBSS_PARM;
UCHAR  Ccx2Ie	 = IE_CCX_V2;
UCHAR  WapiIe	 = IE_WAPI;

extern UCHAR	WPA_OUI[];

UCHAR	SES_OUI[] = {0x00, 0x90, 0x4c};

UCHAR	ZeroSsid[32] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};


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

#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		{
			BssTableInit(&pAd->ScanTab);

			// init STA state machines
			AssocStateMachineInit(pAd, &pAd->Mlme.AssocMachine, pAd->Mlme.AssocFunc);
			AuthStateMachineInit(pAd, &pAd->Mlme.AuthMachine, pAd->Mlme.AuthFunc);
			AuthRspStateMachineInit(pAd, &pAd->Mlme.AuthRspMachine, pAd->Mlme.AuthRspFunc);
			SyncStateMachineInit(pAd, &pAd->Mlme.SyncMachine, pAd->Mlme.SyncFunc);

#ifdef QOS_DLS_SUPPORT
			DlsStateMachineInit(pAd, &pAd->Mlme.DlsMachine, pAd->Mlme.DlsFunc);
#endif // QOS_DLS_SUPPORT //



			// Since we are using switch/case to implement it, the init is different from the above
			// state machine init
			MlmeCntlInit(pAd, &pAd->Mlme.CntlMachine, NULL);
		}
#endif // CONFIG_STA_SUPPORT //


		WpaStateMachineInit(pAd, &pAd->Mlme.WpaMachine, pAd->Mlme.WpaFunc);


		ActionStateMachineInit(pAd, &pAd->Mlme.ActMachine, pAd->Mlme.ActFunc);

		// Init mlme periodic timer
		RTMPInitTimer(pAd, &pAd->Mlme.PeriodicTimer, GET_TIMER_FUNCTION(MlmePeriodicExec), pAd, TRUE);

		// Set mlme periodic timer
		RTMPSetTimer(&pAd->Mlme.PeriodicTimer, MLME_TASK_EXEC_INTV);

		// software-based RX Antenna diversity
		RTMPInitTimer(pAd, &pAd->Mlme.RxAntEvalTimer, GET_TIMER_FUNCTION(AsicRxAntEvalTimeout), pAd, FALSE);


#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		{
#ifdef RTMP_PCI_SUPPORT
			if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_PCIE_DEVICE))
			{
			    // only PCIe cards need these two timers
				RTMPInitTimer(pAd, &pAd->Mlme.PsPollTimer, GET_TIMER_FUNCTION(PsPollWakeExec), pAd, FALSE);
				RTMPInitTimer(pAd, &pAd->Mlme.RadioOnOffTimer, GET_TIMER_FUNCTION(RadioOnExec), pAd, FALSE);
			}
#endif // RTMP_PCI_SUPPORT //

			RTMPInitTimer(pAd, &pAd->Mlme.LinkDownTimer, GET_TIMER_FUNCTION(LinkDownExec), pAd, FALSE);

		}
#endif // CONFIG_STA_SUPPORT //

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
	MLME_QUEUE_ELEM		   *Elem = NULL;
#ifdef APCLI_SUPPORT
	SHORT apcliIfIndex;
#endif // APCLI_SUPPORT //

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

#ifdef RALINK_ATE
		if(ATE_ON(pAd))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("The driver is in ATE mode now in MlmeHandler\n"));
			break;
		}
#endif // RALINK_ATE //

		//From message type, determine which state machine I should drive
		if (MlmeDequeue(&pAd->Mlme.Queue, &Elem))
		{

			// if dequeue success
			switch (Elem->Machine)
			{
				// STA state machines
#ifdef CONFIG_STA_SUPPORT
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

#ifdef QOS_DLS_SUPPORT
				case DLS_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.DlsMachine, Elem);
					break;
#endif // QOS_DLS_SUPPORT //

#endif // CONFIG_STA_SUPPORT //

				case ACTION_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.ActMachine, Elem);
					break;

				case WPA_STATE_MACHINE:
					StateMachinePerformAction(pAd, &pAd->Mlme.WpaMachine, Elem);
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
	BOOLEAN		  Cancelled;

	DBGPRINT(RT_DEBUG_TRACE, ("==> MlmeHalt\n"));

	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
	{
		// disable BEACON generation and other BEACON related hardware timers
		AsicDisableSync(pAd);
	}

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
#ifdef QOS_DLS_SUPPORT
		UCHAR		i;
#endif // QOS_DLS_SUPPORT //
		// Cancel pending timers
		RTMPCancelTimer(&pAd->MlmeAux.AssocTimer,		&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.ReassocTimer,		&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.DisassocTimer,	&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.AuthTimer,		&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.BeaconTimer,		&Cancelled);
		RTMPCancelTimer(&pAd->MlmeAux.ScanTimer,		&Cancelled);


#ifdef RTMP_MAC_PCI
	    if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_PCIE_DEVICE)
			&&(pAd->StaCfg.PSControl.field.EnableNewPS == TRUE))
	    {
		    RTMPCancelTimer(&pAd->Mlme.PsPollTimer,		&Cancelled);
		    RTMPCancelTimer(&pAd->Mlme.RadioOnOffTimer,		&Cancelled);
		}
#endif // RTMP_MAC_PCI //

#ifdef QOS_DLS_SUPPORT
		for (i=0; i<MAX_NUM_OF_DLS_ENTRY; i++)
		{
			RTMPCancelTimer(&pAd->StaCfg.DLSEntry[i].Timer, &Cancelled);
		}
#endif // QOS_DLS_SUPPORT //
		RTMPCancelTimer(&pAd->Mlme.LinkDownTimer,		&Cancelled);

	}
#endif // CONFIG_STA_SUPPORT //

	RTMPCancelTimer(&pAd->Mlme.PeriodicTimer,		&Cancelled);
	RTMPCancelTimer(&pAd->Mlme.RxAntEvalTimer,		&Cancelled);



	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
	{
		RTMP_CHIP_OP *pChipOps = &pAd->chipOps;

		// Set LED
		RTMPSetLED(pAd, LED_HALT);
		RTMPSetSignalLED(pAd, -100);	// Force signal strength Led to be turned off, firmware is not done it.

		if (pChipOps->AsicHaltAction)
			pChipOps->AsicHaltAction(pAd);
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
	pAd->RalinkCounters.OneSecReceivedByteCount = 0;
	pAd->RalinkCounters.OneSecTransmittedByteCount = 0;

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
	SHORT	realavgrssi;

#ifdef CONFIG_STA_SUPPORT
#ifdef RTMP_MAC_PCI
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
	    // If Hardware controlled Radio enabled, we have to check GPIO pin2 every 2 second.
		// Move code to here, because following code will return when radio is off
		if ((pAd->Mlme.PeriodicRound % (MLME_TASK_EXEC_MULTIPLE * 2) == 0) && (pAd->StaCfg.bHardwareRadio == TRUE) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))
			/*&&(pAd->bPCIclkOff == FALSE)*/)
		{
			UINT32				data = 0;

			// Read GPIO pin2 as Hardware controlled radio state
#ifndef RT3090
			RTMP_IO_READ32(pAd, GPIO_CTRL_CFG, &data);
#endif // RT3090 //
//KH(PCIE PS):Added based on Jane<--
#ifdef RT3090
// Read GPIO pin2 as Hardware controlled radio state
// We need to Read GPIO if HW said so no mater what advance power saving
if ((pAd->OpMode == OPMODE_STA) && (IDLE_ON(pAd))
	&& (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_IDLE_RADIO_OFF))
	&& (pAd->StaCfg.PSControl.field.EnablePSinIdle == TRUE))
	{
	// Want to make sure device goes to L0 state before reading register.
	RTMPPCIeLinkCtrlValueRestore(pAd, 0);
	RTMP_IO_FORCE_READ32(pAd, GPIO_CTRL_CFG, &data);
	RTMPPCIeLinkCtrlSetting(pAd, 3);
	}
else
	RTMP_IO_FORCE_READ32(pAd, GPIO_CTRL_CFG, &data);
#endif // RT3090 //
//KH(PCIE PS):Added based on Jane-->

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
#endif // RTMP_MAC_PCI //
#endif // CONFIG_STA_SUPPORT //

	// Do nothing if the driver is starting halt state.
	// This might happen when timer already been fired before cancel timer with mlmehalt
	if ((RTMP_TEST_FLAG(pAd, (fRTMP_ADAPTER_HALT_IN_PROGRESS |
								fRTMP_ADAPTER_RADIO_OFF |
								fRTMP_ADAPTER_RADIO_MEASUREMENT |
								fRTMP_ADAPTER_RESET_IN_PROGRESS))))
		return;

	RTMP_MLME_PRE_SANITY_CHECK(pAd);

#ifdef RALINK_ATE
	/* Do not show RSSI until "Normal 1 second Mlme PeriodicExec". */
	if (ATE_ON(pAd))
	{
		if (pAd->Mlme.PeriodicRound % MLME_TASK_EXEC_MULTIPLE != (MLME_TASK_EXEC_MULTIPLE - 1))
	{
			pAd->Mlme.PeriodicRound ++;
			return;
		}
	}
#endif // RALINK_ATE //

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
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
#endif // CONFIG_STA_SUPPORT //

	pAd->bUpdateBcnCntDone = FALSE;

//	RECBATimerTimeout(SystemSpecific1,FunctionContext,SystemSpecific2,SystemSpecific3);
	pAd->Mlme.PeriodicRound ++;


	// execute every 500ms
	if ((pAd->Mlme.PeriodicRound % 5 == 0) && RTMPAutoRateSwitchCheck(pAd)/*(OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_TX_RATE_SWITCH_ENABLED))*/)
	{
#ifdef CONFIG_STA_SUPPORT
		// perform dynamic tx rate switching based on past TX history
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		{
			if ((OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED)
					)
				&& (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE)))
				MlmeDynamicTxRateSwitching(pAd);
		}
#endif // CONFIG_STA_SUPPORT //
	}

	// Normal 1 second Mlme PeriodicExec.
	if (pAd->Mlme.PeriodicRound %MLME_TASK_EXEC_MULTIPLE == 0)
	{
                pAd->Mlme.OneSecPeriodicRound ++;

#ifdef RALINK_ATE
	if (ATE_ON(pAd))
	{
			/* request from Baron : move this routine from later to here */
			/* for showing Rx error count in ATE RXFRAME */
            NICUpdateRawCounters(pAd);
			if (pAd->ate.bRxFER == 1)
			{
				pAd->ate.RxTotalCnt += pAd->ate.RxCntPerSec;
			    ate_print(KERN_EMERG "MlmePeriodicExec: Rx packet cnt = %d/%d\n", pAd->ate.RxCntPerSec, pAd->ate.RxTotalCnt);
				pAd->ate.RxCntPerSec = 0;

				if (pAd->ate.RxAntennaSel == 0)
					ate_print(KERN_EMERG "MlmePeriodicExec: Rx AvgRssi0=%d, AvgRssi1=%d, AvgRssi2=%d\n\n",
						pAd->ate.AvgRssi0, pAd->ate.AvgRssi1, pAd->ate.AvgRssi2);
				else
					ate_print(KERN_EMERG "MlmePeriodicExec: Rx AvgRssi=%d\n\n", pAd->ate.AvgRssi0);
			}
			MlmeResetRalinkCounters(pAd);



			return;
	}
#endif // RALINK_ATE //



		//ORIBATimerTimeout(pAd);

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


#ifdef DOT11_N_SUPPORT
		// Need statistics after read counter. So put after NICUpdateRawCounters
		ORIBATimerTimeout(pAd);
#endif // DOT11_N_SUPPORT //

		// if MGMT RING is full more than twice within 1 second, we consider there's
		// a hardware problem stucking the TX path. In this case, try a hardware reset
		// to recover the system
	//	if (pAd->RalinkCounters.MgmtRingFullCount >= 2)
	//		RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_HARDWARE_ERROR);
	//	else
	//		pAd->RalinkCounters.MgmtRingFullCount = 0;

		// The time period for checking antenna is according to traffic
#ifdef ANT_DIVERSITY_SUPPORT
		if ((pAd->NicConfig2.field.AntDiversity) &&
			(pAd->CommonCfg.bRxAntDiversity == ANT_DIVERSITY_ENABLE) &&
			(!pAd->EepromAccess))
			AsicAntennaSelect(pAd, pAd->MlmeAux.Channel);
		else if(pAd->CommonCfg.bRxAntDiversity == ANT_FIX_ANT1 || pAd->CommonCfg.bRxAntDiversity == ANT_FIX_ANT2)
		{
#ifdef CONFIG_STA_SUPPORT
			realavgrssi = (pAd->RxAnt.Pair1AvgRssi[pAd->RxAnt.Pair1PrimaryRxAnt] >> 3);
#endif // CONFIG_STA_SUPPORT //
			DBGPRINT(RT_DEBUG_TRACE,("Ant-realrssi0(%d), Lastrssi0(%d), EvaluateStableCnt=%d\n", realavgrssi, pAd->RxAnt.Pair1LastAvgRssi, pAd->RxAnt.EvaluateStableCnt));
		}
		else
#endif // ANT_DIVERSITY_SUPPORT //
		{
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
		}

#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
			STAMlmePeriodicExec(pAd);
#endif // CONFIG_STA_SUPPORT //

		MlmeResetRalinkCounters(pAd);

#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		{
#ifdef RTMP_MAC_PCI
			if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST) && (pAd->bPCIclkOff == FALSE))
#endif // RTMP_MAC_PCI //
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
#endif // CONFIG_STA_SUPPORT //

		RTMP_MLME_HANDLER(pAd);
	}


	pAd->bUpdateBcnCntDone = FALSE;
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

#ifdef CONFIG_STA_SUPPORT
		if ((pAd->OpMode == OPMODE_STA) && ADHOC_ON(pAd))
		{
#ifdef DOT11_N_SUPPORT
			if ((pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED) &&
				(pEntry->HTCapability.MCSSet[0] == 0xff) &&
				((pEntry->HTCapability.MCSSet[1] == 0x00) || (pAd->Antenna.field.TxPath == 1)))
			{// 11N 1S Adhoc
				*ppTable = RateSwitchTable11N1S;
				*pTableSize = RateSwitchTable11N1S[0];
				*pInitTxRateIdx = RateSwitchTable11N1S[1];

			}
			else if ((pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED) &&
					(pEntry->HTCapability.MCSSet[0] == 0xff) &&
					(pEntry->HTCapability.MCSSet[1] == 0xff) &&
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
#endif // DOT11_N_SUPPORT //
				if ((pEntry->RateLen == 4)
#ifdef DOT11_N_SUPPORT
					&& (pEntry->HTCapability.MCSSet[0] == 0) && (pEntry->HTCapability.MCSSet[1] == 0)
#endif // DOT11_N_SUPPORT //
					)
			{
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
#endif // CONFIG_STA_SUPPORT //

#ifdef DOT11_N_SUPPORT
		//if ((pAd->StaActive.SupRateLen + pAd->StaActive.ExtRateLen == 12) && (pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0xff) &&
		//	((pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0x00) || (pAd->Antenna.field.TxPath == 1)))
		if (((pEntry->RateLen == 12) || (pAd->OpMode == OPMODE_STA)) && (pEntry->HTCapability.MCSSet[0] == 0xff) &&
			((pEntry->HTCapability.MCSSet[1] == 0x00) || (pAd->CommonCfg.TxStream == 1)))
		{// 11BGN 1S AP
			*ppTable = RateSwitchTable11BGN1S;
			*pTableSize = RateSwitchTable11BGN1S[0];
			*pInitTxRateIdx = RateSwitchTable11BGN1S[1];

			break;
		}

		//else if ((pAd->StaActive.SupRateLen + pAd->StaActive.ExtRateLen == 12) && (pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0xff) &&
		//	(pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0xff) && (pAd->Antenna.field.TxPath == 2))
		if (((pEntry->RateLen == 12) || (pAd->OpMode == OPMODE_STA)) && (pEntry->HTCapability.MCSSet[0] == 0xff) &&
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

		//else if ((pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0xff) && ((pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0x00) || (pAd->Antenna.field.TxPath == 1)))
		if ((pEntry->HTCapability.MCSSet[0] == 0xff) && ((pEntry->HTCapability.MCSSet[1] == 0x00) || (pAd->CommonCfg.TxStream == 1)))
		{// 11N 1S AP
			*ppTable = RateSwitchTable11N1S;
			*pTableSize = RateSwitchTable11N1S[0];
			*pInitTxRateIdx = RateSwitchTable11N1S[1];

			break;
		}

		//else if ((pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0xff) && (pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0xff) && (pAd->Antenna.field.TxPath == 2))
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
#endif // DOT11_N_SUPPORT //
		//else if ((pAd->StaActive.SupRateLen == 4) && (pAd->StaActive.ExtRateLen == 0) && (pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0) && (pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0))
		if ((pEntry->RateLen == 4 || pAd->CommonCfg.PhyMode==PHY_11B)
#ifdef DOT11_N_SUPPORT
		//Iverson mark for Adhoc b mode,sta will use rate 54  Mbps when connect with sta b/g/n mode
		/* && (pEntry->HTCapability.MCSSet[0] == 0) && (pEntry->HTCapability.MCSSet[1] == 0)*/
#endif // DOT11_N_SUPPORT //
			)
		{// B only AP
			*ppTable = RateSwitchTable11B;
			*pTableSize = RateSwitchTable11B[0];
			*pInitTxRateIdx = RateSwitchTable11B[1];

			break;
		}

		//else if ((pAd->StaActive.SupRateLen + pAd->StaActive.ExtRateLen > 8) && (pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0) && (pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0))
		if ((pEntry->RateLen > 8)
#ifdef DOT11_N_SUPPORT
			&& (pEntry->HTCapability.MCSSet[0] == 0) && (pEntry->HTCapability.MCSSet[1] == 0)
#endif // DOT11_N_SUPPORT //
			)
		{// B/G  mixed AP
			*ppTable = RateSwitchTable11BG;
			*pTableSize = RateSwitchTable11BG[0];
			*pInitTxRateIdx = RateSwitchTable11BG[1];

			break;
		}

		//else if ((pAd->StaActive.SupRateLen + pAd->StaActive.ExtRateLen == 8) && (pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0) && (pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0))
		if ((pEntry->RateLen == 8)
#ifdef DOT11_N_SUPPORT
			&& (pEntry->HTCapability.MCSSet[0] == 0) && (pEntry->HTCapability.MCSSet[1] == 0)
#endif // DOT11_N_SUPPORT //
			)
		{// G only AP
			*ppTable = RateSwitchTable11G;
			*pTableSize = RateSwitchTable11G[0];
			*pInitTxRateIdx = RateSwitchTable11G[1];

			break;
		}
#ifdef DOT11_N_SUPPORT
#endif // DOT11_N_SUPPORT //

#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		{
#ifdef DOT11_N_SUPPORT
			//else if ((pAd->StaActive.SupportedPhyInfo.MCSSet[0] == 0) && (pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0))
			if ((pEntry->HTCapability.MCSSet[0] == 0) && (pEntry->HTCapability.MCSSet[1] == 0))
#endif // DOT11_N_SUPPORT //
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
#ifdef DOT11_N_SUPPORT
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
#endif // DOT11_N_SUPPORT //
			DBGPRINT_RAW(RT_DEBUG_ERROR,("DRS: unkown mode (SupRateLen=%d, ExtRateLen=%d, MCSSet[0]=0x%x, MCSSet[1]=0x%x)\n",
				pAd->StaActive.SupRateLen, pAd->StaActive.ExtRateLen, pAd->StaActive.SupportedPhyInfo.MCSSet[0], pAd->StaActive.SupportedPhyInfo.MCSSet[1]));
		}
#endif // CONFIG_STA_SUPPORT //
	} while(FALSE);
}


#ifdef CONFIG_STA_SUPPORT
VOID STAMlmePeriodicExec(
	PRTMP_ADAPTER pAd)
{
	ULONG			    TxTotalCnt;
	int	i;




	/*
		We return here in ATE mode, because the statistics
		that ATE need are not collected via this routine.
	*/
#ifdef RALINK_ATE
	if (ATE_ON(pAd))
	return;
#endif // RALINK_ATE //

#ifdef RALINK_ATE
	// It is supposed that we will never reach here in ATE mode.
	ASSERT(!(ATE_ON(pAd)));
	if (ATE_ON(pAd))
		return;
#endif // RALINK_ATE //

#ifdef PCIE_PS_SUPPORT
// don't perform idle-power-save mechanism within 3 min after driver initialization.
// This can make rebooter test more robust
if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_PCIE_DEVICE))
	{
	if ((pAd->OpMode == OPMODE_STA) && (IDLE_ON(pAd))
		&& (pAd->Mlme.SyncMachine.CurrState == SYNC_IDLE)
		&& (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE)
		&& (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_IDLE_RADIO_OFF)))
		{
		if (IS_RT3090(pAd)|| IS_RT3572(pAd) || IS_RT3390(pAd))
			{
			if (pAd->StaCfg.PSControl.field.EnableNewPS == TRUE)
				{
				DBGPRINT(RT_DEBUG_TRACE, ("%s::%d\n",__FUNCTION__,__LINE__));

				RT28xxPciAsicRadioOff(pAd, GUI_IDLE_POWER_SAVE, 0);
				}
			else
				{
				DBGPRINT(RT_DEBUG_TRACE, ("%s::%d\n",__FUNCTION__,__LINE__));
				AsicSendCommandToMcu(pAd, 0x30, PowerSafeCID, 0xff, 0x2);
				// Wait command success
				AsicCheckCommanOk(pAd, PowerSafeCID);
				RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_IDLE_RADIO_OFF);
				DBGPRINT(RT_DEBUG_TRACE, ("PSM - rt30xx Issue Sleep command)\n"));
				}
			}
		else if (pAd->Mlme.OneSecPeriodicRound > 180)
			{
			if (pAd->StaCfg.PSControl.field.EnableNewPS == TRUE)
				{
				DBGPRINT(RT_DEBUG_TRACE, ("%s::%d\n",__FUNCTION__,__LINE__));
				RT28xxPciAsicRadioOff(pAd, GUI_IDLE_POWER_SAVE, 0);
				}
			else
				{
				DBGPRINT(RT_DEBUG_TRACE, ("%s::%d\n",__FUNCTION__,__LINE__));
				AsicSendCommandToMcu(pAd, 0x30, PowerSafeCID, 0xff, 0x02);
				// Wait command success
				AsicCheckCommanOk(pAd, PowerSafeCID);
				RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_IDLE_RADIO_OFF);
				DBGPRINT(RT_DEBUG_TRACE, ("PSM -  rt28xx Issue Sleep command)\n"));
				}
			}
		}
	else
		{
		DBGPRINT(RT_DEBUG_TRACE,("STAMlmePeriodicExec MMCHK - CommonCfg.Ssid[%d]=%c%c%c%c... MlmeAux.Ssid[%d]=%c%c%c%c...\n",
			pAd->CommonCfg.SsidLen, pAd->CommonCfg.Ssid[0], pAd->CommonCfg.Ssid[1], pAd->CommonCfg.Ssid[2], pAd->CommonCfg.Ssid[3],
			pAd->MlmeAux.SsidLen, pAd->MlmeAux.Ssid[0], pAd->MlmeAux.Ssid[1], pAd->MlmeAux.Ssid[2], pAd->MlmeAux.Ssid[3]));
		}
	}
#endif // PCIE_PS_SUPPORT //


#ifdef WPA_SUPPLICANT_SUPPORT
    if (pAd->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_DISABLE)
#endif // WPA_SUPPLICANT_SUPPORT //
    {
	// WPA MIC error should block association attempt for 60 seconds
		if (pAd->StaCfg.bBlockAssoc &&
			RTMP_TIME_AFTER(pAd->Mlme.Now32, pAd->StaCfg.LastMicErrorTime + (60*OS_HZ)))
		pAd->StaCfg.bBlockAssoc = FALSE;
    }

    if ((pAd->PreMediaState != pAd->IndicateMediaState) && (pAd->CommonCfg.bWirelessEvent))
	{
		if (pAd->IndicateMediaState == NdisMediaStateConnected)
		{
			RTMPSendWirelessEvent(pAd, IW_STA_LINKUP_EVENT_FLAG, pAd->MacTab.Content[BSSID_WCID].Addr, BSS0, 0);
		}
		pAd->PreMediaState = pAd->IndicateMediaState;
	}




	if (pAd->CommonCfg.PSPXlink && ADHOC_ON(pAd))
	{
	}
	else
	{
	AsicStaBbpTuning(pAd);
	}

	TxTotalCnt = pAd->RalinkCounters.OneSecTxNoRetryOkCount +
					 pAd->RalinkCounters.OneSecTxRetryOkCount +
					 pAd->RalinkCounters.OneSecTxFailCount;

	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		// update channel quality for Roaming and UI LinkQuality display
		MlmeCalculateChannelQuality(pAd, NULL, pAd->Mlme.Now32);
	}

	// must be AFTER MlmeDynamicTxRateSwitching() because it needs to know if
	// Radio is currently in noisy environment
	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
	AsicAdjustTxPower(pAd);

	if (INFRA_ON(pAd))
	{
#ifdef QOS_DLS_SUPPORT
		// Check DLS time out, then tear down those session
		RTMPCheckDLSTimeOut(pAd);
#endif // QOS_DLS_SUPPORT //

		// Is PSM bit consistent with user power management policy?
		// This is the only place that will set PSM bit ON.
		if (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE))
		MlmeCheckPsmChange(pAd, pAd->Mlme.Now32);

		pAd->RalinkCounters.LastOneSecTotalTxCount = TxTotalCnt;

		if ((RTMP_TIME_AFTER(pAd->Mlme.Now32, pAd->StaCfg.LastBeaconRxTime + (1*OS_HZ))) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)) &&
			(((TxTotalCnt + pAd->RalinkCounters.OneSecRxOkCnt) < 600)))
		{
			RTMPSetAGCInitValue(pAd, BW_20);
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - No BEACON. restore R66 to the low bound(%d) \n", (0x2E + GET_LNA_GAIN(pAd))));
		}

        //if ((pAd->RalinkCounters.OneSecTxNoRetryOkCount == 0) &&
        //    (pAd->RalinkCounters.OneSecTxRetryOkCount == 0))
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

			// Lost AP, send disconnect & link down event
			LinkDown(pAd, FALSE);

#ifdef WPA_SUPPLICANT_SUPPORT
#ifndef NATIVE_WPA_SUPPLICANT_SUPPORT
		//send disassociate event to wpa_supplicant
		if (pAd->StaCfg.WpaSupplicantUP) {
			RtmpOSWrielessEventSend(pAd, IWEVCUSTOM, RT_DISASSOC_EVENT_FLAG, NULL, NULL, 0);
		}
#endif // NATIVE_WPA_SUPPLICANT_SUPPORT //
#endif // WPA_SUPPLICANT_SUPPORT //

#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
		RtmpOSWrielessEventSend(pAd, SIOCGIWAP, -1, NULL, NULL, 0);
#endif // NATIVE_WPA_SUPPLICANT_SUPPORT //

			// RTMPPatchMacBbpBug(pAd);
			MlmeAutoReconnectLastSSID(pAd);
		}
		else if (CQI_IS_BAD(pAd->Mlme.ChannelQuality))
		{
			pAd->RalinkCounters.BadCQIAutoRecoveryCount ++;
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - Bad CQI. Auto Recovery attempt #%ld\n", pAd->RalinkCounters.BadCQIAutoRecoveryCount));
			MlmeAutoReconnectLastSSID(pAd);
		}

		if (pAd->StaCfg.bAutoRoaming)
		{
			BOOLEAN	rv = FALSE;
			CHAR	dBmToRoam = pAd->StaCfg.dBmToRoam;
			CHAR	MaxRssi = RTMPMaxRssi(pAd,
										  pAd->StaCfg.RssiSample.LastRssi0,
										  pAd->StaCfg.RssiSample.LastRssi1,
										  pAd->StaCfg.RssiSample.LastRssi2);

			// Scanning, ignore Roaming
			if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS) &&
				(pAd->Mlme.SyncMachine.CurrState == SYNC_IDLE) &&
				(MaxRssi <= dBmToRoam))
			{
				DBGPRINT(RT_DEBUG_TRACE, ("Rssi=%d, dBmToRoam=%d\n", MaxRssi, (CHAR)dBmToRoam));


				// Add auto seamless roaming
				if (rv == FALSE)
					rv = MlmeCheckForFastRoaming(pAd);

				if (rv == FALSE)
				{
					if ((pAd->StaCfg.LastScanTime + 10 * OS_HZ) < pAd->Mlme.Now32)
					{
						DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - Roaming, No eligable entry, try new scan!\n"));
						pAd->StaCfg.ScanCnt = 2;
						pAd->StaCfg.LastScanTime = pAd->Mlme.Now32;
						MlmeAutoScan(pAd);
					}
				}
			}
		}
	}
	else if (ADHOC_ON(pAd))
	{

		// If all peers leave, and this STA becomes the last one in this IBSS, then change MediaState
		// to DISCONNECTED. But still holding this IBSS (i.e. sending BEACON) so that other STAs can
		// join later.
		if (RTMP_TIME_AFTER(pAd->Mlme.Now32, pAd->StaCfg.LastBeaconRxTime + ADHOC_BEACON_LOST_TIME) &&
			OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
		{
			MLME_START_REQ_STRUCT     StartReq;

			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - excessive BEACON lost, last STA in this IBSS, MediaState=Disconnected\n"));
			LinkDown(pAd, FALSE);

			StartParmFill(pAd, &StartReq, (CHAR *)pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_START_REQ, sizeof(MLME_START_REQ_STRUCT), &StartReq);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_START;
		}

		for (i = 1; i < MAX_LEN_OF_MAC_TABLE; i++)
		{
			MAC_TABLE_ENTRY *pEntry = &pAd->MacTab.Content[i];

			if (pEntry->ValidAsCLI == FALSE)
				continue;

			if (RTMP_TIME_AFTER(pAd->Mlme.Now32, pEntry->LastBeaconRxTime + ADHOC_BEACON_LOST_TIME))
				MacTableDeleteEntry(pAd, pEntry->Aid, pEntry->Addr);
		}
	}
	else // no INFRA nor ADHOC connection
	{

		if (pAd->StaCfg.bScanReqIsFromWebUI &&
			RTMP_TIME_BEFORE(pAd->Mlme.Now32, pAd->StaCfg.LastScanTime + (30 * OS_HZ)))
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

				if (RTMP_TIME_AFTER(pAd->Mlme.Now32, pAd->StaCfg.LastScanTime + (10 * OS_HZ)))
				{
					DBGPRINT(RT_DEBUG_TRACE, ("STAMlmePeriodicExec():CNTL - ScanTab.BssNr==0, start a new ACTIVE scan SSID[%s]\n", pAd->MlmeAux.AutoReconnectSsid));
					ScanParmFill(pAd, &ScanReq, (PSTRING) pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen, BSS_ANY, SCAN_ACTIVE);
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
#ifdef CARRIER_DETECTION_SUPPORT // Roger sync Carrier
					if (pAd->CommonCfg.CarrierDetect.Enable == TRUE)
					{
						if ((pAd->Mlme.OneSecPeriodicRound % 5) == 1)
							MlmeAutoReconnectLastSSID(pAd);
					}
					else
#endif // CARRIER_DETECTION_SUPPORT //
						MlmeAutoReconnectLastSSID(pAd);
				}
			}
		}
	}

SKIP_AUTO_SCAN_CONN:

#ifdef DOT11_N_SUPPORT
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
#endif // DOT11_N_SUPPORT //


#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_SCAN_2040))
		TriEventCounterMaintenance(pAd);
#endif // DOT11N_DRAFT3 //
#endif // DOT11_N_SUPPORT //

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

	if (pAd != NULL)
	{
		MLME_DISASSOC_REQ_STRUCT   DisassocReq;

		if ((pAd->StaCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED) &&
			(INFRA_ON(pAd)))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("LinkDownExec(): disassociate with current AP...\n"));
			DisassocParmFill(pAd, &DisassocReq, pAd->CommonCfg.Bssid, REASON_DISASSOC_STA_LEAVING);
			MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_DISASSOC_REQ,
						sizeof(MLME_DISASSOC_REQ_STRUCT), &DisassocReq);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_DISASSOC;

			pAd->IndicateMediaState = NdisMediaStateDisconnected;
			RTMP_IndicateMediaState(pAd);
		    pAd->ExtraInfo = GENERAL_LINK_DOWN;
		}
	}
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
					pAd->MlmeAux.AutoReconnectSsidLen,
					pAd->MlmeAux.AutoReconnectSsid);
		RTMP_MLME_HANDLER(pAd);
	}
}

// IRQL = DISPATCH_LEVEL
VOID MlmeAutoReconnectLastSSID(
	IN PRTMP_ADAPTER pAd)
{
	if (pAd->StaCfg.bAutoConnectByBssid)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("Driver auto reconnect to last OID_802_11_BSSID setting - %02X:%02X:%02X:%02X:%02X:%02X\n",
									pAd->MlmeAux.Bssid[0],
									pAd->MlmeAux.Bssid[1],
									pAd->MlmeAux.Bssid[2],
									pAd->MlmeAux.Bssid[3],
									pAd->MlmeAux.Bssid[4],
									pAd->MlmeAux.Bssid[5]));

		pAd->MlmeAux.Channel = pAd->CommonCfg.Channel;
		MlmeEnqueue(pAd,
			 MLME_CNTL_STATE_MACHINE,
			 OID_802_11_BSSID,
			 MAC_ADDR_LEN,
			 pAd->MlmeAux.Bssid);

		pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;

		RTMP_MLME_HANDLER(pAd);
	}
	// check CntlMachine.CurrState to avoid collision with NDIS SetOID request
	else if ((pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE) &&
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
		RTMP_MLME_HANDLER(pAd);
	}
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

		if ((pBss->LastBeaconRxTime + pAd->StaCfg.BeaconLostTime) < Now32)
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
			RTMP_MLME_HANDLER(pAd);
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
BOOLEAN MlmeCheckForFastRoaming(
	IN	PRTMP_ADAPTER	pAd)
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

	DBGPRINT(RT_DEBUG_TRACE, ("<== MlmeCheckForFastRoaming (BssNr=%d)\n", pRoamTab->BssNr));
	if (pRoamTab->BssNr > 0)
	{
		// check CntlMachine.CurrState to avoid collision with NDIS SetOID request
		if (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE)
		{
			pAd->RalinkCounters.PoorCQIRoamingCount ++;
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - Roaming attempt #%ld\n", pAd->RalinkCounters.PoorCQIRoamingCount));
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_MLME_ROAMING_REQ, 0, NULL);
			RTMP_MLME_HANDLER(pAd);
			return TRUE;
		}
	}

	return FALSE;
}

VOID MlmeSetTxRate(
	IN PRTMP_ADAPTER		pAd,
	IN PMAC_TABLE_ENTRY		pEntry,
	IN PRTMP_TX_RATE_SWITCH	pTxRate)
{
	UCHAR	MaxMode = MODE_OFDM;

#ifdef DOT11_N_SUPPORT
	MaxMode = MODE_HTGREENFIELD;

	if (pTxRate->STBC && (pAd->StaCfg.MaxHTPhyMode.field.STBC) && (pAd->Antenna.field.TxPath == 2))
		pAd->StaCfg.HTPhyMode.field.STBC = STBC_USE;
	else
#endif // DOT11_N_SUPPORT //
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

#ifdef DOT11_N_SUPPORT
        if (pTxRate->ShortGI && (pAd->StaCfg.MaxHTPhyMode.field.ShortGI))
			pAd->StaCfg.HTPhyMode.field.ShortGI = GI_400;
		else
#endif // DOT11_N_SUPPORT //
			pAd->StaCfg.HTPhyMode.field.ShortGI = GI_800;

#ifdef DOT11_N_SUPPORT
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
#endif // DOT11_N_SUPPORT //

		pEntry->HTPhyMode.field.STBC	= pAd->StaCfg.HTPhyMode.field.STBC;
		pEntry->HTPhyMode.field.ShortGI	= pAd->StaCfg.HTPhyMode.field.ShortGI;
		pEntry->HTPhyMode.field.MCS		= pAd->StaCfg.HTPhyMode.field.MCS;
		pEntry->HTPhyMode.field.MODE	= pAd->StaCfg.HTPhyMode.field.MODE;
#ifdef DOT11_N_SUPPORT
        if ((pAd->StaCfg.MaxHTPhyMode.field.MODE == MODE_HTGREENFIELD) &&
            pAd->WIFItestbed.bGreenField)
            pEntry->HTPhyMode.field.MODE = MODE_HTGREENFIELD;
#endif // DOT11_N_SUPPORT //
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
	BOOLEAN					bTxRateChanged = FALSE, bUpgradeQuality = FALSE;
	PRTMP_TX_RATE_SWITCH	pCurrTxRate, pNextTxRate = NULL;
	PUCHAR					pTable;
	UCHAR					TableSize = 0;
	UCHAR					InitTxRateIdx = 0, TrainUp, TrainDown;
	CHAR					Rssi, RssiOffset = 0;
	TX_STA_CNT1_STRUC		StaTx1;
	TX_STA_CNT0_STRUC		TxStaCnt0;
	ULONG					TxRetransmit = 0, TxSuccess = 0, TxFailCount = 0;
	MAC_TABLE_ENTRY			*pEntry;
	RSSI_SAMPLE				*pRssi = &pAd->StaCfg.RssiSample;

#ifdef RALINK_ATE
	if (ATE_ON(pAd))
	{
		return;
	}
#endif // RALINK_ATE //

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
			Rssi = RTMPMaxRssi(pAd,
							   pRssi->AvgRssi0,
							   pRssi->AvgRssi1,
							   pRssi->AvgRssi2);

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
			if (INFRA_ON(pAd) && (i == 1))
				Rssi = RTMPMaxRssi(pAd,
								   pRssi->AvgRssi0,
								   pRssi->AvgRssi1,
								   pRssi->AvgRssi2);
			else
				Rssi = RTMPMaxRssi(pAd,
								   pEntry->RssiSample.AvgRssi0,
								   pEntry->RssiSample.AvgRssi1,
								   pEntry->RssiSample.AvgRssi2);

			TxTotalCnt = pEntry->OneSecTxNoRetryOkCount +
				 pEntry->OneSecTxRetryOkCount +
				 pEntry->OneSecTxFailCount;

			if (TxTotalCnt)
				TxErrorRatio = ((pEntry->OneSecTxRetryOkCount + pEntry->OneSecTxFailCount) * 100) / TxTotalCnt;
		}

		if (TxTotalCnt)
		{
			/*
				Three AdHoc connections can not work normally if one AdHoc connection is disappeared from a heavy traffic environment generated by ping tool
				We force to set LongRtyLimit and ShortRtyLimit to 0 to stop retransmitting packet, after a while, resoring original settings
			*/
			if (TxErrorRatio == 100)
			{
				TX_RTY_CFG_STRUC	TxRtyCfg,TxRtyCfgtmp;
				ULONG	Index;
				ULONG	MACValue;

				RTMP_IO_READ32(pAd, TX_RTY_CFG, &TxRtyCfg.word);
				TxRtyCfgtmp.word = TxRtyCfg.word;
				TxRtyCfg.field.LongRtyLimit = 0x0;
				TxRtyCfg.field.ShortRtyLimit = 0x0;
				RTMP_IO_WRITE32(pAd, TX_RTY_CFG, TxRtyCfg.word);

				RTMPusecDelay(1);

				Index = 0;
				MACValue = 0;
				do
				{
					RTMP_IO_READ32(pAd, TXRXQ_PCNT, &MACValue);
					if ((MACValue & 0xffffff) == 0)
						break;
					Index++;
					RTMPusecDelay(1000);
				}while((Index < 330)&&(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)));

				RTMP_IO_READ32(pAd, TX_RTY_CFG, &TxRtyCfg.word);
				TxRtyCfg.field.LongRtyLimit = TxRtyCfgtmp.field.LongRtyLimit;
				TxRtyCfg.field.ShortRtyLimit = TxRtyCfgtmp.field.ShortRtyLimit;
				RTMP_IO_WRITE32(pAd, TX_RTY_CFG, TxRtyCfg.word);
			}
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

#ifdef DOT11_N_SUPPORT
		if ((Rssi > -65) && (pCurrTxRate->Mode >= MODE_HTMIX))
		{
			TrainUp		= (pCurrTxRate->TrainUp + (pCurrTxRate->TrainUp >> 1));
			TrainDown	= (pCurrTxRate->TrainDown + (pCurrTxRate->TrainDown >> 1));
		}
		else
#endif // DOT11_N_SUPPORT //
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
				//else if ((pCurrTxRate->CurrMCS == MCS_15)/* && (pCurrTxRate->ShortGI == GI_800)*/)	//we hope to use ShortGI as initial rate
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
#ifdef DOT11_N_SUPPORT
			/*if (MCS15)*/
			if ((pTable == RateSwitchTable11BGN3S) ||
				(pTable == RateSwitchTable11N3S) ||
				(pTable == RateSwitchTable))
			{// N mode with 3 stream // 3*3
				if (MCS23 && (Rssi >= -70))
					TxRateIdx = MCS23;
				else if (MCS22 && (Rssi >= -72))
					TxRateIdx = MCS22;
		    else if (MCS21 && (Rssi >= -76))
					TxRateIdx = MCS21;
				else if (MCS20 && (Rssi >= -78))
					TxRateIdx = MCS20;
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
//		else if ((pTable == RateSwitchTable11BGN2S) || (pTable == RateSwitchTable11BGN2SForABand) ||(pTable == RateSwitchTable11N2S) ||(pTable == RateSwitchTable11N2SForABand) || (pTable == RateSwitchTable))
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
#endif // DOT11_N_SUPPORT //
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

	//		if (TxRateIdx != pAd->CommonCfg.TxRateIndex)
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

		{
			UCHAR tmpTxRate;

			// to fix tcp ack issue
			if (!bTxRateChanged && (pAd->RalinkCounters.OneSecReceivedByteCount > (pAd->RalinkCounters.OneSecTransmittedByteCount * 5)))
			{
				tmpTxRate = DownRateIdx;
				DBGPRINT_RAW(RT_DEBUG_TRACE,("DRS: Rx(%d) is 5 times larger than Tx(%d), use low rate (curr=%d, tmp=%d)\n",
					pAd->RalinkCounters.OneSecReceivedByteCount, pAd->RalinkCounters.OneSecTransmittedByteCount, pEntry->CurrTxRateIndex, tmpTxRate));
			}
			else
			{
				tmpTxRate = pEntry->CurrTxRateIndex;
			}

			pNextTxRate = (PRTMP_TX_RATE_SWITCH) &pTable[(tmpTxRate+1)*5];
			if (bTxRateChanged && pNextTxRate)
			{
				MlmeSetTxRate(pAd, pEntry, pNextTxRate);
			}
		}
		// reset all OneSecTx counters
		RESET_ONE_SEC_TX_CNT(pEntry);
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
	BOOLEAN					bTxRateChanged; //, bUpgradeQuality = FALSE;
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

#ifdef DOT11_N_SUPPORT
	if ((Rssi > -65) && (pCurrTxRate->Mode >= MODE_HTMIX))
	{
		TrainUp		= (pCurrTxRate->TrainUp + (pCurrTxRate->TrainUp >> 1));
		TrainDown	= (pCurrTxRate->TrainDown + (pCurrTxRate->TrainDown >> 1));
	}
	else
#endif // DOT11_N_SUPPORT //
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
			bTxRateChanged = TRUE;
	}
	// if rate-down happen, only clear DownRate's bad history
	else if (pAd->CommonCfg.TxRateIndex < CurrRateIdx)
	{
		DBGPRINT_RAW(RT_DEBUG_TRACE,("QuickDRS: --TX rate from %d to %d \n", CurrRateIdx, pAd->CommonCfg.TxRateIndex));

		pAd->DrsCounters.TxRateUpPenalty = 0;           // no penalty
		pAd->DrsCounters.TxQuality[pAd->CommonCfg.TxRateIndex] = 0;
		pAd->DrsCounters.PER[pAd->CommonCfg.TxRateIndex] = 0;
			bTxRateChanged = TRUE;
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
	PowerMode = pAd->StaCfg.WindowsPowerMode;

	if (INFRA_ON(pAd) &&
		(PowerMode != Ndis802_11PowerModeCAM) &&
		(pAd->StaCfg.Psm == PWR_ACTIVE) &&
//		(! RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
		(pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE)&&
		RTMP_TEST_PSFLAG(pAd, fRTMP_PS_CAN_GO_SLEEP)
		 /*&&
		(pAd->RalinkCounters.OneSecTxNoRetryOkCount == 0) &&
		(pAd->RalinkCounters.OneSecTxRetryOkCount == 0)*/)
	{
		NdisGetSystemUpTime(&pAd->Mlme.LastSendNULLpsmTime);
		pAd->RalinkCounters.RxCountSinceLastNULL = 0;
		RTMP_SET_PSM_BIT(pAd, PWR_SAVE);
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
#endif // CONFIG_STA_SUPPORT //

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
	IN PMAC_TABLE_ENTRY pMacEntry,
	IN ULONG Now32)
{
	ULONG TxOkCnt, TxCnt, TxPER, TxPRR;
	ULONG RxCnt, RxPER;
	UCHAR NorRssi;
	CHAR  MaxRssi;
	RSSI_SAMPLE *pRssiSample = NULL;
	UINT32 OneSecTxNoRetryOkCount = 0;
	UINT32 OneSecTxRetryOkCount = 0;
	UINT32 OneSecTxFailCount = 0;
	UINT32 OneSecRxOkCnt = 0;
	UINT32 OneSecRxFcsErrCnt = 0;
	ULONG ChannelQuality = 0;  // 0..100, Channel Quality Indication for Roaming
#ifdef CONFIG_STA_SUPPORT
	ULONG BeaconLostTime = pAd->StaCfg.BeaconLostTime;
#endif // CONFIG_STA_SUPPORT //

#ifdef CONFIG_STA_SUPPORT
#ifdef CARRIER_DETECTION_SUPPORT // Roger sync Carrier
	// longer beacon lost time when carrier detection enabled
	if (pAd->CommonCfg.CarrierDetect.Enable == TRUE)
	{
		BeaconLostTime = pAd->StaCfg.BeaconLostTime + (pAd->StaCfg.BeaconLostTime/2);
	}
#endif // CARRIER_DETECTION_SUPPORT //
#endif // CONFIG_STA_SUPPORT //

#ifdef CONFIG_STA_SUPPORT
	if (pAd->OpMode == OPMODE_STA)
	{
		pRssiSample = &pAd->StaCfg.RssiSample;
		OneSecTxNoRetryOkCount = pAd->RalinkCounters.OneSecTxNoRetryOkCount;
		OneSecTxRetryOkCount = pAd->RalinkCounters.OneSecTxRetryOkCount;
		OneSecTxFailCount = pAd->RalinkCounters.OneSecTxFailCount;
		OneSecRxOkCnt = pAd->RalinkCounters.OneSecRxOkCnt;
		OneSecRxFcsErrCnt = pAd->RalinkCounters.OneSecRxFcsErrCnt;
	}
#endif // CONFIG_STA_SUPPORT //

	MaxRssi = RTMPMaxRssi(pAd, pRssiSample->LastRssi0,
								pRssiSample->LastRssi1,
								pRssiSample->LastRssi2);

	//
	// calculate TX packet error ratio and TX retry ratio - if too few TX samples, skip TX related statistics
	//
	TxOkCnt = OneSecTxNoRetryOkCount + OneSecTxRetryOkCount;
	TxCnt = TxOkCnt + OneSecTxFailCount;
	if (TxCnt < 5)
	{
		TxPER = 0;
		TxPRR = 0;
	}
	else
	{
		TxPER = (OneSecTxFailCount * 100) / TxCnt;
		TxPRR = ((TxCnt - OneSecTxNoRetryOkCount) * 100) / TxCnt;
	}

	//
	// calculate RX PER - don't take RxPER into consideration if too few sample
	//
	RxCnt = OneSecRxOkCnt + OneSecRxFcsErrCnt;
	if (RxCnt < 5)
		RxPER = 0;
	else
		RxPER = (OneSecRxFcsErrCnt * 100) / RxCnt;

	//
	// decide ChannelQuality based on: 1)last BEACON received time, 2)last RSSI, 3)TxPER, and 4)RxPER
	//
#ifdef CONFIG_STA_SUPPORT
	if ((pAd->OpMode == OPMODE_STA) &&
		INFRA_ON(pAd) &&
		(OneSecTxNoRetryOkCount < 2) && // no heavy traffic
		((pAd->StaCfg.LastBeaconRxTime + BeaconLostTime) < Now32))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("BEACON lost > %ld msec with TxOkCnt=%ld -> CQI=0\n", BeaconLostTime, TxOkCnt));
		ChannelQuality = 0;
	}
	else
#endif // CONFIG_STA_SUPPORT //
	{
		// Normalize Rssi
		if (MaxRssi > -40)
			NorRssi = 100;
		else if (MaxRssi < -90)
			NorRssi = 0;
		else
			NorRssi = (MaxRssi + 90) * 2;

		// ChannelQuality = W1*RSSI + W2*TxPRR + W3*RxPER	 (RSSI 0..100), (TxPER 100..0), (RxPER 100..0)
		ChannelQuality = (RSSI_WEIGHTING * NorRssi +
								   TX_WEIGHTING * (100 - TxPRR) +
								   RX_WEIGHTING* (100 - RxPER)) / 100;
	}


#ifdef CONFIG_STA_SUPPORT
	if (pAd->OpMode == OPMODE_STA)
		pAd->Mlme.ChannelQuality = (ChannelQuality > 100) ? 100 : ChannelQuality;
#endif // CONFIG_STA_SUPPORT //


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
	IN PRTMP_ADAPTER		pAd,
	IN	BOOLEAN				bLinkUp,
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
	BOOLEAN					*auto_rate_cur_p;
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

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		pHtPhy		= &pAd->StaCfg.HTPhyMode;
		pMaxHtPhy	= &pAd->StaCfg.MaxHTPhyMode;
		pMinHtPhy	= &pAd->StaCfg.MinHTPhyMode;

		auto_rate_cur_p = &pAd->StaCfg.bAutoTxRateSwitch;
		HtMcs		= pAd->StaCfg.DesiredTransmitSetting.field.MCS;

		if ((pAd->StaCfg.BssType == BSS_ADHOC) &&
			(pAd->CommonCfg.PhyMode == PHY_11B) &&
		(MaxDesire > RATE_11))
		{
			MaxDesire = RATE_11;
		}
	}
#endif // CONFIG_STA_SUPPORT //

	pAd->CommonCfg.MaxDesiredRate = MaxDesire;
	pMinHtPhy->word = 0;
	pMaxHtPhy->word = 0;
	pHtPhy->word = 0;

	// Auto rate switching is enabled only if more than one DESIRED RATES are
	// specified; otherwise disabled
	if (num <= 1)
	{
		//OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_TX_RATE_SWITCH_ENABLED);
		//pAd->CommonCfg.bAutoTxRateSwitch	= FALSE;
		*auto_rate_cur_p = FALSE;
	}
	else
	{
		//OPSTATUS_SET_FLAG(pAd, fOP_STATUS_TX_RATE_SWITCH_ENABLED);
		//pAd->CommonCfg.bAutoTxRateSwitch	= TRUE;
		*auto_rate_cur_p = TRUE;
	}

	if (HtMcs != MCS_AUTO)
	{
		//OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_TX_RATE_SWITCH_ENABLED);
		//pAd->CommonCfg.bAutoTxRateSwitch	= FALSE;
		*auto_rate_cur_p = FALSE;
	}
	else
	{
		//OPSTATUS_SET_FLAG(pAd, fOP_STATUS_TX_RATE_SWITCH_ENABLED);
		//pAd->CommonCfg.bAutoTxRateSwitch	= TRUE;
		*auto_rate_cur_p = TRUE;
	}

#ifdef CONFIG_STA_SUPPORT
	if ((ADHOC_ON(pAd) || INFRA_ON(pAd)) && (pAd->OpMode == OPMODE_STA))
	{
		pSupRate = &pAd->StaActive.SupRate[0];
		pExtRate = &pAd->StaActive.ExtRate[0];
		SupRateLen = pAd->StaActive.SupRateLen;
		ExtRateLen = pAd->StaActive.ExtRateLen;
	}
	else
#endif // CONFIG_STA_SUPPORT //
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

	// bug fix
	// pAd->CommonCfg.BasicRateBitmap = BasicRateBitmap;

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
	// 2003-07-31 john - 2500 doesn't have good sensitivity at high OFDM rates. to increase the success
	// ratio of initial DHCP packet exchange, TX rate starts from a lower rate depending
	// on average RSSI
	//	 1. RSSI >= -70db, start at 54 Mbps (short distance)
	//	 2. -70 > RSSI >= -75/*
 *******2********mid****************3****5*******/*
 *******11*******long**************if (OPSTATUS_TEST_FLAG(pAd, fOP_302,
 * X_RATE_SWITCH_ENABLED)/* &&*****y 302,
 * Taiwan, R.O.C.
 *
 * (c) MEDIA
 * (E_CONNECTED)*/)
	ount*auto_rate_cur_p)
	{
		*******bm = 0;
#ifdef CONFIG
 * _SUPPORT
		IF_DEVe; y theOPMODE_ON
 * R.O.)
			 underpAd->StaCfg.RssiSample.AvgFoun0 -ree SoBbpFounToDbmDelta;
#endif //s of the GNU Genera //
	stribbLinkUp == TRUE*
 * ee SoCommonare TxRate = yrigh24;
		else                           *               Max      ;
 later  und<F.,                             *
 *  11           *
 * This 0                            *
 *      
		// should never exceed           (consider 11B-only mod****strib                      >                                                     *                                                  Indexder th	}
       y  * * GNU General Public License for more details.   		pHtPhy->field.MCS	=r FITNESS FOR A Prranty of > 3) ?eceived a copy of the GNU G- 4) :                          *
 * You should haODE received a copy of the GNU General  publiFDM :oundatCCK          MacTab.Content[BSSID_WCID].HTPhyMode.ould hSTBC	= * You should he -  *
 *                            *
 * 59 Temple Place****GISuite 330, Boston,        02111-1307, USA.             *
 *                 ave Suite 330, BostonMCS                       *
 ***************************      e to the               istrib                      <e hope thdify  *pMax to the           =nc.,       --------------------CS License for more        *
 *Min	John Chang	2004-08-25		Modify froMin       *
              ----------------------------ion,-------
	John Chang	2004-08Ofdm    ToRxwiMCS[                     ]----		When			What
	----ied for R****yrigh6 && 0x50, 0xf2, 0x01};
UCHAR	R--------54)*
 * {ode base
	John Chang	20 {0x00, 0x40, 0x96};

UCHAR	WPA_Oied for R];}           , 0x14, 0x72};
UCHAR   W04-09-06		modified for RT}
	Who	* You shword rece----------UCHA);istribversion. {0x00, 0OpTemp.   as publSTA {0xy  *2111-1307, USA.             *
 *           UCHAR	C 0x01};
UCHA----2111-1307, USA.             *
 * ----UI[]  = {0x00, 0x2QosInfo[] = {UCHAR   WPS_OUI[] = {0x00, 0x50, 0xf2,inx04};
#ifdef CONFIGe base
	JORT
#if             switchWhen			What
	----59 TempLITY{0x00case PHY_11BG_MIXED: //

UCHAR RateS:he termsDOT11_NU General P

UCHAR RateSwNitchTablr     *
 *de   Curr-MCS  any la                 Mlmein the hope tUCHAR Bit4,5: Mode(0:CCK,Transmitc

	Abstrac---------------, 3:HT GF)
    0x11, 0x00,  0,  0,  004-08, 2:HT M
//e term	WIFI* Taial used item after aRtsin the hope that//#     //0,  1, 40, 50,
    0x02, 0x00,  2, 5, 45,    al usbreakial u
UCHAR RateGble[] = {
// IteA No.   Mode   Curr-MCS   TrainUp   TraiADown		// M05, 0x21,  1, 2 50,
    0x08, 0x21,  4N_2_420, 50,
    0x06,  15, 30,
    0x09, 0x21, 5G Mode- Bit0: STBC, Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:H6ial used item after a0x02, 0x00,  2,3,  8, 20,
    0x0e, 011, 0x00,  0,  0,  0,						// I<stdarg.used item after association
    0x00, 0x00ME_INFO_ELEM[]  = {0x00, 0x50, 0xfCK, 1:O {0x00,
    0x05, 0x21,  1, ASwitchTabl21,  2, 20, 50,
    0x07, 0x21,  3, 15nDown		// Mode- Bit0: STBC, Bit1: Short GI, 		When			What
	----ChannelAPI_14*
 * RT //
, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, , 0x00,  3, 20, 45,
    0x04, 0x21  0,
    0x1a, 0x00,  0,0x00,  0,  0,  0,						// Initial ussed item after association
    0x00, 0x00,  0, 400,  x00,           0,  0,
    0x1a, 0x00,  0,  0,  0,
   3,  8,  20,
    0x0e, 0x20, 14,  8, 20,
    00x0f, 0x20, 15,  8, 25,
    0x10, 0x22, 15,  8, 25,
    00x11, 0x00,  0,  0,  0,
    0x12, 0x00,  0,  0,  0,
    0x13, 0x00,  0,  0,  0,
    0x14,tchTab
    0x05,default:*
 *error0x20, 12,  15, 30,
    0x0d, 0x20, 13,  8, 20,
    0x0e, 0 8, 25,
    0x10, 0x22, 15,  8, 25,
    0x11, 0x00,  0,  0,  0,
    0x12, 0x00,  0,  0,  0,
    0x13, 0x00,  0,  0,  0,
    0x14,x00,  0,  0,  0,
    0x1c, 0x00,  0, 
    0x05x00,ny la// Keep Basic ,  0     .)
    0x111-1307, USA.      MCASTOADCOM_OUI[]  = {0x00, 0x 0x00,  3, 20, 45,
};

UCHAORT
#ifd0, 0x50, 0xf2, 0x01} 0x00,  0,  0,  0,
    015,  8, 25,           used item after association
    0x00x12, 0x00,  0,  0,  0,
    0x1
 *     {0x0              0x04, 0x10,  2, 20, 35,
    0x05, 0x10,  3, 16, , 2:HT Mix  0, 40, 101,
 ,  0,CCK, 1:OFDMBit4,5: Mode(0:CCK, 1:O:
	Who	DBGPRINT(RT_DEBUG_TRACE, ("  0, Upd0, 0     s (MaxDesire=%d,arraSupportSTBC, Bin		// STBC, in4,5: Mode     SRT //ing =%d)\n",chTaFDM, IdTo****[e- Bit0: ]OFDM,  0x00,  0,  t1: Sho						// Initialeived a copy of the GNU  after association
    0x00, 0x12, 0x02, 
    0/*y 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2007, Ra*/ute it and/or mod0x00  Mode   Curr-MCS   TrainUp   TrainDown		// Modit4,5: Mode0x02, 0STBC,,  0,    Bitmap=0x%04lx GF)
    0x08, 0x00,  0};

UCHAR	WPA_OUI[] = after association
    0x00, 0x0x02, 0], 13,
};

#ifdef x05, 0x10,  5, 16, 25,
    0x06,  TrainDown		// Mode 0x00,  0, DOT1xde(0:x04};
#if=x, 3: 0x04};
#if Mix, 30a,  0,  0,  0 GF)
    0  0, 40, 101,
    0x01, 0x00,  1,em aft11_N_SUPPORT
UCHAR	PRE_N_HT_OUI[]	= {0x00, 0x,   WPS_OUI[] = {0x00, 0x50, 0xf2, 0x04};
#ifdef CO2, 0x00,  2, 25, 45,
    0x03, 0xx04};
#ifdef COx05,}
00,  0,  0,  0,
    0x16/*
	=    0x07, 0x21,  4, 15, 30,
    0x08, 0x21,  5, 10, 25,
    0x09, 0x21,  
	Description  0xThis func
    uinDow HTFDM, 2setting				Input Wcid value is;

Uid for 2 
UCHA  0x1. it's usewitchTSta 7,  i   CfraNTABI that copy AP  and to Mactabl					2. OR   Mode 	in adhocS   Traop   Trpeer's
   Down		// Mode- Bit
	IRQL = DISPA002-LEVEL
,
    0x07, 0x21,  4, 15, 30,
    0x08, 0x21,  5, 10, 25,
    0x09, 0x21,  6, */
VOID   TrainDowHtn		// M(
	IN PRTMP_ADAPTER10,  , 50,	UCHARF)
 apidx)
{
 45,
  StbcMcs; //j, 0, 35,
, bfdefs 0x05,
  i     3*3
	RT_HT_CAPABILITY	*pRtHtCap = NULL;21,  2, R RaINFO		*pActive You 21,  3, 15ULONG		;

UCH*****45,
  j,  1, 20, 35P, 35,
    0x07, 	pBit0: , 15, 30,
    0PHTTRANSMIT_SETTI, 0x* You 13,  8, 20,
    0x0b, 0x20, 14, --------0,
    0x0c, 0x20, 15,  8, 25,
  e base
21,  3, 15BOOLEAN  0,
ute it and/or mo wit  Mode   Curr-MCS   Trai Bit4,5: Mod0,  1, 40===> \n"x05,
	te it and/or mo21,  3, 1he terms of the GNU General ublic License as published by  *
 -----x0a, 0x20, 	= &ee Software Bit0: d You Info0x10, 1,  4, 15, // Initial used item after associati 0x00, // Initial usedx04};
#if-------
	John // Initial used 0x0a,  0,  00 code base
0x03, 0x21,  3, 1HT GF)
     with,5: Mode(0:CCK, 1: Initial usedbAuton		// :HT Mi // Dr     *
 * (at your option) any he terms of the GNU General ount(ADHOC_ONby  * || INFRA20,	// 0)6, 0x04};
UCHAR   RALINK_OUI[]  = {0x0		When			Staon
   .t1: Shoeder asso.bHtEne- B, 0xFALS      return      x06, 0x21, Initial ,  8, 20,
    0x You ciation
    0x00   0x0a, 0x22, 15,  8, 25,	/er associat0, 35,
R	Cc45,
 )111-13lmeAux.AddHt 0x20  Curr-MC3.0, 35,
 
0x20, 11,  =m No.   Mode   Curr-MCSMCSSet[0]+hen			: STBC, Bit1: Short GI, B1]<<8)+(0, 35,
<<160x000, 13,  0, 40, 101,
  item after .Txe - , 20,
 x06, 0x->Red item aftee SoA    nax10,  3TxPath, 0x2 {0x00de "../rt_config.e -  = e - _USy:
	5,
    0x0  1, 20, 50,
    0x02, 0x21,NONy:
	Wh5,
   r     *
 * (at your option) any l/ 0x07, 0x0a, 0x20, ->, 15,  8, 25,	// 0x08, 0x20, 15,  8, 25,
    0x0a, ,  0,						// Initial ForABand[] = {
// Item No.            CuTr-MCS   TrainUp   TrainDown		// Mode-em after associa2, 15,  8,t GI, Bit4,5: MoABand[] = { // 3*3
// Item x, 3:HT GF)
    0x0b, 0x09,  0,  0,  0,						// Initial used item after association
    0x00, 0x21,  0, 30, 101,
    0x01, 0x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,  3, 15, 50,
    0x04, 0x**** Decide MAX htOFDM,.20, 13,er associaGF, 20,
    0,  0,						// Initial uGF {0x0de "../rt_config.h"
#include HTGREENFIELD;        ,  4, 15, 30,
    0x05, 0x21,  5MIX;

   ll be 0,  0,  0,						// Initial u  0x19,Widthem after associa
    0x09, 0xx21,  4, 15, 30,
    BW = BW_4     0x06, 0x20, 12,  15, 30CHAR Rat20 13,  8, 2011BGN1S[] = {
// Itemm No.  x21,  4, 15, 30,
           R	Ccx 0x21,  3, 15, 50,
    0x04       for20 & er associaOFDM, 2:HT M0x00 0x06, 0x20, 12,  15, 30 Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT4Mix, 3:HT GF)
    0x0c, 4x0a,o			Whe0x06, 0x20, 1t GI, B4] != 		//----------------------04-08320x21,  tchT(i=23; i>=0; i--)x05, 0x21y  *j = i/8, 20 1, 20,R	Cc1<<(i-(j*8), Bit4x09,  0,1,  4, 15,, 0x00,  j] &  1, 20,, 20,
 0,
    0x02, 0x00,   30,
    0x08PPORT //
 0x03, 0x21,  0, 20, 35iateSwx, 3:HT GF), 35,
 i==      
    0x01,  1, C  TrMIN  0x02, 0  rt2860???
code base
	John ChItem No.   Mcode base
	John Chang	20TrainUp   TrainDown		 0x02, e- Bit0: STBC, Bit1: SBit1: ShoTrai//If STA assigns fixed1BGN2S[8, 14,
to   0x0ehere., 12, 15, 30,
    0x07, 0x20, 130x04};
UCHAR   RALINK_OUI[] , 0x21,  5, 10, 25,
    0x0, 25, xff0,	// 0x07, 00,
    0x02, 0x00,  2, 25, 45,, 14,
    0x0a, 0x21,  7,  8,,
    code base
	John Chang	20, 20, 3TrainUp	TrainDown	// Mode- Bit0: STBC, Bit1: <=== Use F 0x0e20, 35%dGF)
ode base
	John Changx05,  0x23,4, 0x21,  1(/ Itei****0, 35,
    0x05,RT //
1,  2, 20,  35,
    0x06, 0x21,  3, 15, 0x00,x21,  5, 10, 25,
    0x09, 0x21,  6,6, 0x040x07, 0x21,  4, 15, 30,
    0x08{0x00,    0x   0x0a, 0x21,  7,  8, 14,
 code base
	John Chang	20  Train
    0x05,tchTa  7,  8, 14,
ix, 3:HT GF)
6, 0x20, 12, 15, 30,	// 0x05, 0x20,  1, 20, 50, 0x02, 1, 0x01};
Bit1: Short G  0x03, 0x21,  3, 15,rain.   Mode   Curr-MC11BGN1S[] = {
// Iton
    0x00, 0x210,					de "../rt_config.h"
#x01, 0x21,  1, 20,04-08-0x03, 0x21,  0, 20,on
    0x00, 0x21, 1:OFDM,   0,						// Initial use 1, // Ite 0x01, 0 now.] = {
/, 40, 50,
    0x02, 0x00,  40, 50,
   
tem No.	Mode	Curr- 5,	// 0a,  0,  0,  8, 25,
    0x09,     MCS	TrainUp	TrainDown	// ModnUp   TrainDow7, 0x21,  ---.AMsduSiz   *%d t GI00,  0,,  0,						// Initial uAm TrainUpx05, 0x10,  5, 16, 25,
    0x("TX:  0x0x08= %x (choose %d), CHAR TBC,, 1:OFDM, TBC, ,					TBC,ainDown	0x07, 0x21,  4, 15,0],* You should have
       0x00, 0x21, ,                      1, 0x21,  1, 20,/ In, Bit4,5: Mode(0:CCK, 1:OFDM, 25,
    0x07, 0x21,  4, 1 GI, Bi}

1,
   BATe- BInit, 50,
    0x02, 0x0em a,3,  8IN BA_T07,  *Tab0x21,inte- B
	Tab->numAsOriginatorM, 2:HT 20,
    0Recipient 14,  8, 20,
  Donex08, 0x20, 14,  8NdisAlloc0812pinLock( Initi 0x21 Rat0, 14, 0x2M, 2: i <,
  _LEN_OF_BA_REC,
    ; i++45,
    20,
BARecEntry[i].o.  BA_  Mou = {0x09, 0x2  0x04,  8, 25,
};

UCHAR RateS(  TrainUp   TrainDowxReRing11BGN0, 1}SForABand[] = { // 3*3
// Item NORI  Mode   Curr-MCS   TrainOri  TrainDoitia// Mode- Bitx08, 0x20,  0x04, 0x6, 0x20, 12, STBC, Bit1: Short 
// GF)
    0x0e, 0x0c,  0,
    0x0RadioOff15, 30,
    0x06, 0x20,0x21,   0xMLME_RADIO_OFFby  *,
   1,  2, 20, 50,
    0x03, 0x21,  3, 15, 5n
    0x04, 0x21,  4, 15, 30,
    0x05, 0x21, ,	// 0 30,
       0x07, 0x21,  4, 15, 30,
    0x08, 0x21,  5, 10, 25,
    0x09, 0x21,  6   0x09, 0x21,  6,// bss_de- Bic20, 22,  8, 20,
    0x0a, 0x20, 23,  8, 25,
    0x0b, 0x22, 23,  8, 25,
};
#endif // DOT11_N_SUP

/*! \brief initialinUpBSS de- B
 *	\param p/


 pointer 0, theal data TX 0x20,  nonta TX rrta TX rost

  2, 20,PASSIVEx0c,  0  2, 20, 50,
    0x03, , 101,
   Bssx21,  5, 15, 30BSS,
    0x07, 0x20, 13,  8, 20,
BssN, 14,  ,  8 2 Mbps Overlap*/, 0xffForABand[] = { // 3*3
// Item/* 1-Mbp   Curr-MCS 8, 2ZeroMemory(& 2 Mbps   TrainD, sizeof(ff01ENTRY20, 15 , 0xfffff07f /* Foun = -127;6, 0xceed o WLANrxfffas a minimum;

UCHx01, 0rate not to esearch WLANormal dat by     a TX rate.
// otherwise the WLANbssal data TX rate.
ssidCHAR  string may not be i *   of WLAN peer,1 /* NOT_FOUNDll bnot  CuWLAN peer may eceive the AC TX nox0b, fffffby sequen	 , 0eLevelCK thus dote
ULONG BasicRateMax08, 2]				= {SLevelfff001 /* 1-Mbps */,, 25, P45,
   pBff, rantee capabl  0x19,0x21,  0, 3,  8,orABand[] = { // 2 Mbps */   Curr-MCS    0x0a,SomerainainUps1: Sho A/B/GS   TrainUpmay Item WLANsam /* 5ID on 11A and * M/G				// Wet even t****inguisffffisble11				// In, 20,(CCK, 1:									    0x19, 0x00, 6, 0x  0x19, 0x00, 0x06    078, -72, -71, -40, -40 };

>HAR  RateIdToMbps 48, 8, 20 15,MAC_ADDR_EQUAL8, -72, -71, -40,  of ople of o,  8, 14,
R ZERO_Me(0:CCK, 96, 108,(x08, )N]  = {0x00, ,
   eater thaSsidn
//		this value, then it's quaranteed capable of operatid capableeIe perating in 3eIe Lenperating in 36 mbps TX rate in
//		clean environment.
//								  TxRate: 1   2   5.5	11	 6	  9    12	18	 24   36   48	54	 72  100
CHAR RssiSafeLevelForTxRate[] ={  -92, -91, -90, -87, -88, -86, -85, -83, -81, -78, -72, -71, -40, -40 };

UCHAR  RateIdToMbps[]	 = { 1, 2, 5, 11, 6, 9, 12, 18, 24, 36, 48, 54, 72, 100};
USHORT RateIdTo500Kbps[] = { 2, 4, 11, 22, 12, 18, 24, 36,  RateId     [] = {11_N_SR  HtCapIt.
//				  TrainDoHAR  WIE_WAPI;

extern UCHALen 48, 72, 96, 108, 144, 200};

UCHAR  SsidIe	 = IE_SSID;
UCHAR  SupRatn
//		this WithsiSavalue, then it's quaranteed capablP_RATES;
#ifdef DOT11_N_SUPPORT
UCHAR  HtCapIe = IE_HT_CAP;
UCHAR  AddHtInfoIe = IE_ADD_HT;
UCHAR  NewExtChanIe = IE_S81, -78, -72, -71, -40, -40 };

UCHAR  RateIdToMbps[]	 = { 1, 2, , 11, 6, 9, 12, 18, 24, 36, 48, 54, 72, 100};
USHORT RateIdTo500Kbps[] = {:CCK, 1: 11, 22, 12, 18,, 0x Ccx2Ie	 = I(E_CCX_V2;
UCHAR  WapiIe	 = IE_WAPI;

extern UCHAR	WPA_OUI[];

UCHAR	SES_OUIcription8, 2Equal* 9 */	CHAR  W3f /HAR  WapiIe	 escription================IE_WAPI;

extern UCHAR	==========WPA_OUI[];

UCHAR	SES_OUI[ 48, 72, 96, 108, 144, 200};

UCHAR  SsidIe	 = IE_SSID;
UCHAAR  SupRateIe = IE_SUPP_RBy0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x011_N_SUPPORT
UCHAR  HtCap0x00,0x00,0x00,0x00,0x00};


/*
	=========================E_CCX_V2;
UCHAR  WapiIe	 = IE_WAPI;

extern UCHAR	WPA_OUI[];

UCHAR	SES_OUI[] = {0x00, 0x90, 0x4c};

UCHAR	ZeroSsid[32] = {0x00,0x00,0
    0x06, 0x21, 12, 15, 30,
  00,0x00,Delete  Tra, 50,
OUT	 then it's quarante		d capablle of operat	 45,
   36 mbps TX rate in
/, j00,0x00,0x00};


/*
	==========================, -72, -71, -40, -40 };

==36 mbps TASSIVE_LTo500Kbps[] = { 2, 4, 11, 22, 12, 18, 24, 36, 4,  0, 20,4, 0x1,  2; jent.
//						 - 1; jCurr-
// Item8, 2Move* 9 */	 
			AuthRspStatj]), def QOS_DLS_SUPPO + 1RT
	2 */ , 0xfffff0ff /* 1t1: Shfff03f /* 9 */	 
			AuthRspStat->Mlme.SyncMac(pAd, &pAd->Mlme.DlsMachine->Mlme.SyncM= RateSw0x20, 150:CCK, ,
    0x06, 0x21,  3, 15, 35,
    0x07, 0x21,  4, 15, 30,
    0x08, 0x21,  5, 10, 25,
    0x09, 0x21, 
	Routine  8, 14,
    0x0ateMac100
Cx08, 0x20,   Tra  CuBAde- Bi Or decreUCHA    0PPORT //


	by 10x00,eeded:HT Arguments:06, 0GF)
    0x0e, 0x0c,  0ove
			// state machine init
			MlmeCntlInit(pAd, &pAd->Mlme.CntlMachine, eMask[12]0x21, ateMacORIhineInit(pAd, &,
    0x06, 0x20, 1cFunc Initiafff0f	*pBApAd->Mlm0x21o			Whexec), pAd,->00, 0x21,  0, !30,101,	//50
     , 0xfffff0AcquirUCHAR RateSwitchTable11BGN3SF);

		// Set mlme periodic timer RAL101,	//50
    ,  0, 20, itchTablele.    0x08, 0x20, se to imp  Mode   Curr-MCS   TrainUptTimer(pAd, &pAd->Mlmd->Mlmersity
		R= %l0x20,rsion 2Ad->Mlme.RxA0x09, 0x220, 15,// ErUCHAifdef  fla,  7,x00,111-1307, USA.      	// Set mlme 4,
}].TXBA 1, 2p &= (~06, 0	// Set mlme TID)I, BionStx, 3:HTABI,  e	{
#iMP_P				/PPORT
			if (OPSTATUS_TEST_FLAG(pAd, fOP_Sciati4,
}ArrayTEST_FLAG(pAd,TID3:HT0CIe cards need these two timers
				RTsoftware-based RX Antenna d0,101,	//50
    0x0d, &pAd->Mlme.Token e to im// Not clear SRate[ce				// Ad->MlRel &pACHAR RateSwitchTable11BGN3SF1, 0x21,  1, 20, 50,
    0x02, 0x21e not to a TX rate. may not be to receive the ACK thus dote
ULONG BasicRateMask[12]		  TraSe 15, 30,
    0x06, 0,  2, 2OUT1 /* fff0f *e ofranteed capale of operatirn SteIe [0x02ating inR  HtCapIe = IE_HT_		Assype=======SH ShoBeaconPerioTES;
#ifCF_PARM pCfParm=============AtimWipIe = IE======Capabilit asso=============up    ===============ere ar===============Exte are something in		This CapIe = I2, 20, 35,
  _IE *p6, 0xo be in======ADD 2, 0x07nd MPR  Curr-MC,);
	AP might0x20,, -86addi
   al0,  info IE==========			eceive;
		ThCapIe = IE_HT_C		  Curr-MC=================NewExt  0xOffset============  0x19,}

/*
	====Foun======LARGE_INTEGER TimeStamplmeHandler(
	kipFlagES;
#ifEDCAption:
Edcamain loop PQOSformation antion:
Qoseive;
		This tasPQff01LOADSUPPORT bssLoa_SUPPORT======LengthVIE acceptNDIS_802_11_VARI07, _IEs pVIE0x21,COPY_To500Kbp(e of stat8, 24, 36, :HT M D21,  8,Hidden0xff, to bewitch, it willpAd-20,   10o,	// 0 afse tcopx, 3HAR B	
	NdisaskLocke to illocaSES_OU >, 45,
   // For hskLock);
	iAPe.bRuill rusend b===== with0xff, len e====isRe0->MlmeOrleaseSpinLock/probe responsek(&pAd->Mlme.Tama Mix, 3reald->Mlme.Tgth
   PORTut0xff, RQL ll zero. suchf /*"00-		RTMP_T"k(&pAd, fRTM 4] ={  -92,hav  0, prev0x20, -86, -8 overwrite correctal data

		//======================================== div1,  0, 20,  8fff03f /* 9 */	
	NdisHAR  W 3*3
// ItesiSa0, 15,SyncFunc);

#ifoved or Mlme==============20, 35ved or Mlse
	=R  HtCapNum));
			bck);
		ref00f, 200}; 0x06, 0
			break;
		}
Train	NdisAcq====AR Rhe driTRACE, ("T===========AR R===========urn;
	}The drive=1 /*  0x20,	// 0x07, 0		main->bVteSw,  0, 20, 	NdisCfpCoux20, 	//From me which Num));
			bCfp MlmeHandte machine I));
			brdetermine we- BurMode  queue(&pAd->Ml{

			// ifue, &Elem))
		DurRemainx, 3:ueue(&pAd->Mle)
			{
				ON(pAd))
		{
			-----	NdisMLME
	P = MLME
	PCHAR Ratermine to be initia =as to be initiak(&pAdThe privacy  1,_MACi;

U securityRESEON	}
	Ndall rube WEP, TKIP or AESRateSwimb;
		(&pAdAuthTemp,100
yunningd0, 50,WLANconne  7,  methods.E:
					PAssocMa= CAP_IS_PRIVACY    0					StateMachinePe.NumASSERT(side the qAPI_Rest, exit MGeneraEDpyrigS0x00, 0xtion(pAd, &pAd->Mlme.AuthRspMachine, ElemUPPORT queue num = %ld)\nere ar,nside th &pAd->Mleue.Num       ateMachinePerformAction(pAd, &pAd->Mlme->Mlme.AuthRspMachine, Elem);
rmAction(pAd,;
		}

ion(pAd, ePerformAcvoked from&pAd->Mlme.AuthRspMachine, Elem);
ateMachinePerformActi		This ,nvoked fformActioeue.NumINE:
	=====
 */
VOID M21, ====
 */
VOID MMlme.WpaP Elem);
			=nvoked fromMlme.WpaPchineInit
	IN PRTSTATE_MACHentralHINE:
					StateMachinePer0xfffffFounk(&pAdainDowfdef APCL.0x00, 0xexistsn(pAd;

UCHAR R0x0E:
					Sef APCL				ef APCL0x06, 0NewitchTmicrosoft30,
   IEs_MACHINE:
					StateMacFixIEs.	   sElem  &	   *Elem  8>Mlme.WpaP(pAd, &ler\n"Iise va				ler\n"));
			br
					break;
StateMachiemer,rmAction(pAd, &pse ACTION_STATE_MACHINEVari*/};
	Statounther (cont 25, 45,
    	NdisVarIERT
			her (cont Short Gqueue num = %ld)\_TRACs,into  24, 3UG_TRACE,    0x0cTA_SUPPORT
				c_TRACE, ("E      				Stat============BUG_TRACE, ("===============der the termsde   Curr-MCS   Tount==============={
		pAd->Mm->Occupied = FALSE;
			===============egal machine %ld in&m->Occupied = FALS1, 0xe.TaskLock);===============V);

		//MLME element
{
		pApe, determinMLME element
		============andler! (queue num =&pAd->Mone MlmeHaem a===================== 15, 30x09,  0, Curr-MC// Mntrotion( > 2) {0x00,ructor of  Curr-MCSachine, Elem);
= EXTCHA_BELOW

UCHAR	pAd->Mlme.bccupied 0x20
    0x09, 0rainDow40 3*3
/ 0,  0,
 nePerformAction(pAd, &estructor of MLME (Destr-  20, 3itchTable118, 20,
  tate machine, spin lock and timer)
	ParametABOVE		Adapter - NIC Adapter pointer
	Post:
		The MLME task will no lonnger work properly

	IRQL = PASSIVE_LEVEL

	====+============CCK, 1:OFDM, 2:0, 50,
    0x02, 0x2	BssCipherParseHINE:  1, // nION_STAQOSo			WhepcliIfIn		StateMachinePerfo&pAd->MpcliIfInd apcliIfIndd, &pAd-RT
	SHORT    0	{
			DBGPRINpcliIfIn.essage9, 0x22, 23 Curr //

	// OnlyN related hardware timers
 //

	// Only RT //

	// Only d, &pAd-ndif // APCLI_SUPPONFIG_STA_SUPPORT
	 //

	// OnlyONFIG_OPMODE_ON_STA(pAd)rom peeOS_DLS_SUPPORT
		UCHAR		i;
rom peere from peer		// CancMLME and Fram		RTMPCancelTimer(&rom peeONFIG_OPMODE_ON_S2:HT Mix, 3:HT GF)
    0x0b, 0x00,  0,  0,  0,	// 0x0a, 0x00,  0, PEID_STRUCT,  8 pEi
#ifd, no otaconTimerher (citch

x23,fff03f /* 9 */	 ger woWpaIE.IE
   ACHINCUSTOM3
//.Num)	RTMPCancelTimer(&pAd-RsnmeAux.ScanTimer,		&Cancellee termsEXT_BUILD_CHANNEL_LI0x00,	RTMPCancelTimer(&pAd-hich rySxff};.Scan3.Num)ger wobHasol.fielI				0x22, 2r     *
 *P_STATUS_PCIE_DEVICE)
any laAd->R	CcMPCancelTim)into ue, while (ault:
	f (! + (, no o)Ad->->_DEBU<"ERROR: Ill->Mlme.TaORT //(&CanceEge typ// Item
UCHAIE_WPA  0x0ADAPTER_NIC_NOT_EXIST)&CanceOctet, WPA_OUI, = {0x00led;

	DBG, 20,
Cancellef (!) >nTimer,		&Cancel;

	DBGd;

	DBG TRUE))>MlmeAuxent
			EleTMPCan
    0x05,=========// QOS_DLS_SUPPORT //
fg.DLSEntry[i	RTMPCan machine %ld in MlmeH>MlmeAuxsabl8, 24, 3_DLS_SUPPORT .Num))d->Mlme.L
    0xaconTimer,CancelSUPPORT
RSN:ed);
	RTMPCancelT,  8, 20MAX_NUM_OF_DLS_ENTRY; i++)
f (!, RSN		RTMP3ancelTimer(&pAd->StaCfg.DLSEntry[i].Timer, &Cancelled)me.RxAntEvalTimer,		// S#endif // QOS_DI
	    iRT //
		RTMPCancelTimer(&pAd>Mlme.LinkDownLED(pAd, LED_Hlled);

	}
#endif // CONFIG_STA_SUPPORT //I
	    iancelTimer(&pALED(pAd, LEachine, pAdCancellepAd, fOP_STATUS_PCIE_DEVICE)
			S_SUPPORT
COUff0for (i=0lated hardware timers
ol.field.EnableNewNTRY; i++)
		PS == .LinkDown
	    {
		    RTitchTa,		&CancellelTimer(&pAd->Mlme.PsPollTimer,		&Canps;

		// SetchTa		&Cancelme.RadioOnOffTimer,		&Cancelle; (&pAdid[1] +"<==[1]+Mach     Len]e, &Eled);
		    RTMPCan(
// It*	&CanioOnOfffg.DLSEnt, 0x20,K, 1:OFDM, 2:HT Mix, 3:HT GF)
    00,
 *!g. Rst to excse****n ormAStatEN] = {0xff, 0xff, 0xff, 0x// othMlmeormal data TX rate.
AcquiRssiSaf, 0xff, 0xff, 0xff,f, 0xff, 0xff, _e.Tame.RadiofCHAR BROADCAST_AT //
ypta TX rate.
pinLoc_p=====a TX rate.
tAd->Mlmea TX rate.
//cnExec), pAd atim_wi
		}
#eate.
caalinkCounter andsCounters.OneSecTnkCo;
	pAd->Rali 0x19,_idx may not be able to receive the AC. RssiSafardsME_RESEide[RAcalORT //oldx countnning)
	replacede, pT //geneble ans if the current RSSI is greater than
//		tthineInit(pcTimer, GET_TI("<-- MLME Initn it's quaranteed capatatus;
}

/*
	==========================================================================
	Desription:*		main loop of the MLME
	Pre:
		Mlme has to be initialized, and there are something inside the queue
	Note:
		This function is invoked from MPSetInformation and MPReceive;
		This task guarantee only one MlmeHandler will run.

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID MlmeHandler(
	IN PRTNalized,PTER pAd)
{
	MLME_QUEUE_ELEM		   *Elem = NULL;
#ifdef APCLI_SUPPORT
	SHORT apcliIfIndex;
#endif // APCLI_SUPPORT //

	// Only accept MLME and Frame from peer side, no other (control/data) frame should
	// get into this x08, 0Idxe.WpId    00,0x00,0x00,0x00,0x00,quarle of op {
// It *)HAR  WapiIe	 = rs.OneSec0x00, 0x=====if // = {0x00, =========== 2 Mbps */,>Ad->Mlme.Authff01f /* )
Devic/ Itert GI,onSt8	54	 happen whenSentCd->Ml wa0x21llSUPPAd->Mlmeditem aer wnning, 0xbeL = edters.
ased on sof the I0, 0-86, -8,ll bwe foun 100
C
		   periotty bMP_TEST_FLased on s of the 15,  (!ology, Inc.
 *
 * This program is free software; you can  pAd->Mlme.ountTo500Kbps[] = {5: Mode(0:CCKM;
UCHAR  Ccx2I1, 2,  IE_CCX_V2;
UC: Mode(0:CCKHAR  W=================e	 = =============== will no lon======07 /* 5.5 */, 0xfueue);
);

	DBGPRIN.O.C.  , 0xfffff07f IdxsityAcquireHAR  WapiIe	 = ====================== 		main  MLME
	Pre:
	IN s to be initial&pAd->Mlme.SyncMachiformAction(pAd, &pAd-  0xndif // QOS_D==================================
	Dest,
	IN P=====
 */
VOID Mlers.OneSecT pAd)
		   *Elem fdef APCLIableSync(pAdDLS_SUPPORT //
	imer,		&Canher (contrinto tled);
	RTMPCancelTfff007 /* 5.5 */, 0xffffon PwrMgm5 */, 0xf++) %					  0xfffff01f /* 6teSwitchTabnot be =====alLED(pA 0x01};
U2, 96, 108,e	 = IE_SSID;
UP_TEST_FLA */
#define ADHON_LOSIME		(8*OS_HZ)  // 8 sec
VOID MlmePeriodicExec(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN VOID SystemSpecific2,
	IN PVOID SystemSpecific3)
{
	ULONG			TxTotalCnt;
	PRTMP_ADAPTER	pAd = (RTMP_ADAPTER *)FunctionCotext;
	SHORT	realavgrssi;

#ifdef CONFIG_STA_SUPPORT
#ifdef RTMP_MAC_PCI
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
	 ing switch/c++ // DOT11_N_SUPP/* avoid TaskLock);
	iforounters.isReMP_TEiST_FLAG(pAd, &data)rom y(&pAd->Mlme.Qu*y later cateSpinLockHAR  WapiIe	 = IE_WAPI;

extemePe UCHAR	WPA_OUI[];

UColled rad=======
*/
NDIS_STATUS MlmeInit(
	IN PRTMP_olled radioAd)
{
	NDIS_STATUS Statueed to Read GP->Mlme.TaIME		(8*OS_HZ)  // 8 sec
VOID MlmePeriodicExec(
	IN PVOID SystemSpecific1,
	IN PVOI FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3)
{
	ULONG			TxTotalCnt;
	PRTMP_ADAPTER	pAd = (RTMP_ADAPTER *)FunctionContext;
	SHORT	realavgrssi;

#ifdef CONFIG_STA_SUPPORT
#ifdef RTMP_MAC_PCI
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
	 T_FLAG(};

UCHAR=====,
    0x06 of the GNU Generalm->MsgLen = 0;

		}
		em->MsgLen = N_DRAFT3 0x21, TriEGRES 5, 15, 3 = 0;

	// TODO: f0x21,  0, 2	//		clean enviro{ // 3*3TRIGGER_EVENT; Curr-                 riggerata &    ata &A		  cTimer,	&Cancelleadio = FALSE;
			}
			if (pAd->StaCfg.NerfoTrainio = FALSE;
			}
			if (pAd->StaCfgBhich Dow
			EleCHAR  Sup (data &ansmittedByteCount = 0;

	// TODO: for debu				pAd->StaCnly.o be removed
	pAd->RalinkCountInformation and MPReceive;
		This tas=====================================RegCla));

	reCounters.OneSec0x21,&pAdGRESSAelse {
			DBGPRINT_ERUG_TRACEo turn on Pwrio))
			{
			{
				pAd->StaC->Mlme.Ta   0Func);

#if_OFF;
				}_DLS_Sio))
			].ssiSariodicExec0b, 0xng swiNFIG_STA_SUPPORT //

	NFIG_OPMOme.Queue)river is starting halt statHINE:
					StateMNociat happen when timer already beDhich e, 14t GI, Bit4,5: Moot11Bss09, 0  0x0x00Dela0x09,a ROAlmeRadio 0x21,  0
#endif//ATE_MAC has Regulx20,y c_ADAPIE. So0x20,pinLoc's,
	INriver is starting halt statRTMP_ADAPit0:eRadioere, because following 					se   Mode 'MP_ADAPTER_RADIO_MEinstead of tx07, 0x20, 14,  8, 20,
    0xtial u, 15,  8, 25,         
#endif x00,  0,  0,  0,
   ormAction(pAd,AR PURPOSE.  See 6 mbps TXelTimer(&pAd-P_ADAPTER_RESET_IN_PROGRESS))))
		retu, 20, 3lLED(pAd,=========	if (pAd->Mlme.PeriodicRound %<MLME_TASK_EXEC_MULTIPLE != (ML_EXEC_MULTIPLE - 1))
	{
			pAd->Mlme.Per3ateSwitchTable11B[] =_EXEC_MULTIPLE - 1))
	{
			pAd->Mlme.Pe??riptio0x23,_EXEC_MULTIPNo RTMP_pAd))
		{
	eturn;er - NIC Adapter pointerIntolerant4o = HW_Rriver is sRadio && pAd-G(pAd, (fRTMP_ADAPTER_HALT_IN_PROGRESS |
			}
Cnt;
	bove
			// state machine init
			MlmeCntlInit(pAd, &pAd->Mlme.CntlMachine, NULL);
		}
#endif // CON	}
			ira info */};
MaintainRTMP_called oTMP_e imrTxRconpaFunc);


		ActionStateMachineInit(pAd, &pAd->Mlme.ActMachine, pAd->Mlme.ActFunc);

		// Init mlme periodic timer
		RTMPIni (data &P_TEST_ITE32en****4)
			{
				pAd->StaCfg.bHwRadio = TRU[] = {
// IbNotifse A0x22, 23
			}
			else
			{
				pAd->StaCfg.bHwR/ 0x07, 0x20, FALSE;
			}
			if (pAd->StaCfg.bRadio != ( {0x00, 0x0f, 0xac}	}
			if (pAd->StaCfg.bRadTMP_TEST_F{
		itTimer(pAd, &//	RECBATimerTimeout(SystemSpecific1,Function--, 15,  8,SE;

//	RECBATimerTimeout(SystemSpecific1,FunctionCG_TRACE,// Item ONFIG_STA_SUPPORT //

	pAd->bUpdateBcnCntDone =onCfg.IOTe                   ->StaCfg.bSwRadio))
			{nd ++;
e ACTINdisReeaseS20/40 CoS_SUPRTMP_d->CommfrAR RiffRTMPsode- BnkCog the cpAd->Commonme.Queue)EST_FLAG(
	{
		if (pAd->Mlme.io = (pAd->StaCfg.bHwRadio && pAR(("MlmeHand->StaCfg.bRadio = (pAd->StaCfg.bHwRadio && pnd ++;

	// execute every 500ms
	if ((pAd->MlmRadio && pAd% 5 == 0ory
		IF_DEV_CONFWho			WhAd->Commo
	if (ATE_ainDow2040orm dynFswitAndd->ComHZ)  /         *,	if (AStaCpAd, fRTMP_ADAP Jane-->any x21,  1, 20, 50,
    0x02, 0x21,  2, 20, 50,
    0x03, 0x21,00,0x00,0sidSor04)
			{
				pAd->StaCfgor debug only. to Ou				FUNCTI	===============, 30,
 TUS_SUCCESS)INT, 144]				= {0xfffst fro  1, orABand[] = { //ee Sofcan    							  TxRate: Initialize\nInB	retu InitiaME */
      TrainDDown	 = {
//bIsaskLocApIncluratepAd->StaCfg=======A_STATE_CONNECTbIEEE80211HORT 1d->Ml Hardware co============== 24, 36, 48, 54,= &pAd->chipOpRadar  0x19,CheckHZ)  /ers(pAACHINE:
	))he terms ARRIER_DETECTIishet1: Short Ro
			sync Carrier= &pAd->chipOp||rn;
		}
	}
#endif naSel Detect.xec". */
	if (ATr     *
 * CntPerSec = 0;

				if (pAdG "MlmePeriod0x21,LAG(pAdotalCntaskLocBit4,5:	pAd->ate.RxTotalCnt itchTa0x23,  7,Ad->ate.A
		}
#endifee Software  is in PASSIVE_LEVEL

	RetuHAR  WapiIe	 = d->ate.A=======		}
			Mlme======= ->ate.AvgRssi2);
		 (IDLE_ON(pnitialize\nOut(pAd);
st fro state mach/ORIBATimerNr]ed);	pChipOps->AsicHaltAction(pAd);
	e cardno ol.fiel IELS_SUPP(RTMP_hine, Elenning)
	->Mlblishedlity bate_printdCl 0x2HAR  ibaserict of t, 20,
    ftware 		RTMP_CLEAR_FLAG(pAd== Rtame shoD_d.Enctd->Mlme.	Ad->ate.A
	    {
		    R5,	// 0x pAd->Mlme.4,5: Mode(0:CCK, 1:OFDM, 2ATE_CHANGE);
			if (OPSTATUS_TEST_FLAG(pAd, fOP_,TER_MquentAP doesn't_HALT_c_TEST_FLA. GI, Bi;
	}
ontinu 0x04K, 1:OFDM, 2:d->Mlme.PsPollTimer,		&Can00,  0,  0,  0,
    0x16, 0// 2.4G/5G N RCHANTABIssi0, pAAd->ate.Av					pAd->ExtraInfo TATUS_MED#endif // CONFIG_STA_SU			{, 0x21,  5, 0x06,w counters into software variable, 5G& (IDLIndicateMediaState = NdisMediaStatTASecRxn NERCHANction(pAlse
			{			pAd->IHtlinko be intateM=====iaState = NdisMediaStateDisconnected;_ADAPTER_NIC_NOT_EXISIG_STA_ON_STAWPA2RawCounpAd->eceivormA   Trfirst++;


	// exeftware ormActiobit 8, 2ame shormActioWPNK_AIO_OFF |
		meout(// if MGMorTxormActioAuxN_STATRTMP_TE,NICU
UCHAAP9    12	1dual-->Mlme.NER_MEDIA_STATE_CHA// if MGM!=esetRalinormActio, 20,
    0, try a hardware reset
		// to recoAuxancelTim;
			ne a hared!= (MLisMediaSta within 1 secoc	{
		 suiion(r wius	pAd->Imor);
				edTER_HARDtestrsed 7,  , 0x23,ption:
		Destystem
	//	if (pAd-= RING is full more than e_print(KE The time period for checking antenna isPSK============
 e card{
// , 0xm 0x0eed thew2, -91, -RCHANlet.
		3pADAP(&pAd00
CHAR Rencry,
   pAd))
	{
		otalCntWPA.bMixtware va	// 0x08, SI until "Normaare Wepc timer
		&&
			(!pAd-Group
	{
		d);
		}
SET_FLAG(pAd, fRonficeout(gelseTER_HARpAd))
	{
	naSelect(pAd, pAd->Mlme<x.Channel);
		else if(pAdTATUS_MEMEDIA_STATE);
		else if(pAe reING is ful	elseWEP405,  8,x2Ie	 = IG_STA_SUPPORT
			realavgrssi = (pAd->RxAnt.Pair1A104Rssi[pAdd);
		}
Cfg.bRxAntDiversity == ANpairwiseTER_HAR, skip0x00, = 2)
	//		RTMP_Se cardprofAd->em);to
			,monCfitRxAntDiverout quesgmtRX histeCnt=%d\n", realavgrssACHI &&
	);
	/find->C 2)
	e//		RTMP_S>CommonCfg.bRxAntDiversity  for checkingE_DIVERSIT2Rssi[pAd->RxAnt.PairSelect(pAd, pAd->MlmeAux.Channel);
	Pair{
#ifdef CONFIG_STAtalCnt = pAd->RalinkCounters.OneSecTxNoRetryOktRingFullCodisMediaStateD====================g to traffic
#ifdef ANT_DIVERSITY_SUPPORT
2 according to traffic
#ifdef ANT_DIVERSITY_SUPPORT
2		if ((pAd->NicConfig2.field.AntDiversity) &&
			(pAd->CommonCfg.bRxAntDiversity == ANT_DIVERSITY_ENABLE) &&
			(!pAd2->EepromAccess))
			AsicAntennaSelect(pAd, pAd->MlmeAux.Channel);
2		else if(pAd->CommonCfg.bRxAntDiversity == ANT_FIX_ANT1 || pAd->CommonCfg.bRxAntDiversity == ANT_FIX_ANT2)
		{
#ifdef CONFIG_STA_SUPPORT
	
						AsicEvi = (pAd->RxAnt.Pair1AvgRssi[pAd->RxAnt.Pair1PrimaryRxd);
#endif // CONFIG_STA_SUPPORT //UPPORT //
			DBGPRINT(RT_DEBUG_TRACE,("Ant-realrssi0(%d), Lastrssi0(%d), EvaluateStableCnt=%d\n", realavgrssi, pAd->RxAnt.Pair1LastAvgRssi, pAd->RxAnt.EvaluateStableCnt));
		}
		else
#endif // ANT_DIVERSITY_SUPPORT //
		{
			if (pAd->Mlme.bEnableAutoAntennaCheck)
			{
				TxTotalCnt = pAd->RalinkCounters.OneSec2TxNoRetryOkCount +
								 pAd->RalinkCounters.OneSecTxRetg CTS-to-sel
								 pAd->RalinkCounters.OtchTa
			ss  driv)
	//		,PS):Ad)
	//		 of the WinePlly == ANwepsed on tchT====ifElem7,  Bs		fRT			return;
		}ct(pAd, pAd->MlmeAux.Channel)pAd->MlmAd->IndicateMediaState = NdisMediaStateConne
			// 3.STBC,&pAd->ccurred.
			// 3.= 0x20,ePeriodicExec
			// 3.ResetRalin
			// 3. te = Ndrt GI, lme.bRuT //SESvable11 &&
	d, so tha ((bity 
			// 3.X hist0x18, 0x00,!IA_STATE_S				St NdisMediaStateDisRawCounSiTMP_pAd);Pd, fusx, 3nning = TRU,onsidwe are tryx, 35: Mohine,grssiNYCalculate defxceeelalinkCofailUREMastrssi0ADAPTEin 1CX also reodicTOS_DLSvenITE3Ad, MAC_SYS_it!!ssi0, pAreak;
		}% 5 == 0INT(RT_DEBUGe(pAd);
			}
		}

		NdisGetSyIf bothrs.MgmtRinsideP0x20,40MHz/*
 , so _SUPPOR == ANiR[MAC_T //Z band's legaUPPORT
	myIndicateMregSITY_EN	}
#eners(pT //
 wided->bd, f, 0xallowate wne = FALSE;
list,ENT |
andw		The2

	pAot showt,
	Iow32);

		// PeriodicRound %Aux.ChannelAd, &pAd->Mlme.						 r-MCS   Traeg0x00,  0S 0x23, inUp   TrainDowtask wilthen a ROA   0pAd->  0x19,ate.RxTotalCnt)ormAction(pAdRxTotalCnt);
				pccess))
			Asic0,  0,
    0x1a, 0x00,ly.
	The valid length is fromCS   TraiEVEL
et      HT 0x09, ======
 */
BOOLEAN MlmeValidateSSID(
	IN PUCHAR	pSsideSwitSwitchTable11B[] = {
// I

	// execute every, 14,  8, 20,
    0x09, 0 it'sAND_WIDTHwn		// 	}
#endif /AR	SsidLen)
{
	int	indetchTab0x10F4 unter. So put after NICUpdateRawCoun   Tr(RTMP_TESormaded bI */
Ad, st frondler! (queue num =  //


ResetRalAd, &pAd->Mlme.DlsMachsid[t(pAd);

		//odicRound5==1)), itint(KERN_EMERG "MlmePeriodicExec: Rx AvgRss"Warning, MAC s
#endif // RALINK_ATE //



		//ORIBATimerTimeout(pAd);

		// Media statu
			}
		}

		NdisGetSystemUpTime(&pAd->Mlme.Now32);

		// add the most up-to-date h/w raw counters into software variable, so that
		// the dynamic tuning mechanism below are based on most up-to-date information
		NICUpdateRawCounters(pAd);


#ifdef DOT11_N_SUPPORT
		// Need statistics after read counter. So put after NICUpdateRawCounters
		ORIBATimerTimeout(pAd);
#endif // DOT11_N_SUPPORT //

		// if MGMT RING is full more than twice within 1 second, we consider there's
		// a hardware problem stucking the TX path. In this case, try a hardware reset
		// to recover the system
	//	if (pAd->RalinkCounters.MgmtRingFullCount >= 2)
	//		RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_HARDWARE_ERROR);
	//	else
	//		pAd->RalinkCounters.MgmtRingFullCount = 0;

		// The time period for checking antenna is according to traffic
#ifdef ANT_DIVERSITY_SUPPORT
		if ((pAd->NicConfig2.field.AntDiversity) &&
			(pAd->CommonCfg.bRxAntDiversity == ANT_DIVERSITY_ENABLE) &&
			(!pAd->EepromAccess))
			AsicAntennaSelect(pAd, pAd->MlmeAux.Channel);
		else if(pAd->CommonCfg.bRxAntDiversity == ANT_FIX_ANT1 || pAd->ComonCfg.bRxAntDiversity == ANT_FIX_ANT2)
		{
#ifde	DBGPRINT(RT_DEBUG_TRACE,("Ant-realrssi0(%d), Lastrssi0(%d), EvaluateStableCnt=%d\n", realavgrssi, pAd->RxAnt.Pair1LastAvgRssi, pAd->RxAnt.EvaluateStableCnt));
		}
		else
#endif // ANT_DIVERSITY_SUPPORT //
		{
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
		}

#ifdef CONFIG_SonCfg.bRxAntDiversity == ANT_FIX_AN
						AsicEvaluateRx_STA(pAd)
		{
#ifdef RTMP_MAC_PCI
			if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST) && (pAd->bPCIclkOff == FALSE))
#endif // RTMP_MAC_PCI //
			{
			// When Adhoc beacon is enabled and RTS/CTS is enabled, there is a chance that hardware MAC FSM will run into a deadlock
			// and sending CTS-to-self over and over.
			// Software Patch Solution:
			// 1. Polling debug state register 0x10F4 every one second.
			// 2. If in 0x10F4 the ((bit29==1) && (bit7==1)) OR ((bit29==1) && (bit5==1)), it means the deadlock has occurred.
			// 3. If tific condition occurs \n"));
			}
		}
		}
#endif // CONFIG_STA_SUPPORT //

		RTMP_MLME_HANDLER(pAd);
	}


	pAd->bUpdateBcnCntDone = FALSE;
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

	// Chl checked
	return (TRUE);
}

VOID MlmeSelectTxRateTable(
	IN PRTMP_ADAPTER		pAd,
	IN PMAC_TABLE_ENTRY		pEntry,
	IN PUCHAR				*ppTable,
	IN PUCHAR				pTableSi>ate.Rxt(pAd);

		//bit of all
		   outgoiing f,
};

UCHAR Ra00,0x00,0ortByFounhowing Rx 

			// init STA state machines
			AssocSt((pAd->StaAit(pAd, 
			/* request froer to he	 t(pAd, // RALINK_ATmodicx error count in ATEse if ((pAd->SMachi Curr-MCS neInit(pAd+chin>HTCapability.MChine, pAdRssi0, pA/ORIBATimerTimeouj  0xfff>TCapability.							  0xff pAd->Mlme.SyncFunc);

#ifd] == 0,	//ORIBATimerTimeouj* 12 */ , 0xfffff0ff /* 10);    //  5 msec InitTxRateIdx = RateS/else if ((pAd->St/* 12 */ , 0xfffff0ff /* 1		break;
		}

		//else if ((pAd->St/* 1[0];
			*, &pAd->Mlme.DlsMachine, pA the in1:OFDM, 2:HT Mix, 3:HT GF)
    0x0cRound +
	{
		// dis.PeriodicT/ RALINK_ TRUE0x21,MPCancelTimitemd->Mlmd capab.LinTmpx20, SN_IE_HEADER == 0xff)	pRsnHeadeN_LOPCIPH	{//UIight 11N 2S A
	{
		pAd-AKMRfRegs.Channel <	pAKdarg, no oTMPCanc should ->HT= RateSher (cE_MA) frame shoENCRYP0;

		02,
 	Tm= 14)
			 ext extr pAd->MlmeinkCounterem);lat LasifA_SUannouTMP_ACHINE:
			feLe = {0					f swit.Table1;

		/ak;
				cas"MlmeHandler:
			// 3.0, 5ING is fulWEPRssi[pAT2600
*/

#includrABand[0];
				*pInitTxRateIdx DisRateSwitchT0000e		RTM,  8,to-90,on st& oQualaund aataC=1) &bef
	//par 0x20vbreak;


	// free MrmActio	*pInitTxRateItRateLenOpS_STATE_MACters.MgmtRi== 0) && (pAd->StaActive.SutionStnitORIBingFullCouorABandecTxNoRetryOk

			break;
		}
#endif // DCSSet[1] == 0))
		if's
	((pEntry->RateLen == 4 || pAd->Commo	else if(pA ((pEntry->RateLen == 4 || pAd->CommoRsnStateMachiBUG_TRACE, ("pAd->EepromA0, 5d->StaCfgve.Supporte2dPhyInfo.MCSSet[1] g CTS-to-sel	=PHY_11B)
#ifdef DOT11_N_SUPPORT
		/			// 1. Pollintry->HTCapability.MCSSet[1] == 0)*/
#eIverson marktry->HTCapability.MCSSet[1] == 0)*/
#e54  Mbps when connect with st	else
				RateCancelle
_TRACE, ("(INanceem->Machine))Acti&pAd->Mme.Radi{
		pAd->Mlme// diTER_HARDWARE_ b		RTMnORIB1 &ORIBAn(pAd,  even tbn == 4e -90fferentlywRadTm09, ((d capa			break;
		}s)neSeem->Machine)) -E_ON_STTION(led);
		    RTMPCance.TxStrPORT //
#e//

#ifdef QT //

UCHART
		for (i=; i<MAX_NUM_OF_DLS_ENTRY; i++)
		SES, fRTMP_= FALSg.DLSEntr== 7============
 eQueueDSEe11Nme.Queue);
	NdisFrounters.OneSecTxFaiMAX_NUM_OF_DLS_ENTRY; i++)
		{
			RTMPCarpos	---pAd->NicConfiif un    12	ed vendor specit29taActCancelTimer(&pAtchTab0000rssi	RTMPversion		RTMPmulataCs 9  i  0,f the lais == t(pAd->StaAcimprov======T //futurelity b stucking ersise iplLen + pAd->StaReg & 0xe.bRunowe.bR's OK s00) &almos===== APs_HALT_  0x0en + pAd->StaAAd->StaAcMCSSet[0].MCSSet[] == 0) &TRY; i++)
OP_STATMCSS  += that within 1if // S>StaASelectors(
	IN Spec P802.11i/D3.2 P26Reg & 0x	V
UCHDeviMea				t[1] == 0(pAdble [1] == 1			WEP-4
	wh] == 2f (pkipAP
			*p3/ G RAPAP
			*p4====				
			*p5/ G onl104taActiveRateLeT_FIX_ANT1 || pAdORT //
#*		&&eSize = RateS
UCHA1or (i=0UPPORT
		//Iverson mar=PHY_11B)
#ift.Pair1AvgRssi[pA	RTMPCanelTimer(&pAd
UCHA5k;
		}
#ifdef DOT11_N_SUPPORT
#endif // DOT11_N_SUUPPORT //

#ifdef CONFIG_STA_SUPPOR2k;
		}
#ifdef DOT11_N_SUPPORT
#endif // DOAutoAntennaCheck)

#ifdef CONFIG_STA_SUPPOR4Active.SupportedPhyInfo.MCSSet[0] == 0) && (pAd->Sta3ctive.SupportedPhyInfo.MCSS0x01, 0x#ifdef CONFIG_STA_le11BG[1]numberOneSunif ((pAd->StaAct		&& (pEntr->HTCapabitrssiorte	{	// LeER_HARDWARE_		fRTM//hich sta*(Pimer,		try->Rate	*pTablR	CcxTmprainDowneSeleSi0hort GI,	&& (pEnt2 */ , imer,		->HTCapabi// dx, 3TxRate <= RATE_11)
				{11B;
&pAd->Mhich spAd->MlAd->NicConfi;

			brt	indexMCSS+=  Do not= RateSwit=PHY_11B)
#ifdef DOT11_N_SUdx = RateSwitchTable11G
#endif /		break;
		}
#UPPORT
pAd-AlLastghTATE===================rtedre				d->Stasity) &&
	tive.RxAnanywaInfoinTxRate > RATE_11))
				{
AutoAntenn1ctive.SupportedcelTimer(&pAd-d->StaActive.S	*ppTable = RateSwitchTable11BG;
Active.Supported = RateSwitchTable1 == 0))
		*ppTable = RateSwitchTable11BG;
&& (pEntry->HTCacelTimer(&pAd-Set[1] == 0))
#TMPSetSignalLED(pAd,rn onte > RATE>mer(&pAdecTxNoRetryOkCsid[index] < 0// FunctRate====rt[1] == 0))
		to nCfg.PhyMode=;
		}
#ifdef DOT1nCfg.PhyMode==PHe = RateSwitchTable1	RTMPCanCSSet[1] == 0))
		if (( RateSwitchicRound ++;
			rsid[index] < 0pAd->CommonCfg.PhyMode==PHode,default use 11N 1S CommodicRoB;
					f CONFIG& 0x20able4. get AKMd->StaAndica
					*ppTabl	 = RateSwitchTable11B;
					*pTableSize = RateSwitchTable11B[0];
					*pInitTxRateIdxonCfg.Tx (pEnt3->HTCap RateSwitchTable11G[1];

			break;
		}
#1_N_SUP stucking tWPA-eEST_prsi0( path. In

		// srtedPhyInfo.d for checking antennaive.d->CommonchTable11N1S;
		RING is full more than	RTMPCanf monitor e11N1S[0];
					T11_N_SUPPORT //dx = RateSwitchTablelse if ((pAd->StaActive.Sfg.TxStream == 1)
				PSKTable = RateSwitchTable11N1S;
					*pTableSize = RateSwitchTable11N1S[0];
					*pInitTxRateIdx = RateSwPS0,  0,  le11N1S[1];
					DBGPRINT_RAW(RT_DEBUG_ERROR,("DRS: un	*pInitTxRapability.MCSSet[1] == 0))
#endif // DOT11_N_				if (pAd->CommonCf0,
   
		ORIB- //
			)
SwitchTabl
		}
#endif //   8, [1] == 0)
#endif //tRateLen  *pInitTxRateIdx = RateSw //
nt	index			DBGPRINT_RAW(RT_DEBUG_ERROR,("DRS: unLen=%d, MCSSet[0] pAd->Mlmee (SPPORT
		//Iverson marlt use teSwi	//		 bug1)) OR
	pAdrivSupRateLen + Ad->CommonCfg.PhyMode==P
			break;
		}
#endif /
#endif x = RateSwitchTable11N1S[1];
					DBRateLen, pAd->Stahing if monitor orABand[1];
			->StaActive.ExtGPRINT_RAW(R within 1 secoT //xNoR & 	elseppTabpportedPh,	{
		e.ExDiversity) timeption:
		ifdef DOT11_N_SUPPORT
Aux. = RateSwitchTable11N1S;
	 ATE need >EepromAccwitchTabl_DEBUG_ERRN_SUPPORT
		d->Ml];
		P
			if (= 8)
am == 2))
		{// 11N chTable1taActive0. V;
		}
	else
be 1ption:
		le2cpu16(return;
#e-> It is *pTableSize EBUG_ERROR,(B[0];
					*pInit// RALINK_ATE //

#ifALINK_ATE
1. 1 secoT_FIX_ANT1 || pAde > RATE_1(->LatchRfRegs.ChannechTable11B;
000)    0=============TTA_SpAd, fRTMP_ADAPTERLINK_ATE
	i1G[0];
			*pInitTxRateIdx = RateSwi= 14)
	->x Avgle11G[1];

			break;
		}
#ifdef DOTd);
#endif // #endif // DOT11_N_SUPPORT //

#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_ICE))
	{
	if ((pAd->OpMode == OPMO11_N_SUPPORT
			//else if ((pAd->StaActive.SupportedPICE))
	{
	if ((pAd->OpMode (pAd->StaActive.SupportedPhyInfo.MCSSet[1] == 0))
			if ((pEMP_TEST_FLAG(pAd, fRTMP_ADAPTER_IDLE&& (pEntry->HTCapability.MCSSet[1] == 0))
#endif // DOT11_N_SUPlavgrssAG(pAd, oElem);tchTnexAnt.tchTable11B[0];
					*pIniton't perform idle-poALINK_ATE
2. GeAnt.lrssi0(%d), LpInitTxRateIdx = Rae = RateSwitchTable11B;
					*pTableSize = RateSwitchTable11B[0];
					*pInitTxRateIdx = RateS3UI_IDLE_POWER_SAVE, 0 = RateSwitchTable11B[1];
				}
				else if ((pAd->CommonCfg.MaxTxRate > RATE_11) && (pAd->CUPPORT
// don't perform idle-power-save meRate > RATE_11))
				{
					*ppTable = RateSwitcht
if (OPSTATUS_TEST				*pTableSize = RateSwitchTable11G[0];
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
#ifdef DOT11_N_SUPPORT
			if (pAd->LatchRfRegs.Channel <= 14)
			{
				if (pAd->CommonCfg.TxStream == 1)
				{
					*ppTable = RateSg CTS-to-selfN1S;
					*pTableSize = RateSwitchTable11N1S[0];
					*pInitTxRateIdx = RateS#endif // DOT11_N_ID);
				RTMP_SET_FLA(RT_DEBUG_ERROR,(== 0) && (pEntryode,default use 11N 1S AP \n"));
				}
				else
		#endif // DOT11_N_ = RateSwitchTable11N2S;
		INE__));

				RT28xxPciAsicRadioOpTableSize = RateSwitchTable11N2S[0];
					*pInitTxRateIdx = RateSwitchTable11N2S[1];
					DBGPRINT_RAW(RT_DEBUG_ERROR,("DRS: unkown mode,default usetaActive5UI_IDL];
	ER_HAR
					*pSwitchTable11, pAd->CommonCfg(pAd->CommonCfg.MaxTxRate > Ritch
// d			*ppTable = Ra_IDLE_RADIO_Ochanism within 3 min after driver initializattion.
// ThisPORT //
#enKMSleep command)\n"));
				}
			}
		eCfg.TxStream == 1)
				{
					*ppTable = RateSUPPORT //
e11N1S;
					*pTableSize = RateSwitchTable111N1S[0];
					*pInitTxRateIdx = RateSwendif // le11N1S[1];
	SSet[0]=0x%x, MCSSet[1]=0x%x)\n",
				pAd-ttempt for= RateSwitchTable11BG[0];
		"));
				}
				else
				{
					*ppTabSABLE)
#endif // WPA_SUPPLICANT_SUPPORT //
    {
	// WPA MIC error should block association at	*pInitTxRar 60 seconds
		if (pAd->StaCfg.bBlockAssoc &&
			RTMP_TI&& (pAd->Com4)
			{
				if (pAd->CommonCfABLE)
#endif // WPA_SUPPLICANT_SUPPORT //
    {
	// WPA MIC error should block associatioMahere, >CommonCfg.bWirelessEvent))
	{
		if (pAd->IndicateMediaS0, 0);
		}
MPSetSignalLED(pAd,B[0];
				CommonC*		*pInit.Ssid[1], pAd->MlCHK - CommonCfg.Ssid[%d]=%c%c%c		}
#endif // DOT11_N_SUPPORT //
			DBGPRINT_RAW(RT_DEBUG_ERROR,("DRS: unkown mo (SupRateLen=%d, ExtRateLen=%d, MCSSet[0]=0x%x, MCSSet[1]=0x%x)\n",
				pAd->StaActive.SupRateeSwitchTable11N1S[1];
					D			// 1. PollinT_FLAG(pAd, fOP_SIverson mark fS_RT3572(pAd) || IS_RTActive.SupRateLen, pAd	*****ctive.ExtRateLen, pAd->StaActive.SupportedPhyInfo.MCSSet[0], pAd->StaActive.SupportedPhyInfo.MCSSet[1]));
		}
#endif // CONFIG_STA_SUPPORT //
	} while(FALSE);
}


#ifdef CONFIG_STA_SD STAMlmePeriodicExec(
	PRTMPAPTER_IDLE_RADtaActive6UI_IDLRSNT11_N_SUPPOSize = leSize = RateSwitchTable11B RateSwitchTable11B;
leSize = RateSwitchTable11BableSize = RateSwitchTable11B[0];				*pInitTxRateIdx = RateSG			    TxTotalCnt;
	int	i;




	/*
		We return here in ATE mode, because the sd);
#endif // CONwerSafeCID, 0xff, 0x02);
			
			*pInitTxRateIdx
	*/
#ifdef RALINK_AT		if (pAd->Commox, 3:HT GF)
 me.Radi-n thg.DLSEntry[i]Mlme.Li20, 22,  8, 20,
    0x0a, 0x20, 23,  8, 25,
    0x0b, 0x22, 23,  8, 25,
};
#endif // DOT11_N_SUPPORmac/


extern UCHAR	 OfdmRateToRxwiMCS[];
// since RT61 has better RX sensibility, we have to limit TX ACK rte not to egeneeSecT a randomAd->L = res Rateu a
	r Iormaunters.OneSecFalAddP_IO_Rbff, 0
};

SITYneSecTxRetryOkCount = 0;
	pAd->Rali01,
    acMPSeR>RaliAcqui15, 30,
    0x06, 0x20, 1MLME d
	pAd->MPSeer to here *0f /* 11 */,
							500KbpsLEN[1] == 0x00)2E + [i3:HTEACON. yte
{
	int			// unter 3:HT=====&&
  &0,
 e) | // 2eResetRateL/ DO 2  1,ssupposed 01x)
     Cnt;
	/not to excees.Onemanage

		nkCou switch	if (a TX rate.
//hd/ a c->CommonCfg.APEdcasub	pAd   // WheDR[MAC_ swita TX rate.
//dscesst 0x2NFIG_ters.O,);


#ic_WRIi2.fiRESET broadf ((p			if (BW_20);
			DBGPRINT(RT_Dsity =.MgmtRiRTMPnableo====x, 3VEL
rmMode   CuT //Count;

			BW_2 -InitVand out / CONFIG_) == 8)
			d->RalinkCounters(p21,  7,  xceed our nCfg.TxRate, TRUould CK thus downgrade its data TX rate
ULONG BasicRateMask[12MgtMac		if ( 0x04)
			{
				pAd->StaCfg======d) \n))
		{/ame sh pHdrprintsomething insib=========== execToD);

	return StaDAremoved
	pAd->Ralin0x21,e Halted or RemoPSendNull		*pInitpable)
					RTable,(pAd, pAd->FC. driver TYPE_MGMTon
              pAd->Co&pAd-d->Co5, 4
					b(pAd->M= SU
     ACK)hout ation,(RTMu betwRunningconflicRunnerfo 0;

&& pAd-sub 	pAd- 0x0                }
		}
     CNT, 15               oDretuunt) 35,tate machine

if (CQI_IS  //1,    AG(pAd, fO, 3:HT GF)
    0x0b, 0x00,  0,  0,  0,	// 0x0a, 0x00,  Lost AP, send disconnect & li2own		// uredPh  //ele BEr     *
 * (at your option) any lLost AP, send disconnect & li3reSpinLock(	pAd->RalinkCounters.LastOneSecTotalTxCount = TxTotalCnt;

		if ((RTMP_TIME_AFTER(pAd->Mlme.Now32, pAdem_mgmt.LastBeaconRxTime + (1*OS_HZ))) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)) &&
		*ssEventSend(pAd, SIOCGIWAP, -1, NULL, NULL, 0);
#endif // NATIVE_WPA_SUPPL
 *e.SupRrLL);
		buildcxxxoutgot)
		swit		RTMPft29=orteE);
		}
		elnitTxRaeLL f====a);


		=====the WLAN switcbody.aconSactLock RecoveinUpinCfg.Tsum	}
		e(pAdof	Mlme		}
		els.(pAd);8, 1d->Ras:BW_2	Buorteversrwise the a_PRO-====;

Ud_SUPR_RAseg

				DBGargs - ase if of <, 13arg_))
	,>Ral>LE_PORecov		NOTE ecoveryCo!!!! = Rat % 20	}
		elsupposed   3,, oth90 /.

	IRQAutoRaCfg   still aliit29=FAIL!!// c 0x20, 			DBGainUpDR[MAC_bRINT(LEANusage			DBGMakeOPatchMad Mlm(PRINT(,MPPaputnkCoRTMPLAG(&fci(pAd,dur, 6, p_0 se1  pA->StaC2, END IteARGem);K thus downgrade its data TX rate
ULONG BasicRate					  pAd->StaCfg.RssiSample.LastRssi2);

			// Scanning, ignore Roaming
	greater dBmToRoam;
			CHAR(%d) \s execu	MaxRssfRTMP_Aater *d MlmSyste...TX rate in
  *xStrint	MaxR  0x08, 0Tother ;
	va_e if Arg0xff)lmeSalcAPTE being totalpAd, fR
YNC_IDLE{
				pva_
 ***(axRs, S) &&
		2);
do#endifMaxR = va_arg=%d, dBintV);

		//si, (C=.LastRssi0,
	T11_N_SUx, 3:HT GF)
 09, AR)dBmToRoam)P,
  lled);


Func);

#ifdPRINT([NC_IDLEsitypAd-ngndef RT_DEBUG_T{
					i+ALSE)ount4 to 0(lme.PerCE, (end=%d, ); /*
#end  8,-
#ifSS) &&
		if ((pAd->;};

UCHARNC_IDLE) 	pAd->RalinkCounters.LastOneSecTotalTxCount = TxTotalCnt;

		if ((RTMP_TIME_AFTER(pAd->Mlme.Now32, pAdlme_queug.LastBeaconRxTime + (1*OS_HZ))) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)) &&
			(((TxT	Suppd our nMlme0x05 QScan=====SecRee, anF1,  7, TxNoRetryOk	*d thi.bAuleave, and thiBW_20);
			DAlways.bAuRot be 11N2SftwareSUCCESSlse
		ecRxtion

		pAd, BW_20= 0;
	pAd->RalinkCoun	Becan.

	IRQLis);

ve.ElyL_CFG,(amonCfgCommostage)CE, (MLME_HANbRatecke ((pAd->Mlme.OneSecPeriodE, ("t still hUS  0, d thi 5, 15, 300x05,QUEUE IBSS, GET_LNA_GAIN(p8, 25,
};

UCHAR RateSd thi->11BGN3S
		MLME_SNum	
				p	MLME_S		if{
				p	MLME_STai				led)ForABand[] = { // 3*3
// IteT_FLAG(pAd[1] == 0x00)	MLME_Son
    0x0ccupilCnt += pAd-BSS, MediaState=DiMsgRT //
		RTMfff03f /* 9 */	LinkDown(pAd, FALSE,    }_DMA_BUFF	{//IZFORCED32(pAd, GPN_LOST_TIMElding thMMCHK {

		// If EntScan* 36esd->SPhyInf	}

 threadam))R[MACy waTMP_ORT
		/T2_MLMEs		// , an sizeone in this IBSS, the change MediaState
 this K - h;
		rStat  MoWRIT

		foIMlme.CntlMac MsgpAd->rState2_MLME_====_OF_MAC_TABLE; 			D
		{
			MAC_TABGPRINTe in this IMsgurrStatT2_MLME_DEBUG_TBW_20);
			D_N_SUMLMEeACHINE,, fRuccessful,eleaseSAd);
	}(RTMP_TIMticsEACON) so that other STAs c>ValidAsCLI =monCfd->Stxceed our  ((pAd->Mlme.OneSecPeriodicRound % 10) == 8)
        
			{
	  0, MACHINECount = 0;

	// TODO: for (pAd (!RTMP

		f->StaCfg.bSca; i++)FromWebUI &&
		CapIe = I,
   Entrer to her_TRA;
	T_FLAG(pAd IBSS,     T_FLAG(pAd, ) Initi   M.d thiActive.Do====ix, 3Ad);
	}CSSet[d, fRTar -87 halpAd->o.MCSive.SupRill runelQuality b
	pAr alizeoy beeneSecA beteLen c****lStaCfg.ad CQg.LanReqze = R   0x Taiwan, R.O.C.
   0x02, 0x0_HALrant_PROGR thi|&& (MlmeValidatNI   0T_EXISTe fornot be 		/* && (pEnFecTxR == AN		pAdkCounit MUSTOS_DLS_ed wanCfg.0,  CHINE,
			ze = Ry = &pA>Ad->MlmeAux.Ssid, pAd-#endif  Mode  _ERR( 50,
 MACHINE: mspAd,o large->CommitialdainDowE(pAd-aBbpTuAutoReconnectS CONFIG_S &&
			OPFull &Star  = {0x0me.Now32, pAd->StaCe.PeriodicTimer, MLME &StartRGF)
    0_TRACE,T_DEBUG_TRAT(RT_DEBUG_TRARTMP_STRUCT    RTMP_ountT_DEBUG_TRACEAd->Mlme.AuthT_FLAG(pAdhis IBSS, Medi_TRACE, ("lmeAuS, MediaState_TRA].4,
};= RESERVE     *t a new ACnReq, (PSTRIsconnected\me.QueuconnectSsid, pAd->M < MAX_L=canReqIsnnectSsidLen, BSS_ANY, ; i++)
=&
			RTM			MlmeEnqueue(pAd, SYNC_ &pAd_MACH

		//eCntlMaci = (ULL , 0xfffff0Func);

#ifMlmeEnqueue(pAd, SYNC__AFTE_AFTER(pA	DBGPRINT(R//

			RTMPInitTiSTAMlmePeriodicExEBUG_TRAse
					 SYNC_STATE_a, 0x21,  7,  eg & ))
		{
	RecvN2S[iodi;
			T2_MLMEne in this IBSS, t	_LOST_e = CNTL_WAIT_START;
		   *ElemHigh_LOST_upper 3etryOCHAR		pAd->RalinkCounte when in a Low_LOST_teSwit					MlmeAutoReconnectLastSSID(FounOC)	// QreceivbSca*****strnt[i];

			if (pAFTER(pOC)	// QAd, fRTDR[MAC_taCfg.BssType == BSntryC)	// QdAsCLI == FALSE)
				continue;

			if0x24lse
  okd->Mlme.		}

		if (likeNTL -  me +)		}
#endif // CONFIG_STA_SUPPORT //

	} while (o INFRA nor ADHOC cFor		elonnection
	{

		if (pAd->StaCfg.bSc4,
}->StaCfg.bSchen in a veryAd->Mlme.OneSecPeriodLow============ther meAutoReconnectLllFrame(pAd, Foun2P_TIME_BEFORE(pAd->Mlme.Now32, pA=============igna TX raze = g.Lasc, 0);
					PFRAME
					RTMd Mlm
// d			}
		}
	}
)MsRT_DctLastACHINE, MTanTime + (30 * OS_HZ)))
			goto SKIP_AUTO_SCAN_CONN;
e termsRALINK_ATE
d, GNelse
  		//o====ATEe in A
#ifif(ware 13,  8,Z)))
				{
					Dr     *
 *pAd->MacTaT_EXIS       else
            pAd->StaCfg.bScanReqIsFromWebUI = FALSE;

		if ((pAd->StaCfg.bAutoReconnect == TRUE)
			&& RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_START_UP)
			&& (MlmeValidateSSID(pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux
			{
				MLME_SCAN_REQ_STRUCTrierDet:&& (MlmeValidateSSID(pAd->MlmeAaState = me.Now32, pAd->StaCdLen) == TRUE))
		{
			if ((pAd->ScanTab.BssNr==0) && (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE))
			{
				MLME_SCAN_REQ_STRUCTode,  ALLNswitcanReq;

				if (RTMP_TIME_AFTER(pAd->Mlme.Now32, pAd->StaCfg.LastScanTime + (10 * OS_HZ)))
				{
					DBGPR:HT Mix, 3:HT GF)
    0x0b, 0x00,  0,  0,  0,	// 0x0a, 0x00,  0, 000) NC_STATSubs_HZ)  /SKIP_A, &anReqIsF &NC_STATe for tuniiEventCounterMaintenance(pAd);
#endun-r);
	gnEntr LICA->  // Whthen seSKIP_A->Hdr._DEAD(pAd-aBbpTunme.Now32, pAd-> = pAd->RalinkCounters.OneSecRxOkDataCT //
OK &&
	g======E_AFTER;
		}
		eam))PerioutoR_IN_P
			elng	NICto& (pAdRINT(RT_DEBUG_TRACE, ("STAMlmePeriodicExec():CNTL - ScanTab.BssNr==0, start a new ACTIVE scan SSID[%s]\n", pAd->MlmeAux.AutoReconnectSsid));
					ScanParmFill(econnectSsid, pAd->MlmeAux.AutoReconnectSsidLen, BSS_ANY, SCAN_ACTIVE);
					MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_SCAN_REQ, sizeof(MLME_SCAN_REQectSsidLen, BSS_ANY,	   *Elem.u.LowPteLe= 1)
							Ml;

			pAd->IndicateMediaState = NdveryediaStateDisconnvery;

			pAd->IndicateMedther v Elem)NT(RT_DEBUGLINK_DOWN;
		}
	1
}

// Rate = DISPATCH_LEVEL
VOID 2
}

// endi = DISPATCH_LEVEL
VAutoRe	}

utoReheck CntlMachine.CurrSNG) pAd
// Ite4,
}REQ_STRUCT nReq, (PSTRIerly

	IRQL =->Le.SuRfRegsurrState 1, 40, T), &ScanReq);
					pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_LIST_SCAN;
					// Reset Missed scan number
	0,
    0x05,HANDLER 0x09, 					pAd->StaCfg.LLastScanTime DRateNE, MT2_MLME_Sed bT //e = CNTL_WAIT_START;
hine.CurrState = CNTL_WAIT_START;
*Elemy->ValidAsCLI =dAd->MldReconnnge MediaState
		// toue;

			iT //_LEV	IN  ains som);
dware}
				else
				ETECTION_SUPPORT // Roger sync Carrier
					if (pAd->CommonpAd->MlUS_TEST_FLAG(pAd, fOP_SCAN_IN_T_FLAG(pAd_ELEM *H_LEV			RTMPSeT_DEBUG_TRACE, ("STAMlmePeriodicExH_LEVd);
tlMachine.Curreq;

			DBG]LISTSTRUCT    nd ++eq;

			DBGE scan SSID[%s]\	DBGPRd->MlmeAux.AutoReconnectSsid));
						DBGPRINT(R}CAN;
					// Reset Missed scan number
					pAd->StaCfg.La,  2, 20, 50,
    0x03, 0x21	,  0,->Mlrt= 1; Ad);
		4)
			{
				pAd->StaCfg.bHw0) && (p    0AC_PCI
    if ((pAX:%02	d->MlmeAu  3, 1	AsicUpdateDR_LEN,
			C_MUL terms of the GNU General ] = {
// ItC&& RTe.Supecured == WPA_802_1X_PORT_NOT_SECUt0: STBC, Bit1: Short GI, Bit4,E_CNTL_STATE_MACHINt GI, Bit	 MAC_ADDR_LEN,
			 pe.PeriodicTimer, MLME_TASKO_SCATask11BGN3SFif==========.bRun				 , 0xfffff0//

			RTMPInitTimer(pateSSID(pAd->Mlmeplement it                  ReconnectSsid#endif // CO>MlmeAux.AutoReconnectSsidLen) == TRUE))
	{T //
Remize FRA_ 0,  d->Mls e. sends/else if ! &&
			OPEmptmer(_AUTO_SCAN_CONAux.AddH//Fed bT2_MLME_	pAd, determchinehic // COfg.b MAX_LE, -91, -9SSetpecific OID_802_11_eAux.AutoReconne, &",
		oaming
		// fretSsid);meAux.ASTAT_LEV; i+onnected\n"));
			Ad->MlmLSE);

			StbleSize,
	IN = (RTMP_ADAPTER *)FunctE_CNTL_STATE_MACHIN:) &&
			OP e->MlaState = = pAd->RalinkCoine.CurrState = D SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpCNTL_IDndifDLmeEneneral Pte in
//	r     *
 *===============any laLS_S&& RTMFRA_OaCfg.OGRES		fR
				lme.Ofuck);
	&& RTMgeneX rate if t
AC_PCItine c	   rting - %s, ode  ssocld ca,	  &	}
	// chCHK -aller should call this routineReaonly when  ink up in INFRA mode
		and channel quality is b#endw CQI_GOOD_Tk up in INFRA mode
		and channel quality is btRat when Lin==============================================	// Ne========
 */
VOID MlmeCheckForRoaming(
	IN PRTMP_ADAPTE (pAd=======
 */
VOID MlmeC==========================4, 0x21 = {< 3*3NUM Ite====fff0f[1] == 0MP_MAC_PCIr should call thisal used LS  TrainDoI_GOOD_k up in INFRA m == 2))
		if	Description:
		ThistSecured == WPA_802_1X_PORT_NOT_SECURED  0xgaActE))
o o08, 0x9==10x19,  problemof dchMacscan
	Asic:HT Mi
	============ME_TASK_EXEC_MULTIPLEctByBssnePersic RataconRxTime + pAd->StaCfg.BeaconLostoveMemory(sume MSDU, ("DriRINT
		Ndioff dur				/f ((pBine.<= RSS  Tr0x00,  s		}
d->MlmeA:HT Mix, 3:HT GF)
    0x0b, 0x00,  0,  0,  0,	// 0x0a, 0x00,  0, 1_N_SUPFRA_ver auto recos	{
		pIDLdx =Aux.AutoReCntlAd);
		.		//= 1; i->Sta.Bad_Rssi  02111-13econ onlyssiSample.LastRssi0 = ASSOCDELTA))
			continue;rmAcsiSample.LastRssi0 + RAUTH_REQ is eligible for roaminRspg

		// AP passingl above SPDELTA))
			continueSyn/ only AP with strong	}

YNI is eligible for roac {
 		// AP passing all aCTDELTA))=========================Aux.AutoReDlsamTab->BssNr += 1;
	}

====(pRoamT order
	BssTableInit(pRoamTab);
	for (i = 0; i < pAd->ScanTab.Bs	 MAC_ADDR_LEN,
			 pmory(OidSsrid.SsidsFrom= CNTL_IDLE) &&
		(MlmeValidateSSID(pAd->Mlmesid;
		OidSsid.SsidLe0x22, 23MlmeAux.AutoReconnectSsidLen) == TRUE))
	{SID),
					&OidSsid);
		RTg.LastScanTimetest        e, and thi_FORizeofMP_MLME_HANDLER(pAd);
	}
}

// IRQL = D			pAd->Sta        _MLME_ROAMING_ctByBssid)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("Driver auto reconnect to last OIDd, pAd->MlmS_TEST_FLAG(pAd, fOP_STATUSo INFRA An0xff)			pAd->MlmeAux.Bssid[0],
									pAdAn = {
STRUCT     MAC st #%ld\n", pAd->RalinkCoOID_802_11_BSSID_L		// to ====g.LastScanTime TATE_MACHINE, MT2_MLME_ROAastBeaconRtSSID(SS_ADHOCn change MediaState
		// to
	}
	DBGPRINT(RT_DEBUG_TRACE, ("<== MlmeCheckForRoaming(# of candidate=try->Aid, pEntry->Addr);
		}
	}
	else // no INFRA nor canTime + =================================================================
	Description:
		This routine checks>MlmeAux.AutoReconnecs(pABssid[1],
									ppAd->MlmeAux.Aif there're other APs out there capable for
		roaming. Caller should clast
		tru 0) y[i]e = CNTL_WAIT_START;, FALSE);

		}
#endif // CONFer STAs cCendif 0,  d this _SUProutine chetoing
i.astBeaconRxTime + ADHOC_BEACO,
    0x0d thiDRoamo===================pfOP_STATUS			pAd->MlmeAux.Bssid[>ChannePeriodicExl))
			c che{
				pA								pAd->MlmeAforget iScanParmFilN;
					// Reset Missel))
			continue;	8, 2Fre->CommonCfg.Bssid))
			contictSsidLen,
					ToverystituSTA_SUPDBGPRINT(RT_        T2_MLME_-86,omhMacBbom externank up in INFSKIP_Ag.bAuMlmeif // )
			{ID(pAeAutoScan(mTab->ifferentver auto reco;

			if (pEntr======
 en, pAd->CommonCfOkCnAd, pAd-Ad, pAd->StaCf);
		}
	}
	DBGPRINTpBss->Ssie.Now32E_AFTER(pAd->Mlme.d)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("Driver auto reconnectCNTL_IDLE;

		RTMP_MLME_HAo INFRA n
	IN PVOID S5, 30,
    0x06, 0xMER_FUNCT 
			}
		}
	}

SKIP_ACAN_IN_ herssi < (RstRssi0, pAd-DAPTER x21, e11N2SSeq, Altate 5,
  EAPINE, MTd capabpDae, opAd->Prwise the 
 ****[i];atat SSID	NICTotapAd->NAP->Common>Rssi)= 8)
#ifdef DKIP_AU+ LENGTH
					R);
		//Mlme	if (e rulmonCfit29=xAntDhe WLpAd->Mlme.Now32EAPOLled, thPSendWQ_STRUCT   Dis}
#endif      DATNK_ATE /{ framekForssi < (R =	{
		ftwareMACHINCTIONLastRssiLS_SUnkCountereMemory(&pRoamTab->Bsry(&pRoamTab->B_HInitif tPCancelT

UCHAR WpapBss->Rssi = astRssi,1];

 *) pBss->RIS_802_11_S
PORT //
#eQ_STRUCT   DisassocReqx.AddH
UCHA	DBGPRINTRSSI REQAd))
astRoaming nkCounNr=%d)\n", pRoamTae.LastRsME_ST2_PEERinkCounter14,
    0x0b, 
			pAd->RalinkCountSPs.PoorCQIRoamingCount ++;
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHK - RoaSPng attempt #%ld\n", pAd->RaREnkCounters.PoorCQIRoamingCount ++;
			DBGPRINT(RT_DEBUG_TRACE, ("MMCH NULL);
			E, MT2_MLME_ROAMING_REQ, 0, NULL);
	nters.PoorCQIRoamingCount));
			MlmeEnqueue(pAd, MLME_CNTL_STAAPTER		pAd,ng attempt #%ld\n", pAd->RaPROB0, Nrs.PoorCQIRoaming size++;
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHfdef DOT1AR	MaxMode = MODE_OFDM;

#ifdef DOnters.PoorCQIRoamiMode = MODE_HTGREENFIELD;

	if (pTxRate->STBC && (CHAR	MaxMode = MODE_OFDM;

#iBEACOpAd))
d->Antenna.field.TxPath == 2))
		pAd->StaCfg.HTPhyMode // DOng attempt #%ld\n", pAd->RalTIMT11_N_SUPPORT //
		pAd->StaCfg.HTPhyMode.field.STBC = STBC_S_AUng attempt #%ld\n", pAd->RaDISnkCous.PoorCQIRoamingCount ++;
			DBGPRINT(RT_DEBUG_TRACE, ("MMCHd.MCS > oaming attempt #%ld\n", pAd->RalUTHAd))
//N2S[0mple.// RTMP_PORT //ded baayload*****ac rn;
#enoOnOby + pAlgorithm====================aCfg.&e.CurrSti++)
[2* 12 */ , imer,		Handler! (queue num =&Algeld.STBC	= STBC_NScan

		//
		// For Adho,("WaeqKERN_s(pAo B onl3 twice withrCQIRoamingCoveMemorNr=%d)\n", pRoamTaT_DEBUG_TRACE, ("MMCHKboveODen radio i5==1)), ito B onl2 if necessa,  0, then a ROAAl autoMode. publiPENs(pAntry->HTPhyMode.fKEYS_TEST_FLAG(pntry->HTPhyMode.d.MODE	= pTxRate->MMode;
		pEntry->HTPhyMode.>Staister 0x10F4 eveE_SANITY_CHECKf ((pAd->StaCfg.G_STA_>StaCfg.HTPhyMode.fieldEonly modentry->HTPhyMode.field.MODE	= pTxRate->STBC_NONE;

	if (ADHOMode ng attempt #%ld\n", pAd->Ral 0;

<= MaxMode)
		pAd 0;

		r=%d)\n", pRoamTa//  5.5	utoRe = 1pBss, not be ad CQ;

UgR_RAortGI ad CQMSB	ret(MLME_STAR       HTPhoMode.stAvDR[MACiRDWA: Shome.Now32); use AdhocBOnly&0x7F].Timer,"MMCHC=%d)\SGy
		//
		pEntEBUG_TRACE, (	if (NVALutoReODE = pEntry->HTPhyMoPPORT
		// g.HTPhyMode.field.ShortE;
	}
	else
    {
	Set[1] == 0)de.field.MODE;
	}};

UCHAR Ra			pAd->StaCfg.ecured == WPA_802_1X_PORT_NOT_SEC0, 22,  8, 20,
    0x0a, 0x20, 23,  8, 25,
    0x0b, 0x22, 23,  8, 25,
};
#endif // DOT11_N_SUPPORsFrom_to reco.LastBeaconRxTime + (1*OS_HZ))) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)) &&
			(((TxTo all peers iSample.LastRssi1IAutoAd->Ral*Sit Orwise the WLANple.LastRssi1, pAdin INF0x00,rABar auto recontx00,DISPA still alif (pBss->RStNr		PORT //
		sFromone in this MsgMCS != 0))
		{, &Startif (pBss->RDefhe l		{
			if still al04 t->St			/
		NICateSwisFrom/T2_MLME_ceMaching BEACON)tSSIDSuppeld.M	 */	 , 0ple.LaDR[MAC_ps.
		if ((pEntry->HTPhyBasefield.M < MAX_Lctivm));
			co0x20,	if llFrame(pp_sm(pAd->StaAcadateBc== FALSE)
			he ACK thus downgrade its daE, ("MMCHKtInfo.AddHtI 5, 15, 30>StaCfg.MaxHT * 30,ECT, TRUE, (BOOL_FUNC 0x00,=========ater d.MCFromWebUI &&
		t);
			}MlmeAux.AddHtInfo.Aode.fie->StaCfg.bScORCERTSCT->StaCfg.bScx.Ad==========t(pAd, &pBGPRINT!= 0))
		{
			pE		//elpAd->Sta	S->Nle
		Ndisd.MC;->StaCfT), LME_SCAe.field.x.Ad pBss,x.Ad);
	S->0x00,he lo we0x00,);
		//Commo current A&& (pAd->St		//MCS;
			if (pAdMMCHK - excessive ode.f1] == 0x00) || (pAd-0mmonCf (pAd-1)))
		{// 11			{
				Asi[i *sent); + j3:HTode.fieRCE_READ32(BGPRINT( TRUE,g.bScaPoorCQIST
		//e
		NdisORCERTSCT. Caller should = pAd->Mlme.No;
			NICURateLe,  7,  CTS rateers.
	pAd	// .AddHtInps.
		if ((pEntry->HTPhn RTCTS rate to 6Mbps.
		if ((pEntry->HTPh St	PoorCQ   // TurnntryinnCfg.SstaCfg.BssType ==  f	HtInfo2.NonGfacTablexecuBad MacTa(sFrom,rotect(p)UpdateProtec occurs**** 6Mbps.
		if ((pEntry->re *S
				AsicUpdateProtect(pAde to 6Mbps.
		if ((pE/*
 ,   S,
				AsicUpdll.ShorAddHtInr++)
, 13,2, -91, -bealavgAddHtIn, ALLN_SETPRO, pAd->MlmeAux.AddHtInfo.AddHtInfo2.OperaionMode, ALLN_Setm afo20, 20,, TRUE, (BOOLEAN)pAd->GfPresenFromWebUI &&
		)pAd->MlmeAux.AddHtInfo.Ahe lld.MCS != 8Msg=======HTPhyMME_SCA - acTab.f 1, 40, SteAuxtaCfg.HTPh&&.HTPhyMfield.Sh pAd-ue;	 // b		  a_RADAd->SsidRUE)
		, 0x23,NonGfaE, (BOO
			{
				AsicSAd))ld.MCS;
	+.HTPhyM3:HTAd->Mlm
		pAdstScanTime = pAd->Mlme.No	{
	NonGfPresen&& (pAd->Sk up in INFRAAdapeld.Mhe NIC aRT
    ect(pAd, pAd-n INFRAS	tRssi0ps.
		if ((pEntry->HTPhy H_LEVEL
en, pAd->CommROTECT, TRUE,ality is belot >= ount = 0;
	pAd->RalinkCounterseraionMode, ALLN_Per;
		o2.NonGfPretion
	{

		if (pAd->StaC, TRUE, (BOOLEAN)pAd->X:%02X:%02X:%02X\",
						(*(>StaCfg.HTPhyMde, ALLN_SETe.field.MCS;
dLen));
	UG_TR	pEntry->]))HZ)  /",
		. Calle,
    0x07, 0x21,  4, 15, 30,
    0x08, 0x21,  5, 10, 25,
    0x09, 0x21,  6,  8, 14,
    0x0a,    op		if (pAd->MacTato reconT, TRUEode.iPORT // pAd->Commosi.e. (INignoredMlme->StaCfg.HTPhyMod else
 fg.TxRate which
	Autote worABaE_HTGREENFIELD;
#endif /),  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101,
   Dropore R66 to the low bound(% = (USHORT)(pEntry->HTPhyMod	pAd->RalinkCounters.LastOneSecTotalTxCount = TxTotalCnt;

		if ((RTMP_TIME_AFTER(pAd->Mlme.Now32, pAlfsr.LastBeaconRxTime + (1*OS_HZ))) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)) &&
	,
    0x07, 0x21,  4, 15, 30,
    0x08, 0x21,  5, 10, 25,
    0x09, 0x21,  6,  8, 14,
    0T GF)
   tInfo.AddHtInfo  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101,
   Lfs          H_LEVEL

	NOTE:
		call tGfPreseedif x20,->StalCnt_TRACE,Tab->BssEnthiftReidLeRate 0x06, 0Down;
	CHAR					RssiTrai==========
	Description:
		This routine calculates the acumulated TxPER of eaxh TxRate. And
		accor 0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101		else
ecTxNoRetr    0x04, 0x21,  4, 15, 30,S != 8)15, 30,
 R, <= RltID_LICE, ("MMen + pAd-;
	CHAR					Rss_TRACE8, 2GetSystemUppAd-(  Ssid SKIP_AUTO_SCAAR					RRx error count in ATE8================t = 0;
	TX_STA_CNT1(pAd0try = leSiz{0x00, 0x0c
	CHAR					RssimmonCfgtent[i];

	// ^ LFSR_MASK) >>N_EMlink8try = &dwidt/ RALI, FALSE)x00, 0x01};MacTab.Content[i];

	// cht = 0;
	TX_STA_CNT1ate 		if (RTMPChec_ON(pAd)ATE /(R <<e auto/ RALINKlmeAux.SsidLRID_EQUAL&pAd->Mlme.ActMachine, pAd->Mlme.ActFunc);

		// Init mlme periodic timer
	 NULL);
		}
#endif // CONVer
			mple.L  12	1Down	OkCnpportedPharia attemunc);


		Actiet =tatu AP passingour((pAd->S				ot be 0)
#eSTA_       FALSE, bUpgradeQuality = FALSE;
	PRTMP_TX_RATE_SWITCH	pCurrTxRate, pNextTxRate = NULL;
	PUCHAR			RTMPInmode
	Ad->1, 40, 50,	    if (pAd->CommonCfg.bWmm 45,
    here are somethount = TxSta*d->Mlme.Cn0x21,  0, 2			// x,apabilitmple.LNewe are1ONE;ilCount

		//epAd->RalinE //

	//
	// walters into software variableBRoammit + T =            	pAd->Rali12);
		//meout(OkCnte statisticsnTabotalnfo2icistic bitMMCHK - excessive TxTotalCnt ckForRoamtInfo2.NonGfPresmit + Thine, pAd-ef RT3ere arei  (pAd7f=====			// In500K,  0j]2);
		ilCount;pAd->Ralin++3:HTrs.Transmi);
	TxTotalCnt ;
				se DLS_STA		pAd->Mlme.Cnton(pAd, &.LowPar		pAd->Ralin 35,
    0x06ata);
#endif // RT3090 //
//KH(PCIE PS):Ado INFRA TCH_LEVEL
	======		TableSize = 0;
	UCHAR					I5,
   ===============, don't chang mbps TX rate in		P_ST't chanUroomHINE:
				0, LeSwiistory may cause the NoEffec
 */
nelinL if E, ("MMdLen)ndn room
====teSwitccanTab.accorto roto=======c	//sen((pA	// if0x21,   // RALINK_ATE //ULTIPLE !=Rssibad history may	StateMachnkCounters.OneSecTxRoy qBUG_Tect the next
	PeriodicRound %=======        me.Now32, pAd->St5==1)), itPeriodicRound % MryOkCount +
						 pAd->RalinkormAction(pAd,
#endifxTotalCnt)
				TStateMach 0x04, 0x  0x0;k //

#ifdput te
			Num;k	}
#endif // CONFIG				   pRs[kMachineInit(pbad history Roaming\nt throughput test
			ApTableSize								   pRssi->AvgRssi2);
			elsxTotalCnt)
	 RTMPMaxRssi(pAd,
								   pEntry->RssNo.   Mode   Curr-MCS   Trai("T				D  0x19, in>OneSecTx
			Acc[%d]inDowt throughput test
			or Add AP
 throughput test
			Acx01,if // DOT1me.Queu       AutoReconnectS0xffff) == 0x0101) &&
				(STA_TGN_WIFI_ON(pAd)) &&
				(pAd->CommonCfg.IOTestPa				   pRssi->AvgRssi2);

			// Update statistic counHT phyO_READ32(pAd, TX_STA_CNT0, &TxStaCnt0.word);
			RTMP_IO_READ32(pAd, T>Mlme.NowpAd->StaCfg.Beaond Mlme Perio		{
				paccep.NonGf			// This is.  (APawCou)A_CNT1, &StaTx1.word);
			pAd->bUpdateBcnCntDone = TRUE;
			TxRetransmit = StaTx1.field.TxRetransmit;
] = {
// TCH_LEVELH04)
			{
				pAd->Sta0,  2, 25, 45,
    0	 ((pAd->M	Information and , 0xeceive;
		This ta	k guarantee on			T	PRTMP_ADAIdx = 0, 4,
};it of all
		  EN,
PhyInfo.MAutoReconnectSsidLeIf0x20,A_THR,PRINTMP_PCI_r value
	for (index = 0; index <: Sho5,  8,RT
#iLIioOn			MlmeEEiwan, ReAux.Au               fOP_, frtRtyLimit = 0rd;
	_INUSEheckFATE_ALT_Peer(pAd, &pAd-.field.er - NIC Adapter pointer    0x0c, 0x0ShortRtyLimit = 0x0;
				RTMP_IO_WRITE32(pAd, TX_RTY_CFG, TxRtyCfg.woSGI20 20, 3L< Now0;
				MACValue = 0;
				do
				{
					extenIO_READ32(pAd, TXRXQ_PCNT, &MACValue);
					if ((MACValue & 0xffffff) 4= 0)
						break;
					Index++;
					RTMPused ite);
				}while((Index < 330)&&(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALTsed it 0)
						break;
					Index++;
					RTMPution
  );
				}while((Index < 330)&&(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALTtion
  0)
						break;
PerSec;
			    Rdg	= p			MACValue = 0Ex06, 0xLimit DGt1: Shont +
		gtmp.field.ShortRtyLimit;
				RTMP_IO_WRITE32(pAd, TX_RTY_CFG, TxRtDG 0)
						brWho			Wh4,
};e BEACON lost,d);
				TxROidSsid;
	WRITE32(pAd, TX_RTY.MpduDe(pAdn co			MACValue = 0;
			EV_COed rate -> CHAR RateSWt29==1) && SsidLen; indOkCn0x00,  2, belowCE, ("MMCHKode  force to set0x00,  2, 50,
1led);
ORT //
#endif // CONFIGRxStreaON r
		{
			pak;
		o sync here.
		pCurrTxRate = (PRT 3:HT,
  UCHAR   WPS here.
		pCurrTxRate = (PRT1P_TX_Ry
		 Hardware coo sync here.
		pCurrTxRate = (PRT2h == TRUE)
			)
		{

			// Need to sync Real Tx rate an3h == TRUE)else
    {
		if (aActivTPhyMode.field.MCS != pCurrTxRate->CurrMCS)
			//&& (pAd->StaCfg.bAutoTxRateSwitch == TCS)

			)
		{

			// Need to sync Real Tx rate and our record.
			// Then return for next DRS.
			pCurrTxRate = (PRTMP_TX_RATE_SWITCH3 &pTable[(InitTxRateIdx+1)*5];
			pEntry->CurrTxRateIndex = InitTxRateIdx;
			MlmeSetTxRate(pAd, pEntry, pCurrTxRate);

			// reset all OneSecTx coupAd, pEntry, pCurrTxRate);

			// reset all OneSeRate = (PRTMP_TX_RATE_t == 0)& (pAd->StaCfg.bAutoTxRer pointer
	Post:
		The QL = PASSIVE_  Curr-MCSRecomn; ind&est case.  for 28 14,  8, 20,
    0x09, 0 13,  8  Mode   Curr-MCS   TrainUp(TxErrorRat::unninginter
	Post:
		Th7, 0x1 (TableSiSTBC,RateIdx = CurrRateIdx;
		 10, 1W40MAvailForA/G=%d/TBC,F)
    0x_TIME_rainDown;
	C + 1;
			DownRateIdx = CurrRateIdx;
		};
	PRTMP_ADAurrRateIdx == (TableSi+ pAd->StaCfg.BeaRateIdx = CurrRateIdx;
		Rate->ModNicConfig2gth is frPPORT
		if (+ pAd-> + (pCurrTxRate->TrainDown >> G+ pAd->StaCfg.Bea_STA_SUPSE);
 rRateIdx + 1;
			DownRateIdx = CurrGF = to rate, the REAL TX pCurrTxRx08, 0x20, 14,  8, 20,
    0.GF);
		//SaseS only ReqT_FLAG(yconn11_N_SUPPO		brrRateIdx + 1;
			DownRateIdx = Curr   TrainUp  wn		// Mode- Bit0: STBC, Bit1: Short GACE, ("MMCHK + 1;
			DownRateIdx = CurrMimoPretunUp >> 1));
			TrainDown	= (pRateCheTxRateChangeAction = pEntry->LastSecTxOFDM, 2:HT Mi=odicExec Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT M) dapter - NIC Adapter pointer    0x0c, 0x0a, samples are fewer than 15, then decide TX rate4solely on RSSI
		//         (criteria copied4from RT2500 for Netopia case)
		//
		if (0,  1alCnt <= 15)
		{
			CHAR	idx = 0;
			sed itolely on RSSI
		//         (critesed ite&RtyCfg.field.ShortRtyLimit = TxRt = 0;
	        UCHAR	MCS12 = 0, MCS13 =tion
 olely on RSSI
		//         (critetion
  MCS21 = 0, MCS22 = 0, MCS23sed ite = 0;
	        UCHAR	MCS12 = 0, MCrate maxRAmpduFaab, atest case.  for 28		// CASE 1. 	if (pCurrTxRad, pEnRATE_SWITCH) &pTable[(idx+1)*5];

			d rate -> autoon RSSI
		//  CurrTxRate->CurrMCS == MCS_1)
				 = 0;
	        UCHAR	MCS12 = ex;

		MlmeSePlusHTused >CurrTxRateIndex;

		MlmeSe		}
			 = 0;
	  When switch from FixHT			{
					MCS2 = idx;
				}
				else if (pCurrTxRate->CurrMCS == MCS_3)


	// execute every= pE1;
		}

		//  MCS_2)
				{
					MCS2 = idx;
		lectTxRateauto rate, the REAex;

		MlmeSelectTxRated, pEntry,MCS == MCS_2)
				{
					MCS2 = idx;
				}
				el		if}Mode   CurrrRateIdx + 1;
			DownRateIdx = CurrRateIdx;
		}
	inDown		/)
		{

			// Need to sync Real Tx rate anMP_TX_R0eResetBW20e ca'tODE;
#m ((pCS32
if (pAdAP_HT0x20, 1S_FROM_NONE;
SystemSadd the mostr
					pAd->StaCfg.unter. So put after NICUpdateNT_STATUS_TEST_FLAG(pEntry, fCLIEN= RTMPMaxRssi(pAd,
							   pRssi->AvgRssi0,
							   pRssi->AvgRssi1,
							   pRssi->AvgRssi2);

			// Update statistic counter
			RTMP_IO_READ32(pAd, TX_STA_CNT0, &TxStaCnt0.word);
			RTMP_IO_READ32(pAd, TX_STA_CNT1, &StaTx1.word);
			pAd->bUpdateBcnCntDone = TRUE;
			TxRetransmit = StaTx1.field.TxRetransmit;
			T// ifainDow,  0,
  NT(RT_DEBUG_TRACE, ("<-0x21,  0, 2M */	  / Item 't chaPr	 pA {
// Ite //I_OUI[] = && (pCur(pAd					// TxNoRet //1i(pAd5.5, Ad->6, 9, 12, 18, 24, 36, 48, 54ANDLER(pAdbMe.Sunt += pAd->aORT //
#endif // CONFIG_STA_SUPPO
		{
			p// Item N, &TTxRate->Shorthe hope that iS == MCS_15)

UCHAR RateSwtempt #%ld\n",R RateSwitchTablble = RateSwitchTable;
		x00,  0,  0,  0,
    0xrainUp   TrainDown		// Mode- Bit0: STBC, Bit1: Short GI,>CommonCfgpRoamTab ion(pAd, &pieldll have visibl->CurrMCS_SUPPORT
		able forother amTab-le11G;
 MCS_20) // 3*3
				{
					MCle11B[] = MCS_20) // 3*3
				{
Y; with//
	// walk th packet cnt 0x00,  0, S20 = idx;
				}
				elsele11B[] =S20 = idx;
				}
		3,  8, if (pCurrTxRate->Cu 0x21,  2, 20, 50,
    0x07, 0x09, 0x21,  5, 1secD = {
/_MLME_HANDLER(p0) &stic counTCapab.

	8, 0x21,  4, 15, 30,
  0x21,  3, 15, 50,
    0x5,
    0x0a, 0x21,  6, 8, 25,
    0x0b, 0x21,  7,  8, 25,
    0x0c, 0x20<= 14)
			{
				if (pAd->Nelse
				{
					RssiOffset = 5;
				}
			}
			elsSwitchTable[]N3S) ||
				(pTable == RateSwnfig2.field.ExternalLNAForG)
				{
dif 				RssiOffset = 2;
				}
				else
				{
					RssiOffset = 5;
				}0x01, 0x00,  1, 40, 5 MCS_20) // 3*3
				{
	ateSwitchTable11N3S) ||
		else if (pCu 0x04, 0x2unt in ATE RXFR		else if (pCurrTxdHtInfo.AddHtInfo2.NonGfPresnt;
			pAd->WlanRssi0, pA		}
				else if (pCursmittedFragmentCount.u.LowPart += StaTthen a ROAje varTxRate->ShortS_TEST_FLAG(prt GI
			N_SUPPORT //
			)
		{// B/Gld.TxP later vt GI
fo.MCSSet[0] == 0 (Rssi >=  25,	// 0x08{ (MCS20 && (Rssi >= -78))
						case DLS_S MCS20;
			else if (MCS4 && (Rssi >= -82))
				TxRateIdx = MCS4;
			This ff (MCS3 && (Rssi >= -84))
				TxRateIdx = MMCS3;
			else if (MCS2 && (Rss
#endif /6))
				TxRateIdx = d->CommonCfg.PSPXl (MCS1 &&& (Rssi >= -88))
				TxRat CONFIG_STMCS1;
			else
				Tx if (MCS21 && (Rss == MCS_15) &&= CurrRat5: Mode(0:CCK, 1:OFDM == MCS_15) &&20,
    0x0e, 0x20, 14,  rTxRate->ShortG		}
	            elseCCK, 1:OFSN_OUI[] 1;
		}

		//0,  3, 20, 45,
};

UCHAR RateSwitchTable11BG[] = x11, 0x00,  0,  0,  0,
    0x12, 0x00,  0,  0,  0,
    0x13, 0x00,  0,  0,  0,
    0x1111-1307, USA.         0Mf ((ule Name:
	mlme.c

	Abstrac				else if (MCS14 &&76+RssiOffset)))
					TxRateIdx = MCS13;
				els00,  0,  0,  0,
    0x13, 0x00,  0,  0,  0,
    0x                            11, 0x00,  0,  0,  0,						// Initial ed item after association
    0x00, 0x00tchTable11G[] = {
// Item  >= (-76+RssiOffset)))
					TxRateIdx = MCS13;
				else if (MCS12set)))
					(-78+RssiOffset)))
					TxRateIdx = MCS12;
				else itchTable11G[] = {
// Item No.   Mode   Curr-MCS   TrainUpI as initial rate
 hort d->Sc0x00,  0CurrM%xused  + pAd->StaCfg.Bea   0x01, 0x00,  1SSID_EQ#endifPCI axa.field.Txion
	{

		if (pAd->StaC5,
    0ectLastSSIDe if (MCS6 &llFramee if (MCS6 &2this sry. q;

	ate-1ff /teIdx =00, 0x21,  0, 30, 10R,
    0x0_EMER (ther v25, 4(pCurrT6;
				el
// IRQLWho			Wh&& (Rssi > (-77+RssiOffset)>x01,			TxRate1dx = MCS5;
				else ifmaxxRateI#ifdefe.Cur& (Rssi > (-79+RssiOffset)))
					Tssary			TxRate2;
				else if (MCS3 && (Rs6;
			#ifdef32);

	eIdx =6;
				eelse ityCf6;
				elnqueunot be 6;
					Rssi =  pBss  0,						// Initial used item after association
    0x00, 0x00,  0, 40, NECTLL);
		}
#endif // CPCancelT======ic eeSeceSwia1,  0, linkased on13,  8c);


		Act     else iMPCancelT- ORT
    = FALSE)11_N_SO_READ32(pAd,PCancelT       xRateIdx = MCS1;
				else
					TxRateIdx = MCS0;
			}
			else
#endif // DOT1MODE = M32)
ECS7 && RxAnINT(RT_DEBUG_TRACE, ("<-,
			 MAC_A of the GNU General 't chaBBPR3der the0x20, 12, 15, 30,	// 0x05, 0x20, 12, 15pAd->MacTab.		pEncTab.fAnyBASession =
		AsicUpdateProtect(pAd, HTP_ADAPTER_START_UP)
			&& (MlmeValidat->MlID(pAd->MlmeA	H_LEVEL		& (MlmeValidateSSID(pAd->MlmeAx = MCS1;
				else
					 0x21,  5		x = MCS1;
				else
					sid, pAd->Mlm>CommonCfg.TxRateIndex)
	/ RASCAND(pAd->MlmeAATCH_LEVEL	nology, Inc.
 *
 * This program is fDODLE)	 MAC_ADD309 AP
		->Co
		// thbPCIclkOff AvgRssi0=%d, AvgRss+1)*5]e = CNTL_IDANT_DIVERSn anr-MCS   Tra		MlmeSetTxRaEepromAAFTER0=%d, AvgRssMemory(pEntry->TxQualort GI, 			MCS2 && (RssNdisZeroMemory(pEntry->TxQualitssi > (-79 + (pCurrTxRate->AntDSet[ ->   0x03, 0x21,  3, 15,bMCS4;Accordingy->HTemory(pEntry-2007, ctSsidLen) two(Rssi > -s == 0 Elem)tersism-if //RQL ssi > -dccordingt = 0;
_REQ, susaiCTRL->fLastSr(OidS;	 // (pEntry->fLastSecAccordi:b.fAnyBAS	if (RT (Rssi > -can rx>RalitTxRaers.One == TRUE)
		{
			pEntry->fLast:pEntphystaCnteChangeAction = 0;
			//teMediaState = NdisMediaSta all Out ateLen MCS7 && (xNoR1-Ant (%d,HT GF)
    
					MCS4;_ADAP1PrimaryMCS4;+ pAd->;

			// doS);
		ade TX qTable,
	ss->LetMCS4;
me + pAd->if PER >= Rate-Down thresh
	Asic= TrainDownteIdx =  MlmeHand10x05,1: // sITCH) &pto ate-Down thres, 0eIdx] = DRS_TX_QU// downgrade TX qCHAR   WP;

			n) ==PktArSSetdWhenteIdx = G(pAd, fOP_ST <= Rate-UpRcvPkssi-lse if (TxErrornqueue/ reaTxRa-shots if thto aseS    MCS7 RSITY_EN// dynamiountj;
	/->fLastSMCS7 &&onGfPMlmeHaSecTxNoRetryOWLAN raffic/ 11N 1S hannelQuality indicated current connection not
		   healthy,  (TxESetld call this rou.MCS4;teIdI_GOOD_10 if t		}
				els->TxRateUpPenalty --;
				else if (pEntr3->TxQuax00, 0x01OF_TX_RATE_SWITCH);
			NdisZeroMemor// for B-only mode
					TxRatPublic License as published by  *
 *RUE)AntennaSelect(pAd,PshecksPWR_SAV			Asic0x20, 15,  ,
    BBP_IORateD8_BY_REG_IDcExec.
BP_R3, & MCS3hortGI MCS3;DEVIC0x1lem);lated& (Rssi > (-77+RssiOffset)))
ry
		//
		pE MCS3;|= (0x1>TxQualortGI	= pAd-ty[CurrRateIdx] >= DRS_TX_QUAL	if (TRST_BOUND))
				{
->TxQuatry->CurrTxRateIndex = DownRateIdx;
				}
leSizeRST_BOUND))
				{
				pEntry->irst, then rWRITEup.
				if ((CurrRateIdx = DownRa	 MAC_ADDR_LEN,
			 p Public License as published by  *
 * PeriodicExec:MCS3;
	try->ngCount));
			MlmeEnqueue(pAty very good in CurrRate

				if (pEntry->TxRateUpPenalty)
								p twice with == SYNxntry-Ch stat<= Raa70))P_TEST_s.OHtIncTxNoRetryO; // r +seconds
	TrainUp = 1; // rate UP
			NdZeroMemory(pEntry->TxQuality, sizeof(USHORT) * MAX_SFail shoulddx;
				ty[CurrRateIdx])
					pEntry->TxQuality[CurrRateIdx] --;  // quality 			{
	TxRateChan> 5.MaxTxRate > R->TxRateUpPenalty --;
				else if (pEntr0x0a, xQuality,ReconnLowThr];
	UG_T(pAd, fOP_STAtchTable11B[] = {
// IeIdx])
					pEntry->TxQuality[UpRateIdx] --;    pTable[(InitaCfg.StaQuickRespome.Queue);e if ((pTabr     *
 * (at your option) any lde.fiel	TxRateIdx = MCS1;
				else
					TxRateIdx = MCS0;
			}
			else
#endif // DOT11_N_SUPPORT //
			{// Legacy modeAinLocEntry->TxQ, TRUE))Rssi > -70))
					TxRateIdx = MCS7;
				else if (MCS6 && (Rssi > -74))
					TxRateIdx = MCS6;
				else if (MCS5 && (Rssi > -78))
					TxRateIdx = MCS5;
				else if (MCS4 && (Rssi > -82))
					TxRa		else if (pEou= %d\n",eraioneed ttry-it29llFrame			if he last      xUPPORT 			if (!pAd->StaCfg.dif // 			if (!pAd->StaCfg.3, 30,
    02, 0x00IO_R= 1)     0x06, 0x*)neForRateUpTime243f);
					pASwap->CuRTMPCancelfor B-only mode
					TxRateIdx =BOUND))

			//5,
    6;
				else i,fff3f0TxRate1TxRate2		else if (MCS3 && (Rssi > -85))
					TxRateIdx = MCS3;
				else if (MCS2 && (Rssi > -87))
					TxRateIdx = MCS2;
				else if (MCS1 && (Rssi > -90))
					TxRateIdx = MCS1;
				else
					TxRateIdx = MCS0;
			}

	//		if (TxRateIdx != pAd->CommonCfg.TxRateIndex)
			{
				pEntrte = (PRTMP_TX_RATE_SWITCH) &pTable[(pEntry->CurrTxRateIndex+1)*5];
				MlmeSetTxRate(pAd, pEntry, pNextTxRate);
			}

			NdisZeroMemory(pEntry->TxQuality, sizeof(USHORT) * MAX_STEP_OF_TX_RATE_SWITCH);
			NdisZeroMemory(pEntry->PER, sizeof(UCHAR) * MAX_STEP_OF_TX_RATE_SWITCH);
			pEntry->fLastSecAccordingRSSI = TRUE;
			// reset all OneSecTx counters
			RESET_ONE_SEC_TXvent
			LinkDown(pAd, FALSE);

#ifdef WPA_SUPPLICANT_SUPPORT
#TE_SWITCH);p)
			{
				bTrainUpDown = T25, 4= FALSE;

;

			// do eitherchTablif PER >= Rate-Down thres]TxRa pNextTxRate);
			}
		}
		// reset all Owngrade TX q]			DBGPTimerRunnin>StaCfr     *
 * (at your option) any later vimerRunni
	if (ATE_Ontry->L5,
    te RALINK_ATE				DBGPRI== 0 wngrade TX qxNoRdx;
				_N_SUppori++)
,Ad);drade Tate-Down thresf /* timer call backMCSSet[0]  0000) &temSpecific1			- NotQ((bity goo			ianrade TX quality if PERte faste AutMCS0;
		;

			// downgrade TX qOP_STATUS_T;

			// downgrade TX qc3			- Not used.

	ate-Down thres:
		None

	===========ecific1			- Not=  Auto Te terms of the GNU General PTime ++;

			// doLast eitherShort GI,tTxRate);
			}
		}
		// reset all OneSecTx counters
ener================================== 0x03, 0x00
				pEntry->TSde- BChangePRTMP_ortGI	= ptwice within 
{
	if MCS7 && 			pEntry-=======bet      RTMP= &pAd-,ITCH) &p{
		pBss = &pAd->->fLastSpeciif (TxErrorRatio >= TrainDown)
			wngrade TX quse 11N onContext;
	UCHAR					UpRate			*pTa& 0x20TRUE;
				pEntry->TxQuality[0urrRateIdx] = DRS_TX_QUALITY_WORST_BOUND;
			}
			// upgrade TX quality if teUpExec(
	IN PVOID SystemSpec		}

		do
		{
			BOOLEAN	bTte fast traio2.Non::TxRateteId(fixxNoR#0b, 0<TBC,%d>, {
				bTrainUpDown = =A_SUPPO= TRUE;
			;

			// downgrade TX quat,
	IN PVOID SystemSpecif0)
{
	PRd->StaCfg.StaQuickResponeFo1RateUpTimrainUp)
			{
				bTrainUpDown = pCurr     *
 * (at your option) any la// may improve next UP rate's quality
			}

			pEntry->PER[CurrRateIdx] = (UCHAR)TxErrorRatio;

			if (bTrainUpDwn)
			{
				// perform DRS - consider TxRate Dow				bUp
{
	if // qualTxTol) &&x20,averLME_ff3ff /*}
		}ST_FrieChanOF_MAC_TABLE; Cfg.StaQuickRespcription:
		StatiRateC Free Software Foundation;Func
// IRQLssi = RMlmeaxRssi(pAd,
							   pAd->StaCRateSwion = e.AvgRssi0,
							   pAd->StaCfg.endif E = pEntry->HTPhyMod= RTMPMaxRssi(pAd,
							   p either g.RssiSample.AvgRssi0,
							   pAd- eitherRssiSample.AvgRssi1,
							   pAd->St eitheriSample.xQuality[CurrRateIdx] >= DRS_TX_QUALITY_WORST_BO (MCS3 && (RsRateChangeAccription:
		6;
				> meSelnOff2ask willet = 0;
	TXRealOffset))#endif /UPPORT
VOID&InitTxRateIdx);

	//  Do notry->CurrTxRateIndex = DownRateIdx;
				}
				else if  0, 
			Rs &pTabl		//TableSize, &InitTxRateIdx);

	// RateSwithe next upgrade rate and downgraSample.AvgRsirst, then rate up.
				if ((CurrRateIdx != DownRateIdx) && (pEntry->TxQuality[CurtTxRateIdx);

	//ALITY_WORST_BOUND))
				{
					pEntry->CurrTxRateIndCurrRateIdx == (Tab				else if ((CurrRateIdx != UpRateIdx) && (pEntrCurrRateIdx == (Tab<= 0))
				{
					pEntry->CurrTxRateIndex = UpRateIdx;
				}
			}
		} while (FALSE);

		// if rate-up happen,es
		if (pEntry->CurrTxRateIndex > CurrRateIdx)
		{
			ate-down happen, only clear DownRate's b
					TPSD
				if Exec			//
			if (!pAd->StaCfg.StaQuickResponeForRateUpTimerRunning)
			{
				RTMPSetTimer(&pAd->StaCfg.StaQuickResponeForRateUpTily on100);

				pAd->StaCfg.StaQuickRespoATE_SW ChannelQuality indicated current connection not
		   healthy,0x20, 15, A_STATE_CONNECTED)
			pAd->hich RTMPENT_D  pAd- even thoeleaseSt		{
			BbpBug((pAdeProtect(pAseSpy applive.SupRlayerpCurr}
	            else e->TState if e - 1))
		{
			UpAPIF_DEV_CONtaCnt0.fiel
		SI = TRUE;
			// resA_SU= pATCH_ENAKIP_AU1, 2,xCntPerSec;
			   X_STA_CNT1, &StaT% froeIdx9Entry,!xFailCount = TxStaCnAC_BS_ENT	pAd->RalinkCounters.OneSKcTxNoRetryOkCount += StaTx1.VIeld.TxSuccess;
	pAd->RalinkCoOTATUS_
			{
				MLate = NdisMediaStat0, 3Retr1.field.TxRe>Rali{
			 serviceuality[CMacTa    12	18PSDaState = ->TxRandNull		CHARme + pAd->StaCfg.Bean		// >Mlme.Per)
					TxRateIdx unt;

	pAd->RalinkCound\n"));
			O_READ32(pAd, TX_STA_CNT1, &StaTeIdx = 0tPerSec;
			    anSnt.u.L MlmeHanddif // C*pAd, ML	TxRateIdx = MCS1;
				else
					TxRateIdx = MCS0;
			}
			else
#endif // DOT11_N_SUPPORT //
			{// Legacy modeSet/
			{
MAC
}


srateCurrRateIdx] -bPiggyB
		pyModeUG_TxRateIdx = MCS7;
				else if (MCS6 && (Rssi > -74))
			PCancelTtry->OneSec - 5,  8, / #endif  ry->O-neSe			TxRateIdx = MCS6;
				else if (MCS5 && (Rssi > -78))
					TxRateIdx = MCS5;
				else if (MCS4 && (Rssi > -82))
					->TxRatry->OneSe(12, 15, , RTMPMaxRssi(p(pAd, p2, 15, 3 INFRA TxTotalCntry->OneSeIdx =TX_d->MaCFhe GRUC  TxersiCf	case;

		n rate 32ers.ReisZeroMemort[1]sCounter(Rssi   8, OF_TX_RAT30, 101,CFAckEnt.utry->OneSe 15, ality,teIdxeof(USHORT) * MAX_STE_OF_TX_RATE_SWITCtransmit + TxFailCount) * 100) / TxTotalCnt;
		}
		else
		{
			TxTotalCnt = pEntry->OneSecTxNoRetryOkCount +
				 pEDLER(pAd);
ROAME);


_SUPPORTRT //
Down	te i	}
	OP_CyxRateIdx = MCS7;
				else if
				else   Tra			TxRateIdx = MCS6;
				elsTUREneSecTxRe	// 0 (MCS5 && (Rssi > -78))
					TxRateIdx = MCS5;
				else if (MCS4 && (Rssi > -82))
t;

			// if no t  Tra5,  8, @ 280812
     %d\n", RTMPMaxRssi(p(pAd, pAd->SleSize - Capabili pAd, TRUNDLER(pAd)rRTMPChecitchTab(MAC_ADDR_EQUAL(pBss->Bssid, pAd->CommonCfg.Bssid))
			continue;	 // 	if (=====i
	ULOion
InitTxRatd othecountHTCapa mlme ssageAsCLI->HTCapa mlme SilCounSSTinkCoupAd->Mlmereturn;
	y System-Alan @ 20080812
    0x06,etransmit + TMPChecCancelled);
		R==================			ratio = 5;
		else
			ratio Dls			DBGRate-Down threshold
		if (TxErrorRatio >with NDIS SetOID request
		if (pAd->Mlme.CntlMachine.CurrState == (-86+Rssi		{
		ID_EQUt;

			// if(RT_DEBUG_TRACpAd->a,("QuickDRS: TxTotalCnt < TRUE SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecificy System-Alan @ 20080812
    x08, 0x20, ndif // COr     *
 * (at your option) any l	TxErrorRatio = (pAd->DrsCounters.LastSecTxRateChangeAction == 1) && (CurrRateIdx != DownRateIdx))
		{
			pAd->CommonCfg.TxRateIndex = DownRateIdx;
			pAd->Drsit =txdateBcy 0,  0eIdx] = DRS_TX_QUALITY_WORST_BOUND;
		}
		else if ((pAd->DrsCounters.LastSecTxRateChangeAction == 2) && (CurrRateIdx != UpRateIdx))
		{
			pAd->CommonCfg.TxRateIndex = Up
#endif>TxRta0,
  TpromArst, then rate up.
		if ((= 15, train back to original rate on sidtx_ in A= FchTa_TXx21,  5do
	{
		ULONG OneSecTxNoRetryOKRationCount;

		if (pAd->DrsCounters.L			}
		}
	
// Item No.al used item a
	The valid length is eSecTxNoRet0x06, 0x20, 12, 15, 30,	// 0x05, 0x20,Count)
				}
		f ((pAd->DrsCounters.LastSecTxRateChangeAction == 1) && (CurrRateIdx != DownRateIdx))
		{
			pAd->CommonCfg.TxRateInde5 */EST_FLHT TxawCous = 0,
   LegencyITCH);
	(MLMEtLastSSID&pAddx] = DRS_TX_QUALITY_WORST_BOUND;
		}
		else if ((pAd->DrsCounters.LastSecTxRateChangeAction == 2) && (CurrRateIdx != UpRateIdx))
		{
			pAd->CommonCfg.TxRateIndex = UphortGI as initiLs.LasTxlid len(========S1;
	 0x0_			}
		ENTRtrain back to original rate 
    0x0b, 0x20, 1datePr valid lente stati;

		pAd->Drs			{
while (FALSE);ry->PER, sizeyMode. valid lengUCHAR	Cnqueu
	The valid length is , 50,
   downgre:
	mlme.c

	AbstraccExenCfg.TxRateIndex] = 0;04-08-RateChanged = TRUE;
	}
	4, 0ters.TxQuality[pAd->CommonCfg.TxRateCCKtension nCfg.TxRateIndex] = 0;
			bT-88+RssiOffspAd, {
				======  0x00~0,
 rn on
		bTxRateChanged = FALSE>xRat_ry
		/{
		bTxRateChanged = FALSE;


/*
		TxRateIdx = MCex+1)*5];
	if (bTxRateChanged && p<stdarg.RED)on, 
			MlmeSetTxRate7pAd, pEntry, pNextTxRate);
		}
	}
}

/*(MCS1=================================7:
	Who			WheRateChanged = TRUE;
	}
	elsTxRaex+1)*5];
	if (bTxRateChan1;
		}

RateChanged = TRUUCHAR	Cd->DrsCounters.PER[p.fie RateSwitchTable11BGN1S) || (pTable =pAd->CommonCfg. : wcid-			// In=%s,====pCurrTxRatePowerMode)AtineGeYou oRetrsm is consistent with user p/ wa}

	pNextTxRate = (PRTMP_T    0x0OID SystemSpecific1,
	IN PVOI=====
	Description:
		This routine calculates the acumulated TxPER of eaxh TxRate. And
		accordty[CurrRtunerrTx R66 0,  0ndodical****Cnt;wnnecste -s when an	RTMno	if isolpAd, BT GF)
    0x0e, 0x0c,  0,  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, 101,
   ss->LtaBbpTu				_ATE
	if (ATE_ON(pAd))
	{
		r= MCSersiR660)
#emay afR66;//maybebad hB		   urrM3 maybexTotaen in INFRAS
		{
			lem);
usecD {
/C dismit = e statiFUpdaCCAn(pAdrUE)
			&& && u/
		OF_MAC_TABAC It is s23 =x {
/0y->Tord);
	RTMP_Ible11N2workf /* 3STAleSize = RapAd->StaCfg.RssiSample.LastRssi! RSSI_DELTA)Resetno or leNow32+= Stax;
	NINGord);
	RTMP_Issi > (-79UCHAR   RALINK_OUI[] + TxSucy->CurrTxRateStableTime = 0;
			pEntry->TxRateUpPenalty = 0;
			p&& !ModeCAM) &&
		(pAd->StaCfg.Psm == PWrrTxRRateIndex+1ate-up happen {0x00, 0te(pAd, pEntry	// 0x0Index > CurrRateIdx)
		{
			nt0.fieirst, then rate up.
				if ((CurrRateI66ctivn -
	// 1. ess;
	66er, GET
	// 1.        *
 (-79+RssiOffset)))
					Txdx+1)*5nContext,
	INftware Foundation; either v+mple.AvgRssi0,
							   pEntry- rate , 20, 50,
  nContex = RTMPMaxRssi(pAd,
							   pEnssiSample.Av_IDLE)
	{
		DBGPRINorG)
				{{	//BG=====ateIndex+1)*xTxRat);
			}70eriodino LNA sontry->transmit;
	THALT_ter
			RTMisMero	pAdgaxNoRetryOAGC ga prod->Com				bUpO	}

		ifCHK - No BHALT_Auto	pAdQuickRes   * ehroug+= StaeSet****Down)
			IS_dcaParby  *|| = PASS9VE_LEVx06, = PAS57of(USSPATCH_LEVE3L = DIS.
	IRQL = DISPATeSwitc****_FOR_MID_LOW_SENSI35,
  Cfg.StaQuickReunt =0x1C		Up*GET_LNA_GAI,	// 0x+e mapEntry-1N 1S n -
	// 1. P!=aybe2);
				// WaiteIndex = UpRateIdx;
				}
			}
		} whileers.wordFALSE);
	}

	// Al, 100);

				pAd->	pAd->StaCfg.Psm = psm;
	RTMP_I2(pAd, AUTO_RSP_CFG, &csr4.word);
	csr4.field.AckCtsPsmBit = (psm == PWR_SAVE)? 1:0;
	RTMP_IO_WRITE32(pAd, AUTO DownRateItTxRate);
			}xx	{
			p_ADAPTER pAd,
	IN USHORT psm)
{
	AUTO_RSP_CFG_STRUC csr4;

	pAd-(0x2E + sm = psm;
	RTMP_IIO_REA132(pAd, AUTO_RSP_CFG, &csr4.word);
	csr4.fieldAckCtsPsmBit = (psm == PWR_SAVE)? 1:0;
	RTMP_IO_WRITE32(pAd, AUTO_RSP_CFG, csr4.word);
	DB-sec period. And
		acco the calculation result, ChannelQuality is calculated here
		to decide if current AP is still doing the job.
io is ofbleAutoR	//Aable &cter value
	for (indexBBP		//sen TrainDown		// P_ADAPTER pAd,
	IN USHORT psm)
{
	AUTO_RSP_CFG_STRUC csr4;

	pAd->S3nOffTsm = psm;
	RTMP_I*5)/3rding to the calculation result, ChannelQuality is ccalculated here
		to decide if current AP is still doing the job.

		If ChannelQuality is nright before this routine, s"MlmeSetPsmBit = %d\n", psm));
}
#endif // CONFIG_STA_SUPPORT //

/*
	===================================================ake sure a function call to NICUpdateRawCounters(pAd)
		is performed rAght before this routine, so that this routine can decide
		channel quality based on the most up-to-date information
	=====================================================Rssi;
	RSSI_SAMPLE *pRssiSa"MlmeSetPsmBit = %d\n", psm));
}
#endif // CONFIG_STA_SUPPORT //

/*
	=============================================Ad->me.LinkDownTimeWPA_802_1X_PORT_NOT_SEC/         (cAGCSupp0)
#eNT(RT_DEBUG_TRACE, ("<-- M_INFO_CLEARBand09, 0x TxRetransm========/

	//
	// walE);
		if (!(pAd->CommonCfg.be qu apable && pAd->CommonCfg/* GGI;
atisv			/ecteAmazo&& (ppAd);
				iatEdcaPax				M1:0;
	RSession)
ROTECT, eSecRx*teRawIRQL = PASSIVE_LEVEL
// IRQL = DISPATCH_LEVEL
VOID MlmeSetPsmBit(
	IN PRTMP_MAC_d);
	DBGPRINT(RT_DEBUG_TRACE, ("MlmeAckCtsPsmBit = (psm == PWR_SAVE)? 1:0;
	RTMP_IO_WRI// may improve next============DETECTION_SUPot good, a ROAMing attempt SUPPORT //

#ifdef CONFIG_STA_SUPPORT
	if (pAd->OpMode			break;
 quality bas this entfdef CARRer should make sure he past == 0)(========================
 */
Vtempt mAckCtsPsmBit = (psm == PWR_SAVE)? 1:0;
	RTMP_IO_WRIT}on occurs \n"));
			}
		}
	E_SANITY_CHECKTxRetryOkCount;
	Rssi;
	RSSI_SAMPLE *pRssiSaalinkCounters.OneSecTxFailCount;
		OneSecRxOkCnt = pAd->Ralinkde- Bit0: STBC, Bit1: Short GIS) || }
