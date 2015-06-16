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

#include "spectrum_def.h"

#include "aironet.h"

#define VIRTUAL_IF_INC(__pAd) ((__pAd)->VirtualIfCnt++)
#define VIRTUAL_IF_DEC(__pAd) ((__pAd)->VirtualIfCnt--)
#define VIRTUAL_IF_NUM(__pAd) ((__pAd)->VirtualIfCnt)

#ifdef RT2870
////////////////////////////////////////////////////////////////////////////
// The TX_BUFFER structure forms the transmitted USB packet to the device
////////////////////////////////////////////////////////////////////////////
typedef struct __TX_BUFFER{
	union	{
		UCHAR			WirelessPacket[TX_BUFFER_NORMSIZE];
		HEADER_802_11	NullFrame;
		PSPOLL_FRAME	PsPollPacket;
		RTS_FRAME		RTSFrame;
	}field;
	UCHAR			Aggregation[4];  //Buffer for save Aggregation size.
} TX_BUFFER, *PTX_BUFFER;

typedef struct __HTTX_BUFFER{
	union	{
		UCHAR			WirelessPacket[MAX_TXBULK_SIZE];
		HEADER_802_11	NullFrame;
		PSPOLL_FRAME	PsPollPacket;
		RTS_FRAME		RTSFrame;
	}field;
	UCHAR			Aggregation[4];  //Buffer for save Aggregation size.
} HTTX_BUFFER, *PHTTX_BUFFER;


// used to track driver-generated write irps
typedef struct _TX_CONTEXT
{
	PVOID			pAd;		//Initialized in MiniportInitialize
	PURB			pUrb;			//Initialized in MiniportInitialize
	PIRP			pIrp;			//used to cancel pending bulk out.
									//Initialized in MiniportInitialize
	PTX_BUFFER		TransferBuffer;	//Initialized in MiniportInitialize
	ULONG			BulkOutSize;
	UCHAR			BulkOutPipeId;
	UCHAR			SelfIdx;
	BOOLEAN			InUse;
	BOOLEAN			bWaitingBulkOut; // at least one packet is in this TxContext, ready for making IRP anytime.
	BOOLEAN			bFullForBulkOut; // all tx buffer are full , so waiting for tx bulkout.
	BOOLEAN			IRPPending;
	BOOLEAN			LastOne;
	BOOLEAN			bAggregatible;
	UCHAR			Header_802_3[LENGTH_802_3];
	UCHAR			Rsv[2];
	ULONG			DataOffset;
	UINT			TxRate;
	dma_addr_t		data_dma;		// urb dma on linux

}	TX_CONTEXT, *PTX_CONTEXT, **PPTX_CONTEXT;


// used to track driver-generated write irps
typedef struct _HT_TX_CONTEXT
{
	PVOID			pAd;		//Initialized in MiniportInitialize
	PURB			pUrb;			//Initialized in MiniportInitialize
	PIRP			pIrp;			//used to cancel pending bulk out.
									//Initialized in MiniportInitialize
	PHTTX_BUFFER	TransferBuffer;	//Initialized in MiniportInitialize
	ULONG			BulkOutSize;	// Indicate the total bulk-out size in bytes in one bulk-transmission
	UCHAR			BulkOutPipeId;
	BOOLEAN			IRPPending;
	BOOLEAN			LastOne;
	BOOLEAN			bCurWriting;
	BOOLEAN			bRingEmpty;
	BOOLEAN			bCopySavePad;
	UCHAR			SavedPad[8];
	UCHAR			Header_802_3[LENGTH_802_3];
	ULONG			CurWritePosition;		// Indicate the buffer offset which packet will be inserted start from.
	ULONG			CurWriteRealPos;		// Indicate the buffer offset which packet now are writing to.
	ULONG			NextBulkOutPosition;	// Indicate the buffer start offset of a bulk-transmission
	ULONG			ENextBulkOutPosition;	// Indicate the buffer end offset of a bulk-transmission
	UINT			TxRate;
	dma_addr_t		data_dma;		// urb dma on linux
}	HT_TX_CONTEXT, *PHT_TX_CONTEXT, **PPHT_TX_CONTEXT;


//
// Structure to keep track of receive packets and buffers to indicate
// receive data to the protocol.
//
typedef struct _RX_CONTEXT
{
	PUCHAR				TransferBuffer;
	PVOID				pAd;
	PIRP				pIrp;//used to cancel pending bulk in.
	PURB				pUrb;
	//These 2 Boolean shouldn't both be 1 at the same time.
	ULONG				BulkInOffset;	// number of packets waiting for reordering .
	BOOLEAN				bRxHandling;	// Notify this packet is being process now.
	BOOLEAN				InUse;			// USB Hardware Occupied. Wait for USB HW to put packet.
	BOOLEAN				Readable;		// Receive Complete back. OK for driver to indicate receiving packet.
	BOOLEAN				IRPPending;		// TODO: To be removed
	atomic_t			IrpLock;
	NDIS_SPIN_LOCK		RxContextLock;
	dma_addr_t			data_dma;		// urb dma on linux
}	RX_CONTEXT, *PRX_CONTEXT;
#endif // RT2870 //


//
//  NDIS Version definitions
//
#ifdef  NDIS50_MINIPORT
#define RTMP_NDIS_MAJOR_VERSION     5
#define RTMP_NDIS_MINOR_VERSION     0
#endif

#ifdef  NDIS51_MINIPORT
#define RTMP_NDIS_MAJOR_VERSION     5
#define RTMP_NDIS_MINOR_VERSION     1
#endif

extern  char    NIC_VENDOR_DESC[];
extern  int     NIC_VENDOR_DESC_LEN;

extern  unsigned char   SNAP_AIRONET[];
extern  unsigned char   CipherSuiteCiscoCCKM[];
extern  unsigned char   CipherSuiteCiscoCCKMLen;
extern	unsigned char	CipherSuiteCiscoCCKM24[];
extern	unsigned char	CipherSuiteCiscoCCKM24Len;
extern  unsigned char   CipherSuiteCCXTkip[];
extern  unsigned char   CipherSuiteCCXTkipLen;
extern  unsigned char   CISCO_OUI[];
extern  UCHAR	BaSizeArray[4];

extern UCHAR BROADCAST_ADDR[MAC_ADDR_LEN];
extern UCHAR MULTICAST_ADDR[MAC_ADDR_LEN];
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

extern UCHAR  HtCapIe;
extern UCHAR  AddHtInfoIe;
extern UCHAR  NewExtChanIe;

extern UCHAR  ErpIe;
extern UCHAR  DsIe;
extern UCHAR  TimIe;
extern UCHAR  WpaIe;
extern UCHAR  Wpa2Ie;
extern UCHAR  IbssIe;
extern UCHAR  Ccx2Ie;

extern UCHAR  WPA_OUI[];
extern UCHAR  RSN_OUI[];
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

extern UCHAR  RateSwitchTable11BGN1S[];
extern UCHAR  RateSwitchTable11BGN2S[];
extern UCHAR  RateSwitchTable11BGN2SForABand[];
extern UCHAR  RateSwitchTable11N1S[];
extern UCHAR  RateSwitchTable11N2S[];
extern UCHAR  RateSwitchTable11N2SForABand[];

extern UCHAR  PRE_N_HT_OUI[];

#define	MAXSEQ		(0xFFF)

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
	int 	qlen;
};

struct reordering_mpdu_pool
{
	PVOID					mem;
	NDIS_SPIN_LOCK			lock;
	struct reordering_list 	freelist;
};

typedef struct 	_RSSI_SAMPLE {
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

#define InsertTailQueue(QueueHeader, QueueEntry)                \
{                                                               \
	((PQUEUE_ENTRY)QueueEntry)->Next = NULL;                    \
	if ((QueueHeader)->Tail)                                    \
		(QueueHeader)->Tail->Next = (PQUEUE_ENTRY)(QueueEntry); \
	else                                                        \
		(QueueHeader)->Head = (PQUEUE_ENTRY)(QueueEntry);       \
	(QueueHeader)->Tail = (PQUEUE_ENTRY)(QueueEntry);           \
	(QueueHeader)->Number++;                                    \
}

//
//  Macros for flag and ref count operations
//
#define RTMP_SET_FLAG(_M, _F)       ((_M)->Flags |= (_F))
#define RTMP_CLEAR_FLAG(_M, _F)     ((_M)->Flags &= ~(_F))
#define RTMP_CLEAR_FLAGS(_M)        ((_M)->Flags = 0)
#define RTMP_TEST_FLAG(_M, _F)      (((_M)->Flags & (_F)) != 0)
#define RTMP_TEST_FLAGS(_M, _F)     (((_M)->Flags & (_F)) == (_F))

#ifdef RT2860
// Macro for power save flag.
#define RTMP_SET_PSFLAG(_M, _F)       ((_M)->PSFlags |= (_F))
#define RTMP_CLEAR_PSFLAG(_M, _F)     ((_M)->PSFlags &= ~(_F))
#define RTMP_CLEAR_PSFLAGS(_M)        ((_M)->PSFlags = 0)
#define RTMP_TEST_PSFLAG(_M, _F)      (((_M)->PSFlags & (_F)) != 0)
#define RTMP_TEST_PSFLAGS(_M, _F)     (((_M)->PSFlags & (_F)) == (_F))
#endif

#define OPSTATUS_SET_FLAG(_pAd, _F)     ((_pAd)->CommonCfg.OpStatusFlags |= (_F))
#define OPSTATUS_CLEAR_FLAG(_pAd, _F)   ((_pAd)->CommonCfg.OpStatusFlags &= ~(_F))
#define OPSTATUS_TEST_FLAG(_pAd, _F)    (((_pAd)->CommonCfg.OpStatusFlags & (_F)) != 0)

#define CLIENT_STATUS_SET_FLAG(_pEntry,_F)      ((_pEntry)->ClientStatusFlags |= (_F))
#define CLIENT_STATUS_CLEAR_FLAG(_pEntry,_F)    ((_pEntry)->ClientStatusFlags &= ~(_F))
#define CLIENT_STATUS_TEST_FLAG(_pEntry,_F)     (((_pEntry)->ClientStatusFlags & (_F)) != 0)

#define RX_FILTER_SET_FLAG(_pAd, _F)    ((_pAd)->CommonCfg.PacketFilter |= (_F))
#define RX_FILTER_CLEAR_FLAG(_pAd, _F)  ((_pAd)->CommonCfg.PacketFilter &= ~(_F))
#define RX_FILTER_TEST_FLAG(_pAd, _F)   (((_pAd)->CommonCfg.PacketFilter & (_F)) != 0)

#define STA_NO_SECURITY_ON(_p)          (_p->StaCfg.WepStatus == Ndis802_11EncryptionDisabled)
#define STA_WEP_ON(_p)                  (_p->StaCfg.WepStatus == Ndis802_11Encryption1Enabled)
#define STA_TKIP_ON(_p)                 (_p->StaCfg.WepStatus == Ndis802_11Encryption2Enabled)
#define STA_AES_ON(_p)                  (_p->StaCfg.WepStatus == Ndis802_11Encryption3Enabled)

#define STA_TGN_WIFI_ON(_p)             (_p->StaCfg.bTGnWifiTest == TRUE)

#define CKIP_KP_ON(_p)				((((_p)->StaCfg.CkipFlag) & 0x10) && ((_p)->StaCfg.bCkipCmicOn == TRUE))
#define CKIP_CMIC_ON(_p)			((((_p)->StaCfg.CkipFlag) & 0x08) && ((_p)->StaCfg.bCkipCmicOn == TRUE))


#define INC_RING_INDEX(_idx, _RingSize)    \
{                                          \
    (_idx) = (_idx+1) % (_RingSize);       \
}

#ifdef RT2870
// We will have a cost down version which mac version is 0x3090xxxx
#define IS_RT3090(_pAd)				((((_pAd)->MACVersion & 0xffff0000) == 0x30710000) || (((_pAd)->MACVersion & 0xffff0000) == 0x30900000))
#else
#define IS_RT3090(_pAd)				0
#endif
#define IS_RT3070(_pAd)				(((_pAd)->MACVersion & 0xffff0000) == 0x30700000)
#ifdef RT2870
#define IS_RT3071(_pAd)				(((_pAd)->MACVersion & 0xffff0000) == 0x30710000)
#define IS_RT30xx(_pAd)				(((_pAd)->MACVersion & 0xfff00000) == 0x30700000)
#endif

#define RING_PACKET_INIT(_TxRing, _idx)    \
{                                          \
    _TxRing->Cell[_idx].pNdisPacket = NULL;                              \
    _TxRing->Cell[_idx].pNextNdisPacket = NULL;                              \
}

#define TXDT_INIT(_TxD)    \
{                                          \
	NdisZeroMemory(_TxD, TXD_SIZE);	\
	_TxD->DMADONE = 1;                              \
}

//Set last data segment
#define RING_SET_LASTDS(_TxD, _IsSD0)    \
{                                          \
    if (_IsSD0) {_TxD->LastSec0 = 1;}     \
    else {_TxD->LastSec1 = 1;}     \
}

// Increase TxTsc value for next transmission
// TODO:
// When i==6, means TSC has done one full cycle, do re-keying stuff follow specs
// Should send a special event microsoft defined to request re-key
#define INC_TX_TSC(_tsc)                                \
{                                                       \
    int i=0;                                            \
    while (++_tsc[i] == 0x0)                            \
    {                                                   \
        i++;                                            \
        if (i == 6)                                     \
            break;                                      \
    }                                                   \
}

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
// BBP & RF are using indirect access. Before write any value into it.
// We have to make sure there is no outstanding command pending via checking busy bit.
//
#define MAX_BUSY_COUNT  100         // Number of retry before failing access BBP & RF indirect register
//
#ifdef RT2860
#define RTMP_RF_IO_WRITE32(_A, _V)                  \
{                                                   \
    PHY_CSR4_STRUC  Value;                          \
    ULONG           BusyCnt = 0;                    \
    if ((_A)->bPCIclkOff) 	                \
    {												\
        return;										\
    }                                               \
    do {                                            \
        RTMP_IO_READ32(_A, RF_CSR_CFG0, &Value.word);  \
        if (Value.field.Busy == IDLE)               \
            break;                                  \
        BusyCnt++;                                  \
    }   while (BusyCnt < MAX_BUSY_COUNT);           \
    if (BusyCnt < MAX_BUSY_COUNT)                   \
    {                                               \
        RTMP_IO_WRITE32(_A, RF_CSR_CFG0, _V);          \
    }                                               \
}

#define BBP_IO_READ8_BY_REG_ID(_A, _I, _pV)        \
{                                                       \
    BBP_CSR_CFG_STRUC  BbpCsr;                             \
    int             i, k;                               \
    for (i=0; i<MAX_BUSY_COUNT; i++)                    \
    {                                                   \
        RTMP_IO_READ32(_A, BBP_CSR_CFG, &BbpCsr.word);     \
        if (BbpCsr.field.Busy == BUSY)                  \
        {                                               \
            continue;                                   \
        }                                               \
        BbpCsr.word = 0;                                \
        BbpCsr.field.fRead = 1;                         \
        BbpCsr.field.BBP_RW_MODE = 1;                         \
        BbpCsr.field.Busy = 1;                          \
        BbpCsr.field.RegNum = _I;                       \
        RTMP_IO_WRITE32(_A, BBP_CSR_CFG, BbpCsr.word);     \
        for (k=0; k<MAX_BUSY_COUNT; k++)                \
        {                                               \
            RTMP_IO_READ32(_A, BBP_CSR_CFG, &BbpCsr.word); \
            if (BbpCsr.field.Busy == IDLE)              \
                break;                                  \
        }                                               \
        if ((BbpCsr.field.Busy == IDLE) &&              \
            (BbpCsr.field.RegNum == _I))                \
        {                                               \
            *(_pV) = (UCHAR)BbpCsr.field.Value;         \
            break;                                      \
        }                                               \
    }                                                   \
    if (BbpCsr.field.Busy == BUSY)                      \
    {                                                   \
        DBGPRINT_ERR(("DFS BBP read R%d fail\n", _I));      \
        *(_pV) = (_A)->BbpWriteLatch[_I];               \
    }                                                   \
}

//#define RTMP_BBP_IO_READ8_BY_REG_ID(_A, _I, _pV)    {}
// Read BBP register by register's ID. Generate PER to test BA
#define RTMP_BBP_IO_READ8_BY_REG_ID(_A, _I, _pV)        \
{                                                       \
    BBP_CSR_CFG_STRUC  BbpCsr;                             \
    int             i, k;                               \
    if ((_A)->bPCIclkOff == FALSE)                     \
    {                                                   \
    for (i=0; i<MAX_BUSY_COUNT; i++)                    \
    {                                                   \
		RTMP_IO_READ32(_A, H2M_BBP_AGENT, &BbpCsr.word);				\
        if (BbpCsr.field.Busy == BUSY)                  \
        {                                               \
            continue;                                   \
        }                                               \
        BbpCsr.word = 0;                                \
        BbpCsr.field.fRead = 1;                         \
        BbpCsr.field.BBP_RW_MODE = 1;                         \
        BbpCsr.field.Busy = 1;                          \
        BbpCsr.field.RegNum = _I;                       \
		RTMP_IO_WRITE32(_A, H2M_BBP_AGENT, BbpCsr.word);				\
		AsicSendCommandToMcu(_A, 0x80, 0xff, 0x0, 0x0);					\
		RTMPusecDelay(1000);							\
        for (k=0; k<MAX_BUSY_COUNT; k++)                \
        {                                               \
			RTMP_IO_READ32(_A, H2M_BBP_AGENT, &BbpCsr.word);			\
            if (BbpCsr.field.Busy == IDLE)              \
                break;                                  \
        }                                               \
        if ((BbpCsr.field.Busy == IDLE) &&              \
            (BbpCsr.field.RegNum == _I))                \
        {                                               \
            *(_pV) = (UCHAR)BbpCsr.field.Value;         \
            break;                                      \
        }                                               \
    }                                                   \
    if (BbpCsr.field.Busy == BUSY)                      \
    {                                                   \
		DBGPRINT_ERR(("BBP read R%d=0x%x fail\n", _I, BbpCsr.word));	\
        *(_pV) = (_A)->BbpWriteLatch[_I];               \
		RTMP_IO_READ32(_A, H2M_BBP_AGENT, &BbpCsr.word);				\
		BbpCsr.field.Busy = 0;                          \
		RTMP_IO_WRITE32(_A, H2M_BBP_AGENT, BbpCsr.word);				\
    }                                                   \
    }                   \
}

#define BBP_IO_WRITE8_BY_REG_ID(_A, _I, _V)        \
{                                                       \
    BBP_CSR_CFG_STRUC  BbpCsr;                             \
    int             BusyCnt;                            \
    for (BusyCnt=0; BusyCnt<MAX_BUSY_COUNT; BusyCnt++)  \
    {                                                   \
        RTMP_IO_READ32(_A, BBP_CSR_CFG, &BbpCsr.word);     \
        if (BbpCsr.field.Busy == BUSY)                  \
            continue;                                   \
        BbpCsr.word = 0;                                \
        BbpCsr.field.fRead = 0;                         \
        BbpCsr.field.BBP_RW_MODE = 1;                         \
        BbpCsr.field.Busy = 1;                          \
        BbpCsr.field.Value = _V;                        \
        BbpCsr.field.RegNum = _I;                       \
        RTMP_IO_WRITE32(_A, BBP_CSR_CFG, BbpCsr.word);     \
        (_A)->BbpWriteLatch[_I] = _V;                   \
        break;                                          \
    }                                                   \
    if (BusyCnt == MAX_BUSY_COUNT)                      \
    {                                                   \
        DBGPRINT_ERR(("BBP write R%d fail\n", _I));     \
    }                                                   \
}

// Write BBP register by register's ID & value
#define RTMP_BBP_IO_WRITE8_BY_REG_ID(_A, _I, _V)        \
{                                                       \
    BBP_CSR_CFG_STRUC  BbpCsr;                             \
    int             BusyCnt;                            \
    if ((_A)->bPCIclkOff == FALSE)                     \
    {                                                   \
    for (BusyCnt=0; BusyCnt<MAX_BUSY_COUNT; BusyCnt++)  \
    {                                                   \
		RTMP_IO_READ32(_A, H2M_BBP_AGENT, &BbpCsr.word);				\
        if (BbpCsr.field.Busy == BUSY)                  \
            continue;                                   \
        BbpCsr.word = 0;                                \
        BbpCsr.field.fRead = 0;                         \
        BbpCsr.field.BBP_RW_MODE = 1;                         \
        BbpCsr.field.Busy = 1;                          \
        BbpCsr.field.Value = _V;                        \
        BbpCsr.field.RegNum = _I;                       \
		RTMP_IO_WRITE32(_A, H2M_BBP_AGENT, BbpCsr.word);				\
		AsicSendCommandToMcu(_A, 0x80, 0xff, 0x0, 0x0);					\
            if (_A->OpMode == OPMODE_AP)                    \
		RTMPusecDelay(1000);							\
        (_A)->BbpWriteLatch[_I] = _V;                   \
        break;                                          \
    }                                                   \
    if (BusyCnt == MAX_BUSY_COUNT)                      \
    {                                                   \
		DBGPRINT_ERR(("BBP write R%d=0x%x fail\n", _I, BbpCsr.word));	\
		RTMP_IO_READ32(_A, H2M_BBP_AGENT, &BbpCsr.word);				\
		BbpCsr.field.Busy = 0;                          \
		RTMP_IO_WRITE32(_A, H2M_BBP_AGENT, BbpCsr.word);				\
    }                                                   \
    }                                                   \
}
#endif /* RT2860 */
#ifdef RT2870
#define RTMP_RF_IO_WRITE32(_A, _V)                 RTUSBWriteRFRegister(_A, _V)
#define RTMP_BBP_IO_READ8_BY_REG_ID(_A, _I, _pV)   RTUSBReadBBPRegister(_A, _I, _pV)
#define RTMP_BBP_IO_WRITE8_BY_REG_ID(_A, _I, _V)   RTUSBWriteBBPRegister(_A, _I, _V)

#define BBP_IO_WRITE8_BY_REG_ID(_A, _I, _V)			RTUSBWriteBBPRegister(_A, _I, _V)
#define BBP_IO_READ8_BY_REG_ID(_A, _I, _pV)   		RTUSBReadBBPRegister(_A, _I, _pV)
#endif // RT2870 //

#define     MAP_CHANNEL_ID_TO_KHZ(ch, khz)  {               \
                switch (ch)                                 \
                {                                           \
                    case 1:     khz = 2412000;   break;     \
                    case 2:     khz = 2417000;   break;     \
                    case 3:     khz = 2422000;   break;     \
                    case 4:     khz = 2427000;   break;     \
                    case 5:     khz = 2432000;   break;     \
                    case 6:     khz = 2437000;   break;     \
                    case 7:     khz = 2442000;   break;     \
                    case 8:     khz = 2447000;   break;     \
                    case 9:     khz = 2452000;   break;     \
                    case 10:    khz = 2457000;   break;     \
                    case 11:    khz = 2462000;   break;     \
                    case 12:    khz = 2467000;   break;     \
                    case 13:    khz = 2472000;   break;     \
                    case 14:    khz = 2484000;   break;     \
                    case 36:  /* UNII */  khz = 5180000;   break;     \
                    case 40:  /* UNII */  khz = 5200000;   break;     \
                    case 44:  /* UNII */  khz = 5220000;   break;     \
                    case 48:  /* UNII */  khz = 5240000;   break;     \
                    case 52:  /* UNII */  khz = 5260000;   break;     \
                    case 56:  /* UNII */  khz = 5280000;   break;     \
                    case 60:  /* UNII */  khz = 5300000;   break;     \
                    case 64:  /* UNII */  khz = 5320000;   break;     \
                    case 149: /* UNII */  khz = 5745000;   break;     \
                    case 153: /* UNII */  khz = 5765000;   break;     \
                    case 157: /* UNII */  khz = 5785000;   break;     \
                    case 161: /* UNII */  khz = 5805000;   break;     \
                    case 165: /* UNII */  khz = 5825000;   break;     \
                    case 100: /* HiperLAN2 */  khz = 5500000;   break;     \
                    case 104: /* HiperLAN2 */  khz = 5520000;   break;     \
                    case 108: /* HiperLAN2 */  khz = 5540000;   break;     \
                    case 112: /* HiperLAN2 */  khz = 5560000;   break;     \
                    case 116: /* HiperLAN2 */  khz = 5580000;   break;     \
                    case 120: /* HiperLAN2 */  khz = 5600000;   break;     \
                    case 124: /* HiperLAN2 */  khz = 5620000;   break;     \
                    case 128: /* HiperLAN2 */  khz = 5640000;   break;     \
                    case 132: /* HiperLAN2 */  khz = 5660000;   break;     \
                    case 136: /* HiperLAN2 */  khz = 5680000;   break;     \
                    case 140: /* HiperLAN2 */  khz = 5700000;   break;     \
                    case 34:  /* Japan MMAC */   khz = 5170000;   break;   \
                    case 38:  /* Japan MMAC */   khz = 5190000;   break;   \
                    case 42:  /* Japan MMAC */   khz = 5210000;   break;   \
                    case 46:  /* Japan MMAC */   khz = 5230000;   break;   \
                    case 184: /* Japan */   khz = 4920000;   break;   \
                    case 188: /* Japan */   khz = 4940000;   break;   \
                    case 192: /* Japan */   khz = 4960000;   break;   \
                    case 196: /* Japan */   khz = 4980000;   break;   \
                    case 208: /* Japan, means J08 */   khz = 5040000;   break;   \
                    case 212: /* Japan, means J12 */   khz = 5060000;   break;   \
                    case 216: /* Japan, means J16 */   khz = 5080000;   break;   \
                    default:    khz = 2412000;   break;     \
                }                                           \
            }

