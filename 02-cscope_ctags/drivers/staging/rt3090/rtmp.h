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
    rtmp.h

    Abstract:
    Miniport generic portion header file

    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
    Paul Lin    2002-08-01    created
    James Tan   2002-09-06    modified (Revise NTCRegTable)
    John Chang  2004-09-06    modified for RT2600
*/
#ifndef __RTMP_H__
#define __RTMP_H__

#include "link_list.h"
#include "spectrum_def.h"

#include "rtmp_dot11.h"

#ifdef MLME_EX
#include "mlme_ex_def.h"
#endif // MLME_EX //

#ifdef CONFIG_STA_SUPPORT
#endif // CONFIG_STA_SUPPORT //

#undef AP_WSC_INCLUDED
#undef STA_WSC_INCLUDED
#undef WSC_INCLUDED


#ifdef CONFIG_STA_SUPPORT
#endif // CONFIG_STA_SUPPORT //

#if defined(AP_WSC_INCLUDED) || defined(STA_WSC_INCLUDED)
#define WSC_INCLUDED
#endif





#include "rtmp_chip.h"



typedef struct _RTMP_ADAPTER		RTMP_ADAPTER;
typedef struct _RTMP_ADAPTER		*PRTMP_ADAPTER;

typedef struct _RTMP_CHIP_OP_ RTMP_CHIP_OP;


//#define DBG		1

//#define DBG_DIAGNOSE		1


//+++Add by shiang for merge MiniportMMRequest() and MiniportDataMMRequest() into one function
#define MAX_DATAMM_RETRY	3
#define MGMT_USE_QUEUE_FLAG	0x80
//---Add by shiang for merge MiniportMMRequest() and MiniportDataMMRequest() into one function

#define	MAXSEQ		(0xFFF)


#if defined(CONFIG_AP_SUPPORT) && defined(CONFIG_STA_SUPPORT)
#define IF_DEV_CONFIG_OPMODE_ON_AP(_pAd)	if(_pAd->OpMode == OPMODE_AP)
#define IF_DEV_CONFIG_OPMODE_ON_STA(_pAd)	if(_pAd->OpMode == OPMODE_STA)
#else
#define IF_DEV_CONFIG_OPMODE_ON_AP(_pAd)
#define IF_DEV_CONFIG_OPMODE_ON_STA(_pAd)
#endif

extern  unsigned char   SNAP_AIRONET[];
extern  unsigned char   CISCO_OUI[];
extern  UCHAR	BaSizeArray[4];

extern UCHAR BROADCAST_ADDR[MAC_ADDR_LEN];
extern UCHAR ZERO_MAC_ADDR[MAC_ADDR_LEN];
extern ULONG BIT32[32];
extern UCHAR BIT8[8];
extern char* CipherName[];
extern char* MCSToMbps[];
extern UCHAR	 RxwiMCSToOfdmRate[12];
extern UCHAR SNAP_802_1H[6];
extern UCHAR SNAP_BRIDGE_TUNNEL[6];
extern UCHAR SNAP_AIRONET[8];
extern UCHAR CKIP_LLC_SNAP[8];
extern UCHAR EAPOL_LLC_SNAP[8];
extern UCHAR EAPOL[2];
extern UCHAR IPX[2];
extern UCHAR APPLE_TALK[2];
extern UCHAR RateIdToPlcpSignal[12]; // see IEEE802.11a-1999 p.14
extern UCHAR	 OfdmRateToRxwiMCS[];
extern UCHAR OfdmSignalToRateId[16] ;
extern UCHAR default_cwmin[4];
extern UCHAR default_cwmax[4];
extern UCHAR default_sta_aifsn[4];
extern UCHAR MapUserPriorityToAccessCategory[8];

extern USHORT RateUpPER[];
extern USHORT RateDownPER[];
extern UCHAR  Phy11BNextRateDownward[];
extern UCHAR  Phy11BNextRateUpward[];
extern UCHAR  Phy11BGNextRateDownward[];
extern UCHAR  Phy11BGNextRateUpward[];
extern UCHAR  Phy11ANextRateDownward[];
extern UCHAR  Phy11ANextRateUpward[];
extern CHAR   RssiSafeLevelForTxRate[];
extern UCHAR  RateIdToMbps[];
extern USHORT RateIdTo500Kbps[];

extern UCHAR  CipherSuiteWpaNoneTkip[];
extern UCHAR  CipherSuiteWpaNoneTkipLen;

extern UCHAR  CipherSuiteWpaNoneAes[];
extern UCHAR  CipherSuiteWpaNoneAesLen;

extern UCHAR  SsidIe;
extern UCHAR  SupRateIe;
extern UCHAR  ExtRateIe;

#ifdef DOT11_N_SUPPORT
extern UCHAR  HtCapIe;
extern UCHAR  AddHtInfoIe;
extern UCHAR  NewExtChanIe;
#ifdef DOT11N_DRAFT3
extern UCHAR  ExtHtCapIe;
#endif // DOT11N_DRAFT3 //
#endif // DOT11_N_SUPPORT //

extern UCHAR  ErpIe;
extern UCHAR  DsIe;
extern UCHAR  TimIe;
extern UCHAR  WpaIe;
extern UCHAR  Wpa2Ie;
extern UCHAR  IbssIe;
extern UCHAR  Ccx2Ie;
extern UCHAR  WapiIe;

extern UCHAR  WPA_OUI[];
extern UCHAR  RSN_OUI[];
extern UCHAR  WAPI_OUI[];
extern UCHAR  WME_INFO_ELEM[];
extern UCHAR  WME_PARM_ELEM[];
extern UCHAR  Ccx2QosInfo[];
extern UCHAR  Ccx2IeInfo[];
extern UCHAR  RALINK_OUI[];
extern UCHAR  PowerConstraintIE[];


extern UCHAR  RateSwitchTable[];
extern UCHAR  RateSwitchTable11B[];
extern UCHAR  RateSwitchTable11G[];
extern UCHAR  RateSwitchTable11BG[];

#ifdef DOT11_N_SUPPORT
extern UCHAR  RateSwitchTable11BGN1S[];
extern UCHAR  RateSwitchTable11BGN2S[];
extern UCHAR  RateSwitchTable11BGN2SForABand[];
extern UCHAR  RateSwitchTable11N1S[];
extern UCHAR  RateSwitchTable11N2S[];
extern UCHAR  RateSwitchTable11N2SForABand[];

#ifdef CONFIG_STA_SUPPORT
extern UCHAR  PRE_N_HT_OUI[];
#endif // CONFIG_STA_SUPPORT //
#endif // DOT11_N_SUPPORT //


#ifdef RALINK_ATE
typedef	struct _ATE_INFO {
	UCHAR	Mode;
	CHAR	TxPower0;
	CHAR	TxPower1;
	CHAR    TxAntennaSel;
	CHAR    RxAntennaSel;
	TXWI_STRUC  TxWI;	  // TXWI
	USHORT	QID;
	UCHAR	Addr1[MAC_ADDR_LEN];
	UCHAR	Addr2[MAC_ADDR_LEN];
	UCHAR	Addr3[MAC_ADDR_LEN];
	UCHAR	Channel;
	UINT32	TxLength;
	UINT32	TxCount;
	UINT32	TxDoneCount; // Tx DMA Done
	UINT32	RFFreqOffset;
	BOOLEAN	bRxFER;		// Show Rx Frame Error Rate
	BOOLEAN	bQATxStart; // Have compiled QA in and use it to ATE tx.
	BOOLEAN	bQARxStart;	// Have compiled QA in and use it to ATE rx.
#ifdef RTMP_MAC_PCI
	BOOLEAN	bFWLoading;	// Reload firmware when ATE is done.
#endif // RTMP_MAC_PCI //
	UINT32	RxTotalCnt;
	UINT32	RxCntPerSec;

	CHAR	LastSNR0;             // last received SNR
	CHAR    LastSNR1;             // last received SNR for 2nd  antenna
	CHAR    LastRssi0;            // last received RSSI
	CHAR    LastRssi1;            // last received RSSI for 2nd  antenna
	CHAR    LastRssi2;            // last received RSSI for 3rd  antenna
	CHAR    AvgRssi0;             // last 8 frames' average RSSI
	CHAR    AvgRssi1;             // last 8 frames' average RSSI
	CHAR    AvgRssi2;             // last 8 frames' average RSSI
	SHORT   AvgRssi0X8;           // sum of last 8 frames' RSSI
	SHORT   AvgRssi1X8;           // sum of last 8 frames' RSSI
	SHORT   AvgRssi2X8;           // sum of last 8 frames' RSSI

	UINT32	NumOfAvgRssiSample;


#ifdef RALINK_28xx_QA
	// Tx frame
	USHORT		HLen; // Header Length
	USHORT		PLen; // Pattern Length
	UCHAR		Header[32]; // Header buffer
	UCHAR		Pattern[32]; // Pattern buffer
	USHORT		DLen; // Data Length
	USHORT		seq;
	UINT32		CID;
	RTMP_OS_PID	AtePid;
	// counters
	UINT32		U2M;
	UINT32		OtherData;
	UINT32		Beacon;
	UINT32		OtherCount;
	UINT32		TxAc0;
	UINT32		TxAc1;
	UINT32		TxAc2;
	UINT32		TxAc3;
	/*UINT32		TxHCCA;*/
	UINT32		TxMgmt;
	UINT32		RSSI0;
	UINT32		RSSI1;
	UINT32		RSSI2;
	UINT32		SNR0;
	UINT32		SNR1;
	// TxStatus : 0 --> task is idle, 1 --> task is running
	UCHAR		TxStatus;
#endif // RALINK_28xx_QA //
}	ATE_INFO, *PATE_INFO;

#ifdef RALINK_28xx_QA
struct ate_racfghdr {
	UINT32		magic_no;
	USHORT		command_type;
	USHORT		command_id;
	USHORT		length;
	USHORT		sequence;
	USHORT		status;
	UCHAR		data[2046];
}  __attribute__((packed));
#endif // RALINK_28xx_QA //
#endif // RALINK_ATE //


typedef struct	_RSSI_SAMPLE {
	CHAR			LastRssi0;             // last received RSSI
	CHAR			LastRssi1;             // last received RSSI
	CHAR			LastRssi2;             // last received RSSI
	CHAR			AvgRssi0;
	CHAR			AvgRssi1;
	CHAR			AvgRssi2;
	SHORT			AvgRssi0X8;
	SHORT			AvgRssi1X8;
	SHORT			AvgRssi2X8;
} RSSI_SAMPLE;

//
//  Queue structure and macros
//
typedef struct  _QUEUE_ENTRY    {
	struct _QUEUE_ENTRY     *Next;
}   QUEUE_ENTRY, *PQUEUE_ENTRY;

// Queue structure
typedef struct  _QUEUE_HEADER   {
	PQUEUE_ENTRY    Head;
	PQUEUE_ENTRY    Tail;
	ULONG           Number;
}   QUEUE_HEADER, *PQUEUE_HEADER;

#define InitializeQueueHeader(QueueHeader)              \
{                                                       \
	(QueueHeader)->Head = (QueueHeader)->Tail = NULL;   \
	(QueueHeader)->Number = 0;                          \
}

#define RemoveHeadQueue(QueueHeader)                \
(QueueHeader)->Head;                                \
{                                                   \
	PQUEUE_ENTRY pNext;                             \
	if ((QueueHeader)->Head != NULL)				\
	{												\
		pNext = (QueueHeader)->Head->Next;          \
		(QueueHeader)->Head->Next = NULL;		\
		(QueueHeader)->Head = pNext;                \
		if (pNext == NULL)                          \
			(QueueHeader)->Tail = NULL;             \
		(QueueHeader)->Number--;                    \
	}												\
}

#define InsertHeadQueue(QueueHeader, QueueEntry)            \
{                                                           \
		((PQUEUE_ENTRY)QueueEntry)->Next = (QueueHeader)->Head; \
		(QueueHeader)->Head = (PQUEUE_ENTRY)(QueueEntry);       \
		if ((QueueHeader)->Tail == NULL)                        \
			(QueueHeader)->Tail = (PQUEUE_ENTRY)(QueueEntry);   \
		(QueueHeader)->Number++;                                \
}

#define InsertTailQueue(QueueHeader, QueueEntry)				\
{                                                               \
	((PQUEUE_ENTRY)QueueEntry)->Next = NULL;                    \
	if ((QueueHeader)->Tail)                                    \
		(QueueHeader)->Tail->Next = (PQUEUE_ENTRY)(QueueEntry); \
	else                                                        \
		(QueueHeader)->Head = (PQUEUE_ENTRY)(QueueEntry);       \
	(QueueHeader)->Tail = (PQUEUE_ENTRY)(QueueEntry);           \
	(QueueHeader)->Number++;                                    \
}

#define InsertTailQueueAc(pAd, pEntry, QueueHeader, QueueEntry)			\
{																		\
	((PQUEUE_ENTRY)QueueEntry)->Next = NULL;							\
	if ((QueueHeader)->Tail)											\
		(QueueHeader)->Tail->Next = (PQUEUE_ENTRY)(QueueEntry);			\
	else																\
		(QueueHeader)->Head = (PQUEUE_ENTRY)(QueueEntry);				\
	(QueueHeader)->Tail = (PQUEUE_ENTRY)(QueueEntry);					\
	(QueueHeader)->Number++;											\
}



//
//  Macros for flag and ref count operations
//
#define RTMP_SET_FLAG(_M, _F)       ((_M)->Flags |= (_F))
#define RTMP_CLEAR_FLAG(_M, _F)     ((_M)->Flags &= ~(_F))
#define RTMP_CLEAR_FLAGS(_M)        ((_M)->Flags = 0)
#define RTMP_TEST_FLAG(_M, _F)      (((_M)->Flags & (_F)) != 0)
#define RTMP_TEST_FLAGS(_M, _F)     (((_M)->Flags & (_F)) == (_F))
// Macro for power save flag.
#define RTMP_SET_PSFLAG(_M, _F)       ((_M)->PSFlags |= (_F))
#define RTMP_CLEAR_PSFLAG(_M, _F)     ((_M)->PSFlags &= ~(_F))
#define RTMP_CLEAR_PSFLAGS(_M)        ((_M)->PSFlags = 0)
#define RTMP_TEST_PSFLAG(_M, _F)      (((_M)->PSFlags & (_F)) != 0)
#define RTMP_TEST_PSFLAGS(_M, _F)     (((_M)->PSFlags & (_F)) == (_F))

#define OPSTATUS_SET_FLAG(_pAd, _F)     ((_pAd)->CommonCfg.OpStatusFlags |= (_F))
#define OPSTATUS_CLEAR_FLAG(_pAd, _F)   ((_pAd)->CommonCfg.OpStatusFlags &= ~(_F))
#define OPSTATUS_TEST_FLAG(_pAd, _F)    (((_pAd)->CommonCfg.OpStatusFlags & (_F)) != 0)

#define CLIENT_STATUS_SET_FLAG(_pEntry,_F)      ((_pEntry)->ClientStatusFlags |= (_F))
#define CLIENT_STATUS_CLEAR_FLAG(_pEntry,_F)    ((_pEntry)->ClientStatusFlags &= ~(_F))
#define CLIENT_STATUS_TEST_FLAG(_pEntry,_F)     (((_pEntry)->ClientStatusFlags & (_F)) != 0)

#define RX_FILTER_SET_FLAG(_pAd, _F)    ((_pAd)->CommonCfg.PacketFilter |= (_F))
#define RX_FILTER_CLEAR_FLAG(_pAd, _F)  ((_pAd)->CommonCfg.PacketFilter &= ~(_F))
#define RX_FILTER_TEST_FLAG(_pAd, _F)   (((_pAd)->CommonCfg.PacketFilter & (_F)) != 0)

#ifdef CONFIG_STA_SUPPORT
#define STA_NO_SECURITY_ON(_p)          (_p->StaCfg.WepStatus == Ndis802_11EncryptionDisabled)
#define STA_WEP_ON(_p)                  (_p->StaCfg.WepStatus == Ndis802_11Encryption1Enabled)
#define STA_TKIP_ON(_p)                 (_p->StaCfg.WepStatus == Ndis802_11Encryption2Enabled)
#define STA_AES_ON(_p)                  (_p->StaCfg.WepStatus == Ndis802_11Encryption3Enabled)

#define STA_TGN_WIFI_ON(_p)             (_p->StaCfg.bTGnWifiTest == TRUE)
#endif // CONFIG_STA_SUPPORT //

#define CKIP_KP_ON(_p)				((((_p)->StaCfg.CkipFlag) & 0x10) && ((_p)->StaCfg.bCkipCmicOn == TRUE))
#define CKIP_CMIC_ON(_p)			((((_p)->StaCfg.CkipFlag) & 0x08) && ((_p)->StaCfg.bCkipCmicOn == TRUE))


#define INC_RING_INDEX(_idx, _RingSize)    \
{                                          \
    (_idx) = (_idx+1) % (_RingSize);       \
}


#ifdef DOT11_N_SUPPORT
// StaActive.SupportedHtPhy.MCSSet is copied from AP beacon.  Don't need to update here.
#define COPY_HTSETTINGS_FROM_MLME_AUX_TO_ACTIVE_CFG(_pAd)                                 \
{                                                                                       \
	_pAd->StaActive.SupportedHtPhy.ChannelWidth = _pAd->MlmeAux.HtCapability.HtCapInfo.ChannelWidth;      \
	_pAd->StaActive.SupportedHtPhy.MimoPs = _pAd->MlmeAux.HtCapability.HtCapInfo.MimoPs;      \
	_pAd->StaActive.SupportedHtPhy.GF = _pAd->MlmeAux.HtCapability.HtCapInfo.GF;      \
	_pAd->StaActive.SupportedHtPhy.ShortGIfor20 = _pAd->MlmeAux.HtCapability.HtCapInfo.ShortGIfor20;      \
	_pAd->StaActive.SupportedHtPhy.ShortGIfor40 = _pAd->MlmeAux.HtCapability.HtCapInfo.ShortGIfor40;      \
	_pAd->StaActive.SupportedHtPhy.TxSTBC = _pAd->MlmeAux.HtCapability.HtCapInfo.TxSTBC;      \
	_pAd->StaActive.SupportedHtPhy.RxSTBC = _pAd->MlmeAux.HtCapability.HtCapInfo.RxSTBC;      \
	_pAd->StaActive.SupportedHtPhy.ExtChanOffset = _pAd->MlmeAux.AddHtInfo.AddHtInfo.ExtChanOffset;      \
	_pAd->StaActive.SupportedHtPhy.RecomWidth = _pAd->MlmeAux.AddHtInfo.AddHtInfo.RecomWidth;      \
	_pAd->StaActive.SupportedHtPhy.OperaionMode = _pAd->MlmeAux.AddHtInfo.AddHtInfo2.OperaionMode;      \
	_pAd->StaActive.SupportedHtPhy.NonGfPresent = _pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent;      \
	NdisMoveMemory((_pAd)->MacTab.Content[BSSID_WCID].HTCapability.MCSSet, (_pAd)->StaActive.SupportedPhyInfo.MCSSet, sizeof(UCHAR) * 16);\
}

#define COPY_AP_HTSETTINGS_FROM_BEACON(_pAd, _pHtCapability)                                 \
{                                                                                       \
	_pAd->MacTab.Content[BSSID_WCID].AMsduSize = (UCHAR)(_pHtCapability->HtCapInfo.AMsduSize);	\
	_pAd->MacTab.Content[BSSID_WCID].MmpsMode= (UCHAR)(_pHtCapability->HtCapInfo.MimoPs);	\
	_pAd->MacTab.Content[BSSID_WCID].MaxRAmpduFactor = (UCHAR)(_pHtCapability->HtCapParm.MaxRAmpduFactor);	\
}
#endif // DOT11_N_SUPPORT //

//
// MACRO for 32-bit PCI register read / write
//
// Usage : RTMP_IO_READ32(
//              PRTMP_ADAPTER pAd,
//              ULONG Register_Offset,
//              PULONG  pValue)
//
//         RTMP_IO_WRITE32(
//              PRTMP_ADAPTER pAd,
//              ULONG Register_Offset,
//              ULONG Value)
//


//
// Common fragment list structure -  Identical to the scatter gather frag list structure
//
//#define RTMP_SCATTER_GATHER_ELEMENT         SCATTER_GATHER_ELEMENT
//#define PRTMP_SCATTER_GATHER_ELEMENT        PSCATTER_GATHER_ELEMENT
#define NIC_MAX_PHYS_BUF_COUNT              8

typedef struct _RTMP_SCATTER_GATHER_ELEMENT {
    PVOID		Address;
    ULONG		Length;
    PULONG		Reserved;
} RTMP_SCATTER_GATHER_ELEMENT, *PRTMP_SCATTER_GATHER_ELEMENT;


typedef struct _RTMP_SCATTER_GATHER_LIST {
    ULONG  NumberOfElements;
    PULONG Reserved;
    RTMP_SCATTER_GATHER_ELEMENT Elements[NIC_MAX_PHYS_BUF_COUNT];
} RTMP_SCATTER_GATHER_LIST, *PRTMP_SCATTER_GATHER_LIST;


//
//  Some utility macros
//
#ifndef min
#define min(_a, _b)     (((_a) < (_b)) ? (_a) : (_b))
#endif

#ifndef max
#define max(_a, _b)     (((_a) > (_b)) ? (_a) : (_b))
#endif

#define GET_LNA_GAIN(_pAd)	((_pAd->LatchRfRegs.Channel <= 14) ? (_pAd->BLNAGain) : ((_pAd->LatchRfRegs.Channel <= 64) ? (_pAd->ALNAGain0) : ((_pAd->LatchRfRegs.Channel <= 128) ? (_pAd->ALNAGain1) : (_pAd->ALNAGain2))))

#define INC_COUNTER64(Val)          (Val.QuadPart++)

#define INFRA_ON(_p)                (OPSTATUS_TEST_FLAG(_p, fOP_STATUS_INFRA_ON))
#define ADHOC_ON(_p)                (OPSTATUS_TEST_FLAG(_p, fOP_STATUS_ADHOC_ON))
#define MONITOR_ON(_p)              (((_p)->StaCfg.BssType) == BSS_MONITOR)
#define IDLE_ON(_p)                 (!INFRA_ON(_p) && !ADHOC_ON(_p))

// Check LEAP & CCKM flags
#define LEAP_ON(_p)                 (((_p)->StaCfg.LeapAuthMode) == CISCO_AuthModeLEAP)
#define LEAP_CCKM_ON(_p)            ((((_p)->StaCfg.LeapAuthMode) == CISCO_AuthModeLEAP) && ((_p)->StaCfg.LeapAuthInfo.CCKM == TRUE))

// if orginal Ethernet frame contains no LLC/SNAP, then an extra LLC/SNAP encap is required
#define EXTRA_LLCSNAP_ENCAP_FROM_PKT_START(_pBufVA, _pExtraLlcSnapEncap)		\
{																\
	if (((*(_pBufVA + 12) << 8) + *(_pBufVA + 13)) > 1500)		\
	{															\
		_pExtraLlcSnapEncap = SNAP_802_1H;						\
		if (NdisEqualMemory(IPX, _pBufVA + 12, 2) ||			\
			NdisEqualMemory(APPLE_TALK, _pBufVA + 12, 2))		\
		{														\
			_pExtraLlcSnapEncap = SNAP_BRIDGE_TUNNEL;			\
		}														\
	}															\
	else														\
	{															\
		_pExtraLlcSnapEncap = NULL;								\
	}															\
}

// New Define for new Tx Path.
#define EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(_pBufVA, _pExtraLlcSnapEncap)	\
{																\
	if (((*(_pBufVA) << 8) + *(_pBufVA + 1)) > 1500)			\
	{															\
		_pExtraLlcSnapEncap = SNAP_802_1H;						\
		if (NdisEqualMemory(IPX, _pBufVA, 2) ||					\
			NdisEqualMemory(APPLE_TALK, _pBufVA, 2))			\
		{														\
			_pExtraLlcSnapEncap = SNAP_BRIDGE_TUNNEL;			\
		}														\
	}															\
	else														\
	{															\
		_pExtraLlcSnapEncap = NULL;								\
	}															\
}


#define MAKE_802_3_HEADER(_p, _pMac1, _pMac2, _pType)                   \
{                                                                       \
    NdisMoveMemory(_p, _pMac1, MAC_ADDR_LEN);                           \
    NdisMoveMemory((_p + MAC_ADDR_LEN), _pMac2, MAC_ADDR_LEN);          \
    NdisMoveMemory((_p + MAC_ADDR_LEN * 2), _pType, LENGTH_802_3_TYPE); \
}

// if pData has no LLC/SNAP (neither RFC1042 nor Bridge tunnel), keep it that way.
// else if the received frame is LLC/SNAP-encaped IPX or APPLETALK, preserve the LLC/SNAP field
// else remove the LLC/SNAP field from the result Ethernet frame
// Patch for WHQL only, which did not turn on Netbios but use IPX within its payload
// Note:
//     _pData & _DataSize may be altered (remove 8-byte LLC/SNAP) by this MACRO
//     _pRemovedLLCSNAP: pointer to removed LLC/SNAP; NULL is not removed
#define CONVERT_TO_802_3(_p8023hdr, _pDA, _pSA, _pData, _DataSize, _pRemovedLLCSNAP)      \
{                                                                       \
    char LLC_Len[2];                                                    \
                                                                        \
    _pRemovedLLCSNAP = NULL;                                            \
    if (NdisEqualMemory(SNAP_802_1H, _pData, 6)  ||                     \
        NdisEqualMemory(SNAP_BRIDGE_TUNNEL, _pData, 6))                 \
    {                                                                   \
        PUCHAR pProto = _pData + 6;                                     \
                                                                        \
        if ((NdisEqualMemory(IPX, pProto, 2) || NdisEqualMemory(APPLE_TALK, pProto, 2)) &&  \
            NdisEqualMemory(SNAP_802_1H, _pData, 6))                    \
        {                                                               \
            LLC_Len[0] = (UCHAR)(_DataSize / 256);                      \
            LLC_Len[1] = (UCHAR)(_DataSize % 256);                      \
            MAKE_802_3_HEADER(_p8023hdr, _pDA, _pSA, LLC_Len);          \
        }                                                               \
        else                                                            \
        {                                                               \
            MAKE_802_3_HEADER(_p8023hdr, _pDA, _pSA, pProto);           \
            _pRemovedLLCSNAP = _pData;                                  \
            _DataSize -= LENGTH_802_1_H;                                \
            _pData += LENGTH_802_1_H;                                   \
        }                                                               \
    }                                                                   \
    else                                                                \
    {                                                                   \
        LLC_Len[0] = (UCHAR)(_DataSize / 256);                          \
        LLC_Len[1] = (UCHAR)(_DataSize % 256);                          \
        MAKE_802_3_HEADER(_p8023hdr, _pDA, _pSA, LLC_Len);              \
    }                                                                   \
}


// Enqueue this frame to MLME engine
// We need to enqueue the whole frame because MLME need to pass data type
// information from 802.11 header
#ifdef RTMP_MAC_PCI
#define REPORT_MGMT_FRAME_TO_MLME(_pAd, Wcid, _pFrame, _FrameSize, _Rssi0, _Rssi1, _Rssi2, _PlcpSignal)        \
{                                                                                       \
    UINT32 High32TSF, Low32TSF;                                                          \
    RTMP_IO_READ32(_pAd, TSF_TIMER_DW1, &High32TSF);                                       \
    RTMP_IO_READ32(_pAd, TSF_TIMER_DW0, &Low32TSF);                                        \
    MlmeEnqueueForRecv(_pAd, Wcid, High32TSF, Low32TSF, (UCHAR)_Rssi0, (UCHAR)_Rssi1,(UCHAR)_Rssi2,_FrameSize, _pFrame, (UCHAR)_PlcpSignal);   \
}
#endif // RTMP_MAC_PCI //

#define MAC_ADDR_EQUAL(pAddr1,pAddr2)           RTMPEqualMemory((PVOID)(pAddr1), (PVOID)(pAddr2), MAC_ADDR_LEN)
#define SSID_EQUAL(ssid1, len1, ssid2, len2)    ((len1==len2) && (RTMPEqualMemory(ssid1, ssid2, len1)))

//
// Check if it is Japan W53(ch52,56,60,64) channel.
//
#define JapanChannelCheck(channel)  ((channel == 52) || (channel == 56) || (channel == 60) || (channel == 64))

#ifdef CONFIG_STA_SUPPORT
#define STA_EXTRA_SETTING(_pAd)

#define STA_PORT_SECURED(_pAd) \
{ \
	BOOLEAN	Cancelled; \
	(_pAd)->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED; \
	NdisAcquireSpinLock(&((_pAd)->MacTabLock)); \
	(_pAd)->MacTab.Content[BSSID_WCID].PortSecured = (_pAd)->StaCfg.PortSecured; \
	(_pAd)->MacTab.Content[BSSID_WCID].PrivacyFilter = Ndis802_11PrivFilterAcceptAll;\
	NdisReleaseSpinLock(&(_pAd)->MacTabLock); \
	RTMPCancelTimer(&((_pAd)->Mlme.LinkDownTimer), &Cancelled);\
	STA_EXTRA_SETTING(_pAd); \
}
#endif // CONFIG_STA_SUPPORT //



//
//  Data buffer for DMA operation, the buffer must be contiguous physical memory
//  Both DMA to / from CPU use the same structure.
//
typedef struct  _RTMP_DMABUF
{
	ULONG                   AllocSize;
	PVOID                   AllocVa;            // TxBuf virtual address
	NDIS_PHYSICAL_ADDRESS   AllocPa;            // TxBuf physical address
} RTMP_DMABUF, *PRTMP_DMABUF;


//
// Control block (Descriptor) for all ring descriptor DMA operation, buffer must be
// contiguous physical memory. NDIS_PACKET stored the binding Rx packet descriptor
// which won't be released, driver has to wait until upper layer return the packet
// before giveing up this rx ring descriptor to ASIC. NDIS_BUFFER is assocaited pair
// to describe the packet buffer. For Tx, NDIS_PACKET stored the tx packet descriptor
// which driver should ACK upper layer when the tx is physically done or failed.
//
typedef struct _RTMP_DMACB
{
	ULONG                   AllocSize;          // Control block size
	PVOID                   AllocVa;            // Control block virtual address
	NDIS_PHYSICAL_ADDRESS   AllocPa;            // Control block physical address
	PNDIS_PACKET pNdisPacket;
	PNDIS_PACKET pNextNdisPacket;

	RTMP_DMABUF             DmaBuf;             // Associated DMA buffer structure
} RTMP_DMACB, *PRTMP_DMACB;


typedef struct _RTMP_TX_RING
{
	RTMP_DMACB  Cell[TX_RING_SIZE];
	UINT32		TxCpuIdx;
	UINT32		TxDmaIdx;
	UINT32		TxSwFreeIdx;	// software next free tx index
} RTMP_TX_RING, *PRTMP_TX_RING;

typedef struct _RTMP_RX_RING
{
	RTMP_DMACB  Cell[RX_RING_SIZE];
	UINT32		RxCpuIdx;
	UINT32		RxDmaIdx;
	INT32		RxSwReadIdx;	// software next read index
} RTMP_RX_RING, *PRTMP_RX_RING;

typedef struct _RTMP_MGMT_RING
{
	RTMP_DMACB  Cell[MGMT_RING_SIZE];
	UINT32		TxCpuIdx;
	UINT32		TxDmaIdx;
	UINT32		TxSwFreeIdx; // software next free tx index
} RTMP_MGMT_RING, *PRTMP_MGMT_RING;


