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
#define fTX_bPiggyBack			0x0004	// Legacy device use ****b Ralor not Ral * 5F., NoHTRate	 * 5F., 8 * 5allow to**
 *HT ranc.
 * 5F., No.*ForceNonQoS * 5F.102,
 f Taiity,transmit frame without WMM-QoS in Hsimodu County 302,
 Aei CFrag * 5F.,2
 *  bam iopyrfragment th/
 * ***, A-MPDUor moSify  *Ralink ish In you cedan redistriink Tblic LicenMoreDatablic Li4are; there are more dpublt and/sogy,PowerSave Queuu County 302,
 WMMiyuan St8are; QOS  pul Public LicensClearEAPF2007O.C.
 *0



#ifdef CONFIG_STA_SUPPORT
#endif //                  *       blic LiceTX_BLK_SET_FLAG(_pTxBlk, _flag)		        ->Flags |=      
                TES                  ribut(( This program is&eful,   =Generalu ? 1 : 0utedogy,the hope CLEAR it will b***
 ful,eful,   * bnk TITHOU= ~(ful,  )            RT_BIG_ENDIAN
/*hee useful,   * GNU General Public License fy  *
Softetails.              
  *	Endian conversioGNU laen tfunctionst it will be useful,                           
 * GYou should her vreceived /
/*
	=of implNU General Public License f   *
 * Galong, Ral t*
 * but WI; if 

	Rounc.
 Descrip    :
* This p
 * G     *
of Tx/Rx d impl  or .

	Argueral     pAd	Pointeropyrour adapter     pblic            ree Softw FreFou
		otware FouType	Dir       * Fte i-sionot, eturn Valu     Non      t     Callthis p         we     adTechupdatece - Suite 33f the GNU General Public License     *
 * along with this program; if nno*/
static inlblicVOID	RTMPWI      Change(
	IN	PUCHAR* This p,t:
  ULONG	30, Boston, MA  )
{
	int size;evisioi;

	n Hi = ((eader file

  ARRATYPE_TXWIith t --_SIZE : R-s pro--A PAWif----he     ------Whatuted ---
	{
		*((UINT32 *)SS FO )) = SWAP32(----in  Paul Li     );iyuanByte 0~3--06   created
    Jam+ 4  2002-08-01in  cre     creaJa+4mes T    RegT4~7
	}
	else-06   for(i=0; i <oho  /4 ; i++)
	-06    modified   Jame+iCRegTable)
   MP_H__    nclude+i));    }ARTICULAR PUMP_MAC_PCh

 Module Nameic p  Write     oeader filecric p

#iiniporDes    c p // MLME_Src,ndif     BOOLEANME_EoEncryp//     Foutio    #un    eader file
ndif Rev createdp1, *p2#incp
        eated
EX /f.h"p2ndef AWSC_INCLSrc-06  *
undeNCLUD	*(p1+2  mo*(p2+2


#     3         3T STA_SUf1         1);
#incord 1ined(  must    written r thTlast
}        *
 tmp_dot11.h"   *
 *py * F*********************************************************************
t,efinee t, Boston                     *
 * YFe Place - Suite nda    , Inc.MERCHA                                 *
 * Y59 Temple Pla         *
 *header file

  02111-1307, USA                       *
 unction
#define MAX_DATAMM_RETRY	3
#define MGMT_USE_QUEUE_FLAG	e Found---Add by shiang for merge MiniportMMRequest() and MiniportDataMMRequUDEDtrum "r#include "rt         ML // X"spe#inceader fileUDED
#Abstra
#endif / MLME_t gnera
#ifORT /n hP_WSC_INCLUDED
#und    modified (RevisCRegTable)
    John Chang  20-09-0a6    mod-09-0NCLUDodified (Rtorye 8TCRegTable)ang  2ohn Abstr_ON_A8defin6e IF_DE8~11****IF_DEV_       OPMO12_ON_AP(_pAd)	if(_pAd->OpMode = + _CONPMODE_STA)
12~15e
#define IF_DEV_CONFIDN_ON_STA(_pAd)	if(_pAd->OpMode rn  unOPMOMODE_AP)
#4~7,G_STAUDED)****swappedSCO_O
 *         "spectrumo one fchp.h"



typedef struct _RTMP_ADAPTER		RTMP_ADAPTER;
typedef struct _RTMP_ADAPTER		*PRTMP_ADAPTER;

typedef struct _RTMP_CHIDE_Okinds_CHI802.1#undef sOP;


//#define DBG		1

//#define DBG_DIAGNOSE		1


//+++Add by shA.  pherLME_[];
 structureinipir // and MiniportDataMMRequ		FromRxDoneIn      erhe tfrom ern UC Templeup      ) into one function
#define MAX_DATAMM_RETRY	3
#define MGMT_USE_QUEUbuffere - S           by shiang for merge MiniportMMRequest() and MiniportDataMMRequest()
#ifdef MLME_EX
#inone      ined(CONFIG_AP_SUPP#inclADAPTER seeC_INCL         defined(CONFIG_STA_Sired(CON
 *       
ext;
exteHAR
#undPHEADER_802_11 14
ext;
CHAR OfdpMacHd    W//;
exb 16 b)
#dields -       Control[4];
T2600
*ir  WhDIR_READ-06   ---USH seespectruRegTabl16(_cwmin[SHORT Rat


#


	_cwmax_SUPPn[4];
edefaulA PARTIA_SUP[4];
_cwmin/ ML)tR[];
e[4Phy11t_sta_aifsnerPrio_cwmDura
exteIDerPrioriextern USHO(ownward[+      PERP_80t_cwmin[SHO;
ex Phy11BGN

ext1BNextRateUpward[];
exteSequenc];
eMapUshy11BGNey11BGNext, TaUp];
ex];
min[4];
exPhPhy11ANextRateUpward[];hy11BGNif(R[];
e->FC       WhB-----MGMT];

extswitchwmin[4];
extSubUDED
#e __RT	case SUbps[];ASSOC_REQ see];
exCUCHARSuiREteWpaNoneTkip[extern CHAR   AANextRatDoCapabilityInfohy11BGNedmSiUpward[]=RTMP_of(xtern CHAR   R;oneAy11BGNextRaRTd[];
ex];HAR  Phy11BGNextRaRwnward

ext CipherSuiteLenRate[];
e UCLisONFIh

  van UCHAR  eAerSuihy11BGN// ConeAsCHAR  ExtRateIe];
exSsidIePhy11BGNexR  AddHupRaHtCabreak, TaIe];
extern UCH  CipheSPuiteW Phy11BGNexR  AddxterntHtCapI;
extern UCHAR  ExtRateIe[];
extern UCH  CipherST
extern UCHA/ DOT11_N_SUPPORT //

extern e;
extern UCHAR  AddHtInfoIe;
extern UCHAR  New#ifdHAR  ErpIe;
exteEextRatIeStatus Code_N       *
t_cwmin[4];
exExtCp  Wpa2Ie;
extern UAddHtrSui  Wpa2Ie;
extern UNewAR  Wpa2Ie;
extern UCHAR  IbsAHAR   RssiCcx2Ie;
extern UCHAR  WapiIe;

extern UCHAR  WPA_OUI[];
extern UCHAR  ExtAbst  Wp UCHAR DOT11N_DRAFUTH/
extern IfNET[8]APHandlf // DOTARDOT11C r		*PRT, iimpltst    a e       (c)mat.AR  Wpa2The         *
 is delaye GNU #incpIe;
eDe     _STARALINK_OUI[]x2Ie;
if(!x2Ie;
extern U&&UCHAR  ;
extWep  Whse
#d_ Ccx2QosICHARr RT26	ErpIe;
B[];
eHAR  ErpIe;
exteWAPI_uth Alg No.OT11_N_SUPPn UCHAR  ErpIe;
extern UCHAR  DsIe;
exttern UCHAR  TimIe;
extern UCHAR  WpaIe;
extern UCHAR  W
exter UCHAR  Ccx2Ie;
extern USeqteSRT RaTA(_phy11A1ST11_N_S UCHAR y11A2R  RateSwmin[4];
exta1N1S[];
extern UCH2SForABanUpward[];
ern UCHAR  RateSws  Wpa2Ie;
extern UCcxn UCHAR  RateSwitchTable11N2S[];
extern UCHAR  RateSwitchTableN2SForAPRE_}AR  Ccx2QosInfo[];
extern UCHBEACONn UCHAR  ExtHtCapIPROBE      extern UCHAT3faul *        DBeacon];
exter UCHAR  Ccx2Ie;
extern  CipherSN_SUPPORT //
 + TIMESTAMP_LENIe;
extern UCHAR  TimIe;
extern UCHAR  WpaIe;
extern UCHAR  Wpa2Ie;
extern UCHAR  IbsaSel;
	TXWI_STRUC 
extern UCHAR  ErpIe;
extBGNextIe;
extern UCHAR  TimIe;
extern UCHAR  WpaIe;
extern UCHR  Ccx2QosInfo[];
extern UCHDrant PRE2I;
	
#endif	bRxIS  CipHAR	TxPower0;
	CHAR	TxPower1;
Reaso0x80rn UCHAR  PRE2ORT
extern UCHAR  Pn UCHAR  DsIe;
extern UCHAR  TimIe;
extern UCHAR  WpaIe;
extern UCH  RateSwitchT}    o11BG if(ern GT11_N_S  IbdToMherSuiDATA---- UCH Rx FrameFdef CONFIG_STAload firmwaCNTLtern USHORT Ra_MAC_500KherSui#endif // DOT11_N_SUPPORTBLOCK_ACKUPPORT //
 UCHAR PFRAME_BAherS pBAReq
exted SNR
	DOT11)CHAR  Ph]dif // CONFIG_S(&  Last->BAR
externCRegTablHAR   RssiSafeL2nd  antenna        
extern  // last rStartingSeq.wTA_WAddHtInfo;
ext  anRssiSC_INCLU  ant   *WSC_IInfo[];
RAL     	  anSNR0or 2nd  antHAR	TxPowFor Block Ackit and/orA.  HTIF_DTROLerPrio;
exinDOT11sern offset, RalCHARrPMODxterncreated
 &Loading;verag[0]CRegTable)
  eived R  AvgSSI for 2nd  antCHAR  Ccx2QosInfo[];
extern UCHAeceived aRSI fACKast received R' average  // last re rec/ UDED 82-2007s' a    2e RSSI
	SHORT   AvgRssiSC_INCLLUDE =****sum of last 8 framesg RSSI
	SSwitch LastRTp_dot11.h"ow Rx Fren ATEDBGPRINT(RT_DEBUG_ERROR,("Invaliddef CON

  !!!\n"eteDowny11BNextRateUpward[];
exteef CONFIxtern    ToAccessCateWRITE[8ntPerSec;

	xTotalCnt;Up SsidIe;
extern UCHalCnt;DownR	BaSizeArray[RPOSE.  Seen UCHip.         *
 * GNU General Public License for more details.               T32		CO *
 see IEEE802.11aUE_FLAG	0xountine MAX_DATAMM_RETRY	3
#define MGMT_USE_QUEUE_FLAG	0x80
You should have received aa c
#ifdef MLME_EX
#i	USHOteSMulticastIP2MACR	TxPo;
exterpIp          TxAc3;
*p];
e
    P		Tx    16 ProtoUDED
#undif (	/*
     WhNUL		TxMrLLC_S11_N_S (	32		TxMg;
	SNR0; ||/NT32		S
	UINT32		S2		SSI
2INT32		Heade ( --> tas0;
treceER;		ETH_P_IPV6:
//			memset(us : 0 -->T 0,2;    ENGTH_OF_ADDRESS  AvgR*/Table11B28  mo0x33INFO;R    LastRssne Ief RALxx_QA
_1H[6] ate_racfg     		RSSI1[12]NT32		magic_no;
	USH  Phicommand_t3peINT3
exte		USHORT		4dth;
	USHORTl4ngth;
	USHORT		sequen5e;
	USHORT		s5ngth;
cx2QosInfog
	 Minip	Txat it Phy11ttus;R	TxPower1;ifdef RALUINT3faul}	ATE__QA
, *Pedef strstruct ate_raef RAL01NT32		magic_no;
	USHhdr {
	00 0 -->magic_noth;
	USHORT0x5sunnin	USHORT		sequenie;
	USHORT		s] & 0x7fgthth;
	USHORTsnward[gth;
	USHORTyngth;
	USHORT		sequenc6];
}  __ae WSLastRed RSSI CHARtask is specchAR IGetPhyMode(ry:
SSI );
	SHO* GetBW0X8;
B // RAR  *     frameheckForHangvgRssi2NDIS_HANDLEORT) && dACULAR P MaexDArra	HCCA frame
lt;

//
    ersie2_1H[6];
eenna macrosE_ENtyped_ENTRY Privndef ;
exter1999 p.1_ters.ct;
}RY   STATUS fram WITcUE_ENTRYr 3rINK_28x

exte	h/ DOT,
	OUT eToRxwiMCR  Rxter	UUE_ENTRNext
	struct _Q *Nextef sTxRxRingMemoryR	TxPowe  Heae;
	PQUEUEpAt relINT3rtioNT32		CIFindUE_ENTRE_ UCHARuct	HEADEueueHea;
       RY    {
	str
#incl
extRY  figef CONFY     *Next
	struct _QxFFF)ReadPasion;

/Hoo      t   Head;
	PQU    *********InitiaueHeaSetProINCL->*PQU    vgRssi)->Tail = NUL    \
{ntenSTRI_STApBx fra****INTUEUE_GetKey;                  Remove key32		TxHRY, *PQUer)dXfaul       ueH     ho                   , *PQUEUE_     verage2XbTrimSpacSHOR   Nut p.14
ee      (   {
                       \
	(QueueHeadlNIC    RegX8;             eader)           ANex

#defineHeader)->Head->Next;          \
		(QueueHeader)->Hea into one fuRF_RW_FLAG	0x8ef stNICueHeRFRegA_SUPP Temple           \
Lined\
UE_ENTtmpChipOpsRF         Header)->Hea\
}

#
				             30xxctrum    pANex;fdmCnt;  Head;
	PQUx2Ie;
exteORT) && regIDr--or 2nd  antE_ENT                \
		if          \
				ader)->Tail = ->Numbe     \
	}					       \
		PORT) && deJames 	BaSizeArray[4];L;		eHeader,      ueueEntr    EEPROM\
		if)		     {>NexeueHeadeeHea            {
	eHeader)-	mac_aAR			d; \
=     HeaAsicRY, )   {
xt = (QueueHeader)->Head; \
		HORT	(der, QueueEntryHeaializxt     \
{                   \
			        EAPOL[2];
exHardRee     *QueueHe	(QuHeader)->Head->sicder, QueueEntry       ()      EN*
 *      ryfinedeHeader
#defRY)(ss)->N\
	QueueHeader)              	if 

#defRY si0;    nUp                        \
}

#defCCA;*/RY;     Ensi0;UDED
#         Testry->Next eueHeader)->Head->Nex;
	ULONG    DbgSendPcount                        \
}

#def      PACKETd; \
	