#define     MAP_KHZ_TO_CHANNEL_ID(khz, ch)  {               \
                switch (khz)                                \
                {                                           \
                    case 2412000:    ch = 1;     break;     \
                    case 2417000:    ch = 2;     break;     \
                    case 2422000:    ch = 3;     break;     \
                    case 2427000:    ch = 4;     break;     \
                    case 2432000:    ch = 5;     break;     \
                    case 2437000:    ch = 6;     break;     \
                    case 2442000:    ch = 7;     break;     \
                    case 2447000:    ch = 8;     break;     \
                    case 2452000:    ch = 9;     break;     \
                    case 2457000:    ch = 10;    break;     \
                    case 2462000:    ch = 11;    break;     \
                    case 2467000:    ch = 12;    break;     \
                    case 2472000:    ch = 13;    break;     \
                    case 2484000:    ch = 14;    break;     \
                    case 5180000:    ch = 36;  /* UNII */  break;     \
                    case 5200000:    ch = 40;  /* UNII */  break;     \
                    case 5220000:    ch = 44;  /* UNII */  break;     \
                    case 5240000:    ch = 48;  /* UNII */  break;     \
                    case 5260000:    ch = 52;  /* UNII */  break;     \
                    case 5280000:    ch = 56;  /* UNII */  break;     \
                    case 5300000:    ch = 60;  /* UNII */  break;     \
                    case 5320000:    ch = 64;  /* UNII */  break;     \
                    case 5745000:    ch = 149; /* UNII */  break;     \
                    case 5765000:    ch = 153; /* UNII */  break;     \
                    case 5785000:    ch = 157; /* UNII */  break;     \
                    case 5805000:    ch = 161; /* UNII */  break;     \
                    case 5825000:    ch = 165; /* UNII */  break;     \
                    case 5500000:    ch = 100; /* HiperLAN2 */  break;     \
                    case 5520000:    ch = 104; /* HiperLAN2 */  break;     \
                    case 5540000:    ch = 108; /* HiperLAN2 */  break;     \
                    case 5560000:    ch = 112; /* HiperLAN2 */  break;     \
                    case 5580000:    ch = 116; /* HiperLAN2 */  break;     \
                    case 5600000:    ch = 120; /* HiperLAN2 */  break;     \
                    case 5620000:    ch = 124; /* HiperLAN2 */  break;     \
                    case 5640000:    ch = 128; /* HiperLAN2 */  break;     \
                    case 5660000:    ch = 132; /* HiperLAN2 */  break;     \
                    case 5680000:    ch = 136; /* HiperLAN2 */  break;     \
                    case 5700000:    ch = 140; /* HiperLAN2 */  break;     \
                    case 5170000:    ch = 34;  /* Japan MMAC */   break;   \
                    case 5190000:    ch = 38;  /* Japan MMAC */   break;   \
                    case 5210000:    ch = 42;  /* Japan MMAC */   break;   \
                    case 5230000:    ch = 46;  /* Japan MMAC */   break;   \
                    case 4920000:    ch = 184; /* Japan */  break;   \
                    case 4940000:    ch = 188; /* Japan */  break;   \
                    case 4960000:    ch = 192; /* Japan */  break;   \
                    case 4980000:    ch = 196; /* Japan */  break;   \
                    case 5040000:    ch = 208; /* Japan, means J08 */  break;   \
                    case 5060000:    ch = 212; /* Japan, means J12 */  break;   \
                    case 5080000:    ch = 216; /* Japan, means J16 */  break;   \
                    default:         ch = 1;     break;     \
                }                                           \
            }

