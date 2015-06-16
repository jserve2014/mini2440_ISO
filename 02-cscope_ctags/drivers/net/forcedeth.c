/*
 * forcedeth: Ethernet driver for NVIDIA nForce media access controllers.
 *
 * Note: This driver is a cleanroom reimplementation based on reverse
 *      engineered documentation written by Carl-Daniel Hailfinger
 *      and Andrew de Quincey.
 *
 * NVIDIA, nForce and other NVIDIA marks are trademarks or registered
 * trademarks of NVIDIA Corporation in the United States and other
 * countries.
 *
 * Copyright (C) 2003,4,5 Manfred Spraul
 * Copyright (C) 2004 Andrew de Quincey (wol support)
 * Copyright (C) 2004 Carl-Daniel Hailfinger (invalid MAC handling, insane
 *		IRQ rate fixes, bigendian fixes, cleanups, verification)
 * Copyright (c) 2004,2005,2006,2007,2008,2009 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Known bugs:
 * We suspect that on some hardware no TX done interrupts are generated.
 * This means recovery from netif_stop_queue only happens if the hw timer
 * interrupt fires (100 times/second, configurable with NVREG_POLL_DEFAULT)
 * and the timer is active in the IRQMask, or if a rx packet arrives by chance.
 * If your hardware reliably generates tx done interrupts, then you can remove
 * DEV_NEED_TIMERIRQ from the driver_data flags.
 * DEV_NEED_TIMERIRQ will not harm you on sane hardware, only generating a few
 * superfluous timer interrupts from the nic.
 */
#define FORCEDETH_VERSION		"0.64"
#define DRV_NAME			"forcedeth"

#include <linux/module.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/ethtool.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/mii.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/if_vlan.h>
#include <linux/dma-mapping.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#if 0
#define dprintk			printk
#else
#define dprintk(x...)		do { } while (0)
#endif

#define TX_WORK_PER_LOOP  64
#define RX_WORK_PER_LOOP  64

/*
 * Hardware access:
 */

#define DEV_NEED_TIMERIRQ          0x0000001  /* set the timer irq flag in the irq mask */
#define DEV_NEED_LINKTIMER         0x0000002  /* poll link settings. Relies on the timer irq */
#define DEV_HAS_LARGEDESC          0x0000004  /* device supports jumbo frames and needs packet format 2 */
#define DEV_HAS_HIGH_DMA           0x0000008  /* device supports 64bit dma */
#define DEV_HAS_CHECKSUM           0x0000010  /* device supports tx and rx checksum offloads */
#define DEV_HAS_VLAN               0x0000020  /* device supports vlan tagging and striping */
#define DEV_HAS_MSI                0x0000040  /* device supports MSI */
#define DEV_HAS_MSI_X              0x0000080  /* device supports MSI-X */
#define DEV_HAS_POWER_CNTRL        0x0000100  /* device supports power savings */
#define DEV_HAS_STATISTICS_V1      0x0000200  /* device supports hw statistics version 1 */
#define DEV_HAS_STATISTICS_V2      0x0000600  /* device supports hw statistics version 2 */
#define DEV_HAS_STATISTICS_V3      0x0000e00  /* device supports hw statistics version 3 */
#define DEV_HAS_TEST_EXTENDED      0x0001000  /* device supports extended diagnostic test */
#define DEV_HAS_MGMT_UNIT          0x0002000  /* device supports management unit */
#define DEV_HAS_CORRECT_MACADDR    0x0004000  /* device supports correct mac address order */
#define DEV_HAS_COLLISION_FIX      0x0008000  /* device supports tx collision fix */
#define DEV_HAS_PAUSEFRAME_TX_V1   0x0010000  /* device supports tx pause frames version 1 */
#define DEV_HAS_PAUSEFRAME_TX_V2   0x0020000  /* device supports tx pause frames version 2 */
#define DEV_HAS_PAUSEFRAME_TX_V3   0x0040000  /* device supports tx pause frames version 3 */
#define DEV_NEED_TX_LIMIT          0x0080000  /* device needs to limit tx */
#define DEV_NEED_TX_LIMIT2         0x0180000  /* device needs to limit tx, expect for some revs */
#define DEV_HAS_GEAR_MODE          0x0200000  /* device supports gear mode */
#define DEV_NEED_PHY_INIT_FIX      0x0400000  /* device needs specific phy workaround */
#define DEV_NEED_LOW_POWER_FIX     0x0800000  /* device needs special power up workaround */
#define DEV_NEED_MSI_FIX           0x1000000  /* device needs msi workaround */

enum {
	NvRegIrqStatus = 0x000,
#define NVREG_IRQSTAT_MIIEVENT	0x040
#define NVREG_IRQSTAT_MASK		0x83ff
	NvRegIrqMask = 0x004,
#define NVREG_IRQ_RX_ERROR		0x0001
#define NVREG_IRQ_RX			0x0002
#define NVREG_IRQ_RX_NOBUF		0x0004
#define NVREG_IRQ_TX_ERR		0x0008
#define NVREG_IRQ_TX_OK			0x0010
#define NVREG_IRQ_TIMER			0x0020
#define NVREG_IRQ_LINK			0x0040
#define NVREG_IRQ_RX_FORCED		0x0080
#define NVREG_IRQ_TX_FORCED		0x0100
#define NVREG_IRQ_RECOVER_ERROR		0x8200
#define NVREG_IRQMASK_THROUGHPUT	0x00df
#define NVREG_IRQMASK_CPU		0x0060
#define NVREG_IRQ_TX_ALL		(NVREG_IRQ_TX_ERR|NVREG_IRQ_TX_OK|NVREG_IRQ_TX_FORCED)
#define NVREG_IRQ_RX_ALL		(NVREG_IRQ_RX_ERROR|NVREG_IRQ_RX|NVREG_IRQ_RX_NOBUF|NVREG_IRQ_RX_FORCED)
#define NVREG_IRQ_OTHER			(NVREG_IRQ_TIMER|NVREG_IRQ_LINK|NVREG_IRQ_RECOVER_ERROR)

	NvRegUnknownSetupReg6 = 0x008,
#define NVREG_UNKSETUP6_VAL		3

/*
 * NVREG_POLL_DEFAULT is the interval length of the timer source on the nic
 * NVREG_POLL_DEFAULT=97 would result in an interval length of 1 ms
 */
	NvRegPollingInterval = 0x00c,
#define NVREG_POLL_DEFAULT_THROUGHPUT	65535 /* backup tx cleanup if loop max reached */
#define NVREG_POLL_DEFAULT_CPU	13
	NvRegMSIMap0 = 0x020,
	NvRegMSIMap1 = 0x024,
	NvRegMSIIrqMask = 0x030,
#define NVREG_MSI_VECTOR_0_ENABLED 0x01
	NvRegMisc1 = 0x080,
#define NVREG_MISC1_PAUSE_TX	0x01
#define NVREG_MISC1_HD		0x02
#define NVREG_MISC1_FORCE	0x3b0f3c

	NvRegMacReset = 0x34,
#define NVREG_MAC_RESET_ASSERT	0x0F3
	NvRegTransmitterControl = 0x084,
#define NVREG_XMITCTL_START	0x01
#define NVREG_XMITCTL_MGMT_ST	0x40000000
#define NVREG_XMITCTL_SYNC_MASK		0x000f0000
#define NVREG_XMITCTL_SYNC_NOT_READY	0x0
#define NVREG_XMITCTL_SYNC_PHY_INIT	0x00040000
#define NVREG_XMITCTL_MGMT_SEMA_MASK	0x00000f00
#define NVREG_XMITCTL_MGMT_SEMA_FREE	0x0
#define NVREG_XMITCTL_HOST_SEMA_MASK	0x0000f000
#define NVREG_XMITCTL_HOST_SEMA_ACQ	0x0000f000
#define NVREG_XMITCTL_HOST_LOADED	0x00004000
#define NVREG_XMITCTL_TX_PATH_EN	0x01000000
#define NVREG_XMITCTL_DATA_START	0x00100000
#define NVREG_XMITCTL_DATA_READY	0x00010000
#define NVREG_XMITCTL_DATA_ERROR	0x00020000
	NvRegTransmitterStatus = 0x088,
#define NVREG_XMITSTAT_BUSY	0x01

	NvRegPacketFilterFlags = 0x8c,
#define NVREG_PFF_PAUSE_RX	0x08
#define NVREG_PFF_ALWAYS	0x7F0000
#define NVREG_PFF_PROMISC	0x80
#define NVREG_PFF_MYADDR	0x20
#define NVREG_PFF_LOOPBACK	0x10

	NvRegOffloadConfig = 0x90,
#define NVREG_OFFLOAD_HOMEPHY	0x601
#define NVREG_OFFLOAD_NORMAL	RX_NIC_BUFSIZE
	NvRegReceiverControl = 0x094,
#define NVREG_RCVCTL_START	0x01
#define NVREG_RCVCTL_RX_PATH_EN	0x01000000
	NvRegReceiverStatus = 0x98,
#define NVREG_RCVSTAT_BUSY	0x01

	NvRegSlotTime = 0x9c,
#define NVREG_SLOTTIME_LEGBF_ENABLED	0x80000000
#define NVREG_SLOTTIME_10_100_FULL	0x00007f00
#define NVREG_SLOTTIME_1000_FULL 	0x0003ff00
#define NVREG_SLOTTIME_HALF		0x0000ff00
#define NVREG_SLOTTIME_DEFAULT	 	0x00007f00
#define NVREG_SLOTTIME_MASK		0x000000ff

	NvRegTxDeferral = 0xA0,
#define NVREG_TX_DEFERRAL_DEFAULT		0x15050f
#define NVREG_TX_DEFERRAL_RGMII_10_100		0x16070f
#define NVREG_TX_DEFERRAL_RGMII_1000		0x14050f
#define NVREG_TX_DEFERRAL_RGMII_STRETCH_10	0x16190f
#define NVREG_TX_DEFERRAL_RGMII_STRETCH_100	0x16300f
#define NVREG_TX_DEFERRAL_MII_STRETCH		0x152000
	NvRegRxDeferral = 0xA4,
#define NVREG_RX_DEFERRAL_DEFAULT	0x16
	NvRegMacAddrA = 0xA8,
	NvRegMacAddrB = 0xAC,
	NvRegMulticastAddrA = 0xB0,
#define NVREG_MCASTADDRA_FORCE	0x01
	NvRegMulticastAddrB = 0xB4,
	NvRegMulticastMaskA = 0xB8,
#define NVREG_MCASTMASKA_NONE		0xffffffff
	NvRegMulticastMaskB = 0xBC,
#define NVREG_MCASTMASKB_NONE		0xffff

	NvRegPhyInterface = 0xC0,
#define PHY_RGMII		0x10000000
	NvRegBackOffControl = 0xC4,
#define NVREG_BKOFFCTRL_DEFAULT			0x70000000
#define NVREG_BKOFFCTRL_SEED_MASK		0x000003ff
#define NVREG_BKOFFCTRL_SELECT			24
#define NVREG_BKOFFCTRL_GEAR			12

	NvRegTxRingPhysAddr = 0x100,
	NvRegRxRingPhysAddr = 0x104,
	NvRegRingSizes = 0x108,
#define NVREG_RINGSZ_TXSHIFT 0
#define NVREG_RINGSZ_RXSHIFT 16
	NvRegTransmitPoll = 0x10c,
#define NVREG_TRANSMITPOLL_MAC_ADDR_REV	0x00008000
	NvRegLinkSpeed = 0x110,
#define NVREG_LINKSPEED_FORCE 0x10000
#define NVREG_LINKSPEED_10	1000
#define NVREG_LINKSPEED_100	100
#define NVREG_LINKSPEED_1000	50
#define NVREG_LINKSPEED_MASK	(0xFFF)
	NvRegUnknownSetupReg5 = 0x130,
#define NVREG_UNKSETUP5_BIT31	(1<<31)
	NvRegTxWatermark = 0x13c,
#define NVREG_TX_WM_DESC1_DEFAULT	0x0200010
#define NVREG_TX_WM_DESC2_3_DEFAULT	0x1e08000
#define NVREG_TX_WM_DESC2_3_1000	0xfe08000
	NvRegTxRxControl = 0x144,
#define NVREG_TXRXCTL_KICK	0x0001
#define NVREG_TXRXCTL_BIT1	0x0002
#define NVREG_TXRXCTL_BIT2	0x0004
#define NVREG_TXRXCTL_IDLE	0x0008
#define NVREG_TXRXCTL_RESET	0x0010
#define NVREG_TXRXCTL_RXCHECK	0x0400
#define NVREG_TXRXCTL_DESC_1	0
#define NVREG_TXRXCTL_DESC_2	0x002100
#define NVREG_TXRXCTL_DESC_3	0xc02200
#define NVREG_TXRXCTL_VLANSTRIP 0x00040
#define NVREG_TXRXCTL_VLANINS	0x00080
	NvRegTxRingPhysAddrHigh = 0x148,
	NvRegRxRingPhysAddrHigh = 0x14C,
	NvRegTxPauseFrame = 0x170,
#define NVREG_TX_PAUSEFRAME_DISABLE	0x0fff0080
#define NVREG_TX_PAUSEFRAME_ENABLE_V1	0x01800010
#define NVREG_TX_PAUSEFRAME_ENABLE_V2	0x056003f0
#define NVREG_TX_PAUSEFRAME_ENABLE_V3	0x09f00880
	NvRegTxPauseFrameLimit = 0x174,
#define NVREG_TX_PAUSEFRAMELIMIT_ENABLE	0x00010000
	NvRegMIIStatus = 0x180,
#define NVREG_MIISTAT_ERROR		0x0001
#define NVREG_MIISTAT_LINKCHANGE	0x0008
#define NVREG_MIISTAT_MASK_RW		0x0007
#define NVREG_MIISTAT_MASK_ALL		0x000f
	NvRegMIIMask = 0x184,
#define NVREG_MII_LINKCHANGE		0x0008

	NvRegAdapterControl = 0x188,
#define NVREG_ADAPTCTL_START	0x02
#define NVREG_ADAPTCTL_LINKUP	0x04
#define NVREG_ADAPTCTL_PHYVALID	0x40000
#define NVREG_ADAPTCTL_RUNNING	0x100000
#define NVREG_ADAPTCTL_PHYSHIFT	24
	NvRegMIISpeed = 0x18c,
#define NVREG_MIISPEED_BIT8	(1<<8)
#define NVREG_MIIDELAY	5
	NvRegMIIControl = 0x190,
#define NVREG_MIICTL_INUSE	0x08000
#define NVREG_MIICTL_WRITE	0x00400
#define NVREG_MIICTL_ADDRSHIFT	5
	NvRegMIIData = 0x194,
	NvRegTxUnicast = 0x1a0,
	NvRegTxMulticast = 0x1a4,
	NvRegTxBroadcast = 0x1a8,
	NvRegWakeUpFlags = 0x200,
#define NVREG_WAKEUPFLAGS_VAL		0x7770
#define NVREG_WAKEUPFLAGS_BUSYSHIFT	24
#define NVREG_WAKEUPFLAGS_ENABLESHIFT	16
#define NVREG_WAKEUPFLAGS_D3SHIFT	12
#define NVREG_WAKEUPFLAGS_D2SHIFT	8
#define NVREG_WAKEUPFLAGS_D1SHIFT	4
#define NVREG_WAKEUPFLAGS_D0SHIFT	0
#define NVREG_WAKEUPFLAGS_ACCEPT_MAGPAT		0x01
#define NVREG_WAKEUPFLAGS_ACCEPT_WAKEUPPAT	0x02
#define NVREG_WAKEUPFLAGS_ACCEPT_LINKCHANGE	0x04
#define NVREG_WAKEUPFLAGS_ENABLE	0x1111

	NvRegMgmtUnitGetVersion = 0x204,
#define NVREG_MGMTUNITGETVERSION     	0x01
	NvRegMgmtUnitVersion = 0x208,
#define NVREG_MGMTUNITVERSION		0x08
	NvRegPowerCap = 0x268,
#define NVREG_POWERCAP_D3SUPP	(1<<30)
#define NVREG_POWERCAP_D2SUPP	(1<<26)
#define NVREG_POWERCAP_D1SUPP	(1<<25)
	NvRegPowerState = 0x26c,
#define NVREG_POWERSTATE_POWEREDUP	0x8000
#define NVREG_POWERSTATE_VALID		0x0100
#define NVREG_POWERSTATE_MASK		0x0003
#define NVREG_POWERSTATE_D0		0x0000
#define NVREG_POWERSTATE_D1		0x0001
#define NVREG_POWERSTATE_D2		0x0002
#define NVREG_POWERSTATE_D3		0x0003
	NvRegMgmtUnitControl = 0x278,
#define NVREG_MGMTUNITCONTROL_INUSE	0x20000
	NvRegTxCnt = 0x280,
	NvRegTxZeroReXmt = 0x284,
	NvRegTxOneReXmt = 0x288,
	NvRegTxManyReXmt = 0x28c,
	NvRegTxLateCol = 0x290,
	NvRegTxUnderflow = 0x294,
	NvRegTxLossCarrier = 0x298,
	NvRegTxExcessDef = 0x29c,
	NvRegTxRetryErr = 0x2a0,
	NvRegRxFrameErr = 0x2a4,
	NvRegRxExtraByte = 0x2a8,
	NvRegRxLateCol = 0x2ac,
	NvRegRxRunt = 0x2b0,
	NvRegRxFrameTooLong = 0x2b4,
	NvRegRxOverflow = 0x2b8,
	NvRegRxFCSErr = 0x2bc,
	NvRegRxFrameAlignErr = 0x2c0,
	NvRegRxLenErr = 0x2c4,
	NvRegRxUnicast = 0x2c8,
	NvRegRxMulticast = 0x2cc,
	NvRegRxBroadcast = 0x2d0,
	NvRegTxDef = 0x2d4,
	NvRegTxFrame = 0x2d8,
	NvRegRxCnt = 0x2dc,
	NvRegTxPause = 0x2e0,
	NvRegRxPause = 0x2e4,
	NvRegRxDropFrame = 0x2e8,
	NvRegVlanControl = 0x300,
#define NVREG_VLANCONTROL_ENABLE	0x2000
	NvRegMSIXMap0 = 0x3e0,
	NvRegMSIXMap1 = 0x3e4,
	NvRegMSIXIrqStatus = 0x3f0,

	NvRegPowerState2 = 0x600,
#define NVREG_POWERSTATE2_POWERUP_MASK		0x0F15
#define NVREG_POWERSTATE2_POWERUP_REV_A3	0x0001
#define NVREG_POWERSTATE2_PHY_RESET		0x0004
#define NVREG_POWERSTATE2_GATE_CLOCKS		0x0F00
};

/* Big endian: should work, but is untested */
struct ring_desc {
	__le32 buf;
	__le32 flaglen;
};

struct ring_desc_ex {
	__le32 bufhigh;
	__le32 buflow;
	__le32 txvlan;
	__le32 flaglen;
};

union ring_type {
	struct ring_desc* orig;
	struct ring_desc_ex* ex;
};

#define FLAG_MASK_V1 0xffff0000
#define FLAG_MASK_V2 0xffffc000
#define LEN_MASK_V1 (0xffffffff ^ FLAG_MASK_V1)
#define LEN_MASK_V2 (0xffffffff ^ FLAG_MASK_V2)

#define NV_TX_LASTPACKET	(1<<16)
#define NV_TX_RETRYERROR	(1<<19)
#define NV_TX_RETRYCOUNT_MASK	(0xF<<20)
#define NV_TX_FORCED_INTERRUPT	(1<<24)
#define NV_TX_DEFERRED		(1<<26)
#define NV_TX_CARRIERLOST	(1<<27)
#define NV_TX_LATECOLLISION	(1<<28)
#define NV_TX_UNDERFLOW		(1<<29)
#define NV_TX_ERROR		(1<<30)
#define NV_TX_VALID		(1<<31)

#define NV_TX2_LASTPACKET	(1<<29)
#define NV_TX2_RETRYERROR	(1<<18)
#define NV_TX2_RETRYCOUNT_MASK	(0xF<<19)
#define NV_TX2_FORCED_INTERRUPT	(1<<30)
#define NV_TX2_DEFERRED		(1<<25)
#define NV_TX2_CARRIERLOST	(1<<26)
#define NV_TX2_LATECOLLISION	(1<<27)
#define NV_TX2_UNDERFLOW	(1<<28)
/* error and valid are the same for both */
#define NV_TX2_ERROR		(1<<30)
#define NV_TX2_VALID		(1<<31)
#define NV_TX2_TSO		(1<<28)
#define NV_TX2_TSO_SHIFT	14
#define NV_TX2_TSO_MAX_SHIFT	14
#define NV_TX2_TSO_MAX_SIZE	(1<<NV_TX2_TSO_MAX_SHIFT)
#define NV_TX2_CHECKSUM_L3	(1<<27)
#define NV_TX2_CHECKSUM_L4	(1<<26)

#define NV_TX3_VLAN_TAG_PRESENT (1<<18)

#define NV_RX_DESCRIPTORVALID	(1<<16)
#define NV_RX_MISSEDFRAME	(1<<17)
#define NV_RX_SUBSTRACT1	(1<<18)
#define NV_RX_ERROR1		(1<<23)
#define NV_RX_ERROR2		(1<<24)
#define NV_RX_ERROR3		(1<<25)
#define NV_RX_ERROR4		(1<<26)
#define NV_RX_CRCERR		(1<<27)
#define NV_RX_OVERFLOW		(1<<28)
#define NV_RX_FRAMINGERR	(1<<29)
#define NV_RX_ERROR		(1<<30)
#define NV_RX_AVAIL		(1<<31)
#define NV_RX_ERROR_MASK	(NV_RX_ERROR1|NV_RX_ERROR2|NV_RX_ERROR3|NV_RX_ERROR4|NV_RX_CRCERR|NV_RX_OVERFLOW|NV_RX_FRAMINGERR)

#define NV_RX2_CHECKSUMMASK	(0x1C000000)
#define NV_RX2_CHECKSUM_IP	(0x10000000)
#define NV_RX2_CHECKSUM_IP_TCP	(0x14000000)
#define NV_RX2_CHECKSUM_IP_UDP	(0x18000000)
#define NV_RX2_DESCRIPTORVALID	(1<<29)
#define NV_RX2_SUBSTRACT1	(1<<25)
#define NV_RX2_ERROR1		(1<<18)
#define NV_RX2_ERROR2		(1<<19)
#define NV_RX2_ERROR3		(1<<20)
#define NV_RX2_ERROR4		(1<<21)
#define NV_RX2_CRCERR		(1<<22)
#define NV_RX2_OVERFLOW		(1<<23)
#define NV_RX2_FRAMINGERR	(1<<24)
/* error and avail are the same for both */
#define NV_RX2_ERROR		(1<<30)
#define NV_RX2_AVAIL		(1<<31)
#define NV_RX2_ERROR_MASK	(NV_RX2_ERROR1|NV_RX2_ERROR2|NV_RX2_ERROR3|NV_RX2_ERROR4|NV_RX2_CRCERR|NV_RX2_OVERFLOW|NV_RX2_FRAMINGERR)

#define NV_RX3_VLAN_TAG_PRESENT (1<<16)
#define NV_RX3_VLAN_TAG_MASK	(0x0000FFFF)

/* Miscelaneous hardware related defines: */
#define NV_PCI_REGSZ_VER1      	0x270
#define NV_PCI_REGSZ_VER2      	0x2d4
#define NV_PCI_REGSZ_VER3      	0x604
#define NV_PCI_REGSZ_MAX       	0x604

/* various timeout delays: all in usec */
#define NV_TXRX_RESET_DELAY	4
#define NV_TXSTOP_DELAY1	10
#define NV_TXSTOP_DELAY1MAX	500000
#define NV_TXSTOP_DELAY2	100
#define NV_RXSTOP_DELAY1	10
#define NV_RXSTOP_DELAY1MAX	500000
#define NV_RXSTOP_DELAY2	100
#define NV_SETUP5_DELAY		5
#define NV_SETUP5_DELAYMAX	50000
#define NV_POWERUP_DELAY	5
#define NV_POWERUP_DELAYMAX	5000
#define NV_MIIBUSY_DELAY	50
#define NV_MIIPHY_DELAY	10
#define NV_MIIPHY_DELAYMAX	10000
#define NV_MAC_RESET_DELAY	64

#define NV_WAKEUPPATTERNS	5
#define NV_WAKEUPMASKENTRIES	4

/* General driver defaults */
#define NV_WATCHDOG_TIMEO	(5*HZ)

#define RX_RING_DEFAULT		512
#define TX_RING_DEFAULT		256
#define RX_RING_MIN		128
#define TX_RING_MIN		64
#define RING_MAX_DESC_VER_1	1024
#define RING_MAX_DESC_VER_2_3	16384

/* rx/tx mac addr + type + vlan + align + slack*/
#define NV_RX_HEADERS		(64)
/* even more slack. */
#define NV_RX_ALLOC_PAD		(64)

/* maximum mtu size */
#define NV_PKTLIMIT_1	ETH_DATA_LEN	/* hard limit not known */
#define NV_PKTLIMIT_2	9100	/* Actual limit according to NVidia: 9202 */

#define OOM_REFILL	(1+HZ/20)
#define POLL_WAIT	(1+HZ/100)
#define LINK_TIMEOUT	(3*HZ)
#define STATS_INTERVAL	(10*HZ)

/*
 * desc_ver values:
 * The nic supports three different descriptor types:
 * - DESC_VER_1: Original
 * - DESC_VER_2: support for jumbo frames.
 * - DESC_VER_3: 64-bit format.
 */
#define DESC_VER_1	1
#define DESC_VER_2	2
#define DESC_VER_3	3

/* PHY defines */
#define PHY_OUI_MARVELL		0x5043
#define PHY_OUI_CICADA		0x03f1
#define PHY_OUI_VITESSE		0x01c1
#define PHY_OUI_REALTEK		0x0732
#define PHY_OUI_REALTEK2	0x0020
#define PHYID1_OUI_MASK	0x03ff
#define PHYID1_OUI_SHFT	6
#define PHYID2_OUI_MASK	0xfc00
#define PHYID2_OUI_SHFT	10
#define PHYID2_MODEL_MASK		0x03f0
#define PHY_MODEL_REALTEK_8211		0x0110
#define PHY_REV_MASK			0x0001
#define PHY_REV_REALTEK_8211B		0x0000
#define PHY_REV_REALTEK_8211C		0x0001
#define PHY_MODEL_REALTEK_8201		0x0200
#define PHY_MODEL_MARVELL_E3016		0x0220
#define PHY_MARVELL_E3016_INITMASK	0x0300
#define PHY_CICADA_INIT1	0x0f000
#define PHY_CICADA_INIT2	0x0e00
#define PHY_CICADA_INIT3	0x01000
#define PHY_CICADA_INIT4	0x0200
#define PHY_CICADA_INIT5	0x0004
#define PHY_CICADA_INIT6	0x02000
#define PHY_VITESSE_INIT_REG1	0x1f
#define PHY_VITESSE_INIT_REG2	0x10
#define PHY_VITESSE_INIT_REG3	0x11
#define PHY_VITESSE_INIT_REG4	0x12
#define PHY_VITESSE_INIT_MSK1	0xc
#define PHY_VITESSE_INIT_MSK2	0x0180
#define PHY_VITESSE_INIT1	0x52b5
#define PHY_VITESSE_INIT2	0xaf8a
#define PHY_VITESSE_INIT3	0x8
#define PHY_VITESSE_INIT4	0x8f8a
#define PHY_VITESSE_INIT5	0xaf86
#define PHY_VITESSE_INIT6	0x8f86
#define PHY_VITESSE_INIT7	0xaf82
#define PHY_VITESSE_INIT8	0x0100
#define PHY_VITESSE_INIT9	0x8f82
#define PHY_VITESSE_INIT10	0x0
#define PHY_REALTEK_INIT_REG1	0x1f
#define PHY_REALTEK_INIT_REG2	0x19
#define PHY_REALTEK_INIT_REG3	0x13
#define PHY_REALTEK_INIT_REG4	0x14
#define PHY_REALTEK_INIT_REG5	0x18
#define PHY_REALTEK_INIT_REG6	0x11
#define PHY_REALTEK_INIT_REG7	0x01
#define PHY_REALTEK_INIT1	0x0000
#define PHY_REALTEK_INIT2	0x8e00
#define PHY_REALTEK_INIT3	0x0001
#define PHY_REALTEK_INIT4	0xad17
#define PHY_REALTEK_INIT5	0xfb54
#define PHY_REALTEK_INIT6	0xf5c7
#define PHY_REALTEK_INIT7	0x1000
#define PHY_REALTEK_INIT8	0x0003
#define PHY_REALTEK_INIT9	0x0008
#define PHY_REALTEK_INIT10	0x0005
#define PHY_REALTEK_INIT11	0x0200
#define PHY_REALTEK_INIT_MSK1	0x0003

#define PHY_GIGABIT	0x0100

#define PHY_TIMEOUT	0x1
#define PHY_ERROR	0x2

#define PHY_100	0x1
#define PHY_1000	0x2
#define PHY_HALF	0x100

#define NV_PAUSEFRAME_RX_CAPABLE 0x0001
#define NV_PAUSEFRAME_TX_CAPABLE 0x0002
#define NV_PAUSEFRAME_RX_ENABLE  0x0004
#define NV_PAUSEFRAME_TX_ENABLE  0x0008
#define NV_PAUSEFRAME_RX_REQ     0x0010
#define NV_PAUSEFRAME_TX_REQ     0x0020
#define NV_PAUSEFRAME_AUTONEG    0x0040

/* MSI/MSI-X defines */
#define NV_MSI_X_MAX_VECTORS  8
#define NV_MSI_X_VECTORS_MASK 0x000f
#define NV_MSI_CAPABLE        0x0010
#define NV_MSI_X_CAPABLE      0x0020
#define NV_MSI_ENABLED        0x0040
#define NV_MSI_X_ENABLED      0x0080

#define NV_MSI_X_VECTOR_ALL   0x0
#define NV_MSI_X_VECTOR_RX    0x0
#define NV_MSI_X_VECTOR_TX    0x1
#define NV_MSI_X_VECTOR_OTHER 0x2

#define NV_MSI_PRIV_OFFSET 0x68
#define NV_MSI_PRIV_VALUE  0xffffffff

#define NV_RESTART_TX         0x1
#define NV_RESTART_RX         0x2

#define NV_TX_LIMIT_COUNT     16

#define NV_DYNAMIC_THRESHOLD        4
#define NV_DYNAMIC_MAX_QUIET_COUNT  2048

/* statistics */
struct nv_ethtool_str {
	char name[ETH_GSTRING_LEN];
};

static const struct nv_ethtool_str nv_estats_str[] = {
	{ "tx_bytes" },
	{ "tx_zero_rexmt" },
	{ "tx_one_rexmt" },
	{ "tx_many_rexmt" },
	{ "tx_late_collision" },
	{ "tx_fifo_errors" },
	{ "tx_carrier_errors" },
	{ "tx_excess_deferral" },
	{ "tx_retry_error" },
	{ "rx_frame_error" },
	{ "rx_extra_byte" },
	{ "rx_late_collision" },
	{ "rx_runt" },
	{ "rx_frame_too_long" },
	{ "rx_over_errors" },
	{ "rx_crc_errors" },
	{ "rx_frame_align_error" },
	{ "rx_length_error" },
	{ "rx_unicast" },
	{ "rx_multicast" },
	{ "rx_broadcast" },
	{ "rx_packets" },
	{ "rx_errors_total" },
	{ "tx_errors_total" },

	/* version 2 stats */
	{ "tx_deferral" },
	{ "tx_packets" },
	{ "rx_bytes" },
	{ "tx_pause" },
	{ "rx_pause" },
	{ "rx_drop_frame" },

	/* version 3 stats */
	{ "tx_unicast" },
	{ "tx_multicast" },
	{ "tx_broadcast" }
};

struct nv_ethtool_stats {
	u64 tx_bytes;
	u64 tx_zero_rexmt;
	u64 tx_one_rexmt;
	u64 tx_many_rexmt;
	u64 tx_late_collision;
	u64 tx_fifo_errors;
	u64 tx_carrier_errors;
	u64 tx_excess_deferral;
	u64 tx_retry_error;
	u64 rx_frame_error;
	u64 rx_extra_byte;
	u64 rx_late_collision;
	u64 rx_runt;
	u64 rx_frame_too_long;
	u64 rx_over_errors;
	u64 rx_crc_errors;
	u64 rx_frame_align_error;
	u64 rx_length_error;
	u64 rx_unicast;
	u64 rx_multicast;
	u64 rx_broadcast;
	u64 rx_packets;
	u64 rx_errors_total;
	u64 tx_errors_total;

	/* version 2 stats */
	u64 tx_deferral;
	u64 tx_packets;
	u64 rx_bytes;
	u64 tx_pause;
	u64 rx_pause;
	u64 rx_drop_frame;

	/* version 3 stats */
	u64 tx_unicast;
	u64 tx_multicast;
	u64 tx_broadcast;
};

#define NV_DEV_STATISTICS_V3_COUNT (sizeof(struct nv_ethtool_stats)/sizeof(u64))
#define NV_DEV_STATISTICS_V2_COUNT (NV_DEV_STATISTICS_V3_COUNT - 3)
#define NV_DEV_STATISTICS_V1_COUNT (NV_DEV_STATISTICS_V2_COUNT - 6)

/* diagnostics */
#define NV_TEST_COUNT_BASE 3
#define NV_TEST_COUNT_EXTENDED 4

static const struct nv_ethtool_str nv_etests_str[] = {
	{ "link      (online/offline)" },
	{ "register  (offline)       " },
	{ "interrupt (offline)       " },
	{ "loopback  (offline)       " }
};

struct register_test {
	__u32 reg;
	__u32 mask;
};

static const struct register_test nv_registers_test[] = {
	{ NvRegUnknownSetupReg6, 0x01 },
	{ NvRegMisc1, 0x03c },
	{ NvRegOffloadConfig, 0x03ff },
	{ NvRegMulticastAddrA, 0xffffffff },
	{ NvRegTxWatermark, 0x0ff },
	{ NvRegWakeUpFlags, 0x07777 },
	{ 0,0 }
};

struct nv_skb_map {
	struct sk_buff *skb;
	dma_addr_t dma;
	unsigned int dma_len:31;
	unsigned int dma_single:1;
	struct ring_desc_ex *first_tx_desc;
	struct nv_skb_map *next_tx_ctx;
};