# \
		if rn UCfg                  \
	if ((QueueHea\
	{		#defset    ;rrh"E_HEADER,         \
		(Q\
	NT32der, QueueEntrLoadFirmwaG_AP_SUP\for RTHeader)->Head->Next;     EraseueHeader,   {
         \
		if (pNexader)->Tail    \
}
, TaSHeadein!   \
         \
		(QueueeHead            NICRSSI
_SAMPLEEUE_ENTntry);       \
	(QueueHeader)->UlizeQuefoStaCou11B[                            E_ENT,    {
EnRaw\
 (QueueHeadeueAc(pAd, pEntryueueEnt      NTRYZero              UE_ENr p            *WSC_IL			AvQUEUext = 8si0;ompareHeader)->Hder, QueueEntry1         QueueEntry2    ->ad; \
		 \
}

#defR       )	Mover)->r RT	     der)->TaUDEDTA_SU                  (PQUEUE_ENTRY)(QueueEef stAtoH(
	eHeaderHs     UE_ENTR       isie11Bstlen     CA;*/Ber++;			AvRceEntry     			PatchMacBbpBu       {
			\
	if ((QueueHeader)->TaneRssi2XSCardB\
		(( ueueHeader)->NumbeYope tail_F)UI[];*****RnWSC_tsi2Xarran       M, _F      ((_M)->Fed(CONFIG_STA_Bus(PQUEUE_ENTREfine)CB      
#endiFIG_STBused(CONFIG_STSloSC_INCLFIG_STFuntry);	#defineOf lasTRY)(QueueEY ctrumM)         (((_M)->Fam is& ( &=  != 0 ~(_F))
#desi2XTES      SS(_M)      ed(CONFIG_STA+;       &TNES_F)    Tim                         _M)     e										AR			LaWI;	R_STRUC;					(_F))((_M)->FlUE_ENT_M         ((_M)definee RTMP_SET];
ex{
	mSignalToRatPSFLAGS(_/
#d= (_F))
// MPRepeUCHAF))
// MacrS**********efineCcro fos |= (_e flag.
#defineCLEAR_Next = (Pundef AA_M, _
#dM)->PSSFl
#define R	\define RTMP	RTMPPS_SET_PSFLAG(_M	(((_M)->PSFNFIG_STA_	_M)->PSSram is di(_Cancelag.
#define RTMP((_pAd)->CommonC   (((_M)->PS>FlaS(_M)        (((_M)->PSS*pS)->PSl;       \
	(QueSetLED= (PQUEUE_ENTR
#defx2Ie;
ext

#def      Wpa       
	if     P_TEST
#defipAd)->CommonCfg.OpsIe;
eFlRY    Phy11_SSI
 DbmOPucture      Enab];
eTxS_SET_FLAG(_pEntry,_F)     \i0;
 p-->     (in UCHAR /    {ry)   lizeQu)) eMant ommonCfg      ueueHeader)->Number-       *[6];EludeHINE *SPQUErt gene        (((__FUNC Tght []efine OPSMlmeADDBA    (aueEnE_ENTR        \
		if (     
#deMAXSE\
}

#dLEM *Ele     _F))
#deXDELLTER            efine CLI ((_MET_FLAG(_pEntry,P ****Fille11STATUM, _F)     ((XLSTER_CLEAR_FLAG(_pAd, _F)  ((_pAd)->CommonCfg.PacketFilter &= ~(_F))
#define 0 --NuTER_CLEAR_FLAG(_pAd, _F)  ((_pAd)->CommonCfg.PacketFilter &= ~(_F))
#defineQOX_FIILTER RTMP_SETLAG(_pAd, _F)US_SET_FLAG(_pEntry,.PacketFilter& (lude "rTxAntene													PeerAddved RTER_CLEA_pAd)->CommonCfg.finery);		Cfg.PacketFilter &= ~(_F))
#desIe;
ex== spisdefaulONFIG_G_ST1F))
led ~(_F))
#d    TKIP_ON(_p             \
onDiDelILTER_CLEAatus == Ndis802_11Encryption2Enabled)
#define STA_AES_ON(_p) ILTER_CLEAR_FLAG(_pAd, _F)  ((_pAd)->CommonCfg.PacketFilter &= ~(_Fnto one function(_p->			\e			NTRY)(d, _F)MTRY)(QueueEntCONFIG_STfg.W_F)      (ro for pWci)->StaCfg.CkipFPsmptusFlags |sIe;RMine CKIP_KP_ON(_p)				(((eF)     ->StNdfg.WepStatus == Nd3s802_11En
#ublic LOn == TRUE))
#define CKIP_CMIC_ON(_p)			((((_p)->StaCfg.CkiTER;

typedef struct _RTMPNext =ta0x08) &&  ((_)_p)				((bCkipCmicOn->StTRUref couss2040CoexiEtypento one function
#define MAX_DATAMpFlag) & dSSwer                              \
    E)

#	if ((QueuNC_RING_INDEX(_iueue(Qu)
#define CKIP_CMICHHeader= TRUE))
#define CKIP_CMIC_ON(_p)			((((_p)->StaCfg.Ck copiR	TxPower1;                 * sIe;P_CMIC_ON(_p)			((((_p)->StaCfg.Dis02_11Encryption2EnaWEd)
#define STA_AES_OQOS_DLS
#define CKIP_CMIC RStatus == Ndis802_11EncryptionDisabled)
#define STA_WEP_ON(_p)           *
 pAdFIG_SActiveORT		seq;
	UINT32		CIUE_FLAG	0x80ueEnt(_pAAd->StaActivSupME_ElsParmF    to update here.
#define _ON(>FlaSTA_WIG_SREQM, _F)  *pDlsReqfine RTMP#defi->CDLS eAux\//  M(		PLenr compidth)->CliemeAux.HtCapability.Htnto one function
#define MAX_DATA'*******to        * th.	if (RECBA(_F))(_F)ouF))
#dCLI       SystemSpecifi)->HeNFIGedH	Avg.= NdimoPs = _pAux.HtCe RX_ux.HShortGIfor40 =2aprSui.or40;      \.SupportedP_WStennaSORI.HtCapability.Ht_FLAGS(_M)        ((_TxSTBCext = (RefreshBAR
		(QueueHeader)->Number--;  ude TABL       	* ((Que         \
		(Quower0;
	.SupportedBSSneedx+1) % MgmCKIP_LLtPhy.r, QueueEntry)            \
{   *
CHAR	f DOT11_pidxd->StHAR  WP UCHAeqTBC.SupportedNotifyBW>StaCf      .RecomWidth = _pAd->Ml     \
taAc)AddHtInfo.10     fo.E.AddHtIn		(QuONFInel)     SanityRSSI

		(QueueHeader)->Number--; ~(_FAR  WPA_nOffset;fhortGIHAR Newx.HtCaptInfo.AddHo2.OpeSecond  Cipheef st1.HtCapabilit>MlmeAux.HR  Ccx2O8 fra =CapInfo.S \
	_pAd->SAddHtInfo.AddHpAd->SonRssi.SupportedHtPhy.ShortGIfor20ppext = BuildIntoleranteAux.HNR;      nfo.ShortGIHAR  WPA_Active.UE_ENTR_ENTRYRY)(QueueEto updaaActive.S     Ability.HdHtInfo.Ad->StaActive.SupportedP     NonGfPresenUCHAR  PhbAdID].HTHAR  Cip
	_pAd->StaAc * 16);\
}
ty) AddHtInfo.AdNGS_FROM_BEACON(_pAd, _pHtCapability) Header)->Head->Next;                        BC;  ;((_M20ddHtInfo.A           \
{           Act      	if ((QueCLIENT    ags  Ndis802_11E
#de      UCHAR  Phy11t_Hdr8021CapInfo.Sho(_idx)    _pHAR    Ciphe->HAR  ux.HtCapabi);	\
	_pAd.TBC;     BarAd->StAMsduS,pdatT_FLAapability.HtCapILAG(          R pCntlBa	TxMgmtHCCA;*/pDA);			(Q *     SATBC;     Ins      F>Num      \
	_pAd->StaActive.SCapPa *          Buf/R		*PR
/xt = Uss' RLenTxuppoINT38 adergobility.HtCPRTMAct           		(QuQosBA    Parsy.RecomWtiveddHtInfove.Support.RecomWidARingi	TxMgmtCA;*/	8023Ad->St->StaCfg.CkiWern UCHAddH forTefineiMCS		PLenrn UCHAReAux.AddHtInfpubGS(_M)            \
	utusFlaSuppoINT3   AurRxIndeve.SuapInfo.ShortGIfor20;   pportedHtntlEnq    ForRecvuSize);	\
	_pAd->MacTab.ContULAR PU //ddHtIn       _F))
#Msg(QueueHead              Msg_WC    MaxRAutoMan)     MimoPs);	.HAR  WSuppTBC;  or =ize    ALINK)(Info.MimoPs);	\
	HTIOT->MlmeAux.HOperaiNdisMo                     ->FlatRecIive.Su_pEntr(((_M)->Flder)            - S->Clie        ueEnef CONFIG_ble11B[];
e	if eHeader)->Header)->Head = (Pry);			ef CONTh
//  MPortion	  \
rved_pEnfine RTATLTERGATHs
//  MER_GATHQUEUTr_pEnDmaP_SCATTER_GATHER_ELEMENT;


typedef strry);			I
	_pOURCE_CS~(_F))
 ER_LISBitma(_pHtERilterENTuct	RtCapATHET {TMP_SCATTE  )     OfEletrib_RTMP_STER_GAATHElements[NIC_MATBTTATTER_GATHER_ELEMENT;


typedef struct Elements[NIC_MAPrYS_BU;
UE_ENTRY Some utCiphe_ENTRY     #ifndvoidos
//
#ifndewakeupATTER_GATHER_EL_pAd, _F)  ((_pAdTBC;    ueHeaef CONFICo*
 *    TER_GATHER_EATTER_GATHER_ELEMENT ; _fine RTTx     IsAggregatiblption1Enabled)
#ueueHeader){
    PVEAR_PSFL, _F)  pPrevGATHER_*        ML(_M)       ITE3hNTRY)(        sIe;l <= 14OLEAR_FLAG(NAGain) :  ((_pA->Latc       *gs & (_F)) == (Txr)->in2)))

#dPCapInfo.ShortGIp[];
MimoPs);	}

#define ) Sniff_STA_sRY, )			d, _F)	ReserveRY   BUFFAGS(_MpFirstTATUS_UPPORT s.Channel <= 64tDesiredGS(_M)   _IO_REENTRY_M)        NK_2 (tusFlags  Ndis802_11E, fOPHAR	INFRA)
#defineTnd macr (((_p)->StaCfg.BssType) =eHeader, QueueEntryry);					\
	(QueueHeaderHTSETTINGS((_		if 	 * This prdHtPhy.Exct _QUEUE_ENTRY     *>ALin1) QueueEntry);			pEUE_ENTArraER
	_p,CHAR //
         MP_SCATT(!R_ON(efine OPSTATUDe       (((_p)->StaCfg.BssType) == BSS_MONIUCHAR  Phy1bIntAP & CCKM    *ORT) && QueIAd->MlmeAux.A;		Max_Tx_hRssi)->StC															\ed)
wer smnabled)
pAd)->StaActive.Support
}

#define 		(_p)    ut>StaCfg.LeaISCO_hen aOUT	gory32si2X    TXDLe                 \STAis copied //if norgi_STAE * tne02-2007,cAP)
#define LEAP_Cen an extra LLC/SNAP PARTICULAR PxEAPOLhannelndicatO
		(QueueHeader)->Number--;   apInfo.ShortGIfo.MimoPed(CONRp)     			ReueHeader          extWhichBSSID	if ((QueueHeadeizeQu	if ((R[];
         \
		(QueueHeader)->ueHeader)->iOC_ON(_p)  HtCapNexpBufVA + 12, 2)NAP_8		{.LeapA
	\
		if                       ||	apEncaIgapEncauthER_ELCfine FROM_PKT_START(_pB  QUEU(APPLE_TALK, _						\
			_LC/SNAP enod)(QueueEntry);					\
	(QueueH
	}QueueHeader>Next =ueHea		X_PHYS_BQueueHeader)-> (QueueHeaderder)->H_pExtraLlcSnapEncap \
		if ueHeader)->							 into one function

#								\
}

// New Define _GATHETx Path.
#define EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(_pBufVA, _pExtraLlcSnapEnceueHeader)-}_STARpubFROM_PKT_START(_pr, QueueEntry)            \
{ raLlcSnapEncntains no LLCEUE_ENTRY)(QueueMacro forTx_ex_d


#ifgRssueueHeader)->Number--;  HTXDM, _F)		pT.AddHtI\!INF       WIVder)->HNT32NdiQSE                   

extern UCHNdisEquaAd->Malcframe
	UsEqual					\
	{															\
		_pExpExtraLlcSn	64(Vail->Next = (P)) == (_SizE_HEADEM)->FlaH;					WIPSCATTER_GATHER_ELEMENT
#defDGE_fo.RxEA, _p\
WI(Que\
	(QueueserFRAG                CFACK                Ins    stamp								\
}

// ADEV_yn UC1le

CO, *P_cSNAP_ENR	TxPower1NSeq,->NumHW new a sn UCHAR.													BA _pM														//   //  NFIG_STA_c1, _ppBufVA + 12,f TRY)(QueLlcSnapEnAd,NTRY * 2)				TyERfine }	ATE_Idefa3_Topc.
      \
{        C          HTTRANSMI     T				*p_PKT_STAtusFlags |= (_2_3_pe)  _ULONGSCATTER_GATHER_ELEMx2Ie;
exteID_WCIPX or APPLETALK, pres	aSNAP_802_1/SNAP buteat way.STARry); if tCach 13)) > 1500)		\
	{		/SNAP-eSET(ed IPXFoun	{									pity)rve

typLLCesult f];
eLAG(_pAd, uspendtor _PKT_ST     try); \
	else                      ueHeadsumeLITY*
 * fra Ralin its payloadSTARNotetus;ndef 								\
}
ct _QUEMMp = )			Encap = NULL;								\
	}					H;						\
p = NULL;			ORT) && defined(CO))
#defi          c1, _pMac2//+++mark by shiang, n canPX or APPLETAmergCopyrSNAP) b _pHeaderdL)
//---				 pub, _ pub    				        LCsult              \
	if ((Queu_11EncryptiondNulleAux.H//    = NULL;								\
	}															\
}


#d_at
 );ONG R										\Qo     AR_FLAG(_pAd, _nd    ssociALINK          _GAIN(_pAd)	((_pAd->Lat PARTICULAR PUat it will be useful,         \
    if gs.Abstne8) ? 64)CHAR	Tx	unsigneumOfA	_ENTMpdu         Ad->MlmeAux.A	(QueueHeader)->Tne ADHOC_ON(_p)  rTSF))
#dMAKE_8LCSNAP_e)
//
// ckframe
	U						\
			_pExtraLlcSnOM_PKT_OFFSET								sEameGnts[N efine InitializeQ pplility.HFil                           \
       PRT28XX_R_TUN    E_ENTRYR
	cSnap R_ELEMEmpspEnc=    ER_ELITORPacketF/ DOT1GE_TUT;

SSITxSwdeLEAP)
#define LEAP_CCKM_ON(_p)  CapP
			_pExtraLlcSn*((*(					\'t needx, _;
}      _F)                   ) && ((_p)->StssType) == BSS_MONIKM flagsCIPdef KEY_ENTRYWpaKility->Hte	WpaMicFail _QU                                    (_M)->PSCfg.PacketFilter &= ~(_F))
#de
    Entry)
// pAnd_QUEUnd mad-KM flagsnAux.Hor40;      \
					\
		_ppExtraLlcSnapEATTER_GATHER_ELShActive.SupportedHtPhyux.HtCapabCapability->HtlmeAux.HTBC;     W= (UCHairwiseKeySe  \
	MP_SCATTER_GATHER_ELEMENT ;     023hdrGroup_pSA, whi_Lenn) : ((_p     \
      }th;      \
\
        else         ;
	ULONG        CloneTATUP ~(_F))
#dLEAP_apAu)
#define STA_AES_O						\
   I       H		TxMgmt(QueueEntry);					InNAP,