//
// Common fragment list structure -  Identical to the scatter gather frag list structure
//
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
		if (NdisEqualMemory(IPX, _pBufVA + 12, 2) || 			\
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
		if (NdisEqualMemory(IPX, _pBufVA, 2) || 				\
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

#define SWITCH_AB( _pAA, _pBB)    \
{                                                                           \
    PVOID pCC;                                                          \
    pCC = _pBB;                                                 \
    _pBB = _pAA;                                                 \
    _pAA = pCC;                                                 \
}

// Enqueue this frame to MLME engine
// We need to enqueue the whole frame because MLME need to pass data type
// information from 802.11 header
#ifdef RT2860
#define REPORT_MGMT_FRAME_TO_MLME(_pAd, Wcid, _pFrame, _FrameSize, _Rssi0, _Rssi1, _Rssi2, _PlcpSignal)        \
{                                                                                       \
    UINT32 High32TSF, Low32TSF;                                                          \
    RTMP_IO_READ32(_pAd, TSF_TIMER_DW1, &High32TSF);                                       \
    RTMP_IO_READ32(_pAd, TSF_TIMER_DW0, &Low32TSF);                                        \
    MlmeEnqueueForRecv(_pAd, Wcid, High32TSF, Low32TSF, (UCHAR)_Rssi0, (UCHAR)_Rssi1,(UCHAR)_Rssi2,_FrameSize, _pFrame, (UCHAR)_PlcpSignal);   \
}
#endif
#ifdef RT2870
#define REPORT_MGMT_FRAME_TO_MLME(_pAd, Wcid, _pFrame, _FrameSize, _Rssi0, _Rssi1, _Rssi2, _PlcpSignal)        \
{                                                                                       \
    UINT32 High32TSF=0, Low32TSF=0;                                                          \
    MlmeEnqueueForRecv(_pAd, Wcid, High32TSF, Low32TSF, (UCHAR)_Rssi0, (UCHAR)_Rssi1,(UCHAR)_Rssi2,_FrameSize, _pFrame, (UCHAR)_PlcpSignal);   \
}
#endif // RT2870 //

//Need to collect each ant's rssi concurrently
//rssi1 is report to pair2 Ant and rss2 is reprot to pair1 Ant when 4 Ant
#define COLLECT_RX_ANTENNA_AVERAGE_RSSI(_pAd, _rssi1, _rssi2)					\
{																				\
	SHORT	AvgRssi;															\
	UCHAR	UsedAnt;															\
	if (_pAd->RxAnt.EvaluatePeriod == 0)									\
	{																		\
		UsedAnt = _pAd->RxAnt.Pair1PrimaryRxAnt;							\
		AvgRssi = _pAd->RxAnt.Pair1AvgRssi[UsedAnt];						\
		if (AvgRssi < 0)													\
			AvgRssi = AvgRssi - (AvgRssi >> 3) + _rssi1;					\
		else																\
			AvgRssi = _rssi1 << 3;											\
		_pAd->RxAnt.Pair1AvgRssi[UsedAnt] = AvgRssi;						\
	}																		\
	else																	\
	{																		\
		UsedAnt = _pAd->RxAnt.Pair1SecondaryRxAnt;							\
		AvgRssi = _pAd->RxAnt.Pair1AvgRssi[UsedAnt];						\
		if ((AvgRssi < 0) && (_pAd->RxAnt.FirstPktArrivedWhenEvaluate))		\
			AvgRssi = AvgRssi - (AvgRssi >> 3) + _rssi1;					\
		else																\
		{																	\
			_pAd->RxAnt.FirstPktArrivedWhenEvaluate = TRUE;					\
			AvgRssi = _rssi1 << 3;											\
		}																	\
		_pAd->RxAnt.Pair1AvgRssi[UsedAnt] = AvgRssi;						\
		_pAd->RxAnt.RcvPktNumWhenEvaluate++;								\
	}																		\
}

#define NDIS_QUERY_BUFFER(_NdisBuf, _ppVA, _pBufLen)                    \
    NdisQueryBuffer(_NdisBuf, _ppVA, _pBufLen)

#define MAC_ADDR_EQUAL(pAddr1,pAddr2)           RTMPEqualMemory((PVOID)(pAddr1), (PVOID)(pAddr2), MAC_ADDR_LEN)
#define SSID_EQUAL(ssid1, len1, ssid2, len2)    ((len1==len2) && (RTMPEqualMemory(ssid1, ssid2, len1)))

//
// Check if it is Japan W53(ch52,56,60,64) channel.
//
#define JapanChannelCheck(channel)  ((channel == 52) || (channel == 56) || (channel == 60) || (channel == 64))

#ifdef RT2860
#define STA_PORT_SECURED(_pAd) \
{ \
	_pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED; \
	RTMP_SET_PSFLAG(_pAd, fRTMP_PS_CAN_GO_SLEEP); \
	NdisAcquireSpinLock(&(_pAd)->MacTabLock); \
	_pAd->MacTab.Content[BSSID_WCID].PortSecured = _pAd->StaCfg.PortSecured; \
	NdisReleaseSpinLock(&(_pAd)->MacTabLock); \
}
#endif
#ifdef RT2870
#define STA_PORT_SECURED(_pAd) \
{ \
	_pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED; \
	NdisAcquireSpinLock(&_pAd->MacTabLock); \
	_pAd->MacTab.Content[BSSID_WCID].PortSecured = _pAd->StaCfg.PortSecured; \
	NdisReleaseSpinLock(&_pAd->MacTabLock); \
}
#endif

//
// Register set pair for initialzation register set definition
//
typedef struct  _RTMP_REG_PAIR
{
	ULONG   Register;
	ULONG   Value;
} RTMP_REG_PAIR, *PRTMP_REG_PAIR;

typedef struct  _REG_PAIR
{
	UCHAR   Register;
	UCHAR   Value;
} REG_PAIR, *PREG_PAIR;

//
// Register set pair for initialzation register set definition
//
typedef struct  _RTMP_RF_REGS
{
	UCHAR   Channel;
	ULONG   R1;
	ULONG   R2;
	ULONG   R3;
	ULONG   R4;
} RTMP_RF_REGS, *PRTMP_RF_REGS;

typedef struct _FREQUENCY_ITEM {
	UCHAR	Channel;
	UCHAR	N;
	UCHAR	R;
	UCHAR	K;
} FREQUENCY_ITEM, *PFREQUENCY_ITEM;

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


typedef	union	_HEADER_802_11_SEQ{
    struct {
	USHORT			Frag:4;
	USHORT			Sequence:12;
    }   field;
    USHORT           value;
}	HEADER_802_11_SEQ, *PHEADER_802_11_SEQ;

//
//  Data buffer for DMA operation, the buffer must be contiguous physical memory
//  Both DMA to / from CPU use the same structure.
//
typedef struct  _RTMP_REORDERBUF
{
	BOOLEAN			IsFull;
	PVOID                   AllocVa;            // TxBuf virtual address
	UCHAR			Header802_3[14];
	HEADER_802_11_SEQ			Sequence;	//support compressed bitmap BA, so no consider fragment in BA
	UCHAR 		DataOffset;
	USHORT 		Datasize;
	ULONG                   AllocSize;
#ifdef RT2860
	NDIS_PHYSICAL_ADDRESS   AllocPa;            // TxBuf physical address
#endif
#ifdef RT2870
	PUCHAR					AllocPa;
#endif // RT2870 //
}   RTMP_REORDERBUF, *PRTMP_REORDERBUF;

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

typedef struct _RTMP_TX_BUF
{
	PQUEUE_ENTRY    Next;
	UCHAR           Index;
	ULONG                   AllocSize;          // Control block size
	PVOID                   AllocVa;            // Control block virtual address
	NDIS_PHYSICAL_ADDRESS   AllocPa;            // Control block physical address
} RTMP_TXBUF, *PRTMP_TXBUF;

typedef struct _RTMP_RX_BUF
{
	BOOLEAN           InUse;
	ULONG           	ByBaRecIndex;
	RTMP_REORDERBUF	MAP_RXBuf[MAX_RX_REORDERBUF];
} RTMP_RXBUF, *PRTMP_RXBUF;
typedef struct _RTMP_TX_RING
{
	RTMP_DMACB  Cell[TX_RING_SIZE];
	UINT32		TxCpuIdx;
	UINT32		TxDmaIdx;
	UINT32		TxSwFreeIdx; 	// software next free tx index
} RTMP_TX_RING, *PRTMP_TX_RING;

typedef struct _RTMP_RX_RING
{
	RTMP_DMACB  Cell[RX_RING_SIZE];
	UINT32		RxCpuIdx;
	UINT32		RxDmaIdx;
	INT32		RxSwReadIdx; 	// software next read index
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
#ifdef RT2860
	ULONG           LastReceivedByteCount;
#endif
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

	UINT32   		OneSecFrameDuplicateCount;

#ifdef RT2870
	ULONG           OneSecTransmittedByteCount;   // both successful and failure, used to calculate TX throughput
#endif // RT2870 //

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

typedef struct _PID_COUNTER {
	ULONG           TxAckRequiredCount;      // CRC error
	ULONG           TxAggreCount;
	ULONG           TxSuccessCount; // OneSecTxNoRetryOkCount + OneSecTxRetryOkCount + OneSecTxFailCount
	ULONG		LastSuccessRate;
} PID_COUNTER, *PPID_COUNTER;

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

//
//  Arcfour Structure Added by PaulWu
//
typedef struct  _ARCFOUR
{
	UINT            X;
	UINT            Y;
	UCHAR           STATE[256];
} ARCFOURCONTEXT, *PARCFOURCONTEXT;

// MIMO Tx parameter, ShortGI, MCS, STBC, etc.  these are fields in TXWI too. just copy to TXWI.
typedef struct  _RECEIVE_SETTING {
	USHORT   	NumOfRX:2;                 // MIMO. WE HAVE 3R
	USHORT		Mode:2;	//channel bandwidth 20MHz or 40 MHz
	USHORT		ShortGI:1;
	USHORT		STBC:2;	//SPACE
	USHORT		rsv:3;
	USHORT		OFDM:1;
	USHORT		MIMO:1;
 } RECEIVE_SETTING, *PRECEIVE_SETTING;

// Shared key data structure
typedef struct  _WEP_KEY {
	UCHAR   KeyLen;                     // Key length for each key, 0: entry is invalid
	UCHAR   Key[MAX_LEN_OF_KEY];        // right now we implement 4 keys, 128 bits max
} WEP_KEY, *PWEP_KEY;

typedef struct _CIPHER_KEY {
	UCHAR   Key[16];            // right now we implement 4 keys, 128 bits max
	UCHAR   RxMic[8];			// make alignment
	UCHAR   TxMic[8];
	UCHAR   TxTsc[6];           // 48bit TSC value
	UCHAR   RxTsc[6];           // 48bit TSC value
	UCHAR   CipherAlg;          // 0-none, 1:WEP64, 2:WEP128, 3:TKIP, 4:AES, 5:CKIP64, 6:CKIP128
	UCHAR   KeyLen;
	UCHAR   BssId[6];
            // Key length for each key, 0: entry is invalid
	UCHAR   Type;               // Indicate Pairwise/Group when reporting MIC error
} CIPHER_KEY, *PCIPHER_KEY;

typedef struct _BBP_TUNING_STRUCT {
	BOOLEAN     Enable;
	UCHAR       FalseCcaCountUpperBound;  // 100 per sec
	UCHAR       FalseCcaCountLowerBound;  // 10 per sec
	UCHAR       R17LowerBound;            // specified in E2PROM
	UCHAR       R17UpperBound;            // 0x68 according to David Tung
	UCHAR       CurrentR17Value;
} BBP_TUNING, *PBBP_TUNING;

typedef struct _SOFT_RX_ANT_DIVERSITY_STRUCT {
	UCHAR     EvaluatePeriod;		 // 0:not evalute status, 1: evaluate status, 2: switching status
#ifdef RT2870
	UCHAR     EvaluateStableCnt;
#endif
	UCHAR     Pair1PrimaryRxAnt;     // 0:Ant-E1, 1:Ant-E2
	UCHAR     Pair1SecondaryRxAnt;   // 0:Ant-E1, 1:Ant-E2
	UCHAR     Pair2PrimaryRxAnt;     // 0:Ant-E3, 1:Ant-E4
	UCHAR     Pair2SecondaryRxAnt;   // 0:Ant-E3, 1:Ant-E4
	SHORT     Pair1AvgRssi[2];       // AvgRssi[0]:E1, AvgRssi[1]:E2
	SHORT     Pair2AvgRssi[2];       // AvgRssi[0]:E3, AvgRssi[1]:E4
	SHORT     Pair1LastAvgRssi;      //
	SHORT     Pair2LastAvgRssi;      //
	ULONG     RcvPktNumWhenEvaluate;
	BOOLEAN   FirstPktArrivedWhenEvaluate;
	RALINK_TIMER_STRUCT    RxAntDiversityTimer;
} SOFT_RX_ANT_DIVERSITY, *PSOFT_RX_ANT_DIVERSITY;

typedef struct _LEAP_AUTH_INFO {
	BOOLEAN         Enabled;        //Ture: Enable LEAP Authentication
	BOOLEAN         CCKM;           //Ture: Use Fast Reauthentication with CCKM
	UCHAR           Reserve[2];
	UCHAR           UserName[256];  //LEAP, User name
	ULONG           UserNameLen;
	UCHAR           Password[256];  //LEAP, User Password
	ULONG           PasswordLen;
} LEAP_AUTH_INFO, *PLEAP_AUTH_INFO;

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

typedef struct {
	BOOLEAN     Enable;
	UCHAR       Delta;
	BOOLEAN     PlusSign;
} CCK_TX_POWER_CALIBRATE, *PCCK_TX_POWER_CALIBRATE;

//
// Receive Tuple Cache Format
//
typedef struct  _TUPLE_CACHE    {
	BOOLEAN         Valid;
	UCHAR           MacAddress[MAC_ADDR_LEN];
	USHORT          Sequence;
	USHORT          Frag;
} TUPLE_CACHE, *PTUPLE_CACHE;

//
// Fragment Frame structure
//
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

typedef enum _ABGBAND_STATE_ {
	UNKNOWN_BAND,
	BG_BAND,
	A_BAND,
} ABGBAND_STATE;

typedef struct _MLME_STRUCT {
	// STA state machines
	STATE_MACHINE           CntlMachine;
	STATE_MACHINE           AssocMachine;
	STATE_MACHINE           AuthMachine;
	STATE_MACHINE           AuthRspMachine;
	STATE_MACHINE           SyncMachine;
	STATE_MACHINE           WpaPskMachine;
	STATE_MACHINE           LeapMachine;
	STATE_MACHINE           AironetMachine;
	STATE_MACHINE_FUNC      AssocFunc[ASSOC_FUNC_SIZE];
	STATE_MACHINE_FUNC      AuthFunc[AUTH_FUNC_SIZE];
	STATE_MACHINE_FUNC      AuthRspFunc[AUTH_RSP_FUNC_SIZE];
	STATE_MACHINE_FUNC      SyncFunc[SYNC_FUNC_SIZE];
	STATE_MACHINE_FUNC      WpaPskFunc[WPA_PSK_FUNC_SIZE];
	STATE_MACHINE_FUNC      AironetFunc[AIRONET_FUNC_SIZE];
	STATE_MACHINE_FUNC      ActFunc[ACT_FUNC_SIZE];
	// Action
	STATE_MACHINE           ActMachine;

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
#ifdef RT2860
    UCHAR                   bPsPollTimerRunning;
    RALINK_TIMER_STRUCT     PsPollTimer;
	RALINK_TIMER_STRUCT     RadioOnOffTimer;
#endif
	ULONG                   PeriodicRound;
	ULONG                   OneSecPeriodicRound;

	UCHAR					RealRxPath;
	BOOLEAN					bLowThroughput;
	BOOLEAN					bEnableAutoAntennaCheck;
	RALINK_TIMER_STRUCT		RxAntEvalTimer;

#ifdef RT2870
	UCHAR CaliBW40RfR24;
	UCHAR CaliBW20RfR24;
#endif // RT2870 //
} MLME_STRUCT, *PMLME_STRUCT;

// structure for radar detection and channel switch
typedef struct _RADAR_DETECT_STRUCT {
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
	BOOLEAN		bFastDfs;
	UINT8		ChMovingTime;
	UINT8		LongPulseRadarTh;
} RADAR_DETECT_STRUCT, *PRADAR_DETECT_STRUCT;

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
	USHORT		LastIndSeq;
	USHORT		TimeOutValue;
	RALINK_TIMER_STRUCT RECBATimer;
	ULONG		LastIndSeqAtTimer;
	ULONG		nDropPacket;
	ULONG		rcvSeq;
	REC_BLOCKACK_STATUS  REC_BA_Status;
	NDIS_SPIN_LOCK          RxReRingLock;                 // Rx Ring spinlock
	PVOID	pAdapter;
	struct reordering_list	list;
} BA_REC_ENTRY, *PBA_REC_ENTRY;


typedef struct {
	ULONG		numAsRecipient;		// I am recipient of numAsRecipient clients. These client are in the BARecEntry[]
	ULONG		numAsOriginator;	// I am originator of 	numAsOriginator clients. These clients are in the BAOriEntry[]
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
	struct	{
		UINT32		RxBAWinLimit:8;
		UINT32		TxBAWinLimit:8;
		UINT32		AutoBA:1;	// automatically BA
		UINT32		Policy:2;	// 0: DELAY_BA 1:IMMED_BA  (//BA Policy subfiled value in ADDBA frame)   2:BA-not use
		UINT32		MpduDensity:3;
		UINT32       	AmsduEnable:1;	//Enable AMSDU transmisstion
		UINT32       	AmsduSize:1;	// 0:3839, 1:7935 bytes. UINT  MSDUSizeToBytes[]	= { 3839, 7935};
		UINT32       	MMPSmode:2;	// MIMO power save more, 0:static, 1:dynamic, 2:rsv, 3:mimo enable
		UINT32       	bHtAdhoc:1;			// adhoc can use ht rate.
		UINT32       	b2040CoexistScanSup:1;		//As Sta, support do 2040 coexistence scan for AP. As Ap, support monitor trigger event to check if can use BW 40MHz.
		UINT32       	:4;
	}	field;
	UINT32			word;
} BACAP_STRUC, *PBACAP_STRUC;

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
	BOOLEAN			bLastAtheros;
    BOOLEAN			bCurrentAtheros;
    BOOLEAN         bNowAtherosBurstOn;
	BOOLEAN			bNextDisableRxBA;
    BOOLEAN			bToggle;
} IOT_STRUC, *PIOT_STRUC;

// This is the registry setting for 802.11n transmit setting.  Used in advanced page.
typedef union _REG_TRANSMIT_SETTING {
 struct {
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
 UINT32   word;
} REG_TRANSMIT_SETTING, *PREG_TRANSMIT_SETTING;

typedef union  _DESIRED_TRANSMIT_SETTING {
	struct	{
			USHORT   	MCS:7;                 	// MCS
			USHORT		PhyMode:4;
			USHORT	 	FixedTxMode:2;			// If MCS isn't AUTO, fix rate in CCK, OFDM or HT mode.
			USHORT		rsv:3;
	}	field;
	USHORT		word;
 } DESIRED_TRANSMIT_SETTING, *PDESIRED_TRANSMIT_SETTING;

typedef struct {
	BOOLEAN		IsRecipient;
	UCHAR   MACAddr[MAC_ADDR_LEN];
	UCHAR   TID;
	UCHAR   nMSDU;
	USHORT   TimeOut;
	BOOLEAN bAllTid;  // If True, delete all TID for BA sessions with this MACaddr.
} OID_ADD_BA_ENTRY, *POID_ADD_BA_ENTRY;

//
// Multiple SSID structure
//
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

#ifdef RT2870
#define BEACON_BITMAP_MASK		0xff
typedef struct _BEACON_SYNC_STRUCT_
{
	UCHAR        			BeaconBuf[HW_BEACON_MAX_COUNT][HW_BEACON_OFFSET];
	UCHAR					BeaconTxWI[HW_BEACON_MAX_COUNT][TXWI_SIZE];
	ULONG 					TimIELocationInBeacon[HW_BEACON_MAX_COUNT];
	ULONG					CapabilityInfoLocationInBeacon[HW_BEACON_MAX_COUNT];
	BOOLEAN					EnableBeacon;		// trigger to enable beacon transmission.
	UCHAR					BeaconBitMap;		// NOTE: If the MAX_MBSSID_NUM is larger than 8, this parameter need to change.
	UCHAR					DtimBitOn;			// NOTE: If the MAX_MBSSID_NUM is larger than 8, this parameter need to change.
}BEACON_SYNC_STRUCT;
#endif // RT2870 //

typedef struct _MULTISSID_STRUCT {
	UCHAR								Bssid[MAC_ADDR_LEN];
    UCHAR                               SsidLen;
    CHAR                                Ssid[MAX_LEN_OF_SSID];
    USHORT                              CapabilityInfo;

    PNET_DEV                   			MSSIDDev;

	NDIS_802_11_AUTHENTICATION_MODE     AuthMode;
	NDIS_802_11_WEP_STATUS              WepStatus;
	NDIS_802_11_WEP_STATUS				GroupKeyWepStatus;
	WPA_MIX_PAIR_CIPHER					WpaMixPairCipher;

	ULONG								TxCount;
	ULONG								RxCount;
	ULONG								ReceivedByteCount;
	ULONG								TransmittedByteCount;
	ULONG								RxErrorCount;
	ULONG								RxDropCount;

	HTTRANSMIT_SETTING					HTPhyMode, MaxHTPhyMode, MinHTPhyMode;// For transmit phy setting in TXWI.
	RT_HT_PHY_INFO						DesiredHtPhyInfo;
	DESIRED_TRANSMIT_SETTING        	DesiredTransmitSetting; // Desired transmit setting. this is for reading registry setting only. not useful.
	BOOLEAN								bAutoTxRateSwitch;

	UCHAR                               DefaultKeyId;

	UCHAR								TxRate;       // RATE_1, RATE_2, RATE_5_5, RATE_11, ...
	UCHAR     							DesiredRates[MAX_LEN_OF_SUPPORTED_RATES];// OID_802_11_DESIRED_RATES
	UCHAR								DesiredRatesIndex;
	UCHAR     							MaxTxRate;            // RATE_1, RATE_2, RATE_5_5, RATE_11

	UCHAR								TimBitmaps[WLAN_MAX_NUM_OF_TIM];

    // WPA
    UCHAR                               GMK[32];
    UCHAR                               PMK[32];
	UCHAR								GTK[32];
    BOOLEAN                             IEEE8021X;
    BOOLEAN                             PreAuth;
    UCHAR                               GNonce[32];
    UCHAR                               PortSecured;
    NDIS_802_11_PRIVACY_FILTER          PrivacyFilter;
    UCHAR                               BANClass3Data;
    ULONG                               IsolateInterStaTraffic;

    UCHAR                               RSNIE_Len[2];
    UCHAR                               RSN_IE[2][MAX_LEN_OF_RSNIE];


    UCHAR                   			TimIELocationInBeacon;
    UCHAR                   			CapabilityInfoLocationInBeacon;
    // outgoing BEACON frame buffer and corresponding TXWI
	// PTXWI_STRUC                           BeaconTxWI; //
    CHAR                                BeaconBuf[MAX_BEACON_SIZE]; // NOTE: BeaconBuf should be 4-byte aligned

    BOOLEAN                             bHideSsid;
	UINT16								StationKeepAliveTime; // unit: second

    USHORT                              VLAN_VID;
    USHORT                              VLAN_Priority;

    RT_802_11_ACL						AccessControlList;

	// EDCA Qos
    BOOLEAN								bWmmCapable;	// 0:disable WMM, 1:enable WMM
    BOOLEAN								bDLSCapable;	// 0:disable DLS, 1:enable DLS

	UCHAR           					DlsPTK[64];		// Due to windows dirver count on meetinghouse to handle 4-way shake

	// For 802.1x daemon setting per BSS
	UCHAR								radius_srv_num;
	RADIUS_SRV_INFO						radius_srv_info[MAX_RADIUS_SRV_NUM];

#ifdef RTL865X_SOC
	unsigned int						mylinkid;
#endif


	UINT32					RcvdConflictSsidCount;
	UINT32					RcvdSpoofedAssocRespCount;
	UINT32					RcvdSpoofedReassocRespCount;
	UINT32					RcvdSpoofedProbeRespCount;
	UINT32					RcvdSpoofedBeaconCount;
	UINT32					RcvdSpoofedDisassocCount;
	UINT32					RcvdSpoofedAuthCount;
	UINT32					RcvdSpoofedDeauthCount;
	UINT32					RcvdSpoofedUnknownMgmtCount;
	UINT32					RcvdReplayAttackCount;

	CHAR					RssiOfRcvdConflictSsid;
	CHAR					RssiOfRcvdSpoofedAssocResp;
	CHAR					RssiOfRcvdSpoofedReassocResp;
	CHAR					RssiOfRcvdSpoofedProbeResp;
	CHAR					RssiOfRcvdSpoofedBeacon;
	CHAR					RssiOfRcvdSpoofedDisassoc;
	CHAR					RssiOfRcvdSpoofedAuth;
	CHAR					RssiOfRcvdSpoofedDeauth;
	CHAR					RssiOfRcvdSpoofedUnknownMgmt;
	CHAR					RssiOfRcvdReplayAttack;

	BOOLEAN					bBcnSntReq;
	UCHAR					BcnBufIdx;
} MULTISSID_STRUCT, *PMULTISSID_STRUCT;

// configuration common to OPMODE_AP as well as OPMODE_STA
typedef struct _COMMON_CONFIG {

	BOOLEAN		bCountryFlag;
	UCHAR		CountryCode[3];
	UCHAR		Geography;
	UCHAR       CountryRegion;      // Enum of country region, 0:FCC, 1:IC, 2:ETSI, 3:SPAIN, 4:France, 5:MKK, 6:MKK1, 7:Israel
	UCHAR       CountryRegionForABand;	// Enum of country region for A band
	UCHAR       PhyMode;            // PHY_11A, PHY_11B, PHY_11BG_MIXED, PHY_ABG_MIXED
	USHORT      Dsifs;              // in units of usec
	ULONG       PacketFilter;       // Packet filter for receiving

	CHAR        Ssid[MAX_LEN_OF_SSID]; // NOT NULL-terminated
	UCHAR       SsidLen;               // the actual ssid length in used
	UCHAR       LastSsidLen;               // the actual ssid length in used
	CHAR        LastSsid[MAX_LEN_OF_SSID]; // NOT NULL-terminated
	UCHAR		LastBssid[MAC_ADDR_LEN];

	UCHAR       Bssid[MAC_ADDR_LEN];
	USHORT      BeaconPeriod;
	UCHAR       Channel;
	UCHAR       CentralChannel;    	// Central Channel when using 40MHz is indicating. not real channel.

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
	BOOLEAN		bNeedSendTriggerFrame;
	BOOLEAN		bAPSDForcePowerSave;	// Force power save mode, should only use in APSD-STAUT
	ULONG		TriggerTimerCount;
	UCHAR		MaxSPLength;
	UCHAR		BBPCurrentBW;	// BW_10, 	BW_20, BW_40
	REG_TRANSMIT_SETTING        RegTransmitSetting; //registry transmit setting. this is for reading registry setting only. not useful.
	UCHAR       TxRate;                 // Same value to fill in TXD. TxRate is 6-bit
	UCHAR       MaxTxRate;              // RATE_1, RATE_2, RATE_5_5, RATE_11
	UCHAR       TxRateIndex;            // Tx rate index in RateSwitchTable
	UCHAR       TxRateTableSize;        // Valid Tx rate table size in RateSwitchTable
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

	BACAP_STRUC        BACapability; //   NO USE = 0XFF  ;  IMMED_BA =1  ;  DELAY_BA=0
	BACAP_STRUC        REGBACapability; //   NO USE = 0XFF  ;  IMMED_BA =1  ;  DELAY_BA=0

	IOT_STRUC		IOTestParm;	// 802.11n InterOpbility Test Parameter;
	ULONG       TxPreamble;             // Rt802_11PreambleLong, Rt802_11PreambleShort, Rt802_11PreambleAuto
	BOOLEAN     bUseZeroToDisableFragment;     // Microsoft use 0 as disable
	ULONG       UseBGProtection;        // 0: auto, 1: always use, 2: always not use
	BOOLEAN     bUseShortSlotTime;      // 0: disable, 1 - use short slot (9us)
	BOOLEAN     bEnableTxBurst;         // 1: enble TX PACKET BURST, 0: disable TX PACKET BURST
	BOOLEAN     bAggregationCapable;      // 1: enable TX aggregation when the peer supports it
	BOOLEAN     bPiggyBackCapable;		// 1: enable TX piggy-back according MAC's version
	BOOLEAN     bIEEE80211H;			// 1: enable IEEE802.11h spec.
	ULONG		DisableOLBCDetect;		// 0: enable OLBC detect; 1 disable OLBC detect

	BOOLEAN				bRdg;

	BOOLEAN             bWmmCapable;        // 0:disable WMM, 1:enable WMM
	QOS_CAPABILITY_PARM APQosCapability;    // QOS capability of the current associated AP
	EDCA_PARM           APEdcaParm;         // EDCA parameters of the current associated AP
	QBSS_LOAD_PARM      APQbssLoad;         // QBSS load of the current associated AP
	UCHAR               AckPolicy[4];       // ACK policy of the specified AC. see ACK_xxx
	BOOLEAN				bDLSCapable;		// 0:disable DLS, 1:enable DLS
	// a bitmap of BOOLEAN flags. each bit represent an operation status of a particular
	// BOOLEAN control, either ON or OFF. These flags should always be accessed via
	// OPSTATUS_TEST_FLAG(), OPSTATUS_SET_FLAG(), OP_STATUS_CLEAR_FLAG() macros.
	// see fOP_STATUS_xxx in RTMP_DEF.C for detail bit definition
	ULONG               OpStatusFlags;

	BOOLEAN				NdisRadioStateOff; //For HCT 12.0, set this flag to TRUE instead of called MlmeRadioOff.
	ABGBAND_STATE		BandState;		// For setting BBP used on B/G or A mode.

	// IEEE802.11H--DFS.
	RADAR_DETECT_STRUCT	RadarDetect;

	// HT
	UCHAR			BASize;		// USer desired BAWindowSize. Should not exceed our max capability
	//RT_HT_CAPABILITY	SupportedHtPhy;
	RT_HT_CAPABILITY	DesiredHtPhy;
	HT_CAPABILITY_IE		HtCapability;
	ADD_HT_INFO_IE		AddHTInfo;	// Useful as AP.
	//This IE is used with channel switch announcement element when changing to a new 40MHz.
	//This IE is included in channel switch ammouncement frames 7.4.1.5, beacons, probe Rsp.
	NEW_EXT_CHAN_IE	NewExtChanOffset;	//7.3.2.20A, 1 if extension channel is above the control channel, 3 if below, 0 if not present

    BOOLEAN                 bHTProtect;
    BOOLEAN                 bMIMOPSEnable;
    BOOLEAN					bBADecline;
	BOOLEAN					bDisableReordering;
	BOOLEAN					bForty_Mhz_Intolerant;
	BOOLEAN					bExtChannelSwitchAnnouncement;
	BOOLEAN					bRcvBSSWidthTriggerEvents;
	ULONG					LastRcvBSSWidthTriggerEventsTime;

	UCHAR					TxBASize;

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

	BOOLEAN     		bHardwareRadio;     // Hardware controlled Radio enabled

#ifdef RT2870
	BOOLEAN     		bMultipleIRP;       // Multiple Bulk IN flag
	UCHAR       		NumOfBulkInIRP;     // if bMultipleIRP == TRUE, NumOfBulkInIRP will be 4 otherwise be 1
 	RT_HT_CAPABILITY	SupportedHtPhy;
	ULONG				MaxPktOneTxBulk;
	UCHAR				TxBulkFactor;
	UCHAR				RxBulkFactor;

	BEACON_SYNC_STRUCT	*pBeaconSync;
	RALINK_TIMER_STRUCT	BeaconUpdateTimer;
	UINT32				BeaconAdjust;
	UINT32				BeaconFactor;
	UINT32				BeaconRemain;
#endif // RT2870 //


 	NDIS_SPIN_LOCK			MeasureReqTabLock;
	PMEASURE_REQ_TAB		pMeasureReqTab;

	NDIS_SPIN_LOCK			TpcReqTabLock;
	PTPC_REQ_TAB			pTpcReqTab;

	// transmit phy mode, trasmit rate for Multicast.
#ifdef MCAST_RATE_SPECIFIC
	HTTRANSMIT_SETTING		MCastPhyMode;
#endif // MCAST_RATE_SPECIFIC //
} COMMON_CONFIG, *PCOMMON_CONFIG;

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
#ifdef RT2860
    BOOLEAN		AdhocBOnlyJoined;	// Indicate Adhoc B Join.
    BOOLEAN		AdhocBGJoined;		// Indicate Adhoc B/G Join.
    BOOLEAN		Adhoc20NJoined;		// Indicate Adhoc 20MHz N Join.
#endif
	// New for WPA, windows want us to keep association information and
	// Fixed IEs from last association response
	NDIS_802_11_ASSOCIATION_INFORMATION     AssocInfo;
	USHORT       ReqVarIELen;                // Length of next VIE include EID & Length
	UCHAR       ReqVarIEs[MAX_VIE_LEN];		// The content saved here should be little-endian format.
	USHORT       ResVarIELen;                // Length of next VIE include EID & Length
	UCHAR       ResVarIEs[MAX_VIE_LEN];

	UCHAR       RSNIE_Len;
	UCHAR       RSN_IE[MAX_LEN_OF_RSNIE];	// The content saved here should be little-endian format.

	// New variables used for CCX 1.0
	BOOLEAN             bCkipOn;
	BOOLEAN             bCkipCmicOn;
	UCHAR               CkipFlag;
	UCHAR               GIV[3];  //for CCX iv
	UCHAR               RxSEQ[4];
	UCHAR               TxSEQ[4];
	UCHAR               CKIPMIC[4];
	UCHAR               LeapAuthMode;
	LEAP_AUTH_INFO      LeapAuthInfo;
	UCHAR               HashPwd[16];
	UCHAR               NetworkChallenge[8];
	UCHAR               NetworkChallengeResponse[24];
	UCHAR               PeerChallenge[8];

	UCHAR               PeerChallengeResponse[24];
	UCHAR               SessionKey[16]; //Network session keys (NSK)
	RALINK_TIMER_STRUCT LeapAuthTimer;
	ROGUEAP_TABLE       RogueApTab;   //Cisco CCX1 Rogue AP Detection

	// New control flags for CCX
	CCX_CONTROL         CCXControl;                 // Master administration state
	BOOLEAN             CCXEnable;                  // Actual CCX state
	UCHAR               CCXScanChannel;             // Selected channel for CCX beacon request
	USHORT              CCXScanTime;                // Time out to wait for beacon and probe response
	UCHAR               CCXReqType;                 // Current processing CCX request type
	BSS_TABLE           CCXBssTab;                  // BSS Table
	UCHAR               FrameReportBuf[2048];       // Buffer for creating frame report
	USHORT              FrameReportLen;             // Current Frame report length
	ULONG               CLBusyBytes;                // Save the total bytes received durning channel load scan time
	USHORT              RPIDensity[8];              // Array for RPI density collection
	// Start address of each BSS table within FrameReportBuf
	// It's important to update the RxPower of the corresponding Bss
	USHORT              BssReportOffset[MAX_LEN_OF_BSS_TABLE];
	USHORT              BeaconToken;                // Token for beacon report
	ULONG               LastBssIndex;               // Most current reported Bss index
	RM_REQUEST_ACTION   MeasurementRequest[16];     // Saved measurement request
	UCHAR               RMReqCnt;                   // Number of measurement request saved.
	UCHAR               CurrentRMReqIdx;            // Number of measurement request saved.
	BOOLEAN             ParallelReq;                // Parallel measurement, only one request performed,
													// It must be the same channel with maximum duration
	USHORT              ParallelDuration;           // Maximum duration for parallel measurement
	UCHAR               ParallelChannel;            // Only one channel with parallel measurement
	USHORT              IAPPToken;                  // IAPP dialog token
	UCHAR               CCXQosECWMin;               // Cisco QOS ECWMin for AC 0
	UCHAR               CCXQosECWMax;               // Cisco QOS ECWMax for AC 0
	// Hack for channel load and noise histogram parameters
	UCHAR               NHFactor;                   // Parameter for Noise histogram
	UCHAR               CLFactor;                   // Parameter for channel load

	UCHAR               KRK[16];        //Key Refresh Key.
	UCHAR               BTK[32];        //Base Transient Key
	BOOLEAN             CCKMLinkUpFlag;
	ULONG               CCKMRN;    //(Re)Association request number.
	LARGE_INTEGER       CCKMBeaconAtJoinTimeStamp;  //TSF timer for Re-assocaite to the new AP
	UCHAR               AironetCellPowerLimit;      //in dBm
	UCHAR               AironetIPAddress[4];        //eg. 192.168.1.1
	BOOLEAN             CCXAdjacentAPReportFlag;    //flag for determining report Assoc Lost time
	CHAR                CCXAdjacentAPSsid[MAX_LEN_OF_SSID]; //Adjacent AP's SSID report
	UCHAR               CCXAdjacentAPSsidLen;               // the actual ssid length in used
	UCHAR               CCXAdjacentAPBssid[MAC_ADDR_LEN];         //Adjacent AP's BSSID report
	USHORT              CCXAdjacentAPChannel;
	ULONG               CCXAdjacentAPLinkDownTime;  //for Spec S32.

	RALINK_TIMER_STRUCT	StaQuickResponeForRateUpTimer;
	BOOLEAN				StaQuickResponeForRateUpTimerRunning;

	UCHAR           	DtimCount;      // 0.. DtimPeriod-1
	UCHAR           	DtimPeriod;     // default = 3

	////////////////////////////////////////////////////////////////////////////////////////
	// This is only for WHQL test.
	BOOLEAN				WhqlTest;
	////////////////////////////////////////////////////////////////////////////////////////

    RALINK_TIMER_STRUCT WpaDisassocAndBlockAssocTimer;
    // Fast Roaming
	BOOLEAN		        bFastRoaming;       // 0:disable fast roaming, 1:enable fast roaming
	CHAR		        dBmToRoam;          // the condition to roam when receiving Rssi less than this value. It's negative value.

    BOOLEAN             IEEE8021X;
    BOOLEAN             IEEE8021x_required_keys;
    CIPHER_KEY	        DesireSharedKey[4];	// Record user desired WEP keys
    UCHAR               DesireSharedKeyId;

    // 0: driver ignores wpa_supplicant
    // 1: wpa_supplicant initiates scanning and AP selection
    // 2: driver takes care of scanning, AP selection, and IEEE 802.11 association parameters
    UCHAR               WpaSupplicantUP;
	UCHAR				WpaSupplicantScanCount;

    CHAR                dev_name[16];
    USHORT              OriDevType;

    BOOLEAN             bTGnWifiTest;
	BOOLEAN			    bScanReqIsFromWebUI;

	HTTRANSMIT_SETTING				HTPhyMode, MaxHTPhyMode, MinHTPhyMode;// For transmit phy setting in TXWI.
	DESIRED_TRANSMIT_SETTING       	DesiredTransmitSetting;
	RT_HT_PHY_INFO					DesiredHtPhyInfo;
	BOOLEAN							bAutoTxRateSwitch;

#ifdef RT2860
    UCHAR       BBPR3;
#endif
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

#ifdef RT2870
// for USB interface, avoid in interrupt when write key
typedef struct   RT_ADD_PAIRWISE_KEY_ENTRY {
        NDIS_802_11_MAC_ADDRESS         MacAddr;
        USHORT                          MacTabMatchWCID;        // ASIC
        CIPHER_KEY                      CipherKey;
} RT_ADD_PAIRWISE_KEY_ENTRY,*PRT_ADD_PAIRWISE_KEY_ENTRY;
#endif // RT2870 //

// ----------- start of AP --------------------------
// AUTH-RSP State Machine Aux data structure
typedef struct _AP_MLME_AUX {
	UCHAR               Addr[MAC_ADDR_LEN];
	USHORT              Alg;
	CHAR                Challenge[CIPHER_TEXT_LEN];
} AP_MLME_AUX, *PAP_MLME_AUX;

// structure to define WPA Group Key Rekey Interval
typedef struct PACKED _RT_802_11_WPA_REKEY {
	ULONG ReKeyMethod;          // mechanism for rekeying: 0:disable, 1: time-based, 2: packet-based
	ULONG ReKeyInterval;        // time-based: seconds, packet-based: kilo-packets
} RT_WPA_REKEY,*PRT_WPA_REKEY, RT_802_11_WPA_REKEY, *PRT_802_11_WPA_REKEY;

typedef struct _MAC_TABLE_ENTRY {
	//Choose 1 from ValidAsWDS and ValidAsCLI  to validize.
	BOOLEAN		ValidAsCLI;		// Sta mode, set this TRUE after Linkup,too.
	BOOLEAN		ValidAsWDS;	// This is WDS Entry. only for AP mode.
	BOOLEAN		ValidAsApCli;   //This is a AP-Client entry, only for AP mode which enable AP-Client functions.
	BOOLEAN		ValidAsMesh;
	BOOLEAN		ValidAsDls;	// This is DLS Entry. only for STA mode.
	BOOLEAN		isCached;
	BOOLEAN		bIAmBadAtheros;	// Flag if this is Atheros chip that has IOT problem.  We need to turn on RTS/CTS protection.

	UCHAR         	EnqueueEapolStartTimerRunning;  // Enqueue EAPoL-Start for triggering EAP SM
	//jan for wpa
	// record which entry revoke MIC Failure , if it leaves the BSS itself, AP won't update aMICFailTime MIB
	UCHAR           CMTimerRunning;
	UCHAR           apidx;			// MBSS number
	UCHAR           RSNIE_Len;
	UCHAR           RSN_IE[MAX_LEN_OF_RSNIE];
	UCHAR           ANonce[LEN_KEY_DESC_NONCE];
	UCHAR           R_Counter[LEN_KEY_DESC_REPLAY];
	UCHAR           PTK[64];
	UCHAR           ReTryCounter;
	RALINK_TIMER_STRUCT                 RetryTimer;
	RALINK_TIMER_STRUCT					EnqueueStartForPSKTimer;	// A timer which enqueue EAPoL-Start for triggering PSK SM
	NDIS_802_11_AUTHENTICATION_MODE     AuthMode;   // This should match to whatever microsoft defined
	NDIS_802_11_WEP_STATUS              WepStatus;
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

	BOOLEAN			bSendBAR;
	USHORT			NoBADataCountDown;

	UINT32   		CachedBuf[16];		// UINT (4 bytes) for alignment
	UINT			TxBFCount; // 3*3
	UINT			FIFOCount;
	UINT			DebugFIFOCount;
	UINT			DebugTxCount;
    BOOLEAN			bDlsInit;


//====================================================
//WDS entry needs these
// rt2860 add this. if ValidAsWDS==TRUE, MatchWDSTabIdx is the index in WdsTab.MacTab
	UINT			MatchWDSTabIdx;
	UCHAR           MaxSupportedRate;
	UCHAR           CurrTxRate;
	UCHAR           CurrTxRateIndex;
	// to record the each TX rate's quality. 0 is best, the bigger the worse.
	USHORT          TxQuality[MAX_STEP_OF_TX_RATE_SWITCH];
	UINT32			OneSecTxNoRetryOkCount;
	UINT32          OneSecTxRetryOkCount;
	UINT32          OneSecTxFailCount;
	UINT32			ContinueTxFailCnt;
	UINT32          CurrTxRateStableTime; // # of second in current TX rate
	UCHAR           TxRateUpPenalty;      // extra # of second penalty due to last unstable condition
//====================================================

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
#ifdef RT2870
	ULONG   		LastBeaconRxTime;
#endif
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
#ifdef RT2870
	BOOLEAN			fAllStationAsRalink; 	// Check if all stations are ralink-chipset
#endif
	BOOLEAN         fAnyStationIsLegacy;	// Check if I use legacy rate to transmit to my BSS Station/
	BOOLEAN         fAnyStationNonGF;		// Check if any Station can't support GF.
	BOOLEAN         fAnyStation20Only;		// Check if any Station can't support GF.
	BOOLEAN			fAnyStationMIMOPSDynamic; // Check if any Station is MIMO Dynamic
	BOOLEAN         fAnyBASession;   // Check if there is BA session.  Force turn on RTS/CTS
} MAC_TABLE, *PMAC_TABLE;

#define IS_HT_STA(_pMacEntry)	\
	(_pMacEntry->MaxHTPhyMode.field.MODE >= MODE_HTMIX)

#define IS_HT_RATE(_pMacEntry)	\
	(_pMacEntry->HTPhyMode.field.MODE >= MODE_HTMIX)

#define PEER_IS_HT_RATE(_pMacEntry)	\
	(_pMacEntry->HTPhyMode.field.MODE >= MODE_HTMIX)

typedef struct _WDS_ENTRY {
	BOOLEAN         Valid;
	UCHAR           Addr[MAC_ADDR_LEN];
	ULONG           NoDataIdleCount;
	struct _WDS_ENTRY *pNext;
} WDS_ENTRY, *PWDS_ENTRY;

typedef struct  _WDS_TABLE_ENTRY {
	USHORT			Size;
	UCHAR           WdsAddr[MAC_ADDR_LEN];
	WDS_ENTRY       *Hash[HASH_TABLE_SIZE];
	WDS_ENTRY       Content[MAX_LEN_OF_MAC_TABLE];
	UCHAR           MaxSupportedRate;
	UCHAR           CurrTxRate;
	USHORT          TxQuality[MAX_LEN_OF_SUPPORTED_RATES];
	USHORT          OneSecTxOkCount;
	USHORT          OneSecTxRetryOkCount;
	USHORT          OneSecTxFailCount;
	ULONG           CurrTxRateStableTime; // # of second in current TX rate
	UCHAR           TxRateUpPenalty;      // extra # of second penalty due to last unstable condition
} WDS_TABLE_ENTRY, *PWDS_TABLE_ENTRY;

typedef struct _RT_802_11_WDS_ENTRY {
	PNET_DEV			dev;
	UCHAR				Valid;
	UCHAR				PhyMode;
	UCHAR				PeerWdsAddr[MAC_ADDR_LEN];
	UCHAR				MacTabMatchWCID;	// ASIC
	NDIS_802_11_WEP_STATUS  WepStatus;
	UCHAR					KeyIdx;
	CIPHER_KEY          	WdsKey;
	HTTRANSMIT_SETTING				HTPhyMode, MaxHTPhyMode, MinHTPhyMode;
	RT_HT_PHY_INFO					DesiredHtPhyInfo;
	BOOLEAN							bAutoTxRateSwitch;
	DESIRED_TRANSMIT_SETTING       	DesiredTransmitSetting; // Desired transmit setting.
} RT_802_11_WDS_ENTRY, *PRT_802_11_WDS_ENTRY;

typedef struct _WDS_TABLE {
	UCHAR               Mode;
	ULONG               Size;
	RT_802_11_WDS_ENTRY	WdsEntry[MAX_WDS_ENTRY];
} WDS_TABLE, *PWDS_TABLE;

typedef struct _APCLI_STRUCT {
	PNET_DEV				dev;
#ifdef RTL865X_SOC
	unsigned int            mylinkid;
#endif
	BOOLEAN                 Enable;	// Set it as 1 if the apcli interface was configured to "1"  or by iwpriv cmd "ApCliEnable"
	BOOLEAN                 Valid;	// Set it as 1 if the apcli interface associated success to remote AP.
	UCHAR					MacTabWCID;	//WCID value, which point to the entry of ASIC Mac table.
	UCHAR                   SsidLen;
	CHAR                    Ssid[MAX_LEN_OF_SSID];

	UCHAR                   CfgSsidLen;
	CHAR                    CfgSsid[MAX_LEN_OF_SSID];
	UCHAR                   CfgApCliBssid[ETH_LENGTH_OF_ADDRESS];
	UCHAR                   CurrentAddress[ETH_LENGTH_OF_ADDRESS];

	ULONG                   ApCliRcvBeaconTime;

	ULONG                   CtrlCurrState;
	ULONG                   SyncCurrState;
	ULONG                   AuthCurrState;
	ULONG                   AssocCurrState;
	ULONG					WpaPskCurrState;

	USHORT                  AuthReqCnt;
	USHORT                  AssocReqCnt;

	ULONG                   ClientStatusFlags;
	UCHAR                   MpduDensity;

	NDIS_802_11_AUTHENTICATION_MODE     AuthMode;   // This should match to whatever microsoft defined
	NDIS_802_11_WEP_STATUS              WepStatus;

	// Add to support different cipher suite for WPA2/WPA mode
	NDIS_802_11_ENCRYPTION_STATUS		GroupCipher;		// Multicast cipher suite
	NDIS_802_11_ENCRYPTION_STATUS		PairCipher;			// Unicast cipher suite
	BOOLEAN								bMixCipher;			// Indicate current Pair & Group use different cipher suites
	USHORT								RsnCapability;

	UCHAR		PSK[100];				// reserve PSK key material
	UCHAR       PSKLen;
	UCHAR       PMK[32];                // WPA PSK mode PMK
	UCHAR		GTK[32];				// GTK from authenticator

	CIPHER_KEY      SharedKey[SHARE_KEY_NUM];
	UCHAR           DefaultKeyId;

	// store RSN_IE built by driver
	UCHAR		RSN_IE[MAX_LEN_OF_RSNIE];  // The content saved here should b*****vert to little-endian format.
	UCHAR		*****_Len;

	// For WPA countermeasures
	ULONG sinchuLastMicErrorTime; *****record last MIC e02,
 time
	BOOLEANsinchu ink TechnobBlockAssoc;****gy, I ac.
 iate attempt Ral 60 seconds after St., Jh ubei Ci occurred.o.36, Taiyuan-PSK supplica****tateTech Inink Tec	SNonce[32****publish//  Licenftwarder theTech Inc.	GLicense as		36, undati* the Free S from authenticator

	HTTRANSMIT_SETTING				HTPhyMode, Maxer version. iner versio;
	RT_HT_PHY_INFO lat	DesiredHtPhyInfo; 2002-200     		bAutoTxRateSwitch;
	DESIRED_your option) anyl Public       TransmitSetting*
 *         td in th s hope .
} APCLI_STRUCT, *P* but WITHOU;

// -t even the end of APut even the       *
 * MERCHA

struct wificonf
{          bShortGI           bGreenField;
};
*
 *typedef BILITY *   _PCI_CONFIGSS FPeneral Publi           CSRBaseAddresss publ// PCI MMIO           *
, all access will use
}ic License for;* GNU General Public LUSBnse for morUINT                BulkInEp    ;ther bulk-inimplpoint a    *
 a copy of the GNU GeneralOutublic [6];cense    out
 * along with thi      ave receiv *
 *//thou*****miniporg wiapte iBILITYurenc.,GNU General PublRTMP_ADAPTER moreVOID     OS_Cookie     **** specr FI           relative****OS
	PNET_DEV    net_dev;y,
 * H     VirtualIfCnt;

#ifGeneRT2860
 GenUSHORT		            LnkCtrlBitMask;*************************R*******Configuration************y of the GNU Gen********

Offset**************************Host*****

    Module Name:
    rtmp.h

    Abstract:


    Revisport gen	********************PCIePowerSaveLevel*****             bPCIclkOffeithither flag that ind  *
e if the PICE p----he GNus in     Module Na SPace..-----          CheckDmaBusyCt., ******
     I, Jhrupt S  2002Registe iang  ise NTC*******			ThisTbttNumToNextWakeUp             SameRxByteang  2 Inc*onet.h"

#define VIRTUAL_IF_INC(__pAd) ((__pAd)->VirtualIfCnt++)
#define VIRTUAL_IF_DEC(/
/* --------*     ed parameters.h

    Abstract:
F_NUM(__pAd) ((__pAd)->VirtualIfCnt)

#if__pAdnet.h"

#define VIRTUAL_IF_INC(__pAd) ((__pAd)->VirtualIfCnt++)
#define VIRTUAL_IF_DEC(__pre details.                          *
 *                                                   a cop
#defint_enable_reg    ////////////dis/////m*****//////////////p***
ng "ai	PlaceDMABUFF_NUM(__pAd) TxBufSpace[NUM****TX_RING]*
 * Shaill memoryied     1st pre-allo  crdessPacprogram isd with each TXD
	union	{
		UCHAR			WireleRxDescRpe thn[4];  //Buffer ;
		HEADER_802_11twarRX descriptors
	union	{
		UCHAR			WirelessggregatiX_BUFFER_NORMSIZE]36, save Aggregation sTxe.
} TX_BUFFER, *PT_NORMSIHAR			Wireless __HTTX_BUFFER{
	unionublic// AC0~4 + HCCA
#***
f

	NDIS_SPIN_LOCKn[4];  //Birq_y, Iypeddetails.              R			A///////d           *
 70ironet.h"

#define VIRTUAL_IF_INC(__pAd) ((__pAd)->VirtualIfCnt++)
#define VIRTUAL_IF_DEC(__pAd) ((__USB)->VirtualIfCnt--)
#define VIRTUAL_IF_NUM(__pAd) ((__pAd)->VirtualIfCnt)

#ifdef RT2870
////////////////////////////////////////////////////////////////////////////
// The TXBILITY usb_ITNEig_.
} TX_BUF		*itialiypedef struc			eral Public License     *
 * along with this profer;	//Initito the                         *
 * Free SoferBuffer;	//INumberOfPipes    -------ize;
	UCHAR		MaxPacketSiz    MP_H__
#def//Initial one packet is .36,=anytiControl Flags
	 * Hsinchu kOut; // all 	PX_BUFFIoclude "TCRegTable)InitiEAN		PURPOSE.  SBOOLEAN			IRPPendingbUsbssPalkAggr11-130EAN		ion se        data priorityR{
	P anytim * Tr Thread
	unTTX__TIMER_QUEUE		02_3[Q;	RTSFrame;
	}fiedefi;
	ULLgrega IRP anytimemdLENGTH_80CmdQable)