//
//  Statistic counter structure
//
typedef struct _COUNTER_802_3
{
	// General Stats
	ULONG       GoodTransmits;
	ULONG       GoodReceives;
	ULONG       TxErrors;
	ULONG       RxErrors;
	ULONG       RxNoBuffer;

	// Ethernet Stats
	ULONG       RcvAlignmentErrors;
	ULONG       OneCollision;
	ULONG       MoreCollisions;

} COUNTER_802_3, *PCOUNTER_802_3;

typedef struct _COUNTER_802_11 {
	ULONG           Length;
	LARGE_INTEGER   LastTransmittedFragmentCount;
	LARGE_INTEGER   TransmittedFragmentCount;
	LARGE_INTEGER   MulticastTransmittedFrameCount;
	LARGE_INTEGER   FailedCount;
	LARGE_INTEGER   RetryCount;
	LARGE_INTEGER   MultipleRetryCount;
	LARGE_INTEGER   RTSSuccessCount;
	LARGE_INTEGER   RTSFailureCount;
	LARGE_INTEGER   ACKFailureCount;
	LARGE_INTEGER   FrameDuplicateCount;
	LARGE_INTEGER   ReceivedFragmentCount;
	LARGE_INTEGER   MulticastReceivedFrameCount;
	LARGE_INTEGER   FCSErrorCount;
} COUNTER_802_11, *PCOUNTER_802_11;

typedef struct _COUNTER_RALINK {
	ULONG           TransmittedByteCount;   // both successful and failure, used to calculate TX throughput
	ULONG           ReceivedByteCount;      // both CRC okay and CRC error, used to calculate RX throughput
	ULONG           BeenDisassociatedCount;
	ULONG           BadCQIAutoRecoveryCount;
	ULONG           PoorCQIRoamingCount;
	ULONG           MgmtRingFullCount;
	ULONG           RxCountSinceLastNULL;
	ULONG           RxCount;
	ULONG           RxRingErrCount;
	ULONG           KickTxCount;
	ULONG           TxRingErrCount;
	LARGE_INTEGER   RealFcsErrCount;
	ULONG           PendingNdisPacketCount;

	ULONG           OneSecOsTxCount[NUM_OF_TX_RING];
	ULONG           OneSecDmaDoneCount[NUM_OF_TX_RING];
	UINT32          OneSecTxDoneCount;
	ULONG           OneSecRxCount;
	UINT32          OneSecTxAggregationCount;
	UINT32          OneSecRxAggregationCount;
	UINT32          OneSecReceivedByteCount;
	UINT32			OneSecFrameDuplicateCount;

	UINT32          OneSecTransmittedByteCount;   // both successful and failure, used to calculate TX throughput
	UINT32          OneSecTxNoRetryOkCount;
	UINT32          OneSecTxRetryOkCount;
	UINT32          OneSecTxFailCount;
	UINT32          OneSecFalseCCACnt;      // CCA error count, for debug purpose, might move to global counter
	UINT32          OneSecRxOkCnt;          // RX without error
	UINT32          OneSecRxOkDataCnt;      // unicast-to-me DATA frame count
	UINT32          OneSecRxFcsErrCnt;      // CRC error
	UINT32          OneSecBeaconSentCnt;
	UINT32          LastOneSecTotalTxCount; // OneSecTxNoRetryOkCount + OneSecTxRetryOkCount + OneSecTxFailCount
	UINT32          LastOneSecRxOkDataCnt;  // OneSecRxOkDataCnt
	ULONG		DuplicateRcv;
	ULONG		TxAggCount;
	ULONG		TxNonAggCount;
	ULONG		TxAgg1MPDUCount;
	ULONG		TxAgg2MPDUCount;
	ULONG		TxAgg3MPDUCount;
	ULONG		TxAgg4MPDUCount;
	ULONG		TxAgg5MPDUCount;
	ULONG		TxAgg6MPDUCount;
	ULONG		TxAgg7MPDUCount;
	ULONG		TxAgg8MPDUCount;
	ULONG		TxAgg9MPDUCount;
	ULONG		TxAgg10MPDUCount;
	ULONG		TxAgg11MPDUCount;
	ULONG		TxAgg12MPDUCount;
	ULONG		TxAgg13MPDUCount;
	ULONG		TxAgg14MPDUCount;
	ULONG		TxAgg15MPDUCount;
	ULONG		TxAgg16MPDUCount;

	LARGE_INTEGER       TransmittedOctetsInAMSDU;
	LARGE_INTEGER       TransmittedAMSDUCount;
	LARGE_INTEGER       ReceivedOctesInAMSDUCount;
	LARGE_INTEGER       ReceivedAMSDUCount;
	LARGE_INTEGER       TransmittedAMPDUCount;
	LARGE_INTEGER       TransmittedMPDUsInAMPDUCount;
	LARGE_INTEGER       TransmittedOctetsInAMPDUCount;
	LARGE_INTEGER       MPDUInReceivedAMPDUCount;
} COUNTER_RALINK, *PCOUNTER_RALINK;


typedef struct _COUNTER_DRS {
	// to record the each TX rate's quality. 0 is best, the bigger the worse.
	USHORT          TxQuality[MAX_STEP_OF_TX_RATE_SWITCH];
	UCHAR           PER[MAX_STEP_OF_TX_RATE_SWITCH];
	UCHAR           TxRateUpPenalty;      // extra # of second penalty due to last unstable condition
	ULONG           CurrTxRateStableTime; // # of second in current TX rate
	BOOLEAN         fNoisyEnvironment;
	BOOLEAN         fLastSecAccordingRSSI;
	UCHAR           LastSecTxRateChangeAction; // 0: no change, 1:rate UP, 2:rate down
	UCHAR			LastTimeTxRateChangeAction; //Keep last time value of LastSecTxRateChangeAction
	ULONG			LastTxOkCount;
} COUNTER_DRS, *PCOUNTER_DRS;




/***************************************************************************
  *	security key related data structure
  **************************************************************************/
typedef struct _CIPHER_KEY {
	UCHAR   Key[16];            // right now we implement 4 keys, 128 bits max
	UCHAR   RxMic[8];			// make alignment
	UCHAR   TxMic[8];
	UCHAR   TxTsc[6];           // 48bit TSC value
	UCHAR   RxTsc[6];           // 48bit TSC value
	UCHAR   CipherAlg;          // 0-none, 1:WEP64, 2:WEP128, 3:TKIP, 4:AES, 5:CKIP64, 6:CKIP128
	UCHAR   KeyLen;
#ifdef CONFIG_STA_SUPPORT
	UCHAR   BssId[6];
#endif // CONFIG_STA_SUPPORT //
            // Key length for each key, 0: entry is invalid
	UCHAR   Type;               // Indicate Pairwise/Group when reporting MIC error
} CIPHER_KEY, *PCIPHER_KEY;


// structure to define WPA Group Key Rekey Interval
typedef struct PACKED _RT_802_11_WPA_REKEY {
	ULONG ReKeyMethod;          // mechanism for rekeying: 0:disable, 1: time-based, 2: packet-based
	ULONG ReKeyInterval;        // time-based: seconds, packet-based: kilo-packets
} RT_WPA_REKEY,*PRT_WPA_REKEY, RT_802_11_WPA_REKEY, *PRT_802_11_WPA_REKEY;



typedef struct {
	UCHAR        Addr[MAC_ADDR_LEN];
	UCHAR        ErrorCode[2];  //00 01-Invalid authentication type
							//00 02-Authentication timeout
							//00 03-Challenge from AP failed
							//00 04-Challenge to AP failed
	BOOLEAN      Reported;
} ROGUEAP_ENTRY, *PROGUEAP_ENTRY;

typedef struct {
	UCHAR               RogueApNr;
	ROGUEAP_ENTRY       RogueApEntry[MAX_LEN_OF_BSS_TABLE];
} ROGUEAP_TABLE, *PROGUEAP_TABLE;

//
// Cisco IAPP format
//
typedef struct  _CISCO_IAPP_CONTENT_
{
	USHORT     Length;        //IAPP Length
	UCHAR      MessageType;      //IAPP type
	UCHAR      FunctionCode;     //IAPP function type
	UCHAR      DestinaionMAC[MAC_ADDR_LEN];
	UCHAR      SourceMAC[MAC_ADDR_LEN];
	USHORT     Tag;           //Tag(element IE) - Adjacent AP report
	USHORT     TagLength;     //Length of element not including 4 byte header
	UCHAR      OUI[4];           //0x00, 0x40, 0x96, 0x00
	UCHAR      PreviousAP[MAC_ADDR_LEN];       //MAC Address of access point
	USHORT     Channel;
	USHORT     SsidLen;
	UCHAR      Ssid[MAX_LEN_OF_SSID];
	USHORT     Seconds;          //Seconds that the client has been disassociated.
} CISCO_IAPP_CONTENT, *PCISCO_IAPP_CONTENT;


/*
  *	Fragment Frame structure
  */
typedef struct  _FRAGMENT_FRAME {
	PNDIS_PACKET    pFragPacket;
	ULONG       RxSize;
	USHORT      Sequence;
	USHORT      LastFrag;
	ULONG       Flags;          // Some extra frame information. bit 0: LLC presented
} FRAGMENT_FRAME, *PFRAGMENT_FRAME;


//
// Packet information for NdisQueryPacket
//
typedef struct  _PACKET_INFO    {
	UINT            PhysicalBufferCount;    // Physical breaks of buffer descripor chained
	UINT            BufferCount ;           // Number of Buffer descriptor chained
	UINT            TotalPacketLength ;     // Self explained
	PNDIS_BUFFER    pFirstBuffer;           // Pointer to first buffer descriptor
} PACKET_INFO, *PPACKET_INFO;


//
//  Arcfour Structure Added by PaulWu
//
typedef struct  _ARCFOUR
{
	UINT            X;
	UINT            Y;
	UCHAR           STATE[256];
} ARCFOURCONTEXT, *PARCFOURCONTEXT;


//
// Tkip Key structure which RC4 key & MIC calculation
//
typedef struct  _TKIP_KEY_INFO  {
	UINT        nBytesInM;  // # bytes in M for MICKEY
	ULONG       IV16;
	ULONG       IV32;
	ULONG       K0;         // for MICKEY Low
	ULONG       K1;         // for MICKEY Hig
	ULONG       L;          // Current state for MICKEY
	ULONG       R;          // Current state for MICKEY
	ULONG       M;          // Message accumulator for MICKEY
	UCHAR       RC4KEY[16];
	UCHAR       MIC[8];
} TKIP_KEY_INFO, *PTKIP_KEY_INFO;


//
// Private / Misc data, counters for driver internal use
//
typedef struct  __PRIVATE_STRUC {
	UINT       SystemResetCnt;         // System reset counter
	UINT       TxRingFullCnt;          // Tx ring full occurrance number
	UINT       PhyRxErrCnt;            // PHY Rx error count, for debug purpose, might move to global counter
	// Variables for WEP encryption / decryption in rtmp_wep.c
	UINT       FCSCRC32;
	ARCFOURCONTEXT  WEPCONTEXT;
	// Tkip stuff
	TKIP_KEY_INFO   Tx;
	TKIP_KEY_INFO   Rx;
} PRIVATE_STRUC, *PPRIVATE_STRUC;


/***************************************************************************
  *	Channel and BBP related data structures
  **************************************************************************/
// structure to tune BBP R66 (BBP TUNING)
typedef struct _BBP_R66_TUNING {
	BOOLEAN     bEnable;
	USHORT      FalseCcaLowerThreshold;  // default 100
	USHORT      FalseCcaUpperThreshold;  // default 512
	UCHAR       R66Delta;
	UCHAR       R66CurrentValue;
	BOOLEAN		R66LowerUpperSelect; //Before LinkUp, Used LowerBound or UpperBound as R66 value.
} BBP_R66_TUNING, *PBBP_R66_TUNING;

// structure to store channel TX power
typedef struct _CHANNEL_TX_POWER {
	USHORT     RemainingTimeForUse;		//unit: sec
	UCHAR      Channel;
#ifdef DOT11N_DRAFT3
	BOOLEAN       bEffectedChannel;	// For BW 40 operating in 2.4GHz , the "effected channel" is the channel that is covered in 40Mhz.
#endif // DOT11N_DRAFT3 //
	CHAR       Power;
	CHAR       Power2;
	UCHAR      MaxTxPwr;
	UCHAR      DfsReq;
} CHANNEL_TX_POWER, *PCHANNEL_TX_POWER;

// structure to store 802.11j channel TX power
typedef struct _CHANNEL_11J_TX_POWER {
	UCHAR      Channel;
	UCHAR      BW;	// BW_10 or BW_20
	CHAR       Power;
	CHAR       Power2;
	USHORT     RemainingTimeForUse;		//unit: sec
} CHANNEL_11J_TX_POWER, *PCHANNEL_11J_TX_POWER;

typedef struct _SOFT_RX_ANT_DIVERSITY_STRUCT {
	UCHAR     EvaluatePeriod;		 // 0:not evalute status, 1: evaluate status, 2: switching status
	UCHAR     EvaluateStableCnt;
	UCHAR     Pair1PrimaryRxAnt;     // 0:Ant-E1, 1:Ant-E2
	UCHAR     Pair1SecondaryRxAnt;   // 0:Ant-E1, 1:Ant-E2
	UCHAR     Pair2PrimaryRxAnt;     // 0:Ant-E3, 1:Ant-E4
	UCHAR     Pair2SecondaryRxAnt;   // 0:Ant-E3, 1:Ant-E4
#ifdef CONFIG_STA_SUPPORT
	SHORT     Pair1AvgRssi[2];       // AvgRssi[0]:E1, AvgRssi[1]:E2
	SHORT     Pair2AvgRssi[2];       // AvgRssi[0]:E3, AvgRssi[1]:E4
#endif // CONFIG_STA_SUPPORT //
	SHORT     Pair1LastAvgRssi;      //
	SHORT     Pair2LastAvgRssi;      //
	ULONG     RcvPktNumWhenEvaluate;
	BOOLEAN   FirstPktArrivedWhenEvaluate;
	RALINK_TIMER_STRUCT    RxAntDiversityTimer;
} SOFT_RX_ANT_DIVERSITY, *PSOFT_RX_ANT_DIVERSITY;


/***************************************************************************
  *	structure for radar detection and channel switch
  **************************************************************************/
typedef struct _RADAR_DETECT_STRUCT {
    //BOOLEAN		IEEE80211H;			// 0: disable, 1: enable IEEE802.11h
	UCHAR		CSCount;			//Channel switch counter
	UCHAR		CSPeriod;			//Channel switch period (beacon count)
	UCHAR		RDCount;			//Radar detection counter
	UCHAR		RDMode;				//Radar Detection mode
	UCHAR		RDDurRegion;		//Radar detection duration region
	UCHAR		BBPR16;
	UCHAR		BBPR17;
	UCHAR		BBPR18;
	UCHAR		BBPR21;
	UCHAR		BBPR22;
	UCHAR		BBPR64;
	ULONG		InServiceMonitorCount; // unit: sec
	UINT8		DfsSessionTime;
#ifdef DFS_FCC_BW40_FIX
	CHAR		DfsSessionFccOff;
#endif
	BOOLEAN		bFastDfs;
	UINT8		ChMovingTime;
	UINT8		LongPulseRadarTh;
#ifdef MERGE_ARCH_TEAM
	CHAR		AvgRssiReq;
	ULONG		DfsLowerLimit;
	ULONG		DfsUpperLimit;
	UINT8		FixDfsLimit;
	ULONG		upperlimit;
	ULONG		lowerlimit;
#endif // MERGE_ARCH_TEAM //
} RADAR_DETECT_STRUCT, *PRADAR_DETECT_STRUCT;

#ifdef CARRIER_DETECTION_SUPPORT
typedef enum CD_STATE_n
{
	CD_NORMAL,
	CD_SILENCE,
	CD_MAX_STATE
} CD_STATE;

#ifdef TONE_RADAR_DETECT_SUPPORT
#define CARRIER_DETECT_RECHECK_TIME			3


#ifdef CARRIER_SENSE_NEW_ALGO
#define CARRIER_DETECT_CRITIRIA				400
#define CARRIER_DETECT_STOP_RATIO				0x11
#define	CARRIER_DETECT_STOP_RATIO_OLD_3090			2
#endif // CARRIER_SENSE_NEW_ALGO //


#define CARRIER_DETECT_STOP_RECHECK_TIME		4
#define CARRIER_DETECT_CRITIRIA_A				230
#define CARRIER_DETECT_DELTA					7
#define CARRIER_DETECT_DIV_FLAG				0
#ifdef RT3090
#define CARRIER_DETECT_THRESHOLD_3090A			0x1fffffff
#endif // RT3090 //
#ifdef RT3390
#define CARRIER_DETECT_THRESHOLD			0x0fffffff
#endif // RT3390 //
#ifndef RT3390
#define CARRIER_DETECT_THRESHOLD			0x0fffffff
#endif // RT3390 //
#endif // TONE_RADAR_DETECT_SUPPORT //

typedef struct CARRIER_DETECTION_s
{
	BOOLEAN					Enable;
	UINT8					CDSessionTime;
	UINT8					CDPeriod;
	CD_STATE				CD_State;
#ifdef TONE_RADAR_DETECT_SUPPORT
	UINT8					delta;
	UINT8					div_flag;
	UINT32					threshold;
	UINT8					recheck;
	UINT8					recheck1;
	UINT8					recheck2;
	UINT32					TimeStamp;
	UINT32					criteria;
	UINT32					CarrierDebug;
	ULONG					idle_time;
	ULONG					busy_time;
	ULONG					Debug;
#endif // TONE_RADAR_DETECT_SUPPORT //
}CARRIER_DETECTION_STRUCT, *PCARRIER_DETECTION_STRUCT;
#endif // CARRIER_DETECTION_SUPPORT //


#ifdef NEW_DFS
typedef struct _NewDFSDebug
{
	UCHAR channel;
	ULONG wait_time;
	UCHAR delta_delay_range;
	UCHAR delta_delay_step;
	UCHAR EL_range;
	UCHAR EL_step;
	UCHAR EH_range;
	UCHAR EH_step;
	UCHAR WL_range;
	UCHAR WL_step;
	UCHAR WH_range;
	UCHAR WH_step;
	ULONG T_expected;
	ULONG T_margin;
	UCHAR start;
	ULONG count;
	ULONG idx;

}NewDFSDebug, *pNewDFSDebug;

#define NEW_DFS_FCC_5_ENT_NUM		5
#define NEW_DFS_DBG_PORT_ENT_NUM_POWER	8
#define NEW_DFS_DBG_PORT_ENT_NUM		(1 << NEW_DFS_DBG_PORT_ENT_NUM_POWER)  // CE Debug Port entry number, 256
#define NEW_DFS_DBG_PORT_MASK	0xff

// Matched Period definition
#define NEW_DFS_MPERIOD_ENT_NUM_POWER		8
#define NEW_DFS_MPERIOD_ENT_NUM		(1 << NEW_DFS_MPERIOD_ENT_NUM_POWER)	 // CE Period Table entry number, 512
#define NEW_DFS_MAX_CHANNEL	4

typedef struct _NewDFSDebugPort{
	ULONG counter;
	ULONG timestamp;
	USHORT width;
	USHORT start_idx; // start index to period table
	USHORT end_idx; // end index to period table
}NewDFSDebugPort, *pNewDFSDebugPort;

// Matched Period Table
typedef struct _NewDFSMPeriod{
	USHORT	idx;
	USHORT width;
	USHORT	idx2;
	USHORT width2;
	ULONG period;
}NewDFSMPeriod, *pNewDFSMPeriod;

#endif // NEW_DFS //


typedef enum _ABGBAND_STATE_ {
	UNKNOWN_BAND,
	BG_BAND,
	A_BAND,
} ABGBAND_STATE;

#ifdef RTMP_MAC_PCI
#ifdef CONFIG_STA_SUPPORT
// Power save method control
typedef	union	_PS_CONTROL	{
	struct	{
		ULONG		EnablePSinIdle:1;			// Enable radio off when not connect to AP. radio on only when sitesurvey,
		ULONG		EnableNewPS:1;		// Enable new  Chip power save fucntion . New method can only be applied in chip version after 2872. and PCIe.
		ULONG		rt30xxPowerMode:2;			// Power Level Mode for rt30xx chip
		ULONG		rt30xxFollowHostASPM:1;			// Card Follows Host's setting for rt30xx chip.
		ULONG		rt30xxForceASPMTest:1;			// Force enable L1 for rt30xx chip. This has higher priority than rt30xxFollowHostASPM Mode.
		ULONG		rsv:26;			// Radio Measurement Enable
	}	field;
	ULONG			word;
}	PS_CONTROL, *PPS_CONTROL;
#endif // CONFIG_STA_SUPPORT //
#endif // RTMP_MAC_PCI //
/***************************************************************************
  *	structure for MLME state machine
  **************************************************************************/
typedef struct _MLME_STRUCT {
#ifdef CONFIG_STA_SUPPORT
	// STA state machines
	STATE_MACHINE           CntlMachine;
	STATE_MACHINE           AssocMachine;
	STATE_MACHINE           AuthMachine;
	STATE_MACHINE           AuthRspMachine;
	STATE_MACHINE           SyncMachine;
	STATE_MACHINE           WpaPskMachine;
	STATE_MACHINE           LeapMachine;
	STATE_MACHINE_FUNC      AssocFunc[ASSOC_FUNC_SIZE];
	STATE_MACHINE_FUNC      AuthFunc[AUTH_FUNC_SIZE];
	STATE_MACHINE_FUNC      AuthRspFunc[AUTH_RSP_FUNC_SIZE];
	STATE_MACHINE_FUNC      SyncFunc[SYNC_FUNC_SIZE];
#endif // CONFIG_STA_SUPPORT //
	STATE_MACHINE_FUNC      ActFunc[ACT_FUNC_SIZE];
	// Action
	STATE_MACHINE           ActMachine;


#ifdef QOS_DLS_SUPPORT
	STATE_MACHINE			DlsMachine;
	STATE_MACHINE_FUNC      DlsFunc[DLS_FUNC_SIZE];
#endif // QOS_DLS_SUPPORT //


	// common WPA state machine
	STATE_MACHINE           WpaMachine;
	STATE_MACHINE_FUNC      WpaFunc[WPA_FUNC_SIZE];



	ULONG                   ChannelQuality;  // 0..100, Channel Quality Indication for Roaming
	ULONG                   Now32;           // latch the value of NdisGetSystemUpTime()
	ULONG                   LastSendNULLpsmTime;

	BOOLEAN                 bRunning;
	NDIS_SPIN_LOCK          TaskLock;
	MLME_QUEUE              Queue;

	UINT                    ShiftReg;

	RALINK_TIMER_STRUCT     PeriodicTimer;
	RALINK_TIMER_STRUCT     APSDPeriodicTimer;
	RALINK_TIMER_STRUCT     LinkDownTimer;
	RALINK_TIMER_STRUCT     LinkUpTimer;
#ifdef RTMP_MAC_PCI
    UCHAR                   bPsPollTimerRunning;
    RALINK_TIMER_STRUCT     PsPollTimer;
	RALINK_TIMER_STRUCT     RadioOnOffTimer;
#endif // RTMP_MAC_PCI //
	ULONG                   PeriodicRound;
	ULONG                   OneSecPeriodicRound;

	UCHAR					RealRxPath;
	BOOLEAN					bLowThroughput;
	BOOLEAN					bEnableAutoAntennaCheck;
	RALINK_TIMER_STRUCT		RxAntEvalTimer;

#ifdef RT30xx
	UCHAR CaliBW40RfR24;
	UCHAR CaliBW20RfR24;
#endif // RT30xx //

} MLME_STRUCT, *PMLME_STRUCT;


#ifdef DOT11_N_SUPPORT
/***************************************************************************
  *	802.11 N related data structures
  **************************************************************************/
struct reordering_mpdu
{
	struct reordering_mpdu	*next;
	PNDIS_PACKET			pPacket;		/* coverted to 802.3 frame */
	int						Sequence;		/* sequence number of MPDU */
	BOOLEAN					bAMSDU;
};

struct reordering_list
{
	struct reordering_mpdu *next;
	int	qlen;
};

struct reordering_mpdu_pool
{
	PVOID					mem;
	NDIS_SPIN_LOCK			lock;
	struct reordering_list	freelist;
};

typedef enum _REC_BLOCKACK_STATUS
{
    Recipient_NONE=0,
	Recipient_USED,
	Recipient_HandleRes,
    Recipient_Accept
} REC_BLOCKACK_STATUS, *PREC_BLOCKACK_STATUS;

typedef enum _ORI_BLOCKACK_STATUS
{
    Originator_NONE=0,
	Originator_USED,
    Originator_WaitRes,
    Originator_Done
} ORI_BLOCKACK_STATUS, *PORI_BLOCKACK_STATUS;

typedef struct _BA_ORI_ENTRY{
	UCHAR   Wcid;
	UCHAR   TID;
	UCHAR   BAWinSize;
	UCHAR   Token;
// Sequence is to fill every outgoing QoS DATA frame's sequence field in 802.11 header.
	USHORT	Sequence;
	USHORT	TimeOutValue;
	ORI_BLOCKACK_STATUS  ORI_BA_Status;
	RALINK_TIMER_STRUCT ORIBATimer;
	PVOID	pAdapter;
} BA_ORI_ENTRY, *PBA_ORI_ENTRY;

typedef struct _BA_REC_ENTRY {
	UCHAR   Wcid;
	UCHAR   TID;
	UCHAR   BAWinSize;	// 7.3.1.14. each buffer is capable of holding a max AMSDU or MSDU.
	//UCHAR	NumOfRxPkt;
	//UCHAR    Curindidx; // the head in the RX reordering buffer
	USHORT		LastIndSeq;
//	USHORT		LastIndSeqAtTimer;
	USHORT		TimeOutValue;
	RALINK_TIMER_STRUCT RECBATimer;
	ULONG		LastIndSeqAtTimer;
	ULONG		nDropPacket;
	ULONG		rcvSeq;
	REC_BLOCKACK_STATUS  REC_BA_Status;
//	UCHAR	RxBufIdxUsed;
	// corresponding virtual address for RX reordering packet storage.
	//RTMP_REORDERDMABUF MAP_RXBuf[MAX_RX_REORDERBUF];
	NDIS_SPIN_LOCK          RxReRingLock;                 // Rx Ring spinlock
//	struct _BA_REC_ENTRY *pNext;
	PVOID	pAdapter;
	struct reordering_list	list;
} BA_REC_ENTRY, *PBA_REC_ENTRY;


typedef struct {
	ULONG		numAsRecipient;		// I am recipient of numAsRecipient clients. These client are in the BARecEntry[]
	ULONG		numAsOriginator;	// I am originator of	numAsOriginator clients. These clients are in the BAOriEntry[]
	ULONG		numDoneOriginator;	// count Done Originator sessions
	BA_ORI_ENTRY       BAOriEntry[MAX_LEN_OF_BA_ORI_TABLE];
	BA_REC_ENTRY       BARecEntry[MAX_LEN_OF_BA_REC_TABLE];
} BA_TABLE, *PBA_TABLE;

//For QureyBATableOID use;
typedef struct  PACKED _OID_BA_REC_ENTRY{
	UCHAR   MACAddr[MAC_ADDR_LEN];
	UCHAR   BaBitmap;   // if (BaBitmap&(1<<TID)), this session with{MACAddr, TID}exists, so read BufSize[TID] for BufferSize
	UCHAR   rsv;
	UCHAR   BufSize[8];
	REC_BLOCKACK_STATUS	REC_BA_Status[8];
} OID_BA_REC_ENTRY, *POID_BA_REC_ENTRY;

//For QureyBATableOID use;
typedef struct  PACKED _OID_BA_ORI_ENTRY{
	UCHAR   MACAddr[MAC_ADDR_LEN];
	UCHAR   BaBitmap;  // if (BaBitmap&(1<<TID)), this session with{MACAddr, TID}exists, so read BufSize[TID] for BufferSize, read ORI_BA_Status[TID] for status
	UCHAR   rsv;
	UCHAR   BufSize[8];
	ORI_BLOCKACK_STATUS  ORI_BA_Status[8];
} OID_BA_ORI_ENTRY, *POID_BA_ORI_ENTRY;

typedef struct _QUERYBA_TABLE{
	OID_BA_ORI_ENTRY       BAOriEntry[32];
	OID_BA_REC_ENTRY       BARecEntry[32];
	UCHAR   OriNum;// Number of below BAOriEntry
	UCHAR   RecNum;// Number of below BARecEntry
} QUERYBA_TABLE, *PQUERYBA_TABLE;

typedef	union	_BACAP_STRUC	{
#ifdef RT_BIG_ENDIAN
	struct	{
		UINT32     :4;
		UINT32     b2040CoexistScanSup:1;		//As Sta, support do 2040 coexistence scan for AP. As Ap, support monitor trigger event to check if can use BW 40MHz.
		UINT32     bHtAdhoc:1;			// adhoc can use ht rate.
		UINT32     MMPSmode:2;	// MIMO power save more, 0:static, 1:dynamic, 2:rsv, 3:mimo enable
		UINT32     AmsduSize:1;	// 0:3839, 1:7935 bytes. UINT  MSDUSizeToBytes[]	= { 3839, 7935};
		UINT32     AmsduEnable:1;	//Enable AMSDU transmisstion
		UINT32		MpduDensity:3;
		UINT32		Policy:2;	// 0: DELAY_BA 1:IMMED_BA  (//BA Policy subfiled value in ADDBA frame)   2:BA-not use
		UINT32		AutoBA:1;	// automatically BA
		UINT32		TxBAWinLimit:8;
		UINT32		RxBAWinLimit:8;
	}	field;
#else
	struct	{
		UINT32		RxBAWinLimit:8;
		UINT32		TxBAWinLimit:8;
		UINT32		AutoBA:1;	// automatically BA
		UINT32		Policy:2;	// 0: DELAY_BA 1:IMMED_BA  (//BA Policy subfiled value in ADDBA frame)   2:BA-not use
		UINT32		MpduDensity:3;
		UINT32		AmsduEnable:1;	//Enable AMSDU transmisstion
		UINT32		AmsduSize:1;	// 0:3839, 1:7935 bytes. UINT  MSDUSizeToBytes[]	= { 3839, 7935};
		UINT32		MMPSmode:2;	// MIMO power save more, 0:static, 1:dynamic, 2:rsv, 3:mimo enable
		UINT32		bHtAdhoc:1;			// adhoc can use ht rate.
		UINT32		b2040CoexistScanSup:1;		//As Sta, support do 2040 coexistence scan for AP. As Ap, support monitor trigger event to check if can use BW 40MHz.
		UINT32		:4;
	}	field;
#endif
	UINT32			word;
} BACAP_STRUC, *PBACAP_STRUC;


typedef struct {
	BOOLEAN		IsRecipient;
	UCHAR   MACAddr[MAC_ADDR_LEN];
	UCHAR   TID;
	UCHAR   nMSDU;
	USHORT   TimeOut;
	BOOLEAN bAllTid;  // If True, delete all TID for BA sessions with this MACaddr.
} OID_ADD_BA_ENTRY, *POID_ADD_BA_ENTRY;


#ifdef DOT11N_DRAFT3
typedef enum _BSS2040COEXIST_FLAG{
	BSS_2040_COEXIST_DISABLE = 0,
	BSS_2040_COEXIST_TIMER_FIRED  = 1,
	BSS_2040_COEXIST_INFO_SYNC = 2,
	BSS_2040_COEXIST_INFO_NOTIFY = 4,
}BSS2040COEXIST_FLAG;
#endif // DOT11N_DRAFT3 //

#define IS_HT_STA(_pMacEntry)	\
	(_pMacEntry->MaxHTPhyMode.field.MODE >= MODE_HTMIX)

