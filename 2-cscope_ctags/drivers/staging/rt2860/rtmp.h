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
	BOOLEANinchu  ink TechnobBlockAssoc;****gy, I ac.
 iate attempt Ral 60 seconds after St., Jh ubei Ci occurred.o.36, Taiyuan-PSK supplica****tateechn Innk Tech	SNonce[32****publish//  Licenftwarder thU Generac.	G  *
 se as		 * iundati*ee S Free S from authenticator

	HTTRANSMIT_SETTING				HTPhyMode, Maxer version. in         ;
	RT_HT_PHY_INFO lat	DesiredHtPhyInfo; 2002-200     		bAutoTxRateSwitch;
	DESIRED_your option) anyl Pblisc       TransmitSetting*
 *stribut  td in th s hope .
} APCLI_STRUCT, *P* but WITHOU;

// -t evesefue end of APu even the i      that MERCHA

struct wificonf
{ it will  bShortGI FOR A PAR bGreenField;
};
thattypedef BILITYat it_PCI_CONFIGSS FPeneraam is dPURPOSE.  SCSRBaseAddresss ubli// PCI MMIOPURPOSE.  S*
, all access will use
}icy  *
 se Ral;* GNU G details.   c LUSB        morUINTPURPOSE.  SNU GeBulkInEpNU G;ther bulk-inimplpoint a   *
 *a copyied he i   *
 * YouOuthould [6];         outhat along with th       ave receiv*
 * //thou*****minipor * Fapte iral Puurenc.,   *
 * You shouRTMP_ADAPTERed aeVOIDNU GeOS_Cooki      **** specr F PURPOSE.  Srelative****OS
	PNET_DEV    net_dev;y,hat HNU GeVirtualIfCnt;

#if
 * RT2860
*
 *USHORT		 the GNU GenLnkCtrlBitMask*
 * *********************R*******Configurahis ************gram; if not, wr********

Offset**************************Hosgeneri

    Module Name:sion rtmp.hision Abstract:
ision Revisport gen.
 * 5-------    ----PCIePowerSaveLevel-----f the GNU GenbPCIclkOffeithiicensflag that ind    e im; if PICE p----if nous useion History:
 SPace..s Ta-           heckDmaBusyCibut    Min9-06 Iute rupt S      Regis    ang  ise NTC-------			ThisTbttNumToNextWakeUblic spectrum_SameRxByteifnde2are *onet.h"

#define VIRTUAL_IF_INC(__pAd) (__pAd) ->            ++)#define VIRTUAL_IF_IDEC(/
/* se NT---------ed parameters    When          F_NUM__pAd) ((__pAd)->VirtualIfCnt+)     _pAd)et.h"

#define VIRTUAL_IF_INC(__pAd) ((__pAd)->VirtualIfCnt++)
#define VIRTUAL_IF_DEC(___pre details.y of the GNU Gen            at it will et to the device
/////////////////////////s pro#definet_enable_reget t////////////dis/////m-----//////////////pReving "ai	PlaceDMABUF_NUM(__pAd) (TxBufSpace[NUM----TX_RING]ted USha    memoryiedet to1st pre-allo  crdessPacprogram isd* Freeeach TXD
	union	{
	ech Inc.	WireleRxDescRpe thn[4******Buffer ;
		HEADER_802_11 theRX descriptorsTSFrame;
	}field;
	UCHAR		ssggregatiX_		UCER_NORMSIZE] * i**** Aruct __Hon sTxe *
 TTTX_BUFFT ANTER{
	unR;

typedef st __HTAX_TXBULK{TSFramehould// AC0~4 + HCCA
#Revif

	NDIS_SPIN_LOCK[4];  //Buirq_his NU GUFFER structure forms ;
	UA////////he transmitted70irnet.h"

#define VIRTUAL_IF_INC(__pAd) ((__pAd)->VirtualIfCnt++)
#define VIRTUAL_IF_DEC(__pAd) ((__pUSB->VirtualIfCnt+--
#define VIRTUAL_IF_DUM(__pAd) ((__pAd)->VirtualIfCnt)

#ifdGene *
 70
edef struct __RP			pIrp;			//used to cancel pending bulk out.
									//Inithou****TXral Pubusb_ITNEig_et[MAX_TXB		*itialiNU GeneBILIT			 You should h          *
 * M  *
 * Free Sos profer;	//Inititoof        *ipeId;
	UCHAR			Seted U the LutSiuffer ze;
	NumberOfPipesipeI((__pAdize;Tech Inc.MaxPacketSizion HP_H__#defi;
	UCHAal one p pack is 
 * =anytiControl Flags
	    7, RalikOut;****    	PTTX_BUIoclude "TCRegT////)	UCHAEAN		PURPOSE.  S002-200			IRPP***
ngbUsbE	PslkR			11-130AN			lessPulkOutPipdata priorityPOLLProgrtim * Tr ThreadTSFrame;_TIMER_QUEUE		02_3[Q;	RTSFfCnt;
	}fi Geni/ atLLuct _ IRHeader_8emdLENGTH_80CmdQwaiti
md * H			Dataport g/ at dma     l L-09-EXUT ANT spinhis 9-06  / used toINT		Func_k    enerated writemlmever--   		Header_8Semaphores (ven t)in M     s_TXnse T    rps
;	e;
	UCHPaul     *sleep tata_d on	e use			pAdin MinipoialRTUSBCm			//InitiartI		Tranze
 bulB			pUrb;	ized in Minizeitialize
	PIRPracktialize
	PI	//Initiacomplery:
R			rackQC	PIRPng bulk out.
					pendingizedame;
		PSP	ed inAN			InUse;
	BnEXT, ized in Miwait_queue_head_t			*ze;	;S_FRAMEeive2_3];
 //"aBUFFER, *PHTTX_BUFFER;


// used to track driver-generated write irps
typedef struct _TX_CONTP			 *
 * oth_pAd/, Bosed Adlized in MinFER	TransferBuffer;	//Inp;			//used to cancel pend MiniportInitialize
	PIRP			pIrp;			//used to cancel pending bulk out.
									//Initialized is in one bulk-transmission
	UCHAR			BulkOutPipeId;
	BOOLEAN			IRPPending;
	BOOLEAN			LastOne;
d) ((__pTxD			pAd;		//Initialized in MiniportInitialize
	PURB			pUrb;			//Initialized in MiniportInitialize
	PIRP			pIrp;			//used to cancel pending bulk out.
									//Initialized in       7,ftwank TechnollDeQ IndRunnl	DataOff	PSPOLLulk-ou09-0or ensuring 
	PIR in tss for m get istri *
 NG			DataOffset;    ech Inurb dma    TTX_BUFFER{
	union sizket[Mrame;
PollDtibl>Virtual-----xt and ACSAtructued, 4bfferublic edon linux

}	TX_CONTch Inc./Initill bcensch Inc.//InitEXTr thecateson linux

}	TX_CONTMLMEcol.
//
tyde "sp, Boct _R//InT
{
;

HTE];
TEXT
{ut; acket	ext to canpAd;// Strun linux

}	TX_CONTIin.			//HT		//Init				pUrb;
	//Thestrucn't both be 1  at the same// 4l,  s1	NuAN		t;	//indexnd bbAX_TXBU for o* thee Fo		_

#ch Inc.Ireor[4eneratonlygrega	Pch InEDCA	    enerpipRP		enerated wrich Inc. ate
/r----------edma;prtotwarenow. 2002-200 latbRxHandlportReset	bWaiion size.
} ion Hgmze;
 rdware ll , so ze;	i latReadaReq Gene    outcture usebytesied resourless2 os				bRxHbacklog /misss
ndicsv_ave AER structure TxSwb dmarack	      ;
	dma_addrAR		
+ 1
	NG		TEXT, *PHT_TX_CONTEXT, **;
	}fie		at; if sameright TecLOy latX_CONTE at the sameFrame;
	}field;
	UCHAR		lete			Wireles4];  //Buffer X_BUFFER;

typedef stPMGMTket[MAX_TXBULK_SIZEOR_Vr save Aggregats
//
relemain Mi urbEXT, on linux
}ERSIONSPINRatTSFr50_MINIPORT#defPrio  __HTTt the samBUFFER, *PHTTX_BUFFER;


// used to track driver-generated write irps
typedef struct _TX_CONTEXT
{
	PRwhichfor makinow bRxHwriope  to;
#endon li_

#eralOutPosielese bacInd  *
tionerdware Ostart oort gied ase    td in ts    ar   SNAP_EAIRONET[];
extern  unsigned char   CipherSAR			SelfIdx----TSFrRor save Aggregati	RullF Occ0
#***
fzed in MiKM24[51char	C_MAJOine VPlaceTSFrama_adRxMINIPOR   5#defulk-ou	ture****k
	PURrRpnding;se   	R't both B];
exte_SIZ------- 1	PUCHredundm; imultiple;
	dnow.
gin.egatiblR			Bulproto				In*PPT     5
#etails the s
			Hield;
	U	T.
	BOOLEANrdware Rx     5
#ialiMaximifndo put Rx val70 /-------  Ciphn  unsignTechnare Fndling;RpackeInify t  SNAPI1ip[]cred
  odifynt kip., NoeT8[8ex R_DESChortmp.hWhator marollerrn 
 * HsBIT32[32tern  urcaten/ USB BI];
efdmRate[n xter* DR_Ler:
  [ SNAP_802_1H[drivi    n sed t&ern kip[]i Techenerated SToOfdmRate[extery:
 xterSuWm; itoor maaten
ext2 URBrdware 802_leDataOisnow.
in failedPOL_       AR CKIP_UCHAataOOL_Lbulk ferLengthAP_AIRONET[8]h In APPBC_SNALE_TALUCHAC1H[6];
ellcpSi1	Nuern pef  NDLeIdTo[2];
extern 12];HAR IPX[Lffer;	1H[6];
esed tpAR IPX[2]n a1a-1999 p.14
exg packet.
	BOOLEAN				inady fern  unsigned char  eld;
	UNET[];
eipeINTEXOOLEAN			IRPPending;UCHAR defaultCounOne;
ut packeASICD			pAd;		//Initialized in MiniportInitialize
	PURB			pUrb;			//Initialized i MiniportInitialize
	PIRP			pIrp;			//used to cancel pending bulk out.
									//Initialized inRP		32T[];
 // all ttxMACVersToRxLC_IS_MAJOMAC    nal[. R.O.C.
r    *C(0xern 0100) CISC_802_D UCHAR Phy11).lfIdx;         
 * MERCHANNateUpwaTnal[E2PROMult_c[];
extA_

#    Upthe HAR SNA     7, RalieIdToMbps[];Eeprom_802_  RssiSafexterSuAN		 0:ForTxRat,ps[];
1: reviSHORT RateC2~3: un. Waef  NDIS50_MINIPORTceivesaEERssiSy11ANeNumigned ch// 93c46=6;
ext66=8 usefuis ;
exterSuiteWpaNoneTkipDefaultVMAC_TTX_BerSuit_BBP_PARMSvingigned char   C end offset of a bulk-trAP_802
	BOO ait fackSHORT Rate    dToMbps[];

Firm: To ******HtCapIe;
0Kxtern U
eMinorAP_802_WpaNoneTiphMajtChanIe  Nexostruwisetern  uHORT RateIdeLevelForTxRate[];
externernBBP9-06  Wp  RssiSaf------For      HAR SNAP_BRUCHAR  CipherSuiteWpaNoBbpWriteLatch[140n  unPsn,ard[];
eANexte;
er600
*/
#_AesLAunsi;
exvia  RS_IO_WRITE/NFO_ELEMrn U_VY_REG_IDe;
ehanIe;

extern cx2e;
exteAR  ToDbmDeltwith BP_R66_TUNsave AggregatBbpTuuldnN*
 * iiSafeLevelForTxRate[];
exte  WpaIRFICHAR SNAPLINK_OUIHAR SNAP_BR  RssiSa----ConsEM[];
extern UCHAR  Ccx2RfIcTypTaiwaern UCHAFIC_xxxn UCHAR  HtCapIe;
extern URfFreq------ CHAR  ssiSuenc11	N----	PUCHAhannel itinge;

CCKM24[]FE_PAS-------    --h InRfReged USB// l RssithBGN2est RFeSwiPackmNIPOxtern Di    RF IC doesn't of ndat READ

	PURRateANTENNARANTY;;
	UCialinaUCHAR  CipherSuiteCipherSuiteWpaN1exteANeSwit signeOfdmSig;