/*
 * SMP locking:
 * All hardware access under netdev_priv(dev)->lock, except the performance
 * critical parts:
 * - rx is (pseudo-) lockless: it relies on the single-threading provided
 *	by the arch code for interrupts.
 * - tx setup is lockless: it relies on netif_tx_lock. Actual submission
 *	needs netdev_priv(dev)->lock :-(
 * - set_multicast_list: preparation lockless, relies on netif_tx_lock.
 */

/* in dev: base, irq */
struct fe_priv {
	spinlock_t lock;

	struct net_device *dev;
	struct napi_struct napi;

	/* General data:
	 * Locking: spin_lock(&np->lock); */
	struct nv_ethtool_stats estats;
	int in_shutdown;
	u32 linkspeed;
	int duplex;
	int autoneg;
	int fixed_mode;
	int phyaddr;
	int wolenabled;
	unsigned int phy_oui;
	unsigned int phy_model;
	unsigned int phy_rev;
	u16 gigabit;
	int intr_test;
	int recover_error;
	int quiet_count;

	/* General data: RO fields */
	dma_addr_t ring_addr;
	struct pci_dev *pci_dev;
	u32 orig_mac[2];
	u32 events;
	u32 irqmask;
	u32 desc_ver;
	u32 txrxctl_bits;
	u32 vlanctl_bits;
	u32 driver_data;
	u32 device_id;
	u32 register_size;
	int rx_csum;
	u32 mac_in_use;
	int mgmt_version;
	int mgmt_sema;

	void __iomem *base;

	/* rx specific fields.
	 * Locking: Within irq hander or disable_irq+spin_lock(&np->lock);
	 */
	union ring_type get_rx, put_rx, first_rx, last_rx;
	struct nv_skb_map *get_rx_ctx, *put_rx_ctx;
	struct nv_skb_map *first_rx_ctx, *last_rx_ctx;
	struct nv_skb_map *rx_skb;

	union ring_type rx_ring;
	unsigned int rx_buf_sz;
	unsigned int pkt_limit;
	struct timer_list oom_kick;
	struct timer_list nic_poll;
	struct timer_list stats_poll;
	u32 nic_poll_irq;
	int rx_ring_size;

	/* media detection workaround.
	 * Locking: Within irq hander or disable_irq+spin_lock(&np->lock);
	 */
	int need_linktimer;
	unsigned long link_timeout;
	/*
	 * tx specific fields.
	 */
	union ring_type get_tx, put_tx, first_tx, last_tx;
	struct nv_skb_map *get_tx_ctx, *put_tx_ctx;
	struct nv_skb_map *first_tx_ctx, *last_tx_ctx;
	struct nv_skb_map *tx_skb;

	union ring_type tx_ring;
	u32 tx_flags;
	int tx_ring_size;
	int tx_limit;
	u32 tx_pkts_in_progress;
	struct nv_skb_map *tx_change_owner;
	struct nv_skb_map *tx_end_flip;
	int tx_stop;

	/* vlan fields */
	struct vlan_group *vlangrp;

	/* msi/msi-x fields */
	u32 msi_flags;
	struct msix_entry msi_x_entry[NV_MSI_X_MAX_VECTORS];

	/* flow control */
	u32 pause_flags;

	/* power saved state */
	u32 saved_config_space[NV_PCI_REGSZ_MAX/4];

	/* for different msi-x irq type */
	char name_rx[IFNAMSIZ + 3];       /* -rx    */
	char name_tx[IFNAMSIZ + 3];       /* -tx    */
	char name_other[IFNAMSIZ + 6];    /* -other */
};

/*
 * Maximum number of loops until we assume that a bit in the irq mask
 * is stuck. Overridable with module param.
 */
static int max_interrupt_work = 4;

/*
 * Optimization can be either throuput mode or cpu mode
 *
 * Throughput Mode: Every tx and rx packet will generate an interrupt.
 * CPU Mode: Interrupts are controlled by a timer.
 */
enum {
	NV_OPTIMIZATION_MODE_THROUGHPUT,
	NV_OPTIMIZATION_MODE_CPU,
	NV_OPTIMIZATION_MODE_DYNAMIC
};
static int optimization_mode = NV_OPTIMIZATION_MODE_DYNAMIC;

/*
 * Poll interval for timer irq
 *
 * This interval determines how frequent an interrupt is generated.
 * The is value is determined by [(time_in_micro_secs * 100) / (2^10)]
 * Min = 0, and Max = 65535
 */
static int poll_interval = -1;

/*
 * MSI interrupts
 */
enum {
	NV_MSI_INT_DISABLED,
	NV_MSI_INT_ENABLED
};
static int msi = NV_MSI_INT_ENABLED;

/*
 * MSIX interrupts
 */
enum {
	NV_MSIX_INT_DISABLED,
	NV_MSIX_INT_ENABLED
};
static int msix = NV_MSIX_INT_ENABLED;

/*
 * DMA 64bit
 */
enum {
	NV_DMA_64BIT_DISABLED,
	NV_DMA_64BIT_ENABLED
};
static int dma_64bit = NV_DMA_64BIT_ENABLED;

/*
 * Crossover Detection
 * Realtek 8201 phy + some OEM boards do not work properly.
 */
enum {
	NV_CROSSOVER_DETECTION_DISABLED,
	NV_CROSSOVER_DETECTION_ENABLED
};
static int phy_cross = NV_CROSSOVER_DETECTION_DISABLED;

/*
 * Power down phy when interface is down (persists through reboot;
 * older Linux and other OSes may not power it up again)
 */
static int phy_power_down = 0;

static inline struct fe_priv *get_nvpriv(struct net_device *dev)
{
	return netdev_priv(dev);
}

static inline u8 __iomem *get_hwbase(struct net_device *dev)
{
	return ((struct fe_priv *)netdev_priv(dev))->base;
}

static inline void pci_push(u8 __iomem *base)
{
	/* force out pending posted writes */
	readl(base);
}

static inline u32 nv_descr_getlength(struct ring_desc *prd, u32 v)
{
	return le32_to_cpu(prd->flaglen)
		& ((v == DESC_VER_1) ? LEN_MASK_V1 : LEN_MASK_V2);
}

static inline u32 nv_descr_getlength_ex(struct ring_desc_ex *prd, u32 v)
{
	return le32_to_cpu(prd->flaglen) & LEN_MASK_V2;
}

static bool nv_optimized(struct fe_priv *np)
{
	if (np->desc_ver == DESC_VER_1 || np->desc_ver == DESC_VER_2)
		return false;
	return true;
}

static int reg_delay(struct net_device *dev, int offset, u32 mask, u32 target,
				int delay, int delaymax, const char *msg)
{
	u8 __iomem *base = get_hwbase(dev);

	pci_push(base);
	do {
		udelay(delay);
		delaymax -= delay;
		if (delaymax < 0) {
			if (msg)
				printk("%s", msg);
			return 1;
		}
	} while ((readl(base + offset) & mask) != target);
	return 0;
}

#define NV_SETUP_RX_RING 0x01
#define NV_SETUP_TX_RING 0x02

static inline u32 dma_low(dma_addr_t addr)
{
	return addr;
}

static inline u32 dma_high(dma_addr_t addr)
{
	return addr>>31>>1;	/* 0 if 32bit, shift down by 32 if 64bit */
}

static void setup_hw_rings(struct net_device *dev, int rxtx_flags)
{
	struct fe_priv *np = get_nvpriv(dev);
	u8 __iomem *base = get_hwbase(dev);

	if (!nv_optimized(np)) {
		if (rxtx_flags & NV_SETUP_RX_RING) {
			writel(dma_low(np->ring_addr), base + NvRegRxRingPhysAddr);
		}
		if (rxtx_flags & NV_SETUP_TX_RING) {
			writel(dma_low(np->ring_addr + np->rx_ring_size*sizeof(struct ring_desc)), base + NvRegTxRingPhysAddr);
		}
	} else {
		if (rxtx_flags & NV_SETUP_RX_RING) {
			writel(dma_low(np->ring_addr), base + NvRegRxRingPhysAddr);
			writel(dma_high(np->ring_addr), base + NvRegRxRingPhysAddrHigh);
		}
		if (rxtx_flags & NV_SETUP_TX_RING) {
			writel(dma_low(np->ring_addr + np->rx_ring_size*sizeof(struct ring_desc_ex)), base + NvRegTxRingPhysAddr);
			writel(dma_high(np->ring_addr + np->rx_ring_size*sizeof(struct ring_desc_ex)), base + NvRegTxRingPhysAddrHigh);
		}
	}
}

static void free_rings(struct net_device *dev)
{
	struct fe_priv *np = get_nvpriv(dev);

	if (!nv_optimized(np)) {
		if (np->rx_ring.orig)
			pci_free_consistent(np->pci_dev, sizeof(struct ring_desc) * (np->rx_ring_size + np->tx_ring_size),
					    np->rx_ring.orig, np->ring_addr);
	} else {
		if (np->rx_ring.ex)
			pci_free_consistent(np->pci_dev, sizeof(struct ring_desc_ex) * (np->rx_ring_size + np->tx_ring_size),
					    np->rx_ring.ex, np->ring_addr);
	}
	if (np->rx_skb)
		kfree(np->rx_skb);
	if (np->tx_skb)
		kfree(np->tx_skb);
}

static int using_multi_irqs(struct net_device *dev)
{
	struct fe_priv *np = get_nvpriv(dev);

	if (!(np->msi_flags & NV_MSI_X_ENABLED) ||
	    ((np->msi_flags & NV_MSI_X_ENABLED) &&
	     ((np->msi_flags & NV_MSI_X_VECTORS_MASK) == 0x1)))
		return 0;
	else
		return 1;
}

static void nv_txrx_gate(struct net_device *dev, bool gate)
{
	struct fe_priv *np = get_nvpriv(dev);
	u8 __iomem *base = get_hwbase(dev);
	u32 powerstate;

	if (!np->mac_in_use &&
	    (np->driver_data & DEV_HAS_POWER_CNTRL)) {
		powerstate = readl(base + NvRegPowerState2);
		if (gate)
			powerstate |= NVREG_POWERSTATE2_GATE_CLOCKS;
		else
			powerstate &= ~NVREG_POWERSTATE2_GATE_CLOCKS;
		writel(powerstate, base + NvRegPowerState2);
	}
}

static void nv_enable_irq(struct net_device *dev)
{
	struct fe_priv *np = get_nvpriv(dev);

	if (!using_multi_irqs(dev)) {
		if (np->msi_flags & NV_MSI_X_ENABLED)
			enable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_ALL].vector);
		else
			enable_irq(np->pci_dev->irq);
	} else {
		enable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_RX].vector);
		enable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_TX].vector);
		enable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_OTHER].vector);
	}
}

static void nv_disable_irq(struct net_device *dev)
{
	struct fe_priv *np = get_nvpriv(dev);

	if (!using_multi_irqs(dev)) {
		if (np->msi_flags & NV_MSI_X_ENABLED)
			disable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_ALL].vector);
		else
			disable_irq(np->pci_dev->irq);
	} else {
		disable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_RX].vector);
		disable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_TX].vector);
		disable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_OTHER].vector);
	}
}

/* In MSIX mode, a write to irqmask behaves as XOR */
static void nv_enable_hw_interrupts(struct net_device *dev, u32 mask)
{
	u8 __iomem *base = get_hwbase(dev);

	writel(mask, base + NvRegIrqMask);
}

static void nv_disable_hw_interrupts(struct net_device *dev, u32 mask)
{
	struct fe_priv *np = get_nvpriv(dev);
	u8 __iomem *base = get_hwbase(dev);

	if (np->msi_flags & NV_MSI_X_ENABLED) {
		writel(mask, base + NvRegIrqMask);
	} else {
		if (np->msi_flags & NV_MSI_ENABLED)
			writel(0, base + NvRegMSIIrqMask);
		writel(0, base + NvRegIrqMask);
	}
}

static void nv_napi_enable(struct net_device *dev)
{
#ifdef CONFIG_FORCEDETH_NAPI
	struct fe_priv *np = get_nvpriv(dev);

	napi_enable(&np->napi);
#endif
}

static void nv_napi_disable(struct net_device *dev)
{
#ifdef CONFIG_FORCEDETH_NAPI
	struct fe_priv *np = get_nvpriv(dev);

	napi_disable(&np->napi);
#endif
}

#define MII_READ	(-1)
/* mii_rw: read/write a register on the PHY.
 *
 * Caller must guarantee serialization
 */
static int mii_rw(struct net_device *dev, int addr, int miireg, int value)
{
	u8 __iomem *base = get_hwbase(dev);
	u32 reg;
	int retval;

	writel(NVREG_MIISTAT_MASK_RW, base + NvRegMIIStatus);

	reg = readl(base + NvRegMIIControl);
	if (reg & NVREG_MIICTL_INUSE) {
		writel(NVREG_MIICTL_INUSE, base + NvRegMIIControl);
		udelay(NV_MIIBUSY_DELAY);
	}

	reg = (addr << NVREG_MIICTL_ADDRSHIFT) | miireg;
	if (value != MII_READ) {
		writel(value, base + NvRegMIIData);
		reg |= NVREG_MIICTL_WRITE;
	}
	writel(reg, base + NvRegMIIControl);

	if (reg_delay(dev, NvRegMIIControl, NVREG_MIICTL_INUSE, 0,
			NV_MIIPHY_DELAY, NV_MIIPHY_DELAYMAX, NULL)) {
		dprintk(KERN_DEBUG "%s: mii_rw of reg %d at PHY %d timed out.\n",
				dev->name, miireg, addr);
		retval = -1;
	} else if (value != MII_READ) {
		/* it was a write operation - fewer failures are detectable */
		dprintk(KERN_DEBUG "%s: mii_rw wrote 0x%x to reg %d at PHY %d\n",
				dev->name, value, miireg, addr);
		retval = 0;
	} else if (readl(base + NvRegMIIStatus) & NVREG_MIISTAT_ERROR) {
		dprintk(KERN_DEBUG "%s: mii_rw of reg %d at PHY %d failed.\n",
				dev->name, miireg, addr);
		retval = -1;
	} else {
		retval = readl(base + NvRegMIIData);
		dprintk(KERN_DEBUG "%s: mii_rw read from reg %d at PHY %d: 0x%x.\n",
				dev->name, miireg, addr, retval);
	}

	return retval;
}

static int phy_reset(struct net_device *dev, u32 bmcr_setup)
{
	struct fe_priv *np = netdev_priv(dev);
	u32 miicontrol;
	unsigned int tries = 0;

	miicontrol = BMCR_RESET | bmcr_setup;
	if (mii_rw(dev, np->phyaddr, MII_BMCR, miicontrol)) {
		return -1;
	}

	/* wait for 500ms */
	msleep(500);

	/* must wait till reset is deasserted */
	while (miicontrol & BMCR_RESET) {
		msleep(10);
		miicontrol = mii_rw(dev, np->phyaddr, MII_BMCR, MII_READ);
		/* FIXME: 100 tries seem excessive */
		if (tries++ > 100)
			return -1;
	}
	return 0;
}

static int phy_init(struct net_device *dev)
{
	struct fe_priv *np = get_nvpriv(dev);
	u8 __iomem *base = get_hwbase(dev);
	u32 phyinterface, phy_reserved, mii_status, mii_control, mii_control_1000,reg;

	/* phy errata for E3016 phy */
	if (np->phy_model == PHY_MODEL_MARVELL_E3016) {
		reg = mii_rw(dev, np->phyaddr, MII_NCONFIG, MII_READ);
		reg &= ~PHY_MARVELL_E3016_INITMASK;
		if (mii_rw(dev, np->phyaddr, MII_NCONFIG, reg)) {
			printk(KERN_INFO "%s: phy write to errata reg failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
	}
	if (np->phy_oui == PHY_OUI_REALTEK) {
		if (np->phy_model == PHY_MODEL_REALTEK_8211 &&
		    np->phy_rev == PHY_REV_REALTEK_8211B) {
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG1, PHY_REALTEK_INIT1)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG2, PHY_REALTEK_INIT2)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG1, PHY_REALTEK_INIT3)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG3, PHY_REALTEK_INIT4)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG4, PHY_REALTEK_INIT5)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG5, PHY_REALTEK_INIT6)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG1, PHY_REALTEK_INIT1)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
		}
		if (np->phy_model == PHY_MODEL_REALTEK_8211 &&
		    np->phy_rev == PHY_REV_REALTEK_8211C) {
			u32 powerstate = readl(base + NvRegPowerState2);

			/* need to perform hw phy reset */
			powerstate |= NVREG_POWERSTATE2_PHY_RESET;
			writel(powerstate, base + NvRegPowerState2);
			msleep(25);

			powerstate &= ~NVREG_POWERSTATE2_PHY_RESET;
			writel(powerstate, base + NvRegPowerState2);
			msleep(25);

			reg = mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG6, MII_READ);
			reg |= PHY_REALTEK_INIT9;
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG6, reg)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG1, PHY_REALTEK_INIT10)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			reg = mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG7, MII_READ);
			if (!(reg & PHY_REALTEK_INIT11)) {
				reg |= PHY_REALTEK_INIT11;
				if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG7, reg)) {
					printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
					return PHY_ERROR;
				}
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG1, PHY_REALTEK_INIT1)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
		}
		if (np->phy_model == PHY_MODEL_REALTEK_8201) {
			if (np->driver_data & DEV_NEED_PHY_INIT_FIX) {
				phy_reserved = mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG6, MII_READ);
				phy_reserved |= PHY_REALTEK_INIT7;
				if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG6, phy_reserved)) {
					printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
					return PHY_ERROR;
				}
			}
		}
	}

	/* set advertise register */
	reg = mii_rw(dev, np->phyaddr, MII_ADVERTISE, MII_READ);
	reg |= (ADVERTISE_10HALF|ADVERTISE_10FULL|ADVERTISE_100HALF|ADVERTISE_100FULL|ADVERTISE_PAUSE_ASYM|ADVERTISE_PAUSE_CAP);
	if (mii_rw(dev, np->phyaddr, MII_ADVERTISE, reg)) {
		printk(KERN_INFO "%s: phy write to advertise failed.\n", pci_name(np->pci_dev));
		return PHY_ERROR;
	}

	/* get phy interface type */
	phyinterface = readl(base + NvRegPhyInterface);

	/* see if gigabit phy */
	mii_status = mii_rw(dev, np->phyaddr, MII_BMSR, MII_READ);
	if (mii_status & PHY_GIGABIT) {
		np->gigabit = PHY_GIGABIT;
		mii_control_1000 = mii_rw(dev, np->phyaddr, MII_CTRL1000, MII_READ);
		mii_control_1000 &= ~ADVERTISE_1000HALF;
		if (phyinterface & PHY_RGMII)
			mii_control_1000 |= ADVERTISE_1000FULL;
		else
			mii_control_1000 &= ~ADVERTISE_1000FULL;

		if (mii_rw(dev, np->phyaddr, MII_CTRL1000, mii_control_1000)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
	}
	else
		np->gigabit = 0;

	mii_control = mii_rw(dev, np->phyaddr, MII_BMCR, MII_READ);
	mii_control |= BMCR_ANENABLE;

	if (np->phy_oui == PHY_OUI_REALTEK &&
	    np->phy_model == PHY_MODEL_REALTEK_8211 &&
	    np->phy_rev == PHY_REV_REALTEK_8211C) {
		/* start autoneg since we already performed hw reset above */
		mii_control |= BMCR_ANRESTART;
		if (mii_rw(dev, np->phyaddr, MII_BMCR, mii_control)) {
			printk(KERN_INFO "%s: phy init failed\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
	} else {
		/* reset the phy
		 * (certain phys need bmcr to be setup with reset)
		 */
		if (phy_reset(dev, mii_control)) {
			printk(KERN_INFO "%s: phy reset failed\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
	}

	/* phy vendor specific configuration */
	if ((np->phy_oui == PHY_OUI_CICADA) && (phyinterface & PHY_RGMII) ) {
		phy_reserved = mii_rw(dev, np->phyaddr, MII_RESV1, MII_READ);
		phy_reserved &= ~(PHY_CICADA_INIT1 | PHY_CICADA_INIT2);
		phy_reserved |= (PHY_CICADA_INIT3 | PHY_CICADA_INIT4);
		if (mii_rw(dev, np->phyaddr, MII_RESV1, phy_reserved)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		phy_reserved = mii_rw(dev, np->phyaddr, MII_NCONFIG, MII_READ);
		phy_reserved |= PHY_CICADA_INIT5;
		if (mii_rw(dev, np->phyaddr, MII_NCONFIG, phy_reserved)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
	}
	if (np->phy_oui == PHY_OUI_CICADA) {
		phy_reserved = mii_rw(dev, np->phyaddr, MII_SREVISION, MII_READ);
		phy_reserved |= PHY_CICADA_INIT6;
		if (mii_rw(dev, np->phyaddr, MII_SREVISION, phy_reserved)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
	}
	if (np->phy_oui == PHY_OUI_VITESSE) {
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG1, PHY_VITESSE_INIT1)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG2, PHY_VITESSE_INIT2)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		phy_reserved = mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG4, MII_READ);
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG4, phy_reserved)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		phy_reserved = mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG3, MII_READ);
		phy_reserved &= ~PHY_VITESSE_INIT_MSK1;
		phy_reserved |= PHY_VITESSE_INIT3;
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG3, phy_reserved)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG2, PHY_VITESSE_INIT4)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG2, PHY_VITESSE_INIT5)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		phy_reserved = mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG4, MII_READ);
		phy_reserved &= ~PHY_VITESSE_INIT_MSK1;
		phy_reserved |= PHY_VITESSE_INIT3;
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG4, phy_reserved)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		phy_reserved = mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG3, MII_READ);
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG3, phy_reserved)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG2, PHY_VITESSE_INIT6)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG2, PHY_VITESSE_INIT7)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		phy_reserved = mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG4, MII_READ);
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG4, phy_reserved)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		phy_reserved = mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG3, MII_READ);
		phy_reserved &= ~PHY_VITESSE_INIT_MSK2;
		phy_reserved |= PHY_VITESSE_INIT8;
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG3, phy_reserved)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG2, PHY_VITESSE_INIT9)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REG1, PHY_VITESSE_INIT10)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}
	}
	if (np->phy_oui == PHY_OUI_REALTEK) {
		if (np->phy_model == PHY_MODEL_REALTEK_8211 &&
		    np->phy_rev == PHY_REV_REALTEK_8211B) {
			/* reset could have cleared these out, set them back */
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG1, PHY_REALTEK_INIT1)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG2, PHY_REALTEK_INIT2)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG1, PHY_REALTEK_INIT3)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG3, PHY_REALTEK_INIT4)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG4, PHY_REALTEK_INIT5)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG5, PHY_REALTEK_INIT6)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG1, PHY_REALTEK_INIT1)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
		}
		if (np->phy_model == PHY_MODEL_REALTEK_8201) {
			if (np->driver_data & DEV_NEED_PHY_INIT_FIX) {
				phy_reserved = mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG6, MII_READ);
				phy_reserved |= PHY_REALTEK_INIT7;
				if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG6, phy_reserved)) {
					printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
					return PHY_ERROR;
				}
			}
			if (phy_cross == NV_CROSSOVER_DETECTION_DISABLED) {
				if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG1, PHY_REALTEK_INIT3)) {
					printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
					return PHY_ERROR;
				}
				phy_reserved = mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG2, MII_READ);
				phy_reserved &= ~PHY_REALTEK_INIT_MSK1;
				phy_reserved |= PHY_REALTEK_INIT3;
				if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG2, phy_reserved)) {
					printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
					return PHY_ERROR;
				}
				if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG1, PHY_REALTEK_INIT1)) {
					printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
					return PHY_ERROR;
				}
			}
		}
	}

	/* some phys clear out pause advertisment on reset, set it back */
	mii_rw(dev, np->phyaddr, MII_ADVERTISE, reg);

	/* restart auto negotiation, power down phy */
	mii_control = mii_rw(dev, np->phyaddr, MII_BMCR, MII_READ);
	mii_control |= (BMCR_ANRESTART | BMCR_ANENABLE);
	if (phy_power_down) {
		mii_control |= BMCR_PDOWN;
	}
	if (mii_rw(dev, np->phyaddr, MII_BMCR, mii_control)) {
		return PHY_ERROR;
	}

	return 0;
}

static void nv_start_rx(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	u8 __iomem *base = get_hwbase(dev);
	u32 rx_ctrl = readl(base + NvRegReceiverControl);

	dprintk(KERN_DEBUG "%s: nv_start_rx\n", dev->name);
	/* Already running? Stop it. */
	if ((readl(base + NvRegReceiverControl) & NVREG_RCVCTL_START) && !np->mac_in_use) {
		rx_ctrl &= ~NVREG_RCVCTL_START;
		writel(rx_ctrl, base + NvRegReceiverControl);
		pci_push(base);
	}
	writel(np->linkspeed, base + NvRegLinkSpeed);
	pci_push(base);
        rx_ctrl |= NVREG_RCVCTL_START;
        if (np->mac_in_use)
		rx_ctrl &= ~NVREG_RCVCTL_RX_PATH_EN;
	writel(rx_ctrl, base + NvRegReceiverControl);
	dprintk(KERN_DEBUG "%s: nv_start_rx to duplex %d, speed 0x%08x.\n",
				dev->name, np->duplex, np->linkspeed);
	pci_push(base);
}

static void nv_stop_rx(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	u8 __iomem *base = get_hwbase(dev);
	u32 rx_ctrl = readl(base + NvRegReceiverControl);

	dprintk(KERN_DEBUG "%s: nv_stop_rx\n", dev->name);
	if (!np->mac_in_use)
		rx_ctrl &= ~NVREG_RCVCTL_START;
	else
		rx_ctrl |= NVREG_RCVCTL_RX_PATH_EN;
	writel(rx_ctrl, base + NvRegReceiverControl);
	reg_delay(dev, NvRegReceiverStatus, NVREG_RCVSTAT_BUSY, 0,
			NV_RXSTOP_DELAY1, NV_RXSTOP_DELAY1MAX,
			KERN_INFO "nv_stop_rx: ReceiverStatus remained busy");

	udelay(NV_RXSTOP_DELAY2);
	if (!np->mac_in_use)
		writel(0, base + NvRegLinkSpeed);
}

static void nv_start_tx(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	u8 __iomem *base = get_hwbase(dev);
	u32 tx_ctrl = readl(base + NvRegTransmitterControl);

	dprintk(KERN_DEBUG "%s: nv_start_tx\n", dev->name);
	tx_ctrl |= NVREG_XMITCTL_START;
	if (np->mac_in_use)
		tx_ctrl &= ~NVREG_XMITCTL_TX_PATH_EN;
	writel(tx_ctrl, base + NvRegTransmitterControl);
	pci_push(base);
}

static void nv_stop_tx(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	u8 __iomem *base = get_hwbase(dev);
	u32 tx_ctrl = readl(base + NvRegTransmitterControl);

	dprintk(KERN_DEBUG "%s: nv_stop_tx\n", dev->name);
	if (!np->mac_in_use)
		tx_ctrl &= ~NVREG_XMITCTL_START;
	else
		tx_ctrl |= NVREG_XMITCTL_TX_PATH_EN;
	writel(tx_ctrl, base + NvRegTransmitterControl);
	reg_delay(dev, NvRegTransmitterStatus, NVREG_XMITSTAT_BUSY, 0,
			NV_TXSTOP_DELAY1, NV_TXSTOP_DELAY1MAX,
			KERN_INFO "nv_stop_tx: TransmitterStatus remained busy");

	udelay(NV_TXSTOP_DELAY2);
	if (!np->mac_in_use)
		writel(readl(base + NvRegTransmitPoll) & NVREG_TRANSMITPOLL_MAC_ADDR_REV,
		       base + NvRegTransmitPoll);
}

static void nv_start_rxtx(struct net_device *dev)
{
	nv_start_rx(dev);
	nv_start_tx(dev);
}

static void nv_stop_rxtx(struct net_device *dev)
{
	nv_stop_rx(dev);
	nv_stop_tx(dev);
}

static void nv_txrx_reset(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	u8 __iomem *base = get_hwbase(dev);

	dprintk(KERN_DEBUG "%s: nv_txrx_reset\n", dev->name);
	writel(NVREG_TXRXCTL_BIT2 | NVREG_TXRXCTL_RESET | np->txrxctl_bits, base + NvRegTxRxControl);
	pci_push(base);
	udelay(NV_TXRX_RESET_DELAY);
	writel(NVREG_TXRXCTL_BIT2 | np->txrxctl_bits, base + NvRegTxRxControl);
	pci_push(base);
}

static void nv_mac_reset(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	u8 __iomem *base = get_hwbase(dev);
	u32 temp1, temp2, temp3;

	dprintk(KERN_DEBUG "%s: nv_mac_reset\n", dev->name);

	writel(NVREG_TXRXCTL_BIT2 | NVREG_TXRXCTL_RESET | np->txrxctl_bits, base + NvRegTxRxControl);
	pci_push(base);

	/* save registers since they will be cleared on reset */
	temp1 = readl(base + NvRegMacAddrA);
	temp2 = readl(base + NvRegMacAddrB);
	temp3 = readl(base + NvRegTransmitPoll);

	writel(NVREG_MAC_RESET_ASSERT, base + NvRegMacReset);
	pci_push(base);
	udelay(NV_MAC_RESET_DELAY);
	writel(0, base + NvRegMacReset);
	pci_push(base);
	udelay(NV_MAC_RESET_DELAY);

	/* restore saved registers */
	writel(temp1, base + NvRegMacAddrA);
	writel(temp2, base + NvRegMacAddrB);
	writel(temp3, base + NvRegTransmitPoll);

	writel(NVREG_TXRXCTL_BIT2 | np->txrxctl_bits, base + NvRegTxRxControl);
	pci_push(base);
}

static void nv_get_hw_stats(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	u8 __iomem *base = get_hwbase(dev);

	np->estats.tx_bytes += readl(base + NvRegTxCnt);
	np->estats.tx_zero_rexmt += readl(base + NvRegTxZeroReXmt);
	np->estats.tx_one_rexmt += readl(base + NvRegTxOneReXmt);
	np->estats.tx_many_rexmt += readl(base + NvRegTxManyReXmt);
	np->estats.tx_late_collision += readl(base + NvRegTxLateCol);
	np->estats.tx_fifo_errors += readl(base + NvRegTxUnderflow);
	np->estats.tx_carrier_errors += readl(base + NvRegTxLossCarrier);
	np->estats.tx_excess_deferral += readl(base + NvRegTxExcessDef);
	np->estats.tx_retry_error += readl(base + NvRegTxRetryErr);
	np->estats.rx_frame_error += readl(base + NvRegRxFrameErr);
	np->estats.rx_extra_byte += readl(base + NvRegRxExtraByte);
	np->estats.rx_late_collision += readl(base + NvRegRxLateCol);
	np->estats.rx_runt += readl(base + NvRegRxRunt);
	np->estats.rx_frame_too_long += readl(base + NvRegRxFrameTooLong);
	np->estats.rx_over_errors += readl(base + NvRegRxOverflow);
	np->estats.rx_crc_errors += readl(base + NvRegRxFCSErr);
	np->estats.rx_frame_align_error += readl(base + NvRegRxFrameAlignErr);
	np->estats.rx_length_error += readl(base + NvRegRxLenErr);
	np->estats.rx_unicast += readl(base + NvRegRxUnicast);
	np->estats.rx_multicast += readl(base + NvRegRxMulticast);
	np->estats.rx_broadcast += readl(base + NvRegRxBroadcast);
	np->estats.rx_packets =
		np->estats.rx_unicast +
		np->estats.rx_multicast +
		np->estats.rx_broadcast;
	np->estats.rx_errors_total =
		np->estats.rx_crc_errors +
		np->estats.rx_over_errors +
		np->estats.rx_frame_error +
		(np->estats.rx_frame_align_error - np->estats.rx_extra_byte) +
		np->estats.rx_late_collision +
		np->estats.rx_runt +
		np->estats.rx_frame_too_long;
	np->estats.tx_errors_total =
		np->estats.tx_late_collision +
		np->estats.tx_fifo_errors +
		np->estats.tx_carrier_errors +
		np->estats.tx_excess_deferral +
		np->estats.tx_retry_error;

	if (np->driver_data & DEV_HAS_STATISTICS_V2) {
		np->estats.tx_deferral += readl(base + NvRegTxDef);
		np->estats.tx_packets += readl(base + NvRegTxFrame);
		np->estats.rx_bytes += readl(base + NvRegRxCnt);
		np->estats.tx_pause += readl(base + NvRegTxPause);
		np->estats.rx_pause += readl(base + NvRegRxPause);
		np->estats.rx_drop_frame += readl(base + NvRegRxDropFrame);
	}

	if (np->driver_data & DEV_HAS_STATISTICS_V3) {
		np->estats.tx_unicast += readl(base + NvRegTxUnicast);
		np->estats.tx_multicast += readl(base + NvRegTxMulticast);
		np->estats.tx_broadcast += readl(base + NvRegTxBroadcast);
	}
}

/*
 * nv_get_stats: dev->get_stats function
 * Get latest stats value from the nic.
 * Called with read_lock(&dev_base_lock) held for read -
 * only synchronized against unregister_netdevice.
 */
static struct net_device_stats *nv_get_stats(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);

	/* If the nic supports hw counters then retrieve latest values */
	if (np->driver_data & (DEV_HAS_STATISTICS_V1|DEV_HAS_STATISTICS_V2|DEV_HAS_STATISTICS_V3)) {
		nv_get_hw_stats(dev);

		/* copy to net_device stats */
		dev->stats.tx_bytes = np->estats.tx_bytes;
		dev->stats.tx_fifo_errors = np->estats.tx_fifo_errors;
		dev->stats.tx_carrier_errors = np->estats.tx_carrier_errors;
		dev->stats.rx_crc_errors = np->estats.rx_crc_errors;
		dev->stats.rx_over_errors = np->estats.rx_over_errors;
		dev->stats.rx_errors = np->estats.rx_errors_total;
		dev->stats.tx_errors = np->estats.tx_errors_total;
	}

	return &dev->stats;
}

/*
 * nv_alloc_rx: fill rx ring entries.
 * Return 1 if the allocations for the skbs failed and the
 * rx engine is without Available descriptors
 */