#define IS_HT_RATE(_pMacEntry)	\
	(_pMacEntry->HTPhyMode.field.MODE >= MODE_HTMIX)

#define PEER_IS_HT_RATE(_pMacEntry)	\
	(_pMacEntry->HTPhyMode.field.MODE >= MODE_HTMIX)

#endif // DOT11_N_SUPPORT //


//This structure is for all 802.11n card InterOptibilityTest action. Reset all Num every n second.  (Details see MLMEPeriodic)
typedef	struct	_IOT_STRUC	{
	UCHAR			Threshold[2];
	UCHAR			ReorderTimeOutNum[MAX_LEN_OF_BA_REC_TABLE];	// compare with threshold[0]
	UCHAR			RefreshNum[MAX_LEN_OF_BA_REC_TABLE];	// compare with threshold[1]
	ULONG			OneSecInWindowCount;
	ULONG			OneSecFrameDuplicateCount;
	ULONG			OneSecOutWindowCount;
	UCHAR			DelOriAct;
	UCHAR			DelRecAct;
	UCHAR			RTSShortProt;
	UCHAR			RTSLongProt;
	BOOLEAN			bRTSLongProtOn;
#ifdef CONFIG_STA_SUPPORT
	BOOLEAN			bLastAtheros;
    BOOLEAN			bCurrentAtheros;
    BOOLEAN         bNowAtherosBurstOn;
	BOOLEAN			bNextDisableRxBA;
    BOOLEAN			bToggle;
#endif // CONFIG_STA_SUPPORT //
} IOT_STRUC, *PIOT_STRUC;


// This is the registry setting for 802.11n transmit setting.  Used in advanced page.
typedef union _REG_TRANSMIT_SETTING {
#ifdef RT_BIG_ENDIAN
 struct {
         UINT32  rsv:13;
		 UINT32  EXTCHA:2;
		 UINT32  HTMODE:1;
		 UINT32  TRANSNO:2;
		 UINT32  STBC:1; //SPACE
		 UINT32  ShortGI:1;
		 UINT32  BW:1; //channel bandwidth 20MHz or 40 MHz
		 UINT32  TxBF:1; // 3*3
		 UINT32  rsv0:10;
		 //UINT32  MCS:7;                 // MCS
         //UINT32  PhyMode:4;
    } field;
#else
 struct {
         //UINT32  PhyMode:4;
         //UINT32  MCS:7;                 // MCS
		 UINT32  rsv0:10;
		 UINT32  TxBF:1;
         UINT32  BW:1; //channel bandwidth 20MHz or 40 MHz
         UINT32  ShortGI:1;
         UINT32  STBC:1; //SPACE
         UINT32  TRANSNO:2;
         UINT32  HTMODE:1;
         UINT32  EXTCHA:2;
         UINT32  rsv:13;
    } field;
#endif
 UINT32   word;
} REG_TRANSMIT_SETTING, *PREG_TRANSMIT_SETTING;


typedef union  _DESIRED_TRANSMIT_SETTING {
#ifdef RT_BIG_ENDIAN
	struct	{
			USHORT		rsv:3;
			USHORT		FixedTxMode:2;			// If MCS isn't AUTO, fix rate in CCK, OFDM or HT mode.
			USHORT		PhyMode:4;
			USHORT		MCS:7;                 // MCS
	}	field;
#else
	struct	{
			USHORT		MCS:7;			// MCS
			USHORT		PhyMode:4;
			USHORT		FixedTxMode:2;			// If MCS isn't AUTO, fix rate in CCK, OFDM or HT mode.
			USHORT		rsv:3;
	}	field;
#endif
	USHORT		word;
 } DESIRED_TRANSMIT_SETTING, *PDESIRED_TRANSMIT_SETTING;




/***************************************************************************
  *	Multiple SSID related data structures
  **************************************************************************/
#define WLAN_MAX_NUM_OF_TIM			((MAX_LEN_OF_MAC_TABLE >> 3) + 1) /* /8 + 1 */
#define WLAN_CT_TIM_BCMC_OFFSET		0 /* unit: 32B */

/* clear bcmc TIM bit */
#define WLAN_MR_TIM_BCMC_CLEAR(apidx) \
	pAd->ApCfg.MBSSID[apidx].TimBitmaps[WLAN_CT_TIM_BCMC_OFFSET] &= ~BIT8[0];

/* set bcmc TIM bit */
#define WLAN_MR_TIM_BCMC_SET(apidx) \
	pAd->ApCfg.MBSSID[apidx].TimBitmaps[WLAN_CT_TIM_BCMC_OFFSET] |= BIT8[0];

/* clear a station PS TIM bit */
#define WLAN_MR_TIM_BIT_CLEAR(ad_p, apidx, wcid) \
	{	UCHAR tim_offset = wcid >> 3; \
		UCHAR bit_offset = wcid & 0x7; \
		ad_p->ApCfg.MBSSID[apidx].TimBitmaps[tim_offset] &= (~BIT8[bit_offset]); }

/* set a station PS TIM bit */
#define WLAN_MR_TIM_BIT_SET(ad_p, apidx, wcid) \
	{	UCHAR tim_offset = wcid >> 3; \
		UCHAR bit_offset = wcid & 0x7; \
		ad_p->ApCfg.MBSSID[apidx].TimBitmaps[tim_offset] |= BIT8[bit_offset]; }


// configuration common to OPMODE_AP as well as OPMODE_STA
typedef struct _COMMON_CONFIG {

	BOOLEAN		bCountryFlag;
	UCHAR		CountryCode[3];
	UCHAR		Geography;
	UCHAR       CountryRegion;      // Enum of country region, 0:FCC, 1:IC, 2:ETSI, 3:SPAIN, 4:France, 5:MKK, 6:MKK1, 7:Israel
	UCHAR       CountryRegionForABand;	// Enum of country region for A band
	UCHAR       PhyMode;            // PHY_11A, PHY_11B, PHY_11BG_MIXED, PHY_ABG_MIXED
	UCHAR       DesiredPhyMode;            // PHY_11A, PHY_11B, PHY_11BG_MIXED, PHY_ABG_MIXED
	USHORT      Dsifs;              // in units of usec
	ULONG       PacketFilter;       // Packet filter for receiving
	UINT8		RegulatoryClass[MAX_NUM_OF_REGULATORY_CLASS];

	CHAR        Ssid[MAX_LEN_OF_SSID]; // NOT NULL-terminated
	UCHAR       SsidLen;               // the actual ssid length in used
	UCHAR       LastSsidLen;               // the actual ssid length in used
	CHAR        LastSsid[MAX_LEN_OF_SSID]; // NOT NULL-terminated
	UCHAR		LastBssid[MAC_ADDR_LEN];

	UCHAR       Bssid[MAC_ADDR_LEN];
	USHORT      BeaconPeriod;
	UCHAR       Channel;
	UCHAR       CentralChannel;	// Central Channel when using 40MHz is indicating. not real channel.

	UCHAR       SupRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR       SupRateLen;
	UCHAR       ExtRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR       ExtRateLen;
	UCHAR       DesireRate[MAX_LEN_OF_SUPPORTED_RATES];      // OID_802_11_DESIRED_RATES
	UCHAR       MaxDesiredRate;
	UCHAR       ExpectedACKRate[MAX_LEN_OF_SUPPORTED_RATES];

	ULONG       BasicRateBitmap;        // backup basic ratebitmap

	BOOLEAN		bAPSDCapable;
	BOOLEAN		bInServicePeriod;
	BOOLEAN		bAPSDAC_BE;
	BOOLEAN		bAPSDAC_BK;
	BOOLEAN		bAPSDAC_VI;
	BOOLEAN		bAPSDAC_VO;

	/* because TSPEC can modify the APSD flag, we need to keep the APSD flag
		requested in association stage from the station;
		we need to recover the APSD flag after the TSPEC is deleted. */
	BOOLEAN		bACMAPSDBackup[4]; /* for delivery-enabled & trigger-enabled both */
	BOOLEAN		bACMAPSDTr[4]; /* no use */

	BOOLEAN		bNeedSendTriggerFrame;
	BOOLEAN		bAPSDForcePowerSave;	// Force power save mode, should only use in APSD-STAUT
	ULONG		TriggerTimerCount;
	UCHAR		MaxSPLength;
	UCHAR		BBPCurrentBW;	// BW_10,	BW_20, BW_40
	// move to MULTISSID_STRUCT for MBSS
	//HTTRANSMIT_SETTING	HTPhyMode, MaxHTPhyMode, MinHTPhyMode;// For transmit phy setting in TXWI.
	REG_TRANSMIT_SETTING        RegTransmitSetting; //registry transmit setting. this is for reading registry setting only. not useful.
	//UCHAR       FixedTxMode;              // Fixed Tx Mode (CCK, OFDM), for HT fixed tx mode (GF, MIX) , refer to RegTransmitSetting.field.HTMode
	UCHAR       TxRate;                 // Same value to fill in TXD. TxRate is 6-bit
	UCHAR       MaxTxRate;              // RATE_1, RATE_2, RATE_5_5, RATE_11
	UCHAR       TxRateIndex;            // Tx rate index in RateSwitchTable
	UCHAR       TxRateTableSize;        // Valid Tx rate table size in RateSwitchTable
	//BOOLEAN		bAutoTxRateSwitch;
	UCHAR       MinTxRate;              // RATE_1, RATE_2, RATE_5_5, RATE_11
	UCHAR       RtsRate;                // RATE_xxx
	HTTRANSMIT_SETTING	MlmeTransmit;   // MGMT frame PHY rate setting when operatin at Ht rate.
	UCHAR       MlmeRate;               // RATE_xxx, used to send MLME frames
	UCHAR       BasicMlmeRate;          // Default Rate for sending MLME frames

	USHORT      RtsThreshold;           // in unit of BYTE
	USHORT      FragmentThreshold;      // in unit of BYTE

	UCHAR       TxPower;                // in unit of mW
	ULONG       TxPowerPercentage;      // 0~100 %
	ULONG       TxPowerDefault;         // keep for TxPowerPercentage
	UINT8		PwrConstraint;

#ifdef DOT11_N_SUPPORT
	BACAP_STRUC        BACapability; //   NO USE = 0XFF  ;  IMMED_BA =1  ;  DELAY_BA=0
	BACAP_STRUC        REGBACapability; //   NO USE = 0XFF  ;  IMMED_BA =1  ;  DELAY_BA=0
#endif // DOT11_N_SUPPORT //
	IOT_STRUC		IOTestParm;	// 802.11n InterOpbility Test Parameter;
	ULONG       TxPreamble;             // Rt802_11PreambleLong, Rt802_11PreambleShort, Rt802_11PreambleAuto
	BOOLEAN     bUseZeroToDisableFragment;     // Microsoft use 0 as disable
	ULONG       UseBGProtection;        // 0: auto, 1: always use, 2: always not use
	BOOLEAN     bUseShortSlotTime;      // 0: disable, 1 - use short slot (9us)
	BOOLEAN     bEnableTxBurst;         // 1: enble TX PACKET BURST (when BA is established or AP is not a legacy WMM AP), 0: disable TX PACKET BURST
	BOOLEAN     bAggregationCapable;      // 1: enable TX aggregation when the peer supports it
	BOOLEAN     bPiggyBackCapable;		// 1: enable TX piggy-back according MAC's version
	BOOLEAN     bIEEE80211H;			// 1: enable IEEE802.11h spec.
	ULONG		DisableOLBCDetect;		// 0: enable OLBC detect; 1 disable OLBC detect

#ifdef DOT11_N_SUPPORT
	BOOLEAN				bRdg;
#endif // DOT11_N_SUPPORT //
	BOOLEAN             bWmmCapable;        // 0:disable WMM, 1:enable WMM
	QOS_CAPABILITY_PARM APQosCapability;    // QOS capability of the current associated AP
	EDCA_PARM           APEdcaParm;         // EDCA parameters of the current associated AP
	QBSS_LOAD_PARM      APQbssLoad;         // QBSS load of the current associated AP
	UCHAR               AckPolicy[4];       // ACK policy of the specified AC. see ACK_xxx
#ifdef CONFIG_STA_SUPPORT
	BOOLEAN				bDLSCapable;		// 0:disable DLS, 1:enable DLS
#endif // CONFIG_STA_SUPPORT //
	// a bitmap of BOOLEAN flags. each bit represent an operation status of a particular
	// BOOLEAN control, either ON or OFF. These flags should always be accessed via
	// OPSTATUS_TEST_FLAG(), OPSTATUS_SET_FLAG(), OP_STATUS_CLEAR_FLAG() macros.
	// see fOP_STATUS_xxx in RTMP_DEF.C for detail bit definition
	ULONG               OpStatusFlags;

	BOOLEAN				NdisRadioStateOff; //For HCT 12.0, set this flag to TRUE instead of called MlmeRadioOff.
	ABGBAND_STATE		BandState;		// For setting BBP used on B/G or A mode.
#ifdef ANT_DIVERSITY_SUPPORT
	UCHAR				bRxAntDiversity; // 0:disable, 1:enable Software Rx Antenna Diversity.
#endif // ANT_DIVERSITY_SUPPORT //

	// IEEE802.11H--DFS.
	RADAR_DETECT_STRUCT	RadarDetect;

#ifdef CARRIER_DETECTION_SUPPORT
	CARRIER_DETECTION_STRUCT		CarrierDetect;
#endif // CARRIER_DETECTION_SUPPORT //

#ifdef DOT11_N_SUPPORT
	// HT
	UCHAR			BASize;		// USer desired BAWindowSize. Should not exceed our max capability
	//RT_HT_CAPABILITY	SupportedHtPhy;
	RT_HT_CAPABILITY	DesiredHtPhy;
	HT_CAPABILITY_IE		HtCapability;
	ADD_HT_INFO_IE		AddHTInfo;	// Useful as AP.
	//This IE is used with channel switch announcement element when changing to a new 40MHz.
	//This IE is included in channel switch ammouncement frames 7.4.1.5, beacons, probe Rsp.
	NEW_EXT_CHAN_IE	NewExtChanOffset;	//7.3.2.20A, 1 if extension channel is above the control channel, 3 if below, 0 if not present

#ifdef DOT11N_DRAFT3
	UCHAR					Bss2040CoexistFlag;		// bit 0: bBssCoexistTimerRunning, bit 1: NeedSyncAddHtInfo.
	RALINK_TIMER_STRUCT	Bss2040CoexistTimer;

	//This IE is used for 20/40 BSS Coexistence.
	BSS_2040_COEXIST_IE		BSS2040CoexistInfo;
	// ====== 11n D3.0 =======================>
	USHORT					Dot11OBssScanPassiveDwell;				// Unit : TU. 5~1000
	USHORT					Dot11OBssScanActiveDwell;				// Unit : TU. 10~1000
	USHORT					Dot11BssWidthTriggerScanInt;			// Unit : Second
	USHORT					Dot11OBssScanPassiveTotalPerChannel;	// Unit : TU. 200~10000
	USHORT					Dot11OBssScanActiveTotalPerChannel;	// Unit : TU. 20~10000
	USHORT					Dot11BssWidthChanTranDelayFactor;
	USHORT					Dot11OBssScanActivityThre;				// Unit : percentage

	ULONG					Dot11BssWidthChanTranDelay;			// multiple of (Dot11BssWidthTriggerScanInt * Dot11BssWidthChanTranDelayFactor)
	ULONG					CountDownCtr;	// CountDown Counter from (Dot11BssWidthTriggerScanInt * Dot11BssWidthChanTranDelayFactor)

	NDIS_SPIN_LOCK          TriggerEventTabLock;
	BSS_2040_COEXIST_IE		LastBSSCoexist2040;
	BSS_2040_COEXIST_IE		BSSCoexist2040;
	TRIGGER_EVENT_TAB		TriggerEventTab;
	UCHAR					ChannelListIdx;
	// <====== 11n D3.0 =======================
	BOOLEAN					bOverlapScanning;
#endif // DOT11N_DRAFT3 //

    BOOLEAN                 bHTProtect;
    BOOLEAN                 bMIMOPSEnable;
    BOOLEAN					bBADecline;
//2008/11/05: KH add to support Antenna power-saving of AP<--
	BOOLEAN					bGreenAPEnable;
	BOOLEAN					bBlockAntDivforGreenAP;
//2008/11/05: KH add to support Antenna power-saving of AP-->
	BOOLEAN					bDisableReordering;
	BOOLEAN					bForty_Mhz_Intolerant;
	BOOLEAN					bExtChannelSwitchAnnouncement;
	BOOLEAN					bRcvBSSWidthTriggerEvents;
	ULONG					LastRcvBSSWidthTriggerEventsTime;

	UCHAR					TxBASize;
#endif // DOT11_N_SUPPORT //

	// Enable wireless event
	BOOLEAN				bWirelessEvent;
	BOOLEAN				bWiFiTest;				// Enable this parameter for WiFi test

	// Tx & Rx Stream number selection
	UCHAR				TxStream;
	UCHAR				RxStream;

	// transmit phy mode, trasmit rate for Multicast.
#ifdef MCAST_RATE_SPECIFIC
	UCHAR				McastTransmitMcs;
	UCHAR				McastTransmitPhyMode;
#endif // MCAST_RATE_SPECIFIC //

	BOOLEAN			bHardwareRadio;     // Hardware controlled Radio enabled



	NDIS_SPIN_LOCK			MeasureReqTabLock;
	PMEASURE_REQ_TAB		pMeasureReqTab;

	NDIS_SPIN_LOCK			TpcReqTabLock;
	PTPC_REQ_TAB			pTpcReqTab;

	// transmit phy mode, trasmit rate for Multicast.
#ifdef MCAST_RATE_SPECIFIC
	HTTRANSMIT_SETTING		MCastPhyMode;
#endif // MCAST_RATE_SPECIFIC //

#ifdef SINGLE_SKU
	UINT16					DefineMaxTxPwr;
#endif // SINGLE_SKU //


	BOOLEAN				PSPXlink;  // 0: Disable. 1: Enable


#if defined(RT305x)||defined(RT30xx)
	// request by Gary, for High Power issue
	UCHAR	HighPowerPatchDisabled;
#endif

	BOOLEAN		HT_DisallowTKIP;		/* Restrict the encryption type in 11n HT mode */
} COMMON_CONFIG, *PCOMMON_CONFIG;


#ifdef CONFIG_STA_SUPPORT
/* Modified by Wu Xi-Kun 4/21/2006 */
// STA configuration and status
typedef struct _STA_ADMIN_CONFIG {
	// GROUP 1 -
	//   User configuration loaded from Registry, E2PROM or OID_xxx. These settings describe
	//   the user intended configuration, but not necessary fully equal to the final
	//   settings in ACTIVE BSS after negotiation/compromize with the BSS holder (either
	//   AP or IBSS holder).
	//   Once initialized, user configuration can only be changed via OID_xxx
	UCHAR       BssType;              // BSS_INFRA or BSS_ADHOC
	USHORT      AtimWin;          // used when starting a new IBSS

	// GROUP 2 -
	//   User configuration loaded from Registry, E2PROM or OID_xxx. These settings describe
	//   the user intended configuration, and should be always applied to the final
	//   settings in ACTIVE BSS without compromising with the BSS holder.
	//   Once initialized, user configuration can only be changed via OID_xxx
	UCHAR       RssiTrigger;
	UCHAR       RssiTriggerMode;      // RSSI_TRIGGERED_UPON_BELOW_THRESHOLD or RSSI_TRIGGERED_UPON_EXCCEED_THRESHOLD
	USHORT      DefaultListenCount;   // default listen count;
	ULONG       WindowsPowerMode;           // Power mode for AC power
	ULONG       WindowsBatteryPowerMode;    // Power mode for battery if exists
	BOOLEAN     bWindowsACCAMEnable;        // Enable CAM power mode when AC on
	BOOLEAN     bAutoReconnect;         // Set to TRUE when setting OID_802_11_SSID with no matching BSSID
	ULONG       WindowsPowerProfile;    // Windows power profile, for NDIS5.1 PnP

	// MIB:ieee802dot11.dot11smt(1).dot11StationConfigTable(1)
	USHORT      Psm;                  // power management mode   (PWR_ACTIVE|PWR_SAVE)
	USHORT      DisassocReason;
	UCHAR       DisassocSta[MAC_ADDR_LEN];
	USHORT      DeauthReason;
	UCHAR       DeauthSta[MAC_ADDR_LEN];
	USHORT      AuthFailReason;
	UCHAR       AuthFailSta[MAC_ADDR_LEN];

	NDIS_802_11_PRIVACY_FILTER          PrivacyFilter;  // PrivacyFilter enum for 802.1X
	NDIS_802_11_AUTHENTICATION_MODE     AuthMode;       // This should match to whatever microsoft defined
	NDIS_802_11_WEP_STATUS              WepStatus;
	NDIS_802_11_WEP_STATUS				OrigWepStatus;	// Original wep status set from OID

	// Add to support different cipher suite for WPA2/WPA mode
	NDIS_802_11_ENCRYPTION_STATUS		GroupCipher;		// Multicast cipher suite
	NDIS_802_11_ENCRYPTION_STATUS		PairCipher;			// Unicast cipher suite
	BOOLEAN								bMixCipher;			// Indicate current Pair & Group use different cipher suites
	USHORT								RsnCapability;

	NDIS_802_11_WEP_STATUS              GroupKeyWepStatus;

	UCHAR		WpaPassPhrase[64];		// WPA PSK pass phrase
	UINT		WpaPassPhraseLen;		// the length of WPA PSK pass phrase
	UCHAR		PMK[32];                // WPA PSK mode PMK
	UCHAR       PTK[64];                // WPA PSK mode PTK
	UCHAR		GTK[32];				// GTK from authenticator
	BSSID_INFO	SavedPMK[PMKID_NO];
	UINT		SavedPMKNum;			// Saved PMKID number

	UCHAR		DefaultKeyId;


	// WPA 802.1x port control, WPA_802_1X_PORT_SECURED, WPA_802_1X_PORT_NOT_SECURED
	UCHAR       PortSecured;

	// For WPA countermeasures
	ULONG       LastMicErrorTime;   // record last MIC error time
	ULONG       MicErrCnt;          // Should be 0, 1, 2, then reset to zero (after disassoiciation).
	BOOLEAN     bBlockAssoc;        // Block associate attempt for 60 seconds after counter measure occurred.
	// For WPA-PSK supplicant state
	WPA_STATE   WpaState;           // Default is SS_NOTUSE and handled by microsoft 802.1x
	UCHAR       ReplayCounter[8];
	UCHAR       ANonce[32];         // ANonce for WPA-PSK from aurhenticator
	UCHAR       SNonce[32];         // SNonce for WPA-PSK

	UCHAR       LastSNR0;             // last received BEACON's SNR
	UCHAR       LastSNR1;            // last received BEACON's SNR for 2nd  antenna
	RSSI_SAMPLE RssiSample;
	ULONG       NumOfAvgRssiSample;

	ULONG       LastBeaconRxTime;     // OS's timestamp of the last BEACON RX time
	ULONG       Last11bBeaconRxTime;  // OS's timestamp of the last 11B BEACON RX time
	ULONG		Last11gBeaconRxTime;	// OS's timestamp of the last 11G BEACON RX time
	ULONG		Last20NBeaconRxTime;	// OS's timestamp of the last 20MHz N BEACON RX time

	ULONG       LastScanTime;       // Record last scan time for issue BSSID_SCAN_LIST
	ULONG       ScanCnt;            // Scan counts since most recent SSID, BSSID, SCAN OID request
	BOOLEAN     bSwRadio;           // Software controlled Radio On/Off, TRUE: On
	BOOLEAN     bHwRadio;           // Hardware controlled Radio On/Off, TRUE: On
	BOOLEAN     bRadio;             // Radio state, And of Sw & Hw radio state
	BOOLEAN     bHardwareRadio;     // Hardware controlled Radio enabled
	BOOLEAN     bShowHiddenSSID;    // Show all known SSID in SSID list get operation

	// New for WPA, windows want us to to keep association information and
	// Fixed IEs from last association response
	NDIS_802_11_ASSOCIATION_INFORMATION     AssocInfo;
	USHORT       ReqVarIELen;                // Length of next VIE include EID & Length
	UCHAR       ReqVarIEs[MAX_VIE_LEN];		// The content saved here should be little-endian format.
	USHORT       ResVarIELen;                // Length of next VIE include EID & Length
	UCHAR       ResVarIEs[MAX_VIE_LEN];

	UCHAR       RSNIE_Len;
	UCHAR       RSN_IE[MAX_LEN_OF_RSNIE];	// The content saved here should be little-endian format.

	ULONG               CLBusyBytes;                // Save the total bytes received durning channel load scan time
	USHORT              RPIDensity[8];              // Array for RPI density collection

	UCHAR               RMReqCnt;                   // Number of measurement request saved.
	UCHAR               CurrentRMReqIdx;            // Number of measurement request saved.
	BOOLEAN             ParallelReq;                // Parallel measurement, only one request performed,
													// It must be the same channel with maximum duration
	USHORT              ParallelDuration;           // Maximum duration for parallel measurement
	UCHAR               ParallelChannel;            // Only one channel with parallel measurement
	USHORT              IAPPToken;                  // IAPP dialog token
	// Hack for channel load and noise histogram parameters
	UCHAR               NHFactor;                   // Parameter for Noise histogram
	UCHAR               CLFactor;                   // Parameter for channel load

	RALINK_TIMER_STRUCT	StaQuickResponeForRateUpTimer;
	BOOLEAN				StaQuickResponeForRateUpTimerRunning;

	UCHAR			DtimCount;      // 0.. DtimPeriod-1
	UCHAR			DtimPeriod;     // default = 3

#ifdef QOS_DLS_SUPPORT
	RT_802_11_DLS		DLSEntry[MAX_NUM_OF_DLS_ENTRY];
	UCHAR				DlsReplayCounter[8];
#endif // QOS_DLS_SUPPORT //
	////////////////////////////////////////////////////////////////////////////////////////
	// This is only for WHQL test.
	BOOLEAN				WhqlTest;
	////////////////////////////////////////////////////////////////////////////////////////

    RALINK_TIMER_STRUCT WpaDisassocAndBlockAssocTimer;
    // Fast Roaming
	BOOLEAN		        bAutoRoaming;       // 0:disable auto roaming by RSSI, 1:enable auto roaming by RSSI
	CHAR		        dBmToRoam;          // the condition to roam when receiving Rssi less than this value. It's negative value.

#ifdef WPA_SUPPLICANT_SUPPORT
    BOOLEAN             IEEE8021X;
    BOOLEAN             IEEE8021x_required_keys;
    CIPHER_KEY	        DesireSharedKey[4];	// Record user desired WEP keys
    UCHAR               DesireSharedKeyId;

    // 0: driver ignores wpa_supplicant
    // 1: wpa_supplicant initiates scanning and AP selection
    // 2: driver takes care of scanning, AP selection, and IEEE 802.11 association parameters
    UCHAR               WpaSupplicantUP;
	UCHAR				WpaSupplicantScanCount;
	BOOLEAN				bRSN_IE_FromWpaSupplicant;
#endif // WPA_SUPPLICANT_SUPPORT //

    CHAR                dev_name[16];
    USHORT              OriDevType;

    BOOLEAN             bTGnWifiTest;
	BOOLEAN			    bScanReqIsFromWebUI;

	HTTRANSMIT_SETTING				HTPhyMode, MaxHTPhyMode, MinHTPhyMode;// For transmit phy setting in TXWI.
	DESIRED_TRANSMIT_SETTING	DesiredTransmitSetting;
	RT_HT_PHY_INFO					DesiredHtPhyInfo;
	BOOLEAN							bAutoTxRateSwitch;

#ifdef RTMP_MAC_PCI
    UCHAR       BBPR3;
	// PS Control has 2 meanings for advanced power save function.
	// 1. EnablePSinIdle : When no connection, always radio off except need to do site survey.
	// 2. EnableNewPS  : will save more current in sleep or radio off mode.
	PS_CONTROL				PSControl;
#endif // RTMP_MAC_PCI //

#ifdef EXT_BUILD_CHANNEL_LIST
	UCHAR				IEEE80211dClientMode;
	UCHAR				StaOriCountryCode[3];
	UCHAR				StaOriGeography;
#endif // EXT_BUILD_CHANNEL_LIST //



	BOOLEAN				bAutoConnectByBssid;
	ULONG				BeaconLostTime;	// seconds
	BOOLEAN			bForceTxBurst;          // 1: force enble TX PACKET BURST, 0: disable
} STA_ADMIN_CONFIG, *PSTA_ADMIN_CONFIG;

// This data structure keep the current active BSS/IBSS's configuration that this STA
// had agreed upon joining the network. Which means these parameters are usually decided
// by the BSS/IBSS creator instead of user configuration. Data in this data structurre
// is valid only when either ADHOC_ON(pAd) or INFRA_ON(pAd) is TRUE.
// Normally, after SCAN or failed roaming attempts, we need to recover back to
// the current active settings.
typedef struct _STA_ACTIVE_CONFIG {
	USHORT      Aid;
	USHORT      AtimWin;                // in kusec; IBSS parameter set element
	USHORT      CapabilityInfo;
	USHORT      CfpMaxDuration;
	USHORT      CfpPeriod;

	// Copy supported rate from desired AP's beacon. We are trying to match
	// AP's supported and extended rate settings.
	UCHAR       SupRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR       ExtRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR       SupRateLen;
	UCHAR       ExtRateLen;
	// Copy supported ht from desired AP's beacon. We are trying to match
	RT_HT_PHY_INFO		SupportedPhyInfo;
	RT_HT_CAPABILITY	SupportedHtPhy;
} STA_ACTIVE_CONFIG, *PSTA_ACTIVE_CONFIG;



#endif // CONFIG_STA_SUPPORT //



