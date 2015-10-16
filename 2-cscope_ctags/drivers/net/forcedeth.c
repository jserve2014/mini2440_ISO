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
	 a cleanrautoneg && eanr Thisntrolle& NV_PAUSEFRAME_AUTONEGmentat	adv_ Thilersadv & (ADVERTISE docum_CAP| gerleanuinceand ASYMIDIA		lpal-Daniel , nFfinLPAcey.
 *
ndreNVNVID made QuNVINVIDswitch (CarForce ten by chernw de Quincey.
 *
nd:tradeion , nForce a&re trademarCAPks of N	e      engine|=red   anden byiRX_ENABLE tradationEthernC) 20enginee 2003,4,5 ManfrTX_REQ) * Coprt)
 yright (C) 2003,4,5 ManfrTd SpraulleanCo}ger (break trad tradeCorporopyriginclean Uarksd Statesy.
 *o==ror rrademarks aher
 * counks nsanet)
.
 *nger (pyright (C) 2004 Carl-Daniel Haiher ger (invalid MAC handling, insane
 *		IRQrks arw de Quinceyt (c) 20 fixes, bigendian fiedialeancountrieyright (c) 2004,2005ener6,22007,2008,2009 Nl Hailfinger (pyright (C) 2004are; you can redis (wol suppyrigh
 * the Free Software  trademarNVIDIA Corporation
 *, bigendian fixesfication)
 * Coms of the GNU General Publi007will8will9 blished by
 * tion
 *
 * This pion
} elses of N      enginersee Software Fouclea}
ILITnv_updateForce (dev,:-Dani enginr region txrxF Andrew deRESTART_TXbutenv_start_tx PARleanSee three  GNU General PublRc License for rore deta
	return retval;
}

sanfrc void nv_ andchange(struct net_device *the 
{stion b FITNESS  anddr NV the 5 Manfron !netif_carrier_okc., 5, Inc.	Temple Place, Sn the GNi		printk(KERN_INFO "%s:e
 *  up.\n", dev->nama acBostv_ *
 _gatAre dTIfali hardwrifi copy of
 *
 GNBTY oY even * Thion bon, MA  02111-uite 330, dwar 330MA  02111-ff307  USA (c) 20Known bugs: * YWe suspdownt that on some harre genno TX done intetrutverifigouldopdt (c-Daniemeanseived a ctivalong with _irqrogram; if not, wre onto
 *
u8 __iomem *edian= get_hwedia307  USAu32 miid a ou srrupts, = readlme* Thet drivMIIbigeuOSE.	.
 * l(NVREG__TIMTAT_LINKCHANGE EliablEV_NEEDdataERftwafrodrupt fires (100 times/secontaniep irq, d a us 0x%xonfigurable wit,en you cSE.  ls.
n you ca&_NEEDer_data trollactivDEVthe Frx pack gener307  USAme nicableaneDEBUGre ge, onlytimer anotificc., 59FAULonfigurable with NQMask, or if a rx msi_workaroundrrives bfe_priv *np you(c) 2NeedIf yooggl.
 *
 msiatin maskx pacin.h>
#eEED_TIMurabice,
	 * oe.h>wise, futurLT)
 *n you  willdule bthe Gected.linuihe Freeanr>
#i4 Andrew deMSI HailfiD5 Manfth NVREG_Preliablyse
 *ediaE.  im.h>
#d0_NEED will not SIIrqMask in tx/skbuf
#definSI_VECTOR_0oolrdevirdeviinclude </spix/miirdeviIRQmii., or inlint.h>
 rx  gener_h>
#incls_modprrives by chance.
 * If,nit.htotalx/ini your x/ <linux/s<linuxncludtdev/uacc * ThisU fromoptimizux/inh>

# proNV_OPTIMIZATION_MODE_DYNAMIC, Inc., 59<asm/io.h> >tk		ine dpr_THRESHOLux/initc) 2transix/inito pollNEED_t.h>
#inclschinlo	inuxfquiet_ thecan t (copyright (irq>
#in!ntk	defiux/iASK_CPUeETH_V
 
#def0)
#DEVh>
#include <lrq fl the Lhould h1Y;x pacout recovery iion base * H NVREG_P<le (0)
#
 * MAX_QUIET_COUNTrq f0x0 ReliIMER       ++ask */fine DEV_NEc) 2reached a periodd.
 low activity, tew d
  /*   640000r tx/rx packetWORK_PER_LOOPuppor
r is00001  /* se.h>
#include <l  THROUGHPUttings. R Reli1  
 * No.h>
#timeratinrq flaq flag in tpV_NErq <linu* ANY W/
#deans the irq 0x/init.h>
#iirqhould _ce suppcet arit.hfoo, if a *ne Fnux/init.h>ay chance.
 * Ifan rrives by chance.
 *) 001 ;DEV_HAS_Vsm/uaccess<linux/init.h>taggsystre <linux/timer.h>
#ux/modenditxude <#ifndef CONFIG_FORCEDETH_NAPI
	 */
#defi{ } whng andit.hloopNVREG_Png an02  /f
AME			"f
 * Foun" dpr/init rxNVIDcksnfigurable with Nemrdev!linux/init.h>
#inclethtol HailfiD330, Bo
/*
recotrantremov or iine DEV_ux/ml not harx/init.h*/00001  /*NEED_TIMERIRQ1*/
#des. Rel device supdevice suppine HAS_STATISTICS_VMSIXstics version200 deviot, wriion 2 os hw ense
. Rel6e supports }ce supports MSI-X */
#defiirq: %08xerfluousorts 64bdevice sup USAls.
  /*00001  /*r, or 1  /* sthe Fhould hIRQ_NONE*/
#e <linx/init.h>
#inp*/
##ifts hw statisticthtodevice suon bapi_s_LARul youep(&
/*
1  /330, Bo/*
	#incDisable furdela. Re's (msix000  enot, d supp*/
#de2ion uppox/skbuff01000  /* devinclude <lini___HAS_STATISTIUNIttingaddrine 
#reco
	do
f theit.h_MSI_Xaddreevice(HAS_COLnv_rx_pro ande inteRX_nd needs pack)ly happe0000unlikelyee Sallocd.
 * Thly happe	spin_lockct macsionverifi., 59 T0)
#n_shutd, c 2 of tmod_rts 6ct macoom_kick, jiffies + OOM_REFILived sta versunsione supports hw st*/
#defstics. Rice supports tx paHAS_C+versitx_t.h>e inteTstatistictx collISIOsupports hw statistictx ncpports } w
#
 *
 * Thi
		TATISTI_HAS_=TISTIess oics versios 64b}
	whi0x00ics version< maxa-mapping.hn 3 *ics vers>
#incldm
#define DEe dprTf the	do { } wED_Ts. Re* Note:newnit.h>
#inCORRECT_MACA_HAS_STATISDRcs versio4e supports txrivex device suppD, 593c test 
#includeEDETHo limit version 2 */
#define DE frames a arive in t supports tx pause framep} supports hw statisneed or ic tesversc te_after(ocumentisticsames c teouttatiTX_VED_PHY_INIT_FIOLLISIimit    00suppo supRV_NAM supports hw statisticaroua
/*
 ands specia =documentatiDETH willO001  nd/* device supne DEts gearion * deviceRECOVER_ERROR power up nclude <li device suppporports hORK_PER_LOOuppohe nicrevs *version1e supports hw statisticefine savevic/
#defiDGEAR_MODE          0x0200eerc teice supports TATISTIGEARe
#de dma */
#d0xice Ates and e
 * 0080 0  /* deviceua corives happen.h>
ic_001  /* 000  /*1  /* sthe tip->recodefierror =CHECKSUon 1 device sups. R20
# documentatiPOLL_WAIe in the eedsi workfic phyREG_IRQSTATpports nded diagnosti-Xus = 0x000,
#defHAS con, Mted_POWER_CNTRLc addreT_EXTENDED HANDLED
#def/*c) 20All _
#if 0
ed func
#deverifiu supto help increTATIperformanceiver(reduce CPU anuppoOK|rivethroughput). TheyRR|N descripter IX  /ini3,iverPUT	iler direEV_NEEDvver_00001  memory acdevi    e NV0020000  /* devi.
 *e DEV_HASowerALL		(um offload    0DEV_HAS_STATISTIVLAN dma */
#detings. Reli2 supports hw statisticvlan taggingMER|Nstriine VREG_IRQMASK_THROUGX_LIetupReg6 = 0x00gs. Reli4 supports hw statisticstic test */
e interval leCOLLISI the timer sourc8 on the nic
 * NVREG_POLL NVREG_IRQMASK_THROUGLINK|NVREGf
#define NVREG_IRQG_IRQ_RX_ERROR		0x0001
#define NVREG_IRQ0x00_DEFAULT=97 would reCS_V2      00600  /* device supports hw statistics versiostics_RX|N, 59FORCED		0x00_STATISTICS_V2      02= 0x008,
#de/
#define DEp0 = 0x020,
	NvRegMSIMap1 = 0x024,2	NvRegMSIIrqMask = 0x030,
#defin3= 0x008,
#dee	NvRegMSIMap0 = 0x020,
	NvRegMSIMap1 = 0x024,s gear m= 0x00c,
#defiESMASK_CPU		0 0x008,
#dice supports hw statisticextit *d diagnoIMap test_DEFAULT=97 would resGMTect mac addreUGHPUT	HAS_COpports hw statisticmanagn, In uni84,
#define NVREG_XMIevs */
#defi2
#define NVREG_IRQ_RX_NOBU statisticcorrect mac addr/* dos juNVREG_XMITCTL_SYNC_MALLviceNnd */
#defineth oET_ASSEgs.
RX_ALG_0x1000000  /* devi* deix024,fi /* device supTATISTIPAUITCTL_SEMA_MASl power es version 2 */
#define DE00  /* devicee NVREG_IR = 0x024,
	NvRegMSIIrqMask = 0documentatiwer e NV80
#deSET_ASSERT	0x0F3
	NvRegTne NVREG_XMITCTL_HOST_SEine NVREG_MISC1_PAUSEfine NVREG_Xx0
#define NVRK	_HOST DEV_ED	0x00004000
#define NVREG_XMITCTL_TX_PATH_Ec

	NvRegMacReseth>
#inX_LIMART	0x01
#defineG_XMdefine NVREG_00
#det DEV_NEEtXMITCTL_MGMT_SEMAL_DATA_ERROR	e NVREGTL_HOS1RegTransmitterStatus = 0x088,
#def, exp	0x0opy e wite NVREERROR)

	NvRegUnknQ_TX_ERR		0x0008
#define 
#define NVREG_XMITCTL_DVREG_IRQSTAT_MIIEG_XMITSTAT_BUworkaround */
#define DEV_NEnsmitterStatus = fine NVREG_IRQ_RECOVER_ine NVREG_XMITSTAT_BULOWf
#defid */
#de0xvRegTrvRegOffloadConfig = 0x90aldefine NVREG_IRQSTAT_MASK		0x83TISTICS_VX_LIMd */
#defegPackeice sAL	RX_NIC_BUFSIZE
	NmsiREG_IRQSTAT_MAS
enumenta drivIrqMERIRlers.NVRE,RROR)

	Nriver_IRQCS_V_MIIEVENT	ine DREG_RCVSTAT_BUSY	0x01

		0x0	0x83ffStatus = 0mii.define 4VREG_RCVSTAT_BUSY	0_ed S40
#TIMEC_REdefine NVREG_SLOTTIME	0_FULL	2define NVREG_SLOTTIME_NOBUF0_FULL	orts ne NVREG_SLOTTITE_10_0_FULL	8REG_SLOTTIME_HALF		0x0OKEG_SLOT1e = 0x9c,
#define N will EG_SLOTdefiG_SLOTTIME_HALF		gs.
EG_SLOTme = 0x9c,
#define NIME_rts MS0_FULL800007f00
#define NVREEG_TX_DEFERRA10l = 0xA0,
#define NVRVENT	0x_10_100_FU82DEFERRAL_RGMII_10_10RQ         LINK|NVREGNX_NOFULLdfEG_TX_DEFERRAL_RGMII_10CPU0_FULL6ASK		0x0#defefine NVREG_SLT_SEMA_
	NvRtx_IRQ_RECOVER_ERROR)

	NvRegUnknownSetupReg6 = 0x008,
#define NVREG_UNKSETUP6_VAL		3

/*
 * NVREG_POLL_DEFAULT is the interval length of the timer source onT)
 *G_SLOTTB = 0xitatunsigned  a rxR * DTACVSTAT_BUSTX_DEFERRAL_RGMII_ice TIME1t_XMITCTL_MGMT_SDATA_opy (i=0; ; i++NVREG_Ssk = 0x030,
#define NVREG_MSI_VECTOR_0_EG_IRQSTAT_MIIINK|NVevice suppoKA     TIMEfkB =c1 = 0x080,
#define NVREG_MISC1e supports MSI-X */
#defitxEG_MISC1_HD		0x02
#define NvRegMSIMa_RGMII_3c

	NvRegMacReset = 0x34R	0x20
#def version  /* sav	0x0
#dX_DE,tAddrthe ne DE_XMITCTL_DATA NVRRe NVRE NVREGSK		0x000000ff
ine NVREG_ /* destorLne NAULTEG_SL7 Reliee NVREGSK		0x000i >fine NVREG_XMITSTAT_NVREG_SSTAT_BUSBKOFFCTRL_SELECT			24
#define0
LEGBF_ENABLED	0x80000000
#define NVREG_x/init.h>
#incstmii.BdefinBCVREG_RCVST0  /* deviceG_SLOTTIME_DEFAULT	  	0x00007f00
#define NVREG_SSLOTTIME_MASK		0x0C) 20ticastMaskB = fff
NVREG_TX_DEFERRAL_DEFAULT		0x15050f
#define NVREG_TM     G_SLOTTIME_HA00,
	NvRegSELEC			22VREG_SLOTterrupt fires (tatus PhyIntoo4000y iter0  /*s (%d) inTf000AG_TX_D	0G_IRdevice supporne NV *
 * This rive_PAUSE_TX	0x01
#define NVREefine NVREG_LRCVSTAT_BUSfine NVREG_MCASTADSTRETCH_10	RETVAL(ne N070fgTransmitterControl = 0x084,_		3
s toR|NVRE0)
#eefi0x0000020e NVRives b*SELEinit.hbudgetriping */
#defiing and str   eainworts T=97g a de <asm/uacc,NVREG__VAL		3

/wnSetupReg6 = 0x00>
#ioev_SELECT		0x16Status Macx70000efinA8,StatuT1	0xulticas0x70000=efine etcodeSK		0x0txTSTAT, r_IDLE	1
#definenv_MGMT_SEMA_npdefine NVREG_IRQ0,
	NvRegRxRingPhysAddr = 0x10L008
#defFULLNVREG_XMIT3   NVRtx_ring_sizd the efine NVREG_LINKSPEED_10	1000
#define NVRE NV
#definne NVREG_XMITCTLot, wrVSTAT_BF)
	STAT_BU NVREGfine NVREG_XM024,
	NvRegMSIK		0x000003ff
#dTXRXNVRERXCHECKotTimeNINS	0x00080
	NvRegTxRL_DATA_START	0x0_1	NINS	0x00080
	NvRegTxRinDESC_2TIME_M_DEFERRAL_RGMII_10Frame = 0x1703	0xc02NVREG_TX_DEFL_DATA_START	0x0RegTxRinownSSTRIPMODE    SK		0x0xK		0x000003ff
_PFF_PROMISSTAT_BUT	 	0x00007f00
#defineRegTxRingPhysAddrHigh =define NVREG_XMITCTL_TX_P_SEMA_ACQ	0x0000f000
#define NVREG_XMITCTL_HOST_LO_SLOTTIME_HAgs.
SPCVCT10	ice SK		0x000000ffrive1

	NvRegPacketFilterFlags = 0xLE	0x0f+0
#definDATA_ERROC	0x80
#define NVREG_PFF_MYADDR	0x20
#define NVREG_P0,
	NvRegRxRingPhysAddr = 0x10#dG_IRRegOffloadConfig = 0x90,
MAC_RESET0_BIT1	0xTIMERIR#defin18n 1 */
#defiHY	0x601
#define NVREG_OFFLOAD_NORMAL	RX_NIC_BUFSIZE
	NvRegReceiverControl = 0ULL	7SK		0x000000ff
REG_REG_SLOTK|NVRVCTL_START	0x01
#define NV0x00010000
	NvRegMIIStatus = 0x1_ATH_EN	0x01000000
	NvRegReceiverStatus = 0x98,
#define NVREG_RCVSTAT_BUSY	0x01

	NvRegSlotTime = 0x9c,
#define NT31	(1<<31)documentatiSpraul_V0x0f09f0088efine NVTxVREG_SLOTTIME_MASK		0x000000ff

	NvRegTxDeferral = 0xA0,
#define NVREG_TX_DEFERRAL_DEFAULT		0x15050f
#define NVREG_TX_DEFERRAL_RGMI_SLOTTIME_HAADAP NVRERUNNINx0561008

	ine NVRETxRxCABLE_V1	ld ha#define MIIControlLE	0x0f<gMIICont88,
#defre-ne NVRED	0x800000HAS_C00000
#define NVREGASK_ast#defineine NVREGBroadT2	0efin004
#define NVREG_IRQ_TX_ERR		0x0008
#define ne NaNVREG_TegWakeU0x040
#		define NVREG_T)
	NvRegTxWatermark = 0x130RXCTL300_TX_DEFERRAL_RGMSHIFT	8
#deftermark = 0FORCE5AGS_Status RxDeferd hafine 
#define NVREG_SLRHIFT	8
#defVREG_TXRXCTL_BIT1	0x0002
#define NVREG_T	0x0002
##definAC2
#defineL_BIT2	0x0004
#d0xBNVREG_RCVSTAT_BUSMCASdefine NVREGr_TX_AGS_ACCEPT_LINKCHANG#define4LAGS_ACCEPT_LINKCmii.E	0x04
8define NVREG_WAKEUPFLSLOTlticastMaR
	NvRegLi0x204,
#define NVnitVerine NVREG_RINGON     	0x01
	NBticastMaskB =
n = 0x2definrerface	0x04CNVREG_RCVSTworkCASTAFORCEr = 0x10tatus BackOffC  engiWERCAP
#define 	0x01


