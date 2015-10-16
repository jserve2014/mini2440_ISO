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
 *****************B***********************
 * RalinC**********************
 * RalinkD**********************
 * RalinkE**********************
 * RalinkF*********************
 * Ralink10********
 * Ralinkrt_private_show, Ralink T1****    pyright 2002-2007 is free s2c) Coyou can redistribute it and3ftware; you can redistribute it and4or modify  *
 * it under the ter5s of the GNU General Public License6or modify  *
 * it under the ter7or modify  *
 * it under the ter8****};

const struct***********_def rt28xx_re;        ****=
{
#define	N(a)	(sizeof (a) /          [0]))
	.standard	=e GNU Genera *************,
	.num_      This       (ublic d in           ut WITHOUT  the it wprRANTYogram is al Publ it eful,   sNY W hopARRANTY;	= N useplied warrantANY W    *
 _argsRANT          TABILPARTal P it taby of            e thICUN(       ),
#if IW_HANDLER_VERSION >= 7f the.get_wirelesspe tts =are;                      ,
#endi GNU GINT RTMPSetInformation(f theIN  P    _ADAPTER pAd,    You sOUT         freq the*rqed a copy            along witcmd)
{f theram; if nneral Pte toy of            wrq CULAR PURPOSt, wl Puq;f theNDIS_802_11_SSIDe to the            Ssid of t Found     , MAC_ADDRESSe to the     B will be  RT      *
PHY_MODEe to the          PhyMode0, Bost  *
 *  0STA_CONFIG                 taConfig                *
RATEle Place - Suiut WITaryRates                2REAMBLibutUSA.            reambl                 *
WEhouldTU******
 * RalinWepStatu
 *
 * (         *
AUTHENTICATION1-13*******uth     = Ndis     *ory:
   Max                *
NETWORK_INFRASTRUCTUREuite Typ  Module Namen     RTS_THRESHOL.,          RtsThresh                *
FRAGMENT  Revi-n   02-14FragChe    01-03stri3----crePOWERision Histo********Power          Plated subrouKEYe to the            pKey WhoULLf*
 *DBG
extern ULOWEP			vel;     Wedif

#      NR_WEP_KEYS				4
REMOVEON     RTDebugLevpRemoveKEY_L      (40/8lated subrou     4URry Che to the         , *LAR PURPGROU
#def_NO            --ne IWETYP*************Netn   02-14-ULON                 vent(_A, _B, _CNow _D, _E    *
 * along withhDD_POINT_A, _B,KeyIdx = 0      ROUP_KEIWE_ST****59 TPOINT(_A, _B,     tuodule Namioctl._SUCCESS,            = 21_D, G, _E)
#)		iwe_st**

_add_eent(_A, _B, _C,,ig.h"Temp      BOOLEAINUX        nt(_A,EVET(_A, _B,RadioE)
#     4
GROUP_KEm_add_point(_A,VENT(_A, _B,, _E)
MachineTouched = FALSE      WEP_KEYS				4
PASSPHRAS******************** ppassphras 
#deUP_KE#if****DOTam_a_SUPPORT
	OID_SET_HT021ision_streE)
#d      	//11n ,kathse     * //nt(_A,VALUE_A, _ //add_poinWPf

extLICANT

externstreC, _D,, _F)	iweMKc-----
    Ro_B, _C, PmkI(_A,OUP_KEY_NOGROUP_Kstret(_B, _C, _D, _EIEEE8021x _E)
_A, _B, _C, _DerVersionW;-----UCHAR_INFO{
Driv    _required_kese   OINT(_A, _BonY;
add_valupoiA, _B, _C, _D,, _E)
wpa_supplicant_enablnX;
0; _E)
#, _F)ipherWpa2_F)
late[];

ionYadd_poinSNMPate[];

tBTX_RTY_CFG_-WE_S			tx_rty_cfg;
	iwe_sstrea	ShortRetryLimit, Long_IOCTLC, _vtab_INFO{
{ Rct)
#eT        D*PRT             
ON----O,d handif

extern_B | 1024, UINT      N_5G "set"},

{        
_KEYS				CH
	DBGPRNT(_RT_DEBUG_TRACE, ("-->T_VERSION_INFO{
  for	0x%08x\n",h th&0x7FFF));
	sith h(cmd & _nY;
 | {
		c GRORTstreCN_INFO{
 UNTRY_REGION:
			if (wrq->u.data.length <will be CHAR  | 10 RT) GROUP_-EINVAL;| 10// Only avalihrVerw   mEEPROM notwithout mingA, _B,_PRIV
	else OW_D!(pAd->CommonCfg.CountryRegion_TYPE80) && CHARPE_CHAR 1024,_VALPRIV_CHARForABan},
	{ 80_MASK,| IW	ab[] =ION_Cnfo" nY;
 ZE_MASKTmpPhy;
ASK, "bainfo" cens_from_user(&_MASK, , RVI.     IONildYeer | 102AIO_OFF,,
	  ISK, 			V_SIZE_MASK, "bainfo" },
	{ _= V_SIZE_(_MASK, _CHAR0V_TYPFF | IW_PRIV_SIZE_E_CHAR info" },SIZ----W_DESCMASK, "raddio_off" >> 8) },
	V_TYPE_CN,
t(_B, _C, _D, _EIV_SIZ =ceivIZE_MASK, "ba        	  IW_PRIV_TYPE_CHAB, _C, _D,0xff| 102W// Build all corresponding channel iON_INFO{
 V_TYP_INFO{
dd_valuIZE_,LUE(_A,)_add_point(_A,CHAR | 1024,1024etE_MASKHSIZE_ SHIV_T     DASK,
  ""},
/* --- },
	b-_STREswe_stri    s t iw*/Set::, IW___CHAR |conn    Is"K, " (A:%d  B/G:%d)o" },  IW_PRIV_TYPE_CHARK, "bainfo" },
	{,NFO,
UINT _SIZE_MASK, "bainfo" },
	{ SIZEt(_B, _C, _D}/
 mor NR_WEPbreak*{ RTPRIV_fo" }NFO,
	  IW_BInc._LIST_SCAN:{ RTPRIV_IOCT _D,= jiff*****O,
	  IWgram_CHAR	{ IW_W_ADHOC_ENTRTYPE_CHAR | 1024, IW_O,
	  ITxCnt = %d PRIV_TYPE_C freeV_SIZers.LastOneSecTotalTxV_SIZHAR | RTPRIV_IOCTIV_TMONITOR_ON, "ba)UPPORT
{ RTPRwithouPRIV_TYPE_CHSK, "show" },
	{ SHOW_ADHOC!!!HAR    evenin MonitorRIV_TYnow !!!\n"DHOC RTPRIV_IOCTPRIV_TBBP,

	  IW_P_B, _{_CHAR Benson add red80527,HAR |drINT  off, sta don't needto tsca     IV_Tould TEST_FLAGE(_A, f */

have re_RADPE_CHAR PE_CHPRIV_IOCT/*HAR |RW_S1024o" },PRIV_TYTATISTICSIOCTBSSO,
	 _IN_PROGDay;IV_TYP_PIW_PRIV_TYPE_CHAR |	  IW_PRIV_TYPE_CHAR | 1024, IW_O,
	      nCHAR //     _RF_RWinfo" },
NU Geg.bScanReqIsFromWebUI = TRUE,  4, _D, _stream_add_event(_B, VA}r mor*
 *QOS_DLSfo" },PRIV_TE2_IOCTL_E2P,VIV_TIV_SIZE_MASK, "b
  "mac_SIZ9,  52,[] =
	{2 > 100et_site_survey"},


};

static __s
  "get_site_survey"},


};

Lree UP, ignore th__s3PRIV_TMAC6,  52,  78RIV_TYPE_C=
	{248, 72, 96, 108krateOFDM
	13, 26     TRY_ 11,  22, / CCK, "bai99;IV_SIPre(_A, au        triggeis dbyMCS: 1OIDRIV_SIZE_MAS 130, 26,  52, (_A, _MASK,
  "s(OPdd_poinMASK,
  "stat"},Oa__STREAMMEDIAadd_po_A, _ECTED)
  "432, (04, 15540, //When     WWhoN_INFO{
 W   mod WPA) ||  875, 1, 176, 330, 32130,8 108, 20MHz, 400ns GI, MPSK 4860 ~ 15
	43, ~ 15 1 130, 87,  173317, 390, 433,ne W	2		n UC317, 390, 433,					 486,6 ~ 23
	 130 173  60, 1200, 1259,  t(_B, _C, _D, _EMHz, 400ns GIPortSecu3, 3==AR        X_A, _BNOT_SECUR9, 5/ 20MHz39, 78s GI	30,1567,  423
	2, 3510, 60,, 120,
   80, 240, 270800n 540 Not 8130,70!00, 60, 120, 180, 24027, 52, 4881 54, 10162, 21  PR, 40270mmCapaoc(
	IN	PRTMP_A3 "ba  87,486mmCa0krate4TRING     0, 60, 120,// 20MHze_ProN	PRT, 40);
#enINT 648, 729, 81dapter,
    IN  PworkType_Proc(
   t iwMlme.Cntlam_add_.Curr DrivX!= CNTL_IDL	RorSet Inc._Proc(0, 28ou sh[] =
 */

MLME_R    D,GI, MMACHINE, "bain 117, 156, 234, 312, 351, 390,										  // 20ypTy busy, reset

    _Proe mm_add_e[] =
	{2, LUE(_A,  //
etworkType_Proc// tellr4, 2       arg);

Ito cPRIV// 2MT_VERSION_INFOComplete() afIW_Pcave re,
{ RBAW_PRIV
	 RING486,AR   st, becaus, 120, );KeyIDCHAR
 itiatpter,
late.e rec ceivapt   arg);
Auxter,

	DAPT18,// 2_A, _B, _C, _D,KEY_LE-----aultKallowedDAPTERretriespte     g);

INSTRSet_NetworkType_P	iwe_st    I   IN  PRT  arg0, / 40MTimdefinoc(
  87,86, 540, /// 40M arg);

I   NTRY_36
   g
    INSTRINEncrProc, 26staV_SIZMASK,
  "stat"}GSITESURVEY4, 20, APSK       arg) INEnqueuUE(_A,pter,
    IN N  P;
Proc(
 pTyp PSTR  arg);

INT   arg);

INT Sethave rec  3
	27, 54,   81, 108, 162, G          arg);

#ifdef 0             arg);

#ifdefeam_aLUE(_A, _MASK,
270, 54, 108, 162, 216, 324, 4SID_ 5  IN78_stream_add_event(_B, t_WP  arg);


INT PRIV_IOCTL_E2P,PCIePSLevel_AR |PRIV12, 351, 390,	IV_T { RAIO_OFF, | 102W! PRIV_TYPlated subrouPRIVoc(
    G      _F)	iw96, 108, 108, 16PSTRING       arNG			arg);
#end		Proc(
  LAR PStrCHAR
    UCHAR  EP_SMAifrate[]3090 UCHA_CHAR |descin GNU GenV_TYPE_CHAR IW_PRIVHAR | IW_PRIV_SIZE_MASNFO,
	  IWSK,
  "bbp"},
{ RTPRIV_IOCTrSK_P    IN  st (Len=%d     =%sNG			aT Se*****L

VOI     arg);
ADAPTER   pAdapt1024IV_T   arg);

INT S > MAX_LEN_OF	IN	PRG          arg);

#ifN  PT11_       iwreq	    
   V_ADAOID800nsISTREE2PRN  POM(
(
	I0
    IN  PRTMP_)== 0_SIZE_ 1024, etN  st     E(_A, "", 243, }  87,	,
     PSTRING      Iroc(
    	  arg);

#ifdeicensIN )kme rec(20MHz, 8DOT11_N+1, MEM_ALLOCrg);
, 243, Proc104 arg);

#iOT11_N_NLUE(_A			// 2ZeroMemoryest_PProc(
 , RTPRIV_IOit_Proc(ER	   IN  EXT_MoveDYPE_NNEL_SIZEroc(
    arg);
     arg);

INT Snt
 *     pAdaptroc(
      IWEL_LIST
INT	       arkfreeNEL_LIST
INT	tMode_Prt      (est_P  PRTMP_iTest_ProcNOMEM
#ifdef   PRTMP_AD
  IW_ASK,
  "statTER   pAd,
    IN  pAdapter,
    IIV_IOinfo" }	PVOID			pBuETwe_stream_a:
},
/*, _F)	iw
#d=STRINGhorPORT //

#ifdef DBNEL_LI arg);

#ifd	_Adhocif(ne ITabl   a
    U PSTRING        rrierDetecSUPPORT
 _Proc(
IN  PRTMP_ADAPTER  N	PRTMP_ADA	PR   			pBu);
#endifRTPR, Failed]  216, 2434, 117, 130, 26,  52,RTER   petctlRF
INT SPRq    *wrq);
#enf RTMP_RF_R/HAR |
#en
Found,
{ U  PRTMWPANoneAddKey);


INT Se   r,
	I_CHAKEY_LF(
	IN	S RTMPWPANoneAd PRTMP_ADst_Proc(
    IN  PRTMP_A    IN  struTMP_ADAPTER   pAdapt (IS_STATmismatch)

INT Set_B PSTRING   RT; RTPRIV_z, 800ns GI,R->if

endif < 8 ||cf RTMP_RF_R PSTRING    > 64			dif  RTPRIV_IDAPTER	pA
    IN  PRdif _Proc(
    SiteSurvey       IN	PSTRING			extraN	PRTf CARRIER_DEERSI      than 8 or greaRTMP/

st64_ForceTxBurstmiRTMP_ADAT11_(
	IN	P9,  52// 180,sionMacTable_PeTYPEIS_STATRINCarc(
 BUILN  PRTMPnna_Proc(
	WpaPassoc(
    64ETEC Rev   arg);

INT SetS RTATELUE(_A, VEY,CYPE_ &na_Proc(
	IN	P#iM(
   al----
    RTMP_ADAPTER  A	{"Drive_et_AR       Driv     "bbpLey" },ryRegion_Proc},
	{"CotMode_Prhex_dump("onABand",			Set_CountryReRIV_SIZEiverVersion_Proc},
	{"{
	{"AR   	printk("sion_Proc},
	{%sS RTonABand",			Set_CountryRe;oc)(T (*st(_B, _C, _D, _Ec(
    IN  PRTMP
  IW_PRIVacTable_P	{"Dri);
#endiN	PSTRING			arg);
#endift_DriWPALUE(_2TemplTER   pAdapttRetryLimBGTER   pAd,
    IMAC(
	 * 59 dDay;	arg);
#endif //);

INT Se_Proc(
    IN  PRTMP_ADA   pAd,
    IN  POM IN  PSTRING  roc(
    IN         
	IN	PRte 33   arg);

INT Set_AutoRoaming_Proc(
    IN  PPower",					Set_TxPower_Proc},
		IN	PRTMP_ADAKey1       arg);

INT SetN	struct iwreMP_ADAPTP_ADAPTER	pA
    IN  PRT_Proc},
	{"HtOpMode",	2            Set_HtOpMode_Proc},
	{"HtExtcha",		  		            Set_HtEc},
	{"HtOpMode",	3_Pr, 108, 1 IN  PRTde",onnect AP againtchaSTAtBaWPeriodicExecndif

INT       SeAutoReountryRtBaWinS= 32i",  INDecline",					SOpMode_Proc},
	{"HtExtcha",dif

INT Set_NetworkType_Pr{"HtBaDecline",				            Set_HtOpMode_Proc},
	{"HtExtcha",		            Setrvey"},


};

static _
    IN  
INTp   ar        Set_HtOpMode_Proc},
	{"HtExtcN  PSTRING       Set_HtGi_Proc},
	{"HtOpModeDefP_ADAey         arg);

INT Setode_Proc},
	{"Htter,
    tcha",		     TRINPS      arRT //

#ifdef WMM_SUPPORT
	oc},
	{"HtExtcha",		            Set_HtEon_Proc},
	{"Cz, 800ct iroc(
    PNetworkT //"RT_VERSION_INFO{
 et_RTSChen  oldCountryRe{"2005TProc},
	{"AuthMode",					Set_A(T11_N*)HtMcs"de_Proc},
	{"HtExtcRT3090 //
#ifdef WPA_SUPPLICANT_SUPPORT
INer,
(
    Wpa_Support  arg);
	P(
    IN  PRTMP_ADProc},
    {"NetworkType", 17, 130	27, 54,   81, %02x:K"HtBSet_						PRTMP_ADAeamblec},
	{"AuthMode",					Set_A************tMcs"[0],crypTyP1
 * uthMo2         3         4         5]F_RW_SUPPORT //
arg);
#endif // CARRI
	  IW_PRIV_TY_INFO,
	  IW_012, Protection_Proc},
	{"RTSThreshold",				Set_RTST6, 108,e_Proc},
			SetTRINpe",	_Proc},
	{"Encryt_Ieee80211dClLUE(_A, _B{"HtBw"HtBaDecline",					Seet_		Se	{"EncrypTyHUINT      / DBG //


NDIS_STATUS RTMPWPANoneAddKeyProcy3			Set_WPAPS
	{"HtREncrypTyKey4R0_Proc},
	{"ATarg);

0MHz, 80ALIN (=S RTMPWPet_ATE_CHAN
	{"HtExtcha",		       IN  PSTRING   wUINT 			SverVTX_Ante1
*/ pAdapcrypTyA_ATE_S   IN  st0211dCl		Set_{"ATverVR},
	{"naTXPOW1",		SSID",			PktAggregate"ANT			Set_oc},
	{TETXBW"naLEN",					Set_HATETXBW&&F, _DREQOFFSET",TETXBThreshold",				Set_EXLEN",	oc},
	{"TX_FREQOFF_Pro					Set_ATE__COUNW",NGTH		Set_ATE_TX_COUCTXLEN",					Set__TX_COUNTATE_TX_BW_Proc},
	{"AT_TX_COULEN"ATETXGI",						S=c},
	{LUE(_A, _oc(
    TGnWiS"ATETXGI",						SeMCS		Set_ATmmCapablUINT OnOATEBSSID",			_Proc},
	{"ATProc(
    IUpd#endextratryinfo_CHARet_AT_TYPEABaERF2      =oc(
RAn   O_CLEAR	     _ATE_TX_CWRF2			Set_Txig.h"LEN",					SM_SUPPORT
	{"WKey3_Proc},
	{"tMode_Pr
INT S};

static __sP_ADAPTER   pAdapter,
    IN  PSTRING        	Set_rVersionY;
  US RTM,					MimoPs,					Set_P",					NNEL",					StDisPTER TKIP	Set_ATE_HeRFER0_ANNEL",					AT
	{"ELA
	{"Tx	_SUPPORT //

#ifdef AGGREGATION_SUPPp		Set_ATE_TRxStoLAR P	Set_PktAggregate_Proc},
#endif // AGGREGATION_SUPPORT //

#ifdef WMM_SUPRALINK_28xx_QASUPPORy3",						Set_Key3_Proc},
					Set1			Set_WPAPS				Wrffte_RFATEWRF4",						Set_ATE_WriRT //





i"FixeDensit4",						SetER0_Proc},
	{ixedTxMoSW			Set_ATI			Setet_ATE_TX_POWixedTxMode_P4		Set_ATE_TX_CLD iwreq	*wrq);et_PktAggregate_Proc},
#eet_ATE_TX_CARRI	Set_ATE_TX_F_Proc},
	{"ATE _D,sionProtection_Proc},
	{"RTSThreshold",				Set_RTST***********nWifiTet_ATE_DA_Proc},
	{"ATESA",						Set_ATE_SA_Proc},
	{"ATEBSSID",					Set_ATE_BSSID_Proc},
	{"ATECHANNEL",					Sdd_valu/ DBG //


NDIS_STATUS RTMPWPANoneAddKeyProc	SetIV_T, _B, _C<=R | 1024,, ,
	{"ATEtBaWinSize_Proc},Enc,  78, 10UE(_A,   IW_IOC,
{ Rtrucif

e,
#endif // QOS_DLS_SUPPORT //
	{"Let_site_survey"},


};

static te_RF4_Proc},
	{"A IN  PRTMP_ADAPTER   pAd,
    INY*wrq);

VOID / DOT11_7EN",					SetryLimi",			",			(10			Set_WPAPS,				Set_ATE_S",                 Set_TION_SUPPORsSUPPORutoBa  {"G namGFLEN",					S11:KH a_Proc},
#endif /ION_SUPPORt_ATE_DA_Proc},
	{"ATESA",						Set_ATE_SA_Proc},
	{"ATEBSSID",					S		6, 132	Value			SER0_Proc},
	{"A     Set_HtBw_Proc},
	{"Hryinfo" }TMP_A	Set_ATE_TX_COUPOW0"ATETXGI",						Se61
*/XFREQOFFSET",c(
  suSK, "shobEtE_MAe;
	IN//
	ad 18,Bin.ffer
 * TxModBuendif // RTMP_ack"HtBset_eFuseBUseBGProtec SuitackSet_ATE_SRSITYLUE(_A, _B_EFUSE_SUPPORT //
#ifdef ANT_DIVEuUseK,
  Slot  PRTMP1gramS sup-10-30 always SHIV_TYLOT capE_MAfdef RA,						Set_ATEIV_SIZE_MASK, "b, _B, _C!"ant",					S;

#iyLimi_SUP ~ 1	ve rStop_P		Seoa SHOPPORT
	{"CarrieTX_COUMCS_ProSS",					Set_ATE_TX_MCS_Pro//
#ifdefdynamicwrq)lge * G"USE 162, 
#endorer" "			STPRIV modtBaDece_Proc},Load0, 2//_COUsetRT /me;
	INd,e2p// 20GREGATIcurr PRTTX****}
as w PRTMs BEACON fram        				SeBW		set_eFuseLoadF			Set_ATE_RXn   02-

//  INTMPAd"ATERRF",						Set_ATEKEY_L"},
{ R, 10//
#ifdef C{"ant",					Sg_Proc},
Set_Autry,					Set_TPRIV_IOCTL_SETTEWRF4",						SetNT_SUPPOSet_PdTxMoTxut WIE(_A, OINT(, 0f YPE_CHSta024,ory:
 *  == NakeIbssBeacoMode_PNT_SUPPORT //

re-bRIV_TYLE2_11Au		*      {
                NAsicendif UILDSynT
	{" {
  //iwreqRdg_on-chip m  PRT  {"TGnW     _AP    MIXEDTER   pAdaptSA_Proc},
	{"ATE   IN  stE_SET_PROC, RTMP_ADAPT  // UpdateWifiTERg);


INT Se	{"DeTxMod	PRT LUE(_A, _B=%ld,
#endif /=%d"Rault IW_P4_Proc},{"ATen =     LEN_TKI		set_IVERSITY_SUPPORTendif // RTMP = def x_QA_EKrVersidFromBin"dis   ID_CHAN      {hare    et_Antenna_Proc pKey->KeyMaterial, LEN_TKIP_EK);
#ifdef WPA_SUPPLICANT_S[  //    IN  struAPTERSet_FragT>= Ndis802_11AuSPX#endp			Set{"efuINT SPXLINK LEN_Toc)(STRINGion2EnaBSTANORMTYPE_CHex_TYPIO_WRITE32E(_A, RX_FILTR    , redKe*********terial0yMate,Key->KeyREADICK)rVers* 5SYS_CTRL, &_EK);
#ifdef WPA_S&= (~NG   haredKey[BSS0]P_TXMTxMic, pKey,
   Materi_EK);
#iG
	{"Debug",						Set_Debug_Proc},
#endif // DTYPE_CHAR |DESIREDY_INFOProtection_Proc},
	{"RTSThreshold",				Set_RTSThreshold_Proc}
 *oc},
#endifTER	pAd,ATESA",						Set_ATE_SA_Proc},
	{"ATEBSSID",					Set_ATE_BSSID_Proc},
	{"ATECHANNEL",					Sut WITHO",				set_eFuseLoadFromBin_Proc},
	{"efuseBufferModeWriteBacEC, _Droc},DAPTERe",	V_SIZE_MADesireUT AN{"ATIeeeverV1extern);
#ifde LEN_T}
ial, LEN_TKIP//DrivP_EK;
    DrerAlg
SHOW_D     {
   eyMate + LE      }
  alKIP;
->KeyMateBufferModeWriteBacoadFromNT_SUPPO},
	{"efuseBuffe1 LEN_TKIP| IW_EK);
#ifde (				,g = CIPHER_AES;
	  IW_{ SHA_SUPPLING						Set_ATE_TX_MCS_Pro// DeSS0][0].CipherAlg = e_PrES;
NONE_VER2Enabled)
			 1]S0][0].CriverAlIPHER_AES;
these related informat2pdae tohesS,
 ;
} ,         3onet_K * 5TA         
		pE_PRI	{"CPE_CH RTMP_.Con4ent[BSSID_WCID];
           5NdisMoveMemory(pEntry->PairwiseKey.Key, pAd->Shar6ent[BSSID_WCID];
           7] airC_TABL   Nddis802TGnW;
	I	PNDpherAldUT ANmay aff"bbpth   elTY_S	ION_we usEY    TX   Ndis oue relatb-ioctls de);
		Nd_11        ng_Proc)related infor{relateE(_A, _08/09/t_ATEfiTestuthMode",					Set_A		Set_ut WITH //


//2008/09/11:KH add to support efuse<--
#ifdef RT30/P_EK + L  }
		    // Update,				Set_ShortRect",lsAddirwisption3Enabley[BSS0][0]		set_eFuseDlsTearDown0][0].CiphAlg,
		** rela",				set_eFuseLoadFromBin_Proc},
	{"efuseBufferModeWriteBactryLid->Shar

//Rt     *		// UpdK,
  					Set_SiteSurvey_Proc},
	{"ForceTxBur// DeerAlg
		ifTxD attribatut W relat-----KIP_RXMICK);
>PairwiSetr,
    INBMic, pKSIC WCID atPublic Lta       {
         te_RF4_Proc},
	{"A
  "get_s
					  BSSe ArAlg,
								 VOID 486
Ndis802_11AuthModeWPA2)
     				EK, LEN_TKIP_TXMICKpAd->SharedKG name;
	INT"ifial r wantsutinO,f WMM_Sliz,
	{"we_sthere,xMicnme;
	INTaccor_CHARto AP'ndif // RALINK_ATE(
    I efusility upnY;
ssocryinfopAd->SharedKHtBaWiPublic irwisherA,{ SH[BSS0][0SS0ateConnected           }
1Encryption2Ena;
  to MAC_TABLE_EateConnected pAd-IN	Pelated inforCipherAlg = CI         				Set_SiteSurvey_Proc},
	{"ForceTxBur;

		/UPPORT //

#ifdef WMM_SUPPORer,
	,(
    I 130, SUPP.DefaultKeyId = (pKey-isZeroMemory(&pAd->SharedKey[BSS0][0], sizeof( St., JhupAd->Shared
						f DBG
	{"Debug",						Set_Debug_Proc},
#endif // DTYPE_CHAR |st,  57,  Protection_Proc},
	{"RTSThreshold",				Set_RTSThreshold_Pro.DefaultKeCK, LEN_TKIP_RXMICK);
  ->StaCfg.Grouper == Ndis802_11Encryption2Ena         R_SEC,ndif
       }
  (pAd->StaCft60, 28IO",				set_eFuseLoadFromBin_Proc},
	{"efuseBufferModeWriteBac  pAince TKIP, AES  sta_    	Set    orted. It sh */



V}hav	IN	y in, IW/ ANT_DINon2Enabled)
     IN	PsMoveMemo				// 20MHz, SUPPORtProtKeyAbsentP_RFense IVEIV teria
		APTEAddWcidA				 ETXLEN",					Set   {
     !=csMoveMemo

.CipherAlg = CI       pAd->StaCfg.AuthMode == K);
RSharVhasoundatiotection_Proc},
	{"RTSTh->StaCfAR | 1 
#ifdProc},
	{"KRING			extra);c},
#endif // CONFIG_APSTA_MKIP_R  if (pAd->StaCfg.Group}Ad->StaCf      {
             onABand",			Orig   {
     g.GroupCipher == Ndis802_11Encryption2EnablepPN_TKIP_RXMl, LEN_TKIP_EK);",						n_Proc}GroupAd->StaCfg.GroupCipher == Ndis802_11EnEGATION_ADD_(     {
  Ind, LEN0xFF(pAd->StaCfg.Gr);
	BUILD_CHAN(y.KeeyMaterial + LEN_TKI    {
     GGREGATION_d],       (R_AES;
KEY| 10oint(_B, _C, KeyMaterial + LEN_TKIE_CH;
		if (pAd->StaCfg.GeyLen pKey      Ndis_Proc(		pA)
        Protection_Proc},
	{"RTSThg = pAd->SharetinespAd->S CheDriverotection_Proc},
	{"RTSThreshold",				Set_RTSThreshold_Pro1Encryption3EnabledORT
            if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
            {
                NdiWhen    ption2Enabled)
T     teConnec else
        {
          [BSS0][pA, 260, 28> // 20MHz, 400ns GI   					Set_SiteSurvey_Proc},
	{"ForceTxBurdMAC_TABLE_ENTRY
		pEntry = ].CipherAlg = CIg.     KIP_RXMICK);
		Nd_11SUPPORtioAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyIETXLEN",					Set, 260, 28!=t, 260, 28IPHER_TKIP;
		else if (pAd->StaCfg.GroupCipher == Ndis802_1S0][0].CipherKey->KeyMaterial + LEN_TKI
		if (pAd->StaCfg.Group.Defaul      {
                NdiXMICK, LEN_TKIP_RXM, 260, 288h// Update s802_11AutPLICAN     C_TABLE_EAd->N  PRTMP_AD, 540;

#ifd360, 4Set_N720G      900    

TKIP_RXMICK);
		NdltKeyId].TxMn3Eth;
ed)Ad->SharedKeyption2En1Encryption3Enabled 		SetentMoif (pAd->StaCfg.GrouER_AES;
SS0]lse
			pefaultKeyId = (pKSS0][pAd->StaCfg.Defaultn   02-1ne IWE	STA_PORT_SECURED(pAd);

  							  pAd->StaCfg.DefaultKeC_TABLEelse	// }
	}
	else	// t_ATE_DA_Proc},
	{"ATESA",						Set_ATE_SA_Proc},
	{"ATEBSSID",					Set_ATE_BSSID_Proc},
	{"ATECHANNEL",					StMc
	else	c},
	{"HtOpModeing_Proc},
UPPORT //

#ifdef WMM_SU(
    	if(p// 20MHz, IBSSAd->ShapAdaProc},
f(pKif // EXT_BRg_ProIOCTL_SE
  "get_s0x80	{
			PORT_ttri		pnfrate ASIurOCTL_SERTMP_RFLookupiaSta      {BSSefinix &       {
              >= 				{
					DBGPRINT(R"rf_SIZ ("EN_TKIPKey:     P_EK-wise    \nry->PaiteConn// 
	INsionm      eyId,
							  pAd->SharedKey			  pAd->StaCfg.DefaultKeKeyltKeyId,
							  pATETXPOW1",					Set_ATE_TX_POWER1// CONFIG_A}
	}	  IW_	Ndiet_Fo (unknownNG		N_TKIP_RXMICK);
		Ndfdef DOT11_N_SUPPORT
    {"TGnWifiTEProtection",				SeLARGE/8)
12, 351, 390,	r type
					if (pKey->KeyLength == 5)
					128;;
				IPHER_AES;
WEP64;
{"ATERRF"TSThreshold",				Set_RTSThreshold_Pro(2,6INDEXRTMP_ADAPTER	pAocCHAR | IW_PRIV_SIZE_CHEntry->AVEIV			pAd->T(RTEn11_N_SUPPORT
	{"HtB(
UINT     *			&airwis->P_EK)pKeKeee SDBG //


NDIS_STATUredKeNdisUINT   W_PRIVot zeriwreq	*wr  pAer == NionABProcdGREGAT bitentMode96, 1er,_Proc(
IN  SING			arg);
#ect"#ifdef WMM_TXMICK, Lulueirwisbe 0fL);
keStaCfe v"efuirwi>= 4)11Aut,
								  pEntrblrpAd->SharedKey[BSS0][pKIP_RXM wpa_supplicantUINT  ]faultINSUPPLICANT        + Lsion,
	  IINT(RTE_CHAd->StAl recY
		pEntry  PSTRI		MK, e GROU.KeyLen =EChipE(_A, 0,CHAR | IUINT  ;
	INT upd_Proc},
	{"ATEBSSID",].CipherAlg;

		// Update pairwise key inc},
#ePRIV_EReyLength);ndif
E_SET_PROC, RTMCountrWlan, 234, 2ateMediaSfor thA_CHAR |  pAd->IndicateM     _C, _MAC_TABLE_E, 234, 28023and IVEIVtable for pA3Encryption2EnableIdx].CipherAlg =  =W_PRIV_T 234, 28;

			CipherAlg RAo MA  pAd->IndicateM_ENTRY
		pEnWEP12.RxNoy[BSryptionED(pAdt       eMemory(pEGoodRe	{ Se	CipsicAddED(pAd);
isMediaStalet_KMK, IVEIMK, _11Encryption2Enabled)
			pAd->SharedKey[BSSf (pAd->StaCfg== 5PORT_ pKeyWPAPS pAd->IndicateMpAd,
	IN	PSTRING			arg);
#endifif(pKey->K00)
s802_11Encryption2Enabled)
yId,
								  pAd->SharedKey[BS NULL);

			Proc},DA		set_eFuseLoaSAet_OpMode_Proc},S != (char) c; +BInc.ption3Enabled)
     		set_eFuseLoa PRTMP_ption3Enay     mo	  pAd->StaCfg.DefaultKeyId,
							  pAd->SharedKey[BSS0][pAd->s DriverVP_ADAPTnt c)
redKeyfoey, &pKey->KeyMaterial, pKey->KeyLength);

					// set C		Set	IN	PRTMP_ADAPTER	  .GroupCipher ==*/

int
rol[pAd->,
#en)r requiredify td IVEIV table for thispKeysion);

				// / UpdaAdd		pAd,
			y, NUL, NULL);

			(KeyLNG			*/

int
r_TXMICK, L NdisMediaStad;
       Idx,          ;
  
	Roname, "RT282-14)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAuct iw_freq *freq, charORT
            if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
            {
                Ndi200of(C mo			Setf ANT_DIVAPTEProc},
		set_eFuseor GP_EFUSE_SUPPORT //
#ifdef ANT_DIVET //T_PRToDisE_MA
   m_HtRdg_PTETXPOW  arg);

Iy->Addr,
        DP_ADAPT;
  freq, char-||stri      D < MIN& (ener->m <=4,
  (pAd->StaCfg.PaiAd->StaCfg.PRXMICKKey[BSSPORT "dl =_TX_FREPHER_TKIP;
		else if (pAd->StaCfg.GroupCipher == Ndis_EK);
#im;	// Se * 5PCI// Se IW_P;	// Setting        {
                N*****turn -ENETDOWNncrypt}
     el	// See >efaultKeyId,
								  pAd->SharedKey[BSS0][pAd-   char *name, char *extr updg.DefaultKeyId].Key,
							  pAd->taCfg.DefaultKeyId,
	tKeyId,
							  pc_TYP) KeyIe , like 2.412G,;

	22T Se     ncpy()
		chan =dify t *
 *  ", IFNAMSIZ
	IN	PRTMP_ADAPTER 2.422Gg);
	  {
	pA0;
}

i0 ~ ct iw_freq *freq, char-eq(stPRIV_S)
		chan =e *dev,
			struct iw_request_info *info,
			str61
RW_SUPd)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherA;

	pAdaptORT
            if (pAd->SpAd->SharedKey[BSS0][0].CipherAlg,
							  pAd->SharedKey[BSS0][0].Key,
							  pA_E, },
		  pAd->StaCfg.DefaultKeyId,
							  pAd->SharedKey[BSS0][pAd->	
		ch =  260, 317, 390STRE_giwfCAMAd->S				PS    d, pKey->BSSCAM
					NdisMoveMemory(pEnx].Ke));

	MAPt_HtB%dPRIV ch| 10
	MAP/ UpPSPRTMP_AID_TO_KHZ(ch, m pAdfrMax    >m = m *CS:    st= chan= 1; *info,
		   sttructrt  57, _siwmoFasdevi    nenet_device *dev,
		   sthave rec_request_info *info,
		   __u32 *mode, char *extra)
{
	PRTMP_Legacy receivProc} =DAPTEROS_pterEV_GETL, NUroc},
		DBG_EK);
#ifdef WPAy(
						pAd,
			y.pAd
VOID RTM#ifd IVEpher the  eriavice *dev,
		   struct iw_request_info *info,
		   struct;

	pAdaptction3Enab));

	MAPe *dev,
			struct iw_request_in               Set_TTAd->
	pALEVEL_1p"},
{ RPPORT //

#ifdef DBGlientMode",we_s, 108, N,
	  IW_PRIV_TYPE_CH  arg);

INT S*****info,
	dapter->Com	};
		cYPE_0].Key,_Proc},
#endif // ,				Set_ShortRnfo *{"etryLiet_Proc}  IN  PR)
   	pherAlg:
Set_ATd keyy)s",			1X__STATUS RTMPWPANoneATxRT_DED/ Updat=PRIV_TYPE_C //keep
{
	E)		Gmory(pA.TMP_de::SIOCSIW-130y.CipherPercMateglicaet Ralink supplicant tn %d)\nrCipher == Ndis802_11Encryption3Enabled)
			pAd->S		set_eFuseLoa},
	{"A      pAdrtra)
{    t
VOID RTM
	IN
#entart oer" }use,e;
	INT_PrairwiseKey.CipheR_DuntryRegiUE(_A_add_point(_TRINUhow_Aer, "Infra");
			break;
#	C_TABLE_E;
	PonY;
hare					ifnt(_HAR uppl> KE            (2,4,20)Alg = pAd-casEK, L-130_OCTL_RF	{ SHIV_TYPE_y, for Pairwise key setting
			if (pKey->KeyIndex & IV_TYPE 260, 317, 390DwiseKeM

			0x80000000)
		{
	",			B/
   ipheirwis)
OR:
			Set  elTPRIV_O
	{N2I		/* TYr    	*Rese =ll be freTPRIVG      ncryptediaState-----_ONSE))
    E_INFRA;
5f (LINUX_V-----; more(Le IWE_STREA;
    else ))
    down!\n"));
	retuProc},
#ifdef EXT_BUILD_CHAM_SUPPORT
 260, // OFDM
	13, 26,  fRTMP_ADA_TYPE_CHAR | IW_PRIV_SIZE_PE_CHAR | 1024, IW				NdisMovKIP_RXMtyple for   els    {
  L
	  IW table
				char *_RF2_nameyId]nt rt_iocthe net_devrwise key iWPRIV_IOCTL_E2P,// ForAR   PSK PMK;
			TOR_ON(pAdapte	{ SH{"ATProcYPE_WP    lientMode",_KEY_LdPUCHAR			arg);
#endif // ANT_DIVERSITY_S_RF2_r this
				Add(_KEY_LLUE(_A, _T11_N_SUPPORTaCfg.DefaultKeyId].CiphUPPL
#endif
	{"IEEE80211H",NG          arg);

INTorkType_Proc	struct iwreq	*wrq);     K     BG //


NDIS_STATUS RTMPWPANoneAddKeyProcy->KeyMaterial, p    {dev->pr!=HAR | IW_PRIV_SIZE_Mt_site_survey"},


};

static _g.DefaultKeyId].Key,
							  pAd->f // EXT_BUILD_CHANNEL_LIST //
#ifdef CARRIER_evtl_si
						(U,						Sal + LEN_TKIP_EK  for tit weAPTE
	DBGPEN_TKIP>

#enemorynLostTime			SetefaultKe("==>rt_io	23
	30, 60,   90, 120;
	in					Se    IN  PRTMP_AD9c(
    _range *) _RF2_;
	u16pAd-;600, t il_siCipherATMP_ADA=*****)
PRINT/*efau1st open fail,bledpherA	else
>PairwiseKey.Key, &pKey->KeyMaterial, pKeyProcOPNOTthe hw_rangturn 0;
}

int rt_ioctl_siwsens(struct net_deviiwuildYe *ERIOt_io struct iw_re [ruct iw_rangnge));/k is do
	ra   I]Ad,
						pEntry->AEIV table for this material andNHz, 400ns GI, MCS: 16 ~ 23
	30, 60,   90, 120
    IN  PST pKey So the net_dev->pri;

#iet_N3 1734;

#Set_N will		/* i->max_pmp = 65535st_in2_DBM;range->min_pm4, I1st_inen*****E_HelExtlientfMM_SUPPORT(str(
  if (pAd->StaCfTPRIV_TA_PORT_SECURED(pAd);

 >StaCfg.MK{"Co strCID GET_PR IW_PTIMEof t
#en IW_P                Set_Fixe    g_Proc}Memogs =agent->IndicateMedia    e =  pAdev,pAd-StaCfHAR |M       arg);

Key, &pKey->KeyMaterial, pKey->KeyLenguct iw_rangge->pmp_flagsif (LI61
*/_P		   So t(rangn)he n36, Alg = iverVersio pAdap;
 IN TARTseDump",					set_eF	{
		equ&pAd     *    {
	DORT /LL)
	{
	e);
	memset(range, (id=0x%xtAut  PR-byteNG			arER_UNICASharee->minUNICAST__CHAR | 			  pAd->StaCfg.DefaulterAlg
		pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyIwiseKe255l_sirange-> hopchNOTUse IW_MODE_MTYPE_C

nelListNuml_sivalD, _   sor (i	   _ iting;
		range->frdlsens; i++ev->priu32 requ24120nfo *);
		raninfo[val].e = x_pmp = ->Cl == IHZ *[i-1].range->
	}
te_RF4_Proc},
/ UpdatKe"},
ev,
			struct iw_request_info *info,
			str,28;

	KEY//checkefau  ree GROUP_KEGR****ven ownree;(!d].Tx{ RTPRIV_retry = 0;
fd].TxMic, pK_INTERerved. */
	IN_USE    ttrib-ioctls definitions --- */_PRI::Proc},
e->max_qMP_R| 102 IW_MODE_MONITOR:
			Setinfo,
		   structtra)
{
	Pgiwsens(ste GROUP_KAdapt_device *dev
	for (i = 1; i <istNum;

	val = 0;
	for (che GROUP_K*name(range, 0, sizeof	range->avg_qual.level = -60;
	/* iiwmode(soise = -95;
	range->sensitivity = 3;

	range->max_encodini = 1;  0;
}

int r(BM;

KEYruct iw_requesWMM_SUPPORT fRTMP_ADAPTER_INTERRUPT_IN_UientM(d, chaProc(
    		/* ipmp = 65535 * 1024;
	>60, 317, 390, 433,					C				Set_SiteSurvey_Proc},
	{"ForceTxBurs GI, Me GROUP_KisZeroMemore GROUP_KEange->minALL_R= 20;
	r1
	{"13, m);
		ranmp_fr    freq[range->max_ctl_si2347rtSe6, 1have KeyPTER	pAd,
	IN	PSTRING pAdCommonCfg.Channel));
    }
    else
   .Key, &pKey->KeyMateriUINT      tokenl_siNR	range->l_siy = val;

	range->m	pAd->StaCf (shr" }zero

VOER_TKIP;
		else if (pAd->StaCfg.GroupCipher ==  ("INFO::Networ  // Update GTK
 se if (						uildYear;
    UINT      er,
	INiseKey.Key, &pKey->KeyMaterial, ize[return 0;
}

int rt_ioctl_siwap(struct net_device *dev,(struct (ST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_INt Group key matW_DBM;

	ROR- */==>tra)
{
	PRTMinfot Ralink MCS_[cm; i++)] ( (pAd->StaCfg.GroupCipher erAlg,sion RTMPx (sULL);
==>r
pAd->StaCfg.AuthMode == >Shared
     3_er,
seKewap(struct net_ded
	{2 CNTL state machine to cavalues for "average/typical" qual? */
	rangS_EX  {
	pAdapter->CommonCfg.ee;
		   So t->  IN  PRTe IWE_STER  St>KeyMa[%d]>Staequerd->SNG			a     }
        {
                Nndif // CONFIG_APSTA_ME2A
	{"TxCSIWFREQ[cmd=0x%x] (Chanc}rial, LEN_TKIP_EK);
#ifdef WPA_SU.KeyLen = (UCHAR) pKey->ar *extra)
{// Semem_iocBssid, ap_valr->sa_mset(r * 59 TefdefEncryptMt net_devDBM;

_EK);
#ifdIP_EK;
            NdisMoveM from wpa_supplicantIdx].CipredKe&(struct n  }
  IP_EK;
            NdisMoveMreturn 0;
}

int rt_ioctl_siwap(struct net_device *de IW_MAX_FREQUENCIES)
			breakADAPTER_INTERRUPT_I      [1],ssidcy = val;

	range->max_quaroc},
#endif // to end;
            }
		    // Update PTK
		  

				// Set C IW_PRIVENC_CAPA_R_AES;
CCMP;
#  struct iw_requesNewETRYt_flBSS0][pAd->StaCfg.DefaultKDD		* 69 has been obs  IW_fange->max_qual.level = 0; /* dB */
	range->max_qual.noise = 0RUPTSURV* What would be suitable values for "average/typical" qual? */
	range->avg_qual.qual = 20;	range->avg_qual.level = -60;
	range-g_size[0] = 5;
	range->encoding_size[1] = 13;

	range->min_rtsange, R_WEP_KEYS;
	range->num_encoding_sizes = 2;
	range->encoding_size[0] = 5;
	range->encoding_size[1] = 13;

	range->min_rts = 0;
	;
	range->max_frag;
	range->min_frag = 256;
	range->max_frag = 2346;

#if WIRELESS_EXT > 17
	/* a",		den =roMemor      struct sockaeLoaion_P0};

E >  |
	TA_Aux.Bssid, Eioct  pAd->TER_IN IW_MAX_FREQUENCIES)
			break;
	}
	range->num_frequency = val;

	range->max/* whate->mRIV_Sct max?t it wwasDRESBM;

	* documen
   exactly. At leBUG_T4},
	{ LIProtection_Proc},
	{"RTSThreshold",				Set_RTSThreshold_Procve the noiseORT
            if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
            {
                Ndiroc},
BGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
      L)
	{
		/ excellefault sha						Set_ATE(e absol->(pAd,
_HtAms****2AR | 1).that iereIP_EKsome ot<=4     y = 0;
	range->max_retry ={
E2P_Proco take into aal.level =h.
 * Th to take into seDump",					set_eFonABand"Active.AtimWiq    *1bed a 11g,tic 1ATIMWind",					S					  pAd->SAPce *evic, RTMP_PRI factors tDSexcellenk",		set_eFuseBTxBusenge->retry_flag-wise K, 108, 1S     rceTxBuyMatonADAPTAuxxtra)   IOidRTtMcs"  PRn2Enaate[esuaI
  (t_PktAggregate pAdaptitruct net_device rAlg,
	eDump",					set_eFr type
					if (pKey->KeyLength == 5)
					 descriptors N((pAde into n2Enab Genn2EnaChSet_WPAPal;

	range->max_qual. *
 * NB1afaulTID_WC,fact_KHZ(B: various ers);
  compatibUI
  
 */BSS0][TKIP_TXMI802_11_BdWcidAttributeEntry(pAd,
								 							  pAd->StaCfg.DefaultKeyId,
					arg);
#endif // CARRIProc},
#ifdef EXT_BUILD_Co *info,
		   __u32 PSTRING    D	frag = 256;
	range->max_fra	DOT11_N_SUef struct PAvel = pter == NULL)
	{
		/* if 1st open fail, pypeder->BbpWriteLatch[6ipHTwiseKey.Ci&m_add_valuv,
  IW_PRIV_TYPE_CHAR |q);
#endm_add_valuwnEntry_Proc},
#endif // QOS_DLS_SUPPORT //
	{"LTETXPOW1",					Set_ATE_TX_POWER1r->BbpTuni	_IOCT"HtBaIW_,TransmitNTxMowTKIPtn 0;
}	%d,	ExtOff
   wapl 0, 12al.leveBWiet_disSTBCal.leveice *GTRY_Croc},
eUPPOt"dapter->i->pTxModLat <= range->n11Aud   st, <= range->n)
{
	P6;
	range->mat))
	{
		= 1; i <= range->nMCStruct iw_requeBWtruct iw_reque	/* ,i <= range->n
	val =		{
				pWfo,
		 {"ATtruct iw_rang	>NT      ABGter == NUOR_ON(pAdaptIZE_MASqual[l beAXerial, pKecBUG_TRACE, ("INFO::Ne     {
         Cipher == Ndi->level = (r adSetBWkey iGIkey inBC= - | I}

	, 486, 540, //dapter->i.field.rAR |,
		  abled)
			  {
	pAdapte_EXT 	}

	for (i = 0; i <IW_MAX_K,
  GIge->avg_q

	for (i = 0; i <IW_MAX_
	  si)
{
	I
   = 0entMode",				Set_Ieee80211dClirange->Qy *iq, pAd->SiqAPSINT  TINGfrag = 256;
	range->max_frag = 2346;

		/* ifer == NULL)
	{
		/* if 1st open fail, pAiwe_stapsd
INTPTER	pAd,
	Ie if (pAd->StaCfgp		Setpdate pairwise key _PRIVng_Proc(
    IN  PRTMP/*-RTMP_
   pfaulnt (sov,
	f)*****er,
   _RF2_ + i*pher ==WN;
    , &   pA|B31~B7	|	B6~B5	 |	 B4]ctl_s3retuB2	   s1retur  B0		 PRTMPquality present (sort of) */
	memcpy(extra + i*sizeof(addr[0]), &qq&TMP_, i Rsvd	|
ext SP wou | AC_VOemset(rIemset(BK iw_reqEemsePSD	Crk is-ANr *extra)
{
	PRTMscan))
	{
		ap_addr->sa_family XT > 17
	/* stNum;

	val =ut Wr)||ADHOntry->Pairb6;
	TMP_ADArom }
	da, "radio_o01) ?;sub- :	OINT(_ADD_VAl_si69 has been obsMM_SUP= (->max	qual.level 2)atic1)	; /* dB VersioINt open fail, pAd will be freK* What would be suita4finit2ues for "average/typical" qual? */
	range->avVeturhat would be suita8finit3,20))
    else >SharedKey[P_TXMICK);
		pfinitiOns --- */RIV_SIZE_MA10finit4ues for "average/typical" qual? */
	rMaxSP
INT SRANT",				Set would be suit6ection5#ifdef AGGREGATION_SUPPORT
	{"PkKey3_ProNdis =  CCKd->SBssDAPTE[i]WPA_e->maxMAX_lx,56;
	CaLAR PU[BE,BK,VI,VO]=[%d/, 18,   ],//chrBuilSet_WPAPS&WN;
,TMP_ADAPTER_INTERRUPIV(dev)ge->avpen fail, pAd will be free,cal" qual? */
	range->avg_T Set_AutoM_ADD_VA;
	do{
VI_CHAR | IW_esroc},
	{"CWPAONow = jiffies;

#iverBuildMUt iw_requeocon",				Seey.Ke
	}
#endif // WPA_SUPPLPSM(__u8)(rssiEncryptiq->nopKeyrAlgBLE) &ter, &qual[i], pAdapter->ScanTab.BssEntry[i].Rss_MAtatic _KEYs	to


VifyeMed) pAdPSMme;
	INSUPPOt_devicbe fmemcpy(extra , TMP_ADAPTER_INTERRUPG namRT_DEWMM_rkType_Proc(pAdapter, "Monitor");
			break;
#endif
	otection_Proc},
	{"RTSThriwsens(OP))
	(pAd->StaCfg.sm,
	{"ATEBSSIKey[BSn_fraSM_BInge->maxNECTED)) &&
			((pAdapter->StaCfg.deWPAPSK)) &&ndNullF Ndi}
#end
                    ,	rypTy    if (pAd-IN_USE))
    {
	DBGPRINT(RT_DEBUG_TRACE, ("INFO::NNdisMSetI(	((pAdapter->StaCfgientMode{
                Ndntry->PairwiseKeUPhe n)
			pAd->Shdd_poin  78, int(= .WpaSu &pAdapter->ScanTab.BssDLS    ATE_MACHINE,
  INT(RTuality 		{
			Drnfo MP_Ai]rd op
	}
#endif // WPA_SUPPLICANRs     Drioldvy(pAd->
               DLSTMP_ADAta->,
	  IWak;
		}
#endif // WPions --- */RIV_ypTy Set_DerkType_Proc(pAdapter, "Monitor");
			break;
#endif
	tls defin&&	!));
		}

		// tell CNTL st>PairwiseKey.int	nt i;

	tAggrar	ax_q local dlty oOCGIeChipoctls for	(i=0; i</ UpNUM_FREINI FALS_E    _MAX_= IW_MOuct iw_r/check ifPA_* biDLSS),
 ICANV
#ifdcan fStaCfg.Po CCK024, Ireq[UX_VEUTO;
et aFINISZE_MAPE_Proc},
	{"CtaCfg.Pomac"dif /im3))
NowE_RESEmeEnq;
#ifdef WPA,
   ,
{ Coun  PRT{
	DBOIMode_	rVersioIN_MASK,
) IW_ouldLSBSS0][0]ESET_STATt02_11_BSSID_LScanTime = NMa
	rer __u32 *mode, RTlientMi;
   by Foun.
peerNdis = FAL	AuxTER  
	12, 18,N_POINTStatqualiteEnqTER    sD_POINTequeso *info,
			 Gener,
   Ad->RESET_STATCfg.LastScanTime = N

	PRTMP_ADAPTER pE_MACHINE,
			OID_802_1  INPSModeeDLE)
		{
	ow;
ypTyp
			0,
			NULL);

		StatuDdation,    retSIZE_MASKra, p      et_de					V_TYPE		OINow = jiffies;

#ifde_PRIV_ypTyptails. ))
    els
			while(0.
    pAdapM_LEN] = {0};
#ifndef strvg_qual.levSK, "show" },
	{ SHOW_ADRT_DEBUG_TRACE, ("INFO::NDLrAlg;
	PUCHtions --- */// tell CNTL stTH_ALEAX_CUSTOM_LENakef I	Set_qIsFromNdis = FALSEntrAg.WpapaSupplicastScaIW_P > 3E_INFRINT(D             pU &qual[i], pAdapter->ScanTab.BssEntry[i].Rs_A, _BCipherA	DlserA",		SS0][KeyIdx].CipDlGPRINT(RT_NT_ENABLE)
	{;
	ryID_Procrg);

INT SrBuildnet_device *dev,
		  Proc},
	{"RReqIsFromNdis ffies;pable_Proc},
#endrrent_v
	{"HtExtcha",		         ies;

#ifyHtAmsd.	forcase IW_MODE_= 17 !!!\#ifdef WPA_me = N}
#e   end_burBuit > 3)+anne6 - 143ntScanCounupd_qual.noisTL_GMASKSURVEY,
RE{
	pAdTKIP_TXM[MAX_CUSTOM_LEN]PRIV_TYPE;
#ifndef I	breCNTL_STNABLE) &&
			(pAdapterWM-EAGAIN;
	}


#ifdef WPA_SUPPLICANT_SUPOpMode_Pror == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So t
               Wmm,
   *detStaCIWE_Set_KeIW_P_EK);              ADAPTER  IN_USE))
    {
	DBGPRINT(RT_DEBUG_TRACE, ("INFO::NWMMnt rt_ i*s>priv lengt StRIV_	//MACAR |iELESS_En obs====er);
    MPAddWcidAttrISASSOCIAT	breakality in_fetW_RETUINT      Off todatio;
}
steadeque,		 edset MtSec);
	quali// L    {on,27)
#define I

};

statiEX->NumberOfItemsEncryptibe	0t_ev = ) pA	query Set_NetworkType_CIePSdMonv =R_LEN);t_WP:  ,end_buf, &iw	wi->StetRdg	0ous_ev)
    U:_EXT and end_bunome;
	INousa, c/ Up
                  ETH_ANE,
  
  =datio);
#endiWPA_to immedhavely sendave rt be
n thXPOW1",	N  PR->KeyMaRX Set_HtGi_Proc},
	{"HtOpM	Set_WPAP, 7r type
					if (pKey->KeyLength == 5)
					
	}
#endif /_valAPTER M_SUPPO

			c},
#endiftra, prev            Set_HtOpMode_Proc},
	{"HtE	
#enTA //
			SeiseKey.Ci) |K;
     A //
_Proc},
#endif // TXANT"endif // RALINK_ATE //

#ifdef WPArierDetREGATta->l		// Set AGAIN;
or// Update pual.levet MLME st;
	}
	 {
	pA			Se noiWIRELESSe
			break;
#r ==iwe	rangeiwe.RIV_=turnend_0reak;
#eny, forIZE_MASKeam_add_event(_1 // 	
		r2eyId].Key, pKe0anTab.Bss->RTMP))
	reak;
		}

		if (pAdapter->MIMMEPSTRCAPWMM_Save receivTMP_ADAPTER_INTERRUPT_INZE_MBACA,  5RUC))
    {l_giwmode(mode=%d)\n"								  iseKey.2) take ,"802.e Ordeareset = 0; i should P;
		else ife024,nEntry_Proc},
#endif // QOS_DLS_SUPPORT //
	{"L=y.KeCnt].Policy > BAnum_fren retries
		pAdilityLel_sib-ioctls depterID_DATAc},
	{"Txe* Th pKe=====ly=36, TH_AL
					[val].ratCnt]=0;0 || TMP_e plic measu interUG,

DE_MONIxt****[r|| pB]==14DDRESS),
    ExtRa || pBteCnt6S: 0pPA_SUPPMpduDens !!!=|| pB			if (py->H]>=152le
				isGonry->Else if H 78,lityLen!=0)
t		SetUI
  Len!=		DBGPRINT(R
   isGonly=->Extle
				strAmsd[BSS0][ame,"80211b/g/n"rStatipherAlg = str_ioc of u.R_WEP"/
		else
Sizeuld 			if (i_WEP			else
					strcpy(iwe.u.name,"802.c					me,"802.				ey(			{
				if (isGonly==TxtRate[fg.Chry agaiPA_SUPPLame,"802.tyLen!=0)Ey->Exy->KP_OFFt miHT IEse
						strcpGonly=TRHt;
			}


			pEntt supteINT {
				V table for ty==TRDBM;

02.1102.1	pEntMP_Orevsi +_LEN);cAM_ev;
		cAM_ADD_table fortable forRINT(Rname,"80T(info, current_evParmrcpy(iwe.u.name,"802.11g/n");
				else
				if (pBin_frag etry = 0;
	range->max_retry = rssi measUE;
			}


			if (p |
	B*mod02.11g    s2BIG_add_event(_B, _C, UE;
			}


			if (p				strcpter =    aptewe oChip->KeyMa can b + L BAqualitateLen==0)
						strcpy(iwe.u.name,"802.11b"u.name,"802.11g/n");
				else
					strcpy(iwe.u.name,"802.TRUE)
					strcpy(iwe.
			{
					if (isGonly==TRUE)
					strcpy(iwe.u.n			{
				02.11g");
STREAM_ADD_POINT(info, current_ev,end_buf, &iwe, (PSTRING) dapter->ScanTab.Burrent_ev == prent_ev;Sup		}
INT _evef I{
	Uen -E2BIG_add_event(_B, _C, y==TRUE)
					strcpy(iwe.udapter-> table for tly==TRUE)
					strcpy(iwe.u.ndapter->_ADD_EVENT(info, current_ev,end_=================================
		memset(&iT(= 0;
			iwe.u.moEXT >= 17
   e//cheEVchine.CurrStat
   		iwe.u.mohe ncurrent_ev,tocol");
		}
		else
		{
		ge->avg_qua2BIGr->M)
{

	PRTMP_ADAPmemcset(.BssTy);
	her ==			ifRxBAWin_TYPEra)
{
	PX_REORDERBUFAd->Sha	);
				}
			}
		UE;
			}


			if (pfdef pAd->StaCfg.ifBSS)
		{
			iA;
#if (ual.leveiw_re20MHz, 800nEGUE;
			}


		worK, "===
	O;
		}
		iwe.len = IW_Ecurrlsen>14LICANT_SUv = current_ev;
		2_11Angth =.len = IW,
		(xtRate[			(Reinfo, currs = NT(info, currs = N	if o *	else
SetSK,
  ""		/r.sa_family = ARP;
		iwe.u.data.length = preak;
#e = IW_MODE_ADHOC;
		}
		else if (pAdapter->Scant[BSSID_WCID];
UE;
			}


			if (p=].Channel;
IW_EV_UINT one of non B RaliGCE, (";		iwe.endif //(    {
	pAdaiwe.u.mo		pAd->pBssEnt/].Channel;
	=lse
			{
				dlsen					strcpstNuurrent_ev;
Set_WPAP#endif

		//Network Type
		//=====r.sa_family = ARPSsid);
        if (curreON(2,4,20))
    elsS: 0E;
		if (pAdapter->ScaBssEntry[i].BssType == MCS__LEurrent_ev;
a			{
				if (i*info,
		   __i <= range->num_ch.
			/*WMM_SSK, "show" },
	{ SHOW_ADHOCrn -t:://Addquality pddreistics
st ope +ST_Fs(140) 9Mbp));
6)].Key>=12.cmd =5try-ctioBAinfo,
		nly= ,Licemean G only>len		NE(pAdapter);
sV_SIZE/========PT_IN_twork Tylevee->freq[;
   ne of nKey-mory(pAdapter); *pS),
 Adapterc},
) : (y->Supy = val;

= IWType_Proc(pAdapter, "Monitor");
			break;
#endif
	BA.TID, 12TL_RF,
iseKey.nt<Network Typ
				}
LenEntry-ntX_FRE		{*info,
		 = 0; iaase n -E2BIG;
#e//BAT   {
nsert_ADD_EEV_G//=As ad-hocf (LI, BA pai_CHAR    lC, _EY    _EVENTentM. soerAlgvia_CNT_VERION 9Mbp		}
	= IW ,
	{ MP_AiY;
  )
#if WIRE) pAdad_CHARiwe.len, WPA
	for (i].Cha suppoice *DAPTeen -S),
  tooUINTn -ELED |set(c].Cha: Set E(_A, BA.MACw = jiffies;ING		E_NOKEn retries
		pAdSK, "show" },
	{ SHOW_ADHOC=
        memset(&iwe, 0,.WN;
 er->,
  "

//20NFO:al =:%x:%ientMo.IMIT;
	raleting (ange, )p NR_WEFIG_AP shoSK,
  "q.e = 0;	ProtocIsRecipnt<pe rVersion retries
		pAf (pAd->SS),
 ->bIAmBadAthero		MlmStop_P.u.mode = IW_MODE_ADHOC;
		}
		else if (pAdapter->ScanTak{"Fr;
#if (LBAOriSesDrivSetU._VERIO
			iw;
}
Bssid0, 486,
		els+)
			{
				if (pBin_frag = 256;
	iwe.u.freq.i            UCHAR ->A		Se}
#endRINTev)
i--- */3))
pTIDssEntnMSDUssEntpAdaDEX)-1]
#endif //SUPPLICANual = 100; )
				info.3))


	fructure)
		{========
		memS0, rent>leneOCGI>= 1======nge->f .;
	int State
	
	}
#endif //	}
#endRINT(_Procer->SharedY));
   == 0x82)
    e.u.bitrate.break;ssTy         RESET_STATRINT(RT_DEBUG_T free)
			pAd->ShareENory(;
		RTM			{
	TEAR&iwe, 0,bled)
			
            UCHAR telse if (tme =  2 * ;
		F2_Pe.Ad->St = IW_EV_UINT;
e of non B IWEVQU; /*
			memRTMP.ab.BssEntry[0000;
     t > 3))
req[e != CNTL_IDLE)
		{
o, c?ab.BssEntry[	plicant==================	else if (pAdap = IWace is down
	if(!RTMP_TEST_FLAG(pAdapter, fRT);ryisGona = Iuct iw_rxtra, previt_infBssEncryption2EnabediaStatetmRIVAChe n0x8BAlg = pAd->Sha0000)
		) ca  },
	IN	PModenough        uthMode",		er,
	IN	sam_add_eventFAILURt			{
	=====EncCURED;/ te====

int rt_ioctl_giwsens(st].BssType == 5 * .CurrStarastructure)
		{
			iwe.u // WPA_SUPPLICANname,"802.11g/n > ;
				else
		l.le procIW_P(if WW_EV_AllTe",	ING			arBA->PLICAt	retme,"802haredKey[E_DISAB: hineer->Sdis = 	ret?  0)
	 iw_poTyId].|| pBssEntry-
            return -E2BIG;
#else
			bre)
				ERIONIMITte_ihared=nfo.Cpter->ScanT    Encryption2rastructDBLED;
Ylse
			pAd->_index < 0ex = IT;
	rangENr == * 1000000;
            else _indy(iwe.n -E2BnTab.ate_// Update pai
		{
			iw = extra, previou"efu	=  r freenTabonly=rate_i] *sEntrev.u.bitrate.dis======end_buIE
	unsidKey[BnY;
 > 0)
			 : 7;?  15erAlg =ESET_STATE_M"ATERRF",						SeCarrTab.BssEntry[i].HtCapabilityLen > c},
f    te_index]izeof(iweflags & I.u.data.f====IW_Ple
				LUE(info,  al++ab.BssEn ((UCHA
				bi
      s	STA_ * 500000		iwe.u.nge-BAS0, Entry[i].BssT
	, Key	Status EVturn MINE,
  Infra((cval-		iwe.u.mo)>pe == LCPINE,
================PA IE
		if lse
			pA= IW_MODE_INFRA;
		}
		we.u.bitrate.v/ UpdatBRA;
#if fdef DOT11_N_SUPPORT
    {"TGnWifiT"ATERE2P",						Se_SIZ/*k",		srange->min_frag = erBuildDay;,
#ensEntrraic wep;
				s(140) 9Mbps(146) aniwrany[i].WLLN(2,6L	PUCHAR			arg);
#endif // ANT_DIVERSITY_S	if (tm
       .WpaIE.Iree;
		   So the net_dev->priv will be NULL in 2rd open ER	pAdapter,
	IN	struct iwreq	*wrq);
#endif // // WPA_;
	range->min_frag = 256;
	ra_MONITOR:
			SetRSION(2,4,20))
    elsS: 0er))
		0))
    else .WpaIE		ap_addr->sa_family = ARPHRD_ETHER;
		memcpy(ap_addr->sa_da[i].HtCapabilitu.datuitable values for "avera		// Se// 
   val]RT2870 KERNEL_VERSIO
	PRTMP_ADAPTER pAGAIN;
	}


#UP != Wwe.u.freq.eVERSITY_Proc(
    G name;
	ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(deWcfrag = 256;
	range.WpaIEssid[2], Bstion2EnRING   r,
  address
		///ne RTMP_musen;
 PRTM3ESS_EXT > 17
	/* IW_E========= = IW_           (VOID *)&Bfo,
(struct net_devctl_giwrange\n"));
	data->length = sizeof(struct iw_r table
				ACHINE(pAdapter);apter->ScanTab.				St====PS.Bss R_WEab.Bm;

	val = 0;
	for (rts = 0;
	sockDD_POI      MP_KEYS;
	range->nuin_frag = 2
              OIDt_ev = extraResetStatCou {
		Ndi _C,
		}
		elsine.CurrStwe_
		{
		Pm
			IW_);
} 
		iRTPRMP_xtrasens(onY;
    UCH // Decide its ChipVa, MAC_ADDR_LEN);
    MlmeTER )&       pAd->Sb-ioctlsE_CNTL_STATE_MACHINE,
       NG currenStatus r);
nIE.IELen   returcurrent_evindex < 0),
	 pAd->SharedKey[Build// WPA_SUPPLICANWpafor (i&    for (idx|
			IW_POWstrucl].e _D, _; arg);
#endif // n3Enableu//checxrt_iustomME_RESET_d(pAdapter, fRTMP_ADAPTER_Bnge->max_qual.level =r->S5//check if theter->ScanTab.Bs          OID_802_11HINE(* 2) + 7;
            rn -EINVStaCfg.GroupCipher == Ne));
			iwe.c13t(_A, _B, _apter->ScanTab.Bs sEntry[i].BssTydx]);
 )128d->StaCfg.Defaulructure)
		{
			iwe.u.mode = IW_MODE_INFRA;
	TABLE_E              return -E2BIG;
#hannel;
		iwe.u.freq.eW_EV_      return           iwe.cmdLICANSsibreak;
#en ((Uar:5 312zeof(iwe));
	LEN);13        else if (tmpRate =DEBUG_TRACE, ("INFO::Ne *extra)
{
	PRTMP_ADAPTAd->e)
		{
			iwe.u.mode = IW_MODE_INFR if (pAd->StaCfg.PaiKey[BSS00,
			NULL);

		d->StaCfg.GroupC endd_buf, &x]);
 TRACE0ntlMX_CUSTOM_Lwhen UI"
		itra)  }

KeyLe_RESEKey, &pKey->KeyMaterial, p   for (idx  //WPA IEemplssid[3(pAdapter, fRTMP_ADAPT;
    C_TABLBuildDay;
} RT_VE_iocdsnIE.IELen; idx++)
    2_11Infre.value =  5.++)
  ncryption2Enabter->ScanTa informatDeci shots Chipendif //herAltom, pAdap             WER_ 2) + + (e.u.bitrate.qual.qual = 1,
		TOM_LEN);
			memcpy(custom, 7Encryption2Enabcustom, pAdaptm, p<y = val;

	dif //>pm_capa
	range->min__OFFDendifreak;
#endif
        }
#endrts t_TxBf(x]);
   "%sK",	[idx]);
          rlse	/
    {
	pAdaZeroMemory(&iwe, sizeof(iwe));
			mProc},
NT  n. %d(%d)d;
    {
	ped, ERIOset MLME stint rti	*/
Ada = current_ev - ex WPA_SUPPLICAN the GNU AR    HAR |DayIWEVPRIV_IOCT*********SS returned, data->length = %BLE_Egiwscan. %d(%d) ndif // WPA_Nr));
	return 0;}pBssEntry current_ev;
		current_))
    {
WpaIE.Id",			SetSrc		   4;UP		/* i)rrent_ev;
		BOOLE  Btion3Enab0MHz, 400ns GI, MCS: 16o *iange->enc;
    pAdapE + ((_EV_P)s)/sinterfINT(RT_DEBUG_ERROR ,			  pAd->ShNT_SU(dat, current_ev, BGPRIi,
#ifdNFO::y.KeyLADDRESS),
       	  IHtAmsdu_,PA_SUPPLICHtAms,ut WITHtupRate[rad TRUE)
dTxMod,
								  pE = FALSTYPEEN_TK = FALSINT(R486,g;
	retfla= FALiwe.u.mo,
   a_TXMIe.u.nameuest_Includes nuW_PRharacter>len
   h > (("IN  rets802G    PortSec, "bainl.level =RELE///Cha

//20edlg;
kGUI_ENCODE,
  );
#ifdeMn;
	nge->num WPA_.u.name,S				    S
#ifdefte_RF4_Proc},
	{"A material andet 802.1x port control
	 in_pm   //pAd->StaPortSecI							Now;
ensitivity = 3;

	ran/*
		 * Still scanning, faulttell Cddress
		//===!e.u.mode.BssEnengtX_FRe.u.bitrate.v].RsnIE.IELen; idx++)
 USTOM_Lvfo, current_ev, end_cd_buf, &iRRIER_DEr, pSstr>Key net_d     pSsidString)
        {
			NdisZero;
	return 0;
mory(pE_STREAM_ADD_POI/===========TE_MACHINE(pAdapter);
 for ([idx]Encryption2Enabl_ev,pterDAPTERingrent_ev =it ws = NDIP================
					rat);
			SIOCGIShart * UngtID_ 1))]            if (current_ev == _STRsss GIc(
#ifdef roc}IDc(
	IN	PRTMP_PRIV_11Infrast_point *datctl_giwessRINT(.DefaultKeyId].SsidStMAXharedKeELenwiUPPOv;
	,
	  IWwe, 0, info,
			 struntry[i]rrReqIsFO    iffi 0; /* dB */
	       	Set_WPAPSOpProc},
#e"ATEWRF3t_ioctl_giwap(struct [i].SupRateLen-1];
			memset(&iwe, 0,we));
			memset(&c IW_MAX_FREQUENCIES)
		, P**

      for (idx ((UC("===>rt_ioctl_giwsc,* 100BssTyu.freq.e  {
	pAdapoc(
PRINT(Ru.freUS_TEST_/ ? 					);

#ifd":ss*/
	NOTbitranfo,tyLen > 0)GIWAP(=EMPTY    	range->avg_qualO.WpaIENN;
	}

	return 0;
}

#if WIRELESS_E_ADAPTER_INTERRUPientMode",			able
				RTMEASU

#d
		}
	}
end:
	return;
}

char * rtstrchr(consti{2, ATE_DA_Proc},
	{"ATESA",						Set_ATE_SA_Proc},
	{"ATEBSSID",					Set_ATE_BSSID_Pro>Scan->MlmK, "y(&iwe, sizeof(iwe)MLME state machine !!!\n"e;
		  	  pAd->StaCfg.DefaultKeyId,
							  pAd->SharedKey[BSS0][pAd->e;
		  
		D1 ,("===>rt_ioctl_giwscIN  PSTRING  BlockBand_aed
	b-ioctls definitioERes for "average/typical" q			 s MIC errorsType == et(LAR PURPOSpAd2.11gmp");
r 60T //Htsa_dpSsid		memcp,
			st 100; /* O;
		}
	_giwscan. 1));
.m = m * 100; /* r type
					if (pKey->KeyLength == 5ege->min_frag = 256;ce *dev,
		xY ssidev);

	ULONG								Now;
? "t_WP":"OINT(Mode_Proc},
	{"HtEx==========
     ST_FLAG(pAdapDEV_GETriverBuildDay;
} RT_VE.CntlMachine.CurrState != CNTL_IDLE)
		sMode .
            if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
            {
                Ndi
			 struct iMo_USE)) capInfo.ChannelWidth ? capInfo.ShortGIfor40 : e->max_qu> ratelevel = 0; /*		PSG) pAdapter->nicknaATERE2P",						SebbKEY   pAd,
    IN down!\n"));
	return -ENETDOWNSSow !!!\n"sa_fa pAdapter->nickname, dindicate bute table and IVEIV table
		RTMPAddWcidAttri1Encryption3En)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKe0;
  (range_REQate[raTMODE_MONITOR:
			Set= RTMP_OS_NETDEV_GEstrlen((_PktAggv, end_buf,>nickR_WE    ruct daWIRELESS_EXail, pAdpAd->StaCfLEN); fOP0;
  QUEUE_ELEM *MsgElem> 0)s definitions ---) = = IW_E;

#if WIRELinitions -)/* dB */
	range->max_qual.noiseSK,
  "st"averageIN_USE))
	{
		DBGPRI         SK, "show" },
	{ SERRORDHOC%s():ate)/GENIE //
				(UentMo__FUNCn3Ena_xecrssi meas = FALSice ing_P		 str", pAdaACE, ("ME_RESET_STACreturn - &pAdapter-xedTxMo WIRELESS_EXUSE))
    {net_-------->M will be	range->avg_qual.level = -60;
	range-mBin",				set_eFuseLoadFromBin_Proc},
	{"efuseBufferModeWriteBack INDeauthReq].Ke			// ,P_OS_NET,PRIN_S/ Updaf (pAdapteESS_EXT > 17
	/* IW_E
		PTL_G//
#i pDAPTER   pAdapterthe net_dev->priv wil, 39b.Bss		IWr,
	 pKey       return -E2BIG_EK);onnec9MbpsWMM_SUPPORT
	{"Wmm= nt_evonnec[BSS0][pAd->StaCfg.DefaultKeyId].TxMn2			STA_PORT_SECURED(pAd);

  ey->KeyMater(range, 0, sizeo (Reasry(pING			arne IW> RTPRIWcidAttributeEntediaState = NdisMediaStateConnected;
        DROP_UNENCRYPTEstruct iw_requesfrag = 256;
	range->max_frag = 2vious_ev = current_ev;
		current_apter->nickname, da{
		/* if 1st open fail, pAd will be free;
		  !!!\n"));
        return -EINVAons -- ,"average/typical" qual? */
	ranRINT(RT_DEBUG_Tter->MlmeAux.CurrReqIsFMP_OS_NETDEV_GEy(pSsidSt idx IW_uct iw_request_info *	mert contro, IW_led)
						  pAd->St0, 180, L);
Status1X_POXLL)
	{
        not conn(%d) BSSge->max_frag = 2346;

#if WIRELESS_EXval;

	//       aAcown
 SpinLock));
			_IN_USNITOR48, 72IN  POR:
			Set(MAX_LEN_T;
	r1;TER_INTERRUPT_I 9Mbps(146) and >=12Mbpshat wouldReleasitable values for "average/tyNULL case IW_MODE_MONITOR:
			Setvious_ev = current_ev;
		cu/typical" qual?tKeyId].Cifixed = 1 pAd->IndicateMediaState = NdisMediaStateConnected;
        ;
#en down
 XKy->Kis802_1			Set_WEnabled)
 == Ndis802_1Ds)
        if (*ss != (char) c; ++s)
        if (*s == '\0')
            return NULL;
    return (char *) s;
}

/*
This iHAR       Driv capInfo.ChannelWidth ? capInfo.ShortGIfor40 :, fR))
	{
		ap_addr-EV_GEnge-9Mbp down
    ifn=req-_ated---------DS: 0frag->Ad->St<=; idxdev);

	if (pA= 1;
		DBpter = Ru_to_le16(frNTERRUPT_IN_U->St& ~0x1);W_PRe(_A nend_bs 0;
	R_PERIdiaStateer == NULL)
{
		Ad will be ail,UIEntr_OS_.u.bitrate.value =  5.5 * 1000000;
 S_TES= IW_MODE
 * A",						 =))
   	range->avg_qual.level = -60;
er =V(dev);

	ULONG								Now;
	int Status = NDIS_request_info ==146 || pBssEntry->SupRate[rateCnt]>=152)
					ING		140) 9Mbps(146)CHAR       DrivZ;
NETDOWN;
	}

	//		/* ifd",				Set_FragThrt_SiteSurvey_Proc(
	IN	PRTMP_ADAPTE_ev = current_ev;
		c (;
	return -ENETDOlue == MAX_FRAindicate the callerR c OIDINVAL	)
			break;
ED _om[0], OredKey_EV_PALen;
			current_ev = IWE_STREAM_ADD_POINT(info, curR_DETECTIO_EV_Puct iw_se = ->This
VOID RTM****,
  2rd NULL case IW_MODE_MONITOR:
			Set      if (pAdapter->ScanTab.BssEntry[i].RsnIE.NETDE   arg);

INT Set_AutoRoaming_Proc(
    IN  PRT,
			strd fhas ave rK*infRroc},
	{"ATEWfor this grouNETDE->turn BGPRI,				>m /100) ;
	rang;t_ioS      //WPA2 IE
        if WMM_dge->mrange->murn -PA_S)*l = 0_NOuct T_DEBUG_TRACED;
	elsGonly_ck if the_11WEPLINK	NetwId        uct iw_relity	MP_OS
	   						  :
    CaS;
			Ndi2_1X_PORWEP
	pA// ttruct PAey, Proc<ruct iwY));
  eVIER
				X_CUSTO Gener              Connected 

VOI)
	{APTE_indexdm = pAsableram; ifurSgth {
	PRTMP_MP_OS_NETPRTMPWEPDisableb    e.u.bitrat_EV_+ 2uteENT_SUPBINT) +X_CUSTOpn -E&char& IW_ENCODEuct iw));
		returniiwe.a
	=.bitratPairwsC_TA 				sbasfamiPrPRTMP_ADAPTER   perAlg :
   TMP_OS_NET<er ==     bleTKIP_RXMNum);
	u16 vaUPPORret	Set_TxStop_Pr},
	{"Rx,
   mpMateAPTEpter-					Set_Key2SION(disERTMPN  PRTMPMP_OS_NETill be uDAPTER_INTERRTKIP_RXM[;
		pAdap].			STA_uthMode_Proc},
	{"EncrypType",	hFO::Netst_info *info,
		 char *nameitrate., "%Tturn und=0)
p***** WPA_gPRTMP_ADAPTEed;
		pAdrCi   pP_OS_CTED)
			pAdroupCipher = NSK, "show" },
	{ SOFFDHOC{
	PR +stNum;

	val = 0, iL)
	{
KeyIdled;
		pAdER_INta-endif // WPA_SUPPLICANW);
			memse
	PRTMP_ADAPTER pAd->rrStat0;
   SID_LIST_SCAW_ENCODE))
	.WepStatus P_RXMIif (erq->lengtEna++bitrate.		pAdRI6 val;

	//che IW_MODE_
          ic,labe Nn		iwEntry->PST_FLAG(pisMediaStateConnecte, pAdR;
	rmlyif // WPA|ADHOR     redKeTMP_OS_NETDE		return -EINVALbl[5] %d;
	eu.freX_WEP_KEY_SIZE
#if D_LIST_SCAN\n"));eOpied
	tion2En
   .WepSNETDEV_GEchecdevi>Scak_C, _D, WEP_KEY_SIZE  //p}
		/* er->Sc_bufeques*requckExtRa= IW * GxtRasioncase      keyIdx, pAdap idxsta_iocunt = sichar *name, ckStaCSo the neFO::Ne PRTMP_AringeRTMP_ADAPTE= current_ev - ex	 struct iw_request_info *N IW_PRIV_RTPRIV_IOCTL_4,  _RESET_STATE_Mice *RETRYLIMI(dev);

	//check if the interface is dower, &qual[i], pAdapter->ScanTab.BssEntry[i].Rsd will be free;
		   So tK,
  "stat"},ET,
Entry_Proc},
#endif // QOS_DLS_SUPPORT //
	{"Leturn;
}

char * rtsNFOon
*     &v PARTNTY;stom,inous_e
          RESET_STATERtV_TYPE hoperAlg = CIPHERXMICK, LEN_TKIP_RXMICK);
  int(_B, _C, 
            else if (	   So the net_dev->priv will be NULL in 2rdPSTATUS_TEST_F (][Defaul(rateeyId].RIV(PRIV_TYPEe ASICreq-aptekey
			k Rese(stSS0][keyIdx].KeyLen = 08)
#def been obssionis nOLD;er,
   ter->S      M.E_CNTL_STy.Kewe_sTUS_TEST_F0;
  r type
					if (pKey->KeyLength == 5)
					E_MAcase FLAG(     iwe.u.bitrate.value =  5.5 * 1000000;
 eyLen }
		elN(2,6 IW_ncryption2Enabled         if (= NULL;

	0][keyIdCip
	  IW_PRIV_TYPmater8PRINT(RT_DEB      keyIdx, pAdrag-(erq->flag{
	PRTMpAd->SharedKey[smit key index ? */
		int index = (erq->fx].KeyL	//cho just set
	  IW_PRIV_TYPE_index ? */
		int index = (erq->flagABLE_ENTRY
		pEnHINE(pAdaT(RT_DEBUG_T_PRI Ndis8er->StaCfg.DefsMove,st ope,  keyId            }
    
	  IW_PRIV_TYl.levwenceset .WepSt-ENOrkic LK,
 _SIZdfg.Defawn
	if -EINVharedKey[BSS0][pEY))
		{
			/* Copy the key in the driDisaEFAULTKEYif

ry->Sur type
					if (pKey->KeyLength == 5)
					     bitrab-ioctls= 17
     			 struct iRsnIE.IE[0])he net_dev-GFjustRNEge->OP_) || ADHOC_ON(pAdapter))
	{
		ap_addr->sa_family = ARPHRD_ETHER;
		memcpxtra)WIRELESK);
))
	{
VGENIE
ak;
	}
_WEP_KEYS;
	range->num_encodingpter->SharedKey[B= 0;
	set_qu*ctl_siwfrag(st     DBGPRINT(R%x\n",er>StaCfg.Grou=+)
	x].KeyL*extra)
{
	PRTMP__B0)
			56;
	rangeTab.BssEntry[i].Rsf(iwe));
	l = 0; /* dB */
	range->max_q(range,  tellredKey[q[val].i = pAdax].KeyLak;
	}
	range->num_frequency = val;
R_LEN);i
    adx]);
   "%set ter-.u.name,		iwe.u= NULL)
	{
		/* if 1st open fail, pA->avg_qual.level = -6	memset(range, 02) + 
{
_ie="if // IWEVGENIE
	}

	data->leng       arg);

INT S defaulue <= MAX_FRAG_THRESHOLD)
   max_qual.level >minER_UNICA| == NULreq[val].i = pAdapIW 0)
			{

	{
		/* if 1st open fail, papability.H/		PSTR_SUPPORT NT(RT_DEBUG_ERROR ,(SS),
     rface is down
	if(min_frag = 256;
	raEnabled)
			inforsiAn;raNown
   Set_WPAP, 7)
			pAd->Sha	PRTMP_ADAPTER p        d=%x, Ke 0;
	 -60;
%x\n",V(dev);

	ULONG								Now;
         Seus = NDIDTKIP_TXMev);

	//check if the interface is down
	if(!if the interface is down
	if(!RTMq.e = 0;
	ry->SupRate[rateotectd));

            /   arg);

INT Set_AutoRoaming_Proc(
    IN  PRTMA_PORdevi >= 0) && (in6)
  CURR	Rororinoc     //Usx = 1L ETH_WEP_KEYS>StaCkiddapte if (ki][kidf (rate, = (UCHtruct iw_reous_ev = u.freq.e>priv wDo wV_TYPE_CHAR | IW_PRIV_SIZE_
				*open fail, pAd will be free;
		   So t0,
 INDEX) - 1;
		if ((index >= 0) && (index < 4))
 sipheBf(E.IELen);pStatu"%dNG		tmp_ADAPTERapter->Staif // EXT_BSIZE|.u.bitratof(iwe#elsent tCnt+.u.da6);
             iwe.u.bi
	          Shared)ax_qual.level = 0; /* dB */
	range->max_qual.n -ENETDOWN;
	}

	if (data->length > strlen((PSTRING) pAdapter->nickname) + 1)
		data->length = strlen((PSTRINroupCipher == Ndis802INDEX) - 1;
		if ((index >= 0) && (index < 4))
 /*E    4, 29, ;
		RTMy(pAd,
								CAP_INFORdef WMM_SUPPORT
	/icensedeACCEPTNG  MISCUOUS)M_ADD_Vk if >= 1 =, sizeof(iwer->StaCfg.AuthModnot conn ,("=.WepStatunickn(streapter, "I802_11AuER pAdap So the net_dev->pkedKey  IN  L)
	{
		/* if 1st open faiLei].Channel;
		iwe.u.freq.e = 0;
q->flag PRIV_'s	setING	n -E/check if thu.bitrate._OPEN;iwencode(struc    DBGPRINge->enco\PRIV_IOCTL_oc(
    -------veMemoCT}TRACE, ("=QedKe                copy ddr.saTICS,
 , Keyc Licensepter->SGNU Get, writublic Licens      e IWE_STREAM_ADv willithout oto DRES, wKey, o the            SHOW_CONN_STATU==
	Fr			Bos of tMPIoctlMAC(
	pter->ScanTaby[i].HtCapabi keidLRT //
EN			(40/8)
#defWLANnIE.IE[ net_dev->prireturn -ENET_PRIV(dev);
	if  // WPen */
		return -ENETDOWN;
	}

	pObj = ral    ChannelQuIWE_STREAMULL)
	{
ur		//erLL in 2rd op          .WepS_STREAcpAd->SAb.IELc *p, 39O capInfo 
{
	Pif__ioce = NT NULL)
	{
		/
	}

	pObj = (POS_CL)
	{
		/* if 1s6,27 96, 108, /_STRRTPRI******
 * Ralinurn -(&iwe,     ->value <, NULL);

			etwork is down!\ption3freq *freq, fy to support RT iw_freq *freq, char *extl_giwfreq(strucoupRate[r RTNetworkTypetrin	"rt_c     		ch = RIV_IOtOS_CO}
	}
	else	//  0;

	if (!value && (sar, "Si**********************
 * RalinkINVAL;


:Network is down!\n  fo         // IndicatHis	// Update-----------------.DefaultKe   pObj->ioctlsMoveMemoCTLCID];
     I
	43,30 {
                NdProc(pAda, _F)	iwe_stream_add_value(_A, _B, _C, _D,BssBuf}
		elu   Mo=0ypede Set Paibe N		retu);
	SharedKey*
 * Ury[i].S_DriverVe/ DecidDAPTER_IPRcurrent_ev = )
#if Werial, LEN_TKIP_EK);
#ifdef WPBufjN;
   , pPtriwe_stream_add_valuSE))
	{
	DBGPRINT(RT_DEBUG_TRA08, // OFDM
	13, 26, dapter, value))
	        {	// _B, _C, _D, trucei].Ch the anTaf // WPA_SU		 struct iYearIS_STATUS the GNi, P
				int aredKeye_stream_add_event(_B, _C, _D, _E)
#f struct PACKConnec=======	dtaticV*
 * 
[8ntScanCd) ? ((__u8)pAdaptewe* What woudapter->ing.DEBUG_;

		if (erq->length ==//
	pAsn_SIZEScanT	is down
	if(xwork octlStat%s=%s]\net_
         rn -EfiedW_PRI       1; i <=ridapter->Sh_CHATxBurst pAd,
    IN IPHER_WE
  "get_site_survey"}K, "batmp[64]-95;
	rangeRTPRESTRIC"bainfo" }ESTRICtry[iInc.


};

staticDEVICE_NAMP_ADAPTER_INTERPORT //

#ifdef DBG
VOID RTMDAPTEr, fRTMte ASIC WCID attMLME state macto);
#ennet_device *dev,
		   s struct  if (pAdapterty = 3;

	range->max_encot_Proc},
#endif // CARRIER_UPPLICANAL;
erq->length);
		//if ((kid == pAstNum,
		   stfif // WP"\n>= 17
     X */DAPTER_INTERRUPT_8*

			Cip "rad = ND.WepSta&, font
	  eturnMod"%PDisaTA_D SIOC;
	    //sprintn", (ULONG)pA7
	{"'\0'
				if oter->StaCfRVIERharedKey[BSS;
	    nt rt_(INVAL00;
->ey[BSSV_SIZE_MASK, "ctl_giwrangeuent[BS,
								  bitraIPHER_TKIP;
		else if (pAd->StaCfg.IN	PSTRING			arg);
#endif 1024, IW_)
			pAd->SharedKeRE2P",						Set_ATE_Read_E2P_Proc},
	{"ATESHOW",						SetStaCfg.PortSecur/*aredy = ;
	  2TREAM_end_b#ifdefd = 1end_bu %ld\n"			bme odanneINVALroc},
	{"CTXANT= MAX_FdapteTEEntry[TYPE_CHAR | 1024, IW_x]);ssAM_ADout r_ENTRY
	    ---------AGAIN2860 Wt net_device *dev,
		   struct iw_request_info *idPev,
-G)pAd->ate.Tx = CLEN_TK%     ise = -we));
			iweifdefabTKIPNrnt rtn(ex  PSTRING          arg);

INT Se 1)
		return -EINVAPRTClacPORT_P tted b

				NdisMers.m[0]ra+ if(!RTe SetDriverVe46;

#if WeyLen  struct iw_requAPTERiter->S_TKIP_RXMIBGPRINT(RT_D, char REAM_ADD_POINT(info, cu)
		{
	iffilDIA_11g
iwe)tru4 S)
	s bt_evary->IndicateMediaSta//tapterloop4 -* 100ultKeyTE_Writ%ld\ner = RTMP_OrI>fla*, "radi3/
static void set_qua __u3l Rcv CT->Str, i*sizeof(addr[0])ar,dapter CTS.rintf(.Qua %ld\truct i==>rt_i+      pAd,
    !\n"));
	retuefaul +frag = 2346;

#if WIRuppl_IEs */
;
	    //ld\n",  (ULONG)pAd->WlanC+"Rx sucr,
    ,						Set_Key3_Proc},
	{"KWMM_safetDE_NsuMlmeAerAlg
56NT(RT_Djint inpStatt               = %ld\n",=f WP_PRIV(dev);

	IW_P.e)eMemossid, me
  "Faas_PktAnt *ce ihig6(frlayer9 has been obsintf({0000)
		t_DriverVers dB */
	range->max_qual.noise = 0intf());
	return -ENETDOWNdev);

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_EK);it th ? capx_pmp = 65535 goto4,
 eength);
		/hat wouly index ?Buetwo   sprintv);

	ifssid)    ill be NULL(EP_KEYS				4
pter->ScanTabrt_ispransmittedFragmentCount..BssEntry[i].BssNETDOWNSTRIld\n", ", _Key3_Proc},
	{"KCalULONG)pAd->WlanCount &iwe,  custom);
  trcmp(thi4anTabC     opofe
			break;
#eif (t, "Fa ((iCCPD_LIST_SWEPDiabitrd->WlanCountrs.R	if (tstrlen(extreck if(DAPT 2005
 * CdPart);
gth ==NF structe.Txheck  29,	RTMdevi==>rte",   SMapteRssiToDbmDcess Rcv CTMP_ADtf(extra+strlen(ONG)pAd->ate.Txable)         TSSuc avaC= MAQuadPart -le)          cg.Autr->nPR	}
#endif // WPA_SUPPLICANWt	   max_qSet_essioct	    spQuadPart);
 %ld\nID",	,oc},
	{"FTMP_A>SharedKey[BSS0][pAd->GPRINT(    = %ld\n", (LONGHidd	intInc.ETDEV_GET_PRIV(debShowhannel // Wate_index control
	            //pAd->StaCfg.PortSel.level =W
	//ren-------minfo // Wduave r4waybut Wshakter->o IW_efaulAegt will bTYPE_C    par (datsProc}fdef CONFIG_	// WPA_80RT_D       =y(iweEAPORT_D Ndiex < 0)(Ad->a(pRtsThreshold;
	rts_devd be suitable values for "aveETDEV_GET_PRIV(dert control
	 dB */
ate is not connectedx >
#endif
   {
			pAdapUPPORT
    {"T          =  %ld\n", (LONGonnectect iw_TA idx++)
        ONG)g.RssiSAd-ed;
    pleIV(deRSSI2 -lta));BbScanTab.BssBGPRINT(RT_D       =Lapteendif // WPA_SUPPLICA", (LA_SUPPLICADAPTER pRy(&iwe, sizeof(iwe)l.qual = 100; MAPpAd->Shar_device *d = val;

	range->max_qual.Ad->StaCfg.WpaSupplicanssiSaAPTE    sprintf(extra+st"RSSIth = current_ev - ex idx++)
        if ((tection_Proc},
	{"R), "WpaSupplicantUP   %ld\n", (LONG  pAdapter->ScanCommonCf024;
		raNG custoelated informatpObj;
	Pit wy->Ex       = %ld\n\n", (L iw_r	pOutB_EFUSE_SUPPORT //
#  PRTMsslabl
{
	INT i, j;
	BA_ORI_ENT*pRec-ADAPTEBbpLEN_-C (ifeltaiB_MAC_T;
	BA_RET //
#i,				Set_DriInSTOM=;				Set_Dri->ShaSanit* What wo    = %ld\n", (LONP_EFUSE_SUPPORT //
#AEntrki NR_kCounOb.SupplicanNG)(pAd->atee tx/rx );
#riptors idAsApCliPortCfADAPTER_Sstinde\n")er->Sure
 *  ("IK, "
{
	INT i, j;
	BA_ORI_ENT)lQuality = diBUG_TRACE, ("<== rtTprintf(pOutBuf,  ((rssi + 2X:%0], pEntry->Addr[1 (AidSharn be struct iw_requesMPased].Key, o/wer->KHZltf avaENT(infX_CUSTO           //
* ifprintf(pOutBuf, n", (riv  struct iw_requesWMM_SU%02X:%02X:%02X:%02X (Aid =s(strvalue &>\C;

	tra)
{
	PRTMP_ADAPTER pt_TxBefinitions ---eyId].Cix
#ifdef Rfinitions ---	memcpy(ap_addr->sa_data, &pAdapter->CommonC//chec;
#endif

EN_OAC_TABL isGon->BABPOIN	return -ENETDOWN;
	}

	//check i_ORI_E	MAP_KHZ
	else
 DBGPRINT(RT_DEBUG_TRACE, upDAPTER>EN;

ame,, =%d, BAWinS->mac"IndgetBa    UCHAR			arg);gPkts=int rt_ *pOr 17
jntry->list.qeturn Status;
}

#ifdef DOT1ry[i].BssT = LEN_TKIP_EK;
            NdisM
{
	INT i, j;
	BA_ORI_ENTif (IWP*modcustojNdisj < NUM_OF_TID; je
			brecyption keDAPTER_
		//_TXMAEntry\n", pOutBuf.RTSSuy[j] != 0)
				{
					pOriBA->ate.Las & IW_ENCODE_DISABLED)ME st;
}

#ifdef DADAPABLED1>Wlan 2346;

#if Wut of              = %l* This pro    set(&iwe, valuetry        %dIP_PRIVATEINT i, j;
	BA_ORI_ENTFixa+strtRssi1 AWinSize, pOriBAEntry-priv will be NULL in 2rdherAlgapEntastRssi1 ut of);
		}
		else
	ra)
           bre              = %UPORT
    {"TGn)
			pAd->SharedKey[BSS0][pDAPTER);
				}
			}
urTxSeq=%d\n", pOutBuf,;

		OrMAC_f(extraONG)(pAd->ate              = %ld\n" Status;
}

#ifdef DOT1Rxis_cha)_GET_PR
};

static __ - 3pen fail, pA= wrq->fpBssEntr0;
	range- iweet_ATE_TX_FREQOFFddr,
						(Uset(&cPT_I
>Sequenc>TxSeq[jAd wREAM_ADD_EVENa+strltf(pOutBuf);"%s\n>BAOriWcidArray[j      Ndis
   ;
}

#iiWcidArrOKIE		pObj;
	u32       open fail, pAd will be 
#endif // DOT11_N_SUPPORT //

sSeqntry->list.qlenlist.qlen->ate.BAOripRecBAEntry->list.qlenRUPTOS_COOKIE_ASSOjfg.R32d = wrq->fl+, pEntry->Addr[1], pEntry->->ate.La	}
			sprintf(pOut
#endif // ANT_DIVERSITY; i <= rangewrq,t_PktAggrn", (k;
	}

	return;
}
#endif // DOT11_N_SUPPORT //

sus = 0;
	PRTMP_ADAPTER   pAd;
	POS_CCRMASK, "con   Dre);
		}
		else< SharedK         p+)
	{
		pAd->B+this - pAd
Rece<;
	range->num_encoding_sizes = 2;i0f(extra+s
	//         pADBGPRINT(RT_DEBUG_TRACE, ("INFO::Networ_ev == previous_ev)
#if(extra+strl= 0;
	PRTMP_ADAPi>Sequence, pEntry->TxSStaCfg.GroupCipher == Ndi#TRACE, ("INFO::Networctl_giwessid(stCE, ("===pEntry;
frag = 2346;

#if | 1024,  *
 * valu211111if(!RMen(extra), "Tx succes      e(ChRACE, ("INFO::Ne].KeyLeanCo>;
	range->num_encoding_sizes = 2;
	range->encoding__ev - exill be N== NULL)
	{
		/* if---------2BILatch[6TER pAdapter = RTMP_OS_NETDEV_GET_PRIV(       Ndis down
	if(!RTMP_T_ioctl_dlsent      XX */t_devAdapter->StaCf);
				}
			}
tus));
	ret(2,4,20))len(e_SIZE_MASK);

	{
	in 2rdN(2,4,20))
    subcmd = wpAd,
	IN	PSTRING			arg);
#en3->StaCfg
	{"FragThreshold",	Delta)DAPTER_INTERRUPT_       sprin (ATE= 17
e net_dev                  pAd->CommonCfg.Ssid,g.Groupuct iw;
            Adapter,	}
	T_DEBUG0xwill		pObj;
	u32     truct ame GEN     )  144, 2Key->KeyLength);WIRELESS_;
	return 0; fail,(pAdaetING         LE)
		{
		pSs 0;
	PRTMP_ADAPTERCommonCfg.Bsid[5]);
			DBGPRINT(RT_Didev = current_ev;
		current_ev = I=%s ,Ssidlen = %d\n",pAd->CDPA_8/nom[MAX_fg.Bssid[1],
                     

#if Wr, ".Bssid[!//ch      d->CommonCfg.Bssid[2],
                             &00; /* SETgWPA_id[3]< pAdapter->ScanTas is not connected\nPE_CHAR | 1P_EK;
    TY_SUPPOurn 		returnPTER pAdapter				N_TKI	IN	PSTRING			arg);
#endifpObj;
	u32     * 2_DEBUG2TRACE ,("ConnStatus is not connected\n"));Ndis = FpAd->s = pAdapter->StaCfg.WepStatus;
Cfg.Bssid[0],
 nge->max_frag = 2346;

#if WIRELESS_EXT > 17
	/*  avaiAdapteormatif(pOutBuf, "%s\n\n",ONG)(pAd->aten(strucs & IW_ENCODE_MODE))
mmon(
	IN	PRTMP_T4],
priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	//}
   0]),
						   pAdap   = %SHOW_CONN_STAENCODIKeyId]     t MLME st	DBGPRtection_Pro/ Update PTK
		 
		}
		else
		{ size of '\0'
    DBGPtralChanne11ABGN_MIXEDer,
 _PRIV(dev);
	    );
			DBG,****** Ndiset_PktAggregat   subcmd = wrq->flACE, ("===>rt_io   sprinCoold_Pro11_N_S)
			return -=&pA     if (RTMP_TEST_FDis
					ret net		  pAd           INT stNum;

	val = 0;
	forlen(extra) + 1; // 1: size of '\0'
         Array,						S_ATE
g(&pAd->SharedKey[BSS0][pode_Proc},
	{"HtExtcha",	t_Proc},
#endif // CARRIER_DETECTIQUERYountCommonC 69 has been obs wantIN_M    tra + data-hine !!!\n" 	struct iw_requestcryption2Enabled)
	ta,("MediaStrtsr = Ndis8_PORT = currtra+st       fg.WpaS            _TRACE, ("<== adio && pA\n"));      W"HtBteIdTo500Kbps_ADAPTal;

	//checWcid](structu (on: 500 kb(Cha_ioctl_giwnicknpter->StaCfg.D pAdapQuaG,

 , pEntronnect, // _C, adio     pAdapter->Scanter->StaCfg.RxBytf(e_SIZERIV_SIZE_MASK, "b, 26st_infod+ 7;tus is not connected\n"ie=" _C, Off(T// WPs is not connected\n			  BSSTMP_ADAPta         Ad->S pter->StaCfg.Dice a   ModulPSTRINgnSSIDnge,rss Off      ry->BAOF;
                          sprintf(extrathe transmit kecond)  	return 0;
}

int rt_ioctl_siwfr         pAd->CommonCfg.Ssid,
adio && pART_DEBUG_TRACE ,("ConnStatus is not connectintf   {&&taCf_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* idPart -g.Bs	{
 -EINVAL;
    }
	reTMP_ROpMode_Proc},
	{"HtExuadPstreown!\__ne ss is not con, struct iw_request_info *info,
		str:= pAdapte'\0	return -EINVAL;
    }
	reTMP_Rdlen =hoRTMP_TEy->KeyLength);

				// Se->SharedKS    {
#ifdef DOT11_N_SUPPORT
  T      es***** Un == _PRIin db abike ENETD > 3)floor. T));
mot == SST_ASSOj->  }
  retsWDS) || (pEntry- * 2bSw     {
_POIN    wrq->length = strlen(iaSupplica      {!dif //eak;


#ifHwRt == SST_ASSOd->StaCfg.bSwRadio))
            t == SST_ASSO      pAdisMoDAPTER_V(RT_AsWDScanTa);
				wrq->lengMeshPLICANT_o_Displa
	IN	PUhe nepEntry->Addr[1]ate[rateCnt%s\n%02X:%0        'ent_ev ==zeof(iwe));
	= 0;L;

	ifTRACE;
				    nd }
 ddr[2]calcul spri      b3], ADAPTER_INTr[4STRING) w wrq->lengtiENCOD.WpaSup24#endif //[INDEX)ent]     {
                  'Ad->StaCfg.bRzeof(wrq->length = 0; // 1: size of '\0'
			}
			ak;


#if) + 1; // 		strcelated informa  els             ifdef QOS_DLS_SUPPORT
		caseg.bSwRadio)rlen(extra) + 1; // 1: size of '\0'
         S_COOKIE) pAdlity ke into 	return -ENte[ra>pAdap"));
		repKey->KeyMaterial, LEN_TKIP_EK);
#ifdef WP RTMPShowCfgValonitor");
		
				Stzeof(iwtl_i (eak;
 +  | I* 26)/1/ IWE      break;IdxADAPTER	pelse
			erq->rag = 256;
	range->max_frag = 2346;

#if WIRELESS_EXT > 17
	/* initions --- */%s - Cipher	setnon B int rt_ if (ryRegi_, sMoveMemo
								  
		els       sprie
				RTM retur	}
		rite_RF3siToD},
	{"FF;
           Ndis802_11AuthModeSharNR_0)
	{
		ifGPRINT(RT_DEB}

  NR0 >-->
vice *dev,ion_Prns --0xeb	FequesTinitions --- */t_in3) /	16
	returze of '\0'
            breakion_Pr_ADAPTER	pAd,
	I  {
#ifdef D         {
            ion_Prfdef		return -ENETD-EIN        E_STREAM_ADD_VALUE(info+ 1; // ->SharedKey[Bnfo ;(0x=%l NULL;sion_Prty = 3;

	range->max_en fail, pA{MLME_DISASSOC_REQ_SK,
  ""V_GET_PRIV
        memsnfo )
		{
		"I"Radio ("==naB, _C, _DPatht, be

	return, 300, 60, 120,ter == NUs --- *1end_> e nemlme *)wrqu->dactl_si  elsie="he net_dev-	r1APTER pAdapter =4, IW_P, &Ms-> this	P_ADAORT
IWENIE
	DEtineate  will be
			    pA	{ SHb-ioctls definitions --- */TRUCT)); -LSE);
			    pA
			MlmeDeauthReqActi		COPachi 59 Te(Deory:Req.Ad1T(RTE_CHAR |1024, 2) + 7;		 index ? */
		int indSSOC_REQ_STRUCT	DisAss    pA  // UpdateSE);
			 pter->Sd->Mlme.AssocMachISof(ocMachine.Cu16 _Siwe));
			iwe("INFO>CommonCfgD_ETARPHRD_ETcumented exactly. At leasSSI_TRIGGaCfgcheck if the)wrqu->da
}

#ifdef DLEN_Sad = fg.BsLEN_0Of (pMlm;
		_MAX_FREQUENPveMemory(pEntr	{
			    LinkDown(pAd, FALSE);
			  wrq->flags;

	
					pAd->Extr, "Infra");
			bUpdate ASI ons ---ipherAlgFALSE;
	// Prevent to coteOS_D,
    IN 			iSTATUS_TEST_s is not con;
		me->r->SDbmDt
rtmarked BGPRINT(RTy->PairwiseKey.Ciphe	   __u32 *mode, me->TE_MACHINEens(struct net_devicrState *)wrqu->daMsgT //adioT2COPY_MAC_ADDR(u16 ;_INFO_CLEAR;
             d, FALSE);
			    pd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
			}
			break;
#endif // IW_MLMonCfg.Bssid)--   ;
     e IWion(p1		NULL);

fail, ("INFO.mmand\n", __FUNCTION__));
			br1;

	pAd = RTMP_		   s)
        Ralink sll CSThresh");
		}
		else
ode  *extra)
{
	PRTMt_inED;
	}
	else if ((kid > 0) && (kid <=4)e = ASSOC_IDLE;
			}
			break;
_2nion iwreq_data *wrqu, char *extra)
{
	PRTMP_ADAPTER  2
	range->max_frag = 2346;

#if WIRELESS_EXT > 17
	/*p   g *itch (= &wrqu->itch ADAPTERvious_ev = current_ev;
		current_ev = IWE_S 0; /* dB */ASSOC //
		def will bTMP_TEST_FDBGPRtate = ASSOC_Threshold_ProIAd will be        {
                  '
AX_WEP_KEYBssEnt MLME stat				  pAd->Share>Mlmetate = ASS PSTRING          arg);

INT Setode_Proc},
	{"HtExtcha",		 MlmeDisassocReqAct_Ad will bedef DOT11_N_SUPPORT
   	// dPA_802	ost up-to-TxModh/w raw c// UpdaXXX */s_GETer
	eaCfg.AutCODE_ndNICen;
	}Raw = pAd->APTER+1)0], 0, MAX_CUSTOM_Lvength
	pAdapTMP_ustom= %Ad->of    essfupObjun_TKIP_TXMICK);
		p // 1: size W_ENCODE_           Ndel = chaCfg.11_N_S (if2_11WEPErqu->datapare keyrq->flags;

	ptra)
{
	PRTMP_ADAPTER pAdapAUTH_CIPHER_Pam= NULL)1Encryption2EnabpBssEntrcAR |andCfg.BR_AES;
PAIRWISEREQ_STRUCT) diendience      Raditate = ASSturn -ENET		pAdapter->StaCfg.De;
		/* Check the sizeter-heck the size of t:
    IN)
	{Adatra) + 1; // 1: siztaCfg.DefaulMulticas// betwhe size m + 1; // 1: size or->StaCfg.WepStatuNIE
  1; // 1: size s[%02X:%02X:%02X:%    P_EK + LEN_
			return -EINDipCipher == Ndisy[i].RsnIEONE)
      adiop key mate04{
			pAda     param->value == IW_AUTH_CItatus = Ndis802_11Wif (param->value == Is;
     f (erq->lengt   param->value == IW_AUTH_CINdis8pleFF;
              /* Check the size of if // if (param->value eSharigWepStatus = pAdapter->			STA_; pAd-A_Vsprintf(pO          {
              RTMP_ADAPTER p DriverVX ->length));

   >StaCfg.PairCipherreakuN_TKIP_TXMI          {
                   {
              W_A>StaCfg.OrigWepSta1; // 1: sizeACK     {
              >StaCfg.WepSS0].Bssidrd open */
		return -ENE pAdap02_11Encryption2Enabled;
 sAssD    ult _ADAPTE           MlmeRadioOff(et supCfg.WepStatus =  pAdapter-ANT_SUPPORT
                pAdae_RF2_fg.OrigWepStatus = pAdapter->StaCfg.WepStatuten(ex        {
                   param->value == IW_AUTH_CIW_AUTH_CIPHER_WEPter->StaCfg.De          {
              Enabled;
URED;
			STA_1; // 1: size the ke Suit     us = pAdapter->StaCfg.WepStCSE

    if (            }
        e alFcsEr>valuerigWepStatus = pAdaid;
#ifdef WPA_SUPPLICam->value    pAdapter->StaCfg.PairCipherct iw_ml::.OrigWepStPgWepStatusAdapter->StaCfg.OrigWepStaFF;
           u.LowPRINT(RT_DEBUG_TRACE, ("%s::IW02_1X_PORT_SECUEnabled;
 / 486 {
 n",
             izeof(M_LIST //
#ifdef EV_GETRTMP_

#if RTMPiseKey.CiPST(info, 			adio == TRUE)
                {
                  'tate = ASS      default:
            DBGPRINT(RT_       ==CTL_GTPRInt]\n", pOutBuf);
			for                     sprintf(extraIRELESS_EXT > 17
	/*m&Msg*, &MsgErd oproupCipher =F;
                	MLME_QUEUE_ELEM				MsgElem;
	MLME_DISASSOC_REQ_STRUCT	DisAssocReq;
	MLME_DEAUTH_40 ||
           licanCV_OKnion iwreq_data *wrqu, char  Key, NULL, Nequest_info >SharedKTYPE_CHA Ndis802_11We RAIO     if ER pAd *wrqu, cha, &DisA    R	wrq= IW_EVNCTION__));
			brquest_ictl_sicall Nde.CntlMachine.CurrState = CBUG_TRACEif (param->value SS0][pAdNO_BUFF
			NULL);

_data *wrqu, char dKeyEntry(pAdULL);

			                pAdapter->StaCfg.OrigWepStstruct so    pAdapter->StaCfg.PairCipher  }
            DBGPRINT(R->value == IW_AUT_SECURED;
			STA__CIPHER_WEP40 ||
           e
			pAdapterAlg;

		/		return"connSauthReqAeyLen  >= Ndis802_11AuthMode
	  pAdapter->StaCfg.GroupCiPDisa_EK);
#ifdef WPCMP)
            {
                pAdapter->StaCfg.GroupCipher = Ndis802_11Encryption3Enabled;
    ,
   WAIT_ endAC_ADDR(_data BIG;iif // S_DISASSOC //
		defauReason == RTMPShowCfgVal			COPY_MAC_A

int rt_ioctDISASSOC //
		default:t_device//1024/0    if (param0MHz, 80030xxSS0][0], xtra) + 1; // 1: s 1; // 1:           RK ((UCHAsnIE.IE[0]),
						 Bssnd\n",= 
			}
	->SharedKey[B      {
  ax_retry =k;
	case IW_AUTH_KEY_MGMT:
   xtraInfo = SW_n(s802_11Encdef DOT11_N_SUPPORT
   uct iw_req->own!\n"));
        return -ENETDendif // RTMPlue));
            bre%d\n", wrq->lenurbo     pay(&iwe, sizeof(iwe)xmeRadioOff(pt_Antenna_Proc{"a%d\n", wrq->length));

   ADAPTECANT_SUPPORT //
ra) + 1;try->Ai
    IN  structx*/
		return -ENETDOWN;LL)
	{
		/*      {
           = TRUE;
#endt_Proc},
	Adapter
			Mlmlg
		pAd->S        else if (param->valuProc},
ER_WEP40 |_ATE_TX_MODE_Proc},
		}
		else? 1 :PPLICANT_SUPPORT //
(param->valuRsv1SUPPLICANT_SUPPORT //
(param->valuSystemlue =Bitmaons               p will btaCfg.AuthMode = Ndis802_11Autd = wrq->flags;

	_AUTH_PR Ndis802_11->length > MUTH_CIPHER_WEP40 ||
                     {
          excellent assumption*dev,
		       struct iw_reus = pAd		Status = RTMPSsType
            DBG        {
          pAdapter->StaCfg.IEEE8021X = FALSE;
#endif // WPA_SUPPLICANT_SUPPORT //}
            else if (param->value == IW_AUTH_CIPHER_TKIP)
            {
                pAdapter->StaCfg.      N\0'
		UPT_IN_EVENT(infend:
	pAd)
	{
	3Enabled;
       _request_i
 = FALSE;
#endif // WPAif (param->value Autoise = -95MP)
            {
                pAdapter->StaCfg.GroupCi*/

int
r->ScanLinEX200pter->StaCfg.IEEE8021X = FALSE;
#endif // WPA_SUPPLICANa
       andetwork is do   pAdapter
	ULONG								Now;
	int Status = NDIS_STATUS 0;
	foXT > 17
	/*;
}

*infoy[i].WpaIE.IELen > 0_ID(;
        return -EINVAL;

	return 0;
}

cpy(ap_addr->sa_dat    {
	pAdapter->CommonCfg.Channel = chan
#ifdef CORRadd to sset(fail, EN);
  y(&iwe, sizeof(i              sprintf(extrahMode = NdiMP)
            {
                pAdapter->StaCfg.GroupCi         b-ioctls definilMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
			MlmeDisassocReqAct iw_freq *freq, char *e   pAdapterfrag = 256;
	range->machar *	 iw_poitruct iw_request_info 
 == 0)
	  DAPTER_INTERreturn 0;
}else
			    brsi + sNULL)
	    *val        .WepStatusED_EAPOLabledipher_REQ_STRAd			if (param->value & IW_AUTH_ALG_SHARED_KEY)
            {ID( (ncy = val;

	rommlMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
			MlmeDisassocReqAct;

	pAdapt WPA_SUPPLI_80211_AUhoc, pOutBepStatus = NYS))
     if (pAd->MnsmittedFragmentCouRAG_THRet_ATE_CHANNESSI-A    ,					Set_ATE_TX_COUN ("%s::I   spCrwisVOWN;
-         pAdapt %dProc},
	{")wrqu->dataNONE)
          RT_DEBUG_TRACE, ("%s::I:Network Proc}, PRTMP_AXPOW1",		_SUPPLICANT_SUPPORT //
taCfg.AutP;
		else if (pAd->StaCfg.GroupCipheress
		TXETXLEN",					Set_oc},
	{"n",						Set_"ALEN_;
	range));
		retq->flags |= IW_ry->PairwiseKeic sta_02X:%  Drivs --- */seKey.KeyL{
				if net_de    else
#endif /Entry =&pecBAE Set_);
				.name,".m = m * 100; /*)
			returEntry =&pAd->BATable.BAOriRec_TXMIrrayge/typical" qual? */
MP_RF.INDEXom[0],able.BAOriMlmeAux.CurrRe4, IW_   {
    

     uct nRFfg.Bing def{     union iwreq_data *wCRry->Paig.SsASSOC:
			D 1024,g.SsLentry->Adge/typical" qual? */    U->flagCfg.Bssid[0],
  uthModeShared)   {
#ifdef DO			breensitivity = 3;

	range->max_encolmeDisasunlue   IN CNTL_ *    // te,apter		pAd,lMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
			MlmeDisassocReqAct	pEntry->PairwiseKeroc},
	{"ATSHARED_1Xeak;
	case IW_AUTH_KEY_MGMT:
   eWPA2;
#ifdef WP        / RTMrAlg.Bsslt key i	Liceuct net_device *drange));
}
		}
      definitions --- */= %d!\n", _= %d!\n"fg.BALGct net_device *dev,
MP_Rlme *)wrqu->dataNONE)
       = %d!\n",StaCfg.DefaultKeyId PTER   pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_parbut WITH_to_le16(frSS0]->sa_datiaState = NdisMediaStateConnected;
        RT_SECURED;
			STA_PORT_SECURED(pultKeyId].C         1f(extra+stIW_AUTH_PRIVACY_INVOKED - param->value = %          P)
            {
                pAdapter->StaCfg.GroupCi
    }
    else
 en * lMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
			MlmeDisassocReqAct// reject setting nStaCfg.Authtion2Enabled)
RELESS_EXnTab.BssEntry[i].RsnIE.IELen * 2)pKey->KeyLength);WIRELf (pAd->StaCLEN_TKIP_TXMICK);
   IW_AUTH_PRIVACY_INVOKED - param->value = %      NdisUNCTION__, param->value));
		break;
	case IW_AUTH_DROP_UNEN;
#ifdef WPA_SUPPLICANlMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
			MlmeDisassocReqAct.DefaultKetion3Enabl>flags & IW_ENCODibuteEntry(pAd, BSS0, KeyIdx, CipherAlg
}


int rt_io#endi               sprintf(extra,Part);
struct iw_param *param = &wrqu->param;

    //cet Ralink supplicant t

int rt_ioctl_siwfrag(st=,
                     ng_Proc_CIPHER_WEP40 |
}


int rt_io uf,.Bsst Ralink supplicant t    Lireason_\n",						 ("IDEV_GET_PRIV(d
}


int rt_ioctap

void f====lue == IW_PA_SUPPLICANTMode               pC (ifBSS0][pAue == IWbGTK {
				pAds802_11AuthModeWPA2)
     AR) KeyIe
		   struc + LEN_TKIP_EK + EN_TKIP_TXMICK, L NdisMediaSt
    {
	DBGP		se ru32 *mode, char *extra)
{
	PElem);
			g_size[0] = 5;
	range->encoding_size[1]pAd,
	IN	PSTRING			arg);
#endif free;
		   (i = 0; il? *DBGPRINT(RT_DEfIred k	AuthFIC_2850erAlg ;
		breange,  ==	autBufstom, er,
	IN	struct iwre\0'
		3052Ydata.lengt=				Set_DriverVe0
		e3 WPA_SUPAd->haredcase  0;
	PRTMP_ADAP3e);
	me%x\n"data, &wr1 Updapter))
		N(eAuthReq strTE_SET_PR *)ext11bstruct iw_encode_ext 2

  pen RRUPT_RA;
#itra)
{ta, ext)
  gnge->11gstruct iw_encode_ext 3ge->max_qual.level5(structLAG(pAdapter* Wha11ae = %d)\n", __F                pAdapte16    pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;t iw_encode_ext quand IVEIV table
	RTMPAddWc       arg);

INT S{
        keyIdx =->e2_encod_EXT > 17
	/*%x\n",_Idx]ructice  the Pair-wise Key tabe is down
	if(g.Defaul, aAlg;
pAdapterH_INDEX) {
	case IW_AUTH_WPA_VERSION:
            if (param->v
	range->max_qual. net_dev  {
	pAdapter->Comerage/       w_encod);

            //UsG)pAd->Rawn(pCfg.AuthModaultKeyICIPHER_NONE;
		AsicRemoveShIidAttributeEntry(pAdapter,
							  BSS0,
			TEM;
		bre
	case IW_AUT          	pter, fRTW_AUTH_ALG_OPEN_SYSTEM;
		breunNCODE_IND				  BSS0,
							  keyIdx,
						 + LEN_TKIP_Eue <= MAX_FRAG_THRESHOLD)
    retbj = (Pmlme *)wrqu->dat would be suital_siwfrarange *rang the interface iscanT Get K&qual[i], _OS_NETDE 0, (UCHAR)keyIdx);
        &qual[ixding-convevice our own24 valued k("==>rt8)
#defiE_INF		 sDq->length));
{
		u3                pAdapter->StaCfg.GroupCipher = Ndis802_11Encryption3E		if (!(erq->flags 
    }
    led;
  ess
		/AST;
#elLen; _11_BSSID_LIST_SCAN\n"));Ndis80K));
		     y(pAdapterIVACY_INVOKED - param->value = %        Nstruct iw_param *param = &wrqu->param;

    //check ifnge the mode */
		if (!(erq->flags 
    }
    NABLE) &&
	 pAd->Shardata[B IN  PRTP_RXMICK);int rt_ioctlBGPRINT(RT_DEruct iw_request_info *info,
IW_ENCODE_AatusLen; idx++Idx,
    IN  f (param->va (alg)TRACEy->PairwiseKey.Citer-al? */
	range->avg_qual.qua = 0; i <I		memset(&cr = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_param *param = &wrqu->param;

    //check if the interface is down
	iId));
        }

                pult:
            DBGPG		els-apte", (LCHAR | 1024, 2) + 7;d));
      b-ioctls definitions --- */%sdown
 pherAlsp						  BSS0,
							  keyIdx,
						r->SharedKey[BSS0][keyIdx].CipherAlg,
							  &pAdapter->Maed;
          Eep    LONG)p("%s::I    MGMT the inUPP;
	}
    DBGPRINT(RT_DEBUG_TRACE, ("rt_ioctl_ss
		FIRMWA30, faultKeyId = index;
            }
       dex < 4))
        FF;
                }
y index ? */
		int index = (erq->flaFirm    def DOT11_N_SUPPORT
 02.11g");ls definitions --- *0; /* >StaCfg.GroupCiOISEDefaultKeyId,ter->SharedKey[BSS0][keyIdxd));
     cidAttributeEntry(pAdapter,
							  BSS0,
						Bba, charLINK[66CHAR)keyIdx);
        NdisZaredKey[BSS0][keyIdx].CipherAlg = CIPHER_WEP128;
				}EP40			STA_ruct net_device OUP - param->valuBSS0][keyIdx].S0][pAd->StaCfg.DefaultKFixeETXPOW ||
					pAdapter->StaCfg.GroupCip= (erq->f[BSS0][pAbuteEntry(pAdapter,
							  BSS0,
						e_Proc},
erKey(
    IN  PRTMP_ENC pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_parUpdate WCIint rt_ioctl_siwee_Proc},
    LLAG(pAdapter, fRT2rd ops down!\n"UILD_CHAN_COMPILCNTL_aredKey[BSS0][keyIdx].KeyLen = MIN_WErate		returor  by , (ULONGset(BsstherACfg.Channel     case I if (encoding->flags & IW_ENCODE_DISABLED)
	{_ENCODE_ALG_TKIP:
st_ieKeyEntry index ? */
		int i       {
            pter->ShUX_VESUPPLICANT_S.CntlROC->naultKeyId].Key, e	((pAIV(dev);ng default k   {kell be free;<<u, cRT_DEBUG_TRACE,pAdap);
 flaK    2ynge->ENCODE_EXT_SET_TX_KEY)
   VI    3rt_iocfaultKeyId));
         O    4SIC WCID atteShared)
dapter->Sc    5yta.l keribute table and IVEIV table for this group key table
					RTMPAddWcidAttributeEntrST_FLAi*sizeof(addipherAlg, Key, NULL, NU		{
		uest_i = %d\n", __FUNCTION__, keyIBSSection_Proc},
printf(eIdx = %d\n		for (jEN);
taCf&MsgV	retu is dndicMAXh ? cap     i __FUST_FLB */
	range->max_qual.noisBSe
    end_ruct n -ENETDONDw = jiffies;

#ifdef WP		NPPLICANT_SUPPORT
		if ((_S, wrq->length));
r->StaC= IW_EN,
						   pAdapter->Scan== EntrpAdapultKlaram->RINT(RT_DEBUG_TRACE			   pAribute table and IVEIV table for this group key table
					RTMPAddWcidAttributeEntry(pAd						  pE	((pAdapter->StaCfge is connected  }
         ed;
#ifdef WPA_SUPal;

	//check if the intAux.Bssid, ETHonfigrtct net_device mily = ARPHTRING current net_devi	Stdx, ext->key_len));
              X_FREQUE(alg) {.BssEn) ?g.OrigWepALG_\n", __   {
_ADAPTER	pAd,
	IPTER	pAd,
	IN	PSTg);
2.1x = rts_OS_NETDEherARD_ETHER;
reD_Pro, end_b           //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURE}

#ifdap->leENTR end_buf;
	PSson_cHE}
    )
		PEN_SYSTEM;
		bi = 1; i <= range->numyIdx,
    IN  UCHAR NEW   br);nel;
 iw_mlme xtra)
{
	IG_CCMP:

     ixed = 1;

ipheredKey[BSS0][keyIdx].KeyLen = MIN_WEEINVAL;
             {
                         LG.Key, - ke/typical" qual? */
	r{"efuseBufferModeWriteBac_11Encryption3Enabled)
			pRadio = (pAd-> (param->vu_to_le16NETDOWN;
	}

iv will be NULpriv will be NULL in 2rd ops down!\n"));
	return -ENETDOWNdevRT_SECURED;
		        STA_PORT_SECURED(pAdapter);
CTION__, p  }
#endif //ar idx;
#P_OS_NETDEV_Indreshold;
	rts->disabled = (erAlg,
    IN  BOOLEAN         bGTK,
    IN  struct iw_encode_ext *ext)
{
   *extra)
{
	PRTMrtr))
	{
		ap_addr->sa_family = A;
    }

    memltKeyId].faultKeyId = index;
            }
    IN 6)\0'
    Staticate ther->St
mmonCfg.Ss  arg);

#ifdef      {
                if (pAdE
	if (ATEfo,
		   struc{
		 EXTRA_INFO_CLEAR;
                    d->BbpetryCount.+ 1; // 1: sld\n\n", (
}

#ifdef Ddev_EIOACE ,("Me3],
          usSharedKey[BS	   strCipheAlg,
t_CarrierDetect_Proc},
#endif // CARRIER_DETECTI      try .BssEn(_   pAdapter =UG_TRACE, ("EXT_p key mat Set[6  switch (alg) {
		y_len;

	DBGPRINT(6eak;


#ifdef QOS_DLS_SUPPORT
		case_qual.nois->StaCfg.bSwRadio))
            <= range->num_chanPPLI*wrqu,
			   cifdef CARRIE;
	range->max_fragf);
			for , 0, (iaStateSET_TX_KE       frag->fio juinfo *inDab.BssrADAPTER_APT_VE					sdxer->StaCCfg.Wede >=||ength>    re -ENETDOWN;
    BWdx--       _ENCOual? aCfg.WepStatus == Ndis802_11Encryption2Enabled) ||
			(p
C,
    MP_ADAPTER(pAd->	inA_802_1X_PORT_SECURED;
			STA_P  return -E2BNdis! para->S       Ad->StaCfg.Grou)
		  INT(iidx > 4)
		 500000;t iw_point         }*/T_DEBUG_NdisTB7, 54Set_P346;

#if WIRELESSiwfragfg.bRadio == TRUE)
         , __FUNC6)/10);
	else if ( ? (EX (ra_BELOW) :aptere = ABOVEturn -ENETDOWN;
	}

               sprintf(extra,=> rt_ioctl_giwenc>SharedKey[BSS0][pAd->rlen(extra) + 1; // 1: size of '\0'dapter->iRRUPTnY;
 dif

pAdapter-i = 1; i <= rangeR)keyIdxe_ext       if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
		{
 b-ioctls dtry->s
     up-95;
		Now;
	iNum;

tus = NDISPRIV(dev)Ndis802_11WEP;
	range->max_frag en <S = pAP_ADENCODltKeyId)
			{
1WEPEnableuality pryption2EnablPRIV(dev)BSS0][k* 1000000;
            else D_LIST_SCAN\n")); (			iwe.u%xame[Count);
	if (idx < 1 || idx > 4)
			return -E    lue & ~0x1); /* even numbers oRINT(RT_DEBUG_TRA = pAdapter->StaCfg.WepS      pAdapter->StaC->value));
            break;
	case Iabled;
#ifdef WPA_SUPPLICue == IW_AUTH_KEY_dis802_11Encr		brea         if (param->value == IW_AUTHY_INFO,
	  IW_PRIV_TYPE_CHAR       IW_AUTH_KEY_MGMT:
      Idx].CipherAlg = CIPHER_WEP128;entMode",				SeEXTRA_INFO_CLEAR;
             abled;
       break;
	case Inabled;
     GRORIV_SIZE_MASK, "bainfo" },
	{
    memset(extra, 0x00           8)|IV_SIZE_MASK, "bainfo" },
	{         }
           pAdapter->StaCfg.GroupCipher = Ndis802_11Encryptionntry->Channel>14= MAXak;
#e

    return Status;
}

#ifdef DOT1          Pair-wise Key table as ctl_siweiDAnt<pencode_ext AsicRemovaCfg.iEntrmax_       Entry->ExtRateL table
	RTMPAddWcUCT)]);
}

int r%x\n",extT(RT_DEBU_LEN_OF_RSNAdapCIPHER_NO,
	  IW-        ructth ? cap

	if (wrqu-tion2EnablPTER pAdapter =m, pAdR)keyIdx);
        NdisZeroMer->Sh ? cap.WepStaturqu->data.lec(MA_ADAPS>value info *Hs & I->StaCfg.WepStatu>StaCfg.OrigWepStr))
 	{"CNATIVE;

	tection_Proc},
	N_IE_FromWpaSupplicsEY));
    pAdapt // CCK
	12, dendif // NATIVE_WPA_SUEN_TKIP_RXMICK);
 ntry =&pAd-	encoding->fant = TRUE;
#endif // NATIVE_WPA_SU, parar this group keynfo *->StR)keyIdx);
RSNIE}

int r= RTMP_OS_NETDEV_GET_PR_DEBUeHER_NONE;
			{
		if (pength);
		//ifue = %d)\n", __F
                pAd	{ SHpAdaptere.u.bitrate.	pAdaWEcateMdeWPA2;
       else if (tm, pA
       >X_LEN_OF_RSNeturn -ENETDOWNiwe.u. SIOCGIWMODIE[0], MAX_LEN_OF_SIOCSIWGE

	if ((pAd->StaCfg.RSNIE_fail, .KeyLen(][idx)&V(dev);

	if ((pAd->StaCfg.RSN    emset(extreq_data *wrqu, char *ext
		return -ENETDOW	{ S>StaCfg.WpaSupplicantUP =
			STA_	{ SHOW_DSN_IE_FromWpaSupplic
	}
	else
	{
		pA	return -ENETDOWN;
	}DAPTER   pAd = RTMP_OS_NETndicateM_ADDR_LEN);PTER   pAd = RTMP_OS_NETt sockGET_PRIV = wrqu->data.3				if >StaCfg.AuthMode < Ndis802_11AuthModeWPA))
	{
	32 0;
	}

#ifdef NATIVEdef SIOCSIWGEMK[0		{
WGENIE
	if (pAd-T_DEBUG_Trn -ENETDOWN;
    }
rrReqIsF	encoding->flRadiate_pAdapteCIPHER_NONE;
		*info,
			        MULTIPLE_CARDNG)pAd->WlanCED;
		        STA_PORT_SECURED(pAdapINT              bred_poinicanct n_    + 2Cfg.RN__, pAdap2ter- |1-95;
	range-    (pAd->StaCfg.Auth->StaCf (wrqu->data.length && extra == NULsiwgixDoneter->Stif ((kid A_SUPPLICANT_SUPPORT
	pAd->StaCfg.bRSN_IE_FromWpaSupplicant = FALext(struct net_device *dev,
			  struct aSupplican(pAd->StaCfg.AuthEINVXXXiw_eiOTct i_Len;        retuDefaultKeyId;
     		returnt iw_param *frc},
	{"Frag;
        1TRACE ,("ConnStatus is not connected\n"));
		}
   ;
        case SHOW_DRVIER_VERION:
            sprint>key_len == MIN_WG_TRACE ,("ConnS= 0x84)
                iwe.u.biMANUFTH_R--ROU
#endi_DEAUTH //
#ifdef IW_MLMEE_DISASSOC
		case IW_MLr *esb-ioctls d = pAd->eniLED;
	}
	else if ((kiManufan", (rOUUCT);
	f (param->     apter->data.length urrent_RALINK_    e =     CaAM_A, _D, _,engththe keyabledPmksCommonCfg.BP_ADAPTER pAdapter =b-ioctl
rt_io= (char)RSNIe;
		extra[1] = pAd-pIW_P net_dev			{
			 IW_PLinkDN_SYSTEM;
		br
	case IW_AUTH_WPAdx =;
}

#iaultKeyId].(ChaodeWPAPSK)) &&h;
		NdisMoveMemsubcmd = wrq->flags;  wrq     key_lext->key_len));
           we.u.ap_addr.sainSidata->lengRESOURCN	PRDIDPMKSTRY ARGE< (pAcustoUG_TRACE ,("r; UG_TRACE , = cu.TxMiefaultKeyId].Key->KUG_TRACE AEntry =&pApter,
					)/10)reResou/

	mksadCommoifg.SavedPMK, sizeof(BSSID_INFO)*PMKID_NO);
			Dcmd = SIcustom, pAE_WPUG_TRACE ].     ntlMachine.Curr_DISASSOC;
			Ml>Statl_giwaOPn3EnIMPLEiw_fUTH_WPAUG_TRACE ,("===> rt_ioctl_siwpmksa\n"));
	switchCSIWGENIE //SavedE_WPidx{
		case IW802_11AutxanTab1 : 16, 0, MATab.E_MAC2        dx].+1]qualinCfg.Bssid[3],
BGPRIaCfg.Wealged) ?TRACE, ("RTMP_OS_NETthesparam->value =n 0;
}

int rt_ioctl_siwfran));
           g.SavedPMK[idx].ory(&pATRACE&p
intMANAGK[idxEntry->Su);
			brelMachine.CurrStatDBGPRINT(RT_DEBUG_THAR )\n", __FUNCAd->St     iwe.u.bitr2_11_BSSID_LISTus ==Sw, cbRadi      R_WEP_KEY.SavedPMK[ipULL) a>poin
			bre16);Cfg.Sa, ETqualiNdis802_11 2)) // eMedd->StaCfg.SavedPMK[iNumStaC idx < pAdapBssEntry>flags |= IEY_SIZE)
   State = ASSOC_IDLE;
			}
g(stedIdx = 0; Cachedndif /Ad->StaCfg.Sngth = 0;
		erq->flags =    (wrqu->data.length &&    DBGPRINTDE_ALG_NONE\n", __F  yId=%x, KeyLen {
		case IW//isval r, vau->dontrol
n")) 540.RSNI Usinc(MAX_LSTREIF] is down
	if(tra+staptchar, va(erq          nfo *inf//, __FUNCTION__, pA       eplaNdisMoveMemory(&pA[0]IVE_eturn->StaCfg.SaNTERRUaultKeyIdisEqualMnge->avg_qual.level = -6AUTH_WPA_VERSION:
            ifACY_INVOKED:;

INT SeN))
			  ++;
work SE;
	DBGPREXT_GROUuld be suitable values for "average/typical" q   if (NdisEqualMxtra+st11WEreg.Authon ,("[isEqualM]=nt = FpAd->StaCfg.SavedPMK[break;
	   _ADApter [char, va                              pAd->CommonCfg.Ssid,
FF- */  BSS_PORT_SECURED(pAdapter);
			pAdathes    keyIdx,
  ("gs & IW_ENCODE_IVE_->ate.ct i    mks*dev,qu->daKeyne
		 S				ls definitions -us_evf(iweEnt_val;
	STRING cMLME stG_OFFx = 0; Cach	arg);
#endif // ANPMKID_NO)
	      nd			rg.Auth		speyIdx >ipherAlg,
   CipherAlg,
	        if (CachedIdx < PMKID_NO0qu->dctl_        if (CachedIdx < PMKID_NO1/E_STw_param *param = &wreshold",			set(&2 int
rt_private_ioctl_bbp(struct net_devi3 int
rt_BSS0][keyIdx].ter->SharedKey[BSS0STRICTED;	we.u.bitrate.gth);
		//if ((kid == pArAlg = CIPHER_WEP128;d <=4hat woul.PMKIp    .bRSN_IE_FromWp_802_11_BSSID_LIST_SCAN\g.SavedPMK, sizeof(BSSID_INFO)*PMKID_NO);
			DBGPRINter->SharedKey[y index ? */
		int           //pAdapter->StaCfg.PortSecurgs & IW_ENCODElientMode",		d));

            /       case ange, 0, sf //gBB SIO0;
//2347MAPPINGLEt_ATlue = IVERSITY_SStaC *devdapter->StaCdif //arameters.
		nCfg.CentralChan{
		case IW_PMKSA_FLUSH:
			Ndis	range-d = WPA_802_1X_POR, sizeof(BSSID_INFO)*PMKID_NO);
			x = 0; CacTRACE ,("Cfg.Savedb>keykey in tg defpAdaID_NOOCSIW indicags;

	pA_ADDR_LE		*value+ // Found,K[CachedIdx], ("Update OPSTATUS_TEST_FG_OFFidAttributeEntry(pAdapter,
							  BSS0,
			
          else 55]={Port	 bre3IW_P	bbpoctl)) == 1)
					th             {
			pAdapter->StaCfg.DefaultKeyId = index;
     NdisMoveMemory=BGPRINT(RT_DEBUG_Tf(!WEP_KEY_SIme[data->length-1] = '\0';
	}
	reBuf, "%sTID	am;
2EnabBBP_IO_READ8_BY_REG_ID(p key is notPRTMP_AerAlg = CIPHER_Was invalid */
		if(!(erq->flagBSS0][kelse
    end_ruct net_.CipherAlg = CipherAlg;lags & IW_ENCODE_MX) - 1;
		)
		{ //Read
			DBGPRINT(RT_DEBUG_TRAC, extra, erq->;
#ifdef WPA_Sndex ? */
		int in %ld\n", (ULONG)pA(RT_DEBUG_TRACE		//erq->fge->mruct  =&pAd->BAT	{
		
	{
		/*BBP_IEntr	{//InsmittedFragmentCou000;
			uadPart -E))
	{
		DBcheck if theE defauO_[0].8_Btus"ODE_ENABL/*ame[data->length-1] = '\0';
	}
	ro f(exfail, pAd UPPORT
	pAd-Buf, "%sTIDInvalid para
			{ //          //Usin		strcpy(	ld\n"			bIsPriEVENTralChannel);
     TER   pAd;%02d[	  I2X] pEnt    kb          //Using default ke ("INFO::NCfg.SavedPMK[idx].n"));
			breakAODUCTCache, "\n");

		//Parsing Read or Write
		this_char = tAllBBP =255]={0}(ChannelcanTab2.422down
{(
  	 (__   sp = -95_802_IV(dev);
(IV_SIZE_MAn -E2Bdefaoo On)->pci keyfg.Oradio)Key 
	  read_%d!\n"_
		 L;
  }
			else
			{ //G;
		eyLen = 0;bxit I   struc
			&oid w2BIG/Bit Ratebe free;= IWE_STREAM_ADD_VALUE(info, 			   bId, &r  DBGPRINT(TRING				thi	{
		urn -"%04qu->4pObj;
NI
#ifdeVENDORIP_TX_ATE //
					{
				elLatch[    nd Frequ->StaCfavedPMK[CachedIdx].PMKID, 16}
			else
		luding#definN)
			IsPtralAllrameteruct iw_    }

>data.length = pA_siwpmksa - IW_PMKSA_FLUSH\n"));
			break;
		case IWTE_ON(pAdapter))
					{
						ATE_BBP_IO_WRITE8_BY_REG_g=);
			M is d->StaCfg.SavedPMK[CachedIdx].PMKID, 16Ad->StaCfg.Savedlue == IWfg.WepStay(pAd,
	his_char, '=')) != NASSOC:
_ID)
				{
#ifdeisMoveMemory(&pAd->StaCfg.SavedPMK[id		int index = aultKeyId)) keyIdx, pAn(extof '\0dapte{
		A= (struct iw_mlme *pAd->StaCfg.t pr//.WepStitions --- */ wilCachelt printASK);
		sSK, "show" },
	{ SHOW_ADHOCUPPLICANT_E_u->daCfg.bRa         sprintf(extra("%s::Remove all keys!(ecase IWcidAttributeEntry(pAdapter,
							  BSS0,
							  keyIdx,\n", pOutP_MAC_PCI //
	re       //pAdapter->StaCfg.PortSecurTL_RF;K_ATE //
E_ENTRY
	->reason_cpAdaptermerq->length =W_ENCODE_ALG_CC IW_T_PRO(pOutBuf,EXStaCf)
		{
			iwoctls W_MLME_DEAUTH //
#ifdef IW_MLMEE_DISASSO RALINK_ATE //
					{
					LSE;
	DBGPR W_PRIV_SIZE_MASK);
		sprintf(extra, CAP_INFOXTE //
					{
			 IW_OM_LEN);
			memcpy(custom, &(pAda, 0, MA (sscanf(v%03Cfg.AbpVa.  DBGPRINT(wrqu->dairwistf(extra,ag = 2346;

#if WIRELESS_E
oesinkr		}
			.al =g -\n"
#encasemdaptetion2Enable);
				 break;	//  Dchange display format
->StaCfg  break;
		ocMachine.CurrState = ASSOC_IDLE;wrqu-;
#e      INK_ATE //
					{
					   {
		 shohis_cOCSIWGENIE
 IW_{
			/* Copy the key in the driGble
HONG)pAdm);
     bRREQ_ay format
		{
		pChDeltx
	{"  {
#ifdef DOT11_N_SUPPORT
   lue, "%x", &(bbpV		el
	val = 0;, regBBP);
	efaultKeyI ,("MeDeltNu		gottate = ASSOC_I		break;
	case IW_AUTADDRESS)    return -E);
   	if(!RTMP_ipherK	        // ngth > pAdapter->StaCfg.AutP_TEST_FLAG(pAd, 	return -EINVAL;

	i 0)
			{
("INFO::Netoctl_siwrate    gs & Ief IWEVGfix   pr!\n"));
		        }IV(dev);

	if (pAda down!\n"));
	revapherKeywn!\n"));
	res ---  available)      nitions --- */tra)
{
	PRT, char *extrad        /*we.u.q= -1 [i is te::	}

	fg.Au,AL;

 + 7;
	nt
rt_iOW_ADHOC_ENTRY_INFO:
			ShowrModfree;
		   So theefaultKeyId].Key,
							  MK, 

#defrAlg, Key, N     rate            =e
        pAd->StaIW_MLME_DEAUTH //
#ifdef IW_MLME	Cioctl_siwrate(struct net  /* rate = -1 => wl);
%d\AUTH_WPAase   ret	ULONG								Now;
	int Status = NDIS_STARIV_TYPCSA6MbpavedPMK[CachedIdx].BSSID, MAC_ADDR_LEsocReqActSet (pAd-d*****(ef W&parame)else
				{//Invalid perage/open fail, pAreak;
	         all bbp
					bIsPrinllBBP = TRUw_requCSECUR2// RALINK_ATE //
					{
					   _ioctl_siwrate(struct net_deS_NETDE;
	int Status = NDIS_STATntry->GEOGRAPH("INFOP104)
            {
                pAdapter->StaC 		case   retWC    chedIdx++)
			{
		        // ,, ext);

                        // set 802.1x port control
		  Gd->StphaCfgfg.AuthMode =lue == IWYPE_CHAR | 1024,annel);
<        GcanTksa->bssN] = {0};
#ifndef IedKey[BSS0][try again.
		 roc},
	IN  PRTuct net_device *dev,
			  sF;
                }
EqualMemory(pPmksa->bssid.sa_data, pVOKED:
    ->SharIWE_STREAM_ADD_POI/=====           //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECUREDL         elsa_family = ARPHcharE, (,e = ESS_ADDR_LEN))
		        {
				Nds    se
 
		Ndisintf
	case IW_ssNr == 0)
value Dlsrqu->dadapter->StaCfg.AuthMode =   NdisM)Key = ATOMIC    /* rate ES, FALSEoctl_siwrate%d)\n", *mod       rq->l = strlen(point *data, char *extE;
		int raTER   pAd;SSIES, FALion_eters.ORT_SECURED;
		ScanTime =            = %NeyId ter->Sta"====(dev);

 + data  ndiaSakey_len = 0;
	switchEntry:
	i-1.Bssid[2],
     //ing_ Rats(pAd, reak;

	c_TKIP_HT(pAdated;
}.Bss, (" -ENETr ad=, 12

		MlmAES, FAL        else if (tmpR)
#if WIRELESS_EXT >= 17				if onX, turn eter =>		}

	ioctl_giwrate(sr, bbpId,ke(pAd, ftruct edRaD].HT  *
 ING     dapter->StaCfg.AuthModeurn -Eval].e = >len_LENde == Ndis802_11AuthModeRESTRICTED;	_IO_RRINT(RT_DEBUG_TRACE, ("rt_ioctl_siwratIZE;
                    pAdapter->SharedKey[BSS0][keyIdx].Ci	er, 0, (,))
		{
		* XXX */
		erZE;
                    pAdapter->SharedKey[BSS0][keyIdx]		/* Check  29,MLME_ t iw_r(extra, &p40, // 40MHz, IW_truct iw_rSTRING				this_if (param->value [i-1].Channel pOud paraNB:dPart -= IW, 433,									*info,
			         D;struct iw_rpter = RTMP_OS_iwe;== NULL)
	{
		)
{
	PRetus = ctl_ize[0] = 5;
	rangefg.WpaSup;
      a_iSOC
(    / OFDM
	net	   ice	*, 433,	rn UC of torkTypedapte	POS_CO// 
}

int ar = ex	if // RALI/ The

	{ult prchar *ex What wouAY;
    UCHcs=%endif
 value;
	intdapter = RTMP_OS_NETDEV_GETeplace it	iwe.u.q.al.noise = 0;CK(extra) (REAM_ADDR	pAd,
	IN	PSTRING			arg);
#end*
 * U;
			, 300dapte"INFO:	   So  0 ~DEVLESS_    (, 433,	 -EN NdisMoizeof(
rt_i %ld\nMP_R1pEntLAR PURPOSE_SIZtralbWE_STd02_1X_PS>Pairw, 433,	-> it           shote[]erfa11_N_    T---------		}
OWn * o *in//bitraf // RALINs=%d)\ //
					))
	{
	//t_infoifPairwis(dev (algs wsca;

#iEG_ISIZE_MASK,
  "stat"},
{ RTPRIV_IOCIr=%sRUP(pAd> 0)f // NAent_ev,end fixedD;
     DE 
	memset(range, "Infra");
			b        _PRIV(dePRe == IW_AUTH70, 360, 540, 7, &pAda ~ 104, 11str Softry        = %ld\n", "Op }
 ")TMIse
     0;
	PRTMPyLength);

				// SeZeroMtRI_ENTRY Ndis802_11A");
		for (bbpId = 0; bbpId s GI:pAdapteksa->bss2.11a/n");
	.CipherAlg 	{
	X */
pStatus = pAdapset{avaideterm  PRT486,value 802mlablisCfg.Smoryse Iuwhig.AuCommand!anne))
 ->
#ifde
	//      if tMRTMP_4, 117, 130T
  =16, 2IN_'=')) _162,, "bainfo" } = (pommonVal[keyIy->Kapter-htetting. iw_reTRY g.Group    _.WepS, 11r == E_MASKRtmpDoAt MAC_TAwrq_WmmCapable_Proc(
	IN	PRTMP_E*wrqu	PSTCCK)| erqif(!RTMP_TESate_WepSal = 0;
	fock fopAdaptFHW	{"FStaCfg.SavedPMK[CachedIdx].BSSID.WepS::te_index >x =, &(bbpId) *16) +Cfg.Sav(dev);("Update PMKID, idx =cannessd.BWdown
	if(!RTMP_TESP_TESif /turn -ENETDAR | 0MHz,->HtC&Entry[i]wrq, (LO		rKey[BSSnt rif (fg.APortreturn 0(dev);
	PyIdx        CommonCfg.Bssnet_NULL      iwEdes ng.HTgs &, 180.ap_rt_ha/*
      *
 *E.f (pa*end_d*****/*
 Wy[i]	elsRTMP_ADAPTERhandl;
	elsCOMMIT, 18handlhandl *				     >Sca]);
}

int rR_WEPd parapAdapte *de(iw_********_SUP *****onst i6, 48,  SIOCSSI	N/
	(iw_*********/*
 ********d
		i }
         SIOCSIWFRSIOCGIWFREQ   */
	(iw_h  DBGPRINT(RT_DEr, i*siq,		    /E, (";
		W*****NT(info,n::Netword (     ell    IOCGIWNAME  _Gk suppl  gFREQ   */
	(i, pAd will bels
}

static _point *e== NULL         /* IOCSIWET_PR ,("Me/apteuency (Hziwreq	*wrq = CIPHERt, w*  */   */
	(i(iw_handler) 	(iw_ha  */   /* SIOCGIWNWI  */
	range->OP_q,		    /* SIOCSIWFREQE PRINQ   */
SENS ******wmod   stru_DEBID aode,		   RANGEIOCSIWMODE   */
	(iw_handlerge->encogtl_giwmode,		G code */,    /* SIOCGIWPRI/
	(iw_handler) NULLRaliICKN:*/
	(iwnCfg.Same/rn -EIO;MCS: 0 f);
_giwmode,		   N_OFF   */
	(i_OFFer;
	M  */
	(iw_ha char   /* SIOCGIWNWI_OFFEQ   */
	(iw_handl /*/* kerneed */,		/xx pEntwiIV   sKey = wmodkersent	pKey = (PCHAn 0;
}
ctl_giwmode
			   r->ni	IWTHRSFREQ   */
	(iw	}
	else
#end{ RAValue))_ioctlPY */
	siwras=%d)\n",pAdO;pter,
    IN  PSTRING          arg);
     ruct iw_request_infdev);       _han iw_request_sta not used */,		/  {
 OCSIWIV  LG_NONE\n", 	KeyI(io);se
	ate_IOCSIWRANGE	Key   /* SIOCGIWNWI  */
	(SIOCGIWPRIV   */
	(iw_,		     {
				pAdratSE SIOCSIW/
	(iwtra)
{
	PRTMPl	   odeext(struct n */
	(iw_haGIWFR.u.freqQ   */
	(iw_handler) achedIdx].ode,		   MLMld\n\n", (LONGnst iwRTSv);
	RIV  RTS/e)  t2.422G,

(tralCL /* kernel code *p++)
 *rts   */
	(iwt17, 1RIV(dev);
	s ts   /* SIOCGIWNWIrtsLL,				        /* SIOCSIWMLME	/*_siwscan,		    /*PRIV(devSCAIW_P   /* SIOCGIWPRIV   */
	(iw_charctl_giwmode,		GSCAN _PMKIWAPLIST */
#ifdef SIRaliWSCAN
	(iw_handler) rt_iosiwessid,	*/
#ifdef SIOCGIWSCAN
	(iw_handler) /* kAGv);
	IV  fel = chd!\n",thIWSCAN */
	(iw_handler) rt_ioctl_wnial =iwrange,aate_*/
#ifdef SIOCGagL /* not used */,	a }
       GIWFREQ   */
	(iw_handler) 	(iw_handler) rt(iw_handlIW_P_iockhandler) rt_iocWSTAx_ge/,    /* SIOCGIWPRIV   */
	(iw_ /* -- hole --    *Gr, i*_handler) rt_ioctl_si-    */
	(iw_handler-- holeIWE_STRid,		    /* SIOCME
	N
#enA)
{
	PRTG;
	fg.AuthManCou_CIPHER_ioctl_siwmode,		WSTAWr */
	(iw_hctl_giwmioctif****     andler) r -EOPN	/* PLIST */
#ifdeferql_giwmo */
	(iw_handler) rt_ioctl_giwmod */
	(iw_handler) NUtra)
{
	PRTIW_Pctl_giwmode,		   R)         /* SIOCGIWPRIV   */
	(iw_htsandler) rt_ioctl_) NULL,		        MCSrange,		GPRINT(RT_agctl_giwmode,		    /A    CSIWMODE   */
	(iw_hdeext(struct nAP */
* SIOCGIaprint if (paMAC260,am->vaSS; kernel cod->Scj = (Papng */
  */
	(iler) rtdler) rt_ioctl_Nap   /* SIOCGIWNWIler) rtler) rt_iLME_C_OFFnickj = (P// Updat	(iw_handler) rt* SIOer) NU(iw_handler)Ndis802_11Encryptiw_handler    L,		                /* SIOCSce *dev,
L /* kL,				 trate.v    /* SIOCGIWPRIV   */
	(iw_%x\n",ioctl_siwfreq,		    /* SIOCSIWFREQE  Entry*/
#ifdefooctl_    ) NULL,		  __		pO*ntryler) rt_iength = RIV(dev);
	sntry   /* SIOCGIWNWI		}
		IWSCAN   */
	(iw_handler) NULLSIOCSIWFREQ CGIWRASIOCGIIW_Pram *param = &wNULL)octl_giwrange,		] = pAd-> IW_pen */
		Y)\nRalink ::WepL,		                /* SIOCSIWTTH   */
	(iw_hWPOWER  *tl_sENctl_
{
	PRTra),->ivqu->(dBX_PORWPOWER  */
G     */, xt_ioctlndler) rt_      
			id,	GIWedPMKeMediaStats WNT(R-age= chamory(pAalg)EEXT */
	(iweext,		/* (iw_iwauth,		    /* SIOCSIWAUachedpen */
* SIFromEEXT */
	(E_WPA_Sault priodeext,		/* SIOC(iwiw_h int
s32)rersTxBurst"ler) rt_ioctl_giwr /*MSIOCNGEiwral_gipAdaMfg.Bw_handler        /tl_giwn2_11WElerIV  xtra			/*))we, } *ifetie_PRI   /* SIOCGIGULL, /* (iw_	(iw_hThis proSIOCSIbbp*****	(iw_handler)      /* SIOCSIWPOWER  */
SR *wrqF Netwo, ("Update PMKID, eak;
NetworEXs(pAd,0, 300, 6Cfg.SavedPMKurn e
	(i;if(173, 260&ALG:
  be fT_TOGGnly=FALSpId, &F;
                }
*   casdef DOT11;
		Nd           else ifndleQueryInformation(pAd, rq, subcmd);
			break****case SIOCGIWPRIV:****if (wrq->u.data.pointer)****{**** 5F., access_ok(VERIFY_WRITE,  5F.,
 * Ralink Tec, sizeof(privtab)) != TRUEh Inc * 5F., ***		i City,
 * Hlength =ounty 302,y,
 Ta /ink Technology,[0]*
 * (c) Cocopy_to_user(c) Coy,
 * Hsinchu Conology,Cink Technology, i.C.
 *
Status = -EFAULTc) Co}*
 * (c) Coc LicenRGNU G_IOCTL_SEGNU Ge**(o.36,it uyuan St.READbei CityologyHsinchu Coright 2002-2007, Riwan, R.Onder th * (c) Coprt_ioctl_setparam(net_dev, NULL        u can redistribute hral Public Licens**e as publisheGSITESURVEYicense MPIany GetSiteSurveyc Licewrqt yo          **#ifdef DBG         hope that itMAC hope that it will bat yoThis phe hope that it TY; without E2PNTY; without evenROM *
 * bbut WITHOUT ANYistribuTY; _RF_RW_SUPPORTUT ANY WARRANTY; withRFICULAR PURPOSE. y of         t yoMERCHAendif //LITY or FITNESS FOR //lic Licenseted in 
rogrenera        ETHTOOL:NU General                default     DBGPRINT(RT_DEBUG_ERROR, ("     ::unknown      's cmd = 0x%08x\n",opy )      e terms*
 *OPNOTetail Public Lice}      if(c LieMachineTouched)    Upper layer sent a MLME-related op you cas
     _  *
_HANDLER    );

	returnic Lice;
}

/      =General59 Temple Place - Suite 330, Boston, MA  02111-13enseUSA.NTY; without eDescripF., N         Set SSIDRPOSE.RTICULAR PURPOSE..O if all r vereters are OK, FALSE otherwise GNU General9 Temple Place - Suite 330, Boston, MA  02111-1307, USA.            */
INR A Pion, _Proc(     IN  PndatiADAPTE    pAdapte i:oundablisSTRINd in the hoarg)
{     NDIS_802_11   AbTY; without-----     sid, *p----=  Ab;     BOOLEANatounda---- --    ------------if not, wrte 3to t =      ------int--------y Chen   02-14-2005    mosuhe Fr      *; p    am; strlen(ho   <= MAX_LEN_O  SeAb)ANTY; without eWdisZeroMemory(&y Chennk TechWhentern ULONG hl Puy Chen  *****nclude	"rt_!= 0DBG
ex(40/tern ULONG WEP it wovebugLevey Ch.y Chenarg,#P_SMALL_KEY			4
#defie NRy Che)

#dLense, HsiOUP_KEY_NO          }KERNEL_VEel*****//ANY ssidKERNEL_VE(2,6,2 WEP_LAR 4

#if LINUX_VERS0;
T61
*memcp(104/8#if Lef"", 0     utines

->StaCfg.BssType = BSS_INFRA _E)ON(2,6,2 IWE_STREAuthMod_POI          ream_addOpen KERNEL_VE, _D, _E)		iwe_stWepc Licen_ink T(_A, _BEncry   *
 isabledtt WI _D, _E)	-----= l;
#e RT61
*EVENT(_A, _D, _E)	Mlme.CntlRory Ch.Currounda.h"
CNTL_IDLE		(40_A,  _D, _E)_A, _B,L reln, MIRESET_STATE wilHINED, _E)		iHOUT ANY WARRANTY; without e  *
 TRACEu sh!!!strea busy, resetB, _C,sfine mory Ch !!!\n"OTY; without}

s freeD, _E)		iwe_ADD_VALpaPassPhraseLen >= 8) && *
 _F)	iwe_st, _C, _D_value(_efine  _D<= 64underer t n Histopasspefinexter[65] = {0neral UCHAR keyMaterial[40] without evY.h"
			(1ct PACKED _RT_,)		iwe_strMeam_add_   CipherWpasionX;ounda   UCH    *
 riverVerLen          RTDLEN			(1, _D, _E)		iwe_stPMK, 32      , _D, _E)		iwen UCHAR    CipherWpa2Tem= Fre[C.
 typed#elsAtoH((oef str)ionY;
    UCHAR       DriverVersionY;
    UCHAR      Utl.c

   neral27)
ildDay;
} RT_ CipwordHashION_A, O, *PRT	iwe_024, 0,
;

struct iw_prit(_A, _B, _ _C, _D, _E)		i,HAR       Dr       _B,GE_K    UCHAR  v_args 2,
 * T[SION_
{IV_TYPE_CHAR{ RT****ublis/* -ogr _E)		iwe_strM_strAuxD, _EReqIsFromRIV_support R _D, _E)		iwe_strUCHAR   bScanE_  UCH| WebUI-03Lice3  _C, _D, _E)	bConfigChang01-03W_PRIV_F)
# 102_strEnqueue ""},        Re */
 SIZE_MASK, , _E),, _C_CHAR | IW_PR| IW_RIV_SIZE_MASK, ""O       EP_KEYSW_BA_INFO,
	  IW_PRIV_if _B, _ _E)NR__A, KEYS	W_BA_INFO,
	  IW_PRIV_(VTYPE*)efine------------oundaRory Cine NRRIV_TA_INFO,TYP, "cDrVer"	f

extern UCHAR ink 
oundaAbstrac::(Len=%d,    -%s)U GenconnSt1024sion_P, _C, _D, (_A, _B,  Ciphed b,5AR |modify   0sup
	{ SHOT61
*/
 ;     Rory Chen   01h********PRIV_TYPE_CHAR | IneralFree Softw*
 *FouC, _D, _E)nc.,PRIV_TYd_poNT(_RIV_rPRIV_TY_off" }HOW_DLNTABILWMMdetails. CHAR | IW_PRIV9 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *            BG
extern ULONG mmCap_C,  En
	  Ior ern UCHT(_A, _B, _C, _D, _E)		iwe_streendif // QOS_DLS_"show" },
	{ SHOW_ADHOC_ENTRY_INFO,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV   { RModule Name   Revsta) any .	   {VALUE,PRIVO_OFFt:	IN	TL  * Free sum_ad,sub-iN_INFO,				"rt_  	y Chen 	bntry" },
/  AbsBBP,
 PRIV HsinmplerVertolL_KE, 0, 1_SIZ
 _E, _ IW_PRIVonnSt= 1uild_MASpAd->CommonIZE_M | IW_PRIV_SIW_PRIVhed bRIV_WMAC,
A_INFO,
	 0KE_CH"bbpE_CH --- */
    {L_MAC,

	{ SHOW_MASKoundat    	{ SHRIV_InvalidV_SIument   { Rendif // QOS_DLnfo" }F_RW_S{ntryV_SIZ/*   4::_PRIV_S, IW_=%dW_PRIIV_TYPE_CHAR | 1024,
  "mac"}(_A, _B, _C, W_PRIV}           CHAR | IW_P_PRIV_SIZE_MASK, "IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "dlsentryinfo" },
#endif // QOS_DLS_SUPPORT //
	{ SHOW_CFG_Network am_a(InfraCTL_SHure/Adhocadio    
ex_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASE_MASK, "show" },
	{ SHOW_ADHOC_ENTRY_INFO,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "achocEnIV_IOCam_a* --- s  Revisioctls relatiobro
#define| IW_Pvisionf strury96, 10WG
{ RTP = {
{ R32	ValuAC,
0O_ON },
/IWstrcmpSK, "I"SK, 
") | 1024,typed DriverB_MASMonth;
 PRIV_SIP!_SUPPOADHOC];

typed// , 260, has c| 1024privDRVIER	iweI, 260, //HAR | IW_PRIVRIV_SIZE_MAS, //MONITOR_ON312, 3_SUPe_stream_a6, 48EVE 390, _E,,erWpa2TemIO., Jhu32| IW_PRIV_SRX_FILTR_CFG, STANORMA         							00ns GI, MCSradi 15
	81, 162,MAC_SYS_CTRL, &6, 20	  Iis ,   48&= (~0x8_SIZE_ = {
{0MHz: 0 ~0ns GI, MCS: 16 ~ 23
	14it a   43, 0,
 ,   81, 108, P_E)	US_CLEAR_FLAGs GI, MCS: fOPV_SIZUS_MEDIAV_SIZE_CONNECTEDz, 4		  // 40MHz: 0_MAC,
A_INFO,
	  IAutoReconnectIOCTL_MAC,
1, 108, 6W_PR2LinkDow"show, MCS: "showMHz, 400ns GI, IOCTL, //MHz, CS:  216UINTW_PRdDay;
}5//y Chen  CancelCHAR  defi 2				1024, 9, 120W_PR8fine to prevOW_DLt rID_Proc(0to old							900};

ince calling d_valindicine e; y don't wanines
ubroutines
th | 1024,anymore.IOCTSne IUSns GI, MCS: et_    stractoundatIW 3_Pro900  UINT        D****
ub-iPn Hist			ho  , _E Lic

INsionY;
    UADAPTER   pAdapter,
    IT S FITN			 pAda, 240, 30, 14   I5  PRstrac | IW_PRIV 16 ~ 23
	30, 60,fine , 3_SSI433,		DIS		900}; ETMP_ABBtry"40MH/ZE_MAS S_C, _D, _E)		iwe_str 312, 3_SUPPO,				er,
    IN  PSTRING ion    ->tSTRING, _D, _E)		iwe_stOriDevam_a     r,
    IN  PSTRING        ===> 20M	90, e Fr48, 72,::(AD-     IOCTRTM_SIZE_MASK0,00};
0MHz, 8		  /0, 3,   87, 115, 39, 78,  117, 15,   34, 312, 351);

IMHz,     ypType_,
    IN  PSTs GI, MCS, 260, 2827TER  20MI, MC08 MCS: 0 ~ 152Hz, 27PTER _Key3_Proc(
    I
	90,432Key16TER   p// 4r,
    IN  PSTRING   7, 115, 173, 230IN  PR    INRING6y1_Pro9, 810     ER   pAdapt  arg);

INT Set_Key4_, 260, 288, //9 20MHz,  57,  8_Key15, 10, B14    IN 5IN  PER   pAdap7 PRT0, B20, 1288apter,
    I, 400ns GI, MCS:, 115, Hz, ER  apter,
 arg)0, 13t_Ke

INT Set_EPSTRING      e_Proc(
    IN  PRTMP_, 260, 280, B0, 1et_SSID_Proc(  IN  PRTMP_A30N  PRTMTER   pAdapter,TMP_ADAPTER   p6UCHA       arg, 400narg);   87, 115, _SSIDpAdapMP_A

t_AuthM	arg);
#endif

INTPRTMP_ADP_ated subroutines
rF_RW_SPRTMPn History:
    Who  ;C, _DE_CHWMMNESS FOR_Aut		PRTVALUE,
ble#endif
RTMP_PSTRING			ar	IN  PRTMP_ADAPTER   pAdapter,
    IIN	PRTV_TYPE_ADD_DAPTER	pAd,
	IN	PSTRING			arg);
#endif // WPA_SUPPLICANT_SUPPORT //

#ifd2PROM(
 ream_adMP_ADAPTER   pAdapter,
    IN  stTRING			arg);
#endif // WPA_SUPPLICANT_SUPMHz, 8 //


NDIS_STATD      Key_ADAPTER	pAd,
	IN	PSTRING			arg);
#endif // WPA_SUPPLICANT_SUPPORT //

#ifd2PROM(
 Key1#endif

ated sapter,
	IN	stg);
#endif // WPA_SUPPLMonitor,   87, 1, 540,{
    UC	bbp  87, Proc(			BCN_TIiverFGwe_sUC csrNESS DAPTER   pAdapter,
    IN  PSTRING       s GI, MCoc(
    IN  PRTDAPTER   pAdapter,
    IN  PSTRING       ANTNESS TRING	DAPTER   RIV_er,
    IN  PSTRING          arg);

#ifdef RT3090
900};
d_CHAR | 1024eriofdefr,
    IN  PS          arg)IN  PRTMP_ADAPTER   pAdaifdef RTMort(_C, _D "radlme	IN	PSTRING			arg)a2TemrVer"  pAdapter,
    IN  PSTRING    ndif // WPA_SUPPLICANT_SUPPORT ING          arg);

#ifdef RT30 IN  PRTt_Fra WMMlsDriverBuildYearCHAR | 102Cent 400IW_neld in the ho IN  PSTREO,
	  ItOT11_Ndetails. RIV_SIZE_MASK, " Licens CARR 23
DETECTIOPhynfo" }= PHYz, 8N_MIXEuted
exINT Set_PCIePSLevel_Proc(


#ifdef RNNESS FOR //ORT
et_Key 400ns GI, MCS:_MASK_TYPE_CHAR R	pAdapter,
	IRIV_I				2-14RT
{, "radi	IN	structndif //ub-iCTL_SHOW,req	*wrdapter,
 			eRT}MP_ADAPTER	pAdapter,
	IN	struct iwreIW_PRIV_radiorouti,
 N_ iwreq	Check PSTRING    truct iwreq	RT
INT SeBeaconLoMP_ADAPTER	pAdapter,
	IN     Driver117, 1fdef DBG
PSTRIr FI>ID RTMPIoBGFO,
	RT _F)mMP_ADAPTER   pAdapter,
    IN  struRegTransmitSetting.field.BWITNEBW_40(
    IN  PRTMP_ADA// WPA_SUPPLICANT_SUPPORT /

#ifdef DBG
R	pADIEXTIN  W_SAdapte_ABOV*
 * (_S IW_PR(  arg ,control PSTReq	*at lower    IN  PSBB  arg);

8_B "raG_IDs GI, MCS: *setR4, & arg)DOT,
    I arg);
}1 pAdapt18 "setMP_INFOATE_|f th1	struc	2PRO(, PS_pr: 0 ~(
    IN  PRTMADAPTER  PPLICAN	pAd,
	} RTMP_PTER   pAdapiteSurvey_BP    entiwe_TY);

etwork};
 RX :VALU/* -_SHO{
P_ADAPTE name; pAd{
	{"Driveoc)(ion",				Set_DriverVersion_3Regi},
	{"CountryRRIVATE_SUdefiPRO2r,
    IN  PS{"ruct iw_priv_"arg);(
   uct iw_priv__3roc},
	{" mod    IN  PSTRI   IN  PSTRING   TX_BANDTER   IN  PRTMP_ADAPTER   pA0xft_Procndif  IN  PSTRING          arg);
st_ProcTxPowe		Set_TxtryReg_TxBurst_Proc modi,
    IN  PSTRING PRTMP_ADAPTER   pAd,t_Count+// 2, 400ns GI, MCS: sicSwitchshold",s GI, MCS:  PRTMP_ADAPTER   pAd,
    IN  PSTR
	IN	PSTRING	oldstracLockribut
	IN	PID RTMPIo	{"HtBwBursIV_TYPE_CHAR | I	PRTroc(
    IN  PRTMP_A,
    IN  PSTRING          
	{"CPatic/* -_et_Coun(%d), 	struct iwreq	tMcsARGEW_BA_INFO,
	  IW_PRIV_cs_PMpduDensityc},
	{"HtMcs",				SeFragThredef DOT     tExtcha  Set_HtMpdtMpduDensity",		              MpduDensity"    pe that it wRING			arR | IW_PRIegion",				Set_CountForceTxBurstDAPTER	pAd,
	IN	PSTRING			arg);
#e,tenna_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PUCHAR			ar},

{,
	{"CTMPIoctlMAC(
Antenna				SADAPTER	pAdapter,
	IN	struct iwreq	   UCR   pAdapter,
 			eANTBELOWect_Proc},
	{wreq	st_HtOpModSet_CountryRLITY ABand_Proc},
	{"SSID",						Set_SSID_Proc},
	{"WHistossMode",				Set_WirelessMode_PC,t_Forc_Wirelessc},
	{ORT /b-ioctl	st",					Set_TxBurst_Proc},
	{"TxPreamb Set_HtMpdn_Proc},
	{"RTSThreshold",		c},
	{"R"IEEE80211H",					MP_ADAPT				SeGProterBurst_Set_BGProter"IEEE80211H"hannelPORT
	Set_A"IEEE80211HBGProtec_D, Burst_ProcypType",				_PrABan   Set_HtMpd    kType_P_Procg);
#endi				SeWirelessTMPWBurst_Procey1",		|= (        Set_N		SetkType_Proc},
		Set_HtAu   Set_Ne, _CblSet_Key1_PrTx	Set_KeyRTSsity_Proc},efaultKey4_Proc},
	{  Set_HtMpduDensity_Proc},	-aultKeuDensity_Proc"IEEE802istribut         Set_HtMcs_Proc},
	{"HtGi",		          tBwize",				Set_Hcs        Set_PSMode_Proc},
Mcsize",				Set_Gi        Set_PSMode_Proc},
Giize",				Set_Op			Set_KSet_PSMode_Proc},
Proc},ize",				Set_BaWinS
	{"ATEDA",						Set_ABaWinSize",				Set_HtBaWinSize_Proc},
	{"HtRdg",					Set_HtRze",				Set_BaWinSizSet_Key1_PrNNEL_Proc},Set_ATE_CHANNRdgkType_Proc}OWER0Set_ATE_CHANNAmsdukType_Pro			Set_Count          ADAPTER	p// DOT11_N_2UPPOSUPPORT
	{"PktAggregate",				Set_PktAggregate_Proc},
#endif // AGGREGATION_SUPPORT //

#ifdet_Key2_Proc},
	{"Key3",						Set_Key3_Proc},IEEE80211H",					Set_IEEE80211H_Proc},
    {"NetworkType20		Set_ResetStatCounter_Proc},
	{"PSMode",                      Set_Proc},
#ifdef DBG
	{"Debug",						Set_Debug_Proc},
#endif // DBG //

#,								endif // WPA_SUPPLICANT_SUPPORTTETXC,CANT								CHAR {"ATE_RGIet_DefaultKeE_SUTX_et_ATE_TX_P// 23
	30,Rx with_ON,miscuousd su******priv_W;
_Key4_Proc(
    IN  PRTMP_ADAPTER   0x3e82111dClASIC4, IW_Psts sniffer fu
 *
 *TE_ReareplacR	pARSSITE_Reatimestamped su//       arg);

INT Set_WPAPSK_Proc(
    IN  PRTMP_AD//Set_KeyKey2"er,
    le",ATELDENG          arg);


INT Set_PSMode_Proc(F3",		ientTMPWsyncE2P_PWRF2",	   IN  PSTRING   t iwreq	*wT , &csr.IN  Set_KeSet_R			arb_Proc(G Dri     See_Proc},
StTBTT1, 108,_KeyxStopet_DefaulTsfSyncnfo" },roc},
_ATE_Helet_ATE_Read_RWChenRALINK_28xx_QASet_N#endifRTMP_ADAPTER   pAd,
 N  PSTRING     Key3_PSet_FragThreshoc(
    IN  PRTMP_ADAPTER ARPHRD_IEEEWRF3"def SM;IV_S,		           FTMPWt(
       _CarrierDee",	DAPTER	pAd,
	IN	P_SUPPORT
INT Set_TGnWifiTesSet_WpaER	pAd,
Wpa2Tem	PRTnse RRIER* HsinE_RealiclMAC(
	notVOID,atedwill be   DrtoN  Prt wine UI     SMKs de,pteronY;
    UCHAR        arg)= SS_NOTUAI34, 260,
    IN  PSTRING          MASK
INT Set_TGnWifiTesNT Set_Key1_SIZE_MA             _TGnWN  PSTRr FITHtMcef RTMDwreq ---IV_IOCTL_MAC,, IW_PRIV_T,
    IN  PSTRI, IW_V_TYe2_CHARowTKIP_ /*uted */
R | 1024,
  "macne IISTICSnEnt0wnEntry", _CentE_CHle", "sttCHAR | 1024,
  "mac           ,				Set_Long,			Set_DlsTearDownEntrget_site_s     E_CH

TRINORT //
__s32 r HsinrerBu] =TETX,  INT S11,  22#ifdeCCK
	  INc(
 _B, _C, _D, 72, 96, 108, // OFDM
	13, 26,   39,  52,  78, 104, 117, 130, 26,  52,  78, E_CHAter,
    IN  WEPAUTO,   87, 1||apter,
    IN  wepauto,   87, MPIany RF(ifdef CARMASK, "descinfo" },
  _D, _E,rWpa2TemN  P_Proc},
Set_routines
RING			extra);OPEN	IN	struct iwreq	
//2008/0open addf" },
	W_PRIefuse<--
INT Set_F30xx
INT Set_ForcEFUS_SUPPORT
f CARRIER_borkType_"DrieFuseGetFrSHAREDockCount_Proc},
	{"efuseDushared					set_eFusedump_Proc},
	{"efuseLoadFromBin",				set_eFuseLoadSChenBBiNetworkType"mp_PrBuRF3_TMPWWPAPSKockCount_Proc},
	{"efuseDu9papsk					set_eFusedump_Proc},
	{"efuseLoadFromBin",				set_eFuseLoader_ProHtMcsan_Proc},
	{"K",				SSet_ATNONEwTKIP_Proc},tProtect_Proc},
	none/       arg);
LoadwreqSITY_SUPP/11:KH					set_eFusedump_Pr-->ixedTBNoneonLostTime",				Set_BeaconLostTi2c},
#endi	{"AutoRoaming",					Se2N_SUPRoaming_Proc},
	{"SiteSurvey",					Set_SiteSurvey_Proc},
	{"ForceixedCHANTABILWPAdetaiLICANTpter,
	IN	strTime",				Set_BeaconLostTiroc},
	{"AutoRoaming",					SePRTMP_ADAPTER	    pAd,
	IN	PNDIS_802_11_KEY    pKey)
{
	ULONG				Key2_Proc},tAutoBa",				Set_HtAu},
    ,}Mode"
   IN  MPAddKey(
dapter,
	IN	strPORT N  PRTMP_When       , "bPORT Key)
{
	e WEPLINKKey				Set_CounBLE_ENTRY		*pE},
 ;

 c(
	stTiTMP_ADAPTER  TNESS FORry_P 0x800""},
/* --- sub-ioortSecurRIV_TK, 3
	{"HX_ils.nEnt_SECURED"oBa",GFkType_Proc}       unter_ProendISIN	PRTf DBGf

I _C, _Df QOS_DLS   Set_HtMcsDlsAd& "bbpShType_Proc}eof(Cd     Set_ATE_CHADlsTearDown     RALIN       0][0].KeyLen te PTK
		   L,}
};edKey[BSS0][0],wreq	{"LongReen = LENN_TKI.KeyPRTMf

extIN  P{"ShortRetry",				Set_ShortRetryLimit_Proc},
#ifdef EXT_BUILD_CHANNEL_LIST
	{"11dClientMode",				Set_Ieee80211dClientMode_Proc},
#endif // EXT_BUILD_CHANNEL_L, LEN_Key1_Proc 96Set_Wpa/ 20FDM
	1IN  PEL_L);

 5ProcINT 10APTEt_Key;


INKIP_EK, LEN_	{"efuseBufferModeP_ADAPT				Se  90,oamin0_Proc}ATUS 0][0].TxSet_Adefine WEP9, 78,  117, 156, 234rn UCHAR >eSurvey_Proc},
	{"Forcesupport ef  PRT       pAd-RTMP_ADd},
	{hteri
    IN  onX;
    UCHAR              a    RTMP_EFUSEWEPern UCHAR },
	{"ATMimoPndif - sub-ioairCipherLevePHER_KEiteBKey[BSS0][0].TSet_H, _D, _E)		iwe_stGroup_TKIP_EK,LEN_TKIP_TXMICK);
       xMiced subroutines
RING			extra);

#endif // ANT_DIVERSITY_SUPP/1>KR       Dr + h"

>Shared
           TXMICK,          RChipe);
     _B, _C, __TYPE_CHAR |;
#endTKIP_EK +WPASUPPOY		*pE);

INT Set__TYPE_CHAR |{
		NdisEY_LbugLeveLEN_TKIP_TXMICK1, 108y->KeyM,     -          }

            /rAlg
		if ( Chipe IWE_STRE0, IW_PRIV= CIPHER_TKIP;
		else if (pAd);
      RxM IWE_STREPaiClie  }

            // Decidee its ChiperAlg
		if (pAd->Statki_A, _B, _C, _D_TYPE_CHAR |rg);ecide its ChiperAlg
		if <Ad->StaCfg.PairCipher == Ndis802_11Encryption2Enabled)
			pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_TKIP;
		else if (, LEN_TKIP2edKey[BSS0][0_TKIP_ ==Ciphe       _F)	p_D, 3Enam_ad)**** "bbg = CIPHER_TKIP;
EntSS0][0].CipherAlg = CIPHER_AES;
		else
			pAd->Sc, LEN_TKIP_RXMICK);ry->].CipherAlg = CIPHER_NONE;

 AESockCount_Proc},
	{"efuseDuae

#i*********   016 ~TAK, 32);
 
		       = IPHER_MacTab.Co,			t
			ID_WCID]aCfg.PairCiphe>SharedKey[BSS0][0].TxM0][0****Key.Key				S_TKIP_TXMICK);
      					          /*****ipherAlg 3AddSharedKeyEntry(pAd,
		RPairCipR_AES;
		else
			pAd->SharirCi       TKIP_Alg
#if->SharedKey[BSS0][0].TxMntry(pAd,
		.PairCip BSS0,
					[BSS0][0].>KeyO,
	  IW_PRIV_TER  dKey[BSS0  UIpher == Ndis802_1
		RTMOrigdisMoveMem	pAd,
	IN	PSTRING		disMoveMe           }
		    // Update PTK
		   else
			pAd->Sh::(XMICK);
  aredKey[BSS0][0], sizeof(CdisMoveMeY));
            pAd->SharedKey[BSS0][0].KeyLen = LEN_TKIP_EK;
            NdisMoveMemory(pAd->SharedKey[BSS0][0].Key, pKey->KeyMaterialPE_CHAR Ke	iwe_sSTA_PORT_SECURED(pAd);

                //V_TYPE_CHAR | 1024,
  "get_site_survey"},


};

static __s32 ralinkrate[] =
	{2,  4,   11,  22, // CCK
	12, 18,   To end;KeyAIO_OFF 96, 108, // OFDM
	13, 26,   39,  52,  78, 104, 117, 130, 26,  52,  78, 1LO17, 130, 26, 0xFFtaCfg.PairCipheN
#endx		RTMPA  RTDeW_BA_INFO,
	  IW_PRIV81, 108, 61
*/

(IPHER_K>= 1 , _F yId]Coun<= e[];PairCipher == Ndis802_11En802_11Encra2Tet:
 UC)ER, "b));-of(CT
	{"ant",TMP_ and IVEIV tTMP_
	1024,
  "macRFE_CHAR P        }
		    // Update PTK
		   ][pAd->StaC][pAd-::t:
   Key->KearedKey[BSS0][0], sizeof(C][);

		E_STY));
            pAd->SharedKey[BSS0][0].KeyLen = LEN_TKIP_EK;
            NdisMoveMemory(pAd->SharedKey[BSS0][0].Key, pKey->KeyMaterialWEP KEY1{"ShortRetry",				Set_ShortRetryLimit_Proc},
#ifdef EXT_BUILD_CHANNEL_LIST
	{"11dClientMode",				Set_Ieee80211dClientMode_Proc},
#endif // EXT_BUILD_CHANNEL_Lt_TG		  pAd->Shpher == Ndis802_11Encryption3Enabled)
			pAd->ShtaCfg.PairCipheTER	pted
	24, IW_PRIV_Te_Proc(
    IN XMICLCARRIER_pher == Ndis802_11Encryption2Enabledi  IN     UCHP_TXMICK);
   UPPORT
   g.  IN      Alg=CIPHE_MASK6434, 260, //Decide its ChiperAlg
		if (pAd->StaCfg.PairCipher == Ndis802ncryption2Enabled)
			pAd->SharedKe)
			p,

{ RTCODED, _MICK);sProc}eyId]am_a }

           "show5: //wep 40 Ascii tracSUPPORToc},
#endiEnableUPPORTMICK);
      g.Defauul)
			pAd->ShpAd->Mapdd_evne NR_WEP_on to MAC_TABLE_ENUpdat _F)	iwG.CipherSet_FragThresh PRTMPd]. = KeyMaterial + ATE",							Set_ATE_Proc},
	{"ATEDA",		.PairCipher =::	if 1=%s    >Shar    IN  PIN  PSTh"

_ADAPTER	pAd,
	#e"show"      // Decid10 its ChiperHex	if (pAd->StaCfg.Pfor(i=0; i <edED;
	C i++ IN  PSTRING          arg);

INT/

#!isxdigit(*L_KE+i))lg =  PSTRING  K Shared Key Table
		AsicNotoupCiv   4 DOT11_N_SUPPORTAd->StaCfg.PairCipher ][pAd->StaCfg.DefaultK KIP_TXED;
		};

 keyirwiseKey.},

{Ad->SdKey[BSS0][pAd->StaCfg.Defau  PRTM  pAd);
 ,lg = xMic, pme_P********Ad->StaCfg.Dy(pAd					  pAd->RxMic, pAd->Shared2ey[BSS0][0].RxMiTKIP_TXMICK);
   						  pAd->SharedKeyHex[BSS0][0StaCfg.Def>Sha****27)
#d].Ciph3			pAd->TKIP_lg
		if (pAd->StaCfg.PairCipher cAddSharedKeyEntry(pAd,
							  B					 lg
		pAyId].Key,
							  pAd->SharedKey[BSS0][pAif (ribute taStaCfg.DefaultKeyId].CipherAlg,
			128		  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].Key,
							  pAd->SharedKey[BSS0][pAy(pAd,
								  BSS0,
d].TxMic,
							  26Id].Key,
					  pAd->SharedKey[BSS0][pAd->StKey[BSS0][0].RxMig.DefaultKeyId].CipherAlg,
								  NULL);

            // seAES    ddSharedKeyEntry(pAd,g(pAdHAR SharedKey[BSS0][				S
							  BTNdisMo	ProcAddKIP_TXMIC[0].T*****CID a[BSS0][pAdEP from wpipherAlg,
								  NULLpplicant
	{
		UCHDefaultKeyId].CipherAlg,
								  NULL);

       pplicAlg;
	PUCHAR	Key;


		if(pKey->KeyLength == 32)
			goto end;

		KeyIdx = pKey->Keyfrom wpNUherAlg,
								  NULL);.PairCCID aUI
     eMemory(pAd->SharedK  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].Key,
mory(pAd->Shared ( pAd->SharedRF_RW_SUPPORT   IVEIV table
		te PTK
		  edKey[BSS0][pAd->StaCfg.DefautKeyId].Ciphe  NULL);irCiphertf

INkeys (intofrom w     KIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
          ipherAlg =nabl// FOR _Read_ate Ma_SET#de    OldpAd->stuff }

           tracAddGUI
     K);
		PRTMP_ADroc},
	{"ATEBSSID",					Set_ATE_			S	// UpdaredKey[BSS0][0d,
								  BSS0,
Key->*****ic,
		CID a    PHER// seteMedim].Cipher    MedileCIPHER_WEP64;
					else
						pEntry->Pairw128 CIPHER_WEPAdd  PRToc},
	{"ATEBSSID",					Set_ATE_AddKey->Add   py->Pa(   UC)IPHER_WEAid,
DA_Proc},
	#en        pAd->ShTYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
  "e2p"},
#endif  /* DBG */

{ RTPRIV_lg = CIPHER_AES;
		else
			pAdUPPORT
2,
								  NULL);>KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
          ipherAlg = CIPHER_AES;
		else
			pAdipherAlg,
								  NULL);.PairCi2her == Ndis802_11Encryption3En/ Decide its ChiperAlg
		if (pAd->StaCfg.PairCipher == Ndis802_11Encryption2Enabled)
			pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_TKIP;
		else if (pAdhared key)
				pAd->StaCfg.DefaultKeyIdrwiseKey.CipherAlg,
						pEntry);

				}
			}
			else
            {
				// Default key for tx (shared key)
				pAd->StaCfg.Defau>KeyMaterial + LEN_TKIP_EK + LEN_TKIP_/ Decide its ChiperAlg
		if (pAd->StaCfg.PairCipher GUI
            1ate ASIC WCID attribute table and IVEIV table
		RTMPAddWcidAttriory(pAtry(pAd,
								  BSS0,
								  pAd->StaCfg.Def				  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyI = (UCTXMICK2Id].CipherAlg,
								  NULL);

            // set 802.1x port contrherAlg,
							  pAd->SharedKey[BSS0][pAd->St2_1X_PORT_SECURED;
			STA_PORT_SECURED(pAd);

            // Indicate Connected for GUI
            pAd->IndicateMediaState = NdisMediaStateConnected;
        }
	}
	else	// dyory(pA =c);

		// _supplicant
	{
		UCHAR	CipherAlg;
	PUCHAR	Key;

		if(pK			// Ad 4)
		{
			// it is a default shared key, for Pairw
		if(pKey->KeyLength == 32)
			goto end;

		KCID at_Proc(e  Ndis            Ndis11En ey setting
			if (pKey->KeyIndex & ff;

		if (KeyIdx < 4)
		{
			// it is a default shapAd->     _WEPUp}

/*
This is requTXMICK);
    NULLx					 CIPHER//     yIdx,;

					// AddsMedsicry->   }
	}
	else	// dynamic W0][pAdic, Index & 0x0fffffff;

		if (KeyIdx < 4)
		{
			// it *name, char *extra)
{

#ifdef RTMP_MAC_d_valdicateMedi NdisMo			ode == WcidAe, chol IW_PRIVLE_EibuteEntry(pP>KeyLengeWCIDbled     X_ORT
	 end;
 *****STAT(RT_DEBUG_TRribu			SUI
            IifdepAd,C20, 18ed_MAC_GUI!\n"));
     ibutereturn -Mediae te_POIipher-EINVAL;
NETDOWN;
aCfg.PairC}
	}
c,
		requdy     
ut WIis requi
   MAC_LinEX2004/kernel2.6.7uct prov
		pEwlist scann  arroc}_D, 
_DLSint
on) any lgiwonAB(
#ifdefion.   ice);
#endif  =t_ForcOS_NETDEV_GMode_IV(dev****int	chans of1wn!\n")//cING 				1024k Tecfce -is eyP128fdefST_Fd].Ciher == NIndex & 0x80T(RT_D][0].rAlg	// Update se keyleLookup******nel = ion tint rtg.Chann *name, G_ERROR, 	                *
_add_u shode == Ndi:     0][0-*****/*
T(freq->m_con1_DEB][0]anity(pretuPair-wise7, Rry->PaaredKeyEntry(pAd,
					, _DID atseKeyher == NLioctleKey.Cible
		AsicAddSharedKeyEntry(pAd,
							&ct iw_reqcidAttributeEnw_request_ix%x] (Ch_WEP128;					  		  ry->PaChannel = chauest_iseKe5][0].= CIPHER_WEP64;
					else
						pEntry->PairwiseKey.CipherAlg =1 key to Asic
					AsicAddPairwiseKeyEntry(
						pAd,
				}
an);  on) e WCf DOT11__E, ****_Pro| 1024ion.   ->2,
 	return -     in 2);

				pEntry->PairwiseKey);

					// _GET&IPHER_WEP64;
					eL;
	UCHAR chuy(pAd, ASI-1;

    //check if the interface is down
_SUPyry->PaAG(pAdapter, fRTMute dynamry->Paiic WEP fromBSS[BSS0][KeyIdx].Key, &pKey->KeyMateri3ot zero
						pEntry->PairwiseKey.CipherAlg,
						pEntry);

				}
			}
			else
            {
				// Default key for tx (shared key)
				pAd->StaCfg.DefaultKeyId3= (UCHAR) KeyIdx;

				// set key material and key length
				pAd->SharedKey[BSS0][KeyIdx].KeyLen = (UCHAR) pKey->KeyLength;
				NdisMoveMemory(pAd->SharedKey[BSS0][KeyIdx].Key, &pKey->KeyMaterial, pKey->KeyLength);

				// Set Ciper type
				if (pKey->KeyLength == 5)
					pAd->SharedKey[BSS0][KeyIdx].CipherAlg = CIPHER_WEP64;
				else
					pAd->SharedKey[BSS0][KeyIdx].CipherAlg = CIPHER_WEP128;

			CipherAlg = pAd->SharedKey[BSS0][KeyIdx].CipherAlg;
2ate ASIC WCID attribute table and IVEIV table
		RTMPAddWcidAttri      al to Asic
			AsicAddSharedKeyEntry(pAd, BSS0, KeyIdx, CipherAlg, Key, NULL, NULL);

				// Update WtAmsduTXMICK3ared key)
			y(pAdr->CommonCfg.Channel));
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
ter->CommonCfg.Cha_supplicant
	{
		UCHAR	CipherAlg;
	PUCHAR	Key;

		if(pKn", *modion
*/

int
rt_ioctl_giwname(struct net_device *dev,
		   struct iw_request_info *info,
		   charIP128;PMKir-w
_ADAPTER ->SHex.WpNVAL;

	ic},
   SE           0     rd open IFNAMSIZ);
#endif // RTMP_MAC_PCI //
	return 0;
}

int rt_ioctter->CommonCfg.Chaarg)on) any laiw "st::****S54, DE (ve recei%d)U GeNETDOe)******       -EINVAL;00))
requR)
#d * HsincsupIndex & 0x0fffffff;

		if (KeyIdx < 4)
		{
			// ite = IW_MODE_INFRA;
#if (LIE_STREVERSION_CODE > KERNEL_VERSION(2,4,20))
    eP_ADAPTER_INTERRUPT_IN_USE))
    {
        DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
        return -ENETDOWN;
    }


	if (freq->e > 1)
		return -EINVAL;

	if((freq->e == 0) && (freq->m <= 1000))
		chan = furn -ENETDOWN;
	}

		ch = pAdapter->CommonCfg.Channel;re SofnS_DLRACE, ("==>, 2.OWNctl_giw****,				_ONribundif 

	reRT_DE =>StaMT_DEBUG_TRSION(2,4,20)d open *e
      sensquency - search the*    OWN;
	
#ifdE)		iwe_tKeyId].Rx KERNELv,
		   (2,4,2;

	RINT(RT_DEBUG_ERROR, ("==>rt_ioctl_siwfreq::SIOCSIWFREQ[cmd=0x%x] (Channele = IW_M*****SIWFREQic);

#if (L"},
{ fg.D		Set_ABUG_ipher == Ney->KeyMate_B, , ("==>rt_ioct char *extra2 2rd open *tting byretut_ioctl_giwrange(struct net_device HOW, tingest_hare *harel be free;
		   Sretu *retu, char *extraNdisZion",				Set_DriverVer
	ifULoctl   UCHcinfoeroMemm = 2412000 i;
table , like 2.412G, 2.422G,

    if (Chann].CiphT(RT_DEEBUG_T)
ERROR/*				1stens(stfail,2 key to Asic
					AsicAddPairwiseKeyEntry(
						pAd,
				range, 0, singe->txp{"Autcapa   chaTXPsAddBM i;
	memA, _Brequest_info ||A

REQ, pAdapter->CommonCfg.if
   else
   		/* if  %dRINT(chBUG_
	MAP IN NequeID_TO_KHZ(ch, m****return -= m  1020 = IW_POW	   1, char *extra)
{{
	return 0;
}

iT_DE 1st open fail, pAd will be free;
		   So the net_dev->pri4ot zero
						pEntry->PairwiseKey.CipherAlg,
						pEntry);

				}
			}
			else
            {
				// Default key for tx (shared key)
				pAd->StaCfg.DefaultKeyId4= (UCHAR) KeyIdx;

				// set key material and key length
				pAd->SharedKey[BSS0][KeyIdx].KeyLen = (UCHAR) pKey->KeyLength;
				NdisMoveMemory(pAd->SharedKey[BSS0][KeyIdx].Key, &pKey->KeyMaterial, pKey->KeyLength);

				// Set Ciper type
				if (pKey->KeyLength == 5)
					pAd->SharedKey[BSS0][KeyIdx].CipherAlg = CIPHER_WEP64;
				else
					pAd->SharedKey[BSS0][KeyIdx].CipherAlg = CIPHER_WEP128;

			CipherAlg = pAd->SharedKey[BSS0][KeyIdx].CipherAlg;
3ate ASIC WCID attribute table and IVEIV table
		RTMPAddWcidAttrin 2rdual to Asic
			AsicAddSharedKeyEntry(pAd, BSS0, KeyIdx, CipherAlg, Key, NULL, NULL);

				// Update Wms oIWTXMICK4 table and IVEIV table for this group key table
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
t max? nc
Thival;
_supplicant
	{
		UCHAR	CipherAlg;
	PUCHAR	Key;

		if(pKt max? Tion
*/

int
rt_ioctl_giwname(struct net_device *dev,
		   struct iw_request_info *info,
		   charevelof t;f //dBtructraAdaptmax_qual.noisPCI
    strncpy(name, "RT2860 Wireless", IFNAMSIZ);
#endif // RTMP_MAC_PCI //
	return 0;
}

int rt_ioctzes = 2;
	range->eNC_CAPA_* bit fiel fiemt_f017
	/*whatdaptcorrecange->mit WIwasIV_Try->Pa* doc
  IWed exactly. At least
Index & 0x0fffffff;

		if (KeyIdx < 4)
		{
			// it_EXT > 17
	/* IW_ENC_CAPA_* bit field */	   17
	/* IW_EN
trucWtrucwouldT11_sui NdiP_ADAPTER_INTERRUPT_IN_USE))
    {
        DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
        return -ENETDOWN;
    }


	if (freq->e > 1)
		return -EINVAL;

	if((freq->e == 0) && (freq->m <= 1000))
		chan = fzems o2, chCAPA_*encodteriunty[0SION5CNTL_IDLE)
    {
       1SION13

	return 0;in_rtms o
		r_CAPA_* bitGPRINT2347RT_DEBUG_TRin8, 7
		 sockaddr *ap, _Dr*/
		return -ENETDOWN;
	}

	DBGPRINT(RT_DEBe 2.412G, 2.422G,

    ier->CommonCfg.Channel = chan;
	DBGPRINT(RT_DEBUG_ERROR, ("==>rt_ioctl_siwfreq::SIOCSIWFREQ[cmd=0x%x] (Channel_EXT > 1GET_PRIV(dev);
	struct iw_range *range = (struct iw_range *) extra;
	u16 val;
	int i;

	if (pAda3R_TIMEOUT;
		ran		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	DBGPRINT(RT_DEBUG_TRACE ,("===>rt_ioctl_giwrange\n"));
	data->length = sizeof(struct iw_range);
	memset(range, 0, sizeof(struct iw_range));

	r3 key to Asic
					AsicAddPairwiseKeyEntry(
						pAd,
				meAux.  90, tra;
NFO,
	  &BWE_Sown!\n")EQ, pAdapter->CommonCfg.Chablish::

	DBGPRINT(RT_DEBUG_TRACE,("==>rt_ioctl_giwfreq  %d\n", ch));

	MAP_CHANNEL_ID_TO_KHZ(ch, m);
	freq->m 	{"DlsTearDownEntry",			Set_DlsTeaBA_INFO,
	  IW_PRIV_dl    ryhareV_SIZveMemory(pAd->SharedKey[BSS0][0ef RTMCFG_VPA PS"_SUPPORT //
T_SECURED(pAd);

                // Indicate Connected for GUI
                pAd->IndicateMediaState = NdisMediaStateConnected;
            }
		}
   WCID        {
            // Update GTKits ChiperAlg
		if (pAd->StaCfg.PairCipher		iwreless", E_CHAF)	ts ChiperAlg
		if !eSurvey_Proc},
	{"Forceeacmcpy(W_DESC_INy(ne !!!\->sa_* Raxtra	struct iw_range *rx:%0ixedmcpy(
ctl_gef DBG

led)
			pAd->SharedKeter, ch				pMAC_ey3"MASK, edKey[BSS0][KeyIdx].Key, &pKey->KeyMaterialF",					Set_ForceGF_Proc},
#endK		   struc::ifdef Wpter->CommonCfg.
	 //
ETine t_CfgSetne !!!K  // Indd->SharePRIV(dev);

	_SUPPL	pAd->SharedKe, _DS0][el- sub-ioctredKapter,LicensTER	pAd   arg)=PORT //
ETHERaCfg.PairCdd_ev#ifdef WPA_SU():y(pAd,
	);

	ed,
	IN	PSTRIVEIV table
		d suPRTMP_ADAPTER   pAdON_INFO;

struct iw_pri64				  B_SIZE_MASK, V_TY"}ssi values reported in tId].CROUK, "b_N(_A, _B,the| 10driver *
 *1024SNR  IN  ent(,)		pAd->SharedKey61
*/

f you assumeN  PSTRINstory:
    TH_ALEN****#ifdef WPA_SUPPLICANT=VERSupplicantUPwan,dapter->St}

           f you assume thTe neSharedKipher"FHZ(ch, m);
	freq->mdefine WEPairwplicaSTAP("INID RD_SIN	PSTRING			axMic, pKey-ere *
 *some****** sligSTARTpt_ioctl_giwrange(struct net_de free;
		   So the net_dev->priv will  free;
		 tate machine !!!\n"));
    }

    // tell CNTL state machine Pgion SavR	pAyID",		hort, pKey->KeyMateic void seLimiher slighef DBG
EXT_BUILD 1024;
		LISHtMcs	{"ATEHELP",	"WPAPSK",	IeeEWRF3",		EHELP",	  NdisMoveMemory(p                  sPSsZeroMemoextra);

#ifdef R    Set_HtMcsode_Proc},
#iTSThresholdde_Proc},
#ifdef , cha  pAdapter,
    IN  PSTRINapter,
	IN	st}

            // _Antenna_Proc(
ax_PSts ChiperAlg)n"));,
    IN  PS bitpGIWAP(en -
							rssiD, _-90) fig.26)/10****eutoRoSpherdd_event(_B, _d)
			NOR A PARon  IN bit here, wait untilW_PRIHistoPsmo,
		 ( = CIPHER_AES;//c},
exSMALL certain situK, "ra.    IN  PRTMP_ADA*info,
		      strWindowsACCAM#ender sWAP(=EMPTINT Set_PCIePSLevel_Proc(
IN  PRT6] > pApatib#ifdef RTMP_EFUSEbpTueq->.Set_AQu_SUPPORT, char *extraWPAu8)_SUPPORT
 BBatteryThresholdFalseCca     sity_Proc) : ((__u8)DriverVer-MP_ADAPTER   pAdap,
                   RECEIVEIP_TM				  BSS0,
			(pAdapter->StaCfg.WpaSupListen modAPTERr,
    IN  _info *i6,KeyLe)(24 + (dbm
  + 80Fasuct uality = 
						requ,
    IN  PSfretu between -8ADAP				S_DlsAdd);
	}
d op*PLICANchaAST ((_alit
Thi43;
  t *data,   Ch* lQuali
	PRTMP
				Set_AAX_AP];
	sn"))OW_DLq->rt_ioct43;
  t i;

	//checkthe interfal_EXT > truct ibm
 struct erfaRIV(dev)ribut_inhis i
#if (Liw_ORT s rt_i.L_ID_Tdra)
{
	return 0;
}iwapL_ID(st6]);		//bpifdefLatch[6pter->B66]);		//level (dBm)
    iq->noise += 256 ? 143;
         //return -ENETDOWN;
	}

	for (i = 0	return !\n"));
		dataoctl_giwrange(str]);AR chRIV(depter, (dBm)NTERRUPT_IN_USE+= 25b forNr][0].*********!!!\[i].sa_family uct net,
    IN  So t3iw_request_info *istruct iw_point *data, chLegacyurn -ENETDOWN;
	   if (Cha
,
    INtate machaddls[0].T12G, 2.422G,

    if (Chath = i;
	memcpy(extraLEGACY_fig.AP];th = i;
	iwt fieP];
 fie[charlity presd opidatachan) == TRUE)
    {
	pAdaptdowne intef(!MP_PRTEST  130mset(rang, fPSTRING			ar_IN,			UPT_IN_USE)y(addr{5535 * 1024;
		range->min_, BsNFO::V_TYPE_Cal[i]))ALUEBUG_TR* Radapt7, RalT(RT_char *extraAPTER_INTEnet_device *dev,
		   sMAC_(iev);
 i <y(extra +IW_P++zeof(st	mem             //rMASKTas &addr[i]>ScanTab.BssEntry[i].Bssid, MAC));
		return -EN				}

	rettry[i].BssPLICANT_UCCESor Gdr[0]), &qual, i*sizeof(qual[i])6, 4R.h"
*****"Driof) */
 SIOCGIWSCA&memcpi]CHARct iw_quality qual[IW_M_DEBUG_T    iq->noise += 25CAMMP_TEST_FLAG(pAdclealse
AX_AP]imm-EINtePLICANTTMP_E_SUPPORTRIV_PSM_BITs GI, MCS: PWR_ACTIV090 //
#ifdef WPvice *dev,
			struct iw_request_info *info,
			struct iw_point *data, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	ULONG								Now;
	int Status = NDIS_STATUS_SCAM//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER		   struct idapter ONN_F",					Set_ForceGF_Proc},
#endgiwmodN****::(giwmod=%ld_E2P_PRR    }
		MAX_AP ; i++)
	{
		if (fo,
		      struct soc and IVEIV table
		RHZ(ch, m);
	freq->m =/AdORT /suibut2PROMharedKey[Bhich clDDRESdynamed inly e
    /EHELP",	PCIembpTuni */

 400ns GI, , 32);
              _info   pAd->SharedKey[BSS0][pAd->StaCfg.DefauS0][0].CipherAlse
            {
				// Default key for tx (shared kpaStribut flagT_IN_U,   40,									: Driver ignore wpa"Loner the T_IN_USE))1:T_FL(mset(rang- init},
	s( (freR	pA keyAP sele				ST_IN_USE))2: d  // 2takes cneraofroc}) ||
Key[   struct ALIN(p_Fix 802.11 assoc pAdaptt
    -----_IN_Uddr[0]));
	data	memset(range, 0, sizeof(struct iw_range));

	r    	return -ENETDOWN;
	}

		ch = pAdapter->CommonCfg.Channel;wsens(struct net_device *dev,
pa_f SIOCGI96, 108L)
	{ettil _D, s   4S_DLSistributedR | 102ST_Fdd_earedKey[BSS0][pAd->StaCfg.ctlMAhBUG_SK,
_m 11bology11g, opAdapter-UPstruct 2);
       u8)((rctors tretu&&
		E)		iw][0]RROR,MP_PR   *_
#def    q->e > 1)
uest_info ST_FLEQ, pAdapter->CommonCfg.ChaEN    *, _D, _E)
#dnComplORT e mt, wriD_VALUEBUG_TR2t rt_iotell , _Eleting
		// thito     if((frSet***********Com_)
#enWEB_UI pAd->SharedP_ADAPTINENTL state machine to call NdisMSetInformatt(_BCompl&pKey->KeyMaterial, pKey->KeyLengthKTERRUScanTaP_ADmachine to calaredKey[BSS0NTL state machine to calY));
            pAd->ShIVEIV table
	32		else
            {
dle_Proc(e
    IN HAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
  "e2p"},
#endif  /* DBG */

{ RTPRIV_IOCTL_STATISTICS,
  0, IWRead / ifdefer,
t *daA>Sharedbetween -8		SeTMP_Aor tx (shared key)
	Pinchu ndif ur ao *inft *data, wrqor tx (shared key)
				pAockaddr addPROMPRTM_PCI
t net_devicf the td,
	 433,	"Longey3"t *extNotnt_ev = extUsagnt_ev = extCfg.DAut.) iwnolo ra0_str 0buf;ount==> rion.will {"re				r=0x0t *data, char *e2Histocurrent    ;
	=12E
	unsstowe(structSTOM.h"
SION_IN,diaSta=1not zR_TKIP;
FO,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,00ns usefdaptious c */

	if (pAdapter->M39,  52 */
IV_SIZN  PSTRI but-- *tlMachine.	
#ifned  pKe-EAGAI,
		 SharedK	 "aemor(jT(RT, kN	PRTMP_SUPPLICANmsg[, IW]iw_requelute arg[255evelAdaptute si-90dbN	PRTMPchar *00))emp[16evel (i.e. -CAantS2cact iwTKIP_T5&
			(IVATE_S     T
			DBGtry->Pai24,
  "ma	SetIsPrintAPLICAIV(deR {"Fo
= CNset(ms->Stx01, 102e tx, IW_PRIV_T,
 * Hcense,a> 1024,NoDBGPRINT(RT_D
	{"CHAR | 1, ETH softfromrM_SUoAd->Su can re_STATUS_SU itSS_EXTD, _17;

	ret (d255) ?extr :n 2*
 *1024       ,PRIV(spEBUGfu16 va"disMter, chPars>KeyMion.orch the   end
ntlMachin=exp*/
stT Se!*r->ScanTat_Wpago->leve------iw_poShare = rf thMhr(r->ScanTa, '='a or  ANNEL			S*Share++11_N_S,					NoCipher|| !("==>2					 //_PRIdKey[BTSanP];
cistoryT >= tKeyId];
	int cur > 117, Dt_evD, _end_buf&&
		=(iwe));
		iwe.cm				Swhile(j-- >try",	80, 270F.(iwe));
		[jter-'f'yptie is down
	if< '0'
 * (at 000000ING			AC addrMac	stru.u.a
	raty 30iwEBUG_TRiwe.py of *******ATxMicTH_Au.ne !!!\e is down
4-k+j	   s is down
	iLICANT_SUPP      k <== != CNev = IWE_S3-k++]='0'				Sgned ev Now]='\IW_ arg(current_ev == previousr.sas_ev = curStaCfguf = extra#end,buteEP_ADAPTER_BSS_*#end*256S0][pAd[1 IN  Pfreak  struct<0xFF)FF != CN0, 270tGi",		            IN  PRT#if (LI, &->Shared;
	u16 uest, bnet_deviceOTN_PRctl_gMif (LI=%lx,_STA,   4=%NCHAR .Cfg.Ct m of b412000;	Port+o,
	SCAN_M+tKeyId]msg), "[1024GlX]:%08X   NdPortSeg ry->PaPortS
	u16 vOCTLhed burre	{24,
  "macisMoveMem, sot_devicet anntk      aeyMatNge, 0EBUGrAlg *dat  81, 1 0;
		re&iwe, 0PNT(_2);
ated sub pBssEn   st <eset	f thShar&yptiow>Shartessed in dr.sa_
		PBSyption,
  
	reious_	   HOW_DLS (cddress rt_if (current_ev == previous_{
			/*char {
			/*
TKIP{
		8 != CN (pBssEntryCounty 30.Bssid, ETH_ALEN);

        previous_ev = curface is down
	if(!RTMP_TEST_FLLAG(pAdaor (rateCnt=0;ratPLICANT_SUPPted rate . i non    y.
			*/
			for (rateCnt=0;ratTKIP_;
		return -E ,ADAPme++)
			{
				/*
					6Mbps(14te ratenkey BUCCESS;

	 for d = exme, thenf // WPA_ PRTiousset(=unsigned EN);
 Rate[rateCvel =
    IN  PSTR, // 4>privpRate[rateC,e, 0, s, ssEntr   iVG_TRACE, ("I

		// trcpy(teCnt]Share0 || pBssEntry->SupR)d Pai>=12Mbps(152)
 * 11eFuseded 802.1	8Rate[rate
				*/RUE;
			}

			for (ra
		 rat|| pBs7ssEntry->ExtRat	if s_ev)
)
			{
	WIRELEbuf = extra + IW_ extra;
	u16 val;2BIGPHERherAlg nTab.BsyptioHtCapabilitt= RT_B, _  f WIRpAd->SharedKeyE)
					same,"802trcpy(iwe..11b.11b/g/n");
2	0))
trcpy(iw3.u.onAB,SUPPebuS_DLstaft rt_*.namrotoc== (HW
		RTMPSETTING_BASES0][pAdf DOTshow s// 0x2bf4: byte0) an-zero:e machi R17 tuSK)) &0:     signn==EBUG_E	disMoP_ADAPTER   pAd,name,"8irelessT(RT_UE)
ff
    iq->noise += NG          arg);

INTxMic, pKey->KeyMateturtl_gi.br->BbpTucroc(
    IN  PRTMPATE",							Set_ATE_Proc},
	{"ATEDA"("	int i;
u.name,"80T_SUPPORT //

#if#eet_dev->priv wi	sirwiseProc},
	{"HtBaDecline",		Entr0))
0))
_giw	pBssEntry->SupRa   UCHR6         arg);
}
	dy->SupRate[rateCnt]>=152)
					isGonly
	{ SHO  crEN);

      Eon t;
	R66    e2nt SG,

LNA_GAI: 0 ~ 15
	9;
	MAC_TAIWPA_SUATE
#enif			pAdTEUPPORT
INT Set_WpaEntry->Sup		
 * st",					Set_TxBurst_Proc},
	{"TxPreamb66,	Set,a., char *e = NDIS_STATUS_	elseinfohannelY_POWERGnWifi_EV_ADEntry----When  pBssEnt_Key2_Proc},
	{"Key3",						Set_Key3_Proc},N  PSTRING   for (rateCnt=0;rateCnt<pBor tx (shared key)
				p;rateCnt<pBssEntry->ExtRateLen;rff.u.name,"80				st &&
 | 10242
					R66					i= pBssEntry->), _D,W{
				/*
					6MTMP_ADA.11aNAME;


	{
1a/n (pAdapter->gcanTa02d = SIOCGlse
0xanTab.BssEntry (pAdapter->b/_Proc},
	{"EncrypType",					Set)
		{
			iwe.u.moderAlg uct rate is set supported rate . it mean G only.
			_STATUS_S0 || pBated su}_ForceTxB****previ ope== 0nt=0;
iwe,:HtCapaALSE;
		int ra   arg)again.
file		*NT_L_w;
	itlMachine.

  Nam {
#er->Dump.txt";
	imm_segCHAR t	n 2 _f=====E;
			}
 = EL_Lfs(rn - +=0;raiw_requeDSA(iwe.u.E_MAnI

  
				is_w =    p_ateC(>SupRate,N;
	}ONLY|O_CREATfdef CAR	  //S >= BssEn_w[						  /,
    IN  PSTRING          -->2) %s: Error %lZE_MASK))	%sCHAR __FUN (rssi_, -PTE_CHAR 	u16 v,     nsig=TRUE)te machi1024;
P_ADAP=
		me->f_opMic, >=12Mbps(FR->/sGon));
		HtCap >=12Mbps(posignroc},
   struct iwx100E)
			

			fome,"802.11o.ER_P80ous_evtry->Sup(freed AP's ey1",						SteCnt]		*/2 of 802.11b/
		*/
		memset(AX_EBUG	intTH_A1IBSS)
		{
			iwe.u.mod   char.h"
(pAdaV
 * ous_ev)
  i t iw_requestrange->maN);

  ,      set supported&iw_request_inpBssEntry
		BOO("%	for8sxMicgned   struct+= 4name,, (PSTRnel;
		iwe.u.fDAt]>=     			DBGBsss(current_evpBssEntryt_ATE_TX_Pmset(clos		}

			foANNEL_Inty 30<pBssEn;
			}
ata, &= previous_eif SIZE_MASKry[i].BssType == Ndis802_1ho  ndV_SIcomm keyMP_TES!T_UINTER pApy
    iMP_TEST_FLUPPO

    OID RbuRF3_
y 2 of the License, Hsi		{
			/*
fg.Dbuf = urn - +K, ">len    else
        end_     ->KeyMae, 0, sual, pAequest_info *info,
		      st<==till scannin\ndisMove& 0x8000 can be documented.
 *
 * NB: various calculations are based on the orinoco/wavelan
 *     drivers for giwrange(strarrantDR_LEN);
		set_quahe net_dev->priv wisent (sort ink Tckaddr addrreturn -ENET // tell CNTL state machine to call NdisMSetInformaf (Channel ryptountryRegi=152)
					isurn - an
ssEntry->Supurn -b.BssEntry[e, 0, sb.BssEntry[i].Capabve2p 0		if /m[MAXption IE //
	struct i;tlMaD_EV currGENIE
	unsigwe.ucxMic,=1234yption2Enacurr
 * Raler, evel =_ENCn!=0)
		iw3ICAST_rcpyn 0;
}

#ifdef SIOCGIWSCAN
int rt_ioctls_evemsetINde_PGRESSnet_devicename," * StEntrycanption y,
#ifdeing
1024    1a. hDDRESpdatW_EV_UIlagsuct net_devicSUPPLICANr->ScanTabdapter->StaCfg.WpaSuppl			DBGPRINT(Rruct iw_requelute signal =Count = 0;
	}
NT_ENpher)
	SHOR));
	eepeak;
#endif

		//Bit RaMASK modi *extra}ryption2Enable	{
			/*
			,   4 || ntlse
			break;mode = IW_=FALSE;
		intE2    a, char *extra)
{
	PRTMP_XT >ter) || ADbuf = extra + IW_SCAN_*data, char >E;
	dis802_11v = IWE_STREAa + we, 0, sizeoteCnt+ge *) extra;
e.cmd = SIOCGIWRATIRELESS_EXT >=d = IWy->KeyMa IW_EV_LCP_LEN;
            if (tmpRat= NULLe.u._GET_PRIV(dev); cur_TRACE, ("Adapter;
					strw;
	int =152)
					&iwe, 0, se));
			iw{y[i].SupRateLen-1];
			memse	{
				if (isGonly==TRUE)
					strcpy(iwe.u.fq->m <= 100 rt_iMAACxtrae.u.name,"802.11a");
		}
		else
		{
			/*y[i].BssType == Ndis802_11Infrastructure)
		{
			iw*/
			for (rateCnt=0;rateCnt<pBssEntry->SupRateLen;rateCnt++)
			{
				/*
					6Mbps(14char * Ral==140 || pBssEntry->SupRate[rateCnt]==146 || pBssEntry->SupRate[rateCnt]>=152)
					isGonly=TRUE;
			}

			for (rateCnt=0;rateCnt<pBssEntry->ExtRateLen;rateCnt++)
			{
	152)
					i 0, sizeof(iwe));
		HtCapabilityLen!=0)
			{
				if (iseyId].Ciph              iwe.u.bitdata.feyId].CiolhModeitssEntry->SupcaPPLICEEion lBbpT16s GI, MCS: eyId].CiabilpBssEntry		Set_Aprevious set supported rate 4X]:int UCpy ofeyId].CiMCSSetE(pA?  15 :32);
    re[0].T=MP_ADAPTER_Bmode = IW_MOSS;

	 Now------- bbponly=*****		{
			.SupRamode =eyId].CiGI *48) ->		Set_A>14ate machindex > rate_couHt	PRTMRACE,/n")name,"80a/n");
			elTRUE)
	}
			ea
			{
.CipherAlg ==  ralinkrate[rate_index] 500000		br)
					RROR,ame,"ex = one*
 * andB  "stiwe.u			ster xtRate[ratrent_.			*/
an G 		ra802.1us_ev000000we.u.mode we.u.mo< > rate_cou				itC, _(current			No_ERROR, E_STREA	6Mbp(140) 9| pBss46nt]==140 || pBssEntry->ExtRate[ratrent_				*/
 &iwe,
			IW_EEV_PARAx = rate_count;
_ev)>IW[we.u.mo]==140 ||rtGI *48)      }

#ifdef IWEVGEN6E
        //WPA IE
		if (pAdapt>=sEnt12000;
sG		rat R.O0000;ue = AM_LEN);

		if((current_val-currentExt)>IW_EV_LCP_LEN)
		current_endex > rate_cou 0, MAXifdef IWEVGENIE
        //WPAter->ScanTab.BssE>=152)
			aIE.IE[0]),
						   pAda>=1ate[rateTKIP_BssEntry->ExtRat&&
			nt++)
			{
	            else if (tmpRate  cap****.ic voGIfor40 :].WpaIE.nfo, currery[i500000;	pBssEntryW  ralinkrate[rate152)
					isGonlAR)maxMCS);
		: 0 ~ssEntry				iwe.u.bi.dth * 24) + ((UCLME py(c		iwe.u_DD_Ex.Aut2oint 2revio2aIE.I		Set_AWdth * 24) + ((U, _B
			current_val = IW] (Channerat charODE_ETEC
			curriE2Plv,
		->ExtR
{ R.h"
t]==146 || pBssEntry->SupRate[rat==140 t]>=152)
					isGonly=TRUE;
			}

			for (rateCnt=0;ra82)
    BssEntry)
        {
		manTab.BssEntry[i].HtCapability.HtCapInfo;
				int shortGI = capInfo.ChannelWidth ?= 0x8B)
                iwe.u.bit rt_i		Set_Ad PaiFs = 2;
	;
            else if (tmpRate == 0x96)
                iwe.u.bitrate.value =  11 * 10000(devKeyId].C;
		range->max_pmpE
  t iw_request_info *inf	iweyId].Ciphax(UCHAR)maxMCS);
for2xtra) if F*
 * (a, current_ev,end_40 || pBssEntrys_ev)
#if WIRELESS_EXT >= 17
:
           u.retudev);
4
					break;
idthual[4)ointsEntry->SupRate[rateCnt]>=152)
					isGonly=TRUE;
			}

			for (rateCnt=0;rateCnt<pBssEntry->ExtR(dev.RsnIE.IE[0]),
						   peyId].Ci+=eyMapInfo;
				int shortGI = capInfo.Ch    }
d = IWEVGENIE;
			iwe.u.data.length = pAdapAPTER_INTE			pof) */
	ORT /sticFoundatr, ch802.11a");
		}
		else
		{
			/*
	OTCONN;
	}
e is set supported rate . itTH_ALEN);
IcurrQUioctlious_e fielpter, fRssEn          sRIV(dev);

FO::Network is down!\n")          );
	struct ixMCS);
				if (rate.RTER_INustom[0], 0, MAX_CUSTOM_LEN);
			memcpy(custom, &(pAdapter->Scanption >ExtR
   Inc.ssEntrc},
	NTERRUhine !!!\n")nc.,   NTL state mac}OCGIWAity =	u16 vaWhen pAd,
	_S_INTERRU)
{
	return 0;
}giwNCOD		addr[i].sa_family RF reg	addrOCTL_SHOW, s = 2;===============
		memset(&iwe, 0, sizeof(iwe));
		iwe.= SIOCGIWENCODE;
		if (CAP_IS_PRIVACY_ON (pAdapter->ScanTab.BssEntry[i].CapabilityInfo ))
			iwe.u.data.flags =IW_ENCODE_ENABLED | IW_ENCODE_NOKEYrfd = SIOCGIWESSIDe
			iwe.ey[BSS0][KeyIdxsODE_DISABLED;

        previousrf 1E[idx]);
            prRFIE //
	RegID=Cfg.Def // WPA_SUPPLICA   {
				// D=10	Set_Hev;
      RF R1=o, c	memcpy(custom, &(pAdapter->SssEntr(zeof(iX_AP ; i++)Key->KeyLength =t++)
		current_ev & IWE_STODRFX)-1					anTab.BssEntry[i].HtCapability.HtCapInfo;
				i
		//Bit          ; IN  PSTRIN>SharedK

		//BitregRFk;
#endif

		//Bit Ra2048[0])
		//Bit===========G;
#elserfI     ble anarfrssi valtmv)>IWo, current_ev,end_40 || pBssEntrtom);te[pAdapter->ScanTab.BssEntrsign
				iIW_EV-1e_indi].BssType == Ndis802_11InfrastruETH_ALEN);

      RATt(&iwnsigned chaSupRate[rateC
     EV_LCP
		memPRIV(dev);

rcpy     retuf th82reak;
		}
#endcurreous_ebitwe.u.   Ci   *1RIOD;
r->SR ,("===>rt_ioRTMP_ADA_giwscan. 					Now;
	int        iwe.u.bitrate.value =  2 * 00000;
            else if (tmpRate == 0x8B)
                iwe.u.bitrate.value =  5.5C aey->KWPAPf024, 0,
  "T >= 17
   "%d", &(teLe)		{
			/*
ap_addr.s, teLe;
	i3Adapte	TMP_ADAPTanTab.BssE_EV_02_1Tab.BssIncoun-60 I *4},
	, we
			pAd load 8051 firmwaated d = SIOCGIWESSID;
		We mustNthe Fr; iddidevilyIN	str		ForRate 7AN
int rt_io[i].RFuct iw_pri(/SID",)PSTRING			=====reRIV_SIdrequest*_Readevious_ecurrent_ev = dapter, || pBssEalue =lengSID",						Set_SSID_Proc},teLeQA
	if /dif

intdapter-G   iq->upda            previEntry->SaccordR	pAdo Andy, Gary, Davidtting 
	IN	str++)
PROMrate . irf shNN;
	iwe.    BSS0][p
			structom, dubugpherAlg	Nd},
	{"SSID",						Set_SSID_Proc},bbp	u16 valBBPe.cmd = IW_EV_Ai =RFRBSS0][pIZ	if 1XT >= f (isGonly==T, &iwery[i].BssType == Ndis802_1R%02d==TRUExherAlg  RTPR				rroc}Y sonly==TRU
	AR)shortGI *48) + ((UCHAR)maxMCS);
				if (rate_index < 0)
RF		rate_index = 0;
#endf (rate_index > rate_count)
					raY pBssEntI024,
  "macLER  TRACE, ("==>rt_ioct<= 1000RSION(2,4eturn -E2BIG;
#elsee>Sup(sex > rate_coun		rate_index = rate_count;
	iw_poid = SIOCGIWENCODE;
		if (CAP_IS_PRIVACY_ON (pAdMic, emset(range, 0, sname,"8"%lx, 2.422(pAdaptSIZE_M);
	datar[0]), &qual, i*sizeof(qual[i]));

	return 0;
}


#ifdef SSIOCGIWSCAN
int rt_ioctl_siwscan(struct net_device **dev,
			struct iw_request_info *info,
			struct iw_poit *data, ice *dev,
	.value =t(&iwe, 0,nt_ev
		int _ADAPTERp----Str  arEBUG_TRnt rt_ioIIncludes null character.
		if ()
			iwe.u.zeof(i(RT_Don to.u.fIZE_= FALSE)
				return -EIate_i 0, sizeof(i		Set_TxBurst_Proc},
	{"TxPSharedK/ ANYSharedKer->Commoct iw_ran =  5.5DAPbackry(pSy->S.11b iw_range *r-----BGPRINT(RT_DTMP_TELAG(pAda_ADAP);
	struct iw_ran		current_val				802.11a/MEM
			curel)
					CHAR ssid
		sid;
	int 	PRTMP_s%02_Proc},ing = kmalloc(MAX_LEN_OF_SSIta->length = pAMEM_ALLOC  130id, ET].Ci_DEBUG_TRAvalue =  2 *o *info,
RTDebugLevRF_DEBUG_TRA,/IE_EV_ id.h"

#ieyID_+100000;ipherAlg = CIPIN  PV(dev);

  not cowe, 0, sizeo  pAdap*dev,
			 
#endifprn 0;
}

#i;
	struct iw_range *rE ,("Meev)
#TER_INTSharedKe
#endif // AGGRc SIOCGIWSCARTMP_ADAPTER. %d*****      	u16 val;
 extra;
	u16 h theNETDOWN;
    }

	if (,("INFO:)
				{//1024ne IWE_S	    specifi}
  t *data, char  *extra)     0;
}

#ifdef SIOCGIWSCAN
int rt_ioctl_siwscan(struct net_depter, fOP_STATUS_MEDIA_STATE_CONNECT sizeof(i81, 108, 162, 2onnSt("INFO char *extrareturn -E2BIG;
#esiwnick	    break;
#endif
  _ADDR_LE d = SIOCGIWESSIDdapterN_USE))
IOCGIWSCA""   }

	if (datao,
			 struct iw_point *data, chreturn -E2BIG;
#else_ADAPTERPTER pAdap	//==============================priv will bt(&iwe, 0, sizeof(iw:%02xNETDOWN;
	}

	DDE_A, _B
			current_val = IWee;
		   Soab.BssEntry[i].RsnIE.IRmaxMdapt teLen)
     ED;

    y[BSS            if (tmpR *info== TRUo *icy - UE)
  UE)
 pt nef(qual[i]));

	return 0;

#ifdefIOCGIWSCAN
int rt_ioctl_siwscan(struct net_device 
			struct iw_request_info *info,
			struct iw_papter, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		DBGPRINT(RT_DEBUG_TRACE ,("apter-ncludes null character.
	int shoBGPRINT(RT_DEBUG_TRACE ,diaState is not connected, esRF_PNG) pAdapter->Sc    }
#else
      a, char *nickname)
{
	PRTMP_ADAPTER pAdapter =.412G, 2.422G,

    if (Chater, chan) == TRUE)
    {
	pAdapti]));

	reioctl_giwnickn(struct net_device *dev,
			 structd open */
		return -ENETDO%03 *ni*nickname)
{
	PRonly==TRUd ch+)
                sprintf(custom, "%s%02x", ciwe.u.data.length = pAdapters down!\n")=%dCHAR or iIeee-95, whoctlte[ratustom, pAdapter->ScanTab.BssEntry[i-ENETtra +ter->ScanTab.BssEntry[i].Rssi current_ev;
		current_eNIE;
			iwe.u.data.length = pAdapterTab.Threshold)
		try->E(current_ev == prupported ) pAdaptearg);

            for (idx = 0; idx < pAdapter->ScanTab.BssEntry[i].WpaIE.IELen; idWiwe.u.data.length = pAdaptertra knamOID R[msg=%s]y[i].C rtsge *>nfig.RTS_THRESHOLD *inCE, ("==>rt_ioctlata->lengrts->\n",i ,SE;
			BSS0 FALS pAd wil l be freeX_AP];
	in */
		ll be Port Not Secured! ignore this set::OID_802_11_BSSIue	{
			DB */
!, current_evw_range *r.WpaIE.IE[idx]);
            previous_ev = current_ev;
		current_ev TMP_AEN);
			memcpy(custom, &(pAdapter->ScanRF)
     
               * GNU Gets->value == MAX_R

 }
		}
   GnWifiTeetur(current_ev == previous_ev)
#if WIRELESS_EXT > = Rfdef WPA_SUP is down
ickna state machine !!!\n"));
		ine IWX_CUREAMV(dev);

bLESS_l bef.Bssid, ETH_ALE}

	if  {
	mode(modeallowzeof(iwe));
		iYPE_CHAR | ,
    IN  PSTRING          IF free;ar *extra)x{
	Dst_idown
	if(!RER   p		!\n"), _E)(pAdfdef SIOCGIW));
        pAd->ShaO,
	  I              ructstru2, 18,                   	// Normalize Rssi
	if (rssi >= -50)
        ChannelQuality = 100;
	else if (rssi >= ===============================
		memsetaraD, _E)		iwe_st         ter, fOP_STte[rdata.lenDKey3",		"WPAPSK",	Fompleting
		// this request, bue =  5this re_FRAGll be free;
					BSS0 FALS__cputwarle16(fraFlexi0, 30ue & ~0x1)7
	/*Sharenumb    		raous_eRTMP_ecaus;
  Settie == 0)
	    val = MAX_FRAG_THRESHOLD;
	else
		rUG_TctmNdis = FALSE;
		//       {
				i].SupRa,
		   struct iw_rLasBssEnTimquest, d      do = R_Fix_PRIV__TKIP_TXMICK);
     County 30Cal = MAX_FRAG_THRESH(_A, _B,edKey[BSS0] | 1024,
  "ma_request_info *info,
	 17
        xtra);

#ifd{
			/etails. , 18,   100;
	else iRTMP_Tcter.char *extra)
{
	return 0;
}

ifrag		addr[i].sa_famf  re, fO\n",i >= MINe == 0)
	    vaE
  FLAG(pAdapteconfigDAPTER   pAd,uct net_devi.ted rate . it mean GTER pAdapter = RTMv,
			struct iw_request_info *inf (Chanu16ndif

	rchan) == TRUE)
    {
	pAdaptruct net_device *d::(ct iw_request_info *er, fRTMP_ADAPv,
			struct iw_request_info * netruct                      dapter->CommonCfg.ChannelpAd wiT
	ihSTATErs i_se kPRIV_TYPEg, indicate the caller lme.CntlMachine.urn -EVGEN, 18iux.Bta->lSCA fRTM if (tmpRaAdapter,
	IN	struct iwre&iwe,e****zeof(i%sHT Oresholal, HAR :  be fre_OS_NETine to call NdiAddHGNU G;
			     2.

   [BSS, 0,				Set_CountATETXANT",					Set_, like 2.412G, 2.42\n%-19s%-4equest_7nfo *info,
10s%-6uct iw_pointr) == TRUE)
*rts"MAC"f (rIDf ((BSS->le	{"D0_OS_N0) 1&
      ta.l(PhMG, 2"BWE_STMCP_OS_SGILED))TBCd = IW= CNTi=1; i<anneh"ry[i], 26TcanCn[0].Rx {
		PxMic, pW 32);
  puest_2TemtThresdif

 informatPRIVIWEent(_BSMALL */
		>CommoRTMP_SIZEonlySK - 3fRTM_ON,
	  I"));rVermcpyuest_->V, //AsCLIypti		break;
#endiOApClit Secu		break;S-ENEpterT_ASSOCION(2,4,20 like 2.412G, 2.42 cha;
	inUS RTMPW, _ER ,("===	    b*dev,
		 			break;llocGET_e terms ofTED)1E[idx]);
   "));2]ev->priv wie****fla3TRICTED || erq->4TRICTED || erq->5led;
  	PSTRTMPW
	if((f 0;
4G, 2_OS_NET = RCS: 0;
   c},
#        DBGPRINT(RT_DEBUG_TRACE,("INFO::Network apidx state machP_ADAPTER_BSS_f7.PairCipher		break;ev =Sa ~ 15Avg    ATE_Read_Adapter->StaCfg.KeyIdx, CipherAaredKey[BSSWEPey[BSS0stru1   pAdaptStaCfg.WepStatus = Ndis802_11WEPEnabled;
        pAdapt==140 |     DBGPRINT(RT_DEB1_MEDI_OS_NETGetForceTxWepe termHTForceTx						SssEnte[rate     DBGPRINT(RT_DEB6][0].RxM		breaBW= WPA_802_1X_PORT_S_11AutBWeint i=X_AP];
	ing.OrigWepSt Ndis802_11WEPEnabl_11AuthModeOpen;hC    _ioctl_CODE_ sizeof(iwe)ow;
	in8;

	Idrn -NCODE_OPEIE[idx])ELen;
	 erqINDEX) -_flagtrucRING 
    unty
    endMedi
		}
DEX) - 1;
	TBC	if (erq->l || erq->Proc/ 20d, %f the%%) == TRUE)
  	break;D		iwFIFOsort value =  2 *ed)
	e16(fr)
   SS.AuthModeer->CommonCff (tmStatus = er->CommonCf    -#endif // WPA_SUPP)*100/urn 0;
}

i
    e::Wro:def CARWEP_KEY_SIZE 13     *->lengthe net_dev-X_WEP_KEY_SIZE 13_;
			Stat>BbpTuNdisMom Normalize Rssi
	if (rssi >= -50)
 own!\n"));
   	eturn 0;
}

#ifdef the s ltmpfloo/WPAK)redKey[BSS0][pAd->StaCfg.Dequest_iE_STR UCHAMic, E_STRplatWEPDise2007tcureumpRTPRaredDE_INg.DeE_STR* OS_HZairCiphenCfg.FragmentThreshold;
	frag->disg  16);

		if 			ke::(pter====* Checjiffiesiw_sta,
			stru.KeyLen = MAX_ MAX_WEP_KEY_SIZE 13t_ev    stru0][0].Txce *dev,
		   s"INFO::Network is down!\n"));
      }
		else
			pAda SIOCGIWSCAN
int rt_ioctl_siwscan(struct net_WEP_KEY_IN  PRTMP_ADAtaCfg.   // tell CNTL state machine to call NdASK, "ben;
	Capaer->CommonCfg.FragmentThreshold;
	frag->dis				pEntry->Pairst_i						  BSSWSCAN
int rt_ioctl_siw						  BSS0haredKey[BSS0][keyId       if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
                return -E2BIG;
#else
			    breIssu    i);e GIWAP(TDebugLevtoseKey.Resent (sort and IVEIV redK========
		memset(&iwe, 0, sizeof(iwe));
		(RT_D= SIOCGIWENCODE;
		if (CAP_IS_PRIVACY_ON (pAdapter->ScanTab.BssEntry[i].CapabilityInfo ))
			iwe.u.data.flags =IW_ENCODE_ENABLED | IW_ENCODE_NOKEYLengbugL_ is doget_site_survey"},


};

static __s32 ralinkrate[] =
	{2,  4,   11,  22, // CCK
	12, 18,   _CHAR | 102ice *dev,
			  struct iw_request_info *infoCurrStatfine NR_WEP_KEYSa    pAd	/=", 7)		iwom, "% down!\n"));
  HtCapa!_ATE_TESter->iw_st, fctls relatiol_siwscan(struct BSS0PRIN11a
					802.11a/n
					802_info NdisZerota, cha db abov;
    noiice *dev,*dev,
	_ADAPTER   pAdapteCipherAlg = CIPHE<--
#ifdef RT30xx
#ifdef RTMP_B	  // 20ry[i SUPPLIC
	datanow
		NdisMoveMemor_C, WPA_802)rt_iy Chen PRTM {
{ RTPRIV_  DD, _E pAdapPRIV_SIZE_MASK, "bailess"pa2Tem--
#ifdef RT30xiw_poarveMet_Proc},F)
#pAdapterSIZE_cN,
	 er = NdiributMAX_AP];
	structEWRF2   UCHAR  t(_A, _B, _xpressed in db.
 *
 * I			  _C, _D, _E)		iwe_tKeyId].RxMic PRTM rt_ioctl_siwW_PRIV_120,e termV_SIYPE_CHA);

INT ly=TRUE;
			}

			       arg)ACE ,("==>rt_iadd_
#ifdef CARRIER_DETECTION_SUPPORT
 MAX_ode::DefaultKeyId=%x, KeyLen = P_OS_NETDEV_G  pAdapter,
    IN  PSTR	NdisMoveedKey[NdiellE ,("sGonly=TRUE;
		(
	I			 V_SIZSe
#ifdMP_TESTCo    te() af erqc"));
 (custoredKiF1_P=====, bld = vOCGIWSCAN
intg.Auth WPA_80 by fine.priv wilEntry",			Set_DlsTearDown Isabl.DefING			exOalegioProc})pter-ies	return -ENETDpy(aCPA2 I mod,
			  systemUpE_IN(2_11WEonly=TR, chpy(aalloc kt co81, 108, 162,E ,(ssume VerV_SIZE_MASef RTM		equest_infoB RAIOPE_C->Shaen ="DlsTearDownEntrterms WEPEna		   o_on" },
#ifdef QOS_D000000)
	  struct sockescp_addr, cDE_INDde		/* Check!(W_EV_QUAL           pAd->Sha, 18,   oBa",				S!RTMP_TOWN;
	}

	//check if the interface is down
	if(!RTMP_TEt key instead (%d)\n"tom, pAdapter->ScanTab.Bssof the					ev,
		fRTM	 */
		if (e   // tell CNTL state machine to call Nd pAdid     prLerAlg = CIPHER_WEP64;
		}
		else
			/* Disable0][kid-1].Ke=giwest_i0][kid-1].KeWSCAN
int rt_ioctl_siw0][kid-1].Ke/* Check if the key i->CommotaCfg.otect_PCAN\n"));
		SK,
EA
   naf (!(erq->flags & IW_ENCODE_MODE))


		//Bi NULLE))
 Kerssi UsedA0].TxAdapter,
      CONFIG_whether>eam_a 0][0].CODE_ *E;
		int 	memset(r&& (kid <=4, 29,t iw_request_info *info,
E)
 
    iq->nCTED))
SharedKey[BSS0][pAd->StaCfg.DeY net_device *dev,
	bRxAntD // .name=/
		if (eRUPTe taAdapter=======
	
		if (e|=A_802_1WEPEnablmeAux.MAX_AP];
	structter = RTMTED)).ESharateSt_C, (keyIdx IATE",							Set_ATE_Proc},
	{"ATEDA",		<rq->t net if (kid =f XXXr, chif_TRAC), (%d,r, fRTMP_ADAPcanTim[BSS1PrimaryTED))

		if I========pAS    dreak;
#efo,
		      st/*802_FixRatePROMUPPO							N;		1*NdisMoT(info, current_ev,FIX_ANTue =  5eak;
#endi  IN, ("RTtTED))MP_TESd->ShareAdapter->ScanTab.BssStaCfg.WepStatus Cfg.LastScanTim    Adapt If (eTED;//  sof  16);

		ifhMode == NdiStatus = pAdapt WPA_802_1rq->flags & IW_ETMPWint i= if (kid =3 pAdaptf ((keyIdx < 0) || (k2E)
		rq->
		}
)
					s	//erq->flagd = va|| erqeeBlARPHo,
			 void ODE_OPallo, current_ev,;			/* NB: base 1 = FA;t_ev  NB: b****1	POS_COOKIE pev, struc2ENCODE_ENABLED;	/* XXX */
	}

	return 0;

}

static int
rt_ioctl_setparam(struct net_XMASK, "ben;
		13yLen));
	Mc int
rt_ioctl_setpar //