typedef struct _MAC_TABLE_ENTRY {
	//Choose 1 from ValidAsWDS and ValidAsCLI  to validize.
	BOOLEAN		ValidAsCLI;		// Sta mode, set this TRUE after Linkup,too.
	BOOLEAN		ValidAsWDS;	// This is WDS Entry. only for AP mode.
	BOOLEAN		ValidAsApCli;   //This is a AP-Client entry, only for AP mode which enable AP-Client functions.
	BOOLEAN		ValidAsMesh;
	BOOLEAN		ValidAsDls;	// This is DLS Entry. only for STA mode.
	BOOLEAN		isCached;
	BOOLEAN		bIAmBadAtheros;	// Flag if this is Atheros chip that has IOT problem.  We need to turn on RTS/CTS protection.

	UCHAR		EnqueueEapolStartTimerRunning;  // Enqueue EAPoL-Start for triggering EAP SM
	//jan for wpa
	// record which entry revoke MIC Failure , if it leaves the BSS itself, AP won't update aMICFailTime MIB
	UCHAR           CMTimerRunning;
	UCHAR           apidx;			// MBSS number
	UCHAR           RSNIE_Len;
	UCHAR           RSN_IE[MAX_LEN_OF_RSNIE];
	UCHAR           ANonce[LEN_KEY_DESC_NONCE];
	UCHAR           SNonce[LEN_KEY_DESC_NONCE];
	UCHAR           R_Counter[LEN_KEY_DESC_REPLAY];
	UCHAR           PTK[64];
	UCHAR           ReTryCounter;
	RALINK_TIMER_STRUCT                 RetryTimer;
	RALINK_TIMER_STRUCT					EnqueueStartForPSKTimer;	// A timer which enqueue EAPoL-Start for triggering PSK SM
	NDIS_802_11_AUTHENTICATION_MODE     AuthMode;   // This should match to whatever microsoft defined
	NDIS_802_11_WEP_STATUS              WepStatus;
	NDIS_802_11_WEP_STATUS              GroupKeyWepStatus;
	AP_WPA_STATE    WpaState;
	GTK_STATE       GTKState;
	USHORT          PortSecured;
	NDIS_802_11_PRIVACY_FILTER  PrivacyFilter;      // PrivacyFilter enum for 802.1X
	CIPHER_KEY      PairwiseKey;
	PVOID           pAd;
	INT				PMKID_CacheIdx;
	UCHAR			PMKID[LEN_PMKID];


	UCHAR           Addr[MAC_ADDR_LEN];
	UCHAR           PsMode;
	SST             Sst;
	AUTH_STATE      AuthState; // for SHARED KEY authentication state machine used only
	BOOLEAN			IsReassocSta;	// Indicate whether this is a reassociation procedure
	USHORT          Aid;
	USHORT          CapabilityInfo;
	UCHAR           LastRssi;
	ULONG           NoDataIdleCount;
	UINT16			StationKeepAliveCount; // unit: second
	ULONG           PsQIdleCount;
	QUEUE_HEADER    PsQueue;

	UINT32			StaConnectTime;		// the live time of this station since associated with AP


#ifdef DOT11_N_SUPPORT
	BOOLEAN			bSendBAR;
	USHORT			NoBADataCountDown;

	UINT32			CachedBuf[16];		// UINT (4 bytes) for alignment
	UINT			TxBFCount; // 3*3
#endif // DOT11_N_SUPPORT //
	UINT			FIFOCount;
	UINT			DebugFIFOCount;
	UINT			DebugTxCount;
    BOOLEAN			bDlsInit;


//====================================================
//WDS entry needs these
// if ValidAsWDS==TRUE, MatchWDSTabIdx is the index in WdsTab.MacTab
	UINT			MatchWDSTabIdx;
	UCHAR           MaxSupportedRate;
	UCHAR           CurrTxRate;
	UCHAR           CurrTxRateIndex;
	// to record the each TX rate's quality. 0 is best, the bigger the worse.
	USHORT          TxQuality[MAX_STEP_OF_TX_RATE_SWITCH];
//	USHORT          OneSecTxOkCount;
	UINT32			OneSecTxNoRetryOkCount;
	UINT32          OneSecTxRetryOkCount;
	UINT32          OneSecTxFailCount;
	UINT32			ContinueTxFailCnt;
	UINT32          CurrTxRateStableTime; // # of second in current TX rate
	UCHAR           TxRateUpPenalty;      // extra # of second penalty due to last unstable condition
#ifdef WDS_SUPPORT
	BOOLEAN		LockEntryTx; // TRUE = block to WDS Entry traffic, FALSE = not.
	UINT32		TimeStamp_toTxRing;
#endif // WDS_SUPPORT //

//====================================================



#ifdef CONFIG_STA_SUPPORT
#ifdef QOS_DLS_SUPPORT
	UINT			MatchDlsEntryIdx; // indicate the index in pAd->StaCfg.DLSEntry
#endif // QOS_DLS_SUPPORT //
#endif // CONFIG_STA_SUPPORT //

	BOOLEAN         fNoisyEnvironment;
	BOOLEAN			fLastSecAccordingRSSI;
	UCHAR           LastSecTxRateChangeAction; // 0: no change, 1:rate UP, 2:rate down
	CHAR			LastTimeTxRateChangeAction; //Keep last time value of LastSecTxRateChangeAction
	ULONG			LastTxOkCount;
	UCHAR           PER[MAX_STEP_OF_TX_RATE_SWITCH];

	// a bitmap of BOOLEAN flags. each bit represent an operation status of a particular
	// BOOLEAN control, either ON or OFF. These flags should always be accessed via
	// CLIENT_STATUS_TEST_FLAG(), CLIENT_STATUS_SET_FLAG(), CLIENT_STATUS_CLEAR_FLAG() macros.
	// see fOP_STATUS_xxx in RTMP_DEF.C for detail bit definition. fCLIENT_STATUS_AMSDU_INUSED
	ULONG           ClientStatusFlags;

	HTTRANSMIT_SETTING	HTPhyMode, MaxHTPhyMode, MinHTPhyMode;// For transmit phy setting in TXWI.

#ifdef DOT11_N_SUPPORT
	// HT EWC MIMO-N used parameters
	USHORT		RXBAbitmap;	// fill to on-chip  RXWI_BA_BITMASK in 8.1.3RX attribute entry format
	USHORT		TXBAbitmap;	// This bitmap as originator, only keep in software used to mark AMPDU bit in TXWI
	USHORT		TXAutoBAbitmap;
	USHORT		BADeclineBitmap;
	USHORT		BARecWcidArray[NUM_OF_TID];	// The mapping wcid of recipient session. if RXBAbitmap bit is masked
	USHORT		BAOriWcidArray[NUM_OF_TID]; // The mapping wcid of originator session. if TXBAbitmap bit is masked
	USHORT		BAOriSequence[NUM_OF_TID]; // The mapping wcid of originator session. if TXBAbitmap bit is masked

	// 802.11n features.
	UCHAR		MpduDensity;
	UCHAR		MaxRAmpduFactor;
	UCHAR		AMsduSize;
	UCHAR		MmpsMode;	// MIMO power save more.

	HT_CAPABILITY_IE		HTCapability;

#ifdef DOT11N_DRAFT3
	UCHAR		BSS2040CoexistenceMgmtSupport;
#endif // DOT11N_DRAFT3 //
#endif // DOT11_N_SUPPORT //

	BOOLEAN		bAutoTxRateSwitch;

	UCHAR       RateLen;
	struct _MAC_TABLE_ENTRY *pNext;
    USHORT      TxSeq[NUM_OF_TID];
	USHORT		NonQosDataSeq;

	RSSI_SAMPLE	RssiSample;

	UINT32			TXMCSExpected[16];
	UINT32			TXMCSSuccessful[16];
	UINT32			TXMCSFailed[16];
	UINT32			TXMCSAutoFallBack[16][16];

#ifdef CONFIG_STA_SUPPORT
	ULONG			LastBeaconRxTime;
#endif // CONFIG_STA_SUPPORT //



	ULONG AssocDeadLine;



	ULONG ChannelQuality;  // 0..100, Channel Quality Indication for Roaming

} MAC_TABLE_ENTRY, *PMAC_TABLE_ENTRY;

typedef struct _MAC_TABLE {
	USHORT			Size;
	MAC_TABLE_ENTRY *Hash[HASH_TABLE_SIZE];
	MAC_TABLE_ENTRY Content[MAX_LEN_OF_MAC_TABLE];
	QUEUE_HEADER    McastPsQueue;
	ULONG           PsQIdleCount;
	BOOLEAN         fAnyStationInPsm;
	BOOLEAN         fAnyStationBadAtheros;	// Check if any Station is atheros 802.11n Chip.  We need to use RTS/CTS with Atheros 802,.11n chip.
	BOOLEAN			fAnyTxOPForceDisable;	// Check if it is necessary to disable BE TxOP
	BOOLEAN			fAllStationAsRalink;	// Check if all stations are ralink-chipset
#ifdef DOT11_N_SUPPORT
	BOOLEAN         fAnyStationIsLegacy;	// Check if I use legacy rate to transmit to my BSS Station/
	BOOLEAN         fAnyStationNonGF;		// Check if any Station can't support GF.
	BOOLEAN         fAnyStation20Only;		// Check if any Station can't support GF.
	BOOLEAN			fAnyStationMIMOPSDynamic; // Check if any Station is MIMO Dynamic
	BOOLEAN         fAnyBASession;   // Check if there is BA session.  Force turn on RTS/CTS
//2008/10/28: KH add to support Antenna power-saving of AP<--
//2008/10/28: KH add to support Antenna power-saving of AP-->
#endif // DOT11_N_SUPPORT //
} MAC_TABLE, *PMAC_TABLE;




#ifdef BLOCK_NET_IF
typedef struct _BLOCK_QUEUE_ENTRY
{
	BOOLEAN SwTxQueueBlockFlag;
	LIST_HEADER NetIfList;
} BLOCK_QUEUE_ENTRY, *PBLOCK_QUEUE_ENTRY;
#endif // BLOCK_NET_IF //


struct wificonf
{
	BOOLEAN	bShortGI;
	BOOLEAN bGreenField;
};


typedef struct _RTMP_DEV_INFO_
{
	UCHAR			chipName[16];
	RTMP_INF_TYPE	infType;
}RTMP_DEV_INFO;


#ifdef DBG_DIAGNOSE
#define DIAGNOSE_TIME	10   // 10 sec
typedef struct _RtmpDiagStrcut_
{	// Diagnosis Related element
	unsigned char		inited;
	unsigned char	qIdx;
	unsigned char	ArrayStartIdx;
	unsigned char		ArrayCurIdx;
	// Tx Related Count
	USHORT			TxDataCnt[DIAGNOSE_TIME];
	USHORT			TxFailCnt[DIAGNOSE_TIME];
//	USHORT			TxDescCnt[DIAGNOSE_TIME][16];		// TxDesc queue length in scale of 0~14, >=15
	USHORT			TxDescCnt[DIAGNOSE_TIME][24]; // 3*3	// TxDesc queue length in scale of 0~14, >=15
//	USHORT			TxMcsCnt[DIAGNOSE_TIME][16];			// TxDate MCS Count in range from 0 to 15, step in 1.
	USHORT			TxMcsCnt[DIAGNOSE_TIME][24]; // 3*3
	USHORT			TxSWQueCnt[DIAGNOSE_TIME][9];		// TxSwQueue length in scale of 0, 1, 2, 3, 4, 5, 6, 7, >=8

	USHORT			TxAggCnt[DIAGNOSE_TIME];
	USHORT			TxNonAggCnt[DIAGNOSE_TIME];
//	USHORT			TxAMPDUCnt[DIAGNOSE_TIME][16];		// 10 sec, TxDMA APMDU Aggregation count in range from 0 to 15, in setp of 1.
	USHORT			TxAMPDUCnt[DIAGNOSE_TIME][24]; // 3*3 // 10 sec, TxDMA APMDU Aggregation count in range from 0 to 15, in setp of 1.
	USHORT			TxRalinkCnt[DIAGNOSE_TIME];			// TxRalink Aggregation Count in 1 sec scale.
	USHORT			TxAMSDUCnt[DIAGNOSE_TIME];			// TxAMSUD Aggregation Count in 1 sec scale.

	// Rx Related Count
	USHORT			RxDataCnt[DIAGNOSE_TIME];			// Rx Total Data count.
	USHORT			RxCrcErrCnt[DIAGNOSE_TIME];
//	USHORT			RxMcsCnt[DIAGNOSE_TIME][16];		// Rx MCS Count in range from 0 to 15, step in 1.
	USHORT			RxMcsCnt[DIAGNOSE_TIME][24]; // 3*3
}RtmpDiagStruct;
#endif // DBG_DIAGNOSE //


struct _RTMP_CHIP_OP_
{
	/*  Calibration access related callback functions */
	int (*eeinit)(RTMP_ADAPTER *pAd);										/* int (*eeinit)(RTMP_ADAPTER *pAd); */
	int (*eeread)(RTMP_ADAPTER *pAd, USHORT offset, PUSHORT pValue);				/* int (*eeread)(RTMP_ADAPTER *pAd, int offset, PUSHORT pValue); */
	int (*eewrite)(RTMP_ADAPTER *pAd, USHORT offset, USHORT value);;				/* int (*eewrite)(RTMP_ADAPTER *pAd, int offset, USHORT value); */

	/* MCU related callback functions */
	int (*loadFirmware)(RTMP_ADAPTER *pAd);								/* int (*loadFirmware)(RTMP_ADAPTER *pAd); */
	int (*eraseFirmware)(RTMP_ADAPTER *pAd);								/* int (*eraseFirmware)(RTMP_ADAPTER *pAd); */
	int (*sendCommandToMcu)(RTMP_ADAPTER *pAd, UCHAR cmd, UCHAR token, UCHAR arg0, UCHAR arg1);;	/* int (*sendCommandToMcu)(RTMP_ADAPTER *pAd, UCHAR cmd, UCHAR token, UCHAR arg0, UCHAR arg1); */

	/* RF access related callback functions */
	REG_PAIR *pRFRegTable;
	void (*AsicRfInit)(RTMP_ADAPTER *pAd);
	void (*AsicRfTurnOn)(RTMP_ADAPTER *pAd);
	void (*AsicRfTurnOff)(RTMP_ADAPTER *pAd);
	void (*AsicReverseRfFromSleepMode)(RTMP_ADAPTER *pAd);
	void (*AsicHaltAction)(RTMP_ADAPTER *pAd);
};


//
//  The miniport adapter structure
//
struct _RTMP_ADAPTER
{
	PVOID					OS_Cookie;	// save specific structure relative to OS
	PNET_DEV				net_dev;
	ULONG					VirtualIfCnt;

	RTMP_CHIP_OP			chipOps;
	USHORT					ThisTbttNumToNextWakeUp;

#ifdef INF_AMAZON_PPA
	UINT32  g_if_id;
	BOOLEAN	PPAEnable;
	PPA_DIRECTPATH_CB       *pDirectpathCb;
#endif // INF_AMAZON_PPA //

#ifdef RTMP_MAC_PCI
/*****************************************************************************************/
/*      PCI related parameters																  */
/*****************************************************************************************/
	PUCHAR                  CSRBaseAddress;     // PCI MMIO Base Address, all access will use
	unsigned int			irq_num;

	USHORT		            LnkCtrlBitMask;
	USHORT		            RLnkCtrlConfiguration;
	USHORT                  RLnkCtrlOffset;
	USHORT		            HostLnkCtrlConfiguration;
	USHORT                  HostLnkCtrlOffset;
	USHORT		            PCIePowerSaveLevel;
	ULONG				Rt3xxHostLinkCtrl;	// USed for 3090F chip
	ULONG				Rt3xxRalinkLinkCtrl;	// USed for 3090F chip
	USHORT				DeviceID;           // Read from PCI config
	ULONG				AccessBBPFailCount;
	BOOLEAN					bPCIclkOff;						// flag that indicate if the PICE power status in Configuration SPace..
	BOOLEAN					bPCIclkOffDisableTx;			//

	BOOLEAN					brt30xxBanMcuCmd;	//when = 0xff means all commands are ok to set .
	BOOLEAN					b3090ESpecialChip;	//3090E special chip that write EEPROM 0x24=0x9280.
	ULONG					CheckDmaBusyCount;  // Check Interrupt Status Register Count.

	UINT					int_enable_reg;
	UINT					int_disable_mask;
	UINT					int_pending;


	RTMP_DMABUF             TxBufSpace[NUM_OF_TX_RING]; // Shared memory of all 1st pre-allocated TxBuf associated with each TXD
	RTMP_DMABUF             RxDescRing;                 // Shared memory for RX descriptors
	RTMP_DMABUF             TxDescRing[NUM_OF_TX_RING];	// Shared memory for Tx descriptors
	RTMP_TX_RING            TxRing[NUM_OF_TX_RING];		// AC0~4 + HCCA
#endif // RTMP_MAC_PCI //


	NDIS_SPIN_LOCK		irq_lock;
	UCHAR				irq_disabled;


/*****************************************************************************************/
/*      RBUS related parameters																  */
/*****************************************************************************************/


/*****************************************************************************************/
/*      Both PCI/USB related parameters														  */
/*****************************************************************************************/
	//RTMP_DEV_INFO			chipInfo;
	RTMP_INF_TYPE			infType;

/*****************************************************************************************/
/*      Driver Mgmt related parameters														  */
/*****************************************************************************************/
	RTMP_OS_TASK			mlmeTask;
#ifdef RTMP_TIMER_TASK_SUPPORT
	// If you want use timer task to handle the timer related jobs, enable this.
	RTMP_TIMER_TASK_QUEUE	TimerQ;
	NDIS_SPIN_LOCK			TimerQLock;
	RTMP_OS_TASK			timerTask;
#endif // RTMP_TIMER_TASK_SUPPORT //


/*****************************************************************************************/
/*      Tx related parameters                                                           */
/*****************************************************************************************/
	BOOLEAN                 DeQueueRunning[NUM_OF_TX_RING];  // for ensuring RTUSBDeQueuePacket get call once
	NDIS_SPIN_LOCK          DeQueueLock[NUM_OF_TX_RING];


	// resource for software backlog queues
	QUEUE_HEADER            TxSwQueue[NUM_OF_TX_RING];  // 4 AC + 1 HCCA
	NDIS_SPIN_LOCK          TxSwQueueLock[NUM_OF_TX_RING];	// TxSwQueue spinlock

	RTMP_DMABUF             MgmtDescRing;			// Shared memory for MGMT descriptors
	RTMP_MGMT_RING          MgmtRing;
	NDIS_SPIN_LOCK          MgmtRingLock;			// Prio Ring spinlock


/*****************************************************************************************/
/*      Rx related parameters                                                           */
/*****************************************************************************************/

#ifdef RTMP_MAC_PCI
	RTMP_RX_RING            RxRing;
	NDIS_SPIN_LOCK          RxRingLock;                 // Rx Ring spinlock
#ifdef RT3090
	NDIS_SPIN_LOCK          McuCmdLock;              //MCU Command Queue spinlock
#endif // RT3090 //
#endif // RTMP_MAC_PCI //



/*****************************************************************************************/
/*      ASIC related parameters                                                          */
/*****************************************************************************************/
	UINT32			MACVersion;		// MAC version. Record rt2860C(0x28600100) or rt2860D (0x28600101)..

	// ---------------------------
	// E2PROM
	// ---------------------------
	ULONG				EepromVersion;          // byte 0: version, byte 1: revision, byte 2~3: unused
	ULONG				FirmwareVersion;        // byte 0: Minor version, byte 1: Major version, otherwise unused.
	USHORT				EEPROMDefaultValue[NUM_EEPROM_BBP_PARMS];
	UCHAR				EEPROMAddressNum;       // 93c46=6  93c66=8
	BOOLEAN				EepromAccess;
	UCHAR				EFuseTag;


	// ---------------------------
	// BBP Control
	// ---------------------------
#ifdef MERGE_ARCH_TEAM
	UCHAR                   BbpWriteLatch[256];     // record last BBP register value written via BBP_IO_WRITE/BBP_IO_WRITE_VY_REG_ID
#else
	UCHAR                   BbpWriteLatch[140];     // record last BBP register value written via BBP_IO_WRITE/BBP_IO_WRITE_VY_REG_ID
#endif // MERGE_ARCH_TEAM //
	CHAR					BbpRssiToDbmDelta;		// change from UCHAR to CHAR for high power
	BBP_R66_TUNING          BbpTuning;

	// ----------------------------
	// RFIC control
	// ----------------------------
	UCHAR                   RfIcType;       // RFIC_xxx
	ULONG                   RfFreqOffset;   // Frequency offset for channel switching
	RTMP_RF_REGS            LatchRfRegs;    // latch th latest RF programming value since RF IC doesn't support READ

	EEPROM_ANTENNA_STRUC    Antenna;                            // Since ANtenna definition is different for a & g. We need to save it for future reference.
	EEPROM_NIC_CONFIG2_STRUC    NicConfig2;

	// This soft Rx Antenna Diversity mechanism is used only when user set
	// RX Antenna = DIVERSITY ON
	SOFT_RX_ANT_DIVERSITY   RxAnt;

	UCHAR                   RFProgSeq;
	CHANNEL_TX_POWER        TxPower[MAX_NUM_OF_CHANNELS];       // Store Tx power value for all channels.
	CHANNEL_TX_POWER        ChannelList[MAX_NUM_OF_CHANNELS];   // list all supported channels for site survey
	CHANNEL_11J_TX_POWER    TxPower11J[MAX_NUM_OF_11JCHANNELS];       // 802.11j channel and bw
	CHANNEL_11J_TX_POWER    ChannelList11J[MAX_NUM_OF_11JCHANNELS];   // list all supported channels for site survey

	UCHAR                   ChannelListNum;                     // number of channel in ChannelList[]
	UCHAR					Bbp94;
	BOOLEAN					BbpForCCK;
	ULONG		Tx20MPwrCfgABand[5];
	ULONG		Tx20MPwrCfgGBand[5];
	ULONG		Tx40MPwrCfgABand[5];
	ULONG		Tx40MPwrCfgGBand[5];

	BOOLEAN     bAutoTxAgcA;                // Enable driver auto Tx Agc control
	UCHAR	    TssiRefA;					// Store Tssi reference value as 25 temperature.
	UCHAR	    TssiPlusBoundaryA[5];		// Tssi boundary for increase Tx power to compensate.
	UCHAR	    TssiMinusBoundaryA[5];		// Tssi boundary for decrease Tx power to compensate.
	UCHAR	    TxAgcStepA;					// Store Tx TSSI delta increment / decrement value
	CHAR		TxAgcCompensateA;			// Store the compensation (TxAgcStep * (idx-1))

	BOOLEAN     bAutoTxAgcG;                // Enable driver auto Tx Agc control
	UCHAR	    TssiRefG;					// Store Tssi reference value as 25 temperature.
	UCHAR	    TssiPlusBoundaryG[5];		// Tssi boundary for increase Tx power to compensate.
	UCHAR	    TssiMinusBoundaryG[5];		// Tssi boundary for decrease Tx power to compensate.
	UCHAR	    TxAgcStepG;					// Store Tx TSSI delta increment / decrement value
	CHAR		TxAgcCompensateG;			// Store the compensation (TxAgcStep * (idx-1))

	CHAR		BGRssiOffset0;				// Store B/G RSSI#0 Offset value on EEPROM 0x46h
	CHAR		BGRssiOffset1;				// Store B/G RSSI#1 Offset value
	CHAR		BGRssiOffset2;				// Store B/G RSSI#2 Offset value

	CHAR		ARssiOffset0;				// Store A RSSI#0 Offset value on EEPROM 0x4Ah
	CHAR		ARssiOffset1;				// Store A RSSI#1 Offset value
	CHAR		ARssiOffset2;				// Store A RSSI#2 Offset value

	CHAR		BLNAGain;					// Store B/G external LNA#0 value on EEPROM 0x44h
	CHAR		ALNAGain0;					// Store A external LNA#0 value for ch36~64
	CHAR		ALNAGain1;					// Store A external LNA#1 value for ch100~128
	CHAR		ALNAGain2;					// Store A external LNA#2 value for ch132~165
#ifdef RT30xx
	// for 3572
	UCHAR		Bbp25;
	UCHAR		Bbp26;

	UCHAR		TxMixerGain24G;				// Tx mixer gain value from EEPROM to improve Tx EVM / Tx DAC, 2.4G
	UCHAR		TxMixerGain5G;
#endif // RT30xx //
	// ----------------------------
	// LED control
	// ----------------------------
	MCU_LEDCS_STRUC		LedCntl;
	USHORT				Led1;	// read from EEPROM 0x3c
	USHORT				Led2;	// EEPROM 0x3e
	USHORT				Led3;	// EEPROM 0x40
	UCHAR				LedIndicatorStrength;
	UCHAR				RssiSingalstrengthOffet;
	BOOLEAN				bLedOnScanning;
	UCHAR				LedStatus;

/*****************************************************************************************/
/*      802.11 related parameters                                                        */
/*****************************************************************************************/
	// outgoing BEACON frame buffer and corresponding TXD
	TXWI_STRUC			BeaconTxWI;
	PUCHAR						BeaconBuf;
	USHORT						BeaconOffset[HW_BEACON_MAX_COUNT];

	// pre-build PS-POLL and NULL frame upon link up. for efficiency purpose.
	PSPOLL_FRAME			PsPollFrame;
	HEADER_802_11			NullFrame;




//=========AP===========


//=======STA===========
#ifdef CONFIG_STA_SUPPORT
	// -----------------------------------------------
	// STA specific configuration & operation status
	// used only when pAd->OpMode == OPMODE_STA
	// -----------------------------------------------
	STA_ADMIN_CONFIG        StaCfg;		// user desired settings
	STA_ACTIVE_CONFIG       StaActive;		// valid only when ADHOC_ON(pAd) || INFRA_ON(pAd)
	CHAR                    nickname[IW_ESSID_MAX_SIZE+1]; // nickname, only used in the iwconfig i/f
	NDIS_MEDIA_STATE        PreMediaState;
#endif // CONFIG_STA_SUPPORT //

//=======Common===========
	// OP mode: either AP or STA
	UCHAR                   OpMode;                     // OPMODE_STA, OPMODE_AP

	NDIS_MEDIA_STATE        IndicateMediaState;			// Base on Indication state, default is NdisMediaStateDisConnected


	/* MAT related parameters */

	// configuration: read from Registry & E2PROM
	BOOLEAN                 bLocalAdminMAC;             // Use user changed MAC
	UCHAR                   PermanentAddress[MAC_ADDR_LEN];    // Factory default MAC address
	UCHAR                   CurrentAddress[MAC_ADDR_LEN];      // User changed MAC address

	// ------------------------------------------------------
	// common configuration to both OPMODE_STA and OPMODE_AP
	// ------------------------------------------------------
	COMMON_CONFIG           CommonCfg;
	MLME_STRUCT             Mlme;

	// AP needs those vaiables for site survey feature.
	MLME_AUX                MlmeAux;           // temporary settings used during MLME state machine
	BSS_TABLE               ScanTab;           // store the latest SCAN result

	//About MacTab, the sta driver will use #0 and #1 for multicast and AP.
	MAC_TABLE                 MacTab;     // ASIC on-chip WCID entry table.  At TX, ASIC always use key according to this on-chip table.
	NDIS_SPIN_LOCK          MacTabLock;

#ifdef DOT11_N_SUPPORT
	BA_TABLE			BATable;
	NDIS_SPIN_LOCK          BATabLock;
	RALINK_TIMER_STRUCT RECBATimer;
#endif // DOT11_N_SUPPORT //

	// encryption/decryption KEY tables
	CIPHER_KEY              SharedKey[MAX_MBSSID_NUM][4]; // STA always use SharedKey[BSS0][0..3]

	// RX re-assembly buffer for fragmentation
	FRAGMENT_FRAME          FragFrame;                  // Frame storage for fragment frame

	// various Counters
	COUNTER_802_3           Counters8023;               // 802.3 counters
	COUNTER_802_11          WlanCounters;               // 802.11 MIB counters
	COUNTER_RALINK          RalinkCounters;             // Ralink propriety counters
	COUNTER_DRS             DrsCounters;                // counters for Dynamic TX Rate Switching
	PRIVATE_STRUC           PrivateInfo;                // Private information & counters

	// flags, see fRTMP_ADAPTER_xxx flags
	ULONG                   Flags;                      // Represent current device status
	ULONG                   PSFlags;                    // Power Save operation flag.

	// current TX sequence #
	USHORT                  Sequence;

	// Control disconnect / connect event generation
	//+++Didn't used anymore
	ULONG                   LinkDownTime;
	//---
	ULONG                   LastRxRate;
	ULONG                   LastTxRate;
	//+++Used only for Station
	BOOLEAN                 bConfigChanged;         // Config Change flag for the same SSID setting
	//---

	ULONG                   ExtraInfo;              // Extra information for displaying status
	ULONG                   SystemErrorBitmap;      // b0: E2PROM version error

	//+++Didn't used anymore
	ULONG                   MacIcVersion;           // MAC/BBP serial interface issue solved after ver.D
	//---

	// ---------------------------
	// System event log
	// ---------------------------
	RT_802_11_EVENT_TABLE   EventTab;


	BOOLEAN		HTCEnable;

	/*****************************************************************************************/
	/*      Statistic related parameters                                                     */
	/*****************************************************************************************/

	BOOLEAN						bUpdateBcnCntDone;
	ULONG						watchDogMacDeadlock;	// prevent MAC/BBP into deadlock condition
	// ----------------------------
	// DEBUG paramerts
	// ----------------------------
	//ULONG		DebugSetting[4];
	BOOLEAN		bBanAllBaSetup;
	BOOLEAN		bPromiscuous;

	// ----------------------------
	// rt2860c emulation-use Parameters
	// ----------------------------
	//ULONG		rtsaccu[30];
	//ULONG		ctsaccu[30];
	//ULONG		cfendaccu[30];
	//ULONG		bacontent[16];
	//ULONG		rxint[RX_RING_SIZE+1];
	//UCHAR		rcvba[60];
	BOOLEAN		bLinkAdapt;
	BOOLEAN		bForcePrintTX;
	BOOLEAN		bForcePrintRX;
	//BOOLEAN		bDisablescanning;		//defined in RT2870 USB
	BOOLEAN		bStaFifoTest;
	BOOLEAN		bProtectionTest;
	/*
	BOOLEAN		bHCCATest;
	BOOLEAN		bGenOneHCCA;
	*/
	BOOLEAN		bBroadComHT;
	//+++Following add from RT2870 USB.
	ULONG		BulkOutReq;
	ULONG		BulkOutComplete;
	ULONG		BulkOutCompleteOther;
	ULONG		BulkOutCompleteCancel;	// seems not use now?
	ULONG		BulkInReq;
	ULONG		BulkInComplete;
	ULONG		BulkInCompleteFail;
	//---

    struct wificonf			WIFItestbed;

#ifdef RALINK_ATE
	ATE_INFO				ate;
#endif // RALINK_ATE //

#ifdef DOT11_N_SUPPORT
	struct reordering_mpdu_pool mpdu_blk_pool;
#endif // DOT11_N_SUPPORT //

	ULONG					OneSecondnonBEpackets;		// record non BE packets per second

#ifdef LINUX
#if WIRELESS_EXT >= 12
    struct iw_statistics    iw_stats;
#endif

	struct net_device_stats	stats;
#endif // LINUX //

#ifdef BLOCK_NET_IF
	BLOCK_QUEUE_ENTRY		blockQueueTab[NUM_OF_TX_RING];
#endif // BLOCK_NET_IF //



#ifdef MULTIPLE_CARD_SUPPORT
	INT32					MC_RowID;
	STRING					MC_FileName[256];
#endif // MULTIPLE_CARD_SUPPORT //

	ULONG					TbttTickCount;
#ifdef PCI_MSI_SUPPORT
	BOOLEAN					HaveMsi;
#endif // PCI_MSI_SUPPORT //


	UCHAR					is_on;

#define TIME_BASE			(1000000/OS_HZ)
#define TIME_ONE_SECOND		(1000000/TIME_BASE)
	UCHAR					flg_be_adjust;
	ULONG					be_adjust_last_time;

#ifdef NINTENDO_AP
	NINDO_CTRL_BLOCK		nindo_ctrl_block;
#endif // NINTENDO_AP //


#ifdef IKANOS_VX_1X0
	struct IKANOS_TX_INFO	IkanosTxInfo;
	struct IKANOS_TX_INFO	IkanosRxInfo[MAX_MBSSID_NUM + MAX_WDS_ENTRY + MAX_APCLI_NUM + MAX_MESH_NUM];
#endif // IKANOS_VX_1X0 //


#ifdef DBG_DIAGNOSE
	RtmpDiagStruct	DiagStruct;
#endif // DBG_DIAGNOSE //


	UINT8					FlgCtsEnabled;
	UINT8					PM_FlgSuspend;

#ifdef RT30xx
#ifdef RTMP_EFUSE_SUPPORT
	BOOLEAN		bUseEfuse;
	BOOLEAN		bEEPROMFile;
	BOOLEAN		bFroceEEPROMBuffer;
	UCHAR		EEPROMImage[1024];
#endif // RTMP_EFUSE_SUPPORT //
#endif // RT30xx //

#ifdef CONFIG_STA_SUPPORT
#endif // CONFIG_STA_SUPPORT //

};



