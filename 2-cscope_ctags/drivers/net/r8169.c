/*
 * r8169.c: RealTek 8169/8168/8101 ethernet driver.
 *
 * Copyright (c) 2002 ShuChen <shuchen@realtek.com.tw>
 * Copyright (c) 2003 - 2007 Francois Romieu <romieu@fr.zoreil.com>
 * Copyright (c) a lot of people too. Please respect their work.
 *
 * See MAINTAINERS file for support contact information.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>

#define RTL8169_VERSION "2.3LK-NAPI"
#define MODULENAME "r8169"
#define PFX MODULENAME ": "

#ifdef RTL8169_DEBUG
#define assert(expr) \
	if (!(expr)) {					\
		printk( "Assertion failed! %s,%s,%s,line=%d\n",	\
		#expr,__FILE__,__func__,__LINE__);		\
	}
#define dprintk(fmt, args...) \
	do { printk(KERN_DEBUG PFX fmt, ## args); } while (0)
#else
#define assert(expr) do {} while (0)
#define dprintk(fmt, args...)	do {} while (0)
#endif /* RTL8169_DEBUG */

#define R8169_MSG_DEFAULT \
	(NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_IFUP | NETIF_MSG_IFDOWN)

#define TX_BUFFS_AVAIL(tp) \
	(tp->dirty_tx + NUM_TX_DESC - tp->cur_tx - 1)

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
   The RTL chips use a 64 element hash table based on the Ethernet CRC. */
static const int multicast_filter_limit = 32;

/* MAC address length */
#define MAC_ADDR_LEN	6

#define MAX_READ_REQUEST_SHIFT	12
#define RX_FIFO_THRESH	7	/* 7 means NO threshold, Rx buffer level before first PCI xfer. */
#define RX_DMA_BURST	6	/* Maximum PCI burst, '6' is 1024 */
#define TX_DMA_BURST	6	/* Maximum PCI burst, '6' is 1024 */
#define EarlyTxThld	0x3F	/* 0x3F means NO early transmit */
#define SafeMtu		0x1c20	/* ... actually life sucks beyond ~7k */
#define InterFrameGap	0x03	/* 3 means InterFrameGap = the shortest one */

#define R8169_REGS_SIZE		256
#define R8169_NAPI_WEIGHT	64
#define NUM_TX_DESC	64	/* Number of Tx descriptor registers */
#define NUM_RX_DESC	256	/* Number of Rx descriptor registers */
#define RX_BUF_SIZE	1536	/* Rx Buffer size */
#define R8169_TX_RING_BYTES	(NUM_TX_DESC * sizeof(struct TxDesc))
#define R8169_RX_RING_BYTES	(NUM_RX_DESC * sizeof(struct RxDesc))

#define RTL8169_TX_TIMEOUT	(6*HZ)
#define RTL8169_PHY_TIMEOUT	(10*HZ)

#define RTL_EEPROM_SIG		cpu_to_le32(0x8129)
#define RTL_EEPROM_SIG_MASK	cpu_to_le32(0xffff)
#define RTL_EEPROM_SIG_ADDR	0x0000

/* write/read MMIO register */
#define RTL_W8(reg, val8)	writeb ((val8), ioaddr + (reg))
#define RTL_W16(reg, val16)	writew ((val16), ioaddr + (reg))
#define RTL_W32(reg, val32)	writel ((val32), ioaddr + (reg))
#define RTL_R8(reg)		readb (ioaddr + (reg))
#define RTL_R16(reg)		readw (ioaddr + (reg))
#define RTL_R32(reg)		((unsigned long) readl (ioaddr + (reg)))

enum mac_version {
	RTL_GIGA_MAC_NONE   = 0x00,
	RTL_GIGA_MAC_VER_01 = 0x01, // 8169
	RTL_GIGA_MAC_VER_02 = 0x02, // 8169S
	RTL_GIGA_MAC_VER_03 = 0x03, // 8110S
	RTL_GIGA_MAC_VER_04 = 0x04, // 8169SB
	RTL_GIGA_MAC_VER_05 = 0x05, // 8110SCd
	RTL_GIGA_MAC_VER_06 = 0x06, // 8110SCe
	RTL_GIGA_MAC_VER_07 = 0x07, // 8102e
	RTL_GIGA_MAC_VER_08 = 0x08, // 8102e
	RTL_GIGA_MAC_VER_09 = 0x09, // 8102e
	RTL_GIGA_MAC_VER_10 = 0x0a, // 8101e
	RTL_GIGA_MAC_VER_11 = 0x0b, // 8168Bb
	RTL_GIGA_MAC_VER_12 = 0x0c, // 8168Be
	RTL_GIGA_MAC_VER_13 = 0x0d, // 8101Eb
	RTL_GIGA_MAC_VER_14 = 0x0e, // 8101 ?
	RTL_GIGA_MAC_VER_15 = 0x0f, // 8101 ?
	RTL_GIGA_MAC_VER_16 = 0x11, // 8101Ec
	RTL_GIGA_MAC_VER_17 = 0x10, // 8168Bf
	RTL_GIGA_MAC_VER_18 = 0x12, // 8168CP
	RTL_GIGA_MAC_VER_19 = 0x13, // 8168C
	RTL_GIGA_MAC_VER_20 = 0x14, // 8168C
	RTL_GIGA_MAC_VER_21 = 0x15, // 8168C
	RTL_GIGA_MAC_VER_22 = 0x16, // 8168C
	RTL_GIGA_MAC_VER_23 = 0x17, // 8168CP
	RTL_GIGA_MAC_VER_24 = 0x18, // 8168CP
	RTL_GIGA_MAC_VER_25 = 0x19, // 8168D
	RTL_GIGA_MAC_VER_26 = 0x1a, // 8168D
	RTL_GIGA_MAC_VER_27 = 0x1b  // 8168DP
};

#define _R(NAME,MAC,MASK) \
	{ .name = NAME, .mac_version = MAC, .RxConfigMask = MASK }

static const struct {
	const char *name;
	u8 mac_version;
	u32 RxConfigMask;	/* Clears the bits supported by this chip */
} rtl_chip_info[] = {
	_R("RTL8169",		RTL_GIGA_MAC_VER_01, 0xff7e1880), // 8169
	_R("RTL8169s",		RTL_GIGA_MAC_VER_02, 0xff7e1880), // 8169S
	_R("RTL8110s",		RTL_GIGA_MAC_VER_03, 0xff7e1880), // 8110S
	_R("RTL8169sb/8110sb",	RTL_GIGA_MAC_VER_04, 0xff7e1880), // 8169SB
	_R("RTL8169sc/8110sc",	RTL_GIGA_MAC_VER_05, 0xff7e1880), // 8110SCd
	_R("RTL8169sc/8110sc",	RTL_GIGA_MAC_VER_06, 0xff7e1880), // 8110SCe
	_R("RTL8102e",		RTL_GIGA_MAC_VER_07, 0xff7e1880), // PCI-E
	_R("RTL8102e",		RTL_GIGA_MAC_VER_08, 0xff7e1880), // PCI-E
	_R("RTL8102e",		RTL_GIGA_MAC_VER_09, 0xff7e1880), // PCI-E
	_R("RTL8101e",		RTL_GIGA_MAC_VER_10, 0xff7e1880), // PCI-E
	_R("RTL8168b/8111b",	RTL_GIGA_MAC_VER_11, 0xff7e1880), // PCI-E
	_R("RTL8168b/8111b",	RTL_GIGA_MAC_VER_12, 0xff7e1880), // PCI-E
	_R("RTL8101e",		RTL_GIGA_MAC_VER_13, 0xff7e1880), // PCI-E 8139
	_R("RTL8100e",		RTL_GIGA_MAC_VER_14, 0xff7e1880), // PCI-E 8139
	_R("RTL8100e",		RTL_GIGA_MAC_VER_15, 0xff7e1880), // PCI-E 8139
	_R("RTL8168b/8111b",	RTL_GIGA_MAC_VER_17, 0xff7e1880), // PCI-E
	_R("RTL8101e",		RTL_GIGA_MAC_VER_16, 0xff7e1880), // PCI-E
	_R("RTL8168cp/8111cp",	RTL_GIGA_MAC_VER_18, 0xff7e1880), // PCI-E
	_R("RTL8168c/8111c",	RTL_GIGA_MAC_VER_19, 0xff7e1880), // PCI-E
	_R("RTL8168c/8111c",	RTL_GIGA_MAC_VER_20, 0xff7e1880), // PCI-E
	_R("RTL8168c/8111c",	RTL_GIGA_MAC_VER_21, 0xff7e1880), // PCI-E
	_R("RTL8168c/8111c",	RTL_GIGA_MAC_VER_22, 0xff7e1880), // PCI-E
	_R("RTL8168cp/8111cp",	RTL_GIGA_MAC_VER_23, 0xff7e1880), // PCI-E
	_R("RTL8168cp/8111cp",	RTL_GIGA_MAC_VER_24, 0xff7e1880), // PCI-E
	_R("RTL8168d/8111d",	RTL_GIGA_MAC_VER_25, 0xff7e1880), // PCI-E
	_R("RTL8168d/8111d",	RTL_GIGA_MAC_VER_26, 0xff7e1880), // PCI-E
	_R("RTL8168dp/8111dp",	RTL_GIGA_MAC_VER_27, 0xff7e1880)  // PCI-E
};
#undef _R

enum cfg_version {
	RTL_CFG_0 = 0x00,
	RTL_CFG_1,
	RTL_CFG_2
};

static void rtl_hw_start_8169(struct net_device *);
static void rtl_hw_start_8168(struct net_device *);
static void rtl_hw_start_8101(struct net_device *);

static struct pci_device_id rtl8169_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8129), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8136), 0, 0, RTL_CFG_2 },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8167), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8168), 0, 0, RTL_CFG_1 },
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8169), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_DLINK,	0x4300), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_AT,		0xc107), 0, 0, RTL_CFG_0 },
	{ PCI_DEVICE(0x16ec,			0x0116), 0, 0, RTL_CFG_0 },
	{ PCI_VENDOR_ID_LINKSYS,		0x1032,
		PCI_ANY_ID, 0x0024, 0, 0, RTL_CFG_0 },
	{ 0x0001,				0x8168,
		PCI_ANY_ID, 0x2410, 0, 0, RTL_CFG_2 },
	{0,},
};

MODULE_DEVICE_TABLE(pci, rtl8169_pci_tbl);

static int rx_copybreak = 200;
static int use_dac;
static struct {
	u32 msg_enable;
} debug = { -1 };

enum rtl_registers {
	MAC0		= 0,	/* Ethernet hardware address. */
	MAC4		= 4,
	MAR0		= 8,	/* Multicast filter. */
	CounterAddrLow		= 0x10,
	CounterAddrHigh		= 0x14,
	TxDescStartAddrLow	= 0x20,
	TxDescStartAddrHigh	= 0x24,
	TxHDescStartAddrLow	= 0x28,
	TxHDescStartAddrHigh	= 0x2c,
	FLASH		= 0x30,
	ERSR		= 0x36,
	ChipCmd		= 0x37,
	TxPoll		= 0x38,
	IntrMask	= 0x3c,
	IntrStatus	= 0x3e,
	TxConfig	= 0x40,
	RxConfig	= 0x44,
	RxMissed	= 0x4c,
	Cfg9346		= 0x50,
	Config0		= 0x51,
	Config1		= 0x52,
	Config2		= 0x53,
	Config3		= 0x54,
	Config4		= 0x55,
	Config5		= 0x56,
	MultiIntr	= 0x5c,
	PHYAR		= 0x60,
	PHYstatus	= 0x6c,
	RxMaxSize	= 0xda,
	CPlusCmd	= 0xe0,
	IntrMitigate	= 0xe2,
	RxDescAddrLow	= 0xe4,
	RxDescAddrHigh	= 0xe8,
	EarlyTxThres	= 0xec,
	FuncEvent	= 0xf0,
	FuncEventMask	= 0xf4,
	FuncPresetState	= 0xf8,
	FuncForceEvent	= 0xfc,
};

enum rtl8110_registers {
	TBICSR			= 0x64,
	TBI_ANAR		= 0x68,
	TBI_LPAR		= 0x6a,
};

enum rtl8168_8101_registers {
	CSIDR			= 0x64,
	CSIAR			= 0x68,
#define	CSIAR_FLAG			0x80000000
#define	CSIAR_WRITE_CMD			0x80000000
#define	CSIAR_BYTE_ENABLE		0x0f
#define	CSIAR_BYTE_ENABLE_SHIFT		12
#define	CSIAR_ADDR_MASK			0x0fff

	EPHYAR			= 0x80,
#define	EPHYAR_FLAG			0x80000000
#define	EPHYAR_WRITE_CMD		0x80000000
#define	EPHYAR_REG_MASK			0x1f
#define	EPHYAR_REG_SHIFT		16
#define	EPHYAR_DATA_MASK		0xffff
	DBG_REG			= 0xd1,
#define	FIX_NAK_1			(1 << 4)
#define	FIX_NAK_2			(1 << 3)
	EFUSEAR			= 0xdc,
#define	EFUSEAR_FLAG			0x80000000
#define	EFUSEAR_WRITE_CMD		0x80000000
#define	EFUSEAR_READ_CMD		0x00000000
#define	EFUSEAR_REG_MASK		0x03ff
#define	EFUSEAR_REG_SHIFT		8
#define	EFUSEAR_DATA_MASK		0xff
};

enum rtl_register_content {
	/* InterruptStatusBits */
	SYSErr		= 0x8000,
	PCSTimeout	= 0x4000,
	SWInt		= 0x0100,
	TxDescUnavail	= 0x0080,
	RxFIFOOver	= 0x0040,
	LinkChg		= 0x0020,
	RxOverflow	= 0x0010,
	TxErr		= 0x0008,
	TxOK		= 0x0004,
	RxErr		= 0x0002,
	RxOK		= 0x0001,

	/* RxStatusDesc */
	RxFOVF	= (1 << 23),
	RxRWT	= (1 << 22),
	RxRES	= (1 << 21),
	RxRUNT	= (1 << 20),
	RxCRC	= (1 << 19),

	/* ChipCmdBits */
	CmdReset	= 0x10,
	CmdRxEnb	= 0x08,
	CmdTxEnb	= 0x04,
	RxBufEmpty	= 0x01,

	/* TXPoll register p.5 */
	HPQ		= 0x80,		/* Poll cmd on the high prio queue */
	NPQ		= 0x40,		/* Poll cmd on the low prio queue */
	FSWInt		= 0x01,		/* Forced software interrupt */

	/* Cfg9346Bits */
	Cfg9346_Lock	= 0x00,
	Cfg9346_Unlock	= 0xc0,

	/* rx_mode_bits */
	AcceptErr	= 0x20,
	AcceptRunt	= 0x10,
	AcceptBroadcast	= 0x08,
	AcceptMulticast	= 0x04,
	AcceptMyPhys	= 0x02,
	AcceptAllPhys	= 0x01,

	/* RxConfigBits */
	RxCfgFIFOShift	= 13,
	RxCfgDMAShift	=  8,

	/* TxConfigBits */
	TxInterFrameGapShift = 24,
	TxDMAShift = 8,	/* DMA burst value (0-7) is shift this many bits */

	/* Config1 register p.24 */
	LEDS1		= (1 << 7),
	LEDS0		= (1 << 6),
	MSIEnable	= (1 << 5),	/* Enable Message Signaled Interrupt */
	Speed_down	= (1 << 4),
	MEMMAP		= (1 << 3),
	IOMAP		= (1 << 2),
	VPD		= (1 << 1),
	PMEnable	= (1 << 0),	/* Power Management Enable */

	/* Config2 register p. 25 */
	PCI_Clock_66MHz = 0x01,
	PCI_Clock_33MHz = 0x00,

	/* Config3 register p.25 */
	MagicPacket	= (1 << 5),	/* Wake up when receives a Magic Packet */
	LinkUp		= (1 << 4),	/* Wake up when the cable connection is re-established */
	Beacon_en	= (1 << 0),	/* 8168 only. Reserved in the 8168b */

	/* Config5 register p.27 */
	BWF		= (1 << 6),	/* Accept Broadcast wakeup frame */
	MWF		= (1 << 5),	/* Accept Multicast wakeup frame */
	UWF		= (1 << 4),	/* Accept Unicast wakeup frame */
	LanWake		= (1 << 1),	/* LanWake enable/disable */
	PMEStatus	= (1 << 0),	/* PME status can be reset by PCI RST# */

	/* TBICSR p.28 */
	TBIReset	= 0x80000000,
	TBILoopback	= 0x40000000,
	TBINwEnable	= 0x20000000,
	TBINwRestart	= 0x10000000,
	TBILinkOk	= 0x02000000,
	TBINwComplete	= 0x01000000,

	/* CPlusCmd p.31 */
	EnableBist	= (1 << 15),	// 8168 8101
	Mac_dbgo_oe	= (1 << 14),	// 8168 8101
	Normal_mode	= (1 << 13),	// unused
	Force_half_dup	= (1 << 12),	// 8168 8101
	Force_rxflow_en	= (1 << 11),	// 8168 8101
	Force_txflow_en	= (1 << 10),	// 8168 8101
	Cxpl_dbg_sel	= (1 << 9),	// 8168 8101
	ASF		= (1 << 8),	// 8168 8101
	PktCntrDisable	= (1 << 7),	// 8168 8101
	Mac_dbgo_sel	= 0x001c,	// 8168
	RxVlan		= (1 << 6),
	RxChkSum	= (1 << 5),
	PCIDAC		= (1 << 4),
	PCIMulRW	= (1 << 3),
	INTT_0		= 0x0000,	// 8168
	INTT_1		= 0x0001,	// 8168
	INTT_2		= 0x0002,	// 8168
	INTT_3		= 0x0003,	// 8168

	/* rtl8169_PHYstatus */
	TBI_Enable	= 0x80,
	TxFlowCtrl	= 0x40,
	RxFlowCtrl	= 0x20,
	_1000bpsF	= 0x10,
	_100bps		= 0x08,
	_10bps		= 0x04,
	LinkStatus	= 0x02,
	FullDup		= 0x01,

	/* _TBICSRBit */
	TBILinkOK	= 0x02000000,

	/* DumpCounterCommand */
	CounterDump	= 0x8,
};

enum desc_status_bit {
	DescOwn		= (1 << 31), /* Descriptor is owned by NIC */
	RingEnd		= (1 << 30), /* End of descriptor ring */
	FirstFrag	= (1 << 29), /* First segment of a packet */
	LastFrag	= (1 << 28), /* Final segment of a packet */

	/* Tx private */
	LargeSend	= (1 << 27), /* TCP Large Send Offload (TSO) */
	MSSShift	= 16,        /* MSS value position */
	MSSMask		= 0xfff,     /* MSS value + LargeSend bit: 12 bits */
	IPCS		= (1 << 18), /* Calculate IP checksum */
	UDPCS		= (1 << 17), /* Calculate UDP/IP checksum */
	TCPCS		= (1 << 16), /* Calculate TCP/IP checksum */
	TxVlanTag	= (1 << 17), /* Add VLAN tag */

	/* Rx private */
	PID1		= (1 << 18), /* Protocol ID bit 1/2 */
	PID0		= (1 << 17), /* Protocol ID bit 2/2 */

#define RxProtoUDP	(PID1)
#define RxProtoTCP	(PID0)
#define RxProtoIP	(PID1 | PID0)
#define RxProtoMask	RxProtoIP

	IPFail		= (1 << 16), /* IP checksum failed */
	UDPFail		= (1 << 15), /* UDP/IP checksum failed */
	TCPFail		= (1 << 14), /* TCP/IP checksum failed */
	RxVlanTag	= (1 << 16), /* VLAN tag available */
};

#define RsvdMask	0x3fffc000

struct TxDesc {
	__le32 opts1;
	__le32 opts2;
	__le64 addr;
};

struct RxDesc {
	__le32 opts1;
	__le32 opts2;
	__le64 addr;
};

struct ring_info {
	struct sk_buff	*skb;
	u32		len;
	u8		__pad[sizeof(void *) - sizeof(u32)];
};

enum features {
	RTL_FEATURE_WOL		= (1 << 0),
	RTL_FEATURE_MSI		= (1 << 1),
	RTL_FEATURE_GMII	= (1 << 2),
};

struct rtl8169_counters {
	__le64	tx_packets;
	__le64	rx_packets;
	__le64	tx_errors;
	__le32	rx_errors;
	__le16	rx_missed;
	__le16	align_errors;
	__le32	tx_one_collision;
	__le32	tx_multi_collision;
	__le64	rx_unicast;
	__le64	rx_broadcast;
	__le32	rx_multicast;
	__le16	tx_aborted;
	__le16	tx_underun;
};