static int nv_alloc_rx(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	struct ring_desc* less_rx;

	less_rx = np->get_rx.orig;
	if (less_rx-- == np->first_rx.orig)
		less_rx = np->last_rx.orig;

	while (np->put_rx.orig != less_rx) {
		struct sk_buff *skb = dev_alloc_skb(np->rx_buf_sz + NV_RX_ALLOC_PAD);
		if (skb) {
			np->put_rx_ctx->skb = skb;
			np->put_rx_ctx->dma = pci_map_single(np->pci_dev,
							     skb->data,
							     skb_tailroom(skb),
							     PCI_DMA_FROMDEVICE);
			np->put_rx_ctx->dma_len = skb_tailroom(skb);
			np->put_rx.orig->buf = cpu_to_le32(np->put_rx_ctx->dma);
			wmb();
			np->put_rx.orig->flaglen = cpu_to_le32(np->rx_buf_sz | NV_RX_AVAIL);
			if (unlikely(np->put_rx.orig++ == np->last_rx.orig))
				np->put_rx.orig = np->first_rx.orig;
			if (unlikely(np->put_rx_ctx++ == np->last_rx_ctx))
				np->put_rx_ctx = np->first_rx_ctx;
		} else {
			return 1;
		}
	}
	return 0;
}

static int nv_alloc_rx_optimized(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	struct ring_desc_ex* less_rx;

	less_rx = np->get_rx.ex;
	if (less_rx-- == np->first_rx.ex)
		less_rx = np->last_rx.ex;

	while (np->put_rx.ex != less_rx) {
		struct sk_buff *skb = dev_alloc_skb(np->rx_buf_sz + NV_RX_ALLOC_PAD);
		if (skb) {
			np->put_rx_ctx->skb = skb;
			np->put_rx_ctx->dma = pci_map_single(np->pci_dev,
							     skb->data,
							     skb_tailroom(skb),
							     PCI_DMA_FROMDEVICE);
			np->put_rx_ctx->dma_len = skb_tailroom(skb);
			np->put_rx.ex->bufhigh = cpu_to_le32(dma_high(np->put_rx_ctx->dma));
			np->put_rx.ex->buflow = cpu_to_le32(dma_low(np->put_rx_ctx->dma));
			wmb();
			np->put_rx.ex->flaglen = cpu_to_le32(np->rx_buf_sz | NV_RX2_AVAIL);
			if (unlikely(np->put_rx.ex++ == np->last_rx.ex))
				np->put_rx.ex = np->first_rx.ex;
			if (unlikely(np->put_rx_ctx++ == np->last_rx_ctx))
				np->put_rx_ctx = np->first_rx_ctx;
		} else {
			return 1;
		}
	}
	return 0;
}

/* If rx bufs are exhausted called after 50ms to attempt to refresh */
#ifdef CONFIG_FORCEDETH_NAPI
static void nv_do_rx_refill(unsigned long data)
{
	struct net_device *dev = (struct net_device *) data;
	struct fe_priv *np = netdev_priv(dev);

	/* Just reschedule NAPI rx processing */
	napi_schedule(&np->napi);
}
#else
static void nv_do_rx_refill(unsigned long data)
{
	struct net_device *dev = (struct net_device *) data;
	struct fe_priv *np = netdev_priv(dev);
	int retcode;

	if (!using_multi_irqs(dev)) {
		if (np->msi_flags & NV_MSI_X_ENABLED)
			disable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_ALL].vector);
		else
			disable_irq(np->pci_dev->irq);
	} else {
		disable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_RX].vector);
	}
	if (!nv_optimized(np))
		retcode = nv_alloc_rx(dev);
	else
		retcode = nv_alloc_rx_optimized(dev);
	if (retcode) {
		spin_lock_irq(&np->lock);
		if (!np->in_shutdown)
			mod_timer(&np->oom_kick, jiffies + OOM_REFILL);
		spin_unlock_irq(&np->lock);
	}
	if (!using_multi_irqs(dev)) {
		if (np->msi_flags & NV_MSI_X_ENABLED)
			enable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_ALL].vector);
		else
			enable_irq(np->pci_dev->irq);
	} else {
		enable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_RX].vector);
	}
}
#endif

static void nv_init_rx(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	int i;

	np->get_rx = np->put_rx = np->first_rx = np->rx_ring;

	if (!nv_optimized(np))
		np->last_rx.orig = &np->rx_ring.orig[np->rx_ring_size-1];
	else
		np->last_rx.ex = &np->rx_ring.ex[np->rx_ring_size-1];
	np->get_rx_ctx = np->put_rx_ctx = np->first_rx_ctx = np->rx_skb;
	np->last_rx_ctx = &np->rx_skb[np->rx_ring_size-1];

	for (i = 0; i < np->rx_ring_size; i++) {
		if (!nv_optimized(np)) {
			np->rx_ring.orig[i].flaglen = 0;
			np->rx_ring.orig[i].buf = 0;
		} else {
			np->rx_ring.ex[i].flaglen = 0;
			np->rx_ring.ex[i].txvlan = 0;
			np->rx_ring.ex[i].bufhigh = 0;
			np->rx_ring.ex[i].buflow = 0;
		}
		np->rx_skb[i].skb = NULL;
		np->rx_skb[i].dma = 0;
	}
}

static void nv_init_tx(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	int i;

	np->get_tx = np->put_tx = np->first_tx = np->tx_ring;

	if (!nv_optimized(np))
		np->last_tx.orig = &np->tx_ring.orig[np->tx_ring_size-1];
	else
		np->last_tx.ex = &np->tx_ring.ex[np->tx_ring_size-1];
	np->get_tx_ctx = np->put_tx_ctx = np->first_tx_ctx = np->tx_skb;
	np->last_tx_ctx = &np->tx_skb[np->tx_ring_size-1];
	np->tx_pkts_in_progress = 0;
	np->tx_change_owner = NULL;
	np->tx_end_flip = NULL;
	np->tx_stop = 0;

	for (i = 0; i < np->tx_ring_size; i++) {
		if (!nv_optimized(np)) {
			np->tx_ring.orig[i].flaglen = 0;
			np->tx_ring.orig[i].buf = 0;
		} else {
			np->tx_ring.ex[i].flaglen = 0;
			np->tx_ring.ex[i].txvlan = 0;
			np->tx_ring.ex[i].bufhigh = 0;
			np->tx_ring.ex[i].buflow = 0;
		}
		np->tx_skb[i].skb = NULL;
		np->tx_skb[i].dma = 0;
		np->tx_skb[i].dma_len = 0;
		np->tx_skb[i].dma_single = 0;
		np->tx_skb[i].first_tx_desc = NULL;
		np->tx_skb[i].next_tx_ctx = NULL;
	}
}

static int nv_init_ring(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);

	nv_init_tx(dev);
	nv_init_rx(dev);

	if (!nv_optimized(np))
		return nv_alloc_rx(dev);
	else
		return nv_alloc_rx_optimized(dev);
}

static void nv_unmap_txskb(struct fe_priv *np, struct nv_skb_map *tx_skb)
{
	if (tx_skb->dma) {
		if (tx_skb->dma_single)
			pci_unmap_single(np->pci_dev, tx_skb->dma,
					 tx_skb->dma_len,
					 PCI_DMA_TODEVICE);
		else
			pci_unmap_page(np->pci_dev, tx_skb->dma,
				       tx_skb->dma_len,
				       PCI_DMA_TODEVICE);
		tx_skb->dma = 0;
	}
}

static int nv_release_txskb(struct fe_priv *np, struct nv_skb_map *tx_skb)
{
	nv_unmap_txskb(np, tx_skb);
	if (tx_skb->skb) {
		dev_kfree_skb_any(tx_skb->skb);
		tx_skb->skb = NULL;
		return 1;
	}
	return 0;
}

static void nv_drain_tx(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	unsigned int i;

	for (i = 0; i < np->tx_ring_size; i++) {
		if (!nv_optimized(np)) {
			np->tx_ring.orig[i].flaglen = 0;
			np->tx_ring.orig[i].buf = 0;
		} else {
			np->tx_ring.ex[i].flaglen = 0;
			np->tx_ring.ex[i].txvlan = 0;
			np->tx_ring.ex[i].bufhigh = 0;
			np->tx_ring.ex[i].buflow = 0;
		}
		if (nv_release_txskb(np, &np->tx_skb[i]))
			dev->stats.tx_dropped++;
		np->tx_skb[i].dma = 0;
		np->tx_skb[i].dma_len = 0;
		np->tx_skb[i].dma_single = 0;
		np->tx_skb[i].first_tx_desc = NULL;
		np->tx_skb[i].next_tx_ctx = NULL;
	}
	np->tx_pkts_in_progress = 0;
	np->tx_change_owner = NULL;
	np->tx_end_flip = NULL;
}

static void nv_drain_rx(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	int i;

	for (i = 0; i < np->rx_ring_size; i++) {
		if (!nv_optimized(np)) {
			np->rx_ring.orig[i].flaglen = 0;
			np->rx_ring.orig[i].buf = 0;
		} else {
			np->rx_ring.ex[i].flaglen = 0;
			np->rx_ring.ex[i].txvlan = 0;
			np->rx_ring.ex[i].bufhigh = 0;
			np->rx_ring.ex[i].buflow = 0;
		}
		wmb();
		if (np->rx_skb[i].skb) {
			pci_unmap_single(np->pci_dev, np->rx_skb[i].dma,
					 (skb_end_pointer(np->rx_skb[i].skb) -
					  np->rx_skb[i].skb->data),
					 PCI_DMA_FROMDEVICE);
			dev_kfree_skb(np->rx_skb[i].skb);
			np->rx_skb[i].skb = NULL;
		}
	}
}

static void nv_drain_rxtx(struct net_device *dev)
{
	nv_drain_tx(dev);
	nv_drain_rx(dev);
}

static inline u32 nv_get_empty_tx_slots(struct fe_priv *np)
{
	return (u32)(np->tx_ring_size - ((np->tx_ring_size + (np->put_tx_ctx - np->get_tx_ctx)) % np->tx_ring_size));
}

static void nv_legacybackoff_reseed(struct net_device *dev)
{
	u8 __iomem *base = get_hwbase(dev);
	u32 reg;
	u32 low;
	int tx_status = 0;

	reg = readl(base + NvRegSlotTime) & ~NVREG_SLOTTIME_MASK;
	get_random_bytes(&low, sizeof(low));
	reg |= low & NVREG_SLOTTIME_MASK;

	/* Need to stop tx before change takes effect.
	 * Caller has already gained np->lock.
	 */
	tx_status = readl(base + NvRegTransmitterControl) & NVREG_XMITCTL_START;
	if (tx_status)
		nv_stop_tx(dev);
	nv_stop_rx(dev);
	writel(reg, base + NvRegSlotTime);
	if (tx_status)
		nv_start_tx(dev);
	nv_start_rx(dev);
}

/* Gear Backoff Seeds */
#define BACKOFF_SEEDSET_ROWS	8
#define BACKOFF_SEEDSET_LFSRS	15

/* Known Good seed sets */
static const u32 main_seedset[BACKOFF_SEEDSET_ROWS][BACKOFF_SEEDSET_LFSRS] = {
    {145, 155, 165, 175, 185, 196, 235, 245, 255, 265, 275, 285, 660, 690, 874},
    {245, 255, 265, 575, 385, 298, 335, 345, 355, 366, 375, 385, 761, 790, 974},
    {145, 155, 165, 175, 185, 196, 235, 245, 255, 265, 275, 285, 660, 690, 874},
    {245, 255, 265, 575, 385, 298, 335, 345, 355, 366, 375, 386, 761, 790, 974},
    {266, 265, 276, 585, 397, 208, 345, 355, 365, 376, 385, 396, 771, 700, 984},
    {266, 265, 276, 586, 397, 208, 346, 355, 365, 376, 285, 396, 771, 700, 984},
    {366, 365, 376, 686, 497, 308, 447, 455, 466, 476, 485, 496, 871, 800,  84},
    {466, 465, 476, 786, 597, 408, 547, 555, 566, 576, 585, 597, 971, 900, 184}};

static const u32 gear_seedset[BACKOFF_SEEDSET_ROWS][BACKOFF_SEEDSET_LFSRS] = {
    {251, 262, 273, 324, 319, 508, 375, 364, 341, 371, 398, 193, 375,  30, 295},
    {351, 375, 373, 469, 551, 639, 477, 464, 441, 472, 498, 293, 476, 130, 395},
    {351, 375, 373, 469, 551, 639, 477, 464, 441, 472, 498, 293, 476, 130, 397},
    {251, 262, 273, 324, 319, 508, 375, 364, 341, 371, 398, 193, 375,  30, 295},
    {251, 262, 273, 324, 319, 508, 375, 364, 341, 371, 398, 193, 375,  30, 295},
    {351, 375, 373, 469, 551, 639, 477, 464, 441, 472, 498, 293, 476, 130, 395},
    {351, 375, 373, 469, 551, 639, 477, 464, 441, 472, 498, 293, 476, 130, 395},
    {351, 375, 373, 469, 551, 639, 477, 464, 441, 472, 498, 293, 476, 130, 395}};

static void nv_gear_backoff_reseed(struct net_device *dev)
{
	u8 __iomem *base = get_hwbase(dev);
	u32 miniseed1, miniseed2, miniseed2_reversed, miniseed3, miniseed3_reversed;
	u32 temp, seedset, combinedSeed;
	int i;

	/* Setup seed for free running LFSR */
	/* We are going to read the time stamp counter 3 times
	   and swizzle bits around to increase randomness */
	get_random_bytes(&miniseed1, sizeof(miniseed1));
	miniseed1 &= 0x0fff;
	if (miniseed1 == 0)
		miniseed1 = 0xabc;

	get_random_bytes(&miniseed2, sizeof(miniseed2));
	miniseed2 &= 0x0fff;
	if (miniseed2 == 0)
		miniseed2 = 0xabc;
	miniseed2_reversed =
		((miniseed2 & 0xF00) >> 8) |
		 (miniseed2 & 0x0F0) |
		 ((miniseed2 & 0x00F) << 8);

	get_random_bytes(&miniseed3, sizeof(miniseed3));
	miniseed3 &= 0x0fff;
	if (miniseed3 == 0)
		miniseed3 = 0xabc;
	miniseed3_reversed =
		((miniseed3 & 0xF00) >> 8) |
		 (miniseed3 & 0x0F0) |
		 ((miniseed3 & 0x00F) << 8);

	combinedSeed = ((miniseed1 ^ miniseed2_reversed) << 12) |
		       (miniseed2 ^ miniseed3_reversed);

	/* Seeds can not be zero */
	if ((combinedSeed & NVREG_BKOFFCTRL_SEED_MASK) == 0)
		combinedSeed |= 0x08;
	if ((combinedSeed & (NVREG_BKOFFCTRL_SEED_MASK << NVREG_BKOFFCTRL_GEAR)) == 0)
		combinedSeed |= 0x8000;

	/* No need to disable tx here */
	temp = NVREG_BKOFFCTRL_DEFAULT | (0 << NVREG_BKOFFCTRL_SELECT);
	temp |= combinedSeed & NVREG_BKOFFCTRL_SEED_MASK;
	temp |= combinedSeed >> NVREG_BKOFFCTRL_GEAR;
	writel(temp,base + NvRegBackOffControl);

    	/* Setup seeds for all gear LFSRs. */
	get_random_bytes(&seedset, sizeof(seedset));
	seedset = seedset % BACKOFF_SEEDSET_ROWS;
	for (i = 1; i <= BACKOFF_SEEDSET_LFSRS; i++)
	{
		temp = NVREG_BKOFFCTRL_DEFAULT | (i << NVREG_BKOFFCTRL_SELECT);
		temp |= main_seedset[seedset][i-1] & 0x3ff;
		temp |= ((gear_seedset[seedset][i-1] & 0x3ff) << NVREG_BKOFFCTRL_GEAR);
		writel(temp, base + NvRegBackOffControl);
	}
}

/*
 * nv_start_xmit: dev->hard_start_xmit function
 * Called with netif_tx_lock held.
 */
static netdev_tx_t nv_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	u32 tx_flags = 0;
	u32 tx_flags_extra = (np->desc_ver == DESC_VER_1 ? NV_TX_LASTPACKET : NV_TX2_LASTPACKET);
	unsigned int fragments = skb_shinfo(skb)->nr_frags;
	unsigned int i;
	u32 offset = 0;
	u32 bcnt;
	u32 size = skb->len-skb->data_len;
	u32 entries = (size >> NV_TX2_TSO_MAX_SHIFT) + ((size & (NV_TX2_TSO_MAX_SIZE-1)) ? 1 : 0);
	u32 empty_slots;
	struct ring_desc* put_tx;
	struct ring_desc* start_tx;
	struct ring_desc* prev_tx;
	struct nv_skb_map* prev_tx_ctx;
	unsigned long flags;

	/* add fragments to entries count */
	for (i = 0; i < fragments; i++) {
		entries += (skb_shinfo(skb)->frags[i].size >> NV_TX2_TSO_MAX_SHIFT) +
			   ((skb_shinfo(skb)->frags[i].size & (NV_TX2_TSO_MAX_SIZE-1)) ? 1 : 0);
	}

	spin_lock_irqsave(&np->lock, flags);
	empty_slots = nv_get_empty_tx_slots(np);
	if (unlikely(empty_slots <= entries)) {
		netif_stop_queue(dev);
		np->tx_stop = 1;
		spin_unlock_irqrestore(&np->lock, flags);
		return NETDEV_TX_BUSY;
	}
	spin_unlock_irqrestore(&np->lock, flags);

	start_tx = put_tx = np->put_tx.orig;

	/* setup the header buffer */
	do {
		prev_tx = put_tx;
		prev_tx_ctx = np->put_tx_ctx;
		bcnt = (size > NV_TX2_TSO_MAX_SIZE) ? NV_TX2_TSO_MAX_SIZE : size;
		np->put_tx_ctx->dma = pci_map_single(np->pci_dev, skb->data + offset, bcnt,
						PCI_DMA_TODEVICE);
		np->put_tx_ctx->dma_len = bcnt;
		np->put_tx_ctx->dma_single = 1;
		put_tx->buf = cpu_to_le32(np->put_tx_ctx->dma);
		put_tx->flaglen = cpu_to_le32((bcnt-1) | tx_flags);

		tx_flags = np->tx_flags;
		offset += bcnt;
		size -= bcnt;
		if (unlikely(put_tx++ == np->last_tx.orig))
			put_tx = np->first_tx.orig;
		if (unlikely(np->put_tx_ctx++ == np->last_tx_ctx))
			np->put_tx_ctx = np->first_tx_ctx;
	} while (size);

	/* setup the fragments */
	for (i = 0; i < fragments; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		u32 size = frag->size;
		offset = 0;

		do {
			prev_tx = put_tx;
			prev_tx_ctx = np->put_tx_ctx;
			bcnt = (size > NV_TX2_TSO_MAX_SIZE) ? NV_TX2_TSO_MAX_SIZE : size;
			np->put_tx_ctx->dma = pci_map_page(np->pci_dev, frag->page, frag->page_offset+offset, bcnt,
							   PCI_DMA_TODEVICE);
			np->put_tx_ctx->dma_len = bcnt;
			np->put_tx_ctx->dma_single = 0;
			put_tx->buf = cpu_to_le32(np->put_tx_ctx->dma);
			put_tx->flaglen = cpu_to_le32((bcnt-1) | tx_flags);

			offset += bcnt;
			size -= bcnt;
			if (unlikely(put_tx++ == np->last_tx.orig))
				put_tx = np->first_tx.orig;
			if (unlikely(np->put_tx_ctx++ == np->last_tx_ctx))
				np->put_tx_ctx = np->first_tx_ctx;
		} while (size);
	}

	/* set last fragment flag  */
	prev_tx->flaglen |= cpu_to_le32(tx_flags_extra);

	/* save skb in this slot's context area */
	prev_tx_ctx->skb = skb;

	if (skb_is_gso(skb))
		tx_flags_extra = NV_TX2_TSO | (skb_shinfo(skb)->gso_size << NV_TX2_TSO_SHIFT);
	else
		tx_flags_extra = skb->ip_summed == CHECKSUM_PARTIAL ?
			 NV_TX2_CHECKSUM_L3 | NV_TX2_CHECKSUM_L4 : 0;

	spin_lock_irqsave(&np->lock, flags);

	/* set tx flags */
	start_tx->flaglen |= cpu_to_le32(tx_flags | tx_flags_extra);
	np->put_tx.orig = put_tx;

	spin_unlock_irqrestore(&np->lock, flags);

	dprintk(KERN_DEBUG "%s: nv_start_xmit: entries %d queued for transmission. tx_flags_extra: %x\n",
		dev->name, entries, tx_flags_extra);
	{
		int j;
		for (j=0; j<64; j++) {
			if ((j%16) == 0)
				dprintk("\n%03x:", j);
			dprintk(" %02x", ((unsigned char*)skb->data)[j]);
		}
		dprintk("\n");
	}

	dev->trans_start = jiffies;
	writel(NVREG_TXRXCTL_KICK|np->txrxctl_bits, get_hwbase(dev) + NvRegTxRxControl);
	return NETDEV_TX_OK;
}

static netdev_tx_t nv_start_xmit_optimized(struct sk_buff *skb,
					   struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	u32 tx_flags = 0;
	u32 tx_flags_extra;
	unsigned int fragments = skb_shinfo(skb)->nr_frags;
	unsigned int i;
	u32 offset = 0;
	u32 bcnt;
	u32 size = skb->len-skb->data_len;
	u32 entries = (size >> NV_TX2_TSO_MAX_SHIFT) + ((size & (NV_TX2_TSO_MAX_SIZE-1)) ? 1 : 0);
	u32 empty_slots;
	struct ring_desc_ex* put_tx;
	struct ring_desc_ex* start_tx;
	struct ring_desc_ex* prev_tx;
	struct nv_skb_map* prev_tx_ctx;
	struct nv_skb_map* start_tx_ctx;
	unsigned long flags;

	/* add fragments to entries count */
	for (i = 0; i < fragments; i++) {
		entries += (skb_shinfo(skb)->frags[i].size >> NV_TX2_TSO_MAX_SHIFT) +
			   ((skb_shinfo(skb)->frags[i].size & (NV_TX2_TSO_MAX_SIZE-1)) ? 1 : 0);
	}

	spin_lock_irqsave(&np->lock, flags);
	empty_slots = nv_get_empty_tx_slots(np);
	if (unlikely(empty_slots <= entries)) {
		netif_stop_queue(dev);
		np->tx_stop = 1;
		spin_unlock_irqrestore(&np->lock, flags);
		return NETDEV_TX_BUSY;
	}
	spin_unlock_irqrestore(&np->lock, flags);

	start_tx = put_tx = np->put_tx.ex;
	start_tx_ctx = np->put_tx_ctx;

	/* setup the header buffer */
	do {
		prev_tx = put_tx;
		prev_tx_ctx = np->put_tx_ctx;
		bcnt = (size > NV_TX2_TSO_MAX_SIZE) ? NV_TX2_TSO_MAX_SIZE : size;
		np->put_tx_ctx->dma = pci_map_single(np->pci_dev, skb->data + offset, bcnt,
						PCI_DMA_TODEVICE);
		np->put_tx_ctx->dma_len = bcnt;
		np->put_tx_ctx->dma_single = 1;
		put_tx->bufhigh = cpu_to_le32(dma_high(np->put_tx_ctx->dma));
		put_tx->buflow = cpu_to_le32(dma_low(np->put_tx_ctx->dma));
		put_tx->flaglen = cpu_to_le32((bcnt-1) | tx_flags);

		tx_flags = NV_TX2_VALID;
		offset += bcnt;
		size -= bcnt;
		if (unlikely(put_tx++ == np->last_tx.ex))
			put_tx = np->first_tx.ex;
		if (unlikely(np->put_tx_ctx++ == np->last_tx_ctx))
			np->put_tx_ctx = np->first_tx_ctx;
	} while (size);

	/* setup the fragments */
	for (i = 0; i < fragments; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		u32 size = frag->size;
		offset = 0;

		do {
			prev_tx = put_tx;
			prev_tx_ctx = np->put_tx_ctx;
			bcnt = (size > NV_TX2_TSO_MAX_SIZE) ? NV_TX2_TSO_MAX_SIZE : size;
			np->put_tx_ctx->dma = pci_map_page(np->pci_dev, frag->page, frag->page_offset+offset, bcnt,
							   PCI_DMA_TODEVICE);
			np->put_tx_ctx->dma_len = bcnt;
			np->put_tx_ctx->dma_single = 0;
			put_tx->bufhigh = cpu_to_le32(dma_high(np->put_tx_ctx->dma));
			put_tx->buflow = cpu_to_le32(dma_low(np->put_tx_ctx->dma));
			put_tx->flaglen = cpu_to_le32((bcnt-1) | tx_flags);

			offset += bcnt;
			size -= bcnt;
			if (unlikely(put_tx++ == np->last_tx.ex))
				put_tx = np->first_tx.ex;
			if (unlikely(np->put_tx_ctx++ == np->last_tx_ctx))
				np->put_tx_ctx = np->first_tx_ctx;
		} while (size);
	}

	/* set last fragment flag  */
	prev_tx->flaglen |= cpu_to_le32(NV_TX2_LASTPACKET);

	/* save skb in this slot's context area */
	prev_tx_ctx->skb = skb;

	if (skb_is_gso(skb))
		tx_flags_extra = NV_TX2_TSO | (skb_shinfo(skb)->gso_size << NV_TX2_TSO_SHIFT);
	else
		tx_flags_extra = skb->ip_summed == CHECKSUM_PARTIAL ?
			 NV_TX2_CHECKSUM_L3 | NV_TX2_CHECKSUM_L4 : 0;

	/* vlan tag */
	if (likely(!np->vlangrp)) {
		start_tx->txvlan = 0;
	} else {
		if (vlan_tx_tag_present(skb))
			start_tx->txvlan = cpu_to_le32(NV_TX3_VLAN_TAG_PRESENT | vlan_tx_tag_get(skb));
		else
			start_tx->txvlan = 0;
	}

	spin_lock_irqsave(&np->lock, flags);

	if (np->tx_limit) {
		/* Limit the number of outstanding tx. Setup all fragments, but
		 * do not set the VALID bit on the first descriptor. Save a pointer
		 * to that descriptor and also for next skb_map element.
		 */

		if (np->tx_pkts_in_progress == NV_TX_LIMIT_COUNT) {
			if (!np->tx_change_owner)
				np->tx_change_owner = start_tx_ctx;

			/* remove VALID bit */
			tx_flags &= ~NV_TX2_VALID;
			start_tx_ctx->first_tx_desc = start_tx;
			start_tx_ctx->next_tx_ctx = np->put_tx_ctx;
			np->tx_end_flip = np->put_tx_ctx;
		} else {
			np->tx_pkts_in_progress++;
		}
	}

	/* set tx flags */
	start_tx->flaglen |= cpu_to_le32(tx_flags | tx_flags_extra);
	np->put_tx.ex = put_tx;

	spin_unlock_irqrestore(&np->lock, flags);

	dprintk(KERN_DEBUG "%s: nv_start_xmit_optimized: entries %d queued for transmission. tx_flags_extra: %x\n",
		dev->name, entries, tx_flags_extra);
	{
		int j;
		for (j=0; j<64; j++) {
			if ((j%16) == 0)
				dprintk("\n%03x:", j);
			dprintk(" %02x", ((unsigned char*)skb->data)[j]);
		}
		dprintk("\n");
	}

	dev->trans_start = jiffies;
	writel(NVREG_TXRXCTL_KICK|np->txrxctl_bits, get_hwbase(dev) + NvRegTxRxControl);
	return NETDEV_TX_OK;
}

static inline void nv_tx_flip_ownership(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);

	np->tx_pkts_in_progress--;
	if (np->tx_change_owner) {
		np->tx_change_owner->first_tx_desc->flaglen |=
			cpu_to_le32(NV_TX2_VALID);
		np->tx_pkts_in_progress++;

		np->tx_change_owner = np->tx_change_owner->next_tx_ctx;
		if (np->tx_change_owner == np->tx_end_flip)
			np->tx_change_owner = NULL;

		writel(NVREG_TXRXCTL_KICK|np->txrxctl_bits, get_hwbase(dev) + NvRegTxRxControl);
	}
}

/*
 * nv_tx_done: check for completed packets, release the skbs.
 *
 * Caller must own np->lock.
 */
static int nv_tx_done(struct net_device *dev, int limit)
{
	struct fe_priv *np = netdev_priv(dev);
	u32 flags;
	int tx_work = 0;
	struct ring_desc* orig_get_tx = np->get_tx.orig;

	while ((np->get_tx.orig != np->put_tx.orig) &&
	       !((flags = le32_to_cpu(np->get_tx.orig->flaglen)) & NV_TX_VALID) &&
	       (tx_work < limit)) {

		dprintk(KERN_DEBUG "%s: nv_tx_done: flags 0x%x.\n",
					dev->name, flags);

		nv_unmap_txskb(np, np->get_tx_ctx);

		if (np->desc_ver == DESC_VER_1) {
			if (flags & NV_TX_LASTPACKET) {
				if (flags & NV_TX_ERROR) {
					if (flags & NV_TX_UNDERFLOW)
						dev->stats.tx_fifo_errors++;
					if (flags & NV_TX_CARRIERLOST)
						dev->stats.tx_carrier_errors++;
					if ((flags & NV_TX_RETRYERROR) && !(flags & NV_TX_RETRYCOUNT_MASK))
						nv_legacybackoff_reseed(dev);
					dev->stats.tx_errors++;
				} else {
					dev->stats.tx_packets++;
					dev->stats.tx_bytes += np->get_tx_ctx->skb->len;
				}
				dev_kfree_skb_any(np->get_tx_ctx->skb);
				np->get_tx_ctx->skb = NULL;
				tx_work++;
			}
		} else {
			if (flags & NV_TX2_LASTPACKET) {
				if (flags & NV_TX2_ERROR) {
					if (flags & NV_TX2_UNDERFLOW)
						dev->stats.tx_fifo_errors++;
					if (flags & NV_TX2_CARRIERLOST)
						dev->stats.tx_carrier_errors++;
					if ((flags & NV_TX2_RETRYERROR) && !(flags & NV_TX2_RETRYCOUNT_MASK))
						nv_legacybackoff_reseed(dev);
					dev->stats.tx_errors++;
				} else {
					dev->stats.tx_packets++;
					dev->stats.tx_bytes += np->get_tx_ctx->skb->len;
				}
				dev_kfree_skb_any(np->get_tx_ctx->skb);
				np->get_tx_ctx->skb = NULL;
				tx_work++;
			}
		}
		if (unlikely(np->get_tx.orig++ == np->last_tx.orig))
			np->get_tx.orig = np->first_tx.orig;
		if (unlikely(np->get_tx_ctx++ == np->last_tx_ctx))
			np->get_tx_ctx = np->first_tx_ctx;
	}
	if (unlikely((np->tx_stop == 1) && (np->get_tx.orig != orig_get_tx))) {
		np->tx_stop = 0;
		netif_wake_queue(dev);
	}
	return tx_work;
}

static int nv_tx_done_optimized(struct net_device *dev, int limit)
{
	struct fe_priv *np = netdev_priv(dev);
	u32 flags;
	int tx_work = 0;
	struct ring_desc_ex* orig_get_tx = np->get_tx.ex;

	while ((np->get_tx.ex != np->put_tx.ex) &&
	       !((flags = le32_to_cpu(np->get_tx.ex->flaglen)) & NV_TX_VALID) &&
	       (tx_work < limit)) {

		dprintk(KERN_DEBUG "%s: nv_tx_done_optimized: flags 0x%x.\n",
					dev->name, flags);

		nv_unmap_txskb(np, np->get_tx_ctx);

		if (flags & NV_TX2_LASTPACKET) {
			if (!(flags & NV_TX2_ERROR))
				dev->stats.tx_packets++;
			else {
				if ((flags & NV_TX2_RETRYERROR) && !(flags & NV_TX2_RETRYCOUNT_MASK)) {
					if (np->driver_data & DEV_HAS_GEAR_MODE)
						nv_gear_backoff_reseed(dev);
					else
						nv_legacybackoff_reseed(dev);
				}
			}

			dev_kfree_skb_any(np->get_tx_ctx->skb);
			np->get_tx_ctx->skb = NULL;
			tx_work++;

			if (np->tx_limit) {
				nv_tx_flip_ownership(dev);
			}
		}
		if (unlikely(np->get_tx.ex++ == np->last_tx.ex))
			np->get_tx.ex = np->first_tx.ex;
		if (unlikely(np->get_tx_ctx++ == np->last_tx_ctx))
			np->get_tx_ctx = np->first_tx_ctx;
	}
	if (unlikely((np->tx_stop == 1) && (np->get_tx.ex != orig_get_tx))) {
		np->tx_stop = 0;
		netif_wake_queue(dev);
	}
	return tx_work;
}

/*
 * nv_tx_timeout: dev->tx_timeout function
 * Called with netif_tx_lock held.
 */