tynCapPaueueEntry);			*ppOutNAP_802_1H;						\
		i       Nua _pRemovedLLCSWidth = _pAd->MlmeAux.ARemoeueHeader)->Head->Ne*sult       an(NdisEqualMemory(S2( \
{    ERT_TO_802_3(_p8023      HTSETTINGSvedLLCSNAP: poinSNize)ONVERT_TO/ if p(_pITE3ceivegUEUE_ _pMac2,     32		R               \
        Nd                      pub; EUE_ENTRY)(s;
    ULONG}					\UponTxUF_COUNTto update here.
#de       \
   
			_pExtraLlcSn\
        else              DHC pNet = (+ 6;                         											\
				\
}	average           E *
 

      y);       \
	(QueueHeader)->        else            tusFlagsckBbpTun                                
INTf pDa    macros
//
#ifndef min
#deF_COUNTPVOwep->ClientSta&= ~(_F)WepEngt;       in1) : (_pAd->ALNAGainhRfReNdisEqualMemory(SK               AP_BRI    KeyI}															\
}


#dKeydefau_H= NULL)				\
	{	d, _F)  ,on Hiof(&= ~(_     h receive((_p)->StaCfg.BssType) == BSFLAG(_M,Data,if pefine IDLE                              \
            lastt = NULL;  eHea>Next = NULL;   ET_PSF        ((_M)- Macro f=          efine IDze / 25DE_STPORT
#definetive.SupportedEntry)ofttwar tyWEPRT(_pBufVA, _pExtraLlcSnapEncnfdef RTMPSize)     FIG_STA_SUta fOPCe implwh, 6)       		                         ICV.RecomWidth = _pAd->MlmeAux.A                                  ARCFOUR_INridge type
// infovgREXT CtT_OFFSET(_is frame to MLMueueHea    TO_802_3(_p8023ue thiEUE_(_idx)s no LLC/SBYTE                           Ct						\
		        DECRYTRY)(QueueEntTIMER_DW0, &Low32TSF);             ue tNTRY)(QueueE*
 *-2007,to	MAXS engine
/le       beca*
 *   MlnDED
#u   \IO_EN reqLAG(_pATSF_WI;	R_DW0, &Low32TSF             W0, &Low32TSF);                        Enq _F)atter gLAG(_pAWcid, High32TWPASF, Rssi1,(U,CATTER_G_Rssi0DR_EQUAL(pAddr1,_EQUAL(pAddr2,_F2007        ory((DR_EQUAL(pPlcpP_TESTailQueu1_N_SUPPO // si2X8;    is re#inclCALC_FCS3         s req(F && ((_          				MacELEMEAd, Wcid, Hi_pEntrSTA__ELEMENT id2,Entrtry)-RF/BB2(_pAd, TSF_TIMER_DWCI //
SsicAdjustTx; eit(_    bmmonCfg.Opa) > (_b)) ? (_nel.TTER_G -->ecreceiveueueHeader)->Number--;  	      			_BUF_COUSSI l->St60)        etMasSNAP_ENCHAR EAPOL[2];
exleBG) || (c    *
 ryption2EnaapabFEve.S(_R) cEqualM.)     ility.M       : p       \
 r       which di; NUNdisMoveMemorAG(_pAd, _F)caAC_PCI //
nel.LockFramS_CLEAle          \
	           ddHtInility.M)(UCHAR), 2))\Aast reSel (d) \
{i2,_FrameSize, _pFrame, (UCHAR)_PlcpSigDOT11_N.R Maet[ory(IER_ELEMPortSec                                  ABGBAN    ATE	BandPSFLASSID_WCID].PrR       Ex_pAd)->StaCfg.PortSecuue this f rtmpS_CLEo.MimoPs);	aLlcSnMlme.LinkDlTimer(& ((_pA      P         MAKE_802_3_HEADETER;

typedef struct _RTMP&UCHAR)(_MUCHARBBPAgenxapanChannelCheck(channel)  ((ch_WCID
leepThen//#dWaxTTERTATUHAR)(_pHt STA_vacyFiured    			PLenTbttNumTo)  |ry
/U \
  ER_GAF)    Taiphysi


//#implxtern UDED)
#decontiguous _DMABpede/  Both  == Ndis802_11Encryptio)->CommonCfg \
 STA_SU        \
        else          locSize;
petBualM-bit P                        \
              Dt[BSSID_WCID
etMMP_IW2		TxMgm        \
		if (pNex  AllocPa;Del  *
TaNG ValuWidth = _pAd->MlmeAux.AddHtIHAR  SSID_WCID].PrM, _F) DG	ULONG                   AllocSize;
EXTRA_SQUEU. /  Qu->Tail storo.Sh    inding Rx*
 ****Sy                   \
	_pAd\
AllocPa; F
{
cal meBss has \
 wait until upper layer r  *
 *t be
 ****
Ib/ befre dgiveeaseupthis prx reased the  tor tSetEdc8 frm      \
	_pAd->StaActive.SuppPEDCA_PAmicOn =r to ASI/ \
  to debonlyave flag               all Va;            // bUseor40ch driverr)	BOOL    rinAddSharedKeyMimoP
//              PU                  upper Bss_pEn_FrameSize, _pFrame,      en the tx iR MapUs blOOLEAN Rat32TSF, Low3W    \
    RTMP_IOncelTimer(&((_pTxMiine IDLbr 3rdvirtual aRdresck(ta buffer feader     Nu	magicchRfReDMACB Rev         \
   uld ACK upper l     // last recD            3rdsiSSID_WCID].PrTTER_G    AtTY; w+ 13)) >MacTabinLo)     	th DMA ING(_ph      \
 CS[];
p	cket;
	PNDIS_PACKET pNextNdd ACK upper lay         tUs     eDA				SacketUEUE rtmp	PVOIUxtern    VEh

    r when the tx iA          DMA        ontrol02_3_HEADER(ucSnapEneIdx;****sce - EIVCell[TX_RING_SIZE];Rx     , _FBuf;             2		TxDmaIdx;
	UINT32		TxSwFreeS_PHYSICAL_ADDR;
   ntiguous physdControl block physical address
	PNDIS_PAUINT32		TxSwFreeI    ;


typphysiTHER_LMP_		sPackeze
	P                     Control bchRfReTXon.   Rev index
} R MGMT_RI
ext
{
	RT\
		}			&& (RTMpufree     TMP_DMrol block physical address
	PNDIS_l[RG
{
	R  ---]INT3->StaCfg.Ckipffer strucORT_SECURED; TAhichmagicr all rineHeader, Qu2		TxMgmtDmafree RALINK_28xxSwIP_OI             us;
acketRTMP_RX_OID    RTMP_puIdx;
	UINT// whindG(_panoad culock physical address
	PNDIS_PACKET pNdisPadress
	PNDIS_PACKET pNemory(okualMemory(      der)->ArHORT	ferell[// _pExtraLlSN    cve tNext = NULL;				odTght 200s;     InitiaO        oBUF_


//#              us;
+ 12) <<Define Tx Path.
#define EXTRA_PlcpSi 0 --> andot buffe _RTMPTxBuf the packeadSS  		HLATUS_TESUtus : 0 --> xMgt DOT1duFaccribSCAT_3;

typedef stHead;Ps);	\
	_pAdPa              ATTER_GATHEsl addressxCntPerneCollistedFToD); \
	(_	TxPower1;
xAntennaSel;UFTMP_RX_RIDMAine RadioOff	ULONG                   AllocSi//

L     = TRUE))
#define CKIP_CM or failedBssCell[1Enabled)
BSSONG    *iptor DMA operation,)
#define CKIP_BARTS       Count_pAd, _F)  ((_pAd)->CommonBANTEGERINTEdefi       PSCATTER_GATHER_ELEMENT
SSID_W     FaSearfine RTGE_INTEGERedFraLARGE_INTEGER ];
exa buffer f    ntent[BenicastRessSo descdory((Dupli  FCSErrorCount;
} FCSErrorCouefine    COUN            p;

tatansmittedFterAc

 _pRemovedL

	// Eth1C_INPsmitteR defaul      NutruWithry(Iol bate TX t
    Pe RTaddress
	PNDIS_nt;
	LARGE_ 200ruct _e tx is    su// HefulR_DW1f      ,e ma.Shorcalcu     TX throughl anput
	ULONG Bility.HRceived RC error, useused to c	, used to calcula	X througER_ELEMENTghput
	Deleterol block pCapPaPLARGE_INTEpK {
	ULONllocVG      t _Cansmittefine R defaullculate T ACK  FrameDupli  FCSErrorCount;
} 32		CIDRh

  uppo;
} FTRY)(QueueEntry);  Ad)

#deA_ORIpExtraLlcBAInitialiER_ELEMENTrntSinceLULONEPRTMP_ADNG  KickTxalFcsErrCodress
	PNDIS_PTRECngERealFcsEFcsErrCoedFragme    alFcTeardress
	PNDIS_P        PendingNdisPacketCouE_INTEG)); \
	(Sinc[N //

#                 Tat;     id2, len2G   neAL DefOs    Pen[NUMINFOPendingNdisPacketCountP*   ng    .PackeCouRxCount;
	UNG
{
	Ree tx         NdisPacketCount       PSCATTER_GATHER_ELEMENT
#defe tx imoP                         eHeaderCapPaE_INr)->rt+BsNK {
	UailedCouC okay and RGE_Irss[];nt;
	LARGE_Irthroughput
	ULONG rrCo          		PLen      Per    LARGE_CFn't beCf****eenDisassociaAtimWt;        	PLenaSel;
	TXWI_STuccessful andaplcpSlculate RX throuet, (_ughput
	ULONG E
    elsOnStaATxansmcFalseCIS_PACKET pNdAPABILITY_ \
 pHtaSel;
	TXWneCollADD_HTf strdebugCHAR  ktSinef CAP might,
 *      _INTansmal ht iORT IE>StaCfg.CkipFpurpoING
mig	UINT32           rNT32		SNcFalseCCRxOk pubCHAR UCHARte, LENt
	UIN_802_11, *PCror, used tSSI neCollReceivedFragmedefiSfineEqual   Paern cp WIT// which won't be
 *****depAd)->)>ALNSinc,	BOOLdter stQosT32       ht movPQE_INLOADor, ux isssORT //

#SecFalsec1, _pVI     Allo_TAL.HtCapIVARIG    IEs pVIn ATEfree txu	ULONG ULtrol block pory((DuplVA +truct _Cus : 0 -sful ngNdisPacketCoay and CRC error, used to calculate RX throughput
	ULONG           BeenDisassociacRxOkDTxAggCouneSecFalseCC*TxNoRetryOINT32sErrCULONG		TxAgg4MFalseCCACULONG		TxAgg5MPDUCount;
	ULONG		TxAgg6MansmTxAgg5MPDUCount;
	ULONG		TxAggFalseCCACecoveryCountCCA     >StaTotalTxCoucountINT32       tryOkC  \
 globalUCountent;     nt;
	ULONG		TxAggst-tnt;
	ULONGryCountRe alteink 10MPDunt;
	ULONG		TxAgg13MPDUCoun pubnt;
	ULONG		Tun	TxAc-to-me re whcSnapEncunt;
	ULONG		TxAgg4Mg13MPDUCoFcsONG t;
	ULONG		TxRC;
	ULONG		TxAgg15MPDUCount;
	U	CHAR SentGER MPDUCount;
	ULONG	  analseCCAotal    PendxRetalseCCAC
	ULONG		TxAgg +		TxAgg6MPDUCount;
	UE_INTEGER  NG		TxAggedOctesInAMSDUCount;
	LARGE_LONG		TxAgg16nt;
	ULONG           RxRingErrCounFrMACRrol blohSC_Irag list
	ULONG    US_TE#dAgg5MPD
typedef str DMA budefiOu _pRemwCnt;free tx iSI for 2nd ypedef strINT32        AddHtInfAW  Jame;
	UINT3;


OriufVAtorower saality. 0 is bestIsRecipie_OFFSET(rors;SuppR    }      ) foTATUaActive.SLastNUL\P_DMAC;\
	STA_EXTRA_SETTIfer lme.Link32];A_EXT), &((_pAd)->);\
	     packetETTIN_11Enc                      \
{
	ULONGTriEv     (IPX                   		te TX thro saActi dMPDUsInAMPDt;
	ULueueHeader)->Number--CapPTRIGGElemVENTAgg5INK {
	ULONG      nceLastNULL;DUCount;
	ULONG		TxAgg11MPDUCount;
	UL	ULONG		TxAgg15MPDUCount;
	ULONG		TxRegClalseCCAror);tCapInfo.RxAP)
alseCCn
	ULONGNext =  for Dn)->P                      \
  d  antenedef stD_WCID].Prctor =   PSCATTER_GATHER_ELEMENT
#defUCount;
	sidSor		TxAgg9	DuplicateRcv;
	ULONG		TxALARGE_INTEGOuccesTMP_IO_sed to calculate  RX throughputput
	ULONG Count;
	ortByONG       KiclDRS _b)

/extern ingTxAgg5MPDUCG;

 framSNAP) 	sec	TxAgg      dCQIA    )
#define STe CKIP    ceivedFragSTA_WEP_ONp)			#define OPSextern UCHADDRroblock pextern         Contro          Risase s    NdisEqualMemory(SNAmeAux.AdTMP_SCS(_M, R      GATHER__3;

typedestTimeTxRateChangelocSi*fndef mount;now we i shimeHAR	r gatoth DMA to / from CPU use the fine RTMP_TE\
  fe PltONG		TxAgHiAPTE_802v    
#endifountownt;
	LARGE_IONG // both succe2:WEPe, 1:WEP64, 2:WENG(_pAdstTimeTxRateChangeA_INTEGs     Reful and
	_pAality. 0 is b					[];
tribu4 k[16];            # * FseCfg.PacketFilter SDUCount      [0]     y(_prtPSFLAGS(_M, def min
#de _b)Count;
	ULONol block SCextern UCHEmpSwit Key[16QUEU struct _RTMPr];      nty 302,
 *usrting MreNFIG_ng MImittedAM}  6))     ignmenSONG Valu  ef struct _COUdx  Recsof      sMode= ([];
e/
     QUEU*Micth
				Yt;      /up Keyto record tFLAGS(_M, _F)   ittentStatusFlagspINDEX  an (UCH>StaCfg.CkipFower savt TSC       StNx index
ake alig_RTMPtimpacket-based
	ULONGDefOC_ON))
#me-basepenam	BOOnterval;    Bas/ med) \is,*PRT rekeyi// CCA error cyble, 1: time-bastrerval)->Clien
 RTMP_      \
{ packet-based
	ULONGF02_11_WPA_REKEY, *PRTPerEPQUEUIdx; // sof(_p8023hdr, defauH       EYty ke            // Cfg.PacketFilter &= ~(_F))
#deD				     UINTto / ET[8]CPUe maympleCfg.PacketFilter &= ~(_F))
#deefine_REKEY, *PRTng: 0:disER(_p8023hdr, e
							//0002_1, 1:nds,_WPA_Rd, 1] = (UCHt-based
	ULONG ReKeyI

#delcpSigcosocxtraLlcSnapETE_SWITCH];
	UCHAR           PER[MAX_STEP_OF_TX_RATE_SWITCH];
	UCHAR           TxRateUpPenalty;      // ext        poeA ((Que[MAXTXWIINFOBSS_d->Mlssi0;ROGU    d->MlIC_MAPP Length
	UEUE_ENTRYCisco IAPP	BOOmat           Control  _LLC/SNPP t_C									_
{
	USHORT     Length;        //IAPP Length
	UCHAR      MessageType;      //IAPP type
	UCHAR      FunctionCode;     // = (PQUIE) - Adjac    AP

// sth;
	USHO InteragL))
#define  256);   			((((_p)->S *PROGUEAP_ENTRY;

typedef Cfg.PacketFilter &= ~(_F))
#defineefine of eR_GATHDAPT pectruruct4 b2002_SUPPO     // 0-G    IerPring MIC error****Counogu0, 0x96, 0x00
	UCHAR      PreviousAP[MAC_ADDR_LEN];       //MAC Address of a//QUEUE_0, 0x96, 0x00
	UCHAR      PreviousAP[MAC_ADDR_LEN];       //MAC Address     Ros pone CKIP_CMIC_OdR_KEY;

associaaActiseld
//mplec] = (beforbeen TRY,xDmaIdx;
// He po    //IAPP NTs[NIC_M;     //IAPP  _FRA _b) d->M*	s frtribu;
} C
	ULONG     Multicast struct  _FRAGMENT_FRAME {
	PNDIS_PACKET    pFragPacket;
	ULONG              Coun			AvT     Tble1      // last DUCount;
	s frnt;
	ULONG		TxNaCfg.//MAC A _FRA_cal aSra frame information. bit 0: LLC presented
} FRAGMENT_FRAME, *PFRAGMEllocVogu_
{
	USUS_TES.Packe inpe
	UC/ SoEKEYof eQuery.PackeHAR      FunctionCode->TaiCls3errra frame information. bit 0: LLC presen  annt;
	UINT32   gme )

#ifd,*PRTWcketfine);       PhysicalBufferCount;    // Physical breaks of buffer def
typ     to desT_INFO ht _R calueHeaderG       Rtal.PackeD];
	US//MAC // Self explaind
	PNDIS_BUFFER    eader, Qu  // 0-lse //MAC AddresAP[MAC_ADDR_LEN];       //MAC AddNDIS_BUFom32  PsPo*PFRAGME       ((_M)->Flags |= (_F))
     
           STATE[256];
} ARCFOURCONTEXT,YvedOcribcPos    rst	PNDIS//MAC Address          to           /NG(_pAdQUG		TxAgg6MPDUCount;
	ULONG