#define NV_DATA_START	0x0s. RelifgMIIStatus3ff
#define NV0
#define FREEwerS		0x000003ff
#define NVHOSdefin02
#definRxRinfinesx700INKCHA04= 0x190,
#define NVREGPausST_SEMA_ACQ	0x0000f000
#define NVREG_XMITCTL_HOST_LOADED	0x000040efine NVREG_ADAPTCTL_RUNNING	0x10L NVREG_IREG_LINKSPEED_1Q_TX			1268,
#defT0x0100
#define NVREGTE_VALID		0x0100
#define NVREG_E_VALID		ingSizeLINKCHA0NITGETVERSION    RINGSZ_TXSHIFT ION		0x0GMTct mVERSISZ_RvRegTxZTL_BIT1	0TER_LmitPol)
#def10cVREG_RCVSTAT_BUSTRANSMIT_TX_FMAC_f000_REVwerStat800 NVREGx024nkSeth:ManyRe1NVREG_RCVSTAT_BUSx00010000
rts Mine NVRErrier = 0x298,
	NvRegTxExe NVREG_POWERSTATE_D2		00010000
	NNvRegvRegRxFrameErr = 0x2a4,
	NvR0	5MIIStatus = 0x18= 0x2a4,
	SLOTT(0xFF_VLAdefinUnkiresNoteReg5ManyRe3NVREG_RCVSTAT_BUSUNKSETUr5_BIT31	(1<<31vRegRxFrTxWatern)
 2b4,
	Nmt = 0x28c,
	Nvfine NVREG_TX_DEFERRAL_RGMII_SIdela_WAKEUPFLAGS_D1SHIFT	4
#define NVREG_WAKEUPFLAGS_D0SHIFT	0
#define NVREG_WAKEUPFLAGS_ACCEPT_MAGPAT		0x01
#define NVREG_WAKEUPFLAGS_ACCEPT_WAKEUPPAT	0x02
#define NVREG_WAKEUPFLAGS_ACCEPT_LINKCHANGE	0x04
#define NVREG_WAKEUPFLAGS_ENABLE	xUnic1111

	NvRegMgmtUnitGetVersion = 0x204,
#define NVREG_MGMTUNITGETVERSION     	0x01
	NvRegMgmtUOTHEnitVn = 0x208,
#defineMII_L_MGMTUNITVERSION		0x08
	NvRegPowerCap = 0x268,
#define NG_POWERCAP_D3SUPP	(1<<30)
#define NVREG_POWERCAP_D2SUPP	(1<<26)
#define NVREG_/*DEV_HA<linfn cleanweTISTILARGmaxB = 0xEV_N.
 *		0x0sre NVREdr = 0x100,
	NvRegRxRingPhysAddr = 0x10K		0x000000ff
INKSPEED_10	nt = 0x2werStateLEGBs untested */
struct ring_fine NVREG_TXRXCTL_D0x10e NVREG_PFF_MYADDR	0x20
RegMgmtUnitControl = 0x278,
#define NVREG_MGMTVREG_ADwerSta 0x3f0,,
	NvRegTxRetryErr = 0x2a0,
	NvRegRxFrameErr ED_Mion base1
#define NVREG_OFFLOAD_NORMAL	RX_NIC_BUFSIZE
	NvRegRece;
	__le32 tx		3
fine LEN_Mtrollen;
};

un, 59efinetyticaL_START	0x01,
	NvRegTxRetryErr = 0x2a0,
	NvRegRxFrameErr =ATH_EN	0x01000000
	NvRegReceiverStatus = 0x9 2 */
#deSTAT_BUSY	0x01

	NvRegSlotTime = RegMgmtUnitControl #define NVREG_XMUNITCONTROL_INUSE	0x20000
	NvRegTxCnt = 0x280,
	NvRegTxZrbigen2	0x046 NVREGvRegTxOneReXmt = 0x288,
	NvRegTxManyReXmt = 0x28c,
	NvRegTxLateCol = 0x290,
	NvRegTxUnderflMII_LINKCxDeferral = 0xA0,
#define NV
	NvRegTxLossCarrier = 0x298,
	NvRegTxExcessDef = 0x29c,
	NvRegTxRetryE#define NVREG_XMRxFriveTooLoG_SLOTTIME_HA
#defefinE_D3pe {
	s3n = 0x20gmtfixe(1<<26)
#def27NITGETVERSION      = 0x28CONTROL_INUSEREDAGS_D 0x2bc,
	CnKEUPFx28TE_VALID	TxZRLOSTFCSEr27)
	__le32 flTA_ERATE	0x000400<27)
#8efine NV_TX2_UNDUNDERFLOW	<27)
#9efine NV_TX2_UND10_100_CSErr0000G_SLOTTI	NvRegTxLossCarrier = 0x298,
	NvRegTxExcessDef = 0x29c,
	NvRegTxRetryErr = 0x2a0,
	NvRegRxFrameErr = 0x2a4,
	NvRegRxExtraByte = 0x2a8,
	NvRegRxLateCol = 0x2ac,
	Nfine egRxRunt = 0x2b0,
	NvRegRxFrameTooLSong = 0x2b4,
	NvRegRxOverflow = 0x2b8,
fine LID		0FCSErne NVR2bcE_VALID		0MASK	AlignAN_TAG_PRcTE_VALID		0Lee NV_RX_DESTUNITCONTR0x08_WAKEUPFLAGS_D1SHIFT	4
#define NVREG_WAKEUPFLAGS_D0SHIFT	0
#define NVREG_WAKEUPFLAGS_ACCEPT_MAGPAT		0x01
#define NVREG_WAKEUPFLAGS_ACCEPT_WAKEUPPAT	0x02
#define NVREG_Wx04
#define NVREG_WAKEUPFLAGS_ENABLE		est_POLL_DEFAULT_THROUGHPUT	65535 /* backup tx cleanup if loop max hed */
#define NVREG_POLL_DEFAULT_CPUG_IRQSTAT_MIFORCEH_DM = 0x3f0,

	NvRegPowesIM1<<27)
#define NV_Tp1 = 0x024,
	NvRegMSI#define NVREG_MGMTUNITGETVERSION     	0x01
	NvRegMgmtfine NV_TX2_ME_10_10 = 0x2bV_RX2_CHECK1|V_RX2_CHE_EN	0x01000000
#defiVREG_IRQ_TX_OK		2_FOR2 NV_TXUP_SLOTTIME0F15
	__le32 flagleNV_TX2_FORV_RX2_CHREG_POWERCAP_D2SUMMASK	(0x1C00 0x34,
#define NVREG_RC0EUPFL_RESET_ASSERT	0x0F3
	NvR NVREG_IRQSTAT_MASK		0x8e NVREtrCVSTAT_BUSY	f000
#define NVREG_XMITCTLdefine NV_RX2_CRC000ff27)
#define NV_TXvRegRxFCSErr = 0x2bc,
	NvRegRxFrameAlignErr = 01ux/init.h>
#inclpcset3 */x_RL_Dor_mapping */
#defiirq01000  /*)
 *NT	0FLentaIN_SEMA_FRyouEV_HAS_MSI                0x0000040  e NVRFLAGS<linsixmas to0BSTR/* EachED	0x80000 bit can/inidefi__le32a ULL	NG000f( (4IL		s)x/spiRX2_EMap0 represT=97 <<30)
rst 8ED	0x8000000 = 0x0)
#de12V_TX2_2|NV
#incrts jemaining3)
#defERR|NV2_ERRnlotGetVeOR		( i < 8n = 0x204,
000
#T=97 wou>> iVAIL0x0x00007fTX2_ERRO|=HECKSUMM<<3_VL<< 2IRQ h>
#ILOTTIME_030,
#define NVREG_MSI__RX2) |_TX2_ERRGS_Define NV_TX2_RV_PCIardwnX2_ERROR		(1_RX2celaAN_TAG_PRESENT CSEr16   	0x270
#defX3(i + 8)0x2d4
#de= 0x2b0,GS_DF
	NvR
/* Miscelanevice <linux/timeaes, G_SLOTs:ne NVREG_PFFed d1I_RE0,
	VERstics vOST	7 is untesRX2_ERROR32)t uni	__lrequest /* d
#desof NVIX_FOactivIfinit.hC000000

#d_TX_WM_DESC2_3_1000	0xfeimern NVREe NVREG_WAKEUPFLAGS_ACCEPT_WAKEUPPAT	0x02
#de_SLOTTIME_HALFKEUPFLAGG_IRQ_TIMER|(*is prer)_IRQ_RECOVER_ERROR)

egPacketTX2_UNSTOPen byETUP5_Dne NVRE NV_RX2_MIS024,
	NvRegMSI	0x01

RegTxRingESET(0)
#/
#definX2_CHECDELAY	4050f
#def0
#define NNV_MIIBUSY_DELAY	50
eUpFrollers.X_ERROR		0x0001
#define CAPilfi is un    	0x2d4
#defiXvRegM is untested 0x29RESdo  0xST=97 )n = 0x204,
NGERRvicex_entry[i].ys: a = NV_R 2 */
#deol = = tes ne NVRNGERRght (Ccihance0x14C <linu */
#,UPPATTERNS		(0x14000000_WAKEUPSLOTENTRIE)) prog5 Manfld ha <liight (C) 200efine NVREG_Ies0 timt 2 _ALL	<30)
#ded*
 * K	
 *
 * K
#even24
#dn intervalIX  !YMAX	5TTERNS	5
<<24)R*/
#denit.hx700rx ine NVFRA  /*forsc) 20fe FLAGame_rx,ctiv-rxigurable with NVREontrol NV_TX2_UNSe TX_RIN2
#defiu	256
#define RX_RX].ROR_MA12 of th&wTAG_PRENVREG,PU		F_SHAR   064)  	0recovm NVREn, M5 Manfterrupt fires (100 tiorts MSI-: LLOC_PAD		( faiERROlack*/
%#defLLrENABLE_eXmt = tatus =<30)
#deRX0x280EG_WILL	(1000  /* device = ~24
#definAIT	(* poDES		goto outne NVCKSUM* rx/t	3

+ afine + s*/

tusec */
#defiRX_HEADERS		( known */tn mf tht*/

.n usec */
#defiRNK|NV to NVidi kno  	0maximum mtu siz* device sT#definKTRROR	_1	I */G_BKOLEN	|NVRardDEV_NEEnot eTooL */|NVR*/
#defins.
 * - 2	9NvRe/* ActualDEV_NEEaccordDEFAto NVidia: 920ine N
tG_SLOTTTX_V2   0x	(1+HZ/2    	0x270
_TX_FORCEELL		0x1     	0x270
= 0x	NvRegUT	(3*HZ1<<30)
#deefinS_INTERVAfrereco10SSE		
/c) 20|NVR_maxivalue0 tiecon0x10 NVREGThe nic supports three different fine scriptfine  types:
 * - DESC_VER_1: Original
 * - DESC_VER_2: support for jumbMII_LIrames.
 * - DESC_VER_3: 64-(1<<30Iformat.
 */
#define DEfine PHER_1	1
#define DESC_VER_2	2
#define DESC_VER_3	3

/* PHY defines */
#dTEK2	(1<<30)OUI_MARVEtype {50433SUPP	(1<<30)V_RECICADApe {
3f0x00007f00ine PHY_VITESSstMas01c_8201		0x0200
#defREAtTE
	__le73IME_10_ERRORK_PER_LOOnetdhei <lispine NVRECKSUMMV_RX_HECT_MACADDR    0x000400lin*/
#defiarou1MODEfTERNS	5
#defworkMODEL_0000fTCTL_HINGERR)GERR	(ame f	(1+Hsupport for jumbneT4	0xUndNKSPoH_DAT_TX_DEfine PHY_CICADA_INIT3arou_CICT_LO24
#definTICADA_INIT6	0skB = 004
#define PHY_CICADA_INIT6	0x02000
#define PHYRLOST	(FLOW	(1<<28)
/*E_D2		0e NVREG_PFF_ALWesc_ver values:
allTE_C0
#de000  /*RQ_TXting to NVidi * - DESC_VER_2: support for jumbALLHYID2_MOD_SLOTT_CICAD
#de000
ALTvice supporER_1	1
#define DESC_VER_2	2
#define DESC_VER_3	3

/* PHY defines *ne PHY_OUI_MARVELL		0x5043
#define PHY_OUI_CICADA		0x03f1
#define PHY_OUI_VITESSE		0x01c1
#define PHY_OUVELLK		0x07E_INILTEK_8_E3016T6	0xSLOTECKSUMM0INIT6	0x0e00
#define PHY_CICADA_INIT3arou,
#deedefine PHY_CICADA_INIT6	0x 0x19X_ERR
#definentrol = NV_PIX   or   /* device supportETAY	50
# forTCHDOG	NvReg	(5		0x07<30) PHY_OUI_CICANATE_efine NVREol s
#defIN	efin_MASK	(
#define STforce_SLOTTIME_WAIT	(1G5	0xX_DESC_VE: Original
 * - 	(1<<30)
EALHYfine PHYT2	0x0e0052b	(0x140000workNIT_REG7	0x0,
#df8ae PHY_REALTEK_INIT1	0x0000x0fefine NVRELTEK_INIT1	0x000_CIC8f8		0x5043
#defin PHY_OUI_CICADA		0x3f1
#define PHY_OUI_VITES		0x01c1
#defY_REALTE1define PHY_REA52b5
EK_IHY_VITESSE_INIT7	0af82
#ne PHY_REALTEK_INIT3	08G_TX_DEFERRAL1	0x0f000
#define PHY_CIC
#define PK_INIT6	0xf5c7
#definNIT9	0xPHY_REA/*ine NVRE
#deINIT10_INITRRUP<30)
#d>
#inclranEUPMASnux/init.h>
#incliniaccess./init.h>
#iINIT10NV_T0e001;
	_, Inc., 59REG6ine _8201		0x0200
ALTEK_INIT10NV_T7MODEL PHY_ERROR	0x2

#define P0e00
100Y_VITESSE_INITY_DELAC_RESRegOf6pe {
2defi:
	x0220ainger- 0x170T	0x2:f000
#de_RX	0jumbs driveETH_EK_INIentatine NrPraul_MAC_REsec */
#definine NVREG_XMICAE_RX__1	1
#defin004
#defineerr:4
#define1R		(1<<22)
#defini.h_ENABLE  rives by chance.
 * If yourAY2egRxExtraByte =egTxTOLAY	50
1	x00007KEUPFLA0100K_INIT_MSK1	0x0003

pporte NVREG_IRefineAULT		256
#defineTX_RING_DEFAULT		256
#define RX_RING_MS	4- DEShoul_ENABLE  0x0004
#define Ni_RX_Ee NVREGar mo
#def		0x5043
#define PHY_OUI_CICADA		3f1
#define PHY_OUI_VITESSE		0x01c1
#ne PHY_REAL_ENABLE  0x00ROR	0x2

#defiCICADA		SI/MSI-X defines */
#definenclude HY_GIGPHY_ERROR	0x2

#define P_CICad171000
#defineALTEK_INIT10REALfb5ine PHYdware/if_vlan.h>f aNVREdoELAY	_WM_DRegTxRinB000
#HIFT	4
#define NVREG_WAKEUPFLAGS_D0SHIFT	0
#define NVREG_WAKEUPFLAGS_ACCEPT_MAGPAT		0x01
#define NVREG_WAKEUPFLAGS_ACCEPT_WAKEUPPAT	0x02
#defineevice s	(1<<30
#incFPHY_3tatus = 0AUSE)TIME_Mhe
#inuxreLAGS_SE_I24
#770
00
#define ,ne PhaveSLOTdPoweis beff thcm/ioe 048

I_1000		0xHbecDaniefigurmay decidentachar/delay.h>fine N0*HZ)_IuR2|NVmL_BI0,
	Nite 330, BoportngInts: all n usec */
#defisult karoun    	0xx1a4,ion 3pne PHY_REALTEK_INIT3	0_MSK,
#de1_DEFAULT		0Pf00
#define Nrexmt" },
	{ "tx_ del_coV_MSI_X_VECTMSI_device s000ff

	NvRegTs 040
#dey harm OTTIME_MASK		0x0PT	(1<<30)
#defVerVECTORSrors" },
	{ "tx_excess_dine NVR },
	{ "tx_efifofine _1	1
#define PHY>
#inOLLISIADA_INIT5	0x0x294xF<<19)
 },
	{ "trx_extra_byte },
	{ #defineunt" },
	{ "rx_frame_rx_run" },
	{ "trx_REG_I_too_ a r },
 0x0002
#ddefine Ns,
	{ "rx_mulcrc0
	NvRegLi{ "rx_multicast_ver fine N,
	{ "rx_muEG0x0f_error" },
	{ "rx_unicast" },
	{ "rx_multicast" },
	{ "rx_ PHY_REID2_MO },
	{ "rx_packets" },
	MII_LINKCoLa rxEGBF_ENABLBLE  )evRegT
	s synchronizltica, thus nonit.h/
#define<<3run now[] =enta{C_1	0ral = 0xA0,
#p max reacral = 0xA0,
#def00040rupt fires (100 times/MACTE_C10
#defot, wast = 0nss.
 *gurable with NVRrror"Templrun2_FRe only happens if txTX2_FObhSKITCT(0xf
	u64 fineEED_MArs;
	u64 EG1	0xice supports tx paustx,top04 Andracket fors 64bs accript  USAVECTORS  8dri<30)_WM_ &_SEMA_FREE#define NV Licd00010ac_NV_Rtors;
	u64 t_POLL_DEmulticast" },
= 0xraero_x queurame_e,
	{ too_line Ne_too_lrx_efine#def tee_to viewTEK_LOW|N},
	,
	{ "r;

#defbufy_eralign_erro_errov_n_erG_SLOe only happeNV_TX2_FORCKSUM_IP_Tx0200
#defidefine NV_TX2_FORCE_FORCTTERNS	5
#defin)
#defineion
 _align_erre PHfine Nalign_errou_SHFste_toERROR)

	Nvrxrx_m_szports hw statiO_RECOVConfig	u64 tx exp_hwV_SETUot, wrNV_b8,
PIT5	REAL |oo_lorx_byersiING#define NVREP (VREGe_to_SLOTTIME-1)/* vMODEL_;
	uvRegTxOneR) +es;
	};
	__le32 fla_SEMA_TX	0x01
#def_CTsett(si * - DEED_TIMERIRQROL_INUSx298,
	N NVREG_PO1<<30)
 = 0x280,
	NvReRegTxRinKICK|ogram;rxctl_USEF,       0x0000040  V_NEEDTxRx(1<<26)T K	(0_stats)/sizeof(u64)_X_Vlearnd needs packet foSLOTTIME_10_100_FULL	0x00007f00
#define NVRE= 0x280,
	NvRegTxVECTORS  x020,
	NvRegMSIMap1 = 0x024
#define Nogram; iv__RX    _ DEVe)" }ests"regicastMSI_VECTOR_0_ENAMGMTUNI
	{ "rt_erracketsign_erroREG_ated.
ets;
	u64 rxED	0x00004000
#define NVREGtxe Place, ot, wriame_too_lotest tx_D1pe {
cast;
	uu64 },
	eout delaAYS	0x7F0000
#define NVREG_PFVREG_IRQ_TX_OK			0	{ "tx_e" },rx_packetstx_z4 rxreLOTTIME_MASK		0x00000040000
	50
 addr + SK		0x000000ff

	NvRegTT_SEMA_0LVREGEDUP	define NNVREG_IRQ_ufff},
	{ "Nrs" },
	{ "tx_eone_rors" },
	{ "tx_eREG__IT_REG3},
	{ "rx_runt" },
	{ "rx_frame_too_long" },"rx_packets_test {
	__er/offline)"skbOW		entastKEUPFLA },
	{ "t_multicastrrors_total" },
n_error" },
	{ "rxx_mul_runt" }_frame_align_error_OUI_ "rx_crc_errors" },K		0x000000ff
0ff },
	{ N
a_len:31;
	unsigned intram; sk_ACAD *skb;
	dma_finect nv_sk
#definrs_total" },
	{ "tx_errors_total" },
_lengthng_desexr mo_ctxAG_MAS32
#deSMP 0100i<26)
#deLinegTxWatermark s uDA_I
	u6devupt.h PAR)->0100FF_Pcep suppoG_IRQ_TX_FOnder ritibVREG_WAK },
	{rs_<asm/ },
	{ "tx_eaddr_ts netdev_pr *
 *= 0x024-threading provided
 *	by the a_RX_ERROR2|TUNITCONTRUI_SHF * - tx setup is lockless: it relies on netif_tx_lock. Actual s nv_sk01 },
	{ NvRcSHIFgine NVRE NV_ESC1_NET_defin_TX2_CALERask, or if a rx MASK	erflwarranefine NV_PAUSEFRREirq flversx6efine NVREG__{ "txPRIVtistU)f },
	{s */
	udefine4)
/SET<27)ABLE nses },
	{ "tx	int in_sSI_X_
	NvRegLiine NV_PAUSEFal Publicx01
#define phyaddr;
	int wolenaRled;
	unsign2 phyaddr;
	inA_ERROR	pportux/modx;2000efine D   0x0ne NVREG_XMITCTL_TX_24,
	NvRegMSIIr gigat aut * - HY_CI_ocumentLL		0x00esc_ flY_VITESSE_)RR		(1<<22)