mdLONG			DataOffset;
	U dmaTxRatl Lin  EXT, *PT spiny, I
              INT		Func_kill               mlmever-gen			Header_8Semaphores (event)in Minipos_TX_CONTte irps
;		//Initeithe*****sleep tNGTH_ on	d in Minipo;		//InitialRTUSBCmd in MiniportIitialize
	PURB			pUrb;			//Initialized in MiniportINT		 in Miniporin Minipocomplee Nato trackQCportInized in MiniportInitialiirpsTTX_BUFFER	TransferBuffer;	//In dmaTTX_BUFFER	wait_queue_head_t			*ze;	;S_FRAMEfor 2_3];
 //"aironet.h"

#define VIRTUAL_IF_INC(__pAd) ((__pAd)->VirtualIfCnt++)
#define VIRTUAL_IF_DEC(__pP			U Geneoth----/VOID			pAd;		//Initialized in MiniportInitialize
	PURB			pUrb;			//Initief RT2870
////////////////////////////////////////////////////////////////////////////
// The Taironet.h"

#define VIRTUAL_IF_INC(__pAd) ((__pAd)->VirtualIfCnt++)
#define VIRTUAL_IF_DEC(__pAd) ((__Tx)->VirtualIfCnt--)
#define VIRTUAL_IF_NUM(__pAd) ((__pAd)->VirtualIfCnt)

#ifdef RT2870
////////////////////////////////////////////////////////////////////////////
// The TX002-2007, Ralink TechnolDeQ IndRunnlFrame;
		PSPOLL_FRAMEn   or ensuring iportansmisse pack get c    icen	RTSFrame;
	}field;
	UCHARansmissTxRaX_BUFFER_NORMSIZEn size.
} HTTX_BPollDtibl->Virtua*****xt and ACSA.     ed, 4buffeup    edNG			DataOffset;
	UUCHAR		_CONT    ther UCHAR		X_CONTEXTr_t		catesNG			DataOffset;
	UMLMEcol.
//
typ     VOIDct _RX_COTEXT;

HTE];
CONTEXOut;Txe.
	extTEXT;


//
// StruG			DataOffset;
	UIin.
	PURHT_TX_CONTEXT;


//
// Strther n't both be 1 X_CONTEXT;

// 4l,  sied EAN	 X_COindexand bTX_BUFF   20oftware Fo		_

#UCHAR		Ireor[4      only    d	PUCHAREDCA			bA    pip////            UCHAR		 buffer         being prtotal 6now.
	BOOLEAN				bRxHandle   Reset	bWaid               Mgm;	//  bufferll , so waiti				ReadaReqedefbulk-out size in bytes of resourion 2 osoftware backlog / Inds
			Rsv_HEADEails.        TxSwsmissNT			TxRate;
	dma_addrcate
+ 1
		RTSRTSFrame;
	}field;
	UCHARIN_LOCK		at the same time.
	ULOG				_LOCK		X_CONTEXT;

union	{
		UCHAR			Wireleleteggregation[4];  //Buffer	{
		UCHAR			WirelessPMGMTe.
} TX_BUFFER, *PTOR_V
		HEADER_802_1letegatioma;		// urb dma on linux
}ERSION  TxRatNDIS50_MINIPORT
#dePrio gatiX_CONTEXT;
ironet.h"

#define VIRTUAL_IF_INC(__pAd) ((__pAd)->VirtualIfCnt++)
#define VIRTUAL_IF_DEC(__pAd) ((__Rwhich packet now are writing to.
	ULONG			NextBulkOutPosition;	// Indicate the buffer start offset of a bulk-transmission
	ULONG			ENextBulkOutPosition;	// Indicate the buffe        *
 ***  NDISR;
		HEADER_802_11	RullFr   0
#endif

#ifdef  NDIS51char	CT
#define RTMP_NDIS__addrRxRSION     5
#de_FRAME	ture to keep trRpending bulk	Rin.
	PURB];
exte_SIZ********1r_t		redundf thmultiple IRP			bAgin.e data to the protocol.In *PTX_CONTEXeral PONTEXT
{
	PUCHAR				T		bRxHandl bufferRxX_CONTEX****Maximang .
	BOORx val70 /*********Ciphn  unsign Tech Inc.ndling;RLEAN	Inify tLONG		I1    cred
  odifynt kipLen;
eT8[8ex which ho****y Whatpackerollerrn ULONG BIT32[32];
exterReadn UCHARBIT8[8];
extern char* CipherName[];
extern chadrivistran 			pU& pro     ik Tec         2[32];
exterPosie NampherSuWf thtopackeaten cre2 URB bufferern lerame;is			bAin failedPOL_. ine  AR CKIP_rn Uame;OL_Led inferLengthAP_AIRONET[8]CHAR APPBC_SNALE_TALrn UCchar* CilE_TALied d
  pon linLC_SNAP_AIRONET[8]12];AR CKIP_L strucchar* Ci			pUpAR IPX[2]n a1a-1999 p.14
exulk-out size in bytes in one bulk-transmission
	UCHAR			BulkOutPipeId;
	BOOLEAN			IRPPending;
	BOOLEAN			LastOne;
BOOLEAN	ASIC)->VirtualIfCnt--)
#define VIRTUAL_IF_NUM(__pAd) ((__pAd)->VirtualIfCnt)

#ifef RT2870
////////////////////////////////////////////////////////////////////////////
// The TX////32lkOut; // all txMACVersIP_LLC_ORT
#deMAC vern UC. R.O.C.
rt
 **C(0x
 **0100) CISCtern D CHAR  Phy11). *
 * i        *
 * MERCHANMERCHANTn UCE2PROMUCHAR  Phy11ANextRateUpward[];
ext
 * Hsinchu eIdToMbps[];Eepromtern UCHAR  PhipherSubyte 0:xtRateUp,bps[];1: revirn UCHAR  C2~3: un beiation[4];  //Buffer for saEEHAR      *
Numefine RT// 93c46=6 Ciph66=8 in this  CipherSuiteWpaNoneTkipDefaultVMAC_X_BUFneTkip_BBP_PARMSvingure to keep tr002-2007, Ralink TechnolexternA     ing packrn UCHAR  RateIdToMbps[];
Firm: To USHORT RateIdTo0Kbps[];

eMinorextern UCHAR  CiphMajtChanIe;

exotherwise];
exten UCHAR  Phy11ANextRateUpward[];
externBBP9-06exte  RssiSafeLevelForTxRate[];
extern on[4];  //Buffer for saBbpWriteLatch[140RAME	Psn, R.O.C.
 *
 *e;
er600
*/
#_MAC_Awritten via  RS_IO_WRITE/NFO_ELEM[];
_VY_REG_IDIe;
extern UCHAR  Ccx2Ie;

exRssiToDbmDelta    BP_R66_TUN	HEADER_802_1BbpTu
	UINo.36,   Phy11ANextRateUpward[];
eexternRFIC[];
exteLINK_OUI[];
extern UCHAR  PowerCons;
extern UCHAR  Ccx2Ie;
RfIcTypTaiwaipherSuiFIC_xxxrn UCHAR  RateIdToMbps[];
RfFreqport geI[];
eAR  uenc11	Nrt gr_t		channel s     ingCCKM24[]FE_PAS*************CHARRfReg
 *   // lCHAR thBGN2est RFrn UgrammION _MAC_ADisionRF IC doesn't
// rece READ

	extern ANTENNA WITHOriori****naon[4];  //Buffer f //Buffer for s1BGN2ANrn UC defin IPX[2];
di_SNA* Ci_t		a & g. We nell bo07, USiand[];fu *
 *  ForABce.RateSwitcNICnse for21N1S[];
exNic    Mo2No.36, rn UCTODOZEROtern UC DTUNNsity meAR  ismrn U beinet iswhen;		/rl,  nstraiX	*next;
	= DIVERSITY ON
	SOFT[];
ANT_	Sequence;
extAde "atern UCHAR  RateSwitchTablFProgSeq;
	CHANNELE];