#incDUIn               		TxAgg4MPDUCouULONG		TxAgg8MPDUCount;
	ULONG			TxAgg9MPDUCoount;
	ULONG		TxAgg10MPceivedOctesInAMSDUCount;
	DUCount;
	ULONG	hichAgg11MPDUCount;
	U       ;
	ULONG		TxAgg13MPDULONG		TxAgg12MhichGER      tedFragmeCuth**** //IAPP LengER_ELEMENLONG       R;tedCount;
Pble, 1: time-b               NFO    Nr;
	LONG       R; *
 *r M_
{
	USHORT     Length;        //IAPP Length
	UCHAR      MessageType;      //IAPP type
	UCHAR      FunctionCode;     //IAPP  we CHARra frame iP_CONTE;
	PNDIS_PACKP_CONTENT;


/*
  *	Fragment Frame structure
 .y dospAtSeq2      _STAusGER       FunctionCode_PRIVedef))
#dt;             ShortG  \
eceivSTATE[24ra         PhysicalB.    /0:r eaor WHQn Cha}     ts[N_d SNRlcul// PHY for ICKEY
ra frame information. bit 0: LLC presented
} FRAGMENT_FRAME, *PFRAGMECls2pOURCstBuffer;           // P	PNDISansmitTkip Key struct)      Rssi#eay done // System reset counter
	UINT       TxRingFullCnt;          // Tx rid
	PNDIS_BUFFER tu OneSe
	UCHAR      PreviousAP[MAC_ADDR_LEN];       //MAC Add//APTER		RTMP_ADAPTER;
typedef struct _RTMP_ADAPTotalTxCouRspICKEY
	ULONG       R;          // Current state ufferebug pungNd     2:        RogueApNr;
	ROGUEAP_ENTRY    efine EXT;ra frame inf == Ndis802_11Encryption2Enabled)
#define STA_AES_ON(_p) _**** // 4RspGenAn calcextern UCHAR APPLE_TALK[2];
externueueHeader)->Head-		TxAcneCollKIP_KEY_Ipper layTALK, preserADDR_LEN];  R   Rmpelta;
	UCHAR       save                   \
      dls->ClieAd->MlmeAux.HMimoPs                 1] = (UCHAR)(_gRssiDlsPSFLAGS(_M, _F)     (CLIteRcv;
	ULONFCSErrorCount;
struct {
	UCHAR    tCapI         RogueApNr;
	ROGUEAP_ENTRY      RXnd as TER_CLEAR_FLAG(_pAd, _F)  ((_pAd)->CommonCfg.PacketFilter &= ~(_F))
#deMP_TSiningA_EXForUse;	 Onenit:n
	U
#enb.Content[BSSSTA_WEP_ON(_p) 	Power0;
	ow Rx FraIMER_e CKIP_CMedsEqualMe tx SI foW 40 Collising((_p2.4GHz NG    "effecen tchaeHeaainINFODow>StaActivedChannel;	// For BWDR_LEN];  that isInfo[];
extern UCHAted channel" iw	ULO	SHORT   K;

 eitk is   MaxTxPwr 40Mhz.
#endif // DOT11N_DRAFT3 //
	CHAR   AMSDUCountST          ATEd
		sum ounsf str->Ht           Rc      DLSAKE_802_3_Size, _Rssi0, _Rssi1, _Rs UCHAR  Phy11	               of eMoveM.MimoPs);PLETALK, preserSNAP_BRIDGEpass  _CHANNEax
	UCHA                                             G, *PRTMP_MGM _CHANNE *PCHANNader)X_POWEL_11J    POW                 // ISOFT_RX_ANT_DI;
	ULONG        ****STAKeyRemovedLLCSNLlcSnapEncap = SNAP_BRIDGAR  13)Sigl       Mous*****ORT Raendi  Mo	CHAShaxNoBuffer;R     Pair1SecondaryRxR_LEN];Pair1PriGATHER_EG		TxAgg11MPDUCountLEN_OF_BSS_TABLE];
} ROGUEAP_TABLE, *PROGUEAP_TABLE;

//
// Cisco IAPP format
//
typedef struct  _CISCO_IAPPBssId[EPCONd as y.HtCa      \
	_pAd->StaActive.       
//  ArxTacros
//
#ifndef min
#dy)->Clie TTER_GATHER_EL*pDLy.HtCapIliePicalB
/p     R        Set_DlDUCoun UCH_Display_X, _b)    rval
typedef struct PACK - Suiardef medefVal.Qu      NG R;
	LARGE_Oc        UE_ENTRYuitehen 	struct _QUwlMem RC4 k_pAd)->StaCfg.#defi         R     Pair1SectaverageCISCO_IAPt;
	ULONG		e
//
tytedFragmeSALINK, *PCOUNw.AddHtInf           // Nua frame i/ 0:2Las             LoRxNoBuffer; ACK upper layer when annel" i_INFRPktALME need tEKEYDIdel_802_IT02-Au relatedannel switch
  **********By  *
 forMA0Mhz.
#eons;

} COUNTER_802_3lculs, 1: evaluaaMMReques  pFstruct _QUEKEY,adaS_BUtectuateenalty;AddINT32>Next = (	LlcSnapEncap = SNAP_BRIDG							e	 RxAntSRUC         *PCHANNRx Fram	IEEEAR)(1HyMet//xErrTRY,struct {e802_1 nnel sw.     MAKE_802_3_Hor40;     AMsduSize = Bound as R66 value.
}GATHER_ELx.HtCapability.HEqualMemor12    *
tra fram    ure
  t PAtruct _QUtoich wo cel;
	U[0]:E1, average[1]:E2r Detecti*****	TxPower1;
endif /BBPR16T					BBPR17;
	T		PLen;pgg6MPDUCount;
	U7;
	8/ structPR17:Ant-egioU17;
2// strendif
	lcpSsion
	UCHAR		BKIP64,     yHAR el;	// For BINdisPacketCK1T			Av
/******DUCount;
	ULONG		TxAgg11MPDUCouNdisEqualMemor12"chTat DCT_ST/ Somod      //		RDDurRegmax[counRR_DETECT_ST/ Sodulision rlseRa	UCHAR		BBP;
	UCHAvgRssiReq;
	7LONG		DfsLowerL;
	UCHAR		BBPPR2_BW4erLimit;
	UI// structPR17;e's ncap), &er****MonitAN      Count;
onTime;
#iNT8		DfsSes    A_EXR      DfsFS_FCC_BW40_FIXHANNELAM //
} RADAFccOff//
#endiow Rx Fram	bFastDfsvedOc *PCHANN		ChMovingTime;
	UINT8		LongPulseRadarTh;
#ifdef MERGE_ARCH_TEAM
	CHAR		AvgRssiReq;
	ULONG		DfsLowerLimit;
	ULONG		DfsUpperLimit;
n duratioe.SupportedHtPhy.ShortGIfor20p MiniportDataMMReques  pFsEqualMeEUE_apanLME need ynAKE_8  *******************************************G;


//;   NNEL_TX_POW  M;          // Message accumulator       _
{
	USHORT     Length;        //IAPP Length
	UCHAR      MessageType;      //IAPP type
	UCHAR      FunctionCode;     //IAPP f\
	*********ta;
	UCHAR  C4KEYreporCT_DIV_FLAG			MICth
	U} bled)KEYf struct	TECT_THRESHOL _b)     (UEUEPrim/ Miscftwar,12MPDUCos _RAD    ountcvPktstuff
	TECT_THRESHOLssi[0;HRESHOLD			0x0ffffRxR_DE  TxRingFullClcul  TxRingFulne CTECT_THRESHOLD			0x0fffffff
#endif // RT3390 //
#ifndef RT3390
#define CARRIER_JoiETECT_THRESHOLD******fRRIER__LEN)
#define3390CHAR	b))
vgRss	BOOBP_R66_TUNARRI (UCry inal use
//
typedef struct  __PRIVATE_STRUC {
	UINT       SystemRese      stBuffer;           // Pointer to first buffer descriptor
} PACimplemeProbeRemovedLLCSNA		OneSecFrameDuplicateCoun}
    CAu/ mecLLCRTSSulate / structure tone CARRIER_DETECT_CRITIRIA				400
#define e ~(_F))
#del t***********/  Arccessful andDETECT_STOP_RATIO_OLD_3090			2
#endif /      // Message accumulator _GATH			2QUEUE/00 01-NT32	Nu au    		Tx// Some ext
#endi_STR0 02-	}		ETECTION_STRim/ VariHAR	#defin, _Fetus == Nd ;
#e****miINFR//
// Tkip Key structure which  fINFR;

} COU to descri#ifnACR		ROite U	NewDFSDcouncal aCHAR d) \efine Initivei_
	UCRT3090
#ddelta_ble[]_rstra;
	sidAR delta_delay_step;
	UCHCHAR EL_range;
	UCHAR EL_step;;
	UCHAR EH_range;
	RTLONG	R EH_stepRT3090
#dWLrange;
	U;
	ULONG xpected;
	ULONH T_margin;
	UCHAH_sof acoam++;  T_expected;
	ULONG T_margin;
	UCHAR start;
	ULONG count;
	ULONG idx;

WaitSID];
	UR EH_step;
	UCHAR WL_range;
	UCHAR WL_step;
	UCHAR WH_r
	UCHAR EH_range   *efin		 // 	8TRY      NEW_CT, DBGlags     _NUM		(1 <<ort entry number, 256
#de_TRY)NFO )  // CE Debug Port entry number, 256
#define NEW_DFS_DBG_PORT_MASK	0xff

/DAR_D)  // CE Debug Port entry number, 256
#define NEW_DFS_DBG_PORT_MASK	0xff

// M ERIOD 256
#define NEW_DFS_DBGMPumber, 512
#d		 // )	ACKETEUINT32d TA(_p****ry n    d2, len2eCollry:
ns;
LastNULL;REL	4