diIdTo];
e thea & g. We nell boffseUSian;
exfutted USForABce.         NIC       21N1SHAR SNNdistriMo2RALINK_eSwitTODOZEROateSwit DTUNNsity mble11ismeSwi. Waiakingwhenin Mracken    iX	*nex_CON= DIV_MINTY ON
	SOFTrn UANT_	SeqBG[]LEM[];AfullaateSwitchTab           waiFProgSeq;
	UpwaNELl pePOWLock;	RTSFTx__pAd[/*
 TX_BUFFg_list
SA_OUI[]teWpaNtoret whes Tanxtern ;
extll		bAMSDUs.int 	qlen{
	BILITY fy tderCbAMSDUListu 
	int			ong 	ql, No};

11BGistP, Boxtern Uivg;
			mem
#endTite survey
	NDIS_SPI11JIN_LOCK			loc
	NDmpd11Jdering_list 11J	freelist;
};BILITY 802.11j		bAMSDU;erinbwssi0;enerated writ11BG*
 *LOCK			lock; RSSIing_l;
	UCounAR  1R			Las
;
	PerBuffer;	t 	_rece_SAMPLE {ived RSSI
	CHAR packet.
	bAMSDU          LOCK			lockRateSwitchT wait11N2ering_mtn2-2001	Nu	bAMSDU;-09-OCK			lock;]RssiMULTICASBbp9is packe GNU GenebpForCCKor driver Tx20MPwrCfgAB_OUI5heseBILITY 			Rsv_ENTRGLen;

_

#;
}   QUE4E_ENTRY,Y*PQUEUE_ENTRY;

// Q Ind     *PEUE_ENTssiSSuptCapIeR			LastAgcAUCHAR  CipherSuitRateRP			GEx2IeNEautoerinAgcEAR  N_SAMULONG *
 *rssiRefAPaul L			lock;
	ssiine 	MAXSEu_pool
as 25 ee serwith rn ULONG DLK_SIZQPlusBo UCHryAEUE_DDR_LEe Inbt  _QUEN];
eincrpherS_dmaeiveuM[];
mpensatenerated write\
{/Iniet to the device
/////////////////////dulkOutPipeId;\
	(t  _Qtruct )->truc =       ead;StepE		IrpLR    signex TSSI dSwitcrite i];
e/OLEAN      xtern
	NDIR QUEAgc		Bu      eHead			lock;
802_1t  _QUEUdmSi(ned cemov * (idx-1))vgRsEADER   structurtruc;G            \
	PQTailNT			* Hs          OOLEANTRY;

// UeHeadQue \
{ UEUGHeadQueue(QueueH    bCopySav                        ->Head = (QueueHea \
		(QueueH             = pNext;                \
		i                     \
}

#defi              =           \
}
pE_ENTd write iueue= AR			LastRssi2;       \
		if (}
#define V     LL)si0X\
	{I_SAM	)->Head->Next;      if (                  \    \
		(QueueHeader     t;          \
		(QueueHe                          //+++t unze in ,/ lasa-/InitiaeseSwiteCis      BG    ------1 ~>_

#
}

#def3Header, der)->Head; 0     5
#lock;
B/G rece#0 ------ueueHeaonherSuit 0x46h
	                  1\
}

           \)     1Entry)AvgRssi0ader)->Tail == NULL2                       2\omic          //;
exRateIe     \
		if (p	(              t  _Q\
			(->Ar)->Head;         sertTai     t  _QU(QueueHea \
}

         A      }

#def = NULL;    if ( QueuA         eEntry)    generated wr    \
		(Qu
		(QueueHeader      eEntry)                 eE                          OOLEAeueHeadLNAGaiRxwiM               teSwital LNA#                      struct  _QUEUE_   \
}

     \
	((PQader)->Tail = (PQUTaiwitchT36~64                \
	if ((Quel			bAggreg->Head = 1ueueHea    ch100~128                      		(QueueHeader)->Head = (2EUE_ENTRY,    32~165Axtern UCHAR  RateSwitchTable[];
exttraLEDr;
}   QUEEAN					bAMSDU;
};

strucHAR  IbsMCU_LEDCSRANTY;		LedCntgenen this TxCLed1eSwitsed t              3ct opeule Nas
//2rderin_FLAG(_M,ket.       ((_M)3>EAN		 |= (_F)40acros
//
tyLLEM[]1ANextStrepSigacros
//
ty    SingalstrlcpSiOff--  erdistribut.  SeLedOnScaar  Igned char CLUCHAeiveR FreFFER, *PHTTX_BUFFER;


// used to track driver-generated write irps
typedef struct _TX_CONTEXT
{
	P      D			pAd;		//Initialized in MiniportInitialize
	PURB			pUrb;			//Initialized MiniportInitialize
	PIRP			pIrp;			//used to cancel pending bulk out.
									//Initialized in pacutgoNIPOBEACON f    HAR 4
exerincorrespcharBOOta_dmTXWRRANTY;RateDownward[];BeaconTxW PUR_BUFFExe.
	ex(((_MBuf_cwmi[];
eT(_F)!= 0)
------[HW__Mad->_ *neCOUpedef{
//		TxRbuild PS-te;
SEAN	NULL 0)
#((ua-1999 k up.	PUCHdfficiGrn UpurpoitingPSSuitulk-outeDownward[]; srackF      otifpLogregati RTMP_NDIS_MNu
#define ELEM[];
extern UCpherSuiteCCX_PS|= (pLen;
exe OPST(EN];
exteed.OpStatusR_FLAG& _F)t both |= (_FpAd, ((( OPS_F))
pAd)->VCommonCfgLAG(_pAdRTS & (_F)) g packet.
	BOOLEAN				
Header_8===AP OPST_pEn==Statuy)->CliSTAefinClientSt/* Histo inCSToWu Xi-Kun 4/21/2006->PStern UCHAR  RateSwitchTable[];
e            mber++;     STAfers to   		Tran mcharie& otructdmSiOLEAN     (		/* co****ed tpAd->Op     == OPMODE_Smber++>Clien#defin
#definepAd, _F)   = ~sFlagsnsigned CLSTA_ADMIN       ssi2X8;
StaCf  NDIS50_MINIAG(_pA = NU     K_OU   spackFilCTIVE(_M, _FmmonCfgStaA      *    ished bvalitStatusFlagsADHOC_ON(er of|| INFRAStatus(; \
	elOOLEAN			IRPPending;nickn UCHIW_ESSID(_M)-exte+1Zl pe	e(QueueH)
#ding  Waiin    \iw_FLAG(_i/f  0
#enMEDI11N1AT _F)    (PreMedia    onCfusFlags |) != 0_FLAG(_pAd, & (_OP_pEne: eixt;
	warorNT_S	AvgRssi0X8;
	SHORT			AvgRF)) !=            \
}

//
11 RX_gsigned c((_pAdonCfPta_dmFraryThis Dis
//
dmmonHAR SNAPne etFiWEP_      //acketon2E////= 0)R_SEe,    ipheeSwiNdisd)
#defineDisConn 	_RSe    (_ |= (_FR_SET_xter char SE2OUI[]ry &nnward[]  end offset of a bulk-troLocalAdminMACigned char   C// UR  DonCfssiSgedBGNe	AvgRssi0X8;
	SHORT			AvgRPerman****y11ANe[Table1DRscoCA_OUI[ waiactgati(_p->StaGNexLONG			BulgAR  0efin	*******	AvgRsH[6];
)CKIP_)

#bCkipCmicOn 0)
TR IPX[KP_p     (     xUCHAR defaFILTER_SET_FLAG(_pAd, _F)    ((_pAd)->CommonCfg.Pamber++;           s |= (_FR_SET_,to b	bCuCfg.Wep->Sif

#)

#WeG(_pRMSI_INDEX(_idx, _RingSizead->N                             COMMOG(_M, _FmmonCfg.Flag)->StX_FI
ef stRANTY; onteSwitern  sl    ArackPexterns thost  _iRP		AvgRssi2;
	SHORT	 fe>Head->N				SAUXAR  defin} rece__pAd)uxILTER_CLEAR |= eueHoueHea_pAd)d)-#definda_d  0
#pIrf not,ma
strt 20SS_TABL _F)    ((_|=_pAdcanTab30900000))
#els90xxx    \       SCANPendult)->VMA     MacTab      staONG     ->Head = #0if

##n whIS     cted erinuP)->VAC0S_RT30RUE))

pAd)->V MACV_RTn UCHAR  ityToon-chip WCID enFI_OtRP		.  At TX,0
#dCValway9)
#d keis beorefineR			Swit       &) 0)
0pherSuiteCiscoCCKM24Len;
e_)				textLoc	BA)
#def(_pAATN				InUEXT, *PHT_TX_CONTEXT, ** _TxR      	R     ault_cw_RT3090RECBA					d)->VMAencr	PIRP	/    l[fdef KEYidx)  s
	CIPHER_ pac+;           save AKeyderinMB_NO_SNUM]];
eeWpaN% (_eCisceSwitc->Number-BSS0][0..3]

tructRX TxRassembly(_M)->PSUCHARra/////eueHeadFRAGMENT
#define , _F)  Frag      PF)  _pad->Next;          11AN=xD, TXD__TxD, TXusFlagxRing-various#ifndeeBUFF>PSFlATUS_CL3g, _is 0x    DS(_Tx8023                      l3tribute egme_IsSD0ad->N))
#elsmonCfWlar)->Tail = NULL;   \
	(QueueHe\
11 MIine     0) {_TxD{_TxXD_SIZEM[];
exteraSET_1 = receieueHase D->Lwith er)-teSwiprietis b
}

// Increase DR              Drsransmission
// TODO:
/st// WSC has d02_1(Dynamicent
#defNDIS_SPI
//
PRIVAT)				30lag) & 0x10)rivhar or saon & 0xffff0000SAM							 int of a  (__FSC has doRing-for s, see funion - CHAR    icrosoSwitchTabHtCapELEM[];
exter|= (bnitialize
	PURB			pUrb/ Whepresent
1H[6];
edevicRT30x			(n UCHAR CiscoCCUCHAR  HtCapIe;
extern UPS = pNext;                \
		i_mpdu_pAdtatusFlS_RTfor .bulk-ou		RCRSSI_SAMerTX s

ty[]e #eAeern U               UCH    {   TxRing->o put pdiscregati ion3(_pAd)ENTRY--         ber++; Did    aCfg.anymor
#deCHAR  HtCapIe;
extern ULinkDown02_3a>Tail)  ///////////////////////////) ||/ When TRY;


// StaActive.SupportedHtPhy.MCSS+++UentStatus theT(_Tsmiss        (_p->StaCfg.bTGnW
{
	stvgRsgeddefine RX_FIL
{
	stILITYst dntrytT(_Tine 
//Sine d)				30>Tail)    UCHAR  HtCapIe;
extern UextraNC		//TSC(_tscad->N_FLAG QueueEntry)      ;	pPaplayNIPOR_SET_FL
// StaActive.SupportedHSystem3CopyBitmapp)->StaCfgb0:(_p)   ForTxRat) Copy        ----reak    \
		(QueueHeader)->Numb \
	_pAd->MacIcdHtInfoIe;
exter5      TRUn UCseria_QUEUerf    issC_ADo-----redistver.D>Tail)    r+ UCHAR  TimIe;
extern UCHAR  WpaIaActiv \
	_pA here. siSafeLevelForTxRate[];
externipor000))_EV ful
#defineE    UE))ell[_idxr20 =HTC      d)->Vmin[4];
extern UCHAR default_cwmax[4];
extern UCHAR default_sta_aifsn[4];
extern UCHAR MapUHAR defauAP besticD			pAd;		//Initialized in MiniportInitialize
	PURB			pUrb;			//Initiali = _pA= 0x0)     KCHAR        US_TEST_F_1H[6n whern UCHAR pAd->MlmerName[     	lity.HtCapInf	xSTBC;      \
   Cdriver to ng foOu[];
eOneSe    fuor driver to abiliIn  \
  nfo.RxSTBC = NULL\
	_RT3->    {ve.Supporteded UTility.Active.Supporte +Rfg.bCkActive.SupportelmeAuxfo.Addawed RDogR TxTscKIP__pAd-e.Sup	PIR    OverFlowPhy.RecomWidth = hanOffMlTxrdware Cn/usedd to c.36,Thes
exterek Teut packet.
--------------UpdateBcnCntDo      er--dth = _pAd->MlMacDeadruct ptione S_ON(_Pso.Recinto ion xtCh    dteToRx     \
	(QueueHeader)->Number++;     DEBUG       rt_FLAG(ill have a cost down version w----------BanAllBaSet full_F)) nt[BSPromiscuo          \
	(QueueHeader)->Number++;         Doc em(_idxon-     //Initial                    \
}

//
//  Macee.Su2.rtsaccu[30E_ENTRY;

/cbdHtInad->Next;      feIT32               \b 0)
iali[1ivin _ptive.Sxi	intapability.H    acros
//
rcvba[6ion whility.MCSy.Nober++;  xtChanOffMForcOR_VntTXity.MCSSID_ 0xf].AMsduSRze
}

ult_c)(_DRP			psPlaceTEStruct    Size);	BOOLEAUSBent[BSSID_WCStaFifoTeslity.MCSHAR)(_Protec16);apInfo.       bilta_dInfo.MimoPs);	\
	GenOneta_dtent[BSSID_WCBroadComHTMCSSp->SFollowNIPOad)
#defTG(_pHtCapaextern UCHddHtInfRderinFactor);	er--;		BulkOutSiMACROT(_Tx32-bit     rO(_TxR00
*/r GTH_ / unsierb;
	CanceltOne;7eems no.HtCapnow?e : RTMP_IO_Re[12 of ster read /				e
//
/ge : RTMP_IO_R_port g,
/Fai(_M)ail)   tInfral Pubo    TNE
	UCIFIRatebelessalize
	PIock;
			0 \
	(eueHe Ad,
/blk			In002-2pHtCapOpeve.SupondnonBEoRateI       rd[];
enon BEToRateIs p// l3 of fer for saTMPiw_usinfo.Ad{           08) &ral PubUS_TEST_ceis fny	have O_MA           bttTicunsigndELEM[];
 *
 M    UPDIS_rsioo.RecomWidHaveMsi_pAd->MlmePhndpStatusFlME_IAN				cros
//
typis_:
  
#> IS_Rwitch_BASsmis(10	NulB/OS_HZ)etry befoRxSTaONE_SECOND        BBPregisilinaActiv// \
	iflg_be_adjuInfo.E_TUFreeo i_A, _V) _ANex_ightANext

tyN8/
typTM_FlgSusAX_T & (_F)) != 0)

#d\
	_pAd->MUseEfus;
exte    }             T AN                 ,   Cd to tAPPet of a y11ANextR59 TeortI _extRO_CntimplingNT_ed a           Plc#deffine RX_FI(_A)=PlcpSiUE))


#d08) MessageBHAR SNAP_B							   *        retur dritCapItCapPIPX[}      f
// StaActive.SupportedHtD.HtCnaio= TR((_p)->StaCfg.b        returSifsn[ {_TxD->LastSec1 =   	         TaF30900000))
#elTag(el      IE) - Adj      ASN_     G0, &Value.wordad->Nextnsmis//PlcpSi1	Nu \
  fiee - incluhy.Op4ps[];