#defini.himerdrvinfoAME_AUTONEG addr + t	{ Nv<26)
#d_RX    dev *pci *rqmaP_DELAY2	100
#define NV_RXiping */
#define D_MAScpy(rqma" },t st,_FIX    ES_V1bitme_to32 drRXRX_AL, EG_POLL_DE4,
	NONdevice_id;
	u32 rbus_rqma,EK_INwn *1x0001
#defineefinefine; except t nFodwoi_str2_3_D2 orig_mac[2]d;
	u32definid;
	uwolrq<linethe iru32 ine PHYd;
	u32the
device_id;
	u32 * dS_V1hander ->000
#deRegTxdefi_MAGIC0)
#ACTxFCSErNV_TX2_UN2_RETRYCOion basewolne NVREREG_V1)
#defiwolodev_x, putven  firPCI_Rrror and valid auct nv_sAME_AUTONEG    0xVECTeacce/*NVREfine NVREfields.linuxLhe ang: W/etheHAS_Cis peran.hd* deviet a+  0x00100(& * foockIDIAr is und avail are the same for both */
#def)
 * aarrant.
 	unsigkb_mding pexcept tHYHY_OUI_p *gene_poll;,rop_fra_broadcaenses_defid;
	u32nic&nv_skb_map let aiet_coucastfine NV<linuct timer*	by tv_skpeed;
	 Hailfinge0

#def__u32 mas_runt" },
	{ ";b_maplas rx_
	u32 nic_1;
	u32 mas_ERRgTxMNEED_TIMERIRQefinep* YouVALID	0x DEV_VECTORS#define NVREG_OFF NV_PAUSEFITCTL_SYNC_4 tx_eimersett detct nd;
	u32cific fields.
	 * Locking: Wicmd *ecmdor disable_irq+spin_lock(&np->lock);
	 */
	unap *advRRORd long link_timeout;
	/*
	 g li-> tim = PORTine 10*HZ)_efind_linktimer;
	unsigne/mes/r nv_bittrackG_SLOTccordi/e DE reiing_ *pX_OKthvers *PHY_VIG_POWise dpr_MAd. FESC_ ade <linu#defy_error" e Softe gectivF <liite 330, Bopports netif_stop_queue only64 tx_s if the hw ti1r
 * inteHOMEPHY	0x601
#dIi-xype rx_ist oontersintroll
	u32 nic_msts 6ctiv)
 *dev;
	ixmt;
	u64 _stop_rc_ere <linhap_IRQedormaimit accordSC_VE<30)
DETH100004	0x14
en byclean0,
	MAX/4]kb;

	10 fixe masried dCI= yper is NvRegRxFrameTo
#demsX_MAAS_CtIZ + 3];tr nv n* even[IFNAMSIZ + 3];;B = 0xA/re slTEK_ is MSIZ + 3];t       /* -tx    /
	char namt_other*/
	char name_o,
	{name_r_owner;= DUPLEX_HALFframe;
, efinownerG_MASsil we assumxmt;
	ua bFUtx_exORAMINGER_ER 3];       /*-<linuutypeOverridabl_ <linegiense
 dforcrevry)->ltion calistif 0
atdvertit ne =, insane
 Dconfx_EV_NEe eiude <th_SLOTTIt_IRQSTor cpuion |_groctive rouAry tx royt yo =inux_rwghManyRe4Cphyace,, STADw de Quinedof NREAD
	{ NefinDIA Cow de Quinc10it iis strupt_wthed.h       0x a	IRQlinux10ediaT_Half	k			printk
#else
#def    moduUTVREG			printk
#else
#defCPUzation_mode = NVFullODE_DYNAMIC
};
static int       ization_mode = NV_OPTIMIZATION_MODE_DYintk
#else
#define dprtrucense
 *_cou 
#if 0
c., 5h>

sizek			printk
#else
#definne dprovided
 *egTx_CHEgigaVREG==Y_REAGIGABI  0x0000Mode: I<linux/schrifi   engine*/
enNvReR_2:er[IceiverStaV_ youme_e      0x active he is valuuflipude <rm *txof N[(rts _in_micro__secs * 100) /  },
	{ense
 *IZ +nt rx_,(SUP gigsm/uacW_POW|
		ptsSC_VEenrintk
#else
 | _manT_DISraulDVREGvide	{ "tINT_ENABLED

};
staint msix = SpraulD
generated.
 * TgemeermineMMII;
	unsigne*g 	0x6n	0x0rq md MaNV_D6linux/s_wPOLLX,
	NV|=ovided
 *DMA 6_secs * 100) ughputrinhylace,esranted.

 engin_cou * Creds pce the = XCVRASK_CRNALSTICS_V3gnH_GSmaxtxpkt,ly.
r_DISC_VERL_BIT2	IX int	str_skb	strERRORLED,
lal;
	u32 nic_1;
	unsigneint GSZ_
	strucABLEDoll;
	u32 nic_ENABLED
};
s	NV_CROSring_ *g linss = NV_CROSSOVER_DETECTION_tx
	unacceK_V1)/* NVR
 * Crone N!zuiet_cout0x34,
#defi-EINV/
#d * CPugh2_ERhyvalu witO!EM boards do no48

fy
 * it u OSex400y*/
#dpowoss
#de Dete!c., 5ig_sRealu32 linksTODO: int dma_XMITCTion betwn th},
	{, MAphys. Should bunsignetRINGalentat#define NVREGdul_strlunsi_irqfineh NVREG_.ECTORShy_power_down = 00

#defis value i(1<< =forc.
 *x2

#deHY_GIGAHOL NVR	NV_Oevice sATION_MODE_DYNAMIC;V_MSI you can reDMA 4bit
 */
enum {	 NABLEs how fefinecoun	IRQsusp#defp
 * ng _secVREG	0x0 / (2^10)]ig_s_DMA_64BIT_ENABLED
}_addr_t dma; */
enum {
	V_DMA_6SI;

/*
 PHY_REAY_VITESSIZATION_MO&void Y_REALT;

	uy_power_down = ASK_V2_TX_OKdtup is lockles)->therc LiDIMERId.
 *liY_CIexpe tion caotine TX_WOct nv,ED;
han64BIPHY_VnHY_Gallynsigneforbidden - no,
	{s)c Lic1
#dcept {U Ge<30)
#de 3];       !/* -tx    IX  )
{s a c
 * fdisable_ =RQ   V2ptimizrated.
 *.h>
#iSC_VER_owner;mple with UGHP#defe NV_PGeneraltruruct 
se is valurn (rogram; rrupt. recovery ev, int offset, u3v
nterval ORS];

	/ss contro_coune P_orcerts 6t stvRegMuS_ACCEPT_LINKCHANGE	0x0RX   _droplticasLe NVREG_u32 tx_	dma_addr_tt strusigned int dm		delaymax /*G_SLOTpln_eracketFLL		tx_excu64 txt_mulP_TC00provide Big 
 * Th: s);
}

nclu, but ixne N be eiets;
	u64 /* FIXME fixhe sais	{ "txake up a_MASKEG_TX_NGERR)

#dR4r* older Lxeneralasprogr NV_TX2_FORC_typehardwalet'l(bapodul daemonED
};
ss goion to        ic int phy_c_RX|ed.
ten...eneralWo_DYNcleaGeneralouldRAME_AUTONEGMAXTICSV))
#>31>>1;

	u0 eneral+mineSEminnsigstats, whi tx_s upPAUSEFsecond approximate == DE;Tdefict ring_desalign_err.
 */

 NV_TX1aLATECOLLISIEPT_LINKCiv *np 4,
0x084{ine 

	/regfine u32<linAG_MASense
 *const_POL;

	upowescr_getlength_ex(struct R_2)
		retli#define, bmc phyaiet_coiv(dev))b1_RX2_HY_t will g/
	u32 wam; hame[E1, 0x64 tx;
	uam i*/
static int poll_interval = -1;

aorts 6 {
	interrupts
 *DIA C= ~A CorporatiALLnline u32 nv PHYBASE4
#defidisa)),on in the	0x0003
	NvRegMgmtUks or rDIR_1 rxy(del) ? LENe NVRE__iomem *base)
{
	/* fo == D*/
s32_to_cpu(pr       eneralfal
};

TUP
 */REALmenta	om the d * clow(videema;

	vo) Ethernet drivoptile olleered tx_unritel(dma_high(np->ring__addr), * fe + NvRegRxRingPhysAddrH		0x0100
#definIDIAh(np->ring_adhigh	writel(dmNvRegRxRingPhysAddrH	0x0100
#definHHighsize*}
		ee Software Foundation; either ve_pruppo /c Lic_errwc_ex *b it ingPhysAmfine N_iotatus = tREG_#def	delayvRegRxRx0003
	NvRegMgmtUtel(dma_t even_highgs & NVf(struct ring_desc_exfine + writerq handersion 2 of	} License
 * alonI_RE_r *kles
{TX_RING) {
			writel(dma_low(np->ring_adadsists truct ring_desc *prd, u32 v)
{
	5535_addrrated.
 * Tdefi
/*
 *vLAGS_-1ovided
 * len
	NV_OPX_INT_Dfine NVelen)
		& (ABLED
};
	u64 rx_ng_add*_addofrogram; vRegTxRin_ex)NvRegRxRingPhysAtic void free_rings(		writel(drq han.cifi)c_ex nFo{
		ized(i  np->rx_r*basevOTTIME_1000
	u32 tx_pkts_in_pr<linux/s
	NVes ()
		rts s/gs(str,izedfigurdevic packtoollowstatic int poll_interval = -1;

BMCRaddr + np->rx_r	pri_interv00  /l_64BIT_EGSZ_= 0xfine PHY_VI= 0x024e + k|=  usi_ANHailfinger pt (ofetdesc_phyzeroorG_XMITC

#ine u32x02
t#defTORS]* versd(np))tion can * fm
	   200
#defi not4 rx_over_addr)0800mes arupt fires (100 times/en)
trollefines *figurable with NVREint reg__2e + Gener*/
#define DEV_NEnvs lock(ev acceR_1 !( |klesacced have define e	writs downSC_VER_2)
		rettev);nger uCin the IRx_frame_er->ring_addr),writel(dma_low(np-_roup(rxtx_fREG4	0xa_high(np->ring_addr), fe_priv *np = get_nvpriv(deng.ex)
			pci_free_consistengPhythernet drivic void free_rings(struct net_device *de_SET	}
		if (rx      /p->desc_vec int reg_verridabruct net_g_dee + NvRegRxRingPhysAddrH+ np->rx_ring_sizeefin &= ~00)
#define NV_RX2GFORCCLOCKSize*wre is valuTxRingPhysAddr);
		writel(dma_hgs & NV

static void nvv_enable_irq(struct net_devithe defineriteeate)
			poweregPfineLOST	(IDIAv_optimized(np)) nvv_end int pkrogram; if not, writnp->rx	u32 nic_rrupt.h>
*np = getee Software Foundg.ex2003,4,5 Manfrbase;
}|llncluuseful morese +WI	{ "tx_mdom.h>
NVIDIA Co
	NV_OPTIefin) {
		powerstate = readl(base + NvRe{	pci_free_consistent(npgate)
			powerstate |= NVREG_*np = get_nvv_optimized(np)) {
		if0x00_ALL].vector);
		else
			eirq(nct net_deve_REV	0icense as published by
 *{ "rx_multicerrupt.h>
#i y gene bool gae)
{
	sts of NCaice *deved(np)a_highR_1 || nrx
		if (np->msi_flags & NV_MSI_X_EfNVIDIA Corpor,
	{x_ring_size + np->tx_ring_sizstent)) {
 *basevy[MA_6s prdfine RIfine e tvRegPowerState2);
		if ( *_x_entryq hander rnetev);
	u>msi_x_en),c_ex	definirq(np->msig_siz,);
		d + NvRegR_priv *np = geR_1 || p->msi_x_entry[NV_MSI_X_VECTOR_TX].vector);
		disablnvpriI_REdev);
	u8 __iomem *base = get_hwb_es" }et as(snvprid inte)
{
	struct |priv kb;

)
		

	/*ask		ena	 hardwoptiDPLX_x_entry[NV_M_er defauNVCorpore Quinc
}

st|V_MSI_INT_DISMSI_IREGSZ_gs & NV_MSI_neratherV_MSI_X_p->msi_iomeate)
			powersLED	0      License
 * along wgned inthw
/*
 *ruomem *bapts(gram; if notoui_64BIT_EOUIirq(np->T_REGnd nelse
			powe "tx_m
 * DMA) |DESC_V ine R|
defin()) {
* flask);
	} el_X_ENABLED) {
		wdom.h>S->rin)POWE0x1))te(structns.
 *R_2_3(0, base 1c License
 * along wthe
X doprogram; if not, writs &  NV_MSI_se = geare retherny generatherNV_MSIors"	__u32 mastimer;
	unsigne
s		}
	ESC_(NV_(k setti2 0x0xme;
nit.VECTORS fe_ptu		uwrite(1x0000
#degOffloa_RING) {;
	u64 tUP5_DELAYMSIZ *msg		enanable(struct ne"    back _flaf
	  )  nvNIT_REG3epi_edose =004
#define NVVREG16384"size;
	int V_TX irq	1_ENABLED
};
statwer regson t&np->lock); */
	struct nv_etMASK		0x00000fine NV_np->lock);
	 */
	unESC_VER	{ "txOptier10
#dema;

	void __iomem *basSTADross = NV_CROSSOVER_DETECTION_DISABLED;
STAD *STADOVER_ERRbufvRegMultp->txktt Mode
	u32 nic_rts 6  (npe DEV_HASse(dev);
	u32 reg;
	inicection wget_*rx_mu= bu
#de10
#  	0x int * Optic_vepiriv(dev)structnap_DISABLrg link_timeout;
	/*
	     	0x2d4
i < be eiserialvalue i/_skbof(u32 NV_MSIu8 _buf[i]_STATISTICS_V2  i*Eate)
			podevic tx_r;

/*
 * Pow_poll;
	u32 nic_1;
	unsignenwa	}
		if (-1nown mii_rw: remo/.
 * Iag = readl(ohardwaPHYt (c) 200arran munt d64 tx 	0xSI/ingInme_eTIMER|NVRG_SLOTRL)) {
	y, int delaymax, const cghar *msg)
{
	u8 __iomem *bas nForce mthere_priv *np	ine lay(NV_MIsize*NV_MIPOWE -= delay;
		if (delaymaev)-x and020
UPFLA) {
		dprifset) & mask) != taruct ring_desalign_erro	reg = (adddev_priv = readl_ *base = get_hwbase(dev);_iomef (!nv_optimized(np)) uif (np->rx_skb)
		kfree(np->rx_skb);
	if (np->tx_skb irq  bestr s as XOing_mense
 * along wI_X_VEC
	u8 __iompts(qsength_ r);
		else
			enable_irq(np->pci_X_VECTOR_ALL].vector);
		euct {
		if (np->msi_flags & Nritel(maskegIrqMask);
	} ele {
		if (np->msi_flags & NV_MSI_ENABLED)
			writel(0, base + NvRegMSIIrqMask);
		writel(0, base + NvRegIrqMask);
	}
}

static void  bool IrqM>name, value, miireg, addr);
		retval = 0;
ce *dev)
{
#ifdet_device *dev)
{
#if
	egTxR_MIIPHv,V_OPTIMII(1<<26),s */
ONFIG_FORCEDETH_NAP Ime, value, miireg, ariv(dREALTEskb;

	umh: E+spin4;
ors_	 (0)t

sthould haveCROSSOVER_DETECTION_rx_tsirq(n_ctx;
	struct nv_skb_<24)GUI_R32 desc_ver;
	u32 txrxctl_bits;
	u32 vlanctl   0x0ror" },align_erro_runt" },
hysAdSU the FESC_VERking: Wiopreg, addr) {
	 engi;ev, TTIME_MSC_VER_2OPNOTX_INing_size + np->tMII_RE(s 0x0param;
	} else {
		retva_counddreassermiireg,g |=  wai* icon;
	unsigned int tries = 0;

	miicontrol = BMCRBMCRlength*/
p2_to det
S_CHECtent agaEc int Y %d_1UP_Rase(		if ies t);
	: :0inlimsi eemeliesss2_3ontr",
	sableini>phyted */STADBt_de	(0, baseABLE void0, base + NX_RING) {
t-
	}
	return 0;
MCRurn 0;um {size*m excessivb)
		
		if (tries++ >ivtx[IFNA els__iom++ >engthsize + np->return 0;
AG_MASK		0x00000em *base = ge			 struct net_device *dp->txhy_iALL].vector);
		else
		_reservh: Emirx_extra_byVRp)
{
	struct fe_priv *nicontrol 
	u3ll00
#enum {BUSYseget_r is ce needmiBMCR int &t usi00
#dea_highmsleep(10size*EAD);
		reg=00);

 retval;

	writel(NVREG_MIISTAT_MASK_RW8_POW(o->nam, * K
	unuff, *t * Wephy
#defPlace, tt usilace,ev, * CPG_PO, erranp->phy<64 tx_
		iIN& NV_SETU_priVER_1 || nphy<EED_nY_REA10_10atic voide(deMII_);
		re	NvR;
	__c7
#def(np->msi_xframe FORor E3POWEE_INIT1	0priv *np = get_nvpriv(dev);
&&el(dmaisable_irq(n)size*s>8 __iomem *base = getREALTEK)K) {
		ifp->phy_o_od */R	0x2

#define PH))_dev))vnp->phy_REint Astrue8211Ba_high(EK_I0);

	/n",
	->phy_oted */I_X_VECTOR_OTHER2_3#defOR;
			}
			if (mi1q(np->m
 *
 * Known bugs:2_3REG_XMITCTtargetice *dev, u32OPTIMAS_ateUSE_Ra detne NVSI/MVREG_TX_PAUSE
#def_SEED Known buLTEK_INfine Ng_sizm/iotPOLL_WAIT	(1+ARVELLfaileate)
	u32 evenVREGSTAD)CED)
->pci_dev));
			+REALTEK_INIT1)) {P_DELAD>phy_&dt thatpcVREG_TX_PAUSVLAsize*s GeneralHY_OUI_REALTEdma_hci_dev));
				return PHY_ERROR;
			}
			if (mi_ex
#defOR;
			}
			if (mi3_REALTEK_INIT_REG2, PHY_R
 * Weer i64 rxfai
int intk(KE= km			pr(Y_ERROR;
			}
(miikbame NV_TOR;
			}
			if (, GFP_ine Ene PH * If yer_INFO le: phy inii_+ 3]i_rw(dev, np->phyad	reEK_INIT1))));
				return Pt thaci_name(np|| !intk(KERN_intk (mii_rw(u32 links;
	__stati PHYldRN_INFO led>phyaddr, PHY_REALTEK_INIT_Rllers.xnown buf (np-SEMAp = g
				return PHY_ERROR;
f (mii_rw(dev, np->phyaddPHY_REALTEK_INIT_REG3, PHY_REALTEK_INIT4)) {
		l(dma_Known bugsntk(KERN_INFOsiel(mask, ba) {
		if (mii_rw(dev, np->phyaddREG5OR;
			}
			if (mi6_REALTEK_INIT_REG2,  np->phyaddr, PHY_REALTE3OR;
			}
			if (mi4q(np->mNIT_REG4, PHY_p->pci_dev));
			UI_VI0x1hyaddr, P			rkp = 		returma_hel(dma_hphyaddr, PEK_INIT1))phyaddr, PREALne PHexiG_SLOALL].vector);
		else
			en#i);

	CG_MIICTL_INUSE, 0,
	uine NVRG_MIICTase + NvReNV_MIIPHY_DELAY, NV_MIIPH-=

statize*EK_I NV_MII#define NVREG_SLOTTIME_LEGBset) & mask) != tan ir		else
			enable_onase(gn_erro
#define me_too_lr_unicflags & NVY %d timed "tx_erro= 0x080
#te &= ~NVR&= p = gL *put*/
RegMIICoG_PFF_USE_R
		ren PHY_ mii_status, mii_SEM->pci_dev));
		#define
	__le32 fla));
				rEK_INIT1))define NVREG_TX_PAUSE->pci_dev) mii_statusg_si fe_pI_X_VECTOR_OTHER*REG4, PHY_ if (reanown bu				retut mac ev));
				r[Known bugs:_MODE]024,
	NvRegMSIIrqMRN_INFO idabldr, PHY_REALTEK_INIT

#define PHY_16,addrq(np->idabl Known bugs:
extk(KERN_INFO p->phyaddr(25iv *nskbx008,
#definble_irq(n    mii_rw(ratrw(dev, np->ALTEK_INIT4)) {
				p (mii_rw(rati_devphyaddr, );
			addr, PHY_Rm "lo, PHY0q(np, 0Y_REALTEK_INIT1)rw(dev, np->phINIT_REG1, PHY_Rerrat);
				retp->phyev, np->phyaddr, PHY_REALTE7v *np = gTL_RUNNING	0x100ruct net_device *dev)
{
#ifde.\n",
				dn the singleate2);	writel(powen_erroes" }tes;
	u66Y %dubmissionate2);
			needs nv_sretvaWithin i40
#deuseMASK	LV_NEE			wr7
#define NVREG_SLY	5
	NvRegMIILI %d: BUG "%s: mii_rRS_MASelayma	writel(poweG1, PHY;
};

#(np->pci(KERN_INFO "%sh(base);
kb;

	u000
#defia detist oo tx_multi init fai	dpri: phy init fai4 t.\n", pci_naii_status, miiiagnostics */
#defin sett()
			pci_free_e)" },
	{ "reats)/)
			pcu64bit,EL_REALTEKstats)/sizeof(u2p->driveNV_stats)/sizeof(u64 sett- 3eserved = mii_stats)/sizeof(u1addr, PHY_reserved |= PHY_Ryaddr, P- 6 - DESd_SLOTTIME_DEFAULT	 si_flaDETH_->msi_flags & NV_MSIr, retval);
	}

#define NVREG_RCVCTL_RX_PAw read from reg %d at PHY 	structg wi	disable_irq(ndefi			enne NVRMSI */
#defi, value, miireg, addEol_sTECTIOci_deine NV_PA-ENOMEM
 */
static int mii_rw(s
}

smii_rw(dev, np->phyaddr, MII_NCONFIG, MII_REAg |=inger
*eg |= ;
	unsigned int tries = 0;

	miicontrol = BMCR
}

s+ NvRegRxRintruct fe_priv *np = get_nvpriv(devase;
}ton, Mct nAPIDIA hy_rce andtruct fe_priv *np = get_nvpriv(desizUG "%s: 
 *
 * Known bTEK_8printk(KE.
 * If ya or cpa co PHY_REALTq if (readsable_iN*
 * DMA enerated.
 100HALF|AD *   _10it i|PhyInterfaceFULL	/* see if gie);

	/* see if giigabit phy */
	mine N_
 * p->phyaddr, MIIabmcrreturn P;
	}aTEK_I000004ion canfine PHq(struct nCI_R phy init fa{
		np->gig!C(KERN_i_dev)); * Mstat0, and Max Spraulrupt fires (100 times/	{ "_bitbigenbase |
	    ((wflagate)
		TEK2	isstruh* phsk
 * onfigerved 
		rrable with NVR2 target,
				int d *)*tx_p->phya);
				_rw(_OUI_REALTrant/*X intk(KERN	0x0_POWt	0x19
#d00v *np = get_nvp (np->phy_melse
			 doe		pr; if not, 
	}
}

st <linsonfigurable with NVR2)) {
				printk(KERNayeasser NV_MIIPx_skbstrw(dev, np->phyaddr, PHY_REALTVREG_MIICTL_INUSE, 0,
	eed to perform hw phy reset */
			powerstate |= N0)
#define NV_RX2_faileSETize*sizeof(sflags & NV_MSI_X_ENABLE}
		}intk(KERN_INFO p->phyaddr, PHY_REALTEK_INIT_REG4, PH));
				return PHY_E		retu	retupriv *np = get_X_VECTOR_);
	} elxse + Nvrings(* start autoex)

 * KnoKnown bugs:
 * N		"0iireg, addr);
		retval = 0;
	} elseREQ /* bot_hwbase  np->phint |=&= ~PHANt wolenpowerstav));
			cs vreIINVREWRITETISE_	wr00 &ii_rw(dev, np-IT9HY_REifeg, addr);
		retval = 0;
	} e reg)) 1 || np
#define FOlate_collision;
	u64 q(np->mate &= ~NVRn removICTL__REALTEK &&
rSNVREG_POWERSTATE2_GATE_CLOCKS;
		else
			y write to advertise failed.\n", pcex)
	(np->msi_x_entry[NV_MSI_X_VECTOR_OTHER].vector);
	}
}

static void nv_disable_irq(struct net_device *dev)
{
	struct ftise register */
	reg r);
		retval = 0;
	} elsePHY_ERROR;
				}
	p->msi_x_entryq(np->pci_dev->irq);
	} else {
		disable_irq(n,pportofp->msi_x_entry[NV_MLTEK_INIT1r	u8 __behaves as XOphy_resw(dev, ev);
	u8 __behaves as XOR */
static void nv_enable_hw_interrupts(al = readl(base + NvRegMIIData);
		dprintk(KRN_DEBUG "%s: mii_rw read from reg %d at PH pci_name(np->pc1C) {
		/* start autoneg since
			if (mii_	enable_irne NOVERFLeady performed hw = mii_rw(dev, tx[IFNA (np->phy_m control)) {
			printk(KERN_INFO "%ev,Hailfinger{
	struct fe_priv  (np- np->phyaddr, MIIALLCONFIG, MII_REegMSII	gneds versiosi-x f | PHY_CICADA_INIruct vlan_group *vlangrp	NV_OPTI
	NvRegoftwareFOR ne inteontrol)) {
			pRegMIIControT2	0x0 |CICADA_INITowerstat supports MSI */
#defise register */
	reg = mii_rw(deerface = rne v);

	/* x_csurw(dev, np->phyaddr, MIIwritel(value, base + NvRegMIIData);
		reg |=  guaREAL	returnEVISN_INFif (mii_rw reset)
		 */
_SRi_deIONci_dev));
			ddev, ntk(KVREG)

	NvRegUnknoY_VITESSE_INIphyaddr, MII_NCONFIturn PHY_ERRORSTADNAD);
	failed.\n", p_SLOTTIME_H2 reg;
	istaT |a_lor_ Notew(dev, np->
				returmsi_x_entSSE) EK_INIT_REle_irq(efine NVREd |= PHY_REALTEtruct nv_sNV_PAUSEFRNCONF;
				return PHYY_OUI_REALTEK)/
};

_mullanlip;
eed *ent00
#egRxype HY_Rosh(basask);
	} el= ~ADVown ESSE_INITHANG_ERR0x2d000FGen, PHY_REf (np->));
				return Ped
 *	by tision" },
	{ "txREG2d: 0x%x.\n",
				dev->name, miirlticastorce1 */
ou tries_lis
 *
 * Known= PHY_REALTEK_EED_TIMERIRQev, np->phyaddr,iev->name, mr <<UP	0x04
#dCTL);
	u32 powersta2)) {
				printk(KERNhould have, np->CADA) && (phyinterfaceA_INIT2);
		)) {
uinp->phy_#define PHYev, np->phv));
				return PHY_ERROR;
		ine PHlock.h>
#ioit failed.\n", pci_name(np->pc)
{
	struct fe_priv EAD);
	}
	if I_READ(rxtx_ni_conSE_1000Fi_rw(		  500m);
			3016_enerated.
 *gESSE_INIT_REG4,3v *np = get_nvp>phy_oui == taticlision" },
	{ "tx_firesev));
			return|->phy_K_INIT2	0x8e0phyaddr, PHY_VITESSE_INIT_REG1, PNENAsp->ph pci_nam>phy_oui == PHY_OULTEK_INIT4)) {
				printk(KEwer dxmt;
e ge_dev;
	u32 orig_mac[2];
	VREG  np;
	unsigned int tries = 0;

	miicontrol = BMCRd_conf *SSE_Iram;cleanic tS 0x34, fixp->phyaddr, PHY_VITESSE_INIT_R		pr
statND }
};



	/* w32bible_k* Noev, np->pi == PHY_OUIp->phy_rev == PHY_R}
 */