POWLock;
	NDITx-----[/*
 _BUFFERg_list
SRAME	Ps for storet whes Tan_MAC_Ad[];
llHAR  Rats.ing_list
{
	struct reorderCR  RatListu *next;
	int 	qlen;
};

11BGistPVOID// receivD					mem		// Tite surveying_list
{11J{
	struct reoing_mpd11Ju *next;
	in11Jt 	qlen;
};

struct 802.11jHAR  Ratend bbwssi0;             // last struct reord RSSI
	CHAR			LastRssi1;      

typedef struct 	_RSSI_SAMPLE {
	CHAR			LastRsOLEAN					bAMSDU;
};

strustruct reorxtern UCHARTable11N2S[];
extnOLEANied AR  Rate-09-truct reord]HAR MULTICASBbp94               BbpForCCKll , so waTx20MPwrCfgABand[5hesestruct _QUEUE_ENTRG     *Next;
}   QUE4EUE_ENTRY     *Next;
}   QUEQueue str *PQUEUE_EAR  SupRateIe;       AgcAon[4];  //Buffer ern /////GE_TUNNEautot whAgcE[];


extch Incast rssiRefAPaul Lct reorderssifine	MAXSEu_pool
as 25 ee sera     Tech IncDER, *PQPlusBoOUI[ryA *NeDDR_LEe Inb       
{
	Pincre    ring_mpdu
extempensat              \
{ Min                                      de            \
	(QueueHeader)->Head = (Queueead;StepE_HEADER;

#definx TSSI dtern      m* Ci/umber     \_MAC_ing_lRt _QAgcTTX_ader)-E_HEAct reordeern c        PX[2(ine Remov * (idx-1)) {
	PQUEUE_ENTRY    Head;G	PQUEUE_ENTRY    Tail;
	ULONG           Number;
}   QUEUE_HEADER, *PQUEUG_HEADER;

#define InitializeQueueHeader(QueueHeader)              \
{             G                                         \
	(QueueHeader)->Head = (QueueHeader)->Tail = ader)->Head = pNext;        mber = 0;                          \
}

#define RemovLL)				\
	{						eader)                \
(QueueHeader)->Head;                   LL)		        \
{                                       //+++Taiy2_3];
,02.11a-fCnt--)esrn UstarticenseBGInfoport g1 ~>Next = (Queu3ead;    Next = (Queu0X_CONTEXreordeB/G RSSI#0 port gu_pool
ononeTkip 0x46h
		(QueueHeader)->He1d = (PQUEUE_ENTRY)(Queu1Entry);      
		(QueueHeader)->He2d = (PQUEUE_ENTRY)(Queu2\
			(QueueHead//HANTA                \
		((PQUEUE_ENTRY)QueueEntry)->Axt = (QueueHeae InsertTai\
		(Queue InsertTaiad = (PQUEUE_ENA)(QueueEntry);       \
		if ((QueuAHeader)->e InsertTail           \
{        \
			(QueueHeader)->Te InsertTaiE_ENTRY)(QueueE       \
		(QueueHeader)->Numbe	(QueueLNAGaiRxwiM (PQUEUE_ENTRY)external LNA#0       \
		if ((Queu4                 ad = (          \

		(QueueHeader)->Tai UCHAR36~64UE_ENTRY)(QueueE       \
	else                 1u_pool
{
	Pch100~128UE_ENTRY)(QueueEE_ENTR \
	else                 2QUEUE_ENTRY)(Q32~165ALINK_OUI[];
extern UCHAR  PowerConstraLEDE[];


extern UCHAR  RateSwitchTable[];
extMCU_LEDCS WITHO		LedCntgeneMP_H__
#deLed1rn UC			pUcense	if ((Que3ct operations
//2rn UCT_FLAG(_M,ket.perations
//3>Flags |= (_F)40HAR MULTICALe;
ex  *
 *StrepSigHAR MULTICAInfoSingalstrE_TALOff generic           bLedOnScan
	UIfine RTMP_CLLed for Rwithonet.h"

#define VIRTUAL_IF_INC(__pAd) ((__pAd)->VirtualIfCnt++)
#define VIRTUAL_IF_DEC(__pAd) ((__// las)->VirtualIfCnt--)
#define VIRTUAL_IF_NUM(__pAd) ((__pAd)->VirtualIfCnt)

#ef RT2870
////////////////////////////////////////////////////////////////////////////
// The TXackeutgoION BEACON ffCntLLC_SNAPnd bcorrespo.
	BOO		RTSTXWt WITHOkOut; // all txBeaconTxW PURe detaxContex(((_MBufs in this TxCon!= 0)
port g[HW__M)   _/*
 COUNT   {
//;
		Pbuild PS-POLLSFlagNULL     ((upon link up.r_t		dfficiG[];
purpos    PS#end_FRAMEut; // all tx sPollFfCnt;
		IrpLo_802_1150_MINIPORT
Nu)
#defineIe;
extern UCHAR pending bulk_PSFLAGn.
	PURB    (((n  unsigned.OpStatusFlags & _F)n.
	PURFLAG(_pAd, _F)   (_F))
_pAd)->CommonCfg.OpStatuRTS_pAd)->Coulk-out size in bytes 
P anytim===AP   ((_pEn==F)      ((_pSTAy)->ClientSt/* Modto inCSToWu Xi-Kun 4/21/2006->PSINK_OUI[];
extern UCHAR  PowerCotern UCHAR  PowerConstraSTASA.       itiali modifie& oHeadePX[2   200    (		/* coverted tpAd->Oprsio == OPMODE_STA    ((_pEntry)->ClientStatusFlags &= ~(_F))
#define CLSTA_ADMINnse forssi2X8;
StaCfion[4];  //Bu>Clien;    will ,      scketFilCTIVE|= (_F))
#defiStaAc    s published bvali* coverted tADHOC_ON(pAd) || INFRAF)   (((      BOOLEAN			IRPPendingnickname[IW_ESSID(_M)-sign+1ZE];
	

#defin,ket is beinin\
{  iwitiali i/fma;		//MEDI11N1ATtatusFlagPreMedia for(_pA     ((_pCommony)->ClientSt& (_OP mode: ein UC warorNT_SOLEAN					bAMSDU;
};

struF)) !=HAR  RateSwitchTable11taCfgdefine R,     (_pAP		RTSFraryptionDisabled)
#d8[8];
exne STA_WEP_ struct    onon2Enabl(((_pEnte,teSwaNonrn UNdisne STA_WEPDisConneceives & (__FLAG(_pEntrypher RTMP_SE2600
*ry &n CHAR   002-2007, Ralink TechnoloLocalAdminMACefine RTMP_NDI// UR  D(_pAAR  gedBGNeOLEAN					bAMSDU;
};

struPermanent     *
[MAC_ADDR ***RAME	PTablact2_11(_p->StaGNexwith this gRssi0X8;
	SHORT			AvgRshar* C)->StaCfg.bCkipCmicOn == T CKIP_KP_p)				((((_px;
	BOOLEAN  ((_pEntry)->ClientStatusFlags &= ~(_F))
#define PowerConstrac     T_FLAG(_pEntry,to b	bCu    (_p->SSFlagCfg.WepStaRING_INDEX(_idx, _RingSize)    \
{                          COMMO |= (_F))
#definvgRs     X_FI
PVOID WITHOUon definitionsl((_pAPollPPUCHARs thosQueui////E {
	CHAR			LastR feder)    ne ISAUXRssi2X8;
} RSSI_((_pAuxILTER_CLEAR_FLAee sor    ((_pAd)-lientSda_dma;		pIre GNU machTat 20SS_TABLtatusFlags |= &= canTabILTER_CLEAR_FLAs    \
{  xtern USCANPPenultd)->MAb    MacTab\
		((staGE_TUNNE         #0SFlag#   CISexterc*
 *nd buPd)->AC0(_pAd)				(((_pAd)-> e IS_RT_OUI[];
eityToon-chip WCID enFI_Ot////.  At TX,->MACValway90(_p key    or
	BOOto tn UCersion &) == 0  0
#endif

#ifdef  NDIS51_IS_RTTxRate;	BA0(_pAds &=AT////    TSFrame;
	}field;
	UCHAR _TxRTxRate	RALINK	UCHAR	_RT3090RECBAINT		Ad)->MAencryp    /mberl[_idx KEYidx)  s
	CIPHER_acke             	HEADEKeyu *neMB_NO_SNUM]his for s% (_if

#defin       \
}BSS0][0..3]

HeadeRX 		PSssemblyLLC_SNAPUI[];rag         
	FRAGMENTg.OpStatusFlags Frag#definP_ON(_p)             #defi00) =ag      _TxD, TX     (d)->MAvarious#ifndeeFFER>PSFlATUS_CL3ion is 0x3090DS(_Tx8023                   // l3 St., JhxD, _IsSD0)    EAR_FLAG(_pAWlan                             \
11 MIB   if (_IsSD0) {_Tx      ;
extern UCaSET_  \
    else {_TxD->La    ext trn Upriety   if (_IsSD0) {_TxDRextern UCHAR  Drs  \
    else {_TxD->Last// W  if (_Imory(Dynamic TX            TablePRIVATIS_RT30lag) & 0x10)rivate     Rssi2X8;
} RSSI_SAMefine I inRalinktry,_F  if (_Is)->MA  20s, see fPlace - SuitRate      n UCHAR  HtCapIe;
extern UC tx bF_NUM(__pAd) ((__pAd)-    epresent
char* Cidevic(_pAdtry)herSuiteCiscoCC UCHAR  RateIdToMbps[];
PS                              s Tan----F)     (((_  20.S_FRAME		RCS[];
exterTX s11BG[]e #eAes[];
extern UCHAR  CiphS       Ad)->MAe.
	BOOLdisc802_11 ion3E     
{
	P gen    (((ber++; Did];
e beinanymorket.UCHAR  RateIdToMbps[];
LinkDown * Taer)->Num                          astR     ext;
}                                    +++U	/* cover_t		 for
    002-2007, Ralink Technolo    Moructgeds published b    Motructst d 200t for1(_p
//S_NO_ IS_RT30er)->Numbe UCHAR  RateIdToMbps[];
extraNC_TX_TSC(_tsc)    lags                 LL;  isplayION  Entry)-                        System302,
BitmapbCkipCmicOb0:n CHAR xtRateUp) Copymber++;    break;                                    MacIc USHORT RateIdTo50    MACexte seriaue sterface iss70 /ol****redistver.Der)->Number+R  Phy11ANextRateUpward[];
externaActiv       lo     R  Phy11ANextRateUpward[];
extRTUS_CLEA_EV_TxD(_pAd)		E{
	P			(cryp         HTCl;
	ULAd)->n one bulk-transmission
	UCHAR			BulkOutPipeId;
	BOOLEAN			IRPPending;
	BOOLEAN			LastOne;
	BOOLEAN	AP bestic)->VirtualIfCnt--)
#define VIRTUAL_IF_NUM(__pAd) ((__pAd)->VirtualIfCnt)e;
	BOuiteCiscoCCKM[];
extern  unsigned char   CipherSuiteCiscoCCKMLen;
extern	unsigned char	CipherSuiteCisp tr, so waiting foOut of OneSecre full , so waiting foInHtCapInfo.RxSTBC;      \
	_pAd->S    pInfo.RxSTBC
 * Tility.HtCapInfo.RxSTB +R[MAC_AHtCapInfo.RxSTBlmeAux.HtCapawCHARDogRx     >StaActive.SupportedHtOverFlowPhy.RecomWidth = _pAd->MlTx bufferCnRB				pUrb;
	//Thesg packet.
	BOOLEAN				             bUpdateBcnCntDon     \
}
tive.SupportedMacDeadggreg& (_F))     oPs = _pinto d   \
	_on3EdCKIP_LINK_OUI[];
extern UCHAR  PowerConstraDEBUG(PQUEUErty)->Cl \
{                                   bBanAllBaSetude "Content[BSPromiscuo  (((INK_OUI[];
extern UCHAR  PowerConstraRateDoc emu    on-efinPIfCnt--)
xtern UCHAR  RateSwitchTable[];
exteInfo2.rtsaccu[30Next;
}   Qcbility)              fendlity)             b((_M****[1ivin _pHtCapaxinextern  unsignON(_HAR MULTIrcvba[6     Content[BS  \
A       \
	_pAd->MForcePrintTXtent[BSSID_WCID].AMsduSRze = (UCHAR)(_D//////sRTMP_TESHeadeSwitfg.Wepze in bUSB  \
	_pAd->MStaFifoTesntent[BSSID_WCProtec16);CHAR)(_pHtCapabil		RTCHAR)(_pHtCapabilGenOne		RT   \
	_pAd->MBroadComHTMCSSet iFollowION ade STA_TGent[BSSIDAP_AIRONETddHtInfRderinFactor);	\
}

TTX_BUFFER	MACRO for 32-bit PCI rOn UCegister read / write
//
/Cancel1-1307eems noeak;  now?gister read /e[12
// MACRO for 3Init PCI register read /_Offset,
/Faigene)->NumbPhy.BILITY or FITNE			WIFIern btion n Miniporeorde_dma_mpdu_pool Ad,
/blk//   BOOLEInfo2.OpepInfo.ondnonBEa-1999s      R.O.C.
non BE1a-1999s p802.3
//          RTMPiw_    .HtCa
#def access)->StBILITY        icete any	e any val];
extern UbttTictransmdIe;
exteicenMSI_SUPPORTMode = _pAd->MHaveMsiupportedHtPhnd pending via bytes AR MULTICASis_Name
#>MacTa UCHA_BAS
   (10ess B/OS_HZ)etry before faONE_SECOND access BBPre failin)     // Numbflg_be_adjuAR)(_driver to i_A, _V) _ *
 _ight *
 *11BGN8TICASTM_FlgSusTX_B_pAd)->CommonCfg.OContent[BSUseEfuse;

extern} Place - SuitT ANPlace - Suit_ON(_thouC     IAPP Ralink     *
 * 59 Temple _CISCO_Cnt endingNT_ed a s[];
exterPlcpSigpublished Cnt =LE_TAL			((((_p)->SMessageB[];
extern							GNU 			((((_p)->S dri16);CSTA_TKIP_							f                         Destinaio= TRfg.bCkipCmicOn 			((((_p)->SSding;                 \
  s[];
exterTaFILTER_CLEAR_FLTag(eleHeadeIE) - Adjac    ASN_O    G0, &Value.word)       \
   //LE_TALied alue.fie_ADAinclu
	BOO4bps[];cate
extecketFilterOUIINIT(rtedHtPhy.Mi0x00, _F)    \96   \0     ((_ed)
#defivET_LAP((_p)->StaCfg.bCkipCm //
#de     *
1	Nul      alongG0, &Value.woructurent operatAd)->Msid., No        RTMP_sidu *ne*******_NO_CFG0, &Value.wot,
//             //_WRITE302-08-ern cli    has been    llPacket;
 *
   if ((_A)->bPCIclT AN  if ((_A)->bPCIcl retry befoDELAYINTMASK		hile03fffbetry befo_ID(_A, _ _I, _pV)        \
{  nd****             cetry befoRxkOutSizI, _pTRUC52.NonelayedZERO     divi rxetry beforx of Inthe FG_STRUCfaBbpCsr;     T            t          \
leteint          102i, k;                               \
Co****nt        2TRUCCapatx c   {      \
    BB    {           cess LONG                    Mcu90xxand          2RTMP_Imcuetry befoPreTBTTint         cessMAJOR_e-Csr.meAux.ifieetry beforsr.field.        800DDR_LE            \
        GPINT	Notift         8 BUSher vPight            \
            inclup    cFG_STR4        
        }                     \= (Uode=ull    continue    Capa fifo     . BeforfullmeAux.   \
* GNU General PublRX_BLKkOff)arm.XX_RXDlag andRxDSFlaRTEST_PSFL			pR->PSFlaOPSTATUS_CLEA		pH BusySFlaTSFraPACKET.fiele packfine RTMP_CL*p of nt operations of et is in this TxCr tx bulgs & (_F)n ==R_VEUCHApAd->NTRY)alc * 1e TKIP* (c)us    }  = 1; hort  \
    B= 1;  SET_FLAG(_pRxBlk, _UX_T)		pCsr.wor->
	BOOL|=);     TE32(_A, BBP_CSRTESFG, BbpCsr.word);     \        for (k=0&k<MAX_BUSY_COUNT; k++) CLEAR           \
        {                 = ~(;     )RITE32(_A, fRX_WDS \
      Csr.word); \
AMSDU             if (BbpCsr.fieTxTsc valueI, _p4Csr.word); \
HTst re-key
I, _p8Csr.word); \
PAD             10if (BbpCsr.fielP.Busy == IDLE2              QO        4              )->Co      8              EAP      10              MESH     2   (BbpCsr.fiel* but     4   (BbpCsr.fielDL       8   (BbpCsr.fielWPI     cessITE32(_A, LENGTHield.B_SUB.OpSt	Irp	1 break;         *(       = (UCHAR)BbpCsr.field.Value;         \
OPSTATUFIELD	 2retry beforX_UNKOWNg.OpSt        \
        }MCA               if (BbpCsT    GACY           \          T                  \ break;        pV)           \          _NOR                           TX    G     \
    20F)   	g) & 0xlyTIVE_Cizeof(T= 1; )2];
e48bps[]s.  *
 * 59 Temple     DBkOff) s & (_F)QueIdxBbpCsr.fieldTx#defiB[];
ueHeade8[8];
extern ed in tsd->MlGNU EE802.11VOID    (02-09ono betc
       #defi USB#defixter_ADDR_LE USB     ((PLE;

/w[8];
exsend                    BulkOut;      \
}g//#define RTMP_BBP_IO_R_TxD, TXs e11Bwill pV)    {}
// Read BBP register bme., Nefine RTMP_BBsee IEEE80           _REG_ID(_A, _I, _pV)    {}
// omic_t			IrpLo (_Ae packreorefin00)
#def_ENTRYd.BuMacE0000ueueHeadedefi:1a-1999 	RTS_1 = 1;}RA f    rn Ufff0000) /btor 00) ==ith thist your option) any *ped in thNo.36, Taapabilit      *
 *. Wait forrn character. Befor    SA.       a-1999>Com           \
      BbpCags & (_F)pSrcBuf       ueHeadeRtializeQuINIT(    Bu    sk_LC_S->atibAX_BUSY_COUNT; i++)sy =              \
    {                 ,(_pAd))				(((deTX_BTxRi hanl
	BOOR  Raes		BulkOutSiz; i++)_A, _I,               a-1999 payloadern cha             \Layer 2   BusyCn_BUSY_COUNT     LlcSnapEncap STA_AEdefinmeanMP_A    ra LLC/SNAPrn Uest BA
#));      \
      Buf[80ef stdif /empteIdTolessPay == O +
   WI +               + pad
	BOO+ eld.BuSub                \));      \
Mpdu      _A, _I, _pV1 = 1;}  Busy    \
{ NO           \2.11a-                 drPa             R.O.C.ION        P       PlcpSig));      \
apidAC_ADDDDR_LEN];eAux.HtCapllPacket;
	INIT(_Txa-1999));      \
Wcid           \
 
#def00000Csr.field.RegNum = _I;              .RegNum = _I;       e;
	UCHA cla < MAX_I;              G_ID(G;          w-08-kiplied IFSNum = _I;        r.field.fRead Req//#definCapaPLE;

//
/e PER to tE802.is
#defin*(_pV) = (_APhy.Maul Lin  TODO: ObsoBUFFd? S******)				(RegNMCSd,
/        CipherAl	_pAwiMCS[]BbpCs alogrithm
	P;         OUNTKey           \
        Baul Lin S             eSwitchTabnd a sdetail *
 * YOU SHOULD  BbpTOUCH IT!           lIfCnt--)
# To  being prhard: To-);				e.
 *yn UCHAInfo2.Opefin          H     \
SA.       _MAC_AD*****in***** *
     DBT AN    DBWRITE32(_A, fTX_bRtsRst BA
#\
      n UCHAR SNAP_ifUCHAR _A,  RTSGenerateor(Bbp->HtCap. NADAPTEb.Content60/arm.Ma.             \Ack      { INIPORT
           .11a-1999 CHAR ackPPenponse             \PiggyB      Phybreak;4      gacfineile (
}

#iggbe r    no\
            \HTt micMINIPORT
      CapaSPOLwRegNPTERHT rGNU              ID].ANonQoextern U     1     fD].ARegNue useful    ((	RTS    WMM-Y)  in  ifWepSld.Value;          r by            2             \ segment
          , A-    ord))S	\
   hen i==iMP_ADA     d.RegN_TxD, TXpCsr.field.BusMore of f ((BbpCs       re         gatible// BBP in           smisspCsr.field.BusWMM        8(_A, QOSk of               \ClearEAPG_ID(                     n", ASSIGNG, BbpCsT.word);    ,      )	\
		do {ut; // a	     	if (                    {        for (k=0; k<MAX_B         else                    \
}

#define_CSR_CFG, &BI, _V)}[8];
(0)T, BbpCsr.word);	_CFG, BbpCs             \
    \
}

#define BBP_IO_W, BbpCsr.word);	            BP_CSR_CFG_STR((      \
{        BP_IO_WR=BBP_IO_WR? 1 : 0               \
       \
            BusyCnt     \
{                   *
 * 
)->Nu                    \
        RTMP_IO_READ32(_A, BBP_CSR_CFG, &BbpCsr.word);     \
              *
 ***      l;
	ULO& AMsduSi NIC          WME_IUCHAION         \
//
tN_OUI[];
thouern UCiDAPTER - Suit        \
,    h    idx)e putpInfo.M      *
 *eSwitchTab.ield__inlbefo   , Bong_mpICAMsduSi  modifie(     IN    \
    ULONG  s & )      MO_ELEM[];
32  ((,    _(_A,_CSR whil)_OUI[];
e0:     blder)-d.BBP_RW_MODE = 1;   PBF_    ENA       	T
#de0x418(_A)data HAR  Ad . SWBand[];
ehandl    )    d.BBP       \
  1;                 IsSD0RUPTonCfg.P);
}
     BbpCsr.field.fRead l;
	UL                      \
        BbpCsr.fiel/)    (     "fOPonDisUS_DOZE" On,       ityTo    alize
	P, REG_I           includt reoro>StaActiv _pAd->MLME, w_ADDR[MACenalb \
{          \
ted )->MACtyTois alfiney      Up.     ipherSuiT2661 =>LAG(_pA       ze
	Pi   \
#de_OUI[];
eRTMPot;   fine nd bUCHAR  if (B  *
 **BUSY_COUNT)                      \
    {                               \!OPLatch[_          Csr.fiiteLatch[_I] =))
	{
e CLBBP_RW_MODE = 1;                  & (_////////////// /*Y_REG_ID(_A,*/     \
   1://////
	}    REG_    	DBGPRINT(R    BUG  *
CE, (WriteLatch[_I] = !\n"))->StaAeld.Busy = 1;                        UC  B3    har  :    8_BY_     _CFG, BbpCsr.field.RegNum = _I;                        c   BbpCsield.C******Mff0000) IP2MAC(
	IN e detaipIblic ,f == FALSE) *p              ////16 ity-oB[];.fiel   \           defi)
		retur No.3         {           _pAd\
    {             \
    for (BSwitch (                c    ETH_P_IPV6:    		memset(\
    {     0,           *OFCkipCESS);    *TMP_IO_READ) =(_M,3sr.word);				\
  			d      if (BbpCsr.field.Busy 2           [12 \
 bpCsr.field.Busy 3          con3inue;                4          con4inue;                5          con*Next		brea                   :    _p->St        \
		RTMP_IO_READ32(_A, H2M_BBP_AGENT, &BbpCsr.word);				\
        01nue;                == BUSY00                 \
      0x5.MCS;                              ] & 0x7efin\
        BbpCsr.word = 0;    tinue;                     \
           \
.field.	}


    for }pportedHt*        offsc.,            routin      rtmp_itch.cord)TSFramatch[     	DBGcb.Conergy, Iff == F, Bosto      ,
	OUT    \
        Bbp*pp	\
    
	     0xff, 0x0, 0x0);				Txchar	CM802_1ff ==     \
        BbpCsr		RTMPusecDelay(1      OfdmIfCnt--)
Hoo    if	   \
        B _V;     ield.    Free	\
    A)->BbpWriteLatch[_I] = _V;                ead 12];Reg    break;A)->BbpWriteLatch[_I] =    1;  )->BbpTSFraHANDAd)				(((_Wrapper    Module Nan.
	PUR		RTMPe;
extern UCHAield.NICInitRT30xxRF2600
*/
t == MA                 )upportedHtPhy.OperaionMBGPRINT_12];neTkipf (BusyCnt == MAX_BUSY_COUNT)                     \
    mac_             NT_ERR(AsicFromneTkip == MAX_BUSY_COUNT)                      \
		R		\
    And    logsmissA)->BbpWriteLatch[_I] = _V                   ERR(ializ   }                                     Flags = ENTRY&   Reada\
    }                       sic                      \
}
#endif moPs);	\
	_ifdef RT287        *
 ***D32(_A, H20) ==BBPeAesLx%x fail\n", _I, BbpCsr.word));	DBGPRINT_EbiliReada                          \
           gatiTE32nUp                      \
}
#endif 	((((_p)->StaCfggati     _ID(_A, n ==CfgERR(e RTMP_BBP_IO_WRITE8_BY_REG_ID(_A, AD8_BYetIO_WR02,
TUSBWriteBBPRegister(_A, _I, _V)
#defiEraseCHAR  Adx%x fail\n", _I, BbpCsr.wo                Loadgister(_A, _I,                    \
    }               M          inif (Bu0x%x fail\n", _I, BbpCsr.wo
002-2007NIC
    ForHangTUSBWriteBBPRegister(_A, _I, _V)
#defieAux.A     Bb                                                 Raw                                  \
  r.fiel    NotAllZero       , BosT; i1Busy =r.fiel                    bre    (_A)->BbpWield.fRT; iV)