struct rtl8169_private {
	void __iomem *mmio_addr;	/* memory map physical address */
	struct pci_dev *pci_dev;	/* Index of PCI device */
	struct net_device *dev;
	struct napi_struct napi;
	spinlock_t lock;		/* spin lock flag */
	u32 msg_enable;
	int chipset;
	int mac_version;
	u32 cur_rx; /* Index into the Rx descriptor buffer of next Rx pkt. */
	u32 cur_tx; /* Index into the Tx descriptor buffer of next Rx pkt. */
	u32 dirty_rx;
	u32 dirty_tx;
	struct TxDesc *TxDescArray;	/* 256-aligned Tx descriptor ring */
	struct RxDesc *RxDescArray;	/* 256-aligned Rx descriptor ring */
	dma_addr_t TxPhyAddr;
	dma_addr_t RxPhyAddr;
	struct sk_buff *Rx_skbuff[NUM_RX_DESC];	/* Rx data buffers */
	struct ring_info tx_skb[NUM_TX_DESC];	/* Tx data buffers */
	unsigned align;
	unsigned rx_buf_sz;
	struct timer_list timer;
	u16 cp_cmd;
	u16 intr_event;
	u16 napi_event;
	u16 intr_mask;
	int phy_1000_ctrl_reg;
#ifdef CONFIG_R8169_VLAN
	struct vlan_group *vlgrp;
#endif
	int (*set_speed)(struct net_device *, u8 autoneg, u16 speed, u8 duplex);
	int (*get_settings)(struct net_device *, struct ethtool_cmd *);
	void (*phy_reset_enable)(void __iomem *);
	void (*hw_start)(struct net_device *);
	unsigned int (*phy_reset_pending)(void __iomem *);
	unsigned int (*link_ok)(void __iomem *);
	int (*do_ioctl)(struct rtl8169_private *tp, struct mii_ioctl_data *data, int cmd);
	int pcie_cap;
	struct delayed_work task;
	unsigned features;

	struct mii_if_info mii;
	struct rtl8169_counters counters;
};

MODULE_AUTHOR("Realtek and the Linux r8169 crew <netdev@vger.kernel.org>");
MODULE_DESCRIPTION("RealTek RTL-8169 Gigabit Ethernet driver");
module_param(rx_copybreak, int, 0);
MODULE_PARM_DESC(rx_copybreak, "Copy breakpoint for copy-only-tiny-frames");
module_param(use_dac, int, 0);
MODULE_PARM_DESC(use_dac, "Enable PCI DAC. Unsafe on 32 bit PCI slot.");
module_param_named(debug, debug.msg_enable, int, 0);
MODULE_PARM_DESC(debug, "Debug verbosity level (0=none, ..., 16=all)");
MODULE_LICENSE("GPL");
MODULE_VERSION(RTL8169_VERSION);

static int rtl8169_open(struct net_device *dev);
static netdev_tx_t rtl8169_start_xmit(struct sk_buff *skb,
				      struct net_device *dev);
static irqreturn_t rtl8169_interrupt(int irq, void *dev_instance);
static int rtl8169_init_ring(struct net_device *dev);
static void rtl_hw_start(struct net_device *dev);
static int rtl8169_close(struct net_device *dev);
static void rtl_set_rx_mode(struct net_device *dev);
static void rtl8169_tx_timeout(struct net_device *dev);
static struct net_device_stats *rtl8169_get_stats(struct net_device *dev);
static int rtl8169_rx_interrupt(struct net_device *, struct rtl8169_private *,
				void __iomem *, u32 budget);
static int rtl8169_change_mtu(struct net_device *dev, int new_mtu);
static void rtl8169_down(struct net_device *dev);
static void rtl8169_rx_clear(struct rtl8169_private *tp);
static int rtl8169_poll(struct napi_struct *napi, int budget);

static const unsigned int rtl8169_rx_config =
	(RX_FIFO_THRESH << RxCfgFIFOShift) | (RX_DMA_BURST << RxCfgDMAShift);

static void mdio_write(void __iomem *ioaddr, int reg_addr, int value)
{
	int i;

	RTL_W32(PHYAR, 0x80000000 | (reg_addr & 0x1f) << 16 | (value & 0xffff));

	for (i = 20; i > 0; i--) {
		/*
		 * Check if the RTL8169 has completed writing to the specified
		 * MII register.
		 */
		if (!(RTL_R32(PHYAR) & 0x80000000))
			break;
		udelay(25);
	}
}

static int mdio_read(void __iomem *ioaddr, int reg_addr)
{
	int i, value = -1;

	RTL_W32(PHYAR, 0x0 | (reg_addr & 0x1f) << 16);

	for (i = 20; i > 0; i--) {
		/*
		 * Check if the RTL8169 has completed retrieving data from
		 * the specified MII register.
		 */
		if (RTL_R32(PHYAR) & 0x80000000) {
			value = RTL_R32(PHYAR) & 0xffff;
			break;
		}
		udelay(25);
	}
	return value;
}

static void mdio_patch(void __iomem *ioaddr, int reg_addr, int value)
{
	mdio_write(ioaddr, reg_addr, mdio_read(ioaddr, reg_addr) | value);
}

static void mdio_plus_minus(void __iomem *ioaddr, int reg_addr, int p, int m)
{
	int val;

	val = mdio_read(ioaddr, reg_addr);
	mdio_write(ioaddr, reg_addr, (val | p) & ~m);
}

static void rtl_mdio_write(struct net_device *dev, int phy_id, int location,
			   int val)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	mdio_write(ioaddr, location, val);
}

static int rtl_mdio_read(struct net_device *dev, int phy_id, int location)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	return mdio_read(ioaddr, location);
}

static void rtl_ephy_write(void __iomem *ioaddr, int reg_addr, int value)
{
	unsigned int i;

	RTL_W32(EPHYAR, EPHYAR_WRITE_CMD | (value & EPHYAR_DATA_MASK) |
		(reg_addr & EPHYAR_REG_MASK) << EPHYAR_REG_SHIFT);

	for (i = 0; i < 100; i++) {
		if (!(RTL_R32(EPHYAR) & EPHYAR_FLAG))
			break;
		udelay(10);
	}
}

static u16 rtl_ephy_read(void __iomem *ioaddr, int reg_addr)
{
	u16 value = 0xffff;
	unsigned int i;

	RTL_W32(EPHYAR, (reg_addr & EPHYAR_REG_MASK) << EPHYAR_REG_SHIFT);

	for (i = 0; i < 100; i++) {
		if (RTL_R32(EPHYAR) & EPHYAR_FLAG) {
			value = RTL_R32(EPHYAR) & EPHYAR_DATA_MASK;
			break;
		}
		udelay(10);
	}

	return value;
}

static void rtl_csi_write(void __iomem *ioaddr, int addr, int value)
{
	unsigned int i;

	RTL_W32(CSIDR, value);
	RTL_W32(CSIAR, CSIAR_WRITE_CMD | (addr & CSIAR_ADDR_MASK) |
		CSIAR_BYTE_ENABLE << CSIAR_BYTE_ENABLE_SHIFT);

	for (i = 0; i < 100; i++) {
		if (!(RTL_R32(CSIAR) & CSIAR_FLAG))
			break;
		udelay(10);
	}
}

static u32 rtl_csi_read(void __iomem *ioaddr, int addr)
{
	u32 value = ~0x00;
	unsigned int i;

	RTL_W32(CSIAR, (addr & CSIAR_ADDR_MASK) |
		CSIAR_BYTE_ENABLE << CSIAR_BYTE_ENABLE_SHIFT);

	for (i = 0; i < 100; i++) {
		if (RTL_R32(CSIAR) & CSIAR_FLAG) {
			value = RTL_R32(CSIDR);
			break;
		}
		udelay(10);
	}

	return value;
}

static u8 rtl8168d_efuse_read(void __iomem *ioaddr, int reg_addr)
{
	u8 value = 0xff;
	unsigned int i;

	RTL_W32(EFUSEAR, (reg_addr & EFUSEAR_REG_MASK) << EFUSEAR_REG_SHIFT);

	for (i = 0; i < 300; i++) {
		if (RTL_R32(EFUSEAR) & EFUSEAR_FLAG) {
			value = RTL_R32(EFUSEAR) & EFUSEAR_DATA_MASK;
			break;
		}
		udelay(100);
	}

	return value;
}

static void rtl8169_irq_mask_and_ack(void __iomem *ioaddr)
{
	RTL_W16(IntrMask, 0x0000);

	RTL_W16(IntrStatus, 0xffff);
}

static void rtl8169_asic_down(void __iomem *ioaddr)
{
	RTL_W8(ChipCmd, 0x00);
	rtl8169_irq_mask_and_ack(ioaddr);
	RTL_R16(CPlusCmd);
}

static unsigned int rtl8169_tbi_reset_pending(void __iomem *ioaddr)
{
	return RTL_R32(TBICSR) & TBIReset;
}

static unsigned int rtl8169_xmii_reset_pending(void __iomem *ioaddr)
{
	return mdio_read(ioaddr, MII_BMCR) & BMCR_RESET;
}

static unsigned int rtl8169_tbi_link_ok(void __iomem *ioaddr)
{
	return RTL_R32(TBICSR) & TBILinkOk;
}

static unsigned int rtl8169_xmii_link_ok(void __iomem *ioaddr)
{
	return RTL_R8(PHYstatus) & LinkStatus;
}

static void rtl8169_tbi_reset_enable(void __iomem *ioaddr)
{
	RTL_W32(TBICSR, RTL_R32(TBICSR) | TBIReset);
}

static void rtl8169_xmii_reset_enable(void __iomem *ioaddr)
{
	unsigned int val;

	val = mdio_read(ioaddr, MII_BMCR) | BMCR_RESET;
	mdio_write(ioaddr, MII_BMCR, val & 0xffff);
}

static void rtl8169_check_link_status(struct net_device *dev,
				      struct rtl8169_private *tp,
				      void __iomem *ioaddr)
{
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);
	if (tp->link_ok(ioaddr)) {
		netif_carrier_on(dev);
		if (netif_msg_ifup(tp))
			printk(KERN_INFO PFX "%s: link up\n", dev->name);
	} else {
		if (netif_msg_ifdown(tp))
			printk(KERN_INFO PFX "%s: link down\n", dev->name);
		netif_carrier_off(dev);
	}
	spin_unlock_irqrestore(&tp->lock, flags);
}

static void rtl8169_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	u8 options;

	wol->wolopts = 0;

#define WAKE_ANY (WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_BCAST | WAKE_MCAST)
	wol->supported = WAKE_ANY;

	spin_lock_irq(&tp->lock);

	options = RTL_R8(Config1);
	if (!(options & PMEnable))
		goto out_unlock;

	options = RTL_R8(Config3);
	if (options & LinkUp)
		wol->wolopts |= WAKE_PHY;
	if (options & MagicPacket)
		wol->wolopts |= WAKE_MAGIC;

	options = RTL_R8(Config5);
	if (options & UWF)
		wol->wolopts |= WAKE_UCAST;
	if (options & BWF)
		wol->wolopts |= WAKE_BCAST;
	if (options & MWF)
		wol->wolopts |= WAKE_MCAST;

out_unlock:
	spin_unlock_irq(&tp->lock);
}

static int rtl8169_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned int i;
	static struct {
		u32 opt;
		u16 reg;
		u8  mask;
	} cfg[] = {
		{ WAKE_ANY,   Config1, PMEnable },
		{ WAKE_PHY,   Config3, LinkUp },
		{ WAKE_MAGIC, Config3, MagicPacket },
		{ WAKE_UCAST, Config5, UWF },
		{ WAKE_BCAST, Config5, BWF },
		{ WAKE_MCAST, Config5, MWF },
		{ WAKE_ANY,   Config5, LanWake }
	};

	spin_lock_irq(&tp->lock);

	RTL_W8(Cfg9346, Cfg9346_Unlock);

	for (i = 0; i < ARRAY_SIZE(cfg); i++) {
		u8 options = RTL_R8(cfg[i].reg) & ~cfg[i].mask;
		if (wol->wolopts & cfg[i].opt)
			options |= cfg[i].mask;
		RTL_W8(cfg[i].reg, options);
	}

	RTL_W8(Cfg9346, Cfg9346_Lock);

	if (wol->wolopts)
		tp->features |= RTL_FEATURE_WOL;
	else
		tp->features &= ~RTL_FEATURE_WOL;
	device_set_wakeup_enable(&tp->pci_dev->dev, wol->wolopts);

	spin_unlock_irq(&tp->lock);

	return 0;
}

static void rtl8169_get_drvinfo(struct net_device *dev,
				struct ethtool_drvinfo *info)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	strcpy(info->driver, MODULENAME);
	strcpy(info->version, RTL8169_VERSION);
	strcpy(info->bus_info, pci_name(tp->pci_dev));
}

static int rtl8169_get_regs_len(struct net_device *dev)
{
	return R8169_REGS_SIZE;
}

static int rtl8169_set_speed_tbi(struct net_device *dev,
				 u8 autoneg, u16 speed, u8 duplex)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	int ret = 0;
	u32 reg;

	reg = RTL_R32(TBICSR);
	if ((autoneg == AUTONEG_DISABLE) && (speed == SPEED_1000) &&
	    (duplex == DUPLEX_FULL)) {
		RTL_W32(TBICSR, reg & ~(TBINwEnable | TBINwRestart));
	} else if (autoneg == AUTONEG_ENABLE)
		RTL_W32(TBICSR, reg | TBINwEnable | TBINwRestart);
	else {
		if (netif_msg_link(tp)) {
			printk(KERN_WARNING "%s: "
			       "incorrect speed setting refused in TBI mode\n",
			       dev->name);
		}
		ret = -EOPNOTSUPP;
	}

	return ret;
}

static int rtl8169_set_speed_xmii(struct net_device *dev,
				  u8 autoneg, u16 speed, u8 duplex)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	int giga_ctrl, bmcr;

	if (autoneg == AUTONEG_ENABLE) {
		int auto_nego;

		auto_nego = mdio_read(ioaddr, MII_ADVERTISE);
		auto_nego |= (ADVERTISE_10HALF | ADVERTISE_10FULL |
			      ADVERTISE_100HALF | ADVERTISE_100FULL);
		auto_nego |= ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;

		giga_ctrl = mdio_read(ioaddr, MII_CTRL1000);
		giga_ctrl &= ~(ADVERTISE_1000FULL | ADVERTISE_1000HALF);

		/* The 8100e/8101e/8102e do Fast Ethernet only. */
		if ((tp->mac_version != RTL_GIGA_MAC_VER_07) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_08) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_09) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_10) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_13) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_14) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_15) &&
		    (tp->mac_version != RTL_GIGA_MAC_VER_16)) {
			giga_ctrl |= ADVERTISE_1000FULL | ADVERTISE_1000HALF;
		} else if (netif_msg_link(tp)) {
			printk(KERN_INFO "%s: PHY does not support 1000Mbps.\n",
			       dev->name);
		}

		bmcr = BMCR_ANENABLE | BMCR_ANRESTART;

		if ((tp->mac_version == RTL_GIGA_MAC_VER_11) ||
		    (tp->mac_version == RTL_GIGA_MAC_VER_12) ||
		    (tp->mac_version >= RTL_GIGA_MAC_VER_17)) {
			/*
			 * Wake up the PHY.
			 * Vendor specific (0x1f) and reserved (0x0e) MII
			 * registers.
			 */
			mdio_write(ioaddr, 0x1f, 0x0000);
			mdio_write(ioaddr, 0x0e, 0x0000);
		}

		mdio_write(ioaddr, MII_ADVERTISE, auto_nego);
		mdio_write(ioaddr, MII_CTRL1000, giga_ctrl);
	} else {
		giga_ctrl = 0;

		if (speed == SPEED_10)
			bmcr = 0;
		else if (speed == SPEED_100)
			bmcr = BMCR_SPEED100;
		else
			return -EINVAL;

		if (duplex == DUPLEX_FULL)
			bmcr |= BMCR_FULLDPLX;

		mdio_write(ioaddr, 0x1f, 0x0000);
	}

	tp->phy_1000_ctrl_reg = giga_ctrl;

	mdio_write(ioaddr, MII_BMCR, bmcr);

	if ((tp->mac_version == RTL_GIGA_MAC_VER_02) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_03)) {
		if ((speed == SPEED_100) && (autoneg != AUTONEG_ENABLE)) {
			mdio_write(ioaddr, 0x17, 0x2138);
			mdio_write(ioaddr, 0x0e, 0x0260);
		} else {
			mdio_write(ioaddr, 0x17, 0x2108);
			mdio_write(ioaddr, 0x0e, 0x0000);
		}
	}

	return 0;
}

static int rtl8169_set_speed(struct net_device *dev,
			     u8 autoneg, u16 speed, u8 duplex)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	int ret;

	ret = tp->set_speed(dev, autoneg, speed, duplex);

	if (netif_running(dev) && (tp->phy_1000_ctrl_reg & ADVERTISE_1000FULL))
		mod_timer(&tp->timer, jiffies + RTL8169_PHY_TIMEOUT);

	return ret;
}

static int rtl8169_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&tp->lock, flags);
	ret = rtl8169_set_speed(dev, cmd->autoneg, cmd->speed, cmd->duplex);
	spin_unlock_irqrestore(&tp->lock, flags);

	return ret;
}

static u32 rtl8169_get_rx_csum(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	return tp->cp_cmd & RxChkSum;
}

static int rtl8169_set_rx_csum(struct net_device *dev, u32 data)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);

	if (data)
		tp->cp_cmd |= RxChkSum;
	else
		tp->cp_cmd &= ~RxChkSum;

	RTL_W16(CPlusCmd, tp->cp_cmd);
	RTL_R16(CPlusCmd);

	spin_unlock_irqrestore(&tp->lock, flags);

	return 0;
}

#ifdef CONFIG_R8169_VLAN

static inline u32 rtl8169_tx_vlan_tag(struct rtl8169_private *tp,
				      struct sk_buff *skb)
{
	return (tp->vlgrp && vlan_tx_tag_present(skb)) ?
		TxVlanTag | swab16(vlan_tx_tag_get(skb)) : 0x00;
}

static void rtl8169_vlan_rx_register(struct net_device *dev,
				     struct vlan_group *grp)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);
	tp->vlgrp = grp;
	/*
	 * Do not disable RxVlan on 8110SCd.
	 */
	if (tp->vlgrp || (tp->mac_version == RTL_GIGA_MAC_VER_05))
		tp->cp_cmd |= RxVlan;
	else
		tp->cp_cmd &= ~RxVlan;
	RTL_W16(CPlusCmd, tp->cp_cmd);
	RTL_R16(CPlusCmd);
	spin_unlock_irqrestore(&tp->lock, flags);
}

static int rtl8169_rx_vlan_skb(struct rtl8169_private *tp, struct RxDesc *desc,
			       struct sk_buff *skb)
{
	u32 opts2 = le32_to_cpu(desc->opts2);
	struct vlan_group *vlgrp = tp->vlgrp;
	int ret;

	if (vlgrp && (opts2 & RxVlanTag)) {
		vlan_hwaccel_receive_skb(skb, vlgrp, swab16(opts2 & 0xffff));
		ret = 0;
	} else
		ret = -1;
	desc->opts2 = 0;
	return ret;
}

#else /* !CONFIG_R8169_VLAN */

static inline u32 rtl8169_tx_vlan_tag(struct rtl8169_private *tp,
				      struct sk_buff *skb)
{
	return 0;
}

static int rtl8169_rx_vlan_skb(struct rtl8169_private *tp, struct RxDesc *desc,
			       struct sk_buff *skb)
{
	return -1;
}

#endif

static int rtl8169_gset_tbi(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	u32 status;

	cmd->supported =
		SUPPORTED_1000baseT_Full | SUPPORTED_Autoneg | SUPPORTED_FIBRE;
	cmd->port = PORT_FIBRE;
	cmd->transceiver = XCVR_INTERNAL;

	status = RTL_R32(TBICSR);
	cmd->advertising = (status & TBINwEnable) ?  ADVERTISED_Autoneg : 0;
	cmd->autoneg = !!(status & TBINwEnable);

	cmd->speed = SPEED_1000;
	cmd->duplex = DUPLEX_FULL; /* Always set */

	return 0;
}

static int rtl8169_gset_xmii(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	return mii_ethtool_gset(&tp->mii, cmd);
}

static int rtl8169_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&tp->lock, flags);

	rc = tp->get_settings(dev, cmd);

	spin_unlock_irqrestore(&tp->lock, flags);
	return rc;
}

static void rtl8169_get_regs(struct net_device *dev, struct ethtool_regs *regs,
			     void *p)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	unsigned long flags;

	if (regs->len > R8169_REGS_SIZE)
		regs->len = R8169_REGS_SIZE;

	spin_lock_irqsave(&tp->lock, flags);
	memcpy_fromio(p, tp->mmio_addr, regs->len);
	spin_unlock_irqrestore(&tp->lock, flags);
}

static u32 rtl8169_get_msglevel(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	return tp->msg_enable;
}

static void rtl8169_set_msglevel(struct net_device *dev, u32 value)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	tp->msg_enable = value;
}

static const char rtl8169_gstrings[][ETH_GSTRING_LEN] = {
	"tx_packets",
	"rx_packets",
	"tx_errors",
	"rx_errors",
	"rx_missed",
	"align_errors",
	"tx_single_collisions",
	"tx_multi_collisions",
	"unicast",
	"broadcast",
	"multicast",
	"tx_aborted",
	"tx_underrun",
};

static int rtl8169_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return ARRAY_SIZE(rtl8169_gstrings);
	default:
		return -EOPNOTSUPP;
	}
}