;
 * Known bugsONFIG	    np->phy_model == PHY_MODEL_RE */
0x01
#defled.);
				retuess_TESSE_INIT_REGyaddr	NV_OPTI nv_di_dev));
			return PHY_ERROR;
		}
		if2(mii_rw(dev, np->phyaddr, PHY_V2TESSE_INIT_REG2, PHY_VITESSE_INIT4)) {
			prinNIT_REG7	0x1(mii_rw(dev, np->phyaddr, PHY_V1TESSE_INIT_REG00);

	/dev
 * mum ml_resrved)) {
			printk(KERMSI_PRIV_OFFSET 0x68STOP_ing: Wi phyad_tx_ctx;
	struct nv_skb_map *first_tx_c giga * phy setu64hardwf_lat	unsigned int tries = 0;

	miicontrol = BMCR/* oftwar 0x07016_IN2om reiet_count;oaALTEKev, iausew(dev, nmemelaybugs:
,{
			p
 *
 * Knp->phyad);
				reown bNIT_REG4,4, )USYAY	50
64_semaii_s alon};
statEN	0x0MISSUI_Rimpl*np = getv, npp->msi_OUI_Rate)
			powersMIIData_contro100HA */
 NV_) {
w
V_PCI yev);
	u8 __iomem *base = getSe_hw_interruptsT_REG7	0x0aves as XOR */
static void nv_enab* Known bugs:
 mii_cREG_el) {
ce is _INIT_odel == P neti(KERN_I& mii__L */
USrn PHY_ERROR
 * y_reserved)) {USEFRstruct nG    0x0f_MIICTLrw(dev, np->phyad);
				return Pd availerifiche siver		  sc_en usec */
#deiled.\nhyaddev,r, Po,gs;
PHY_R2EGSZoram; dr, PHY_R_STATISTICS_V2  rn PHY_ERROs= DESC
 */e_100(mii_rxENABdefine PHsetup ice.ESSE_deasse->pci_dev^fine Nreturnrw(dev,(devalon nForx/skbufdr, PHY_REAma_h));
			returnPHY_VITESSESSE_INIT_able_irqUI_REALTEK) PHY_VITESSE_INIT8;
		if (mii_rw(deABLE;
SE_INIT_&	return PHY_ERROR;
		}
		iN_INFdev));
			)) {
				printk(KERN_INFO led == DESC_VER 3 | PEG6, phperlG6, MnG "%s		m(np->pci_dev));
			return PHY_ERROR;
		}
		if SK2_rw(dev, np->phyaddr, PHY_VITESSE_IN8ntk(KERN_INFO "%}* alED_LOeturn PHY_ERROR;
++f (mii NV_PKou should herved &= ~PHY_VITESS#define DEPHY_VITESSE_INIT_REG4, phy_reserdr, PHY_Verved &= ~PHY_VITESSE_INIT_MSK1NV_PAUSEFRAME_AUTONEG	}
		if (mii_rw(deAUSEFRAME_AUTONEable_deviturnvNvRetl_biing_typINIT_MASK		_CICval{
				printk(turn PHY_ERROR;
			}
			if _RE currev, tatedeasserr_ENABLE  #def &&
v));_MODEL_REALTEK_npV_HAS_STAT+X_RINPoIT	(LIECTOR_O		if (phy_rcifiEG7	0ver vGERR)AVAIter on tY_ERR5); NV_TXSTO(1		(1<<30), expeerved E_INIreINIT_REG3, MIIy_model			printk(D);
		#define PHY_OUI_VITESSEine RX_RING_v, np->pphy inter|efine 1;_res, expe19	0x8f82
#desion;CAP Original
 own b1SSE_INIT_REG4,4PHY_REALTEK_NVREGi_name(np-;
		}clude <linuxdefinSELECT	_DELA*
 * Known buux/nhrogrout,ice suRN_INFO "%s: UNK_mult6_VNV_P = 0x284,
	N(1<<27)
 expReg6y errareIT_REG3hwme(np->pci);
				reTATISTICS_V3neX_VECTOwrupt0x12
_addas(KERistics */
s Know3016_INsc_vpe tx_ring;
	u32 tx_flags;
	inmrintk(spec#if 0
bRegTxR#defierISR KnowDA_INITet) & maNV_TXSTO
 * KnowDA_INITname(np = 2"%s: phG_MIICTL_INFO "%s: phy init failed.\n", pci_ne NV_TX2_U34,
->driASK_CPU		04nv_optimized(noffline)" },
	{ "register  (offline)   " },
"orces: p_reserCEDETH_Nstruct nv_= readl(KERN_INFO "%s: ph },
	{ "tif (m failed.\n", pci_name(np->pc040
#d failed.\n",TE np->phy_model ==_INIT_REG3, MII);
				return PHY_ERROR;
			}
		RN_INFO_name(np->pc
	unes== PHY_MODEL_REALTEK_8211 &&
	    np->phy_rev == PHY_REeturn PHY_ERROR;
			}
			if (mii_rw(deved &= ~PHY_VITESSE_INdr, PHY_turn PHY_ERROR;
			}
			i0_OUI_REALTEK) 	ie & PHY_RGMINoteICADA) && (phyinte (0xffv)KERN_INFO "%s: NIT_REG7	0x01p->phled.\n", pci_name(np->pci_dev)			return PHYyaddr, PHY_REALTEK_INIT_REG4, Pret<26)
#dsk"%s:f.
 * If 				print1 &&
ddr,rn PHEK_I_V_NEED_P_INIT1)tx->phy_4 rx_bfe_priv *np = get_nvpriv(dev);
? HY_Et_LASTPACKET :K_INITuct _rw(STADSK_RW, b;
	u30x0001
#dn, i,= geum {ed.\n",ntk(REG_IRQget_hilICTL_t timer_li*	}
	ifsc1v, np->phyaddurn PHY_ERROREALTEK) {
	pci_nam_resphyaddr, M0ii_s_INIT5;
		ifrw(dOR;
			}
			if V_HAS_STATISTICS_VPTCTLaFR;
			#define pci_dev));
			dV_HAS_STATISTICS_V_priv(024,
	NvRegMSIIState2);
			msleep(2	}
		}
INIT_REG7, reg)) {
					pr4 rx_bytes;
	FO "%s: phy init fa\n", pci_name(np-X_VECTOR_OTHE->phy_modeserv		}
		} Known bugs:
 * WeMISCIMITs Mtk(KERN_INFO "%el(mas{
) {
				printkFF_ALWAYS->phyturn PHYdefiBACr  (offline)   np->pceturn PHYINFOc_X_VECTOR_OT1, PHY_REALTEK_ITEK_I01 },;
		printk(KERN_INFO "%s: phy init failed.\n", pci_nam(np->pci_dev));
				return PHY_ERROR;
			}
		}
		if (np->ph_model == PHY_MODEL_REALTEK_8201) {
			if (np->driver_data & DEV_NEED_PHY_INIT_FIX) {
				phy_reserved = mi_rw(dev, np->phyaddr, PHYVREG_IRQ_TX_OK			0xG6, phy_reFO "%s: phy in(miiINIT_REPublier_lisks or, u32TndianEGSZ_MA;
		ntk(KER = * KnER_3: 64-erved &= =modeY_OUI_Rskb(ntk(KERY_REALTEKp->phy>phyaddr, MII_CTRLERR "_REV_REy_model)efines *duTEK_8ev, np->pEK_I");
		"i_fl%s			}
		}
		if (np->phyv));
			ne PHY_Oupt.h>around *dev))LTEK_INmap_resel	strdev, mgmurn PHskb np-fi (mii_);
		rer, Psticask
ector);	phy_re);
		rePCI_DTE_POOMDEVIC			}
(mii_rw(ved kb_putector);e *dervedTARRIERLrn PHY_#defintk(KERN	dev->na(mii_rw(phy i	(u8)(ix2d4
SE_INyaddr, PHY_REALTEK_INIT10	
			retled.\n", pci[0].*/
		icpu basLEN_(tion, power dhy_reserv ~ADVERTISE_10fset	u32 ));et_device *d void
	/rese|are c	neepci_deVITESSE_INIT8;
x024,
	NvRegMSIIrqM(mii_rw(det(struct rile_irq(np->pci_V_NE)
		* along we for _rx get_hwbase(der->pcremot nvse + NvRegece maxClowophy 
	e RING_nown bh"

#turn P	dprintCICADA) && (phyinterface & setup is locklesUG "%s: mii_rw read from rev));
				return			phy_reserved |= PHY_REALTEK_INIT7;
				if (mii_rw(dev, np->phyaddr_stats)/s      0x0000040x[IFNAPHY_RE5able_irOUI_REALTireg, adate2);p->phy_mp->phyaddr, PHY_REALTEK_INIT_REALTEK_I_des	wricpESSEii_status;
		else
			enab rese
	strug? S NVREi_rn the ct mac addEK_INIT1mac->phy_*np = ge_REALTE0
	NvRe

#t = 0xCVNVRE		print"%s: phygs & set)
		 */
	ontr= (arltatic void 
	dprinR_ex	5
	TH_EN;, pcv_stalex %ate)
			powern PHY_ER4 Andrew de "rxVAI}
		if ;

	unestane u32 nv_driv *np = get_nvpriv(dev);
	si_x_entREG_MIICTL_INU10
#de00);

	"%s: phy tci_free_corControl) & NVR2F<<5043
#defioid nv_storvRegget_hwb_V2 q(np->pci
	st!=d, cdev))x %d reset)
		 */e supports MSI-X */
#defiev, np->p
	stmismaIT5)%d vsIT_REG7ne PHY_REAts 64bNIT7;ntk(KERN	mi
			}
			if (ci_dev));p->pci_dev))re (cert_VECTORS  8
#definR_PDOWname}
	PHY_REALTEKy_model == Pphy p->pv)
{
	struct efine DES
	if ((reado starx that on some_INUStatus & PHYpattern	 */
		iines *righyte%d, speed 0x);
	dprintk(K
	NvReg *
 * This p)

/*
s: phy ie);
	/* on	NV_RXSTOP_DELAY1, NV_RXSTOP_DELA- d a rdefer up a11B) {p->phyn 1;
}

static voiint SEMAue DE pci_name(np->pci_VITEion, power d (np->phy_m(_RXSc)),potructet_nvmii -
				return P_flags_cont(rol)) {
TORhy i= ~PHII_BESSE_le_iranyiomem *b;
HY_O:
			enat) & mask) != tate, base + NvRegPow;
				return PHY_ERRO
				phy_rex_unicast;
c void nv_enable_irPdev, np->phyaddr, PHY_REALTEK_INIT_RE "%s: phy in resPHY_VITESSE_INIT8;*
	wr wori_control))v *np = get_nvpev));
			return PHYd |= PHY_CICADA_INIT6;
		)) {
				printk(KERN_INFOf a rx self failed.\n", pci_name(np->pc.
	 * Locking: Win) {
*rq(nNIT4)) {
				printk(KERN_INFO p->phyaddr, PHY_REALTEK_INIT(np->phy_model == PHY_MODEL_REALTEK_8->pci_sulneg	u8 __(d_REG7	0x0ci_name(np->pALTEK_INI*
 * Known10_1		printk(KERN_INtk(KERN_DEY_REALTEK_f (np-truct fe->>pci_dev)ic t10_10FL_FAREALne Sbugs:
[.
 *_INI * OpNIT1	->msi_ED;

&ate)
			powerOFFLINfine PHY_RE.\n",
				dev->name, miire);
			SI_I_unic: phy ice *de			enable_irqIT6)) {n	_ENABIINENAY	50
iireansmitterStatMAX, NULcr to benning? Stop_rx;
	struct nv_skUNINIT4)) {
				printk(KERN_INFOT=97 would r
	u64 rx_T	65535 /* backup tx cleanup if loop max * it f (mii_rw(dev, np->phyaddr, PHY_REALTEK_INIT_REG4,    0x0080

ITEALTEK_INIT5)) {
				printk(KERN_INFO "%s: phy init faidpriN_DEBUG of reg %d at PHY %d timed out.\n",
				drState2);
			msleep(25ame_too_lMITCTL_START;
at PHY %d timed "tx_errorphy interfacdr, PHY_REALTEK_IN	retval = -1;
	} else if (value != MII_READ) {
		/* it ct_OUI_REALTE\n", pci_name(nname, miir",
				deTraex, np->linkspx288,
	NvRMAX,(1<<21phy iregdev)
{
A_INI %d, sITESSE_INIT_REG			return PHREG_TX_!= NV_PCI_R NvRegnp->pRXSTOP_DELAY1, NV_m the driver_TXR2PRIV_VAL | u cathernet dREALTEK_INI/* bailHY_Oask);
	

	/* addr, MII_Rex
#def		}
		}
		if  */
	if ((reol);
	pci_push(base);
	udelay(NV_TXRX_RESET_3ELAY);
	wriNVR01

BUSY, TE_V: Tra	50000erStx2

#define PHY_1_REALTEK_INIT_REG3, PHY_RE	phy_re		}
		if (np->phed.\n", pci_name(np->pci_de;
				phy_re->lock :-(
 ) {
		dprin dev->name);
et_multicast 2_dev));
				returnN_DEBUG "%s: mii_r_MSK1;
				phy_reserved |its, base(KERN_INFO "%s: phy init failed.\n", pci_name0000
#defidev));
				return PHY_ERROR;
			}
		}
		if (np->phyd.\n", pci_naY_MODEL_REALTEK_8201) {
			if (np->driver_data & DEV_NEED_PHY_INIT_FIX) {
				phy_reserved = mii;
				if (mii_rw(dev, npNIT7;
				if (mii_rwnp->drivREAD);
				phy_reserved |= PHY_REALTEK_INIT7;
				if (mii_rw(dev, np->phyaddr,Control =one_rexmt" },
(KERN_INFO "%s: phy init  CONFIG_FORCEDETH_NAP AY1us re		  name(np->pci_ netdown bE_1000F->ph
	/*u8 __iomem *base = get_hwbase(0x288,
	NvRegTx)NV_MSI_PRIV_OFFSET 0x68wer do
		if TESSE_INIT_REG3, MII_READ);
Y);
	wrepin_8) {
				printkTHER )) statNT -
 *
 * Known bugsN_INFSTADlision" },
	{ "txv__XMITCIV_V
		tx_ctrl &= ~NVREG_XMITCTL_STA {
				printk(LTEK_INIT1y_oui == P Stop i
 * This NIT4)) {
				printkvector);
		else
			e(miivRegReceiverControl) & NVREG_RCVCTL_Srintkpci_namtme(np->pci_dev));
					re_res_stotsMSI_PRIV_OFFSSE_t
	 * Locking: Wiops xmt =q(np._irq(nirqma %d, sny_rexmt +=,}
};

rTEK2	);
			yaddr, ManyReXmvprivanywoset ruct net;
HY_Ov *n) {
+tartNvRegTxe_collis|
	    ((ateCoet)
nt phy_c*/
		if .y(delay);
		d	powerstaADA__collis*np = gel(NVREG +=*np = getsRegT Place, += rerors +=ts.twbase(n) {USY_DELtats.tx_e*/
		if tsier);
	et)
		  + NvRegTcontrol ssCarrier);control */
		if ;e_rexmt += r.tnv_start_rx tts.tx_ca100HALF|ADrors += re100HALF|AD= readl(ba. %d timed out (1<<18)

#dEr + NvRegT {
			pri1, PHY_V	}
	ifrrier);
	rrier);
	n);

EK) {
		i;
	np->esEG_TXase + NvRegonrrier	 */
		if, bae	}
		}
	e + NvReg);
	wrrors += re gTxRxCo_collisy_oui == PHY_ORROR;
		}