dr_t	    >CommonterOUIINIT(fine MAXy.Mi0x00.OpStatu \9    \      ((_ryptioefivET_LAP   retur&& ((_p)->Sta //< MA       1	Nugenerat  *
        \
     ucsignn_F)     TxRingsid., NUCHAR ndirect_sidderin-------ine CF       \
    G  p		InUse;			// //EM[];
302-08-802_1      hataCf     \lle pack;UCHARSC has(_A)->-----
                       rindirect DELAYINTMASK		hile03fffbindirect _O_SE  \
 _I, _pVad->Next;    \nd-----------------cindirect Rx[];
Siz     ITHO52.pherlayedpdu	)->Numivi rxindirect rxied In2 of G WITHOf

exCsrEAD32(py of the GNU)->Head = (\
s
//ong >Head = (P02i, tCapInfo.ChannelWidth;      \
	_p\
C* MCSor (i=0; i2ITHOve.Stx       \
		(nsmissBine O  \
		(Queue                    
	_pAd->Stcu90xrderiPQUEUE_ENTPlaceImcuindirect PreTBTTfor (i=0; i<    MAJOR_e-Csr.>StaActry,indirect rsr    ldtructure 800      _A, RF_CSR_CFG0,ueueHeaP				Notifr (i=0; i<8 BUSefinvP                         \itchTacl/ recec       struct          \
}

// StaActive.Suppo\>HtCode=BC;     ontinuusyCnve.S fifUCHAR . Bct rTBC;>StaAc          *
 * You shouRX_BLK    )arm.XX_RXD 200andRxDif

RTEST&= ~(d->SR_F) FlaOPSTD0)    EA		pH n ChBBP_s == PACKET      for mST_FLAG(_M, *pied 
    {   	(((ied p->Stane RTMP_TESr     ul)    ->St.bCkR_VEult_anOffueEntalc * 1e     * (c)u     }  = 1; CULA         P_IO_ xt tec0 = 1RxBlk, _UX_T)		 k;     ->hy.Ope|=       TE32     UCHACS    FG,_ENT       d       \      \
{or (k=0&k</*
 BUSY_>PSFl; k++) 00))
                    \                  Ad)-EAD32()G0, __COUNTfRX_WDS              \
    \
;
	SH   \
	_pAd->S           fieTxTscO_MAue     4if (BbpCsr.fiHTER, e-key
     8if (BbpCsr.fiPAoston,i=0; i<MAX             \lP.n Ch 0)
IDLE_pEn    \
     Q          struct  _QUEUE_F)) ntinue;              Etry)-i<MAX    \
	_pAd->StES      _pEn            Y WARR(BbpCsr.            Dine OP&&                WP PURPO    pCsr.word)	data_     B_SUBLAG(_OPST	1ity.HtCapInfo.Ch*; \
	els>HtCapInf           d.  \
 EAD32(_A, \
RW_MODEFIELD	 2IO_READ8_BrX_UNKOWN
#defi                 \}MCber++;                   edef GACto it.
//   {                               ty.HtCapInfo.C                         ER{

// StaActive.SupportedHtPhnt
#de      nsmiss20Statu	re-key
lyfg.P_Cizeof(TP_IO_)OfdmR48ps[];stru            \
      B      sr.fieldQueIdx   break;   Tx MAX_					       HAR SNAP_802_bulk otsmWidt   *EE// las, Boston(V); 9ono betc         MAX_ USB MAX_    CkipC    SID_W(BusyPLEer owAR SNAPsd RS_I, _pV)    {}
// RNET[];
HtPhy.Ext}g//nsigned char UCHAIO_R segments e11B            {}RITEcateT; k
//       bme    ister's ID. G    IE    _I, _pV)   E_PARM_                 TMP_BBomic the PSTAT (_A for mP_ADigne00mmonCf     \d.BuMacE0000TailQued;
	UI:ToRateIdNG		_1MP_IO}RA 
    eSwifff    ) /btor Set l=ONG			But  *
 * This progr *p      \hRALINK_Ta);	\
	i       ted . Wai_ACTI02_1H[6acter       ->StaATUS_TESToRateI)) !                       F)    ->StpSrcBu
    do        R	\
		pNex     / ReadpAd)-k__M)-->atib               i++)    2(_A, RF_CSR_CFG0,P_IO_READ32(_A, BBP_,RT30xxine IS_RdeAX_TTxRi hanlHAR ZEAMSDesmax[4];
eSiz              _ENTRY)(QueueEnRateId[aylo->Ne2_1H[+;            Layer == _n Chan           )     lcSnapEncap
#defAEsignemeanace k;   a LLC/4
exeSwirn UBA
#)         (i=0; i<uf[80Buffedif /e sopIe;
S_MAJa     O +READWI +rtedHtPhy.MCSSet pH_80BOO+ (_pV)uSu              \
             Mpd                                       N            \		((PQ(QueueHeader)->NumrP              rd[];
CXTkip[]>Com                         apid.bCkipine RTN];StaActive.          	     _Tx                 Wc		// Receive \
 < MAX    0break;    Reg    = _IEAD32(_A, RF_CSBBP_AGENT, BbpCsr.wo// at le cla < /*
 BbpCsr.word);			     GEAD32(_A, Rw;   kipl1	NuIFSAGENT, BbpCsr.wor        fP_IO_Req registeve.SEAD8_BY/
/e PER];

e     is{      *(     _M, A     HEADE    ng_m: Obso		UCd? extern ine ISBP_AMCS    ersion whexterAlChanwiMi++;     COUNgrithm
	PEAD32(_A, PSFlK RING_                \B            {         ;
};

strunff) 	UFFER  BBP rYOU SHOULD i<MATOUCH IT!        Bbp	//Initiali Add. Wait fohard  Ad-)canc	e UCHySwitchgister_Of           = _->Tail-          hTable1-----it:
         \n",      \n",FG0, _.word);TX_bRtsR    }          SwitchTa4
extifet = NUA,direS
 * Yoteor    ->  \
 . N - Suib.\
	_pAd60/    Matructure forms\Ac       { P_NDIS_Mr.word);				((PQateIdt = NackPendponiportInDLE)     PiggyBBP_IOCHARty.HtCis packgac				ile (r--; iggbe      no         \
   \HTt micMP_NDIS_M      iapaate;wBP_ASuitHT r   *

// used to tHtCaNonQo                 \
{HtCaBP_AGe    fX_BUSY((NG		pCsrWMM-Y)chTa\
  e);                    r b== IDLE)     == _  \
        segm    M_BBP_AGENT AW_MOD\
  )S	nsmishen i==iace - )->NumBBP_A segment break;    BusM regof             *
 *   R%d=0x%xON  ble//_READer)->Tail = Ngned             \
WMMcontinue;     QOSkMP_I++;            ClearEAP     
// StaActive.Supportn", ASSIGN        T  \
              ) \
		do {teDownwaode =      _I, _pV)    {}
// Re  \
		(Qu        ;            break;eueHeader)->Head = (umber--;       ++) 
   , &B    V)}AR SN(0)T          \
   	      AD32(_A, H2M            er--;         GeneraW                uct  _QUEUE_Ek++) 
    WITg.Packet    \
		(Qu       R=        R? 1 :   \  \
		RTMP_IO_READ3Cnt<MAX_BUSY// Read                                ted U
ail)                     \
             eratEADY_COUNT; k++)                \
        {break;      iteCiscoCCK           & apabili NI  Busy     WME_IpCsr.    BbpCsr \pAd;
Nn UCHAR ,   RateUpi- Suite      R_CFG_STRU     hTEST_dx)e putve.Sup       HAR  
};

stru.;   __inlect      Bo   \
ICY)      _pEntry,\
    I== _I
                ad->NexMern UCHAR 32         _    ++) ern l)n UCHAR  0: 0)
#dl>Nextd.UCHARW_defiMP_IO_  PBFccessENber++;  	_MAJO0x41    )atiblRssiSAd . SW     ux.HhICAST    WriteLREADX_BUSY_COU	AvgRssi// used to t {_TRUPT 0)

#P);
}         break;    _BUSY_sy == +)                    \
 _QUEUE_ENT        /     \
    "fOP02_11US__mpd" On        >MACVSY_COUSavePa, _PARM         \
     d		lockoecomWidthRecomWidLME, wCkipCfg.benalb {            \
FlagRing-CMACVi    				== IDLEUptructuern UCHAT2661 =>c0 = 1;ld.RegNp;			    \
exte UCHAR  recto regi					erin  RssiS     teCisco          	\
}

#define InsTMP_IO_READ32(_A, H2M_BBP_AGENCnt=0; BusyCnt<!OPUCHAR ccess      if (firn UCHAR _I] =))
	{
Packd.Busy = 1;        te BBP registerY_CORP			pIrp;			/ /*ME_PARM_    *P_AGENsr.wo1://Initi	
}

/_PAR    \DBGPRINT(     BUG    CE, (tern UCHAR       !\n")08) &&A     \
    Csr.field.RegNum = e;			// U  B     Info.    B8_BY          \
    BA, H2M_BBP_AGENT, BbpCsr.word);			   \
         \
   ;    MP_H__
M        IP2MAC(.Reg _BUFFERpIould ,f 0)
FALSE) *de "spectrum_dl;
	U16    -o				                   teSwi)
ISCOtur RALI              \
}
    pAd_READ32(_A, H2M_BBP_sr.wor     B       \
    }               ETH_P_IPV6    BAMPLEset(_READ32(_A, 0            *OF)->StESS      *MP_IO_READ3) =_F))3             \
    	 _I, _p                      ead R%d=0x%x[12  \
              \
            con30;  EAD32(_A, RF_CSR_ struct  _Qcon4    \
        BbpCsr.5ord = 0;     UE_EN		ty.H_I, _pV)    {}
// R    Be CKIPEUE_ENTRY)QuTMP_IO_READ32(_A, BH2M. GenAGENT &BbpCsr.word); field.Bu     \01   \
        BbpCsr.==    Y                           0x5.MCS   \
    BBP_CSR_CFG_STRU_V;   ]-key
7usy sr.word);     \
  \
         \ 0;     \
    BBP_CSR_CFG_S
        if (\
       	} What
     }pAd->Mlme---------oCCKc.             rout            _    .O.C.)			DatCHAR       DBGc= (UCerThis f      ieldstDLE) &&,
	OU     sr.word);    *pp       

    i0xff, 0x0elay(      Txxtern Mregat   if               \   \
      usecDr;  (       Ofdm//InitialHo     if
   sr.word);   _VEAD32(          the           
extern UCHAR                     break;ad  OfdR//////ty.HtC                        IO_W;y ==      == HANDefine IS_RTWrappeueHeaHistory:
t both       LEM[];
extern      NIC				    xxRFGN_WIF/
tBusy    l\n", _I, BbpCs)
#define MAXyr_OfraionM_V)    _ OfderSuit    ++)  \
%x fa               \
        DBGPRINT_sr.wormac           ueueT_ERR(AsicFrom	if ((Qr.word);				\
		BbpCsr.field.Busy = 0;  \
           A, _I, loggned                           _I, _pV)    {}
// R
		RingEm                     \
        \
}
#endifR_FLAG=     \&			Readasr.wor
}

// StaActive.Supports>Flags = 0)
#ader)->Number--SuiteC moPsfor 3	_ in MiniporherSuiteCiscoCr.field.BB     BBP    Lx%xefine\d);	    bpCsr.word););, _V)    _E    ef RT; i++)                    \
           ON      nude "spectrum_d          RTUSBWrite      retur&& ((ON  t<MAX_       .bCkCfg
		Rr's ID. GeneraM[];
                AD    etPRegiCopyporttern BBP2600
*/r          VmmonCfgErase RssiSAd_I, _pV)   RTUSBReadBBPReg QueueEntry)     oad, _I, _pV)   		sr.field.Busy = 0;       
}

// StaActive                  u0_I, _pV)   RTUSBReadBBPReg
 end offNIC_ID(_ForHang8_BY_REG_ID(_A, _I, _pV)   		RTUSBReadStaAcail\n"B    \
        Bbp case 1:     khz = 2412000;   breRawead = pNext;                \
		if (k=0; k<M     otAllZe UCHAR .fields    1      khz = 2417se 3:     khz = b_BBP_A      
ext  RTMP_    V

#ie wrCHAR   case 3    Bb            				AR			ty.HtCapInfG_ID(_A, _I,   \
                 \
      case 44     khz = 24Mo->Mlty.HtCadefin   \
        if (Bbpcase 45    Bkhz = 243200break;     \
    AtoH(		RTMP	*sy.HtC_MODE *deak;  BY_R    blen    B++)       7    Bbc4:     kk;    
    MacBbp                                           Y;

;
	U(_A, H2M_BBP_AGENT, BbpCsr.word);				\
  
                            \hz = 2437000;   10:    khz = 24                 _A->OpMn,     BY_REG_RF2600
        (         epeaiportse 9    BSe     \5            1     Bk;     \57                    khz = 2472000;   b(_A, IDLE)           bpCs00;   b	   \
               	           NTRY)(QueueEntCapInf    khz =case 36        case 13:   6:     khz = 2437 2437000defi end offset of a bulk-tr\
    {  failase 4 khz = 2tLED_I, _pV)   RTUSBRead	RTUSBWrit_MODE = 1     /        ;   b00Signalreak;     \
                       == OffstaAueHe Dbm/  khz = 5200sy == RxTx_I, _pV)   RTUSBRea  khz = 2472000;  870 //5220      \
8(_A, H2      efineM>MACV    RTbpCsr.field.BusBP_AGENT, BbpCsr.word_11ExterHINE *Se 56:     UNII RTMPk;  _FUN       []         dx)  DDBA 5GN_W case 1:     khz = 2412000280000 /*>MACV      n UC *Ele break;   _RT3DEL /*case 64:  UNII */3 khz  UNII */  khz = 5180000;   break;     \
e 6eak;*/  kLS/  khz = 5320000;   break;     \
                    case 149: /* UNII */  In9000T/  khz = 5320000;   break;     \
                    case 149: /* UNII */  QOII */745reak;     \
                    case 149: /* 153: */  khz = 5320PeerAddBAReq/  khz =, _pV)   RTUSBReadBBP       /  khz = 5320000; 80   \
              sBWriteBBPRegister(_A,I */  65z = 5805000;   b8250002   \
             DelNII */  kh       case 100: /* HiperLAN2 */  khz = 5500000;   break;     */  khz = 5320000;   break;     \
                    case 149: /* UNII SendPSM case 10:    khz = I */  I */  khz = 530: /*  112: /* HiperLANPsmp   \
        R         sw            cas4z = 5HiperLAN2 */  khz = 5/  khzreak;     \ is distribut            case 116: /* HiperLAN2 */  khz = 5580000;   break;Hpy of the GNU Gen   case 116: /* HiperLAN2 */  khz = 5580000;   break;                  case 161: /* UNII */  61HiperLAN2 */  khz = 55;   break;z = 51800_    o           : /* UmWidthStructur1k;     \
: /* U          			//: /* HiperLAN2 /* UNII */  322ak;     \
                   3         ORI   break;     \
                    khz = 2422    RefreshBAR  khz = 5180000;   breacase 1C  Bb, k;       \
    {         cES_ON(AX_BUSY_COUNT; BusyCnz = 2437000;      \
definRW_MODE = 1; RpHdr80228: /* Hiper   khz = 517000038II */ Japan          scaz = 519000N2 */  khzBar 3NII */ JapLAN2 */  khz =     000;   .HtCapInfo._BE    couBne RTMP_BBP_IO_pBbpWrit	_pAd->StaA         (Queue       \
                    case     case 					\
	++)            Hs      L     \        Category49580000;   hz = 5230000;   b\
  \
EnignedForRecv               case 149: /* * Japan */N2 */    \
	_pAd->St            reak;   _REQ pMsD, TXD_SIZ      TMP_ak; ) !=anToMbcu&Bbpak;    end offE) &&     Rxfo   \
  eak;   \
             : /* 2    Bb1900RTMP_khz = 498Nhar	CDmaeak;     \
    pan MMAC */   khz = 5170se 46:  /* JOURC     rer osts J08 Ad->Ml/  khz = 5200, mean_MINIPOR/* Japan, m504  khz = 5 5040000;   break;  k;   \
      6  kh  \
		( 5040000;   break;   \
              means J16 */      \
    5080000;   break;   \
              voBY_REJ16pan, m     Y;

 5040000;   brI */      khz = 2424120    7, meanII */                     pan MMAC */   khz =  end off  breakIsSI
	C &BbpC  case 1:     khz = 2412000;   breafine BBP_IO_WRIprd);9000GNex4:  /* Japan MMAC */   khord =       \
 reak;     \Oak;     \
ase 2412000:    ch = 1;     break;     \
          	AvgRssbrePeak;     \
      khz = 242port if {      Sniff2
#insPRegze);      (khz, ch     		UCct reopFirst                          RIpy of th------ase 2412000:    ch = 1;     0  cfo.R4        c/  khz = 511   break;     \
 TA      n lin(khz   \
        DBGPRINT_E2437000:    ch = 6;     xter000;   brek;     \
   Csr.wordP_IO_READ32(_A,                 56 */  EAD32(_     B     6;  p       Arraeak;     \
             t packet.              khz =if

#ifdef  N\
                    case 24370INhecking busbInt  khz = 518000_MODE = 100000;   br	_pAd->Stax_Tx              
#define Insert_A,f == FAL   \
                 12z = 5+)  \
    {   ine Bi<0           1\
        cOUT	51900*         XDLefiportInsigned chComm;    b	AvgRs    case 24320              c \
                         break;    khz = 51800RxEAPO /* Jap[8];
exkhz = 519000GNexk;   \
         \
                  eak;  ; k++)         Bb/  khz = 5180PRegW2_1H       break;     \
 = 5;     Xostones;   break;   \
              43700              caer(_A, _   br      00;      \
             {       X_BUFFER structur    02-200eak; 4;  /* 6    NII *3;    break;     \  khz          4    II */  khz =ability.HtCa /* UNII */     c   case 13:    khz = 2000;  GN_W/* UNII _MINIPORfo.R52eak;     \
       \
                    case 149: /* 528k;     \
         c  case 5300000:    ch = 60s J08            case 5300000:    ch = 60;  /* UNII */  break;     \
                     \
 II */ lcDstory:
;     ;  /* UNII */  break;     \
        case 243        1:     k;     \720p->S/  khz = 5200     )->P /* UNII */  3      case 5320	\
	_ BbpCsr.retuP     \
           	for 3  break;    hz = 5: CFACKh = 161; /* UNII */ InT2862stamp     Bb300000;   break; 500000        cNII */           \Seq,LE) &&W ne     \
     ./  khz = 5180BAp->S* UNII */  b0;usec0000;   break;     \
                 		
                T;    bre4;case 38       \N2 */     casope);   khz = 5180000;Cfy.HtCapInfo                           BUSY_COUNT; BuNII */  b_    \
0:    ch = 36;  /*/  khz = 5    ca 5180000;   break;             NII */  5180000;   break;     \
Cach
             II */   /* HiperLAN2 */ 00:    ch = 60;  /* UNII */  break;    5             se 52:gr;				UF  br57  breh = 153; /* UNII */  breakHANNEL_ID(khz, ch)  {    WIV              QSEL/* UNII */  kh92: /* Jd = 1;  nsmissiUSBWriteBBPRegister(_A, _I, _V)
#d     esum      B    br28  break;                 case 5600000:    ca(_A, H2M        k;     ;  /* UNII */  break;                     case_BUSY_COUak;     \
   program; if not,ase 4:     kkhz = ;    pStatus(           case 5765000:    ch = 153; /* UNII */  khz = 5180000;   breakQos* Hi = 14;    brreak/apablPacketion ch = 132; / 5180000;   break;     \
eak;   \RTS ch = 132; /* HiperLAN2 */  break; :  /* Japan MMAC */   k000: ;
ext  \
Cot            ch = 13*/  break;    1/   k    case= 153; /* UNII */  TSk;     \
    HAR  RateSwitch   \
   /* UNII */  break;     4I */  khz = 53*/  break \
                 \
  c           _BBP  brea
                    case 24defifine BBP_IO_WRIT*  khz = 5180000;          y e.Sup\
                    case 2437000EAD32(_A, RF_CSpWpaKN2 */  khz	WpaMic  \
urek;    */   break;   \
                    case 52        case 149: /* UNII    bre        A;    his nograak;   \
                   13         cherLAN2 */  khz =      \
       /  khz = 5180000;   break;     se 506066: /* HiperLAN2 */ secDelay(1000);Cloneze);                    case 149: /* * UNn.  Do		Bunffses     &               ca280000:  Incase 5230defin             c*pp Har             case 53200);					at           cas 5180000;   break;    dWpaNo_BUSY_COUNT; BusyC*ch = 12;    b
                  case 5040000:    ch = 208; /      \
    BBk;     1
           \
                        rect2(_A, BBP_CSR_CF ch =) != 0 fran", _ 	qle      /igne-  Id     *l];

extescfreer        cas08: /* Japan,  \
    ECUR/* Japa               case   khz = 5170fine BBP_IO_WRIT.  khz = 5108: /* Japan\
   JHCd.BBP_               case 5620000:   R%d=0x%x fai       casLEAN		Ele", _s;
  EBBP_R[    served_COU       SCAT9000GAT    n UCENT PHYS_BUF[NIdHtPh   break;   \
             wep2 */  khz =    ch =WepEngi}   
	CH  \
                    case 2:     #ifndef  49800      case 4920000: KeyI   case 5320000:    ch =y == IDLE) 80;
	CHAR			AvgRss_Res  khz = 51khz = _

#NHipeak;  (_a) < (_b)) ? el <=:14) ? rSuiteCiscoCne wrm    case 2      GUSY_NA_GAI                 case 4920007I */  khz =\

#deSoftD>TailNWEP       case 5600000:    ch = 1    \
              A, H2M_BBTMP    cnA                   pGroumaxBP_AUNII */  khz ICV     case 216: /* Jap          8

typedef struct _R
#defin_RT30ARCFOUT, Bcase ch = 1Ad, _FerSuite C = 5230000: ((_pAd->LatchRax2000:    800:    ch = 56in) : 000;   break;Ad, _F)BYTak;  aCfg.BssType)(RW_MO     TE32(_A,          ODECRYP             \
e) == BSSpAd-\
   ec0 = 1, fOPne R \
  k;     \
: IS_RT3O:
/tchSwitcs.ErpI0>BLNACCKM flags
#defin								DLE_OEN              \
  (!I!)->Co        && !Ad, _F)  _p))r of 
     L
   & Mlme   20        e                       \
        WPA);         LeapAuthrsiodx)    if (aCfg.Lea    mmonCfg.Pa.LeapMlme                  \THER_LIST {
    ->StaCfg.LeapAuthInfo.CCKMsion 
//  SALC_FCS = 1 ch =sion isFc                C /* U
     flags
#defin000:   endi         con,       /R    1) %      

#ifdoeSwitS_RT3sicA\
{      \
	      case 506  \
            }0	+ *(          >Ht60000:  khz = 5040000;   break	->StaCfg.BO   \
           f (== _            ch = = 3reak;  JaY)     BGcSnapEn
		(Q.WepEqualMemoNonGFExiNommonCfg.Pa2(_A, _V)      \hz = 5170000; \
             1II *:    craLlcSnap  breaEqualMemo
#en}        \
	ntextra  continue;   case 100: /* HiperLATUS_(_      \)usy 	else				Rf RssiSExe   \
{                 6 */  :LL;					}e INFRA_ON(     \
r new Tx PRITENew D      Check ase 5080000:    ch = 216; khz 
		(Q;			Then                     case 5765000:    ch = 1P_KHZ___Placehis      ludntinue;     {y == LL;		) > 1500)		\
	{															\
	e				_pTE32\
      \
    }  100: /* HipBC;      \
	_pAPRTMP_SCALIS	gBulkO_A, _I, _pLEM[];
extern U       HiperLANPRegp)   
       							\
	etBssi  bre   BbpCsr.field.Value = _V;    
//  p
extB											\
	etM    Weak;    ue;  =      regatHRA_LLCSNA		 \
 oft de
	}								\
	else				_pE      con     		\
		_pExtra    400Dy lat  \
	}						;								\
	}								;     2_3HeadQueHOC_O_pMac1{      2G_STRyp We wi->StaAn									\
}

// ng indireURITEnsigned MAKE_80Bs                  MP_BBP_IO_WRITE8_BY_REG_ID(_.WepIbove))		ry \
{         g.bCkipC   \         00000EdcaPar              case 116: /* HipP	Nul{
	PQER_LIN){     _p +oveMemory((_Slo     c2, MAC_ADDR_LEN);          \
out.
	BeUseICULA* 2ory((		\
		_pExtraAdry(IPX,  ch = 1ap = NULL;								\
	}												ar maki= Nstern U, *PRTMP_SCALIST {
<< 8) ****ffCnt           \BbpCsr.wo           \
  _STATUS_ADHOC_O        ((((_ppTxMients[NICovr   Ci      RP fi		\
		_pExtra     eC1042 nor Bridge tu Rat),char  iT, *at way	pAd_REG_Iated
  ndatioed IPX or APPLETALK,-eue; 		\
		_pExtra       0xfAttribu\
     pMac1, _pMac	\
	}								-------  khz = 51z = 519ithin its payload
// Note:
;
		_SCAThe resY++) 4 WITHO Paire = NKey\
  n_pnux
   (nux

//  may IVEeak;    m th8-ps[];       \)INT_[];
ester RITE32(_CHAR  HtCapIutCapInfo.Cha    \
		(QEIVVERT_TO_802_3(_p802Rxmay e CONd (rtch f_pData, _DataSize, _pRemovedLLCSNAP)  om the result E ch =NAP (nrtInir RFL only, which did not turn on Netbios RemovedLLCSNAP)   _patch fdLLC4
ex: alonger		/     				PX            pBbpCsrS_ADHOC_OefiniMP_A         monCfg.Pac   continueNDIS_SPIN					_AuthMo       ch f{       ly, which did not turn on Netbios                     \          LLCSNAP)              II *_TEST_FLAt IPX o butPgs
# fnux
, 6) _pAdTO_KHZ(ch, khz)  {     
	}move 8-byte L	withivedLLCSNP_IOAMode = _pAd->MlacTabory((n    BbpCcase which did not turn on Netbios but use IPX whz = 2427 frame is LLC/SNAPTok mea  khz  break;     Ar   ch = 4;     b         ak;     \
  CiscoCa +0000:   khz = 52600          LLRA_LLCS LLC/             \
         A, _I, _pV)   MAP_KHZRandom					RIDONG     , 2))xtra         nelWidth;      \
	_pAd->StgtM       bre cas/* rd);				\
		BbpCsr.fiUNII */ def s0;   break;   \
          ((A + 12,      \
        BbpToD_ENCAP               bre    \
emove 8-byte LLp)  Radioeak; > 1500)		\
	{															\
56 + MAC_ADDR           case 116:            TX_COi }  iportWr307)
#defi*Ta           ULL;      \
    0;   break;     \
                             \
        Search           bpCsr.word[1    tCapInfo.Aatt way.
// elinue;  =AN2 */  khz = 55                                       \
             ch =         ((ALNAG     A + 12, 2))     \
    r, _pDA, _pSA, pPro       _pRemovedLLCSNAP WithpAd) (_b)) ? (_a) : (_b))
#endif

#ifndef m    Ndis           \
    hdr{   D    pSA, pity-o02_3_HEADER(_            CF.,         De
	BOOly, which = 1; P    }     ALNAGain0) : (                  \
	     \
                          T_TO+== 56	data_dma2_1;                  pBufVA +A_ORI         Bail\n", _0000:  519MoveMemoMacTa \
                    defadefin        casB            HAR             + MAC_ADD]e / 2_802_3_H                  om th               PSFLAGPerio        CF.WepMoCf                 AtimWi           \
	s);	\
	
      \
    {       p242200_READ32(_A, H2M_B\
               ExTOR)
#define     Leze % 25         HT_CAPAral Pu_IE *pHt          \
    AD    }        _DA;	\
pBufVA,
extemigh- SuiteAP = _M     al h    fo IEextern      	ataSize -= L               ch        _pD= 124; /* HiperNew				uct  case 52       {        // used to tn        LARGE, BbpG}   = 2S61; /* UpType, LE ((QN);               Mo      (STRUACHARQ fail\n",   Nd /* JapanaSize -= LENGTH_PQ    LO     }    boveMeM     (_p802     \
 VI           c   case VARI   \
IEs pVeak;                     ly, which      case REG_ID(_A, _I, _V)                                   \
            MAKE_80                                                                         \
         n[0                                  P_BBP_IO_WRITE8_BY_REG_ID(_A, _MLME nee                 (_p802    802_3_HEADER(_p8023hdr)
#endif

#ifndef        _DataSize -= LENGTH_802_1_H;     MLME nemation from 802.2870
#define RTMP_RF_IO_WRITE3                             \
            MAK--;        SWITCH_AB(X_BU1_H; BBWe will have a cost down version w                                  \
            MAKE_802, BospCdHtPhy.E                 \
    RTMP_IO_READ32(_pAd, TSF_TIMER_DW1,pCC	if (BBE_TUNNEL;			\
		}														\
	a) : (_b))
#endif

#ifSF_To.RecoA   BbpCsr.field.Value = _V;   k        49              case 216: /* Jed to p           9-------                          S    ExtCBusyC(UCHAR) + 6;               BAWinn fragmentTUSBWrOrigind)->Co               nd offIsRecip           \
         sidS      PRINT_ERR(("BREG_ID(_A, _I, _V)          Ouhy.SHtCapInfo.ChannelW                                       ortB                break; SF=0, Lowze RT2((_p;    br\PaeyinIDGE_T1_H00:       ca2;     break;     \
 DGE_TUNInd    \
                              sion HlmeE_ON)ro which ecvonCfg.O    , High3_H; 	Dat     51900*(_p, _pMac1, _pMac2, _pType)        break;case 192: /* Japan */   k9      rameSizSA,      *se 24470R)=0;  2,_	DataSiz* Japan,   Rate<eak; (_pAdKM flB      r)->Tail = NULL;   \
	(QueueHeadHilcSnapEcen 4 Ant      \Loe the LLC/SNA     to);            // informatio      khz =to collect each anki0, _Rsigh32TS802_3_HE   cT;

/_ord)PHYExtraL ant2TSF=pF0, (UCHAR)_Rssi1,                         = 20          n,E(_pAd, crteak;     \
             \
    else        08 colle Low32TSF,Emptrrently0,        =0;  1,ssi[UsedAnt] Low32TSF, \
 move1pFlagsi[UsedAnt]\
		_pExtra    Aak;  /    k;         case 5210000:    NdiJ12 */6: /* Japan, m       fVA* RTUSBWritsi	if ))
#(AvgRs000;   break;     \
       )    AN2 */  khz = 5HiperLAN2 */  b
        else     AneringX_ANTStN         494000/
	}					     \
            Def 1	{				kac1, _p
		R																		_ONd->RxAnt.												Sekhz 0000:    = AvgRssi;						28Mac1, _pMac2    								 \
   
			Avgo.RecomWiRxAF_pAd->RxAnt.Pair1AvgRPert of\
			AvgRssi                           = AvgRssi -(							 <            case 149: /* UNII DroualMemo    B _rs) << 8) 	_pEArriv            case 149: /* UNII    \
												\
			AvgR                            = 							\
		_pExtr} UNII */  khz = 5180000;   break;     \R             case LCSNAP_ENCAP_FROM_PKT_    caseTx    h.      \
 EX       4
ext      FROM_PKT_OFFSETtArrivedpAd-
        {       )	\WhenE    ate++ 2)) &&  \
            NdisLLCSNAP_Ensigned TSFraQUERYTX_BUFF(_.WepBufpAd-p      Buf    TO_KHZ(ch, khz)  {               QueryBuffer(_NdisBuf, _ppVA, _pBufLen)

#define MAC_ADDR_EQUAL(pAddr1,pAddr2)           RTMPEqualMemory((PVOID)(pAddr1)on from 802.11 header
#ifdef RT286TMP_IO_READ32( 149: /* U0.Weptructure
//
#define NIC_MAX_PHYS_BUF_break;   \
                   UNII */  TMP_Wha, 6)  |elect eErpInel
    (AR  Rat)<< 8AR  Rate=AvgR(_pAdT2860
#define6(_pAd        l == 60) || (channel == 64))

#ifdef RT2860
#define STA_PORT_SECURED(_pA   catChan_pFrame, (UCHARecurebpCsWPA      X_DIS_
//
UREDr.fi      xt t= ~(_FonCfg.OfT2860
#dS_CAN_GO_SLEEPsr.fiVA + AcquireSpERAGck(&RT30xx(_ IS_RTStaC.PortSSSID_WCIS_Rns J08 *BSSID_WCID].PortSecured = _pAd->StaCfg.PortSecured; \
	NdisReleaseSpOID)  ((dro LLMtSecured; \
	NdisReTUSBWrited in MiniportIigh32TSF,TAnLock(&(_pAd)->AL.WepQueryBu->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED; \
	NdisAcquireSpinLock(channeQueryBud; \
	NdisReleaseSpinLoc= (UCHAR)(UCHAR)(_pHtCPort); \
	NdishanOffstaCCls3err->StaCfg.PortSecured = WPA_802_1X_PORT_  ((channel == 52) || (chanBetweenWe    ER(_p8023hdr, _pDA{															\
 576   \					W{         sReleaseSpinLock(&_pAd->MacTabLock); \
}
#endif

//
// Registeffff    _PARPAIR(channe   Value;
} ;0;
	CHAR			AvgRss      ;
} }     RssiSmory(SNAP)   HAR   Value;
}          \
RxAnt.PaPrimaryd->Rx;	fdef RT2860
#define STA_PORT_SECURE    }         _V)        \
           veMemory((_p + MAC_ADDR \e 494000AvgRs/Nelags            ister(\
        BbpCsr.fie    if (sFlags khz = 5040000;   break;   \
      fault_cw\
                    case 560UENCY_ITEM, *T ANRPostPrAIead !PREG_PAIR;
b;
	//ory(SNAPN			bAggreg
           2
// information from 802.1ssi[UsedAnt];2  bre;     \
            \0
#define REPORT_MGMT__FRAME_TO_MLME(_pAd, Wcid, _pFrame, _FrameS             cid, High32TSF, Low3           <MAX_taSize -= LENGTH_ault_cw ((channel == 52) || (l)        \
{     
{             casethMac1, _pMac2, _ppVA, _pBufLen)

#\
	}		nEvalua \
  }apan, khz =s						Pair1AvgRssi[UsedAn802_11_SEQ{
.RcvPkr muQueryBuffer(_NdisBuf, _ppVA, _pBufLen)

#define MAC_ADDR_EQUAL(pAddr1,pAddr2)           RTMPEqualMemory((PVOID)(pAddr1), (PVdx)   th Registe    }
#endif

//
AcquireSpinLock(&(_pAd)->MacTabLock); \
	_pAd->MacTRtux.HpAtSeq2 = 0guous phys} RTM_802_1RITE3BkInODMA    /icenseCPU    , *PRX_CONPULONG		R	pAd;
	Ped4         s physical memory
//  Both DMA to / from Cecured = _pAd->StaC    CONTE ->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED; \
	NdisAcquireSpinLocCls2t paiiteWpa)				alzreless//       KM[]pAd)				((ocVa;    Buffere					,_PAC == 60) || (channel == 64))

#ifdef RT2860
#define STA_PORT_SECURED(_pAter set definiti,    \
 r;  nel == 64))

#ifdef RT2860
#define STA_PORT_SECUREusFlags |EnORDERBUFer mu   ValRDERBUF,ontiguous Aggregati_RspTMP_DMABUF;


typedef	union	_HEADER_802_11_SEQ{
disEqBILITY or i \
            case 5600000:    ch = 120; /*						SizetSecured; \
      case 104: /* HiperLAN2 */  khz = 5520000;   break;  PRTMiPIRPRspGen           _dma;.
} TX_BUFEAN		)       (, \
           _ta type
// infossi[UsedA/SNAP; N	IsFull; 5220SFraX_BUFF iTMP_
}
#endif / pairs4NII */  ragment list structure -  Ilsak;       TMP_REORDERBUF; *PRTMP_REORDERBUF;

//
        y to scriptor) for all ring descriptor DMA operation											\
		}							Frag:4;
	USHORT			Sequence:12;
    } = 0)
)ueryBuffer(_NdisBuf, _ppVA, _pBufLen)

#define MAC_ADDR_EQUAL(pAddr1,pAddr2)           RTMPEqualMemory((PVOID)(pAddr1), (PV
exteueryBuffer(_NdisBuf, _ppVA, _pBufLen)

#define MAC_ADDR_EQUAL(pAddr1,pAddr2)           RTMPEqualMemory((PVOID)(pAddr1), (PVOHAR   Value;
} 
#en *
 ***RTSFraPHYS)->FCkipCES    AllocPaEAD32(_A, RF_
	CHsPac _RTMP_REOa_adess
#J  }   John fR			LastRssi2;   nc.
 m isdEAN		pherSuite      \TMP_DMABDMACB *PRTMP_RE)_PlR, *PREG_PAIR;

//
// Register set pair for initialzation regisrameSizProb       > 3) + _rssi1;				ne SWITCH_AB( _pAA
#enchar  I(
	 -= LENGTH_802_1_H;     tx packet descriptor
// which driver shoul         All  \
    \
    EG_ID(_A, _I, _pV)  Ad->RxAnt.Pair1AvgR\
		 _RTMP_DMACB
{
	ULONG                   AlRTMP_REO\
		Pair1AvgR - 					\
		>> 3alua				\1r laye_Datals0:  ef	union	_HEADER_8{Adriveno    si Fre		Length; || AACB,tJapauffer must be contiguous physical mCHAR 		DataOffset;
	USHORT 		DarucOid
#en);	\
NORMSI}   HAR       Cell[x;
	UINnsig_p)   cop32				CpuIdefin free txsidmaIdx;
	UINT32		TxSwFreeI  dx; 	// software next free  tx index
} RTMP_TX_Rreak; M*PRTMP_Rx;
	UINR;

//
// Register> 1500RMP_TX_RING;

typedef structINT32TA_POoa    	UINT32		RxCpuIdx;
	UINT32		RxDmaIdx;
	INT32		RxSwReadIdx; 	// software  {           *PRTMP_TX_RING;

typedef struct _RTMP_RX_RING
{
	RTMPtx index
} RTMP_TTMP_    dx; 	OR_V software next free tx index
} RTMP_TX_RITxDmaRTMP_MGMT_RING, SwFr(channex; // software next free tx index
} RTMP_MGMT_RING, *PRTMP_MGMT_RING;

//
/;
	ULx; // software next free tx index
} RTMP_MGMT_RING, *PRTMP_MGMT_RING;

//
/              Gooded in ths               xErrR_11Encr               Tx302,
Ethernet StNDIS_QUEto, 2)EAP)
.Pair1PrimaryRffer;

	// Ethernet Stats
	ULONG      , _FiHtCa St., Jh    Next;
	              xDmaI>PSFlAggrega3}   //*
 *      Y_REG_ID(_A, _I, _pV)   		RTUSr;                 1ust b                                   break;     \
  se IPX wi, _qPRegAP      2		Tt    eO(_p)    khz = 5040000;   break;   \
      ed in thted	Datgn", _ONG       RcvAliTEST_FLA/   khze    armFiult_cw];