static void rtl8169_update_counters(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	struct rtl8169_counters *counters;
	dma_addr_t paddr;
	u32 cmd;
	int wait = 1000;

	/*
	 * Some chips are unable to dump tally counters when the receiver
	 * is disabled.
	 */
	if ((RTL_R8(ChipCmd) & CmdRxEnb) == 0)
		return;

	counters = pci_alloc_consistent(tp->pci_dev, sizeof(*counters), &paddr);
	if (!counters)
		return;

	RTL_W32(CounterAddrHigh, (u64)paddr >> 32);
	cmd = (u64)paddr & DMA_BIT_MASK(32);
	RTL_W32(CounterAddrLow, cmd);
	RTL_W32(CounterAddrLow, cmd | CounterDump);

	while (wait--) {
		if ((RTL_R32(CounterAddrLow) & CounterDump) == 0) {
			/* copy updated counters */
			memcpy(&tp->counters, counters, sizeof(*counters));
			break;
		}
		udelay(10);
	}

	RTL_W32(CounterAddrLow, 0);
	RTL_W32(CounterAddrHigh, 0);

	pci_free_consistent(tp->pci_dev, sizeof(*counters), counters, paddr);
}

static void rtl8169_get_ethtool_stats(struct net_device *dev,
				      struct ethtool_stats *stats, u64 *data)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	ASSERT_RTNL();

	rtl8169_update_counters(dev);

	data[0] = le64_to_cpu(tp->counters.tx_packets);
	data[1] = le64_to_cpu(tp->counters.rx_packets);
	data[2] = le64_to_cpu(tp->counters.tx_errors);
	data[3] = le32_to_cpu(tp->counters.rx_errors);
	data[4] = le16_to_cpu(tp->counters.rx_missed);
	data[5] = le16_to_cpu(tp->counters.align_errors);
	data[6] = le32_to_cpu(tp->counters.tx_one_collision);
	data[7] = le32_to_cpu(tp->counters.tx_multi_collision);
	data[8] = le64_to_cpu(tp->counters.rx_unicast);
	data[9] = le64_to_cpu(tp->counters.rx_broadcast);
	data[10] = le32_to_cpu(tp->counters.rx_multicast);
	data[11] = le16_to_cpu(tp->counters.tx_aborted);
	data[12] = le16_to_cpu(tp->counters.tx_underun);
}

static void rtl8169_get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	switch(stringset) {
	case ETH_SS_STATS:
		memcpy(data, *rtl8169_gstrings, sizeof(rtl8169_gstrings));
		break;
	}
}

static const struct ethtool_ops rtl8169_ethtool_ops = {
	.get_drvinfo		= rtl8169_get_drvinfo,
	.get_regs_len		= rtl8169_get_regs_len,
	.get_link		= ethtool_op_get_link,
	.get_settings		= rtl8169_get_settings,
	.set_settings		= rtl8169_set_settings,
	.get_msglevel		= rtl8169_get_msglevel,
	.set_msglevel		= rtl8169_set_msglevel,
	.get_rx_csum		= rtl8169_get_rx_csum,
	.set_rx_csum		= rtl8169_set_rx_csum,
	.set_tx_csum		= ethtool_op_set_tx_csum,
	.set_sg			= ethtool_op_set_sg,
	.set_tso		= ethtool_op_set_tso,
	.get_regs		= rtl8169_get_regs,
	.get_wol		= rtl8169_get_wol,
	.set_wol		= rtl8169_set_wol,
	.get_strings		= rtl8169_get_strings,
	.get_sset_count		= rtl8169_get_sset_count,
	.get_ethtool_stats	= rtl8169_get_ethtool_stats,
};

static void rtl8169_get_mac_version(struct rtl8169_private *tp,
				    void __iomem *ioaddr)
{
	/*
	 * The driver currently handles the 8168Bf and the 8168Be identically
	 * but they can be identified more specifically through the test below
	 * if needed:
	 *
	 * (RTL_R32(TxConfig) & 0x700000) == 0x500000 ? 8168Bf : 8168Be
	 *
	 * Same thing for the 8101Eb and the 8101Ec:
	 *
	 * (RTL_R32(TxConfig) & 0x700000) == 0x200000 ? 8101Eb : 8101Ec
	 */
	const struct {
		u32 mask;
		u32 val;
		int mac_version;
	} mac_info[] = {
		/* 8168D family. */
		{ 0x7cf00000, 0x28300000,	RTL_GIGA_MAC_VER_26 },
		{ 0x7cf00000, 0x28100000,	RTL_GIGA_MAC_VER_25 },
		{ 0x7c800000, 0x28800000,	RTL_GIGA_MAC_VER_27 },
		{ 0x7c800000, 0x28000000,	RTL_GIGA_MAC_VER_26 },

		/* 8168C family. */
		{ 0x7cf00000, 0x3ca00000,	RTL_GIGA_MAC_VER_24 },
		{ 0x7cf00000, 0x3c900000,	RTL_GIGA_MAC_VER_23 },
		{ 0x7cf00000, 0x3c800000,	RTL_GIGA_MAC_VER_18 },
		{ 0x7c800000, 0x3c800000,	RTL_GIGA_MAC_VER_24 },
		{ 0x7cf00000, 0x3c000000,	RTL_GIGA_MAC_VER_19 },
		{ 0x7cf00000, 0x3c200000,	RTL_GIGA_MAC_VER_20 },
		{ 0x7cf00000, 0x3c300000,	RTL_GIGA_MAC_VER_21 },
		{ 0x7cf00000, 0x3c400000,	RTL_GIGA_MAC_VER_22 },
		{ 0x7c800000, 0x3c000000,	RTL_GIGA_MAC_VER_22 },

		/* 8168B family. */
		{ 0x7cf00000, 0x38000000,	RTL_GIGA_MAC_VER_12 },
		{ 0x7cf00000, 0x38500000,	RTL_GIGA_MAC_VER_17 },
		{ 0x7c800000, 0x38000000,	RTL_GIGA_MAC_VER_17 },
		{ 0x7c800000, 0x30000000,	RTL_GIGA_MAC_VER_11 },

		/* 8101 family. */
		{ 0x7cf00000, 0x34a00000,	RTL_GIGA_MAC_VER_09 },
		{ 0x7cf00000, 0x24a00000,	RTL_GIGA_MAC_VER_09 },
		{ 0x7cf00000, 0x34900000,	RTL_GIGA_MAC_VER_08 },
		{ 0x7cf00000, 0x24900000,	RTL_GIGA_MAC_VER_08 },
		{ 0x7cf00000, 0x34800000,	RTL_GIGA_MAC_VER_07 },
		{ 0x7cf00000, 0x24800000,	RTL_GIGA_MAC_VER_07 },
		{ 0x7cf00000, 0x34000000,	RTL_GIGA_MAC_VER_13 },
		{ 0x7cf00000, 0x34300000,	RTL_GIGA_MAC_VER_10 },
		{ 0x7cf00000, 0x34200000,	RTL_GIGA_MAC_VER_16 },
		{ 0x7c800000, 0x34800000,	RTL_GIGA_MAC_VER_09 },
		{ 0x7c800000, 0x24800000,	RTL_GIGA_MAC_VER_09 },
		{ 0x7c800000, 0x34000000,	RTL_GIGA_MAC_VER_16 },
		/* FIXME: where did these entries come from ? -- FR */
		{ 0xfc800000, 0x38800000,	RTL_GIGA_MAC_VER_15 },
		{ 0xfc800000, 0x30800000,	RTL_GIGA_MAC_VER_14 },

		/* 8110 family. */
		{ 0xfc800000, 0x98000000,	RTL_GIGA_MAC_VER_06 },
		{ 0xfc800000, 0x18000000,	RTL_GIGA_MAC_VER_05 },
		{ 0xfc800000, 0x10000000,	RTL_GIGA_MAC_VER_04 },
		{ 0xfc800000, 0x04000000,	RTL_GIGA_MAC_VER_03 },
		{ 0xfc800000, 0x00800000,	RTL_GIGA_MAC_VER_02 },
		{ 0xfc800000, 0x00000000,	RTL_GIGA_MAC_VER_01 },

		/* Catch-all */
		{ 0x00000000, 0x00000000,	RTL_GIGA_MAC_NONE   }
	}, *p = mac_info;
	u32 reg;

	reg = RTL_R32(TxConfig);
	while ((reg & p->mask) != p->val)
		p++;
	tp->mac_version = p->mac_version;
}

static void rtl8169_print_mac_version(struct rtl8169_private *tp)
{
	dprintk("mac_version = 0x%02x\n", tp->mac_version);
}

struct phy_reg {
	u16 reg;
	u16 val;
};

static void rtl_phy_write(void __iomem *ioaddr, struct phy_reg *regs, int len)
{
	while (len-- > 0) {
		mdio_write(ioaddr, regs->reg, regs->val);
		regs++;
	}
}

static void rtl8169s_hw_phy_config(void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x06, 0x006e },
		{ 0x08, 0x0708 },
		{ 0x15, 0x4000 },
		{ 0x18, 0x65c7 },

		{ 0x1f, 0x0001 },
		{ 0x03, 0x00a1 },
		{ 0x02, 0x0008 },
		{ 0x01, 0x0120 },
		{ 0x00, 0x1000 },
		{ 0x04, 0x0800 },
		{ 0x04, 0x0000 },

		{ 0x03, 0xff41 },
		{ 0x02, 0xdf60 },
		{ 0x01, 0x0140 },
		{ 0x00, 0x0077 },
		{ 0x04, 0x7800 },
		{ 0x04, 0x7000 },

		{ 0x03, 0x802f },
		{ 0x02, 0x4f02 },
		{ 0x01, 0x0409 },
		{ 0x00, 0xf0f9 },
		{ 0x04, 0x9800 },
		{ 0x04, 0x9000 },

		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0xff95 },
		{ 0x00, 0xba00 },
		{ 0x04, 0xa800 },
		{ 0x04, 0xa000 },

		{ 0x03, 0xff41 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0x0140 },
		{ 0x00, 0x00bb },
		{ 0x04, 0xb800 },
		{ 0x04, 0xb000 },

		{ 0x03, 0xdf41 },
		{ 0x02, 0xdc60 },
		{ 0x01, 0x6340 },
		{ 0x00, 0x007d },
		{ 0x04, 0xd800 },
		{ 0x04, 0xd000 },

		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0x100a },
		{ 0x00, 0xa0ff },
		{ 0x04, 0xf800 },
		{ 0x04, 0xf000 },

		{ 0x1f, 0x0000 },
		{ 0x0b, 0x0000 },
		{ 0x00, 0x9200 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8169sb_hw_phy_config(void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0002 },
		{ 0x01, 0x90d0 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8169scd_hw_phy_config_quirk(struct rtl8169_private *tp,
					   void __iomem *ioaddr)
{
	struct pci_dev *pdev = tp->pci_dev;
	u16 vendor_id, device_id;

	pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, &vendor_id);
	pci_read_config_word(pdev, PCI_SUBSYSTEM_ID, &device_id);

	if ((vendor_id != PCI_VENDOR_ID_GIGABYTE) || (device_id != 0xe000))
		return;

	mdio_write(ioaddr, 0x1f, 0x0001);
	mdio_write(ioaddr, 0x10, 0xf01b);
	mdio_write(ioaddr, 0x1f, 0x0000);
}

static void rtl8169scd_hw_phy_config(struct rtl8169_private *tp,
				     void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x04, 0x0000 },
		{ 0x03, 0x00a1 },
		{ 0x02, 0x0008 },
		{ 0x01, 0x0120 },
		{ 0x00, 0x1000 },
		{ 0x04, 0x0800 },
		{ 0x04, 0x9000 },
		{ 0x03, 0x802f },
		{ 0x02, 0x4f02 },
		{ 0x01, 0x0409 },
		{ 0x00, 0xf099 },
		{ 0x04, 0x9800 },
		{ 0x04, 0xa000 },
		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0xff95 },
		{ 0x00, 0xba00 },
		{ 0x04, 0xa800 },
		{ 0x04, 0xf000 },
		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0x101a },
		{ 0x00, 0xa0ff },
		{ 0x04, 0xf800 },
		{ 0x04, 0x0000 },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0001 },
		{ 0x10, 0xf41b },
		{ 0x14, 0xfb54 },
		{ 0x18, 0xf5c7 },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0001 },
		{ 0x17, 0x0cc0 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

	rtl8169scd_hw_phy_config_quirk(tp, ioaddr);
}

static void rtl8169sce_hw_phy_config(void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x04, 0x0000 },
		{ 0x03, 0x00a1 },
		{ 0x02, 0x0008 },
		{ 0x01, 0x0120 },
		{ 0x00, 0x1000 },
		{ 0x04, 0x0800 },
		{ 0x04, 0x9000 },
		{ 0x03, 0x802f },
		{ 0x02, 0x4f02 },
		{ 0x01, 0x0409 },
		{ 0x00, 0xf099 },
		{ 0x04, 0x9800 },
		{ 0x04, 0xa000 },
		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0xff95 },
		{ 0x00, 0xba00 },
		{ 0x04, 0xa800 },
		{ 0x04, 0xf000 },
		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0x101a },
		{ 0x00, 0xa0ff },
		{ 0x04, 0xf800 },
		{ 0x04, 0x0000 },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0001 },
		{ 0x0b, 0x8480 },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0001 },
		{ 0x18, 0x67c7 },
		{ 0x04, 0x2000 },
		{ 0x03, 0x002f },
		{ 0x02, 0x4360 },
		{ 0x01, 0x0109 },
		{ 0x00, 0x3022 },
		{ 0x04, 0x2800 },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0001 },
		{ 0x17, 0x0cc0 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168bb_hw_phy_config(void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x10, 0xf41b },
		{ 0x1f, 0x0000 }
	};

	mdio_write(ioaddr, 0x1f, 0x0001);
	mdio_patch(ioaddr, 0x16, 1 << 0);

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168bef_hw_phy_config(void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x10, 0xf41b },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168cp_1_hw_phy_config(void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0000 },
		{ 0x1d, 0x0f00 },
		{ 0x1f, 0x0002 },
		{ 0x0c, 0x1ec8 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168cp_2_hw_phy_config(void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x1d, 0x3d98 },
		{ 0x1f, 0x0000 }
	};

	mdio_write(ioaddr, 0x1f, 0x0000);
	mdio_patch(ioaddr, 0x14, 1 << 5);
	mdio_patch(ioaddr, 0x0d, 1 << 5);

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168c_1_hw_phy_config(void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x12, 0x2300 },
		{ 0x1f, 0x0002 },
		{ 0x00, 0x88d4 },
		{ 0x01, 0x82b1 },
		{ 0x03, 0x7002 },
		{ 0x08, 0x9e30 },
		{ 0x09, 0x01f0 },
		{ 0x0a, 0x5500 },
		{ 0x0c, 0x00c8 },
		{ 0x1f, 0x0003 },
		{ 0x12, 0xc096 },
		{ 0x16, 0x000a },
		{ 0x1f, 0x0000 },
		{ 0x1f, 0x0000 },
		{ 0x09, 0x2000 },
		{ 0x09, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

	mdio_patch(ioaddr, 0x14, 1 << 5);
	mdio_patch(ioaddr, 0x0d, 1 << 5);
	mdio_write(ioaddr, 0x1f, 0x0000);
}

static void rtl8168c_2_hw_phy_config(void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x12, 0x2300 },
		{ 0x03, 0x802f },
		{ 0x02, 0x4f02 },
		{ 0x01, 0x0409 },
		{ 0x00, 0xf099 },
		{ 0x04, 0x9800 },
		{ 0x04, 0x9000 },
		{ 0x1d, 0x3d98 },
		{ 0x1f, 0x0002 },
		{ 0x0c, 0x7eb8 },
		{ 0x06, 0x0761 },
		{ 0x1f, 0x0003 },
		{ 0x16, 0x0f0a },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

	mdio_patch(ioaddr, 0x16, 1 << 0);
	mdio_patch(ioaddr, 0x14, 1 << 5);
	mdio_patch(ioaddr, 0x0d, 1 << 5);
	mdio_write(ioaddr, 0x1f, 0x0000);
}

static void rtl8168c_3_hw_phy_config(void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x12, 0x2300 },
		{ 0x1d, 0x3d98 },
		{ 0x1f, 0x0002 },
		{ 0x0c, 0x7eb8 },
		{ 0x06, 0x5461 },
		{ 0x1f, 0x0003 },
		{ 0x16, 0x0f0a },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

	mdio_patch(ioaddr, 0x16, 1 << 0);
	mdio_patch(ioaddr, 0x14, 1 << 5);
	mdio_patch(ioaddr, 0x0d, 1 << 5);
	mdio_write(ioaddr, 0x1f, 0x0000);
}

static void rtl8168c_4_hw_phy_config(void __iomem *ioaddr)
{
	rtl8168c_3_hw_phy_config(ioaddr);
}