#def * Hsin case 3:            TTX_afine0;   break;     \
           k;     \
     2               case 4:            Mov   break; MODE_    \
               case 5:    khz = 2432000;   break;     \AtoH(
	    	*sreak;P_AGEN*d  khz i   c   blen:    usyCnBase 7:     c 3:     khz = P    MacBbpBu  {                                      }   2_3[ == MAX_BUSY_COUNT)                      P                    \
   = 24         case 5                00;    driBusy = , Boston,usy =SBWriteRFRegisle11G[];
extern UCepeaT2870se 9:    Sez = 2452000;   ase 10:    khz = 2457000;   break;                         eAesL         \
     d = 2452000	ak;     \
          	00;   break            ak;     se 9:          67000;   break;     \
                    casMODE002-2007, Ralink Technol*p      le   case 9:    SetLEDx%x fail\n", _I, Bbp	
}
#endifP_AGENT, for R/  khz = 5200000Signal;   break;     \
                TSFrad->StaA(Que Dbm  case 9:    l;
	ULRxTxx%x fail\n", _I, Bb                  o     gnal      0x80,     c     A_WEPMne IS_			RTU                                  nDisa_MACHINE *Se 56: MODEUNII */  khz _FUNC      []G_ID(_A, ) == DDBA 52600                          e 56:  /*ne ISic_t		ELEM *Ele UNII */  ((_pDEL /* UNII */  khz = 5300000;   break;     \
                    case 64:  /* UNLS* UNII */  khz = 5300000;   break;     \
                    case 64:  /* UInTER_T* UNII */  khz = 5300000;   break;     \
                    case 64:  /* UQOz = 5745000;   break;     \
                    case 153: /* UNII */  khPeerAddBAReq* UNII *x fail\n", _I, BbpCsr      /* UNII */  khz = 5805000;   break;     sp                    case 165: /* UNII */  khz = 5825000;   break;    Del /* UNII *            case 165: /* UNII */  khz = 5825000;   break;     /* UNII */  khz = 5300000;   break;     \
                    case 64:  SendPSMP                  case 10              case              casePsmp5000;   breakRM                  case 104: /* HiperLAN2 */  khz = 5520000;   break; Public                  case 104: /* HiperLAN2 */  khz = 5520000;   break; HT                  case 104: /* HiperLAN2 */  khz = 5520000;   break;  break;     \
                    case 161: /* UNII */  khz = 5805000;   b   \
    _ = 2ou          case 6aActivS.      1e 56:  /*case 6          
	PUR /* HiperLAN2 *      case 13220;   break;     \
           3G_ID(_A, ORI  break;     \
                            break; RefreshBARk;     \
              break;UC  BbpCsr;     *p              cct           \
                    case 56:  /*MODE_OPSTATUS_CLEARpHdr8021: /* HiperL           case 38:  /* Japan M         ca/* Japan M2 */  khz Bar 34:  /* JapAN2 */  khz = 5700000;   beak;       _BE)   couBa              pDA              SAG_ID(_A, InsertAct                 case 104: /* HMODE_       G_ID(Buf          * Hs  caseLen          8 Category4920000;   bAct              \
 couEn/ IndForRecv \
                    case       * Hs2 */               Ms           ;   brea_REQ pMsg          	\
		AsicSendCommandToMcuatibI */  002-2007    H     Rxfo.A                           case 2:     pan */   khz = 498NullFrDma00;   break;   \
                    ca           SOURCE    requestNullFrported  case 9:     = 498ERSION  */   khz = 5040000;   break;   \
          */   khz = 5060000{      reak;   \
                    case 2  khz = 5060000BbpCsr.fiereak;   \
                    case 2voi    J16 */   w    }   reak;   \
    case 1:     khz = 24120 2417 = 49800    {                   \
                  002-2007_A)->BbIs		Lasgatibl                                   	((((_p)->StaCfpAX_Ban MMAC          \
              h      002-2007reak       OI */  khz                                                    1;     brePUC  BbpCsr;              T2870 //

#definSniff2
#insIO_W.WepteIdTo  \
    TSFraBUFFLock;
pFirstteIdToV)

#define BBP_IO_WRIT       port g                        
#in0  ch = 4;     break;     \
 1  break;     \
  TAak;   pack (khz)                                              pack00;   brea break;     t == MAX    \
    {     M       	\
     5660000;      0:    ch = 6;  p  breakArra \
                     OOLEAN			        case 9:    urb dma on li (khz)                          INode = _pAd-bIntk;     \
     P_AGENT,      break;        Max_Tx_                         \
(_A,ed in th                case 112: /*    \
    for (i=0; i<00:    ch = 10;    breakOUT	apan *2000\
  TXDLefT2870
#define RTketFch = 11;    break;     \
                word);ld.Bu      0:    ch = 10;    br;     \
    RxEAPOL      1    cr  /* Japan MMAC */   khz = 5reak;     \
            II */ BBP_CSld.Bur.wordak;     \
   IO_WWn chfine T2870 //

#defin    \
  TXD    es \
                    case 243700fine BBP_IO_WRITE8_BY_REh = 14;    b   break;     \
       { h = 14re details.      \
  OOLEANI    case 2462000 /* Uch = 11;    break;20000:    ch = 44;  /* UNII */  ;    break; 000:    ch = 6;     break;             case 5260000:    cERSION  h = 52;  /* UNII */  break;     \
                    case 5280000:    ch = 56;  /* UNII */  break;     \
 NullFrh = 52;  /* UNII */  break;     \
                    case 5280000:    ch = 56;, &Value   caslcDdule Naase 5220000:    ch = 44;  /* UNII */  break;     \     se 13:    khz = 24720et i  case 9:    tern )->P000:    ch = 36;  /* UNII */ P_TEST_PSFL		p)->Pbreak;     \
     	);	\
      case 5805000: CFACK      case 5805000: Ins = 2stamp0:                   e 5825000:    ch c00:              NSeq,LE) &&W new           .ak;     \
   BAet i0:    ch = 40; 0xfbreak;     \
                     case		P     \
         T ch = 104; /* Hip       N2 */  break; opWepSk;     \
       Cfreak;      if ((_A)->bPCIclkOff == FAL    \
        00:    ch_ of   /* Japan MMAC */  * UNII */ MODE_ \
                   OpSt  case00:    c
                    casCach0000:    ch = 36;  /*/* HiperLAN2 */  break;     \
                    case 5580                ggreTX_BUF = 157; /* UNII */  break;     \                \
            WIV\
            QSEL/  khz = 52000      Msdu      \
    e RTMP_BBP_IO_WRITE8_BY_REG_ID(_A, _I, _esume0:    ch = 128; /* HiperLAN2 */  break;     \
          cas       MM      case 5220000:    ch = 44;  /* UN\
           break;  ags & (_F0;   break;  copy of the GNU case 3:     khz = ak;  _F)   ((ase 5220000:    ch = 44;  /* UNII */  break;     ;     \
               Qos _F)0:    ch = 140; /AMsdogram iionperLAN2 */  \
                    cas= 140; /RTSperLAN2 */  break;     \
                  \
             case	unsigTab.Cot];
exead HiperLAN2         case 5170000:    ch II */  break;     \TS break;     s[];
extern UCHAck                      case 5240             ch = 40;  \
		RT    Pic_t			IrpLock;   ca    // RT2870 (khz)                       MODE	((((_p)->StaCfg*k;     \
        \
       y 302,
 (khz)                            ;              pWpaKe         	WpaMicValuure0000: perLAN2 */  break;     \
                                  case 64:      pa
       Ap    y, Inc.
                       case 132: /* HiperrLAN2 */  khz = 5660000;   breaak;     \
                    ccase 136: /* HiperLAN2 */  0xff, 0x0, 0x0)Clone.Wep;     \
                    case 2437n.  Don't nepInsfieldH        00:    ch = 6;    In0:    ch MODE_:    ch = 6;  *pp Har ch = 56;  /* UNII */0x0);				at */  break;   \
                    defaul     \
           *00:    ch = 12     \
                                  case = 1;          case 5210000:    ch 
                    case RTMPL               \
   Common fragment list structure -  Identical to the scatter  break;   pan */   khz       Up_M)-*/   kh            case 104          ca	((((_p)->StaCfg.k;     \
 pan */   khz
    JHC, H2M_ = 157; /* UNII */  break;               \  break;   mberOfElements;
  EH2M_B[];Reserved;
    RTMP_SCATTER_GATHER_ELEMENT Elements[NIC;   \
                    case wepI */  khz =     khzWepEngi;


type                                \
          Ke    ch =       case 5240KeyI /* UNII */  break;     Key        8

typedef struct _Resk;     \
 khz = NextNe 5560000(_a) < (_b)) ? (_a) : (_b))
#endif

#ifndef mbreak;    efine GET_LNA_GAIN(_p                  case 570          \ 2417SoftDpNextNWEP break;     \
                reak;     \
        r.field.RTMP
#incnALNAGaibpCsr.field.BpGroumax
/  khz = 5200000ICVbreak;   \
                    case 5210000:    ch (_pAd)	((_pAARCFOU= _IIT  \
     ADHOC_nding b Ct         endif

#ifndef max
#define 8;     break;  ))
#en           e ADHOC_OBYTE)                (OPSTA0x309t  \
       ADHOC_ODECRYPp)                (OPSTATUS_TEST_FLAG(_p, fOP_STATU   khz = 243: ((_pAd->LatchRfRegs.Chan0) : ((_pAd->LatchRfRegdefine IDLE_OEN_p)                 (!INFRA_ON(_p) && !ADHOC_ON(_p))

// Check LEAP & CCKM flags
#define LEAP_ON(_p)                 (((_WPAp)->StaCfg.LeapAuthMode) == CISCO_AuthModeLEAP)
#define LEAP_CCKM_ON(_p)            ((((_p)->StaCfg.LeapAuthMode) == CISCO_Aut))
#d   \
 ALC_FCS32  \
   ))
#defFcs               Cse 582     ->LatchRfReg      		pIrcSendComlcSnthou32(_/RF = _p->Virtua
    do s   (((_psicA _V) ing_mpd        case 1:     khz = 241200	+ *(eAux.Aity->Ht  /* J00;   break;   \
       	E)        OHead       case f (N   case e*****\
    = 34;  /* JaAMsduSiBGity->Ht\
			NdisEqualMemoNonGFExiN))
#define sic      ructure       case 5680000:    ch = 136; /* Hipructure  ch = 34;  /* JaMACV									\
		TxRaraLlcSnapEnca        case 165: /* UN max(_ructure) 								\
		RfCHAR  Exe_A, _V)          case 5060000:						\
}
/  khz = 5660000;						\
}

// New Define  khz =ase 136: /* HiperLAN2 */  khz \
			e
	PThen
        }ase 5220000:    ch = 44;  /* UNII  {    __RTMP_H__

#includnapEncap)	\
{ID].A					        case 1:     khz = 241200		\
		_pE			\
	if (((        case 165: /herSuiteCiscoCCR_GATHER_LIS	 -----Register(_e;
extern UCHARIN            bIO_WT  \
_FRAME		ncap)	\
{	etBssi30000;                                 \
pNAP_BnapEncap)	\
{	etM00) WOff == Fncap = SNAP_802_1H;						\
		Del    Tab
	{															\
		_pExtraLlcSnN2 */1H;						\
		= 52400DG						\
	{															\
		_pExtraL BbpCs2_3_HEADER(_p, _pMac1, _pMac2, _pType)        Syn_A, _V)                 RTU}


#define MAKE_80Bss                                       \
    NdisIboveMemory(_p, _pMac1, MAC_ADDR_LEN);         SetEdcaParm            case 104: /* HipePess AR  STMP_SN), _pMa_p + MAC_ADDR_LESloz = 2            case 104: /* HipePOSE.  SeUseICULA* 2), _p1H;						\
		Add       \
     
	{															\
		_pExtraLlcSnaacket = Nss;
extTTER_GATHER_LIST {
  (((_ved frame is LLC/SNAP&BbpCsr.w             ifndef max
#definP & CCKM flagspTxMieak;   move the LLC/SNRP fi1H;						\
		RemoveC1042 nor Bridge tunnel), keep it that way.
// else if the received frame is LLC/SNAP-encap1H;						\
		eAux.A 0xfAttribu80000: 													\
		_pExtraL*******k;     \
 /* Japae received frame is LLC/SNA preserve the LY_CSR4_STRUC PaireHAR KeyTxRin_pData & _DataSize may IVEI       ove 8-byte LLC/SNAP) by this MACRO
//     UCHAR  RateIuk;          \
{       EIV_pData & _DataSize Rx 0xfTxRind (remove 8-byte LLC/SNAP) by this MACRO
//    move the LLC/SN\
   NAP (neither RFnor Bridge tunnel), keep it that way.
this MACRO
//     _pRemovedLLCSNAP: pointer		-encaped IPXbpCsr.field.p&BbpCsax
#definULL is not removed
#define CON                Txine INFRA_ONther RFmove
#defin Bridge tunnel), keep it that way.                   //     _pRemoCRO
//    pCsr.field.B:  /        et frame
// Patch fData, 6)  ||                     \
 
	}														he reaped IPX or A		               \
ADDR_Ln = (&BbpToMcuge tunnel), keep it that way.
// else if th       TTER_GATHER_LIST {
 Tok= 4920000      case 52Arg               \
       00:         *
 ***a + 6;          c      O    if (LL;								\
	}														        Register(_A, _I   {   RandomNAP_BRIDGE_TUNNEL;			\
		}								                         MgtMac  case 42:  /* AX_BUSY_COUNT)       khz = 5210000 \
                    ca ((NdisEquSub                ToD_ENCAP_           case 46:  /* 													\((_pRadioOff        case 1:     khz = 24120056);                   case 104: /    \
    BssTxRin			RTUSBWr3070(_pAd)*Tab          _TxRin			RTUSBWr= 5300000;   break;     \
      \
             \
);      Search\
        }         n[1] = (UCHAR)(_DatpExtraLlcSnapEncap =se                                                                   \
    P & CCKM fp    if ((NdisEqualMe       if ((NdisEqualMe                               With_NO_                           \
           MAKE_802_3_HEADER(_p8023hdr, _pDA, _pSA, pProto);           \
          C_Len);      DeBUFF Bridge tunMODE P3070(_pAd)p             \
            \
    
{
	PV   {                        ata +=ORI LENGTH_802_1_                  \
			NdiA_ORI\
       BA        8; /* JapaBss     S  \
                    case 24MODE_3070:    chBs           LLC_    MAKE_80);        ]e / 256);    , pProto);        move           {    != 0)
PerioMAKE_802CFNdisMoCf_pMa             AtimWioto);    {    Capabilit     e / 256);     p bre  \
    {          \
Proto);        Ext  \
        LLC_Leze % 25Proto);  HT_CAPABILITY_IE *pHt256);     e / 25ADD        _802_3AddHt     ->MACVemighDAPTER     W_MO    al h _pVfo IEPUCHAR pProt	_HEADER(_p80Proto);        ch en);                   \
NewExtruct000:    ctraLlcSnapEncap            Info      LARGE _I; GER  = 2S case 582        kip         \
    NdisMoemory((_pTRA_LLCQ MA      MAKE_      QosEADER(_p8023hdr,PQ3070LOAD        bss   M(_DataSize /       VIE          cad->StaAVARIbpCsrIEs pVI                      t Bridge tun    \
    else                                                                \
    {                                                                  \
  *      LLC_Len[0] = (UCHAR)(_DataSize / 256);                          \
        LLC_Len[1] = (UCHAR)(_DataSize % 256);                          \
        MAKE_802_3_HEADER(_p8023hdr, _pDA, _pSA, LLC_Len);              \
    }                                                                   \
}

#define SWITCH_AB( _pAA, _pBB)    \
{                                                                           \
    PVOID pCC;                                                          \
    pCC = _pBB;                                                 \
    _pBB = _pAA;                             k Bridge 4940000;   break;   \
        [0] = (     case 19*******                          SueEninorde   case 19     \O
//     _pRe BAWineak;   \
 }
#endOrigin ~(_F)_tsc          02-2007IsRecip \
           ovedLLCSsidSor           \
    else                 }      Ouhy.Sak;               \
     {                               ortByInfoTH_802_1_H                 ze, _Rssi0,      \Pars       _1_H;     :    c     T2870 //

#defin00;    Ind			RTUSBWr                          
    MlmeE_ON)rodge tunecv(_pAd, Wcid, High3, _pFram((_pJapan *
	{															\
		_pExtraL2: /* ne IS_            Ms           92: /* Japan */   ield.*  breakAR)_Rssi2,_FrameSiz/   khz =annel <= 14) ? (_pAd->BLNAGain                               Hig      cen 4 Ant
#definLow            Info if ((NdisEquInfoze / 256);   Info  khz =92: /* Japan */   k MAKE_sID pCC;6);     5220s[NIC_MAX_PHY/* UN    ize, _pFecv(_pAd, Wcid, H                         = 208; /* Japan,56);    crt00;   break;  \
                    case 208: /* J
    MlmeEEmptR)_Rssi0, (UCHAR)_Rssi1,(UCHAR)_Rssi
    MlmeEpCsrPair1AvgRssi[UsedAnt];						\
		if (A //

/Sub case 52           \
        Ndi;   br         */         ufVA*\
}
#endifsi = _rssi //

/00;   brea0;   break;     \    UNII */  khz = 5       break;     \
             Ant when 4 AStN_BRIDGE_RT2870 /
	{					break;     \
      Def 11:    k							ERR(Pair1Ant when 4 AS_ONd->RxAnt.Pair1AvgRssiSekhz 8; /* HipUNII */  khz = 528											\= 24RT2870 /		\
		UsedAnt = _pAd->RxAFd->RxAnt.Pair1AvgRssiPerRali[UsedAnt];		                          				\
		if ((AvgRssi <                    case 64:  Dro	if (((*(_pBufVA) << 8) + *(_pBuf                    case 64:  nc.
 Pair1AvgRssi[UsedAnt]                            = AvgRssi;						\
	};   break;     \
                    caRe        break;   			\
}

// New Define for new Tx Path.
#define EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(_pBufVA, _pExtraLlcSnapEncap)	\WhenEvaluate++;								\
	}																		\
}

#define NDIS_QUERY_BUFFER(_NdisBuf, _ppVA, _pBufLen)                    \
   
       nEvaluate++;								\
	}																		\
}

#define NDIS_QUERY_BUFFER(_NdisBuf, _ppVA, _pBufLen)                                            \
        RTMP_IO_READ32    case 60Ndis \
                                                           case 64:  /* UtNumWhe
#define JapanChannelCheck(channel)  ((channel == 52) || (channel == 56) || 
       
#define JapanChannelCheck(channel)  ((channel == 52) || (channel == 56)reak;
	_pA              Secured = WPA_802_1X_PORT_SECURED; \
	RTMP_SET_PSFLAG(_pAd, f(channelS_CAN_GO_SLEEP); \
	NdisAcquireSpinLock(&(_pAd)->MacTabLock); \
	_pAd->MacTans J08 *AN_GO_SLEEP); \
	NdisAcquireSpinLock(&(_pAd)->MacTabLock); \
	_pAd->OID)(pAddr2), Md)->MacTabLock); \
}
#endif
#ifdef RT2870
#define STA_PORT_SECURED(_p NdisQueryBud)->MacTabLock); \
}
#endif
#ifdef RT2870
#define STA_PORT_SECURED(_pktNumWhenEvaluaTabLock); \
	_pAd->MacTab.Content[BSSID_WCID].PortSecured = _pAd->StaCCls3errd)->MacTabLock); \
}
#endif
#ifdef RT28                           BetweenWe/  b            case 1:     khz = 241200 5765000Pair1W				  \
  ; \
	_pAd->MacTab.Content[BSSID_WCID].PortSecured = _pAd->StaC
} RTMP_REG_PAIRtNumWheTMP_REG_PAIR;

typedef struct  _REG_PAIR
{
	UCHAR   Register;
	
} RTMP_REG_PAIR
          nt.Pair1PrimaryRxAnt;	el)  ((channel == 52) || (channel =         \
		DBGPRI((_p cou    MMac2, MAC_ADDR_LEN);          \ RT2870 //

//Need t {      breard));	\
		RTMP_IO_READ32(_    Ad)-(_F))
0;   break;   \
                   
	UCHAR	* HiperLAN2 */  break;     \
 UENCY_ITEM, *, *PRPostPrAIR, *PREG_PAIR;

//
// Register se              khz = 2ize / 256);               (UCHAR)_Rssi1230000:         \
         LLC_Len[1] = (UCHAR)(__DataSize % 256);                           \                          
        MAKE_80    _HEADER(_p8023hdr,	UCHAR	                      _pDA, _pSA, L    }

#defin          uth											\
		}																	\
		_pAd->RxAnt   }  */  khz =sdAnt] = AvgRssi;						\
		_pAd->RxAnt.RcvPk *PRnEvaluate++;								\
	}																		\
}

#define NDIS_QUERY_BUFFER(_NdisBuf, _ppVA, _pBufLen)                    \
   ) == 0thAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED; \
	RTMP_SET_PSFLAG(_pAd, fRt bespAtSeq2ontiguous physical memory
//  Both DMA to / from CPU use the same structure.
//
typed4->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED; \
	NdisAcquireSpinLock  field;
 d)->MacTabLock); \
}
#endif
#ifdef RT2870
#define STA_PORT_SECURED(_pCls2t pair for initialzation register set definition
//
typedef str				\
, or
#define JapanChannelCheck(channel)  ((channel == 52) || (channel == 56)
} RTMP_REG_PAIR,ut       hannelCheck(channel)  ((channel == 52) || (channel =     ((_pEnORDERBUF, *PRTMP_REORDERBUF;

//
// DER_802_11_Rsp											\
		}																	\
		_pAd->RxAnt.    struct {
	U	\
	}			 break;     \
                    ca     \, ord)->MacTabLo        case 165: /* UNII */  khz = 5825000;   break;     *PRTiortIRspGenAnd    all ring descriptor DMA operation,OPSTATUS_CLEAR_(_DataSize / 25(UCHAR)_Rve the Lto / froignalDIS_BUFFER itNum          FFER is44:  /* U;   \
                    dlsI */  }   RTMP_REORDERBUF, *PRTMP_REORDERBUF;

//    caseyn;											\
		}																	\
		_pAd->RxAnt.Pair1AvgRssi[UsedAnt] = AvgRssi;						\
		_pAd->RxAnt.RcvPk (((_M)Evaluate++;								\
	}																		\
}

#define NDIS_QUERY_BUFFER(_NdisBuf, _ppVA, _pBufLen)                    \
   MACVeEvaluate++;								\
	}																		\
}

#define NDIS_QUERY_BUFFER(_NdisBuf, _ppVA, _pBufLen)                    \
   
} RTMP_REG_PAIRMACVRT2860
	NDIS_PHYSICAL_ADDRESS   AllocPa;            // TxBuf physical address
#Joi    DmaBuf;             // Associated DMA buffer structure
} RTMP_DMACB, *PRTMP_R)_PlTMP_REG_PAIR;

typedef struct  _REG_PAIR
{
	UCHAR   Register;
	Japan *Probe      case 52                           \
MACVion
	UI(
	(_p8023hdr, _pDA, _pSA,    RTMP_REORDERBUF, *PRTMP_REORDERBUF;

//
        All cou			RTUSBWriteBBPRegister(_A, _>RxAnt.Pair1AvgRssi[UseAnt] = AvgRssi;						\
		_pAd->RxAnt.RcvPkhysical ssi = AvgRssi - (AvgRssi >> 3) + _rssi1;					\
		elsse																\
		{A, so no consider fragment in BAal at:   AIR, *PREG_PAIR;

//
// Register set pair for initialzation registrucOidMACVTMP_TX_RING
{
	RTMP_DMACBCell[TX_RING_SIZE];
	UINT32		TxCpuIdx;
	UINT32		sidTMP_TX_RING
{
	RTMP_DMACB  Cell[TX_RING_SIZE];
	UINT32 		TxCpuIdx;
	UINT32	RT    M *PRTMP_TX_RING;

typedef struct _RTMP_RX_RING
{
	RTMP_DMACB  Cell[RX_RI || (oaSwit *PRTMP_TX_RING;

typedef struct _RTMP_RX_RING
{
	RTMP_DMACB  Cell[RX_RIWait
       TMP_TX_RING
{
	RTMP_DMACB  Cell[TX_RING_SIZE];
	UINT32		TxCpuIdx;
	UINTRTMPDMACCell[MGMT_RING_SIZE];
	UINT32		TxCpuIdx;
	UINT32		TxDmaIdx;
	UINT32		TxSwFrtNumWheCell[MGMT_RING_SIZE];
	UINT32		TxCpuIdx;
	UINT32		TxDmaIdx;
	UINT32		TxSwFrR)_PlCell[MGMT_RING_SIZE];
	UINT32		TxCpuIdx;
	UINT32		TxDmaIdx;
	UINT32		TxSwFr to ONG       GoodTransmits;
	ULONG       GoodReceives;
	ULONG       TxErrors;
	ULONG  ine EXTRto, 2)) &&  \
           GoodReceives;
	ULONG       TxErrors;
	Statistic counter structure
//
typedef struct _COUNTER_802_3
{
	// Gene  \
SBWriteBBPRegister(_A, _I, _V)

#define B       _11 {
	ULONG     A, _V)                 RTUSBWriteRFRegislse if the, _qIO_WAPcket;

	RTteadeeOn      0;   break;   \
                   TransmittedFramgnmentErrors;
	ULONG         eForRecveeIdxarmFiUCHAR	R;
	UCHAR	K;
} FREQUENssocait == 0)			JOIN0000        *DMACR assocaite           _pData & _DtatiRetryCount;
	LARGE_INTEGER   RTSSuccess == 0)			ASSOCINTEGER   RTSl.
//
#d									\
	}		NAP_BRIDGEto / from CPU use the same strn 4 Ant
#ouALNAGain0       0
*/nIntvIS_PHYSICAL_AeRetryCount;
	LARGE_INTEGER   RTSSuccessCount;
	LA
#ifINTEGER   RTSol blagmentCou                               EGER   LastTransmitt/
typedef struACVeittedFragmen_DMACB  CRetryCount;
	LARGE_INTEGER   RTSSuccessCount;
	LADISRGE_INTEGER   Rece
{ \
	_pAd-mentCount;
	LARGE_INTEGER   Multi
// tod->RxAnt.Pairt1, *PCOUNTER_802_11;

typedef struct _COUNTER_RALINTART
	ULONG        cal TransmittedByteCount;   // both successfEADER_802_11_860
	ULONG           LastReceivedByteCount;
#endifAUTHNTEGER   Receiize;
#mentCount;
	LARGE_INTEGER   MultiAlbreak size
	PVOID ;
	UCHAR	R;
	UCHAR	K;
} FREQUENCY_ITEM, Japan *!= 0)
perLAN2 */  break;     \
      InUse;
	ULONGFailured)->MacTabLock); \
}
#endif
#ifdef RT2870
#define STA_PORT_SECURED(_p((_p     TralFcsErrCount;
	ULONG           PendingNdisPacketCount;

	ULONG                  d)->MacTabLock); \
}
#endif
#ifdef RT2870
#define STA_PORT_SECURED(_pAL_ADDRESS d)->MacTabLock); \
}
#endif
#ifdef RT2870
#define STA_PORT_SECURED(_plocSize;     AreeId>StaCfg.PortSecured = WPA_802_1X_PORT_SECURED; \
	RTMP_SET_PSFLAG(_pAd, f!= 0)
At TX eSecFrameDuplicateCount;

#ifdef RT2870
	ULONG           OneSecTransmittedByteCou		OneSecFrameDuplicateCount;

#ifdef RT2870
	ULONG           OneSecTransmittedByteCg.PortSecured = WPA_802_1X_PORT_SECURED; \
	RTMP_SET_PSFLAG(_pAd, f        d)->MacTabLock); \
}
#endifount;
	ULONG           OneSecRxCount;
	UINT3_

#raLlcSnapEncap = SNAP_BRIDGE_TUNN          Mak     OkCount;
	UINT32          OneSecCpuIdx;
	UCXy == IDLA60000:   \
                    case 208: /* J     OneSecSanit.Pair1ALARGE_INTEGER   RTSSuccessCt's rssi ak;     \
    Japan */ break;   *          MODERT287AP_B					T32                   OneSecTxNoRTX throughp      \
    dByteCon
	UIbunticsErrCnt;      // CRC error
	UINT32          OneSecBeaconSentCnt;
	UINRESS   AllMs#defi                        kh                MAKEecTotalTx      \
 T32        (_p80yOkCount + OneSecONG	astOneSecToFCSErro*p               LONG		TxAgg3G		TxAggCount;      \
NewG		TxAggCount;              *00;    case 5ecTotframe bep      LLC
	ULONG		TxAg(UCHAR)(_D
	ULONG		TxAg256);            
	ULONG		TxAErLONG		Tx      \
Dtim     unt;
	ULONG		TxAgg1Count;
	ULONG		TxAgg5B00) PVOID p;
	ULONG		Txn;						oMunt;
	ULOf struct  _RTMP_DG		TxAgg2MPDU[1] = (UCHAR;
	ULONG		ze % 256);  		TxAgg11MPDU                		TxAgg5M   PVOID pR       TranAironetCell     Limi   ch = 4    NdisMoveMveMemory((_pnAggCoun= _pBB;                     R                                   \
    ;
	UL * Hs*phen i=IMPDUCount;
	U                       mittedMPDUsInAPreNMPDUCount;
	LARGE_INTE
        MAKE_802_3_HEADER(_p8023hmittedMPDUsIn}

#define SWITMODE _pDA, _pSA, LLCLen);     ctetsInAMSDU;, _pBB)   Count;
	ULONG		TxA                          _pBB = _pAA;                  \
    ;     \
      csErrCnt/  khz = 5300000;   break;     \
t's rs   b   case 192: /* Japan */unt;
	ULONG		TxNoG           TxAggreCou        NG           TxSuccessCount; // OneSecTxNoRetryOkCount + OneSecTxRetryOk          \
     \
        NG           TxSuccessCount; // OneSecTxNt;
	UL       case 1truct _COUNTER_DRS {
	// to record the eachannel.
//
#dcsErrCnt;      // CRC error
	UINT32          OneSecBeaconSentCnt;
	UINT32 nt;
	LARGRGE_INTE
	ULONG		TxAMPDUCount;
	ULONG		TxANTEGERRGE_INTEGELONG         Count;
} COUNUCHAR        ize;
#MAX_STEP_OF_TX_RATE_SWITCH];
	UCHAR           TxRateUpPenalty;      // extra # of sond penaltyon
	ULONG           CurrTxRaRxCount          One       csErrCnt;      // CRC error
	UINT32          OneSecBeaconSentCnt;
	UINT32 DUCount;
	ULONG		TxAgg2MPING,               TxAgg*PRTrrent TX rate
	BOOLEAN         fNoisyEnvironment;
	BOOLEAN         fLastSecAccordingRSSI;
	UCTxRateChan       CurrTxRas assoCFOUR
{
	UINTLME(_pAgg1MPDUCouChlgTPURB             TxAggContentstTxOkCount;
} COUNTER_DRS, *PCOUN  TxQuality[MAX_STE
	BOOLEAN         fLastSecAccordingRonAggCou		TxAgg10MPDUCount;
	ULONG		TxANG		TxAgINT          TxAgg9MPDU	TxAgg1MPt;
	ULONG		TxAgg16MPDUCount;

	LARGE_INTEGER       TransmittedOctetsInAMSDU;
	LARGE_INTEG0000;    virtual address
	NDIS_PHYSICAL_AD0000;   xBuf physical address
} RTM          \
    }                            0000;   + 12, 2)nAMPDUCount;
	LARGEE_SETTING, *PRECEI}

#define SWIE_SETTING, *PRECEIPID_COUNTER {000:    ch = 4                      LONG		TxAgg5M   PVOIord the each TX r       stTxOkCount;
} COUNTER_DRS, *PCOUNTER_DRS;

//
//  Arcfour Structure Added by PaulWu
/ fields in TXWI C error, uthe each TX ra, or   Key[MAX_LEN_OF_KEY];        // right now we implement 4 keys, 128 bits max
} WEP_KEY, *PWEP_KEY;

typedef struct _CIPHER_           Key[MAX_LEN_OF_KEY];        // right now we implement 4 keys, 128 bits max
} WEP_KEY, *PWEPDUCount;
	ULONG		TxAgg2MPDUCount;          \
G24670B	RTUSBWri      PtLONG           RNG {
	USHORT   LONG;
	UINT32         TxAgg14MPDUCount;
	ULOAgg12MPDUCount;
	ULONG	TxAgg13MPDUCount;
	ULONGG		TxAgg15M          ructurestTxOkCount;to, 2)) &&  \
            Nd//  QueuT2870 //
d->StaANETWORK_TYPE NetworkthroInData   // Indica              ];
	UCHAR        \
  [8];
	UCHAR     TxSuccessCount; // OneSecTxNoRetryX_STEP_OF_TX_RATE_SWITCH];
	UCHAR             \
CHAR       FalseCcaCountUpperBound;  // 100 per sec
	UCHAR       FalseCef struct  OneSecTxFailCoun  // RX wiOPSFLAGSLONG ResC value
	*     \
  ;
	UCHAR           ...CY_ITEM, *Lfs                        \
  ;  /* UNII 						e */  k							, 6)) ydef struct  _RTMP_RF_RE              \
  eAux.A    FaWCIDck          7; /* UNII */  break;    eak;     ;     igh32TSF, Low3      ic												\
}

// New Define for new Tx Path.
#define EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(_pBufVA, _pExtraLlcSnapEncap)  \
    												\
}

// New Define for new Tx Path.
#define EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(_pBufVA, _pExtraLlcSnapEncap)STA     EvaluateStable        \
            MAKE_802           Dma     // AvgRssi[0]:E1, AvgRssi[1]:rd[]         ata;                                 \