#ifdef TONE_RADAR_DETECT_SUPPORT
#define DELAYINTMASK		0x0013fffb
#define INTMASK				0x0013fffb
#define IndMask				0x0013fffc
#define RadarInt			0x00100000
#else
#define DELAYINTMASK		0x0003fffb
#define INTMASK				0x0003fffb
#define IndMask				0x0003fffc
#endif // TONE_RADAR_DETECT_SUPPORT //

#define RxINT				0x00000005	// Delayed Rx or indivi rx
#define TxDataInt			0x000000fa	// Delayed Tx or indivi tx
#define TxMgmtInt			0x00000102	// Delayed Tx or indivi tx
#define TxCoherent			0x00020000	// tx coherent
#define RxCoherent			0x00010000	// rx coherent
#define McuCommand			0x00000200	// mcu
#define PreTBTTInt			0x00001000	// Pre-TBTT interrupt
#define TBTTInt				0x00000800		// TBTT interrupt
#define GPTimeOutInt			0x00008000		// GPtimeout interrupt
#define AutoWakeupInt		0x00004000		// AutoWakeupInt interrupt
#define FifoStaFullInt			0x00002000	//  fifo statistics full interrupt


/***************************************************************************
  *	Rx Path software control block related data structures
  **************************************************************************/
typedef struct _RX_BLK_
{
//	RXD_STRUC		RxD; // sample
	RT28XX_RXD_STRUC	RxD;
	PRXWI_STRUC			pRxWI;
	PHEADER_802_11		pHeader;
	PNDIS_PACKET		pRxPacket;
	UCHAR				*pData;
	USHORT				DataSize;
	USHORT				Flags;
	UCHAR				UserPriority;	// for calculate TKIP MIC using
} RX_BLK;


#define RX_BLK_SET_FLAG(_pRxBlk, _flag)		(_pRxBlk->Flags |= _flag)
#define RX_BLK_TEST_FLAG(_pRxBlk, _flag)	(_pRxBlk->Flags & _flag)
#define RX_BLK_CLEAR_FLAG(_pRxBlk, _flag)	(_pRxBlk->Flags &= ~(_flag))


#define fRX_WDS			0x0001
#define fRX_AMSDU		0x0002
#define fRX_ARALINK		0x0004
#define fRX_HTC			0x0008
#define fRX_PAD			0x0010
#define fRX_AMPDU		0x0020
#define fRX_QOS			0x0040
#define fRX_INFRA		0x0080
#define fRX_EAP			0x0100
#define fRX_MESH		0x0200
#define fRX_APCLI		0x0400
#define fRX_DLS			0x0800
#define fRX_WPI			0x1000

#define LENGTH_AMSDU_SUBFRAMEHEAD	14
#define LENGTH_ARALINK_SUBFRAMEHEAD	14
#define LENGTH_ARALINK_HEADER_FIELD	 2


/***************************************************************************
  *	Tx Path software control block related data structures
  **************************************************************************/
#define TX_UNKOWN_FRAME		0x00
#define TX_MCAST_FRAME			0x01
#define TX_LEGACY_FRAME		0x02
#define TX_AMPDU_FRAME		0x04
#define TX_AMSDU_FRAME		0x08
#define TX_RALINK_FRAME		0x10
#define TX_FRAG_FRAME			0x20


//	Currently the sizeof(TX_BLK) is 148 bytes.
typedef struct _TX_BLK_
{
	UCHAR				QueIdx;
	UCHAR				TxFrameType;				// Indicate the Transmission type of the all frames in one batch
	UCHAR				TotalFrameNum;				// Total frame number want to send-out in one batch
	USHORT				TotalFragNum;				// Total frame fragments required in one batch
	USHORT				TotalFrameLen;				// Total length of all frames want to send-out in one batch

	QUEUE_HEADER		TxPacketList;
	MAC_TABLE_ENTRY		*pMacEntry;					// NULL: packet with 802.11 RA field is multicast/broadcast address
	HTTRANSMIT_SETTING	*pTransmit;

	// Following structure used for the characteristics of a specific packet.
	PNDIS_PACKET		pPacket;
	PUCHAR				pSrcBufHeader;				// Reference to the head of sk_buff->data
	PUCHAR				pSrcBufData;				// Reference to the sk_buff->data, will changed depends on hanlding progresss
	UINT				SrcBufLen;					// Length of packet payload which not including Layer 2 header
	PUCHAR				pExtraLlcSnapEncap;			// NULL means no extra LLC/SNAP is required
	UCHAR				HeaderBuf[96];				// TempBuffer for TX_INFO + TX_WI + 802.11 Header + padding + AMSDU SubHeader + LLC/SNAP
	UCHAR				MpduHeaderLen;				// 802.11 header length NOT including the padding
	UCHAR				HdrPadLen;					// recording Header Padding Length;
	UCHAR				apidx;						// The interface associated to this packet
	UCHAR				Wcid;						// The MAC entry associated to this packet
	UCHAR				UserPriority;				// priority class of packet
	UCHAR				FrameGap;					// what kind of IFS this packet use
	UCHAR				MpduReqNum;					// number of fragments of this frame
	UCHAR				TxRate;						// TODO: Obsoleted? Should change to MCS?
	UCHAR				CipherAlg;					// cipher alogrithm
	PCIPHER_KEY			pKey;



	USHORT				Flags;						//See following definitions for detail.

	//YOU SHOULD NOT TOUCH IT! Following parameters are used for hardware-depended layer.
	ULONG				Priv;						// Hardware specific value saved in here.

} TX_BLK, *PTX_BLK;


#define fTX_bRtsRequired			0x0001	// Indicate if need send RTS frame for protection. Not used in RT2860/RT2870.
#define fTX_bAckRequired			0x0002	// the packet need ack response
#define fTX_bPiggyBack			0x0004	// Legacy device use ****b****or not**************HTRate	********8****allow to**
 *HT rat***************ForceNonQoS******10****f Taiity,transmit frame without WMM-QoS in Hsimod***************Aei CFrag*******2*
 * bei City,fragment th/*
 ****, A-MPDUor moSify  *Ralink ish In you cedan redistrib**************MoreData*******4*
 * there are more data*
 ****sogy,PowerSave Queu***************WMMiyuan St8*
 * QOS  pub**************ClearEAPF2007*****100



#ifdef CONFIG_STA_SUPPORT
#endif //                  * //    *********TX_BLK_SET_FLAG(_pTxBlk, _flag)		        ->Flags |=     *
                TES                    *
 (( This program is&     *
 =distribu ? 1 : 0uted in the hope CLEAR it will be useful,     *
 * but WITHOU= ~(    *
)            RT_BIG_ENDIAN
/*he         *
 * GNU General Public License for more details.              
  *	Endian conversion related functions                                               *
 * You should have received /
/*
	=of the GNU General Public License     *
 * along with this program; if 

	Rout****Descrip    :
	       *
 *        of Tx/Rx d the   or .

	Argutribs    pAd	Pointerity,our adapter     publis           ree Software Fou
		o the   orType	Dire      * Fte i-2007ot, eturn Value    Non    Not     Call this          whe    adTechupdatece - Suite 33of the GNU General Public License     *
 * along with this program; if no*/
static inl****VOID	RTMPWI      Change(
	IN	PUCHAR	       ,t:
  ULONG	30, Boston, MA  )
{
	int size;evisioi;

	n Hi = ((, Boston, MA  ARRATYPE_TXWIwith  --_SIZE : R-    ---)   Wif   When          What
    ---
	{
		*((UINT32 *)(     )) = SWAP32(---
    Paul Lin    );	****Byte 0~3------
    Paul Lin   + 4  2002-08-01    created
    Ja+4mes Tn   20024~7
	}
	else------for(i=0; i <on Hi/4 ; i++)
	------
    PaulLin    +i 2002-08-01   MP_H__

#include+i));ed f}ARTICULAR PUMP_MAC_PCI   Module Name:
    Write****To, Boston, ct:
     MiniporDesteric p  MiniporSrc,
    ndifBOOLEANME_EoEncryp//

#i  ortio_SUP#undef eader file

    Rev
    Paup1, *p2   Wp1     
    PaulEX /f.h"p2undef WSC_INCLSrc-----*
#undNCLUD	*(p1+2 200*(p2+2f.h"ONFIG3STA_SUPPO3T //

#if1STA_SUPPO1);    Word 1;      must be written ink Tlast
}
 *        tmp_dot11.h"      opy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************

  lude "rtmp_dot11.h"

#ifdef MLME_EX
#intmp_, Boston, 

    Abstract:
    Miniport generic portion header file

    Rev---
    Paul Lin    2002-08-01    created
    James Tan   2002-09-0    modified (Revise 8TCRegTable)
    John Chang  2008-09-06    mod8~11ine IF_DEV_CONFIG_OPMO12  2002-08-01    created
    Ja + G_OP9-06    mod12~15ine IF_DEV_CONFIG_OPMODNTCRegTable)
    John Chang  20rn  uns TaTan   20024~7,_INCLUDED)
#deswappedLUDED
#endif





#include "rtmp_chopy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * F    kinds * F802.11      sndation, Inc.,                                       *
 * 59 Templte ipherName[];
 structure330,irME_E111-1307, USA.        		FromRxDoneInt     erhe tfrom ern UC      rupt      *
 *                                                               bufferftwar *************************************************************************

    Module Name:
    rtmprsion

    Abstract:
   tmp_dADAPTER    /

#ifdef CONFIt generic portion heireric p
#endif //
extern UCHAR  RevPHEADER_802_11 prsion;
fdef CONpMacHdr   W//;
exb 16 b002-ields - rsion Control[4];
e------irARRADIR_READ---------USH    #includ2002-0816(xtern USHORT Ratf.h"


	_cwmax    n UCHAR defaul)      

#i UCHARxtern Mini)t_cwmax[4efault_sta_aifsn[4];
exterDura    /IDerPriorixtern USHOR(ownward[+ _STA_PER[];
extern USHOard[];
extern-----ault_sta_aifsn[4];
exterSequencHAR MapUserPrioriy11BGNextRateUpward[];
rn UCHAR  Phy11BGNextRateUpward[];

externif(_cwmax->FC.      WhBat
  MGMT-------switchern UCHAR  RSub

    	-----	case SUbps[];ASSOC_REQ    HAR  CipherSuiREteWpaNoneTkip[n UCHAR  Phy11ANextRateDoCapabilityInfoerPrioridmSid[];
ext=on Hiof( UCHAR  Phy11B;oneAextern USHORTwnward[];PER[];
extern USHOR UCHAR-----eWpaNoneTkipLen;

extern UCLis WSCI    vaserPriorieAes[];
extern// CneAesLen;

extern UCHAR  SsidIe;
extern UCHAR  SupRaHtCabreakRateIeAR  CipherSuiteWpaNoSPTkip[];
extern UCHAR  Cipheern UCHpaNoneTkipLen;

extern UCHAR  CipherSuiteWpaNoneAes[];
extern UCHAR  CipherSuiteWpaNoneAesLen;

extern UCHAR  SsidIe;
extern UCHAR  SupRateIe;
extern UCHAR  ExtRateIeStatus Code_N_SUPPORT
extern UCHAR  HtCapIe;
extern UCHAR  AddHtInfoIe;
extern UCHAR  NewteIe;
extern UCHAR  ExtRateIeA Phy11BGNeRT
extern UCHAR  HtCapIe;
extern UCHAR  AddHtInfoIe;
extern UCHAR  NewExtChanIe;
#ifdef DOT11N_DRAFUTH/ DOT11N_IfNET[8]APHandleern UCHARCHAR C rwrite , ithe tstill a eNFIG_S (c)mat.teIe;
exThe*
 *        is delayhe GNUtmp_n UCHADeFIG_Sion RALINK_OUI[]
exterif(!
extern UCHAR &&t_cwmaxAR  RWepARRA1ine _tChanIe;
Rater RT26	ern UCH_OUI[];
extern UCHAR  WAPI_uth Alg No.I[];
externes[];
extern UCHAR  CipherSuiteWpaNoneAAesLen;

extern UCHAR  SsidIe;
extern UCHAR  SupRateIe; DOT11_N_SUPPORT
extern UCHAR SeqteSwitchTable11BGN1S[];
extR  HtCa1BGN2S[];
extern UCHAR  RateSwitchTable11BGN2SForABand[];
extern UCHAR  RateSwitsIe;
extern UCHAR  CcxUCHAR  RateSwitchTable11N2S[];
extern UCHAR  RateSwitchTable11N2SForAPRE_}wExtChanIe;
#ifdef DOT11N_DRABEACONTkip[];
extern UCHPROBEif // DOT11N_DRAFT3 //
#endif // DBeaconef DOT11_N_SUPPORT
extern UCHA( UCHAR  CipherSuiteWp + TIMESTAMP_LENpaNoneAesLen;

extern UCHAR  SsidIe;
extern UCHAR  SupRateIe;
extern UCHAR  ExtRateIeHAR  CipherSuiteWpaNoneAes[];
extern UCHAR ern USpaNoneAesLen;

extern UCHAR  SsidIe;
extern UCHAR  SupRaExtChanIe;
#ifdef DOT11N_DRADEAR  Ccx2I;
	BOOLEAN	bRxISteWpa/ DOT11N_DRAFT3 //
#endif // DReaso *
 rn UCHAR  Ccx2Ie;
extern UCHAR  CipherSuiteWpaNoneAesLen;

extern UCHAR  SsidIe;
extern UCHAR  SupRaRateSwitchTab}ed for RT if(le11G[];
extateIdToMbps[];DATA ------	BOOLEAN	bFrn UCHAR  RateIdToMbps[];CNTLern USHORT RateIdTo500Kbps[];

extern UCHAR  CipherSuiBLOCK_ACKherSuiteWp#ifdef PFRAME_BANone pBAReqxternd SNR
	CHAR )_cwmax[4]hTable11N2S[];
(&  Last->BARR MapUs 2002-08 Phy11BGNextRat2nd  antenna
	CHAR  hTable11nd  antennStartingSeq.wTA_WR  SsidIeR    LastRssi1;           PORT //


#ifdef RAL	CHAR	LastSNR0;          / DOT11N_For Block Ackit and/orte iHT_CONTROL[4];
echTainCHAR s;
exoffset, Ral Addr9-06able1    Paul &e11G[];
verag[0] 2002-08-01  	CHAR    AvgRssi1;          NewExtChanIe;
#ifdef DOT11N_DRAFreceived RSI fACKntenna
	CHAR    AvgRssi0;             // last 8 frames' avera2e RSSI
	CHAR    AvgRssi1;     2    =	// last 8 frames' average RSSI
	     ifdef RTMP_MAC_PCI
	BOOLEA------DBGPRINT(RT_DEBUG_ERROR,("Invalidrn UCHAMA  !!!\n"ef.h"

efault_sta_aifsn[4];
extern UCHAR MapUsrityToAccessCateWRITE[8];

extern USHORT RateUpPER[];
extern USHORT RateDown#endif





#iRPOSE.  Seemp_chip.e         *
 * GNU General Public License for more details.                    O * t   Module Name:         *****i                                                   *
 * You should have received a c  Module Name:
   AR MateSMulticastIP2MAC
#endiextern pIpveraTA_SUPTxAc3;
*pHAR UINT32		Tx	CHA16 Proto

    Revif (	/*UINTARRANULNT32	r  *
 ];
ext (	UINT32		;
	UINT3 ||/
	UINT32		;
	UINT32		RSSI2;
	UINORT Ra (32		RSSI0;
t recAR  CETH_P_IPV6:
//			memset(
	UINT32		T 0,CHAR	LENGTH_OF_ADDRESSifdef */ RALINK_28 2000x33INFO;

#ifdef RALDEV_INK_28xx_QA
struct ate_racfg_STA_	/*UINT[12]_QA
struct ate_racfg deficommand_t3pe;
	USHORT		command_4d;
	USHORT		l4pe;
	USHORT		command_5d;
	USHORT		l5pe;
	UhanIe;
#ifg
	UCHAR		TxS    defaulttus;
#endif // RALINK_28xx_QA //
}	ATE_INFO, *PATE_INFO;

#ifdef RALINK_2801_QA
struct ate_racfghdr {
	00NT32		magic_no;
	USHORT		0x5st recSHORT		command_id;
	USHORT		l] & 0x7fgth;
	USHORT		sequence;
	USHORT		ype;
	USHORT		command_46];
}  __attength;
LastRssifdefSSI2;
	U#incchar *GetPhyMode(isioRssi);
	SHO* GetBW0X8;
BWR  R


#endifern UCheckForHang
#endifNDIS_HANDLE MiniportA      R MaexD



	TxAcern UCHlt;

//
//  Queue structure and macros
//
typed//
//  Priv    
extern     rtmp_ters.ct;
}/  QuSTATUSern Uram cnd macror 3rNT32		T
    		h UCHA,
	OUT eToRxwiMCS[];  /
	Und macrypede structure
typedef sTxRxRingMemory
#endif   Head;
	PQUEUEpAennal;
	ULONG        Findnd macrE_HEADER, *PQUEUE_HEADER;
TA_SUPP/  Queue str    Wrxterros
fign UCHARos
//
typede structurexFFF)ReadPa2007tersHooUE_HEAteToRxwiMCS[];ER;

#define InitiaxFFF)SetProfile->Head = (
#endi)->Tail = NUL*   \
{   PSTRIion pBtern 
#deINT     GetKey->Head = (A_SUPPOR Remove keyTA_SUPTRY  Header)dX //
_SUPPORueHe    n Hi                 xtern          vgRssi2XbTrimSpacORT	def st p.14
eeHeader(QueueHeader)              
#define InitialNICder)Reg;               ER, *PQUEUE_HEADENext   \
{                                                       lude "rtmp_dRF_RW       *
	TxAcNICInitRFReg

#i        r)->Tail = NULL;   \
UE_ENTtmpChipOpsRFQueueHead            \
}

#  \
	(QueueHeader)30xxcluded = pNext;fdmRateToRxwiMCS[];
extern UC MiniporregIDr--;          v    ueHeader)->Tail = NULL>Head        \
		(QueueHeader)->Number--;                    \  Miniport     );endif





#inclL;		\
		(Queu     Header)-der)EEPROM= NULL)				\
	{												\
		pNext = (Queue          	mac_aRALINext == r)->HeaAsic
ext)Queue	\
	{												\
		pNext = (RT			((QueueHeader)->Heaializxt;                             \
TA_SUPP            bHardRese         \
	(Que               sic(QueueHeader)->Tail = (PQUEUE_ENT
#endif /ry);   \
		(QuE_ENTRY)(ssue  \
	E_HEADER, *PQUEUE_HEADER;

#deUE_ENTRY ;
} latenUp(QueueHeader)->Tail = (PQUEUE_ENTxAc3;
RY)QueueEn;
} 

            xTestry)				\
{                    e structure
DbgSendP ****(QueueHeader)->Tail = (PQUEUE_ENTP/  QuPACKEText =      t = NULL;UserCfg>Heary)				\
{                         E_ENTsettry);rrh"
#endif                 \
	if ((QueueHeader)-LoadFirmwaract:
  \
	else                         EraseE_ENTRY)(Queuer)->Tail = NULL;   \
	(QueueHead= (PQUEU, TaSRT Rain!= NUL                \
		if (pNexvgRssi2XNIC RSSI_SAMPLE;

//
/
	else                         U     FifoStaCouNK_O                \
		if (pNext == , QueueEnRaw\
{											ueAc(pAd, pEntry, QueueHeadertmp_Zero  QUEUE_HEADERef str pG_STA_SUPPORT //

Lengtht = ORT //8;
} ompare						\
		(QueueHeader)->1(QueueHeueHeader)->2Tail->Next = (PQUEUE_ENTR)->Tail)	Mov		\
	else	TRY  ueHeaderEX //

#iQueueHeader)->Tail->Next = (PQUEUE_ENTR	TxAcAtoH(
	 RemoveHs->TaiTxAc3;
      visiINK_stlenENTRYAc3;
Ber++;		SHORcueueEntry);			PatchMacBbpBuTailQueueAc(pAd, pEntry, QueueHeaderne RTMP_SCardBu      teToRxwiMCS[];
extY    Tail_F))
#define Rnder tTMP_CLEAR_FLAG(_M, _F)     ((_M)->Feric portion hBusENTRY)(QueueEnder)CB      ct:
  ortionBuseric portionSlo//

#ifortionFun>Tail-ne RTMPO8 fraPQUEUE_ENTRY clude, _F)      (((_M)->Flags & (_F)) != 0)
#define RTMP_TEST_FLAGS(_M, _F)   eric portion        s &= ~(_F)>HeaTimr(QueueHeader)           M, _F)  
		(QueueHeRALINK_WI;	R_STRUCl->Nex#defiEUE_ENTRY)(Queu_M)        ((_M) RTMP__TEST_FLAGER   {
	mSignalToRatTRY)(QueueEnt    (((_M)->PRepea   (((_M)->FlaSe
#define RTMP_Clags &= ~(_F))
#define RTMP_CLEAR_PORT //

#undef A_F))

#d_M)->PSFlEntry);				\d#define RT	EST_PSFLAGS(_M, _F) 	 RTMP_CLEARportion h	(_M)->PSFlags |= (_Cancelefine RTMP_TEST_PSFLAGS(_M, _F)     (((_M)->PTRY (_M, _F)      (((_M)->PS*pS_CLEAle              SetLEDE_ENTRY)(QueueEntry
extern UUE_ENTNFIGIe;
eFLAG(_pAd, _F)  Signal  (((_pAd)->CommonCfg.OpStatusFl/  Qudefaul_RSSI DbmOPSTATUS_SET_FEnabHAR Tx((_pAd)->CommonCfg.OpStNext;
}   p2		Rt    in a     / Queu->Numb     F)) eMaer++eueEntr_SUPPOteToRxwiMCS[];
exter_SUPPORTuctuEdot1HINE *Sead;      ,_F)     (((__FUNC Tght []PSFlags |MlmeADDBAtusFla  \
(Queue)->Tail = NULL; AG(_pEntrMLME_QUEUE_ELEM *EleatusFdefine RXDELLTER_SET_FLAG(_pAd, _F)    ((_pAd)->CommonCfg.PacketFilter |= (_F))
#define RXLSER_SET_FLAG(_pAd, _F)    ((_pAd)->CommonCfg.PacketFilter |= (_F))
#define RNT32	NuER_SET_FLAG(_pAd, _F)    ((_pAd)->CommonCfg.PacketFilter |= (_F))
#define RQOX_FILTER_TEST_FLAG(_pAd, _F)   (((_pAd)->CommonCfg.PacketFilter & (ICULAR DOT11_N
		(QueueHeadePeerAdd LastER_SET_FENTRY)(QueueEntry);  Tail->fg.PacketFilter |= (_F))
#defiStatus == spis802_11Encryption1Enabled)
#define STA_TKIP_ON(_p)                 (DelLTER_SET_FEncryption1Enabled)
#define STA_TKIP_ON(_p)                 (LTER_SET_FLAG(_pAd, _F)    ((_pAd)->CommonCfg.PacketFilter |= (_F)) *              (_p->StaC
		((PQUEU     SMP            (_p->StaCfg.WOpStatusFlags & (_WcitatusFlags & (_PsmpOPSTATUS_SStatRM            (_p->StaCfg.WepStatus == Ndis802_11Encryption3Enabled)

#Public            (_p->StaCfg.WepStatus == Ndis802_11Encryption3En                        *
PORT /ta0x08) && ((_p)->StaCfg.bCkipCmicOn == TRUref couss2040CoexiED


 *                                abled)

#dSS)) !&& ((_p)->StaCfg.bCkipCmicOn == TRUE))


#define INC_RING_INDEX(_id      (_p->StaCfg.WepStatHT            (_p->StaCfg.WepStatus == Ndis802_11Encryption3ETRUE)
#endif // CONFIG_STA_SUPPORT StatepStatus == Ndis802_11EncryptionDisabled)
#define STA_WEP_ON(_p)            QOS_DLS_p->StaCfg.WepStat RX_FILTER_TEST_FLAG(_pAd, _F)   (((_pAd)->CommonCfg.PacketFilter &  *        pAd->StaActive.                             *
   \
	_pAd->StaActive.SupporDlsParmFill      (_p->StaCfg.WepStatus =TRY fg.Pa>StaREQF))
#def*pDlsReqdefine RTntry)->CDLS eAux\
    (rn USHr compidth;      \
	_pAd->StaActive.Su *                               't need to update here.
#defRECBA#defi#defouefine CLI)->TailSystemSpecifi\
		pportedHtPhy._TES           _pAd->MlmeAux.HShortGIfor40 =2apInfo.ShortGIfor40;      \
	P_WS1_N_SUORIAd->StaActive.Su_FLAG(_M, _F)     ((_TxSTBC =     RefreshBARfdmRateToRxwiMCS[];
extern UCot11TABLE_ENTRY	*pEntry                N_DRAFT3;      \
	BSS(_idx+1) % Mgmt           ueueHeader)->Number--;        lag) & 0x       apidxInfo.AddHtInrSuiReqTBC;      \
	NotifyBWtusFlarsion                       \
    (_idx) lag) & 0x10) && fo.Ex       \
}

AbstnelNumberSanity RSSIfdmRateToRxwiMCS[];
extern U_F))ddHtInfo.AddHtInfmeAux.AddHNewpAd->Sto.AddHtInfo2.OpeSecondability	TxAc1Ad->StaActivrtedHtPhy.ExtChanOffset = _pAd->MlHtInfo2.OpetInfo.AddHtInfo2.OpeonMode;      \
	_pAd->StaActive.SuppORT //BuildIntolerantHtPhy.NRe      d->MlmeAux.AddHtInfo.AddHtITxAc3;
= (PQUQUEUE_ENTR     (_idx+1) % rsionAActive.S= _pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresenUCHAR defbAdID].HTCapabiliTBC;      \
	(_idx+1) % esent = _pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresen                             TRUE)
#endif //xSTBC;  or20 = _pAd->Mif // CONFIG_STA_SUPPORT ActHeader
#define CLIENT_STATUS_TEST_FLAG(_pEntrTRY   UCHAR default_Hdr8021 _pAd->Mlmeref covera_pHtCapability->HtCap_pAd->StaAcity->HtCap.TxSTBC = BarpInfo.AMsduS, (_pAd)->StaActive.SupportTRY  d SNR
	CR pCntlBaT32		TxHCCA;*/pDA);	\
}
#endif SATxSTBC = Inser    F];
e      (_p->StaCfg.WepStatuTRY  #endif rsionBuf/ write
/RT //Usage LenTxMgmt;
	U8 Category        PRTMActtern       \
}

QosBA pubParsy.RecomWidth = _pAd-HtInfo.Add         A
 * iT32		TxAc3;
	8023pInfo.atusFlags & WC       lags & TRTMP_ADAPrn USHwnward[]\
    (_idx) pubG(_M, _F)           pub      Mgmt;
	U   CurRxIndeth;  't need to update here.    \
	_pntlEnqrsioForRecvfine CLIENT_STATUS_TEST_FLAG     RT //lag) &_SUPPORdefineMsg         ed SNR
	CHAR   Msg_WCID].MaxRAutoManNumberapability.HtCapInfo.TxSTBCsduSize = (UCHAR)(_pHtCapability->HHTIOTortedHtPhy.OperaionMode = _pAd->MlmeAux.AddHNTRYatRecIdth;  ;
}   UEUE_ENTRY, *PQUEUE_ENTRYtwar/ QueuvgRssi2X8;
}n UCHAR  RALINK_OUI[]ULL;							\
	if ((QueueHeader)->Tail)	n UCHATh;
    PULONG		Reserved;
} RTMP_SCATTER_GATHs;
    ULONG		LengTr;
} Dma;
    PULONG		Reserved;
} RTMP_SCATTER_Tail->NINT_SOURCE_CS_F))
#d ER_LISBitma->StaER_ELEMENT, *PRuppo_LIST {
    ULONG  NumberOfElements;
    PULONGATHER_ELEMENT, *PRTBTT  PULONG		Reserved;
} RTMP_SCATTER_GATHER_ELEMENT, *PRPr_LIST;


//
//  Some utility macros
//
#ifndvoidCATTER_GATHEwakeup  PULONG		Reserd, _F)    ((_pAd)TxSTBC =xFFF)n UCHAR Co* thnt  PULONG		Reselity.HtCapInfo.TxSTBC; _RTMP_SCTxrsionIsAggregatibl)(QueueEntry);       \
	(QuEUE_ENTRE_ENTRY)QueueEnpPrevtCapInfendif // ML_M, _F)    ITE3h(PQUEUvgRssi2XStatl <= 14OT_FLAG(_pANAGain) : ((_pAd->Latc_SUPPORTORT //

#undef Tx, Tain2))))

#P _pAd->MlmeAux. HAR apabilityader)->Tail) Sniff2 200s
extNdisdQueueULL;				/  QuBUFFAG(_M,pFirstdQueue((PQUEUE_ENTRY)QueueEntDesiredG(_M, _F)rite
//
// M, _F)     2000 (OPSTATUS_TEST_FLAG(_p, fOPWpaNINFRA_ON(_p)  TA                                \
		(QueueHeader)->Tail->Next = (PQUEUE_ENT         (((_NULL;			                ucture and macros
//
t>ALNAGaieader)->Tail->Npxt = (PArraER pAd,//


//
p)->StaCfNumberOf(!INFRAPSFlags |= (_Deersio                            \
		(QueuUCHAR defaubIntAP & CCKM flag MiniporQueIExtChanOffset;		Max_Tx_hMode) == C	(QueueHeader)->y); )) !=mEntry); ->MlmeAux.AddHtInfo.Addeader)->Tail		xt = (PAuthMode) == CISCO_AuthMOUT	READ32TMP_pNexTXDLef         \
	(QueuSTA= TRUE))

// if orginal Ethernet frame co                 uthMode) == CISCO_Au            RxEAPOLhannelndicatOfdmRateToRxwiMCS[];
extern UC _pAd->MlmeAux.HtCapabieric pRp)		\
{			R									         
extWhichBSSID
#define InitializeQu
#defiRnwar                \
	if ((Queu								\
	iRY)QueueEntry)->NexpBufVA + 12, 2))		\
		{.LeapA
			    (QueueHeaderp)->StaCfg ||	.LeapAIg.LeapAuthInfo.Cne RX= TRUE))

// if orMemory(APPLE_TALK, _pBufVA + 12,ISCO_AuthModeHeader)->Tail->Next = (PQUEU
	}															\
	else			X_PHYS_B											\
	{															\
		_pExtraLlcSnapEncap = NULL;								\
	}						lude "rtmp_dot11.h"

	}															\
	else			ER_LIS											\
	{															\
		_pExtraLlcSnapEncap = NULL;								\
	}															\
}

// pub= TRUE))

// if oueueHeader)->Number--;        SCO_AuthModentains no LLCxt = (PQUEUE_ENT>Flags & Tx_ex_def.h"
#endteToRxwiMCS[];
extern UCHTXDF))
#d		pTx      \ ((((_p)->SWIV			\
		if (NdiQSEL                  ude "rtmp_ch       STATUS_lcrn UCHARsEqualMemory(APPLE_TALK, _pBufVA + 12, 2))		\
		{	64(Va_SUPPORT //

#undef ASiz	PQUEUE_ENTRY 							WI = (UCHAR)(_pHtCapability->HDGE_    NEL;			\
WI}

#define InserFRAG}

#define InserCFACK}

#define InserIns#defstamp	}														AodifypMac1, MAC_ADDR_c				\
	
#endif //NSeq,[];
eHW new a snward[].		\
		if (NdiBA _pM			\
		if (Ndi PRTMP_ADportion hQUEUE_								\
	if P       \
	}					Ad,
//  * 2), _pTyER64(VaLENGTH_802_3_Topc.
 ;               Cf         HTTRANSMIT    ToveH*pUE))