static void rtl8168d_1_hw_phy_config(void __iomem *ioaddr)
{
	static struct phy_reg phy_reg_init_0[] = {
		{ 0x1f, 0x0001 },
		{ 0x06, 0x4064 },
		{ 0x07, 0x2863 },
		{ 0x08, 0x059c },
		{ 0x09, 0x26b4 },
		{ 0x0a, 0x6a19 },
		{ 0x0b, 0xdcc8 },
		{ 0x10, 0xf06d },
		{ 0x14, 0x7f68 },
		{ 0x18, 0x7fd9 },
		{ 0x1c, 0xf0ff },
		{ 0x1d, 0x3d9c },
		{ 0x1f, 0x0003 },
		{ 0x12, 0xf49f },
		{ 0x13, 0x070b },
		{ 0x1a, 0x05ad },
		{ 0x14, 0x94c0 }
	};
	static struct phy_reg phy_reg_init_1[] = {
		{ 0x1f, 0x0002 },
		{ 0x06, 0x5561 },
		{ 0x1f, 0x0005 },
		{ 0x05, 0x8332 },
		{ 0x06, 0x5561 }
	};
	static struct phy_reg phy_reg_init_2[] = {
		{ 0x1f, 0x0005 },
		{ 0x05, 0xffc2 },
		{ 0x1f, 0x0005 },
		{ 0x05, 0x8000 },
		{ 0x06, 0xf8f9 },
		{ 0x06, 0xfaef },
		{ 0x06, 0x59ee },
		{ 0x06, 0xf8ea },
		{ 0x06, 0x00ee },
		{ 0x06, 0xf8eb },
		{ 0x06, 0x00e0 },
		{ 0x06, 0xf87c },
		{ 0x06, 0xe1f8 },
		{ 0x06, 0x7d59 },
		{ 0x06, 0x0fef },
		{ 0x06, 0x0139 },
		{ 0x06, 0x029e },
		{ 0x06, 0x06ef },
		{ 0x06, 0x1039 },
		{ 0x06, 0x089f },
		{ 0x06, 0x2aee },
		{ 0x06, 0xf8ea },
		{ 0x06, 0x00ee },
		{ 0x06, 0xf8eb },
		{ 0x06, 0x01e0 },
		{ 0x06, 0xf87c },
		{ 0x06, 0xe1f8 },
		{ 0x06, 0x7d58 },
		{ 0x06, 0x409e },
		{ 0x06, 0x0f39 },
		{ 0x06, 0x46aa },
		{ 0x06, 0x0bbf },
		{ 0x06, 0x8290 },
		{ 0x06, 0xd682 },
		{ 0x06, 0x9802 },
		{ 0x06, 0x014f },
		{ 0x06, 0xae09 },
		{ 0x06, 0xbf82 },
		{ 0x06, 0x98d6 },
		{ 0x06, 0x82a0 },
		{ 0x06, 0x0201 },
		{ 0x06, 0x4fef },
		{ 0x06, 0x95fe },
		{ 0x06, 0xfdfc },
		{ 0x06, 0x05f8 },
		{ 0x06, 0xf9fa },
		{ 0x06, 0xeef8 },
		{ 0x06, 0xea00 },
		{ 0x06, 0xeef8 },
		{ 0x06, 0xeb00 },
		{ 0x06, 0xe2f8 },
		{ 0x06, 0x7ce3 },
		{ 0x06, 0xf87d },
		{ 0x06, 0xa511 },
		{ 0x06, 0x1112 },
		{ 0x06, 0xd240 },
		{ 0x06, 0xd644 },
		{ 0x06, 0x4402 },
		{ 0x06, 0x8217 },
		{ 0x06, 0xd2a0 },
		{ 0x06, 0xd6aa },
		{ 0x06, 0xaa02 },
		{ 0x06, 0x8217 },
		{ 0x06, 0xae0f },
		{ 0x06, 0xa544 },
		{ 0x06, 0x4402 },
		{ 0x06, 0xae4d },
		{ 0x06, 0xa5aa },
		{ 0x06, 0xaa02 },
		{ 0x06, 0xae47 },
		{ 0x06, 0xaf82 },
		{ 0x06, 0x13ee },
		{ 0x06, 0x834e },
		{ 0x06, 0x00ee },
		{ 0x06, 0x834d },
		{ 0x06, 0x0fee },
		{ 0x06, 0x834c },
		{ 0x06, 0x0fee },
		{ 0x06, 0x834f },
		{ 0x06, 0x00ee },
		{ 0x06, 0x8351 },
		{ 0x06, 0x00ee },
		{ 0x06, 0x834a },
		{ 0x06, 0xffee },
		{ 0x06, 0x834b },
		{ 0x06, 0xffe0 },
		{ 0x06, 0x8330 },
		{ 0x06, 0xe183 },
		{ 0x06, 0x3158 },
		{ 0x06, 0xfee4 },
		{ 0x06, 0xf88a },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x8be0 },
		{ 0x06, 0x8332 },
		{ 0x06, 0xe183 },
		{ 0x06, 0x3359 },
		{ 0x06, 0x0fe2 },
		{ 0x06, 0x834d },
		{ 0x06, 0x0c24 },
		{ 0x06, 0x5af0 },
		{ 0x06, 0x1e12 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x8ce5 },
		{ 0x06, 0xf88d },
		{ 0x06, 0xaf82 },
		{ 0x06, 0x13e0 },
		{ 0x06, 0x834f },
		{ 0x06, 0x10e4 },
		{ 0x06, 0x834f },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x009f },
		{ 0x06, 0x0ae0 },
		{ 0x06, 0x834f },
		{ 0x06, 0xa010 },
		{ 0x06, 0xa5ee },
		{ 0x06, 0x834e },
		{ 0x06, 0x01e0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7805 },
		{ 0x06, 0x9e9a },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x049e },
		{ 0x06, 0x10e0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7803 },
		{ 0x06, 0x9e0f },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x019e },
		{ 0x06, 0x05ae },
		{ 0x06, 0x0caf },
		{ 0x06, 0x81f8 },
		{ 0x06, 0xaf81 },
		{ 0x06, 0xa3af },
		{ 0x06, 0x81dc },
		{ 0x06, 0xaf82 },
		{ 0x06, 0x13ee },
		{ 0x06, 0x8348 },
		{ 0x06, 0x00ee },
		{ 0x06, 0x8349 },
		{ 0x06, 0x00e0 },
		{ 0x06, 0x8351 },
		{ 0x06, 0x10e4 },
		{ 0x06, 0x8351 },
		{ 0x06, 0x5801 },
		{ 0x06, 0x9fea },
		{ 0x06, 0xd000 },
		{ 0x06, 0xd180 },
		{ 0x06, 0x1f66 },
		{ 0x06, 0xe2f8 },
		{ 0x06, 0xeae3 },
		{ 0x06, 0xf8eb },
		{ 0x06, 0x5af8 },
		{ 0x06, 0x1e20 },
		{ 0x06, 0xe6f8 },
		{ 0x06, 0xeae5 },
		{ 0x06, 0xf8eb },
		{ 0x06, 0xd302 },
		{ 0x06, 0xb3fe },
		{ 0x06, 0xe2f8 },
		{ 0x06, 0x7cef },
		{ 0x06, 0x325b },
		{ 0x06, 0x80e3 },
		{ 0x06, 0xf87d },
		{ 0x06, 0x9e03 },
		{ 0x06, 0x7dff },
		{ 0x06, 0xff0d },
		{ 0x06, 0x581c },
		{ 0x06, 0x551a },
		{ 0x06, 0x6511 },
		{ 0x06, 0xa190 },
		{ 0x06, 0xd3e2 },
		{ 0x06, 0x8348 },
		{ 0x06, 0xe383 },
		{ 0x06, 0x491b },
		{ 0x06, 0x56ab },
		{ 0x06, 0x08ef },
		{ 0x06, 0x56e6 },
		{ 0x06, 0x8348 },
		{ 0x06, 0xe783 },
		{ 0x06, 0x4910 },
		{ 0x06, 0xd180 },
		{ 0x06, 0x1f66 },
		{ 0x06, 0xa004 },
		{ 0x06, 0xb9e2 },
		{ 0x06, 0x8348 },
		{ 0x06, 0xe383 },
		{ 0x06, 0x49ef },
		{ 0x06, 0x65e2 },
		{ 0x06, 0x834a },
		{ 0x06, 0xe383 },
		{ 0x06, 0x4b1b },
		{ 0x06, 0x56aa },
		{ 0x06, 0x0eef },
		{ 0x06, 0x56e6 },
		{ 0x06, 0x834a },
		{ 0x06, 0xe783 },
		{ 0x06, 0x4be2 },
		{ 0x06, 0x834d },
		{ 0x06, 0xe683 },
		{ 0x06, 0x4ce0 },
		{ 0x06, 0x834d },
		{ 0x06, 0xa000 },
		{ 0x06, 0x0caf },
		{ 0x06, 0x81dc },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4d10 },
		{ 0x06, 0xe483 },
		{ 0x06, 0x4dae },
		{ 0x06, 0x0480 },
		{ 0x06, 0xe483 },
		{ 0x06, 0x4de0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7803 },
		{ 0x06, 0x9e0b },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x049e },
		{ 0x06, 0x04ee },
		{ 0x06, 0x834e },
		{ 0x06, 0x02e0 },
		{ 0x06, 0x8332 },
		{ 0x06, 0xe183 },
		{ 0x06, 0x3359 },
		{ 0x06, 0x0fe2 },
		{ 0x06, 0x834d },
		{ 0x06, 0x0c24 },
		{ 0x06, 0x5af0 },
		{ 0x06, 0x1e12 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x8ce5 },
		{ 0x06, 0xf88d },
		{ 0x06, 0xe083 },
		{ 0x06, 0x30e1 },
		{ 0x06, 0x8331 },
		{ 0x06, 0x6801 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x8ae5 },
		{ 0x06, 0xf88b },
		{ 0x06, 0xae37 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4e03 },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4ce1 },
		{ 0x06, 0x834d },
		{ 0x06, 0x1b01 },
		{ 0x06, 0x9e04 },
		{ 0x06, 0xaaa1 },
		{ 0x06, 0xaea8 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4e04 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4f00 },
		{ 0x06, 0xaeab },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4f78 },
		{ 0x06, 0x039f },
		{ 0x06, 0x14ee },
		{ 0x06, 0x834e },
		{ 0x06, 0x05d2 },
		{ 0x06, 0x40d6 },
		{ 0x06, 0x5554 },
		{ 0x06, 0x0282 },
		{ 0x06, 0x17d2 },
		{ 0x06, 0xa0d6 },
		{ 0x06, 0xba00 },
		{ 0x06, 0x0282 },
		{ 0x06, 0x17fe },
		{ 0x06, 0xfdfc },
		{ 0x06, 0x05f8 },
		{ 0x06, 0xe0f8 },
		{ 0x06, 0x60e1 },
		{ 0x06, 0xf861 },
		{ 0x06, 0x6802 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x60e5 },
		{ 0x06, 0xf861 },
		{ 0x06, 0xe0f8 },
		{ 0x06, 0x48e1 },
		{ 0x06, 0xf849 },
		{ 0x06, 0x580f },
		{ 0x06, 0x1e02 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x48e5 },
		{ 0x06, 0xf849 },
		{ 0x06, 0xd000 },
		{ 0x06, 0x0282 },
		{ 0x06, 0x5bbf },
		{ 0x06, 0x8350 },
		{ 0x06, 0xef46 },
		{ 0x06, 0xdc19 },
		{ 0x06, 0xddd0 },
		{ 0x06, 0x0102 },
		{ 0x06, 0x825b },
		{ 0x06, 0x0282 },
		{ 0x06, 0x77e0 },
		{ 0x06, 0xf860 },
		{ 0x06, 0xe1f8 },
		{ 0x06, 0x6158 },
		{ 0x06, 0xfde4 },
		{ 0x06, 0xf860 },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x61fc },
		{ 0x06, 0x04f9 },
		{ 0x06, 0xfafb },
		{ 0x06, 0xc6bf },
		{ 0x06, 0xf840 },
		{ 0x06, 0xbe83 },
		{ 0x06, 0x50a0 },
		{ 0x06, 0x0101 },
		{ 0x06, 0x071b },
		{ 0x06, 0x89cf },
		{ 0x06, 0xd208 },
		{ 0x06, 0xebdb },
		{ 0x06, 0x19b2 },
		{ 0x06, 0xfbff },
		{ 0x06, 0xfefd },
		{ 0x06, 0x04f8 },
		{ 0x06, 0xe0f8 },
		{ 0x06, 0x48e1 },
		{ 0x06, 0xf849 },
		{ 0x06, 0x6808 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x48e5 },
		{ 0x06, 0xf849 },
		{ 0x06, 0x58f7 },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x48e5 },
		{ 0x06, 0xf849 },
		{ 0x06, 0xfc04 },
		{ 0x06, 0x4d20 },
		{ 0x06, 0x0002 },
		{ 0x06, 0x4e22 },
		{ 0x06, 0x0002 },
		{ 0x06, 0x4ddf },
		{ 0x06, 0xff01 },
		{ 0x06, 0x4edd },
		{ 0x06, 0xff01 },
		{ 0x05, 0x83d4 },
		{ 0x06, 0x8000 },
		{ 0x05, 0x83d8 },
		{ 0x06, 0x8051 },
		{ 0x02, 0x6010 },
		{ 0x03, 0xdc00 },
		{ 0x05, 0xfff6 },
		{ 0x06, 0x00fc },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0000 },
		{ 0x0d, 0xf880 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init_0, ARRAY_SIZE(phy_reg_init_0));

	mdio_write(ioaddr, 0x1f, 0x0002);
	mdio_plus_minus(ioaddr, 0x0b, 0x0010, 0x00ef);
	mdio_plus_minus(ioaddr, 0x0c, 0xa200, 0x5d00);

	rtl_phy_write(ioaddr, phy_reg_init_1, ARRAY_SIZE(phy_reg_init_1));

	if (rtl8168d_efuse_read(ioaddr, 0x01) == 0xb1) {
		struct phy_reg phy_reg_init[] = {
			{ 0x1f, 0x0002 },
			{ 0x05, 0x669a },
			{ 0x1f, 0x0005 },
			{ 0x05, 0x8330 },
			{ 0x06, 0x669a },
			{ 0x1f, 0x0002 }
		};
		int val;

		rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

		val = mdio_read(ioaddr, 0x0d);

		if ((val & 0x00ff) != 0x006c) {
			u32 set[] = {
				0x0065, 0x0066, 0x0067, 0x0068,
				0x0069, 0x006a, 0x006b, 0x006c
			};
			int i;

			mdio_write(ioaddr, 0x1f, 0x0002);

			val &= 0xff00;
			for (i = 0; i < ARRAY_SIZE(set); i++)
				mdio_write(ioaddr, 0x0d, val | set[i]);
		}
	} else {
		struct phy_reg phy_reg_init[] = {
			{ 0x1f, 0x0002 },
			{ 0x05, 0x6662 },
			{ 0x1f, 0x0005 },
			{ 0x05, 0x8330 },
			{ 0x06, 0x6662 }
		};

		rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
	}

	mdio_write(ioaddr, 0x1f, 0x0002);
	mdio_patch(ioaddr, 0x0d, 0x0300);
	mdio_patch(ioaddr, 0x0f, 0x0010);

	mdio_write(ioaddr, 0x1f, 0x0002);
	mdio_plus_minus(ioaddr, 0x02, 0x0100, 0x0600);
	mdio_plus_minus(ioaddr, 0x03, 0x0000, 0xe000);

	rtl_phy_write(ioaddr, phy_reg_init_2, ARRAY_SIZE(phy_reg_init_2));
}

static void rtl8168d_2_hw_phy_config(void __iomem *ioaddr)
{
	static struct phy_reg phy_reg_init_0[] = {
		{ 0x1f, 0x0001 },
		{ 0x06, 0x4064 },
		{ 0x07, 0x2863 },
		{ 0x08, 0x059c },
		{ 0x09, 0x26b4 },
		{ 0x0a, 0x6a19 },
		{ 0x0b, 0xdcc8 },
		{ 0x10, 0xf06d },
		{ 0x14, 0x7f68 },
		{ 0x18, 0x7fd9 },
		{ 0x1c, 0xf0ff },
		{ 0x1d, 0x3d9c },
		{ 0x1f, 0x0003 },
		{ 0x12, 0xf49f },
		{ 0x13, 0x070b },
		{ 0x1a, 0x05ad },
		{ 0x14, 0x94c0 },

		{ 0x1f, 0x0002 },
		{ 0x06, 0x5561 },
		{ 0x1f, 0x0005 },
		{ 0x05, 0x8332 },
		{ 0x06, 0x5561 }
	};
	static struct phy_reg phy_reg_init_1[] = {
		{ 0x1f, 0x0005 },
		{ 0x05, 0xffc2 },
		{ 0x1f, 0x0005 },
		{ 0x05, 0x8000 },
		{ 0x06, 0xf8f9 },
		{ 0x06, 0xfaee },
		{ 0x06, 0xf8ea },
		{ 0x06, 0x00ee },
		{ 0x06, 0xf8eb },
		{ 0x06, 0x00e2 },
		{ 0x06, 0xf87c },
		{ 0x06, 0xe3f8 },
		{ 0x06, 0x7da5 },
		{ 0x06, 0x1111 },
		{ 0x06, 0x12d2 },
		{ 0x06, 0x40d6 },
		{ 0x06, 0x4444 },
		{ 0x06, 0x0281 },
		{ 0x06, 0xc6d2 },
		{ 0x06, 0xa0d6 },
		{ 0x06, 0xaaaa },
		{ 0x06, 0x0281 },
		{ 0x06, 0xc6ae },
		{ 0x06, 0x0fa5 },
		{ 0x06, 0x4444 },
		{ 0x06, 0x02ae },
		{ 0x06, 0x4da5 },
		{ 0x06, 0xaaaa },
		{ 0x06, 0x02ae },
		{ 0x06, 0x47af },
		{ 0x06, 0x81c2 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4e00 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4d0f },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4c0f },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4f00 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x5100 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4aff },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4bff },
		{ 0x06, 0xe083 },
		{ 0x06, 0x30e1 },
		{ 0x06, 0x8331 },
		{ 0x06, 0x58fe },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x8ae5 },
		{ 0x06, 0xf88b },
		{ 0x06, 0xe083 },
		{ 0x06, 0x32e1 },
		{ 0x06, 0x8333 },
		{ 0x06, 0x590f },
		{ 0x06, 0xe283 },
		{ 0x06, 0x4d0c },
		{ 0x06, 0x245a },
		{ 0x06, 0xf01e },
		{ 0x06, 0x12e4 },
		{ 0x06, 0xf88c },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x8daf },
		{ 0x06, 0x81c2 },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4f10 },
		{ 0x06, 0xe483 },
		{ 0x06, 0x4fe0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7800 },
		{ 0x06, 0x9f0a },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4fa0 },
		{ 0x06, 0x10a5 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4e01 },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x059e },
		{ 0x06, 0x9ae0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7804 },
		{ 0x06, 0x9e10 },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x039e },
		{ 0x06, 0x0fe0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7801 },
		{ 0x06, 0x9e05 },
		{ 0x06, 0xae0c },
		{ 0x06, 0xaf81 },
		{ 0x06, 0xa7af },
		{ 0x06, 0x8152 },
		{ 0x06, 0xaf81 },
		{ 0x06, 0x8baf },
		{ 0x06, 0x81c2 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4800 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4900 },
		{ 0x06, 0xe083 },
		{ 0x06, 0x5110 },
		{ 0x06, 0xe483 },
		{ 0x06, 0x5158 },
		{ 0x06, 0x019f },
		{ 0x06, 0xead0 },
		{ 0x06, 0x00d1 },
		{ 0x06, 0x801f },
		{ 0x06, 0x66e2 },
		{ 0x06, 0xf8ea },
		{ 0x06, 0xe3f8 },
		{ 0x06, 0xeb5a },
		{ 0x06, 0xf81e },
		{ 0x06, 0x20e6 },
		{ 0x06, 0xf8ea },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0xebd3 },
		{ 0x06, 0x02b3 },
		{ 0x06, 0xfee2 },
		{ 0x06, 0xf87c },
		{ 0x06, 0xef32 },
		{ 0x06, 0x5b80 },
		{ 0x06, 0xe3f8 },
		{ 0x06, 0x7d9e },
		{ 0x06, 0x037d },
		{ 0x06, 0xffff },
		{ 0x06, 0x0d58 },
		{ 0x06, 0x1c55 },
		{ 0x06, 0x1a65 },
		{ 0x06, 0x11a1 },
		{ 0x06, 0x90d3 },
		{ 0x06, 0xe283 },
		{ 0x06, 0x48e3 },
		{ 0x06, 0x8349 },
		{ 0x06, 0x1b56 },
		{ 0x06, 0xab08 },
		{ 0x06, 0xef56 },
		{ 0x06, 0xe683 },
		{ 0x06, 0x48e7 },
		{ 0x06, 0x8349 },
		{ 0x06, 0x10d1 },
		{ 0x06, 0x801f },
		{ 0x06, 0x66a0 },
		{ 0x06, 0x04b9 },
		{ 0x06, 0xe283 },
		{ 0x06, 0x48e3 },
		{ 0x06, 0x8349 },
		{ 0x06, 0xef65 },
		{ 0x06, 0xe283 },
		{ 0x06, 0x4ae3 },
		{ 0x06, 0x834b },
		{ 0x06, 0x1b56 },
		{ 0x06, 0xaa0e },
		{ 0x06, 0xef56 },
		{ 0x06, 0xe683 },
		{ 0x06, 0x4ae7 },
		{ 0x06, 0x834b },
		{ 0x06, 0xe283 },
		{ 0x06, 0x4de6 },
		{ 0x06, 0x834c },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4da0 },
		{ 0x06, 0x000c },
		{ 0x06, 0xaf81 },
		{ 0x06, 0x8be0 },
		{ 0x06, 0x834d },
		{ 0x06, 0x10e4 },
		{ 0x06, 0x834d },
		{ 0x06, 0xae04 },
		{ 0x06, 0x80e4 },
		{ 0x06, 0x834d },
		{ 0x06, 0xe083 },
		{ 0x06, 0x4e78 },
		{ 0x06, 0x039e },
		{ 0x06, 0x0be0 },
		{ 0x06, 0x834e },
		{ 0x06, 0x7804 },
		{ 0x06, 0x9e04 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4e02 },
		{ 0x06, 0xe083 },
		{ 0x06, 0x32e1 },
		{ 0x06, 0x8333 },
		{ 0x06, 0x590f },
		{ 0x06, 0xe283 },
		{ 0x06, 0x4d0c },
		{ 0x06, 0x245a },
		{ 0x06, 0xf01e },
		{ 0x06, 0x12e4 },
		{ 0x06, 0xf88c },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x8de0 },
		{ 0x06, 0x8330 },
		{ 0x06, 0xe183 },
		{ 0x06, 0x3168 },
		{ 0x06, 0x01e4 },
		{ 0x06, 0xf88a },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x8bae },
		{ 0x06, 0x37ee },
		{ 0x06, 0x834e },
		{ 0x06, 0x03e0 },
		{ 0x06, 0x834c },
		{ 0x06, 0xe183 },
		{ 0x06, 0x4d1b },
		{ 0x06, 0x019e },
		{ 0x06, 0x04aa },
		{ 0x06, 0xa1ae },
		{ 0x06, 0xa8ee },
		{ 0x06, 0x834e },
		{ 0x06, 0x04ee },
		{ 0x06, 0x834f },
		{ 0x06, 0x00ae },
		{ 0x06, 0xabe0 },
		{ 0x06, 0x834f },
		{ 0x06, 0x7803 },
		{ 0x06, 0x9f14 },
		{ 0x06, 0xee83 },
		{ 0x06, 0x4e05 },
		{ 0x06, 0xd240 },
		{ 0x06, 0xd655 },
		{ 0x06, 0x5402 },
		{ 0x06, 0x81c6 },
		{ 0x06, 0xd2a0 },
		{ 0x06, 0xd6ba },
		{ 0x06, 0x0002 },
		{ 0x06, 0x81c6 },
		{ 0x06, 0xfefd },
		{ 0x06, 0xfc05 },
		{ 0x06, 0xf8e0 },
		{ 0x06, 0xf860 },
		{ 0x06, 0xe1f8 },
		{ 0x06, 0x6168 },
		{ 0x06, 0x02e4 },
		{ 0x06, 0xf860 },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x61e0 },
		{ 0x06, 0xf848 },
		{ 0x06, 0xe1f8 },
		{ 0x06, 0x4958 },
		{ 0x06, 0x0f1e },
		{ 0x06, 0x02e4 },
		{ 0x06, 0xf848 },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x49d0 },
		{ 0x06, 0x0002 },
		{ 0x06, 0x820a },
		{ 0x06, 0xbf83 },
		{ 0x06, 0x50ef },
		{ 0x06, 0x46dc },
		{ 0x06, 0x19dd },
		{ 0x06, 0xd001 },
		{ 0x06, 0x0282 },
		{ 0x06, 0x0a02 },
		{ 0x06, 0x8226 },
		{ 0x06, 0xe0f8 },
		{ 0x06, 0x60e1 },
		{ 0x06, 0xf861 },
		{ 0x06, 0x58fd },
		{ 0x06, 0xe4f8 },
		{ 0x06, 0x60e5 },
		{ 0x06, 0xf861 },
		{ 0x06, 0xfc04 },
		{ 0x06, 0xf9fa },
		{ 0x06, 0xfbc6 },
		{ 0x06, 0xbff8 },
		{ 0x06, 0x40be },
		{ 0x06, 0x8350 },
		{ 0x06, 0xa001 },
		{ 0x06, 0x0107 },
		{ 0x06, 0x1b89 },
		{ 0x06, 0xcfd2 },
		{ 0x06, 0x08eb },
		{ 0x06, 0xdb19 },
		{ 0x06, 0xb2fb },
		{ 0x06, 0xfffe },
		{ 0x06, 0xfd04 },
		{ 0x06, 0xf8e0 },
		{ 0x06, 0xf848 },
		{ 0x06, 0xe1f8 },
		{ 0x06, 0x4968 },
		{ 0x06, 0x08e4 },
		{ 0x06, 0xf848 },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x4958 },
		{ 0x06, 0xf7e4 },
		{ 0x06, 0xf848 },
		{ 0x06, 0xe5f8 },
		{ 0x06, 0x49fc },
		{ 0x06, 0x044d },
		{ 0x06, 0x2000 },
		{ 0x06, 0x024e },
		{ 0x06, 0x2200 },
		{ 0x06, 0x024d },
		{ 0x06, 0xdfff },
		{ 0x06, 0x014e },
		{ 0x06, 0xddff },
		{ 0x06, 0x0100 },
		{ 0x05, 0x83d8 },
		{ 0x06, 0x8000 },
		{ 0x03, 0xdc00 },
		{ 0x05, 0xfff6 },
		{ 0x06, 0x00fc },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0000 },
		{ 0x0d, 0xf880 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init_0, ARRAY_SIZE(phy_reg_init_0));

	if (rtl8168d_efuse_read(ioaddr, 0x01) == 0xb1) {
		struct phy_reg phy_reg_init[] = {
			{ 0x1f, 0x0002 },
			{ 0x05, 0x669a },
			{ 0x1f, 0x0005 },
			{ 0x05, 0x8330 },
			{ 0x06, 0x669a },

			{ 0x1f, 0x0002 }
		};
		int val;

		rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));

		val = mdio_read(ioaddr, 0x0d);
		if ((val & 0x00ff) != 0x006c) {
			u32 set[] = {
				0x0065, 0x0066, 0x0067, 0x0068,
				0x0069, 0x006a, 0x006b, 0x006c
			};
			int i;

			mdio_write(ioaddr, 0x1f, 0x0002);

			val &= 0xff00;
			for (i = 0; i < ARRAY_SIZE(set); i++)
				mdio_write(ioaddr, 0x0d, val | set[i]);
		}
	} else {
		struct phy_reg phy_reg_init[] = {
			{ 0x1f, 0x0002 },
			{ 0x05, 0x2642 },
			{ 0x1f, 0x0005 },
			{ 0x05, 0x8330 },
			{ 0x06, 0x2642 }
		};

		rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
	}

	mdio_write(ioaddr, 0x1f, 0x0002);
	mdio_plus_minus(ioaddr, 0x02, 0x0100, 0x0600);
	mdio_plus_minus(ioaddr, 0x03, 0x0000, 0xe000);

	mdio_write(ioaddr, 0x1f, 0x0001);
	mdio_write(ioaddr, 0x17, 0x0cc0);

	mdio_write(ioaddr, 0x1f, 0x0002);
	mdio_patch(ioaddr, 0x0f, 0x0017);

	rtl_phy_write(ioaddr, phy_reg_init_1, ARRAY_SIZE(phy_reg_init_1));
}

