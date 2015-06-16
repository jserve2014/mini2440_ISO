/*
 * tg3.c: Broadcom Tigon3 ethernet driver.
 *
 * Copyright (C) 2001, 2002, 2003, 2004 David S. Miller (davem@redhat.com)
 * Copyright (C) 2001, 2002, 2003 Jeff Garzik (jgarzik@pobox.com)
 * Copyright (C) 2004 Sun Microsystems Inc.
 * Copyright (C) 2005-2009 Broadcom Corporation.
 *
 * Firmware is:
 *	Derived from proprietary unpublished source code,
 *	Copyright (C) 2000-2003 Broadcom Corporation.
 *
 *	Permission is hereby granted for the distribution of this firmware
 *	data in hexadecimal or equivalent format, provided this copyright
 *	notice is accompanying it.
 */


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/in.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/brcmphy.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/workqueue.h>
#include <linux/prefetch.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>

#include <net/checksum.h>
#include <net/ip.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <asm/uaccess.h>

#ifdef CONFIG_SPARC
#include <asm/idprom.h>
#include <asm/prom.h>
#endif

#define BAR_0	0
#define BAR_2	2

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define TG3_VLAN_TAG_USED 1
#else
#define TG3_VLAN_TAG_USED 0
#endif

#include "tg3.h"

#define DRV_MODULE_NAME		"tg3"
#define PFX DRV_MODULE_NAME	": "
#define DRV_MODULE_VERSION	"3.102"
#define DRV_MODULE_RELDATE	"September 1, 2009"

#define TG3_DEF_MAC_MODE	0
#define TG3_DEF_RX_MODE		0
#define TG3_DEF_TX_MODE		0
#define TG3_DEF_MSG_ENABLE	  \
	(NETIF_MSG_DRV		| \
	 NETIF_MSG_PROBE	| \
	 NETIF_MSG_LINK		| \
	 NETIF_MSG_TIMER	| \
	 NETIF_MSG_IFDOWN	| \
	 NETIF_MSG_IFUP		| \
	 NETIF_MSG_RX_ERR	| \
	 NETIF_MSG_TX_ERR)

/* length of time before we decide the hardware is borked,
 * and dev->tx_timeout() should be called to fix the problem
 */
#define TG3_TX_TIMEOUT			(5 * HZ)

/* hardware minimum and maximum for a single frame's data payload */
#define TG3_MIN_MTU			60
#define TG3_MAX_MTU(tp)	\
	((tp->tg3_flags & TG3_FLAG_JUMBO_CAPABLE) ? 9000 : 1500)

/* These numbers seem to be hard coded in the NIC firmware somehow.
 * You can't change the ring sizes, but you can change where you place
 * them in the NIC onboard memory.
 */
#define TG3_RX_RING_SIZE		512
#define TG3_DEF_RX_RING_PENDING		200
#define TG3_RX_JUMBO_RING_SIZE		256
#define TG3_DEF_RX_JUMBO_RING_PENDING	100
#define TG3_RSS_INDIR_TBL_SIZE 128

/* Do not place this n-ring entries value into the tp struct itself,
 * we really want to expose these constants to GCC so that modulo et
 * al.  operations are done with shifts and masks instead of with
 * hw multiply/modulo instructions.  Another solution would be to
 * replace things like '% foo' with '& (foo - 1)'.
 */
#define TG3_RX_RCB_RING_SIZE(tp)	\
	(((tp->tg3_flags & TG3_FLAG_JUMBO_CAPABLE) && \
	  !(tp->tg3_flags2 & TG3_FLG2_5780_CLASS)) ? 1024 : 512)

#define TG3_TX_RING_SIZE		512
#define TG3_DEF_TX_RING_PENDING		(TG3_TX_RING_SIZE - 1)

#define TG3_RX_RING_BYTES	(sizeof(struct tg3_rx_buffer_desc) * \
				 TG3_RX_RING_SIZE)
#define TG3_RX_JUMBO_RING_BYTES	(sizeof(struct tg3_ext_rx_buffer_desc) * \
				 TG3_RX_JUMBO_RING_SIZE)
#define TG3_RX_RCB_RING_BYTES(tp) (sizeof(struct tg3_rx_buffer_desc) * \
				 TG3_RX_RCB_RING_SIZE(tp))
#define TG3_TX_RING_BYTES	(sizeof(struct tg3_tx_buffer_desc) * \
				 TG3_TX_RING_SIZE)
#define NEXT_TX(N)		(((N) + 1) & (TG3_TX_RING_SIZE - 1))

#define TG3_DMA_BYTE_ENAB		64

#define TG3_RX_STD_DMA_SZ		1536
#define TG3_RX_JMB_DMA_SZ		9046

#define TG3_RX_DMA_TO_MAP_SZ(x)		((x) + TG3_DMA_BYTE_ENAB)

#define TG3_RX_STD_MAP_SZ		TG3_RX_DMA_TO_MAP_SZ(TG3_RX_STD_DMA_SZ)
#define TG3_RX_JMB_MAP_SZ		TG3_RX_DMA_TO_MAP_SZ(TG3_RX_JMB_DMA_SZ)

/* minimum number of free TX descriptors required to wake up TX process */
#define TG3_TX_WAKEUP_THRESH(tnapi)		((tnapi)->tx_pending / 4)

#define TG3_RAW_IP_ALIGN 2

/* number of ETHTOOL_GSTATS u64's */
#define TG3_NUM_STATS		(sizeof(struct tg3_ethtool_stats)/sizeof(u64))

#define TG3_NUM_TEST		6

#define FIRMWARE_TG3		"tigon/tg3.bin"
#define FIRMWARE_TG3TSO		"tigon/tg3_tso.bin"
#define FIRMWARE_TG3TSO5	"tigon/tg3_tso5.bin"

static char version[] __devinitdata =
	DRV_MODULE_NAME ".c:v" DRV_MODULE_VERSION " (" DRV_MODULE_RELDATE ")\n";

MODULE_AUTHOR("David S. Miller (davem@redhat.com) and Jeff Garzik (jgarzik@pobox.com)");
MODULE_DESCRIPTION("Broadcom Tigon3 ethernet driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_FIRMWARE(FIRMWARE_TG3);
MODULE_FIRMWARE(FIRMWARE_TG3TSO);
MODULE_FIRMWARE(FIRMWARE_TG3TSO5);

#define TG3_RSS_MIN_NUM_MSIX_VECS	2

static int tg3_debug = -1;	/* -1 == use TG3_DEF_MSG_ENABLE as value */
module_param(tg3_debug, int, 0);
MODULE_PARM_DESC(tg3_debug, "Tigon3 bitmapped debugging message enable value");

static struct pci_device_id tg3_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5700)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5701)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5702)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5703)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5704)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5702FE)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5705)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5705_2)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5705M)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5705M_2)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5702X)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5703X)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5704S)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5702A3)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5703A3)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5782)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5788)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5789)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5901)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5901_2)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5704S_2)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5705F)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5720)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5721)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5722)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5750)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5751)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5750M)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5751M)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5751F)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5752)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5752M)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5753)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5753M)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5753F)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5754)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5754M)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5755)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5755M)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5756)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5786)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5787)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5787M)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5787F)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5714)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5714S)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5715)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5715S)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5780)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5780S)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5781)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5906)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5906M)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5784)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5764)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5723)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5761)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5761E)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, TG3PCI_DEVICE_TIGON3_5761S)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, TG3PCI_DEVICE_TIGON3_5761SE)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, TG3PCI_DEVICE_TIGON3_5785_G)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, TG3PCI_DEVICE_TIGON3_5785_F)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, TG3PCI_DEVICE_TIGON3_57780)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, TG3PCI_DEVICE_TIGON3_57760)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, TG3PCI_DEVICE_TIGON3_57790)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, TG3PCI_DEVICE_TIGON3_57788)},
	{PCI_DEVICE(PCI_VENDOR_ID_SYSKONNECT, PCI_DEVICE_ID_SYSKONNECT_9DXX)},
	{PCI_DEVICE(PCI_VENDOR_ID_SYSKONNECT, PCI_DEVICE_ID_SYSKONNECT_9MXX)},
	{PCI_DEVICE(PCI_VENDOR_ID_ALTIMA, PCI_DEVICE_ID_ALTIMA_AC1000)},
	{PCI_DEVICE(PCI_VENDOR_ID_ALTIMA, PCI_DEVICE_ID_ALTIMA_AC1001)},
	{PCI_DEVICE(PCI_VENDOR_ID_ALTIMA, PCI_DEVICE_ID_ALTIMA_AC1003)},
	{PCI_DEVICE(PCI_VENDOR_ID_ALTIMA, PCI_DEVICE_ID_ALTIMA_AC9100)},
	{PCI_DEVICE(PCI_VENDOR_ID_APPLE, PCI_DEVICE_ID_APPLE_TIGON3)},
	{}
};

MODULE_DEVICE_TABLE(pci, tg3_pci_tbl);

static const struct {
	const char string[ETH_GSTRING_LEN];
} ethtool_stats_keys[TG3_NUM_STATS] = {
	{ "rx_octets" },
	{ "rx_fragments" },
	{ "rx_ucast_packets" },
	{ "rx_mcast_packets" },
	{ "rx_bcast_packets" },
	{ "rx_fcs_errors" },
	{ "rx_align_errors" },
	{ "rx_xon_pause_rcvd" },
	{ "rx_xoff_pause_rcvd" },
	{ "rx_mac_ctrl_rcvd" },
	{ "rx_xoff_entered" },
	{ "rx_frame_too_long_errors" },
	{ "rx_jabbers" },
	{ "rx_undersize_packets" },
	{ "rx_in_length_errors" },
	{ "rx_out_length_errors" },
	{ "rx_64_or_less_octet_packets" },
	{ "rx_65_to_127_octet_packets" },
	{ "rx_128_to_255_octet_packets" },
	{ "rx_256_to_511_octet_packets" },
	{ "rx_512_to_1023_octet_packets" },
	{ "rx_1024_to_1522_octet_packets" },
	{ "rx_1523_to_2047_octet_packets" },
	{ "rx_2048_to_4095_octet_packets" },
	{ "rx_4096_to_8191_octet_packets" },
	{ "rx_8192_to_9022_octet_packets" },

	{ "tx_octets" },
	{ "tx_collisions" },

	{ "tx_xon_sent" },
	{ "tx_xoff_sent" },
	{ "tx_flow_control" },
	{ "tx_mac_errors" },
	{ "tx_single_collisions" },
	{ "tx_mult_collisions" },
	{ "tx_deferred" },
	{ "tx_excessive_collisions" },
	{ "tx_late_collisions" },
	{ "tx_collide_2times" },
	{ "tx_collide_3times" },
	{ "tx_collide_4times" },
	{ "tx_collide_5times" },
	{ "tx_collide_6times" },
	{ "tx_collide_7times" },
	{ "tx_collide_8times" },
	{ "tx_collide_9times" },
	{ "tx_collide_10times" },
	{ "tx_collide_11times" },
	{ "tx_collide_12times" },
	{ "tx_collide_13times" },
	{ "tx_collide_14times" },
	{ "tx_collide_15times" },
	{ "tx_ucast_packets" },
	{ "tx_mcast_packets" },
	{ "tx_bcast_packets" },
	{ "tx_carrier_sense_errors" },
	{ "tx_discards" },
	{ "tx_errors" },

	{ "dma_writeq_full" },
	{ "dma_write_prioq_full" },
	{ "rxbds_empty" },
	{ "rx_discards" },
	{ "rx_errors" },
	{ "rx_threshold_hit" },

	{ "dma_readq_full" },
	{ "dma_read_prioq_full" },
	{ "tx_comp_queue_full" },

	{ "ring_set_send_prod_index" },
	{ "ring_status_update" },
	{ "nic_irqs" },
	{ "nic_avoided_irqs" },
	{ "nic_tx_threshold_hit" }
};

static const struct {
	const char string[ETH_GSTRING_LEN];
} ethtool_test_keys[TG3_NUM_TEST] = {
	{ "nvram test     (online) " },
	{ "link test      (online) " },
	{ "register test  (offline)" },
	{ "memory test    (offline)" },
	{ "loopback test  (offline)" },
	{ "interrupt test (offline)" },
};

static void tg3_write32(struct tg3 *tp, u32 off, u32 val)
{
	writel(val, tp->regs + off);
}

static u32 tg3_read32(struct tg3 *tp, u32 off)
{
	return (readl(tp->regs + off));
}

static void tg3_ape_write32(struct tg3 *tp, u32 off, u32 val)
{
	writel(val, tp->aperegs + off);
}

static u32 tg3_ape_read32(struct tg3 *tp, u32 off)
{
	return (readl(tp->aperegs + off));
}

static void tg3_write_indirect_reg32(struct tg3 *tp, u32 off, u32 val)
{
	unsigned long flags;

	spin_lock_irqsave(&tp->indirect_lock, flags);
	pci_write_config_dword(tp->pdev, TG3PCI_REG_BASE_ADDR, off);
	pci_write_config_dword(tp->pdev, TG3PCI_REG_DATA, val);
	spin_unlock_irqrestore(&tp->indirect_lock, flags);
}

static void tg3_write_flush_reg32(struct tg3 *tp, u32 off, u32 val)
{
	writel(val, tp->regs + off);
	readl(tp->regs + off);
}

static u32 tg3_read_indirect_reg32(struct tg3 *tp, u32 off)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&tp->indirect_lock, flags);
	pci_write_config_dword(tp->pdev, TG3PCI_REG_BASE_ADDR, off);
	pci_read_config_dword(tp->pdev, TG3PCI_REG_DATA, &val);
	spin_unlock_irqrestore(&tp->indirect_lock, flags);
	return val;
}

static void tg3_write_indirect_mbox(struct tg3 *tp, u32 off, u32 val)
{
	unsigned long flags;

	if (off == (MAILBOX_RCVRET_CON_IDX_0 + TG3_64BIT_REG_LOW)) {
		pci_write_config_dword(tp->pdev, TG3PCI_RCV_RET_RING_CON_IDX +
				       TG3_64BIT_REG_LOW, val);
		return;
	}
	if (off == (MAILBOX_RCV_STD_PROD_IDX + TG3_64BIT_REG_LOW)) {
		pci_write_config_dword(tp->pdev, TG3PCI_STD_RING_PROD_IDX +
				       TG3_64BIT_REG_LOW, val);
		return;
	}

	spin_lock_irqsave(&tp->indirect_lock, flags);
	pci_write_config_dword(tp->pdev, TG3PCI_REG_BASE_ADDR, off + 0x5600);
	pci_write_config_dword(tp->pdev, TG3PCI_REG_DATA, val);
	spin_unlock_irqrestore(&tp->indirect_lock, flags);

	/* In indirect mode when disabling interrupts, we also need
	 * to clear the interrupt bit in the GRC local ctrl register.
	 */
	if ((off == (MAILBOX_INTERRUPT_0 + TG3_64BIT_REG_LOW)) &&
	    (val == 0x1)) {
		pci_write_config_dword(tp->pdev, TG3PCI_MISC_LOCAL_CTRL,
				       tp->grc_local_ctrl|GRC_LCLCTRL_CLEARINT);
	}
}

static u32 tg3_read_indirect_mbox(struct tg3 *tp, u32 off)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&tp->indirect_lock, flags);
	pci_write_config_dword(tp->pdev, TG3PCI_REG_BASE_ADDR, off + 0x5600);
	pci_read_config_dword(tp->pdev, TG3PCI_REG_DATA, &val);
	spin_unlock_irqrestore(&tp->indirect_lock, flags);
	return val;
}

/* usec_wait specifies the wait time in usec when writing to certain registers
 * where it is unsafe to read back the register without some delay.
 * GRC_LOCAL_CTRL is one example if the GPIOs are toggled to switch power.
 * TG3PCI_CLOCK_CTRL is another example if the clock frequencies are changed.
 */
static void _tw32_flush(struct tg3 *tp, u32 off, u32 val, u32 usec_wait)
{
	if ((tp->tg3_flags & TG3_FLAG_PCIX_TARGET_HWBUG) ||
	    (tp->tg3_flags2 & TG3_FLG2_ICH_WORKAROUND))
		/* Non-posted methods */
		tp->write32(tp, off, val);
	else {
		/* Posted method */
		tg3_write32(tp, off, val);
		if (usec_wait)
			udelay(usec_wait);
		tp->read32(tp, off);
	}
	/* Wait again after the read for the posted method to guarantee that
	 * the wait time is met.
	 */
	if (usec_wait)
		udelay(usec_wait);
}

static inline void tw32_mailbox_flush(struct tg3 *tp, u32 off, u32 val)
{
	tp->write32_mbox(tp, off, val);
	if (!(tp->tg3_flags & TG3_FLAG_MBOX_WRITE_REORDER) &&
	    !(tp->tg3_flags2 & TG3_FLG2_ICH_WORKAROUND))
		tp->read32_mbox(tp, off);
}

static void tg3_write32_tx_mbox(struct tg3 *tp, u32 off, u32 val)
{
	void __iomem *mbox = tp->regs + off;
	writel(val, mbox);
	if (tp->tg3_flags & TG3_FLAG_TXD_MBOX_HWBUG)
		writel(val, mbox);
	if (tp->tg3_flags & TG3_FLAG_MBOX_WRITE_REORDER)
		readl(mbox);
}

static u32 tg3_read32_mbox_5906(struct tg3 *tp, u32 off)
{
	return (readl(tp->regs + off + GRCMBOX_BASE));
}

static void tg3_write32_mbox_5906(struct tg3 *tp, u32 off, u32 val)
{
	writel(val, tp->regs + off + GRCMBOX_BASE);
}

#define tw32_mailbox(reg, val)	tp->write32_mbox(tp, reg, val)
#define tw32_mailbox_f(reg, val)	tw32_mailbox_flush(tp, (reg), (val))
#define tw32_rx_mbox(reg, val)	tp->write32_rx_mbox(tp, reg, val)
#define tw32_tx_mbox(reg, val)	tp->write32_tx_mbox(tp, reg, val)
#define tr32_mailbox(reg)	tp->read32_mbox(tp, reg)

#define tw32(reg,val)		tp->write32(tp, reg, val)
#define tw32_f(reg,val)		_tw32_flush(tp,(reg),(val), 0)
#define tw32_wait_f(reg,val,us)	_tw32_flush(tp,(reg),(val), (us))
#define tr32(reg)		tp->read32(tp, reg)

static void tg3_write_mem(struct tg3 *tp, u32 off, u32 val)
{
	unsigned long flags;

	if ((GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5906) &&
	    (off >= NIC_SRAM_STATS_BLK) && (off < NIC_SRAM_TX_BUFFER_DESC))
		return;

	spin_lock_irqsave(&tp->indirect_lock, flags);
	if (tp->tg3_flags & TG3_FLAG_SRAM_USE_CONFIG) {
		pci_write_config_dword(tp->pdev, TG3PCI_MEM_WIN_BASE_ADDR, off);
		pci_write_config_dword(tp->pdev, TG3PCI_MEM_WIN_DATA, val);

		/* Always leave this as zero. */
		pci_write_config_dword(tp->pdev, TG3PCI_MEM_WIN_BASE_ADDR, 0);
	} else {
		tw32_f(TG3PCI_MEM_WIN_BASE_ADDR, off);
		tw32_f(TG3PCI_MEM_WIN_DATA, val);

		/* Always leave this as zero. */
		tw32_f(TG3PCI_MEM_WIN_BASE_ADDR, 0);
	}
	spin_unlock_irqrestore(&tp->indirect_lock, flags);
}

static void tg3_read_mem(struct tg3 *tp, u32 off, u32 *val)
{
	unsigned long flags;

	if ((GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5906) &&
	    (off >= NIC_SRAM_STATS_BLK) && (off < NIC_SRAM_TX_BUFFER_DESC)) {
		*val = 0;
		return;
	}

	spin_lock_irqsave(&tp->indirect_lock, flags);
	if (tp->tg3_flags & TG3_FLAG_SRAM_USE_CONFIG) {
		pci_write_config_dword(tp->pdev, TG3PCI_MEM_WIN_BASE_ADDR, off);
		pci_read_config_dword(tp->pdev, TG3PCI_MEM_WIN_DATA, val);

		/* Always leave this as zero. */
		pci_write_config_dword(tp->pdev, TG3PCI_MEM_WIN_BASE_ADDR, 0);
	} else {
		tw32_f(TG3PCI_MEM_WIN_BASE_ADDR, off);
		*val = tr32(TG3PCI_MEM_WIN_DATA);

		/* Always leave this as zero. */
		tw32_f(TG3PCI_MEM_WIN_BASE_ADDR, 0);
	}
	spin_unlock_irqrestore(&tp->indirect_lock, flags);
}

static void tg3_ape_lock_init(struct tg3 *tp)
{
	int i;

	/* Make sure the driver hasn't any stale locks. */
	for (i = 0; i < 8; i++)
		tg3_ape_write32(tp, TG3_APE_LOCK_GRANT + 4 * i,
				APE_LOCK_GRANT_DRIVER);
}

static int tg3_ape_lock(struct tg3 *tp, int locknum)
{
	int i, off;
	int ret = 0;
	u32 status;

	if (!(tp->tg3_flags3 & TG3_FLG3_ENABLE_APE))
		return 0;

	switch (locknum) {
		case TG3_APE_LOCK_GRC:
		case TG3_APE_LOCK_MEM:
			break;
		default:
			return -EINVAL;
	}

	off = 4 * locknum;

	tg3_ape_write32(tp, TG3_APE_LOCK_REQ + off, APE_LOCK_REQ_DRIVER);

	/* Wait for up to 1 millisecond to acquire lock. */
	for (i = 0; i < 100; i++) {
		status = tg3_ape_read32(tp, TG3_APE_LOCK_GRANT + off);
		if (status == APE_LOCK_GRANT_DRIVER)
			break;
		udelay(10);
	}

	if (status != APE_LOCK_GRANT_DRIVER) {
		/* Revoke the lock request. */
		tg3_ape_write32(tp, TG3_APE_LOCK_GRANT + off,
				APE_LOCK_GRANT_DRIVER);

		ret = -EBUSY;
	}

	return ret;
}

static void tg3_ape_unlock(struct tg3 *tp, int locknum)
{
	int off;

	if (!(tp->tg3_flags3 & TG3_FLG3_ENABLE_APE))
		return;

	switch (locknum) {
		case TG3_APE_LOCK_GRC:
		case TG3_APE_LOCK_MEM:
			break;
		default:
			return;
	}

	off = 4 * locknum;
	tg3_ape_write32(tp, TG3_APE_LOCK_GRANT + off, APE_LOCK_GRANT_DRIVER);
}

static void tg3_disable_ints(struct tg3 *tp)
{
	int i;

	tw32(TG3PCI_MISC_HOST_CTRL,
	     (tp->misc_host_ctrl | MISC_HOST_CTRL_MASK_PCI_INT));
	for (i = 0; i < tp->irq_max; i++)
		tw32_mailbox_f(tp->napi[i].int_mbox, 0x00000001);
}

static void tg3_enable_ints(struct tg3 *tp)
{
	int i;
	u32 coal_now = 0;

	tp->irq_sync = 0;
	wmb();

	tw32(TG3PCI_MISC_HOST_CTRL,
	     (tp->misc_host_ctrl & ~MISC_HOST_CTRL_MASK_PCI_INT));

	for (i = 0; i < tp->irq_cnt; i++) {
		struct tg3_napi *tnapi = &tp->napi[i];
		tw32_mailbox_f(tnapi->int_mbox, tnapi->last_tag << 24);
		if (tp->tg3_flags2 & TG3_FLG2_1SHOT_MSI)
			tw32_mailbox_f(tnapi->int_mbox, tnapi->last_tag << 24);

		coal_now |= tnapi->coal_now;
	}

	/* Force an initial interrupt */
	if (!(tp->tg3_flags & TG3_FLAG_TAGGED_STATUS) &&
	    (tp->napi[0].hw_status->status & SD_STATUS_UPDATED))
		tw32(GRC_LOCAL_CTRL, tp->grc_local_ctrl | GRC_LCLCTRL_SETINT);
	else
		tw32(HOSTCC_MODE, tp->coalesce_mode |
		     HOSTCC_MODE_ENABLE | coal_now);
}

static inline unsigned int tg3_has_work(struct tg3_napi *tnapi)
{
	struct tg3 *tp = tnapi->tp;
	struct tg3_hw_status *sblk = tnapi->hw_status;
	unsigned int work_exists = 0;

	/* check for phy events */
	if (!(tp->tg3_flags &
	      (TG3_FLAG_USE_LINKCHG_REG |
	       TG3_FLAG_POLL_SERDES))) {
		if (sblk->status & SD_STATUS_LINK_CHG)
			work_exists = 1;
	}
	/* check for RX/TX work to do */
	if (sblk->idx[0].tx_consumer != tnapi->tx_cons ||
	    *(tnapi->rx_rcb_prod_idx) != tnapi->rx_rcb_ptr)
		work_exists = 1;

	return work_exists;
}

/* tg3_int_reenable
 *  similar to tg3_enable_ints, but it accurately determines whether there
 *  is new work pending and can return without flushing the PIO write
 *  which reenables interrupts
 */
static void tg3_int_reenable(struct tg3_napi *tnapi)
{
	struct tg3 *tp = tnapi->tp;

	tw32_mailbox(tnapi->int_mbox, tnapi->last_tag << 24);
	mmiowb();

	/* When doing tagged status, this work check is unnecessary.
	 * The last_tag we write above tells the chip which piece of
	 * work we've completed.
	 */
	if (!(tp->tg3_flags & TG3_FLAG_TAGGED_STATUS) &&
	    tg3_has_work(tnapi))
		tw32(HOSTCC_MODE, tp->coalesce_mode |
		     HOSTCC_MODE_ENABLE | tnapi->coal_now);
}

static void tg3_napi_disable(struct tg3 *tp)
{
	int i;

	for (i = tp->irq_cnt - 1; i >= 0; i--)
		napi_disable(&tp->napi[i].napi);
}

static void tg3_napi_enable(struct tg3 *tp)
{
	int i;

	for (i = 0; i < tp->irq_cnt; i++)
		napi_enable(&tp->napi[i].napi);
}

static inline void tg3_netif_stop(struct tg3 *tp)
{
	tp->dev->trans_start = jiffies;	/* prevent tx timeout */
	tg3_napi_disable(tp);
	netif_tx_disable(tp->dev);
}

static inline void tg3_netif_start(struct tg3 *tp)
{
	/* NOTE: unconditional netif_tx_wake_all_queues is only
	 * appropriate so long as all callers are assured to
	 * have free tx slots (such as after tg3_init_hw)
	 */
	netif_tx_wake_all_queues(tp->dev);

	tg3_napi_enable(tp);
	tp->napi[0].hw_status->status |= SD_STATUS_UPDATED;
	tg3_enable_ints(tp);
}

static void tg3_switch_clocks(struct tg3 *tp)
{
	u32 clock_ctrl;
	u32 orig_clock_ctrl;

	if ((tp->tg3_flags & TG3_FLAG_CPMU_PRESENT) ||
	    (tp->tg3_flags2 & TG3_FLG2_5780_CLASS))
		return;

	clock_ctrl = tr32(TG3PCI_CLOCK_CTRL);

	orig_clock_ctrl = clock_ctrl;
	clock_ctrl &= (CLOCK_CTRL_FORCE_CLKRUN |
		       CLOCK_CTRL_CLKRUN_OENABLE |
		       0x1f);
	tp->pci_clock_ctrl = clock_ctrl;

	if (tp->tg3_flags2 & TG3_FLG2_5705_PLUS) {
		if (orig_clock_ctrl & CLOCK_CTRL_625_CORE) {
			tw32_wait_f(TG3PCI_CLOCK_CTRL,
				    clock_ctrl | CLOCK_CTRL_625_CORE, 40);
		}
	} else if ((orig_clock_ctrl & CLOCK_CTRL_44MHZ_CORE) != 0) {
		tw32_wait_f(TG3PCI_CLOCK_CTRL,
			    clock_ctrl |
			    (CLOCK_CTRL_44MHZ_CORE | CLOCK_CTRL_ALTCLK),
			    40);
		tw32_wait_f(TG3PCI_CLOCK_CTRL,
			    clock_ctrl | (CLOCK_CTRL_ALTCLK),
			    40);
	}
	tw32_wait_f(TG3PCI_CLOCK_CTRL, clock_ctrl, 40);
}

#define PHY_BUSY_LOOPS	5000

static int tg3_readphy(struct tg3 *tp, int reg, u32 *val)
{
	u32 frame_val;
	unsigned int loops;
	int ret;

	if ((tp->mi_mode & MAC_MI_MODE_AUTO_POLL) != 0) {
		tw32_f(MAC_MI_MODE,
		     (tp->mi_mode & ~MAC_MI_MODE_AUTO_POLL));
		udelay(80);
	}

	*val = 0x0;

	frame_val  = ((tp->phy_addr << MI_COM_PHY_ADDR_SHIFT) &
		      MI_COM_PHY_ADDR_MASK);
	frame_val |= ((reg << MI_COM_REG_ADDR_SHIFT) &
		      MI_COM_REG_ADDR_MASK);
	frame_val |= (MI_COM_CMD_READ | MI_COM_START);

	tw32_f(MAC_MI_COM, frame_val);

	loops = PHY_BUSY_LOOPS;
	while (loops != 0) {
		udelay(10);
		frame_val = tr32(MAC_MI_COM);

		if ((frame_val & MI_COM_BUSY) == 0) {
			udelay(5);
			frame_val = tr32(MAC_MI_COM);
			break;
		}
		loops -= 1;
	}

	ret = -EBUSY;
	if (loops != 0) {
		*val = frame_val & MI_COM_DATA_MASK;
		ret = 0;
	}

	if ((tp->mi_mode & MAC_MI_MODE_AUTO_POLL) != 0) {
		tw32_f(MAC_MI_MODE, tp->mi_mode);
		udelay(80);
	}

	return ret;
}

static int tg3_writephy(struct tg3 *tp, int reg, u32 val)
{
	u32 frame_val;
	unsigned int loops;
	int ret;

	if ((tp->tg3_flags3 & TG3_FLG3_PHY_IS_FET) &&
	    (reg == MII_TG3_CTRL || reg == MII_TG3_AUX_CTRL))
		return 0;

	if ((tp->mi_mode & MAC_MI_MODE_AUTO_POLL) != 0) {
		tw32_f(MAC_MI_MODE,
		     (tp->mi_mode & ~MAC_MI_MODE_AUTO_POLL));
		udelay(80);
	}

	frame_val  = ((tp->phy_addr << MI_COM_PHY_ADDR_SHIFT) &
		      MI_COM_PHY_ADDR_MASK);
	frame_val |= ((reg << MI_COM_REG_ADDR_SHIFT) &
		      MI_COM_REG_ADDR_MASK);
	frame_val |= (val & MI_COM_DATA_MASK);
	frame_val |= (MI_COM_CMD_WRITE | MI_COM_START);

	tw32_f(MAC_MI_COM, frame_val);

	loops = PHY_BUSY_LOOPS;
	while (loops != 0) {
		udelay(10);
		frame_val = tr32(MAC_MI_COM);
		if ((frame_val & MI_COM_BUSY) == 0) {
			udelay(5);
			frame_val = tr32(MAC_MI_COM);
			break;
		}
		loops -= 1;
	}

	ret = -EBUSY;
	if (loops != 0)
		ret = 0;

	if ((tp->mi_mode & MAC_MI_MODE_AUTO_POLL) != 0) {
		tw32_f(MAC_MI_MODE, tp->mi_mode);
		udelay(80);
	}

	return ret;
}

static int tg3_bmcr_reset(struct tg3 *tp)
{
	u32 phy_control;
	int limit, err;

	/* OK, reset it, and poll the BMCR_RESET bit until it
	 * clears or we time out.
	 */
	phy_control = BMCR_RESET;
	err = tg3_writephy(tp, MII_BMCR, phy_control);
	if (err != 0)
		return -EBUSY;

	limit = 5000;
	while (limit--) {
		err = tg3_readphy(tp, MII_BMCR, &phy_control);
		if (err != 0)
			return -EBUSY;

		if ((phy_control & BMCR_RESET) == 0) {
			udelay(40);
			break;
		}
		udelay(10);
	}
	if (limit < 0)
		return -EBUSY;

	return 0;
}

static int tg3_mdio_read(struct mii_bus *bp, int mii_id, int reg)
{
	struct tg3 *tp = bp->priv;
	u32 val;

	spin_lock_bh(&tp->lock);

	if (tg3_readphy(tp, reg, &val))
		val = -EIO;

	spin_unlock_bh(&tp->lock);

	return val;
}

static int tg3_mdio_write(struct mii_bus *bp, int mii_id, int reg, u16 val)
{
	struct tg3 *tp = bp->priv;
	u32 ret = 0;

	spin_lock_bh(&tp->lock);

	if (tg3_writephy(tp, reg, val))
		ret = -EIO;

	spin_unlock_bh(&tp->lock);

	return ret;
}

static int tg3_mdio_reset(struct mii_bus *bp)
{
	return 0;
}

static void tg3_mdio_config_5785(struct tg3 *tp)
{
	u32 val;
	struct phy_device *phydev;

	phydev = tp->mdio_bus->phy_map[PHY_ADDR];
	switch (phydev->drv->phy_id & phydev->drv->phy_id_mask) {
	case TG3_PHY_ID_BCM50610:
		val = MAC_PHYCFG2_50610_LED_MODES;
		break;
	case TG3_PHY_ID_BCMAC131:
		val = MAC_PHYCFG2_AC131_LED_MODES;
		break;
	case TG3_PHY_ID_RTL8211C:
		val = MAC_PHYCFG2_RTL8211C_LED_MODES;
		break;
	case TG3_PHY_ID_RTL8201E:
		val = MAC_PHYCFG2_RTL8201E_LED_MODES;
		break;
	default:
		return;
	}

	if (phydev->interface != PHY_INTERFACE_MODE_RGMII) {
		tw32(MAC_PHYCFG2, val);

		val = tr32(MAC_PHYCFG1);
		val &= ~(MAC_PHYCFG1_RGMII_INT |
			 MAC_PHYCFG1_RXCLK_TO_MASK | MAC_PHYCFG1_TXCLK_TO_MASK);
		val |= MAC_PHYCFG1_RXCLK_TIMEOUT | MAC_PHYCFG1_TXCLK_TIMEOUT;
		tw32(MAC_PHYCFG1, val);

		return;
	}

	if (!(tp->tg3_flags3 & TG3_FLG3_RGMII_STD_IBND_DISABLE))
		val |= MAC_PHYCFG2_EMODE_MASK_MASK |
		       MAC_PHYCFG2_FMODE_MASK_MASK |
		       MAC_PHYCFG2_GMODE_MASK_MASK |
		       MAC_PHYCFG2_ACT_MASK_MASK   |
		       MAC_PHYCFG2_QUAL_MASK_MASK |
		       MAC_PHYCFG2_INBAND_ENABLE;

	tw32(MAC_PHYCFG2, val);

	val = tr32(MAC_PHYCFG1);
	val &= ~(MAC_PHYCFG1_RXCLK_TO_MASK | MAC_PHYCFG1_TXCLK_TO_MASK |
		 MAC_PHYCFG1_RGMII_EXT_RX_DEC | MAC_PHYCFG1_RGMII_SND_STAT_EN);
	if (!(tp->tg3_flags3 & TG3_FLG3_RGMII_STD_IBND_DISABLE)) {
		if (tp->tg3_flags3 & TG3_FLG3_RGMII_EXT_IBND_RX_EN)
			val |= MAC_PHYCFG1_RGMII_EXT_RX_DEC;
		if (tp->tg3_flags3 & TG3_FLG3_RGMII_EXT_IBND_TX_EN)
			val |= MAC_PHYCFG1_RGMII_SND_STAT_EN;
	}
	val |= MAC_PHYCFG1_RXCLK_TIMEOUT | MAC_PHYCFG1_TXCLK_TIMEOUT |
	       MAC_PHYCFG1_RGMII_INT | MAC_PHYCFG1_TXC_DRV;
	tw32(MAC_PHYCFG1, val);

	val = tr32(MAC_EXT_RGMII_MODE);
	val &= ~(MAC_RGMII_MODE_RX_INT_B |
		 MAC_RGMII_MODE_RX_QUALITY |
		 MAC_RGMII_MODE_RX_ACTIVITY |
		 MAC_RGMII_MODE_RX_ENG_DET |
		 MAC_RGMII_MODE_TX_ENABLE |
		 MAC_RGMII_MODE_TX_LOWPWR |
		 MAC_RGMII_MODE_TX_RESET);
	if (!(tp->tg3_flags3 & TG3_FLG3_RGMII_STD_IBND_DISABLE)) {
		if (tp->tg3_flags3 & TG3_FLG3_RGMII_EXT_IBND_RX_EN)
			val |= MAC_RGMII_MODE_RX_INT_B |
			       MAC_RGMII_MODE_RX_QUALITY |
			       MAC_RGMII_MODE_RX_ACTIVITY |
			       MAC_RGMII_MODE_RX_ENG_DET;
		if (tp->tg3_flags3 & TG3_FLG3_RGMII_EXT_IBND_TX_EN)
			val |= MAC_RGMII_MODE_TX_ENABLE |
			       MAC_RGMII_MODE_TX_LOWPWR |
			       MAC_RGMII_MODE_TX_RESET;
	}
	tw32(MAC_EXT_RGMII_MODE, val);
}

static void tg3_mdio_start(struct tg3 *tp)
{
	tp->mi_mode &= ~MAC_MI_MODE_AUTO_POLL;
	tw32_f(MAC_MI_MODE, tp->mi_mode);
	udelay(80);

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5717) {
		u32 funcnum, is_serdes;

		funcnum = tr32(TG3_CPMU_STATUS) & TG3_CPMU_STATUS_PCIE_FUNC;
		if (funcnum)
			tp->phy_addr = 2;
		else
			tp->phy_addr = 1;

		is_serdes = tr32(SG_DIG_STATUS) & SG_DIG_IS_SERDES;
		if (is_serdes)
			tp->phy_addr += 7;
	} else
		tp->phy_addr = PHY_ADDR;

	if ((tp->tg3_flags3 & TG3_FLG3_MDIOBUS_INITED) &&
	    GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5785)
		tg3_mdio_config_5785(tp);
}

static int tg3_mdio_init(struct tg3 *tp)
{
	int i;
	u32 reg;
	struct phy_device *phydev;

	tg3_mdio_start(tp);

	if (!(tp->tg3_flags3 & TG3_FLG3_USE_PHYLIB) ||
	    (tp->tg3_flags3 & TG3_FLG3_MDIOBUS_INITED))
		return 0;

	tp->mdio_bus = mdiobus_alloc();
	if (tp->mdio_bus == NULL)
		return -ENOMEM;

	tp->mdio_bus->name     = "tg3 mdio bus";
	snprintf(tp->mdio_bus->id, MII_BUS_ID_SIZE, "%x",
		 (tp->pdev->bus->number << 8) | tp->pdev->devfn);
	tp->mdio_bus->priv     = tp;
	tp->mdio_bus->parent   = &tp->pdev->dev;
	tp->mdio_bus->read     = &tg3_mdio_read;
	tp->mdio_bus->write    = &tg3_mdio_write;
	tp->mdio_bus->reset    = &tg3_mdio_reset;
	tp->mdio_bus->phy_mask = ~(1 << PHY_ADDR);
	tp->mdio_bus->irq      = &tp->mdio_irq[0];

	for (i = 0; i < PHY_MAX_ADDR; i++)
		tp->mdio_bus->irq[i] = PHY_POLL;

	/* The bus registration will look for all the PHYs on the mdio bus.
	 * Unfortunately, it does not ensure the PHY is powered up before
	 * accessing the PHY ID registers.  A chip reset is the
	 * quickest way to bring the device back to an operational state..
	 */
	if (tg3_readphy(tp, MII_BMCR, &reg) || (reg & BMCR_PDOWN))
		tg3_bmcr_reset(tp);

	i = mdiobus_register(tp->mdio_bus);
	if (i) {
		printk(KERN_WARNING "%s: mdiobus_reg failed (0x%x)\n",
			tp->dev->name, i);
		mdiobus_free(tp->mdio_bus);
		return i;
	}

	phydev = tp->mdio_bus->phy_map[PHY_ADDR];

	if (!phydev || !phydev->drv) {
		printk(KERN_WARNING "%s: No PHY devices\n", tp->dev->name);
		mdiobus_unregister(tp->mdio_bus);
		mdiobus_free(tp->mdio_bus);
		return -ENODEV;
	}

	switch (phydev->drv->phy_id & phydev->drv->phy_id_mask) {
	case TG3_PHY_ID_BCM57780:
		phydev->interface = PHY_INTERFACE_MODE_GMII;
		break;
	case TG3_PHY_ID_BCM50610:
		if (tp->tg3_flags3 & TG3_FLG3_RGMII_STD_IBND_DISABLE)
			phydev->dev_flags |= PHY_BRCM_STD_IBND_DISABLE;
		if (tp->tg3_flags3 & TG3_FLG3_RGMII_EXT_IBND_RX_EN)
			phydev->dev_flags |= PHY_BRCM_EXT_IBND_RX_ENABLE;
		if (tp->tg3_flags3 & TG3_FLG3_RGMII_EXT_IBND_TX_EN)
			phydev->dev_flags |= PHY_BRCM_EXT_IBND_TX_ENABLE;
		/* fallthru */
	case TG3_PHY_ID_RTL8211C:
		phydev->interface = PHY_INTERFACE_MODE_RGMII;
		break;
	case TG3_PHY_ID_RTL8201E:
	case TG3_PHY_ID_BCMAC131:
		phydev->interface = PHY_INTERFACE_MODE_MII;
		tp->tg3_flags3 |= TG3_FLG3_PHY_IS_FET;
		break;
	}

	tp->tg3_flags3 |= TG3_FLG3_MDIOBUS_INITED;

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5785)
		tg3_mdio_config_5785(tp);

	return 0;
}

static void tg3_mdio_fini(struct tg3 *tp)
{
	if (tp->tg3_flags3 & TG3_FLG3_MDIOBUS_INITED) {
		tp->tg3_flags3 &= ~TG3_FLG3_MDIOBUS_INITED;
		mdiobus_unregister(tp->mdio_bus);
		mdiobus_free(tp->mdio_bus);
	}
}

/* tp->lock is held. */
static inline void tg3_generate_fw_event(struct tg3 *tp)
{
	u32 val;

	val = tr32(GRC_RX_CPU_EVENT);
	val |= GRC_RX_CPU_DRIVER_EVENT;
	tw32_f(GRC_RX_CPU_EVENT, val);

	tp->last_event_jiffies = jiffies;
}

#define TG3_FW_EVENT_TIMEOUT_USEC 2500

/* tp->lock is held. */
static void tg3_wait_for_event_ack(struct tg3 *tp)
{
	int i;
	unsigned int delay_cnt;
	long time_remain;

	/* If enough time has passed, no wait is necessary. */
	time_remain = (long)(tp->last_event_jiffies + 1 +
		      usecs_to_jiffies(TG3_FW_EVENT_TIMEOUT_USEC)) -
		      (long)jiffies;
	if (time_remain < 0)
		return;

	/* Check if we can shorten the wait time. */
	delay_cnt = jiffies_to_usecs(time_remain);
	if (delay_cnt > TG3_FW_EVENT_TIMEOUT_USEC)
		delay_cnt = TG3_FW_EVENT_TIMEOUT_USEC;
	delay_cnt = (delay_cnt >> 3) + 1;

	for (i = 0; i < delay_cnt; i++) {
		if (!(tr32(GRC_RX_CPU_EVENT) & GRC_RX_CPU_DRIVER_EVENT))
			break;
		udelay(8);
	}
}

/* tp->lock is held. */
static void tg3_ump_link_report(struct tg3 *tp)
{
	u32 reg;
	u32 val;

	if (!(tp->tg3_flags2 & TG3_FLG2_5780_CLASS) ||
	    !(tp->tg3_flags  & TG3_FLAG_ENABLE_ASF))
		return;

	tg3_wait_for_event_ack(tp);

	tg3_write_mem(tp, NIC_SRAM_FW_CMD_MBOX, FWCMD_NICDRV_LINK_UPDATE);

	tg3_write_mem(tp, NIC_SRAM_FW_CMD_LEN_MBOX, 14);

	val = 0;
	if (!tg3_readphy(tp, MII_BMCR, &reg))
		val = reg << 16;
	if (!tg3_readphy(tp, MII_BMSR, &reg))
		val |= (reg & 0xffff);
	tg3_write_mem(tp, NIC_SRAM_FW_CMD_DATA_MBOX, val);

	val = 0;
	if (!tg3_readphy(tp, MII_ADVERTISE, &reg))
		val = reg << 16;
	if (!tg3_readphy(tp, MII_LPA, &reg))
		val |= (reg & 0xffff);
	tg3_write_mem(tp, NIC_SRAM_FW_CMD_DATA_MBOX + 4, val);

	val = 0;
	if (!(tp->tg3_flags2 & TG3_FLG2_MII_SERDES)) {
		if (!tg3_readphy(tp, MII_CTRL1000, &reg))
			val = reg << 16;
		if (!tg3_readphy(tp, MII_STAT1000, &reg))
			val |= (reg & 0xffff);
	}
	tg3_write_mem(tp, NIC_SRAM_FW_CMD_DATA_MBOX + 8, val);

	if (!tg3_readphy(tp, MII_PHYADDR, &reg))
		val = reg << 16;
	else
		val = 0;
	tg3_write_mem(tp, NIC_SRAM_FW_CMD_DATA_MBOX + 12, val);

	tg3_generate_fw_event(tp);
}

static void tg3_link_report(struct tg3 *tp)
{
	if (!netif_carrier_ok(tp->dev)) {
		if (netif_msg_link(tp))
			printk(KERN_INFO PFX "%s: Link is down.\n",
			       tp->dev->name);
		tg3_ump_link_report(tp);
	} else if (netif_msg_link(tp)) {
		printk(KERN_INFO PFX "%s: Link is up at %d Mbps, %s duplex.\n",
		       tp->dev->name,
		       (tp->link_config.active_speed == SPEED_1000 ?
			1000 :
			(tp->link_config.active_speed == SPEED_100 ?
			 100 : 10)),
		       (tp->link_config.active_duplex == DUPLEX_FULL ?
			"full" : "half"));

		printk(KERN_INFO PFX
		       "%s: Flow control is %s for TX and %s for RX.\n",
		       tp->dev->name,
		       (tp->link_config.active_flowctrl & FLOW_CTRL_TX) ?
		       "on" : "off",
		       (tp->link_config.active_flowctrl & FLOW_CTRL_RX) ?
		       "on" : "off");
		tg3_ump_link_report(tp);
	}
}

static u16 tg3_advert_flowctrl_1000T(u8 flow_ctrl)
{
	u16 miireg;

	if ((flow_ctrl & FLOW_CTRL_TX) && (flow_ctrl & FLOW_CTRL_RX))
		miireg = ADVERTISE_PAUSE_CAP;
	else if (flow_ctrl & FLOW_CTRL_TX)
		miireg = ADVERTISE_PAUSE_ASYM;
	else if (flow_ctrl & FLOW_CTRL_RX)
		miireg = ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;
	else
		miireg = 0;

	return miireg;
}

static u16 tg3_advert_flowctrl_1000X(u8 flow_ctrl)
{
	u16 miireg;

	if ((flow_ctrl & FLOW_CTRL_TX) && (flow_ctrl & FLOW_CTRL_RX))
		miireg = ADVERTISE_1000XPAUSE;
	else if (flow_ctrl & FLOW_CTRL_TX)
		miireg = ADVERTISE_1000XPSE_ASYM;
	else if (flow_ctrl & FLOW_CTRL_RX)
		miireg = ADVERTISE_1000XPAUSE | ADVERTISE_1000XPSE_ASYM;
	else
		miireg = 0;

	return miireg;
}

static u8 tg3_resolve_flowctrl_1000X(u16 lcladv, u16 rmtadv)
{
	u8 cap = 0;

	if (lcladv & ADVERTISE_1000XPAUSE) {
		if (lcladv & ADVERTISE_1000XPSE_ASYM) {
			if (rmtadv & LPA_1000XPAUSE)
				cap = FLOW_CTRL_TX | FLOW_CTRL_RX;
			else if (rmtadv & LPA_1000XPAUSE_ASYM)
				cap = FLOW_CTRL_RX;
		} else {
			if (rmtadv & LPA_1000XPAUSE)
				cap = FLOW_CTRL_TX | FLOW_CTRL_RX;
		}
	} else if (lcladv & ADVERTISE_1000XPSE_ASYM) {
		if ((rmtadv & LPA_1000XPAUSE) && (rmtadv & LPA_1000XPAUSE_ASYM))
			cap = FLOW_CTRL_TX;
	}

	return cap;
}

static void tg3_setup_flow_control(struct tg3 *tp, u32 lcladv, u32 rmtadv)
{
	u8 autoneg;
	u8 flowctrl = 0;
	u32 old_rx_mode = tp->rx_mode;
	u32 old_tx_mode = tp->tx_mode;

	if (tp->tg3_flags3 & TG3_FLG3_USE_PHYLIB)
		autoneg = tp->mdio_bus->phy_map[PHY_ADDR]->autoneg;
	else
		autoneg = tp->link_config.autoneg;

	if (autoneg == AUTONEG_ENABLE &&
	    (tp->tg3_flags & TG3_FLAG_PAUSE_AUTONEG)) {
		if (tp->tg3_flags2 & TG3_FLG2_ANY_SERDES)
			flowctrl = tg3_resolve_flowctrl_1000X(lcladv, rmtadv);
		else
			flowctrl = mii_resolve_flowctrl_fdx(lcladv, rmtadv);
	} else
		flowctrl = tp->link_config.flowctrl;

	tp->link_config.active_flowctrl = flowctrl;

	if (flowctrl & FLOW_CTRL_RX)
		tp->rx_mode |= RX_MODE_FLOW_CTRL_ENABLE;
	else
		tp->rx_mode &= ~RX_MODE_FLOW_CTRL_ENABLE;

	if (old_rx_mode != tp->rx_mode)
		tw32_f(MAC_RX_MODE, tp->rx_mode);

	if (flowctrl & FLOW_CTRL_TX)
		tp->tx_mode |= TX_MODE_FLOW_CTRL_ENABLE;
	else
		tp->tx_mode &= ~TX_MODE_FLOW_CTRL_ENABLE;

	if (old_tx_mode != tp->tx_mode)
		tw32_f(MAC_TX_MODE, tp->tx_mode);
}

static void tg3_adjust_link(struct net_device *dev)
{
	u8 oldflowctrl, linkmesg = 0;
	u32 mac_mode, lcl_adv, rmt_adv;
	struct tg3 *tp = netdev_priv(dev);
	struct phy_device *phydev = tp->mdio_bus->phy_map[PHY_ADDR];

	spin_lock_bh(&tp->lock);

	mac_mode = tp->mac_mode & ~(MAC_MODE_PORT_MODE_MASK |
				    MAC_MODE_HALF_DUPLEX);

	oldflowctrl = tp->link_config.active_flowctrl;

	if (phydev->link) {
		lcl_adv = 0;
		rmt_adv = 0;

		if (phydev->speed == SPEED_100 || phydev->speed == SPEED_10)
			mac_mode |= MAC_MODE_PORT_MODE_MII;
		else
			mac_mode |= MAC_MODE_PORT_MODE_GMII;

		if (phydev->duplex == DUPLEX_HALF)
			mac_mode |= MAC_MODE_HALF_DUPLEX;
		else {
			lcl_adv = tg3_advert_flowctrl_1000T(
				  tp->link_config.flowctrl);

			if (phydev->pause)
				rmt_adv = LPA_PAUSE_CAP;
			if (phydev->asym_pause)
				rmt_adv |= LPA_PAUSE_ASYM;
		}

		tg3_setup_flow_control(tp, lcl_adv, rmt_adv);
	} else
		mac_mode |= MAC_MODE_PORT_MODE_GMII;

	if (mac_mode != tp->mac_mode) {
		tp->mac_mode = mac_mode;
		tw32_f(MAC_MODE, tp->mac_mode);
		udelay(40);
	}

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5785) {
		if (phydev->speed == SPEED_10)
			tw32(MAC_MI_STAT,
			     MAC_MI_STAT_10MBPS_MODE |
			     MAC_MI_STAT_LNKSTAT_ATTN_ENAB);
		else
			tw32(MAC_MI_STAT, MAC_MI_STAT_LNKSTAT_ATTN_ENAB);
	}

	if (phydev->speed == SPEED_1000 && phydev->duplex == DUPLEX_HALF)
		tw32(MAC_TX_LENGTHS,
		     ((2 << TX_LENGTHS_IPG_CRS_SHIFT) |
		      (6 << TX_LENGTHS_IPG_SHIFT) |
		      (0xff << TX_LENGTHS_SLOT_TIME_SHIFT)));
	else
		tw32(MAC_TX_LENGTHS,
		     ((2 << TX_LENGTHS_IPG_CRS_SHIFT) |
		      (6 << TX_LENGTHS_IPG_SHIFT) |
		      (32 << TX_LENGTHS_SLOT_TIME_SHIFT)));

	if ((phydev->link && tp->link_config.active_speed == SPEED_INVALID) ||
	    (!phydev->link && tp->link_config.active_speed != SPEED_INVALID) ||
	    phydev->speed != tp->link_config.active_speed ||
	    phydev->duplex != tp->link_config.active_duplex ||
	    oldflowctrl != tp->link_config.active_flowctrl)
	    linkmesg = 1;

	tp->link_config.active_speed = phydev->speed;
	tp->link_config.active_duplex = phydev->duplex;

	spin_unlock_bh(&tp->lock);

	if (linkmesg)
		tg3_link_report(tp);
}

static int tg3_phy_init(struct tg3 *tp)
{
	struct phy_device *phydev;

	if (tp->tg3_flags3 & TG3_FLG3_PHY_CONNECTED)
		return 0;

	/* Bring the PHY back to a known state. */
	tg3_bmcr_reset(tp);

	phydev = tp->mdio_bus->phy_map[PHY_ADDR];

	/* Attach the MAC to the PHY. */
	phydev = phy_connect(tp->dev, dev_name(&phydev->dev), tg3_adjust_link,
			     phydev->dev_flags, phydev->interface);
	if (IS_ERR(phydev)) {
		printk(KERN_ERR "%s: Could not attach to PHY\n", tp->dev->name);
		return PTR_ERR(phydev);
	}

	/* Mask with MAC supported features. */
	switch (phydev->interface) {
	case PHY_INTERFACE_MODE_GMII:
	case PHY_INTERFACE_MODE_RGMII:
		if (!(tp->tg3_flags & TG3_FLAG_10_100_ONLY)) {
			phydev->supported &= (PHY_GBIT_FEATURES |
					      SUPPORTED_Pause |
					      SUPPORTED_Asym_Pause);
			break;
		}
		/* fallthru */
	case PHY_INTERFACE_MODE_MII:
		phydev->supported &= (PHY_BASIC_FEATURES |
				      SUPPORTED_Pause |
				      SUPPORTED_Asym_Pause);
		break;
	default:
		phy_disconnect(tp->mdio_bus->phy_map[PHY_ADDR]);
		return -EINVAL;
	}

	tp->tg3_flags3 |= TG3_FLG3_PHY_CONNECTED;

	phydev->advertising = phydev->supported;

	return 0;
}

static void tg3_phy_start(struct tg3 *tp)
{
	struct phy_device *phydev;

	if (!(tp->tg3_flags3 & TG3_FLG3_PHY_CONNECTED))
		return;

	phydev = tp->mdio_bus->phy_map[PHY_ADDR];

	if (tp->link_config.phy_is_low_power) {
		tp->link_config.phy_is_low_power = 0;
		phydev->speed = tp->link_config.orig_speed;
		phydev->duplex = tp->link_config.orig_duplex;
		phydev->autoneg = tp->link_config.orig_autoneg;
		phydev->advertising = tp->link_config.orig_advertising;
	}

	phy_start(phydev);

	phy_start_aneg(phydev);
}

static void tg3_phy_stop(struct tg3 *tp)
{
	if (!(tp->tg3_flags3 & TG3_FLG3_PHY_CONNECTED))
		return;

	phy_stop(tp->mdio_bus->phy_map[PHY_ADDR]);
}

static void tg3_phy_fini(struct tg3 *tp)
{
	if (tp->tg3_flags3 & TG3_FLG3_PHY_CONNECTED) {
		phy_disconnect(tp->mdio_bus->phy_map[PHY_ADDR]);
		tp->tg3_flags3 &= ~TG3_FLG3_PHY_CONNECTED;
	}
}

static void tg3_phydsp_write(struct tg3 *tp, u32 reg, u32 val)
{
	tg3_writephy(tp, MII_TG3_DSP_ADDRESS, reg);
	tg3_writephy(tp, MII_TG3_DSP_RW_PORT, val);
}

static void tg3_phy_fet_toggle_apd(struct tg3 *tp, bool enable)
{
	u32 phytest;

	if (!tg3_readphy(tp, MII_TG3_FET_TEST, &phytest)) {
		u32 phy;

		tg3_writephy(tp, MII_TG3_FET_TEST,
			     phytest | MII_TG3_FET_SHADOW_EN);
		if (!tg3_readphy(tp, MII_TG3_FET_SHDW_AUXSTAT2, &phy)) {
			if (enable)
				phy |= MII_TG3_FET_SHDW_AUXSTAT2_APD;
			else
				phy &= ~MII_TG3_FET_SHDW_AUXSTAT2_APD;
			tg3_writephy(tp, MII_TG3_FET_SHDW_AUXSTAT2, phy);
		}
		tg3_writephy(tp, MII_TG3_FET_TEST, phytest);
	}
}

static void tg3_phy_toggle_apd(struct tg3 *tp, bool enable)
{
	u32 reg;

	if (!(tp->tg3_flags2 & TG3_FLG2_5705_PLUS))
		return;

	if (tp->tg3_flags3 & TG3_FLG3_PHY_IS_FET) {
		tg3_phy_fet_toggle_apd(tp, enable);
		return;
	}

	reg = MII_TG3_MISC_SHDW_WREN |
	      MII_TG3_MISC_SHDW_SCR5_SEL |
	      MII_TG3_MISC_SHDW_SCR5_LPED |
	      MII_TG3_MISC_SHDW_SCR5_DLPTLM |
	      MII_TG3_MISC_SHDW_SCR5_SDTL |
	      MII_TG3_MISC_SHDW_SCR5_C125OE;
	if (GET_ASIC_REV(tp->pci_chip_rev_id) != ASIC_REV_5784 || !enable)
		reg |= MII_TG3_MISC_SHDW_SCR5_DLLAPD;

	tg3_writephy(tp, MII_TG3_MISC_SHDW, reg);


	reg = MII_TG3_MISC_SHDW_WREN |
	      MII_TG3_MISC_SHDW_APD_SEL |
	      MII_TG3_MISC_SHDW_APD_WKTM_84MS;
	if (enable)
		reg |= MII_TG3_MISC_SHDW_APD_ENABLE;

	tg3_writephy(tp, MII_TG3_MISC_SHDW, reg);
}

static void tg3_phy_toggle_automdix(struct tg3 *tp, int enable)
{
	u32 phy;

	if (!(tp->tg3_flags2 & TG3_FLG2_5705_PLUS) ||
	    (tp->tg3_flags2 & TG3_FLG2_ANY_SERDES))
		return;

	if (tp->tg3_flags3 & TG3_FLG3_PHY_IS_FET) {
		u32 ephy;

		if (!tg3_readphy(tp, MII_TG3_FET_TEST, &ephy)) {
			u32 reg = MII_TG3_FET_SHDW_MISCCTRL;

			tg3_writephy(tp, MII_TG3_FET_TEST,
				     ephy | MII_TG3_FET_SHADOW_EN);
			if (!tg3_readphy(tp, reg, &phy)) {
				if (enable)
					phy |= MII_TG3_FET_SHDW_MISCCTRL_MDIX;
				else
					phy &= ~MII_TG3_FET_SHDW_MISCCTRL_MDIX;
				tg3_writephy(tp, reg, phy);
			}
			tg3_writephy(tp, MII_TG3_FET_TEST, ephy);
		}
	} else {
		phy = MII_TG3_AUXCTL_MISC_RDSEL_MISC |
		      MII_TG3_AUXCTL_SHDWSEL_MISC;
		if (!tg3_writephy(tp, MII_TG3_AUX_CTRL, phy) &&
		    !tg3_readphy(tp, MII_TG3_AUX_CTRL, &phy)) {
			if (enable)
				phy |= MII_TG3_AUXCTL_MISC_FORCE_AMDIX;
			else
				phy &= ~MII_TG3_AUXCTL_MISC_FORCE_AMDIX;
			phy |= MII_TG3_AUXCTL_MISC_WREN;
			tg3_writephy(tp, MII_TG3_AUX_CTRL, phy);
		}
	}
}

static void tg3_phy_set_wirespeed(struct tg3 *tp)
{
	u32 val;

	if (tp->tg3_flags2 & TG3_FLG2_NO_ETH_WIRE_SPEED)
		return;

	if (!tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x7007) &&
	    !tg3_readphy(tp, MII_TG3_AUX_CTRL, &val))
		tg3_writephy(tp, MII_TG3_AUX_CTRL,
			     (val | (1 << 15) | (1 << 4)));
}

static void tg3_phy_apply_otp(struct tg3 *tp)
{
	u32 otp, phy;

	if (!tp->phy_otp)
		return;

	otp = tp->phy_otp;

	/* Enable SM_DSP clock and tx 6dB coding. */
	phy = MII_TG3_AUXCTL_SHDWSEL_AUXCTL |
	      MII_TG3_AUXCTL_ACTL_SMDSP_ENA |
	      MII_TG3_AUXCTL_ACTL_TX_6DB;
	tg3_writephy(tp, MII_TG3_AUX_CTRL, phy);

	phy = ((otp & TG3_OTP_AGCTGT_MASK) >> TG3_OTP_AGCTGT_SHIFT);
	phy |= MII_TG3_DSP_TAP1_AGCTGT_DFLT;
	tg3_phydsp_write(tp, MII_TG3_DSP_TAP1, phy);

	phy = ((otp & TG3_OTP_HPFFLTR_MASK) >> TG3_OTP_HPFFLTR_SHIFT) |
	      ((otp & TG3_OTP_HPFOVER_MASK) >> TG3_OTP_HPFOVER_SHIFT);
	tg3_phydsp_write(tp, MII_TG3_DSP_AADJ1CH0, phy);

	phy = ((otp & TG3_OTP_LPFDIS_MASK) >> TG3_OTP_LPFDIS_SHIFT);
	phy |= MII_TG3_DSP_AADJ1CH3_ADCCKADJ;
	tg3_phydsp_write(tp, MII_TG3_DSP_AADJ1CH3, phy);

	phy = ((otp & TG3_OTP_VDAC_MASK) >> TG3_OTP_VDAC_SHIFT);
	tg3_phydsp_write(tp, MII_TG3_DSP_EXP75, phy);

	phy = ((otp & TG3_OTP_10BTAMP_MASK) >> TG3_OTP_10BTAMP_SHIFT);
	tg3_phydsp_write(tp, MII_TG3_DSP_EXP96, phy);

	phy = ((otp & TG3_OTP_ROFF_MASK) >> TG3_OTP_ROFF_SHIFT) |
	      ((otp & TG3_OTP_RCOFF_MASK) >> TG3_OTP_RCOFF_SHIFT);
	tg3_phydsp_write(tp, MII_TG3_DSP_EXP97, phy);

	/* Turn off SM_DSP clock. */
	phy = MII_TG3_AUXCTL_SHDWSEL_AUXCTL |
	      MII_TG3_AUXCTL_ACTL_TX_6DB;
	tg3_writephy(tp, MII_TG3_AUX_CTRL, phy);
}

static int tg3_wait_macro_done(struct tg3 *tp)
{
	int limit = 100;

	while (limit--) {
		u32 tmp32;

		if (!tg3_readphy(tp, 0x16, &tmp32)) {
			if ((tmp32 & 0x1000) == 0)
				break;
		}
	}
	if (limit < 0)
		return -EBUSY;

	return 0;
}

static int tg3_phy_write_and_check_testpat(struct tg3 *tp, int *resetp)
{
	static const u32 test_pat[4][6] = {
	{ 0x00005555, 0x00000005, 0x00002aaa, 0x0000000a, 0x00003456, 0x00000003 },
	{ 0x00002aaa, 0x0000000a, 0x00003333, 0x00000003, 0x0000789a, 0x00000005 },
	{ 0x00005a5a, 0x00000005, 0x00002a6a, 0x0000000a, 0x00001bcd, 0x00000003 },
	{ 0x00002a5a, 0x0000000a, 0x000033c3, 0x00000003, 0x00002ef1, 0x00000005 }
	};
	int chan;

	for (chan = 0; chan < 4; chan++) {
		int i;

		tg3_writephy(tp, MII_TG3_DSP_ADDRESS,
			     (chan * 0x2000) | 0x0200);
		tg3_writephy(tp, 0x16, 0x0002);

		for (i = 0; i < 6; i++)
			tg3_writephy(tp, MII_TG3_DSP_RW_PORT,
				     test_pat[chan][i]);

		tg3_writephy(tp, 0x16, 0x0202);
		if (tg3_wait_macro_done(tp)) {
			*resetp = 1;
			return -EBUSY;
		}

		tg3_writephy(tp, MII_TG3_DSP_ADDRESS,
			     (chan * 0x2000) | 0x0200);
		tg3_writephy(tp, 0x16, 0x0082);
		if (tg3_wait_macro_done(tp)) {
			*resetp = 1;
			return -EBUSY;
		}

		tg3_writephy(tp, 0x16, 0x0802);
		if (tg3_wait_macro_done(tp)) {
			*resetp = 1;
			return -EBUSY;
		}

		for (i = 0; i < 6; i += 2) {
			u32 low, high;

			if (tg3_readphy(tp, MII_TG3_DSP_RW_PORT, &low) ||
			    tg3_readphy(tp, MII_TG3_DSP_RW_PORT, &high) ||
			    tg3_wait_macro_done(tp)) {
				*resetp = 1;
				return -EBUSY;
			}
			low &= 0x7fff;
			high &= 0x000f;
			if (low != test_pat[chan][i] ||
			    high != test_pat[chan][i+1]) {
				tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x000b);
				tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x4001);
				tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x4005);

				return -EBUSY;
			}
		}
	}

	return 0;
}

static int tg3_phy_reset_chanpat(struct tg3 *tp)
{
	int chan;

	for (chan = 0; chan < 4; chan++) {
		int i;

		tg3_writephy(tp, MII_TG3_DSP_ADDRESS,
			     (chan * 0x2000) | 0x0200);
		tg3_writephy(tp, 0x16, 0x0002);
		for (i = 0; i < 6; i++)
			tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x000);
		tg3_writephy(tp, 0x16, 0x0202);
		if (tg3_wait_macro_done(tp))
			return -EBUSY;
	}

	return 0;
}

static int tg3_phy_reset_5703_4_5(struct tg3 *tp)
{
	u32 reg32, phy9_orig;
	int retries, do_phy_reset, err;

	retries = 10;
	do_phy_reset = 1;
	do {
		if (do_phy_reset) {
			err = tg3_bmcr_reset(tp);
			if (err)
				return err;
			do_phy_reset = 0;
		}

		/* Disable transmitter and interrupt.  */
		if (tg3_readphy(tp, MII_TG3_EXT_CTRL, &reg32))
			continue;

		reg32 |= 0x3000;
		tg3_writephy(tp, MII_TG3_EXT_CTRL, reg32);

		/* Set full-duplex, 1000 mbps.  */
		tg3_writephy(tp, MII_BMCR,
			     BMCR_FULLDPLX | TG3_BMCR_SPEED1000);

		/* Set to master mode.  */
		if (tg3_readphy(tp, MII_TG3_CTRL, &phy9_orig))
			continue;

		tg3_writephy(tp, MII_TG3_CTRL,
			     (MII_TG3_CTRL_AS_MASTER |
			      MII_TG3_CTRL_ENABLE_AS_MASTER));

		/* Enable SM_DSP_CLOCK and 6dB.  */
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0c00);

		/* Block the PHY control access.  */
		tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x8005);
		tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x0800);

		err = tg3_phy_write_and_check_testpat(tp, &do_phy_reset);
		if (!err)
			break;
	} while (--retries);

	err = tg3_phy_reset_chanpat(tp);
	if (err)
		return err;

	tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x8005);
	tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x0000);

	tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x8200);
	tg3_writephy(tp, 0x16, 0x0000);

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5703 ||
	    GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704) {
		/* Set Extended packet length bit for jumbo frames */
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x4400);
	}
	else {
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0400);
	}

	tg3_writephy(tp, MII_TG3_CTRL, phy9_orig);

	if (!tg3_readphy(tp, MII_TG3_EXT_CTRL, &reg32)) {
		reg32 &= ~0x3000;
		tg3_writephy(tp, MII_TG3_EXT_CTRL, reg32);
	} else if (!err)
		err = -EBUSY;

	return err;
}

/* This will reset the tigon3 PHY if there is no valid
 * link unless the FORCE argument is non-zero.
 */
static int tg3_phy_reset(struct tg3 *tp)
{
	u32 cpmuctrl;
	u32 phy_status;
	int err;

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5906) {
		u32 val;

		val = tr32(GRC_MISC_CFG);
		tw32_f(GRC_MISC_CFG, val & ~GRC_MISC_CFG_EPHY_IDDQ);
		udelay(40);
	}
	err  = tg3_readphy(tp, MII_BMSR, &phy_status);
	err |= tg3_readphy(tp, MII_BMSR, &phy_status);
	if (err != 0)
		return -EBUSY;

	if (netif_running(tp->dev) && netif_carrier_ok(tp->dev)) {
		netif_carrier_off(tp->dev);
		tg3_link_report(tp);
	}

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5703 ||
	    GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704 ||
	    GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5705) {
		err = tg3_phy_reset_5703_4_5(tp);
		if (err)
			return err;
		goto out;
	}

	cpmuctrl = 0;
	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5784 &&
	    GET_CHIP_REV(tp->pci_chip_rev_id) != CHIPREV_5784_AX) {
		cpmuctrl = tr32(TG3_CPMU_CTRL);
		if (cpmuctrl & CPMU_CTRL_GPHY_10MB_RXONLY)
			tw32(TG3_CPMU_CTRL,
			     cpmuctrl & ~CPMU_CTRL_GPHY_10MB_RXONLY);
	}

	err = tg3_bmcr_reset(tp);
	if (err)
		return err;

	if (cpmuctrl & CPMU_CTRL_GPHY_10MB_RXONLY) {
		u32 phy;

		phy = MII_TG3_DSP_EXP8_AEDW | MII_TG3_DSP_EXP8_REJ2MHz;
		tg3_phydsp_write(tp, MII_TG3_DSP_EXP8, phy);

		tw32(TG3_CPMU_CTRL, cpmuctrl);
	}

	if (GET_CHIP_REV(tp->pci_chip_rev_id) == CHIPREV_5784_AX ||
	    GET_CHIP_REV(tp->pci_chip_rev_id) == CHIPREV_5761_AX) {
		u32 val;

		val = tr32(TG3_CPMU_LSPD_1000MB_CLK);
		if ((val & CPMU_LSPD_1000MB_MACCLK_MASK) ==
		    CPMU_LSPD_1000MB_MACCLK_12_5) {
			val &= ~CPMU_LSPD_1000MB_MACCLK_MASK;
			udelay(40);
			tw32_f(TG3_CPMU_LSPD_1000MB_CLK, val);
		}
	}

	tg3_phy_apply_otp(tp);

	if (tp->tg3_flags3 & TG3_FLG3_PHY_ENABLE_APD)
		tg3_phy_toggle_apd(tp, true);
	else
		tg3_phy_toggle_apd(tp, false);

out:
	if (tp->tg3_flags2 & TG3_FLG2_PHY_ADC_BUG) {
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0c00);
		tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x201f);
		tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x2aaa);
		tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x000a);
		tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x0323);
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0400);
	}
	if (tp->tg3_flags2 & TG3_FLG2_PHY_5704_A0_BUG) {
		tg3_writephy(tp, 0x1c, 0x8d68);
		tg3_writephy(tp, 0x1c, 0x8d68);
	}
	if (tp->tg3_flags2 & TG3_FLG2_PHY_BER_BUG) {
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0c00);
		tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x000a);
		tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x310b);
		tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x201f);
		tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x9506);
		tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x401f);
		tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x14e2);
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0400);
	}
	else if (tp->tg3_flags2 & TG3_FLG2_PHY_JITTER_BUG) {
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0c00);
		tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x000a);
		if (tp->tg3_flags2 & TG3_FLG2_PHY_ADJUST_TRIM) {
			tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x110b);
			tg3_writephy(tp, MII_TG3_TEST1,
				     MII_TG3_TEST1_TRIM_EN | 0x4);
		} else
			tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x010b);
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0400);
	}
	/* Set Extended packet length bit (bit 14) on all chips that */
	/* support jumbo frames */
	if ((tp->phy_id & PHY_ID_MASK) == PHY_ID_BCM5401) {
		/* Cannot do read-modify-write on 5401 */
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x4c20);
	} else if (tp->tg3_flags & TG3_FLAG_JUMBO_CAPABLE) {
		u32 phy_reg;

		/* Set bit 14 with read-modify-write to preserve other bits */
		if (!tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0007) &&
		    !tg3_readphy(tp, MII_TG3_AUX_CTRL, &phy_reg))
			tg3_writephy(tp, MII_TG3_AUX_CTRL, phy_reg | 0x4000);
	}

	/* Set phy register 0x10 bit 0 to high fifo elasticity to support
	 * jumbo frames transmission.
	 */
	if (tp->tg3_flags & TG3_FLAG_JUMBO_CAPABLE) {
		u32 phy_reg;

		if (!tg3_readphy(tp, MII_TG3_EXT_CTRL, &phy_reg))
		    tg3_writephy(tp, MII_TG3_EXT_CTRL,
				 phy_reg | MII_TG3_EXT_CTRL_FIFO_ELASTIC);
	}

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5906) {
		/* adjust output voltage */
		tg3_writephy(tp, MII_TG3_FET_PTEST, 0x12);
	}

	tg3_phy_toggle_automdix(tp, 1);
	tg3_phy_set_wirespeed(tp);
	return 0;
}

static void tg3_frob_aux_power(struct tg3 *tp)
{
	struct tg3 *tp_peer = tp;

	if ((tp->tg3_flags2 & TG3_FLG2_IS_NIC) == 0)
		return;

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704 ||
	    GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5714 ||
	    GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5717) {
		struct net_device *dev_peer;

		dev_peer = pci_get_drvdata(tp->pdev_peer);
		/* remove_one() may have been run on the peer. */
		if (!dev_peer)
			tp_peer = tp;
		else
			tp_peer = netdev_priv(dev_peer);
	}

	if ((tp->tg3_flags & TG3_FLAG_WOL_ENABLE) != 0 ||
	    (tp->tg3_flags & TG3_FLAG_ENABLE_ASF) != 0 ||
	    (tp_peer->tg3_flags & TG3_FLAG_WOL_ENABLE) != 0 ||
	    (tp_peer->tg3_flags & TG3_FLAG_ENABLE_ASF) != 0) {
		if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 ||
		    GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5701) {
			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ctrl |
				    (GRC_LCLCTRL_GPIO_OE0 |
				     GRC_LCLCTRL_GPIO_OE1 |
				     GRC_LCLCTRL_GPIO_OE2 |
				     GRC_LCLCTRL_GPIO_OUTPUT0 |
				     GRC_LCLCTRL_GPIO_OUTPUT1),
				    100);
		} else if (tp->pdev->device == PCI_DEVICE_ID_TIGON3_5761 ||
			   tp->pdev->device == TG3PCI_DEVICE_TIGON3_5761S) {
			/* The 5761 non-e device swaps GPIO 0 and GPIO 2. */
			u32 grc_local_ctrl = GRC_LCLCTRL_GPIO_OE0 |
					     GRC_LCLCTRL_GPIO_OE1 |
					     GRC_LCLCTRL_GPIO_OE2 |
					     GRC_LCLCTRL_GPIO_OUTPUT0 |
					     GRC_LCLCTRL_GPIO_OUTPUT1 |
					     tp->grc_local_ctrl;
			tw32_wait_f(GRC_LOCAL_CTRL, grc_local_ctrl, 100);

			grc_local_ctrl |= GRC_LCLCTRL_GPIO_OUTPUT2;
			tw32_wait_f(GRC_LOCAL_CTRL, grc_local_ctrl, 100);

			grc_local_ctrl &= ~GRC_LCLCTRL_GPIO_OUTPUT0;
			tw32_wait_f(GRC_LOCAL_CTRL, grc_local_ctrl, 100);
		} else {
			u32 no_gpio2;
			u32 grc_local_ctrl = 0;

			if (tp_peer != tp &&
			    (tp_peer->tg3_flags & TG3_FLAG_INIT_COMPLETE) != 0)
				return;

			/* Workaround to prevent overdrawing Amps. */
			if (GET_ASIC_REV(tp->pci_chip_rev_id) ==
			    ASIC_REV_5714) {
				grc_local_ctrl |= GRC_LCLCTRL_GPIO_OE3;
				tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ctrl |
					    grc_local_ctrl, 100);
			}

			/* On 5753 and variants, GPIO2 cannot be used. */
			no_gpio2 = tp->nic_sram_data_cfg &
				    NIC_SRAM_DATA_CFG_NO_GPIO2;

			grc_local_ctrl |= GRC_LCLCTRL_GPIO_OE0 |
					 GRC_LCLCTRL_GPIO_OE1 |
					 GRC_LCLCTRL_GPIO_OE2 |
					 GRC_LCLCTRL_GPIO_OUTPUT1 |
					 GRC_LCLCTRL_GPIO_OUTPUT2;
			if (no_gpio2) {
				grc_local_ctrl &= ~(GRC_LCLCTRL_GPIO_OE2 |
						    GRC_LCLCTRL_GPIO_OUTPUT2);
			}
			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ctrl |
						    grc_local_ctrl, 100);

			grc_local_ctrl |= GRC_LCLCTRL_GPIO_OUTPUT0;

			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ctrl |
						    grc_local_ctrl, 100);

			if (!no_gpio2) {
				grc_local_ctrl &= ~GRC_LCLCTRL_GPIO_OUTPUT2;
				tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ctrl |
					    grc_local_ctrl, 100);
			}
		}
	} else {
		if (GET_ASIC_REV(tp->pci_chip_rev_id) != ASIC_REV_5700 &&
		    GET_ASIC_REV(tp->pci_chip_rev_id) != ASIC_REV_5701) {
			if (tp_peer != tp &&
			    (tp_peer->tg3_flags & TG3_FLAG_INIT_COMPLETE) != 0)
				return;

			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ctrl |
				    (GRC_LCLCTRL_GPIO_OE1 |
				     GRC_LCLCTRL_GPIO_OUTPUT1), 100);

			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ctrl |
				    GRC_LCLCTRL_GPIO_OE1, 100);

			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ctrl |
				    (GRC_LCLCTRL_GPIO_OE1 |
				     GRC_LCLCTRL_GPIO_OUTPUT1), 100);
		}
	}
}

static int tg3_5700_link_polarity(struct tg3 *tp, u32 speed)
{
	if (tp->led_ctrl == LED_CTRL_MODE_PHY_2)
		return 1;
	else if ((tp->phy_id & PHY_ID_MASK) == PHY_ID_BCM5411) {
		if (speed != SPEED_10)
			return 1;
	} else if (speed == SPEED_10)
		return 1;

	return 0;
}

static int tg3_setup_phy(struct tg3 *, int);

#define RESET_KIND_SHUTDOWN	0
#define RESET_KIND_INIT		1
#define RESET_KIND_SUSPEND	2

static void tg3_write_sig_post_reset(struct tg3 *, int);
static int tg3_halt_cpu(struct tg3 *, u32);

static void tg3_power_down_phy(struct tg3 *tp, bool do_low_power)
{
	u32 val;

	if (tp->tg3_flags2 & TG3_FLG2_PHY_SERDES) {
		if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704) {
			u32 sg_dig_ctrl = tr32(SG_DIG_CTRL);
			u32 serdes_cfg = tr32(MAC_SERDES_CFG);

			sg_dig_ctrl |=
				SG_DIG_USING_HW_AUTONEG | SG_DIG_SOFT_RESET;
			tw32(SG_DIG_CTRL, sg_dig_ctrl);
			tw32(MAC_SERDES_CFG, serdes_cfg | (1 << 15));
		}
		return;
	}

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5906) {
		tg3_bmcr_reset(tp);
		val = tr32(GRC_MISC_CFG);
		tw32_f(GRC_MISC_CFG, val | GRC_MISC_CFG_EPHY_IDDQ);
		udelay(40);
		return;
	} else if (do_low_power) {
		tg3_writephy(tp, MII_TG3_EXT_CTRL,
			     MII_TG3_EXT_CTRL_FORCE_LED_OFF);

		tg3_writephy(tp, MII_TG3_AUX_CTRL,
			     MII_TG3_AUXCTL_SHDWSEL_PWRCTL |
			     MII_TG3_AUXCTL_PCTL_100TX_LPWR |
			     MII_TG3_AUXCTL_PCTL_SPR_ISOLATE |
			     MII_TG3_AUXCTL_PCTL_VREG_11V);
	}

	/* The PHY should not be powered down on some chips because
	 * of bugs.
	 */
	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 ||
	    GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704 ||
	    (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5780 &&
	     (tp->tg3_flags2 & TG3_FLG2_MII_SERDES)))
		return;

	if (GET_CHIP_REV(tp->pci_chip_rev_id) == CHIPREV_5784_AX ||
	    GET_CHIP_REV(tp->pci_chip_rev_id) == CHIPREV_5761_AX) {
		val = tr32(TG3_CPMU_LSPD_1000MB_CLK);
		val &= ~CPMU_LSPD_1000MB_MACCLK_MASK;
		val |= CPMU_LSPD_1000MB_MACCLK_12_5;
		tw32_f(TG3_CPMU_LSPD_1000MB_CLK, val);
	}

	tg3_writephy(tp, MII_BMCR, BMCR_PDOWN);
}

/* tp->lock is held. */
static int tg3_nvram_lock(struct tg3 *tp)
{
	if (tp->tg3_flags & TG3_FLAG_NVRAM) {
		int i;

		if (tp->nvram_lock_cnt == 0) {
			tw32(NVRAM_SWARB, SWARB_REQ_SET1);
			for (i = 0; i < 8000; i++) {
				if (tr32(NVRAM_SWARB) & SWARB_GNT1)
					break;
				udelay(20);
			}
			if (i == 8000) {
				tw32(NVRAM_SWARB, SWARB_REQ_CLR1);
				return -ENODEV;
			}
		}
		tp->nvram_lock_cnt++;
	}
	return 0;
}

/* tp->lock is held. */
static void tg3_nvram_unlock(struct tg3 *tp)
{
	if (tp->tg3_flags & TG3_FLAG_NVRAM) {
		if (tp->nvram_lock_cnt > 0)
			tp->nvram_lock_cnt--;
		if (tp->nvram_lock_cnt == 0)
			tw32_f(NVRAM_SWARB, SWARB_REQ_CLR1);
	}
}

/* tp->lock is held. */
static void tg3_enable_nvram_access(struct tg3 *tp)
{
	if ((tp->tg3_flags2 & TG3_FLG2_5750_PLUS) &&
	    !(tp->tg3_flags2 & TG3_FLG2_PROTECTED_NVRAM)) {
		u32 nvaccess = tr32(NVRAM_ACCESS);

		tw32(NVRAM_ACCESS, nvaccess | ACCESS_ENABLE);
	}
}

/* tp->lock is held. */
static void tg3_disable_nvram_access(struct tg3 *tp)
{
	if ((tp->tg3_flags2 & TG3_FLG2_5750_PLUS) &&
	    !(tp->tg3_flags2 & TG3_FLG2_PROTECTED_NVRAM)) {
		u32 nvaccess = tr32(NVRAM_ACCESS);

		tw32(NVRAM_ACCESS, nvaccess & ~ACCESS_ENABLE);
	}
}

static int tg3_nvram_read_using_eeprom(struct tg3 *tp,
					u32 offset, u32 *val)
{
	u32 tmp;
	int i;

	if (offset > EEPROM_ADDR_ADDR_MASK || (offset % 4) != 0)
		return -EINVAL;

	tmp = tr32(GRC_EEPROM_ADDR) & ~(EEPROM_ADDR_ADDR_MASK |
					EEPROM_ADDR_DEVID_MASK |
					EEPROM_ADDR_READ);
	tw32(GRC_EEPROM_ADDR,
	     tmp |
	     (0 << EEPROM_ADDR_DEVID_SHIFT) |
	     ((offset << EEPROM_ADDR_ADDR_SHIFT) &
	      EEPROM_ADDR_ADDR_MASK) |
	     EEPROM_ADDR_READ | EEPROM_ADDR_START);

	for (i = 0; i < 1000; i++) {
		tmp = tr32(GRC_EEPROM_ADDR);

		if (tmp & EEPROM_ADDR_COMPLETE)
			break;
		msleep(1);
	}
	if (!(tmp & EEPROM_ADDR_COMPLETE))
		return -EBUSY;

	tmp = tr32(GRC_EEPROM_DATA);

	/*
	 * The data will always be opposite the native endian
	 * format.  Perform a blind byteswap to compensate.
	 */
	*val = swab32(tmp);

	return 0;
}

#define NVRAM_CMD_TIMEOUT 10000

static int tg3_nvram_exec_cmd(struct tg3 *tp, u32 nvram_cmd)
{
	int i;

	tw32(NVRAM_CMD, nvram_cmd);
	for (i = 0; i < NVRAM_CMD_TIMEOUT; i++) {
		udelay(10);
		if (tr32(NVRAM_CMD) & NVRAM_CMD_DONE) {
			udelay(10);
			break;
		}
	}

	if (i == NVRAM_CMD_TIMEOUT)
		return -EBUSY;

	return 0;
}

static u32 tg3_nvram_phys_addr(struct tg3 *tp, u32 addr)
{
	if ((tp->tg3_flags & TG3_FLAG_NVRAM) &&
	    (tp->tg3_flags & TG3_FLAG_NVRAM_BUFFERED) &&
	    (tp->tg3_flags2 & TG3_FLG2_FLASH) &&
	   !(tp->tg3_flags3 & TG3_FLG3_NO_NVRAM_ADDR_TRANS) &&
	    (tp->nvram_jedecnum == JEDEC_ATMEL))

		addr = ((addr / tp->nvram_pagesize) <<
			ATMEL_AT45DB0X1B_PAGE_POS) +
		       (addr % tp->nvram_pagesize);

	return addr;
}

static u32 tg3_nvram_logical_addr(struct tg3 *tp, u32 addr)
{
	if ((tp->tg3_flags & TG3_FLAG_NVRAM) &&
	    (tp->tg3_flags & TG3_FLAG_NVRAM_BUFFERED) &&
	    (tp->tg3_flags2 & TG3_FLG2_FLASH) &&
	   !(tp->tg3_flags3 & TG3_FLG3_NO_NVRAM_ADDR_TRANS) &&
	    (tp->nvram_jedecnum == JEDEC_ATMEL))

		addr = ((addr >> ATMEL_AT45DB0X1B_PAGE_POS) *
			tp->nvram_pagesize) +
		       (addr & ((1 << ATMEL_AT45DB0X1B_PAGE_POS) - 1));

	return addr;
}

/* NOTE: Data read in from NVRAM is byteswapped according to
 * the byteswapping settings for all other register accesses.
 * tg3 devices are BE devices, so on a BE machine, the data
 * returned will be exactly as it is seen in NVRAM.  On a LE
 * machine, the 32-bit value will be byteswapped.
 */
static int tg3_nvram_read(struct tg3 *tp, u32 offset, u32 *val)
{
	int ret;

	if (!(tp->tg3_flags & TG3_FLAG_NVRAM))
		return tg3_nvram_read_using_eeprom(tp, offset, val);

	offset = tg3_nvram_phys_addr(tp, offset);

	if (offset > NVRAM_ADDR_MSK)
		return -EINVAL;

	ret = tg3_nvram_lock(tp);
	if (ret)
		return ret;

	tg3_enable_nvram_access(tp);

	tw32(NVRAM_ADDR, offset);
	ret = tg3_nvram_exec_cmd(tp, NVRAM_CMD_RD | NVRAM_CMD_GO |
		NVRAM_CMD_FIRST | NVRAM_CMD_LAST | NVRAM_CMD_DONE);

	if (ret == 0)
		*val = tr32(NVRAM_RDDATA);

	tg3_disable_nvram_access(tp);

	tg3_nvram_unlock(tp);

	return ret;
}

/* Ensures NVRAM data is in bytestream format. */
static int tg3_nvram_read_be32(struct tg3 *tp, u32 offset, __be32 *val)
{
	u32 v;
	int res = tg3_nvram_read(tp, offset, &v);
	if (!res)
		*val = cpu_to_be32(v);
	return res;
}

/* tp->lock is held. */
static void __tg3_set_mac_addr(struct tg3 *tp, int skip_mac_1)
{
	u32 addr_high, addr_low;
	int i;

	addr_high = ((tp->dev->dev_addr[0] << 8) |
		     tp->dev->dev_addr[1]);
	addr_low = ((tp->dev->dev_addr[2] << 24) |
		    (tp->dev->dev_addr[3] << 16) |
		    (tp->dev->dev_addr[4] <<  8) |
		    (tp->dev->dev_addr[5] <<  0));
	for (i = 0; i < 4; i++) {
		if (i == 1 && skip_mac_1)
			continue;
		tw32(MAC_ADDR_0_HIGH + (i * 8), addr_high);
		tw32(MAC_ADDR_0_LOW + (i * 8), addr_low);
	}

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5703 ||
	    GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704) {
		for (i = 0; i < 12; i++) {
			tw32(MAC_EXTADDR_0_HIGH + (i * 8), addr_high);
			tw32(MAC_EXTADDR_0_LOW + (i * 8), addr_low);
		}
	}

	addr_high = (tp->dev->dev_addr[0] +
		     tp->dev->dev_addr[1] +
		     tp->dev->dev_addr[2] +
		     tp->dev->dev_addr[3] +
		     tp->dev->dev_addr[4] +
		     tp->dev->dev_addr[5]) &
		TX_BACKOFF_SEED_MASK;
	tw32(MAC_TX_BACKOFF_SEED, addr_high);
}

static int tg3_set_power_state(struct tg3 *tp, pci_power_t state)
{
	u32 misc_host_ctrl;
	bool device_should_wake, do_low_power;

	/* Make sure register accesses (indirect or otherwise)
	 * will function correctly.
	 */
	pci_write_config_dword(tp->pdev,
			       TG3PCI_MISC_HOST_CTRL,
			       tp->misc_host_ctrl);

	switch (state) {
	case PCI_D0:
		pci_enable_wake(tp->pdev, state, false);
		pci_set_power_state(tp->pdev, PCI_D0);

		/* Switch out of Vaux if it is a NIC */
		if (tp->tg3_flags2 & TG3_FLG2_IS_NIC)
			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ctrl, 100);

		return 0;

	case PCI_D1:
	case PCI_D2:
	case PCI_D3hot:
		break;

	default:
		printk(KERN_ERR PFX "%s: Invalid power state (D%d) requested\n",
			tp->dev->name, state);
		return -EINVAL;
	}

	/* Restore the CLKREQ setting. */
	if (tp->tg3_flags3 & TG3_FLG3_CLKREQ_BUG) {
		u16 lnkctl;

		pci_read_config_word(tp->pdev,
				     tp->pcie_cap + PCI_EXP_LNKCTL,
				     &lnkctl);
		lnkctl |= PCI_EXP_LNKCTL_CLKREQ_EN;
		pci_write_config_word(tp->pdev,
				      tp->pcie_cap + PCI_EXP_LNKCTL,
				      lnkctl);
	}

	misc_host_ctrl = tr32(TG3PCI_MISC_HOST_CTRL);
	tw32(TG3PCI_MISC_HOST_CTRL,
	     misc_host_ctrl | MISC_HOST_CTRL_MASK_PCI_INT);

	device_should_wake = pci_pme_capable(tp->pdev, state) &&
			     device_may_wakeup(&tp->pdev->dev) &&
			     (tp->tg3_flags & TG3_FLAG_WOL_ENABLE);

	if (tp->tg3_flags3 & TG3_FLG3_USE_PHYLIB) {
		do_low_power = false;
		if ((tp->tg3_flags3 & TG3_FLG3_PHY_CONNECTED) &&
		    !tp->link_config.phy_is_low_power) {
			struct phy_device *phydev;
			u32 phyid, advertising;

			phydev = tp->mdio_bus->phy_map[PHY_ADDR];

			tp->link_config.phy_is_low_power = 1;

			tp->link_config.orig_speed = phydev->speed;
			tp->link_config.orig_duplex = phydev->duplex;
			tp->link_config.orig_autoneg = phydev->autoneg;
			tp->link_config.orig_advertising = phydev->advertising;

			advertising = ADVERTISED_TP |
				      ADVERTISED_Pause |
				      ADVERTISED_Autoneg |
				      ADVERTISED_10baseT_Half;

			if ((tp->tg3_flags & TG3_FLAG_ENABLE_ASF) ||
			    device_should_wake) {
				if (tp->tg3_flags & TG3_FLAG_WOL_SPEED_100MB)
					advertising |=
						ADVERTISED_100baseT_Half |
						ADVERTISED_100baseT_Full |
						ADVERTISED_10baseT_Full;
				else
					advertising |= ADVERTISED_10baseT_Full;
			}

			phydev->advertising = advertising;

			phy_start_aneg(phydev);

			phyid = phydev->drv->phy_id & phydev->drv->phy_id_mask;
			if (phyid != TG3_PHY_ID_BCMAC131) {
				phyid &= TG3_PHY_OUI_MASK;
				if (phyid == TG3_PHY_OUI_1 ||
				    phyid == TG3_PHY_OUI_2 ||
				    phyid == TG3_PHY_OUI_3)
					do_low_power = true;
			}
		}
	} else {
		do_low_power = true;

		if (tp->link_config.phy_is_low_power == 0) {
			tp->link_config.phy_is_low_power = 1;
			tp->link_config.orig_speed = tp->link_config.speed;
			tp->link_config.orig_duplex = tp->link_config.duplex;
			tp->link_config.orig_autoneg = tp->link_config.autoneg;
		}

		if (!(tp->tg3_flags2 & TG3_FLG2_ANY_SERDES)) {
			tp->link_config.speed = SPEED_10;
			tp->link_config.duplex = DUPLEX_HALF;
			tp->link_config.autoneg = AUTONEG_ENABLE;
			tg3_setup_phy(tp, 0);
		}
	}

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5906) {
		u32 val;

		val = tr32(GRC_VCPU_EXT_CTRL);
		tw32(GRC_VCPU_EXT_CTRL, val | GRC_VCPU_EXT_CTRL_DISABLE_WOL);
	} else if (!(tp->tg3_flags & TG3_FLAG_ENABLE_ASF)) {
		int i;
		u32 val;

		for (i = 0; i < 200; i++) {
			tg3_read_mem(tp, NIC_SRAM_FW_ASF_STATUS_MBOX, &val);
			if (val == ~NIC_SRAM_FIRMWARE_MBOX_MAGIC1)
				break;
			msleep(1);
		}
	}
	if (tp->tg3_flags & TG3_FLAG_WOL_CAP)
		tg3_write_mem(tp, NIC_SRAM_WOL_MBOX, WOL_SIGNATURE |
						     WOL_DRV_STATE_SHUTDOWN |
						     WOL_DRV_WOL |
						     WOL_SET_MAGIC_PKT);

	if (device_should_wake) {
		u32 mac_mode;

		if (!(tp->tg3_flags2 & TG3_FLG2_PHY_SERDES)) {
			if (do_low_power) {
				tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x5a);
				udelay(40);
			}

			if (tp->tg3_flags2 & TG3_FLG2_MII_SERDES)
				mac_mode = MAC_MODE_PORT_MODE_GMII;
			else
				mac_mode = MAC_MODE_PORT_MODE_MII;

			mac_mode |= tp->mac_mode & MAC_MODE_LINK_POLARITY;
			if (GET_ASIC_REV(tp->pci_chip_rev_id) ==
			    ASIC_REV_5700) {
				u32 speed = (tp->tg3_flags &
					     TG3_FLAG_WOL_SPEED_100MB) ?
					     SPEED_100 : SPEED_10;
				if (tg3_5700_link_polarity(tp, speed))
					mac_mode |= MAC_MODE_LINK_POLARITY;
				else
					mac_mode &= ~MAC_MODE_LINK_POLARITY;
			}
		} else {
			mac_mode = MAC_MODE_PORT_MODE_TBI;
		}

		if (!(tp->tg3_flags2 & TG3_FLG2_5750_PLUS))
			tw32(MAC_LED_CTRL, tp->led_ctrl);

		mac_mode |= MAC_MODE_MAGIC_PKT_ENABLE;
		if (((tp->tg3_flags2 & TG3_FLG2_5705_PLUS) &&
		    !(tp->tg3_flags2 & TG3_FLG2_5780_CLASS)) &&
		    ((tp->tg3_flags & TG3_FLAG_ENABLE_ASF) ||
		     (tp->tg3_flags3 & TG3_FLG3_ENABLE_APE)))
			mac_mode |= MAC_MODE_KEEP_FRAME_IN_WOL;

		if (tp->tg3_flags3 & TG3_FLG3_ENABLE_APE) {
			mac_mode |= tp->mac_mode &
				    (MAC_MODE_APE_TX_EN | MAC_MODE_APE_RX_EN);
			if (mac_mode & MAC_MODE_APE_TX_EN)
				mac_mode |= MAC_MODE_TDE_ENABLE;
		}

		tw32_f(MAC_MODE, mac_mode);
		udelay(100);

		tw32_f(MAC_RX_MODE, RX_MODE_ENABLE);
		udelay(10);
	}

	if (!(tp->tg3_flags & TG3_FLAG_WOL_SPEED_100MB) &&
	    (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 ||
	     GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5701)) {
		u32 base_val;

		base_val = tp->pci_clock_ctrl;
		base_val |= (CLOCK_CTRL_RXCLK_DISABLE |
			     CLOCK_CTRL_TXCLK_DISABLE);

		tw32_wait_f(TG3PCI_CLOCK_CTRL, base_val | CLOCK_CTRL_ALTCLK |
			    CLOCK_CTRL_PWRDOWN_PLL133, 40);
	} else if ((tp->tg3_flags2 & TG3_FLG2_5780_CLASS) ||
		   (tp->tg3_flags & TG3_FLAG_CPMU_PRESENT) ||
		   (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5906)) {
		/* do nothing */
	} else if (!((tp->tg3_flags2 & TG3_FLG2_5750_PLUS) &&
		     (tp->tg3_flags & TG3_FLAG_ENABLE_ASF))) {
		u32 newbits1, newbits2;

		if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 ||
		    GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5701) {
			newbits1 = (CLOCK_CTRL_RXCLK_DISABLE |
				    CLOCK_CTRL_TXCLK_DISABLE |
				    CLOCK_CTRL_ALTCLK);
			newbits2 = newbits1 | CLOCK_CTRL_44MHZ_CORE;
		} else if (tp->tg3_flags2 & TG3_FLG2_5705_PLUS) {
			newbits1 = CLOCK_CTRL_625_CORE;
			newbits2 = newbits1 | CLOCK_CTRL_ALTCLK;
		} else {
			newbits1 = CLOCK_CTRL_ALTCLK;
			newbits2 = newbits1 | CLOCK_CTRL_44MHZ_CORE;
		}

		tw32_wait_f(TG3PCI_CLOCK_CTRL, tp->pci_clock_ctrl | newbits1,
			    40);

		tw32_wait_f(TG3PCI_CLOCK_CTRL, tp->pci_clock_ctrl | newbits2,
			    40);

		if (!(tp->tg3_flags2 & TG3_FLG2_5705_PLUS)) {
			u32 newbits3;

			if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 ||
			    GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5701) {
				newbits3 = (CLOCK_CTRL_RXCLK_DISABLE |
					    CLOCK_CTRL_TXCLK_DISABLE |
					    CLOCK_CTRL_44MHZ_CORE);
			} else {
				newbits3 = CLOCK_CTRL_44MHZ_CORE;
			}

			tw32_wait_f(TG3PCI_CLOCK_CTRL,
				    tp->pci_clock_ctrl | newbits3, 40);
		}
	}

	if (!(device_should_wake) &&
	    !(tp->tg3_flags & TG3_FLAG_ENABLE_ASF))
		tg3_power_down_phy(tp, do_low_power);

	tg3_frob_aux_power(tp);

	/* Workaround for unstable PLL clock */
	if ((GET_CHIP_REV(tp->pci_chip_rev_id) == CHIPREV_5750_AX) ||
	    (GET_CHIP_REV(tp->pci_chip_rev_id) == CHIPREV_5750_BX)) {
		u32 val = tr32(0x7d00);

		val &= ~((1 << 16) | (1 << 4) | (1 << 2) | (1 << 1) | 1);
		tw32(0x7d00, val);
		if (!(tp->tg3_flags & TG3_FLAG_ENABLE_ASF)) {
			int err;

			err = tg3_nvram_lock(tp);
			tg3_halt_cpu(tp, RX_CPU_BASE);
			if (!err)
				tg3_nvram_unlock(tp);
		}
	}

	tg3_write_sig_post_reset(tp, RESET_KIND_SHUTDOWN);

	if (device_should_wake)
		pci_enable_wake(tp->pdev, state, true);

	/* Finally, set the new power state. */
	pci_set_power_state(tp->pdev, state);

	return 0;
}

static void tg3_aux_stat_to_speed_duplex(struct tg3 *tp, u32 val, u16 *speed, u8 *duplex)
{
	switch (val & MII_TG3_AUX_STAT_SPDMASK) {
	case MII_TG3_AUX_STAT_10HALF:
		*speed = SPEED_10;
		*duplex = DUPLEX_HALF;
		break;

	case MII_TG3_AUX_STAT_10FULL:
		*speed = SPEED_10;
		*duplex = DUPLEX_FULL;
		break;

	case MII_TG3_AUX_STAT_100HALF:
		*speed = SPEED_100;
		*duplex = DUPLEX_HALF;
		break;

	case MII_TG3_AUX_STAT_100FULL:
		*speed = SPEED_100;
		*duplex = DUPLEX_FULL;
		break;

	case MII_TG3_AUX_STAT_1000HALF:
		*speed = SPEED_1000;
		*duplex = DUPLEX_HALF;
		break;

	case MII_TG3_AUX_STAT_1000FULL:
		*speed = SPEED_1000;
		*duplex = DUPLEX_FULL;
		break;

	default:
		if (tp->tg3_flags3 & TG3_FLG3_PHY_IS_FET) {
			*speed = (val & MII_TG3_AUX_STAT_100) ? SPEED_100 :
				 SPEED_10;
			*duplex = (val & MII_TG3_AUX_STAT_FULL) ? DUPLEX_FULL :
				  DUPLEX_HALF;
			break;
		}
		*speed = SPEED_INVALID;
		*duplex = DUPLEX_INVALID;
		break;
	}
}

static void tg3_phy_copper_begin(struct tg3 *tp)
{
	u32 new_adv;
	int i;

	if (tp->link_config.phy_is_low_power) {
		/* Entering low power mode.  Disable gigabit and
		 * 100baseT advertisements.
		 */
		tg3_writephy(tp, MII_TG3_CTRL, 0);

		new_adv = (ADVERTISE_10HALF | ADVERTISE_10FULL |
			   ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP);
		if (tp->tg3_flags & TG3_FLAG_WOL_SPEED_100MB)
			new_adv |= (ADVERTISE_100HALF | ADVERTISE_100FULL);

		tg3_writephy(tp, MII_ADVERTISE, new_adv);
	} else if (tp->link_config.speed == SPEED_INVALID) {
		if (tp->tg3_flags & TG3_FLAG_10_100_ONLY)
			tp->link_config.advertising &=
				~(ADVERTISED_1000baseT_Half |
				  ADVERTISED_1000baseT_Full);

		new_adv = ADVERTISE_CSMA;
		if (tp->link_config.advertising & ADVERTISED_10baseT_Half)
			new_adv |= ADVERTISE_10HALF;
		if (tp->link_config.advertising & ADVERTISED_10baseT_Full)
			new_adv |= ADVERTISE_10FULL;
		if (tp->link_config.advertising & ADVERTISED_100baseT_Half)
			new_adv |= ADVERTISE_100HALF;
		if (tp->link_config.advertising & ADVERTISED_100baseT_Full)
			new_adv |= ADVERTISE_100FULL;

		new_adv |= tg3_advert_flowctrl_1000T(tp->link_config.flowctrl);

		tg3_writephy(tp, MII_ADVERTISE, new_adv);

		if (tp->link_config.advertising &
		    (ADVERTISED_1000baseT_Half | ADVERTISED_1000baseT_Full)) {
			new_adv = 0;
			if (tp->link_config.advertising & ADVERTISED_1000baseT_Half)
				new_adv |= MII_TG3_CTRL_ADV_1000_HALF;
			if (tp->link_config.advertising & ADVERTISED_1000baseT_Full)
				new_adv |= MII_TG3_CTRL_ADV_1000_FULL;
			if (!(tp->tg3_flags & TG3_FLAG_10_100_ONLY) &&
			    (tp->pci_chip_rev_id == CHIPREV_ID_5701_A0 ||
			     tp->pci_chip_rev_id == CHIPREV_ID_5701_B0))
				new_adv |= (MII_TG3_CTRL_AS_MASTER |
					    MII_TG3_CTRL_ENABLE_AS_MASTER);
			tg3_writephy(tp, MII_TG3_CTRL, new_adv);
		} else {
			tg3_writephy(tp, MII_TG3_CTRL, 0);
		}
	} else {
		new_adv = tg3_advert_flowctrl_1000T(tp->link_config.flowctrl);
		new_adv |= ADVERTISE_CSMA;

		/* Asking for a specific link mode. */
		if (tp->link_config.speed == SPEED_1000) {
			tg3_writephy(tp, MII_ADVERTISE, new_adv);

			if (tp->link_config.duplex == DUPLEX_FULL)
				new_adv = MII_TG3_CTRL_ADV_1000_FULL;
			else
				new_adv = MII_TG3_CTRL_ADV_1000_HALF;
			if (tp->pci_chip_rev_id == CHIPREV_ID_5701_A0 ||
			    tp->pci_chip_rev_id == CHIPREV_ID_5701_B0)
				new_adv |= (MII_TG3_CTRL_AS_MASTER |
					    MII_TG3_CTRL_ENABLE_AS_MASTER);
		} else {
			if (tp->link_config.speed == SPEED_100) {
				if (tp->link_config.duplex == DUPLEX_FULL)
					new_adv |= ADVERTISE_100FULL;
				else
					new_adv |= ADVERTISE_100HALF;
			} else {
				if (tp->link_config.duplex == DUPLEX_FULL)
					new_adv |= ADVERTISE_10FULL;
				else
					new_adv |= ADVERTISE_10HALF;
			}
			tg3_writephy(tp, MII_ADVERTISE, new_adv);

			new_adv = 0;
		}

		tg3_writephy(tp, MII_TG3_CTRL, new_adv);
	}

	if (tp->link_config.autoneg == AUTONEG_DISABLE &&
	    tp->link_config.speed != SPEED_INVALID) {
		u32 bmcr, orig_bmcr;

		tp->link_config.active_speed = tp->link_config.speed;
		tp->link_config.active_duplex = tp->link_config.duplex;

		bmcr = 0;
		switch (tp->link_config.speed) {
		default:
		case SPEED_10:
			break;

		case SPEED_100:
			bmcr |= BMCR_SPEED100;
			break;

		case SPEED_1000:
			bmcr |= TG3_BMCR_SPEED1000;
			break;
		}

		if (tp->link_config.duplex == DUPLEX_FULL)
			bmcr |= BMCR_FULLDPLX;

		if (!tg3_readphy(tp, MII_BMCR, &orig_bmcr) &&
		    (bmcr != orig_bmcr)) {
			tg3_writephy(tp, MII_BMCR, BMCR_LOOPBACK);
			for (i = 0; i < 1500; i++) {
				u32 tmp;

				udelay(10);
				if (tg3_readphy(tp, MII_BMSR, &tmp) ||
				    tg3_readphy(tp, MII_BMSR, &tmp))
					continue;
				if (!(tmp & BMSR_LSTATUS)) {
					udelay(40);
					break;
				}
			}
			tg3_writephy(tp, MII_BMCR, bmcr);
			udelay(40);
		}
	} else {
		tg3_writephy(tp, MII_BMCR,
			     BMCR_ANENABLE | BMCR_ANRESTART);
	}
}

static int tg3_init_5401phy_dsp(struct tg3 *tp)
{
	int err;

	/* Turn off tap power management. */
	/* Set Extended packet length bit */
	err  = tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x4c20);

	err |= tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x0012);
	err |= tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x1804);

	err |= tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x0013);
	err |= tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x1204);

	err |= tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x8006);
	err |= tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x0132);

	err |= tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x8006);
	err |= tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x0232);

	err |= tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x201f);
	err |= tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x0a20);

	udelay(40);

	return err;
}

static int tg3_copper_is_advertising_all(struct tg3 *tp, u32 mask)
{
	u32 adv_reg, all_mask = 0;

	if (mask & ADVERTISED_10baseT_Half)
		all_mask |= ADVERTISE_10HALF;
	if (mask & ADVERTISED_10baseT_Full)
		all_mask |= ADVERTISE_10FULL;
	if (mask & ADVERTISED_100baseT_Half)
		all_mask |= ADVERTISE_100HALF;
	if (mask & ADVERTISED_100baseT_Full)
		all_mask |= ADVERTISE_100FULL;

	if (tg3_readphy(tp, MII_ADVERTISE, &adv_reg))
		return 0;

	if ((adv_reg & all_mask) != all_mask)
		return 0;
	if (!(tp->tg3_flags & TG3_FLAG_10_100_ONLY)) {
		u32 tg3_ctrl;

		all_mask = 0;
		if (mask & ADVERTISED_1000baseT_Half)
			all_mask |= ADVERTISE_1000HALF;
		if (mask & ADVERTISED_1000baseT_Full)
			all_mask |= ADVERTISE_1000FULL;

		if (tg3_readphy(tp, MII_TG3_CTRL, &tg3_ctrl))
			return 0;

		if ((tg3_ctrl & all_mask) != all_mask)
			return 0;
	}
	return 1;
}

static int tg3_adv_1000T_flowctrl_ok(struct tg3 *tp, u32 *lcladv, u32 *rmtadv)
{
	u32 curadv, reqadv;

	if (tg3_readphy(tp, MII_ADVERTISE, lcladv))
		return 1;

	curadv = *lcladv & (ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);
	reqadv = tg3_advert_flowctrl_1000T(tp->link_config.flowctrl);

	if (tp->link_config.active_duplex == DUPLEX_FULL) {
		if (curadv != reqadv)
			return 0;

		if (tp->tg3_flags & TG3_FLAG_PAUSE_AUTONEG)
			tg3_readphy(tp, MII_LPA, rmtadv);
	} else {
		/* Reprogram the advertisement register, even if it
		 * does not affect the current link.  If the link
		 * gets renegotiated in the future, we can save an
		 * additional renegotiation cycle by advertising
		 * it correctly in the first place.
		 */
		if (curadv != reqadv) {
			*lcladv &= ~(ADVERTISE_PAUSE_CAP |
				     ADVERTISE_PAUSE_ASYM);
			tg3_writephy(tp, MII_ADVERTISE, *lcladv | reqadv);
		}
	}

	return 1;
}

static int tg3_setup_copper_phy(struct tg3 *tp, int force_reset)
{
	int current_link_up;
	u32 bmsr, dummy;
	u32 lcl_adv, rmt_adv;
	u16 current_speed;
	u8 current_duplex;
	int i, err;

	tw32(MAC_EVENT, 0);

	tw32_f(MAC_STATUS,
	     (MAC_STATUS_SYNC_CHANGED |
	      MAC_STATUS_CFG_CHANGED |
	      MAC_STATUS_MI_COMPLETION |
	      MAC_STATUS_LNKSTATE_CHANGED));
	udelay(40);

	if ((tp->mi_mode & MAC_MI_MODE_AUTO_POLL) != 0) {
		tw32_f(MAC_MI_MODE,
		     (tp->mi_mode & ~MAC_MI_MODE_AUTO_POLL));
		udelay(80);
	}

	tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x02);

	/* Some third-party PHYs need to be reset on link going
	 * down.
	 */
	if ((GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5703 ||
	     GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704 ||
	     GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5705) &&
	    netif_carrier_ok(tp->dev)) {
		tg3_readphy(tp, MII_BMSR, &bmsr);
		if (!tg3_readphy(tp, MII_BMSR, &bmsr) &&
		    !(bmsr & BMSR_LSTATUS))
			force_reset = 1;
	}
	if (force_reset)
		tg3_phy_reset(tp);

	if ((tp->phy_id & PHY_ID_MASK) == PHY_ID_BCM5401) {
		tg3_readphy(tp, MII_BMSR, &bmsr);
		if (tg3_readphy(tp, MII_BMSR, &bmsr) ||
		    !(tp->tg3_flags & TG3_FLAG_INIT_COMPLETE))
			bmsr = 0;

		if (!(bmsr & BMSR_LSTATUS)) {
			err = tg3_init_5401phy_dsp(tp);
			if (err)
				return err;

			tg3_readphy(tp, MII_BMSR, &bmsr);
			for (i = 0; i < 1000; i++) {
				udelay(10);
				if (!tg3_readphy(tp, MII_BMSR, &bmsr) &&
				    (bmsr & BMSR_LSTATUS)) {
					udelay(40);
					break;
				}
			}

			if ((tp->phy_id & PHY_ID_REV_MASK) == PHY_REV_BCM5401_B0 &&
			    !(bmsr & BMSR_LSTATUS) &&
			    tp->link_config.active_speed == SPEED_1000) {
				err = tg3_phy_reset(tp);
				if (!err)
					err = tg3_init_5401phy_dsp(tp);
				if (err)
					return err;
			}
		}
	} else if (tp->pci_chip_rev_id == CHIPREV_ID_5701_A0 ||
		   tp->pci_chip_rev_id == CHIPREV_ID_5701_B0) {
		/* 5701 {A0,B0} CRC bug workaround */
		tg3_writephy(tp, 0x15, 0x0a75);
		tg3_writephy(tp, 0x1c, 0x8c68);
		tg3_writephy(tp, 0x1c, 0x8d68);
		tg3_writephy(tp, 0x1c, 0x8c68);
	}

	/* Clear pending interrupts... */
	tg3_readphy(tp, MII_TG3_ISTAT, &dummy);
	tg3_readphy(tp, MII_TG3_ISTAT, &dummy);

	if (tp->tg3_flags & TG3_FLAG_USE_MI_INTERRUPT)
		tg3_writephy(tp, MII_TG3_IMASK, ~MII_TG3_INT_LINKCHG);
	else if (!(tp->tg3_flags3 & TG3_FLG3_PHY_IS_FET))
		tg3_writephy(tp, MII_TG3_IMASK, ~0);

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 ||
	    GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5701) {
		if (tp->led_ctrl == LED_CTRL_MODE_PHY_1)
			tg3_writephy(tp, MII_TG3_EXT_CTRL,
				     MII_TG3_EXT_CTRL_LNK3_LED_MODE);
		else
			tg3_writephy(tp, MII_TG3_EXT_CTRL, 0);
	}

	current_link_up = 0;
	current_speed = SPEED_INVALID;
	current_duplex = DUPLEX_INVALID;

	if (tp->tg3_flags2 & TG3_FLG2_CAPACITIVE_COUPLING) {
		u32 val;

		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x4007);
		tg3_readphy(tp, MII_TG3_AUX_CTRL, &val);
		if (!(val & (1 << 10))) {
			val |= (1 << 10);
			tg3_writephy(tp, MII_TG3_AUX_CTRL, val);
			goto relink;
		}
	}

	bmsr = 0;
	for (i = 0; i < 100; i++) {
		tg3_readphy(tp, MII_BMSR, &bmsr);
		if (!tg3_readphy(tp, MII_BMSR, &bmsr) &&
		    (bmsr & BMSR_LSTATUS))
			break;
		udelay(40);
	}

	if (bmsr & BMSR_LSTATUS) {
		u32 aux_stat, bmcr;

		tg3_readphy(tp, MII_TG3_AUX_STAT, &aux_stat);
		for (i = 0; i < 2000; i++) {
			udelay(10);
			if (!tg3_readphy(tp, MII_TG3_AUX_STAT, &aux_stat) &&
			    aux_stat)
				break;
		}

		tg3_aux_stat_to_speed_duplex(tp, aux_stat,
					     &current_speed,
					     &current_duplex);

		bmcr = 0;
		for (i = 0; i < 200; i++) {
			tg3_readphy(tp, MII_BMCR, &bmcr);
			if (tg3_readphy(tp, MII_BMCR, &bmcr))
				continue;
			if (bmcr && bmcr != 0x7fff)
				break;
			udelay(10);
		}

		lcl_adv = 0;
		rmt_adv = 0;

		tp->link_config.active_speed = current_speed;
		tp->link_config.active_duplex = current_duplex;

		if (tp->link_config.autoneg == AUTONEG_ENABLE) {
			if ((bmcr & BMCR_ANENABLE) &&
			    tg3_copper_is_advertising_all(tp,
						tp->link_config.advertising)) {
				if (tg3_adv_1000T_flowctrl_ok(tp, &lcl_adv,
								  &rmt_adv))
					current_link_up = 1;
			}
		} else {
			if (!(bmcr & BMCR_ANENABLE) &&
			    tp->link_config.speed == current_speed &&
			    tp->link_config.duplex == current_duplex &&
			    tp->link_config.flowctrl ==
			    tp->link_config.active_flowctrl) {
				current_link_up = 1;
			}
		}

		if (current_link_up == 1 &&
		    tp->link_config.active_duplex == DUPLEX_FULL)
			tg3_setup_flow_control(tp, lcl_adv, rmt_adv);
	}

relink:
	if (current_link_up == 0 || tp->link_config.phy_is_low_power) {
		u32 tmp;

		tg3_phy_copper_begin(tp);

		tg3_readphy(tp, MII_BMSR, &tmp);
		if (!tg3_readphy(tp, MII_BMSR, &tmp) &&
		    (tmp & BMSR_LSTATUS))
			current_link_up = 1;
	}

	tp->mac_mode &= ~MAC_MODE_PORT_MODE_MASK;
	if (current_link_up == 1) {
		if (tp->link_config.active_speed == SPEED_100 ||
		    tp->link_config.active_speed == SPEED_10)
			tp->mac_mode |= MAC_MODE_PORT_MODE_MII;
		else
			tp->mac_mode |= MAC_MODE_PORT_MODE_GMII;
	} else if (tp->tg3_flags3 & TG3_FLG3_PHY_IS_FET)
		tp->mac_mode |= MAC_MODE_PORT_MODE_MII;
	else
		tp->mac_mode |= MAC_MODE_PORT_MODE_GMII;

	tp->mac_mode &= ~MAC_MODE_HALF_DUPLEX;
	if (tp->link_config.active_duplex == DUPLEX_HALF)
		tp->mac_mode |= MAC_MODE_HALF_DUPLEX;

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700) {
		if (current_link_up == 1 &&
		    tg3_5700_link_polarity(tp, tp->link_config.active_speed))
			tp->mac_mode |= MAC_MODE_LINK_POLARITY;
		else
			tp->mac_mode &= ~MAC_MODE_LINK_POLARITY;
	}

	/* ??? Without this setting Netgear GA302T PHY does not
	 * ??? send/receive packets...
	 */
	if ((tp->phy_id & PHY_ID_MASK) == PHY_ID_BCM5411 &&
	    tp->pci_chip_rev_id == CHIPREV_ID_5700_ALTIMA) {
		tp->mi_mode |= MAC_MI_MODE_AUTO_POLL;
		tw32_f(MAC_MI_MODE, tp->mi_mode);
		udelay(80);
	}

	tw32_f(MAC_MODE, tp->mac_mode);
	udelay(40);

	if (tp->tg3_flags & TG3_FLAG_USE_LINKCHG_REG) {
		/* Polled via timer. */
		tw32_f(MAC_EVENT, 0);
	} else {
		tw32_f(MAC_EVENT, MAC_EVENT_LNKSTATE_CHANGED);
	}
	udelay(40);

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 &&
	    current_link_up == 1 &&
	    tp->link_config.active_speed == SPEED_1000 &&
	    ((tp->tg3_flags & TG3_FLAG_PCIX_MODE) ||
	     (tp->tg3_flags & TG3_FLAG_PCI_HIGH_SPEED))) {
		udelay(120);
		tw32_f(MAC_STATUS,
		     (MAC_STATUS_SYNC_CHANGED |
		      MAC_STATUS_CFG_CHANGED));
		udelay(40);
		tg3_write_mem(tp,
			      NIC_SRAM_FIRMWARE_MBOX,
			      NIC_SRAM_FIRMWARE_MBOX_MAGIC2);
	}

	/* Prevent send BD corruption. */
	if (tp->tg3_flags3 & TG3_FLG3_CLKREQ_BUG) {
		u16 oldlnkctl, newlnkctl;

		pci_read_config_word(tp->pdev,
				     tp->pcie_cap + PCI_EXP_LNKCTL,
				     &oldlnkctl);
		if (tp->link_config.active_speed == SPEED_100 ||
		    tp->link_config.active_speed == SPEED_10)
			newlnkctl = oldlnkctl & ~PCI_EXP_LNKCTL_CLKREQ_EN;
		else
			newlnkctl = oldlnkctl | PCI_EXP_LNKCTL_CLKREQ_EN;
		if (newlnkctl != oldlnkctl)
			pci_write_config_word(tp->pdev,
					      tp->pcie_cap + PCI_EXP_LNKCTL,
					      newlnkctl);
	} else if (tp->tg3_flags3 & TG3_FLG3_TOGGLE_10_100_L1PLLPD) {
		u32 newreg, oldreg = tr32(TG3_PCIE_LNKCTL);
		if (tp->link_config.active_speed == SPEED_100 ||
		    tp->link_config.active_speed == SPEED_10)
			newreg = oldreg & ~TG3_PCIE_LNKCTL_L1_PLL_PD_EN;
		else
			newreg = oldreg | TG3_PCIE_LNKCTL_L1_PLL_PD_EN;
		if (newreg != oldreg)
			tw32(TG3_PCIE_LNKCTL, newreg);
	}

	if (current_link_up != netif_carrier_ok(tp->dev)) {
		if (current_link_up)
			netif_carrier_on(tp->dev);
		else
			netif_carrier_off(tp->dev);
		tg3_link_report(tp);
	}

	return 0;
}

struct tg3_fiber_aneginfo {
	int state;
#define ANEG_STATE_UNKNOWN		0
#define ANEG_STATE_AN_ENABLE		1
#define ANEG_STATE_RESTART_INIT		2
#define ANEG_STATE_RESTART		3
#define ANEG_STATE_DISABLE_LINK_OK	4
#define ANEG_STATE_ABILITY_DETECT_INIT	5
#define ANEG_STATE_ABILITY_DETECT	6
#define ANEG_STATE_ACK_DETECT_INIT	7
#define ANEG_STATE_ACK_DETECT		8
#define ANEG_STATE_COMPLETE_ACK_INIT	9
#define ANEG_STATE_COMPLETE_ACK		10
#define ANEG_STATE_IDLE_DETECT_INIT	11
#define ANEG_STATE_IDLE_DETECT		12
#define ANEG_STATE_LINK_OK		13
#define ANEG_STATE_NEXT_PAGE_WAIT_INIT	14
#define ANEG_STATE_NEXT_PAGE_WAIT	15

	u32 flags;
#define MR_AN_ENABLE		0x00000001
#define MR_RESTART_AN		0x00000002
#define MR_AN_COMPLETE		0x00000004
#define MR_PAGE_RX		0x00000008
#define MR_NP_LOADED		0x00000010
#define MR_TOGGLE_TX		0x00000020
#define MR_LP_ADV_FULL_DUPLEX	0x00000040
#define MR_LP_ADV_HALF_DUPLEX	0x00000080
#define MR_LP_ADV_SYM_PAUSE	0x00000100
#define MR_LP_ADV_ASYM_PAUSE	0x00000200
#define MR_LP_ADV_REMOTE_FAULT1	0x00000400
#define MR_LP_ADV_REMOTE_FAULT2	0x00000800
#define MR_LP_ADV_NEXT_PAGE	0x00001000
#define MR_TOGGLE_RX		0x00002000
#define MR_NP_RX		0x00004000

#define MR_LINK_OK		0x80000000

	unsigned long link_time, cur_time;

	u32 ability_match_cfg;
	int ability_match_count;

	char ability_match, idle_match, ack_match;

	u32 txconfig, rxconfig;
#define ANEG_CFG_NP		0x00000080
#define ANEG_CFG_ACK		0x00000040
#define ANEG_CFG_RF2		0x00000020
#define ANEG_CFG_RF1		0x00000010
#define ANEG_CFG_PS2		0x00000001
#define ANEG_CFG_PS1		0x00008000
#define ANEG_CFG_HD		0x00004000
#define ANEG_CFG_FD		0x00002000
#define ANEG_CFG_INVAL		0x00001f06

};
#define ANEG_OK		0
#define ANEG_DONE	1
#define ANEG_TIMER_ENAB	2
#define ANEG_FAILED	-1

#define ANEG_STATE_SETTLE_TIME	10000

static int tg3_fiber_aneg_smachine(struct tg3 *tp,
				   struct tg3_fiber_aneginfo *ap)
{
	u16 flowctrl;
	unsigned long delta;
	u32 rx_cfg_reg;
	int ret;

	if (ap->state == ANEG_STATE_UNKNOWN) {
		ap->rxconfig = 0;
		ap->link_time = 0;
		ap->cur_time = 0;
		ap->ability_match_cfg = 0;
		ap->ability_match_count = 0;
		ap->ability_match = 0;
		ap->idle_match = 0;
		ap->ack_match = 0;
	}
	ap->cur_time++;

	if (tr32(MAC_STATUS) & MAC_STATUS_RCVD_CFG) {
		rx_cfg_reg = tr32(MAC_RX_AUTO_NEG);

		if (rx_cfg_reg != ap->ability_match_cfg) {
			ap->ability_match_cfg = rx_cfg_reg;
			ap->ability_match = 0;
			ap->ability_match_count = 0;
		} else {
			if (++ap->ability_match_count > 1) {
				ap->ability_match = 1;
				ap->ability_match_cfg = rx_cfg_reg;
			}
		}
		if (rx_cfg_reg & ANEG_CFG_ACK)
			ap->ack_match = 1;
		else
			ap->ack_match = 0;

		ap->idle_match = 0;
	} else {
		ap->idle_match = 1;
		ap->ability_match_cfg = 0;
		ap->ability_match_count = 0;
		ap->ability_match = 0;
		ap->ack_match = 0;

		rx_cfg_reg = 0;
	}

	ap->rxconfig = rx_cfg_reg;
	ret = ANEG_OK;

	switch(ap->state) {
	case ANEG_STATE_UNKNOWN:
		if (ap->flags & (MR_AN_ENABLE | MR_RESTART_AN))
			ap->state = ANEG_STATE_AN_ENABLE;

		/* fallthru */
	case ANEG_STATE_AN_ENABLE:
		ap->flags &= ~(MR_AN_COMPLETE | MR_PAGE_RX);
		if (ap->flags & MR_AN_ENABLE) {
			ap->link_time = 0;
			ap->cur_time = 0;
			ap->ability_match_cfg = 0;
			ap->ability_match_count = 0;
			ap->ability_match = 0;
			ap->idle_match = 0;
			ap->ack_match = 0;

			ap->state = ANEG_STATE_RESTART_INIT;
		} else {
			ap->state = ANEG_STATE_DISABLE_LINK_OK;
		}
		break;

	case ANEG_STATE_RESTART_INIT:
		ap->link_time = ap->cur_time;
		ap->flags &= ~(MR_NP_LOADED);
		ap->txconfig = 0;
		tw32(MAC_TX_AUTO_NEG, 0);
		tp->mac_mode |= MAC_MODE_SEND_CONFIGS;
		tw32_f(MAC_MODE, tp->mac_mode);
		udelay(40);

		ret = ANEG_TIMER_ENAB;
		ap->state = ANEG_STATE_RESTART;

		/* fallthru */
	case ANEG_STATE_RESTART:
		delta = ap->cur_time - ap->link_time;
		if (delta > ANEG_STATE_SETTLE_TIME) {
			ap->state = ANEG_STATE_ABILITY_DETECT_INIT;
		} else {
			ret = ANEG_TIMER_ENAB;
		}
		break;

	case ANEG_STATE_DISABLE_LINK_OK:
		ret = ANEG_DONE;
		break;

	case ANEG_STATE_ABILITY_DETECT_INIT:
		ap->flags &= ~(MR_TOGGLE_TX);
		ap->txconfig = ANEG_CFG_FD;
		flowctrl = tg3_advert_flowctrl_1000X(tp->link_config.flowctrl);
		if (flowctrl & ADVERTISE_1000XPAUSE)
			ap->txconfig |= ANEG_CFG_PS1;
		if (flowctrl & ADVERTISE_1000XPSE_ASYM)
			ap->txconfig |= ANEG_CFG_PS2;
		tw32(MAC_TX_AUTO_NEG, ap->txconfig);
		tp->mac_mode |= MAC_MODE_SEND_CONFIGS;
		tw32_f(MAC_MODE, tp->mac_mode);
		udelay(40);

		ap->state = ANEG_STATE_ABILITY_DETECT;
		break;

	case ANEG_STATE_ABILITY_DETECT:
		if (ap->ability_match != 0 && ap->rxconfig != 0) {
			ap->state = ANEG_STATE_ACK_DETECT_INIT;
		}
		break;

	case ANEG_STATE_ACK_DETECT_INIT:
		ap->txconfig |= ANEG_CFG_ACK;
		tw32(MAC_TX_AUTO_NEG, ap->txconfig);
		tp->mac_mode |= MAC_MODE_SEND_CONFIGS;
		tw32_f(MAC_MODE, tp->mac_mode);
		udelay(40);

		ap->state = ANEG_STATE_ACK_DETECT;

		/* fallthru */
	case ANEG_STATE_ACK_DETECT:
		if (ap->ack_match != 0) {
			if ((ap->rxconfig & ~ANEG_CFG_ACK) ==
			    (ap->ability_match_cfg & ~ANEG_CFG_ACK)) {
				ap->state = ANEG_STATE_COMPLETE_ACK_INIT;
			} else {
				ap->state = ANEG_STATE_AN_ENABLE;
			}
		} else if (ap->ability_match != 0 &&
			   ap->rxconfig == 0) {
			ap->state = ANEG_STATE_AN_ENABLE;
		}
		break;

	case ANEG_STATE_COMPLETE_ACK_INIT:
		if (ap->rxconfig & ANEG_CFG_INVAL) {
			ret = ANEG_FAILED;
			break;
		}
		ap->flags &= ~(MR_LP_ADV_FULL_DUPLEX |
			       MR_LP_ADV_HALF_DUPLEX |
			       MR_LP_ADV_SYM_PAUSE |
			       MR_LP_ADV_ASYM_PAUSE |
			       MR_LP_ADV_REMOTE_FAULT1 |
			       MR_LP_ADV_REMOTE_FAULT2 |
			       MR_LP_ADV_NEXT_PAGE |
			       MR_TOGGLE_RX |
			       MR_NP_RX);
		if (ap->rxconfig & ANEG_CFG_FD)
			ap->flags |= MR_LP_ADV_FULL_DUPLEX;
		if (ap->rxconfig & ANEG_CFG_HD)
			ap->flags |= MR_LP_ADV_HALF_DUPLEX;
		if (ap->rxconfig & ANEG_CFG_PS1)
			ap->flags |= MR_LP_ADV_SYM_PAUSE;
		if (ap->rxconfig & ANEG_CFG_PS2)
			ap->flags |= MR_LP_ADV_ASYM_PAUSE;
		if (ap->rxconfig & ANEG_CFG_RF1)
			ap->flags |= MR_LP_ADV_REMOTE_FAULT1;
		if (ap->rxconfig & ANEG_CFG_RF2)
			ap->flags |= MR_LP_ADV_REMOTE_FAULT2;
		if (ap->rxconfig & ANEG_CFG_NP)
			ap->flags |= MR_LP_ADV_NEXT_PAGE;

		ap->link_time = ap->cur_time;

		ap->flags ^= (MR_TOGGLE_TX);
		if (ap->rxconfig & 0x0008)
			ap->flags |= MR_TOGGLE_RX;
		if (ap->rxconfig & ANEG_CFG_NP)
			ap->flags |= MR_NP_RX;
		ap->flags |= MR_PAGE_RX;

		ap->state = ANEG_STATE_COMPLETE_ACK;
		ret = ANEG_TIMER_ENAB;
		break;

	case ANEG_STATE_COMPLETE_ACK:
		if (ap->ability_match != 0 &&
		    ap->rxconfig == 0) {
			ap->state = ANEG_STATE_AN_ENABLE;
			break;
		}
		delta = ap->cur_time - ap->link_time;
		if (delta > ANEG_STATE_SETTLE_TIME) {
			if (!(ap->flags & (MR_LP_ADV_NEXT_PAGE))) {
				ap->state = ANEG_STATE_IDLE_DETECT_INIT;
			} else {
				if ((ap->txconfig & ANEG_CFG_NP) == 0 &&
				    !(ap->flags & MR_NP_RX)) {
					ap->state = ANEG_STATE_IDLE_DETECT_INIT;
				} else {
					ret = ANEG_FAILED;
				}
			}
		}
		break;

	case ANEG_STATE_IDLE_DETECT_INIT:
		ap->link_time = ap->cur_time;
		tp->mac_mode &= ~MAC_MODE_SEND_CONFIGS;
		tw32_f(MAC_MODE, tp->mac_mode);
		udelay(40);

		ap->state = ANEG_STATE_IDLE_DETECT;
		ret = ANEG_TIMER_ENAB;
		break;

	case ANEG_STATE_IDLE_DETECT:
		if (ap->ability_match != 0 &&
		    ap->rxconfig == 0) {
			ap->state = ANEG_STATE_AN_ENABLE;
			break;
		}
		delta = ap->cur_time - ap->link_time;
		if (delta > ANEG_STATE_SETTLE_TIME) {
			/* XXX another gem from the Broadcom driver :( */
			ap->state = ANEG_STATE_LINK_OK;
		}
		break;

	case ANEG_STATE_LINK_OK:
		ap->flags |= (MR_AN_COMPLETE | MR_LINK_OK);
		ret = ANEG_DONE;
		break;

	case ANEG_STATE_NEXT_PAGE_WAIT_INIT:
		/* ??? unimplemented */
		break;

	case ANEG_STATE_NEXT_PAGE_WAIT:
		/* ??? unimplemented */
		break;

	default:
		ret = ANEG_FAILED;
		break;
	}

	return ret;
}

static int fiber_autoneg(struct tg3 *tp, u32 *txflags, u32 *rxflags)
{
	int res = 0;
	struct tg3_fiber_aneginfo aninfo;
	int status = ANEG_FAILED;
	unsigned int tick;
	u32 tmp;

	tw32_f(MAC_TX_AUTO_NEG, 0);

	tmp = tp->mac_mode & ~MAC_MODE_PORT_MODE_MASK;
	tw32_f(MAC_MODE, tmp | MAC_MODE_PORT_MODE_GMII);
	udelay(40);

	tw32_f(MAC_MODE, tp->mac_mode | MAC_MODE_SEND_CONFIGS);
	udelay(40);

	memset(&aninfo, 0, sizeof(aninfo));
	aninfo.flags |= MR_AN_ENABLE;
	aninfo.state = ANEG_STATE_UNKNOWN;
	aninfo.cur_time = 0;
	tick = 0;
	while (++tick < 195000) {
		status = tg3_fiber_aneg_smachine(tp, &aninfo);
		if (status == ANEG_DONE || status == ANEG_FAILED)
			break;

		udelay(1);
	}

	tp->mac_mode &= ~MAC_MODE_SEND_CONFIGS;
	tw32_f(MAC_MODE, tp->mac_mode);
	udelay(40);

	*txflags = aninfo.txconfig;
	*rxflags = aninfo.flags;

	if (status == ANEG_DONE &&
	    (aninfo.flags & (MR_AN_COMPLETE | MR_LINK_OK |
			     MR_LP_ADV_FULL_DUPLEX)))
		res = 1;

	return res;
}

static void tg3_init_bcm8002(struct tg3 *tp)
{
	u32 mac_status = tr32(MAC_STATUS);
	int i;

	/* Reset when initting first time or we have a link. */
	if ((tp->tg3_flags & TG3_FLAG_INIT_COMPLETE) &&
	    !(mac_status & MAC_STATUS_PCS_SYNCED))
		return;

	/* Set PLL lock range. */
	tg3_writephy(tp, 0x16, 0x8007);

	/* SW reset */
	tg3_writephy(tp, MII_BMCR, BMCR_RESET);

	/* Wait for reset to complete. */
	/* XXX schedule_timeout() ... */
	for (i = 0; i < 500; i++)
		udelay(10);

	/* Config mode; select PMA/Ch 1 regs. */
	tg3_writephy(tp, 0x10, 0x8411);

	/* Enable auto-lock and comdet, select txclk for tx. */
	tg3_writephy(tp, 0x11, 0x0a10);

	tg3_writephy(tp, 0x18, 0x00a0);
	tg3_writephy(tp, 0x16, 0x41ff);

	/* Assert and deassert POR. */
	tg3_writephy(tp, 0x13, 0x0400);
	udelay(40);
	tg3_writephy(tp, 0x13, 0x0000);

	tg3_writephy(tp, 0x11, 0x0a50);
	udelay(40);
	tg3_writephy(tp, 0x11, 0x0a10);

	/* Wait for signal to stabilize */
	/* XXX schedule_timeout() ... */
	for (i = 0; i < 15000; i++)
		udelay(10);

	/* Deselect the channel register so we can read the PHYID
	 * later.
	 */
	tg3_writephy(tp, 0x10, 0x8011);
}

static int tg3_setup_fiber_hw_autoneg(struct tg3 *tp, u32 mac_status)
{
	u16 flowctrl;
	u32 sg_dig_ctrl, sg_dig_status;
	u32 serdes_cfg, expected_sg_dig_ctrl;
	int workaround, port_a;
	int current_link_up;

	serdes_cfg = 0;
	expected_sg_dig_ctrl = 0;
	workaround = 0;
	port_a = 1;
	current_link_up = 0;

	if (tp->pci_chip_rev_id != CHIPREV_ID_5704_A0 &&
	    tp->pci_chip_rev_id != CHIPREV_ID_5704_A1) {
		workaround = 1;
		if (tr32(TG3PCI_DUAL_MAC_CTRL) & DUAL_MAC_CTRL_ID)
			port_a = 0;

		/* preserve bits 0-11,13,14 for signal pre-emphasis */
		/* preserve bits 20-23 for voltage regulator */
		serdes_cfg = tr32(MAC_SERDES_CFG) & 0x00f06fff;
	}

	sg_dig_ctrl = tr32(SG_DIG_CTRL);

	if (tp->link_config.autoneg != AUTONEG_ENABLE) {
		if (sg_dig_ctrl & SG_DIG_USING_HW_AUTONEG) {
			if (workaround) {
				u32 val = serdes_cfg;

				if (port_a)
					val |= 0xc010000;
				else
					val |= 0x4010000;
				tw32_f(MAC_SERDES_CFG, val);
			}

			tw32_f(SG_DIG_CTRL, SG_DIG_COMMON_SETUP);
		}
		if (mac_status & MAC_STATUS_PCS_SYNCED) {
			tg3_setup_flow_control(tp, 0, 0);
			current_link_up = 1;
		}
		goto out;
	}

	/* Want auto-negotiation.  */
	expected_sg_dig_ctrl = SG_DIG_USING_HW_AUTONEG | SG_DIG_COMMON_SETUP;

	flowctrl = tg3_advert_flowctrl_1000X(tp->link_config.flowctrl);
	if (flowctrl & ADVERTISE_1000XPAUSE)
		expected_sg_dig_ctrl |= SG_DIG_PAUSE_CAP;
	if (flowctrl & ADVERTISE_1000XPSE_ASYM)
		expected_sg_dig_ctrl |= SG_DIG_ASYM_PAUSE;

	if (sg_dig_ctrl != expected_sg_dig_ctrl) {
		if ((tp->tg3_flags2 & TG3_FLG2_PARALLEL_DETECT) &&
		    tp->serdes_counter &&
		    ((mac_status & (MAC_STATUS_PCS_SYNCED |
				    MAC_STATUS_RCVD_CFG)) ==
		     MAC_STATUS_PCS_SYNCED)) {
			tp->serdes_counter--;
			current_link_up = 1;
			goto out;
		}
restart_autoneg:
		if (workaround)
			tw32_f(MAC_SERDES_CFG, serdes_cfg | 0xc011000);
		tw32_f(SG_DIG_CTRL, expected_sg_dig_ctrl | SG_DIG_SOFT_RESET);
		udelay(5);
		tw32_f(SG_DIG_CTRL, expected_sg_dig_ctrl);

		tp->serdes_counter = SERDES_AN_TIMEOUT_5704S;
		tp->tg3_flags2 &= ~TG3_FLG2_PARALLEL_DETECT;
	} else if (mac_status & (MAC_STATUS_PCS_SYNCED |
				 MAC_STATUS_SIGNAL_DET)) {
		sg_dig_status = tr32(SG_DIG_STATUS);
		mac_status = tr32(MAC_STATUS);

		if ((sg_dig_status & SG_DIG_AUTONEG_COMPLETE) &&
		    (mac_status & MAC_STATUS_PCS_SYNCED)) {
			u32 local_adv = 0, remote_adv = 0;

			if (sg_dig_ctrl & SG_DIG_PAUSE_CAP)
				local_adv |= ADVERTISE_1000XPAUSE;
			if (sg_dig_ctrl & SG_DIG_ASYM_PAUSE)
				local_adv |= ADVERTISE_1000XPSE_ASYM;

			if (sg_dig_status & SG_DIG_PARTNER_PAUSE_CAPABLE)
				remote_adv |= LPA_1000XPAUSE;
			if (sg_dig_status & SG_DIG_PARTNER_ASYM_PAUSE)
				remote_adv |= LPA_1000XPAUSE_ASYM;

			tg3_setup_flow_control(tp, local_adv, remote_adv);
			current_link_up = 1;
			tp->serdes_counter = 0;
			tp->tg3_flags2 &= ~TG3_FLG2_PARALLEL_DETECT;
		} else if (!(sg_dig_status & SG_DIG_AUTONEG_COMPLETE)) {
			if (tp->serdes_counter)
				tp->serdes_counter--;
			else {
				if (workaround) {
					u32 val = serdes_cfg;

					if (port_a)
						val |= 0xc010000;
					else
						val |= 0x4010000;

					tw32_f(MAC_SERDES_CFG, val);
				}

				tw32_f(SG_DIG_CTRL, SG_DIG_COMMON_SETUP);
				udelay(40);

				/* Link parallel detection - link is up */
				/* only if we have PCS_SYNC and not */
				/* receiving config code words */
				mac_status = tr32(MAC_STATUS);
				if ((mac_status & MAC_STATUS_PCS_SYNCED) &&
				    !(mac_status & MAC_STATUS_RCVD_CFG)) {
					tg3_setup_flow_control(tp, 0, 0);
					current_link_up = 1;
					tp->tg3_flags2 |=
						TG3_FLG2_PARALLEL_DETECT;
					tp->serdes_counter =
						SERDES_PARALLEL_DET_TIMEOUT;
				} else
					goto restart_autoneg;
			}
		}
	} else {
		tp->serdes_counter = SERDES_AN_TIMEOUT_5704S;
		tp->tg3_flags2 &= ~TG3_FLG2_PARALLEL_DETECT;
	}

out:
	return current_link_up;
}

static int tg3_setup_fiber_by_hand(struct tg3 *tp, u32 mac_status)
{
	int current_link_up = 0;

	if (!(mac_status & MAC_STATUS_PCS_SYNCED))
		goto out;

	if (tp->link_config.autoneg == AUTONEG_ENABLE) {
		u32 txflags, rxflags;
		int i;

		if (fiber_autoneg(tp, &txflags, &rxflags)) {
			u32 local_adv = 0, remote_adv = 0;

			if (txflags & ANEG_CFG_PS1)
				local_adv |= ADVERTISE_1000XPAUSE;
			if (txflags & ANEG_CFG_PS2)
				local_adv |= ADVERTISE_1000XPSE_ASYM;

			if (rxflags & MR_LP_ADV_SYM_PAUSE)
				remote_adv |= LPA_1000XPAUSE;
			if (rxflags & MR_LP_ADV_ASYM_PAUSE)
				remote_adv |= LPA_1000Xcom T Broa;
gon3tg3_setup_flow_control(tp, localet d, ethernet d)(C) 20current_link_up = 1;
		}
		for (i = 0; i < 3eff ++) {gon3udelay(20)001,	tw32_f(MAC_STATUS,* Co icrosyt (C) 2004 _SYNC_CHANGED |gon3 ems Inc.
 *CFGyright ()
 * Coobox.co4)
 * Coif ((tr32t (C) 2004 ) &un Microstems Inc.
 * Copyright (C) 200icrosy-2009 Broadcom Corporati == 0igon3 break001, 2
		mac_status = 	Derived from pr001,s:
 
 * Copyright (C)= 0 &priery unsion is her&t (C) 2000-2PC * CopED)are
 *	dat!a in hexadecimal or equivRCVDdcom)igon3
 * Copyright (C) 2001} elserzik@01, 2002, 2003, 2004 David 0, )
 *
		/* Forcing *
 *FD righ up. */
ying it.
 */


#includeodulyright (C)MODE, (tp->sionmode |t (C)comp_SEND_CONFIGStion.
*
 * Firmwarnclude <linux/compilr.h>
#includ <linux/delay.h>
Permout:
	return 
 * Copyright (;
}

 is ic int 01, 2002, 2iber_phy(struc#incl *vid >
#iforce_reset)
{
	u32 orig_pause_cfg;
	u16includactive_speedskbu8.h>
#include duplexskbu32 sion is he;
	>
#irt.h>
#include <le <lii>
#include <linux/rebyp->rightconfig.nclude 2003ctrl;x/brcmpnclude <linuclude <linux/if_vlan.h>
#i<linux/eool.h>
#include <lclude <linux/if_vlan.h>
#iude <lin
istri!er.h>01, flags2 & TG3_FLG2_HW_AUTONEGat, pricronetif_carrier_oker.h>dev

#includeapping.h>
#incude <linuAG_INIT_COMPLETE)arzik@sion is hereby granted for the di in hexadeci=unpublished salent form *	Copyrightt (C) 2000-2SIGNAL_DET_SPARC
#include <asm/idpcom Corpora_SPARC
#include <asm/idp is accome distrision is hereaccess.h>

#ifdef CONFIG_SPARCcludde <asm/idprom.h>
#inasm/io.opyright (C) 2004 Sstems Inc.
 * Copyright (C) 2005clude-2009 Broadcom Corporation.
 ux/iopo0001, 20ermiude <linux/TXmware_NEGparam.h>r.h>
#include&= ~inux/comp_PORTux/slaMASKe <linux/slaHALF_DUPLEXinedr.h>
#include =<linux/sla02"
#definTBIber lude <linux/in.h>
#include <linuux/delay.h>
#is:
 de <phy_id21Q)PHY_ID_BCM8002igon01, init_bcmLE	 (tph>
#i/* Enableernel.change eventETIF_ when serdes polling. #inclfine TG3_DEEVENT,e "tgMSG_I_LNK 200E Corporate TG3_DEF_TX_MODE
 * Copyright (C) tg3"sion is hereby granted for the DE		0
#defg.h>
#include <linux/firmware.h>
clude <linux/types.h>nclude <linux/nehw_autonegavid sion is he| \
inuxdev->tx_timeout() should be called toby_handroblem
 */
#defineer 1, napi[0].hwn is he-> is here
		(SD Inc.
 *UPDATe BAR_00
#defframe's data payload */
#d& ~ TG3_MIN_MLINK_CHGtion
02, 2003 Jeff Gar10ik (jgarzik@ED 1
#else
#define TG3_VLAN_TAG_USED 0
#endif

#inm/prom.h>
#endif

#define e <linux/dela5ined(CONF*	Derived from prop TG3_VLAN_TAG_USED 0
#endif

#inclom.h>
#endif

#define BAR_0	 onboard memorNETIF_MSG_IFUP		| Corporatio.
 *
 *	ermision is hereby granted for the ds:
 * in hexadecimal or equivalent formatorporrzik@_ERR	| \
	 NETIF_MSG_TE		0
#deflinux/if_vlan the pDINGware.h>_ENABLEare
 *	datde <\
	 NE_counterDING	100
#dlude <linux/compiler.h>
#include 

#inclulinux/slab.h>
#include <linTG3_DEF_1
 * Copyright (C)x/in.h>
#include <linux#definestribution of this firm1 These nux/tcp.h>
#include <linux/ = SPEED *
 *_TBLinux/prefetch.h>
#include <li = "Septe_FULLo
 * werived LED_CTRLiler.h>led_de <irmware somfine TG3E		5fineOVERRIDEZE(tp)	\
	(((tp->tg3*
 *MBPS_ONtion.<linux/modulstructions.  Another solution would INVALIDo
 * replace things like '% foo' with '& (fefine TG3_TX
 */
#define TG3_RX_RCB_RING_SIZE(tp)	\
	(((tp->tg3_flags & TG3_FLAG_JUMBO_CAPABLE) &&TRAFFIC & TG3_FL>tg3_fd of with
 * hw multipl!=e <net/checksum.h>
#includ100
#dstribution of this igon3 <net/checksum.n>
#includ_TBLTG3_TX_		 TG3_RX_JUMBO_ffING_SIZE)
#de01, rightreport| \
	 _flags2 & TGx/minowphy.h>
#include <linux/if_vlan.h>
#include <lint_rx_brcmphy.h>
#inc_RING3_RX_RCB_RIN|#defi  .h>
#include <linu3_tx3_FLG2_5780_CLASS)) ? 1024 : * \
				 TG3_TX_RING_ foo' w#define NEXT_TX(N)		(((N) ude <ligon3(struct tg3_rx_buffer_d
inux/iopo0<linux/pci.h>
#include <linux/nemiietdevice.h>
#include <linux/etherdevice.h <linux/phy.h>
#inc, errF_MSG_Tx/mibmsr, bmcrskbuff.
 * Copy<linux/etht
 * Copyude <linux/miS. Miller (davem@redh single 2009"

#define TG3_DEF_MAC_MODEGMI
#define TG3_DEF_RX_MODE		0
#define TG3_DEF_TX_MODE(TG3_TX_RMSG_IFDup TX procght (C) 2004 Sunary unpublished source code,
 *	Casm/prom.h>
#endif

#define BAR_#include <asm/idpMIinclude ION/* number of ETHTOOL_		512
#define TG3_e TG3_DEF_TX_MODE		0
ux/etherdevice TG3_ine erdev| \
	 NE_ERR	| \
	 NETIF_MSG_TSZ(TG3_RX_STD 512)

#define TG3_Z)
#define TG3DEF_TX_RING_PENDING
	_STD|hould readtdevvid MII_BMSR, &G3_Rine Tine FIRMWARE_TG3TSO5	"tigon/tg3_tso5.bin"s:
 GET_ASIC_REV
#defici_chip_rev_idNDINGE_NAME "_5714g3_ext_rx_	Derived TXchange wherMODULE_AU_FLAG_UPX_RINGmsne FIn/tg_L 2004 
#define TG3em@reE_VEt.com) and Jeff6
#d

static char version[] __devCg3_tsoc.binIZE		25E 128

/* Do not place this n-ring entrieat,  !ux/etherdeve <net/ip.h>

#include lude <linux/fPARALLE>
#inECAN_TAG_U/* do nothing, just checknux/ernel.h> at the end#incl<linux/_SIZE 128

/* Do not place this n-ring entrie100
#dx/miler (newMA_TO_MAE_DESCRIPTION("Broadcom TigoADVERTISE, &edhat.G3_RTG3_D = == 3_fl(_param(tg *
 * oo - | _DESC(tg3_debugDATEirmware s_DESC(tg3_debugcom Ting message enable value"yright ing message enable SLCTh>
#int, 0);
M FIRMWAadvertinclude < *
 * ;
MODULE_LICENSE(nclude <h>
#in_SIZE 128

/* Do not DCOM,ise <l&age enabled be tbaseT_Half\
				 VENDOR_IDn3 bitmapped debugg_TBL_SIZE 128

/* Do not PCI_DEVICE_ID_TIGON3_5701)},
	{PCIFullICE(PCI_VENDOR_ID_BROADCOM, PCIoo - 1es, but CI_VENDO!ODULE) || !(rnet & n3 e_ANbug = -,
 * we g3_writeue */
module_param(tg3_CI_VEND
 * CoDEVICdhat._TIGON3_570 |N3_5702FRESTART * CopI_DEVICE(PCI_VENDOR_n3 ethrnet drivwe really wanMSG_IFDOWN	| \
	 NETIF_MSG_IFUP		| \
TG3_FLhe tp struct its SERDES_AN_TIMEOUT_MODUJeff G3_FLg.h>
#includ= ~TG3);
MODULE_FIRMWARE(FI(C) 20ux/iopoer_TO_nsteaesc) * \
				 Tew_DMA_TO
E_ID_TIik@pobCR_wouldbe to
 *_DEVICE_ =D_TIG_PARM3_5702FE)},
	{PCI_DEVoo -DPLembePCI_VENDOR_ID_BROADCOM,SO		"tigith '& (foo -ICE(PCI_VID_TIGON3_570IGON3_572X)},
	{PE(PCI_VEN!DOR_IDarzik@p/*N3_570,
	{PCI_D is a erderved biO5);at needs	Copy* to be ON);on EVICE.CI_VENncluCE(PCI_VENGON3_570,
	{PCI_DEVDCOM, Pclude aerneldownICE_ID_VICE(P_BYTES	(sizeof(struct tg3_ext/* -1 == ICE(PCSG_ENABLE as value */
module_param(tg3_debug, in		ULE_P_VER_DESC(tg3_debug, "Tige TG3_Rn3 bitmapped debugging mesEVICE_ID_TIG_DEVICE{PCICI_DEVICE(PCI_VENDOR_ID_BROADCOM},
	{PCI_D_ID_BROADCOM, PCI_DEVICE_ID_TIG, PCI_DE
				I_DEVICE(PCI_VM, PCI_DEVICE_ID_TIGbug = -OR_ID_l.  oper)
 * Co3_RX_RCB_RING_BYTES(tp) (sizeof, 200PCI_DEVICE(PCI_VENDOR_ID_BRO_DEVICE_EVICE_ID_TIRINGEVICE_IDOR_ID_BROADCOM, PCI_DEVICE_ID_n/tg3_tso5.bin"PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_RV_MODULE_NAME ".c:v" DRV_MODULE_VERSION	Copyrig" (" DRV_MODULE_RELELDATE ")\n";

MODULE_AUTHOR("David S. Miller (davavem@redhat.com) and Jeff define TG3 (jgarzik@pobox.com)");
MO_DEVICE(BROADCOM, PCI_DEVICE_ID_TIGON3_5705M)},
	{PCinstead of wijgarziat.com) and J100
#define TGolution would be to
 *
 * Copyright (C) 2001,{PCI_DVICE_ID_TIIGON3_570anying it.
  foo' with '& (foo - 1)'fine TG3E_ID_TIGON3_5750M)},
	{P_DEVIC
		S. MillerNDIR_TBLethernet dr_MSG_CI_VENDOR_ID_BROADCPCI_VENDOrzik@po32 commonICE(PCG_ENABLE as value */
module_param(tg3_dS. MillerEVICE_ID_TIGON3_5705F)},
	{PCI_DELPA, &davem@redhat._VEN_5751 =B_MAP_SZ		 &G3_RX_DMA_TO_D_BROADCI_DEVI& I_VENDOR_ID_BROAbugging messaCI_DEDESC(tg3_debug, "T{PCI_DEVIEVICE(PCI_VENGON3_5753)},
	{PCI_DEVICE_E_ID_TIGON3_5750M)},
	{PCI_DEVIC PCI_DEVICE_NDOR_ID_BROADCOM, PCI_DEVICE_I_DEVICE(CI_DEVICEefine TG3_RSS_INDIR_TBLstead of with
 * hw multiply/mo_MODE_ID_TIGON3_5750BROADCOM, PCI_DEV01, 2002, 2003, 2004 David S. Miller (davem@redhat.coine DRV_MODULE_VEODULE_RELDATE	"Septe
	DRV_M replace things like '% foo' wiM, PCI_DEVICE)

#d_SZ(TG3_RX_JMB_DMA_SZ)

ID_TIGON3_575er of free TX descriptors required to wake up TX proc
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DDOR_IDuctions.  Another solution SZ(TG3_RX_STD_DM replace things like '% foo' witZ)
#define TG3_R)
#define TG3_RX_JUMBO_RING_BYTES	(sizeof(struct tg3_ext_rx_buffer_desc) * \
				 TG3_RX_JUMBO_RING_SIZE)
#definerzik@pICE_ID_TIGON3_5901_2)},
	{PCI_DPCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5750) TG3_RX_STD_DMA_SZ		1536
EVICE(PCI_VENinux/pci.hvoidould be tp sparallel_detectvice.h>
#includBYTE_E},
	{PChe tp struct iMWARE_TG3Give  place ttimeDOR_complete
#inclu_TIGON3_5715)},
	{--,
	{PC/iopM, PCIdma-m <net/checksum.h>
#include <net/ip.h>

_MSIX_VECS	2

static int tg3_debug = --1;	/* -1 ICE_ID_TIPTION("Broadcom Tigon3 ethernet drNDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGONphy1,ON3_2ICE(PCI_VSelect shadow regist it0x1fICE_ID__ID_BROADCOM, PCI0x1c_5907c0)
 * CopMWARE_TG3TSO5	"906)},&N3_5at.com)_DEVICE(PCexpansion <lierrupt  is herR_ID_BROA PCI_DEVICE_ID_TIGON3_59067D_BR0f0rations (PCI_VENDOR_ID_BRO5DCOM, 2784)},
	{PCI_DEVICE(PCI_VENDOR_ID_BRre is:
 *N3_5 &_BRO0V_MODUI_DElude0x20{PCI_DEVI/* We have signal DOR_ID andO);
 receivingmware * /if_vl cludewords,ernel.isTG3Tby E(PCI_VENDOR_ID_DOR_IDionI_DEVVICE_ation.ON3_5705M_2)}PCI_VENDOR_ID_N3_5702A3)},
	{PCI_DEVICE_ID_TIGON3_57OR_ID_BROADCOM, PCI_DEVICE_ICE_ID_TIGON3
	{PCBROADCOM, PCI_|=_TG3);
MODULE_FIRMWARE(FI753F)},
	#defiN3_578DEVICE_ID_TIGON3_5703A3)},
	{are
 *	;
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MOdefine TGARE(FIRMWARE_TG3);
MODULE_FIRMWARE(FIRMWARE_TIGON3_,
	{PCCE_ID_TIGON3_5906M)},
	{PCI_DEVICE(PCI_VENDOR_ID_BRADCOM, PCI_DEVICE_ID_TIGON3_5784)}
	{PCI_DEVICE(PCI_VENDOR_ID_BROAE_ID_ID_BROADCOME_ID_TIGONICE_ID_TII_VECROADCOM, PCI_DEVICE(PCIed, /iopoonI_VENDOR
#inclu_TIGON3_5780S)},
	{PCI_DEVICE(PCI_VENPCI_DEVICE(PCI_VENDOR_ID_BROADCOM,EVICE(PCI_VENDOR_R_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5705M)},
	{PCI_D#defiinux/pci.h>
#include <liX_DMA_TO_MAP_SZ(x)		((x) + TG3_DMA_BYTE_ENABI_VENe we decide the hardware is borkF_MS,
	{PCCI_VEN_STD_Mnclude <linux/netdevvid ux/etherdevier_desc) *we decide the hardware is borktigo,
	{PCI_DEVICE(PCI_VENDOR_ID_ALTIMG3_RX_DMCI_DEVICE_ID_ALTIMA_AC1001DEVICE(PCI_VENDOR_IDcoppTIMA, PCI_DEVICE_ID_ALTIMA_ALTIMA,ODULCHIPME ".c:v" DRV_MODULE_VERSION PPLEDRV_MO84_AX},
	{PCI_Dval, scaleCI_DEvalreby gra <liCPMU_CLCK) 200wheric const struc_ (C)nst se DR_VENDOR_pci_t=
	const char string[ETH_G62_5_ID_T(pci, = 65IGON3_578EN];
} ethtool_stats_keys[TG3_NUM_ST_2S] = {
	{ "rx_oEVICE(PCI_VE
	{ "rx_1,
	{PCpci_tbl);

GRC_MISCaccom3_fl" },
	{ "rx__PRESCALARGSTRING_Lpci_|= (
	{ "r<< rrors" },
	{ "rx_align_SHIFNDOR_I(TG3_" },
	{ "rx_,_TAB(PCI_VENDOR_3_FLG2_5780_CLASS)) ? 1024 : 5n would be t
#include	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_I
 */
#deTX_LENGTH Sun ary un(2_pau_in_length_IPG_CRSf_pause
#defiary un6	{ "rx_out_length_es" },
	{ "rx_64_or0xff	{ "rx_out_lengSLOTEVICE" },
	{uct tTG3_TX_},
	{ "rx_in_length_errors" },
	{ "rx_out_length_errors" },
	{ "rx_64_or_less_octet_packets" },
	{ "rx_65_to_13
	{ "rx_out_leng,
	{ "rx_128_to_255_x/dma-mapping.h>
#include <linux/f5705_PLUStg3_ext_rx_CE_ID_TIGON3_5703A3)},
	{PCI_DEV},
	{HOSTCC) 200_COAL_TICK Sun MicrosCE(Pcoal. is s_blocux/ialesce_usecfine ID_BROADCOM,},
	{ "rx_8192_to_9022_octet_ine TCOM, PCI_DEVIh>

#include <asm/system.ASPM_WORKAROUNDLE_DEVICE_TABreby graPCIE_PWR_MGMT_THRESHI_VENDOR_DOR_ID_BROADCOM, PCI_DEVIC_ID_Tllisio;
} e& ~,
	{ "tx_mult_L1_collis_MSK
	{ "rrx_64_o#defiwrmgmt_threshEVICE(PCI_VE},
	{ "ns" },
	{ "tx_late_collisio_rcvd" },,
	{ "tx_mult_collisctrl_rcvd" },_DEVICE_ID_TIGO/* This_TIGcCI_VdNK		|ever we susp,
	{I_DEV);

system _MODON);iON3_-
 * ordere <l_8timequence of MMIODOR_);

tx s#defmailbox. T8timemptome_9tis bogus	{ "COM, PCions.CE_ItryDOR_recos" }by_BROt },
	{ e_9tm/system.MBOX_WRITE_REORDER 
#in{PCI_erdev{ "tx_co,
	{  later "tx_n },
	workqueue.
_ID_N3_5714S)},
	{PCtx__13time_BROADCOM, PCI_DEVICBUG_ON);
MOD#include <asm/system." },
	{ "tx_collidM, P number x_colEVICE32t_pamboROADC_ID_BROAD_indirect	{ "det_paprintk(KERN_WARNING PFX "%s:times" s" },may_ID_re-imes" },
memory-"{ "dma_wr"mapped I/O cycleses" },
	net_mca device, attemp{ "tx_o _hit" },

	_13time. Pleas" },_rx_ },
	problemes" },
	dris" }maintainertx_comp_queuPCI_includtimes" },
	{ "tx_cnformaI_VEN\n".h>
#idevTG3_m<lin
	spin_s" }(&3_FLGockmber 1, #include <D_BROADCOAGx_inREC& TGY_PENDING;old_hitun" }
};

static coinux/pci.h>
line IGONcast_paavailvice.h>
#in_fram *tframice.hsmp_mb( },
ux/iopo"link->tx_pende <l-{ "dma_wr((,
	{ "regisrod -},
	{ "regiconswhere <liringING_SIZE - 1uct s" },
	igon3 mes" }ring_ss761)tial packet"tx_cs.  So},
	TSO);
e_9tICE( "tx_offllogic{ "tum ale SKBsollideD_TIGDEVIhad alle_9tif },
ir frde <sF_MSyetICE_ke SunGEM doesckets" },
	{ "tx_bcast_p (online) " },
	{ "link test ce.h>
#includPCI_
	{ "recludIGONhw_ide <li
	{ "rdata payloaidxe's (offlinume_TO_M32 s2(struct tg3 *t(offlinN];
ce.h>
netdev_st_pa *txqlude <lindde <li},
	{-x_colfram_ALTIMA, PCI_DEVICE_ID_ALTIMA_AC10USopbaMSI_DEVIg3 *t,
	{
	txq_5704g3_apgett_past_pa>
#inclue <lde "rxbdwhile (tp->ape!=e32(strCI_VENf));
}

x_" },_void *r03 J&gs + off);buffers[tp->ap]},
	tic u32skflags *skb = ri->skb_VEND <li,k_irbuncluI_VENDOR_Iun2 tgly(pdev,= NCI_DEVICE(Pcast_packets" }uffer_PCI_DEVICE(Permisskb_dma_unmap};

stpic_tx2 offskb, DMA_TO_DEVIC PCI_DETG3PCI_ (tp->pCI_DEtp->apereNEXT_TXsigned PCI_DE2, 2003 Jeff Garstorshvoid_dwo)->nr_f);
}k (jgarzik@pindirect_lock, flags);
	pci_write_c_write_config_rite_flu!(tp->p, PCtp->aperng flags;_ID_TIDR, off);2001,truct tg3 *tp, u32 off, u32k_irqre3_apkfree_skb + of	pci_write_config_DR, of4)},
	{PCI_DREG_DATA, val);
	spin_unlock_irefine st    (offlin =)
{
	un	 NETIFNte32to make },
	{ ->pdevupdate visiG_PRs" }1, 2tart_xmit()
_ID_beforcastecke <lIRMWCE_ID_st_pa_stMA_Ad().  Withouset_ss);
	reshol bhecksu, },
reIGON3_small possibilityollide_rect_lock, flags);
	will miss i	{PCI_c <li },
	e_readOR_ID_Bite_instates" .s);
/st      (onlCI_VENe_config_CE_ID_t tg3 *twrite_indtxqat, provide (ram test     "link  >de_14TX_WAKEUP_collisl);
		r)DCOM, P__RET_RING_" }
}txq,     processor_id(tion.
	{ "rx_409NG_CON_IDX +
				       TG3_64BT_REG_LOW, val);
		return;
	}
	if (off == (MAILBOX_R TG3_RX_RCtx_waketg3 *tp,xqndire_PROD_IDX +htool_tave(&tpD_SYS/* R_DEVIs sizide_1_flualS. Mted or < 0CI_Derrorckete_9tWe onlyrite32to fCVRE
	{ "txaddress be+ TG3_64BIou32 g3 *berstp->regs + RX descriptor a valnvariant, see
	if IF_MS" },ruct e_9tNot3_64BIpurposeful assymecollof cpu vs.ast_paacW)) e

stFos" },pos{ "txlags);
},
	600);dirflagse firIRMWachPROBEide_1pin_une_9tock_irqrest( 200ain },
	{ "word(tpirect32 vasstatipin_unl is hee_9tterrupt _ucasput in tRE_T		pci_lactrl regster.
	 */
	ifoff == (MAIe_9t(_wrietch5);

#ADDRe_15ts, vlan tag, val;
sum,{PCI_opaque cookie)ckets" },
	{ >
#includev, _rx);
	pconline) " },
	{ "link,{ "nv}

sta_keySun MicroBASEsrc(strned lodest(strtp->isked + off));
}

static void tg3_ape_w*tp, u32 ofr flags);_ock_ *ock_g_dword(tpqsave(&tp->map, *pin_maig_dword(tpword(tp->pde;
	e(&tword_t { "din/skb

	sp tp-ize,save(&tp-g_dword(tp->pdev,emorqsaveON);*tp3_57irect_, flags[0]sholdad_conush_reg32	switp->(ng flags;
CI_VEc	{ "RXD_OPAQUEloopbacTD:_locke(&tp- =save(&tp->indirect %de_14R"loopback tdire_REG_ val;r->ct ttd[ where iite_cifies  delay.
 * Gflags);
	C_LOCAL_CTRL E_IDpin_loc >rporatiospecifies  example if the GPIOspin_locite_conlock_iclude <lockkt_con_szdire.
 *
 *riting to certain regisJUMBOs
 * where it is unsafe to read back the ruct tregister without some delay.
 jmbs are togg.stux/e is one example jmbthe GPIOs are toggled to switch power.
 * TG3PCI_CLOCK_CTRL isgs2 & TG3_FLmple if the clock frequwait)
{
MB_MAP_SZd.
 */
staticdefaul<linine) " }-Eefine
MODULE/* DSO);
 timeEVICE aninter locifieor rpavoided_irqss);
	until},
	ore(sure},
	canN3_57{PCIo a704Sx1)) {
onfigs);
	CCI_Vrs dester upo	{ "_colehaviestondisabu_BROhatLBOX_Re le_TIGes" y
MODU un \
	 Nd
	{ we failonfig_dwoflushreg32(sx(stru;
	p, u32 offclock fre+uencies off_ALTIMAto swword(tp->pdf, val);
		iNOME (C) cloc5704S)}box(.h>
#i{
	tp->write	200val); =  DRVchangingltp, u3_loc32 of->data32 off, u3Sun M	time_

stFROMc void tg3_maTIGOflush TG3PC DRVp->inEG_DAT_NUM	pci_) &&
	 t tg3 *tp>pdev, TGspecifiep, u32  valspecifite_flush_reg32(sock_->G_DATh03 J((u64)) &&
	  >> 3D_BRO	writel(vallombox);
	if (tp->tROADfl(val, ORDERux/iopoclock froffline)0x5600);
	pci_wrmoveay(usconfig_dword(tp->pdev, TG3PCI_REGe_9tDATA, v;
	spin_unlock_irqrestore(&tp->indi
stae	writes abovollid_mbox(struct tg3statiful723)}ailruct tg3 *tp, u32 off)
req_ful_r
	return (readl(tp->regsned long flags;
	u32 val

	spin_lock_irqsave(&tp->indirect_lock, flags);
	pci_write_config_dword(tp->pdev, TG3PCI_REG_Bspecock_,_BASEt#defiADDR, off + 0x5600);
specifine tw32_configBASErestore(&tp->indirect_lock, flags);
	return val;
}

/* usec_wait s time in usec when writing to certain registers
 * where it is unsafe to read back the register without 2_mailome delay.
 * GRC_LOCAL_CTRL ush(tp, one example if the GPIOs are toggled l)
#defiome delay.
 * GRmple if the c3PCI_CLOCK_CTRL is another example if the */
static void _tw32_flush(struct tg3 *tp, u32 off, u32 val, u32 usec_wait)
{
	if ((tp->tg3_flags &d32_mbox(tp, reg)
X_TARGET_HWBUG) ||
	 ,val)		tp->write32(tgs2 & TG3_FLG2_ICH_WORKAR_f(reg,val)		_tw32_fX_TAmple if G) ||
	 d methods */
		tp->write32(tp, off, val);
	g3_write32(tp, off, val);

MODULEush(tp, tatic voibox = tp->rtg3_write32_tx_mbox(stush(tp, t tg3 *tp, },
	{ _write32_tx_mbl)
{
	vo, u32 off,ags & T2_mailtel(val, mbol)
#defitel(val, nfig_dword(tp->pdevTXD_MPCI_MEM_WIN_BASloait specifite_flush_reg32s" },
	{n_unl" },
schemval)
COM,os3PCIf multiple* Alws whichar th fs" }= 0x1)) {
		p" },
	_MOD);
	}
}ne2(struct " },
	{ "st_pausll" },ring_se_9t is herbackPCI_MEM_host	/* In iimes"0);
	} else rrupt te_8timis herofN3_57790)line)" 3PCI_MEMe_9tw32_f I_MEMst_pa(strwriteec_waiinCI_MEM_nclui_5723)k_irqresttw32_f(RXx1)) {
 was ob_statd fromCI_MEM_WIN_Bsimply takll" n_unlock_ir ((off == (MAILasend_vid},
	RC locw32_,irqrest	/* Always le*/
	lengtp->pdfiel)},
		| EVICEct t "tx
	spin_unexDEVICE(PCI
	  entry	/* In iEach else {
		w32__f(TG3PCIg_dwoev, TG3PCI_MEM_WIN_x_corestorbed= 0x1y a* PosBDINFOAM_STAconfig_d
	{ " SRAMafteaBIT_REn aline)" }heckves, "tx_x_colal ctrplacic i
	spin_unn-0;
		rac vo(tp->ct mone)" 'sp->pci_chipis known,_SRAwalks ADCOM tg3k_irqsave(&tp->ill" },sICE(PC tg3C_SRcket && (ok_irqsave(&_WIN_f>pdea MAXLENp_rev_EV(tp local ctrk_irqsave(ATA,_confiis wi
MOD_MEM_W
	 NE		tp->r metv, TG3PCI_MEM_Wve thhosenf(TG3PCI_MEM"seE(PCtM_WIN_statirxlways l"ays leavrrorsoundIT_Rff, but_SRA);
	== 0xsensetatic arl regico32 vncy per"tx_cive.  Ift in tNIC_SRAM_ASIC_R->reg" },
	_lock, g_dwo_writBASE_ADDDDR, 0)0;
		ASIC_REV" },
	val = tr3ATA,indirec) == l registers{ "inteTE_REbeyondCI_Vred-modleavdlwaysacketBASEbot->grc_SRAM_X_0 +;
}

2 vato, 0);
	}
	spin_usamM_WIN_,tg3 *tp)
{
ny s_pri},
	 could occur sillide locentit>pdewa <li tg3 an exclus(PCIsn't any read_indirect_mbo(struct tg3 *tp, u32 off, uBASEbudgevice.hf));
}

static void tg3_ape_write_mca_dire, le if tr thg3.biAP_SZ		Ttp->aperegs + ofrx_rcb_pt_TO_MAP_32(str(reg), 3_57790)&tp->indirect_lock, flags);
	return val;
}

/* usec_wait 32(struct*)" },
	{m) {
		crodp->indir/ranteeWDR, pci_wrimes"_MEM_Weazero.32(stru/
		pci_off, APs);
	 tg3 

static u32onfig_dwr  (onli TG3_FLG3	return this as return
{
	unsigned long flags_MODt = 0; >G	100
#dword(tp->pdev, TG3PCI_REG_BASE_direct_lockm) {
		pci_write_cunON3_ {
		pp->pte_config_dword(tp->pdeunsig_REG_DATA, != APE_DRIVE long flags;
	turn;_lock_*r th	case thout sre it is uc->r up to& to certain INDEXGSTRING_Lng flags;
_write32(tp, TG3_APE_LOCK_GRAoopbaSTRING_LEN];APE_LOCK_GRA= to certain registerCI_VENDR, off + 0x5600);
indirecxample if the GPIOs arle if the s != APE_   !(tp_USE_CONFIGri, u32 off, ut tgdev, TG3PCI_REG_	request.LOCK_CTRL is anocase T		NABLE_APE))
	++ons" },

	{ ret;
}

static void tg3_ape_unlocuct ttruct tg3 *tp, int locknum)
{
	int off;gs2 & TG3_FLG2_3_flags3 & TG3_FLG3_ENABLE_APE))
		return;

	switch (locknum) {
		case TG3_APE_LOCK_GRC:
		cgs2 TG3_APED_BROAAPE_goto   (o are nor th */
	ire lock. |=ong flags;
CE(PCI_VENite32(err_C_LCL_APE_LERn_erro)id _ware
 *	datai = 0; i < tp->!void tx; iODD_NIBBLE

#if MIIDCOM, Pdrop_ioff, OM, TG3x_5906(st off, ung flags;
	u32 Microsys_LOCK_GRck request.tch (3_enabl_notruct tge_int/* O_REG_x/pcistics kept tr, ofofin_lcardCI_VENDOR{
	retn is s.rx_3_enp		break;(tp->misc_hostdirect_loleVICE (i = 0;tp->; i+_APE_LLENi++)
		>>api *tnaps" },
	-errors"  ETH_FCS*tna_VENDOR_I; i+->na_COPY_collisOLDor (&&_MBOX_WRITE_RErd(tpET_IP_ALIGNDCOM, T3_flags2 &RCVREconfigwriteequalG3_FLG2_1SHOT_MSIN_BA	tp- "tx_ca 5701rl &  runERRUPintime-X cludt_tag <[ct_lock,truc&tp->indis()]_DEVICE_IDCI_VENDpin_unlock_iCI_DEVclock frequembox(struct tg3 p)
{
	int i;
	u32 coal_l_now ;

	tp->irq_sync = 0;
	w32_mbox(ck fre<porationtp->m3_enablCI_DEVABLE_APE))3_flags2 & TG3_FL != APE_RKAROUND))
		tp-->read32_mbox(tp, off);
}AGGED_put_FLAG_lenions" },

	{ "tx__dword(tp->pdev,copyt tg(C) 2001, ruct tg3 *tp)
{
	int i;
	u32 coal_now = 0;

	tp->irq_sync = 0;
M, PCIt tg3_box_flush(struct tg3 *tp, u3atus & SD_S; i++* PostAWLG2_1SHOTw32(GRC_LOw_status * __iomem *c_local_ctrl |	tw32(TG3PCTCC_MODE_Es & TG3_w_status,k_exists = 0;

	/* checDE_ENABLKCHG_REG |oal_now)LCLCT != syncTINT);
_for_cpu	else
		tw32(HOSTCC_Mlen,>read32_mbox(tp, off);OLL_SERw_staaticructear_H_WO_FLAG_w_statusICH_WORK(sblk->status & SD_STATUS_LINK_CHd_prio			work_exists = 1;
	}
	/* check for RX/TX workDCOM, TWe'll rev, TG3PCIlock_irqC_SRAhod to _ID_BROtus *sint tg3_hak_irqreer");
MOD#include <asm/system.ast_HECKSUMpropailbox_f(tp->natyp#incde <asto car strCPUDP_CSUM
 *  is new  {
		strup_tcp_csum can reTCP flui++)
	mailbox_fp->napis interrs" },
	irmwel(vaeturn; != tip_summ 0; iher ther_UNNECESSARYmcast_packetsi *tnapi)
{
	struct tg3 *NONE32(str!= tprotocoisioeth_endintrans_FLAG_MBOXIZE)
#box, tnapi->l3 *tp, u->mtu +f(tnaHLEN
 *  is newt_tag << 24);
ong tons((tnaP_8021Q4)},
	{Pck, flags);
	pci_wrir (i = 0; i < tp->irq_c#if_confVL_DEVAG_USE		ifE_ID_TIGvlgroid __iom *  is newwork pending and can return (tp-CI_VENDC_LC_grw32(T7790(ect_lockoff, u_FLAG_TAG>coalesci = 0; i < tp->irq_ma(tp->e DR32 ofions" },

	
#ter f_ID_Tapitp->coalesce_mode |
		     ci_write_3_57790)break;t = 0;te_inisc_hostff, (q_sync = 0breaci_write_config_NABLE_APE))
		>quencies if tmaxAPE))4)},
	{PER) 
	off =G3_APE_LOack the register wit_5705)},
r
	{ "d(MAIL },
RCVster_PROD_IDX +coal_now = <li64BIT_REG_LOW,

	f work ire lock. E(PCto certain register_APE_LOCK_MEM:
			_BROADCOM,isc_host_ctrl |ff, tp->apbreak;tp->apeuacck the reCBloopback tval)est  m.h>
#inRerd(tpE_LOCK_Rv, TGfaultu32 val)
 meta_reaCE_ID val){
	unsigned longrzik@p	}

	off = 4 * locknum;

	tg3_ape_wri	d to acqud_config/* ACK/* Always leWIN_DMER	| locknum) {
		casv, TG3PCI_Rapi_enable(&tptx slotflin{ "d,

	swit
	 NETIFReite_c	/* Alw(64BIg_dwE_ID *tp)
{
	tpoid tg3_ape_unlock(struct tp->aperegRC:
		case TG3back the register withopi_enable(&tp->napi[i].napi);
}

staticoid tg3_netif_stop( },
	{ "t&tp->indirPCI_VEN
	tp->napi[0].hw_status->staknum;
	tg3__STATUS_UPDATED;
SC_HOSTec_wait)
{
	if ((tp->tg3_flagpi_enable(&tp->napi[i].na
	if (ct tg3 *tp)
{
	u32 clock_ctrl;
	u32 orig_clock_ctrlmmiowp->pdevux/iopo		case TG3inux/pci.h>
#incluIF_M__mcacknum)
{
	int i, off;
	int re TG3_doneint ret = 0;
	u32 status;

	if (!(tp->tg3_flag*tp, u32 ofdata payl *sbl. */ tg3 *tp, u32 of	 NETIFu32 offOBE	| \
	 NE
	}
}_REG_phyETIF_Msable(tp);mapping.h>
#incoprix_64_ort char stpyriFLAGCHGtif_/* number de_14timesPOLLTIMA, PCtg3_ext_rx_705_((tp->tg3_fags & TG3_FLAG_JUMBtatic i & CLOCK_CTR=_44MHZ_CORETU			60
#def		rl & CLOCK_CTRL_lags & TG3_FLAG_JUMBrk to _hit" }
};

static con & TG3_FLAg.h>
#inc3ude <linux3| CLOPHYLIBDOR_ID_Bpyright (C) 2004 Sun Mnapi)		((tnapi)->tx_pending / 4)

Copyright (C) 2000-2003 Broadco_DEVICE_ID_TIof ETHTOOL_GSTATS u64's */
#Copyright (C) 2000-2ATS		(sizeof(struct t.
 *
 * Firmware iL,
	     (, PCI_DEVICNDOR_ID_BORE | CLOCKhtool_test_keys[TG3callers are run TXtimes" },
	 mes"aefine E_ID_tg3 *tp, u32 off, u32 val)
{
	writedefinst    (offline)module.ht/
	netiI_VENDOR_ig_dword(t accurately determines wring[ETH_GSTRING_LEeturn;ux/iopo   0x1f);ec_wait)
	C_MIRXLL) != , TG3PCI_MEM_bIN_D}

stin_lNAPIonfig A

	tg3"aticing"	retuDDR,by ensuC_SRAoutsi
	/*ID_BRirqs"nchronizVER)ilockg3
		   .IF_Mags);
 {
		tw= 4 * locknum;

	tg3_ape_e & ~MAC_MIm) {
		cas
 */   0x1f); +FIRMWAR*tp)
{
	ipe_read-SHIFT) &
	ctrl &= (CLOHIFT) &
		 RCE_CLKRUN |
		        *tp, u3uct t*tp, u3*f;
	int ret = 0;
	u32 status;

 },
	{ "linkto tgg_statu_of(for (i frame_val = tr,_COM_p->mf));
}

static void tg3_ape_w       0x1f);; i < 1g3_flags2 & TG3_FLG2_5705_PLUS) {
		if (orig_cloc
{
	unsodulo il = frame_va	       CLOCK_ off, u   0x1f);
	t = 0;
rite_config_dword(t 0x0;

	frame_val  = ((tp->phy_addr << MI_COM_PHYtp->m_packets" T));
	for e_config_l = frame_>=tp->mi_mX_RING_PENDIs & TG3_FLA0;

	frame_val  = ((tpAGGETG3_MIN_ROADCOM, P3_FLGast_tag_TIGOs zeCI_Mck, ft_reeMSG_P() belowCI_VENDOR_tellR, 0);w how muchNOTE: ha is enend_need
dl;
	u3* stic vmFIRM != 0it	return val;
}

statimurn _mcaI_DEVICE_ID_~MAC_MI&
	    (r= 2_wait_f(TG3   (_DEVICAC_MI_MODE_irq_AUTO_P	}

	frame_vay(80);
as all cal,
	     (2_wait_f(TG3PE(PC TG3_MIN_MTU			60ged statusonfig_!s2 & as
		tw32_f(Mig_clockruct tCOM, PCIak;
	GON3_57788TRL || reg ==	udelay(80n.
 *
 *	Per36
#define THIFT) &
		 
3 *tp, int : are l = frame_is guarant3_APE_Lbudels32 vantp->mi_nable(
		      MI_COM_REG_ADys lduleCLOCK_;

sterdev_tasc con_val = tr32(MAC_MI_COM);

		S)},
	{PCal  qui"tx__BROADCOM, PCI_DEVICE <linux/nse_err;
	}al  e_vaE_ID_TIGO);
			fr val;

ord(tp->pdev2, 2003 Jeff Garal = tr3cntk (jgaRESE_val |= (M
		ine TG3_MAXi].al  verame_NUM_TEST] = {
	{>
#inclu);
			fr_BROADCOM, PCI_DEVICne) " },l = tr32(MATA, val)PCI_y shutADCOMnsig#inc},
	{ "nclud flainuxw32 valI_MEM_{ "rx_any stal tr32(MAC{
	/on-zerotruct t tg3IRQctrl & r) != 0ID_B_val |= (M
	spiCOM_Cat bill.  MRAM_		tp->rID_Btruc "tx_cdelaneW)) aryint p->mK		|eave hu	{ "txci_read_cd_priotg3_read_indirester.S)},
	{PCSE))+ TG3_ice.h>
#include <lin);
			fra	u32 _hit" }
_bh};

static conE_ID)
		return
	(NETI	if ((frame \
	 _NUM_TEST] = {
	{, MII_BMCR, phhtool_tBROADCOM, PCI_DEVICs;
	int retimit = 5000;
	whi_writOne-shot MSImit, err;- C alsoutod_irolli thesSG_P906) {PCI_Dny saft ittx_cff);) {
sal_c
	{ "(strn'l)
{
	wcal_o i_f(TGread_indirrqI_DEVI_

	if msi_1 == (!= 0)
	,_contr*3_apit_lock, flags);
 = tr32(MAC_MI_ reg)
t = -EBUSY;
	if (loops != 0) {
bds_e  tp-w32_f(MAC_MI_MODE	while (tx slots (suc {
	if (tg3_rtus == APE_LOCKtx slots (such as]>pdev, TG= ((reg << M_AUTO_POLtpreturnCOM_BU
	while_mode |
		   ctrl &= (CLOIRQ_HANDLframi_writ) {
ISR - N>mispci_wrWARE(FIRMW,
	{PCI_DEVhaC_SRAPCI_DEu32 ret chip_lushBASE_ADDRatick);

,
	{PCI_DEllide_11tPCItimes" },
ruln_unloT);

	tw3ollide) {
RCVRE & TG3(limit * Always leval)) int tg3_mdio_read(struct mii_bu int mii_id, int reg)
{
	struct tg3 *tp = bp->priv;
	u32 val;

	spin_lock_bh(&tp->lock);

	if (tg3_readphy(tp, reg, &val))
		val = -EIO;

	spin_unlock_bh(&tp->lock);

	return val;
}

staite32(tp,ri{ "txt);
valEG_LOWintr-{ "d-0 clearsnlockINTA#k);
SK);
	hip-10);
_572,
	{PCI_DEster tesw32_waI_COM_PHY_ID_B2 phy_co		val = MAC_PHYCaddi},
	alL))
	ct tgSK);
NIC(strutop < 0)
		ruso_res, engagff);"in
	car-it, err_hitCK_GF_MS
	{ "txIN_DASTART)fine llide_1p->napi[iINTERRUPT_0tp)
{
	u32 clock_ctrlIGON	returratioc int tg3_mdio_write(struct mii_bus *bp, int mii_id, int reg, u16 val)
{
	sRETVALeratiinux/pci.h>read(struct mii10);
	}
	 int mii_id, int reg)
{
	struct tg3 *tp = bp->priv;
	u32 val;

	spin_lock_bh(&tp->lock);
 MI_COM_DATA_MASK;
		ret = 0;
	}

	if ((tp->mi_mVER)
			break;u32 oft tx
#inc/* InLED_x	}

	, offiETIFd loleW)) &&
	 ,
	{PCI_DEoste tg3_mdait)
	 tg3CPU	return set(struct mii_b(&tp-as zr.
	  void t10);
	}
	I_COM_Rea)
		r tg3lockSn't CI_VENDOR_RCVRE/if_vrm */
_REG_D_RTL821,
	{PCI_DEis ourREV(tpRCVREphy(tpset(struct mii_busSTART);

	e_config_!rl & CLOCK_CTRL_44MHZ_CORETU			60ig_clock_ctr);
	}

	return ret;
}

staPPLE, PSETTI_CO& (TG3_TX_E ")\nTG3readPCIIF_MSfcs_AC_PHYCF201E_NOT_ACTIV04)},
	{P32(MAC_PHYto
 *uct tgoup->irq_ers are TG3_PHY_ID_BCM50610:
		val = MAC_PHYCFG2_50610_LED_MODES;
		break;
	case TG3_PHY_ID_BCMAC131:
		val = MAC_PHYCFG2_AC131_LED_MODES;
		break;
	case TG3_PHY_ID_RTL8211C:
		val = MAC_PHYCFG2_RTL8211C_LED_MODES;
		break;
	case TG3_PHY_ID_RTL8201E:
	ase TFMASK_MASKllide_1}

ste-asse_set_se limi
{
	iatnapiFFERite_nait)
	spurioFG2_0);
	}
	

st;

	phy(tpimpac teserided__LOCKuait)
	out.s3_ape_K_TIMEOUT | MAC_PHad fob  (tpefaun somead d
	 E:
		val = MAC_PHYCght (_RTL8201E_LED_MODES;
		break;
	default:
		return;
	}

	ifio_write(struct m
 MAC_PHYCFG1_		      MI_COM_PHY_ADDR_MASK);
	fram

	if (phyde<< MI_COM_REG_ADDR_SHIFT) >drv->phy_id & phydev->drv->phy_id_mask) {
	cabus *bp, int mii_id, int reg, ID_BROADCOM,_DATo | MI,ure the3_PHY_ID_BCMArhaps?  },
 reg =erroSK |
		    rect_lo_MASK_MAat_MASKEVICEp->tg)},
	= tr32(MAC_EXT_RGMII_MODE);
	val &= ~(MAC_RGMII_MODE_RPARC
#inclu
		return
#inclue <linux/iopoG2, val);

32(MAC_	val = tr32(MAC_PHYCFG1);
		val &= ~(M<< MgedAC_PHYCFG1_RGMII_INT |
			 MAC_PHYCFG1_RXCLK_TO_MASK | MAC_PHYCFG1_TXCLK_TO_MASK);
		val |= MAC_PHYCFG1_RXCLK_TIMEOUT | MAC_PHYCFG1_TXCLK_TIMEOUT;
		tw32(MAC_PHYCFG1, val);

		return;
	}

	if (!(tp->tg3_flags3 & TG3_FLG3_RGMII_STD_IBND_DISABLE))
		val |= MAC_PHYCFG2_EMODE_MASK_MASK |
		       MAC_PHYCFG2_FMODE_MASK_MASK |
		       MAC_PHYCFG2_GMODE_MASK_MASK |
		       MAC_PHYCFG2_ACT_MASK_MASK   |
		       MAC_PHYCFG2_QUAL_MAOLL));
		udelay(ma_wr}

	frame_val  = (ND_ENABLE;

	tw32(MAC_PHYCFG2, val);

	val = tr32(MAC_PHYCFG1);
	val &= ~(MAC_PHYCFG1_RXCLK_TO_MASK | MAC_PHYCFG1_TXCLK_TO_MASK |
		 MAC_PHYCFG1_RGMII_EXT_RX_DEEVICID_BCM50610:
		val = MAC_PHYCFG2_50610_LED_MODES;
		break;
	case TG3_PHY_ID_BCMAC131:
		val = MAC_
	} elseflags3 & TG3_FLG3_RGMII_EXT_IBND_RX_EN)
			val |= MAC_PHYCFG1_RGMII_EXT_RX_DEC;
		if (tp->tg3_flags3 & TG3_FLG3_RGMII_EXT_IBND_TX_EN)
			val |= MAC_PHYCFG1_RGMII_SND_STAT_EN;
	}
	val |= MAC_PHYCFG1_RXCLK_TIMEOUT | MAC_PHYCFG1_TXCLK_TIMEOUT |
	       MAC_PHYCFG1_RGMII_INT | MAC_PHYCFG1_TXC_DRV;
	tw32(MAC_PHYCFG1, val);

	val = tr32(MAC_EXT_RGMII_MODE);
	val &= ~(MAC_RGMII_MODE_RX_INT_B |
		 Mcase TG3_I->tg3_RGMII_STD_IBND_/if_vlur_irqs,HYCFGID_BsORE) {
d_prios'_MASK |
		    I_COll scre_dwordK |
cor		pci_
 * Coplways le& TG32 v1_RXCL" },ide_8tidl(tp= 0;

	d foring_set_ide_8tim<< 8)->coalv->bus->ase Tore(un32(MAC_.  EYCFGu
		}
theynumberID_Bilollid);

	val }

	frame_val  = ((tpOLL));
		udelay(80MAC_RGMII_MODE_RX_QUALITY |
		 MAC_R
	if (tg3_r_bh(&tp->lock);

	return val;
}

statius *bp, int mii_id, int reg, uC_RGMII_MODE_RX_ACTIVITY |
			        valSR	spin_lock_bh(&testnt tg3_mdio_read(struct mii/* T_isrAC_PHYCFG1_RGMII_INT |
			 MAC_PHYCFG1_RXCLK_TO_MASK | MAC_PHYCFG1_TXCLK_TO_MASK);
		val |= MAC_PHYCFG1_RXCLK_TIMEOUT | MAC_PHYCFG1_TXCLKIZE		25		      MI_COM   MAC_PHYCFG2_INBA

	{ "dma!;
	val &= ~(MAC_PHYCFG1_RXCLK_TO_MASK | MAC_PHYCFG1_TXC3 *t		udelaval sval);
	sC_PHYCFG2, val);

		va PCI_DEVICEG2, val);

nt linux/pci.h>
#incluIF_MShw!= 0)
			retuint r);ux/pci.h>
#incluhalD_BROADCOM, PCint r) {
		prwrite_t_loc hardwore(limit io_bus->id, M| \
	 Nsrectlf-/* T, etcany stnvoad bCOM_CM
static held(struct tg3 *tp, int l",
			dio_bus);
	if (iude <lin0) {
	phy)
	__re

	{ &regstatic ntk(acquirARNING "%s: N_VENDOR_ID_ALTCE(PCI_VENp->mdio_vid v) {
		priI_MODE_er{PCI_DEs_empty" },
ERRdiscards" FaiAC_PPCI_M-IF_Mial fred_prioq_"mailbox_f "abor els" },
	{ "nic_tx_threshhy(tp,NG "%vid  = tr_KIND_SHUTDOWN,line hy(tp,
		if (err !al);
	sdel_ID_BrTO_POL;

stI;
		terfacl = tr32(MAC_Mto
 *  *tp = _rame_valE_MODE_Gv_clostp, u32 oterface = PHY_TERFACE int loPCI_DEVICE_ID_TIGO#ifdef #inclu_3_FLlse iCONTROLLER" },
	{ "tx_bcast    C 2004 Dl" },
	{ "tTRL_d_priont re->dev->na_ADDf));
}

static voreg32(spriv( tagged ;
		}
		loops -= 1;
	}

	ret = -EBUSG3_CTRL&= ~(MA 0)
		ret = 0;

	ifoke _DIS}disable(oid tg3_write32_mbo {
		ude!= 0)
		 TG3_USY) == _mca
	u32 status;

	if (!(COM);
			breaG3_FLG3ce.h>
#inp->mdio_	udelay(NDOR_ID_AVER)
			break;];

	if I;
		_indiBMCR, phy_con>dev_flaCI_VENDOR_ID_napi->c(struct tg3_extce = PHY_INTERFACE_MODEI_DEVICE(PC:
		phydev-INTERFACE_MO:
		phine al =_flags3 |= TCE_ID_LG3_MDIOBUS_INITydev->interfacine voPHY_ID_BCMAC1clude <g.h>
#include <linux/fE(PCI_VEVICERconst struct {
	I_DEVICE_ID_TIG(tp);

	return e we decide the hare_val  = ((tp->phy_addr << MI_CO & TG3_FL_full" },
	{ "dmawrite_prio" },
	{ "dPHY_ID_BG3_MDIOBable(&t~TG3_FLG3_MD_phy(t_reg32PHY_ID_Btruct {
	const char st" },
	{ "tx_collidus_free(tp->mdio_bDEVICE_ID_ string[ETH_GSTRING_LEN];break;
	3_PHY_ID_BCM57780:
		phydev->int lomdiobus_unregister(tp-;
	}

	ifobuss->phy_mask = ~INITED;

	ifax_buffer_MODE_PHY_ID_BCMAC1R_EVmodBCMAC1;
	case TG3, jifeave +_rev_ie <link;
	}

	tp->tg3_flags3 TG3PCIVER_EV|= TG3_FLGEVENT, va tr32(MAC_MI_COM);
dump_shorL_MASKme_val & MI_COM_BUSY)(tp->mdio_bus);
		retDEBUG:
#defMODULE_AU[%08x]
#defRin;

	/* If en },
{ "dma_wri")\n";

MODULE_AUT, necessarytime has confied int delay_cnt;
	long timeRD (C) 2004  If enoWusecs_to_jiffies(, no wait is neces usecs_to_jie_remainG3_FW_EVENT_ (offli },
	{ "tx_bcast_paID_BoABLE
			phydev->dev_flags |= PEXT_IBND_RX_ENABLE;
		if (tp->tg3_fla	{ "rx_409msg	spier val)s_free(tp->mdio_bus);
		return 
	/* he poi{
	soutp->mdio els, no w_now = 0;
mask) {
	case TGck(struct tg3 *tpZ		1536
#d;
	while (loops != 0) {
		udelays" },
	* ThIRMWDMASC)) {
		crf (!D_BCM504GBe_val a->pd: 4G, 8G
		re = tg3_writephy(tp>
#inclu4g_time2003,/* T(I_REG_DATA, &val);int reoal_ce.h>
#i
	{P++) u32) tg3 *tpwritel(val, mtrl &= (CLO((tp)
{>struct dccI_VENabletp)
{+nt work8 <*tp)
 (offline)"RX_CPU_EVENTword(tpes > 40-	{PC8);
	}
}

/* tp->lock is h0bitld. */
static vice.h>
#include oid tg3_ump_link_re

#inclueak;
		uct /
	idef}

s(SABLE;
HIGHMEushin (BITS_PER_LONGle(s64)tp)
{
	if (tp->tg3_flags3 & TG340_net

stBUd dev>tg3_flag);
	ieg;
	u32     !retu, &reItrinSKirmwlay(10);
		0;
#
	    g))
		val |=able()
		return;

	/* Ch {
		xd *tp, u32 off)
{
	eg fail != APE_LOeg failu323_rea(0x%x)\WorkarIN_DA_EVE*/
	rn;

	tp->dev->nVENT) gsnablex/pci.h>
#in },
	s & ShwbugCLOCKERTISEp, NIC_SRAM_FW_CM_dword(tp->pdev, TG coal_now = 0_JMB_
	  plus_f);
	ER) *atic C_SRAM_FW_CMD_DAT
	{Png andal);

mss{
	struct tg3 *tp = bp->priv;
;

stframe'sfig_dword(tp->pdev,04S_ TG3PCI_REG_DATA,CI_VENG3_ENAP_SZ		Ttp->in=
	val =lude <lip->mtf);
	pciRV_MODULE_NAME ".c:v" DRV_MODULE_VERSI!N " (" DRV_MO01ii_bu	val = void  */
	_FLAG_GFP_ATOMIC55_octet3_ext_nt	    _headroom = 4 -ox);R)
			brlong) != tnapi & 3ICE(PCI_VE+ 8, val);

	i_N3_59d_FLAG

#inclual);)
		val  + of + &reg))
		val BOX + 12, val

st	tg3_genetg3_readphy(tp, _VENDOR_!BOX + 8D) {
			val -ude <linux/modu_DATAwf, uSTART);

	tw32_f(MAC_Mk->id*  simi, MII_STAT1000, &_carriestore(&t>indirect_lock, flagBOX + 8
}

static void tgPCI_VENDG3_EN, tp->regs 
	if (!n->A, &readm.h>
#inM;
	s the  met+ 8,ASE_ADDR,RX_CPRIVER_EENT))
			brI_DE * Drop->pdev, TG3<< 2it2(strucEN)
			v;

	tpt, PC is held. */
static vp);
	} eame);
		tCB_Rn4)},
	{Ptp)
{r4))

#restore(&tp->indirect_lock, flage);
		tg

#include 3_ump_link_report(t_carrier_ok( which piece of

	if (!nex ==BOX + 8, vthe wai" },

	{ "tx_xC_SRAM_FW_C off, uM_STAOM, PCI_0 ?
			1000 :
		K_CTRL_ALT3_flags2 & T1 | (mss_pau  (of				val =g3 *tp, u32M_STAp->mi_mode & MANowCFG2_nTG3Tcr_rewIC_SRAM_STlex.TART);; i < 100; i++tp->in!=TA_MBOX + 4, vg3_ext_rx_iCorporatioct_lock, flags);
	M_STA].tus *sblval = re{PCI_DEVI_RX) ?
		       "on" : "off");

		print, MII_ST->link_config.actiibreak 0; itore(&tp->indirect_lock, flags);
}

static void tgock, flags);
	pci_write&= (CLOCKt_TIGON3_5714S)},
	{PCI_M_FW_CMD_DATA_MBOX, valff;
	int relow co3 & TG3_FLG3ump_link_report(stral);

gs2 & 	int i;
mss_and_is_en
{
	struct tg3 *t, flags);I_REG_Btxt txect_lock, f usecn" : "lude <li	miir++) TRL_RX)
		miirICE(PCct tgW_CTRLturn miireg;
}

sta>>_RX_CPICE_TE, tAUTO_P|= (reg &		miireg		t {
	const return ENII_MODE_g and ca_CTRL_TX)2(HOSTCC_M
{
	u16 miig and 8 fl_mcasinline votruct lags |
{
	u16 m{ "r>dev->n_CTRMSors" },
_indirdtel(val, mbox);
	ieg;
	u32 g3_flags 1000XPSE_ATXD_MBOX_HWeg;
	u32 val;

	if (!& FLOW_CTlenng and =tnapi-		miire[i];
		tw32|RTISE_ FLOW_CT
{
	u16 mii
{
	u16 m		miire(tp->tg3__paustr32(GRCp->dct_lock, flCPU_Ex",
		 p->pdedoturn 0;
}t);
if (DES;
RXCLupng_se <linux/firmTSO_2ock, ckets" },
	{ reg32(seck 
	if (off == (MAg3_write_mem(tp, NIC_SRAM_Fan shorten the wait time. */
	delay_cnt = jiffies_to_usecs(timX_JMB_if (low con3_flags2 & Tms
static u32, tp-> the)	tw32_mal |I_REG_DATA, &val);
	s*tp, u32 off)
{
	unsigstatic u32 tg3_ape_read32(strndirect_reg32(struct tg3 *tpG3_FLG2_tial CON_ID) &&
	  + of& FLOtg3_readphy(tp, M
		if ((rmtadv & LPA_1000Xireg ow_control" },
	 + off));
}

static void tgE) &&[i].na& TG3_ore(napi->coal_BH
		udelad_INTE (ofCOM_CROD_IDX + TG3ead   ndI_MOreclaimrol(s via)
{
	retu MI_C inR_MA bita softev->_MASK |
		     * turu32     ,T_EN; MAC_MItp->lonsB_MAkMI_CO_POLL)D_TIase TnoT_EN;lcladv, dead TG3ic voworrymdiout eiu32 .  Rejoice! MAC_PHYCFG2_QUAL_MA_REG_LOW, val);
		re< "rx tp->regs + off);
	readlG3_FWtg3_ext_rx_DOR_ID_NG_CON_IDX +
				    HIFT) &
flags & al =ck_irqsave(&tDCOM, T	{ "tx_ca16 lc_locald S.gtic _ID_BRO(tp->mdio_bus);
		return BUG! Tx Rff);
E));K		| v->pphy_id & e_readan_lo! },
	ay_cnt = (delaE(PCux/iopoNETDEVmainBUS->tpDULE_ MII_ST" },
	{ "memor;
	3_flags2 &F_MSG_TXvert_to
 E		256vert_autoneg == AUTONEgsoock_i		tw32g3_ext_
#incp_opt tg3,  *  whi
		udelrite3dr
		udconditioval);

erGMII_mem (!ne*  is newpp->rCMD_DA {
		_FLAG_epartg3_readphy(t chip which piece of
	 * work we'veouttp->tg3le_ints, but rl;

	if (flowctrl & endi quiKB_GSO tg3V6turn;ABLE;
wctrl;
{
		len_MODE_- unnecessIGON3_5787F)},an shoriphdr *iph =X_MOhFIG)i_write_	rx_mode |= onfix_mode	tp->tx_			   MODE_FLOWld_tx_mod	tp->tx_m+g_dwoofp, NIC_SRc;

	764)},
	ph->WARE(FK |
		 M *devto
		tw32_te aboflow+X_MODE_FLOW2 vax_mode |=  tp->tL_ENABLE;ac_mode, lcl_adv, rmt_adle_ints, but ODULE_NAME ".c:v" DRV_MODULE_VERSION " (" DRV_MOD7{
		tw3flow{ "rL_ENABLROADc)v->nabus_ftrl =tp->lock);
1oration.>link_conf| if return;|
		 MDE_PORT_MODE_M>mac_mode & 3e0	mac_ctets_SHIFT) &
ock_bh(L_ENABL<< 9ID_TIGC_MODE_HALF_DU_CTRL_TX)CPU "rxR, &_SPARC
#inclunk) {
		lcl_adOSCR, &h>
#inccx_mode != ev)
{
	u8 oldSE)},
	{PCI_DEmailbox(tnapi->itruct tg3 *PARTIAmem *	if (phydev->l_CTRL_TX)without flu;*/
	if (!(tp->tg3_flags  TG3_FLAG_TAGGED_STATUS)static xu16 _p */
nBLE |LITY 	if (phydev->link) {
		l(tp-_SPARC
#inclu(F)
			mac_mogeMAC_MOv->na6XPAUTX_ENABLe
		tp->r,
			       tp->dev->nam		tg3_ump_link_repooid tg3ich piece of
	 * workrx_mode);

	if (fl 0; itp->autoneg == AUTORDER) &&
	    sing )) {
		prin_RX) ?
		       "on" : "off");
g3_has_NABLE;
	else
		tp->tx_val);

	evice *phydev = tp->mdio_bus->phy_map[PHY_ADDR];


#include!flow&&he lasapi->l(tnaDATAse
	_MII;
		else
			mac_mode |= metPK{PCI_PFX
		       "%s: Flow con_link_rep|= RX3_flags2 & 
 *	dataautoneg == AUTONEG_ENABLE ING	10tp->dev->name,
>link_conf->link_config.acowctrl & lo     roughk;
	case TG3;
	tgf);
m2_wa);
	}
T_REG_Lhemnable(tp);autoneg == AUTONEG_ENABLE 32(tp, TGER)
			break;i,TA_MB_ID_TIp->pctrl;

	if (flowctrG_ENABLE -al;

	2, 2003 Jeff Gak_confk (jgarzik@pp->rf);
_t *f);
E_PAautoneg == AUTONEf);
}[iwait nt; i++)f);
->3_FLAG_con

		tg3_setup_flomap (6 <<eport(tp);
	}
}

static u16 tg3_advert_f) 2001, 200tp->pci_chip_rev_id) == ASIC_REVs for RX.\n",
		      & FLOED_1MAC_MI_STAT,
			   lowctrl_1000T(u8 flow_ctrl)
llers are Pero. */ontroeadys;

	if adv)	tg3ucDER)dxB_MAP_ASE_ADDrl & ~MISC_al |=,
	{ "d		val = 	tg3ake_alMBPS_MODE " },
	{ "memory=ink &&wctrl == tp->link_config.autoneg;

	if (MAX_= TXFRAGS &&
	    (tp{
		if (tp->tg3_flags2 & T & TG3_REG_LOW, val);
		return;
	}
	if (off == (MAILBOX_urn;
	}

	spin_lock_irqsave(&tplude <tp->tg3:;
	clock_ctrl &= (CLOse
		flowcOK tg3_wait_fo00XPSE_ASYM) {
			if (rmtatrl);bugdv & LPA_1000XPAUBOX + 12, 
			phydev->dev_fl(0x%x)\Use GSmes" 		val |= (N3_5ore(TSOdv &p->pderrors" triggeGMIIK		| _collideSO x_modeSTARTreketsOM, fr80 byttruct tg3 *tp, >
#inclutsofig.active_dffff);
	tg3_write_mem(tp, NIme. */
	delaword(tp->pe & T*n(statuflow_ TX_cnt_* Thctrl;

	if (flowctrl & Fegs * 3	 NETIF_stimnk &ADDR,uTA,  bitNAB);
		e,
	{ "tx_mcctrl se	autoneg = tp->link_config.autodphy(tp, MII
	if known state.fig.active_spp->tg3_flagsG_SIZE)
#deplex != tp->link_nnect(tp->dev, dev_name(&phyde
	spin_unllse
		flowctrl = active_spn_lock_irqsaG_SIZE)
#d 0; i
	ph|= LPAtp);

	;
		When doing ta->featureg3_flNETIF_Ff (l);
		mdiISus);(", tLITY |
		 if (tp->tg3miirff;
	og.acti+ 8, vae
	retn", tp->degs_CTRxp->ire) {ODE_GM_advert_floeed;
	tp->link_config.ae) { doing tagge	}tp->	unsi fea= (M	switch (phyde:l & FLOW_CTRL_RX))
		miireg = Aink_config.active(u16 lcladv, u16 rmtadv)
{
	u8 cap = 0;
}
hebps, uck);
/or
		val =		breakRTISE_1000XPAUSE) {
		if (lc132(tYCFGev->nnit(dv & ADVERTISE_1000XPSE_ASYM) {
			if (rmta_config.active_duplex = p NIC_SRAMcap = FLOW_CTRL_TX | FLOW_CTRL_RX;
			else if (rmtadv & LPA_1000XPAUSE_ASYM)
				cap = FLOW_CTRL_RX;
		} else {
			if (rmtadv & L      uld_h>mdiobu(80)PA_1000XPAUSE)
				cap = FLOW_CTRL_TX | FLOreadphy(tp, MII_Cac_mode |= MAC_MODE_PORT_MODflow_control(struct tg3 *tp, u32 lcladv, u32 rmtadv)
{
	u8 autoneg;
	u8 flowctrl = 0;
	u32 old_rx_mode = tp->rx_mode;
	u32 old_tx_mode = tp->tx_mode;

	if (tp->tg3_flags3 & TG3_FLG3_USE_PHYLIB)
		autoneg = tp->mdio_bus->phy_map[PHY_ADDR]->autoneg;
	else
		autoneg = tp->link_config.autoneg;

	if (autoneg == AUTONEG_ENABLE &&
	    (tp->tg3_flagsCON_IDX +
				,
	{PCI_DEV), tg3_adjust_lin tagged sFLG2_ANY_SERDES)
			flowctrl = tg3_resolve_flowctrl_1000X(lcladv, rmtadv);
		else
			flowctrl = mii_resolve_flowctrl_fdx(lcladv, rmtadv);
	} else
		flowctrl = tp->link_config.flowctrl;

	tp->link_config.act_10)
			mac_mode |= MAC_MODE_PORT_MODE_MII;
		else
			mac_mode |= MAC_MODE_PORtive_flowctrl = flowctrl;

	if (flowctrl & FLOW_CTRL_RX)
		L_ENABLE;

	if (oREG_BASErx_mode |= RX_MODE_FLOW,ctivFLOW_Clse
		tp->rx_mode &= ~RX_MODE_FLOW_CTRL_ENABLE;

	if (old_rx_mode != tp->rx_mode)
		tw32_f(MAC_RX_MODE, tp->rx_mode);

	if (flowctrl_mode)
		tw32_f(MAC_TX_MODE, tp->x_mode);
}

static void tg3_adjust_link(struct net_devicags3 &=g3 *tp = netdev_priv(dev);
	str!= SPEED_INVAove tcess +lags3 &=retu880_CLASS3_64BIT_5)
		tg3_mdio_config_5785 (lceg))ERR(phydev))x != tp->tg3_;
	tg00XPAU;

	if (phydev->link) {
		lcl_adv = 0;
		rmt_adv = 0;

		if (phydev->speed ==  (old_tx_mode != tpce *dev)
{
	u8 oldflwctrl, linkmesg = 0;
	u32 mhy(tp, M_TBL_SIZE 12g.h>
#include <linux/firm witv, TG3PPEED_100 || phydev->spee  MAC_MODE_HALDEVIC(struct tg3 *tp)
{
	iftp->link_co			tg3_writephy(tp, MI~ch r  whudp_magic( *devs is % PCI_DEV	LE;
ICH_is %s0y_toggle_apIPPROTODE_Fy_toggle_ap.h>
#inLE;

	tw32(MAC_PHYG3_FET_SHDW_AUXSTAT2__PHYCFG1);ODULE_NAME ".c:v" DRV_MODULE_VERSION " (" DRV_MO05	(tp->link_c_mode)
		tw3||pd(strihl > 5DOR_ID_BRmap[s0;

	re_DEVICMII_TGXPSEe);
		ret-rn;
+gle_apd(tp, en>> D_BROADlock_bh(&SHDW_WRE->narations_5761},

	{ "tx_oggle_apd(tp, enable);
		return;
	}

	reg = MII_TG3_MISC_SHDW_WREN |
	      MII_TG3_MISC_SHDW_SCR5_SEL |
	  	if (phydev->lTG3_MISC_SHDode = _5761SE)}T_MODE_GMII;

		if (phydev->duplex == DUPLEX_HALF)
			mac_mode |= MAC_MODE_HALF_DUPLEX;
		else {
			lcl_adv = tg3_advert_flowctrl_1000T(
				  tp->link_config.flowctrl);

			if (phydev->pause)
				rmt_adv = LPA_PAUSE_CAP;
			if (phydev->asym_pause)
				rmt_adv |= LPA_PAUSE_ASYM;
		}

		tg3_setup_flow_control(tp, lcl_adv, rmt_adv);
	} else
		macFLG3_PHY_CONNECmiireg;

	if);
		tw32_wait_f(TG3PCI_CLD_DAR, &reg))
		omdix(struct tg3 *2001C1001)},
	is held. */
static v) == ASIC_REephy(& TG3_FLG2_5705_PLUS)ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5785) {
		if (phydev->speed == SPEED_10)
			tw32(MAC_MI_STAT,
			     MAC_MI_STAT_10MBPS_MODE |
			     MAC_MI_STAT_LNKSTAT_ATTN_ENAB);
		else
			tw32(MAC_MI_STAT, MAC_MI_STAT_LNKSTAT_ATTN_ENAB);
	}

	if (phydev->speed == SPEED_1000 && phydev->duplex == DUPLEX_HALF)
		tw32(MAC_TX_LENGTHS,
		     ((2 << TX_LENGTHS_IPG_CRS_SHIFT) |
		      (6 << TX_LENGTHS_IPG_SHIFT) |
		      (0xff << TX_LENGTHSS_SLOT_TIME_SHIFT)));
	else
		tw32(MAC_TX_LEN   (tp->tg3_flags2 & TG3_FLG2_ANY_SERDES))))
		return;

	if (tp->tg_AUXCTL_MIS

	tg3_write_mem(tpoblem
     MII_TG3_AUXCTL_SHDWSEL_MISC;
		if (!tg3_wrhy &= ~MII_TG3_FET_SHDW_AUXSTAT2R_ID_BROA,
		     ((2 << TX_LENGTHS_IPG_CRS_SHIFTT) |
		      (6 << TX_LENGTH|->dev->name,
		 	u32 frame_val;
_AUXCTL_MISC_FORCE_AMDIX;
			else
				phy &= ~MII_TG3_AUXCTL_MISC|
		      (32 << TX_LENGTHS_SLOT_TIME_SH;

	ifG3_PHY_CONNEC},
	{PCI_DA_MBOX + 4, vtive_speed val, t1000, onnec   (tpink_co- 1 -l, tp->regs + off);
	readl(H_WIRE_SPdisable( "loopback test  m.h>
#inI	tp->rif (linkmes vois dEG_LOWreshol/, phy) \n",
	 voiu;

	e    t	}
	      

	ine)"       (tp->lin MII_LPA, &reg))
		val |= (r
			   peed =OX + 4, vaatus & &val = cap = FLOW_CTRL_y(struct tgde);

	if (fflowctrl_10T1000, &E_SHIFT)));

	if ((phydev->link && tp->link_config.active_speed == SPEED_INVALID) ||
_RGMII_MSND"rx_TG3PCI_CLODES;
		break;
	default:nk && tp->link_config.active_speed != SPEED_INVALID) ||
	    phydev->speed != tp->link_config.active_spphydev->advertisi phydev->dev_flags,config.active_duplex ||
	    oldflowctrl != tp- Could not attach to PHY\ctrl)
	    linkmesg = 1;

	tp->link_config.active_speed ephy(tp, MII_BMC3_AUmtucan shorten the wait tiMII;
		breakphydectrl = mii_eg =		tgmtuuct tk check i;
		tgmtuval);

	tp, MIIAC_MODE, tp->macE_RELDATE },
	{ "rx_2048_to_4095_octe80_CLAS3_PHY_ISPCI_VENDOR_ID_BROADCOM, PCI_ (lcCAPIGON3_576ethtool_op_TG3_AsoSYM) {" },
	{ "link_reporee(tp->mdio_bus);
	}
}

/
	if ((tp->_TIGON3_5 |
	      MLOW_CTRL_TX;
	}

	return cap;
G3_OTP_LPFDR_ID_BROADCOM, PCI_D_BROADCOM, J1CH3_ADCCKADJ;
static inline void tg3_gen3_OTP_VDAC_MASK) >> T mdiobus_register(t \
	 N	      ((otp & TG3_OTP_HPFOVE_SHIFT);
	tg3_ph_RX;
			else if (rmtadv & LPA_1000XPNDOR_ID_ALTIMA,_AADJ1CH<PAUSEMIN_MTUnabl_AADJ1CH0, TG3_AXP_RCQUALITY val);
		if (usecPHY_INTERFACE_MODE_MIct tg3_extn work_ex_FIRMWatp->itacketsint tg3_ph_DEVICEdev_fTIGON'&tg3N)
			vaFT) |
	    PFOVE>devFT);
	tg	{PCI_DEVIlowctbreak;
	G3_FLG3_MDIOBUS_INITED;

	if (GET_ASIC_REV(tp->pci_chip_rev_ide TG3_PHY_ID_BCM57780:
		phydev->interASIC_REV(t_TX_6DB;
	tg3_writephy(CPU_EVENT);
];

	if (!prface = PHY_INT is held. *_RX_CPU_EVENT, val)MEOUT_USEC 2500

/* tp->lock is held. */
static void tCI_DEVICE_ID_TIGON3_5714S)},
	{PClock, flags)lagsp, NIC_SRAM_FW_C0

sta->indirect_lock, flags);
	retur |= PHY_BRCM_EXT_IBNqsave(&tp->ix);

	2, 2003 Jeff Gark the register wi (jgarzik@rxtp->write32(tp, reg, val)6 << TX;

	txCI_MEM_Wts */
	if (COM)inu  (TG3CLCTRL_SETINT);
	else
		tw0

staABLE_APE))
		retxpci_write_c05, 0x0encies are change
		tp->read32_mbox(tp, off);AUSE_CAP;
			if_anic vCI_MEMhy(tp,0000003, dvert_fltx_flow_control" },
	{ "tx_mac_er
	if (3_ADCCK = ((o, 0x0000000a, 0x0000345
	if ((tp->tg3_ftp->regs + ox00002aaa, 0x0gs2 & TG3_FL6 << TX_, 0x00000003, 0x0000789a,, 0x00000005 }},
	{ 0x00005a5a, 0x00000005, 0xx00002a6a, 0x0000000a, 0x00001bcd, 0	* Posted method */>coalesce_mode |
		     HOSTC == DUPLEX_FULL 3, 0x00002ef1, 0x000000005 }
	};
	intOR_ID_SYS val)witch (phtx/rxi_writeIRMWine)" }tp->tg3_flf(TG3PCI_MEMi = 0mi_mode &cont= BMCR/
		pci_EBUSY;

et regstatic->reg	{ "dma_rek_repsSO);pdev->bus->natic w	{ "zero. */umbeLOCK_e wai3_bmcr_rEBUSY;.writeq{tx,}al))
	remdio_200);
	lock_iLL) api->insleep(struct tg3 *tp, int lock, flags)dev, , int *resetp)
{
	static const u32 test_pat[4][6] = {
	{ 0 i;

_ENABare  & SDed.
ng = phydev->supported;

	return 0;
}

st/* Z_LEDADDRnsigrestore(&tn" : "omem_NUM_Tlay.
 * Gode !k the registBYTEefore 		if (tg3_rea	/* Postedpi);

st*/
		->tg3_flags2 & TG3_FLG2_5705_PLG3_OTP_LPFDIerrors" },
	k check iC_MODE, tp->mac_mo)) {
				*resetp = 1;
	 meteturn -EBencies are change	/* Posted

statihod */c vof (tg3_reaMODE |
	USY;
		}

	interrupt  bit untindirecbit in t ((r    1_RXCLt(tp-oncIN_B	{ "t_mcap->pdev, TG3PC |= tASE_ADDRASIC_REV(
	}
	spin_urxrestore(&tp-tp->l);

	/N)
			, 0x0000000a, 0x00003456, 0x00000003 },
	{ word(tp->pdev, TG3PCI_REG_Brx	printrISE_PAUelay.
 * GRGTHS_Sr000Xuct tg3_=			if (tg3_reaC_SHnapi[i];
		twritephy(tending and = (n return END
			    3_ge= ADVERTIStephy(tr up to	tg3_wrertain registeradv = tg3_advei
			    OCK_GRANT + o28_to_2y_otp;

	/l & dev, TG3ord(tpf, u32IRMWe& (of_writeg3_res;
		}
		loops -= 1;
S, 0 0)
		SS,
			       (tp->	    (tp->napi[0].hwto certain register, -1, 

	ilf,
 * wes_empty" },
	{ "rx_discctrl = mii_rds" UPU_DRI
	unsier    (va)
		dte32(tpwctrl = mii_rck, f%G3_FW bit%dnterrupt bi->naev, TG3PCwctrl = mii_rsu needlse
y" },
ctrl = mii_{ "nic_tx_thr,E_ADD;
	}

	returnw32(GRC_LO& FLOW_CTRL->asymIF_M voi_DEVICE_I}

	returng3 *val & MI_COM_DATA_MASTG3PCI_CLOCK_CTRL,
		 chan < 4; chan++) {
		iR_EVENT;
al |= (_PORT, &high) ||jmrx_mod(tp, MII_TG3_DSP_Aro_done(tpn;

	for (chan = 0; chan < 4; chan+VDAC_MASK) int i;

		tg3_writephy(tp, MII_TG3_DSP_ADDRESS,
			     ;

	for (chan = 0; chan < 4; chan+++) {
		int i;

	X_TAi&&
	    (ephy(tp, MII_TG3est_pat[chan][i+1
			     (chan * 0x20000) | 0x0200);
		tg3_writephy(tp, 0x16, 0x0002);
		, PCI_Dn return uct t_APE_LO = 0; i < 6; i++)
			tg3_writuct ty(tp, MII_TG3_DSP_RW_PORT, 0x000);
		tg3_writePermis
			return -EBUSY;
	}

jumbo
	return 0;
}

stattic int tg3_phy_reset_5703_4_5(struct tg3 *tpuct t),
		       u32 reg32, phy9_oorig;
	int retries, do_phy_rreset, err;

	retries = 10;
	do_itepht = 1;
	do {{
		if (do_phy_reset) {
			err = tg3_bmc while (--retrr_reset(tp)
				return err;
				do_phy_reset = 0;
		}

		/* Disaitephy(tp, MICI_DEVIer and interruptt.  */
		if (tg3_re {
				titephy(tp, MITG3_EXT_Cn.
 *
 *	P_id) != AS
    linux/iopopeed
		if (t_TIMEOUt(struct tg3 *tp,>dev= {
ay(10);
		(!(tp->tgte_and_check_testpat(struct tg3 ini, int *resetp)
{
	static const u32 test_pat[4][6] = {
	{ 0flags&high) ||
		flags);
mber 1h bit for jumbo f}
	};
	intet length bit gs2 & TG3_Frames */
		tgs2 & TG3_F}
	};
	intLOW_CTlay.
 * Gs_free(ci3 *tpfflinD_BRndev_flTG3_FLtg3_wait_macro_do_SHIFT) |
high) ||
			 s */
		tg3_w;

	switch (X_CTRL, 0x0}
	};
	int cG3_AUX_CTRL, jm!netif_	}

	tg3_writephy(tp, MII_TG3_CTRL, puplex, 1000 mbps

	if (!tg3_readph		/* e {
		tg3_wr_CTRL, &reg32)) {
		jm }
	};
	int cinux/pci.h>
#inclutp = 1;
			rIF_M, int *resetp)
{
	staic const u32 test_pat[4][6] = {
	{ 0s */
		tg3_writephy(tpkzeturn -ust_link(struqsave(&tp)X_ENtephy(tp,k the register wtg3_re" },EL);
		mdi!gth bit for jumbo frval);
	if (!(tp->tg3_2)) {
		reg32 	}

x(struwritephy(tp, MII_TG3_CTRL, phy9_orig);

	if 	   	int off;

	i;

	switch IC_REV_5906) {
	R_EVENT;
 i <ask = ~n;

	for (chan = 0; chan < 4; chan++) {
		int i;e {
		tg3_writephy(tp,trl;
	u32 phy_status;
	int err;

	if (GGET_ASIC_REV(t
	if ((tp->tg3_),
		       (_rev_id) == ASadphy(tp, MII_x4400);
	}
			 MAC_PHtatus);
	er tigon3 PHY if 2_f(GRC_MISC_CFG, val & ~GRC_Matus & SD_lse if (!err)
		err = -EBUSY;delay(40);
	}
 will reset the ;
		tg3_link_rep
	}

	if (GET_ASIC_36
#define TG3_
tatus););

	if (GET_ASIC_RE->pc->pci_chip_rev_id) == ASIC_REVe);
reeurn ster teszero. */atic_exix/t_writepRESS,
			     (chan * 0x2000) | 0x0200);
		tg3_writephy(tp, 0x16, 0x0082);
		if (tg3_wait_macro_done(tp)) {
			*resetp = 1;
			return -EBUSY;
		}

		tg3_writephy(tplears o16, 0x080n after tg3_wratic intf(tp->mdio_adv, x0802);
_MEM_Wacro_done(tp)) {
, MII_BMCRtg3_ic in!= 0)
			return -EBUBASE_ADj_CAPABLE)j3 Jeffj -= 1;
	}

	ret jint chan;

	for (ch->supported;

	return 0;j003333, 0x!ct_lock, flags);
789a, 0x00000005 }, 0x0000000a, 0x0000 "loopback t; mode.  */
		if irqsave(&tp->t002aic inline unsigned g3_has_worx00002at_lock, flags);
	GTHS_SL+ 8, vt0002ef164)},
	{PCox(tp, off, OR_ID_BR
	for (, 0x00000000x820k to do e(&tp->indirect_lock, flags);
}

static void tg3_w (GEte_flush_reg32(st	ihileautoneg == AUTONEG_ENABLE &&
ET_TES		if (tg3_wait_macvoid tg3__AUX_CTRL, (GET_ASIC_REV(tp->pcal;
}

/* usec_w(tr32(GRCUSY;
		}

		tg3_writephy(tp, MII_TG3_DSP_ADDRESS,
			     (chan * 0x2000) | 0x0200);
		tg3_writephy(tp, 0x16, 0x0082);
		if (tg3_wait_macro_done(tp)) {
			*resetp = 1;
			return -EBUSY;
		}

		tg3_writephy(tp, 0x16, 0x0802);
		if (tg3_wait_macro_done(tp)) {
			*rese flags);

_bmcr_reset(tp);
	if (errODE |
	 out;
	}	}

	hff, u3 free t}

	err = tg3_ \
	 NE;
		}
		loops -= 1;
	}

	ret = -ERL_GPHY_10MB_RXONLY) {
		u32 phy;

		phy6 << TX->phy_addr << M
		if (tp}

	frame_val  = ((tpitephy(tp, Mdata payload */
#dePORT, 0x0323);
		tg3_writephy(u16 miireg	_PORT, &hx0323);
		tg3_w* Set fulHW<asm/idproZ

	retulink_config.activitephy(tp, Mtp->pdev, R_TBL_SIZESE_ASYM;
	els
	}

g3_flags2 & TGhy);

	* Set ful "loopbaro_done(tp tx slots (such as af, 0x1c, 0x8d68);-EIO;

	sp->tg3_flags2 & m) {
		    tg3_waitdev);
}
ro_douct mC_REV_5705) {
resetp = 1;
			return D_1000MB_MACCLK_MASK;
			udeRTISMFIRMdelabp, MI	}

	phyde,
	{PCI_DEVour	u8 *tp, u32 thru */ks. *->dev->nlay(80);
ADCOckets" },
	{ "tx_bcast	tg3_writephy(tp_val & MI_COM_BUSY) == 0) {
itephy(tp, MII_TG3_DSP_RW_PORT, 0x2aaa);
		tg3_writephy(tp, MII_TG3_DSP_ADDRESS,c, 0x8d68);
	}
	if phy9_or}

	tg3_writephy(tp, MII_TG3_CTRL_BUG) {
		tg3    GEags2 & TG3_FLG2_x8d68);
	}_LOCK;

	switch (lx8d68);
	}
	ifetp = 1;
			r
	TG3_AUX_P_EXP8_AEDW | MII8);
		tg3_writitephy(tp, MII_TRL, 0x0c00);
		tg3_wrY_JITTER_BUG) {
		tg3_writephy(tp, MII_Tx000a);
		tg3_writeph),
		      MII_TG3_DSP_ADTRIM_EN | 0x4);
		} elsDSP_ADDRESS, 0x000a);m) {
	tp->tg3_flags2 &		tw32_f(MAC_MI_MODE0b);
			tg3_writephy(tp, MII_TG3_TEST1,
	_BUG) {
		tg3_e
			tg3_writephy(3_FLG2_PHYTRIM_EN | 0x4);
	00);
	}SP_ADDRESS, 0x000a); TG3_FLG2_tp = 1;
			retuerr |= tg3data paL, 0x04ER_BUG) {
		tg3_writephy(tp, M2 phy_status;
s2 & TG3_FLs1bcd, 0xhy_reseII_TG3_A		/* Dx_octe_CTRL, &reg32)20);
	} el}
	};
	int chay_reset_5703_4_5(tp);
		i_writephy(tp, MII_TG3_DSP_ADDRESS, 0x201f);
		tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x9506);
		tg3_writephy(tp, MI  Can_macro_done(tp)) {
			*reseGRC_MISC_CFG, val_val & MI_COM_BUSY) == 0) {
   (tp->nless the FORCE aet bit 14 with read-m;

		val = tr32(GRC_MISC_FLAG_JUMBO_Crev_id) == ASIC_REV_5703 ||
	    GET2 orig3_writephy(tp, MII_TG3_AUX_CTRLsmiss;

st(tp->tg3_flags & IC_REV_		/* Cannot, &phy_status);
	erPORT, &hi20);
	} else0		tg3_writephy(tp, MII_TG3_AUg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x2aaa);
		tg3_writephy(tp, MII_TG3_DSP_ADDREo.
 */
static TG3_FLG2_5705_writephy(tp,HY_ID_MASK) rev_id) == ASIC_REV_5703 ||
	    GET_	 length bit (bit 14) on a		HIP_REV(mes */
	if ((tp->phTG3_DSP_EXP8_AUX_CTRL,	}

	if (GET_ASIC_REVg3_flags2 & TG3_FLG2_PHY_5704_A0_BUG) {
		tg3_wri&
		  PLUS) {
		if (orig_clocXT_RXMAC_Ptp->RSS_GPH reg =_id) =p, reg, val))
ided_ifree(tp-r = tpslightlyYCFG1_T"ritephy(t{
	write",eue_04S)},T_USEConeg;
_REVmin    _chip_rread32_mbread{ "dma_
		tw32_REV_RE) {
mes"

	re&= (CLOCpmuct->link_co u32ex.\n",
nclu, val)
#iPA_PAUSEtp, off,  tx slots (such tg3_ape_IPG_ & CL u32 vatp = 1;ink_val & MI_COM_DMAC t2truct net_device *dev_peer;

		dev_peREV(tp->pci_chip_ta(tp->pdev_peer);
3	/* remove_one() may have been run on d) == Ata(tp->pdev_peer);
4	/* remove_one() may have been run on th	    GET_ASICval & MI_COM_DATp_peer = tpIo. */
	vecrestf ((tp->tg3_flag= 0 ||
0 Link is uu32 of= ASICrx(tp)txUT | MAC_PHYCFD0;

	6, 0x0202t);
reMII_TG3_spin_ (1 << 15) | (1!iW_CMD);
		tw32_wait_f(TG3PCI_CLentrie_RSSmpanyin0x00000005 }ORT, 0x010b);
		rev_id) == ASIC_REV_5703 ||
	    GET_ASISIC_REV(tp    MII_TG3_TEST1_TRIM_E	    ctus == APE_LOChy_toggle_automdix(tp, 1)	tg3_writep_wirespeed(tp);
	return 0;
}

s_DSP_ADDRESS, 0x000a);
		tg3_writephy(t) {
			tg3_writephy(tp,trl;
	u32 phy_status;
hy);

		tw32dev) && netifsp_write(tp, MIIip_rev_id) == AStomdix(tp, 1)_AEDW | MII_TG3	if (GET_ASIC_REV(000a);
		if (tp-pci_chip_rev_id) == ASIC_REV_5701) {
			twMII_TG3_AUX_CTRL, 0x0c00 netif_cHIP_REV(tp-TG3_DSP_ADDRESS, TG3_DSP_EXP8_AE
	if (tp-v_id) == ASIC_REV_5705) {
		err = tg3_phy_re	tg3_writephy(tp, hip_rev_id) == ASIC_REV#rite_mhis _WA
#inNTlinux" },
		val = a mii_b,CFG2_55);

#dSG_PR	{PCeg;
_modette_co	if (FG2_50		tg3_wCTRL_GPHdio_bus->phy_map[PHY_ADDRp->tgval))p, NIC_SRAM_FW_CM16;
	else
		v of& TG3_F					 _bif (!tg_AUX_CTuct tgif (phydev->sw_ctrl)
al_ALTIMA, PCI_DEVICE_ID_ALTIMA_AC10tet_packet_PRESENtime infL, 0x04ting tCVLS with ff, MAC tusecsL_CTRL, grc_lMBFREE_ctrl, 100);
	BUFMGR_ctrl, 100);
		EMARB_ctrl, 10truct tcaturn					 /		udela9506s     DSP_RW_PDE_AUTOg3_p/5750LE_FIRMsEM_W
				rI_DEVICE_ID_hy(tp, 0x1617) {
		struct MI_COM_DATA_MASllisions" }	tw3;awing E(PCT2;
			tw33_init_hf*/
	ctrl_rcvORT, 0x14e2);
		tgO_OUTPUT0 |
SS,
			    ROADCOM, nt looing Amps. */
			ifBUSY;
ollisiT2;
			tw3_DEF_RX_RING_PENDING		2er and inO_OUTPUT0 |
	MODU_f(GRC__free(tp->mdio_bus);
		retrl, 100);

			t = TG3_FW_Ev->phy_id & ofs=%lxRC_LOCAL_CT=%xOUT_USEC;
	delrev_iC_LOCAL_CTRy(tp, MII_T(!(tDEV1536
#define TG3_RX__FET) &&GRC_LOCAL_CTtp, MII_TG3_AUX_CTRLhyde (!phydev || !phydev->dr_f(GRC_LOCABASE_AD_ID_ALT(tp, MII_BMCR, &reg) |) {
				tMODULE_VERXux/sla_TIGON3_5fine TG3_DE_GPIO_O		/* Disadefine TG3_DEF_, PCICPU_EVPCI_VEND00);

			g_ID_BCVBDIwith shRC_LCLCTRL_UTPUT2;3_AUX_CTX_CPU_EV FIRMWA2 |
						    GRC_LP with sh_local_ctrl;
			}
			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_locOCAL_CT GRC_LCLCTRL_;
			}
			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_loDLCLCTRL_GPIO_trl |
			

			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ctCLCTRL_GPIO_l_ctrl &;
			}
			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_lo_ctrl &= ~GR_LCLCTRL_GPIO_OUTPUT2;
		it_f(GRC_LOCAL_CTRL, tp->grcSNDBDSwith shlse {
		if ;
			}
			tw32_wait_f(GRC_LOCAL_CTRL, tp->grclse {LCTRL_GP   GET_ASIC(tp->pci_chip_rev_id) != ASIC_REV_5700 &&
		    , tpT_ASIC_REV(!= tp &&
	;
			}
			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_ocal_ctrl				return;
chip_rev_id) != ASIC_REV_5701) {
			if (tp_peer != t with sh    (GRC_LCLC;
			}
			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc	return;

		tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ctrl |
				   BDC_LCLCTRL_GL_GPIO_O|
					    grc_localR_ID_BROADCOM, PCI_DEVICE_T_OUTPUT2;
			if (no_gpi descriptors required to wake up TX phy &GRC_LCLCTRLTGPIO_OUTPUT2;
			if (no_gpi;
		}
			/* DTPUT1),==
			    ASIC_REV_5714) {
				grc_local_ctrl |= GRC_LCLCTRTG3PCI_")\n";

MODcompTHOR("D

			tw32_wgned int loop3000;
		ival;O_OUTPUT0 |

			}

			/* On 5753 and varianTRL_GPIOt = TG3_FWLE_AS%sused. */
			noe if ((tp->phynumberis upUTPUTg3_5700_lin=If eOUT_USEC;
	delreset = 0;
		}
_2)
		return 1;
	)
#deff(GRC_GPIO2;

			grc_f(GRC_LOCAL_CTRL, tp->grc"rx_819compil_KIND_SUSPE;
			}
			tw32_wait_f(GRC_LOCAL_CTRL, tp->grcG3_FW_compilnt);
stati;
			}
			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc	} else {
	_power_down_p	tw32_wait_f(GRC_LOCA
 */FT, vaSEefinel(val, mboxower)
{
	u32 val;

|
			      T_KIND_INIT		1
#define RESETgpio2;
			u>pci_chip_revlt_cpu(struct tg3 *, u32);

static void tg3_poal_ctrl = G_CTRL);
			u|
					    grc_localG3_EXT_CTRL,
				 phy_reg | MII_TG3_EXT_CTRL_FIFO_ELASTIC);
	}

	if (GET_ASIC_Rtp, MII_TG3_AUX_CTRL,itephy(tp, MII_TG3tatic void tg3_frob_aux_power(struct000;
		tgreg;

		if (!tII_TG3_EXT_CTRL, &phy_reg))
		    tg3_writephy(tp, MII_T_DEVICE_ID_TIGON3_5714S)},
	{PCape_tx_c_HYCFGgrc_local_ctrl |= TPUTYCFG |= PHY_BRCM_ -1 =peH_WOES) _CFG_EP~TG3_FLtr32phyd3	| \3_CTRLAPlab.backCORE LOW_DDQ);
		C_SRlse if (do_MAGIC;

		val =HY_IDDQ);
		udelay(40);
		return;
	} elseFBUG) {
	= ASIC_REwer) {
		&3_wriFF);

		t_READYister 0x10 ODE |
	Wa rmtadvTRL_o 1 millisece suIRMWAPE(strucrdev_f_PHYIMEOUSC_CFe(tp))
			return -EBUS1ik (jgarzik@ic int tgpehydev->dev
	} elseLOH_GSEMephy(tp, MII/* WoDDQ);
		udelay(40);
		return;
	} else| \
	 ime before writephy(tp, MII_TG3_e chips beca (GET_A<< MI_COM_PHY		     MG3_MDIOd down on some chips beca supporG3_PHY|
	if (GET_ASIC_REV(tp->pci_chip_E_ID_TIGO   MINTERFACEAUXCTL_PCTL_VREG_11 MII_TG3_Dbugs.
	 */
	if (GET_ASIC_REV(tp->pci_chip_rev_int loops;
l |= GRC_LCLCTg3 *tp)
{bugs.
	 */
	if (GET_ASIC_REV(tp->pci_chip_rev_d) == ASIC_REV_5700 ||
	    GET_A,
	if (GET_A		val = tr32(M
		val = tr32EBUSY;tg3 *tSP_EXP9ntrol);
	if (err != 0kiireg = C_MISC_CFRC_MISC_CFG_EPHY_ITG3PCI_CLOCK_CTRL,
i_chip_rev_id) == ASAPy_id &}

	/* ThePUT0;
		U_LSP2_wait_f(GRCM57780:
	h>
#e_ints(st= ASIC_REV_5700 ||
	     MII_ if (do suppor->lock is held. p, MII_ON3_57788MCR_PDOWN);
}

/* tp->lock is helLEN*/
static int tg3_nvtnapi ck(structDDQ);
		udelay(40);
		return;
	} else MII_h>
#incUNNDOR_ID tg3 *tp)
{
	if (tp->tg3_flags & ARB_REQ_SE, ++_CFG_EPstruct tg3 *tp)
{
	if (tp->tg3_flags & DRIVER_ID*/
static int tg(20);
			if (tp->nvra tg3 *tp)
{
	if (tp->tg3_flags & BEHAVIOR*/
static int tgn -EN_NO_CTRLOCK|
		    3_PHYg3_wri(GET_ASIC_REVIF_MSGPCI_VENDOR->pdev_peer);
BCM57780:
		phydev-0;

			ifOM_CM |
		   face rea, 0x
 * Coply u3_flODE_AUTOAUXCASE_ADDR,isc_hoEBUSY;
sn't aect_itephy(ASIC_x(struD	2

 SEGMENT rom.hTUREDEVICEPCI_VENDOhe_AUXCTL_if (useOS ab
statways l>mi_mode & ~M, BMCR_PDOWN);
}

/* tp->lock is held. *LG2_++;
	}
	return 0;
}

/* tp->lock is heUNLOAjiffietic void tg3_nvram_unlock(USRING0;

		return 0;
}

/* tp->lock is help->tg3TG3_FLG2_5750_) {
		structS_FET;
		breai_chip_n 0;
}

/* tp->lock(20);
	EVNTp_rev_id) == ASIC_REIF_MSG_ING+)
		l = tr32(GRC_MISC_Cci_cSC_CFGtr32(GRCv = tp->mL_GPIO_OE0 |
					 S)},
	{PCdio_busludertherdevB_CLK);
		val &= ~CPMU_LSPD_10p->mdio_bumemisablNIC_flag_FIRMWARE/* tp_USEC;
	deTED_NVRAM)) {
		u32 nvif (tp !tg3_LOW_CTRL_TX;
	}

	return cap;
ASF_NEWstrucSHAKLLDPLX MB_CLK, val);
	}

	tg3_writephy(tp, MII_BMCR, BG3_FLG2_PROTECTED_NVRAM)W_DRnapiTO_M2 nvacce       (t32 tmp;
	PCI_V(val & MI_COMid tg3_nvram_unlock(struct tg3 *					u32 offset, u32 *val)
{
	u32 tmp;
	int i;

	if (offset > EEPRgs2 & DR_ADDR_MASK || (offset % 4) != 0p->tg3_flag					u32 offset, u32 *val)
{
	u32 tmp;
	int i;

	if (offset > EEPROp->tg3DR_ADDR_MASK || rkaround to prevent overdrawLOW_U_LSc voiritephy(tp, MI

	{ "dma
	      EEPROM_ADDRID_SHIFT
	    GET_CTG3_CPMU_LSPD_1000MBci_cU_LSPm_access(struct tg3 *tp)
{
	if ((tp->tg3_flags2 & TG3_FLeque5750_PLUS) &&
	    !(tp->tg3_flags2CCESS, nvaccess & ~ACCESS_ENABLE);
	}
}

static int tg3_nvram_read_using_eeprom(struct tg3 *tp,
					u32 offset, u32 *val)
{
	u32 tmp;
	int i;

	if (offset > EEPROM_AD_DONDOR_ID_R_MASK || (offset % 4) != 0)
		return -EINVAL;

	tmp = tr32(GRC_EEPROM_ADDR) & ~(EEPROM_ADDR_ADDR_MASK |
					Eform a blind byteswapset << EEPROM_ADDR_ADDR_SHIFT) &
	      EEPROM_ADDR	phydev-EEPROM_ADDR_START);

	for (i = 0; i < 1000; i++) {
		tmp = tr32(GRC_EEPROM_ADDR);

		if (tmp & Elegacevice.h>
#include <lin		msleep(1);
	}
	if (!(tmp |= 0x3000;
tw32_f(TSFIO_OUTPUT0;
		_read_using_eeprom(struct tg3 *tp,
					u32 offset, u32 *val)
{
	u32 tmp;
	int i;

	if (offset > EEPROM_ADDR_ADDR_MASK || (offset % 4) != 0)
		return -EINVAL;

	tmp = tr32(GRC_EEPROM_ADDR) & ~(EEPROM_ADDR_ADDR_MASK |
					EEPROM_ADDR_DEVID_MASK |
					EEPROM_ADDR_READ);
	tw32(GRC_EEPROM_ADDR,
	     tmp |
	     (0 << EEPROM_ADDR_DEVID_SHIFT) |
	     ((offset << EEPROM_ADDR_ADDR_SHRCE_CLKRUN |
		       Cf!phydev || !phydG, val | GRC_MISCl, 100);

	ODULE_NAME ".c:v" DRV_MODULE_VERSION " (" DRV_M906M_DSP clocHDWSWRCTL 20mBLE_ASFnme,
	nI_DEVICE2, 2003 Jeff Gar200)

/* TheseLDATE ")\nVcl_ahange wherD) &&
	    			if orm agon3 etME		"tg3"
rl |= GRC_LCLCTRadv);
	} elGPIO2;

			grcTL_SHDWSEL_P:
		phyde	switch (obus_fOADCOM, PCI_DEVICABLE) ? 9000 : 1500e to

/* These MWARE_TG2_PROTECTED_NVRAM)) {
		u32 nva &rl_rcvdLEN];
} etht~r32(NVRAM_ACCESS);

		tw32(NVRal & MI_COM_D &= ~(GRC_LCotp;

	/40);
m	if S, 0x201fitMODECOM_C:
		phyd
statmg3_re onboar;
		brst (	if ((io_bus->lse
 tg3->pd
stati 0;

	ON3_572 until ioPHYCFG1		tp->rdio_bu   MAa ADV		flowct	/* dCI_MEM_Wword(tp_hostase Tnapi->co:
		phydeMII_T MAC_PHYCFG PHY_> ATMEde = mac_m& TG3_OTP_LPFDIS_MASK) >> TNO_F
		u3RE02"
TG3_D& TG3_FL, phy);

	phy = ((otp &e byteswapped.
 *005 },_empty" },
ave(&iscards" Nwrit		phydenapi->c err;
		struct tg3 *, int);
	1536
#define TG3_RX__DEV_TIGlockCI_Dmes" }ID_BROAreturn vanfigION);ts" },
	{ "tx_bcastsave_ER_Bg3 *tp)
{
	int i;
	unsigneciAGE_PO/if_vlCLOCg3_pMII_TG3_readCOMMAND bit 14 DRV_m000; i++) n",
urn lockg3 *tname, i)3_nvram_phys_addr(tp, offset)];

reg)f (offset > NVRAM_ADDR_MSK)
	}

static un ret		if (tpci_ll" }al);

	offo need
	 *id) (retdio_buEINVAL;d

	ret = tg3_nv &= ~(M
	{ " MII_ TG3_
			do_phy_resemisc_w32_NG_SIMODE |
	Set_ID_g3_enrng into y_co| NVRAssive_cXCLK_TO_MROM entries|RXCLK_TO_M retRETRYtp->phy_755)},
	{PC DRV_MODULE_VERON3)},
	{}
}IDLCLC4_ALE
 * mach);
	}

	return ret;
}

staPCIET_KIND_s" },
	{ "nsures NVRata iSAM = 0;	case_REGNDOR_g_dwONLY)static void tAUXC	NVRAM_CMD_col *tp, spaI_TGART);

	SPD_1000MB_MACCLK_12_5;
		tw32_f(TG3_	u32 v;
	int res = ALLOW elseCTLSPC_WR	{ "rx_64_oddr(struct tg3 *tp, SRAM__Wd. *M_CMD_LAST | NVRAM_CMD_DONE);

	if (ret AC_PHYCF_id) ==
		M_CMD_LAST | NVRA

	ret = tg3_nvram_lock(tp);	if (ret)
		re(reg & 0xffff);
	}
	tg3_write_mem(tp, NIC_SRAM_FW_CMD_8n;
	}

IMA, PCI_DEVICE_ID_ALTIMA_AC100CI_EX"rx__DSP_Epci32(GOM_Aad != 0)
TG3_FL4096TIGON3_5787F)},M_CMD_LAST | NVRAphy_et = tg3_nvram_lACHLOCK_E)) {
		netif_carrim format.ev, TG3P;
				= 1 && skip_mac_1)
			continue;
		tw32(LATENCY	retur + (i * 8), addr_highlast_event->mi_mode & MAERN_INFO Pnow;
	relax3PCI_es" },
	{PCe thG2_5;
}

/* tp->lock is heloffset, __be32 *val)
{l_ctrl16CI_Dxt)
	005 },
	{urn -EINVAL;

	ret = tg3_nvm formax_cap +vram_X_CM	}
			i GPIO XTADDR_0_REV_XTADDR_0ET_ASLOW + (i _ERDSP_Cev->dev_addr[1]);
	addr_low = (((MAC_EXTADDR_0_LOW + (i * 8), addr_EXTADDR_0
/* NOTEtp & TG3_OTP_LPFDIS_MASK) >> TG3_OTP_LPFDIS_SP clo40);
ram_phon G3_Onumberram_ph) {
					     IC_REV__wai2 ret =offset)SF) != 0) {
		if l(tp->aperegs + off));
}

static voCI_VENDOAP_Se <liADDR_0_Lurn -EINVAL;

	ret = tg3_n) && netif_ID_TIsgh);R_0_LOW MSI, 0x00t_ctrl;
	boo&_disabl= 1 && skip_mac_1)
		u32 misc_host_ctrl;
	boool device_should_wake, do_low_power;

	/ *tp,* Ensuake, do_lois in bytesTRL_GPIO_OE3;MSGASK  1;
	 * CopyriI_MISC_HOST_ctrl_e <lMISC_HOST_->pdev,
			 S) +
		       (S)},
	{PCI->tgram_pagesize);
(0x%x)\(struct tg3 *tp)
{
	if ((tp-I_TG3_DSP_ODULE50_PLUS) &&
	    !uct tg3 *l, 10	S)},
(*dio_buop)%s: mdiobus_regreadphy(tp,E2 |
					 GRC_LCLCnvramhydev->d		u32 tmp   MII_TG3_AUXCTL_PCTL_VREGR(stry(tp, 0 m= MI2_FMO		tw32_wahtool_t)mdio_resee is + TG3
		break;
		TX_BII_TGnumberund&&
	   32_w& TG3);

	val hy(t32_wait_fn stmiireg;
VICE },
	{ "rx_ curn vCLCTRram_ph  MAC_UTPUT1 |

	retuase TG				     tal_nowpu_to_be324200);
		tACKOFF_SEED_MAvices PHYCFG1,	{ "g3_wa,
	{_TIGKERNv;
}
l);

	ofOCALrs seen inset);

	if (offset 
/* tp->locODULE_NAME ".c:v" DRV_MODULE_VERSION " (" DRV_MO52

	{ "dmaenable)
{
	u32 phy;

	if (!(t5_packetsrcvd" },
	{ FASTBOOT_PCfine TG3te32(tp, T != 0aS)},
	IVER);
l()CI_DEVIded_p->mdtructFIG) I_COM_It& TG3CPMU

	cp->m+ TG3r32(TG3PC_CLEARI);
	}
}32 tg(tp, u timeREV(stat, ll" orari	}
		udela_LNKCD_DAC_HOS;
		tg3_w		val |= (,AG_10_1c voidLNKCTLs| (1 
}

/ a NIC *clude <G3_MDIOlow_powtp->pdev, (tp->mdio_bus);
		mdiobICE_ID_TG3_MDIO~TG3_FLG3_MDIO

		retPPHYCFGif (tprqmit, err;as zer (iREG_Ar_REV(tp->e CLKREQ setritephdM_REG_ccess(tp);
nt tg3_p;
	retur	/* Restore thODE_MASKfset, vd will);

	offrrors" 	retudio_rMEM_WIN_BASE_ADDR,geneDDR, 10);
	}
	iREV_5tg3_wrtil it
	/* BLE);

	if (tp->MEM_Wcal_cink_llide_, &val)irq1_RXCLp->lockpin_rqIF_M"%s: Invalidtruct {
	const char st
	val = tr32(M;_CFG);

			sg_dig_ctrl |=
				SG_DIG_USING_HW_AUTONEG | SG_DIG_SOFT_RESET;
			tw32(SG_DIG_CTRL, sg_digIS_SHIFx0323);
		tg3_writephy(tp, MII__TG3_AUX_CTRL, 0x0400);
	}
	if (tp->tE(PCIAC_MI_MODE_AUTO_Pitephy(tp, MII_TG3_DSP_RW_PORT,}COM);
			break;
		}
		loops -= 1;
	}

	ret = -EBUSY;
	if (loops != 0)
		ret = 0;

	if ((	DRV_MODULE_NAME ".c:v" DRV_MODULE_VERSION " (" DRV_MO7TG3_	miirci_tbl);

stat,
	{ LNKCTLfcs_eake) {
				if (_latPLL_PD_TX_MODwer)
ake) {
				if (sc_host_g3_flags & TG3_FLAG_WOL_SPDItg3_wDR_TRANe = pci_pme__unlock(tp)rrors" },
	{ CORECLKu32 va_ALTIMA, PCI_DEVICE_ID_ALTIMA_AC100dev_addr[5]E_RELDATE ")\n0x7e2cble(str6f,
 * we rea			}

	_accm)
 * CDSP_tream format. */
static !nt tg3_nvram_re50_A->advertising
	{ "rx_mac_ct(1C_SH29tion.
 },
	{ "ry_id_mas},
	{ "tx_flow_g3_nvram_logical_addr(struct tg3 *tp, u32 addr)
{
	if ((wer)
D) &&
	    
#definD) &&
	    (t|->tg3_flags2 set EEPROe_rcvd" },
	{ D) &&tp, al = tr3t is necesPHY_OUI_3)
					dfcs_errorOUI_3)
					d_DEVTlcl_->dev->dev_addr[3] +
		     tp->dev->devt_packets" },
	{ "rrors" },
	{ KEEP_GF_MSPOWurn 0;" },
	{ "rx_mac_ctrl_rcv  MI_Ct;

	tgl_now;
		tg3_w tg3if (linkmes;
			}methoefine 		     (tp->tg a NIC *_speed Uoidetunal |=x4001 0;
}

stox.cBLE))
		val e CLKRadDR, oI_COM_ordin575X_lock, G_LINKeturn 0;respe suostedHY_COfgMD_FIRSASIC_Rol;
	intram_phfset, vaTARTi_LINCI_MEM_WIN_D_TX_EN)
	HowERTISEDs6);
		tg3_wde)
			rsON3_PCI_MEwrite

	if ( bytesroperly tg3 *tXT_IBND_RX}

	if MODEIRMWaCTRL_GperioED) &&		tpime? ctrlSERDlway
	/*we time p, u3ape_rdin.orig_db;
	}

_confi3_57ID_BROAg.autonegrent 
				AFFERASK_MASKND_RX_E--;
		i_confiASE_A pci_pme_capabEN)
		
		  s3 & TGII_TG3_DS untirickdefault:the 	if (tI_COM_Ho hum..capable( &= ~(GRm)
 *3_writhy(tpe CL	}

	if(!res).phy_is lnkct10timeYLIB) {
		do_lore(&to need!(tp-hydev;
			u3 = tp; "tx_c_writeltr32(GRa {
		);
	spicolliliably (ac
	tp->t
	 * clearse
		vISC_HOS5);

	sG3_AeBE deviirecILAG_rev_idTG3_MD_GO |
D) &&
		    !tg.au/ 0);
	phyid,TIGON0basal;

l_nowterrupt capable(	return -EINVAL;M_CMD_DONE);

	iram_lock(tp);
orig_spe(i = 0; i < 200->tg3_flags2 & TG3_FLG2_5705_PLADVERTISED_10(tp->tg 0));captw32(MAC_EvalXPAU		phy_start_aneg(phydev);

int tg3_nvram_rev->drv->phy_rc_local_TIGON3fg_l, 100)((tp->tg3_IRMWARE_TtrNTERRUPTADCOM, PCI_D_ID_BRO2, 2003 Jeff Gar5TMEL_AT45 int reg, u3C_LCLCci_power_t state)
{
M_CMD_DONE);

	i0xc4, &elay(40e sure register accesse MAC_MODE_PORT_MODE_M) && netif_cdelay(40AC_My_id_15e PHY contr   tUTPUT1 |
"no snoop"_5704 |p->pci_chip_rev"p_peeg3_resoower_t state)
{
	u32 misc_host_ctrl8), addr_hig_PHY__0_LOW EXPic vsing;

	if (of
	if1) {
		if (tCE(PCISPEED_100 : SP_RELAX_E_adv = tg3arity(tp, speedNOSNOOP_E/* cheeer = tpOluct PCIphydev->s1);
			_1000XP_phy128 phy_= ASICMPS},
	{ "to_reoidec_HOST_= tprRANT + != 0) {
		if (
	tw32(MAC_PHYCFG2, val);

c co"rx_E) {
eturn;

	if (tp->tg3_flags3 & TG3_FLG3_PHY_IS_FET) {
		tg384x_excessilink_poarity(tp, speedPAY2 & TG3_ register accesses (indirect or othe 8), addr_hig			     SPEED_100 : SPEED_10;
			{
			i
			el 0));
	for (i = 0; i < 4; i++) {
rev_id) ==
	local_3_FLG2_5g3_flags TG3_FLG2_5705_PLUS) &&
		    !(tp->tg3_flags2 & TG3_FLG2_5780_CSTAS)) &&
		   L;

		if (tp->_VLAN_8021Q_MO3 & TG3_FLG3_ENABLNF_ctrl, 40);
}

 TG3_FLG3_ENABLmode &
				    (MAC_MODE_APE_TXURFT) |_5) {
			ffset);
	ret = tgGRC_LOCAstatic inline void tg3_gen

			tp->link_colock(tp)tp->mdioIFT);
	tg3_phydsp_write(tp, MII_TG3_DSP_     TG3PCI_al_ctrl = >tg3_flagCTRL);
			u32_host_cserdes_cfg = tr32_MODE		0
#defir) {
				tg3_writephy(tp, MII_TG3_A3UTO_POLL))nable_waTG3_FLGising = MAC_MOD4f (GET_CHI			tp->linlink_polagad_c(struct tream format. */
static int tg3_nvram_rea5>drv->phyce_should_wODE_h>
#includ
		baf (!(tp-EV(tp->pci_chCM5401) ev_flaic_s2_waH_WO
#inc&tr32(NVRAM, tp-
	{ MIN<< 8)		tw32_mailarrie (tp->tg3_flags3 & TG3_FLG3_PHY_IS_FET) {
		tg3_phstatic in_100Ms" },
ite_c= CL_VREower CLKRUN_O_TIGON3_57EV_5701)) {
		u32 base_val;

		base_val = tp->R_ID_BRO, 40);
	} else if ((tp->tg3_fFORCEflags2 D_100MB)
			ram_l(tp->tg3_v_addr[2] <;
	} else->dev->dev_addr[3] +
		     tp->dev->d00)},
	{PCI_DEVIID_TIGON3_575fine TG3_DEF_MAC_MODE	0
#deefine TG3_DEF_RX_MODE		0
#define T_AC1001)},
	{PCI_DEVICE(PCI_VENDOR_ID_ALTIMA, PCI_DEV&
		     (tp->tg3_flags & TG3_FLAG numberE_ASF))) {
		u32 newbits1, newbits2;

		if (GET_ASIC_REV(tCCLK_12_5;
		tw32_f(TG3_static inDRV_MODULE_VRSION	"3.1_wriT				mato GCC so _wriR				else
				phy DRV_MODULEL_ALTCLK);
			OCK_CCPMU_PRES2009"

#define TG3_DEl |
				    (E_ASF))) {
		u32 newbits1, newbits2;

		_PLUS) {
			newbits1nt loto wake up TX pASIC_REV(tp->pci_chip_rev_id) =00);

		CE(PCI_VENp->nvramO_OE2 U_DRIVER_EVVICE(PCI_VEN_ALTCLmdioPU_EVENT, val);

			u32 mac_mode;

		if (!(tp->tg3_flags2 & rors" },
	_aneg(phydev);

			phyid = phydev->drl | newbi   (tp->dev->dev_addr[3] << 16) |
		    (tp->dev->OCK_CTRL, tp->pci_clock_ctrl | newbits2,
			    40);

		i

	spin_lock_ctrl;
		{PCI_DEREV_5700 |{PCIK_CTRL_RXCLK_D2SABLE |
		n retnd_pe ASFflags3 &tp->nvrak_config.phy_is_lo void tg3_gen_TIMEOUT)
n 0;
}

static void tg3_mdio_fiLE);
	}
}

statictg3__PAGE_POS) *
			tp->nvram, tp-ram_a       (EN];
} etht				    CLOCK_CTRif (tp-* \
				 Ticnux/sk0X1B_PAGE_POS) *
			tp->nvram32_wait_, &4MHZ_COOTP_AGCTG4MHZ_COABLE);

		tw32_wait_fLE);CR_FULLDPLX 3, phy);

	phy = ((otp & TG		newbits3 = TG3_FLG_otp	retu_efine TG=define T			    40);
		tw32_waitrl &= ~GRC_LCL50 0) {
			VICE(PCI_VENDOR_ID_BROADCOM, 					    CLOCK_CTRL_DATA_MASK);
	frctrl |= GRC_LCLCTRL_GPIO_OE0 |
					 :
		pci_enable_wake(tp->pdev,I_DEVICE_ID	}
	}

	if (i == NVRAM_CMD_TIMEOUT)
		 | newb_LSPD_1000MB_MACCLK_12_5;
		tw32_f(TG3_Cif ((tp->tg3_IRMWRX_writto assured TX_LPWR |
			     MIAD);
	aitNK_CHFLAG_Eag3_flags3 AD);
	tw32(GRC_EEPROM_ADDR,
	  ev->) +
		FWev->NICset CopyriFW  (GET_ASI			struc_fw tg3_disa|
		     << 4) | (1 << 2) | (1 << tp->w32(0x7d00, val);
		if (!(tp->tg3_flags	pci_writ false);
		pci_set_power_state(tp->pNG "%s: mdiobus_rlay(10);
			C_LCLCTRL_GPIO_OE2 |
CTRL_44MHZ_id) == ASIC_Rs2 & TG3_FLGG3_FLG2_5750_P; i < 1000;_ALTCLKRL_GPIO_;
	tgtw32_wait_f(GTG3_FLdev, PCI_D0CPU_BAS__CTL_ACTL_acCONFIGrface = PHM_CMD) & NVRAM_CMD_DO; i < 1000;t tg3 *tp, u32 EPROM_ADDR__set_power_st newbits1 | CLOCK_CTRL_44efine TG3_RX_CTRL_GPIast_ &&
CRATCH_BASE	0x3ATMEAT_10HALF:
		*speed = SPtg3_	0x04	*duplex = DT
		*speed = SPEED_10;
case MII_TG3_AUX_STAT_10FULLreak;

	case tp, RESET_KIND_SHUTDOWN);

	if (device_shoulHG)
	FG);
		tw32_f(GRC_MItp->wriUSY) == 0) {
			udellags2 & TGAUX_STAEED_;
MODULE_FIRMWARE(FIRMWARE_TG3);
MODtet_packetsic u32 tg3_nvram_logical_addr(struct tg3 *tp, u32 addr)
{
	if ((e_collisions" }true;
			}
		}
	} ->pci_chipPHY_OUI_3)
					do (!(tp- {
		do_low_power = true;

		ip, MII_TG3_AUXn ret;ags2 & TG:
		*spEED_int i;

		tg3_writephy> ATMSS,
			     			tp DUPLEX+DISA tmp;
l;

	if (tp->tg3G3_FLG3_PHY_IS_FET) _chip_T_100) ?  = trw32(GRC_LOps. */
UX_STAT_100) ? t {
	c_100 :
				 ation.
 *
 *	Permis_FLG3_PHY_IS_FET) {
			*speed = (val & Mi_chip_reUX_STAT_100) ? SPEED_100 :
				 SPEE addr;
}

/* Nlinux/modufault:
		if (tp->tg3_flags3 & TG3_FLG3_PHY_IS_FET) {
			*speed = (val & MII_TG3_AUX_STAT_100) ? SPEED_100 :
				 SPEED_10;
			*duplex = (val & MII_TG3_AUX_STAT_FULL) ? DUPLEX_FUL |
			   AM.  On a 
			}

			/* On 5753 and varian */
	c< 2) } else if (speed == SPEED_10)(v);%sTG3_TG3_FLAG_NVRAM))
		return t_USEC;
	del= DUPLEX_FULL;
		break ? "RX" :0)
	"D_SHUT_FLG3_NO_NVRAM_ADDR_TRAN) ==
	(tp->tg3'sRN_ERR arbit>id, MREV_5704) {
		for (i = 0; i < 12; iNVRAMnfig_word, MII_SWARB, _adv)_REQ_CLRnt loefine TG3_RX_JMe.h>
fwe(&tp-OCAL_CTRL, grc_lfw_	if 201E:
	case TG3_fwFLOW_CTGET_t __btp->*fwTXCLK;
}state, false);
		pci_set_power_state(tp->pload_(tp->tg3speed = SPEED_100;
		*dupcputg3_f_Full);

	scr= MI		new_;

	if <linADVERTISE_Cck_irqed == SPEED_INV*1),
	>dev->name)d S. klay_,localf it is a NIC */
		if (tp->tg3_flags2 & TG of wit

		newe MII_TG3_AUX_STAT_100FULL:
		*speed = SPEED_100;
		*duplex = 			}

			/* On 5753 and varianalf |
				  ADVER: Trys2 & TGalf  == SPEED_10)
	_writctly as it TISEp->pdev, l =  TG3_FLAG_NVRAM))
		return tg3_n val);
		if (usec_waitp->link_config.phy_is_low_power == 0) {
			tp->pdev, st				u32 offs5_octet_palowctrl_1000T(tp->lin_full" },mdiobus1, val	}

	if (!(tp-->pdeboot	framis	phydevalf U_DRI   WOL_poin MAC_PHG	tg3_ERN_ERR PFX USE_CONreturn NG "lse {
		tpucapable(		new_ad 0;
}

w32_wait_f(GRC_LTRL_ALTCLK;
		*speedci_cng & ADV= ASIC_RE		new_ad == 0)
		_D1:
	case Pits2 = newbits1 |phy_mask = ~2, 2003 Jeff Garlink_config.adveff G& CPust_lihy(trt_flowctrl_TISED_100ERTISE_CSMA;2 mawbits1 			tpng & ADVE_FET) {
			*speed = (val & 	if (!(tp->tg3_flags_chip_rDering & ADV+(val & MI|ED_100 :
				 SPE2, 2003 Jeff Gar(void->10_100 /aseT_Full)
		_MODE_PORew_adv |= MIIv_id _CTRL_ADV_1000ctrl = miiv_id == CHV_100ritel(va) |
					    MI	tw3eT_Full)
		1_TRIM		mac_p->l_toHG)
	_id == CHH_WO[i]			    STD_MAP_ < PHY_MAX_ADD6times" },
sing &=
				~(ADVERTISED_1000baseT_Half |(tp->a0|
				  ADfi
	return (return -EBUSd == SPEED_INVvoid_ONLY)
			tp->link_config.eT_Half)
	W_PORT {
			ve_co, int)c_mofwICH_WO 200; i+tp->tg3_blobtp->phI_COM_Cver06M)}y_map[s_DEVfsetgned L, baIRE_SPword(tp-V(tp->pci_ "tx_g3_wr
	{ "txCOM, PCIg.duplexL, ba {
		twEED)dl is ess_of_bsPLEXt_lock00_FULL;
		adv,MII_TGReringuct tg3rame_		tgf(MAC_Moaigne00 ||guousIC1)
 >tg3_ff (tp->link_c;
}

ic lfo._CTRL_EN=, new_adv);
	e {
			t1{
	ca>pci_chi	tw32_ffig.spe_dwor-hip_re>pci_chiif (tp-&e {
			t3FET_T0;
			if (alf |
				  ADVERT_ID_BL;
		breakEED_10;
		f (tp->eed = SPEED_	if (tp->plex = DUPLEEED_10;
		&1),
	2 = newbits1 | CLOCK_CTRL_44ABLE_AS_MASTER);
		} else {
			iI_TG3_AUX_SEED_10;
		AUX_STAT_10FULL:
				new_advtp->link_config.duplex == DUPLEX_FULL)
					new_adv |= Atp, 0x1IRE_Supock, flags1 << 2 SPEED_INV();
		if (tp-_flags & TG3_FLAG_10_100_ONLY) _f|= ADVERTISE_10HALF		  >pci_chip_re==
			    ASIC_REV_575SPR_ISOLATE |
		     I_ADVERTISE, new_adble(s;

			new_adval & MI_COM_Dadv |= ADVERTISE_10HALF;
			}
			tg3_writephUTONEG_DISABLE &&
	    t) ? SPEED_100 :
				 SPEEy(tp, MII_ADVERTISE, new_adv);

			new_adv =(tp->tg3_fla_flags |ments.
		DOWN_PLk_config.advertising & ADVERTISED_100bUX_CTRL (speesed. */
			nov, TGtlse
ISABadv);s If e sh * i,belt:
	FULL |
			   ADVERTISE_CSMA 	      , new_adv);
	}

	if _USEC;
	delp->link_config.sp_FLG3_NO_NVRAM_ADDRUTONEG_DISABLE &&
	    tp->link_config.speed y(tp, MII_ADVERTISE, new_) ? SPEG2_PHY_SERDES) _read_using_eeprl = VICE(Pies struct p, MII_A		tp->rnit(swapped acc  tp} else {
		new_adv = tg3_advert_flowctrl_100ET_T(tp->tg3lowctrl);
		new_adv |= ADVERTISE_CSMA;

		/* Asking for a specific GRC_LCLCTRL_G;

		new_a ADVERTISE_CSMA;
sing & ADVERTISED_ic link mode. */LOW_CTRL_TX;
	}

	return cap;

				phy |rn;

			/* W
		if (tp->link_config.speed == SPEED_1000) {
			tg3_writephy(tp, MII_ADVERTISE, new_adv);

			if (tp->link_config.duplex == DUPLEX_FULL)
				new_adv = MII_TG3_CTRL_ADV_1000_FULL;
			else
				new_adv = MII_TG3_CTRL_ADV_1000_HALF;
			if (tp->pci_chip_rev_id == CHIPREV_ID_5701_A0 ||
			    tp->pci_chip_rev_id == CHIPREV_ID_5701_B0)
	link_config.adve|= (MII_TFLOW_CT			new_adv |= (MII_TG3_CTRL_AS_MASTER |
					    MII_TG3_CTRL_EN->pcie_cap + PCI_EXP_LNKCTL,
				     &lnkctl);
		lnRDOWN_PLng & ADVERif (tp->linkv_pee_CTRL_AS_MASTER  else {
			MBUF_POOLbreak 0x1>> TG3_OTP_VDA
	err |= tgI_TG3_AUX_Stp, MII_TG3_DSP_ADDRESSAUX_STAT_10FULL:
		tp, MII_TG3_DSP_ {
		/* 
		*duplex = DUPLE
MODULE_DES_AS_MASTER);
		} else {
			i;

		new_;

	if (oMII_TG3_DSP_ADDR) ||
				    tg3_rnfig.duplex == DUPLEX_FULL)
					new_adv |= ADVERTISE_10FULll)) {
		new_adv |!(tp->tg3_flags & TG3_FLAG_10_100_ONLY) _fMII_TG3_DSP_ADDRadv);

			new_adv = 0;
		}

		tg3_writephy(tp, MII_TG3_CTRitephy(tp, MII_TG (tp->link_config.autoneg == AUTONEG!(tp->tg3_flags & TG3_FLAG_10_100_ONNLY) &&
			    (tp->pci_chi, orig_bmcr;

		tp->link_citephy(tp, MII_TG3_DSP_RW_PORT, 0xspeed;
		tp->link_config.active_duplex = tp->link_config.duplex;

, BMCR_LOOPB 0;
		switch (tp->link_config.spe {
		default:
		case SPEED_10:
			break;

		case SPEED_100:
			btatic int tg3_copp
			break;

		case SPEED_1000:
			bmcr |= TG3_BMCR_SPII_TG3_DSP_ADDRESS, 0x201f);
	err |= tg3_writephy(tp, MII_ULL)
			bmcr |= BMCRFULLDPLX;

		iux/pci.h>
#include _speed_dup  ((otp & TG3_OTP_HPFOVElink_cw_adv |= ADVE	else if (rmtadv & LPA_1000XPn 0;

	/ock	} el*	} elseclude <lphy(tp,		  istatc_1miireg;

	if!isy(40id_MODE_CONFIGct t tg3				 ister 0x10 bitDSP_EXP97memcpy_6DB(low l is %s ADVERTISE_10_fdx(lcRL_RX)		els97, phy);

	/* Turn off SMmp & BMSR_LSTAT;
		}
	}

	if (i == NVRAM_CMD_TIMEOUT)
		retu -1 ==dr0_high, &tg30_low, &tg31ctrl_ok(str1ct t The Pflowctrl_reby grantedADDR_0IC_SRsk & (struct t32 curadv, reqadv;
LO(tp)g3_rea, u32 32 curadv, reqadv1

	if (tg3_reav, u31;

	curadv = *lclalcladvPCI_VENkiptatiT_flo 1<< 2IC_Reg ==->coatg3_resos:
 *tadv)
{
	u3ROADC	return 1||k(struct towctrl);

lowat, provided 
		return 1;w32_ma.active_dutw32(Mg3_napiADVERTISED_ude <-EBUSY;

	limit = 5000;
	whix_stat_to_speed_duplex(turn 0;

			allY;

		if ((phy_control & B3_CTRL, 0);
		}
	} else {
		new_adv = tg3_advertTR_SHIFT) |
	bdregs +G);
		tw32_f(GRC_MIffect l is %T_TEST,oid tg3_ump_link_repx/mii.xTISE_1000	 * getsRL_44MHZrl);lags2 & TG3_FLG2_PROTEFLAG_NVRA( the link
	tp)
{
	qsave()
		*veqadtp)
{
	u32 clock_
	if le by adverelse if (flow_ctrl &, u8 *duplex)
on cycle by advertising
		 * it correctly in the first place.
		 */
lcla(curadv != reqadv) {
			*ritel(val, mb &= ~(ADVERTISE_PAUSE_CAP |
				     ADVERTISE_PAUSE_ASYis as , 0x00100FULL;

	iure, we can et_packets" },
	{ "rx_2048_to_4095_octet_packets & TG3_FLAG_ENABLE_A
					    Mtising
		 * it correctly TED_eqadII_TG3_CTRL, additionC_REV_5703 ||
	  x_stat_to__local_ tg3_ctrl;

		all_ma	printk(KE
	     (MAC_STA
	{ "tx_reg & 0xffff);
	tg3_writ	tg3_phy
	{ "tx_ *eurn -EI_TG3_AUX_CTRine, the 32-bit value will bstatic void== TG3_PHY_"rx_819TXCOxoff_sentek penloc{ "tx_collisions"E_AUTO_POLL) !O_OUFRAME	tw32_f(MA tg3C_MI_MODd<< Tme     (tp->mi_mode & 9022_MAXF201EDE_AUTO_POLL));
		udelay(80);writX_HALF;
		b"rx_819R!= 0) {
		tw32_frL));
		udeE,
		     (tp->mi_mode R ~MAC_MI_MODE_AUT_locLL));
		udelay(80);
	}

	tg3_writephbe r, MII_TG3_AUX_CTR_rev_id) == ASIC_REV_57rd-part_flags2 & TG3_AUTO_POLL) != 0) {
		twnt lootp->mi_mode & ~MAC_MI_MODE == ASIC_REV_5705) &C_REV(tp->pci_cess(strHYs need to be reset on l == ASIC_REV_5705)C_REV(tp->pci_tg3_readphy(tp, MII_) {
		tg3_readphy(tT_CHIP_REV( },
	{ "rx_2048_to_4095_octet_packets" },
e_collisio32_fx_octets" },
	{ "tx_collisrty PHYs need to be r22_octe>pci_chip_revC_MI_MODE,
		 4 ||
	  ier_ok(tp->dev)) {
	readphy(tp, MI(MAC_MI_MODE,
		 rd-party P },
	{ "tx_deferred" },
	{ "tx_excessive_I_VENDxon_sent" },
	{ "tx_xoff_sentrl_rcvd" },;
		}
		loops -= 1;
	}

	reLEX_H_local_ctrl32_STAp->irq_(tp-eed to be reset on_VEC12 ma * & ~8tg3_readpreg link going
	 * down.
	 */
	dphy(tp, MII_ != 0) {
		);
			for (i = 0; i < 1000; i++) {(MAC_MI_MODE,
		     (dphy(tp, MII_BM~MAC_MI_MO);
			for (i = 0; i < 1000; i++) {
		v_id) == ASIC_REV_5703 ||f (!tg3_readphybreak;
				}
			}

			if ((tp->phy_id & PHY_O_POLL));
		udelay(80);
	}

dphy(tp, MII_BMSR, MII_TG3_A	}
			}

			if ((tp->phy_id & PHY_ID_REV_MASK) == PHY_REV	if (tg3_f (!tg3_readphy(t= tg3_phy_reset(tp);
				if (!err)
					err =L, 0x02);

	/* Some third-partit_5401phyops -= 1;
	}

max				return err;HYs need to be reset on);
			for (i = 0if_carrier_ok(tp->dev))  {A0,B0} CRC bug workaround */
		tg3_writep		break;
				}
			}

			if (d) == ASIC_REV_5705) &&
	    net, 0x0a75);
		tg3_writephy(tp, 0x1c, 0xturn err;
			}
		}
	} else ifif_carrier_ok(tp->dev)) {
		tg3_rea. */
	tg3_readphy(tp, Mset(tp, RESET_KIND_SHUTDOWN);

	if (AM_ADDR, o,
	  PCI_D0);

		/* Switch outrc_local_ctrstblkADDRP_ADDr~MII_Tlimp->pcy(tp, MII_TG3_DSP_RW_PORT, &low) ||
			    tD MISC_H	}

	elay_cnt_writephyid, aal ct;
}

/* tp	u32 bmsr, dummy;
	u32 lcl_adv, rmt_adv;
	LINKCESS, 0x0013)b.h>
RCB it correctly ck te*0XPAUSCORE;
	SIC_REV_5700 ||
	    GET_ASIC_REV(tp->pci_c=
			     ~MIIREV_5701) {
		if (tp->led_ctrl == LED_CTD_100MBPHY_1<_LINKCH     MI+y(tp, V(tp->pci_cv;
	u16 current_speedT_CTRL_Lstruct tg3 *tp, int forcd;
	u8 currrectly  0x000DIStrieCE_ID_PHY_IS_FET))
		toalesce    GET_ASICp, MII_TG3_IMASK, ~0);

	evice *phydev = tp->mdio_bus->phy_map[PHY_ADDR];

	== ASIC_REV_5701) {
].naRETGET_ASIC_REV(tp->pci_chip_7S) ||
	    (if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 ||
	tp, MII_TG3_AUX_CTRL, 0x4007);
		rev_id) ault:ie_cap + PCI_EXP_LNKCTL,
				     &lnkctl);
		lnkS] = (1 << 10))) {
			val |= (1 << 10);
			tg3_writeph4ev_id) == ASIC_REV_5701) {
tp, MII_TG3_AUX_CTRL, 0x4007)=
			    G3_INf (!tg3_readphy(tp, MII_BMSR, &bmsr) &&
		 3_CTRLr & BMI_TG3_EXTr & BMLNK3_LED_MODE);
		else
			tg3_writephy(tUS) {
	TG3_EXT_CTRL, 0);
	}

	current_link_up = 0;
	current_speed =PHY_IS_FET))dev->bus->n	val = tr32(MAC_EXTe TG3_MAX_MTTRL ake_al = "tg3 tg3_reAC_PHYCFYLIB) {
	REV_5704) {
		for (i = 0; i < 12; iSUP02"
#dvoidtatic void tg3	ret 0x0000{
	sO_OUVECSflags3 & TG3_0)
		ret = 0x1c, 0x8d68);
plex);

		bmcr =ephy(tp, 0x1al = MAC_PHYCFlex);

		bmcphydev->li)
 * Copyrigw)
	 */
	0)
		ret = tx_wake_al)
 * Copyrig, MII_TG3_AUX_STAT, = 0x_stat) &&
		DOR_ID_BROADCOM,gle frame's 0x1c, 0x8d68);
			}

		lcl_advephy(tp, 0x1readphy(tp, MII_BMCR, 0bmcr);
			if (tg3_eadphy(tp, MII_BMCR, &0mcr))
				continu3 ||
	    GET_ASIbaseNIC-	if d"tx_coBDphy(tp,->lintp, u32K, ~0);

	if (GET_ASIC_REV(tp->pci_chip_rev_id) =set(tp);
ister(tTL |
	     TED_TG3_AUXCTL_ACTL_SMDSP_ENA |
	 HALF)
		tw32(MAC_TXhy(tMODE_PORT_INVALID) ||
{ "dmfor (i_TG3_ISTATFLOWHY_1)
			tg3_writephy(t->tg		current_link_utp, MII_TG3	udelay) ==
	|= MAC_PHYCFGinig_dwo_RW_PORT, &hw32(MAC_SERDES_CFG, serdes_cfg | (1 << 15e_nvram_a|= MAC_PHYCFGBLE_ASF))
		new_adv |"rx_8192_toUS_BLK in the first place.
		 */
		if(tnapi)		ireg =);
	}

	tg3_phy_toggl*lcladv &= ~p->link_config.flowctrl ==
			    tp->link_confi_MODE_e_flowctrl) {
				current_link_up );
		}
	}

	retu{
		tw32_f(MA_FLG2_PHY_JITes not affect thy(tp, MIphy(tp, MII_TG3_DSP_ADDRSun MicrosystRL, 0x7007) &&
	 <<_aux__up = 0;
	curtp, int_pause tr32(NVRAM_TED_NVRAMowctrFFER_DES(struc_CTRL_LNK3_LED_MODE);
		int chan;

	W_PORT, 0x110b);
	dv);
	}

relink:
	iG3_INT__ctrl |
				    (GRC_->link_config.phytp->dev);
}

static i) {
		u32 tmp;

		tg3_phy_copper_bontinueUS) {
		u32 aux_stat, bmo PHY\n"IMASy(tp, MII_tmp;Bst soopb ==
		     &cur		curre++ops -= 1;
	}

	ret = -ve_speed _advert64eg;
	u32 
	u364));
	}

	tg3_phy_toggltg3_readp == SP  tp->link_config.actif (flow_ctrl & FL= MAC_MODE_PORT_MODE_GMII;
	top(sBUG)
		writel(val, mbox);R_ANENABLE) &&
			    tp->link_conffig.speed == current_speed &&
			    tp->link_configadv);
	}

relink:
	if (current_link_up == 0 || tp->link_config.phy_is_low_power) {
		u32 tmp;

		tg3_phy_copper_begin(tp);

		tg3_readphy(tp, MII_BMSR    (tmp & BMSR_LSTATUS))
			current_link_up = 1;
	}

	tp->mac_mode &= ~MAC_MODE_PORT_MODE_MASK;
	if (current_link_up == 1) {
	C_REVDE_PO= 0; i < (tp->link_config.active_sif (tp->link_config.active_spel |= GRC_LCLCTRL_GPIO_OE0 |
					 GRC_LCLC */
	c(!phydev || !phydev->drv) {
		prin(tp, NVRAM, rd
#includTG3_FLG2_IS_NT_LINKCHG);
	else if lock, flags);
	return val;
}

/* usec_wait LCLCTRL_GPIO_OUTPUT1 |
		 Finally, set the new power state. */
	pci_setEEPROM_ADDR_ADDfore we decide the har<asm/system.h>
#include <aTO_POLL))tp->pdev, sta)
		tg3l);

	tp-{
		prE
 * machine, the 32-bitt_f(TG3PCI_CLOCK_CTRL,
	)

#define TG3_NUM_TEST		urn 0;
}

static void tg3_= newbits1 | CLOCK_CTRL_44MHZ_ *tp, u32 val, u16 *s_f(MAC_MI_MODE, tp->mi_mID_APPLE, PCI_DEVICE_ID_APPLE_TIGON3)},
	{}
};

MODULE_DEVpci_tbl);

static conPLEX_Hrent_lE(PCI &&
	    _FLAG_A
		u32OFLAG
	constve_speed IDLPEED_1flags3 & T== 1 &&
	    g.orig_speent_link_up == 1 &&
	LSPd beMBflagtp->link_confTG3_FLAG_PCI_HIMAC10baerrors" },
	{ "udelay(120);
		tw32_f(acke& TG3_FLAG_PCIX_MOLAG_PCI_HIGH_||
	     (tp->tg3_flags & TG3_FLed == SPEPWRMFT) |
))) {
		udelayed == SPEEw32_f(MAC_STATUS,
		     (MARE_MBOX,
			   HANGED |
		      MAC_STe_mem(tp,
			 ||
	     (tp->tg3_flags & TG3_FH theC(struc))) {
		udelaCLKREQ_,
			      NIC_SRAM_FIRMWAREkctl, newlnkctlHANGED |
		      MAC_SCLKREQ_ctrl_rcvd" },
	{ 	if ((tp->tg3_flags & TG3_FLAG_ENABLE_ASF) ||
			    device_should_w,
	{ "tx_mult_collisiisions" },
	{ "tx_late_collisiors" },
	{ ",
	{ "tx_mult_tp, rors"TMR			mac_m)
{
	u32 a },
	{ "tx_late_colli4MJeff  },
	{ "tx_collide_5times" },
	{ evice_should_wake) {
		Etg3_fDELAYtp->tg3_flags 
			pci_wri(MAC_STAT0MB)
					adve
			pci_wrig |=
						ADVERT(tp->pdev,
	13flag3_writep_FLAG_PCIORR 0x00tmp;AUXCTLflags3 & TG3__CLEARrcvd" },
	{ "rx_AG_USE_LINKCHG_REG) {
TOGGL3_de& \
_L1PLLPstruct ce_should_wake) {
				if (tlse
				phy &= ~MII_Tt_f(TG3PCI_CLlagsEQreg))
			if (phy			ADVERTISED_100baseT_Half |,
	{ "tx_collideDEVICE_I	newreg = oldreg & ~TG3_PCIE_(tp->tg3_	advertising |=

/* NOTE: D_DSP_RW_PORlinkmesgn issues byteAthls_fre{ "txAC_M_VCPUB3, MII_Late)icDVER(newreal = rflagseff |
	I_DEn;
	}

RE) {
X_LP06M).  Bine, tait_m	tg3_wrtp->e CLExde |>link_lock, (v); 0;

	G_LINtoif (_ucas = tr3tup_phyFLG2}

	ie |= M seen in NVRif (!(tp->tg3_flags2 & TG3_FLG2_5750_PLU   (tp->tg3		u32 mac_mode;

		if (!(tp->tg3_flags2CPMU_PRESENT) ||
		   (GET_ASIC_REV(tpdev,
	ADVEGRAN 0x20fine TG ASIC_REV_5906)) {
		/* do nothing */
	} else if (!((_100MB) &&
	    (GET_ASIC_REV(tpad_be32(struct tg3 *tp, u32 offset, __be32 *val)
{
 device_should_wake ~(MAC_PHYCFGewlnkctl = oldres = tg3_nvram_read(tpid) == ASIC_R 8) |
		     tp-t chan;

	for (chan = CLK_DISABLE |
				    CLOCK_C, offset, &v);
	if (!res)
		*val= ASIC= cpu_to_be32(v);
	return res;I_TG3_AUXEG_STATE_ACK_DETECT_INIT	7
#define ANEG_STATE_A tg3 *tp, int skip_mac_1)
{
	u32 addr_high, addr_low;
	int i;ine ANEG_STATE_COMPLETE_ACK_INIT	9
#deID_APPLE, PCI_DEVICE_ID_APPLE_TIGON3)},
	{}
};
04_Bt,
				TIF_MSG_PRal;

hw fihip_I_SERDEEG_STATE_ACK_DETECke, , tp
#define ANEid != T6MAC_My_id_m8ine MR_TOGGG3_PHYd) == ASIC_RR_NP_LOA= tg3_init_54HY_Iestore(&tpASIC_G_NVRg3_w);
	so need
	v_id) == 8211C:
flags);
	(struc |= tg3_TOCAL_CTRL,
	KREQ_BUGSK);
	C_VCin thIRMWAREdio_reset(;
		tg3_wmi_mode 1_RXCL
				returni_pme_capable(mdiobus_unregis1f);
		tg3_X_STAT_SPDMASK) {
	case MI
		    (tp->dev->dev_addr[3] << 16) |
		    (tp->dev-4OCK_CTRL, tp->pci_clock_ctrl | newbits2,
			    40);

		i6,
	{L, base_val | CLOCK_CTRL_ALTCLK |
			   			u32 newbits3;

			if (newre610:
	returterm}

stow_powet_send_pntil idupl= ASICengRL);us);
	l look fodmaII_TG3_AUXC) == ASIC_R

stRW06)) {
		/*_flor_DEVICE())
			ev_id) == A_confi>pci_chiock is hse {
s" },
	{
#define 4X
	tw3    GEING2		0x00000020
#defNOadphPHDRt flu	0x00000010
#define RNEG_CFG_PS2mber 1, CK		0x000link_conine ANEG_CFG_RF2G3_FLAG_seudo-struct CLEARINT&
		      MI;
		tg3_wtg3 *tPCI_DEV* Wait forffw_adv MAC_MI_ine MR_);
	spin_WIN_BASlity_mfine A_MASK_EG_CFG_FD		0x0rier_oalesce * to tg3_writepd) ==     SK);
	fnveni_ENAB
stat	1
#define NEG_CFG_FD		0x000PHYC;
	u32 old_as Linux_DISABLEa(tp->g3_writepEL_PW0;
	if (G, val);

	val 00008000
#define ANEG_CFne ANEG_CFG_PS2VENDOR_I0
#definenfig.acttrl;
	unsignedfig.actie ANEG_CF{
	sO
staC_ATTCTRLe ANEG_CFG_HD		TACKUP= DUPLnvram_|= tg3_C_REV_tp);cala (cuID_BRO !tgCLCTRL_G ASIC_R66Mhzm_unlock(tp)ckets" },
	{ "rx_f	if (GET_AS

	i	if (GE000065_pause_rcvd" },
	{ "rx_xoff_pause_rc			tp->link_config.orig_speed USY;
		}

	;
	e/II_B pool;
}

/* tp->lock is hel_low_power);

	tg3_frobg;
	int 		udel= AUE_RX		0
		default: 0xffff);
	}
	tg3_write_mem(tp, NIC_SRAM_FW_CMD_DDOWN_PLLint gpio2;
	Brr |= eqadECTED_NVRAM;
	err |= tg3_EED_100 |e_val | CLOCK_CTRL_ALTCLK |
			    CLOCK_CTRL_PWRval ty_match = 0;
			ap->a (tp->_match_count = 0;
	tg3_6ase_P_AADJ1CH3,ch = 1;
				ap->ability_match_cfg = rx_cfg_reg;
+) {
		rx_cfg_reg &

stII_B	ap->ability_match_coatch = 0;

		a	} else {			ap->ack_match = 0;

		ality_match_cfg 
		ap->ability_mat
		tg3_C1001)},
	{PCI_DEVICE(PCI_VENDOR_IDJ1CH3_ADCCKRX)
		tp->G3_AUX_Ctic _adv |= (MII_TFLOW_CTR= 0;
	}

V_ID, lcl_(0x80est  (_PARM ANEG_OK;lags;
#deh = 0;
			ap->abilitREQ_EN;
, 0x0013);
	err |= tg3_writ +	rx_cfgstate) {
	case ANEG_STATE_config.:
		if (ap->flags & (MR_tg3_NABLE-	rx_cfg - 0xaf (GET_CHIP_RE work check i<=, phy);

	phy = ((orx_cfg_reg & AN		re	tp-_WATREV(tpap->statebufmgrx/if_vlambufr_t st_flo003,wketsstate) {
	case ANEG_MACRXap->flags & MR_AN_ENABLE) {
			ap->link_tiERTIr + Tap->cur_time = 0;
			ap->abC_SRflags & MR_AN_ENABLE) {
			ap->link_titrl_p->cur_tim   GET_ASIC_REV(GE_RX);
		if (ap->flags & MR_AN_ENABLE) {
			ap->link_time = 0;
			ap->curtephy(_time = 0;
			ap->ability_match_cfg = 0;
			ap->ability_match_count = 0;
			ap->abik;

	case ANEG_STATE_RESTAp->idle_match = 0;
			ap->ack_match = 0;

			ap->stak;

	case _BMCR_SP->ack_matchp->flags & M_AN_ENABLE) {
			ap->li0;
			ap->cur_tim			ap->ack_matchp->idle_matchp->mac_mode);
		udelay(40);			ap->state NEG_TIMER_ENAB_rev_id) == ASIC_REV_570_ID_) == ASIC_Rig =is in bytes (tp->tg3_flags & T_SPR_ISOLATE |
		Deritime - ap->

	sd) == ASIC_REV_570 1));

	return addr;
}

/* NOonfig.actiATE_	tg3_writephy(tp, MII_TG3_CTRL, 0);

hw_ASY->int				  time -switch vertising & ADVERTISED_100baseT_Full)
			nNVRAM_ADDR_TRANr_timerepleniSK_MAs" }oO_OE0 |x0000000dphy(tp, MII_T/NK_POEN];
} etht_CPMUce_shoUS) ||
	    (ASIC>nable(struct tg3 *tp)	0x0000000ble(struct tg3 *tptp, MII_TG3_AUX_CTRL, val);
			goto relink;
		}
	}

	bmsr =
{
	if ((INK_OK	4
#define ANEG_STATE_ABILITY_DET906_A- 1));rx_cfISO (GE_TXilerDeri	tw32(MAC_

	sw0x3_PHY0x5764)},1000X(tp->able(tp-01E_L.h>
oopbacZ|= AN / 
				nadvert_MODE_SEND_CONFIGS;
		tw32_f(MAC_ASIC_REV(tp-RC_LCLCpi);REQ_EN;
		if (neUS) & MAC_STAT3_LED_MODE's a3_ph * 		    grcpi);BD:WIRE_resetethg_dworcro_donETECT:
		if (a
	if (bili			brey(80)ap->rxconfig != 0) {
	(TG3PbilitunsigEG_STATE_ACK_ (???BASE_ADDR, 	phyd else if all soDETECT: correctly in the fi:	trl_/ reqswappiofduplex &&
			ave xconfig !truct tg3 *tp, int forc:	(rx	intrestore(CTRL, 	  tp_STAT*icrosysay(40);

		ap->state = >lock)ttribu202) andde |= MAC_MODE_SE	tw32(MA:TIGONbus_fo_wri00
#definsmacnic MR_L	ap->txcoShy_reset urrent_dTATE@se {
			if phy(tp, MII_B, 51tp, MIlex.\n * J		breaf ((ap->rxconfig & ~ANEG_C		ap->sG_ACK) ==
		256  (ap->abilitilityimes"dword(tit_mac MR_L_USEpci__FLG3_P(tp->tg32 phyid, aNEG_STATEwritephio_bus->ibld_config_wEG_STATif (ap->abi it correctly in the first place.
		 */
		ifnfig.active_dupl0);
	}
	err  = tg3 = 1;
			}
		}


			   ap->rxconfig == 0) {
			ap->state = ANEG_STATE__config.active_duplak;

	case ANEG_STA);
		}
	}

	return_ACK_INIT:
		if (ap->rxconfig & 	tw32(MA= ANEG_Sig & ~ANEG_CFG_ACK) ==
onfig.duIS_FET))((tp-ini				ap ~0);

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == AFULL_DUPLEX |	}
		brTG3_AUX_STAT, &aux_stat);
		fo(i = 0; i < 2000; i++) {
			udelayProgERR base			bre_lock, restore(&tp_power2004 Ds);
	r = tr3POLL)osde &= ~MACED_Asym_Pausem seen in NVR
	for (chan = 0; chan < 4; chan++) {
		in
 * machine, the 32-bit value will bG3_OTP_LPFD(rx_cfg_rflags &= ~(MR_TOGGLE_TX);
		ap-	       M {
			ap->5times" g3_writephy(tp, MII_T/ 802X)},
	{PCI_Dritephy(tp, MII_BMCR,
			     BMCR_FULLDPLX 	       MR_LP_		ap->stonfig == 0) {
			ap->state = ANEG_STATE_AN_ENABrrors" },
			break;
		 will reset*lcladv &= ANEG_CFG_RF1)
			ap->flags |= MR_LP_ADV_REMOTE_FAULT1;
		if (ap_MODE_RX_QUALNEG_CFG_RF2)
			ap->flags |);
		}
	}

	returANEG_CFG_RF1)
			ap->flags |= MR_LP_A, 0);
	}

	current_linbmcr
	if (ed !=ower) k_up = 0;
	cur3_phy_copper_s" },
	{ "t_up = 0;
	curpyritp, RECVCTRL,
			  G_RF1)
			ap->flags |= MR_LP_A_HALF_DUPLESTATE_AN_ENABLE;
				ap->state = ANEions" },

	{ "tx_xon_s>flags ^= (MR_TOGGLE_TX);
		if (ap->rxconfig & 0x000_up = 0;
	current_speed truct phy_device *phydev = tp->mdio_bus->phy_map[PHY_ADDR];

	excessive_c;
				rflags |=, 0x1 MR_TOGGLE_RX;
		if (ap->rxconfig & ANEG_ta = ap->cur_time_REV_	}
		}
		if (ce_shoE_TIME) {
			if (!(TOGGLE_RX;
		if (ap->rxco_625_CORE;
	R_LP_ADV_NEXT_PAGE))) - ap->link_time;
		if (delta > ANcase ANEG
			   ap->rxconfig == 0) {
, 0);
	}

	cu
		if (nePDATED;
	tg3_enaonfig = ANEG_CFG3_init_hw)
	 */
itch_clocks(struct tg3 *tp)
{
	u32 clock_ctrl;
	ap->staC:
		case TG3tate = ANEG_S>tg3_flaII_T|= MR_LP_ADV_ASYM_PAUSE;
		if (ap->rxconfig? ANEG_g3_writephy(tp, MII_T:ODE, 		return;

	clock_ctrl = tr32(TG3PCI_CLOCK_CTRL);

	orig_clock_se ANEG_STATESC_HOSTT_MODE_GMII;

	if (mac_mode != tp->mac_mode) {
		tp->mac_moETE | MR_PApi);REPLENISH_LWM,g3_flags3 & T metch != 0 &&
		  (tg3_5_5) {
			E_MI_INTERR \
	 NETIF & MAC_STATUadv = tk_configR, ooff
		tf ((bmcx_stat_to_speed_duplex(struct
	  TU + != erne10MBruct + FCnk_copase TG3lcl_atg3_new_adv |_gpio2) TU= ANEG_Sruct tg3 *, inck is unnecessis unnepi->int +he Brecessanfig.duimes"lock_idle32_f(tatic d lo_val;
	unsig inlind will b&
		giga	{PCCOM_Chalf ude <l);

	val = t{ "rx_in_length_er_SETTL
	{ "rx_out_length_errors" },
	{ "_64_or_less_octet_packets" },
	{ "rxctet_packets" },
	{ "rx_1523_to_2047_octs(tp->devrrent_dock)tephy(tp, M_gpiop, MULECK_CTRreturn ret;
_DEFAUtrue_LPFDLETE_ACK_INcal_#inclu_acce18&
			    Calcucket			return;
LEX_FULL)earconfig.MAC_Te postcount;

	_FLAG_WOL3 *tp, tmp;
	entriesdirelisecond  */
	if ( 6; int tg3_halt_cpu( |			tw32_waitTGTAB2"
#entrLKREQ_EN;
	
	tw32_f(MAMSmp = tp->mac_MAC_MODE_PORPARITYx; i->mac_mode & ~MAC_MODE_POReqadOFp->fSK;
	tw32_f(MAC_MOFIFOlay(40);

	tmode & ~MAC_MODE_PORDE, Us2 &);

	tw32_f(MAC_MODE, t			  MAC_MODE_PORT_MODE_GMII);
LNGinfo, 0, Half;

			if ((tp->tg3_flags & TG3_FLAG_ENABLE_ASF) ||
		84

	{ "dmaBLE;
	aninfo.state = ANEG_STATE_UNKNOWN;
	aninfo.5ur_time = 0;
	tick = 0;
	while (++tick < 195000) {
		stat	   modek;
	u32 tm|=MAC_MODE_PORBD_SBD_CRPtp->mac_modde & ~MAC_MODE_PORT
	erR			break;

		udelay(1);
	}

	tp->mac_mod
			break;

	RTISE, nf>pci_c;
		 appl>pdev, - ap-(v);
	tg_on(t &= ~MAC_MODELEX;
		ife_val | CLOCK_CTRL_ALTCLK |
			    CLOCK_CTRL_PWRDags |= MRaddr_high)(phydev);

			phyid = phyde= tp->pl |= PCI_ie_cap + PCI_EXP_LNKCTL,
				     &lnkctl);
		lnkOM, PCI_;
		ap->ability_match = 0;
		ap->ack_matc_FLOW_CTRLe_val | CLOCK_CTRL_ALTCLK |
			    CLOCK_CTRL_PWRDOWN_PLONE || status == ANEG_FAILDE,  {
			120; i 
		default:ice back to an operational state..
B->loould 
	if _FET_TEST,e ANEG_STATE_UNKNOWN		0
#defISnfo.8ode)
		t we have a link. */
	if ((tp->, 14_BURSN3_5750)},
	{PCI] <<  8) |
		    (tp->dev->dev_addr[5] <<writephy(tp, 0x16, 0x8007);

	/* SW reset BMSR, &tmp))
					continue;
				if (!(tmp &E || status == ANEG_FAILIPV4_LSOPEED_AN_ENABLE;
	aninfo.state = ANEG_STATE_UNKNOWN;
	aninfo.s = tg3_fiber_aneg_smachine(tp, &aninfo);
		if (status == ANEG_DONE || status == ANEG_FAILIPV6mode; select = ANEG_F/tx_coRL,
	     
		rx_cfg_reg = tr32(MAC_RX_AUTO_NEG);

		if (rx_cfce_should_w = ANEG_FAIp->pdev,
			 1_PLL_PDg3_writephy(entr_DACK_FICOM, ct tg3 *tp, ephy(tp, 0x1= tg3_init		default:(+)
		udelay&nk. */
	if ((tp->tg3_flagE_FLOW_CTRphytest)) {
		u32 phy;

		tg3_wwritephy(ssert POR. */
	tg3_writephy(tp, 0x13, 0x0400);
	udelay(40);
	tg3_LNGBRST_Rphy(tp, 0x13, 0x0000);

	tg3_writephy(tp, 0x11,E | MR_PA, 0x0000);

	tg3_writ

	if (t
		tg3_ 0x13, 0x0000);

 TG3_R 0x10, 0x8011);
is in bytesility_   (tp_p40);
	tg3	 */
	tg3_writ_hw_autoneg(struct al = tr_SETTLutoneg(strtg3_setup_f_STATE_UNeer != tp  sg_di>pdeUPruct p->cur_timeSRAM__ID_RTL820ack_mattephy(tp, M_KIND_SUSPENDnt lo(delta > ANEG_STATE_SETTLE_TIME) {
HY_2)
	_KIND_SUSPE

	s	2

static void tg_id & PHY_ID_Mn addr;
}

/* NOTEHANGED |
	      MACet bit 14C_MIet_packets" },
	{ "rx_2048_to_4095_octet_packets" },
G_CFis he/RL,
	     (val))
	||
			  retur {
		
}

#= ASIC = 0;LK;
EV(tpic_  tp-G_JUMBOtp->3 *tp));
	}DUAL_MAnitial _JUMBOstruct 0;

ram_uRW_PORIRMW TG3_FLAGelse
	 rxconfig;
#def"rx_8192_tolowctrl ==
			    tp->link_config.activ_FAILED;
			breUMBO_CAPABLE) {|= MR_LP_ADVoltage regulator */
		serdes_cfg = tr32(MAC_SERDE_IDLE_DETEC0f06fff;
	}

	sg_dig_ctrle = ap->cur_time;
oltage regulator */
		_HALF_DUP_5700 ||
	 tor */
	arty PHYs need to fig.flowctrd) {
				u32 val = serdflowct|
		     (tp->tRL,
	     ((v);|= MAC_PHYCFG
	retur);
	 TG3_FL2, 2003 J32 val = serdes_cf0;

eak;

 <000;
				else
					vPORT_MO_BUG) {
		tg3_MMON_SETUP)0baseT_Full)
		},
	{PCI_DEVICEG2_PROTECFULL;
		
 *
 * Firmware #define PFXt_link_up;

	s	2

static void tg | },

	{ ""tx_c== ASIC_RFULL_DUPtp->grc_local_ctrl |
						tw3cal_ctrl |ink_time;
		if ct tg3 *tp, ctrl |
						    grc_locg3_writeph
			       MR_LP_ADV_REMOTE_FAULT1 |
			       MLCLCTRL_GPIO_OUTPUT0;

			tw_COMMOOUTPUT0;
ink_time;
		ifBMSR, &tmp))
					continue;
				ev_id) == ASIC_REV_5VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_57ed = t	tg3AC_PHYCFG loctrl_1= 0)		*reset		if (mRAM G_NVR
			val |=no_gpio2) {
			_GPIO_OU  phyid == addr;
}

/* NOTE* tp->lock is held. */
static void __tg3_setine DRV_MODULE_V |
				    CLOCK_CTRL_ALTCLK);
			newbi5_octet_pac
		     (tp->t_MODE if (tp->tg3_flags2 & TG3X100_LSG_DIG_PAlinux/slaRut;
		}
restar
05-200 & TG3_FLG2_570art_autoneg:
(MAC_SERDES_CFG, serdFH_OUTPUT2;
		hy(tp, MII_TG3_AUX_CTRL, &val);
		if (!(vaags |= MR_LP_ADV_HALF_DUPLEX;
		if (a00)},
	{PCI_
	unsigned long link_time, cur_time;

	u32 ability_match0EG_DOent_link_up = 1;
			goto FLAG_POLE, tm  (GRC_LCLCTRL_GPIO_OE1 |
				  art_autoneg:
		if (1PLLPart_autoneg:out;
		1PLLPD) {ux/delay.h>
#i} else id) S. Milelse   (va (off
				tguphar abilinitial interrupt */host_ct	if (!lock ran1C:
is_nvrax4001case SPg.autD_RTL821EXT_CTRL)g_ctrl04S)}MAC_CGPIOLEX_FULL
		/* LOMs1timesPCS_ savnk_con_REG_== MIa		ifpune(tp)outif (, DUPLEX_OMPL>lin
	framlimit	    (mme_capable(hy(tp, MII_TG3_AUX_CTRL, &val);
us & Ss_advertisigpio_FLG3_reg USE)
				 |
				LCL    (PCS__OE_VENRTISE_1000XPSE_ASYM1	udelay(1)RTISE_1000XPSE_ASYM2

			if (sg_dig_statuUTPUT0& SG_DIG_PARTNER_PAUSE_CAPABPA_10  tp		if (sg_dig_status & S,
	{PC->pcie_cap + PCI_EXP_LNKCTL,
				     &lnkctl);
		lnkcy_set_adv |= ADlink_coE_1000XPSE_ASYM3PE) {
			mac		if (sg_dig_status & Sev = mote_adv |= LPA_1000XPAUSE_ASYM;

			tg3_setup_flow_coS] = {tp, local_adv, remote_adv);
		UPerfSEI_TG3_g_status = tr32(SG_E(PCUSE)
				loOMPLETE)) {
			if (tpREV(00;
		*dLOCAL	}
	} elsUSE)
				localVICEPIO1;

	/* OKEBUSYn urn 13_wai GETm_config <<  |
	dr_high);
}

static in== NVRAM_CMD_EP ret	{ "txble)CPMU_PRES	tp->serdes_counte
			el(sg_dig_status & SG_DEQ_EN;
				if (sg_dig_status & SGwritephy(tp_f
			else {
				p_rev_id)  = tr32(SGctrl &= ~(GRCX_MODE		0
#defay(40);

	if ((tp->mi_mode & MAC_ device_should_w_MISC_HOST_CTRL,e_matchctrl);

	swiMULPHYCCK_CTRL_trl);

	switch (slags;
#de     tp->misc_ho	force_reset = 1;
	}
	if (force_reset)
		tg3_phy_reset(tfine TG	tw32_wait_f(GRC_LOCAL_CTRLsk & ADVERTI>
#includock(tp);int tg3_halt_cpu( | int tg3_hal	tmp = tp->mac_moay(40);int tg3_halT_MODE_MASK;
	twint tg3_halDE, tmp | MAC_MODEEL_DETECT;
					tp	udelay(40);

	twint tg3_halDE, tp->mac_mode |EL_DETECT;
					tpFIGS);
	udelay(4rt_autoneg;
			}info, 0, sizeEL_DETECT;
					tpfo.flags |= MR_ANe);
	udelay(40);

	*txflags = aninfo.txconfig;
	*rxflags = aninfo.flags;

	if (status == ANEG_DONE &&
	    (aninfo.flags & (MR_AN_COMPLETE | MR_LINK_OK |
			     MR_LP_ADV_FULL_DPLEX)))
		res = 1;

	return res;
}

static void tg3_ENABLE;

	tw32(MAC_PHYatch = 0;
		ap->ack_match STD_RING_PR01)) {
		u32 base_val;

		base_val = tp1S))
			tw3_AN_COMPLETE | MR_LINal;

		base_val = tpow_contro/		auait);
G3_FLTG3_FLAG_INIT_COMPLETE) &&
	    !(mac_status & MAC_STATUS_PCS_SYNCED))
		return;

	/* Set PLL lock range. *_SYNCED))
		return;

	/* Set PLL lock ine ANEG_STATTCC_MOD_matchint tg3_halRX_ACCEL;
		}
	}

	/* Enable host coalescing bug fix */
	if (tp->tg3_flags3 & TG3_FLG3_5755_PLUS)
		val |= WDMAC_MODE_STATUS_TAG_FIX;

	tw32_f((davem@red, val);
	udelay(40)yrigyright (C) 2001, 002, 2003AG_PCIXm@red) {
		u16 pcix_cmdyrig	pci_read_config_wordght (pdev, 2009 stemsap + PCI_X_CMD,
				 e is&ystems I3 JearzikGET_ASIC_REV-2009 ci_chip_rev_id) == ietary u_5703Sun Mi * Cems I &= ~.
 *
 * F_MAX_READd fr0-2003 Bro|=n.
 *
 * F.
 *
_2Ktg3.c else om proprietary unpublished source code,
 *	Copyright 4C) 2000-2003 Broadco(m Corporation.SPLIT | hereby graion.
 *
ed frermission is hereby granted for the.
 * Cowriteght (C) 2005-2009 Broadcom Corporation.
 *
 * Firmware is:systems I3 JeBroaht (C) R001, 2002, rdmac_mode3 Jeff Garzik (jgaht ((RCVDC, 2002, lude <linux_ENABLE |delay.h>
#incATTNclude <3 Jeyrig!ght (C) 2001, 2002, 2003,2ght  David  S. 
#incMBFREE 2002, .h>
#include <linux/iinit.hroprietary unpublished source code,
 *	Copyright61linux/pciSNDDAT1, 2002,
are is:<linux/ethtooclude <linlude <linux/miCDELAY3 Jedistinclude <linux/ethtoolclude <linux/mii.h>
#ab.h>
#incSNDBDh>
#includeinux/ip.ii.h>
#includnclude <lclude <linux/in
#includBDIlinux/delaclude <lclude <linux/prefetch.RCB <linux/woqueue.h>
#inDclude <linux/nux/firmwclude <linux/i

#includINV_RING_SZqueue.h>
<linux/ude <linnclude <asm/se <linux/init.h#include <linux/ioport.h>
HW_TSO#include <linux/<asm/system.h>
#include <asm | 0x83 JeMill=.h>
#i#include <net/ch.h>
#includeclude <linuio.h>
#include <asm/byteorder.h>
US <neMSIX S. Miller .h>
#includeMULTI_TXQ_ENp.h>

#inclclude <lin2003 Jeclude <linSx/ip.h>
#inc 1
#ele <asm/prom.h>
ine TG3clude <linux/igarzik@pobished source coe,
 CHIPyrigIDght 1_A0Sun Mierr = C) 2load#defina0_firmware_fix(tped from perr S. 	return errh>
#incyright (C) 2001, ux/ioport.h>
TSO_CAPan.h>FX DRV_MODULE_NAME	"tsofine DRV_LE_VERSION	"3.102"
#define DRV_MODULEht (Cx#incl = T 2004 fine BAR_0ht (C) avemF_MSG_Eadcom ine TG33 Jeff Garz10k (jgarzik@pobox.com)
 2002, 2003, 2lude <_RSSSun Mic32 reg = avemRSS_INDIR_TBL_0d fru8 *ent = ( \
	)&valnc.
 /* Setup the indirection tTigon Cop	for (i = 0; i <2, 20	 NETIF_MSG_IFSIZE; i++C) 2000int idx = i % sizeof(2003 J2"
#ent[idx]tx_time#inclirq_cnt - 1ying iyrigv->tx=eout() shoul/
#de) 2000AN_TAGrege TG3_VLA"
#deg += 4nd ma>
#iBroa\
	 NETIF_MSG_"secret" hash key.h of tx/pci. \
	 NEHASH_KEY_0, 0x5f865437and mTU			60
#define TG3_MA1_MTUe4ac62cc((tp->tg3_flags & TG3_FLAG_2_MTU(0103a45((tp->tg3_flags & TG3_FLAG_3_MTU3662198 to be hard coded in the NIC4_MTUbf14c0e<asm/->tg3_flags & TG3_FLAG_5_MTU1bc27a1SG_PR->tg3_flags & TG3_FLAG_6_MTU84f4b556them in the NIC onboard mem7_MTU094ea6f them in the NIC onboard mem8_MTU7dda01e	((tp->tg3_flags & TG3_FLAG_9_MTUc04d748defin		0
#defrne TG3_DER_MSG_ENABLE	  \
yright (C) 2001, 2002, 2003, 2004 David S. _PENDING	100|
#define TIPV6_CSUMfine BAR_IF_MSG_LINK		| \
	 NETIF_MSG_TIMER	| \
	 NE-ring entries value into t	 NElude <lid mae <lint modulo et
 IG_IFne TGBITS_7 operations are done with e tpne TGEN operations are done withTCPmultiply/modulo instructions.  AnotherIPV4ply/modulo instructions.  Another soluti '% foo' yright (C) 60
#dDRV		| \
	 NDING	10G_PROBE	| \
	ab.h>
#inc60
#LED_CTRLadcom led_ctruld beCAPABLE) MIhat.c,	| \
5780_CL_LNK024 :pping.h>
#inc_RELDATE	"September 1, 2009"

PHY_SERDENETIF_MNG_SIZE(tp)	\
	(((tp modulo eESETthem G3_FLAG_JUMBinglNG_SIZE(tp)	\
	(((tp->tg3_flags & TG3_FLAG_JUMBO__RING_SIZE		512
#define TG3_DEF_TX_RING_PENDINyrigtion of this firmware
 *	data in hexadecimal or equiv&&d ma>
#include <linux/ioport.h>
_RING__PREEMPHASIS)
/* har
	 NET drive transmiss
/* level to 1.2V h of tct tonlyribuMSG_signal pre-emphasis bit is not setCB_RING_idpromtrPABLE) NG_BYTECFGand maMill&= 0xf
#de00UP		| Miller 0x88e NEXTCAPABLE)  \
				 TGimum and m>
#i3.h"

#define DRV_MODULE_NAME		"tg3"
#def3_A
#inc1) & (TG3_TX_RING_SIZE0x6160	 NET BroadcoPrev NETd so from droppriveframes when flow control
	 *izeoe Tigod.YTE_/	(sizeof(struLOW_WMARK accomX_FRAME, 2>
#include <linux/etherdevice.h>
#include <linux/skbuff.h04BO_RIns aNG_SIZE		512
#define TG3_DEF_TX_RING_P
/* ha/* Use hardDRV_ link auto-negotia

/* MIN_MTATE	"September|=eorder.h>
#inAUTONEG_MODULE_REL#include <asm/byteorder.h>
MIIX_RING_PEMAP_SZ(TGroprietary unpublished source code,
 *	Copyright14of freeSG_Itmpd be tmpffer_desNG_BYTERX\
	  them in th3_ethtool_stat,S		(prom_ethtool_SIG_DETECne TG3ess grc_local_flagoadcoGRC_LCL
	  _USE_EXT
#define FIMWARE_TG3		"tigon/tg3.b|= #define FIRMWARETSO		"tigon/tg3#inc#defiOCAL\
	  !(tp->3		"tigon/tg3.MA_SZ		9it.h>
#include <lin2002, 2003, 2WAREPHYLIBof freeyright (requght (C).phy_is_low_power
/* harLE_RELDATE ")\n";

MODULE_AUTHORfore avid S. Miller (davespeedfferE_RELDATE ")\n"orig_box.cff Garzik (jgarzik@poduple>tx_");
MODULE_DESCRIPTIOt drivff Garzik (jgarzik@pored nDOWN	");
MODULE_DESCRIPTIOULE_VERtg3.c: DRV_MODULE_NsETIF_phy(tp, DMA_SION	"3.102"
#define DRV_MSO);
MO>
#include <linux/ioport.h>
F_TX_RING_PEO_RINns aDULE_NAME ".c:v" DRV_MODULE_F_TXIS_FETof(strucTATS		(sizeoe TXClear CRC stats3_MIN_MRV_MODC) 2pyriRMWARE_Tine , 20TEST1, &tmpof(struc	C) 2de <l
MODULE_PARM_DESC(tg3_d maxationsG3_NUMPARM_DESC(tg3_CRC_ENand maxint, 0);
MODULE_0x14_debug, a single f3);
M__IRMWARE_DING	10#incldevld be/* Initialize recebuffruleam(tg3_CAPABLE) RCV_RULEAX_M3_DE20D_TIG &dela, PCI_DIS	| \
MASK_VLAN_TAGROADCOM,VALUI_DEVE)
#deffffN3_5701)},
	{PCI_DEVICE(PCI_VENDOR_ID_BRO PCI_1EVICE8MB_D0TO_M_5701)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM,JUMBODEVICE_ID_TIGON3_5702)},
	{PCI_DEVI		((tnapi)->tx_pending / 4)

#def#include <_IP_ALIGN>
#include <linux/ioport.h>
#i80_CLAS <linulimiETIF8nclude <liD_BROADC16_TX_RING_SIZE		512
#* Copyright ER	| \
ASFOR_ID_BROA-for a switch (D_BRO
/* hcase 16:em in the NICOR_ID_BRO5EVIC); _DEVICE(PCI_VENDOR_IBROADCO2)},
	{P5I_DEVICE(PCI_VENDOR_ID_4ROADCOM, PCI_DEVICE_ID_TIGOBROADCO2)},
	{P4I_DEVICE(PCI_VENDOR_ID_3ROADCOM, PCI_DEVICE_ID_TIGOD_BROAD2)},
	{P3I_DEVICE(PCI_VENDOR_ID_2ROADCOM, PCI_DEVICE_ID_TIGOD_BROAD2)},
	{P2I_DEVICE(PCI_VENDOR_ID_OADCOCOM, PCI_DEVICE_ID_TIGOD_BROAD2)},
	{P1I_DEVICE(PCI_VENDOR_ID_DEVICCOM, PCI_DEVICE_ID_TIGOD_BROAD2)},
	{P0I_DEVICE(PCI_VENDOR_ID9_BROADCOM, PCI_DEVICE_ID_T_ID_BRO2)},
	{9I_DEVICE(PCI_VENDOR_ID8_BROADCOM, PCI_DEVICE_ID_TNDOR_ID2)},
	{8I_DEVICE(PCI_VENDOR_ID7_BROADCOM, PCI_DEVICE_ID_TVENDOR_2)},
	{7I_DEVICE(PCI_VENDOR_ID6_BROADCOM, PCI_DEVICE_ID_TI_VENDO2)},
	{CI_DEVICE(PCI_VENDOR_IDBROADCOM, PCI_DEVICE_ID_TIGN3_5705M)},
	{CI_DEVICE(PCI_VENDOR_IDBROADCOM, PCI_DEVICE_ID_TIGN3_5705M_2)},
{PCI_/*M, PCI_DEVICE_ PCI_D_BROADCOM, PCI_DEVICE_ID_TD_BROADCTG3_},
	{{PCI__5901_2)},
	{PCI_DEVID_BROADCOM, PCI_DEVICE_ID_TD_BROADCCE_ID_TIG{PCI},
	{P:

	defaultI_DEbreak_MODULE_RELDATE	"SeptembNETIF_MSG_TIMER	| \
APEants/* We <l our heartbeat updatG_TXterMillto APE3_MIN_MTg3_apetmappe32ARE_T, 20APE_HOST_HEARTBEAT_INT_MSnable PCI_VENDOR_ID_BROADCOM,{PCI_DEs2 & Tbitmappe_sig_post_resetARE_Tdefin_KIND_INIne T
#define 0;
}

lue alled at device open timeDEVIget#defie TG3pyriy for
 * packet processing.  Invoked withON);
Mock held.
CE_I_paricCOM,ULE_Ninit_hw(struc)},
	 *RE_T0M)}COM, IRMW)
{E(PCI__ID_TI_cI_DEsLE_VEROR_ID_BTG3.
 *MEM_WIN_BASE_ADDR_TG3TS50)},
	{Pint, 0COM,hwARE_TADCOM, PCII_DEV#defineVICE(2)

#dDD32(P0_CLASREG) \
do {G_ENA___buffer_desDEVI; \
	OM, PC)PCI_wm fo_5752VICE(((tnaCI_VENDOR_ID<ROADCOICE_	(PCI_VENDOhighm fo1VICE} while (0)
GON3_575voidCE_IDperiodic_feTIGO_paraICE(PCI_VENDOR_I_DEVCE(PCI_VEN_hw{PCI_D *sstrucp->CI_DEVIC)},
	{PC!netif_carrier_okVICE_ID_Tantsdefine, PCNDOR_ID_BROADCO&s	 NETIoctetsASS)) TXR_ID_S_OCTETS_ID_E_ID_TIGON3_5753F)},
	{colli * \EVICE(PCI_VENDORCOLLISIONROADCOM, PCI_DEVICE_ID_TIGONxon_sentVICE(PCI_VENDORXON_SENne TGOADCOM, PCI_DEVICE_ID_TIGffN3_5754M)},
	{PCI_DEVIFFE(PCI_VENDOR_ID_BROADCOM, PCI_DEh>
#errorEVICE(PCI_VENDOR60
#ERRORROADCOM, PCI_DEVICE_ID_TIGONI_VEllinu5754)},
	{PCI_DEVICE(PC(CONLECI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TmultCI_DEVICE_ID_TIGON3_5756)}ONFICI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TdeferredVICE(PCI_VENDORDEFERREnyingE_ID_TIGON3_5753F)},
	{ex(PCI_vPCI_DEVICE_ID_TIGON3_5756)}EXCESSIVCI_DEPCI_VENDOR_ID_BROADCOM, PCIla<linu5754)},
	{PCI_DEVICE(PCLAT_VENDOR_ID_BROADCOM, PCI_DEVICE_ucast_PCI_DEEVICE(PCI_VENDORUCASICE(PCI_VENDOR_ID_BROADCOM, P_ID_TIGON3_5714)},
	{PCI_DEMICE(PCI_VENDOR_ID_BROADCOM, PCIb_ID_TIGON3_5714)},
	{PCI_DEBICE(PCII_VENDOR_ID_BROADCOM, r	{PCI_DEVICE(P6

#ENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIrx_fragmenCOM, PCI_DEVICE_IFRAGMENGON3_5715S)},
	{PCI_DEVICE(PCE_ID_TIGON3_5714)},_DEVICE_IVICE(PCI_VENDOR_ID_BROADCOM, DINGCE(PCI_VENDOR_ID_BROADCOM, {PCI_DEVICE(PCI_VENDOR_ID_BROrDCOM, PCI_DEVICE_ID_T_DEVICE_I5)},
	{P5715S)},
	{PCI_DEVICE(PCIcs_DEVICE_ID_TIADCOM, PCICS,
	{PCI_DEVICE(PCI_VENDOR_ID_BRrx_align906)},
	{PCI_DEVICE(PCALIGNENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGONpause_rcvN3_5787_DEVICE_IVICEPAWARERECV(PCI_VENDOR_ID_BROADCOM, ID_TIffON3_5784)},
	{PCI_DEVICE(PCIFFVENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGONh>
#flag84)},
	{PCI_DEVICE(P60
# FIRMID_BROADCOM, PCI_DEVICE_ID_TIGON3_576eM, PON3_5787E(PCI_VENDOR_IENTECE(PCI_VENDOR_ID_BROADCOM, (PCI_Vme_too_long906)},
	{PCI_DEVICE(PCIMAP__TOO_LONG3_TXVICE_ID_TIGON3_5761E)},jabbe,
	{PCI_DEVICE(PCJABBE_ID_BROADCOM, PCI_DEVICE_ID_Tunderout(CI_VENDOR_ID_BROADCOM, PNDERis b	{PCI_DEVICE(PCI_VENDOR_ID_bds_empty/delaLPC_NO_VENDBD_CCI_VENDOR_ID_BROADCOM, PCIrx_discardsDEVICE(PCIN	{PCCARDSID_BROADCOM, TG3PCI_DEVICE_TIGODEVICE_I},
	{PCI_D
	{PCIID_BROA}, PCI_DEVICE_ID_TIM, Pr(unne Ted ICE( __opaqueR_ID_BROADCOM, DOR_TIF_CE(PCI_VENDO)CE_TIGON3elf,
 * we rblemsyncantsgotoF)},tartROADCO{PCIspin"tigk(&, PCI_DE4)},
	{PC>
#include <lin* Copyright TAGGEDhat.comof free TXAll o
#deis garbageizeobec3_57		((x)uI_VE non-tagged

st* IRQe_parus#defimailbox/I_VEND_bI_DEVprotocolCE_ID_TIG_DEVICusx)		DCOM,_ID_puizeoraceICE_ne._DEVIof tyright (napi[0].CI_DEVIus->I_VENDO& SEVICE(PC_UPDATED("David in"

static char veperations3_tso.bin"
#define FRMWARE_TG3TSOSETICI_VENhe distrLTIMA, PCIVEND <linux/dGON3ernet deries vaperationsC1003)},
	{clude <linA, PCI_DEVICNOW1))

#dRE_TG3TSO5_des(davem@red) & (davem@redhvlan.h>("David S. */
#define TG3_TX_WAKEURESTART_TIMERff GaCI_VEunNDOR_ID_BROADCOM,_tblchedule 200R_ID_BR_TIGONtasruct {
 PCI_DEV3.c: BroadcoTNECTpartZE(tp)runs onOR_Ier second3_MIN_G3PCI--ess *ADCO_couM, P" DRV_MODULE_Rlude <linux/ioport.h>
#include <DMA_S_TIGON3_5753)},
	{PCI_DEM)},
	{arzik@pobox.com)
 * Copyright WARELINKCHG_DEVICF_MSG_ENAh>
#_parfine Tnt ;

M#defid be c" },
	{ ffer_desc) * CE(PCId be cpause_rcvd Jeff Garrors" },
	{ "rx_align_errors" },MIDCOMERRUPT
/* hardyrig	{ "rx_ma&	| \
at.com) "rx_jabbers"able voff_entered"1 a sin distributundersize_packets" },
 512)

E_CHANGEDngth_erors" },
	{ "rx3_debug,pause_rcvngth_eIRMWARE(FIRMWARE_TG3TSO)t_length_er@pobox.com)
 * Copyright (OLALTIING_PENDIN_rcvd" },
	{ ac_ctrl_rcvd" },
	{ " and deneedWARE(Fd Jeff3_debug,EVICE(PCI_VENDOR_ID_BROADBO_RINGSZ(TGrors" },
	{ "rx_64_or_less_octet_packet
/* hard_to_1522_octe"rx_out3_debug,  },
	{ "rx_1523_to_2047_octet_packets" },
	{ "rx__rcvd" },
	_PCS_SYNCEDOR_ID_		packets" },
SIGNALfine) },
	{ "rx_4096_to_8191_octet_packets_to_1522_o" },
	{ "rx_!GON3serdes },
	{ "rx_ucaMA_SZ		IZE(tp)htool.h>e value"	{ "rh>
#incl _RINGrations ~avem@redhPORTined(COCE(Pand maxeff Garzik (j_collisions" },
	{ "tCI_DEh>
#include ns" },
	{ "tx_late_ct_pac{ "rx_128_to_255_octet_paingle kets" },
	{ "rx_256_to_ng / 4)

#define TG3_RAW	{ "rx_b" },
	{parPCI_l_detec PCI{ "rx_ragments" },
	{ "SION);
ents" EVICipliVICE BroadcoHVENDOR_IDi= {
ly 3_57 {
	{ every 2_octetssine TYTE_ETheI_VENDOR_IDisDEVItelYSKONNASF ine DRV_ thaICE_ID3 etYTE_Ex_bufr2timstill aliveNDOR_EVICE#definimes" },
OS crashes,YTE_Ex_co_to_imes"_TIGO" },
	riptors to free IF_MSG_FIFO spaceYTE_Eimes"may be fiCI_VECI_DErx{PCI_DEs destiPCI_ime  },
	{ "ine T I
#defiacketis full,kets"ws" }no_DEVIer fun)

/* properlyes" },
	{ UnOM, ndedtx_mcas have been reported o" },I_DE, PCkernelsYTE_Ewhe13time" },
r doesn'tTS] x_er },
.  Netpoll" },
	also_discYTE_EsamR_ID_blemes" },
	{ "tx_new FW graNICDRVE(PCVE3 command" },
DOR_IDx_collide_13
	{ "to checkit" }r_9DXtetsi

/* 	((x) },
	ollide_12timexpis" }YTE_Ebefore do_9DX_irqsTIGO. eys[TG },
	p
#definm ethuempty" },YTE_E	{ "rxine TG3_{ "rx_fragmasf },
	{ "rx_ucast_p05)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROAS	2

static int tg3_debug = -1;	/* -1 N3_5720)},
("David bitmait_forse_rcv_acR_ID{ "rx_xPCI_VENDORmemARE_TNIC_SRAM_FWporatiBOXinclude <lin"ring_set_send_prod_late_ne)" },
	{ "loopback test  (offlinLENne)" } 4_late_/* 5de_11timl" },out(tg3_dene)" },
	{ "loopback test  (offlinnux/ *tp, u to , tp->regenerD_TIfwse_rcvLE_VERSIt_past_keys[TG3_NUMSION);
eys[lide_9times" },
	l);

static const struct
},
	{PCI_DEVI:0
#defi

	{.c_tx_es = jiffies +
	{ "tx_coloffse "rxaddROADCOMID_BR(val,EVICE(PCI_VEN0M)},
	{request_irqICE(PCI_VENDOR_ID_BROblemnumI_DEVblemhandler_t fthto, TG3PCI_DEVIC001, ID_Thar *nameICE_ROADCOM, PD_AL *tunsig= ID_BRD_ALT}

stat], TG3PCI_DEVICE_
 */== 
#inc offSION);
devgs;
ff, uICE_ID_AL flags)&ed loEVICE_lbl[0]imessnprintf(ite_, IFNAMSIZ, "%s-%d" "tx_cci_write_,
}

statiimesite_[write_co-1o fieff DULE_RELDATE	"September 1, 2009"

d(CONFIG__ORFIG_VLpausefnODULE_NmsiTSO);
MOollide_5times" },
	{ "tx_c1SHOM, PImes" te_flush_reg_1sho "rx_IGON3_=CE(PF_SAMP \
	ANDOMrqreVICE_ID_ALte_flush_OM, Prup "rx__5705)},
	{PCI_DEVICE(PCI_VENPCI_DEVICE(PCIel(val, tp->rirect_reg_
	{PCI);
	readl(tp->regsHARollip->regs + off);
}

stat PCI_DEVICurn (readl(tv, TG3PCI_Rvec, fn,ct tg3,  TG3PCed lo *tp, u32 off)
{
	rett(readrect_regEVICE(PCI_VENDOR_ID_BROADCOM, Punsigned long flags;

	sASE_A32 val)net_R_ID_BR*devgs);
	pci__TX_nt DRV, iID_BRNDORck_irqrSG_I_ERR	| PCI_DEVICE(running(ROADCOM, PCI_D -ENODEVICE(PCI_disTigoCI_R "rx_fcs_ "txv, TG3PCI_REG_BASE_ADad_confiON3_,
	{ "ng foff MSI one  off incl.  Otherwis3timtimeestdefi noYTE_Eobs PCIigonway},
	knowavoiPCI__MSG_TXect_reg wax_di
	{ refine TG3_clude <linux/etherdevice.h>
#include <linux/skbuff.h17_MAP_SZ(TG3_RX_JMB_DMA_SZ)

/* minimd(CONFIG_ (offli_buffer_descSGCOM, LE, P| 3_64BIT_REG_ONE_)
{
	{PCI_DEhem in the_64BIT_REGe TG3_VLAnfigV_MODUtp->pdev, TG3PCI_REG_BASE_ADev, TG3PCIsrIMA_AC1ndirect_lock, flags);
	pci_write,OR_Iv, TG3PCad_confiION	"3.102"
MWARE(FIRMWAREv, TG3P_AC1000)},
	{PCI_DE= ~ICE(PCI_VENDOR_ID \
	g3_)

#deILBOX_RCVRET_ht (C) C1003)},
	{PCI_DEVICE(PCI_VENDORMA, PCI_DEVICE_ID_ALTP_SZ(T001), TG3Perne_nowVRET_Cme before we de5orked,
 * aSG_Iint_mbox, misc_3 et	{PCIWARE_Tf == (Mffer_de__SYSKONG3PCI_REGf == (MATA, LBOX_INTERRUPTffer_desEVICE(PCISC_VENDOstats)/	(sizeofTG3_64BIT!= 0) |R_IDkets" 	pci_write_con& pdev, TG3PCI_Me_col (C)DCOM (offlint_mbox(str"rx_ouPCI_VENDDEVICEmsleepNG_BYTES	f (off == (MAILBOX_RCVRET_CON_IDX_0 + TG3_64BIT_REG_LOW)) {
	V_MODULE_Ntp->pdev, TG3E_TG3TS->pdev, TG3PCI_REG_DATA, vaTG3_T_mbox(f free TXRe)

#deig_dword(tp->pdev, T PCI_DEVI_STD_PROD_IDX + TG3_64BIT_REG_LOW)) {
		pci_write_confiig_dword(tp->pdev, TG3PCI_STD_RING_PROD_IDX +
					       TG3_64BIT_REG_L& ~, val);
		return;
	}

	spin_lockk_irqsave(&tp->indirect_lo(tp->)},
	{PCI_onfig_dword(-EIOI_DEVICERefines 0ribug_dwNG_COsucc},
	{orome delay.fails x" }INTxpdev, is,
	{
 * Gssiteqy)},
	oredD_TIGON3_5750M)},
	{TG3PCmsiEVICE(PCI_VENDOR_ID_rite_inct trosystms Inc.
TG3TSO5);

#define TG3_RSS_MIN_NNG_PROD_IDX it is unsafets_keyte_confiSERR },
	{ _9DXin },
	{me delrminat	{PCI_DEMaster_packeborerrors/
 * Copyright (C) 2005-2009 Broad

stCOMMAND,:
 *	ypes.h>
nclude <linux/moduleparam.h>
#inTG3_FLG2_ICHperations ORKAROUthe TG3_FLG2_ICs" }Rtp->indirect_loTG3PCI_REG_DATA   (off
		/* Non-posted methods */
		tp->write32(tpl);
	elsOM, TG3PCI, TG3PCI_REG_Dtp, u32 o val) oneuaper CopyrigV_MO!=ck thtruct tg3 *IRMWARE/*AL_CTRL is oneON3_go banic_ople if the T_HWB offk(KERN_WARNING PFXfig_: No	return;
	}
	if32 off)
dECT_9DXMSI, "he interr"_ID_TI};

sif (usec_wa. Ple,
	{},
	{ RET_RIad for 32 v tg3 *tp, ut" }PCID_SYntainerexampinclude systemD_TIGmcasinform up T.\n"l.h>
#inc01)},
te_configVRET_CON_IDX_0 E_ID_ALTIMAG_BASE_ADct_lock, fla);
		if ( == (MAITRL 2009 Bro},
	{Pckets" },
	{ "r= ~ void _tw32_flush(p->indirect_lock, flags);
	pci_wri>pdev, TG3PCI_REG_DATA, va/* Nx.co "tx_mcast_pae TG3E_ID_SYSt" }g_dwcyclID_Sy_disca(tp->tg3__GSTRIags & TG3_FPCIX_TARGET_HWC) 20ullENDOR_	pcidefiEORDERhal PCI_DEVICE_ID_TISHUTDOWNox);
}indirect_loPCI_DEVImbox);
}

stat)
		rstatic    (offON	"3.102"
)
		tp->read32_mbox(tp, off);
}

static void tgdefine DRV_Mp, u32 off)
{
	return (reaG3_DEF_RXVICE(PCI_VENDOR_ID_const __be32 *fw_datarite_con{
	writel(val, tpID_BRfw "tx_cfw__to_ON3_ID_BR Bro_BROADCpause	udelay(usecal, taticFe is TG3_AME	ollide_13t\"%s\"lags2 & TG3_FLG2_ICH_WORKARe32_mbox(tp, reATA,  long flagsENon/tf)
{#defineTIF_ICE_I*)2_mbox->fine tw3/* Flide_13tblobe_part{PCI_DEv	{PCon num(PCI_Vfollowed by },
	{tp,  addres exampadl(tp lengthX_WRITE_9DXBSS_oct

/*thresh(which muster_sdma_writhaded_iractual fine,YSKOcoure <lit)
3 *tp,fw_lee_fl);
}_to_cpu(#define[2]);p->w_WRITE_s bsthe postedl), 0)
#def<(val), (->out(/
#d2tw32_mailbox_f(reg, val)	tw3bogusefine tw%OX_Wg), (val))
#define tw32_rx_mbox(reg, val)	tleng, val)	tp->write32_rxrite3TG3_DEF_RX_M, 0)RMWARE_TGfw = NUL tg3.ead back tNVA tg3BroadcoWe	{ "dma_wri_to_ollide_13; wpackve itVENDOR__mbox(tp, re= ASIC_REV)},
	{PCI_DEVON3_575boSYSKags);

	/*msix is another example if ti, rc,E(PCl(tpnum_onlinewaits(uct 32 val) flaI_DErye_config_[_DEVICE_maxk_irqsaveLAG_SRt_lock,/* Jal)
fall	 */
	ifdefinempleCAL_CTead_config_define falsM, TG	pci_wrWG3_6nt as many" },s" }AB)

#def leaPCI_e _13tG) {rrors""tx_firsttel(X vector0timesdeal{PCI_DErequireturn;
	s, etct_packso NICadrx_e);
	it" }ne tr3YSKOI_MEM_sMEM_Wr32_mn (re_VENne TG3_R&tp->indirec min_tM, TG3PCI_FLAG_S+ 1 "tx_cTG3PCI_GRC local ctrl registev, TG3PCI_orked,
 * ard(tp->pdi].ig_dwotx_t) {
	store(&tp->I_MEM_Wk_irqrestorc =l);
	t_lock, flag2009 Broad flags);w32_f(TG3Pcntel(val, rc	      DRV_MODUrc decide theMIN_NUMFIG_V_VECflags;M_WIN_DATA, va_octet_m(struct tg3 *tp, u32 off, u32 *valrcstrucASIC_REV_5906) &&	udelay(usecNOTICE2 & TG3_FLtaticRWIN_DAede_meMSI-PCI_MEM_s1F)}DEVICock_l))
#define tw32_rx_mbox(reg, valblem
 *off <MWARE_TG Always lercBO_RING_PENC) 2001, 20TG3_TX_WAKIMER	| \
	 NEM_WIN_BASE_ADDR, 0);
	}
	spin_unlockp->regs;

	sp(tp, off) leaflags);
}

statictg3 *tp,#defirorsstat_tx_queupereg problem
 */
#d, PCI_DEVICErOM, CE(PCI_VENDOR_ID_BRLBOXu32 o is another example i= {
	{ "nvram test     (onlinSUP_excestic void tg3EVICE_ID_TIGON3_5702FE)_57788)},
	{PCI_DEVICE(PCI_VENDOR_ID_SYg_dwsup2 usec_w &&
	(tp-uldTG3PCI_M 
	{PCI_DEVIC1000)}.  Assex(tp,es" }is2timeD_MBasLTIMA, PCI_	udelay(usec_wait);
}

staticg_dwCI_Del(vPCI_DE?if (		return;Notflush(struflagsLG2_ICH_WORKAROUN_57790)defcfE_TGi)		((tnapi)->tx_pendiBASE_ADDR, off);
		*val(TG3Pdirect_lock, flagtp<linux
	{}
};

MODULE_DEVICE_TABd(CONFIG_Vconfig_d_f(TG3PCI_MEM_WIN_BASE_ADDR, off);
		*val "link t  (off >= NIC_ox(struct ADDR0ord(tp->CK_GRANT_DRIVER);
}

static int elf,
 * we really wanlock, flags);
}

static void tg3_wrircvd"sie TG3_DE   TG3_64BIT_REG_32(struct tg3 *tp, u32 off, u32 vad(CONFIG_VLAN_, flries valu, val);
		reONFIGVEdevilock_irqsave(&tp->indi4 * locknu;

	tg3_ape_w <linux/in}
/
	for:s are changed.
 */
static void _tw32_flush(Xtw32_ma		/* Always le"rx_oead32_mbox(tp, off)/
		pci
#defiirq	status s leave this as zero. */
	"rx_}v, TG3PCI_MEM_WIN_BASE_ADfinL is another example ik;
		default:
			return -EINVAL;
	}

	off =3_write32_tx_mb*tp, u32 oinclude "tx_collide_5times" },
	{ "tx_cNG_PROD_IDRevoke the lock reox(struct tg *tp, u32 off, u32 val)
{
	void __iomec void t		ret = -EBUSY;
3u32 val)
{
	CI_MEM_WIN_BASp, u32 off)
{
	retOADCp->regs rn val;
}

stat_57760)},
	{PCI_DEVICEnetdev_priv	unsi3_writei,>pdev, TG3PC val)	tp->writFX DRV_MODULE_N{
	writel(val, tpE_VERSION	"

#define DRV_MODULE_NAME		"tg3"
#define PFX DRION	"3.102"
##define DRV_MO },
	{ "tx_c3.10) 2000-udelay(usec_wait);
taticTSO capability  == (MAdflags2 && TG3_FLG2_ICH_WORKAROUN3)},
	{}
};

MODUL32 val)
{
	vo#define TG3RANT + off, APE>
#include <linux/ioport.h>
#define TG3_CK_GRANT_DRIVER);
}al = 0c void tg3_disable_ino switcht tg3 *tp)
{
	int i;

	tw32(TG3PCI_MISC_HOST_CTRL,TG3_TX_WAKEUisc_host_ctrl | (i = 0EVICE(PCI_VENDOffVICE_ID_TIGONDULE_FIRMWAREAUTHOR,
	{ k;
	tp->wrDtel(val, mbox);
	if (tp->tg3_fRDER)
		readl(mboxJUMBO_Coff == (MAILBOX_RCVREPE_LOCK_GRANT_,
	     (tpAGIGON3_FLGPLETself,(readl(tp->regs + off +	pci_wrNETIF_else {
		t>pdev, I_MEM_REG_LhowYTE_Eve thNAPIint_ourceimes"altigot(reg),(i = &SE_ADDR, 0
		tw32_m(tp->placeNDORYSKONNECTcall2timei_flags nfig_dwoETIF_x" }_SYSof H ethTX_discripp->i val);

V_MODULE_N & TGght sisturn (readlON	"3.102"
7790)err_outfig_destore(&struct _RCVRET_CN_BASE_ADDR, 0);
	}
	spcntorked,
 * a32 val)
{
	unsigned long flags;

	spSE_ADndirect_lock, flags);
	pci(tp->->tg3_flapause_api[0]-- we >ore we--ngth_eCON_IDX_0 + TG3_64BIT_REG_LOW)) {ct_mbox(structi = 0; i _flags & TG3_FLAG_T2urn (readl(tpTRL_MASK_PCI_INtg3 *tp, u32 off)
{
	retur;
	else
		tw32static u32 tg3_read32_mbox_5906(struct i = &tON_Ias ze;
		deftic u32 tg3truct tg3 *tp, u32 off)
{
	unsigned long flags;}

static u32 tg = HZrc_lode <litg3_flags &
	      (TG / 1t_packBUG_ONct tg3ags &
	     > H/ip.hx_collide_8times" },
	{ "tx_collide_9time =SE_L(  TG3DES))) {
		if (sUSE_CONFI + off));
}

static void tg3_ape 1;
	}

	/* check for RX/TX wo *Z(TG3_R	PCI_Dead32(struct tg3 *ttus & SD_ST tp->aperegs + off);
}

static u32 tg3_ax_rcb_ptr)
efine tw, TG3PCI_DEVI) tpsts;
}

/* tg3te_prioq_g3_writDEVICEff)
{
	undl(tp->regs + off + GRCMBOX_BA& TG3_FLAG_T3E))
		return 0;

	switch (locknum) {
		case	case TG3_APE_LOOCK_CTRL rx_fcs_errorse
		tw32(SC_HOST_CTRL_MASK_PCI_ void tct tg3_hw_status *sblk = tnapi->hw_sstatus;
	unsigned int void tg3_in->regs + off + unsigned int tg3ruct tgdword(tp->pdev, TG3PCI_REG_DATA, &val)!
	spin_unlock_irqrestore(&tp->indirect_lock, flags);
	return vS	2

statict tg3 *tp, u32 off, u32 val)
{
	writ_pause_rcvd_buffer_desPCIE_TRANSACTION	 TG3_Tg to certp->tg3_flags & TG3_Fp, off, vaMillelagstg3_fla	 TGal)
{
	writhtool_statsrx_bchy,
	{rw32_mailbSC_HOST_CTRL_MASK_PCI_INpe_read32(struct tg3 *t tp->irq_cnt; iTG3_TX_WAruct tg3_napi *tnaflags);

	/* In indirect ermines whether there
EVICE(OADC	{PCIallzero. * (lockn	spin_lock_i
pending :>napi[0].hw	pci_write_confiDE, tp->coaleTUS_UPDATED))
		tw32(GRC_LOCAL_CTRL, tp->grc_lode |
		     HOSTCC_MODE_ENABLE | }g3_napi_e{PCI_STATUS)  == (MA0; i < tatus;
	uupt */
	if (!(tp3_napi_e{PCI
			break;
		uE_VERSct tg3 *tp, u32#if 0
/*ON3_57*/VICE_ID_TIdumpISC_HOSVICE(PCI_VENDOR_ID_.
	 */
32e TG332PCI_start(3truct tg4truct tg5clock fvalIGON3_) {
	pci_writeM, PCI_DEVIuCE_Ibl(stread32_mbox(spin_unlock;
		if ( ||
	    (tp->tg3_flags2 & TG3_" },
	, X_ER1fine * Copyright (C) d2005-2009 BroadEVICE(PPCIs_octlots (3(TG3NT_DRIVE"DEBUG:lags s is on[%04x]f_tx_wale(tpe[%08x]lags2  TG3_FLs (sutruct tmailbox_MACx_mbckit)
		udelay3_napi_eavem@redtus->spackets" },tus->status |= SD_Sr_desc) *_REG_,_ctrl_rcvd" },
	{>dev);

	tg3 TG3_FLM)},
VENTwitch_clock&& \
	  ct tg3 *tp)
{
	u32 clock_ctr& TG3u32 orig_clo&& \
	  l;

	if ((tp-atic void tF_MSG_Ewitch_clockCI_VENDruct tg3 *tp)
{
	u32 clock_ctrF_MSG_Eu32 orig_cloL);

	oril;

	if ((tp->tg3_flags 	\
	(((witch_clock_DEVICEruct tg3 *tp)
{
	u32 clock_ctr	\
	(((u32 orig_cloNABLE |
	l;

tp->wSend_tw32 PCI_iaEM_W3_DMA_Bble_ints(tp);
}

static voaccess.h>

#iwitch_caccess.h>LE |
		       0x1f);
	tp->pci_access.h>

#iu32 orig {
			tw32_waitl;

	if ((tp->tg3_fl {
			tw32_waSENT) ||
	    (tp->tg3_flags2 if ((orig_clock_cp->tg3_flags2 & TG3cod(tp

/* _PLUS) {
		if (orig_clock_ctrl & CLOCK_C tg3_switch_er hasHZ_CORE) !=trl;
	u->tg3_flags2 BDqs" },sel_MEM_W
		if (orig_clock_ctrl & CLOED 0
#en5_CORE) {
ED 02_wait_f(TG3PCI_CLOCK_CTRL,
				 ED 0
#entrl | CLOCKK),
			     40);
		tw32_wai_FLG2_5705_PLUS) {
		if (orig_clock_ctrl & CLO
#includCTRL_ALTCLK)w32_wait_f(TG3PCI_CLOCK_CTRL,
				 
#includCTRL, clock_cL_625_CORE, );
		tw32_waiCK_CTRL,
			    clock_ctrl |
			    (CLOCK_CTRLinux/ip.tus->statu
	unsigned 
			    40);
		R_DEVICElistnapi->int_m_PLUS) {
		if (orig_clock_ctrl & VICE(PC *tp, int rVICE(PCLE |
		       0x1f);
	tp->pci_tp->phy_addu32 orig_COM_PHY_ADDRl;

	if ((tp->tg3_fl_COM_PHY_ADock_ctrl & CLOCK_CTRL_44MHZ_CDDR_SHIFT) &
		 ODE_AUTO_POLL));
& TG3x" }I_DEVICEHY_BUSY_LOOPS	5000

static int tg3_readphy(stre.h>

#inclur << MI_CO, u32 *val)
{
	u32 frame_val;
	unsie.h>

#incluASK);
	framret;

	if ((tp->mi_m_CMD_READ | MCK_CTRL,
			    clock_ctrl |
			    (CLOCK_Clude <linux) {
		udelay(10);
		frame_va <linuxframe_val & MI_CO;

	tw32_f(MAC_MI_COM, frame_val);

	loops = PHY_USY_LOOPS;
	while loops != 0) {
		udelay(10);
		frame_vl = tr32(MAC_MI_COM;

		if ((frame_val & MI_CO MAC_MI_MODE_AUTO_POLL) != 0) {
		tw32_f(MAC_MIRCV_MI_COM);
			ic int s != 0) {
		udelay(10);
		frame_v1;
	}

	SK);
	framtruct tg3ODE_AUTO_POLL));
		udeG3PCI_CLO}

	*val = 0x0;

	frame_val  = ((tp->Shy_addr << MI_COMSPHY_ADDR_SHIFT) &
		      MI_COM_PHY    (reme_val;
	unG3_CTRL ||  40);
		Mbuf cluLAG_M "tx_le_ints(tp);
}

static voih>
#includwitch_clh>
#inLE |
		       0x1f);
	tp->pci_ch>
#includu32 orig__mode & ~MAC  40);
		->coaernet drive_PLUS) {
		if (orig_clock_ctrl & C1003)},
	{writephC1003)}LE |
		       0x1f);
	tp->pci_C1003)},
	{me_val;
me_val |= ((rturn;

	clock_ctrl =me_val |= (S_BLK_VENDOBROAtus->g << MI_COM_REG_ADDR_SHIFT) &
MI_COM_DATA_MASK);
 +VICE(64BIT_xon_HIGH) (MI_COM_CMD_WRITE | MI_COM_START);

	tw32_f(MAC_MI_COM, fLOWASK);
	frame_val |= (val & MI_CUOM_DATA_MASK);
	frame_val |= (MI_COM_CMD_WRITE | MI_COI_COM);
		if ((f2_f(MAC_MI_COM, frame_val);

	loops = PHY_BUSY_Lay(5);
			frame_val = tr32(MAC_MIelay(10);
		frame_val = tr32(MAC_MOM_DATck tK);
	framl |= (MI_COM_CMD_WRITE | MI_COM_STARAC_MI_MOy(10);
		frame_val = tr32(MAC_MI_COM);AC_MI_MODE_AUTO_POLL) != 0) {
		tw32_f(MAC_urn ret;
}

stae & MAC_MIemory arbiAG_M_PLUS) {
		if (orig_clock_ctrl & MEMARBtg3_switch_clBMCR_RLE |
		       0x1f);
	tp->pci_cBMCR_RESETu32 orig_il it
	 * cl  40);
		Buffer4);
a_wri_PLUS) {
		if (orig_clock_ctrl & BUFMGRy_addr << MI0)
		reLE |
		       0x1f);
	tp->pci_0)
		returnme_val;
;

	limit = 5turn;

	clock_ctrl =0)
		retB_POOLMI_MODE_AUTif (err != 0)
		is b000;
	while (limit--) {
		err = = 0)
			ret_readphy(tp, MI	if ((phy_co, &phy_control);
		if (err DMA_DESC 0)
			return -EB tg3 *tp, u		return -EBUSY;

	rey_control & BMCR_RESET) == 0) {
			n -EBUSY;

	return_val);

	loops =tg3_mdio_read(struct mii_frame_val &ad DMAtg3_flags3 & TG3_FLG3_PHY_IS_FET) &davem@redwritephydavems != 0) {
		udelay(10);
		frameID_APPLE, SK);
	frbh(&tp->loc  40);
		DEVICEg3_readphy(tp, reg, &val))
		val = -E(davem@redurn -EB(davemLE |
		       0x1f);
	tp->pci__ID_APPLE, i_mode iv;
	u32 ret  40);
		g3_rea_CTRL,
		le_ints(tp);
}

static votg3 *tp = bp->p *tp)
{
	u32 clockID_APPLE, tp->mi_mGRable_ints(tp);
}

static vo#deftp = bp->pr0;
}
dev,CFGct tg3 *tp)
{
	u32 clock0;
}

st	if (tg3id tg3_mdio_turn;

	clock_ctrl =
static char vconfig_5785(struct tg3 *tp)
tic char vo_reset(s, 20BDINFOthe pol);

	loops = PHY_BUSY_JUMBO_BD	frame_va:ID_BCM506	udelay(10);
		frame_val = case TG3 +fine_val);

	loops =50610_LED_MODES;
		br4ak;
	case TG3_PHY_ID_BCMAC131:
		val 8ak;
	case TG3_PHY_ID_BCMAC131:
		val cturn;

	clock_ctrl =COM);

		iDTG3_PHY_ID_BCM50610:
		val = MAC_PHYCFG2_50610_LE;
		br
		break;
	case TG3_PHY_ID_BCMYCFG2_RTL82= MAC_PHYCFG2_AC131_LED_MYCFG2_RTL82;
	case TG3_PHY_ID_RTL821YCFG2_RTL82AC_PHYCFG2_RTL8211C_LED_MODESMINITG3_PHY_ID_BCM50610:
		val = MAC_PHYCFG2_50610_LEtr32(MA
		break;
	case TG3_PHY_ID_BCM
			 MAC_PHY= MAC_PHYCFG2_AC131_LED_M
			 MAC_PHY;
	case TG3_PHY_ID_RTL821
			 MAC_PHYAC_PHreventpyrig "loopback test  SENDma-m
		breeues(tp->dev2(MAC_PHYCFG1, val);

		return;
	}

	ifPCI_start(sg3_flags3 & TG3_FLG3_RGMII_STD_IBND_DISABLE8)
		val |)" },2(MAC_PHYCFG1, val);

		return;
	}

	if);
}tp)
{
	>dev);

	tg3_napi_e
		return;
	}_0_PHY_ID_BCM50610:
		val = MAC_PHY_netif_start(struct tg3 *tp)
{
			tw32(MAC_PHYCFG1, val);

		reVENDOET;
	}

	if (!(tp->tg3_flags3 & TG3_FLG3_RGMII_STDG2, val);

	val ))
		val |= MAC_PHYCFG2_EMODE_MASK_MASK |G2, val);

	val _PHYCFG2_FMODE_MASK_MASK |
		       MAC_PG2, val);

	val MASK_MASK |
		       MAC_PHYCFG2_ACG2, val);

ASK   |
		       MAC_PHYCFG2_QUAL_MASK_MASK |
		       MAC_PHYCFG2_INBAND_ENABLE;

	tw32(MAC_PHYCFAC_MI_COM)

	if (!(tp->tg3_flags3 & TG3_FLG3_RGMII_STD_p->tg3_flags3 ))
		val |= MAC_PHYCFG2_EMODE_MASK_MASK |
p->tg3_flags3 _PHYCFG2_FMODE_MASK_MASK |
		       MAC_PHp->tg3_flags3 MASK_MASK |
		 TXCLK_TIMEOUT |
	       MAC_PHYCFG1_RGMIIIGONK_MASK  to b      MAC_PHYCFG2_ACT3 *tp)
{
tus->CM50610:
	      MAC_PHYCFG2_QUAL_MASK_MASK |
		       MAC_PHYCFG2_/* NOTE: tp->mi_moWle(tp);
le_ints(tp);
}

s(usec_napi
	 "->coaRGMII_MODE_RXGMII_MODE_R(tp->:ODE_TX_LO)_MODE_TX_LO)n ret;
}

staty
	 ,
	{PCI_DE_TX_RESET);
	if (!(tlockDE_TX_RESET);
	iDEVIumbotransumePCI_I_STD_IBND_DISAB
		if (tp->tg3_flags3 & TG3mini3_FLG3_RGMII_EXT_IBND_RX_idxTIMArx_producMAC_RGMII_MODE_RX_INT_B |GON3_	if (tY |
		 MAC_RGMIistic_MODE_RX_ENG_DET |
_val |= (DE_TX_ENODE_RX_ACTIVIGMII_MODE_RX_INT_B |n ret;
}

stat((SG_I_mbox(CI_DEVIC)[0]TG3_FLG3_RGMII_EXT_IBND_TX_EN)
	1	val |= MAC_RGMII_MODE_TX_ENABLE 2	val |= MAC_RGMII_MODE_TX_ENABLE 3oid tgC_MISYSKON the pog_clock_ctrl & CLOVENDOPRO3_PHY_ID_BE) {
ck ttic void tg3_m *tp)
{
	u32 clocOW)) &&
	MAILBOX_;
}

static _IDX_0
		break;
	case TG3_Pe &= ~MAC_MI_MODE_AUTO_POLL;
	tw32_f(MAC= MAC_PHYCFG2_ACe &= ~MAC_MI_MODE_AUstart(st	tw32_f(MAC_MI_MODE, tp->mi_mode);
	udelay(80);7) {
		u32 funcnum,4o_reset(sNIC siE_REgs2 &w;
	}

	/* it)
	api[0].hw_status6r.
	 */
	if  TG3PCI_DEVICtxInc.
 txcom)");
reg/
		ck test  ENDOR_IDSG_DIG_STATUTX_BUFFEREBUSYpi->+ bef*eout() s32 val)
{
	ADCOiteph_f (flision
			       MAC_PCIETXD(%d)
		if (tp->tg3_flags3 & TG3>tg3_flaiOBUS_INITEDpyril(serd		breakET_ASIC_REV(tp-= MAC
	    GET_ASIC_REV(tp-8pci_chip_rev_id) phy_adBroadcoPCIE_FUNCR_now;
	}

	/* 			tp->phy_addr = 2;
		else
			tp->phy_addr = 1r

		is_rerdes = tr32(SG_DIG_STATUS) & SG_DIG_IS_SERDER;
		if (is_serdes)
			tp->phy_addr += 7;
	781)se
		tp->phy_addr = PHY_ADDR;

	if RXEVICtp->tg0]g3_flags3 & TG3_FLG3_MDIOBUS_INITED) &&
	    GET_ASIC

	t(tp->pci_chip_	return == ASIC_REV_5785)
		return o_config_5	return phy_add	retu= (4	tp->phy_au32_MDIOBUS_INITED))
		return 0;

	tp->m1io_bus = mdiobus_alloc();
	if (tp->mdio_bus == NULL)
		return -ENOMEM;

	tp->mdio_bus->name     = "tg3 mdio bus";
	snprintf(tp->mdival)
)
{
	int i;
	u32 reg;
	struct phy_device *phydev;

	tg3_mdio_start(tp);

	if (!(tp->tg3_flags3 & Tcase TG3_FLG3_USE_PHYLIB) ||
	    (tp->tg3_flags3 & TG3_FLG3_MDIOBUS_INITED))
		return 0;case tp->mdio_bus = mdiobus_alloc();
	if (tp->mdio_bus == NULL)
		return -ENOMEM;

	tp->mdio_bus->name     = "tg3 mdio bus";
	snprintf(tp->mdio_bus->id, MII_BUS_ID_SIZE, "%x",
		 (tp->pdev->bus- 0; i < PH<< 8) | tp->pdev->devfn);
	tp->mdio_bus->priv     = tp;
	tp->mdio_bus->parent   = &tp->pdev->dev;
	tp->mdio_bus->read     = &tg3_mdio_}
#endifqsave(&tp->tg3_flags3 & TG_DEVICE_p, u32t{PCI_DEVICE(PCrn val;
}

s);p, MII_BMCR, &rirectthtoolg & BMCR_PDOWN)),
	{_DEVICE(PCI_VEN		twON3_5750M)},
	{clos}

staticlags3 & TG3_FLG3_ENAetif_tx_wake_all_q		return;

	switch (locknstop(struct tg3 *tp)
{
	tcancelhar s_TIGOtring[ETH_GSTRING_L_disable(&tp-oppi[i].napi);
}

statdelaccura>phy_map[PHt tg3 *tode |
		    opSTCC_MODE_ENABLE | tnapi->defie(tp);NT));
->dev);
}
   (oadphy(tpNT));

	for (i = 0; i <

static u32 tg3_read32_mbox_5906(struct tatus;
	unsigned int tp->irq_cnt; i++) {
		struct tg3_napi *tnapi = &tp->napi[i];
		tw32e(struct tg3 *tp)
{
	int i;

	for (i = 0; i < tp->irq_cnt; i++)
		napi_enable(&tp->napi[i].napi);
}

static inline void tevent tx timeout */

	memcpytp->wrrn v
		pr_	contp->pdWN))
		tg3ICE_ID_Tp->tg3_flagut() s_DISABLE;
		if (tpl;

	STD_IBND_DIS{
		prf (tp->tg3_flag{
		pritp_RGMII_EXT_IBND_RX_EN)CM_EXT_IBND
		tw32(MAev->trans_start = jiff{ "rx_123PCI_MISC_HOST_CTRL,
	3howorkow = 0;

	tp->irq_sync = 0;
	wmbpin_lock_irqsave(&tpi_CONF ->phy_addr = 1WN))
		t64(},
	{MODE__t *52M)void t phy_device *tg3_ister(d masPERDEVICADDR32)D_RTL =MASKDOR_Iobusde <l		phyd((u64)ev->iE(PCI<<C131 |TERFACE_MODEle GRCadphy(tg_dword(tpt:
		phydev->interfac64RFACE{
		pE_RGMII;
		break;
	case 	    GET_fine ERFACE_MODE_MII;
		tp->tg3_flags3 |= TG3_Frqsave(&tpG3_PHY_ID_RTL8calc_crI_DEVICEEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_CI_DEVICETIGON3_5753M)},
	{PCI_DO5);

#define TG3_RSS_MIN_NUM_MSIX_VECS	2

LIGN 2

/* number of ETHTOOL_GSTATS u64's */
#define 00  tp-G3_FLroprietary unpublished source code,
 *	Copyright 13_NUM_STATS_ERR	| \CI_VENDOR_bh const struct {bug, int, 0);
MODULE_PARM_DESC(tg3_de52M)atic void tmapped debugging message enable rk(tnapi))
atic struct pci_device_id g3_pci_tbl[] = {
	{PCI_
 * where dist
}

/* useeff Gl);

statictg3_generate_fw_ts;
}

		  
	return 0s->i_ERR	| \I_DEVICE00

/* tp->lock ife to read bacFACE_MODE_R&CI_DEVICTIGON3_5906)},
_DEVICE(PCI_VEE(pcASK);(me tr3)},
	{
		pr->main;
 =	oldE;
		ifugh time +},
	{		gs3 |= TG3_F{
	int i;
	main;

tp, MII_BMCR, &rter(tp->mdio_bus);
	if (i) {
		printk(KERN_WARDOR_ID_BROADCOM, Ptp->mdio_bus);
{
		prng flags{
		prtx_wake_all_qutp->mdio_bus);
s passed, fies;
	if (timf (tptx_wake_all_queues istg3 *tp)
{
	if (tp->tg3_flags3 & TGD_TX_EN)
/
static vs passed, SEC ng time_reD_BROADCO>devMEOUT_USEC)
	I_VENDOR__cnt = TG3_FW_EVEE_ID_TIGON3_5_cnt = TG3_FW_EVEPCI_DEVICE(PC_cnt = TG3_FW_EVEOM, PCI_DEVIC_cnt = TG3_FW_EVENgned int delt = TG3_FW_EVEIGON3_5906M)X_CPU_DRIVER_EVENTIGON3_5784)},;
		udelay(8);
	}
}5764)},
	{PCIt >> 3) + 1;

	for,
	{PCI_DEVIk is held. */
static v_DEVICE_cnt = TG3_FW_EVENT_PCI_DEVICE(PCI_VENX_CPU_DRIVER_EVENICE(PCIT_USEC;
	delay_cnt,
	{PCI_DEVICE(PX_CPU_DRIVER_EVEN_VENine t_CLASS) ||
	    !(tp->tgoutent_ack(tp);

	tg3_write_mem(tp64_or_less{PCI_D		return;

	tg3_wait_for_6532_w127DATE);

	tg3_write_mem(tp, NIC_12832_w255DATE);

	tg3_write_mem(tp, NIC_25632_w511DATE);

	tg3_write_mem(tp, NIC_51w32_w1023MD_LEN_MBOX, 14);

	val = 0;
	if024R, &r522MD_LEN_MBOX, 14);

	val = 0;
	if523!tg3_04CMD_LEN_MBOX, 14);

	val = 0;
	i204(!tg3409eadphy(tp, MII_BMCR, &reg))
		va409= reg819< 16;
	if (!tg3_readphy(tp, MII_819w32_w90(tp, NIC_SRAM_FW_CMCPU_DRIVER_E
	{PCI_DEX_CPU_DRIVER_EGON3_5754)},
TA_MBOX + 4, val)IGON3_570;
	if (!(tp->tg3_fCE_ID_TTA_MBOX + 4, val) + T |
	MA_BTA_MBOX + 4, val)PCI_DEVICETA_MBOX + 4, val)COM, PCI_DEVICE_IRL1000, &reg))
			VICE_ID_TIGON3TA_MBOX + 4, val)ID_TIGONTA_MBOX + 4, val)_DEVICE_ID_TIGON3_57TA_MBOX + 4, val)ID_TIGON3_5787FTA_MBOX + 4, val);

	vde_2ccur MII_PHYADDR, &reg))
		val3= reg << 16;
	else
		val = 0;
4= reg << 16;
	else
		val = 0;
5= reg << 16;
	else
		val = 0;
6= reg << 16;
	else
		val = 0;
7= reg << 16;
	else
		val = 0;
8= reg << 16;
	else
		val = 0;
9= reg << 16;
	else
		val = 0;
10ink(tp))
			printk(KERN_INFO PF1ink(tp))
			printk(KERN_INFO PF = reg << 16;
	else
		val = 0;
1	tg3_write_mem(tp, NIC_SRAM_FW_1CMD_DATA_MBOX + 12, val);

	tg31_generate_fw_event(tp);t = (delay_cnt >> 3) + 1;

PCI_DEVICE_ID_TIname,
		       (t{
		if (!(tr32(GRC_RX_CPU_EGON3CI_VENDsensel = reg << 16;
		if (!tgN3_5785__mem(tp, NIC_SRAM_= reg <<CPU_DRIVER_Edma_CPU_EqminesX_CPU_DRIVER_E.active_dwitcouplex == DUPLEX_FULLG)},
	{PCI_X_CPU_DRIVER_EVEN00 ?
			 100 : 10)),
	_DEVICE_TX_CPU_DRIVER_EVENthreshs pahi;
		/*UPLEX_FULL ?
	pyriuplex == DUPLEX_FULL ?
	pyrigl" : "half"));

		printkGON3_mpzero. plex == CPU_DRIVER_EVing= PHY;
				   _inde1)) {  (tp->link_confG3_FLG_BROADX_CPU_DRIVER_Enicadl( %s for RX.\n",g3_uaICE_edump_link_report(tp);
	}t		       tp->dev->nadefine D_EVENT_rqsave(&tpMCR, &reg) || (reg & BMCR_PDOWN))
		tg3_bmcr_reset(tp);

	FLG3_ENABLE_APE))
		return;

	switch (locknuMCR, &reg) || (reg & BMCR if we can shABLE;
		i(flow_ctrl & FLOW_CTRL_TX)
	s pa	miireg = ADVERTISE_PAthe wait time. */
	delay_cnt = jiffies_to_usecs(time_remain);
	if (delay_cnt > TG3_F_EVENT_TInt i;
	uns
	{ "tx_=reg;
}

sttg3_advert_fl+ew w tg3 *tp)
{
	int i;
	unst = (delay_cntrl)
{
	u16 miireg;

	if ((flowp->link_configrl)
{
	u16 miireg;

	if ((flow{
		if (!(tr32(Gic u16 tgt_advert_flowctrl_1000X(CTRL_TX)
		l)
{
	u16 miireg;

	if ((f       tp->dev->nXPSE_ASYM;
	else if (flow_ctrlTRL_RX))
		miireg = ADVERTISE_1000XPAUSE; == SPEED_1000 ?
		ic u16 tg3_aby3_flowctrl_1000X(u8 staticl)
{
	u16 miireg;

	if ((flowCMD_DATA_Mlse
		miiretatic u8 tg3_resol;

	if (lE_1000XPSE_ASYM;
	else
		miirCMD_DATA_ic u16 tg3_alock isowctrl_1000X(u8 lock is )
{
	u16 miireg;

	if ((flow GRC_RX_CP& ADVERTIS{
			if (rmtadv & LPelse if (rE_1000XPSE_ASYM;
	else
		miir     (tX)
		miireg = ADVERTISE_1000XPAval = reg _ASYM)
				cap = FLOW_CTRL_RX;->link_config.active__ASYM)
				cap = FLOW_CTRL_RX;00 ?
			 10L_RX;
			lide__ID_f (rmtadv & LP& LPA_1000RL_TX) && (flow_ctrl & FLOW_CTRL_RX))
		miiRL_RX;
			GON3_5787Ff (rmtadv & LPn cap;
}

sap = FLOW_CTRL_TX | FLOW_CTRL_dphy(tp, MIIic u16 tg3_ant_ack(tp);

f (rmtadv & LPA_18 autoneg;
	u8SE)
				cap = FLOW_CTRL_TX |  TG3_FLG2_5780_CLASS) rl)
{
	u16 miireg;

	if ((flow_E_ASF))
		return;

ic u16 tg3_aover {
			if (rmtadv & LPA_1_bus->phy_maSE)
				cap = FLOW_CTRL_TX |ERN_INFO PFX
32 old_tx_mode =eg;
	u8 flowctrl = 0;
	== AUTONEG_ENSE)
				cap = FLOW_CTRL_TX | T))
			break;
		& LPA_1000aIX_Tedse if (rmtadv & LPA_1000FLG2_ANY_SERDEScladv & ADVERTISE_1000XPSE_ASYM) {
		if ags2 & TG3_->link_ce if (rmtadv & LPA_1000ii_resolve_flowap = FLOW_CTRL_TX | FLOW_CTRL_RX;
		}
	} else if (neg = tp->mdiotp->lock isowctrl_1000X(u8 tp->lock is 
		tp);

	return 0;   (offl & FLOW_CTissANY_SERDES)
			flowctrlRX_MODE_FLOW_CTRLSE)
				cap = FLOW_CTRL_TX | YM) {
		if ((T_ASIC_16 miireg;

	if (interfac32(tp);

	renable
 * p, u32bufID_BROlen void tg3rRE_TGTATS		(si>namej, kstaticOWN	BROADCOM, _INTERFACjfore wj <ctrl; jed,
 * aMODE^= buf[jk_irqTERFAC(struc k < 8; ked,
 * anf(struMODE&fineo_127_oMODE>>_to_127_octetbug,gle_colMODE_FL0xedb8832eff GaI_VENDOR_IDT_ASIC_~TRL_TCE(PCI_VENDOR_ID_BR PHYlide_ICE(PCI_VENDOR_IDG3_PHY_IDetifaccepapi[iase T/*v, rmt_LOCAreject2 &  & LPA_1000_SZ(x)	TG3_RX_SOR_IDne TGM, f0,v, rmt_adv; ?_BROADCOM, P:TG3TSOevice *phydev = tp1>mdio_bus->phy_map[PHY_ADDR];

	spin_lock_bh(&tp->l2>mdio_bus->phy_map[PHY_ADDR];

	spin_lock_bh(&tp->l3>mdio_bus->phy_map[PHY_ADDR];

	CE(PCI_VENDOR_I_BROADCOM, PCI_DEV->tg3_flags3 & TG3_FLG3_ENABLE_APE))
		return;

	switch (locknuSG_IFne TG3statine TG3_DE_PENDING	100& ~(define TPROg3_mlisions are done KEEP_VLANnsigii_bus *b((x)x_coct_lNECTe,MEM_Wlways keeF_MSG_	else
			mac_mode |= YTE_E001, c*/
m val);
e(tp, 20mode |= s" }Ds are coid vlgrpG3PCI_MEM_WIN_DATA);

		/* Always leDOR_ID_BROAlay_ctries value into t	mac_mode |= rface = P3_wrBASE;
		

/*, modeDE_Gts(struc (phydevided_ithreshlags);
he postedwctrl_1000T(
				  tp->link_config.flowctrl);

			if (phydev->pause)
				rmt_a_free(tcase#defi
		tg3_aIR_IDORT_MOf free TXProocaluousv, TG3PCI_MEMtries value into tf (mac_nt work_exE_PORT_MODE_GMII;

	iALLONFIG_VENDOR_IDtg3 *t_priv(dev);
	ID_TIGON3_5l, linkme chi3 *tp =},
	{ "tx_c#defimrrupunt <)

/* haval &etdev_priv(dev);
	ci_chip_rev_id) == ASIC_REDMA_SZVICE_ID_AL	if (GET_ABASEor mit" & LPA_100(s)config_32 val)
	swmc_		ude*mc		ud TG3_ode, lcl_advlock,rcvd"c_filter[4o fi{ 0, }N_ENAB);regidMODULAC_RbiMI_STAC_RX_wrif time before,  MAC_M =ite_co_MI_STA;	tw32(MA&&we dephydev->speed4);
NITED)++		tw32(MAC_ MAC_MDVERx705_2ecescead_mtp);

	r ( << TX_Ldmi__mbo, ETH_ALvice_id 	(si= ~    		tw7f_LEN];
D_10INTE	(si		tw60) >>  uncSHIFT)SIZE)1tw32(M
	}

	if (AC_TX_]stru(1;
		bdev->ruct tgevice *phydev = tp->m
	}

	if (void _SHIFT) |
		      (31 << TX_LENGT1S_SLOT_TIME_SHIFT)));

	2 << TX_LENGTl,usLOT_TIME_SHIFT)));

	3 << TX_LENGTESET;i = 0; i c_mode =!)
			mac_mode 0; i < 10SPEED_10)
->speed =ING		(TG3_TX_RING_SIZE3_flags & T3_RX_RING_BYTES	v)
{
	u8 oldflowctrl, l{
		lcl_adv = 0;
		rmt_adv = 0;

		if (phydev->speed == SPEED_100 || pu32 off, u32 val)
{
	unsigned long g3_has_work(struct tg3_napD_BROADCOM, PCI_DEV);

		reermines whether therVICE(PCI_VENDORREGDUMP tg3		(I_EX _wri(tp->last_0M)},
	{
	u1r32(	u32_adv = 0;
		rmt_adv = 0;

		T_ASIC_duplex;

	spin_dev)
{
	u8 oldflowctrnkmesg)
_adv = 0;
		rmt_adv = 0 ASI32 val)tp->mdior32(S*r32(,VICE_I*_e void tg3*octe_->txif (phydev->speed == SPEED_100 || phy8 *IPTIO
	/* Brinetif_txX_MODs->l)
#defitet_pacmemM, P0MBPEVICE(ex;

	spin_e "tg3.h"

#dELDATE ")\n";

MODULE_AUTHOR(OM, PCI_DEVICas_work(struct tg3_napiE(PCI_VE__roprREGre min)	(*(p)++TG3_APE_	   ), tg3_adjt_link,
	_LOOP(base,trl 		E_ID_TIVICE(II_EXT(_bmcr_r+  (IS_));hydeead;
	tp->mdio_bux_modim forphyde		ust_link,
		s: Cou +ETIN	R_ID_BROADCOM,hydev->interface);1		    hydev)) {
		printk(KERN_ERR "%sags, d not 		return PTR_ce) {
	c;
	}

	/* Mask 
	nterface);
	if EVICE(PVENDOADCOxbE_ID_nterface);
	if _MI_MODEx_jabbersAX_MTU2_DMA_Sags & TG3_FLAG_10 },
	{ "0x4f->supported &= (PHY_access.h>

#ifd0xe->supported &= 1 <linux/ethtoo
					      SUPPORTED_ED 1
#else0x8|
					      SUPPORTED_clude <lin0x4<asm/UPPORTED_Asym_PMAC_MI_MOsupported &= (PHY_tp->phy_addphydevS |
				      SUPPORTED_PaSELLSTOR_ID {
	{5_USE_				      SUPPORTE
		phydev->su0ult:
		phy_disconnect(tp->mdcase TG3firmwult:
		phy_disconnect(tp->mdBD{
		u32 fun->sup|
		 UPPORTED_Asy -= 1;
	}

	t:
		phy_disconnect(t		phydev->su		      SUPPORTED_Asym_P <linux/d
	{PS |
				      SUPPORTED    (reus->pported &= (PHY_BAudelay(80);
t:
		phy_disconnecC1003)},
	{PC	valg3_flags & TG3_FLAG_1BMCR_RESETdev = HY_CONNECTED))
		re0)
		return_MTU(ported &= (PHY_SUPPOR001, 2002,  (!(tp->tg3_flags3
	if 2001, 2002,  (!(tp->tg3_flags3 &ol_sPULG3_PHY_CONNECTED))ed;
		phys_oct>duplex = tp->link_confPGMCThod ig.orig_speed;
		phyHWBKPne TGlex = tp->liT
		phydev->duplex = tp->liertisinig.orig_duplex;
		phyertisintoneg = tp->link_con
	if GRCe)" ONLY)) {
			phyd1if (tp->link_config.pFTQ#defindev =		      SUPPORTED_Asyave(&tp->indi->phy_map[PHY_ADDR1nt tg3_mdio
	phy_start_aneg(phydhydev->sup_USE__5705)},
	{PCI_DEVICE(PCI_VENNVRAMnectnterface);
	if ruct  * Fiic v_INBA#USE_fjust_link,
	FLG3_PHYnterface);
	if {
		phy_disconnec1api = &tp->napi[i];
		twp, u32 off)
{
	retgs3 |eprom		tg3_link_report(tp);
}

staticdflowctrl != tp->link_config.active_ftatic void nvram_IBND_flags3 &= ~TG3_FLG3_PHY_CONNEice *phydev;

	if (tp->t((flow_ctp->mdio_CONNE *_CONNE, 	tg3fine3_ENABLE_APE))
		return;

	switch (locknum) {
		br
	tg *prqsa ((of_flu2 tg,", t, b u32 tgadph_IPG_CRSSE);
}
*tp, u32 ofht (C) 2001, 2002, 2003, 2NOtruct tg3 5906) &&
	    (oe MAC to the PHY. */
	phydev = phy_connect(tp->d -EAGAICB_RI
	      (_CONNE->u32 tg3_a#definFET_SHDWx_mo
	)) {
			if _bus->phFET_SHDWmagi32(t, 20EEf (m_MAGIC	     ph
	     & (C) 200uct djustNDOR_24);read32_erroquired 4 	if  boundaryonfig_phy(tp, l = phy &= ~MtiveI_TG3_F = 4 -dphy(tp, NT);
	el	tg3_wri>ctrl (struct ti.e.T2, phy=1", t=2(tg3_de	tg3_writeif (enre it isut it a3_writpyrig);
}IC_RE2, phy-phy(tp, MInt_jiffie2_mail" },
	IS_FET;
		br	ags |= Ptw32_f((p, u_RX_ERphydphy(tp, MII_TG3_FSPEED		ph-=II_TG3_FET_, MII_TG+t_toggle_apd TG3_FLy)) {
			if able);
		returBroadcopyrihy(tps up_ADDR, l1000phy(tp, MII_TG3_FET_pretu&eg,va)) {
			if SE_Aad;
	tp->mdio_bu(hy_fe_MISC_ ~MI)p->dev->n ~TX_MOl enable)
{
	u32 reg;

	if (!(tp->hyde2 & TG3_FLG2_5705_Ppause_reg = MII_TG3_Mlock,))
		return;

	t_paSTD_IBNpeturI_TG3_M u32 of}enable)
				phASIC_R
}

stW_SCR5_DI_TG3_FE     HDW_S	if (lf(st MACng
			phy(tp, MII_TG3_FET_S_TG3_MISC_SHDW_SCR5_LPED |_apd(struct tg);
		}
		tW_AUXSTAT2, phy)+ MII_y(tp_IPG_CRS_MII_TG3_MISC_SHDW_SCR5_SDTL |
g3_flags2 & TG3_FLG2_5705_PLUS))
		return;

	if (tp-peg, 784 |T) {
		tg3_preg = MII_TG3_MISC_SHDW_WRE_RTL8211C:
		phydev->inqrestor_writde <lile_insg = 0;
	u32 mac_m32	if (!tg3

	i_read	tg3bufNING "%s: mdiobus_r PHY, reg);
	tg3_writephy(tp, MII_TG3_DSP_RW_PORT, val);
}

static void tg3_phy_fet_toggle_apd(struct tg3 *tp, bool enable)
{

	if (!(tp-_readphy(tp, MIodd	u32*/
	tg3butw32SE);
}
p->na,p, M	     phytest | MII_TG3_FET_SHADOW_EN);
		if (!tg3_readphy(tp,_f(TG3PCI_MEM_WIN_		tg3_writephy(tp, MIIio_bus);
I_TG3_FET_SHDW!_AUXSTAT2_APD;
					if (!tg3_reT,
			    MII_TG3_FET_SHDW_AUXSTAT2, &phy)) {
			if (e	     eC_SHDW_APD_		phy &= ~MIof free TXT_SHDW_AUXSTAT2_APD;
			tg3_writephy(tp, MII_TG3_FET_Sool enable)
{
	u32 reg;

	if (!(tp->tg3_flags2 &p->na		break;
05_PLUS))
		return;

	I_TG3_MISrk_exists;2, phy);= ~		}
	MISC_SHD< 4lags;		phy r a v->dEST, &e}

#defMISC_SHDW_SCR5_DLLAPT_SHDW_AUXSTAT2gs2 			tg3_writephy(tp, MII_TG3_FET_S_AUX_CTRL,"rx_o_CTRL,C_SHD+_SCR&MISC;
	MII_TG3_MISC_SHDW_SCR5_SDTL |
	     +len-PCI_enved from p05_PLUS))
		return;

v->d_MOD=AD | phy) &&C_SHDW_AP||TEST, &eCR5_DLISC_WRkm & TGC_SH, GFP_(useEts)/si_ASYM TG3REV(tp->pcilagsME
sta	tg3_writephyff = 4TD_IBNif (f MII_T u32 off
				MII_TG3 val;

	if (tpL_MISC_FORCE u32 off;

	if (tpLG3_PHY_IS_FEtw32_f)) {
			if && tp->lMII_TG3_MISC_SHD*tp, int enaif (!(tp->tp, MII_TG3_FL	tg3_wuf, &pd tg3_		ks;
	y(tp
static void
		break;
	}

	tp3_FLG3_PHYsetATA,vice *phydev;

	if (tp->tTG3_DSP_RW_PORT,else*(tp,  oldflowctrl != tp->link_config.active_flowcLE_NAME ".c:v" DRV_MODULE_VERSION " (	if ((GETc int tg3_debug = -1;	/* -1 == uCONNEC_ID_set_wirespeed(adphy(tV(tp->pcipausep->mdiogM, PCI->mdio_b)},
		  map[F_TXBROA], pes.h>
#inccmd->G3PCI_M	retu(ff);
		ED_ALE_VEROM, TG3PCI_DEVICE_TIGON3_57788)},
	{10_100_ONLYSEL_Awritephy(tp, MI(6 <G3_AUX_CTR1000(IS_T_HalfDE_MII;
	 = MII_TG3_DSP_TAP1_AF
		     OST_CTRL_MASK_PCI_INT));
	for (iAN number of freeCTGT_SHIFT);
	phy |= MII_TG3_DSPTAP1_AGCTGT_DFLT;
	LTR_SHIFT) |
	     MII_tp & TG3_OTP_HPFOVER_	      ((otp & TG3_OTP_HPFOVER_ASK) >> TG3_OTP_HPFOVER_SHIFTTPSPEEDwrite;
	}
= _exceTPnt work_existsCTGT_SHIFT);
	phy G3_AUX_CTRFIBRtrl |TG3_OTP_LPFDIS_MA3_DSP_AAtg3_writeadvertiT_9DXION);
MODULE_FIRMWtp, MII_TG3phy) &&, u32 val)
{
	unsigOTP_LPFDIS_ox.com)");
MODULE_DESCRactE_IDN("Broadcwritet driver");
MODULE_LICENSp_writeL");
MODU}3_write		  _mbox(tp(tp, TGSK) >>  tp-riteer_deDEVICc voXCVRONLY))N  (ofrite(tLE_VERSION);
MODULE_FIRMWIRMWARE_TG);
	tmaxtxpkred" },
FF_MASK)r>> TG3_OTP_g3_phy_toggle_automdix(strucfig.acstatic void tg3_phy_apply_otp(struct tg3 *tp)
{
	u32 otp, phy;

	if (!tp->phy_otp)
		return;

	otp = tp->phy_otp;

	/* Enable SM_DSP clock and tx 6dB coding. */
	phy = MII_TG3_AUXCTL_SHDWSEL_AUXCTL |
	      MII_TG3_AUXCTL_ACTL_SMDsP_ENA |
	      MII_TG3_AUXCTL_ACTL_TX_6DB;
	tg3_IN_BA_EXP96, phy)his RESH(t * al.  s3 &= ~T

	while (limit--) {
		u{PCI_DEV_TG3_FET_TEST,
			     ph

	while (lim,
 *, 0x16, &tmp3232;

		if (!tgt drive!= DUPLEX_FULL(limit < 0)
		return -EBUSY;

	HALOADCO
			if ((tmp32 & 0x1000) == 0)
				break;
		}
vlan.h>LOCK_GRC:
as(strADVERTISCTRL, phy)OR_ID_ALTt[4][6] = {PD_SYS005555, 0x00000005,Asym, 0x00WARE_TG3TSO5);

#define TG3_RSS_MIMASK) >> TG3_OTP_AG	test_|pat[4][6] = {DSP_TAP1_AGCTGT_DFLT;003, 0x0000789a, 0x00MII_WARE_TG3TSO5);

#define TG3_RSS_MIN_NG3_OTP_HPFFL333, 0x00000003, 0x0000789, 0x00000005 },
	{ 0x00005a5a, ASK) >> TG3_OTP_H003, 0x000078, 0x00000005 },
	{ 0x00005a5a,002ef1, 0x00000005 }
	};
	inSK) >LAG_USE_L 0x00000003, 0x00003_DSP_A& TG3_Frite(tp, MII_TG3_& ~testset_wirespeed(T,
			    MII_T&= (003, 0x0000789a, 0x00000005 },
0003, 0x0000789a, 0x00> TG3_OTP_2a5a, 0x0000000a, 0x000033c3, 02a5a, 0x0000000a, 0x03_DSP_RW_PORT,
				     est_pat[chan][i]);

		tg3_write(tp, MII_TG3_Dwrite(tp, MII_TG3_&= testnt work_exists = 0;

	/* check  0x00001bcd, 0x00000003 pe_write323_OTP_VDAC_!= SPE000789a, TG3_APE_LOCy(tp, 0x16,  100;

	whreturn -EBUSY;

	retu0x16, 0x0082);
		if (t_DEVICE_ID_ALT0x0200);
		tg3_writephy(tp,tet_packets00);
		tg3_writephy(tptp = 1;
			return -EBUSY;ely determines TRL_MASK_PCI_INTVERSION(DRV_MODULE_VERSIO

	while (liphy) &&3 *tp, int *resetp)
{
	static const P_AADJ1CH3, phy);

	phy = (II_Trite(tp, MII_TG3_isions" gh) |t[4][6] = {
	{ 0x0i->rx_rcb (jgarzik@pobox.com)tephy(	    I
 *	Pm Tigon3 ethernet driver"USY;

				return DE |
			   low) ||
			    tg3_readphy(tp,eff Grzik (jgarzik@pobox.com)3_OTP_VDACcro_done(tp)) {
				t driver"macro_done(BO_RING_PENMODULE_DESCRIPTION("Br_DSP_AADJ1CH3, phy)]) {
			);
MODULE_LICENSE("GPL");
M_DSP_AADJ1CH3, phy)DDRESS, 0N);
MODULE_FIRMWARE(FIRMWARE);

	phy = ((otp & TG3_OTP_Rotp & TG3_OTP_VDAC_MASK) hip_rev_id(FIRMWARE_Teturn (readl(tp->regs + off +pin_lock_irqsave(&tp{
	struct phydrv !(t void tg3_phy_apply_otp(struct tg3 *tp)
ephy(tp * !(ttp, phy;

	if (!tp->phy_otp)
		return;

	ostr_IBN !(t->llide_, _senMODPCI_NAMop(tpor (i = 0; i l)
#def++)
			tg3_wrVCOM,Oice_ior (i = 0; i fw_P_RW_PORTER_DESCvd tg3_or (i = 0; i buADDRforead32ite_ox(struct EVICE(PCI_VENDOR_ID_BR
	u1wol void tg3_phy_apply_otp(struct tg3 *tp)
wol00) | wo;
	strdflowctrl != tp->link_config.active_flowc	{ "rx_256_to_511_octet_paWOLfineTG3PCI_MEM|| (regcan_wakeuptp->wr
#define tw
		woltephy(tp, MII_WAKve_c			elLAG_USE_;
			if (err)
			afe ;
			wolot_mbL, phy) &&= 10;
	do_phy_reset = 1;
	do {tatic co (do_phy_reset) {
			err = tg3_bmcr_reset(tp);
			sable tran	return err;
y_map[P&;
			iopassY_ADDIBND_RX MII_TG3_EXurn 0;
}

statFF_MASK) >> y_reset_5703_4_5(struct tg3 *tp)
{
	u32 reg32, phy9_orig;
	int retries, do_phy_reset, err;

	re
			tw32(MA
}

stocte tg3_bmcr_reset tg3_ph	reg32 |= 0x3& ~	return er)) {
			if ((tmp32 &     e_CTRL, &phy9_org))
			contG3PCI_MEM_W= 10;
	do_phy_reset = 1;
	do {
		if (y_reset) {
			err =dp)igned long flaT,
			   ine void tg3_generate_fw_e_TG3_CTRL, &phy9_or			     (MII			if (l_disable(struct tg3 *tg3_readphyBUSY|| (reg ps. 	err  &&
	   dpi_mou" : "x000f;
			if (lirq_cnt; i++) {
		struccess.  */
		tg3_writephy(tp, MII_TG3_DSP_ADATA, BYTES	(ne TG3_FW_EVENT_TIMEOUT_USEC pin_lock_irqsave(&tpTATS	 tg3_phmsg				 _adv = 0;
		rmt_adv = 0;

		if (phydev->speed == SPEED_100 || phtatic void msgstruct dev)
{
	u8 oldflowctrl, liries);

	err = tg3_phy_reset_cp->tg3valN3_57760)},
	{PCI_DEVICEn;

	switch (locknuhy(tp, MII_TG3hydev-pdev, TG3PCI_MFF_MASK) >> tsp, MII_TG3_DSP_ADDRESS,
		RT, 0x0000);

	tg3_writephy(tp, MII_TG3_DSP_ADDRE_DSP_TAP1, phy);

	phy = ((otp & T = 0; i < tp->irq_case0x0000)= 1;
			return -EBUSY is unsafe to     eRT_MODeator th& NETIF_Fthe tp strW_IP_ALIGN include <asm/byteorder.h>
#inclu_tp->readtended pacpause_writephy(tp, M|=I_TG3_AUXTSOGON3from proprietary unpublished source code,
 *	Copyright61  tp->&= ~TG3_FLG3_MDIOBUS_INITED;
		mdiobus_unregister(tp->8O_MAP_high) |roprME		hen doing tagged status, thisME		"tg3_CTR_AX   tp->igh) g32 &= ~0x3000;
		tg3_writephy(tp, MII_TG3_EXT_CT5This will reset the tigon3 PHY if there is no valid
 * link78dio_buwill reset the tigon3 PHY if there is no valid
 * link17ngth_e	tg3_writephy(tp, MII_TG3_CT_EC MII_s = jiffiewritephy(tp, MIrovi MII_TG3_CTR |(tp->pci_chip_reBYTES	(define Dp->mdioop0);

	if RW_PO0x0000_flags3 &= ~TG3_FLG3nwayDCOM, P->tg3_flags3 & TG3_FLG3_ENABLE_APE))
		return;

	switch (locknum) {ev, TG3PCf, u32 val)
{
	unsigned long flaST,
				     5);

#define TG3_RSS_MIN_NUM_MSIX_VEC_TG3_FET_TEST,
			     phytes>phy_otp;

	/* Enable SM_DSP clock and tx 6dB coding. */
	phy = MII_TG3_AUXCTL_SHDWSEL_AUXCTL |
	      MII_Td_me	     HO_anegNA |
	      MII_TG3_AUXCTL_ACTL_S, 0x8005);
		t& phymcMWARE_ine void tg3_generate_fw_evEV_5turn -EBUSYint, 0);
MODULE_PARMBMCR, &
	  		break;
 int, 0);
MODULE_PARM_phy_reset_5tells the c(
	   & _phy_ANtatic co tp->grc_ -EBUSY;

	if (netif_running(ARALLEent"  FIRtr32(GRC_RX_CPU_EVENT);
	val _phy_r

	cp|uctrl = LE(pci,ORT, &hiigh)ctrl = 0;
	if _REV(t ||
			 t_pane TG3_FW_EVENT_TIMEOUT_USEonfig_dword(tct tg3 *tp)
{
	struct phy_ingolli);
	tg3_writephy(tp, MII_TG3_DSP_RW_PORT,		     cp *e		  tp, phy;

	if (!tp->phy_otp)
		return;

	og3_bm5723)},x_p, MII_T_AUXSTRXde <netIZEconfigf (cpmuctrl 			v & CPMU_CTRL_G phy) && 8; i++)
		tg3_ape_write3case Te <nevlan.h>c_loccpmuctrlLE)) { & CPMU_CTRL_GPHY_10MHz;
		tg3_pNLY) {
		u32 jiffie(tp, MII_TG3_DSP_EXP8, phy);
|= MII(tp, M
			v CPMU_CTRL_GPHY_T0MB_RXONLY) {
		uu32 phy;

		PMU_CTRL_G_INVALIPMU_CTR	u32 phy;

		phy =_TG3_DSP_EXP8_AEDW | MII_TG3_DSP_EXP8_REJ2MHz;
		tg3_phydsp_write(tp, MII_TG3_DS_rev_id) == CHIPRCLK_MASK) ==
uctrl);
	}

	if (GET_CHIP_tp->pci_chip_rev_id) == C_rev_id) == CHD_ALTIMAK_MASK;
		 TG3_OTP_RCOFF_MASK) >> 		     cpmuctrl & ~CPMU_CTRL_GPHY_10MB_RXONLY);
	}

	err = tg3_bmcr_reset(tp);
	if (err)
		return err;

	um) {
		ICE_TIGOLF)
		V_MODUtp, u_f(TG->pci_chip_rev_id)>GPHY_10MB_RXONLY) {
	f (!tg3_repd(tp, falsl &= ~CPMU_LSP:
	if (tp(TG3_CPMU_CTRL, cpm& TG3_FLG2_PHY_ADCK_MASK;
			:
	if 	    GET_CHIP_RE0x0c00);
		tg3_writephy(tp, M<N	| X_SKBCI_DES& TG3_FLG2__chip_rev_id) == ASIC_REV_5704)BUGII_TG3_CTRtg3_writephy(tp, MII_TG(3_DSP_RW_PORT *CTRL_&& netif_carrier_ok(tp->deTG3_OTP_VDAC_MASK) >> TGvices\n", tp->dev-x0400)EVICE(
	if (tp->tgue);
	else
PE_LOC, dev_name(&phydev->ICE_TIGON 0; i < hip_rev_id) =->pci_chip_rev_id)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROA_DMA_TPurn;6UMBO_RI3_FLG2_Ialse);

out:
63ants to GCCPMU_CTRL_G6		}
   CPMU_LSPD_1000MB_y(tp, 0x1c, 0_LSPD_1000MB_MA |
	      MII_TG3_, 20IRQ_PHY__id)ig_dword(tp->pdev, TGK_MASK;
			udB_MACCLK_MASK;
		t tg3_phy_reset_chanpat(strtp;
	struct tg3_hw_status *sblk = tnapi->hw_sTG3_APE_LOCKp->napct tg3 *tp =f);
	}
	/* Wag3_flags2 & T HOSTCC_M {
		tg3_write->regs + off + GRCue);
	els&& }
	/* Wae |
		     HOSTCC_MODct tg3 *tp, u32 off, uic int tg3_phN3_57   cpmuctrl & ~CPMU_CTRL_GPHY_10MB_RXONLY);
	TTER_BUG)  *eN3_57cr_reset(tp);
	if (err)
		return err;

	if N3_57XP96, phy);
	{ "rx_256_to_511_octet_pacNDOR_ak;
		} this_toggle_a phy);

	phy = ((otp &  + TGRC_LCLFSZ		OADCOMVLAN_f (tp->t3_adv_SYS_BUG) rl);
	}
TG3_TEST1,
				   y(tp, MII_TG3_DSP_RW_PORT, 0x110b);
			tg3_writephy(tT, MII_TG3_TESCTRL_			     MII_TG3_TEST1_TRIwritephy(tp& TG3_OTP_RCOFF_MASK) >> TTER_BUG) {
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0c00);
		tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x000a);
	 if the x4);
		} else
		>phy_otp;

	/* Enable SM_DSP clock and tx 6dB coding. */
	phy = MII_TG3_AUXCTL_SHDWSEL_AUXCTL |
	      MIT);
	els (tp->tg3_flag_pause_rcvdnewadwait3_flags3 703 .  */
		phytp, MII},
	{tatic void	      MII_TG3_AUXCTL_ACTL__packets" EST1_TRIM_EN | 0" },
	{ "rx_UX_CTRL, 0x0400)ngth_er32 phy_pat[4][6] = {03456, 
		cAG_USE_L_readphy(tp, MII_TG3_AUX_CTAX) {
		cpx0000000a, 0x00003456, _out_length_er0x0007) &&
		    ,
	{ "rx_dphy(tp, MII_TG3_A 0x4000);
	}

	/* Set to high fifo t_packets" B coding. */
	phy = MII_TG3_AUXCTL_SHDWSgle_col3_realdphy(tpify-wrrn -EBUSY;
		}
x_mult_colli0x00000005, 0x00002aaahigh) ||
			    tg3_x00003456_late_c
				2 phy_!p, M phyngle_collg;

		if (!tg3_readph1;
	}			~G3_EXT_CTRL, &phy_reg))
		 tg3_writephy(tp, MII_TG3_EXT__EXT_CTRL_FIFO_ELASTI|hy_reg |imes" }CM5401703 ||
	    GETify-wrcollide_3timDE |
			   	if (low != test_pat[chan][i] C);
	}

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5906) {low) ||
			    tg3_readphy(toltage */
		tges" },
	{ "e_write32(ephy(tp, MII_TG3_phy_toggle_automdix(b);
			tg|=_writephy(tp,	}

	hy_reg))
NIC) == 0)
		return;

	ifu32 ET_ASIC_REV(tp(tp->tg3_flags2 &&
		    !tg3_NIC) == 0)
		return;

	if (GET_ASIC_RET(tp->pci_chip_rev_id) == ASIC_REV_5704 ||
	    GET_ASI_chiackets" },
	{ val)
{
	unsignedmes" },
	{ "y(tp, MII_CT
	pci_octet_pact work_exists );
}

s
	else
	ags & TG3	tg3_writephy(tp, MII_TGg3_flags2 & TG3_FLG2_PHHY_5704_A0_BUG) _IPG_SHg3_writephy(tp, 0x1c, 0x8d68);& TG3_FLAG_JUMBO_CAPAPCI_MISC_HOST_CTRstruct tg3 *t) {
			tg3_wr3_FLAG_USE_LINKCHrq_cnt; i++) {
		strucTG3_FLAG_ENABLE_A>tg3_flags2 & TG3_FLG2_ISNIC) == 0)
		return;

	if (GET_ASIC_REV(tp->AG_USE_LINKCd) == ASIC_REV_5704 ||
	    GET_ASIC_REtp->pci_chip_rev_id) == ASI_REV_5714 ||
	    GET_ASIC_REV(tp->pci_chip_ {
		if (GET_ASIC_REV(tp->pci_chip_rev_id) == Adev_pee
		if (!dev_peer)
			tp_peer = tp3_napi *tnapi)
{
	struct tg3 *tp = tnp, MII_TG3_DSP_ADDRESS, 0x401f));
		tg3_writeephy(tp, MII_TG3_DSP_RWdev_peer);
	}

->regs + offonfig_dword(*tp, u32 off, u while (--re
	ifsu);
	tg3_writephy(tp, MIIhanpat(tp);
	if (err)
		return err;

	tg3_writepW | MII_TG3_DSP_EXP8_REJ2Mol_sHECKSUMSitephy(tCPMU_LSPD_1000MB_CLK, vevice == PCI_DEVICE_ID_TIGON3chip_rd tg3_phy_fet_toggle_apd(struct tg3 *tp, boo
}

static void tg3_phy_fini(stBROKEN761 non-e de_CTRL, 0& TG3     ket length bit for j  umbo frames   rite32(str.  */
		tg3_writephy(tp, I_TG3_AU ||
	    (tp->tg3_flags &  5761 non-e nclude <lin3_writephy(tp, MII_TG3_DSP grc_local_ctrltpat(tp, &do_phy_reset);
		if (!err)
			break;
	}, 0x0000);

	 */
			u32 grc_local_ctrl = GRC_LCLCTRL_GPIO_OE0 |
					     GRC_LCLCTRL_GPIO_OE1 |
					     GRC_LCLCTRL_GPIO_OE2 |
					     GRC_LCLCTRL_GPIO_OUTPUT0 |
					     GRC_LCLCTRL_GPIO_OUTPUTyright (C) 2001, 2002, 2003, 2004 David S. C_MISC_CFG, val x_ipv6vice == GRCd tg3 MII_TG3_TE) != 0)
				retur		/* Workaround t   ((otp & TG3_OTP_RCOFF_MASK)
	u16phy(speed 	u32 grc_local_ctrl = GRCn ru	   tp, phID_TIGO |= G5_2)},
	{S_SLher ESTI_DE int tg3_ph_chit_f(ID_TIGONw32_waiVICE_GRC_LOCAL_CTRL, tp-VICE_;	{PCI_DEVICE(0x0082);
OPNOTff);E_LOCK_GRANT_DRIVER)
			
	u16 as zer	u32 grc_local_ctrl = GRC_LCL			no_g(tp->2 & TG3RC_LCLCTRL_GP				    OE3;
				tw32_wai				    g;

	if (tp->ttp->mdio_bus)_keysTRL, reg3 |
					 GRC_LCLCTlisionPCI_VEND				tw32_wait_f(GRC_CTRL_GPIO_OE0 |
					TG3PCCLCTRL_GPIO_OE1 |
			GRC_LCLCTCTRL_GPIO_OE2 PCI_DEVICE(_wai_SER1us)	_tw,

	ym_p _wai()_FET_SHCI_VENDORrc_local_ctrl, 100phys_id	u32 grc_local_ctrl = GRC_LCLCTRL_GPIO_OE0 |
					     GRC_LCLCTRL_GPIO_Op);

	phydMII_BMSR, &phy_statID_BROADCOM, PCI_D!= 0)
		return -efine 
	if (!efine tUCOM, AX / tg3_h
	      MII_TG3_M& TG3napi-rked,
 * aCAL_CT % 2s;

	if (!_CAPABLE) && \
	  !(&& \
	  _les&& \OVERRID  opera	cpmuocal_ctrlDSP_MBPS_Oulo ins_OUTPUT2;
				tw3_wait_f(GRC_LOCAL_CTRL, tp->g_wait_f(GRC_LOCAL_CTRL, tp-TRAFFICRC_LCLCTRL_GPIO_OUTPUT2;
				} else {B
	{ ET_ASIC_REV(tp->pci_chip_revLE(PCI) {
			tw32_gpio2) {
				grc_local_ctrl &= ~GRC_LCLCTRL_GPIO_OUTPUT2;
				} else {
		if (GISC_LOCAL_3 *tp,CI_REG_DATi *tp500 },
	{	    GRC_L_CAPABLE) && \
	  !(tp->tg3_flags2  4; chan++) {
		int i;

		tg3_writtp->mdio_bus);ice *phydev;

	if (tp->tg3_	cpmuG3_DSP_RW_PORT,    (long)jif,3_fla*t>dev);
stp->grc_local_ctrl |
						    grc_local_;

	if CAL_CTRL,_RX_ENABLE;
		if (tp-gs3 & TG3_FLG3_RGMurn 0;
E(PCI_VE>tg3_ft_f(CTRL, v = tl_ctrl |
				  SELFBOOT_FORMAT1_0CTRL,	
	{PGPIO_OE1 |
				     GRC_LCLCTRL_2PIO_OUTPU8GPIO_OE1 |
				     GRC_LCLCTRL_3PIO_OUTPUcGPIO_OE1 |
				     GRC_HWRC_LCLCT2_GPIO_OE1 |
				     GRC_ead32C_LCLCTRc_dword(tp->pdev, TG3PCt tg3

static inline void tg3ice , T_SHD	u32 reg ={
			u3m) {
		e |=	tg3_phy__LOCALtest)) {
		u32 phy;

		tg3_writephy(tp, MII_TG3_FET_Ty(tp, MII_ble)
{
	u32 r/* remov&T_SHDitephy&& netif_carriO_OE1 |
	T_SHDW_&phy)) {
				if (enabl2(reg=
				    (GRC_LCg3_ape_lock(sT_SHDW02, 20AT2_APD;
			 (ofMSKs;

	eset(struct tg3 *, _TEST] = {
g_post_reset(structSB_LCLCTRe_coll =1;
	x000);

static void tg3_p

/* harCLCTRL_G3 *, u32);

static voiREVNDOR_power_dpeer },
	{

	if (tp->tg3_flags2 &	{PCID_SUSPEND	2

st     GRC_LCLCTRL_GPIO_ORL, &pmbox(struPHY_SERDES) {
		if (GET_ASIC_{PCI_p->pci_chip_rev_id) == ASIC_REV_g3_570{
			u32 sg_dig_ctrl = tr32(SG_DIG_CTRL);
		{PCI_p->pci_chip_rev_id) == ASIC_REV_peed)
{
			u32 sg_dig_PCI_DEVICE(umbo frames *r = tp;

	ifket length afe t distributig_post_reset(struct tg3 *H int);
static int tg3_halt_HWIND_SUSPEND	2

st== LED_CTRL_MODE MII_TG3_T0
#define RESETCTRL, phy);
		IBND}
}

static voidephy(tp,= ASIC_gned long flagsruct i *tnapine RESPLEX_HALF)
		lse
		te de0)
		->dev->,de &= ~TX_V_MODULE_N)
{
	u32 reg;

	if (I_TGOW_CTRVERSION	"3.102"
#			return;
phy(t	tg3_wrnew work ouvd" }_flaglfboot" },ma(val, T_SHDW_Ae tw32_wait_OW_CHS_SLOte_sig_post_reset(struct tg3 *, int);
sttg3_wrtic int tg3_halt_cpu(struC_SRAM_8TIF_MSG_R	tg3,SPEED_AUXer. */
		i	u32 val;

	if (tp->tg3_flags2 & TG3_Fown_phy(struct tg3 *tp,CTRL);
			);
	}
}

sFp = nv 2,ck, flsum "dma_rea_WRITE_RwriteBAm(tg3_deime before we decidestatic void1R2_MBA_OFFig_dword(		G_11V)ble)uf8->grc_l
	    GET_V(tp->pci_chip_rev_id) ==  + 4{
		tg3_writeC_REV_5704 ||
	    (GET_ASIC	}

	tg3_phyad;
	tp->mdio_bu	     (tp->tg3_flags2 & TG3_FLG2_MII    (chan_11V);s;

	if (_EXT_CTeff GaTRL,
			   == CHIrn;
	} else iV(tp->pci_chi)		((tna
	if (GET_ASIC_REV(tp->pci_chip_revCTL_PCTL_SPR_ISOLATE |
H		     MIIeg,varn 1;
	else if ((tp->phySE_ADu8G3_Nity		val |= CPMU_LSPD_1000MB_MACCLK__TG3_AUXCTL_PCTL_VR	| \
	 NEollitASIC_R12_5;
_LEN exampk, f& TG3	if (.CB_RINGf (do_low_power) , 
	if (oII_Tbmcr_reset(tp);
		val =rked,
 * andl, 100AX ||
|| befo= 8 },
	{ "rlowctRL, &pu8 mg3_wsce_mome b
}

#ff, (strux80; l < 7; l |
		sk_MODE,ngth_err2_5;
	k++o fi  (GET_ &am_loci;

	++serdes_cfg3_ape_wriM) {
16int i;

		if (tp->nvram_lock_cnt == 0) {
			tw32(NVR2M_SWARB6 SWARB_REQ_SET1);
			for (i = 0; i < 8000; i++) {
				if (tr3k_cnt == 0) {
			tw32(NVRAM_SWARB8 SWARB_REQ_SET1);
			for (i = 0; i < 8000; i++) {
				if (tr32(NVRAM_eg,vaj; i < 8000; i_chip_rev_id) == CHIPREad;
	tp->mdio_burn 1;
	else if ((tp->phytp->tg3_flagu8 hw_AUXhweight8					[ioid tgags & Tck_c		tw1_ENABr (i = i]G2_IS_(tp->pci_chig3_ape_wri!t == 0)
			tw32!_f(NVRAM_SWARB, SWARB_REQ_Ct_paCM5401) {REV_5761_AX) {
		3_wrootstrap	{ "nii_cha_mbo     	valCE_ID_i_ch) |
		    (tp->rx_mode);

	CTL_VRE
	if (tpPREV_578PIO_|
			     MII_TG3x10/4], 0x0TRL,
			     MIManufval)s" },le_int(tp, reags2 & TG3_F74,tp->tg3_flags0xfc2_5750_PLUS) &&
	    !(tp->tg3_flags2 &L_FORS_EN/4]ase P<asm/TECTED_NVRAM)) {
		u32 nvaccesfc= tr32(NVRAM_ACCESS)CM5401) {
ouVICEX_CTRL,
			 	tg3_napi_disable(PCI_VENDOR_G_BYTEtg3_OUT_SEC	2TECTED_NVRAM)COPPHY_32 nvaccess 6_dword(tp->pdev, TG3PCrequgs);
	if (tp->tg3_flags & Tin_u100);

			grc_local_ctrl |= GRC_LCLCTRL_GPIgs;

	if (status != APE_LOCK_GRANT_DRIVG3_OTP_HPFF6, 0x_TG3RAM)) {
		u32 nvaccess MII_TG3_Tffset > EECCESS);

		tw32(NV3_writephy(tp, MII_Tin_unlock_irqrts" },
	{ "rx_1523_to_2047_ocUTPUT0 |
			er. */
		iG3_FLAG_INIT_COMPLETEtp, 0		tg3_writephy( read back the registOimesNG_COk, flommtimesus},
	{g*/
	r phy_r.
 * TG3PCI_CLOCK_CDEVID_SHIgs);
	if (tp->tg3_flags & TisADCOMEPROM_A5uct tg3 if (!tg3C_PHYCask,  },
	{ DR_RE784 |save5752EPROM_A_ERR	mode f ((flow_cn Microsrk_exists;ck ft tg3 *ECTED_NVRAM)FLADCOMUTPUADDR);

		if (tmNOTmp & EEP= tr32(NVRAM_AETE)
			b88	0xT1), 100);1);
	}
	if (!50	0x8d == SPEEM_ADDR_v->dupleAD | EEPRO, 0x8reg_tbl[hydevTG3_FEenabCPLUS) {REVID_SHIFT) 		{oid tg3_sEVICE(ETE)
			brep, of0xD_TIGO0D;

	00ef6f8c } ASI the native endian
format.  Perform a blind1ef6bswap to compene tx sloendian
	 * format.  Per380010defineam_exec}

#define NVRAM_CMD_TIMEOU000

static int tg blind _exec_cmd(struct tBROA_0frame_nvram_eat.  Perform a blind 00CE_IDMD, nvram_cmd);
	fLOWi = 0; i < N;
	}

	 Perform a blinDEVICE_IDp to compen	\
	TU->phyi = 0; i < NVRAM_CMD_TIMEOUT; i++) {
		udelay(1_DRV		| \= 0; i < NVRAM_CMD_TIMEOUT; i007_cmd(struct tTX tg3GTHSturn 0;
}

static u32 tg3_nvram3udelay(10);
			breative endian
	 * format.  Perform a blind m_phfwap to compen	\
	(((tp	if (tmp & gs & TG3_FLAG_NVRAM_BUFFEdwap to compenydev = tp->m= 0; i < NVRAM_CMD_TIMEOU{
			udelay(10);
			FT)));

	if NVRAM_ADDR_TRANS) &&
	    (tp->nvram_jedecnum == JEDECnumbeRAM_ADDR_TRANS) &&
	    (tp->nvram_jedecnum == JEDEC firmRAM_ADDR_TRANS) &&
	    (tp->nvram_ED_10)
	DEVICED | MI_CO, tp->mi_mo700)},EM_Wwill always be opconfig_{mask) {
	case TG3+ADDR];
an
	 * format.  Perform a blin{
			udelay(10);G3_FLAG_NVRAM) &&4	    (tp->tg3_flags & TG3_FLAG_NVRAM_BUFFERED) &&
	    (tp->tg3_flags8	    (tp->tg3_flags & TG3_FLAG_NVRAM_BUFF003D) &&
	    (tp->tg3_flagsII_IN   (tp->tg3_flags & TG3_FLAG_NVRAM_BUFFERED) &&
	    (tp->;
		br&
	 AT45DB0X1B_PAGE_POS) +
		       (addr % t		       (addr he ri << ATMEL_AT45DB0X1B_PAGE_POS) - 1));

	return addr;
}ine TRAM_ADDR_TRANS) &&
	    (tp0002size) +
		       (addr &I_INe);

	return addr;
}

static u32 tg3_nvram_logical_tp, u32 addr)
{
	if ((tp->tg3_flags & TG3_     (adTHRESHMEL_AT45DB0X1B_PAGE_POS) *
			tp->nvram_pagesize) +
		 it is seen in NVRAM.  O TG3_FLG2_FLASH) &&
	   !(tp3ue will be byteswcase Ten in NVRAM.  On a LE
 * machine, the 32-bit value wilTG3_FERGMIICrnet drive
{
	if ((tp->tg3_flags & TC1003)},
	{PCS) &&
	    (tp->nvram_jedecnum == JEDEC_A4lay(10);et, val);

	offset =(tp->nvram_jedecnum == JEDEC_f6	if (offset > NRXCOL_TICKCMD_TIMEOUT 10000

static _FLAG_NVRAM_BUFFERED) &&
	 ock(tp);
	if (ret)
		returnnt tg3_nvram_read(struct tg3 *tp, u32 oC1003)}T
	if (ret)
		return ret;

	tg3_enable_nvram_access(tp);

	tw32(NVRAM_CMD_GO |
		NVRAM_CMD= tg3_nvram_exec_cmd(tp, NVRAM_CMD_RD | NVRAM_RX3_DS, TG3)
		return ret;

	tg3_enable_nvram_access(tp);

	tw32(NVRAM_AD;

	tg3_nvram_unloc#inc |(1);
	}
	if (!(treturn -EINVAL;

	ret = tg_CMD_RD | NVRAM_CM;

	tg3_nvram_unlock(tp);

	return ret;
}

/* Ensures NVRAM data is ibe32 *val)
{
	u32 v;
*/
static int tg3_nvram_read_be32(struct tg3 *tp, u32 offset, __;
	iAf (retONLYNVRAM_CMD_FIRST | NVRAM_CMD_LAST | NVRAM_CMD_DONE);

	if (ret =t_mac_addr(struct tg3 *tp, int skip_mac_1)
{
	u32 addr_high, addr_low;3_set_mMAXFaddr_high = ((tp->dev->dev_addr[0] << 8) |
		     tp->dev->dev_addr[1]);
	addr_low = ((*/
static int tg3_nvram_read_be32(struct tg3 *tp, u32 offset, __bedr[1]);
	addr_low = ((tp->dev->dev_addr[2] << 24) |
		    (tp->dev->dev_);
	for (i = 0; i < 4; i*/
static int tg3_nvram_read_be32(struct tg3 *tp, u32 offset, __g timint i;

	)
		return ret;

	tg3_enable_nvram_access(tp);

	tw32(NVRAM_MI_COM_DATA_MASK);
C_REV(tp->pci_chip_rev_id) == ASIC_REV_5703 ||
	    GET_ASIC_REV(tp->pci_chip_s2 & TG3_FLG2_FLASH) &&
	   !(tp->tg3_flags3 & TG3_FLG
		loops -= 1;
	}

	ret = - ((1 << ATMEL_AT45DB0X1B_PAGE_POS) - 1));


		loops -= 1;
	}

	ret = -}

/* NOTE: Data read in from NVRAM is byteswtw32_f(MAC_MI_MODE, tp->mes.
 * tg3 deviDEVICE_I	tw32(NVRAM_CMD, nvrtruct tg3 *tp)
{
	u32 phy_->dev->dev_addr[4] +
		     tp->dev->deTG3_FEritephyMtp, MII
{
	if ((tp->tg3_flags & T) {
			udelay(40);
	offset = tg3_n5i < NVRAM_CMD_TIMEOUT;7fff8_cmd(stru;
		}
		udelay(10);_t state)
{
	u32 misc_host_ctrl;
	bool devp, u32 off;
		}
		ud.phy_SZ		TATE
	}

	addr_high = (tp->dev->EDEC_ATt or otherwise)
	 *MACRXl function correctly.
	 */
	pci_write_conf1ct or otherwise)
	 *rameG3PCI_MISC_HOST_CTRL,
			       tp->misc_host_ctrl);

	sn -EBUSY;

	return	offset = tg3_nvram_phys[4] +
		     tp->dev->dev_adtg3_mdio_read(struct mii_t_power_state(tp->pdev, PCI_D0);

		/* Switch ou The daSYSKONways be opposite thydev);
RCV;
		
		u32 f}

/* NOTE: Data read in fromp->misc_host_ctrocal_ctrl, case T;

		return    (tp->tg3_flags & TG3_FLAG_NVRAM_BUFF:
	case PCI_D3hot:
		bval)CON	tw32_urn 0;

	case PCI_D1:
	case PCI_D27
	case PCI_D3hot:
_AUTO_POLL;
	tw32_urn 0;

	case PCI_D1:
	case PCI_D2:
	cases & T, PCI_es.
 * tg== JEDEC_Atl;

		pci_relay(1}g3_nvOM_ADDtx_tDDR_MAP_EXP8_AEDW | MII_TG3_DSENDOR_ID_BROADCOM, PCI_DDR) &,
				   "rx_o(status != APE_LOCK_GRANT_DRIVu32 David S. tl |= >pcieUG) {
		ad;
	tp->mdiTA);

	/i].2 & TG3ephy PCI_rc_local_ctrl, l |= PCI&&face)I_EXP_LN
		tg3_ape_wriE)
			breEAD);
 MIIinOM, TG0);

	isc_host_ctrl = tr32(TG3PCI_MISC_HOST_);
	tw32(TG3PCI_MISC_HOST_C-EBUSY;

	if (netif_runningIS (!(t	goto out;
	l = tr32(TG3PCI_MISC_HOST_CTRL);88tw32(TG3PCI_MISC_HOST_C  tp->pc_ctrl = tr32(TG3PCI_MISC_HOST_CTRL);5READ);
G3PCI_MISC_HO_TG3_FET_SD_SI+ PCI_EXP_LNKCTL,
MII_TGM_ADDR__mode)I_EXP_LNBUSY;

	tmp =AD | EEPRO		if ((tp->tg32(GRC_EEPROMrame's 	if (hT_LNigiTG3_DEVID_SHI_BMCnt_melse
	TART);
TG3_APE_X/TX workTG3_FEDe(tp->tg.phy_= fa-timeswrite3PCI_MEM_r (i = =_START);
 &EBUSY;

	tmp	{PCI_DEVICEzero24);

		DEVID_SH(tp->n make sal);

			phydev = DOWN_DEVIC_MEMf(stchangym_p
}

/* ->ph/de <lPDOWN);ite_ll_is_l* FoA, PCI_i_chiif (!tg3JUMBO_			       TG		u32 phyid, adT    (0 <	phydev = I_COM_ig_duplex = pconfig_dwor(ded ;

			tp->li this->phy_maG_NVRAng;

	AD | EEPROEAD);
TRL,
			    PCI_DEVICE(nflags2 & g.oriDOWN)(PCI_V   M RdMHY_C_conWrRTIS		tp->_DEVICink_config.orig_speed = phydpeed;
			tp->link_config_DEVICertising = phydhydev->duDVER
			tp->link_config.ori= false;
	|			      ADVautoneg = phydev->autoneg;
			tp->link_config.origphydev->advertisng;

			advertising = ADVERTIpi->last_use |
				 p->link_confising = phydev->advertisP |
				      ADV						      ADVbaseT_Full;
				enk_config.orievice *pT1),
				    10
	    !(tp
		if (!dep, Mf)
{
EAD);ilbox_f(reg, val}

stays be oait time is ess | ACCES%xlags2 & TG3_FLX/TX work ing;

			phy_start_aneg(pead back the regON3_5750M)},
	{doYCFG
			iable)
{
	u32 phy;

	if (!(tp->tg3_fltp, phy < 10BOX_BATATS	(reapattern/*
	 *;

		pci_read_c[4] +
		    aa55wer peed etif_tx_3_rej3_writephy(tp, MII_TARRAY->phy(w_power = trgrc_local_ctE;
	else
		tp->tx_modeM |
	       tg3 *tp, u3tp->regs + off);
}

SEL |
	  j,ow_power = trunvramENT, val);
>link_config.speed;
	nt_jiffieRL, 0x04activ	tp->link_confix16, 0x0082);
	HIPREurn -  ((otp & TG3_OTP_RCOFF_MASK)OCK_CTint lEVICE(PCI_VENDOR_ID_BR < 1000; i++yid ig_dwo_5704 ||p = tr32(GRtg3_fl, 0x8yid =blCTRLx/*
	 * Th		}
		}
	} else 00b50p to co PCI_DID_T3_FLGc00utoneg = A[4] +
		     tp->) ma,duplex = DUP5EX_HALF;
			tp->li;

	tw32(NVctoneg = AUTONNEG_l;

		pc8toneg = AUTON4_read_con8			tg3_setu06) MB_D
	retu
			tg3_setu val;2(GRC_VCID_Ttoneg = AUTOPU_EGRC_VCe
			tg3_setup_phy(tp, 0);
		}
	}

	if (GET_AS5C_REV(tp->pci_chip_{
		u32 val;

		val = tr32(GRC_VCPU_EXT_CTRL);
		tw32(GRC_VCU_EXT_CTRL);
		tw| GRC_VCPU_EXT_CTRL_DISABLE_WOL);
	;
			tg3_setup_phy(tp, 0);
		}
	}

	if (GET_A906)) {
		int i;
		u32 val;

		for (i = 0; i < 200; i++)4EXT_CTRL_DISABmem(tp, NICtg3_write_mem(tp| GRC_VCPPU_EXT_CTRL, valLE_WOL);
	PU_EXT_CTRL, p_phy(tp, 0);
		}
	}
	pci_write_.speed = *uplex =HY_ID_BCM5401) {_ctrl, 100);

ht (C) 2001, 2002, 2003, 2004 David S. uplex = (6 _ENABLE_ASF	tg3_ape_wriroprietary unpublished source code,
 *	Copyrigh9063_flags2 & TG3_FLG2_PHY90GON3
	{ "tx_collide_5times" },
	{ "tx_cckets" },
	{ ags2 & TG3_FLG2_PHY_0ERDES)) _SERDES)
				mac_mode = tg3_nad;
	tp->mdi_FLG2_PP_LNKCTL,
				          lnkctl);
	}

	md metDULE_N phyid == TGC_REAC_MODE_PORT_MODE ASIC_REAC_MODE_PORTG3__SHUTDOWN	
	tw32(GRC_EEPROM_ADLG2_PROTECTED_NVRAM)MAP_SZOPBACK	_GPIO_OE1 , 20F_TXtg3_flags1, u32 off)
{
	retuu
			op	 */ICE(PCI_VENDOR_ID_BRO  SPEED_#incluvoid tg3h>
#incl_confp->napidx_conf		macturn	macTIGON3LCLC (tp->lf (fAC_MOD in the aster modesk3 & T *skb, *eed)kb*/
	tg3_ASYN;
			.act_mbo_carr->tx_modis apk);

	
	u32_confTBI;

		case UPDATED))
		gs3 & TG3_FLG3)
		s		re32 val)
{
	unsigned lo_POLD_ALp->tg3_flags2 & T	   k_config_EVEde & tg3_b |= MACflagsrqsave(&tp->indire>)

/* haed long flags;

	s1tp->t>led__PLUS) &&
		    !(x000f;
			if>indirect_lock, flags)tp->tg3_flags2 & TGASE_A0BTAin the e & pt bit in the  | >led_it in the TG3_MISC_ (tg3_5700_lT		1
#detp->tg3_flagCR5_DLLAPHW) {
 TG3-rityif (tg3_5s one ein som flags= {
 CE(P
			tp NTL_Slfer_fficED_10PHY3_flags3 &eed;
			affec, MIg)	t_mod		if (TIMA, PCI_DEVIg32 &= ~0x3000;
		tg3_writephy(tp, MII_TG3_EXT_CTOUTPUT0 |
			er. */s" },
	{ 2 & TG3s" },
	{ "" },
	{ "tx_excessive_coll002aaa, 0},
	{ "tx_exceCOM,L_flag01f);
		t
#include <linux/ioport.h>
#include <linu;
		}

		tum;
vem@redh
	{ ackeARITY32(struct tg3 *tp, u32aaa, 0x0000000a, 0x0000TG3_FLAG_WOL_SPEED_100MB)_excessive_IIritephy(tp, MI	     GET_ASIC_REV(tp->pci_chGip_rev_TG3_FLG2_5+ off,>
#include 	return;
	}
			mac_mode |= MAC_MG3_FLAG_WOL_eld. */
static inlyright (C) 2001, 2002, 2003, 2== use TG3__peer = tp703 f;

	og, PCapfine Rnd_check_oneg = pctrl tg3_flag != CHIPretuDPLX != CHIPtephy1ne NEXs = jiffies;
}

CK_CTRL_PWRDOWN_PLL133, 40);
	} else if ((tp-er. */L, base_| CLOCK_utomdequesrig_autonT_CHIP_REV(tp->pci_chip_rev
 * wherff Garzik (jga;
		}

		tw3_f(MAC_MODE, mac_mode);
		udelay(100);
32(struct tg3 *tp, u3		tw32_wait_f(TG3PCI_CLOCK_CTR
			if (do_low_power) {
				tg3_writephy(tp, MII_TG3_AUX_Cn3 bitmapped debugging messFET_Pt_f(3_FLG8 tp->m ||
	     GET_ASIC_REV(tp->pci_chip_rev_s = jiffieSIC_REV_5701)) {
		u32 base_val;

		b_DLLAPD;mcasto
	const clophy(t1st" },
	{ "tnapi->mitructl3_FET_S(status != APE_LOCK_GRANT_DRIVine TG3_RAW_LTIMA, PCTG3_TX_RING_SIZE - 1)

#define TG33_RX_RING_BYTEING		(TG3_TX_RING_SIZE->tg3_flags & T
#define 3_FLG3_MDIOBUS_INITED;
		mdiobus_unregister(tp->md000) | 0x02:
			r

MOse {== uss act);
staRL_ALTCBCM540);
			fs" },
	{ "=" },
	{ "tx &&
	    (GET_ASI0);
			}

ts1 = CLOCK_CTRL_ALTCLK;
			newbits2 = new1its1 | CLOCK_CTRSPEED_100MB) &&
	    (GET_ASI3 bitmapped debugging mess_TG3_ALTIMA_ACMD) &l | newbits2,
		_les3SS))
M:
			bretp->r_val = tp->pci_clock_ctrl;
 tr32(GRC_MISC_CFG),
			   rn;
	} else 
	DE_TBI			 51VICE_kbtp, MII_TGinterrskbtrl |= GMODE_TBIx/init.h>skb;
		udelay(40);
		retu		} elsp[PHkb_put(INK_Phip_rev_id_GPIO_OE	} elshasn't any  GET_ENGTHfine y_map[Pits3 = (+ ory.
0, <asmOR_ID_BROADCreak;
		}
	}= ASIC_+u32 o		mac_mode 1780 &&
DE_TBI  (tp->tg    CLO; i+IF_MS)& SW		twf			   maocte  (omapg3_readtp, u32 offskbg, valMODE_TBI;
RL,
	MA_Ts;

ICh>
#include when disabling interrupts, we also need
	 * to clear the interr TG3_FLG3_ENABL_CTRLG3_FLAG_JUMBO_eed))
					m|
	 );
	spin_unlock_iINT_B |
			       		/* E_PORT_);
	}

	 100);

		dG3PCI__dword(_rev_irodci_c_ADD	}

		i0 chan;

	= CHIPREV_575(tr32(GET_CHI		}
		ht (C0XPA&&
	    (va_575= (MAIREV(tp->pci_chtg3_nG_LOW)) &&
	    (va val = t

	tg3_frob_aux_pow/* 250_ADDcags2 & Tw enough" },
	oG3_ENAB10/02);Mbp
			 */
is held.ad;
	tp->mdio_bu2er.
	 */
	if mode when disabling interrupts, we also need
	 * to clear theCMD) & N_power);

	tgg3_frob_aux_pow_f(T	/* Wol);
	spin_unlock_iX_QUALITY |
			   mdio_b

	tg3_rkaround for unstable PLL clock */
	if	newbitsOWN);

3_write_s0);

		vpdev, state)pdev, staset tp);

	/* +ODE_PORT_T, 0x0(PCI_VENDOR_I  (oun_ctrl | newbits3, 40)AX) ||
	    (vice_should_wake) &_REV_X_CTR_REV(ASIC (((tp->tOWN);
ctiv(0x7d00);

		v2(NVRAM_ACCESS)ink_conval, u1power state. */
	pci_set2(NVRAM_ACCESS)_5750= &rkarounrx_rcb[power state.tp->				elseAC_TXsc->TIGON3VICEXD_OPAQUinclDEX_FLG2_57E_LINK_POL= DUPLEX_HALF;
		break;

	case <neFLG2_57
				STAT_10FUL!
#de_10;
		*duplexSTets" ex)
{
	switch (v(UPLEX__FLAvlaI_TG
	caERRCLK;
		ephy_IP_ALIGN d = SPEED_100;
k;

	calex ODg_seB| \
	CVD1 | r32(NVRAM_ACCESS)		}

	INTERUPLEX_iduplex 		*duptg3 *K;
		>>eak;

	caSHIFT HZ)r a ink_conlex ctivip_rev SPEED_100;
		*dup||
			tpr:
		*std3 & TG3s[				else].ITY;
p->pci_clock stateISABtp->
		break;

	case MII_TG3_AU}

stO_MAch as af.actTIGOg3_readp "meait_turn 0;
}

stat		}

		ivice_shoFROMd_wake) &&
_44MHZ_CORE;
			}

			tw32_waGRC_LCLCT* powe
		}
	}
hydev SPEK_CTRL,
				   rtising = adver
			iM5401) {
	_590atus;
	unsignuct {
 statED_10 "tx_.origPLEX_H*/   !(tpV_5700) {
				u32 speed = (tp->tg3_flag_FAILED		ROM_ADDR_COMPLG3_FLAG_WOL_break;
	}= tr32(NVRAM_A3_phy_copper_begiGMIIX_INVALID;
		break;
 |me);
  40 void tg3_phy_copper_bep->lock);

	if (lis & ~A SPEED_100 : SPEED_10;mple if the struct tg3 cpmue_confi_toggle_a			grc_local_ctrl |= GRC_LCLCTRL_Gg3 *tp)
{
	u32 new_return;
	}E_ID_TIGON3_5751F*tp = tnapi->RC_LOCAL_CTRL,dv = (ADVERTISE_10H32 off, u32 gphyired UTHORdown" },
	{ "rK_DISABLE);

		tw32_wait_f(TG3N3_5720)}ets" MU_PRESENT) ||
	CTRL_ALTCLK |
	
}

static void tg3_phy_fini(stCPMUES(t(PCI_e been runp->link_ng as all {PCI_DEVIv);
	}MUTEn.
 Q, ID) {
		if (tp_DRIVEhod */PCI_DaiUXCTL_bcaso 40 microval)
{
	wo ac3_wri e_inconfig_ad;
	tp->mdio_bu780 ed,
 * ans is onfig_dword(LID) {
		if G, PCI_D
		*sRTISED_1=>tg3_flags &G,
	{FLAG_1
			u32 sg_dig_TG3_FLG2_5705_= CHIPREVs is on	ret		if (tp->link_config.adv);

		new_adv = (ADVERTISE_10H;
		if (tp->trequ-(IS_d m) and(tp, MNDORconfig_ments.
		 *00baseT_Full);stats)/sizeof(if (tp->link_ ASIC_REVments.
		|= Mtp->link_;
		}
tephy(p->pDE_MII;
	)
			new_adv |=AWARy(80);
&& tp->link_					     SPEED_1_DEVICE(ODE_KEEP_FRAMwrite(rstruct t_INVALID;
		break;
itephy(tp, MII_ADVERTISE, new_adv);
	} else if (tp-g.advertising & ADV_100baseTphyid, adRT_ASICOR_ID_ute * CopPEED_INVALID) {
		if Gr(stdv |= ADVERTISE_10HALFX) {
		val G3_FLG3_MDIOBUS_INITED) {
		tp->tg3_flags3 &= ~TDULE_NAME ".c:v" DRV_MODULE_VERSION " (" DRV_MODULVERTISED_100baseT_Full)
_DISABLE |
		g.advERTISE_100Fd tg3_phy_copper_beW_WREN |
	Re-0);
	tgg3_flags & TG3_FLAG_WOL_SPEED_100MB)
			new_adv |= (ADVERTISE_100HALF | ADVERTISE_100FULL);

		DRESS,  if (tp->tg3_flags2 & TG3_FLG2_PHself == TG3_PHY_O3_phy_apply_otp(struct tg3 *tp)
NG_CO*efig.p, off,RC_LOCTRL_GPIO_OE0 |
					     GRC_LCLCTRL_GPIO_OE1 |
					he PHY. */
	phydev = phy_connecttw32(TG3PCI_MISC_HOST_CTRL,
	    					    
	}

	CTRL, reg3FACErd(tRL, tp->grcritephy(tp== PHY_ID_BCM5tp_SHUTDO_TG3_Efig.MODE_GMI|=ctrl   (GRFL->link_conct tg30CLOCPE_LOCephy(tp, MII_TACCESL, 0);
		}
	} else {
		new_adv = tg3_advert_flowctrl_1000lock_>link_confielse {
		new_&v = tg3_adverOFFLIN const rite_indierr2 {
			n on the peer. */
		if (!dev_peer)
			tp_peer = tp;
	}
	if (tp->tg = tp;
		else
			tp_peer = netdev_priv(dev_peer);
	}

	if ((tp->tg3_flags & T|
				     GRC_LCLCTRL_GPUSR_BUitephy(tp, MII_TG3t tg3 O_OUTPUT1),_TG3_CTRLtg3_flaZE - 	phyR_ID5703_4_5(tdelay(10);
	}

	if (!(tp->tg3_flags & TG3_01_A0 ||
			    tertisinchip_rev_id ==g3_writephy(t_writnt_mbox, tnapi-tx_collide_5times" },
	{ "tx_collide_6times" },
	 CLOCOM, PCIISC_LOCAL_ffset << EEPROM_ADDL, 0);
		}
	}  else {
		new_adv = tg3_advert_flowctrlct tg32nk mode.
#define Tags2 & TG3_FLG2L)
					new_adv |= ADVERTISE_100FULL;
				else
					new_adv3|= ADVERTISE_100H (tp->phydeower mode.  Disabld & PIO_OUTPUTelse {
		new_adv = tg3_advert_flowctw_statusi->int_mbox, tnapi-100HALF;
			}tp, off, val)					new_adv |= ADVERTISE_100FULL;
				else
					new_adv5|= ADVERTISVERTISE, newTRL_MASK_PCI_IN	struct tg3_hw_status *sblk = tnapi->hw_s
		if (!dev_peer)
			tp_peer = Block the PHY control acp)
{
	int i;

	f_ADV_tephLCLCTRL_GPIO_OE2 |
				     GRC_LC31:
RL_GPIO_OUTPUT0 |
				     GRC_LCLCTRL_GPIO_OUTPUT1)			     p, MII_TG3_AUX_>link_e |
		     HOSTCC_Mnk_configMASTER |
					    MII_TG3_CTRL_ENABLE_AS_MASTER);
			tg3_writephBLE;
		/Y_OUI_1 ||
				   ioctreset_5703_4_5(struct tg3 *tp)
{
ifreq *ifndir;
	} otp, phy;

	imiiSPEED1s3 = (HIPREtx_tf_mii(ifit_macro_APE))
		return;

	switch (locknum) {case TG3_APE_LO>phy_otp;

	/* Enable SM_DSP clock and tx 6dB coding. */
	phy = MII_TG3_AUXCTL_SHDWSEL_AUXCTL |
	      MII_TG3_AUXCTL_)
			bmcrNA |
	      MII_TG3_AUXCTL_ACTL_TX
	}

	pes.h>
#incCLCTRL MIIOE3;
				tSIOC;

	PHYI_DE els = CLOCK_OTP_10BTAMP_SHIFing & _writhruCE_ID_TIGO &tmp))
REG: SPEED_10)
		regCK_CTRL_TXCLK_DISABLE);

netif_running(tp->dev) && n3_writeff, u3WIC_SRAMno&
			),(va MAC to the PHY. */
	phydev = phy_connect if (tp->tg3_flags &ine void tg3_generate_fw_evTG3_APE_LOCK);
MODULE_ntinuef ((num 0)
		p->t}
			}
			ig.orne TG3_FW_EVENT_TIMEOUT_USEC 2ntinuevalp, N leav			}
			tg3_wdefine DRV_MODULEelay(40);S					bre {
			if (tp->link_config.speed 		udelay(40);
		}
	} else {
		tg3_writephy(tp, MII_BMCR,
			     BMCR_ANENABLE | BMCR_ANRESTART);
	}
}

static int tg3_init_5401phy_dsp(struct tgmapped debuggt err;

	/* Turn off tt length biev_idment. */
	/* Set Extended packeMWARE(FIRMWAREPCI_DEVICE(/* do;
		f, u3						    GRC_LOn 5753 and variants,able(tpPLEX;
		else {
			s2 & TG3_FLG2_PH100; & TDEVID_SH void tg3_phy_apply_otp(struct tDRESSgroup *grOR_ID_BROADCOM, != tp->link_config.active_flowctrl)
	    linkmesg =

		/* Bltg3_ad= grr to ;
} ethto{
		tg3_ags2 & TG3_FLG2_EBUSY;
		}

		for (i = 0; i < _writephy(tp,);
		UBROADC(phydev->pause)
				rS	(siznvertisinr = 1;

	tg3 *t phydev->speed;
	tp->linRW_PORT, 0x0a2 HOSTCC_MODE_ENABLE 
		tp->tg3_flaadphy(tp, MII_B1 << 4)));
}VICE(PCI void tg3_phy_apply_otp(struct tg3 *tp)
{ICE(PCI *ectp, phy;

	if (!tp->phy_otp)
		return;

	o;

	if f);
}

stE_10_LOCAL_CT
	if tp->grc_local_ctrl |
		FF_MASK) >>  ADVERTISED_10baseT_Full)
		all_mask |= ADVERTISE_10FULL;
	if (mask & ADVERTISED_100baseT_Half)
		alpolaritx_rx) ||
tick

		 {
			tax_tmask) != all_mask)v_reg & alltp->ll_ma != aIP_REAILBn TG3_FLAG_10_100_ONLV(tp->pci_chip_rev_id) == ASIC_REV_5#include <l_irqreall_mask) != all_mask_DMA_Tint i;

	addr;

	ireturn 0;
	if (!(tp->3_DS);
	forLF;
		if (mask &TG3_FLAG_10_100_ON3_DSP

	if (GET_ASI) {
		{
		u32 tg3_ctrl;

	>pcif (tg3_readphy(tp,i)		((tnaec& TG3_FCE(PCI_		tws >RTISE_100f (ret)& TG3_FLG2_PcTIGON3_mask) != all_mask)
CMD_GO |
		;
	}
	return 1;trl & C
static dp->tg3l_mask)
		;

	tg3_nv;
	}
	return 1;
}
tp, u32 *lcladv, u32 *rmtadbe32 *val)
{rl_ok(struct tg3 *
static int tgadl( >& all_mask) != all_m;
	}
	return 1;
}

static int tgv & (ADVERTurn 0;
	if (!(trl_ok(struct tg3 *tp, u32 *lcladv, u32v & (ADTISE_1000HA);
	addrradv, reqadv;

	if (tg3_readphy(tp, MI>link_confi);
	for (i = 0;;
	}
	return 1; GRC_Lle_in

static int tg3_a= ADVERTISE_1000FULp->tg3_flags & TG3_FLAG_PAUSE_AUTONEG)
			<)) {
		u32 tg3_ctrl;, 0x0323);
		tg3_writep into" },api->int_mb },
	be2_mailbox_fif bothC_MODis_lo Copyrigtrl & all_mask) != all_AX ||
s3 &= ~TGrl & altp, u32 *lcladv, u32 AX ||, even if it
		 * does not atfect the current link.  If the link
		 * gets renegotiated in
}

static int tg3n save an
		 * addi	if (tg3_readphy(tp, MIIle by advertising
		 * it corrp |
	cop.intlevays    cpertis, ignit"  (tp-PCI__VENDOR_TISE_10|
		 &= ~(ADVERTISE_ rl & all_mask) != all3_write tg3_adv &= ~(ADVERTISE_ruct 
}

static int tge_reset)
{
	tional renegotiation cyclruct tg3tp, u32 *lcladv, u32e_reset)
{
	int_adv;
	u16 current_speed;
	w32(MAC_EVENT, 0);

	tw32 lcl_adv, rmtSYM);
	reqadv = tg3truct tg3 *tp, int forcadl(e_reset)
{
	int current_link_C_STATUS_CFCOMPLETION |
	      M32 lcl_adv, rmt_adv;
	u16 current_sC_STATUS_CFG_C
	if ((tp->mi_mode & MAC i, err;

	tw32(MAC_EVENT, 0);

	tw   MAC_STATUS_UTO_POLL) != 0) {
		tw32_f(MAC_MI_MOTG3_FLAG_PAUSE_AUTONEG)
			eed;
	TG3_FLAG_PAUSE_AUTONEG)
		201f);
		tg3_writephy(tp, MII_TG3_DSg3_int_reenable(stru_BROADCOM, ADVERTIS   tERTISE_10p->tg3_flTRL_GPIO_OUTPUT1),
	f (!err)
			break;
	}BOX_BAG3_DSP_RW_PORT,opsjiffies + 1 +IC_R	 * T.);
}

static			tp->l);
}

static,
	. >> TG3_OTP_pci_chip >> TG3_OTP_ASICwritephy(tppci_chip_revephy(tp_ok(tp->sg)
		tgpci_chip_revsg)
		tgMII_BMSR, &b;
		if (!tg3_rea_ok(tp->wolpci_chip_revwolASIC_REVr & BMSR_LSforce_r_ok(tp->g3_writepci_chip_revg3_writeASIC_REV_reset)
		tg3_phf ((tp->phy_ASICay(40);
	}pci_chipay(40);
	}_ok(tp->requpci_t overdrawi MII_BMS_ok(tp->_CONNECTEDpci_chip_rev_CONNECTEDadphy(tp, MII_, &bmsr) ||
		    ASIC_REVags & TG3_FLAG))
			bmsrMII_BMSR,	     cp;
		if (!tg3_	     cpASIC_REV_LSTATUS)) {
			e1phy_dsp(tp);_ok(tp->TTER_BUG) pci_chip_revTTER_BUG) ASIC_REV
			tg3_readphy(tp;
			for (i = MII_BMSR,ps. */;
		if (!tg3_ps. */_5401phy_10);
				if (! MII_BMSR, ASIC_REVmps. */&bmsr) &&
		mps. */ASIC_REV_g	R, &bmsr);
		if);
			bmsr & BMs{
		tg3_r);

	ifASIC_Rchip_rephy_id & Pchip_re_ok(tp->PIO2;

pci_chip_rev_		no_gASIC2);
			pci_chip2);
			adphy(tp,p->mdio_bus) &bmsr) ||
	p->mdio_bus)_ok(tp->
static pci_chip_rev
static ASIC_REVet(tp);
				if ( tg3_init_54B0 &&
			    ASIC_pci_chip_rev_    ASIC_,
->pd;

	if (phydevG3_Fni3_FLG3_PHY_CONNECIBND5411) {
		if (speed != SPu{PCI_M_ADDR_0)
			ric int3_writephy = AT2_APDse ifval = _config.flnt);

#define RESET_KIND_SHUTDOWN	0
#defphy &= ~M, reg, &phy)) {
				if (ene an
		 * ig_post_reset(struct tg3 *, int);
 &phy)) {
				if (e_cpu(writephy(tp, 0x1c, 0x8d68);
		tg3_wi_chip_rtp, 0x1c, 0x8c68);
	HWDCOM, PCI_DEVIC_mailbox 570XD_MBOX_HW.intaMII_TG & TGcess increaphy(tUTHORsYSKONworrors"_PORTwe enff));
}
(PCIvalid up TXne TG(tp,		if REG_Lreg,vmbox(tthresholN_IDwrappTG3_rMII_,k_confiux_disca(PCIe TG3IBNDPA_PAUSEv_id ==E_FLO_FLAG__BROADCwritephy0);
	}3_writephySPEED_100 *, int);

#define Rv_id == CTG3_FLIO_OUTPUT0 |
		onfig.spe_map[
		tIND_ower_state(DVER, ~0);

<_BUG) {
		tB0) {
		/* 5701 5701) {wctrl;

	if (phydevev_id == CHIPREV3_writephy

static inline void tg3_ne			     ephy | MII_TG3_FET_SHADOW_EN);
			if (!tg3_re*, int);

#define RESET0 ||
	    GETmy);
	tg3_reII_TG3_AUXCTL_SHDWSELp->link_conc, 0x8c68);
		tg3_ 0x0400)PREV_ID_5701_A0 E_VERSIII_TG3_DSP_RWtg3 *, int);

#define RExfl = tr3s;

	ifet Extended 					new_adv_keys[TGtnaponfCT_9D. * Always Fullpoff)
full"devic_mod16-	(siwriteess | ACCESS_f2nst seT_CTRL, 0);
	}

d) ==dv |=_read_pr     _RX_Druct ED_10	if (waF_MSG_ elsal);
		accorMII_T_ADDR, l);
			gO_MAPTG3_OTP_" },
allal);
		the rewer) {
		, rmssAG_ENys[TGenonfiock, fR_FULwval);
		ways ad_priohydevre_FUNC				r		~( andif (!(g3_fl_wriHowx_co(tp->p& TG3_FUX_CTRLE_GMIILEXCTL_SH,D_BRchbreak;
mean_readphy(tp_RX_DBMSRX_CTRL     msr) &&
		  bval);
		opposplexllidendiannx(tpSKONNe CPU		val |if (!(break;
l);
			goto n bas zer40);
	}

to++) i = 0; i < 	break;l, tp-B0) {
		/* 5701 swab16((u16)aseT_FuOUT; i++) )} el_wri_LEN];
} ethtool_sta  aux_stat)
				brse {
mcr_reIZE_BMSKBPHY_1)
			tg3_writephy(tp, MII_TG3_EXT_CTy(tp, MII_TG inline void tg3nvcfgEV(tpy(tp, FULL;
		>tg3_flFG*tp = tnaBMCR, &&(tp->nv(tg30x00AT_1g.h>
#

		/* Block the Puct tg3 *tp)
{))
		FLG2_5780_CLAS(tp, MII= ~BMCR, &bmcr_napAT_BYPA_BASlink_coBMCR, &bmc,hy(tp, baseT_Half |		newbits2 = newbits1 | CLOCK_CTRL_ALTCLK;
		} el5    tp-IC_REV(tp->pci_chip_rev_id) == CE(PCI_VENDOTUS_UPID_TIGO(tp, MII_BMCR, &bmcrif (!(& TG3_FLG2_},
	{))
		cr & BMCATMELff)
		>phy_maEDC_SER aux_stat)jedec* Tu= JEDECopper_3PCI_MISCS_MASTpagetephy(tpper_iAT45DB0X1B_PAG"tig sg_dignk_config.active_speed = t
					rtising_ | coal_now);
) &&
			    tg3_copper_is_adveUNrtising_all(tp,
						tp->link_config.advertising)) {
				if (tg3_adv_1000T_flo25FBMSRtp, &lcl_adv,
;
			}
		} else {
			if (!(bmcr &AT2_AP&&
			    tp->link_config.speed == current_speed &&
			    tp->link_conf4CBMSR CRC bug wov,
								  &rmt_adv))
					current_link_up = 1;
			}
		} else {
			if (!(f(GRC_tp,
						tp->link_config.adrc_log)) {
				if (tg3_adv_1ST_M45PEX0(tp, &lcl_adv,
								  &rmt_adv))
					current_link_up = 1;
			}
		} else {
			if (!(SAIFUN			tg3_setup_flow_control(tp, lclin(tpadv, rmt_adv);
	}

relink:
in(tp_SA25F0XXplex == current_duplex &&
			    tp->link_S(GRCMALL0);
TUS))
			current_link_LARGE			tg3_setup_flow_control(tp, lcll_adv, rmt_adv);
	}

relink:
ST_25VF0rrent_link_up == al_now);
}

s000f;
			if (l					tp->link_config.advertising) {
				if (tg3_adv_1000T_flowctrl_ok(tp, &lcl_adv,								  &rmt_adv))
					current_link_up =  phydev->duplex tephy(tp, MII_S_MAST, MII_tg3_adable)
{
	u32 phy;

	invmonfigRC_LCLCTRL_GAC_MODEII_BMCR, &bmcr5752tp, &lcl_& TG3_FLG2) &&
			   de |= MAC_MODE25CI_DEV {
				if (tg3_adv_125RL, pPIO_OE2 |
			I;

	tp->mac_mode &=5	{PCI_D {
				if (tg3_adv_15124);
nk_config.active_duplex == DUPLEX1KLF)
		tp->mac_mode |= MACduplex(tnk_config.active_duplex == DUPLEX2>pci_chip_rev_id) == ASIC_DVER5700) {
		if (current_link_up == 1 &&4>pci_chip_rev_id) == ASIC_g3_r5700) {
		if (current_link_up == 1 &&
6{PCI_Dtg3_5700_link_polarit6_5700) {
		if (current_link_up == 1 &&5282)},
	tp->mac_mode |= MAC_2tp, tp->link_
	} else if (tp->tg3_flags3 & T
	u1de | = 0; i < 200; i++) {
			tg3_readphy(tp, MII_BMCR, &bmcr);
			if (tg3_rees not_CTRLCE_IR)

/* 		tpTPMenegotiat(tp, MII_<< TX_27			APE_LOCK_GRANT_DRIVER);
}

stPRO84 &ED	curreneg =TONEG_ENABLE) {
			if ((bmcde |r & BMCR_ANENABLg.active_duplex->link_config.flowc_64KHZ3_5705F)_flags & TG3_FLAG_USE_LINKCHG_376G) {
	link_config.active_speed == SPEED_10)
							  &rmt_adv))
					current_link_up = nk_config.active_duplex tg3_copper_is_advertising_all(0);
	} else {
		tw32_f(MAC_EVENT, MAC_EVENT_LNKSTATE_CHANGED);
	}
	udelay(40);

		if (bmcr && bmcr != 0x7fff)
				b
	if (GET_ASIC_REV(tp->pci_chip
	if (cu
	{PCDE) ||
	     (tp->tg3_flags & 2G3_FLAG_PCI_HIGH_SPEED))) {
		udel4	{PCI__setup_flow_control(tp, lcl_adv,   tp->link_config.active_speed == SPEED_1000 &&
	    ((tp->tg3_flags & TG3_FLAG_PCIX_MODULE_RELDATE	"September 1, 2009"

))
		VALID;

	iG3_FLG3_PHY_IS_FET)   t_config.actE |
			     M_ASI

staticMODE (tg3_advtog3_rimumgs3 & T_FET)X process  {
				current_link_up = 1;
			}
		}

		if 	udelay(10);
		}

		lcl_adv = 0;
		rmt_adv = 0;

		tp->link_config.acti..
	 */
	if ((tp->phy_id & PHY_ID_MA5 = 0; i < 200; i++) {
			tg3_readphy(tp, ,->mi_mod	 */
		t_id == CHIPREV_ID_5700_ALTIMA) {
		tp->mi_mode |= MAC_MI_MODE_AUTO_POLL;
		tw32_f(MAue;
			if (bmcr && bmcr != 0x7fflay(80);
	}

	tw3hyde)
			newUG) {
		elay(10);
ac_mode);
	udelay(40);

	iICE_ID_TIGO_config (tp->tg3_flags & 5 tg3_copper_is_adve{PCI				      newlnkctl);
	} else if (N3_5705F)     newlnkctl);
	} else if (nablPLLPD) {
		u32 newreg, oldreg = trCI_DEV &&
	    current_link_up == 1 &&
	    tp->link_config.active_speed == SPEED_1000 &&
	    ((tp->tg3_flags & TG3_FLAG_LINK_POLARITY;
	}

	/* ??? _AUTO_POLL;=(GET   newlnkctl);
	} else if (t(GET_ASIC_oldreg | TG3_PCIE_LNKCTL_L1_PLL_PD_EN;
5 != 0 ||
 {
		/* 5701 (	pci_wriy_ma3e200 C_SERD	PCTL_S
					     &curry_dsp(_ape_wrioldreg | TG3_PCIE_LNKCTL_L1_PLL_PD_EN;

		case
	}

	if (current_link_up != 1ftif_carrier_ok(tp->dev)) {
	256 (current_ldv, rmt_adv);
r_off(tp->dev);
		tg3_link_report(tp);
	}

	return128 (curre_PCIX_MODE) ||
	     (tp->tg3_flags & TG3_FLAG_PCI_HIGH_SPEED))) {
		udelay(120);
		tw32_f(MAC_STATUS,
		     (MAC_STATUS_SYNC_CHANGED |
		      MAC_STATUS_CFG_CHANGED));
		udelay(40);
		tg3_write_mem(tp,
			      NIC_SRAM_FIRMODE_HALF_DUPLEX;
	if (tp->link_up)
			netif_carriertp->tg3_flags & TGwreg);
	}

	if (current_link_up arrier_ok(tp->dev)) {
	64KBe ANEG_STATE_UNKNOWN		0
#define ANEnt_link_up)
			netif_carrierPEED))) {
		udelayE_ACK_INIT	9
#define ANEG_STATE_COMPLETE_ACK		10
#define ANEG_STATE_IDLE_DETECT_INIT 0;
}

struct tg3_fiber_aneginfo {
	int stateANEG_STATE_UNKNOWN		0
#defin_carrier_ok(tp->dev)) {
		if (curre	    GRC_LCLCTRL_GPIf ((tp->phy_id & PHY_ID_M87K) == PHY_ID_BCM5411 &&
	    tp->pci_chip_rev_id == CHIPREV_ID_5700_ALTIMAf(MAC_MODE, tp->mac_mode);
	udelay(40);

	if (tp->tg3_flags &87G3_FLAG_USE_LINKCHG_REG) {
		/* Polled viX	0x00000080
#define MAC_EVENT,_SYM_PAUSE	0x00000100
MICROdefine MR_LP_ADV_SYM_PAUSE	0x00000100
ine MR_LP_ADVAC_EVENT, 0);
	} else {
		tw32_f(MAC_EVENT, MAC_EVENT_LNKSTATE_CHANGED);
	}
	udelay(40);

nkctl;

		pci_read_config_word(tp->pdev,
				     tp->pcie_cap + PCI_EXP_LNKCTL,
				     &oldlnkctl);
		if (tp->lin
	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_57				      newlnkctl);
	} else if (tp->tg3_flags3 & TG3_FLG3_TOGGLE_10_100_L1PLLPD) {
		u32 newreg, oldreg = tr32(T.active_speed == SPEED_100 ||
		    tp->link_config.active_speed == SPEED_10)
			newreg = oldreg & ~TG3_PCIE_LNKCTL_L1_PLL_PD_EN;
		else
			newreG_STATE_AN_ENABLE		1
#define ANEG_STATE_RESTART_INIT		2
#define ANEG_STATE_RESTART		3
#define ANEG_STATE_DISABLE_LINK_OK	4
#define ANEG_STATE_ABILITY_DETECT_INIT	5
#define ANEG_STATE_ABILITY_DETECT	6
#define ANEG_STATE_ACK_DETECT_INIT	7
#define ANEG_STATEETE		0x00000004
#define MR_PAGE_RX		0x000000061	    tp->link_config.active_speed == SPEED_10)
			newlnkctl = oldlnkctl & ~PCI_EXP_LNKCTL_CLKREQ_EN;
		else
			newlnkctl = oldlnkctl | PCI_EXP_LNKCTL_CLKREQ_EN;
		if (newlnkctl != oldlnkctl)
			pci_write_config_word(tp->pdev,
					      tp->pcie_cap + PCI_EXP_LNKCTL,
					      new61 tg3_copper_iADB021ility_match, idle = 0;
		ap->ack_mat4h = 0;
	}
	ap->cur_time++;

	if (tr328h = 0;
	}
	ap->cur_time++;

	if (tr316h = 0;
	}
	ap->cur_time++;

	if (Mmatch = 0;
	}
	ap->cur_time++;

	if (atch(MAC_STATUS) & MAC_STATUS_RCVD_CFGatch		rx_cfg_reg = tr32(MAC_RX_AUTO_NEM);

		if .active_speed == SPEED_100 ||
		    tp->link_config.active_speed == SPEED_10)
			newreg = oldreg & ~TG3_PCIE_LNKCTL_L1_PL_dword(tp->pdev, TG3PCIy(tp, MIcmd);
3_flasts;
}

HALF_DUPLEX;
	if (tp->link_config.active_dupl = 0;
		aMASK		udelay(120);
		tw32_f(0;
	} else {
		ap->  (MAmatch = 1;
		ap->ability_match_c8g = 0;
		ap->ability_match_count = 0PCI_Dmatch = 1;
		ap->ability_M		ap->idle_match = 1;
		ap->ability_	}

	apfg = 0;
		ap->ability_match_co	}

	ap;
		ap->ability_match = 0;
		a	}

	apPCI_DEV	0
#define ANEG_DONE	1
#define ANEG_TIMER_ENAB	2
#define ANEG_FAILED	-1

#define ANEG_STATE_SETTLE_TIME	10000

static int tg3_fiber_aneg_smachine(struITIVE_	pci_wrP_LNKCTL_C {
		/* 5701 cr);
			if md);
LOCKOUne TG
	if (tp->tf(MAC_MODE, tpENABLE) &&
			   32(MAC_RX_AUTO_NEG);

		if h = 0;

		rx_cfg_reg = 
		} else {
			ifp->ability_match = 0;
		ap->ack_matchE_UNKNOWN:
		if (ap->flags & (MR_AN_ENA		     &current_speed,
					     2MBp = 1;
			}
		} else {
		C_STATUS_RCVD_CFG) {
		rx_ceg;
			ap->ability_match = 0;
			ap->ab 0;
		ap->ability_match_count = 0;
		a
	switch(ap->state) {
	case ANEG_STATENIT;
		} else {
			ap->state = ANE1_STATE_DISABLE_LINK_OK;
		}
		break;

	case ANEG(MAC_S {
			ap->ability_match_cfg = rx_cfg_re_match = 1;
		ap->ability_match_cfg = config = rx_cfg_reg;
	ret = ANEG_OK;

NIT;
		} else {
			ap->state = ANE&currenTE_DISABLE_LINK_OK;
		}
		break;

	case ANEGch = 0 (rx_cfg_reg != ap->ability_match_cfg) ->idle_match = 0;
			ap->ack_matcay(12h = 0;

		rx_cfg_reg = 0;
	}

	ap->rxcNIT;
		} else {
			ap->state = ANEG0;
} | coal_now);
}

st..
	 */
	if ((tp->phy_id & PHY_ID_906 = 0; i < 200; i++) {
			tg3_reactive_speed == SPEED_100 ||
		    								  &rmt_adv))
					current_link_up =trl) {
				current_link_up = 1;
			}
		}

		if ..
	 */
	if ((tp->phy_id & PHY_ID_ME(PCdefine MR_NP_LOADED		0x00000010
#define MR_TOGGLE_TX		0x00000020
#define MR_LP_ADV_FULL_DUPLEX	0x00000040
#define MR_LP_ADV_HALF_DUPLEX	0x00000080
#define M_ADV_ASYM_PAUSE	0x00000200
#define MR_LP_ADV_REMOTE_FAULT2	0x00000800
#define MR_LP_ADV_NEXT_PAGE	0x00001000
#define MR_TOGGLE_RX		0x00002000
#define MR_NP_RX		0x00004000

#define MR_LINK_OK		0x80000000

	unsigned long link_time, cur_time;

	u32 abili;
} ethto	/* Polled via timer. */
		tnt;

	char ability_match, idle780 0;
		ap->ack_owctrl1h = 0;
	}
	ap->cur->txconfig);
		tp->mac_moBe |= MAC_MODE_SEND_CONFIGS;
		tw32_f(MAch = 0;
	}
	ap->cure);
		udelay(40);

		ap->MODE, tp->mac_mode);
		udelay(40);

		ap(MAC_STATUS) & MAC__STATE_ACK_DETECT:
		if (MODE.active_speed == SPEED_100 ||
		    tp->link_config.active_speed == SPEED_10)
			newreg = oldreg & ~TG3_PCIE_LNKCTLe = 0;
			ap->abi->mac_mode);
	udelay(40);

	if (tp
		ap->txconfig |= ANEG_CFG_ACK;
		tw32(MAC_TXX_AUTO_NEG, ap->txconfig);
		tp->mac_mode ||= MAC_MODE_SEND_CONFIGS;
		tw32_f(MAC_MODETO_NEG, 0);
		tp->mac_mode |= MAC_Mefin;
		if (delta > ANEG_STATE_e);
		udelay(40);

		ap->statte = ANEG_STATE_ACK_DETECT;

		/* fallthru NK_OK:
		ret = ANEG_DONE;
		break;

	case ANEG_STATE_*/
	case ANEG_STATE_ACK_DETECT:
		if (ap->>ack_match != 0) {
			if ((ap->rxconfig & ~A		delta = ap->cur_time - ap->link_time;
		if (delta t_pa_PCIX_MODE) ||
	     (tp->tg3_flags & TG3_FLAG_PCI_HIGH_SPEED))) {
		udelay(120);
		tw32_f(MAC_STATUS,
		     (MAC_STATUS_SYNC_CHANGED |
		      MAC_STATUS_CFG_CHANGED));
		udelay(40);
		tg3_write_mem(tp,
			      NIC_SRAM_FI	} else {
				ap->state = ANEG_STATE_AN_ENABLE;
			}
		} else if (ap->abilflags & TG3_FATE_COMPLETE_ACK_INIT:
		if (ap->rxconfig & ANEG_CFG_INVAL) {
			rSPEED))) {
		udelay(12
			       MR_LP_ADV_HALF_DUPLEX |
			       MR_LP_ADV_SYM_PAUSE |MAC_STATUS,
		     (MAC       MR_LP_ADV_REMOTE_FAULT2 |
			       MR_LP_ADV_NEXT_PAGE |PCI_DEVICE((rx_cfg_reg & ANEG_CFG_ACK)
			ap->MII_TG3_AU_DSP_RW_PORT* Prevent send BD corruption. */DEVICE_ID			if (tg3_adv!
	/* _LOCK008)
			ap->flags |= 528f (!(tp->tg3_flag ANEG_CFG_ACK)
			ap->ack_match = 1}
.
	 */
	if ((tp->phy_id & PHY_ID_M1
#define MR_NP_LOADED		0x00000010
#define MR_TOGGLE_TX		0x00000020
#define MR_LP_ADV_FULL_DUPLEX	0x00000040
#define MR_LP_ADV_HALF_DUPLE1	0x00000080
#define tch = 0;

		rx_c == 0) {
ine MR_LP_AD& ~ANEG_CFG_ACK) ==
			    (ap->ability_match_cfg & ~ANEG_CFG_ACK)) {
				ap->state = AN)
			ap->flags xconfig != 0) {
			ap->state = ANEG_STATE_ACK_DETECT_INIT;
		}
		break;

	case ANEG_STATE_ACK_DETECT_INIT:
		ap->txconfi == 0) {
			ap-atchmode |= MAC_MODE_SE == 0) {
			ap-_matC_MODE, tp->mac_mod)) {
					ap->state =>flags & MR_NP_RX)) {
					ap->satch_cfg) {
			ap->abil)) {
					ap->statelthru */
	case ANEGcase ANEG_STATE_IDLE_}
		}
		break;

	case ANEG_STATE45USP(GRC_NEG_CFG_ACK) ==
			    (ap->ability_match_cfg & ~ANEG_CFG_ACK)) {
				ap->state = ANEG_STATE_COMPLETE_ACK_INIT;
			} else {
				ap->state = ANEG_STATE_AN_ENABLE;
			}
		} else if NEG_FAILED;
				}
			}
			}
		break;

	case ANEG_STATE_IDLE_DETEECT_INIT:
		ap->link_time = ap->cur_timNK_OK:
		ret = ANEG_DONE;
		break;

	case ANEG_STATE_tw32(MAC_SERE_COMPLETE_ACK_INIT:
		if (ap->rxconfig & ANEG_CFG__NEXT_PAGE |
			       MR == 0) {
ags & 2 & TG3_FLAG_PCI_HIGH_TATE_LINK_OKA
		}
		break;

	case ANEG_STATE_LINK & (MR_Abreak;

	case ANEG_STATE_LINK_OKs & TG3_FLAG_PCI_HIGH_TATE_LINK_OK;
		}
	ay(120);
		tw32_f(G_STATE_LINK_OK:
				/* ??? unimplemented */
		br	}

	ap->rxconfig = rx_cfG_DONE;
		break;

	??? unimplemented */
		break;
2E_SEND_Cnimplemented */
		break;
DE_SEND_CONFIGS;
		tw32_f(MAC_MODE, tonfig & ANEG_CFG_HD)
			ap->flags |= MR_LP_ADV_HALF_DUPLEX;
		if (ap->rxconfig & ANEG_CFG_PS1)
			ap->flags |= MR_LP_ADV_SYM_PAUSE;
		if (ap->rxconfiXT_PAGE_WAIT_INIT:
		/*  ??? unimplemented */
		break;

	case  ANEG_STATE_NEXT_PAGE_WAIT:
		/* ??? uunimplemented */
		break;

	default:
	>cur_time - ap->link_time;
		if (delta > ANEG_STATE_SETTLE_TIME) {
			/* XXX another gem from the Broadcom driver :( */
			ap->s_ADV_NEXT_PAGE;

		ap->link_time = ap->cur_time;

		ap->flags ^= (MR_TOGGLE_TX);
		if (ap->rxconfig & 0x0008)
			ap->flags |= MR_TOGGLE_RX;
		if (ap->rxconfig & ANEG_CFG_NP)
			ap->flags |= MR_NP_RX;
		ap->flags |lue N_BASthe re32_f(->md/ is |= tneadphy(tp,		tp)},
	ec_waifotg3 *lse if (tp->tg3_flags3 & TG3_FLGDR, 0);
	} else {
		tw32ht (C) _mapAT2_APDBROAD, 0x000ags = aninfo_FSM#defin the inter
	if (stDEFAEVICE= 0;_PHYIOD <<urn;
	}

	if (status CLKPERmbox_100o_rese3 *tp, uLTIMA) {m Tigons6 oldlnSR, &bmsr)TG3_RX_STD__DEVICE_ID_ALTIMA[PHY_ADDR];
	switch (phydCI_VENDOR_ID_ALak;
_SAT2_APG_PROBE	| \
	 NETIF_MSG;

	/* When doing tagged status, this work check2);
		ill reset the tigon3 PHY if there is nolink. */
	if ((G3_FLG2__EVENT_LNKSTATE_CHANGED);
	}
	_config.dupled == CHIPREV_ICK_GRANT_DRIVER);
}

static}

staticCanf(stEVICnvarm	~(ADct tg

	/* Makeay(40);

	*txfime is ver hasn't any stale loc];
} ethtool_sflags);

	/*)
			aSR, &b},
	{ "tx_col {
		/* 5701 er. */
		id = current_speed;
		tp->link_config.active_duple
		case SPY_ID_MASK) == PHY_IDE_VERSIdistribution of this firmware
 *	data in hexadecimal or e5ewreg);ED_100 ||
		    tp->lin0x8411);

	/* Enable auto-lock and comdet, select txclk for tx. 87eset(strg32 &= ~0x3000;
		tg3_writephy(tp, MII_TG3_EXT_CTRLff);

	/* Assert and deassert POR. */
	tg3_writephy(tp, /
	tg3_writephy(8
#define MR_N0x8411);

	/* Enable auto-lock and comdet, select txclk for tx. >
#inc3_writephy(_aneginfo *ap)0x8411);

	/* Enable auto-lock and comdet, select txclk for tx.SIC_REV(gs &= ~(MR_TOGGLE_TX);
	0x8411);

	/* Enable auto-lock and comdet, select txclk for tx.  MAC_MODif (flowctrl & ADVERTISE_0x8411);

	/* Enable auto-lock and comdet, select txclk for tx.  err;

ate = ANEG_STATE_COMPLET0x8411);

	signal to stsg_dig_ctrl, sg_CI_DEVICE_ID{
		/* 5701	if (!no_II_TG3_EXT_CTRL,
	,
	{ "tx_off == (MAIe_timeout() ... */ABLE_AS_MASTER);
		} else x8005);
		tg3_writephy(tp, MIverti					curretatic int			current_link_unt_link_upf (tp->tg3_flags2 & TG_LCLCTRL_GPIO_OUTPUTt tg3 *tp, int en_CT_9D    (tp->tg3_flVENDOR_Irmware is

	if (!(tp->tg3_flags2 & TG33_flags & Tjoff struct tg3 *tp, u3attach to PHY\n", tp->dev->n SPEED_10P_SHIFT32 reg =fine tw3now)eT a	      MII do noTD_IBND
	}

	phy(tpi u32 oerr |AC_MODal |ATUS);
ect thfDOR_expecXSTAII_TG3_AUX_UX_STAT, tat);
		fAC_MODDR, oawriti = 0; XCTL_SHphy(tp	}

mplisDEVIVICE.intl)
#dhres		}
	_Autone3_AUX_
}

s>indiwADDR,discards" perCTL_"rx_er device_s;

		tg3_raitephy_ADDL,
			     MII_TG3_)
			tp->link_coxflags = annux/,reak;32(e tw32_wait_d tg3o_rese			       TGxflags = aninfoSPEED_INVAxflags = aninfo.napi))
INK_OK |
			 napi *trtising |=u32 v	if (status atus se MIG_COMMON_SETUPd_waTCLK;
ORT, &	if (status mpanying , PCI_DEVG_DIG_CTRL, SG_DIG;
	}
0;
		) {
			tg3_setup_fAT_1000 out;
erve &	/* Want autoS_PCS_SYNn.  */
	if (status 5784_AX) {
	if (status WRI
		if (mE;
	else
		tp->t3_FLAde &= ~TX_f(MAC_SERDES_CFG, val);
			}

			tp->link_ted_sg_dig_ctr);
		}
		
			u32 sg_dig_DUPLEX)))
	RTISE_100H! & ADVERTISE_1000XPAUSE)
		eTRL_44M		por-EBUSCTRL, al_now);
}

stat  (val |cI_DEVICE2 & TG3_confine twydev_hw)
},
	3PCI_T) |
	     ((offse != CHIPREV_ID_5704n & TG3e	}
			tw3	u32 phy;

	if (!(tp->tg3_fla   MII_TG3_MAC_CTRLMII_TGtg3_flag (tg3_adv_1LE_RX;
		if (ap->rtatus & (MAtest_pa (tg3_adv{
		u3|= MACwrits Inc			}
		(sizef(struphy);
		Y_IS_FET, val | GRC_MISC_CFGf(str_IDDQ);
		udelay(40);
		retu_BROADC_TG3_AUX__modep->link_BTAMP_SH,CVD_C_TG3_10)
		ret,
	{ erve bits 20-2& ~C_STATUS= DUPLEX_Hlse
		tp->tED |
				
			tp->link_cMII_TG3_MISC_SHDW_SCR5_SDTL |
BTAMP_SHeed;
>pci_chip(SE);
}

)ink_up+ jlisionsX;
			phy |=ected_sg_dig_->serdes_coun ASIC_REVn;
	}

	MAC_SERD| 0xc011000)
		tw32_f(D_SUSPENDED |
				  		if (!tg3_G3_AUX_C_SUSPEND			phy phy_fet__CFG, ser_GPIO_OE1, +(MAC_SERDESL_VREIC_RE & TG3_FLG3_USSEL |
	  des_count -lags2 &= nt_link_upschedule_timeout() ... */
	fg = tr32B_hit" w fla0) {adv);

	flfineY_IS		if RL_GG_CTRL)o iss& (1 special "duplex)

#de"_index" 
			tp->liMAC_STATUEND	2

st graWRdulo_dig_ctrl &GODIG_PAUSE_CAPDON	     (cha;

				if execTATU corrupC_STATUEAD);
	tw32(Gerr |=E&
		    (tarEVICY_IStp->link_co>link_time	tw32_f(SG tr32(

			if (sg_dig_ctrl &)
				local_adv |= DIG_PAUSE_CAPWR.  */
dig_ctrl &FIRS
 *	dig_ctrl &LA		if (sg_dig_stERAS	    n;
	}

	TISE_1000XPAUSE;
			if (sg_dig_ctrl & SG_DIG_ASYM_PAUSEIYNCED)3_wrerhydev-l_adv =TAT2_APD;

	}
de <lconfig_

			if (sg_dig_ctrl & SG_DIG_PAUSE_CAP)
				local_adv |= ADVERTISE_1000XPAUSE;
			if (sg_dig_ctrl & SG_DIG_ASYM_PADIG_CTRL, expected_sg_dig_ctrl | SG_DIGsis */
		/* presf(GRC_LO*(ected_sg_dig_ctrl);

		(!no_gpio
					WRlse
		|= 0x4010000;
				else {
				if (woASYM;

			if (rl);
else {_status & SG_DIG_PARTNER_PAUSE_CAPABLE)
			ink_cdig_ctrl & S(flowctrl j workaroun|= 0xc01000tp, tg3_flagE;
			wait_f(TG3PCI__CFG,mac_status =4et_drvd				}

				tw32_f(SG_DIatus(flowctrl 			pp_rev_id == CE;
			if (sg_dig_ctrl & expected_sg_digES_AN_TIMEOUT_5704S;
		nfig_wo			if (sg_dig_ctrl & SDIDIG_PAUSE_CAP)
				local_adv |= AD^= (MR_TOGGE;
			if (sg_dig_ctrl &elseX_CTRLDEVICEY_IS_FET;
		breakM_PAUSE;

	if (sg_dig_ctrl != expected_sg_dig_ctrl) {
		if ((tp->tg3_flags2 TG3_FLG2_PARALLEL_DETECT) &&
		    tp->serdes_counter &&
		    & TG(mac_sta* preserve bits 0-11,13,14 foonfig.spee for signal pMAC_SERDES			tw32_f(MAC_STATUS_Pp->serdes_counterltage regulator */
		serdes {
				if (workaround) {
					u32 val = ser->tg3_flags2 &= ~TG3_FLG2_%US_PCS_SYNCED |
				 serdes_cfg | 0;

				if 2);
	d = S_config.spUSEC 25_1000XPSE_ASYM;

			if (sg_dig_status & SG_DIG_PARTNER_PAUSE_CAPABLE)
				remote_adv |ASYM_PAUSE)
				des_c &= ~TFLAG_NVRAM) {
READ);
				}

				tw32_f(SG_DIG_CTRL, S	if (goto out;

x0008)
			ap->flags udelay(40;

				/* Link parallel detectio) & SWARBMISC_SH(fiber_autoneg(tp, &txflags, &rxflags)) {
	ags & TG3_FLAG_INIT_COMPLETE) &&
	    !(mac_stat52S	2

static int tg3_debug = -1;	/* -1 004 David tells the chip 					tp->link_co, u32 *txf |= ADVERTIS words */
I_BMCR, &_DIG_CTRSE_10tion - link is up */
				/* oncmd(tp,
				NVRAM_CMD_WREN |  Broadcom GO |g3.c: Broadcom DONE)))
g3.c:break;
		}
		if (!(tp->tg3_flags2 & TG3_FLG2_FLASH)) {g3.c/* We always do complete word writes to eeprom. */g3.cnvram_cmd |= ( Broadcom FIRSTn3 ethernet dLAST), 20033, 2004(ret =  S. x.com)exec)
 * * t x.com)
 *(C) 01, 2002, 2}
	return ret;
}

/* offset and length are d003 Jalignedik@pstatic int) 2005-2009eff G_block(struct (C) * Coru32 propri *
 *	len, u8 *buf)
{
	ght rived
 2004avid S. Mille (davem@rAG_EEPROM_WRITE_PROT)
 * Ctw32_f(GRC_LOCAL_CTRL, avidgrc_local_ctrl &
		 	notic~lent CLt, p_GPIO_OUTPUT1Inc.
udelay(40Inc. * C2004 David S. Mille firmware
 * Broam)
 * Cht (C) 2005-2009003 Broadco_using_ (jgarm CorPermissis hergranare iselse
 * C
 *	d thmode disude <linux/kerneladcomtpInc.
2004ret*
 * :
 *	Derived or g3_enable05-2009accesselay.h>
#incDavid S. Miller (davem@redh5750_PLUS) &t
 *	not David S. Miller (davem@redhdeciECTEDram.h>
#
 *  equyright n hex1, 0x406)#incl <linux/(C) r32alentMODEInc.
clude.h>
#inc,e <linux/ | .h>
#incram.h>_WR_ENABLclud<linux/ioport.h>
#incux/moduleparam.h>_BUFFERED) |er.
  David S. Miller (davem@redhat.com)
 * <linux<linux/kernel.h>
#includebuffereom Cor#include <li2001, x/comp003, r.h>
#incude <linux/kernel.h>
#include nux/prefetch.h>
#include <linux/dma-mappilude <linux/ethtool.h>
#include <linux/mii.h>
#include <&e is aphy.h>
#include <linux/brcude disinux/init.h>
#include <li 2005-2009unux/delay.h>module.hution of this firmware
 *	data in hexadecimal or equivalent format, provided this copyrig

#include <linux/modu:
 *	Derived fr Corporsubsys_tbl_ent
 * u16VLAN_TAGvendor,VLAN_TAGdevid;
cludephy_0
#e};ine opyrie TG3_VLAN_TAG_USED 1
LAN_TAGid_to_
#incl[] =
 * /* Broadcom boardszik@po{ PCI_VENDOR_ID_BROADCOMff.h1644, PHYRSIONCM5401 }, DULECM95700A6ine DRV_MODULE_VERSION	"3.102"
#d0001e DRV_MODULE_7ELDATE	"Septembe1A5, 2009"

#define TG3_DEF_MAC_MODE	0
2e DRV_MODULE8002DATE	"SeptemberT1, 2009"

#define TG3_DEF_MAC_MODE	0
3, 0 }, *	not	"September 9, 2009"

#define TG3_DEF_MAC_MODE	0
5define TG3_DEF_RX_MODE		0
#defiT1, 2009"

#define TG3_DEF_MAC_MODE	0
6MSG_IFUP		| \
	 NETIF_MSG_RX_ERR8, 2009"

#define TG3_DEF_MAC_MODE	0
7INK		| \
	 NETIF_MSG_T1A7, 2009"

#define TG3_DEF_MAC_MODE	0
8define TG3_DEF_RX_MODE		0
#defin10ine DRV_MODULE_VERSION	"3.102"
#d8MEOUT			(5 * HZ)

/* hardware minimu2, 2009"

#define TG3_DEF_MAC_MODE	0
9define TG3_DEF_3DATE	"Septembe3Ax	| \
	 NETIF_MSG_TX_ERR)

/* lengt8p->tg3_flags & TG3_FLAG_JUMBO_CAPABL		60
MODUL3E	": "
#define DRV_MODULE_VERSIO3102"
#de000e DRV_MODULE_RELDATE	"S3C996T can't change the ring sizes, buf time before we decide ou plBace
 * them in the NIC onboard me4INK		| \_RX_RINGSX can't change the ring sizes, bu7ory.
 */
#define TG3_RX_R butace
 * them in the NIC onboard meOUT			(5 * HZ)

/* hardw3C940BR0	| \
MODULDELL: "
#define DRV_MODULE_VERSIOentrMODE	0d#define TG3_DERELDATE	"SVIPERe into the tp struct itself,
 1emory.
 */
#defRELDATE	"SJAGUAse constants to GCC so that modul>tg3_flags & T41LDATE	"SMERLOace
 * them in the NIC that modulaof with
 * hw multiply/SLIM_modulo insMODULCompaq: "
#define DRV_MODULE_VERSIOCOMPAQMODE	07cdefine TG3_DEF_RX_MODE		ANSHEE 1)'.
 */
#define TG3_RX_RCB_RING9 would be to
 (tp->tg3_flags & _		60
#define TG3_MAX_M_RX_RCB_RING_dINK		| \
	o' wHANGELINGASS)) ? 1024 : 512)

#define TG3_8_MSG_IFUP		| \
	 NETIF_MNC778m and maximum for a siE) && \
	  !(>tg3_flags & TG3#define TG3_RXCLASS)MODULIBM: "
#define DRV_MODULE_VERSIOIB_MODE	28uff. }E		5IBM???ik@pe "tg3.h"

inline
#define DRV_MODULE_NAM*lookup_by_LAN_TAm Corporation.
nted for ix/brfor (i = 0; i < ARRAY_SIZE(E		"tg3"
#define PF); i++)
 * Cnux/iE		"tg3"
#define PFXi].define TG3_VL ==
 *	notiavidpdev->LAN_TAtem TG3_VLclude <linufine TG3_TX_RING_BYTES	(sizeof(ED 0
 tg3_tx_buffer_desc) * \
				 TED 0ceh>
#in:
 *	De&ine TG3_TX_RING_BYTES	are is:
 *	DeNULLdefine opyrivoid _TG3_Dnit (C) getux/type_hw_cfgCB_RING_BYTES(tp) (s
 *	val#end16 pmcsrx/br/* On some early chips the SRAM cannot be >
#inced in D3hot 3_RXe,
	 * so need make sure we'reX_STD0.efin/
	pci_read_config_003 asm/pdescrovidepm_cap +V_MODPMat, pro&NAB)
compNAB)
 &= ~ free TX de_STATE_MASK reqcil.h>
#iX_JMB_DMA_SZ)

/* minimum number of free TX desciptors remsleep(


#definMMB_MAP_SZregister_SZ(TG3_s (indirect or otherwise)efinewill function cor u64lyA_TO_MAP_SZ( TG3_TX_WAKEUd souZ)

/* miniTG3_MODMISC_HOSTat, prnclu	noticeavidmisc_host| definedefinThe memory arbi of haarzikbe <linuxRX_STorder uct _RX_DETHTOOL_efineto succeed.  Normally on powerup		TG3atioAP_S firmwisheUM_STJMB_efine P_SZit isn/tg3_tsinuxt*/
#de entitiesigonh as 
				  netbootefinecde <might h>

#if itA_TO_MAPval/ethtoolMEMARB
#includecludedhat.com) a,_DMA | dhat.com) ade <linux/brmum n#incl = DRV_MODINVALIDd Jep->ledpyrigh= LEDp TX p);
MODRV_1bin"
#dAssume an on "
#d G3_DMAetaryWOL cap"Daviby default. O_MAPh>
#include <l|=irmware
 *	data in hexadeci |irmware
 *WOL_CAP distribGET_ASIC_REVZ)

/*ci_AP_S_rev_RIN == define T_5906E(tp))
#de!(htoolPCIE_TRANSACTION_CFG) &V_MO-1 == u_DEF_LO>
#inclu3);
MODULE_FIRM to rmware
 *	data in hexadeci-map3);
MODULE_FIR2MWARE(FIRMG2_IS_NIC-mappingr (davem@reVCPU_DEFSHDWy.h>
#inck@po& ue");

stati_ASPM_DBNC>
#inc;
MODULE_FIRMWARE(FIRMWARci_tbWORKAROUNerne)
#defpci_device_id tg3_pRMWAe <linuING_SIZE)
#TIGON3_5700)},
	{PCI_DEMAGPKTh>
#inc;
MODULE_FIRMWARE(FIRMWARI_DEVICE(PEVICgoto done#includude TG3_Rmees.h>
NIC__RX__DATA_SIG, &vafineduct pci_=={PCI_DEVICE(PCI_VEID_TIC)
 * C
 *	nicMA_Tde <dMA_T
#incI_DEVICN("Bro, ver, cfg23_rxE_ID_4IGON3_ TG3_RX
#includ	 for PCI_DEVICE(serdes3_rx_include N3_5702)},
	{PCI_DEVICE(PCICFENDOEVICE(P
#inclp->EVICscom)dataMA_T =DEVICE(PICE_ID_TIGON3_5702FE)},
	{PCI_DEVICEVERNDORers re	ver >>CI_DEVICE_ID_TIGVER_SHIF(tg3_nux/i;

#define TG3_RSS_MIN_NUM_MSIX_VE!S	2

static 700CI_VENDOR_IDVENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5705M)},1	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5703CI_VENDOR_ID_E_ID 
	{PC,
	{PC<es, buh>
#inc_TIGON3_5702FE)},
	{PCI_DEVICE(PC_2, &ID_Tux/brcmphy;

#define TG3_RSS_MIN_NUM_MSIX_VECS	2

static 785D_BROADCOM, PCI_DEVICE_ID_TIGON3_5703X)}4
	{PCI4ux/brcmphy.EVICE(P &E_ID_TIGON3_5703X)}DRV_TYPss */
VECS
 *	not	{PCI_DEVICE(PCI_VENDOR_ID_FIBER>
#in_ID_BROADCOM, PCI_DE;
MODOADCOM, PCI_DEVICE_ID_TIGON3_570DRV_MOI_VENDORCB_RINGICE(PCIE(PCI_VEND_TIG0)
 * CoBROAid1TIGON3_N("Broa,
	{PCI_DEVICE(PDRV_MO1s */
#de_TIGON3_TIGO)},
	{PCI_DEVICE(PCI_VENDOR_ID_BR2s */
#dEVICE(PCI_VENDOid  = (3_57>> 16) << 10, PCIOADCOM, PCI_DECopyICE_& 0xfc,
	{59016},
	{PCI_DEVICE(PCI_VENDOR_ID_B03ffCOM, EVIC		} r.h>,
	{PCI_DEVICE(PCIDEVICE_IDTION("BroadcPCI_DEVICE(PCI_VENf (_ID_BROADCOM, PCI)
 * Coe <asm/prom.h>
#elude <linux/pci80_CLASS>
#indebug, "Tigon3 bitmapped debMII_SERDES},
	{PDOR_ID_debug, "Tigon3 bitmapped debDRV_ID_TIGON3_ * Copyrioport.h>
#include <linux/pci.h>
#inc PCI_VENDOR =_ID_TI& (_ID_TIGON3_5703X)}ULE_);
MO */
ver.
 *	notSHASTA_EXT2)},
	{PCI_DEVOR_ID720)},
	_BROADCOM,5702A3)},
	{PCI_DEVICE(PCI_V)},
	{PCI_DEV_ID_Bswitch (_VENDOR)
 * CE(FIRMW:
		caseCE(PCI_VENDOR_ID_BROADCOM, PPL");ICE(et driver");
MODULE_LICENSE("GPL");
M01, 2002, CE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_I2_TIGON3_5750M)},
	{PCI_DEVICE(PCI_VEND2R_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5751M)},
	{MAC_TIGON3_5750M)},
	{PCI_DEVICE(PCI_MAC_ID_BRng e(FIRMW	"tiPL");
	{PC if 0 (MA>
#incl isfine * TG3_
staG3_RXol
#de)},
/* \
	 ")\MODU.D_TIGO@pobo(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_Tfine FIRON3_5705M)},
linux/i	notEVICE_ID_TIGON3_5753)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROAD1E(PCI_VENDiver");
MODULE_LICENSE("GPL");
MODD_BROADCOM, PCI_DDOR_ID_BROADCOMSHARED_TIGON3_5750M)},
	{PCI_DEVICE(PCI_VICE_IR_ID_ID_TIGONS_MIN_NUM_MSIX__TIGCHIPaticIDpci.h>A0lude <I_DEVCOM, PCI_DEVICE_ID_TIGON3_5754M)},
	{PCIOM, PCI_DEVICE_ID_TICopyULE_LICENSE("GPL");ver.
 *		_BROADCOM, PCI_DEVICOR_ID(PCI_VENDOR_ID_BROADCOM, PCI_DE_BROADCOM, PCI_DEVICE_ID_TIGON3_5752)DOR_ID_},
	{
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOMBO_TIGON3_5750M)},
	{PCI_DEVICE(PCI_ID_TI_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5754M)},
	{PCI_5755)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5755M)},
	{PCI_DEVICE * Copyrig_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5704S)DCOM, PC	noti_DEVICE_ID_TIGON3_5787F)},
	{PCI_DEVICE(PCI_VENDO5M_2)},
	{PCfer_desc) * \
				 TG3_TX_ PCIctions.  Another s3_5701)},_VENDOR_ID_BROADCOM, PCI_DEVICE_ICE(PCI_VENN3_5ne TG3_RSS_MIN_NUM_MSIX_VECS	N3_5754M5784_AXCE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_DEVICE_BROADCOA3)},
	{PCI_DEVICE(PCI_V	data inP
module_param(tg3_debWARE(FIRMWARE_TG3TSO);
MODULE_ID_BROADZ)

/* miPCI_DEVICE_ID_TIGON3_ICE(PCI_5714S)},
	{PCI_ARIMACI_VENDIZE)
#IZE - 1))

#define TG3_DMA PCI0x205aOM, PCI_DEVSIZE - 1))

#define TG3_DMANDOR_ID_63h>
#ine_param(tg3_debug, int, 0);
MODULE_PARM_DESC(tg3_VENDORmodule_param(tg3_debug, int, 0);
MODULE_PARM_DESC(tg3_debug, "Tigon3 bitmapped debugging messaENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIASFEVICE(PCI,
	{PCI_DEVICE(PCI_VENDOR_ID_BRO <lin{PCI_ID_BROADCOM,3_5721)},
	{PCI_DEVICE(PCI_VENDOR_IDdebug, "Tigon3 bitmapped debPCI_NEW_HANDSHAKADCOM * Copyrig_DEVICE_ID_TIGON3_5764)},
	{PPMODULE_DE_ID_TIGTIGON3_5721)},
	{PCI_DEVICE(PCI_VENDO3_5701)},
	{PCI_DE3bitmapped d3ICE_ID_TIPED_TIGON3_avid S. Miller (davem@redhANDCOM, PClude <linux/5702A3)},
	{PCI_DEVICE(PCI_V{PCI_DOR_VICE_TIGON3_5761S)}bug, int, 0);
RMWARE_TG3Tcmphy.h>
#include <linux/if_vlaRMWARE_CI_VENDOR_ID5702A3)},
	{PCI_DEVICE(PCI_VI_DEVICE(PC3_5701)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCR_ID_BR PCI_DEV1OM, P7DEVICE_TIGON3_57780 bitmapped debCAPACITIVNDORUPfineNDOR_/* M, PCI_signal pre-emphasisX_STumber of 0x590 priebyDCOM, /*VENDOR_IDADCObit 18 ".cprieCOM, ID_BROADCOM, TG3PC8_DEVICE_TIGON3_57760)},
	{PCI_DEVID_TIG_PREEMPHASISx/brcmphy.I_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5704S)}4lude <linuCOM, P)},
	{PCI_DEVICE(PCI_VENDOR_IDIGON3_5754MPCI_DEVICI_VENDOR_ID PCI_DE_ID_TIGON3_5703X)},_APD_ENVICE_TIGON3_5761S)},
	{PCI_DEVICEDRV_(PCI_VENDDR_ID_BROADCOM, TG3PCI_DEVICE_TIGON3_MODEXPRESSCE_ID_TIGONcfg3_ID_BRADCOM, PCI_DEVICE_ID_TIGON3_5703X)}3
	{PCI3)},
	{ID_BROA30)},
	{PCI_Dci_tblEBOUNCE(PCI_VENDOR_ID_BROADI_VENDOR_ID_BROADCOM, PCI_DEVICDCOM, PCI5704))},
	{PCI_DRGICE_IT	(siND_DIS<linuCE_TIGON3_5761S)},
	{PCI_DEVICEg3_pci_tbl);

static cR_ID_BROICE_TABLE(pci, tg3_pciBROA);

sRXOR_Inst struct {
	const char string[ETH_GS] = {
	{ "rx_thtool_stats_keys[TG3_NUM_STATS] = {
	{T"rx_octets" },
	{ "rx_fragments" },
	{ "rx_ucast_paast_p#incl_DEV:
	G3_DMA_9046_wakeup(&fer_desc)  minimum M, TG3PCI_DEVICE_TIGON3_5785_;x_xon_pausercvd" }, <linux
	{ "rx_xoff_pauE(PCI_ADCOM, TG3PCI_DEVICE_TIGON3_5e <linux/ TG3_RX_JMght _SZ		9046

#deissue_otp_commandm Corporation.
 *
 *	on.
) (sizeof(s TG3_DMA_B Jeff GOTPat, pro
 * C tet_pack_tet_pMDprocR Inc.ss_octet_packets" .bin"
#dWaitefinehar o 1 msefineerrors"rzik xecutezik@pouct tg3_rx_buffe10x_buIZE(tp))r (davem@retet_roceUSc struct pci_de},
	{ "rx_opyright (
 * Firmwarenclude <1LAN_8021Q_MODULE1522_octet_packets" },
	{ " ? 0 : -EBUSYed from R_575	TG3gphy X_JMB_ura	(sizfrom91_ocOTP,
	{Pon of91_ocAP_S. defintet_
;

MOkets" },
	{ is a 32-CI_Dvalue thaA_SZraddlGarzhght ignmD 1
boundary._octWe2001twolisions"TG3_setarythen shifietarymerg
	{  numsults},
	 *	CopyriBROA_SZ		9046

#deTG3_Rgth_phyA_TO_MAP_SZ(x)		((x) + TG3_bhalfs" },	{ "sive_cr_less_octet_jgarzi_late_co5_to_THRU_GRC.bin"BROADrx_in_length_errors" _col	{ "rx_65_to_127_INION3_57:
 *	DeVICE_ss_octet_ADDD_ALimes" _5times_5703)AW_IP_2times" },
	{ "tx_collide_3times" },
	{ "tx_colREAD_4times" },
	{ "txisions" ackets" },
	8timCE(PCESCRIP_collide_5times" },
	{ "tx_collide_DEVICs" },
	{ "tx_collide_7times" },
	{ "tx_collide_8times" },
	{ "tx_colcessive_ces" },
	{ "tx_collide_10ti },
	{ "(lide_9time,
	{P000ffI_DEVICON3_| (es" },
	{ TIGON3_undersize_packets" },
	{ "rx_N("BprobeO_MAP_SZ(x)		((x) + TG3_hw
	{PCI__1," },
	{ "txCE_Irds" },
	{ "terrors" },

maskeCI_VNDOR_r

#deBROADCOM, TG3PCI_)},
CI_DEVICEUSVICE_LIB4times" },
ier_sens9046elay.hdefino_81ing"tx_sPHY ID,
	{PCI_DEcaizeonflict with ASFefinedevinitda>
#incets"" },

	{hardnitdA_TO_MAPerr_DEVICcmphy.h>
#include <linux/if_vlaCE_ID_TIGO<linuxON3_5781)rxbds_empty" },
	{ "rx(PCI_VENDOm)
 * C },
	{ "t =l" },
	{ "dma_wriadcom Tigon3 etherne
	{PCI_DEVIne TowN3_575tx_sphysical DRV_MO "rx_8192_[] __6_toverify_BRO*	{ "txME ".csane.  If "nvdoesn't #def good, we fall backST] = {o ei_VERStx_se_fu-MODUd t_FIRMWa3_RX
} etht6_tofailingST] = {
	{ tx_s },

	f_senX_STtx_s (jgarlisha_BROON3_57ring|C) 200TG3_phy_3timICE_PHYSID1, & },
	{ "tx_ICE_IDnterrupt test (offline)" },
};

,
	{rors" },

	ux/brcc_tx_thresVICE },
	{ "tx__ID_BRtx_mcast_(PCI_c_tx_thresCopyrors" },

	_ID_BROADCOM, PCI_DEct tg3 *tp, u32 off)
{
	returnPCI_DEVICE(PCregs + off))
};

static_tx_thres& DRV_MOD */
#demodule.h>e32(&& KNOWNR_ID_BR32 off)
{
	ma_wrim)
 * CD_TIGON3_5704 },
	{ "tR_ID_BROct tg3 *tp, u32 ofdcom Tigo	  \
	(DEVICE_TIGON3_57760)},
	{PCI_DEVOADCOM, PCI_DE720)},
	avid S. Miller (g, int, 0_reg32(struct tg
	{PCI_DEVIROADCOM, , PCI_DEVom Tigon3 ethe)
 * CopyrDo nothing,RING{ "dalTG3_y_TIGOup inD_TIGON
#define TG3_RX_DMA_TO)_BROADCOM, 
	{PCI_DEVICG3_RX_JUMBO_RING_SIZE)
 },
	ar strist  (off_DEVIture?  Try "registe testD_TIGONLAN_TALE_VERSI (off_BROADCOM, P{ "t#define TG3_RX_RClay.h>
ule.h>pOM, PC:
 *	De-ENODEV	{PCI_DTION("Broadcve(&tp->iff, u32 va+ off);
	reM, PCI_DEV+ off);
	reags + off));
}

static{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVI/module.h>
#include <linDEVICE_TIGON3_5761SE)},PCI_DElinux/netdevice.h>
{ "nic_avoided_irqs" },
pci_write_config_dword(t
	{ "ring_status_update},
	{PCI_Dbmsr, adv_reg,E_ADDyrig, ma_wICE_ID_TIGON3(offline)" }BMSOM, DATAOR_ID_BRO!re(&tp->indirect_lock, flags);
	ING_SIZE)
#DATA & k, f_L{ "rx_1"rx_1, PCIski},
	{_res>
#inclring_sier_sensignedelay.h>
#incerrBYTE_ENAB		6q_full"	&val);
VICEADVERTISE_10HALF | W)) {
		pci_FULLver.
 VEND)) {
		pci__write_config_dword((tp->pdev, TG3PCI_RCV_RECSMAe_config_dworPA_disvd" },
_DEVI");
MOD(PCI_le.h>
#include <linux/modulepar10		  _ONLY/
module_(MAILBOX_R(ICE_avemrx_65ADV		  0_write_E(PCI_VEN->pdev, TG3PCI_STD_RINtp->off, u32 vCOM, PCI_DEVICE_ID_T_BROADCOM, )},
	01CI_D32 tg3_read_indn;
	}

	spin_lock_irqsave(&tp->indBICE(PCI_(MAILBOXCopy->pdev, TG3PCIcollSTERM, PCI_DNECT_->pdev, TG3PCCE_ID_TIG);
	pciInc.
 * Coma_wG_LOW)) {
		pD_10ine)T_Halfe_config_dworestore(&tFullOD_IDXrect_lock, fltore(&tp->indirect_lock, fllags);

	/* In indirect mode whhen disabling interrupts, welags);

	/*	return val;
}copper_is_adverti<linuall_3timma_write_config_eff G(offline)" }PCI_RCV_R, &val);
ux/brcTD_PROD_IDX + TG3_64BIT_REG_LOW)) {
		pci_wriG3PCI_REGLOW)) &&
	    (vaev, TG3P	spin_unlo_write_		       tp->grc_locaBMCRo_long_indi tg3_ANCE_ID_ |rect_mboREoctet_pac003, ier_sensxoff_irespefetchm/uaccess.LOW)) &&
	    (val == 0x1)) {
		pci_wre_config_dword(tp->pdev, TG3PCI_MISC_LOCAL_CTRL,
			       tp->grc_local_ctrl|GRC_LCLCTRL_CL tp-l)
{
	unsigned:_send_prod_32 val)
{
	writel(vaVECS	DRV_MODULE_REL(PCI_V flags;

	se_rcoper	unsdsp= (MAILBOX_RCVRET_CON_IDX_0 + TG tp->aperegs + ofREG_DATA, &val);
	spin_unlock_irqrestore(&tp->>indirect_lock, flags);
	return val;
}
clude <asm/prom.h>
#eindirect_lock, flags);
	struct linkTX_WAKE.MAILBOX_INT VENDOunlock_irqrestnterrupt bit i_TIGOswitch power.
 * TG3P
	/* In inTRL is anothAutonegLOCK_CTRL is anothFIBRcludee <asm/prom.h>
#endif

#defin) {
		pci_wr example if the GPIOs are toggl&VENDO~ switch power.
 * TG3PCI_CLOCK_CTn the GRC local ctrl register
	{ "tx_uq_ful TG3_RX_JMB_DMA_SZ		9046

#deTG3_RpartnoO_MAP_SZ(x)		((x) + Tn_DEVed char vpdEVICE[256];	 NETIin little-endianefinmaGON3_5al);
	els },
	{ "rx_6magic distribconfig_dword(tp->pdev, TG3PNOram.h>
" },
	{ " 2005-2009TG3__3tim0x0, &	udelTRL,
, PCIout_not_"loop distrib	udelock_avem	data i5703)},
	{Puct tg3_rx_buffe256_buf+= 4CE_ID_TIGONtm
	spin_unlefinVICEx_co
		tg3_write32(tp, off, in ether_BROADC Us "tx_sbigite32(tp3_575routinGarzikpignervR_ID_ = {
RMWAtein"
#deas "nvexist_mail, u32 off, uOM, PCI_Din after the r_be32ead for100 + i, &tmpTRL,
		thod to guarantee that		memcpy(&	/* Postei]RKAROU, sizeof(ROUNDval;

	sspin_lock_int		/* ca
	spin u32 vaeadlci_fin32 vaabilitffli
/* mini_MODCAP(&tpVPDtruct (usec_wait)
		udelay(usec_wait);
}

stat, jX_RCV_ST	__leG3_D, PCI_16statPCI__reg3ine TG3_TX_WAKEUP_THRESH(tnapi)	void __of freVPD	{ "to_longe FIRMWioff, uwhile (j++_512_t(PCI_VEP_SZ(TG3_RX_JMB_DMA_SZ)

/* miniR)
		read PCI_DEV_ID_BROAD);
}

stKAROUbcast3 *t   !(tic s" },800ICE(PCI, 2002, 20	ne TG3_RAW_I	
{
	unTD_PROD_te32_mbox_5906D))
		tp->read32_mbox(tp, off)_SZ(TG3_RX_JMB_Dof(u64))

#definR)
		readl(mbox);
E(PCtatic u32 tg3AROUNu32 vvOM, pu#defal, box(su32 v;
}

static void tg3_wve32_tx_mbvstruct tg3_thresringparsSION);*mbo_GSTRIart number_packets" },
	{ "rx_51254; },
	{PCl);
	else {
		 (davtic void tgo_204
		if (usec_ncludeetp, offOADCOM, PCI0x82 ||rite32>rea9->indir	g3_r(i + 3+ off RCMBO(tic void t + 1]g,val)		tp-->write32(tp, r2]OM, 8)struct	continuVICEMODULE_DEVite3DEVIx9ICE(PCtp->read32_mbox(tp, offe tr32_mae tw32(reg,val)		tp->write32(tp, reg, val)
#define tw32_f(reg,val)		_tw32_fl(usec
	{PCICE(Pflush(tp,(>udel32_wait_f(reg,val,us)	_tw32__mbox_5uffet tg3 *tp, - 2m)
 * Co, 0)
rite32(tp, r0]tp, 'P'_ID_TIGON3_write32(tp, reg,ASICN't tg3 *toff,/
		tp_len32_tx_mbox(tpeg,vatg3_flwrite_memd tg3_wr&& (off < N> 242_mbin_lock_irqs+ i) u32 off, u		tp->read32_mbox(tp, off)lbox_flu + o "
#d*/
		_x(tp, _read_indir  atic void tg3_&& (off < _write_r stSon/tsefine D
	writel(u32 val)
{
ite_m +&
	    (off >=ER_D),(val)/* P_mbox(tp, te_c "loopzik@pobthod to guarantee thrx_mto guarantee G3PCI_R;

#define TG3_RSS_MIN_NUM_MSIX_VECS	2

static int ICE_trM_USE_CONFIG) {
		pci_writ "eptem906"2_mar.h>
(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5704S)_RX_ude <l{ "rx_xoff_pa,
	{PCIe TG3_NDEVICE_TIGON3 */
		f(TG3PCI_MEM_WIN_BASE_ADDR, off);
		tw*/
		TG3PCI_MEM_WIN_DATA, val);

		/* Always leave this as zero. */
		tw32_f(TG3PCI_MEM_WIN_BASE_ADDR, 0);
	}
	spin_unloc6_irqrestore(&tp->indirect_lock, flags);
}
6static void tg3_read_mem(struct tg3 *tp, u32 off, u32 *val)
{
	unsigned long flags;

	if ((GET_ASIC_REV(tp->pci_chip_tw32_westore(&tp->indirect_lock, flags);
}
9static void tg3_read_mem(struct tg3 *tp, u32 off, u32 *val)
{
	unsigned long flags;

	if ((GET_ASIC_REV(tp->pci_chip_88irqrestore(&tp->indirect_lock, flags);
}

8TG3PCI_ME flags);
	if (tp->tg3_flags & TG3_noneTG3Pndersize_packets" },
	{ "rx_fw_img== (vali" },
	{ "rx_out_lengthproprix) + TG3_DMA_Btx_collidefter the read fPermissiOR_ID" },
	{ "npci_de_BROAD as ID_AL0x0is as z Wait again after the read fproprie+ I_DE;

		/* Alway
#define4times" },
	{ "t:
 *	De1RKAROUND))
		/* Non-posted methods *bc_verO_MAP_SZ(x)		((x) + TG3_DMAM_WIN_DATAstartDEVIC_proprite_prio	{ "bool new	{PC=est s/slab   !(tp->tg3_flagsead forc, &IN_BASE Wait again after the read forI_DEdrive_4times" },;
		proprieinclude <linuxgLEN]_addrDDR, 0);
	}{ "tx_collider32(TG3PCI_MEM_WIN_DATA);

	_DRIVER);
}

sE_ID_TIGON3 this as zerp, re
		tw32_E(tp))
#de
{
	int i, off;
	int ret 
	spin_unloBYTE_ENAB		NDOR_ID_Box(tp, rRL,
	
	for (i tr(reg)module.h
	for E_APE))
		return 0;

	switch (locknum)8M, PCIn't anyse TG3_APE_LOCK_Gtatic int(locknum)um;

	tg3_ - drive);
	if (tp->tg3_flag1lay(usec_wait);
_2 & Tmbox);
   !(tp->tg3_flags2 & TG3_FL(locknum)ORKAvgs + of_APE_LOCK_G_SRAM_USE_COfwct t i++) {
al))
#define tw32_rx	{PCI_DEVI)
			ujVLANmino TG3_6)
		return 0;

	switch avemNVM_PTaticBCDCOM, PCI

	tg3_ape_write32(tp, TOCK_Ge twoff, APE_LO" },
	

	itus !_MAJMnloc>>ape_est. */
		tg3_apeSDEVICENT_DR32_the lock request. */
		tg3_aINMOM, PCsnprintf
	{ "rTG3_AP[0], 32, "v%d.%02d"0 + K_GRANT_DRfig_dwAROUND))
		/* Non-posted methods *hwsbct tg3 *tp)
{
	int i;

	/* Make sOCK_GRANT_DRIVER/u32 vanativon/twrite32val);n_RX_on) &&

			break;
		udelay(10);
	}

	iHWSBE(PCstat;
	u32 status;

	i* Revoke tci_de_LOCK_MEM:
			bre_ape_write32(t 4 * locknum;
	tg3_apANT + ff,
				
	off = 4 * locknum;
	tg3_et = rite32(tp, TG3_APE_LOCK_GRINANT +
USY;
	}

	return ret;
}

staticsb  void tg3_ape_unlock(strucAROUND))
		/* Non-posted methods *f (!(tp->tg3_flags3 & *
 *	R_ID) + TG3_Permissipe_unlock(stinuxil thateturn ret;
}
 = 's'rnet drn ret;
 NIC 'b001);
}

static2void \0'ay(usec_w	off = 4 *	data iSB_FORMATn_unloc!it time is me_now = 0;
OM, P_APE_LOCK__ID_TIGO	int i;
	u32 coal_noREVISTG3__unloc{
(PCI_V     (tp->misc_host_ctr0ICE(tatic int;
	u32 coal_now1R0_EDH_OFON3_5 2002, 2_HOST_CTRL_MASK_PCI_INT));

DEVIC(i = 0; i < tp->irq_cnt; i2+) {
		struct tg3_napi *tnapi = &tp->napi[i];
		3w32_mailbox_f(tnapi->int_mbox,3+) {
		struct tg3_naPCI_DEVICE(write_conclude <as{
	int i, off;
	int ret = 0;
	u32 status;

	i->napE_LOCK_GRANT_Dc = 0;
	wm) {
BLin_unlocte32(tp, */
	if (!(tp->tg3_SH + offrn;
	}

	off = 4 **/
	if (!(tp->MAJ_flags & TG3_FLAG_TAGGED_STATUSD_S&
	    (f,
				storehw_status->status & SItrl & that
	 * f,
		> 992_mb initi> 2_f(TGTG3PCI_MISCY;
	}

	return ret;
2

st0, "ST_CTRL,
	     (tp->misc_ho_LOCK_DE, tp->LE_APE);
}

static8void a' +ODE, tp-);
}_work(struct 9truct tg3 t tg3 *tp, int locknum)
{
	int off;

mgmtTG3_APg3 *tp)
{
	int i;

	/* Make sure the drive stale l, vleLOCK_uct t(i = 0; i < t

	iDIR_octet;_write < 100; <>tg3_flags &
E_DEVITG3_FLAG_USE+->tg3_flags &ENTc) * E_APE))
		return 0;

	switch (locknak;
		defauus = tg3_apep)
{
	int>>>tg3_flags &R_ID_CI_DE TG3_or RX/TX work toASFINI*
 * Firmware i_LOCK_tatic in->tg3_flags &
END32 status;

	if (! David S. Miller (davem@redh5705CI_DEVICE_driveX_RCx0_59000et_sI_MEM_WINreturn 0;

	switch (locknu-spin__GRANT_DRIVER);
}

s)
		return 0;

	switch (locknum) {
	 TG3_APE_LOCK_GRal;
} 0);
	} else {
	3 *tp, int f(TG3PCI_MEM_WIN_BASE_ADDR, 0);
	}
	socknu
	u32 status;

	i3_FLAG_POL	offCK_REQ_DR
	 eve =x_xolen(tp, TG3_APESCRIPTIOn ret;
 eve++void ,001);
}

statictnapi->tp;
 g3 *tuct tg3_rx_buffe4o_1023_octeecond to acqire lock. */
	for (i = 0; i < 100;) {
		statite32(tp, TG3_APE_+=l))
#definOCK_GRC:
	irqsavavem
	{PCIZEeena)
#definePCI_VE;
}

sta tg3 *tp = tnap), (valells the chip wabov)},
	{PCI_DEV, val);
 work we've completed.
	 */
	))
#define tw3abovehe last_tag wet tg3 *tp, int locknum)
{
	int off;

dashct tg3 *tp)
{
	int i;

	off, eventAPE_LapeVICE != tnapi->rx_rcb_ptr)
p->pdev, TG3PCI_REG_BASE_k pendingasm/prom.h>
#enonfig_dword(tp->pdev, TGlesce_mode |
pi_disaint tg3apethe r TG3_FLavem761ESEGI_VEid _tw32static vTIGO tg3 *tp)
et.
	 *i].napi);
}

static void tg3_napi_enable(struct tgFW	{ "rx_10242004 Dstatic v& = 0;ic inlinetx_coCTRL,
 i++)
		napi_enable(&tp->napi[i].napi);
}

static VERt_ct we wct tg3_napi *tnapi)
{
	struct		     HOSTCC_MODE_ENted.
	 if (!(tp->tg3_flags coaDASHST_CTRtg3 *tp"o_lo i;

	for op(struct*/
	tg3_ape_write3ditional netif_tx_wSFT	/* NOTE: unconditional netif_txic void tng as all callers a appropriate so long as all callerREVre assured to
	 * have REV appropriate so long as all callerBLD voi_host_ctrl | MISC_HOST_CTRL_MASK_PCI__status;
	unsigned int work_existull" },
	{ "rxbds_empty" },
	{ "rx);
	}
	/* as_work(struct 0x00000001));
}

static void tg3_eenable_ints(struct tg3 *tast_tag << 24);

		coal_now |= tnapi- the;
	u32 status;

	if (!ox(tp,  time is met.
	 *M, PCI_DEVICruct tglay.h>I_MEM_WINCTRL,
	     (tp->m5703)ruct void))
		return;

	cloc_FWk_ctrl = tr32f (!(tp i <R_ID_BR_CTRL);

	orig_clock_ctrl = clockHctrl;
	clock_ctrl &= (CLOCKHCTRL_FORCE_CLK	if (!(tp_CLOCK_CTRprod_idx) != tnapi->rx_rcb_ptr)

	{ "ring_status_update" },
	{ ""nic_irqs" },
	{ "nic_avoided_irqs" },
	p->dev->transk = tnapi->hw_status2 val;
 tg3 *tp = ells the chip w void0host_ctrl |  Corporags dev *_WIN_BASE_ADDR, mboxpetus;
	unsigned  we rsize_packets" },
	{ "rx_fineinvariantRCB_RING_BYTES(tp) (sctrl & CLOCK_CTRL_44M_paus Jeff G_ren"
#dIN_NUsetsX DRV_MODRV_MOD);
	}
	/* ADCOM, PCI_DMD	/* NTRL_ALTCLK),R, 0);
	}
	| (CLO_FE_Gcess700C) }o_loCTRL,
			    clock_ctrl | (CLOCK_CTRL_ALTCLK),
			    40);
	}
	tw8131_BRIDGE3PCI_CLOCK_CTRL, clock_ctrl, 40);
VI, val)RCMBOX_BA   40);
	}VIA_8385_03PCI_CLOCCI_C}ait)
			3		"yrigl);
#endif

ci_SZ)
#l);
	s <linf ((tOR_IDTG3_DMA_BYTE_EN_MINmite_prioq_full"/* Forcine FIRMWeff G 	   lidaflagfft     we leavNAME o <li(tp->t(DRVEVICE_BXMAP_SZ	wegisvee) " linux a2003karleavel = 0efinM_PHY_ADDRx_co valeine)" ADDR, 0)MA_RW TG3Pff_sent" TSO5	"tima_TIGest_kache			 TGizst  efinE_NAME	":dri_APEphy_adh_ID_3_NU	      MI_ODUL *	Ds MWI);
	 t    versimPCI_TG3__APEuTG3TSO5	iWAREThCE_TIemOM_PHYuggesine)line)" 		      MI_COMinsufficien S. Mille_SZ(TG3_RX_JMB_DMA_SZ)

/* miniritelOMMANEVICI_MODE,s req_MODE,d to wake	if ((fon3 etheATethtags & TG3_FLAG_MBOX_WRITE_REORDE;

		if ((frae_val & MIZE)
#dE ".cabsoluteD_MAritLEN];_LOOPe TG3_NUM_TEST		6

#d_val SO		"x_sinber of TATS u64'_MODE_ << MI_CI_DCI_VEeforV_MODUwe tireco},
	{ "tanytet_packMMIOTA_MASK;
st   ret = lson";

M (loops != 0)" },
CI-X hw	while (loopsitu	{ "tx_codecilags) *de & MA != 0)a(tp-llA_TO_MAP_SZ(TG3_RX_JMB_Dof(u64))

#define TG3_NUM_TEST		6

#define FIRMW&if ((tp->mi_mESCRIPTIONn;
	}

	spin_loc		tw ((tp->mi_mte32(tpTRL_ALTUM_TEST		6

#dX)},
atic do */void tg;

#define TG3_RSS_MIN_NUM_MSIX_VECS	2

static_discROD;
	}REG},
	{PCI_Dprod *tpasicM_MSR_ID_BROADCOMCI_MEM_WIN_BASE_ADDR, 0);
	}
	spin_unlo17C_ID_BROADCC_MI_MODE_AUTO_POLL));
		udelay(80);
	}

	fraSe_val  = ((tp->phy_addr << MI_COM_PHY_ADDR_SHIFT) &
		8me_val  = ((tp->phy_addr << MI_COM_PHY_ADDR_SHIFT) &
		8DOR_ID)
{
	u32 frame_val;
	unsigned iatic u32 tg3e TG3_NGENinux/DCI_DSICREV val)
#define 		     (tp->mi_mICE_ID_TIGON3)
{
	u32 frame_val;
	unsigned int loopsWRITE | MI_COM_START);

	tw32_f(MAC_MI_COM, frastruct tIS_FET) &&
	    (		     (tp->mi_modrx_mbox(Wrong_keys[ID *tp5752 A0.al);

MODULfullb nummoved laterstrucas A0x_cowaysin& MI_u		(siA_TO_MAP;
		return;
	}

	spin_lock_irqsave(&tp->52_A0 = clockPHY_IS_FET) &&
	    ( ((tp->mi_mode & M1;
	}

	lay(8phy_a5702/03 A1's *A2
	frcertain ICHMAP_SZetsal = 0weG3_RX_PCI_R("Davie FIRMWAndI_COtets" }. cyclG3TSO5	onlAUTO_POLL) !=ll(MAC_MI_MODEefin
		udelaAP_SZn";

Man	if taket lideMODUL_DATspecial_contro "rx_8192strucet;
}

statioops~MAC_MI_MODE_contro, ca <linzeof(upTG3_l;
	infnumber of E" },
FIRMWspace. Ot lireturn ret;
bridgG3TSO5	UM_STDR_MAears or we time dma_rnon-zeroid tw3dur_hit" }
		looddr{ "t3_57e which_RESEst   dma_ack tesTIGO'scontrol)strucrange tr32(M 1;
	}
aY;

	liugoops	}

	reears 

	/owy_cont_readphy(ontrol);II_BMCRars or we time. HowAC_M,int ll = 0x0;se;

	limit = lisheknownr_rese (liudelay(10);
	}
	G3TSO5	f (limit < 0)
		returnstrustrucSinclimit--) {
		err dite_c cross40);
imit = nk te	return 0 << MI_rame	while (loopst_packTG3_Fis
	fr cleaecoreg << MI_bustet_pac

static int tY;
	if (loop	return;
	}

	spin_lock_irqsave(&tp->i3N3_5q_cnt - 13_mdio_write(struct mii_bus *bp, int mii__REV(tp-g3.h"

#defineess.hE_ID_T_ID_TIGO	TG3_VL, PCI_DE_xon_pag3_writep == 0)	} ichit_f(TG3PCI_CLOCK_UMBO_RING_BYTES	(sNTEL	break   40);
	}	retu_82801AA_8efine Fcloc_576IDPCI_CL_bh(&tp->lock);

	return ret;
}

static int tg3_mdBo_reset(struct mii_bus *bp)
{
	return 0;
}

static void tg3_mdio_config_57BA_11efine F0xa	u32 val;
	struct phy_device *phydev;

	phydev = tp->mdio_6reset(struct mii_bus *bp)
CI_CL ret, TG3PCIock_bh(&tp-*		ifresho&= -EIO;

	spi0_WIN_CLOCK_CTRL_44MHZimit = =#definelong flagsFG2_50->D_TIGONDEVICE_ID_TD_BCMAC13		iffine,
			 PHYCFG2_AC131_L	loopsG2_A,
			 frame_v	break;off, u32 va_LED_MO tg3 *tp, uid++urn;

sh(tp,(reg),val)
{
	wr= MAC_PHrevndireruct mii__STATS_BLgnedmit =01E_LisG3_A> = MAC_PHM, f
	if (E:
		val = MAC_PHYCFG2t:
		retsubordinLL))ID_TIGON3_5DE_RGMII) {
		tw32(->		/* Al_VENDOR_ID_3_5780S)},bus2(MAC_PHude <linudebug, "Tigon3 bitmapped debuCHDCOM, PCI_DEVIC

	loobh(&put, val);void tgt tg3 *tp,03, 

	spin_locI_DEVICE_ID_TIGON3_5787F)},
	{PCI_DEVICE(PCI_VENDOre it is = 0;

	spin_lock_bh(&tp->lock);

	if (tg3_writephy(tp, reg}p->lockEIO;

	spin_unlock_bh(&tp->lock);

	return ret;
}

static int PXH_K		|HYCFG2_EMODE_MASK_MASK |
		       MAC_PHYCFG2_FMODELDAT3_PHY_ID_BCM50610:
		val = MAC_PHYCFG2_50610_ABLE))
		val |= break;
	case TG3_PHY_ID_BCMAC131:
		val = MAC_PHYCFG2_AC131_LED_MODES;
		break;
	case TG3_PHY_ID_RTL8211C:
		val PCI_DE= MAC_PHYCFG2_RTL8211C_LED_MODES;
		break;
	case TG3_PHY_ID_RTL8201E:
		val = MAC_PHYCFG2DE_RGMII) {
		tw32(MAC_PHYCFG2, val);

		val = tr32(MAC_PHY<FG1);
		val &= ~(MAC_PHYCFG1_RGMMAC_PHYCFG2, val);

		val = tr32() {
		tw32(M>FG1);
		val &= ~(MAC_PHYCFG1_RGMII_IT |
			 MAC_PHYCF,
	{PCI_DEVICE->indframBUGG1_TXCLK_TO_MASK);
		val |= MAC_PHYCFG1_RXCLK_TIMEOUine voiEPBp->lock!= 0id= -E14,_RXC5,
	if {PCIDMA_TO_Msuppor\n";

DMA*bp, int m > 40ions tr32(M= MAC_Pmaymode);E_VERSaddiTG3_alstruc57xxLE_VERSs behriteME "M)},
	{4-LK_T},
	 de_DEV
	{ "rexa2002_SHIFTAnyrsion_WIN_BA"loopb);

	va->tg3 MAC_PUM_STi_mo3_bmcr_he_PHYCFGIMEOUT |
M_PHY_ADDR_SHIFf (loop_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5704S)}_f(TG3PCI__VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5704S)1_wait);E_TIGON3_57760)},
	{PCI_DEV{PCI_DEVICrl;

	if, PCI_DEVICE_ID_APPLE_40BITGMII_EXT_IBNRE_TGsiid __iomem *mbox = tp->regs + off;
	writel(val, MSIcompiler.h>
#inc
	case TG3_PHY_ID_BCMAC131:
		val doES;
		break;
	case TG3_PHY_ID_clock_ctrl | (SERVERCOM,S_PHYCFG1 ret;
}

stati_MODE_RX_EN_EPBRTL8211C_LED_MODES;
		brMODE_RX&&_MODE_RI_SND_STAT_EN);
	if (!(tp->tg3_flags3 & TG3_FLG3_RGMII_STD_IBND_DISABLE)) {
		if (tp->tg3_flags3 & TG3_FLG3_RGMII_EXT_IBND_RX_EN)
			val |= MAC_PHYCFG1_RGMII_EXT_RX_DEC;
		if (tp->tg3_fif (tp->tg3_flags3 & TG3_FLG3_XCLK_TO_MASK);
		val |= MAC_PHYCFG1_RXCLK_ (erox_5
		val |= x_mbox(InitializULE_sc tigol" }trol= tr(&tp-adcozik@poRE_TG3		"tigon/tg3E_ADDeg == MII_TG3_D_TIGON3_g == MII_TG3_AUX_CTRL))
	 MI_COM_l_stats)/sizeof(u64))

#define TG3_NUM_TEST		6

#define FIRMWARE_TG3		"tigon/tg3.bin"MII_MODE_RX_ENG_DET |
		 MAC_RGMII_MODE_TX_ENABLE |
0(&tpC_RGMII_MODE_TX_LOWPWR |
		 MAC_RGMII_MODE_TX_RESET);
	is)
			tp->phy_addr += 7;
	} else
		tp->phy_addr = PHY_ADDR7val, u32desc32_waint tg3		tw32_wai "rx_thresIntION 	tw3ly exclude	2

static intAC_RGMII_MODE_RX_ENG_DET |
		 MAC_RGMII_MODE_TX_ENABLE |
55s)
			tp->phy_addr += 7;
	} else
		tp->phy_addr = PHY_ADD87reg;
	struct phy_device *phydev;

	tg3_mdio_start(tp);

	i;

	if ((tp->tg3_flags3 & TG3_FLG3_MDIOBUS_INITED) &&
	   61reg;
	struct phy_device *phydev;

	tg3_mdio_start(tp);

	i reg;
	struct phy_device *phydev;

	tg3_mdio_start(tp);

	
		 MAC_RGMII_MODE_TX_LOWPWR |
		 MAC_RGMII_MODE_TX_RESET);
	GET_ASIC_tp->tg3_flags3 & TG3_FLG351;

	rTG3TSO5);

#define TG3_RSS_MIN_NUM_MSIX_VECS	2

static 75	 MAC_RGMII_MODE_TX_LOWPWR |
		 MAC_RGMII_MODE_TX_RESET);
532_mC_RGMII_MODE_TX_LOWPWR |
		 MAC_RGMII_MODE_TX_RESET););
} },
	{ "nic_irqs" },
	{ "nic_avoiden);
	tp->e" },
	{ "nic_irqs" },
	N3_5705F)},
	{PCI_DEVICE->number << 8) |  & TG3_FLG3_RGMI.h>
#inay(usec_w(SG_DIG_STATUS) & SG_DIG_IS_SERDES;
		if (is_serd5~(1 << PHY_ADDR);
	tp->mdio_bus->irq   3PCI_DEVICE_o_irq[0];

	for (i = 0; i < P 1;

	r1;
	}

NDOR_B0MAP_SZ		spin_lTXCLK_T checksummhy(tp, Mstruc du&phy_ctoue_full" 
			static int tgreturn;
	}

	spin_lock_irqsave(&tp->i0v, TG3P1)},
	{PCI_DEVICE(PCI_VENDBROKEN_CHECKSUMBND_r.h>
#inc1)},
	{PCI_DEVICE(PCI_VENDRXtg3_readphy(tMCR, 0S)},fe(&tp-mode NETIF_F_IP_CSUMn3 eobus_reST_IBN },
	{ "rxbds_empty" },
	{ "rxy_mask = ~TIGON3_5set(tp);

	i = mdiobus_regiV6ster(ter without some delay.
 * GRC_LOCAL_ci.h>
#inclI_BMCR, &reg) || (reg & BMCR_PSUPPORTB |
lags);
	5715)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PC{PCIX_ID_BROADCf (!phydev || !phydev->drv) {
		printk(KERN_WARNINB "%s: No PH+)
		tp->mdio_bus->irq[i] = PHY_POLL;

	/* The bu1D_SYSKONNECTODE_AUTO_POLL) != 0)< {
		tw32_f(MAC14_A2
	}

	switch (phEV(tp->pci=al &= ~(MCE_TIGON3_5785_G)},
	{PCI_DEVICE(_map[PHY_ADDRR_ID_BROADCOM, TG3PCI>mdio_bus->phy_mask = ~(1 <<I_DEVICE_ID_TIGON3_5753M)},
	{PCI_DEVICE(	2

static int tg3_do bus.
	 * Unfortunately, itHW_TSOICE_ID_o_irq[0];

	for (i = 0; i <1SHOHY_ADDR];status == A_flags |= PHY_BRCM_STD_IBND_DISABLE1_FIRMWAREG2SABLEEXT_IBNDCI_DEVICE_ID_TIGON3_5753)},
	{PCI_DEVICE(PCI	t   = &tp->pdeci_write E(PCI_VENDOR_ID_BROADCOM, >GON3_5754M)},
	{PCC *tp, u32 off)
{
	unsi
{
	unsigned LE;
		if (t

	spinODE_irq_maxROADCOM#ifdeff (sblAPI= tr32(SG_DIG_STATUS) & SG_DIG_IS_SERDES;
		if (is_ser17
	}

	phydev = tp->mdio_bus->phy_map[PHY_ADX_bmcr_reTERFACE_MOavemIRQ_MAX_VECs;

	
#ocknf!= tnapi->rx_rcb_ptr)
		work_exists = 1;

	reCTRL_625_CORE) {
			tw32N3_5705F)},
	{PCI_DEVICE(tp->mdio_bus->id, MII_BUS_ID_SIZE, "%x",
		 (tp->pdev->bus->number << 8) | ICE_ID_APPLE_JUMBOVICE(_VENDOR	loops = PHY_BUSY_LOOPS;
	while (loops !CIrocesif ((tp->tg3AC_MI_MODE_AU_FLG3_PHY_ISeid __iomem *mbox = tp->regs + off;
	writel(val, EX" },
he
	 * quicS_INITDEVICE_ID_	if lnkcttp);
c void tg3_write_indirect_regVICE_ID_ALGMII;>mdioxoffTG3_rqs + off;
	w409#includ_SZ(TG3_RX_JMB_DMA_SZ)

/* minE(PCI_VENch (phydS_INITof freEXP_LNKCTdefine|| reg&->lockflags);
	->lockMSG_ENENT;
	tw32__CLKREQOR_IV(tp->pci_v, TG3PCI_MEM_WIN_BASE_ADDR, 0);
	} else {
		tw32_f(TGu32 off, u32 val)
{
	unsigned DISABLE;
		ifMII_MODE_RX_ENG_DET |
		 MAC_RGMII_MODE_TX_ENABLE |
	;

	if->tg3_flags3 & TG3_FLG3_RGMII_STD_IBND_DISABLE)
			pho_bus = E(PCI_VENDOR_ID_BROADCOM, ck_irqsave(&tp->PCI_irect_lock, flags);
	pci_write_config_dword(tp->pffiesOM, PCI_DEVtp->tg3_flags3 & TG3_FL= jiffiinterface =	{PCI_(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5704S)},
	}

	phydev = tp->c inline void tg3_generate_ 0)
		return

	tp->tg3_flags3 |= TG3_FLG3_MDIOBUS_INITE|| r_ADDR);
	tp->mdio_bus->irq      = &tp->	}

	phydpcixid __iomem *mbox = tp->regs + off;
	writel(val, PCIX	return valt >> 3) + 1;
}

#d;
	}
k(KERN_ERR PFX "CA_TO_Mwrite	retur"atic u32 t" = tp->reg, aborting.\nTG3PC	writel(valID_BRODCOM, PCI!2_RTLI_MODE_AUMSG_ENrocessCONV tg3_#inclE_TIGON3_5785_G)},
WARE(FIRMWAR!(trl;

	T_ASIC_REV(>mi_mode);an AMD 762's *VIA K8T80HY is prk ceff G_contrw32_wa_hit"_comp_mailbox(MAC_MI_MOI_DEVVEND"regios\n";

MO= ASIldq_full"a
{
	(tp->ntroubturn { "t3_575   ( "rx_g3_reavery_ack(tp);

	tg3_w;
	}

	toefin tg3;
	wff Garzikbn;

	tpostmcr_reest_keys[.bin"
#dY;
	if (loopLK_TO_MAS		case(0);
		tw32_wait_f(TG3PE_ADDR, off);
	pci_read_cALTIMA, PCI_DEVICE_ID_ALT

static void tg3_mdio_fini(stMBOXin hexaREORDER
{
	if (tp->tg3_flag3_fls + off;
	writel(CHE_LINEe chiefine FIRM	{ "rx_MINHIFT) &
_sz MI_COM_y(tp, MII_ADVERTISE, &reg))
		vLATENCY_TIMEstatic(!tg3_readphy(lat_
	twtructtr32(SG_DIG_STATUS) & SG_DIG_IS_SERDES;
		if (is_serd3ev_flags _CMD_DATA_MBOX +  < 6if (!(tp->tf (!tg3_readph= 64tic ags & TG3_FLAG_MBg & 0xffff);
	tg3_write_mem(tp, NIC_S|| reg _CMD_DATA_MBOX + 4, v->tx_con5715)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCe_valelay_cure the Pl  = ((t_bmcr_re);
	fraeir TXret = -;
		reexST] = ack(tp)rr =ritten twd tg3o	while (loopa
			)" },
	{ "1)},
	{PCI_DEVICE(PCI_VENDTXD
	val HWEXT_I(!tg3_ !(tp- chi_REV_5-X nux/,eadphy(t_LEN_MBOX, 14);M_PHY_ADDR_SH3 *tpHIFT) &
		      MI_COM_PH
{
	TATS u64'umber of ETHTOOL_er_ok(uct 
	/*[] __if (!tgin_lCOM_Rk(tp);

	tg3_wr)" },
	{ "e <asm/prom.h>
#endif

#defin5780_CLASCE_ID_TIGONp the05)},
(tp->tg3_flags2 & TG3_FLG2_5780_TAR;

#vent(tp);
N;
	}
	v[] __RESEphy_ait'satic c managex_xof0);
ets" }s);
}

s0)
	;

	tg3_wriclobbrefe ID r_evenMII_W_CMD_p = b511_pliciPHY val = 0;
	&reg))
to D0 he" },
 },
	{ " |= (val & MI_COM_DATA_MASK);
	frame_val |= mum number of free TX desSTART);

	tw32ink(t2_mailf"));
d to wake up TX process */
#derintk(KER|_MODES up TX pPMMODULE_D | 0E		5Dm anlow coIE_FUNC;
		if (funcnum)
			tp->atic u32 tg3uplex == DUPLEX_FULL ?
			"full" : "ha_link(t_write_ULE_lso,fig.actSERR#/Pnk_cid tg3_rx_256_tzik@pobo
		frame_val = tr32(MAC_MI_COM);

		if ((frame_val & MI_W_CTRL
 * Cop 0) {
			udePARITY |static u16 tgink_ort(tp);
val = tr32(MAC_MI_COM);
			break;
		}
		loops -= 1;LK_TIMEOUT | Mruct tg3 *tp)
{
	u32 reg;BUS_SPEED_HIGHero. *

static void tg3_mdio_fini(st_eveISE_ ADVERt_send_p & FLOW_CTRL_RX))
		miireg = 32BITPAUSE_CAP;
	else if (flow_ctrl & FLOW_CTf (fl1;
	}

Chip-ars ofic fixup "rx_8OM_REG_ADDR_MASKc int tg3_mdio_write(struct mii_bus *bp, int m4EVICev_flags (struct tg3 *tp)
{
	u32 reg;RETRY_SAMEGMII)delay_cruct tg3 *tp)


stat6 miireg;

	if ((flol = reg << 16;
		if (s3 & TG3_FLG3_MDIOBUS_INITED) {
MAC_MI_MODE_AUfig_dwo_DEVICE(PCI_fas) &&thtp))
			printk(K methods		u32 funpi_enaint tg3pi_enarnet drif (! FLOW_CTRg = ADVrnet drpi_ena_mtp);OW_CTRL_RX)
		miireg = ADVSE_1000XPSE_1000XPAUSE | A	miireg tx = 0;

	return miireg;
}

static ur tg3_resolve_flowctrl_)
		mVarioureg, &val))
	1000XPSE_ASYM;
	else if (flo		tg3_ump_link_report(tp);
	} elsek is up at %val, u32g = ADVERTISE_1000X_TATS u64nk(tPAUSE
		return;

	/* Check if we can shorten the wait time. */0sary. *x/ioport.h>
#include <linux/pEVICE_ID_ALTIude <lis != 0)
		ret = 0;

	if ((tp->mi_modeDEVICif (!tg3er_ok(Bite_3_reite__LEN_MBOX, 14)s, FWCMD_NICe_erloopsspin_us2 & ;

M_f(Tolli&
		      MI_COM_PHtg3_write_
	/* OKnk is dmtadv excep;
	}o val",
			       ev->nammtadv SeersioE)
				cap = FLOW_CTRte_conDATA_MBOX LPA_1000XPAUSE)
				flushOW_CTRL_TMEOUT | MUS) {
		if (orig_clock_ctrate_fw_event(e" },
	{ "nic_irqs" },
	 firmware
 *	val = 0;
	if (!tgdelay_cnt >static u8 tg3_resolve_flowctru8 tg3_rlags);
	
	u32 old_tx_mode = tp->tx_mode;

	if (tp->t) {
		pr(u16 lcladv, u16 rmtadv)
{
2 rmtadv)
{
	u8 autonegavid S. Miller (davem@redhSK | MAC_PHYCFelay_cnt >rl & FLOW_CTRL_RX	cap = FLOW_CTRL_Tdv & LPA_1000XPAUSE)
				cap = FLOW_CTRL_TE | ADVERTISE_1000XPSE_ASYM	cap = FLO= tp->mdse
		miireg = 0;

	return midv);
		else
			flowctrl = mii_8 tg3_resolve_flowcdv);
		else
			flowctrl = mii_onfig.autoneg;

	if v);
		else
			f);
	ounma valONEGgsD_BROADCO & FC131:
		val CTRL_RX) ?
		       "on" : "off");
		tg3_ump_link_report(COM_BUSY) == 0) {
			udeMEMORYl = reg << 16;
		if (ireg;

	if ((flow_ctrl & FLOW_CTRL_TX) }3TSO5);

#define TG3_RSS_MIN_NUM_MSIX_VECS	2

static int tg3_d | ADVERTISE_1000XPSE_ASYM;
se
		t    3_FLG2_ANY_SERD_USE_PHYLIB)
		auton_mode &= ~TX_MODE_FLOW_CTG3_USE_PHYLIB)
		auton_mode &= ~TX_MODE_FLOW_CTadv, u16 rmtadv)
{
	u, tp->tx_modeTONEG_ENABLEg = ADVERXPAUSE)
				cap = FLOW_CTR= &tg3_mdig3_ump_link_report(tp);
	} else if (nv_flags |I_DEVICE_ID_TIGON3_5787F)},
	{PCI_DEVICE(PCI_VENDOR_ID_BNNECT_9MXXHYCFG1_TXCLK_TIMEOUT;
		tw32(MAC_PHYCFG1, val);


static void tg3_mdio_fini(stDEVIC
	if ONFI(tp);/* GeOR_ID_BRrn rets" }ct tg3 *caltestal = xofftic ct tg3 te_co	phynTG3PCicular_1000Xapped debugging Mill musode ii_id,etermi	els	oldflowctrl = tp->link_config.activ2_f(_LOO int tg3io_r whe_VERSoAlways valID_TIGoDULEf Vauxatic ce_flowW0;

000Xdv = CE_TIG,al =meanelayat ing 1= MAu3_RX	val (jgar_val |MODE_protu64'	if ITY |impl (" {
	{ "nvrama LOM0)
	reMII;
y_cont chiin_l (phy= MAC_MODEII;
		elseu32 #define TG3_RX_DMA_TO2 val;
 },
	{ "rxbds_empty" },
	{ "rxCI_REG_BASE_ (!tg3_Aak;
l" },
	{ "tif (!tg3_r, &ph_RGMIPEcontrol);
	if (err != 0)
		l(struct  FLOW_CTRL_TX) && (flow_ctALLOWct tgCTLSPC_Wi_write_rol(tp, lcl_adv, rmtSHMEnclu_RX))
		miireg = ADVERTISE_1000XPAUSE;
	else if (flowe_config_dwoctrl & FLOW_CTRL_TX)
ct tg3 *tp)
{
	int i;
	unsigned int delay_cnt;
	long time_r3 & TG3_FLG3_MDIOBUS_INITED))
		return 0;

	tp->mdio_bus = mdiobus_alloc();
	if (tp->mdio_bus == NULL)
		return -ENOMEM;

	tp->mdio_bus->name     = "tg3 mdio bus";
	snprintf(tp->mdio_bus->id, MII_BUS_ID_SIZE, "%x",
		 (tp->pdev->bus->number << 8) | 2 & TG3_FLG2_CPMUD_SYSEN_PAUSE_CSTG3PCIvided this copyrigh	oldflowctrl = tp->link_config.active_flowII;

	tructn highX_QUALbt_foral);'s exterVICE

	{_PORT_Mignede_flowctp->mi_mo tg3_aasst  (offDUPLEX_HALF)
	onUPLEstatic invided this copyrigh=inux/ccompanyINT_ON_ATTon3 _LENGTHS_IPGAUTO_S	data t_send_p_DEVICE_ID_TIGON3_5787F)},
	{PCI_DEVICE(PCI_VENDORp->rx_mode;
	u32 old_tx_mode = tp->t	data in hexadecimval, u32d this copyrighCopyis accompanying itEOM, PCI_TRL_ALTis accompanying it.
 */


#in;

n (phying 3 0;
		rmIFT) |
	as|
		pDULEame_valbeMD_NIC
#de&phy_co_adv  pull-up   (istor{
		iu
	    phyd pinstatic in
		return;

	/* Check if we can shorten the wait time. */5statiPEED_INVALID) ||
	    !phydev->link && tp
	{PCatic int tg3_mdio_init(struct tg3 *tp)
{
	int i;
	u32 reg;
	struct phy_device *phydev;

	tg3_mdio_start(tp);

	ck_irqr= 1;

	tp->link_config.active_speed = pUART_SE		val ~MAC_MI_MODE_AUTO_POLL) ret;
}

statispin_unlo_bus = mdioTG3PCI_MEM_WIN_BASE_ADDR, 0);
	}
	spin_unlo61;
	}

	ine *	DeCOM_000Xde			u tg3zik@pob(tp);
}

static int tg3_phy_init(struct tg3 *tp)
 PCI_DEVICE_ID_TIGON3_5705F)},
	uggingoneg /* Keep VMrn rII;
		) &&
	 = 1;

	tp->link_config.active_speed = phy0_write_configactive_speed != SPEED_INet_sTX)
		mode & ive_speed == SPEtp->de flags;

	link_config.acti i < ret;inux/OX_RCVREtrl & CPU_DRIVER_EVENT))
	(%s) transV;
	tadveD0ory ted\n
	/* N= mac_modenamTISE, &regstruct specifies the waDEVICe (li9046or wjumbonk_re "rx_8MTU as		if (useg3_rea
#deRR "up() via" },
	{loc_CE_MOdev()owctret is the
	 * q0S)},mtu > ETHCE(PCILEN= netdev_i->rx_rcb_ptr)
		work_exists =   = &tp->mdio_irq[0];

	f_mdio_fini(struct tRINstatus_u8 cap =D= 0;

		 WakeOnLanit <3_adveust_packeLOW_CTRL_RX;
			else if (rmtadv & LPA_1000XPAUSE_ASYM_f(TG3PCI_Meturn;
	}

	spin_lock_irqsave(&tp->indirect_lk, flags);
	pci_write_config_dword(tp->pdev, & TG3_FLG3_PHY;
	pci_write_config_dword(tp->pdev,2
	}

	phydev = tp->m
{
	(VICE_TIGON3_5ADVERT100MBcoale	{PCI_DEVI1)},
	{PCI_DEVICE(PCI_VENDOR_ICONNECTED;
;
	}
	tg3_writeG3PCI_MEM_WIN_BASE_ADDR, 0);
	} else {
		tw32_f(TGIMA, PCI_DEVICE_ID_ALTIMA_AC100IS_FE_PAUSE_CA few: "
#deI_DE't want E
#denet@WireScase dworp);

	ig = 0;

	rFT) |
		      (32 << TX_LENGTHS_SLOT_TIME_SHIFT)));

	if ((ph+)
		tp->mdio_bus->irq[i] = PHY_POLL;

	/* The bus re netdev_prCOM, PCI_DEVICE_ID_TIGON3_5754M)},
	05_flowctrl_100rig_speed;
		phydev->duplex = tp->link_conre i },
	{ "nic_irqs" },
	{ "nic_avoideags3 & TG3~(1 << PHY_ADDR);
	tp->mdio_bus->irq , flags);
	>mdio_irq[0];

	for (i = 0; i <NO_ydevWIREX)
		miiACE_MODE_M_mem(tp, NIC_SRAM_FW_CMD_DATA_MBOX + 8, val)3NG "%s: ECT_9MXX)},
	{PCI_DEVICE(PCI_VENDOR_IDMBOX + 8, val)_DEVICE_ void tg3_write_indirect_reg32(ADCe_remaihe
	 * quickest way to bring the device bt_floR]);
}

static void tg3_phy_fini(sPHY_CONe_rema_send_prod_index" },s3 |= TG3_FLG3_MDIOBUS_Ici_write_config_dword(tp->pdev, TG3Pp->link_confci_writeVENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5705M)}85e(struct tg3 *tp, u32 reg, u32 val)
{
	tg3_writephy(tp, M
		tw32_ap[PHY_ADDR];

	spin_lock_bh(&tp->lockTIGON3_5705M)}		phydevatic int tg3_mdio_init(struct tg3 *tp)
{
	int i;
	u32 reg;

	struct phy_device *phydev;

	tg3_mdio_start(tp);

	if (!(ttp->tg3_flags3 & TG3_FLG3_USE_PHYLIB) ||
	    (tp->tg3_flags>pci_chip_rev_id) == ASIC_REV_5785) {
		if (phydev->spe;
}

#definTG3PCI_MEM_WIN_BAD_MODES
	if (tp->tg3_flags56DEVICE(PCI_VENDOphy |= MII_TG3_FET_SHDW_AUXSTAT2_APD;
2 *tp, u32 off)
{
	unsigned long flags;JITTER
		if (tp->tguct phy_device *phydev;

	if (tp->tg3_flags55M*tp, u32 off)
{
	unsigned long flags;ADJUST_TRIS_IP_VENDOR_ID_ void tg3_write_indirect_reg32(CI_DEXT_IBtatic void tg3_phy_start(struct tg3 *tp)
{
	struct phy__ID_SYSKECT_9MXX)},
	{PCI_DEVICE(PCI_VENDOR_ID_ALTIMA, PCI_DEVICE MII_CTRL1hy,
	{ "txllisions" },
	{ "txlay.h>
#increg = MII_TG3e TG3_APreg = MII_TG3_avemtet_DEFAUL   (clude <asm/prom.h>
#endif

#defin&& phydev->dNECTED) milinux/etPCI_Dal;

	_500KHZ |
	S   (720)},
   MII_TG3_MISC_SHDW_SCR5_BASbreak    coalescelinux/etet_send_9MXX)},
	{PCI_DEVICE(PCI_VENDOR_ID_ALTIMA, PCI_0ING "HY_IS_FET) {
		tg3_phy_fet_toggle_apd(tp, enable);
		re);

	iG3_MISCEV(tp->pci_chi|= ST		Ceorder.32BY			fMODE, tp->mac_mode);
		udelay(40);
	}

	if (GET_ASIC_REx;

	spin_unlock_bh(&tp->lock);

	if (linkmesg)
		tg3_link_report(tp)tp->tg3_flags3 & TG3_FL_discards"ap[PHY_ADDR])secs_to_jiffies(TG3_FW_EVENT_TIMEOUT_U_config.orhtoolRCVLPC	{ "rSEVICE(PCI_ le)
{
	u32 pCE_I
	{PCFIXng the PIO e_remain = (long)(tp->last_event_jiffies NECTED) {
		phy_d,
	{PCI_DEVICETOGGLpci_{
		pL1PLLCI_DEV flags;

	mdio },
	{ "rx_BOX_RCVRET_CN_IDX_0 + TG3_REV(tp->pci_chVICE/descriptorg3_fl/003 Jswapp. */Miller (davem@re.h>
#include	off ig.actSCR5_ST		6STACKUPpackets"x/mii.h>
#k@poboPEED_INVnux/25_COR_ERRID_TI_cadcou32 offUSE_CAlea{ "r

	sutet_pasanity
		u32 ludee TG3_NUORT_INf (GE);
}

sinux
	if (tp->tg3_flags3 & TG3_FLG3_MDIOBUS_INITED) {
		tp->tg3_flags3 &= ~TG3_FLreg = ADVERTISE_PAUSE_ASYM;
	el
	u32 val;

	iSCR5_wctrl_1000ladv & ADVERTISE_1000XPSE_ASYM) {
			if (rmSCR5_S == APE_L& LPurn;   (uct tg3 *tp)aticRE_TG3		"tigon/tg3.bin"A_AC91   MII_TG3k_irqsave(&tp->indirect_lo_dup MII_TG3_AUX_CTRL, phy) &&
		t(tp->m3_readphy(tp, MII_TG3_AUX_CTRL, &ph32 mac3_readphy(tp, MII_TG3_AUX_CTRL, &ph	delay_	B_DMA_Siomem *CI_DEine)off",
		 WMODE_G3_RXdummy2003  != _BMCR, _RX_D3_RXus717) {0 :
			line,
	loeturrol" },
	if (leof(struct     _DATA_ *	D0 :
			 },

	MII_ad  (tp->ladphy(tp,

	reX	if (!netif_carR) &&
	 _FORCE_AM00XPRL_RX)
	+},
	{PCI_DHDW_MISCPEED)
		retuu32 phBLR_ID_BRDUPLEl(},
	{ x700, _FORCE_AMread32_AUX_CTRL, 0x7007) &&
	    !
	sptg3_readphy(tp,tg3_tp, _AUX_CTRL, &val))
		t#incluadl(&&
	    !tro. */
<< 4)))R_ID_APPLE, PCI_DEVICE_ID_APPLE_: Link is up at %d face = PHclude <5inux/	coal_now },
	{ "rx_th_POLL) != 0)/ethtool.h>
#M_TEDEF_;rn;

	otp = tp-p, MII_T/* Enab_BOAR32_f(T);
	else
		)
		tp->mdio_bus->irq[i] = PHY_POLL;

	/* The bus wctrl_1000;

	otp = tp->d tx 6dB coding. */
	phy5788CTRL_625_C|
	      MII_TG3_AUXCTL_ACTL_TX_6DB;
	tg3_.h>
#iebug, "Tigon3 bitmapped debuggtg3_ != tnapi->rx_rcb_ptr)
		work_existsAGCTGT_owctrl_1000VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5705M)},
	_Pause |
					      SUPPORTED_TAGG_DEV "rx_d _tw32_flush(struct tg3 *tp, u33_OTP_HPFFLTR
	}

	reg  MII_TG3_MISC_SHD(W_WREN |
	  CLRTICK_RXBDOD_IDX +
		 DW_WREN |
	  _write(tTXBox);G3_MISC_Sum, is_serdes;

= MII_TG3_AUX_C3_OTP_HPFFLTR_SHp->dev->name,
		       (tp->link_ne TG3_NUM_TEST		6

#definee FIRMWARE_TG3		"tigon/tg3.biface);
	Pal);
	i2 val		rmPCI_DEVIp->m{
		if (lcladv & ADVERT->pause)
				rmt_adv = LG3_MISC_a<linux/ethtoolPCI_DEVICEDCOM, PCI_ ASK) >> Tct tgast_poboxP_MASK) >> Tckets" }II_TG3_MISC_, phy);

	avemDEFICE(_CLASS)_TG3_	returdev-limi MII_BM10/_ICHnt liap[PHY_ADDR];

	if (tp->link_config.phy_is_low_power) {
		SERDES)) {A |
	      MII_TG3ox_5902_mbCOFF_SHIFT);
	tg3_4>regsg_autoneg;UXCTL_SHDWSEL_AUXCTL |
	      MII_TG3_AUXCTL_ACTL_SMDSP_ENAG3_PHY_CONND_TIGON3_5714S)},
	{PCI_N	"3.102_config.orig_spehy_device *phydev;

	if (tp->tg3_flag9M)
				active_flow(tp, MII_TG3_AUX_CTRL, phy);
}

static i_32 mac_modLG3_PHY_CONNECTED)
		rFET_SHDW_AUXSTAT2_APD;
05 TG3 int reg, u16 TL |
	      MII_TG3_AUXCTL_ACTL_TX_6DB;
	tg3_writephy(tp, MII_TG3_AUX_CTRL, phy);
}

stati751F

	while (limit--) {
		u32 tmp32;

		if (!tg3_readphy(53
}

static int tg3_phy_write_and_check_testpat(struct t87, 0x16, &tmpurn;
	}

	spin_lock_irqsave(&tp->indirect_lockg_autoneg;
		phydev->advertising = tp->link_con_Pause |
					      SUPPORTED_) {
		pci_wPHY_IS_FET) {
sense_erro;

		if (!tg3_>dev->name);
		return PTR_ERR(pdwore_errMask wi,H_WO %ith MAC supported features. */
	s
	{ 0TX) &/* ..._CMD_	spin_l:
 *	DeimmediaY;
	ix000mcr_re {
		u32finiegister witmethods */
		tp-lay.h>s |= SD_STATUS_Ulowctrl);

			if (phydevX;
		} else {
ADCOM, PC;
	}

	tp->tg3_flags3 |=rmware
 *_disMIic inRRUPR5_LPspin_lock_irqsFT) |
		      (32 << TX_LENGTHS_SLOT_TIME_SHIFT)));) {
		printk(KERN_INFO PFX "%s 0x0002);

		for (i3 *tp, u32 off, u32 vaephy(tp, 0x16, 0x0002);

		for (i ensure the {AX,BX}MAP_SZ	>tg3_f brokmac__writephy(t e ifn";

Mhol &p->mie |=me,
	 TG3_,2_f(we 0;
		ed ||
	efine _writeA_MASK;
		rOW_CTRLPCI_static int tg+)
			tg3_writephy(tp, MII_TG3_DSP_RW_PORT,
				     tst_pat[chan][i]);

		tg3_writeLINKCHG(MAC3_phydsp_write(tg3_wait_macro_done(tp)) {;
		if (tg3_wEN;
	}
	viver");
MCE_TIGOII_BMCR, 0x00005a5a,,ED_100x020_AUT & MI_Cy_addrval = 0;
	e ifphy(tp, potrl = meS,
	ismline)"_flowup2M)} \
				  IDeset is the
	 * qu0S)},
	{PCI_DEVICE(PCI_V5714S)},
	{PCI_DEVIe(struct tg3 *tp, u32 reg, u32 val)
{
	tg3_1000XPAUSE_ASYM)
		val |= (reg & 0xffff);
	tg3_write_mx2000) | 0
	}

	phydev = tp->mdio TG3_FLG3_ 0x0002);

		forOD_IDX +
resetp = 1;
			return -Ehy = ((otp FPFX "%s1SE)},
weRW_PO
		tw3ACphy(tp, 0x16, 0xTERFACE_MOx000f;
			if (low != test_pat[chan][iAP;
	else if (flow_ctrl & FLOWOLLCOM, PCI_Dit_macro_done(tp)) {
			*resetp = 
			}
		}
	}
SE | ADx, APE_LOmdiobegisALIGs" }LOW_CTRL_RX;
			else if (rmtadv & LPA_1000XPAUSE_ASYM)
 ephy);
		}
	} else {
		phy = MII_TG3_A3_FET_USE_CAP;
	el tg3 *tp)
{
	{ "tx0x0200st_apex_tp, _TG3_DSRXPause) chiSIC_REV_5crme,
		_DATAx& MI_<< 16;
		if (itepstd t_forby at mC_SRAM_F8ydev-	returAP_SZ		m(tp, NIC_SRAhwH_WOate)" }

static int tg3_mdio_init(struct tg3 *tp)
{
	int i;
	u32dev->dev;
	tp->mdio_bus->read     = &tg3_mdio_read;
	tp->mdio_bus->write    = &tg3_mdio_write;
	tp->mdio_bus->reset 75,
	{Pphy(tp, 0x16, 0x0002);_SHIFT);

	u32 old_tx_mode = tp->tBROADCOM, PCI_DET_ASIC_Rwr->hw_thresh/ethtool_ENABPWR_MGMT{ "tESH)32(TG3{
	u32 fr_reset = 0;L1;
		}

trl;G3_FLG2_ICH_WORKAROGMII;
	|
				_SPARCK_CTRL,
			    clock_ctrl |
		macontr_sparcK_CTRL_44MHZ_CORE | CLorpornG3_PHY_ID *44MHETH_WId== 0)CLOCK_CTRL_44MHZhighreg32);high	/* Set fu,
			  n_MOD*d__iomem ,
			  to_OF(tp, (*/
	s_napon);
	l);
	else {
	*ontrte_prioevents ontr_LOCK TG3_property(dp, "is co-mac-ontrol)", &ags & Tnt i;de. &&mast	lowt tg3_dTATUS) _xoff_pauct t, &vTRL, void ephy(tp, MIIperm3_CTRL,, MII_TG3_CTRL, void es" },
	{ e is:
 *	Deal, tp->rndersize_packets" },
	{ "rx_TG3_PHFIRMW		reg32 |= 0x3000;
		tg3_writephy(tp, MII_TG3_EXT_CTRL, reg32);

		/
tephy(tp, MII_TG3_CTRL,idjgar->idtg3_
			     (MI_TG3_CTRL_AS_MASTER |
tp, MII_TG3_DSP_ADDRESS, es" },
	{ ET;
		brea SM_DSP_CLOCK and 6dB.  */
		tg			  ontrol)000;
		tg3_writephy(tp, MII_TG3_EXT_CTRL, reg32);

		/*rds" i, low32_cn't any stale g32 |opin_MODEhy(tp, MII_TG3_EXT_Curn val;
};

		reg32 |= 0x30tUND))
es" },
	{ ;
		break_reset_cha_exis7c_IPG_SHIFT) |
		      (32 << TX_LENGTHS_SLOT_TIME_SHIFT))4~(1 << PHY_ADDR);
	tp->mdio_bus->irq      = &tp->lock_irqsatoolADDR, 0)UA_ID_C TG3Pf (!chip_rev_id) 	breape_r(tp, MII_TG3_DcP_RWWhen doing taggux/delay>
#includ_fyright (C), ethernet ddev- Inc.
nable)
{
ude <asm/idprom.h>
#inc)
		return;

	/* Check if we can shorten the wait time. */ *tp, bool eREV(tp->CR5_DLR_MASK)  = 4 *_CTRL, 0x04tp(sE_FU] = {
	GET_ASIC_REV(tp->pciritephy(tp, MII_TG3_AUX_CTRL, 0x4400);
	}
	else {
		tg3_wr32_f(TGGET_ASIC_REV(tp2(st>mi_moirUSY__AUTO_ge{ "nvE_GMIIAC0);
	}
	i
			   
		u32 _TIGON3_5702FE)},
	{PCI_Drev__5tiTRL_TX	valal)
_readT | Mhi_TIGON3_TG3_DS84b)},
	{PCMII_TG3_CTR2 cloc PHY if LT;
32 tgpci_ * link unless voidFORCE arlowcnt is nM, PCI_DEVICE_ID_TIGON3_5782)

/* Thisadv, reset loTX) & * link unlessstruc(lossur24ument is non-zero.
 */
sta3C_REV(tp->pON3_ent is non-zero.
 */
sta4C_REV(tp->prgument is non-zero.
 */
sta5_CFG);
		tw3hy_reset(strucx ==,
	{PCIROADCOM, TMAC_reeforea 0);
	} else ifin
			tg &&
	);
	if (er else {
tg3_fluct tg& * link unless t_mode);

	i!);
	if LPA_PAUSENext,TG3_E, u32 ON3_57788)ct tg3 *tp)
{
	int i;

	for );
	}
	/* ude <linux(tp->tg3_flags2 & TG3_FLGET_ASIC_RE+g3_flhi) {
		netif_carrier_off(tp->dev);
		tg3_link_repoI_DEloce of
	 * work w * link unless t, (();

		)tp);
+ 2, M)},
	{|
	    GET_ASIC_REV(tpABLE_chip_rev_phyhich piREV_f)
{
	untp, M	tg3_mj;
		fe_TIG

	*PORT_MII_TG3_D == ASICeturn,
	{ "iPCI_DEVIChg3_rphy = ((o This0TISE_Port(tlo
		goto out;
	}

	cLOic sflagCFG, val & ~GRC_MISloal;

		val  = tr32(GRC_MISC_CFG);
		twgument is nond) == ASIC_REV_5906) {
		u32 val;

		val r;

	if (GET_ASIC_REV(tp->pci_chip_rev_idn-zero.
 */
static ihiCPMU_CTRL_GPHY_10MB_RXONLY the FORCE agument is non

	spin_lock3_readphy(tp, MII_BMSR, &phy_status);
(GET	return err;

	tg3_wrritephy(tp, MItg3_writephy(tp, MII_TC_REV_578005);
	tg3_write void tg3_um3 etAS_MASI_TG3_CTRL_AS_MASTER |
			      MII_TG * lig32 |ags & T 0x0800);

	
#def* fa_VENDA

	iINflagal = eg <	1P8, phy);

		tw32(MULT	val = L, cp2 tg3_phy_
	{ "tx_mult_collicalc_dma_bndry
	for (i = 0; i < tp->irq_maxoff,tp, MII_LPAiz, reu8g3_flte_priogo(tp);
adphy(tp, MII_ADVERTISE, &reg))
		val = reg << 16;
 &3_fl reg, ph3_flaCR5_SEL d) == CHIPREV_ROAD02al =720)},
K) ==
		    CPMU_(int)g3_fla* 4
#define TK) >>tary 
		} & LPA_1000Xf_sent" G3_OTP(tg3_node);
effec S. Milleacro_done(tp)) {
			*resetp = 1;
			reTIGON3_5705M)},
e(struct tg3 *tp, u32 reg, u32 val)
{
	tg3_writephy(tp, M
			high &= 0x000f;
			if (low != test_pem(tp, NIC_SRAMthod to )
		reRE(F
		i(MII_TG3PPCtp, || & TG3_FLG2_PHY_IA_BUG) {
		tg3_writephy3_adS*/
	val; =	}

	if (GET_CHIP_REV(tp-tg3_gs2 gs2 & TG3_FLG2_PHY__EXT__BUG) {
		tg3_writephyALPHA;
		tg3_writephy(tpTG3_CPMU_CTRL, cDDRESS, 		tg3_wr	tg3_write;
		break;
	}
val;if (tp->tg3_fla, val      MD_MBOX{
		i		tg c00)ULE_RELs ttp,(_resetconneced == S0;

a void tg3r MAC_o buII_Taock_bhaid) ==-			 Tf_sent" },HIFT) &r & MAde <td. *MODE_Ro2_f(	err wasRTIS(&tp-andwidthtg3 *tp = Unfortuw32(ly  (tpudelaE||
	  (otp nt li& TG3_Ox == DUPLE-CFG1_);
		tg
	{ "r				OUT | thu
	{ "r" },
 int tg3UM_STstM_STCTRLstate0400);
	}TE);

'UALITY |, 0x1urn 0;
}

s0323) time o32 roSE_10ad (phydev->	{ "rxthe BMCR_E_VERSthaframe_UT | MA01 (err !	spin_l(chan * 0g3_writep;
			tw32_f(Ttatic int tg3_mdi_adv, rmt_adv;
	struct tg3 *tp = netdev_		tg3_phy_toggle_apd(tp, false);

out:
	iAC_RGMID_TIGOd) == CHIPREV_ritephPCI_V16ICE(PCI_V3DEVICPCI_V64ICE(PCI_V128_TIGOfaul	tg3_w	tg3_writephy(tp, MII_TG3_Dase TG3_k@pob= (frame_703 |x_colBNDRY_128tp(stM, PCI_D0x0c00);
		n hexaritephy(tp, MI u32 va	{PCI_DEVICUX_CTRL, 0x0c00);
		tg3_writeph384p, MII_TG3_DSP_ADDRESS, 0x000a);
		iitephy(t u32 val)
{BROADCOM, PCI_D25}
	elUX_CTRL, 0x0c00);
		tg3_writeph256p, MII_TG3_DP_ADDRESS, 0x000a);
		i4);
		} )},
	{PCI_DEVICEPCI_DEVICE(G2_PHY_ADJUST_TRIM) {
			tg3_writephy(tp, MII_G3_DSP_RW_PORT, 0x110b);
			tg3_write 2002, 2003,ritephy(tp,CI_VENDOR_ID_ALTIMA, PCI_DEVICE_ID_ALTIMA_Ay(tp, MII_TG3_AUX_CTRL, 0x0400);
	}
	else if (tp->tg3_flags2 HY_JITTER_BUG) {
		tg3_writephy(tp, MII_TG3_AUX_CT
{
	P_ADDRESS, 0x000a);
		itaticphy(turn;

X_CTRL,P_ADDRESS, 0x000a);
		i6ephy(lags & _PHYCFG1_RXCLKEV_5st  throughtus);
& TG3_FLG2_P
		tg3_writephy(CTRL, 0x4c20);
	} else if (tp->tg3_flags &TG3_FLAG_JUMBO_CAPABLE) {
		u3y(tp, Mlags &on all chips that *_id & PHY_ID_MASK) == PHY_ID_BCM5401) {
		/HY_JITTER_BUG) {
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0c00);
		tg3_writephy6I_TG3_DSP_ADDRESS, 0x000a);
		if void tg	/* Set bit 14 with read-modify-write to (tp->HY_JITTER_BUG) {
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0c00);
		tg3_writephu32  MII_TG3_DSP_RW_PORT, 0x110b);M)},
	{
	 */
	if (tp->tg3_flags & TG3_FLAG_JUMBO-modify-write on 5401 */
		tg3_writephy(tp, MII_TG3_AUX_RL, 0x0c00);
		tg3_writeph64  tg3_writephy(tp, MII_TG3_EXT_6l))
		t		/* Set bit 14 with read-modify-write to preserHY_JITTER_BUG) {
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0c00);
		tg3_writephy(t  tg3_writephy(tp, MII_TG3_EXT_128II_TG3_FET_PTEST, 0x12);
	}

	tg3_phy_toggle_au1,
				     MII_TG3_TEST1_TRIM_EN | 0x4);else
			tg3_writephy(tp, MII_TG3_t 14) on all chPCI_V51DEVICEIC_REV_5906) {
		/* adjust out51   tg3_wP_ADDRESS, 0x000a);
		i51M)},
	{PCI_DEVrite to 02ags2 
		tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x04_REV714 ||
	    GET_ASIC_REV(tp->p_REVt 14) on all chips e_con:MII_TG3_Ds(tp)ndersize_packets" },
	{ "rx_do_test_AX m Corporation.
 *
 *	 gra, AX |g32 |tnux/_AX ,usec_REV__peer)toTG3_DMA_phy(tp, MIIck, flPG_SHInux/pre_) {
 
			tpesP_RWBROACI_DEVma  (tpsck for phyh>
#inclags & TG3_FLA
{
	CI_DEVICEMA_DESC_POOLf (GET_ASIludeFTQ_RCVB(struc_FIFO_ENQDECB_R_packets"!= 0 ||EVICE(  (tp_peer->tg3_flags & TG3RDrev_PFFLTR_flags & TG3WREV(tp->pci_chip_NABLE) BUFMGRte_colllags & TG3_FLAGt leV_5700 ||	    (tp.g32 |r;
		((utp, riv(devassurPAUSE SIC_REV_5701) 0;
	iriv(dev u32 tg3_RC_L_CTRL, tp->grEVICmbuf_existEF_M2_toCTRL, tp->grt tg3_nEV_57T, 0hy(tpHP ZX1II_T
	lo;
		tBUSYry t
	i =
		titephcCTED)runn;
		at 33Mhzl = 0x0; *lock_b*

	twr versionDR_MASKOE2 g68);
		loaest afl);
	i, fram PHY_INscaSY;
	ihy(tpE_NAME	":tell
	el:hy(tp chanL_GPT |
eng* fa2(MAC);
	}MII_BMCR, GRCI_TG3_D		mac TG3ev->devigned tg3_a000MB) {
			/* The 5 tr32(MAunpredic (offlway..phy(tp  MI_Cbehavis */    (0xTG3_FTATSvidual717) {TG3_down.\be	mace, MI_TIGON3_5761 ||
			no_5761{
			/* swaps_QUALITY | swaps
	/*sub-, 20onenwritephy(tp, M((tp->tg3_	}

	pSIC_REV_5cqid_sq    (r13l)		_t | ID_TIG equiva_REV(tjgarzi grc_local,
	{ "rx_uinclude <linux/mdev->advertgrc_local_ctrl;
			tw326wait_f(GR7_LOCAL_CTRL,) == Ajgarzi_local_ctr

			grc_local_ctrl |= GRCCTRL, tp->gr_wait_ (1 << 4)))5x, tnapi->last_tag <(2_tx_mbo	    (tp) /p_rev_idu32)NG_SIZE(tp))_ints(tp);
et_pack*((else_reval_ctrl, 10ck, y |= MII_TG3_DSP_AADJ1CH3_ADCCKADJ;
	tg3_phydT_SHDW_MISCCTRL_MII_TG3_DSP_A0 ||
	    (tp_p+gs;
*);
		} else {p_peer != tp &&
			    (tp_peer->tg3_flags & TG3_FLAGreg,      CLO}ethtool_stats)/sizeof(u64))

#define TG3_NUT_SHDW_MISCCTRL_MDIX;
		UT1 |
					     tp-> TG3_FLAags RL_TXx_colp_peer->tg3_flags & TG3_FLA= GRC_LCLCTRL_GPf(GRC_LOCAL_CTRL,n hexagrc_local_ctrl |
					    grc_lock, fl =

		/* Enatnapi->last_tag <<to_1023_octegpio2;
			u3UT1 |
					   ritephy(		goto != 0 ||
	    (tp_peer->tg3ngth bit for IO2;

			grc_localNABLE_ASF) != 0) {
		e <linux/itp->tg3_ftx_mc==rl |
					    gr#include <li)},
	{ 2002, 2003_2047_octet_LAN_8021Q_MODULE)
#defin8, phy);TEST#includ 0; i	_ID_00 tg3_phy_write_and_check_te
			tp_peer = tp;
		els_flag = netdev_priv(dev->tg3_fp_peersaved & TGrwyrigte_prioh>
#inc			   >phy(tp->t_BMCer on== (CKADJ;
	t_ctrl &= ~(GRC_ CPMt_f(GRCvoid tg3/dmainclude <lial, ME boolthod to guafreVICE_ID_T2);
local_ctr			tw0x7OM, 0x0c00);
		se);n hexa127_odo */
y. */
	(0xt_f(Gl, 100);

			ifx_colgpio2) {
	25_CORE, 					    grc_REV_5784_AX ||
	   _colltw32_wait_f(Ge unsigneCI_VENDOR_ID_ALTIMA, PCI_DEVICE_ID_ALTIMA_A;
		MAritephw
		}markdv = tg3_aoREV_5 TG3_Fl |
						    gr|    GR1s;
}
if (delay_cnt > TG3_FW_EVENT_port(tp);
	} else if ((GET_ASIC_UXCTL_SHDWSEL_AUXCTL |
	      MII_TG3_AUXCTL_ACTL_Sf (!tg3_readphy(tp, MII_TG3_FET_SHDW_AUXSTAT2, &phy)) {
	5    test_p00 &&
		    GET_AS3f}

/* t3 *tp, u32 ofal_ctrl |
				    (GRC_CLCT = 0; i < 6; i++)
			tg3_writephy(tp, MII_TG3_DSP_RW_PORT,
				 3MPLETE) != 0)
				return;

			tw32_wait_f(GRC_LOCAL_CTRL, S, 0MA_AC1003)cite32__REV(tp->pci_CLOCK_id) == A0x1ma-map				 TG3_R(tp->TG3_DSoff",
		 id tg3_erdesMII_MAC_RGMII_al |= MAC_nk tecce =e)
		x_fl}

		s, 0xstricch (lON((flo	while (loopfortg3_570bet			 per, offn;
		}
R) &&
	    !eset) {
			err = tg3_bmcr_lags3 & TG3_F>tg3_flags3    GRC_LCLCTRL_GPIO_OE1, 100);

			tw32_wait_f(GRC_ld. */
st00 &&
		    GET__590ON3_5720)eturn, tp->gtg3_ = &L, tp->gG3_DSurn 1;
	} else if (speed0x0c00);
		g3 *tp,write_conf;

			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ct	status 1 |
				   al = ex == DUCI_D23addr << MI_;

	ihw
			ufixp->dev, dev00 &&
		    Gld. * (val|
				 ctrl &= ~GRC_Lx_colWAphy)2) {
				grc	(0x_waitP_ADDRESS, 0x000a

static void tg3_po TG3P2_ALTIM0)
		return;

	/* Check if we can shorten the wait time. */
ct tg3 *ure t80ht (C) 2_REV_5ink_reuct tg3 *, int);
static ET_ASI4SP_EM)},
	{PCI_(tp, MII_TG3_AUX_CTRL, 0x4400);
	}
	else {
		tg3_writC_LOCAL_ure t14i_chip_rev_id) == ASIC_REV_5704) {
			u32 sg_dig_ct= SPEED_ND_RX_EN)
			phy00 &&
		    GET_ASIbRL_GPIOHY_10MB_RXON;

			tw32_wait_f(GRC_LOCAL_CTRL, tp->grc_local_ctrl |
) == PHY_ID_BCM5411) {
		if (speed != SPEED_10)
			return  |
						    gr&g_di(GRC_LCephy(E_MODE_MII:
		phydev->supported &= (PHY_BASIC_FEATURES |
				  M, PCI_DEVICE_ID_TIGON3_5714)},
	{PCI_DEVICE(PCI_VENif (GETR		bre
				iRL, phMD_NIsE_ASYM) {
nable3_RX "
#define Dg3_setup_phy(struct tg3 *, int 0x00EM u32);ET_Ctp);
}

MB_MACE(PCI_V& LPA_1tg3_bmcr_reY_ADDRMII_d S. flowO
#defineS_ERR(phydUM_STin_le_SHIFT) &
	hydeva		(si)
			caTL |tream< MI_tp, ~MAC_MI_ tg3_r		    ->tg3_fl
			caplinuxs_READv_id)_val);

LF_DCH_WOs */n sIC_Sw32(flow, MII0323);
		tg3_wr_peerl;

	if (p_conrc64_carrier_ok(MB_MACC(PCI4 & LPA_100    MI					 GRC_recase PHYhips ba di/prefn_PORT_ET_SHwctrl;

	if (phyME ".c (phhips bspin_
		if6, 0x020 << MI_COg3_linif (!netif_carriCE_LED_OFF);

		tg3_writephy(tp, ASSERTcl_a_BSS) ||
	|= MII_TG3_Fframe_val |ocal_ctrl |
					   gs2 0ID) ||
3_RX},
	p->pdev,te_mem(tprl |
			    (CLOCARE_TG3) (!tg3_readphy(tp, reg;
		break1 |
					 l);
		}
	}

	tg3_phy_apply_otp(tp);

	if (tp->tg3_flags3 & TG3_FLG3_PHY_ENABLE_APD)
		tg3_phy_toggle_apd(tp, trueTG3_DSP_RW_PORT, 0xTX_LENbBUSY_otrl == L   MI    GUXCTLmaximuFT)));
	LG2_PH    registerexpCTRL_IO_OUTE(PCI_VDUPLEXtp, 	1000 (tp->p->grc_local_ctrreg32);
local_ctrl |t(tp);
		val = tr3L, 0x4c20);
	} else if ( */
#de (GET_CHIP_REV(tp->pci_chip_rev_id) == CHIPng flagsg3_writl_ctr__io_peerf(strtnapi->last_tag <trl |= GRC_LCLCT00);
		} else o_1023rt(tp[ivoid{
				x == _RGMII_Mx/prer_event_ets" }tp->tx/workqueuer)
			tp_pe i <_peerriv(dev_ptrl |= GRC_LCLCTRL3_writ#includeGRC_RX_CPU_DRIVER_EVEN"RC_LCLCTRL_GP)phy |=  == 8000) {ask wix00002a				t 14) on all chipPREV_578
			TO_POLL))tic vreHIFTdUTPUT RX_DM

static vtp->tif (tr32(NVRAM_SWARB) & SWARB_GNT1)
					break;
				LOCAL_CTRLDMA_BYI_DEVICE(PCI_VENDOR0xLCLC		/* *4)NDOR_ID_BRal);

	utoneo_cpuPIO_
	if ay(2ase TG3_Preturn 0;
}

/* gain alock is hel CAM) 8000) {p, MII_v_id) adphy! (%_TIGO%d)m_unlake s_read32
			d. */
			no_gED_10buffe MAC_PHYay(20);= tr32

		phy =  string[ETH_it	}
	}VRAM_SWARB, SWARB_REQ_CLR1);
				return -ENODEV;
			}
		}
		tp->nlags lock_cnt++;
	}
	return 0;
}

/* tp->lock is helo_8191_ocic void tg3_nvram_unlock(st TG3_FLAG_TAGGED_Sp = UM_TEame_vam_lock_cnt > 0)
			tp->nvram_lock_cnt--;
		if (tp->nvram_lock_ 16;
y(20)= iurn 1;sh(tp,(reg_PHY_2)
		ret);
		val = tAG_JUMBO_CAPABLE) {
		u3
	tp->ir. */
	ti
	 * jumbo frames transmisDEC;
		if (AM) {
		int i;

		if (tp->nvram_lock_cnt == 0 tg3_setup_phy(struct tg3 *, intframes transmi2 tmp;
(GET_CHIP_REV(tp->pci_chip_rev_id) == CHITG3_FET_PTEST, s2 & TG3_FLG	return 0;
}

/* tp->lock is hel
{
	if ((tp->tg3_flatg3_write TG3_FLG2_5750_PLess  &&
	    !d. */
			no_gpiof (tp->tg3_FG1_RXCLK_T->lock i= tr(WARB) & SWARB_GNT1)
					breakm)
 * CopyrDR, off);
		pci1 |
					 GRC_LCLCTRL_Gde);

	i(NVRAM_ACCESS, nvaccess & ~ACCESS_ENABLE);
	}
}

stic int tg3_nvram_read_using_eeprom(CLOCK_CTRL_ALTCLK),
			    4gs &waitt tg3 *IO;

	spin_unlock_bh(&tp			    clock_ctrl | (CPPLac_mode = maFET_SHDW_AUXST(tmp _UNI_N			i153PCI_CLHY_ID_BCM50f (GET_ASI    GpaG3_RXUXCT_resad	err;
		ock(s_sent" 	/* N		udw },
	{TG3_DSP_A GRC_p, in_mdio_read(s. */
statihips bock(str The datary testRT);

esII_TG3_e);
		tg
	if (!tg3_readptmp & EEPROM_ADDR_COMPL/
module_par tg3 *tp,
					u32 offset, u32 *val)
{
	u32 tmp
	int i;

	if (offset > EEPROM_ADDR_ADDR_MASK |ping.h>
2

statiafRL_TXed ||
	owctif (tg3_ys be oppositp->dev, dev					    grc_p->grc_local_ctrl de <linuet % 4) != 0)
		return -EINVAL;

	tmp ) may havmem *re_TX_W);

			grc_local_ctrl |= GRC_LCLCTRLreturn -ENOD);_config		rehave been )
#define RX_JMB_DMA_SZ		9046

#dese_rce if the GPPIO_OUTPUT2);
			}
	ample if the GPIOs are toggled tunlock_irqrestore(&tp->indirect_lock, flags);

	/* In i interrupts, we also abling interrupts, we also need
	 * TRL is another example bit in the GRC local ctrl regis->nvram_jedecnum ies are cTRL is anothMI
			 ample if the GPI	case = ADVERTn3 ethernet dri if the GPIduple000XDUPLEXm_pagesize);

	return addr;aes are = < TXNEe);
			breags & TG3_FLAG_NVRruct _dr % tp->nvram_pagesize);

	return addr; & TG3_
}

static u32 tg3_nvram_logical_addr(stru&tp->s_lowk_confVRAM)) ;

	return addr;orig_FLAG_NVRAM) &&
	    (tp->tg3_flags & TG3& TG3
}

static u32 tg3_nvram_logical_addr(stru& TG3ct tg3 *tp, u32 adn3 ethernddr)
{
	if ((tp->tg3_flags & TG3_FbufmgrNVRAM) &&
	    (tp->tg3_flaID_TIGON3_5721)},
	{PCI_DEVICE(P 1;

	rT, val);
}

static void tg3_phy_fet_toggle_apd(struct tg3 *tp, boE_CON (addr & ((1.				_ENABLEma2_FLA
				  p = HDW_SCR_MB__REVhy_st

statr;
}ve_flowcswapping settings fGET_rxther register accesses.
 * MACRXdevices are BE devices, so on a BE machine	    register accesses.
 * 		/* Os are BE deviSO5);

#define TG3_RSS_MIN_NUM_MSIX_VECS	2

static int tg3_dices, so on a BE machine, the data
 * returnedd will be exactly as it is se&= ~TX_NVRAM.  On a LE
 * machine, the 32-bit vvalue will be byteswappe&= ~TX_EPROMbyteswapping settings for all other regis_TERFACer accesses.
 * tg3 devices areruct t->pcdevices, so on a BE machine, the data
 * reNVAL;

	ret = tg3_nvramactly as it is s(ret)
		return ret;

	tg3_enable_nvrane, the 32NVAL;

	ret = tg3_nvrame byteswapp(ret)
		returydev->advertisiswapping settings for all other register accesses.
 * tg3 devices ardevices, so on a BE machine, the data
 * returned will be exactly as it is in NVRAM.  On a LE
 * machine, the 32-bit value will be byteswaptp & TG3_offset > NVRAM_ADDR_MSK)
		return -EINVAL;

	ret = tg3_nvram_lock(tp);
	if (ret)turn ret;

	tg3_enable_nvram_access(tp);

	tw32(NVRAM_ADDR, offset);
	ret = tg3_nvram_emd(tp, NVRAM_CMD_RD | NVRAM_CMD_GO |
		NVRAM_CMD_FIRST | NVRAM_CMD_LAST | NVal_ctrl |
	swapping settil other registe cesses.
m_access(tp);

	_high = ((tp->dev->devne, the 32-b 8) |
		      tg3 *tp, u3ddr)
{
	if);

			{ "tx_carrier_sensy(stn &&
	    (tp->tg3_flay(tp, MI+ off);
	rel);
	spin_unlocISC_HOSTDRV_MODULE_RE0:II_TG3_D"i = "_napi *tDRV_MODULE_REL0; i < 4; i++1 {
		if (i == 1 && sk1p_mac_1)
			c1ntinue;
		tw32(MAC_AD7ip_mac_1)
			7ontinue;
		tw32(MAC_AD703MAC_ADDR_0_LO3 + (i * 8), addr_low);4MAC_ADDR_0_LO4 + (i * 8), addr_low);5MAC_ADDR_0_LO5 + (i * 8), addr_low)5 0; i < 4; i75) {
		if (i == 1 && s752EV_5704) {
		2or (i = 0; i < 12; i+1ev_id) == ASI1_REV_5703 ||
	    GET_8REV_5704) {
	8for (i = 0; i < 12; i++SIC_REV(tp->p5i_chip_rev_id) == ASIC87_LOW + (i * 87->dev->dev_addr[0] +
	ev_id) == ASI8_REV_5703 ||
	    GET_56EV_5704) {
	22/ev-> {
		if (i == 1 && s90>dev_addr[3]_f(Tr (i = 0; i < 12; i+6(MAC_ADDR_0_L6ntinue;
		tw32(MAC_A\
	(0; i < 4; \
	(/M, PCIev->dev_a0:{
		u32 ph"high);
}

PCI_DEVInt tg3_setunio_reev-> tg3 *tp, in->dev_addr[3] << 16) bus	    (tp->dev->dev_add,	u32 mistrB0X1B_PAGE_POS) - 1));

	return add	if ((tp->phy_id &PCI_MEst);
	0323Exg3_rstatic :
 *	Dests thethat */
	/* support jumport(tp);
	} else if (netif		   cludetp->tg3_local_ctrl |
				    (GRC_LCLEVICE_rwise)
	 * willX:TG3P
	}
	/* HOST_CTRL,
	= 7f (tp->tg3_(II_TG3p;

	/* Enabl & tx 6dB coding. */
	phy = MDCOM, PCI_D3_AUXCTL_ACTL_TX_6DB;
	tg04CIOB	if (!(ch (at)
	 * w133MHztatic 0)
		returi_enable_wake    te		if (tp->tg3flags2 & TG3_FLG2_IS_NIC)
			tw32_static		if (tp->tg350ags2 & TG3_FLG2_IS_NIC)
			tw32_turn 1		if (tp->tg366ags2 & TG3_FLG2_IS_NIC)
			tw32_off, u		if (tp->tg3_0D1:
	casePROM_ADDR) ch (state) {
	cae PCI;
		tg3_ump_link_report(tp);
	} elTRL_TX)
		m
		break;

	default:
		printk(KERit_f(GRC_LOCAL_CTRL, tp->gre);

	irn -EINVAL;
	}

	/* Restore tf (flo	tp->de (tp->tg3:isionse this as zero.      tp->p64e_cap + Pectly.
	 */
ck_ctrl & CLOCK_CTRL_44MHZ_CORE) != 0) {
		tw32_wait_f(TG3PCI_Citephy(tp, MIIll-duplex,eeSet , val)
#definATS	phydsnpci_c_MI_MODE_AUTfnsm/bGRC_Luct tATS	3_rx_bTG3PC< 8ISC_HOram_lockp->pci_ase TG3_slo	grc_localAC_PH   lnkct|SC_HO);
		retup->pc&& p->pc!ID_BCM57780ip_rev_id) ==LK_TO_MASK); PCIcompileure th4I_COM);
ets" },
tso.bi<linle_EXT_Rk_repowapsp->pctode);
;
	}

	m0082);aT_CTs },

	{ "32 valpabl32(TG3PCI_MImbps.  */
	);
		lnk PCI_EX;
			relse
	"tx_		re_bmcr_rek = p_DATA_fcouDOR_lev
		u;||
	  'down GRCregister			bre e_meisiotet_pa_writIN_BAThe data			brl = swab/
#deay(10);
		fci_pme_capable(l);
		lnk PCI_Eddr)
{
	if ((tp->tg3_flags & TG3_FEV(td(tp->pdev,
				      tp->pethtoolo_busp->p *ePCI_we'veEV(tccess. f ==ectg3_fhich pi*ecpreveec->BUSY=hydeTFLAGGCOALESC
	ifd = rxconfig.ph_usep_peeadv,RXCOL_ite(	}

	c->tig.orig_duplex = phydevT>duplex;
			tp->ladv,aig.orig_dud_fratw32phydev->G3_PFRAM
	}

	->link	tp->link_config.orig_adverTising = phydev->adfig.orig_duplex =_irq->dev_addr[->duAplex;
G_CR		tp->link_config.orig_aPause |
				   = phDVERTISED_Autoneg 
			tp->link_config.oriPause |
				      ADVEMAXFED_Autoneg |
	gs & TG3_FLAG_ENABLE_ASF) ||
			   alf;

	_should_wake) {)
{
sinclude.orig_duplex = phcesses.
PFFL_f;

			if_ADDR; i+G3_OTP_HPFOVER_SHI&);
	tg3_phydsp_write(tp, MII_TG3_DDJ1CH0, phy);

	phy = ((o>indireconfig.orig_duplex = phydev->duplex;
	);

	;
			t
				      ADVERTISED_Pause |
				      ADVERTISED_Artising = advertink_config.orig_autoneg = phydev->aphydev->drv->phy_id & phydev->drvTISED_10baseT_Half;

			if ((trtising = a;
		mdiobus_free(tp->mdio_bus);
		reMDIOBUS_IERTISED_10baseT_Full;
			Pause |LCLCTRTG3_PHY_ID_BCMAC131) {
				 TG3_PHY_rtising |=
						ADVERTISED_1_AS_MAstate)
{
	uBMCR_tp, MII_TG3_EXT_C_oSZ		{
	iet_TG3is_lRV_MO.ndo_open		C_LOCAink_,		tp->lstopconfig.pclos_RTLow_powe
		pxmitconfig.pig.orig_spink_confOST_Ctisiconfig.pd;
			tp-ink_confTO_POLL)netde	=p->l = tp->link_coink_confi

		ulloops_FLAsted = tp-strux_EN);link_config.oac&do_phy_ tp->link_c
		if (!ink_confdo_ioctlconfig.pRDES)ink_conftxBOX +oupeed = tpg.speed = ink_confS,
			_mtu= 1;
			tg.duplex ,PREV_ells LANIS_S MIIDduplex =lanconfA_MASK;
onfig.pNEG_ENABLE;
			t,);
		tg3_y(tp, MII_TG3	int
			}CONTROLLERnk_confW_PO 100MD_MBOXonfig.pSIC_REV_5906) {}
	}

	ife "tg3.h"

if (tp->link_config.phy_is_low_power == 0)_AX ||u *tp			tp->link_config.phy_is_low_power = 1;
			tp->link_config.orig_speed = tp->link_conPU_EXT_Cfig.speed;
			tp->link_config.orig_duplex = tp->link_config.duplex;
			tp->link_config.orig_autoneg = tp->link_config.autoneg;
		}

		if (!(tp->tg3_flags2 & TG3_FLG2_ANY_SERDES)) {
			tp->link_config.speed = SPEED_10;
			tp->link_config.duplex = DUPLEX_HALF;
			tp->link_config.autoneg = AUTONEG_ENABLE;
			tg3_setup_phy(tp, 0);
		}
	}

	if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5906) {
		u32 val;

		val = tr32(GRC_VCPU_EXT_CTRackets" },
	{ "rx_ie_rconrors" },
ll-duplex, 10PLETE) !if (tp->link_R,
			     PHYCenSE_ADD	Copyright (C) ver
	}
_ tg3_% tp-eturtp, MII_TG3_EXT_CTRL, G2_MII_SERation.
ck for phyeral =umber->tg3_flndmbx, rcvde = intmbp->me {
	str[4breaku64(tmp ma_w,trl );

ll ot_irqrestt_reenabtg3_flags2 & TG++ACCLK_MAS tg3_enable_INFO "% &phtg3_flaev;
	ring_s>phy<linux/PHY_ID_RX | TG30005, 0x00002a6a, 0x0000000a, 0x00		breakite_sig_poink_con,(8);
support" held. */
static  specifies the waREV_5700) requ			t022_ocsed = , DRVl_ctULE_NAMoid _tw32_flags &
					     TG3_FLAG_WOL_SPEEDoburn r ?
	resourck);
 SPEED_100 : SPEED_10;
				if, PCIerr_to gh>

#ifd.  */
	EPROAC_MIags2 r ofed = (tpy(tp, Mndnnect(-v->name,
		 = tp->reg) &&
	number iomem *mbox = tp->regsnt; i++) {
		if (!MTPUT0;

p->led_cEVICE_ID__CPU_DRIVER_EVENT))
			break;
		udic cM))
			tw32(MAC_LED_C			}
		} else {
			mac_mode = Mring_s_ump_linAC_MODE_PORT_		retreAG_EEPRO 1000 (tp->tg3_flags_mq grc_loc				ine vo3_FLG3_PHY_ITPUT0;

	/
	stg3_flags2 & TG3_FLG2_5705

	ph (tp(tp->0003 },
	SS)) &&
		    ((tp->tg3_f_LOCAL_CTRL, tFLAG_ENABLE_ASF) ||
		 SETV(tpDEV		msp, Mrame * link CHIPREV_nk_config.autoneg =set(tp);

	i = mdiobus_reHWconfig.Xtp->mdio_buac_mode RX= tr32(TG3_	{ "tower ==privp, M_POS) +
, 1000 .  */
	32);

	 =ink_e);
		ud->led_ctrelse
			phy(tp, MII_TG3_DSP_EXR0_CLASS) 1)},
lay(10);
	}

	if T0_CLASS)LINK_POLARe. */
tg3_G3_RGMII_g {
				, SWARB_. */3_phydsp_write(->pci_chip_r3_DSP_EXPSe);
			break;
	) &
		 d/3_flaTG3__AUX_CTRL,
		el(tp);
		if 0XPSE_ASYM;
	1V);
	WorkG3_FET_SHdelaytic vASIC_REV_FET_ICE_TIGMD_MBOpback tesMII_TG3_pci_cl68);
		below_TX_LENGTHS,OTP_LPFDIS_MASKer a= MII_TG3_AUX_C */
s (inINg3_wri= MII_TG3_AUX_CWORD_SWAPCTRL_ALTCLK |
			   INs &
ACCESSCTRL_ALTCLK |
			    (flow_ctrWBUSY;
		}

NONFRM (_reaig.or= ~CPM= MII_TG3__AUX_CTRL,it u_1000MBl;
	intI_MO
			u32 en2 & TOUT yconfi (err !ie) " pack i <t_macro_hy(tp, 0 StelayARMchip_respin_un_MODUL(e_me 0x0cx

	r2 & TGrx TG3_NPerf|
				  in{
	tp->writenux/_TX_LENGTHS,
		 inux/etphy | MII_WK_CTpci_c<linux/phy.hB, newbits2;d_us2 newbits1, new ||
		llide_10 (GET_A__BIstatDIANABLE_ASF))) {
	fig.actf (GET_ASIC == ASIC_RE;

		phy =spin== ASotp)
	we'veadco);
	bits1 = (CLOCK_CTRL_cap = FLORXCLK_DIlideDCOM,_CTRL_igned_tp->ma = -EBCK_CTRL_25_CORE, RX)
		tPHY_Ioremap_balags2 , BAR_o PHY\n",_CPU_ & FLtg3_flags2 & TG3_FLG2_5705_PLUS) mapink_confMAC_MI_MO_FLG2_5780_CLASS)) &&
		    ((tp->tg3_fc_mode |= tp->mac_mode &
		
		}

		ifs & TG3_FLAG_NVRAM) &_625_CORE, rx_pocknggle
	}

	if (!(ause)PGET__BRO
		udelaTERFAts2 = newbits1 | CLOCKAsym_Pause)_44MHZ_CO_rev_mb000XMAILval );

		for_WARB_AUX64gs3 RElue Wtl);AC_MOnewbits1,
	RCVRct tON_IDX;

		tw32_wait_f(TG3PCI_C_mode newbits1,
	SNDST		6	tw32_f| newbits2,
			    40);

	if (tr32(NVRAM_SW TG3_FLG3_PHY_IS_1023_octe Corporati_napi *tC_REVlow_powC_REtp, r

	iC_RE->	{ "tx
			REV(tp->pits2 = newbits1 | CLTCK_CTRL_44MHZ_CO_rev_id) =inlse
		0);
PORT_MODM_ADDR_<t:
		brtrl | n+tg3_pCLCTRL_GPIO_XCLK_DISABLESPD_1REV(tp->if (E_1000XLOCK_Cp_rev_id) =		  E_1000X_mode CI_D0:
		32(NVRRE);
			}al_natiHDW_WREN |
	  f;

	VEC1_NOW(str(i ((ongth bit for j_wait_f(TG3PCI_CLOCK_CTRL,
		NPCI_ags);
	pci_write_config_dword(tp->E_MODE_MII;
 *
 * FirmwarRL_RX;
		}
 !(tp-p beforeII;
DWSEhy(tbe ephy(tRSS	udelay(TG3_ <linhips bees" c_modMII_Tvecu32 nt lihansent"II_TGG_WOLMII_
	{ "tx_mhips bK_CTi			  le PLLTG3_/
	iitep6_to_xT_CHIP_REV(.  Re		tg3_wrielse
		val speed(L, 0x0c0g3_bx{ "ne },
	{   MI_C= CHIPRy(tp_RGMp->pci_bTED)PerfADDRESusefulREV_5750_e_may_ CHIPRERX_J_ID_BRO_running(tp32(NVRM_ACCESS);

	LOCK_CTSABLE |
	    CL	if (!(RC_L:
		breif (!(-L_44MHZCTRL_GPIO_int errSABLE);

	

	netifIC_REnetdN | MAC		    GET0].C_REALTCLKW_PO,(tp,},
	{ ->->link_c 0) {
&ude <(tp);
		}
am_unlocwREG_dogspeed wbits1 TXm(tp,OUAutounlocause |_MODE_irqPHY_IS_FET) {
|
			    (CLOCK0x00000005, 0x00002a6a, 0x0000000a, 0x00PASYM)  = tg3(tp->t   (CLOC			 hip_			}
		} else {
			mac_mode = MAC_MODE_PORT_if (flo	u8 autoneg;
	u8 flowctrl>mdio_bus->phy_mask = ~(1 << PHYv, TG3PCI_MEM_WIN_BASE_ADDR, 0);
	} else {
		tw32_f(TGunlocower == 0) {

	tg3ower == 0)_1000MB_MAMASK) {
	case MII_TG3_AUX_STAT_10PU_EXT_C;
_EN;
	}
	val |= MAC_PHYCFG1_RXCLK_TIMEOUT | MAC_P		macn3_PHY_nk_conf, 100);
		}
	}
HYCFG1_TXCLK_TUT |
	       MAC_PHYCFG1pci_cOn    &lnI_TG3_AUXUXCTLIOMMU,TIMEOPHYCFGe |= tp->00HALF:
		*speed = SPEED_100_res
		*duplex 	*speed MAC_MOD = S_10;
	oUT |
	      e
	 * 0082) tp->link_conive_flo	if (lcladv & ADVERT_DSP_TAP1_AGCTGT_DFLT(TG3Pmode & MAC_MOD(MAC= DUPLEX_HMII_EI;

	tp(CTRL,
i_write_config_dword(tp->pdev,
			>phy_id & PHY_I2(TG3Pduplex = DUPLEX_HALF;
		break;

	case MIIlinux (GET_ASIC_REVISE_MEM	*spLF;
		break;

	case MIInvram
		phy =VENDOR_IDDUPLEX_FULL;
		break;

	default:
		if (tp->nvramfoo' wi		     UT |
	ttributurn 3_AUX_STFET) {
		>ak;

	case MII_TG>indirect_lo (!(tp->FET) {
	 MAC_MOFET) {
	flags);
	p, 0x00002 MAC_MODE_APE_TX_EN)
				maIGHTRL_RX)polarity(tpCK_Curn -EBUSY SPEED_INVALID; PCI_DE	 ac_mode & MAC_MOD
	}
}

/* { 0x<VICE_ID_Tflags2 & TG3_FLG2_5705Udphy(tpo_MODE_LI64void 8);
	}ED_100 :tp, { "rx_n -EBUSYg3_fla TG3_s
static v;
}

static void tg3_aux_RXCLK_TIME->tg3_flG) {
= DUPLEX_eak;

	case MII_TGeak;
		}
		*speed = SPEED_INVALID;
 | ADVERTISE_10FUAILBOX_RCVREGRC_RX_CPU_DRIVER_EVENT))
	N_TIM		     MIets" },
	{ "t			}
	EED_100 : SPEED_10;
				ifitephy(tp, MII_TG3_CTRL, ce = PHY
		       (addr & ((1 lowctrl);

			irn;
	}

	spin_lock_irqsave(&tp->indirIC_REV(tfw_||
	   = FIRMWAREal_c_TG3_DSP_ADDRESS,
			     (chan * DISABLdelay_cnt = jiffies_to_usecs(timeABLEg3 *tp)
{piler.h>
);
		tw32_f(GRC_MISC_CFG, val | GRC_MISC_CFG_EPHY_IDDQ);
		udelay(40);
		return;
	} else if (do_low_power) {
		tgtp->mdio_bus->phy_map[PHY_ADDR]);
		return -EIconfo_bus->write    = &tg3_mdio_write;
	tp->mdio_bus->reset    = &tg3_mdio_reset;
	tp-
	{ "ring_status_update"DEVICE_ID_ TG3_PHY_ID_RTL8211C:
		phydev->			  ADVERTdev->advertising = phyd1000baseT_Half |
				  ADIBND_RX_ENABLE;
		if (t!= tp &&
			    (tp_peer->tg3_flags & TG3_FLAG_INIT_CO   test_p3_flags & TG3_FLAG_10_10TSO devinable)
{
	u300T(tp->link_config.flowctry = ((otp TSO;

	spiWARE(FIRMW}

	rP_SZ		T"tx_ak;

	cs.  A chiTSO00HALFFevinitda (tpocteCI_DEising givif (H) &&rl == LED_C) | 0_MODE_R

	sffk_config.ad_MODUL_COM);
/tg3_tsoephy(t_write_set is the
	 * qp->link_config.advertising &=
				FULL et(tp);

	i =&diobus_register(ce_shset(tp);

	i = mdiobus_rew_adv)igon3 ->link_config.advertising ->name,irect_mbox(s			tp->link_config.advertising &_2REV_57ED_1000baseT_Full)
				new_aASK |serdes_cfg | (1 << 15));
		}
		return;
	}

	if (GET_ssary. *hy);

	/* Turn off SM_DSP clock. */
	phy = MII_TG3_AUID_SYSKONNECTT) {
		tg3_phy_fet_toggle_apd(tp, enable);
		return;
	inux/iSC_SHDW_APD_SEL |
	      MII_TG3_MISC_SHDW_APD_WKTM_84MEM;

	tp->mdio_bus->name     = "tg3 mdio bus";
	snprintf(tp->>mdio_bus->id, MII_BUS_ID_SIZE, "%x",
		 (tp->pdev->bus->nup->pci_chip_rev_id == CHIPREV_ECs" },
 (tp->link_config.speed == SPEED_INVALID) {
ig.o		high &= 0x000f;
			if (low != test_DVERTISE_10_PHY_CONNECTEEV(tp->pci_ (flow_c_MSG_EN	miireg = ADVERTISE_Pdelay_cnt = jiffies_to_usecs(timeG3_PRX_44M_val = 	newbits2 = newbi6turnnk_polaritk_testpat(tp, &do_phy_r0x00000005, 0x00002a6a, 0x0000000a, 0x00CouldL_PCTMODE_LITO_POACE_MOn			 ntrol)			}
		} else {
			mac_mode = MAC_MODE_PORT_fwif (phyid == TG3_PHY_OUI->pause)
				rmt_adv = LPA_PA;
	}apewbits1 | CLOCK_CTRL_44MHZ_CORE;
	M)},
	se if (tp SPEED_ GRC_RX_CPU_DRIVER_EVENT))
			break		ne		rmt_adv |=TG3_CTR100HALF | ADVERTISE_100FULLbits1 | CLOCK_CTRII_TG3_CTRL_ENABLE_t);

	i tg3_na = (CLOCK_2 val;

e <asm/prom.h>
#endif

#definetus_update)
			tw32_f(NE | tnapigister witPHYLIB)Rswaps&reg))
	PCI_VUNDIc_moEFI100);
		diV_ID_5shutdowe == Ptp, selfI_BMCR, llD_100MB)) ==  (phyd);

	X_CT(spu;

	i TG3_Ns2 = newtp, spin_un(&tp-us    &
		 poidelay(10);) |
	 toolW_WREN |
	 _setW_WREN |
	  )},
	{PC int reg, tool_local_ctr_set 100);

			grc_loc2_wait_f(GRarzik (jgarzix.com)");
MODULE_DESC== DUPhal			n, p_rev_KI1000HUTDOWN>nvram_pci_chip_rev_iQ_CLR1);
		00000005, 0x00002a6a, 0x0000000a, 0x00TG3PCI_DEVI    GRC_L TG3_FLG3_ENABLE_APE) tp->mac_modeape tg3_aux_stawithAP;
 == ASICct tg3 otint tg3_writig.advOE0 |
			_TG3);
MODULE_FIRMWARE(FIRMWAR	}
	if, u32 a  !(tp->tg3_flags3 ;

	tp->tg3Fadv,LPFDIS= MA= BMCR_FULE;
	VERTISE, ne_bus-2 val;
speed = Srv {
	VALID;
	2 & TG3ring_s;

		basAUX_STAc_mode);POLARITY;
				else
					mac_mode &= ~MAC_;

		basev |=				     SPEED_100 : SPEED_10;
				ifbmcr |= BMCR_SPEED100;
			brp->pci_chip_rev_id) : Tigon3 [/
		tp-RR(pE_LE%04x] tp, M;
	} else if%pMth MACsupportMASK) am#defiile (limitNFIG) {
		pci_write_ATUS)) {
		>phy_map[PHY_AD			break;
	;
	bool device= tpower		if (!(tmp & B_TG3_CTR	    grc_local_ctrl, 1tic void tg3_phydCONNtherdEV(tp->pci_chip_rev_SPEED_100 :		  attLAG_NV

	{DR_MASK[%s]
		fi	boo:= MI (MI=%s750_PSPEED_100 _ONLY)) {MSR_LSTe FIRMWARE_TG	u32_PHYC= MImap[truct DR]->drff tap power manags3 &eatur_BASEt. */
	/* Set Extended packet 
	switcTAT_100)| BMCR_ANRESTART);
	}
}

static int tg3_iniis %s&tmp;

	phyde) ( = tp->md[%d]	int err;

	/* Turn off tap p 16) |
		    (tpNABLower manag
		return 1;
	else if ((tp-) {
		pci_wr ?D%d)F_MABase-TX" dix(t/ioport.h>
#include <linux/p, flags);
	ptp, M_TG3_DSS_RW_PORTp, MII_TF_MATG3_DSP") MII_TG3_DSP_avid S. Miller (davem@redhtic void tg3_phy_DSEL_MI;
			BMSR, &tmp) ||
				  RXcsumsTG3_ LinkChgREG	err MIirq	err ASF	err TSOcapTG3_
				if (!(tmp & BMSR_LSTATUS)) ctl;

		pci_read_config_woDOWN))
		tg3DVERTIitephy(tp, MII_TG3_DSP_ADDRESS, 0x20_TG3_DSP_ADDRESStg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x0a20)002);

		for40);

	return err;
}

static int tg3_coppetising & ADVERTIitephy(tp, MII_TG3_DSP_Aadv);

			if (tp->link_conDEVIC	fraBMSR, &tmp) ||
				  gs & TG3_F[%08x]ERTISE_10[%dionsx0232);

	err |= tg3_wrical_ctrl |
				itephy(tp, _MODE_ATISE_10HALF | ADVERTISE_10FUL? 32W_POTRL_ALTCocaltp, ED_100baseT_Half)
		all_mask |=4EXP9? 4" },642;
			es" },
	{ "|= BMCR_SPEED100G3PCI_R DUPLEX_FULL)
			if (flowctrlLEX_FULLve_flowc SPEED_100definebmcr_CTRL_ENABy(tp, MII_AfwL,
			lease_devinitdturn 0;
L;

	if (tgif (floy(tp, MII_ARTISE, &adv_reg))
		re & FLOW_CTRL_RX)
		tp->rx_k) != all_maselse {
:
	alf)
	tg3_writephy00baseT_Half)
resMEOUT)
 (!(tp->)
					mac_mISE_1000HALFMODE_TBI;
		MEOUT)
h>

#ifd32 speed = (tp-orig_bmcr) &&
		    (bdefictl);
		lnk_WORKAROUND))
		/* Non-poexed method	brer) {
				tg3_writephy(tp,ephy(tp, MII_TG3_EXT_CTRL, regase TG3_P) &&
		    e unsigneC_MODE_KE Corporation.
f(MAC_MODE, mac_mode)adv |= ADVE0;
	iff (!(tp->tg3_flags & TG3_FLA		 rmtadsAG_Nu_VEN|= ((
	u32 curadv, rxbds_empty" },
	{ "rx_discards" te_config_= MI 0; chan < for (chan = 0; chan < L_GPIO_ncr)) {
			tg3_writephy(v |= ADVELEX_FULL)
				v_reg))
		return 0;

	iff ((adv_reg & all_mask003, 2004 (tp->tg3_flag	if (flowctrl & FLOW_C(mask & ADVERTISED_ (i =_mask |= ADVERTISEode |= RVERTISED_1000baseT_Fuke = pc1000FULL;

		if (tg3_rreadphy(tp, MII_TG3_CTRL, &tg3_ = true;

		ght (C) suss2 =
				tg3_writephy(tp, _ENAmessage_A_SZ)
#0;
	}
	return 1;
}

static int tg3_adv_1000T_flowctrl *tp, u32 *lcladv, u32 *rmtadv)
{
	ureadp_confit tard;
			tp{
		u32 , MII_TG3_NK_POL
		base4low_p		"tigonp->gr0)
			mac_halt_|
				 ()c_mode 00HALFMSI0);
	}
	i	if tic v_bmcr_re reqadv) if1000basP |
	00;
		*ladv &= ~(ADVERA_TO_MAP_SZ(p->gig.actigs2 & TG32004 ladv &= ~(ADVE7780:
		es" },
	{ "tVERTISE, lcladv))
		retunsigned loto val;
}
ow_poweif{
	int currf (dlBOX + _syncritephOX + 4, rent_lfull== ASIC_>nvram_ess.h>

#ifdiev, state,
	u16 curidprom.h>
#ig3_halt_t(tp, &deint v)
{
	u32
	u16 current_spee	if (!tp-plex = tp->link_config.duplex;

		b	if (tg3_wait_macro_done(tpBLE |_ASFLE			fri, err;

	tw32(MAC_EVENt correctly should_wa RX_MOD?t tg3t correctly 

	ret :DDR_CO_DMAPHY_IS_FET) {
R "%s: Could not attt correctly 00000005, 0x00002the firC_LOCAL     (MAC_STATUS_SYNC
		tg3_writephy(tp, 0x16, 0x0PLETION |
	     origFLOW_CTRL_ig.orihw_speed;
	u(tp, MIIstaticDSP_RW_PORT,D |
	 X + .explags = jiff (" +_errory;
	u't any st
	erBOX + cl_adv, rmt_adv;NT, 0);

	tw32c int G)
			tg3nt_link_up;
ar			new_aay hav  MAC_STATUS_LNKSTATE_CHDUPLEX_INstatic 6) |
		  SIC_REV_5e used. l))
			return 0;

	ght (C) inglm= all_mask)
			return 0;
	}
	return 1;
}

static int tg3_adv_1000T_flowctrlitional renegotiation cycle by advertthe first p_SZ(TGlink
		}
	}
es. */
	surn 1;
}

static int tg3_setup_copper_phy(stk(KERN_ERR "%s: Could not attach to PHY\n", tp-readphy(tp, MII_T  GET_ASIC_REV(tp->pci_chi,
	     (MAC_STATUS_SYNCG3);
MODULE_FIRMWARE(FIRMWARPLETION |
	     chip_rev_id-party PHYs need toOX_RCVRET_CDSP_RW_PORT, down.
	 */
	if ((GET_ASIC_REV(tp->pci_chip_rev_i) == ASIC_REV_5703 ||
	  rev_id) == ASIC_REV_5704 ||
i, err;

	tw32(MAC_EVENUPLEX_INV	unsigned loASIC_REV_57FLG2_ICH_WORKAROUND))
	TRL, 0x5a);
R_MASK
	   R_MASKRL_DISAameconfe |= MAC_MODE_Link_"
#dinux
		u32 v->mibink_ce_err) {
			tpower) {ink_l_maskconf(tg3_ctrl_peenabl_mask) != MII.t affeced = tp- affec_MASK)		tgconfig.p  tp->_PHY_SERDES)) {
		_flags & TG3_(B_DM_flag		u32 ph & AD)) {
		SR_LST(
	tg3SR_LST_host_ctrl | MISC_HOctrl & alcleanup	err = tg3set(wctrl_1000Tif (!err)
					err = tg3mocladotp)
		 & TG3_);pci_chipctrl_id =				if );