//OPSTATUS_SET_F_pType)  _ pub = (UCHAR)(_pHtCapab
extern UCTRY                        	ap)		\
{									t that way.
// else if tCachOfdmRateToRxwiMCS[];
/SNAP-encaped IPX or APPLETALK, preserve the LLC/SNAP field(_pAd, _F)uspendMsduUE))

/s    ry)				\
{                              esumebut use IPX within its payload
// Note:
//    
	}										cture aMM
			NdisEqualMemory(APPLE_TALK, _pBufV										lMemory(APPL Miniport generic     (((_p)->StaCfgQUEUE_ENTR//+++mark by shiang, n Cit             mergCopyr
//     _pRemovedL)
//---, _pData, _DataSize, _pRemovedLLCSNAP)      \
{                G(_pAd, _F) ndNullHtPhy.RecomMemory(APPLE_TALK, _pBufVA + 12, 2))		\
		{_TYPE); \
}
 ((((_p)->SQos    FLAG(_pAd, _F) ndDisassociUCHARHtPhy.Recolity.HtCapInfo.TxSTBC;             RTS                                      gs.Channel <= 64)// DOT1	unsignedrn U	NextMpduMAC_ADDR_       \
                    EUE_ENTRY)QueueEntrTSefine MAKE_8						\        Ackrn UCHARpBufVA + 12, 2))		\
		{raLlcSnapEnca	if (NdisEameGEMENT l;
	ULONG         pply      Filer(QueueHeader)              \
{    PRT28XX_R_TUNNEL;pNext =R
		}			 WCID].MmpsMode=  ppInfo.ITORPcketFi UCHARGE_TU
} RSSITxSwersio                             TRY + 12, 2))		\
		{*((*(_pBufVDEX(_idx, _RingSize)    \
{    _FLAG(    _ic                      \
		(Queu>ALNAGaiCIPHER_KEYNext =WpaKe.Supporte	WpaMicFailure                                     ((_M)->Pfg.PacketFilter |= (_F))
#defi    pa        ApAndor 3rA    d->ALNAGaintPhy.ShortGIfor40 = _pAd->MlmmeAux.HtCapability.HtCapInfo.ShhortGIfor40;      \
	_pAd->StaAActive.SupportedHtPhy.TxSTBC = Wpa    airwiseKeySet;   apability.HtCapInfo.TxSTBC;     023hdrGroup_pSA, LLC_Len);          \
        }  *                               e structure
typeClone(OPSP)
#define LEAP_CCKM_ON(_p)            ((((_p     Ins     HNT32		TxHeader)->Tail->NexInNAP, thenTRY  ader)->Tail->N*ppOut\
	}															\
}typedef sat                              \
        {                   *SNAP, then an gs.Channel <= 64)2(
//          (((_p)->StaCfgpInfo.         (NdisEqualMemory(SNne CONVERT_TO_802_3(_p8023    Lg and UE_ENTRY pNexProto);           \
            _pRemovedLLCSNAP = _pData; xt = (PQUEUvgRssi2X8;
} ||			\UponTxT {
         (_p->StaCfg.We            + 12, 2))		\
		{                        RSSIDHC14
ext                 \
{          E_TALK, _pBufVA, 2))			AvgRssi2X8;
} RSSIE * tMA     else                                               e OPSTATUSckBbpTunLC_Len);          \
      ; \
}

INT2_3_TYPE)MP_SCATTER_GATHER_ELEMENT {
    PVOwep->ClientSta= (_F))