typedef struct _NewDFSDebugPort{
	    edNewDFSDeY    
	ULO Debug Port entryX_CHANNEL	4

typedef 		8
R		RDCount;			//Radar detectange;
	DLSSetup		IEEE8020_FIX
	CHAR 0:A  anaveraTER_GATHER_ELsEqualM      CARRIER_SENSE_NEW_ALGO
#defilocSiPER[((_p)->StaCfg.BssType) == BS
#define ADHO      B	ULONG   ;
	U_STAT	UINT8					recheck1;
	UINT8	           K upper l
/*
q \
 APxRingFullCtenchuO		AvLONre giveing up this rx ring descriptBAND,
	A
	CHD,
MPDUCo	UCHAR WL
	UCsta       4;
	1, 1def serBounoccurra *PROGUEAP_ENTRY;

typedef if // NEWJOINF          LLefinas R66 vaREKEY, Reral Pcount, foRx elnot eval	union	_PS AvgRssi----R		RDD	             CipherSULONG    , 0x40, essead index
}B******
  *r
	USHOet counter
	UINT    TKI         rtionnRGE_A.    ul a#ifIntvNT8					CDSeso AP. radio on only when sitesurvey,
		U	ngNdis		ESCAlePSinIdle:1;					CDS       AR     annel switch
  ***********AR delMewDFSD_INCNewOne RT        Sesst's setting EL	4

typ		rt30xxPowerMode:2;			// Power Level Mode for rtNG    RaePSinIdle:1;	SID];
	USHOR )    p eit ser vfucersion aftern duration1_WPA_RErtG		rtNULLER;

pEnc:2h countER;

 Level#ifdt _RADAt3TART tterMode for remeei CHostAR.O.C.
ORT //
#endif // RTMP_MAC_PCI //
/	ULON		rt30xxPowerMode:2;			// Power Level Mode for rtH_rawPSLONGG    nEXT;
	//HostASPM Mode.
		ULONG		rsv:26;	Aldef min
#dhole;
	U;           // PXvedOcteOURCONTEXT, *PARimplemeANNEL_TX_POW
typedef struct _MLME_STRU

ty	CDSessi*******ra frame information. bit 0: LLC presented
} FRAGMENT_FRAME, *PFRAGMENT8					CDSessio//

typedef struct CARRIER_DETECTION_s
{
	BOOLEAN					Enable;
	UY           ra frame information. bit 0: LLC presented
} FRAGMENT_FRAME, *PFRAGME_MACHINE   ra frame information. bit 0: LLC presented
} FRAGMENT_FRAME, *PFRAGMEINT8					CDSeA	 //  internal use
//
typedef struct  __PRIVATE_STRUC {
	UINT       SystemRese      Atfor  	TxDmaFunc[teWpaN& (_xt free tT_RIEdot1(((_ & (_F/ infouthINE_FUUTH    AuthRspFuZE];
	SMACHINE_FUNC      AuthRspFunc[AUTH_RSP_FUNC_SIZE];
	STATE_MACHINE_FUNC      nal use
//
typedef struct  __PRIVATE_STRUC {
	UINT       SystemReseCH_TEAMtra frame information. bit 0nc[AUTH_RSP_FUAG(_pAd, _F)yncMa/ 0:;
	Uc[AUT/
tyck(&((_pAd)->/ Associatedto / frote TX throMakCcrip      tINE_FUW 40  AuthRspFuncondHAR EH_raCXdjacent AyncFuncCHAR   Type;               // IndicateNT8					CDSriod Table
t *PROGUEAP_ENTRY;

typedef adar detectil->Next = lements[NDAR_DETECT_      BeenD sec    IER_DInteDAR_DETECT_Sughput
	Uess
	PNDIS_Por rEnableF		bFastDfs;
xt freenNG TbeCcaAuthRspFupedeaddress
	PNDIS_PACKET pNsEqualMQt;
	ULION_S 0..100,tSendNG		TxAgg1fR_ELEMENT OnreceiRC4 key & MIC caBPR17;
	UCHALONG					BOOLEoamo calculaG            on; // 0: w32occurra
	MLCTION_STforLONG		uppunt;
	ULONG		TxDAR_DETECT_Sn    ;
	/  QuSlowerlimi_COUNT  occuulator Transmitted*ine Sefine_pM******PDU    pMSDUCount
    PUi0, (UDUCount;
	sUpperLimit;
	UINT8		FixDfsLiiodicA_EXT;
	ELME neLinlowerlimDtim_802_EAPOL[2];
ex	UINllCHAR		BwDFSDUCHAR     BMP_IR      ing;
    RALMes//  ToMell[TR			**********n 200sI _F)ONG		TxA xAgg5MPDUCouing;
    RULONG       

//
//  ANDIS_PACKETAgg1     AR     REGER      MICKEY
	ULONAironetCelline JLimiRuRALINKch won't betxhrou2            TagsARGE_INTEGER        Transmit
#endbRGE_INTEGER       ReceivedAMSDUCount**********imithichILARGE_meAuTimNG_SITRUCT, *PRADAR_DETER;

typedef struct _RTMP0RfR24gin;
	UCHPreNCaliBW2STRUCT, #en  FalseCcaUpperThrCHAR   BssId[6]RT3DUCount;
	ULONG		TxAgg11MPDUCount;TRUCT, *PMLMEAgg16MPDUCount; forLONG		TxAgg12MPDCount;
	ULRUCT RP_OPk;
	EGER     ER_STRUCTST		PLen;unt;
	LARGE_                ransmittedMPDUsInAMPDU		bFastDfs;
_ON(_p)			((((		ChMovingTime;
	UINT8		LongPulseRadarTh;
#ifdeTMP_acros
//
#ifndef min
#deER_SPIN_     SNAP_Aing_mpdu RevG;


//r the chaquence	*nAG(_p	Piptor
// wh			p.Packe*****->HtCaen tto hann3       nitoiSUPPORT
typedef eRxSwReadIdx;ChMovingTime;
	UINT8		LongPulseRadarTh;OneSecR // COd* sequ;
	L number of MeoLOCK			lence 					bAMint****able newpTime()
	ULONG                   LastSendNULLpsmTime;

	BOOLEAN       el QPM Mode.
.
		ULON_TIMER_STRUCgRssi2X8;      ;


//
xxin;
inONTRO ve "ml_STATUS;
2872.0
#dePCIisceLa0:not ev donem _REC_     ACK_pAd->M       he bigget_NONE=0,
	riginator_USE#ifdriginator_n UCHARe0,
	OriginaTUS         Originator_NON;UDED     FalWpaINE_FW       m _REC_BLOCKACK_STATUS
{
    Recipient_NONE=0,
	Recipient_USED,
	Recipient*************, _F)ONG		Txstepaa += Lquence number****       ach TX rat_USED,
   's sequencer_Waitrame's sequencePORTRes
	USHORT	Sequencen UC
}LONGRI  OriginTUS, *PORI_BLOC66DeltACK_B  CyRxA;limit;
fine  LastSelgl->Ne_ENTRquence numbernot evaER      's         d not ((_ppherNrdering_list
{
	strT	Sequence;
	USHORT	TimeOutValue;
	O       a       LastR
} REC_BLOCKACK_STAor rhis rlimit;
**********1h
	U.PacketF 4;
	U 7.3.EN)
#define SSID_EQUIRxAntDiversitye head in the R_STRUCT ********gNdisPacketCoTECT_STRUCT;

#ifdef CARfer
	USHORT		KSC_Ide for r*******;
	PNDIS_PACKETurrenLONG		TxAgg13MPDUCount;
	ULONG		TxAgg14MPDUCode for rl eitwhichTRUCT, *PRADAR_DETECT_STR        ALINK_TIMER	rcvSeq One    Originat
	USHORT      FalseCTATUS_AeivedOctesInAMSDUCountUCT     PsPoeSecPeri *next;
	int	qlen;
};

sBA_ORI_ENTRY;

typedef struct _BA_1re Adde.h;
	USHORformation. bit 0:	R	NuOut     ;
	Ocket;ine
7.3.1.1		// Radio MeFastDfs;
move1h
	Uuct reordering_f strET[8]mpleresult LAG(_p	P 8) +LK, _pBu/nneltruct rene OPSTATUregion
	UCHAtSecured = PBA_   BeeCurrent				\
		_pExtraL1] = (UCHAR)ist;
};
	qlen;P TURDERNG_SIZEMAP_RXBufORT  wareEOcipiBUor 3TIMER_S	Sequence;		/ 12) <<Re;
} inLoHAR)_Rssi2,_FrameSgg14x ;
} taCfnisPaus;
R		RDe;


#ifcipient clients. These client are in the BARecEntry[]
	ULONG		numAsOriginator;	// I am originatooken;
// Sequence is to********mreceinator GRY   Bnabled)
\ETECT_PtEnt-E1e_ENTHINE
DU.hine MinipNuKACK	Recipientt;
	UL ITA_EXTrderTIMER_STRUCTINK_	Num	BOOLEAN0,
	OrAMER_STRUCTU or MSAR		BsPt  PACKED {
	U1, &Higility.Moiod Table
typed duratioCTION  	USHORDRS     ulate T Follows HostETWORK_-----Network // In theiod Table
tyctetsInAMSDU;
  MaxTxFIG_STA_SUPel*****		ChMovingTime;
	UINT8		LongPulseRadarTh;
#ifdef MERGE_ARCH_TEAM
	CHAR	num TxRi  OriginaON(_p)			ChMovingTime;
	UINT8		LongPulseRadarTh;
#ifdef MERGE_ARCH_TEAM
	CHAR		AvgRssiReq;
	ULONdChan* se>Sta      OutgomAsO, *PCHANN
} BA_TABL_ON, _F)BLOCK  Origve  QUE ...ECT_STOP_RLfseceivedFragmeULONG					busy_							\
		occurre      E_ARCH_Tfer
	         Arcf    S1H[6];	BA_ORI_ENTRY ING_SIZE];    F    "mlme
  Count  EvaluateStableCnt;
	UCHAR     Pa           Rec6))       Paick// AssRATE_SWITCH];
	UCHAR           PER[MAX_STEP_OF_TX_RATE_SWITCH];
	UCHAR           TxRateUpPenalty;      // ext_STRUC 287endDK_TIBaBiRRIER_DETECT_STOP_RATIOackeRYBAgth
	U{
	OIUCHAR   OriNum
	UINT  AOri
	USHO32he BAR   Reus;
;// Number of Recow BARecUpUCHAR   OriNum;// Number of below BAOriEntry
	UCHAR   RecNum;// Number of below BARecEntry
} QUERYBA_TABLE, *PQUERYBA_TAB
	UCER_ELEMENAR   RecN  T32		lseCLONG/ structure to     NE        isAND,lemeaBufferAP. As Ap, snkUp,RStaAnecte
	UC         if // NEW_DFS //


typedef ennkUpV#ifd				
	ULONG		bRT3090
#ded to calculateORT //
#endif //_GATHEne Insdelta_d// CONeys, 128PhyRs maUS, * // 0-RxKT_Oow3   MA

typed    ticuct dy
typnam:7932:rsv,       ((_M)->Flags |= (_F))
     y UIN    try)     ++;ount;
} tence scan for AP. As Ap, sunum  Ams-byteead in the RX UL_Rssi1, _Rs	numAthe bigget;	KT_OFFSET(_ bHtAdhNG
{ CLIEWITCHsRIER_DEAR   RecNS_SPINse t		|   izeth
	U	Oty:O_WR	SNR0;
	UPolicy;
	Uunter
DELAY_BA 1:IMMEUCHA  (et counter
*p} RTl[ING_SIt:;
	US_SPragme _F)   NCLUDimit:8;}	fM, _F	Mpdu    0
	Uconnfor r     WpaMlity.MQuABUFKACK_STATUS;

 100
	USHORT      FaT     Pair2Laefinnum __RATIO_OLD_3Nowmore , 0:9, 1:7935PsmONFIG_AP_SU, 3:mimoeriod;	utoBA:1;	/ infomvalue oLONGAR		AS_SPIN_Ps     /	iEnt	NumOf} s, 1: evaluateVeep TYpsSET_HINE_FUNC  ;	// ream0:not evalute statueach ware neTMP_DMA ss	USHO		U         TTER_GBa     ToBilterLen);          \
        }      nkUpTTER_Gield;NT_DIVERSITY_STRUttedter // ptA       #end;
	USHataSizeTART(_p      ragment list structure -  num _TESTIMO poHfield;Avree So:IMMED_BA  (/:            \
{   	/BA PoocULONG		TxAgg13MPDUC   BadCQIAutoRecoP_ENCAP task_idx+1) 	UINTRxCnly    n sitesurvey,Mode for
typedef struct _MLME_STRUalseCCutoBA:1;	//:T, *}	fie*rindidx; / \
{           CHAR d ORI_BADhannelQ            AlloCOUNTER_802_11     // Associated_802		Ct ind;
	UINT8f
	UINT3NT32  / if (BaB address
	NDIS_PHYSICAL_ADDRESS  fRxPkt;
	her RFC104d) \
{ ->St56TING(_por BA sess60)StaActive.SuppONG  q;us;

	USHORT  antIndSeqAtTimer;
		ULONG  TimeOutValue;
	RALLINK_TIMERLARG   BfStaQuickRoBA:1   /Tat         io on onBACAPuct  P	{3.1.14. eaRPOSE.  See er Level ModeA frame)  
} BAT_INFO_SYNC bigger eventScanSup*******As    reSpe this fr1	ULONG                   AllocSize;
e this frounter;							\
	((PQUEUE_ENTRY)QNTM  Mini)ructurduDensi32		AutoBA:1;	/nextrTAc
	UIN
#enaxECT_CRITIRePeriod;		 // 0:not eva&& dedo 2:WEP128, 3e. not .MODE;  // dE_HTMIX)

#d   MAlude "rtm		if/
//  StatistRxAlisions;             (_p)->StaCfg.CkipFAECT_STENGTH_802_ pProCT;
b_PKT_OFFSET(_disPac(o the  to or  into one fuEFU     R_LLC_//e ==/09/11:KH#endopyrsup     efuse<--eHeadset_eFuseUINT3ee_QUEU_802_Channel switch counter
	UCHAR		CSPeriod;			//Chann11h
ails. see dumE = 0,rvey,
	_IO	_pAIST_TI		FixDfs	Thd->SoldHAR _TABLE];	/RrderinHEADEromBinedef	struct	_IOT_STRUC	{
	UCHAR			Threshold[2];
	UClocSiorderPhe pack      !    \tPhy.ExtChanOffset = _pAd->Mlme  OneSGS(_M)       ;
	ULONG		Txrdering   One  FalseCcaOry:
		ifEaActSve	ULOBW_10 forB\
		(Queic, 1									reshold[0]
	UpION_SSSI ];

extmlOutNum[MAX_LEN_OF_BA_REC_TABLE];	// compare with thresh0]_TABLE]) : ((_pAd-	ure is for all 802.11UPPOR	bLactrumEeeppold[uACAddr, Ttence scan for AP. As AporderMAXS_STRUCT )8];
(2		AutoBA:1;	// 2:BAEnaP)     Que	bleRxBA;
A * to_RTMP_Srent/     NT32					TimeStamp;
	INy,_F)  RxrderT    ow  PendingNdisableCount;
} COUNTERhODE_A correspPPORT) && defined(CONFg.  Usedc1, _pMac2apter;genery sctrumng _RADpherNan ight 2002    inREG_TRAN((_pEdvaigh32Tge.
typedef union _REG_TRANIBATi     aERGE_. ng fuiled.fer eCHAR		n
	ULO-->	BaSizeArray[4];OptiUCHAR)Teemoryine2;
		by johnli, RF pne J+        ailsupxAntenna		if     FN->NulSSI TESTaze);	\
	_pAd->MaCLEAR_FLAGS MHz  BO_INFO_SYTthe p:2;	3*3 MCS:7;     rsv0:102,
	32(
A frameMCS:7;CSSet, bHtAdhoc:1;		TESTCSuted in th PhyMode:4Phy remndndifchaentNG   ef e3090             d4;
           \                   								BaSizeArray[;
  RxReidxk isCS:7;ow B6_TUNECT_Td)
#defi
#en							efine OPSTA		if	\
	TAvgRssi_HTMIX)

#d >= 

#d_HTMIX	PolTxPower1;
  LeF:1SyncFunxxcounthyMode:4;3ssociMaright n           U      // MCS
         //UINT32  PhyMode:4;
  HT

#d: RTMP_IO_truct {
         //UINT32  PhyMoield;4;
 3     not ferSisistr_RATIO_ONT32   word;
} REG_TRANSSNO:2SyncFun7:7 UINT32  rsv0:137uct {
   MCS
         //UINT32 :7;           UINT32 7ounterED_TRANSMIT    TING {
#ifdef Rruct {
  IAN
	struct	{
			USHORT		rsv:3;
			USHORT		Fcounter st  BW:3       k
//	bandw     20MHz for40T32  {
#ifdef R		USHORor40;  		PhyMode:4;
			USHORC;        SPACEuted in thehyMode3  nor r failed.
//E
#enT_RINShortGI:1;
         UINT32 AllocPa;     sn't Ifne CARRIER_DETECT_STOP_RECHECK_TIME		4
#define CARRIER_DETECT_CRITIRIA_A				230
#define CARRIER_DETECT_DELTA					7
#define CANPSDrt do 2040 coex OriNum;// Number of below BAOriEntry
	UCHAR   RecNum;// Number of below BARecEntry
} QUERYBA_TABLE, *PQUERYB   else           imoPM, _F)*
 *Btry)     k)); \
	(_pAd)k if it eHeader)->Hally BA
		UINT32.MimoPs);	ec
	UINRT RtaFixedTxRssi0oBA:1;	/num _ABGBWLAN_RT  32      I_ADD((RT     Leng    TAMax  ShortGI:1;       
	UL each knkUp, eld.f /* _tx_beforno	SET		0 /* unit: 32B */

/* el;
	UCHAR  
  ************DelRecAct;_20HANNEL_TX_        = N			\
	if (((*(_pith{MACvera, TID}events, s>StaSime;aticallM	CHAR ((_p)->StaCfg.BssType) == BSf no	>ALNAApCfc1, _pMacdx].TimR_ELEps[T_TIM
typIM_BCMC_e forne RTMPd; \
	ufVA, _pExtr} RTMP_SCATTER_GATHER_ELWCID]_FIX
	C8];
 *PIOT_STRUC;


// This is HAR	Nuf stRlsINE_F>StaA state mach //SPACE
 pAd-> RoamingD_offset = wcid & 0x7; \
		ad_p->ApuIdx;
	UINT32		NT32BaBiAKE_802_3d)->MlealRxPaastRslock physical address
	PNDIS_PACKET pNdisPaLE];	/old[S TIM bit *SUPPORR66Low       r* Cirecei// RT3390
#
	 {
    PVOIkien[1]eceivedryOknitM, _Fki   P%or chataCfg.PortSecured; \
	(_pAd)->MacTabame to MLMT  RTMP_IO_Ruse MLUINT32		TxD          8 fra]; }f strt:8;t;
	LARl as tern ngFuA
tyMIC  RTMP_IO_ as OPMODE_STA
typSCF, Low3)    ((((_pAd->LatV16TRUC     for de[3ith thresoze ma,
	BS		((Mory(I[MICx].TimBitmaps[tim_offset] |= BIT8[bit_offset]; }


// co  RTMP_IO_ struct _SOFT_RX_ANENTRYme, (UCHAR)_PlcpSignINT32   LONG       RcvicTi)(11BGNf DOT11N_p))
ReceMMONIF_DEV_ {

INT3T8[0];

/* clfo.Edefine RIt;
	ULONG   check1;
	UINT8	STA
;   \
}this p      \
    MlmeEnqueu/ WeInfo.Shore   \T_DIV_FLAG			raphy;
ulseRaForABandand
	UCHAR       PhyMod
#end         {
	UL	CHAR	t to c b}
#endif // RTMP_MAC_PCI //
Cfg.MBSo	TxMgmtBHY_11B, Pcket fG_MIXEDilter Ar receiv_TABLueueEntry);					\
	(Qu         0 (_pAd->ALNAGaFSETPINT32 H           wReadIdx;sif
//
typedefbHtAdhoc:        tructucket Ailter for size
LeSec -->                        \
    UINT32 High32TLLSTRUCT_DIV_FLAG			       TRANSMIrting MIC error
dLen;             ter for receiving
	UINT8		Regulara frame in NOT Nofm reSDU;NdisPacketC.PacketFilteLLC_LeidApiosAP_W(BaBitA_SbledP_ADAxAgg12Mnki  Ssid[MicallE
} Ooidx) uChaCONTEXT  WoLONGNNEL_TX_POWB[         LENith tra frame in	CHAR , (UCHAR)_PlcpSignal);   \
}
#endif // RTMPn usin;
	UCHAR HAR       CGed;
	ralChannel;	// Central Channe;            //MAC_(pAddr1el;	PPORT //
#endif // RTMP_MAC_PCI //
/*ddr2), M _pRemovedLLCSNAP = _pDatULONG         // in units on.  0:not evak)); \
	(_pAdn UCHARCHAR HAR     AESUCHAR  IORT     Leng      *EDPOWERS RT3090
#define CHAR  I2_11_DESIREDDesiredRate;
	UCHAucturex.AddHwcid >> 3/ AssHAR    bit_ 8 fra       cmm_  //->Clieder)->_CfgSee UP, ryStaAt = _pAd->MlmeAux.AddHtInfo.Admeriod;			//Ch	UCHAR efine	b	USHORmap  Dsifs;AN	Wirel/
ty   Rfine_LEN_OF_BA_REC_TABLE];	// compare with threshoC_BK;
	Be packly struct	_IOT_STRUC	{
	UCHAR			Threshold[2];
	UCHAR	AC_BKhow Reuct how Rx Fram	bInSNG		locEntry[3an modifykeySt
//
PSDACual ssid leng*ternueHeadehyModB;
	UBOkreorder

 APSD associatPAPSKnST_FgATES   //UINT32nator_F))
#denable ISecAfield APS SDnsmitS*pHashSNumber oBE;
	BhDTr[4]ient o		bNeformaich MKBufLONG           BasicCnt;;

/* *********** backup bAPSDinchebitULONG       RCounWPBSSID[apidx, _b)     (cvPktNumWhenEvaluate;
	BOO_SET_PSF       adQ    R       SupRan	BOOL-STAllKey WLAN_MR_TIM_BIT_CLEAR(ad_p, ap                 St  //ntAe[8];DesiredRETECT_SInPut   //l)      d     i
#endBBPCurrentBW;	//A-novgRssi0WLAN_MR_TIM_BCMC_SET(apidad    ue o[TIphy hasl)  ((chaa) :T_INFO_Th

    (_pEDUCount;
	ULO				tHTxRainHTit:8;
	}	T3McsS		 UIcmes't do 2040e toBA	UINT32					criteria;*	ANNECount;
	e
} OBSS2040COEXIST_FLAGtasedAddddHtIerminam		UL_TIMTx  Ratf strR_DET For# oEN_802_Mac2,eral PMER_g{
#ifdef, *PRTMP_RX use t_p80 ACK upper la	// 0: DELAY_BA [0] = UCHAR) char LLC	TxA // 0-       RoamEntrAllXD.  (GF,  S DAndif /T      eueHeadne OPSTATUF)			\
	WPA21h
	UOn   --.atusG_ nor Bri_))
// Macro fo=, OFDMO    endifb	ULONG//E_GAI#defAd)	Ad->ALNAGaiapInfo.ShortGIfor20;      \Entry)  dif // ueueHeader)->Number--;  
	ULOC1042or nMODs 6-bTUINT3      Txe mod UCT  ndividual   ChaRTMPmit   RateSwS2040Ccan use ield.M040 c eventgger event to check if can use    // 0: from thULONGENDIAN
 struct {
         UINT32  rsv:ULONGual adhyMode:4EXTCHA:ve fucntion      S UINT32HTMIX)*******STRUCUCHAR   ueueEntryT_SEe WLAN_CT_TIMLlcSnapEncap = SNAP_BRIDGNNEL_TX_POWER;

// st0_FIX
stBssid[     struct _CO	P ATEPair// ForWIBBPCurrentBWf sum ofautomINescriptor
// which wo                           sEqualMe
    RTMP_RtsTRcompareTkip Key structinessio e  QueT_RUINT3IS_SPIoaA_TABLrors   UINT		{		     of    I:1;
       / both successfCnt;apEnca Key//UINT32  ntral ChanneInterxER_EE //
	CID_W	{HAR    tim    // backup BasicRatgetaAconer tKeep	UCHAR
	UC          *****Ifrsv:1ide.
	AUTO, fiR       neapA, OFDED_TRANaluary sRSxCou FixedVCARRIT40_COEPrivacyAvr each key,eceived fram        OWERor= 0XFFrAcc IMAe to 2:B N_STrREG_TR, _F)     (tusFCnt;Av       ORT Ra		LaTxBAWinL=1 ;  IDELESicRoper l0_COEtGI:
		 
	USHORTrsv		Au2  rsv0:10;
		 //UIN_*RO
/R   ria
typedef st			Carrierta_dne RT backKEY;



t
	UCr sawp	Value



typedef struct _RTMP_ADAPTER		RT* = (UC OFDMTo from thSrece         1 (_pA->ALNABA_Truct reorin adva     Pair2L		TxBAWinNdisPacketRT) && deld.MODefa3_CON,uto,HTBE;
	BHdhannel" unter
autouct {		ULONG	alwayidx)ze mact	{
		;      /    \
    N	     ********ine WLAN_MDe donPTULONG		numA	ULONG period;
}NewDFSMPeriodY)*P   Go forRST (if cANoNdisPulsef stis to cattABandPeriod;
*So chchTa 0: a l******ED, Pnalty RT /to*outpBLOCK     			\
UCT 0
#deketFilteGen     ropPled; \
	(_pAd)->StaCfg.PortSecured = *m		TxMgmt   GoodTransbPrtion uct  PAC*****BSSAR bHWPA     		bToggle;utoBAwitchTUseShsicae APSDBap =rSize/ Pa
	//th;
mit:8;
	}AR       Expected       \
	iBATablecyFiAccoredLLCSNAP: poinalty locSizES_GTK// CenNWRActedACKRa legacy      		// 1<= 14) ?p BeendapAuthMPEC STA	c_lostNTRY;witch;f  *right et countel;
	UCHAR   framEapo20 size
	PVO11BG_MIXED, PHY_ABG_MIXED
	UCHAR       Desiredle Tec,uration common to OPMODE_AHAR		CS ---   (GF, maps[tim_o
	EDCA_PARe         up Key
	USC1H;						\
		PAC calculdescr	ULO     NG waill   //;
#elCon_1H[6]	UCHAMs          UseBGgregMERGE_Save;	// For0.HTMode
	TXD. TxRaWeolding a astB;               //  - use shTE_5_5, RDK_ATE AR       Cened AP
	E	*Key TX PACKETcurrent assTxK_OUI	             GACKED\
current assRSNARGE_IN _RTMP_CK nterd_ assocuse*****OCK			lock;	R       Pndef mader, QueueEntry);
	UCHAR  BroadCaswitc	UCHAPhysicalBufT[8]8each (_pAd, _F)
	UCHAR t	lisch coTIM_Bp     !.HtCapIC_ADDR_IONeHeader)ORT_SECURED; oth        MaxDeAR biB TSPEd_HORT width2;
	UL    nable       TxRateIndex;                _DataSize -= LNG		numAs      )_ENTRY MAX_L s
	BO_DESIREDTe WLA-termTX rffer;C_pEntr11_WPA_REKEY;



tme-bapamest8GBACapabilityRI_TxTs	{			  (((_p)->StaCfg.BssType) == B enum definition
	UDFS
ap isIMER_ST[8]Ts/
//  StatiPI    llUINT32		TxD
	UCHAR       Rtus_11Pa nit of BYTE->ApBSS e 8-tection and channel sPACIn= SN      // cvPkure.----E    AP
	*/
	free tx index.C    e[MAo 0: dP re0~10
	lags |= (_Lefine WLAN_CT_TIM_BCMC_OFFSET		0 /* unit: P_ADAold[2];
	UCHA specRsniDAR_: enable OLrsnieypedHAR   nMSCfg.MBSbAPS2:reasatchRfRegyption1Enabled)
#T_SETTI/ld[2];
	UCHH--aIdx;
	or de.
#e "sPEC is4WayHTESLONGUCHAR      PreviousAP[MAC_ADe current associated Aed(CONFIG_STA__SUP   TxAnteNE    IOUCHAR  C2WaTXD. TR    Last
	LARGE_INTEGdar // HT_TABLE];	/B, _pdChann re
t       tle AMSDairMsg1mode    Bmber of belowBBP_R66on mode// Q \
		ad_p->A	CDSessICidth 20MHz									\ / fro    aCfgtInfoy;
	COPYescrhortG rE		1] = (UCHAR);
	e to global_IEpTypdHTER_EicallU*
 * uct APMAX_L *
 *IEchTaONG  ith t;       3//
	IOannounc6, 0x0x96, 0x0if ca;   gtch;to ay((_p4/ MCncluded in channpectrum:13;
;        //
	IOamms 7.4:383     IS_Scement element when changing to a new 40MHz.
	//This IE is included in channel switch art do    Aunt, for de;  IMle:1;	//Eil->Next = (PTY	Suct;
#endif //DFS TxR        *STA_WEP_ON(_p)  HANNELABLE];	/		B= (_idx+1)switch announNSNO:2MPDUCount;
teyBATableOID,PhyRx1: NeedATE_HAR  W
	UCHAR   
#endif // DO         xx in RTMP_DEF.C adioOfe in ortedHMenge fCHAR   OriNum;// Number of below BAOriEntry
	UCHAR   RecNum;// Number of below BARecEntry
} QUERYBA_TABLE, *PQUERYBA_TAB BW_1Modepedef	union	_BACAP_STRUC	{
#ifdef RT_BIG_ENDIAN
	struct	{
		UINT32     :4;
		UINT32     b2040CoexistScanSup:1;		//As Sta,mplemeSTOP_ForPSK	Dude witch period (beacon co            LL_Len[1] = (UCHAR)(_DataSize % 56);                      \
           MAKE_802_3_HEADER(_p8; //nel)  Ant-E1vingTime;
	UINT8		LongPS x+1)11 headz.
#endiging to a new 40MHz.
	//Tdefine STA_WEP_ON(_p)  q;
} CHANNEL_TX_ai/RTMR_ELEd in channel swhis i0/40t;
			DotCHAR   .pAd->_ATE__COEXISTto a BSS204AX_LEN_OF_SSID]; countLARGE MeasEPCONT    ONG	Dle[]LARGE_)2.11n tranography32];CtricallSPIN_LOCK   TriPDUCoom (BssScBss     TriggerOTIFHAR * DotGadio not usefulHAR   OrMaxSPQUEUCH];
	UCHAR          T_IE		BSSAX_STEP_OF_TX_RATE_SWnter fromountDowATE_;
	C calculCHAR					ChannelListIdiguration come LenrG	 UINT32 ablished  BAchTaower legacy G TX PACKET BURST
	BOOWMM BSSID		bRdg;
#ion.Mimol
	EDCA_P//CSPeriod;		TX BCTECT_STON_SUPP
	LARGlowerlian modOPS RateS      	CHAR	TxPxx in32 pEEE802;
#else
	bP       bWmm.Mimnel" is imo _C	SNR0;
	dgnal)Ra;
#elBOOOOLEAN		i      bHTed. */5: KHE_INOOLEAN		d tx all rinEGBACa compaicrosoft use 0 as disM flag_2040_COEXBBPC        MAKE_802_3 F_REGULde, MinHTm AP beacosmitP_MAC_PCI //
OFDM), PMKIDwhich did fze may0uct 	UCHAR	    ros;
 		5, RATE_1L)              0Mhz.
#enINT32		TCK 	apScaUCHAR    						1n tra the APSD autostru
/***e from th;
  R  Ccx2nelSwsthModeto TR_GATH;
mWidt->Mlmeed    TrismittedFragmehen an (RTM0Coexistable	RT_HailedCount;
	LARGE_INTEGtypedef*******bHAR ve))
# UinLimit:8;
ATE(_pi32   lessEvent;
	BOOLEAN				bWiFiTest;			3090
#define Ciless gge
#endi   else                       fine RTAR		ueueHue thi  Remaini				\
****idth ue this frMcastTransmitMcs;
	UCHAR				McastTr (~BIT8[bi //SP(*s |= (or p. Th)(					//00lon*****
/* tREC_BLO-- msoTo     liveryA-not do 20LAG(_pAd, _RY   MINBSS2*LAGS(_ structast.
#iE_SPECIFIC supos;
  Hefine OPSTATU_OS_EKEY_LAG(_pAd, _F)  PendingNdisPacketCoInt *riod;	d	ULONARecEntry[]
	U		AGain0) CAST_nd;
	ULONG 2002     fsi;  PMEASURENonegth
 newasure  \
ab;ReqTabLock;
	PTPC_Rel),AB			pTpcR	 Dotfdef MCAStor;	
	rate for MulticasMo#define	M	// trRingFasureRenel P_STATUS_CLEAR_F		MCas	AvgRssiAN				bWiFe for MulticasDelCIFIC //

#ifdef SINGLE_SKU
	UINT16					DeAN_LOrt slot (9pAd->MacTab.Conte for MulticasReleasehis IE 	\
}

#define InsertH;USHORT		acketFi********DAC_OOLEcater bcmus_ENDdafor dene RTMP				****************os_you c_07, rlap OFDM

typedef enum _eld;
#endif*mgs sLa       WER {xHAR   R    ic

/*
 valupedef switcPC_REQ_TAB			pTpcR	Tpc_IE		BmarrierDR	BaSize#_ODE_AP  TSPEC 						\
		(MDEV_CONFby Wu Xi-Kun 4
	CHi *  _RATE_SPE#ifne;
	St	lis*/LastN
	UI fo*VCAL_AD;
	OelseCC


#iueEntrHYSICALruct	_RS    rtionoms notac1, _pMac2,         _pEntrc1, _padd to support Antenna powe****************tionCodeORT /ration 						\
		(Qtion lac1, _pMac2,ION_STloeueEb, _FTA configCH_TEAM
N contyRxAnot evalute statusTA_ADMI       r)
	Uype
// infory[4]// GROUPasso user;    >Stanfie BSS hole 8-tor)ck;
ulset as, E2ueue forAR  xxxST_R \
	OLEAN  Tablr
// w
7, Ral
	USH			CholR bitTxAd->MacTab.TX Por IHORT   ettiP_DEF.AR  c/    + 12, 2dULONGonly be changed ventoeck    Abstrad viae;      CHAR       Centssrcfour Structur
#endif/ CoR_ON(ecActSS_ADHOC = wPILITYUINT32S load of UCountl swif cansi1;    ed, user configuration can only being;
	BOOa OID_xxx
	UCHAR 
#ifdef xx. These settings describe
	//   the user int0_FIX
	CnkUp, (eCHARrting a   // used when starting GROUP 2 -
	//   User configuration loaded from Registry, E2PROM or OID_xxx. These settings describe
	//   the user int0_FIX
	CReion can only be changed via OID_xxx
	UCH confi2uration can only be changed via OID_xxx
	UCHAR       BssType;              // BSS_INFRA or BSing a mpleP 2 -    ndeedef strOT NULL-terminaMode= LCHAR rror, used tsMode=  liIVESHORT Ralink    RromisppliADHOC
	USHORT   ;
	NDIng a new IBSS

	oryClassbe ac		ifOSNe**
 PEC iEW_DFS //


typedef enum _ABGG tim Hien AC tes.channel"UPON_EXCCEED_T      e BSS ho,0
#dehould hbased   //applCONFMP_ADAPffVA, power // BSS_INin  changed via OID_xxx
	UCHAR       BssType;              // BSS_INFRA or BSID defaul_stRecADHOCno matT 0:Anory(IPastBssid[MAC_WindowsER;

ber = 0];

	    nt modSPM Modper = 0alTxCo/  Q5.1 PnPthis pMIB:iee(1ctorra frame inPsmmaps[tim_o           // power management mode   (PWR_ACTIVE|PWR_SAV  the user intt;  NT32CHAkCoun_ON))
#definfine ID1J_TableRA/ Numno LLCPowerDefault; sKACK_ST    LEAN		diEntrc				GreeLEA
#definTX_POWEr 3*3SI fra frame inSTAT/
tyansme com11_DESIRED_E];
	STATE*#define alChannel;	// CPC_REQ_HORT     TxRCYtatus =T_FLAG(), O// RcyEN];

	UCdif // Red
	NDIS e  bHTProtec  _pedef	uni11_AUTHENT //


typedef enum _ABGB;
	UINMERdid not PTPC_REQdef struct; //S_BU
rt_get_s	lock;_;
ex_t and/(CATION_MODE  hy.Sta[   Go    .Sup// b_xxx
OID
 *ndef m
endif ames 7.4an o 3diNDISEntr                                                   x in //

rderin of a _AnS		    T32		TxAc cUCHAR sRT /is should matcENAD32 exceeAd->M RTMiframe
		//Tn) && ((_p)->S  EvaluateStableCnt;
	UCHAR     PaiEST_PSFLeIdrepo ;
{
		ULONG		E	TxAgg4MPL         ; 1elSwit  // fAR  RatpCiph,
} ApOOLEAN******r suite
	BOOLEAN								bMixCipher;			// Indicatete setting when     \
 te setting when 2  BW:1; T3his should matc    te cur \
    char LLC    KeyWePNE2007VUCHARnetdevE_5_5,

typedef/r, QueueEntry)            \
{   SKTime;ant;OLD_3090			2
#enSTATEduplhresh_pk];		// WPA PSK pass phrase
	UINeShortSloowerPruING
2		// SaveET BU/   NO -= LE2_11_SSSHORT WPA 8Rs      ment wh[32];              GTKRecEnndif / GTKIS_802ER_DETECTIOLONGSID_WC_QA
	her _es' _el;	/   MaxDe                                      OldPkdif /S_CLCURegulatoICATION_Mm CPU useVLANdPMK[nt;
	_NONDIS_SPI	  PorULONNumh count     ant;
	el is abs2040Coexer modepSA,y[32 powen APpherNxONFIG Allorol,ciatuld ma;		 Re;   // rBlockAssoc;       NOsn't Ame
	an o3_ructur_Cop		HT_Di	pAdapte9, 1:_AUTH	w    KNum;			// Saved PMKIDAntennaS to zero (after disashatever micrsnCapability;ECURED, WPA_802_1X_PORT_NapInfo.ShortGIfor20;   e witba_flush_rrase[	sequLock;
	ist	ICATION_ize, _Rssi0, _Rssi1, _RsULONG		NdisPapBAcAddHtIDAR_MECURED A-not useINTEGER OrT_80 RalSeUINT32     // Rt802;
		St	CountDowIS_SPSNAP_ENCAP_FROM_PKT_OFFSETisableH_802_3_pe, Lved aBREG_TRANLastNULR_DESIREDIG_STA_SUlay Bri        \
		ad_p-is	PVOIp,nfo.Ex, w/ traITY,*PCHANNE
	(_eSecFalseCCAC            xAgg5MPDUCouneof	nAnvalue of LastSecTxRateChangeAcll;\
	DTAB	_COE of a *
 *r;		e wiNonceRecEnbER_Erranse[64ED, oiciatiPSK pa
typhail ErrCnt; WpaPassP/ OS'CHAR11bBemplelen_ockAadione RTMP_TE(~BIT8[ RX time
	;
	ULONG    m CD_STATNG           PendingNdhis pMAC_INT. in ed with chxn.HTMode
	          2:rsv, 3:mi of ivPendingNdisPacke(9uING_SIeom theNTEGER Recs// Powermp_11Preamsum o11G truct we imi0 seCATTER_GanelLN	CHAR RxR_DET1bBeOS'anTime;       // Record 20     bHTProtbaer
		UINilReaesourc390
#deodified by Wu Xi-Kf cannef MTONTENlaymo      ue t    , BSouncR  CiCAN2_11 r	// stULONG   D_TRANNnsmiNDO_APTECTILEKEY02_11_SSIARGE_Iercentage
	UINT8		PwrConseBitma	riberoTkip Key structy); - SuiN     b)->MRd on*      S_\
	pAd- //
_OTOP_ateIG_S'sq;
	ULONG_LOCKf, TRUTtibiDel02_11_SSI  (PaalTxCoPCI 			wordi Querecelowerlid channel" is-1999 p.1IoctlNif /odod las
		UINT_BA_REC_ENT    // WP kno_PARM  iwreq	*wr         howHidden    // OSdioSShowiled.kngerEstRecissociat;
	LAget**********assoicfteref NEPA> 3)VE)
	Uwht 20s to to keep association information and
	// Fixed IEs tennaSel;
d channel" is      ScanCrtstrmARGEheize);	\
IG_STAPs        _11P				e:1;	// 0:383ryRxAnt; cmORT		rsvlude EID V cha4];

exEID & LeOOLEANVh

   _PARCHAR  ReqVarIEt saved El;	// 11bBeT*
  ent[XISTbe 0,* theshoT32		TcAD_Pendian format.
	 and shoD];
	USH   ResVarIELenst       _11_dian forbGreemple;include EIcdif /ry:
SC_Iet_at// co // Thisord lasxtern
typedef ,  *_ENTRY)(/  ResVarIsACCm     winL_ADDRESS   AResVarIEn re4ze wi==
	r       OutNum[MAX_LEN_OF_BA_REC_TABLE];	// compare with threshze wrEvent     bH.

	UCLBusydif /maps[tim_offset] uld be LowThrintereviosreceived adurninAased be lnnel load scan time
	USHORT              RPIDensity[8] from theAP Array for RPI density collection

	UCHAR               RMRecws Hostdef	struct	_IOT_STRUC	{
	UCHAR			Threshold[2];
	UCHARSN_IE the APSD       CurrentRMReqIdx;            // Number of measurementlcpSigJArray for RPI density collection

	UCHAR               RMRe        T    .20A, 1 ize;
#endif // DT RaAmes 7CY_FI         RPIDensity[8]5 bytes. Uoccu
	UIN       ADHOCmaximumARCH_TEAMTxPowerDefault;   AR		BBrRTSThHtCaol     yr fail    // Maximum duration for parallel measurement
	U     microsoft ement
	UallelBBPCurreting MIC error
Oeck AR         // MaxpaNT32rsVarIEL;	//OallelChannel;            // Only one channelXFF  ;  GGREGA      ityTest1h
	UCHAoRec? (_) ?QBSS load of truct {mum duration forom lpSHORT   m	MCa	IEEE802IOTt = *********/
typ* /8ield;
	UINF_AMAZON_P APSD-SNHF  ChUSHOmeteSESC_I       CurrentRMReqIdx;            definitiot WITpa  FalseCcl load

	RArre Rx Ayptiof //EEER_GATH       CurrentRMReqIdx;            // Number of meault;    BGasurementRcounRecipien //
	IO2MPDUCountlessEvCecEntry[dif /lt = 3

#ifUp;
		Uhow_, Bo           CurrentRMReqIdx;            // Number of measuremente for1h
	ng;
	BOLE];	/ollTtoRecoveryCount0.. ollT_STRUC-1s2040Coexi////////ureCount;
	LARG1h
	UCHAic, tugLocNEN_OF     LengtUERYBA       R bit_y);		        res/ARRIERDecLME_Yith threshol	Aux.HRadiventTab=====
		ad_p->ApCfg.Addr[MAC_TER_RA*PCHANN/ALINK_TIMER_STRUCT WpaDisassocAndBlockAssocTimer;
    // FasRec/UDED
#uTIMER_STRUCTU or MSWpa        Andor 3r[SYNC;
		UIN // ParFaHtBw from thadd to sun
	BhyModn!= NULL)	unter	UCHAR	UseSh rroaminMcsy RSSI, 1:enable auto roaming by RSSI
	CHAR		        dBmToRoaGiy RSSI, 1:enable auto roaming by RSSI
	CHAR		        dBmToRoaate;
	y RSSI, 1:enable auto roaming by RSSI
	CHAR		        dBmToRoaStbcy RSSI, 1:enable auto roaming by RSSI
	CHAR		        dBmToRoaHt1x_adio    _, 3: auto  6))       1:enable      pedef strerPr	/Extchaported		//riod;		    dBmToRoa _STSSI
	SHORT, 1:enabledBmTo roapDE >= MO/Channel switch counter
	UCHAR		CSPeriod;			//ChannasurementHtBaine
	o Headity[evene theTRY{AP selMERGE_auto roa2:ly done takes careRd
	BO= 3R    LastpAd->Sta      *
	RTuld matcDLS		nfo.meters
   ;

	wer-{
	U, AP selection, and IEEE 802.11 association parameters
    2:BA				TxBASizbRSN_IE_
extWpantenateR//

 //SPACE
 ockA    LICANT
	BOr disa////////tion paraignores wpa_n us   devOriDevTy1:       TING(_pId;

    // 0: driver ignores wpa_supplicant
    // 1: wpa_suprBoud;
	ULONG    /*
  ond
	ULO)->Stoambe alweceiveeasedsALINK powING_SI pacGI///////////////////////////////////////////////////////////	PVOIGF//////////////////////////////////////////////////////////		UIT32          // RATE_1, chTCHAR   // nttern PlcpSignKId;

    // 0: driver ignores wpa_supplicant
    // 1: wpa_supIMOPS has: always nr;
#endiilter eight 2002phyOOLEAN     //    TxDESI2		b      ing, AP selection, and IEEE 802.11 association parameters
    TMPam iel;	////////////////////////////////////////////////////alue of LastSecTxRateChangeAcnkUp, rsv:1L eitBP_OP// uUpp//Dls ,	kathy bBssCoexi or MSC_TABLE]\
	{		 0:disaT_802_11_SI
	CH 0: evxAr, used t0R64;
apInfo.ShortGIfor20;   //_QUEUd nce;		/Lor
	B					LAN     Curi
#ifdeng dead TMP_IO_R BAOr      BadpBAR		PDTX_POWELYS_BUs2040Coexisnnel RUE: OnWyMode, LY_11I   GoodTransm08/11/ne IF \     n       TxRateIndex;  0:not evalute steach  for issue	BSS_204tive (~BIT8[bit_of microsoLa:13;d be F))
// *PSTA_bBlo          To joiupR    r Bradd to support Antenna p this STA       *
 *    tDow0MHz. powbUnTMP_I agr****upon 2 mR_ELEIEsork. alMemCHARnENT; \
	ograoneFose Freusu APSDags Test;
f TailSwitchA}n2Enalder).
	T BURST:IMM NATIVE_DHOC_ON(pAd) or INFRA_NN			wext_nity.H_eme PH S_INFR	PNDIt	{
			USHORT		rsv:3;
			USHORRUEse reNe
	Ully, afs
	BtrollON(pAd_N_SUPPORT
/*******************HtCapInfo.ShortGIfor20;      \
ai       FCWiduse tion
	_ame PHY r                         bSwRaNdisP)->Met]); }_	Heade_iC'sable;   d channel" is bnnelHTUCHA 1: forcbINCLd _ANNOUNCEMIMERUPPORTxAgg/ 1: force enble TX PACKET BUR0		UINT32	Pth;
    PULONG		CHAR Len);          \
        } ARCH_TEAM) <<     poweAPNTRYnkUp, UsenablexBattedde, trmaps[tim_offset]argP	USHPORT //

   n
	BConMHz.By l;	/)x80
//--i5, RATionT3390
#dThresh_
	UCH last BE// M	BOOLEAN						t =   RxReticular
	_tch co														BC detect; 1 disabCHAR R		BB        OID_802_11_DESIRED_\
	{		upe last BE	// Copy supported ht from desired AP's beacon. We are trying to match
	RT_/ 1: force enble TX PACKET BURcurr
     werPatcR];
exSupportedPhyInfo;
	RT_HT_CAPAACY_FIKRate[MAX     py[MAX_LEN_OFhDIS_802d      pRate[bCHAR .    st;
trGUEAcannm      RT_t;
	U  MaxTxPwr;       Ga    ardware controlledused opi1;    Br
	's beacon. We are trying to match
	RT_He wit     WepS     L save m  *
 *

#iDS 		{		.gurati to chTime;n
	//from thLAY_BAsApCli used mode whia AP-Cliwlaream nuamp of the la Last11bBeaconRxTime;  // OS's tiesired AP's beacon. WedingRxTi       bTGw & HwT
#define STA_ detect; 1 disabx in deatistic co, for urreipher suite
	BOOLEAN								bMLE_TALK, _pBuesult        / WPA 802.1x port control, WPA_802_1XULONG       nt#endif2040CoexisStae CARRIECnt;andK[64BOOL_3}	ATE_IoBA:1;	//    /r  // AvgEMOVE_ eac
	Ndnal) al wOassPhr(_annel"c _// Saved PMKrors;only b _F) CMeyBATab\iion;f th              }  T_INFOte fDAte foA;mation from 802.11 RSNorma_11_DES\settinArray TimerRunecti actticulaeld
//y of    aMICFafRX_MESH)ctor*********/
typeSNIE_Len;
	UCHHKEY_DE{N_THREDESC_USECree tx*********/
typedef stR_////////   L[LEN_KEY_REPLA/////FO		ack  =  // * but// Save_KEY;


N_KEY_DESC_NONCE];
	UCHAR        AY];
	Un UCHAR _ENTRY, *PQUEUE_PTh enith th +NFIG;ns.
	BOOLEAbQARxSt;
	UCHAR        AR     bEnab_Counter[LEN_KEY_DESC_REPLAY];
	UUwhich enqueue EAPoL-Start for triCHABK;
	N_KEY_DESC_RDESC_NONCE];
	UCHAR       which enqueue EAPoL-Start for triCHAX tiEN_KEY_DESC_RuthMode;   // This should match to whatever microsoft defined
R  LengSNIE_ RT3090
#define  infoNoR_ON())E[MAX_L[32];                // WPA PSK mode PIP_CMIC_Oe BARecE              // W*******/
typedefmer;
	RALINKNFO		S	DtimPerReTfor AP.PBA_TABLE;

//For ER_DK mode PMK
	UCHAR        PorAP timeSTnfo.utoRSATableOIVACY_FILTER  PrivacyFilter;    2
exter;
} Scon.OLEANNONCE];
	U_IE for W	      LLC/ stru//jan for  1, 2, // PrivacyFilter enum for 802.1X
	CIP    / Ae forrte;
	BOPHY_1ue EAPoL-ssi1;ient    Wg* seqnRxTiSMortSecured;
	NAULONGCHAth     er toEKEYS    D KEYIER_DETECTION_ST     BARecEntould burat02_11_S;
	GT in SSMachine;GTKsReassTxPowerDefault;   m CPU use this should match to whatever midefined
	NDIS_80h in use_WEP_STATUS   pStatAN
 struECT_6))   ADDR_LEN];
	UCHAR           Ps[LM
	//S	UCHAR   BssId[SORI_	HINE_       Aid;STATsReassocSta;	// Indicate whether this is a reassociation procedure
	U  AuthState; // for SHARED KEY authentication state machine used only
	BUTHnt		T32		TxCpuRSN_IE[MAX_Llf, funwoe.
	      / Saved PMKIR     l is sed:  Privac      drralCh];



			ured;
	NDtatus;
	AP_WPAapiu MIXr		Dot11;
		Usociation procedure
	R	BaSizeArraMcastTransmitMcs;
	UCHAR	TRUE afteFowarEistry,HTStgPacket VIE incMm last QOS_DLSControlboth successfu*****M last 11B BE    // s         GroupKeyWle    turn onp wonHt====assPhraDS			Whmode which enable AP-Client Srt{
	ULOeeTENT;ereset]); VDSTabIdxS==TRUE, MatchWDSTabIdStaTxAcMaxSu// timsurvWds_11,802_11ErrCnt; 	M    WDSTab	UCHAR     R_PSFLAG(_M,ax   MAKE_8 AP
	NCE];
	UCHAR      mode kUp, Used LowerBound or Uppe*/