static void rtl8168d_3_hw_phy_config(void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0002 },
		{ 0x10, 0x0008 },
		{ 0x0d, 0x006c },

		{ 0x1f, 0x0000 },
		{ 0x0d, 0xf880 },

		{ 0x1f, 0x0001 },
		{ 0x17, 0x0cc0 },

		{ 0x1f, 0x0001 },
		{ 0x0b, 0xa4d8 },
		{ 0x09, 0x281c },
		{ 0x07, 0x2883 },
		{ 0x0a, 0x6b35 },
		{ 0x1d, 0x3da4 },
		{ 0x1c, 0xeffd },
		{ 0x14, 0x7f52 },
		{ 0x18, 0x7fc6 },
		{ 0x08, 0x0601 },
		{ 0x06, 0x4063 },
		{ 0x10, 0xf074 },
		{ 0x1f, 0x0003 },
		{ 0x13, 0x0789 },
		{ 0x12, 0xf4bd },
		{ 0x1a, 0x04fd },
		{ 0x14, 0x84b0 },
		{ 0x1f, 0x0000 },
		{ 0x00, 0x9200 },

		{ 0x1f, 0x0005 },
		{ 0x01, 0x0340 },
		{ 0x1f, 0x0001 },
		{ 0x04, 0x4000 },
		{ 0x03, 0x1d21 },
		{ 0x02, 0x0c32 },
		{ 0x01, 0x0200 },
		{ 0x00, 0x5554 },
		{ 0x04, 0x4800 },
		{ 0x04, 0x4000 },
		{ 0x04, 0xf000 },
		{ 0x03, 0xdf01 },
		{ 0x02, 0xdf20 },
		{ 0x01, 0x101a },
		{ 0x00, 0xa0ff },
		{ 0x04, 0xf800 },
		{ 0x04, 0xf000 },
		{ 0x1f, 0x0000 },

		{ 0x1f, 0x0007 },
		{ 0x1e, 0x0023 },
		{ 0x16, 0x0000 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8102e_hw_phy_config(void __iomem *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0003 },
		{ 0x08, 0x441d },
		{ 0x01, 0x9100 },
		{ 0x1f, 0x0000 }
	};

	mdio_write(ioaddr, 0x1f, 0x0000);
	mdio_patch(ioaddr, 0x11, 1 << 12);
	mdio_patch(ioaddr, 0x19, 1 << 13);
	mdio_patch(ioaddr, 0x10, 1 << 15);

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl_hw_phy_config(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	rtl8169_print_mac_version(tp);

	switch (tp->mac_version) {
	case RTL_GIGA_MAC_VER_01:
		break;
	case RTL_GIGA_MAC_VER_02:
	case RTL_GIGA_MAC_VER_03:
		rtl8169s_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_04:
		rtl8169sb_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_05:
		rtl8169scd_hw_phy_config(tp, ioaddr);
		break;
	case RTL_GIGA_MAC_VER_06:
		rtl8169sce_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_07:
	case RTL_GIGA_MAC_VER_08:
	case RTL_GIGA_MAC_VER_09:
		rtl8102e_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_11:
		rtl8168bb_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_12:
		rtl8168bef_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_17:
		rtl8168bef_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_18:
		rtl8168cp_1_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_19:
		rtl8168c_1_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_20:
		rtl8168c_2_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_21:
		rtl8168c_3_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_22:
		rtl8168c_4_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_23:
	case RTL_GIGA_MAC_VER_24:
		rtl8168cp_2_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_25:
		rtl8168d_1_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_26:
		rtl8168d_2_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_27:
		rtl8168d_3_hw_phy_config(ioaddr);
		break;

	default:
		break;
	}
}

static void rtl8169_phy_timer(unsigned long __opaque)
{
	struct net_device *dev = (struct net_device *)__opaque;
	struct rtl8169_private *tp = netdev_priv(dev);
	struct timer_list *timer = &tp->timer;
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned long timeout = RTL8169_PHY_TIMEOUT;

	assert(tp->mac_version > RTL_GIGA_MAC_VER_01);

	if (!(tp->phy_1000_ctrl_reg & ADVERTISE_1000FULL))
		return;

	spin_lock_irq(&tp->lock);

	if (tp->phy_reset_pending(ioaddr)) {
		/*
		 * A busy loop could burn quite a few cycles on nowadays CPU.
		 * Let's delay the execution of the timer for a few ticks.
		 */
		timeout = HZ/10;
		goto out_mod_timer;
	}

	if (tp->link_ok(ioaddr))
		goto out_unlock;

	if (netif_msg_link(tp))
		printk(KERN_WARNING "%s: PHY reset until link up\n", dev->name);

	tp->phy_reset_enable(ioaddr);

out_mod_timer:
	mod_timer(timer, jiffies + timeout);
out_unlock:
	spin_unlock_irq(&tp->lock);
}

static inline void rtl8169_delete_timer(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct timer_list *timer = &tp->timer;

	if (tp->mac_version <= RTL_GIGA_MAC_VER_01)
		return;

	del_timer_sync(timer);
}

static inline void rtl8169_request_timer(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct timer_list *timer = &tp->timer;

	if (tp->mac_version <= RTL_GIGA_MAC_VER_01)
		return;

	mod_timer(timer, jiffies + RTL8169_PHY_TIMEOUT);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Polling 'interrupt' - used by things like netconsole to send skbs
 * without having to re-enable interrupts. It's not called while
 * the interrupt routine is executing.
 */
static void rtl8169_netpoll(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct pci_dev *pdev = tp->pci_dev;

	disable_irq(pdev->irq);
	rtl8169_interrupt(pdev->irq, dev);
	enable_irq(pdev->irq);
}
#endif

static void rtl8169_release_board(struct pci_dev *pdev, struct net_device *dev,
				  void __iomem *ioaddr)
{
	iounmap(ioaddr);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	free_netdev(dev);
}

static void rtl8169_phy_reset(struct net_device *dev,
			      struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned int i;

	tp->phy_reset_enable(ioaddr);
	for (i = 0; i < 100; i++) {
		if (!tp->phy_reset_pending(ioaddr))
			return;
		msleep(1);
	}
	if (netif_msg_link(tp))
		printk(KERN_ERR "%s: PHY reset failed.\n", dev->name);
}

static void rtl8169_init_phy(struct net_device *dev, struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;

	rtl_hw_phy_config(dev);

	if (tp->mac_version <= RTL_GIGA_MAC_VER_06) {
		dprintk("Set MAC Reg C+CR Offset 0x82h = 0x01h\n");
		RTL_W8(0x82, 0x01);
	}

	pci_write_config_byte(tp->pci_dev, PCI_LATENCY_TIMER, 0x40);

	if (tp->mac_version <= RTL_GIGA_MAC_VER_06)
		pci_write_config_byte(tp->pci_dev, PCI_CACHE_LINE_SIZE, 0x08);

	if (tp->mac_version == RTL_GIGA_MAC_VER_02) {
		dprintk("Set MAC Reg C+CR Offset 0x82h = 0x01h\n");
		RTL_W8(0x82, 0x01);
		dprintk("Set PHY Reg 0x0bh = 0x00h\n");
		mdio_write(ioaddr, 0x0b, 0x0000); //w 0x0b 15 0 0
	}

	rtl8169_phy_reset(dev, tp);

	/*
	 * rtl8169_set_speed_xmii takes good care of the Fast Ethernet
	 * only 8101. Don't panic.
	 */
	rtl8169_set_speed(dev, AUTONEG_ENABLE, SPEED_1000, DUPLEX_FULL);

	if ((RTL_R8(PHYstatus) & TBI_Enable) && netif_msg_link(tp))
		printk(KERN_INFO PFX "%s: TBI auto-negotiating\n", dev->name);
}

static void rtl_rar_set(struct rtl8169_private *tp, u8 *addr)
{
	void __iomem *ioaddr = tp->mmio_addr;
	u32 high;
	u32 low;

	low  = addr[0] | (addr[1] << 8) | (addr[2] << 16) | (addr[3] << 24);
	high = addr[4] | (addr[5] << 8);

	spin_lock_irq(&tp->lock);

	RTL_W8(Cfg9346, Cfg9346_Unlock);
	RTL_W32(MAC0, low);
	RTL_W32(MAC4, high);
	RTL_W8(Cfg9346, Cfg9346_Lock);

	spin_unlock_irq(&tp->lock);
}

static int rtl_set_mac_address(struct net_device *dev, void *p)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	rtl_rar_set(tp, dev->dev_addr);

	return 0;
}

static int rtl8169_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct mii_ioctl_data *data = if_mii(ifr);

	return netif_running(dev) ? tp->do_ioctl(tp, data, cmd) : -ENODEV;
}

static int rtl_xmii_ioctl(struct rtl8169_private *tp, struct mii_ioctl_data *data, int cmd)
{
	switch (cmd) {
	case SIOCGMIIPHY:
		data->phy_id = 32; /* Internal PHY */
		return 0;

	case SIOCGMIIREG:
		data->val_out = mdio_read(tp->mmio_addr, data->reg_num & 0x1f);
		return 0;

	case SIOCSMIIREG:
		mdio_write(tp->mmio_addr, data->reg_num & 0x1f, data->val_in);
		return 0;
	}
	return -EOPNOTSUPP;
}

static int rtl_tbi_ioctl(struct rtl8169_private *tp, struct mii_ioctl_data *data, int cmd)
{
	return -EOPNOTSUPP;
}

static const struct rtl_cfg_info {
	void (*hw_start)(struct net_device *);
	unsigned int region;
	unsigned int align;
	u16 intr_event;
	u16 napi_event;
	unsigned features;
	u8 default_ver;
} rtl_cfg_infos [] = {
	[RTL_CFG_0] = {
		.hw_start	= rtl_hw_start_8169,
		.region		= 1,
		.align		= 0,
		.intr_event	= SYSErr | LinkChg | RxOverflow |
				  RxFIFOOver | TxErr | TxOK | RxOK | RxErr,
		.napi_event	= RxFIFOOver | TxErr | TxOK | RxOK | RxOverflow,
		.features	= RTL_FEATURE_GMII,
		.default_ver	= RTL_GIGA_MAC_VER_01,
	},
	[RTL_CFG_1] = {
		.hw_start	= rtl_hw_start_8168,
		.region		= 2,
		.align		= 8,
		.intr_event	= SYSErr | LinkChg | RxOverflow |
				  TxErr | TxOK | RxOK | RxErr,
		.napi_event	= TxErr | TxOK | RxOK | RxOverflow,
		.features	= RTL_FEATURE_GMII | RTL_FEATURE_MSI,
		.default_ver	= RTL_GIGA_MAC_VER_11,
	},
	[RTL_CFG_2] = {
		.hw_start	= rtl_hw_start_8101,
		.region		= 2,
		.align		= 8,
		.intr_event	= SYSErr | LinkChg | RxOverflow | PCSTimeout |
				  RxFIFOOver | TxErr | TxOK | RxOK | RxErr,
		.napi_event	= RxFIFOOver | TxErr | TxOK | RxOK | RxOverflow,
		.features	= RTL_FEATURE_MSI,
		.default_ver	= RTL_GIGA_MAC_VER_13,
	}
};

/* Cfg9346_Unlock assumed. */
static unsigned rtl_try_msi(struct pci_dev *pdev, void __iomem *ioaddr,
			    const struct rtl_cfg_info *cfg)
{
	unsigned msi = 0;
	u8 cfg2;

	cfg2 = RTL_R8(Config2) & ~MSIEnable;
	if (cfg->features & RTL_FEATURE_MSI) {
		if (pci_enable_msi(pdev)) {
			dev_info(&pdev->dev, "no MSI. Back to INTx.\n");
		} else {
			cfg2 |= MSIEnable;
			msi = RTL_FEATURE_MSI;
		}
	}
	RTL_W8(Config2, cfg2);
	return msi;
}

static void rtl_disable_msi(struct pci_dev *pdev, struct rtl8169_private *tp)
{
	if (tp->features & RTL_FEATURE_MSI) {
		pci_disable_msi(pdev);
		tp->features &= ~RTL_FEATURE_MSI;
	}
}

static const struct net_device_ops rtl8169_netdev_ops = {
	.ndo_open		= rtl8169_open,
	.ndo_stop		= rtl8169_close,
	.ndo_get_stats		= rtl8169_get_stats,
	.ndo_start_xmit		= rtl8169_start_xmit,
	.ndo_tx_timeout		= rtl8169_tx_timeout,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= rtl8169_change_mtu,
	.ndo_set_mac_address	= rtl_set_mac_address,
	.ndo_do_ioctl		= rtl8169_ioctl,
	.ndo_set_multicast_list	= rtl_set_rx_mode,
#ifdef CONFIG_R8169_VLAN
	.ndo_vlan_rx_register	= rtl8169_vlan_rx_register,
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= rtl8169_netpoll,
#endif

};

static int __devinit
rtl8169_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	const struct rtl_cfg_info *cfg = rtl_cfg_infos + ent->driver_data;
	const unsigned int region = cfg->region;
	struct rtl8169_private *tp;
	struct mii_if_info *mii;
	struct net_device *dev;
	void __iomem *ioaddr;
	unsigned int i;
	int rc;

	if (netif_msg_drv(&debug)) {
		printk(KERN_INFO "%s Gigabit Ethernet driver %s loaded\n",
		       MODULENAME, RTL8169_VERSION);
	}

	dev = alloc_etherdev(sizeof (*tp));
	if (!dev) {
		if (netif_msg_drv(&debug))
			dev_err(&pdev->dev, "unable to alloc new ethernet\n");
		rc = -ENOMEM;
		goto out;
	}

	SET_NETDEV_DEV(dev, &pdev->dev);
	dev->netdev_ops = &rtl8169_netdev_ops;
	tp = netdev_priv(dev);
	tp->dev = dev;
	tp->pci_dev = pdev;
	tp->msg_enable = netif_msg_init(debug.msg_enable, R8169_MSG_DEFAULT);

	mii = &tp->mii;
	mii->dev = dev;
	mii->mdio_read = rtl_mdio_read;
	mii->mdio_write = rtl_mdio_write;
	mii->phy_id_mask = 0x1f;
	mii->reg_num_mask = 0x1f;
	mii->supports_gmii = !!(cfg->features & RTL_FEATURE_GMII);

	/* enable device (incl. PCI PM wakeup and hotplug setup) */
	rc = pci_enable_device(pdev);
	if (rc < 0) {
		if (netif_msg_probe(tp))
			dev_err(&pdev->dev, "enable failure\n");
		goto err_out_free_dev_1;
	}

	rc = pci_set_mwi(pdev);
	if (rc < 0)
		goto err_out_disable_2;

	/* make sure PCI base addr 1 is MMIO */
	if (!(pci_resource_flags(pdev, region) & IORESOURCE_MEM)) {
		if (netif_msg_probe(tp)) {
			dev_err(&pdev->dev,
				"region #%d not an MMIO resource, aborting\n",
				region);
		}
		rc = -ENODEV;
		goto err_out_mwi_3;
	}

	/* check for weird/broken PCI region reporting */
	if (pci_resource_len(pdev, region) < R8169_REGS_SIZE) {
		if (netif_msg_probe(tp)) {
			dev_err(&pdev->dev,
				"Invalid PCI region size(s), aborting\n");
		}
		rc = -ENODEV;
		goto err_out_mwi_3;
	}

	rc = pci_request_regions(pdev, MODULENAME);
	if (rc < 0) {
		if (netif_msg_probe(tp))
			dev_err(&pdev->dev, "could not request regions.\n");
		goto err_out_mwi_3;
	}

	tp->cp_cmd = PCIMulRW | RxChkSum;

	if ((sizeof(dma_addr_t) > 4) &&
	    !pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) && use_dac) {
		tp->cp_cmd |= PCIDAC;
		dev->features |= NETIF_F_HIGHDMA;
	} else {
		rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (rc < 0) {
			if (netif_msg_probe(tp)) {
				dev_err(&pdev->dev,
					"DMA configuration failed.\n");
			}
			goto err_out_free_res_4;
		}
	}

	/* ioremap MMIO region */
	ioaddr = ioremap(pci_resource_start(pdev, region), R8169_REGS_SIZE);
	if (!ioaddr) {
		if (netif_msg_probe(tp))
			dev_err(&pdev->dev, "cannot remap MMIO, aborting\n");
		rc = -EIO;
		goto err_out_free_res_4;
	}

	tp->pcie_cap = pci_find_capability(pdev, PCI_CAP_ID_EXP);
	if (!tp->pcie_cap && netif_msg_probe(tp))
		dev_info(&pdev->dev, "no PCI Express capability\n");

	RTL_W16(IntrMask, 0x0000);

	/* Soft reset the chip. */
	RTL_W8(ChipCmd, CmdReset);

	/* Check that the chip has finished the reset. */
	for (i = 0; i < 100; i++) {
		if ((RTL_R8(ChipCmd) & CmdReset) == 0)
			break;
		msleep_interruptible(1);
	}

	RTL_W16(IntrStatus, 0xffff);

	pci_set_master(pdev);

	/* Identify chip attached to board */
	rtl8169_get_mac_version(tp, ioaddr);

	/* Use appropriate default if unknown */
	if (tp->mac_version == RTL_GIGA_MAC_NONE) {
		if (netif_msg_probe(tp)) {
			dev_notice(&pdev->dev,
				   "unknown MAC, using family default\n");
		}
		tp->mac_version = cfg->default_ver;
	}

	rtl8169_print_mac_version(tp);

	for (i = 0; i < ARRAY_SIZE(rtl_chip_info); i++) {
		if (tp->mac_version == rtl_chip_info[i].mac_version)
			break;
	}
	if (i == ARRAY_SIZE(rtl_chip_info)) {
		dev_err(&pdev->dev,
			"driver bug, MAC version not found in rtl_chip_info\n");
		goto err_out_msi_5;
	}
	tp->chipset = i;

	RTL_W8(Cfg9346, Cfg9346_Unlock);
	RTL_W8(Config1, RTL_R8(Config1) | PMEnable);
	RTL_W8(Config5, RTL_R8(Config5) & PMEStatus);
	if ((RTL_R8(Config3) & (LinkUp | MagicPacket)) != 0)
		tp->features |= RTL_FEATURE_WOL;
	if ((RTL_R8(Config5) & (UWF | BWF | MWF)) != 0)
		tp->features |= RTL_FEATURE_WOL;
	tp->features |= rtl_try_msi(pdev, ioaddr, cfg);
	RTL_W8(Cfg9346, Cfg9346_Lock);

	if ((tp->mac_version <= RTL_GIGA_MAC_VER_06) &&
	    (RTL_R8(PHYstatus) & TBI_Enable)) {
		tp->set_speed = rtl8169_set_speed_tbi;
		tp->get_settings = rtl8169_gset_tbi;
		tp->phy_reset_enable = rtl8169_tbi_reset_enable;
		tp->phy_reset_pending = rtl8169_tbi_reset_pending;
		tp->link_ok = rtl8169_tbi_link_ok;
		tp->do_ioctl = rtl_tbi_ioctl;

		tp->phy_1000_ctrl_reg = ADVERTISE_1000FULL; /* Implied by TBI */
	} else {
		tp->set_speed = rtl8169_set_speed_xmii;
		tp->get_settings = rtl8169_gset_xmii;
		tp->phy_reset_enable = rtl8169_xmii_reset_enable;
		tp->phy_reset_pending = rtl8169_xmii_reset_pending;
		tp->link_ok = rtl8169_xmii_link_ok;
		tp->do_ioctl = rtl_xmii_ioctl;
	}

	spin_lock_init(&tp->lock);

	tp->mmio_addr = ioaddr;

	/* Get MAC address */
	for (i = 0; i < MAC_ADDR_LEN; i++)
		dev->dev_addr[i] = RTL_R8(MAC0 + i);
	memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);

	SET_ETHTOOL_OPS(dev, &rtl8169_ethtool_ops);
	dev->watchdog_timeo = RTL8169_TX_TIMEOUT;
	dev->irq = pdev->irq;
	dev->base_addr = (unsigned long) ioaddr;

	netif_napi_add(dev, &tp->napi, rtl8169_poll, R8169_NAPI_WEIGHT);