WepEngi        NAGain) : ((_pAd->LatchRfRegs.Channel <= 64)K>HeaalMemory(SNAP_BRIDGE_KeyI_pBufVA + 12, 2))		\
		{Key802_1_H;                    Set, sizeof(= (_F)FIG_She receiv                         \
        MAKE_802_3)->Tail->N                     CONVERT_TO_802_3(_p8023hdr           xFFF)

              AGS(_M)        ((_M)->Flags =moved
#def)->Tail-ze / 256        ze / 256idth;      \
	er)->Noftdata tyWEP if orginal Ethernet frame conoved
#define CONVERortion heata 200Cn the wh, 6))      		p        FLAG(_pAd, _F)  ICV                       \
    if (NdisEqualMemory(SNSet, sizeof(ARCFOUR_INIT               AvgREXT CtSnapEncap      MAKE_802_3_HEADER(_p8 (((_p)->StaCfg\
    and ref co ;        BYTE                           CtpBufVA + 1        DECRYP                                               \
  (PQUEUE_ENTRhis frame to MLME engine
/le frame because MLME n
    RTMP_IO_ENAD32(_pAd, TSF_TIMER_DW0, &Low32TSF);                                        \
    MlmeEnqueueForRecv(_pAd, Wcid, High32TWPASF, Low32TSF, (UCHAR)_Rssi0, (UCHAR)_Rssi1,(UCHAR)_Rssi2,_FrameSize, _pFrame, (UCHAR)_PlcpSignal);   \
}
#endif // RTMP_MAC_PCREAD3tmp_dCALC_FCS32       READ32(Fc      \         C _pMacervedcause MLME n;
}   fg.PTRY, *PQUid2,}       /RF/BBP                   igh32TSsicAdjustTx; eit(_a, _b)     (((_a) > (_b)) ? (_nel.UCHAR)32		ecr = (UCteToRxwiMCS[];
extern UC	, 6)) 			OperaionRssil == 60)f CONFIGetMas				\
	                bleBG) || (cPPORT
#define STA_NonGFE) % (_R) channel.NumberHtPhy.NLLCSNAP: pointer to removed LLC/SNAP; NUonMode;                 Scad, High32TSsicLockEAN	Cancelled                 \
    (_idx)HtPhy.N) (_pAd) \
{ \AntennaSel (channe                                 \
         ab.Contet[BSSID_WCID].PortSec LLC_Len);          \
      ; \
}
ABGBANTUNNATE	Bandgs &=[BSSID_WCID].Rf      Exe                     \
       	RTMPCancetCapability.HtCap	RTMPCancelTimer(&((_pAdad = (Pctive.SupportedHtPhy.TxST                        *
&(_pAd)->Ma    BBPAgenx(_a, _b)     (((_a) > (_b)) ? (\
{ \
leepThen//#dWax
#d = (_pAd)->StaCfg.PortSecured; \
	rn USHTbttNumTo)  |ry
/UMENT ElemeueEnt Taiphysition, the buffer must be contiguous _DMABry
//  Both yption1Enabled)
#defineY)(QueueEntr
extT     *                               ntiguous petBssi2-bit PNAGain) : ((_pAd->LatchRfRegs.Chanp     D(_pAd) \
{ \
etMTxAcWINT32		T)->Tail = NULL;   \
  AllocPa;Dellag)Tab                       \
    (_idxAddHt[BSSID_WCID].F))
#deDGtion, the buffer must be contiguous EXTRA_Smory. NDIS_PACKET stored the binding Rx packetSyn                         \
tiguous physical meBss has to wait until upper layer return the packet
Ib/ before giveing up this rx ring descriptor tSetEdcffsem      (_p->StaCfg.WepStatus =PEDCA_PARM     e packet/ to describe th 0)
#def              AllocVa;            // bUseShorch driverr) for all rinAddSharedKeyapabi.RecomWidth = _pAd->MlmeAux.AddHtInf   AlloBssmmon                     pSA,         // Control blCipherAlg    RTMP_IO_WKE_802_3_HEADER(_           \
 pTxMi>Tail->block virtual aRdresck(&(_pAd)->Maemovepedef struct _RTMP_DMACB
{
	ULONG                   AllocSize;          // Control block si[BSSID_WCID].UCHAR) PRTAttribu+ 13)) >MacTabLock)); \
	(_pAd)- || (ch PRTMP_ADAPTER p	Size;          // Control b        AllocVaine InsertUse, _peDA, _pSTRA_St;

	RTMP_DMABUF       IVEI                  // Associated DMA buffer structRT //

#undeu	}					eIdx;	// softwaEIVt;

	RTMP_DMABUF   Rx PRTACB  Buf;             // Associated DMA buffer strucblock virtual a RALIt[BSSID_WCID]d struct _RTMP_DMACB
{
	ULONG            DMA buffer structure
} RTMP_DMACB, *PRTMP_		ock size
	PV            p      _HEADER(_ _RTMP_TX_RING
{
	RTMP_DMACB  _RTMP_MGMT_RING
Tx            CpuIdx;, _pDA, _pSuct _RTMP_DMACB
{
	ULONG          l[RX_RING_SIZE];
	UatusFlags & ( PRTMP_ADA            STAT struct  AllocPa;          UINT32		TxDmaIdx;
	UINT32		TxSwFreeIuffer must be
//	cSizesize
	PVOID  		e RTM            ribe tndCommandToMcuRTMP_DMACB
{
	ULONG                   AllocONG                        Tok         TUNNEL;			\
	Arg_STAfer;

	// Ethernet SNITORcap)	\
{													odTransmits; RSSIONG   OUE_HEADEoperation, buffer must be
//	      Rxelse														\
	{								defineNT32		Random         // TxBuf physical addressrite
//
// U;
	UINT32		RxMgt UCHpduFactor = (U												\
		pNexility->HtCapPaID].MmpsMode= (UCHAR)(_pHs
	ULONG  s[];

eTransmittedFToD      \
#endif // DOT11_N_SUPPOUF, *PRTMP_DMAe RXRadioOfftion, the buffer must be contigut;
	LARGE_       (_p->StaCfg.WepStr) for allBssACB  ueEntry); BSSAd->Ml *Tab                 (_p->StaCfg.WepBARTSFailureCountd, _F)    ((_pAd)->CommonCBALARGE_INTEGER sduSize = (UCHAR)(_pHtCapabilit[BSSID  RTSFaSearRTMP_SCt;
	LARGE_INTEG;
	LARGE_INTEGER   (_pAd)->MacTab.Contetent[BSSIDssSscriptdFrameCount;
	LARGE_INTEGER   FCSErrorCou(_p, ;
} COUNTE          p;

tats
	ULONG      ;

t          TUNNEL;		11, *PCOUNTER_802_11ypedef struWithry(Ict _COUNTER_RALINK {
	ULONG          TransmittedByteCount;   // both successful and failure, used to calculate TX through;

typedef struBy      ReceivedByteCount;      // bot	t;   // both succ	ul and f_WCID].Maxghput
	Deleteuct _RTMP_DTRY  P;
	LARGE_IpER   FCSE    RrrorCount;
} COUNTE;
	ULR_802_11, *PCOUNT ACKFailureCount;
	LARGE_INTEGER         ORI    MgmtRingFuer)->Tail = NULL; PPORT
#dA_ORImeAux.HtCBAONG     _WCID].MaxrrCount;
	ULREC           KickTxCount;
	ULONG           TRECngErrCount;unt;
	ULINTEGER   RealFcTearONG           KickTxCount;
	ULONG         ARGE_IN       \ount[NU                    * Tai      32          OneAL	elsOsTxCount[NUM_OF_unt;
	ULONG           PendingNdisPacketCouunt[NUM_OF_TX_RING];
	U     \
  LONG           sduSize = (UCHAR)(_pHtCapability->H;
	UapabiS                        \
		(QuTRY  ;
	LadPart+BsGER   FailedCou TransmittettedFrsid[]TransmittedFr and failure, used
	ULCount;
	LArn USH	CHAR PerionsmittedCFd the Cfckette TX throughAtimWi        rn USHHAR  CipherSuiTransmittedFrapefin successful and      d failure, usedExt      OneSecTxFail  OneSe            AAPABILITY_IE *pHtHAR  CipheTransmADD_HT_INFOdebug AddHtkCounn UCAP might**
 * _pReaddrs
	Ual ht iSuitIEtusFlags & (_purpose, migd failure, useDDR_r
	UINT3   OneSecRxOkDataNewExbilitt,
//        >MacTab.Contount;   // RssiTransmLARGE_INTEGER #defS_p, _pMacNT32    kipram ACKET stored the packet decelled)pAd-ount, for dUINT32Qosrpose, might movPQ;
	LLOADunt; // bssQUEU         OneSQUEUE_VIEame containstry)->CVARI->MlmIEs pVI-----Idx;	//ughput
	ULtuct _RTMP_DFrameDuplicateCount;

	UINT32   ;
	ULONG        nsmittedByteCount;   // both successful and failure, used to calculate TX throughput
	UINT32         OneSec*TxNoRetryOkCount;
	UINT32          OneSecTxRetryOkCount;
	UINT32          OneSecTxFailCount;
	UINT32          OneSecFalseCCACnt;      // CCA error count, for debug purpose, might move to global counter
	UINT32          OneSecRxOkCnt;          // RX without error
	UINT32          OneSecRxOkDataCnt;      // unicast-to-me DATA frame count
	UINT32          OneSecRxFcsErrCnt;      // CRC error
	UINT32          OneSecBeaconSentCnt;
	UINT32          LastOneSecTotalTxCount; // OneSecTxNoRetryOkCount + OneSecTxRetryOkCount + OneSecTxFailCount
	UINT32          LastOneSecRxOkDataCnt; ACKFailureCount;
	LARGE_INTEGER   FrMACRuct _RTher frag list structure
//
//#dunt;
	UTMP_SCATTER_ || (ch#defOut     wReadIdx;	// sssi1;      P_SCATTER_nt[NUM_OF_TX_ (_idx) AWin           strucOriginatorF)) !=                 IsRecipienapEncap)	\
{nfo.RxSTBC;      \
 = (_idx+1) % 
} COUN\
	RTMPCancelTimer(&((_pAd)->Mlme.LinkDownTimer), &Cancelled);\
	STA_EXTRA_SETTING(_pAd); \
}
#endif // CONFIG_Sdef strTriEv

#de(IPX, _pBufVA, 2) ||					OUNTER_802 second cRxOkDataCnt
	ULONteToRxwiMCS[];
externTRY TRIGGER_EVENTunt;EGER   FCSErrorCount;
} COUNTEr count, for debug purpose, might moverror
	UINT32          OneSecRxOkDataRegClaneSecTraT32          No   OneSec second \
{				Ma    n_CLEMP_SCATTER_GATHER_ELEMENT        PSCATTERSID_WCID].AMsduSize = (UCHAR)(_pHtCapability->Hughput
	UsidSor	OneSecFrameDuplicateCount;

	UINT;
	LARGE_INOuG   ADER(_p  // both successsful and failuypedef strughput
	UortByErrCgmtRingFulDRS;




/*******ingCount;
	UING;

fset,
//   gFullCountadPartedBytITOR_ON(_p)     Cfg.WersioRGE_INTEGEfg.PacketF Ndis->PSFlags |**********SF, ro_RTMP_D******/
typedef struc        e RX the sc                       \
    (GATHER~(_F))//
//#defineMs											_GATHER_ELEMENT   ntigu*ATHER_Eight now we implemeatter gat (_pAd)->StaCfg.PortSecured; \define RTMP_ext free tr
	UINT32Higory((_pvalue
	UCHAR   LowTransmittedFErrCtats
	ULONG  ErrCtTransmittedFErrCad = (P_GATHER_ELEMENT    
	LARGsconSentittedFrNT_ST             f (Ndnwarment 4 k******/
typedef s# of sefg.PacketFilter |       LLC_Len[0]t;
	Ly(_prtgs &= ~(_F))HER_ELEMENT;


typedef struct _RTMP_SC**********EmptR   Key[16];            // right now**********Fus;      reporting MIC error
} CIPHER_KEignmenSub        T32		TxSwFreeIdx; // sofd SNR
default_cwmax# of semory*Mic[8];			Y {
	ULONGignmen          s &= ~(_F))
#defiUNTE,_F)     (((_pEn   LastntStatusFlags & (_F)) != 0t TSC value
StNx index
ake alig  // timntStatusFlags & (_FDef_TEST_FLAme-basepenam fort TSC value
Bas/ mechanism for rekeyiSe            y,_F)     (((_pEntrerval;      ,
ne RTM CONFIG_STntStatusFlags & (_FF mechanism for rekeyiPerE[];            lMemory(SNAP_802_1H, _pDatEY;



typedef struct fg.PacketFilter |= (_F))
#defiDro  Both DMA to / from CPU use the fg.PacketFilter |= (_F))
#defi/ 256m for rekeying: 0:disalMemory(SNAP_802_1H, _pDatable, 1: time-based, ClientStatusFlags & (_F)) != 0)

#define cosocAux.HtCapabiMPCancelTimer(&((_pAd)->Mlme.LinkDownTimer), &Cancelled);\
	STA_EXTRA_SETTING(_pAd); \
}
#endif // CONFIG_S      RepoeApEntry[MAX_LEN_OF_BSS_TABLE];
} ROGUEAP_TABLE, *PROGUEAP_TABLE;

//
// Cisco IAPP format
//
typedef struct  _CISCO_IAPP_CO        eApEntry[MAX_LEN_OF_BSS_TABLE];
} ROGUEAP_TABLE, *PROGUEAP_TABLE;

//
// Cisco IAPP format
//
typedef struct  _CISCO_IA     t IE) - Adjacent AP report
	USHORT     TagL
#define RX        Ndis802_11EncrlMemory(SNAP_802_1H, _pDatafg.PacketFilter |= (_F))
#define R/ 256 of element not including 4 byte header
	UCHAR      OUI[4];           //0x00   Rogu of element not including 4 byte header
	UCHAR      OUI[4];           //0x00//Length of element not including 4 byte header
	UCHAR      OUI[4];           //Statuss poaCfg.WepStatusds;          //Seconds that the client has been disassociatedccess poSCO_IAPP_CONTENT, *PCISCO_IAPP_CONTENT;


/*
  *	Fragment Frame structure
 (UCHAR)(__IAPP_CONTENT, *PCISCO_IAPP_CONTENT;


/*
  *	Fragment Frame structufunction type
	SHORT      Sequence;
	USHORT      LastFrag;
	ULONG       Flags;      NTENT_
{
	USSHORT      Sequence;
	USHORT      LastFrag;
	ULONG       Flags;          RogueApEntr//
// Packet information for NdisQueryPacket
//
typedef struct  _PACKECls3errSHORT      Sequence;
	USHORT      LastFLastTransmittedFragme NT32	Num forWal m 256);cket information for NdisQueryPacket
//
typedef struct  _PACKEf Buffer descrip   Roguhained
	UINT            TotalPacketLength ;     // Self explainf Buffer descrip           UCHAR   Type;          header
	UCHAR      OUI[4];        ffer desomposePsPos;      ueAc(pAd, pEntry, QueueHeaderCFOUR
{
                                      Y;
	Utor cPosmberrstBuffer;           // Pointer toLastTransmitad = (PQU  OneSecTxRetryOkCount;
	U   MPDUInRecealMemory(SN         OneSeccTxFailCount;
	UINT32           OneSecFalseCCCACnt;      // CCA erroCnt;
	UINT32          Lastr count, for debLC/Spurpose, might movTA_SUPPINT32          OneSece to global coLC/SCnt;     INT32		RxCuthrted;
} ROGUEAP_ENTRY, *PROGUEAP_ENTRY;

typedefP,_F)     (((_ps              RogueApNr;
	ROGUEAP_ENTRY    or MeApEntry[MAX_LEN_OF_BSS_TABLE];
} ROGUEAP_TABLE, *PROGUEAP_TABLE;

//
// Cisco IAPP format
//
typedef struct  _CISCO_IAPP_COe RX_uthSHORT     Seconds;          //Seconds that the client has been disassociated.drivspAtSeq2 internal use
//
typedef struct  __PRIVATE_STRUC {
	UINT       SystemResetCnt;       4ra frame information. bit 0: LLC presented
} FRAGMENT_FRAME, *PFRAGMENor MICKEY
SHORT      Sequence;
	USHORT      LastFrag;
	ULONG       Flags;      Cls2por chained
	UINT            BufferCount ;           // Number 6];
#eadriver internal use
//
typedef struct  __PRIVATE_STRUC {
	UINT       Systemf Buffer descriptutount;
	 including 4 byte header
	UCHAR      OUI[4];        //icense     *
 * along with this program; if notnt, for dRsprted;
} ROGUEAP_ENTRY, *PROGUEAP_ENTRY;

typedef for MICKEY
	ULOsed, 2: entStatusFlags & (_F)) != 0)

#defin_p)   EXT;SHORT      Syption1Enabled)
#define STA_TKIP_ON(_p)                 (_r MIimpleRspGenAnd
	UL**********************************                 lticastTransm   MPDUIn AllocVa           	UCHAR       Re comp	UCHAR       R6)) != 0)
CATTER_GATHER_ELEMENT {
  dls/ QueupportedHtPhy.MimoPs = _pAd->MlmeAux.HtCapability.H#endiDlsgs &= ~(_F))
#define CLIicateCount;
	LARGE_INTEGER ble, 1: time-based,y)->ClientStatusFlags & (_F)) != 0)

#define RXAux.HtER_SET_FLAG(_pAd, _F)    ((_pAd)->CommonCfg.PacketFilter |= (_F))
#defi  RxSiningTimeForUse;		//unit: sec
	UCT_FLAG(_pEntrfg.PacketFilter	1N_DRAFT3
	BOOLEAN    aCfg.WepStedChannel;	// For BW 40 operating in 2.4GHz , the "effected cha Remain_OF_DowtaActive.se;		//unit: sec
	UCHAR      Channel;
#ifdef DOT11N_DRAFT3
	BOOLEAN   wer;
	CHAR       Power2;
	UCHAR      40 operating in 2.4GHz , the "effected cha          LSTEP_OF_TX_RATE to last unstable covgRssi2X8;
}Rcv     DLSortedHtPhyrginal Ethernet frame con UCHAR defaul	  \
           NdisMoveMCapabilit              	    (QueueHxFFF) _CHANNE  \
    else                      (NdisEqualMemory(SNA                  LSwer;
	CHR, *PCHANNEL_11J_TX_POWER;

typedef struct _SOFT_RX_ANT_DIe structure
type
	ULSTAKey
			NdisEquateToRxwiMCS[];
extern UCHAR OfdmSigluate status, 2: switching statn UCShak                               AR     Pair1PritCapInfoebug purpose, mightPCancelTimer(&((_pAd)->Mlme.LinkDownTimer), &Cancelled);\
	STA_EXTRA_SETTING(_pAd); \
}
#endif // CONFIG_S  BssId[6];
#ux.Hte.Supp      (_p->StaCfg.WepStat_SUPPORHAR   TxTP_SCATTER_GATHER_ELEMENd;       ity.HtCapInfo.*pDLntry)->CliePation
/pe comp(QueueHeSet_DlUINT32rSui_Display_XT;


//
/T32		TxSwFreeIdx; // software arHER_E   (Val.QuadPartNG RansmittedOcdif // C

//
// Tkip Key structure which RC4 ke             e / 2dif // CO              tAvgRssi       //
	ULONG    aIdx;
	INT32		RxSwReadIdx;	// wag) & 0x1LastTransmittedFHORT     Pair2Lasdif // CypedeLook                AllocVa;       OLEAN   FirstPktAr         for DIdel\
{		ITY;


/********************************Bylag)or DMA operation, buffer must be
//, *PSOFT_RX_ANT_********
  *	structure for radar detectNNEL
#endifAddapabiPPORT //
	teToRxwiMCS[];
extern UCHRemoveHe	  //
	SRUCT {
    wer;
	CHOOLEAN		IEEE80211H;			// 0: disable, 1: enable IEEE802.e.SupportedHtPhy.ShortGIfor20 = _pAd->MlmeAux.HtCapability.HtCapInfo.pAd->StaActive.Shannel <= 12PPORT
	SHORT  _TUNING;

// structure to store cvgRssi[0]:E1, AvgRssi[1]:E2
	SHORT     Pa#endif // DUCHAR		BBPR16;
	SCHAR		BBPR1ern USHOpecTxRetryOkCountBPR18;
	UCHAR		BB:Ant-E2
	UBBPR22;
	UCCA;*/
	efins
	SHORT     PKIP64, CounyInt unit: sec
	UILONG       K1;
	SHORT     Pr count, for debug purpose, miggs.Channel <= 12" is t Detection mode
	UCHAR		RDDurRegion;		//Radar detection duration region
	UCHAR		BBPR16;
	UCHAR		BBPR17;
	UCHAR		BBPR18;
	UCHAR		BBPR21;
	UCHAR		BBPR22;
	UCHAR		BBPe's quality.erviceMonitorCount; // unit: sec
	UINT8		DfsSessionTime;
#ifdef DFS_FCC_BW40_FIX
	CHAR		DfsSessionFccOff;
#endif
	BOOLEAN		bFastDfs;
	UIwer;
	CH Detection mode
	UCHAR		RDDurRegion;		//Radar detection duration region
	UCHAR		BBPR16;
	UCHAR		BBPR17;
	UCHAR		BBPR18;
	UCHAR		BB AvgRssi[h;      \
	_pAd->StaActive.Sup********************
  *	Channel and BBP r        ynorted;
} ROGUEAP_ENTRY, *PROGUEAP_ENTRY;

typedef struct {
	UCHAR               RogueApNr;
	ROGUEAP_ENTRY    	CHAR eApEntry[MAX_LEN_OF_BSS_TABLE];
} ROGUEAP_TABLE, *PROGUEAP_TABLE;

//
// Cisco IAPP format
//
typedef struct  _CISCO_IAPP_CO \
	MICKEY
	UCHAR       RC4KEY[16];
	UCHAR       MIC[8];
} TKIP_KEY_INFO, *PTKIP_KEY_INFO;


//
// Private / Misc data, counters for \
	
	// Tkip stuff
	TKIP_KEY_INFO   Tx;
	TKIP_KEY_INFO   Rx;
} PRIVATE_STRUC, *PPRIVATE_STR \
	 stuff
	TKIP_KEY_INFO   Tx;
	TKIP_KEY_INFO   Rx;
} PRIVATE_STRUC, *PPRIVATE_STRJoiETECT_THRESHOLD			0x0fffffff
#endif // RT3390 //
#ifndef RT3390
#define CARRIER_Dry i Seconds;          //Seconds that the client has been disassociated	CHAR hained
	UINT            TotalPacketLength ;     // Self explain the scProbe
			NdisEqual                        \
}

ne CAun    LLCRTSSuccessCount;
	LARGE_********************
  *	Channel and BBP re)
#define Rl toRGE_INTEGER   TransmittedFr

typedef struct {
	UCHAR              RogueApNr;
	ROGUEAP_ENTRY    ONG					2];  //00 01-Invalid authentication type
							//000 02-Authentication tim/ Variables for WEP encryption /			ine miirstBuffer;           // Pointer to first buffer descriptor
} PACstruOid \
	NewDFSDebug
{
	UCHAR chanl;
	ULONG wait_time;
	UCHAR delta_delay_range;
	sidNewDFSDebug
{
	UCHAR channel;
	ULONG wait_time;
	UCHAR  delta_delay_range;
RT TranR EH_step;
	UCHAR WL_range;
	UCHAR WL_step;
	UCHAR WH_range;
	UCHAR WH_sof acoam++;  EH_step;
	UCHAR WL_range;
	UCHAR WL_step;
	UCHAR WH_range;
	UCHAR WH_sWait//LengthNewDFSDebug
{
	UCHAR channel;
	ULONG wait_time;
	UCHAR delta_delay_rangPORTRT /_POWER	8
#define NEW_DFS_DBG_PORT_ENT_NUM		(1 << NEW_DFS_DBG_PORT_ENT_NUM_P   Rogu_POWER	8
#define NEW_DFS_DBG_PORT_ENT_NUM		(1 << NEW_DFS_DBG_PORT_ENT_NUM_PonTim_POWER	8
#define NEW_DFS_DBG_PORT_ENT_NUM		(1 << NEW_DFS_DBG_PORT_ENT_NUM_P    ERIOD_ENT_NUM		(1 << NEW_DFS_MPERIOD_ENT_NUM_POWER)	 // CE Period Table entry numbed2, len2eCollisions;

} COUNTER_ENT_NUM_POWER)	 // CE Period Table entratched Period definition
#define NEW_DFS_MPERIOD_ENT_NUM_POWER		8
>MlmeAux.HtCapability.HtCapIrange;
DLSSetupPORT //
	SHORT     Pair1LastAvgRsty.HtCapInfo.ChannelWidth;      \
	_pAd->StaActive.SupntiguLink                         \
	((PQUEUE_ENTRcalcula   OneSec	USH
	CH                        \
}

#define Ins    Alloc theq
extAPVATE_STRUCtenchuOn
	ULON to wait until upper layer return tBAND,
	A_BAND,
ounter;
	ULONG timestaDER;

#nt-E1, 1OWER)MimoPs;      lMemory(SNAP_802_1H, _pData         JOINF = _pAd->MlmRT /.HtCapabiREKEY, Rneral *PFRAGMENT_FRl
typedef	union	_PS_CONTROL	{
	struct	{
         teWpaNone_pAd->Mlm, 0x40, ess
} RTMP_DMABFirstPktArrition
//
typedef struct  _TKIalue
	UCHLONG	ntion . New ;

#ifIntvdefine CARRIEl
typedef	union	_PS_CONTROL	{
	struct	{
		ULONG		ESCAlePSinIdle:1;	ne CARRif // CARader)**************************NewDFSMPeriod, *pNewO  {
	UINT    RIERNewDFSMPerio_ENT_NUM_
typedef	union	_PS_CONTROL	{
	struct	{
		ULONG		EError RaF = _pAd->Mlm//Length of  Chip power save fucntion . New  AvgRssi[1hanism frtG		rt30xxPowerMode:2;			// Power Level Mode for rt3TART chip
		ULONG	remellowHostAS****************************************ry nu
typedef	union	_PS_CONTROL	{
	struct	{
		ULONG		EAR  wPS:1;		// Enadriver  Chip power save fucntion . New AlHER_ELEMENhold;
	UUINT            X;
	UINT            Y;
	 the sc	CHAR                                 //
}CARRIER_		// EnSHORT      Sequence;
	USHORT      LastFrag;
	ULONG       Flags;      define CARRIER_DETECT_THRESHOLD			0x0fffffff
#endif // RT3390 //
#ifndef RT3390
defin // CONSHORT      Sequence;
	USHORT      LastFrag;
	ULONG       Flags;      RRIER_DETECSHORT      Sequence;
	USHORT      LastFrag;
	ULONG       Flags;      #define CARRIAPOWERRT     Seconds;          //Seconds that the client has been disassociated	CHAR AtONG	  AssocFunc[ASSOC_FUNC_SIZE];
	STATE_MACHINE_FUNC      AuthFunc[AUTH_FUNC_SIZE];
      AssocFunc[ASSOC_FUNC_SIZE];
	STATE_MACHINE_FUNC      AuthFunc[AUTH_FUNC_SIZE] Seconds;          //Seconds that the client has been disassociatedUINT8			SHORT      Sequence;
	USHOR	STATE_MACHINE           SyncMachine;
	STATE)  |EAN	Cancelled; \
	(_pAd)->StaCfg.OUNTER_802MakC. ND    ActFunc[ACT_FUNC_SIZE];
	// ta_delay_rCXAdjacentA
      HER_ELEMENT;


typedef struct _RTMP_SCdefine CARR	SHORT     PlMemory(SNAP_802_1H, _pDatavgRssi[0]:E_SUPPORT //R_ELEMENTonTime;
#ifcalculate TRY TUNNE    yInteonTime;
#ifd failure,G           NG		rt30xxFhannel <= 12_SIZE];nNG TbeCcaC_SIZE];



	ULONG                   ChannelQuality;  // 0..100, Chann      // fMsg       OneR   LastTransmitad =R		BBPR16;
	 Transmi for Roamboth succonTime;
#ifd         Now32;      
	MLication forUCHAR		BBput
	UINT32    onTime;
#ifdnning;
	NDIS_SceMonitoraionMode;   TRY    // CRC erro* RTMP(_p, _pM for MPDUCounpTxNoRetryRALINK_TIMER_UINT32    8;
	UCHAR		BBPR21;
	UCHAR		BBiodicTimer;
	Er     LinceMonitoDtim\
{		           bPsPollT     PeriodicTimer;
	BTxAcBeaconS        bPsPMessageToM;

	RALINNT        nBytesIueue;

	UINT ount;
	UINT3        bP  OneSecFals  UCHAR          // CCA eKickTimer;
	RSecBeaconS          OnAironetCell; eitLimiRunning;stored the tx tx packet de     Tasunt + OneSecTxRRetryOkCount 					beSecTotalTxCount; // OneSecTxNoRetry         AtorCer tIRadioOnOffTimGMT_Ref DFS_FCC_BW40_F                        *
0RfR24;
	UCHAR PreNCaliBW20RfR24;
#en*                              f RT3r count, for debug purpose, might RfR24;
	UCHARCnt;      // unTRY e to global couner
	UINT32iodicRound;
	 DATA framNK_TIMER_STrn USHOFailCount
	U   \
           LastOneSecRxOkDataCnt;hannel <= 12us == Ndis802_ Detection mode
	UCHAR		RDDurRegion;		//Radar dGATHP_SCATTER_GATHER_ELEMENTIS_SPIN_LOCK     ering_mpdu
{
	struct raCfg.Wepg_mpdu	*next;
	PNDIS_PACKET			pPacket;		/* coverted to 802.3 frame */
	iLEAN		bFastDfs;
	           Detection mode
	UCHAR		RDDurRegion;		//RNG];
	UINT32rdering_list
{
	struct reordering_mpdu *next;
	int0x00, 0x40, C_SIZE];



	ULONG                   ChannelQuality;  // 0..100, Channel Qpower savave fucnRALINK_TIMERf RTMP_MAC_PCI
    UCHxx
	UCin chip ve*************2872. and PCIist;
};

typedriverm _REC_BLOCKACK_STATUS
{
    Recipient_NONE=0,
	Recipient_USED,
	Recipient_HandleRe
    RecipiTUS, *PREC_BLOCKACK_STATUS;

  ****    WpaFunc[W // CONC_SIZE];



	ULONG                   ChannelQuality;  // 0..100, Channel Q             Queue;

	UINUCHAag and g_mpdu
{
	strr MI
{
    Originator_NONE=0,
	Originator_USED,
    Originator_WaitRes,
    Originator_Done
} ORIRI_BLOCKACOCKACK_STATUS;
66DeltRI_BA_Status;'s qualUEUE      ChlgText 0)

#g_mpdu
{
	str
typedeTA frame's sequence field in 802.1ket;		/* coverted triginator_WaitRes,
    Originator_Do      Taer;
#ifdef RTMP_MAC_PCI
    UCHNG		upperlimit;
K_TIMER_STRUCT ME_QUEUE ffTimer;
#endif // RTMP_MAC_PCI //
	ULONG                   PeriodicRound;
	ULONG        IX
	CHAR		DfsSessionFccO	ULONG       K1;  	ULONG		      L;          // Curren2          OneSecRxOkCnt;          // RX with	ULONG		lowerLLC/Sef DFS_FCC_BW40_FIX
	CHARf CONFIG  // Curren	rcvSeq;
	REC_BLOCKACK_*************       (OPSTATnt;
	UINT32          LodicTimer;
	RSecBeacLEAN		bFastDfs;
	       TA frame's sequence field in 802.11 header.
	USHORT	Sequence;
	USHORT	TimeOutValue;
	OSize;	// 7.3.1.1 AvgRssi[1hannel <= 12   {RUCT   Detection modield from the result Next;
	P 8) +s no LLC/Sted to 802.lags |= (_:E2
	SHORT  LC/SNAP; NUPBA_culateNTRY;

 _pAd->MlmeAux.HHtCapabilityxt;
	int	qlen;EXT;RDERDMABUF MAP_RXBuf[MAX_RX_REORDERBUF];
	NDIS_SPIN_LOCK          RxReRingLock;                 // Rx Ring spinlock
//	strucUINT8			RDERDMABUF MAP_RXBuf[MAX_RX_REORDERBUF];
	NDIS_SPIN_LOCK          RxReRingLock;                            Queue;

	UINT       m recipient G#defiBEntry); \e;
#ifPtE state machine
DU.
	//UCHAR	Nu*PRE Channel Quality IlTimer;
	RALINK_TIMER_ollTimerRunning;
    RAINK_TIMER_STRUCT     PsPTRUCT     Rand ref coHtPhy.NoSHORT     Pair1AvgRssi[2];    UNTER_DRS c1, *PCOUNT************NETWORK_at
  Network	rt3In phySHORT     Pa2          OneCHAR   BssId[6];
#el Last Detection mode
	UCHAR		RDDurRegion;		//Radar detection duration regionnum _ORI_BLOCKACKs == Nd Detection mode
	UCHAR		RDDurRegion;		//Radar detection duration region
	UCHAR		BBPR16;
	e;		/* seDLS_SUPPOROutgoing  \
    el Quality I_ON))
#d} ORI_BLOCKveMemor ...ypedef strLfsARGE_INTEGER   TransmittedFr, _pBufVA +;     eT_FLAGec
	UINT	ULONy
//
//  Arcfour Structu              _DMABUF   //#dFall "mlmG;

typedteToRxwiMCS[];
extern UCHAR OfdmSi);        t _CIPHER_INT32 ick); \
	RTMPCancelTimer(&((_pAd)->Mlme.LinkDownTimer), &Cancelled);\
	STA_EXTRA_SETTING(_pAd); \
}
#endif // CONFIG_SPeriod;

#endD_BA_ORI_ENTRY;

typedef struct _QUERYBA_TABLE{
	OID_BA_ORI_ENTRY       BAOriEntry[32];
	OID_BA_REC_ENTRY       BARecEntry[32UpD_BA_ORI_ENTRY;

typedef struct _QUERYBA_TABLE{
	OID_BA_ORI_ENTRY       BAOriEntry[32];
	OID_BA_REC_ENTRY       BARecEntr  //ENTRY, *POID_BA_OR  MultipleRetryCount;
	LARGE_I//#dDETECT_THRistence scan for AP. As Ap, supporRaActnectLast      Rece                        \
}

uppoV2	Nu)->NNT32     b;
	UCHAR  // both succes****************ONG		RSSI_SADFSDebunt 4 keys, 128 bits max
	UCHAR   RxcSnaow3/* sequence nstatic, 1:dyFastnamic, 2:rsv,ueAc(pAd, pEntry, QueueHeader Remaynamiumbe)->Number++;TEGER   MultipleRetryCount;
	LARGE_I
#de Amsd
//              PULt frame con	numAsRecipient;	cSnapEncap  bHtAdhTX_RF)  SWITCHs[8];
} OID_BA_OR	UINT3ured		Mpduize[8];
	Oty:3;
		UINT32		Policy:2;	// 0: DELAY_BA 1:IMMED_BA  (//
typedef *pNG  l[MGMT_Rt:8;
		UIN2		RxB          mit:8;
	}	fF))
# Amsdhen not connONG		alcu    HtPhy.NQuaiphe******************************   (Val.QuadPart++)

#deftruct {
	UCHNow more, 0:static, 1PsmAbstract:
 , 3:mimo enable
		UINT32     AmsduSize:1;	ion
		UINT32PsBLE];
	BA_Timer;
} SOFT_RX_ANT_DIVERSITYpshMachine;
	STAT32		Pream;

typedef struct _RTMP_RX_RING
{
	RTM sstion
		U, sizeof(UCHAR)Ba->MaeToB_ELEMapability.HtCapInfo.TxSTBC;     uppoUCHAR)		RxBA                \
		terAcceptA ((((_p)->S	USHOR; \
}

// if pmWidth;  't need to update here.
#def// MIMO poH2		RxBAve more, 0:static, 1:umber--;          	bHtAdhoc32          OneSecReceivedByteCount;	\
		_pRSSI2040Coexist   RxCnly when sitesurvey,
		ULONG                          OneSec
		UINT32		:4;
	}	fie*I //
	ULONIG_STA_SUPPORT //



//
//  DavgRssi2X8;
} RSSIck(&((_pAd)->MacTabLock)); \
	(_pAd)->Mac		Central       OneSec
	UCHAR 1, *PCOUN *                               
#endif /         Hchannel == 56) || (channel == 60)Aux.AddHtInfo. Kickq;
//	USHORT		LastULONG       K1;      Kick      L;           // Current state fStaQuickR*****bit Tate
typedef	union	_BACAP_STRUC	{
#ifdef RT_BIG_ENDIAN
	struct	{
		UINT32     :4;
		UINT32     b2040CoexistScanSup:1;		//As StareSp
        1tion, the buffer must be contiguous 
        d2, len         \
		if (pNext == NTMPUCHAR)t;
	LAduDensity:3;
		UINT32	ne
	STAIP64, 				ax******
  *L_11J_TX_POWER;

typedeport do 2:WEP128, 3e.field.MODEtTransme.field.MODE/* seICULAR PUNULL  AllocPa;   RxAration, the buffer muOpStatusFlags & (_AdetectUE_ENTRY p pProCalibLlcSnapEncap block (Descriptor) flude "rtmp_dEFUSE       *
//2008/09/11:KHnt; ity,supre a efuse<--eueHeset_eFuseG    eeor 3r\
{		N		IEEE80211H;			// 0: disable, 1: enable IEEE802.11h
tails see dumpedef	struct	_IOT_STRUC	{
	UCHAR			Threshold[2];
	UCHAR			ReorderQUEUEromBinN		IEEE80211H;			// 0: disable, 1: enable IEEE802.1ntigu see Physical>Head !Next;      ueueHeader)->Number--;   rn USHG(_M, _F)    eSecTxFailCoodicRounrn US*         OisioNULLEeconSvery nBW_10 or BW        \
			(QueueHeHAR			Reorderp;  //Rssiclude "mledef	struct	_IOT_STRUC	{
	UCHAR			Threshold[2];
	UCHAR	0]
	UCHAR;       \
		 block (Descriptor) fEAN			bLacludeEeepp		ReuNTEGER   MultipleRetryCount;
	LAR see MLMEPeriodic)
typ(y:3;
		UINT32		AmsduEnaPREAD3ct;
	EPeriodicAtheros;
    Y;

/  RTSSuccessCount;
	LARGE_INTucture
 see der)dowCount;
	ULONG			OneSecFrameDuplichan      (OPST   Miniport generic pog.  UsedQUEUE_ENTRthe registry scludeng for 802.11n transmit setting.  Used in advan
    Miniport generic pog.  UsedlUEUE_ENT action. Reset all Num every n secon-->endif





#inclOptibilityTes#defineall Nby johnli, RF p eit+ MAC_ADDtailupOT11_N_SNULLeaderFN[];
lRssi// Mae CLIENT_STATUS_CLEAR_FLAG( MHz
		 UINT32  Tphysi // 3*3
		 UINT32  rsv0:10;
		 //UINT32  MCS:7;Re    e             // MCS
         //UINT32  Phy// end; //chaentErrors;
	3090eHeader)->Head MCS:d = pNext;               \
			(QueueHndif





#iMCS:ORT	idx2;
		 UIN _QUine CKIP_KP_ON(_p)				((((_p)PSFlags |= NULLy->HTPhyMode.field.MODE >= MODE_HTMIX)

#endif // D TxBF:1;
     xxor20 UINT32  M3
	(_pMac      T32  TxBF:1; // 3*3
		 UINT32  rsv0:10;
		 //UINT32  MCS HTMODE:1           // MCS
         //UINT32  PhyMode:4;
 3  } field;
#else
 struct {
         //UINT32  PhyMoSNO:2;
     7:7;              37 // MCS
		 UINT32  rsv0:10;
		 UINT32  TxBF:1;
     7   UINNO:2;
     S:7;              3  // MCS
		 UINT32  rsv0:10;
		 UINT32  TxBF:1;
         UINT32  BW:3; //channel bandwidth 20MHz or 40 MHz
         3INT32  ShortGI:1;
         UINT32  STBC:1; //SPACE
          UINT323 TRANS for all rinE				TMP_TPhyMode.field.MODE >= MODEck(&(_pAd)->Masn't If eApEntry[MAX_LEN_OF_BSS_TABLE];
} ROGUEAP_TABLE, *PROGUEAP_TABLE;

//
// Cisco IAPP format
//
typedef struct  _CISCO_IAPP_CONPSDRY, *POID_BA_ORI_ENTRY;

typedef struct _QUERYBA_TABLE{
	OID_BA_ORI_ENTRY       BAOriEntry[32];
	OID_BA_REC_ENTRY       BARe                 \pabiF))
#duse B)->Number        \
    }                 	// 0: DELAY_BA 1CapabilityKIP64, witctaFixedTxRssi0*******/
#define WLAN_MAX_NUM_OF_TIM			((MAX_LEN_OF_MAC_TAMaxHTPhyMode.fi******TNT32LLC_Lenupport do f /* _tx_ has no	F_TIM			((MAX_LEN_OF_MAC_TAvgRssi2X8;
}**************BW_10 or BW_20
	CHAR    ->Tail == N
	}													ith{MACAddr, TID}exists, so reSmode:2;	// Mn UCHA                         \
	if 
	pAd->ApCfQUEUE_ENTdx].TimBitmaps[WLAN_CT_TIM_BCMC_for Dfine RText = NULL;							\
	if ((QueueHeader)->TaD_WCIHORT   
typ  RTSSuccessCount;
	LARGE_IffTime_INFRlsFunc[DLS_FUNC_SIZE];
#endif // QOS_DTUNNE   DlsFunc[DLS_FUNC_SIZE];
#endif //              // if (BaBiortedHtPh    	RealRxPath;
	RTMP_DMACB
{
	ULONG                   AllocHAR					RealRxPath;
	LEAN		R66Low_F)    r* Ciid;
	// counters
	UEUE_ENTRYtkien[1] = (UCHt monitF))
#kiize % 256);                          \
        MAKE_802_3T_HEADER(_p8023hdr, _pDA, _pSA, LLC_Lenffset]; }


// con/ DOT11l as OPMODE_STA
tyMIC_HEADER(_pffset]; }


// conSCTMP_IO_READ32(M, _F)    IV16AR		CountryCode[3];
	UCHARot use
		UICfg.MBSSID[MICze % 256);                          \
        MAKE_802_3_HEADER(_pNdisEqualMemory(SNAP_802             \
    M	UCHAR  ;

	// Etherne TRY)(PriorK1;       uct _COMMON_CONFIG {

	idx].TimBitmaps[apidtry);			IC                     \
}


// Enqueue this frame to MLME engine
// We need to enque	UCHAR       CountryRegionForABanduct _COMMON_CONFIG {

	BOOLEef CONFIGntry region for A beForRecv(_pAd, Wcid, High32Tt monitoT32		TxBHY_11B, PHY_11BG_MIXED, PHY_ABG_MIXED
	UCHader)->Tail->Next = (P>ALNAGain0) : ((_pAd->LaEncaPEqualMe, 6))              Dsifs;         mWidth;      \
	_  // PHY_11A, PHY_11B,     LLINT32		                 \
    if (NdisEqualMemory(SLLHAR	
	UCHAR       DesiredPhyMode;            // PHY_11A, PHY_11B, PHY_11BG_MIXED, PHY_ABG_MIXED
	USHORT      Dsifs;of usec
	ULONG       PacketFilter;   apidApios ead ORI_BA_STKIP    lobal conkiPEqualMe;	// Enum of couuCha// Number o

	UCHAR       B[MAC_ADDR_LEN];
	USHORT      Beacon           \
    MlmeEnqueueForRecv(_pAd, Wn     Channel;
	UCHAR     GeEnum[MAC_ADDR_LEN];
	USHORT     idx].TimBitmaps[id, _pFrameR_LE*************************************pSignal)        {                , used tontry region for A RING;

typede        \
   SupRateLen;
	UCHAR  AES ExtRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR       ExtRateLen;
	UCHAR _SUPPORTED_RATES];     fset = wcid >> 3; \
		UCHAR bit_offset = wcidcmm_ RX / QueuueHead_CfgSee UP, ryeSecHtPhy.ExtChanOffset = _pAd->Mlm enable IEEE8;      / 256	bUNTER_map

	BOOLEAN	Wireless + 1 */
#t	_IOT_STRUC	{
	UCHAR			Threshold[2];
	UCHAR		BOOLEAN	ysically EE80211H;			// 0: disable, 1: enable IEEE802.11h
	AC_BK;
	BOe // ;
	BOOLEAN		bInServicePeriod;
	BOOLEAN	keyStrinAPSDAC	            *ppedef str UINT3BE;
	BOkPacket;

APSDAC_BK;
	BOPAPSKn stage   rsv0:10;
		CK_STdefine RemoveHeover the APSD SDU;
	US*pHashSY        / 256hDTr[4]
	int						SequeC/SNMKBuf

	ULONG       BasicRateBitmap;        // backup basic ratebitme structure
typeWPA          T;


//
// Tkip Key structure which RFLAGS(_M)      adQuChannel;
	UCHAR n APSD-STAllKeyNULL;							\
	if ((QueueHeadervgRssi2X8;
} RSSISty rentA;

ty_SUPPORTe;
#ifdInPut[4]; 2))))

#d RoamiBOOLEChannel;
	UCHAR 		MpvgRssi0ACAddr, TID}exists, so read BufSize[TIphyc.
 _b)) ? (_a) :	UINT32TI      in ar count, for 2_1HtHpAd)

#d:8;
		UINT3McsSadvanced RY, *POID_ADD_BA_*********************	FT3
typedef enum   // Current state te alAddlag)      Dma   CurrTxRateStableTime; // # oEN), _pMac2,neral Statg registrock size
	Pecured = W       AllocV		numAsRecipient    pability        GataCHAR   {     TUNNEen  AllXD. TxRate S DAng in T.RecomuE_ENTRlags |= (_F)fVA + WPA2RUCT On TXWI.
	REG_TRANSMIT_((_M)->Flags &= te alOFLAGduEnable:1;	//E_GAIN(_pAd)	((_pAd->Lat't need to update here.
#deer)->NumH      teToRxwiMCS[];
extern UCOID     HT_PHYMODCHAR Ttting i // Tx rate i    ndividualtable size in RateSwitAs Sta, support do 2040 coexistence scan for AP. As Ap, supporswitchinOOLEAN		cond ng for 802.11n transmit setting.  Usedcond     * UINT32  EXTCHA:FirstPktArrinsmitSetting.field.   LastSssi // Tx ra, QueueEnstru#define WLAN_teToRxwiMCS[];
extern UCHCHAR       Power2;
	USHORT ULONG   NFIGameCount;
		P----  it: sec
WIChannel;
	UCf last 8olicyINry. NDIS_PACKET store                         ((*(_pBufVA + 1     RtsTRreshold;           // in unit e IS_HT_RG    
	UINToacEntry)	\
	(_pMacEntryunit of BYTe.field.MODEats
	ULONG     Rate.LeapAnmentErrors;
	USHORT          Tx_WCIEffecte) \
	{	UCHAR tim_offset = wcid >> 3; \geAction; //Keep last time *********		// If MCS isn't AUTO, fix rate in CCK, OFDNO:2;
 ANT_INT3RSor d   // Valid TACAP_S].PortSAv LLC_Len[0] = (UCHAR)(_DataSize  Poor= 0XFF  ;  IMAnt   2:B RegTrg.  Us))
#define OPSTRateAvhannel switch   IMMED_BA =1  ;  DELESS   AllocCAP_Sode.
			USHORT		rsv:3;
T_STATUS_CLEAR_FLAG(_*			criteria;
	UINT32					CarrierDebuP_TESt = wy,_F)    ((_pbasiwp	Add  of the GNU General Public License   *ientStte alToOOLEAN		S rec>ALNAGain1) : (_pAd->r;
	struct re      (Val.QuadPar:IMMED_BALONG      Miniportt;
	LAdefa3Mode, MinHT / 256HdOOLEAN  // 0: auto, 1: a       always not use_pData {
	ULONG	
#endif //	 later       }         DerivePTK                       \
	((PQUEUE_ENTRY)*PM    \
  RST (whenANoLONG Regitablished oForABand;	// Enu*Sr AP is not a legacy nForA(IPX, pProto*outpip versionmory(ssag and etFilterGenize, rLLCSNAP: pointer to removed LLC/SNAP; NU*mNT32		TxM           bPrze, r_STRUCT for MBSS
	//HWPA-2007	UINT32		AutoBA:1;	// automatically BA
		UINT		TxBAWinLimit:8;
		UI;
	UCHAR       Ex\
{        merRunntSecAccordisEqualMemory(IPX, ntiguoES_GTKN];
	UNWRA  ExtRatablished ->Hea     bAggregatplate dCCKM fldef STA	c_lost's setting f  *c     //
typedvgRssi2X8;
}fset,Eapo20,                     \
}


// Enqueue this frame to MLMle WMM,ADER(_p8023hdr, _pDA, _pSAable, 1 -    TxRate;         e;        eLEAP) && ignment
	UC											\
PAad = (PQ // Same value to fill in T
    Con_1H[6]disabMs_Len);    UseBGProtection;        // 0g registry        Weerlimit;
	ULO;


typedef struct {
	ULONG	pport do D_28xx_UCHAR       TxRate; 	*Keyr AP is nog registry TxRCHAR	#ifdef CONFIGGT    \
g registry RSNnt
	UIN   // ACK / 0:d_, 1 - useespondering_list	ON_CONFIGTHER_E	(QueueHeader)->Nd, _pFrameBroadCasype
// information from 8RTMP_         SNAP_802 LLC/SH;						\
p) && !try)->C2TSF, (IONeueHeade            Ssid[MAX_LEN_OF_S
	// B TSPEd_k               PPOR/ 0:dn TXWI.
	REG_TRANSMIT_SETTING      (((_p)->StaCfg        R_FLAG() macros.
	// se AP
	UCHAR T NULL-terminated
	UC;
}            y,_F)    ((_pEpt, Rt80] = (UCHAR)(RI_TxTsntry)                            \
}

#dULL-terminated encap is_Status[8]Ts  AllocPa; PInstall, _pDA, _pSoexistence scan us of a d;         // QBSS loadITY;


/*************PACIn= SNONG     RcvPktNumWhenEvaluate;
	BOIdx;	// softw.C f     Gonot uAP rate.
	ATUS_SET_FL**/
#define WLAN_MAX_NUM_OF_TIM			((MAX_LE    le IEEE802.11h specRsniBW40tSecAccordirsnieT //   OneSecn UCHA UP, 2:reasuEUE_ENTRY)(QueueEntry);  struct / IEEE802.11H--ciated AP
	QBSS_

#def st4WayHTES
	UC including 4 byte header
	UC// Same value to fill eric portion h#defef DOT11_DETECTION_SUPPOR2Way     

#ifdef DOT11_N_SUPPORT
	// HT
	UCHAR			BASize;		// USer     ActMachineairMsg1NING)
typedef struct _BBP_R66_TUNINGtect;
#endif // CARRIERIC_ON(_p)			((((_p)->StaCfg.CkipFlag) & 0y;
	HT  // System rE		HtCapability;
	ADD_HT_INFO_IE		AddHTInfo;	// Useful as AP.
	//This IE is used with channel 3witch announcement element when changing to a new 40MHz.
	//This IE is included in channel switch ammouncece number
	UIE		HtCapability;
	ADD_HT_INFO_IE		AddHTInfo;	// Useful as AP.
	//This IE is used with c     	HT_CAPABILITY_I ANT_DIVERSITY_SUPPORT //


	// IEEE802.11H--DFS.
	RA_SUPPORTfg.PacketFilter AFT3
	UCHAR					Bss2040Coex  // System reset counter
	UINtTimerRunning, bit 1: NeedSyncAddHtI   Channel
#define OPSTCONFIG_ST() macros.
	// setus[8];
} O	TxAc1M#defiD_BA_ORI_ENTRY;

typedef struct _QUERYBA_TABLE{
	OID_BA_ORI_ENTRY       BAOriEntry[32];
	OID_BA_REC_ENTRY       BARecEntrn APS****_BA_ORI_ENTRY;

typedef struct _QUERYBA_TABLE{
	OID_BA_ORI_ENTRY       BAOriEntry[32];
	OID_BA_REC_ENTRY       BARecEntr the scef stForPSK	Dot11pportedHtPhy.ShortGIfor40 = _pAd->MlmeAux.HtCapability.HtCapInfo.ShortGIfor40;      \
	_pAd->StaActive.SupportedHtPhy.TxSTBC = a) : (_b))g stattion mode
	UCHAR		RDDurS CoexM)->Flaerating _INFO_IE		AddHTInfo;	// UCommonCfg.PacketFilter 11N_DRAFT3
	BOOLai/RTMNTRY,his IE is used for 20/40 BSS Coexistence.
	BSS_2040_COEXIST_IE		BSS204, 6))           			// Radio Meas6];
#eS DATranDelayFactor)
	ULONG					CountDownCtr;	// CountDown Counter from (Dot11BssWidthTriggerScanInt * DotGREKEYRY, *POID_BA_ORI_EMaxSPLenglTimer(&((_pAd)->MlmeMaxSPLengownTimer), &CancelledT_IE		BSSCoexist2040;
	ad = (PQ_IE		BSSCoexist2040;
	HEADER(_p8023eTxBurGt;       RST (when BA is established Gr AP is not a legacy WMM A    bAggregationCapable;      // 1: enable TX BC detect

#def DOT11_ceMonit	BOOLEOPSEnable;endif // DOT11() ma32 pT //
	B        bPs      bWmmCapaEAN      128_C	UINT32	d MlmeRa
    BOOd MlmeRainCapable;ed. */5: KH addd MlmeRama  AllocPa;[0] = hresho>ALNAGain1) : (_pAd->ALNAGa11BssWidthChanegs.Channel <= 64) F_REGUL2))))

#define INC_COUNWcid, High32TOFDM), PMKIDLLC/SNAP fft use 0 as disable
	ULEAN					 do 2040 (QueueHeader)->H operatin    // ACK 	n BA       \
	}					LONG		N		bAPSDAC  MimeCont;
	BOOLEAN					bExtChannelSws flag to TRement;
fo.ExtChanOedSendTri;
	UINT32		RxAuthMo    CHAR					TxBASize;
#endif // DOT11_N_SUPPORT //

	// Enableervedern Uen not connATE(_pintainCHAR					TxBASize;
#endif // DOT11_N	UCHAR       MindTriggeBOOLEA                 \
{          R   {
	adQueue  \
    NdisMoveMemory((_p + MAC\
                                           //        #endi(*= ~(_F& (_. Th)(1H, _pDatloncket 
/* tn chip -- msoToDisab  rsv0		MpY, *POI(_pAd, _F) /  QuMINI   *&= ~(_IMER_STast.
#iE_SPECIFIC //EAN			bHPSFlags |= (__OS_pena_(_pAd, _F)   Count;
	ULONG        adio enabled



	NDIS_SPIN_LOCK		dif // MCAST_R         smit rate fftwarPMEASURE_REQ_TAB	AddasureReqTab;adio enabled



	NDel),PIN_LOCK			MeasureReqTabLock;
	PMEASURE_REQ_TAB	Mo
#ifdef MCAST_RATE_SPECIFIC
	HTTRANSMIT_SETTING		MCastPhyMode;
#endif /ASURE_REQ_TAB	Del#ifdef MCAST_RATE_SPECIFIC
	HTTRANSMIT_SETAntDort slot (9STATUS_TEST_FLAGASURE_REQ_TAB	Release->StaCfeHeader)->Tail = NULL;  UINT32cketFil*******B */

/* clear bcmuseng dastage fortion	BOOITOR_ON(_p)     os_bei c_me when te al       \
}

#def          O*me   Las      ATE tx;		/* Restrict the freeption type

	NDIS_SPIN_LOCK			TpcaxSPLem struct#endif

#_ _pSA, ppedef   QUEUE_HEAModified by Wu Xi-Kun 4/ortionveMemory((_#if definLLC/S*/
} COMate fo*Virtualr_DoeneSecAntDiader)-HYSICALO, *PATE 
	ULONG	om RegiPQUEUE_ENTRY pNexmber;
}   QUEUE_>ALNAGain1) : (_pAd->ALNAGaITOR_ON(_p)     truct  _QUEU _pSA, p  QUEUE_HEADate foPQUEUE_ENTRYation loENTRb))
#endif

#guration 					\atus
typedef struct _STA_ADMIN_CONFI for           AG {
	// GROUP 1 -
	//   User configuration loaded from Registry, E2PROM or OID_xxx. These settings describe
e with the BSS hol_INFRTxTATUS_TEST_ AP or IBSS holder).
	//   Once initialized, user configuration can only be changed via OID_xxx
	UCHAR       BssType;              // BSS_INFRA or BSS_ADHOC
	UEPer   AtimWin;          // used when starting G {
	// GROUP 1 -
	//   User confi
#define aded from RegistrAST_RATEype;              // BSS_INFRA or BSS_ADHOC
	USHORT   uppor (either
	//   AP or IBSS holder).
	// ed, user configuration can only be changed via OID_xxx
	UCHAR       BssType;              // BSS_INFRA or BSS_ADHOC
	USHORT   Rer (either
	//   AP or IBSS holder).
	// GROUP 2 -
	//   User configuration loaded from Registry, E2PROM or OID_xxx. These settings describe
	//   the user intendeHRESHOLD
	USHORT      DefaultListenCount;   // default liIVE BSS without compromising with the BSS holder.
	//   Once initiaoryClass[MAX_NULLOSNetPktdef s                 \
}

#defineision Hien AC on
	BOOLEAN UPON_EXCCEED_TDisablguration, and should be always applied to the final
	//   settings in guration loaded from Registry, E2PROM or OID_xxx. These settings describe
ID_802_11_SSID with no matThing BSSID
	ULONG       WindowsPowerProfile;    // Windows power profile, for NDIS5.1 PnP

	// MIB:iee(1)
	USHORT      Psm;         s fring BSSID
	ULONG       WindowsPowerProfile;    // Winr BSS_ADHOC
	UQuer  PUCHArSuiTEST_FLAG(_p,->Tail-_OF_REGULATRY  >Tail	USHORT        s
  ******* MlmeRadINT rcBufV BOOLEAze / 25      Pri// For SHORT      Auth)  |FailReason;
	UCHAR       AuthF*N(_p)   MAC_ADDR_LEN];

	NDIS_802_11_PRIVACY_FILTER          PrivacyFilter;  // PrivacyFilter eable;        _oPs;     ason;
	UCH           \
}

#define      IMERAP field


	NDISCATTER_GAT)   LIST
rt_get_sg_list_ET[8_
 ****(R       AuthFailSta[M    // atus set from OID
 *THER_E
ode;
#announcether 3differen  else                                               ) macnt;
eorder_     _AnS		GrouMulticast cipher suite
	NDIS_802_11_ENCRYPTION_STATUS		Pairn UCHA// Unic           teToRxwiMCS[];
extern UCHAR OfdmSignalToRateId[16] ;
ata                 OLBC detect; 1 disabenum f  RateSther ON topCipher;		// Multicast cipher suite
	NDIS_802_11_ENCRYPTION_ST UINT32  EXTCHA:tchRfRe UINT32  EXTCHA:2;
		 UINT3

	NDIS_802_11_WEP_STATUS              GroupKeyWePNErameV  Addnetdevport db        /ueueHeader)->Number--;          SK mode PMK
	UCHAR         AuthFduplVA + _pk Multicast cipher suite
	NDIS_8uto, 1: always use, 2: always not ataSize -= LE	BOOLEAORT								RsnCapability;

	NDIS_802_11_WEP_SGTK[32];				// GTK from authenticator
	BSSID_INFO	Save_ Ral_R_LENLEN_OF_Sse                                    OldPk	PMEAT_SECURED
	UCHAR       PortSecurVLANdPMK[PMKID_NO];
	UINT		SavedPMKNum;			// Saved PMKID number

	UCHAR		DefaultKeyId;


	// WPA 802.1x port control, WPA_802_1X_PORT_SECURED, WPA_802_1X_PORT_NOTPhyMome
	ther3_t;
	LA_Copstage from the station;
		we neuto, 1: always use, 2:T11_N_SUHAR		DefaultKeyId;


Y_FILTER    SignalToRateIK[32];				// GTK from aut't need to update here.#endiba_flush_rer;			ing_AN			bH_mpdAR      ginal Ethernet frame connt;

	ULONG  pBAFS.
	RADAR_MK[32]; uSize:1;	GE_INTEGOriSe withSet     
	UINT32					TimeStS Coexist
	UIN				\
		_pExtraLlcSnapEnca
	UIN 2), _pType, Lived Bg.  Used
} COUNR
	UCHAR rtion healaySMITR
	UCHAR
#endif /is_DMABp, apidx, wA    // wer;
	CHALL       OneSecTxAggregationCount;
	UINT32e Rx AnsduSize = (UCHAR)(_pHtCapabiliABGBAND_OS_Need_     use diff#endNonce[32];b_WCILEARse[64];		// WPA PSK pass phrase
	UINT		WpaPassPhraseLen;		// the len_WPA_REKEfine RTMP_/      Nonce[32];         // wer;
	CH      KickTxCount;
	ULOthis MACaddr.
} OpFlag) & 0xng registrM_OF_TX_RI ((((_p)->SPassivunt;
	ULONG     (9u_DMABUeEAN		bE_INTEGRecs timestamp of the last 11G BEACON RX time
	ULONG		Last20NBeaconRxTime;	// OS's timestamp of the last 20Capable;    baer[8];
	UCHAresourcSTRUC, 

	NDIS_SPIN_LOCK	hen snule T  Replaymost recent SSID, BSLEANase SCAN OID request


	ULONO:2;
 NCRC NDO_APQBSS_Lpena
	BOOLEANd->Ml tim_offset = wcid >> 3; \
		UCH	or tro;           // Hardware controlled Rd onendif /S_de:2;	/ster_Of stateHost'sBPR16;
	Uast.
	ULONOTUSE Del
	BOOLEANwerPa, for High Power issue
	UCceMonit
	BOOLEAN    
    rtmpIoctlNte dodoHAR  [8];
	ORI_BLOCKACK_STATUS  ORI__1H[6] iwreq	*wr\
	_pAd->howHiddenSSID;    RI_TShow all known SSID in SSID list get operation

	// New for WPA, windows wansmisShow all known SSID in SSID list get operation

	// Ne1_N_SUPPOR
	BOOLEAN    // OS's timrtstrmactohene CLIENHostASPs)->Mlme. of next/* sequence n     AR  cm		 UINT3 of next VIE include EID & Les[MAX_VI     _1H[n     ReqVarIEs[MAX_VIE_LEN];		// The content saved here shoMulticcAD_PEs[MAX_VIE_LEN];     // Length oontent saved hstrt        s[MAX_VIE  BOOL    // Length c	PMEAisio1;  et_atActFuLEN];

	UCHAR  able1H, _pData,  *= (PQUEU/ntent sav comm= wcdenS           ntent save1]:E4
#endxBurrV      edef	struct	_IOT_STRUC	{
	UCHAR			Threshold[2];
	UCHAR	
#en	bAPSDCapable     CLBusyBytes;                // Save the total bytes received durninAaseS     CLBusyBytes;                // Save the total bytes reOOLEAN		bAPS     CLBusyBytes;                // Save the total bytes rec******N		IEEE80211H;			// 0: disable, 1: enable IEEE802.11h
	UCHAysically N		IEEE80211H;			// 0: disable, 1: enable IEEE802.11h
	UCHAefine J     CLBusyBytes;                // Save the total bytes reETTING(_pning channel 		bExtChannelSwitchAnnoun*****ave the total bytes resstion
		Uhe same channel with maximum duration
	USHORT              ParRTSThd->Sol Array for annel with maximum duration
	USHORT              Pars frR               ParallelChannel;            // Only one channel with paimWirs saved.
	BOannel with maximum duration
	USHORT         0XFF  ; GGREGA. Thes     *
RUCT {
 oRec= 14) ?tion;           // Maximum duration for parallel measuORT //
	IOTs
	UCHAR          edTxMode:2;	INF_AMAZON_PPA     NHFa// ParameteSEer fN		IEEE80211H;			// 0: disable, 1: LL-termingram pa*        // Parameter f     r channelEEEAR)(_HN		IEEE80211H;			// 0: disable, 1: enable IEEE802.1       TBG11h
	UCHARebugChannel switch counter
	UCHAR		CSPeriod;			//Channel swiUpTimerhow_o thONFIG		IEEE80211H;			// 0: disable, 1: enable IEEE802.11h
	UCHAfor DRUCT#defineHAR			DtimCount;      // 0.. DtimPeriod-1
	UCHAR			DtimPeri     (_p->StaCfRUCT {
 	RSStueOutNum[MAX_LEN_OF_BA_REC_TABLE];	// compare with thres/
	// TDecNameY];
	UCHAR				DlsReplayCounter[8];
#endif // QOS_DLS_SUPPOR     wer;
	CH/////////////////////////////////////////////////////////////Rec/

    RALINK_TIMER_STRUCT WpaDisassocAndBlockAssocTimer;
    // FaHtBwOOLEAN		        bAutoRoaming;       // 0:disable auto roamingMcsOOLEAN		        bAutoRoaming;       // 0:disable auto roamingGiOOLEAN		        bAutoRoaming;       // 0:disable auto roamingO     OOLEAN		        bAutoRoaming;       // 0:disable auto roamingStbcOOLEAN		        bAutoRoaming;       // 0:disable auto roamingHt1x_required_keys;
    CIPHER_KEY	        DesireSharedKey[4];	/Extchay RSSI, 1:enable auto roaming by RSSI
	CHAR		        dBmToRoampduDensitAN		IEEE80211H;			// 0: disable, 1: enable IEEE802.11h
	UCHAHtBa	// to itiates scanning and AP selection
    // 2: driver takes careRdult = 3

#ifdef QOS_DLS_SUPPORT
	RT_802_11_DLS		DLS takes care	USHwer-sitiates scanning and AP selection
    // 2: driver takes careAmsduBOOLEAN				bRSN_IE_FromWpaSupplicant;
#endif // WPA_SUPPLICANTutoById;

    // 0: driver ignores wpa_supplicant
    // 1: wpa_su) || (cy RSSI, 1:enable auto roaming by RSSI
	CHAR		        dBmToRoamimoP          // the condition to roam when receiving Rs/////
	//_DMABUsicaGIY];
	UCHAR				DlsReplayCounter[8];
#endif // QOS_DLS_SUPPOR_DMABGFY];
	UCHAR				DlsReplayCounter[8];
#endif // QOS_DLS_SUPPOONG ontable size in RateSwitchT measurement /

#define CKy RSSI, 1:enable auto roaming by RSSI
	CHAR		        dBmToRoamIMOPSc.
 ode, MinHTPhyMode;// For transmit phy setting in TXWI.
	DESI2		b, MAC_itiates scanning and AP selection
    // 2: driver takes careRTMPei CR_LENAR			DtimCount;      // 0.. DtimPeriod-1
	UCHAR			DduSize = (UCHAR)(_pHtCapabilinkUp, Used LowerBound or Upp//Dls ,	kathyIVERSITY_STRUCT {
	UCHAR     EvaluatePeriod;		 // 0:not evxAnt;   // 0:Ant-'t need to update here.//or 3rd CK     L AuthBA    // HardPCI register read / write
QUERY  ReceivepBAUINTD_CHANNEL_LIST
	UCHAR				IEEE8NO:2;
 WP      LIC  IM            08/11/0     \apternn TXWI.
	REG_TRANSMIR;

typedef strucRTMP_time
	ULON(Dot11Bsidth//            R       Lad inS    (((_M)-8/11/05: K{         To1/05upD_INFNSMI>ALNAGain1) : (_pAd->ALN       La_SUPPORT
#endifxistInfo;
	//bUn	TxAc agreed upon 2 mNTRY,IEsork. Which means these parameters are usually de  // 1: forcedisable
} STA_ADMIN_COT BURST, 0: NATIVE_disable
} STA_ADMIN_CONE];	wext_nive.S_eme PH pFirstBuffe rsv0:10;
		 UINT32  TxBF:1;
 RUE.
// Normally, after SCAN T BURS*                                 't need to update here.
#defair & G;
	LWidcuretream;_cond                 \
		if (pNex  ReplLONG led // if (_ORT Ra_iC's version
	BOOLEAN     bIEEEHT_EXT_CHANNELbfiled _ANNOUNCEMrrenUCHARtaCntD_CHANNEL_LIST
	UCHAR				IEEE80ONG      PR  RALINK_OUI[]n UCHAapability.HtCapInfo.TxSTBC;
	UINT8		) <<atch
	// AP's supported and extended rate ;               argP;		/BOOLEAN				bAutoConnectBy _LEN)*
 *****i5, RATionE_STRUC,fVA + __LEN)use different cipher suites
	USHORT	 SNAP_802_1H;						\
		if (NdisEqualMemory(IPX, S];
	SCHAR       SupRateLen;
	UCHAR       Eup use different cipher suites
	USHORT	 SNAP_802_1H;						\
		if (NdisEqualMemory(IPX,D_CHANNEL_LIST
	UCHAR				IEEE8currBF:1; l******RxAR       SupRateLen;
	UCHAR      ******ateLen;
	// Copy supported ht from desired AP's beacon. We are trying to match
	RT_H;
	UCHAR      CONFIG_ Gary, for High Power iss of a particular
	_1H;						\
		if (NdisEqualMemory(IPX, #endi       _os    Lasic ra This is WDS Entry. only for AP mode.
	BOOLEAN		ValidAsApCli;   //This is a AP-CliwlanCHAR		WpaPassPhrase[64];		// WPA PSK pass phrase
	UI SNAP_802_1H;						\
	WPA-PSK supplicant state
	            qualMemory(IPX, ) macdea;        BILITYUS		GrouMulticast cipher suite
	NDntains no LLC/SNAP, then a								RsnCapability;

	NDIS_802_11_nmentErrors;ntMode;
	UCHAR				Sta r      LLC andK[64UCHA_3LENGTH_*********d to rtry)->ClEMOVE_LLC_
	NdCONVEd


OpCiphe(_OLEAN c _always use, )	\
{r
	// B    CMTimerRu\io state
_p      dLLCSNAP 	UINT3FailDAFailSA;S(_M)        ((_M)-RSNIE_Len;
	UC\ recei     CMTimerRunninT32	 SNAP_ that it will aMICFafRX_MESH))
	UCHAR           RSNIE_Len;
	UCHUCHAR {N_KEY_DESC_NONCE];
	UCHAR                 R_Counter[LEN_KEY_DESC_REPLAY];
	AR  MBSS =HAR s progalways ;      r
	UCHAR           RSNIE_Len;
	UCHLAY];
	UCHAR  S    
extern     PTK[64];
	UC +TE tx.
	BOOLEAN	bQARxSt RSNIE_Len;
	UCHAR }        R_Counter[LEN_KEY_DESC_REPLAY];
	U_Counter[LEN_KEY_DESC_REPLAY];
	UCHAOLEANEN_KEY_DESC_NONCE];
	UCHAR           R_Counter[LEN_KEY_DESC_REPLAY];
	UCHAnce[LEN_KEY_DESC_NONCE];
	UCHAR           R_Counter[LEN_KEY_DESC_REPLAY];
	UCHAREN_OF_RSNIE];
	UCHAR           ANoINFRA))rRunnin
	NDIS_802_11_WEP_STATUS              WepStatus;
	NDIS_802_11_WEP_STATUS AR              PTK[64];
	UCHAR   1       ReTryCounter;
	RALINK_TIMER_STR         GroupKeyWepStatus;
	AP_WPA_STDLS  WpaSerRunnin             PTK[64];
	UCHAR   2n  un;
	INT				r RT        RSN_IE[MAX_L	PMKID_CacheIdx;
	UCHAR			PMKID        ReTryCounter;
	RALINK_TIMER_STRMAC_/ A timer which enqueue EAPoL-Start for triggering PSK SM
	NDIS_802_11_A


	UCHAthState; // for SHARED KEY authentication state machine used only
	BOOLE;
	GTK_STATE       GTKState;
	USHORT          PortSecured;
	NDIS_802_11_PRIVACY_FILTER  PrivacyFilter;      // PrivacyFilter enum for 802.1X
	CIPHER_PMKID_CacheIdx;
	UCHAR			PMKID[Lde;
	SST             Sst;
	AUTH_STATE      AuthState; // for SHARED KEY authentication state machine used only
	BOOL/ A timer which enqueue EAPoL-Start for triggering PSK SM
	NDIS_802_11_AUTHENTI         CMTimerRunninlf, AP won't updatelways use, 2 MBSS numbe ali  PTK[64     ddr[MAC_NT			TxBF_802_11_WHAR           apiueStartForPSKTimerachine used only
	BOO#endif





                         trying toFowarE_xxx
	HTStagment;     // Mor WPA countermeasures
	ULONG       LastMaseLen;		//     // lBC detect; 1 disable OLBS		GrouporedHt====pCipherDS;	// This is WDS Entry. only for S entry needs these
// if Vable OLBC detect; 1 disable OLStacasts the index in WdsTab.MacTab
	UINT			MatchWDSTabIdx;
	UCHAR           MaxSupportedRate;
	UCHAR           CurrTpportedHtPhy.MimoPs = _pAd->*******/ Copy s_OR_FORWARDdsTab.Ma>Tail(_st
	B_e for WPA_os chip that)tDown;
	UCHAR           CurrTxRateIndex;
	 OneSecTxOkCount;
	UINT32			Onssi2;//US		GroupCipher;		// M OneSecTxOkCou(_RingSize);       \
}


#ifdef DOT11enticator
	BSDID_INFO	ndex;
	// to record the each TX rate's quality. 0 is best, the bigger the worse.
	USHORTenticator
	BS     acTab
	UINT			MatchWDSTabIdx;
	UCHAR           MaxSupportedRaunning;  // Enqueue EAPoL-Start for trigg_MAC_TABL,;
	UCHAaverILIT===>
	UmmRxnonAR Cal *(_pBufVA + 13)) > 1500)		\
	{											LI;		// Sta mode, set this TRUE after Linkup,too.
	BOxRingendif // WDS_SUPPORT //

//=====================				\
		_pExtraLlcSnapEncap = SNAP_802_1H;						\
		if (NdisEqualMemory(IPX, zeof(UCHAR)_Rate_SaalseMode.field.MODE >= MODE_HTMIXlien_SAMPL// FiErrCnt;   sending MLMEE frames

enticator
	BS			APUCHARexter *(_pBufVl == 56) || (channel =ode; ForUse;		//unit: s	pher       ode; 
#endif //*pre fr coul[MGMT_Rilter;  32e.
	BOOP*   //

	BID_802_11_SSID wiDRT   trib_802

	// transmit phy mode, trasmit r SNAP_802_1H;			e content save;

	// a bitmap of BOOLEAN flaE_2, RATE_5_denSGLIENteSurv stage from the station;
M)->Flags =get operation

	// NeE80211dClieSNp stConnectByfor snmp , ntryComitPhyMoget ope_policy of the     #undeMinipock si;LAG(), C     [16];
}o-paENT_STATUS_TEST, *PENT_STATUS_CLEAR_FLSEntry[MAendif // RT30xx //

} MLME_enum {
	DIDm to nxind_s;	/s    rm		t receINUS44, definition. fCLIENT_STATUS_hostAN		AMSDU_IN1SED
	ULONG           ClientStatus    ;

	HTTRANS2SED
	ULONG           ClientStatus// if (AMSDU_IN3SED
	ULONG           ClientStatusrssi_AMSDU_IN4SED
	ULONG           ClientStatussq_AMSDU_IN5USHORT		RXBAbitmap;	// fill to on-NT_STAMSDU_IN6SED
	ULONG           ClientStatusnoiHAR MSDU_IN71_N_SUPPORT
	// HT EWC MIMO-N used TaiyMSDU_IN8SED
	ULONG           ClientStatusistxPDU bit i9SED
	ULONG           ClientStatusfrmlenDU bit iASED

};ail bit dPAR)(_ENUM_msgitem_  Mous_no_					DU bit e mapping wcid of reciptruth_faCHAR  t receUCHAUSHORT		BAOriWcitruay[NUM_OF1e maOOLEDoffset = wET[8]madwifioToDccessed via
	//{ided
     008/11did;		BAOriSequen16TMP_Ous_OF_TID]; // The len_OF_TID]; // Tce[NtRate}Latch11t sesuint32_t;
is masked
	USHORT		BAOriSequence[msgled  session. if TXBAmsgtor sEP_OF_TXW secDEVNAMELEN_MAX 16		BAOriSequen8****name[uSize;
	UCHAR		Mmps]_OF_TID]; it is masked

	// 8 Flags;

_IE		HTCapability;

#ifdef DOTe, MinH_IE		HTCapability;

#ifdef DOT// if (_IE		HTCapability;

#ifdef DOTd pa_IE		HTCapability;

#ifdef DOTsq	bAutoTxRateSwitch;

	UCHAR    NT_ST_IE		HTCapability;

#ifdef DOTas orf // DOT11_N_SUPPORT //

	BOOLEatOF_TID];
	USHORT		NonQosDataSeqSHOR_IE		HTCapability;

#ifdef DOT[NUM_Oap bs;	//ng_prism2_hInfo.ator sCHARrARGE cap];
e T32			 precedesern UCHAR SNT32			.it is masked
	USHOR     D _ieeet is _d[16]tapNT32			RT		BAOO pow	it_       ;	/*         0. Only inc>StaesDown; * (c) drasodul// igeAX_V


	ULiMapUdu-1307, UScry);? (_p // 0..1((_p4];
extdoe termsc
{		
exteroToDf // CONFIG_SpaM_OF_TI// The  _MAitT //sQIdleCoun/*atorgth, USA.  whocation forme;
#enin b    	BOOclu of  // 0..10_STA_SUPP UCH

ty  // 0..10TABLE,whichtware4];
exRY, *PMAC_TABLE_EN32MAC_TApresentORT /Aifsnmap telling wlMemDown;
	ULO];
ext Fre      P.es rifsn[31   fAnySt(0x8_INUSE0)ity,ex;   CHAR   fAnyStount;
	by ano;
	RT32on i  McastnyStA;         Check    InPsm;madany Stationytail;         f.  We nee/
}NG			LastBeaconRxTime;
#en;
ail biNG			LastBeaconRxTi    (T		BAOning;

	U_RADIOTAP_TSFTRssicided
/		fAllStationAsRal    S =  _pAd->M		fAllStationAsRaly su = _pAd->St		fAllStationAsRal CfpPer =  2: alwaf all stations are HSink-4/ Check if I use legacy rDBM_ANTSIGNAcy;	5ransmit to my BSS Station/
	BOOLNOIST
	B6/ Check if I use legacy r     QUAfor  = 7/ Check if I use legacy rTX_tus NUHAR   = 8ransmit to my BSS Station/
ck if any Station 9ransmit to my BSS Station/
	BTX_POWERnk-c Check if all stations areABOOLN    (_pHtCapapport GF.
	BOOLEAN			OOLEAN      1OOLEAN         fAnyStationA sess any Sta13inatoAR		AMsduSizetionAsRalPRESENT (Runnin(1 <<N			fAllStationAsRalink;)	|//2008/10/28: KH add to suppo rali) enna power-saving of AP-->
#eny su)  DOT11 eveaccessed via
	// s;	//eaconRxTime;
#endi	NG			LastBeaconRxTime;
#enwt_ihAR dORT 64N SwtsftSTAT powewPHY rasIST_HEADER N;

	RScessfuleaconRxTime;
#e;or session. if TXBAbitmap bit iAP-Clisend_monitor    Lasused to send MLME frames
	UCHAR[MAX_STEP_OF_TX_RAT transmit phy se       2040Coeadd to support Antenna power-saving of AP-s & (_F)) == (_FNT8		TimeOut;
	BOOLEAN bAllTid;  // If Tr		Desired /* /8 + 1HAR			DtimCount;      // 0.. DtimPeriod-1
	UCHAR			DtimPer       Afor _MIXED             NHFaORT
    BOOLEAN             IEEE8021X;
    BOOLEAN      ct _STA_ACTIVE_COsigned char		initedSE_TIME	es reLongT				th;
	Control;
#endif // RTMP_MAC_11 header
#i	USHORT              ParRANSMaCnt[DIAGNOSE_TIME];
	USHORT			TxFailCnt[DIAGNOSE_TIME];
//	USHORTvgRssi2X8;    )   se		DelRecAct;#endi*_    ->StaCfg.bC			TxDDMAEXTRA_Sor delivery-enabled/ FixePSFlags |= c queueF))
#d in scale of 0~14, >=15
//	USHORT			Txxx_SizeToBdefineo                     \
}o read BufHORTis parameteEKEY, RCHAR               A == 52)osassoRSNIE_t in RUC, *PIOT_STRUC;


// ThisVIE include EIpENT_STAMass
	NDISf 0, 1, HostNLEAN   
	USHORT			TxDSecuion KeyAding for    // RATE_1, RATE_2, RAlags |= (_fo.ExtChanOlags |= (_UCHAR       		numAsRecipient;		// I am ree structure
ttmpNesposkRGE_INTEGE        \
			(QueueHet == NULLrange fExC, *PIOT_STRUC;


// This is tation count in uppoge from 0 to 15, in setp of 1.
	USHORT			TxAMDU Aggr[DIAGNOSE_        \
			(QueueHe#enditbtt_taskletTE_SPECIFIC //smit rate       PTK in PhyNEV_Ivrom 0 to 15, in setp of 1.
      fEQ_TAB	NETDEV_OP_HOOs;	/NetQueuTxAggCnt[DIAGscale.
	USHO1.
	USHORT			TxRalinkCnt[DIame cont    PTKen;
ded PCIueHeatmpRaDevCtr	idle_time;           \
}

#define	// T// P sessinf // latch the vatal Data coun1.
	USHORT			TxRalinkCnt[DIAGNOcap)	\
{													;
}   ong, Rt80cid >> 3;bleShortbitm_pciOLEAN	sv:26;		scalCI_							RSSID, B========================
//WDap)		\
{													  ((((_p)->Sts
		UOutWindowCoun >=1}									OUNTE
}RtmpDiagStruct;
#SingTHER_if // DBG_DIAGNOSE //


struct _RTMP_CHIP_OP_
{
	/*  Calibration access related callback functions */
	int (*eeinit)(RTMP_ADAPT32		Ad);										/* int (*eeinit)(RTMP_ADAPTER *pAd); */
	int (*te;
	UCH-2007NuLEAN			T32  HTMODnctions */
	int (*eei	DiagStruct;
#ralleead)(RTMP_ADAPTER *pAd, int offset, PUSHORT pValue); */
	int (*eewrite)gTMP_ADAPTER *pAd, USHORT offset, USHORT nit)(RTMP_ADAPTubAd);										/* int (*eeinit)(RTMP_ADAPTER *pAd); */
	int (*eeread)(RTMP_ADAPTER *pAd, USHORT offset, PUSHORt == NULLStruFinalct;
#endif // DBG_DIAGNOSE //


struct _RTMP_CHIP_OP_
{
	/*  Calib       LatotalodifMAC_ADDR_LElback fu   Atimhen not connDiagStr_pData arg0, 
		(QueueHeader)->Number--;          SCO_AuthModeLREG_TRANS(*sendCoUCHAR arg1);;	/* intKickOF_TX_RATER *pAd, int offset, PUSHORT pValue); */
	int (*eewri          		DelOriAPCIuppo RF access rcale of 0~14, >}

#define, UCHAR cmd, UCHAR ontains no LLC/SNAP, then anifdef RTMP_MAPrivacyF
//2008We nWEP_STATUS                     r trix   NdisEqualMemory(SNAP_802_1H, _pData,                      e for sending MLME fram                           \
     TxAggCnt[DIAGNOSxxPcinterOARGE_INTEGER   MultipleRetr                             LevAR   nMSDt11BssWidthTriggucture.
//
typedef stru			OS_Cookie;	// save speci                        \
    (_idx)ev;
	ULONelated element
	unK, pProto, 2)) &&  \
penaPCIe	USHcoun11B, PHY_1RemovedLLCSNAP = NULL;         ueueSHOR	/* egather frag list structure

//
#define RT       *pDirectpentroY)(Queud->MlmeAux.AddHtInfo.AddHtte;
	UC  g_if_id*********************Filter = Ndis802_11PrivFilterAcceptAoken, UCMa test

	// Tx rt3NT32****lRxPath  *pDiT_LNA_GAIN(_pAd)	((_pAd->LatCS Count inPciStaze;
	PVOID                   AllocVa;            // TxBTxBuf virtualPUCHAR                physical memory
//  Both 	AmsduSize:1;	// 0:3839, 1:7935cture.
//
typedef struct  _******ry
/	Dot11BssWidthTriggerScanInt;			// Unit : Second
	USHORT					Dot11OBssScanPassiveTotalPerChannel;	// Unit : TU. 200~10000 umToNex	Dot11BssWidthTriggerScanInt;			// Unit : Second
	USHORT					Dot11OBssScanPassiveTotalPerChannel;	// Unit : TU. 200 address
	NDIS_PHYSICAL_ADDRESS   Allookie;	// GE_INTEGER   RTSSuccessCount;
	LARGE_ISHORT				DeviceID;      FFPercentage
	UINT8		PwrConstraint;

#pedef struct _COUNTER_reSpTurme cRFClFFSET] &= ~BIT8[0];

/* setl bit defind)->MacTab.Content[BSSID_WCID].e PICnpower status in Configurumber--;          11, *PCOUNTrom 0 to 15, s= ~(_FTASKADMIN_CONFIGg1);;SMIT_QR   ting foif // 	TxAc1;
//
typed are ok to set .alue to		b3090ESpttedOcUSHORT			RxCrcErrCnt[DIAGNOSE_lags &= ~(_F))
#defANSMIT_xMcsCnt[DIAGNOSE090ESp      		CheckDmaBusyCount;  // Check Interrupt Status Register Cou#endifT					int1.
	USHORT			TxRalinkCnt[DIAGNOSE_TIx9280.
	ULON
	USHORT			TxRalinkCnt[DIAGNff;						// flaok to set .
	BOOLE       H];

	// a bitmap of BOOLEAN flagAN flags. eachunt + oadpenalty due in 11n HT m=15
//	USquence[ // Share= (_     CentralChfor RX descriptors
CHAR			L CONFI		*buf/ 3*3	      // ShareUCHAR)mory for RX descriptors
	RTMDMABUF             RxDescRing;          ventsTimehowCfgrectpathCb;
#endif // INF_AMAGNOSE_TIME];
//pT			AR				irq_disableR		BBPC, >=8

	isRadioAR Cal        S    _SUPPORT, either ON AR  ENTICHAR   toTxR_1, ch;
	UCH*********************is 6-*****************Virtualstraich;
	UCMABUF             RxDescRing;           DEX(_idx, _RingSize)    \
{    		irqta
        LLC_L;
	UINT8					recheck1;
	UINT8		taAddtAvgRssi
	ULONG     RcvPktNumWhenmum duration
0211H;			// 1: enable IEEE80TMP_ADAPT****UCHAR		_
{
	UIn500Kbpn1==len2q;
//	USHORT		LasLONG       K1;         // 				INT32          OneSec	TimeOutValue;
	RALINK_TIMER_***************Cnt;      // unicaR       LaHAR  CipherSuiS           	******eqhreshold;  // default 10      Both PCIin 2.4GHz , thep= (_(_M)->PSFlags &= ~(_F))
#defpor debuglways be 	USHORT  	pSMd;


/***at this STA
SeqNRTMP_INFbRcvBSSWidt DAT    TxR   Wcid;
	U***********aconRxct _STA_ACTIVE_CONFIG {
	USHORT  e with theHAR     Medi3hdrG_ENDIAN
 struct {
      ENTRY      _DET#define CAP_SCATTER_GATHER_LIST, *PRTMP_SCATTER_G_802G     11B, PHY_11)->CommonCfg.OpStatusFlags & (_aseS     IAGNOSE_TIME][9closeOOLEAGNOSE_TI			// OSE_TIME][9openR_TASK_SUPPORT //

port AntenVIRTUAL_IF_INC(_ One) (*******->aded frIfCntefin*******************DE*******************************--*/
/*      Tx relatedNUM******************************A PANO:2;
 LINUX
__e Name:N			***********UPntrolled Radio On/O
	UINT32               *****== evet recT32	************pAd->ME];			/ !      t recef last 8 frames' RTRACE, ("*********** SSI2;
 failple;


#i	RSSI2;
 -ssi0;        // sum }
	***************dle th    DeQu0ssi2;********* tras Tx related OWN*********************** Tx related par for so********/
	BOOLEAN             	// RTMP_TIMERUM_OF_TX_RING softwaressi2ct _STA_AC*****        opy OS R           it   OpStatusFlounters
	UI.
	TODO: May#defe******to  recothesePid;
	// cy,_F)    s     ;
	RTpropS_SPlaDDR_*/		DelOriAOSWrix
	HTTRANShreshold;            \
}

#define last ti atteculate TX cReveretIfL*************MP_MA, 5, 6, 7,********pSignal)   last tibitmaconRxT MgmtRingL
	USHOLEAN			OneSecSK_SUPPORCounDev**************def RALIparameters          tta*********                     	// TxAMSUD Aggregation CDevOpt in 1 sRING]; //        CTIMER**************************/

#ifdef RTMP_MDe**************************// Rx Total          onnect;   *********R	RxB                 Rxprivart for trig*/

#ifdef RTMP_MEPerI
	RTMP_RX_RING            in 1 sec sca        ssi1yT			                             	USHORT****T			TxAg*/

#ifdef RTMP_MiceRefPF_TX_RATEommand Queue spinlock
#endif // RT3090 /CreODE >= MO           \
}

#definecReverdev****************devTMP_AD         CK  Mem
#else
	strRemoveHeaT			PrefiULONG *
	ge f N_LOateLe                     MgmtRin    */

#ifdef ge fCustomizle_reg;
	UINOountSs;	//asORT //
} */
	UINT32tive.STo1.
	USHORT			TxAC version. Record10 sec, TxDMA APUINT32KPs;      (0x28600101)..

	// -------------------------d memory of allAC version. ReUCHA	USHORT  ONG skd;


/*R   {
	ME_STi;    ----------------------************sion;          // byte when s(*fn)EACON *)inor vte 1: g in TXW****File******************************************	// TxAMFrg1);;OSunusO****_VIE_LSHORTpPamory((_pE];	Y rate se  // ansmitSe MgmtRingLunusAC_PCI
	RTMOM_BBP_PARMosfF_TX_RING]; //epromASeeRecAct;
	UCH	EFuseTag;inor versi 8 fraBOOLEAN				EepromAetting fo------
	// BBP ControlAddressr_OffY       RSNIEea[MAX_LENEAN				EepromA_BIG_ENDIAef MERGE_ARCH_TEAM
	UCHAR                   BbfineeaconRxT// ---------SrSui Policy subfOM_BBP_PAS	USHOR*ID
#else
  pValue)
//
//had agreue spin    __	// TH__
