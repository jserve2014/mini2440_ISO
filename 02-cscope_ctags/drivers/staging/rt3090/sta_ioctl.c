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
    sta_ioctl.c

    Abstract:
    IOCTL related subroutines

    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
    Rory Chen   01-03-2003    created
	Rory Chen   02-14-2005    modify to support RT61
*/

#include	"rt_config.h"

#ifdef DBG
extern ULONG    RTDebugLevel;
#endif

#define NR_WEP_KEYS				4
#define WEP_SMALL_KEY_LEN			(40/8)
#define WEP_LARGE_KEY_LEN			(104/8)

#define GROUP_KEY_NO                4

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#define IWE_STREAM_ADD_EVENT(_A, _B, _C, _D, _E)		iwe_stream_add_event(_A, _B, _C, _D, _E)
#define IWE_STREAM_ADD_POINT(_A, _B, _C, _D, _E)		iwe_stream_add_point(_A, _B, _C, _D, _E)
#define IWE_STREAM_ADD_VALUE(_A, _B, _C, _D, _E, _F)	iwe_stream_add_value(_A, _B, _C, _D, _E, _F)
#else
#define IWE_STREAM_ADD_EVENT(_A, _B, _C, _D, _E)		iwe_stream_add_event(_B, _C, _D, _E)
#define IWE_STREAM_ADD_POINT(_A, _B, _C, _D, _E)		iwe_stream_add_point(_B, _C, _D, _E)
#define IWE_STREAM_ADD_VALUE(_A, _B, _C, _D, _E, _F)	iwe_stream_add_value(_B, _C, _D, _E, _F)
#endif

extern UCHAR    CipherWpa2Template[];

typedef struct PACKED _RT_VERSION_INFO{
    UCHAR       DriverVersionW;
    UCHAR       DriverVersionX;
    UCHAR       DriverVersionY;
    UCHAR       DriverVersionZ;
    UINT        DriverBuildYear;
    UINT        DriverBuildMonth;
    UINT        DriverBuildDay;
} RT_VERSION_INFO, *PRT_VERSION_INFO;

struct iw_priv_args privtab[] = {
{ RTPRIV_IOCTL_SET,
  IW_PRIV_TYPE_CHAR | 1024, 0,
  "set"},

{ RTPRIV_IOCTL_SHOW, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
  ""},
/* --- sub-ioctls definitions --- */
    { SHOW_CONN_STATUS,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "connStatus" },
	{ SHOW_DRVIER_VERION,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "driverVer" },
    { SHOW_BA_INFO,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "bainfo" },
	{ SHOW_DESC_INFO,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "descinfo" },
    { RAIO_OFF,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "radio_off" },
	{ RAIO_ON,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "radio_on" },
#ifdef QOS_DLS_SUPPORT
	{ SHOW_DLS_ENTRY_INFO,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "dlsentryinfo" },
#endif // QOS_DLS_SUPPORT //
	{ SHOW_CFG_VALUE,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "show" },
	{ SHOW_ADHOC_ENTRY_INFO,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "adhocEntry" },
/* --- sub-ioctls relations --- */

#ifdef DBG
{ RTPRIV_IOCTL_BBP,
  IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
  "bbp"},
{ RTPRIV_IOCTL_MAC,
  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
  "mac"},
#ifdef RTMP_RF_RW_SUPPORT
{ RTPRIV_IOCTL_RF,
  IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
  "rf"},
#endif // RTMP_RF_RW_SUPPORT //
{ RTPRIV_IOCTL_E2P,
  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
  "e2p"},
#endif  /* DBG */

{ RTPRIV_IOCTL_STATISTICS,
  0, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
  "stat"},
{ RTPRIV_IOCTL_GSITESURVEY,
  0, IW_PRIV_TYPE_CHAR | 1024,
  "get_site_survey"},


};

static __s32 ralinkrate[] =
	{2,  4,   11,  22, // CCK
	12, 18,   24,  36, 48, 72, 96, 108, // OFDM
	13, 26,   39,  52,  78, 104, 117, 130, 26,  52,  78, 104, 156, 208, 234, 260, // 20MHz, 800ns GI, MCS: 0 ~ 15
	39, 78,  117, 156, 234, 312, 351, 390,										  // 20MHz, 800ns GI, MCS: 16 ~ 23
	27, 54,   81, 108, 162, 216, 243, 270, 54, 108, 162, 216, 324, 432, 486, 540, // 40MHz, 800ns GI, MCS: 0 ~ 15
	81, 162, 243, 324, 486, 648, 729, 810,										  // 40MHz, 800ns GI, MCS: 16 ~ 23
	14, 29,   43,  57,  87, 115, 130, 144, 29, 59,   87, 115, 173, 230, 260, 288, // 20MHz, 400ns GI, MCS: 0 ~ 15
	43, 87,  130, 173, 260, 317, 390, 433,										  // 20MHz, 400ns GI, MCS: 16 ~ 23
	30, 60,   90, 120, 180, 240, 270, 300, 60, 120, 180, 240, 360, 480, 540, 600, // 40MHz, 400ns GI, MCS: 0 ~ 15
	90, 180, 270, 360, 540, 720, 810, 900};



INT Set_SSID_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

#ifdef WMM_SUPPORT
INT	Set_WmmCapable_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg);
#endif

INT Set_NetworkType_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_AuthMode_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_EncrypType_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_DefaultKeyID_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_Key1_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_Key2_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_Key3_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_Key4_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_WPAPSK_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);


INT Set_PSMode_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

#ifdef RT3090
INT Set_PCIePSLevel_Proc(
IN  PRTMP_ADAPTER   pAdapter,
IN  PUCHAR          arg);
#endif // RT3090 //
#ifdef WPA_SUPPLICANT_SUPPORT
INT Set_Wpa_Support(
    IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg);
#endif // WPA_SUPPLICANT_SUPPORT //

#ifdef DBG

VOID RTMPIoctlMAC(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq);

VOID RTMPIoctlE2PROM(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  struct iwreq    *wrq);
#endif // DBG //


NDIS_STATUS RTMPWPANoneAddKeyProc(
    IN  PRTMP_ADAPTER   pAd,
    IN	PVOID			pBuf);

INT Set_FragTest_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

#ifdef DOT11_N_SUPPORT
INT Set_TGnWifiTest_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING          arg);
#endif // DOT11_N_SUPPORT //

INT Set_LongRetryLimit_Proc(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	PSTRING			arg);

INT Set_ShortRetryLimit_Proc(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	PSTRING			arg);

#ifdef EXT_BUILD_CHANNEL_LIST
INT Set_Ieee80211dClientMode_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);
#endif // EXT_BUILD_CHANNEL_LIST //

#ifdef CARRIER_DETECTION_SUPPORT
INT Set_CarrierDetect_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING          arg);
#endif // CARRIER_DETECTION_SUPPORT //

INT	Show_Adhoc_MacTable_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			extra);

#ifdef RTMP_RF_RW_SUPPORT
VOID RTMPIoctlRF(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq);
#endif // RTMP_RF_RW_SUPPORT //


INT Set_BeaconLostTime_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING         arg);

INT Set_AutoRoaming_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING         arg);

INT Set_SiteSurvey_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg);

INT Set_ForceTxBurst_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING         arg);

#ifdef ANT_DIVERSITY_SUPPORT
INT	Set_Antenna_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PUCHAR			arg);
#endif // ANT_DIVERSITY_SUPPORT //

static struct {
	PSTRING name;
	INT (*set_proc)(PRTMP_ADAPTER pAdapter, PSTRING arg);
} *PRTMP_PRIVATE_SET_PROC, RTMP_PRIVATE_SUPPORT_PROC[] = {
	{"DriverVersion",				Set_DriverVersion_Proc},
	{"CountryRegion",				Set_CountryRegion_Proc},
	{"CountryRegionABand",			Set_CountryRegionABand_Proc},
	{"SSID",						Set_SSID_Proc},
	{"WirelessMode",				Set_WirelessMode_Proc},
	{"TxBurst",					Set_TxBurst_Proc},
	{"TxPreamble",				Set_TxPreamble_Proc},
	{"TxPower",					Set_TxPower_Proc},
	{"Channel",					Set_Channel_Proc},
	{"BGProtection",				Set_BGProtection_Proc},
	{"RTSThreshold",				Set_RTSThreshold_Proc},
	{"FragThreshold",				Set_FragThreshold_Proc},
#ifdef DOT11_N_SUPPORT
	{"HtBw",		                Set_HtBw_Proc},
	{"HtMcs",		                Set_HtMcs_Proc},
	{"HtGi",		                Set_HtGi_Proc},
	{"HtOpMode",		            Set_HtOpMode_Proc},
	{"HtExtcha",		            Set_HtExtcha_Proc},
	{"HtMpduDensity",		        Set_HtMpduDensity_Proc},
	{"HtBaWinSize",				Set_HtBaWinSize_Proc},
	{"HtRdg",					Set_HtRdg_Proc},
	{"HtAmsdu",					Set_HtAmsdu_Proc},
	{"HtAutoBa",				Set_HtAutoBa_Proc},
	{"HtBaDecline",					Set_BADecline_Proc},
	{"HtProtect",				Set_HtProtect_Proc},
	{"HtMimoPs",				Set_HtMimoPs_Proc},
	{"HtDisallowTKIP",				Set_HtDisallowTKIP_Proc},
#endif // DOT11_N_SUPPORT //

#ifdef AGGREGATION_SUPPORT
	{"PktAggregate",				Set_PktAggregate_Proc},
#endif // AGGREGATION_SUPPORT //

#ifdef WMM_SUPPORT
	{"WmmCapable",					Set_WmmCapable_Proc},
#endif
	{"IEEE80211H",					Set_IEEE80211H_Proc},
    {"NetworkType",                 Set_NetworkType_Proc},
	{"AuthMode",					Set_AuthMode_Proc},
	{"EncrypType",					Set_EncrypType_Proc},
	{"DefaultKeyID",				Set_DefaultKeyID_Proc},
	{"Key1",						Set_Key1_Proc},
	{"Key2",						Set_Key2_Proc},
	{"Key3",						Set_Key3_Proc},
	{"Key4",						Set_Key4_Proc},
	{"WPAPSK",						Set_WPAPSK_Proc},
	{"ResetCounter",				Set_ResetStatCounter_Proc},
	{"PSMode",                      Set_PSMode_Proc},
#ifdef DBG
	{"Debug",						Set_Debug_Proc},
#endif // DBG //

#ifdef RALINK_ATE
	{"ATE",							Set_ATE_Proc},
	{"ATEDA",						Set_ATE_DA_Proc},
	{"ATESA",						Set_ATE_SA_Proc},
	{"ATEBSSID",					Set_ATE_BSSID_Proc},
	{"ATECHANNEL",					Set_ATE_CHANNEL_Proc},
	{"ATETXPOW0",					Set_ATE_TX_POWER0_Proc},
	{"ATETXPOW1",					Set_ATE_TX_POWER1_Proc},
	{"ATETXANT",					Set_ATE_TX_Antenna_Proc},
	{"ATERXANT",					Set_ATE_RX_Antenna_Proc},
	{"ATETXFREQOFFSET",				Set_ATE_TX_FREQOFFSET_Proc},
	{"ATETXBW",						Set_ATE_TX_BW_Proc},
	{"ATETXLEN",					Set_ATE_TX_LENGTH_Proc},
	{"ATETXCNT",					Set_ATE_TX_COUNT_Proc},
	{"ATETXMCS",					Set_ATE_TX_MCS_Proc},
	{"ATETXMODE",					Set_ATE_TX_MODE_Proc},
	{"ATETXGI",						Set_ATE_TX_GI_Proc},
	{"ATERXFER",					Set_ATE_RX_FER_Proc},
	{"ATERRF",						Set_ATE_Read_RF_Proc},
	{"ATEWRF1",						Set_ATE_Write_RF1_Proc},
	{"ATEWRF2",						Set_ATE_Write_RF2_Proc},
	{"ATEWRF3",						Set_ATE_Write_RF3_Proc},
	{"ATEWRF4",						Set_ATE_Write_RF4_Proc},
	{"ATELDE2P",				    Set_ATE_Load_E2P_Proc},
	{"ATERE2P",						Set_ATE_Read_E2P_Proc},
	{"ATESHOW",						Set_ATE_Show_Proc},
	{"ATEHELP",						Set_ATE_Help_Proc},

#ifdef RALINK_28xx_QA
	{"TxStop",						Set_TxStop_Proc},
	{"RxStop",						Set_RxStop_Proc},
#endif // RALINK_28xx_QA //
#endif // RALINK_ATE //

#ifdef WPA_SUPPLICANT_SUPPORT
    {"WpaSupport",                  Set_Wpa_Support},
#endif // WPA_SUPPLICANT_SUPPORT //





	{"FixedTxMode",                 Set_FixedTxMode_Proc},
#ifdef CONFIG_APSTA_MIXED_SUPPORT
	{"OpMode",						Set_OpMode_Proc},
#endif // CONFIG_APSTA_MIXED_SUPPORT //
#ifdef DOT11_N_SUPPORT
    {"TGnWifiTest",                 Set_TGnWifiTest_Proc},
    {"ForceGF",					Set_ForceGF_Proc},
#endif // DOT11_N_SUPPORT //
#ifdef QOS_DLS_SUPPORT
	{"DlsAddEntry",					Set_DlsAddEntry_Proc},
	{"DlsTearDownEntry",			Set_DlsTearDownEntry_Proc},
#endif // QOS_DLS_SUPPORT //
	{"LongRetry",				Set_LongRetryLimit_Proc},
	{"ShortRetry",				Set_ShortRetryLimit_Proc},
#ifdef EXT_BUILD_CHANNEL_LIST
	{"11dClientMode",				Set_Ieee80211dClientMode_Proc},
#endif // EXT_BUILD_CHANNEL_LIST //
#ifdef CARRIER_DETECTION_SUPPORT
	{"CarrierDetect",				Set_CarrierDetect_Proc},
#endif // CARRIER_DETECTION_SUPPORT //


//2008/09/11:KH add to support efuse<--
#ifdef RT30xx
#ifdef RTMP_EFUSE_SUPPORT
	{"efuseFreeNumber",				set_eFuseGetFreeBlockCount_Proc},
	{"efuseDump",					set_eFusedump_Proc},
	{"efuseLoadFromBin",				set_eFuseLoadFromBin_Proc},
	{"efuseBufferModeWriteBack",		set_eFuseBufferModeWriteBack_Proc},
#endif // RTMP_EFUSE_SUPPORT //
#ifdef ANT_DIVERSITY_SUPPORT
	{"ant",					Set_Antenna_Proc},
#endif // ANT_DIVERSITY_SUPPORT //
#endif // RT30xx //
//2008/09/11:KH add to support efuse-->

	{"BeaconLostTime",				Set_BeaconLostTime_Proc},
	{"AutoRoaming",					Set_AutoRoaming_Proc},
	{"SiteSurvey",					Set_SiteSurvey_Proc},
	{"ForceTxBurst",				Set_ForceTxBurst_Proc},

	{NULL,}
};


VOID RTMPAddKey(
	IN	PRTMP_ADAPTER	    pAd,
	IN	PNDIS_802_11_KEY    pKey)
{
	ULONG				KeyIdx;
	MAC_TABLE_ENTRY		*pEntry;

    DBGPRINT(RT_DEBUG_TRACE, ("RTMPAddKey ------>\n"));

	if (pAd->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
	{
		if (pKey->KeyIndex & 0x80000000)
		{
		    if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPANone)
            {
                NdisZeroMemory(pAd->StaCfg.PMK, 32);
                NdisMoveMemory(pAd->StaCfg.PMK, pKey->KeyMaterial, pKey->KeyLength);
                goto end;
            }
		    // Update PTK
		    NdisZeroMemory(&pAd->SharedKey[BSS0][0], sizeof(CIPHER_KEY));
            pAd->SharedKey[BSS0][0].KeyLen = LEN_TKIP_EK;
            NdisMoveMemory(pAd->SharedKey[BSS0][0].Key, pKey->KeyMaterial, LEN_TKIP_EK);
#ifdef WPA_SUPPLICANT_SUPPORT
            if (pAd->StaCfg.PairCipher == Ndis802_11Encryption2Enabled)
            {
                NdisMoveMemory(pAd->SharedKey[BSS0][0].RxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][0].TxMic, pKey->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }
            else
#endif // WPA_SUPPLICANT_SUPPORT //
            {
		NdisMoveMemory(pAd->SharedKey[BSS0][0].TxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][0].RxMic, pKey->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }

            // Decide its ChiperAlg
		if (pAd->StaCfg.PairCipher == Ndis802_11Encryption2Enabled)
			pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_TKIP;
		else if (pAd->StaCfg.PairCipher == Ndis802_11Encryption3Enabled)
			pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_AES;
		else
			pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_NONE;

            // Update these related information to MAC_TABLE_ENTRY
		pEntry = &pAd->MacTab.Content[BSSID_WCID];
            NdisMoveMemory(pEntry->PairwiseKey.Key, pAd->SharedKey[BSS0][0].Key, LEN_TKIP_EK);
		NdisMoveMemory(pEntry->PairwiseKey.RxMic, pAd->SharedKey[BSS0][0].RxMic, LEN_TKIP_RXMICK);
		NdisMoveMemory(pEntry->PairwiseKey.TxMic, pAd->SharedKey[BSS0][0].TxMic, LEN_TKIP_TXMICK);
		pEntry->PairwiseKey.CipherAlg = pAd->SharedKey[BSS0][0].CipherAlg;

		// Update pairwise key information to ASIC Shared Key Table
		AsicAddSharedKeyEntry(pAd,
							  BSS0,
							  0,
							  pAd->SharedKey[BSS0][0].CipherAlg,
							  pAd->SharedKey[BSS0][0].Key,
							  pAd->SharedKey[BSS0][0].TxMic,
							  pAd->SharedKey[BSS0][0].RxMic);

		// Update ASIC WCID attribute table and IVEIV table
		RTMPAddWcidAttributeEntry(pAd,
								  BSS0,
								  0,
								  pAd->SharedKey[BSS0][0].CipherAlg,
								  pEntry);

            if (pAd->StaCfg.AuthMode >= Ndis802_11AuthModeWPA2)
            {
                // set 802.1x port control
	            //pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
				STA_PORT_SECURED(pAd);

                // Indicate Connected for GUI
                pAd->IndicateMediaState = NdisMediaStateConnected;
            }
		}
        else
        {
            // Update GTK
            pAd->StaCfg.DefaultKeyId = (pKey->KeyIndex & 0xFF);
            NdisZeroMemory(&pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId], sizeof(CIPHER_KEY));
            pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].KeyLen = LEN_TKIP_EK;
            NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].Key, pKey->KeyMaterial, LEN_TKIP_EK);
#ifdef WPA_SUPPLICANT_SUPPORT
            if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
            {
                NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic, pKey->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }
            else
#endif // WPA_SUPPLICANT_SUPPORT //
            {
		NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic, pKey->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }

            // Update Shared Key CipherAlg
		pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_NONE;
		if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_TKIP;
		else if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption3Enabled)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_AES;

            // Update group key information to ASIC Shared Key Table
		AsicAddSharedKeyEntry(pAd,
							  BSS0,
							  pAd->StaCfg.DefaultKeyId,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].Key,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic);

		// Update ASIC WCID attribute table and IVEIV table
		RTMPAddWcidAttributeEntry(pAd,
								  BSS0,
								  pAd->StaCfg.DefaultKeyId,
								  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg,
								  NULL);

            // set 802.1x port control
	        //pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
			STA_PORT_SECURED(pAd);

            // Indicate Connected for GUI
            pAd->IndicateMediaState = NdisMediaStateConnected;
        }
	}
	else	// dynamic WEP from wpa_supplicant
	{
		UCHAR	CipherAlg;
	PUCHAR	Key;

		if(pKey->KeyLength == 32)
			goto end;

		KeyIdx = pKey->KeyIndex & 0x0fffffff;

		if (KeyIdx < 4)
		{
			// it is a default shared key, for Pairwise key setting
			if (pKey->KeyIndex & 0x80000000)
			{
				pEntry = MacTableLookup(pAd, pKey->BSSID);

				if (pEntry)
				{
					DBGPRINT(RT_DEBUG_TRACE, ("RTMPAddKey: Set Pair-wise Key\n"));

					// set key material and key length
					pEntry->PairwiseKey.KeyLen = (UCHAR)pKey->KeyLength;
					NdisMoveMemory(pEntry->PairwiseKey.Key, &pKey->KeyMaterial, pKey->KeyLength);

					// set Cipher type
					if (pKey->KeyLength == 5)
						pEntry->PairwiseKey.CipherAlg = CIPHER_WEP64;
					else
						pEntry->PairwiseKey.CipherAlg = CIPHER_WEP128;

					// Add Pair-wise key to Asic
					AsicAddPairwiseKeyEntry(
						pAd,
						pEntry->Addr,
						(UCHAR)pEntry->Aid,
				&pEntry->PairwiseKey);

					// update WCID attribute table and IVEIV table for this entry
					RTMPAddWcidAttributeEntry(
						pAd,
						BSS0,
						KeyIdx, // The value may be not zero
						pEntry->PairwiseKey.CipherAlg,
						pEntry);

				}
			}
			else
            {
				// Default key for tx (shared key)
				pAd->StaCfg.DefaultKeyId = (UCHAR) KeyIdx;

				// set key material and key length
				pAd->SharedKey[BSS0][KeyIdx].KeyLen = (UCHAR) pKey->KeyLength;
				NdisMoveMemory(pAd->SharedKey[BSS0][KeyIdx].Key, &pKey->KeyMaterial, pKey->KeyLength);

				// Set Ciper type
				if (pKey->KeyLength == 5)
					pAd->SharedKey[BSS0][KeyIdx].CipherAlg = CIPHER_WEP64;
				else
					pAd->SharedKey[BSS0][KeyIdx].CipherAlg = CIPHER_WEP128;

			CipherAlg = pAd->SharedKey[BSS0][KeyIdx].CipherAlg;
			Key = pAd->SharedKey[BSS0][KeyIdx].Key;

				// Set Group key material to Asic
			AsicAddSharedKeyEntry(pAd, BSS0, KeyIdx, CipherAlg, Key, NULL, NULL);

				// Update WCID attribute table and IVEIV table for this group key table
				RTMPAddWcidAttributeEntry(pAd, BSS0, KeyIdx, CipherAlg, NULL);

			}
		}
	}
end:
	return;
}

char * rtstrchr(const char * s, int c)
{
    for(; *s != (char) c; ++s)
        if (*s == '\0')
            return NULL;
    return (char *) s;
}

/*
This is required for LinEX2004/kernel2.6.7 to provide iwlist scanning function
*/

int
rt_ioctl_giwname(struct net_device *dev,
		   struct iw_request_info *info,
		   char *name, char *extra)
{

#ifdef RTMP_MAC_PCI
    strncpy(name, "RT2860 Wireless", IFNAMSIZ);
#endif // RTMP_MAC_PCI //
	return 0;
}

int rt_ioctl_siwfreq(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_freq *freq, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	int	chan = -1;

    //check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
        DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
        return -ENETDOWN;
    }


	if (freq->e > 1)
		return -EINVAL;

	if((freq->e == 0) && (freq->m <= 1000))
		chan = freq->m;	// Setting by channel number
	else
		MAP_KHZ_TO_CHANNEL_ID( (freq->m /100) , chan); // Setting by frequency - search the table , like 2.412G, 2.422G,

    if (ChannelSanity(pAdapter, chan) == TRUE)
    {
	pAdapter->CommonCfg.Channel = chan;
	DBGPRINT(RT_DEBUG_ERROR, ("==>rt_ioctl_siwfreq::SIOCSIWFREQ[cmd=0x%x] (Channel=%d)\n", SIOCSIWFREQ, pAdapter->CommonCfg.Channel));
    }
    else
        return -EINVAL;

	return 0;
}


int rt_ioctl_giwfreq(struct net_device *dev,
		   struct iw_request_info *info,
		   struct iw_freq *freq, char *extra)
{
	PRTMP_ADAPTER pAdapter = NULL;
	UCHAR ch;
	ULONG	m = 2412000;

	pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

		ch = pAdapter->CommonCfg.Channel;

	DBGPRINT(RT_DEBUG_TRACE,("==>rt_ioctl_giwfreq  %d\n", ch));

	MAP_CHANNEL_ID_TO_KHZ(ch, m);
	freq->m = m * 100;
	freq->e = 1;
	return 0;
}


int rt_ioctl_siwmode(struct net_device *dev,
		   struct iw_request_info *info,
		   __u32 *mode, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	//check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
	DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
    }

	switch (*mode)
	{
		case IW_MODE_ADHOC:
			Set_NetworkType_Proc(pAdapter, "Adhoc");
			break;
		case IW_MODE_INFRA:
			Set_NetworkType_Proc(pAdapter, "Infra");
			break;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,20))
        case IW_MODE_MONITOR:
			Set_NetworkType_Proc(pAdapter, "Monitor");
			break;
#endif
		default:
			DBGPRINT(RT_DEBUG_TRACE, ("===>rt_ioctl_siwmode::SIOCSIWMODE (unknown %d)\n", *mode));
			return -EINVAL;
	}

	// Reset Ralink supplicant to not use, it will be set to start when UI set PMK key
	pAdapter->StaCfg.WpaState = SS_NOTUSE;

	return 0;
}


int rt_ioctl_giwmode(struct net_device *dev,
		   struct iw_request_info *info,
		   __u32 *mode, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	if (ADHOC_ON(pAdapter))
		*mode = IW_MODE_ADHOC;
    else if (INFRA_ON(pAdapter))
		*mode = IW_MODE_INFRA;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,20))
    else if (MONITOR_ON(pAdapter))
    {
        *mode = IW_MODE_MONITOR;
    }
#endif
    else
        *mode = IW_MODE_AUTO;

	DBGPRINT(RT_DEBUG_TRACE, ("==>rt_ioctl_giwmode(mode=%d)\n", *mode));
	return 0;
}

int rt_ioctl_siwsens(struct net_device *dev,
		   struct iw_request_info *info,
		   char *name, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	return 0;
}

int rt_ioctl_giwsens(struct net_device *dev,
		   struct iw_request_info *info,
		   char *name, char *extra)
{
	return 0;
}

int rt_ioctl_giwrange(struct net_device *dev,
		   struct iw_request_info *info,
		   struct iw_point *data, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_range *range = (struct iw_range *) extra;
	u16 val;
	int i;

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	DBGPRINT(RT_DEBUG_TRACE ,("===>rt_ioctl_giwrange\n"));
	data->length = sizeof(struct iw_range);
	memset(range, 0, sizeof(struct iw_range));

	range->txpower_capa = IW_TXPOW_DBM;

	if (INFRA_ON(pAdapter)||ADHOC_ON(pAdapter))
	{
		range->min_pmp = 1 * 1024;
		range->max_pmp = 65535 * 1024;
		range->min_pmt = 1 * 1024;
		range->max_pmt = 1000 * 1024;
		range->pmp_flags = IW_POWER_PERIOD;
		range->pmt_flags = IW_POWER_TIMEOUT;
		range->pm_capa = IW_POWER_PERIOD | IW_POWER_TIMEOUT |
			IW_POWER_UNICAST_R | IW_POWER_ALL_R;
	}

	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 14;

	range->retry_capa = IW_RETRY_LIMIT;
	range->retry_flags = IW_RETRY_LIMIT;
	range->min_retry = 0;
	range->max_retry = 255;

	range->num_channels =  pAdapter->ChannelListNum;

	val = 0;
	for (i = 1; i <= range->num_channels; i++)
	{
		u32 m = 2412000;
		range->freq[val].i = pAdapter->ChannelList[i-1].Channel;
		MAP_CHANNEL_ID_TO_KHZ(pAdapter->ChannelList[i-1].Channel, m);
		range->freq[val].m = m * 100; /* OS_HZ */

		range->freq[val].e = 1;
		val++;
		if (val == IW_MAX_FREQUENCIES)
			break;
	}
	range->num_frequency = val;

	range->max_qual.qual = 100; /* what is correct max? This was not
					* documented exactly. At least
					* 69 has been observed. */
	range->max_qual.level = 0; /* dB */
	range->max_qual.noise = 0; /* dB */

	/* What would be suitable values for "average/typical" qual? */
	range->avg_qual.qual = 20;
	range->avg_qual.level = -60;
	range->avg_qual.noise = -95;
	range->sensitivity = 3;

	range->max_encoding_tokens = NR_WEP_KEYS;
	range->num_encoding_sizes = 2;
	range->encoding_size[0] = 5;
	range->encoding_size[1] = 13;

	range->min_rts = 0;
	range->max_rts = 2347;
	range->min_frag = 256;
	range->max_frag = 2346;

#if WIRELESS_EXT > 17
	/* IW_ENC_CAPA_* bit field */
	range->enc_capa = IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2 |
					IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP;
#endif

	return 0;
}

int rt_ioctl_siwap(struct net_device *dev,
		      struct iw_request_info *info,
		      struct sockaddr *ap_addr, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
    NDIS_802_11_MAC_ADDRESS Bssid;

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
	DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
    }

	if (pAdapter->Mlme.CntlMachine.CurrState != CNTL_IDLE)
    {
        RTMP_MLME_RESET_STATE_MACHINE(pAdapter);
        DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME busy, reset MLME state machine !!!\n"));
    }

    // tell CNTL state machine to call NdisMSetInformationComplete() after completing
    // this request, because this request is initiated by NDIS.
    pAdapter->MlmeAux.CurrReqIsFromNdis = FALSE;
	// Prevent to connect AP again in STAMlmePeriodicExec
	pAdapter->MlmeAux.AutoReconnectSsidLen= 32;

    memset(Bssid, 0, MAC_ADDR_LEN);
    memcpy(Bssid, ap_addr->sa_data, MAC_ADDR_LEN);
    MlmeEnqueue(pAdapter,
                MLME_CNTL_STATE_MACHINE,
                OID_802_11_BSSID,
                sizeof(NDIS_802_11_MAC_ADDRESS),
                (VOID *)&Bssid);

    DBGPRINT(RT_DEBUG_TRACE, ("IOCTL::SIOCSIWAP %02x:%02x:%02x:%02x:%02x:%02x\n",
        Bssid[0], Bssid[1], Bssid[2], Bssid[3], Bssid[4], Bssid[5]));

	return 0;
}

int rt_ioctl_giwap(struct net_device *dev,
		      struct iw_request_info *info,
		      struct sockaddr *ap_addr, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	if (INFRA_ON(pAdapter) || ADHOC_ON(pAdapter))
	{
		ap_addr->sa_family = ARPHRD_ETHER;
		memcpy(ap_addr->sa_data, &pAdapter->CommonCfg.Bssid, ETH_ALEN);
	}
#ifdef WPA_SUPPLICANT_SUPPORT
    // Add for RT2870
    else if (pAdapter->StaCfg.WpaSupplicantUP != WPA_SUPPLICANT_DISABLE)
    {
        ap_addr->sa_family = ARPHRD_ETHER;
        memcpy(ap_addr->sa_data, &pAdapter->MlmeAux.Bssid, ETH_ALEN);
    }
#endif // WPA_SUPPLICANT_SUPPORT //
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, ("IOCTL::SIOCGIWAP(=EMPTY)\n"));
		return -ENOTCONN;
	}

	return 0;
}

/*
 * Units are in db above the noise floor. That means the
 * rssi values reported in the tx/rx descriptors in the
 * driver are the SNR expressed in db.
 *
 * If you assume that the noise floor is -95, which is an
 * excellent assumption 99.5 % of the time, then you can
 * derive the absolute signal level (i.e. -95 + rssi).
 * There are some other slight factors to take into account
 * depending on whether the rssi measurement is from 11b,
 * 11g, or 11a.   These differences are at most 2db and
 * can be documented.
 *
 * NB: various calculations are based on the orinoco/wavelan
 *     drivers for compatibility
 */
static void set_quality(PRTMP_ADAPTER pAdapter,
                        struct iw_quality *iq,
                        signed char rssi)
{
	__u8 ChannelQuality;

	// Normalize Rssi
	if (rssi >= -50)
        ChannelQuality = 100;
	else if (rssi >= -80) // between -50 ~ -80dbm
		ChannelQuality = (__u8)(24 + ((rssi + 80) * 26)/10);
	else if (rssi >= -90)   // between -80 ~ -90dbm
        ChannelQuality = (__u8)((rssi + 90) * 26)/10;
	else
		ChannelQuality = 0;

    iq->qual = (__u8)ChannelQuality;

    iq->level = (__u8)(rssi);
    iq->noise = (pAdapter->BbpWriteLatch[66] > pAdapter->BbpTuning.FalseCcaUpperThreshold) ? ((__u8)pAdapter->BbpTuning.FalseCcaUpperThreshold) : ((__u8) pAdapter->BbpWriteLatch[66]);		// noise level (dBm)
    iq->noise += 256 - 143;
    iq->updated = pAdapter->iw_stats.qual.updated;
}

int rt_ioctl_iwaplist(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_point *data, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	struct sockaddr addr[IW_MAX_AP];
	struct iw_quality qual[IW_MAX_AP];
	int i;

	//check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
	DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		data->length = 0;
		return 0;
        //return -ENETDOWN;
	}

	for (i = 0; i <IW_MAX_AP ; i++)
	{
		if (i >=  pAdapter->ScanTab.BssNr)
			break;
		addr[i].sa_family = ARPHRD_ETHER;
			memcpy(addr[i].sa_data, &pAdapter->ScanTab.BssEntry[i].Bssid, MAC_ADDR_LEN);
		set_quality(pAdapter, &qual[i], pAdapter->ScanTab.BssEntry[i].Rssi);
	}
	data->length = i;
	memcpy(extra, &addr, i*sizeof(addr[0]));
	data->flags = 1;		/* signal quality present (sort of) */
	memcpy(extra + i*sizeof(addr[0]), &qual, i*sizeof(qual[i]));

	return 0;
}

#ifdef SIOCGIWSCAN
int rt_ioctl_siwscan(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_point *data, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	ULONG								Now;
	int Status = NDIS_STATUS_SUCCESS;

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	if (MONITOR_ON(pAdapter))
    {
        DBGPRINT(RT_DEBUG_TRACE, ("!!! Driver is in Monitor Mode now !!!\n"));
        return -EINVAL;
    }


#ifdef WPA_SUPPLICANT_SUPPORT
	if (pAdapter->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_ENABLE)
	{
		pAdapter->StaCfg.WpaSupplicantScanCount++;
	}
#endif // WPA_SUPPLICANT_SUPPORT //

    pAdapter->StaCfg.bScanReqIsFromWebUI = TRUE;
	if (RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
		return NDIS_STATUS_SUCCESS;
	do{
		Now = jiffies;

#ifdef WPA_SUPPLICANT_SUPPORT
		if ((pAdapter->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_ENABLE) &&
			(pAdapter->StaCfg.WpaSupplicantScanCount > 3))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("!!! WpaSupplicantScanCount > 3\n"));
			Status = NDIS_STATUS_SUCCESS;
			break;
		}
#endif // WPA_SUPPLICANT_SUPPORT //

		if ((OPSTATUS_TEST_FLAG(pAdapter, fOP_STATUS_MEDIA_STATE_CONNECTED)) &&
			((pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeWPA) ||
			(pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK)) &&
			(pAdapter->StaCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("!!! Link UP, Port Not Secured! ignore this set::OID_802_11_BSSID_LIST_SCAN\n"));
			Status = NDIS_STATUS_SUCCESS;
			break;
		}

		if (pAdapter->Mlme.CntlMachine.CurrState != CNTL_IDLE)
		{
			RTMP_MLME_RESET_STATE_MACHINE(pAdapter);
			DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME busy, reset MLME state machine !!!\n"));
		}

		// tell CNTL state machine to call NdisMSetInformationComplete() after completing
		// this request, because this request is initiated by NDIS.
		pAdapter->MlmeAux.CurrReqIsFromNdis = FALSE;
		// Reset allowed scan retries
		pAdapter->StaCfg.ScanCnt = 0;
		pAdapter->StaCfg.LastScanTime = Now;

		MlmeEnqueue(pAdapter,
			MLME_CNTL_STATE_MACHINE,
			OID_802_11_BSSID_LIST_SCAN,
			0,
			NULL);

		Status = NDIS_STATUS_SUCCESS;
		RTMP_MLME_HANDLER(pAdapter);
	}while(0);
	return NDIS_STATUS_SUCCESS;
}

int rt_ioctl_giwscan(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_point *data, char *extra)
{

	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	int i=0;
	PSTRING current_ev = extra, previous_ev = extra;
	PSTRING end_buf;
	PSTRING current_val;
	STRING custom[MAX_CUSTOM_LEN] = {0};
#ifndef IWEVGENIE
	unsigned char idx;
#endif // IWEVGENIE //
	struct iw_event iwe;

	if (RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
    {
		/*
		 * Still scanning, indicate the caller should try again.
		 */
		return -EAGAIN;
	}


#ifdef WPA_SUPPLICANT_SUPPORT
	if (pAdapter->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_ENABLE)
	{
		pAdapter->StaCfg.WpaSupplicantScanCount = 0;
	}
#endif // WPA_SUPPLICANT_SUPPORT //

	if (pAdapter->ScanTab.BssNr == 0)
	{
		data->length = 0;
		return 0;
	}

#if WIRELESS_EXT >= 17
    if (data->length > 0)
        end_buf = extra + data->length;
    else
        end_buf = extra + IW_SCAN_MAX_DATA;
#else
    end_buf = extra + IW_SCAN_MAX_DATA;
#endif

	for (i = 0; i < pAdapter->ScanTab.BssNr; i++)
	{
		if (current_ev >= end_buf)
        {
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif
        }

		//MAC address
		//================================
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWAP;
		iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
				memcpy(iwe.u.ap_addr.sa_data, &pAdapter->ScanTab.BssEntry[i].Bssid, ETH_ALEN);

        previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev,end_buf, &iwe, IW_EV_ADDR_LEN);
        if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

		/*
		Protocol:
			it will show scanned AP's WirelessMode .
			it might be
					802.11a
					802.11a/n
					802.11g/n
					802.11b/g/n
					802.11g
					802.11b/g
		*/
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWNAME;


	{
		PBSS_ENTRY pBssEntry=&pAdapter->ScanTab.BssEntry[i];
		BOOLEAN isGonly=FALSE;
		int rateCnt=0;

		if (pBssEntry->Channel>14)
		{
			if (pBssEntry->HtCapabilityLen!=0)
				strcpy(iwe.u.name,"802.11a/n");
			else
				strcpy(iwe.u.name,"802.11a");
		}
		else
		{
			/*
				if one of non B mode rate is set supported rate . it mean G only.
			*/
			for (rateCnt=0;rateCnt<pBssEntry->SupRateLen;rateCnt++)
			{
				/*
					6Mbps(140) 9Mbps(146) and >=12Mbps(152) are supported rate , it mean G only.
				*/
				if (pBssEntry->SupRate[rateCnt]==140 || pBssEntry->SupRate[rateCnt]==146 || pBssEntry->SupRate[rateCnt]>=152)
					isGonly=TRUE;
			}

			for (rateCnt=0;rateCnt<pBssEntry->ExtRateLen;rateCnt++)
			{
				if (pBssEntry->ExtRate[rateCnt]==140 || pBssEntry->ExtRate[rateCnt]==146 || pBssEntry->ExtRate[rateCnt]>=152)
					isGonly=TRUE;
			}


			if (pBssEntry->HtCapabilityLen!=0)
			{
				if (isGonly==TRUE)
					strcpy(iwe.u.name,"802.11g/n");
				else
					strcpy(iwe.u.name,"802.11b/g/n");
			}
			else
			{
				if (isGonly==TRUE)
					strcpy(iwe.u.name,"802.11g");
				else
				{
					if (pBssEntry->SupRateLen==4 && pBssEntry->ExtRateLen==0)
						strcpy(iwe.u.name,"802.11b");
					else
						strcpy(iwe.u.name,"802.11b/g");
				}
			}
		}
	}

		previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev,end_buf, &iwe, IW_EV_ADDR_LEN);
		if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
			return -E2BIG;
#else
			break;
#endif

		//ESSID
		//================================
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWESSID;
		iwe.u.data.length = pAdapter->ScanTab.BssEntry[i].SsidLen;
		iwe.u.data.flags = 1;

        previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_POINT(info, current_ev,end_buf, &iwe, (PSTRING) pAdapter->ScanTab.BssEntry[i].Ssid);
        if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

		//Network Type
		//================================
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWMODE;
		if (pAdapter->ScanTab.BssEntry[i].BssType == Ndis802_11IBSS)
		{
			iwe.u.mode = IW_MODE_ADHOC;
		}
		else if (pAdapter->ScanTab.BssEntry[i].BssType == Ndis802_11Infrastructure)
		{
			iwe.u.mode = IW_MODE_INFRA;
		}
		else
		{
			iwe.u.mode = IW_MODE_AUTO;
		}
		iwe.len = IW_EV_UINT_LEN;

        previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev, end_buf, &iwe,  IW_EV_UINT_LEN);
        if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

		//Channel and Frequency
		//================================
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWFREQ;
		if (INFRA_ON(pAdapter) || ADHOC_ON(pAdapter))
			iwe.u.freq.m = pAdapter->ScanTab.BssEntry[i].Channel;
		else
			iwe.u.freq.m = pAdapter->ScanTab.BssEntry[i].Channel;
		iwe.u.freq.e = 0;
		iwe.u.freq.i = 0;

		previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev,end_buf, &iwe, IW_EV_FREQ_LEN);
        if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

        //Add quality statistics
        //================================
        memset(&iwe, 0, sizeof(iwe));
	iwe.cmd = IWEVQUAL;
	iwe.u.qual.level = 0;
	iwe.u.qual.noise = 0;
	set_quality(pAdapter, &iwe.u.qual, pAdapter->ScanTab.BssEntry[i].Rssi);
	current_ev = IWE_STREAM_ADD_EVENT(info, current_ev, end_buf, &iwe, IW_EV_QUAL_LEN);
	if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

		//Encyption key
		//================================
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWENCODE;
		if (CAP_IS_PRIVACY_ON (pAdapter->ScanTab.BssEntry[i].CapabilityInfo ))
			iwe.u.data.flags =IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
		else
			iwe.u.data.flags = IW_ENCODE_DISABLED;

        previous_ev = current_ev;
        current_ev = IWE_STREAM_ADD_POINT(info, current_ev, end_buf,&iwe, (char *)pAdapter->SharedKey[BSS0][(iwe.u.data.flags & IW_ENCODE_INDEX)-1].Key);
        if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

		//Bit Rate
		//================================
		if (pAdapter->ScanTab.BssEntry[i].SupRateLen)
        {
            UCHAR tmpRate = pAdapter->ScanTab.BssEntry[i].SupRate[pAdapter->ScanTab.BssEntry[i].SupRateLen-1];
			memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = SIOCGIWRATE;
		current_val = current_ev + IW_EV_LCP_LEN;
            if (tmpRate == 0x82)
                iwe.u.bitrate.value =  1 * 1000000;
            else if (tmpRate == 0x84)
                iwe.u.bitrate.value =  2 * 1000000;
            else if (tmpRate == 0x8B)
                iwe.u.bitrate.value =  5.5 * 1000000;
            else if (tmpRate == 0x96)
                iwe.u.bitrate.value =  11 * 1000000;
            else
		    iwe.u.bitrate.value =  (tmpRate/2) * 1000000;

			if (tmpRate == 0x6c && pAdapter->ScanTab.BssEntry[i].HtCapabilityLen > 0)
			{
				int rate_count = sizeof(ralinkrate)/sizeof(__s32);
				HT_CAP_INFO capInfo = pAdapter->ScanTab.BssEntry[i].HtCapability.HtCapInfo;
				int shortGI = capInfo.ChannelWidth ? capInfo.ShortGIfor40 : capInfo.ShortGIfor20;
				int maxMCS = pAdapter->ScanTab.BssEntry[i].HtCapability.MCSSet[1] ?  15 : 7;
				int rate_index = 12 + ((UCHAR)capInfo.ChannelWidth * 24) + ((UCHAR)shortGI *48) + ((UCHAR)maxMCS);
				if (rate_index < 0)
					rate_index = 0;
				if (rate_index > rate_count)
					rate_index = rate_count;
				iwe.u.bitrate.value	=  ralinkrate[rate_index] * 500000;
			}

			iwe.u.bitrate.disabled = 0;
			current_val = IWE_STREAM_ADD_VALUE(info, current_ev,
				current_val, end_buf, &iwe,
			IW_EV_PARAM_LEN);

		if((current_val-current_ev)>IW_EV_LCP_LEN)
		current_ev = current_val;
		else
#if WIRELESS_EXT >= 17
                return -E2BIG;
#else
			    break;
#endif
        }

#ifdef IWEVGENIE
        //WPA IE
		if (pAdapter->ScanTab.BssEntry[i].WpaIE.IELen > 0)
        {
			memset(&iwe, 0, sizeof(iwe));
			memset(&custom[0], 0, MAX_CUSTOM_LEN);
			memcpy(custom, &(pAdapter->ScanTab.BssEntry[i].WpaIE.IE[0]),
						   pAdapter->ScanTab.BssEntry[i].WpaIE.IELen);
			iwe.cmd = IWEVGENIE;
			iwe.u.data.length = pAdapter->ScanTab.BssEntry[i].WpaIE.IELen;
			current_ev = IWE_STREAM_ADD_POINT(info, current_ev, end_buf, &iwe, custom);
			if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
                return -E2BIG;
#else
			    break;
#endif
		}

		//WPA2 IE
        if (pAdapter->ScanTab.BssEntry[i].RsnIE.IELen > 0)
        {
		memset(&iwe, 0, sizeof(iwe));
			memset(&custom[0], 0, MAX_CUSTOM_LEN);
			memcpy(custom, &(pAdapter->ScanTab.BssEntry[i].RsnIE.IE[0]),
						   pAdapter->ScanTab.BssEntry[i].RsnIE.IELen);
			iwe.cmd = IWEVGENIE;
			iwe.u.data.length = pAdapter->ScanTab.BssEntry[i].RsnIE.IELen;
			current_ev = IWE_STREAM_ADD_POINT(info, current_ev, end_buf, &iwe, custom);
			if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
                return -E2BIG;
#else
			    break;
#endif
        }
#else
        //WPA IE
		//================================
        if (pAdapter->ScanTab.BssEntry[i].WpaIE.IELen > 0)
        {
		NdisZeroMemory(&iwe, sizeof(iwe));
			memset(&custom[0], 0, MAX_CUSTOM_LEN);
		iwe.cmd = IWEVCUSTOM;
            iwe.u.data.length = (pAdapter->ScanTab.BssEntry[i].WpaIE.IELen * 2) + 7;
            NdisMoveMemory(custom, "wpa_ie=", 7);
            for (idx = 0; idx < pAdapter->ScanTab.BssEntry[i].WpaIE.IELen; idx++)
                sprintf(custom, "%s%02x", custom, pAdapter->ScanTab.BssEntry[i].WpaIE.IE[idx]);
            previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_POINT(info, current_ev, end_buf, &iwe,  custom);
            if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
                return -E2BIG;
#else
			    break;
#endif
        }

        //WPA2 IE
        if (pAdapter->ScanTab.BssEntry[i].RsnIE.IELen > 0)
        {
		NdisZeroMemory(&iwe, sizeof(iwe));
			memset(&custom[0], 0, MAX_CUSTOM_LEN);
		iwe.cmd = IWEVCUSTOM;
            iwe.u.data.length = (pAdapter->ScanTab.BssEntry[i].RsnIE.IELen * 2) + 7;
            NdisMoveMemory(custom, "rsn_ie=", 7);
			for (idx = 0; idx < pAdapter->ScanTab.BssEntry[i].RsnIE.IELen; idx++)
                sprintf(custom, "%s%02x", custom, pAdapter->ScanTab.BssEntry[i].RsnIE.IE[idx]);
            previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_POINT(info, current_ev, end_buf, &iwe,  custom);
            if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
                return -E2BIG;
#else
			    break;
#endif
        }
#endif // IWEVGENIE //
	}

	data->length = current_ev - extra;
    pAdapter->StaCfg.bScanReqIsFromWebUI = FALSE;
	DBGPRINT(RT_DEBUG_ERROR ,("===>rt_ioctl_giwscan. %d(%d) BSS returned, data->length = %d\n",i , pAdapter->ScanTab.BssNr, data->length));
	return 0;
}
#endif

int rt_ioctl_siwessid(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *data, char *essid)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	//check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
	DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
    }

	if (data->flags)
	{
		PSTRING	pSsidString = NULL;

		// Includes null character.
		if (data->length > (IW_ESSID_MAX_SIZE + 1))
			return -E2BIG;

		pSsidString = kmalloc(MAX_LEN_OF_SSID+1, MEM_ALLOC_FLAG);
		if (pSsidString)
        {
			NdisZeroMemory(pSsidString, MAX_LEN_OF_SSID+1);
			NdisMoveMemory(pSsidString, essid, data->length);
			if (Set_SSID_Proc(pAdapter, pSsidString) == FALSE)
				return -EINVAL;
		}
		else
			return -ENOMEM;
		}
	else
    {
		// ANY ssid
		if (Set_SSID_Proc(pAdapter, "") == FALSE)
			return -EINVAL;
    }
	return 0;
}

int rt_ioctl_giwessid(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *data, char *essid)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	data->flags = 1;
    if (MONITOR_ON(pAdapter))
    {
        data->length  = 0;
        return 0;
    }

	if (OPSTATUS_TEST_FLAG(pAdapter, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		DBGPRINT(RT_DEBUG_TRACE ,("MediaState is connected\n"));
		data->length = pAdapter->CommonCfg.SsidLen;
		memcpy(essid, pAdapter->CommonCfg.Ssid, pAdapter->CommonCfg.SsidLen);
	}
	else
	{//the ANY ssid was specified
		data->length  = 0;
		DBGPRINT(RT_DEBUG_TRACE ,("MediaState is not connected, ess\n"));
	}

	return 0;

}

int rt_ioctl_siwnickn(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *data, char *nickname)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

    //check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
        DBGPRINT(RT_DEBUG_TRACE ,("INFO::Network is down!\n"));
        return -ENETDOWN;
    }

	if (data->length > IW_ESSID_MAX_SIZE)
		return -EINVAL;

	memset(pAdapter->nickname, 0, IW_ESSID_MAX_SIZE + 1);
	memcpy(pAdapter->nickname, nickname, data->length);


	return 0;
}

int rt_ioctl_giwnickn(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *data, char *nickname)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	if (data->length > strlen((PSTRING) pAdapter->nickname) + 1)
		data->length = strlen((PSTRING) pAdapter->nickname) + 1;
	if (data->length > 0) {
		memcpy(nickname, pAdapter->nickname, data->length-1);
		nickname[data->length-1] = '\0';
	}
	return 0;
}

int rt_ioctl_siwrts(struct net_device *dev,
		       struct iw_request_info *info,
		       struct iw_param *rts, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	u16 val;

    //check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
        DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
        return -ENETDOWN;
    }

	if (rts->disabled)
		val = MAX_RTS_THRESHOLD;
	else if (rts->value < 0 || rts->value > MAX_RTS_THRESHOLD)
		return -EINVAL;
	else if (rts->value == 0)
	    val = MAX_RTS_THRESHOLD;
	else
		val = rts->value;

	if (val != pAdapter->CommonCfg.RtsThreshold)
		pAdapter->CommonCfg.RtsThreshold = val;

	return 0;
}

int rt_ioctl_giwrts(struct net_device *dev,
		       struct iw_request_info *info,
		       struct iw_param *rts, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	rts->value = pAdapter->CommonCfg.RtsThreshold;
	rts->disabled = (rts->value == MAX_RTS_THRESHOLD);
	rts->fixed = 1;

	return 0;
}

int rt_ioctl_siwfrag(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_param *frag, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	u16 val;

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	if (frag->disabled)
		val = MAX_FRAG_THRESHOLD;
	else if (frag->value >= MIN_FRAG_THRESHOLD || frag->value <= MAX_FRAG_THRESHOLD)
        val = __cpu_to_le16(frag->value & ~0x1); /* even numbers only */
	else if (frag->value == 0)
	    val = MAX_FRAG_THRESHOLD;
	else
		return -EINVAL;

	pAdapter->CommonCfg.FragmentThreshold = val;
	return 0;
}

int rt_ioctl_giwfrag(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_param *frag, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	frag->value = pAdapter->CommonCfg.FragmentThreshold;
	frag->disabled = (frag->value == MAX_FRAG_THRESHOLD);
	frag->fixed = 1;

	return 0;
}

#define MAX_WEP_KEY_SIZE 13
#define MIN_WEP_KEY_SIZE 5
int rt_ioctl_siwencode(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *erq, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	if ((erq->length == 0) &&
        (erq->flags & IW_ENCODE_DISABLED))
	{
		pAdapter->StaCfg.PairCipher = Ndis802_11WEPDisabled;
		pAdapter->StaCfg.GroupCipher = Ndis802_11WEPDisabled;
		pAdapter->StaCfg.WepStatus = Ndis802_11WEPDisabled;
        pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
        pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeOpen;
        goto done;
	}
	else if (erq->flags & IW_ENCODE_RESTRICTED || erq->flags & IW_ENCODE_OPEN)
	{
	    //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
		STA_PORT_SECURED(pAdapter);
		pAdapter->StaCfg.PairCipher = Ndis802_11WEPEnabled;
		pAdapter->StaCfg.GroupCipher = Ndis802_11WEPEnabled;
		pAdapter->StaCfg.WepStatus = Ndis802_11WEPEnabled;
        pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
		if (erq->flags & IW_ENCODE_RESTRICTED)
			pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeShared;
	else
			pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeOpen;
	}

    if (erq->length > 0)
	{
		int keyIdx = (erq->flags & IW_ENCODE_INDEX) - 1;
		/* Check the size of the key */
		if (erq->length > MAX_WEP_KEY_SIZE)
		{
			return -EINVAL;
		}
		/* Check key index */
		if ((keyIdx < 0) || (keyIdx >= NR_WEP_KEYS))
        {
            DBGPRINT(RT_DEBUG_TRACE ,("==>rt_ioctl_siwencode::Wrong keyIdx=%d! Using default key instead (%d)\n",
                                        keyIdx, pAdapter->StaCfg.DefaultKeyId));

            //Using default key
			keyIdx = pAdapter->StaCfg.DefaultKeyId;
        }
		else
			pAdapter->StaCfg.DefaultKeyId = keyIdx;

        NdisZeroMemory(pAdapter->SharedKey[BSS0][keyIdx].Key,  16);

		if (erq->length == MAX_WEP_KEY_SIZE)
        {
			pAdapter->SharedKey[BSS0][keyIdx].KeyLen = MAX_WEP_KEY_SIZE;
            pAdapter->SharedKey[BSS0][keyIdx].CipherAlg = CIPHER_WEP128;
		}
		else if (erq->length == MIN_WEP_KEY_SIZE)
        {
            pAdapter->SharedKey[BSS0][keyIdx].KeyLen = MIN_WEP_KEY_SIZE;
            pAdapter->SharedKey[BSS0][keyIdx].CipherAlg = CIPHER_WEP64;
		}
		else
			/* Disable the key */
			pAdapter->SharedKey[BSS0][keyIdx].KeyLen = 0;

		/* Check if the key is not marked as invalid */
		if(!(erq->flags & IW_ENCODE_NOKEY))
		{
			/* Copy the key in the driver */
			NdisMoveMemory(pAdapter->SharedKey[BSS0][keyIdx].Key, extra, erq->length);
        }
	}
    else
			{
		/* Do we want to just set the transmit key index ? */
		int index = (erq->flags & IW_ENCODE_INDEX) - 1;
		if ((index >= 0) && (index < 4))
        {
			pAdapter->StaCfg.DefaultKeyId = index;
            }
        else
			/* Don't complain if only change the mode */
		if (!(erq->flags & IW_ENCODE_MODE))
			return -EINVAL;
	}

done:
    DBGPRINT(RT_DEBUG_TRACE ,("==>rt_ioctl_siwencode::erq->flags=%x\n",erq->flags));
	DBGPRINT(RT_DEBUG_TRACE ,("==>rt_ioctl_siwencode::AuthMode=%x\n",pAdapter->StaCfg.AuthMode));
	DBGPRINT(RT_DEBUG_TRACE ,("==>rt_ioctl_siwencode::DefaultKeyId=%x, KeyLen = %d\n",pAdapter->StaCfg.DefaultKeyId , pAdapter->SharedKey[BSS0][pAdapter->StaCfg.DefaultKeyId].KeyLen));
	DBGPRINT(RT_DEBUG_TRACE ,("==>rt_ioctl_siwencode::WepStatus=%x\n",pAdapter->StaCfg.WepStatus));
	return 0;
}

int
rt_ioctl_giwencode(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *erq, char *key)
{
	int kid;
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
	}

	kid = erq->flags & IW_ENCODE_INDEX;
	DBGPRINT(RT_DEBUG_TRACE, ("===>rt_ioctl_giwencode %d\n", erq->flags & IW_ENCODE_INDEX));

	if (pAdapter->StaCfg.WepStatus == Ndis802_11WEPDisabled)
	{
		erq->length = 0;
		erq->flags = IW_ENCODE_DISABLED;
	}
	else if ((kid > 0) && (kid <=4))
	{
		// copy wep key
		erq->flags = kid ;			/* NB: base 1 */
		if (erq->length > pAdapter->SharedKey[BSS0][kid-1].KeyLen)
			erq->length = pAdapter->SharedKey[BSS0][kid-1].KeyLen;
		memcpy(key, pAdapter->SharedKey[BSS0][kid-1].Key, erq->length);
		//if ((kid == pAdapter->PortCfg.DefaultKeyId))
		//erq->flags |= IW_ENCODE_ENABLED;	/* XXX */
		if (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeShared)
			erq->flags |= IW_ENCODE_RESTRICTED;		/* XXX */
		else
			erq->flags |= IW_ENCODE_OPEN;		/* XXX */

	}
	else if (kid == 0)
	{
		if (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeShared)
			erq->flags |= IW_ENCODE_RESTRICTED;		/* XXX */
		else
			erq->flags |= IW_ENCODE_OPEN;		/* XXX */
		erq->length = pAdapter->SharedKey[BSS0][pAdapter->StaCfg.DefaultKeyId].KeyLen;
		memcpy(key, pAdapter->SharedKey[BSS0][pAdapter->StaCfg.DefaultKeyId].Key, erq->length);
		// copy default key ID
		if (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeShared)
			erq->flags |= IW_ENCODE_RESTRICTED;		/* XXX */
		else
			erq->flags |= IW_ENCODE_OPEN;		/* XXX */
		erq->flags = pAdapter->StaCfg.DefaultKeyId + 1;			/* NB: base 1 */
		erq->flags |= IW_ENCODE_ENABLED;	/* XXX */
	}

	return 0;

}

static int
rt_ioctl_setparam(struct net_device *dev, struct iw_request_info *info,
			 void *w, char *extra)
{
	PRTMP_ADAPTER pAdapter;
	POS_COOKIE pObj;
	PSTRING this_char = extra;
	PSTRING value;
	int  Status=0;

	pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	pObj = (POS_COOKIE) pAdapter->OS_Cookie;
	{
		pObj->ioctl_if_type = INT_MAIN;
        pObj->ioctl_if = MAIN_MBSSID;
	}

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
			return -ENETDOWN;
	}

	if (!*this_char)
		return -EINVAL;

	if ((value = rtstrchr(this_char, '=')) != NULL)
	    *value++ = 0;

	if (!value && (strcmp(this_char, "SiteSurvey") != 0))
	    return -EINVAL;
	else
		goto SET_PROC;

	// reject setting nothing besides ANY ssid(ssidLen=0)
    if (!*value && (strcmp(this_char, "SSID") != 0))
        return -EINVAL;

SET_PROC:
	for (PRTMP_PRIVATE_SET_PROC = RTMP_PRIVATE_SUPPORT_PROC; PRTMP_PRIVATE_SET_PROC->name; PRTMP_PRIVATE_SET_PROC++)
	{
	    if (strcmp(this_char, PRTMP_PRIVATE_SET_PROC->name) == 0)
	    {
	        if(!PRTMP_PRIVATE_SET_PROC->set_proc(pAdapter, value))
	        {	//FALSE:Set private failed then return Invalid argument
			    Status = -EINVAL;
	        }
		    break;	//Exit for loop.
	    }
	}

	if(PRTMP_PRIVATE_SET_PROC->name == NULL)
	{  //Not found argument
	    Status = -EINVAL;
	    DBGPRINT(RT_DEBUG_TRACE, ("===>rt_ioctl_setparam:: (iwpriv) Not Support Set Command [%s=%s]\n", this_char, value));
	}

    return Status;
}


static int
rt_private_get_statistics(struct net_device *dev, struct iw_request_info *info,
		struct iw_point *wrq, char *extra)
{
	INT				Status = 0;
    PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);

    if (extra == NULL)
    {
        wrq->length = 0;
        return -EIO;
    }

    memset(extra, 0x00, IW_PRIV_SIZE_MASK);
    sprintf(extra, "\n\n");

#ifdef RALINK_ATE
	if (ATE_ON(pAd))
	{
	    sprintf(extra+strlen(extra), "Tx success                      = %ld\n", (ULONG)pAd->ate.TxDoneCount);
	    //sprintf(extra+strlen(extra), "Tx success without retry        = %ld\n", (ULONG)pAd->ate.TxDoneCount);
	}
	else
#endif // RALINK_ATE //
	{
    sprintf(extra+strlen(extra), "Tx success                      = %ld\n", (ULONG)pAd->WlanCounters.TransmittedFragmentCount.QuadPart);
    sprintf(extra+strlen(extra), "Tx success without retry        = %ld\n", (ULONG)pAd->WlanCounters.TransmittedFragmentCount.QuadPart - (ULONG)pAd->WlanCounters.RetryCount.QuadPart);
	}
    sprintf(extra+strlen(extra), "Tx success after retry          = %ld\n", (ULONG)pAd->WlanCounters.RetryCount.QuadPart);
    sprintf(extra+strlen(extra), "Tx fail to Rcv ACK after retry  = %ld\n", (ULONG)pAd->WlanCounters.FailedCount.QuadPart);
    sprintf(extra+strlen(extra), "RTS Success Rcv CTS             = %ld\n", (ULONG)pAd->WlanCounters.RTSSuccessCount.QuadPart);
    sprintf(extra+strlen(extra), "RTS Fail Rcv CTS                = %ld\n", (ULONG)pAd->WlanCounters.RTSFailureCount.QuadPart);

    sprintf(extra+strlen(extra), "Rx success                      = %ld\n", (ULONG)pAd->WlanCounters.ReceivedFragmentCount.QuadPart);
    sprintf(extra+strlen(extra), "Rx with CRC                     = %ld\n", (ULONG)pAd->WlanCounters.FCSErrorCount.QuadPart);
    sprintf(extra+strlen(extra), "Rx drop due to out of resource  = %ld\n", (ULONG)pAd->Counters8023.RxNoBuffer);
    sprintf(extra+strlen(extra), "Rx duplicate frame              = %ld\n", (ULONG)pAd->WlanCounters.FrameDuplicateCount.QuadPart);

    sprintf(extra+strlen(extra), "False CCA (one second)          = %ld\n", (ULONG)pAd->RalinkCounters.OneSecFalseCCACnt);

#ifdef RALINK_ATE
	if (ATE_ON(pAd))
	{
		if (pAd->ate.RxAntennaSel == 0)
		{
		sprintf(extra+strlen(extra), "RSSI-A                          = %ld\n", (LONG)(pAd->ate.LastRssi0 - pAd->BbpRssiToDbmDelta));
			sprintf(extra+strlen(extra), "RSSI-B (if available)           = %ld\n", (LONG)(pAd->ate.LastRssi1 - pAd->BbpRssiToDbmDelta));
			sprintf(extra+strlen(extra), "RSSI-C (if available)           = %ld\n\n", (LONG)(pAd->ate.LastRssi2 - pAd->BbpRssiToDbmDelta));
		}
		else
		{
		sprintf(extra+strlen(extra), "RSSI                            = %ld\n", (LONG)(pAd->ate.LastRssi0 - pAd->BbpRssiToDbmDelta));
		}
	}
	else
#endif // RALINK_ATE //
	{
	sprintf(extra+strlen(extra), "RSSI-A                          = %ld\n", (LONG)(pAd->StaCfg.RssiSample.LastRssi0 - pAd->BbpRssiToDbmDelta));
        sprintf(extra+strlen(extra), "RSSI-B (if available)           = %ld\n", (LONG)(pAd->StaCfg.RssiSample.LastRssi1 - pAd->BbpRssiToDbmDelta));
        sprintf(extra+strlen(extra), "RSSI-C (if available)           = %ld\n\n", (LONG)(pAd->StaCfg.RssiSample.LastRssi2 - pAd->BbpRssiToDbmDelta));
	}
#ifdef WPA_SUPPLICANT_SUPPORT
    sprintf(extra+strlen(extra), "WpaSupplicantUP                 = %d\n\n", pAd->StaCfg.WpaSupplicantUP);
#endif // WPA_SUPPLICANT_SUPPORT //



    wrq->length = strlen(extra) + 1; // 1: size of '\0'
    DBGPRINT(RT_DEBUG_TRACE, ("<== rt_private_get_statistics, wrq->length = %d\n", wrq->length));

    return Status;
}

#ifdef DOT11_N_SUPPORT
void	getBaInfo(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			pOutBuf)
{
	INT i, j;
	BA_ORI_ENTRY *pOriBAEntry;
	BA_REC_ENTRY *pRecBAEntry;

	for (i=0; i<MAX_LEN_OF_MAC_TABLE; i++)
	{
		PMAC_TABLE_ENTRY pEntry = &pAd->MacTab.Content[i];
		if (((pEntry->ValidAsCLI || pEntry->ValidAsApCli) && (pEntry->Sst == SST_ASSOC))
			|| (pEntry->ValidAsWDS) || (pEntry->ValidAsMesh))
		{
			sprintf(pOutBuf, "%s\n%02X:%02X:%02X:%02X:%02X:%02X (Aid = %d) (AP) -\n",
                pOutBuf,
				pEntry->Addr[0], pEntry->Addr[1], pEntry->Addr[2],
				pEntry->Addr[3], pEntry->Addr[4], pEntry->Addr[5], pEntry->Aid);

			sprintf(pOutBuf, "%s[Recipient]\n", pOutBuf);
			for (j=0; j < NUM_OF_TID; j++)
			{
				if (pEntry->BARecWcidArray[j] != 0)
				{
					pRecBAEntry =&pAd->BATable.BARecEntry[pEntry->BARecWcidArray[j]];
					sprintf(pOutBuf, "%sTID=%d, BAWinSize=%d, LastIndSeq=%d, ReorderingPkts=%d\n", pOutBuf, j, pRecBAEntry->BAWinSize, pRecBAEntry->LastIndSeq, pRecBAEntry->list.qlen);
				}
			}
			sprintf(pOutBuf, "%s\n", pOutBuf);

			sprintf(pOutBuf, "%s[Originator]\n", pOutBuf);
			for (j=0; j < NUM_OF_TID; j++)
			{
				if (pEntry->BAOriWcidArray[j] != 0)
				{
					pOriBAEntry =&pAd->BATable.BAOriEntry[pEntry->BAOriWcidArray[j]];
					sprintf(pOutBuf, "%sTID=%d, BAWinSize=%d, StartSeq=%d, CurTxSeq=%d\n", pOutBuf, j, pOriBAEntry->BAWinSize, pOriBAEntry->Sequence, pEntry->TxSeq[j]);
				}
			}
			sprintf(pOutBuf, "%s\n\n", pOutBuf);
		}
        if (strlen(pOutBuf) > (IW_PRIV_SIZE_MASK - 30))
                break;
	}

	return;
}
#endif // DOT11_N_SUPPORT //

static int
rt_private_show(struct net_device *dev, struct iw_request_info *info,
		struct iw_point *wrq, PSTRING extra)
{
	INT				Status = 0;
	PRTMP_ADAPTER   pAd;
	POS_COOKIE		pObj;
	u32             subcmd = wrq->flags;

	pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
	if (pAd == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	pObj = (POS_COOKIE) pAd->OS_Cookie;
	if (extra == NULL)
	{
		wrq->length = 0;
		return -EIO;
	}
	memset(extra, 0x00, IW_PRIV_SIZE_MASK);

	{
		pObj->ioctl_if_type = INT_MAIN;
		pObj->ioctl_if = MAIN_MBSSID;
	}

    switch(subcmd)
    {

        case SHOW_CONN_STATUS:
            if (MONITOR_ON(pAd))
            {
#ifdef DOT11_N_SUPPORT
                if (pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED &&
                    pAd->CommonCfg.RegTransmitSetting.field.BW)
                    sprintf(extra, "Monitor Mode(CentralChannel %d)\n", pAd->CommonCfg.CentralChannel);
                else
#endif // DOT11_N_SUPPORT //
                    sprintf(extra, "Monitor Mode(Channel %d)\n", pAd->CommonCfg.Channel);
            }
            else
            {
                if (pAd->IndicateMediaState == NdisMediaStateConnected)
		{
		    if (INFRA_ON(pAd))
                    {
                    sprintf(extra, "Connected(AP: %s[%02X:%02X:%02X:%02X:%02X:%02X])\n",
                                    pAd->CommonCfg.Ssid,
                                    pAd->CommonCfg.Bssid[0],
                                    pAd->CommonCfg.Bssid[1],
                                    pAd->CommonCfg.Bssid[2],
                                    pAd->CommonCfg.Bssid[3],
                                    pAd->CommonCfg.Bssid[4],
                                    pAd->CommonCfg.Bssid[5]);
			DBGPRINT(RT_DEBUG_TRACE ,("Ssid=%s ,Ssidlen = %d\n",pAd->CommonCfg.Ssid, pAd->CommonCfg.SsidLen));
		}
                    else if (ADHOC_ON(pAd))
                        sprintf(extra, "Connected\n");
		}
		else
		{
		    sprintf(extra, "Disconnected\n");
			DBGPRINT(RT_DEBUG_TRACE ,("ConnStatus is not connected\n"));
		}
            }
            wrq->length = strlen(extra) + 1; // 1: size of '\0'
            break;
        case SHOW_DRVIER_VERION:
            sprintf(extra, "Driver version-%s, %s %s\n", STA_DRIVER_VERSION, __DATE__, __TIME__ );
            wrq->length = strlen(extra) + 1; // 1: size of '\0'
            break;
#ifdef DOT11_N_SUPPORT
        case SHOW_BA_INFO:
            getBaInfo(pAd, extra);
            wrq->length = strlen(extra) + 1; // 1: size of '\0'
            break;
#endif // DOT11_N_SUPPORT //
		case SHOW_DESC_INFO:
			{
				Show_DescInfo_Proc(pAd, NULL);
				wrq->length = 0; // 1: size of '\0'
			}
			break;
        case RAIO_OFF:
            if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
            {
                if (pAd->Mlme.CntlMachine.CurrState != CNTL_IDLE)
		        {
		            RTMP_MLME_RESET_STATE_MACHINE(pAd);
		            DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME busy, reset MLME state machine !!!\n"));
		        }
            }
            pAd->StaCfg.bSwRadio = FALSE;
            if (pAd->StaCfg.bRadio != (pAd->StaCfg.bHwRadio && pAd->StaCfg.bSwRadio))
            {
                pAd->StaCfg.bRadio = (pAd->StaCfg.bHwRadio && pAd->StaCfg.bSwRadio);
                if (pAd->StaCfg.bRadio == FALSE)
                {
                    MlmeRadioOff(pAd);
                    // Update extra information
					pAd->ExtraInfo = SW_RADIO_OFF;
                }
            }
            sprintf(extra, "Radio Off\n");
            wrq->length = strlen(extra) + 1; // 1: size of '\0'
            break;
        case RAIO_ON:
            pAd->StaCfg.bSwRadio = TRUE;
            //if (pAd->StaCfg.bRadio != (pAd->StaCfg.bHwRadio && pAd->StaCfg.bSwRadio))
            {
                pAd->StaCfg.bRadio = (pAd->StaCfg.bHwRadio && pAd->StaCfg.bSwRadio);
                if (pAd->StaCfg.bRadio == TRUE)
                {
                    MlmeRadioOn(pAd);
                    // Update extra information
					pAd->ExtraInfo = EXTRA_INFO_CLEAR;
                }
            }
            sprintf(extra, "Radio On\n");
            wrq->length = strlen(extra) + 1; // 1: size of '\0'
            break;


#ifdef QOS_DLS_SUPPORT
		case SHOW_DLS_ENTRY_INFO:
			{
				Set_DlsEntryInfo_Display_Proc(pAd, NULL);
				wrq->length = 0; // 1: size of '\0'
			}
			break;
#endif // QOS_DLS_SUPPORT //

		case SHOW_CFG_VALUE:
			{
				Status = RTMPShowCfgValue(pAd, (PSTRING) wrq->pointer, extra);
				if (Status == 0)
					wrq->length = strlen(extra) + 1; // 1: size of '\0'
			}
			break;
		case SHOW_ADHOC_ENTRY_INFO:
			Show_Adhoc_MacTable_Proc(pAd, extra);
			wrq->length = strlen(extra) + 1; // 1: size of '\0'
			break;
        default:
            DBGPRINT(RT_DEBUG_TRACE, ("%s - unknow subcmd = %d\n", __FUNCTION__, subcmd));
            break;
    }

    return Status;
}

#ifdef SIOCSIWMLME
int rt_ioctl_siwmlme(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu,
			   char *extra)
{
	PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_mlme *pMlme = (struct iw_mlme *)wrqu->data.pointer;
	MLME_QUEUE_ELEM				MsgElem;
	MLME_DISASSOC_REQ_STRUCT	DisAssocReq;
	MLME_DEAUTH_REQ_STRUCT      DeAuthReq;

	DBGPRINT(RT_DEBUG_TRACE, ("====> %s\n", __FUNCTION__));

	if (pMlme == NULL)
		return -EINVAL;

	switch(pMlme->cmd)
	{
#ifdef IW_MLME_DEAUTH
		case IW_MLME_DEAUTH:
			DBGPRINT(RT_DEBUG_TRACE, ("====> %s - IW_MLME_DEAUTH\n", __FUNCTION__));
			COPY_MAC_ADDR(DeAuthReq.Addr, pAd->CommonCfg.Bssid);
			DeAuthReq.Reason = pMlme->reason_code;
			MsgElem.MsgLen = sizeof(MLME_DEAUTH_REQ_STRUCT);
			NdisMoveMemory(MsgElem.Msg, &DeAuthReq, sizeof(MLME_DEAUTH_REQ_STRUCT));
			MlmeDeauthReqAction(pAd, &MsgElem);
			if (INFRA_ON(pAd))
			{
			    LinkDown(pAd, FALSE);
			    pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
			}
			break;
#endif // IW_MLME_DEAUTH //
#ifdef IW_MLME_DISASSOC
		case IW_MLME_DISASSOC:
			DBGPRINT(RT_DEBUG_TRACE, ("====> %s - IW_MLME_DISASSOC\n", __FUNCTION__));
			COPY_MAC_ADDR(DisAssocReq.Addr, pAd->CommonCfg.Bssid);
			DisAssocReq.Reason =  pMlme->reason_code;

			MsgElem.Machine = ASSOC_STATE_MACHINE;
			MsgElem.MsgType = MT2_MLME_DISASSOC_REQ;
			MsgElem.MsgLen = sizeof(MLME_DISASSOC_REQ_STRUCT);
			NdisMoveMemory(MsgElem.Msg, &DisAssocReq, sizeof(MLME_DISASSOC_REQ_STRUCT));

			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
			MlmeDisassocReqAction(pAd, &MsgElem);
			break;
#endif // IW_MLME_DISASSOC //
		default:
			DBGPRINT(RT_DEBUG_TRACE, ("====> %s - Unknow Command\n", __FUNCTION__));
			break;
	}

	return 0;
}
#endif // SIOCSIWMLME //

#if WIRELESS_EXT > 17
int rt_ioctl_siwauth(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	PRTMP_ADAPTER   pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_param *param = &wrqu->param;

    //check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
	}
	switch (param->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
            if (param->value == IW_AUTH_WPA_VERSION_WPA)
            {
                pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPAPSK;
				if (pAdapter->StaCfg.BssType == BSS_ADHOC)
					pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPANone;
            }
            else if (param->value == IW_AUTH_WPA_VERSION_WPA2)
                pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPA2PSK;

            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_WPA_VERSION - param->value = %d!\n", __FUNCTION__, param->value));
            break;
	case IW_AUTH_CIPHER_PAIRWISE:
            if (param->value == IW_AUTH_CIPHER_NONE)
            {
                pAdapter->StaCfg.WepStatus = Ndis802_11WEPDisabled;
                pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
                pAdapter->StaCfg.PairCipher = Ndis802_11WEPDisabled;
            }
            else if (param->value == IW_AUTH_CIPHER_WEP40 ||
                     param->value == IW_AUTH_CIPHER_WEP104)
            {
                pAdapter->StaCfg.WepStatus = Ndis802_11WEPEnabled;
                pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
                pAdapter->StaCfg.PairCipher = Ndis802_11WEPEnabled;
#ifdef WPA_SUPPLICANT_SUPPORT
                pAdapter->StaCfg.IEEE8021X = FALSE;
#endif // WPA_SUPPLICANT_SUPPORT //
            }
            else if (param->value == IW_AUTH_CIPHER_TKIP)
            {
                pAdapter->StaCfg.WepStatus = Ndis802_11Encryption2Enabled;
                pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
                pAdapter->StaCfg.PairCipher = Ndis802_11Encryption2Enabled;
            }
            else if (param->value == IW_AUTH_CIPHER_CCMP)
            {
                pAdapter->StaCfg.WepStatus = Ndis802_11Encryption3Enabled;
                pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
                pAdapter->StaCfg.PairCipher = Ndis802_11Encryption3Enabled;
            }
            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_CIPHER_PAIRWISE - param->value = %d!\n", __FUNCTION__, param->value));
            break;
	case IW_AUTH_CIPHER_GROUP:
            if (param->value == IW_AUTH_CIPHER_NONE)
            {
                pAdapter->StaCfg.GroupCipher = Ndis802_11WEPDisabled;
            }
            else if (param->value == IW_AUTH_CIPHER_WEP40 ||
                     param->value == IW_AUTH_CIPHER_WEP104)
            {
                pAdapter->StaCfg.GroupCipher = Ndis802_11WEPEnabled;
            }
            else if (param->value == IW_AUTH_CIPHER_TKIP)
            {
                pAdapter->StaCfg.GroupCipher = Ndis802_11Encryption2Enabled;
            }
            else if (param->value == IW_AUTH_CIPHER_CCMP)
            {
                pAdapter->StaCfg.GroupCipher = Ndis802_11Encryption3Enabled;
            }
            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_CIPHER_GROUP - param->value = %d!\n", __FUNCTION__, param->value));
            break;
	case IW_AUTH_KEY_MGMT:
            if (param->value == IW_AUTH_KEY_MGMT_802_1X)
            {
                if (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK)
                {
                    pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPA;
#ifdef WPA_SUPPLICANT_SUPPORT
                    pAdapter->StaCfg.IEEE8021X = FALSE;
#endif // WPA_SUPPLICANT_SUPPORT //
                }
                else if (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
                {
                    pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPA2;
#ifdef WPA_SUPPLICANT_SUPPORT
                    pAdapter->StaCfg.IEEE8021X = FALSE;
#endif // WPA_SUPPLICANT_SUPPORT //
                }
#ifdef WPA_SUPPLICANT_SUPPORT
                else
                    // WEP 1x
                    pAdapter->StaCfg.IEEE8021X = TRUE;
#endif // WPA_SUPPLICANT_SUPPORT //
            }
            else if (param->value == 0)
            {
                //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
				STA_PORT_SECURED(pAdapter);
            }
            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_KEY_MGMT - param->value = %d!\n", __FUNCTION__, param->value));
            break;
	case IW_AUTH_RX_UNENCRYPTED_EAPOL:
            break;
	case IW_AUTH_PRIVACY_INVOKED:
            /*if (param->value == 0)
			{
                pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeOpen;
                pAdapter->StaCfg.WepStatus = Ndis802_11WEPDisabled;
                pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
                pAdapter->StaCfg.PairCipher = Ndis802_11WEPDisabled;
		    pAdapter->StaCfg.GroupCipher = Ndis802_11WEPDisabled;
            }*/
            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_PRIVACY_INVOKED - param->value = %d!\n", __FUNCTION__, param->value));
		break;
	case IW_AUTH_DROP_UNENCRYPTED:
            if (param->value != 0)
                pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_NOT_SECURED;
			else
			{
                //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
				STA_PORT_SECURED(pAdapter);
			}
            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_WPA_VERSION - param->value = %d!\n", __FUNCTION__, param->value));
		break;
	case IW_AUTH_80211_AUTH_ALG:
			if (param->value & IW_AUTH_ALG_SHARED_KEY)
            {
				pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeShared;
			}
            else if (param->value & IW_AUTH_ALG_OPEN_SYSTEM)
            {
				pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeOpen;
			}
            else
				return -EINVAL;
            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_80211_AUTH_ALG - param->value = %d!\n", __FUNCTION__, param->value));
			break;
	case IW_AUTH_WPA_ENABLED:
		DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_WPA_ENABLED - Driver supports WPA!(param->value = %d)\n", __FUNCTION__, param->value));
		break;
	default:
		return -EOPNOTSUPP;
}

	return 0;
}

int rt_ioctl_giwauth(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	PRTMP_ADAPTER   pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_param *param = &wrqu->param;

    //check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
    }

	switch (param->flags & IW_AUTH_INDEX) {
	case IW_AUTH_DROP_UNENCRYPTED:
        param->value = (pAdapter->StaCfg.WepStatus == Ndis802_11WEPDisabled) ? 0 : 1;
		break;

	case IW_AUTH_80211_AUTH_ALG:
        param->value = (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeShared) ? IW_AUTH_ALG_SHARED_KEY : IW_AUTH_ALG_OPEN_SYSTEM;
		break;

	case IW_AUTH_WPA_ENABLED:
		param->value = (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA) ? 1 : 0;
		break;

	default:
		return -EOPNOTSUPP;
	}
    DBGPRINT(RT_DEBUG_TRACE, ("rt_ioctl_giwauth::param->value = %d!\n", param->value));
	return 0;
}

void fnSetCipherKey(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  INT             keyIdx,
    IN  UCHAR           CipherAlg,
    IN  BOOLEAN         bGTK,
    IN  struct iw_encode_ext *ext)
{
    NdisZeroMemory(&pAdapter->SharedKey[BSS0][keyIdx], sizeof(CIPHER_KEY));
    pAdapter->SharedKey[BSS0][keyIdx].KeyLen = LEN_TKIP_EK;
    NdisMoveMemory(pAdapter->SharedKey[BSS0][keyIdx].Key, ext->key, LEN_TKIP_EK);
    NdisMoveMemory(pAdapter->SharedKey[BSS0][keyIdx].TxMic, ext->key + LEN_TKIP_EK, LEN_TKIP_TXMICK);
    NdisMoveMemory(pAdapter->SharedKey[BSS0][keyIdx].RxMic, ext->key + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
    pAdapter->SharedKey[BSS0][keyIdx].CipherAlg = CipherAlg;

    // Update group key information to ASIC Shared Key Table
	AsicAddSharedKeyEntry(pAdapter,
						  BSS0,
						  keyIdx,
						  pAdapter->SharedKey[BSS0][keyIdx].CipherAlg,
						  pAdapter->SharedKey[BSS0][keyIdx].Key,
						  pAdapter->SharedKey[BSS0][keyIdx].TxMic,
						  pAdapter->SharedKey[BSS0][keyIdx].RxMic);

    if (bGTK)
        // Update ASIC WCID attribute table and IVEIV table
	RTMPAddWcidAttributeEntry(pAdapter,
							  BSS0,
							  keyIdx,
							  pAdapter->SharedKey[BSS0][keyIdx].CipherAlg,
							  NULL);
    else
        // Update ASIC WCID attribute table and IVEIV table
	RTMPAddWcidAttributeEntry(pAdapter,
							  BSS0,
							  keyIdx,
							  pAdapter->SharedKey[BSS0][keyIdx].CipherAlg,
							  &pAdapter->MacTab.Content[BSSID_WCID]);
}

int rt_ioctl_siwencodeext(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu,
			   char *extra)
			{
    PRTMP_ADAPTER   pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_point *encoding = &wrqu->encoding;
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
    int keyIdx, alg = ext->alg;

    //check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
	}

    if (encoding->flags & IW_ENCODE_DISABLED)
	{
        keyIdx = (encoding->flags & IW_ENCODE_INDEX) - 1;
        // set BSSID wcid entry of the Pair-wise Key table as no-security mode
	    AsicRemovePairwiseKeyEntry(pAdapter, BSS0, BSSID_WCID);
        pAdapter->SharedKey[BSS0][keyIdx].KeyLen = 0;
		pAdapter->SharedKey[BSS0][keyIdx].CipherAlg = CIPHER_NONE;
		AsicRemoveSharedKeyEntry(pAdapter, 0, (UCHAR)keyIdx);
        NdisZeroMemory(&pAdapter->SharedKey[BSS0][keyIdx], sizeof(CIPHER_KEY));
        DBGPRINT(RT_DEBUG_TRACE, ("%s::Remove all keys!(encoding->flags = %x)\n", __FUNCTION__, encoding->flags));
    }
					else
    {
        // Get Key Index and convet to our own defined key index
	keyIdx = (encoding->flags & IW_ENCODE_INDEX) - 1;
	if((keyIdx < 0) || (keyIdx >= NR_WEP_KEYS))
		return -EINVAL;

        if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
        {
            pAdapter->StaCfg.DefaultKeyId = keyIdx;
            DBGPRINT(RT_DEBUG_TRACE, ("%s::DefaultKeyId = %d\n", __FUNCTION__, pAdapter->StaCfg.DefaultKeyId));
        }

        switch (alg) {
		case IW_ENCODE_ALG_NONE:
                DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_ENCODE_ALG_NONE\n", __FUNCTION__));
			break;
		case IW_ENCODE_ALG_WEP:
                DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_ENCODE_ALG_WEP - ext->key_len = %d, keyIdx = %d\n", __FUNCTION__, ext->key_len, keyIdx));
			if (ext->key_len == MAX_WEP_KEY_SIZE)
                {
				pAdapter->SharedKey[BSS0][keyIdx].KeyLen = MAX_WEP_KEY_SIZE;
                    pAdapter->SharedKey[BSS0][keyIdx].CipherAlg = CIPHER_WEP128;
				}
			else if (ext->key_len == MIN_WEP_KEY_SIZE)
                {
                    pAdapter->SharedKey[BSS0][keyIdx].KeyLen = MIN_WEP_KEY_SIZE;
                    pAdapter->SharedKey[BSS0][keyIdx].CipherAlg = CIPHER_WEP64;
			}
			else
                    return -EINVAL;

                NdisZeroMemory(pAdapter->SharedKey[BSS0][keyIdx].Key,  16);
			    NdisMoveMemory(pAdapter->SharedKey[BSS0][keyIdx].Key, ext->key, ext->key_len);

				if (pAdapter->StaCfg.GroupCipher == Ndis802_11GroupWEP40Enabled ||
					pAdapter->StaCfg.GroupCipher == Ndis802_11GroupWEP104Enabled)
				{
					// Set Group key material to Asic
					AsicAddSharedKeyEntry(pAdapter, BSS0, keyIdx, pAdapter->SharedKey[BSS0][keyIdx].CipherAlg, pAdapter->SharedKey[BSS0][keyIdx].Key, NULL, NULL);
					// Update WCID attribute table and IVEIV table for this group key table
					RTMPAddWcidAttributeEntry(pAdapter, BSS0, keyIdx, pAdapter->SharedKey[BSS0][keyIdx].CipherAlg, NULL);
					STA_PORT_SECURED(pAdapter);
					// Indicate Connected for GUI
					pAdapter->IndicateMediaState = NdisMediaStateConnected;
				}
			break;
            case IW_ENCODE_ALG_TKIP:
                DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_ENCODE_ALG_TKIP - keyIdx = %d, ext->key_len = %d\n", __FUNCTION__, keyIdx, ext->key_len));
                if (ext->key_len == 32)
                {
                    if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
                    {
                        fnSetCipherKey(pAdapter, keyIdx, CIPHER_TKIP, FALSE, ext);
                        if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA2)
                        {
                            //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
                            STA_PORT_SECURED(pAdapter);
                            pAdapter->IndicateMediaState = NdisMediaStateConnected;
                        }
		}
                    else if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY)
                    {
                        fnSetCipherKey(pAdapter, keyIdx, CIPHER_TKIP, TRUE, ext);

                        // set 802.1x port control
		        //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
		        STA_PORT_SECURED(pAdapter);
		        pAdapter->IndicateMediaState = NdisMediaStateConnected;
                    }
                }
                else
                    return -EINVAL;
                break;
            case IW_ENCODE_ALG_CCMP:
                if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
		{
                    fnSetCipherKey(pAdapter, keyIdx, CIPHER_AES, FALSE, ext);
                    if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA2)
			//pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
			STA_PORT_SECURED(pAdapter);
			pAdapter->IndicateMediaState = NdisMediaStateConnected;
                }
                else if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY)
                {
                    fnSetCipherKey(pAdapter, keyIdx, CIPHER_AES, TRUE, ext);

                    // set 802.1x port control
		        //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
		        STA_PORT_SECURED(pAdapter);
		        pAdapter->IndicateMediaState = NdisMediaStateConnected;
                }
                break;
		default:
			return -EINVAL;
		}
    }

    return 0;
}

int
rt_ioctl_giwencodeext(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	PRTMP_ADAPTER pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
	PCHAR pKey = NULL;
	struct iw_point *encoding = &wrqu->encoding;
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
	int idx, max_key_len;

	DBGPRINT(RT_DEBUG_TRACE ,("===> rt_ioctl_giwencodeext\n"));

	max_key_len = encoding->length - sizeof(*ext);
	if (max_key_len < 0)
		return -EINVAL;

	idx = encoding->flags & IW_ENCODE_INDEX;
	if (idx)
	{
		if (idx < 1 || idx > 4)
			return -EINVAL;
		idx--;

		if ((pAd->StaCfg.WepStatus == Ndis802_11Encryption2Enabled) ||
			(pAd->StaCfg.WepStatus == Ndis802_11Encryption3Enabled))
		{
			if (idx != pAd->StaCfg.DefaultKeyId)
			{
				ext->key_len = 0;
				return 0;
			}
		}
	}
	else
		idx = pAd->StaCfg.DefaultKeyId;

	encoding->flags = idx + 1;
	memset(ext, 0, sizeof(*ext));

	ext->key_len = 0;
	switch(pAd->StaCfg.WepStatus) {
		case Ndis802_11WEPDisabled:
			ext->alg = IW_ENCODE_ALG_NONE;
			encoding->flags |= IW_ENCODE_DISABLED;
			break;
		case Ndis802_11WEPEnabled:
			ext->alg = IW_ENCODE_ALG_WEP;
			if (pAd->SharedKey[BSS0][idx].KeyLen > max_key_len)
				return -E2BIG;
			else
			{
				ext->key_len = pAd->SharedKey[BSS0][idx].KeyLen;
				pKey = (PCHAR)&(pAd->SharedKey[BSS0][idx].Key[0]);
			}
			break;
		case Ndis802_11Encryption2Enabled:
		case Ndis802_11Encryption3Enabled:
			if (pAd->StaCfg.WepStatus == Ndis802_11Encryption2Enabled)
				ext->alg = IW_ENCODE_ALG_TKIP;
			else
				ext->alg = IW_ENCODE_ALG_CCMP;

			if (max_key_len < 32)
				return -E2BIG;
			else
			{
				ext->key_len = 32;
				pKey = (PCHAR)&pAd->StaCfg.PMK[0];
			}
			break;
		default:
			return -EINVAL;
	}

	if (ext->key_len && pKey)
	{
		encoding->flags |= IW_ENCODE_ENABLED;
		memcpy(ext->key, pKey, ext->key_len);
	}

	return 0;
}

#ifdef SIOCSIWGENIE
int rt_ioctl_siwgenie(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);

	DBGPRINT(RT_DEBUG_TRACE ,("===> rt_ioctl_siwgenie\n"));
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
	pAd->StaCfg.bRSN_IE_FromWpaSupplicant = FALSE;
#endif // NATIVE_WPA_SUPPLICANT_SUPPORT //
	if (wrqu->data.length > MAX_LEN_OF_RSNIE ||
	    (wrqu->data.length && extra == NULL))
		return -EINVAL;

	if (wrqu->data.length)
	{
		pAd->StaCfg.RSNIE_Len = wrqu->data.length;
		NdisMoveMemory(&pAd->StaCfg.RSN_IE[0], extra, pAd->StaCfg.RSNIE_Len);
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
		pAd->StaCfg.bRSN_IE_FromWpaSupplicant = TRUE;
#endif // NATIVE_WPA_SUPPLICANT_SUPPORT //
	}
	else
	{
		pAd->StaCfg.RSNIE_Len = 0;
		NdisZeroMemory(&pAd->StaCfg.RSN_IE[0], MAX_LEN_OF_RSNIE);
	}

	return 0;
}
#endif // SIOCSIWGENIE //

int rt_ioctl_giwgenie(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);

	if ((pAd->StaCfg.RSNIE_Len == 0) ||
		(pAd->StaCfg.AuthMode < Ndis802_11AuthModeWPA))
	{
		wrqu->data.length = 0;
		return 0;
	}

#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
#ifdef SIOCSIWGENIE
	if (pAd->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_ENABLE)
	{
	if (wrqu->data.length < pAd->StaCfg.RSNIE_Len)
		return -E2BIG;

	wrqu->data.length = pAd->StaCfg.RSNIE_Len;
	memcpy(extra, &pAd->StaCfg.RSN_IE[0], pAd->StaCfg.RSNIE_Len);
	}
	else
#endif // SIOCSIWGENIE //
#endif // NATIVE_WPA_SUPPLICANT_SUPPORT //
	{
		UCHAR RSNIe = IE_WPA;

		if (wrqu->data.length < (pAd->StaCfg.RSNIE_Len + 2)) // ID, Len
			return -E2BIG;
		wrqu->data.length = pAd->StaCfg.RSNIE_Len + 2;

		if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK) ||
            (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2))
			RSNIe = IE_RSN;

		extra[0] = (char)RSNIe;
		extra[1] = pAd->StaCfg.RSNIE_Len;
		memcpy(extra+2, &pAd->StaCfg.RSN_IE[0], pAd->StaCfg.RSNIE_Len);
	}

	return 0;
}

int rt_ioctl_siwpmksa(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu,
			   char *extra)
{
	PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_pmksa *pPmksa = (struct iw_pmksa *)wrqu->data.pointer;
	INT	CachedIdx = 0, idx = 0;

	if (pPmksa == NULL)
		return -EINVAL;

	DBGPRINT(RT_DEBUG_TRACE ,("===> rt_ioctl_siwpmksa\n"));
	switch(pPmksa->cmd)
	{
		case IW_PMKSA_FLUSH:
			NdisZeroMemory(pAd->StaCfg.SavedPMK, sizeof(BSSID_INFO)*PMKID_NO);
			DBGPRINT(RT_DEBUG_TRACE ,("rt_ioctl_siwpmksa - IW_PMKSA_FLUSH\n"));
			break;
		case IW_PMKSA_REMOVE:
			for (CachedIdx = 0; CachedIdx < pAd->StaCfg.SavedPMKNum; CachedIdx++)
			{
		        // compare the BSSID
		        if (NdisEqualMemory(pPmksa->bssid.sa_data, pAd->StaCfg.SavedPMK[CachedIdx].BSSID, MAC_ADDR_LEN))
		        {
				NdisZeroMemory(pAd->StaCfg.SavedPMK[CachedIdx].BSSID, MAC_ADDR_LEN);
					NdisZeroMemory(pAd->StaCfg.SavedPMK[CachedIdx].PMKID, 16);
					for (idx = CachedIdx; idx < (pAd->StaCfg.SavedPMKNum - 1); idx++)
					{
						NdisMoveMemory(&pAd->StaCfg.SavedPMK[idx].BSSID[0], &pAd->StaCfg.SavedPMK[idx+1].BSSID[0], MAC_ADDR_LEN);
						NdisMoveMemory(&pAd->StaCfg.SavedPMK[idx].PMKID[0], &pAd->StaCfg.SavedPMK[idx+1].PMKID[0], 16);
					}
					pAd->StaCfg.SavedPMKNum--;
			        break;
		        }
	        }

			DBGPRINT(RT_DEBUG_TRACE ,("rt_ioctl_siwpmksa - IW_PMKSA_REMOVE\n"));
			break;
		case IW_PMKSA_ADD:
			for (CachedIdx = 0; CachedIdx < pAd->StaCfg.SavedPMKNum; CachedIdx++)
			{
		        // compare the BSSID
		        if (NdisEqualMemory(pPmksa->bssid.sa_data, pAd->StaCfg.SavedPMK[CachedIdx].BSSID, MAC_ADDR_LEN))
			        break;
	        }

	        // Found, replace it
	        if (CachedIdx < PMKID_NO)
	        {
		        DBGPRINT(RT_DEBUG_OFF, ("Update PMKID, idx = %d\n", CachedIdx));
		        NdisMoveMemory(&pAd->StaCfg.SavedPMK[CachedIdx].BSSID[0], pPmksa->bssid.sa_data, MAC_ADDR_LEN);
				NdisMoveMemory(&pAd->StaCfg.SavedPMK[CachedIdx].PMKID[0], pPmksa->pmkid, 16);
		        pAd->StaCfg.SavedPMKNum++;
	        }
	        // Not found, replace the last one
	        else
	        {
		        // Randomly replace one
		        CachedIdx = (pPmksa->bssid.sa_data[5] % PMKID_NO);
		        DBGPRINT(RT_DEBUG_OFF, ("Update PMKID, idx = %d\n", CachedIdx));
		        NdisMoveMemory(&pAd->StaCfg.SavedPMK[CachedIdx].BSSID[0], pPmksa->bssid.sa_data, MAC_ADDR_LEN);
				NdisMoveMemory(&pAd->StaCfg.SavedPMK[CachedIdx].PMKID[0], pPmksa->pmkid, 16);
	        }

			DBGPRINT(RT_DEBUG_TRACE ,("rt_ioctl_siwpmksa - IW_PMKSA_ADD\n"));
			break;
		default:
			DBGPRINT(RT_DEBUG_TRACE ,("rt_ioctl_siwpmksa - Unknow Command!!\n"));
			break;
	}

	return 0;
}
#endif // #if WIRELESS_EXT > 17

#ifdef DBG
static int
rt_private_ioctl_bbp(struct net_device *dev, struct iw_request_info *info,
		struct iw_point *wrq, char *extra)
			{
	PSTRING				this_char;
	PSTRING				value = NULL;
	UCHAR				regBBP = 0;
//	CHAR				arg[255]={0};
	UINT32				bbpId;
	UINT32				bbpValue;
	BOOLEAN				bIsPrintAllBBP = FALSE;
	INT					Status = 0;
    PRTMP_ADAPTER       pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);


	memset(extra, 0x00, IW_PRIV_SIZE_MASK);

	if (wrq->length > 1) //No parameters.
				{
		sprintf(extra, "\n");

		//Parsing Read or Write
		this_char = wrq->pointer;
		DBGPRINT(RT_DEBUG_TRACE, ("this_char=%s\n", this_char));
		if (!*this_char)
			goto next;

		if ((value = rtstrchr(this_char, '=')) != NULL)
			*value++ = 0;

		if (!value || !*value)
		{ //Read
			DBGPRINT(RT_DEBUG_TRACE, ("this_char=%s, value=%s\n", this_char, value));
			if (sscanf(this_char, "%d", &(bbpId)) == 1)
			{
				if (bbpId <= MAX_BBP_ID)
				{
#ifdef RALINK_ATE
					if (ATE_ON(pAdapter))
					{
						ATE_BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
					}
					else
#endif // RALINK_ATE //
					{
					RTMP_BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
					}
					sprintf(extra+strlen(extra), "R%02d[0x%02X]:%02X\n", bbpId, bbpId, regBBP);
                    wrq->length = strlen(extra) + 1; // 1: size of '\0'
					DBGPRINT(RT_DEBUG_TRACE, ("msg=%s\n", extra));
				}
				else
				{//Invalid parametes, so default printk all bbp
					bIsPrintAllBBP = TRUE;
					goto next;
				}
			}
			else
			{ //Invalid parametes, so default printk all bbp
				bIsPrintAllBBP = TRUE;
				goto next;
			}
		}
		else
		{ //Write
			if ((sscanf(this_char, "%d", &(bbpId)) == 1) && (sscanf(value, "%x", &(bbpValue)) == 1))
			{
				if (bbpId <= MAX_BBP_ID)
				{
#ifdef RALINK_ATE
					if (ATE_ON(pAdapter))
					{
						ATE_BBP_IO_WRITE8_BY_REG_ID(pAdapter, bbpId, bbpValue);
						/* read it back for showing */
						ATE_BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
					}
					else
#endif // RALINK_ATE //
					{
					    RTMP_BBP_IO_WRITE8_BY_REG_ID(pAdapter, bbpId, bbpValue);
					/* read it back for showing */
					RTMP_BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
			}
					sprintf(extra+strlen(extra), "R%02d[0x%02X]:%02X\n", bbpId, bbpId, regBBP);
                    wrq->length = strlen(extra) + 1; // 1: size of '\0'
					DBGPRINT(RT_DEBUG_TRACE, ("msg=%s\n", extra));
				}
				else
				{//Invalid parametes, so default printk all bbp
					bIsPrintAllBBP = TRUE;
					goto next;
				}
			}
			else
			{ //Invalid parametes, so default printk all bbp
				bIsPrintAllBBP = TRUE;
				goto next;
			}
		}
		}
	else
		bIsPrintAllBBP = TRUE;

next:
	if (bIsPrintAllBBP)
	{
		memset(extra, 0x00, IW_PRIV_SIZE_MASK);
		sprintf(extra, "\n");
		for (bbpId = 0; bbpId <= MAX_BBP_ID; bbpId++)
		{
		    if (strlen(extra) >= (IW_PRIV_SIZE_MASK - 20))
                break;
#ifdef RALINK_ATE
			if (ATE_ON(pAdapter))
			{
				ATE_BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
			}
			else
#endif // RALINK_ATE //
			RTMP_BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
			sprintf(extra+strlen(extra), "R%02d[0x%02X]:%02X    ", bbpId, bbpId, regBBP);
			if (bbpId%5 == 4)
			sprintf(extra+strlen(extra), "%03d = %02X\n", bbpId, regBBP);  // edit by johnli, change display format
		}

        wrq->length = strlen(extra) + 1; // 1: size of '\0'
        DBGPRINT(RT_DEBUG_TRACE, ("wrq->length = %d\n", wrq->length));
	}

	DBGPRINT(RT_DEBUG_TRACE, ("<==rt_private_ioctl_bbp\n\n"));

    return Status;
}
#endif // DBG //

int rt_ioctl_siwrate(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
    PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
    UINT32          rate = wrqu->bitrate.value, fixed = wrqu->bitrate.fixed;

    //check if the interface is down
	if(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("rt_ioctl_siwrate::Network is down!\n"));
	return -ENETDOWN;
	}

    DBGPRINT(RT_DEBUG_TRACE, ("rt_ioctl_siwrate::(rate = %d, fixed = %d)\n", rate, fixed));
    /* rate = -1 => auto rate
       rate = X, fixed = 1 => (fixed rate X)
    */
    if (rate == -1)
    {
        //Auto Rate
        pAd->StaCfg.DesiredTransmitSetting.field.MCS = MCS_AUTO;
		pAd->StaCfg.bAutoTxRateSwitch = TRUE;
		if ((pAd->CommonCfg.PhyMode <= PHY_11G) ||
		    (pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.MODE <= MODE_OFDM))
            RTMPSetDesiredRates(pAd, -1);

#ifdef DOT11_N_SUPPORT
            SetCommonHT(pAd);
#endif // DOT11_N_SUPPORT //
    }
    else
    {
        if (fixed)
        {
		pAd->StaCfg.bAutoTxRateSwitch = FALSE;
            if ((pAd->CommonCfg.PhyMode <= PHY_11G) ||
                (pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.MODE <= MODE_OFDM))
                RTMPSetDesiredRates(pAd, rate);
            else
            {
                pAd->StaCfg.DesiredTransmitSetting.field.MCS = MCS_AUTO;
#ifdef DOT11_N_SUPPORT
                SetCommonHT(pAd);
#endif // DOT11_N_SUPPORT //
            }
            DBGPRINT(RT_DEBUG_TRACE, ("rt_ioctl_siwrate::(HtMcs=%d)\n",pAd->StaCfg.DesiredTransmitSetting.field.MCS));
        }
        else
        {
            // TODO: rate = X, fixed = 0 => (rates <= X)
            return -EOPNOTSUPP;
        }
    }

    return 0;
}

int rt_ioctl_giwrate(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
    PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
    int rate_index = 0, rate_count = 0;
    HTTRANSMIT_SETTING ht_setting;
/* Remove to global variable
    __s32 ralinkrate[] =
	{2,  4,   11,  22, // CCK
	12, 18,   24,  36, 48, 72, 96, 108, // OFDM
	13, 26,   39,  52,  78, 104, 117, 130, 26,  52,  78, 104, 156, 208, 234, 260, // 20MHz, 800ns GI, MCS: 0 ~ 15
	39, 78,  117, 156, 234, 312, 351, 390,										  // 20MHz, 800ns GI, MCS: 16 ~ 23
	27, 54,   81, 108, 162, 216, 243, 270, 54, 108, 162, 216, 324, 432, 486, 540, // 40MHz, 800ns GI, MCS: 0 ~ 15
	81, 162, 243, 324, 486, 648, 729, 810,										  // 40MHz, 800ns GI, MCS: 16 ~ 23
	14, 29,   43,  57,  87, 115, 130, 144, 29, 59,   87, 115, 173, 230, 260, 288, // 20MHz, 400ns GI, MCS: 0 ~ 15
	43, 87,  130, 173, 260, 317, 390, 433,										  // 20MHz, 400ns GI, MCS: 16 ~ 23
	30, 60,   90, 120, 180, 240, 270, 300, 60, 120, 180, 240, 360, 480, 540, 600, // 40MHz, 400ns GI, MCS: 0 ~ 15
	90, 180, 270, 360, 540, 720, 810, 900};										  // 40MHz, 400ns GI, MCS: 16 ~ 23
*/

    rate_count = sizeof(ralinkrate)/sizeof(__s32);
    //check if the interface is down
	if(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
	}

    if ((pAd->StaCfg.bAutoTxRateSwitch == FALSE) &&
        (INFRA_ON(pAd)) &&
        ((pAd->CommonCfg.PhyMode <= PHY_11G) || (pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.MODE <= MODE_OFDM)))
        ht_setting.word = pAd->StaCfg.HTPhyMode.word;
    else
        ht_setting.word = pAd->MacTab.Content[BSSID_WCID].HTPhyMode.word;

#ifdef DOT11_N_SUPPORT
    if (ht_setting.field.MODE >= MODE_HTMIX)
    {
//	rate_index = 12 + ((UCHAR)ht_setting.field.BW *16) + ((UCHAR)ht_setting.field.ShortGI *32) + ((UCHAR)ht_setting.field.MCS);
	rate_index = 12 + ((UCHAR)ht_setting.field.BW *24) + ((UCHAR)ht_setting.field.ShortGI *48) + ((UCHAR)ht_setting.field.MCS);
    }
    else
#endif // DOT11_N_SUPPORT //
    if (ht_setting.field.MODE == MODE_OFDM)
	rate_index = (UCHAR)(ht_setting.field.MCS) + 4;
    else if (ht_setting.field.MODE == MODE_CCK)
	rate_index = (UCHAR)(ht_setting.field.MCS);

    if (rate_index < 0)
        rate_index = 0;

    if (rate_index > rate_count)
        rate_index = rate_count;

    wrqu->bitrate.value = ralinkrate[rate_index] * 500000;
    wrqu->bitrate.disabled = 0;

    return 0;
}

static const iw_handler rt_handler[] =
{
	(iw_handler) NULL,			            /* SIOCSIWCOMMIT */
	(iw_handler) rt_ioctl_giwname,			/* SIOCGIWNAME   */
	(iw_handler) NULL,			            /* SIOCSIWNWID   */
	(iw_handler) NULL,			            /* SIOCGIWNWID   */
	(iw_handler) rt_ioctl_siwfreq,		    /* SIOCSIWFREQ   */
	(iw_handler) rt_ioctl_giwfreq,		    /* SIOCGIWFREQ   */
	(iw_handler) rt_ioctl_siwmode,		    /* SIOCSIWMODE   */
	(iw_handler) rt_ioctl_giwmode,		    /* SIOCGIWMODE   */
	(iw_handler) NULL,		                /* SIOCSIWSENS   */
	(iw_handler) NULL,		                /* SIOCGIWSENS   */
	(iw_handler) NULL /* not used */,		/* SIOCSIWRANGE  */
	(iw_handler) rt_ioctl_giwrange,		    /* SIOCGIWRANGE  */
	(iw_handler) NULL /* not used */,		/* SIOCSIWPRIV   */
	(iw_handler) NULL /* kernel code */,    /* SIOCGIWPRIV   */
	(iw_handler) NULL /* not used */,		/* SIOCSIWSTATS  */
	(iw_handler) rt28xx_get_wireless_stats /* kernel code */,    /* SIOCGIWSTATS  */
	(iw_handler) NULL,		                /* SIOCSIWSPY    */
	(iw_handler) NULL,		                /* SIOCGIWSPY    */
	(iw_handler) NULL,				        /* SIOCSIWTHRSPY */
	(iw_handler) NULL,				        /* SIOCGIWTHRSPY */
	(iw_handler) rt_ioctl_siwap,            /* SIOCSIWAP     */
	(iw_handler) rt_ioctl_giwap,		    /* SIOCGIWAP     */
#ifdef SIOCSIWMLME
	(iw_handler) rt_ioctl_siwmlme,	        /* SIOCSIWMLME   */
#else
	(iw_handler) NULL,				        /* SIOCSIWMLME */
#endif // SIOCSIWMLME //
	(iw_handler) rt_ioctl_iwaplist,		    /* SIOCGIWAPLIST */
#ifdef SIOCGIWSCAN
	(iw_handler) rt_ioctl_siwscan,		    /* SIOCSIWSCAN   */
	(iw_handler) rt_ioctl_giwscan,		    /* SIOCGIWSCAN   */
#else
	(iw_handler) NULL,				        /* SIOCSIWSCAN   */
	(iw_handler) NULL,				        /* SIOCGIWSCAN   */
#endif /* SIOCGIWSCAN */
	(iw_handler) rt_ioctl_siwessid,		    /* SIOCSIWESSID  */
	(iw_handler) rt_ioctl_giwessid,		    /* SIOCGIWESSID  */
	(iw_handler) rt_ioctl_siwnickn,		    /* SIOCSIWNICKN  */
	(iw_handler) rt_ioctl_giwnickn,		    /* SIOCGIWNICKN  */
	(iw_handler) NULL,				        /* -- hole --    */
	(iw_handler) NULL,				        /* -- hole --    */
	(iw_handler) rt_ioctl_siwrate,          /* SIOCSIWRATE   */
	(iw_handler) rt_ioctl_giwrate,          /* SIOCGIWRATE   */
	(iw_handler) rt_ioctl_siwrts,		    /* SIOCSIWRTS    */
	(iw_handler) rt_ioctl_giwrts,		    /* SIOCGIWRTS    */
	(iw_handler) rt_ioctl_siwfrag,		    /* SIOCSIWFRAG   */
	(iw_handler) rt_ioctl_giwfrag,		    /* SIOCGIWFRAG   */
	(iw_handler) NULL,		                /* SIOCSIWTXPOW  */
	(iw_handler) NULL,		                /* SIOCGIWTXPOW  */
	(iw_handler) NULL,		                /* SIOCSIWRETRY  */
	(iw_handler) NULL,		                /* SIOCGIWRETRY  */
	(iw_handler) rt_ioctl_siwencode,		/* SIOCSIWENCODE */
	(iw_handler) rt_ioctl_giwencode,		/* SIOCGIWENCODE */
	(iw_handler) NULL,		                /* SIOCSIWPOWER  */
	(iw_handler) NULL,		                /* SIOCGIWPOWER  */
	(iw_handler) NULL,						/* -- hole -- */
	(iw_handler) NULL,						/* -- hole -- */
#if WIRELESS_EXT > 17
    (iw_handler) rt_ioctl_siwgenie,         /* SIOCSIWGENIE  */
	(iw_handler) rt_ioctl_giwgenie,         /* SIOCGIWGENIE  */
	(iw_handler) rt_ioctl_siwauth,		    /* SIOCSIWAUTH   */
	(iw_handler) rt_ioctl_giwauth,		    /* SIOCGIWAUTH   */
	(iw_handler) rt_ioctl_siwencodeext,	    /* SIOCSIWENCODEEXT */
	(iw_handler) rt_ioctl_giwencodeext,		/* SIOCGIWENCODEEXT */
	(iw_handler) rt_ioctl_siwpmksa,         /* SIOCSIWPMKSA  */
#endif
};

static const iw_handler rt_priv_handlers[] = {
	(iw_handler) NULL, /* + 0x00 */
	(iw_handler) NULL, /* + 0x01 */
	(iw_handler) rt_ioctl_setparam, /* + 0x02 */
#ifdef DBG
	(iw_handler) rt_private_ioctl_bbp, /* + 0x03 */
#else
	(iw_handler) NULL, /* + 0x03 */
#endif
	(iw_handler) NULL, /* + 0x04 */
	(iw_handler) NULL, /* + 0x05 */
	(iw_handler) NULL, /* + 0x06 */
	(iw_handler) NULL, /* + 0x07 */
	(iw_handler) NULL, /* + 0x08 */
	(iw_handler) rt_private_get_statistics, /* + 0x09 */
	(iw_handler) NULL, /* + 0x0A */
	(iw_ha/*
 *****************B********************************C********************************D********************************E********************************F*******************************10******************rt_private_show, Ralink T1****    pyright 2002-2007, Ralink T2c) Copyright 2002-2007, Ralink T3ftware; you can redistribute it and4c) Copyright 2002-2007, Ralink T5ftware; you can redistribute it and6c) Copyright 2002-2007, Ralink T7c) Copyright 2002-2007, Ralink T8****};

const struct **********_def rt28xx_           *
 *=
{
#define	N(a)	(sizeof (a) /          [0]))
	.standard	= you can red **
 * *******,
	.num_ *
 * This       (ributed in          ************ theThis pris program is distribThisbuted ins the hopARRANTY;	= N useplied warrantANY WARRANTY_argsis p          pliedPARTdistThistaby of        *
  PARTICUN(      *),
#if IW_HANDLER_VERSION >= 7are; .get_wirelesspe tts =*             *
 *        ,
#endif     INT RTMPSetInformation(are; IN  P    _ADAPTER pAd,
 * You sOUT         freqre; *rqed a copy       *
 * along witcmd)
{are;          neral Pte to the           *wrq CULAR PURPOSneraistrq;are; NDIS_802_11_SSIDte to the           Ssidtware Foundation, MAC_ADDRESSte to the    Bs         RTdation, PHY_MODEte to the         PhyMode0, Boston, MA  0STA_CONFIG                 taConfigtware Foundation, RATEle Place - Sui******aryRates0, Boston, MA  02REAMBL07, USA.            reambl       Foundation, WEP    TU***************WepStatu*******Foundation, AUTHENTICATION1-1307, USAuth
 *  = Ndisation,ory:
   Maxtware Foundation, NETWORK_INFRASTRUCTUREuite Typ  Module Name:
    RTS_THRESHOL.,          RtsThreshtware Foundation, FRAGMENT  Revi----------FragChen   01-03-2003    crePOWER1-1307, USA.       Power
 *       PFoundation, KEYte to the           pKey WhoULLfdef DBG
extern ULOWEP			vel;
#endWedif

#efine NR_WEP_KEYS				4
REMOVEONG    RTDebugLevpRemoveif

#define NR_WFoundation,       UR  Revte to the        , *p       GROUP_KEY_NO            --    --TYP07, USA.     Net----------ULON                 vent(_A, _B, _CNow _D, _E     *
 * along withDD_POINT(_A, _BKeyIdx = 0tware define IWE_STREAM_ADD_POINT(_A, _B,    tu    Foundioctl._SUCCESS, Max  *
 *  = 211111G _D, _E)		iwe_stream_add_event(_A, _B, _C,ig.h"Temptware BOOLEAINUX_VERSIONM_ADD_EVENT(_A, _BRadioE)
#       #define IWE_STREAM_ADD_EVENT(_A, _B,_D, _MachineTouched = FALSEtware DBG
extern ULOPASSPHRAS07, USA.             ppassphras  Whofine #if*
 *DOTe IW_SUPPORT
	OID_SET_HT0211-130	iwe_, _E *
 *  	//11n ,kathy        //M_ADD_VALUE(_A,  //E_STREAMWPALUE(_LICANTLUE(_A, _, _B, _C, _D, _E)	MKc.,                 pPmkIADD_efine NR_W#defineiwe_M_ADD_EVENT(_A, IEEE8021x_D, _DD_POINT(_A, _erVersionW;
    UCHAR       DriverVe_required_key    POINT(_A, _UCHAReam_add_point(_A, _B, _C, _D, _wpa_supplicant_enablnX;
0;_D, _E, _F)ipherWpa2Template[];

 UCHE_STREAMSNMPLUE(_A, _BTX_RTY_CFG_---  			tx_rty_cfg;
	E)		iiwe_s	ShortRetryLimit, Long_IOCTL_SETvtab     {
{ Rct)
#eD, _E, _F)*PRT_VERSIONRSION
ON_INFO, _ADD_VALUE(_A, _BE(_A, _B, _C, _D, _N_5G "set"},

{ 
#endif

extern UCH
	DBGPRINT(RT_DEBUG_TRACE, ("-->                  *),	0x%08x\n",h th&0x7FFF));
	switch(cmd & _CHAR | {
		c
#deRT_, _C         UNTRY_REGION:
			if (wrq->u.data.length <will be AR |     { RT)
#defin-EINVAL;| 10// Only avalih;
  when EEPROM not programming_A, _BINFO,
	else OW_D!(pAd->CommonCfg.CountryRegion_TYPE80) && TYPE_CHAR | 1024, IW_PRIV_TYPEForABanV_TYPE80R | 10| IW	ab[] =ION_CW_PRICHAR HAR | 1TmpPhy;
| 1024, IW_PRIcopy_from_user(&R | 102, RVIER_VERIONpointer   { RAIO_OFF,,
	  I 102			PE_CHAR | 1024, IW_PRIV_TYPE_= PE_CHAR(R | 102_TYPE0{ RAIFF4, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZESHOW_DESCMASK, "raddio_off" >> 8) },
	{ RAIO_ON,
M_ADD_EVENT(_A, YPE_CH =ceivCHAR | 1024,  *
 *   IW_PRIV_TYPE_CHAR |A, _B, _C,0xff
	  IW// Build all corresponding channel i          	  IW       am_add_YPE_,_SUPPOR)WE_STREAM_ADD_VALUE(_A, _B{ RTetAR | 1HTYPE_ SHO, _E, _F)
#endif

extern UCHV_TYb-ioctls definitions --- */Set::_SIZE_MASK, "connStatus" },
 (A:%d  B/G:%d)PRIV_W_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYP,ZE_MA _C, E_CHAR | 1024, IW_PRIV_TYPE| 10M_ADD_EVENT(}/

#ifdef DBGbreak*/

#ifdef_PRIVZE_MASK, "cBInc._LIST_SCAN:/

#ifdef DBGNow = jiffi****_MASK, "show" },
	{ SHOW_ADHOC_ENTRE_CHAR | IW_PRIV_SIZE_MASK, TxCnt = %d V_TYPE_CHARalinkR | 1ers.LastOneSecTotalTxR | 1| 10/

#ifdef DBGOW_DMONITOR_ON024, )/

#ifdef DBG progrPRIV_SIZE_MAb-ioctls definitions --- */!!! Driver is in Monitor _B, _now !!!\n"- */

#ifdef DBGIOCTL_BBP,
  IW_PRI DBG
{HAR | Benson add 20080527,SK, "dr_C,  off, sta don't need to sca
#endOW_Dould TEST_FLAGUPPORTfould have re_RADIO_OR | ZE_MAL_BBP,
  /* DBG */

{ RTPRIV_IOCTL_STATISTICS,
  BSS_MASK_IN_PROGempl
	  IW_PIW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,ndifnASK, // RTMP_RF_RWIW_PRIV_T    fg.bScanReqIsFromWebUI = TRUE,  4, E)
#define IWE_STREAM_ADD_VA},
#ifdef QOS_DLS_PRIV_IOCTL_E2P,
  IW_PRIVOW_DYPE_CHAR | 1024,
  "mac"},
#ifdef RTMP_RF_ > 100RIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_Link UP, ignore th__s3IOCTL_MAC,
  IW_PRIV_TYPE_CHAR=
	{2,  4, , 96, 108, // OFDM
	13, 26,    4,   11,  22, / CCK024, I99;YPE_CPrevent au
#endif triggered byMCS: 1OIDTYPE_CHAR | CTL_E2P,
  IW_PPPORT
{ RTPRIV_I(OP_STREAM{ RTPRIV_IOCTL_SOa_ioctl._MEDIAE_STRE    NECTED) IW_432, (04, 15  22, /ory:
    WWho         When    WPA) ||  87, 15, 173, 230, 260, 288, // 20MHz, 400ns GI, MPSKCS: 0 ~ 15
	43, 87,  130, 173, 260, 317, 390, 433,					2		  // 20MHz, 400ns GI, MCS: 16 ~ 23
	30, 60,   90, 120,				59,  M_ADD_EVENT(_A, 5
	43, 87,  1PortSecu3, 3==DriveationX__A, _NOT_SECUR9, 5 ~ 15
	39, 78,  117, 156, 234, 312, 351, 390,										  // 20MHz, 800n0, 1 Not 80, 270!ns GI, MCS: 16 ~ 23
	27, 54,   81, 108, 162, 216, 243, 270, 54, 108, 162, 216, 324, 432, 486, 540, // 40MHz, 800ns GI, MCS: 0 ~ 15
	81, 162, 243, 324, 486, 648, 729, 810,										  //  40MHz, 800ns GI, --- Mlme.Cntle IWE_S.CurrrsionX!= CNTL_IDLENT Set_SSID_Proc(
    IN  PRTMP_ould MLME_RE, _D, 130,MACHINE024, IWIW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_ypTy busy, reset

INT     e m IWE_S RTMP_RF_RW_SUPPORT //
// 40MHz, 800ns// tellr,
  _Proc(
    IN  to cIW_Po   M              Complete() after cDAPTERSHOW_BA_INFO,
	 RINGS: 1Drivest, becaus MCS: 1);

INTASK,
 itiat, 324,Foun.PTER   pAdaptc(
    INAuxTER  
	12, 18,o   DD_POINT(_A, _BNG        RaultKallowed	81, 1retriespter,
    IN  PSTR540, // 40MHz, 80_E)		iwPSTRING          argmac" CCKTim  Who_D, 432,  11,  22, // CCK
	12, 18,   24,  36, 48g);

INT Set_Encr, _D
  "stat"},
{ RTPRIV_IOCTL_GSITESURVEY,
  0, APSK_Proc(
     INEnqueuSUPPORPTER   pAdapter,
;


INT SpTyp,
   (
    IN  PRT(
    IN  PRTMP_ADAPTER   L_MAC,
  IW_PRIV_TYPE_CHAR 
    IN  PRTMP_ADAPTER   0_Proc(
IN  PRTMP_ADAPTER  ne IW_SUPPORT
{ RTPR, 96, 108, // OFDM
	13, 26,   39,  52,  78define IWE_STREAM_ADD_t_WPAPSK_Proc(
   L_BBP,
  IW_PRIV_TYPE_CHAR | IWInc., IW_PRIV_TYPEOW_DRVIER_VERION,
	  IW! will be Foundation, Inc.INT Set_SSID_PrD, _E)
#definV_TYPE_CH_BA_INFO,
	  IW_IV_TYPE_CHAR | 		PSTRING p    StrASK,R       Driv;
#endif // RT3090 //E_MASK, "descinf       { RAIO_OFF,
	  IW_PRIV_TYPE_CHAR | 1024, ZE_MASK, "show" },
	{ SHOW_ADHOC_ENTRrg);
#endif //  (Len=%d,    =%sIV_TYP    .    L
	  Iroc(
    INF_RW_SUPPORT //
{ RTOW_Dc(
    IN  PRTM > MAX_LEN_OF	IN	PR
    IN  PRTMP_ADAPTEer,
	IN	struct iwreq	*wrq);

V;

VOID RTMPIoctlE2PRE2PROM(
8, 10NG          arg)== 0_TYPE__PRIV_Setf // _ProcUPPORT""2,  4, }432, 	   pAd,
    IN  PSTRISTRING   	PRTMP_ADAPTER (    IN )kmPTERc(
#ifdef DOT11_N+1, MEM_ALLOCMP_AD2,  4, 78, 104RTMP_ADAPT_TYPE__N_SUPPO			o   ZeroMemory
	IN	PSTRING,

#ifdef DOT11_N+1ER	pAdapt EXT_MoveD_CHANNEL_LIST
INT Sc(
    INroc(
    IN  PRTMntMode_ProRT //

INT Set_Long	IN	PSTRING	tMode_Prokfree
	IN	PSTRING	R	pAdaptt_Proc((
	IN	     argr,
	IN	strNOMEMAPTER   pAdapter,
 DBG
{ RTPRIV_IOCT DBG
{ RTPRIV_IOCT
{ RTPRIV_IOCTL_BBP,
IW_PRIVrg);
#endif ET)		iwe_stre:
},
/* _D, _E)
#d= Set_ShorRVIER_VERION,
	  I
	IN	PRTMP_ADAPTER	_Adhocif(_MacTable_PrR     NT Set_SSID_ProcrrierDetect_Proc(
  
    IN  PRTMP_ADAPTER   pAd,
    IN	PVOID			pBuT //

INT	Sho, Failed] =
	{2,  4PRIV_IOCTL_E2P,
  IW_RT
INT SetctlRF(
	IN	PRE_MASK, "descin_MacTable_P/ DBG //


NDIS_STATUS RTMPWPANoneAddKeyProc(
        )
#de_TYPNG   ierDetePRIV_TYPE_CHAR Adapter,
	IN	struct iwreq	*wrq);
#endif // RTMP_RF_RW_SUPPORT //
 (,
	  IWmismatch)] =
	{2,  4,
    IN  PRT;

#ifdefdef RTMP_RF_R->Key   arg)< 8 ||c_MacTable_P,
    IN  PS> 64			arg)

#ifdef IN  PSTRING         arg);

INT Set_SiteSurvey_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			ar  *   than 8 or greaapte/

st64_ForceTxBurstmit_Proc(
	IN	arg);

#ifdef//16 ~ key _D, _E)
#deDESC,
	  ISet_CarEXT_BUILD_CHANNE        argWpaPassP       64ETECTIONc(
    IN  PRTMP_PRIVATE_SUPPORT_PROC[] = &       arg);

#iMuct ial,        arg);

#ifdef AETECTION_et_DriverVersion_Proc},
	LeE_MA       arg);

#ifdef AR	pAdapthex_dump("et_DriverVersion_Proc},
	TYPE_CHAVATE_SUPPORT_PROC[] = {
	{"Drive	printk("PPORT_PROC[] =%sPRIVet_DriverVersion_Proc},
	;
	INT (*sM_ADD_EVENT(_A, RING          ar/

#ifdef_D, _E)
#dETECTICHAR | I  IW_PRIV_TYPE_CHAR | IW_PRIVWPA_SUPPLICANT_SUPPORT //

#ifdef DBG

VOID RTMPIoctlMAC(
	 * 59 TemplRTMP_ADAPTER	pAdapter,
	INN	struct iwreq	*wrq);

VOID RTMPIoctlE2PROMpter,
    IN  struct iwreq    *wrq);
#endte 33/ DBG //


NDIS_STATUS RTMPWPANoneAddKeyProc(M_ADD_EVENT(_A, RING          arg);

INT Set_Key1_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING              arg);

INT Set_Key2_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  IN  PSTRING          arg);

INT Set_Key3_PrV_TYPE_CMCS: 0 ~t_Keonnect AP again,
  STA  INPeriodicExec432, 486, ING     AutoReProc},
  IN  P= 32i",		                SMP_ADAPTER   pAdapter,
    32, 486, 540, // 40MHz, 800i",		              _Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING      HAR | IW_PRIV_SIZE_MAS Set_EncrypType_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
er,
    IN  PSTRING          arg);

INT Set_DefaultKeyID_Proc(
    IN  PRTMP_ADAPTER   pAdaptTER   pAd,
    IN  PSTSet_PSMode_Proc(
    IN  PRTMP_ADAPTER   R   pAdapter,
    IN  PSTRING          arg);

#ifdef def RT3090
INT Set_PNetworkType",                 et_RTSThreshold_Proc},
	{"FragTNetworkType",                 (VOID *)HtMcs"DAPTER   pAdapter,
, 96, 108, // OFDM
	13, 26,   39,  52,  78, 10T Set_Wpa_Support(
    IN	PRPTER   pAdapter,
    IN  PSTRING          ar_IOCTL_MAC,
  IW_PRIV %02x:K",						Set_WPAPSK_ProcPRIVetworkType",                  Place - Suite 33[0],},
	{"P1Mode",   2Mode",   3Mode",   4Mode",   5]- */

#ifdef DBG
{ RTPRIV_IOCTL_BBP,
  IW_PRIV_TYP_SIZE_MASK, "c0, IWWPA_SUPPLICANT_SUPPORT //

#ifdef DBG

VOID RTM#defineThreshold",				Set_FragThreshold_Proc},
#ifdef DOT11_N_SUPPORT
	{"HtBw",		                Set_HtBw_Proc},
	{"H _C, _D, _   { RAIO_OFF,
	  IW_PRIV_TYPE_CHAR | 1024, y3",						Set_Key3_Proc},
	{"Key4",						Set_KeyBG //

#ifdef RALIN (=PRIV_TYP _C, _D, _ pAdapter,
    IN  PSTRI  11,  22, // w _C, G

VATE_TX_AnteOWER0_Proc},
	{"Aoc},
#endif // DOT11_N,					Set_ATE_RX_Antena_Proc},
	{PPORT
	{"PktAggregate"ANT",					Set_ATEX_Antenna",					Set_ATEHRX_Ante&&FSET_Proc},
	{"ATETXBRTMP_ADAPTER	pAdaptET",				Set_ATE_TX_FREQOFFQOFFSET_Proc},
	{TETXBW",NGTH_Proc},
	{"ATETXCNT",					Set_ATE_TX_COUNTPPORT
	{"PktAggregate"{"ATETXLEN",					Set_ATE_TX=   IN	_SUPPORT
INT Set_TGnWiS",					Set_ATE_TX_MCS_Proc},
mmCapabl _C, OnON_SUPPORT
	{"PktAggregate"    IN  PRTUpdoc(
extratryinfo" },
#endiegionABaERF2_     = EXTRA----O_CLEAR1_Proc},
	{"ATEWRF2",					TxPower",					Set__ADAPTER   pAd,
    IN  PSTRIR	pAdapter,
	IRIV_SIZE_MASK,
  "stat"},
{ RTPRIV_IOCTL_GSITESURVEY,
  0, IW_PR;
    UCHAR  _PRIV_
	{"HtMimoPs",				Set_HtMimoPs_Proc},
	{"HtDisallowTKIP",				Set_HRF3",	w_Proc},
	{"ATATEHELP",					_EncrypType_Proc(
    IN  PRTMP_ADAPp_Proc},
	{"RxStop    IN  PSTRING          arg);

INT Set_DefaultKeyID_Proc(
    IN  PRTMP_ADAPp_Proc},
	{"Rxt_ProcTER   pAdapter,
    IN  PST"ATEWRF1",						Set_ATE_Wrffte_RF1_Proc},
	{"ATEWRF2",						Set_ATE_Write_RF2_Proc},
	{"ATEWRF3",						Set_ATE_WritSW 0, IW_PRIATEWRF4",						Set_ATE_Write_RF4_Proc},
	{"ATELD  pAd,
    IN  PSTRING          arg);
#endif // CARRI,
#endif // DBG //

#ifdef 2111-130WPA_SUPPLICANT_SUPPORT //

#ifdef DBG

VOID RTMton, MA  02111-130Threshold",				Set_FragThreshold_Proc},
#ifdef DOT11_N_SUPPORT
	{"HtBw",		                Set_HtBw_Proc},
	{"Ham_add_   { RAIO_OFF,
	  IW_PRIV_TYPE_CHAR | 1024, IW_POW_DA, _B, _<=UE(_A, _B, OT11_N_S  arg);

INT Set_Enc QOS_DLS_SUPPORTLongRetrSHOW_CFG_VALUE,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MATxPower",					Set_SK, "show" },
	{ SHOW_ADHOC_ENTRY_INFO,
	  IW_2111-1307,					Set_LongRetr DBG
	{"Debug",						Set_Debug_Proc},
#endif // DBG //

#ifdef           st_Proc},
    {"ForceGF",					Set_ForceGF_Proc},
#endif /          Threshold",				Set_FragThreshold_Proc},
#ifdef DOT11_N_SUPPORT
	{"HtB		#def32	Value"Key3",						Set_Keruct iwreq    *wrq);
#endi        NNEL_Proc},
	{"ATETXPOW0",					Set_ATE_TX_POWER0_Proc},
	{"A --- sub-ioctlsbEth;
 TxBurs4, IadFromBin.fferModeWriteBufferModeWriteBack",		set_eFuseBUseBGProtec    Back_Proc},
#RSITY_SUPPORT
	BufferModeWriteBack",		set_eFuseBuUseTPRIVSlot  pAdap1;   S2003-10-30 always SHCTL_SLOT caph;
 ,
	{"HtMimoPs",				SeYPE_CHAR | 1024,A, _B, _!ack_Proc},
#AdhocgRetr9,   87,	APTE,
	{"AutoRoaming			Set_LongRetr"ATETXFREQOFFSET",				Set_ATE_TX_FREQOFF//APTER  dynamic, "dlge of "USE OFDM roc(
orer" ",
  ADHOC mod		    Set_ATE_Load_E2P//TETXset IN eTxBursd,e2p"},
#efaultKcurr 0 ~TXULL,}
as w    as BEACON frame       TE_TX_BW_Proc},
	{"ATETXLEN",					Set-------roc}TL_GTMPAd_SUPPORT
INT Set_TGnWiNG   { SHOW_DLS_ENTRY_INFO,Back_Proc},
#toRoaming     etry",				Set_ShortRetryLimit_Proc},
	{"ATEWRF1",					  INE_WritTx*****UPPORTPOINT, 0f (pAd->StaCfg.AuthMode == NakeIbssBeacoite_RF1",						Set_ATre-bAR | BLE_ENTRY		*(pAd->StaCfg.AuthMode == AsicfferMoeroMSynet_Lo>StaC// pAd,Rdg_on-chip m_CHANdif // CONFIG_APSTA_MIXED_SUPPORT //
#ifdef DOT11_N_Sendif // EXT_BUILD_CHANNEL_LIST //
#ifdef CARRIER_Proc(
         (Write Set _SUPPORT
	=%ld,
#endif /=%d"ResetCounter",				Set_ResetStatCounter_Proc}k",		set_eFuseBufferModeWrite = LEN_TKIP_EK;
            NdisMoveMemory(pAd->SharedKeyRSITY_SUPPORT
	 = LEN_TKIP_EK;
            NdisMoveMemory(pAd->SharedKey[ //
#endif // RTRW_SUdapter,
	 SHOW_DLS_ENTRYSPXR | p",				{"efu_C, SPXLINK      oc)(PRTMP_redKey[BSTANORME_CHAR ex & _IO_WRITE32UPPORTRX_FILTRstru, {"efu       redKey[B0IP_EK, LEN_TKIREADICK);
   * 5SYS_CTRL, &NdisMoveMemory(pAd&= (~NFO,
IP_EK, LEN_TKIP_TXMICK);
  Key->KeyMateriNdisMove

#ifdef DBG
{ RTPRIV_IOCTL_BBP,
  IW_PRIV_TYPE_CHAR | IWDESIRED  *
 *WPA_SUPPLICANT_SUPPORT //

#ifdef DBG

VOID RTMPIoctlMAC(
	 *
 *,					Set_SiteSurveFragThreshold_Proc},
#ifdef DOT11_N_SUPPORT
	{"HtBw",		                Set_HtBw_Proc},
	{"H********NNEL_Proc},
	{"ATETXPOW0",					Set_ATE_TX_POWER0_Proc},
	{"AE_SET_PROC, RTMP_PRIAR | 1024,Desire**** Set_Ieee80211E(_A, sMoveMem      }

            //sion",				Set_DrerAlg
		if (pAd->StaCfKIP_EK + LE>KeyMaterial + LEN_TKIP_EKPOWER0_Proc},
	{"ATETXPOW1",					Set_ATE_TX_POWER1         {
		NdisMoveMem (K",	,g = CIPHER_AES;
		else
			pAd->ShaIV_TY			Set_ATE_TX_FREQOFFSET_PerAlg
		if (pAd->StaPSMoHER_NONE;

            // 1]S0][0].CipherAlg = CIPHER_NONE;

            // 2pdate these related informat3on to MAC_TABLE_ENTRY
		pEntry = &pAd->MacTab.Con4pdate these related informat5on to MAC_TABLE_ENTRY
		pEntry = &pAd->MacTab.Con6pdate these related informat7] airCipher == Ndis802// CxBur	PND (pAd-d**** may aff},
	the

#iG				KeyIwe us"},
#eTXTRY		*s ou;

    DBGPRINT(RTNdis802_11AuthModeWPANone)
            {
     UPPORT
    {"TGnWifiTest",                 Set_TG*******st_Proc},
    {"ForceGF",					Set_ForceGF_Proc},
#endif //*******N_SUPPORT //
#ifdef QOS_DLS_SUPPORT
	{"DlsAddEntry",					Set_DlsAddEntry_Proc},
	{"DlsTearDownEntry",			Set_Dls**

   NNEL_Proc},
	{"ATETXPOW0",					Set_ATE_TX_POWER0_Proc},
	{"AongRe**

   roc}Rtation,		// UpdTPRIV"ATETXFREQOFFSET",				Set_ATE_TX_FREQOFFSET_PAR | 1024,Tx		// Updat****

    ModulKIP_TXMICK);
		pEntrSet							  BK);
   SIC WCID attribute ta (pAd->StaCfg.AuthTxPower",					Set_ IW_PRIV_
		// Update ASIC WCID attrib  IWCS: 

		// Update ASIC WCID attrib			S,					Set_SiteSurvey_Proc},
	{"ForceTxBurst"ify[BSr wants AUTO,RTMP_ADlizSet_K)		iwhere,xMicneTxBurstaccorMASK,to AP'  IN  PSTRING     IN  PRT efusility upCHARssoci     y_Proc},
	{"HtBaWitributeEntry(pAd,
								  BSS0,
								  0,
								  pAd->SharedKey[BSS0][0].CipherAlg,
								      y);

            if (pAd->StaCfg.AuthModATETXFREQOFFSET",				Set_ATE_TX_FREQOFFfiTest_Proc(
    IN  PRTMP_ADAPTER, 104, 117, 130, 26,  5   if (pAd->StaCfg.Autf // EXT_BUILD_CHANNEL_LIST //
#ifdef CARRIER_D********_SUPPORT
	{**

   - */

#ifdef DBG
{ RTPRIV_IOCTL_BBP,
  IW_PRIV_TYPE_CHAR | IWsta_ioctl.WPA_SUPPLICANT_SUPPORT //

#ifdef DBG

VOID RTMPIoctlMAC(
	sta_ioctl.EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][0].RxMic, pKey->KeyMaterial + LEN_TKt:
    IONNEL_Proc},
	{"ATETXPOW0",					Set_ATE_TX_POWER0_Proc},
	{"A
{ Rince TKIP, AES, WEP are IW_Piverorted. It shoulder" }havg);
y in_SIZd,
	IN	PNey[BSS0][0].RxMic);

t:
    IO			So         Encryp    3KeyAbsentable and IVEIV table
		RTMPAddWcidAttribANT",					Set_ATd->StaCfg.!=ct:
    IO

	if (pAd->StaCfg.AuthMProc},
	{"ATEWRF1",					isMoRNEL_VhasDIS_802__SUPPLICANT_SUPPORT //
       b      oveMe(
    IN	PRTMP_ADAPTER	_ATE_Write_RF4_Proc},
	{"ATELDKIP_RXMICK);
               }
        (pAd->StaCfg.AuthModeet_DriverVerOrigd->StaCfg.        NdisMoveMemory(pAd->SharedKey[BSS0][pPairCipher            NdisMegionABand",			Group + LEN_TK        NdisMoveMemory(pAd->SaultKeyId = (pKey->KeyIndex & 0xFF);
            NdisZeroMemory(&pAdd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId], sizeof(CIPHER_KEY));
            pAd->SharedKey[BSS0][pAd->pAd->StaCfg.DefaultKeeyLen = LE}
             else
#endif // WPA_SUPPLICANT_SUPPORT //
            {
tines

    RevisionWPA_SUPPLICANT_SUPPORT //

#ifdef DBG

VOID RTMPIoctlMAC(
	tines

    RevisionEK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][0].RxMic, pKey->KeyMaterial + LEN_TKory:
   edKey[BSS0][0].TxMic,
							  pAd->SharedKey[BSS0][0].RxMic);

ory:
    >ho         When       "ATETXFREQOFFSET",				Set_ATE_TX_FREQOFFd].CipherAlg = CIPHER_NONE;
		if (pAd->StaCfg.GroupCipher == Ndis802_11Encryptio>KeyIndex & 0xFF);
            NdisZeroMemoANT",					Set_ATory:
    !=tory:
        else
#endif // WPA_SUPPLICANT_SUPPORT //
            {
		NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMory:
    Whory:
     // Update Shared Key CipherAlg
		pS: 0 ~ 15
	90, 180, 270, 60, 540, 720, 810, 900};



rCipher == Ndis802_11Encryption3Enabled)
			pAd->SharedKey[BStines

    Revision ,				PRIV_>StaCfg.DefaultKeyId CIPHER_TKIP;
		else if (pAd->StaCfg.GroupCipher == Ndis802_1--------    --abled)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherA--    ----------    --Threshold",				Set_FragThreshold_Proc},
#ifdef DOT11_N_SUPPORT
	{"HtBw",		                Set_HtBw_Proc},
	{"HtMc----   arg);

INT Set_AutoRoaming_Proc(
    IN  PRTMP_ADAAddKey ----o         IBSSp",				RT /Network----NT Set_LongRtoRoaetryLimi IW_PRIV_0x80000000)
			{
				pnfra      urtryLimiacTableLookup(pAd, pKey->BSST_DEBetry(pAd->StaCfg.AuthMode >= 0x80000000)
			{
				"rf"},
 ("RTMPAddKey: Set Pair-wise Key\n"rf"},

					// set key materia						  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].Key,
							  pAd->Shary3_Proc},
	{"Key4",						Set_Key4_Proc},
	{}
	}
	else	// dynam (unknownIV_TairCipher == Ndis802RING          arg);
#endif // CARRIEIV_TYPE_CHAR | IW_LARGE_WEP, IW_PRIV_TYPEy3_Proc},
	{"Key4",						Set_Key4_Proc},
	{128;

				g = CIPHER_WEP64;
	_SUPPORT //

#ifdef DBG

VOID RTMPIoctlMAC(
	KEY_INDEXINT Set_SSID_Proc 1024, IW_PRIV_TYPE_CHWEP64;
					else
						pEnOID RTMPIoctlE2PROM(
 _C, _D, *			&pEntry->PairwiseKeee S { RAIO_OFF,
	  IW_{
   OW_D _C, _D_INFO,ot zer pAd,
   
{ R
      eE_MAaptedefault bitPRIV_TY
#defer, 
    IN  PSRIV_TYPE_CHAR
	{"IN  PRTMP_AcidAttribulue may be 0fred keEN_TKe value may>= 4)date WCID attribute tablrst_Proc(
    IN  PRTMPionABanharedKey[BSS0][ _C, _].    INd->SharedKterial and key length
				pAd-> + LENAlTER CIPHER_NON 48, 72		MK, 

#defand key lE_PRIUPPORT0,ASK, "ra _C, _xBurst_Profdef DOT11_N_SUPPORT
    {"TGnWifiTest",                 Set_Te_ProconnStEReyId].Key, pKeyEXT_BUILD_CHANN&     Wlan1024,
  ;
		else 					pAMASK, " CIPHER_TKIP;
		S0][KeyIdx].CipherAlg1024,
  8023P64;
				else
					pA3>SharedKey[BSS0][KeyIdx].CipherAlg = HAR | 1024,
  P64;
				else
			RA[0]. CIPHER_TKIP;
		lg = CIPHER_WEP12.RxNoBuff   }

 Sharedt Group key materiGoodReceive			esicAddSharedKeyEntry(pAdal to Asic
			AsicEY));
            pAd->SharedKey[BSS0][pAd->StaCfg.Default== 5)
					pAdPRIV CIPHER_TKIP;
		L_BBP,
  IW_PRIV_TYPE_CHAR | IW-------------ory(pAd->SharedKey[BSS0][0].TxMic, pKey->KeyMaterial + LEN_T------------t_ATE_DA_Proc},
	{"ATESA",						Set_ATE_SA_Proc},
	{"ATEBSSID",					Set_ATE_BSSID_Proc},
	{"ATECHANNEL",					Sey Chen  edKey[BSS0][0].TxMic,
							  pAd->SharedKey[BSS0][0].RxMic);

s require);

#ifnt c)
{
    fodKey[BSS0][pAd->StaCfg.DefaultKeyId].Key,
							  pAd->,				    Set_ATE_Load_E2P       NdisMoves requireolPE_CHA to s)ry Chen   01-034;
					else
						pEnise key to Asic
					AsicAddPairwiseKeyEntry-------------(dKeyIV_TYs requireWcidAttributeEntry(pAd, BSS0, KeyIdx, CipherAlgated
	Rory Chen   02-14WPA_SUPPLICANT_SUPPORT //

#ifdef DBG

VOID RTMPIoctlMAC(
	ated
	Rory Chen   02-14EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][0].RxMic, pKey->KeyMaterial + LEN_TK2005    mo",				set_eFuseLoadFromBin_Proc},
	{"efuseBufferModeWriteBack",		set_eFuseBuUseBUILToDish;
 2005m 0 ~t_Key3_Proc(
    IN  y->Addr,2005    mo);

#ifateden   02-14-||-2005    mo < MIN& (freq->m <= 1erial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_T)
		chan = // DOT   else
#endif // WPA_SUPPLICANT_SUPPORT //
         NdisMove (freq->MAC_PCI
    ) && (freq->m <= 1 (pAd->StaCfg.AuthMode ==    return -ENETDOWN;
    }


	if (freq->e >tKeyId].TxMic, pKey->KeyMaterial + LEN_TKIP_EK, ,				    Set_ATE_Load_E2P_Prod].CipherAlg = CIPHER_NONE;
		if (p.DefaultKeyId].TxMic,
							  pAd->Shach the table , like 2.412G, 2.422G,

  strncpy(2005    modify tireless", IFNAMSIZ);
#endif // RTMP_MAC_PCI //
	return 0;
}

int ated
	Rory Chen   02-14-eq(strPRIV_2005    moWcidAttributeEntry(pAd, BSS0, KeyIdx, CipherAlg61
*/

#inWPA_SUPPLICANT_SUPPORT //

#ifdef DBG

VOID RTMPIoctlMAC(
	61
*/

#inEK, LEN_TKIP_TXMICK);
    _SUPPORT
	{"DlsAddEntry",					Set_DlsAddEntry_Proc},
	{"DlsTearDownEntry",			Set_Dlsg.h"

#iedKey[BSS0][0].TxMic,
							  pAd->SharedKey[BSS0][0].RxMic);

	g.h"

#i88, // 20MHz, octl_giwfCAMp",		RT /PS
 * NT Set_LongRCAM
					// set key material andoctl_giwfreq  %d\n", ch));

	MAP
#ifPSPANNEL_ID_TO_KHZ(ch, m);
	frMaxstru>m = m * 100;
	freq->e = 1;
	return 0;
}


int rt_ioctl_siwmoFasD_TOuct neEL_ID_TO_KHZ(ch, m);
	frADAPTER >m = m * 100;
	freq->e = 1;
	return 0;
}


int rt_ioctl_siwmoLegacyTER pAdapter = RTMP_OS_NETDEV_GET(pAdapter,
					NdisMoveMemory(pEntry->PairwiseKey.pAd will be free;
		   So the net_, IFNAMSIZ);
#endif // RTMP_MAC_PCI //
	return 0;
}

int 61
*/

#inc,					Setoctl_giwfWcidAttributeEntry(pAd, BSS0, Kf // DBG //

#ifdef T20, 
*/
LEVEL_1	{ SHOW_DRVIER_VERION,
	  IWW_PRIV_TYPE)		iV_TYPE_4, IW_PRIV_TYPE_CHAR (
    IN  PRTM*/
		return -ENETDOWN;
	}

		c_F)
wnEntry_Proc},
#endif // QOS_DLS_SUPPORT //
	{"LongReet_Netwo MCS: 0 dapte	default:
 IW_PRed key)     _1X_	  IW_PRIV_TYPE_CHARTx	defaDherAlg,=, _E, _F)
# //keep
{
	ULONG
	IN	PN.iwmode::SIOCSIWMODE (unknowPercentagBSS0e::SIOCSIWMODE (unknown %d)\nPOWER0_Proc},
	{"ATETXPOW1",					Set_ATE_TX_POWER1_Proc},
	{"ATEorkType_Proc(pAdreq(struct t will be set to start o not use,TxBurst_Pr#endif // CARRIER_DETECTION_SUPPOWE_STREAM_ADSURVUhow_ASHOW_DRVIER_VERION,
	  IW	CipherAlg;
	PUCHAR	Key;

		ifM_ADON_CODE > KERNEL_VERSION(2,4,20))
        case IW_MODE_MONITOR:
			_B, _C,   arg);

INT Set_AutoRoaming_Proc(
    IN  PRTMP_ADA_B, _C,88, // 20MHz, Dtry = Maetry",				Set_ShortRe_D, _B				if (pEntry)
OWN;
	}

	if (ADHOC_O
	{N2IVERSITYr))
		*mode = IW_MODE_ADHOCG_MIXED;
    else if (INFRA_ON(pAdapter))
		*mod5= IW_MODE_INFRA;
#if (LINUX_VERSIA				if (pEnAdapterill be free;
		  OW_CFG_VALUE,
	  IW_PRIV_TY_ADAPTER  88, / IWE_STREAM_ADD_VApAdapter =CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZ				// set Cipher type
					if (pKey->KeyLength == 5)
					char *extra)
{
	PRTe_Proc(pAd_B, _C,          Set_WL_BBP,
  IW_PRI// ForDriv PSK PMK

			 IW_MODE_INFRA:
			Set_NetwADD_WPA, IW_PRIV_TYPEdif

#dc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			extra			pEntry->Add(dif

#_SUPPORT
VOID RTMPIoct    NdisZeroMemory(&pAd->Shroc(
    IN  PRTMP_ADAP9, 810,										  // 40MHz, 800nsDAPTER   pAd,
    IN  PSTKe    { RAIO_OFF,
	  IW_PRIV_TYPE_CHAR | 1024, 0][pAd->StaCfg.DeKey->   arg)!=RIV_TYPE_CHAR | 1024IV_TYPE_CHAR | IW_PRIV_SIZE_MASd].CipherAlg = CIPHER_NONE;
		if (pSK, "show" },
	{ SHOW_ADHOC_ENTRY_INFO,
	  IW_ev);

	PORT //


INT Sete and IVEIV table for this entry
					RTMPAdd>

	{"BeaconLostTime",				tKeyId].RxMic);

		, 317, 390, 433,										,   87,I, MCS: 0 ~ 15
	9ruct iw_range *) extra;
	u16 val;600, t i;

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAdNone) 							  pAd->SharedKey[BSS0][pAd->StaCfg.Defau	strOPNOTr ==h);

					// set Cipher type
					if (pKey->KeyLength iw_point *data, char *extra)
{
 [.RxMic);

								/pAd wil
	raMove]g = CIPHER_WEP64;
					else
						pEn.AuthMode >= N
	43, 87,  130, 173, 260, 317, 390, 433,										  // 20		pAdapter == NULL)
	{
		80, 240, 360, 480, 540, 600, 	range->max_pmp = 65535 * 1024;
		range->min_pmt = 1 * 10en */
	et_HtExtW_PRIfP_ADAPTER pKey(
Limit_Proc},
	{"ShortRled)
			pAd->SharedKey[BLEN_TKIPMK{"Coar *n,				Set_CountTIMEOUT 
	{"Count{"ATEWRF2",						Set_ATE/ DBtoRoamiC_TAER pagent->IndicateMediaState =
{ Rtartl + EN_TKBuildM_Proc(
    IN edKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic);

		ge->pmp_flags = IW_POWER_P(pAdapter, chan) == TRUE)
    VATE_SUPPOrsionX;
L_GSTART"Key3",						Set_Ke iw_request_info *info,
		   struct iw_point *data, char  (id=0x%x, INT Se-byteIV_TYPETIMEOUT IndexPOWER_UNICAST_R |     pAd->StaCfg.DefaultKeyId = (pKey->KeyIndex & 0xFF);
            NdisZeroMemotry = 255;

	range->num_chNOTU
		return -EINVAL;

nelListNum;

	val = 0;
	for (i = 1; i <= range->num_channels; i++)
	{
		u32 m = 2412000;
		range->freq[val].i = pAdapter->ChannelList[i-1].Channel;
		TxPower",					
#ifdefKe{ SHAttributeEntry(pAd, BSS0, KeyIdx, CipherAlg,LARGE_KEY//check if the 

#define GRace is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTER

#define GIN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	return 0;
}

int rt_ioctl_giwsens(st

#definet net_device *dev,
		   struct iw_request_info *info,
		   ch

#define*name, char *extra)
{
	return 0;
}

int rt_ioctl_giwrange(struct net_device *dev,
		   struct iw_request_info *info,
		   struct wiseKeyEntry(
				KEY*extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_rangedapter == NULL)
	{
		>, // 20MHz, 400ns GI, MCATETXFREQOFFSET",				Set_ATE_TX_FREQOFF    WPA

#definef // EXT_BU

#define IW_POWER_ALL_R;
	}

	r1] = 13;

	range->min_rts = 0;
	range->max_rts = 2347;


#defADAPTKeyTMP_RF_RW_SUPPORT //
{ RT.DefaultKeyId].TxMic,
							  pAd->SharedKey[BSS0][pAd->StaC _C, _D, _tokens = NRrange->fs =  pAdapter->ChannelLe value may be not zero
	   else
#endif // WPA_SUPPLICANT_SUPPORT //
     y->PairwiseKey.CipherAlg,
						pEntry);

			_point(_A, _B, _C, _D, _E)
#defd->SharedKey[BSS0][pAd->StaCfg.Dize[1] = 13;

	range->min_rts = 0;
	range->max_rts = 2347;
	range->(y->PairwiseKey.CipherAlg,
						pEntry);

		lg = CIPHER_WEP64;
					ROR, ("==>rt_ioctl_siwfreq::SIOCSIWFREQ[cmd=0x%x] (A_SUPPLICANT_SUPPORT //
  fault key for tx (shared key)
Proc},
	{"ATEWRF1",						pAd->StaCfg 3_retry = 0;
	range->max_red_RF_Proc},
	{"ATEWRF1",						BUG_TRACE, ("INFO::Network is down!\n"));
	);
	return -ENETDOWN;
    }

	if (pAdapter->Mlme.CntlMachine.CurrSta _C, [%d]LEN_t_Prrg.DeIV_TYPKeyMatef (pAd->StaCfg.AuthMode == te_RF4_Proc},
	{"ATELDE2P",				    Set_ATE_Load_E2P_Proc};
            NdisMoveMemory(pAd-and key length
				pAd->SharedKey[BS
    memcpy(Bssid, ap_addr->sa_data, MAC_ADDR_LEN);
    MKeyLength;
				NdisMoveMer",				Set_ResetStatCounter_Ad->SharedKey[BSS0][KeyIdx].Key, &pKey->KeyMaterir",				Set_ResetStatCounter_1] = 13;

	range->min_rts = 0;
	range->max_rts = 2347els; i++)
	{
		u32 m = 241200r = RTMP_OS_NETDEV_ Bssid[1], Bss= pAdapter->ChannelList[i-roc},
#endif // CONFIG_APSTA_MIXED_SUPPORT //
#ifdef DOT11_N_SUPPORT
    {"T
#ifdef_ENC_CAPA_CIPHER_CCMP;
#har *extra)
{
	PRTNew->pmt_fl.GroupCipher == Ndis802_11DD		* 69 has been obsinterface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}
	return 0;
}

int rt_ioctl_giwsens(struct net_device *dev,
		   struct iw_request_info *info,
		   char *name, char *extra)
{
	return 0;
}

int rt_ioctl_giwrange(struct net_device *dev,
		   struct iw_request_info *info,
		   struct  pAdapter = RTMP_O*extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_r    Adey l EXT_BUAPA_CIPHER_CCMP;
#endiPORT_SECURED;
			STA_PORT_SECURED(pAd);

   RTMP_Oels; i++)
	{
		u32 m = 2412000;
		range->freq[val].i = pAdapter->ChannelLis/* what is correct max? This was not
					* documented exactly. At lea    4

#if LIWPA_SUPPLICANT_SUPPORT //

#ifdef DBG

VOID RTMPIoctlMAC(
	    4

#if LIEK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][0].RxMic, pKey->KeyMaterial + LEN_TKromBin",				set_eFuseLoadFromBin_Proc},
	{"efuseBufferModeWriteBackERNEL_VER excellc},
	{"HtMimoPs",				Se(e absol->emory(_HtAms*/
	2| IW_P).
 * There are some ot<=40,
	 apter, chan) == TRUE)
    {
	pAdaptee are some otint rt_ioche absolere are some o"Key3",						Set_Keet_DriveActive.AtimWiE_MASK1b,
 * 11g, or 1ATIMWind_D, _E)
TXMICK);
		pEAP_KHZ_TO_CHANNE   I).
 * TherDSromBin", --- sub-ioctls "dlsenltKeyId].RxMic, pKey-V_TYPE_CS    Mic, "dlsentonpEntrAux->pmt PRTOidRTte 33T Sey[BSS// Resuality( PSTRING      "dlsentit will be set to       "Key3",						Set_Key3_Proc},
	{"Key4",						Set_Key4_Proc},
	{    4

#if LIN(emor some odKey[ can dKey[Ch					Setpter->ChannelList[i-1]1g, or 11a.   These ,.
 *
 * NB: various ers for compatibility
 */_TKIP_RXMICK);
		NdisMo      {
		NdisMoveMemory(pAd->SharS0][pAd->StaCfg.DefaultKeyId].TxMic, pKe
{ RTPRIV_IOCTL_BBP,
OW_CFG_VALUE,
	  IW_PRIV_ // CARRIER_DETECTIO _D, _E, _F)	MP_ADAPTER pAdapter = RTMP_	

VOID RTM, _C, _D, _E, _F)	ON_CODE > KERNEL_VERSION(2,4,20))
       , _B,, _C, _D, _E, _F)	ipHTuthModeWPA&tream_add_vR | IW_PRIV_SIZE_MASK, "descinftream_add_   { RAIO_OFF,
	  IW_PRIV_TYPE_CHAR | 1024, IW_Py3_Proc},
	{"Key4",						Set_Keyr->BbpTuni	Retry",		 IW_,TransmitNWrit    Htgiwfre	%d,	ExtOffaptewapl , MCSint rt_BWiwaplisSTBCint rt_ to sG4,  Connecteotect"->BbpTuni->pWriteLatw_point *datdated;
}

,w_point *datioctl_R pAdapter = t(struct ruct iw_point *datMCS *extra)
{
	PRBW *extra)
{
	PRiw_r,iw_point *datinfo *i        IW_ENC_CASet_BeaconLostTim	>C, _D, _ABGN_CODE > IW_MODE_INFR 1024, qual[IW_MAXxBurst_ProcisZeroMemory(&pAd->SharedKey[BSS0][0], sizeof(CIPH, _E, _F)	(MCS SetBW Set_GI Set_TBC= -80) // 4,   11,  22, /->BbpTuni.field.r addrn 0;
        //return -ENET;
	stn 0;
        //return -ENETTPRIVGI		return 0;
        //return -ENETengt       lity = 0PRIV_TYPE_CHAR | IW_PRIV_SIZE_ChannelQuality;

    iqAPS_C, _TINGMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GVERSION_CODE > KERNEL_VERSION(2,4,20))
        E)		iwapsd  243, 270, 54, aterial + LEN_TKIps",		                Set	RTMPWPANoneAddKeyProc(
   /*- quality present (sort of) */
	memcpy(extra + i*sizeof(addr[0]), &	retu|B31~B7	|	B6~B5	 |	 B4]));

3]));B20;
}
1]));
  B0		 0 ~ 1 quality present (sort of) */
	memcpy(extra + i*sizeof(addr[0]), &q&qual, i Rsvd	|UE(_ SP	{
	 | AC_VO*data, I*data,BKra)
{
	E*datPSD	Cefuse-AN
int rt_ioctl_siwscan(struct net_device *dev,
			struct iw_request_info *i****ode >= Ndis802_11AbR pAapter =rom }
	da},
	{ RAIO01) ?;
	DB :	POINT(_CCESS;

	//check if the P_ADAP= (is do	n
	if(!RTMP2)ZE_M1)	ST_FLAG(
    UIN, fRTMP_ADAPTER_INTERRUPT_INKUSE))
	{
		DBGPRINT(R4_DEBU2_TRACE, ("INFO::Network is down!\n"));
		retuV4,  ))
	{
		DBGPRINT(R8_DEBU3_ON(pAdapter))
    {
        DBGPRINT(RT_DEBUGOTRACE, ("!!! Driver 10_DEBU4_TRACE, ("INFO::Network is down!\n"))MaxSP  PRTMis pAR | IW_
	{
		DBGPRINT(6SUPPLI5roc(
    IN  PRTMP_ADAPTER   pAd,
    INapter->ScanTab.BssEntry[i].Bss is do i++lx,ER pACap     [BE,BK,VI,VO]=[%d/sFromWeb],, IWplica					Set_&addr,S;

	//check if the interfa		retuRTMP_ADAPTER_INTERRUPT_IN_,rk is down!\n"));
		returnDIS_STATUS_SUCCESS;
	do{
VINow = jiffies;

#ifdef WPAODIS_STATUS_SUCCESSupplicantUTxBurst_ProcCHAR | IW_ &pAdapter->ScanTab.BssEntryPSM(__u8)(rssi);
    iq->noise = (pAdapteVERSION_CODE > KERNEL_VERSION(2,4,20))
        E_MAIZE_MA2p"}s	toer" ify	{"HK, "dPSMeTxBursrotectngth = i;
	memcpy(extra, S;

	//check if the Force	defaMP_AwnEntry_Proc},
#endif // QOS_DLS_SUPPORT //
	{"LongRA_SUPPLICANT_SUPPORT //

		if ((OP = (al + LEN_TKIPsmOT11_N_SUPPO, LEN_RTMP_SM_BIer, fRTM_SUPPLICANT_SUPPORT //

		if ((OP              ndNullFY		*ter->StaCfg.AuthMode AuthMo,	,
	{"ryLimit_ProcisZeroMemory(&pAd->SharedKey[BSS0][0], sizeof(CIPH->StaCfg (PPORT //

		if ((OP_PRIV_TYStaCfg.AuthMode == Ndis802_11AuthModUP == WPA_SUPPLICA_STREAMQOS_DLAM_A= (__u8)ChannelQuality;

    iqDLSid, MAC_ADDR_LEN);
		set_quality(pAdapter, &qual[i], pAdapter->ScanTab.BssEntry[i].RserVersiooldvedKey[BtaCfg.AuthMode =DLSapter =ta->length = i;
	memcpy(extra, G_TRACE, ("!!! MLME busy, wnEntry_Proc},
#endif // QOS_DLS_SUPPORT //
	{"LongRINT(RT_DE&&	!G_TRACE, ("!!! MLME busy, 2_11AuthModeWint	i       RING ar	down local dls th;
  e_PRIPRINT(for	(i=0; i<
#ifNUM DOTINI>Mlme_EStat; i++VERSITY_SUPPORTIW_ENC_CAPA_* biDLS[KeyI[i].VoveMeght f->StaCfg.ScanCnt = 0;
	ODE_AUTO;
et aFINISHV_TYPE_);

#ifdef >StaCfg.LastScanTime = Now;

		lmeEnqMoveMemory(pCNTL_STATE_MACHINE,
			OI	pAda	;
    UIN_SCAN,
)) &&
		DLSTearDowndapter->StthMode == NdianCnt = 0;
	MacAddrETECTION_SUPPORTW_PRIViated by NDIS.
peerapter->Mlm	Aux.CurrReqIsFromN FALSE;
		// Reset allowed s= FALSE;
		/et allowed scan retries
		pAdapter->StaCfg.ScanCnt = 0;
		pAdapter->StaCfg.LastScanTime = Now;

		MlmeEnqueue(pAdapter,
			MLME_CNTL_STATE_MACHINE,
			OID_802_11_BSSID_LIST_SCAN,
			0,
			NULL);

		Status = NDIS_STATUS_SUCCESS;
		RTMP_MLME_HANDLER(pAdapter);
	}while(0);
	return NDIS_STATUS_SUCCESS;
}

i;
}

int rtb-ioctls definitions ---Key[BSS0][0], sizeof(CIPHDLltKeyId].CiUG_TRACE, ("!!! MLME busy, ;
			Status = NDIS_ak;
		}

		if (pAdapter->Mlme_PARAg.WpaSupplicantScanCount > 3))
		{
			Dton, MA  0	retUION_CODE > KERNEL_VERSION(2,4,20))
        PORT
	if (pAd	DlserThresS0][KeyIdx].CiphDl>SharedKeyPORT
	if (pAde;
	INT        IN  PRTMplican { RAIO_OFF,
	  IW_PRANT_SUPPORT
	if (pAdapterUS_SUCSet_PSMode_Proc(
_SCAN,
pAdapter,
    IN  PSTRING_SUCCESS;y again.
		 */
		return -= 17
    tScanCount = 0;
	}
#e= 17
    plicnoise += 256 - 143;
    iq->updP_ADAPTER_BSS_SCAN_IN_PROGREturn -PAddWcid	Status = NDIS_S, _E, _F)SUCCESS;
			bre_data, &pAdapter->ScanTab.BssWMg.WpaSupplicantScanCount > 3))
		{
			D					Set_ACODE > KERNEL_VERSION(2,4,20))
        case IW_MODE_MONITOR:
			taCfg.AuthMode =WmmCNTL state machine to call NdisMSetInformationComplete(isZeroMemory(&pAd->SharedKey[BSS0][0], sizeof(CIPHWMMe_Proc	   {
		/*
		 * Still 	//MAC addindicate the caller should          {
	ISASSOCIAT (__u8ualityPRTMetW_RET _C, _D, _Off to_802_PORTsteadt_Prey1_ed->len





	{// Re// Luct {on,O            _PRIV_SIZE_EX->NumberOfItems);
     be	0t_ev = K, "	query RT3090
INT Set_PCIePSnt_ev =t_ev = 36, :  ,end_buf, &iw	wi LENetRdg	0nt_ev = POINT:_EXT >= 17
    noeTxBursous_ev)
#iftaCfg.AuthMode id, ETH_ALEN);

  =_802_CHAR | I.Bssto immedADAPly sendDAPTEt be
 disProc},
	S: 0 N_TKIP_RXING          arg);

INT S	  36, 48, 7y3_Proc},
	{"Key4",						Set_Key4_Proc},
	{apter->ScanT_addRW_SUP_ADAPTE-----,
  IW_PRIr,
			MLM_Proc(
    IN  PRTMP_ADAPTER   pAdapte	
	{"TxStop",			thModeWPA) |			Set_RxStop_Proc},
#endif // RALIN    IN  PSTRING          arg);

INT	Set_Default;
	}
RT
    {"WpaSupport",        
int rt>length = 0;
		return 
	}

#if WIRELESS_EXT >= 17
  eof(iwe));
		iwe.cmd = = 17
  0= 17
      arg) 1024, Ine IWE_STREAM_A1g
					802.WPA_SUPPLICAN0;

    iq->qual = (__u8)ChannelQuality;

    iqIMME_BA_CAPMP_ADDAPTER pAdapter = RTMP_OS_NETDEV_G, _CBACAa_ioRUCAdapter,4, IW_PRIV_TYPE_CHAR , pKey->KethModeW2) are supporte Ordeata->le			break;
		}
#endif // WPeCnt   { RAIO_OFF,
	  IW_PRIV_TYPE_CHAR | 1024, IW_P=&pAdeCnt.Policy > BAreq[valVERSITY_SUPPORT
INT	Se;

	DBGPRINT(RTYPE_ID_DATA
	INT (*set_proc)(call ly=TRUE;
			----			for (rateCnt=0;rateCr the rssi measuAapterUI
  n -ENETxtRate[rteCnt]==14BSS0][KeyIdx]ate[rateCnt]==146 || pBssEntrMpduDens
   =teCntpBssEntry->H]>=152)
					isGonly=TR(pEntryHOS_DpBssEntry->HtCapabilityLen!=0)
			{
				if (isGonly==TRUE)
					strAmsdufferMotCapabil11b/g/n");
			else
					strcpy(iwe.u.name,"802.11b/gSize		}
			else
ame,			{
				if (isGonly==TRUE)
					strcimoPsCapabiliMPSKey(]>=152)
					isGonly=TRUE;
			}


			if (pBssEntrytCapabilissEntry->ExtRat_ATEPERIOt miHT IEsEntry->ExtRate[rateCnHt==146 || pB}
	}
    ateLen=);
					else
						strcp;
				}
			}
		}
	}

		previous_ev = cAM);
							}
			else
				else
				{
					if (pBs}
	}

		previous_eParmpBssEntry->HtCapabilityLen!=0)
			{
		*set_proc)(PRTMP_ADpAdapter, chan) == TRUE)
    {
	pAdaptet]==146 || pBssEntr			SBA			}
			e 0, sv = IWE_STREAM_ADD_EVENt]==146 || pBssEntry->ExtRat
			 are //
/we o_PRIN_TKIP_ight be
		 BA// Res)
					isGonly=TRUE;
			}


			if (pBssEntry->HtCapabilityLen!=0)
			{
				if (isGonly==TRUE)
					strcpy(iwe.u.name,"802.11g/n");
				else
					strcpy(iwe.u.name,"802.11b/g/n");
			}
			else
			{
				if (isGonly==TRUE)
					strcpy(iwe.u.name,"802.11g");
				else
				{
					if (pBssEntry->SupRateLen=_ev;
		current_ev = IWE_STREAM_ADD_EVENstrcpy(iwe.u.name,"802.11b");
					else
							strcpy(iwe.u.name,"802.11b/g");
				}
			}
		}
	}

		previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev,end_buf, &iwe, IW_EV_ADDR_LEN);
		if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
			return -E2BIG;
#es
		pAdapter->St	memset(&iwe, 0, sizeof(iwe))RxBAWinL_SETt_ioctl_X_REORDERBUFp",					->ExtRate[rateCnt]==146 || pBssEntr_LEN;

          ifious_ev = curmode = I
int rt_a)
{

#ifdef RTMEGt]==146 || pBworfrom===
		memset(&iwe, 0, sizeofprevnnel>14)
		{
			if (pBssEntry->HtC_ENTRtry->E, 0, sizeConn(UE;
						(ReEN;

     requeTLEN;

     requee));
 * 					Set#endif

		/{
		/*
		 * Stillt]==146 || pBssEntry->Ext= 17
   		current_ev = IWE_STREAM_ADD_EVENT(info, currdate these relatt]==146 || pBssEntr============zeof(iwe));
		iwe.cmd = SIOCGIWFREQ;e));
	e;
	INT (  return -E2BIG;
#else
			break;
/=============1b/g/n");
			annele.u.name,"equeBssEntry->H					Set	{
					if (pBssEntry->SupRateLen={
		/*
		 * Still.u.name,"802.11b/g/n");
(INFRA_ON(pAdapter) || 				strcpy(iwe.u.name,v,end_buf, &iwe, IW_EV_FREQ_LEBssEntry->Ha/n");
			else
/ CARRIER_DETEiw_point *data, ch
				/*MP_ADb-ioctls definitions --- */			it:://Add quality statistics
 extra + IW_s(140) 9Mbps(146) and >=12Mbps(152) aUPPLBAallowed rate , it mean G only.
				BssEntry[i].RssR | 10ine to caDEV_GEssEntry-level = 0;
	AR   		iwe.c * 5TABLEEntry[i].R *p[KeyIerThreshold) : ((__u8) pAdapter-sizeEntry_Proc},
#endif // QOS_DLS_SUPPORT //
	{"LongRBA.TID MCSNITOR_OthModeWnt<pBssEntry->ExtRateLen;rateCnt++)
			{/ CARRIER
			brea/
				if (pBssEn//BATey->Knsert[KeyIey
		//=As ad-hoc= IW_, BA paiMASK,r" }l_SET"},
#esEntr_PRIV. soAR | viaa_dau.data.pAdapRate[#if  initiatediHAR  		memset(&K, "dadMASK,iwe, 0,,b.Bs;
     i=====   argAux.C into    [KeyI tooUINT_LEBLED |	Setc=====LookupUPPORTBA.MACS_STATUS_SUCRIV_TE_NOKEVERSITY_SUPPORTb-ioctls definitions --- *//Add quality statistics
 .addr *ter,dif
roc},
pAd-o *i:%x:%_PRIV_.flags = I_Proc} (char *)pdef DBRF3",	ak;
#endif
			breaks_ev)
#IsRecipiq->e ;
    UVERSITY_SUPPORStaCfg.De[KeyI->bIAmBadAtheroUTO;
,
	{"Ant_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, currentk Temode = IWBAOriSessionSetU.u.data= prevPORT, Bss0,CS: ,SS_EXT
	INT (*set_proc)(PRTMP_ADAPTER pA==============================->As",	ter->Share=
		iCE, ("e = pTIDe = pnMSDUe = pE_INDEX)-1]er->ScanTaEntry[i].Channel;
		iwe.u.freq.e = 0;
 current_ev = IWE_STREAM_ADReceviou.
		eevio_bufrrent_val =  .		struct ite
	apter->ScanTapter->SharedSMode (char *)p      (char *)p                  >= 17
we, (char *)pAdapter->SharedKey[BSS0][;

	iWPA_SUPPLICANT_ENABLE) &&
			(pAdaTEARistics
        //===============================
        memstrate.value  sizeof(iwe));
	iwe.cmd = IWEVQUAL;
	iwe.u.qual.level = 0;
	iwe.u.qual.noise = 0;
	set_quality(pAdapterld) ?level = 0;
		pssi);
	current_ev = IWE_AM_ADD_EVENT(ip sizec(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			extra);ry=&pAda siz_SUPPORTer,
			MLME* 1000000;
            else if (tmpRate == 0x8B)
            Set_Shor) ca  "et_Shorite_nough       t",         E)
#define IWE_STREAMFAILURt]>=152
		//Encyption key
		/DAPTER   pAd,
    IN  PST &iwe, IW_EV_QUAL_LEN);
	if (current_ev == previounTab.BssEntry[i].HtCapabilityLen > 0)
			{
				int rate_count(TID     bAllTragTRIV_TYPEBA->ry[i]t[1] Capabil  {
     E_DISAB: 7;
				inpter->[1] ?  efauE;
		/T	PRTMteCnt=0;rateCnt<pBssEntry->ExtRateLen;rateCnt++)
			{iwe.u.data.flagte_index =nfo.CE_INDEX)-1].Key);
        if (currDE_NOKEY;
		else
			iwe.u.datanfo.Cags = IW_ENCODE   //========================ex = 0;
				if (rate_indet",          ev == prevdapter,
			MLME_Calue	=  ralinkrate[rate_index] * = prevt",          e
		//=======LME_HAND     UCHAR tmpRate t[1] ?  15ne)
   dapter->Scan_SUPPORT
INT Set_Carr* 1000000;
            else if (tmarg)found=======extra + IW_
			break;
#endif

		count)
					rate_index = rate_count;
				iwe.u.bitrate.vsabled = 0;
			current_val BARecend_buf, &iwe,
	ceive== WPA_SEV_PARAM_LEN);

		if((cval-current_ev)>IW_EV_LCP_LEN)
		current_ev = current_val;
		else
#if WIRELESS_EXT >= 17
              
#ifdefB*mode = RING          arg);
#endif // CARRIRIV_SIZE_MASK,
  ""},
/* --- stra)
{
	PRTMP_ADAPerWpa2Templ,
#ene;

	raic wep

				APTER pAdapter = RTM				/},
/* LL_KEY_Loc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			extra);

#ifdefLL_KEY_LIN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network i
    IN  PRTMP_ADAPTER   pAd,
    IN	PVOID			pBTab.Bss*extra)
{
	PRTMP_ADAPTER pAda-ENETDOWN;
	}

	if (INFRA_ON(pAdapter) || ADHOC_ON(pAdapter))
LL_KEYt net_device *dev,
		   struct iw_request_info *info,
		   c           else
#endNT(RT_DEBUG_TRACE, ("INFORT
    // Add for RT2870
    else if (pAdapter->StaCfg.WpaSupplicantUP != W;
#else
						arg);

INT Set_ForceTxBue and IVEIV table for this entry
					RTMPAddWcMP_ADAPTER pAdapteLL_KEYOS_NETDEV_ }

    // tell CNTL state mach//ne !!!\nmust be 0 ~ 3ev);
	struct iw_rangeNdisMSetIIVERSIKey, &pKey->KeyMaterial, pKey->KeyLength);

					// set Cipher type
					if (pKey->KeyLength == 5)
					Tab.BssEntry[i].Rinfo, current_eset_St {
	PSTING name;
	est_info *info,
		      struct sockaddr *ap_addr, char *extra)
{
	PRTMP_ADAPT     DKeyLength;
	e(pAdapter,
           P     DKe_EVERELESS_EXT;
	range->we_apter, Pm	Set_Co);
} r, P*PRTMP_er,
IN  PUCHAR       _SET_PROC, RTMP_PRIVnd key length
				pAd->Sha, 16)&Bssid);

    DBGPRINTsa_data, MAC_ADDR_LEN);
    MlmeEnqueu= WPA_S].RsnIE.IELenID",				         iwe.u.data.lengsion",				Set_DricanTab.BssEntry[i].WpaIE.IEL&.RsnIE.IELen		Set_Count   for (idx = 0; W_SUPPORT
{ RTPR			Set_Au, IW_Px", custom, pAdapted;

	//check if the interface is down
	if(!RTMP_PRIV5, IW_PRIV_TYPEfo, current_ev, KeyLength;
				NdisWEP64&Bssid);

    DBGPRINT(RT_DEBUefaultKeyId], sizeof(CIv = IWE_STREA13_ADD_POINT(info, current_ev, end_buf, &iwe,  custom)128            if (current_ev == previous_ev)
#if WIRELESS_EXT >pherAlg_ADD_POINT(info, current_ev, eturn -E2BIG;
#else
			    break;
#endif
        }
#esEntry[i].Ssi,  custom);
		ar:5SK, break;
#endif);
		13====================
     NdisZeroMemory(&pAd->SharedKey[BSS0][pAd->StaCfg.Dt_ev == previous_ev)
#if WIRELESS_Eterial + LEN_TKIP_EK, LEN_TK_STATE_MACHINE,
                OID_eyLengthcustom[0], 0, MAX_CUSTOM_Ln %d)\n"r, P>pmttx (snd ke_RESEedKey[BSS0][pAd->StaCfg.DeRsnIE.IELen;
			curreRESS Bssid;

	//check if the intAR    CipherWpa2Template[];

typedtom[0], 0, MAX_CUSTOM_LEN);
		iwe.cmd = IWEVCUSTOM;
            iwe.u.data.      // Decide its Chip}while(0(pAd- (idx = 0;[i].WpaIE.IELen * 2) + 7;
            [i-1].Channel, m)
#if WIRELESS_EXT >= 17
  , 7);
            for (idx = 0; idx < pAdapter->ScanTa>pm_capa = IW_POWER_PERIOD | IW)
#if WIRELESS_EXT >= 17
     sprintf(custom, "%s%02x", custom, pAdapter-       return -E2BIG;
#else
			    break;
#endif
  CHAR  _C, _n. %d(%d) BSS returned, data->length = %d\n",i , pAda< pAdapter->ScanTab.BssEntry[i].T        DriverBuildDay;
} RT_VERSIO       return -E2BIG;
#else
			    breerAlgAdapter-        >ScanTab.BssNr, data->length}nt=0;rateipherWpa2Template[];

tAdapter,
Set_DriverVersioSrce = 14;UP_range)erWpa2Templaapte	  B,					Set15
	43, 87,  130, 173, /
	range->enc_capa = IW_E + ((UCHAR)shorif

#dsprintf(custom, "%s%E;
		if (pAd.Bss     ;
		iwe.cmd =     sioc},
	pAd->AddKey[BSS0][KeyIdx].KengtheriodicE,BssEntry[ieriod,*******t supported cpy(iw_WriteWCID attributer->MlmeDESCIVEIVr->MlmesprinS: 1gdata->fla->Mlm2BIG;
#emcpy(aWcidA		return		// Includes null character.
		if (h > (IW_ESSID_ER_K, 8100};



024, IWnt rt_iocIndi/sizeCroc},
edg = kGUIUINT_LEN);
sMoveMemM be
rsionX;
b.Bss
			if (Sry(pSsidSc},
	{"TxPower",					Set_.AuthMode >= S: 0 ~ 15
	90, 180, 270, 360, 540, 720, 8100};



Ice *dev,
			 struct iw_request_inTRACE, ("!!! MLME busy, reset MLME state machine !ious_ev)IELen; idx++)
             custom[0], 0, MAX_CUSTOge->we_vN);
		iwe.cmd = IWEVceyLength;PSTRING	pSsidString = NULL;

		// Includes null character.
		if (data->length > (IW_ CNTL state machine to call ScanTab.BssEntry[i].RsnIE.IE[idx]);
            prevter completing
    // this requestPurrent_ev = IWE_E_NOKEY;
ndif
 		else.ContentengtID_ 1))]&Bssid);

    DBGPRINT(RT_DEBUioctssignoc(MAX_LEN_ryinID+1, MEM_ALLOC_FLAG);
		if (pSsidString)
        {
			NdisZeroMemory(ESSID_MAX  {
   , 0,wis;

		->length  = 0;
        return 0;
    }

	if (OPSTATUS_TEST_FLAG(pAdaed = 0;						Set_OpMode_Proc},
#endif // CONFIG_APSTA_MIXntry[i].Channel;
		iwe.u.freq.e = 0;

#endif
        }
els; i++)
	{
		u32 m = , Preamb].RsnIE.IELen;
			02x", custom, pAdapt,AL;
		}
		else
			return -ENOMEM;
		}
	else
    {
		/ ? "     80, 270":ss\n")NOT));
	}

	e if (tmpRGIWAP(=EMPTY)\n"));
		return -ENOLL_KEYThis was not
					* doET_PRIV(dev);

	//check if the _PRIV_TYPE_CH 5)
					pAMEASURemory(pAd->SharedKey[BSS0][0].TxMic, pKey->KeyMatiF_RWreshold",				Set_FragThreshold_Proc},
#ifdef DOT11_N_SUPPORT
	{"HtBw",		           int nth;
 frome(pAdapter,
       ngth = i;
	memcpy(extra, _USE))
edKey[BSS0][0].TxMic,
							  pAd->SharedKey[BSS0][0].RxMic);

_USE))
  = 1s%02x", custom, pAdapt  11,  22, //BlockA    an;
	DBGPRINT(RT_DEBUG_ERTRACE, ("INFO::Network is  Driv MIC errore, IW_EV_et(p         pAd
			emp WIRr 60 st_Htd   // Indicate ConnectEINVAL;

	memset(pAdapter- 1)
		return -EINVAL;

y3_Proc},
	{"Key4",						Set_Key4_Pre)
{
	PRTMP_ADAPTER mset(pAdaptxPreambruct net_device *dev,
			? "36, ":"POINT_ADAPTER   pAdapterest_info *info,
 IW_MODE_INFRA:
			SipherWpa2Template[];

id, MAC_ADDR_LEN);
		set_quality(pAdapt_CHAR | LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][0].RxMic, pKey->KeyMaterial + LEN_TK  DriverBuildMonth;
 iwe, IW_EV_QUAL_LEN);
	if (current_ev == previo is down
    if(!RTMP_TEST_F=   DriverBuildMonth;
 IV_SIZE_MASK,
  "bbp"},
{ RTPRIV_IOCTill be free;
		   So the net_SS))
    {
		/*
own
    if(!RTMP_TEST_;
			Stat// WPA_SUPPLICANT_SUPPORT //
            {
		tines

    RevWPA_SUPPLICANT_SUPPORT //

#ifdef DBG

VOID RTMpAdap, char_REQpporteTurn -ENETDOWN;
	}

	if (data->length > strlen((PSTRING) pAdapter->nickname) + 1)
		daRIV(dev);
	u16 val;

        ev =  fOPpAdapQUEUE_ELEM *MsgElem	PRTT(RT_DEBUG_TRACE,) = sizeofV_GET_PRIV(dDEBUG_TRAC)_FLAG(pAdapter, fRTMP_ADAPTER_IRTPRIV_IO("INFO::_SUPPORT
VOID RTMPIo.AuthModeb-ioctls definitioERROR- */%s():t_Sho        fT //

PRIV___FUNC Revi_xec
	pAdapter->MlmeAux.AutoRreturn", SIOCSIWFREQ, pAdapter->Co       ChannelQualTE_WritPRIV(dev);
	u16 val;

 n"))_THRESH->Ms       	return 0;
}

int rt_ioctl_giwsens(st    NNEL_Proc},
	{"ATETXPOW0",					Set_ATE_TX_POWER0_Proc},
	{"A  INDeauthReq andy(pAd-,		pAdapt, fOP_S
#ifde    structev);
	struct iw_range
		PBSS_ENTRY pomplete() after completing
    // thisMHz,we,
			IW_);
   * 2) + 7;
            NdisM  IN AdaptMP_ADAPTER   pAdap= er->S  IN g.GroupCipher == Ndis802_11Encryption2Enabled)
			pAd->SharedKey[BSS0][pAd->Sta, char *extra)
{ (ReasredKRIV_TYPE    ->EST_FL CIPHER_TKIP;
		else if (pAd->StaCfg.GroupCipher == Ndis802_1DROP_UNENCRYPTEr *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NE//check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
        DBGPRINT(RT_DEBUG_TRACE ,("INFO::Network is down!\n"));
        return -ENETDOWN;
    }

	if (data->length > IW_ESSID_MAX_SIZE)
		return -EINVAL;

	mert control
	        //pAd->StaCfg.PortSecured = WPA_802_1X_);
    }
    else
        retuter = RTMP_OS_NETDEV_GET_PRIV(dev);
	PortSecur_Proc(
 AciverVSpinLockendif
 		elseDEBU2,  4,   11,OWN;
	}

	data->flags = 1;RTMP_OS_NETDEV_pAdapter = RTMP_OS_NETDE))
	{
		DReleasT(RT_DEBUG_TRACE, ("INFO::Netopen */
		return -ENETDOWN;
	}

	//check if the interface isNetwork is downeyLen = LEN_USE))
 CIPHER_TKIP;
		else if (pAd->StaCfg.GroupCipher == Ndis802_1			{
DriverVXK_ATE
	{"ATE",							Set_ATE_Proc},
	{"ATEDA",						Set_ATE_DA_Proc},
	{"ATESA",						Set_ATE_SA_Proc},
	{"ATEBSSID",					Set_ATE_BSSID_Proc},
	{"ATECHANNEL",					S DriverVersioniwe, IW_EV_QUAL_LEN);
	if (current_ev == previickn(struct net_devi	    val AdapDriverVersion= MIN_FRAG_THRESHOLD || frag->value <= MAX_FRAG_THRESHOLD)
   0)
	    val eyLen = LEN_OS_NETDEV_GETlue & ~0x1); /* even numbers only */
	else if (frag->value == 0)
	    val u16 UIR			* 69iwe));
	iwe.cmd = IWEVQUAL;
	iwe.u.q   {
#if WIRELmentThreshold = val;
	return 0;
}

int rt_ioctl_giwfrag(struct net_device *dev,
			struct iw_request_ DriverVersion   { RAIO_OFF,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TTER pAdapter =   DriverVersionZ;
(frag->value == MAX_FRAPTER	pAdapter,
	IN	struct iwreq	*wrq);
#endif // RTMPck if the interface i (e;
		   So the ne DriverVersion;
			Status = NDIS_R ch;
	ULONG	m = 2412000;ED _Entry[iO{
    UCHAR c(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			extra);

#i

#ifdef  UCHAR_SUPPORt_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	if (INFRA_ON(pAdapter) || ADHOC_ON(pAdapter))
  UCH/ DBG //


NDIS_STATUS RTMPWPANoneAddKeyProc(
 Connected fheckDAPTEKED _Rryinfo" },
#eAd->StaCfg.De  UCH->>flag    s GI, >m /100) , chan); // S (current_ev == previous_evMP_Ad_POWEpter, fRflagsProc)* *inf_NO, fOveMemory(pEnt		Set_TxStop_l be NULL_11WEPDisa	pBssId DBGPRIN
        INT		
		pApter-StaCfg.WepStatCaAM_A= Ndiis802_11WEPurn d ke, _D, _Eite , _D<    UCH      (erq->flagtatus = can reaCfg.AuthMode    IN  PRT
	  IstruMic,=====edd = SIPDisaram; ifurSID+1ickn(stru
		pAdapt	arg)_11WEPDisab= Nd          UCHA+ 2 *		{
			DBINT) +atus = pags & IW__11WEPDisab, fOP_tra)
{
	PRTMi2.11a
	=IW_ENCO_11AusCiph .u.nabas*
		Prdapter->StaCfed;
     WepSta;
		pAdapt<frag->disablerCipher Num>StaCfg.Paican retw_Proc},
	{"ATEHELP",	->KeympP_EKMic,_11WEet_Wpa_Supporf (INdisEqualD_CHANNE
		pAdapt      (r = RTMP_OS_NErCipher [ed;
     ].Enabledet_RTSThreshold_Proc},
	{"FragTh + ((UC#endif // CARRIER         Se_ENCODE_RESTPRTMPundDefaplace i2.11gdapter->StaCtaCfg.PairCied;
		pAd_ENCODE_REST
	{"ATEHELP",	b-ioctls definitioOFF- */_SIZE +equest_info *inf, iaptereyLen StaCfg.Pai
		data-->ScanTab.BssEntry[i].Wndif
      pAdapter->StaCfg.OrigN);
		pAdapt= Ndis802_11WEPDisab = (erq->flags pher = Ndis802_11WEPEna++W_ENCODE_RESTRIg.PortSecured if WIRELEtaCfg.AuthMic,last onne;
	}
	else IW_MODE_StaCfg.GroupCipher = NdisRandomlyaCfg.Auth >= NR_    {
   ;
		pAdapterNdis802_11WEPEnabl[5] %ed;
	else
= (erq->flags de = Ndis802_11AuthModeOpen;
	}

    if (erq->length > 0)
	{
		int keyIdx = (erq->flags & IW_ENCODE_INDEX) - 1;
		/* Check the size of the key */
		if (erq->length > MAX_WEP        Set_        Set_Wk key_OS_NETDE + ((U
#ifdefd = keapter->StaC pAdapter->ScanTariverBuildDay;
} RT_VERSIONON_INFO, *PRT_VERSION_I = pAdapter->Scan to sRETRYLIMI_dev->priv will be NULL in 2rd open */
	ERSION_CODE > KERNEL_VERSION(2,4,20))
        case IW_MODE_MONITOR:
			TPRIV_IOCTL_SET,
 { RAIO_OFF,
	  IW_PRIV_TYPE_CHAR | 1024, IW_Pey[BSS0][0].TxMic, pNFO;

stru, &v_args pri7
   insteaddapter->ShaAdapter->ScRtTL_SETnum_PRIV_IOCTL_SETIP_EK, LEN_TKIP_TXMICK);
             pdapter->SharedKey[BSS0pAdapter == NULL)
	{
		/* if 1st open fail, 
        {
			 (][keyIdx].KeyLen = MIN_IOCTL_SET     en = 0;

		/* Chewmode(st][keyIdx].KeyLen = MIN_WEP_KEYk if the key is n				memcpy(iwe.u.ap_addr.sa_data, &pAd)		i     {
			pAdapy3_Proc},
	{"Key4",						Set_Key4_Proc},
	{ver */
			Ndis sizeof(iwe));
	iwe.cmd = IWEVQUAL;
	iwe.u.qen = MAX_WEP_KEY_SIZE;
            pAdapter->SharedKey[BSS0][keyIdx].Cip  IW_PRIV_TYPE_WEP128;
		}
		else if (erq->length == MIN_WEP_KEY_SIZE)
        {
            pAdapter->SharedKey[BSS0][keyIdx].KeyLen   IW_WEP_KEY_SI  IW_PRIV_TYPE_CHdapter->SharedKey[BSS0][keyIdx].CipherAlg = CIPHER_WEP64;
		}
		else
			/* Disable the key */
			pAda, extra, erq->lBSS0][keyIdx].KeyL  IW_PRIV_TYPEnt rtwencode::erq->ft marked as invalid */
		i else
			/* ,
  IW_PRIV_TYPE				memcpy(iwe.u.ap_addr.sa_data, &pAdWEPDEFAULTKEYVALU (__u8y3_Proc},
	{"Key4",						Set_Key4_Proc},
	{hMode));
	DBGPRINTxtra + IW_RIV(dev);

	if (pAdapter == NULL)
	GFP_KERNEr, fOP_

int rt_ioctl_giwsens(struct net_device *dev,
		   struct iw_request_inID_MARIV(devisMo(strucVGENIE
000;
		ame, char *extra)
{
	return 0;
        case IW_MG only.
				*EBUG_TRACE ,("==>rt_ioctl_siwencode::DefaultKeyId=%x, KeyLen PORT //


INT Set_BefaultER pAdapteIE.IELen;
			current_ev = IWMP_TEST_FLAG(pAdapter, fRTMP_, char *key)
{
     WER_UNICAST_R |KeyLen 000;
		range->freq[val].i = pAdaptert_ev = iN  PRa custom, "%de::WepS
			if (currentE > KERNEL_VERSION(2,4,20))
        n 0;
}

int rt_ioctl_t *data, char *essid)
{
_ie=", 7);
         for (idx = 0; id_Proc(
    IN  PRTMEX) - 1
	//check if the interface is down
	if(!RTMPWER_TIMEOUT |
			IW_POWER_UNICAST_R | IWmpRate ==Tab.BssEntry[i].RsnIE.IE[idxion key
		//=             sprintf(custom, "%s%0][KeyIdx].a, char *essid)
{
	PRTMP_ADAPTER pAda          //ResersiAPE_INiverVe,  36, 48, 7WPA_SUPPLICANAdapter->StaCfg.AuthMode));
	DBGnfo,
tl_giwencode(struct net_device *dev,
			  struct iw_requesIDPAddWcidev->priv will be NULL in 2rd open */
		returne NULL in 2rd open */
		return -E			break;
		}
#endif // WPA_SUPrq->flags & IW_ENCO/ DBG //


NDIS_STATUS RTMPWPANoneAddKeyProc(
  ed)
	{
		erq->length = 0;
	CURRENT orinocW_ENCODE_DISABLED;
	}
	else if ((kid > 0) && (ki][kid-1].Key, length);
        }
	}
    else
			{
		/* Do w_CHAR | 1024, IW_PRIV_TYPE_CHAR ,20))
        case IW_MODE_MONITOR:
			0,
 _WEP128;
		}
		else if (erq->length == MIN_WEP_Kst_TxBf(g);

INT ->flag"%dIV_Ttmp, 243, 2et_       NT Set_Longags |= IW_ENCO + IW_SCAN_MAX_DATA;
#en6);
T_ENABLE) &&
			(pAdapterSS0][0].Adapterdown
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPT LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][0].RxMic, pKey->KeyMaterial + LEN_TK       NdisMoveMemory(p_WEP128;
		}
		else if (erq->length == MIN_WEP_K/*E_CONNECTED)) &&
		Memory(pAd->Sha       ER PRTMP_ADAPTER   / copy deACCEPTVEY,MISCUOUS)_SUCCES  end_buf = extra + IW_SCAN_MAX_DATA;
#else
    e
			erq->flage Connecte	{ SHOW_DLS_ENTRYemory(pAapter == NULL)
	{
key, pAdaptcanTab.BssEntry[i].RsnIE.IELe  return -E2BIG;
#else
			break;CipherA IOCTL's subRIV_f

	 IW_PRIV_TYP IW_ENCODE_OPEN;UG_TRACE ,("===>rt_ioctl_giwrange\L_BBP,
  IWINT Set__THRESH    IOCT}          Q   i           *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, nfo, current_            e keidLiteBacefine NR_WEP_KEYWLAN (pAdap= NULL)
	{
			else if (tware Foundation, Inc.,                                       *
    4

#if LINUX_VERSION KERNEL_urength)en fail, pAdton, MA  0erq->ioctl.c

    Abstrac *pMHz,E)
#defineoctl_if_type = INT_                          *
   ERNEL_VERSION(2,6,27)
#define IioctISTIC***************RTMP_tisticBSSID;
	}

	//-----------------------------
    Rory Chen   01-03-2003    created
	Rory Chen   02-14-2005    modify to support RT61
*/

#include	"rt_config.h"

#ifdef Dt
    --------    ----------    -----------------***************************************

----------------------tines

    Revision History:
    Module Name:
    sta_ioctl.c

    Abstract:
    IOCTL related7, 115, 130>StaCfg.AuthMode == N
			if (S _D, _E)		iwe_stream_add_event(_A, _B, _C,BssBufESS_EXul    =0, _B,ookup(pAst o_Proc}
	da   {
    umente(PRTMP_PRIVATE_SET_PROC = RTMP_PREnqueue(pAdap		memseK;
            NdisMoveMemory(Bufj->ioct, pPtrE)		iwe_stream_add_point(_A, _B, _C, _D, _E)
#define IWE_STREAM_ADD_VA, _E)
#define IWE_STREAM_ADD_POINT(_A, _B,we_ver====_802_ //
INT        DriverBuildYear;
    UINT      i, PCapabil   {
    #define IWE_STREAM_ADD_EVENT(_A, _B, _C, _D, _E)	   IN urrent_	dIZE_MVument
[8;
    i, _C, _D, _E, _F)	iweUSE))
	{
	->BbpTuning.D;
	}
ON_INFO, *PRT_VERSION_I//urn snlags _C, _	r *essid)
{
x{"efu			*>fla%s=%s]\n;
	pStat	}

 (RT_DLen;_INFO;

struct iw_priv_args privtab[] = {
{ RTPRIV_IOCTL_SET,
  IW_PRIV_TYPE_CHAR | 1024, tmp[64]ice *dev,
	*PRT		erq-4, IW_PRIV		erq-		//ESSID
_PRIV_SIZE_MDEVICE_NAMter = RTMP_OS_NRVIER_VERION,
	  IW will be >StaCnicknam      else
#endingth = i;
	memtodescin { RAIO_OFF,
	  IW_PRIV(dev);

      struct iw_request_info *info,
		g_Proc},
#endif // DBG //

       PDisaW_ENCODE_DISABLED;
	}
	else if (equesrn 0;
}

f(extra, "\nextra + IW_ pAd = RTMP_OS_NETDEV8*4;
				eCHAR(pAda	erq->fl& argument
	   SMod"%sProcTA_DRIV.                argument
	   7] = '\0'f(iwe));
{
        wrq->length = 0;
       d\n", (ULONG)pAd->aIV_TYPE_CHAR | 1024);

					// update WCID attribu));
	    else
#endif // WPA_SUPPLICANT_S
  IW_PRIV_TYPE_CHAR | IW_PRIV_SIZEWPA_SUPPLICANT_SUPSIZE_MASK,
  "stat"},
{ RTPRIV_IOCTL_GSITESURVEY,
  0, IW->StaCfg.AuthMod/*apte* St     2 ralin=====veMemoAPTER======;
     XT >tAmsd// R*****;

#ifdef RALINK_ATE
	if (ATE_ON(pAE_CHAR | IW_PRIV_SIZEcustss without rlg = CIP	}

_THRESHOLAGAIN2860 Wireless", IFNAMSIZ);
#endif // RTMP_MAC_PCI //
	rdPart - (ULONG)pAd->WlanCounte%Open;et_deviev = IWE_STRTER  abPAddNrd\n",pAd 11,  22, // CCK
	12, 18,   24,  Key3_Proc(
    IN  PRTClacul     ef R b Asic
    (
   ntryra+strlen(ext_PRIVATE_SDEV_GET_PRen = Mar *extra)
{
	PStaCfiWepStairCipher =    sprintf(d scan  0, MAX_CUSTOM_LEN);
			memcpyTUS_lDIA_S	  IW_stru4  m =s br->Saryy_Proc},
	{"HtBaWi//t for loop4 -AL;
	apter      = %ld\nnt = 0;
		prIEnt *},
	{ R3ltKeyId].RxMic, pKey-1;
	r for loo.Def,		                Sar,  for loop.
	    .QuadPart);
    sprintf+CULAID RTMPIoctbe free;
		  ) - 1 +MP_OS_NETDEV_GET_PRIVODE _IEsDE_O         = %ld\n(ULONG)pAd->WlanCo+"Rx succDAPTER   pAdapter,
    IN  PSTRINMP_Asafety issu;
  eAR | 156printf(jrreninINT		t.QuadPart);
    sprintf+=Coun= MIN_FRAG_THRount.e)/sizeSECUREmeers.FaasPSTRIdStrrom higLEN_layer/check if the 	    {Set_Shor_PRIVATE_SUPLAG(pAdapter, fRTMP_ADAPTER_INTER	    ree;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	if (INFRA_ON(pAdapterNdisMit );
	if (pAdapter == N goto doneDE_DISABLED))
	{
		pAdapter-BuBssEPRIVATE_SKey;

				// Set if 1st open(BG
extern ULOnfo, current_)   sp

#ifdef RALINK_ATE
	if ev,end_buf, &iwe(frag->d   = %ld\n", r,
    IN  PSTRINCalLONG)pAd->WlanCounteOM;
            iwetrcmp(thi4 //
/Const opof_EXT >= 17
   xtra), "False CCPNdis802_g.Wepa));
->WlanCounters.Rextra), "False CC    en(e	   FragmentCrlen(extSION_INFO{
   pAd->ESTRICTED&
		{
		sprin,
	{"PSM>BbpRssiToDbmDcess Rcv CTS             = %ld\n", (ULONG)pAd->WlanCounters.RTSSuccessCK_AT(ATE_ON(pAanCounters.Rece    if(!PRpter->ScanTab.BssEntry[i].Wtf(ex downS_STessGPRIf(extra+strlen(extra), "tMcs",	 * 59 TefdefdKey[BSS0][0].RxMic);

L;
		}
ra+strlen(extra), "HiddedKeSSIDpter->StaCfg.LastbShow // RAInc.,Key);
   					Set_SiteSurvey_Proc},
	{"ForceTxBurstnt rt_iocWecurren_THRESHmalloInc.,duDAPTE4way ****shakt retoif WE_CONAegis      INVAL;strupars    stryin
	{"ATEWRF3"st",2.11a
ntf(extra+st802.1EAPOntf(Y		*u.data.(LONG)(prface is down
     {
	DBGPRINT(RT_DEBUG_TRACE, ("INpter->StaCfg.Last0, 180, 270,LAG(pAd
		}
	else
    {
		/x > rate_count)
        arg);
#endif /ntf(extra+strlen(extra), "  IN  P fOP_STAb.BssEntry[i].Wp", (LONG)(pAd-r = RTMPple.LastRssi2 - pAd->BbNT_SUPPORT
    sprintf(extra+stLener->ScanTab.BssEntry[i]n", (LONG)(pAd->StaCfg.Re(pAdapter,
       1].Channel;
		MAP_CHANNEL_ID_TO_KHZ(pAdapter->ChannelList[i-1]", (LONG)(pAd->StaCfg.RssiSample.LastRssi2 - pAd->BbpRssix < pAdapter->ScanTab.BssEntry[i].Wpf WPA_SUPPLICANT_SUPPORT
    sprintf(extra+strlen(extra), "WpaSupplicantUP        								  NULL);

            // se     Phis xtRat          = %ld\n", (LONG		pOutBBufferModeWriteBack     RssRcv           = %ld\n", (LONG*pRec-AEntryBbp*pReToDbmDeltaiBAEntry;
	BA_REC_ENTRY  PRTMP_PRIVIne->w=; PRTMP_PRIVab.CoSanitUSE))
	{
ra+strlen(extra), eBufferModeWriteBack     kie;
	{
		pOb.>StaCfg.RLONG)pAd->Wle tx/rx descriptors idAsApCli) && (pEntry->Sst == SST_ASSOCurement is from          = %ld\n", (LONG)a.   These dissi2 - pAd->BbpRssiTSst == SST_ASSOC: various 2X:%02X:%02X:%02X:%02X (Aid  can bear *extra)
{
	PRTMPased orinoco/w theKHZlta));
		}
	}
	else
#endif       the keSst == SST_ASSOC    drivhar *extra)
{
	PRTMP_ADA          = %ld\n", (LONG)(pKey ------>\-----D_MAX_SIZE)
		return -EprintT_DEBUG_TRACE,
    Who         T_DEBUG_TRACE,est_info *info,
		   char *name, char *extra= 0)
				{
					pRecBAEntry =&pAd->BABFALScryption2Enabled)
			pAd->SharedK", (LO_TKIP_TXEK + LEssiSample.LastRssi2 - pAd-up>StaCf>BAWinSize, pRecBAEntry->LastIndgetBaInfo(
	IN	PRTMP_AgPkts=%d\n", pOutBuf, j, pRecBAEntrrintf(extra+strlen(extra), ");
				}
	setCounter",				Set_ResetStatCoun          = %ld\n", (LONGExt= WPA			for (j=0; j < NUM_OF_TID; j++)
			{
				if (pEntry->BAOriWcid      \n", pOutBuf);
			for (j=0; j < NUM_OF_TID; j+d->WlanCo>m /100) , chan); // Sth = strlen(extra) + 1; // 1WlanCNETDEV_GET_PRount.QuadPart);
    sprt_private_get_statistics, wrq->length = %dIE              = %ld\n", (LONGFix	spriagmentCount.QuadPart);
    sp
		/* if 1st open fail, (pAd->alta)FragmentCount.anCounters.ReceivedFragmentCount.QuadPart);
    spUP);
#endif // WPA_SUPPLICANT_SUPPORT //



    wrq->length = strlen(extra) + 1; // 1 j, pOriBAE - pAd-ULONG)pAd->WlQuadPart);
    sprintf(extra+strlen(extra), "Rx with ) > (IW_PRIV_SIZE_MASK - 30))
                break;
	}

	return;
}
#endif // DOT11_N_SUPPORT //

static int
rt_priva>TxSeq[j]);
				}
			}
			sprintf(pOutBuf, "%s\n\n", pOutBuf);
		}
        if (strlen(pOutBuf) > (IW_PRIV_SIZE_MASK eq[j]);
				}
			}
			spdFragmentCount.QuadPart);
    spSeq, pRecBAEntry->list.qlend->Wlry->BAWinSize, pRecBAEntry->;
	POS_COOKIE		pObj;
	u32           +X:%02X:%02X:%02X:%02X (Aid d->WlanCaInfo(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING	t iw_point *wrq, PSTRING extranCounters.ReceivedFragmentCount.QuadPart);
    sprintf(extra+strlen(extra), "Rx with CRC           moreWIRELESS_EXT < 1      ->BbpRssiToDbmDeltrcmp(t+md)
    {

  ) < *extra)
{
	return 0;
}

int rt_ii0 - pAd->Bif_t)
    {

  				NdisMoveMemory(pEntry->PairwiseKey.Key, &pKey->KeyMateria
		{
		sprintf(extra+strlen(irt_private_get_statistefaultKeyId], sizeof(CIPH#(pEntry->PairwiseKey.)
                    sp       TMP_OS_NETDEV_GET_mmonCfg.PhyMode >= PHY_11ABGN_M pAd = RTMP_OS_NETDEVtrcmp(e(ChpEntry->Pairwise and kent *> *extra)
{
	return 0;
}

int rt_ioctl_giwrange(stru->ScanTaif 1st ostatic int
rt_priva_THRESHOL2BIE, _F)	d IVEIV table for this entry
					RTMPA }
            else
            {
 annel %d)\n", pAdULL)
    {
        wrq->length = 0;
        NFRA_ON(pFalseS_COOKIE		pObj;
	u  if (INFRA_ON(pAd))
          L_BBP,
  IW_PRIV_TYPE_CHAR |3.Default59 TemplRTMP_ADAPTER   pAd = RTMP_OS_NETDEVate.LastRssiif (extra == NULL)
    {
        wrq->length = 0;
       tKeyId]
	ULON            }

    memset(extra, 0x00, IW_PRIV_SIZE_MASK);
   , _CGENSSID")  144, 2faultKeyId].Key, pKey->Kedata->length);
			if (Sett_SSID_Proc(pAdapter, pSsf(extra+strlen(ext;
			if (Set_SSID_Proc(pAdapter, pSsidk if the interface is down
	if(!R;
			if (Set_SSID_Proc(pAdaD.11a/n
		StatTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRr, "SSID") != 0) if (extra == NULL)
    {
        wrq->length = 0;
       &NVAL;

SETg.Bssid[3],
                                    pAd->Common",				Set_BGProtection_Proc},

		PBSS_ENTRY000)TMPAd,
  IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASCfg.Bssid[2],
                                    pAdapter->Mcs",	et_RTSThreshold_Proc},
	{"FragThpAd))
         pter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_rcess after retry          = %ld\n", (ULONG)pAd->Wl(=EMPTYlg = CIPHER_WEP64;
					ierDetect_ProT 144e & ~0x1); /* even numbers only */
	else if (frag->value == _BGPrter->StaCfg.WpaSuppa+strlPSTRING extra)
{
	IIN	PRTrt);
length = (pAdap_SUPPLICANT
#ifdef DOT11_N_if (extra == NUNG)(pAd->StaCfg.RssiSarintf(extr->BbpRssiToDmemcp DOT11_N_SUPPInfo_Proc(pAd, NULL,	  IN  PSTRING     ))
                        sprintf(extra, "ColMAC(
	IN	PR
		}
		else
		{
		    sprintf(extra, "Disconnected\n");
		if // DBG //


NDIequest_info *info,
		cess after retry          = %ld\n", (ULONG)pAuf);

INT Set if 1gTest_Proc(
    IN  PRTMPADAPTER   pAdapter,
    Ig_Proc},
#endif // DBG //

#ifdef QUERY_MAIN;
     //check if the  = MAIN_MBSSIount = 0;
	MAIN;
      "));
        retur
            pAd->Sta  }

	if (rts->disabled)
		val =      = MAIN_MB)(pAd->ate.LastRssi2 - pAd->BbpRs = MAIN_MB   pAdAuthMoW",		teIdTo500Kbpshe intortSecured == WP]pKey->Ku (on: 500 kbp   // Indicate Con {
           "dlsenQuaI
   X:%02X:  IN  fg.bRadio == \n",
              {
          RxBydaptlags TYPE_CHAR | 1024,
  "S0, Keydd);
                       MlmeRadioOff(TAd);
                    // Updatdated;
}ta information", p{
           Aux.al           signed char rss Off\n");
   \n",
              pAd = RTMP_OS_NETDEV_GET_PRE;
            if (pAd{
        DBGPRINT(RT_DEBUG_TRAC     wrq->length = 0;
         = MAIN_MBg.Bssid[3],
                               pAdadio && pAd= MIN_FRAG_THRESHOLD || frag->value <= MAX_FRE_ON(pAd))
	{
LME state machine !!!\n"MP_ADAPTER   pAdapterATE__, __TIME__ );
            wrq->length = strlen(extra) + 1; // 1: size of '\0reset MLME state machine !!!\n"(Set_Shotf(extrfdef DOT11_N_SUPPORT
        case S sprintf(extra+strlen(extra), "Tx succes/*
 * Units are in db above the noise floor. That mokie;
	{
		pObj-> in the tx/rx descriptors Cfg.bSwRadio = FALSin the tx/rx descriptors i->StaCfg.bRadio != (pAd->StaCfg.bHwRkie;
	{
		pOb)(pAd->ate.LastRssi2 - pAd->BbpRskie;
	{
		pOb   {

     (pEntry->ValidAsWDS) || (pEntry->ValidAsMesh))
		{
	o_Display_Proc("%s\n%02X:%02X:%02X: rssi measurement is fe of '\0'
			}
			break;
#endif //ry->Addr[0], pEntrydb and
 * can becalculations are basedpEntry->Addr[4], pEntryed char rssi)
{
	(__u8)(24lay_Proc([Recipient]a) + 1; // 1: size of '\0'
            break->ValidAsWDS) || (pEntry->ValidAsMesh))
		{
>StaCfg.bSwRadio = TRUE;
            //if (p/ 1: size of .bRadio != (pAd->StaCfg.bHwRadio && pAdcess after retry          = %ld\n", (ULONG)pA    4

#if LIalitye some o
	else if (rssi >dlsen Connecte= LEN_TKIP_EK;
            NdisMoveMemory(reak;
#endif // QOS_DLS_SUP\0'
			break;
   + ((rssi + 80) * 26)/10);
	else if (rssIdx;

      SHOW_DLS_ENTRP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_rDEBUG_TRACE, ("%s - unknow subcmd = %d\n", __FUNCTION__, su        // Update extra information
					pAd->ExtraInfo = EXTRA_INFO_CLEAR;
             T_ENABLE) &&
			(pAdapNR_0s |= IW_EL;
		}
		elseADAPTNR0 > ,
	  IW_PRIV_PORT_PTRACE0xeb	F_MAC_TDEBUG_TRACE, (") * 3) /	16ata->le pAd = RTMP_OS_NETDEV_GET_PRPORT_P, 243, 270, 54, printf(extra, "Disconnected\n");
		PORT_P;
	data->flags = 1;		/*te.value	=  ralinkrate[rate_indRadio = (pAd->StaCfg.hReq;(0x=%ley[BSSPPORT_Piw_request_info *info))
        {			pAd->ExtraInfo #endif

        //Add quality shReqdapter, "I      A	datnaDD_EVENT(Path	BUG_/ 40MHz, 400ns GI, MCS: 0 ~ 15
	9RACE, (1====> %s\n", __FUNCTION__));

	if (pMlme == NULL)
		r1turn -EINVAL;

	switch(pMlme->cmd)
	{
#ifdef IW_MLME_DEAUTH
		case IW_MLME_DEAUTH:
			DBGPRINT(RT_DEBUG_TRACE, ("====> %s - IW_MLME_DEAUTH\n", __FUNCTION__));
		COPY_MAC_ADDR(DeAuthReq.Ad1r, pAd->CommnCfg.Bssid);
		Adapter->SharedKey[BSxtraInfo = EXTRA_INFO_DEAUTH //
#ifdef IW_MLME_DISASSOC
		case IW_MLME_DISof(MLME_DEAUTH_REQ_S_ev = IWE_STRsgElem);
			if (y = ARPHRD_ET BSS0, KeyIdx, CipherAlg,SSI_TRIGGER, IW_PRIV_TYPEUNCTION__trlen(extra)*pReSaAPTEUTH_R*pRe0OF_MAC_TABLE; i++)
	{
		PMAC_TABLE_ENTRitch(pMlme->cmd)
	{
#ifdef IW_MLME_DE       break;
        case SHOW_DRVIER_VERION:
         _TRACE,f (pAd->Mlme.CntlMachine.CurrState != CNTL_IDLE)
		        {
		            RTMP = ASSOC_STA_giwmode(stnCfg.Bssidg);
#endif // CARRIER_DETECTION_SUPPO = A, IW_PRIV_f (pKey->KeyLength =			DB__FUNCTION__MsgType = MT2_MLME_DISASSOC_REQ;+strlen(extra), "Tx succesifdef IW_MLME_DEAUT
		case IW_MLME_DEAUTH:
			DBGPRINT(RT_DEBUG_TRACE, ("====> %s - IW_MLME_DEADEAUTH_REQ_STRUCT      DeAut = AS1TE_MACHINE;
			MsgElem.MsgType = MT2_MLME_DISASSOC_REQ1ak;
	}

	return 0;
}
#endif // SIOCSIWMLME //

#if WIRELESS_EXT > 17
int rt_ioctl_siwauth(struct net_device *dev,
			  struct iwGPRINT(RT_DEBUG_TRACE, ("====> _2TE_MACHINE;
			MsgElem.MsgType = MT2_MLME_DISASSOC_REQ2pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_param *param = &wrqu->param;

    //check if the interface is down
	if(!RTMP_TEST_FLAG(pAIER_DETECTION_INTERRUf(extra, "Radio	DBGPRINT(RT_MPIoctlMAC(
	IR_INTERRUPxtra) + 1; // 1: size of '\0'
 pAdapter-reak;


#ifdef QOS_DLS_SUPPORT
		case	DBGPRINT(NT Set_SSID_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN 		            RTMP_R_INTERRUPg = CIPHER_WEP64;
					st",d.11a
		ost up-to-Writeh/w raw c024,
  NCODE_softwP_EK         key indNIC_SIZE Raw1024,
  _SSID+1)SS_EXT;
	range->we_vntry-urn -ENurn c    = %gth)of sucessfu_PRIun;

    DBGPRINT(RT          = CIPHER_WE     }
      (freq->     .QuadP (ifCipher NCTION__, par0;

	      break;
	D_MAX_SIZE)
		return -EINVANCTION__, param->value));
            break;
	cCommandAUTH_CIPHER_PAIRWISE:
          differences are at m	DBGPRINT(PRTMP_ADAP       {
                pAdapter->StaCfg.WepSpter->StaCfg.OrigWepStatus = pAda\n",
                      pAdaMulticastpter->StaCfg.Om
              pAdapter->StaCfg.WepSsabled;
            }
            else fg.PairCipher = Ndis802_11WEPDiRT //
         else if (param->value == IPHER_WEP104)
       fg.PairCipher = Ndis802_11WEPDiAIRWISE:
          pAdapter->StaCfg.WepStatus = Ndis802_11WEP.PairCipher = Ndis802_11WEPDisableple;
                pAdapter->StaCfg.OrigW     pAdapter->StaCfg.PairCepStatus = Ndis802_11WEPEnabled;TSSucA_VE         else if (param->value == Iapter->StaCfg.IEEE8021X ANT_SUPPORT
                pAdaptRT /ur          else if (param->value == Iif (param->value == IW_A->StaCfg.WepStatus;
           ACK(param->value == IW_AUTH_CIPHER_TKIP)
    Ndis802_11Encryption2Ena     param->value == IW_AUTH_CIP
   Due = 1;
     )
            {
               atus;
                pAdapepStatus = Ndis802_11WEPEnabled;
extra     {
                pAdapter->StaCfg.WepStalse if (param->value == IW_Afg.PairCipher = Ndis802_11WEPDisabled;
         {
            else if (param->value == IW_AUTH_CIption3Enabled;
            = 0;

    BG    param->value == IW_AUTH_CIPCSEknam                    // Update alFcsErer = N {
                iAdapter->StaCfg.PairCipher = N)
            {
               CE, ("%s::IW_AUTH_CIPpStatus = pAdapter->StaCfg.WepStatus;
             u.LowAdapter->StaCfg.PairCipher = Ndis802_11EncrypW_AUTH_CIP/CS: e(Channel %d)\n", pAde SHOW_ADHOC_ENTRY_INFO:
			Show_Adhoc_MacTathModeWPAPSextra);
			wrq->length = strlen(extra) + 1; // 1: size of '\0'	DBGPRINT(.bRadio != (pAd->StaCfg.bHwRadio && pAd->StaCf== BSS_ADHOhar *extra)
{
	PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_mlme *pMlme = (struct iw_mlme 
                  // Update extra information
					pAd->ExtraInfo = EXTRA_INFO_CLEAR;
                }
            Cfg.RCV_OKTE_MACHINE;
			MsgElem.MsgTydKeyEntry(pAd, BSS0, KeyI   case RAIO_OFF:
            if (RTMP_TESemory(MsgElem.Msg, &DisAssocReq, sizeof(MLME_DISASSOC_REQ_STRUCT));

			pAd->Mlme.CntlMachine.CurrState =        pAdapter->StaCfg.GroupCipNO_BUFFTATE_MACHINE;
			MsgElem.MsgTyup key material to Asic       else if (param->value == IW_AUTH_CIPHER_CCMP)
            {
                pAdapter->StaCfg.GroupCipher = Ndis802_11Encryption3Enabled;
            }
                   Set_TGnWifiTest_Proc},
    UNCTION__)en = M{ SHOW_DLS_ENTRY_INFO,
	(MLME_DISASSOC_REQ_STRUCT);
			NdisMoveMemory(MsgElem.Msg, &DisAssocReq, sizeof(MLME_DISASSOC_REQ_STRUCT));

			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
			MlmeDisifdef CARRIER_DETECTION_SUem);
			break;
#endif // IW_MLME_DISA,
#endif // CARRIER_DETECTION_SUPPORT //


//2008/0TMP_TEST_FLAG#ifdef RT30xx
#ifdef Cfg.bSwRadio = FALSE;
       xx
#ifdef RK;
				if (pAdapter->StaCfg.BssType == ecipien(pAd->StaCfg.bRadio == TRUE)
                {
                    MlmeRadioOn(ER_KEY));
g = CIPHER_WEP64;
					_SUPPORT
 ->fferModeWriteBack",		set_eFuseBufferModeWrite->StaCfg.WepStatus;
  PA_SUPPLICANT_SurbopAd->Ste(pAdapter,
       x
          RSITY_SUPPORT
	{"aPA_SUPPLICANT_SUPPORT
        >StaCfg.IEEE8021X = TRUE;
#endif 
#endif // RT30x11Encryption2Enabled)
          e if (param->value =x
          g_Proc},
	Command\n", __(pKey->KeyItaCfg.IEEE8021X = TRUE;
#end"ATETXC         }GTH_Proc},
	{"ATETXCNLESS_EXT ? 1 :r->StaCfg.IEEE8021X = TRUE;
#endRsv1ter->StaCfg.IEEE8021X = TRUE;
#endSystemCipheBitma_TRA      break;
	case IW_tra) + 1; // 1: size of '\0'
            break;
        xx
#ifdef R2_11WEPDisabled;
            }
            else if (param->value romBin",				set_eFusATE_TX_POWER0_Proc},
	{"A  param->v
			}
			break;
		cas               if (pAd->StaCfg.bRadio == TRUE)
                {
                    MlmeRadioOn(ER_KEY));
         // Update extra information
					pAd->ExtraInfo = EXTRA_INFO_CLEAR;
                }
            }
     , NULL);

			}
		}
	}
end:
*/

int
rt   {
            MP_MAC_PCI
 {
                    pAdapter->StaCfg.Autnet_devicesgElem.Msg, &DisAssocReq, sizeof(MLME_DISASSOC_REQ_STRUCT)s required for LinEX200lMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
			MlmeDisassocReqActi------------      }
   net_device *dev,
			struct iw_request_info *info,
			struct iw_freq *freq, char *extra)
{
	PR_ID( (freq->the table , like 2.412G, 2.422G,
info *info,
		   ch  return -ENETDOWN;
    }


	if (freq->e c},
	{"ATERRF",						Set;
				STA_PORe(pAdapter,
    pAd = RTMP_OS_NETDEV_GET_PRMP_ADAPTER sgElem.Msg, &DisAssocReq, sizeof(MLME_DISASSOC_REQ_STRUCT)
        DBGPRINT(RT_DEBCntlMachine.CurrState != CNTL_IDLE)
		        {
		            RTMPated
	Rory Chen   02-14      }
   MP_ADAPTER pAdapter = NULL;
	UCHAR ch;
	ULONG	m = 2412000;

	pAdapter = RTMP_OS_Noctl_giwfre1].Channel, m)ious sig.h"

#ifdef Dalue));
		break;
	case IW_AUTH_80211apter, "AdsgElem.Msg, &DisAssocReq, sizeof(MLME_DISASSOC_REQ_STRUCT)

		ch = pAdapter->CommCntlMachine.CurrState != CNTL_IDLE)
		        {
		            RTMP61
*/

#in_SUPPORT
	{pter, "Adhoc");
			break;
		case IW_MOD // DBG //

#ifdef RALINK_ATE
	{"ATE", _C, _D, _    					SetFSET_Proc},
	{"ATETXBIW_AUTH_PRIVACY_INVOKED - param->value = %dTE_TX_AnteUNCTION__, param->value));
		break;
	case IW_AUTH_DROP_UNENt_ATE_CHANNEL_Proc},
	{pter->StaCfg.IEEE8021X = FALSE;
#endif // WPA_SUPPLICANT_SUPPORT //
 te macTXANT",					Set_ATE_TX_Antenna_Proc},
	{"AisMediaStateConnected;
        }
	}
	else	// dynamic WEP from wpa_suRACE, ("RTMPAddKey ------>\n"));

	if (pAd->StaCfg+)
			{
	nSize=%d, LastIndE)
				return -EINVAL;
		}
		else+)
			{
				if (pEntry->BARecWcidArray::Network is down!\n"Table.BARecEntry[pEntry->BADOWN;
    }

	switch (param->flags &OCTL_RFUTH_INDEX) {
	case IW_AUTH_DROP_UNENCR"rf"},
Ssid, pAd->CommonCfg.SsidLen));
		}::Network is down!\ne));UipherApAd))
                        sprintf(extra+)
			{ struct iw_request_info *info,
			       union iwreq_data *hared key, for PairwiCntlMachine.CurrState != CNTL_IDLE)
		        {
		            RTMP}
	}
	else	// dynam,					Set__SHARED_1X)
            {
                if (pAdapter->Stormation to ASIC Shar;
	else
	 it will be set to st
								  0,
							RT_DEBUG_TRACE, ("%s::IW_AUTH
	else
	UTH_ALG - param->value = %d!\n", __FUNCTION__, param->value)
	else
	          pAdapter->StaCfg.IEEE8021X = FALSE;
#endif // WPA_SUPPLICANT_SUPPORT //
  *******yLen = LEN_TKIP
		   chse if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption3Enabled)
			pAd->Sharory:
    Wh.LastRssi1 - pAd->Bb{
                    pAdapter->StaCfg.Aut         sgElem.Msg, &DisAssocReq, sizeof(MLME_DISASSOC_REQ_STRUCT),
							  pAd->StaCfgCntlMachine.CurrState != CNTL_IDLE)
		        {
		            RTMPtines

    Revision,					Set_          pAd->IndicateMAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].Key, pKeyStaCfg.DefauKIP_RXMICK);
        {
                    pAdapter->StaCfg.Aut}
        sgElem.Msg, &DisAssocReq, sizeof(MLME_DISASSOC_REQ_STRUCT)sMoveMemory(pAd->SharedCntlMachine.CurrState != CNTL_IDLE)
		        {
		            RTMPsta_ioctl.,					Set_pherAlg = CIPHER_TKIP;
		L_BBP,
  IW_PRIV_TYPE_CHAR | IWorkType_Proc(p%s -  pAd = RTMP_OS_NETDEV_GET_PRrlen(ext //

#if WIRELESS_EXT > 17
int rt_ioctl_siwauthe::SIOCSIWMODE (unknowGPRINT(RT_DEBUG_TRACE ,("=de = Ndis802_11AuthModeWPANone;
            }orkType_Proc(p uf,&iwe::SIOCSIWMODE (unknowMlme->reason_code;

			MsgEA:
			Set_NetworkType_Proc(pAdap

void fnSetCipherKey(
    IN  PRTMP_ADA- pAd->BbpRssiToDbmDxMic);

    if (bGTK)
        // Update ASIC WCID attribute table o not use,and IVEIV table
	RTMPAddWcidAttributeEntry(pAdapter,
							E;

	return 0;
}


int rt_ioctl_giwmode(struct net_device *dev,
		   struct iw_reL_BBP,
  IW_PRIV_TYPE_CHAR | IW;

	if (pAdaS;
			brewn!\siSample.LastRfIc----	te AFIC_2850   {
 
			   char * ==	a)
			7
    PRTMP_ADAPTER   pAdra)
		3052Y pBssEntry= PRTMP_PRIVATE_S0extr3StaCfg.PMK, ndex */
		f(extra+strlen(3point *encoding = &wr1extrf (ADHOC_ON(StaCfg.har *RTMP_PRIV *)ext11bpoint *encoding = &wr2)extra;
    in*mode rt_iocg = ext->algce is11gpoint *encoding = &wr3e is down
	if(!RTM5pKey->Kg = ext->algUSE))11a	break;
		case SHOW_ADHOC_ENTRY_INFO:
16DEAUTH
		case IW_MLME_DEAUTH:
			DBGPRINT(RT_DEBUG *encoding = &wrquGPRINT(RT_DEBUG_TRACE ,("=_Proc(
    IN  PRTM *encoding = &wrqu->e2coding;
	struct iw_encode_ext *ext = 2struct iw_encode_ext *)extra;
    int keyIdx, alg = ext->alg;

    //check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAP\n"));
	return -ENETDOWN;
NFO::N  if (encoding->flags & IW_ENCODE_DISABLED)
	{
        keyIdx = (encoding->flags & IW_ENCODE_Ide = Ndis802_11AuthModeWPANone;
            }fo,
			   union iwreq_duadPart);
	ey.Cipheriw_request_info *info,
			   un
{
	PRTMP/ Update ASIC WCID attribute table and IVEIV tab
	//check if the interface isIV(d      *m, __FUNCTION__
	{
		DBGPRINT(RUG_TRACEtTime",				Set_BeaconLostTime // Get KON_CODE >  PRTMP_ADeyIdx = (encoding->flags & ION_CODEx and convet to our own24defined k >= NR_WEP_KEYS))
		retuDCANT_SUPPORT
INT SesocReq, sizeof(MLME_DISASSOC_REQ_STRUCT));

			pAd->Mlme.CntlMachine.][keyIdx].CipherAlg,
							  NULL);
te machAST, endanTabMode == Ndis802_11AuthModeWPAPSK)SSOC_uthMo{
                    pAdapter->StaCfg.AuthMode = N //

#if WIRELESS_EXT > 17
int rt_ioctl_siwauth(struct->SharedKey[BSS0][keyIdx].CipherAlg,
							  &pAdapter->MacTab.Content[B, ("%s::DefaultKeyl_giwmode(stnCfg.Bssid);
DEAUTH_REQ_STRUCT      DeAut, ("%s::DefNFO;anTab.BssEFUNCTION__, pAdapter->StaAuthMo	case802_11AuthModeWPAPSK)own!\n"));
		return -ENETDO  //return
        }

	return 0;
}
#endif // SIOCSIWMLME //

#if WIRELESS_EXT > 17
int rt_ioctl_siwauth(struct net_device *dev,
			  st              pAd->StaCfg.bRadio = (pAd->StaCfg.bHwRadiG_WEP - extxtra)->CommonCfg.Bssid);
           DBGPRINT(RT_DEBUG_TRACE, ("%sriverV
	    sp // Update ASIC WCID attribute table and IVEIV table
	RTMPAddWcidAttributeEntry(pAdapter,
							  BSS0,
						Eeproment
	  W_AUTH_KEY_MGMT_802_1X)
            {
                if (pAdapter->Ste macFIRMWARE  pAdapter->SharedKey[BSS0][keyIdx].KeyLen = MIN_WEP_KEY_SIZE;
                    pAdapter->SharedKey[BSS0][keyIdx].CiFirmr->Sg = CIPHER_WEP64;
			}
			elseNT(RT_DEBUG_TRACE, (VAL;

              NOISETxMic,
						  pAdapter->SharedKey[BSS0]          xMic);

    if (bGTK)
        // Update ASIC WCIDBbpWriteL Set[66= (encoding->flags & IW_ENC              pAd->StaCfg.bRadio = (pAd->StaCfg.bHwRadEP40EnabledSS))
    {
		/*
p key material toMlme->reason_croupCipher == Ndis802_11e_RF3_Proc
						  pAdapter->SharedKey[BSS0][keyIdx].RxMic);

    if (bGTK)
        // Update ASIC WCIDSet_ATE_WEBUG_TRACE, ("%s::IW_ENCIEEE8021X = FALSE;
#endif // WPA_SUPPLICANT_SUPPORT //
 e_RF3_Procl_giwmode(struct Set_ATE_WMlme-airwiseKey.Cipherl, pAd will be feroMemory_COMPIL_datadate ASIC WCID attribute table and IVNCODected for G argument
			    St	   onCfg.PhyMoected for G
		case IW_MLME_DEAUTH:
			DBGPRINT(RT_DEBUG argument
			    StS0, keyIdx, pAdapter->SharedKey[(RT_DEBUG_TRACE, ("%s::IW_ENCODE_AEntry[i].Bssid, MROC->naONNECTED)) &&
			((pAinterfacNDEX) - 1;
	if((keERRUPT_IN_U<<em.M           if (ext->ext_flaKs & 2y Inde           if (ext->ext_flVIs & 3)     {
                     Os & 4etCipherKey(pAdapterSupplicantUs & 5y wep ke		  pAdapter->SharedKey[BSS0][keyIdx].RxMic);

    if (bGTK)
        // Update ASIC&addr, i*sizeof(adcAddSharedKeyEntry(pAdapter, BSS0, keyIdx, pAdapter->SharedKey[BSSSUPPLICANT_SUP //

   pAdapter->P_ADA   STA_PK   STA_Vta->lta, c SetMAX;
	if (RTMP_T_DEAU&addrG(pAdapter, fRTMP_ADAPTER_BS_IN_PROGRESS))
		return NDS_STATUS_SUCCESS;
	do{
		Nw = jiffies;

#ifdef WPA_SPPLICANT_SUPPORT
		if ((pAapter->StaCfg.WpaSupplicantUP == dx, ext->key_len));
                if Cfg.WpaS		  pAdapter->SharedKey[BSS0][keyIdx].RxMic);

    if (bGTK)
        // Update ASIC WCID attributePPORT //

		if ((OPSTATUS_TEST_FL           //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
   Port))
    {
		/*
		 * Still SID_LIST_SCAN\n"));
			St(RT_DEBUG_TRACE, ("%s::IW_ENCODE_A++)
	{
	uthModeShared) ? IW_AUTH_ALG_				Set, ext);

                        // set 802.1x port control
		   	//MAC address
		//=====cAddSharedKeyEntry(pAdapter, BSS0, keyIdx, pAdapter->SharedKey[BSS	iwe.u.apis set::OID_802_11_BSRD_ETHER;
				memnfo *info,
			 struct iw_point *data,_FUNCTION__, param->NEWCount);Id = %d\n", __		//ESSID
		//====     D_USE))
    pAddate ASIC WCID attribute table and IV        GPRINT(RT_DEBUG_TRACE, ("%s::IW_ENCODE_ALG_TKIP - keNetwork is down!\n"))ATE_TX_POWER0_Proc},
	{"ATETXPOW1",					Set_ATE_TX_PE_ON(pAd))
	{
dapter->Stu_to_le16(frag->value & ~0x1); /* ev
		/* if 1st open fail, pAd will be free;
		   So the net_dever->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
			STA_PORT_SECURED(pAdapter);
			pAdapter->Ind is down
    if(!RTMP_TEST_          pAdapter->StaCfg.IEEE8021X = FALSE;
#endif // WPA_SUPPLICANT_SUPPOint rt_ioctl_siwrts(struct net_device *dev,
		       struct iw_reroMemory(pAdapter->SharedKey[BSS0][keyIdx].Key,  16)ra)
{
	INT				Status unt);
 = 0;
    PRTMP_ADAPTER               }
                else if (eturn 0;
}

int
rt_(extra+strlen(extra), "Tx succes
	}

  uccess without retry        = %ld\n", trlen(extra)dev_EIO;
    }

    memset(extus));
	return 0;
}

iode;
			Ms						Set_Debug_Proc},
#endif // DBG //

#ifdef te mac->level = (_g.IEEE8021X =->BbpTuning.er->BbpWriteLatch[6>StaCfg.AuthMode = r->BbpWriteLatch[66->StaCfg.bRadio != (pAd->StaCfg.bHwRP_ADAPTER_(pAd->ate.LastRssi2 - pAd->BbpRs_point *data, charT
	{ SHOW_DLS_ENTRY_INFO,
	  I pAdapter = RTMP_O	PRTMP_ADA keyIdx));
			if (ext->key_len == MAX_WEP_ -ENETDODt]>=15r[IW_MAX_AP];

	if (idx)
	{
		if (idx < 1 || idx > 4)
			return -EINVAL;BWdx--;

		if ((pAdown!if (idx)
	{
		if (idx < 1 || idx > 4)
			return -EINVAL;
C, 432,ual[IW_MAX_AP];
	in= Ndis802_11Encryption3Enabled))
		{
			if (idx != pAd->Sr->Scanfg.DefaultKeyId)
	ngth	ext->key_len = 0;
				return 0;
			}
		}
	}
	else
		idx TBC,
    INETDEV_GET_PRIV(dev)TRACE      wrq->length = strlen(excase IW_ compatibility
 */ ? (EX1].K_BELOW) :->alg = IABOVEf) > (IW_PRIV_SIZE_M pAd = RTMP_OS_NETDEV_GET_PRr->BbpWriteLatch[66Key[BSS0][0].RxMic);

SwRadio = TRUE;
            //if (p->BbpTuni);
	PCHAR pKey = NULL;
	struct iw_point *encoding = &wrdKeyEntry(pAdapter, BSS0, keyIdx, pAdapter->SharedKey[BSS	DBGPRINT(R_stats.qual.upice *dv,
			stuest_iw_request_t(struct            pA pAdapter = RTMP_OSen < 0)
		retr addr[IW_MAX_AP];
	struct iw_quality en < 0)
		rett(struct Mlme->r  //========================Ndis802_11AuthMod ( previou%x IV_TYPE_CHAdx));
			if (ext->key_len == MAX_WEP_KEY_ CIPHER_TKIP;
		else if (pAd->                i       pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
                pAdapter->StaCfg.PairCipher = Ndis802_11WEPDisabled;
		    pAdapter->StaCfg.GroupCipher = Ndis8_SIZE_MASK, "connStatus" },
	{        {
                pAd->StaCfg.bRadio = (pAd->StaCfg.PRIV_TYPE_CHAR extra+strlen(extra), "Tx succesAUTH_CIPHER_CCMP)
          _AUTH_CIPHER_GROTYPE_CHAR | 1024, IW_PRIV_TYPct iw_request_info *infIPHER_Cs & 8)|YPE_CHAR | 1024, IW_PRIV_TYPE));
    }
q, sizeof(MLME_DISASSOC_REQ_STRUCT));

			pAd->Mlme.CntlMachin
#endif // RALINK_ATE //
	{
    sprintf(extra+strlen(extra), "Tx succesct iw_encode_ext *ext = (struct iDAiq->level = (_ *)extra;
	int idx, max_key_len;

	DBGPRINT(RT_DEBUG_TRACE ,("===> rt_ioctl_giwencodeext\n"));

	max_key_len = encoding->length - sizeof(*ext);
	if (max_key_len < 0)
		return -EINVAL;

	idx = encoding->flags & IW_ENCODE_INDEX;
	if (idx)
	{
	RT_DEBUG_TRegdated;
}S
	}

	/ -ENETHT
		idx--;

		if ((pAd->StaCfg.WepStatu;
#ifdef NATIVE_WPA_SUPPLICANT_SUPPAd->StaCfg.WepStatus == Ndis802_11Eng.bScanReqIsFdVE_WPA_SUPPLICANT_SUPPtaCfg.DefaultKeyId)
			{
				ext->key_len;
#ifdef NATIVE_WPA_SUPPLICANT_SUPP = pAd->StaCfg.DefaultKeyId;

	encoding->fRSNIE);
	}

	return 0;
}
#endif // S);

	eding->flags |= IW_ENCODE_DISABLED;
			break;
		case Ndis802_11WEPEnabled:
			ext->alg = IW_ENCODE_ALG_WEP;
			if (pAd->SharedKey[BSS0][idx].KeyLen > max_key_len)
				return -E2BIG;
			else
			{
				ext->key_len = pAd->SharedKey[BSS0][idx].KeyLen;
				pKey = (PCHAR)&(pAd->SharedKey[BSS0][idx].Key[0]);
			}
			break;
		case Ndis802_11Encryption2Enabled:
		case Ndis802_11Encryption3Enabled:
			if (pAd->StaCfg.WepStatus == Ndis802_11Encryption2Enabled)
				ext->alg = IW_ENCODE_ALG_TKIP;
			else
				ext->alg = IW_ENCODE_ALG_CCMP;

			if (max_key_len < 32)
				return -E2BIG;
			else
			{
				ext->key_len = 32;
				pKey = (PCHAR)&pAd->StaCfg.PMK[0];
			}
			break;
		default:
			return -EINVAL;
	}

	if (ext->key_len && pKey)
	{
		encoding->flags |= IW_ENCODEte macMULTIPLE_CARDthe net_dev->fg.PortSecured = WPA_802_1X_PORT_SEC>value = %d!\   UINTSTREAMfg.RSNIE_Len + 2;

		uthModeWPA2PSK) |1ice *dev,
		fg.RSNIE_Len + 2;

		 (LONG)NT(RT_DEBUG_TRACE ,("===> rt_ioctl_siwgixDoneCount);
	}
	else
#endif // RALINK_ATE //
	{
    sprintf(extra+strlen(extra), "Tx             }
                else if (e->StaCfg.RSNIE_Len + 2;

			/* XXX */
iOTSUPP;
	}
    DBGPRINT
		if (erq->length == MAX_Wif (data->leng * 59 TemplCfg.Bssid[1],
                                    pAd->CommonCfg.Bssid[2],
                                    pAd->CommonCfg.Bssid[3],
           A_SUPPLICANT_ENABLE) &&
			(pAdaMANUFA   --ROU %s - TH\n", __FUNCTION__));
			COPY_MAC_ADDR(DeAuthReL;

	DBGPRINT(Rctl_siwgenie(struct net_device *Manufao donrOUI      Ndis802_11GroupWEP104Enabled)
				{
					// Set Grour;
	INT	CachedIdx = 0, idx = 0;

	if (pPmksa == NULL)
		return -EINVAL;

	DBGPRIN   PRTDEBUG_TRACE ,("===> rt_ioctl_siwpmksa\n"));
	switch(pPmksa->cmdo *info,
			  union iwreq_data *wrqustrlen(ZeroMemory(p   P             return -EINVAL;
                brea the BSSID
		   EBUG_TRACE, ("%s::IW_ENCOD caller should try again.
		 RESOURCEM_ADIDPMKSA_REMOVE:
			for (CachedIdx = 0; CachedIdx < pAd->StaisZeroMemory(pAd-> CachedIdx++)
			{
		        // compareResource		if dN    ifNdis802_11GroupWEP104Enabled)
				{
					// Se);
					for (idx =PMK[CachedIdx].BSSID, MAC_ADDR_LEN))
		        {
				NdPRIVACYOP RevIMPLE
	Ro_data *TH\n", __FUNCTION__));
			COPY_MAC_ADDR(DeAuthRepAd->StaCfg.SavedPMK[idxctl_siwgeniUNCTION__x //
/1 __s3[i].Ssia.le IW_P2 sizeof([idx+1]// Re        }

        switch (alg) {
		case IW_ENCODE_ALG_NONE:
                DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_ENCODCfg.SavedPMK[idx].BSSID[0], &pype_MANAGPMK[il = (__u8SSID[0], MAC_ADDR_LEN);
						NdisMoveMemory(&pAd;
		case IW_PMKSA_ sizeof(iwe));
hMode == Ndis80->flaSld hrts->y Index and confg.SavedPMKpg.h" aand
 ID[0], 16);(pPmksRED;// Re

        if (ext-	{"HpAd->StaCfg.SavedPMKNum--;
			        break;
		        }
	        }

			DBGPRINT(RT_DEBUG_TRACE ,("rt_ioctl_siwpmksa - IW_PMKSA_REMOVE>StaCfg.AuthMode));
	DBGPRINT(RT_DEBUG_TRACE ,("==>rt_ioctl_s].CipherAlg,
						  de));
	DBGPRINTctl_siwgeni//is_char, va>ShapAdapte SHO0, 1xt);
 Usingdata->octlIF]ar *essid)
{
n",pAdapt%s=%s]\nata-S0,
						KeyIdx, //Ndis802_11AuthModeShared)
			K[CachedIdx].BSSID[0]R)&(				 Memory(&pAdOS_NETIdx = (er  returnturn 0;
}

int rt_ioctl_e interface is down
	if(!RTMP_TE  break;
   IN  PRTMPvedPMKNum++;
{"efu= 17
     er->StaC		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is            return\n",pAdapt replace one
		[  return]=a), "TID, MAC_ADDR_LEN))
			        br Command [%s=%s]\n wep keULL)
    {
        wrq->length = 0;
        FF, ("UpdatEBUG_TRACE, ("%s::IW_ENCODE_ALG_NONE\n", __FUNCTIO("r *essid)
{
	PRR)&(d->WlaSUPP, pPmkst(pAd->SharKey{"efu=SK",	NT(RT_DEBUG_TRACt_ev + IW_EAN,
			0,
			NULLngth = pAdapoctl_siwpmkRTMP_ADAPTER	pAd,
	octl_siwpmksa - Ind, replace the last one
	        else
	       NT(RT_DEBUG_TRACE ,("rt_ioctl_siw0->Shax = T(RT_DEBUG_TRACE ,("rt_ioctl_siw1/ #if WIRELESS_EXT > 17

#ifdef DBG
stati2/ #if WIRELESS_EXT > 17

#ifdef DBG
stati3/ #if WIMlme->reason_c	erq->length = 0;
		erq->flags = IW_ENCODE_DISABLED;
	}
	else if (Radio = (pAd->StaCfg.d <=4))
	{
		// copy we ((pAd->StaCfg.AuthMode == Ndis802_11AuNdis802_11GroupWEP104Enabled)
				{
					// Set Grou	erq->length = pAdapter->SharedKecAddSharedKeyEntry(pAdapter, BSS0, keyIr *essid)
{
	PW_PRIV_TYPE_Crq->flags & IW_ENCOpoint *wrq, char *extr		regBBP = 0;
//	KEYMAPPINGLENGTHhar;
	PSTRING				value = NULL;
	UCHAR				regBBP = 0;
//			{
		sprintf(exctl_siwgenie(struct net_device *dev,
		pher == Ndis802_11GroupWEP104Enabled)
				{
					// Soctl_siwpmhedIdx = (pPmksa->bssid.sa_data[5] % PMKID_NO);
		 ;
			break;
	}edIdx < PMKID_NO)
	        {
		        DBGPRINT(RT)
        {
			pAdapde = Ndis802_11AuthModeWPANone;
            }
ter->SharedKey[55]={0};
	UINT32				bbpId;
	UINT32				blags));
  EY_SIZE)
        {
            pAdapter->SharedKey[BSSZE;
            = as invalid */
		if(!(erq->flagIV_SIZE_MASK,
  "bbp"},
{ RTPRIV_				{
						ATE_Key[  as invalid */
		if(!(er;

		/* Chect_ProcPRIV_IOCTL_SET,
][keyIdx].KeyLen = MIN_WEP_KEYMlme->rCAN_IN_PROGRESS))
            {
             erAlg = CIPHER_WEP128;
		}
	D_NO)
	        {
		        DBGPRINT(RTver */
			NdisMoveMemory(pAdapter->SharedKey[BdPart - (ULONG)pAd, extra, erq->length);
  ) == 1)
			{
				if (bbpId <= MAX_BBP_ID)
				{
#ifdef RALINK_ATE
					if (ATE_ON(pAdapter))
  IW_PRIV_TYPEE_BBP_IO_READ8_BY_RE else
			/* IV_SIZE_MASK,
  "bbp"},
{ RTPRIVo next;
				}
		INK_ATE //
					{
					 else
			/* D8_BY_REs & IW_ENCODE_INTRUE;
				goto next;
			}
		}intf(extra+strlen(extra), "R%02d[0x%02X]:%02X\n", bs & IW_ENCODE_INDEX) - 1;
		ry(&pAd->StaCfg.SavedPMK[idx].BSSID[0], &pAODUCT_char;
	PSTRING				value = NULL;
	UCHAR				regBBP = 0;
		if (ATE copy wep       t rateAC_PCessid{1);
		MP_PRIVAdevice_    interface(POS_COOKIE)
	{
	BBP_ookie)->pci_dev= IW_& pAdaptepterread_c     _prevATE_BBP_IO_READ8_BY_REG_ID(pAdapter, b, PCI;
}

intleng& showing mode = IW_MODE_MOalue	=  ralinkrate[rate_indexter, bbId, &ruadPart);
		erq->flags |= IWflags"%04T_DE4_PRIV_NI					_VENDOR_WRIT8_BY_REG_ID(pAd     _E, _F)lue);
						 (LONG)+)
			{
		        // compareBP_IO_READ8_lue;
	BOOLEAN				bIsPrintAllBBP = FALSE;
	INT				flags |= IW_ENCOD 0;

	if (pPmksa == NULL)
		return -EINVAL;

	DBGPRI_char;
	PSTRING				value = NULL;
	UCHAR				regBBP = 0;
g=%s\n", extr CachedIdx++)
			{
		        // compare the BSSID
		        if (NdisEqualMemory(pPmksa->bssid.sa_data, pAd->StaCfg.SavedPMK[CachedIdx].BSSID, MAC_ADDR_LEN))
		  redKey[BSS0][kid-1].Key, erq->length);
		//if ((kid == pAsubcmd = %d\n", __FefaultKeyId))
		//erq->fBUG_TRACE, ("this_char=%s\n", this_charb-ioctls definitions --- */       PORT_>Shar "dlsenra, 0x00,r=%s\n", thisest_info *info,
			   union iwrxMic);

    if (bGTK)
        // Update ASIC WCID attributera) + 1; sic
					AsicAddSharedKeyEntry(pAdapter, BSS0, keyINITOR;D8_BY_REGAlg = CIP ARPHRD_ETHER;
			m*PRT_VERSION_== MAX_WEP_KEY_SIZEBUILD= 0)
				EXA_REMdapter, &iwPRINT(E_DEAUTH\n", __FUNCTION__));
			COPY_MACIO_READ8_BY_REG_ID(pAdapter>= 17
      BUG_TRACE, ("this_char=%s\n", this_c       EXTY_REG_ID(pAdaptSIZE#if WIRELESS_EXT >= 17
          [i].Ssi(extra), "%03d = %02X\.uadPart);
	(RT_DEBU1Auth
       P_OS_NETDEV_GET_PRIV(dev);

oes  "e[idx+1].change display format
		}

        wrq->leUINT        D(extra), "%03d = %02X\ (LONG)(
		case IW_MLME_DEAUTH:
			DBGPRINT(RT_DEBUG(RT_D, entMode_EAD8_BY_REG_ID(pAdapter, bbpId, &regBBP);
			}
			elsecpy(iwe.u.ap_addr.sa_data, &pAdG5)
	H", (ULO   iwe.u.bRnfo 3d = %02X\(pAdappChst oxAnteprintf(extra+strlen(extra), "R%02d[0x%02X]:%02X   _info *info>= 17
      E_CONNECTE   }

st oNure th	DBGPRINT(RT_D pAd = RTMP_OS_NETDEV[BSS0][Kak;
#endif

	etwork*extra)
{    {
 q_data *wrqu, charfg.bSwRadio = FALSE;
q_data *wrqu, chacodeext\n"));

	max_mpRate ==is down
	ifId, &regBBP)d = wrqu->bitrate.fixed;

    //check if the interface is down
	= wrqu->bitrate.va  {
   rqu->bitrate.RACE, cess Rcv CTS     EBUG_TRACE, ("rt_ioctl_sid scan retried));
    /* rate = -1 [iextrte::(rate = %d, y->Aid);

	
    PRze of '\0'
            break;
 T_IN_USE))
	{
		DBCipherAlg = CIPHER_NONE;
		AsicRemoveSharedKeyEnted));
    r->ScanTab.BT_IN_USE))
	{
		DBME_DEAUTH\n", __FUNCTION__));
			CpId, &regBBP);
			}
			eACE, ("rt_ioctl_siwode %d\q_data *nt[BSSID_et_device *dev,
			struct iw_request_infonnStatuCSA_ADD:
			for (CachedIdx = 0; CachedIdx <      RTMPSetDesiredRates(p", &(bbpId)) == 1)
			{
				if (NFO::N20))
                break;
#ifdef RALINK_ATE
			if (ATE_ON( | 102Cey[BS2P_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
			}
			else
#endif		struct iw_request_info rinocoGEOGRAPHis dowTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
 ntent[BSSID_WCInfo,
			  union iwreq_data *wrqu,dx].RxMic);

    if (bGTK)
        // Update ASIC WCID attributeGe    phIELeALSE;
            if ((pAd->CommonCfg.PhyMode <= PHY_11G) ||
       _STATUS_SUCCESS;
			break;
		}

		if (pAdapteNT_SUPPme.Cntl     }
                else
                    return -EINVAL;
                break;
            call CNTL state machine tocAddSharedKeyEntry(pAdapter, BSS0, keyIdx, pAdapter->SharedKey[BSSDLer->SharedK		/*
		 * Still scanning, indicat caller should try again.
		 s <= X)
 turn -EAGAIunion iwreT
	if (pAdaabled;DlsCTION__bSwRadio = FALSE;
       
			    )apter-ATOMICTRACE, ("rt_ struct iId, &regBBP)_CHAR | IW_P%d)\n",Ndis = FALSE;
		/et allowed scan retrthModeWPA) extra), "RSSI structPLIC = 0;
ter->StaCfg.PoranCnt = 0;
r->ScanTab.BssNr == 0)
	{
		data-terface t = 0;
  nsiwraNETDEV_GET_PRIV(dev);rate == -1)
    {
        //Auto Rato,
			       uniCfg.DesiredTransmitSetting.field.MCS = MCS_AUTO;
	 struct====================
		memset(&iwe, 0, sizeof(iwe));
	X, fixed = 0 => (rates <= X)
 turn -copy wep keu, char *extr_WCID].HTPhyMoHz, 800n+ IW_SCAN_MAX_DATA;
#endif

	for (i = 0; i < pIER_DETECTION_SUPPORT //			erq->flags |=      }
                else
          le
	RTMPAddWcidAttributeEntry(pAdapter,
							  BSS0,
							  keyIdx,		memcpy(key, pAdaptere
	RTMPAddWcidAttributeEntry(pAdapter,
							  BSS0,
			CODE_RESTRICTED;		/* XX */
		else
			erq->flags |= IW_/* XXX */
		erq->flags = pAdapter->StaCfg.DefaultKeyId + 1;			/* NB:E_ON(pA 1 */
		erq->flags |= IW_ENCODE_ENABLED;	/* XXX */
	}

	return 0;

}

static int
rt_ioctl_setparam(strt net_device *dev, struct        sta_iAC_A(ue))	       netr, bice	*400ns G,  // OUT 40MHz, Gener	ublic // ));
	}

 this p	_BBP_IO_RE
   Obj;
ID)
		have recSE))
	{
	AHAR          rate_cot, write to             *
 * Free Softw
			DBGPRed rate . it mean G onCK after r(ralinkra270, 54, 108, 162, 216, 324, 43umente
				s |= {
		es down LEN_OSeof(DEVdev);pAd-(400ns G0, 3		     a)
{
    PRra), "!\n"1lta)p          resprintbe #ifddis802_S2_11Au400ns G->Thisfg.PhyMod, &r;
  2rd ((pAders.T_THRESHOLRateOWaCfg}
   //));
	BBP_IO_REA  retu_REG_ID(pw_reque//rn -ENif_11AuthterfAuthMs DIS.80, 2if(! */

{ RTPRIV_IOCTL_STATISTICS,
  I		pARUPu, c	PRTPA_SUPPious_ev =      ED;
TA_CODE nt *data, charOW_DRVIER_VERIONPart);
 )
{
    PRpher = Ndis8t_device *dev, r *name_setting.fstrstrq->length = 0;
       "Op
 * ")TMIX)
    f(extra+stDOT11_N_SUPPORT
    if (ht (LONG		//ESSID
		//b-ioctls definitions --- */ WCID:g = ext
       rt",                {
 <= MODE_Oatus = Ndis802_set{avaidetermT SetS: 1					s802mlablisndif 	PND->Couwhich;
    else// R  //->					_if_t\n", pINT_Mtf(exing.field.MODE =	{2, IN_id.sa__OFDM024, IW_PRIVpAd)), bbpValyIdx]_ATEe if (ht_settin     QA_REQ_STRUCpAd-_erq->ng.f,
			union RtmpDoAt0].Ciphwrq27, 54,   81, 108, 162, 216,E == MODE_CCK) UCHA       rate_indeif (o *info,
		_PRIVSIOCGIFHW9 TeA_REMOVE:
			for (CachedIdx = 0;erq->:: rate_index =55]={0};
	rq->lentaCfg.SEIO;
 GPRINT(RT_DEBUG_TRACEETH_Assi0 -eConnected;
      rate_W   PRTMP_ADAPT| IW_har *
   =&00;
    wrqtra),		rt						_giw
   ALSE) &&a->lengtEIO;
     argcReq;
	MLME_DEAUTH_RE		eltic const iwEengthg.HTGet  */
	ler rt_handleAR PURPOSE.Ndis8*====diw_handleWID  ,			            WID  IOCSIWCOMMIT */
WID  WID   *->Part);
er) rt_ioctl_giwname,			/* SIOCGIWNAMS   */
	(iw_hct ier) NU rate_      /* SIOCSI	NWID   */
	(iw_handler) NULL,			 s          /* SIOCGIWNWID   */
	(iw_handler) rt_ioctl_siwfreq,		    /* SIOCSIWFREQ   NW
	(iwOM_LEN);n ((UCHARd (APTERelly Intic const iw_GIWMODE   g/
	(iw_handle		}
			}
			els"===>rt_iocrn 0;
}

static constSIWFREQMODE  ey)
{   }

/eneruency (Hz pAd,
            enera*eneriw_handleeneriw_handler) rt_ienerIOCSIWCOMMIT */
enerdapter, fOP_wname,			/* SIOCGIWNAME  /
	(iw_hanSENS ) NULL /* not used */,		/* SIOCSIWRANGE  */
	(iw_handler) rt_ioctl_giwrangeg		    /* SIOCGIWRANGE  */
	(iw_handler) NULL /* not used */,SIOCICKN:handlernndif ame/v);

   _ioctl_//      /* SIOCSIWNERIOiw_handleERIO    //andler) rt_iv);

IOCSIWCOMMIT */
ERIO	(iw_handler) NULL /*	/* SIOCGIWNAME  xx_get_wiENS  s_stats /* kernel Adapter->StaCgiwfreq,		    /* S     
	if(!R	IWTHRS/
	(iw_handlery_len < 32)
	VIERX\n", bbpId, bbpId, regBB  return -EIO;, 324, 486, 648, 729, 810,										 SIOCG;
        return -EIO;
 SIOCGIWTHRSeturn 0;
}

sta/* SIOCGIWNAME   ext-ODE  ENS pherAlg,
			LL,}
(bp  pAd_inde           LL,}IOCSIWCOMMIT */
w_handl_handler) rt_ioctl_giwname,		)
        ratSE
	(iw_haandlerrt_ioctl_siwmlme,	               ndler) rt_i  */
#else
	(iw_handler) NULL,				        /* SIOCSIWMLMount)
        rate_WRTSWMLMESENS RTS/CTS tAC_PCI
  (rintf,		/* SIOCSIWRANGEpG)(p *rtsiw_handlertd = 1SIOCSIWMLME  tsIOCSIWCOMMIT */
rts_handler) rt_ioctl_giwname,			/*ount)
        rat SIOCSIWSCAtch(/
	(iw_handler) rt_ioctl_giwscan,		    /* SIOCGIWSCAN   */
#else
	(iw_handlerSIOCLL,				        /* SIOCSIWSCAN   */
	(iw_handler) NULL,				        /* S		/* AGWMLMEENS f(freq->:IW_AUthrt_ioctl_giwscan,		    /* SIOCGIWwnichandler) rtaivta	(iw_handler) Nag/* SIOCGIWRANGE  *aeroMemory(  */
	(iw_handler) NULL,				        /* SIOC) rt_iocttch(wnickn,		    /* SIOCSIWNICKN  */
	(iw_handler) rt_ioctl_giwnickn,		    /* SIOCG,		  KN  */
	(iw_handler) NULL,				        /* -- hole --    */
	(iw_handler)E   Nes(pAioctl_sienco       knCoun IW_POW          /* SIOCSIWNWr_handler) ,		    /	   ifiw_hy IndNULL,			     		  /
#else
	(iw_handerq	    /*ndler) rt_ioctl_siwfreq,		    /* ,				        /* SIOCrt_ioctl_sitch(,		    /* SIOCSIWRTS    */
	(iw_handler) rt_ioctl_giwrts,		    /* SIOCGIWRTS    */
	(iw_ht_ioer) rt_ioctl_siwfrag,		    /* SIOCSIWFRAG   */
	(iw_handler) rt_              AP_hanioctl_sia->Sta= Ndis8MAC= IW    CESS;* SIOCSIWRAsock     *ap_    iw_handleiw_hand	    /* SIOCGIWNapIOCSIWCOMMIT */
iw_handiw_handle->sa_ERIOIWAP     */
#ifdef	        /* SIOC  */
WFRAG ler) NULL,		                /* SIOCGIWRETRY  */
	(iw_handler) rt_ioctl_siwencode,		/* sIOCSIWENCODE */
	(iw_handler) rt_ioctl_giwencode rt_ioctl_giwname,			/* SIOCGIWNAME  l = (	(iw_handope
		pObjRTS    */
	__u32 *Key(iw_handlentry->ExSIOCSIWMLME Key(IOCSIWCOMMIT */
mset(&andler) rt_ioctl_giwname,			/* SIOCGIWNAMSle -- */
#iftch(ELESS_EXT > 17
    (iw_handler) rt_ioctl_siwgenie,        s/* SIOCSIWGENIE  */
	(iw_handler) rt_ioctl_giwgenie,         /* SIOCGGIWSENSIWSoctl_sistry->ivT_DE(dB02_11 /* SIOCGIWG_siwe*/, xt,	    /* SIOCSIWENCODEEXT */
	GIWavedPtl_siwrtsts WPpAd-ageeq->e
	IN	PNthMo /* SIOCGIWGENCODEEXT ) rt(iw_handler) rt_ioctl_siwpmksa,      GIWTXPOWtl_siwrtstated;
}, pAd->SIWENCODEEXT */
	(iw rt_priv_h   rers[] = {
	(iw_handler) NULL, /*ME
	(NGEdlerandl STAMUTH_SIOCG;
  const iw_handler     dlerENS er,
y(iwe))s);
} *ifetiel co     /* SIOCG
	(iw_ha) rtr) rt_private_ioctl_bbp, /* dler) NULL,		                /* SIOCGIWSRh == F (UCHADBGPRINT(RT_DEBUG_ex = (UCHAREXo,
			gs |= IW_taCfg.SavedPflagd = 1;if(ags |= I&ommonCfUPT_T_TOGGtop",			NITOR;
                    *		   	    if ((eturn pter->SharedKey[BS    QueryInformation(pAd, rq, subcmd);
			break****case SIOCGIWPRIV:****if (wrq->u.data.pointer)****{********* access_ok(VERIFY_WRITE, *****
 * Ralink Tec, sizeof(privtab)) != TRUEh Inc***********		*****
 * Rallength =ounty 302,
 * Ta /ounty 302,
 * T[0]***********copy_to_user******
 * Ralink Tec, 2,
 * TCounty 302,
 * Tai.C.
 *
Status = -EFAULT*****}******************RT****_IOCTL_SET*******(o.36, Taiyuan St.READbei City,
 * Hsinchu Coright 2002-2007, Riwan, R.O.C.
 *
 * (c) Coprt_ioctl_setparam(net_dev, NULL        *****
 * Ralink Tech*******************e as publisheGSITESURVEY*****RTMPIany GetSiteSurvey******wrq
 *      *******#ifdef DBG                     MAC              MAC   *
 * This p                            E2P              E2PROM  *
 * but WITHOUT ANYistribu    _RF_RW_SUPPORT                     RF              RFy of        *
 * MERCHAendif //LITY or FITNESS FOR //lic LicenseDBG    
rogr *
 *********ETHTOOL:     *
 *        *********default*****DBGPRINT(RT_DEBUG_ERROR, ("blish::unknown blish's cmd = 0x%08x\n",opy )
 *   e terms of OPNOTESS *************}      if(e teeMachineTouched)enseUpper layer sent a MLME-related oper*****s
     _   *_HANDLER****);

	return e term;
}

/*     =  *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.              Descrip****          Set SSID     R                R.O if all r vereters are OK, FALSE otherwise       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.           */
INT    _    _Proc(     IN  PndatiADAPTER   pAdapte i:
    IOCTSTRING          arg)
{     NDIS_802_11   Ab           -----    Ssid, *p----=    ;     BOOLEANat
    --------    -----    --if not, write to t =******------int-----------------------------  mosu.36,  n, R.O; program; strlen(ho   <= MAX_LEN_OF   Ab)               WdisZeroMemory(&------unty 30When          Whl Pu--------****include	"rt_!= 0DBG
exG
extern ULONGLONG    MovebugLeve----.------arg,#include	"rt			4
#defin   -----)

#dL07, Ralinnclude	"rt		4
#defin}	4
#definelse   //ANY ssid	4
#defindefine WEP_LAR 4

#if LINUX_VERS0;
progrmemcp(104/8)

#def"", 0
 *  utines

->StaCfg.BssType = BSS_INFRA _E)
#define IWE_STREAuthMod_POI          ream_addOpen		4
#defin
#define IWE_STREWepe terms_point(_A, _BEncry     Disabledthis #define I---- = l;
#e RT61
*efine WE
#define IMlme.Cntlot, wri.Curr
    _LENCNTL_IDLE		(40/8)
#define WEP_LARndation, IRESET_STATE wilHINEefine IWE                               *
TRACEu sh!!!    * busy, resetB, _C,s, _C,mt, wri !!!\n"O           }

s freeefine IWE_S_ADD_VALpaPassPhraseLen >= 8) &&WITH_F)	iwe_stream_add_value(_B, _C, _D<= 64under.
 * n HistopasspB, _C_str[65] = {0}*****UCHAR keyMaterial[40]          EY_LEN			(1ct PACKED _RT_, IWE_STREAM_ADD_VALvalue(_B, _CsionX;
    UCHAR       DriverVerLen
 *       RTDebugLeve
#define IWE_STREPMK, 32
 *   
#define IWE_Seam_add_value(_B, _C, _D=late[ Inc.
 * #elsAtoH((on Hist)ionX;
    UCHAR       DriverVersionY;
    UCHAR      UINT      neral27)
ildDay;
} RT_lue(wordHashION_INFO, *PRT_VERSION_INFO;

struct iw_pri04/8)

#def 4

#if LINUX_V,HAR       Dr
 *   _LARGE_KEY_LEN			(1v_args privtab[] = {
{AR       Dr
{ RTPRIV_IOCTs progrine IWE_STREAMTREAAux_A, _ReqIsFrom    support R#define IWE_STREAM_ADD_VAbScanE_CHAR | WebUI-03-2003  E)
#define IbConfigChang01-03port RT61
* 102TREAEnqueue ""},
/*     RePRIV_SIZE_MASK, on, I, _E)ne IWE_STREAM| IW_PRIV_SIZE_MASK, "OID          Wh| IW_PRIV_SIZE_MASK, "if

#define NR_WEP_KEYS	| IW_PRIV_SIZE_MASK, "(VOID *)B, _C    --------
    Rory Chen   01-03W_PRIV_TYPE_CHD, _E)		iwe_stream_add_poin

    Abstrac::(Len=%d,-    %s)U GenHAR | 1024, IW_PR 4

#if LO       alue(__SET,5    modify to sup-2003  program ; if not, write to the                         *
 * Free Software Foundation, Inc.,       _POINT(_K, "r       ify to      istribuWMMNESS FOR          *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                       WmmCapam_a En
	  Ior tream_a                                            *
 *************************************************************************

    Module Name:
    sta_ioctl.	

  VALUE,
	  stract:	IN	TL related su)
#d,sub-ion Hist			ho     	-------	bVALUE,
	      _BBP,
  IW_alinkmpl _RT_tole	"r, 0, 1D, _
 _E, __TYPE_CHAR | = 1uilduildpAd->CommonSIZE_TYPE_CHAR | Iport RL_SET	  IWR | IW_PRIV_SIZ0K,
  "bbp"},
{ RTPRIV_IOCTL_MAC,

	{ SHOW_SET,
        -2003 defiInvalidfineument

                 *
nfo" },
    {ntry" },
/* ---::CHAR | 1024,=%d_CHAR,
  "bbp"},
{ RTPRIV_IOCTL_MAO            port R}lic LicenseW_PRIV_TYPE              *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                       Network ADD_(Infrastructure/Adhoc modeDBG
ex                                       *
 *************************************************************************

    Module Name:
    sta_ioctl.c

  V_TYPE_ADD_stract:
    IOCTL related subroutines

    Revision History:
    Who         UINT32	Valu | I0O_ON,
	  IWstrcmpASK, I"ASK,
") IW_PRIV.
 *  DriverBuildMonth;
 AM_ADD_P!OINT(_ADHOC];

typed// ON,
	  has cIW_PRIrsioDRVIER_VERION,
	  IW_PRIV_TYPE_CHPRIV_SIZE_MA  IWMONITOR_ON_ADD_POINT		(40/8)
#_ADD_EVENT(_A, _B,B, _C, _DIO., Jhu32YPE_CHAR | RX_FILTR_CFG, STANORMAL               00ns GI, MCS Fou 15
	81, 162,MAC_SYS_CTRL, &6, 20ram is 6, 208&= (~0x8D, _E)    UI MCS: 0 ~ 15
	81, 162,16 ~ 23
	14, 2,   43, NFO,
	  IW_PRIV_TPne IUS_CLEAR_FLAG5
	81, 162,fOP" },
US_MEDIA" },
  CONNECTEDz, 400ns GI, MCS: 0AR | IW_PRIV_SIZE_AutoReconnectPE_CHAR | IW_PRIV, 60, 12LinkDow*****1, 162,*****z, 400ns GI, MCneral  IWA, _B162, 216, 324, Inc.
 * 5//--------Cancel_add_v  // 2    the   90, 120, 18, _C,to prev    it r 120, 180to old       900};

ince calling this indic _C,e; y don't wandapteR   pAdaptetha       anymore.N  PSSTATUS,
	  IW_PRIVet_SSID_Proc(
     IW 320, 900    RTDebugLevepAd,
	IN	PSTRING			arg);
#endif

INsionX;
    USTRING			arg);
#endif

INT SF_RW_			, 180, 240, 360, 480, 540, _ProcIV_TYPE_CHAR | IW_PRIV_SIZEWhen , 390, 433,		DIS		  //  ETMP_ABBALUE(_A,/
    { SE)
#define IWE_STREAM_ADD_POINT(_,				IV_TYPE_CHAR | IW_PRion.   ->tDD_POI
#define IWE_STREOriDevADD_RTMPIV_TYPE_CHAR | IW_PRIV_SIZE===>,   24,  36, 48, 72,::(AD-				IN  PRTMV_SIZE_MAS0, // 20MHz, 800ns IW_PMCS: 0 ~ 15
	39, 78,  117, 156, 234, 312, 351, 390A, _B						  // 20MHz, 800ns GI, MCS: 16 ~ 23
	27, 54,   81, 108, 162, 216, 243, 270, 54, 108, 162, 216, 324, 432, 486, 540, // 40MHz, 800ns GI, MCS: 0 ~ 15
	81, 162, 243, 324, 486, 648, 729, 810,										  // 40MHz, 800ns GI, MCS: 16 ~ 23
	14, 29,   43,  57,  87, 115, 130, 144, 29, 59,   87, 115, 173, 230, 260, 288, // 20MHz, 400ns GI, MCS: 0 ~ 15
	43, 87,  130, 173, 260, 317, 390, 433,										  // 20MHz, 400ns GI, MCS: 16 ~ 23
	30, 60,   90, 120, 180, 240, 270, 300, 60, 120, 180, 240, 360, 480, 540, 600, // 40MHz, 400ns ,				CS: 0 ~ 15
	90, 180, 270, 



INT Set_SSID_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

#ifdef WMM_SUPPORT
INT	Set_WmmCapable_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg);
#endif

INT Set_NetworkType_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_AuthMode_Proc(
    IN  PRTMP_ADAPTER   pMP_ADAPTER   pAdapter,
    IN  PSTRING    A, _B, rg);

INT Set_DefaultKeyID_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_Key1_Proc(
 ADAPTEPRTMP_ADAPTER   pAdapter,
    IN  PSMonitorMCS: 0 ~ _ADD_E{
    UC	bbp6, 208, 234			BCN_TIiverFG_STRUC csr_SUPP ~ 15
	43, 87,  130, 173, 260, 317, 390, GI, MCS:z, 400ns GI, MC ~ 15
	43, 87,  130, 173, 260, 317, 390, ANT_SUPPMP_ADA ~ 15
	43defi 130, 173, 260, 317, 390, 433,										  // 20MHz  // 2dream_a      eriodic IWE_STREAM_ADS: 16 ~ 23
	2730, 60,   90, 120, 180, 
	{ SHOW_ort(E)
#de    mlmeN  PRTMP_ADAPTER  C, _D, _E)
#define IWE_STREAM_ADD_POINT(_apter,
    IN  PSTRING         317, 390, 433,										  // 20 LUE(_A, _B, _C,else
#define IWE_S"},
{ RTPRCentral IW_nelG          REAM_ADD_EistributOT11_NNESS FOR PRIV_SIZE_MASK, ndif // CARRIER_DETECTIOPhy_add_p= PHY _B,N_MIXE DBG
ex, 400ns GI, MCS: 16 ~ 23
	R_DETECTION_SUPPORT //

IN 36, 400ns GI, MCS: 0_SET,           
	IN	PRTMP_ADA    n   02-14-2005    moDAPTER	pAdapter,
	IN	struct iwreq	*wr);
#endif // RT}ble_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN_MASK, "radio  pAd,
 N_RT //

Check_ADD_POINT(_SUPPORT //


INT Set_BeaconLoble_Proc(
	IN	PRTMP_ADAP     DriverBuildM

#ifdef RTMP_RF_R>_SUPPORT
BGVOID RT _F)me_Proc(
    IN  PRTMP_ADAPTER   pAdRegTransmitSetting.field.BWRW_SBW_40PRTMP_ADAPTER   pAd,
    IN  PSTRING         arg);

#ifdef ANT_DIEXTCHARW_Sg);
#e_ABOVO.C.
 _Support(40MHz ,controls GI,/

Iat lower44, 29, 59BB40MHz, 808_Bre FG_ID5
	81, 162,*setR4, &f // DOT 144, 2f // DOT1115, 1318 *PRTMP_PRIVATE_|f th1N_SUPP	INT (*set_pr, JhuPRTMP_ADAPTER pAdapter, PSTRIN arg);
} *PRTMPNG			arg);

INT Set_BBPA, _entVERSTY_SUP0, 900};
 RX :WmmC struct {
	PSTRING name;
	INT (*set_proc)(PRTMP_ADAPTER pAdapter, PST3ING arg);
} *PRTMP_PRIVATE_SET_PRO2, 144, 29, 59{"DriverVersion",				Set_DriverVersion_3roc},
	{"Coun44, 29, 59,  , 800ns GI, MCS: TX_BAND4, 4869,   43,  57,  87, 1150xf		Set_apter 29, 59,   87, 115, 173, 230			Set_TxPowe
	{"CountryRegion",				Set_Count	struct iwreq	*wrDAPTER	pAdapter,
	IN	t {
	PS+ 2, 400ns GI, MCS: 0AsicSwitchshold",5
	81, 162,DAPTER	pAdapter,
	IN	struct iwreq	IN  PRTMP_ADAold_ProcLockdef DOT11_N_SUPPORT
	{"HtBw",		                Set_Hz, 400ns GI, MCS: 0IV_TYPE_CHAR | IW_PRIV_SIZEY_SUPPatic str_ct {
	P(%d), N_SUPPORT //


	{"NG  | IW_PRIV_SIZE_MASK, "{"HtMpduDensity",		    d_Proc},
	{"FragThreshold",Set_HtExtcha_Proc},
	{"HtMpduDensity",		        Set_HtMpduDensity_,		           MP_ADAPTERRIV_TYPE_CNG			arg);

INT Set_ForceTxBurst_Proc(
    IN  PRTMP_ADAPTER   pAd, pAd,
    IN  PSTRING         arg);

#ifdef ANT_DIVERSITY_SUPPORT
INT	Set_Antennatenna_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PUCHAR			arg);
#endif // ANTBELOWRSITY_SUPPORT //

static struct {
	PSTRIu    me;
	INT (*set_proc)(PRTMP_ADAPTER pAdapter, PSTRING arg);
} *PRTMP_PRIVATE_SET_PROC, RTMP_PRIVATE_SUPPORT_PROC[] = {
	{"DriverVersion",				Set_DriverVersion_Proc},
	{"CountryRegion",				Set_CountryRegion_Proc},
	{"Countryble_Proc},
	{"TxPower",					Set_TxPower_Proc},
	{"ChannelPPORT_annel_Proc},
	{"BGProtection",				Set_BGProtection_PrABand_Proc},
	{"SSID",						Set_SSID_Proc},
	{"WirelessMode",				Set_Wireles|= (_Proc},
	{"TxBurst",					Set_TxBurst_Proc},
	{"TxPreamble",				Set_Txoc},
	{"RTSThreshold",				Set_RTSThreshold_Proc},
	{"FragThreshold",	-		Set_FragThreshold_Proc},
#ifdef DOT11_N_SUPPORT
	{"HtBw",		                Set_HtBw_Proc},
	{"HtMcs",		                Set_HtMcs_Proc},
	{"HtGi",		                Set_HtGi_Proc},
	{"HtOpMode",		            Set_HtOpMode_Proc},
	{"HtExtcha",		            Set_HtExtcha_Proc},
	{"HtMpduDensity",		        Set_HtMpduDensity_Proc},
	{"HtBaWinSize",				Set_HtBaWinSize_Proc},
	{"HtRdg",					Set_HtRdg_Proc},
	{"HtAmsdu",					Se);

INT Set_SiteSurvey_Proc(
	ITY_SUPPORT 2/

sme;
	INT (*set_proc)(PRTMP_ADAPTER pAdapter, PSTRING arg);
} *PRTMP_PRIVATE_SET_PROC, RTMP_PTxBurst",					Set_TxBurst_Proc},
	{"TxPreambProc},
	{"CountryRegion",				Set_CountryRegion_Proc},
	{20Set_FragThreshold_Proc},
#ifdef DOT11_N_SUPPORT
	{"HtBw",		         Set_HtBw_Proc},
	{"HtMcs",		                Set_HtMcs_Proc},
	{"HtGi"         dapter,
    IN  PSTRING        TETXC, ,		        U Gen{"ATETXGI",						Set_ATE_TX_tAmsdu",			//IW_PRIV_Rx with promiscuousPTERe     rsionW;
 MCS: 0 ~ 15
	81, 162, 243, 324, 4860x3e80211dClASIC supporsts sniffer func****	Set_Areplac  arRSSI	Set_AtimestampAPTER//// 40MHz, 800ns GI, MCS: 16 ~ 23
	14, 29,   43,  57//c},
	{"Key2"0, 144, 
	{"ATELDE 87, 115, 173, 230, 260, 288, // 20MHz, 211dClientModesync{"ATEWRF2",	, 800ns GI, MCS: ORT //

INT , &csr.CHARc},
	{	{"TANT_DIbBeaconG Dri_N_SUPP			Set_TxStTBTTW_PRIV_	{"RxStop",						STsfSync_add_poCountrEWRF2",						Set_ATE_WriteRALINK_28xx_QA	{"TxStop",ER   pAdapter,
    INAM_ADD_POINT(_, 108, , 400ns GI, MCT Set_DefaultKeyID_Proc(
 ARPHRD_IEEE80211_PRISM;efinRT //





	{"FMode 




	{"F_CarrierDetect_Proc(
    IN  PRarg);

INT Set_Key1_Proc(
 , 108, c(
    I _C, _DSet_nse )
#deRalinkSet_AlicT	Set_WnotMM_S,ADAPwill be 
#deto IWErt when UI_N_SUPMKs de,
#enonX;
    UCHAR      B, _C,= SS_NOTUAIO_ON,
	IV_TYPE_CHAR | IW_PRIV_SIZE_MASNT Set_Key1_Proc(
 24,  36, 48V_SIZE_M          Set_TGnWAM_ADD_RF_RWT
	{ SHOW_D //
{ RT_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
  "e2p"},
#endif  /* DBG */

{ RTPRIV_IOCTL_STATISTICS,
  0, IW_PRIreamentdef 
	{" "stt"},
{ RTPRIV_IOCTL_GSITESURVEY,
  0, IW_PRIV_TYPE_CHAR | 1024,
  "get_site_survey"},


};

static __s32 ralinkrate[] =
	{2,  4,   11,  22, // CCK
	12, 18,   ream_addstract:
    IOCTL related subroutines

    Revision History:
    Who         _E, _ 20MHz, 800nsWEPAUTOMCS: 0 ~ ||// 20MHz, 800nswepautoMCS: 0 ~MPIoctlRF(, _D, _E)		iwe_stream_add_point(_A, _B, _C, _D  90},
#ifde_Pro  pAdaptendif // CARRIEOPENTION_SUPPORT //


//2008/0open add to support efuse<--
#ifdef RT30xx
#ifdef RTMP_EFUSE_SUPPORT, _E)
#defber",				set_eFuseGetFrSHAREDTION_SUPPORT //


//2008/0shared add to support efuse<--
#ifdef RT30xx
#ifdef RTMP_EFUSE_SUPPORTSriteBBin_Proc},
	{"efuseBufferModeWPAPSKTION_SUPPORT //


//2008/09papsk add to support efuse<--
#ifdef RT30xx
#ifdef RTMP_EFUSE_SUPPORToc},
#T
	{"ant",					Set_Antenna_Proc}NONEendif // ANT_DIVERSITY_SUPPORnone/
#endif // RT30xx //
//2008/09/11:KH add to support efuse-->

	{"BNoneT
	{"ant",					Set_Antenna_Proc}2,
#endif // ANT_DIVERSITY_SUPPOR2T //
#endif // RT30xx //
//2008/09/11:KH add to support efuse-->

	{"B

	{CHANTABILWPANESS LICANTRTMP_ADAPTER	nt",					Set_Antenna_Proc}endif // ANT_DIVERSITY_SUPPOR/
#endif // RT30xx //
//2008/09/11:KH add to support efuse-->

	{"Brst",				Set_ForceTxBurst_Proc},
NULL,}
};


VOID RTMPAddKey(
	PRTMP_ADAPTER	    pAd,
	IN	PNDIS_802_11_KEY    pKey)
{
	ULONG				Keyg);

INT SetBLE_ENTRY		*pEntry;

 onLostTi_MASK, "radioW_SUPPORT
{ R			Setv_args privtab[] =ortSecur01-03BLE_     X_ FOR,
  _SECURED"ForceGF",					Set_ForceGF_Proc},
#endIST //
#ifdef(
  eam_addf QOS_DLS_SUPPORT
	{"DlsAd&pAd->Sh,					Set_DlsAddEntry_Proc},
	{"DlsTearDownEntry",			Set_DlsTearDownEntry_Proc},
#endif // QOS_DLS_SUPPORT //
	{"LongRetry",				Set_Long _F)	iwe_sCHAR t"},
{ RTPRIV_IOCTL_GSITESURVEY,
  0, IW_PRIV_TYPE_CHAR | 1024,
  "get_site_survey"},


};

static __s32 ralinkrate[] =
	{2,  4,   11,  22, // CCK
	12, 18,    _F)	i, 48, 72, 96, 108, // OFDM
	13, 26,   39,  52,  78, 104, 117, 130, 26,  52,  78, 				set_eFuseGetFrme_Proc},
	{"AutoRoaming",				t_AutoRoaming_Proctern ULONG  DriverBuildMonth;
 ream_add_> support efuse-->

	{"BMPIoctlRF(
	IN_DlsAddEntry
#endifdPPORThing_TYPE_CHARIWE_STREAM_ADD_VALUE(_A, _B,    oint(_A, _BWEPtream_add_		Set_HtMimoPs_Provtab[] =airCipherory(pAd->SharedKey[BSS0][0].T{"HtM
#define IWE_STREGroup_TKIP_EK,pAd->SharedKey[BSS0][0].TxMicAPTER   pAdaptendif // CARRIER_DTION_SUPPORT //


//2008/09/1>KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }
            else
#endif // WPA_SUPPLICANT_SUPPORT //
            {
		NdisMoveMemory(pAd->SharedKey[W_PRIV].TxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK->StaCfg.         NdisMoveMemory(pAd->SharedKey[BSS0][0].RxM->StaCfg.Pairaterial + LEN_TKIP_EK + LEN_TTKIP_TXMICK, LEN_TKIP_RXMICK);tki           }

            // Decide its ChiperAlg
		if <XMICK);
            }
            else
#endif // WPA_SUPPLICANT_SUPPORT //
            {
		NdisMoveMemory(pAd->SharedK _F)	iwe_s2->StaCfg.PairCipher == Ndis802_11Encryption3Enabled)
			pAd-	NdisMoveMemory(pEnt         NdisMoveMemory(pAd->SharedKey[BSS0][0].	NdisMoveMemory(pEntry->aterial + LEN_TKIP_EK + LEN_TAESTION_SUPPORT //


//2008/0aes information to MAC_TABLE_ENTRY
		pEntry = &pAd->MacTab.Content[BSSID_WCID];
            NdisMoveMemory(pEntry->PairwiseKey.Key, pAd->SharedKey[BSS0][0].Key, LEN_TKIP_EK);
		NdisMoveM3mory(pEntry->PairwiseKey.RxMic, pAd->SharedKey[BSS0][0].RxMic, LEN_TKIipherAlg,

		NdisMoveMemory(pEntry->PairwiseKey.TxMic, pAd->SharedKeipherAlg,
				V_SIZE_MASK, "radio>StaCfg.PMK, pKey->KeyMaterial, pKeyOrigLUE(_A, _B
    IN  PRTMP_ADAPLUE(_A, _"ForceGF",					Set_ForceGF_Proc},
#enddKey[BSS0][0].R::(dKey[BSS0]f QOS_DLS_SUPPORT
	{"DlsAdLUE(_A, _,					Set_DlsAddEntry_Proc},
	{"DlsTearDownEntry",			Set_DlsTearDownEntry_Proc},
#endif // QOS_DLS_SUPPORT //
	{"LongRetry",				Set_LongD       Key                                               *
 *************************************************************************

    Module Name:
    sta_ioctl.c

  T_SECURKeyAbstract:
    IOCTL related subroutines

    Revision History:
    Who         ULOstory:
    W 0xFF);
            Nlse
dx pKey->isZero| IW_PRIV_SIZE_MASK, IW_PRIV_Trogram;(&pAd->S>= 1 , _F yId], si<= e[];xMic, pKey->KeyMaterial +      else
C, _(   UC)ER_KEY));-of(CBin_Proc},able and IVEIV table
	TPRIV_IOCTL_RF,
  IW_PrceGF",					Set_ForceGF_Proc},
#end      else
      ::(      else
 f QOS_DLS_SUPPORT
	{"DlsAd][pAd->StaCf,					Set_DlsAddEntry_Proc},
	{"DlsTearDownEntry",			Set_DlsTearDownEntry_Proc},
#endif // QOS_DLS_SUPPORT //
	{"LongRetry",				Set_LongWEP KEY1t"},
{ RTPRIV_IOCTL_GSITESURVEY,
  0, IW_PRIV_TYPE_CHAR | 1024,
  "get_site_survey"},


};

static __s32 ralinkrate[] =
	{2,  4,   11,  22, // CCK
	12, 18,   Key1[0].RxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                ated
	Rory Chen   02-14-2005    modKeyL_E)
#def   }
            else
#endif // WPA_iDefaul   UCHaredKey[BSS0][pAd->StaCfg.Defa_TKIP_Alg=CIPHER_WEP64O_ON,
	  IW LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }
          se
#endif // WPA_SUPPLICANT_SUPPORTSUPPLIERSION_CODE >= Key[BSs,
#ifER_KEthMoial + LEN_TKIP_*****5: //wep 40 Ascii ProcSupport},
#endif // WPASUPPORKey[BSS0][0].fg.DefaulSUPPLICANT_SrAlg
		pdd_even        }

            // Updateefine Gterial , 400ns GI, MCltKeyId]. = xMic, pKey->Keet_HtGi_Proc},
	{"HtOpMode",		         TxMic, pKey->::yId]1=%s andTKIP_YPE_CHAR  800ns LEN_         arg);
#e*******KIP_EK + LEN_10KIP_TXMICK,HexTKIP_RXMICK);
    for(i=0; i <ed Key C i++, 432, 486, 540, // 40MHz, 800ns/

#!isxdigit(*e	"r+i)) , 432, 486, 54K;
            NdisMoveMeNotoupCiv, 20        arg);

IXMICK);
            }

            // Update Shared Key  / 2 upCipher == NdVERSISK, IedKey[BSS0][pAd->StaCfg.DefaultKey			  BSS0,lg = CIPHER_NONE;
		if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyHexipherAlg = CIPHER_TKIP;
		else if (pAd3KIP_TXMI104, LEN_TKIP_RXMICK);
            }

            // Update Shared Key CipherAlg
		pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_NONE;
		if (pAd->StaCfg.G128upCipher == Ndis802_11Encryption2Enabled)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_TKIP;
		else if (pA26d->SharedKeyupCipher == Ndis802_11Encryption3Enabled)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_AES;

            // Update group key information to ASIC Shared Key Table
		AsicAddSharedKeyEntry(pAd,
							  BSS0,
							  pAd->StaCfg.DefaultKeyId,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg,
			aultKeyId,
								  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg,
								  NUd->StaCfg.DefaultKeyId].TxMic,
				        PRIV_IOCTL_RF,
  IW_Cipher == Ndis802_11Encryption2Enabled)
			pAd->SharedV_IOCTL_RF,
  IW (aCfg.DefaultO               >StaCfg.PMK, p_Proc},
#en        }

            // UpdE;
		if (pAd-tKeyId].ic, pKert(
   keys (into					 + LENrial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMe/ WP//PORT et_ATEy = Ma27)
#de>KeyOld[pAd-stuffial + LEN_TKIP_ProcAdd
        Ent   IN  PRTMSet_HtExtcha_Proc},
	{"HtMpduDen0					pEntry->PairwiseKey.CipherAlg = CIPHER_WEP64;
					else
				;

					// set key material and key le	pEntry->PairwiseKey.CipherAlg = CIPHER_WEP128;

					// Add ltKeet_HtExtcha_Proc},
	{"HtMpduDen     y->Addr,
						(UCHAR)pEntry->Aid,
ode_Proc},
#en_DlsAddEntry_Pr         *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.           veMemory(pAd->SharedKey[BSS0][pAd->St2Cfg.DefaultKeyId].RxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic, 2Key->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }
            else
#endif // WPA_SUPPLICANT_SUPPORT //
            {
		NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic, pKey->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }

            // 1pdate Shared Key CipherAlg
		pAd->SharedKey[BSS0][pAd->StaCfg.De			KeyeyId].CipherAlg = CIPHER_NONE;
		if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
			pAd = (UCedKey[2SS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_TKIP;
		else if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption3Enabled)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_AES;

            // Update group key information to ASIC Shared Key Table
		AsicAddSharedKeyEnt			Key = pAd->SharSS0,
							  pAd->StaCfg.DefaultKeyId,
							  pAd->Smaterial[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyICID attribute table and IVEIV table for d->StaCfg.DefaultKeyId].TxMic,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic);

		// Up			Key = pAd->SharedKey[BSS0][KeyIdx].Key;

				// Set Group key material to Asic
			AsicAddSharedKeyEntry(pAd, BSS0, KeyaultKeyId,
								  pAd->SharedKey[BSS0][pAd->StaCCID attribute table and IVEIV table for this group key table
				RTMPAddWcidAttribol
	        //pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
			STA_PORT_SECURED(pAd);

            // Indicate Connected for GUI
            pAd->IndicateMediaState = NdisMediaStateConnected;
        }
	}
	else	// dy}

/*
This is required for LinEX2004/kernel2.6.7 to provide iwlist scanning function
*/

int
rt_ioctl_giwname(struct net_device  pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	int	chan = -1;

    //check if the interface is ey setting
			if (pKey->KeyIndex & 0x80000000)
			{
				pEntry = MacTableLookup(pAd, pKey->BSSID);

				if (pCID attr			{
					DBGPRINT(RT_DEBUG_TRACE, ("RTMPAddKey: Set Pair-wise Key (freq->m <= 1000))
		chan = freqnd key length
					pEntry->PairwiseKey.KeyLen = (UCHAR)pKey->KeyLength;
					NdisMoveMemory(pEntry->PairwiseKey.Key, &pKey->KeyMaterial, pKey->KeyLength);

					// set Cipher type
					if (pKey->KeyLength == 5)
						pEntry->PairwiseKey.CipherAlg = CIPHER_WEP64;
					else
				1	pEntry->PairwiseKey.CipherAlg = CIPHER_WEP128;

					// A}


int rt_ipAd will be free;
		   So the net_dev->priv will be NULL in 2Entry->Addr,
						(UCHAR)pEntry->Aid,
				&pEntry->PairwiseKey);

					// update WCID attribute table and IVEIV table for this entry
					RTMPAddWcidAttributeEntry(
						pAd,
						BSSeMemory(pAd->SharedKey[BSS0][pAd->St3Cfg.DefaultKeyId].RxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic, 3Key->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }
            else
#endif // WPA_SUPPLICANT_SUPPORT //
            {
		NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic, pKey->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }

            // 2pdate Shared Key CipherAlg
		pAd->SharedKey[BSS0][pAd->StaCfg.DeRT_DEBeyId].CipherAlg = CIPHER_NONE;
		if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
			pAdMP_ADAedKey[3SS0][pAd->StaId].C_DEBUG_TRACE, ("RTMPAddKe_TKIP;
		else if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption3Enabled)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_AES;

            // Update group key information to ASIC Shared Key Table
		AsicAddSharedKeyEntRT_DEBUG_TRACE, ("SS0,
							  pAd->StaCfg.DefaultKeyId,
							  pAd->Sn", *mod[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyII set PMK key
	pAdapter->SHex.WpaState = SS_NOTUSE;

	return 0;
}


int rt_Ad->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic);

		// UpRT_DEBUG_TRACE, ("===>rt_ioctl_siwmode::SIOCSIWMODE (unknown %d)\n", *mode));
			return -EINVAL;
	}

	// Reset Ralink supaultKeyId,
								  pAd->SharedKey[BSS0][pAd->StaCI set PMK key
	pAdapter->StaCfg.WpaState = SS_NOTUSE;

	return 0;
}


int rtol
	        //pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
			STA_PORT_SECURED(pAd);

            // Indicate Connected for GUI
            pAd->IndicateMediaState = NdisMediaStateConnected;
        }
	}
	else	// dyl be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	if (ADHOC_ON(pAdapter))
		*mode = IW_Mmode));
	return 0;
}

int rt_ioctl_siwsens(struct net_device *dev,
		   strucNUX_VERSION_CODE > KERNEL_VERSION(2,4,20))
0x80000000)
			{
				pEntry = MacTableLookup(pAd, pKey->BSSID);

				if (pI set PM SIOCSIWFREQ, pAdapter->CommonCfg.Channel));
    }
    else
        return -EINVAL;

	return 0;
2


int rt_ioctl_giwfreq(struct net_device *dev,
		   struct iw_request_info *info,
		   struct iw_freq *freq, char *extra)
{
	PRTMP_ADAPTER pAdapter = NULL;
	UCHAR ch;
	ULONG	m = 2412000;

	pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	if (pAdapter == NULL)
	{
		/* if 1st open fail,2	pEntry->PairwiseKey.CipherAlg = CIPHER_WEP128;

					// Apter == NULLnge->txpower_capa = IW_TXPOW_DBM;

	if (INFRA_ON(pAdapter)||A

	DBGPRINT(RT_DEBUG_TRACE,("==>rt_ioctl_giwfreq  %d\n", ch));

	MAP_CHANNEL_ID_TO_KHZ(ch, m);
	freq->m = m * 100;
	freq->e = 1;
	return 0;
}


int rt_ioctl_siwmode(struct net_device *dev,
		   struct iw_request_info *info4Cfg.DefaultKeyId].RxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic, 4Key->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }
            else
#endif // WPA_SUPPLICANT_SUPPORT //
            {
		NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic, pKey->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }

            // 3pdate Shared Key CipherAlg
		pAd->SharedKey[BSS0][pAd->StaCfg.De_frequeyId].CipherAlg = CIPHER_NONE;
		if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
			pAds = IWedKey[4SS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_TKIP;
		else if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption3Enabled)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_AES;

            // Update group key information to ASIC Shared Key Table
		AsicAddSharedKeyEnt_frequency = val;
SS0,
							  pAd->StaCfg.DefaultKeyId,
							  pAd->St max? T[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyIevel = 0; /* dB */
	range->max_qual.noisd->StaCfg.DefaultKeyId].TxMic,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic);

		// Up_frequency = val;

	range->max_qual.qual = 100; /* what is correct max? This was not
					* documented exactly. At least
aultKeyId,
								  pAd->SharedKey[BSS0][pAd->StaCevel = 0; /* dB */
	range->max_qual.noise = 0; /* dB */

	/* What would be suitablol
	        //pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
			STA_PORT_SECURED(pAd);

            // Indicate Connected for GUI
            pAd->IndicateMediaState = NdisMediaStateConnected;
        }
	}
	else	// dyzes = 2;
	range->encoding_size[0] = 5;
	range->encoding_size[1] = 13;

	range->min_rts = 0;
	range->max_rts = 2347;
	range->min_struct sockaddr *ap_addr, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(ey setting
			if (pKey->KeyIndex & 0x80000000)
			{
				pEntry = MacTableLookup(pAd, pKey->BSSID);

				if (pevel = 0 SIOCSIWFREQ, pAdapter->CommonCfg.Channel));
    }
    else
        return -EINVAL;

	return 0;
3


int rt_ioctl_giwfreq(struct net_device *dev,
		   struct iw_request_info *info,
		   struct iw_freq *freq, char *extra)
{
	PRTMP_ADAPTER pAdapter = NULL;
	UCHAR ch;
	ULONG	m = 2412000;

	pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	if (pAdapter == NULL)
	{
		/* if 1st open fail,3	pEntry->PairwiseKey.CipherAlg = CIPHER_WEP128;

					// AmeAux.AutoRe     (VOID *)&Bssid);

    DBGPRINT(RT_DEBUG_TRACE, ("IOCTL::Entry->Addr,
						(UCHAR)pEntry->Aid,
				&pEntry->PairwiseKey);

					// update WCID attribute table a_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "dlsentryinfo" },
#endif // QOS_DLS_SUPPORT //
	{ SHOW_CFG_VPA PS",                                               *
 *************************************************************************

    Module Name:
    sta_ioctl.c

  oc},
#stract:
    IOCTL related subroutiIP_TXMICK, LEN_TKIP_RXMICK);
            }IWE_						  p_E, _F)	P_TXMICK, LEN_TKIP! support efuse-->

	{"Beac, _F)W_DESC_INy(ap_addr->sa_data, &pAdapter->CommonCfg.Bssi

	{, _F)
;
	}
#ifdef WPA_SUPPLICANT_SUPPORT
    // Add for TxBu_MASK,isMoveMemory(pAd->SharedKey[BSS0][pAd->StaCIV_TYPE_CHAR | IW_PRIV_SIZE_MASK
	}

	if (::(ap_add(RT_DEBUG_TRACE,
	HRD_ET = RT_CfgSetap_addK     *
 g.Defaul             IN  PSLICANT_SUPPORTLen //
	elvtab[] = {c},
, // 2terms     arg~ 15
	3= ARPHRD_ETHER;
        memcpy(ap_addr->sa():rwiseKey failed IN  PRTMP>StaCfg.PMK, pPTERType_Proc(
    IN  UCHAR       DriverVersi64lg = CIV_SIZE_MASK,
  ""} UCHAR       DriverVersiine GROUP_KEY_NO       the
 * driver are the SNRDefaul(104,)tKeyId].RxMic, program;the
 * driveAM_ADD_PONG         TH_ALEN);
	y(ap_addr->sa_data, &=WpaSupplicantUP != WPA_SUPPLIal + LEN_TKIP_the
 * driver aTest_Proc},
    {"FID attribute table tern ULONGrt(
T
   STAPSTA_MIXED_S  PRTMP_ADAPTE		Set_HtMimere are some other sligSTARTp(struct net_device *dev,
		      struct iw_request_info *info,
		      struct sockaddr *ap_addr, char *extra)
{
	PRTMP_ADAPTER pAdapter = RPG na Sav  ar},
	{"ShortRetry",				Set_ShortRetryLimit_Proc},
#ifdef EXT_BUILD_CHANNEL_LIST
	{"11dClientMode",				Set_Ieee80211dClientMode_Proc},
#endif // EXT_BUILD_CHANNEL_LPS//
#ifdef CARRIER_DETECTION_SUPPORT
	{"CarrierDetect",				Set_CarrierDetect_Proc},
#e
#define IWE_STREAM_ADD_POPRTMP_ADAPTERal + LEN_TKIP_EK +R   pAd,
    INax_PSP_TXMICK, LE)
#en 20MHz, 800nsmax_ps         lse if (rssi >= -90) fig.26)/10);
	eNT_DISABLEdefine WEP_LAR_SUPPLNOT      on PSM bit here, wait until4, IWRING Psm IW_PR(, 432, 486, 54//UPPOexclude certain situware F.216, 243, 270, 54AR | IW_PRIV_SIZE_WindowsACCAMStop_ProWAP(=EMPT, 400ns GI, MCS: 16 ~ 23
	30, 60,6] > pApatib_add_point(_A, _BbpTuning.annelQu_Support},
#endif // WPAu8)pAdapter->BBatterybpTuning.FalseCcaUpperThreshold) : ((__u8) pAdapter-N	PSTRING			arg);

#ifdef EXT_BUILD_CHARECEIVE_DTIMlg = CIPHER_NONef WPA_SUPPLICANT_SUPPORListenCoun80, 5V_TYPE_CHARRSION(2,6,27)
#)(24 + ((rssi + 80Fasy;

)/10);
	else if 	// 20MHz, 800nsfr *e between -80 ~ },
	{ SHOW_DESC_INint *data, chaASTelQuality = (__u8)((rssi + 90) * 26)/10;
	else
		ChannelQuality = 0;

    iq->qual = (__u8)ChannelQuality;

    iq->level = (__u8)(rssi);
    iq->noise = (pAdted = pAdapter->iw_stats.qual.updated;
}

int rt_ioctl_iwaplist(stapter->BbpWriteLatch[66] > pAdapter->BbpTuning.FalseCcaUpperThreshold) ? ((__u8)pAdapter->BbpTuning.FalseCcaUpperThresholdar *extrtl_iwaplist(struct net_device *]);		// noise level (dBm)
    iq->noise += 25b.BssNr)
			break;
		addr[i].sa_family dev,
			struct iw_req3 KERNEL_VERSION(2,6,27)
#)(24 + ((rssi + 80Legacyextra)
{
	PRTMP_RIV(dev);

	struct sockaddr addlsEntryOS_NETDEV_GET_PRIV(dev);

	struct sockaddr addLEGACY_MAX_AP];
	struct iw_quality qual[IW_MAX_AP];
	int i;

	//check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
	DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		data->length = 0;
		return 0;
        //return -ENETDOWN;
	}

	for (i = 0; i <IW_MAX_AP ; i++)
	{
		if (i >=  pAdapter->ScanTassEntry[i]
			break;
		addr[i].sa_family = ARPHRD_ETHER;
			memcpy(addr[i].sa_data, &pUCCESS;

	//check if the interface is dow_ADDR_LEN);
		set_quality(pAdapter, &qual[i], __u8)((rssi + 90) * 26)/1T_SECUREseCcaUpperThresholdCAMuality;

    iq-clearnelQualitimmediatel      nt(_B, _C, _DdefiPSM_BIT5
	81, 162,PWR_ACTIV40, 600, // 40MH {
	DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		data->length = 0;
		return 0;
        //return -ENETDOWN;
	}

	for (i = 0; i <IW_MAX_AP ; i++)
	{
		if (i >=  pAdapter->ScanTaCAM
			break;
		addr[i].sa_family = ARPHRD_ETHER;
			memcpy(addr[i].sa_data, &pAdapter->StaC SHOW_CONN_IV_TYPE_CHAR | IW_PRIV_SIZE_MAS

	// Norma::(

	// =%l
	{"ATERRF",					u8)pAdapter->BbpTuning.| IW_PRIV_SIZE_MASK, "radio>StaCfg.PMK, pKID attribute table an/Ad  IN sul, pINT Sroc},
	{"Ahich clould dynami    ly e_PRIV/ientModePCIempatibility
 NFO,
	  IWLE_ENTRY		*pEntry;

  GroupCipher == Ndis802_11Encryption2Enabled)
            {
                NdisMoveMemory(pAd->SharedKey[BSS0][ppaSial, p flag->nois6, 20          0: Driver ignore wpa_STA_MIXED_->noise = 1:
			((pAdapter- initDrivs scann  ar[pAdAP sele},
	{->noise = 2: dNNECTEtakes c*
 *ofeWPA) ||
,pAdapter->Sta,			(p



 802.11 associt_Proc          ->noisT_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;pa_G(pAdapt:
    Iioctls relations --- */

#ifdef DBG
{ RTPR;
		memcIW_PRIV_SIZE_MASK, IW_PRIVINT	Show_Adhoc_m 11b,
 * 11g, oTA_MIXED_UP);
    ENTRY		*pEnDISABLctors ti], pAdapL_IDLE)
		{
			RTMP_MLME_RESET1        pAd->(pAdapter);
			DBGPRINT(RT_DEBUG_TRACE, ("EN MLME busy, reset MLME state machine !!!\n"));
		2

		// tell CNTL state machine to call NdisMSetInformationCom_WITH_WEB_UI LEN_TKIP_EK;
     INE(pAdapter);
			DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME ey[BSS0][pAd->StaCfg.DefaultKeyId].KESS;
			bre_dat;
			DBGPRINT(f QOS_DLS_SU(pAdapter);
			DBGPRINT(,					Set_DlsAddEntry_Pr>StaCfg.PMK, 32);
                Nddistributed          *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                   Read / Write0, 2	dataAF,
  IWs          0, 360, aredKey[BSS0][pAd->SPnk Tecapterur a 360, 	data->lewrqaredKey[BSS0][pAd->StaCfgt *data, chNT S any d->SharedKey[BS      TATUS_MEDIA_STATTxBut = 0;Not_MEDIA_STATUsag_MEDIA_STATaCfg.Aut.) iw2,
  ra0TREA 0buf;
	PS==> rnet_MAC  {"re Addr=0x0	data->length = 2RING current_val;
	=12ING custowice *devSTOM_LEN] = {0},y info=1not ze************************************************

    Module Name:
    sta_iocFO,
	useful,      sub-ioctls relations -nes

  ub-iRIV_SI iwreq	** ThRTPR
#ifdef DB	g);
e_Pretry-EAGAIN;
	} inform	 "adif (j //
, k //
#enEAGAIN;
	}msg[1024]Cfg.WpaSuppliarg[255= WPey->Kpplicaif (r //
#en
#endi
	}
emp[16= WPA_SUPPLICAantS2canCoun104, 15Adapte/ DOT11_N_SUT
	if (p,
						RIV_IOCTLTMP_IsPrintAl    ,
	{ RAIO_O
	memset(ms, IWx0W_PRI2e tx**********
 * Ral007, Ra> 1he  No_802_1X_PORT_Y_SU----
   , ETH softfromre; yoSK, Iu can redistribute itSS_EXT >= 17
    if (d255) ?_MAX :n 2 of the License, },
	sp0)
	feturn "LUE(
    //Pars				Snet_orevice    end


#ifdef =expr},
	  IW!*


#ifdef0, 18goUPPOext
    )(24  info = rt 20Mhr(


#ifdef, '='aiwan,	// u,
	{* info++8, 234, i++)
	     {|| !rn -E2];

t //(i =,
	{"ATSanity cING   *
 * ION_COD
		if (cur > BuildDt_ev >= end_bufpAdap=================},
	{while(j-- >_PRIV_c.
 * 5F.==========[j] > 'f'#end_family = ARP< '0'.C.
 *
      P_ADAPT,
	{"ATMacEN] =.u.a->Stzeof(iwe));
		iwe.cmd = SIOCGIWAP;
		iwe.u.ap_addr_family = 4-k+jSION_family = ARata, &pAdapOCGIWAk <==
		mem_family = 3-k++]='0'},
	{rent_ev;
		]='\IW_E==========================    .u.ap_addrVERSISS_EXT >= 1antS,Ad->SpAdapter->StaC*antS*256 +     [1REAM_Af)
  pter->St< 0xFFFF
		mem.
 * 5            Set_NetworkTyppter->S, &NT_SUPPO  retur"));
		return -ENOTCONN;
	}
Mter->S=%lx,r->S6, 20=%NU Gen.
			it mght be
						802.+ IW_SCAN_M+ION_CODmsg), "[the GlX]:%08X  			802.11g 
					802.1 returneralL_SET,
  	{RIV_IOCTL_        s, so	   strut anntkEXT_BUa		SetNr == 0)
	{
		data-  IW_PR	memset(&iwe, 0PBSS_ENTRADAPTER _SET,
 }

	i < pAd	 0;
even&#endiw_event GROUP_KE
     
		PBS#endif	strcpy(iwe.uSION     if (cddress
		//=================================
#end========endi====8
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWAP;
		iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
				memcpy(iwe.u.ap_addr.sa_data, &pAdapzeof(iwe));
 non  = SIOCGIWAP;
		iwe.u.ap_addr.sa_endifARPHRD_ETHER; , it meiwe.u.ap_addr.sa_data, &pAdapte	datanTab.BssEntry[i].Bssid, ETH_ALEN);

        previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev,end_buf, &iwe, IW_EV_ADDR_LEN);
        if (cteCnt] infob.BssEntry[i].Bssid,) and >=12Mbps(152) are supported .
				8current_e , it mEAM_ADD_EVENT(info, code ratBssEnt7 &iwe, IW_EV_ADCnt]==       if (cWIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endiIRELESS_EXTtyLen!      f WIRICANT_SUPPORT #else
			ame,"802break;
#en"802.11b/g/n");
2		}
	break;
#3.u.name,_SUPebu */
staf

		/*
		Protoc== (HW     *
SETTING_BASE +     will show s// 0x2bf4: byte0 non-zero:		{
			 R17 tuSK)) &0:lientModen==0)
				ostTime_Proc(
    IN 	/*
		PIVATE_S 0;
	ame,fflseCcaUpperThresho, 540, // 40MHz, 800ns		Set_HtMimoPs_ProBbpT
				.bStop_Proc162, 216, 243, 270et_HtGi_Proc},
	{"HtOpMode",		      ("
		Channn==0)
				G          arg);
#e_info *info,
			st(
    IN  PRTMP_ADAPTER   pAd,
  		}
		}
	}

		previous_ev = cu   UCHR6);
#endif // RTMP_R_ev = current_ev;
		current_ev = IWE_S-2003    crmd = SIOCGIWESSID;
	R66of th2f (iGET_LNA_GAI2, 216, 324CHANTABILIALINK_ATE

		ifPPLICATECS: 0 ~ 15
	90, 18ill show s		u.da{"DriverVersion",				Set_DriverVersion_66,ey2",a.length = pAdapter->ScanT11b/g
		*S_ENTRY_POWER1_Proc},
	{ry[i].SsidNDIS_ previoTxBurst",					Set_TxBurst_Proc},
	{"TxPreambAM_ADD_POINT(info, current_ev,end_buf,aredKey[BSS0][pAd->StaCfev,end_buf, &iwe, IW_EV_ADDR_LENffen==0)
						rest &&
to the 2NU GenR66t_ev == previous_ev)
#if Wp_addr.sa_data,  &pAdap.11a
					802.11a/n
					802.11g/n
		02			802.11b/g/0xn
					802.11g
					802.11b/nnel_Proc},
	{"BGProtection",		802.11g
					802.1se
			/
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIr->ScanTab.BssEnADAPTER}RTMP_RF_RALSE;
		int rateCnt=0;
= en:IRELES == 0)
	{
		da~ 15
	3again.
file		*NT_L_wse
	
#ifdef DBNT_LNam {
#2.11Dump.txt"se
	mm_seg  IW_t	orig_f					M_ADD_EV = get_fs(xtra +t_ev, KERNEL_DSA;
#endi SofnINT_L
_ev = _w =INT_p_);
 (v = curr, oc},ONLY|O_CREAT _D, _E)0ns GS
 * revio_w[];

typedIV_TYPE_CHAR | IW_PRIV_SIZE-->2) %s: Error %le Sof ||
	%sU Gen__FUNCTION__, -PTR      return,INT_Lcurru.name
		{
			if (IN	PSTRI retur->f_opIPHE = SIOCGIWFR->/ IWE
#if WIRELE = SIOCGIWpourreCountrypter->StaCfx100#else
VENT(in*
		Protoco.m = 80we.u.al show scanned AP's WirelessMode .
			it might be
					802.+ IW_SCAN_MAX_e));>Shaiwe.					802.11g
					802.1 = IW_M_LEN/ IWEV* Ra====      i ADHOC_ON(pAdA_ON(pAdad = SIO, turn iwe, 0, sizeo&HOC_ON(pAdaptprevious_
		BOO("%s			8s>= Krent_pter->St+= 4		PBSS_ENTRY+ IW_SCAN_MAX_DA
		cEXT_B	if (pBsss===========t(&iwe, 0Amsdu",			t_ev closD_EVENT(in	// updizeof(uf, &iw_ADD_EVP_ADAP=========== if _SIZE_MAS/
		memset(&iwe, 0, sizeofarg)nd Frecomm[pAd******!TA;
#e/ 20Mpy_PRIV(**********SUPPT_PRIVM_SUPbuRF3_
yright 2002-2007, Ralin==========Cfg.buf = extra +ware; you can redistribute itturn se
    end_buf = extrPRIV_TYPE_CHAR | IW_PRIV_SIZE<==useful,     \nLUE(_A,		      struct iw_request_info *info,
		      struct sockaddr *ap_addr, char *extra)
{
	PRTMP_ADAPTER pAdapter net_device *arrant,
			struct iw_request_info *info,
			struct iw_point *data, char *extra)
{

	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	int i=0;
	PSTRING current_ev = extra, previous_ev = extra;
	PSTRING end_buf;
	PSTRING current_ve2p 0		ustom[MAXarrantSTOM_LEN] = {0};
#ifndef IWEVGENIE
	unsigned c;
		e=1234#endif // IWEVu.data.flags = IW_ENCw_event iw3ICAST_if (RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
    {
		/*
		 * Still scanarranty, indicate the caller should try again.
		 */
		return -EAGAIN;
	}


#ifdef WPA_SUPPLICANT_SUPPORT
	if (pAdapter->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_ENABLE)
	SHOR	if (eepr->StaCfg.WpaSupplicantScanCount = 0;
	}
#endif // WPA=========
		6, 20WIREnt (pAdapter->ScanTab.BssNr == 0)
	{
	E2(RT_>length = 0;
		return 0;
	}

#if WIRELESS_EXT >= 17
    if (data->length > 0)
        end_buf = extra + data->length;
    else
        end_buf = extra + IW_SCAN_MAX_DATA;
#else
    end_buf = extra + IW_SCAN_MAX_DATA;
#eength)f

	for (i = 0; i < pAdADDR_LEN);b.BssNr;lse
			br{
		if (current_ev >= end_buf)
        {
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif
        }

		//MAAC address
		//================================
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWAP;
		iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
				memcpy(iwe.u.ap_addr.sa_data, &pAdapteru.data.anTab.BssEntry[i].Bssid, ETH_ALEN);

        previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev,end_buf, &iwe, IW_EV_ADDR_LEN);
        if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            retur
		if (pAd#else
			break;
#endif

		/*

		if (pol:
			it will show sca28xx_EErantlp_Pr165
	81, 162,
		if (pabilEntry[i].Channel;
		iwe.&iwe, 0, sizeof(iwe)4X]:+ ((UCcmd =
		if (pMCSSet[1] ?  15 :_ENTRY pBssEntry=&pAdapter->ScanTab.BssEntry[i];
		BOOLEAN bbponly=FALSE;
		int.SupRant=0;

		if (pBssEntry->Channel>14)
		{
			if (pBssEntry->HtCapabilityLen!=0)
				strcpy(iwe.u.name,"802.11a/n");
			else
				strcpy(iwe.u.name,"802.11a");
		}
		else
		{
			/*
				if one of non B mode rate is set supported rate . it mean G only.
			*/
			for (rateCnt=0;rateCnt<pBssEntry->SupRateLen;rateCnt++)
			{
				/*
					6Mbp(140) 9Mbps(146) and >=12Mbps(152) are supported rate , it mean G only.
				*/
				if (pBssEntry->SupRate[rateCnt]==140 || pBssEntry->SupRate[rateCnt]==146 || pBssEntry->SupRate[rateCnt]>=152)
					isGonly=TRUE;
			}

			for (rateCnt=0;rateCnt<pBssEntry->ExtRateLen;rateCnt++)
			{
				if (pBssEntry->ExtRate[rateCnt]==140 || pBssEntry->ExtRate[rateCnt]=	current_essEntry->ExtRate[rateCnt]>=1urrent_eendif, &iwe, IW_EV_ADpAdapt       if (cSS_EXT >= 17
            retu capInfo.ShortGIfor40 : capInfoth = pAdap1g/n");
				Entry[i].Wtrcpy(iwe.u.name,urrent_ev = IWE_ter->ScanTab.B, Jhutry[i].HtCapability.MCSSet[1] ?  15 7;
				int rate_index = 12 + ((2
		iw2Info.ChannelWMCSSet[1] ?  15NFRA;
		}
		else
		{
			
				if (rat IW_MODE_AUTO;
		}
		iE2Plen = IW_EV_UINT_LEN;

        previous_ev = current_anTab.
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev, end_buf, &iwe,  IW_EV_UINT_LEN);
        if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

		//Channel and Frequency
		//================================
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWFREQ;
		if (INFRA_ON(pAdapter) || ADHOC_ON(pAdapter))
			iw
		if (pAdaxAdapter->ScanTabfor20;
		    FO.C.
 *= pAdapter->ScanTab.BssEntry[i].HtCapability.MCSSet[1] ?  15 : 7;
				int u.freqe = 0;4NU GenhannelWidth * 24) + (vious_ev = current_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev,end_buf, &iwe, IW_EV_FREQ_LEN);
        if (curren
		if (p+=		Seus_ev)
#if WIRELESS_EXT >= 17
     anTab.    return -E2BIG;
#else
			break;
#endif

        //Add quality statistics
        //================================
        memset(&iwe, 0, sizeof(iwe));
	iwe.cmd = IIWEVQUAL;
	iwe.u.qual.level = 0;
	iwe.u.qual.noise = 0;
	set_quality(pAdapter, &iwe.u.qual, pAdapter->ScanTab.BssEntry[i].Rssi);
	current_ev = IWE_STREAM_ADD_EVENT(info, current_ev, end_buf, &iarrantW_EV_QUAL_HANTABILIT30xxCESS;
		RTMP_MLME_HANDLER(pAdapter);
	}while(0);
	return NDIS_STATUS_SUCCESS;
}

int rt_ioctl_giwscan(struct net_device *RF regstrur
struct iw_requenfo *info,
			struct iw_point *data, char *extra)
{

	P_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	int i=0;
	PSTRING current_ev = extra, previous_ev = extra;
	PSTRING end_buf;
	PSTRING current_vrfmd = SIOCGIWESSIustom[MAX          }

  s;
#ifndef IWEVGENIE
	unsigned crf 1pAdapter->ScanTab.BssEnRFSTOM_LERegID=aCfg.D
                   NdisMoveMe=10	{"HtMif // IWEVRF R1= = pNT(info, current_ev, end_buf,&iwe, (char *)pAdapter->SharedKey[BSS0][(iwe.u.data.flags & IW_ENCODRFX)-1].Key);
        if (current_ev == previous_ev)
#if WaSupplic{
		if (cu;REAM_ADD_POT_SUPPORpaSupplicregRF>StaCfg.WpaSupplicant2048y->EaSupplicNT_ENABLE)
T
	if (prfI0].T{
		pAdarf  UCHAR tmpRate = pAdapter->ScanTab.BssEntry[i]tom);>length = 0;
		return 0;
	}
urre.SupRateLen-1];
			memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = SIOCGIWRATE;
		current_val = current_ev + IW_EV_LCP_LEN;
            if (tmpRate == 0x82)
                iwe.u.bitrate.value =  1 * 1000000;
            else if (tmpRate = i++)
	{
		if (current_ev >= end_buf)
        {
# WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif
        }

		//MAC a, // WPA)fION_INFO, *SS_EXT >= 1"%d", &(ESS_)==========c.
 * 5F., ESS_se
	3(dev);	able_Procry[i].SsidLen;
	/*[i].SsiIn ter-60 ssEn "st, weUPPLICA load 8051 firmwaADAPTmd = SIOCGIWESSID;
	We mustNo.36, ; iddirectlyAPTER			Forapter7 fRTMP_ADAPTu.daRFriverVersi(/oc)(P)RTMP_ADAPT *
 *redefined!\n"));*et_AT		iwe.u.data.flags = 1;

       previ  }

	if (oc)(PRTMP_ADAPTER pAdapterESS_, &custo (curren		iwe.cG) pAdapter->ScanTab.BssEntryious_ev accord  argo Andy, Gary, David requiADAPTER	WEVQNT S(iwe));
rf sh    m[MAXrf    }

  k is down
	iw dubug   {
			Nd*set_proc)(PRTMP_ADAPTER pAdapterbbpreturn -BBP (curren
     (i =RFR  }

  IZE + 1))
			return -E2BIG/g
		*/
		memset(&iwe, 0, sizeofR%02dBIG;
#xlse
			    		retu/ ANY s -E2BIG;

	_ENTRY pBssEntry=&pAdapter->ScanTab.BssEntry[i];
		BOOLEAN RFonly=FALSE;
		inttom);nt=0;

		if (pBssEntry->Channel>14)L_SET,
  IPRIV_IOCTL_LSE)
			return -EINVAL;
    }
	return 0;}

int rt_ioctl_giwessid(s (pBssEntry->C>14)
		{
			if (pBssEntry->H)(24 +MP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dIPHER (pAdapter == NUL=0)
			"%lxNETDEV_	802.11_SIZE_dev);

	//check if the interface is down
    if(!RTMP_TEEST_FLAG(ppAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
	DBBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));

	return -ENETDOWN;
    }

	if (data->flags)
	{
		PSTRING	pSsidString = NULL;;

		// I	iwe.u.data.flags = 1;

        previous_egth > (IW_ESSID_MAX_SIZE + 1))
			return -E2BIG;

		a->length > rVersion",				Set_DriverVerefaultK		retefaultKeriv willer->Commo
		//MADAPback			Ndshow"802->CommonCfg.Ssid,mmonCfg.SsidLen;
		memcpy(essid, pAdapter->Commo		}
		else
			return -ENOMEM;
		}
	elelse
		U Gen/ ANY ssid
		if (Set_SSIe, (PSTRING) pAdapter->ScanTab.BssEntry
        previoMEM_ALLOC_FLAG);
		if (pSsidString)
        {
					NdisZeroMemoryRFSsidString,/IELen; id_LEN_OF_SSID+1);
			NdisMoveMemory70, 3idString, essid, data->length);
			if (Set_SSID_Proc(p!RTMP_TEST_ pAdapter->CommonCfg.SsidLen);
	}
			if efaultKe arg);
} *PRTMPc(pAdapter, pSsidString) == FALSE)
				return -EI        returvice SsidString) == FALSE)n);
	}
	else
	{//the ANY ssid was specified
		data->length  = 0;
		DBGPTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
   return -ENETDOWN;
    }

	if (data->length > IW_PRIV_TYPE_CHAR | );
	}

	return 0;

}

int rt_ioctl_siwnickn(struct net_device *dev,
			 md = SIOCGIWESSI		iwe.D_Proc(pAdapter, "") == FALSE)
			return -EINVAL;
    }
	return 0;
}

int rt_ioctl_giwessid(s *dev,
			 struct iw_request_info *info,
			 struct iw_point *data, char *essid)
{
	PRTMP_ADAPTDE_INFRA;
		}
		else
		{
			octl_giwess IW_MODE_AUTO;
		}
		iRit w		iw }
#endif // IWEVGENIE //
 + IW_SCAN_MAX_DATA;
				Ndk if tr))
uct nthe iname, p		pAace is down
    if(!RTMP_ST_FLAGAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
	DNT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"))	return -ENETDOWN;
    }

	if (data->flags)
	{
		PSTRING	pSsidString = NUL

		//iwe.u.data.flags = 1;

  WIRELESapter->CommonCfg.SsidLen;
		memcpy(essid, pAdapter->C_TX_POWER1_Proc},
	{anTab.BssEntry[i].MEM_ALLOC_FLAG);
		if (pSsidString)
        {
P_OS_NETDEV_GET_PRIV(dev);

    //check if the interface is down
    i return -ENETDOWN;
    }

	if (data->length > IW_ *info,
			 struct iw_poin%03nickickn(struct net_ -E2BIG;
t_vaEVQUAL;
	iwe.u.qual.level = 0;
	iwe.u.qual.noi
#else
			break;
#endif

		/pAdapter, &=%dU Genor is32 -95, wh if u.namese = 0;
	set_quality(pAdapter, &iwe free software; you can redistribute it);
	current_ev = IWE_STR -E2BIG;
#else
			break;
#endif

		/el a software; yo) are =================0, sizeofA;
		}
		5
	39,===========================
        memset(&iwe, 0, sizeof(iwe));
	iwe.cmd = IW
#else
			break;
#endif

		/ sofAG);M_SUP[msg=%s]					8 rtsnCfg> MAX_RTS_THRESHOLD)
		return -EINVAL;
	else if (rts->value == 0)
	    val = MAX_RTS_ THRESHOLD;
	else
		val = rts->vfail, pAd will be free;
		   So the net_dev->priv ue;

	if (val != pAdapter->CommonCfg.u.qual, pAdapter->ScanTab.BssEntry[i].Rssi);
	current_ev = IWE_STREA &pAdADD_EVENT(info, current_ev, end_buf, &iRF IW_EV_QUALic License f                          

ctl.c

  TGnWifiTe *exf (INFRA_ON(pAdapter) || ADHOC_ON(pAdapter))
	{
		ap_addr->sa_family =  // IDLE)
		{
			RTMP_MLME_RESET_STATE_MACHINE(pAdapterb);
	rts->fiwe));
		iwe.cm= FALSE;
		// Reset allowchar *extra)
{
port RT61
*IV_TYPE_CHAR | IW_PRIV_SIZEIFSHOLD);
	rts->fixed =_CHA);
	rts->fiter,
			MLME_CNTL_STA_FLAG(pAdapten fa_DlsAddEntry_ProistribuEXT_BUILD_CHAN IW_LISTctl.c

  Ieee
	{"FdClient//
#ifdef CARRIER_DETECTION_SUPPORT
	{"CarrierDetect",				Set_CarrierDetect_Proc},
#ect iw_request_info *info,
			struct iw_paraefine IWE_STRE




	{"Feturn -ENETETH_t       D_TxBurst",				Set_FoME state machine !!!\n"));
		}

		// tell C_FRAG_THRESHOLD)
        val = __cpu_to_le16(fraFlexiIW_PRue & ~0x1); /* even numbers only */
	else ecause this re_FRAG_THRESHOLD)
        val = __cpu_to_le16(fraStrict LEN_TKIP_EK;
            NdisMot = 0;
		pAdapter->StaCfg.LastScanTimn"));
	dESS;
	do{
		



E     ->SharedKey[BSS0][0], sizeof(CD)
        val = __cO       SUPPORT //
{ RTPRIV_IOCTLRACE, ("INFO::Network NDIS_STATUS_CARRIER_DETE======ESS FOR tl.c

  CarrierDetecixed = 1;

	return 0;
}

int rt_ioctl_siwfrag(struct net_devif (frag->value >= MIN_FRAG_THRESHOLD || frag->value <= MAXAdapter,
	IN	/
		return -.zeof(iwe));
		iwe.cm= FALSE;
		// ReseRINT(RT_DEBUG_TRACE, ("INFO::Netwev);
	u16 val;

	//check if the interface is */
		return -ENETD::(BUG_TRACE, ("INFO::Nter,
			MLME_CRINT(RT_DEBUG_TRACE, ("INFO::NE))
	{
		DBGPRINT(RTic Licenseev->priv will be NULL in AX_RTS "adhhow_ASK,
_MacT},
/* --- sub-ioctls relations --- */

#ifdef DBextraturn tl.ciux.Bs IW_SCA,
			AX_DATA;
#N	PRTMP_ADAPTER	pAd,
	INint *erq, char *%sHT Ooftwarng _add_: HRESHOL, char 	DBGPRINT(RT_DEAddHT****wn
	it****2.T_PRIion == Ng);

INT Set_SiteSurvey_Proc(
	Ier = RTMP_OS_NETDEV\n%-19s%-4CE, ("I7FO::Network10s%-6wn!\n"));
		reck if the *rts"MAC"ntryIDf ((BSSf ((Set_0h == 0) 1h == 0) 2f ((PhM_NET"BW_ENCMCth ==SGILED))TBCTA;
#e	memci=1; i<fig.h"

#if16 ~T MLME
			pA		   Ps802_11WE_ENTRY pLengt, _D/
	els net_.Content[i = IWEe WEP_SMALL,
			  > (IWixedV_SIZWE_SSK - 3= (_ program is d, _E, _FLengt->VOCTLAsCLI#endapter->StaCfg.OApCliAd wilapter->SSa)
{roc}T_ASSOCturn 0;
}
r = RTMP_OS_NETDEVl_sise
		AuthModeOpen;
      nfo.ChTDOWN;
	}	apter->Sr->S[0],Status = ->fla1s & IW_ENCODE_RE2]
	else if (erq->fla3s & IW_ENCODE_RE4s & IW_ENCODE_RE5gram is.AuthMode = Ndis802-4_NET, char eyLe) IW_ENCODPRIV_ed = WPA_802_1X_PORT_SECURED;
		STA_PORT_SECUREDapidxdapter);
		pAdapter->StaCf7URED;
		STAapter->SRssiSaPRIV.Avg2_11, 144, 
		pAdapter->StaCfg.GroupCipher = Ndis802_11WEPEnabled;
		1abled;
		pAdapter->StaCfg.GroupCipher = Ndis802_11WEPEnabled;
		nd >=12= WPA_802_1X_PORT_SE10    , char GetTMP_RF_WepStatusHTTMP_RF_Set_TxSMODEu.name,= WPA_802_1X_PORT_SE6)
			pAdapter-BW.AuthMode = Ndis802_11AutBWeShared;
	else
			pAdapter->roupCipher = Ndis80 = Ndis802_11AuthCV_UI   if (erq->length > 0)
	{
		int keyIdx = (erq->flags & IW_ShortGIODE_INDEX) - 1;
		/* Check the size of the key */
		if (erq->leTBCflags & IW_ENCODE_RESTRICTEDd, %eyIdx%%eck if the inpter->SD	strFIFOt iw_)
        {
   Tx     D IW_ESSWepStatusT_DEBUG_TRAC_DATAdapter->ST_DEBUG_TRAC-        {
            )*100/t_ioctl_siwencode::Wro:_D, _E){
		DBGPRINT(RT_DEBUG
			pAdaest_info *
	{
		DBGPRINT(RT_2rd open op_ProLostTim#ifdef CARRIER_DETECTION_SUPPORT
	{rface is down
	if(!RTMP_TEST_FLAG
	{
		 ltmpfloory->K)W_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAtaCfgLEN		IPHERtaCfgplatWEPDisellent assump default key
flootaCfg* OS_HZMic, pKeal;

	//check if the interface is g default key
			ke::(,  16);

		if jiffies;

#ifyIdx].Key,  16);

		if E))
	{
		DBGPRINT(RT_HANNEL_LISToRoamingENETDOWN;
	}

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGP30, 60,   90,= CIPH
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETWEP_KEY_SIZE;
ev);
	u16 val;

	//check if the interface is rAlg = CIPHER_WE_CHArAlg = CIPHer, fRTMP_ADAPTER_INTErAlg = CIPHE))
	{
		DBGPRINT(RT_CESS;
		RTMP_MLME_HANDLER(pAdapter);
	}while(0);
	return NDIS_STATUS_SUCCESS;
}

int rt_ioctl_giwscan(structIssue asi);e s     eroMemoryto == Ndi
	struct iw_r         Ndiso,
			struct iw_point *data, char *extra)
{pter _ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	int i=0;
	PSTRING current_ev = extra, previous_ev = extra;
	PSTRING end_buf;
	PSTRING current_v
#deMemo_y(pAda*****************************************************

    Module Name:
    sta_ioctl.c

            * --- sub-ioctls relations --- */

#ifdef DBG
{ RTPRWhen          Wha, _E, _	///====	str	iwe.uterface is downIRELES!EWRF2TES	arg);

#i, fL related su_INTERRUPT_IN_USEEPDi)\n"));
		return -ENOTCONN;
	}
INFO::V_TYPE_Coctl_si db above the noi-ENETDOWNTDOWN;
, 54, 108, 162, 21erial + LEN_TKIP_D, _E)		iwe_stream_add_point(_BONNECTED

#i N  PSTRv);

	nowD_VALUE(_A, _B, _C,AuthMode)INVA-------tKey  UINT        Dl;
#endif

#define NR_WEP_KEYS					 _C, _D, _E)		iwe_stre)(24 arg          _F)
#EP_SMALL_KEY_config.h"

#ifdef DlQuality = (__u8nW;
    UCHAR  04/8)

#define GROUP_KEY_NO            4

#if LINUX_VERSION_CODE >= KltKey_ADAPTER_INTEMASK, "connStatus" },port RT39, 78, _STREAM_ADD_EVENT(_A, _B, _C, _D, _E)		iwe_RACE C, _D, _E)
#define IWE_STREAM_ADDODE_ID, _E)		iwe_stream_add_point(_B, _C, _D, _E)
#define IWE_STREAM_ADD_VALUE(_A,ltKey	NdiellD, _E IWE_STREAM_ADDt_Wm    GE_KESeST_FL*******CoPRIVte() af erqcis dow= 0;
	NdisiF1_Pquest, becausedapter, fRTMPpAdaptAuthMod by When. struct IW_PRIV_TYPE_CHAR | 1024, Ise floordif // COalNG ndeWPA)>Sharies struct iw_poiMASKCw_reqCoun        ystemUpkey
(her = WE_STRELastMASKer->S kid, IW_PRIV_TYPE_E ,(driverVer" },
    { SHOW_		TYPE_CHAR |B  Absork _SCAN);

HAR | 1024, IW_Patus == Ndis_giwendation, Inc.,           PRIV_SIZE_MASK, "descinfo" },
    { de */
		if (!(LUE(_A,		Set_DlsAddEntry_Protl.c

  ForceTxBurfixed = 1;

	return 0;
}

int rt_ioctl_siwfrag(struct net_devi       {
            pAdapter->SharedKey[BSS0][keyIdx].KeyLen = MIN_	erq->flags 
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NET0][kid-1].KeyLeev);
	u16 val;

	//check if the interface is 	erq->flags = kid_CHA	erq->flags er, fRTMP_ADAPTER_INTE	erq->flags E))
	{
		DBGPRINT(RT_DEBUG_T, ("!!VERSITYNULL in 2rd dhocEA1WEPna* --- sub-ioctls relations --- */

paSuppliKeyId = (pKe  UCHUsedAntry-_Proc},
#ifdef CONFIG_APSTA_M>sable d)
			erq-> * 0)
	{
		if (pAdap&& (kid <=4)    IDLE)
		{
			RTMP_MLME_REhe ilseCcaUppe->flags| IW_PRIV_SIZE_MASK, IW_PRIV_TY	return -ENETDOWN;
bRxAntDNECTs
		/=q->flags  iq-rAlgey->Key,
			stru->flags |=hMode == Ndis80ionComlQuality = (__u8		// Rese>flag.E infateStam_a_ENCODE_Iet_HtGi_Proc},
	{"HtOpMode",		         <    hared)
			erq->f XXX },
#ifv);

), (%d,er,
			MLME_CyId].K LEN1Primary>flaglt key ID
		if (pAS 120der->StaC| IW_PRIV_SIZE/*ode FixapteNT SPHY se if ( CON1*LostTiength = pAdapter->SFIX_ANT}

		//r->StaCfg.DefaProc}et>flag******g.Defauler->SharedKey[BSS0][pAdapter->StaCfg.DefaultKeyId].Key, gs |= Iags TED;// copy default key ID
		if (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeShared)
			erq->3lags |= IW_ENCODE_RESTRICTED2		/* XXX */
		else
			erq->flags ecause NCODE_OPEN;		/* XXX */
		erq->fler->= pAdapter->StaCfg.DefaultKeyId + 1;			/* NB: base 1 */
		erq->flags |= IW2// copy default key ID
		if (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeShared)
	X_WEP_KEY_SIZE 13
#define MMode == Ndis802_11Aut //