#ifdef CONFIG_R8169_VLAN
	dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
#endif

	tp->intr_mask = 0xffff;
	tp->align = cfg->align;
	tp->hw_start = cfg->hw_start;
	tp->intr_event = cfg->intr_event;
	tp->napi_event = cfg->napi_event;

	init_timer(&tp->timer);
	tp->timer.data = (unsigned long) dev;
	tp->timer.function = rtl8169_phy_timer;

	rc = register_netdev(dev);
	if (rc < 0)
		goto err_out_msi_5;

	pci_set_drvdata(pdev, dev);

	if (netif_msg_probe(tp)) {
		u32 xid = RTL_R32(TxConfig) & 0x9cf0f8ff;

		printk(KERN_INFO "%s: %s at 0x%lx, "
		       "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, "
		       "XID %08x IRQ %d\n",
		       dev->name,
		       rtl_chip_info[tp->chipset].name,
		       dev->base_addr,
		       dev->dev_addr[0], dev->dev_addr[1],
		       dev->dev_addr[2], dev->dev_addr[3],
		       dev->dev_addr[4], dev->dev_addr[5], xid, dev->irq);
	}

	rtl8169_init_phy(dev, tp);

	/*
	 * Pretend we are using VLANs; This bypasses a nasty bug where
	 * Interrupts stop flowing on high load on 8110SCd controllers.
	 */
	if (tp->mac_version == RTL_GIGA_MAC_VER_05)
		RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) | RxVlan);

	device_set_wakeup_enable(&pdev->dev, tp->features & RTL_FEATURE_WOL);

out:
	return rc;

err_out_msi_5:
	rtl_disable_msi(pdev, tp);
	iounmap(ioaddr);
err_out_free_res_4:
	pci_release_regions(pdev);
err_out_mwi_3:
	pci_clear_mwi(pdev);
err_out_disable_2:
	pci_disable_device(pdev);
err_out_free_dev_1:
	free_netdev(dev);
	goto out;
}

static void __devexit rtl8169_remove_one(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);

	flush_scheduled_work();

	unregister_netdev(dev);

	/* restore original MAC address */
	rtl_rar_set(tp, dev->perm_addr);

	rtl_disable_msi(pdev, tp);
	rtl8169_release_board(pdev, dev, tp->mmio_addr);
	pci_set_drvdata(pdev, NULL);
}

static void rtl8169_set_rxbufsize(struct rtl8169_private *tp,
				  struct net_device *dev)
{
	unsigned int max_frame = dev->mtu + VLAN_ETH_HLEN + ETH_FCS_LEN;

	tp->rx_buf_sz = (max_frame > RX_BUF_SIZE) ? max_frame : RX_BUF_SIZE;
}

static int rtl8169_open(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	struct pci_dev *pdev = tp->pci_dev;
	int retval = -ENOMEM;


	rtl8169_set_rxbufsize(tp, dev);

	/*
	 * Rx and Tx desscriptors needs 256 bytes alignment.
	 * pci_alloc_consistent provides more.
	 */
	tp->TxDescArray = pci_alloc_consistent(pdev, R8169_TX_RING_BYTES,
					       &tp->TxPhyAddr);
	if (!tp->TxDescArray)
		goto out;

	tp->RxDescArray = pci_alloc_consistent(pdev, R8169_RX_RING_BYTES,
					       &tp->RxPhyAddr);
	if (!tp->RxDescArray)
		goto err_free_tx_0;

	retval = rtl8169_init_ring(dev);
	if (retval < 0)
		goto err_free_rx_1;

	INIT_DELAYED_WORK(&tp->task, NULL);

	smp_mb();

	retval = request_irq(dev->irq, rtl8169_interrupt,
			     (tp->features & RTL_FEATURE_MSI) ? 0 : IRQF_SHARED,
			     dev->name, dev);
	if (retval < 0)
		goto err_release_ring_2;

	napi_enable(&tp->napi);

	rtl_hw_start(dev);

	rtl8169_request_timer(dev);

	rtl8169_check_link_status(dev, tp, tp->mmio_addr);
out:
	return retval;

err_release_ring_2:
	rtl8169_rx_clear(tp);
err_free_rx_1:
	pci_free_consistent(pdev, R8169_RX_RING_BYTES, tp->RxDescArray,
			    tp->RxPhyAddr);
err_free_tx_0:
	pci_free_consistent(pdev, R8169_TX_RING_BYTES, tp->TxDescArray,
			    tp->TxPhyAddr);
	goto out;
}

static void rtl8169_hw_reset(void __iomem *ioaddr)
{
	/* Disable interrupts */
	rtl8169_irq_mask_and_ack(ioaddr);

	/* Reset the chipset */
	RTL_W8(ChipCmd, CmdReset);

	/* PCI commit */
	RTL_R8(ChipCmd);
}

static void rtl_set_rx_tx_config_registers(struct rtl8169_private *tp)
{
	void __iomem *ioaddr = tp->mmio_addr;
	u32 cfg = rtl8169_rx_config;

	cfg |= (RTL_R32(RxConfig) & rtl_chip_info[tp->chipset].RxConfigMask);
	RTL_W32(RxConfig, cfg);

	/* Set DMA burst size and Interframe Gap Time */
	RTL_W32(TxConfig, (TX_DMA_BURST << TxDMAShift) |
		(InterFrameGap << TxInterFrameGapShift));
}

static void rtl_hw_start(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned int i;

	/* Soft reset the chip. */
	RTL_W8(ChipCmd, CmdReset);

	/* Check that the chip has finished the reset. */
	for (i = 0; i < 100; i++) {
		if ((RTL_R8(ChipCmd) & CmdReset) == 0)
			break;
		msleep_interruptible(1);
	}

	tp->hw_start(dev);

	netif_start_queue(dev);
}


static void rtl_set_rx_tx_desc_registers(struct rtl8169_private *tp,
					 void __iomem *ioaddr)
{
	/*
	 * Magic spell: some iop3xx ARM board needs the TxDescAddrHigh
	 * register to be written before TxDescAddrLow to work.
	 * Switching from MMIO to I/O access fixes the issue as well.
	 */
	RTL_W32(TxDescStartAddrHigh, ((u64) tp->TxPhyAddr) >> 32);
	RTL_W32(TxDescStartAddrLow, ((u64) tp->TxPhyAddr) & DMA_BIT_MASK(32));
	RTL_W32(RxDescAddrHigh, ((u64) tp->RxPhyAddr) >> 32);
	RTL_W32(RxDescAddrLow, ((u64) tp->RxPhyAddr) & DMA_BIT_MASK(32));
}

static u16 rtl_rw_cpluscmd(void __iomem *ioaddr)
{
	u16 cmd;

	cmd = RTL_R16(CPlusCmd);
	RTL_W16(CPlusCmd, cmd);
	return cmd;
}

static void rtl_set_rx_max_size(void __iomem *ioaddr, unsigned int rx_buf_sz)
{
	/* Low hurts. Let's disable the filtering. */
	RTL_W16(RxMaxSize, rx_buf_sz + 1);
}

static void rtl8169_set_magic_reg(void __iomem *ioaddr, unsigned mac_version)
{
	struct {
		u32 mac_version;
		u32 clk;
		u32 val;
	} cfg2_info [] = {
		{ RTL_GIGA_MAC_VER_05, PCI_Clock_33MHz, 0x000fff00 }, // 8110SCd
		{ RTL_GIGA_MAC_VER_05, PCI_Clock_66MHz, 0x000fffff },
		{ RTL_GIGA_MAC_VER_06, PCI_Clock_33MHz, 0x00ffff00 }, // 8110SCe
		{ RTL_GIGA_MAC_VER_06, PCI_Clock_66MHz, 0x00ffffff }
	}, *p = cfg2_info;
	unsigned int i;
	u32 clk;

	clk = RTL_R8(Config2) & PCI_Clock_66MHz;
	for (i = 0; i < ARRAY_SIZE(cfg2_info); i++, p++) {
		if ((p->mac_version == mac_version) && (p->clk == clk)) {
			RTL_W32(0x7c, p->val);
			break;
		}
	}
}

static void rtl_hw_start_8169(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	struct pci_dev *pdev = tp->pci_dev;

	if (tp->mac_version == RTL_GIGA_MAC_VER_05) {
		RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) | PCIMulRW);
		pci_write_config_byte(pdev, PCI_CACHE_LINE_SIZE, 0x08);
	}

	RTL_W8(Cfg9346, Cfg9346_Unlock);
	if ((tp->mac_version == RTL_GIGA_MAC_VER_01) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_02) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_03) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_04))
		RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_set_rx_max_size(ioaddr, tp->rx_buf_sz);

	if ((tp->mac_version == RTL_GIGA_MAC_VER_01) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_02) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_03) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_04))
		rtl_set_rx_tx_config_registers(tp);

	tp->cp_cmd |= rtl_rw_cpluscmd(ioaddr) | PCIMulRW;

	if ((tp->mac_version == RTL_GIGA_MAC_VER_02) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_03)) {
		dprintk("Set MAC Reg C+CR Offset 0xE0. "
			"Bit-3 and bit-14 MUST be 1\n");
		tp->cp_cmd |= (1 << 14);
	}

	RTL_W16(CPlusCmd, tp->cp_cmd);

	rtl8169_set_magic_reg(ioaddr, tp->mac_version);

	/*
	 * Undocumented corner. Supposedly:
	 * (TxTimer << 12) | (TxPackets << 8) | (RxTimer << 4) | RxPackets
	 */
	RTL_W16(IntrMitigate, 0x0000);

	rtl_set_rx_tx_desc_registers(tp, ioaddr);

	if ((tp->mac_version != RTL_GIGA_MAC_VER_01) &&
	    (tp->mac_version != RTL_GIGA_MAC_VER_02) &&
	    (tp->mac_version != RTL_GIGA_MAC_VER_03) &&
	    (tp->mac_version != RTL_GIGA_MAC_VER_04)) {
		RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);
		rtl_set_rx_tx_config_registers(tp);
	}

	RTL_W8(Cfg9346, Cfg9346_Lock);

	/* Initially a 10 us delay. Turned it into a PCI commit. - FR */
	RTL_R8(IntrMask);

	RTL_W32(RxMissed, 0);

	rtl_set_rx_mode(dev);

	/* no early-rx interrupts */
	RTL_W16(MultiIntr, RTL_R16(MultiIntr) & 0xF000);

	/* Enable all known interrupts by setting the interrupt mask. */
	RTL_W16(IntrMask, tp->intr_event);
}

static void rtl_tx_performance_tweak(struct pci_dev *pdev, u16 force)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);
	int cap = tp->pcie_cap;

	if (cap) {
		u16 ctl;

		pci_read_config_word(pdev, cap + PCI_EXP_DEVCTL, &ctl);
		ctl = (ctl & ~PCI_EXP_DEVCTL_READRQ) | force;
		pci_write_config_word(pdev, cap + PCI_EXP_DEVCTL, ctl);
	}
}

static void rtl_csi_access_enable(void __iomem *ioaddr)
{
	u32 csi;

	csi = rtl_csi_read(ioaddr, 0x070c) & 0x00ffffff;
	rtl_csi_write(ioaddr, 0x070c, csi | 0x27000000);
}

struct ephy_info {
	unsigned int offset;
	u16 mask;
	u16 bits;
};

static void rtl_ephy_init(void __iomem *ioaddr, struct ephy_info *e, int len)
{
	u16 w;

	while (len-- > 0) {
		w = (rtl_ephy_read(ioaddr, e->offset) & ~e->mask) | e->bits;
		rtl_ephy_write(ioaddr, e->offset, w);
		e++;
	}
}

static void rtl_disable_clock_request(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct rtl8169_private *tp = netdev_priv(dev);
	int cap = tp->pcie_cap;

	if (cap) {
		u16 ctl;

		pci_read_config_word(pdev, cap + PCI_EXP_LNKCTL, &ctl);
		ctl &= ~PCI_EXP_LNKCTL_CLKREQ_EN;
		pci_write_config_word(pdev, cap + PCI_EXP_LNKCTL, ctl);
	}
}

#define R8168_CPCMD_QUIRK_MASK (\
	EnableBist | \
	Mac_dbgo_oe | \
	Force_half_dup | \
	Force_rxflow_en | \
	Force_txflow_en | \
	Cxpl_dbg_sel | \
	ASF | \
	PktCntrDisable | \
	Mac_dbgo_sel)

static void rtl_hw_start_8168bb(void __iomem *ioaddr, struct pci_dev *pdev)
{
	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);

	rtl_tx_performance_tweak(pdev,
		(0x5 << MAX_READ_REQUEST_SHIFT) | PCI_EXP_DEVCTL_NOSNOOP_EN);
}

static void rtl_hw_start_8168bef(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_hw_start_8168bb(ioaddr, pdev);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	RTL_W8(Config4, RTL_R8(Config4) & ~(1 << 0));
}