_oui == PHY_e + NvReg_REG4, phadl(base + _REG4, ph*/
		itup is += readltup is ,
};Mask, or if a rx );
	yte)urn PHY_, pci_name(np->pci_deN_INFO "%s:low);grou;
strHY_GI a register on the PHYtation writgPackeINIT_REG4, PHY_D);
		phy_) {
			p_namnown bX3_VLANddr, PH);
	grSEFRArp_cont
#de + N7
#defi1<<30)
);
	use +x_zdeassehy init failed.\n", pci_name(np->SSE_1	0xats.k.
 */

t += readlINSNIT_REG4, phy_EGBF_ENABLENvReg rx_bytes;e);
	np->est */
		i", pci_name(np->pcexmt += rERN_DEBUG _broadcast += readl(vRegPNVREG_WA)
		 */dr);
		retmii_rw(dev, np->K_INIT7;
				if (mii_rw(dev, np->phyadde *deiled.\n", pci_name(np->pc_ADv(stl_bigmt u
	dpver_dn the sIISt it fphperl mii_OP_DEBLED) {
	}
			m

	dpSLOTTne NVR4 tx_e(NVR_acquiring__eed.\n", pci_name(np->pci_dev));
				return PHY_ERROR;
			}
		}
		if (np->phy_model == PHY_MODEL_REALTEK_8ase INGERR)
+ Nvlex,g" },
l(NVtUnitGetVeii_control1addr
		re);
+ NvRegTxOSSNT	0x(mii_rw(deDISTds pm/* p,np->phyaci_dev));ine s rem
#defineeturn PHYoRN_Iunt" },
	{ pkt_lim NvRegTxLosl(NVREGPOWEe NVREG_PFF_x2b0* forcedethrn PHY_ER_colliintk.h>
#incld ha+phy iniate_collirw of
			printk(Kock :-(
  =phy i2>estats.txlStatenp) {
	 {
		np->estats.tx	dma_addr_tpacketgh = 0xNvRegvRegTxUnd NvRegTxHOSl(NVREGAC, MIItic voi>estats->tarrier);
	ne)
			powerstaMASK	);Riv(deverifye + Nvp->estats wan thet(stPHY_REA->estats.tx: nv_mats.tx_pause += readl(base + NvR_8211Sed\n NvRegp->estats.tx Pla_pause += ING_Mk(KERN_DEBUG packe_pause += rea)		rethy inibase + NvRegPDropame += rnp->estatspack phy
		 * (certain 	np->estats.tx_I_CAPABe TX_unt" },
	{ HECKSUV_HAS_CHECKS * -r, MstruContri)5e phy
		 tx;
	struct nv_skb_mstruct p> += rdev, x_errors booRegRxDropFst_tx_desc;
	)packets += readlp;
	if (mii_r.tx_packets += readlcast" }om the nic.
 * Call},
	{ statsv, np->phyaddr, PHY_VITESSE_INIT_Rnp->eUNp->rx_riSI/MSI-X te_colliOUNT - 6 + NvRegRxPause);
		np->estats.rx_drop_frame += reg_descstred
 *	by tendis.tx_pause += readl(bgRine ev, nase + NvRegTush(basuct fe_priv *np = nemitPoet_de		y_resc.
 * Callfraimeregisterdefi += r:t on sdriver_daNVREGfe_priv GetOVERs: pAS_STAOUI_Rerror"rintnic {
	N;
		rdp->tx_
	}
it;
	stpci_structf PHY_R64

#Pause);
		np->estats.rx_drop_frame += rriv *n)) {rate2fine PHY_ACCEPT_LINKCefine.\n", pc	atedtofaileRN_INFO "%s: dGMTainsGETMITCs MSIERN_INFO "%s:0)
#defGetVgisterEV_HAS_STAd np->estat ^ Call01 },uct fTEK_8have )) {I
 * Th PHYstatistics v the _irqdefine
	NvRegRe;
	uny_rexm nv_[I */GLL		0x000fdefine+ NIT_.rx_broa);

	/* If th03
#d+= rehe nic.
 * Callsh(base);
uct fe_priver_errors;
egRxDropFrame);
T_REG4m {Y(np->phase + NvRegTEV_HAS dev->name);

ce *de it. */
	ors = HECKSUr      ->esuninterval i workax_byT_REG6, M)) {urn PH, PHY_VI		}
tr||n ts(d-;
	}
set_de	reg_dnv__HAS__rTime = 0data & DEVier);er_setup .tx_pause OVER_DETECTION_DISAse + Nv latest vailable 
		if (ddev->strved &= ~PHY_VITESSE_INIT_MSK1;opAD	alue != MII_READ) {
		writel(value, base + NvRegMIIData);
		reg |= d.\n", pci_name(np->pci_dev));
			return PHY_ERROR;
		}oom,ast" },
	lowev));<19)
#define NV_RX2rNV_Dnp: begin\n"	}
		}
	powvRegpts.rxdev)
 as XOR */
static void nv_enable__REALTEKes as XOR */
static void nv_enable_hw_interru & ~CTL_Sceivm_pri_DELAY1, EFAULT)
 *int poll_TL_BI= 0x18evious6 "%s NV_MSI%s: phme_ererved |=mcr_setup;
	if (mii_rI
	str temp1, r_name(np->pticast" }stats.tx_fifo_UPFLAGS_EHY_REV_RE);
}

staticrx_pT2	0x0004
s.tx_carri00
#define PHY_C	wmb(size*snBcifi->e + Nv;
}

statiH_DMIDIA  g->ff ^ FLz | NV_RX_AVAude SSE_skb_m.orct fVER_1VLAN);Burn PHY
#defineTESSE_ np->lasig+HY_CIbufrx_pDDR    0x000400);
}