eult_cwKxfffFREQUENc.
 {   )   ine JOIN            *    R auccessC            VERT_TO_802 COURindiCt., ;
	     _INTE         Su      unt;
	LAASSOCER   FrameDup		pAd;#Enqu) &&  \
              	IsFull;
	PVOID               X_ANTENNAou    (Va    eak;  
*/nIntv             eCount;
	LARGE_INTEGER   FrameDuplicateC;
	LARGE_I    ER   FrameDupol blLengthC  \
        nt;   // both success  Framecs
/t;
	LARGE2_3;

typedef exteRGE_INTEn", ypedef st, *PCOUNTER_802_11;

typedef struct _COUNTER_RALI24[]TEGER   FrameDece
{nt[BSSID_ansmittLARGE_INTEGER   FrameMxterouldto_pAd->RxAnt.Pt1T ANct _COUNTER_11dx;
	UINT32		RxDmaIct _COUN     TARTgmentCount;
     lnt;
	LARGE_I
#in      tructulkInOsicateCfve Aggregati_DmaBu                
	CHAin its AutoRecoveUSBWritAUTH          Reci; // #unt;      // both CRC okay and CRAlbreak;ture    e IS	LARGE_IN
	LARGE_INTEGER   RTSS DMA operrameSizPSFLAGS                 case 5600000:InUlue;mingCoFail\
	N>StaCfg.PortSecured = WPA_802_1X_PORT_SECURED; \
	NdisAcquireSpinLocG   Ributed lFcsErr      //                ending;.Wepe pack      /OF_TX_RING];
	ULONure -  I>StaCfg.PortSecured = WPA_802_1X_PORT_SECURED; \
	NdisAcquireSpinLoc    // AssoUINT32          OneSecTxDoneCount;
	ULONG           OneSecRxCount;
	Uloc
// 						Artipl      // TxBuf virtual address
	UCHAR			Header802_TabLock); \
	_pAd->MacTPSFLAG07000 nfo.	DataDums oftRingFulled in MiniportIF_TX_RING];
	ULONpInfo.     BadCQIAutoRec		pInfo. both successful and failure, used to calculate TX throughput
#endif // RT2ous physical memory
//  Both DMA to / from CPU use the same structuRING];
	UINT32          OneSecTxDon[NUM_OF_TX_RING];
	ULONpInfo.Rxt[NUM_OF_free		\
						\
	{												           \
	_pAd->Stak;    Okglobal counte _rssi1;			pInfo.ndex
} RTMCX        Aak;     \Pair1PrimaryRxAnt;							\
		AvgRssi    OneSecRSani.
//
typ02_11;

typedef struct _COUt's, *PQ               ect each     case ,_FrameSizedefi 4940ecRxntCou32          Oate TX throughpuxNoRTX through PVOID       MgmtRinck vibu    CountoveryCtructuCRC e02,
	UINT32          OneSecR (((_MSnsmibal coun AssociateMs      ueueForRecv(_pAd, Wcid,  break;     \
      NecT USBe TxTsc \
 32          -= Lyt error +OneSecR SNAHAR MaghpuoFCS302,\
    {         TMP_ING, Agg3G		TxAggRecoveryC _pAA, _MPDUCount;
	ULONG		T scatter gaUsedAn;    g1MPDIPX orbe PVOID LLCar   SNAPTxAgtCapInfo.ANG		TxAgg9MPDME(_pAd, Wcid, _pNG		TxAgg9MPEr	TxAgg9MONG		TxADr_80UF, *UM_OF_TX_R		TxAgg1t[NUM_OF_TX_R		TxAgg5B     &High3DUCount;
	ULer layeroMCount;
	U Register seAR   ;
	ULONG2MPDUne REPORT_MGDUCount;
	TO_MLME(_pAdNG		TxAg1;

	si1;					\
		else	ULONG	     &High3        ed iABUFFERdx; nt;
	Uimk;   */  b          veM         (_pnount;
	 TSF_TIMER_DW0, &Low32TSF);            \
    {           _PlcpSignal) ITE32(n */*ppV) = I;

	t[NUM_OF_xAggCount;
	ULONG		TxNoARGE_I;

	sInAPreNransmittedMP_INTEGER  ze, _Rssi0, _Rssi1, _Rssi2, _PlcpSINTEGER      2 High32TSF, Lo= 1; l)        \
{            ctet    
	SHO          g13MPDUCount;
	ULO ((channel == 52) || (chan     \
    MlmeEnqueueForRecv(t;
	LAR									\
	e2        5320000;   break;     \
                    c(UCHAR)o collect eachCount;
	ULONG		No            TxAggroRec
		else		Count
	ULONG		uct _COUNTER_uctut + OneSecTindit;
	ULONG		TxAggTxruct _Ce, _FrameSize, _R	\
		else		} PID_COUNTER, *PPID_COUNTER;

typedef sM_OF_T     case 506ef struct _COUNDRSust  errothin Bbphe imaAR  RatvedFrag2          LastOneSecRxOkDataCnt;  // OneSecRxOkDataCnt
	ULONG		Duplic32  LARGE_INTnAMPDUCouCount;
	ULOransmittedMPDunt;
	ULOR   FrLONG                         /}->PSFinitialz   swixCounord)STEP****x;
	ATE_, Low3next / # of seconONG		rTxRatP    ty    \
 but      #MP_Iso_BUSY_AN  har   CipP_FROM_PKT_Surern Uo globaetryOkCount +emory(APAX_STEP_OF_TX_RATE_SWITCH];
	UCHAR           TxRateUpPenalty;      // extast unstable conditiunt;
I    R   FailedCounttSecPRTMr \
  TX     ine INC_RIN   \
}

NoisyEnvBUFF", _fine INC_RIN   \
}

cs
/SecAcH];
ingrece	LARGent;
	ErpI LastSecTxRateCsreCouDHOC for iINTLME
	UIggDU;
	CouChlgTdefine Oction
	ULONG		\
	_pAdstTxt error
	Time; EP_OF_Tulate RXULONQua    [urrent  Arcfour Structure Added by PaulWu
/oEGER   NG		TxAg0 last unstable conditio;
	ULONcopy of the GONG		9;

	G		TxAgMPunt;
	ULONG		TxAg6ransmittedMGE_INTEGER   FrameCount;
	LARGE_IOedef struct _GE_INTEGER     khz =  v      B, *PRTMPuf;             /  khz = e
} RTMP_DMACB, *PRTMPHAR   khz)  {               \
         {            Count;+ 12rs;
no last unstab     r
//) anyer musCEI2 High32TSF, Lhared key data strPID BeenDis { /* UNII */  bxAggCount;
	ULONG		TxNunt;
	ULONG	smittedA];
	UCHAR   Count_WRITE32OURCONTEXT, *PARCFOURCONTEXT;

//EP_OF_Tontiguous Arcf*
 *ThesNG		ReAdd_FILy _HEAWu
/1_SEQ	     TXWI cRxOkDa, *PR is invalidaSize(_b))
ramet*******KEY****  LastOner     extewe 
 * YS_BU 4ne Rs,5660 bitsTUS_A} ine KEYT ANAR   Rxt
	ULONG           B      O:1;
 } RECHAR   Key[16];            // right now we implement 4 keys, 128 bits max
	UCHAR   RxMic[8]; time value of LastSecTxRnsmitteddAMPDUCount;G24670Bdress
} RBbpCsr.HAR Bount;
	LARGENGust btInfoIength 
	UINT32          		TxAg4          able     ;          able con		TxAg3Key length for eaONG		TxAgh key_A, H2M_s, 128   Key[MAX_Lrors;
	ULONG       OneCol Ndimplt  _  break;   case NETWORK_TYPE NetworktryOInRT_TO  fLaned chalue = _V;     acket = NPRINT_ERR((AR SN   fNoisyEnR, *PPID_COUNTER;

typedef struct rent TX rate
	BOOLEAN         fNoisyEnvironm  \
 EAN     EnFalseCca     U    Bounader// 100n */ you    fNoisyEnviUCHAR / Register RS {
	//ER       case X wiO= ~(_FS     ResC      
	,_FramPRTMP    fNoisyEnvironm... DMA operaLfveMemory(_p, _pMac1, MAC_PRTMP_   case 5							k;    ntCount    ) y// Register sex;
	IF_R             

type       \
 Fa 0xf                      case 5620000:  case 5170000:  (UCH2TSF, LowBusyCnti	//ImentCount;
			\
	}																		\
}

#define NDIS_QUERY_BUFFER(_NdisBuf, _ppVA, _pBufLen)                    \
    rate's eStableCnt;
#endif
	UCHAR     Pair1PrimaryRxAnt;     // 0:Ant-E1, 1:Ant-E2
	UCHAR     Pair1SecondaryRxAnt;   // 0:Ant-E1,e RX_FI ueryBuffSt
//
:  /* Japan MMAC */                       mTUNINGP_TX						\0]:E1,r2AvgRssi1]:];
e            E_TUNNEL;			\
		}														\
	}	_INTV5000C_TX_TSCAvgRE (UCHAR)J= LENGTH_802_1_HX_RING];
	ULONGoRTMP_LNAGain2)))d    R4xffff  EvaluatGTEXT;  FirstPktA // ow3Bound    }fg.PortSecurOOLE tLock;002-20UCHATUNpuIdx;
	UINT32		RxDmaI seqEvaluate++;GE_RSSI(_pAdatedec reak;   \
       Arcfour Strreak;PktArriv 4:AES, 5:CKIP;	\
		RTCATTER_GATHER_ELEMEN>TEGER   2)se 2417000:    ch = 2;   air14StaCf    casre Low3lure, used to (_A,* UNII c //Ture: Enable LEAP        har    *
char rcfour StructureMlme // 0-none,                 \
        \
                 \next; \
            y(SNEGER   ACKFtNumWhenEvaluas, 128QuTUNING_OLEAN   FirstPktArrivedWhenEvaluaSequenceR;

//
/ortSePsm_    _NFO {
	BOOLEAN         
typedeeryBuff          UCHAR	STRSetPs4, 2:WEPlist st2437000:    ch = 6;     brsunt[NUM_OF_TX_RIN//Prea             rve[2];
	UUF
{
	ULO2       recHAR                   da    	//0d->Ml  khz = 5180000;   break;     \
_INTtabl//56        //           \
  e										f struct {pinLocU      5G, *PREC           eAd->MlxffffOH	\
		RT    \rivedOGULeap    \      case ];
} ROGUEAP_											\S_BUF_COU PasogueAp     \
R   Frame both successful an023hx(_a, _bRogueApNr;             fNoisyEnvixtern;
	*tsInAMPDUCdAMPDUCount;S_BUF_COULL;						 = NULL;								\
	}														CRITEtruct _LEAP_AUTma
ext
_pAd- (c) H_802	Dat

#deortSeH
	{															\
		_pExtraiANG, *PRE       case 24r 40 MHz
	USHORT	uf;             // AsE;

e
} RTMP_DMACB, *HORT		rsv:3     iptoStaQu           repTxRateStableCnt;
#endif
	UCHAR     Pair1PrimaryRxAnt;     // 0:Ant-E1, 1:Ant-E2
	UCHAR     Pair1SecondaryRxAnt;   // 0:Ant-E1, 1IBRATEROGU    re:       orgin aCfgN(_p)      {       ax  \
    	{										              AG(_M, is ri1, _rsented
} FRAG// infoented
} FRAG        Erfine RTM*/		RTMPu;     \
                   ch = A
// SF=0;  \
   xter \
}

#de(&_pAd->Me pack802_3;

 {           LONG		TI// Associated DMA buffer structue.
	BOOLby, IRTMP_DMACB, *PRTMP	Pf;          p     OneSe;          // Numb (_per of BuffeCHARPSDr1AvgRssi[2];  G		DuSuiteCi   fNoisyEnstruct  _RTMP_RF_R LastOne0:Ant-;   1Buffer2 Self explained
	Sou caS_BUFFER    irstBuf								aited BeenE_SEd->Mlmex3090xxxxRssi0;
	CHAR			AvgRss    EvIST, *PRT    UserName[256]			\
		else			cont<= de= xedTxPXcketeys, 128 R_DESCRCkeys, & {
	Bcalcu                     ucreak; NG    Fader)   Txe              case 4    _t      cas	 in M for MICKEY
	ULONG    CKate
POWER_itializ ((QKeyortSe structure which RC4 key & UNII */  break;    address
} RTMP_TXBUF, *PRTMP_T 		RTUSBR  brea= _I)eak;     \
                   \
  R  brealF_TX_RING];
	R // 0-none,d
	Udify****tatchar Chanize
igned  khz = 5040000;   break;   \
      B_M, ys, 128 and    {      UserName[256    th for   caNT32          OneSecRRRCON   // 0-nonerROGUataCnt;  // OneSecRxOkDatpedef structccessfRcv	UCHE    {
	BOO
} RTMruct To. WE HAVE     which did not turn on Netbios but use IPX w// MIMO. WE HAVE      153; /* UN                16FirstPktArrivedWhenEvaluate = TRVAC_AD))------KIP128
	U16;
i[0]:E1,    \
HY    xOkDa St., e 44: debug (_pAd)e,   \
t    Count;
	ualIirHE;
E_ENTRY. Taiy  \
      NAP- + 12{         \le  case tkiatchRin;
#ify[MAX_Lrd[25ki Delta;
	BS_RT <= 14) ? (_pAd->BLNAGain) : ((_pAd->LatchRTS_ADHOC_ON)       Delta;
	B Rx;
} > (_ine RUCer of G    12, 2) |typedef strud.BusMICS_ADHOC_ONdef struct _BBP_R6SC16: /*ect each an(_pAd, _V16rENGTsholund;   Identit 10R        Erure:TefinTKM   c_INFO   Rx;
} PRIVATE_STRUC, *PPRIVATE_STRUC;

// struS_ADHOC_ON
typedef struct _R= 42;\
         ((((_p)->SER;
    ne BBP R66 (BBP BBP_AGENT, B} SOFT_vate / Misc datab      F*PC         // cOn 2       IC           ir2 Ant and rss2 is reprotON(_p)                e LEAPpair2 A6t and rss2werBoundre    kUp, 
			R   owerBouR66_TUNING, *PBBP_R66_T	UCHS:AES, 5:Ks R66       *
 UCHApAuthMode) == CISCO_AuthModey[MAX_L, *PCCK_Tre];
	RemaiTX p  Ch	ULONG           B_list case 5280000:    ch =valid authentication typRegNum          \
    else   ived R    RcvPkweANNELABLEIdx; _BBP_R6signed creord *PCHANK, L    (>BbpWr 60;  /* UNII */  break;     def struct _LLerTh_list
       // 6:CKIP128
	U    mag.Weg * TForLARGtFraunit:  // specified in   Remaring_lChannel;
	UCegisATE_ {axTxPwegis  fNoisyEnvDfsRderi}    NNEcOn ApAX_TTHOUust bn with CC         ccOn          case 4920000: uC            D8_B
	CHAR			AvgRuct WITHOUX_RATE_e RXRC4Kune ch		R6        ((((_p)->StaCfg.LeapAuthMode) == CI(_p)       CntlMe;
	ST;

}Get 51ACHINE           AssocMachinUNING;

// struc5660 and rsP_BBP_IO_WRITE8_BY_REG_ID(_A, _A + 12, 							NC BeenDis6Vadef TxAgg2MPDVTUNING_ST\r2;
	UCHAR      MaAP theU     k;   e    0thRspMacTBOOL/  khz =  Arag;
}WpaPsk AuthRspMacFunc[ASSOC_FUNC Key lengea    ACHINE_FUNC      AuE_FUNC      Aut        RC32;
	 ADHOCRt the s HAR t the sam{
	CHkipcmm TxMic */  ksecDelay(1000);WPA      y ==dware Omuse.
/rd = 0ruct  _RTMP_REORDAR       Power2;
	USHORT     AuthRsFUNC     Ally == IMICAR SN}       Rx*    **PPT         // CALIBSt_AGEnt    TxAckRequiredCo\
InPutSP,           DUCouTUS_TEST_FL     AuthRsode[      ey datr;
} SOFT_RX_ANT_DIVERSITY, *PSOFphye);         ER        \
TIegiste25000:    ch = 165; 2tH purpose           Mcs Indiect;/
//ARGE_INTEGER   Frameis in DUIn     	} TUPLE_CACHET ANUPLE_CACHE;

//
// 

#de RFC104be a    e          Japan MMAC */   khz = 5r bre   \,_FrameexterSnapEnsk              Head       cd LLC/SNAP; N    \
          U;
	LARGE_INTto Da:AES,((_pAd)HE;

RIVATen covfINE      SI
	e;

	ULRIVATY {
	U/   khz = 50octlG    teS00) ||0000:    ch = 112;  khUC  BbpE_MACdifromern q	*wrq * TegisAR        Err                       			//00 03-Cha_STR    RnkU r;
#ifdd in My[MAX_LaderL_11J_TX_llocVa;       sr.word);     \
          \
Ant];						\d      // 0-nLE];
} ROGUEAP_TITY;

typctM      _Data* Ta
      UsSetH       
	NDIS_SPIN_LOCK        Boston,, 2) |    stru        RetryOkCount + O    vidualc     dicRrBounneCoe fai070(_pAl peNTRYCHANNELTIMErivedeak;   \pedef ste MAX_, *Peorderrame;
	}fie GNU GeneralBP re MAX \
     gocMacusR_GATHtSystemUpTime(    ct reorruct Key lengFRAGM_TEST_FLAG(_p, fOP_:  wpThreshold;  // dRA_Odex;
	RTMP_REORD((_pAd)->EAPUCount;
	UL.Busytor DMA operation
	STAT layer when the tx is physically done oR(_p8023hdr, _ buffer must b ActFunc[_RTMP_DMACB
{
	ULONG                   AlWp     Afine \
	NdisReleaseSpinLock(&_pAd->MacTabLock); \
}
#endif

//
// Registen,ORDERt.
/g1tFraRadar Detrom on.WepSfault_cwmRDDurory(  unHAR		BBPRd6;
	UCHARddule NaRT 		ount3CHAR		BBPR16;
	UCHAR		BBPR17;
	UCHAR		BBPR18;
	UCHAR		BBPR21;
	UCHAR		BBPR2;
#ifefault_cwmID(_16/ at leastFastD7s;
	UINT8		ChMo->St	UINT8		ChM21	LongPulseRadarvalid authenticationfer must rcfour Strucer;
	CHAR 		DBvWait uppRemovrx return the pacled;       cipdefi_Uwene;
	STAse 46:  /* JFunc[ASSOch = 											\ me22;
	UCH	UINT8		C        defa		\
DGE_TUNNEL;	00:    ch Def         case 5													\ meC_B}fieANT8		ChM6 data sor_NONECKp))

/now 
	CHAR		enum     USED,
    Origin
XT  WEIMER_STRUKeyegs.Ch, *PREC_BLOCKACK_STATUS;

typedef enu!ADHOC_ON(_p))

/S_AC4 keyoAntennaCheble c Note:
//  EXT  WE          \
  b                   AuthRspo MiniporS     Remainin
typedef struct _CHANNEL_TX_POWER {
	USdAMPDUCountiPacket = NU_INFO, *PPACKTtPhy.ndif

#define GET_LNA_GAIN4 keyr of MPDCKIP128	 * TOutVWin

	UIision           \
 s4wayicRou     ErrorCUHC  BSHA1DAR_DETECTINE  *          ca)       bul_ls    nd bfineur  *k_Ad, _F) sion is key_BAND,
}     	LongPudigON))
#definef strF    \ator_WaitReer.
TID;
	)       0
#de

	UIATE_7.3.1.14.p_STRvgRssi = _rs M; KIP128SHORT		LastIndSeq;
	:    ch = 5utValue;ATim_BAND,
}     case 2*outpR   FrameutValue;f second RTMP_IMlmep    Fof T   / RTa
	BO eld.Buor 
	SH Tec*******cs
/IndrderintInd#ifdele cond    RxReRiAtck;            nDropof Bufferle condrcvrderinRECW	rcvSeqPTK           case 5765000:    ch = 153; /* *PM;     \
tor_WaitRAn Miner must bock
	PVu Channel;or 17Lo*in Minrcipient_sy = umATimeg spinlock
	PVOID	pAdapteror_U    t reordering_BLGenAKE_S, *PCCK_ = SNAP_BRIDGE_TUNNEL;			\
		}			*m         UCount;
	cliOOLE_STR radar detection and c/EGER  TATE_MACHINE  TEGER  t., JhKACK_STATCSeAutoAcancel   Remais      pAutoA (b(((_M St., )PR17;
	UCHA      stFraHAR		BBPR21;
	ATTER_GATHER_ELEMtrR   Key[MsgCHAR		BBPR16;
	UCHAR		BBPR17;
	UCHAR		BBPR18;
	UCHAR		BBPR21;
	UCHAR		BBTEGER  RY, *PRCHAR		BBPR16;
	UCHAR		BBPR17;
	UCHAR		BBPR18;
	UCHAR		BBPR21;
	UCHAR		BBraLlcSnapEncaBaportederyCoun     ze[TID]&(1<<TID)),s[];
ex       Delta;
	     ureyBA// TkONTEReH;
exSize[TID] for BufferSize
	UCHAR   rsv;
	UCHAR      
// 
	UCHAor_USED,
    OrigPSFLAG

//FoTID]					uffer 
// A_BAND,
} rs // SATE_ {
Y;

//For QureyBATableOID use;HAR   MAC7ACHIN] for BufferSize
	UCHAR   rsv;
	UCHARsed cha* Fre{[AIRdTH_8TID}exist    se FIO_RufSBitmap&(1<<TID)), this session with{MACAddrMACAddr[MAC_ADDR_LEN];
	UCHAR   BUSreyBATA_RssiED _OID_BA,ize[TI OrigA_, _F) ruct  PACK2870 //ORI_ENTRY{
	UCHAR   MACAddTEGER  Final	 Originator_Done
}   ORI_BA_Status[8]_STATID    ;or_WaitRuct _QUERYBA_TABLE{
	OID_BA_ORI_ENTR_STROID_BA_ OriogueApEnt2];
	UCHAR   Oriator_Wai;       ToOfdmE];
	BA_REC_ENTRY       BARecEntry[MAhz = 5300000;   breakn its payl  case 10:    khz ure tveMemory((_pUCHAR     Cre	//0ry
	UCHAR  PRegTxAgg2MPDUld.BBP_RW_MODE = 1            \
    in_TUNINif ToPX or nRalink     rite0CHAR			AvgRented
} FRAGto);               02-200unc[
	UL_ion
//PCONtection and cCHARTRY       BAR;
	PIRP	Validuan (_A)->BbppaPskMachine;
	STATE_MACHin M for MICKEY
	ULONG           \
}

//      LeapMach/ exne;
	STAT       eak;   \
           case 5620       S_GTKE_FUNUNWRurrentWp  REC_BA_Status;;            plaStaA_QUERY_ *PMLMEcinSize;	// 7.3.1.14.       savSecBeaconSentCak      K_STATUS  ORI_BA_Status[8];
} OID_)
#define MONITR_STR_MACHD   htnt;
}.stPk// exW/ Register            ypedefNr;led      def A:  /*IPX oe BAapA-        t    							ime()
	U and/or            case 116: /* Hipe:BA-iftReg     AR        Erfsetif can -- mserve globaical    _LEN];
ch period (be== NINIME_IHAR   nsmitteENTRY, reshold; lo    TEGER ned cUC, *PBACOS\
		Ror radar d	ULOr WEP encryption / d10 or BNIT(_Tx    // las casrd int   AllocPaT                k;       RxR    Readaf	strNumexisy AN		u ca.odic)
typedef	strucct _OT_STRUC	{erOpti      Trn Ua	UCHAimeOutNum[MAX_LMo****BA_sOri_TIMER_ATE_iporrn  uFree SHORT   [0]fault_cwma0: /* HNumramemeOutNum[MAX_LDelABLE];	// compare with threshold[1]
	ULONGRead = 0;         II */  khz = meOutNum[MAX_LR   case 3:     0:    ch = 36;  /           
	BOOLEAN			bR            100
;
use				ag;
   D      acke  break;     \
 os_ate;
	me/s prm.  (DFFER s         P       STRUCSIZE] 124; /* HiperLt packet.bR      AtfreeABLE;
rcfour StructurebNowAth  *
 GS;

tmerLAN2 */       _P_ENC
//save Aimer;
	ULONLEAN         bNowAthee28, 3:TKerLAN2 *		\
	}							   cT_STrosBu \
  ,_FrameSize, _pF_TRANSMDLE) &&       = 1;   CHARsiiefres 2) | 124; /* HiperLA          \
bpCs= IDLE)              \   \
 1	\
		}  Origin  ch = 16cationwe impl    case 2optionhecki)

#def option)                 ry_RINndComman\uct	_IOTCHAR   t UINT32g. HAR   in a0RfRQUERYBA_TA adv    ualIge.or_WaitReFrame      your option) anyEXT ffer must 	nfor32   rsv0:10ne O_TRANSMITTxBF:1ABLE;

  2_DESIRED_inforT32   caE	PsPdefa  ca_SETTING {EXT, CHA:TRANSMIT_SETTING {IT_SE:13ABLE;
}1_SEQ			STRANSMITase 2_STRUord;
} REG_TRANSMITer must your option) anyator_WaitReNT32   _        ;
} REG_TRANSMIT_S(_M)TIMEDE:1;
         UINT32  EXTCHA:2;
   v:13;
    } field;
 UINT32   word;
} REG_TRANSMIT_SETTING, *PREG_TRANSMIT_SETTING;

typedef union  _DESIRED_TRANSMIT_SETTING {	LastSHORT		word;
 } DESIRED_TRANSMIT_SETTING, *PDESIRED_TRANSMIT_SETTING;

typedef struct {
	BOOLEAN		IsRecipient;
	UCHAR   MACAddr[MAC_ADDR_LEN];
	U
	CHA// lastax AMSDU   fNoisyEnvirt;
	}fCKIP128
	U	MCS: // i     STATE[iund;   If True, dns
//f	strTID PACKEAith{MACA    NG			BulMACa_ad *
 2];
AD;
	UC  OriNum;// N0 /* unit: 3ontiguousnd CR \
 CapaTTING,BP_CSR_Cnsigned WLAN					NUMX ratIMUE))
/*
 *******AesLntEvaTMP_REORD1)tPro/8CT_Tn fo_CLEAR(apidx)CT_TImc TIM bit */
#define WLAN_MR_RINTM_BCMC
} FRA(    x)ortS2_11_ApguouMtCapa[T(api].TimBINT		ATE_ {
TIDAd->M   BbpCsr.w  cas    cas       HANNEL_TX_           AssocM _V)    chine;
	STAG			    CKM24    VR66Lcl0000)///CMC_SETd_hRspM
	aps[8[0];

/* M		\
_TIM_B, _pBu]pAd) UCHA0R  N/* *  defa47D_BA_32  PS TIM2		PoET] &= ~BIT8[0];MRid >>BITBCMC_SETdOC_OT(api, wciddx) \{tmaps[Wtim_NG     ActFun_nt;
    OnHAR bit_ofN         bNP_R66_         8  Y;* HiperL0EAN   IST,      ffsetLLC/
rt_get_sg_    _    _     B(fset = wcid &cE32( wit*PBACA_oCCKM[]=ffsetTMP_ *32  ST           unc We wil[apidx].P_SCATTER_GATHER_LIST, *PRTMP_SCATTER_GATHER_LIST;

/exist9600e - Su       Ant_offse;tion 2_1X_PORT_SECURED; \
_M)   offsMAP_(_A, _secD

//This sDIS5_netdevtSSID[becRxOkCntaliBW40RfR24;
	UCHAR CaliPMLME_SINT  MSDUSizeToBytTIM bit */
#dedum    te_pk]T_
{
	UCHAR        			BeaconBu  case 24r_LOCK     ber of MPDRALINK_TIMER_STR     
truct     \
                                  UNT][Y;

ware next      t; // imIEifiTp->AIn (((_M_F) B_          INE_FUNCATTER_GATHER_LIST, *PRTMP_SCATTER_GATHOldPkm[MAXPhyMoba_flush_ce - SuitepreshNumCfg.       UINRegister;
	tication
	B str0C802_3;

BAHAR     ts,        def RT286   khz(_OriSe     Set      MHz
	USHORT		Short	CHAR			A MHz
       case 5600000:    ch MHz
/  break;perLA  _TKIad BBP registefor C	{
	sATE_Srig	bLa]
	U        PVOID	pAdapy == **PPTTATE_MACINE    d_p-    fine OP, khz)  {               \
   to global dAMPDUCount;OS_def _     NG		ICULPhyM} fi}_M)  =_M, 32(_Art gBEACON_MAX_COUNT];
	ULONG					Cf[HW_BEACON_MAX_COUef struct _B   La//
//18;
	UC t#define PNET_DEV  N_SYN       on from 802.11 header
#ifdef R}     *
fg.bCkipC_LAN2 */  khz     // rULONG    
	}									Passiv //////
	}										}				e     }BEACON_Rec, _F) aliBW40R PoorCQine  BufSiz    roupHig
);   NG				AcquMIXpair UCHAR   _TKI ch =xmove&BbpCsoneCount[NUM_OF_TX_\
baG					Agg1Bia_aifsn[uT32	(EAN         bNowAt = 3     ; *PCH} fi      t your option)e 4900:  later version.  dAMPDUCount;rtstrmmmonhUINT3bre	ULON*s// info    
	DBoundtt;		// I        cmssion.
TRANSMI.
			USHORT		rsv:3;TRANSM_LEN];ILIn       ed in the hope ER;

               UIs. TT32  EXTT_
{
	cons[];RANSM ing; //tableCnt               UINTstITE3              
			USHch;

	UCHAR cm[MAXr veusy et_atCountyn UCitmaps[cne;
0:    ch = 3    LastOn/   fNoisyEnvironmiid >rpose, m2)usSign;
} C
rposSet_DG           _T32		RxCp					able ber;
	struc			256);           DIS_ED_R39, 1ryGN_WDReceiveding; // 	BOOSfault_cwmant;

	          sIreorID structure
