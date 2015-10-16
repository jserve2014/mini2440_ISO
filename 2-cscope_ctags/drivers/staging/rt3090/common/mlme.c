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
 *******2********mid****
 * h Inc.
 3h In5h Inc.
***********11h Inc.
longech Inc.
 ****if (OPSTATUS_TEST_FLAG(pAd, fOP_302,
 * X_RATE_SWITCH_ENABLED)/* &&chu Cy  * (c) CTaiwan, R.O.C.
 **** (c) MEDIAram iE_CONNECTED)*/)
	ount*auto_rate_cur_p)
	{
		h Inc.
bm = 0;
#ifdef CONFIGram _SUPPORT
		IF_DEVe; y theOPMODE_ONram This)
			 underpAd->StaCfg.RssiSample.AvgFoun0 -ree SoBbptherToDbmDelta;
#endif //s ofnse  GNU Genera //
	stribbLinkUp == TRUEgram sion Commonare TxRate = yrigh24;
		else                           *               Max      ;
 later the <F.,                            ogram  11uted in the hopeThis 0rogram is distributed in the hope TY; 
		// should never exceed           (consider 11B-only molink ater                       >program is distributed in the                                         details.                          Index *
 th	}
 detaily    *ur option) l Public License for more details.   		pHtPhy->field.MCS	=r FITNESS FOR A Prranty (at> 3) ?eceived a copGNU G your opt- 4) :gram is distributed in the hopeYout even thaODE r Public License     *
 * al        p    FDM :oundatCCK            cTab.Content[BSSID_WCID].HTPhyMode.      STBC	=te to the      e - WARRANTY; w is distributed in the hope59 Tetion Placehu CGISuite 330, Boston program 02111-1307, USA   *HOUT ANY WARRANTY; w***********ave                  MCS07, USA.             *
 *  Module Name:
	mlme.c

	Abs******e to   *
on History:
	WiY or FITNESS FOR A PARTICUL<e hope thdif    p    Revision History:=ncs program------
	John Chang	2CS                   in the hopMin	John Chang	2004-08-25		Mo-----froMin in the ho00
*/

#inclu-----
	John Chang	20#includeion,#includ
base
	John Chang	20Ofdm00
*ToRxwiMCS[07, USA.             ]#inc		When	0, 0at
	ude <ed     Rechno *  6 && 0x50, 0xf2 0x001};
UCHAR	R#include54)gram {ode base
UCHAR	CISCO_OU {0x0, 0x04, 0x096};
UCHAR	WWPA_O};
UCHAR	];}00
*/

#inc 0x014 0x072;
UCHAR	   Wng	29-06		m06		};
UCHAR	T}
	Who	e to theword     
#include CHAR);o			Whversion.ME_INFO_EOp       *asundatSTAME_I                        *
 ********************CHAR	WC0xac};
UCHAR#inc                    *
 **********fo[] I[]  =ME_INFO_EL2QosInfo[]#ifdM_ELEM[] PS_O04};
ifdef CONFI0, 0x0f, inx04}the terms of thx14, 0x72eral#if00
*/

#incluswitch 0x50, 0xf2, 0x01       LITYE_INFcase PHY_11BG_MIXED:any RM_ELEM    S:he termsDOT11_            ] = {
// ItemwNT //Tablrn the hopdrom Curr-*****any la07, USA.         Mlmein   *
-----	M_ELEMBit4,5: Temp(0:CCK,Transmitc

	Abstrac-----
	John Cha, 3:HT GF)600
*0x11 0xac0,  ion
    0ng	20, 2d itM
//o.   M	WIFInc.
 al used item af  *
aRts 1:OFDM, 2:HThat//#00
*///ion
1, EM[]50,after a0, 0xacion
2, 5, 45 prog00,  breakx00,  = {
// IteGbleSUPPOR
// IteA No   *TempTBC, Bit1: Sh TrainUp0x07, 0ADownthoutM05ONFIG103, 0x2  3, 20, 45,808, 0x21,4N_2_420,  3, 20, 45,603, 5, 33, 20, 45,908, 0x215G 2, 2- 3:H0: e - , 3:H1: S*****GI5,
  T GF)
    0x11,  1:OFDM,  0,6x00,  1, 40, 50,
    45,
    0x04, 03,  0,  3, 20, 45,e, 0ssociation
    0x00,,	2, 15 0x0<stdarg.  1, 40, 50,
    ssociation 20, 45,, 0x000ME_INFO_ELEM};
#ifdef CONFI0, 0x0f  0x0d,ME_INFO 20, 45,x08, 0x21,  4ASRT //// M0x21,2 0x0f  3, 20, 45,708, 0x21,3, 15n, 50,
    0x21,  7,  8, 25,
    0x0c, 0x20,0, 0x50, 0xf2, 0x01JohnnelAPI_14gram RTany 0, 12,  15, 30,
    0x0d, 0x20, 1T Mix,
UCHA0x04,3  0,  45, 0x00,     WM21 0x2after asaociation
  25,
    0x10, 0x22, 15,  8,nitx00,  0x11, 0x00,  0,  0,  0,
    0x12, 0x00,  on
   4
UCHA

UCHAble11B[] UCHAR  0,  0,
    0x1d, 0x = {
// Item,
    00x0f, 0x20, 15, x0,  R      0x0f, 0x200x0f Mode- Bi 0, 0,  
    0x1c1CONFIG2Bit4,5: Mode(0:CCK, association
    0x00, Item No.
    0x04,Curr-MCS   TraK, 13	// Initial used item after4,15, 0x 0x00,  0,default:****errorode- Bi2a, 0x21,  6,  8, 2d Mode- Bi,
    0x0f, 0x20, 15, : Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
   0x04, 0x03,  0,  0,  0,						// Initial used item after association
    0x00, 0x00,  4, 0x03,  0,  0,  0,				c	// Initial  0x00,  0

UCrt GI// Keep Basic itiable11. after ass                 *MCASTOADCOM1_N_SUPPPORT
UCHAR	x00,  0,  0,  0,
  {0x00, 0ndif //dCHAR	PRE_N_HT_0xac};// Initial used item aftt4,5: Mode(able11B[] = 0x11, 0x00,  0,  0,  0,
    0x12, 0				// Initial used item afterRRANTY; E_IN00
*/

#includ1c, 0x001:OF0,  0,  3
    0x1c,x08, 16, 2,  06, 0x    0x1bHAR Rat, 101,
  0, ,   0x0d, 0x 12,  15, 30,
    0x0d,: 0x01,DBGPRINT(RT_DEBUG_TRACE, ("tial Up0, 50_SUPPO (MaxDesire=%d,arraSupport8, 25,
 0,
   8, 25,inT GF)
   ble11S 0,  ing =%d)\n",5, 0 0x20IdToModu[21,  7,  ], 0x20// Initial u   0x0c   0x1e, 0x00,  ublic License     *
 * a00,  0,  0,  0,
    0x12, 0x00,			// I2Mix, 3:H/nology, Inc.
 *
 * This program is fCopOUI[]t 2002-206, 0Ra*/ute it and/     d00,   2, 20, 50,
    0x07, 0x21,  3, 1  0,
    0x1712,  15, 3045,
   8, 25itial u  3:Hmap=0x%04lxtem after a  0x09
UCHAR{0x00, 0x50, 0xN_SUPPO0,  0,  0,  0,
    0x12, 0x00, 5,
   ],
   x01, e terms0,  6, 10, 25,
    de(0:CCK, x0a, 10,  6, 10, 25,
 e// Initial de  x   0xI[]	= {0x=xusedx10, 	= {0x0x1b, 30aitial used item after ,  7, 10, 13,
} 0x10,sociation
1,, 50,
  CuU GeneralCHAR	WPRE_N_HT = {
/	ifdef CONFI};

OT11_N_SUPPORT
UCHAR	PRE_N_HT_0x0a,  0,  erms o
    0x04, 0x, 20,
    0x1c, assoI[]	= {0x00, 0x0,  }
, 0x03,  0,  0,  0,				6/*
	=   0x16, 0x00,  04Bit4,5,  6,  8, 2 0x09, 0x2C, B0t1: Short GI,5,
    0x 
	Descrip,
  // Ieful,funciatiou  6,  HT 0x20,setting2, 1Input Wcid value is0x00i
UCHAR2   0x0 {
//. it's 0x00x15, Sta 7,  iTBC,fraNTABI, 35,icenseAPShord----Mactabl2, 152. OEM[]K, 1:	in adhoc 0x07, o1,  3,peer'siati 0,
    0x17, 0x00
	IRQL = DISPA 0x0LEVEL

    0x16, 0x00,  0, 15, 30,
    0x08, 0x21,  5, 10, 25,
    0x09, 0x21,  66, */
VOID 0x10,  6, Ht0,
    (
	IN PRTMP_ADAPTER16, 2  0,
	CHAR	m afapidx)
{
, 20, 3StbcMcs; //j,ial 0x08, b 35,sx10,  
   Cur  3*3
	RT,  1CAPABIPPOR	*pRtHtCap = NULL;0,  0,  // I 0, 		*pActive to t0,  0,  0,ULONG		0x00, Modul,
    j21,  4,4, 0xP  0x08, 0x10,7, 	p  7,   15, 30,
    0xPHTTRANSMIT_SETTI21, e to t    0x03, 0x00,  3,b Mode- Bit0:#include4, 0x00,  OFDM,, Bit4,5: Mode(0:Cx14, 0x0,  0,  0,BOOLEAN  0, 4, 10, 25,
    0 wit  2, 20, 50,
    0x07, 0 3:HT GF)
  x03, 0x00===> \n"35,
 	, 10, 25,
    00,  0,  0No.   Mo (at your option) l               LINK_OUished b    
    0xx				, 15,
	= &sion ftw      7,  d to tA_SU, 10, // Initial e, 0x00,  0, x11, 0x00,  0,  0,  0,
RateSwi 0x21,  0, 30, 0a,  0,  arg.h>

UCHAR	 0x21,  0, 30, 100,  0 0x00,  c 0x14, 0x
    0x00,  0,  0 item afterMCS	h  15, 30,
    0x0dx21,  0, 30, bAuto0,
      0x10x03Dode- Bit0: (at your o,
   )hort 2:HT Mix, 3:HT GF)
    0x0b,trib(ADHOC_ON 0x00 || INFRA20,,  80)621,  0, 2M_ELEM[]RALINKon
    0x00, 00, 0x50, Sta    0x.   0x0ce *
   0,.bHtEn21,  0, FAL*******returd for RI, Bi 30,
 0x00,  0
    0x0f, 0x20, to t  0,
    0x12, 0 0x10,  0,   2:HT Mix, 3:H	/ 0,  0,  0,4, 0x21R	Cc,
   )itial lmeAux.AddHt0,   BC, Bit1:3.   0x08,
, 15,
},  6=m0x21,  2, 20, 50,
    MCSSet[0]+0x50,   8, 25,
    0x0c, 0x20, 1]<<8)+(4, 0x21<<16ateS5,
    0after associat40, 50,
   .Tx, MA 0x0f, , 25,
 ->R1, 40, 50,
 ion A, 11na 10, 25TxPath5,
  ME_INFde "../rt_config., MA = , MA_USy:
	
    0x1c, 0x09, 0x 3, 20, 45,
    21,NON2, 2Wh
    0 0x20, 12, 15, 30,	// 0x05, 0x20l/0x16, 0x0};

UCH0, ->R RateSwitchTab 30,
 0x09, Bit4,5: Mode(0:CCK, };

 0,
    0x1e, 0x00,  0ForABand50,
    0x06, Bit0: Sx0a, 0x22CuT, 25,
    0x06, 0x10,  6, 10, 25,
 e-0x00,  0,  0,  0 2:HT Mix, 0x20, 12,  15, , 0x20, 15,0x03 0x2 8, 25,
x0c,  item after a 15,  9, 0,  0,  0,
    0x1e, 0x00,  0, x11, 0x00,  0,  0,  0,
    0x12, 0x00,0x21,4, 0r association
    0x0x21,  4, 0x21,  3, 15, 50,
    0,  0,  0,
    0x1615, 30,
    0x50x21,  3, 15,   WMAbstrDecide MAX ht, 0x2.45,
    0,  0,  0GF 0x0f, 0x20 0,
    0x1e, 0x00,  0,GF0, 25,  1, 20, 50,
   h"
#include HTGREENFIELD; 0x01};
UCInitial used item ax08, 0x21,5MIX;
iatill be,  0,  0,
    0x1e, 0x00,  0, {
//9,Width0x00,  0,  0,  0    0x09, 0x2		// Initial used iteBW = BW_47, 0x10,25,
  50,
    0x02,  {
// It2020,
    0x0teSwN1S0, 15,  8, 25, Bit0: 		// Initial used ite, Bit1:= {
  0, 0, 30, 101,
    0x01, 0, Bit1:for20 &  0,  0,  0,  0,
    0x  8, hTable11BGN1S[] = {
// ,
    0x0c, 0x20, 12,  15, 30,
    0x0d, 0x20, 1T4,  0,	own		// Mode- Bc, 408, m afWhx10,ble11BGN1S 0x20, 4] != 5,  "../rt_config.h"
#inclng	2032GI, Bit15, (i=23; i>=0; i--)
    0x07    j = i/  0x0Initial= {
1<<(i-(j*8)0, 12, STBC, B// Initial RateSwitj] & Initial 0x0f, 3, 20, 45,
    0x04,30,
    0x08,eneraany 0, 0x21,  0, 3, 254, 0xirainD40, 101,
    0x08,i=    0 ix, 3:HT x21,  4Cit4,MI// I5,
     rt2860???
4, 0x21,  base
	Jo 25,
    0xMem No.   Mode   Cuhn Cha  0x06, 0x10,  6, 10,e11BGN221,  7,  8, 25,
    0x
    0x0c7, 0//If I[] assigns fixed-MCS2S[8 Bit0
to0x20, 1here.0,
   0x21,  6,  8, 2, 0x005,
  ,
    0x08, 0x20, 14,  8, 20 0x21,  5, 10, 25,
    0x091,  1,xff 13,  8after 3, 20, 45,
    0x04, 0x  1, 20  0,     0x08, 0GI, Bitde  8,0, 35,inUp   TrainDown		// Mod  7,  87, 0x21	10,  6, 10  0x17, 0x00,  0,  0,  0,
 <=== Use F20, 17,  8,%dem a 0x14, 0x72};
UCHAR 
    0x043, 0x00, , 0x(0x06,iModu   0x08, 0x10,  , 14,
ssociation
 0x08, 0x10,25,
    030, 101,ateSw association
    0x00, 0x00,  0, 40, 20,
   0,						// Initial used item afE_INFO_25,	//25,
};

UCH, 0x21,  1, 20, inUp   TrainDown		// Modit4,5: 5, 30,
   15, 0urr-MCS   Tra 40, 101,
   ble11BGN1S[] 0x21,     0x07x08, 0x03, 0xial usee11BGN2sociat};
U
    0x0c, 0x00, 0x21,  0, 30, 1010,  1,  2, 20, 50,
   r-MCS   TrainUp   Tx, 3:HT GF)
    0x22, 15  4, 15, 30,
    0x050,						// Initialng	200   0x0a, 0x21,  7, x, 3:HT GF)
    0x00d, 0x20 Short GI, Bit4,5: Mode( 0,	 0x06,on
    0 now.0,
    0x00,  3, 20, 45,
    0x04,00,  3, 20,
25,
   9-06e	, Bit 
    0x				// Initi, 20,
    0x08TBC,fter S06, 0x0x06, 0x21,  3, 15,06, 0x10,  6, , 0x00,  0---.AMsduSizthe %d  0x2
UCHAR  0,
    0x1e, 0x00,  0,Am07, 0x21e- Bit0: STBC, Bit1: Short GI("TX:le11Bx08= %x (choose %d), _ELEM10, 130d, 0x20, 25,22, 15, 25,  6, 10 0,						// Initial0],e to the       ve600
*/

 GF)
    0x0 program is distributed						// Initial, 0x0, 12,  15, 30,
    0x0d, 0x20,, 15, 30,
   			// Initi0x20, 1}

ociatiBAT21, 0x00,  3, 20, 45,
    0, 500,
   IN BA_Tfter *Tabe   Cint21, 
	Tab->numAsOriginator0,
    0x0f, 0x20RecipientBit0: STBC, BitDone07, 0x20, 13t0: SNdisAlloc0812pinLock(0x0a, x00,  RrABanR   WM2x20,  i <0, 3_LEN_OF_BA_REC0, 35,; i++,
    0xx0f,BARecEntry[i].21, BA_ BituPPORT
U5,
   0x10,  5,
};

UC{0x00, 0x/ Item(    0x06, 0x10,  6, xReRingr-MCSN3SF}Sx09, 0x20, 15,S   TrainUp   TrNORI  2, 20, 50,
    0x07, 0xOriit4,5: Mo00,   0x17, 0x0007, 0x20, 1x10,  5, ble11BGN1S[] 8, 25,
    0x0c, 0inUp	// Mode- B/ Mod 0x01,  5, 10, RadioOff0x21,  6,  8, 2ble11BGNe   Cur 0xMLME_RADIO_OFF 0x000, 35ssociation
    0x00, 0x21,  0, 30, 101,
: STBC, B, 15, 30,
20, 12,  15, 30,
    0x07,  3:HT t0: STBC, Bi 0,						// Initial used item after association
    0x00, 0x00,  0, 40x00, 0x00,  0, 40,// bss_x21,  c, 20,      0x0f, 0x20,   0,     2,
    0x
    0x1c,15,  83, 0 since RT61	= {     *
 T Mix40, 101

/*! \brief ix00,  BGN2BSS x21, 
 *	\param p/


 po, 13r1,  theal data TX,	//50
 nonr may rre to reost13, ,  0, PASSIVE 0x03, ociation
    0x00, 0x21 associatiBss21,  5, 10, 30,BSSd item after associaAR	 OfdmRaBssN2, 15, AR	  2******Overlap*/ 0x0ffc, 0x09,  0,  0,  0,						// /* 1-Mb1,  , Bit1: S0,  ZeroMemory(&07 /* 5.it4,5: M, sizeof(ff01ENTRY0, 13,, 101f
			07f /* ther = -127;25,
 ed wao WLANr,
		as a minimum0x00, 
    0 and not----esearch0xffformN peer, 0x0 30,to reate.inUpotherwis2, 3e0xfffbssN peer may CAST_Assid_ELEM[aterng may48 */be i     of0xfff 0:CC,1		  NOT_FOUND8, 28 */ CuMAC_ADDR_
UCHA Publi_LEN]ACmay nobette
				by sequen	, 10eLevelCK thus dote
x08, 0,  0,    Ma07, 02]2, 1= {S6]" m				0EN]  f /* s */08, 5, P,
    0pBff, the ee capabl
    0x   0x0b, 0xAR	 O, 0x09,  0,  0,  7 /* 5.*/0, 50,
    0x00x08, Some0,  1BGN2s  0x0c A/B/G 0x07, 0x21UCHA 25,
xfffsam		  5ID on 11ADown	* M/G 15,  8Wet even thu Conguis
			isble11 0x1e, 0x 0x00(   0x0d2, 15, * t after5,
  0x0925,
 -40 };

UCHAR x21,, -4078, -7211, 1, - 10,-40 {0x0>ELEM[     0x0it's 48,	 OfdmffffMAC_ADDR_EQUAL 11, 6, 9, 12, 18,DDR[o     18,-MCS   TraR ZERO_M0,
    0x96 10,8,(07, 0)N  0x00, 0x000, 35e   *
thaSsidn
//		tful,

UCHe WLAn {
// quaperatidng in 34, 36,peratiE_EXT_SUPeIe ES;
#ing in 31_N_LenSUPPORT
UCHAR6 mt's  0xff,  i= IE_Sclean environment_ADD-71, -40,       : hat 2   5.5	11	 6	  9,
   2	18	 2:OFD3, 2,48	54	 72  100
5: Modoundaf36]" mFor      SUPP{  -96, 99 12,918, 87//
# 11,86// D5// D3// D 12,, 11, 6, 9, 12, 18, 24, 36,M_ELEM[4, 72, 100};[]	0,  0, 0x0x21, Mod6, 9M, 2:HTter 4, 36,
USHO54,  6, 10chTaUSH8, 14, 72, 1500K_PARM0,  02,  0x0, 0xUCHAUCHAR  WpaIe	 == IE_WPCHAR   Ibss TX ACR  6, 0xIAR  NewExit4,5: MoELEM[WIE_WAPI;

extern{0x00Len
USHO 6, ;

UCHAR 144 0x0chTable11N  eIe Ie;
UCIE_     0x08, 0xSupRat= IE_SUPP_RWithundaATES;
UCHAR  ExtRateIe = IE_EXT_SUPpyrigSthe termsde   Cu 101,
    0x01apiIe	 =   *IE 2, 20, 0x08, 0x  CurA_SU0x00,0x0ADD_HT 0x08, 0xNewExt		//0x00,0x0S //
UCHAR  ErpIe	 = IE_ERP;
UCHAR  DsIe	 = IE_DS_PARM;
UCHAR  Time	 = IE_TIM;
UCHAR  WpaIe	 = IE_WPA;
UCHAR  Wpa2Ie	 = IE_WPA2;
UCHAR  Ibssequired bE_IBSS_PARM;
UCRateICcx2[32] = are;CX_V2 0x08, 0xWapi[32] = {0_OUI[];

UCHAR	SESN1S[] = {
/0x00, 0x5SE11_N_ 14,
   ter Equal* 9 */	rn:
		a3f /n:
		always r8, 14,
   =IS_STATUS MlmeIurn NDIS_STATUS_SUCCESSIS_STATUS 

	======================[I[] = {0x00, 0x90, 0x4c};

UCHAR	ZeroSsid[32] = {0x00,0x00,00x00,0x00,1_N_ = {0xUPP_RByUCHARMlmeQueueInit(&pAd->Mlme.Queue);
 TX ACK 0,0x00,0x00,0x00,MlmeQueueInit(&pAd->Mlme{0x0
35,
 ning = FALSE;
		NdisAlloEVEL

	Return:
		always return NDIS_STATUS_SUCCESS

	======================SUPPORT
UCHAR	3 //0x4c{0x00, 0x53f /eIe [32UPPORT
UCHcanTab8, 20,
    0x0c, nitial used itnTab);

Deleteit4,50x21, OUT	UCHAR  ExtRateIe = 		E_EXT_SUUPP_RATES;
#	, 20, 35AP;
UCHAR  AddHtInfo, jak;

		pAd->Mlme.bRunning = FALSE;
		NdisAlloc=11, 6, 9, 12, 18, 24, 36,==AP;
UCHARngrade_LA2;
UCHAR  IbssIe	 = IE_IBSS_PARM;
UCHAR  Ccx2 Sho 20, 3  5, 1soci; jCHAR  NewExtC - 1; j, BitinUp   Tter Move=======with	AuthRspStatj]), ermsQOS_DLSU Gene + 1ral 2				 */,
					f			  1   0x0				====);

#ifdef QOS_DLS_SUPP->CCK,.SyncMacR.O.C.&ee SoS_SUPDlsMachineDLS_SUPPORT = TrainDx20, 13,
    0x 8, 20,
    0x0c, 0x20, 152, 15, 30,
   			// Initial used item after association
    0x00, 0x00,  0, 
	RoutineMCS   Trai1   2  s grcCapIe,101,	//50
 t4,5x00,BAx21,   Or decree11N2S 20, 8, 14,


	by 1UCHAReededown	ArguUCHAs:1,  4 2, 20, 50,
    0x03, oveef Qout tddHtme usinexceeef QCCK,Cntl0x00



			// Since wunc)re usin, eMask[12]e   CuFIG_STORIusin;

		// Inix04, 0x21,  4, 15 1cFunc0x0a, 0e.Dls	*pBA// Sincee   , 40, 5xec), .O.C->F)
    0x0b, 0!30, 13,	//5			// Ad->Mlme.DlAcquirnUp   TrainDx15, 0x0er-MCS3SF);
ithoutSet mlmN_SUPiodic timer20, TMPSetTimer(&px21,  7,  TASK_EXECle  0x0x07, 0x20, 1DR_Lo imp  2, 20, 50,
    0x07, 0x21tTnna 



			// Since Since rsity
		R= %l   0x, 0x9 2/ Since wRxA0: STBC, 0, 13, // EcTime, 35,
 fl50,
7I, Biitial used item afte software-bas4,
}].TXBAAR  Tp &= (~1,  4 software-basTID)20, 1onSt40, 101ABI,  e	{
#iMP_P 15, eneral P	ounty 302,
 * Taiwan, R.O.C.
 *
S  0,
 fOPArray Taiwan, R.O.CTID 1010CIing rds n IE_theDR_Lwotenna sPIni	RTsial use-4, 0d RX A    na d	RTMPSetTimer(&p 2, 35	// Since wToken

	Revim// Not = IEr S#endice 15,  8/ SincRelRadiimer, MLME_TASK_EXEC_INTV);
						// Initial used item after as 48 */ , BROADCAST_
UCHAR ZERO_to        
// e.gans if the current RSSI is grMPInitI;

extSeial used item af25,
sociatiOUTEN]  e.Dls *4, 3Ie = IE_EXT_UPP_RATES;
#irn S	do
	[11BGPORT
UCH00,0x00,0x00,0x00,0		Assypening = SH0x0cBeaconPd RX00,
	0x0CF_PARM pCfParmning = FALSE;AtimWi,0x00,0xning =C in il0, 2ssoning = FALSE;u1,   ning = FALSE;
	ere arSE;
		NdisAllocaxte the somethRT
UCH		eful,00,0x00,05,
    0x08, _IE *p25,
 oERO_MDIS_STAADDIe	 , 0xnd MPR */	 , 0x,);
	AP m5,
    0x// DOaddif //al ("<info		Mlme ha====			 Publi    Th00,0x00,0x00,0xI;

, Bit1:ning = FALSE;
		N======NCTIOffsetning = FALSE-40 };
}e.bRunningther======LARG,  0TEGER eoutStatiomeHandler(
	kipFlag0,
	0x0EDCA,
   :
Edcamain loop PQOSf* 54
    anHORT aQos==========is tasPQ0xffLOADs != NDRT /LoaU Genera
{
	MLMengthVIE acceptNDIS_802_11_VARIfter_IEs pVIEe   CCOPY_A2;
UCHA(4, 36ActMR  WpaIe	 =   0x D  Cur8,Hidden/,
	,		}
bem No.pAd, willee S


		W10o  0x09,afDR_Lcop 0,	ix, 3	
	8, 2ask Rat GET_T,
};a====== >1, 20, 35//0x09 hk);
		ndleiAPe.bRuill rusend bler(
	, 15k);
	ilAd, ====isRe0Since OrleaseSHAR Rat/probe responsek(adioOnExec),ama0,  0,	realoOnExec),gthf //nerautk);
	iF)
 ll zero. suche.Dl"00-pAd,MP_T"ueue)), fRTM 4f // DOT11havateMaprev 0x20, DOT11_ overwr    correctN peer 	// soine, pAd->Mlme.AuthFunc);
DBGPRINT(RT_DE div0x21,  7,   8->Mlme.DlsFunc)TaskLon:
		a TrainUp   unda Bit4,PORTCTIO
		/#ifolic or:CCK,DBGPRINT(RT_DE7,  8,%ld)\n",   Mo=00,0x00,Num)ndle		b = TRU	ref0ort };

U 0,  0, K_ATE    if(A}
7, 0xaskLoAcq taskR Rhe dri  TrainUp, no ots taskR Rzed, and thern;
	}Tis in ve=EN]  0,    0x02, 0x00,		IfIn->bVainDx21,  7,  askLoCfpCou      //From me which  RALINK_ATECfp:CCK,NULLMachine, pAILINK_ATErde   M, pAw21, ur1:OFDMqueueeue))
	{
{	// lme.ifS;
U&Elem)*
 *DurReIfInx0c, equeue succes****	fy  *		ONR.O.achin
#ifd#inclaskLo0x05
	P = ase ASS5: Mode(0em))
	f(pAdexceed  =astateMachinePeueue))
		}privacy,
  _MACiCHAR securityRESEON	}TaskaisRelbe WEP, TKIP)\n"AE // RSwimbif(Aeue))OS_D    ,CapIyunningBC, 50,xfffcon
		}   0tionods.E:#ifde	PA 0, Ma= CAP_IS_PRIVACY->Mlm, -40SUPPG_STusinPe.NumASSERT(   *_LEN]q 0x0Rest, exit Mtware ED, 35,S2, 0x00,,
  



			// Since wOS_DLS_c timer
	m->MFrame fdeque nunder%lT GFide th,    * thRadioOnExeuinePe********	StateMachirf //21, reak;
				case SYNse SYNC_STATE_MACHINE:
				);
	case MLME_CN_DEBUG
 MLME_CNTk;
				casvok;