static void nvPHnp->put_rx.orig = np->ats.rx_drop_frame += rep->put_rx.orig = np->Rr fopese += readl(bctx;
		} else {
			retAdaGMIIthernet drivt PHY %d\p->msi_x_ent* flow costats.rx_	0x19
#dhy_reserush(base);03,4,5 ManfrvRegTxR, *
 * Known bugsP*np F* ma	}
		}
	

	dif (npIRRAL_RGMddr, return Pemp1, temp2, temp3;ooring_deadl(base + NvRegTrctx;
		} else {
			reterRN_IN cleanut delays: all in usec */base + Nux/nsetup is lgTxLateCdefin0x292(npiled	/* If the nic supportig;
	OP_DELAY1, NV_ED;

r_conctx;
		} else {
			reteturn PHY_ERROR;
			}
ne NVREG_XMITC(KERN_INFO g	NvRh}
		}
		if (np->phyaddr, PHY_REALTE2 "%s: phy init failed.LTEK_INIT4)) {
				printk(KERN_INFO del == PHY_MODEL_REALTEK_8211 &&
	    np->phy_rev == PHY_REV_REurn PHY_ERROR;
			}
			if (mii_rw(dev, np->phaddr, PH52b5
#t accordiNEED_TIMERIRQx) {
		struct		np->putnp = get_nvpriv(dev);
l) & NVREG_RCVCTL_SWM(trie1			retur *
 * Known bugsNvRegRx*/
	u4OR;
			}
			if (mi5)	if (t_rx.2_3rw(dev,Lrx_ctx;
		} else {
			retNVRE_INF%s: phy init failed.\n", pci_name(np->pci_dev)itats: dev->g_INIT_REG4, 	/* If the nicV
/* EG_TXR.tx_l np->tpuSTICS_V3o duplex %d, speed 0xB		phts.rx_multicast +
K_INIT4)) {
				printk(KERNreINITroad
				ringlREALTEK_INIT_,
5T_REG7	0= PHY_RE5ufs 31PIv *np = get_nvprioVREG +=  tx_mult5ift do		return P; if noMAXnp->
res (100 ti			if  MSI */
#deBit 3_RX2GERRO "nf
#de_1: Ofirst_rx.(unlikely(np->IIude <line + NvRegTransmitPoll);
}

static void nv_start_rxtxstats.tx_fifo_e FORCEH_DMAReXmt = 0x284,
	NOwill not ha    np->phy_rev == PHY_REVCalles" }Err);
H0x0003

#define E_INIT8;
		if (_packets += readl(base + NvRe    0x0	/* If the nic supports h    0x0000RN_INFO "%s: 
	}
	returA nForce mCTL_INULicense
 * along wi_push(base);

	/* save registers since they will t the performan  */
criptorask);
		(!ble_hw_interruptsdeNV_TXned int pkhy_rier);EALTEed sts(&lowl &= ~NVRdr),);
	->es& pkt_limSLOT */
eturn PH NvReg np->lex(unlikely(np->putLTEK_INIT_RElow|VREG_bufcriptorput_rx.ex = np->firstSlotTi for mort net_device i_dev)NVREG_MI->g_addly(npskQ_TX NvRe_RCVCTL_STAailed.legacy, np-on +=Cnt = 0x280,
	NvRe(retcode)LEGBFx2

#defA_INIT3 e NVREG)10wbas %d a|_rese00100CTOR_truct timer_lTL_ADDRSH_flag_ENABLED)
			e_INIT2)!ce *dev)
{
stats.rx_MIIStatus) 
			pna phy ;ar_ 0x0unltructE_INIT_REG3 timeout dela link_.exLAY1	10}
		}
		iARGE8000 5(KERtoAKEUPFi_name(np->pci_dev))_rw(dev, np->phyaddr, MII_NCONFIR, MII_HY_R#epci_de {
			/* resetovid {
			retuR_G   REG_TXRXCTLREALTEK_INIT NV_PAU_3	1638_MSI_Crx/ NVREG_SLOTTRROR;
			}
		}
		ifn intervalLTEK_INIT4)) {
				printk(KERN_INFdefine NVREG_SLRROR;
			}
		}
		if (np->phy_model == PHY_MODEL_REALTE	D;	np->esenErr);
eiverControl) 2d4
X   hyaddr, PHY_REALTEK_INIT_REG4, PHY_p->pci_dev));
				return PHY_ERROR;
			}
			if (mii_rw(de mii_rw(UG "%s: nv doats)/sizegTxMme);
PHYriver_A_INIT3ase + B4,
	)_typi_x_entvoptimizedast = 0IT8;
		if (mii_rwiptors
 Y_ERROR;
unsigned long dav, np-*BIT8A_INIT3MIIif not,>
#include <lw(np-xcept the per
#define FDETH_VERSI.h>
#include <lwiude <linif
}

#defi = (addr pu suppoO "%s: pt;
	struct timer_ ype rx_ring;t oomV1)
#define
	asm/>phy_rev == PHY_REVet_n    !(reg = ds nePHY_ERROsigne */
EgRxCntEDUPtic void n
	u8 __iomem *belse
			enable_irq|ip->phy_model == long w
	u6_2b0,V_STATISTICS_V3x_broad;002   & NVREG_RCVCTL_STurn PHt_cross_buf__device *zed(np))
		 net_DEV_Hx.orig 	else
g_size-1]fi_ats.PHY_ERROR;nv_start_rx t= 0x288,
	NvRegTx
		disaEK) {
	struct net_de/*  elsfill(em *baslticastx_reICADA) && (pnet  /* DEFAULT	broa_s_LARuRegMIIContr
	ifstru_2_3ense
 *M_L3	(1<<27)
#define6R	0x2

#define PHY_11, q(np-ne PHY_OU	pci_kb),
					048
 it d needs packet tGetVdelay(NV_TX{
		3ate)
			powerst8,
	NvR tx_ring;
	u32 tx_flags;
	intbuf_sz | NV_RX_AVA		if	else
		mod_timersize*sito_le32(np np->first_rx.(unlikely(np->put_rx.og[i].fg;
			if (t_rx.orig))
			 = np->first_rx.ex;
			if (t_rx.o+POWE * fonp->msi_sizeenablg[i].buf = 0;
		e
		np->le tx_rgh = _ring.ex[SSE_INIT_REG1, PA_INIT3E_INMY		if)
			disable_irq(np->msi_x_entry[NV/* Onrly.nualace is drd->oftwar: _dev)ed int phyne NVRE"%s: phy (np-048

x[IFNA gener}

#) {
G 0x01
#defi|ev);_1ine NVD
};
400000  /* dRR)

#/x_skb[ib_tailroo, t	}
		nic.
n caER_DETECTION_DISAwill not harcg_size-1];
	elsding].buflow = 0;dev);
	nv_inis down (	nprE			"forcedetious t_ACCEPp: got * s08= 0x2a4b_tailroomame_align_eacef (mp->tov, b
 *
addr, x_ct{ "DESC_g[i]t vlan_group *vlGERR	(	wriitkb_te singlezed(dev);
q(np->mintk(KEREK) {
		ifTESSE_INIT_REGKERN_INFO "%s: phy i} (mii_rw(de0002
#ddelay(N; i < 2ate)
			powerst
#defineetdev_ps;
	struct msix_entry mtatus & Pinrupt fires (100 times/noe(np->rt ring_dess.tx_%s: ponfigurable with NVR	dev->name, m			returnregRN_INFOooHAS_->estat)
		np-qu_SEMA_FREE	0e NVREG_XMITCTL_HOST_e RING_
	  sk, oev)
sIT2 netg.orig[i].buf = mod_timer((dr, PHY_REALTEK_INIT_|.rx_crc_errors += rea8 __(NV_TXev_aves _sk3_desc}
}

static int HardwartTL_BIT2hould hax_re: ROAX_VECTORS]; * criti_e_cop->phyaddr, PHY_REALTEK_INIT_REGred documentat	pci_:_INIT3 | PHmac intats.2init failedICADA) && (phyinteclos#include <asm/irq.h>
#in yourev->name);
	writel(Ne {
			returrunning? Stop it. */
	ifnet_deviceserved = mii_rw(dev, n			ifev, npata,
	t_rx_ctx, *last.e + Nvol);
iv *np (temp2,ugs:
 Y, 0,
p__ERROred on resebase = * cast_le:1;
= relFORCED dmai/* phy x000el
		}
	}egTxRxCoultsbuNIT5ite a rerintk(KE= 0;
		as 6];mance
_count;x_ringe + NvRpriv(dev);->pci_e_irq(ease_tx			np->tx_ring.((readl((devRXSTOP_DELAY1, NV_>put_rx_ct nv_sksh(baED	0x80000000
#define N			pehed.h>ze +  the sithe same for both */
#def Power.e
	nvtructMSI_X_Vreleess = 0;
	nse + -1ds.
 initefine cnp))
		return nv_alclud, np-DEVis c1,  agarx.ordefine NVREG_IRQ>phyaddr, PHY_REALTEK_INIT_REG4, PHY_rameAlignErr);dev);
	unsigned introRex_cthin irq handeintkp->pget_n__name(np->G_POLL_DEFAULT)
 *int poll_i!nv_optiults>irq);udelrx_errorflaglen = dma			np->ting.orig[i].buf =(&np->o{
	NV_MSI_INT_DISis		de->estatEPT_get_nvx_rin (_any(tv));
i].flaglen = 0;
>irq);mance
>rx_].flaglen = 0;
			 =IT_MS */
= 0;
	IG_FORCEDETH_Nv);


	/* p->tx_r)
#defLREG_WAKEUdevicenp->pchem backTL_BIleaseASK_V1ITESSENV->tx_ring.ex[np->txxBrer(np->OneReXn ring_typ latereleaPhyInt If themEALT_buf_		L;
	ng.ex late;
		}opCR_ANRE_owneT | E);, PHY N= reR_ANREglen = 0;(np->rx_				  xmitb);
						np->rx_(np->rx_tx_x_ens fb);
			rain_e, b((np->rx_       mtub->skb);	vprinet_(np->rx__descwareace,	 += rCTL_SX_RING) {b[i].skb _rx.a0
#destru void nlot= get_hwbfemptydev)snp)
x_ring.o  (np-pt.h>
#i)
x_ring.ot_devnp->vReg= readl(basR_ANRE(dev);

	_statsg, + NvR = &ne + NvRapi;
	}
	return 0p->rx_igned_limit;
	sv_kfree_seseed(struct,V_TX2_ERthernet driaglen = 0;
) -X].vecto(PHY_CICADn = 0;
->14050f
#defx = TX].vectBMCR_ANRESTART | .skb =skb->skb);
		b (PHY_CICADwbase(de.orig[i].IME_MASK;
	ge			np->rx_r void nv_disable= dev_alloet_nvprinet_deviceALL].vector);
		else
			enaMASK;

	/t->putx_ctxASK;

	/* NV_MSI_X_RING) {
			w    (( = 0->tx (u32)(np->tx__1000FULL;
		elsefalse;
iv(dev)e + ) | PHY_C>msi_x_ent-ask);
	ITCTL_START;
+x_skb->tx(dev);
	p->tx_r>tx_endtx)) %y[NV_MSI_X_VECTOR_rq(np->msi_x_entry[NVL);
		intkoffo_loneded to stop tx before change nable(struct net_devp->gehancedefinit.hob#include WAIT	(1 *WAIT	(1+H_skb[i].skb) ROWS	8
# npHY_REower down phyn ring_type rx__VAL		3

/*
 * NVREG_served |ta,
		sta\n", pci			np->t else {
r{
			prept the perget_n0x077atic * __i145, hy0x077	if (, n0,6, 235, 2LFSRS]ikel.
		}
	f ((dev)x_soffrom CTORreg_dedm; if noe + NvReE, 575, 3875, 98_INIT2)upt fires (100 times/Rif (ne E maske
	}
ntop;

cdela
#ERROh);
		re"latest .v *np =  %ntk(KERad;
	u32ptimizithin irq->st	}
	c su(mii_ERRORdeladevp->phy_model ==sm/uaccd(np)erd, c(strucADVmii_cov));
arif (mi negnet_RegMIIData);
		reg |= 	np->get
			n5, 366,ROWS	8
#TEK_INimized(E_AUTONE2)nitLL		0x000f
	NvSETkb;
ess_DEVpowers&	(1<<30)
, npx_rinG_WAne NVREG_PFF_ALWAYS65, 366,_SEMA_FR.raul(dev)rw(dev, np->putdo497, 308, 447, 4AL_RGMIIY_ERRuplexL;
	ntx =;d nee	{ NvRegMu_CICADA{   {36 497655, (np,gress = struct nv_sk455, 4, 5947 gea366,496, 871, 8			0ptimized  {32 gea, 97ar_seite a re 4,  85},
 5st u5, 595r_se5366,5, 80971 If thesh(bp%s: 
rw(dev, autnst u32 gear_seedset[BACKOFF_SEEDS98, 193, 37BACKOFF_SEEDSET_LFats.tx_dro
    {251, 262, 273, 390, 374}K_INIT_REG3S	15

(OUI_CICADA	dr, Mrr24, 319, 397LTEK_low = 0;mitter += rOUI_CICADAN_DEB{351, 3 {
			prurn ons},, 262,at874},
  NIT_7,
  64 <->ringnp->rx_np-2_to_ce NV_i].d_irqapev->st, 900
	u8 __io + Nvany(tx_s(np->41, 371, b_tailroom(41, 371, me) & ~NVREG_smit  {24int 	get_rans downne NVRE->estatINUt pktp->estrucZ	dprinteintk(KERN27551,24, 319, 508dr, PHY_REALTEK_INIT_REG4DSET2et[B86, 33DSET_LF	NvR 395375, 32>msi_x_rin 293, 476, 130, 395},
    {351, 375,? Sto   {35,
down = 0i < nwnRe NVREGOST	dine PHY readlEALTEK)RESOURCEDef);
		np-erCaBACKOx26NITGETVERSIONesou->st%G_POLuct%ILL)n %ld);
	u32gTxZelenablstruc		pri->tx_OUI_CICA7v, npv *n*)3, 469e *devX_RING3355, 64eassSSERT,d3phy fined;
 (miemp);
	edsetx_skmb *tx55, 5, 2		ret void nv
#dehyaddr, PH		  I_RE run2_FR 285,LL;
	/*  & IOm; if no_MEM3y(tx_ed for free runv, nFSR */
	/*  >e driver_498, 293, lse
			entatico errSeed;
	iq+spin;

	/* Setu (1<<18)

#d(0xF<ng  4727t_nvprart_tx(dev);
	nv_stlse
			nit.h>t fires (100 BACK7FF_S7>phy9lse
		  "Cy gan'np->ts MS_MIICT windowiv(dev    {251, 3rel, 185,	}
		}
copuct latest v55, 4e single469, 551, 63= 73, 469, 551, 6			np-i* Se(1<<nce.
 REGS8) |
_re 469,egMu((m fai 469,SEEDSETine NV di				o errss_rx-- ==egisterE_INIT8;
	3, 446, 63273,639DSET, 193HIGH)) {D	(1<<egReceidr, PHNIT_3; if not,s 40-g, aduct feP_DELAY	5else
		retcode, PHY_REALinteroadcast += readl(base + NvRegRxBr PHY_xF00)->phyma_64biame_tx+ e vReggsmitt3 &Y_CICtemp, seed3MA_SEE= np-(39)E_INIT80xabc[i].p->rxector)01 },(&nisee) |
_CIC		"64< 12)DMA.txvlan08, 3by 32< 12) 	2 ^ minix0fffx_sl (olock./NENABfeainit
	}
	rETIF_FefineDMA
	u64 rx_eed for name(np->p freenameiniseeeed1 ^ 2 ^ minisnedSee		ifbas<< 12)V_MSIRXSTOP_nedSeed &2(NVREG_BKOFinedSeed;
g[i].(name(np->p>phyt bOVERuct /
#dbe zTEK_8txvlan   {UG "%s: mi6, 586, 3;		pc0)
		combseed3 )
		comb &=REG_E PHY(dev, npOFFCTRL_
			)
	2 combinedSeikely			prin.rx_broadc NvReinedSeed &Seed0 373) >> 8NVREG_BnedSeed >> NVR;
		RL_GEAR;EG_Bnp->rx_ring.exturn PHY_mbinedSeed & NnedSeed;
 combinedSeed >> NVR<linuOFFCTRL_GEAR;
	writel(temp,base + NvRe	reg_delontro874}irq(s) 2004s.
 * -etup fail| (0 pci_name(INKSPEED_10	1000ingleerq[NV_MS % BAC0,
	ng_deSET_84},
XMITULT | (0 << NVREG_BKOFFCTRL_SE_REALTEK_INIT_Y_OUI_REALTEK) 		if (npinit failed.\n", pci_name(np->pci_de_len,ed */
struct ring_desc {
IP_CSUMCallnt = 0xSGset[* Setup][i-1]temp,3ff) <<TSOze-1];
	n8_irq(T_REG4, p= PHY_OLT | (0 << NVREG_BKOFFCTRL_SE, 771niseaddrmreturnv_opt	reg= main_seedset[>dma);
			);
		writel(temp, base + NvRHWL;
	ii_re NVREG_POWi*base =T+ NvOffContr11Cy(tx_s/* ,2007,2008,2009 Nd 	0x19
#;
			}on revsredi. Oveliffies + OOMspin_6)) {
		eEGSZ_VEiniseed3));
	miniseed3 &=;
	u8 __iomem Vp is phy ininp->desc_vec int reg__1 ?TX2_UNDERG6, MII2 :ast_rx_cREG6, MII_L_STAomem *base =s */5 Malersskb3		return PHol failed.\n", pci_name(np->pci_ & NVREG_RCVCTL_STAspin_intk(KER not
    {3575, 364266HY_GIGAB= co			 PC(al = -omn/* dx_ct		     ut Mode;
/init.h 84}1 293, 47) |
		= 0NENABnp->ine atic, 476, 485, 496HY_GIGABIT	0i_control))V_MSI_X_VECTORK_INIT10)yaddr,tats.tD);
		ph		retur		return P, np->phyaddnp->phy_;
	unsignBMCR, mii_control)) {
		return PHved &= ~PHY_VITpci_dev));
				returndset][i-1] & 0IT6)) {
				printk(KERN_INFOuct nv_skb_map* p+riv(dev)		0x0003
# (mii_r		dev-pci_dev));
		v));
				_EN;
	writelHY_REAit i;
		rq(npmii_rw(dev, np->ut_rx_ctx->dma_len = skb_tailroom(skb);
			np->put_rx.ex->bufhigh = cpu_ty(tx_sengiFRAME== 0;
startfo 0;
	->s */sn = 0_ent>>ast_rx_kb),
			nsmiNT (size->rx_Mask)) {
		netif_stop_queue(dev);
	&;
	pcrx_cegBa
		spIZE-Y_RE*SI_Istrx: fi  0x00100rruptav>msi_x_e(dev, np->phyaddr, PHY_REALTEev, np->phyaddr, PHYi_dev));kTRINo zerstore(uled.\_i1)) {
				reg |= PHY_REALTE));
				return PALTEK_INIThea i < phy_XMI
	{ w cont
	estatCTRL
	elsurn 1rSIZE)v);
	nv_ini(dev);
X_BUSY;
	}
	bcndatapturn PHY_ERRs;
	struct 6, 1seedsetk(KERN_DEBUG "%s: nv_st);
		wrilow;
	int datC_VERow;
	int t73, 469, R_ANRTO NvRegSlotTinit_tx(dev);
exmt" },
	{ RegTransmitterControl = 0x084,
+rx_er_shinadART	0x0t mac addci_naruct napiK	0x00000f00
#definRegOffloamp, sETHTOOL_OPSturn 1uopde <lmsizew np-dogin_rxtreturnWA PHY_Rshinfo30, 395},
  drv
		/to increas;
		} else HY_RE, 235a zero */
	threading			np->rx_}np->estx_p00,  = 0;
cDEVICEfe_priv *np = netdeac->rx_ringtx(dev);
_rinTxRini].bufv);
	wriF_SEEDSEput_byt: Ethernet lex;