static void nv_tx_timeout(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	u8 __iomem *base = get_hwbase(dev);
	u32 status;
	union ring_type put_tx;
	int saved_tx_limit;

	if (np->msi_flags & NV_MSI_X_ENABLED)
		status = readl(base + NvRegMSIXIrqStatus) & NVREG_IRQSTAT_MASK;
	else
		status = readl(base + NvRegIrqStatus) & NVREG_IRQSTAT_MASK;

	printk(KERN_INFO "%s: Got tx_timeout. irq: %08x\n", dev->name, status);

	{
		int i;

		printk(KERN_INFO "%s: Ring at %lx\n",
		       dev->name, (unsigned long)np->ring_addr);
		printk(KERN_INFO "%s: Dumping tx registers\n", dev->name);
		for (i=0;i<=np->register_size;i+= 32) {
			printk(KERN_INFO "%3x: %08x %08x %08x %08x %08x %08x %08x %08x\n",
					i,
					readl(base + i + 0), readl(base + i + 4),
					readl(base + i + 8), readl(base + i + 12),
					readl(base + i + 16), readl(base + i + 20),
					readl(base + i + 24), readl(base + i + 28));
		}
		printk(KERN_INFO "%s: Dumping tx ring\n", dev->name);
		for (i=0;i<np->tx_ring_size;i+= 4) {
			if (!nv_optimized(np)) {
				printk(KERN_INFO "%03x: %08x %08x // %08x %08x // %08x %08x // %08x %08x\n",
				       i,
				       le32_to_cpu(np->tx_ring.orig[i].buf),
				       le32_to_cpu(np->tx_ring.orig[i].flaglen),
				       le32_to_cpu(np->tx_ring.orig[i+1].buf),
				       le32_to_cpu(np->tx_ring.orig[i+1].flaglen),
				       le32_to_cpu(np->tx_ring.orig[i+2].buf),
				       le32_to_cpu(np->tx_ring.orig[i+2].flaglen),
				       le32_to_cpu(np->tx_ring.orig[i+3].buf),
				       le32_to_cpu(np->tx_ring.orig[i+3].flaglen));
			} else {
				printk(KERN_INFO "%03x: %08x %08x %08x // %08x %08x %08x // %08x %08x %08x // %08x %08x %08x\n",
				       i,
				       le32_to_cpu(np->tx_ring.ex[i].bufhigh),
				       le32_to_cpu(np->tx_ring.ex[i].buflow),
				       le32_to_cpu(np->tx_ring.ex[i].flaglen),
				       le32_to_cpu(np->tx_ring.ex[i+1].bufhigh),
				       le32_to_cpu(np->tx_ring.ex[i+1].buflow),
				       le32_to_cpu(np->tx_ring.ex[i+1].flaglen),
				       le32_to_cpu(np->tx_ring.ex[i+2].bufhigh),
				       le32_to_cpu(np->tx_ring.ex[i+2].buflow),
				       le32_to_cpu(np->tx_ring.ex[i+2].flaglen),
				       le32_to_cpu(np->tx_ring.ex[i+3].bufhigh),
				       le32_to_cpu(np->tx_ring.ex[i+3].buflow),
				       le32_to_cpu(np->tx_ring.ex[i+3].flaglen));
			}
		}
	}

	spin_lock_irq(&np->lock);

	/* 1) stop tx engine */
	nv_stop_tx(dev);

	/* 2) complete any outstanding tx and do not give HW any limited tx pkts */
	saved_tx_limit = np->tx_limit;
	np->tx_limit = 0; /* prevent giving HW any limited pkts */
	np->tx_stop = 0;  /* prevent waking tx queue */
	if (!nv_optimized(np))
		nv_tx_done(dev, np->tx_ring_size);
	else
		nv_tx_done_optimized(dev, np->tx_ring_size);

	/* save current HW postion */
	if (np->tx_change_owner)
		put_tx.ex = np->tx_change_owner->first_tx_desc;
	else
		put_tx = np->put_tx;

	/* 3) clear all tx state */
	nv_drain_tx(dev);
	nv_init_tx(dev);

	/* 4) restore state to current HW position */
	np->get_tx = np->put_tx = put_tx;
	np->tx_limit = saved_tx_limit;

	/* 5) restart tx engine */
	nv_start_tx(dev);
	netif_wake_queue(dev);
	spin_unlock_irq(&np->lock);
}

/*
 * Called when the nic notices a mismatch between the actual data len on the
 * wire and the len indicated in the 802 header
 */
static int nv_getlen(struct net_device *dev, void *packet, int datalen)
{
	int hdrlen;	/* length of the 802 header */
	int protolen;	/* length as stored in the proto field */

	/* 1) calculate len according to header */
	if ( ((struct vlan_ethhdr *)packet)->h_vlan_proto == htons(ETH_P_8021Q)) {
		protolen = ntohs( ((struct vlan_ethhdr *)packet)->h_vlan_encapsulated_proto );
		hdrlen = VLAN_HLEN;
	} else {
		protolen = ntohs( ((struct ethhdr *)packet)->h_proto);
		hdrlen = ETH_HLEN;
	}
	dprintk(KERN_DEBUG "%s: nv_getlen: datalen %d, protolen %d, hdrlen %d\n",
				dev->name, datalen, protolen, hdrlen);
	if (protolen > ETH_DATA_LEN)
		return datalen; /* Value in proto field not a len, no checks possible */

	protolen += hdrlen;
	/* consistency checks: */
	if (datalen > ETH_ZLEN) {
		if (datalen >= protolen) {
			/* more data on wire than in 802 header, trim of
			 * additional data.
			 */
			dprintk(KERN_DEBUG "%s: nv_getlen: accepting %d bytes.\n",
					dev->name, protolen);
			return protolen;
		} else {
			/* less data on wire than mentioned in header.
			 * Discard the packet.
			 */
			dprintk(KERN_DEBUG "%s: nv_getlen: discarding long packet.\n",
					dev->name);
			return -1;
		}
	} else {
		/* short packet. Accept only if 802 values are also short */
		if (protolen > ETH_ZLEN) {
			dprintk(KERN_DEBUG "%s: nv_getlen: discarding short packet.\n",
					dev->name);
			return -1;
		}
		dprintk(KERN_DEBUG "%s: nv_getlen: accepting %d bytes.\n",
				dev->name, datalen);
		return datalen;
	}
}

static int nv_rx_process(struct net_device *dev, int limit)
{
	struct fe_priv *np = netdev_priv(dev);
	u32 flags;
	int rx_work = 0;
	struct sk_buff *skb;
	int len;

	while((np->get_rx.orig != np->put_rx.orig) &&
	      !((flags = le32_to_cpu(np->get_rx.orig->flaglen)) & NV_RX_AVAIL) &&
		(rx_work < limit)) {

		dprintk(KERN_DEBUG "%s: nv_rx_process: flags 0x%x.\n",
					dev->name, flags);

		/*
		 * the packet is for us - immediately tear down the pci mapping.
		 * TODO: check if a prefetch of the first cacheline improves
		 * the performance.
		 */
		pci_unmap_single(np->pci_dev, np->get_rx_ctx->dma,
				np->get_rx_ctx->dma_len,
				PCI_DMA_FROMDEVICE);
		skb = np->get_rx_ctx->skb;
		np->get_rx_ctx->skb = NULL;

		{
			int j;
			dprintk(KERN_DEBUG "Dumping packet (flags 0x%x).",flags);
			for (j=0; j<64; j++) {
				if ((j%16) == 0)
					dprintk("\n%03x:", j);
				dprintk(" %02x", ((unsigned char*)skb->data)[j]);
			}
			dprintk("\n");
		}
		/* look at what we actually got: */
		if (np->desc_ver == DESC_VER_1) {
			if (likely(flags & NV_RX_DESCRIPTORVALID)) {
				len = flags & LEN_MASK_V1;
				if (unlikely(flags & NV_RX_ERROR)) {
					if ((flags & NV_RX_ERROR_MASK) == NV_RX_ERROR4) {
						len = nv_getlen(dev, skb->data, len);
						if (len < 0) {
							dev->stats.rx_errors++;
							dev_kfree_skb(skb);
							goto next_pkt;
						}
					}
					/* framing errors are soft errors */
					else if ((flags & NV_RX_ERROR_MASK) == NV_RX_FRAMINGERR) {
						if (flags & NV_RX_SUBSTRACT1) {
							len--;
						}
					}
					/* the rest are hard errors */
					else {
						if (flags & NV_RX_MISSEDFRAME)
							dev->stats.rx_missed_errors++;
						if (flags & NV_RX_CRCERR)
							dev->stats.rx_crc_errors++;
						if (flags & NV_RX_OVERFLOW)
							dev->stats.rx_over_errors++;
						dev->stats.rx_errors++;
						dev_kfree_skb(skb);
						goto next_pkt;
					}
				}
			} else {
				dev_kfree_skb(skb);
				goto next_pkt;
			}
		} else {
			if (likely(flags & NV_RX2_DESCRIPTORVALID)) {
				len = flags & LEN_MASK_V2;
				if (unlikely(flags & NV_RX2_ERROR)) {
					if ((flags & NV_RX2_ERROR_MASK) == NV_RX2_ERROR4) {
						len = nv_getlen(dev, skb->data, len);
						if (len < 0) {
							dev->stats.rx_errors++;
							dev_kfree_skb(skb);
							goto next_pkt;
						}
					}
					/* framing errors are soft errors */
					else if ((flags & NV_RX2_ERROR_MASK) == NV_RX2_FRAMINGERR) {
						if (flags & NV_RX2_SUBSTRACT1) {
							len--;
						}
					}
					/* the rest are hard errors */
					else {
						if (flags & NV_RX2_CRCERR)
							dev->stats.rx_crc_errors++;
						if (flags & NV_RX2_OVERFLOW)
							dev->stats.rx_over_errors++;
						dev->stats.rx_errors++;
						dev_kfree_skb(skb);
						goto next_pkt;
					}
				}
				if (((flags & NV_RX2_CHECKSUMMASK) == NV_RX2_CHECKSUM_IP_TCP) || /*ip and tcp */
				    ((flags & NV_RX2_CHECKSUMMASK) == NV_RX2_CHECKSUM_IP_UDP))   /*ip and udp */
					skb->ip_summed = CHECKSUM_UNNECESSARY;
			} else {
				dev_kfree_skb(skb);
				goto next_pkt;
			}
		}
		/* got a valid packet - forward it to the network core */
		skb_put(skb, len);
		skb->protocol = eth_type_trans(skb, dev);
		dprintk(KERN_DEBUG "%s: nv_rx_process: %d bytes, proto %d accepted.\n",
					dev->name, len, skb->protocol);
#ifdef CONFIG_FORCEDETH_NAPI
		netif_receive_skb(skb);
#else
		netif_rx(skb);
#endif
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += len;
next_pkt:
		if (unlikely(np->get_rx.orig++ == np->last_rx.orig))
			np->get_rx.orig = np->first_rx.orig;
		if (unlikely(np->get_rx_ctx++ == np->last_rx_ctx))
			np->get_rx_ctx = np->first_rx_ctx;

		rx_work++;
	}

	return rx_work;
}

static int nv_rx_process_optimized(struct net_device *dev, int limit)
{
	struct fe_priv *np = netdev_priv(dev);
	u32 flags;
	u32 vlanflags = 0;
	int rx_work = 0;
	struct sk_buff *skb;
	int len;

	while((np->get_rx.ex != np->put_rx.ex) &&
	      !((flags = le32_to_cpu(np->get_rx.ex->flaglen)) & NV_RX2_AVAIL) &&
	      (rx_work < limit)) {

		dprintk(KERN_DEBUG "%s: nv_rx_process_optimized: flags 0x%x.\n",
					dev->name, flags);

		/*
		 * the packet is for us - immediately tear down the pci mapping.
		 * TODO: check if a prefetch of the first cacheline improves
		 * the performance.
		 */
		pci_unmap_single(np->pci_dev, np->get_rx_ctx->dma,
				np->get_rx_ctx->dma_len,
				PCI_DMA_FROMDEVICE);
		skb = np->get_rx_ctx->skb;
		np->get_rx_ctx->skb = NULL;

		{
			int j;
			dprintk(KERN_DEBUG "Dumping packet (flags 0x%x).",flags);
			for (j=0; j<64; j++) {
				if ((j%16) == 0)
					dprintk("\n%03x:", j);
				dprintk(" %02x", ((unsigned char*)skb->data)[j]);
			}
			dprintk("\n");
		}
		/* look at what we actually got: */
		if (likely(flags & NV_RX2_DESCRIPTORVALID)) {
			len = flags & LEN_MASK_V2;
			if (unlikely(flags & NV_RX2_ERROR)) {
				if ((flags & NV_RX2_ERROR_MASK) == NV_RX2_ERROR4) {
					len = nv_getlen(dev, skb->data, len);
					if (len < 0) {
						dev_kfree_skb(skb);
						goto next_pkt;
					}
				}
				/* framing errors are soft errors */
				else if ((flags & NV_RX2_ERROR_MASK) == NV_RX2_FRAMINGERR) {
					if (flags & NV_RX2_SUBSTRACT1) {
						len--;
					}
				}
				/* the rest are hard errors */
				else {
					dev_kfree_skb(skb);
					goto next_pkt;
				}
			}

			if (((flags & NV_RX2_CHECKSUMMASK) == NV_RX2_CHECKSUM_IP_TCP) || /*ip and tcp */
			    ((flags & NV_RX2_CHECKSUMMASK) == NV_RX2_CHECKSUM_IP_UDP))   /*ip and udp */
				skb->ip_summed = CHECKSUM_UNNECESSARY;

			/* got a valid packet - forward it to the network core */
			skb_put(skb, len);
			skb->protocol = eth_type_trans(skb, dev);
			prefetch(skb->data);

			dprintk(KERN_DEBUG "%s: nv_rx_process_optimized: %d bytes, proto %d accepted.\n",
				dev->name, len, skb->protocol);

			if (likely(!np->vlangrp)) {
#ifdef CONFIG_FORCEDETH_NAPI
				netif_receive_skb(skb);
#else
				netif_rx(skb);
#endif
			} else {
				vlanflags = le32_to_cpu(np->get_rx.ex->buflow);
				if (vlanflags & NV_RX3_VLAN_TAG_PRESENT) {
#ifdef CONFIG_FORCEDETH_NAPI
					vlan_hwaccel_receive_skb(skb, np->vlangrp,
								 vlanflags & NV_RX3_VLAN_TAG_MASK);
#else
					vlan_hwaccel_rx(skb, np->vlangrp,
							vlanflags & NV_RX3_VLAN_TAG_MASK);
#endif
				} else {
#ifdef CONFIG_FORCEDETH_NAPI
					netif_receive_skb(skb);
#else
					netif_rx(skb);
#endif
				}
			}

			dev->stats.rx_packets++;
			dev->stats.rx_bytes += len;
		} else {
			dev_kfree_skb(skb);
		}
next_pkt:
		if (unlikely(np->get_rx.ex++ == np->last_rx.ex))
			np->get_rx.ex = np->first_rx.ex;
		if (unlikely(np->get_rx_ctx++ == np->last_rx_ctx))
			np->get_rx_ctx = np->first_rx_ctx;

		rx_work++;
	}

	return rx_work;
}

static void set_bufsize(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);

	if (dev->mtu <= ETH_DATA_LEN)
		np->rx_buf_sz = ETH_DATA_LEN + NV_RX_HEADERS;
	else
		np->rx_buf_sz = dev->mtu + NV_RX_HEADERS;
}

/*
 * nv_change_mtu: dev->change_mtu function
 * Called with dev_base_lock held for read.
 */
static int nv_change_mtu(struct net_device *dev, int new_mtu)
{
	struct fe_priv *np = netdev_priv(dev);
	int old_mtu;

	if (new_mtu < 64 || new_mtu > np->pkt_limit)
		return -EINVAL;

	old_mtu = dev->mtu;
	dev->mtu = new_mtu;

	/* return early if the buffer sizes will not change */
	if (old_mtu <= ETH_DATA_LEN && new_mtu <= ETH_DATA_LEN)
		return 0;
	if (old_mtu == new_mtu)
		return 0;

	/* synchronized against open : rtnl_lock() held by caller */
	if (netif_running(dev)) {
		u8 __iomem *base = get_hwbase(dev);
		/*
		 * It seems that the nic preloads valid ring entries into an
		 * internal buffer. The procedure for flushing everything is
		 * guessed, there is probably a simpler approach.
		 * Changing the MTU is a rare event, it shouldn't matter.
		 */
		nv_disable_irq(dev);
		nv_napi_disable(dev);
		netif_tx_lock_bh(dev);
		netif_addr_lock(dev);
		spin_lock(&np->lock);
		/* stop engines */
		nv_stop_rxtx(dev);
		nv_txrx_reset(dev);
		/* drain rx queue */
		nv_drain_rxtx(dev);
		/* reinit driver view of the rx queue */
		set_bufsize(dev);
		if (nv_init_ring(dev)) {
			if (!np->in_shutdown)
				mod_timer(&np->oom_kick, jiffies + OOM_REFILL);
		}
		/* reinit nic view of the rx queue */
		writel(np->rx_buf_sz, base + NvRegOffloadConfig);
		setup_hw_rings(dev, NV_SETUP_RX_RING | NV_SETUP_TX_RING);
		writel( ((np->rx_ring_size-1) << NVREG_RINGSZ_RXSHIFT) + ((np->tx_ring_size-1) << NVREG_RINGSZ_TXSHIFT),
			base + NvRegRingSizes);
		pci_push(base);
		writel(NVREG_TXRXCTL_KICK|np->txrxctl_bits, get_hwbase(dev) + NvRegTxRxControl);
		pci_push(base);

		/* restart rx engine */
		nv_start_rxtx(dev);
		spin_unlock(&np->lock);
		netif_addr_unlock(dev);
		netif_tx_unlock_bh(dev);
		nv_napi_enable(dev);
		nv_enable_irq(dev);
	}
	return 0;
}

static void nv_copy_mac_to_hw(struct net_device *dev)
{
	u8 __iomem *base = get_hwbase(dev);
	u32 mac[2];

	mac[0] = (dev->dev_addr[0] << 0) + (dev->dev_addr[1] << 8) +
			(dev->dev_addr[2] << 16) + (dev->dev_addr[3] << 24);
	mac[1] = (dev->dev_addr[4] << 0) + (dev->dev_addr[5] << 8);

	writel(mac[0], base + NvRegMacAddrA);
	writel(mac[1], base + NvRegMacAddrB);
}

/*
 * nv_set_mac_address: dev->set_mac_address function
 * Called with rtnl_lock() held.
 */
static int nv_set_mac_address(struct net_device *dev, void *addr)
{
	struct fe_priv *np = netdev_priv(dev);
	struct sockaddr *macaddr = (struct sockaddr*)addr;

	if (!is_valid_ether_addr(macaddr->sa_data))
		return -EADDRNOTAVAIL;

	/* synchronized against open : rtnl_lock() held by caller */
	memcpy(dev->dev_addr, macaddr->sa_data, ETH_ALEN);

	if (netif_running(dev)) {
		netif_tx_lock_bh(dev);
		netif_addr_lock(dev);
		spin_lock_irq(&np->lock);

		/* stop rx engine */
		nv_stop_rx(dev);

		/* set mac address */
		nv_copy_mac_to_hw(dev);

		/* restart rx engine */
		nv_start_rx(dev);
		spin_unlock_irq(&np->lock);
		netif_addr_unlock(dev);
		netif_tx_unlock_bh(dev);
	} else {
		nv_copy_mac_to_hw(dev);
	}
	return 0;
}

/*
 * nv_set_multicast: dev->set_multicast function
 * Called with netif_tx_lock held.
 */
static void nv_set_multicast(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	u8 __iomem *base = get_hwbase(dev);
	u32 addr[2];
	u32 mask[2];
	u32 pff = readl(base + NvRegPacketFilterFlags) & NVREG_PFF_PAUSE_RX;

	memset(addr, 0, sizeof(addr));
	memset(mask, 0, sizeof(mask));

	if (dev->flags & IFF_PROMISC) {
		pff |= NVREG_PFF_PROMISC;
	} else {
		pff |= NVREG_PFF_MYADDR;

		if (dev->flags & IFF_ALLMULTI || dev->mc_list) {
			u32 alwaysOff[2];
			u32 alwaysOn[2];

			alwaysOn[0] = alwaysOn[1] = alwaysOff[0] = alwaysOff[1] = 0xffffffff;
			if (dev->flags & IFF_ALLMULTI) {
				alwaysOn[0] = alwaysOn[1] = alwaysOff[0] = alwaysOff[1] = 0;
			} else {
				struct dev_mc_list *walk;

				walk = dev->mc_list;
				while (walk != NULL) {
					u32 a, b;
					a = le32_to_cpu(*(__le32 *) walk->dmi_addr);
					b = le16_to_cpu(*(__le16 *) (&walk->dmi_addr[4]));
					alwaysOn[0] &= a;
					alwaysOff[0] &= ~a;
					alwaysOn[1] &= b;
					alwaysOff[1] &= ~b;
					walk = walk->next;
				}
			}
			addr[0] = alwaysOn[0];
			addr[1] = alwaysOn[1];
			mask[0] = alwaysOn[0] | alwaysOff[0];
			mask[1] = alwaysOn[1] | alwaysOff[1];
		} else {
			mask[0] = NVREG_MCASTMASKA_NONE;
			mask[1] = NVREG_MCASTMASKB_NONE;
		}
	}
	addr[0] |= NVREG_MCASTADDRA_FORCE;
	pff |= NVREG_PFF_ALWAYS;
	spin_lock_irq(&np->lock);
	nv_stop_rx(dev);
	writel(addr[0], base + NvRegMulticastAddrA);
	writel(addr[1], base + NvRegMulticastAddrB);
	writel(mask[0], base + NvRegMulticastMaskA);
	writel(mask[1], base + NvRegMulticastMaskB);
	writel(pff, base + NvRegPacketFilterFlags);
	dprintk(KERN_INFO "%s: reconfiguration for multicast lists.\n",
		dev->name);
	nv_start_rx(dev);
	spin_unlock_irq(&np->lock);
}

static void nv_update_pause(struct net_device *dev, u32 pause_flags)
{
	struct fe_priv *np = netdev_priv(dev);
	u8 __iomem *base = get_hwbase(dev);

	np->pause_flags &= ~(NV_PAUSEFRAME_TX_ENABLE | NV_PAUSEFRAME_RX_ENABLE);

	if (np->pause_flags & NV_PAUSEFRAME_RX_CAPABLE) {
		u32 pff = readl(base + NvRegPacketFilterFlags) & ~NVREG_PFF_PAUSE_RX;
		if (pause_flags & NV_PAUSEFRAME_RX_ENABLE) {
			writel(pff|NVREG_PFF_PAUSE_RX, base + NvRegPacketFilterFlags);
			np->pause_flags |= NV_PAUSEFRAME_RX_ENABLE;
		} else {
			writel(pff, base + NvRegPacketFilterFlags);
		}
	}
	if (np->pause_flags & NV_PAUSEFRAME_TX_CAPABLE) {
		u32 regmisc = readl(base + NvRegMisc1) & ~NVREG_MISC1_PAUSE_TX;
		if (pause_flags & NV_PAUSEFRAME_TX_ENABLE) {
			u32 pause_enable = NVREG_TX_PAUSEFRAME_ENABLE_V1;
			if (np->driver_data & DEV_HAS_PAUSEFRAME_TX_V2)
				pause_enable = NVREG_TX_PAUSEFRAME_ENABLE_V2;
			if (np->driver_data & DEV_HAS_PAUSEFRAME_TX_V3) {
				pause_enable = NVREG_TX_PAUSEFRAME_ENABLE_V3;
				/* limit the number of tx pause frames to a default of 8 */
				writel(readl(base + NvRegTxPauseFrameLimit)|NVREG_TX_PAUSEFRAMELIMIT_ENABLE, base + NvRegTxPauseFrameLimit);
			}
			writel(pause_enable,  base + NvRegTxPauseFrame);
			writel(regmisc|NVREG_MISC1_PAUSE_TX, base + NvRegMisc1);
			np->pause_flags |= NV_PAUSEFRAME_TX_ENABLE;
		} else {
			writel(NVREG_TX_PAUSEFRAME_DISABLE,  base + NvRegTxPauseFrame);
			writel(regmisc, base + NvRegMisc1);
		}
	}
}

/**
 * nv_update_linkspeed: Setup the MAC according to the link partner
 * @dev: Network device to be configured
 *
 * The function queries the PHY and checks if there is a link partner.
 * If yes, then it sets up the MAC accordingly. Otherwise, the MAC is
 * set to 10 MBit HD.
 *
 * The function returns 0 if there is no link partner and 1 if there is
 * a good link partner.
 */
static int nv_update_linkspeed(struct net_device *dev)
{
	struct fe_priv *np = netdev_priv(dev);
	u8 __iomem *base = get_hwbase(dev);
	int adv = 0;
	int lpa = 0;
	int adv_lpa, adv_pause, lpa_pause;
	int newls = np->linkspeed;
	int newdup = np->duplex;
	int mii_status;
	int retval = 0;
	u32 control_1000, status_1000, phyreg, pause_flags, txreg;
	u32 txrxFlags = 0;
	u32 phy_exp;

	/* BMSR_LSTATUS is latched, read it twice:
	 * we want the current value.
	 */
	mii_rw(dev, np->phyaddr, MII_BMSR, MII_READ);
	mii_status = mii_rw(dev, np->phyaddr, MII_BMSR, MII_READ);

	if (!(mii_status & BMSR_LSTATUS)) {
		dprintk(KERN_DEBUG "%s: no link detected by phy - falling back to 10HD.\n",
				dev->name);
		newls = NVREG_LINKSPEED_FORCE|NVREG_LINKSPEED_10;
		newdup = 0;
		retval = 0;
		goto set_speed;
	}

	if (np->autoneg == 0) {
		dprintk(KERN_DEBUG "%s: nv_update_linkspeed: autoneg off, PHY set to 0x%04x.\n",
				dev->name, np->fixed_mode);
		if (np->fixed_mode & LPA_100FULL) {
			newls = NVREG_LINKSPEED_FORCE|NVREG_LINKSPEED_100;
			newdup = 1;
		} else if (np->fixed_mode & LPA_100HALF) {
			newls = NVREG_LINKSPEED_FORCE|NVREG_LINKSPEED_100;
			newdup = 0;
		} else if (np->fixed_mode & LPA_10FULL) {
			newls = NVREG_LINKSPEED_FORCE|NVREG_LINKSPEED_10;
			newdup = 1;
		} else {
			newls = NVREG_LINKSPEED_FORCE|NVREG_LINKSPEED_10;
			newdup = 0;
		}
		retval = 1;
		goto set_speed;
	}
	/* check auto negotiation is complete */
	if (!(mii_status & BMSR_ANEGCOMPLETE)) {
		/* still in autonegotiation - configure nic for 10 MBit HD and wait. */
		newls = NVREG_LINKSPEED_FORCE|NVREG_LINKSPEED_10;
		newdup = 0;
		retval = 0;
		dprintk(KERN_DEBUG "%s: autoneg not completed - falling back to 10HD.\n", dev->name);
		goto set_speed;
	}

	adv = mii_rw(dev, np->phyaddr, MII_ADVERTISE, MII_READ);
	lpa = mii_rw(dev, np->phyaddr, MII_LPA, MII_READ);
	dprintk(KERN_DEBUG "%s: nv_update_linkspeed: PHY advertises 0x%04x, lpa 0x%04x.\n",
				dev->name, adv, lpa);

	retval = 1;
	if (np->gigabit == PHY_GIGABIT) {
		control_1000 = mii_rw(dev, np->phyaddr, MII_CTRL1000, MII_READ);
		status_1000 = mii_rw(dev, np->phyaddr, MII_STAT1000, MII_READ);

		if ((control_1000 & ADVERTISE_1000FULL) &&
			(status_1000 & LPA_1000FULL)) {
			dprintk(KERN_DEBUG "%s: nv_update_linkspeed: GBit ethernet detected.\n",
				dev->name);
			newls = NVREG_LINKSPEED_FORCE|NVREG_LINKSPEED_1000;
			newdup = 1;
			goto set_speed;
		}
	}

	/* FIXME: handle parallel detection properly */
	adv_lpa = lpa & adv;
	if (adv_lpa & LPA_100FULL) {
		newls = NVREG_LINKSPEED_FORCE|NVREG_LINKSPEED_100;
		newdup = 1;
	} else if (adv_lpa & LPA_100HALF) {
		newls = NVREG_LINKSPEED_FORCE|NVREG_LINKSPEED_100;
		newdup = 0;
	} else if (adv_lpa & LPA_10FULL) {
		newls = NVREG_LINKSPEED_FORCE|NVREG_LINKSPEED_10;
		newdup = 1;
	} else if (adv_lpa & LPA_10HALF) {
		newls = NVREG_LINKSPEED_FORCE|NVREG_LINKSPEED_10;
		newdup = 0;
	} else {
		dprintk(KERN_DEBUG "%s: bad ability %04x - falling back to 10HD.\n", dev->name, adv_lpa);
		newls = NVREG_LINKSPEED_FORCE|NVREG_LINKSPEED_10;
		newdup = 0;
	}

set_speed:
	if (np->duplex == newdup && np->linkspeed == newls)
		return retval;

	dprintk(KERN_INFO "%s: changing link setting from %d/%d to %d/%d.\n",
			dev->name, np->linkspeed, np->duplex, newls, newdup);

	np->duplex = newdup;
	np->linkspeed = newls;

	/* The transmitter and receiver must be restarted for safe update */
	if (readl(base + NvRegTransmitterControl) & NVREG_XMITCTL_START) {
		txrxFlags |= NV_RESTART_TX;
		nv_stop_tx(dev);
	}
	if (readl(base + NvRegReceiverControl) & NVREG_RCVCTL_START) {
		txrxFlags |= NV_RESTART_RX;
		nv_stop_rx(dev);
	}

	if (np->gigabit == PHY_GIGABIT) {
		phyreg = readl(base + NvRegSlotTime);
		phyreg &= ~(0x3FF00);
		if (((np->linkspeed & 0xFFF) == NVREG_LINKSPEED_10) ||
		    ((np->linkspeed & 0xFFF) == NVREG_LINKSPEED_100))
			phyreg |= NVREG_SLOTTIME_10_100_FULL;
		else if ((np->linkspeed & 0xFFF) == NVREG_LINKSPEED_1000)
			phyreg |= NVREG_SLOTTIME_1000_FULL;
		writel(phyreg, base + NvRegSlotTime);
	}

	phyreg = readl(base + NvRegPhyInterface);
	phyreg &= ~(PHY_HALF|PHY_100|PHY_1000);
	if (np->duplex == 0)
		phyreg |= PHY_HALF;
	if ((np->linkspeed & NVREG_LINKSPEED_MASK) == NVREG_LINKSPEED_100)
		phyreg |= PHY_100;
	else if ((np->linkspeed & NVREG_LINKSPEED_MASK) == NVREG_LINKSPEED_1000)
		phyreg |= PHY_1000;
	writel(phyreg, base + NvRegPhyInterface);

	phy_exp = mii_rw(dev, np->phyaddr, MII_EXPANSION, MII_READ) & EXPANSION_NWAY; /* autoneg capable */
	if (phyreg & PHY_RGMII) {
		if ((np->linkspeed & NVREG_LINKSPEED_MASK) == NVREG_LINKSPEED_1000) {
			txreg = NVREG_TX_DEFERRAL_RGMII_1000;
		} else {
			if (!phy_exp && !np->duplex && (np->driver_data & DEV_HAS_COLLISION_FIX)) {
				if ((np->linkspeed & NVREG_LINKSPEED_MASK) == NVREG_LINKSPEED_10)
					txreg = NVREG_TX_DEFERRAL_RGMII_STRETCH_10;
				else
					txreg = NVREG_TX_DEFERRAL_RGMII_STRETCH_100;
			} else {
				txreg = NVREG_TX_DEFERRAL_RGMII_10_100;
			}
		}
	} else {
		if (!phy_exp && !np->duplex && (np->driver_data & DEV_HAS_COLLISION_FIX))
			txreg = NVREG_TX_DEFERRAL_MII_STRETCH;
		else
			txreg = NVREG_TX_DEFERRAL_DEFAULT;
	}
	writel(txreg, base + NvRegTxDeferral);

	if (np->desc_ver == DESC_VER_1) {
		txreg = NVREG_TX_WM_DESC1_DEFAULT;
	} else {
		if ((np->linkspeed & NVREG_LINKSPEED_MASK) == NVREG_LINKSPEED_1000)
			txreg = NVREG_TX_WM_DESC2_3_1000;
		else
			txreg = NVREG_TX_WM_DESC2_3_DEFAULT;
	}
	writel(txreg, base + NvRegTxWatermark);

	writel(NVREG_MISC1_FORCE | ( np->duplex ? 0 : NVREG_MISC1_HD),
		base + NvRegMisc1);
	pci_push(base);
	writel(np->linkspeed, base + NvRegLinkSeth:);
	pci_push(ther);

	pause_flags = 0;
	/* setup ss co frame */
	if 
 * fduplex != 0) {
	 a cleanrautoneg &&  * fss controlle& NV_PAUSEFRAME_AUTONEGmentat	adv_ This = adv & (ADVERTISE docum_CAP| ger
 *      and ASYMIDIA		lpal-Daniel lpalfinLPA   and AndreNVIDIA mar
 * NVIIDIAswitch (Carl-Daniten by chernger
 *      and And:IDIA a cl, nForce a&re trademarCAPten by 	ess controlle|=red documentatiRX_ENABLEVIDIAation base      engineered documentatiTX_REQ) * Cop* Copyright (C) 2003,4,5 ManfrTd Spraul
 * Co} * CobreakVIDIAVIDIA Corporation in 
 *  United States and o==r NVIDIA marks are trademarks orport)
.
 *
 * Copyright (C) 2003,4,5 Manfrel Hailfinger (invalid MAC handling, insane
 *		IRQ Andrew de Quincey.
 *
 *  United States and other
 * countrieyright (c) 2004,2005,2006,2 2003,4,5 Manfred Spraul
 * Copyright (C) 2004 Andrew de Quincey (wol support)
 * Copyright (C) 2004 Carl-Daniel Hailfinger (invali, bigendian fixese trademarks oryright (c) 2004,2005,2006,2007,2008,2009 ed Spraul
 * Coinvalid MAC handinva} elseen by ss controllersht (C) 2004 AndVIDI}
ILITnv_updatel-Dani(dev,: Thisntrollr rega cltxrxFngineered RESTART_TXbutenv_start_tx PARIDIASee the
 * GNU General PublRc License for rore deta
	return retval;
}