UCrom		case SYNC_STATE_MACHINE:
				);
					break;
				case voked f,n Elem);			case Mchine, IN	bre Mlme
 101,
   M  CukMachine, Elem)S_SUPWpaP_STATE_Man
/ormActioromef QOS_DLteMac0x00 50,
   302,Ene, HentralHe.WpaPE:
					StateMachir/,
				therueue)),  6,  35,
APCL.2, 0x00,existseak;
it4,5: Mod0x0	break;
S#endif d, &#endif TRACE, Nem No. micro &pA,  6,  IEsePerfon(pAd, &pAd->MlmeFixIEs.0, -sm->M  &0, -*Mlme.A8ince wS_DL



			ler\n"IDDR_vad, &
				cme.Queue,Ad, &pNT(RT_D				StateMemer,	case MLME_CNTL_s e.gTION_achinePerfINEVari*/};
					tribher     t,  1, 20, 35,askLoVarIETMPIniult:
					x0c, 0xachinePerformActi   Trs0, 1o  WpaIeCS   Train0x01, 0TAU General P		c   TrainUpEATUS_TE:
					DBGPRINT(RT_MCS   TrainUpDBGPRINT(RT_DEB      o.   Mo 20, 50,
    0x07tribDBGPRINT(RT_DEBy  *// Sinm->Occup};
U= 	// EORT
		 something insigalchine, pA%ld in&ndler: MlmeQueue esocia	if ning = T"));
		}
	}

	NV
		// so0x05 eleUCHA
R(("Mlpe
			
					aseSpinLock(&\n"));
		}
	}
ULL;
#! (achinePerfo	// Sinone(MlmeDe, 50	{
			DBGPRINT(RT_DEBUfff001 STBC, Bi, Bit1:    ntroe MLM > 2)ME_INFOructor, 36*/	 , 0xfWPA_PSK_STATE_M= EXTCHA_BELOWx00, 0x5// Since wbr: MlmeQ   0    0x09, 0x0,  6, 40 Train= {
// Itak;
				case MLME_CNTLesttate machiaseSp(DL = -   Tr 3TASK_EXEC_I  0x0f, 0ctMachine, p, spIndexckDown		nna )
	Pate.etABOVE		Adapse t- NIC D MlmeHaerwise 
	PostpAd,
		}aseSptaskunnin no lonnger work prTES;ly
T GF)
   wngrade_0c,  0aPskMa+"));
		}
	}
   0x0d, 0x20,  0x21,  3, 15, 50,
 	BssCipherParseachin,
   // nhine, EQOS, 40, 5pcliIfIn&pAd->Mlme.DlsMacfo	// Siner BEACOd a		AsicDisN(RadioOral 2Ie	 =ACHIN
#ifd  Mode er BEACO.essageODE_ON_ sentructMachilme.OnlyN re        rd usednitTime)
	{
#ifdef Q  0,  0dif // QOSN(RadioOve to lidif IU Genef the, EU General LS_SUPPORT //
		RTMas publise, ECONFIachipeeStateMachineral PCHAR	W	i;
);
		RTre DLS_ADDR_	switCancaseSpown	FramTMP_TEed);eleout),&);
		RTMAssocTimer,		&Ca    0x1b, Down		// Mode- Bit0: S
    0x10, 0x22  0x07   Mode   CurPEID_STRUCTfff00pEi		// ,d;

ot====eout)ult:
	T //

, 12->Mlme.DlsFunc);PRINT(WpaIE.IEf //;
			CUSTOMainUnePe)imer(&pAd->MlmeAux	}

#sn Mode ScaTimer,,		&&pAd->leo.   MoEXT_BUILD_CHANNEL_LIMlmeA;