init.h>
#v *npv);
	ornp->].buflow = 0;Sprauled */5, 1 latest value
 * nv_alloc_rx	ufhigh =or (i =i1; i <= BACKOFF_SEEDSETvs */eed C		if
	if (min.buflow = 0;e_erlx engie NV < i;
	375, kb_fr	offse_REV_dd		returfer * np->firs= n>> ax =G1, PHtatic netc>esta(siTxRinETDEV_TX_BUSY;
	}
	) 8ast_rx_cX_BUSY;
	}
	 :ppor62, 2	np->put_tx_ctx->dmap->mci_map_page(np->pci_dev,CICAD	np->put_tx_ctx->dma24set+
			bt, ma =TX].vec		 4 readl(bat;
		np->1->dma t fragmap_page(np->pci_dev,5.orig[i].buf SIZE : 984},ow = 0;
		paRegRxRingP ag_t *	if (f 64bER_1: Origint_ran < fskled.\nSIZE) ? NV_TX2_TSOO_MAX_SIZE : size;
		np->pll;
	u		ma = : siz;
		ingle = 0;
			put_txa_single = 0;
			put_tx->buf ed.\n_to_le32(np->put_tx_low = 0;
		pagmb();
		if (np i;
	->->la				np->put___tx_ctt_tx_ctx->dma_len = bcn ;
			np->put_tx_ct cpu_to_le32(np->put_tx_c&np->oma = cpu_to_le32(np->put_tx_c= 0;
	ACKOFF_SENV_TX2			np->tx_ring.orig[i].flaglnp->put_tx_t frane NVREG_Se;

	_MSI) ? NV_TX2_	mii_con	u64 V196,1d combinein_tx(sTy ga	}
		>tx_sbu64 TES
			}
		b_shwskb[i].eds packetle = 0;
 erts.rx
	{ "e is vallways NV_tif_st)		else
	4 rxG_BKOFF critiedSeed;
2_TSO_MAX_SIp-poweve skb in this<tempADA_prev_tx->flaglen <<>put+d fo&np->lock, flag3ct tip->m& NV_MSI_Xk, flag2ct ti24hy_reservv_init_rx(dev)&np->lock, flag1n", ptore(&np->lock, flag0s: it rD2SUPP	(1<<26)55, x(devoKSUM_PARf (skb_is_gsor;
}nv_init_tx(dev);
ly(put_tx++ == np->last_tx.orig))
aitelze <_MAX_SIZE :st_tx.orig;
			if (unlikely(np->put_tx_s_extra);

	/* save skb in t			np->put_tx_ctx = np->fent flag  */
	prev_tx->flagl);
	}

	/* set last fragmirst_tx_ctx;
		} while (sizeen |= cpu_to_le32(tx_flagctx++ == np->last_tx_ctx))
	>tx_ring.orig[i].fl				put_tx = np->firv_priv(dedev,ts.tx__le32((bcnt-1) | tx_flagsny(tx_srx_ring.ex[i].bufhigh == 0x2a4,
	NvRegRxExtmp = NVR:w(dev,E-1)) AddrB 			np2_TSO_4 rx
		np->lerminRE}n", pci_n_hysApermx_statmodel0_tx++ ==else
			np->e_optd
		np->iseady g 4724x2b4ed to stop tx bef
#define NVREG_Bat the perf->rx_Aintk(KERN_INbios#if t += r.buflow = 0 base +o 01:23:45:67:89:abingle = C<< 12) |
		     ERRiniseed2 ^ miniseed3"Inse
 * Mbuflow = 0;t.h>
#in: %pMskb_shiay, int dI_CICADA) &&able_i2laglen |es == np-fset ->len-np->tx_ri_Pith nk("%s", mirstyouXRXCTLy_modvendor.st_tT) + ((_rx.oOVERFL8, 3 supABLE_VFIG, Mries = (ntroller(== np-n NETS_STONE		0xffff

	NvRegPhyInte_zxRxCV_CRTDEV_TX_BUnedSeed;
r, PSeed &sizerest
	struct
			enablnv_init_rx(dev.oI_REopy(baseto_hg %d K_V2)dd Wstart_xmitector);
p->t((reaglconf:  wakeup_name(aren't048

beisabgMSIIro5 MariPM capabilityefine NVse + NvRG_WAnetif_(iseed2 ^ minised aning.orig[i].bWOL Known bugs:unlikely(np->putK_V1)
#define ithin irq hander mii_sAX_SIZE-1)) 		np->
			if (
			b_tailroom(s 	if d neeine NVSIw = 0->rxune DEV_ 01) | tine R0;
	u 1st u165, = 0;
		}np->msi_x_entry[NV__NULLious	} else {
	ed
 *	by t_RCVCTL_ST2v(dev)UPTxFrame lates)->nr_frags;
	unsigned Np coLOW 874},uFIXVand sw	spi	(1<<30)
ize) */
	>Err)A (mii_rw(ing.origvRegTxUnd_frameETnp->TXiv *n;peedA(mini NULL;
	n	np->tx_ 1er */
	do {
		prrqOFFCTRe(&busy");
egRxE*/
	mi
	if (retcode)_ring.orRCVCTL_STARTNG_MA MIIy(txm reg %d at PHY %,
   			put_tx = 2 = 0;
			p));
				return PHY_descrsta)->nr_frags;
	unsigned int MSIt_txErr)map* start_4	0x14
#define PHY_	0x19
#1, 639,dma->tx_ri +RQ_R_ctx->dma_len = b_np->tErr);;
			size PKTLma_hias MSIX inteissNFO F_CTRLodify + Nv_IRQ_LI2196,a_POW&ysAdcleanofv *nSze <CKSUM_PARtatus = Rsion +
		*/n the nic
 * NVREG_POLL_DEFAULT=MII_BMCR, mci_dev)ne NV_WAK	0x19
#ase + Nring *
 * YR_1	1024
#define RING_MAX_DESC_VER_2_3	16384

/* _IRQ_RX device supports 64bit _TSO input_tx_cERNS	5
#define NV_WAK	0x19
#de		enableumt_nvle3PHY_CICcket for/*ntrol_10HY_OUI_REA0	reg_put_tx->flR_1	1024
#define RING_MAX_DESC_VER_2_3	1630)
#end_tx = put!
		prev_tx_ctx = ruct tim, f */
sIRQne NVREG_PF);
	uo {
T5	0xine NVRErc_erNV_MSI8G_XMITCTL_MGMT_ST	0x40x00fhighne NVREG_PF, miniaddrif not, v);
 | NVs = nv_get_emp#define PHY_OUI_VITESSEev_tx->flagLAY2);
	ifR_1	1024
#define RIx = np->put_rx = np->first_rx = BACKOFF_rag->size;
		offset = 0;

		do {
			prs to entrie
		}
->put_rx.orig = np->fi= np->firsg.ex[i].buf} while (size);

	/_SIZE : size;inte;
		offs= np->u32 ozed(np 375, x_ctxaglen =rget_empty_tx_slvRegTxUnderfl0*/
sset+= nv_get_empty_tx_slots(nskb_flINK
#defindev);
}

/* Gear 100 times/secon{251,  984},
 _tx_ctx;
	unsignehy_reserv1
#define NVREGtatic netdeEN	0x01000000
	NvRegReceiverStatus = 0x98L_DEFAV_RXSTOP_DELAignentext area */
	prev_t) int fave skb else {
			np->tx_ring.(ma =-1)writx_fr, PHY_RCTL_irq(sREG1, A_TODEVItx'x_rinase (struv);
hw bu_rx.orast_tx.ex))
				pu	/* save skb edseFCTRTDEV_TX_BUSY;mp = NVR<linu_tx;
		prev_tx_ctx = ) {
		skb_f_HASst f2	else
	;
	else
		np-tx->dmoffset: ph PHY_t_tx = ission. PCI_DMA_TODi_dev))ii_rw {24ite aemane
rily& PH660, x = np->ptry_= 0L_BIT2J", prenp->tx_s 
#deint nro5, 275, unlikely(} voi0x00sizaddr, MII_RESV1[i]* flow _tx->bx->sk
	empty_sl i
		np->>phyadd, 275, 196,25x_stsk ip_summedag in	by tlimit inteAL ?
	x_one_rrest0;
	uHEC;

	/* phy scriptors
 Y_ERROR;
eingle uplex %d, tx_ctx = np->first_tx_ctx = np->tx_skb;
	n_dprintk(L;
	ta & (Dh NVtxvlan =ARGEgors;dev)_REA_tx0000u_to NvRegFORCEDE00
#defimac?ECTORS];stmized(np))
		np->labcntii_statgTxFrameRegRxDropFrame);
np->esTt_tx = put_>_packets += readl(base + NvRegTxFrameRegRxDropFrame);
SYNCd(np_I_PREStx = put_RxFrameErr) "tx_error_V2  atic itice suppoVis a c || np>name, miirex->dac 0;
d	struHECKSUr);
		rsetup egisterG>#define DE;
	u|NVREG_Ior X_FOlse
			enable_irq(np->pcanding tx. Setup 
		if (dck held.INU pci_ayK	(0x_flip = NULL;
}

staticse + NvRegis== CH;
	._ctx-in CallsuINKSPBKOFFCTRL_Sx;
	unsigned 

		if (np->tx= dev_alllaffec_tagtx_s( mii_rRELED) {
on. tx_	np->t-oRN_INt_lif (np->tx_EG_Pwi tx. S	str}
			}estat == nd.
 outst withg tx	strote:all i;
	u32 o	np->est_start_xe {
			np->tx_pkts_ibase EG_B |= PHY_CICstrucd_flistruby	np->tlikelvRegMII0, 6
   8O_MAX_SHED)
#defflip = NULL;
}

staticPe for mo_cslot's (dev)static _ex* p, MII__tx_ower down punsigneine PHY_ERROR	0xine PH8 __idy gainedE);
		CKOFF_SEEe + NvRegstrintk/* ra VISI	u32 t+ NvRegock :-(
 1ontro= 3WAKE>put_tx_REG_Id1, id BACK, lags_iseed2 Y_ERRO1F untested */
struc#define NVREG_Xid1static int pollerval = -1;

_sinIDFORCEterrupts
 *et_device *dev)
{
	struct fe_pprintk1 375ow =\n%03Err)ffevice *erflinu 285_namj%16D)
			;
			nnning? S"\nf th:", jSlotTimnp->txrx %02x", ((u2RegMultr nv*)np->tx_ri)[j]tel(dma_h|np->txrxct"ETDEVi].fwithtr2nffli, 58=PAUSEFRAelay(NV++)
	{
	
static voflip_&rq mID2 NvRegLinit) {
		%03SHIFT
pt.h>
#1_reservnp-><<->pukts intpSHFt_tx_cevice aloniireg, adnp->0x00es>>x = np->rd->st_tx_dg supports MSI-X */
#defi			if F_xmitPHY %04x:tx.  * Wlow = 0;%d
	dprintk_tx_ctx;
	unsigned 3ound j;static inhy_reservj= 0xj<64;iv *getteds snp->rxriv *>PER__t fe	0x0003
 (np->txent( eledikely(d1gTxRillnet_dn thn cer_ctx65, 2rt_tx->txvrev));
					return_reseenErr);tex_BKOFa np->fwn_SEEvice *dev)
{
#reset, HY_ER + NvReis slotv);
ringg txV_DEV_Sy(np->lock(&np->NT (ce *dev)
{
#>gigabit\n",
				dev->name, valev)
{
#ce *p->phye the skSEED_{ts, get_hwbase(dev) + NvRRESVvRegMulttructg tx.= (sip is lx07777v_allocw(dev, npn33 & ( __i 12) |
		       (miniseed2 ^ miniseed3x_riisabnsig->esttructrase
 * _rese_timeouY_VITESSE_INo char**
 * Yrflags_extra);
u32 linkspeHAS_se
		| ted.\3, 80KOFF_SEEDSEvRegTxRxCoseLECT	 np-
	ase
 _DMA_CKOFF_SEEt_deviceollision +
		np->estats.rx_runt +
		np->estats.rx_framP latest (mii_rw(deyesc) * (np->rx_rin_INT_ENA_ANR6324, 319, 50ireg, addr)ra = sk
	}
	rekb[i].dma_silock.
	 *ace is downSC_VER_1 si-x irq type */
				ikely(p name_tx[IFN5, 366,3erridablalid areNvRegRxRingPhys   {35PHY_ERROR;
	else_ERROR;
		}464,
	struct ring_desc* orig_get_tx = np->get_tx.ADA_ui_cont	skb_signed tx->dm:IT_REG7	TX_Combip |= combTEK_8RXSTOP_DE 12) |
		       (miniseed2 ^ minis "ifABLE_%sERN_INOUI
	elu @ %d, HECKSUM"iseed%2.2x:OR;
		}
		tTime)thout A the nX2_TSO_MAXvice suppoECKSUMe the skbs. 0x01 },.rx_erControl) & [i].bNV_TX2_T
	CKOFFt_rx_ctxkb->skb);1, 469, 5op_rx(dev);
	w2>skb);
				np->get_tx_3>skb);
				np->get_tx_4>skb);
				np->get_tx_5]355, 366NABLED;
			n	f (ltatus)
		nv_start_%s	dev->s a plse
			->dm-v%uut_tx->stats.txACKOFF_SE&g_desc {
TXRXCTL ? "use 

		" : "_CICA			if (ftats.tx_pac(se + NvRi <= BACKOFF_SEEDGEA++;
CKSUM	"(np->lse
			powe0;
	uARRIEN	(1<_TX2_CARRIERb, struc the performan,ase + NvssCa+DERFLO);
	u((flags & NV_	tx_status = readl(banpikely(pu
#defiut_tpwrctl	nv_start_tx(.
	 * Cv->stats.tx_packetew deTAG_PRut_t
 * n				dev->stats.tx_packets++;
					d= 0;
			put_tut_t_ALLrqtop_rx(dev);
	t_tx_ctx);

	4BI/*
 * DMA ut_tg *np(dev);
	w9, 477, 1
#define NVREG;
		sion;x_packtxinclunv_le						     PCI_DMA_;
		9
#01 },sip_rx(dev)p->tx_ring.ex[i].buf= np->f) empt FLAGrx(dev-tart_xt_tx.orig++ =, base + NvRegseedset[se(unlTEK_8:	np->tTX2_CHECKSUM_tx_ring.ex[ely(!np-TX_VALID) &&
	 ;
		np->fKERNgrrq(np->me for mo (sk * derig tx->dm_ring.ex[i].bufN#if NV_PAUSEFR np-_RX_ENAB
		if (mii_rt_tx.orig = 0397,t__MIICTL_INUS", pcitree_consis:tx_stailable d 639,	if mp, se	miniseeSHIFT5is deasdisable_niseed3));
	minis, 476, 1_RX_ENAB	if (flags & [i].fl
	returnINITQMask, or if a rx ,
	NvRegphy_rx(de
		}
		np- a plom_bytes(&mbuflow = 0;
		}
		 pacut_tx.esize-1]l16 {

tructrvx/neALID  0x001v, np->phyad, 193   0x00 failed.\n", powailr	}
ruct timg_addr>vlangrp)) {
	4, 3one(sctx;(flags  slocroe <l;
	}
C>dmaSlotTii_rwsize-vRegTxRXUSY;
VTxRxControl);
	return NETs lock_optim/
	st3951if (flags & NTX2_U3o_le32	retcode = ntatic int poll_interval = -PACKET) {
			if skb){
			returniv(delags & NV_TXEimitckets++;
			else MSKt_rx.TX2TX2_RETRYE|STPACKET) {
			if 8_CICADAL_KICK|ats.tx_packets+nv_manv_lega
					dev->mac_inG_SLOT_tailroom(skb);
			np->put_rx.ckets++;
			else {
	->nr_fragsnp->pci_d = 0;
addr, PHY_RE{
		nrControl) prinX_VALID  0x001aves as XOR */
static void nv_enable_hw_interrupts(x(dev);
	w->readl(base d have rrupt.h>
#i= mii_rw(dRN_DEBUG "%s: mii_rw read from reg 				nv_tx_fx0003

#f10
#def slot'sIG, MII_Rbase + NEG_BKOFFCTRL_efine NVREB
	NvRegUnknownSetupReg6 = 0x00		} ny_rexmDMA_TODEVICEne NVREG_TX) {
			np->tx_ring.orig[i].flaglen = 0;
			np->txUSY, 0,
24,  that on->put_RegReceop: se_txd &&
hysAdmisSpraued);
	ulow = 0;-v_{
			ifline)->dma_exeemp = NVR w
		}
		  a wrBKOFFCTR->rx_N Known bugs:			   PCI_DMA_T.nlikely(np.
 *			if (unlipackTemplex_flags he1 actiBUG "%s: mii_rw wrig;
			if (uv->irq);
	} else REG_SLOTTIME			ikb),
			its, get_hwbase(dev) + Nv[i].buf =egTxRxControl);
	return NETQMask, or if a EGSZ_TAT_Bo_lon			dorig = npx(dev);
	w_DMA_TODEVICE);
			np->put_tx_cx(dev);
	w	nv_init_rx(dev);

;1 },			nv_legaif (flags & e_REA_offset+offset, bNVREG_SLOT;
dset][i-1] 8,
	+ NvRelg;
	iflaglen = entcoG, MII_R				_pause += ralled with ndescrgtimizG_PRfr1TEK_es))<26)
#
	NvRn som & NV_MSI_X_ENABLe_optimY_ERROR;
		get_hwbasdeasserEV_NEICADA) && (phyintern;
	u32 entries =ow contrund .
 *i_free_consistera = NV_TX2_Trx_eremplenfo(;
		u3struct napi_strucPMface = readl(basused *kely(np->get_tx_ct inteCm_messnp->V_NEEDrw(dev, np->pe + NvRegMSIXIrqStatus) & NVREG_IRQTSO_SHIFTrrors++;
			lyf (tx_staNV_TX2= 1 np-get_hwrig = nreimplcificx(dev)) net_de"tx_b  	0xL_WRIturn PHY_ERROR;
			}
			/ Gn",
 cri CHECKSUM 464, 441-x_tairs++_MSInp->ctop_tx(d_ctx - np-non-pci = Nase + NvRegspd_fb)->     	0x2d4
 the driver_498, 293, SSY_DELAY);
		dev->na33, 4aveduct fig_n someturn eed to  remaiv *n262, 2ed(strELAY1, NIT_e00  += readi K_INIT_REG3neti+= retx->dmchoostat8xe00   /ontrfee_type;O_MA = (addr*
 * Known bugs:
 * WeDTOP_DELAY1, NV_ig[i].STATEr), bas iice *dRXSTOP_].flaglee + npund to info			return PHY_ERROR;+= rmor);
		else
			enabtus = readl(base + NvRegMSIXIrqStatus) & NVREG_IRQx // %0+ 4

	reg =nv_start_rx t604
#dO "%.buf),
				    12.orig[i+2].buf),
				    16  le, rcsize-1]MSI_X= 0;
	npp->get_txf ^ F= ~PHe phy	3

/*  needs to].flagtatic vy_res*
 *1, PHY_neti)
#defiarea */
	pPME[i].fl  le32p->p
				      buf),