static void nv_orcechange(struct net_device *e de
{s a cle FITNESS orcedeth:re dementation !netif_carrier_okation, Inc.	Temple Place, Snre detai		printk(KERN_INFO "%s: orce up.\n", dev->nama acBostv_the
_gatA PARTIfalia acdware  copy of the GNBILITY even the a cleemple Place, Suite 330, Boston, MA  02111-ff307  USA
 *
 * Known bugs:
 * We suspdownt that on some hardware no TX done intetruts are generopd.
 * This means License
 * along with _irqrogram; if not, write to theu8 __iomem *thern= get_hwtherre detaiu32 miienseGNU rrupts, = readlmedianet drivMIIStatuOSE.	writel(NVREG__TIMTAT_LINKCHANGE Ethernet driv_TIMERIRQ frod*
 * Known bugs:
 * We suspthis p irq, enseus 0x%xt that on some ,errupts,SE.  See n you ca& driver_data flags.
 * DEVopyrig with this pre detaim you on saneDEBUGdware, only generanotification donet that on some harLicense
 * along wmsi_workaroundrogram; fe_priv *npo th *
 *Need to toggle the msiatin mask within.h>
#ethernett onice,
	 * oe.h>wise, future interrupts willdule be detected.linuis a cleanr>
#iengineered MSI SpraulDmentat hardware reliably  * fther regim the d0_NEED_TIMERIRQ SIIrqMaskhis mm the driver_dSI_VECTOR_0ool.h>
#.h>
#include <linux/mii.h>
#IRQMask, or inlinde <lng wthis p_ <linux/s_modprogram; if not, write t,lude totalincluo theux/interrupt.h>
#i>
#ietdevupt.h the GNU a cloptimiz#inclh>

#ixesNV_OPTIMIZATION_MODE_DYNAMICmentation <asm/io.h> >tk		ine dpr_THRESHOL#includ*
 *transiincluto pollEtherde <linux/schinlo		 * fquiet_councan .
 *ation baseirq<linu!ntk	ver_IRQMASK_CPUes.
 *
 
#define DEVNEED_TIMERIRQ      
 * CoGeneral1Y; without even the ia cleanr * Hardware <le (0)
#endiMAX_QUIET_COUNT    0x000000 * Hardware++Y; wit even the i*
 *reached a period of low activity, tered
ine D  64
#der tx/rx packetWORK_PER_LOOP  64

 */

#define DEV_NEED_TIMERIRQ   THROUGHPUttings. R000001  /* set the timer irq           
 * Cophe irq mask * ANY WthoutITY oGeneral0include <linirqGenera_e <linicet arude foo,* alon*data
#include <aif not, write t = rogram; if not, writ) defi;nclude <asm/uaccess.h>
#include <asm/systr hardware reliably generates tx done#ifndef CONFIG_FORCEDETH_NAPI
	ude <asm/io.h> accessude looprdware acces#endif
AME			"forcedeth"

#includ rx checks that on some harem.h>
!h>
#include <linux/ethtod SpraulDn, Inc. * feventlersremove
 * DEV_NEEDIrqMERIRQ froinclude */
#define Ethernet driv1      0x0000 */
#define*/
#define DEV_HAS_STATISTICS_VMSIX1      0x0000200  /* device supports hw statix0000600  /* devi}AME			"forcedeth"

#includirq: %08xerfluous timer i*/
#defineetails.
000100define rse
 *fine DEopyriGeneralIRQ_NONE    i.h>
#include <linnp    #ifevice supports MSI */
#defin cleapi_s_LARulruptep(& * ffinen, Inc./*
	inuxDisable fure.h>x000's (msix>
#inendevidux/et      2000   64x/skbuff.h>
#include <ux/mii.h>
#i__fine DEV_HAS_UNIT        DEV_
#even
	do
ht (cude _MSI_X     /
#de(_MSI_X nv_rx_process PARTIRX_WORK_PER_LOOP)330, Bos
#deunlikelyee Salloc of the 330, Bos	spin_lockUNIT  0100s are ., 59 Tine n_shutd, cport)
 mod_timerUNIT  oom_kick, jiffies + OOM_REFILLice sup  0x0un010000  /* device su      
1   0x0010000  /* device s_MSI_+ 0x00tx_de < PARTITsupports tx collISIO  /* device supports tx nc., 59 o.h>
#alid MAC h
		EV_HAS_MSI_+=_HAS_ DEV_     0x000mer i}
	while (     0x0000< maxa-mapping.ho.h>
     0x0<linux/dma-mapping.h>

#iT)
 * asm/io.h>
   0x000* Note:newlude <linuCORRECT_MACAfine DEV_HADR    0x0004000  /* devicramex */
#define Dion 3 */
#deED_TIMERIags.
o limit  0x0010000  /* device srx packet ar* This m  /* device supports tx p}0  /* device supporneede
 *  */
#vers */
_after(AUSEFRAorts hpacke */
outAME_TX_VED_PHY_INIT_FIX      0x0400000 define DRV_NAM0  /* device supports tx pa * forces specia =PAUSEFRAME_gs.
_TIMEOdefinnd */
#define DEV_NEts gear mode */
#deRECOVER_ERROR power up workaround */
#define/* d* devic <linux/sch  64he nicrevs * 0x0000100  /* device supports power sav/
#dCT_MACADDR    0x0004000  /* deviceer */
0200  /* deviEV_HAS_GEAR_MODE          0x0200A nForce media accepports tx pause frames0, Bost_NEEic_defiet a>
#inclfine DE are gp->recover_error = mask *on 1 */
#define0x0020
#_PAUSEFRAME_POLL_WAIThis meaneeds specific phy workaround/* devie supports MSI-X */
#define DEV_HAS completed_POWER_CNTRL       T_EXTENDED HANDLEDinclu/**
 * All _
#if 0
ed funcincls are uRX_Wto help incre * DperformanceVREG(reduce CPU an_WOROK|NVREthroughput). TheyRR|N descripter versnclu3,VREGPUT	iler dire0000 devREG_
#definmemory ac /* es.
revsice supports tx and rx checks_TX_ALL		(um offloads */
#define DEV_HAS_VLAN               0x0000020  /* device supports vlan tagging and striping */
#define DEV_HAS_MSI                0x0000040  /* device supports MSI */
#define DEV_HAS_MSI_X              0x0000080  /* device supports MSI-X */
#define DEV_HAS_TX_ALL		(_POWER_CNTRL        0x0000100  /* device supports power savings */
#define DEV_HAS_STATISTICS_V1      0x0000200  /* device supports hw statistics version 1 */
#define DEV_HAS_STATISTICS_V2      0x0000600  /* device supports hw statistics version 2 */
#define DEV_HAS_STATISTICS_V3      0x0000e00  /* device supports hw statistics version 3 */
#define DEV_HAS_TEST_EXTENDED      0x0001000  /* device supports extended diagnostic test */
#define DEV_HAS_MGMT_UNIT          0x0002000  /* device supports management unit */
#define DEV_HAS_CORRECT_MACADDR    0x0004000  /* device supports correct mac address oder */
#define DEV_HAS_COLLISION_FIX      0x0008000  /* LINK|NVREG_evice supports tx collision fix */
#define DEV_HAS_PAU_MGMT_SEMA_MASAME_TX_V1   0x0010000  /* device supports tx pause frames version 1 */
#define DEV_HAS_PAUSEFRAME_TX_V2   0x0020000  /* device supports tx pause frames version 2 */
#define DEV_HAS_PAUSEFRAME_T_MGMT_SEMA_MASK	 0x0040000  /* device supports tx pause frames version 3 */
#define DEV_NEED_TX_LIMIT          0x0080000  /* device needs to limit tx */
#define DEV_NEED_TX_LIMIT2         0x0180000  /* device needs to limit tx, expect for some revs */
#define DEV_HAS_GEAR_MODE          0x0200000  /* device supports gear mode */
#define DEV_NEED_PHY_INIT_FIX      0x0400000  /* device needs specific phy workaround */
#define DEV_NEED_LOW_POWER_FIX     0x0800000  /* device needs special power up workaround */
#define DEV_NEED_MSI_FIX           0x1000000  /* device needs msi workaround */

enum {
	NvRegIrqStatus = 0x000,
#define NVREG_IRQSTAT_MIIEVENT	0x040
#define NVREG_IRQSTAT_MASK		0x83ff
	NvRegIrqMask = 0x004,
#define NVREG_IRQ_RX_ERROR		0x0001
#define NVREG_IRQ_RX			0x0002
#define NVREG_IRQ_RX_NOBUF		0x0004
#define NVREG_IRQ_TX_ERR		0x0008
#define NVREG_IRQ_TX_OK			0x0010
#define NVREG_IRQ_TIMER			0x0020
#define NVREG_IRQ_LINK			0x0040
#define NVREG_IRQ_RX_FORCED		0x0080
#define NVREG_IRQ_TX_FORCED		0x0100
#define NVREG_IRQ_RECOVER__ERROR		0x8200
#define NVREG_IRQMASK_THROUG_TX_ALL		(NPUT	0x00df
#define NVREG_IRQMASK_CPU		0x0060
#defin		(NVREG_IRQ_TIMER|NVREG_IRQ_Ltxum offloads */
#define DEV_HAS_VLAN               0x0000020  /* device supports vlan tagging and striping */
#define DEV_HAS_MSI                0x0000040   intedefine       i	NvRnsigned long RCHANTAine NVREG_TX_DEFERRAL_RGMII_1000		0x1t /* device suppED_TXfor (i=0; ; i++RQ_TIMEV_HAS_STATISTICS_V2      0x0000600  /* dr mode */
#deTX_ALL000200  /* KA_NONE		0xffffftistics version 2 */
#define DEME			"forcedeth"

#includtxx0000e00  /* device supporw statistiEG_IRQ_ 3 */
#define DEV_HAS_TESdefine DEV_  0x00100et asavect mac0100,tAddrA  0x0400_XMITCTL_DATA_START	0x00100000
#define NVREG_XMITCTL_DAet arestorL_DEFAULT			0x7000000x100000
#define i >x */
#define DEV_NEERQ_TIMEe NVREG_BKOFFCTRL_DEFAULT			0x70000000
3ff
	NvRegIrqMask = 0x004,
#define NVREinclude <linuxstMaskB = 0xBC,
#define ux/mii.h>
#iNVREG_IRQ_TX_OK			0xx0010
#define NVREG_IRQ_TIMEER			0x0020
#definC) 20_NONE		0xffffffff
Q_RX_FORCED		0x0080
#define NVREG_IRQ_TX_FORCED		0x ANY Wdefine NVREG_BKOFFCTRL_SELECT			24
#defineA
 *
 * Known b	NvRegPhyIntoo many iter#incls (%d) inTADDRA_FORCE	0perfluous timer idresslid MAC hanrameV_HAS_STATISTICS_V3      0xADDRA_FORCE	0fine NVREG_TX_DEFERRAL_RGMII_STRETCH_10	RETVAL(dres070fs extended diagnostic test *_vlan.h>
nd rx ine defirogram; iine Dgram; *DEFAclude budget
#include <asm/uaccess.h>
containw timefineg a /interrupt.h,e DEV_rts vlan tLAN               eanroev_DEFAULT	0x16
	NvRegMacAddrA = 0xA8,
	NvRvRegMulticastAddrA = DEV_retcode
#defintxEV_NE, r_IDLE	     0x00nvLINK|NVREG_np power up workarKOFFCTRL_DEFAULT			0x70000000
L_IDLE	  0x00FRAME_TX_V3  	0x0tx_ring_sizts aredefine NVREG_BKOFFCTRL_SELECT			24
#define NV0008
#d  0x0008000  /* devicene NVREF)
	e NVREG  0x00_HAS_PAUSEFRAion 1 */
#defi0
#define NVREG_TXRXCTL_RXCHECK	0x0400
#define NVREG_TXRXCT_MGMT_SEMA_MASK	_1	0
#define NVREG_TXRXCTL_DESC_2	0x002100
#define NVREG_TXRXCTL_DESC_3	0xc02200
#define _MGMT_SEMA_MASK	TXRXCTL_VLANSTRIP 0x00040
#definx0
#define NVRE200000  /* e NVREG	0x0010
#define NVREG_TXRXCTL_RXCHECK	0x0400
ports tx pause frames veron 1 */
#define DEV_HAS_PAUSEFRAME_TX_V2   0x00200efine NVREG_LINKSPEED_10	1000
#define NVREGrameIT2         0x0180000  /* devicSC_3	0x+x0008
#dED_TX_LIMdevice supports gear mode */
#define DEV_NEED_PHY_INKOFFCTRL_DEFAULT			0x70000000
#de000  /* device needs specifi0x00010000
	NvRegMIIStatus = 0x180 */
#define DEV_NEED_LOW_POWER_FIX     0x0800000  /* device needs special power up workarx0007
#define NVREG_MIISTAT_MASK_ALL	ED_MSI_FIX           0x100_LINKSPEED_10	1000
#define NVREG_ needs msi workaround */

enum {
	NvRegIrqStatus = 0x000,
#define NVREG_IRQSTAT_MIIEVENT	0x040
#define NVREG_IRQe NVREG_TX_PAUSEFRAME_ENABLE_V3	0x09f00880
	NvRegTxQ_TIMER			0x0020
#define NVREG_IRQ_LINK			0x0040
#define NVREG_IRQ_RX_FORCED		0x0080
#define NVREG_IRQ_TX_FORCED		0x0100
#define NVefine NVREG_ADAPTCTL_RUNNING	0x100ine PUT	0x00efineL_VLANSTeral 008
#defAME_ENABLESC_3	0x<RAME_ENAlimit txre-it */
rqMask = 0x2000 nagement unit */
#d
#deast revs *
	NvRegTxBroadcast = 00200  /* deviEV_HAS_GEAR_MODE          0x020000x1a8,
	NvRegWakeUR_ERROR		efine NVREG_TX_DEFERRAL_RGMII_SrRETCH_100	0x16300f
#define NVREG_TX_DEFERRAL_MII_STRETCH		0x152000
	NvRegRxDeferral = 0xA4,
#define NVREG_RX_DEFERRAL_DEFAULT	0x16
	NvRegMacAddrA = 0xA8,
	NvRegMacAddrB = 0xAC,
	NvRegMulticastAddrA = 0xB0,
#define NVREG_MCASTADDRA_FORCEr0x01
	NvRegMulticastAddrB = 0xB4,
	NvRegMulticastMaskA = 0xB8,
#define NVREG_MCASTMASKA_NONE		0Rffffffff
	NvRegMulticastManitVer0xBC,
#define NVREG_MCASTMASKB_NONE		0xffff

	NvRegPhyInrerface = 0xC0,
#define PHY_RGMII		0x10000000
	NvRegBackOffControl = 0xC4,
#defin_LIMIT2EG_XMITCTL_MGMT_SEMA_MASK	0x00000f00
#defineNVREG_XMITCTL_MGMT_SEMA_FREE	0x0
#define NVREG_XMITCTL_HOST_SEM0,
	NvRegRxRingPhysAddr = 0x104V3	0x09f00880
	NvRegTxPaussion 1 */
#define DEV_HAS_PAUSEFRAME_TX_V2   0x0020000  /* device_LINKSPEED_10	1000
#define NVREG_Lse framesREG_BKOFFCTRL_GEAR			12

	NvRegTxRingPhysAddr = 0x100,
	NvRegRxRingPhysAddr = 0x104,
	NvRegRingSizes = 0x108,
#define NVREG_RINGSZ_TXSHIFT  NVREG_MGMTUNITVERSISZ_RXSHIFT 16
	NvRegTransmitPoll = 0x10c,
#define NVREG_TRANSMITPOLL_MAC_ADDR_REV	0x0000800nitVersionnkSpeed = 0x110,
#define NVREG_LINKSPEED_FORCE 0x10000
#define NVREG_LINKSPEED_10	1000
#define NVREG_LINKSPEED_100	100
#define NVREG_LINKSPEED_1000	50
#define NVREGrLINKSPEED_MASK	(0xFFF)
	NvRegUnknownetupReg5 = 0x130,
#define NVREG_UNKSETUr5_BIT31	(1<<31)
	NvRegTxWatermark = 0x13c,
#define NVRE		(NVREG_IRQ_TIMER|NVREG_IRQ_LIe.h>ETCH_100	0x16300f
#define NVREG_TX_DEFERRAL_MII_STRETCH		0x152000
	NvRegRxDeferral = 0xA4,
#define NVREG_RX_DEFERRAL_DEFAULT	0x16
	NvRegMacAddrA = 0xA8,
	NvRegMacAddrB = 0xAC,
	NvRegMulticastAddrA = 0xB0,
#define NVREG_MCASTADDRA_FORCExUnicx01
	NvRegMulticastAddrB = 0xB4,
	NvRegMulticastMaskA = 0xB8,
#define NVREG_MCASTMASKA_NONE		0OTHERfff
	NvRegMulticastMatatus0xBC,
#define NVREG_MCASTMASKB_NONE		0xffff

	NvRegPhyInface = 0xC0,
#define PHY_RGMII		0x10000000
	NvRegBackOffControl = 0xC4,
#defin/* checknterfn VIDIAwe_HAS_LARGmax      limiand nterfsrrevs *e NVREG_BKOFFCTRL_DEFAULT			0x70000000
#define NVREG_BKOFFCTRL_SEED_MASK		0x000003ff
#define NVREG_BKOFFCTRL_SELECT			24
#define NVREG_ts gear mode */
#define PhysAddr = 0x100,
	NvRegRxRingPhysAddr = 0x104SK_ALL		0x000f
	NvRedefine NVREG_LINKSPEED_10	1000
#define NVREG_0100a cleanrED_LOW_POWER_FIX     0x0800000  /* device needs special ;
	__le32 txvlan;
	__le32 flaglen;
};

union ring_typED_MSI_FIX     define NVREG_LINKSPEED_10	1000
#define NVREG_L needs msi workaround */

enum {
	NvRegIrqSt

#definee NVREG_IRQSTAT_MIIEVENT	0x040
#dPhysAddr = 0x100,
	00  /* device su,
	NvRegRingSizes = 0x108,
#define NVREG_RINGSZ_TXSHIFT rState2 = 0x600,
#dSZ_RXSHIFT 16
	NvRegTransmitPoll = 0x10c,
#define NVREG_TRANSMITPOLL_MAC_ADDR_REV	0x0000800tatus = 0			0x0040
#define NVREG_IRQ_kSpeed = 0x110,
#define NVREG_LINKSPEED_FORCE 0x10000
#define NVREG_LIN00  /* device suRxFrameTooLodefine NVREG_POWERSTATE_D3		0x0003
	NvRegMgmtUnitControl = 0x278,
#define NVREG_MGMTUNITCONTROL_INUSE	0x20000
	NvRegTxCnt = 0x280,
	NvRegTxZRLOST	(1<<27)
#define NV_TX_LATECOLLISION	(1<<28)
#define NV_TX_UNDERFLOW		(1<<29)
#define NV_TX_ERROR		(1<<30)
#define NSpeed = 0x110,
#define NVREG_LINKSPEED_FORCE 0x10000
#define NVREG_LINKSPEED_10	1000
#define NVREG_LINKSPEED_100	100
#define NVREG_LINKSPEED_1000	50
#define NVREGxUnicINKSPEED_MASK	(0xFFF)
	NvRegUnknownSetupReg5 = 0x130,
#define NVREG_UNKSETUxUnicvRegRxFCSErr = 0x2bc,
	NvRegRxFrameAlignErr = 0x2c0,
	NvRegRxLenErr = 0x2c4,
	NvRegRtestETCH_100	0x16300f
#define NVREG_TX_DEFERRAL_MII_STRETCH		0x152000
	NvRegRxDeferral = 0xA4,
#define NVREG_RX_DEFERRAL_DEFAULT	0x16
	NvRegMacAddrA = 0xA8,
	NvRegMacAddrB  0xB0,
#define NVREG_MCASTADDRA_FORCE	est_POWER_CNTRL        0x0000100  /* device supports power savings define DEV_HAS_STATISTICS_V1      0x0r mode */
#da flaRQ  fff
	NvRegMulticastMasIMST	(1<<27)
#define cs version 1 */
#defiulticastMaskA = 0xB8,
#define NVREG_MCASTMASKA_NONE		
#define NV_RX_ERROR_MASK	(NV_RX_ERROR1|NV_RX_ERRn 2 */
#define DEV_H nForce media acSTATE2_POWERUP_MASK		0x0F15
#define NVREG_POWERSTATE2_POWERU000000
	NvRegBa_MASK	(NV_RX_ERS_TEST_EXTENDED ,
#defi0EUPFL001000  /* device suppor up workaround */
#defin tx patrine NVREG_IR supports tx pause frames #define NV_RX_CRCERR		(1<<27)
#define N_BIT31	(1<<31)
	NvRegTxWatermark = 0x13c,
#defi1#include <linux/pcseth>
#x_vector_mapinclude <asm/irq.h>
#inclinteOVERFLFRAMIN DEV_HASyour hardware reliably generates tx done  0xAC,
	Nntersixma.h>
0BSTR/* EachrqMask = 0 bit cannclumappNVREG_a x000NGERR	( (4IL		s)x/spiRX2_EMap0 represfine 
#defirst 8rqMask = 0x0REG_R1|NV_R12_ERROR2|NVlinux
#deremaining3|NV_RX2_ERROx/spinloAddrB 1<<30 i < 8B4,
	NvRegMN_FIXfine DEV>> iVAIL0x1
#definRROR		(1|=RROR_MAS<<3_VL<< 2 in the IR			0x000ATISTICS_V2      0x000V_RX) |ERROR		(0000)
#define NV_RV_PCI thenROR		(1<<30)V_RX3_VLAN_TAG_PRESENT (1<<16)
#define NV_RX3(i + 8)VLAN_TAG_MASK	(0x0000FFFF)

/* Miscelaneous hardware related defines: */
#define NV_P1I_REGSZ_VER1      	0x270
#defineERR		(1<<22)
#ine NVRrequestet arrives by chance.
 * Ifclude _ERROR1		
#include <asm/uaccess.h>
genenv_DEFERRAL_DEFAULT	0x16
	NvRegMacAddrA = 0xA8,
	Nvefine NVREG_IR 0xAC,
	ports tx and(*handler)um offloads */
#defin     0x0 NV_TXSTOPentatETUP5_D  0x00ne NV_RX_MISion 1 */
#defi_LIMIT2TXRXCTL_RESET	ine ine NV_POWERUP_DELAY	TX_ALL		(NOBUF		0x000ine NV_POWERUP_DELAYeUpFlags = 0100  /* device supports CAPraul0
#def_RX3_VLAN_TAG_PRX	10000
#define NV_MAC_RESdom.h>Sfine )B4,
	NvRegMV_RX2>
#ix_entry[i]. defa =AC,
	

#defineLE_V =  nFoit */
V_RX2ght (Ccinot, 0x14Cdriver defa,UPPATTERNS	5
#define NV_WAKEUPMASKENTRIE))ixesementateral drivight (C) 200ports power sess:
 */

#if 0
#define dprintk			printk
#else
#def          vers!YMAX	50000
#def<<24)Rfine Nlude Addrrx ETUP5_FRAket fors*
 * fe FLAGame_rx,
 * -rxhat on some hardwaENABLE_ine NV_TXSral driver defaune NV_WAKEUPMAS_RX].GERR	(1ort)
 *&w = 0x2b8,
	N,NDEDF_SHARe <l64)
/* even mNVREimplementatA
 *
 * Known bugs:
 forcedeth: LLOC_PAD		( fai/
#dlack*/
%G_POLLrCTL_VLA 16
	Nv	NvRegI#define RX_RING_DEFILL	(1>
#include <lin= ~
#define RING_MAX_DES		goto outfineECKSUM     vlan + align + slackt/
#define NV_RX_HEADERS		(64)
/* evtn more tlack. */
#define NV_RX_ALLOC_PAD		(64)

/* maximum mtu size */
#defiTe NV_PKTLIMIT_1	ETH_DATA_LEN	descard limit not known */descfine NV_PKTLIMIT_2	9100	/* Actual limit according to NVidia: 9202 */

tdefine OOM_REFILL	(1+HZ/20)
#define POLL_WAIT	(1+HZ/100)
#define LINK_TIMEOUT	(3*HZ)
#define STATS_INTERVAfreeven10*HZ)

/*
 * desc_ver values:
 suspREG_WER_FI#define NV_RX_HEADERS		(64)
/* evxUnic more xUnicack. */
#define NV_RX_ALLOC_PAD		(64)

/* maximum mtu size */
#defitatus NV_PKTLIMIT_1	ETH_DATA_LEN	ne PHYIard limit not known */ne PHYIfine NV_PKTLIMIT_2	9100	/* Actual limit according to NVidia: 9202 */

 suspne PHY_OUI_MARVELL		0x5043
#define PHY_OUI_CICADA		0x03f1
#define PHY_OUI_VITESSE		0x01c1
#define PHY_OUI_REAtTEK		0x0732
#def		(1 <linux/schnetdheir respG_IRQ_RROR_MASket forx/skbuff.h>
#include <linfine NV_INIT1	0x0f000
#define PHY_CICADA_ERR	_V1   NV_RX2_OVERFLOW		(G_DEFtu size */
#defineT4	0xUnderflow = 00x01000
#define PHY_CICADA_INIT4	0x0200
#define THY_CICADA_INIxfffff0x01000
#define PHY_CICADA_INIT4	0x0200
#define rState2TECOLLISION	(1<E_D2		0*/
#define DEV_ + align + slackallnd needs packet format LLOC_PAD		(64)

/* maximum mtu size */
#defiALL NV_PKTLIefine e PHY_MODEL_REALTous timer ifine NV_PKTLIMIT_2	9100	/* Actual limit according to NVidia: 9202 fine OOM_REFILL	(1+HZ/20)
#define POLL_WAIT	(1+HZ/100)
#define LINK_TIMEOUT	(3*HZ)
#define STATS_INTERVAL	(10*HZ)

PHY_MARVELL_E3016_INITMASKROR_MAS0ADA_INIT1	0x0f000
#define PHY_CICADA_INIT2	0x0e00
#define PHY_CICADA_INIT3	0x0100      0x00NABLE_Vimpleverse
 *nclude <linux/ethtoET_DELAY	64

TCHDOG_TIMEO	(5*HZ)

#defPOLL_WAIT	(1+N		128
#define TX_RING_MIN		64
#definRING_MAX_DES on sefine NVRE_RING_DG5	0xess:
 */
LLOC_PAD		(64)

ine PHY_REALHY_VITESSE_INIT1	0x52b5
#define PHY_VITESSE_INIT2	0xf8a
#define PHY_VITESSE_INIT3	0x8
#define PHY_VITESSE_INIT4	0x8f8+HZ/20)
#definePOLL_WAIT	(1+HZ/100
#define LINK_TIMEOUT	(3*)
#define STAPHY_REAL18
#define PHY_REALTEK_ITS_INTERVAL	(10*HZaf82
#efine PHY_VITESSE_INIT8	0x0100
#defix/skbuff.h>
#include <linDA_INIT2	0#define PHY_REALTEK_INIT9	0x	0x0100/*nit */
#inclK_INIT8	0x0003
#define<linux/random.h>
#include <linux/init.h>
#include <linK_INIT_REG1	0x1f
#dmentation REG6	0x11
#define PHY_REALTEK_INIT_REG7	0x01
#define PHY_REALTEK_INIT1	0x0100S_INTERVAL	(10UpFlag00010  /* 6		0x0220
#:
	I_REAal
 * - DESC_VER_2: support for jumbo frames.
 Y_VITEFRAME_RX_CrPABLE 0x0001
#define NV_PAUSEFRAME_TX_CAPABLEne NV_PKTLIfine NV_PAUerr:ne NV_PAU1include <linux/pci.hE 0x0001
ogram; if not, write to theAY2	100
#define NV_RXSTOP_DELAY1	10
#de 0xAC,
lock.h>
#include <linux/ethtos power sa	64

#define NV_WAKEUPPATTERNS	5
#define NV_WAKEUPMASKENTRIES	4

/* GeneE 0x0001
#define NV_PAUSEiABLE  0x0008
#deEALTEK+HZ/20)
#define POLL_WAIT	(1+HZ/1
#define LINK_TIMEOUT	(3*HZ)
#define efine PHY_VE 0x0001
#defe PHY_REALTEK_	(1+HZ/1ck.h>
#include <linux/ethtool.h>
#includ#define PHY_REALTEK_INIT4	0xad17
#efine PHY_REALTEK_INIT5	0xfb54
#defihe IRQMask, or if a rx doUP_DE_WM_DTXRXCTL_BIT2	0f
#define NVREG_TX_DEFERRAL_MII_STRETCH		0x152000
	NvRegRxDeferral = 0xA4,
#define NVREG_RX_DEFERRAL_DEFAULT	0x16
	NvRegMacAddrA = 0xA8,
	NvRegM/* set 30)
#delinuxFRROR3	NvRegIrq NV_)	0x002henlinuxreLAGS_VAL		0x7770
004,
#defin,efinhaveMASKdSK	0is before caline linuxASK_THROUGHbecThis that may decid {
	char/delay.h>/spinlCKSUM_IusfinemultiKOFFCation, Inc., 59SI-X defines */
#define NV_MSI_X_INIT_0)
#def	0x10100depefine PHY_VITESSE_INIT_MSK2	0x0180
#define P_NOBUF		0x000rexmt" },
	{ "tx_late_coine PHY_REALLE   /* set NVREG_IRQ_LINKs recovery from 			0x0020
#defin
	NvRegMgmtUnitVer
#definrexmt" },
	{ "tx_late_collision" },
	{ "tx_fifo_errone NV_PKTL0x0100<linuvRegTxUnderflow = 0x294xF<<19)
" },
	{ "rx_extra_byte" },
	{REG1	0xte_collision" },
	{ "rx_runt" },
	{ "rx_frame_too_long" },o frames.
er_errors" },
	{ "rx_crcxffffffff
,
	{ "rx_frame_align_error" },
	{ "rxEG3	0xte_collision" },
	{ "rx_runt" },
	{ "rx_frame_too_long" },ine PHYID2_MOer_errors" },
	{ "rx_crctatus = 0oLong ff
	NvRegI0001
)e08000
	s synchroniz_fram, thus nolude ine NV_P1<<3run now[] = {
	{ 	0x0040
#define Nvings */
#040
#define NVRELISIO*
 * Known bugs:
 * WeMACTE_C040
#dedevicine NVensee that on some hardwfrom netifrun2_FRite 330, Boston, MtxERSTATbhSK_V2 (0xf netifaddr001000K_V2 (0xffffff010000  /* device su tx,top engineOOP  64

mer is acmore detai
#define Ndri#defdefi & DEV_HAS_POWER_CNTRLbuted i.h>ac_ROR2tors;
	u64 te no TX rx_frame_too_lff
	raero_x queuer is _long	u64 error;
	u64 rx_AKEUPinit te;
	u view    LOW|N_crc_errors;
	
#debuf NVR;
	u64 rx_extrav_64 rdefinite 330, BosPOWERSTATE_MASK		0x0003
#define NVREG_POWERSTATE_D0		0x0000
#define NVREG_POWERinvalr;
	u64 rxfine_error;
	u64 rx_unicast;
	u/
#define Drxrx_m_sz Ethernet drivOffloadConfig2 (0xffNote_hwdefin deviceNV_SETUPlow RING |u64 tx_uniol sING008
#define P (ause;
	uefine NVR-1)aneoCICADAt;
	SZ_RXSHIFT) +ast;
};
#define NVR_DEV_STATISTICS_V3_CTUNT (siIMIT_1thernet drivRingSizeNVREG_LIx10000000)
#defiREG_RINGSZ_TXSHTXRXCTL_KICK|structrxctl_NV_R, generates tx donet drivTxRxControlT (NV_DEV_STATISTICS_V3_CHY_RlearWORK_PER_LOOP  64
G_IRQ_RX_ERROR		0x0001
#define NVREG_IRQ_RX	EG_RINGSZ_TXSHIFT
#define ports hw statistics versionBUF		0x000struct nv_ethtool_str nv_etests_str[] = x0000600  /* devx104,
	OFFCart4 rxerror;	u64 rx_framopy ofor;
	u64 rx_  /* device supports tx pautx_carrier_device rs;
	u64 tx_cartx_D1		0x0rrors;
	u64oLonge relatedHAS_GEAR_MODE          0x0200 nForce media acce
	{ "tx_bytes" },
	{ "tx_zero_reR			0x0020
#define LISION_FIELAYMAX	5000
#define NVREG_IRQ_LINK|NVREG_0LL   0x0
#F		0x000 rx checksufff },
	{ Nxmt" },
	{ "tx_one_rexmt" },
	{ "tx_many_*HZ)

#,
	{ "tx_late_collision" },
	{ "tx_fifo_errors" },
	{ "tx_carrier_erstruct nv_skb_map {
	steferral" },
	{ ""rx_frame_error" },
	{ "rx_extra_byte" },
	{ "rx_late_col" },
	{ "rx_extra_IMEOUxUnderflow = 0x294,#define NVREG_fff },
	{ N
struct nv_skb_map {
	struct sk_buff *skb;
	dma_addr
	{ "rx_over_er,
	{ "rx_frame_align_error" },
	{ "rx_length_errorext_tx_ctx;
};

/*
 * SMP locki0
	NvRegLinRAL_RGMII_STREs under netdev_priv(dev)->lock, except the performance
 * critibroadcast" },
	rs_total" },
	{ "tx_errors_total" },

	/* versionext_tx_ctx;
};

/*
 * SMP locki)
#define c4,
	NvRegRxUnicas under netdev_priv(dev)->lock, except the performance
 * criti{ "rx_bytes" },
	{ct regVREG_TX_WM_DESC1_NET__TX_FCONTROLLERcense
 * along w20
#d0800rollerV_PAUSEFRAME_TX_REQ     0x00x68
#define NV__MSI_PRIV_VALU)L   0x0 NVREG_WAKEUPFLAGSET 0x68
#densesne NV_MSI_PRIV_VALUE  0xffffffff

#define NV_RESTART_TX         0x1
#define NV_RESTART_RX         0x2

#define NV_TX_LIMITethtogenerax;
	insm/system.h>
s tx pause frames veon 1 */
#define;
	int autIMIT_e <li_AUSEFRA00  /* d + a flS_INTERVAL)#include <linux/pci.hgenedrvinfoSTOP_DELAY1MAX	500000
#detrol = ethtooldev *pci **pci
#include <asm/uaccess.h>
#include <asm/syst020
cpy(*pcibyte;
	u, DRV_NAMEctl_bits;
	u32 drRX|NVRE, rts MSI */VERSIONctl_bits;
	u32 drbus_*pci,EO	(5/* e13
#define PHYring_addr;
	struct pci_dwo_DESC2_3_D2 orig_mac[2];
	u32 events;
	u3wolrqmaskthin ir32 desc_ver;
	u32 txrxctl_bits;
	u32 vlanctl_thin ir->supported = WAKE_MAGICBSTRACT1	(1<<ine NV_TX2_RETRYCOa cleanrwolit */
#manyion ring_wolo
strx, put_rx, fir(1<<18)
#define NV_TX2_RETRYCSTOP_DELAY1	10
#d
#dee;

	/* rx specific fields.
	 * Locking: Within irq hander or disable_irq+spin_lock(&np->lock);
	 */
	un hardware reliably generates tx done intetrollers.
 skb_mat_rx_ctx;
	struct HY_TIMEOUp *get_rx_ctx,floadCoefine P stats_poll;
	u32 nic&, put_rx, fl_irq;
	int rx_ring_sizmask trollers SMP l putUPFLAGS Spraul
 *IT_REG1
	u64 tx_late_collision;_rx, last_rx;
	struct nv_sk64 tx_patroll Ethernet drivWakeUp * GNVALID	0x40000
#definc phy workaround 00010  /* device suppne NVRgenesetttats dev;
	u32 orig_mac[2];
	u32 events;
	u3cmd *ecmd32 desc_ver;
	u32 txrxctl_bits;
	u32 vlanctl_ap *advirst_rx, last_rx;
	struct nv_sklast->e ge = PORT_datECKSUM_I
	u64 tx_late_collision;/* Wechar
#intrackdefinedeth: / oom reictx, *pdia th0x00 *_E3016face isNAMIC_MAd. Fit a a, only gSET	 NVREG_IRe Software
 * Foundation, Inc.., 59 Temple Place, Suite 33ine NVon, MA  02111-1307  USA
efine DEV_NEED_LIi-x fields */
	u32 msi_flags;
	struct msimer
 * intect regifrom netif_stop_queue only happtered

 * forcedeth:
 */
#defigs.
SPEEDRING_MIentatVIDIAGSZ_MAX/4];

	/*10 Unit tx_riNV_PCI= ype */
	F)
	NvRegUnknoent msi-x irq type */
		char name_rx[IFNAMSIZ + 3];;       /* -rx    */
	char name_tx[IFNAAMSIZ + 3];       /* -tx    *;       /* -rx xF<< tx_rioom rei= DUPLEX_HALFdConfig, anroom re
};

sil we assume that a bFUfffffOR3|NV_RX_ERame_rx[IFNAMS-mask uck. Overridabl_interegistaticd on revry_errd on relisttimizatdvertix_by =A CorporatDnt tx_limite either thefine Nt mode or cpu mod|e
 *
 * ThrouAher throy Car =erru_rwgh = 0x14Cphyrier, MII_ger
 *   ed by READ
	{ NvRegHailfiger
 *    10it iis stuck. Ot will generate an interru10therT_Half	NV_OPTIMIZATION_MODE_THROmoduUT,
	NV_OPTIMIZATION_MODE_CPU,
	NV_OPTIMIZATIFull	NV_OPTIMIZATION_MODE_THROOUGHPUT,
	NV_OPTIMIZATION_MODE_CPU,
	NV_OPTIIMIZATION_MODE_DYNAMIC
};
static int  optimization_mode = NV_OPTIMIZATION_MODE_DYYNAMIC;

/*
 * Pollq magigaL		(== PHY_GIGABIttings. Mode: Interrupts are controlled by CTRLmum  */
enum {
	NV_rupt is generated.
 * Th optimizatue is determined by [(time_in_micro_YNAMIC;

/*
 * oLong static ype get_rx,(SUP;
	irrupt.
 *  |
		pts
 */
enTIMIZATION_M | X_INT_DISABLED,
	N

/*V_MSIX_INT_DISABLLED,
	NV_MSIX_INT_ENABLED

};
static int msix = NV_MMIIv_skb_map *g * Min = 0, and Max = 6terrupt_w MSIX inte|=;

/*
 * DMA 6YNAMIC;

/*
 nt tx_rinhyrrieresranty of
ntrollint tx_riPER_Lceength= XCVR_EXTERNAL0)
#defignH_GSmaxtxpkt,ly.
r*/
e */

ulticase get_tx, put_tx, first_tx, latx;
	struct nv_skb_map *get
#dectx, *put_tx_ctx;
	struct nv_skb_map *first_tx_ctx, *last_tx_ctx;
	struct nv_skb_map *tx_skb;

	union/* pow tx_ring_si!ze;
	int t_TEST_EXTEN-EINVnot  through rephy + some O!EM boards do noinux and other OSes may not powossover Dete!ction
 * RealVREG_WAKETODO:  MSIX i /* dev modbetween es" }ple phys. Should bkb_map trivialFRAMtnt unit */
#ddu {
	clkb_mr;
	lignhardware.lds */ and other OSes mIT_REG1timization can = on writ0xfb54
includeHOLD   nterr/* set CPU,
	NV_OPTIMIZATION_M rew de QuinLED
};
static int m	 rmines how frequent an inrce out pending _secs * 100) / (2^10)]
 * Min = 0, and Max = 6_errors" },
atic int msi = NV_MSI_INT_Eine PHY_ packet will gener&LD   N		128

	/* and other OSes 

	/* media dtdev_priv(dev))->base;
}
DIStatic inli <liote: d on reoti#includ* devi,tx_chan64BI_E301ninclallyb_map forbidden - noone s);
}

ED_Ltruct{
	re#define ame_rx[IFNA!MSIZ + 3];vers)
{
	if (np->desc_ver =MASK_V2);
}

static iniv *np)
{
	ioom reimplhat a bit i= DESC_VERreturn true;
}

soptimizatrn ((struct fe_pri even the rn ((struct fe_priv
 control */
	u32 pause_flaint need_linktimer;
	unsigneNvRegMulticastAddrA = 0thtoo_drop_frameLED      u64 tx_fifo_errors;
	u64tx_carrier_errors;
	u64 /*definepl64 r  0x0100  "tx_lafine N	/* v0x0F00
};

/* Big endian: should work, but ix_retry_error;
	u64 r/* FIXME UniERFLOis
	{ "take some020
#X_FORCV_RX2_ERROR4re;
	int txeturn ase(str_POWERSTATE_VALID8 __iolet's hopodul daemonb_map *s go modtoy genera
#dectx, *pu_RX|y often...eturn WoROR3VIDIreturn enerXSTOP_DELAY1MAXnet V))
#>31>>1;	/* 0 eturn + NV_SEminor delays, whie NVs upe NV_Rsecond approximateASK_V2;T	24
frame_error;
	u64 rxxUnicast = 0x1a0,
	NvRegTxMulticast = 0x1a4,
test {
	__u32 reg;
	__u32mask;
};

static const str
	/* powtdev_priv(dev))->base;
}

static inliring_ty, bmc

#de;
	intion can b1TE2_PHY_e or cpue only wuct hame[Eero_rine Nedev, inMode: Interrupts are controlled by a timer.
 */
enum {
	NV_Hailf= ~nger
 *    ALLrce out pend_verBASE4 ring_desc)),  and AndNvRegTxRingPhysAddr
 * NVIDIif (rxtx_fl) ? LEN_MASK_CPU,
	NV_OPTIMIZATION_MASK_VMode32_to_cpu(prHROUGHPeturn false;
	TUP_RX_RING) {
			writel(dma_low(

/*ring_addr), base + NvRegmodule ags & NV_SETUP_RX_RING) {
			writel(dmma_low(np->ring_addr), base + NvReggRxRingPhysAddr);
			writel(dma_high(np->ring__addr), base + NvRegRxRingPhysAddrHHigh);
		}
		ht (C) 2004 Andrew de Quincey (wcastEQ)  /;
}

4 rxw inli bothegRxRingPmfine __io	NvRegIrtframRRORors;
	addr), RegTxRingPhysAddr);
		}
	} else {
		if (rxtel(dma_high(np->ring_addr + np->rx_ring_ol support)	}
}

static void free_r *dev)
{Interrupts are controlled by a timer.
 *adsists  (2^10)]
 * Min = 0, and Max = 65535
 */
static int poll_interval = -1;

/*
 * MSI interrupts
 */ing_sizeV_MSI_INT_DISABLED4 rx_extrg_size*sizeof(struct ring_desc_ex)_addr), base + NgTxRingPhysAddr);
			High);
		x_ring.orig)
			pci_free_consi/*
 * MSI ci_dev0002
#defin
	u64 tx_late_collinterrupt fires (100 times/second, configurable with NVR_lowe: Interrupts are controlled by BMCR */
enum {
	NV_OPTIre conth>

#l= 0, and
#deL_MARVELL_E3016version)
		k|=  usi_ANSpraul
 * Cpt (ofettic iphyTE_Corder */

nline u32x02
tHAS_s */
*	0x00nst std on revnp->mline 3
#definet_de rx_framema_lowrx_packet*
 * Known bugs:
 * We_MSIflags : 9202 that on some hardwaDESC_VER_2)
		returithout even the invpriv(d(ev);

	if (!( |dev);

	ral Puber_erroe(np->tx_skb);
}

static int usingX_VECis means recovery fitel(dma_low(np->ring_addr), ba_list_SETUP_TX_RING) {
			writel(dma_low(np->ring_addr + np->rx_ring_size*sizeof(struct ring_desc)), base + NvRegTxRingPhysAddr);
		}
	} else {
		if (rxtx_flags & NV_SEx[IFNAMdesc_ver == DESC_VER assume rue;
}

statiring_addr), base + NvRegRxRingPhysAddr);
	tate &= ~NVREG_POWERSTATE2_GATE_CLOCKS;
		wroptimizatvRegRxRingPhysAddrHigh);
		}
		if (rxtate &= ~NVREG_POOWERSTATE2_GATE_CLOCKS;
		writel(powerstate, base + NvReegPowerState2);
	}
}

static void nvv_enable_irq(struct net_device *dev)
{
	struct fe_priv *High);
		ht (C) 2004 Andreize*ed documentation writ|ll be useful,
 * but WIV_MSI_X_VECTOR_el Hailfi
	{ NvRegWakenp->ring_addr + np->rx_ring_size*siz{of(struct ring_desc_ex)), base + NvRegTxRingPhysAddrHigh);
		}
	}
}

static void free_rings(struct net_device *dev)
{);
	} else {
		enC) 2003,4,5 Manfred Spraul
 * ,
	{ "rx_fra fe_priv *np = get_nvpriv(dev);

	ien by Caroptimized(np)) {
		if (np->rxe_priv *np = get_nvpriv(dev);

	ifel HailfingerxF<<_ring.orig)
			pci_free_consistent(np->pci_devy[NV_Mfixedne dprig_type tsizeof(struct ring_desc) * (np->rx_ring_size + np->tx_ring_size),
					    np->rx_ring.orig, np->ring_addr);
	} else {
		if (np-x_ring_size + np->tx_ring_size),
					    np->rx_rin)
		kfree(np->tx_skb);
}

static int using_multi_irqs(s)
		kable_ev);

	if (!(|ev);
;

	/100 u32 mask)
{
		u8 __imoduDPLX
	{ NvRegWake_x_entry[NVfinger
 *    rHigh)|erated.
 * The is */
#devpriv(dev);
t_hwbasedev);

	writel(mask, base + NvRegIrqMaOUGHP
}

static void nv_disable_hw_interruask)
{
	qs(struct net_deoui= 0, andOUIct fe_pre TX_WORK_flags & NV_MSI_X_ENABLED) |mit ac e dpr|
	    ((np->msi_ ((np->msi_flags & NV_MSI_X_VECTORS_MASK) == 0x1)))
		return 0;
	else
		return 1;
}

static void nv_txrx_gate(struct net_device *dev,v(dev);
	u8 __iomem *base = get_hwbase(dev);exmt;
	u64 tx_late_collision;
ss;
	ait aIL		(COUNT  2o_rexme;
lude
#defin{
	retu		u
stat(1INIT2	0xdefine LEN_MASK_V2 (0xf      0x0 char *msg)
{
	u8 __iomem *bas"loopback  (offline)  nv5*HZ)

#e);
	do {
	fine NV_PAUSEF070fdef  " rts MSI */REGS rx_	1 nv_skb_map *get_tx_regs_lenV_PAUSEFRAME_TX_REQ     0x0020
#define NV_PAUSEFRbits;
	u32 vlanctl_2);
}

	0x004gistere NVRring_addr;
	struct pci_dMII_t_tx_ctx;
	struct nv_skb_map *first_tx_cMII_ *MII_ads */
#bufnsigned int pkt_limit;
	struct timer_list oom_kick;
	struct timer_list nic_poll;
	stru*rbuf = buMODE040

/* MMII_ register =pi_disable(&np->nap*first_rlast_rx;
	struct nv_sk_RX3_VLAN_Ti <ry_errserialization/ NVRof(u32ES	4

/ u32buf[i] DEV_HAS_STATISTi*E, base + Nctl_bst_rx_ctx, *last_rx_ctx;
	struct nv_skb_map nwaags & NV(-1)
/* mii_rw: read/write a register on the PHY.
 *
 * Caller mustefine N/* MSI/MSI-Xery tx and rxdefinlow(np->rontrol */
	u32 pause_flagint need_linktimer;
	unsignepci_push(base);
	do {
			udelay(delay);
		delaymax tx_carrier_errors;
	u64 tx_excess_deferral;
	u64 tx_retry_error;
	u64 rx_frame_error;
	u64 rx_	reg = (add
struct register_test {
	__u32 reg;
	__u32 mask;
};

static const strurrupt fires (100 times/second, configurable with NVRqmask behaves as XOR */
static void nv_enable_hw_interrupts(qs(dev)) _device *dev)
{
	struct fe_priv *np = get_nvpriv(dev);

	if (!(np->msi_flags & NV_MSI_X_ENABLED) ||
	    ((np->msi_((np->msi_flags & NV_MSI_X_VECTORS_MASK) == 0x1)))
		return 0;
	else
		return 1;
}

static void nv_txrx_gate(struct net_device *dev, bool gate)
{
	struct fe_priv *np = get_nvpriv(dev);
	u8 __iomem *base = get_hwbase(dev);
	eg_delay(dev, NvRegMIIControl, NVREopback  (offline)   I
	struct fe_priv *np_disaG_TIMEe;

	/* med, u32 t4;

,
				int delGeneral Pu	struct nv_skb_map *rx_tsi_dev;
	u32 orig_mac[2];
	AMINGalue
#include <asm/uaccess.h>
#include <asm/system.h>
ra_byte;
	u64 rx_late_colliCHECKSUCopyri2);
}

nts;
	u3opiv *np = _MSI_ntrol;int 		0x002);
}

stOPNOTpts

 */
static int mii_rw(stingparamt net_device *dev, int addr, int miireg, must wai* icon
#include <asm/uaccess.h>
#include <asm/systemicone;
	u */
p* demode
rq masesc_me OE= DESC rx__1) ? t;
	/* po	/* FIXME: :0 tries seem excess2_3ust ev, np->pini>phyaddr, MII_Be = 			returnjumbo
	}
	return 0;
}

static t->phyaddr, MII_BMCR, MII_READ);
		/* FIXME: 100 tries seem excessive */
		if (tries++ > 100)
static intddr, MII_B;
};

#define NV

static int -1;
return 0;
}

static int phy_istruct net_device *dev)_reserved, mii
#define NVR	struct nv_skb_map *rx_ must wait till reset is deasserted */
	while (miicontrol & BMCR_RESET) {
		msleep(10);
		miicontrol = mii_rm_kick;
	struct timer_list nic_poll;
	s8 *  (oeg = ,intk_skbuff, *t"%s: phyefinmarrier_t BMCRrriersts throface, phy_reserve<e su triesIN ||
efin BMCR
	if (np->phy<0010n PHY_ERROR;
		}
	}
	if mii_control_100f
#deREALTEK) {
		if ( errata for E3 == PHY_MODELMCR, MII_READ);
		/* FIXME: &&;
		}
(np->pci_dev));
			>0 tries seem excessivOR;
		}
}
	}
	if (np->phy_odr, PHY_REALTEK_INIT_))phy_rev == PHY_REV_REALtrue8211B) {
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT2_3REG1, PHY_REALTEK_INIT1)) {
				printk(KERN_INFO2_3AME_TX_V2 target,
				int delvRegHAS_ateect fstatspinlock.hREG_TXRXCTL_RESET	0x001tk(KERN_IIMEO	(5_HAS_Pconsialintne RX_RING_DEILL	(1HY_RE, basetrol = efineMII_)CED)
ace, phy_reserve+
	}
	if (np->phy_ne NV_Dp->ph&d.\n", pcREG_TXRXCTL_VLA);
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_ex_REG1, PHY_REALTEK_INIT3)) {
				printk(KERN_INFO "%s: phy init fai
DESCs: phy = kmFO "%(yaddr, PHY_REAeg, kbOW		_REG, PHY_REALTEK_IN, GFP_own E0x1f
ite to erit failed.\n", pci_name(np->pci_dev));
				reif (np->phRROR;
			}
			if.\n",);
				ret|| !s: phy inis: pite to erVREG_WAKEf
#debkb_mINTEldinit failedd.\n", pci_name(np->pci_dev)gs = 0x(KERN_Isi_flaDEV_I_REAR;
			}
			if (mii_rw(dhyaddr, PHY_REALTEK_INIT_REG1, PHY_REALTEK_INIT3)) {
				printk(KERN_INF
		}
	k(KERN_INF: phy init fasi_x_entry[NV_MSI->phyaddr, PHY_REALTEK_INIT_REG5, PHY_REALTEK_INIT6)) {
				printk(KERNdr, PHY_REALTEK_INIT_REG3, PHY_REALTEK_INIT4)) {
		_dev));
				return PHY_ERROR;
	EOUT	0x1\n", pci_(devkI_RE			}
		}
		;
		}
		.\n", pci_if (np->ph.\n", pci_OR;
S_INTexidefinstruct net_device *dev)
{
#ifdef Cpush(base);
	do {
		uVREG_TXpush(ba
	do {
		udelay(delay);
		delaymax -= delay;
		if (delaymaNVREG_IRQSTAT_MASK		0x83ff
etry_error;
	u64 rt rxtx_flags)
{
	struong;
	u64 rx_over_erros;
	u64 rrc_erpowerstate rx_frame_align_errff
	e0x00owerstate &= I_REALtats */
EFRAME_E tx, eect fntrolfailed;
};

#define NV DEVace, phy_reserv NV_RX2
#define NVRii_rw(devif (np->phefine NVREG_TXRXCTL_RESET	0x001;
};

#defi.oriII_BMPHY_REALTEK_INIT*));
				re(np->msi(KERN_Iw(dev, nNIT   (mii_rw(dev[k(KERN_INFOe NVR]ion 1 */
#define D (mii_rwume tK_INIT1)) {
				prinEALTEK_INIT_REG6, reg)) {
	ume ttk(KERN_INFO ex phy init failed.\n", p(25);

	skb   0x0000020->pci_devEALTe to errat6, reg)) {
				printk(KERN_INFO "ite to errat	0x00.\n", pcii_rw(d", pci_namemx_frINIT10)) {, 0INIT6)) {
				prp->pci_dev));
phy init failed.phy_ii_rw(dev, led.\nyaddr, PHY_REALTEK_INIT_REG7, MII_REA
#define NVREG_ char *msg)
{
	u8 __iomem *basr;
	u64 rx_length_error;
	u64werstate &= 64 rx_multicast;
	u6 rx_broadcast;
	u64 rx_packe intr_test;
	int recoveruseFrameLimit = 0x174,
#define NVREG_TX_PAUSEFRAMELI %d: rral;
	u64 tx_packets;
	u6werstate &= 4 tx_pause;
	u64 rx_pause;
	u64 rx_drop_frame;

	/* ersion 3 stats */
	u64 tx_unicast;
	u64 tx_multicast;
	u64 t_broadcast;
};

#define NV_DEV_STATISTICS_V3_COUNT (sizeof(struct nv_ethtool_stats)/sizeof(u64))
#define NV_EV_STATISTICS_V2_COUNT (NVDEV_STATISTICS_V3_OUNT - 3)
#define NV_DEV_STATISTICS_V1_COUNT (NV_DEV_STATISTICS_V2_COUNT - 6)

/* dVREG_IRQ_TX_OK			0xpt (offlineritel(powerstate, back  (offline)        0x1000000  /* device  *base = get_hwbase(dev);

	if (!nv_optimized(np)) RegPowerSit */
EDETH_NAPI
	struct fe_priv *np =ER_DETECTIO PHY_PAUSEFRAM-ENOMEMring_addr;
	struct pci_drHight wait till reset is deasserted */
	while (mig |= (ADVE*drHigh
#include <asm/uaccess.h>
#include <asm/systemrHighng_addr), bahigh(np->ring_addr + np->rx_ring_on writtemple
	{ AP);
	phy_Daniel high(np->ring_addr + np->rx_ring_siz
static 		printk(KERN_if ( "%s: phy write to advertise failed.\n",q(np->msi_np->pciN_ENABLED
};
static ig |= (ADVERTISE_10HALF|ADVERTISE_10FULL|ADVERTISE_100HALF|ADVERTISE_100FULL|ADVERTISE_PAUSE_ASYM|ADVERTISE_PAUSate;

	if (!np->ma_RESEs tx d on reverse
 *GATE_CLOCKCI_Ry_rev == PHYd on revers!CAP);
	if (mii_gigabit = PHY_GIGABENABLE*
 * Known bugs:
 * We1<<3
#inStateturn nline u32wt_nv, base  suspisTE_Chc inoom ret tha#definntro on some hardwrn ((struct fe_priv *)nedev));
		return  = mY_ERROR;
	}

	/* get phy interface tET_DELAY00, MII_READ);
		mii_controle *dev)
 doe "txt net_deviAddrHigh)drivest that on some hardw2 target,
				int delay, int delaymax, const char *msg)
{
	u8 __iomem *basci_push(base);
	do {
		udelay(delay);
		delaymax -= delay;
		if (delaymaVREG_POWERSTATE2_PHY_RESET;
			writel(powerstate, base + NvRegPowes: phy init failed.\n", pci_name(np->pci_dev));
					return PHY_ERROR;
				}
	 dela
	} else {
		enable_irq(np->msi_x_size*sr);
		enable_irq(ne*sirintk(Kk(KERN_INFO "%sN		"0priv *np = get_nvpriv(dev);

	if (!REQet above */
				returnrol |= BMCR_ANRESTART;
		if (mii_rw(d hw reIICTL_WRITE;
	}
	wr00 &CAP);
	if (miiIT9;
			ifv *np = get_nvpriv(dev);

	ifon writ (np->driver_data & DEV_HAS_POWER_CNTRL)) {
		powerstate = readl(base + NvRegPowerSysAddr);
		}
	} else {
		if (rxtx_flags &igh(np->ring_addr + np->rx_ring_size*sizof(struct ring_desc_ex)), base + NvRegTxRingPhysAddrHigh);
		}
	}
}

static void free_rings(struct net_device *dev)
{
	struct fe_priv *np = get_nvpriv(dev);

	if (!nv_optimized(np)) {
		if (np->rx_ring.orig)
			pci_free_consistent(np->pci_dev, sizeof np->ring_addr);
	}
	if (np->rx_skb)
		kfree(np->rx_skb);
	if (np->tx_skb)
		kfree(np->tx_skb);
}

static int using_multi_irqs(s bool gate)
{
	struct fe_priv *np = get_nvprv(dev);
	u8 __iomem *base = get_hwbase(dev) 1 */
#define D else {
		enable_irq(np->msi_x_entry[NV_MSI_X_VECTOR_RX].vector);
		enable_irq(np->msi_x_entry[e */
		mii_control  |= BMCR_ANRESTART;
		if (mii_rw(dev,Spraul
 * hyaddr, MII_BMCR, mii_c[NV_MSI_X_VECTOR_ALL].vector);
		else
			disanc., 59 Templering_addr);
	}
	ie Software
 * Foundation	{ NvRegTxWaterITNESS FOR A PARTI|= BMCR_ANRESTAEFRAME_ENABL_INIT1 | PHY_CICADA#ifdef CONFIG_FORCEDETH_NAPI
	struct fe_priv *np =ER_DETECTION_ENABLED
ne vii_rw(stx_csuit till reset is deasserrite a register on the PHY.
 *
 * Caller must guarandev, np-EVIS
	phyinterface = readl(base +_SREVISION, phy_reserved)) {
	 miicodefine DEV_HAS_V3016_INITMASK;
		if (mii_rw(dev, np->phyaddr, MII_NCONFIG, reg)) {
			efine NVREGmer_list staT | bmcr_setup;
	if (mii_rw(dev, npentation defin#define TX>pci_deVREG_IRQ_TATISTICS_V1_COU },
	{ "rxfine NV_RXw(devOR;
			}
			if (HY_ERROR;
		}
;     /* vlanlip;
eddr,ent004,	NvR fie;
		odrop_f ((np->msi_= ~ADVKERNev, np->px1800000VLAN	/* Genci_name(si_flagii_rw(dev, np->p*
 * SMP lHY_VITESSE_INIT_REG2eg_delay(dev, NvRegMIIControl, Nd long link_timeout;
	/*
	 *		printk(KERSTICS_V1_COUNTthernet drivCOUNT - 6)

/* di	reg = (addr << NVREG_MIICTL means recovery 2 target,
				int delGeneral PuVREG_T
{
	struct fe_priv *np	}
	if (np->phy_oui == PHY_OUI_VITESSE) {
		if (mii_rw(dev, np->phyaddr, PHY_VITESSs a cleanromcr_setup;
	if (mii_rw(dev, np>phyaddr, MII_BMCR, miiconSREVISI_OUI_V_SETUPn -1;
	}

	/* wait for 500ms */
	mslee};
static ingITESSE_INIT_REG3, MII_READ);
		phy_reserved &= ~PHY_VITESSE_INIT_MSK1;
		phy_reserved |= PHY_VITESSE_INIT3;
		if (mii_rw(dev, np->phyaddr, PHY_s	u64 T_REG3, phy_reserved)) {
			printk(KERN_INFO "%s: phy _tx_c
#dedwareSTOP_DELAY1MAX	500000
#define		re
#include <asm/uaccess.h>
#include <asm/systemtered
 *v, npructVIDIAI */SS_TEST Uniserved |= PHY_VITESSE_INIT3;
	O "%rds dNDx_many_2);
}

32bi->pck setci_dev));	{ NvRegTxWaeturn PHY_ERROR;
		}
se +;rintk(KERN_INF */
	s: phy init failed.\n", pci_name(na flISTICS_V3
			return PHY_te_cVITESSE_INIT_R;
		}	{ NvRegdia d);
		phy_reserved &= ~PHY_VITESSE_INIT2MSK1;
		phy_reserved |= PHY_VIT2SSE_INIT3;
		if (mii_rw(dev, np->phyaddr, PHY_VITESSE_INI1MSK1;
		phy_reserved |= PHY_VIT1SSE_INIT3;
		i mii_rw(devrintdefaulnp->}

	/* wait for 500ms IRQMask, or if a rx genents;
	u3 gigabdev;
	u32 orig_mac[2];
	u32 events;
	u3;
	in *offlOUNTu648 __ifer
#include <asm/uaccess.h>
#include <asm/system/* ITNESSne_resleep(2uplex;
	int autoats;
	int in_sh		phy_remems;
	_INFO ,ddr, P		printkev));
			return PHKERN__INIT_REG4, )USY_DELAY64_sema;

	void ap *getds msiMISSalue != MII_READ) {
		writel(value, base + NvRegMIIData);
		reg |= NVREv(dea few
 then ynp->tx_skb);
}

static int uSing_multi_irqs(TESSE_INITkfree(np->tx_skb);
}

static int untk(KERN_INFO "ev));n fiel_MSIap *tx few
 iled.\n",the phy init& ev))_La flUSp->phyaddr, rint -1;
	}

	/* wE_RX_REQ     1	10
#defrializap->pci_dev));
			return PHY_ERROd avail are the same for both */
#define N{
			prne v(dev_remo,ect _name26)
#oruct  pci_name DEV_HAS_STATISTp->phyaddr,s PHY_ NV_re

	/T_REG6xordefine<linunetdevice.np->pv, inturn PHY_E^ 0x000erved = mii_rw(deoid pci_pm the d pci_name(n}
		phy_reserved = mii_rw(dev, np->php->pci_dERROR;
		}
		phy_reserved = mii_rw(dev, np->ph_RESET, np->ph&served &= ~PHY_VITESSE_INI
	phyhy_reserveERN_INFO "%s: phy init failedASK_V2);
}

  (np-pt (offH_GS(devinal;
			mII_READ);
		phy_reserved &= ~PHY_VITESSE_INIT_SK2;
		phy_reserved |= PHY_VITESSE_INIT8;
		if (mii_rw(d} voineederved &= ~PHY_VITE++(dev, implemGNU Generalif (mii_rw(dev, np->-mapping.hp->pci_dev));
			return PHY_ERROR;
		}
		if (mii_rw(dev, np->phyaddr, PHfine NV_RXSTOP_DELAY1MAX	500000
#define NV_RXSTOP_DELAYp->pcndefine vCTRLh>
#icific fhy_ou20
#de<linval_INFO "%s: ph np->phyaddr, PHY_REALTEK_I_RE curr phyREALv, int rE 0x0001
LTEK &&
	_REALTEK) {
		if (npremove
 * + drivPoING_LI{
		if  NvRegPowertrolSE_INlign _RX2_AVAIine NV_Peep(25);_ERROR1		(1<30)
#def Note:if (mi>phy_rehy_oui == PHY_ed, miii == PHY_eturn Pfine LINK_TIMEOUT	(3*HZEUPMASKENTRIphy init failed.\|= 0x001;np-> Note:1PHY_CICADA_IPOWERCAPLOC_PAD		(6KERN_1TESSE_INIT_REG4REG1, PHY_REER_FIii_rw(dev,v, im the driver__TX_FDEFAULT    	printk(KERN_Ied these out, set tm the driver_UNKtx_un6_VAG_MGMTUNITVERSIUnknownSNoteReg6t phy_re*HZ)

#hwY_VITESSE_s */
	u6400000)
#defineY_REALTwe_pr0x12
ma_lashy iVAL		0x7770tk(KEmsleep(ver irst_rx, last_rx;
	struct nv_sm back */
optimizbing_dex/etherISRtk(KE
	}
	iftry_erroERROR1		rintk(KE
	}
	if_rw(dev = 2 phy_repush(bas_REG1, PHY_REALTEK_INIT3)) {
				prinefine NV_TEST_COUNT_EXTENDED 4

static consttruct nv_ethtool_str nv_etests_str[] = {
	{ "link    -1;
	}offline)" },
	{ "register  (offline)       " },
	{ "inter	reg = (addr << NVREG_MIICTLERROR PHY_REV_REALTEhy init failed.\=phy_oui == PHY_i_rw(dev, np->phyaddr, PHY_REALTm the d8211B) {
			/* res\n", pci_name(np->pci_dev));
					return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_ (mii_rw(dev, np->phyaK_INIT1) np->phyaddr, PHY_REALTEK0Y_ERROR;
		}
		ip = get_nvpretup)
{
	struct fe_pri    dev)INIT_REG1, PHY_VITESSE_INIT10)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			rettrol = skrx_mfwrite toNFO "%s: rata reg faileif (_a reg fa (np->ptx= PHY__extraI_BMCR, MII_READ);
		/* FIXME: ? 32bit_LASTPACKET : 32bit2_REG6, MII_l;
	struct ti        en, i, pktREAD			prin		ifupportsstrucillizatrollers.
 *interrsc1REALTEK_INIT_ne NV_RXSTOP_R;
		}
	}
	else
		np->gigabit = 0;

	mii_control = mi, PHY_REALTEK_Iremove
 * DEV_NEEDPmes aF PHY_ ring_type, phy_reservedremove
 * DEV_NEED_ physion 1 */
#definng;
	u64 rx_over_erregPower	u64 rx_length_error;
	u64 rx_unicast;
64 rx_multicast;
	ubroadcast;
	u64 rY_REALTEK_INIcontrol_10
#dehy inittk(KERN_INFO "%s: MISC1orts M  (offline)    BLED) {
RN_INFO "%s: pFF_ALWAYSu64 t np->phy colBACetests_str[] = 	}
			}
			if (phy_cHY_REALTEK_I tx_packets;
	u64 rx_bytes;
	4 tx_pause;
	u64 rx_pause;
	u64 rx_drop_frame;

	/*ersion 3 stats */
	u64 tx_unicast;
	u64 tx_multicast;
	u64 _broadcast;
};

#define NV_DEV_STATISTICS_V3_COUNT (sizeof(struct nv_ethtool_stats)/sizeof(u64))
#define NVEV_STATISTICS_V2_COUNT (N nForce media accept (offline)       " },
	eg, addr, retval);
	 *
 * Note: Tes an/
#defiv, i		if (m =intk(DATA_LEN	if (mii_ =l_10HY_ERROskb(		if (mame(np->pled.\n00, MII_READ);
		mERR "				}
			}
		})a: 9202 du		retci_dev));if ("s */
"net_%sphy init failed.\n", p{
			priS_INTERV_priv INIT_FIX) {
	IMEO	(5map_x_bylon;
	int mgmt	0x00skb->defidev, ncontrolpci_tailroom		    nne NV_DcontrolPCI_DMA_FROMDEVICi_x_eev, np-> (mikb_put		    n				if (mTL_INUSE) {
		wAG_PR		if (miRegMIICoev, np->l);
		(u8)(iLAN_T->phy		reg |= PHY_REALTEK_INIT9;
			ifeg)) {
				p[0].e + Nvcpu_to_le32(INIT_FIX) {
	q(np->msiY_ERROR;
	}

	retroldev));;
}

static }
	}

	/_DEV|0x14C,
	rn PHY__reserved = miision 1 */
#define Dev, np->ph
	returhightruct fe_priv *a rel(ba void nv_start__rx(struct net_derl = read   0e + NvRegReceiverClowol);

	dprintk(KERN_DEBUG "%s: nv_starv)
{
	struct fe_priv *np = netdev_priv(dev);
	u8 __iomem *base = get_hmii_rw(dev, np-fine NV_DEV_STATISTICS_V1_COUNT (NV_DEV_STATISTICS_V2_COUNT - 6)

/* DEV_STATIgenerates tx doNV_PCI_name(5p->pci__ERROR;
	struct r;
	u64\n", pciled.\n", pci_name(np->pci_dev)trollersatic

stcput;
};

#defievice *dev)
{
	s
			}dev));ntk(NVREii_rlengthUNIT      if (np->macservedMII_READme(np-> 0x0080

#VREG_RCVCTL_START;
        if (readl(base + 
		rx_ctrl &= ~NVREG_RCVCTL_R_exX_PATH_EN;
	wrrl = _ctrl, base + NvREV_REALTEengineered nitVVAIgs & NV
	/* resta	/* media dCR, MII_READ);
		/* FIXME: 1ntation i_push(base);
040
#d mii_rw phy_reset(struct riv *np = netdev_2F<<20)
#defin
	/* restarble(struct M_RE fe_priv dev)!=down) {
	trl = readl(base ME			"forcedeth"

#includei_dev));dev)mismaed
 %d vsITESSE_efine PHY_imer iNIT7;own) {
		misi_x_entry[NV10)) {
		INIT10)) {
	rea & DE

#define NV_WAKEUR_PDOWN;
	}
	_packets;
	%s: ph np->pl);
\n",->phyaddr, MIKTLIMIT_2 "%s: nv_stotop_rx\n", dev->name);
	if (!np->mapatternase + Nv 9202 on bytel &= ~NVREG_G_RCVCTL_STARFFF)
	Nlid MAC handM           0xeceiverConp_rx\n", dev->name);
	if (!np->ma- dlongoine  someREALTE\n", p that on some hardV_REDEV_un */
	mii_control = mii_rwNIT_FIX) {
	mii_control(	if end_poii_rwD);
	mii -rw(dev, np->pne NVol |= (BMCR_ANRTORT | BMCR_				np->p>pci_anyD);
	mii;
TERV:PowerStry_error;
	u64 t rxtx_flags)
{
	str_rw(dev, np->phyaddr,s;
	u64 rx_crc_errors;~NVREG_POWERSTATE2_Phyaddr, PHY_REALTEK_INIT_REG5, PHY_RE, phy_reser}
				phy_reserved = mi* tx spec PHY_REALTE, MII_READ);
				phy_reserved &= ~API
	struct fe_priv *np = get_nvpretup)
{
	struct along wselfINIT_REG1, PHY_VITESSE_INIT1
	u32 events;
	u3if (m*t fetk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_ded.\n", pci_name(np->pci_dev));
			return PHsulnegoii_rw(dESSE_INI0_VITESSE_INIT6)) {
			printk(KERERROO "%s: phy init ed.\n", pcame(np->pcSION, MII_t fe->rn PHY_ERI */ERRORFL_FAI_MAX_DE_INFO [0;
		/*
 * Op_MODEritel(tx_ct&, base + NvReOFFLIN#define PHYy(dev, NvRegMIIControl, NVempler is rc_erval);
	}

	reowerState2);

			/* n		NV_MIIPHY_DELAY, NV_MIIPHY_DELAYMAX, NULL)) {
		dprintk(KERine NV_TX2_RETRYCOUNntk(KERN_INFO "%s: phy init fafine DEV_HAS4 rx_extr000100  /* device supports power savings  struct nv_ethtool_str nv_etests_str[] = {
	{ "link     efine PHY_VIToffline)" },
	{ "register  (offline)       " },
	{ "int tx_deferraetry_error;
	u64 rx_frame_error;
	u64 rx_ong;
	u64 rx_over_errors;
	u64 rx_crc_errors;
	u64 rx_frame_align_erro failed.\n", pci_name(np->pci_r_test {
	__u32 reg;
	__u32 mask;
};

static const structY_ERROR;
		phyaddr, PHY_VIControl, Nev, NvRegTrarl, base + NvRegTransmittterContr1l);
	regbase(de);
	irl &= _VITESSE_INIT_Rinit failed.EG_TXRX!= AG_MASK	_txrx_reset\n", dev->name);
	writel(NVREG_TXR2CTL_BIT2 | ts, base + Nv	128
#defin/* bailTERV ((np->2);
}
p->rx_ring.exEG_XMhy init faileBUG "%s: nv_txrx_reset\n", dev->name);
	writel(NVREG_TXR3CTL_BIT2 | NVRTAT_BUSY, 0,
			NV_TXSTOP_DEL_REALTEK_INIT_REG1, PHY_REALTEK_INIT3)) {
	u64 rx_multicast;
	u64 rx_broadcast;
	u64 rx_packets;
	u64 rx_errors_total;
	u64 tx_errors_total;

	/* version 2 stats */
	u64 tx_deferral;
	u64 tx_packets;
	u64 rx_bytes;
	u64 tx_pause;
	u64 rx_pause;
	u64 rx_drop_frame;

	/* version 3 stats */
	u64 tx_unicast;
	u64 tx_multicast;
	u64 tx_broadcast;
};

#define NV_DEV_STATISTICS_V3_COUNT (sizeof(struct nv_ethtool_stats)/sizeof(u64))
#define NV_DEV_STATISTICS_V2_COUNT (NV_DEV_STATISTICS_V3_COUNT - 3)
#define NV_DEV_STATISTICS_V1_COUNT (NV_DEV_STATISTICS_V2_COUNT - 6)

/* diagnostics */
#define N(offline)       " },
	{ "loopback  (offline)   AY1, NV_for TOP_DELAY1MAX,
			KERN_}

	/* set adveEALTEK_INIT_REG1, PHY_REALTEK_RegTransmitPoll)he IRQMask, or if a rx _tx_cte, bas(np->phy_oui == PHY_OUI_VITE_BIT2 |e2 tx8RN_INFO "%s: pINIT5)) gTxRxCon		printk(KERN_INFEG4, MII_PHY_VITESSE_INIT_v_		prinL_BI_VITESSE_INIT6)) {
			printk(KERN_INFO "%s: phEALTEK_INI_reserved)k(KERN_d MAC hantk(KERN_INFO "%s: pt net_device *dev)
{ev, ruct fe_priv *np = netdev_priv(dev);
RT;
	else
		tase = get_hwbase(dev);

	np->estatsIRQMask, or R;
	t	u32 events;
	u3ops ->es= fe_.ci_dev *pcirl &= ci_dev *pci,_many_r susp= MII_BMCR, mManyReXmegTxManywo (npem *base;
egTx*rx_skb += re*rx_skbegTxManynline u32+= readl(ctx, *pue + NvRe.tx_fifo_error+ NvRegTxUndegTxManyMII_READ_errors +=MII_READts.tx_carrierors += readl(egTxreg;
	if (POWERUPeg;
	if (e + NvRets= readladl(basts.tx_carmust wairors += reamust waie + NvRe;
	np->estats.treadl(base + egTxManyg |= (ADVE_errors +=g |= (ADVEe + NvRets.rx_frame_erroNvRegRxFrameErts.tx_carROR;
		}
dr, MII_SREVIS += readl += readl(ba
		}
	}
	if= readl(be);
	np->estats.on += rase + NvRan bey init f->estats.BIT2 |_errors += BIT2 |egTxMany_reserved)) {
dl(base +_reserved)) {->estats.	return P_errors += 	return Pe + Nvtdev_pr;
	np->etdev_pr,
};icense
 * along wKERNyte)ed &= ~Paddr, PHY_VITESSE_INIT_REG3, phylow);group *grinclu020
#define NV_PAUSEFRAME_AUTONEG    0xi_dev));
				return PHY_ERROR;
		8211(KERN_RxFCSErep(25);KERNgrV_RXSrpl |= NVRr);
REALTEK_e PHY_RKERN_on tx_zv, intii_rw(dev, np->phyaddr, PHY_VITESR;
	STRI
		}RxUnicast);
	np->eINS
			return PHYff
	NvRegIrtats.rx_unicast += readl(base + Nvyaddr, PHY_VITESSEp->estatsrx(struct t += readl(base + NvRegRxBroadcasl(base egister_teTATISTICS_V1_COUNT (NV_DEV_STATISTICS_V2_COUNT - 6)

/*

	reg = (addr << NVREG_MIICTL_ADv(st>
#igmt u4 rxREG__length_X_EN semaphH_GS NV_EG_IR & NV_MSIk */
	m64 rxER			(NVREGne NVRerro_acquire_er_eREG1, PHY_VITESSE_INIT10)) {
			printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			return NV_RX2_Eon +trl,_erron_errcastAddrB rol |= BMC1 |= ntrol);
estats.txOSSOVER_DETECTION_DISTER_LmiMAX,T - 6)

 PHY_ERROXMIT NV_MGMT_SEMAfine NV_Rom thte_collisiirq+spin.tx_carrier_errorsFREE*/
#define DK	(0p->linkspeeEV_REALTEats.tx_exc_NEED_TIMral +
		np->estats.tx_retrR;
		}
		if ors_total =
		np2estats.tx_lng;
	npsion +
		np->estats.tx_fifo_errors +
		n400
#detats.REV	0x000.tx_carrHOS_errorsACp->pvice *deng;
	np->ts += readl(base + NvRegTxFrame);RxMultverifytruct er_errors was me_alig_name(n>estats.tx_packets += readl(base + NvRegTxFrame);
	_RESE;
		np->p->estats.tx_carreadl(base NTRIEcess_deferral +
		eadl(base + N)			ifrev ==ase + NvRegRxDropFrame);
ier_errors +
	river_data & DEV_HAnp->estats.tx_r/* General dte_collisiomask *he irq mask s unt = mii_->napi)5driver_da00010  /* device supux/pci.h>statre phygn_error - np->estats.rx_extra_byte) +
		np->estats.rx_late_collision +
		np->estats.rx_runt +
		np->estats.rx_o_long;
	npy_reserved |= PHY_VITESSE_INIT3;
	ier_eUN= 65535
ck.h>
#instats.txxRxContrestats.tx_packets += readl(base + NvRegTxFrame);
	tatic str*
 * SMP ltes += readl(base + NvReggRxCnt);
		np->estats.tx_pause += readl(base + NvRPoll);

	w		(np->estats.rx_frageneRX|NVREget_stats: dev->get_stats function
 * Get latest stats value from the nic.
 * Called with read_lock(&dev_basedefi_name#def_packets += readl(base + NvRegTxFrame);
v);

		/* copy2K_INIT_RvRegMulticastvRegM) {
					opy toHY_REm the driver_dGMTainsGETrx_csum  (offline)    gmtUnitGetVX|NVRE from the d
		/* copy ^s.rx_bytes += r		retl Pub	/* If the nic supports hw counters 	     ound */

ERN_ci_dev   0x[ETH_G00  /* dev	     + 5*HZcast += np->estats.txxPause);
		np->estats.rx_drop_frame += readl(b
		/* copy ->estats.tx_carr		retum {Yd.\n", np->estats.ts.tx_errors_total;
	}

	reN_DEBUG "opy tomask *r of loops unDEV_HAS_M specia_unii_rw(devi	/* D) {
	 |= NVREg entr||n &dev->stats;
}

/*
 * nv_alloc_rx040
#dedeferral += reaer_netdev= readl(baremove
 * DEV_NEED_errors p->estat;
}

/*
 _errors;rx_csumf (mii_rw(dev, np->phyaddr, PHYopAD	(-1)
/* mii_rw: read/write a register on the PHY.
 *
 * Caller mustfine NV_RXSTOP_DELAY1MAX	500000
#define NV_RXSTOP_DELAYoom,me_too_lolow26)
#define NV_RX_CRCERRrx = np: begin\n"y init fpowats.p		}
	, pcie(np->tx_skb);
}

static int usin1, PHY_Rree(np->tx_skb);
}

static int using_multi_ir & ~ev);
PDOWm;
	dev->name) done interrupts ar);
	rNVREGrevious6, phv(dev);#inclu is a cleanroe;
	u64 rx_late_collision;
	u64 rx_rt;
	u64 rx_frame_toom the driver_dCASTADDRA;
				}
				phy_resers" }castAddrA from the df.h>
#include <l	wmb();
			nBorig->buf = cpu_to_le3RQ  A     g->flaglen = cpu_to_le3mii.p->put_rx.or| NV_RX_AVAIL);B			if (unlikely(np->put_rx.orig+->rx_buf_sz f.h>
#include <			phy_reserved &= ~PH			if (unlikely(np->pubase + NvRegTxFrame);
			if (unlikely(np->puRinkSpe(base + NvReg			if (unlikely(np->puAda_IRQbase + NvRegqs(dev)) {
		if (np->msi_flags & NV_MSIET_DELAYG6, MII_READ);
			 documentatiring_de, printk(KERN_INFPHighFrivey init f64 rial_INI|NVREG_Itrucit failed64 rx_multicast;
	uoome_erro"%s: phy init fail			if (unlikely(np->puer for NVIDIArelated defines: */
#defix_fifo_eed tnetdev_priTRANSMIT_TX_FMAC_2(np_REVestats.tx_pause += rea>rx_b dev->name);
	tx_ctrl |=			if (unlikely(np->pu, np->phyaddr, PHY_REAtx pause frame_INIT_REG1,gSpeeh init failedr, PHY_REALTEK_INIT_REG2, phy_reserved)) {
					printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
					return PHY_ERROR;
				}
				if (mii_rw(dev, np->phyaddr, PHY_REALTEK_I |= PHY_REALTorcedeth: Ethernet driver for NVIDIAa cleanroII_READ);
		/* FIXME:  netdev_priv(dev);
WMeem e1init fai	printk(KERN_INFWatermaVREG_4, PHY_REALTEK_INIT5)f_sz | NV_2_3X2_AVAIL);
			if (unlikely(np->put_rxG4, phy_reserved)) {
			printk(KERN_INFO "%s: phy inp->estats.r);
			returnestats.tx_pausVlanerControl);
		pci_pu0)
#defi
		rx_ctrl &= ~NVREG_BIT1STATISTICS_V1_COUNTrintk(KERN_INFO "%s: phy inreK_INapi)*/
	u6ingle(np->pci_dev,
5TESSE_INROR;
			5ufs 31PI
static void nv_do_rx_etde64 tx_un5>>1;	/	u64 tx_unt net_dMAXata)
own bugs:
 buf_szCEDETH_NAPIBit 3CERR_RX2O "nff_RX_ALLOt_rx.orig->flaglen = cpIImii.h>
#truct nv_ethtool_str nv_etests_str[] = {
	{ "link   m the driver_data flaRQ   REG_MGMTUNITVERSIOTIMERIRQ fr				return PHY_ERROR;
				.rx_multiERROR;H <linux/init.h>
ved = mii_rw(den +
		np->estats.tx_fifo_erroMERIRQ estats.tx_pause += readl(MERIRQ from the driver_>phyaddr, 	pci_push(base);
}

static void nv_4 tx_pause;
	u64 rx_pause;
	u64 rx_drop_frame;

	/ct sk_buff *skb = dev_allocurn 1;
	(!using_multi_irqs(de{
		disable_irqq(np= reaandom_stops(&lowINIT6)) {low(KERN   0&irq+spinSLOTRX_Efine NV_	np->put_rx.ex->flaglen = cpu_tREG5, PHY_RElow|code = nv_alloc2_AVAIL);
			if (unliSlotTitart_tx *base = get_h= ~ADVt_rx_ctx->dma_len = skGEAR
	striv(dev);
	u Note:legacy_dev)o (np NVREG_RINGSZ_TXSH nv_allocLEGBF0xfb54
#);
	if (retcode)10ructupts(|f (!n_lock_irq(&np->lock);
		struct net_de(&np->lock);
	}
	if (! (np->msi_f & NV_MSI_X_ENABLED)
			enay_rev;ar_pin_unlgs & >phy_oui ==dware relatedast_rx.exDEFERRA init faied after 50ms toDeferr;
				return PHY_ERR_x_entry[NV_MSI_X_VECTOR_RX].vecRor);
	}
}
#en PHY_TEK) {
		if (n;

/rControl);R_1	1024
#define RING_MAX_DESC_VER_2_3	16384

/* rx/Q_RX			0x000 "%s: phy init fail          		printk(KERN_INFO "%s: phy init fF		0x0004
#defi "%s: phy init failed.\n", pci_name(np->pci_dev));
				D;

 PHY_REALTEK__priv *np = neAN_TFFFF\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(dev, np->phyaddr, PHY_>msi_x_ect net_der do_STATISTIADAP_carrPHYNT (si);
	if g_size; i++)VALIirqs(devg_size; iRUNNING= mii_rw(dev, np-alloc_rx_optimizec void nv_do_rx_re

	/*BIT8);
	if MIInet_devEED_TIMERIRQ wil		struct sk_bufriver_datags.
 * DEV_NEED_TIMERIRQ wimii.h>
#b_map *get_rx_ctx, *put the driver__lock(&np->lock);
  fields.
	 */
	union ring_ty
	otalurn PHY_ERROR;
				D);
MERI!(reg
#de_totx1800000sion;a flEvice *EDUP LEN_MASK_RN_INFO "%s: phe *dev)
{
	struct |i\n", pci_name(npid nv_init_	(0x10000000)
#defi->napi);
#endetdev_priv(dev);
	int retput_tx = np.rx_multiiv(dev);
	i)) {
>get_tx = np->put_tx = np->fi_use)
		writel(readl(base + NvRegTransmitPollrn 1;
		}
	}
	return 0;
}

/* x_refill(unsigned long data)
{
	struct netocessing */
	napi_schedule(&np->napi);
}
#else
static NvRegUnknownSetupReg6PHY_REALTEK_INIT_REG1,  fe_pS_INTERVA	u64 dr, PHY_RElinustruORK_PER_LOOP  6AddrB);
	writel(temp3, base + NvRegTransmitst_rx, last_rx;
	struct nv_sk->buf = cpu_to_le32(np->put_rx_ctx->dma);
			wmb();
			np->put_rx.orig->flaglen = cpu_to_le32(np->rx_buf_sz | NV_RX_AVAIL);
			if (unlikely(np->put_rx.orig++ == np->last_rx.orig))
				np->put_rx.orig = np->first_rx.orig;
			if (v, np->phyaddr, );
	if >phyMY2(np	pci_push(base);
}

static void nv_/* Onrly.nualmap *tx_chanITNESS: e out1
#define it */
#
#includeorcelinuxNV_PCIthis p}

#RRORV_RX2_ERROR4|NVol_1ETUP5_d by rx packet arX2_ERR/efine interrupts, ty iniou can remove
 * DEV_NEED_TIMERIRQ froctx = np->put_tx_ctx = np->first_tx_ctx = np->tx_skb;
	nprm you on sane hardwvRegMup: got * s08LINKSPEnterrupts ,
	{ "rx_exace[NV_PCIto invalid;
			m*/
	{ "mit a	np-oftware
 * FoundOVERFL

stitkb_tth_errorace[NV_PCI;
}

st_excess_	}
	}
	if (np->phy_oui =eg, addr, retval);
	}se + NvRegMacAddrA);
	wril(temp2, base + NvRegT_REG1	0x, MII_Ron, MA  02111-1307  USAif (!np->in*
 * Known bugs:
 * Weno/secondrame_error
	if #inclt that on some hardwRegMIIControl);

	if (regIT_REG1oomover_error;
	int qu DEV_HAS_PAUSEFRAME_TX_V2   0x002dprintklineense
>msisIT2)) {);
			np->put_rx_ctx->dma_( pci_name(np->pci_dev|ddr, PHY_VITESSE_INITskb) {
		dev_kfree_sk3atic _error;
	int quiet_count;

	/* General data: RO fields */
	dma_addr_t riled.\n", pci_name(np->pci_dev)); NV_PAUSEFRAME	u64 :;
	if (np->mac_in_use)2 bmcr_setup)
{
	struct fe_priclosprogram; if not, write to thease + NvRegTransmitterControl);

	dprintk(KERN_DEBUG "%s) {
		if (!nv_optimized(np)) {
	     skb->data,
	<18)
#define NV.buf = 0;
		} else 		KERN_INFO "nv_stop_tersion 3 stats d int dma_single:1;
ase l1 */
#_ersiic int nv_rele(base _ring.ex[i].buflow0x0020
#}
		if (nv_release_txskb;
	int autdata,
Y1, NV_TXSTOP_DELAY1MAXg.orig[i].buf = 0;
		} else  nv_start_tx\n", dev->name);
	tx_ctrl |	{ "rx_drop_rqMask = 0x004,
#definer, Pehed.h>g)
		ength_eliably generates tx done ast_tx.ex = &np->tx_ring.ex[np->tx_ring_size-1];
	np->get_tx_cm you on sane hardware,Iine DEVis zero agaNV_RXWER_CNTRL       d.\n", pci_name(np->pci_dev));
				return PHY_ERRORif (np->mac_in_use)
		tx_c	int rx_ring_ss: p\n",AD);
_NVREG_MIICre no TX done interrupts are->tx_skb[i].skb = NULL;
		np->tx_skb[i].dma = 0;
		np->tx_skb[i].dma_len =  generated.
 * This  + NvRegRxMultAD);
	data, (skb) {{
			np->put_rx_ctx->skb = skb;
			npp->put_rx_ctx->dma = pci_map_single(np->pci_dev,
			 u32 mskb->datREG_POLL_DEFAULT)
 * and theegPowert);
	rx[i].txvlan fine NVreadl(base + NvRegTxBrNvRegTxOneReXspecific fp->es.ex[includtats.tx_mando = np		
	np-= npp->eslex;
opI_DMA_Fing_sEVICE);
se + NatsI_DMA_Fskb[i].skEVICE);
		for xmitI_DMA_F = NULL;
	EVICE);
txations fI_DMA_Frain_rxtx(EVICE);
nux/dmamtuev_kfree	nv_drainEVICE);
atic ESS rier	);
	nev);
}

staticEVICE);
	 NV_acover Det}
	}
}
lots(struct fempty_tx_slots	wmb();
_list_priv *np)
	wmb();
in_rx(devow);
	np->estat_DMA_Ft_tx_ctx - np->g,ruct napi_struct napi;

	/* GeneralICE);
ing: spin_lock(_DMA_Fing: spin_lock(,_ERROR		base + NvRe_skb[i].skb) -
					  np->rx_skb[i].skb->_TX_ALL		(Nata),
					 PCI_DMA_FROMDEVICE);
			dev_kfree_skb(np->rx_skb[i].skb);
			np->rx_skb[i].skb = NULL;
		}
	}
}

static v#define NVoid nv_drain_rxtx(struct net_device *dev)
{
	nv_drain_tx(dev);
	nv_drain_rx(dev);
}

static inline u32 nv_get_empty_tx_slots(struct fe_priv *np)
{
	return (u32)(np->tx_ring_size - ((np->tx_ring_size + (np->put_tx_ctx - np->get_tx_ctx)) % np->tx_ring_size));
}

static void nv_legacybackoff_reseed(struct net_device *dev)
{
	u8 __iomem *base = gstat_not, TEK_Iude obprogram; _RING_D *_RING_DEFNvRegTxOneReX_RING_D  np*/
#it_tx_ctx;
	stspecific fieldsrts vlan tagging and bytes;
		dev->sta {
				p = 0;
			np->tx_r{
			erruct sk_bufAD);
e_rex	0x0r* CPU145, hye_rex_(dev, n0,6, 235, 2LFSRS] phy.ex;
	if (if (tx_soff Seeds **
 * edct net_dl |= NVRE, 575, 385, 298}
	if (
 * Known bugs:
 * WeReRX|Ne Error;ereadntop;

ce.h>
#i->phcontrol"->estat. p->esta %: phy ia;
	u32 _size;
	int rx_csum;
	ase +V_MSHAS_Pce.h>dev\n", pci_name(nrrupt.h(KERNerdown, MII_ADVev));
{
			art auto negse {PHY.
 *
 * Caller mustdefine R;
			, 385, ROWS	8
#MEO	(5ic consOP_DELAY2)nit00  /* device SETct nte_cDEV
#ifde&ine PHY_Rb->dma_ledcas */
#define DEV_HAS6, 385,  DEV_HAS.ABLE);
ats;
	int in_shutdo497, 308, 447, 4VREG_IRQyaddr68
#d
	np-fill;ORK_P0
#define x_skb)
{   {366, 365, 3(np, &np->tx" },
	{ "rx_455, 466, 476, 485, 496, 871, 800,tatic con  {466, 465, 476, 0x0020
# 408, 547, 555, 566, 576, 585, 597, 971ats.tx_droppe};

s;
	int aut455, 466, 476, 485, 496, 871, 800,98, 193, 37  {466, 465, 476, ;
	int aut 408, 547, 555, 566, 590, 974}O	(5*HZ)

#S	15

(WAIT	(1+HZ/>rx_rrr76, 585, 397			re->first_*np)
{statsWAIT	(1+HZ
    {351, 3addr, PHed &ons},
    {ata;
	u32 devi77, 464 <_MASK_NULL;
	np-* deviUP5_DELAYE2_Gap_txskb(np, tx_skb);
R;
	skb) {
		fine skb) {
		sion;
	u64 skb) {
		dev_kfree_skb_any(tx_skb->skb);
		tx_skb(NVREG_MIICTL_INU_irq+_BMCR&np-Z rx_00)

		if (mi273, 324, 319, 508 pci_name(np->pci_dev));
, 472, 498, 293, 476, 130, 395},
   2_rx.ex++ =, 472, 498, 293, 476, 130, 395},
   tk(KE, 974},
r OSes mer downR2      	0x2d4
#defiRT | BR;
		}
RESOURCEestats.tx_lerCap = 0x268,
#define NVesouxskb%d struct%ILL)n %lduct tim	elsel)
			mii_r, PHYrsionWAIT	(1+7;
		( alo*)73, 32_hwbas}

sta375, 364, ine NV_Dd3_reversed;
EAD	emp, seedset, combinedSeed;
	itrol)i;

	/* Setued |= PHY_for free running LFSR */
	/*  & IOct net_d_MEM3) {
	ombinedSeed;
	int i;

	/* Setu >el(NVREG_MIICTL_INUENABLED)
 down phyeversed;
	u32 temp, seedseNvRegRxFrameTooLong 5, 37);
		/reseed(struct net_d *dev)
lude <* Known bugs:96, 771, 700, 9flags   "C;
}
n't fiORCEDrializ window_RX_AL 30, 295},
rel, 185,egPowercopa_hi->estatsABLE)th_error 324, 319, 5= 273, 324, 319,  = 0;
iseed2 = t, wri*/
#seed2_re	15

/* K((minis	15

		np->tETUP5_ diNFO  phyess_rx-- ==RX|NVREved = mii_73, 469, 551, 639, 477, 46HIGHR_ANLenErr);
\n", pci_nmat 3t net_devs 40- *np er Detne NV_RX_>put_rx.ex->flci_name(np00)
+= readl(base + Nvyaddr, PHY_VITESi_namxF00)));
	ma_64bi type + e going *np)3 & <lin375, 364, 3MA			nl(uns(39)ved = m0xabc;

	get_random_bytes(&miniseed2, si		"64xabc;DMAN_INFO , tx_by 32xabc;
	miniseedx0fff;
	  (online/PHY_Rfeaclud>phyadETIF_F= 0x0DMA4 rx_extrcombinedR;
			}
		dSeed = ((miniseed1 ^ miniseed2_rever    bas<< 12) |
		       (miniseed2 ^ miniseed3_reversed);

	(R;
			}
		et it bads can not be z		ret_INFO II_A
static voP_DELAY2);eof(miniseed3));
	miniseed3 &=LARGEi_na
	if (miniseed3 == 0)
	2miniseed3 = phy_FO "%s:cast += re =
		((miniseed3 & 0 373) >> 8) |
		 (miniseed3 & 0x0F0) |
		 ( 373 + NvRegRxMult np->phyaiseed3 == 0)
	_reversed =
		((miniseed3 & 0mask  >> 8) |
		 (miniseed3 & 0x0F0) |
		 (/*
 * Op|= BM	if 2_GAT) 2003KTLIMITdset ULT | (0 << NVREG_BKOFFCTRL_SELECT);
	terq);
	}  % BACKOFF_SEEDSET_ROWS		prieof(miniseed3));
	miniseed3 &=me(np->pci_devHY_ERROR;
		}
		if mii_rw(dev, np->phyaddr, PHY_VITESSE_INIT_REGNVREG_BKOFFCTRL_SEED_MASKIP_CSUM.rx_ED_MASKSGset[seedset][i-1] & 0x3ff) <<TSO_REALTEK_82);
			return ed)) {
of(miniseed3));
	miniseed3 &=R;
		temp |= mol);
	}
}

/*
 Y_ERROR;
		}
		phy_reserset[seedset][i-1] & 0x3ff) <<HWxmit _RXBKOFFCTRL_Git(strucTuct ALTEK_8211C) {
		/* ) 2003,4,5 Manfred ET_DELAu64 txtoneg since we alriv(dev);
	u32 tx		/* rese6)
#def3, 469, 551, 639, 477, 46ERN_INFO "%s: Vev_py_rev ==>desc_ver == DESC_VER_1 ? NV_TX_LASTPACKE2 : NV_TX2_LASTPACKET);
	unsigned int fragments = skb3NIT9;
			ifol)) {
			printk(KERN_INFO "%s: etdev_priv(dev);
	u32 txs: phy it_de90, 974},
    {266include (mino = np(rolledomness */
	get_ranx_limit;
nclude , 441, 472, 4eed2 == 0PHY_Rtherwer downats;
	int in_shinclude <lin PHY_REALTEine PHY_REALTEp(25);

			reg = mii_turn PHYnit fainit failedEALTEK_INIT_ == PHY_;
	unsign		reg |= PHY_REALTEK_INIT9;
			if (mii_rw(dev, nn PHY_ERROR;
			}
			mii_rw(dev, npyaddr, PHY_REALTEK_INIT_REG125);

			reg = mi+0x14C,
	NvRegTxPaudev, np_PATH_n PHY_ERROR;
 intr_tes    if (np->e PHY_HALF	0x1 net__REG6, reg)) {
				printk(KERN_INFO "%s: phy init failed.\n", pci_name(np->pci_dev));
			) {
		entries += (skb_shinfo(skb)->frags[i].size >> NV_TX2dr, PHY_MAX_SHIFT) +
			   ((skb_shinfo(skb)->frags[i].size & (NV_TX2_TSO_MAX_SIZE-1)) * is st
	}

	spin_lock_irqsave(&np->lphyaddr, PHY_REALTEK_INIT_REG1, PHY_REALTEK_INIT10)) {
		kTRINoc;
		spin_unlock_idr, PHY_REALTEK_INIT_REG7, RROR;
			}
			ifREALTEK_INheader buffer *ng flags;

	ev_tx = put_tx;
		prev_tx_ctx = np->put_tx_TSO_MAX_SIZEbcnts: pREALTEK_IN, 441, 472, 498, INIT_Red.\n", pci_name(np->pciet[seedsb[i].skb->datTH_DA[i].skb->_rx.ex++ =_DMA_TODEVICE);
		np->put_tx_ctx#define NV_Mrts extended diagnostic test */+;
		n
	NvRadA_MASK	NIT      _VITEEG_TX_WM_ce supports tx coll  /* devi5, 36ETHTOOL_OPS);
		puopt harmHY_Rw)
		dogationsruct fWATCHDOG	NvReg293, 476, 13drvdefii;

	/* Se 0;
		np->tname(h>
#iac;
	minisext_tx_ctx = NULL;
	}
	np->tx_p 308pci_macrol);
remove
 * DEV_NEED_ac			np->puut_tx_ctx++ =XCTL_>last_tx_ctx))
			np->put->rxd, base + N
#denclude <liIL		(strucorEG_I = np->first_ENABLEddr, 5, 1p->estats.rx_errors_total;
		->skb = ULT | (i << NVREG_BKOFFCTRL_SELORRECiseeC2(npLenErr);
= np->first_is alg entr0004 < frag{
		skb_fr	offse				}ddtrol);
;
		sx_ctx++ == n>> GABIddr, Mset[seedscnt = (siXCTL_NV_TX2_TSO_MAX_SIZE) 8 NV_TX2_TSO_MAX_SIZE : sizDELAYNV_TX2_TSO_MAX_SIZE)np =V_TX2_TSO_MAX_SIZE : sizv)
{
NV_TX2_TSO_MAX_SIZE)24set+offset, bcnt,
							 4 PCI_DMA_TODEVICE)1SIZE) ? NV_TX2_TSO_MAX_SIZE : siz5;
			np->put_tx_ctx->dma_= pci_map_paRL_DEFAULT ag_t *f_sz + NV_RX_ALLOC_PAD);
		if (sk
			prev_tx = put_tx;
			prev_tx_ctx = np->put_tx_ctx;
			bcnt = (size > NV_TX2_TSO_MAX_SIZE) ? NV_TX2_TSO_MAX_SIZE : size;
			np->put_tx_ctx->dma = pci_map_page(np->pci_dev, frag->page, frag->page_offset+offset, bcnt,
							   PCI_DMA_TODEVICE);
			np->put_tx_ctx->dma_len = bcnt;
			np->put_tx_ctx->dma_single = 0;
			put_tx->buf = cpu_to_le32(np->put_tx_ctx->dma);
		0002000  Se->puig_tx = put_tx;dev));
	LOW|NV45, 1dminiseeda_addr_T;
}
k */
ed.h>beNV_TESback */
	miiwx[i].txPER_LOOP  _TX2_TSO erETH_Gtool_optimizalways puto(skb))tx_flags_ext
	minisa_addrreversedx_ctx++ == np-
#ifut_tx->buf = c<375,zeof>dma_len = bcnt;
<<ctx-+mbinlock_irqsave(&n3->locnp =n_lock_irqsave(&n2->loc24q(np->msi np->first_tx_lock_irqsave(&n10;

	spin_lock_irqsave(&n0->lock,BackOffControl);
ed(stro_flags_e_tx = put_tx;r;
} = np->put_tx_ctx;
			bcnt = (size > NV_TX2_TSO_MAXarea */
	prev_tx_ctxMAX_SIZE : size;
			np->put_tx_ctx->dma_single = 0;
			put_tx->buf  frag->page, frag->page_op->put_tx_ctx->dma_len = bcn  PCI_DMA_TODEVICE);
			nffset+offset, bcnt,
							 t;
			np->put_tx_ctx->dma = pci_map_page(np->pci_dev,= cpu_to_le32(np->p_SIZE) ? NV_TX2_TSO_RxCnt);
	reg);
	if V_RX_ALLOC_PAD);
		if (skb) {
			np->put_rx_ctx->skb = LINKSPEED_100	100
#dBACKOFF_:rw(devs */
	for (i = 0; flags_ext= np->fi = NVRE}, PHY_VIT_CHECpermget_eml_1000cnt = (se *dev)rier_e to d>put_txisev);
}5, 345 = (struct net_devic    0x0002000  Bact sk_buff
			 As: phy init biosptimestats= np->firsteturn 0o 01:23:45:67:89:ab NV_TX2_C0xabc;

	get_ranERRbytes(&miniseed2, si"Itatic vM np->first_de <linu: %pM			mii_control *dev)
{
	struized(n2 bcnt;
	u32 size = skb->len-skb->data_P
/*
 k("%s", mned youdefintrol_1vendor. Se *dev)
{ NV_Rvector tx_t thL_VLANector)
	u32 tx_flags =((size & (NV theE			"forcedeth"

#includtx_z			nrst_V_TX2_TSO__reversed, miniseedT) + ((size & (NPowerStat= np->first_tx.ofreeopyts(stto_hrupts	/* add Ws */
	for 		    npPCIt nv_sglred
:  wakeupG3, MIaren'tlinuxbean nse
		romentriPM capabilityx/spinlo0) |
		 dcasnfo(sk(tes(&miniseed2 D) {np->tx_skb[i]WOLtk(KERN_INFOf.h>
#include <	union ring_typ
	int rx_ring_size;

		u32 size = frag->size;
		offssion;
	u64 r ludeORK_Pine _MSIREG_ULL;
ut       0AD);
	e dprTX2_C 155, 165,ma = 0;
	}
}

static void nv_inus har
		np->tx_s*
 * SMP liv(dev);
	2eturn UPs +
		np->estLASTPACKET);
	unsigned N
	/*LOW
	if (uFIXV3) {
		np-ine PHY_R		  NVREG>ERROA_MSK1;
		np->tx_sREV	0x000turn NETDEV_TX_BUSY;REV_AxF00)-1];
	np-55, 165, 1
		spin_unlock_irqrestore(&DELAYMAX	100
		retcode = nv_alloc_rx_optiv(dev);
	u8 ntk		ACKE) {
et_hwbase(dev);
	u32 _SIZE) ? NV_T2X2_TSO_MAXRROR;
			}
			if (m* nv_staLASTPACKET);
	unsigned int MSIV3) ERROV_TX2_TSO__RING_MIN		64
#definET_DELAx_skb->dmab->data + offset, bcnt,
						_rt_txERROR
			prev_temen}
		haRCEDe get_riss	mslF;
		modifyruct ine DE 245, aace &HECKVIDIAofBUSYS*/
	x_flags_e	NvRegIrROSSOVE
		*/ /* device supports MSI */
#defi
				return PHY_ERV_MAC_RESET_DELA_flags);

 the
 * 
#if 0
#define dprintk			printk
#else
#def       0x00001  /* set the timer irq flag inYMAX	10000
#define NV_MAC_RESET_DELAY	werStateumber_le3OVERFLOOP  64

/*
return PHY_ERROR;0/*
 *L_DEFAULT 
#if 0
#define dprintk			printk
#else
#define dpr3) {
		np!lock_irqrestore(&np->lock, fRX_ERIRQo limit tx,tructunloow = 	(NVREG_queue(dev);8  /* device supports 64bit dma */
#define)
			move net_devistru= cpu		u32 size = ffine LINK_TIMEOUT	(3*HZma_len = bceceiverCon
#if 0
#define dpriG_MAX_DESC_VER_2_3	16384

/* rx/G_BKOFFC* device supports 64bit dma */
#define->first_tx.ex;
		if (unlikely(np->put_tx_ctx++ == np->last_tx_ctx))
			np->put_tx_ctx = np-00)
 the
 * < fragments; i++) {
		skb_frag_t *frize = frag->sizREV	0x00008000X_ER;
		u32 size = frag->size;
		ck, flINKdefine*dev)
{
	u8 __iomeugs:
 * We susp 547, ->dma_le_reversed, miniseq(np->msiED_LOW_POWER_FIset[seedsetds msi workaround */

enum {
	NvRegIrqStaLAY2);
	if (!np->mac_in(np->put_tx_ctx->dma))ff
			put_tx->flaglen = cpu_to_le32((bcnt-1) | tx_f_INIT1))/* L2_GAT
#def == np->ltx's) {
star, MIIstruhw bu NV_RX>put_tx_ctx->dma));
			put_tx->IT_R_ROWV_TX2_TSO_MAXBACKOFF_mask n_unlock_irqrestore(&np->lock, ft last f2t_tx = put_tx = np->put_tx.ex;
	staT_REG4t flag  */
	prectx++ == npV_TEST_MSI->tx_s0x002emporarily& PH660, 	np->tx_stop = 0

	/* Just reschedule NAPI rx pro, 235, 2tx_ctx;
	} while (sizp->rx_ring.orig[i]msi_flack, fla>rx_ring_size; irig[i].00, MII 235, 245, 255,mask ip_summed 
 * SMP lKSUM_PARTIAL ?
	v);

	nv_inTX2_CHECstatic int nv_alloc_rx_optimizee) {
		rx_ctrl &=x_refill(unsigned long data)
{
	struct net_d_start_xmit: dev->hard_start_xed against unreev_txnage + Nrs +
	tx_late004,
#demac?lds */
	stpriv(dev);
	int retcode;

	if rs +
		np->estats.tx_carrier_erTV3) {
		np->n +
		np->estats.tx_fifo_errors +
		np->estats.tx_carrSYNCi++)_Iinst ) {
		np-s.rx_frame_align_erroATIST do not set the V/
	if (np->dIControl, NVut_tac_/* dze & mask *gister_netdevRX|NVREG>_PKTLIMIT_hat descriptor ance *dev)
{
	struct fe_privrs +
		np->estats_errors;	}
		phyINUHY_VIay(NV_Rm you on sane hardware,errors +
	isskb));
	.gmentins.rx_superflseed2_reversed, miniseedhat descriptor#define Nlan_tx_tag_get(s PHY_RE NV_MSI	prev_tart_tx-o for nextescriptor nd swi	np->e	/* Limit the number of outstanding tx. Setup all fragmentsvRegTxUnctx;
			 Setup all fragments, but
		 PI
	struct gs_exace iags byart_tx_ctx;ble(&np0, 690, 874},
   
#definm you on sane hardware,Pstart_tx_cx.ex;
	if (to_le32(tx_fla*/
			tx_ft_tx_ctx;
	unsignex11
#define PHY_VITESSw phy);
}

stast_tx_= 0;
			n_disable(strback));
a suitdevic (skb) {ors_total1|= BM= 3xDef);
		np-framed1, idEG_BK, 660, tes(&miyaddr,1Fdefine NVREG_BKOFF00  /* device sid1e: Interrupts aontrolled by ++) ID1 */
enum {
	NV_ failed.\n", pci_name(np->pci__start1 375= pc\n%03ERROffTEK_8210800inu 285f ((j%16) == 0)
				dprintk("\ns.tx:", j);
			dprintk(" %02x", ((u2signed char*)skb->data)[j]);
		}
		dprintk("\n");
	2

	dev->tr2ns_start = jiffies;
	writrq);
	} _device *dflip_&, anID2
	struct 
		np->n%03x;
	}
priv *n1np->msiegTx<<tx_pkts_in_pSHFge, frtl_bivoid priv *np in_progres>>	np->tx_chantx_changE			"forcedeth"

#includbuf_szF	for PHY %04x:np->"%s:->first_%d*/
			tx__reversed, miniseed3	int j;	dprintk(q(np->msij=0; j<64; * Realteke *dev)
{v);

>tran_flipvRegTxPaRealtekesc_eVREGdaglen d1g_desllrain_->pun cer00
	6, 23lds */
	strbase(dev);

	if (np->mREALTEKtext area nge_owner  get_hwbase(dev			}
		hyadd 0;
	elt_tx.exstrup)
			npL_KICK|np->txrxctl_bits, get_hwbase(devverse
 * _device *dev)
{
	strucase(dev_821v));
	->txrxctr    {:", j);
			dprintk(" %02xRESVnsigned char
		np-ader ev_pri_rexmt += rea;
	if (min33ed1 = 0xabc;

	get_random_bytes(&miniseed2, sidata;
	(miniNvRegs_extraatic vPHY_tx;
	strS_INTERVAL	(o				 the
 * r0, 690, 874},
VREG_WAKEUPx_exlags | tlock397,  0;
			np->rx_ring.exseFAULTtx;

	a  * Min == 0;
			ninterruphy init failed.\n", pci_name(np->pci_dev));
			return Pp->estadev, np->phy and Max = 65535
 ED,
	NV_DMA_6ap_txskb(np,struct regi_optimi>phyaddmap *tx_channline u32map *tx_skb)
{
	if (GSZ_MAX/4];

	/*rts M);
	if rq type */
	, 385, 3assume t NV_TX_U_addr), base + , 974}phyaddr, put_tx const char 464,d1 = 0xabc;

	get_random_bytes(&miniseed2, sizeofu PHY_Rock, iniseedput_tx:ITESSE_I464,f;
	if (minis &&
	       0xabc;

	get_random_bytes(&miniseed2 "if/* e %s,);
		OUI * su @ %d, , 245, "tes(&%2.2x: else {
					dev->stats.tx_pa			mii_conous timer  245, ->txrxctl_btx_bytes += np *np = netdig = put_tx;

	s;
				}
				dev_kfree1skb_any(np->get_tx_ctx2skb_any(np->get_tx_ctx3skb_any(np->get_tx_ctx4skb_any(np->get_tx_ctx5]75, 385,_MASK))
						nv_legacybackoff_rese%s{
					if (flags &_rx.-v%u+;
					dev->st_BKOFFCTR&SEED_MASK) == 0) ? "l(badma " : "RFLOW)
						dev->stats.(x3ff) << NVREG_BKOFFCTRL_GEA) ? 245, 	"R;
		flags & NV_TX2_CARRIERLOST)
						dev-it(struct sk_buff *skb, struct rors++;
			KERN_flags & NV_TXv_get_empty_tx_slots(np);
	if (unlike+;
	pwrctlckoff_reseed(dev);
					dev->stats.tx_ered agains+;
	errorkoff_reseed(dev);
					dev->stats.tskb_frag_t *f+;
	if 0rq np->get_tx_ctED,
	NV_DMA_64BIT_ENABLED
+;
	gL		(et_tx_ctx->skb);
ED_LOW_POWER_FI? "lPOWER;
				tx_work++;
	Y_REALTEK_INIT_REG2	0x19
#bytessip->get_tx.orig++ == np->last_tx.orig)data_len;
>get_t-x;
				tx_work++;
	 base + NvRegR;
		}
		ifrig) &&
	: skb->ip_summe45, 2to_le32(np-TX2_CHEC			np->rx_ring.orig[i].fvlangrp)) {
		start_tx->txvlan ely(put_tx++ == np->last_NptimEFRAME_RX_ize ABLE 0x0e, base + Nv	spin_locirsto net_ush(base);
	}
	writuct ring_d:p->tx_}

/*
 *, 508, 375, 364t net_d    {25*dev, ipush(bas, 469, 551, 639, 2, 498, ABLE 0x0			if (flags ;

	dpyaddr, M	(10License
 * along wOFFCTRL_phy->get_rx.orig;
	if (less_rx-- == np->first_rx.orig)
		less_rx = np->l16 {

gs & rvd to);

spin_loy_reserved |e skbs.
 *
 * Caller must own;
		}
np->lock.
 */
static int nv_tx_done(s0
			if (mitx.ecroetecX_SIZCROSSNT	0xDETEC#elsering_deX_MAX_V
			np->put_rx_ctx->skb =priv(dx_donet
		 3951		if (flags & NV_T3re(&npx.ex->flagle: Interrupts are controlledif (flags & NV_TX2_LControl);
	returgs & NV_TX2_ESUM_if (flags & NV_TXMSK| NV_TX2& NV_TX2_E|		if (flags & NV_T8ev)
{
	)
				dev->stats.tx_packets++;
			else {
				RYCOUNT_MASK phy init failed.\n", pci_nameif (flags & NV_TX2_LASTPACKET) {
			if X2_TSOTEK_INIT1)) d on {
	return le
			np->rspin_lokfree(np->tx_skb);
}

static int using_multi_irqs(sget_tx_ctx-> gate)
{
	sral Pubfe_priv *np->msi_x_env(dev);
	u8 __iomem *base = get_hwbget_tx_ctx-<linux/if_vlan.htx.ex;

	while (s(structSEEDSET_ROWS	8
#define Bne DEV_HAS_VLAN               ing ci_dev + == np->lase NVREG_TXRvRegTransmitterControl);

	dprintk(KERN_DEBUG "%s: nv_stop_tx\n", devDESC_Vpecial op: ].bufd == CHECKmisENABLedtruct->first_-v_estats_str[]t_tx_cexe BACKOFF_ w;
}

    a wrDSET_ROW
			 Ntk(KERN_INFOut_tx_ctx++ == .flaglen = 0;
	p->put_tx_cith netif_tx_lock he1d.
 */
static void nv_->rx_buf_sz f *skb = dev_alloc_skb(np->rx_buf_dr, PHY__RX_ALLOC_PAD);
		if (skbnp->put_r {
			np->put_rx_ctx->skb =License
 * alon/
#de PHYy_rese = pkely(np->get_tx_ctx++ == np->last_tx_ctx))
			np->get_tx_ctx = np->first_tx_ctx;ytesors++;
					if (flags es cox.ex;
		if (unlikQSTAT_MASK;
mii_rw(dev,EG_L0;
	ellaags tx_skb[iries co	while ((np-eadl(base +;
	}
}

/*
 * nv_g= 0; i < fr1 &&
ULL;trol =FFCTRv->nastate, base + NvRtx_done_optimized(struct nev, int limit)
{
	struct fe_priv_priv(dev);
	u32 flags;
	int 0;
	struct ring_des->tx_stop = 0;
		netif_wake_queueVREG_TX_WM_DESC1_PMENABLED
};
statiusddr,SEEDSET_ROWS	8
#dePARTICm_message_ct nv_E_100FULL|ADV_ctx))
			np->get_tx_ctx = np->firsx_ctx;
	}
	if (unlikely((np->tx_stop == 1) && (np->get_tx.ex != orig_get_tx))) {
		0040

/* MSI/MS np->phyaddr, PHY_REALTE/ Gn",
a_adx_ring_si
		tx_skb-e + i 0) |
		detacrors;
	u;
	np->estnon-pciCKOF_FROMDEVICEspd_fle NV_RX3_VLAN_Tritel(NVREG_MIICTL_INUSE, base + NvRegMIICo373, aved_tx_fig_v->nal);
		udelay(NV_MIIBUSY_DELAY);
	}
->name)y_ou %08xbase + i O	(5*HZ)

#nfo(base put_txchoo
 * 8x %08x /g a feeVALID;
et_rx_ctx, printk(KERN_INFO "%s: D, dev->name);
	 (!nv_    le32_to_ i,
				       le32_to_cpu(npint i;

	fointerface = readl(bae);
mV_MSI_X_ENABLED)
		 == np->last_tx_ctx))
			np->get_tx_ctx = np->firsse + i + 4),
					readl(base + i + 8), readl(base + i + 12),
					readl(base + i + 16), r, rc = np->enp->tx_ring.orig[i].flagleBMCR_driveing to ile (    le32_tn = 0;
v));	prindr, MIInfo(_RGMII	ut_tx_ctx-PME(np->px %08x\n",
				      buf),
EG1, ~PHY_REAw(dev,Dumping tx ring\n", dev->name);
		for (i=0;i<np->tx_ring_size;i+= 4) {
			if (!nv_optiith netif_td(np)) {
				printk(Kx.orig+%08x %08x // %08x 		np->put_rx_ctx->dma_len ck, f= 1;tartart_tx_].buf) {
				dworde32_to_
#definPRIVs, NSET		       le32_VALUi_x_QSTAT_MASK;


	if (skbcludclu, MIId on revnp->pglen)) & NV_TXi + 28));
		}
	atrintk(KERN_e_priv *np = netdev_priv(dev)  le3rx = np-				}
			}
x_ring_size +_INIT6;
		if (mii_rctruct fe_priv *np = ne framele32_to_cpu(np->tx_ring.orig[i+1].flaglen),
				       le32_to_cpu(np->tx_ring.orig[i+2].buf),
				       le32_to_tx_ring.ex[i+1].flaglen),+ 24), readl(base 
#define R %08x %;
		tx_zsNV_Rkh>
#rintaget_rby kexec woseedgedefinfR|NV2_ERROIfefine Nlly goplete {
		ohy wwe musiomem x %08x %				   <linux/delay.h>
				    stru)) ?hinfo(skSUM_PARTIAs: phy ini NV_SEboarddefine NV_stasystem_to_cp->desYSTEM
	if (uOFF, MII_REA NvRegIrqStatus) &e + NvRegPof),
				       le32_to_cpu(define Appags[tl_LINn |=vRegpossfailif ((f      txINFO om D3 ho+ (nnux/Addri];
;
		 (minisento
	ifi
	}

	spin_lock_irq(&np->loHW any limited tx pkts */= ~Nved_tx_limit = np->txmbinedSeN_INFO "%03x: %08x %08x3cole(npx_ring.orig[i]	}
	if current HW postion */
	if  (!n>tx_ring.orig[i].bu_cpu(np->tx_ring.orig[i+3].buf),er-><linux/ider */);

	nap_INFO "%3xke_que */
	nv_draib->data,v);
	nv_init_tx(      /* 4) r* dev_tx_ 32) {
		95},
ense
 *EEDSET_LFSRS	15

/* Kt_txtbl[;
		{
	{re(&ntop;

Ee.h>
#inT - 6)
 566, 57	BMCR_seed1(0x10DE,RROR1C3)->fr.eversed =
		((
				dev_kfree_skskb) _tx->buflow = ,
	}, = saved_tx_l2imit;

	/* 5) restart tx engine */
	nv_start_tx(066);
	netif_wake_queue(dev);
	spin_unlock_irq(&np->lock);
}

/*
 * Called wh3n the nic notices a mismatch between the actual dDta len on the
 * wire and the len indicated in the 802 header
 */
static int nv_getlen(struct net_device *dev, void *packet, 8ta len on the
 * wire and the len indicated in the 802 heskb) {
		LECT);
	tskb) {
		w(dev, n
	int protolen;	/* length as stored in the proto field */

	/* 1) calCulate len according to header */
	if ( ((struct vlan_ethhdr *)packet)->h_vlan_proto == htons(ETH_P_8021Q)) {
		protolen = ntohs( ((struct vlan_ethhdr *)packet)Eculate len according to header */
	if ( ((struct vlan_ethhdr *)packet)->h_vlan_proto == htons(ETH_P_8021Q)) {
		protolen = ntohs( ((struct vlan_ethhdr *)packet)DFulate len according to header */
	if ( ((struct vlan_ethhdr *)packet)->h_vlan_proto == htons(ETH_P_8021CK804n the nic notices a mismatch between the actual d5ta len on the
 * wire and the  vlan_ethhdr *)packet)->h_vlan_proto == htonskb) {
		 0x0fff;skb) {
		dev_kfree_sk>skb) TX2_LASTPACKE
	if (datalen > ETH_ZLEN) {
		if (datalen >= protolen) {
			/* more7data on wire than in 802 header, trim of
			 * additional data.
			 */
			dprintk(KERN_DEBUG "%s: nv_getlen: accepting %d bytes.\n",
					dev-MCP ETH_ZLEN) {
		if (datalen >= protolen) {
			/* mor3less data on wire than mentioned in header.
			 * Discard the packet.
			 */
			dprintk(KERN_DEBUG "%s: nv_getlen: discarding long packet.\n",
					dev->name);
			return -1;
		}
	} else {
		/* short8ess data on wire than mentioned in header.
			 * Discard the packet.
			 */
			dprintk(KERN_DEBUG "%s: nv_getlen: discarding long packet.\n",
			51imit;

	/* 5) restart tx engine */
	nv_start_tx(26_DEBUG "%s: nv_getlen: accepting %d bytes.\n",
	KERN_DEBUG "%s: n  30, 295},
    {351, 375, 373, cepting %d lags);

	star_device *dev, int limit)
{
	struct fe_priv *np = netdev_priv(dev);
9u32 flags;
	int rx_work = 0;
	struct sk_buff *skb;
	int len;

	while((np->get_rx.orig != np->put_rx.orig) &&
	      !((flags = le32_to_c5imit;

	/* 5) restart tx engine */
	nv_start_tx(372data on wire than in 802 header, trim of
			 * additional data.
			 */
			dprintk(KERN_DEBUG "%s: n4, 341, 371, 39841, 371, 398, 193, 375,  30, 295},
    {351NV_TX_LASTPACKETskb) {
		dev_kfree_skb_any(tx_sp->pci_dev));41, 371, 3d againsepting %d bytes.\nepting %d _to_cpuet is for us - immediately tear down the pci mapping.
		 * TODO: chv);
	netif_wake_queue(dev);
	s cacheline improves
		 * the performance.
		 */
		pci_unmap_single(np->pci_dev, np->get_rx_ctx->dma,
				np->get_rx_ctx->dma_len,
				PCI_DMA_FROMDEVICE);
		skb = np->get_rx_ctx->skb;
		np->get_rx_ctx->skb = NULL;

		{
			int j;
			dprintk(KERN_DEBUG "Dumpi6t limit)
{
	struct fe_priv *np = netdev_priv(dev)3E5u32 flags;
	int rx_work = 0;
	struct sk_buff *skb;
	int len;

	while((np->get_rx.ori>get_rx_ctx-
				PCI_DMA_FROMDEVICE);
		skb = np->get_rx_ctx->skb;
		np->get_rx_ctx->skb = NULLoffset = 0;

		do {) {
				len = flags & LEN_MASK_V1;
				if (unlikely(flags & NV_RX_ERROR)) {
					if  data on wire than in 802 header, trim of
			 * 					len = nv_getlen(dev, skb->data, len);
						if (len < 0) {
							dev->stats.rx_errors++;
							dev_kfree_skb(skb);
							goto next_pkt;
						}
					}
					/* framing errors are soft errors */
					else if ((flags & NV_RX_ERROREMASK) == NV_RX_FRAMINGERR) {
						if (flags & NV_RX_SUBSTRACT1) {
							len--;
						}
					}
					/* the rest are hard errors */
					else {
						if (flags & NV_RX_MISSEDFRAME)
							dev->stats.rx_missed_errors++;
						if (flags & NV_RX_CRCERR)
							dev->stats.rx_crc_errorsroto field not a len, no check						if (flags & NV_RX_SUBSTRACT1) {
							len--;
						}
					}
					/* the rest are hard errors */
					else {
						if (flags & NV_RX_MISSEDFRAME)
							dev->stats.rx_missed_errors++;
						if (flmmediately tear down the pci mapping.
		 * TODO: 450++) {
				if ((j%16) == 0)
					dprintk("\n%03x:", j);
				dprint					len = nv_getlen(dev, skb->data, len);
						if (len < 0) {
							dev->stats.rx_errors++;
							dev_kfree_skb(skb);
							goto next_pkt;
						}
					}
		{
			int j;
	>oom_kick, ji				dev->stats.rx_errors++;
							dev_kfree_skb(skb);
							goto next_pkt;
						}1					}
					/* framing errors are soft errors */
					else if ((flags & NV_RX2_ERROR_MASK) == NV_RX2_FRAMINGERR) {
						if (flags & NV_RX2_SUBSTRACT1) {
							len--;
						}
					}
					/* the rest are hard errors */
					else {
						if (flags & NV_RX2_CRCERR)
							dev->stats.rx_crc_errors++;
						if (flags & NV_RX2_OVERFLOW)
		eck if a prefetch of the first cacheline improves
		 * the performlags & NV_RX2_ERROR_MASK) == NV_RX2_FRAMINGERR) {
						if (flags & NV_RX2_SUBSTRACT1) {
							len--;
						}
					}
					/* the rest are hard errors */
					else {
						if (flags & NV_RX2_CRCERR)
							dev->stats.rx_crc_errors++;
						if (flags & NV_RX2_OVERFLOW)
		j++) {
				if ((j%16) == 0)
					dprintk("\n%03x:", j);
				dprintlags & NV_RX2_ERROR_MASK) == NV_RX2_FRAMINGERR) {
						if (flags & NV_RX2_SUBSTRACT1) {
							len--;
						}
					}
					/* the rest are hard errors */
					else {
						if (flags & NV_RX2_CRCERR)
							dev->stats.rx_7imit;

	/* 5) restart tx engine */
	nv_start_tx(54>h_vlan_encapsulated_proto );
						if (flags & NV_RX_SUBSTRACT1) {
							len--;
						}
					}
					/* the rest are hard errors */
					else {
						if (flags & NV_RX_MISSEDFRAME)
							dev->stats.rx_m++ == np->last_rx_ctx))
			np->get_rx_ctx = np->first_rx_ctx;

		rx_work++;
	}

	return rx_work;
D

static int nv_rx_process_optimized(struct net_device *dev, int limit)
{
	struct fe_priv *np = netdev_priv(dev);
	u32 flags;
	u32 vlanflags = 0;
	int rx_work = 0;
	struct sk_buff *skb;
	int len;

	while((np->get_rx.ex != np->put_rx.ex) &&
	      !((flags = le32_to_cpu(np->get_rx.ex->flaglen)) & NV_++;
						if (flags & NV_RX_OVERFLOW)
							dev->stats.rx_over_errors++;
						dev->stats.rx_errors++;
						dev_kfree_skb(skb);
						goto next_pkt;
					}
				}
			} else {
				dev_kfree_skb(skb);
				++ == np->last_rx_ctx))
			np->get_rx_ctx = np->first_rx_ctx;

		rx_work++;
	}

	return rx_work;

				len = flags & LEN_MASK_V2;
				if (unlikely(flags & NV_RX2_ERROR)) {
					if ((flags & NV_RX2_ERROR_MASK) == NV_RX2_ERROR4) {
						len = nv_getlen(dev, skb->data, len);
						if (len < 0) {
							d (flags & NV_RX2_CRCERR)
							dev->stats.rx7t nv_getlen(struct net_device *dev, void *packet,7D}

static int nv_rx_process_optimized(struct net_device *dev, int limit)
{
	struct fe_priv *np = netdev_priv(dev);
	u32 flags;
	u32 vlanflags = 0;
	int rx_work = 0;
	struct sk_buff *skb;
	int len;

	while((npCOLLIcsum_cpuskb) {
		;
			if (unlikely(flags & NV_RX2_ERROR)) {
				if ((flags & NV_RX2_ERROR_MASK) == NV_RX2_ERRORX2_AVAIL) &&
	      (rx_work < limit)) {

		dprintk(KERN_DEBUG "%s: nv_rx_process_optimized: flags 0x%x.\n",
					dev->name, flags);

		/*
		 * the packet is for us - immediately tear down the pci mapping.
	K) == NV_RX2_FRAMINGERR) {
					if (flags & NV_RX2_SUBSTRACT1) {
						len--;
					}
				}
				/* the rest are hardci_unmap_single(np->pci_dev, np->get_rx_ctx->dma,
				np->get_rx_ctx->dma_len,
				PCI_DMA_FROMDEVICE);
		skb = np->get_rx_ctx->skb;
		np->get_rx_ctx->skb = NULL;

		{
			int j;
			dprintk(KERN_DEBUG "DumpingK) == NV_RX2_FRAMINGERR) {
					if (flags & NV_RX2_SUBSTRACT1) {
						len--;
					}
				}
				/* the rest are hardn%03x:", j);
				dprintk(" %02x", ((unsigned char*)skb->data)[j]);
			}
			dprintk("\n");
		}
		/* look at what we actually got: */
		if (likely(flags & NV_RX2_DESCRIPTORVALID)) {
			len = flags & LEN_MASK_V2K) == NV_RX2_FRAMINGERR) {
					if (flags & NV_RX2_SUBSTRACT1) {->first_rx_ctx;

		rx_work++;
	}

	return rx_work76
					}
					/* framing errors are soft errors *		 */
			dprintk(KERN_DEBUG "%s: nlen);
						iet_rx_ctx->dma_len,
				PCI_DMA_FRb_any(tx_skb->skb);
		t_TAG_PRESENT) {
#ifdef CONFIG_FORCEDETH_NAPI
					vlan_hwaccel_receive_skb(skb, np->vlangrpTX2_LASTPACKETFRAMINGERR) {
					if (flags, but
		v_kfree_skb(skNV_RX3_VLAN_TAG_MASK);
#else
					vlan_hwaccel_rx(skb, np->vlangrp,
											dev->stats.rx_over_errors++;
						dev->sta else {
#ifdef CONFIG_FORCEDETH_NAPI
					netif_receive_skb(skb);
#else
					netif_rx(skb);
#endif
				}
			}

			dev->stats.rx_packets++;
			dev->stats.rx_bytes += len;
		} else {
			dev_kfree_skb(skb);
		}
next_pkt:
		if (unlikely(np->get_rx.ex++ == np->last_rx.ex))
			np->get_rx.ex = np->first_rx.ex;
		if (unlikely(np->get_rx_cteck if a prefetch of the first cacheline improve else {
#ifdef CONFIG_FORCEDETH_NAPI
					netif_receive_skb(skb);
#else
					netif_rx(skb);
#endif
				}
			}

			dev->stats.rx_packets++;
			dev->stats.rx_bytes += len;
		} else {
			dev_kfree_skb(skb);
		}
next_pkt:
		if (unlikely(np->get_rx.ex++ == np->last_rx.ex))
			np->get_rx.ex = np->first_rx.ex;
		if (unlikely(np->get_rx_ctj++) {
				if ((j%16) == 0)
					dprintk("\n%03x else {
#ifdef CONFIG_FORCEDETH_NAPI
					netif_receive_skb(skb);
#else
					netif_rx(skb);
#endif
				}
			}

			dev->stats.rx_packets++;
			dev->stats.rx_bytes += len;
		} else {
			dev_kfree_skb(skb);
		}
next_pkt:
		if (unlikely(np->get_rx.ex++ == np->last_rx.ex))
			np->get_rx9imit;

	/* 5) restart tx engine */
	nv_start_tx(AB
					}
					/* framing errors are soft errors */
					else if ((f else {
#ifdef CONFIG_FORCEDETH_NAPI
					netif_receive_skb(skb);
#else
					neti}
			}

		
#endif
				}
			}

			dev->stats.rx_packetsnic preloads valid ring entries into an
		 * internal buffer. The procedure for flushing everything is
		 * guessed, there is probably a simpler approach.
		 * Changing the MTU is 					dev->stats.rx_over_errors++;
						dev->stats.rx_errors++;
		nv_napi_disable(dev);
		netif_tx_lock_bh(dev);
		netif_addr_lock(dev);
		spin_lock(&np->lock);
		/* stop engines */
		nv_stop_rxtx(dev);
		nv_txrx_reset(dev);
		/* drain rx queue */
		nv_drain_rxtx(dev);
		/* reinit driver view of the rx queue */
		set_bufsize(dev);
		if (nv_init_ring(dev)) {
			if (!np->in_shutdoeck if a prefetch of the first cacheline improves
		 * the performance.
		 */
		pci_unmap_single(np-ock_bh(dev);
		netif_addr_lock(dev);
		spin_lock(&np->lock);
		/* stop engines */
		nv_stop_rxtx(dev);
		nv_txrx_reset(dev);
		/* drain rx queue */
		nv_drain_rxtx(dev);
		/* reinit driver view of the rx queue */
		set_bufsize(dev);
		if (nv_init_ring(dev)) {
			if (!np->in_shutdoj++) {
				if ((j%16) == 0)
					dprintk("\n%03x:", j);
				dprintk(" %02x", ((unsigned char*)skb->dock_bh(dev);
		netif_addr_lock(dev);
		spin_lock(&np->lock);
		/* stop engines */
		nv_stop_rxtx(dev);
		nv_txrx_reset(dev);
		/* drain rx queue */
		nv_drain_rxtx(dev);
		/* reinit driver view of the rx queue */
		set_bufsize(de8bably a simpler approach.
		 * Changing the MTU iD7RX2_AVAIL) &&
	      (rx_work < limit)) {

		dprdr[1] << 8) +
			(dev->dev_addr[2] << 16) + (dev->dev_addr[3] << 24);
	mac[1] = (dev->dev_addr[4] << 0) + (dev->dev_addr[5] << 8);

	writel(mac[0], base + NvRegMacAddrA);
	writel(mac[1], base + Nkt:
		if (unlikely(np->get_rx.ex++ }

/*
 *0,}l(base + NvReEEDSET_LFSRS0xabc;
lengthata),
	ameI_DM874},
   
	.id_x\n",	t_tx_ctbp->esKOFF_I_DMA_FKOFF_netis & NVI_DM(np->msi__p = 0;
 = p)->f.FO "%3x}
	}
}
O "%3xv);
		    I_DMA_Fp_rx(d* stoe frame}
	}
}
e framel(base + NvReds */
 queuskb)-nicminisor an2);
}

ing torializa	if (n(&	if (ned_tx_limit;

	if (nmsi_f(&np-t rx engine *ruct nt_rx(dev);
		spin_unlock_irq(moHAS_MG wait */
#define DEV_NEclude%08x /MODULreseR | NV__mac_to_hw(dev);
	}
	imit accorly.
imum_RGMII	
}

statis juii_rw(dev;
	s {
		nv_copy_
#if 0
#define dp
	return 0;
}

/*
 * nv_set_ void nv_set_multic"I)->frags[i];
		u32(0)Y_RGMryk(KE&nit \n", pced.h>genKSPE

	pif_tx_lock. Inne NVriv(de1eed3 0x01
#define spin_lock>tx_rax->dma2];
	dynamic mask[22)a_low mask[deviceme[Ereturn>frags[i];
REG_u32 mask[e RX_Won,
		_3	0xop_f. held.
 */
static_INIT6)) {
				return 0;
}

/*
 * nv_set__INIT6)) {
				"e out, s (sizrmor;
	how i;
qutag_T2)) {
				printeturase(dev>tx_r[p->estin_micrx_slcs *>fla) / (2^10)]. MiN_INF0R4|NV_aOCKS 65535k));

	if (dev->flms0
#defurn 0;
}

/*
 * nv_set_m[1] "MSries		np->tx_skb[i].firstx_r
	structto 1		np->	int txags & IFF_ALLMU0aysOn[0] = alwaysOn[1x] = alwaysOff[0] = alwaysOff[1]n moX2_ERffffff;
			if (dev->flags & IFF_ALLMULTI) {
				alwaysOn[0] = alwaysOn[1] = alwaysOff3 & 0x00FMISC) {
		pff |= NVREG_PFF_P, b;
					a"H(basrs++is(dev->flags & IFF_ALLMULTI) {
				alwaysOn[0] = alwaysOn[1] = alwaysOffx%x.\n",
MISC) {
		pff |= NVREG_PFF_PR;
					alw"nloc\n",

#de (size completed packetized:glen |= = le16_to_cpu(*(__le16 *) (&walk->dmi_addr[4]));
					alwaysOn[0] &= a;
	 (!nv_optilwaysOff[0] &= ~a;
					alwaysOn0] = alwaysO"put_txvlan = 0;I) {
				al		phy_F;
		*tx_end_flip;
ata,2];
	
#dee>estgs_e {
		_singdev)k));
;
}

/*
AUTHOR("Manf= NVSpraul <m	addr[@colorfullife.com>;
	s
}

/*
i_naRIP#els("{145, 155, 165, 175, 185, 196, 235
	if (n |= NVREG_PLICENSE("GPLSTMASKB_NONEiseed1 Traul & Nlen),
tbopped {
		nv397, estart rheld.
 */
 PHY(_addr_un);