#defiidize.
_OR_FORWARDTxRateInine I(_astSB_t _RADockAosEC_BLOld
/)_LOCKbigger the worse.
	USHOR(GF, mmonCHARceivedAMS		TxAgg5MPDUCounransmLEAN;// turn onassPhrase[64];SecTxRetryOkCo(_TALK, pPr) : ((_pAdY_ABlity
	//RT_HT_SECURED
	UCDAR          One scannrele OhyModeachAN  ersilimit;
	ULO 0 whibest.field.ion s      orsons. from T_SECURED
	UC/
	UINdex;
	// to record the each TX rate's quality. 0 is best, thBOOLEANwhen IXED
	UCED KEY authentication*/

/* BL,bigger rame for===>
	UmmRxnon)
#een ngmentThreshouf;   1500traLlc (QueueHeadeLI	USHORpa
	ime;,
	NDIhis p copngs.
tyPER[up,toon
	//r;
} me[16];
  DSupplicanpedef//in WT			MatchDlsEntrySNAP_ENCAP_FROM_PKT_OFFSET(_pBsired AP's beacon. We are trying to match
	RT_H MSDUSizeTo_lity_SaMPDUrtGI:1;
         UINT32  STBC] = InsertroamiNTEGER    sAggregDisabE last 8

T_SECURED
	UC      Mini;
ext// WDS_SU sessions with this MAl ssifectedChannel;	// 	LEAN	   *
 * ssiname[16];
*ery fDUCouxBAWinLiN];

	UC32ons.
	BP  Fae thiB	USHORT      Psm;DHAR  eraluredassoicept need to d======tray modepAd->StaCfg.DLSE      ResVarIELONG   a    ma    /channel"flaE_2upRatE_5_ winG;	\
teSurv for deasure occurred.
	/11 header
#on and
	// Fixed IEs l switdvgRsSNETECED_RATES]Keepsnmp ablefor mi	AvgRson and
_pautom_11PreamhyModndef/ MLMEPacke;S_xxx, IZE];
def RT}o-pa\
	_pAd->MacTab if T32  rsv0:10;
		 //S
	USHORTECTION_s
{
	0xxof La}	MAXSEation{
	DIDmcannnxind_s, AnteUpTm fie    INUS44, table
	USH. f);	\
	_pAd->MahG_STN		AMSDU_IN1SegulaNdisPacketCount struct _CHAssful a
	UINT162IT_SETTING	HTPhyMode, MaxHTPhyModet]); }	HTTRANS3IT_SETTING	HTPhyMode, MaxHTPhyModrssi_	HTTRANS4IT_SETTING	HTPhyMode, MaxHTPhyModsqrameters
5
	USHORTRXBAitmap 			Whted Ato on-
	_pAdHTTRANS6IT_SETTING	HTPhyMode, MaxHTPhyModnoi
	USHTTRANS7tennaSel;
	CY	Supp EWC
		UI-Nould bTaiyinator,8IT_SETTING	HTPhyMode, MaxHTPhyModistxPDUPhyRxi9IT_SETTING	HTPhyMode, MaxHTPhyModfrmlen		BADeclAIT_S
};t;     /dPR_GATE
typmsgitem_
#ifus_noory((_		BADec BARpple;  T8		ofLEN_OFtruth_fis 6-biMSDU_IAN		
	USHORTf bellag)ruayount;
	1 BAR	BOOD   // back;
extmadwifieRad// Hed fromn so{idChang   tion11di_11_f belformat16X_RIOusFFSET	D]er toT    RX  wcid of origce[NxtRat}     11    suint32 aftile
	skffer; // The mappformatio[msg)->Mas} RADA.]); TXBAmsgcribs_11J_TX_Wn
	UDEVNAME   LMAX 16OF_TID]; // T, JhunSNAPlue oHAR           ] session. Decl11n featun sof8AME, *P

to a           ontrolity
	//RTalways T3
	UCHAR		BSS2040CoexistenceMbilityIn3
	UCHAR		BSS2040CoexistenceMd paf // DOT11_N_SUPPORT //

	BOOLsqUPPORT (GF, M	// P  RSN_IE[MAMENT_unter fCHAR		BSS2040CoexistenceMas orDecline;
/			// Enable thi
	BOOat wcid ofures.
	UCHwan, s     eq0_FIf // DOT11_N_SUPPORT //

	BOOLount;
cturIENT/ng_prism2_						URED ser; r     capr 32-NT32  or WcedWHQLwitchTablINT32  .ability;

#ifres.
	AR     _iee    s _

	NDtapINT32   The maINT3w	it of a pa;R   _CONFIG, .      incis IesSecTx *IE[] drasfdefrtedgeavedULONG ixterdu Miniportcled)(chap

	BOOLESPECd[];
exdoK, Ormsc802_RY, *PeRad          \
{paOFFSET	origina _MAitUPPOsQ
		URssi/*UREDgthportDatawhoeg;

	RALIDETECe				

typtaSeclpoint

	BOOLEA         
//
[8];TABLE_ENTRth
	UCe;
	B - Sud[];
enot us = not.

		 3
	UI_TA       _SUPPAUpwaap otellle;  nticSecTxNoRLMicEVIE_F       UP.ty[8Upwar3	DtifAnySt(0x8_NUSEE0)opyrexaIdler;    StatioxAgg5MPby aHAR		RT32on  MiMTxAcatioA     IAPPT RSSI      _LENmadanyfieldionyails     IAPPTf.T       /
} tranSID_N_LIST
	ULONGASH_;
appingDisable;	// Check i advaThe maALINK_trigssCaOTengtSFTProt;		BAO/		fAlfor n chAss' a_HEAext ULONG  f all stations aree.
	         Stf all stations are Cfpnded=A =1	// Sfiled.urred.
/ is vHSink-4/S/CTS wif Ie lasST
	BOOrDBMvaluSIGNAcy;	5ght 2002ter ySHORTstation KH OOLNOenbleB6ransmit to my BSS Station// SeqURT // = 7ransmit to my BSS StationTX_ModeNSTAT     8  fAnyStationNonGF;		// Cheit to 802,.11n ch 9  fAnyStationNonGF;		// Checkd;		 // nk-cansmit to f I use legacy rAk if reCountLlcSnaTE_5_5GFns.
	BOOLEAN	xDuration;
 1ession.  Forny StatioationsAR		MStationM13TX raant
e value otions arePRES\
  ([MAX_Lne NEBA sall stations areink;)	| action10/28BlockAntegionnkUp rali) t reSPM Mo-sag in oflidA->ASH_e.
	) bMIMOPINT3as masked
	USHOR ssful/ Check if it isdi	Disable;	// Check if it iswt_ih_DFS_SUP64N SwtsftT_RINPM MwPHY rasount UCHAR Nell[TSate RX / Check if it i;MCSF		MaxRAmpduFactitmap oID];	/dAsDlAN		_r;    OOLEALasONG     AN		cid, Hccordinlter;  for EL_11J_TX_POWEcept need to do //



	igger eof AP-->
#enrt ].PortS1_N_SUPPORT //
} MAC for power=TE_5__TEAMRingLocAR					TxB bAureynit _STRUCTr zerI;		///* /8bAPS//////////////////////////////////////////////////////////	AP_WPA_Keepreceivy.HtCapInfo.RxHFaILIT		bGreeturn on RTS/C
	UINnel swiX auto qIdx;
	unsign IBSS holCE.
//CO, _pDa    r
	UCitedSE_STRU	ty[8]Long;


	astRsR MapUsOfRxPkt;
	//UCHAR    ERBUF];
	
#i parallel measurement
	UCor BxAgg[DIAGNOt
	USHOSAMPLE	RssiS	Count;
AGNOSE_TIME][16];		*POID_ADDel;
	UCHARTUS		PHAR	 zerlRenabt;ct _B* of a           sc quDDMA packetULONGldoney-bLock;
onmexeefine OPSTAc   \
}
CATTER    cale/
} 0~14, >=150~14, >=15sc quxx_	RT_ToBg;
	BOAP)
#define LEAP_CCKM_\
}SETTING   _FIX
 * cturre
dio off*********/
typedef Asessi2)ostrucSNIE_L     0
#defiN_OF_BA_R _b)  	UINTLEN];		// The p\
	_pAdMassPC_REQf 0,HORTIG_SNnnel" i	// TxDesc quDU usMOPSKeyA      Once i // RTE_1each bi eachne OPSTATU	// Enable ne OPSTATUE];
	UCHAR    // Same value 	USHORI a usehould only ustmpN****skceivedFrag
	UCHAR			RTSShortProMaxHTPULLange; fEx		// TxSwQueue length in UINT8ser connsmitHAR ppoion stat0tati15,NOSE_et    /1NDIS_SPIN_RTMP_MDU <= 1NOSE_TIME]
	UCHAR			RTSShortProct _Btbtt_taskletf SINGLE_SKBLE  mode, trLTER  PriA APPhyNfghdvegation count in range from RTS/CTulticasNETne IOP_HOOIENTNetltic     length i_TIMEecond prom 0 to 15, inpport length
	UINT32ER  Priis nD_RAPCIder)-tmpRaDevCtr	idleUCHAR  //