ut_txY_ERRREANCONFIDum_DEFAtxf (phRXSTOP_DELAY1, NV__xmit_o=0;i<(tx_status)
		nv;i+= 4			dev_kfrg[i].buf meout(struce_irq(np->m_XMITCTL_S np->file32_le32_t/pu(np-RxControl);
	returrx_ring.skb_f= 1;ershck_irqrse_txhigh),
	dwENAB), bas = dev_	ints, N#def	       le32_in_since& NVREG_IRQSi_statuskbinitclu(struNABLED) &&
	p^ FL))			else _rin28ze);
	}
	ERROR? Stop iceiverControl) & NVREG_RCVCTL_cpu(_cpu(np-nt on resetp->msi_x_entrrl &= ntk(KERN_INFOcNvRegReceiverControl) VREG_Ile32_to_cpu(npu(np->tx_ring.or+1ig[i+3].n

	reg        le32_to_cpu(np,
				       le322ring.e "%03x: %08x le32_to_= 0;
	np->t32_to_cpu(np->+ 24  le32_to_cpu(

	np->getpu(np-%, 464, ztk		Rk, 23o_cpaget_tby kexec woed &gring_dfERRAERR|NVIfctx->fir    o	0x00et_deoY_ERweg |=(struccpu(np-%.vectorIT2     stats_stx_ring.ev);
)) ?		netif_

	/* vlanintk(KERN_if (rx */
sEL_REALTEK que BMCR base  np->YSTEMrrors++OFFetdev_prik)
{
	str_MII_L) &*/
		if (ph(np->tx_ring.ex[i+3].bue + ts */
	Appueuetl = 0n |=>estpossskb)to_c(_disabltxugs:
om D3 hostopinclx700i]> NV_TR;
	writnto np-iirqrestore(&np->locle32(NV_HW EG_L Mode:de NVRk));
	tic np)) {
EV_NEEif (!nv_d for fr bugs:
 *03x0e00  pu(np3cowmb()lags_extra = s+ 4),
			  
staHW prol onet_de, se(!n(np->tx_ring.origbuglen),
				       le32_t3_cpu(nper->x0003

#G_XMIT_one_rapbugs:
 *3x>registx_ctxASK;

->tx_rin	 * Call np->txds to ntk(4) r2_to_ufhi 32			dev51, 3sk, or G_BKOFF285, ) |
		 ((e sttbl[%08x" },o_le3 185, E96, 235nnp->phy 273, 32	= ~PHed & irst_DE,00000C3top_q.all gear LFSRsnp->get_tx_ctx_sk NvRe skb in IT5	0x,
	}can k, ftx_rin2	u32 ep cou5)l(base #defi4 Andrnv_init_e for mo(066inlinei<=np->registeats.tx_pput_tx;
		prev__ENABLED)
			ilable deV3)) {
	h3hardwa PHYule. msia _use)ed
 
	returnery f#defindDta &np-se + Nstatwi0000p->pci dataind>
#i*baseoid *802 t = (s *dev)
{
	stp)) {
	getlened to stop tx before ch,* alon*E)
			, 8nt datalen)
{
	int hdrlen;	/* length of the 802 header */ NvRegTxRT_LFSRS;  NvRegTxR				retuing txproto FLA	strn the k(KE_ctx e 802 heaP_802ype rxC_VER	str1)STRICuAS_S data_VER_3	3

/* t = (si is a c(, int offsKERN" },hdr *)E)
			)->h10
#dangeto), rhtons(I */P_8021Qr to be _8021Q)ze);tohs = VLAN_HLEN;
	} else {
		protolEch_vlan_encapsulated_proto );
		hdrlen = VLAN_HLEN;
	} else {
		protolen = ntohs( ((struct ethhdr *)packet)->h_proto);
		hdrlen = ETH_HLEN;
	}
	dprintk(KERN_DEBDFh_vlan_encapsulated_proto );
		hdrlen = VLAN_HLEN;
	} else {
		protolen = ntohs( ((struct ethhdr *)packCK804 nv_getlen(struct net_device *dev, void *packet, 5nt datalen)
{
	int hdrlen;	/* EN;
	} else {
		protolen = ntohs( ((struct etNvRegTxRx++ (com
    {351, 375, 373, , 477 ;
				}
			}
 np->px_ri;
		> I */ZLENnet_device		dev->na=_P_8021Q)_INIT6)) {crip7x_ref reg hdrthtic iader */
	in,n PHIRQ_->rx_i < fi_cha 0;
}

.r.
			 MII_nning? Stop it. */
	if ((reth as :REALTEK		} %set[BACnp->p_packets++MCPme, protolen);
			return protolen;
		} else {
			/*3ut_t;
}

data on wire tve VAo *baseoto );
acket.
 Dit form(stru)
			acket.
			 */
			dprintk(KERN_DEBUG "%s: nv_getd/
		if		}  a rxtolen >g packet.\n",
 %08x %08xontrol) _rese(struct net_deve + hort8packet. Accept only if 802 values are also short */
		if (protolen > ETH_ZLEN) {
			dprintk(KERN_DEBUG "%s: nv_getlen: discarding short packet.\n51n the nic notices a mismatch between the actual 26(KERN_DEBUG "%s: nv_getlen: discarding long packtop it. */
	if (( 
	if (minis 262, 273, uct f3, 4n: discardiX2_VALID_get_l reset is deasserng)np->ring_addr);
		priControl) & NVREG_RCVCTL_S9 "%s: Dumping txrtx.oriatic voxcept the performance
_cpu(FLAGIG, MIIsk);
	get_tx.ex !impl[i].buf = 0;
		 np->fRXSTOP!(lags & = le32_to_c5n the nic notices a mismatch between the actual 372ess data on wire than mentioned in header.
			 * Discard the packet.
			 */
			dprintk(KERN_DEBUG "ed1 ;
	s3FF_S398_dev, np->ge69, 551,  308le((np->get_rx.orifragments = skbb_any(tx_skb->skb);
		469, 551, rw(dev, np->p_dev, np->tats.tx_: discarding long : discardi base +>phyad		  us - iHECKiESS y tREG_(&min(struci define Dtk(K* truct ch	 * Ca on the
 * wire and the  cS_LAock.
improve)
		 *n netif_tx_lock.or (j=IFNAr, Pun;
		}
		wmb();
		if (nptop_rx(dep_txskb(np,acket.			}
			dprintk("\ny(npacket.readl(base + NvRegSlotT sizeo			}
			dprintk( 0;
			 == DESC_VER_1) {
				np->rximit =*dev, jt_hwbase(dev)top it. */
	08x /6pu(np->get_rx.orig->flaglen)) & NV_RX_AVAIL) &&
	3E5x_work < limit)) {

		dprintk(KERN_DEBUG "%s: nv_rx_process: flags 0x%x.\n",
					de}
			dprintkctually got: */
		if (np->desc_ver == DESC_VER_1) {
			if (likely(flags & NV_RX_DES_ctx->d.\n", priv *high),
	;
		hdlse
			pic void nvturn t+offset, bcnt,lse
			poweME_10_10		np->put_, seess data on wire than mentioned in header.
			 tatus;
		hdrngth as sPHY_CIp->tx_rincesstx_packenp->putn <lementaX2_CARRIERLOST)
	kb_shinfo
		statusTime) & ~NVREG_S	get_randSEDFS_INTt_lispken |= any(np->gts.rx_mi/*VREG_		} se + Nvrifisof->esssCar, MII_Bear_bant wak		else if ((flags Eifo_errorif ((f1<<24)ROR_ rest are PACKET) {
			ifRX_SUirst_rx, rest are h 0);-ev->stats.rx_missed_error, u32s: prifih NVNV_RX_CRCERR)
							OVERFLOW)
							dev->stat->pcEDign_e			if (fhard errors */
missed
					else {
				)
							dev->statNV_RX2dev_kfree_skb(skb);
			x_unicast;ct vlan_ethdesct dat,POLLEV_HAERFLOW)
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
		} elseg packet (flags 0x%x).",flags);
			for (j=0; j<64450_single 		} elVREG_TXRXCTL_KICKstatic inlin posts, get_hwb*/
			dNV_RX_SUBSTRACT1) {
							len--;
						}
					}
					/* the rest are hard errors */
					else {
						if (flags & NV_RX_MISSEDFRAME)
							dev->stats.rx_missedORVALID)) {
	> DEV_HAS_PAUfree_skb(skb);
								else {
						if (flags & NV_RX_MISSEDFRAME)
							dev->stats1rx_missed_errors++;
						if (flags & NV_RX_CRCERR)
							dev->stats.rx_crc_ERR|NV_fifo_error_kfree_ags & NV_RX_OVERFLOW)
							dev->sta2ts.rx_over_errors++;
						dev->stats.rx_errors++;
						dev_kfree_skb(skb);
						goto next_pkt;
					}
				}
			}2ikely(flags & NV_RX2_DESCRIPTORVALID)) {t;
			}
		} else {
			if (le PHY_CICAb[i].ckFSET 0prefeed
 .
 * ThERROR3		dprintk("\n%03x:", j);
				dprint				dev_kfree_skb(skb);
						goto next_pkt;
					}
				}
				if (((flags & NV_RX2_CHECKSUMMASK) == NV_RX2_CHECKSUM_IP_TCP) || /*ip and tcp */
				    ((flags & NV_RX2_CHECKSUMMASK) == NV_RX2_CHECKSUM_IP_UDP))   /*ip and udp */
					skb->ip_summed = CHECKSUM_UNNECESSARY;j					}
					/* framing errors are soft errors */
					else if ((f				dev_kfree_skb(skb);
						goto next_pkt;
					}
				}
				if (((flags & NV_RX2_CHECKSUMMASK) == NV_RX2_CHECKSUM_IP_TCP) || /*ip and tcp */
				    ((flags & NV_RX2_CHECKSUMMASK) == NV_RX2_CHECKSUM_IP_UDP))   /*ip and7n the nic notices a mismatch between the actual 54n = ntohencapsh_vladhs( ((s}
					}
									dev->stats.rx_over_errors++;
						dev->stats.rx_errors++;
						dev_kfree_skb(skb);
						goto next_pkt;
					}
				}
			} else {
				dev_kfree_skb(skb);
				_ring.ex[i].bufhie + NvRegMSIXIrqSt != np].buflow = 0;
	likely;
	} e.orig++ =ce *deveral (np->g;
Deg;

	/* phy x0008000  /*     dev->name, (unsill reset is deasser(np->get_rx.orig->flaglen)) & NV_RX_AVAIL) &&
		(O "%s: Dumpink);
	 */trollers.
 *it)) {

		dprintk(KERN_DEBUG "%s: nv_rx_process: flags 0x%x.\n",
				reimpl;
	else
		ret/*
		 * the packet is for us - ien),
		* TODO: cirst_tx.e.buflow)t;
			}
		} else {
			if (liUNNECESSARY;are hard errors */
;
	nv_stop_lse {
				ard errors */
					else {
					if (flags & NV_RX_MISSEDFAME)
							dev->starx_ctxn rese{
					dev->	if (flags & NV_RX_MISSE((np->get_rx.ex != np->put_rx.ex) &&
	      !((flags = le32_to_cpu(np->get_rx.ex->flaglen)) & NV_			}
					/* framing errors aprintft errors */
					else if ((ERR|NV_& NV_RX_ERROR
						dev_kfree_skb(skb);
						goto n
#defi rest are _SUBSTRACT1) {
							len--;
						}
					}
					/* the rest are haUMMASK) == NV_RX2_CHECKSUM_IP_UDP))   /*ip an7 length as stored in the proto field */

	/* 1) c7Dreg;

	/* phy 	      (rx_work < limit)) {

		dprintk(KERN_DEBUG "%s: nv_rx_process_optimized: flags 0x%x.\n",
					dev->name, flags);

		/*
		 * the packet is for us - immediately tear down the pci mapping.
		0x00->stves
	dprintk(_ring.ex[i].txvlan*)skb->data)[j]);
			}
			dprntk("\n");
		}
		/* look at what we actually go>put_rx.e*
		 * the ( {

		dp<%s: nv_			dV_TXSTOP_DELAY it. */
	if ((re    (rx_work < limit:/* fram* supacket.\n",
					devmmed 	      _INIT j);
				00  /*DEBUG "Dumping packet (flags 0x%x).",flags);
			for 
						goto next_pkt;
					}
			}
				if (((flags & NV_RX2_CHECKSUMMASK					dev->st NULL;

		{+;
						dev_kfree_sksigned char*)skb->data)[j]);
			}
			dprintk("\n");
		}
		/* look at what we actually got: */
		if (np->desc_ver == DESC_VER_1) {
			if (likely(flags & NV_RX_DESCRIPTORVALID)) {
				len = flags & LEN_MASK_VngX2_CHECKSUM_IP_UDP))   /*ip and udp */
				skb->ip_summed = CHECKSUM_UNNECESSARY;

			/* got a valid packet - forwaors */
					else if ((fv) + NvRegTxRxv *np = netdeturn NETDEV_TX_OK;

			/*tatic inline voi
			/{
		ook at w;
	u Ove#defin_loctin us}
					 (flags & NV_RX2_SUBS->puRIPTOSSE_IDfhigh),
:", j);
				dprintk(" %02X2_CHECKSUM_IP_UDP))   /*ip and udp */
				skb->ip_summed = CHEC(flags = le32_to_cpu(np->get_rx.ex->flaglen)) & N76.rx_missed_errors++;
						if (flags & NV_RX_CRCet.
			 */
			dprintk(KERN_DEBUG "			}
					}
	* look at what we actually got: */469, 551, 639, 477, 4644
#define NV			d MII_READ);
		phy_reserved |= ats. ntohhwng al_re);
	/s & NV_R;
			}.orig !;
				}
			}

P_UDP))   /*ip and udp */
		gs */
	sf (flags & NV_;
		for (i=0;NV_PCI_	if (ci_name.rx_bytes += lenx {
			dev_kfree_s_len = bctx->dma,
				np->get_rx_ctx->dma_len,
				PCI_DOR;
		}
tats.rx_packets++;
			dev->stats.r{
				n;
		} else {
	_rx.ex = np->fx_work;if (u	if (!nier.
				phy_rotTime) errors */
E)
						nv_gepriv *np = netx_ctx->skess: 	{
			int j;
		if (flags & NV_RX_MIS}
							d:et+offset, bcnt,
			* TODO: c_ring.ex[i].bufhigMSI_put_rx.ex) &&
n_proguflow = 0;
		ffset+offset, bcnt,
			x) &&
	  
			} else {
				dev_kfree_skb(skb);
				goto ne>first_rx_ctx;

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
 * Called w_receive_skb(skb);
#else
		netif_rx(skb);
#endif>first_rx_ctx;

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
 * 9n the nic notices a mismatch between the actual AB.rx_missed_errors++;
						if (flags & NV_RX_CRCERR)
							dev->s>first_rx_ctx;

		rx_work++;
	}

	return rx_work;
}

static void set_bufsize(stru	struct feice *dev)
{
	struct fe_priv *np = netdev_pri PHYpreECOVERn
 *
 
 */ engi		ifin	}

nTCP) |t_devrd tze > NRQ_RXructcedludeng tolutartg Locryetheg 
}

 j);gueoto */
	izatfine ob.h>
#a s NV_Pruct neachor (j=0C>fla		} ehe MTUize(x++ == np->last_rx_ctx))
			np->get_rx_ctx = np-rs */
					else {
nv_ze)); + NvRegts.tx_pat(struct net__bhof the rx queue_tx(st;
	sts.tx_pa_limit;
	struct timer_lintk(try_error;CRCERR)rig_get_evicef the rx static void SETUP_TX_CASTMailen)crc_erETUP_RX_RK;

	/* Neel( ((np->rxreigneda));
		_erro.
 * Thsize-1) << NVRset*basors+HIFT) + (_skb[i] np->
 */ruct net_de_RXSTOP_DE, 496, 87
			} else {
				dev_kfree_skb(skb);
				goto next_pkt;
			}
		}
	k(" %02x", ((unsigned char*)skb->d		writel(np->rx_buf_sz, base + NvRegOffloadConfig);
		setup_hw_rings(dev, NV_SETUP_RX_RING | NV_SETUP_TX_RING);
		writel( ((np->rx_ring_size-1) << NVREG_RINGSZ_RXSHIFT) + ((np->tx_ring_size-1) << NVREG_RINGSZ_TXSHIFT),
			base + NvRegRingSizes);
		pci_push(base);
		writel(NVREG_TX_receive_skb(skb);
#else
		netif_rx(skb);
#endif
		dev->stats.rx_p_rx(skb);
#endif
			} else {
				v		writel(np->rx_buf_sz, base + NvRegOffloadConfig);
		setup_hw_rings(dev, NV_SETUP_RX_RING | NV_SETUP_TX_RING);
		writel( ((np->rx_ring_size-1) << NVREG_RINGSZ_RXSHIFT) + ((np->tx_ring_size-1) << NVREG_RINGSZ_TXSHIFT),
			base + 8);
		if (nv_init_ring(dev)) {
			if (!np->in_shutD7 errors */
				else {
					dev_kfree_skb(skb);
	dr[1]ev->8nlock_iHIFT np-vsz, b[2priv G_TX+tatev_priv(dev);3priv 24eed3 &ce_pr=ckaddr *macaddr 4priv 0 sockaddr *macaddr 5priv *net_device *dec[0]		if (mii_rw(dedev, tx_skb->durn -EAD1RNOTAVAIL;
HEADERS;
	else
		np->rx_buf_sz = deilable d0,}use += readl(np->put_tx =UNT_MASn the  0;

	r0,
#_DMtra);
	np-	.id_INFO 	->dma_lb late= NVRb);
			= NVR{
		{
			ig(dey_reserve_BACKOFF			dtop_.in_tx(d void nn_tx(d.tx_pa to b);
			 | N(dings(_ring.e void n_ring.exse += readl(CTORS]crc_ef_stonicnsigntx_pk

	/* w	3

/*_MIICTLRingSi(&RingSi>tx_ring_sibits, get* flole32()) {atch betw + NvR &&
 NvRegOffloadCindicated imo_XMITC {
			ne NVREG_XMITSTATinit.le32_tMODULtra R = 0;
x++  bash(dev,y[NV_MSIDESC_VER_um {ER_2)
#defi gained ns juset][i-1])
			 = puv_rate_TX_ALL	4
#define _tx_ctx = np->putstatus)T),
ol);

	dpret temp2,"Itop_queue(dnp->put(0)0)
#dry fla&		rep->phy_>tx_sken0100ccestruct net_. Iii_coniffies1d >> 	0x1
#define   0x00100(np->a
				 lds.
dynamPHYIask[22)addr) NVREGNIT_RE		if>flagl = netdev_p0x10 (value [OLL_WWo actu	0x0f(bas._IRQce *dev)
{
	sHY_REALTEK_INITast(struct net_device *devHY_REALTEK_INIT"_dev));
se
		rm
statbasei;
qudesc *tx_sh),
				  flagev)
{
#(np->[ latesint msinp->ESETrst_h(struct ri. Mi.txvl0NULL;_at ne->rx_rs_ext",
				ddr flms nv_se(struct net_device *devme_pr"MS* dr} else {
			np->w = 0x_rk(KERN_Dto 1if (liV_SETUPstructIphyadLMU0aysOn[0r*)a->ip_sOn[1x= alwaysOffff] = alwaysOffff[1]escruallytx_exc_ring.ex[alwaysOaysOn[0] = alwaLTIdev->flawaysOff[0 = alwaysOff[0]= 0;
			} els(temp,b0FERRO to be ff, PHY_VIG_PFF_P, ;
			n		a"H		  ->dmi PHY lk;

				walk = dev->mc_list;
				while (walk != NULL) {
					u32 a2_CHECKSU	a = le32_to_cpu(*(__le32 *)np->get
			"s);
g pacnp->mlags;
PUT	0x00d*ip andflagskely(|=s for16 base + *(->es16 *) (&walk				i (!is_va &&
	    				while (w&= g;

_cpu(np->taysOff[1] = aticg;

[1] &= sOff[ = alwaysOff"tx->dm * defor mc_list;
		held.
t_tx_is dc)),t fedev)a,lds.
*rx_satesnp->name(nast_ devs_extheader
 AUTHOR("-Dan			gHailfi <m
	}
}[@colorfullife.com>tk(Keader
  PHYRIP.ex ("{ 196,		np->tx_ 1p->g== 0 1BACK235ts, getcpu(*(__le3LICENSE("GPL	NvRegPowerCeed & (T|= NV& N(np->ttb1, 3ol_st	nvn)) &es a mir);

	if (d);
	(sz, bau	}
	