static void __rtl_hw_start_8168cp(void __iomem *ioaddr, struct pci_dev *pdev)
{
	RTL_W8(Config1, RTL_R8(Config1) | Speed_down);

	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	rtl_disable_clock_request(pdev);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8168cp_1(void __iomem *ioaddr, struct pci_dev *pdev)
{
	static struct ephy_info e_info_8168cp[] = {
		{ 0x01, 0,	0x0001 },
		{ 0x02, 0x0800,	0x1000 },
		{ 0x03, 0,	0x0042 },
		{ 0x06, 0x0080,	0x0000 },
		{ 0x07, 0,	0x2000 }
	};

	rtl_csi_access_enable(ioaddr);

	rtl_ephy_init(ioaddr, e_info_8168cp, ARRAY_SIZE(e_info_8168cp));

	__rtl_hw_start_8168cp(ioaddr, pdev);
}

static void rtl_hw_start_8168cp_2(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_csi_access_enable(ioaddr);

	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8168cp_3(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_csi_access_enable(ioaddr);

	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	/* Magic. */
	RTL_W8(DBG_REG, 0x20);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8168c_1(void __iomem *ioaddr, struct pci_dev *pdev)
{
	static struct ephy_info e_info_8168c_1[] = {
		{ 0x02, 0x0800,	0x1000 },
		{ 0x03, 0,	0x0002 },
		{ 0x06, 0x0080,	0x0000 }
	};

	rtl_csi_access_enable(ioaddr);

	RTL_W8(DBG_REG, 0x06 | FIX_NAK_1 | FIX_NAK_2);

	rtl_ephy_init(ioaddr, e_info_8168c_1, ARRAY_SIZE(e_info_8168c_1));

	__rtl_hw_start_8168cp(ioaddr, pdev);
}

static void rtl_hw_start_8168c_2(void __iomem *ioaddr, struct pci_dev *pdev)
{
	static struct ephy_info e_info_8168c_2[] = {
		{ 0x01, 0,	0x0001 },
		{ 0x03, 0x0400,	0x0220 }
	};

	rtl_csi_access_enable(ioaddr);

	rtl_ephy_init(ioaddr, e_info_8168c_2, ARRAY_SIZE(e_info_8168c_2));

	__rtl_hw_start_8168cp(ioaddr, pdev);
}

static void rtl_hw_start_8168c_3(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_hw_start_8168c_2(ioaddr, pdev);
}

static void rtl_hw_start_8168c_4(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_csi_access_enable(ioaddr);

	__rtl_hw_start_8168cp(ioaddr, pdev);
}

static void rtl_hw_start_8168d(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_csi_access_enable(ioaddr);

	rtl_disable_clock_request(pdev);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R8168_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8168(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	struct pci_dev *pdev = tp->pci_dev;

	RTL_W8(Cfg9346, Cfg9346_Unlock);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_set_rx_max_size(ioaddr, tp->rx_buf_sz);

	tp->cp_cmd |= RTL_R16(CPlusCmd) | PktCntrDisable | INTT_1;

	RTL_W16(CPlusCmd, tp->cp_cmd);

	RTL_W16(IntrMitigate, 0x5151);

	/* Work around for RxFIFO overflow. */
	if (tp->mac_version == RTL_GIGA_MAC_VER_11) {
		tp->intr_event |= RxFIFOOver | PCSTimeout;
		tp->intr_event &= ~RxOverflow;
	}

	rtl_set_rx_tx_desc_registers(tp, ioaddr);

	rtl_set_rx_mode(dev);

	RTL_W32(TxConfig, (TX_DMA_BURST << TxDMAShift) |
		(InterFrameGap << TxInterFrameGapShift));

	RTL_R8(IntrMask);

	switch (tp->mac_version) {
	case RTL_GIGA_MAC_VER_11:
		rtl_hw_start_8168bb(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_12:
	case RTL_GIGA_MAC_VER_17:
		rtl_hw_start_8168bef(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_18:
		rtl_hw_start_8168cp_1(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_19:
		rtl_hw_start_8168c_1(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_20:
		rtl_hw_start_8168c_2(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_21:
		rtl_hw_start_8168c_3(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_22:
		rtl_hw_start_8168c_4(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_23:
		rtl_hw_start_8168cp_2(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_24:
		rtl_hw_start_8168cp_3(ioaddr, pdev);
	break;

	case RTL_GIGA_MAC_VER_25:
	case RTL_GIGA_MAC_VER_26:
	case RTL_GIGA_MAC_VER_27:
		rtl_hw_start_8168d(ioaddr, pdev);
	break;

	default:
		printk(KERN_ERR PFX "%s: unknown chipset (mac_version = %d).\n",
			dev->name, tp->mac_version);
	break;
	}

	RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

	RTL_W8(Cfg9346, Cfg9346_Lock);

	RTL_W16(MultiIntr, RTL_R16(MultiIntr) & 0xF000);

	RTL_W16(IntrMask, tp->intr_event);
}

#define R810X_CPCMD_QUIRK_MASK (\
	EnableBist | \
	Mac_dbgo_oe | \
	Force_half_dup | \
	Force_rxflow_en | \
	Force_txflow_en | \
	Cxpl_dbg_sel | \
	ASF | \
	PktCntrDisable | \
	PCIDAC | \
	PCIMulRW)

static void rtl_hw_start_8102e_1(void __iomem *ioaddr, struct pci_dev *pdev)
{
	static struct ephy_info e_info_8102e_1[] = {
		{ 0x01,	0, 0x6e65 },
		{ 0x02,	0, 0x091f },
		{ 0x03,	0, 0xc2f9 },
		{ 0x06,	0, 0xafb5 },
		{ 0x07,	0, 0x0e00 },
		{ 0x19,	0, 0xec80 },
		{ 0x01,	0, 0x2e65 },
		{ 0x01,	0, 0x6e65 }
	};
	u8 cfg1;

	rtl_csi_access_enable(ioaddr);

	RTL_W8(DBG_REG, FIX_NAK_1);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W8(Config1,
	       LEDS1 | LEDS0 | Speed_down | MEMMAP | IOMAP | VPD | PMEnable);
	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	cfg1 = RTL_R8(Config1);
	if ((cfg1 & LEDS0) && (cfg1 & LEDS1))
		RTL_W8(Config1, cfg1 & ~LEDS0);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R810X_CPCMD_QUIRK_MASK);

	rtl_ephy_init(ioaddr, e_info_8102e_1, ARRAY_SIZE(e_info_8102e_1));
}

static void rtl_hw_start_8102e_2(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_csi_access_enable(ioaddr);

	rtl_tx_performance_tweak(pdev, 0x5 << MAX_READ_REQUEST_SHIFT);

	RTL_W8(Config1, MEMMAP | IOMAP | VPD | PMEnable);
	RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd) & ~R810X_CPCMD_QUIRK_MASK);
}

static void rtl_hw_start_8102e_3(void __iomem *ioaddr, struct pci_dev *pdev)
{
	rtl_hw_start_8102e_2(ioaddr, pdev);

	rtl_ephy_write(ioaddr, 0x03, 0xc2f9);
}

static void rtl_hw_start_8101(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	struct pci_dev *pdev = tp->pci_dev;

	if ((tp->mac_version == RTL_GIGA_MAC_VER_13) ||
	    (tp->mac_version == RTL_GIGA_MAC_VER_16)) {
		int cap = tp->pcie_cap;

		if (cap) {
			pci_write_config_word(pdev, cap + PCI_EXP_DEVCTL,
					      PCI_EXP_DEVCTL_NOSNOOP_EN);
		}
	}

	switch (tp->mac_version) {
	case RTL_GIGA_MAC_VER_07:
		rtl_hw_start_8102e_1(ioaddr, pdev);
		break;

	case RTL_GIGA_MAC_VER_08:
		rtl_hw_start_8102e_3(ioaddr, pdev);
		break;

	case RTL_GIGA_MAC_VER_09:
		rtl_hw_start_8102e_2(ioaddr, pdev);
		break;
	}

	RTL_W8(Cfg9346, Cfg9346_Unlock);

	RTL_W8(EarlyTxThres, EarlyTxThld);

	rtl_set_rx_max_size(ioaddr, tp->rx_buf_sz);

	tp->cp_cmd |= rtl_rw_cpluscmd(ioaddr) | PCIMulRW;

	RTL_W16(CPlusCmd, tp->cp_cmd);

	RTL_W16(IntrMitigate, 0x0000);

	rtl_set_rx_tx_desc_registers(tp, ioaddr);

	RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);
	rtl_set_rx_tx_config_registers(tp);

	RTL_W8(Cfg9346, Cfg9346_Lock);

	RTL_R8(IntrMask);

	rtl_set_rx_mode(dev);

	RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

	RTL_W16(MultiIntr, RTL_R16(MultiIntr) & 0xf000);

	RTL_W16(IntrMask, tp->intr_event);
}

static int rtl8169_change_mtu(struct net_device *dev, int new_mtu)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	int ret = 0;

	if (new_mtu < ETH_ZLEN || new_mtu > SafeMtu)
		return -EINVAL;

	dev->mtu = new_mtu;

	if (!netif_running(dev))
		goto out;

	rtl8169_down(dev);

	rtl8169_set_rxbufsize(tp, dev);

	ret = rtl8169_init_ring(dev);
	if (ret < 0)
		goto out;

	napi_enable(&tp->napi);

	rtl_hw_start(dev);

	rtl8169_request_timer(dev);

out:
	return ret;
}

static inline void rtl8169_make_unusable_by_asic(struct RxDesc *desc)
{
	desc->addr = cpu_to_le64(0x0badbadbadbadbadull);
	desc->opts1 &= ~cpu_to_le32(DescOwn | RsvdMask);
}

static void rtl8169_free_rx_skb(struct rtl8169_private *tp,
				struct sk_buff **sk_buff, struct RxDesc *desc)
{
	struct pci_dev *pdev = tp->pci_dev;

	pci_unmap_single(pdev, le64_to_cpu(desc->addr), tp->rx_buf_sz,
			 PCI_DMA_FROMDEVICE);
	dev_kfree_skb(*sk_buff);
	*sk_buff = NULL;
	rtl8169_make_unusable_by_asic(desc);
}

static inline void rtl8169_mark_to_asic(struct RxDesc *desc, u32 rx_buf_sz)
{
	u32 eor = le32_to_cpu(desc->opts1) & RingEnd;

	desc->opts1 = cpu_to_le32(DescOwn | eor | rx_buf_sz);
}

static inline void rtl8169_map_to_asic(struct RxDesc *desc, dma_addr_t mapping,
				       u32 rx_buf_sz)
{
	desc->addr = cpu_to_le64(mapping);
	wmb();
	rtl8169_mark_to_asic(desc, rx_buf_sz);
}

static struct sk_buff *rtl8169_alloc_rx_skb(struct pci_dev *pdev,
					    struct net_device *dev,
					    struct RxDesc *desc, int rx_buf_sz,
					    unsigned int align)
{
	struct sk_buff *skb;
	dma_addr_t mapping;
	unsigned int pad;

	pad = align ? align : NET_IP_ALIGN;

	skb = netdev_alloc_skb(dev, rx_buf_sz + pad);
	if (!skb)
		goto err_out;

	skb_reserve(skb, align ? ((pad - 1) & (unsigned long)skb->data) : pad);

	mapping = pci_map_single(pdev, skb->data, rx_buf_sz,
				 PCI_DMA_FROMDEVICE);

	rtl8169_map_to_asic(desc, mapping, rx_buf_sz);
out:
	return skb;

err_out:
	rtl8169_make_unusable_by_asic(desc);
	goto out;
}

static void rtl8169_rx_clear(struct rtl8169_private *tp)
{
	unsigned int i;

	for (i = 0; i < NUM_RX_DESC; i++) {
		if (tp->Rx_skbuff[i]) {
			rtl8169_free_rx_skb(tp, tp->Rx_skbuff + i,
					    tp->RxDescArray + i);
		}
	}
}

static u32 rtl8169_rx_fill(struct rtl8169_private *tp, struct net_device *dev,
			   u32 start, u32 end)
{
	u32 cur;

	for (cur = start; end - cur != 0; cur++) {
		struct sk_buff *skb;
		unsigned int i = cur % NUM_RX_DESC;

		WARN_ON((s32)(end - cur) < 0);

		if (tp->Rx_skbuff[i])
			continue;

		skb = rtl8169_alloc_rx_skb(tp->pci_dev, dev,
					   tp->RxDescArray + i,
					   tp->rx_buf_sz, tp->align);
		if (!skb)
			break;

		tp->Rx_skbuff[i] = skb;
	}
	return cur - start;
}

static inline void rtl8169_mark_as_last_descriptor(struct RxDesc *desc)
{
	desc->opts1 |= cpu_to_le32(RingEnd);
}

static void rtl8169_init_ring_indexes(struct rtl8169_private *tp)
{
	tp->dirty_tx = tp->dirty_rx = tp->cur_tx = tp->cur_rx = 0;
}

static int rtl8169_init_ring(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	rtl8169_init_ring_indexes(tp);

	memset(tp->tx_skb, 0x0, NUM_TX_DESC * sizeof(struct ring_info));
	memset(tp->Rx_skbuff, 0x0, NUM_RX_DESC * sizeof(struct sk_buff *));

	if (rtl8169_rx_fill(tp, dev, 0, NUM_RX_DESC) != NUM_RX_DESC)
		goto err_out;

	rtl8169_mark_as_last_descriptor(tp->RxDescArray + NUM_RX_DESC - 1);

	return 0;

err_out:
	rtl8169_rx_clear(tp);
	return -ENOMEM;
}

static void rtl8169_unmap_tx_skb(struct pci_dev *pdev, struct ring_info *tx_skb,
				 struct TxDesc *desc)
{
	unsigned int len = tx_skb->len;

	pci_unmap_single(pdev, le64_to_cpu(desc->addr), len, PCI_DMA_TODEVICE);
	desc->opts1 = 0x00;
	desc->opts2 = 0x00;
	desc->addr = 0x00;
	tx_skb->len = 0;
}

static void rtl8169_tx_clear(struct rtl8169_private *tp)
{
	unsigned int i;

	for (i = tp->dirty_tx; i < tp->dirty_tx + NUM_TX_DESC; i++) {
		unsigned int entry = i % NUM_TX_DESC;
		struct ring_info *tx_skb = tp->tx_skb + entry;
		unsigned int len = tx_skb->len;

		if (len) {
			struct sk_buff *skb = tx_skb->skb;

			rtl8169_unmap_tx_skb(tp->pci_dev, tx_skb,
					     tp->TxDescArray + entry);
			if (skb) {
				dev_kfree_skb(skb);
				tx_skb->skb = NULL;
			}
			tp->dev->stats.tx_dropped++;
		}
	}
	tp->cur_tx = tp->dirty_tx = 0;
}

static void rtl8169_schedule_work(struct net_device *dev, work_func_t task)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	PREPARE_DELAYED_WORK(&tp->task, task);
	schedule_delayed_work(&tp->task, 4);
}

static void rtl8169_wait_for_quiescence(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	synchronize_irq(dev->irq);

	/* Wait for any pending NAPI task to complete */
	napi_disable(&tp->napi);

	rtl8169_irq_mask_and_ack(ioaddr);

	tp->intr_mask = 0xffff;
	RTL_W16(IntrMask, tp->intr_event);
	napi_enable(&tp->napi);
}

static void rtl8169_reinit_task(struct work_struct *work)
{
	struct rtl8169_private *tp =
		container_of(work, struct rtl8169_private, task.work);
	struct net_device *dev = tp->dev;
	int ret;

	rtnl_lock();

	if (!netif_running(dev))
		goto out_unlock;

	rtl8169_wait_for_quiescence(dev);
	rtl8169_close(dev);

	ret = rtl8169_open(dev);
	if (unlikely(ret < 0)) {
		if (net_ratelimit() && netif_msg_drv(tp)) {
			printk(KERN_ERR PFX "%s: reinit failure (status = %d)."
			       " Rescheduling.\n", dev->name, ret);
		}
		rtl8169_schedule_work(dev, rtl8169_reinit_task);
	}

out_unlock:
	rtnl_unlock();
}

static void rtl8169_reset_task(struct work_struct *work)
{
	struct rtl8169_private *tp =
		container_of(work, struct rtl8169_private, task.work);
	struct net_device *dev = tp->dev;

	rtnl_lock();

	if (!netif_running(dev))
		goto out_unlock;

	rtl8169_wait_for_quiescence(dev);

	rtl8169_rx_interrupt(dev, tp, tp->mmio_addr, ~(u32)0);
	rtl8169_tx_clear(tp);

	if (tp->dirty_rx == tp->cur_rx) {
		rtl8169_init_ring_indexes(tp);
		rtl_hw_start(dev);
		netif_wake_queue(dev);
		rtl8169_check_link_status(dev, tp, tp->mmio_addr);
	} else {
		if (net_ratelimit() && netif_msg_intr(tp)) {
			printk(KERN_EMERG PFX "%s: Rx buffers shortage\n",
			       dev->name);
		}
		rtl8169_schedule_work(dev, rtl8169_reset_task);
	}

out_unlock:
	rtnl_unlock();
}

static void rtl8169_tx_timeout(struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);

	rtl8169_hw_reset(tp->mmio_addr);

	/* Let's wait a bit while any (async) irq lands on */
	rtl8169_schedule_work(dev, rtl8169_reset_task);
}

static int rtl8169_xmit_frags(struct rtl8169_private *tp, struct sk_buff *skb,
			      u32 opts1)
{
	struct skb_shared_info *info = skb_shinfo(skb);
	unsigned int cur_frag, entry;
	struct TxDesc * uninitialized_var(txd);

	entry = tp->cur_tx;
	for (cur_frag = 0; cur_frag < info->nr_frags; cur_frag++) {
		skb_frag_t *frag = info->frags + cur_frag;
		dma_addr_t mapping;
		u32 status, len;
		void *addr;

		entry = (entry + 1) % NUM_TX_DESC;

		txd = tp->TxDescArray + entry;
		len = frag->size;
		addr = ((void *) page_address(frag->page)) + frag->page_offset;
		mapping = pci_map_single(tp->pci_dev, addr, len, PCI_DMA_TODEVICE);

		/* anti gcc 2.95.3 bugware (sic) */
		status = opts1 | len | (RingEnd * !((entry + 1) % NUM_TX_DESC));

		txd->opts1 = cpu_to_le32(status);
		txd->addr = cpu_to_le64(mapping);

		tp->tx_skb[entry].len = len;
	}

	if (cur_frag) {
		tp->tx_skb[entry].skb = skb;
		txd->opts1 |= cpu_to_le32(LastFrag);
	}

	return cur_frag;
}

static inline u32 rtl8169_tso_csum(struct sk_buff *skb, struct net_device *dev)
{
	if (dev->features & NETIF_F_TSO) {
		u32 mss = skb_shinfo(skb)->gso_size;

		if (mss)
			return LargeSend | ((mss & MSSMask) << MSSShift);
	}
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		const struct iphdr *ip = ip_hdr(skb);

		if (ip->protocol == IPPROTO_TCP)
			return IPCS | TCPCS;
		else if (ip->protocol == IPPROTO_UDP)
			return IPCS | UDPCS;
		WARN_ON(1);	/* we need a WARN() */
	}
	return 0;
}

static netdev_tx_t rtl8169_start_xmit(struct sk_buff *skb,
				      struct net_device *dev)
{
	struct rtl8169_private *tp = netdev_priv(dev);
	unsigned int frags, entry = tp->cur_tx % NUM_TX_DESC;
	struct TxDesc *txd = tp->TxDescArray + entry;
	void __iomem *ioaddr = tp->mmio_addr;
	dma_addr_t mapping;
	u32 status, len;
	u32 opts1;

	if (unlikely(TX_BUFFS_AVAIL(tp) < skb_shinfo(skb)->nr_frags)) {
		if (netif_msg_drv(tp)) {
			printk(KERN_ERR
			       "%s: BUG! Tx Ring full when queue awake!\n",
			       dev->name);
		}
		goto err_stop;
	}

	if (unlikely(le32_to_cpu(txd->opts1) & DescOwn))
		goto err_stop;

	opts1 = DescOwn | rtl8169_tso_csum(skb, dev);

	frags = rtl8169_xmit_.c: R(tp, skb, opts1);
	if (/8101) {
		len =erne_headlen(skbr.
 	drive |= FirstFrag;
	} elseight (c) 2002 ->lenhen@realtek.com.tw>
 * | Laromieu@Co	tp->tx_skb[entry].skb03 - 2 Cop

	mapping = pci_map_single(ieu@e redevh002 ->data, len, PCI_DMA_TODEVICEr 816il.compyrt (c) 200 2003 7 Frantxd->addr = cpu_to_le64(too. Plchen <linreal2odule.h>
#in32(Tek  816tx_vlan_tag et* See) contwmb( cont/* anti gcc 2.95.3 bugware (sic) */
	statuRealois Romincl | (RingEnd * !(((c) 2 + 1) % NUM_TX_DESC/netm.h>
#para1includeclude <lvlanin contact cur_tx += /8101e+ 1 816smp_vic#incluRTL_W8(TxPoll, NPQ);nux/set poll Plebitx/if
 *
 *TX_BUFFS_AVAIL(tp) < MAX_SKB_FRAGSrmationetif_stop_queue( * r81	h>
#irx/in.h	inux/if_vlaninux/init.h>>=nclude <asm/io.
	a-too. Pwake
inux/if_vlaasplk.
 turn NETDEVasm/OK;

errng.h>:
fine RT81699_VERSION "2.3dev->vlans.tx_dropped++;-NAPI"
#define MODUBUSY;
}

<asm/c voidlinux/pcipciE "rinterrupt(struca-map
#inice *ude 
{f_vlfaile				\
		private *tp =ed!
 * __FILION "2.3n",xpr,k.>
#in *p( "A=  wore dprin;
	)) {	__iomem *ioux/h>
#ieu@mmio_BUG ;
	u16ne dp<asm/i,hile cmd 816k.
 read_config_word((fmtS fileCOMMAND, &ene assnclussert(expr) do {} while (0)ne aSTATUS( "Ak(f <linux/if_#inctoo. Pmsg( "Ar#incrmatio(fmt,k(KERN_ERR
		 _MSG_P"%s:)
#d error (cmd = 0x%04x, <asm/io.hdefiIF).\n",TIFMSG_PRR "

#namrighne ass
	(tp- <linux/i3LK-N/*
	 * Thk.
 covery sequence below admits a  1)

elaborated explanation: 
 * - it seems torgs..;vs. Rx-I did noll-mu whatormaticould be done  >cur_Rit makes iop3xx happy.vs. vs. RFeel freeicasadjustultiyour needs
e <l/ssertRle (->broken_parityx +lude 

#dne ass &= ~ne assertdp_PARITY;
	rmatAC_ADDR_LEN|=if /*ine MAX_SERR | MAC X_FIFO_TREAD_REQrgs...write {} while (0)
#endif /*ine MAXriDEBUmt, arhold, Rx buffer level before fm.tw RTL816SHIFT	1<asm/io& (CI burst, _DETECTEs NO thr |
		ssert<linMASIG_SYSTEM| NEOSH	7	/* burst, REC_MASTER_ABORTimum filne TX_DMThldTARGET/* 0x3F ssertEarlyTx'6' nsmitnclu#demii.de/->inuxinfamous DAC f*ckup onlyRC. *ens at boot time

/* MAC rrgs.cp12
#d	)
#dDAC) && !ieu@dirty_rxe shortes/io.rxx/dma-ma R 8169SG_PDEFAULT \ne as_BUFFS_DRV |INFOROBE |disabsm/iodefinACCI bux/init.h>\ncluds I"AssFrame6
. acDACcludnux/i16(CPlusCt onor registencludddresfeaturesrs */

#de_F_HIGHDMA2._DESCAnux/pcihw_reset(DEBUG t PCIE	1536	/scheh>
#le (kif_v,		#do {,_re>
#i_task)
 *
 *!	do {} pri	#expr,_txDEFAs...)ion",	\
	d! %s,NUM_RXlin0x1c		 
#defin	(NUM_TXNE__E__,__f* sizeo{TIF_MSG_DRV |er size=%d\unsigned int t le btx, tx_leftsserY_TIMEOUPFX fmtefine RT
	}
systeminclu(10*HZ)L_EEPRO/io.hcr-Hefine RTallyv0)
#en29first > 0_REGS_SIZEst, '9_PHtool.h=_SIG_MASKlude <asm/io.h<asm/(structinefinfo *informL_EEPRO RTL_E_+'6' ADcludu32clude=	(10-03 -ncludecal8)<asm/isser	e32(0x81i<asm/io.h_vlanclucpuns InTxDescArray (c) 200realtchen_SIZE024 ... a(regOwn9_NAPbreak16(reAIL(tifdefTL_Ebytesrc32g))
#dereg))
#define packetsBUG
l ((nux/pciunspecdr + (ns Ine dprin* readw * Numbdw (eg)if /)	Rx bubt PCI, val32		((uns.zoreil.rmatioadb _knthe dw (ieadw (eshucranc129)nightRTL = NULLreg,}t(expine RTdefi#_MAC. ac--<linux/ (reEPROM_SIG		c !R	0x0000

rmatio_GIGA_MAC_VER02 = 0x02,MMIO sysux/in.hS, val		25 */
ince PFXpedif_vl &&X_BUFFSON "2.3m/io <linux/if_vlaTL_GIrqPFX M_R32(r) MAC _EEPROMVERSION "2.3TL_GI - t
 * 8168 hack:MSG_IFD r* Masf mure lost when the Tx R32(regx07,8110SCtoo close. Let's kick an extra	RTL_GIGA_07 = 102e
	ane TX_8 = 0xofL_GIrt9/816 activity is detecsses(if
	RTisips ual8)9S
	lude * 0x0911snumbenough). -- F*/
#d/* 3 0x03g, val16)x/if val8)E_GIG_02P | N02, S
	Rnclude <asm/io.h>p.hlinu(struct TxinC_VEsucuct RxDesc/810mentedC_VEme(* MAC incl#defidefine ) readl (i(u <romieu@froaddr + )) // x11, // 8101EcNE   _GIof(struct Tx01 ?
	RDesc))
#definrx_csumING_BYTEsk__BUR *DR_LENregistRL_R32DESCscrst PC32/delay. ((val16), ioae
	Rux/if_v14 = 8101 ?
	Rw ((v_MAC& RxProtoMasreadw mean) readl ==x15ER_17TCPh

#de(_21P | NTCPFail)) ||
x/pciS
	RTL_GIGA_20d, //UDER_17 =68C0, // UDx17, // 81683P | N17GIGA_MAC_VPNE  IGIGA_MAC_VER_24 S
	RTL_G01EbSee Mip_summeUP |CHECKSUM_UNNECESSARREQUEST_SHI, // 81686CP
	RTaGIGA_MAC_NONE68Bf0, // 81
	RTL_Gbool(reg)t PCIre */_copyAC_VER_2E,MAC,MAS*E,MAC,M RTL816 MASK fon faileR (reg))*/
#d 8101 ?
	Gpkt_TL81Mask =MASK }}dma args_t HZ .name version = MAC, .C_V;
	 \
	{ble  = falseVER_24 Ghar *namif_vE, .macioaddIGA_goto outruct rted0definLIallocc_versoadBYTESnclu}lTek+t {
_IP_ALIGNchen (re!io.h
	_R("GIGA_MA",e dprma_syncct	RTLi_for 0x14x/modr + (regBUG 880)ER_17 e;
	u8 mac
#de forFROMupport co,		RRx Burve<shu,9A_MAC_VER_02,s",st, info_from__MACar_AINT(RxC) do M MAC_V 816AI */
} rtAC_VSB
	_R("ot of peo0S
	_edtrue;
out:MAC,MASK) le ba_R(NAME,MAC,uct RxDescrZE		256RX_RING_7e188	(ude Rlinux/  RTL81

static const struct {
	const819_TX_TIMEOUT	(6*HZ)
, / 81budgetGA_MAC_VER_07ROM_SZE		25,hip_
#de

RTL_GIGA_MACPHYel/811coun169",	E		25 8101 ?
	EEr03 =08, 0xf, // M_RIFDOWN)+13 = A_MAC_r_R("9 = 0x09,9 0xfff7e1min(9, 0xff,MAC_VER_L_GI.SG_P;_9, 0xfff)
#AC_VER_02--,"RTL8TL+69
	_GIGA_MAC_	_R("RTLIFT	1	IGA_MAC/if_vlER_17PCMMIO_19 = 0x13GIGA_MAC_L_EEPROA_MAC__R32(		((ne Rne assert ?
	Rr + (glong)16		((unsw_VER_20 = 0x14GIGA_MAC_VER_2_GIGA_M readl (il_VER_232= 0x14x/mo long)unlikelc_ve)	writeRxRE8 = 0xd
	SIZE		256
#defrx_er R8169	(NETAPI_WEIGHT	64.name;
	u8 mac_vene NURx  ... -E
	_IFDOWN%08xI bursBde <linux/init.h>\
	efine MTX_x06, /)

e 0xffdr MAC,MAorct {
_VER_00RTL_ // 81RxRWR_11RxRUNT632),  = 0eIGA_MAC_GIlengthS
	RTL_GIGA_16A_MAC_VERA_MARxCRCGIGA_M8cp/8111cp",	RcrcMAC_VER_18,RTL81681880), ER_1FOVF_18,5, 0ct {
	conL_GIGT06, 0xff7e1880), /<lins%s,lTL816fE
	_R("RTL8168c/81fifoA_MAC_VER_18, _VER_cp",R16(markncluasicb/811fine RrxMAC,Msz, // 6ormation.
ge;
	;ine Clears	RTLA_MAC_VERC_NOuff (c) 20 0xffrsion); }32 RxC_GIGA64_19, 0xff7ans NTL81 *_VERt c */
} rt =ff7e1880),  = 0x01FFF) - 4L_GIGirst PCI xfer_DEBU
#defiold, 0), /do_GIG17 =1
 * inuxL_W32( doe1
	RTLs10S
	_RincomlinuR_18,5RTL_RTL_G-E0f, /s.E
	_yER_0 se02e
 mulsymptom_GIG- 1)-mtuf7e1880} rtdcp",	RTLf7e18817 =MAC_VER_19, 0xA_MAC_VER_18,, 0xfx), ///A_MAC_T",	RTL, 08cp/8111cp",	RL_GIGer si
#
	_R("RTL8168c/81/* Clears theGIGA_MAcsion {
	RTL_CFG_0 = 2E
	_R("RTL8168d,	RTL_G-E		cludinvoid8cRTL811,
	RTL_CFGIGA_MAC_sb",MAC_V19, 0x (reg.p) \= 0xAMip_info_&ct {
	c_09 = 0x09,("RTLo0), // P_VER_19, 0xff723 = 0A_MAUM_RX_)
#endiRTL_Gl_hw_de <lxpr)) 1(struct net10evice("R00,sion =CFG_1 PCI_DEVICE2
};
32;

xpr)) {lTek_hwA_MAC_VER_02rt_81 // PC/ 8169rTEK,	0n fai_8101(struct nctse re	lTek 8169e retbl[] =ight{ // 8168Ctart_8101(stGIGA_{ PCI_ failedR_07putTEK,	00s,
	RTL_CFG_2 // 0p,	0x8ol =e <l_type_tranLTEK,	0x8e <a	/* Rsta },
	68(rL_GIGA_01, 0xpr)) {	FDOWN)< 0GIGA_Mtoo. Preceiva), /rRTL_GI16, 0xp/8111cp",	R",	RT8(re */
} rt void { PCI_DEVICE(R32(reg)efix8167),/* Work around -E
	AMD platximum. rtl_hTL_GIGIGA_MAC_V2 &linux/if_vlanrt_8fe000)04GIGA_MAC9SEPRO,0, RTL_ion 8168 {
	RTL_CFG_0 = 05t pcista_DEVICE0 },
= 0file 
	RTL_GICI-E
, fail_VER_tl_hA_MAC_-VENDOR_ID_LINKE
	_R("RTL,
	{ 0x00*/
#d{ 0xea					\
		 rtl_llE(fileVR32(tesY_TIME*/fine Re("RTL8sIGA_MAC 0, RT&&C_VER_32;
TL_GIGCI-E
	_R("RTL8NAxff7e1880), // P11, NUnoR_07art_erDEVICEessember_05,Tx descripA_MAC_VER_001+=
	{ 0xally exprconIXME: untilon {reTL_Gperiodic2 = 3rulti_ADDand re0,}ion {
er *,vs. Ra tempora)

/defiage may def10SCely k,
	MAR0	Rx proces 32;

R("RT<lineon {
0 = .ncludMAC4		aDesc)an,MAC,fGA_MACdb, // x0a,gainvs. R  after	= 4= 0x?rHigh		how whio* SesAC_VER_0AC4	AddrTisMAC0diter  (Uh oh, 0xdrHig* 3 meaE,MAC,MASK)m rt111cp",	RTL_G=69S
	RTL_GIG // 8x .macioadd =03 -;ALTEK,	09_PHuse_EMERGMAC_VER_0ctights exhaus_enable;
} debuleas{MAC,MASK)

enum _R(NAME,MACrqdefine_uct RxDesc8169_RX_RI8168irq,_0 = 0ESC _i	0x8nce#defi
#definS	(NUM_RX_DESC 	RTL) do 3		= 0pu_tct {
	t RxDesc))

#defineuncGIGA_MANE__);expr,}9_TX_TIMEOUT	(6*HZ)
L_EEPRO, ##ER_24); fig2	gh	= 0x1a { PCInburst,		RTL/* looptrMitilinu8169_RX_Rs,ine Etwe have_VENnew_TIMs orvs. R8,
	h	=  invalid/hotplug caseR		= 0x3<asm/io.hfileTL_GIntrS PCI-E
	_h>
#in3oid rtl_&11b",	RT!WN)
ffffrmatioxe4,
gate	if_vldrLoHMitig0allG_0 =168DBUFFSntMas f <rortl_hse w= 0x",		R8_GIGA_he chi<lino st_fiexitek 81w	=  voi70), 4,
	CER_19, 0x!0,
	{9)unn thef_vlYS,		0x1",	RTL_G0 = _down{
	uGA_MACAC_VER_18,4,	{ file pport};

MOENDORrx 
	{ 0+ (regSta6AddrCSIAR	56,
	x6R_1GA_MAC_VEIFOOver0x0116)bl);

32,
		PCI_A00000
#R_BYTE_IDHYstuct _R("RTn {
	R PFX MODULENAME ":m8 0x00x02tx_dresout0x0
#definD			ITE_CMD		0art_8101(struct nrtl_hSYSE), 0, 	0x1	#expr,__FIMSG_8169_RX_RITE_CMD0000000
#defybreak =	 t(exl1e",inkChgIGA_
",	RTL_GcheckA_MAk132)	wrff7e188consAR_WRITE/
0
#dee	/* =ultise a	CSIDastt ne
		PCI_ATBI_p->ine _m;
	uto8 = 0xTE_E20igno	= 8{ PCMSI(reg)ddrHiartAdhavSEARcastait_BYTA_MASK	nx0f
# even7 = ich	Counne	RxCoo		0x	GIGA_MAC_Be(NAME,MAC,MASoid rtl_hEAR_FLAG800000SHIFT	napi_defin0), // , // 811te	= },
ULEefin8
#d#defin& ~FUdefi_DAT	RTLSMAC,MAST		8
#define=ine  regiertiStaAR_ADDR_MA_19, 0xterru("RTL8168prep(&* Interr,MAC,M		__WIntBYTE_E010 (regUnavaine	EPrmatiSIZE		256
#define R8169R_15,0x3e,
	TxConfidac	= 0x3eEPHYAR_DAT
#de inf_vla",		RTL_GEPHYAR_REG_MASK			A_MAC_VER00000
#deect {
	geent	ThreIne	EPHYAR_DAT8102e
ll // PCe2	 rtl
#desourcTE_Eine	EP8_81
	feMtbst, 'cknowledged. So, PCIES	= ( Intythdefiwe'vC_VER_02nd IX_NA56,
= (11 << 21)(nb	= 0_REGA_MASK	22),
	lticDesc)blockInterll_CMD		0x80ghER_1ek 81NT	=StatK00000fD_REAenum rart_8,8136)NABLEER_2FT		12tiIntr	?InterREG_001,G_0 f
#d) := 0x0001,

	AddrFuncPx BufStrs {TE_EfuncEFu_SIZEefine IRQ_RETVAL(egister03ffR(NAME,MAC,tart0x5c,
	ollING_BYTES,
	Rx#defin/
#d0onst c};

enum 2ef(struct RxDesc))

#defineunc_CFGa = N_ofS0,
	_18,9VICEt RxDesc))

#defGIGA_0x6ctiIntr	4BYTE_E55ne	C) do rt_8101x6	CfgRxMaxSizt		= 0dane	CC	256mdts */e 0xe2,
, 0xf"RTL81
	B
	_R("BiVENDOR_I		0x0f		1pybreak =	gDMAShiF0x10,  net)AR_REG_MASSK		0xffffEVER_xCfgt, 'hift	=  8,

	/ct {
	con/meGapShFI<ts */c0,/dma-merrucompleteeptRuAK_2			8,
	Cm3)
	01,
forcAR_BYTvisibilum r* Inter	8
#defineS	= (01,
= 0x80CPUs,168dwe canIDR	seR_WR << 23),
	RxRNO thCMDd potentiters	E* Inter a reEK,	00x0aHYA << 56,
wtMiss'tHPQ		=E
	_RpomeGa_DMA_BxBufoll tl_rp",	Raf)) {sMitix6eptA1c",hipntu8000,e EtMitig*/

	_R(	= ge Swe wMAP	t		=8,
	CmdBits,
	LED	= 0xeit
	MS/HPQ		= 0x8ters
	0x1f
r56,
< 2),
8CP
	R0MAC_VER_180,		/* Poll ,
	V9 = 0erpr) nale = 0x* Forced so/
	RxCfgFts(struct TxDesc))
#definrx_missed 0xff7e1880), // 8110SCeTL8168ine RTT	(6 0xffct {
	 rx_mode_bits */
	AcceptErr	,
	PHYstatus	= 0x69(NAME,MACDR_MASK			0x>ff

	EPHYAR			= 0xK6
	RTLefine4,
	CSp/8111cp",	R Peg))AC,MASKegis{
	Ret32(RxM.27 *)), 0, rtlA_MA/GIGA_M)h	= 0Accep, 0eo

static + (reg{ .name =ne	CSNG_BYTES	(NUM_RX_DESC x54,
	Config= 0x5c,
	PHYAR		= 0x60,
	PHYstatus	= 0x6c,
	RxMaxSize	= 0xda,
	CPlusCmd	= 0xe0,
}RTL_EEPROM_Sine 0000 ... acine MAeVER_P (1 rn_en/* Codefine PFX MODULENAME "
)1e
	sP
	RT4,its */004{ PC
cddr ne	E:
	spin_Rese_irqr	= 0xReseAK_2	),	/* PMfgDMAShiCSIAR<< 5),/E		0 regigicp.27 *t = 24,,

	/y bitILoopunbackts */4
#defin 0x2BIc vohronx02,
	Acfig5 irquters {
G
	Rxa racdefihard	(1 03MHz =a, few cyclefilte= 24VER_0x64,
FOOveComplet("RTL();  /AC056,
	,sh hashx2c,
beIFOOveCompletnabl)?cpPFX ightMACAnhipswDS0		anag50k$ , // er (	_R("IRQts */14,d or1 = 0oDR		vs. Rcwo paths lead 002 (  The1)x/initt net 0x2xHD ->
#dec,7e1800000)TL_G040,l< 11
	o/ unuM8 81eurrsed
codnb	=<< 101
	Cxpl_   reserMitigr.TL81 do 1tMultic2st	= / 816detailll regi2	GA_MAC_hange",		RTktCntrdbgHz = 0Cfg934xMiss = 0bet hsued#defi4rLow	= -en 8101
Tx	PktCntrDis8169_RX_Rions102e
RTLsimpl01e
suAR_BYT/* Coown 0x36aximu		= (ALTEK,	0coNo8 8100if queueEv1ame /majorX_BUFFS_3 regi8R		= 0x36s/* Conue */
	FSWInt		.5 *RTL_CFG_PHYe <lis&&	TxFlowCtrlfig3 regi8
	RTL_GIGNwCompletBINwRe_CFG_	tx_clea R816 0x10,
	_100x10,BIReseuncEable	kSum	6 Wak
	RxChkt netING_BYTES	(NUM_RX_DESC = (1 << 4),	/* Accept Unicast wakeup frame */
	LanWak11cp",	RTL_GIGA_MAC_VER_24, 0xff7e18dCSR	upd#def_VER_g	= b	0xce godefi	_R(,rtl_",	RTL_GrightD_scOwn		=nableTBICpt MuBufEast ude <a69.ceorce_h llPhys	 (regEnd o},
	fbug =onsisnale	CSIAR_Rx/pci_06, 0xff7e18egisteREALTEK,	0x8136)< 3) = 0LaPhyA<< 5),	>
 */* Confi 29 voi*com.tw segmeTR_05,a reg))
z = 0TL_R32(reg)8,
	Cmd8/

	/TFinalrivaten8101e",R32, // PEALTEK,	0e	EPHYAR_REG_MASEALTEK,	MAC,MASK)0ake up102e
	), 0, es_ theer pod01escript_nd		SRB/* ...e	= LinkOKBIRese2sed
	Force_h Dumpne	EterCommC4			RxChe4,
	8,
	Cm3		= * LanWMaskRxMiss/M_TX_lAK_2		PMEl880),lagsn {
	RTmc{0,}ter[2]nclud1 << 30)ntrMsh  0 = 0 // PCI-_08,MAC_VER_GIGtmErr	0PCS		= ( = { tTCPC168DFF_	_R(ISC (0-7)/* Un	FLASH4,
	 reglogeptM taps (1 <<t 1/2		256
#def_1		fl,	// 8x00I_WEIGHT	64
NOTICEine NU  // scubeyoMAC_ 
	PCIMdCI bursB_17, 0xff7e1880),PCI-E
, /nTof a  =X_BUFFS
	MWFtBroadTCP/I|1 | PIDT,		x16,/Ix15, // 8yPhysF meanIDRxProtoAllail	ne	E	= (1 = 0/1_REAum failed 0/
	Uoad30), k_33MA_MAC_VAR_ADDRv->mc NISCe
> mask	RxPr failed_limit
	RT   ||AN lude*/, /* C	CouALLMULTct pcist lifoo many1
	Acksum *perfectly0d, aProtonableilUDPCS		D bit 1/x15, // IRxProto first PCI 15, // 816k	truct TxP

	IGA_MA	eg avai_BYTESum */UD4 addrDPCS		= (15/

	/*UDP/IP IX_N (c)ne	Cfg95		=mc_list *mcfailINwEn checksuSWInu_11, 0< 5),	/* Accight_orce2 drive;	u8		 		RT;D_REALTatic const ;
	u8		__pad[sizne	E01,

ickett	= gsuckig5t (cd *		= (WOLle32 && i <cket0
	Rx

enum pts1;
	i++_EATURE_M1WOL		= ->nexQ		= 0xfo00,
it_nTL_Ge* Sect n(ETH_ALENI	= (1 <->dmi= 0xe0 >> 26ne	EPum failed ate IPs>> 5],	/*nb	= (;
	__le& MSSShi,skbE
	_R(|];
};

eMask	RxPrt 1/2DDR_MA 0x020000,
	Tsave	= Res /* MulRCPCnclude/

	/et_deux/i *)} whi |a RxProtF me << 3)nb	= 6ame *C} whit Br  /*NT	=BINwE[101e"2),	et].x_m << 3t		= 088,
	Cm0ame */e
	RTdefi. Reserv8169_	RTL_.5 *	/* PM32  816esc {
	__le32 o	PCI_ {
	__le32 optsswab32(

struct RxD,
	Li

struct RxDescailepci 816 <linux/up ftersx10, +REI	= 
	__le32 oL816t_device *dev;
4n failedR_DAinlo	ct napi;
	sptx_und,17),kO0,
	TB(1 << 18),	Trestore320bpstx_un_collisio}= 0x*
	Cxp",	RTL_Gget	(1 <s - Ge1ipse
	MW )	do/Ple32 ruct _fils0x800@dev:E
	_RE* SetocoDM_RX_DtoRxFOVx pkttartA	= 0x
	__ Inag */X/RXtag *orA_BURST	ndex
	Tx
= { ruct TxNG_BYTES	(NUM_RX_pts1 de*
	u32 do	RTL_Arra+ LargeSendfc00: 12fc00ullDupIPCSle32 opts1;rge Se CalcuMask	ts2;
	_ availDesc {Desc *RxDescAx64,;	/* 256-alig2 opts2;
	_descriptoolliS PCI bE		256
 (1 << 9	= (18		_64
	u32 cu-E
	8		__pc_version;
	u32 cu-map	rx_unicaxused
	Flete	= /
	IPk/ 8110Sn_23,
	__l.
 *m mac_CFG_0 },E
	_R("cu	/* orced so& CPluter pMask		= 0xfff,     / a MaS	(NUusped! %s /* inkUple32 opts14#defiA_MAC_GIGA_MARopyriuff[Nbailed */
Contoo. P *+ (re_selchterFraHYdefine PFX MODULENAME "}g))
#defiCONFIG_PMus	= 0x02,
	FullDup		; } w
	Txr_0000UM_RX_DESC ilticine	C) do RTL_GIGA_MAC_VER_o__FI#defifai speinkSta | PID1 << 30),ble;
	4t Plethedrv/
	PIMAC_V
	_10bpsle32cmd); } w inhyRx Buf_rtAddSSle32 		= 0xf,
	RTL_CFG_1,esuG_0 = u8 auton_GIG__iospeed,signduplexr.
 *nt (*get_setcmd s)on failed! 	rx_unica,ENDOR_te <lIG_A_IFUP*r.
 x8129(*pm *);
	vdevice *, 		0xid __iotruc10AC_VER_02, 0g;p;
#endif
	atG_("RTL8VLABINwEnable("RTL8168c/8111c",	RTL_GIGA_M // 81680, ci_tbl("RT_R(w_start)(strune RTL8169_pm_ops
	RxRxChkSTek 81=8		_.d __iomM_RX_agicund __iom,
	.icas;
ULE_AUTHOR(i the GA_M	= (1880)_AUTHO= {
is RomcludhawL/io.	First crew <netpoweroDescSkernel.org>");
MODUd t__ioESCRIPTION("RealTek_REA#terAde */
/
	PIPM_OPS	(&9_"RTL__le cL8169"rmati/* !fd __iomef_dupt	= t, 0);
(expLE_REAM_LTEK, "Cndif ioaddpo__iofor copt)(struf, M_DE/* hut30), ruct naRTL_GIGA_MAC_eed, u8 duplptMulticast	= 0x04nt (*do_ioctl)(struct rt RxDesc *RxDescArray;	/* 256-aligned Rx descriptor ring */
	dma_addr_t TxPhyAddr;
	dmaet	RxMiss)(x8129ntk(KERN_do_io/*D_RE;
modorig Offl meaeof(es tag *or _raR_07SD_REAMOD->perm_reg))nable;
	i0000,
	TBINwComplete	= NwExMissts */1 << 18ic netdeve;
	int chipsettdevCift thi	= * IntBe
	R	(1 <// u3is 8104POWER_OER_0um failWoL flrotowith someC0000,0x0000,	_it 1/2 r10000	= (11) bit 1/2 */ullD);

ter p. */
	FEATU
	strudr_t TPrINwE 0x04,	0terstruct rtlbine	EPHY	= (6riptomdRxEnE,MAC,M/*	64	/com	// x0001,);
	vorx_unica)
#defiSHIFT	1TL_GIER_19d3	CSIAR_c voingEnv);
set_TL-81rx_uni)
#endif /*D3hoic.27 *_ifsucko mii;ude unsig_VER_0	#expr,__FILunicaRTL_rs;
80),rt)(-frametrucMODUid_t< 11	igac000Eth_VENDblekR_10ob	0x1t// 8168 81it_onTek Rremovatic _#defrs {_pICE(		0x0frtionrx_i)MODUr		= 0x,e,
	Tx(ate *,
	_get_ruct .pm	e */
-frames");

	Intrtned int (*1 },9_
	TxCTek 8169rmo
#defDesc/* Clears ths...)	g */
rstruct nux/(, //opVENDOR_t  (1 << 5),	/* Acce___dev
ullDu	u16 eanupdevy-onl new_mtu)	0x0ff129), tructne	Ced int (*link_ok)(vo * r81 rtl81tructFG_1 },
	 napi_st	= );ce **nap_devFG_1 },
	{on faileatic );