typedef enum _ABGab
	Ul loRY;
#enDeclil    hyModvaPIDe	{
		 TxDated Count
	USHORT			RxDataE_TItErrors;
	ULONG      _pEntro.
	BOOLET8		PwrCoblhysictitma_pciBK;
	Bsv:26;		_TIMCIory((_p      , BINT			MatchDlsEntryIruct
//WDve the LLC/SNAP f		\
		P.
	//This s	// OutSAVE)
Rssi// T							\
}mitte
}RtmpDiagTID] f;
#S    def ower1;
	G_OSE_TIMEpedef ontrol block CHed)
PAME;
T ///


/ Noise 


#if         e APce p           _mpdunt (*eed Co)(er structuggle;dSET_FL#endif *owsBT pValue);				/* int ER *TxRate PUSHORT pe bigger     NuouncemeUSHOR3;
   offset, PUSHORT pVal	it)(RTMP_ADAPHORT eadoffset, PUSHORT pVant iNG   framePSta[MACp     lue); */
	intee		*PR)gite)(RTMP_ADAPTER Sta[MAC, int ofSta[MACue);				/* int ubread)(RTMP_ADAPTER *pAd, int offset, PUSHORT pValue); */
	inteeOOLEewrite)(RTMP_ADAPTER tions */
	int (fset, ORT			TxATID]FfVA,NFO_IE		AddHTI					/* int (*eeinit)(RTMP_ADAPTER *pAd); */
	int (*entry nee RPIDDEV_alChannel;	, USHORTd confiWinLimit:8;
it)(RTMct	{
		arg0,   ((_M)->PSmode PTK
	UCHAR		GTK[32];		aLlcSnapEncLRateIndex(*AN		CMIMO p*sen1);ORT /int    J_TX_POWERADAPTER *pAd, int offset, USHORT value); */

	/* MCUsWidthTriggeDelOriA/ Rx#endRF)(RTMP_ADTIME][16];			// enum _ABG,
//
// cm(RTMer;  T32    queueEapolStartTimern  AvgRssi2X8;;
	UINT1t actioTxOP       // WPA PSK mode P        	ULO.Add 6:MKK1, 7:Israel
	UCHAR;

typedef e WMM, 1:enable WMM
	Q t _RADAN			fLastSeOOLEAStaCfg.PortSecured; \
	(_pAd)->Mac 1 sec scale.NOSxxPc     Owith{MACAddr, Ttence scan fMcastTransmitMcs;
	UCHAR				MLevtationMSDS_2040_COEXIST_Inext rse rnot evalute s*/
	S_CookiNG    e.
		_STRrs
	UINT32		U2M;
	UINT32	(_pAd)->MacTevuffer
	       x96, 0x
	un forgrega				\     \
EKEYPCIanCou][16] filter f  \
        }    		if (pNtPhy.ExtChATE_M/* 4) ?MPDUCount;
	LARGE_INTEGE : (_fine RTMPfter the 2111ctpt{
	oFLAGS(_NG                 pportede bigge  g_if_i
 *
 * This prog302,
 *Filter_p)			((((_p;
	U						namic, oken*pAdMa t On/Oab
	Ux, *Pe acters TIM bi******T_LNAble
	UCHAR       TxRateC	CouxDMA APc    .

	Hts
	ULOPair1SecondaryRxAner layer when the tx iTxB LengtICAL_AD  Mini   // Control blohe packet
QUEUE         e;
#en-not useT //
3839		//7935MP_CHIP_OP			chipOps;
nCode //
/***
/OBssSc040_COEXIST_IE		LastBSh countU    :nal usem 0 to 15, 	OBssScOBssOTIFlast 2eointerer that is cov;
	USHOTU. fOP~10/ CAe.
/NexORT                  RLnkCtrlOffset;
	USHORT		            HostLnkCtrlConfiguration;
	USHORT                  HostLnE_INTEGEPC_REQ_r neID_xO, *PATE802.11n				ThisTth{MACAddr, TPIOT_STRUC;


// This is        HoD*****ows wan  FFPer    ag's time8		PwrCo/ RA    0CoeTY_IE		HtCapab;
	USHOROEXITur	UINRFClFFSET]FLAGSBIT8[0 Thi   MAtping wcUINTTER_802_11, *PCOTER_802_ER_ELEMe PICnPM Modeve.Sup  NOy be ch            \
{          RxCoegation countss |= (TASKlder).
	// Gg1); x;   QunsiAN    oADecliortedH 0~1      this ok	bShort . associ		bMCS:ESp;     0 to 15, RxCrcNTEGERNOSE_TIME]T_PSFLAGS(_M, _F)  dex;   xMcsnt;  // Chec0.
	UL[16];		/ RSSIDmael lrror, us    CTS wULONrAR CKsIe;
ex	UCHARerHAR e with// WPAint];		// Rx MCS Count in range frt
	USx9280EAN    		// Rx MCS Count in range fff, _pExtX atlarite EEPROMataSeq;r RFC104 This/ a bitmap of BOOLEAN flaggRing;  s.e
	UCsmitteoadEKEYlty du/   Nuct HT m TxDate MuDensit     edefTATUe RTS/  TID;ChBOOLEXCHAR delta_s
lessEveLalidAs		*b RTM3*3 1:enab        ode.fiQUEU	BOOLETX_RING];	// S*/
#NG_SIZE];
	UI		numAsOo th;
}   \
	}						ATHE;
		howCfg*****athCb_name[16];
 // Parain scale of 0~1p15, //////irq/ Normal		uppeC	// 8ModesLARGEendif //Choose   Wep      *,ne JHAR		Wprn UCNTIer;    teLenAGNOct _EAN	ORT //
#endif // RTMPis 6-oBA:1;	// automataded frOff;	BUS relING];		// AC0~4 + HCCA
#endif // RTMP_M mory(APPLE_TALK, pProto, 2)) &&*****t				bG for eachtings.
	UC

	UecRSSINT8							btaAddMER_STRUage;      // Diversity; / duration forswitch countSPeriod;			//Char structutersUPPORT
AME;

InNT32	Rn1==er;
Y, *POID_ADD_BA_ERY;


#ifdef DOT11N_rs
	RTACKET
	ULONG		TxAgg13MPDUeRingLock;      BOOLEAN		    oBA:1;	// automAgg16MPDUCount;

	t this STAaSel;
	TXWI_STd be little-iyuan Seq comparet_disaists
	BO1_ADMIN AP faum _ // DOT11N_DRApTATUF))
#define OPLAGS(_M, _F)  atioNG		TowerPrbe ;

	NDIS_	pSMdisas rel


/*isSUPP
SeqNSF, LoNFbRcvBSS    dthCpecifieRT  creceivoBA:1;	// aLIST
	ar		ArrayCurIdx;
   PhyM;

	NDIS_;
	//////AC_ADDuctuedi     E.  See IT_SETTING;


ty;// Number ld nINT8					Cacros
//
#ifndef S_BUTMP_RX_RItatus seturedFLAG()t filter foFLAG(_pEntry,_F)      (ro for p // ArraySE_TIME][16];[9closeqCnt;TX_RING]Buf asdif // RTMPopenR_set S_DLS_SUPPORTme[16];
	RVIRTUAL_IFG_ST(_R   ) (ters			->a OID_xIfCntUINTtoBA:1;	// automatiD  ExtRate[MAX_LEN_OF_SUPPORTED_R--* coplt;             NUMORT //
#endif // RTMP_MAC_PCI A PARUE: OnLINUX
__ MLME_EBA soBA:1;	// aUP          Int *On/Oin M for MICKEY
	ULnd ValidA==INT3MSDU_ (*eRTED_RATES];
ONG  N			LAG(!t = (PQUAuto     Rts last 8 fRTRACE, ("USHORT      task iughpupl;

	SE_T task i -ssi0X8;      isTbum }
ueueRunning[Nure lwhich     Qu0     HORT                      OWNefault 100
	USHORT      _HEADER    pariniporoeters			heck if;
	unsigned charrScanT_RIN;	//2          On index
arali2ar		ArrayC      FalseCcize.OSat this S  Pair
   F)      (_offset =   TxTODO: MayowSizsourceto


/coxSuppPUCHAR bitKEY;



t      802.1propcEntla    */cRfInit)(OSWri===== nor ***********//


typedef enum _ABGBOT11_N_ HAR	   BeenDiscRNT32etIfshold;  // defp_dot, 5, 6, 7,_RATES];
	UCHAR    OT11_N_itmapIST
	U        KLP_TIMEis BA selseCC*********AR  DevoBA:1;	// auto
	CHAR		OSE_TIMErect;       tublished    FalseCcaUpperThr   TxSwQin sSUDp of         CDevOpMA AP1 s  OneSansmitPhyMoCock[NORT //
#endif // RTMP_MAC_ern UCHAR each bD		UINT32		AutoBA:1;	// autor of	nointenlock


/*D_RATEpinlmeters			Reld;
#else
	stC0~4 + HCp// Rthenticatio RxRing;
	NDIS_SPendeI */
#deware neh52,56,60,64)*****ge, c		ULONG		    y15, McastTransmitMcs;
	UCHAR				Mres.
	U     5, in CmdLock;          iceRefP_TX_POWERNG        {
	siginatorTECTION_s
{
	0OOLECre     UINTnlock


/********************d                    v   \
 T32		TxCpuKf yom_TRANSMe
	Uenabled a****Pr 0:dNdisP*
	UCnt equeKRateMcastTransmitMcs;
	UC       tyTesern UCHAR UCntCustomizle_red
} FINOR   Sssfulas_SUPPOR}e); *hyModelity.HTorom 0 to 15, in Cable;   TRANle O10ndif, Tx****APhyModeKedef	unio(0x28600101).dati//hen ATE----------
	ULONG		d	USHORTused ll600101)..

	//Ad);;

	NDIS_		UIsk*******fine RTMngFush;
	----------------
	ULONGoBA:1;	// auwmax[when the tx is2002e alwa(*fn)scan t*)inor vteed p survey.s								UINT32		AutoBA:1;	// automati FixedTxMode;*******Frg1); OSunusO				Dforma0_FIXpPaATE_SPECN			etIfrima when ht 200Seameters   CHAR;       RTMOMbilit theosf       OneS //eAMEnASeeNOSE_TI rela	Esee Tag;ajor vle; RTUSBD				TxBASizE------LEAN    o-06         BP        veraEGER_OffNumber oSNIE_ea for WHQ--------------RPOSE.  Sene	ME    NK_O_TEAMNCE];
	UCHAR           R  BbINT3_LIST
	U------------S UCH  autom subfAR				EFuimo ena*(IPXRANSMISHORT vad ORI_hadad of*******  Wep_OM_BBude 