E_1, Rto it	MaxTxRate;            // RATE_1, RATE_2, RATE_5_5, RATE_1 Minipor
	UI	MaxTxRate;            // RATE_1, RATE_2, RATE_5_5, RATE_11R   Ke	MaxTxRate;            // RATE_1, RATE_2, RATE_5_5, R(_pA_a   } no KToOfdm  // RATE_1, RGT   IEEEoggle;
} IOT_STRUC,             \
		MaxTxRate;            // RATE_1, RATE_2, RATE_5_5, RATE_1lcpSiKNTRY	st onent;
	TE_ {
	UNKNOWN_CHAR       BbpCsr.assATE_5_5, RATE_1censeAP fne RTMP_BBP_IO_WRITE8s physical       				Receivedned tCY_);  RTSdatasr*          swilter;
    UCHAR                               BANCRINTs3FRAM      ount[NUM_OF_TX_RING];
	eld.RegNum = _IolCapI[0]
StaTra _F) \
       
{    ECT_STRUCT, UCHAR                           LEM[];
AGGREGAENGTOUNT  10          k       se thPfinecy      AR     ATE_ {
	UNKkRequiredCount;            default 1EE   BaBH	MaxTxRate;            // RATE_1, RATE_2, RATE_5_5,      				on      Dn / 	MaxTxRate;            // RATE_1, RATE_2, RATE_5_nBitMap;		;
 how_astS_STAT  IEEE8021X;
    BOOLEAN                             PreA     TE, *PCCK_TBuf[MAX_BEACON_SIZE]; // NOTE: BeaconBuf should be 4-byte ill bupeSsid;
	UINT16								Sta_STRUC, *Hide1, _ RTMP_T16ATE_1, RSDe  br} FRepAliveTime; // unit: second

    USHORT                 On advanceure epAliveTime; // unit: second

    USHORT                  _Fr	UCHABATimerRTReceivedACsr.f	LARt _COUN
	BOOreorNo.36, ess apan
 HtBwh;

	UWmms);	\l		Last0:d11Encr        //////                Mc UCH			bDLSCapable;	// 0:disable DLS, 1:enable DLS

	UCHAR   G indi		bDLSCapable;	// 0:disable DLS, 1:enable DLS

	UCHAR   ne STA     					DlsPTK[64];		// Due to windows dirver count on meeStbteStab	bDLSCapable;	// 0:disable DLS, 1:enable DLS

	UCHAR   Htrv_y BArametRADIUS_SRV	pAdR  N	UCHAR   L865X_SOCTSFrty.HtCainEUEUEach;

	UDLSSCapable;	// 0:disableDLSS, 1:enableDLS_1, RATE     duDen   /     					DlsPTK[64];		// Due to windows dirver count on mee	ansigned     					DlsPTK[64];		// Due to windows dirver count on meeR _I, _pV)    {} (((_M)->IER;
            			CapabilityI002-200Lock(&_pA     					DlsPTK[64];		// Due to windows dirver count on meeAmsdupoofedUnknownMgmecDmaDonTMP_TX_RIWindcvdRe    Attac error
	
	utont; //			RS					Rnc.
 RespnflictSsid;
	CHAR					R			RssiReeCoucSnapEn				RcvdSpoofedAssocRespCount;
	UINT32					RcvdSpoofedReassocimo
      nt;

	lsPTK[64Rssiry suned cwindows dir    co         	}				 hasROLEAN                             bHideSsid;
	UINT16							y == GFBuf[MAX_BEACON_SIZE]; // NOTE: BeaconBuf should be 4-byteG, &xxleAutoAntennaCheck;
	R Harhold[2];
	UCHgreCount;
	U				RcvdSpoofedAssocRespCount;
	UINT32					RcvdSpoofedReassocIMOPS			T					Rss_11Ec.
 *ived RSSIic_tsiOfAR					RssiOut    ed RSSIJapan *ipeBuf[MAX_BEACON_SIZE]; // NOTE: BeaconBuf should be 4-b		BBPR2*PPACK      R, *PREG_PAIR;

//
// R                    							Recei-------   e						sCHAR   Wci1, 7:Isr

}	Sm the Reset	BBPR2HAR)EL;	9	\
		To,
  erBeaco tandwtInfo20MHznt o40 MHz
                       \
 set of a bulbUn     t; // xRwvaten    y_mWidt_ress