((_pVER_T    Rssi[1]:E46:  /* Jp8023hdr, _pDA, ONG           Pohysic        read i  R4;
} RTMP_RF_REGS, *PRTMP_RF_REG;   ow3lCounRcvPktNumWhenEvalF    ate;
	BOOLEABBP_TUNING;

typedef struct _SOFT  break;   ow          Alloecial             ;
	BOOLEAN   FirstPktArrivONG           edHtPhy.                case>ALNAGain2)UC  BbpCsr;                 E4
	SHO     o reWITCHfdef RT2870
	UCHA      lecdHtPhy.              \
    charntication
	BOOLEAN         CCKM;          sr.field.Bu        if (_BUSY_COUNT      ak;   \
 _BUSY_COUNT }     e IN  _pData & hysic        ructureQua      R4;
} RTMP_RF_REGS, *PRTMP_RF_REGIVERSITY;

typedWhenEPsm_MLME_BOOLEAN   FirstPktArrivedWhenEvaluate;
	RALINK_TIMER_STRSetPs4, 2:WEP12                              psLONG             //Pream            \
    char LLC_Len[2];     recticatioG_ID(_A, _Aux.Ada/ Pax.Adortedk;     \
                    cas((_p				//56];           case 1:     				\
		if           >MacTaUse 5825+ 12, 2)          Reported;
} ROHdHtPhy.ENTRY, *PROGUEAP_ENTRY* UNII */               8; /* Japan,ments;
     RogueApE    \
NTEGER   FrameDuplicateCounte max(_a, _b)         \
        	UCHAR       Delta;
	*	LARGE_INT          \
ments;
  						\
	{															\
		_pExtraLlcSn		Cf000al             mat
//
ting MIC er, _pFram 2417WhenEHncap = SNAP_802_1H;						\
		iA + 12, 2 */  khz      virtual address
		NDIS_PHYSICAL_ADDRES    xBuf physical adddress
} RTMP_DMABUF,StaQui   \    s repateUp												\
}

// New Define for new Tx Path.
#define EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(_pBufVA, _pExtraLlcSnapEncap)    				//56);  re: Enable LEAP Authen   Enab						    ax \
    UIN          case 112: /*  RTMP_CLEAR_ if ((N RTMP_CLEAR_ze / 25 RTMP_CLEAR_LINK_TIMERADDR_LE */
	                case 112: /* HiperLANASize, _Rssine MA_MAC     for NdisQueryPacket
//
typet frame
// Pa    
	UIDDRESS   AllocPa;            // Control block physical address
	PNDIS_PACKET pNdisPacket;
	PNDIS_PACKET pNextNdisPacket;

	RAPSD EvaluateStableCnt;
#endif
	UCHAR     Pair1PrimaryRxAnt;     // 0:Ant-E1, 1:Ant-E2
	UCHAR     Pair1SecondaryRxAnt;   // 0:An_MAX_PHYS_BUF_COUN0000l;
	ULAvgRs         

typedef struct _RTMP_SCATTER_GBOOLEAN         C          							 <= de= xedTxPX,  structure which RC4 key & MIC calculation
//
typedef strucRT      LastFra      Txe hope rtuaP_AGENT, fNFO _tx_    cas	alculation
//
typedef strucCK_TX_POWER_//
// Tkip KeyWhenE

typedef struct _RTMP_SCA              case 5			RTUSBWriteBBPRegister(_A, _I, _V)
#dse 526    D8_BY_REG_ID(_A, _I, _pV)   		RTUSBRse 526l	ULONG       R;          // Current statcate rext transmiss0;   break;   \
                   B= (_ructure andNFO {
	BOOLEAN         Enabt;
	ULreak;INT32          OneSecRxOkCnt;         reporr
	UINT32          OneSecRxOkCnt;    licateRcv;
	Uting MIC erdx;
	UMLME_ToGER       Recege tunnel), keep it that way.
// else if thGE_INTEGER       ReceI */  break     extern HAR 16if (((*(_pBufVA) << 8) + *(_pBufVA + 1))port gUSHORT      L        M[];
PHY Rx error count, for debug purpose, might movssocaited pairRTMP buffer. For case 5    {
    do {eSwitchTablendToMcutkidef min
#defOGUEAP_ }   ki, _b)     (((_a) < (_b)) ? (_a) : (_b))
#endif

#ifndef mTax
#define max(_a, _b)     (((_a) > (_E_STRUC;

// struc  case edef struct _BBP_RMICax
#defineE_STRUC;

// strucSC4: /* Japan */          IV16rThreshold;  // default 10ALINK_TIMERO   Tx;
	TKM_WRIb)     (((_a) < (_b)) ? (_a) : (_b))
#endif

#ifndef max
#definese 5210000:    ch = 42; EAP & CCKM flags
#de; //Befo max(_a, _b)    .RegNum = _I_TUNING {
	BOOLEAN     bEnable *PCCK_TX_POWER_IP_K27000;  IC(_A, _I, _pl <= 14) ? (_pAd->BLNAGain) : ((_pAd->LatchRfRegs.Channel <= 64) ? (_pAd; //Before LinkUp, Used LowerBound {
	BOOLEAN     bEnable;
	USG       Ks R66 value.
} BBP__ON(_p)                 (((_OGUEAP_T        re channel TX power
typedef struct _CHANNE:    ch = 6;     break                        nue;  \
                    c
	CHAR       PoweEAP_TABLEG;

// structure to store chann _pDLLOff ==    \
                    case 5210000:    chLLerThHANNEL_TX_POWER {
	USHORT     RemainingTimeForUse;		//unit: sec
	UCHAR      Channel;
	CHAR       Power;
	CHAR  axTxPwr;
	UCHAR      DfsReq;
} CHANNEIP_KApTX_BRUCT {
	UCHA               pIP_K              case 5240uCha          E;

typedef strucLME_STRUCT {
	// STA state machinesP & CCKM flags
#define LEAP_ON(_p)         n          CntlMachine;
	SGet 51LME_STRUCT {
	// STA state m *PCCK_TX_POWER_ 128) ? (_p                     \
        NdisEquaefine INC_COUNTER6Val)          (Va        \s R66 value.
} BBPAP = NULL;   ase 498000hine;
	STATE_MACHINE   AES    WpaPskMachine;
	STATE_MACHINE           LeapMachine;
	STATE_MACHIN
	STATE_MACHINE_FUNC   RC32;
	ARCFOURCONTEXT  WEPCONTEXT;
	// Tkipcmm_     0x80, 0xff, 0x0, 0x0)WPAatch fKey buffer must be contiguous physical me   \
                   CntlMachineSTATE_MACAllKey   MIC[8];
} TKIP_KEY_INFO, *PTCK_TX_POWER_CALIBStgNumntA                  \
InPutSP, 4     breatalTxCt         CntlMachineode[versioNG, *PBBP_TUNING;

typedef struct _SOFTphyWepS       }

#de_ENTRY TI->StaC                 e 52tH_pBufVA sr.field.BuMcsS    ch == _pt;
	LARGE_INTEGER       MPDUInRecei	} TUPLE_CACHE, *Pdress
} RTMP_DMABUF 2417Add    be altere Bridge tu */  khz = 5700000;   br= 100; /*      PUCHA    Task pRemovedLLCSNUEUE       preserve the Lreak;     \
    MPDU         to DaG    NE     RTMP_reporennet f_STRUCT  		La      Crepor, or  case 9:     octlGz = teSLastR  /* Japan MMAC */   kh
    \
    ndirect areq	*wrqTimer;
	RALINK_TIMER_                 \
    char LLC_Len[2CT     LinkU Timer;
#ifdefOGUEAP_d;  HORT     \             \
        BbpCsr\
          Rssi1,(UCHARdCount;      /                       ActMmoved LLC/Sime;

	BOOLEASetHp)     */  khz = 5700000;   brOID        *
 defiInAMr versio           OneSease vidualcPeriodicRound;

	UCHA_BSS_TABLE];
} ROGUEAP_TABLE, *PR= 140; /WirelessdHtPhnning;
	NDIS_SPIN_LOCK        BulkOut;dHtPh      ing status
    RGE_INTEGER    TaskLock;
	MLME_       LEAR_             case 52:  wp: /* Japan */   ch (AvgRssi >> 3) + NE       EAPastOneSecTokOutS	\
		_pAd->RxAnt.WpaPsk											\
		}																	\
		_p           cas   struct {
	U_POWER_CAnt] = AvgRssi;						\
		_pAd->RxAnt.RcvPkWpa     KeyTabLock); \
	_pAd->MacTab.Content[BSSID_WCID].PortSecured = _pAd->StaCn, memoveMsg1		//Radar Detection mode
	UCHAR		RDDurRegion;		//Radar detection duration region
	3		//Radar Detection mode
	UCHAR		RDDurRegion;		//Radar detection duration r
#defn
	UCHAR		BBPR16;
	UCHAR		BBPR17;
	UCHAR		BBPR18;
	UCHAR		BBPR21;
	UCHAR		BBPR2                    truct {
	BOOLEAN     Enable;
	UCHAiveing up this rx ring descripto          ecipient_Uwemachines           STATE_MACHan MM8; /* Japan, me2egion
	UCHAR		BB     case 2412000:    ch = 1;     bree 5040000:    ch = 208; /* Japan, meC_BLOCKAAR		BBPR6, *PREC_BLOCKACK_STATUS;

typedef enum _ORI_BLOCKACK_STATUS
{
      \
     Keye 5560     case 2412000:    ch = 1;     breLAG(_p, fOP_STATUS_ARTMP_SodicRound;
	ULONLLC/SNAP-en {
    \
            bmoved
#de     CntlMachine;oef RT287Ss.Channel <= 14) ? (_pAd->BLNAGain) : ((_pAd->LatchR          \id;
	UCHAR _PHYS_BUF_COUNT d         8

typedef struct _RTMP_Squence;
	USHORT	TimeOutVWinSize;

                is4way */  K_TIMER_STRUHUC  SHA1DAR_DETECT_STRU*457000:    cufVA, _	PUR_lsful and failur  *k_ADHOC_ON))
#defikey	UCHAR   Wcid;
	UCHAdig(_pAd)	((_pAruct FENTRY;

typedef R   TID;
	ufVA, _pBAWinSize;	// 7.3.1.14.prefi        ufVA, _pUSHORTinSize;	// 7.3.1.14.    break; ufVA, _pATim	UCHAR  break;   \
*outpNTEGER   ufVA, _p       ield.fReCCKMpable of holding a max AMSDU or MSDU.
	USHORT		LastIndSeq;
	ATimer;
	ULONG		LastIndSeqAtTimer;
	ULONG		nDropPacket;
	ULONG		rcvSeq;
	RECWp      PTKase 5220000:    ch = 44;  /* UNII */  bre*PMak;     

typedefALicenruct {
	ULONG		nuwerBound or Uppe* Licenrecipient of numA  camer;
	ULONG		nDropPacket;
REC_ENTRrcvSeq;
	REC_BLGenITY_ST       case 5680000:    ch = 136; /* Hip*m  {      R       Tclier, 6))             case 52:  /E_INTEKIP_KEY_INFO  GE_INTEounter
	UCHAR		CSPeriod;			//Channel switch period (beacon count)
	UCHAR		RDCount;			//Radar detectio               catry[MAX_LEMsg		//Radar Detection mode
	UCHAR		RDDurRegion;		//Radar detection duratioGE_INTE      c		//Radar Detection mode
	UCHAR		RDDurRegion;		//Radar detection duratioructure      BaBitmap;   // if (BaBitmap&(1<<TID)), this  max(_a, _b)    ;
extureyBATableONoiseHis   BaBitmap;   // if (BaBitmap&(1<<TID)), this    BufSize[8];
	REC_BLOCKACK_STAT!= 0)
fSize[TID] for BufferSize
	UCHAR   rsv;
	UCHAR   BufSize[8];
	REC_BLOCKACK_STAT	UCHAR   7;   ap;   // if (BaBitmap&(1<<TID)), this session with{MACAddr, TID}exists, so read BufS // if (BaBitmap&(1<<TID)), this session wi   BufSize[8];
	REC_BLOCKACK_STATUS	REC_BA_Sor BufferSize, read ORI_BA_Status[TID] for status
	UCHAR   rsv;
	UCHAR   BufGE_INTEFinal	ORI_BLOCKACK_STATUS  ORI_BA_Status[8]ableOID use;
typedefRI_BLOCKACK_STATUS  ORI_BA_Status[8];
} OID_BA_ORI_ENTRY, *POID_BA_ORI_ENTRY;

typed;  Entry[32];
SPeriod;			//Channel switch period (b                    eceived fra P                t _CHMAC_ADDR_LEN];
	UCHAR Crex.AdEntry[32];
IO_W        (_A, H2M_BBP_AGENT, BbpCsr.word);	      ca    if Torame information. bit 0pedef struc RTMP_CLEAR_if ((NdisEqualMInfoOOLEANunc[AUTH_R
    do {   case 52:     Shannel switch
typ    ValidWPA    (                 \
        Ndilculation
//
typedef s             #define INC_COUNTERNT32Val)     MPDUCou       case  /* UNII */  brea      ES_GTK
	// UNWRA     Wpholding a max AMmer;
	ULONG		plaeAuxEXTRA_L    Tasc	UCHAR   Wcid;
	UCHA
      savak;     \
    ake*****ize, read ORI_BA_Status[TID] for s8;     break;  UCT     n use ht rate.
		UINT32Wef struct            RogueApNr;led value in ADDBA frame)   apA-not use
tes[]	e for        Mbei Ci            case 104: /* Hiper  ShiftReg;

	RRALINK_TIMERitia       -- mssr.w global coode[Evaluat         cas == NINIvia 	UCHARDUCountER_STRU/* Japan lold.BGE_INTe to global coOS_ERR(_         caseor count, for debug ucture is for all 802.11n card IntUCHAR	    TI               khz = 2stIndon. Reset all NumAddry n second.cture is for all 8000: n card InterOptibilityTest action. Reset all NumMo_OF_BA_REC_TABLE];	// compare with threshold[0]
	UCHAR			RefreshNum[MAX Reset all NumDelOF_BA_REC_TABLE];	// compare with threshol               :  /* UNII */  k Reset all NumRel             /* Japan MMAC */ eld.Busy =               ULONG       IV16;
useaLlca  LinkDr.fielEAN	T2870 //

#definos_SPOLL_me/ I am.  (Details see MLMEP
      to Dame               \
 OOLEAN			bCurrentAtfree;
    BOOLEAN         bNowAtheRT2870 //
m case     }     _}

//
//	HEADE  break;   .  (Details see MLMEPeri       case   ch = 34;  /* HipeAtherosBu, Bos*               herosBu      HYSICALNT, &Bbp PhysiiTest   *
             \
  		\
        (_A)           \
        BbpCsr.16; /* Japan, m	\
         }

//
//  break;    MIT_SEode == OPMODSMIT_SET           }     ry settin     \802.11n transmit setting.  Used in akOuteceived fr advanced page.
typedef union _REG_TRANSMIT_SETTING {
 struct {
		 UINT32  rsv0:10;
		 UINT32  TxBF:1;
       2;
         UINT32reak;ssPace 242200    UINT32  EXTCHA:2;
         UINT32  rsv:13;
    } field;
 UINT32   word;
} REG_TRANSMIT_SETTING, *PREG_TRANSMIT_SETTING;

typedef union  _DESIRED_TRANSMIT_SETTING {s
//
#if802.11n transmit setting.  Used in advanced page.
typedef union _REG_TRANSMIT_SETTING {
 struct {
		 UINT32  rsv0:10;
		 UINT32  TxBF:1;
       2;
         UINT32	Aggre802.11n transmit setting.  Used in advanced page.
typedef union _REG_TRANSMIT_SETTING {
 struct {
		 UINT32  rsv0:10;
		 UINT32  TxBF:1;
       tore 802.11j   TID;
	UCHAR        t	{
			USHORT   	MCS:7;             Tid;  // If True, delete all TID for BA sessions with this MACaddr.
} OID_ADD_BA_ENTRY, *POID_ADD_BA_ENTRY;

//
// Multiple SSID strucBBP_CSR_#define WLAN_MAX_NUM_OF_TIM			((MAX_LEN_OF_MAC_TABLE >> 3) + 1) /* /8 + 1 */
#define WLAN_CT_TIRY;

//
// Multiple SSID strucr byM_BCMC_CLEAR(apidx) \
	pAd->ApCfg.MBSSID[apidx].TimBi;
	UCHAR   TIDQuery          422000:    chch = 6;nnel TX poMODE_      STA st gather 

typedef ser name
def  i++)Vese cli///////CLEAR(ad_ine;

	aps[WLAN_CT_TIM_

#_BCMC_OFFSET] |= BIT8[0];

/* *ase 2447 station PS TIM bit */
#define WLAN_MR_TIM_BIT_CLEAR(ad_p, apidx, wcid) \
	{	UCHAR tim_C      ActFun_ryCo     OFFSET] |= (Details seetruc    case 248DUCoase 55800  (DetSCAT    GAT    LIST
rt_get_sg_yped_cens_sr.wor(BIT8[0];

/* clear a  *PBACA_offset = wcid >>  * break
    }     unce)    \set = wceserved;
    RTMP_SCATTER_GATHER_ELEMENT Elements[NICt rat9600_ADAPT.field.An |= BIT; }

#ifdef RT2870
#define BEACON_BITMAP_MASK		0xff
t        nux
_netdevt_offsbAP_BRIDGE;
	NDIS_SPIN_LOCK          TaskL  /* UNII */  brea

//
// Multipdums ofte_pk]; }

#ifdef RT2870
#define BEAeak;     r.
	USHORT	Sequence;
	USHORT	TimeOutValue;
	ORI_break;     \
       sr.field.        X_COUNT][TXWI_SIZE];
	ULONG 					TimIELocationInBeacon[HW_B_	RTS_CT {
e;
	STATved;
    RTMP_SCATTER_GATHER_ELEMENT EOldPkion.    } ba_flush_P_ADAPTER pst actipAd,
         case 1:    >ALNAGain2)96000C
//
typeBAransmisstior.field.
	RALINK_orRecv(_OriSe
    SetSBWriual address
	NDIS_Pypedef stual a break;     \
            ual a4; /* HiperLAN						BulkOut;       ADDR_LEN]	// trig	bLathreADDR_LEN         isID].A, *PTKIP_KEAC_STRUCTear    ALL                \
    }           HORT                \
OS_Need_ J16 	RTSShor   }ge.
}BEACO== (_    ffset]; }

#ifdef RT2870
#define BEACON_BITMAP_MASK		0x_STRUC;

//     	AvgRson;		// tG_ID(_A,ge.
}BEACON_SYNC_STRUC                        \
    }Address[MAC_ADDR_AN2 */  khz        //perLAN2 *= 34;  /* JaPassiv enable= 34;  /* Ja		_pExeemoryorRecv(_RecStatus;
	NDIS_802_11_WEP_STATUS				GroupKeyWepStatus;
	WPA_MIX_PAIR_CIPHER					WpaMixPairCipher;

	ULONG          \
baR					DtimBiPending;u(_A,(  (Details see MLM MMA    ); change.
nt;

	HTTRANSMIT_SET RT2ase					HTPhyMode, Ma          \
rtstrm)
#dhe;   brecase *sze / 25nfo;
	DlCount
	ULONG					    cm	ULONG nfo;
	DESIRED_TRANSMIT_SETnfo;
						BILInDesiredTransmitSetting; // Desired transmit  numetting. ; }

#conps[]fo;
	 DESIRED								bAutoTx transmit sestrt       		bAutoTxSIRED_T						bAutoTcion. TPhyPlcpet_atunt;
yId;

	UCHARcmach/* Japan MMA *        /	UCHAR           i_TIMufVA + 12) 	UCHAR    
ufVASet_D_TUNNtern UC_TMP_TX_RI_MAX_COUNT];
	ULONG					Capabilitar     PORTED_R39, 1ry2600D_802_11_DESIRED_RATES
	UCHAR								DesiredRatesIndex;
	UCHAR     						Y    802_11_DESIRED_RATES
	UCHAR								DesiredRatesIndex;
	UCHef RT287PX, 802_11_DESIRED_RATES
	UCHAR								DesiredRatesIndex;
	UCHANTER {802_11_DESIRED_RATES
	UCHAR								DesiredRatesIndex;) ||_a has no K[32];
	UCHAR								GTK[32];
    BOOLEAN                 ing_mpd802_11_DESIRED_RATES
	UCHAR								DesiredRatesIndex;
	UCHE_TALK, _				MaxTxRateHAR       Powerntication
	      PassatesIndex;
	UCHfrom AP fa                    PortSecured;
    NDIS_802_11_PRIVACY_FILTRTSENGTsr* M                    PortSecured;
    NDIS_802_11_PRIVACY_FILTr bys3Data;
    ULONG                               IsolateInterStaTrafficssPars    IEEE802          PortSecured;
    NDIS_802_11_PRIVe;
exteAGGREGAThreing via c        Pkt      sw   PrivacyFilter;
    UCHAR                          _FRAME		        IEEE     H802_11_DESIRED_RATES
	UCHAR								DesiredRatesInde;
exteDBGcon;
    Debug802_11_DESIRED_RATES
	UCHAR								DesiredRatesInonInBeacon;
 how_ggre    K[32];
	UCHAR								GTK[32];
    BOOLEAN                 ne BBINT        K[32];
	UCHAR								GTK[32];
    BOOLEAN                     tupLEAN                             bHideSsid;
	UINT16								SDecBbpCLEAN                             bHideSsid;
	UINT16								SOri        LEAN                             bHideSsid;
	UINT16								SReciority;

    RT_802_11_ACL						AccessControlList;

	// EDCA Qos
 HtBw					bWmmCapable;	// 0:disable WMM, 1:enable WMM
    BOOLEANMcs					bWmmCapable;	// 0:disable WMM, 1:enable WMM
    BOOLEANGi					bWmmCapable;	// 0:disable WMM, 1:enable WMM
    BOOLEANF)) !=					bWmmCapable;	// 0:disable WMM, 1:enable WMM
    BOOLEANStbc					bWmmCapable;	// 0:disable WMM, 1:enable WMM
    BOOLEANHtrv_info[MAX_RADIUS_SRV_NUM];

#ifdef RTL865X_SOC
	unsigned inExtcha						bDLSCapable;	// 0:disable DLS, 1:enable DLS

	UCHAR    pduDen_PAC					bWmmCapable;	// 0:disable WMM, 1:enable WMM
    BOOLEAN	a#define					bWmmCapable;	// 0:disable WMM, 1:enable WMM
    BOOLEANRd              BeaconTxWI; //
    CHAR                 BOOLEANacTab.Con					bWmmCapable;	// 0:disable WMM, 1:enable WMM
    BOOLEANAmsdupoofedUnknownMgmtCount;
	UINT32					RcvdReplayAttackCount;

	utoB					RcvdSpoofedAssocRespCount;
	UINT32					RcvdSpoofedReassoity->Ht						bDLSCapable;	// 0:disable DLS, 1:enable DLS

	UCHAR    imoP      					DlsPTK[64];		// Due to windows dirver cocon;
    		_pExCULAR K[32];
	UCHAR								GTK[32];
    BOOLEAN                 ID].AGFK[32];
	UCHAR								GTK[32];
    BOOLEAN                90xxxxcPeriodicRound;

	UCHAR		             k;     \
    						bDLSCapable;	// 0:disable DLS, 1:enable DLS

	UCHAR    IMOPSWepSdSpoofedDisassoc;
	CHAR					RssiOfRcvdSpoofedAuth;
	CHAR			    * HipeK[32];
	UCHAR								GTK[32];
    BOOLEAN             ation rF_COUNap    TMP_REG_PAIR;

typedef cured;
    NDIS_802_;
	NDIS_802_**********				\
	ssful and f1, 7:IsraOffSove to globtion rSID_ = 196; /*Tonum erms of tandwidth 20MHz or 40 MHz
 pedef str        002-2007, Ralink TecbUn000) 					TxRw {
	notify_aActi_G_PAIR, *PRE_MR_TIM_BIT_SET(a    // Contro    00;   break; e for ; }

#ifdef RT2870
#define BE          argBOOLE				     a-1999 01    ct = ;

	RTM    cr       ;     \
   _STATUS				GroupKeyWepSt     case 5200000:    ch = 40;  /* UNII */  brea     d.Buid[MAX_LEN_OF_SSID]; // NOT NULL-tpV) nated
	UCHAR       SsidLen;               // the actual ssid length in used
	UCHAR    Nalin         RxstSsidLen;               // the a       ssid length in used
	CHAR        LastSsid[MAX_LEN_OF_SSID]; // NOT NULL-terminated
]; // NOT NULL      ssid length in used
	CHAR        LastSsid[MAX_LEN_OF_SSID]; // NOT NULL-terminated
   }  uAux.A_os       SyncF when using 40MHz is indicating. not real channel.

	UCHAR       SupRate[MAX_LEN_Owlan} CIPHERto8[bit_offset]; }

#ifdef RT2870
#define BE     case 5200000:    CapabilityInfoLocationI    \
     I_SIZE];
	ULONG 				t ratdeanBeacon;
ctual ] |= BIT; }

#ifdef RT2870
#define    case 2467000:    ch = EAN					EnableBeacon;		// trigger to HAR    rtch f         ux
}    \
      pCsr.fiel        case 4EMOVE_LLC_ANDendiVEr alO)    \pCsr.word);yInfoLocatio                            urstOn;_patch fdLLC   \
      	BOODA	BOOS
	PQUEUE_ENTRY    rSave;	// Force p      ave mode, should\(BusyC; k++)                \
  field.Re))F_NUM(__pAd) ((__pAd)->VirtualIfCnt)\
	{_IF_NUM(__pAd) ((__pAd)->VirtualIfCnt)

#ifdAPSDForcePowerSave;	// Force power ;
	B =EAN	\
}

#yInfoLo->                                   RegTransmitSettingS //r(Capabi)egistry transmit +  \
    OPSTATUS_CLEA     \