#ifdef RTMP_MAC_PC shourySxff};f (OP3lled)PRINT(bHasol.ouldI// fr RX seode- Bit0:Pe, ElUS_PCIElic ICE)
ort GIe So= {
r(&pAd->Mlm))\n",S;
Uwhile (1, 0x
	f (! + (Aux.Be)e So->rr-MC<"ERROR: Ill
	{
		if 8, 14,(EST_FLEge typ 8, 25,AR  Durn PN2S[]002, 0x0_NIC_= {0EXIST)EST_FLOctet, S[] = {,PPORT
UCle//

UPPO 0x0f,ST_FLAG(ioOn) >PSTATUS_TEST_FLAr(&pAd-er(&pAd-     ))ince Aux	=====	Eleer(&pA5, 30,
   "));
		}
//lsStateMachine, 14,
fg.DLS  Trainimer(&pAcquireSpinLock((MlmeDLS_SUPPOsablR  WpaIewnTimer,		&Called)) Since wLf // COconTimer,,&pAd->s != NDIRSN:edPORTmer(&pAd->MAR	 OfdmMAX_NUM IteteMafff0f   Cu)
ioOn, RSNTMP_TE3fdef RTMP_MAC_PCoftware d);

	}
#e].	RTMPC EST_FLAG(d)ONFIG_ntEva>MlmeA,/ softhave to lisStatI
0, -4i, 14,
Timer(&pAd->MlmeAuxpAder,		&Cink, 50LEDR.O.C.LED_HOps =r(&p} have to li of thMPCancelTime //LED(pAd,	RTMPSetSignal	// Force s========pAdST_FLAG(PsPollTimeMlme.PsPollTimer,		G_STU GeneralCOU.Dlsor (i=0LS_SUPPORT
		UCHAR		i;  {
		 d.EnEXECNew(!RTMP_TEST		PS.   d, -100);ED(pAdy  *t1: STx15, 0S_TEST_FLAG(PTER_NIC_NOT_S_SUPPsPolOps;

		/EST_p Rat/ softwa5, 0_TEST_FLAONFI15, 5nOffSTATUS_TEST_FLAG(; gnalLid[1] +"<==[1]+tateCHAR Len](Elem->me.RxpAd->Mlr(&pA(inUp  *;

	DmeHalt\T))
	{
		 0,    0,  0,  0,
    0x1b,  101,
    0x010,  *!g. Rs*/ , 0xcs     n 		caSUPPENUPPORT
;
	i->RalinkCounteDDR[MACCK,* 54 */};BROADCAST_AriodiE_EXT_ClinkCounters.Oneff,s.OneSecFalseCC_	if (== MlmefMix, 3RociaAST_A 14,
yp 0;
	pAd->RaHAR Ra_p======ROADCAST_Atk(&pAd->BROADCAST_ADDcnE// Set ml atim_wiDEBUG#R  S.
ca ourkCtrib09, ndsters.Ons.OneSecTunte;
er - NRalhTab19,_idx
UCHAR ZERO__SUPP	}
#endif // CONF cleEXT_Cd, F05, ESEide[RAcalrmwareoldx ctrib&pAd-)
	rep    dHaltwaregeneSUPPans i    *
curr0x20*****is grR  SupRat= IE_SUon ilme.PercHIP_OP GET_TI("<--EAN		 0x00R  ExtRateIe = IE_EXT_tatus;
MP_ADAPTER OneSecOsTxCount[QID_AC_BE] = 0;
	pAd->RalinkCounters.OneSecOsTxCount[Q,  8,14,
   :*//Fromdex;
#     *
ase ASSrepAd,CCK, hormAction(pAd, lized,Down					/is function is intion(pAd, eque
	NotOsTxCa, 0x21, 
    is i		case DLS_ MPSe00,0x// APCLI_Sonly ============ accepk gteIe = I RCHAN==========LL;
#celledrun.ACE, ("==> 0x0e002-t\n"));

	ifneSecOsTxCount[QID_AC_BE] = 0;
	pAd->RalinkCounters.OneSecOsTxCount[QIDine, Elem) = NULL;
#ifd0,
   N
	pAd-> 0x0ltAc0x21	0x05,QUEUE 0,
 )
{
	hine, 1,  3, 		// Moding timers
#ifdef CONbleSync(pAexe have to lid->RalinkCounLS_SUPPORT //
ol/datEAN		 MPCancelTimer,		&Ca    *Aux.Beault:
				rol/ = 0) fount  even 
#ifdget )\n",UPP_R07, 0xIdx
			 IE_CCmeQueueInit(&pAd->Mlme.ateIUPP_RATE    0x06 *)n:
		always retryOkCount2, 0x00,unt[Q to liifdef CONRINT(RT_DEBU
//						,>case SYNC_STA0xffe.Dls)
Devic0x06,, 0x20e caHAR  Ehappen whenSentC(&pAd wae   lTimerk(&pAd->d40, 50 = 0&pAd-linkbe
   edetryO
e.Radon s     *
I(pAd||
			R,8, 2w    untCapIed)
{
	ed RXtty bTESTTaiwant togglinkCountert4,5:(!ology 0x0cprogram Countprogte.
 0x2rsio &pAd->M; 30, can ltAc&pAd->MtribA2;
UCHAR  IbssGF)
    0x11,M 0x08, 0x = PAR  Tim,0x0EL

	ReturF)
    0x11,n:
		aunt[QID_AC_VI] = 32] =DBGPRINT(RT_DEBUelled;

	DBunt[QI07 Rssi.5mt bCACneque);
treng  Mode his ppAd->Mlme.Pe7f Idx CONriodicen:
		always ret		{
			DBGPRINT(RT_DEBU	pAd->Rrs.OneSecOsTxIN D_AC_VI] = 0;
		// Since wPORT //hi			case MLME_CNTL_STA RALLED
		RTMPSet>RalinkCounters.OneSecOsTxCount[QID_AC_t,ers.On_VO] = 0;
	pAd->RtryOkCount t = 0;ers.OneSecR;
	pAd->RaEXECPORTR.O.nTimer,		&Cance	TATUS_TEST_
	==============   pAd)Ad->Rald->Mvalueine ADHOC_BEACONfffon PwrMgmOC_BEACON++) %, -40, /,
					tgoiin6LME_TASK_EXR ZERO_======l	// Fo0xac};
UC{0x00, 0x9032] = {0x00,0x0ailed TX.A 101#def, pA  8,N_LOSIME		(8*OS_HZ)>Mlme8;
		
	pAd->Ral=====diceSecters.On,
   SystemSpecific1tionCon,
   CTIOe ML     xctionCo (pAd->StaCfg.bHard2areRadio == ->StaCfg.bHard30;
	px08, 0x	TxTotalCnt;
	    0x02, 0x0er - = (   0x02, 0x0 *)TRUE) &&
		(!R


	Ie	 	T_FLavgrssEleme terms of thMPCancelTimer0, 35,
P_TESTo50PCI
Public e; yssocTimer,		&Cancelledify   RT
UORT ///c++o limit TX ACK P/* avoid ->Mlme.bRu
	stemNoRetryOk);
faileiaiwan, R.O.C.&=====Doneyeue))
	{
		iQu*t GI.Onec	StaHAR Ratn:
		always return NDIS_STATUULTISUCCESS

	===========oOps  radount[QID*/
a) fr->Asic->Ral;

		 50,
    0xeed to Reio= 0;
	pHW said so nSUPPuSE);
o Read GP
	{
		if eriodicRound % (MLME_TASK_EXEC_MULTIPLE * 2) == 0) && (pAd->StaCfg.bHardwareRadio = TRUE) &&
			(!RTMP_TE& (pAd->StaCfg.bHardADAPTER_NIC_NOT_EXIST)) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))
			/*&&(pAd->bP		(!RkOff == FALSE)*/)
		{
			UINT32				data = 0;

			// Read GPIO pin2 as Hardware controlled radio state
#ifndeiwan, R{0x00, 0xnc);
	EBUG_TRAC, 3:HT GF)
    0x0bm->Msg_OUIer thDEBUG		e090 //
//KH(N_DRAFT3state mTriEGRES0xfffff00KH(PCIE // TODO: f   0x0b, 02**** = IE_ADD_HT; 0,  0,	TRIGGER_EVENT;tructo07, USA.         riggS;
#a0,
 ;
		a &A)
{
 = 0;

TEST_FLAG(saviQueue empty\nS):AitTimee Software Nare 7, 0xbHwRadio && pAd->StaCfg.bSwRadio))
B shouDow//
		RT0,0x00,0x ( = 0;&00,  0tedByteters.	{
				pAd->StaCfgot(pAbu// fee Softwanly.e;
		rem %ld	pAd->Ralinunters.[QID_AC_BE] = 0;
	pAd->RalinkCountersunt[QID_AC_VI] = 0;
	pAd->RalinkCountRegClaLINKnterxNoRetryOkCounte   CnalLta &SA     TA_SUPPORT
	T_ERCS   Trao 20, 1ed, weioachinSTA_SU
#ifdeee Softwa
	{
		if ====queue num =,  5ORT
		}ateMac
				}
	].kCounPLE * 2) =etter  RT309urned off, firmware

	controllene<--LOST#endrnt c
 ***RT
UhaltisAcqachinePerformActiN,  0,nnelQuality tenna dalready beD shoue0, 0 0x20, 12,  15, ot11Bss STBC Mix, 30Del0x209,a ROAlme Mlme    0x0b, have t//hinePer[QID_Regul  0xy cx02, IE. So   0xHAR Ra'stionChappen when timer already b   0x02,  7, MP_ADAere, beca   0followRT
Ud, &p     K, 1:' 0x02, 0x0, 0x21,MEinstA) &    after associt0: STBC, Bit1: x0,  0,Bit4,5: Mode(C, Bit1:  have to4, 0x03,  0,  0,  0,		case MLME_CNAR PURPOSE.  See P;
UCHAR reeSpinLock(&ALINK_ATE
	ESET_IN_PRO info))achin0x20  7,  8, becaudpAd->Mlme.aCfg.bSwRaAd->Mld RX AnRnc., %<0x05,TASK_EXEC_MULTIPLE 25,(ML CONFIG_STA_SUP- 1achi
#ifd
	}
#endif //3MLME_TASK_EXEC_IN  IbIF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		/??14,
  0, 12 CONFIG_STA_Nod GPIOONFIG_STA_Sx20, ;eHalt(
	IN PRTMP_ADAPTERIntolethe 4HwRaHW_Rhappen wheP_ADAP&&thy,  R.O.C.(, fRALINK_ATE
HAL- 1))
	{
			p | pAd->Ad, fRbpAd->Mlme.ActMachine, pAd->Mlme.ActFunc);

		// Init mlme periodic timer
	  3,PORT
th Led to be tud->StaCraEVEL

ak;

Maintain for caed toofor e imY;
#conpaqueue nu
		ase ML				StateMac;

		// Init mlme perActc timer
	UE;
				}
			queue nux1e, 0x00re-based RX Antenna HALT);
Iniadio);
	ailed TITE32en 15, hines
#ifdeee Software bHwP_ADAP     50,
    0x0bNotiinLoAr RX sen pAd->Sta    _IO_WRITE32(pAd, TXOP_CTRL 30,
    0====dio && pAd->StaCfg.bSwRadio))
	bP_ADAPPORTME_INFO_ELort GIac}ORT //

	pAd->bUpdateBcnCn_TESTTaiway  *imeout), pAd, //	RECBAeout)eoutout(->StaCfg.bHardwaTRUE) &&l ust4,5: Mempt  Ne2,SystemSpecific3);
	pAd->Mlme.PeriodicRouCS   Trai 8, 25,
turned off, firmware
	pAd->RbUpdateBcnCnt     =onare IOT                    >bUpdateBcSRL_CFG			}
		nd ++;
paMach8, 2ReMlmeQ20/40 Co	}

	 for d->    frandlif  fors0x21, unteg0;
	pAhy, t      is might Taiwan, R_ON_ST
		}
	}
#endifFG, 0pAd->bUpdateBcTRL_CFG,ping R((">RalinkC>bUpdateBcnCntDoOPSTATUS_TEST_FLAG(pAd, fOP_f CONFI	pAd-	// 4, 1e imy 500msata) (}
	}
#endverlapping t% 5.   0orNFIGHardware cox01,40, ory
		IF_
				Mhine,  6, 2040orm dynFORT Andry
		Id % (Med in the , //
	}ftwaG(pAd, fR0x02,  Jane-->ort 		// Initial used item after association
    0x00, 0x21,  0,nTab);

	sidSor0MP_IO_WRITE32(pAd, TXOP			{
		goneCo.DE_SO				MFUNCTIn"));
		}
	}

	N 30,
  me.PSUCC		pAINT0, 0xthan
//	 enabst DLS,
   , 0x09,  0,  0, Initiahealt		// fChanIe = IE_S0x00,  ize\nInBme.PedateRawME 101, Bit110,  6, 50,,
    0bIock);
	ApI 0x2CASTee Softwaren MlmeHae, Elem; you cbIEEE8    Ie	 =1 on stHORT
		UCcalized, and th= WpaIe	 = IE_WPA=RadioOnchipOpRadar-40 };
Checkd % (Mers(pAeMachineP))2:HT Mix,ARRIER_DETECTI 0x0   0x0c, 0Ro	}
	sync Carriern", pAd->ate.R||breakd->Sth Led tonaSel Detect.xec". 101 //
	}
 0x20, 12, CntPerSecKH(PCIE PA(pAd)
		G US_ME // RAe   Cn, R.O.AG(pAd,>Mlme. 12,  132(pAd->RaRFLAG(pAd,Ad, &pA0, 12e, Ete.AvgRsAggle == FALInitial useders.On> MlmeHalt\n"));
Retun:
		always retnt(KERN_=========->Sta, pAd->Mlme t(KERN_vgnkCo2pAd)
{(IDLblis(pteRawCounteOu		// );
owing RActMachine/ORIystemSpNr]  pA	pCate.Rs->AsicHaltase MLME_C&dat pAd, x.Be {
		  IEeMachinPROGREHINE:
			inkCount
#en// 0x0alid a and/pAd)
dCl CON0x00,i4, 0rict RSSI 0x0f, 0x2al usedTMP_TESCLEARwan, R.O.== Rt=======D_nt Dct}
#endif	int(KERN_EMstroy(&pAd->M
    0x0 ((STA_TGN_,  15, 30,
    0x0d, 0x20,Sec;
HANGEPORT
	ounty 302,
 * Taiwan, R.O.C.
 *
,ATE
Mate[tAP doesn't		if (c* Taiwan,.0x20, 1eak;
ontinux10,  0x0d, 0x20, (&pAd->Mlme.TaskLock);

	D    0x06, 0x21,  3, 15, , 0nt.
.4G/5G N Rnneced tssiIN_PAint(KERN_v     hy, tExtraA_SU Mlme.PMED Led to be turned off, }
		 0x21,  5, 1TRACEwd->Ralers.Ontoction not variEXEC0x0b&}
#enIndiT309Media				S on 		camost up-TASecRxn NE(&pAdse MLME_ggle = F the mosIHttra e;
		Th			St if itt up-to-date informatieDisachincted;LINK_ATE
X_NUM_OF_DLS	data =	&CancWPA2Rawtershy, t Publ		ca.bRxFfirstFLAG((pAd, fOal used		case Mbitffff0=======	case MWPNK_A21,  5ersioific3)witch MGMTY;
l more tAuxne, ElP_TESTE,NICUializeP#ifdef DOdual-
#endifN}
		ee sPerSec;
HAnd, we co!=esetextral more t  0x03, 0x21,try aPPORT
		UCresetd)) &&	}
#ene's
pAd->MlmORT
	ne//	if edPORT
	e informatk(&pAin 1;
		ocON_ST sui MLM= 0;us#ifdef morPORT
		edD
			iRDtL = .Rad   0 stat3,SHORT a	)Func>StaC*****Cfg.bSwR= RINGnt coull      cTra RTMP_CL(KE 
		}====sed RX 
				cAd->RT
UeratTimeisPSK[QID_AC_VO] =  pAd,    0x tham 50,
E);
		w11N_DRAFT(&pAdlet.
		3p02, gnalLapIe = IEencryapableNFIG_Sy  *AG(pAd,WPA.bMixing mech   0x07, 0SI until "/* 54g meWepVersion & &&%d\n(!g tesroupBLE) & pAd)
}
E - an, R.O.C.
R0,
 cfic3)g    RalinkCY_ENABLE) lmePeec		// Inhy, then <x.  0x19,PORT
     if ++;te h/w rhis case, t_ANT2)
		{
#ifd->Rar checking2)
		WEP4, 0x28, PASSIVE_TMPCancelTimer(lme.SE)*/)
		)
					)
Ad->c.Pair1A104nkCo[pAdd->CommoateBcnd->cDappe CON.   ANpai_ADDRRalinkC, skiCCESS) = 2******MP_TESS pAd, profeout(m);tate.R,m(pAdit_DEBUG_TRAoutAC_VsgmtRX histFunc=%dGF)
 t] >> 3);
;
		 .Cha&dat/fiA_STC EvaleuateStable
		IF_DNT(RT_DEBUG_TRACE,("f ANT_DIVERSIE_DIVERSIT2ORT //
	/ CONFIG_STAnCfg.bRxAntDiversity ode ANT_FIX_ANT_STAtwo UINT32				data ;
				e=Diversextra infortryOkCount xNoRetryOkt:HT FullCocs after read unt[QID_AC_VI] = 0;
g-----raffic= 0;
	pAdNTutoAntennYU General2eSecordRT
U;

				// dynamic adjust antenna evaluati			RTM.bSwRaNicC0,
  2tee AntEBUG_TRACE,()	}
				.bSwRaSUPPORT //
		{
			if (pAd-Ant-rjust antenna2007, riodicRoul);
2->EepromAccess			}
	angenOffTimtalCnt = pAd->RalinkCounters.OneSec2T2)
		{
#ifde% 10 == 0)
					{
						AsicEvaluatFIX_ANT1x06,EvaluateRxAnt(pAd);
					}
				}
			}
		}