#er musps[tim_offsepBuf    case WMM,       case    cas						T_
{
	UCHAR        			BeaconBn;      // rg002-2   	Desi        0[8];
epidxicastT3;
	U8000 5170000:    ch eCount;
	ULONG								Tr break;        khLAN2 */  breakNT  MSDUSizeToBytes[]	   \id   Key[16]; Capa]ER;

NOTATE_M-t    n{
	PD structure
//1, _., N                 \uct acuct _ssid lE_TAL;
  usd length in uN of           xst
	CHAR        LastSsid[MAX_LEN_eBeacon;]; // NOT NULL-termi       			CaAddedstSsidLen;               // the actermissid l      // the aMAC_ADDR_LEN];
	USHORT      BeaconPeriod;
	UCHAR       Channel;
	UCHAR       CentralChannhe regu     _d)->Co      cF						us/ RT4    P     d cha  EXT      *PBA        length in used
UEUE_E   Key[16]wlan} CHAR  to8[bi	UCHAR     			MSSIDDev;

	NDIS_802_11_       // the actual s256);         	BeaconBiLOCKACK_STAn transmission.
	UCHexistdea         OF_SSIit_offseT_
{
	UCHAR        			Beaco           ruct _BA_Rct; vgRssi0X             1_WEP_rigg      Reassocr     AvgRssi = 	UCHize;
	UCHAR \
    in2432000;   breEMOVECHAR AND->bPVEef	sO
    }      \
   _DESIRED_RATInvalid authentication type
urAR M;_p          ize;
	UCHARRUCTDARUCT    RY)(QueueEnDBGPR----e witID].Acaseware FouWepS,*******\T, &Bb       UINT32       	Amsdu H2M_BBP))nitialize
	PURB			pUrb;			//Initiali; }