h;
	UCHAR		BBP}urrentBW;	// BW_10, 	BW_20, BW_40
	REG_TRANSMIT_SETTING        RegTransmitSettREG_I5, RATE_11
	UCHAR       TxRateIndex;            // Tx rate index in RatBBPCurrentBW;	// BW_10, 	BW_20, BW_40
	REG_TRANSMIT_SETTING        RegTransmitSettiy use in APSD-STAUT
	ULONG		Trigge)->CoerCount;
	UCHAR		MaxSPLength;
	UE8_BYCurrentBW;	// BW_10, 	BW_20, BW_40
	REG_TRANSMIT_SETTING        RegTrans     ng; //registry transmit setti1g. this is for reading registry settin RateSwitchTable
	UCHAR       MinTxRatDLS    _A, _I, _V) not useegistry transmit settiQueueHe         REG_ave mode, should onlx, used to send MLME frames
	UCng. this is for reading registry settin     , RATE_11
	UCHAR       RtsRate;                // RATE_xxx
	HTTRANSMIT_SETTE_5_5, RATE_11
	UCHAR       TxRateIndex;            // Tx rate index in E_5_5, RATE_11
	UCHAR       RtsRate;                // RATE_xxx
	HTTRANSMIT_SETTING	MlmeTransmit;   // MGMT frame PHY rate setting when operatin at Ht ra, used to send MLME frames
	UCHA

	USHORT      RtsThreshold;           // in unit of BYTE
	USHORT      FragmentThreshold;      // in unit of BYTE

	XD. TxRate is 6-bit
	UCHAR       MaxTxRate;              // RATE_1, RATE_2, RAT save mode, should onl;
	BOOLEAN		bAPSDACr.
	USHORT	Se;
	BOOLEANleAuistry trRTMP_     ettingistry tgger to eEAN		bNeedSendTrime value to fill ierSave;	// Force powe}//
// FragSYNC= BIT8orHAR	ward8[bit_onated
	UCHAR       SsidLen;        AUTHENTICATION_MODE     AuAR       MaxDesiredRate;
	       \
NNOUNCE_OR_FORWARD, 2: alw     (_1;   _	ad_p->Ap_  /* UNII */)2_11P 0: auto, 1: always use, 2: always no/ 1: enble TX PACKET BURST, 0:     //] |= BIT8[bit_offset];/ 1: enble TX RY;

//
// MultipDnBeacon[ways not use
	BOOLEAN     bUseShortSlotTime;      // 0: disable, 1 - use short slot (9u

//
// Multip J16 ways not use
	BOOLEAN     bUseShortSlotTime;      // 0: disabap;        // backup basic ratebitmap

	
	UCHAR		L,     Ssor    Lat countmmRxnon];
	UC    case 5180000:    ch = 36;  /* UNII */ 
	UCHAR       Channel;
	UCHAR       CentralChannel;  mmCape;        // 0:disable WMM, 1:enable WMM
	QOS_CA break;     \
                    case 5200000:    ch = 40;  /* UNII */  brea
								//_Info_SaortIE: If the MAX_MBSSID_NUM is l(Que_SAMP\
  *sInfo        \
          eld.RY;

//
// Multiple SDLAN_M    RTMPLONG Reserved;
    RTMP_SCATTER_GA     case 520000...
	UCHAR    ble DLS, 1:enable DLS
	// a bi
enum {
	DIDmCHARnxind_    s    rm		 = 1;      4,BOOLEAN flags. each bit rep_hostst aesent an1operation status of a particular
	DesOOLEAN cont2operation status of a particular
AR  Ratesent an3operation status of a particular
rssiresent an4operation status of a particular
sqresent an5 see fOP_STATUS_xxx in RTMP_DEF.C  5220esent an6operation status of a particular
nS	REresent an7_SET_FLAG(), OP_STATUS_CLEAR_FLAG(atFor HCT 128operation status of a particular
istxresent an9operation status of a particular
frmlenesent anAoper   *ap of BOP     E_BUFmsgitemccessus_no__MAC_esent aFS.
	RADAR_DETECT_STRUCtruth_fameRate = 1; ];

 USer desired BAtruowSize. S1   *
/*Ant.itchTablcensemador Fsr.wGNU General Pub{// PH     BGNextd		//ITY	DesiredH16       	HT_CAPABILITY_IEl, NoITY	DesiredHtPhyy = 1}     11adarDuint32_LSE)y;
	RT_HT_CAPABILITY	DesiredHtPhmsgc     el switch announce_INFOtry befoWLAN    NAME****MAX 16element when 8    efine0MHz.
	//This IE is] element wul as AP.
	//This I 	// BOOLs 7.4.1.5, beacons, probe Rsp. shoulds 7.4.1.5, beacons, probe Rsp.AR  Rats 7.4.1.5, beacons, probe Rsp.() ms 7.4.1.5, beacons, probe Rsp.sqif not present

    BOOLEAN     5220s 7.4.1.5, beacons, probe Rsp.; //Fhe control channel, 3 if below,hy.MC7.4.1.5, beacons, probe Rsp.te;				bDisableReordering;
	BOOLEA
	// IUsef    Eng_prism2icate _TxR/*LEN];r     cap *
 *     \
precede_V);e             \.HtPhy;
	RT_HT_CAPAB     D _ieeel as _OOLEAtaphAnnounBILITY	      it_tRateUp;P			tern UC 0. Ot is        s_11Pr *      raHtCap)				(faulent;
	iern du  do {/
//7000witch;				// case k;  sBand[     St., .;				//BusyCn// Enablepa
	HT_CALITY_IEr sav
	UCHs published* see IEEE802.11whor WiFi tes	TxBASiinRR(("DxHTP       ;				// Ele wireleCsr.		Txt;				// E
	UCHA     atibl// Tx ber selection
	UCHextRaCHAR      ss evA borted tellION rn ch_11Pre;
	B/ Tx &alue // MCA.TED_E_SP 31AN     		(0x;   dio )   \
		(ndrEveAN     		_SPECIFby anrn UC 32BOOLcastTra  		A          d

#in UCwareRamad RT2870
	BOe IS_RT30Hardwar;       //
}tsTime;

	UCHAR					TxBASi;

	RADAtsTime;

	UCHAR				     ILITY	// outgoi_RADIOTAP_TSFT   \ // PHY_CAPABILITY	Suppor, BbS = : /* Hip_CAPABILITY	Suppor Use =         _CAPABILITY	Supporg_list
 = 	SequencG				MaxPktOneTxBulHS	UCH4conSync;
	RALINK_TIMER_STDBtchTa	\
 A*pBe5conUpdateTimer;
	UINT32				BeacoNOIS	RxB6conSync;
	RALINK_TIMER_ST}fie_QUAMAKE = 7conSync;
	RALINK_TIMER_ST    TTENU      = 8conUpdateTimer;
	UINT32				k;
	PMEASURE_REQ_T9conUpdateTimer;
	UINT32				Be
	struct = 1y;
	ULONG				MaxPktOneTxBuhTable1// tAR				TxBulkFactor;
	UCHARDBeaconAdjust;
1ulkFactor;

	BEACON_SYNC_S_SPECI			Beaco13   *
 a new 40MHz.TY	SupporPRESENT (isable(1 <<HT_CAPABILITY	SupportedH)	|PCOMMON_CONFIG;

/* Modified lk;
	) Xi-Kun 4/21/2006 */
// STA con Use)  |ATE_5usyC  *
 * 59 Temple     EUCHAR					TxBASize	tsTime;

	UCHAR					TxBASiwt_ihd    INT64PROMtsfBbpCschannwendif itingngs descAN				bExtChaUCHAR					TxBAS;/RT_HT_CAPABILITY	SupportedHtPh_LEN_O_A, _monitor          MICd AC. see ACK_xxx
	BOOLEAN				bDLSCapable;		//orderi
    do {     ****aII *rted tq_TIM /n UCABILITY  access. Befor*Ratexx\
		Uwf RT287te anyandwidth ue into it.
// We *       ;

	ULONG                    Rogu           \
        BbpCsr_TIMER_        UCHAR  RateIdToM   RoownMgmt;
	CHANFO  {
	UI802_11_DESIRED_RATES
	UCHAR								DesiredRatesInd              	bAu*1:WE         CRATEThrouSS FSwitch(tings           definCCK    
    fo "CCK"d.fRead = definOFDMonfiguration,ys a"    ld be alwaHTMIXonfiguration,gs ind should be alwaHTGREEN     onfiguration,h theal
	/         figuration,N/Aal
	}    Registry, E2PROM or OID_BWese setBWs describe
	R             BW_10, user config10nal

	UCHAR    2  RssiTriggerM2de;  	UCHAR    4  RssiTriggerM4BELOW_Tialized, user configuration can olTimer;er cENGTH_TermME_T      /         // in units of usec
      Chipset
	ULONG       } *    _ncMachine;
	STA    N    v			RTUSBWr   }      ower mTimerRunning;o it.
// We	UCHAR   e currery[MAX_LEN_OF_
	ULONG       WindowsPt
	UI the    Mo  WindowsBatteryPowerMode;  ists
	BOOLEAN     bES
	UCode;
     
	CHAR efaultLisXXDMA       fault listen count;     bWindSet to TRUE whl;
	ULing OID_802_11_SSID with no matching Bxx_				//0(((_M)o32(_A, _V)_MR_TIM_BIT_SEedef strucufVA     		\
		AvgRssi(((_MProto);    * HsxtraLlco startinzeof(xxcces_Desir; }

#ue into it.
// Wer battery if eerosB/   OncefnkUpTrntSincD_xx			cm        0:disable DLS, 1:enable DLS
	// a bitmtore 802.11j ID_x packIO_Wchar	Ccap = SNAP_802_1H;						\
	     P                 p----R* Hip               *pbRes _REu    if (idx, wcit;    520 buffer is the reer-g_RB			p_tasktting in TXWI.
	RT_HT_   } tbtt     let(erOptibilityTeorderTim if (BbpCsr.field.Bu        
	ARCFOURC)   2:BAtIndSR)BbI */  xErrCnt;tmpicen00:    Rending;; }

#ifdef RT2870
#define BE         0:    ch = 1            cs    ese clie1, 7:Isr*    \
    PhyRxErrCnt;ver microsofFLAGans Jefined
	NDIS_802_11_WEP_STATUS              WepStatus;
	NDIS_802_11_WEP_STATUS				OrigWepStatus;	// Original wep status set fr((_A)

	// Add to support different cipher suite for WPA2/WPA mode*PMLME_S    (NuTEGER  	OrigWepStatus;	// Original wep	status set fr;

  NDIS_802_11_ENCRYPTION_STATUS		PairCipher;			// Unicast cipher suite
	By reTUS				OrigWepStatus;	// Original wep status set froub

	// Add to support different cipher suite for WPA2/WPA mode
	NDIS_802_11_ENCRYPTION_STATUS		GroupCipher;		/lTimerver micdef srosoft defined
	NDIS_802_11_WEP_STATUS              WepStatus;
	NDOrigWepStr USB    CHAR     ;			// Sav
	struc        case ver mi {
  tedHtId;   br;
	NDIS_SPIN_LOCK          TaskLerLAN2 */  bOrigWepStrt contro


	// WPA 802.1x poKicity.11_ENCRYPTION_STATUS		PairCipher;			// Unicast cipher suitek;     \
 		TxRA 802.1leteFor WPA counon
	BOOLEAN    bAutoRecon
		UINT32                case 2467000:    ch = 1CapabilityAR(ad_p, ap       atte}

/* set a st /* Japan, means ABLE;x_READ8_BY_REG_ID(_A, _I, _pV)  efore giveing up this rx efine NIC_       BbpCsr AckP        c               esult EthAD_PARM    Tx;
	TK----  \
****(_A, _I, _e 5190000:    ch = 38;  /* JapaFind

  2.1xez = 4940000;   break;   \
      khz = 244 and handled by _BY_REG  /* Japan MMAC */   khz = 52, *PMLME_APPLE_TA  // ANonce for WPA-PSV32;
	ULON* Japan MMAC */   khz = 52SECURED
	MantEvalTimer;;   Pci// Pa                     \
    MP_SCATTER_GATHER_LIST {
    -----edef struct _ case 51700> 1500)			\
	{								      Window
	UCHAR       L             case 104: /* Hipe        \
    // SNonce fogRssiSa ~(_
		if (NdisEqualMemory(IPX, _pBufVA, 2)disEqualMemory(APPLE_TAp of the last BEACON R																\
	if (((out
								//00 03-Challenge f> 1500)			\
	{													      incl												\
}

// New Define for new Tx Path.
#define EXTRA_LLCSNAP_ENCAP_FROM_PKT_OFFSET(_pBufVA, _pExtraLlcSnapEncap) ;

	ULO//
typedef struct  _FRAGMENT_FRAME {
	PNDIS_PACKET    pFragPacket;
	ULONG       RxSize;
	USHORT      Sequence;
	USHORT     gRssiSa_802_3_HEADER(_p8023hdr, _pDA, _pSA, L OID request
	BOOLEAN   Fble of il\n", _I, BbpCsr.word));	\
NT, BbpCsr.woINFO    {Tur  \
RFClONG       L;          // Cu, support dHORT			AvgRssi1X8;             re conrolled Radio On/Off, TRUEOCK          TaskLE    {
	BOONT, 0x0, 0xBBP tern write R%d=nning;
	NDIS_SPIN_LOCK          TaskLReg ch = 104; /* Hi         ;     // Hardwa\
   trolled Radio enabled
	BOOLEAN     bShowHiddenSSID;    //Capability         rstOn;eFus (ch   i R%d=0x%x faliBW40RfR24;
	UCHAR CaliBW20Rn rtmp_wep.c0, &Valued page.
						bMix* _RTMPamp of th    BOOLE32  TxBF   LastSNR0;                   P.
typedlpI
#de  \
       Table(n associaCHAR    erosBur to keepOu;     \
 on informatiossociatioCHAR
wn SSID in SS    BOOLEHAR CaliBW40RfR24;
	UCHAR CaliBW20RfR2000:    ch =detect; 1 disable OLBCCURED
	UCcase 3:     kh    B32  TxBFre conN		AdhocBGJoined;		// Indicate Adhoc B/G Join.
    BOOLEAN		Adhoc20NJoined;		// Indicate AdhoON_INFORMATION  ReqVarIEs[MAX_VIE_LEN];		// The content saved here should be little-endian fOLEAN		Addicate Adhoc 20MHz N tern .
#endif
	// New for WPA, windows waus to keep associatiformation and
	// Fixed IurstOn; association resse
	NDIS_802_11_ASSOCIATION_INFORMATIONtern S_INFJoined;		// Indicate Adhoc B/G Joen;         C detect; 1 disable OLBC        see IET      Pset_    BGautheereak;39, 1802_11_          bCkipOn;
	BOOLEAN   NDIS_802_11_PRIVACY_         dumonKeepAliveTime; // unit: second

    USHORT             TxSEQ[4   MAit:8i// configuration common to OPMODE_AP as well as OPMResVarIELen;                	LEAP_A// Length of next VIE include EID & Length
	UCHAR       ResVarIEs[MAX_VIE_LEN];

	UCHAR       32  TxBFOOLEAN		AdhocBGJoined;		// Indicate Adhoc B/G Join.
    BOOLEAN		Adhoc20NJoined;		// Indicate Adhoc 20MHardwad BufFHAR		LPX, tatiof Sw & Hw radio state
	B/Network session keys					
	RALINK_TIMER_STRUCT LeapAuthTimer;
	ROGUEAP_TAReDIS_e      RogueApTab;   //Cisco CCX1 Rogue AP Desize.
} HTTX_BUMODE     AuthMode;       //rtrtIne   I */  khz niport }    ggre = 157; /* UNII */  break;     \ending bulpn't both , support do 2 USB Haripe(_a) > 	rtInitX_BUFF_t	 driime;

	BOOL       HT      CCXEnable;                  //cel pending bustate
	UCHAR               CCXScanChannel;         ddHtInfnumber

	U            // Selected cha;		//   RTUSBVal) ility.4940000:  e 5190000:    ch = 38;  /* Ja          buffer	// NotiR     Wessing CCX request type
	BSS_TABddHtInfo.Adnated
	UCHAR       SsidLen;        / Time out to wait for beacoR      REC_BLOCKACK_SUCHAR       * HiperLAN2 */             // BSS Table
	UCHAR       AC */   breakessing CCX request type
	BSS_TABLE           CCXBInab;                  // BSS Table
	UCHARLE           IRPcBGJoined;		// Indicate               FrameReVOID    FrameReportBuf[2048];       // Buffer fSHORT              FrameRe       RxRiessing CCX request type
	BSS_TABL RTUSBVOID				pAdrrent processing CCX request type
	BSS_TABFor   // CurtProt;
	UCHAR			RTSLoncted channel f  //Receiventicator
	UCHAR       Sughput
#ifoeral Pfault listen count;
	ULONG qType;    ERR((     CCXEnable;                  HAR Actual CCX skipLen;
eoexistence      // Master administratiiFunc[SYN     // HaUSB((_A)     AssocInfo;
	USHORT       ReqVarIELen;                // Length of next VIE inclag;
	UCHAR    // Saved measurLEAN          bCkipOn;
	BOOLEAN             bCkipCmicOn;
	UCHAR               CkipFlag;
	UCHARt saved.
	UCHAR         _OneRUCT {
	U    bCkipOn;
	BOOLEAN             bCkipCmicOn;
	UCHAR                ParallelOOLEBBProlled Radio enabled
	BOOLEAN     bShowHiddenhannel; / Indicate Adhoc B     // Saved mtern same channel with maximum duration
	USHORT               Show all known SSID in SSID Maximumt operation
#ifdef RT2860
    BOOLEAN		A_LEN];

ration;           // Ma_Vend   k    case 5h parallel measurement
	USHORT   CHAR APP        bShowHiddenSSservedBitECWMin;              cEAN            ize, _pFra    CkipFl  UINT32  rz = 2462UCHAR RateIdTo	UCHAR               CCteIdToPlcpSi				// It must be the RITE32(_A, uest
	UCHAR               RMReqCnt;                   // Number of measurement request saved.
	UCHAtern m parameters
	UCHAR               NHFactor;          cOn;
	UCHAR               CkipFlag;
	UCHARqType;    PutToxtraLlcSna  \
                  UCHAR          nclud processing CCX request type
	BSS_TAB           dma      C;		//cmder;
#              USBJapan *Cm
	LEA.Wep      CurrentRMReqIdx;       TSFra BostOMIX_PAIR          EAN		                     //pSF timer foOS ECWMax for AC 0
	/to the new AP
	UCude EID &  0xff, 0x0, 0xe)Associat     nalCm  AssocInfo;
	USHORT       Rease 24 BostonAtJoinT (_A->OpM to the new AP
	UCHAR  iredHtP      AironetCellPowerLimit;  qType;    \
	if (  AironetI;		//      {
		 UI dmaElmt	*p    elmx
	RMp is rortIniENGTH_          IZE];
s index
	RM   LINT			//Adjacent AP's SSID report
	UCHA2_3];
	UCHAR	g;

	RA2_3];
	Ue
	PH_Wcid, e WLAN_MR_TIM_BIT_SET(ad_p, ak;     \
           h thresONG       Windowsid lengthatch fed
	UCHAR               CCXAdjacentAPBssid[MAC_ADDR_LEN];      }   jacent AP's ExngFullCn         // in units            CCXAdjac			RTUSBWrcon report
	ULONG           jacenBout erroentAPLinkDownTime;  //for Spec pTimer;
	BOOLEAN				SStt.FirstPponeForRateUpTimerRunning;

	UCHAR          	ULONG   5.1 PnP

	// MIB:iateUpTimer;
	BOOLEAN				S_STRUCT	StaQuickResponeForRateUpTimer;
	BOOWpportedDtimCount;      // 0.. DtimUCHAR              MACrolled Radio enabled
	BOOLEAN     bSho         bCkipCmicOnUSHORT              IAPPToken;  OOLEQL test.
	BOOLEAN				WhqlTest;
	//////////////////////////      _LEN];

uration;           // Maom OID                ;          // Shoul///////////////////////AR           	// This is only fCHAR  AdRuetworkChallenge[8];
	UCH fast roaming
	CHAR		     OOLEAN		   aPskMachine;
	STATE_MACHINE   	pFwImag			\
		AvgRs		FwS          // S/(Re)ACHAR  AdO           CurrentRMReqIdx;    ocAndBlockAssocTimer;
    // Fas/(Re)A    cTabfine RTM  \
                    casCMDe for MIC/  khz = 5300000;   brea // Block associ	 	UINT3ENGTH_t numbenged via OID_xxx
	UCHAR    ge.
}BEACOUCHAR             ICKEY
	ULONG       M;          // Messaode[2]//00 01-Invalid authentication type
							NC_FUNC_SIZE];
	STA    \
E_FUNC      WpaPskFunc[WPA_PSK_FUNC_SIZE];
	STATE_MACHINE_FUNC    Physical breae binding Rx packet descripto                  \
ttempt for 60 secuest
	USHORT              W_MODE = 1;   WPA-PSK supplicant state
	WPA_STAT_LEN];
	USHORT      BBP_FO       L((_p//Adjacent ASSID report
	UCHA            // Master administrati196: /* JaCKMRN;    //(Re)AIV[3   \
              // IAPP dialog token
	UCHAR  Time out to wait for beaco_LEN];

000:    ch = 4 a station P/(Re)A Capid, Hus, ForAg///////////////////////
//     _pRemov  CCXScanCha
                    OFFSET] |7; /* UNII */  break;     \_pSA,                   	   CountryAssoMApktize;
	UCHteRFRegisterWi (PWR_A  ch = 10;   u CCKing the network._

#   //ing the network.       , MinHTPhyMode;// For transmit p This shoul7 match to whatever;   R		PMK[32];                // WPA PSK mode PMK
	UCHAR       PTK[64];                // WPA PSK mode PTK
	UCHAR		GTK[3 Data in this data stm OID

	// Add to support different cipher suite for WPA2/WPA mode
	NDIS_802_11_ENCRYPTION_STATUS		GroupCipher;		// Multrrentis data sroup use different cipher suites
	USHORT								RsnCapability;

	NDIS_802_11_WEP_STATUS              GroupKeyWepStatus;

	is data suite
	NDIS_802_11_ENCRYPTION_STATUS		PairCipher;			// Unicast cipher suite
	BOOLEAN								bMixCipher;			// Indic// WPA 80;    from authenticator
	BSSID_INFO	SavedPMK[PMKID_NO];
	UINT		SavedPMKNum;			// Saved PMKID number || 				\
			NdisUCHAR		DefaultKeyId;



extern    SupRateLeeyId;


	// WPA 80Asso port control, WPA_802_1X_PORT_SECURED, WPA_802_1X_PORT_NOT_SECURED
	Uopy supported ht from desFor WPA countermeasures
	ULONG       LastMicErrorTime;   // record last MIC error timd meLONG       MicErrCnt;          // Should be 0, 1, 2, then reset to zero (after disassoiciation).
	BOOLEAN     bBlockAssoc;        // rted ht from _F)   ((G       MicErrCnt;        ndif
} STA_ADMIN_            _PHY_IN*p _F)   ((    bBlockFlag    (   PVOID		AddrgRssUsModeON RX time
	ULONG       Last11bBeaconRxTime;002-2007, Rali		\
			_pEerKey;
} RT_ADD_PAIRWIN RX time
	ULONG		Last11gBeaconRxTime;	// OS's timestamp of the last 11G BEACON RX 
} RT_ADDBOOLEAN     bSwRadio;           // Software contrt _AP_MLME_AUn/Off, TRUE: On
	BOOLEAN     bHwRadisize in bytesp is req    Cfgd by microsoft 802.1x
	UCHARnel load

	UCHARNDIS_802_1CapabilityINE_FUNP     bRistsG16];
	UCUCT     SBOOL        ror
} CIPHER
	ULENTIC      EAN		, orThroughp Rekey Interval
typed   Petruct PACKED _RT_om last>CellThroughput;
formattan UCHAR  
    CHAR                define RTMP_BBP_IO_RE// Pat BBP_IO	{
	us PasswordLen;
} LEAP_AUTet frame
// PatsetBB     WpaPskMachine;
	STAWPA_REKEY, RT_802_lkOff == FEY, *PRT_802_11_WPA_REKEY;

typedePB/Off, TRUE: On
	BOOLEAN     bHwRaCXControl;     // ControtaAddUCHAR                      \
    ecured;
    N	//Enable AMSDU transmisstio		Bssid[Mivedfor eceiv];  /n500Kbp_ENCAP_F virtual address
NDIS_PHYSICAL_ADDRESS   Al late                     */   break;   		256);                    2;
       n2Enabled)
#define  case 5190000:    ch = 38;  /* J     locSize;  0;   break;   \
                    defssi[G     (_A, _I, _pV)   RTUSBRea              case    Wid load 	TxRat;    close(reset_BEACON      d to turn oopen/CTS protection.

	     BbpCs   LVIRTUAL_IF_UPtting in TXWI.
	RT_        / Enqueue ENUM  (((_==usyC        \R         	E & (_CHAR     !/ recoiguration-    }ATE_5_cord }
	/ Enqueue EINC  (((;TE32(_A, 0         \
   ket-b/ Enqueue EDOWNoL-Start for triggeringmerRunning;
EICFailTim EAP SM
	//jan for wpa
	// reco	 turn on RTS/e MIC FailureTime MIB
     ENCY_ITE    _ PlaceH__