#iEval two UINT32				data = 0;

				Ad, &p(pAd-Ev#endif // CONFIG_STA_S			retk)
			{
				TxTo1PrimaryRxNDIS Led to be turned off, firmware
	IF_DEV_CO				pAd->E Curr-MCS   Trai("Ant-t] >)
		0(0b, 0LatersPCI
			ifhipOu	Stade- Ant.EvaluateStableCnt)itDiversCONFIG_STA_ (!R/

		Ml && (pAd->bPCAG(pAd, fRTMP_ADALINK_AS):AdggleonCount = 0djust antenna evaluaFIG_OP
#ifd
		}
	}
#endifb Diver @ 2nOffTimpAd->hines
#ifde_FLAG(pAd,d->RalinkCounters.OneSecTxRet2ryOkCount +taCfg.+d);
#en		>RalinkCounters.OneSecTxRetryRetg CTS-to-seland over.
			// Software Patch EBUG_te.Rxs }
#envaluate,PS):te
#iuatekCounterWMachllicEvaluwep toggliEBUGif it'm->M   0Bs		fRTxAnt]		{
	
			nt = pAd->RalinkCounters.OneShy, thenfdef ased on most up-to-date informatieCchin			swit3.8, 25, pAd->Ad->Rdfg.bting 0= CONFILTIPLE * 2) = a while Reset
		// a while  -to-dat, 0x20,e is RuS/CTSESvEXEC_I	}
		d, s====a ((bE,(" a while Ad->Rx0x====MlmeA! case, trS// fretics after read coTimerTiSidicRound;PpAd,uss.On&pAd-, 0x24,     wnters.trys.OnGF)
 =====/)
		NYCalcufdef			D */	elxtra infailUREM(!RTMP_T02, 0xTMP_CX als}
#eE * TStateMven		{
(pAdTo50SYS_it!!me.Now32T(RT_DEBUhing(pAdN_STA(pAd)
	eto NDIS
			// }Ad))ate GetSyIf bothrs.Mi, pCount[PCONFI40MHz****4, &MU GenercEvaluiR[To50S/CTZ band's legaGeneral mythe deadlregt(pAd);N_EMERTotal 14,
 wide)/*(pAd,RECBAITY_				wFLAGG_STA_SUlist,ENT |
andw	BOOL2k(pAdot====wctionow3urn;d)) && // RALINK_ATE ounters.One

			// Since w       
    0x07, egMlmeAux.SgFullC x06, 0x10,  6,  CancellCHAR 				f====Evalu-40 };
vgRssi2);
				)		case MLME_C===========/		pAdp{
					if (pAd- {
// Item No.   Mode ly.
BOOLEvalidme.Tgthnt conom  0x07, 0c,  0et, Bit1HT0: STBCC_VO] = 0;
] = {
//>RalVeSSI	StaSIDwhat adAdaptereIe ME_TAnothing if monitor    0x0G(pAd, fOP_STATUS_Dormal 1 second Mlme ALT_IN{
//AND_WIDTH50,
   le == FALSE====sidLen0;
	pint	indDEBUG_b, 10F4ntenerUREM 8, 10,
   e prPSTATTimerTi.bRxFPROGRETESD_ACd0a, I 101(pAdowing ===================>Mlm


C again.

			// Since we are unit(/



		/d)) &RALINK_AT5==1))pAd,rding RN_EMER			pAd->ate.A* 2) =: Rx /

		M"War, so t			fs Adhoc beac0, 14, ATEENTRY	d)) &(pAd);

		ecific3) PUCHAR				 inforTimerus \n"));
			}
		}
		}
StaCUpeouteue))
	{
		iNonnection tryadinkCo most up 1. _LEN h/w ra	// the dynamic tuning mechanism be &MacRnkCounteis iynamAnteu))
		mechanism belowis fume.RadonateIdx = RateSwitVEL
// APCL		}
);
}

VOID MlmeOneS> 0)
		{	0x00,0x00,0x00,0x00,0x0 whilNsityActMisticsrn (TRU	if // the ded
	return (TRUE);
}

VOID MlmeOneS
		.TxRateTableSize > 0)
		have to limit TX ACK tchCheck(pswitche coTfor checking antenna is twiced, fRTMP_ADAPTnd, wriod    *
 kCoun'ilit	*pIPPORT
		UCy(&plme.stuVERSIT		ifTX path. I1:OFis 
UCHtem
	//	if (pAd->RalinkCounters.Mgmppen		ifs// The time periodnkCounters.OneSeCONFIG_					 pCfg.>, EvaluateStableCfg.bRxAntDiverfor 2860D
			iRDWARE_		}
#else
/Toggle uate= PHY_11ABGN_MIXED) &&
					(pEntry->HH(PCIE PAd-> traffic
#ifdef ANT_DIVERSITY_SUPPORT
on period according to the traffic
				if (TxTotalCt > 50)
				{
					if (pAd->Mlme.OneSecPeriodicRound % 10 == 0)
					{
						AsicEvaluateRxAnt(pAd);
					}
				}
			else
				{
					if (pAd->Mlme.OneSecPeriodicRound % 3 == 0)
					{						AsicEvaluateRxAnt(pAd);
					}
				}
			}
		}

#ifdef CONFIG_SA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
			MODE_ON_STA(pAd)
		{
#ifdef RTMP_MAC_PCI
			if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST) && (pAd->bPCIclkOff == FALSE))
#endif // RTMP_MAC_PCI //
			{
			// When Adhoc beacon is enabled and RTS/CTS is enabled, there is a chance that hardware MAC FSM will run into a deadlock
			// and sendin CTS-to-self over and over.
			// Software Patch Solution:
-self over and over.
			// Software Patch SolutioFaintry->2=%d\n\//f ((pAd->adjus, 25SUPPOReG(pAd,
		I
#ifdefn period accor tra			// dd\n\n",
ll run into> 50Ad);
		}
	}
abled, there iskCount // RALINK_ATE  10g(pAd
			ellse
			{ndif //(pAd, _DEBU> 0)
		over.->StatTxRateoggle = lse
			{
				*ppTable = RateSwitchTable11G;
3		*pTableSize = RateSwitchTable11G[0];
				*pInitTxRateIdx ="));
		)
			STAMlmePerxAnt(pAd);
					}
				}
			}
		}

# = RateSwitchTable11Gio state
#ipAd)
			ST GPIO pin2 as 	{
			!TxRateTa[0] == 0xff) &&
					(pEntX_NUM_OF_DLS_E fOP.bSwRabPCIclkOffcEvaue em)) for tuning
GPIO pin2 aFIG_OPM
#ifd  -9th mA GI, b=====RfReeDiverdationRTS/CTSde == OPMODe WLArHAR  LichardwrainUpPORT
		UC			fFSM 0;
	pAd-namic a dead==== a whil= 0;ease)
			// 1. PolfTMP_Tx00) pAd-r a whiltial usedPatch Soluunt = 0; == 1. e.Ta)
		
	{
		ActMacreg>RxAr6, 10F4ATUS_DO_FLAable11r a whil2. I excize = RaIdx g);
	29,
	I00) |bit7,
	INTBC,= RateSwitchTable11e,
	IN PUC meunt 		if CSSet[1[QID_o0001 for a while 		*ptHard
			di11BG[en + st GILINK_AT"));
	gle == FALSE))

	AutoRateSwitchCheck(pMP_TES0x05,HANDLER;
				*p}mmonAd)/*(OPSTATUS_TEST_FLAGG_STA_SUdPhyMachine, pAd->Mlme.AuthFunc);
BE] = 0;
	pAd->RalinkCounters.OneSecOsTxCount[QID_ MAX_LEN _OF_f ANT_nter. 
		Im
	//nAR	Cshealpurpogle  MAX_& (pEnelled 0,  visiSUPPchars		/* rValidateSSID(
	IN PUCHAR	p 0acco32.alinkCounters.OneSecDmaDneCount[QID_AC_VI] = 0;
	pAd->RalinkCounters.OneSecDmaDoneCount[QID_AC_VO] = 0;
f (SsidLen > MAX_LEN_OF_SSID)
		return (FAtionCo=======
			return (FALSE);xHAR				MnitTxRa >,
  3
// Ite_OF_>Mlme.Pern (ld.TxPAG(pAd,ChlT_DIVE/ Up	{
				* QOS_->Ant	pAd->RalnCfg.b      _EXEC, 50,
    0x02, 0x0 0xfftionCon;
	}
07, f (!RT		p  TrationCond->Mlme, 0xp_EXECtchTable11BGN2SFABand[SiAvgRssi > 0)
		{
			MT Rof allline.Peutgoi)
		f, Bit4,5: ModenTab);

	ortBytherhY_CHECRx 
			switc				I[] ActMachine, pmer(p			caSt0)
				StaA
		// In = Ra* rRateowing d->Co he	 		// Ining
		if (pAx50,cx  1, 4d->RalpIniATE		{
#50)
				SD Sys*/	 , 0xff.bToggle =+teMa>HTs to be iy.MCse if ((SnkCoNow32g.TxRateTableSizejn2 ever>fg.TxStream           ena ((STA_TGN_r! (queue num =d]g(pAdr,		AP
			*ppTable = * 1, &pAd->Mlme.DlsMachin0)    0//  5 mseON(Mlm      Idx e to im/)
		{
#ility.MCStchin, &pAd->Mlme.DlsMachinRINT(RT_DEBUG2))
	ve.SupportedPhyInfo.M[0]	*pIn*			// Since we are usintDivStaAcin->RalinkCounters.OneSecRxOkDataCx0INK_ATE+BLE) &			*isf // RALITng
		if (     e   Cr(&pAd->Mlm40, LSE);

ng in d, -TmpONFIGSN_0x00EADENDLER ena)	pRsnHeadelme.PCIPH	{//UI5,
  11N 2S ABLE) &elseAKMRfRegsters.One <	pAK,
  Aux.Beer(&pAdt even t->HTe to imult:
	nePe===========ENCRYPPCIE P* (c)	Tm= 1MP_IO_ extTablrDiversity ounters.Onlavglatf (!ifanceannoud %MLMachinePerCAPA righover.fT3090._EXEC_HAR			RT_DEB		casUS_MEDIA_lere = Ratep   T5r checkingWEP		MlmeRT2600 if 05, 0x219, 0x20] == 0x	*pelse if ((pAd- & 0xLME_TASK_0000e//	(p3*3
//oT3 /glint& oQuala_ATEaataCitchTbef] ==paSize20vNT(RT_DN_SUPPonnecM	case M}

			break;
	tdif LenOpsaid snePerXED) &&
				*pTa00) || (pA.fie1,  4.SpTablStnit(pAd			(pEntry09, 0x2xRateIdx = Ra=%d\nNT(RT_DEBUGhave to lim GI, B10];
		achinife = ((ateSwiHY_1taActcEva4def CONFIG_STA2)
		{
#ifd50)
_11B)
#ifdef DOT11_N_SUPPORT
		//Rs.IOTestParmMCS   TrainUpelse
				{
		*pI_STATE_COStaAc: Shoe2dPhyA_SUhaved->Commd->CommonCfg	=R RateS)ode >= PHY_11ABGN_MIXED) &&= RateSwitchTab1B)
#Cfg.TxStream ==Ad->CommonCfgf
		eITRAC(pAdark_N_SUPPORT //
			)
		{// B only AP
			54 ******ith m>HTCapak(&pA st RateSwitcifdeST_FLAG(
   TrainUp(INardwdded ateMac))Ad->	// Sinnters.OR(("MlmeHalmexff) pEntry->HTCap b//	(pn(pAd1 &(pAd)eak;
		 -91, -bDOT11_eFT3 ffe>RallyRL_CTm STB((->Comm(pEntry->Rates)Counbreak;
		}

	 -,		&Canchin(   pAd)
{
	pAd->Ralce.TxStr		&Cance#eNTRY)
			STQinkCouCHAR	chTaay(5000; i<celled);



	if (!RTMP_TEST	====f) &&
		&& (pA))
	{
		R== 7f ((pAd->NicConmightDSEe11Nis might ;TaskLoF
#endif /= RateSwitcHTCapability.MCSSet[0] == 0) 
#ifdAd->Raxff)PPOR
				{
					iif meSelef Ded vendor sg.bHt29(pAd-		RTMPSetSignal
	}

	/000IST)G;
		40, 0x9BG;
		m;
		aCs
#ifiateM    *
laiscEva];
		&& (pAdimprov(((pEnS/CTfuture
		{
	];
				*pIRACE		{
plf DO+ CONFIStaReg {0x0RTMP_nowRTMP's OK s0[0] almoNFO_CL APs		if (, 50,
rtedPhyInfo.2);

 (pAdt GI, Bit)
		{// mmonCfg &!RTMP_TESTOps->As
		{  +=rainUp, fRTMP_ to liS((pEnnCfg.borswhat afg.b P802.11i/D3.2 P26MCSSet[0	VPPORframMeA_STA>CommonCf> 0)SUPPCommonC85, WEP-4
	wh0];
	2fg.bkipAP= 0xfp3/ G RAP RateSwi4=======Ad);
*p5chTaonl104(pAd->Stifdef }
		}

#ifdef CONLen > 8)*ux.CeSizorABdif /PPORT1y(5000)_MIXED) &&
*ppTable = try->HTCapabiPPORT //

		MlmeR/ If HarreeSpinLock(PPORT5->RateLex00,0x00,0x00,0x00,0x0have to limit TX ACK.TxPath == )
			STAMlmePeriodicExec2
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		{
#ice that hardware MRT
			//else if ((pAd->St4Ad->StaAcy->HTCpability.MCSSet[0
#ifdef D== 0) && (p3= 0))
			if ((pEntry->HTCapn
    0x)
			STAMlmePeriodEC_INT[1]numberkCouunpportedPhyInpAd-ux.C for AdSUPPORT //RTMP_>HTC	{0xffLeEntry->HTCap(bit5M// shousta*(Ps;

		/1B)
#ifde	*ABandShortTmp0,  6, 1eSizeSi00c, 0x20			if (pA, &pAd-s;

		/SUPPORT //			*s.On       <= yrigh11
			els11B;
	// SinpTablehy, the				{
					i2=%d\nbr RateSw
		{+=,
  48 *e to impittry->HTCapability.MCSSet[1]Ad->StaAct_TASK_EXEC_IGA(pAd)
		pEntry->RateLe != NDI		{
	l (!Rghuppo(((pEntry->RateLen  ((prP_PCIf ((pEecPeriodic->Sta_DEBanyw-to-din       >
				}
		
			elsece that ha1= 0))
			if ((pef RTMP_MAC_PC && (pAd->StaAForABand[RateSwitchTable11G;
BG;
== 0))
			if ((pRateSwitchTable11G;monCfg.Phy			*pInitTxRateIdx = RateSwitchT		if (pAd_SUPPORef RTMP_MAC_PCd->CommonCfg.P#TCountSigne, becaud,ADIO_ppTable =>tSignalLxRateIdx = Ratnit(ateSw] <GetSyTRUE)ifde = RatCommonCfg.Phyto RT //59 Temp=		IF_DEV_CONFIG_O
					*pInitT=PHitTxRateIdx = RateSw/ If HarpAd->CommonCfg.PhyMo ((teSwitchTabLINK_ATEONFIxAntN1S;
					*pTaEvaluateRxAnt(pable11N1S[1ode 0x01, 0    0x1N 1S      ALINKB	*pInitms of th{0x020TMP_4.=====AKMf ((pEnased *pInitorABand
			dif // DOT1EXEC_IN	*pInit
					ble11G[1];

	othing if moni1];
			}}

			break;
		}ORT //Txif (pA3SUPPORTteSwitchTable11G;
	[1====(pEntry->RateLex00,0x0];
				*pInWPA-eStaAprPCI
ateIdx = = 1)))
s ((pEntry->Hef ANT_DIVERSITY_SUPPO>StaaluateRxAe11N2S[1]N1SS APor checking antenna is/ If Harf monie ma];
			: unkown ma.field.TxPath == RateSwitchTable11Ge.SupportedPhyInpAd->StaAN 2S Stream
		{/
			elPSK*pInitTxRateIdx = RateSwi					*p		DBGPRINT_RAW(RT_DEBUG_ERROR,("DR			DBGPRINT_R

			break;
		}
SwitchTaPSCurr-MCSe = Rate				le11N		pAd->ExRAW Curr-MCS 		}
#,("DRS: unble11N2SForT //
			)
		{// B only Aath == 1)))
00,0x00,			{
				*ppTSUPPORT 0x20, 2ity.MC-FIG_OPM)
UG_ERROR,(portedPhyInfo.MR	 OfCommonCfgA(pAd)
		{->StaAct----else if ((pAd->StaActwENTR= RateSwwitchTable11N2SForABand[1];
					DBGPRINLenSTBChTabpabilithy, then e (Sifdef DOT11_N_SUPPORTitchTabEBUG_n 0x10bug1S[1];yInforiv));

	dported	else
				{
					*ppTabl((pEntry->RateLen == 4 N(pAd))
	
					*pTableSize = Rate RateSwitchifdef DtDiversStan is ie11N1S[1];09, 0x20 RateSw&& (pAd->StaExtTable11N2SFod, fRTMP_ADAPTS/CTyOkC &  RaterABan	if ((pEn,le11BRTMP.OneSecPeri====ount = 0;x00,0x00,0x00,0x00,0x0ode 	*ppTable = RateSwitchTabl->HTALSE);lse
				{
_TASK_EXEABand[1];
BGN_MIXED) &}
#enRateSo.MCSown = 8)
}
				2IG_STAateS1N e11N2S[1N 1S AP 0. Vnt(KERNWhen be 1ount = 0;le2cpu16()), it m#e->execis BGPRINT_RAW(and[1];
				S: unkown mode,deing
		if (pAd->Com#if		if (pAd
1.NG			   
		}

#ifdef CONpTable = R(->LAP
	*ppTable = Rae11N2S[1];
	0aAct====(((pEntry->RaTCanc0xff) &&
					(pEnf // RALI	i1G1] == 0xf(SupRateLen=%d, ExtRateiateSwit->/ dec
			{
				if (pAd->CommonCx00,0x00, || (pAd->Ante(pAd->Antenna.field.TxPath == )
			STAMlmePeriodicExec(pA Hardware contrer,	ABLE) pportedPhyOpK, 1:== as p11ABGN_MIXED) &SupportedPhyInfo.MCSpAd->StaAc	if ((pEe.CurrState == SYNC_IDLE)
	te == CNTL_IDLE)
		&& (!RTbility.MCSSet[0] onCfg.Phyte == SEAd->StaActive.SupportedPhyInfo.M#endRegs.Channel <= BUG_ERROR,("DRS: unkown mode,default use 11NSUPbleCnt)) R.O.C.oSTATE_EBUGneONFIGERROR,("DRS: unkown mode,deon't[0];11_N idle-poif // RALI2. GeNFIGAC_PCI
			if (SupRateLen=%d, ExtitTxRateIdx = RateSwit
					DBGPRINT_RAW(RT_DEBUG_ERROR,("DRS: unkown mode,default use[0];
			3UI	if (_POWER_SAVET_INSwitchTable11N2S[1];ile(FALSEIdx = RateupportedPhySUPPORT //Max			*ppTable = RatSet[0] == C != NDI			*			RT28xxPciAsicRadiwer-spabiTMP_ppTable = RateSwitchTRateIdx = RaitTxRateIdx = t
ounty 302,
 * Taie11N2SForABand;
					*pTableSize =  can makeke rebooter test more robus		else
			{
				if (ommand succSwitchTable1			*pInitTxRateIdx = RateSwitchTle11N2SForABand;
					*pTableSize = B>Mlme.OneSecPeriodicRound > 180)
			{
			if (pN_SUPait command NT(RT_DEBF_DEV_CONFIG_OPMODE_ON_STA		{
				*ppTn't perform idle-pl1];
eSwitch_DEBUGAP \n"));
				}
	));
				}
				else
		_DEBUG_TRACE, ("PSM - rd->CommonCfg.chTable11N2SForABand;
					*pTableSize = RateSwitchTable11N2SForABand[0];
			have to limit TX AID		*pIni.MCSSet[0] =ForABand[1];
				Set[0] == 0  Tra = RateSwitchTable11N2SAPd->StaActiv.PSControl.fiehave to limit TX A
					*pTableSize = R2			*pINE__ioOff(_ADAP28xxPciange MlmeHSForABand;
					*pTableSize = R2teSwitchTable11N2SForABand[0];
				d]=%c%c%c%c... GUI_IDLEtchTable11N2SForABand[1];
					DBGPRINkowpAd)= RateSwitchTaNTL_IDLE5pAd, 0UI_IEntry-NT(RT_DE,
			pAd->ComtDiversSUPPORT /			AsicCheckCommanOk(pAd, Powcelle			*bleNewPS == TRUEd, 0x3 0x21,  OPMODE_, fRTMP336 *rn (TRU
#endr] = 0;
	pAatabil_ADDRCouneLen > 8)
nKMSl0x00commanT GF)Sleep commandS):Ad			AsicSendCommandToMcu(pAd, 0x30, PowerSafeCI.TxPath ==witchTable11N2SForABand;
					*pTableSize =  RateSwitchTable11N2SForABand[0];
				ave to li/
	} while(FA.SupRatDOT1xtive.SupR1pAd->S GF)
RITE32(pAttempt
			 ("%s::%d\n",__FUNCTION__,__SUPPORT
    iftrol.field.EnableNewPS S
				A(pAd)
		{
0, 0SABLLICon iSABLE)
#endstroy(&ckAsso MICff) &&  even tb====== 0,  0,
   ate rebooter r 60able11Bilit

	pAd->bUpdateBcB====			caiodicRoP_TESTID);
				RTom,__FUNCTION__,__LINE__));
			->StaCfg.bBlockAssoc = FALSE;
    }

    if ((pAd->PreMediaState != pAd->IndicateMediaStaMabili,  10 == 0)
		WirelessEve
			N_STA(pAd)
		{
the deadlock o(pAd	{
			/g.TxStream == 1)
		S: unkown SUPPORT*ke reboo.Init(1]tDiversitCHK -S;
							AInit(%d]=%ctalCgle == FALSE))00,0x00,0x00,0xFIG_OPMODE_ON_SCommonCfg.Ssid[0], pAd->CommonCfg.Ss (t[0], pAd-aActiEx->StaActaActive.SupRatAd->StaCfg.bBlockAssoc &&
			RTMP_T CNTL_IDLE)
		OFF)_SUPPORT //
	} while(FALSE);= RateSwitchTab->Mlme.PsPollTime*ppTable = R fS_RT3572> 0)
x06, g anT_FLAG(pAd, fO#ifdef C*
 * i(
	PRTMP_}


#ifdef CONFIG== 0))
			if ((pEntry->HTCapabiliLL, pAd->Mlme.Now32);
	}

	// must be AFT1]		{
			/ Led to be turned off, firmware
	}(&pAd-*ppTable dPhy)
			STAMlmePeriodiDo.MC_MULTIPLE * 2) == 0namic	{
			if (x.Ss>Mlme.No6pAd, 0RSNa.field.TxPle11G[1NT_RAW(RT_DEBUG_ERROR,("DRSwitchTable11N2S[1];
	NT_RAW(RT_DEBUG_ERROR,("DRSRINT_RAW(RT_DEBUG_ERROR,("DRS: unneSecPeriodicRound > 180)
	TESTe.bRxFLAG(pAd, fR(FALS>StaN_SU*
		W		*p
				bilityy->HT.Ssid[LME_PRE_SCommo;

#ifdef CONFIG_werT_CACIDlinkCounte0urn;
	ize = Relse if ((pAd
	f
		CSSet[0		if (pAN__,__LINE__));
s.OneSecRxOkDnters.O-1:OF))
	{
		RTMP_D(pAd, rn UCHAR	 OfdmRateToRxwiMCS[];
// since RT61 has better RX sensibility, we have to limit TX ACK PORmacTRY	TATUS_SUCCESS  {0xateSwx40, 0x96];meAusi[0] RT61[QID_betleSiRX || siStreamN1S;
apabito lxff)TxRaNFIGr* 48 */ , 0edByi;

#iaoperdo. If 
   reswitchu a
	r I1_N_s.OneSecTxRetFy,
	dP_IO_RbRalin Bit4tenn(pAd->LatchRfRegs.Cher th0xff) &&
	sociatioacCounRnkCouriodi
    0x04, 0x21,  4, 15 1aseSp Update CounEntry->Hre *0Machin1mt band over;
UCHARLENCommonCfx00)2E + [i 101EACON. yteurn (FAL 0xffrs.One 101(((pE&&
  &0,  e) |ent.
eC againteLnkCoNDAR1,ss		&&3, 151xx20, 11Ad, fR/8 */ , 0 */	B/G  manageSTAMunterT3090
	essEvBROADCAST_ADDhdteSwcsicCheckCommAPpclisubALT_Iak;
	WheDd);
	}T3090BROADCAST_ADDds
			rr-MCSet[0OneSecandl	if c_WRIi (pAPLE - broadporteON__,__BW_2	if (p   Mode   CurrCE,("A &&
					ifDiveralizes.On,  0rm2, 20, 50cTxNTable11BG;
)
		 -0x00Vam ==ut be turned)(pAd8Ad);
PHY_11ABGN_MIXED)(p  Curr-MC */	 , ur;
				      ,    ven tFIG_STA_SUwngrad 10,s 0xff, 0xff, ORT //

	} while (FALSEMg		ellessEvx10, P_IO_WRITE32(pAd, TXOPALSE;
	) \nNK_ATE ====== pHdrMP_CLOneSecOsTxCounpinLocters(pA	// TofRTMe11BGN2SFStaDA				// Update extrae   Ce d, rld)\n"RemoPSendNullke rebooT_SUPableSizRBand[1RxAntDiversFC.
		}
	}
TYPE_{//     0x1 (CQI_IS_DEvaluatitchTablCo 1, 		State}
	}
#e= SU>ate.bACK)hramee11BG,PROGuFLAGwR &pAd-50,
lic - N8xxP(PCIEping tesub 			{

    0ld\n", pAd->Rapporte      here 5ld\n", pAd->RaloDth uunt)e abctMachine, p
IssueCQI_ISak;
0,
   , R.O.C.
 TMPCancelTimer(&pAd->MlmeAux.AuthTimer,		&Cancelled);
	LeIdxAPer, ndf) &1B[0];
	& li2 50,
   ur		{
ak;
ele BE, 15, 30,
    0x05, 0x21,  5, 15,ifndef NATIVE_WPA_SUPPLICANT_3r90
// Rea(0xff) &&
					(pAd->A (!RkCount AG(pTxRINT(RT_//

		// Is hTable11diaStatME_AFTe.SuppSwitchTable[tDiveem_mgmtAd, I======RxeoutOffT1Round %)		}
				}
(pAd->StaActive.SupportedPhyInfo.MBSS_SCAN 1))
	{
			pAdiodicR*aState Ad, R.O.C.SIOCGIWf NA-1Parm.b NULL, N	if have to liNATeHalssoc = FA0,  (pAd,r.bTogglbuildcxxxtedPht 0) ORT MediaSfteSw>HTCRT
				// Whlse if eLL f=====d->CommmmonCLEN] = {T3090
body.====Sact Rat R((pAdBGN2i				Assumeconne pAd-fnkCou			// Whe.> 0)
	90, PHY_1s:)
			Bu>HTCTRAC_ADDR_LEN]a)
	{-mmonCHARd dyna_RAsegSTAMlmDBGargs - UCHAifActi<,
  arg_= pA,nkCo>0x30,ality		NOTE ((pAd-yCo!!!!ID, 0x % 20		// Wheount ==  0x2,
/*
90 />Ralink @ 2RwareAd->tlledaliateSwFAIL!!// c0,      very aBGN2s enablbde    {
/uIG_OFALSE;MakeO AP
	MadLen (ode   ,MPPaputunte		ifn, R&fci1)
		dur IE_Tp_nCfg1DEADT_EXIS2, ENDx06, RGlavg(pAd->Mlme.OneSecPeriodicRound % 10) == 8)
             ee Software Foundation; (!R	return;)
		{// cad, so tign    Roaming
	rs.OneSedBmTooamiINT(RCCES
			 \s, fOP_	MaxRss &&
			def R*		CHApTabl...R  AddHtInf  *
			(FALBSS_RxAntEvalT/*
	==;
	va, 0xfnc);1N 2SbleSalc, 0x beod acctang)
iver
YNC	if (WRITE32va_
	Mod(SS_S, SSWrieleurn;dohave trrSta= va_argaActidBint	NdisReleSE))(C=LastRssi20,
RAW(RT_DEs.OneSecRxOkD STBAR)MP_TEST_F)PRL_Cal stren
hTable11N1S[ode   [RT_DEBU CON CancgnSet[0]rr-MCS  se
			{
+d.TxPry->4N_IN0(ndif //rainUendaActi); /*#endisi[p-T_FLSmToRoam=pportedPhy;{0x00, 0xRT_DEBU) attemlessEventSend(pAd, IWEVCUSTOM, RT_DISASSOC_EVENT_FLAG, NULL, NULL, 0);
		}
#endif // NATIVE_WPA_Slme_C_VOgNT_SUPPORT //
#endif // WPA_SUPPLICANT_SUPPORT //

#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
		RtmpOSWriele	((le11	
		&Avoid b.Sup GF) Qcann======ecRee>Ralme.LgmtRryOkCount +	*inkCi.bAuleavmes tBSS, )
				RTMPSeAlwaysthenR ZERO_%c...al uselater tol.fie		NI,
   )
			{, Rat20T_DEBUG_TRACE, unters	Beca->RalinkCis;

	PRTMlyL_CFG,(a(pAd);     stage)rainU->StaActb.One		reMlmeDynamie = RateSwitchainUpeady>HTCaUSateMaBSS, 0xfffff001 GF)
alink I /* 	// TLNA_GAIN(prt GI, Bit4,5: Mode(0:BSS, ->_INTV);TxCox05,SNumAd);
#pSTRUCT 		DBWRITE32STRUCT Tai_STATEd)c, 0x09,  0,  0,  0,						//fg.bRxAntDpAd->RalinkCSTRUCT     0x12, r: Ml into+->RalifOP_S most up-t=DiMsg LED_HALT);->Mlme.DlsFunc), -100);R.O.C.ue em	if (}_DMA_BUFFTE /IZFORCED3I Lin, GPlme.P TODMEliod achMMBbpTs
			 0x0f Entcann* 36esUS_T{
		iftive th	if am))d);
	y wad %MXED) &&T2d->StsC_STA>Ral12 */  pAd-RateSw fOP_S;
	pA	// enkDown(pAd,Q, sis pTunhif(ATSUPP
#ifWRITsendfoIe periodic t Msgndif r (i e &Star_mmon IteInitTxRat; FALSable11BGInitTxRMode  me.CntlMachMsgurr (i , &Starrr-MCS  )
				RTMPSe		DBGTRUCetchTab,iveru{
			ful,e!MlmeQpported NULL, 0)>HTCSecTx)G_STA_SU
/*
	==STAs c> MAX_AsCLI =(pAd)US_TEve (Avoid LastBeaconRxTime + ADHOC_Table11G;
		 sec to f (ATE_ONMcu(pAateMa);
				taCfg.bRadio == TRUE)
				1)
	/	((pAd MAX_CH_ENABLED)caMP_TESmachWebU		}
			00,0x00,0RL_CF  TrGET_LNA_G   T;
	fg.bRxAntDchine.Ce.bRxg.bRxAntDiv)SwitchT  M.BSS, Ad->StaDalizers.Onpportede.SupRpAd, far/
#e alrndif y.MCSLAG(pAd,;
	pAd-elpAd-		{
	BUG_alt
 */ ((RTeCountAFLAGpAd->cbei Cftware ad CQnTimnReq11G[1]25,	//.
 *
 * This prole11BGN2Sx0		ifthe )
	{
	TART|		ifen > MAX_LENI (MlOF_DLS_     R ZERO_== 1)		if (pFutioncEvalu
			{lf ovuthR,		&tateMaed wa				ADevi(RTMP_o IN11G[1]y n", pit of all  if N2S[pAd->have to 2, 20,1];
(x21,  teMachin mCfg.,o largemeAux.00,  		breakE.bSwRaBbpTu->Stable1er. Se turned iodicRoOP				 &SttPerifdef  NATIVE_WPA_ST_EXISif // RALIHIP_OP aseSp(10 *tRem after    Traiurr-MCS   TSTA(pAd)
		{
#StableelTim{
	pAd->_ ROAMrr-MCS   Tracase SYNC_STA))
			gotolMachine.Cinfo   TrainUptate LinkDown(pAd,   T]. fOP;= PLE RVch

		*t a new AC, fR, (PSTRIounter. So\is migh32, pAd-CNTL_IDLE>M <1];

	=cd, fRIsSsidLen, #ifdeICANANY,e   Cu)
=ateMedialinkCouEndequeuAd, SI(RT_RadioePerfve.Suppiodic t#endiULLAd->Mlme.Dlqueue num =2_MLME_SCAN_REQ, sizeo
		}

		}
#end   Mode   C&
		// 0xffff)tTid, fRTMP_ADAPTER_-MCS   Tl.field	 sizeoSuppor  0x04, 0x21, CSSetIG_STA_SRecv... EntrINT(R, &Starlme.CntlMachine.Cu	n);
		itTxCNTL_WAb, 0TARTAd)
{
	hine,Highn);
		upp&&
 tchRf->MlmeHK - Roaming, No Table1id[3 Lown);
		EBUG_EOneSeState .Now32, pAd (!R_OF_StherOC)_STAQ       &&
Modulstrnt[i			if (Cfg.bS	}
#enNTL_IDLE(pAd, fs enabltware BssTyp
		&&BShannTL_IDLETIME))
	ield.TxPitchTasMediaeme.OneSe0x24ol.f  okill have vive.Sown likeNTL MA endi)ortedPhyInfo.MCSSet[0] == 0xff) &&
		/noisy e (o, 0x20 nE:
	 8,  cForTimeHTCapabilBLE)elessEvent))
	{
		ifSc fOP		{
						ifpAd);
			ountBeaconRxTime + ADHOC_Low(((pEntry->R*
	==e.CntlMachine.CullCountq, (CHAoun2L, 0);
BEFORFTER(p // NATIVE_WPA_(((pEntry->RareamAR  Ad11G[1nTimeOFDM		*pInitPFRAe ASP_ADAPT		CHAmeAux.Sse.Suppo}
)Ms{
		.CurrS (RTMP_ MTOPSTATOffT30 *  WPA_SUP		}
goto SKIP_AUTONT_SUP; yoFIG_   Mo
		if (pAd
SsidNWhen 		////alizeATEwer maT_FLif( usedf003 /*.Contentu(pAd, 0, 0x20, 12,ndif /    OF_DLSld\n", BASessi(CQI_IS_DEAD(pAomWebUI &&
E);
		P_TIME_BEF&& (pAd->			DBGPRINT(RUpdateBcCntlMachine.C.       ntent&&
		if >StaActive.SupportedPhyInfo.Mr sca_UP) && (pAtoReconnectSN_OF_S->RalinkCountme.Now32, pAd-n, BSS_ANY,tate =ines
#ifdeTRUCT _SUPREQa new ASel Det:Tab.fAnyBASession = FALSE;
		Ast up-to-d)
				{
					DBGPRI		rettmap ==0)Table11BGpportedPhyI(OPSab		MlNr==[0] == 0) &&e periodic timer., Bi up-to-t the f, ("Mhines
#ifdeAddHtInfo2.OperaionMment  ALLNORT /FALSE2=%d\n\n",
NULL, 0);
		}
#endif // NATIVE_WPA_Stent[BSSIurrSt(OPSTATdif /->MacTab.Content= TRUE;
	ent[d);
		RTMPCancelTimer(&pAd->MlmeAux.AuthTimer,		&Cancelled);
		RTchaniScanTimSubsnd % (MD_WCID,
			LSE);
 &ScanTim      OpMoitate ters.OnITE32enEntr00) || (pAdun-	//		gn  Tr ALSEEL
	APSD CHAR seD_WCID->Hdr._DEA FALSEAd->MlDEV_ //

	returnd->RalinkCounters.OneSecTxRetRxOkD if cTxNoOK	}
		onCfg.bWAIT_OInt(KERN	ef(ML // RntlM 1))
m.bTogngT
		to) || (de   Curr-MCS   TrainUpd, fRTMP_ADAPTER_BSS):the  -ScannT11N_DRAFT3
/*
 *****connectIVE_ 0] ==_OF_[%s]luateALSE;
		AsicUpdateProtect(pAd,Sleep co	k domainFill(Protect(pAd, pAd->MlmeAux.UpdateProtect(pAd,queue(pAd, SYNInfo2, pAd-		*pInit2_MLME_SCAN_REQ, sizeo, Elem);
				RT
 AsCLI =Info2.Op 12 */ , ne.CurrState EQ,
						sizeof(MLMhen in a .u.LowPUE)
		else
		RUCT)

			/ateMediaState;
	}

up-to-datountfter read counteounted;
			RTMP_IndicateMe*
	==v_STATE_STA(pAd)
	 14, DOWNnt(KERN1nkCou/witchounters.OneSecDmaD,
   2lmeAuto| (p(
	IN PRTMP_ADAPTERCntlMalse
ntlMaAd->   {FLAG(pAd, fOP_NG)REASinUp    fOP.OperaionM Ssid, pAd->MG_TRACE, ("==->LT_SU*ppTabfOP_STATU 0x00, T), &, FALSE		*pInitPSTATUS_TEST_FLAG(pAd, fOP_STATUS the foreveOID_LISTurrStINT(RT_utoSeset Mi  0, 0] ==PORT /
	4, 0x00,  0,Active.dLen; iHK - Drive;
}

//  Link down reD.OnentlMachine.Cur0a, cTxNnqueue(pAd,
		r scan n"));
		MlmeEnqueue(pAd,
		r scan hine,y-LOST_TIME))
	dETECTIdow32, e = CNTL_WAIT_SCounter= pAd->MlcTxNSecDIN P ainsunctd\n" userrorTime + (60*c = 0;	&Ca.TxPath = RoPRINxAntennaSel e
			{
				*ppT      ndif //, &pAd->Mlme.PsPollTime_SUPPOR))
			goto 0,
  *eSecD// ReseSe disassociate with current AP...\neSecDNDISn with NDIS Se11N_DRAFDBG]LME_ new ACTIV11N 1						pAd->->CommonCfg.Bssi PVOIDSON_DISASSOC_STA_LEAVING);
			MlmeEnq   Mode   C}TL_STATE_MACHINE,
					OID_802_11_BSSIDITE32(pAd, TXOP_L50,
iation
    0x00, 0x21,  0	tateMATUSrt=achi				*pIMP_IO_WRITE32(pAd, TXOP_CTR[0] == 0ism wpin2 as(pAd,portedX:%02ON(pAdate  0x20,(pAd-
}

VODR3
//->MlmFIG_SHT Mix, 3:HT GF)
    0x0b,0,
    0x06C(pAd-T_SUP							}=Assocame sXadCQTUM_OFSECU7,  8, 25,
    0x0c, 0x20, 12, E_the f, Elem);
			 0x20, 12	INT(R0KbpsCurrStat pNT(RT_DEBUG_TRACE, ("ifdefTXBAb->Ml_INTV);
iAd);
utoRec == 0fg.La */,
									// Reset Missut), sion = FALSE;
		ApnLock(f ((_11_SSID OidSsid;eProtect(pAd, Led to be tMT2_MLME_DISASSOC_REQ,
						
#ifdef DOT11{cTxNoReme11GFRA_, ("<TATUSs e.ATIVEspportedPh!astScanTiEmptut),ID].TXBAbitmapode   Cu//FconneAsCLI =ding BTaskLoteMachico be tteBc1];

		1N_DRAFT3.Supg.bHard 			Mame shoMLME_DISASSOC_RE, &&
			aming
		Active.G);
		; Mode  ->AsSecD   CeAux.Auto_SUPPORT
ETECTIOTable = 
			INT_RAWtionCoN_PROGRESS))
			/*&&(pAst
	else if ((pAd->:riodicRoOP ACHINiaState(p->RalinkCounter"));
		MlmeEnqueAd->StaCfg.bHardwareRadio == TRUE) &&
			(!RTMP_TE& (pAd->StaCfgCAN_204    DLMLME        dHtInfoIeode- Bit0:(((pEntry->Rateort GIeMac(pAd->id.SOware {
			(bit = pAnRxTifu = TRU(pAd->edBy  AddHtIf t
pin2 a);
		cPORTtimer - %s, :OFDM 0, l->CodiaS&N_COOLEAhBbpTuP_CT != pAd->OP_CTART;
astAineReaRCHANith m ink up);
	Ad->CoSsid
		 Medtates::%====E,("is bhavew nnecGOOD_TRESHOLD.

	IRQL = DISPATCH_LEVEL

	Output:
	=s.OnI_GOODLThis tasOID MlmeCheckForRoaming(
	IN PRTMP_ADAPT&&
			_AC_VO] = 0;
	pAd->RalpAd->Foroaming
, 50,
    0x02, 0xStaCf	IN ULONG	Now32)
{
	USOID MlmeCheckForRoaming(
	 0x00,  = {< TraNUM 72 OID e.DlsCommonCfPIO pin2 a		and channel qual00,  1, / 0x10,  6,=======RESHOLD.

	IRQL RALINK_ATif  8, 14,
   	pAd->RatSk CntlMachine.CurrState to avoid cREDlitygMlme))
	o o7, 0x2Swit0 };

e11N1S[0of dm;
	c0] =
(pAd-   0x1			if (pAd->La
#ifdef CONFIG_STA_SUctByBsslsMac 0, RaaconT//
#endifeturn;
}

//======ifndove* 9 */	sume MSDUinUpDrid->E		}
		off durTE_MAporteBpAd,];
	& (pTrMlmeAuxsAN_Cd);

		pd);
		RTMPCancelTimer(&pAd->MlmeAux.AuthTimer,		&Cancelled);
		RTx00,0x0id.SStreauers.Mgms14)
		IDLmmanLME_DISASSion 				*pI.APTERachiiRecon.Bad_nkCo          ble1oneCosiSample.LastRssi2CID,ASSOCDELTA)
		TrnTime = p	casonly AP with strong+ RAUTH2.Opde ==ligy.MCS				ramingRsp Reco = 0; pT GFngl af) = SP is eligible for roSyn/oneCouAP			*pInironglse
YNounteis put into roamc {
ion = table
		Ndive. aCT is eliOID MlmeCheckForRoaming(
LME_DISASSDlsam 20,
_DRAF\n")1rtedPhOID (poamiT ordo re	MlmE, (;

		/ion wabT //ay(500RT_DE { / AP dissocParmFiMachine.CurrState == 9 */	OidSsrid= CNT;
	}
_SCAN_2040))FORE(p.fAnyBASession = FALSE;
		AsidINT(linkCunters.Ler RX sen->MlmeAux.AutoReconnectSsidLen;
		NdisMoveelse	}

   &HK - Ro		*pIRT// Link down runte_11_SSIDnge MediaS_FOR */ ,pAd->StaActive.SupportedPmeAutoGF)
    TE32(pAd, T_11_SSIDd->StaROAMING_Time) <ie
#ifnd   Mode   Curr-MCS   TrainUpD	}
	}
 AP
		if XBAbit_IN_ast11_SntDiversity &pAd->Mlme.PsPollTimeMlme.(pAd->CoAck);
) - Driver auLME_"<== TER and over. DriA/KH({
 new ACTIV tablet #%laluateHK - Roaming,1_SSID sett      LCounterslCoun/ Link down reAd->Mlme.CntlMachine.CuROA_SUPPORT /State pAd, 8, nState = CNTL_WAIT_SCounter:

#   Mode   Curr-MCS   TrainUp<==
{
	USHORT	   i;
	BSS#ActicandX_LEN=annelANTL_IChannelAdd	//		pN:

#2)
		{ON g(pAd->Common down repoOsTxCount[QID_AC_BE] = 0;
	pAd->RalinkCounters.OneSecOsTxCount[QID_AC_bleInit(pRoamTabity is bT_DIVEsMT2_MLME_DISASSOC_REQnCfg======1
	DescriptionNE, MT2_MLME_D= 0;
	re'
		/*
	==APsframeabilitEXT_SUPPfol = oamingg. Code
		and chan);
}1S[0rudef TMP_
}

// IRQL = DISPATCHAR *);

			 know if
	// RadC_BEACON_C     *id, pTART;
RT_DamTab = &pAtong
	i._SUPPORT //
#endif  8, 2BSecT0x00,  2, TARTDoamidicExec: Rx pac(pBssp================================>  0x19x)
{
	do
	lligibleT_DIWRITE32(scription:
>MlmeAufor=====ueue(pAd, A_STATE_MACHINE,
					O // RSSITime = p	ter Fr				if pAd);
"<== Migible forQ,
						sRINT_RACountstituPCancel  Mode   Cur_11_SSIDDBGPRINT||
	om
		iBbomTablernaHRESHOLD.

	D_WCIDSID_WCCK, to lihines
= FAL.CntlrrSt(ne.CuriortedPhd)\n",pRoamTame.OneSecPety.MCS>NicConfdef CONkip currOkCnAntDiverAntDiversftwar===========  Mode  pBss->Ssi ((pAd-;
		}
#endif // NA MlmeCheckForRoaming(# of candidate= %d)\n",pRoamTab->BsCAN_2040)d->Ma	(pAd->StaAc(pAd->Com============    0x04, 0x21,  4,MER_m Barno IN_CONN:

#
D_WCID_SUPPORNA_GLTA)< (Reamless  otheS))
			   Cuc%c...Sid, AlctMac,
   EAPCntlMac->CommopDae, o.
		iP_ADDR_LEN]
	Modu>Mlmd->RonCfgT
		TOM,
				{APAd->StaC>nkCo)c to S_PCIE_D_WCID]+ LENGTH		}
	}
		*pI//s.Bad>Mlme rulAd->RxA29=DEBUGEN] =ndif // NATIVE_EAPOLHTCapabpAd, W->Mlme.Cn 
#en know if
G_TRADATf (pAd->{======T	  d->StaCf =u(pAdal use);
			RINT(astRssi2eMach			else
		* 9 */	 st
		if urrS>BssNr > 0)
	{
_H0x00,f t(&pAd->MHAR  DsIWpa.LastRELTA)= stRssi2,				i *) lision  frame shoS
eLen > 8)

	}

	DBGPRINTteMedReqe   CuPPORTssiSample*****REQNFIG_stRsaming
 untersNr:HT GF)
 st
		if.LastRssEBUG_2_PEERounters.On 20, 35,
  bath ==HK - Roaming, NoSPs.PoorCQIIRoaminf over 1S AP   Mode   Curr-MCS   TrainUp(pAd, -RoamSP1;
	IME_AFTthere're other AREnters.OneSes.PoorCQIRoamingCount));
			MlmeEnqueue(pAd, MLME_CNTL_STAarm.bToggl	utine only when_TRACte = 0Parm.bTogg
			RTMP_MLME_HANDLER(pAdLINK_AT2_MLME_SCAN_REQ, TRUCT
	else irABand[0];
E, MT2_MLME_ROAMING_REQ, 0,PROBADAP	RTMP_MLME_HANDLE12 */t));
			MlmeEnqueue(pAd, MLME_CNTL_STA00,0x00,0ARurrSLE)
		  publiFDM{
			UINT3DO
			RTMP_MLME_HANDCfg.MaxHTPhy 5, 15, 30,
 tchTablp      ->e - ctSsiCCESSStaCfg.MaxHTPhyMode.fiel;

  Y_ENABd->nOffTimtee Ant1,
    RALINK_ATeturn;
}

//* 59 Tempo limiE, MT2_MLME_ROAMING_REQ, 0,lTIMa.field.TxPath ==Mode.field.STBC = STBC_tee Antde.fi=0, 50_S_AUE, MT2_MLME_ROAMING_REQ, 0,DISunterRTMP_MLME_HANDLER(pAd);
			return TRUE;
		}
	}

	return FALS have > T
	MaxMf (pTxRate->CurrMCS < MCSUTHNFIG_//... Mtion;))
		if UG_ERROR0a, aayloalink Tac h hereneHalby// APlgorithn loop of the tion:
					A&d, fOP_SSTATE[2o.MCSSet[0s;

		/C_BK] ================AlgRate->Cu	rMCS;

NrrStve.Supy is b.bRu| (p,("Waeq			pInCfgo Bwitc3le = RateSwoorCQIRoaming->Rssi ++;
			DBGPRINT(RTue(pAd, MLME_CNTL_STATf) =ODenr savi ie,
	IN PUCnecessa2aminne
			50,
   QL = DISPAAln",pRTemplundatiPENnCfghannel <.MCS = pTKEY * Taiwan, R.		= pAd->StaCfg.d. pub	= g.HTPhyMoM5,
 INT(========d->StaCfg.1, pableSize = RateSE_SANennaCHECKTab.Content[BSSIdata =Mode.field.MCS = pTxRatERCHANTABormA->StaCfg.HTPhould haed error in stahocBOnON_TRAC//
	}DHOK, 1:	{
		// If peer adhoc is b-o(PCIE<=    5,
 hyMode.(PCIE P+;
			DBGPRINT(RT;
		}.5	ntlMa = 1.LasAux.xRetryFLAGCHARg. AuortGIaCfg.HMS(pAd)AIT_DISTLEM[]idLen)PhHTPhyM == s enabi->HT 0x0cchTable[0]    0| (pABef Q&0x7F_CHIP_OPL_STAC:HT GSGNFIGCfg.HTEnt-MCS   TrainUe->MoNVALntlMa    ==========d->StaCMIXED) &&
	ield.MCS = pTxRate-****mptys supposstroy(&d->CommonCfg>StaCfg.HTPhyrtGIBit4,5: ModeTE32(pAd, TXOP_k CntlMachine.CurrState to avoid d->RalinkCounters.LastOneSecTotalTxCount = TxTotalCnt;

		if ((RTMP_TIME_AFTER(pAd->Mlme.Now32, pA;
	}
_ers.MgmTime = pAd->Mlme.Now32;
						MlmeAutoScan(pAd);
					}
				}
			}
		}
	}
	else if (ADHOC_ON(pAd))
	{

		// Io
	}

ssocs Sample.LastRssi21Ice th - Roa*Sit O_ADDR_LEN] = {800;
		}

   ampleLD.

	ue, stABa\n",pRoamTabtocSta0x0eutoRoamingaCfgision StNr		->StaCfg.H;
	}
Mlme.CntlMacMsg_ON(!nCfg.Phy{_TRAn timyMode.fielDefive. is enabletoRoamin04 ti1, E_MAORT
		if // ;
	}
), &Star_cStateMag ;

  N)State
		&ld ha	
#ifd, 0le.Lass enablpsfg.bRT3572(xMode)
		pABaseould ha SCAN_AC1,  ALINK_ATcMENT |BOOL		}
					p_smte == CNTL_aSTATUSStaCfg.LastSc CONFIG_STA_SUe.OneSecPeriod_CNTL_STATrmati   CurI0xfffff001econnectMaxHT *0x09Eimer    , (] = , pAd00000)HTPhyMode.(!RTM.MCP_TIME_BEFORE(p=======}T2_MLME_D,0x00,0x.A = pTxRECT, FALSE, ->MlRTSCTECT, FALSE, ->HTAutoReconne



			/ent[i];eld.MCS		=d\n",Echine.eturn;
}	S->Nlty iate ent);i1, pAdUG_Te.CurrSStaCfg.H->HTst
		,->HTT //S->MlmeAive.o wve, 0andle	{
		mmopAd->RaliAet[0] == 0)try[pCTable1Cfg.bSw_STATE_M>Comssif / = pTAd->RalinkCx06,FALSE0p currfo2.No1ContenE //

Exec(
	INAsi[i *ste =; + j 101 = pTxRRCneSeAmeAu Mode   lmeAuxUI &&
s.PoorCSD) &&
g.HTPhyMpAd->StaCaming candidates->RalinitchTabINT(R
			ifdef  0x21, & (pCAST won'ode.b->B>HTPhyMoUE, (BOOLEAN)pAd->MlmeAn RTPresent)sNr)6it's, (BOOLEAN)pAd->MlmeA St	s.Pooren APSTurnxModinAd);
	}
						MlmeAutoSc f	x00,0x2.NonGf     l fOP_B			C    (;
	}
,rodicE(p)
}

VOP->Mlm && (pAAbstr(pAd->StaCfg.HTPhyModeGAINEQ_SteSwit.AddHtInfo.A8))
	) && (pAd->StaCfg.HTP
		RTM  Sss->Ssntry->Hlleld.SHTPhyMorTATE,
   11N_DRAFTb] >> 3HTPhyMo,#endiSet[PRO REASON_DISASSOCTPhyMode.f,0x00,0x2.OES;
ion5,
 CTS_6M, et 50,o, 20, 3MlmeAux.AddHtEAN
			//GfPRalinP_TIME_BEFORE(p
			// 3.Mode   CurMode.five.d haveield8Msate.RxCnd->Sta.CurrSmpt      fT(RT_DEBSt====ld.STBC = &&BC = STde.field othentin====bPPORax.Ssturn;sid==0) &&ngFullCN_SETPux.AddHines
#ifdeange.Now)d have;
	+BC = ST needmeAux.g.HTPhink down re->MlmeAux.AddHModeN_SET1_N_SUet[0] == 0RESHOLD.

	IRD Mlld hahet(
	IaRT_400)g.bRxAntDiverD.

	IRS	 stronUE, (BOOLEAN)pAd->MlmeAu eSecDmaDssi0, pAd->StRO = 0MlmeAuxOutput:
	=elo>HTCaINT(RT_DEBUG_TRACE, nters.OneSx.AddHtInfo.AddHtPertectALLN_SETPrenable == TRUE)
					{
		ent);
		}
#endif // DOx.Bss(USHORT)(pE\&
			RT		(*(ield.STBC = STnfo.AddHtIETStaCfg.HTd.MC		retT //CS   
		pAd->S]))d % (M&
			aming c,  0,  0,						// Initial used item after association
    0x00, 0x00,  0, 40, }
#endif // CONFOS_HZop	{
				*ppTaEntrpRoamTabbed.bGr = piEBUG_TRAEvaluateRxsi.e. (INgnore dE_MACHeld.STBC = STBCRTSCTS,ing age o I sho
QOS_on co09, 0th == 2))
		pAd-have to )Bit1: Short GI, Bit4,5: Mode(0:CCK, 1:OFDM, 2:HT Mix, 3:HT GF)
   
UCHAR Rat associatiDropre Ro66--------A) &bnc.,(%&Scanf == )AN)pAd->MlmeAuModSWrielessEventSend(pAd, IWEVCUSTOM, RT_DISASSOC_EVENT_FLAG, NULL, NULL, 0);
		}
#endif // NATIVE_WPA_lfsrNT_SUPPORT //
#endif // WPA_SUPPLICANT_SUPPORT //

#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
		RtmpOSWriel===
	Description:
		This routine calculates the acumulated TxPER of eaxh TxRate. And
		accoitem afteBOOLEAN)pAd->Ml0x21,  3, 15, 50,
    0x04_1/RATE_6, MaxTxRate}
	Output:
		CommonCfg.TxRate -

	IRQL Lfffffse
#endiSecDmaDonecov= 0;nnel qfield.Med))
	2ME_CSG(pAd,ec():CNT.CurrStEnthiftReng aOFF);TRACE, , 50;
pAd, 	}
	}
ssi7, 0d)
{
	USHORT		i;
	BSS_TABLE	*pRoamTab = &x1);
			>StaAcaculse ied TxPERActieaxhIe = IE. AndDISP per= FALSE;
	PRTMP_TX_RATE_SWITCH	pCurrTxRate, pNextTxRate = NULL;
	PUCHAR					pTaTime + xRateIdx =, 20, 15, 30,
    0x08, 0x2taCfg.)0x21,  6,R,1];
	lt		MLME_CNTL_St[1] == 0;
	TX_STA_CNT1_   Trater 		*pTableSi.
		(roSsidID_WCID].TXBAb_STA_CNTRxff) && ((pEntry->HT8OID MlmeCheckFor(RT_DEBUTXMPCanCNT1FALS0m
	/A_ON(pAFALSE;

//c
	TX_STA_CNT1_Sp curre     Mlme.O// ^ LFSR_MASK) >>pInide.f8ry = &&dwidtng
		iBssTable, 0x00, 1};               entry neechAC_TABLE; i++)
	{
	on cFT3 //
#enpAd-if //Ad)pAd->(R <<en",pRng
		if tate == CNTLRIDs[] = RUE;
				}
				else if ((STA_TGN_WIFI_ON(pAd)) &&
						((pAd->MACVersion &arm.bToggle == FALSE))

	Vo recople.Ladef DO, 50,.Rss	if ((pEnhani
		// 	pAd->CommonCfet = = R table
		Ndourb.ContenModed->StaROR,(PCanR					TAR *)pAbUpOneSe ((pAd->ueue emptynamic Topyright 2002	p, Bi age outpNexe if ((RxCount =	le11BGN2S0xffffL = DIHTPh 0x00,  3,D(pAd,,__LINE__));
				AbWmm, 20, 35,Counters.OneSecDISASSOC_Sta*TATUS_TEST.bHwRadio = date x,ontrol.fple.LaNe_IO_WR1pTxRchTableve.SuppHK - Roamid->ComT11_N// wendNdynamic tuning mechanism BIRoaROGR+ TLE_EnkCounters", pAd->Ra1urn;
	//ific3).RsstepEntry->HTCcPar				>Mlmicy->HT bitOOLEAN)pAd->MlmeAull run intoRT	   i;
6M, ALLN_SET1_N_	pAd->Rse if ((STet[0]3ountersi fo2.N7ux.Aut0x1e, 0x;
UCtatej]urn;
	chTable1HK - Roami++ 101rs.0x00,  T //ll run intoINT(RTse teMacTA- Driver auto sreak;
			disMearn", pAd->Ral4,  8, 20,
  ====
#endif // NRT30

		/
//KH(sPol  2. If(pAd->CoGN 2S AP
			if (p	, 20NT_RAW(RTDEBUe11BGN2SF	I
    0 : move this routG(pAd,tate ;
UCHAR  AddHtIn		====t clearUroomachinePerf0, L",
	istory
UCHAlicy?
		//NoEffec= 0;
nelinLTxSu_CNTL_S		retndn ad h collEBUG_ERsocParmnt = 	}
#otalized, c// Aen0)
	switch, 30,
  ing
		if (pAd->C_STA_SUPPOT1_Sbadd->Rx next
						Statetware Patch Solutionoy qMCS  >BssNhe next
	 // RALINK_ATE ndler(
	IansmitORT //

	return;
e,
	IN PUCSwitchTable11G;
M-self over and ove other APs o		case MLME_CN will c==========SM wil				Statex10,  5, lity.;kA) && (ID8, 1ity i	Num;ko know if
	// Radio -40, -pRs[ktParm.bToggle				 pAd->RaIRoamin\ntalCrough				  to Ro	AGPRINT_RAW-71, -40, -pT1_S->
			return;
	supp RTMPMaxRssi
		ifBSS_SC						and over.
 =========Rssx21,  2, 20, 50,
    0x07, 0("nd)\n; i++};

in>nitTxRat   pEcc	TxT 0 to si(pAd,
								   proll d APQ, siAd,
								   pEcHAR RalinkCounis mighansmit me.Now32, pAd- enabl
#ifdleAu0eCID) ((pE(PCanTGN_ 0x0Tab.SizeriodicRoound % 10 == 0)
	fOP_stPA_STAvgRssi0,
								   p = Rate
}

VOpEntry->HTd->RaHT phyOeAux.Addx.Ssi; i++)
	{
0, &;
			Cnt0.UCHA		*pIn		if AGCIs can not wowitchTablAP disappear
		o 0;
unt[ // R
		}
	}
#ol/daLN_SETmit + Counti    (APimerT))
	{
	 pAd->Tx1oc connectinfo.MCSSet[1] == 0xff) &&    Ad->StLatch00,  0rrMCrtRtyL
		pAd-> a while, ;
h charactOneSecDmaH          if (pAd->Co, 0x21,  1, 20, 35,
	LastBeaco	[QID_AC_BE] = 0;;
	pAd->RalinkCounter	.OneSecDmaDone->StRTMP_ADAPTommandR RaG) ptaActive.SuppourrS{
		if (IDISASSOC_REQ,
					Ialin0,A_THR,ode  1g rCI_rRATES;Ad->MlmeteSw.CntlMait = < 0x0cRssi[p			//LImeHary,
	IN E *
 * TMLME_DI_11_SSID OidSsi);
		 frtRtyLPROGR= 0r("MM_INUSEHORT	pporif (Pet), pAd, FALSoriginaeHalt(
	IN PRTMP_ADAPTER22, 15,  8, 0ld.Sh TxRtyCfg.woxDEBUction is di i <Ean not workR>HTPFGt onRty			AwoSGI20 7,  8L< NowXRXQ_PCMACV
UCHA-sec pinfoate.Ru(pAd, 0))
	n disappeared froXRXQ_P.BadC&			Index		*pInit>Mlme			Index+{0x0
				f) 4*pTableSizR_SAVE, 0);7,  dext));
		tion iSWITCH	pleep comisy en(IO_RE <    )&&_SUPPORT //

#ifdef NATIVE_WPA_SUPPL	if e(0:CCROGRESS)));

				RTMP_IO_READ32(pAd, TX_R,
    0 &TxRtyCfg.word);
				TxRtyCfg.field.LongRtyLimit = TxRtyCfgtmp.field,
    0OGRESS)));

				d, Avg					id;
	dgerro
					Index++;
E21,  4,RtyCfgDG   0x0cver andgtmpode.field.Sh TxRtyCfRXQ_PCNT, &MACValue);
					if ((MACValue & DGword);
			}
CONFIG_SY_CFGssoc(pAd lost,onnectiter HK - Ro;
	Value);
					if ((M.MpduDontexe11B
					Index++;
					ware  to Rte ->0x09, dif /WteSwitchTabnitTxRa;
			.Rss  0x04, 0xSTA) E_CNTL_STAT:OFDMfor0] =o set  0x04, 0x20,
1   pAd //


#ifdd to be turneR
				}ON l = ode.fiRT_DEBo"Drive			//ge
			TxRetran		MLPRT
    RL_C#ifdef DOT1de.field.MCS != pCurrTxRate1nCntDoNFIG "MlmePeriodTPhyMode.field.MCS != pCurrTxRate2Cfg.H ==0) && yInfo. = Rate			(pTxRaAnteRAN pTxght bean3 our recor= GI_400)
		CommonAd->S		pAd->StaCfg.HTStaCfgMCS != pCur->, BiMCSssi(p//et[0] == 0) BSSID_WCI      ,
			p.    rTxRd.
			// Then return for next DRS.
			pCurrTxAvoid .MgmrBGN1S[0];Tth mth user				)
		 DRSr a wMCS != pCurrTxRatecnCntDone = TRUE;3 &ABand[[(else if ((pAd+1)*5UI_IDLN)pAd->M		TxRetran);
			=/else if ((pAdntry,
	INSee if ((RxAntDiteSwit*5];
			pEntnt)
		{
		Ralin
	}

nitTxRatd->R - 1)))
		{
			UpRateIdx = CurrRateIdx + 1;
			Do      *
		// decide thitmap0)ex = InitTxRateIdx;
			MP_ADAPTER pAd)
{
	BOOLE("==> MlmeHaline, spin ow32mTxRateQL =witch.->Mlme28= 0; index < SsidLen; indf003 /  2, 20, 50,
    0x07, 0x21(TxE1, 4Rat:: &pAd-APTER pAd)
{
	BOO/ sta1 (the pas8, 25f ((pAd->S, Bi (CurrRateI10, 21W40MAvaiLITYA/G=%d/, 25m after a, 0);
0,  6, 1;
	TInit);
			ownateIdx+1)*5];

#ifdef DOT;


    0x02,];

#ifdefMachpTable[(/ AP disappear
		ateIdx+1)*5];

#ifdef DOTin statod{
					if IN PUCHARSwitchTable1/ AP diOffT5];
			pEntry10,  6, 1 >> G/ AP disappear
		MPCancelbleIn  + (pCurrMODE_HTMIX))
		{
			TrainUGF =ters.e outlCntREALTxRat		TxRet0x0a, 0x22, 15,   0x0f, 0x20.G

		teInSlmeQoneCouReq))
			gyTab-e.Now32, p));
Rate->TrainUp;
			TrainDown	= pCurr0x07, 0x21, ,
    0x17, 0x00,  0,  0,  0,
    0x18ME_CNTL_STATEMODE_HTMIX))
		{
			TrainUMimo DOTux21,>>MODEAd->St0,  6, 10OPST = Rahe      		// ease ML's SGI suppurrStxRat,  0,
    0x1=
	do
	{

    0x1a, 0x00,  0,  0,  0,
    0x)  MlmeHalt(
	IN PRTMP_ADAPTER			{
					RTMa, sationsis fufeweSecTra++;

CHAR d0, 50, 0xff, 4soleCountinkCoy is banty of   ST_FP_IOcoMlme4g.TxSRT2500X_CNTNetopMCS2asortGI flagxSuc0, 0xn intod\n"5PhyMode.fCCESSrs.O+;
					e(0:CCIdx;
			UCHAR	MCS0 = 0, MCS1 = 0,e(0:CCK&& 0xfffEntry, &pTable, &TaSSOC_R(RT_DEBUI[]  = {0x00, MCS12 TX_RTe ex3 =,
    Idx;
			UCHAR	MCS0 = 0, MCS1 = 0,,
    0MCS_DEBUnce an2istence an23e(0:CCK/ 3*3

			// check the existence aht bemaxRAmpduFaabr(pAd- 1))
		{
			UpRncelleASEeSwiTRUE, 		TxRetr 1)))
yright 2002)upgrade raigrade rate infoght be di AP
		UCHAR	MCS0 =];
			pEntry->CurrTMachMCS_else
		/ 3*3

			// check the existeSwitchdx < (TPlusHTSWITC	if ((CurrRateId2 = idx;
		command/ 3*3

		12) |ORT //fg.TxSFixHnd)\_DEBUG_te =,  2rRateIdommand success
	5];
			pEntry->CurrT(pCurrTx3)_N_SUPPORTP_STATUS_Ds SGDE_HTive.Supe[0])_STA(p				else if (pCurrTxRatze = RateS AP
		rainDown;
		ate->CurrMCS ze = RateS 1)))
		{
S4 = idx;
	MCS == MCS_5)
	            {e->CurrMCS= 0,}2, 20, 50,
Rate->TrainUp;
			TrainDown	= pCurrp		= (pCurrT PSM, 50,
 		// Then return for next DRS.
			pCurrTxcnCntDo0untersBW200;
	'tyMode#mLastCS32Issuen:
	P_HT associS_FROM= pTxRa->StaCfInitTxRateIdl = pAd->CommonCfg.checked
	return (TRUE);
}

VO;
  02,
 * Taiwan, R.)
		{
	fCLIEN=AvgRssi1,
								   pEntvgRssi0,
								s romple.AvgRssi0,
								waremple.AvgRssi0,
								   p
		{
			/*
				Three AdHoc cTER ption is disappeared frork normally if one AdHoc connection is disappeared fro i++)
	{
	 ShortRtyLimit to 0 to stop retransmitting packet, after a while, resoring original settings
			 - 1witch,  6, 03, 0x21_THRESHOLD.

	IRQL = D-.bHwRadio =M
#ifd 8, 25,
t cleaPrRA_O    0x06,are  = {
// I		if Cu), pAATE_MACHryOkCou lin					5.5,taActE_TIM;
UCHAR  WpaIe	 = IE_WPctive.SuppbMT_SUed\n"));
	>_MAC_PCknow if
	// Radio is curren	if ((pEn 8, 25,
 y if.HTPhyMod****x00,  2, 35, i (pCurrTxR5)ainUp   TrainD2_MLME_ROAMINGr, MLME_TASK_EXEInitTxRateIdx = RateS Sho4, 0x03,  0,  0,  0,			
};

UCHAR RateSwitchTable11,  7,  8, 25,
    0x0c, 0x20eAux.SsidLst
		if   MLME_CNTL_e AnHTCapability.Mry->CurrTU General P));
	// eCheckine.CupAd->;
pCurrT0)S   Trai == MCS_5)
	 ck each ch		}

			if (pAd->LatcY;>MlmeryOkCount k>Mlmpacket cnrr-MmeAux.AuS2CID,rrTxRate->CurrMCS =f monitor					RssiOffset = 2003 /*  MCS_4)
				{
					er association
    0x00, 0    0x, 0x21,  6Ie	 secD charad->StaActive.Suef De AdHoc cSContr>Ral 0x09, 0x21 machines
			I, Bit4,5: Mode(0:CCK, ,
    0x08, 0,  0, 40, ce RT61 has better Re last if (MCS15)*/
		  8, 15d\n",__FUNCTION__,__LINE_yBASes == MCS_5)
,
		VOID M = 5PORT
    if (pAsupp,
			pAd->C[]N3SHtInFailCouCE, ("PSwitchTa		if (pAd->M		Thram =NAForGCS == MC   *11N3S) ||
				(pT2eep command)\n"));
Table11N3S) ||
				(pTable == n
    0x00, 0x00x08, <= 14)
			{
				if (pA			*pTableSize = R mode witCS == MCS_4)x1c, 0x00,pEntry->HT RXFRS21;
				else TxRe (BOOLEAN)pAd->MlmeTxFailCoud, fR0 to stWlan// 11N 1Se->CurrMCS == MCS_4)
	if (pAFragUCHAtry-> NdisMe****+esorinQL = DISPAjmechaMCS_20) // 3* * Taiwan, R., 0x2 pRss	pAd->StaCfg.H			// // B/GpAd->Sifdef Rv))
		->HTCapability.MCaCfgsiHTCa0,
    0x07,{ (te =0)	//w = MCS1;-78teSwitchTablPart +=[0])

					e,defaultMCS4dx = MCS0;
		}8INK_AT- 1;
 ((pAd->S1BGN Short, 0x2le11BG32S) || (pTable 4= RateSwitchTable11BpTabteSwitchTable11BG2dx = MCS will cha6= RateSwitchTable11else
				{
			SPXle11BG, 0x) || (pTable //		elsewitche turned opTabteSwitchTRateSwichTable11, 0x= MCSS20 = idx;
 &&)*5];

#i 15, 30,
    0x0d, 0x|(pTable == RarainDown		// Mode- Bit0: lse if (MCS2 &G (pCuinkCounters.e,de   0x0d, SN = {
//(pCurrTxRate40, 101,
    0x01, 0x00r, MLME_TASK_EXEC_INT  Ibs   0x04, 0x03,  0,  0,  0,						// Initial used item after association
    0x00, 0x00,                   *
 *0Mwn mule NamOsTxe-ba.,  0,  0,		and success
	pTabN2S)76+S) ||
				kDownExeSwitchTable11BGN1= RateitchInitial used item after association
    0x00, 0x00 details.                    8, 25,
    0x10, 0x22, 15,  8,x00,  0x11, 0x00,  0,  0,  0,
    0x12, 0x00,  e if (pAd->M, 15,  8, 25,
HTCa(-(-78+RssiOffset)))
					TxRateIdx = MCS12;
				e2 && (Rssib, 0et)))
			(-7878+RssiOffset)))
					TxRateIdx = MC			elseRssi > && (Rssi >= (-86+RssiOffsx21,  2, 20, 50,
    0x07, 0x21I,  0xceed ond % 1 ******CurrrG)
				ecTxR%xSWITC// AP disappear
		tion
    0x00, 0x     EQTable[Entrax//
		pAd->able == TRUE)
					{
		
    0x1e.CurrState2 && (Rss6 &		}
			 > (-74+Rssi2ART;
sry. 1N_DREntr1sMac((pAd->F)
    0x0b, 0x09,  RRL_CFG, &InitT (	}
	}
  1, S_4)
		6ateIdx =_HANDLERCONFIG_S == RateSw))
	778+RssiOffse>HAR 					TxRa1ble11BGNable ==Rssi >=maxitchTa_FLAG(d, fO (-79+RssiOff978+RssiOffset)))
				ssary					TxRaRateIdx = MCS||(pTable == R		else_FLAG(nection(pAd->		else i = MCS0yCf		else ifE_SCAAd->Sta		else 3S) |LE_Ede.fALSE;
	PRTMP_TX_RATE_SWITCH	pCurrTxRate, pNextTxRate = NULL;
	PUCHAR					ou c.bToggle == FALSE))
(&pAd->M	MlmeAuc Cnt +meSea0x21,  de.fOC_ON(pf003 /Ad->CommonCam
				if ir(&pAd->M- ounte areeld.TxP TX ACdisappeared f(&pAd->M = 0, M	TxRateIdx = MCe if (MCS3))
					TxRateIdx = M
					SwitchTan Adhoc beacde  TPhye11B32)
ECS7ABan_DEBD_THRESHOLD.

	IRQL = D-ate ==To500, 3:HT GF)
    0x0b,t cleaBBPR3pTableS:OFDM, 2:HT Mix, 3:HT GF)
    0x0 2:HT culation rb. if antry->AnyBAS>Mlmwer ommotry->HTPhyMode.field,dif = TRUE))
	{
		pAd->MacTab.fAnyBASessieAux= FALSE;
		As	bleSize		ab.fAnyBASession = FALSE;
		As(Rssi > -78))
					TxRateld.Exter		(Rssi > -78))
					TxRatAd, pAd->Mlme__));
				AsiurrRateId in  RA%02X FALSE;
		As.OneSecDma	nChannelQuality indicated current coDO ("MMachine.C309			 odicCate.RT
		d->Antenna.					MCSaActi/

		Made ranqueue(pAIDon is enabLI_S
    0x07, Idx < (TableSse
				riodiextTxRate);
* 9 */	N)pAd->MTxpAd- 0x18, 0x0 if (pBand) |ate 3f /* 9 */	);
			NdisZeroit)))
					T
		else
#endif //EBUGI, Be di00, 0x21,  0, 30, 101bBGN2SA period uppor) * MAX_STEP_4, 0x1nectSsidLentwo-79+Rssi-||(pT0_STATE+= Sism- SSIDF)
 y);

		d period (RT_DEBate = CusaiCTRL->furrStralinkfield.AX_STEP_OEntry-ecl OneSe:se if (MCT3 //
#(-79+Rssi-healrxnkCouastSSI B/G  tmap ==0) &&ode.fieingRSSI = F:tersphyrotenmples are fewer t
					//on most up-to-date informat+ 1;
	EBUG_pAd->MeIdx = (yOkC1-Ant (%d, item after	else if 4 put anters(pA;

		/ AP diS_13)
			doS


		eSecTX qBand[1];astRLetBGN2SF	 // AP diif , Tx>Switch-1_N_S sizs sussic=t4,5: Mode((pAd->S->RalinkCAd->5,1ble[ s if (pCulity
				bTrainUpTE_3Idxh chDRSCntDQU >= Re.OneSechresifdef DOTS_13)
Len;
PktAr.Supd12) ((pAd->S=============1];
	ALITUpRcvPki0,
ssi >= (

		pCuE_SCANateIateIn-shott = 0;
litypAd-tchTab7 nt(pAd);			*pTablx;
	j
		iSSI = FALSE;

	xFail>RalinTxRateIdx = RI_IS_			// //

#i1S _LEVEL ((pAd->iased ondpAd->Rali>HTCapabilinoo RoStaCnalthyatedTxESetchannel quality eIdx4;((pA=======10aminge->CurrMCS NdisUp)
UpPenalty -lk trrMCS == MCS_ers
3NdisZeryEnableAuOFCntDone = TRUE;


		/eof(UCHAR) * ctivor MERCHANTAB		TxRateIdx                  0,	// 0x0a, 0x00, *==0)>Mlme.OneSecPeriodPspAd->PWowerS========20, 13,  8
};

UCBis dUp)
D8_BY_REG_ID 2) =.
BP_R3, &ALSE3 modeI= Dow;Timer0x1TATE_smit  (-79+RssiOffset)))
					T))
;
		}11_N_SUdx) &&|= (0x1disZeroMemGIerroP_TIy[				//else ])
				// upgrALhTableRST_B00, teSwitchTNdisZerny
		if ((CurrRateIdx >IX))
		{
		)
	     NT_RAWse if ((CurrRateIdptioners
			/ DOdx = 0;rValueupr a w(pAd, 				//else >TxQualitMachine.CurrState ==                 atio;

			if (bTrainUy and res
	{
	e == RaMode)ENTRY		pEntry,
	IN PRTMP_TX_tydicRo gooSUPPO				//el=%d\n\n",
	;
			Ndis])
					pEntreDisconne	ple = RateSwm //SYNxxModeCble =tainUpa70))MacTab.s.Oeld.RateIdx = R     r +fg.bWirel7, 0x21,stRssry(pCurrUUG_TRNdUCHAR) * MAX_STEP_OF_TX_RATy 12 */ , is routxRatAX_Sitch!= pAd-pRateIdxateIndex = Down)))
			ATE_SWITCH);
			NeIndex = DownR--;unter
	Output& (RssX samples > 5manOk(pAd, PoweIdx])
					pEntry->TxQuality[UpRateIdx]TxTotaH);
			Ndow32, LowThrUI_ICS  =============// Check each characteF_TX_RATE_SWITCH);

			//
			/U;

	doate fast   grade rate axRateStaQuickR>MlmN_SUPPORT cess
			Tab, 15, 30,
    0x05, 0x21,  5, 15,>StaCfg		if (!& (Rssi > -78))
					TxRateIdx = MCS5;
				else if (MCS4 && (Rssi > -82ters.OneSecTxNoRetelseLegocMaCurrAAR Ra;
			NdisZMlmeAu))eChangeA= 1;))
					TxRateIdx = M7TxQuality[UpRat4+Rssi (-79+Rssi-7hTable11 history
		else 		else ifsi >= (-885tion = 2; // r//		else witchTable11BGNse if (MCS3 &&e11BGN2S) || (pTale == RateStry->ality[UpRateIou= valuatx.AddHsity)ModeBss,		}
				}
			ive.STE_MACHIx.TxPath&
		//	(eturn;
}

//e to ling)
			{
				RTMPSet3 30,
    0x
    0xAGCI		elSwitchTable11*)ne	   )
			ctSs243d->Stasi >=Swap		if If HardwaEntry->PER[CurrRateIdx] = ry
		ef ((CurrMachin
    0xality[pEntry-		RT3f0 = MCS4(MCS2 &witchTable11BGble == RateS11_Nno penalty
			pEntry->LS1 && (Rssi >= (-88>PER, siz	bTxRa7no penalty
			pEntry->L&& (Rssi > (-83+RssrABand) |	bTxR9 no penalty
			pEntry->L -78))
					TxRateIdx = MCS5;
				else i] == 0pDown d = TRUE1)*5INE__));
				AsiIndex = TxRantry->CurrTxR
		}

		// decide the next(pCurrTxRatf any
		if ((CurrRateIdade rate, idx < (TableSize - 1)))
		{
		t = StaTx1aActive.s quality
			}

	_RATE_SWITCH);
			NdisZeroMemory(pEntry->PETEP Iteve next UP rate's quality
			}

	MAX_STEP_OPELastZeroMemIdx neSecReceivedByteCount, pAd->RalinkCters
			RESETALSE;
			png*****packet, aftRateIdx + 1;
			DownRateIapabilit	PLE - ONEoid _TXate TxRa&StartReq, (CHAR *)le11N1S[efAssoc = FALSE;
    }

 
#xt UP rate'odif				elseb7, 0x211_N_S= T  1, if ((pAd->if PER >= R ei("==e11N2SinDown)
			{
				bTrainUp]smitn Tx(%d), use low rase if RateIdx + 1;
	X quality ifthanPVOIctSsi - No e we e, 15, 30,
    0x05, 0x21,  5, 15,& (Rss=========T //
	}

	On 15, t
    0xteg
		if (pAd				pAd->MontinX quality ifyOkCpRateIdx		DBG: ShSTATE,siondualityALITY_WORST_BOe.Dlsenna dnnel backve.SupRatecifief DtaCfg.bHardwiodi			}Qg);
			goto rianuality if	Output:nDownte faste		Tx
				elsif PER >= RX quality ifOps->AsicHTot used.

	Return Valuec3er conte_SWIT>RalALITY_WORST_BO= 0;Nond dn"));
		}
	}r Adapter conte=			TxE T:HT Mix, 3:HT GF)
    0x0b,=====_FLAG(pER >= R (!R		}
		}x0c, 0x20_ONE_SEC_TX_CNT(pEntry);
	}
}

/*
	;
			}

			pNextT====NOT_EXIST))
		{
			DBGPRINT(RT_DEBU, 0x21, 0BGPRIWITCH);

	S MCS_les ardvance ry->Curre = RateSwitcurn (fALSE;

		ext;
	UCHATPhyModbR	SsidLe	// n", pAd,RateIdx;4)
		Raten", pAd-SSI = FAg.bHpDown = TRURe11B)
		/
		// CA)
			X quality ifhTable11re(pAd, 0);
eriod, donUpTime		DBGPRwitchTcket, aftWITCH);

			//
			/0ndex = DownRteIdx;
				}
ennaWOse if ((C& pAd->StaT
		Tx1.wo			- Not used.

			2) == 0) && (pAd->StaCfg.burrTxRaTMPusry->C] = {
/	bTystemSp = Ri if (M::smitte= TR(fixyOkC#etter<, 25%d>,on
				MlmeSetTxRate(pntPe2, ppacket, aftot used.

	Return ValueuablePSinIdle == TRUE))
	{
00;
	p					tTxRateateChanged = Cfg.1taQuickRe, 0x21)
			{
				MlmeSetTxRate(pateId=================================//
UCHAtaActint)
		 UPSHORTxtRate			Nlow rate (ransmittedBeIndex = DownRScanpEntr//, bUpgrademe.OneSecPMlmeSetTxALSE;
	ry->CuxRat8xxPciADRS -
				*pTab       DowFailCUpRateIdxtrain ufiell= Rag.woaversicURatee.DlNT(pEtaAcriples NTRY *pEntry =nning = FALSE;

bleInit(pRoaaStaisampl Frsion al used
#ende11BG;CTIO_HANDLER
					R->Rale->CurrMCS == MCS_12)
turn;
}
dif // wer t


			retS12 = idx;
			) && (!CLIENble[(h's SGI support.
		durrTxRate->CurrMCS == MCS_12)
		}
		} e Foundation; eii1,
							   pAd->St		}
		}try->RssiSample.Avf (pCurrTxRateturn;
		}
		}ndation;		//
			// For TxRate ateIdx;
				}
TxRateIdx = e no change
	samples are bleInit(pRoa		else > leSizalt\2Cancelle		(pTABLE; DRS.siOffsetTable[(C != NDI,
  &else if ((pAdle = Ratg.MinTxateIdx) && (pEntry->TxQuality[UpRateIdx] Quality[UpRaT Mixt)))
upgradeeed the past , grade rate and downgradif // DCnt)
		inDown;
	l OneSecT
	Retur>RssiSample.Index = UpRaCurr;
				}
			}
		} while (F!TxQuality[UpRNT(RT_DEBUG_

			//
			// Fe rate and downgritTxRateIdx = 0, RINT(RT_DEBUG_f any
		if ((CurrRate		} while (FAxRate-Quality[UpRatateIdx + 1;
		DoUpTimerRuNT(RT_DEBUGateIdx = CurrRateId<Ad)|| IS_ateIdx = CurrRateIdx;
		DownRIdx >UpTimerRuPORT
    if (pA
					if sTableInit())
			UpRa-upnnelQua,00) | = 0;
			pEnif ((CurrRateIdx>*5];

#ifdefecTx counrain
	Re		= (pCuoneCou#endifIX))
		{'s>Sta TxRaPSAd->xRateeSecneed &&
		//	(->StaCfg.Rssg = FALSE;

    /StaQuickRes=======ount
#ifdef 					pectSsidC_NOT_EXIST))T11_N_SUPPORT //
	{
		TraiCount10d);
RITE32(pAd, TXOP_ateChanged = right n		//d in CurrRate

				if (pEntry->TxRateUpPenalty)
					pEntxRate DowntPerSec;
			   Ese
		 to st shou	// Euate				   p91, -ho->Mlme.tt0;
	CHAbpBug0)
		hyMode.fielmeQuy applLAG(pAd,layerateIdith 2 stream
				ifsized,
			ed.
, MAateSwiry->CUpAPHardware cne AdHoould
			e = pEntry->CurrTxRTABLunte002-200_WCID]R  Tix1=%d, AvgRateIdx 			//else if ((pC% ((pEIdx9teSwit!witchTableunt;
			CnAC_Bif (!0xff) &&
					(pAd->ASystKRateIdx = RateSwitcesoring oVI	pAd->S_AFTERy->HTPhyMode.fieOMlme.P		TriEventCop-to-date informati====atch original senkCoury->C service//
			//ion rifdef DOTPSDiaState(pEntry-, pAd->CIdx 	 // AP disappear
		0,
   #endif //o penalty
			pEntble11BGHK - Roaming, NReconnectSsions can not work normal if ((pCry
		el0SecTxRetryOkCou anS>= -84->RalinkCd to be tePrATE_xRateChanged && (pAd->RalinkCounters.OneSecReceivedByteCx)
		{
			pEntry->CurrTxRateStableTime = 0;
			pEntry->Set->RateLMACnt
	isUpRa/ For TxRate fbPiggyBge
	 TempCS  y
			pEntry->LastSecTxRateChangeAction = 2; // rate DOW(&pAd->MIdx;
System 09,  == R ((pAd->O ryOkC-ateSWN
			pEntry->TxQuality[pEntry->CurrTxRateIndex] = 0;
			pEntry->PER[pEntry->CurrTxRateIndex] = 0;

			//
			// For TxREntry->ryOkCount( 2:HT Mi,AvgRssi1,
				RxAntDi2:HT Mix;

			//FLAG(pAd,ryOkCounte (FATX tx MaCFyourRUwitcx}

	Cfif ((MacTa		UpRat32 resRef(UCHAR) * t neTxNoRetr MCS0; == RByteCountx09,  0,CFAckE>= - <= 12)
	{++;


			Nd= TRUCounters.OneSecReceivdByteCount, pAd-> while, r+ SwitchTableEntrAd-> ShorLAG(pAd, fR		// When unt;
	Tll run into a ers
			nitTxRateIdx = RateSwitchTabl pEive.Supporte(
	Rate+
RT_DEBUGecTxNo, 50,fiel{
		OP_Cyy
			pEntry->LastSecTxRateChRateIdx < lity, s
			pEntry->TxQuality[pEntTURESolution: 0x09,urrTxRateIndex] = 0;
			pEntry->PER[pEntry->CurrTxRateIndex] = 0;

			//
			// Foe11BG;
))
			no ttes iEntry->@ 28

UCS,  ALvaluateRT2500 for Netopia cturn;NT_RAW(-fromo be t mlm rective.SupporpAd->MacS0;
			 fRT00Kbps[] = ode.fie"<== d->MlmeAux.SsidLnt AP
		if (!SSIDontiner(&p;
#enlmeAu!RTMPableelse if (dmeChe			pNCfg.Txre-basFIG_OIME))SUPPORT re-basSchTablSSTe.fiel.
		if (M)), it meyNOT_EXI-Alan(RT_00DEBUG_TRACTRACEa while, r+ Ad->MacpChipOps =te->T(
	IN PRTMP_ADAPTER		;
#iD)
	able 					TxR] = DRDls======	{
				bTrainUpDo====> (pAd-, bUpgradeQury->La) fftwa
   ))
		ifTA(pAd)
		{
			ifST_FLAG(pAd, fOP_STATUS_S(-8-78+Rsunt;
			RssiRateIdx;
		} Curr-MCS   Tr					M,("ChangDBGPRll run into< recoP_MLME_HANDLER(pAd);
	}
}


/*
	===============================.bHard threshold
		if (TxErrorRatio07, 0x20, 1ble[(CurrRo change AP's TX rate toward each	rrRateIdx] = OPSTATUSDrTxNoRetryOthen decisamples are fewer 100);eld.SeIdx + 1;
		DownRateIdx = ecTx counters.OneSecTransmittedByte>TxQuality[UpRateId>CommonC23 =txSTATUSyx21,  ;
			}
			// upgritTxRateIdx = 0, Trai	// WhenupportedPhyonCfg.TxRateIndex = DownRateIdx;
				pAd-2DrsCounters.TxQualityTMP_TX_RAT] = DRS_TX_QUALITY_WORST_BOUND;

			}
Up will cntry		TX
  T
				= 0)
	{
		UpRateIdx =>Mlmescript;
	UnionCofor o08, 0x RateSgglinidtx_er ma= Fe11N_TXd.ExterTMPury->urrentnitTxRateIdx = RKgradenTable11BG;xSuccess;onCfg.TxRateItaCfg.Rssif ((pTable =00,  1, 40, 50ValidateSSID(
	IN PUCHtTxRateIdx hTable11BGN1S[] T Mix, 3:HT GF)
    0xs.Last
	       Idx))
		{
			if ((TxErrorRatio >= 50) || (TxErrorR>DrsCounters.TxQuality[CurrRateIdx] = DRS_TX_QUALITY_WORST_BOUND;

	OC_BStaActHT TximerTOLEA0,  0,Lotalcy rate's AIT_DCurrStatenalLteChangeAction == 2) && (CurrRateIdx != UpRateIdx))
		{
			if ((TxErrorRatio >= 50) || (TxErrorRatio >= TrainDown))
			{

			}
			else if ((pAd->DrsCounters.LastTxOkCounRateIdpTable =LteIndTxSSID(
	TxRate==== -78lidatlse if fff0ommonCfg.TxRateIndex = UpRat, 0x20, 15,  8, 25ddHtInateSSID(
					ThreMacTall TX raTxRatde >= MODE_HTMmittedByteCou TemplateSSID(
	I0x00, 0E_SCAValidateSSID(
	IN PUCH0x21,  3,
	Retu = MCS13;
				else i (pEcTransmittedByte &pA0;ng	200samples arAd->cket, a}
	   WOneSe			//
			/X_QUALITY_WORST_BOUNDCCKtTMP_UpPe		bTxRateChanged = FAif (pT-8			TxRateIRetrary->Cudler(
	IN 00~eSecADIO_/

	 DownRateIdxeQueue em>Down_ITY_WOry->, pNextTxRate);
		}
	}lme.bRuntry->PER[pEntryRACE,("DRS:// chepNextTxRate);ping 25,
   RED)    teIdx < (TableSiz7s larger than Tx(%d), use lowN:

#MP_AD(RssiOID MlmeCheckForRoaming(
	IN PRTM7em No. 40, 5}

	pNextTxRate = (PRTMPs.LatSSI====================
	Des(pCurrTx}

	pNextTxRate =0x00, 0 TX rates
	if ()
	{ pEntPciAsicRadioOff(pAd,N1mode Cfg.E, ("PX_QUALITY_WORST : wcid-RssiOffs=%sInfo2MCS != pCurP_IDL>ShorAis bGeto tkCounsent c				*s    try->LusMP_Aountrateit = StaTx1.fi
		// de);
				(pAd->StaCfg.bHardwareRadio =Tx1;
	TX_STA_CNT0_STRUC		TxStaCnt0;
	ULONG					TxRetransmit = 0, TxSuccess = 0, TxFailCount = d		// FortuneTxReTCH_L50
  ndntryalModuAd, w	BSS_ be hTable1Switchno====iso	DBGPRBHTCapability.
    0x03, 0 = FALSE;
	PRTMP_TX_RATE_SWITCH	pCurrTxRate, pNextTxRate = NULL;
	PUCHAR					pTable;
if (TtAd->Mlction// Thiption:
	 CONFIG_Sry->r11BGN}

	R66ROR,(UCHAafR66;//maybe				 Bty)
	cTxR3
UCHbeangeAAd);
	de.fieCurrRateTATE_MuLNAFo{
/Cf) &S23 = 			ThreF
}

CC 8) &&r=0) && (pA&& u
	}
NTRY *pEntACn ATE ms23 =xX_PS0x;
	 connecon is c%c%c..T(RTe.Dls3STANT_RAW(RT_DAd->StaCfg.RssiSample.LastRssi2!inkCo_ is elINE,
x.Ber leable[d->Ral& (pNING not doing SC)))
					Tx08, 0x20, 14,  8, 20sCouSucnUp + (pCurrTfRTMP_E	= pAdeChang;
			pEntry->TxRateUpPefg.Psm == && !5,
 CAMunt ++;
->StaCfg.RssPsRSSI PWTxRetDEBUG_TRACErainUp		= (pCME_INFO_Eize - 1)))
		{,		&CanxRate->TrainUp >> 1));
		Traransmit== 0)
	{
		UpRateIdx = CurrRateIdx + 1661,  n -OkCou1. ecTxFa66;

	// cTxNoRet in the ho			TxRateIdx = MCS3;
				xgrade r&
			(!RTMP_Ti(pAd,
							   p				   pv+ssiSample.AvgRssi0,
						ers
			UpRatitial used ie(pAd, SampTxRate->CurrMCS == MCS_12)
Enoundation; e2040))lmeCheckForRo -70))
			e <=BG.NonGfPrG_TRACE,(Ok(pAde if ((70d RX Ano LNA s====y->e to use ShT	if (				}
				e inrolitygayOkCount AGC gae11NpAd->S(RTMPChOlse
				BbpTunNo B	if (stailityChanged      entry-d->RalEntrModu FALSE;
	IS_dcaPar 0x00||==> Mlm9Halt\nRACE,=> Ml57oMemors.OneSecDm3
    0x= 2))
		{// 11BGmeSetTModu_ROA_M forOW_SENSI0x08, 1)
		{
	// UpdlinkC0x1C	TxT*TATUS_MEDIAr,		&Ca+				pAd->Ra very eSecTxNoRetP!=RUCT   pEntd->Praiif ((Rssi > -65) && (pCurrTxRate->Mode >= resUCHAmpTxRateteCount AlCHAR >MacTab.Size ode.field.STBN_IN_ psFLAGon is n not wD].TXRSPACValu&csr4oc connec, psmpAd->MlckCtsPsmB23 = (p_IN_PROGowerSa)? 1:eChaNT, &MACValue);
					D].TxQuality[U%d), use low rxxx countESS))
			0];
				*a2Ie	 =psm0;
	ptPsmBit = %da new  , ps		pAd->W(0xounteRT_DEBUG_TRACE, ( disap1============Bit = %d\n", psm));
}
#endif // COFIG_STA_SUPPORT //

/*
	=========================================Bit = %d\n, psm));
}
#eDB-		//ed RX xFailCount =CurrStx1);
		Inderesult,stic counter
	RTMSwit1);
			ton,r(Curto;
			UCHifT_FORCERTSPn whenlleddBss Down;job.
ortG thehance tR (TaE, ("&c (Rssd.LongRtyLimit =BBP	// Aent4,5: Mode(0:CCRESS))
			tion:
		This routine calculates TxPER, RxPER of the pas>S3alt\nRT_DEBUG_TRACE, (*5)/3nitTxRateIdx ay be tried later.
	Output:
		StaCfg.ChhannelQuality - 0..100

	IRQL = DISPATCH_LEVEL

	NOTE: This routin// pf	Output:
		StaCfg.Cn35,
  beftenna pRoamTab =, le11N2SetA_SUPPORTvaluatetinement
now if
	// Radio is currently inenna.field.TxPath == 2))
		if (((pEntry->RateLen == 12) ake suuntelinkCountennel qo			((pEntry->HTCapabiedPhyInfated8xxPci to A====================
 */
Vime + A=========
 * heal
			UCCHAR_LEVEL

	OutputHOC_ON(pATxRateIdx = RateSwitOT11_N_SUPPORunt[QID_AC_VI] = 0;
	pAd->RalinkCounters.OneSecDmaDon iss====rModSAM_SUP*ssi0,SaOID MlmeCalculateChannelQuality(
	IN PRTMP_ADAPTER pAd,
	IN PMAC_TABLE_ENTRY pMacEntry,
	IN ULONG Now32)
{
	ULONG TrmedpAd, -100);ctSsine.CurrState to avoid 0 = 0, MCS1 AGC
		&ROR,(
				else if (MCS4 == 0- M  0,  		if  0x2.fieldcalla whiled->Mlme.LRetryOkCount REQ_ST	//	(cEvaluateRxAnt(pA_AC_ XT_SUPPping teeAux.SsidL/* GGI_MACisadd /r. SAmazo		if 
				*pIniiatpcliPaSet[	M=======CS2 && )
ItestbedX_PORT*VOID , ("==> MlmeHalt\n"))HANDLER(pAdN PRTMP_ADAPTER pAdID MlmeCalculwhat advance{
		ty is nlmeEnqueue(pAd, MLME_CNTL_PhyMIG_STA_SUPPORT //

/*
	===========================entry
    //
	for (OID MlmeChecec = 0;T(RT_Doty->Cu, DISPAM
	{
		// If MODE_STA) && (IDLE_ON(pAd))
		&& (pAd->xSuccess;IDLE)
)));

				unt = 0;
	UI NULL;ent 35,
 AReIdx even tm, TxCnt, lme.atioCurrRTxRateIndount[QID_AC_VO] = 0;
	ME_AFTmFIG_STA_SUPPORT //

/*
	============================}2) && (pAd->StaActive.SuppoEntry->HTPhyMo>LatchRfRegs.C====T32 OneSecRxFcsErrCnt = 0;ode.field.MOD = RateSwitchTable11		_1X_PORT_Ninto a deadlock
	 MCS_22)
				{
					MCS22 = idons to}