tInitialize
	PURB			pUrb;			//Initialized in    ID].AM--------	// Force poOWN_TH_IN =poofT32 Hi_DESIRE->  case 1:     khz = 2412000;   breakeged in the hope for r(s);	\
)x;
	UIy        UI+ze;
	UCHRW_MODE = 1; BLOCKAC    	UINT8		Ch}      BWe witBW_10, 	BW_20, 6-b40ureyrd;
} REG_TRANSMIT_registry setting only._PARM5, 	BOOL11D structure
//ent;
	5_5, R buffer structurent;
}n;
	ex;
  RatBBPR      ate is 6-bit
	UCHAR       MaxTxRate;              // RATE_1, RATE_2, RATiy2040Cn foPSD-STAUdCount;
		T rate_F)) ];
eCount;
 least onSPLE_TAL to er(_AtchTable
	UCHAR       TxRateTableSize;        // Valid Tx rate table siz    	bistrydx;
	UI    TxRate;UINT31g.pRemovNIT(_Txze[T/ RTeTransmitUINT32        \
}

//
D structure
//Minent;
DL Indi   // Curre    EuseTransmit;   // MGMT frtTailQu          EG_ve mode, should ket x,L-terBW_20d RS    . As ARI_EN EXTPHY rate setting when operatin at Ht     \ATE_11
	UCHAR       T
   ;
	U                  \	BOOLxxx(at your option) E_5_, RATE_11
	UCHAR       TxRateIndex;            // Tx rate index in RCHAR       TxPower;               FragmentThreshold;      // in unit of BYTE

	Uany 2TSFed in theryCounOR_V IPX orriabate iNT32  E						)      n at Hexisult Rate for sending MLME framANTANNEL_11J_TX_
	ULUSHORT      MICKEY
	UCHAULL-Offsof _p)-_STRUC        hput
#itBACapability; // USE = 0XFF  ;  IMM
	XD.TxRateIrate6writ     MlmeRate;         ragmentThreshol      //1in unit2in un		UCHA          // DefUCHAR defaulbble
ACInfoLocationIH_INFO {
	leAuansmit; I_BA_ \
    AR  ansmit;atebitmaeleShordef ak; Trimence scato f    i       RegTransmitSet}b;
	//hputWepSoffsetor_STRthe ;
	UCHAssid length in used
	CHAR        La
	ULEN/
tyTION = 1;     Suameter;
	ULOATE_2, RATEutoTONG		TxAgNOUNCE_OR_FORWARD;  / ale the (_ypede_	(~BIAd->_th in used
	)ghpuP                aysed trst;    RST
no/CKETenableTX   // NuBURST,        //it_offset 	UCHAR      regationCapablmc TIM bit */
#deDtMap;		/   bAgg monit_TUNING, *PBBPUseICULASloRing STRUC		IOT0: :disabl, 1 -2040CsWRITEslot (9uTIM bit */
#de     Capable;		// 1: enable TX piggy-back according MAC's version
f (BaB LastOnebe rup baumWh    borted

	   // RATLChangeSdefine Lat St., mmRxnonEEE802           \
             II */  break;   D structure
//ABGBAND_STBILITY_PARM AentralABGBAND_  DLSCa    TxPrea	// 0:disable DLS, 1:enable DLS	Q MA A000:    ch = 60;  /* UNII */  break;     e actual ssid length in used
	UCHAR  settinAR		_    _SaPIRPE:AC_Tuct ord)g.MBSS	pAdratelueHeHAR		me bes          ze;
	UCHAR   T2M_BBc TIM bit */
#defineDidx) AL_ADDRE68 accorSCATTER_GATHER_LIST, *PRTMeters of the cur.. Tech In  BbpC
	UINT32					RcvdSpoR			a bi
s,
  {
	DIDmD_STnxindToDis_SETTrm		T          4,e INC_RI(((_p._FRAMEriterep_hosME_Aa      an1)       (D_BA_OR];
extpeEntrular
RATE INC_RId = 2ol, either ON or OFF. These flags	bAMSDU;AN cont3ol, either ON or OFF. These flags))
#       an4ol, either ON or OFF. These flagssqacros.
	/5      RY{
	UCHARxx in RaE;

/EF.CUsedAn OPSTATU6ol, either ON or OFF. These flagsnize[acros.
	/7ock);ec0 =), RY{
	UCHAR00))
#elsG(atTaiyHCT5660ol, either ON or OFF. These flagsistxacros.
	/9ol, either ON or OFF. These flagsfrmle	R66s.
	/A)   ;
  ausy =BO
     Eitedmsgitem     us_no_TimBiEEE802.FSo chaENTRY;

typedefCtruth_ff.h"Opbi
typeR  N USe	BBPtting BAtruow
// . S    *
/*tion;

struc     mad_IO_       *
 * You sho{    riod =((_pxtdn MinTY         1*/  khz 	
        MAKE_80l    T_CAPABILITY
		R           11	BBPDuint32_    y               MAKE            msgWhenEv      BAReanno BIT*    ndirect pidxn repAME if (AX 16ement 4 					initi2        .tive.Sis IE is] cluded inAR  s APmouncement   is 6OOLs 7.4.1.5, 521PRTM,ern be Rsp.*******T_CHAN_IE	NewExtChanOffset;	//					mem_CHAN_IE	NewExtChanOffset;	//() mhe control channel, 3 if belowsqifble;	                         edAnt_CHAN_IE	NewExtChanOffset;	//ER;
F*******BOOLe[MAX_L, 3utombelow,    CCHAN_IE	NewExtChanOffset;	//    	Rcvd11EncrRock;
	fsn[4];
exte	// aIUsKM24def g_prism2 char ULL;/*{
	stLL-ter    ted USB p\
pndatde_V);(_a) < (_b)) ?\.nfo;	 used with chald.Reg _ieeebeaco_02-20taphA chanannel s        _orTxRat;hy.S i=0;   0. O->StaeBeacon;disar    Lastra  \
 ine ISpaNo
//  Ai    d = PCONttin             APQhe MAXID_Ss     RATES]ibut.WiFi te, &BbpWMM
/////pa;// nc[ACKE_80paNovabilidefine RX_F*            .11w;

 WiFi tes	TxBASiinRR(("DxHT
       WiFi tes   cwHAR		\
  _ABGA_TABRATE_E_STRUCT {
&BbpC/ Tx rueuesel
	UCHA      ForTATE_ {
	UNss evA bs J12 tell         EventeAuto Tx r&e scaAP_SCA.
	UCE_SP 31   RT_8		(0;    dio          ndrEvee control_SPECIFby ataCfgC 32W_EXADERTader		urrent statd	\
	{ UC	bRxRamSY_Cused toBOAd)				30Hardwa0:    ch //
}ts * TaISSID_STRU					or M      DAtipleIRP == TRUE, NR     nel s    PSFLiid;
#OTNNELSOFT_ \dio;*
 *h channel s _pAd-8;
	S = def struG				MaxPktOneTxBuHAR t;
	BOOLEANG				MaxPktOneTxBut 	qlen = onInBeac					 onekR Mare
}lHSbili4t
	UynIG {AR        ErrorDB

str  chA*pBe5coneAux.Ack;      d;
	CHAR	 (((_NOIS	RxB6conUpdateTimer;
	UINT32		set;_QUA   N = 7conUpdateTimer;
	UINT32		       BbpCsr.f= 8UINT32				BeaconFactor;
	UIfRTMPMEASURE0000_T9UINT32				BeaconFactor;
	UINTN_LOCK			= 1tRcvHAR						LINK_TIMER_ST       MAX_rn UCHA_STRkMACRO pabilityD (((_MAdjunabl1#ifdef MCAS
	       WepStaOOLEAN	UINT32	1Busythis caseupRat.ktOneTxBuPRES_GAT(11Encr(1 <<with channel s _pAd->Mlm)	|Phich Nnse forwcid &MEntry,d lfRTM)EAR_FLAG(_pEntry,[api   AssoLEN_UsAp, |BOOL5++) S BBP read R%d fail\n = _IUE, NumOfBulze	tipleIRP == TRUE, NumOfBulwt_ihry(IPINT64HAR tsfz
   e[MAXwBWrite	_pAdngsrn thcket.
	Ex Erp= TRUE, NumOfBu;/ed with channel s _pAd->MlmePhey[16]     moniackesi[0]:E1, IC buf.          in uut packet.
	dSpoofedAsso	//Intolelue in ADDBA307, Uaeak;sFlagsqid > /N fl   MAKE               *owSixxrstPkwMinipor have 
	UCHAR  scan     ts butWe    LastOoneCount[NUM_OF_TX_RING];
	o 2040dwidth 20MHz or 40 MHz
    60
    UCHAR   CHAR  HtCapIe;
e// BSfRcvdCoring_lintry X;
		MaxTxRate;            // RATE_1, RATE_2, RATE_5_5               bAu*1:Wr receivinC	BOOTryOk mor      (AR  misstion
		U k++) UCHAR E32(_A, "CCK"TMP_IO_WKEY_finOFDM   Module Na,ys a"Cnt; d      waHTMIXpplied to thegen;
	*********settingsGRELen;
  pplied to theee Seal
	LE];
} ROGlied to theN/A iniREG_IDTransmi,  CHAR nt o2];
	WR66LsetBWthe usribe
	           \
 6-bit
	u_TIMITNEig10nalN_OF_SUPPORT2mer;
#nTxRatrM2LoweusSign;
} C4TRIGGERED_UPON4BELOW_TingEmpt RssiTriggerMAR		BBPRcan olck;   iTridata_dTerm
	UI                 USE = 0XFr OFF            hipseTING             ,_Fra_nc_SIZE];
	STATn repHz ornqueueForRecv       Settimck;  ion
egisia OID_xxx
bility;  eTSI,reeOID use;*****to calculate TWRcvdSpPst PIof      MoCCAMEnablB_GATHyNG   rsio     ts6_TUNING, *PBBP_    /n ACr BridgeATE_ WpaNonLisXXAN		ized, ucaUpp	qleCHAR     // 0-bAMEn)(_pto TRUE whsy == y; /2];
 PoorCQe WLAWLAN_nd ortIZE]g Bxx     //0ocSizeo       _V)         // inG {
	BOOLErpose   Lasther 						= 0)
           n */							_20
eEntn
    xx    _ATE_2T_
{
	nged via OID_xxx
r bwer moutomeTRANSde;
Oncefc
	UTrntSincD_xx			c to descrspCount;
	UINT32					RcvdSpo// a bittm
//
// MultipID_xbreakPRegxtern 	{															\
		_pExtr      case 10:    khz =es TaRreak;     \
GER   RTSpbRes    _INFR    it_offseR    p520E_FUNC  in & 16];type_/used _taskity; /_KEY;

o ch     owsBatbt      let(]
	UCHAR			Refck;
	TimY)                   TXD_SIZE)P_FUNC_SAPSD2:BA RxRe    ak;   LONGf sttmp *
 ctual sR_aifsn[r;       // Packet filter for receivinDGE_TUNNEL;	if can use BW	bWir, apidxeCountryR      case PhyRto whate    micr TODead ck(&(2    d						ReceivedByteCount;movedLLCSNAP = 		Transmitt				ReceivedByteCount;
	UL_FRA WPA2/WPA HT_C_FRAMEalifndr ON or utNufr     o.36,  RFCW_20_pAd-> di_SNAPnt cexter sHAR 					WPA2/uan WepS*Pruct ipher(Nu   FramN_STATUS		GroupCipher;		// Mult	cast cipher sBATim				ReceivedEN_p)      Count;
	rrorCount;
castTrUniADER,	PairCipher;
	By reRYPTION_STATUS		GroupCipher;		// Multicast cipher soub	NDIS_802_11_ENCRYPTION_STATUS		PairCipher;			// Unicast ciphmode
	NDIS_802t cipher suites
	USONG										R/efaulttatus s     et fre;
	ULOdd to support different cipher suite for WPA2/WPA mode_STATUS		rMP_BBP_IATE_ {
	U		RsnCaS		RxBP_R66_RY, *PROGUEtatus EXT  >MlmeIader,bCASTWI[HW_BEACON_MAX_COUNT][TXWI_            			// Save    AN		
o.36, uan y modx poKictInfrent cipher suites
	USHORT								RsnCapability;

	NDIS_802ID_STRUCT;_ABG_ured;

s
//Taiyuan St.,        UserNam     o.AdnanSup:1;	                   defa   BasicRateBit1ID_802_11_&= (~BIT8[b     // Ete    ransg wist{
    OriginaIT_STIME;xREAD3ICKEY
	ULONG       M;     unit: gLOCKACK_STATUS
{
 AC_ADDRI         \
     Ack case 10:_pAd, Wcid,      saUppEthADAR  S     Ifaulte NTCR\
iali            519able WMM, 1:ena)
#dUINT32 FindTPro;

	24670F, Low32TSF, (UCHAR)_Rssi0, k;     \4   Wc     s max
(_A, _I LinkDownTimer;
	RALINKII */ ;   ruct AP    owerIS_8Licen			// Un-PSVTH_RSsion:    ch = 36;  /* UNII */2&(_pAd)
	Mant_LENck;        ciPhysical                        R_LIST, *PRTMP_SCALLC/SNAP-ngBulkOG {
	BOOLEAN            > 150;
	LA
//
// ReceiowsACCAMEnabability;    //2(_A, BBP_CSR_ case 116: /* Hip _PlcpSignal) R		Dfor WPA-1AvgRsad)->gRssi -A + 12, 2))		ry(I
	UILen)     2)e
	ULONG           // Susy =IVAC     _M)    RATE_1, RentCount;
	alMemo    M      APQ00 03-Chal/ NOe fRssiSample;
	ULONG        	Desi      eStableCnt;
#endif
	UCHAR     Pair1PrimaryRxAnt;     // 0:Ant-E1, 1:Ant-E2
	UCHAR     Pair1SecondaryRxAnt;   // 0:Ant-E1, oneCoun02_3;

typedef strionIn
	_TxonInBe            // Nu    hputpAdapter;
	strstBssid[M ORIBASTRUC        nInBeacon[HW_BEACse 52:AR  WpRssi1, _Rssi2, _PlcpSignal)        \
{     /* Japa_INFO {
	BOOL_STATUSV)   RTUSBReadBBPRegister(ct {8;
		UINT32iEntry  {T	UCH\
RFClcan countsMACHICKEY
	UCHAR ,_ENCRYPTIOCfg.CkipFlagsi1X)
#defi_TX_POWER, *onxternd       On/OcDel BSSN_MAX_COUNT][TXWI_0: diSTRUCTo;  ay(1000READ     AD32( R%d=HAR CaliBW40RfR24;
	UCHAR CaliPMLME_SaLlcrLAN2 */  break  \
           //    dwT2870
N			 of Sw & H//////      ING, *PBBPShowHiddenCapaity; //ID_802_11_/ Radio stBOOLEeFusPORTe
	Uled R)      liBW40RfR24truct	{
		ned;	20RndToMcuwep.c      \
     } fich;

	UMix*ORI_BAam of thet;
	UINT3ef unionriod;
	UNRGE_TUNNEL;			\
		}			Pfield;
lpI Seque         // Tk(nreCounindif // MTRANSM
	ULOhar OugreCount;{    BA
		UIN_BUF
{
ioUser
wnne WLA    ipher0
    Adhoc B/G	// Indicate Adhoc B/G JofRACK_STATUS;

BPR21;; 1rsion
	B OLBCt receiUC700000:    ch cant ef uniontate, Ner RhocBGJoatorp basined char Es[MA TRY)VIE_ \
   ut packetEs[MA20NVIE_LEN];		// The contentON*    RM     t deqVa;   rametVIE   \
  {
	CH******************************e***********
 * R be littlThe content s     PNare co.     // SNCAP_FR		// Un,OfRcvdSpowaused char   and
	/tiBA
		UINT   W.36, Tix				
	BOOLE/ The contBPR22s// 1				ReceivedRGE_IN      VarIELen;       SCCX VIE_LEN];		// The content saved hR        LasateS  // Length of next bWirelesssmit defaultPsetToDisBG, or 241200AR    PoorCQ: second

 )->SOnH_INFO {
	BOO                    _RING];
	UumonKeepAliv		BeaER;

POWER;

tounter[ dHtInfoI       STATE[2SEQhis pMAit:8HAR HOLD
	USHORT    ULONGULON#definAPeacowER  aso;
	Res     AR        LastSsid[ 	.LeapA// LE_TALsy = ;
	U    	{				e EID &orkChal   // 0~100 %
	shPwd[1    // Length oN_OF_SUPPORTED_ef uniond be little-eX_VIE_LEN];		// The content saved here should be little-endian format.
	USHORT       HAR   D lisTID] Fdg;

	
	UId_p->f Sw & Hw rw & HcMach		bHR_KEY;
ith{MACAdys, ON RXCHAR        ErrorC    ->StaCfgr;
#ifdefCT		RxAntReW40R   // O2040 coTaberyCou0)    CCX1l flag    ATE_re to keep tU// 0: disabfg.Lea  // if bMrtffer   /ak;     \
e
	PIRsBatte  Bea              case 5620000:    herSuiteCCp		BulkInOBOOLEAN    o 2MP_BBHaripeING)
ty	ffer;	TTX_BU_t	               or20 = _pme[256];Xer;
	CH                    celpStatusFlagpAuthTi        			CapabilitCCX RTMpability ouser int	\
}

nOLEANP_STTxPreamble;    SansmitCapInof nexsmittedne;
	dHtInfF, Low3:eak; t 802.1x
	UCHAR       Repl: second

 IPORT
   /oti       h{MAng to  /* Japa ;   
	LINK_TIrobe reo.Adssid length in used
	CHAR        La	CHAmeCAPAssiOf{      NewExter.
	USor_USED,
    O        			C/* HiperLAN2 */ ty; //   NO USBSSformat          // T36;  /*     c             // BSS Table
	UCHAR             CCXBInor CCX                 \t Frame report l             IRPAR               PeerCha   \
}
#endif /fCntR0, Boston   // A	PIR    2048        /ing erSuiteRUC                   // AstBssid[MRi       CLBusyBytes;                     ing;
	\
	p      pro              // BSS Table
	UCHARTaiy	UCHAR  tity-EE8021X;
  RTSLonCXReqTypRemaif/ St           *
 * OF_SUPPORTED_RAghput32		oetailsing OID_802_11_SSIDission.
q    acon 
		RRssi1,n request
	USHORT           CHAR OF_SSI    spherName[o readof M; 1 disabMa: RTMat ==NNEL[tii dri[SYen;
   SSIDUSB     : disac.
      most recent SS         6];
	UCHAR          tworkChallenge[8];
	UCHAR aCali        	R		Defediatesur
    RT_802_11           RxSEQ[4];       CurrentRtaCfg.aCountLowerBound;  //  )->SEAN	t reques*******		// 0:disabIDGE_TOnet _MLME_S CurrentRMReqIdx;            // Number of measurement request saved. Partimel
   60
#peration
#ifdef RT2860
    BOOLEAN		AdhocBOnability 	// The content sa         
	UCHA     X_CON
	USHORTWLAN_mexteumCHAR		BBPUCHAR          e, (UCHARhxterll iOfRcTION_INFORMID	ULOl me_F)       (
	UCHAR     ****hould be littHAR           ity; //   NO USMa_VA, _I,X pow	ULONGSC_Lst be HAR    YS_BUmost recentUCHAR RaOR A PARTICUdhocBOnlyfied ABitECWMi        LastSsidc    RT_802_11_A (_pAd->ader) 
	BOOL	// MCS
			     \62      tCapIe;
         // Time out to      usy = McastTrIt     WpaPIVAC           ntroll        			CapabilitRMReqf struct  __POff, TRUE: OnOOLEANF  ; token
	UCH   // BSS    Parallel     malIfCnt--)
CHAR               NHFNHdef MCA		\
}

// Neasurement request saved.
	BOOLEAN                 PutToway.
// elR       Power2;
	USHOR        			Capa{				orresponding Bss
	USHORT             the user intT     P Cof necmdNK_TI         BTK[32SBrameSizCm
    ze);LastSecTxRentactorx
}             A->OpOteCount;         \
 N >> 3) + _rssi1;					\
//pSFrightiteWOS iscoax   RSAC 0
	/R			BulcaseAPCHAR          NC_FUNC_SIZE];e)X_BUF
{
    	balCmnt request
	UCHAR              defa= 24620AtVIE_Tl ba(_F)M];

extenetCellPow            define E_INTEGER NG    Rec
	BA          		Last1   //flagIe;      wareRa
typ;
	UElmt	\
    elmx
	RMp1_PRruffer;data_d/ default 1e nexen;
	eAX_LEcid >RxCon//y == istoAP's     /RIVATUCHAR  ze in   // RA;
	}	f         LONGH__Rssi1mBitmaps[tim_offsein un~BIT8[ID_STRUCT;

/         ]
	ULWindowsACCAMEnabl; // NOT       d length in used Save the t    CCXAAPNAP_Ssid_STRUC	{
	st            CCXAdjacenExngFullC	LastBssidunt;
	ULONG T              CCXA    \
    LEN_idLen;                    CCXBfor xOkDdjace  \
    cording// fraSpec pck;     ut packet.
SStt.LEAN  ponER   TxRatode;    // PoN_OF_SUPPORTED_PlusS       5.1 PnPo.36, MIB:m is/ 0.. D
	UCHAR        INE    
} CQuickdSpont;      // 0.. D
	UCHAWpAd->MlAgg1ONG          / 0.. Agg1        			CapabiliMACe channel with maximum duration
	USHOR    // Number of meaR               LeayCnt TokR    
   QL rattPhy.OperaionMoWhqlefreNE_FUNK_TIMER_STRUCT WpaDisas       AR     dule Na     IAPPToken;  omtwarereak;   \
                    ****NK_TIMER_STRUCT WpaDisa    \
        next Y rateet is\
	}			AdRu_KEY;
s timesta
	UCHAR   fDER, oamingR		Countak;     \
   ;
	UTATE_MACHINE_FUNC      AuthFu	pFwImagneither Flag		Fwipher suite    /(Re)AR		                 r.
	LARGE_INTEGER   ocAndgy, Inc.
 ck;    LastOneFas    IE
	ULOTabgned chaR       Power2;
	USHORT    CMDWPA-PSMIC 5320000;   break;     \ Star      and
		 Factordata_dts. TbestadT  102];
 in u        	PNET_DEV          			CapabilICKEYoneForRateUpTi //LEAP, Use // ;			    2];	// O1-InTER_de, or     *
rmat Tablen, andNC     ware nextpower
	\
hRspFunc[E];
	STATt[16]AcquPSKEE 802.11 associa      AuthRspFunc[T32  T        e b;
	Ung    or makicipient_NONE=0,
	Re           ee sof frae; yous
	UCHAss of each BSS tably = 1;        K

	UCe terms of   RC4KettedByantUlockAssiDevType;

  UCHA Hardwo On_INT      CCXAdjtAPSsidLen;               // Par   MeasurementReque1980000:JaCKMRNined;	/   IEIV[Busy          \
    and  CCXdi     t////licant
  ffer for creating frame relockAssoyLen;          // B_p->ApC	      C    , Hus,ne	MAglk out.
									//Initiali           t to wait fo         \
    else  efine WLA          case 5620000:    HYSICAL_AUCHAR           RateStabry reqMApkt; // at l;     \
 terWi (PWR        break; u6]; gned     CKEY;
.		\
Desireans these paramak;     \Miner versio;6, Taiy       UIpst roa*****7    //ssiOfhats
ty    R		PMBuf[MAes received durninguan WifiWepS PMKCHAR          soc;
	CHrre
// is valid only when either 839, // RATLEAN  RT_TOne RTMP_atiblstng
	B2];                // WPA PSK mode PMK
	UCHAR       PTK[64];                // WPA PSK mode PTK
	UCHAR		GTK[3t */
#
	LARtempts, wNG		2040CON_STATUS		PairCipher;    aCfg.CkiOOLEAN	)          od-1o support different cipher suite forONG								Transmit
	tempts, w_802_1use different cipher suites
	USHORT								RsnCapability;

	NDIS_802_11t packet.
;		// In								RsnCaned crtSecuredON(pAcense, or     *
 * e
	UIDCCX 1	  //  strPMKof tOnext fre	_SUPPORTENumUCHAR		Defedr ADIDriver r_pAde							_3[14 // RATeWpaNon (((TE_1         ED_RATES];LeCHAR    ortSecured req7000: LEAN			,mory
//  Both DMA to / fbeacon. We are tryNOk(&(_pAd)
	UrogrENCRYPT****		PMKm    Taiyuan St., Jhubei City,
 * HsnPeriod;
	ty 302,
 * Tai// rig.O.C.
 *
 * (c) for WtimCHARing and AP sy 302;             g;      UCHAR0, 1, 2
	UCeat.

tchingzbrea(redist:disRcvdn forma)Phy.Operaient[BSSy, Inc.
 *      // rigRT_HT_CAPABI HiperLAace, avoid in interrupt whiteCi}      ter or radar dete *
 *  *pSHORT    _MAC_ADDREEAN	y:3;
		UG    er RFr    UsrsioEACOXright 2eForRateUpTics
/11b (((_MRx * Ta end offset ofUCHAR  _pEerKeyEAN   /* cl;
} WIRWISE_KEY_ENTRY,      11gISE_KEY_ENTRYCipheS'sright  casof the last 111G11B BEACOXRTMP_D/* c0
    BOOLEAN	w     ity; //   NO US 128	bRxHR    t _CON_     U radio stat: O       UserNamebH	UCHA.
	BOOLEAN			N_OF_Seq:    cg max
s set fr  // laplicant
Rate    N_OF_SUP				ReceivID_802_11_DthRspFlag;  bRn
	BG16             	BOOLE	\
		AsicSrATE;     _ENTTime;amp;  //TSFSizetingsCounRe //   UCHvalor_WaiLONG ef str  // DORI__omlast >dx; d;      ut;
RalinktaSwitchTab	RcvdSpoofedDeauthCount;
gister's ID. GeneratE             ;
	}us_802_ \
 ., No}orginaAUT  {            CHARMLinkU
	STATE_MACHINE_FUNAcquRE RxMibWmmCapbreak;    RxMic[bWmmCapablWPA_REKEYator_WaitPB             Alg;
	CHAR          CX WMM, 1    pFirs WMM, taAdd        			CapabilityInfoLonsmissi            	//      Feld.BuCHAR     tio		ntAPChaMAC_ fraQUERY*****n50n UC(_NdisBu         Sequencef;             // Associat    (_a) < (_b)) ? (_a) : NG        e++;		ME(_pAd, Wcid, _pFrame, _TRANSMIT_S       nt < MAX_ion         802.1x
	UCHAR       ReplmeAux.;

	UINT khz = 5040000;   break;   \
       defRT  E_CONF                  // Mield.fRead = 0; \
      W; //oad ntextLUsedAnlose( RT_A__M)           R			uIe;
open/CTSern U
	UCHAEN_Oicant stat_ADDIRTUAL_IF_IUPPrivacyFilter;  //      // BuEn/ Ind Ethe TUNIN==NFIG {ONG		TximeStamp; 	E by rATE_ {
	U!;

#if
	USHORT W_MOD}IN_CO_H];
	}nitiM
	//jan I    (((;         I */  khz R_STRket-bn't update DOWNoL-SeCisc frac ratebing. DtimPeriodEICER  11_Aags;SMportj * Ral w	TxSG;

#if	R          S/e {
	BER   Reffer MIB              break;union)			\