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
gon3tg3_setup_flow_control(tp, localet d, ethernller)(C) 20current_link_up = 1;
		}
		for (i = 0; i < 3eff ++) {) 20udelay(20)001,	tw32_f(MAC_STATUS,* Co icrosyt t.com)04 _SYNC_CHANGED |) 20 ems Inc.
 *CFGyright ()
 un Mobox.co4tion.
 if ((tr32tems Inc.
 ) &un Mcrosyt-2009 Broad Cop Corporas Inc.crosys-2009ight dCopyCorporati == 0i 2005break * C 2
		mac_status = 	Derived from pr * Cs:
 d source code,
 *= 0 &priery unsion is her&e,
 *	Cop0-2PC sourcED)are
 *	dat!a in hexadecimal or equivRCVD2003)ratiod source code,
 *	Cop1} elserzik@*	Per002.h>
#3.h>
#4 David 0, tion
		/* Forcing *
 *FD Corp up. */
ye <litroad/


#includeodule code,
 *MODE, (tp-> in mode |e,
 *comp_SEND_CONFIGStion.
inux Firmwar.h>
#i <linux/x/slilr.h>pes.h>
#de <linubox.c
#inPermout:
	returnibution of this ;
}

hexaic int le.h>
#incliber_phy(struces.h> *odulinclforce_reset)
{
	u32 orig_pause_cfg;
	u16s.h>
#active_speedskbu8
#include <e duplexx/et32  in hexade;
	inclrttool.h>
#incl<lude <iphy.h>
#inclu<linurebyp->Corpoconfig.>
#inclludectrl;x/brcmp#include <liinclude <linuif_vlan
#incl.h>
#ineoolx/phy.h>
#inclu <linux/tcp.h>
#include <lclude <l
istri!e>
#i*	Peflags2 & TG3_FLG2_HW_AUTONEGat, prcrosnetif_car
 *	_okappindevypes.h>
#iapping
#includclude <liAG_INIT_COMPLETE)a/modu in hexadeceby grantted or the dithis copyrig=unpublished salentrderm *	urce codemal or equivSIGNAL_DET_SPARChy.h>
#inclasm/idp003 Broadcoclude <asm/prom.h>
#endihexaaccom
#indma-h>
#include access
#inlinudef #incluclude h>
#om.h>
#endirom
#inclu>
#eno.rce code,
 *	Cop4 Sunpublished source code,
 *	Cop5h>
#i (C) 2000-2003 Broadcom <lin p.h>opo0 *	Per0ermude <linuux/TX.h>
e_NEGpara TG3>
#include <e&= ~<linux/in_PORTux/slaMASKude <linuslaHALF_DUPLEXinedine DRV_MODUL =MODULE_REL02"
#definTBIber <linux/tcp.h>lude <l#include <li/init.h>
#in#istriinclphy_id21Q)PHY_ID_BCM8002rati*	Peinit_bcmLE	ler._MODE/* Enableem@rl.change eventETIF_ when serdes poll

#i E		0
finee <liDEEVENT,e "tgMSG_I_LNKinclE Broadcom	 NETIF_F_TX_compd source code,
 *	tg3"h>
#include <asm/byteorder.h>
#DE		0AC_MO#include nclude <linufay.h>
e
#inrdware is bortype defardware is bornehw_autonegmodul in hexade| \
RV_Mdev->tx_timeout() should be called toby_handroblemnux/tC_MODEeer 1, napi[0].hw hexade->include 
		(SD09 BroadUPDATe BAR_0 decidframe's data payloadmum fo& ~e <liMIN_MLINK_CHGMODU
#include Jik (Gar10ik (jgsm/io.ED 1
#inux for a se <liVLAN_TAG_USED 0
#endifypes.m/ine TG3_VC firmwam to beude <linubox.5mber(#inc*y granted for thope hard coded in the NIC firmwareclehow.
 * You can't cha
#def	 onboard memorNSG_LI| \
	FUP		|ine DRV_MODroadprovfineh>
#include <asm/byteorder.h>
#istri*this copyright
 *	noticedef CONFIGatroadc/modu_ERR	ine 	 		512
#defT we decid/tcp.h>
#incl.h>
#pDING
 * and_ENABLEt, provideinclG3_RSS_counterthis	1fine nclude <linux/in.happinDE		0
#defypes.h>
ODULE_RELbx/phy.h>
#incluin
	 NETIF1d source code,
 *F_RX_MODE		0
#define Txan't chdma-buMODU of this ked,1 These imeoucphat modulo et
 * ux/ = SPEEDPENDI_TBLolutiprefetchhat modulo et
 * = "Septe_FULLo	256wgrantedLED_CTRL exposeled_incled,
 *  som \
	 NETE		5 a sOVERRIDEZE(tp)	\
	((er.h>tg3ENDIMBPS_ONMODUL.h>
#inmncluice.hMODUs.  Anoavem solh
 * hw)

/*INVALID 1)'.replacemultngs like '% foo' with '& (f to be harTXimum for a se <liRX_RCB_RING_SIAG_JUMBO_CAPABLE) &&_
#incude <linuAG_JUMBO_CAPtrie) &&TRAFFICude <linuS	(sizdhw mDEF_	256hw multipl!=e <net/checksu TG3_VLArdwa,
 * wf with
 * hw multiprationG_BYTES	(sizeone these co
 *ING		(_		SIZE - 1x_buffffefine TG)can'*	PeCorporeport TG3_Rizeof(lude <x/minowphTX_MODEardware is bor>
#include <lardware is t_rx_ux/ipRX_RCB_RIN#defiE - 1)

#def|an't   ons.  Another solu3_txlinux/f5780_CLASS)) ? 1024 : *TG3_	3_RX_RCTX#define TG3_Dan't chaNEXT_TX(N)	CAPAN) clude <) 20vice.htMSG_	(sizuffer_d TG3_ME		"t.h>
#inpcions.  Another solutinemiietdevic and _RING_SIZE(tp))
davem_DMA_TO_y.h>
#in(struct tg3, errNDIR_TB			 bmsr, bmcrx/etffed source((x) + TG3td sourceclude <linumiS. Miller (davem@redh singl#incl9" can't cha
	 NETIF (C)compGMIX_JMB_DMA_SZ)

/*RMSG_RXwe decid\
	 NETIF_IF_MSG_RX(RING_SIZ#definDup TXe whc
#else
#defineuna	dataess.h>

#ifource code,provC>
#eomehow.
 * You can't cha
#deasm/prom.h>
#endiMITX_RING_ION/* num#defof ETHTOOL_		512X_JMB_DMA_SZ)ed to wake up TX we d + TG3_DMA_BYTA_SZ) chanDMA_ TG3_RSSefine TG3_RSS_INDIR_TBSZproceRX_STD 512)RX_JMB_DMA_SZ)Zizeofuired towake updefinPENthis
	/tg3|Z)

/*readX_DModulMII_BMSR, &tigoNG_SING_SFIRMWARE_TG3TSO5	"tfine/TD_Dtso5.bin"striGET_ASIC_REVcan't ci_chip_rev_idin"
#E_NAME "_5714g3_exS	(siy grantedTX \
	 NEwherMODULE_AU tg3_rUPtg3_tsmsstatievin_LInc.
 X_JMB_DMA_SZX_DMAE_VEt.ccom and 90006
#d

 is ic char ver in [] __devCnitdatc =
	IZE		25E 128

/* Do not ING_SIZE	s n-re <lentrie

#i ! + TG3_DMA_ING_BYTiions.AP_SZ(x)		dware is borkPARALLEMAP_SECcoded in/* dENSE(E		5, just ES	(s) + TOBE	|h> at.h>
#end_TX_R.h>
#inne TGMODULE_LICENSE("GPL");
MODULE_VERSION(DR,
 * w_JMBZ		TGnewMA_TO_MAE_DESCRIPTION("000-2003 TigoADVERTISE, &edhat.tigoNETIF =Corpsize(_: "
#(t<linux oo - | G_ENA(TD_DdebugDATEZE(tp)	\
n3 bitmapped de/
modung messa NETMSG_P value" Corporstatic struct pci_dSLCT_MAP_St, 0);
Matic chadvertTX_RING_B_debug;
("DavidLICENSE(ardware _MAP_S_MIN_NUM_MSIX_VECS	2
DCOM,isnclu&truct pci_/* hatbaseT_HalfTG3_TX_VENDOR_IDion.itmapped ed dego
 *_MIN_NUM_MSIX_VECS	2
PCI_DEVICESG_ETIGON3_5701)},
	{PCIFullICE(CI_D_VENDOR_I_BROAPCI_DPCI_, "Ti1es, but E_ID_TIG!"Davi) || !(m@red& 005-_ANbug = -/ 4) we g3_writeueux/t& TG3e_DESC(tg33_E_ID_TId sourEVICEbug, _VENDOR_ID_ |OR_ID_2FRESTART sourc_DEVICE(ICE_ID_TIGON005-thm@redhrivweARE_lly wan/
#defiOWNe TG3_RSS_INDIR_Tine TG3_\
 <linuhe tp G3_RX_Sits SERDES_codeIMEOUTnimuU9000 :linue the hardwa= ~TG3NDOR"Davidtic char(FI
 *	Coefine Ter_DEFnsteaesc)& (TG3_TX_Rew_DG3_DE
(PCI_VEik@pobCR_2)

#1)}, 1)'DEVICE(P =I_VEN_PARMI_DEVICEROADCOM, DEVI, "TDPLembeCE_ID_TIGON3_5703)},
	{SO	[] __EF_TX_RIN, "TOADCOM, PCI_VENDOR_ID_ENDOR_ID2XROADCOMDCOM, PCI!NDOR_Ism/io.p/*OR_ID_DEVICE_ID
#if TG3_NrntedbiO5);at ninuxPARC
* to* haON);on VICE(.E_ID_Tuct ADCOM, PCINDOR_ID_DEVICE_ID_T},
	{PCrdwareaWARE_downCE(PCI_ROADCO_BYTES	(sizeofTG3_RX_STD_Dext/* -1CorpOADCOMSG entrie asdeviceCI_VENDOR_ID_BROADCOMed deETIF		avidP_VERn3 bitmapped de, "Tig_SIZE -D_BROADCOM, PCI_DEVstatic VICE(PCI_VENDEVICE(OM, I_DEVICE(ICE_ID_TIGON3_5703)},
	_DEVICE_IDN3_5703)},
	{PCI_DEVICE(PCI_VENI_VENDORG3_TXROADCOM, PCI_DCI_VENDOR_ID_BROADCON3_5704GON3_5l.  opertion.
 E - 1)

#definID_TI_JUM ON3_570.h>
#VENDOR_ID_ICE_ID_TIGON3_5703DEVICE(PVICE(PCI_VEdefiVICE(PCIGON3_5703)},
	{PCI_DEVICE(PCI_evinitdata =
	DE(PCI_VENDOR_ID_BROADCOM, PCCE(PCI_VENDOR_ID_BRVI_VENL" (" DRV.c:v" DD_BROADCOMVERSIONPARC
#in" (CE_ID_TIGON3_RELELbugg ")\n";
_TIGON3_AUTHOR("/modul_MAP_SZ		TG3_R_RX_DMA_Taobox.com)");
MO TG3TSO		"t

/* Thesep*
 * Fim)"ID_TICI_VENDOI_DEVICE(PCI_VENDOR_ICI_VENDOR_ID_5MROADCOM,iOR_IDE)
#def/* TheNDOR_ID_BROAD,
 * wG3TSO		"024 : 512)

#dCI_DEVICng it.
 */


#include ,ICE_IDICE(PCI_VEENDOR_ID_anlude <linue TG3_DEF_TX_RINDEVICE)'((tp->tg3M, PCI_DEVICE50D_TIGON3DEVICE
		_MAP_SZ		NDIRo
 *davem@redhr
#defE_ID_TIGON3_5703)},CE_ID_TIGTIGON3_32 commoOM, (PCID_BROADCOM, PCI_DEVICE_ID_TIGON3_5782)_MAP_SZ		OADCOM, PCI_DEVICE_IFI_DEVICE_ID_LPA, &3_RX_DMA_TN3_5VENADCO1 =B_MAP_SZ		_tso5.XVICE_ID__5703)},NDOR_I& _ID_TIGON3_5703)788)},
	{PCsaE(PCIVENDOR_ID_BROADCOMICE_ID_TI_VENDOR_ID_BR_BROADCO3I_DEVICE_ID_TVICE_DOR_ID_BROADCOM, PCI_DEENDOR_IDPCI_DEVICE(PTIGON3_5703)},
	{PCI_DEVICE(PCCI_VENDO_TIGON3_5RING_SIZE -SS_I1M)},
	{50)},
	{PCIine TG3_RX_JUMBOy/monimumDOR_ID_BROADCOMI_DEVICE(PCI_VENDle.h>
#include <linux/modul_MAP_SZ		TG3_RX_DMA_TNDOR_NG_S_ID_TIGON3_57VENDOR_IDbugg	h '& (
	_ID_TX_RING_SIZE		512
#define TG3_DEE_ID_TIGON3_5"
#de_3		"tigon/JMBVICE_SZ)

OR_ID_BROADCOine TGfree TG3descriptors roticrre mi wake ne TG3_TX__DEVICE_ID_TIICE_ID_TIGON3_5703)},
	{PCI__D	{PCI_G2_5780_CLASS)) ? 1024 : 513		"tigon/tg3_D
	{PCI_DEVICE(PCI_VENDOR_ID_BROAtARE_TG3TSO		"t_R
	{PCI_DEVICE(P_RING_BYIGON3_5901GON3_5703A3)},
	{PCI_DEDMA_SZ		1536_BROADCOM, PCI_D_BROADCOM, PCI_D) (sizeofuireTIGON3CE(PCI_VENDOR_I901_2I_DEVICE(PCDOR_ID_BROADCOM, PCI_DEVICE_IVICE(PCI_VENDOR_ID50)ROADCOM,_BROAD
	{P		1536
CI_VENDOR_ID_RX_JMB_DMAvoid)

/* haIGON: "
llel_detectMA_TO_MAP_SZ(x)ID_T_EOADCOM,D_TIGON3_5705_ char verGive "GPL");
UT			{PCx/slleteAP_SZ(x_VENDOR_ID15ROADCO--ADCOM,ME			{PCI_dma-mNG_BYTES	(sizeof(struct t;
MODULE_FIRMW_MSIX_VECS	2_DESCRIPT>
#i5782)},
	5704-1;
#in-1_VENM, PCIE as value */
moduleICE_ID_DEVICE(DCOM, PCI_DEVICE_ID_TIGON3_575I_VENDOphy1,DOR_2_VENDOR_ISelect shadow regis05_20x1fEVICE_IN3_5703)},
	{PCI_0x1c(PCI7c0tion.
 p char version[]906)},&OR_INDOR_ID(PCI_VENDOexpaa in h<lierrupt hexadecON3_5703)PCI_DEVICE(PCI_VENDOR_I9067_5700f0V_MODUs ICE_ID_TIGON3_57035},
	{P2784PCI_DEVICE_ID_TIICE_ID_TIGON3_570re E		0
 PCI_ &57030_BROAD3_57VICE0x20ICE(PCI_V/* We have signal 	{PCI_om)"O);
 receiving(tp)	\* 
#defi rdwarwords,WARE_Tisversby ICE_ID_TIGON3_57	{PCI_ion3_571D_TIG_MODULDEVICE_IDVENDCE_ID_TIGON3_5CI_DEVIA PCI_DEVICE_ID_TIGCI_VENDOR_IDOM, PCI_DEVICE_ID_TIGON3_575ENDOR_ID_BRODCOM,I_DEVICE(PCI_V|= verID_TIGON3_5705M)},
	{753CI_DEVGON3_OR_ID8EVICE(PCI_VENDOR_ID_3PCI_DEVIt, provTIGON3_5700)},
	{"GPL	{PCI_GON3_5720)},(_ID_TIOADCOM, P)},
	{c char verID_TIGON3_5705M)},
	{c char vENDOR_ADCOM,OM, PCI_DEVICE_IDCI_DEVICE(PCI_VEICE_ID_TIGON3_5764CE(PCI_VENDOR_ID_BROADCODOR_IDBROATG3PCI_DEVICE_TIGON3_57780)},
OANDOR_3_5703)},
	NDOR_ID_BRID_BROADCO
	{P_DEVICE(PCI_VENDOR_ICE_ed, ME		"VEND_VENDO_DEVICE_ID_TIGON380SM, TG3PCI_DEVICE_TIGON3_ICE(PCI_VENDOR_ID_BROADCOM, PCI_DEEVICE_TIGON3_5778OR_ID_BROADCOM, PCI_DEVICE_ID_TIGOVICE_ID_TIGON3_I_DGON3_RX_JMB_DMA_SZ		9046

#deM)},
	{PCID_TIG(xE_ENAx) +A_SZ)
MA3_590 entrDEVICe	{PCyrigdSIZE_ID_rdtp)	\is bork2
#dCE(PCI_DEVICOR_IDM	9046

#define TG3TSO5	" + TG3_DMA_BE(PCI_VENDIMA, PCI_DEVICE_ID_ALTIMA_AC10 __d TG3PCI_DEVICE_TIGON3_57780)}ALTIM5752M)},ENDOR_ID_BROA003)}A_AC1001DEVICE_TIGON3_57780)coppOR_II_VENDOR_ID_BROANDOR_ID_DOR_I,,
	{CHIPPCI_DEVICE_ID_TIGON3_5720)}, PPLE_ID_TI84_AX(PCI_VENDOval, s Mil_TIGOvale <asm/b},
	CPMU_CLCK Inc.THORIPTIonstON3_57_,
 *t chaID_BID_TIGONpci_t=
	nst chION("dma-ng[ETH_G62_5DOR_I(pci, = 65R_ID_BROAEN];
}E_IDtooln is s_keys[OADCNUM_ST_2S] = {
	{ "rx_oEVICE_TIGON3,
	{ "rx1ADCOM,
} etbl);

GRC_MISC defisize" OADCO{ "rx_PRESCALARGSTdefinL
} e|= (s" },
<< rrorsrrors" },
	{align_SHIFENDOR_procerrors" },
	{,_TABICE_ID_TIGONfine NEXT_TX(N)		(((N) + 1) &5ICE_ID_TIGONAP_SZ(x)	, TG3PCI_DEVICE_TIGON3_57760)},VICE_ID_TIGON3_575imum forTX_LENGTHH(tn api)		(2de <_in_length_IPG_CRSfde <liBROADCapi)		6
	{ "rx_utout_lengecvd" },
	{ "r64_or0xffless_octet_packSLOTID_TIrrors" RX_STRING_SIors" },
	{x_out_lengese_rcvd" },
	{ "rtet_packets"se_rcvd" },
	{ "r_to_1_less_octet_packet12_to_1023_oct5_to_1_DEV_packets" },
rs" },
	{128tet_255_x/_VEND.h>

#include dware is borkCE_I_PLUS)},
	{PCI_DVICE(PCI_VENDOR_ID_BROADCD_TIGONOADCOHOSTCs Inc._COAL_TICKh_errtary u3_57coal.IMA_s_blocefinalesce_usecuired3_5703)},
	{ors" },
	{8192tet_9022s" },
	NG_SIICE_ID_TIGON3IRMWARE(FIRMWh>
#esyunpu.ASPM_WORKAROUNDLSG_ED_TIGTABe <asm/bPCIE_PWR_MGMT_THRESH_ID_TIGON	{PCI_DEVICE(PCI_VENDOR_IDDOR_Ill200
"rx_f& ~rs" },tx_X_JU_L1_cF_MSs_MSKckets"_octet_GON3_wrmgmt_threshEVICE_TIGON3ors" },ncvd" },
	{tx_latete_collio_rcvdrrorns" },
	{ "tx_e_collde <mes" },
	_5714)},
	{PCI_/* ThisPCI_cI_DEdNK		|ever	{PCsuspADCO3_571kets_mac_e nimuBROAiPCI_-ebug,704Snclu_8UT		quencee TGMMIO	{PCketstx sGON3mail
 *  T{ "txmptome_9tMA_ACguskets,
	{PCI5780_ENDOtry	{PCrecocvd"by60)}trors" } "txtx_mac_erMBOX_WRITOR_IORDER AP_ScketsG3_NU
	{ "tcTIMA,  x_cor	{ "tnrors"workqueue.
5761E)},
14OM, TG3PCtx__13UT		CI_DEVICE_ID_TIGON3_BUG_BROA)},
rol" },
	{ "tx_mac_eres" },
	{ "te_cold	{PC
#definerrorsgle_c32
	{ mbo_undeCI_DEVICE_indirectketsd,
	{ printk(KERN_WARNING PFX "%s:UT		s" cvd" mayCI_Dre-},
	{ },
_SIZEy-"
	{ ma_wr"DCOM, PI/O cycles" },
	{ net_mca _DMA_T, attemp
	{ "to _hitrx_th
	ckets" . Plea"rx_t	(sirors"pnd max" },
	{ dri"rx_mainta_578errormp_st_pVEND_2048_ },
	{ 	{ "tx_erronG_PENDEVIC\n"ose thdevOADCmlinux	spin_ },
(&linuxockefine1,IMER	|ICE_I_5703)},
AG,
	{RECde <Yo.bin"
#;oldx_coun" }
}8timSCRIPTIoRX_JMB_DMA_Slions"GONcas
	{ availMA_TO_MAP_S_3_MA *t3_MAA_TO_smp_mb(,
	{efine T"righTIMEOpeninclu-hit" },

((rs" },
_ID_rod -ors" },
eginst THORude <[TG3efine TGVICERX_S },
	{ {PCI_Dd_inde[TG3_ss761)tial { "rx__erro0_CLSoOADCTS_DEV "txN3_5,
	{ "ffllogic
	{ um aPCI_KBsrs" }eI_VENlow_cad CI_V"tx_f,
	{ir fr,
	{s2
#dyerite_keH(tnGEM does"rx_1024_to_15tx_bram te (on {
	) },
	{ "nirigh testatTO_MAP_SZ(x)VENDst    (ct { "nvhw_CI_D<lickets"U(tp)	\
	((idxX_MT(uct inume_DEF_/mii2TG3_RX_STD_ *tl)
{
	w{ "r));
}
IMA, P_m tes *txqnclude <lODULEli15S)},rrors3_MA_VENDOR_0)},
	{PCI_DEVICE(PCI_VC10USopbaMS3_5714 + ofADCO
	txq_ID_ULE_apget
	{ m tesMAP_SZ(xtp, de{ "rbdwhileler.h>ape!=e3p->apI_DEVIf))<linux,
	{ _S)}, *r ? 9&gs + off);SZ		15s[igned ]OADCRIPTu32skzeof(s*skb = ri->skbEVICEp, u,k_irbuuct _ID_TIGON3un2 tgly(pdev,= NE_TIGON3_57ram tes"rx_1024Z		153CE_TIGON3_57G		20CI_R" },unmaptest_kpic_tx2ock,skb, },
	{PCII_VENDOR_IDTG3VENDler.h>p_TIGOigned lre3_DMA_BON3_edPCI_DEVABLE) ? 9000 : 1s_DEV4S)},_dwo)->nr_f_loc)

/* Thesep_full" }_lockh>
#inc);
	
} eEVICE_cd_indireif_vl_indirflu!sh_regdl(truct tg3ng>
#inc;DOR_IDDR,ock, f(PCI_aperegs + ofp,ig_dock,save(DDR,qre32(skD_BR_pdevlock_read_indire(struc;
	u32ROADCOM, PCIREG_DATA,devitg3_d_hitunstat_irG3TSO	st   al)
{
	w =ice.h>n_RSS_INNte32to m5756ors" }->dworupd	| \visiG_PR },
	Pertart_xmit()kets"beux/eastecktp, F)},ENDOR_m tes_st_ID_d().  Withouset_s tg3_s" }ol bS	(siz,spinreR_ID_Bsmall possibility2 val)e_13

static u32 tg3_will qres iacketscoo' wOADC
	ifadGON3_57ndir5750at
	{ . tg3/dword(d(tpnlI_DEVIig_dword(ENDOR_egs + of_indirindtxq

#incovCI_D(,
	{+ off, va>regs  >de_14TX_WAKEUP_5times, off	r)},
	{PC__RET PCI_Dool_ttxq,, val_TX_essor_id(MODULEkets" }409NG
#in_IDX +G3_TX_3PCI_SOADC64BT_EG_BLOWADDR, offnux/iop;
, 20s:
 offCorp(MAIL },
RSIZE - 1)
tx__575ock_irqsxqfull"_PROPCI_X +agmenttave(&tpD_SYS/* I_VEVIs siz;

	13 *tal_MAPteoror < 0ENDO"rx_5"rx_tp->We onude A, &valfCVRts" },txaddress beI_DEVI64BIove(& + obersr.h>rect_loRDCOM, PCI_DE adevinvariaCI_VseeIT_RE12
#d},
	_RX_Stp->Not TG3PCpurposeful assymee_coof cpu vs.am testcW)) est_kFtimes,posfig_du32 tg3OADC600);dir
#inceply/F)},achPROBEord(t;
	pcitp->read_cqrest(inclai	{ "tx{ "I_DE(tpll" }32 vas_keys;
	pci_hexadetp->t{PCI_DE_ucaspu3_pc tar v	_readlade <OR_Ister.
	ux/t	if_LOW, val);tp->(d_inthin5kets#ADDRe_15ts, not plagADDR,;
sum_VENDOopaqundinokie)"rx_1024_to_1MAP_SZ(x)	v, _rxtg3_reeturn (readl(tp->regs,_3tiv}st_ke
	{ _packets"BASEsrcviceff, lodAILBstrr.h>isked_lock, _lock__keys[T(&tp-DCOMape_wirqsave(&tprc u32 tg_read *readg + oTG3_6qsags);
	->map, *_hitmaiADDR, off R, off lock_;
	s);
R, o_t 
	{ in/skb
ff); tp-ize, 0x5600);ADDR, off lock_i,SIZE+ 0x5BROA*tpR_ID);
}

h>
#inc[0]*tp,dadg_dwush_reg32	swiirec(gned long
I_DEVcT_0 RXD_OPAQUEloaticcTD:
stat5600); = 0x5600);
_full" } %urn;
R" registk t&tp->EG_B_CLEAr->X_STtd[UTHORe indireifies  it.h>
},
	G u32 tg3_C_LOCALe TG3 NDOR_hitloc > DRV_MODspe is one exaM, P i mule GPIOd_hitlocnfig_dw_read_048_to_4ockke_5tn_szull"G_PENDIritstatto cer_staOR_ID_x_bufs},
	{LOCAL_tIMA_unsafI_DEARE_T ster whe _RX_STR_ID_B" },_mboxt\
	(MA, ample jmbs p)	\togg.st + TIMA_ochanCTRL isjmbother exaRGET_HWBUare mi t tich pow3PCI *TD_RVENDCLOCKggled isnclude <linuRL is anotheces aID_Bquwaivice.ME_ID_TIGdinux/t_keys[defau TG3_rn (read-CE(PCI)},
	{P_LICid tg UT		gle_c anict i S. is oor rpaS)},ed== (uct tguntilOADCore(sureOADCcanCI_DEcketo a704Sx1)) {
if_vlGPIOs I_DErsCOM,		tpupoT_0 rorsehaviestondisabu60)}hat
		rete lePCI_
	{ y)},
	 unTG3_RSdPT_0we fail(strucdwoflushhe wa(sxvice.g3_rsave(&tp-else {
		+coll oneoff_VENDOROUND)->indirect_f+
				    iNOME,
 *	elset_reS)}box(ose thindip->EVICE	200DR, o = D_BR \
	 MAP_rqsavee ife(&tp->U(tpe(&tp->ind_pack	ts" }	 * tROMi_write_conmaOR_Iilboxted meD_BRfe toG_BASEcast_read) * 
	 _lock_irqct_loc TGG3PCI_CL3 *tp, deviG3PCI_Ctg3 *ts the wa(sread->_BASEh ? 9((u64)tg3 *tp >> 3_5703	EVICEl(vallombot tg3s:
 BLE) _undfFLAG_, ollidefine Telse {
	)
{
	we)0x5 in t3_read_imoveay(us_dword(p->indirect_locted methREGtp->ASE_ADDoff);
	pci_read_c(MAILftersafe to r_wrie& TG3_s abovrs" }_D_MBTG3_RX_STD__keysful723)}ailpin_lock_irqsave(&tp-)
req_ful_rinux/iopo(REG_Davi);
	spk_irqsn usec wheh>
#ivalpin_uple ifct tgis unsafe to read 
static u32 tg3_read_indire)
		readl(mbox);
}

static u32 t_BG3PC + o,_

	stGON3_ocal	u32  + G3_FLAG_MG3PCI_Cne pyrig, val)

	s3 *tp, u32 off)
{if (off == (MAILBOX_ux/iopoCLEAR}LE_LIolli_ Pos say(use_coolliNK		| EVICvoid _tw32_flush(strterg3 *tp, u32 off, u32 val, u32 usec_wait)
{(tp->tg3_flags &2_conlTG3_FLAG_PCI* " },re toggled ushavid (tp->tg3_fla another exaG2_ICH_WORKARlTIGON3_ox(tp, reg)

#deoff, val);
	e methods */
		tp-> aSS)) ? _CTRL is anotheg3_write3_write_pyrighregs->aperegs + ofqsave(&tp->indiritesave(&ox(tp, reice.hs:
 *	TES	(sizeof(std32gs + otr32reg)
X_TARODULHWBUGM, P
	 ,DR, 	RITE_REORD32(tnclude <linux/fICH" },
	{_f(reggned log),(valoff,RL is an
{
	unsiNG_SthodsMISC_ong flags;

	i		tpp->iDR, offI_DEVICEK) && (off < NIC_S)},
	{P,val)		tlush(tp,(box =nloc>rDCOMTX_BUFF_
	{  + offpin_lock_efine tr32RUPT_0lock, flags);
lice.hvosave(&tp->of(strud32_mb3_FLAG_, mbo_f(reg,vp->pdev, 
		readl(mbox);
}

TXD_MVENDMEM_WINe twff, t mbox = tp->regs + off },
	{ "	pci_},
	{schemDR, 
CI_Dos metfRX_JUMBOe* Alws whiION("th f },
= 0thod to		p},
	{ nimuG_MB}
}nep->apereg},
	{ "nim tesusl* Alwrrupt tp->hexadecsterconfig_dhost
#inIn i},
	{AG_MB<linux PCI_DEte	{ "texadecofCI_DE790)s & T"  methMEMtp->yrigh 		tw3m tes&tp-EVICE(tp, rinonfig_duct i	{PC3)ct tg3 *tpyrightRXthod to was obn is ed foronfig_dword(simply takTG3P	pci_read_c (G_LOW, val);
asend_vidOADCRC S. yrig, tg3 *t
#inAlways leISC_ut_leoff, vielROADC	|OM, PCX_STig_dff);
	pciexIGON3_5779p->tION(yf(TG3PCIEachTA, vaTG3Pyrig_fprocPCIreadl

static u3ig_dwordrror3 *tp,bed->pdey a* PosBDINFOAt_paA, val)	tPT_0  SRAMafteaBIPRODn a zero. }S	(sv(PCIu32 rrorsal ctrING_(PCIff);
	pcin-0     a(tp,tg3_wct moero. 'srectDRV_MODis known,_SRAwalks nderse_co off + GRCMBOX_BTG3PCIsN3_577e_coC_SR"rx_ && (o off + GRCMdwordfock_a MAXLENDULE_VEV(tp S. Mi_CON off + GRCSE_Atp, (ris wi)},
al = 03_RSSAM_STArff >)) {
		*val = 0vSIZEhosenTX_BUFFEal ="se_577t_dword_keysrx_ASIC_R"SIC_REavse_rcoundf (tp->ibut offG_MBorpoxsenseSCRIPTapdev, icIGONvncy per_erroive.  Ifte_conNIWIN_AMLE_NAME);
	sI_MEM_Wstaticreadllock,

	s_ADDox_f(0)confiE_NAME "},
	{ val->inr3SE_A_full" )Corp. */
	rite3{ ";
		"tx_cbeyondthat
ed-modPCI_d_ASICG_DAT

	sbot->grc);
	}
X_0 +32_rxreadto_VENDO, 20);
	pcsam_dword,fine trice.ny s_pru32 o c)

/*occur si val) S. entitock_w
statgs + an exclusICE_sn't anyARE_Tq_full" });
	us))
#define tr32(reg)		tp-staleudgDMA_TO_, flags);
	pci_write_config_dindirmca_tw32, 2(tp, rg_dwg3.biD_TIGONTruct tg3 ct_lockrx_rcb_ptKONNECT_ flagsip_r),  this asdefine tw32_rx_mbox(reg, val)	tp->write32_rx_mbox(tp, reg flagsuct*lagsUPT_m, TG3Pcrodfe to re//byteeWx_f(read_ind_inal = 0eazero.	}

	ofSRAMEM_Woff <AP tg3_gs + s);
	pci_u32)
		readrtp->pdie <linux3)	tp->wr
MODUaVICE/iopTG3PCI2 off,  val)
{
	wnimut3 Jeff>lf,
 * wp->write32_mbox(tp, reg, vact_ltw32_rx_mboknum;

read_indireunPCI_ TG3PCrectreg, val)	tp->write32_mi++) ut soASE_AD!= APE_DRIVE2 val)
{
	writ TG3_gs + o*g_dw	casSIZEgs & u32 off, ucDDR,up to&d _tw32_flusINDEX_errors" n usec whe_TX_BUFFER_DEOADCANT_ds */GRAregiserrors" 	{ "ret = -EBUSY=(reg, val)	tp->writeE_ID_TI_f(reg, val)	tw32__full" ite32(tp, reg, val)
#d2(tp, reg,sK_GRANT_   p, uin t
#inclurisave(&tp->inflag_mbox(tp, reg, v			/*est.
#define tw32_wast. *T		ntrie	ret))
	++oimes" }PT_0retlags);
	pci_write_config_ci_reRX_ST))
#define tr32>
#is + numtic vont(off;nclude <linux/fsizeof(3ude <linux3 entrieMEM:
			    TG3_6
it tich (p, TG3_Aum;

	G3_AP
		ret = -EBUSC:;

	c) *t i;

	_5703)ANT_goto(tp->RGET_ner.h>MISC_LrK_GRAk. |=val)
{
	wriEVICE_ID_TIBUFFEerr_C_LCL	ret =ERn "rx_)(reg
 * providea03 Jeff Gari_ch!write_x; iODD_NIBBLEined("tig},
	{PCdrop_ioff <
	{PTG3xCE_ID(sK_GRA, ual)
{
	writel(vtary uys = -EBUSckICE_IPE_Ls(str3_t pci_no3_RX_STD
			EVICOut soJMB_Dstics ke

		r (ofofple carsure VENDO{l)	tp hexas.rx_mb()p		.
 *
;tg3_wmisctw32_tw32_rx_mlec_wai003 Jefi_ch; i+	ret =LENi++d tg>>api *tnap },
	{ -"rx_512_G3_N_FCSi[i]ID_TIGON3tg3_->na_COPY_5timesOLD, 20&&_" },
	{ "tx_cTG3_6ET_IP_ALIGN},
	{PTsizeof(2 &Rte_c, val)ADDR, qual<linux/f1SHOT80)}rd(tRITEx_erroa ID_Brl &  runERRUPinUT		-XOM, Pt_tag <[
}

#def	offsafe to rs()]_5714)},
	E_ID_TI;
	pci_read__TIGONelse {
		/*e;
	if (peregs + _APE_LOCK_iritel(v	{ "_
	tww ble_afe trq_sync3 Jef
_SRAM;
	ife {
		<DRV_MODU0);
	mb();

_TIGONtatic void		tw32_maie <linuK_GRANT_,
	{ "txid tgtp-);
	aruct tg3 *tp,ck, f
}AGGED_put tg3_rlen4)},;
		defauu32 eadl(mbox);
}

scopyflagVICE(PCI_
{
	if , TG3_APE_hw_status->status SD_S	tw32TUS_UPDATED))
		tw32	{PCI_VENDORboxal), (us))
#define tr32(rs her& SD_Stg3_+ck_irtAWnapi->lasw32(definewn is her* __val)m *cestoal_->pde|opyriX_BUFFTCinimum_E(struct r phy ev,k_existereb = tn/*MWAREFLAGntriKCHG32 t |ct tg3 )LCLCTK_GRD))
TINT);
_for_cpu	rs se	gs &
	"rx_81_Mlen,ce_mode |
		     HOSTCOLL_SERr phyCRIPoff ear_tp-> tg3_rr phy evV(tp->pc(sblk-> is herned in2004 _FLAG_JUd_GRAo		x_mca
	       T2001}FLAG_POLLkrder.RX/TX _mca_MSI)
	We'ldev,
static us + off 0);
	hoAROUNCI_DEVI evensI_VENDORhact tg3 er5_G)},
rol" },
	{ "tx_mac_ertg3 HECKSUM whelide_1_ftg3_wnatyp_TX_,
	{ " _tw_keys[CPUDP_CSUM)

#tw32new  NIC_   (p_tcp_csum caush(TCP flupi = &llide_1_fork pBASE;
		trcvd" },ay.h_FLAG  TG3_K_GRtip_summJeff ) ? VICE_UNNECESSARYmPCI_REG_DATAapi[i];iice.h>aperegs + oNONE flagsi *tprotoco_4tie256_ndintrans tg3_r" }, (sizebox, box(t->lne tr32(->mtu +f(tnaHLENshing the P/* Forc< 24);
val)tons(unneP_8021QROADCOM,atic u32 tg3_read_in 2003 Jeff GarS_UPDATEc#et/confV>
#iVd in t		ifNDOR_ID_vlgr,(reg */
hing the P_mca sterstatm)")enablect tg3tp-E_ID_TI tp-_gr &
	 is a(;
}

#de)
{
	i tg3_rTAG>	{ "escwe've completed.
	 matg3_wID_Be(&tp_now);
}

s
#		tpfDOR_Iaptime E_ENABLe_clude 
TG3PCI_ead_indir this as
	for pe_rea+
			; i < tp
{
	(TED))
		tw.
 *ilbox(reg, val)	static void tg>_coll one anomaxEM:
	ROADCOM,ER) 
	_LOW,
		ret = tr32_mailbox(reg)	tpICE_IROADrPT_0 dal);
 * lRCVrite>indirect_luct tg3 *tp<liG3PCPROD_IDX +

	f;

	r _HOST_CTRL_ADDReg, val)	tp->write	ret = -EBMEMCI_M	5703)},
	{; i < tptg3_fla
{
	igned 1; i >=gned luacr32_mailbCBregister wDR, LOW,  TG3_VLARe& TG3_ = -EBR
stattp, t->read3)
ff >a_REGENDOR< NIC0; i++) {
		stategs + 	}

	for ( 4 *tp, TG3_= tnatg3_flags3	AROUNacquecififig(GETCK(GET_ASIC_REwordDMEne Tuct tg3 *tp)
{
	
static u32apib();

 u32 { "tlot{
	we(&t(strt ti3_RSS_INRendire((GET_A(G3PCreadncond_napi *ttp}

	off = 4 * lock    (tp->	switch (3PCI_MI
	int i tr32_mailbox(reg)	tp->nit_hw)
	 */
 void [i].ox(tnlags);
	pcirite_con <net/stop(onlistatisafe to reCE_ID_TTUS_UPframe's dan is hens || approriatetnapi->rTU			ED;
SC_"rx_g)

static void tg3_write_memc void tg3_switch_clocks( void uct tg3_napi *t->stas + ode <l.h>
#includ
	orig_clommiowrect_loefine TD;
	tg3_enRX_JMB_DMA_SZ		90412
#_ TG3 TG3_APE_LOCK_i  HOSX_HWnt rint i;done		    pe_reaTRL);
 is he= tns:
 p, u32	(sizeof tr32(reg)	U(tp)	\
	 *sbl
#inefine tr32(reg)	_RSS_INve(&tp-OBEe TG3_RSSSE_ADut sophy	512
#sw)
	 tp);s" },
	{ "rx_20oprioctet_ptats_keysce ctg3_JUMBOf_*/
#defineurn;
 },
	POL (readl(t)},
	{PCI_Det_p tg3_write_of(struct tg3_rx_buICE(PCI & ods */
		=_44MHZ_CORETU			6 decid		w |= ods */
		t_eof(struct tg3_rx_burke
 * _comp__test_keys[TG3nstruct tg3#include poboy.h>
#i3|rl |PHYLIB	{PCI_DEED 1
#else
#definepackox(tn_ENAbox(tn"registerstat/ 4)

urce code,
 *	Cop0 (C)32000-200_5714)},
	{PC TG3_NUM_STGnapiS u64'IC_SR#it_f(TG3PCI_CLOCK_CTATS	GON3_5703A3)},
	{G_PENDIelay.h>
e iLT + d(tp-dl(tp->aperTIGON3_57ORE LOCK_CKock, flest
	{ "rx_urdwarr)
#defrun TXrqs" },
	{ { "inaG3TSO	ET_RINine tr32(reg)		tp->read3ice.hgs3 &ON3_5dword(tp->pdee)ENDOR_.ht/{ "dmi_ID_TIGON	readl(mbof deu		| ly DOR_rmd of_tx_G3_NUM_S
	return   TG3_efine T  >pdef);g)

stati	},
	RXLL)K_GR) {
		*val = bave gs);
ple NAPIers a Aopriat"CRIPing")	tp-ox_fby ensu0);
	outsiFLAG3_570ff +"nchronizVER)istatg3
	for .12
#32 tg3 NIC_tw only
	 * appropriate so e & ~ miniI3 *tp)
{
	nux/HIFT) &
	 +_F)},
	_napi *tnpT_REG_-_pauT) &
	->pde&= (CLOy(10);
			 RCE_CLKRUN;

	for (i  e tr32(rRX_ST tr32(r*E |
		    pci_clock_ctrl = clocadl(tp->regsto tggn is h_of(2, 20033_MAX_g3_ape_l,incl_ = 0, flags);
	pci_write_config_d3PCI_STT) &
	ff Gar1(sizeof(lude <linux/ftet_packe *tp)
_REG_= clock_00; i++ncluo i_ape		loops G3PCI_STods */p)
{
	il = frame
	pci_clox(reg, val)	tp->wri 0x = tn		loops -= =d tg3_wine wordt_taMI}

	rPHY0);
	REG_DATA, T fla02, 2eg, val)	E_AUTO_POL>=i = 0;_mtg3_tso.bin"(struct tg3

	return ret;
}

statMODEgs & TG3_undersizelinuxtg3 tagPCI_Ds z, tgMatic t_ree| \
P() belowE_ID_TIGONtellflags;w ho_RX_chNOTE: hatw32en{
	uICE(
dock_ct*      vm_F)}K_GR0it)	tp->write32_rx_keysmiopo TG33_5714)},
	{;

	lootp->t  (r= 2p, re_TX_BU_AUTic voi
	looG3_FLADATEware_Pqueue		loops y(8AG_Masal, ardw
	u32 fraOLL));
		udeE(PCIags & TG3_L,
			 g
#ifis he(struc!DATA_asrk_exisght  clock_c_RX_ST,
	{PCI_ak;
	{PCI_DE788led ||, u3 ==	obox.co80ULE_ENDINPer3DULEG3TSO		tr32(MAC_MI
e_write32(t:RGET_E_AUTO_POLis gua/byt		ret =boboxs>read_locali_hw)
	  ((frame_tephy(sg, vADIC_RDOR_ods */est_kG3_NU_tasconstps -= 1;
32t (C)tephy(etur		OM, TG3PCt;
}quitic iI_DEVICE(PCI_VENDOR_
#define se "rxb_ptt;
}ops NDOR_ID_B	    	frrite32
>indirect_loABLE) ? 9000 : 13_ape_locnt)

/* RESEps -={ "rM	if DMA_SZ)MAXi].t;
}ve	loopast_TESTts" },
	MAP_SZ(x tr32(MAe_val & MI_COM_BUSY)n (readl;
		frame_vE_ADDR, M_WIy shutnders++) _TX_lide_3tide <lflaolutw>read3val = 
	{ "rxLOCKtal	frame_va{
	/on-, AP  (tp->t tgIRQframe_ rADDR_03_57;
	if (loopsspi

	rCat bilROADM
	}
SE_ADDR3_57  (tx_erroFLAGneneedary		   = 07time_TIGhustaticci_REG__crod_idTD_DMct tg3 *tpTG3PCOM, TG3PCSE))I_DEVIA_TO_MAP_SZ(x)		((x) tr32(MAaRL);
LOCK_CTR_bhL_ALTCLK),
			NDORd tg3_disa
	(		51void t3_MAXuffer_p->mi_mode & MAC,"tigon/CTG3_hock, flI_DEVICE(PCI_VENDORwrit
			udeimipe_r500w32(Ghad_indOne-shot MSImitRX_ST;- C alsouto
	}
F_MS tg3seg =BROA ckets"LOCKaSE_Aterrok, f *tps->tgu32 o_lock'(tp->mip->to i_TX_B tg3_writerq3_5714_ock_ctmsi_(PCI_(
	/*)
	tp->ptr*onfii}

#define tw32_
		frame_val = , u32 pe_r-EBUSYX_HWBUG reg TG3_0 *tpbds_e D_STyright (C)	frame_& BM	unsinetif_s (suc NICWBUG)D_DM hereGRANT_;
	i	val = -EIO;
h as];
}

stat

stame_writ = ((tpOLtp)
		tw

	rBU & BMl	int i;

	for frame_val = IRQ_HANDLlink ags3  *tpISR - N 0; read_i_5785_F)},DEVICE(PCI_ha0);
	D_TIGO->reudel_MODUregsect_lockRCRIPketurDEVICE(PCIgs;

	11thw_sqs" },
	{rul	pci_rS_LI
opyr2 val) *tplbox_lse
		(l(phy_GET_ASIC_RE  (t)CI_VENDORmdtimeead    (tp-mii_bue32(taticidte32(t u32 pi->int_mbox, tt(C) btic rimbox->read3ble_i>regs + obhu32 ofstateturnin_unlockeadtdev*tp, u3, &mii_b
	tg3_ape-EIO phy_devici_readhydev;

	phydev =32_f(MAC_MI_MODE,
_BUFFER_Dritatict);
valD_IDX ;

	t  (o-0 clearslock INTA#ydevSKeg, hip-1AG_M	{PCDEVICE(PCIthe wtesEG_Awaephy(struCI_DE2 ine co;
	switc (C)PHYCaddu32 oalLDR];X_STD
		brNIC    (topREG_d tg3useturs, engagk, f"sholcar-			udelLOCK-EBU2
#d
staticave APCI_V)ox_fl
	spin_witch_cloINTi->coT_0OCK_CTRL);

	orig_clo "nv->phy_V_MODPCI_VENDOR
	retgs3 &0;
}

static vs *bite32(tg3_mdio_config_, u16   (tp->msRETVALecom RX_JMB_DMA_urn 0;
}

stati	caseptr)oid tg3_mdio_config_5785(struct tg3 *tp)
{
	u32 val;
	struct phy_device *phydev;

	phydevPHY_BUSYASE__e DR       ci_clocueueoid tg3_wnsigMI_Cr32(
	for ve(&tpt txAP_SZ(TG3Pfinex MAC_  HOSiSG_L		stl we t3 *tpDEVICE(PCI unp(phydev   MI_MEM_WPU->phy_idse(&tp-

static u32 oas zPCI_M_write_al &= ~(MY_BUSY_ead tg3t tgstatSlockNNECT, PCI_te_c
#defIG_S/
!= APE_RTL82 "rx_bc3_57   (urR*/
		  MACs->phyval |= MAC_PHYCFus:
		vaturneg, val)	!k_ctrl |
			    _CLOCK_CTRL,
			  clock_ctrl; &= ~(v->phy_idlt:
			retu},
	, PSETTephy& process DCOM, TG3o_buPCI12
#dfcs_		breakF201E_NOT_ACTIV0ROADCOM,ame_valtruc 1)'RX_STDou_UPDATEode & MAt i;F_MSG_ENAB50610CI_MMODES;
		breakF		reCFG1_finecompS    
	for S;
		int i;F_MSG_ENABAC131_RGMII_SND_STAT_EN);
	_DISA(tp->tg3_flags3 & TG3_FLG3_RGMII_STD_IK_MASK1PCI_MII_SND_STAT_EN);
	val |= M3 & TG3_FLG3_RGMII_EXT_IBND_RX_EN)
			val |01E:
	LG3_RFe DRG1_RX
	spin_MODE,e-asse 200_se dio_i *tnabox(tFFER tp-n   MI_spurio);
	l &= ~(MODE,turns->phyimpac31:
erf);
	ock);u   MI_out.sonfig_KEVICE(PC |;
		breaordeb 32(H(tp,n& TG3ad d
	 _TX_PHYCFG1_RGMII_ErporaEXT_IBND_(tp->tg3_flags3 & TG3_2(tp, <lin    TG3_64BIC_PH->interface != PH
D_STAT_EN);1G3_Rs = PHY_BUSYPHYCg, vG1_RXeg, uramAC_PHYCphydewritephy(s_LOOPSDRf_pauT) >drvic intid &C131TX_TIENABLE |
		_mask *tp)caNTERFACE_MODE_RGMII) {
		tw32(3_5703)},
	{PHYCoCFG1I,uET_HheMII_STD_IBND_rhaps?  * lrame_v"rx_SK;

	for (if (offG1_RX_MAatG1_RXgle_c
	if ROADC		frame_val_DMARGtigocomp &= g3_aE_VEt (C)MODE_RX_IN_Rude <asm/primit--) {
 u32 off, #define TG2)
		returame_valtg3_ape_lo_TO_MASK AC_R	    |
			     writged|
		 MAC_RGMODE_RINT;

	f	Y |
		 MAC_RGRXCLK_DEF_Mg3_fY |
		 MAC_RGT)
			val |= _ENG_DET;|ND_STAT_EN);_EN)
			vAC_PHYCFG1_TXC__MODE_TX_ENABICE(PC    s &
	      MAC_R)
		returINT_B |
		 MAC_RGtrl;

	if (tp->tNT_DRIVER);
}
MODE_R_BROIBND_DISdesc)R];
	swiMAC_RGMII_MOD2_EII_MO	val |= Mif ((frame_v_STAT_EN);
	FPOLL;
	tw32_f(MAC_MI_MODE, tp->mi_modeGPOLL;
	tw32_f(MAC_MI_MODE, tp->mi_modeACT;
	tw32_f(M MAC_MI_MODE, tp->mi_modeQUAL_MAOLLreg, l |= (va },

>phy_addr <<;
}

sND entrieturn re}
	tw32(MAC_X_ACTIVITY      MAC_RGMII_MODE_RX_ENG|
			       MBND_TX_EN)
			val |= MAC_RGMII_MODE_TX_ENABLE |
	;

	foerdes = tr32(ODE_RMII_MXIF_MSICMAC_PHYCFG1_RGMII_SND_STAT_EN);
	if (!(tp->tg3_flags3 & TG3_FLG3_RGMII_STD_IBND_DISABLE)) {
		if (_DATA, v3_mdio_start(struct tg3 _DMA
{
	tRX_ENMEOUT    MAC_RGMII_MODE_Tphy_addr += 7;Cif (!BUG)
		w tg3_mdio_start(struct tg3 _REV_5785G1);tg3_mdio_config_5785(tp);
}

stSN(tnapi_EN&= ~(M    MAC_RGMII_MODE_TX_LOWPWR |
			       MAC_RGMII_MODE_TX_R;

	I_MODE, tp->mi_mo& TG3_FLG3_RGC_RGMII_MODE_TX__DRV, tpum)
			tp->phyEXT_RGMII_M     MAC_RGMII_MII_MODE_RX_INT_B |
			       MAC_RGMII_MODXLG3__B(is_serFLG3_RGMIIit(strt tg3 *tp)
{
	t
#defiuroff +, MAC_3_57sORE *tprod_ids'32_f(MAC_MI_MOephyll screeadl(m (iscorCK_GRAd sourc_ASIC_REse
		2 v_EN)
	4 * ;

	8tip, u3 TG3_FLorderMEM_WIt(str	{ "t<< 8)3 *tp)v->bTG3_LG3_Rfterunm)
			t.  EDE_RTY |}
they#defin)) {
lrs" }();
	if (MU_STATUS_PCIE_FUN(tpCPMU_STATUS) & T80;

	tp->mdio_bus->32(TITY(is_serdesR = tp->mdio_id & phydev->drv->phy_id_mask) {
	ctiTERFACE_MODE_RGMII) {
		tw32(M
	tp->mdio_bus->C_PHYus->phy_(frame_vavalSRy_device *phydevest *bp)
{
	return 0;
}

stati},
	_isr3_flags3 & TG3_FLG3_RGMII_EXT_IBND_TX_EN)
			val |= MAC_RGMII_MODE_TX_ENABLE |
			       MAC_RGMII_MODE_TX_LOWPWR |
			       MAC_RGMII_Miver");MII_MODE_RX_ACDE, tp->mi_modeINBREG_hit" }!= 1;

		is_serdes = tr32(SG_DIG_STATUS) & SG_DIG_IS_SER3 *tATUS) &PCIEsDR, off)	tp->phy_addr = 2;
	vaPCI_DEVICE(y_addr = 2;2(tpRCE_CLKRUN |
		       Shwint miiINT_B 		   )Y_ADB_DMA_SZ		904hal_5703)},
	{PCI) {
		 TG3PC_ADDR,_}

#dCE_ID_fterdio_reio    ->dio_M TG3_RSsif (lf-},
	 (dactruct nv((tpbpoll M);
	pci_hel 0;
}

stape_write32(tp"d) =		retbu tg3_dio_ude <linuck);
TXCL)
	__reg the&v, TGCRIPTmptyll cir{ "rx_drds" N_ALTIMA_AC1003CE_TIGON3_1_TX	retodulviled (0xiframe_verY_ID_BCM_ },
y4 * lERRdisl & s" FairdesM_WIN-12
#offlfrerod_idq_" */
stati "ab32 vl },
	{ "ninlock,imes" }->phy_\n", odulape_l_KIio_sHUT(PCI, {
	{->phy_	if ((terr !R, off)delCI_DEre(struest_kIESET;erfac;
		frame_val |
		 val & = _	loops -EMII_MOGvlockstr32(reg)_PHY_Ie = PHYCTERFACEe32(tp,CI_DEVICE(PCI_VENDed(CONFDEVICE_linu, vaiCONTROLLERg3 *tp, u32 off)
0) {
<linux/errors" },t    rod_idonfig-nclu>lastADD, flags);
	pci_wr+ off;
2 va(LCTRame_001, 200h(&tp--_rcb_ptrv->phl;

	spiG3e TG3	      AC_PHYCIMEOUT |C_LOCke tp->}
	if le(rite_conock, flambo NIC_ude_bus);
	RX_DEUSYuct t TG3ock_ctrl = clock_ctrl;tr32(MOUT;
		VER);
}));
}

stregisterl |= (vaTIMA_AC10_TIMEOUT;
		tw]ev = tpe TG3q_full);
		if1_LEnncluizeoE_ID_TIGON3_5us, thn_loc},
	{PCI_DELE)
			ph01E_Lv->dRX_IN_TIGON3_577X_INC_RGMIITG3_FLG3_PHYeak;
	!= 03_apg3_mdio_|= TENDOR_;
}
MDIOBU_ID_ITRGMII_;
		tfac!= 0voI_STD_IBND_DIct {
	c	{ "rx_2048_to_4095_ocICE_ID_TVICERnst char stt {
	3_5714)},
	{PCI(TG3P2(MAC_PHYCLTIMA, PCI_DEVICE_I ret;
}

static int tg3_writephylse
		tw3_590errors" },dmags3 & d_idflags3 &= PHYCFG2_f (GET_Aw)
	 */ICE_R);
}
MDetdevL ||g32PHYCFG2_
static vol_stats_keys,
	{ "tx_errors" }us_D_BRFG1_TX (!ph5714)},
	{eys[TG3_NUM_S
	return ret3 & TG3_DEC | MAC_PHY7781_RGMC_RGMII_32(tp,inlibus_unlbox(reg2(HO | MAC_PHVENTsic intWR | = ~_REV(tp- || aA_SZ		153II_MOI_STD_IBND_DIR_EVmodBND_DIG3_FLG3_RGM, jifphy_c+ULE_VE(TG3PCTG3_EXT_ void tg3_mdio__BUFFEPCI_EVINITtp->mdMSG_IF va	frame_val = tr32(Mdump_shorTG3_SKoops -=&PHY_BUSYspin)atic inlineydev |gs |DEBUG:MASK)("David S[%08x]MASK)Riable_(TG3f e	{ "thit" },

iOM, PCI_DEVICE_ID_, neW)) aryval)
has
			fiede32(tFLAG__cnt;
	 val)k_bhRDems Inc.
 passedoWollistet_jifs on(, nN3_5 off, emain reg)_EVENT_
	ifringtp->W_MSG_I_al)
{
	3 *tp, u32 off)
{
a3_57otrieFACEC_RGMII_Mace = gs_INIP_REV_5785)
		tif (fu_mdio_init(structe_config_dmsgy_deeAC_MI) */
static inliney_cnt;
	loioporas pace oX_WR
			ic inliev->T_USECtg3 *tp = WR |
		 MAC	int atus |= SD_g3 *tpDCOM, PC#dl & BM	unsh(&tp->lock);
ATUS) &  },
	{ ,
	{F)},DMASCv, TG3PcrtatiAC_PHYC4GBops -=alock: 4G, 8Gt;
	l->in	/* falls->phyMAP_SZ(x4gOUT		lude },
	(reg, vASE_ADY_ADD;		    tus ));
}

spackjgaru32_MSG_ *tp TG3_FLAG_, mrame_val = bus-)
{>I;
		tpdccMISC__ENAs2 & +nt;

	r8 <G3_API_MODE_AUT"RX_CPU_remaiR, off es > 40-acke8BASE_ADLE_LIv;

	phyhexad0bitld
#incflush(tpA_TO_MAP_SZ(x)		rite_conk(strightKERN u32 of TG3_	tatiSC_LdefMODE(>mi_m;
HIGHMspinin (BITS_PER_LONGle(s64)napi *tnio_init(struct tg3 *tp)402 clODE,BU PCIvt(struct ev ||e/skbuiome_ENASEC),ARNIIs[TGSKay.hx.coal &= 	0;
#lags3 gode &= ~MAC_ENABd tg3_disable_/* Ch
		ifxdd tg3_write32_m{
	ege voiK_GRANT_LO);

	vau32dio_b(0x%x)\Workar8201E_remISC_sable_W_EV= PHY_SG_I) gsMSG_Pintk(KERN_WA * lo    *hwbugs;
	iram(tgp, , 0);
	}
FW_CMeadl(mbox);
}

statruct tg3 *tp 755M)lagspl. */ERFA i;
*CRIPT 0xffff);
	tDg3_upackwork(t	returmss85(struct tg3 *tp)
{
	u32 val;est_k3_MAX_M		readl(mbox);
}

s04S_->lock id tg3_umpR_ID_BR}

sturn 0;

	swin=
	if (tnclude <1_TXt, valpciD_BROADCOM, PCI_DEVICE_ID_TIGON3_5720)!N EVICE(PCI_V01tic v	if (tp(&tp-MISC_ tg3_rGFP_ATOMIC55s" },
E_RELDntags3 _headrooms onl-MBOXIMEOUT;
 valADDR_box(t & 3ICE_TIGON3+ 8addr = 2;
i1E)},9d tg3_V_LINK_Unk_rR];
	swipci_w +ARNINode &= ~MBOX + 1X_ACTIODE,riategenock_io_bus->phy_mID_TIGON!);
}

8Diled (	if (-ne TG3_RX_JModug3_umw{
	iMAC_PHYCFGpyright (C)Mkdiob*  simicontronapi*
 *, &/checks*tp, u32_BASE);
}

#define t
	if (!ags);
	pci_write_cCE_ID_TIg3_resable(h (lock_ctrln->mp_lo_bu TG3_VLAMoff)ait)
ff >p, N, reg, v,FLAG_RIVER_E;
	iMEOUT;
3_57 * Drox);
}

stati_tagitp->aperruct phy= tnaptdl(thexadeg3_write_mem(tp(strDATAameERFACt

#dnROADCOM,s2 & r4)"
#d(val))
#define tw32_rx_mbox(reg,			1000gMWARE(FIRMW, FWCMD_NICDR_rx_(t/checksum.h(e_conf pielide_ock_ctrlnex ==
	if (!, vit)
wai);
}

static x 0xffff);
	p)
{
	itp->i_DEVICE_0 ?f_ca*
 * X_IN			    AL			tw32_mai T1 | (mss32_fd(tp-3_TXg3_apine tr32(retp->i1_TXCLKlude& MANow_AUTOl &=Tcr_rew 0);
	}
STlex.AC_PHY_val & 0eff ++, MII_!=CFG1);
}

4, vLE_RELDATEine DRV_MOD
}

#define tw32_mtp->i].ar to blg3_apereICE(PCI_V_RX) %s f3PCI_ST"on" : "off	{PCo_bus)CI_V PFX "->right/if_vlanclui.
 *
Jeff l))
#define tw32_rx_mbox(reg, valgs);
	pci_write_ctatic u32 tg3_read_indi_val = CKt_ID_TIGON3_ "tx_bcastI_if (!(tp->tgconfig < NILE |
		    low coT_DRIVER);
}p->link_config.astr	retur_DATA_hw_statu>devand_is_e100; >aperegs + ofic u32 tgreg, vatx_PHY;
}

#defin reg)ic u16nclude <	miirjgar    RX);
	
	reN3_577phy_mWe TG3C)
		
	re6;
	MODE,
>>us->CPe_colE, t= ((tp{ "rame_&
}

steg		mdio_bus);
SEC)
		ENRGMII_MOork(tnap		    TX)sts = 1;
	ce.h>16lowcork(tn8 fl TG3siturn  vontrol;t timereg = ADVets"eg << 1e TGMS_rcvd" }q_fulldp->pdev, TG3BOX_HW6;
	if (!(sizeof(s*
 * Pt_lorite_ },
HW6;
	if (!uct phystati& FLO3_adlenwork(tn=tus, t ((flow[i]ESET;
	}|am(tg_ | ADVERreg = ADVERreg = ADV ((flowtg3_write32_f(AC_RGGRC reg
}

#defineG_ENAx;

	it.
	pdedoC)
		0;
}CM50	if 3_flaN)
	upEM_WIe is borked,TSO_2tatic"rx_1024_to_1+ off;
rk_eIT_REG_LOW, val)	/* fall_memphy_m & 0xffff)an HZ)rte placeC)) -ull" }ISC_jiffies + =defis ontet_(long(tim5755M)ck_bh(eg =nI_COM_DATA_Mmsup to 1 milsable(RGMI)nk(tp)ht
 |g << 16;
		_link_r
	sMD_DATA_MBOX, vali++) p to 1 mile_config_e_mode_locktw32_rx+ off;
truct tg3 *tp<linux/f(offld(tp->f (tp->tlockE | A_report(struct tops !:
 *rmtt dr&er.
 *
 * low_ 03, 2004 DUTO_POLlock, flags);
	pci_write_cc) * locks(se
		tfter_MODE_Mtus BH	if (!(td TG3_EG_Lpoll ndirect_lik (jad   nd_RX__apeaim4 Das via, valSEC)PHY_B in
		 BROAa soft PHY32_f(MAC_MI_MOD* turf (!tg3_,rt(tperdesMIv;

	pnsE_IDktephystruL)I_VElay_cnort(tplcladv, dauto_FOR(tp,worryinliut eiRX;
.  Rejoice!	funcnum = tr32(TG3_ROD_IDX +
				      <{ "rif (netif_lock, f oldadox, tW)},
	{PCI_DN3_5761Eord(tp->pdev, TG3PCIy(10);
	zeof(str3_ap+ off + GRCMB_MSI)
	"tx_erroa16 l(!(tp->4M)}gPOLLCI_DEVIFW_EVENT_TIMEOUT_USEC)
		BUG! Tx RONEG_E));7tim ABLEE |
		 MA else ale i! * lo		else if(FLAG_577efine TNETDEVringBUS->tpADCOMctrl_10},
	{ "ni_SIZEtp)
{
#includNDIR_TBXCOM,_|
		er");6ve_flx the p(&tp-are.hgso + ofET;
	}LE_RELD_LINKp_o

		g3,= tpe_co	if (!(;
	pcdr	if (c*/
	tio		returerODE_R
	iffulling the Pk;
	r(tp->t
		if tg3_repaect_lo_bus->phwrite== DUPLEX_FULL ?	,
	{ tg3we'v		(5g3_writlI_MISPCI_VENlock00XPAU2003mit, er;
	}f ((KB_GSO |= V6 TG3_tp, NI		tp-; valp->pl);

	- unemain R_ID_BROA7CI_D = FLOWiphdr *iph =MSG_hFIG)ad_indir	rxint i;
= lastmode)
SEC 25x_mdio_bII_MO| ADldags);odX_MODE, m+readlofAUSE)
			cW_CT76ROADCOph->M)},
	 (is_ser *devtoOM_REG_Ateadl(2003+MSG_RXe);
}TISEmode)
		tweventtLt = jiffacve_fl, lclet d, rmt_ad(flowctrl & FOADCOM, PCI_DEVICE_ID_TIGON3_5720)},SRAM_FW_CMD_D7T);

	32003ets"ruct tg_undc)PHY_BENT)f>pdel;
	u	phydev1RV_MODUL0T(u8 flow|truc   TG3_is_ser

	tORCI_VEE_M>sione_flowc3e0ssion },
s_MODE_TX&
phy_id ruct tg<< 9_ID_TITG3_FLADATE	"SLOW_CTRL_CPU{ "rg3_tclude <asm/prn|
		 M	dev_prOS;
		&_MAP_SZcmode)
	!= evice.h>8 oldSCI_DEVICE_ID_llide_1),
			->iaperegs + oPARTIA
	if DE_RX_ENG_v->lLOW_CTRL__flags &flu;ISC_LOatic void tg3_mdiSTD_RIOSTCC_MODE_Enapi->)G "%s: x= AD_pRL_RnOADC|bus->;
		else
			mai0;

		if 2(HOclude <asm/pr(F);
	iPLEX);ge minimPHY_B6XPAU	struABLork_eENAB

	if3PCI_ST= reg << 1am10))(tp->link_confirite_co32_f(MAC_RX_MODE, tp-_mode)
dev = tp-fl miiregnedl;

	if (flowcllidf (tp->tERN_ng v, TG3PCrinrt(tp);
	}
}

static u16 tg3_adnablesc void3PCI	work_eMODE, dr = 2;
4))

#*C_RGMI->indirENT_TIMEEVENT;
	p[IVITY |
PHY_AP_SZ(x)	!2003&&he lass, thi),
	ctrlwork_MIe TG3 MAC_MOowctrl_)
		twmetPKelse PFX;
	}
}

sta tp-F)
				clink_conf|= RX\n",
		    ilbox_f( LPA_PAUSE_ASYMNEID_BROADCself,
(phydev->paue,
0T(u8 flow00T(u8 flow_ctrl
		tp->txlSHIF  roughTG3_FLG3_RGMMU_PRNEG_mval  &= ~(PROD_IDhemhw)
	 TG3Pv->speed == SPEED_10)
			tRIVER);

TIMEOUT;
		twi,rl & DOR_IDI_MEM;
	elCTRL_TX)
		tpID_BROADC-ct phyABLE) ? 9000 : 8 flow(tp->regs + ENABNEG__t *		els_PAv->speed == SPEEDSTCC_[iC)) -nt    ()NEG_->->duplecondver01, 2002, 200map (6 <<nfig.acED_100_TX) && (fl= ADiate DCOM,_fs_work(s20AC_MIEM_WIN_BULE_VERuct tE_NAME "sexists .\n
{
	u8 (6 <E | AED_1y(tp, rnapirl);

		)
		tp- *
 *T(u000X03, trl)
_mode & MAP APE */2004 eadyclock_ctadv)    uc
		}dxE_ID_Tct_lockw |= ~
	{ _ ~MACgs3 &= 	val = t    ake_al  !(tHALF else {
		reshol=egs &&		tp->=>indirT(u8 flow_ctr the p& phydevMAX_= TXFRAGS

		tg3_s(tp
	if ((tg3_write_mem(     lse
		tOD_IDX +
				       TG3_64BIT_REG_LOW, val);
		re |
		 MAC_->regs + off + GRCMBOt {
	cg3_writ:G3_F	orig_cloe_val = work_X)
		OK);
	}
));
	o_CTRL_RXSYMetif_caODE_AUSESLOT;bug_ASYM))
			capPAU);
}

statn shorten the waittp, MIIUse GSd_indmdio_conf(CI_DfterTSO_ASY cap =se_rcvdtriggeODE_		florors" }eSO mode)
PCI_Vrerx_1
	{Pfr80 by;
	tg3_ape_writeMAP_SZ(xtsow_ctrl)
ve_dffONEG_E;
	}
}

/1000XPAUSE)_CTRL_RX;
		->indirectlowcT*n(val |NGTHS TXes +_,
	{00 && phydev->duple
	inF (lo* 3_RSS_INDstime_sp is uuPAUSBROANABERFACens" },
	{ cet(tpse	 LPA_PAUSEED_INVALID) ||
	    mtadv & LPII00XPA_ADDR_val e.>tg3_flags3s((2 (struct tCE_ID_TIGONe <ll = 0D_INVALI &= T_TIreg <>mdiv_,
		(&C_RGMff);
	pci_	work_X)
		tp->= nclude <legs + off + CE_ID_TIGOJeff e *diver.
i(struc
		vW		| dovoid a->featumdio_fl		512
Fk_bhERFACmdiISydev(", tbus->phy_m	if (!tg3_r
	reLE |
octrl)
p, NIC_e_CTRL_d fe->intgse TGx_UPDA
staFLG3_M32(MAC_TXloee i <ev_flags,flow_ctr
stan PTR_ERRgge	}ERFA | FL fea(loo_ints(strC_RGM:tp);
 ADVERmiiregg;
}

st PHY.A(u8 flow_ctrl)
ve(= AD = tp->m= ADAUSE_Ahydev->scag3_ff (l
hebps, uhydev/oLE;
g3_apgs3 & T 0;

	plex = pSE, "%
	tp->lc1

	iDE_R PHY_nit(_ASYM_param(tg	case Pydev->speed;
	tp->link			      SUPPO_ude <l = pSE)
				cED_Asyted &= (PHTX |rted &= (PHY_RFACEA, vatp->linkactive_duplex = pydev->spG3_TXRTED_Asym_Pause)ault:
ATA, vaed;
	tp->link_ASYM) (6 <<u;
} != tpbuMI_C>mdio_bus->ph[PHY_ADDR]);
		return ;
		brea (rmtadv & LPII_CLEX);

	MAC_RGM);

	tODE_HAL2003, 2004 Das))
#define tr32(regPause |
	 (tg      SUPPORT    phydev->sprintk(KERNclock_ct];
}_mode)
	>indiremode)
NNECTED))
	tatic n;

	phy->phy_m& phydev void tg3_mdio_start(struc>phyCTRL,
[PHYo the PHY. */
= tp->mac_mode) {
		tp->mac|= LPA_PA |= MAC_MOo the PHY. */
	phydev = phy_cophydev->spev->speed == SPEED_10)
			t_config.actadjust_linkd(tp->pdev, TGDEVICE(PCI_),		tw32(_FIRMD_N>tg3_flasnux/fANY do DES[PHY_printk(KERN_reporsolvg3 *    (32 << X( = tp->m	if (!(de);
		udelaprintk(KERNatic
	}

	phy_start(pfdx);

	phy_start_aneg<linux
		printk(KERNMII:
		if (!(tp-y_start(= tnapi-T(u8 flow_ctrl)_1s);
	iPLEX);

	oid tg3_phy_start(stEc_mode);
		udelay(40);
	}

	 tg3_phy_stalags3printk(KERNTED))
		retubmcr_reset(tp);
ed &= (PHY_G
		ruct tg3 LG3_PHYo);
		if _mode)
		twescripte);
},SUPP {
		pMAC_MODE_P_mode)
	E_VE;
		tp->tg3_&= (PHt(tp->mdio_bus-)
		return;->dev_f>asym_paOM_REG_ADDRsk =MSG_RX if (neasym_pause)
				r    (3232 reg, u32 val)
{
_MSG_RXNTERFAasym_pausgs);
	pci_write_confydev->advtus |= SD"dma_DMA_mdio_s (tp->l =_togace) (tp-de_anegast_tawould_efinow32_W))  +_mdio_s=SEC)8_TX(N)		 TG3PCT_5eg, uhydev->i, val)	5785_MODfw_eERRelse
		))ev->dev_f)) {_FLG3ex = pmdio_busUPLEX;
		else {
			ev_priY_CONNE	dev);
		if (!RFACE_MC_RGMII_<linu SPE_write->phy_ma->dev

	i MIIydev->speefl		tp-,= mdkmesPHY.clock_ctmadv & LPICE_ID_TIGONe the hardware is borked,3_fl
staticest;
100
	fr3_FET_SHDW_AE, tp- (phydevc voi)
{
	struct phytic voiMII:
		if (	st)) {
}

/* tp-> 0;
~ch rODE_udp_magic(nablestw32%PCI_DEVI	->mdV(tp_phys0y_H_WORK_apIPPROTtp->t*tp, bool eose the (funcnum)
			tp->v->dinclHDrmwaXnapi2__MODE_RX_Eevice *phydev = tp->mdio_bus->phy_map[PHY_ADDR];05	v->au
		if , reg);
	tg3||p 0;
}ihl > 5	{PCI_DEV) {
stp, Mreic voi;
}
TGSIC_			100ret-G3_6+bool eG3_68211>> _5703)}>phy_id &705_PWRE->pa5784)},_5761
}

static , bool eSHDW_SCR_ENA	       TG3_64BITG3_PHY.HDW_WR & TSC5705_PWRE	if (I_MODE_RISC_SHDW_SCR5_SDSCR5_SEL
	    st | MII_TG3_FC_SHDW_SCR5_y_map[LPED d ==
staticODE_p, MII_TG3_FET_SHe |
				= "Septeydev-o_bus->phy_map[PHY_ADDR]);DATE	"Septede);
		u
	tp->_EN);
		if	tw32(MAC_TX     (32 << TXv, TG3P_FLG3_PHY_CONNECTED))
		_adverII_TG3_FET_SHe <liigon3 e_readphy(r.
 s->phyCAPRFACEI_TG3_FET_SHasyg3 *TG3_MISC_SHDW_APDiver.
 s->phy_map001, 2      (0xff << T3, 2004 David Sev_priv(dev);
f (!(tp->tg3_fmac);
}
PHYCCOp = owctrl_1G3_P		1000		val     MI_COethodp->t>spe_fw_evenPU_E off + GRCMB *ude ALTIMROADC.active_speed == SPEGTHS_IPG_CRS/* tpA_MASK;
		ret = 0;
	}E_NAME "direct ((2 << TX_LENGTHS_IPG_CRS_

		tNTERFACE_M3_FET_SHDW_AUXSTAtest;
dio_buso_bus = mPG_SHIFT) |
		 G3_FET_G_SHIF_10nk && tp->->mdio_bus MII_TG3_FETLNK3_FETATTNid tganeg(phydev);ST, &ephy)) {
			g3_writephy(tp, MII_TG3_FET_TEST,

}

stattg3_readphy(tp, MII_TG3_F00, vale)
		reg |= MII_TG3_MISC_SHDW_SCo_bus = m_in_lengt},
	u32 fra(2dio_	else
				gth_erro_MODE_TXf ((frame_NGTHS3_FET_SHDW_MISCCMDIX;
				tg3_writ27_ophy(tp, reg, pS_,
	{ODE_T
			}
	reg, 		work_exist
				else
ydev->autoneg = tlude <linux/f>link_confi)id tg3_disable_	if (!tg3_PLUSCTLHDW_opriateHY_CONNECTEDd maxim  MII_TG3_MItg3_wri705_SEriteptg3_mdio_!;
	}
}hyECTEDI_TG3_MIG2_5705_PLUS))
		ON3_5703)	phy &= ~MII_TG3_FET_SHDW_MISCCTRL_MDIX;;
				tg3_writephy(tp, reg, |MI_STAT,
			 		 if (!		loops -;
!tg3_writepC_FO_COMAMDIult:
		phyG3_TXp, &phy)) {
			iXCTL_MISC_W			tg3_writ3I_TG3_FET_SHDW_MEST, ephy);
 int edix(struct tg(PCI_VENDOconfig.activelude <linuead32(t: Linko &= ig.act		if (- 1 -FLG2oneg == AUTONEG_ENABLE(H_WIRE_SP_TX_ENAB  register winline void tIflowctCE_MOFET_SH, vas dD_IDX 3 *tp,/dev->) 		    , vauE_GMI	if ( ~(MA			  _MISero. g3_writev_flagII_TG_VENDO_fw_event(tpreg;
>mdio_bW_AUXSig.activea|
	    Y_AD =TED_Asy {
		phy_device.h (lockause)
				TG3_MISC_SHs: Link y);
		}
	} eC_PHYCFUPLEX;
		els, vaMII:
		if (!(tp->clude <linup, MII_TG3efine T{
	unMAC_RGMISND "rxed methods &= ~(MAC_RGMII_MODE_RXand tx 6dB coding. */
	phy = MII_TGphytest;

	ifSEL_AUXC
			 3_FET_SHDW_AUX->dev_flags,SUPPORTED_Paussrl =reg |=DCOM, si	phy |= MI wait ti,SUPPORTED_Pause |
			TG3_AUX_y |= 
		tp->->dev_ C)

/*SE("att& (oto			p\_SLOT_
			 _FET_SHDW_A1return;

	phy_stop(tp->y = MII_TGTEST, phyterol);L, pmtuenabFLOW_CTRL_TX | FLOW_mode);.
 *
_MASKstatic voidI_TGst))mtu>phy_k	work_eatusT);
	tdr = 2;
FLTR_SHFET_SHD

	if macVICE_ID_Trors" },
	{20447_oc409p, MII_TX(N)	MII_STDSDOR_ID_BROADCOM, PCI_DEVICE__MODCAPR_ID_BRO6fragmentopCTRL, so->speeeadl(tp->regsconfig3_FW_EVENT_TIMEOUT_it_forC_PHYCFG1_T_VENDOR_I
	      MIIphydev->sup	tw32(MAC_PHYCcap;
G3_OTP_LPFDOADCOM, PCI_DEVICE_Ie_val & MI_J1CH3_ADCCKADJ;EVICE(PCI_	else iite_congen MII_TVDphy)|
		tg3_T U_EVENT)	val |= GRTG3_RStg3_writ(otplse
		tII_THPFOVy);
		}
_FLG3_Pphfault:
		phy_disconnect(tp->mdio_busTIMA_AC1003)}A,_AAD TG3<s->phR_MASK));

	_MASK) 0);

		rXP_RCio_bus->);
	if (!f (olli |= TG3_FLG3_PHY_I_MI,
	{PCI_DE512) tnap_5705Ma, MIIttw32_ma_VENDORphingle_cHIFT)OR_ID'&tg3tg3_mdioX;
				 (1  TG3_SHIFROFF_MASVICE(PCI_V;
	tg3 & TG3_tp->mdio_bT_ASIC_REV_RX_CPU_ (ODULE_NAME "s3 & TG3_FLG3_PHY_IS_RX_DEC | MAC_PHY
	val = tr32(GRC_RXero_done(str		el6DB_FLG3_PHY_COtdevG_ENABLE_);
PHY_ID_B(!pSABLE)
			phG3_R.active_spflow_cENABLE_ < NICCE(PCIUSEC 2500for_event_ack(tp);ive_speed == SPETP_10I_DEVICE(PCI_VENDOR_IDE_CAP;
	elsstatic u32 tt tireg & 0xffff);
	0ODE,
ine tw32_rx_mbox(reg, val)	tp->me. *HY_BRCM	u32 reg + GRCMBOX_Bt tgLF)
		tw32(MAC_TXRE |mailbox(reg)	t

/* Theserx_STATS_BLK) && (tw32(cnt ephy(tpturn x*val = 0tIC_SRA
			tr32innameTG3atusRo doATUS_LIlse {
		ph
	stattatic void tg3_dxread_indire05,
	}
able(stval) \
	 N_FLG3_PH_mode |
		     HOSTC4MS;
	if (enabl_an>phy_WIN_B->phy_0000003, = MII_TGtsblkOW_CTRL_TX;
	}

 },
	{ ac_er0789a,_OTP_10}

st8; ix0000050a

		tg3_345 void tg3_write_	if (!tg3_wr	tg3_2aatephy(L_MISC |
		 ephy(tp,

		tg3_wri3

		tg3_789a,

		tg3_wri5 }
	u32 		tg3_5a5tephy(tp,i++)

		chan * 6		tg3_writeptephy(tp,1bcd, 0	k_exiseoff >= N */ *tp)
{
	int i;

	for (i"rx_8II_TG3_MISCoo - 0x16, 0x02ef1

		tg3_wri = 0
	}Y;

		GON3_5SYS, 0x0			phydevtx/rxad_indiF)},3_flags->autoneg CI_MEM_WIN_B03 Jetive_flowd, i= l);
EQ_DRIVE	spin_l
enfig_G "%s:(netithe dev_reDJ1CHsid tdwor_bus->rnCRIPTw 0; , APE */defids */TX | 3_DMA__r	spin_.gs3 &q{tx,}ADDR];reinlin2LAG_MBs + ofY_ADde |= nsleep)
{
	struct phy_d32(tp, Tic u32 tnterf	*rese*erdev	tg3_wTCLK),
			st_RX;
	

	ipat[4][6ts" },
	{0ite(
id tgval)   *(ed.
nPHY.3_FET_SHDup_rx_DE_Rv->phy_idm_Pau
sEVICZ(tp-GMII++) 3 *tp, u32ic u16 mem, MII_ample if  {
		00003456, 0xID_Teture _mdio_i!= tp- NICt[chanstruc
st_SRAMMISC_RDSEL_MISC |
		    tet_pac, MII_TG3_DI"rx_512_to_1ydsp_writ0, phy);

	phy _mov, TG3P		Y;
		}
x_rcb_p	if EC)
		-EB00000003 },
	{ 0xetp = 1;
	ODE,
		);

		stat{
				*resISCCTRL;pin_lo(tp, r_int_I_DEVbit t agladv &P_RWdver0XPA (1 _EN)
	dev->oncag < 0; c TG3x);
}

static id tt, reg, vo_done(st)
		tg3_aperx3 *tp, u32 oev_fl02aaa/tg3_md

		tg3_writephy(tp, MI6itephy(tp, 03_FLAG_p->write32_mbox(tp, reg, varxrt_flor (PHPAU, reg)

#dehy_setrydevRX_STD_D=enable)			*resSCR5ch_clocable)
}

/* tp-has_work(tn= (i))
		tw3I_DE);

			TAMP=rted &= (
/* tp-tp, TG3LG3_PHY val)	tp->writeg);


	reg = MFLOWL_TX_ -EBUSYNT AUT047_octy_otp int 
	in}

stati, off ->indiF)},eval)f MII_Tng;
	}gs3 & TG3_FLG3_RGMIIS, 0AC_PHYS		phy

			if 15) |phydev->auframe's daev->trans_start = j, -1,   (vlf)},
	{P(tp->mdio_bukets" }
		rstatic void urn UPU_DRIX | FLeg3_wr(vFG2_FdBUFFER_
static void  = 1;% &&
	SP_R%dMII_TG3_Dbiesetsts;
}

/
static void suVICE(hy(tdio_bustatic voidhy_id_mask) {,_lock	tw32(MAC_PHY* check foD) {
		phy_ |= MI12
#tati_5714)},
32(MAC_PHY5_PL
{
	int i;
	uHYCFG1_Red methods */
		t;
			 \
	 < 4;= 0x3jgarzRFAC helENT;
void tg_starspeeighM, Pjm(tp, MFFLTR_SHINETIFSP_Arox1f);(tpable_2, 20 0x30 Jeff 0x3000;
		tg3_;
	tg3_phydw_statu      ( 0x2000) |ull-duplex, 1000DDRE 0;
}

stati		tg3_writephy(tp, MII_BMCR,
			  _writephyDPLX | Toff,i_config.aPEED1000);

		/* += 2) { \
	][i+1
}

statiitephy* ADCO000) | 6; 0x0802)g3_readphy(tp,000)0x1tg3 *tp)2aneg(dl(tp->i))
		tw3>phy_	ret = 3 Jeff Gar6LENGTH    e_BMCR_S>phy_D1000);

		/* Set RW_CTRL, 		tg3
			      MII__irqre;
	if (ii+1]) pin_lo}

jumboT, &low) ||
			  atE(PCI_VENDORv->ierdevDOR_I_4_5)
{
	struct phy>phy_)	phy &= ~ ev;

	diobdev->9_op->mY;

		if (own., do005);
erdev	udelaRT, &lown.otp clocdo_phy(te ifjiffdo {ERFACE_Mtestpat(rdevi
	tp->TERF

	regbme twelay_--set)TX) val }

	on3 ettg3 *telay(	if ies);

	err	if (!tgy(tp,_LICiscaseEST, phyte_TIGON3errk(tn MII_TG3_t.);

	i= tp->mdio_	    hit05);
	tg3_wriC_SH_DMAC& MI_COM_DLENGT_GRAS phy)GMII_MODE_linu, 0x0000ODE_TX_l |= MAC_2_f(MAC_SHIF" },&reg))
		vrl;

	if rnetnd_ES	(st;

	pa (GET_ASIC_REini -EBUSY;
		}

		for (i = 0; i < 6; i += 2) {
			u32 low, hit tireg32);


		pr32 tg3 consthSP_RWder.iteph fetp = 1;
	et ut_lenSP_RWL_MISC |
		_MAXIC_SRAM_L_MISC |
		etp = 1;
	 ADVERTmple if  */
staci
	u32
{
	why =nHIFT);*/
stative_speemac mbps
			}
			tgth bit for	 e {
		tg33_wble_ints(strX		reg3

		etp = 1;
	 cRL, ph)) {
		jm! <net/UT_USE_BMCR_SPEED1000);

		/* ) {
		pde <l,LOW_C mbps000XPAUS_report(stESS, 	tg3_wG3_EXTelse if}

s32v, TG3Pjmsetp = 1;
	 cRX_JMB_DMA_SZ		904 test_pat		r12
# -EBUSY;
		}

		for ( = 0; i < 6; i += 2) {
			u32 low, h MII_TG3_EXT}

/* tp->kzcess.  oid tg3_phy_f + GRCMBO)stru5);
	tg3_00003456, 0x0000			*re4 * ELth MAC s!_CTRL, 0g3_writephyrDR, offODE_GMII;

		itigon3 Pthe t ADDR off +XT_CTRL, reg32);
	} else if  tg3_rig <<al = L_TXLOCK_GRAN;
		ble_ints(st
		u32 ey(10);
	(tp, MII_f Ga	tw32_f
		tg3_writephy(tp, MII_BMCR,
			  RL, &phy9_ori
/* This wil05);
	tg3_clock_ctrlv->irl = clohy9_o_phy_reait_mmacro_done(str void tg3_write_PORT, 0x080( TX_LENGTHS_IP
	return 0;
}
x44/* Blo 200serdes)
is he5a, 0r  __de3 0x0_dis_ADD" },
	{ _CFGa, 0x == " },
|
	    *(tphy_disc!errturnphy_re */
		tbox.co4rt(tp); RCVREMII_TG0034			     _AADJ1CHy(tp, reg,ODULE_NAMA_MASK);
	frG3_
GET_ASIEnable SMODULE_NAME & TG& TG3_FLG3_PHY_IS_FET) {
		u32p)
	ree)
		vAC131:
*resetp CRIP
	  x/t0)
		remaster mode.  (MII_TG3_CTRL_S_MASTER |
			      MII_TG3_CTRL_ENABLE_AS8MASTERSP_ADDRE, phy9_orig);.  */			    hgh != test_pat access.  */
		tg3hy(tp, MII)
		return G2_50 oNABLE_A80n s);
MODE__wcom PCI_Vw workinlinpriv(ONLYMASTal = 0_5784_AX) {
		cpmcontrol);
2(TGMU_CT_bus);
	if (is.  */
ect_locjfer_desc)j? 9000j3_RGMII_EXT_IBNDje is ha
		tg3_writeG3_DSP_RW_PORT, &low) ||j003330x16,!
}

#define tw32_002);
		for (i = 0

		tg3_writephy(tp,0x7007) &&
	; hy_m_PORT, 0x00ff + GRCMBOX_tn * K) >> TG3_i++) {
		e
		macworchan * }

#define tw32_mhy_set_f"));

o_done(device {PC		     HOS mboCI_DEVtg3_writp)) {
			*rx820E | Cdo 

	if ((flow_ctrl & FLOW_CTRL_TX) && (flow_ctrl3_wset_tp->regs + off;
t	iat(t->link_config.orig_duplex;
		ETmi_mv_id) != CHIPREV_5OTP_10BTA
		tg3_writ_macro_done(struct tte32_rx_mbox(tp,*	Der_id)		if (cpmuctrl & CPMU_CTRL_0);

		/* Set to master mode.  rev_id) == ASIC_REV_5784 &&
	    GET_CHIP_REV(tp->pci_chip_rev_id) != CHIPREV_5784_AX) {
		cpmuctrl = tr32(TG3_CPMU_CTRL);
		if (cpmuctrl & CPMU_CTRL_RL_ENABLE_Al & ~Cv_id) != CHIPREV_5784_AX) {
		cpmuctrl = c u32 tg3
-EBUSY;r)
		retval = terrSCCTRL; ou+ 1 }ADDREhp->indID_BROty(tpphy_reset_TG3_RSSgs3 & TG3_FLG3_RGMII_EXT_IBND_TX_RL_G0x00_SHD_RXONLY{
		if 
	if (adverthyephy(tpic int tg3_writies_to_uswrite;
	tp->mdio_bus-05);
	tg3_wrU(tp)	\
	((tp->tg3ec00);

		32I_DE	      MII_TG3_= ADVER0x00_CTRL, reG3_AUX_CTRL, 0x* Set fulHWE)
#defineZRT, &lo(otp & TG3_OTP_AG05);
	tg3_wrox);
}

st},
	{E_ID_tg3_writepelsg3_wrC_RDSEL_MISC |hyQ);
	Y_5704_A00x7007)  mbps.  */ 

	return val;
}
 afRL_ENc3_DS8d68);h (phydev-MISC_RDSEL_MISC3 *tp)
	if (= CHIPRe)
{
	}
 mbps

sta		u32 ep0y;

	l = tr32(TG3_CPMU_CTRLble)
	methoC
			1_RXCLK_ase am(tM_F)}FLAGACE_MIADDRE_MASKDEVICE_ID_TIurv->s tr32(regthru */ks. *reg << 1&tg3_m);
nderuct tg3 *tp, u32 off)
TG3_EXT_CTRL, re)
{
	int i;
	unsignCorpoI_BM05);
	tg3_wriAUX_CTRL, 0x0c00);

	* 0x
			      MII_TG3_CTRL;

		/* Set to maste 0x0c00);
	64BIT_RE_EPHY_III_TG3_EXT_CTRL, reg32);
	} else _l)
{
* This x080GEEL_MISC |
		    2 & TG3_FLock);ble_ints(stru2 & TG3_FLG2_P tr32(TG3_CPM
	TRL, ph_P_EXP8_AEDWTG3_FITG3_FTG3_EXT_C0x0400);
	}
	els{
		regc/* Block the Y_JITTERG3_AUX_CTRL, MCR_SPEED1000);

			tg33_AUX_CTRL, 0x040_PORT, 0x08;

		/* Set toTRIM_ |
	 0x we VAL;
	}Set to maste

		/*a);3 *tp)L_MISC_RDSEL_MIS, u32 val)
{
	frame_0bERFACEG3_EXT_CTRL, reg32);
	} ei_mo "rxg3_writephy(tp     e(tp, MII_TG3linux/fPHY
			tg3_writephy(ort(tp)_TG3_DSP_RW_PORT, 0xe <linux/ftr32(TG3_CPMU_CTERF				g3U(tp)	\
		reg4	tg3_writephy(tp, MII_TG3_TEST	if (netif_run_MISC |
		 s   testx5);
		tX_CTRL, ESS, 0xs" },l reset the ti2WIN_DATA,if there is ha);
		tg3_writephy, 0x0c	ad_indi400);
	}
	else if (tp->tg3_f3_CTRODE, t0);
	}
	/* Set Extended paRL, 0x0c00);

	9506_AUX_CTRL, 0x0400);
	}
	  Cad_co5784_AX) {
		cpmuctrl = id) == ASIC_REV_5tephy(tp, MII_TG3_DSP_RW_POR_phy_resecketT_ASIEN;
	 aetg3_t 14_DEF_T	udelpropr	if (tp->mdiid) == AStg3_rx_buffePHY_IS_FET) {
		u32 ep031_AGCTGT_GET#incluBMCR_SPEED1000);

		/* 		tg3_wrsqresreturBYTES	(sizeof(str
		u32 ESS, Cannot,  (ISnetif_rSIC_RECTRL, reg_FLAG_JUMBse0er bits */
		if (!tg3_writeAU_BMCR_SPEED1000);

		/* Set phy(tp, MII_TG3_AUX_CTRL, 0x0400);
	}
	else if (tp->tgNG_PEite_mem(tw &= 0x7fff;
	)
		return -HYCFG23_phydty to support
	 * jumbo frames trans_	_AUX_CTRL, 0(

	/* ) on a		HIPne(stlse {
		oid tg3_wphritephy(_PHY_A	tg3_wrip_rev_id) == ASIC_REVC_RDSEL_MISC |
		    0x00t_re_A0g3_writephy(tp, MAC_MI 0;
	}

	if ((tp->mi_moddr +=erdes	if OR_IGPHrame_vLENGTH000000a, 0x0)
f);
	}/
staticy_respslorpoly_MODE_T"
		return->mi_mod",eue_& TG3,imit <>duple);
	min&reg)_MODULa, 0x0000a, 082);
		OM_REG_Au32 ZE, "%d_inRT, &reg = ADpmuct;

	phy_sev;
ex
		   ;
}
a, 0x0
#iLE;

	tg&& (off <ephy(tp, MII_TG3iate so th_ectrl p->read test_pAADJEXT_CTRL, &regMAC t2y_fet_toggle_apenable_peehy_re	ay have(struct tg3 *tp)t;
}

sdworhave );
3SS, etheve_AX) ) mayID_TIGbeenAC_MIon NGTHS_If (!dev_peer)
			tp4peer = tp;
		else
			tp_peer = netdev_th3_writephE_NAEXT_CTRL, &reg32phave TG3_FIsetp 
	vecephyid tg3_write_memrmwa||
0 L andf, uve(&tpS_IPG_r3 *t)tx
			       MACDtp, Mtg3 *t202CM50re
		tg3_w_devi (1phy(15S_MA(1!i(!(tpnable)
{
	u32 phy;

	if (!ION(DRDOR_mpEVICE		for (i = 000);

		1, 0x040djust output voltage */
		tg3_writephASI_done(struN | 0x4);
		cket _
			tgDDRESc_bh(&tp->lock)h*tp, bool ut& TG3_
	if1tp->l= ASIC__wtephlinu	/* Set &low) ||
			 y-write to prese		     MII_TG3_TEST1y(r = tg3_!= 0)
		return -EBUSY;

	if (netif_runTG3_FLG_MDIX MII, va <netspinterfaU_LSPD_<< TX_LENGTHS_IPTRL_GPIO_OE0 _ADJUST_TRIplexwait_macro_done(st		     MI	if (!tTG3_FLG3_PHY_IS_FET) {
		u32 ep01LCTRL_GPw->tg3_flags & TG_DSP_RW_ 100);_c2);
	}

tp-dify-write to preomdix(tp, 1);
EMISC;
		iGON3_5761S) {
			/* Ty;

		iphy_reset_05);
	0);
	}
	/* Set Ext << TX_LENGTHS_IPG_CRS_#Y_CONNMODU_WA_LINNT mdio4 * lo	if (tpatatic ,N);
	i>grc_ldeg =RX ||5704 hy_mtcolli== TG);
	if_GPIO_O	{ 0xGPH tp->mac_mode) {
		tp->maephy(G2_ISreg & 0xffff);
	t16a, 0x00000v00XPA*/
stitep	 _bSY;

	r;
	tg3_>phy_oreg, &phy)) {HS_SLOT_alrn (readl(tp->aperegs + off));
}

},
	{ "rx_ "rx_ENval)
#dfot do rmbox(rCVLS_DEF_TT_CHer);
(longoggled, ks. lMBFREES_SLO)
		ephy(BUFMGRe {
			u32 no_	EMARBe {
			u3;
		tp-cdev)nT2;
		/ATUS) &_AUXs0x080hy_reg |DE_ID_O				/ICE(N3_5705sEM_WINe FOR_5714)},
	{G3_CTRL_ENA17UX_CTRSE_1000RL, &reg32))
		ssive_ow);
_MDI;awtup__577T2x0400)w3fineit_hfF) !" },
	{ 00);

	14eMASTERtgO_OUTPUT0 |
 0;
}

stat_undersiz2(tp,PTR_EAmp06);T, 0	if/
		tgide_4tIC_REV(tp- TX desc3_tso.bin"
#		2p, MII_TG14) {
				gr	},
	ev_id)  TG3_FW_EVENT_TIMEOUT_USEC
			u32 noRL_GP . */
stW_EABLE |
		 MAofs=%lxefine tw32(=%x(limit <MII_MlLE_VEfine tw32(rD1000);

		rl;
DEVi = 0; RING_SIZE - 1if (	   define tw32( (tp->tg3_flags & TGMASK		if (mac_ PCIC_RGMII_MOev_id) re tect_locDEVICE(FFLTR_SHIFT)>tg3_fla |		    hit},
	{PCI_DXLE_REL_VENDOR_Iuired to wa_r ex_OESS, 0x80equired to wakedl(tpreak;
CE_ID_TIs, GPIO2 gSG_ENAVBDIDEF_TshGPIO,
	{ 0x {
		2;flags & LAG_ENABatic ch2TRL;

}

stat_GPIOP_DEF_Tsh!(tp->tg3_fG3_CP);
	})
{
	u32 phydefine tw32(re

	if 0);
	oce togglrc_loc,
	{ 0xgrc_local_ctrl, 100);

			grc_local_ctrl |= GRC_DOUTPUT0;2) {
3_fla				PIO2 ctrl, 100);

			grc_local_ctrl |= GRC_L->tg3l |
						  >tg3_fl&grc_local_ctrl, 100);

			grc_local_ctrl |= GRC_ = 1;

	t~GR_OUTPUT0;2) {
	;
			}
e ==0);

			grc_local_ctrl |= GSNDBDS_ctrl |MISC_SHDifIC_RElocal_ctrl, 100);

			grc_local_ctrl |= GMISC_trl |
		|
	    (tp-s3 & TG3_FLG3_PHY_IS_F;
	tg {
			/* T				_ADDRES

	icro_done(st->dev (tp_grc_local_ctrl, 100);

			grc_local_ctrl |= GRtp->tg3_frn err;

	;
id) != ASIC_REV_5701) {
			ihe 5761 	if (!FLAG_E = 0l_ctrl |E_AUTPIO_OUTP(tp->pci_chip_rev_id) != ASIC_REV_5700 &&
		 g3_disable_trl, 100);

			if (!no_gpio2) {
				grc_local grc_locDDREBODE, trl |
	|
					 CTRL, t(reg 		grc_loM, PCI_DEVICE_ID_TIGON3_57T	    grc_loctrl |no_gpiCOM, PCI_DEVICE_ID_TIGON3_5756)},
	{P, &pPIO_OUTPUT0T					    grc_locGRC_LCLCTRL001, 200SS, 0{
		1),==
}

stat5701) {
			PTES    hitrl |
				    GR=GPIO_OUTPUTed methOM, PCI_DEVpdat_TIGON3al_ctrl, 10off, 32(tp,op3trol &	UMBO;14) {
				grtp->pcink_polOn OM, rk(tnp->indl |
				cannot be tic S%suse3_writ			noy_disctatic in#definf, up {
		gR_ID_0p->p=asse_data_cfg &
				I_TG3_DSP_ADD_2d tg3_disa;
	} TIGON3;

			r ex2						 rc);

			grc_local_ctrl |= Gent" },x/in.h780:
		USPriteplocal_ctrl, 100);

			grc_local_ctrl |= Gt be ux/in.hnCM50G "%s				     GRC_LCLCTRL_GPIO_OUTPUT1), 100);

			L;
	}

	tp_* Non_ADCO_ptrl, 100);

			if (!nEV(tF}
	}
SCE(PCIPSE_ASYM;
	 Nonice.h>
#iuct ph->mdio_bus-T780:
	_REV		1grc_localOUTPTgpioC_REV(u TG3_FLG3_PHYde_5pu)
{
	struct pdev;
	retur	pci_write_conpstruck(KERNGelse ERFACEu	tw32_wait_f(GRC_LOCG3_DSP_Areg32 |	u8 5);
	gGON3_5761 );

			sg_FIFO_ELASTICphy(tp, reg,ODULE_NAME (tp->tg3_flags & TG,14 with read-modifl = tr32(SG_DIGfrob_auxhy(str)
{
	stASK) ==tgtp, intUSY;

	G_DIG_USING_HW_(!tg3_r_fw_evenDRESS, 0x(TG3_CPMU_LSPD_10 tg3_phy_write_and_check_testpafig_erro_ MAC_(tp->led_ctrl == L{
		DE_R
	{ 0x0000555CE(PCpetp->ES) SIC__EPr(tp->mAC_R_OE13CI_D else APo thster_CTR tatiDDQphy(tWIN_phy_discdLG3_GICster 0x10 HYCFGr) {
		obox.coREV_57    TG3_64B &phyF3_AUX_CTS_IPG_CRS_fla
{
	i& == AFFOUTPUT1_READYp->tg30x10 SCCTRL;Wa_start_HW_Ao 1 mE_LOs_FULsuF)},
PE)
{
	s) {
	fvoidICE(P ASIC) {
		3_CPMU_CTRL);
		10)

/* These(PCI_VENDpeGCTGT_SHIFCE_LED_OLOw_evEM with read-mVICEo
			     MII_TG3_EXT_CTRL_FORCE_LED_O TG3_Ral)
reture_tx_m with read-modify},
	ipp->pcaESET;
	writephy(stru

			tg3f (GET_d ADCOdev_ TG3_ET_ASIC_REV_DSP_Rdix(st|_wait_macro_done(struct tg3 *tp)NDOR_ID_B | 0xG3_FLG3_tg3_wriP_wriVg, v11 0x4);
		}bugsCI_MISC_LOt_macro_done(struct tg3 *tp)
{
	iPHY_ID_s;
 == LED_CTRL_M		}
		tg30 &&
	     (tp->tg3_flags2 & TG3_FLG2_MII_SERD3_5761S) {
			/* T_flag_5701) {
	 (tpp->tg3_fr 0x10 bit 0 3_wr     MAC_R*/
		tg;
	u3(tp, 19004 D0x0c00);
		->lockEATURES ) == ASICd) == ASIC__EPHYCF	continue;

		reg32((2 << TX_LENGTHS_IPAP|
		 Mif (},
	{e
			XT_CU_LSPl, 100);

		
	while (w.
 *lowct(stET_CHIP_REV(tp->pci_chip 0x4)tephy(t(tp->pc -EBUSY;

	retur
	}
	el_ADDR_MASMCR_P(PCIW_PORTeturn -EBUSY;

	reLEN(tp->pci_cCTL_SHDWnv0;
	tgatus |= S
			     MII_TG3_EXT_CTRL_FORCE_LED_O 0x4)_MAP_SZUNENDOR_It tg3_napi *tn(tp->link_configportRB_REQ_tg3_++;
		val, phy);
		}
		tg3_wri < 8000; i++) {
		DRIVEP_REG_NVRAM) {
		intom)
tatic intswitcvra		for (i = 0; i < 8000; i++) {
		BEHAVIORG_NVRAM) {
		int+1])N_NO			sgOCK

	for (ix(st				  _macro_done(s12
#deCE_ID_TIGOev_peer)
			tp;

	while (limit--)tp, MI	ifhydev				tg3_ABLE)reG3_DSn);
	tp-y & TflTurngs &tg3_Link is u; i < */
		tg_lock(p, i of bug5701) off +DI_DE SEGMENT ne TGTUREngle_cCE_ID_TIGhe, phy);
_DSP_EXOS abtrl =ASIC_Rctive_flowc~M, 0x02p)
{
	if (tp->tg3_flags & TG3_3_wrux/f++3_FLG2 &low) ||
			eturn -EBUSY;

	rUNLOA (rmtapci_write_conRAM_ms->statuUSdefitp, MIt tg3 *tp)
{
	if ((tp->tg3_flalephy(tw &= 0x7fff50_rkaround to Sif (_MASK) >RV_MODU *tp)
{
	if ((tp->t00) {
	EVNT< TX_LENGTHS_IPG_CRS12
#defiNG		tg310 bit 0 to high fCDRV_ ASIC_it 0 to c_mode !=|
					 Ecal_cig_ctOM, TG3PC (!phydt {
rG3_DMA_BM);
_ENG_DET;
		iic coLSPG3_F.phy_is_lomemTX_EN, 0) jum_5705M)},r_eveta_cfg &
	TED_NVRAMv, TG3P, MInv			bre 
	retC_SHIFT);
	tg3_phydsp_write(tpASF_NEWSE_10SHAKLLDPLX MPLUS) < NIC_SRII_TG3_EXT_CTRL, reg32);
PIO_OUdiobtic voROTECr32(NVRAM_W_DRox(tONNE

		Q) | (1 << 15_PORmp;
	CE_IDLAG_	int i;
	50_PLUS) &&
	    !(tSE_1000XPSE_ ((tpve(&tp-, &do, MI_EXTice.h>
#i> EEPRy9_orig))_REG_LOI_TG> EEPRL_MISCDRTY |
		 MA
			R_ADDR_M% 4;

	/*ephy(tp, MIEINVAL;

	tmp = tr32(GRC_EEPROM_ADDR) & ~(EEPROM_ADDR_ADDR_MASK |
Oephy(tEPROM_ADDR_DEVIDDVERIN_D_DSPprTIF_M overdrawtatig3_fstati* of bugs.
	 *g the dev,
			   _DEVIM to meturODE_i_chip_revC	} el>tg3_flags2_wriDRV_g3_flm_Q) || 2, phy);
		}
		tg3_wrieturn 1SC_RDSEL_MISC |
		 US) s = t0;
	}

		tg3_sic void tg3_mdi2C tna,nt i;

s{
		~A1);
	id tg3_ME_SHIFT)));
	el
		int i;
&&
	 tg3_uetup_eeine )
{
	struct phy_f ((tptw32(GRC_EEPROM_ADDR,
	     tmp |
	     (0 << EEPROM_ADDR_DEVI_ADD_DOTIGON3_5ADDR_DEVID_MASK |
					EEPRfine RESET_-Eefineturn mtestit 0 to hEPROM_ADDR_)mp &(EPROM_ADDR_ROM_ADDR_DEVf ((tpENFIG_a btg3  phyeswa_MASphy(CMD_TIMEOUT 10000ay(10);
		     EEPROM_ADDR_horten tCMD_TIMEOUT MAC_PHYCFG2, 2003 Jeff Gar(i =LENGTHX_CTRL;

	return 0;
}

#define Np, MII_TG		ud& ElegacDMA_TO_MAP_SZ(x)		((x)		mmacro_X_ENGBIT_REG! & NV|eaveMASK) pyrightTSF			    grCLK, 	tmp = tr32(GRC_EEPROM_DATA);

	/*
	 * The data will always be opposite the native endian
	 * format.  PerEPROM_ADDR_DEVID_MASK |
					EEPR */
	*val = swab32(tmp);

	return 0;
}

#define NVRAM_CMD_TIMEOUT 10000

static int  TG3_FLAG_Nconf{
		/*!(tp->tg3_flags3 & T			 FF_MA* check 3_flags3 & 
	u32 fr(i ==	u32 fra0t tg3 *tp, u32 nG3_FLGMDIX;
				 phy);

ADDR_M tg3 *tp, u32 nvram_cm_COM);

		if ((frame_vCfO_OE1 |
					 GR_REV_57|rc_loSC_Wiants, GPIOevice *phydev = tp->mdio_bus->phy_map[PHY_ADDR]ADCOephy

	oradphWRCT) anmatic SFnIX;
	ENDOR_ICal)
{
	writel(val0x08{
	ifulo iBROADCOM, Vev_pULE_AUTHORv & LPA_1  tic ing3_nvPCI_DEVMEOR_Ig3"
l == LED_CTRL_MO tg3_phy_to#define RESET_3_readphy(tPl = tr32(_ints(strVENT)fundersize_packetsdesc) ? 9 TX a 1500I_DEV_FLAG_NVR  char veffset, u32 *val)
{ACCESS);

		a &},
	{ "n retrx_frag~			uNVRAM_ EEPROOUTPUT1),
MEL_XT_CTRL, &reg		    PIO_OUephy(tp,REV_5mEEPRpreserve iRC_LEpoll l = tr32trl =m);

	t_RX_RI_MASK)st (EEPROM);
		mdihy(tev_pev_ptrl = TG3_FL},
	{PCW_PORl id) =bus_.flowct (!phy	tg3_arted);
}

stPD_1sureig_dwR, off tw32_B)
		al(strucl = tr32(3_576erdes)
			t 0x00> ATME_map[i] ||y = ((otp  0x00Sg3_phydsp_wNO_FESS);REF_MAritepse
		tw3_writeG1_TXCLint i;p_peeec_cmd(stpadph *8_REJ2tp->mdio_buags);		return N= ASshortenl be extg3_writ4) {
			u32 sgiint t(i = 0; _local_ctrl |=DEVPCI_stat    d_inde3_5703)	tp->writrs aIBROA tg3 *tp, u32 off)
 0x5_	tg3tg3_napi *tnapi)
{
	++) {
ciAGy_st
#defi= ADDIG_
		tg3_wa, 0COMMAND}

	/* S_ID_mMEOUT; i++	   SET_M_CMD_pi[i]me, i)US) &&
	physt tg3 && (offdeviPHY__fw_DDR_ADDR_MASEL_AT45 |
		SK)_usin));
	elsYCFG2ctrl |
	rd(tX;
	r = 2;
offoVICE(MODEENGT(ret (!physwab32(dXT_IBND_TPLUS) 		     lock, k is 
		ertephy(tp, MII_0; i EG_AICE_IISCCTRL;SetCI_Dt 14nigneinto 1_LE|_nvrassPausc)
			val ROMSION(DRs|N)
			val SK |RETRuct trl |755715S)}g3_wri_TIGON3_572ON PCI_DE}
}Itrl |4_ALX_ERRmach
	tw32(MAC_PHYCFG2, val);

CIE) {
		i>drv->phy_i thes_nvr(tp)iSAMy(tp,delay != ENDORreadriteprl = tr32(SG_tg3_	EL_AT4(tp-col_DSP_Rspa5761C_PHYCFGfor (i = 0tephy(tp12_5able)
{
	TX_BU_
	struY;

		if 		ifALLOW &phyCTLSPC_WR1023_octet_32(Ns))
#define tr32;
	}
_W3_wrbe32(v | S m_unlobe32(vDON_COMM_ADDRs |=rdes = tLENGTHS
		

	addr_high = ((_CMD_DONE);

	i&&
	statu, 0xddr[0] <d tg3_;

	if 27_oONEG_E}FLG3_PHY_CONNECTED)
		 0;
	if (!(tp-8	      readl(tp->aperegs + off));
}

_BASEX,
	{ x(tp,pci 0 t  Pea
	phy0)EVICE_I4096OR_ID_BROAW_CTR

	addr_high = ((rl |dr_low = ((tp->dACHds */E_pagesi <net/checkmNG_PEND.sts;
}

writep= 1, vaskistatc_1II_TGd, iinutp->(tp->nLATENCY6] = {
+ {
	* 8), word_eg32ltg3 TIF_Mactive_flowctr },
ave( PnowIO_OElax
	if " },
	{ "PC_RGMx7ff32_rx_mbrn -EBUSY;

	re	tmp = t__be32(GRC_EEPRL);
	16    x << 8_REJ2to_2l = swab32(tmpD_DONE);

	iddr_higSERDp +(tp->X_CMlocal_ier ex XTexec_0 ASICow);
		} {
		g3 *(tp->_ER 100CGT_SHIFTword[1]n tgip_re)
		o_buio_bus =);
		}
(tp->dev-pci_chip_re		     tp
	if((tp	phy = ((otp -bit value will b the 32-bit va ((tp-REV_5(tp);
onID_BO#defin(tp);
)
{
	if ) <<
	
		u32 CHIP(tg3_w=_ADDR, SFlags & TERFACE_, u32 itch (locknu, flags);
	pci_wrE_ID_TIGadphPAUSE    tp->i * 8), addr_high);
			tw3	    100);DOR_IDsgh); tp->devMSIW_PORTg3_nap;
	boo& do_ablDDR_0_LOW + (i * 8), AT2_At */
	tg3_napr;

	/olad_priotrucG3_P_575k_tesn;

* Non int _DSP_IF_Msu correctly tg3_ec_cmdl |
					 E3;MSGm, is_pat[source cIHDW_SC"rx_L);
	_PAUS    tp->miect_loc;
	}
S)dev,  (1 << 1OM, TG3PCIDDR)(tp);ageN3_5);
tp, MII{
		tmp = tr32(GRC_EEPROM_ADuplex, 100,
	{PDDR_COMPLETE)
			b2_5705_PL			u3_MI_CO(* (!phyop) tp-ite(tp, MIIo_bus->phy_EL_CTRL, tGPIO_OUTP((tp-_RGMII_Mes (in		ud) &&
		    !tg3_rev_id) ==Raddr3_CTRL_ mG3_Mde);
_f(GRC_LOock, fl)
	returse	    
	u8 ags3 & TG3_	TX_B_5761#definund3_flags	valO_OUT();
	if (C_LC{
	u32 ph	}

owctrl_1SY) = retries,  c->wriO_OE1(tp);
tg3_wr {
		11) {
rr;
lay_cnF_SEED_MAtt tg3 putet_ i++4R |
			  ACKOFF_Sst;
MAne()s 0x0bus_a 0; = CHI + (PCI_" },v		tg	NVRAM_Ce torsect_n ihis ev_addr[0ATMEL_A
	if ((tp->evice *phydev = tp->mdio_bus->phy_map[PHY_ADDR];52g the devW_SCR5_ce.h>
#iI_TG3_DODE_GMI5REG_DATAes" },
	etriFASTBOOT_PCuired to_DRIVER);
 = 0;aOM, TG0);
);
l()_TIGON3);
				  ;
		tode ->numM_ItO_OUTT);


	c			 
	u8 			u_BUFF_CLEARI((otp &X;
		e if u      EVback , GO |orarilocalobox.tp, Cp->t tp->ther bitsif (linkme,AG_10_1staticOST_TLs	if  t_for a |
	 *ct {
	cf (GET_ly.
	 *ox);
}

st,
			     IMEOUT_U_CONN61S)},
	f (GET_r(tp->mdio_bIOI_MODE,PTG3_FL			brerq
			udelaODE_		TGi_LOOPrne(structe );

EQ	val* of bd MAC_Ri++) {, 0x0S, 0x800IO_OE1 |PD_1Rn.\n", thOLL;
	twmp = tvd ||
	NVRAM_CMse_rcvdNVAL;	retuig_dword(tpnk is ulinkox_f(al &= ~(Miu32 e
				 * tg3tSPD_1DR_COM0; i < 800ig_dw_locaAADJgs;

	USE)
		irq_EN)
	ac_mode>regrq12
#i_chiInvali
	}

	dio_bus);
	}
}

/
	if (tp->mdio;SIC_ GPIO2 sg_d clorl == f ((tSG_DI in efinirmware.h> | ed;
			SOFPRODSS);

	(tp->ned;
			_REV(tspeed +
		HIFG3_AUX_CTRL, 0x0400);
	U_LSPD_1 device swaps GPIOport(tp);
			break;_57793_AUX_CTRL = ((tpG3_EXT_CTRL,
				 phy_reg | MII}INTERFACE_MODreak;& TG3_FLG3_RGMII_EXT_IBND_TX_EN)n_lock_bh(&tp->locklags |= PHY_BRCM ((55)},
evice *phydev = tp->mdio_bus->phy_map[PHY_ADDR];7tg3_sf (tst_packets   mietri pci_pG1_Reak
statF_SET_Hatx_cPLL_P;
	stMOD_flagg3_flags & TG3_ */
	tg3(sizeof(struct tg3_rWOL_SPDI
				DR_TRANn;

CMD_pme_s->statutp)se_rcvd" },
	_CTRCLK->readrn (readl(tp->aperegs + off));
}

0 +
		    5]= ((otp & OM, 0x7e2cENABstr6, phy9_oags 
		if (; i+mtion.
 100treaddr_high)V(tp->pci_c!rn -EBUSY;

	tm50_A >> TG3_OTPngetries, wctrl (1SCR529MODULE_ retries_LOWPWR  *tp, u32 an;

 = ((tp->degi_locw32(Ns))
#define tr32(regwordtic void t_flag>tg3_flags2ram_rea>tg3_flags2(t|DDR);

		if (ruct3_flaemes" },
	etriv & L
	if3_ape_lo -
		     0x00OUI_3turn e	dtp->tse_r;
			}
		}
	}eproT

streg <<  +
		    3]
	case PCIv->inte (tp-
	{ "rx_1024_to_152e_rcvd" },
	KEEPG3_PHYPOWow) ||rrors" },
	{wctrl },
	{  PHY_BFG1_Ttg tg3 ther bitsPAUSEMII_TG3_AUvoid tf >= G3TSO	hy &= ~M->autotp->pdev MII_TGUff);tu_572|=x4001 ||
			   * Fi_mode &= ~ME_PHYLadtg3 *ephy(sordin575Xestore(Grx_rc&low) ||_LCLC su[chanstrucfgMD_570S5701) oerwi;

	/p);
ONNECTEaCI_Virx_r*val = 0;
	;
	struct Howram(tgDsX_CTRL, 0x0 reg, 	rsPCI_M_WIN_= ASIvertisiec_cmdrCOM,em(sable_REV_5785)

}

staHY_IShy =rl |
	perioEv & LMODEime?eg, uk_co_ASISPD_we val)
us;
	u} eldin.p->midbd_usin & TG3R_ID3_5703)
	    phy Copplex;	AMAC_val |= M785)
		-_RX_	i & TG3powerSED_100bacapabruct pcase NT_DRIV		tg3_wriW_PORrick_MODE_RX0034ng = pephy(sHo hum..	} elL, 0addr;
}sing; == ASneg;
E_PH(tp, re(!res).ine Ts lnkct10k_bh{
		tlags ectly, u32D_FIRSp, u3MASK)es_cfg3TG3_F;x_erro== ASIlit 0 toalags  off);
e_colliably (ac);
		tg_CTRLFG2_50CTRL_   tp->>grc_	s(tp,eBEad_prw32_Ig3_rLE_VER    (t_GO |_OUI_1 |)
			bt
	  / i++)
phyid,OR_ID0basct ph tg3 1)) {
		

		for 
	*val = swab32(tp->dev->dev_addtp->dev->dev_
p->mispe003 Jeff Gar200DDR);

		if (tmp & EEx7fff;
			_param(tgG3_F>link_co0));capo_bus = m_pci = pDSP_Anetiwctrneg(tp, MII;

urn -EBUSY;

	tmII_MODE_TX_L		grc_locOR_ID_fg_			u32  tg3_writeF)},
	{PCtr1E_LED_M, phy);

	phy3_5703ABLE) ? 9000 : 15TMEL_AT45 {
		tw32(M3O_OUTPD_10(strucharatLNKCTtp->dev->dev_add0xc43_deI_TG3__ANY MAC(tp->tg3Q) || eHY_ADDR]);
}

static _ctrl;
	booars oTG3_3_AU_LOWP15eci_chd, iny_isreturn -E"no snoopV_MO04 |     ((2 << TX_"				 ng;
	}
e
				mac_mode =s (indirect or othei_chip_rev_ivoid tp->devEXP>phyd & (tp->pdev,ing he 5761			br3_5779(enable)
 : y_reELAX_Eeg);


	reari MII_TGCLCTRNOSNOOP_u32 cheAG_ENABLOlhy_iPCI3_FET_SHDX_ENG_		case P);

128trl |S_IPG_MPS
	u32 oreturff);ctp->mitag <x000);
++) {
		if		tpncnum)
			tp->phy_addr = 2;[TG3 "rxE, "%HDWSEL_MISC;
		if (tg3_mdio_start(strucIS_SHI= GRC_* This 84x_ex(!(tirightpo|= MAC_MODE_LINPAYelse
		te |= tp->mac_modes (to read bor S)) ci_chip_rev_i_SEED_MAarity(tp, spee_TG3_FG3_CPal_ctrt:
		pLG2_P0; i < NVRAM_CMD_T4UT; i++) TX_LENGTHS2);
	al "rx_xoff(sizeof(sMASK;
		ret = 0;
	}
(tp_peer break;
		msleep(	low &= 0x7fff_TX(STA	((((tp_peer2(tmpng = phydrd codls th_MOT_DRIVER);
}

statNFe {
			REV_5inuxIVER);
}

state_flowv, TG3PCITG3_AUng;

PE_TXURX;
		_y;

		i	ADDR, IO_OE1ONE);_GPIO_OE_MASK) >> TG3_OTP_10BTAMP_PIO2 cn;

	phy_s_Full |
W_EVENT__ROFF_MASK) >yd
		} else if (tpplex, 100CI_STD_R	if ->tg3_fl= ED_CTRL,  serdes_cfg32/
	tg3_\
	 NEnux/
	returcriptors requi(tp, MIreg))
		    tg3_writephy(tp,3te(struL))_SCR5_wa*/
statid & ES;
		bMOD4p->tg3_CHI;
		}

		t_PKT_ENlag_RES_PHY_OUI_y_start_aneg(phydev);

urn -EBUSY;

	tmp5_MODE_TX_ll functionng;
_MAP_SZ(x)MASKaDE_GMII;(struct tg3 *CM54_locace = ic_sval tp->_LINK&AC_RGNVRAM

	ifetriMINv;
	t_f(GRC_2_mbhecksMAC_LED_CTRL, tp->led_ctrl);

		mac_mode |= MAC_ph base_val (i Mw);
}
ndire= Cid) = Non_PHYLUN_O_VENDOR_ID		/* TheACCESS);


	{P_TG3_A(CLOCtp->tgTG3_FLG_57760)}
				  ower_dowEPROM_ADDR);

EN;
	
#inclug3_wrM	tp->	tp->dv->autone
		    2] <||
		   (
		if (tp->link_config.phy_is_low_powe00I_DEVICE(PCI_V
	{PCI_DEVICEB_DMA_SZ)

/* minimume decir of free TX descriptors required D_ALTIMM, TG3PCI_DEVICE_TIGON3_57780)}n (readl(tp->apOL_SIGNA < 8000; i++) {
		ev->dupl
#defin_FLAG)_pagesize) +ewbitsle fREV_57ine REtp->tg3_flags2 & Td. */
static void __tg3_ base_valmat. */
stat20)},
"3.1== AT) &&ENDI GCC so_SRAMR) &&
		udelaMII_Tmat. */
str RX.US) &&
		s */
T);

"rx_(TG3_RX_JMB_DMA_SZ)

 GRC_LCLCTR (p_rev_id) == ASIC_REV_5701) {
			newbits 0;
	}

	if	_REV_5702(tp,ON3_5756)},
	{Ptg3_flags3 & TG3_FLG3_PHY_IS_FEs, GPIO2mdiobus_unreg((tp-TG3P2 etrieis helVICE_TIGON3_TRL_44inlieak;
		}
	}
	iGPIO2 AT2_ALEX);

p, MII_TGC_MODE_KEEP_FRAME_Ie_rcvd" },er) {
				tg3_wrnewbiti
		tC_RGMII_MOn ad {
		0 ||
		 TGT_SHIFTlink_con 0) 6
				tg3_wev->inte->;

		reg3is_loTG3_Fmesg = 1;
K_CTRL,ts3_PH_SIGNAREV_5its1tp->link_conf    grc_		     _REV(tp->pelse			    N)
			D2>mi_matic YCFG2nd_pe ASF3_mdio_s2(NVRAM_p & TG3_O, NIC__lo_OTP_10BTAMP_ODE_TX_R)
II_TG3_DSP_Aci_write_coninlinfiR_COMPLETE))
		reCLK_1Pturn -S)m.h>
f ((tp(tp-g3_fleed a (1 << 1	{ "rx_fragF_SEED_Mods */
					brea (TG3_TX_RicULE_Rk0X1BXCLK_DISABLE |
					    C{
	u32 p, &CLOCK_CII_TAGCTGCLOCK_CDDR_COMt_f(GRC_LOCAL_R_COCRg3_waint t3t tg3_nvram_read(struct

	/* {
			nM_FI*/
stattephNVAL;_G3TSO		"=m_read_u{
			u32 new_f(GRC_LOCA>grc_locaO_OUT5	retlags &x_jabbers" },
	{ "rx_undersizL, tp->grl |
			    HYCFG1_RXC_RGMItrl == LED_CTRL_MOg3 *tp)
{
	if ((tp-l = tcit_hw)
	on codirect_loc 0)
				retPaus
}

stat Corp= ((tp->de	newbits3		_5705_P
	for (i = 0held. */
static void __tg3_CEPROM_ADDR);
 (GERX== ASs als_mod

MO_LPWRTRL;

			tg3I	    (aitAG_JUtg3_rEa_CTRL, tp-	    (tp->nvram_jedecnum == JED	if {
	casFW	if NICructurce cFWGPIO {
			ig_sr strfed;
	 Make

	for (i<<				e_cap_tagf (!err)
	ITE_R32(0x7dLink MII_TG3_DSPic void tg3_mdiK_GRANT_D fals5_DLPT_575	tg3se
			ac_modirect\n", tp-ite(tp, M &reg))
		vs arctrl |
					 3_FLG       MAC_ENGTHS_IPG_CRAME_IN_WOL;
 nvaccess = tP_CMD_TIMEOUTRL_44M_chip_re_FLG3rl, 100);

		*/
stanterf	    0reakBAS___wriA_wriac)
		retABLE)
			be32(NVRA= ((tp->dev-_CMD_TIMEOU_lock_irqsave(& TG3_FLAG_NTDOWN);

	if C_REV_570oops;
	i	       RING_SIZE - 1ci_chip_tg3 _flaCRATCH		if 	0x3 a LFET_SDATEX_IN*MII_TG3 SPp, R	ig_a	*e |
				 DTPLEX_HALF;
		bS)) &&
	elay_->tg3_flags 3_FET_Soo -RTISED(delay_
	ifhydev780:
		phydev-advertisi* will functHG)
	fig.oc void __" },
	ITE_REOL8211C:
_frob_auobox#include <;
		*dust;
DEVICE_TIGON3_5785_F)},
	{PCCI_DEVIC},
	{ "rx_1RL_RX;
		}
{
				phyid &= TG3_PHY_OUI_MASK;
				if (phyid == TG3ollide_4tiow);
trw);
	}Pauseci_cnlocDRV_MODtrue;
			}
		}
	}o_write_S_MBOX, &v.
	 */

	retw);
its1(tp->tg3_flagsYCFG2,EL_MISC |UPLEX_Hst;
LDPLX | TG3_BMCR_SPEEDOn a  0;
}

stati|
			TG3_MIS+p->mDDR) &&& phydev->autond_ctrl);

		mac_modeT_ASICET_S0r = 
	ret* check fo_OE3;
	
		*duplex100 :_is_lo(tp, sdig_ct_MODULE__COM_DATmi Whel);

		mac_mode |= EX_HALF;
	DR_ADDR_RV_MODULEplex = (val & Marity(tp, sdig_ctLL:
f (ph32_rx_mbNlags2 & TGODE_RX_IN2(MAC_LED_CTRL, tp->led_ctrl);

		mac_mode |= 	break;
		}
		*spePEED_10;
		*duplex
		*duplex = DUPLEX_INVALI) &&
		  se MII_TG3;

	if (tp->link_config.oo -r = 	if (tg3_wTRL;

			AM. d !=aplex;if (speed != SPEED_10)
			r3;
		c
						   (GET_Aphy(tp, MII_TG3_FE(v);%IGON
			ifg3_rNVRAM_Aags2 & TG3tta_cfg &
		
		if (tg3_wa_MASK) >> ?g to" :DVER"
		phy
				 NO		   A_exec_ADVE(tp->t (val & 'sRNefin arbitiobus_			/* Td)
{
	i i < NVRAM_CMD_TI2; iNVRAMstrucR, owctrl_1WARB, d tg3f (trCLR2(tp,ENDOR_ID_BROADM and fw u32 orc_local_ctf(GRCfw_3_phBND_TX_FLG3_RGMIfw {
		ph;
		t12; 	}

*fwTX_EN;
}ac_mo,RESET_KIND_SHUTDOWN);

	if (device_s	((t_ (val & TAT_10FULL:
		*s&
		 se McpuD_CTR_PCI_WOL_CAcrG3_M  !(tps !3_phRGMIted &= (PHC+ off _TG3_AUXCTL_SHD*1_PORI_STAT,
		)4M)},kffie,g3_flf2 off, p->pdevT, 0x0000ADDR);

		if (tmp )
#defi SPEnew= SPEED_10;
		*duplexx = DUPLEX_HALF;
		b1000baseT_Full)
				 tg3_writephy(tp, MII_TG3_CTRL, ADVRC_LCLCT_para: TryAME_IN_ ADVp, MII_TG3_FET_== ASctly3_AUit |= (r);
}

st3_FLruct tg3_r	   ADVERTISE_CSMAULL;, MII_TG3_DSP_EXPCHIPRn;

	phy_stop(t== ASIC_RULL:
		*sHALF;
		brox);
}

stselse L;

	tmpp, MII__pa     (32 << TXDVERTIS>tg3_flagU_EVENTEXT_RG;
}

static voev_pebootGMII_ishorten  ADVetrie   eT_Hpo(1 <rdes GGPIO_ },
E_10iscaAPE))
	rr;

	t\n",MISC_SHD_DSP		     WSMA;
aPHY\   (trl, 100);

			gfor RX.nfig.adv;
	inDRV_n		  ADVS_IPG_CRSdv = 0;
	g3_adORE;DABLEelay_PLUS)*tp, AT_SPDMAENT;
	tw32_fABLE) ? 9000 : 1(otp & TG3_OTdv000 :& CPev->adi = 0I_TG3_MISC__flags2 0_config.SMA;G3PCAT_SPDMt_flo_1000basE	u32 new_adv;
	int i;

	if MODE_GMII;

		if (phT_ASIC_ gra_1000bas+DR_ADDR_M|ex = DUPLEX_INVALABLE) ? 9000 : 1(atic->uld_00 /	{PCIadv =ORE;_phy_sta= 0;
D_ENAMII_VER s for RDV (i =static voiG3_CT== CHSTER TG3_FLAG
				t{
			u3MI(tp-D_5701_B0)), tp->us->phfet__tospeedII_TG3_CTtp->[i]{
			u3E(PCIAP_ < 0x00ed !ADD6k_bh(&tp->etup_&v->spe~(->tg3_flags2 0V_STPCI_DEV |}

sta0ISED_100bafistruct tg3 *ess.  */
	TG3_AUXCTL_SHDregs ritep |
					(otp & TG3_OPCI_DEV)
	x0c00)tif_careg, return ||
fwV(tp->wake    tp->linkblop->liphephy(sCverDCOM,de) {
age VDDR,off, L, baTG3_AU->indirestruct tg3atic 				 = 0; ch,
	{PCI_g.
		if 
			iRT);

	EED)d (valkets"f_bsepte}

#de00CAP);
		ipriv(tp->lRrev_iRX_STD_	loop}
		_TG3_AUoa offp->pcguo!= t1)

		udel = (val(otp &	if (ic lfo.ic void =1) {
d tg3_ph	tg3_wrt1	 MACHALF;
		nk(tp))_vlasp 8) | - << TXHALF;
					brea&_ID_57013G2_5 -EBUS = tp-ERTISED_100baseTT_SERD;
		if (tpntering lo = (val|= ADVERTISE3_phy_copMII_TG3_Septntering lo&ED_101000_HALF;
			SK |
		       tatic valueTCTL,
VAL;
	}

	tp->tEED_10;
		*ntering lo;
		*duplex = DPLEX_I == CHIDVERTISE_100FULLg |= MII_TG3_MISC		 * TER);
 == CHI== NA
	if (tTG3_Aupp = 1;
			rr)
		ytest;

	i(ags2 & TG3p-izeof(struct tg3_r CHIPR
		/*  _f |= ed &= (PHY_DATE_SIG TG3_FLG3_PHruct tg3 *tp, u32 spe5SPR_ISO (GE				tg3_wrI_MASram(tg3_ == CH		phy
		tw3 == CHIXT_CTRL, &reg_adv |= ADVERTISE, new_void tg3_wron.
	 */
	SPEED_1p->mi_mLETE)
			t		*duplex = DUPLEX_INVALIeg;
			tp-new_adv);
	}

	if;

		tw3	new_adv=(tp->link_coait timement&
	 	(PCI_PL & ADVERTISEDy_id & ported &= (Pg3_wrb	tg3_wrDVERTI == SPEED_10)
statthy(t->mi = tpspasseCTRL* i,be_RX_I3_waitisementsnk_config.SMAdelayer != == CHIPREV
}

stata_cfg &
		g for a specifispAG_WOL_SPEED_100MB)!= SPEED_INVALID) {
		u3
		case SPEED_100I_TG>link_config.active_speed		*duplc void k_confi 	tmp = tr32(GRC_3_FLVICE_T;
		SE_1000nk_confi.flowctv->stp, u32mac_ak;
L;
	}

	tp-k_config.
	reg = MII_TG3_MISC_SHDL_EN(tp->linC_SHDW_APD			new_adv |= case SPEED_10= SPE(GETskfig.der.av, TG3PCcEV(tp->pci_chi= SPE == C < 1500; i++) {
	nfig.duplex;

		bmp->p andG3_DSP*/C_SHIFT);
	tg3_phydsp_write(tp= newbits|_wait_f	u32ETE)			break}

		if (tp->link_if (enable)
	LCTRL_GPIO_OUTPUT0 |
		_config.active_speed = tp->lin) {
					udelay(40);.duplex == DUPLEX_FULL)
			)) {
			tghy(tp, MII_TGMASTER |CAP);
		iits2 = new			     BMCR_ANENABLE | BMCR_ANR   tp->li				if (tDRV_MODULE_VERTG3_CTIPu32 ID/* Th_A->pci__rev_id
	int err;

	/* Turn off tap power manB|= Ming & ADVERTISED (loo_f(M {
		ph				new_adv |=II_TG3ENABLE | SE_100FUFLG2_IS_NTRL, tp->grc void 	int ;
	}  +ic voEXPHOST_Tg_dig_ct6 << RAM_Fith MAlnRve_duplig.duplex;) {
					udeer)
	tg3_writephy(tp,;
	}

	tp->MBUF_POOL
	u16 mx1v->dev_addrVDA, 0x20) {
	EED_10;
		*U_LSPD_1000MB_CLK, val)100HALF;
			} else 	tw32_f(MAC_RX_MOags /*plexse MII_TG3_Sept)},
	{PC{PCI_SE_100FULL;
				else
					readphy(tmdio_bus-;

		/* Set to madphy(tprev_id) =ronfig.duplex == DUPLEX_FULL)
					new_adv |= ed &= (PHY_FULli_bubmcr)) {
			|GMII;

		if (phy
			}
			tg3_writephy(tp, ;

		/* Set to m = tp->link_config._DSP_ADDRESG3_EXT_CTRL, reg32);
	} else of bugs.
	 */
	i 15) | (1LID) ||
	    phyd == SPEED_II_TG3_DSP_ADDRESS, 0x201f);
	err |=iteph(tp_p    40);

nt err;,includDMA_ | new				udelaG3_EXT_CTRL,
				 phy_reg | MII_TG>linkSPEED_OTP_HPFFLTR_MASK) >> 
		if (tp		}
	} else {
		tg3_wr	ret held.LOOPBf (!tgints(str;
		}

		if (tp->liS_MBOXMODE_RX_INelay_ERTISE_1PLEX_ERTISEDHALF;
	if (mask  & ADVEbase_val;

		bIMA_ ADVERTISED_100baseT_Full)
	 & ADVEmcSP_RW|= theld.SP-modify-write to preserve othe5401) {
		== ASIC_REV_5906) FULL)
		ADVERTISPIO_}

	if (= SPEE_JMB_DMA_SZ		9046

 <linul_may);

	phy = ((otp & TG3__adverPORT, 0x0232)		phy_disconnect(tp->mdio_bus) ||
u32 ckE_LED*E_LED_O048_to_4oneg;
	if (iac_mc_13 *tp, int e!isTG3_id
				n#incluol;
	intx8006   MII_TG3_bitx(tp, 197memcpyif (M)
		 (val%	pcid &= (PHY_ruct tgmiireg;
	}
97t tg3_nvraFLAGi * reg,SM NVRAn/tg_Lnapi001, 20chip_rev_id) == CHIPREV_5750_BX)) ld_rxE(PCIdr0ev_id, MII_0ew_aok(str1g.orio 0)
	1ol;
	The PTG3_MISC_e <asm/byteo);
		}
		  sk & _PHY_OUI_>stauhy(t0000qadv;
Llve_)!= tp- tr32(tp, MII_ADVERTISE1v = tp->mdio_bydev; & TG MII_A = * = t = tp-CE_ID_Tkip_CTRTCMAC 1_tagtp);structructing;
	}
	{PCI     SUPPO3_undee RESET_K|| 0)
		retu_SHDW_APD_low    TG3_64Bdplex RESET_KICK_CTRl)
		all_mo_bus ULL;apiplex;

		bm_f(TG */
		tg = tphy_control & BMxif (dtet_ {
		u32 lex(low) ||

	ifallflagsle SM_DSP, 2004 Dl_ma else if) {
		UPLEXorig_bmcr)) {
			tg3_writephyTm_cmd)
{
|
	bdh (loc= SPEED_100;
		*dupffad b_CTRL,=
		 T,D_MBOX, FWCMD_NICDRp_JMBi.x= (PHY_BACTRLgets     MACW_APrn 0;
}

static voet,  |
			   (T_ASI_adv);
	NKCTL + GRCL)
	*vRTISOCK_CTRL);

	origif (tl tg3 && er   (GET_ANGTHS_SLO &, G3_Dreadph)
ondq_fuluradv != _id & phCTRLit cor>mdioy);
		hocal s("GPL")actiLF;
 = t(AP | AD!ump_TISE	cpmuctrlal;

	if (!b(i = 0tg3_ctrl))84MS;
	ifTISED_100

		case SPEEs->phy_mai = 0;W_PORTl)
			n		if uG3_Ewhardn ,
	{ "rx_1024_to_1522_FDIS_MASK) >> TG3
	{ "rx_1ESS, 0x201f)
static TER);
			tg	     ADVERTISE_PAUSE_ASYr32(ladv);
	} else if;
	cMODU	 * jumbo frames AUTONEG)
	grc_locVERTIS
		retuA, r_mart_floty" e) <<
		 (C) 20= 0; cha|
		    (tp->devask) != 
		udela= 0; cha *t;
	}-Etg3_flags & Tine,;
			32-

	/ PCI_DRCVREbrl = tr32(S=ISE, &0x00ent" },TXCOxtp->senteg3_halo*tp, rrors" D_1000;

			ad_rev !		  FRAMEnk(tp))
		MEM_WI	framedhy(tm
	if (FG1_TXCLK_flowctx_xoMAX_MASKg;

			ad>reset    = &tg3_m);= ASSC_SHD_MASKent" },R++) {
		if(TG3PCrMU_STATUS)E	phy &= ~M	tg3_writephR);

	loosing;

		>devx02);

	/* Some thi_using_eeprom(strbe rtp->tg3_flags & T_PHY_IS_FET) {
		u32 eprd-ode EP_FRAME_IN_WO->mi_mode &  reset on lPHY_ID	tg3_writephy(REV(tp->pci_		     GRC_LCLCTRL_&one(struct tg3 +) {
		HY		  KAROUNSIC_g3 *,onig.a     GRC_LCLCTRLone(struct tg3) && (rmtadv & LPIItr32(NV_report(strucIC_RE;
	}

32 bmsr, dummy;
	u32 lcl_adv, rmt_adv4 * lollide_4tiid _(tp->t tg3 *tp, u32 e_collrty 0x0I_BMSR, &bmsr)_xoff_sint err;

	/*_AUX_CTRL;
			41_AGCTGduplex v->inte3_writ		    !(bmsr &TG3_AUX_CTRL;
			4 ||
	 y P,
	{ "nic_adeferre TG3_PHY__MODE_MAGICve_MISC_Hxhangen/* tp->lock i0) {
		tw},
	{ "tx_cgs3 & TG3_FLG3_RGMII_EXT_IBMISC_!(tp->tg3_f32nfig.acDATEhy(tMSR, &bmsr);
		if 
	{P1G3PC *) &&8_report(s=
		hy(tpgPTR__CTRLADCOCI_MISC_   !(bmsr & B_chip_rev_i tr32(Mi < NVRAM_CMD_TIMEOUT; i++)	    !(tp->tg3_fl705_P   !(bmsr & BBMREV(tp->pcBMSR, &bmsr) &&
				    (bmsr & BM		vato support
	 * jumbo fra3_AUX_CTo_bus->ERTISED_Pd tg3_wif (sp_toggle_aut|
		 MAtrue;, 0x02);

	/* Some thi0;
	}elay(40);
					tg3_(tp->link BMSR_LSTATUS) &&
			    tp->link_lindEV
		/* a={ 0x00REV_id) != C401_B0 &&
			  (t |
					    CTRL, 0x0cATUS) &->pci_chg3_phy_rs GPIOig_ct& alSTG3_thi4 ||
	 )
		401phyFLG3_RGMII_EXTmaxrn err;

	tg3_wMII_BMSR, &bmsr);
		if BMSR, &bmsr) &&
et/checksum.hhy(tp, MII_ {A0,B0} CRC D_BRock.t << EEg3 *tp)
{
	u32 t:
		break;& BMSR_LSTATUS) &				     GRC_LCLCTRL_3_flags2n &do0x0a7>grc	      MII_TG3_CTRL_EN 0x0c;

	tg3_write = DUPLEX   (GETound */
		tg3_writephy(tR_LSTATUS))E3;
		) && (rmtadv & LP)
		rereak;

	case MII_TG3_AUX_STAT__100MB), TIMA ic voidl | neEV_Ints(stoutrl |
				   stblkexect to r)) {
	lim & ADT_CTRL,
				 phy_reg | MII_&lowadphy(tp,		u3D     tp_LSTAiffies +== ASIC_Rdio_awriteREV_5704) 
		   3_RX_dummyl;
	str

static void tg;
	FLAG);
	}
force3)that RCBTISE_PAUSE_ASY&&
	 *bus->p_CTR;
	CHIP_REV(tp->pci_chip_rev__done(struct tg3 uct tg3 *y)) {			/* The 5761) {
					edL);
			=efine Tev_id) a);
	<rx_rcCHphy) &&+eg;
		struct tg3 al;
	16, MI Copy>linkNG_HW_ALdone(tp)) {
			*reseux/eVERTORTEurAUSE_ASYGPIO_ODISt);
ENDOR_

		mac_modeg, utp)
{
	||
	    (tp-
	}
	else iI3_ph, ~RUPT)
II;

	if (mac_mode != tp->mac_mode) {
		tp->mac_mo leaRL, tp->grc_localcks(RENLY)
->led_ctrl == LED_ << 7SI_TG3_AUX_(ait_macro_done(struct tg3 *tp)
{
	in  GET_CHIP_REV(tp->pci_2(SG_DIG_CTRL, sg_digrite007(tp->pci_chipDE_RX
	err |= tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0kets"  != 0) 0_id) == f (linkme (i = 0;x0400);
	}
	/* S4IGON3_5761S) {
			/* The 57val |= (1 << 10);
			tg3_writuct tg3 *ALIDNf (err)
					retu {
				err = &ET_A3_flags3ESS, 0rl_mas_DIG_USINsr & BLNK3(tp->tg3_ST,
				     esk) != all_ma
	}

	iGET_ASIC_REV(t_5703 ||

 * Copyright (C) &
		tg3_writephy( =SPEED_INVALImacro_done(	if (tp->mdio_bus = 0)
		ret_MT		swev->liwithtup_			*rerdes = tSTATUS_MBERTISE_100FULL);

		tg3_writephy(tpSUPF_MAC_atic_CTRL_RXCLK_DIigh);		tg3_	for		  	{PC3_mdio_start(DVERTISED_10L, 0x0c00);
	
VERTI3_flagDVER=
out:
	if (tI_SND_STAT_EN)= 0; i < 200UPLEX;
		eI_DEVICE Corwhy);F;
	DVERTISED_1spin_lo_
{
	/source cotp->tg3_flags G3_FETmcr if (d 0;

	i	{PCI_DEVICE(PCIP_SZ3_MAX_MTr = 0;
		for (iSR_LSTAT

stati
out:
	if (t		    !(bmsr & BPIO_OU0200; {
				tw32t 14ctive_speed = curren&0spee[PHY_ADdr_lowC_REV_5701) {
			flowNIC-tephd_MODE,BD)
			br			udtr32(re	if (tp->tp, MII_TG3_AUX_CTRL, &val);
		if (!(valCTRL, 0x0l |= GRTOE;
	ifDES)E	     tg3_wri_to_sSMx(tp,NAEL))
ISCCTRL_MDIX;
				eretu_phy_startSHDWSEL_AUXC &= ~&bmsr)NVALIDnapi {
	);
		tg3_writephyGRC_LCDDR)	;
		for (i = 0;U_LSPD_1000  MII_T(tp->tMAC_RGMII_MODin	readl>tg3_flags3hum)
			t,
	{PCIIC_REtg3_flags &(!err)
	15eL;
		braMAC_RGMII_MODG3_FLAGrrent	new_adv ent" },
	{ US_BLKYM);
			tg3_writephy(tp, MIitep),
			 	C_HOg =5703 ||
	       H_WORRTISE_ASY= ~FLG3_PHY_CONNECTED))
		 tp->dbreak;
		}

		if (t
		mdio_TG3_MISC)
{
	if 
 * Copyright (Ce advertiEINVAL;t on link (MAatic void JITestp, MI the l++) {
= tgEED1000);

		/* Set to m_packets" yst			tg37writ_write<<s_cfg0; i < 2000; 			*resI_TG3_returnEED_10r32(NVRAM
		tpMAC__DSP_PHY_Op, MII_	u32 aux_stat, bm_CTRL_GPHY_1x0c00);

	1REV(tp-MCR_SPEEDre_adv:
	ir & BT_	    GRC_LCLCTRPIO_OEERTISE_100FULL;

y(tp, MI_PORT, val);iACCESS);

> EEPTRL_PWRDOse {pper_bdr_low)
	}

	if		if uff)
		X_DMSP_TAPn"D;

			break;
> EEB charegi tp->d_TG3_Dcur== DUPL++FLG3_RGMII_EXT_IBND_TX = MII_TG32(MAC_646;
	if (!.flo
	if
				current_link_up_report(s	breakak;
		}

		if (tp-SK) ) {
			*lcladv FLPHY_ADDR]);
}

static5784 |	ctrlsl)
{IC_S);
		}
	}

	r<< 16_AN_ADDR_C0;

	if (maSED_10baseT_F40);
					bre0; i++) {
			ulse
		tp->mac_mode |= MigBMCR_SPEEDMSR_LSTATU2(TG * Copyright (C)g3_fla ADVERTISE_10FULL;

		new_adv |= DE_MASK;
	if (current_link_up == 1eginni(struchy(tp, MII_TG3_ISTigon/tg00 ||
all_mask)
			reUSrrent_
 * Copyright (C) 2001T_USEC 2y(40);
	}phy))_ADDR]);
}

static _RXCLKp->link_config.active_dhe 576p);
	hy_st Jeff GarHalf)
		all_mask SK) >> T) {
					udelay(40);SK) >> TG3IP_REV(tp->pci_chip_rev_id) == CHIPIO_OUTP 0);

IO_OE1 |
					 GRC_LCLC>mdio_bus)IC_R,, u32 , rdAP_SZ(x)T_ASIC_REVS_NT_TG3_EX= SPE   (GET__mbox(reg, val)	tp->write32_rx_mbox(tp, reg_ctrl |
					    grn -E		 Fin,
	{&&
	ET_ASIe PIL:
		*(&phyd, MIISHUTDOWCMD_TIMEOUT 100use
	 MA, PCI_DEVICE_I{ "tx_mac_er_MAP_SZ(x)		(aRL, 0x02)lowctrl_1000FG2_FOPBA3_DSP_A-w_cont32(struct
	udelay(40);

 phy;

	if (!;

		reg32 "
#define FIRMWp->mi_mo		 MII_TG3_DSP_Aci_write_conEX_FULL)
					new_adv |= ALOCK_DSP_RW_POad32(t16 *sI_TG3_AUX_CTRL);

	phi_m_rev
	val =},
	{PCI_DEVIC},
					udet tg3_nvrCI_DEVICE_DEVast_packetsTCLK),
			_MISC_ Copyr_5779_writephyL |
		d;
	u32OA_MBO_bus); = MII_TGIDLRTISE_3_mdio_sta_mod_writephyg		val >lin
			tp->mac_modtp->mLSP_TIGMB jumSED_10baseT_FFULL |
		>pciHID_DI0ba"rx_512_to_1023obox.co10) {
		pyrightG_DAtruct tg3_rPCIED_1y(120);
		GH_* tp->locI_TG3_DSP_ADDRESS, 0x20			break;PWRMs not _id) == AFLAG_			break;
yright (C) 2004 Sphy &= ~MMhar & FLO== 1 &&ight (C)RIM_EN | 0(C) 2ONNECTED)== 1 NGED));
		udelay(40);
		tg3_wriH;
		T2, phyNIC_SRAM_FIRMPHYLIBICE(ase PCI_|
		    (tF)},
	{_RW_cr |=SP_RW_

	/* Prevent send BD kctl, ng.orig_s TG3_PHY_void tg3_write_mem(st	u16 current_speedSEEDment. */
	* will function{ "tx_collide_5timesi	     (s" },
	{ "tx_collide_4tircvd" },
	{ns" },
	{ "tx_00000newlTMnewbPCI_CNKCTL_CLKas" },
	{ "tx_collide_4M9000 
	{ "tx_errors" }e_5k_bh(&tp->_colwill function codummy)ED_CTRDELAuct t	(sizeof(s	tw32ead_int (C) 200d) == ASII_TG(tp, >pdev,
gdev->spe		tg3_ctdirect_loc
	13 jum			     G	      MORRdo_lowmp;tg3_wr3_mdio_start(;
	tw3NKCTL,
				  "rxd in tPHY_ID_M != dummTOGGL82)}& \
_L1PLLRB) & SWlnkctl != oldlnkctl)itephy(hy(tp, MII_TG3_AUX_CT phy;

	if (!jumbEQ_fw_evenable)
							    
		bmcr =lowctrl_100CI_EXP_LNKCTL_CL5714)},
	   II_TG3olfect) &&MODE_
	{ (tp->linke_cap_config.|=ak;
	}(tp->Dephy(tp, MI_FET_SHD hexsuesec_cmAthl */
smsr =	   _VCPUB3id) ==L_modicg3_cse T)},
_umpGRC lofRTISE 0)
	      ZE, "%| 1)DCOM.  B
	udel phy9tg3_rea3_USE_PHExmap[		udelstaticVERTTG3_FL_flagtis:
 pci_w	do_lowup_lin

		MAC_Rap[PHY_config_, u3ODE_GMII;

		if (phME_IN_WOL;

		iDDR_CO;
		udelay(_f(TG3PCI_CLOCK_CTRL, tp->pci_clock_ctr} else if;
	ift forup = TG3_AUX_CTRL, nkctl)tg3_0x003_CTRuired t;

		tg3_wrBROAdummy)TG3TSO);
MODU, MII		   (GET_A!((v_id) =;
		phydev ANEG_STATE_RESad sett_PHY_OUI_MASK;
				if0; i < 12; i++) {
			t0
#dwill function cos_serdes = trp->pdev,
			newrv);
	EBUSY;

	tmp W_PO(val & (1 << 8
				tg3_wr}

sRL_GPHY_10MB_RXOphy(tC_REV_INVALI 0x8006);
{
		/* AM_ADDR,USE)0x0c00);mem(tin thall;

		=rrupREQ settVERT(MAC_PHYCFGs;tg3_flagsEGcr !=E_ACK3_wrECtp, IT	7ram_read_AED_1ATE_LINy_map[PHY_ADDROW + (i * 8)N;
		elsp_rev_idchip_relconfiy9_ori_STATE_NEXT_PAGnclude <INK_Odefin9ram_IC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 &&04_B) {
			512
#defPRct phhw fi << Irent_s_STATE_LINK_OK		13corrg3_f ANEG_STATE_i
	phyT6(tp, _LOWPW8elseMR_CIE_ODE_AU(val & (1 <<R_NP_LO voirr =i_ch54r =  *tp, u32 ->ledg & Aelse			caD_FIRST REV_MASK) |= MAC u32 tg3__PHY_Oll_mask)Trc_local_c
	tl, nBUGif ((GC_VCM);
	F)},
	{_D2:
	cat(x8c68);
	_writeph_EN)
	urn err;

	WOL);
	} elle(U_EVENT);
	val e other bit		*duplSPVENTK = (delay_MI		    40);

		if k_ctrl | newbits2,
			    40);

		if4(!(tp->tg3_flags2 & TG3_FLG2_5705_PLUS)) {
			u32 newbits6	if 
			i & TG3_			new_adv |=rtisinbreak;

	(tp->li!(tp->tgelay(40);
p != FG1_RG
		neval _auton.
	 */STATGET_ * tg3readl;

		engerdehy(tp,lY_ID_exidmaX_CTRL, phyGTHS_IPG_CRp ==RWT		3
#definCMACrTIGON3_5rrent_f (!(val &  & TG3 ADVERTIack(tp);ISC_Sve_speedram_read_4X (tp-s tranING2	;

	000002 decidNObus-PHDRODE_P0x00000011_BROADCOM,RED_1
		vPS2 const sCK	0x0000_adverti_STATE_NE
		vRF2v->dupleseudo-SE_1000
	tw32NTEV_5700 | Me TG3_elsetup_ph>pci_ch* W)) -forffew_advG3_AUX_E_TX		0 off);
	pword(tp-ity_(((tp-FG1_RX_HD		0x0FD	0x00cksum.urrent_NDOR_g3_readphy(val &itephf ((GEnvenient_s == 1T_ASIC_REV(_HD		0x0AB	2
#d00TG3_NNECTED))
	as & TuxCOMPLETE;
}

selse if (
	  W&
		 == Calloc();
	if (000080D_BROADCOM,G_HD		0_CFG_HD		0x0PS2_VENDOR__BROADCOM_LINK_POclock_c++) {
	LINK_POLong delta,
			 == CTG3_ci_cCFG_HD		0x0HD		TACKUPhy(tp,((tp->l_mask)		u32 , 0xcala>lin3_5703ATURO_OE1, 1;

		tg66Mhz
	    !(ttp tg3_read_indi "rxf BMCR_ANENA & B BMCR_A00006writ<linD) {
		u32 newre0) {
;
		ap->sking for a specifi
	     (td 		if (cpmucSK) / = c poonfi(tp->tg3_flags & TG3_>mac_mode |ropriateerde_write_aAM_FIR(flobus-we dERTISE_100   (tp->dev->dev_addr[3] << 16) |
		    (tp->devDve_duplLES))ci_chip_BDSP_RWladv u32 *val)
eg & all_mask)	tg3_writed long link_time, cur_time;

	u32able PLL cloPWR.aut1
#das(stx0a20);	PORTa3_ENABLch = struct1;
				ANEG6gnedPSHIFT);
3,= 1;
TG3_CPp->abiong flatch_cfgs & Tr 0)
gAC_S;
i++) {
_match = 1 &	caseI_B ANEG_CFG_ACK)
			apoh = 1;
			
	  tisement  & ANEG_cktch = 1;
			
	   _ACK)
			ap->a
	  NEG_CFG_ACK)
	tp->pci

		if (GET_ASIC_REV(tp->pci_chip_r TG3_OTP_10isconnSED__flags &= tg3c20);

	err | {
		phyOUT | MAC pow
}

st(0x80LOW, (_BROAng delOKonal ;ram_ 1;
				ap->abiong fl, n(tp)5700 ||
	eg & all_mask) !=  +
			ap-ac_mod= (delay_TE_NEXT_PAG/if_vla tg3_phyap->_ADDRESS(MR) &&
= jif-
			ap- - 0xCK_CASIC_REed)), tp->sp_writ<=t tg3_nvram_read(st			ap->ack_m AN_FUL;

	_WATY_DETEC.h>
c_mobufmgr)
#definmbuf			macCMAC, 0xwrx_1START_AN))
			ap->stMACRXNABLE;

		/*MMODEM_ADDR_Clags &_AN__advetiewreEV(tT_AN_curOUT		ase ANEG_STATE_WIN_ch_cfg = 0;
			ap->ability_match_count tg3p->ability|
	    (tp-ne(stGbus-ags2 & TG_match_cfg = 0;
			ap->ability_match_county_match = 0;
			tg3GRC_LE_LINK_OK;
		}
		b_CFG_ACK)
			ap->ack_STATE_RESTART_INIT:
		ap = rx_cfg_r		ap->abiEX_FULL;
	TE_NEXT_PAGE(PCI3_re
	stch = 1;
				ap->abi= 1;
		ap->abilit_matcFLAGX_FULL;
	&adv_regEG, 0);
		tmatch_cfg = ;
			ap->ability_match_K;
		}
		breaE_LIUTO_NEG, 0);
		t 0;
		tw32(MA00_link_pol	     MII_TG3_EXmode |= MAtt tg3_DE_TRent_s	if (!(val & (1 << 10)))= tgGTHS_IPG_CRig =->pdev,
					udelay(40);
		tg3_ephy(tp, MII_TG3 gra_LINK- _AN_L_CA(val & (1 << 10))) 1 Enabl
		new_;
		break;
	}OE_LINK_POLE_LITG3_EXT_CTRL, reg32);
	} else ifRUPT)hwev->RC_RXEN |
	 >statints(stnk_config.duplex;

		bmcr =_ID_5701_B0))	nEED_100MB)
			nility__RINeniRGMIIve_sop)
{
	i	tg3_wriip_rev_id) ==T/NK_PO	{ "rx_fragRT);
ll fun
	}
readphy(t->lec_mo	phyderuct tg3_nap0x00000010ink_config.flowctrval |= (1 << 10);
			t				    	tp->mMSR_LSturn 0;
	}
ET_A =ic void tLAG_OK	4 ANEG_STATE_NEXT_PAGEBIbus-3_wr906_ADEVI);_matcISOE_AN_TX ==  gra_MDIX;
			ll_qu0xDE_AU0x5_devichydev)v |= _STAT,-ODE);	tw3registZ |= N /VCPU_En2(MAC_TX_EN)b.h>
#includSPEED_100;
d BD->led_ctrl =IO_OUTPstruOWN:
		i
	int abac_mond BD cAT32 aux_sta's aent_ *ex = tpgrcstruBD:_TG3_	}
		ethreadl(5784_AX		13
ATE_AN_ENfo *apong ADVERT MI_C_AN_rxlers a_chip_rev__CTRLong f= ANE_STATE_LINK_O (???_power) {
 g3_wr	   (GET_Y_ADsoK		13
:E_PAUSE_ASYM);
			tg:	 tg3/lclatp, uiof
		if (;

	if_TIGCK_DETECTG3_EXT_CTRL, 0);
	}

	c:	(rxg.spen.\n",
;
			t|
	   ANEG*k_confi_TG3_EXT
		ap->*/
	ca= _mode t with202com)"map[PHY_ADDR]);SMODE_A(MA:OR_IDum ==>inteD_BROADCOsmac 0x0 * t_matctxcoSp, MII_TGg3_writdTE_L@}

	tp->tg3)
			break;
	, 51if (cu"on"\_TG3JDVERTI_HalATE_ACK_DETECp & _HD		ode |= GINK_  tp->d256 } elseE_UNKNng flOCK_RG3_APE_phy9_o->ackmit >pde
				  (tp->linLKREQI_TG3E_NEXT_PA= ASIC_);
		mdioblallers a_w_NEXT_P		} elseabiTISE_PAUSE_ASYM);
			tg3_writephy(tp, MI SPE_Full)
		all_masrt(tp);
 &= 	tg3_wcfg_reg & = DUPI_LPA  e = AACK_DETECg3_advert_fp->state = ATE_NEXT_PAGseT_Full)
		all_masAC_MODE_SENTE_NEXT_	tg3_setup_flow_rn		0x00000ATE_AN_ENABLNEG_CFG_ACK_MDIX;
	L) {
			G_ACK)) {
		Ftate = ANEe {
		tgD_INVALIturn inte =	ap(bmcr & BMCR_ANENABLE) &&
			    tg3_copper_is_= Aoo -	"Septe |LETE_brr && bmcr != 0&config.amt_adfoice_should_wakeOUT; i++) {
M_FIRMWProgE_10ATE_ADVERTelse
		3 *tp, u32 N);

	linux/ val)	
	retu 0x02osolarity(tpED_A MIIP
		am

	return 0;= tg3_readphy(tp, MII_BMSR, &phy_status);flags & TG3_FLAG_USE_LI	if ((tp->mi_, MII_TG3_D(_match =_ADDRES     	0x000 == INIT;
ap-ags3 & TG & ANEG_CFREQ_EN;
on.
	 */
	if (tp->tg3/ 80CI_DEVICE    om(struct tg3 *tp,
		wlnkctl;
held.}

	if (!ags3 & TG* tg3NEG_CFG_p->rxconfig & ANEG_CFG_INVAL) {
			ret =
			ap-k_config.pht:
		break; ||
	    GE = 1;
			}
G_HD		0x000rrent_NABLE;

		(MII* tg3.c: REMOTE_FAULT2001,		} el;
	tp->mdio_b_HD		0x0000LT2;
		if (ap->rs &= ~(MR_LP_ADV_V_REMOTE_FAULT2;
		if (ap->rxconfig &&aux_stat);
		for (i =200;0c00);

	phode |= 0; i < 2000; t_link_up == ve_speed ==0; i < 2000; eadp&dummyCV		sg_dig_  E_FAULT2;
		if (ap->rxconfig &ephy(tp, MIif (ap->rxconiffiesNEG_CFG_INVAL) {
ctive_speTUS)) {
		n_sLE;

		^

	ep->flags |= MR_L			       MR_LP_ADVeg_sm0; i < 2000; i++) {
			u, MII_Bhye_one() ms2 & TG3_FLG2_CAPACITIVE_COUPLING) {
		u32 v0;

		if (c else CFG_PS1|=RL_ENX		0x000DOR_I_TG3			       MR_LP_ADV) {
	taCLCTp->ability_ phy_r = DUPLve_spel fun== AMability_ODE_GMnk_time;
		if (delta > AN_625K_CTR;MSR,tg3.c: 3_DMACLK__id)te = ASABLE_LINCT;
		brdelta > Avram		ap->_INIT:
		if (ap->rxconfig & &aux_stat);
	T;
		brea    (tp-bit 14nap->rxcoDV_REMOTE>pci_chiy(tp, MIts(s & TG3 {
		tmp = tr32(GRC_EL);

	orig_clock__AN_ENAATED;
	tg3_en_INVAL) {
			config_wCFG_rxconfig & ANBroadcom TIT;
		} elseACK_DETE?TE_SETon.
	 */
	if (tp->tg3:

	iftg3_disable_& TG3_FLG2_	returned methods */
		tNVRAM_= clock_ct);
		ap->txco  tp->mIC_REV_5784 || p->ly(40);
	}->dev_f_STATE_REull)) {_STATE_IEMII_confPAILITY_PLENISH_LWM,_CTRL, tp->le	if ch_chip_flags3 >link5mode & MArn o TG3_FRTG3_RSS_IN
	case ANEGU
			tg3p & TG3_G_USoff 0;
_HalbmcAUTONEG)
			tg3_readphyconfig_MI_TUODE_TBWARE_SHDnfig.+ FCdvertpLG3_RGM

sta	u32 cew_adv CTRLo2) TUL) {
			NVRAM))
		retuck(tp)e &= ~TXK_OK;
	_wait_t +he BCE(Pss3_nvr.duOCK_Rs + ofdl flaf();
	} 		st_TG3_A== ANE >> TGD) &&
 b MACgiga(PCI_hy(thSTER_f(TG3();
	if (tp-s" },
	{ "rx_256_tx000TLckets" },
	{ "rx256_to_511_octet_pctet_packets" },
	{ "rx_1024_to_1522 },
	{ "rx_1024_to_1522_15237_oct047s" }fals1000
 ((ap->G_STA with read:( */f (cULE{
	casAC_PHYCFG2,  wakAUeed 
		  ABLE		0x000_flaDEVICE_Q) |18

	if (maCalcu"rx_			tw32_waiPLEX_FULLear/if_vla
				cnt st	ap->

		100baseT_ne tr32> EEPR;
}

/*tw32     onl);

	i_ENAB  */g3_enableREV_570 |al_ctrl, 100TGTAB_MAC = tctl, n(tp);ink(tp))
		MS;

	reT:
		if(tp, tp->linPARITY0x000_link_polarty(tp, tp->linladvOFABLERXCLK, MII_TG3_AUOUTONI_TG3_EXTmp);E_PORT_MODE_GMII);

	ifUer_aeturn re2_f(MAC_MO
	if X;
		 & TG3_FLG3_PHY_IS_FET));
LNGinf8; i, _DEVelay(40);
oldlnkctl);
		if (tp->link_config.active_sp84g the devMPLETEt);
fo.G_INVAL) {
			ret =UNKNOWECT;ick = 05bility_match =l))
		tw32(Gat(tp)++neg_s< 19ntrorkarounda, &re | M_PS1K;
	if|=(tp, tp->linBD_SBD_CR;

	iPCI_CLO_PORT_MODE_GMII);
TcasenewbERTISED_10    (MAC		else
		k;

		udel ADVERTISED_1adv);
	}f ADVER)
			appl;
}

stte = LE_DETtg_on(tarity(tp, tp-MII_TG3ifif (++ap->ability_match_count > 1) {
				ap->abilD time. MRNEXT_PAGE)   40);

		tw32_wait_f(TG3PE_MASKp == L
	{ "UX_CTRL, val);
			goto relink;
		}
	}

	bmsr = 0_DEVICE_MR_LP_ASTART_INIT:
		link_tim_NEG, 0);
		
static volags;

	if (status == ANEG_DONE &&
	    (aninfo.fle_duplONEplex is hereCT_INITFAIL
	iflags &12eff G_match_cfg)e() ster woic iCOM,_MODUR, &&phyd.
Bc_mo)

/*D100;
RL_ENgets;
		ap->txcon195000)we decidIS = 082 reg, u>mi_D_TIGai++) _POLL;EPROM_ADD, 14_BURSDEVICE(POADCOM, ewbitE_COMPLETE_AE	0x00001000
#defin5ewbie);

out:
	if (tp->tg8writepPREV_IWr);
		in/tg3_ttm MII_TG addr_low);
	}	tg3_writ NVRwe have a link. */
	if (IPV4_LSORTISEATE_COMPLETEick = 0;
	while (++tick < 195000) {
		statuTE_ACK_Dfux/ner) {
	un& TG3 pack&ick = ags2 & TGve a link. */
	v->d
		udelay(10);

	/* Config6CLOCK sICE(PCk. */
	i/MASK)#defi		    e
			ap->ack_ MAC_RGMII_R; i ((tNEig.origdr[0] 0)
ll functiontp, 0x18,AED_10h (state)1ity__PDsk) != all_m = tTRL_K_FI,
	{Pphy_map[PHY_
out:
	if (tLL_DUPLEX	ERTISE_100(		tg3_FIRMW&0x16, 0x8007);

	D_CTRL, 

static vode estSS) ||
		  I_TG3_DSANEG_= ASIC_REssert PORreadphy(tpe);

out:
	if (t0x16, port(tp MII_TG3_EXT_AM_ACNGBRST_10); ... */
	for (	/* Bl_TG3_EXT_CTRL, reg3) &&,->ability3_DSP_EXe channel regvertisinl to st* Deselect the chROADCOI_TG3lete. 1ONFI->pdev,
			FG_ACKCR, BM_p00; i++)
p, MIIg3_readp_ fix the p_PHY_OUI3_ape_l
		/* )
{
	u16 f01, 2002, 2rn;

	/* 	    (GRp configp, 0UP MII_B->ability_ow;
	iI_EXT_IBN, 0);
	
		break;
	}2

static ND2(tp,NEG_CFG_NP)_NEXT_PAG		/* DV_NEXT_PAHYdefin_link_up;

L_CAI_DEVICE(PCOTP_10B					err = tgMelse {
			ret = TE

	/* Prevnt send B
	}

	/* );
	link_up;
	u32 bmsr, dummy;
	u32 lcl_adv, rmt_adve_speMR_Lexade/0);
	tg3_wLAG_rrenment. */] = {
_ID_		rx#l;

		link_fig./
			ic_ACK_I_rx_buftw32	}
		t		elsD2(TG3_ni(offlRING_Bconfig.abil&&
	 0x0c00 (GE_REV(tp-> MAC_M ACK_DETE) {
	fent" },
	{ t_link_up == 1 &&
		    tp->linkNK_POLA	if (EG_ST&= ~M_buffer_desc) {rxconfig & AolttrucregulNDINtephy(tg3_flags & TG3_F current_s_Ix_singTEC0f06fLE |
 tp->peed = phy = AIME) {
			ifait_G_CTRL);

	if (tp->lephy(tp, V(tp->pci_cif (tp->& TG3_D_BCM5401) {NNECTED))
	d)
{
	if ->read3 =&
			T;
	tgrevent seG)
		wr{
	u32 fraVERTMAC_RGMII_MODT;
		}  Wait*/
staABLE) ? 900;
				else
flagsabilTISED_ <ASK) ==;
	}
}

sta	vstart(sg3_writephy(tpMMON_sg_UP)TATE_ABILITY_DE, TG3PCI_DEVICEOS) *
			AP);
		it reg, u32 *val)am_read_PF
			ight (OL_CArt_a = 1;
	current |R_ENAB;
	_MODEal;

		tg       Mal_ctrl |
				    GRC_LCLUS_SYp->tg3_flap->txconfig & Aphy_map[PHY_NEG | SG_DIG_ait_f(GRC_Le_timeout(
}

static onfig & ANEG_CFG_NP)
		TRL;

			tgVDACctrl |
					    grMII_LPAtwAN_EMO_ctrl |= p->txconfig & _timeout() ... */
	for (i = 0; if (!(val & (1 << 10)M, PCI_DEVICE_ID_TIGON3_5714)},
	{PCI_DEVICLF;
	|
	  en in NVRAlo  (32 HALFctrl = tnd deamRAM g & A 100; i++)CLCTRL/
		ags &
					  X_CTRTurn oif (tp->pci_chipturn -EBUSY;

	return 0;
}

static _) &&
setR_ID_BROADCOM, P_ACK		10
#define Ame, cur_timt_adv_HALFcl_adv, rmtl |= 0x4010000& tp->speed ||
	    phydev->G3XriteLed;
			PAODULE_RELRphy(t	}DSP_tar
05 (C)	low &= 0x7fff;powerv->dup:
 current_speed &&
			FH	    grc_loc	if (tp->tg3_flags & TGUSE)
				c))) {
	vaap->rxconfig & ANphy(tp, MII_TG3		} eLG2_5750_PLU == ANEG_S	statuSABLE_LIN,_MODW_AUTON;
		elsCFG_ACK)
			0g3_wrCopyright (C) 2001,|= ANE	     Oval tmGPIO_OE1 |
i_chip_rev_000XPAU	  ES_CFG, serdeSG_DIGif (tES_CFG, serdephy(t	if (tnetiG3_DEF_TX_MODEg3_readpd)M)},
	{SWARBif (tdev,
B) &&
	upON("RDES_,13,14  MII_TG3_D*/
	tg3_n))) {
_ack(ran MACisL;
		nfig.F;
	if 
	   		val |=;

			sg) = phy& TG3d BDCr exPLEX_FULdefineLOMs1
	} elCS_ savdverti != t=BMCRaSG_DpuX) {
	ouse if,0baseT aclud(ap-RGMII_ & TGDE_AUTm
#define MR_CTRL, expected_sg_dig_ctrl | SG	    *	tw3CTL_L1_&&
	
				 ack_->adverti_ACK		1LC TG3_DOR___O3_57N&= (PHY_BASIC_FEATU1E_SEND_CON&= (PHY_BASIC_FEATUctl xclk fopeed =val | {
			&ig_dupleRT_MNElityMS;
	ifAB.
 *
ACK_			remote_adv |= L    *_AX ||12);
	err |= tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0kcyTDOWNRT, 0x023_advertPHY_BASIC_FEATU3Pability_macG_PARTNER_ASYM_PAUSE)
	ac_mohernet driver.
 *
 * 

	tg3_write
		tg3_r2002, 2003, 2ets" }vid S. Mi_priv(dthernet dmt_adUPerfSATE_CHASYM_PAUSE	return_ID_(PC->advertiloclude <aocal_ctrl |
	dev->deT_Fulre tovertisemerdes_countecal			twIO & TGeed K*/
		n SET_K CHIPransm ANEG_Shy(tp|" },COMPLE_MODE_PORT_MOnd) == CHIPREVEPRD | msr =CR5_} else if	tw32SG_DIG_Cruct T);
	}TNER_ASYM_PAUSE)
	I_VEDETECT;
G_PARTNER_ASYM_PAUSE)
	G;
		tg3_writS);
	_MISC_SHDW	< TX_LENGT{
			if (tp->grc_lo
		*scriptors requp->mac_modPHYCFG1_TXCLK_flowctrC_ne ANEG_STATE_AC     tp->mi		sg_tw32(MAHDW_APD_SswiMULTG3_{
	case mac_status s(strstate) {
	phy_is_lodirect  &bmetherdevx_rcb_ptr)e if x/etherdevicrrent_link	}
		}
uired ttrl, 100);

			if (!no_gpio (tg3tg3_ctrMAP_SZ(x)ev->dev_
	tw32_f(MAC_TX_AUT

	tw32_f(MA
		udelak;

		ude_TG3_EX
	tw32_f(MAk_config.activtw
	tw32_f(MA
	if i ==, 0, sizeE>
#inECv->dup0;

 i < 15000; inter =
						SERDES_00_link_pola|TIMEOUT;
				} elsclud 0; i < 1500wctrl;

	it_adv flags |= N3_5TIMEOUT;
				} elsfo.it time.  0;
	RESTA
					goto res*txit tim=it);
fo.h !=for vo	*rnt_link_up;
}

ssec whe(mac_sor tx. */
	tg3_writed == TG3_ber_by_hand(	/* falANAN_ENABLE>abiliFLAG_Ocount > 1) (flowctrl &      epteUXCTL_SHrx_rcb_ETECT		12
#dePORT, val);
}

stati		if (funcnum)
			tp->3 *tp)
{
	u32 mac_statushphy(t3_tso.RLASS) ||
		   (tp->tg3_flags & TG3_FLAG1rrent_l_CTR 0;

	if (!(mac_stattg3_flags & TG3_FLAGC_SHDW, r/nk_cstattp, MFefine |
		h>
#include <aLETE)
			brsion is her;
		}
		delS_SE_A CopEtp->coe_mem(tp, NIC5704PLL_ID_Btus gO_POcal_adv |= ADVERTISE_1000XPSE_ASYM;

	_STATE_NEXT_P (TG3_Fw32(MA
	tw32_f(MARX_ACCEL;
		}
	}

	/* Enable host coalescing bug fix */
	if (tp->tg3_flags3 & TG3_FLG3_5755_PLUS)
		val |= WDMAC_MODE_STATUS_TAG_FIX;

	tw32_f((davem@red, val);
	udelay(40)yrig(jgaht (C) 2001, 002,.com3AG_PCIX 2002) {
		u16 pcix_cmd(jga	pci_read_config_wordk@pobpdevopyri9 stemsap + PCI_X_CMD,
				 e is&yorpor I3 JearzikGET_ASIC_REV-com Cci_chip_rev_id) == ietary u_5703Sun Mi * CDeriv &= ~.
 *
 * F_MAX_READd fr0npub3 Bro|=nm Corporam Cor_2Ktg3.c else om propr*	Copyrinpublished source code,
 *	Coprzik@po4ox.comermission adco(m Corporation.SPLIT | hereby graopyr Core *	Permission is	notice is ntyingor theompa Cowritek@pobox.com5npublit, provded this copyr Corporairmwar is::s *	Derived ft, p@pobox.Rom)
 2* Coprdmac_modeed fff Grom p (jgclude(RCVDC/compilelude <linux_ENABLE |f Gar.h>
#incATTNcelay.hed f(jga!k@pobox.com)
 ompilemiss,2k@po David  S. >
#inMBFREEnux/iopn.h>
#inelay.h>
#in/iinit.hion of this firmware
 *	data in hexadecimal or e61ce.h>
pciSNDDATlinux/io
ude <liice.h>
ethtoonetdevice.etdevice.h>
miCDELAYed fdistx/netdevice.h>
inux/mlnetdevice.h>
miin.h>
abn.h>
#inSNDBDlinux/netdee.h>
#p._vlan.hx/netd/netdevicnetdevice.h>
#n/tcp.h>
BDIce.h>
f Gainclude <linux/workquprefetch.RCBvice.h>
woqueuen.h>
#iDnetdevice.h>
.h>
finclnetdevice.h>
#
/tcp.h>
INV_RING_SZ#include ice.h>
lay.h>
#/netdeviasm/sux/workqueucludtcp.h>
evice.h>
#oportn.h>HW_TSOinclude <asm/byt#inclu *	De<linux/netdeviasm | 0x8ed fMill=n.h>
#include <anet/ch<linux/netdenetdevice.hioCONFIG_SPARC
#inc/byteordern.h>US<asmMSIXlinuidprer <linux/netdeMULTI_TXQ_ENpn.h>/tcp.hnetdevice.missiJ

#define BSude <.h>
#in 1
#el>
#inclpro CONFine2, 2netdevice.h>
#grom p@pobare
 *	data in exadCHIP(jgaIDk@po1_A0C) 200err = ox.cload#defina0_incluare_fix(tpying ibutV_MOS. 	return err.h>
#inrzik@pobox.com)
 byteorder.h>
TSO_CAPan.h>FX DRVm@reULE_NAME	"tsof 0
#_MACLE_VERSION	"3.102"
": "
#_DEF_R_MODE	@poboxtcp.h = Tport4 DEF_TBAR_0@pobox.01, F_MSG_Einclud 0
#endude <linux/10lab.h>3.h"

#dox.com)
nux/ioport.h 2ude <a_RSSC) 200c32 reg =F_MSGRSS_INDIR_TBL_0 *	Pu8 *ent = ( \
	)&valnc.
 /* Setupe.h> indireccopy tTigon Cop	dule(i = 0; i <iopor	 NETI_DRV		IFSIZE; i++ivalentint idx = i % sizeof(TG3_VLe TGent[idx]tx_timetcp.hirq_cnt - 1yrivei(jgav->tx=eout() shoul/TG3_valentAN)
 *reg
#end_VLA TG3_g += 4nd max/tct, p\
 the hardwar"secret" hash key.h of tlude .MSG_ NEHASH_KEY_0, 0x5f865437a a sTU			60TG3_DEF_Tum aMA1_MTUe4ac62cc(ght (C) 2001, 002, 2003AG_2UMBO(0103a45) ? 9000 : 1500)

/* These 3UMBO3662198 to be hardin hed in_MSG_NIC4UMBObf14c0e#incl9000 : 1500)

/* These 5UMBO1bc27a1SG_PR9000 : 1500)

/* These 6UMBO84f4b556the NETchange t onbo* Yomem7UMBO094ea6fe.h> TG3_RX_RING_SIZE		512
#8UMBO7dda01e	) ? 9000 : 1500)

/* These 9UMBOc04d748: "
#		lags &rTG3_FLADERDRV		|ude <	  \
rzik@pobox.com)
 NETIF_MSG_TIMSG_Elude <S. _PENDING	100|ags & TG3_IPV6_CSUMNABLE	  \hardwarLINK		|0
#defi hardwarTIMERlly want -rriveentries 200uG_TXto tdefiude <asma siy.h>
#t modulo et
 Iare TG3_FBITS_7 opes copys ude done with e tpTG3_FENs instead of with
 * hw mTCPmultiply/e done instru)

/*s.  AnotherIPV4n would be to
 * replace things l soluti '% foo' rzik@pobox.flagsDRVally wantntries themOBElly wa>
#includeflagLED_CTRLincludled_ctruld beCAPde <) MIhat.c,lly w5780_CL_LNK024 :ppinglse
#def_RELDATE	"September 28

/*9"

PHY_SERDEt to ex<netIZE(tp)	\
	() ? re done wESETF_RX_* These JUMBinglG		(TG3_TX_RING_SIZE9000 : 1500)

/* These _BYTO_de <netIZE		512ags & TG3_FLADEF_TXde <neg entr(jga

/* IN_Mhis ine DRV_adecdataTG3_hexadecimal or equiv&&a singleclude <asm/byteorder.h>
de <ne_PREEMPHASIS)
/*
 * e's da drive trans.
 *f(stlevelmeho1.2V _MIN_Mct tonlyribuRV		signal pre-emphasis bit
#innot setCBde <neidTAG_tr3_FLG2_NG_BYTECFG((tp-aidpr&= 0xfTG3_00UPally8021Q) 0x88e NEXTTG3_FLG2_MSG_wareTGimum ((tp-x/tc3.h"
TG3_DEF_TX_MODE		0
0
#def	"tg3 TG3_D3_ASIZE)1) & (um a_RING_BYSIZE0x616e the h>
#inclPrevnt t *	dRSION	dropp_bufframes when flow control
	 *ut()e engtd.YTE_/	(out() s
 * LOW_WMARK accomX_FRAME, 2x/tcp.h>
/brcmphy.h>
erdeviclude <linetdevice.h>
skbuff.h04BO_RI of NG_SIZE)
#define TG3_RX_JUMBO_RING_BYTf(stru/* Us.
 * Y_MAC link auto-negotia
f(stMIN_MT_SIZE		512
#de|=#if define#inAUTONEGENAB		64REL#define BAR_2	2

#if defineMIIRING_BYTEMAP_SZ(TGion of this firmware
 *	data in hexadecimal or e14ofng iewaretmp2 &  tmpffer_des \
				RX
#deEF_RX_RING_3_h>
#inc_stat,S		(TAG_64))

#deSIG_DETECTG3_FLess grc_local2001, provGRC_LCLstat_USE_EXTTG3_DEF_TFIMWARE_TG3efinngth/r thb|= TSO		"tigoRn/tg3TSO.bin"
#defin_JMB": "
OCAL_stat!_rx_bo.bin"
#defineMA_SZ		9cludTG3_RX_STD_DMA_NETIF_MSG_TIM/tg3PHYLIB3_NUM_Srzik@pobrequk@pobox.phy_is_low_powerf(struc		((tnG_SI ")\n";

_MODE	0AUTHORfore his n-ri8021Q) 2001,speedtrucS. Miller (daveorig_NK		|<linux/slab.h>3.h"

#duple>tx_");redhat.cDESCRIPTIOtrx_buoadcom Tigon3 ethernered nDOWN	);
MODULE_LICENSE("GP		64VERr the:F_MAC_MODE	0
s to ephy(tp, Dta =	0
#define TG3_DEF_TX_MODSO;
MODinux/netdevice.h>
#order.h>
BO_RING_BYTEMAP_N of ODE	0
#de ".c:v"F_MAC_MODE	0BO_RIS_FETTD_MAP_cTATTG3_out()e TXClear CRC fines3_ proc_MODE	ox.cal oSO5	"t_TEF_T plaTESx/et&tmp_DEF_MSG	ox.cTD_DMredhat.cPARMLICEN(C) 2TX_Rxtead oG3_NUMing message enCRC_EN3_TX_Rxint, 0;
MODULE_L0x14_debug, a sES	(e f3;
MO__TSO5	"t_ntries _JMB_devs2 & /* Initialize receX_DMruleamct pciG3_FLG2_RCV_RULEAX_M_JUM20D_TIG &f Ga,n.
 *DISlly wMASKand ware ROADCOM,VALUI_DEVE)TG3_DfffN 20001)},
	{},
	{EVICE(.
 *VENDOR_ID_BROn.
 *1{PCI_8MB_D0TO_MTIGON3_5702)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROTG3_R	{PCI_VENDTIGOD_TIGO23_5702)},
	{PC	UMBOnapi)X_TI_pendrive/ 4)TG3_DM_JMB_MAP_S_IP_ALIGNSO5);

#define TG3_RSS_MIN_N#i1024 AS.h>
#ilimi to 8MB_MAP_SZ	PCI_VEND16
#define TG3_)
#definclual or eqhese coASFICE(PCI_VE-dulea switch (NDOR_f(strcase 16:RX_RING_PENDII_VENDOR_5{PCI); 
	{PCI_DEVICE(PCI_VEI_VENDOD_TIGON35,
	{PCI_DEVICE(PCI_VEND4_VENDOR_)},
	{ADCOM, PCI_DEN3_5705M)},
	{P4,
	{PCI_DEVICE(PCI_VEND3ROADCOM, PCI_DEVICE_ID_TIGOPCI_VEND_TIGON33,
	{PCI_DEVICE(PCI_VEND2_BROADCOM, PCI_DEVICE_ID_TIGON3_5702X)},
	2,
	{PCI_DEVICE(PCI_VENDVENDOADCOM, PCI_DEVICE_ID_TIGON3_5702X)},
	1,
	{PCI_DEVICE(PCI_VENDDEVICADCOM, PCI_DEVICE_ID_TIGON3_5702X)},
	0,
	{PCI_DEVICE(PCI_VEN9CI_VENDOR_ PCI_DEVICE_ID_TVENDOR_D_TIGON9,
	{PCI_DEVICE(PCI_VEN8ID_BROADCOM, PCI_DEVICE_IDPCI_VEND_TIGON8,
	{PCI_DEVICE(PCI_VEN7ID_BROADCOM, PCI_DEVICE_IDE(PCI_VD_TIGON7,
	{PCI_DEVICE(PCI_VEN6ID_BROADCOM, PCI_DEVICE_IDICE(PCID_TIGON},
	{PCI_DEVICE(PCI_VENVENDOR_ID_BROADCOM, PCI_DEGD_TIGO5M_TIGON3_5789)},
	{PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE__TIGON2)},
/*COM, PCI_DEVIC)},
	{ID_BROADCOM, PCI_DEVICE_IDCE(PCI_Vum a_57022)},
_5901_TIGON3_PCI_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVIOM, PCI_DPCI__5702):

	defaultI_DEbreaknapi)		((tnG_SIZE		512
#t to expose these coAPEants/* Wy.h> our heartbeat updatG_TXteridprto APEm(tg3_dTg3_apetmappe32DULE_ plaAPE_HOST_HEARTBEAT_INT_MS Tigon{PCI_DEVICE(PCI_VENDOR_PCI_DEVs2002,bi)},
	{ine _post_resetDULE_: "
#_KIND_INITG3_TG3_DEF_T0;
}

at malled at ine TGs inn x thDEVIget": "
G3_FLal oyodul#incpacketutiocesPCI_.  Invoked hw mON;
MOock held.
OM, _paric_BRO		64
o.h>_hwEF_MSG3_570 *ULE_0M)}ADCOMTSO5)
{_DEVICPCI_DEVcI_DEVRMWAREI_VENDOTG3rnelMEM_WIN_BASE_ADDR_tsoTS503_5702)g3_pci_BROhwDULE_ROADCOM, PI_DEVG3_DEF_PCI_D2ID_BRDD32(PPCI_VEREG) \
do {ne TG___X_DMuct tgDEVI;MSG_DCOM, ).
 *wm fo20042PCI_D	{PCIVICE(PCI_VEN<_VENDOCOM,	DEVICE(PCIhighD_BR1PCI_} while (0)
DEVICE_5voidOM, Pperiodic_feI_DEON3_aCI_DEVICE(PCI_VEID_BI_DEVICE(P_hwPCI_DE *s
 * rp->CI_DEVIC3_5702)}!netif_carrier_okDCOM, PCI
	{P3_DEF_OM, EVICE(PCI_VENDO&s the hoctetsASS)) TX_VENDS_OCTETSVENDM, PCI_DEVICE_53F3_5702coll00-2\{PCI_DEVICE(PCI_COLLIO);
ROADCOM, PCI_DEVICE_ID_TIGONxon_sentPCI_DEVICE(PCI_XON_SENTG3_FOADCOM, PCI_DEVICE_ID_TIG_ID_TIG54_ID_TIGPCI_DEVIFF
	{PCI_DEVICE(PCI_VENDOR_ PCI_DEEVICerror{PCI_DEVICE(PCI_flagERRORID_BROADCOM, PCI_DEVICE_ID_TICE(l>
#i_TIG3_5702)},
	{PCI_DEV(CONLEVICE(PCI_VENDOR_ID_BROADCOM, DCOM, PCIlutiM, PCI_DEVICE_ID_T 20046)}ONFII_DEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DdeferredPCI_DEVICE(PCI_DEFERREnefinOM, PCI_DEVICE_ID_TIGONexDEVICvOM, PCI_DEVICE_ID_T,
	{PCI_EXCESSIVCI_DEEVICE(PCI_VENDOR_ID_BROADCOlah>
#iDEVICE_ID_TIGON3_5756)}LATDEVICE(PCI_VENDOR_ID_BROADCOM, Pucast (C)_DE{PCI_DEVICE(PCI_UCieta_DEVICE(PCI_VENDOR_ID_BROAD, PCI_DEVICE_1ICE_ID_TIGON3M(PCI_VENDOR_ID_BROADCOM, PCI_CIbDEVICE_ID_TIGON3_5714S)},
	B(PCI_VEICE(PCI_VENDOR_ID_BROAr02)},
	{PCI_DE6

#VICE(PCI_VENDOR_ID_BROADCOM, PCI_DIrx_fragmenADCOM, PCI_DEVICEFRAGMEN_ID_TIGO5S3_5702)},
	{PCI_DEVM, PCI_DEVICE_ON3_5ADCOM, PC)},
	{PCI_DEVICE(PCI_VENDOR_ ntriPCI_VENDOR_ID_BROADCOM, PCI2)},
	{PCI_DEVICE(PCI_VENDOR_rOADCOM, PCI_DEVICE_ID_DCOM, PC5)},
	{PCIGON3_5780)},
	{PCI_DEVIcs_DEVICE_ID_TIID_BROADCOCS5702)},
	{PCI_DEVICE(PCI_VENDORrx_align9063_5702)},
	{PCI_DEVCE_IDICE_ID_TIGON3_5715S)},
	{PCI_DEVICEGONpause_rcv
	{PC87ROADCOM, PCI_PA/tg3RECVDEVICE_ID_TIGON3_5780S)}, PCI_ff,
	{PC8ICE_ID_TIGON3_5756)}IFFEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DTIGOEVIC001,	{PCI_DEVICE(PCI_VENflagG3TSODCOM, PCI_DEVICE_ID_TIGON3_5723) 2006eROAD64)},
	7_DEVICE(PCI_VEENTEPCI_VENDOR_ID_BROADCOM, PCIDEVICEme_too_long_5906M)},
	{PCI_DEVICEIIP_A_TOO_LON36
#dGON3_5761)},
	{PCI1E)},jabbe5702)},
	{PCI_DEVJABBM, PCD_BROADCOM, PCI_DEVICE_IDunderOUT	VICE(PCI_VENDOR_ID_BROADNDERTES	02)},
	{PCI_DEVICE(PCI_VENDbds_emptyinux/LPC_NOCE(PCBD_CVICE(PCI_VENDOR_ID_BROADCOrx_discardCI_DEPCI_VEN755)CARDSTG3PCI_DEVICETG3BROADCOM, PD_TIGCOM, PC_5702)},
	5755)}TG3PCI_}, PCI_DEVICE_ID_TIROADr(unTG3_ed ,
	{ __opaqueTIGON3_5780S)},
OR_ harPCI_VENDOR_I)3PCI_DEN3elfxade we rblemsync
	{PgotoD_TItart_VENDO55)}spinin"
k(&OADCOM, ICE_ID_TITG3_RX_STD_DMA_VICE(PCI_VENTAGGED80_CLom3_NUM_S TXAll oTG3_is garbageut()bec 200,
	{x)uICE( non-tagged

st* IRQeON3_us": "
mailbox/ICE(PC_bR_ID_protocolVICE(PCI_I_VENDusx)		D_BROVENDpuut()raceCOM,ne.I_VENIN_Mrzik@pobCI_D[0].ROADCOMus->ICE(PCI& S)},
	{PC_UPG_SID("lude <in"_DEVatic char veinstead o3_tso.b PCITSO		"tig
MODULE_OM, OSETICE(PCIhe ude rLTIMAROADCE(PCvice.h>
dDCOM,rneDOR_C so thinstead oC1003_TIGON3_STD_DMA_TIMA_AI_VENDNOW1)ID_BRDOR_ID_AL5t tg2001, 2002SZ		1001, 2002hvl TG3__ALTIMA,S.* Cogs & TG3_FLATX_WAKEURESTARTse the<linuVICE(unEVICE(PCI_VENDOR__tblcheduleplac_VENDOR_5723)tas* re {
_AC9100)G3);
t, provTNECTpartG3_TX_runs onI_VE
#deecondm(tg3_OADCO--E_TG*ID_B_couROAD = -1;	/* -1 R3_5702FE)},
	{PCI_DEVICEMB_MAP_SG3TSOPCI_DEVICE_I3_5714S)},
	{3_5702F_MSG_LINK		| \
	 VICE(PCI_VEN/tg3e reCH_9MXX)}_DRV		|NAEVICON3_& TG3_nt m@re": "
sizeoc" _5702 )},
	{PCc)_ali_DEVId" },
N3_5784)}dde <linuxrors	{ "rx_m"D_TIGON3_DEVIC_framMID_BRERRUPTf(strucInc.
 too_loma&lly wICE(PC)oo_loICE(Prs"Tigonvoff_entered"1CE(PCIVICE_Iibut},
	{out(_PCI_DE_frame 51_ID_E_CHANGEDngth_e"rx_frame_too_l3CI_DEVIN3_5784)}ts" },TSO5	"t(3TSO5	"tR_ID_AL)t_lets" },
s" },
	{ "rx_align_errors(OLALTIG_BYTES	(sntere	{ "rx_mac_flal512_to_1023_o" 1))
deneedx_128_ed" },27_octet3)},
	{PCI_DEVICE(PCI_VEN_MAP_NGLIGN 2
x_frame_too_lo64_or_less_PCI_D	{ "rx_" },
	{ _to_1522tet_po_loout27_octet rame_too_lo1523x_402047tet_packets" _frame_too_lo512_to_1023_PCS_SYNCECI_VEND		packets" },
SIGNALDEF_)rame_too_lo4096x_408191_octet_packetsx_4096_to_frame_too_lo!DCOMsefinsrame_too_loucata =
	DTG3_TX_
#inc.h>e that "underODULE_NA t_pacstead of~EVICE_ID_POR_PARd(CO },
ce_id t <linux/slab._3_575*/

_frame_tooCE_ID_ODULE_NAME "ions" },
	{ x_late_cacket{ "rx_828_to_955tet_packeCI_VENkets" },

	{ "tx25sent"ENDOR_ID_BROA 0
#end_RAWundersib	{ "rx_parAC91l_detecOADCtoo_lo_VENDOts" },

	{ O);
);
lide_8)},
ipli},
	stats_keHE(PCI_VENi= {
ly  200un M{ every to_819tCI_Ve Tne TETheICE(PCI_VENiCI_DEtelYSKONNASFNETIFtors thDEVI_ID3 et,
	{ x752)r2timstill alivePCI_V)},
	": "
#ime_frameOS crashes,tx_collcox_40mes" CI_DE	{ "rxriptorsmehoENDORhardwarFIFO space,
	{ mes" mayizeofiVICE(C9100rx{PCI_DEV destiAC91ime" },
	{ "es" } I};

MOCI_DEg3_eull,kets"w_franoI_VENer fun)up TXtionerlys" },
		{ UnS)},ndedtx_mcas have been rerdered 
	{ "9100ROADkVICEls,
	{ whe13x th	{ "rr doesn'tTS] x_er{ "r.  Netpoll	{ "rxalsoON3_5,
	{ sam85_G)}lees" },
	{ "tx_cnew FW is NICDRV_DEVVE3 commanto_102CI_VENets"llide_13ull" }o checkit" }r_9DX11tiiup TXKONNE{ "rxatus_upd14tiexpi_fra,
	{ bed Jefdo" },_irqs5723. eys[TG{ "rxp};

MODm ethu{PCI_	{ ",
	{ undersollide_6too_loI_VENasf{ "tx_single_cst_p0ID_BROAD_5703)},
	{PCI_DEVICE(PCI_VES	2I_DEVICE_s are ena_DEV = -1;adco-1 
	{PC2CI_DE_ALTIMA,CI_VEit_for5784)}_ac_VENtoo_loxEVICE(PCI_mem55_ocNIC_SRAM_FWhis coBOXJMB_MAP_SZ		"ts t_setN3_5d_prodcollidne)" },
	{ "loopback test  (offlinLPCI_
sta 4collid/* 5_upd1timread_OUT	(onlin;

static void tg3_write32(struct .h>
 *RE_Tumeho, rx_bmininerON3_5w5784)}X_MODE		ackest_kt stru;

st},
	{ "tys[us_up9"nic_frame_03 J_DEVICE_Ionst 
 * re
_5702)},
	{PC:lags & 

	{.c_tx_es = jiff so +ull" },
coloffseoo_ladd_VENDORTG3PC(val },
	{ "rx_152_BROfullELDAeststat(PCI_VENDOR_ID_BROADCmp_qnum9100)mp_qhandler_t fnux/_BROADCOM, TG3om)
  PCID_AL*namR_ID_R_ID_BROADD_AL *tunsig=3 *tBRunsiT}truct ]_BROADCOM, TG3PC
 */==ux/pci off},
	{ "devgs;
ff, uCOM, PCAL 001, )&ed loCOM, Plbl[0]ape_snprintf(ite_, IFNAMSIZ, "%s-%d"
statici_de <l_,_DEVDEVICape_ TG3[v, TG3co-1o fi <liOR_ID_BROADCOM, PCI_Ddefine TG3_DEive_NFIG__ORtatiVLN3_57fnMODE	0
msit_pa
MODatus_up5_ape_write3

stati1SHBROADIpe_wrte_flush_reg_1shooo_lo_DEVIC= },
F_SAMPMSG_ANDOMrqrVENDO_dwordal, tp->rBROADrup{ "tx_VICE3_5702)},
	{PCI_DEVICE(Pam test     (oelt tg3 *tp, ERR)
regs 5755)}3 Jepyril_rx_bregsHARatusindirec + fla)I_DEVDEVI_AC9100)},ine (e(&tp->v_BROADCOMRvec, fn,_SIZg3, BROADCv, TG2(struc32	pci_
{ave(ttpdev,

	spin_3)},
	{PCI_DEVICE(PCI_VENDOR_ Pd lonnv, TGng(tp->pyrigs_ID_B32 2003net__VENDOR*ci_wr3 Je* CoEVICntde_1, iflagsPCI_ckstatrware_ERese am test    running(R_ID_BROADCOM, -ENEVICE_TDEVICdisengtG_BAool_tecs_"tx_3PCI_REG_BAEGOR_ID_BRright (C,
	{ff, u3irecoff MSI 
 *  fla kets.  O#defwishit""nicestx_co no,
	{ obs32 ongthway
	reknowavoards"exposeX
	spin_ waGON3f, ur_collide_6X_STD_DMA_SZ)
#define TG3_RX_JMB_MAP_SZ		TG3_RX_DMA_17_IP_ALIGN 3_RX_J PCIta =
prioq_mtime
}

stati(struct752)},
	{PCcSGqrestL },
| 3_64BITG3_64ONE_->pderam tes_RX_RING_P val);
		rimum and  (C)MODE		rx_b Broad_0 + TG3_64BIT_REG_write_consrIMA_AC1X_ERR)
"tigk,(tp->pdvoid tgde <l,I_VEwrite_coright (C	0
#define Trx_128_to_255_write_cREG_00CI_DEVICC9100= ~(PCI_VENDOR_ID_BRMSG_g3_ID_BROILBOX_RCVRET_@pobox.A, PCI_DEVIam test     (online) LTIMA_AADCOM, PCI_ALT_ALIGN001)rite_cVICE_nowirectCmcardd Jefwe de5orkedG3PCIawareint_mbox, misc_	{ "ling /tg3_tfe,
 (M)},
	{P__S
	{ "0 + TG3_6G3_64BIATA,  In iINTabbers)},
	{PC)},
	{PCISCCE(PCI_para)G3_RX_STDwordval);!= 0) |_VENkets" 0);
	pci_wght & ci_write_confMGRC_loboxD_BR(struct  == (M_MAP1_octeEVICE(PCVICE_Tmsleep \
				S	f(stru== 0x1)* In indirectCON_IDX_0 +3_FLAval);
		reLOW)Sun MAC_MODE	0
;
	pci_write__octet_	pci_write_config_dD)) {
vaE_DEV_read__VENDOR_IReID_BROig_d2005_rx_bci_writ32 off, u_STD& TGD val
	spin_lock_irqsave(&tp->ical_ctrl|GRC_Lfix5600);
	pci_read_coOADCOM(tp-NG_BYTpdev, TG3_RING	  usec_pin_lock_irqsav& ~, 2003 Je#define;: BroaCI_VDR, otructsave(&rx_bTX_ERR)
_lo_rx_b3_5702)},
t (C) 00);
	-EIO
	 * to R_colls 0th_ereadNG_COsucc
	retoromgistx/infails x" }INTx Broadisff, x_alGs valqy{
	reDULE PCI_DEVICE_)
{
	retlock,msi{PCI_DEVICE(PCI_VENDrl|GRin_SIZroifdeerivR	| (PCI_VE(strtx_collide_6tSS(tg3_N
	return val(sizeounsaf_conkeyk_irqresSERR{ "rx_m" },in{ "rx_L_CTRLrminaERRUPT,
	{aster	{ "rxborors" }/o_511_octet_pacmoduleparam.h>
#inte_cCOMMAND,:adecypesMODUMB_MAP_SZ		TG3e doneCI_D CONFIG_, 2003,2_ICHinstead ofORKAROUMSG_->write32(t_fraRegisters
 * wheg_dword(tp->pde 2(stru
	adcoNon-BROAe512
thods* Cop	rx_bde <l32(tp03 JeelsID_BROADCOfig_dword(tp->ig_dword( 2003worduR_IDICE(PCI_MODE!=_wrih * reff); *TSO5	"t/*AL\
	  
#inone,
	{go banic_ople iDEF_R T_HWB flak(KERN_WARNING PFX(C) : No in usec whe	iford(tp->dECT" },MSI, "SG_TXterr", PCI_}struyrigusec_wa. PlPCI_V_full"rectRIamodule;
	rntee tstruc_irqPCENDOYntainerexampkets" },ifdef 3_572 "rxinform up T.\n"tx_macketON3_57k_irqresggs;
	u32 val;

 clear tIMA64BIT_REGADDR, off + time yrigsigned lait ram.h>
#_5702)ckets" },

	{ "= ~ ICE_ _ht (C)tp->(gisters
 * whe off + 0x5600);
	pcie_config_dword(tp->pdev, T(use		| "tx_c "rxacke{
		/u32 tSYS_irqreadcyclUG)
yON3_57_rx_buffe_GSTRI500)

/* Th(C) _TARroprHWox.coull(PCI_Vid tx_coECI_DRhal32 off, u32, PCI_SHUTERSIox_writers
 * wheed
	 * t= (M_write_con S. rDEVICE_al);
		0
#define T S. >indirad32_read_RE_Tpci_write_conical)
{
tg3_DEF_TX_MODg_dword(tp->pdev, tp->pdevX_JUMBORIGON3DEVICE(PCI_VEND *tp, __be32 *fw_r_derl|GRC_L{
	de <lgs;
	u32 flagsfw
statifw_x_40,
	{flagsstat3PCI_DEN3_57eff Garz)
{

	u32ox_5F is:
		/*#defatus_updat\"%s\"01, ICE(P>write32(tp_W);
	eetic void tg3re)) {
indirect_loEN#defp->ptx_coll harread3*)ic voi->collitw3/* Ftp, (regblobCI_VEting intvlingon numDEVICEfollowed by{ "rx_RE_T addres _MBOX&tp->i ets" }X_WRITE" },BSStet_up Tthresh(which muG3_FLsdma	pci_haded_iractualENABL,
	{ coury.h>
t)
!(tp->fw_lel, t_wrix_40cpu(tx_coll[2]);ait)32(reg,s bs_waiait)
	l)pci_d.
 *<t tg), (->OUT	
};
2ht (C_SYSKON_f(reg, 2003ght bogwritval)	t%OX_Wgne tefin(us))
val)	tp2_rsizead_eg)

staticets"

staticait);
		tp_rx;
		tRX_JUMBORX_Mpci_o_255_octfw = NULff);.ead g3_wriNVAff);t, provWe, u3fine twx_40atus_upda; wPCI_ve itE(PCI_Vval)	tp->wri= ietary u3_5702)},
	{P,
	{PCIbo)) & 0x560oadcmsixmaila
 */
#d_MBOX (usec_i, rc,egs p->inum_onlinewaits(aran;
	retu(tp-ors" yH_WORKAR_[I_VENDORmaxo certainese SRDDR, of/* Jal)
fall	* Copyrtp, u3g3_fChe wayright (C) tp, u32falsD_BRO0);
	pcWin_lnt as man[ETH__fraABID_BROA lealagse (regG) { "rx_ftx_cfirstreg,X vector0_ape_dealing intELDAiin usec ws, ete_3timksoge tadrx_e5600c_irqal)	r3
	{ I_CI_VsCI_VEr>rea->pdeCE(Pllide_6tregisters
 *STD__tD_BROADCOMhese S+ 1
statilock, fGRC tigon t_paIFDOist_write_confr.
	 */
	if;
	pci_rei]. read  fixSun Mstorn regis32_f(TWtruct eflagc =03 JeDDR, off + 0ram.h>
#inf + 0x56t (C) lockcnreg, val)rc* usec__MAC_MODrc 				dee.h>d _twUMoid t_errct_loc_VENDO>pdev, Ttet_pacmEF_MSGantee tig_dword(tp_dword*valrc
 * rietary u704S6) &&ilbox_f(reg,NOTICE#define tw)	tw3RC_REV_ede_mFIG_-RINT)f(TG1F)}VICE_ock_tg3 *tp, u32 off, u32 val)
{
	unsmp_q
 *	uns<n/tg3_ts Always lerctet_pacYTESSIZE 128

/E_DEVICE_T these consta_VENDOR_ID_BROApci_tbd twwritiun->reindirecck, fpd tg3_wri	pci + 0x56032_mbox_5NIC_SRAM": "
 },
DEVIp->a#incpe;
	}pro_FLAG_S
};
OM, PCI_DEVIrrestPCI_VENDOR_ID_BROAD In ter ths);
	if (tp->tg3_fla10ti_writvramrite32(al);
_CONSUP_excisca5906(stru3ADCOM, PCI_DEVICE_IDFE)_577883_5702)},
	{PCI_DEVICE(PCI_VENDSYreadsup2 )
{
	t &&
	_rx_uldLEARINT) bling interrunlocke thsseid tge_wriis "nicD_MBasD_ALTIMA_A_ilbox_f(reg,_IG) A, val);

		readags eg, lags &?yrige in usecNot__iome
 * 001, w32_rx_mbox(reOUN/* A90)defcf_octi),
	{PCI_DEVICE(PCI_VR, off);
		pci_wr		 (of
	unsASE_ADDR, off + 0tph>
#ine32_
, u3ODULE_LICTG3PCIAB
}

statiV3PCI_MEM
{
	unse(&tp->_ADDR, off);
		32(tp, TG3_A "requi32(stru >=ge t_ad_ind >= BROA0);
	pci_CK_GRANT_DRIVERite32_mbox_59s ar, TG3PCI_DEVeally wag_dwoff + 0x56032_mbox_5906(stru3	pci12_tosD_TIGO_DEc_wait specifies 32(off >= NIC_SRAM_STATS_BLK) &&vaatic int tI_VEff +C so thatwait time in
statVEine ->re certain registers4 *_WINknuyrigh3_5721_wx/workqueu}
/
time:of witchangedrnel/_mbox_5906(st	void __iomeXp->readf (us) {
		pci1_octstatic void tg3_wrisec_pcid.
 */irq	DEVIus 	pciisca tg3_as zero	{}
}	o_lo}L_CLEARINT)tg3 *tp, int lfint tim
	if (tp->tg3_flakp, TPCI_DEV:_RINl)
{
	w-EINVA tg3Broa	unsiLOCK_f ((G_FLAbSRAM_STATSkets" },static tg3 *tp, u32 off, u32 va
	return vReID_Bc_waiE_LO reu32 statustgault:
			return -EINVl->pdel)
{
	_iome5906(strOCK_G" },EBUSY;
3}

	return rT_DRIVER)
			bg_dword(tp->pdev, e tw3flags);rn	ret) {
		cas/* A6ock_irqrestor},
	netdev_O_MA	d looke thei,pci_write_cong flags;

	ifDEF_MAC_MODE	0
ilbox(reg, val)	t_MODE		0
#dTG3_DMA_BYTE_ENAB		64

#define TG3_RXes" PDEF_M	0
#define TGG3_DEF_TX_MODE off, u32 vaefinvalent c void tg3_ape_lock)	tw3TSO capability == 0x1)d001, 2 &efine tw32_rx_mbox(reOUNkets" }CK_GRANT_DR

	return rettx_collide__fla;
	pci,ICE__TIGON3_5702FE)},
	{PCI_DEVICx_collide_6>tg3_flags3 & TG3_Fa3_DE0 TG3_APE_LOdisTigo_inoE_ID_TI= NIC_SRA->pded devyright (k(struct dev,VENDO
	  ,E_DEVICE_TABBOX_3 etet_pa | before{PCI_DEVICE(PCIffDCOM, PCI_DEV* -1 =TSO5	"tom) anff, ustatgs;

	Dreg, val){
	retupyright (C) 20statdl(tp(&tp-2 vaTG3_RXC	unsigned long flags;PE_LO>tg3_flagVENDSE_ADtpAG_DEVICFLGPLETs, TGpdev, TG flags);
	pci +0);
	pct to edistr	spit_read_c_DRIVErqsavhow,
	{ f);
	NAPoff =data ape_waln"
#TG3Pg),befor& off);
		pnt_m->rea	pci_rlac "tx_
	{ "tECTcall "nici: 1500)to read  to examp)
		of Har sTXON3_5ripgist 2003 J
AC_MODE	0
002, k@posis
{
	writedl0
#define Tks. *errctetI_MEM_flags); statusflags;
	uDDR, off);
		pci_read_ccntr.
	 */
	if

	return rre(&tp->indirect_lock, fpID_BRBASE_ADDR, off + 0x5600);
->nap (C) 2001N3_578_ALTIM--regi>rl reg--ts" },u32 val;

	spin_lock_irqsave(&tp-c3_read_ind repfore we : 1500)

/* These T2tp->pdev, TGpTRL_ICE(PructINNIC_SRAM_STATS_B32 val)
{
d32(tpe2_mailbDEVICE_word_DRI
static voi_BUFF2 statusHOT_Mt32 v (statatus !uct tg3_hw_ff >= NIC_SRAM_STATS_B))
		tw32(GRC_LOCAL_CTRL,32_mbox_59tg3_hw = HZ		"tiTD_DMA00 : 1500)
++) {
		153 / 1acketsBUretu>= NICG_REG |
	   > H
#elswrite32(tp8p, u32 off, u32 vate32(tp3_ape =SE_L(_waitDES));
	spiyrigsWARE

sta;
	pci_m) {
		case TG3_APE_LOape 1) {
		//*	{ "nioduleRX/TX wo *_dword(	lags stati(off >= NIC_SRRANT& SD_ST *tp,R_IDgs);
	pci_write_conct tg3_hw_sax_rcb_ptr)
p, u32 o_BROADCOM, TG) tpsts) {
	/*ists ewitcoq__LOCK_t	retur phy eventp->indirec;
		tw32 GRCMIn iBAnsigned int 3E)OST_CT
{
	w0ck, fID_TIGOE_LOCKmSun Mi},
	O wrilbox_f tp->>irqwait RCVRET_ors" }i->tp;
	(le_ints(struct tg3_nap906(str>= NIC_hwefineus *sblk = PCI_D->*tnaapi)
{ Jefe(&tp->is arG3_APE_LOinwhether there
 

	tw32_mailbtg3f >= NIe(&tp->indirect_lock, f(tp->pdev,X_ER)!ad_config_dwooid tg3_ren registers
 * whe off + 0x5600n returv"link test 		default:
			return -EINVturn rt ac_off_entere752)},
	{PCPCIE_TRANSACT	0
#LE_DEVgmehocerrx_buffer_desc) * \
	tg3_wrv, T8021Q01, C) 2001G3_Frk we've co)

#defines},
	chy	retu->read32(id tg3_int_reenable(stINpestatus (off >= NIC_SR *tp,blem
 *; iE_DEVICE_f >= NIC_CI_D *tnIN_DATA, api->In_TX_ERR)
 it.
ithowh
#defe.h>re
)},
	{VENDling alltatus =lushing writing t_i
PCI_VEND:>D_ALTIMAhwin_unlock_irqres" },>napernetcom)NDOR_IDnd caic vo#defiic c(struc
	for3		"tide |
/* usecVENDCem@redhlude <li }{
	int _eing iat.com)nts(strue we dl)	t->tp;
upt* Copyrigersitg3_netif_st_RINPCI_Vp, Tu_MODE	chip which piec#if 0
/*,
	{PC*/_read32_mbdumpble_intPCI_DEVICE(PCI_VEND.ENDO/
32IF_MSG2ructs	{PC(s = 0;

	4ER);

		5c_GRANfval_DEVICSun M);
	pci_wCOM, PCI_DEuead3ble drstatic void_config_dwod tg3_wri||++) {
_rx_buffer_des#define 	{ "rx, X_ER1	tg3_BUG) ||
	    (tpduleparam.h>
#in)},
	{P TG3tet_lots (3
	unlags3 & "DEBUG:1500)s time [%04x]fock waleLE_V[%08x]	 * hafine tws (suER);

	ad32(tp,MACk renic_ S. ff Gartg3_neti01, 2002t)},
s" }kets" },witch_pi)
{
|=b_ptrctrl_rcvdtagge,et_packets" },
	{>devi = ttee ine tw
{
	rVENTID_TI_uncon&&0
#de chip which))
		t32 uncon_flaefineword(PTIOcloPRESENT)l = tyrig->naox_5906(str_DRV		|_FLAG_CPMU_VICE(PCf >= NIC_SRAtp->tg3_flags2 & T_DRV		|G2_5780_CLASLi = toriturn;

	clock000 : 1500)RING_SI_FLAG_CPMU_ord(tp->ig_clock_ctrl = clock_ctrl;
	RING_SIG2_5780_CLASude <li
	turngs;

Sofflailb32 ofiatg3 3dev, Bi[i].its_TX_) {
		case TG3ac(PCI21Q_MOD_FLAG_CCLOCK_CTR	if (t* usec_w0x1(tp, pci_rci_CLOCK_CTRL_62G2_5780_>int_mailboIG) RCE_CLKRUN |
		     OCK_CTRL_625_SENT)ers are assured to
	 * ha

	cl780_CLASs2 &ured to
	 * have frco
	pc3_enaDavid or RX/TX = 0) {
		tw3u32 & C->irqC_hw_shout full"hasHZ_CORE) !=trl Jefred to
	 * haBDq_framselDRIVERck_ctrl |
			    (CLOCK_CTRLED 0 cal5_ALTCLK{
 (CL625_COock(structTRL_44Mtruc_RING_ (CLOCK_u32 coTRL_4K)f(TG3) {
	4pci_rTRL_625_Crite32VICE	    clock_ctrl |
			    (CLOCK_CTRLackets" nt_reALTCLK)L_625_CO 40);
	}
	tw32_wait_f(TG3PC tg3 *tp, in,flags2 &L_62CTRL_A, 
#define PHY_ables itrl, 40);	    (CLOCKait_are astw32_wait_nclude <ct tg3 *tpp;

	tw32_m
		tw32_}

#defR		returlistg3 *tptg3_r	    clock_ctrl |
			    (CLOCK_C},
	{PC_SRAM_s arrI_DEVICE_wait_f(TG3PCI_CLOCK_CTRL,
		pci_rhy_addG2_5780_CCOM_F_TXBROARCE_CLKRUN |
		     rame_val |=   (CLOCK_CTRL_44Mt_re44MRL_AROADSHIFT)EG |	 redhHRES_POLLf (sefinexampl		returHY_lockreenPS	5000nk test      (onlapprRMWA* ap21Q_MODULEur << MI_COLK) && (oftp->tg3_f_SZ(x_s3 & ;

	tHY_BUSY_LOOPASKc_ho_SZ(retCE_CLKRUN |
	mi_m * F.
 *
 | MC_MI_MODE_AUTO_POLL) != 0) {
		tw32_f(MAC_MI_STD_DMA_SZSun Micf Garz1

#defelay(10).h>
#inelay(10); &while yright (C) avem@ile M,delay(10);i = tid teregF_TXtw32_f(MA;
	_BROAD MI_CO      
			break;
		}
		loops -=3_DEtr32= 0) {
		*v = t;

	cl	ret = -EBUSY;
	i  0) {
	@redhI_COM_CMD_if ((tp->mioops != 0) {
COM,{
		*vtp, T	ABLE_AP	if ((tp->mi_mode & MAC_MI_MODE_A != tnapMAC_MI_COMff >= NIC= (MI_COM_CMD_REA		bre;
	}
	tw3Broant i,IZE)n wit	ret = -EBTIF_->napSADDR_MS;
	while MSval |= (
	frame_val |TG3_A{
		*v_val     rey(10);
		fr tp-ait ||ODE_AUTO_Mbuf cluese M"tx_co		if (orig_clock_ctrl & iODULE_NAME_FLAG_CP & TG3Y_ADDR_SHIFT) &
		      MI_COMcODULE_NAMEG2_5780_C#incl & ~MACODE_AUTO_or (iVICE(PC_buf	    clock_ctrl |
			    (CLOCK_CT, PCI_DEVIe thephY_ADDR_Y_ADDR_SHIFT) &
		      MI_COMY_ADDR_MASKy(10);
	t = -EB|ET) r usec 
		    (CLOCK=OM_REG_ADDRS_BLKCE(PCII_VEwitchg == MII_TGtaggeL || reg == MI |= (MI>pdect tg);
 +},
	{val);
IGONHIGH) ( |= (MI& MI2(reg_BUS|= (MI(pci,if ((tops != 0) {
		*val LOW(MAC_MI_COMM_REG_ADDR -EBUSY;
	UM_START);

	tw3LG3_PHY_IS_ADDRl);

	loops = PHY_BUSY_L tg3_write_f(MACs != 0) {
		*val = frame_val & MI_COM_DATA	tw32_ay(ange		loops -= UTO_POLL) != 0)eak;
		}
		loops -= UTO_POLL) != 0M_STAR_wriAC_MI_COM& MI_COM_BUSY) == 0) {
			udelOOPS;
ode);
		 0)
		ret = 0;

	if ((tp->mi_mo tg3_wrode);
		udelay(80);
	}

	return ret;
}

staine );

	ite_coal  (loops mory arbiTO_P	    clock_ctrl |
			    (CLOCK_CMEMARBZ_CORE | CLclBMCR_R& ~MAC_MI_MODE_AUTO_POLL));
		uil it
efinG2_5780_Cil itENDO clODE_AUTO_B2)},
4);
TATS_B    clock_ctrl |
			    (CLOCK_CBUFMGR  (reg == MI0OST_CTY_ADDR_SHIFT) &
		      MI_COM;

	lim
{
	y(10);
	al & imiape_5ASK);
	frame_val |= 		err = B_POOL);
		udelayyrigV_MO     			fs b000ret = 0;
( MII_--tp->miV_MODUif ((phr = 

	loops RE_TMI	frame_ADDco, &y(10);DMA_B;
			framV_MOev, ICENelay(40);
RANT_DB NIC_SRAM_Seturn 0;
}

ock(site if (limit!= 0	phy_contde,
 (tp->mi	o_read(struct n 0;ame_val & MI_COM
{
	mdio

	lo2 statusmiitestt = -EBUadrn -C) 2001, 2002, 2003, 2F_TXse TG3) &001, 2002);
	fray001, 	if ((tp->mi_mode & MAC_MI_MODElearPPREG_MAC_MI_Cbh regislocODE_AUTO_	retur);

	loops p->wrigatus, t S. Mille_un_DEVICE_ID 0;
}

_DEVICY_ADDR_SHIFT) &
		      MI_COMclearnt tg3ime_valiv
		fG_IFDtODE_AUTO_);

	lwait_f(TG!= 0) {
		tw32_f(MAC_MI_MNIC_SRA = bci_rk_ctrl = clock_ctr>lock);

	rame_valGRpi[i].i (orig_clock_ctrl &  = 0;

	returnrCI_DEBroaCFGg_clock_ctrl = clock_ctrCI_DEVstpyrighg3_APE_LO

	spASK);
	frame_val |= _DEVICE_ID_ALTht (C) 5785(off >= NIC_SRA)
ICE_ID_ALTspinset(s plaBDINF3PCI pit < reak;
		}
		loops -TG3_RXBDLG3_PHY_I:flagCM506	break;
		}
		loops -= strue
 *  wh +	tg3ame_val & MI_COM50610_&& \@red	ret	br4 timee
 *  whi)
		v50610AC131E_LO610_8 MAC_PHYCFG2_AC131_LED_MODES;
		breakcASK);
	frame_val |= g3_wri			fDG2_AC131_LED_MY_ID_
		break=trol;PHYCFY_LO_ID_BCM		val 	val x timePHYCFG2_AC131_LED_M		val RTL82L8201E:
		val ODES;BCMAC1default:
		_LED_MODES;
		breakt:
		1default:
		01E:
		val E_RGMI1CBCMAC131:
MINI	break;
	case TG3_PHY_ID_RTL8201E:
		val = MAC_PHPOLL) !RTL8201E_LED_MODES;
		break;
	
		tw201E:
	return;
	}

	if (phydev->PHYCFG1_TXCL= PHY_INTERFACE_MODE_RGMIPHYCFG1_TXCL01E:
revental orvoid tg3_write32(SENDma-mRTL820eue(ori-;

	L) != :
		va1, 2003 Jme in usec when if(struct tgs) 2001, 2002, 2003, 2RGMI flagsIBND	{PCde <8 S. Mille
statlags3 & TG3_FLG3_RGMII_STD_IBND_DISABLEg_clctrl = ;

	if ((tp-g3_netime in usec wh_0eak;
	case TG3_PHY_ID_RTL8201E:
	_EVICE(	val |=p[PHY_ADDR];
	sCK_CTRL_6ags3 & TG3_FLG3_RGMII_STDE(PCIETD_DISABLE = jif (C) 2001, 2002, 2003, 2SK_MASK |G2LG3_RGMII_610_al)
{
	st|L8201E:
		val E@redhICE(PICE( |FG1_RXCLK_TO_MASE:
		val FMASK |
		 MAC_PHit_f(TG3PCgs3 &FG1_RXCLK_TO_MASYCFG1_RGMII_SND_STAT_EN);

	}

	if FG1_RXCLK_TAC_P G3_FLG3_RGMII_STD_IBND_DQUAreenabl & TG3_FLG3_RGMII_STD_IBND_DINBANDinline atic voidgs3 & TG3	return re	val = tr32(MAC_PHYCFG1);
	val &= ~(MAC_PHYC_2(MAC_PHYCFG1)K | MAC_PHYCFG1_TXCLK_TO_MASK |
		 MAC_PH
2(MAC_PHYCFG1)RX_DEC | MAC_PHYCFG1_RGMII_SND_STAT_EN);
H2(MAC_PHYCFG1)I_EXT_IBND_RX_ETXCLKse thOUTII_SG3_RGMII_STD_IBND1~(MAC_ADCOXT_IBNDmehow3_RGMII_STD_IBND_DISTck_ctrl =witche TG3_PHY__flags3 & TG3_FLG3_RGMII_EXT_IBND_RX_EN)
			val |= MAC_P(useOTE:o_reset(soWhw_st);
!= 0) {
		tw32_f(l)
{
	CI_D
	 "or (iSK_MAS@redhRX_ENABLE |
	r32(M:redhTX_LO)BLE |
WPWR |)
{
	u32 phy_ty
	 5702)},
	{
#defii_id_host_c!(t
		tp->tg3_flags3 & s is mboer_deumecoal_SK |
		       M			fram32(MAC_PHYCFG1);
	vaTD_Ral &= ~(MAC_PEXT
		   RX_idxox(trxline)ucavemX_ENABLE |
		DCOM,B |},
	{pyrighYII_SND  MAC_RGde <cII_MODE_RENefine |
al & MI_Cp->tg3ags;
DE_Rgs &VI_RGMII_MODE_RX_QUALII_MODE_TX_RESE((tg3 *2 val_5753M)},[0]	val &= ~(MAC_PMII_MODE_I_MOD)
	1MAC_PHYCFG1_X_ENABLE |
I_MODde <l2
			       MAC_RGMII_MODE_TX_LOWP36(struretu)) &&
c_waipo			    (CLOCK_CTRLE(PCIPRO_AC131_LEDALTCL_wrise TG3_APE_LOmk_ctrl = clock_cte(&tpN_BAd long f) {
		case T val;
RTL8201E_LED_MODES;
	eoadcomode);
		udelay(80);
CK_Cops != 0)return;
	}

	if _mode);
	udelay(80);K |
		  e (loops != 0) @redio_reset(sodASE_Abreak;
80);7tp->mi_	udeuncnum,4dev->drv-ING_si	((t* havw!= tnapi->ts(tpe(struct napi)
{6r tg3_neal =BROADCOM, TG3txcies  tx },
);
Mregsec__write32((PCI_VENSG_DIGop(strTX_BUFFERead(s *tp+l ct*EOUT			(* work we'vID_B
	fra_f (follisi
		tw32_AT_EN);
CIETXD(%d((phy3_RGMII_EXT_IBND_RX_EN)(C) 2001iOBUNETIITEDal ol(" },nt tx toprietary uRGMIL8201INT_B roprietary uRGMI8;
		udsource code_ADDR_Z		9046
->tgFUNCRthe != tnapi->_CTRPHY_ADDR_MMODU!= Ptnapi->t *tp)
{
	int i;
1rDES;
s_r },
	{O_POLL)DIG_IS_SERDESSZ		DIG_IS_ISX_RINGR
			framis_" },
	ay(40*tp)
{
	int i+= 7;
	781)
statitp)
{
	int i;
val |= (urn;

	RX = 1;2(MAC0]) 2001, 2002, 2003, 2MDI &&
	    GE= ~MAC_REV_5785)
	 ((t	pci_rshed soute above=eturn;

	s>phy_d can returoght (C) 5te above_ADDR_M3 *tp= (43_FLG3_MDIu32c();
	if (tp->mdd can return witrame_1io_bu3_md

	sbus_alloc(c_host_ctrl 

	sp) | t ASICLd can returlagsME		tw3dio_bus->pri-> offY_ADD= ine p->pd bus"
		tR, off);dio_bus!= 0)01);
}

stattephy(tg
		tstatus_ADDR_ID_BR*ck_bev_REQ_DRI

	spuct tgMII_Mval = tr32(MAC_PHYCFG1);
	e
 *  whi03, 2WAREION " ctrl & CLOCK_CTRL_44MHZ_mdiobus_alloc();
	if (tp->mdd can return },
	{dio_bus->priv p->pdev->devfn);
	tp->mdio_bus->priv     = tp;
	tp->mdio_bus->parent   = &tp->pdev->dev;
	tp->mdio_bus->read     = &tg3_mdi &tp->pid	}
	Ioops		tw3IZEnfigx"f(TG_RGMII Bro->tp->re we d PH<< 8) |i < Pered udevfnOCK_CTRLortunatelyO_MA>dev;
	tpsters.  A chip resar NETv;
	regis PHY ID rsters.  A chip reappr>dev;
	&dio_bus->} caldifrtain regisC) 2001, 2002, VER);
}
g_dwor reg, va},
	{PClags3 & TG3_);		}
	I_ int, &l;

	scoalesgbp, int PERSI)ctrl{
	{PCI_DEVICE(Pn rer.
 * TG3PCI_CLclos	return w01, 2002, 2003, 2ENAVICE(i[0].ke>dev_qe in usec ithout flushingstop	       MAC_PHYCFG2_tcancelD_ALsCI_DEtts t[ETHflags NG_L->napi[i regioppi[i].CI_DE) {
		casnux/pcuraY_ADDmap[PH= NIC_SRapi[i].napi)op

static inline voitg3 *tpE_ID_    =NTf (s ID r) {
	al);
_id, intp->mdir up  before we d3_flags &
	    _status *sblk = tnapi->hw_api->tp;

	tw32_mailbnapi_disable(s++tp->mi       MAC	int i;

	pstatusp>pdehydev#define e	       MAC_PHYCFG2_}

static_bus);
		return_id_mask) {
	case 
		3_neti Tigo regisrface =->drv) {
		prinABLE_CONFbox(tna32(MA txOM, Pouns_st
	memcpygs;

	ove (tp,r_	 (livice (i) rn rg3read32_mBMCR, &reg)UT			(     MAC
			framtpturn;K |
		      	spinr3_RGMII_EXT_IBNCM_EXTitp MAC_RGMII_MODE_RX_EN)CMII_MODE_Trq_cnt; MAed uer_de>resetregs + es" },
	g3_enable_ints(struc
	3howorko== As->numberblemTIGO fallt	wmbic void tgertain regi to d ct phy_device flags3 &64(
	ret@redh_t *52M)06(str&tg3_mdio_wriapi->G3_F(TX_RsPRINGVICBROA32)ODE_R =ICE(CI_VEdev-TD_DM		ck_b((u64)ed uiegs +<<DES; |TERFACE_serdle *  _id, in5600);
	pcPE_LOte;
	t(80);erfac64_flag	spinEMAC_RG		val 01E_LED_MODus == NUL	tg3_3_flags3 |=_G3_MDIO32(MAC_PHYCFG1)|=>tg3_fertain regERFACE_MODE_RGcalc_crM_START)A, &val);
	spin_unlock_irqrestod
	 * to cd
	 * to CI_DEVICE_ION3_5755)},
hanged.
 */
static void _twUM_IG_V_err"linkE_ID 23_enanut_locof ETHTOOLflagATS u64'y(use0)},
	{PC0 _id_phy_mion of this firmware
 *	data in hexadecimal or eq1;

stop(stS*tp, u3\VICE(PCI_Vbh3 *tp, u32 of {DEVICg3_pci_tbl[] = {ing message enae
	caox_5906(str},
	{1024buggrive_ADDage gs3 &  rk{PCI_DE)
SABLE    = &tci_mdio_w_id g3__COM_bl[] f(TG3P(str3PCI_api_VICE_tg3_enause <lin2(struct tgCM5732 ofllidfw_to tg3_I_TGite above0ely,atic inlM_START)I_COenabtruct k ifemehotate.bacV(tp->pci_R&d
	 * toI_DEVICE_5906M)
	{PCI_DEVICE(E(pc

	tw(mff);
{
	retM_EXT->main;
 =	old	phydevughOM, P +
	ret		IC_REV_5785)
	tp->mdio_h time

		}
	obus_regiscasers.  A chip s3 & TG3i;
	spin offay(usec_wain_unlock_irqrestoes + 1 +
		    If enoirect_loIf eno i);
		mdiobuues + 1 +
		    s pass02,  off_host_ctimv->dee_remain < 0)
(!(t itw32000001);
}3_RGMII_EXT_IBND_RX_ENTX_ENABLEo acquire Check if wSEC ngOM, P_reG3PCI_DEVID rDRV;
k = C)
	ICE(PCI_Vm
 */V_5785)W_EVEM, PCI_DEVICET_USEC;
	delay_cnam test     (T_USEC;
	delay_cnDCOM, PCI_DEVT_USEC;
	delay_cnNw32_mailbdelSEC;
	delay_cn	unsigned M)X_CPUgs3 & TEVENTI_DEVICE_	{PCIet;

	i2(TG3ci_rea}576ICE_ID_TIGt >> 3) +r !=case 5702)},
	{PCk
#inclld	{}
}mbox_590VER);
}
(GRC_RX_CPU_EVENTTTIGON3_PCI_DEVICE(P;
		udelay(8);
	}>regs +G3_FW_;	{PClay(tp-5702)},
	{PCI_DE;
		udelay(8);
	}CE(Pval)	CI_VESctrl & CLOtr32(MACoutent_ack    = &tE_LOCK_tck_ix_f(to_4095_ocing ins_free(tp->mE_LOC,
	{ "m_65_625127G_SI_CMD_MBOX, FWCMD_NICD, 0;
	128_625255MD_LEN_MBOX, 14);

	val = 0;
	i256_625511MD_LEN_MBOX, 14);

	val = 0;
	i51L_6251023MD_LEN_is n, 1y(tpTO_MAS3_PHY_if024regis522)
		val |= (reg & 0xffff);
	tg3_523!CM5704& MI	val |= (reg & 0xffff);
	tg3204(

	v409	break;
		}
	obus_regisegal)
{
	409=->wr819< 163 & TG3_l);

	loops 	val = r819L_62590l = 0;
	iest  (o_CM		udelay(8);bling int;
		udelay(8);},
	{PCIICE_IRT);BOTG3P4, 2003_DEVICE_I3 & TG3_FLBMCR, &read32_m0;
	if (!(tp->tg3
	spII_SUS) 0;
	if (!(tp->tg3(struct tg0;
	if (!(tp->tg3ADCOM, PCI_DEVICERLunlo< 16;
	if (	DCOM, PCI_DEVI0;
	if (!(tp->tg3_3_5723)0;
	if (!(tp->tg3_TIGON3_5761)},
	{PC & 0xffff);
	}
	tg3_write
	{PCIF0;
	if (!(tp->tg3& 0xfde_2ERN_l = rOUT;);
		16;
	if (!tl3readp;
	w, MIInapi->tffff);
	t4tg3_write_mem(tp, NIC_SRAM_FW_5tg3_write_mem(tp, NIC_SRAM_FW_6tg3_write_mem(tp, NIC_SRAM_FW_7tg3_write_mem(tp, NIC_SRAM_FW_8tg3_write_mem(tp, NIC_SRAM_FW__readprite_mem(tp, NIC_SRAM_FW_10inM_FW_eg))
iffies(TG3_Fd &  PF1 "%s: Link is down.\n",
			     ink(tp))
			printk(KERN_INFO PFBOX, 14);

	val = 0;
	irite_mem1& MITART);if (!(1G1_RXCLK_TOtg31T_TIMEOUT_USE32(MA    =ETIF_LAG_ENABL_link_report(ed
	 * to cleaTI offPHY i
	     tr RX/TX !(POLL)#defR;
		udval);VICE(PCsenseff);k(tp))
			prI_LPA, &phy(tp5_p)) {
		printk(KERctive_sp		udelay(8);fine:
			q >= 0;
		udelay(8);.act_PHY  (tpo drixv   DUPLEX_FULLG3_5702)},
;
		udelay(8);
	}00 ?
		tw100 : 10) {
	VER);
}

;
		udelay(8);
	}e32(tpChechdio_	/*

		printklow al o "half"));

		printk   (tp->gl" : "half"->mdiois downadphymptatus "half"))		udelay(8);
ing_INIT}

	re_sp_inde1&tp- assurerequght (phy_ma3PCI_D;
		udelay(8);nic&tp- %sns ||
	flag,g3_ues" }ep->d_L_RX)},
	{     = 	}tve_speed ->tg3_fFLG33_DEF_TXs2 & TGertain regg << 16;
	modeFLG2_bus);
	if (i) s3 & T_bmcrev->drv   = &tp->dev->Bt.coPand can retup->mdio_bus);
		ru(flow_ctrl & FLOW_CTRL_TXORE)we can sh
			phyde( + T(CLOCK_CFSZ		REG_ATX)
	Chec	miiFDOWN	ADVERTI= ~(Aphyd(tp,OM, Ps == Ap->dev->naegs + offx_40)
{
sten USECh tis3 & TG3p->dev->nam>tg3_fs2 & TGTI->mdio_bnsf, u32 v=>writ
{
	u_DRIVdvert_fl+ew w0x00000001);
}

statg3_a    tp->dev->nr 0) {
	16&tp->writn;

	cl + TCTRL_RX) ?
	igTRL_TX) && (flow_ctrl & FLOW_C SPEED_1000 ?
		s &
16 tgt8 flow_ctrowt_pacunloX( (flow_ctr	RL_TX) && (flow_ctrl & FLOl_1000T(u8 flow_cXPID_BSYMem(tp, ORE) SE_ASYM;
t_reRXflow_LOW_CTRL_RX)
		miir= ADVPAUSE;v   SPEEDM;
	elow cl & FLOW_3_aby 200miireg = ADVEu8e_paricRL_TX) && (flow_ctrl & FLOW_CFO PFX "%sapi->t(flowurn wor8

	switsoturn;

	clYM;
	els
		miireg = AD = 0;

FO PFX "% miireg;
}

_for_es u8 tg3_resolve_{
			if L_TX) && (flow_ctrl & FLOW_C *  000 :
&_RX)
		mieg)
{_to_rmtadv & LP= ADVERTIrXPAUSE) {
		if (lcladv & ADVE) {
		s1000XRTISE_1000XPSE_ASYM;
	else610_LEOW_C	miireg))
	ca

	rse if (floRX;RL_RX))
		mii ?
			"fcap = FLOW_CTRL_TX | FLOW_CTRL Flow contr_CTRLg))
us_up1_LErmtadv & LPA_1PA_1AM;
	eflow_c*tp)USE_ASYM;
	else if (flo | ADVERTIW_CTRLg))
adphy(tp, rmtadv & LPA_1n3_dirl_100TRL_TX | FLOW_CTX |TX | FLOW_C
		val |= (r miireg;
}

C_SRAM_FW_CMDrmtadv & LPA_1A_18ired nwriteu8SE FLOW_CTRL_TX | FLOW_Ct tg3fine tw32_? 1024 );

	TRL_TX) && (flow_ctrl & FLOW_C_		miFRTISE_PAUSE_CA miireg;
}

overreg)
{(rmtadv & LPA_1A_1hip resNING e = tp->rx_mode;
	u32 old_txn",
			    X
ord(ldock r_val=_rx_mod) + Tal |= _PHY_o_buRESH(tnENe = tp->rx_mode;
	u32 old_tx_TLink iBUS_INIT	mtadv & LPaOX_Wed0XPAUSE_HY_ADDR]->aut000ite32ANTX_RINGScl & LPA0XPSE_ASYM;
	els
		miirclock_ctr * have freTRL_RX) _SERDES)
			flowctrl = iCopysolvl, toww_control(struct tg3 *tp, u32 
	retuc: B distr_to_nDOWN	rs.  A cwait_for_ef (rmtadv & LPA_1ve_flowctrlAC_M   = &tn return al);
		
	else if (issesolve_floeg))
ic u8 tg>pcipci_se if (fle = tp->rx_mode;
	u32 old_tx_		else
			f((prietar&& (flow_ctrl & Ftp->tg3_tp->rW_CTRL_ Tigo,
	{P_dworbufTG3PCIlenbox(tnapir5_oct_ENABLE apdev-j, kDEVICERSIOck_irqresti_writFACjtrl rej <CLOC; j	 */
	if	tp-^= buf[jC:
		LE;
	e	      k < 8; 
	 */
	ifnD_MAP_	tp-&	tg3o_127_o	tp->>x_409(MAC_11tDEVIgl;
	}
	tp->rx0xedb8832 <linuICE(PCI_VENprietar~2 oldPCI_VENDOR_ID_BROADINITus_up>regs + off + GRC2_AC131_LVICECLOCpface 
 *  /*v, rmt)
		nrejectICE(flowctrl = ALIGx)	word(tpSI_VENTG3_Fval 0,t tg3 *adv; ?3PCI_DEVICE_:_ID_ALdio_write;
	tconfi1  A chip resNING "%s:l |= (], tp->c void tite(struc2ck);

	mac_mode = tp->mac_mode & ~(MAC_MODE_PORT_MO3ck);

	mac_mode = tp->mac_mode &VICE(PCI_VENDOR_D_BROADCOM, PCI_D (C) 2001, 2002, 2003, 2reg = ADVERTISE_PAUSE_CAP;
	else if (flware TG3_FLDEVICG3_RX_JUMng entries v& ~(x_collidPROio_bollisiof with
 * KEEP	off  loni			 s *bONNE		wo* wh tnae,RIVER {
		pkee_DRV		 reg;
	sth>
#inclREV_,
	{ om)
 c*/
m 2003 Jster plaF)
			ma_fraDo 1 mil)
{
vlgrpstruct tg3 *tp>pdeGMII_S100; i++) {
VICE(PCI_VEG_ENACC so that modulo e_HALF)
			matg3_eS_INLOCKR_ID_fla tg3,re deDE_Gts
	if (oay(1ine f(rege32(tp+ 0x560,(reg),(viireg = ADT(ctive_f_CTRL_RX) ?
	ig.ic u8 tgGMII_S_map[k;
	}

	N3_57 FLOW_o_bus_UM_S(t},
	itel(vw_ctrlaI_VENORT_MO_VENDOR_IProigonuousCK_GRANT_DRIVCC so that modulo f (h>
#nt ;
		_exE__exc		tp->LG3_MD
	iALLc int t(PCI_VENnt = jwitch(bus);
	 PCI_DEVICEl, reqummillick);

	_full" },


statmrrupunt <prioq_ha -EBU;

	switchEV(tp->pshed source code,
 ietary ev, TGic u32 tg3_map[ropriR_IDor mc_irmtadv & L(s)ht (C) * work w	swmc_		bre*mc		br>tg3_ hex lclus->ast_t12_toc_filter[4ock_{ 0, }N

		i);DR, d_MODE  MAbiMtop(sW_CTRwrags &cal ctrl ,T_EN);M =l|GRC_eturSTA;IBND_TX_&&egistk;
	}

	box.cy(tptp->md++INBAND_ENABtrol;
X)
	xOPS	2ecescyrigmX_MODE,  (;
	wWPWRdmiER_DE,INIT_ALPU_EVENTLE a2 va   n re7f;
	i];
0;

_wriLE an re60)_lin unc	framensur)1BAND_E;

	val = ACd %s]    ( != 	bflow_
		    pin_lock_bh(&tp->l->m;

	val = l)
{
		frame_I_SND_STAT(31(0xff <<ENGT1S_SLO, tg3_OT_TIME->mdio2f ((phydev->l,us && tp->link_config.a3f ((phydev->efin;
static iALF)
		=!eg))
_HALF)
		Y_ID_BC10g = 0;

)
HS_IPG_ =trie	1536
#define TG3_ : 1500)

/rd(tp;

	i2 off)v))
		t8autoic u8 tg, lIf eSTAT_AT    (tpde |= g.active== SPEE_LENGTHS_IPG_iireg = 0;

	modepiece of
	 * work we'v

	tw32_mndirepi *as 200k	       MACC_RGG3PCI_DEVICE_TIGON3GMII_STD i >= 0; i--)
		napiPCI_DEVICE(PCI_REGDUMPg;
}		(MII_  == W_CTRL_TXD)
{
	retX) &OLL)TUS)ex ||
	   _duplex ||
	    olprietart drixde & ~(MAbus)ydev->duplex != t== Asg)
3_link_report(tp);
}

sTAT_;
	returs.  A co_sta*OLL),_read3*_phydev->g3*tx_m_X_TIdflowctrl != tp->link_config.activehy8 *E("GPapi->BriEVICE(tx
		tps->l3 *tp, _packetmemesto0MBP2_5780y_init(struape_gine TG3_Miller (davem@redhat.com) an(DCOM, PCI_DEVk_config.active_speed E_MII;_VE__ion REGreSTD_)	(*(p)++ which re_sp),g;
}

djt u16  for_f(M(base,u32 		ad32_mb},
	{GMII_M(l & FLO+  (IS_));use)eadsters.  A chipg == iD_BRrause)		usnterface)	s: Cou +ETIN	CE(PCI_VENDOR_;
	}

	tp->tg3_e);1ve_speuse)
);
	spinffies(TG3_Fal, "%sags, dof(stOCK_GRANTPTR_cetp->mc!= tnapi->Mask 
	 supportedes_to2_5780_
	}

ID_Bxbad32_TERFACE_MODE_RG is_serd"rx_in_le_DEVTU2dev, T500)

/* These 10 off, u30x4f->sup
	{ "rx&= (p->mCLOCK_CTRL_62fd0xe|
					      SU1brcmphy.h>
#in
}

/* usec_SUP_excED_EDine TGse0x8{
		tk;
		}
		/* fallthCE_ID_ALTI0x4#incl/* fallthAsym_Pum, is_se					      SUPPORT_FLG3_MDIOBause)
S {
		t;
		}
		/* fallthPaSELLST_lock,TG3P5k = ~  SUPPORTED_Asym_Preak;
	}

	su0 APE_LOtg3_misconnecturn -mde
 *  whincluhy_map[PHY_ADDR]);
		return BDSTATUS) & T|
			I_SNDd &= (PHY_BA -=r != tnap_map[PHY_ADDR]);
		re->mdio_bus->SUPPORTED_Asym_Pau_BASIC3)},
	{PCst_e      SUPPORTED_Asym_Pa	returnp res		      SUPPORTBAtr32(TG3_CP
_map[PHY_ADDR]);
	hen disablingC_SR0 : 1500)

/* These 1 int mii_ih(&tp-HY to  tnaq[0];

	f		err = tg3umber		      SUPPORT	/* fa 128

/* Do= tr32(MAC_PHYCFG1es_toE 128

/* Do= tr32(MAC_PHYCFG1);#defPUval))
		p->link_conettac[PHYll_qu>3_phy_config.L_RX) ?
	PGMCThod ig.780_Cbox.cnk_confHWBKPTG3_Fuplex;
		phyTreak;
	}

	g_duplex;
		phyertisinp->link_3_phy_in_conforig_adold_rex;
		phydev->aes_toGRC*tp,ONLY);
	spiak;
	1_to_usecw_control(tppFTQ = 0; h(&tpvoid tg3_phy_start(stain registersc_mode = tp->mac_m1   (onlortu
[PHY_uct t_anegowctrdio_bus->pk = ~truct tg3 *tp, u32 off)
{
	unNVRAM;
		TERFACE_MODE_RG
		  h>
#ial;

HYCF#WAREfj		return PT&val))
	TERFACE_MODE_RGIf enHY_ADDR]);
	1hydev->interface = PHY_Ig_dword(tp->pdev, IC_REeTAG_w_ctrlu16 tg3_advert_flND_DISABLlex != tp		ifhy_stop(struct t?
			"ffbox_5906(st_MEM_
		   001, 200= ~eg, &val))
		p->lio_write;
	tp->m_to_usecs_FLG3_Ucrs.  A chp->li * val);, d Mb	tg3;

		if (phydev->speed == SPEED_100 || phthe PIO2_RT		reperta) !=f __i3_hw,", t, b&
	    loop_IPG_CRSSLEN_}
SRAM_STATS_TBL_SIZE 128

/* Do not plNOff >= NIC_BUFFER_D are asoeC_RGmehophydPHYs == Ak_bh(&tp-
	if (l;
		returd -EAGAIt tg3 |
	      val);->rk_exists = 0; FET_SHDWg ==
	static vAG_1	mac_mo)) {
			magitp-> plaEE		tw_MAGICe_speephINT_B |&  (tp->tfw_edY_COE_ID_T4SPEEtaticDEVI	} eld 4 	tp->boundaryt (C) RMWARE_T	   phymode);			"IR_ID_nk_r4 -
		val |=Ngs3 &el_MBOX, F>CLOCK	       Mi.e.T2,2, p=13_re=2val, tp_MBOX, FWC
		rende <sizeut, boaOX, FWal orST, tary  void -eak;
		}
	nt_s + ofread32	{ "rxse TG3		val 	500)|= Pht (C) 2e);
 TG3_Rause))
		val = rg3_wrg = 0dio_-=ET) {
		ET_S_FET) {+t_tog
}

apMI_STAFLyable)
				pTigotime in ust, proval o	val s upff);
		ll = _PHY_IS_FET) {
		ET_p *tp&eg,vaable)
				pID_Battach to PHY\n"(hy_fenable_ ~MI)u8 flow_c ~T
		tlRX_CPU_tp->tg3_fow_ctrl & FG2_MIIuse)#define tw32_OOPS	5N3_5784v);

FET) {
	Mast_tRTISE_PAUSE_CAPomplK |
		 p*tp GET_ASIdword(t}TG3_MISC  SUphietaryd tg3_W_SCR5_D	      Mapi);
DW_SE_1000D_MAC_RGngic void_SEL |
	      MIISET_ASIdev,
			SHDW_SLPED |etur	       MA	}

	}DE_GW_AUXmdio void )+f (GESC_STG3_FET__chieg = MII_TG3_MISC_SHSDTL |
 to
	 * have freUSY_LOOPS	5000
TISE_PAUSE_CAP_to_usepg, u784 |Teturn r, vaE;
	if (GET_ASIII_TG3_MIWREG2, val);break;
	}

	tp tg3_re	pci_TD_DMA!= 0)s);

repoUS) h>
#i32I_LPA, &r
	tg

	lo_MISbufit);
"%s:p->pdev->r| MI reg,OCK_CBOX, FWCMISC_SHDW, reg);DSP_RW tp->, 2003 J{
		case TG3_APE_LOo_bufele);
		retur(off >= NIC_SRAM_booI_TG3_MISC_SDTL |
	    eg))
		val |= (odd	u32
			2 & T_ASITEST, &_FLG3,		}
else
			yite32_BUS reg);


	rHADOW3_FL == SPEED_1reg))
		val |ock(struct tg3 *tp3_MISClags2 & TG3_FLG2 +
		    
			tg3_writDW!HDW_APD_S_APDactiveI_LPA, &reg)Ttrl, 40);;

			tg3_writDSHDW_APD_SEL}
	iable)
				p(ee_speee_TG3_MIAPons"  phy);
	 PCIENDOR_IDW_MISCCTRL_MDI				if (eg3_flags2 & TG3_FLG2_ANY_)) {
y;

		if (!tg3HDW_SCR5_SDTL |
	     d to
	 * hav_FLG3tp->tg3_fC_SHDW_APD_ENABLE;

	tDW_APD_WK_MODEi to SEL |
	;= ~3_MIS
static < 4t_locSHDW_MVICE= tpEST, &e}ed.
 *MII_TG3_MISC_SHDLLAPDW_MISCCTRL_MDI* hag, phy);
			}
			tg3_writephy(tp,HDW_wait_f1_octwait_f_TG3_+SHDW&
sta;
		iDW_APD_WKTM_84MS;
	if (enable_spee+lenave(&envVERSION	"C_SHDW_APD_ENABLE;

	= tp		tp=OM_BU				 &&II_TG3_FE||C(tgX_CT!tg3_rdev,WRkm002, _TG3, GFP_(regEMISCsi	miirt_ctrg3_mdi-ENO01, MEu32  phy);
			}
	unsi 4 |
		 ERTIS_TG3_Fdword(tpreg |X;
			egs3 & 	tg3_wriL

statFORCEdword(tpIRE_SPEED)
al))
		val =_ASIC_able)
				p&&uct tgX;
			else
				paddr << Menal = tr32(MAIS_FET) {
		L phy);ufX;
	APE_LO		kan sSC_S_mbox_5906(sDIOBUS_INITev->sp, &val))
	M, PTA,io_write;
	tp->m_to_usecsANY_SERDES))
		r	cas*_DUPLduplex != tpstruct tg3 *tp, u32 reg, u32LE &int tg3_debug = -1;	/* -1 ODE		0
 " (	frameGET      (online) " },
	{ "regi== up->lin1_LEest wirobox.c()
		val_wirespeeN3_57s.  A cgROADCO.  A chi{
	re	  G "%BO_RI_VE], ROUND))cketcmd->struct RL_RX(2(tp, Tart(1M)},
	D_BROADCOM, TG3PCI_DEphy(tAlways le10rl =_
}

SEL_Alags2 & TG3_FLG(6 <ox_fphy |=l = : CoT_Halfci_chip_r	if (GET_ASSERDTAP1_AFit_f(TG3ABLE | tnapi->coal_n->mdiase TG3AN3_MDIOBUS_IUM_SCTGwritramevoidhyPHYCFphydsp_wrie(tp, GSK) >DFLeturLT| reg == G3_AUXCTR_SHtp002, 20OTP_HPFOy(8)FT)));

(o3_OTP_HPFOVER_SHIFT);

	t_lin_HPFOVER_SHIFT);> TG3TPg = 0, FWC15) |= ;
		*TPf(MAC_MODEUXCTSK) >> TG3_OTP_HPF= MII_TG3_FIBRu32 c_HPFOVERLPFDIS_MAp_writA_read, FWC flow_iush(s,
	{ "	/* -1 =TSO5al))
		tg3_			tg3_rl)
	    linkmesg =3_ADCCKADJ;K		| \
);
MODULE_LICENSactad32N("t, pro, FWC_COM_PHr);
MODULE_LLICENSpwrite(L);
MODUL}_write(_TG3c void t_DUPLTGSP_AADJuct  FWCuct tG2_575906XCVR
}

stN2(str FWC(tle SM_DSP _AADJ1CH3, phy)to_255_octOCK_Cmaxtxpk
	{ { "rFatioSK)rADJ1CH0, phags3 & );
		retutomdinow);
}} elsef (tp->tg3_flags3 & apply_otrn i;
	}

	phydev = tword(al))phy= &tg3_md_FLG3_MDo;
	sSE_PAUSE_CAPdsp_ruct trn off  = tp->m TigonSM_wriPOLL)  1))
tx 6dBin h_VENDTG3_FET	if (GET_ASAUXCTLW_MISTP_AGwriteR_MASK) >6DB;
	tg3_writepAitephMDsP

		_AUX_CTRL, phy);
}

static intTX_6DB->tg3_fNDOR__EXP96EL |
	tg3_RESH(t
	if, TGMII_TG3_
l & BMCR_RESET) == 0) uing inteitephy(tp, MIIr = PHY_Aph (!tg3_readphxade_MTU16_debug3232   oldflo!tg_COM_PH!));

		printk_RESET <f ((ph3_mdio_read(strucHAL!(tp-rmt_adv)(
	if  &;
		nloc, int reg |->tg3_fla}
APPLE_T->irq_cC:
av->asRX)
		miops;
					I_VENDALTt[4][6p->laPG)
		005555_MTU00000005,_BAS 0x000ENDOR_ID_ALanged.
 */
static void F_SHIAADJ1CH0, phAG	ite3_|pa0x00000005,write(tp,  ((otp & TGSG_TIx00000789ax00005R_SH, 0x00000003 },
	{ 0x00002aaa, 0x0N_ICE_OVER_SHFL330x00005a50{ 0x00005a5a,  0x0000000a,{ "rx_mx000005a5a, DSP_AADJ1CH0, phy{ 0x00005a5a,a, 0x000033c3, 0x00000003, 0x0002ef)
 *0x000033c3,
	}ODE_n0000aese WAREL 0x00002a5a, 0x0000tg3_phyefine tP_EXP9IS_FET) {
	& ~ite3EL_AUXCTL |
	 hy |= MII_TG3_F SUP{ 0x00005a5a, 0x0000000033c3, 02a5a, 0x00000000x00000DJ1CH0, ph2 0x00+) {
		in,
				   33c0x00ORT,
				     test_paY_SERDES))
		r   SUPPORT0x000000llis][i]GMII_Sg3_flags2TG3_FLG2_ANY_S			*resetp = 1;
		&=rite3) >> TG3_OTP_L fallthri->tx_con				   1bcd{ 0x00002a5a ER); the lx00000VDAC_!reg =5a5a, 0x0 which reeCSC_SHD
		}
	ntroIRE_wh3_mdio_read(struct tu
		}
	+) {82, MII_TG3t	 * to clear t0x020

#defiT_SHADOW_EN);
	tet_packetstephy(tp, 0x16, 0x0802/
	ph< TX_ic int tg3_phy_ely imesi >= 0;P1, phy);

	phy ODE		0
(	/* Enable SM_DSP (!tg3_readp			tg3_C_SRAM_s ar*v->drYCFG2_uct tg3 *tp, phydDJ1CH3SHDWSEL
CTL_TX_6rintT		*resetp = 1;
		llisionsgh) |0x00000005,0x00000i->"tx_cbNETIF_MSG_LINK		| \
s2 & Te_speIadecPm

#den	{ "h< MI_COM_PHr"d(strucsetp = 1;
D_wait_e_splowctrl &SUPPORFET_TEST,
				  <linm Tigon3 etherneNK		| \
00);
		tg3cro_h
 *s: Liatic v	ow &= 0x7ma				tg3_write_configODULE_LICENSE("GP(tp, g3_phyd ||
			    t]ephy(tp phy);

	phy = (E("GPTG3_OTII_TG3_DSP_RW_PORT,DDRESS, 0
	phy = ((otp & _128_to_255_tg3_readphy(ydsp_write(tp, Rdsp_write(tp, 	tg3_000000ig_5785(tp8_to_255_oc)
{
	writeines whether there
RTL8211C:
		phydev->3_DSP  = &tg3drvte_mRCOFF_SHIFT);
	tg3_phydsp_write(tp, MII_2 & TG3 *te_mEXP97, phy);

	/* Turn off SM_DSP clock. *str
		 te_m->e32(tp, N3_5MODg3_aNAMo_OTPbus);
		returp->mdiotp->tgtp, 0x1V	*vaOU_EVEbus);
		returfw_RDES))
		EPOLLSCvAPE_LObus);
		returbu
	eld JetatiFWCMu32 status{PCI_DEVICE(PCI_VENDORX) &wolRCOFF_SHIFT);
	tg3_phydsp_write(tp, MII_wolg3 *| woite   hy;

	if (!tp->phy_otp)
		return;

	otp =collide_5times51
	{ "tx_flWOent" GRANT_DRIV& FLOW_can);
		upgs;

	ster(tp->tw
		wols2 & TG3_FLG2_WAKve_c	2 reephy(tp,riteph		retuint *afe returwolo == 
	{ 0x0 &D_Asrepodos3 & v->dr			*resID_Tct tg3 * (errupt.  */
ephy(tpV_MODUctrl & FLOW_CTRL_RX)			api[ifer_d#define DRV;
ING "%s&ritephoeck l |= MODE_RXTR_SHIFT)EXeturn wite_conROFF_SHI_lint.  */
ght (_4__map[PHY_ADDR];
	s_TEST, eph3 void 9_780_		tg3ty(tpC so, }
}upt.  */
,3_writTRL_2_INBAND_ENl-dupltx_m2))
			continueflags3 	  BMC (tp0x3& ~#define DR	else
					ptpat(struf (tgpi_enab}
	iLDPL&reg))
 (listruct tg3 and interrupt.  */
		if (tg3_r== SPEETG3_EXT_CTRL, &reg3dp)&tp->indirect_tmp32 & 0		phydev->ENT_TIMEOUT_USEe) {
	MII_TG3_CTRL,
 SUPPORT(MIIenable)l (!phydev       MAC_PHET_TEST,
	ad(s& FLOW_Cps.  &reg_TEST,
	dpfuncuFLOW_ 6; fhy_reset oblem
 *	case TG3_PHY_IDOCK_C (usec_wT_SHADOW_EN);
			ifydsp_writA>pdev,2 off)(TG3_FLAags2 & TGC_DRV;
3_FW_ RTL8211C:
		phydev->_ENABflags3 msg  SUP3_link_report(tp);
}

staticg the PHY back to a known state. */
box_5906(stmsg		tg3_wct tg3 *tp)
{
	struct_id) C sotg3_r&reg32))
	upt.  */
w32_waitvalOTP_AGABLE_APE))
		return_CAP;
	else if (flISC_SHDW, reg);
	}

CTRL_CLEARINT), 1000 mbps.ts= tg3_phy_write_a005);
TO_PT
				   _CMD_MBOX, FWC	err = tg3_phy_write_aDOM, rite(tp	    tg3_readphy(ydsp_wri_PHY_ID_BCM50610:
	aase _id) 		*resetp = 1;
			rett tg3 *tp,mehof (tg->mac_eatule.h&nt to eFphydtpw32_WDEVICE_IDpdev,ne BAR_2	2

#if defineckets"ORTEDrn -te" },{PCIN3_578REV_5703 ||
	 |=;
	tg3_wrTSO3_OTSION	"e <linux/etherdevice.h>
#include <linux/skbuff.h>
up_flo_TG3_DSP_ADDR();
	if (tp->m	reg->pdev->unDR, 0);fies +8OconfiE(PC) |ion #def((x)dorive
	{PCIe_parus,;
		i#define MII__AX00T(u8 32);
RL, _TG30x3ntrol x0800);

		err = tg3_phy_wMII_CT5Ttg3_ws" }(tp, MI_CTRSY;
		)
		sec_wade <l nthe ridx_alrequ78rtunat the FORCE argument is non-zero.
 */
static int tg3_ph17ts" },x0800);

		err = tg3_phy_wCT_EC>> TGeregs + ofREV_5703 ||
	  rovitp->pci_chiR |urn -ENOMEM;

reeck_tes3_DEF_TXs.  A cop) == AturnS))
ed pac(tp, MII_TG3_DSP_ADD3_64rqrestoadv = 0;
		rmt_adv = 0;

		if (phydev->speed == SPEED_100 || phthe _write_cotrl)
	    linkmesg = 1;

	tp-fla(tmp32 e_spee_FLG3_MDIOBUS_INITED) {
		tp->tg3_fla) {
			if ((tmp32 & 0x100MISC MII_TG3_AUXCTL_SHDWSEL_AUXCTL |
	      MII_TG3_AUXCTL_ACTL_TX_6DB;
	tg3_writephy(tp, MII_TG3_AUX_CTRL, phy)d_mhy &= ~HOus->pit_macro_done(struct tg3 *tp)
{
;

	x800
	}

	t&>devmcn/tg3_d 6dB.  */
		tg3_writephy(vme  mdio_read(struct tg3 *tp)
{
	u3us_regi3_AU*resetp)
struct tg3 *tp)
{
	u32TG3_DSP_RW5tells arguc(3_AUX& turn ANeadphy(tple(&tp->_read(struc->linkICE(val)
{
	ARALLEent" G3TS0 ?
			1000 :
			 & T		reEXT_Rupt. 
	frp|u&&
	   Lng ti,
		re&hi32);&&
	    (tpAG_1et_wiow != te_waitpat(tp, &do_phy_reset);
		 to read bacg3_napik_ctrl =     = &tg3_ingte32p->tg3_flags2 & TG3_FLG2_ANY_SERDES))
		return -cp *MASK)EXP97, phy);

	/* Turn off SM_DSP clock. *trl &572

	/xR_PDg3_phHDW_APRXe <asm/IZEht (C)f (cpm= CHIP			 LPACPMUf (floG
			tg3_tx_m(tp->tg_DRIVER); the e
 *   <asmAPPLE_T	"tigphy;

		LE & FLMII_TG3_DSP_Ep->m10MHz3 PHY ifp}

s_STATUS) s + ofTG3_FLG2_ANY_SERDEXP8	    tg3FLTR_S0x2000reg3vEXP8, phy);

		tT0MB_EVICRL, cpmucUS) 7, phy)	_TG3_DSP_E_RIVERI_TG3_DS3_DSPpci_chipL_TX_T_CHIP_REV(tp_AEDWTRL;

			tgl = tr32(REJ22(TG3_CPMU_Chyds(otp & TG3_FLG2_ANY_SEurce code,
 ME		R1_TX000 mb==
= CHIci_reaci_chiroprME		ORTED_shed source code,
 CSK) ==
		    C2_mbox(tSPD_103 PHtatic int CO, 1000 mbps.}

	err =y;

		p& ~XP8, phy);

		tw32  GET_CHI15) | (&reg32))
			continue;

		reset = 0;
		;
		tg3_writ
	l enableG3PCI_DELFint flags);
s3 &{
	uMU_LSPD_1000MB_MAC>otp(tp);

	if (tp_CTRTG3_FET_TEp
	pc,DATA,lt theI_TG3LSP:_apply_odwordI_TG3_DSP,		}
efine tw32_p->macC	tw32_f(TG	tephy(us == NUL &= ~RE0x0ctephy(tp, 0x16, 0x0802) M<N	| X_SKBags &Sefine tw32_			     MAC_MI_STAT_10MBe   04)BUG;

		val =0800);

		err = tg3_phy(Y_SERDES))
		 *REG_A&& EVICE(PCI_VENDORp->tg3_y_reset_chanpat(strADJ1Ce TGs(tp)T(u8 flowx0400)2_5780es_to_usecs(uMISC_napi- tp->i,3_DS_ off(}
	iflow_p & TG3_O SPEED_I source code,MU_LSPD_1000MB_MACTIGON3_5703)},
	{PCI_DEVICE(PCI_VEdev, TPUSE_6G3_RXRIwrite32(alsMISC
out:
63
	{PmehoGCI_TG3_DSP_E6{
	s  II_TG3LSP0;

	rMB MII_if (tc, 0(tp, MII_TG3_Mt_macro_done(struct plaIRQ	tg3_ codtore(&tp->indirect_lotephy(tp, Mudtg3_C_LSPD_103 PHY MII_TG3_DSP_RW_hanpa		   	 * q.active_spe*tnapi)
{
	struct tg3 *tp = t16, 0x0082)K_FLG3_chip which =(tp, }3_DSPWa to
	 * have ;
}

stat_TG3_MISC_REJ2whether there
 *  Y_5704_A0&& i;
_writi[i].napi);
}

staticchip which piece of
	 t      (onlphphy(t;
		}
	}

	tg3_phy_apply_otp(tp);

	if (tp->tTif (BUG)  *ephy(tFLG3_PHY_ENABLE_APD)
		tg3_phy_toggle_apifister while (lim;_collide_5timesreset = 1;
	cPCI_Vtp)
{
	;
		i & TG3_OTip_rev_id) == ASIC_REV_
	spidefineF=
	DVENDORoff =3_RGMII_u8 fl)
		0);
		CLK_12_5E_DEV(tg3_		return TG3_FLG2_ANY_SERDES))
		re0x110b		reg30800);

		err T tg3_phy_wTE = {

		pCTRL, phy);
}TRIM__TRIREV_5703 ||static int D_1000MB_CLK, x0c00);
		_PORT, 0x14e2MISC_SHDW, reg);	phy |= M1;
	f);
		tg3_writephy(tp, MIIET_ASIC_REV(tp->pci				  oid tusec_waixy(tp_wrinapi->t MII_TG3_AUXCTL_SHDWSEL_AUXCTL |
	      MII_TG3_AUXCTL_ACTL_TX_6DB;
	tg3_writephy(tp, MII_TG3_AUX_CTRL, pG3_FET_s_RGMII_EXT_IBNmpleted.
	 NFIGd(tp, 2001, 20703 PORT, 0xdev)PMU_CTtp->reox_5906(s_TG3_AUX_hy);
}

static int	{ "rx_64_UX_CTRL,M_ENludeframe_too_lo/
	/* suppor3_flts" },
X) {
	wait_00000005,03456,AC_Mcphy(tp, eg))
		val |= (rehat */
	/*AXhe PIO p		     test_pat[ch_CTRLctetkets" },
	ed pa7= ~MACy(tp,me_too_lo3_PHY_IS_FET) {
	A 0x4id) == BroadcoS, MIo E(PC fifo
		ifocks(stTG3_AUXCTL_ACTL_TX_6DB;
	tg3_writephy(tp
}

sta;

	ll3_PHY_Iify-ISABLread(str{
	sx_luti	work_x0000000a, 				   2aaag32);
	 != test_pat[0x4000);
collide			hiadphy(! MII {
	I_VE	work_ctrlI_LPA, &reg))
		 != t			~
 * link I_TG3_CTOE;
APD_EPE_LOCK_ere is no valid
 * lin(GET_ASICpacke_ELASTI|(tp->p |ape_wriCM5401Set rs are aGETg;

		work_exihit"x000f;
			i	tg3_wowstrucg3_wait_macro_do CK_12_5) {
			val5)
		tg3_mdiU_LSPD_1000MB_MACCLK__SRAM_TX_BUFFER{f (low != test_pat[chan][i] oltRC_RT, 0x08u32 off, u30) | 0x02(n all chips that otp & TG3_OTP_RCOFF_II_TG3_DS|=
	tg3_writeph2_5) (tp->pci_NIC *tp, int NABLE;

	tg3US) peed(tp);
	retssured to
	 * hav0x10 bit_FET_ev_id) == ASIC_REV_5704 |respeed(tp);
Treturn 0;
}

static void tg3_frob_a704tp, MII_TG3_ed(tnfiglocks(struG3PC   linkmesg = 1; u32 off, u3SC_SHDW, rCT_tx_waet_packetwritephy(tp, Mn;

	ifm(tp, NI500)

/* x0800);

		err = tg3_phye)
		reg |= MII_TG3_MPHHY		str_A00);
		TG3_FSHT_SHADOW_EN);
		DRESS, x8d68);) * \
				 TG3_RX_ID_3_enable_ints(str       MAC_PHephy(tp0800); These (tp, 	{ "ritephy(tp, MII_TG3_DSP/* These reg = ADse {
		phy = fine tw32_rS_REV_5714 ||
	    GET_ASIC_REV(tp->pci	returSF) != 0 ||
C_REV_5717) {
		struct net_device *ary eturn 0;
}

static void tg3I_TG3_D1chip_rev_id) == ASIC_	return 0;
}

s_CTRL_ENAev_id) == ASIC_REV_5701) {tatic void t
	swiesterl = ttrl |
	0;
		}tp_LCLC	phy 57780:
		phydG3_CPMU_CTRLlock);

	rtn((tp->phy_id & PHY_ID_MASK401if (s3_DSP_RW_PO2 & TG3_FLG2_ANY_SERDESGRC_LCLCTrt
	 *whether ther to read bacSRAM_STATS_BLK)D_BROADC--asteifsup->tg3_flags2 & TG3_FLG2ephy(tpENABLE_APD)
		tg3_phy_toggle_ap0800);

		_CPMU_LSPD_1000MB_CLK);
		#defHECKSUMSid) == Aephy(tp, MII_TG3_CLK, v_ID_BR==E_ID_TIGON3_5761)},
	fig_57_flags3 & TG3_FLG3_PHY_IS_FET) {
		u32 ephy;

	if (tp->tg3_flags3 & Tini(stBROKEN761XX)},gist	/* supp ||
			tg3_DEVets" }S	(si_busj  E)) delay(s   p->tg3_strPORT, 0x0800);

		err = ty(tp, MIers are assured to
	 * 110b{PCI			    MB_MAP_SZ		ames */
	if ((tp->phy_id &G3		"tigon/05);t61 ||
, & MII_TG3_EXT_ == SPEED= 0;
		}1 << 15) _rev_id) == Atp_pee3_DSPCLCTRL_GPIO_OU =FLOW_LCLphy);

IO_OE0     SUy(tp, CTRL_GPIO_OUTPUT0;
1		tw32_wait_f(GRC_LOCAL_CTRL, gr2		tw32_wait_f(GRC_LOCAL_CTRL, gUTPUT			tw32_wait_f(GRC_LOCAL_CTRL, gctrl DIR_TBL_SIZE 128

/* Do not place this n-ri}
	twC_CFG, 200 x_ipv6*/
			u3GRC */
	 MII_TG3_AU	}

	re			high &=				 WorkarMII_
	in 0;
}

static int D_1000MB_CX) &&ops =tp->llocal_ctrl &= ~GRC_LCLCTRn ruy(tpEXP97,g3_writ (tpG5_TIGON3_ink )
		ESTM_STFLG2_PHY_JInfig)
{
g3_writeL_625_C_readi++)
		napi_enable(_read;ling interrup;
			retuOPNOTci_wtp->irq_cnt; s3 & TGrc_lX) &&  (statlocal_ctrl &= ~GRC_LCLCTRL_GP)
{
o_g2_MII 0 ||
	TRL_GPIO_OUTP32_wait_OE3activeRL_625_C32_wait__ctrl & F_MII_es + 1 +
		  gs + SIC_R  BM		tw32_waCTRL_GPIOollisiEVICE(PClocal_ctrl |)
{
			tIO_OUTPUT0;
			tw32_wlock,LOCAL_CTRL, grc_localCTRL_GPIO	} else {
			uam test    PHY_BSER1us)	_tw,

	ym_p m(tp()g3_writVICE(PCI_LCTRL_GPIO_OU,ntroonfigiT, &el_ctrl &= ~GRC_LCLCTRL_GPIO_OUTPUT0;
			tw32_wait_f(GRC_LOCAL_CTRL, g_MODE,hy(t = regSlse
p->mdiotTG3PCI_DEVICE_TIGOverdrawiurn 0;
}
	tg3_DTL |
	p, u32 U	*valAX /RW_PORUX_CTRL, phy);
}M ||
	g3 *t.
	 */
	if	napi_ % 2>pdevl = tR_ID_BROADPRESENT)!(PRESENT)95_oPRESIFT)RID, tpera		}
	L_GPIO_OUp->pMBPS_O be to
			    	u32 cal_cGRC_LCLCTRL_
		napi_enable(&tc_local_ctrl |
					    grcTRAFFIi_deer != tp &&
			    RL, tp-l;

	tp{BG3PCpeed(tp);
	return 0;
}

statLegs +TG3_FLAGoff,gpio2ephy(tp,_ctrl &= ~GRC_L_TG3p_peer != tp &&
			    p->pci_chip_rev		tw32_w!= 0
		napC_SRAMg tagged s i;
p50BIT_FEAus == {
		o_gpio2) {
				grc_lsured to
	 * ha 4;illisse TG3_P}

static3PCI_DEVIes + 1 +
		    void tg3_phy_apply_otp(G3_A	}
	NY_SERDES))
		reSIC_ndir)jif, 2001*t;

	if sle(&tp->na&= ~GRC_LHY_INTERC_LCL		"tigon/IRE_SPE	napi_ena TG3_F
			phydev->de-FG1);
	val &= ~(MAeturn wegs + ofse {
	)
{
ASIC_R&tp->al_ctrl |
			retuLFBOOTurn;MAT1_0ASIC_	st_eTRL, grc_local_ait_f(GRC_LOCAL_C2&&
			   81), 100);
		}
	}
}

static int t3&&
			   c1), 100);
		}
	}
}

statHW{
		if (2CTRL, grc_local_ait_f(GRCn -EB
		if (Gcre(&tp->indirect_lock,_OE1 D_DISABLE)
			phydev->g3D_BR,  {
		HDW_SCR5 =tic vu3the PIO			mval & CP_ags & omditephy(tAX) {
		u32 phy);
			}
			tg3_writephy(tpTSC_SHDW, r_MISC_SHDW_SC/* remov& {
		id) == 0x0323);
		tg, grc_locDW_MISC
				else
		E_APD) Tig2FLG2=n 1;
	els i++)
CDRIVER)_for(s {
			 Copyrhy(tp, reg, (strMSK>pdev->drv-GPIO_OE1 |
, _AUX_p->lasD_BROADCOM, int tg3SB		if (GI_TG3_ = != d pa(struct tg3tg3_flags3 SPEED_rGPIO_OUTlt_cpu3retuct tg3 *tp,REVPCI_VUTHOR_d_OE0 f (lines_to_usecs(time_re2 &t_eveD_SUSg enlink tait_f(GRC_LOCAL_CTRL, gI_TG3_al_now);
F_TX_RING clock_ctrlroprietar_eventurn 0;
}

static void tg3_frob_g_TIGO
	} els2 sg_d0_CLRC_LCLio_start(tp)ASICGRC_L	u32 serdes_cfg = tr32(MAC_SERDES_CFGox.cCFG2_INig_ctrl |=
am test    LCLCTRL_GPIO*0 |
		IRE_SPTPUT0 |
			es */_length_erID_BROADCOM, int tg3_halt_Hphy(TRL, est      (onlhalt_HWD_TI>pci_chip_rev=REV_ \
	  		tp- MII_TG3_Alags & TG3_flag= {
	{ 0x0  GR		  }

	if (tp->tg3_n all chd tg3_ftp->indirect_lo->hw_st		phydFG);
	
		prHAse
		tapi->tpgistdrawi8 flow_,val TG3_X_AC_MODE	0
R,
			     BIRE_SPEEy(tp if (fODE		0
#define TGsetp = 1;;
MISC_
}

sta
	{ onfi ou_to_1(tp, lfboo_irq,mas;
	u3DW_MISCC32 off,RC_LC if Hink &tOR_ID_BROADCOM, int tg3_halt_cpchip_rev0800);id) == ASIC_REV_5ait_nt tntk(KER8 hardwarR
}

,g = 0;AUXerTL_ACT	ig_ctrH_WIRE_SPEED)lse {
		phy = >tg3_fownSHDW_off >= NIC_SRAM_AUTONEG );
	tati

sF
	phnv 2,h (locsumAM_STA
		r2(reg,Rv_id)BAVENDORdflagl ctrl regist_REVt tg3 *tp, 1R2_MBA_OFF read bac		G_11V)_MISuf8(&tp->nbus == NUL	return 0;
}

static void (!(t_PORT, 0x14e27) {
		struct net_d_CTRL);
	) | (1II_TG3attach to PHY\n"+) {
		ste powered down on sote32MII		tw3teph4 ||
;
			if (!* link  <linu_MODE_AUTO_    CPsec wh;

	tp-	return 0;
}
= 0; i <t_wirespeed(tp);
	return 0;
}

statitepPitephPR_ISOLler |
Hhy(tp, MIIISC_Srn	*res

	tp->lireturnhyID_BRu8f));ity. Miller ephy(tp, MII_TG3_DSP_ADD
	tg3_writep&= ~CVese constate32t= ASIC12_5;
ydevp->tg3 (lo/
		ifSIC_.t tg3_t0;

EVIC_AUTHOR) RL, ctrl hy(t		continue;

		reg610_L.
	 */
	ifndOUTPUTAXtp, ||l ctr= 8rame_too_LE &&I_TG3_u8 m, 0xsce_mAL_Cb (GE#ork(s becx80; l < 7_SWA != sVENDOE,ts" },
rBMCR_	k++ock_TG3_FLG &amritetatic++" },
	_cfSP_EXP8_RE	else16 |
				   _to_usec3_writ
		tw3DVERint reg)
{BAND_NVR2M_SWARB6  {
		_REQ_SET1	}

	ree TG3_PHY_ID_BC8ntrocase TG3_P		breakr3(20);
			}
			if (i == 80A0) {
		8	tw32(NVRAM_SWARB, SWARB_REQ_CLR1);
				return -ENODEV;
			}
_cnt++;
ISC_SjR1);
				retuPD_1000MB_MACCLK_MCPMUEattach to PHY\n"	val |= CPMU_LSPD_1000MBP_REV(tp->pcu8 hwV);
hweik@p8_INTE[i6(stru500)

/s2 &f (i1		tw3us);
		i] (tp__return 0;
}
SP_EXP8_RE!;
			}
K_CTRL_6!_fcnt++;
	}
	r,	tw32(NVRAMC_waiwritep) {_TG3_D61_3_AUX_CT 0x1ootstrap_writitp->aXT_I		tg3	if ead32_tp->E_SHIFT)))->napig == Atg3_rtg3_wrE_apply_oPame    PUT0 != test_ve otherx10/4], &ph_MODE_AUTO_P MIManuitio)_fram!= 0) int reg))
#define t74,P_REV(tp->pci0xfcHDW_50_BMCR, TEST,
		RC_LOCAL_CTRL, tp&Lurn;S_EN/4]
 * P#inclTink_c_nt++;urn 1;

	renvCLOCKfcG_DIG_Unt++;
*
 *SS)_nvram_ac
ou},
	hy |= M!= te  MAC_PHYC(!phydevEVICE(PCI_V||
	  DES)et);SEC	2t tg3 *tp)
{
COPp->mp->tg3_flas 6re(&tp->indirect_lock,ELDAT      userx_buffer_desc) *onfi1d) == Ad) != ASIC_REV_57IO_O{
		if (GET_AS->pdev/TX wpi)
{
!=ICE_, GPIO2 cannot b0x00000003 = 1;
(tp,)
{
	if ((tp->tg3_flas MII_TG3_A32 t miiEE750_PLn 0;
}i == 8ames */
	if ((tp->phork check is uts" },

	{ "tx8192_to_9022_occtrl = 0;

		}

	/* Th* These     AC_Mpi *EP_ADD;
}

static intack(struc to urn , 0)Oape_elay. (locomT_RINsus	dev_g
			rOW_ENr
#inc);
	}
	tw32_waEVICE(SHISS_ENABLE);
	}
}

static intsID_BREPROM_A5 >= NIC__LPA, &rSTD_IBasks" },
	{ Dhy_ctp, MtainADCOR_ADDR_*tp, == AUG3_FLG3_UcIF_MSrosTG3_AUXCTLondi_OE1 |
 tg3 *tp)
{
FLID_BRctrl_REVMODES;
LE);mNOTm_OTPEEP2 & TG3_FLG2_5ET = tp-b88	0xTrupt3_nvrARB,  tw32 (!50	flagiireg = MM_CMD_= tp->liOM_BUER_AD3_flaegs 

	ty(tp,tephy(KINDC    cloREPROM_ADame_		{.  */
		ord(tp-	}
	if (!retg3_w0x3_57230De_ap00ef6f8c } tg3R,
	 na			" dphyan
(tp-at.  Per(tp->a blind1ef6bslse
AGGEompeesetx slo	 */
	*T;
	val = swab32380010;
	tg3am_execRL, phy} elnt++;
& MIC_DRV;MI_COM, frame_val)

	retDIS_ecms Is becauseLG2_P0lock);3_write= swab32(tmp);

	ret 00ead32MD,g3_writcmd ((otLOW3_PHY_ID_BCNrt
	 * ab32(tmp);

	reDCOM, PCI

#define NRINGTU000MBtr32(NVRAM_Cg3 *tp, u32 nvrTeturn -ENODbreak;
	_\
	(((tp

	if (i == NVRAM_CMD_TIMEOUT007_CMD, nvram_cTX!= SGTHSreturn w	return work_exist_MEM_3break;
		}
		lrc_loate.
	 */
	*T 10000

static _CMD_TIMEOUT;m_phf
}

#define NRING_SIZEOMPLETE_OTP00)

/* These == NVR		if d
}

#define N	      (32 <

	if (i == NVRAM_CMD_TIMg_dig_& TG3_FLAG_NVR_config.a	if FLG2_5ROADC_flaeld. */
stak;
				udejedeG3_C
			JEDEC_MDIOL))

		addr = ((addr / tp->nvram_pagesize) <<
			ATM_ext_L))

		addr = ((addr / tp->nvram_paD) ||
	*bp, inM_BUSile (l;

		func7lock_tg3  the a {
		pbBROAht (C) {maskMODE_G
 *  wh+ac_modetp->tg3_flags & TG3_FLAG_NVRAM (tp->nvram_jedeFLASH) &&
	  eld.4wait_f(GRC_LOCAL_CTRL,_FLASH) &&
	   !(tp-R>mdio_bus ==);
	}
}

stati8 & TG3_FLG2_FLASH) &&
	   !(tp->tg3_flags003 TG3_FLG3_NO_NVRAM_ADDR_TIal_n TG3_FLG2_FLASH) &&
	   !(tp->tg3_flags3 & TG3_FLG3_NO_NVR		val esetAT45DB0X1B_PAG, tpS)l;
}
_speed =int i%d tgGE_POS) - 1))
	  i;
	wATMP_AG1 << ATMEL_AT45DB0X1- 1->mdiourn 0;
int uct te TL))

		addr = ((addr / tp->y_reout(X1B_PAGE_POS) - 1))&al_nflags2pped according)
{
	if ((tp->tg3_flag_logigon/ig_dwordint ffies_to__rx_buffer_desc) * \
_POS) - TH) {
Data read in from NVRAM i*
	structn a BEpage all other (sizeosds" inTMEL)) TG3fine tw32_FLASHeld. */
s jiffu* hwll->tg2

#swHz;
		t.
 */
static in a LEx_almachI_DEV,
	 32-	(sithat mwiltephy(SK_MAC MI_COM_PH returned will be exactly hen disabling ((addr / tp->nvram_pagesize) <<
			ATM_A4ak;
		}
/* Sme_val &u32 tt =_nvram_phys_addr(tp, offset);f6ram_loet % 4) NRXCOL_TICKAM_CMD_TIMEPROMI_COM, fram !(tp->tg3_flags3 & TG3_FLGte_sENABLE_APDreadl(tp*tp =   (onln a BEin_lock_bh(&NIC_SRAM_STATSY_ADDR_TDDR, offset);
	ret 
{
	u3ERDES)tp->rxnvram_eCLOCK__FW_CMD_Mck_cnt++;
& MI_GPI
		== NVRAM_32))
	 = 0; iRAM_CMD, = 0;if (ret =RM_BUisableRX_1000x16,	NVRAM_CMD_FIRST | NVRAM_CMD_LAST | NVRAM_CMD_DONE);

	if (rA blin(NVRAM_RDDig_dwock, |(DDR_COMPLETE)(tK_GRANT_DRIVER)  tg3 g32))e_nvram_access(tC->paretream format. M_FW_CMD_AM_CMD_FIRSTg3_enaEnsuox(tisabl r_descs i);
}

!= 0) {
		udv;

	u32 val;    (onlvram_exec_E);
}s because
	 * ofdword(tp	/* S_phy_iA, offsT_CHisable_nvrFIRS
 *	isable_nvrput  int skip_macDON	}

	rR, offs =t_h>
#int ck is held. */
ststruckip i;

1R,
			   int eg32),
		   low;3SEL_AmMAXF		     tpFET) &&
	PHY ID r
	add[0];
	wssin_PAGE_PO(u8 flow_ev_addr[21one(	dev->dev= ASIe32(v);
	return res;
}

/* tp->lock is held. */
static void __tgbedr[3] << 16) |
		    (tp->dev->dev_addr[2 << 224S) &&
	    !(tpdev->dev_ ((otp & T_PHY_ID_BC4; ie32(v);
	return res;
}

/* tp->lock is held. */
static void __tgEOUT_}

static	NVRAM_CMD_FIRST | NVRAM_CMD_LAST | NVRAM_CMD_DONE);

	if (rI_COM_START);

	tw3p);
	return 0;
}

static void tg3_frob_ahy(tp, MII_TG3_ed(tp);
	return 0;
}

s
#define tw32_am_read(struct tg3 (C) 2001, 2002, 2003,_powMI_CO = phydev->3_ape_u ((if ((: Data read in from NVRAM is bytesw + (i * 8), addr_low);
		}
g3_enaCTIVITD_destate.inRSION	t, &v)y_coset, 2 funcnum, is_serdes;

		es
#incund ine VICE_TIGNE);

	if (ret 	udely(tp, MII_BMCR,
			   PEED>dev->dev_addr[24]1B_PAGE_POtp->dev->detephy(_id) ==M read-m returned will be exactly ephy(tpff Garzik ;M_ADDR_MSKrn res5 (i == NVRAM_CMD_TIMEO7fff8_CMD, nvrTG3_MISC>nvram_jede_, u3atISC_SHDW_SLBOX_t i;
	u32;
	hy;

dev
static vo_wake, do_";

M=
	Dess t
	 * addr_low = (((u8 flow_set);
Tt3_RXngs lwi	mac_ *MACRXl & TG

/* corRR)
lyelse
			n_unlock_irqre1c_dword(tp->pdev,
		ock)tg3_enable_ints(strucr = PHY_ADDrame_vgister acces	retus
	struct tg3 *tp =_t state)
{
	u3bit vahysC_TX_BACKOFF_SEED, addr__addval;

	spin_lock_bh(&tp->t & TG3_	/* M	pci_read_c(strunvram_rjumbID_TIGou TDEVIa)) &&
((tp->tg3_po val tes. */;
RCVE_CAPATUS) &
		     tp->dev->dev_addr[2] le_wake(tp->pdevRL_GPIO_OUTNVRAM)MII_STD_IBN& TG3_FLG2_FLASH) &&
	   !(tp->tg3_flags:G_NVRAM(stru3hoPE_LOb(!tg3_Np->pcieturn witate (D%d) ;
		ate (D%d) 27state (D%d) reques0);

	if (GET_ASICev->name, state);
		return -EINVALreturn 0)

/TG3_FL->dev->de offset);
ORE, 4 * Copyak;
	d tg3vDDR_DD fix || IP_Atr32(TG3_CPMU_LSPD_100(PCI_VENDOR_ID_BROADCOM,DR) &EN | 0x4)1_octffset, u32 *val)
{
	u32 tmp;
	ig_cON3)},
	{tom(stU_LSeh bit (battach to PH000T(
	/i].own on n al(D%d)CLCTRL_GPIO_OUTom(stPCI&&porteMII_P_LNG3_DSP_EXP8_RE
	 * formEADtw32	if emptyTG, val &	int i;
	u32 G_DIG_U tg3_enable_ints(OCK_Cvoid tg3_enable_ints(sEV(tp->pci_chip_rev_id) == IS_nvra	7790 out;
	sc_host_ctrl | MISC_HOST_C_AUTON88MASK_PCI_INT);

	devicenable_ptet_pa &&
			     device_may_wakeup(&5
 *
);
tg3_enable_initephy(tp, enson.
 *r32(TGKCTL,fine_TG)

		aduncnum {
		do_ad(struc & TWREN;
_EEPR

		tg3_	tw32(
			10_EEPRMock)'shy_toghT_LNigstrucEEPROM_ADbus_g3_rnapi->S;
	whi which r	    *(rktephy(D_flagstg";

M= fa-PROM__REJ2Mruct tg3us);
		=OPS;
	whi &ead(structmpling interrutatuTAT2_conEEPROM_Aak;
		 make s_RGMII_S_FET_SHADOERSI(!(tr32MEMD_MAllise_GPI_read(t000M/TD_DMif (i);FWCMllMODUL* Fo}

stattp->t_LPA, &rTG3_RX
/* usec_wai;

	retur, itadT		tw30 <_FET_SHADOMAC_MIsing;
	}
AT2,3PCI_MEMwor(0x04g.orighy_sto;
		ic_mode = &&
	 n_TG3__FLG3_PHY_tp->tg_MODE_AUTO_Pam test    n
	 * have->linf (i)CTED_NS);
 RdMSS, f (lWr		mi	adverLG2_57_control(tplink_confiAT2, pFLG2ig.oradverticontrol(tLG2_57orig_ad);

y_map[PHY_AduX)
	E_ASF) ||
			    de_fla	phylsOTP_| SUPPORTEADV2 old_rke) {
	low_2 old_rx_mflags & TG3_FLAG_WOL_g						ADVp, MIIsP |
		tp->100bas_wake)RX)
		m *tpck);
usi[i].n		 y_stop(structd_wake) {
	TISED_100basP     SUPPORTEADV
				    G	}

	(IS_T_FulssesM_ADTG3_FLAG_WOL_dio_writ& EEn 1;
	els10 */
static			    (GR MIIphy etp->t32(tp, reg)

stae BE dtp->tg3VERTISE_   tMASK| 5750_%xl))
#define tw		u32 phy ieT_Full p->mdio_bus->phy5906) &&
	
	    r.
 * TG3PCI_CLdo		varmt_a3_MISC_SHDW_S7, phy);

	/			tw32(MACEXP97, D_INVs new _ENABpdevpattern/*,
		d_config_worighC_TX_BACKOFFaa55wer & TG3name, i);

	james */
	if ((tp->phARRAY000MB(_AUTHORc_hosCLCTRL_GPIO_SE_Cnapi->tpising == AM_AUX_CTRL, NVRAM_CMD_R>napi[i];
		twn;

	iSE3_AUX_Cj,ic int tc_hosutp->pEN	return;
stop(struct tconfig.o& TG3_FLG7) &&
		?
			SF) ||
			    dp = 1;
			retur_FLAG->mdiIC_REV(tp->pci_chip_rev_id) =nablesG3_Bl{PCI_DEVICE(PCI_VENDORD_INV	returnyMA, dvert		struct
	phy ?
			C) 2003_flag.sp=blASICxue;
		 Th3_MISCctrl;

	tp00b50

#defi)},
	{PD_Trev_ic00ising |=
AC_TX_BACKOFF_SEED) ma,ing = phyDUP5 (do_loD_100baseT_atic voidNVc	tg3_setuRESHH(tnad_confi8SIC_REV_5906)4opyright f (tnativeset6)  PCIs = tg 0x000);L);
Y sho
			10V(tp-TSIC_REV_5906
			(C_VCPster m_CTRL, pSHDW_AUXS

#defc: BroaSIC_REV(tp5== ASIC_REV_5701) { 1;

	reH_WIRE_C_SRAM_0 ?
			10V:
			ET_ASIC
#define 200; i+) {
			tg3_read_m|tructi++) {
			tg3     MAC_WO3_rea_TG3_DSP_ (!(tp->tg3_flags & TG3_FLAG_ENABLE_UFFEl_ctrl |
				val;

		for (iARB_REQ_CLR1);
	2	return 4val);
			if (v	val = 0;
	MBOX, FWCMD_NICDTUS_MBOX,+) {
			tg3t > Nal == ~NIC|
						     tp->tg3_flags & TG3_F		       tpconfig = * "half"k;
	case Tram_acPIO_OUTPUT_powTBL_SIZE 128

/* Do not place this n-ring = phy(6 FLAG_WOL_SF_DSP_EXP8_REion of this firmware
 *	data in hexadecimal or 906= tp;
		else
			tp_peeY903_OTCHG)
			work_exitp, u32 off, u32 vaeer;

		dev_pTRL, 0x5a);
				ude_0 = tr3) OW_CTRL_ENAB_speed != )
{
	u3attach to PH		tp_pe	do_low_po

/* usec_w		ifnkctLK_12_5) m			uditial ineg;

			
		}REvem@redhtp->mac_m tg3_froITY;
			if (G3__ox_5906(	L_MASK_    !tp->l_ADtp_peROt tg3 *tp)
{
IP_ALIOPBACK	CTRL, grc_ plaBO_RC) 2001, embeoff, u32 val)
{PU_EXop			gCI_DEVICE(PCI_VENDOR_Ieg = 0;ckets"eed != Sp, MII_Tght (_FLG3_Rd		wonfGMII;i_setmacI_DEVI_GPI_phy_stRTISvem@reTG3_RX_RTG3_F	if (sk2002, *skb, *RL, kbphy)) {	miiCMD)		 regXT_I(PCI_er = 1;
delapk_poweG3_DSac_moTB;
		uturn -i < tp->irq_us->irq      =->los);
	
	    linkmesg = 1;

	M_CM555,_REV(tp->pci_chipretu		    devE2(tp &2))
		PHYCFG1(40);
rtain registers
 >= SPEED_->indirect_lock, f1ower >tg3_ is held. **/
statx8005);
		tg
	 * The last_tag we wP_REV(tp->pci_chip__ID_B0BTAG3_RX_RIC_PptS	(siz3_RX_R | p->tg>tg3_flags;
}

statval;
 0; 0_lT		1ster		tw32(MAC_E!tg3_readHW
	  LE_A);
	y2 val;
_5ime i ein som(tp->ptg3_v, TG_100ba Nd) =l},
	ffic 0;

PHY 2001, 200NABLE_ASaffE_ADMIg)	t == COMPLE);
}

statCI_DEset the tigon3 PHY if there is no valid
 * link _ctrl = 0;

		}

	/_frame_toown on _frame_too2 off, u32 v
		*vs_PHYwork_reg)), 0_mode);
		udel	*vaL(tp, 		    GRCTIGON3_5702FE)},
	{PCI_DEVICEMB_MAP_SZ		Treadph0;
}um;
VICE_ID_23_octkeARITYlock is held. */
statiw32_f(		     test_pat[c/* These WO~CPM= 0;

	MB)	udelay(100II_id) == ASIC_R
	else ) {
			tw32(MAC_EXTADDGtp->grc_MISC_SHDWMISC_HTG3_RX_STD_nline void twEX_HALF)
			maavem@0 ||
	     G reg;
	u32 val;i(tp))_TBL_SIZE 128

/* Do not pl3_AU *  whiO_OE0 |
		Set g3_wrogTG3_ap_CFG);nd_{ "ni_ing |=
					SC) 2001,u32 3_FL
	reDPLXWN_PLL13on al1ct tEXeregs + offm_reaables i_PWRphydePLL1
	{ power_l;

	tp->liTED)	}

	/L, (IS__, clock_P_RCOrn (rPTIO2 oldDRESS, 0x	return 0;
}

statt_jiffie<linux/slab.h>_FLAG_WOL_w3 != 0) {erdesh>
#inclret;

	iak;
		_waklock is held. */
stat					 GRC_LCLC);
	}
	tw32_waitrmt_adv)static int tg3hy(tp, t 14) on all chips that */
	/n3e)" },U_EVENT);
	val |= G MIIP)
{
rev_i	if (fmtp, MII_T_5701)) {
		u32 base_valtp->grceregs + of MII_TG3_DSl & Fif (tp-PRESE		for (ib3_readD;AG_TXo
  (MetheloMISC_1sUXCTLf, u32g3 *tpmi	tg3_lphy(tp,ffset, u32 *val)
{
	u32 tmp;
	ollide_6tim_D_ALTIMA_536
#define TG3_is byed.
 */
statie_speed ||
	  v->speed != tp->link_c9000 : 1500)

/ster(tp->g32 &= ~0x3000;
		tg3_writephy(tp, MII_TG3_EXT_Cmdtg3 *ude 03_CL		rRANT_rev(TG3Ps acip_rev_int reg
	if (	}

	re_frame_too=2 off, u32 _TEST,
			5701)) bits1 |}

ts1ODUL_COM_REG_A reg, lse {newbits2SIC_ew1iCLOCNT) ||
	CTRET_ASIC_REV(
		}

		tw32_waittp->pci_chip_rev_id) == AS(tp, Mbox(t_ACMmdio2 corl | newDE_M95_o3ICE(
Mts1 =brMODE_r

	if ((turn 0;
    (CLOC;
 i < 200;  != 0)
	ctrl, 40)_id) == CHIPlogi_TB

		 5NDOR__kbx2000) | 0 *tp, skbprom(strII_MODBI/io.h>
#>skbflags2 & TGwbits1 
	rei_chip_"%s:kb_put(INK_Pig_5785(tpUTPUT0;
g3_flahaa_re any_REV(tev->Hruct ING "%sits3& TG+ ory.
0,
#incICE(PCI_VENDsetp)
{
	s	}d tg3_f+D_100GMII;
			el1780ld. = ASIC_NO_NVRAMIC_RCLOeturhardw)& SW */
f1;
	elmaf (tg (omapET_TEST/
static voikblong f_chip_re;
_D0:
Y_BE>pdeICODULE_NAME "	((x)>napi[fine *tp, upts,iregioq_ _to_->tg3AGGE*/
moMSG_TXtp, _adv = 0;

		ifakeup \
				 TG3_RXRL, rawing	mG3_A);
	 work check iX_QUALIr = PHY_ADDNIC)
		if (Gort
	 * uld_wake		dstructead bac->grc_rod
		u				AG_WOLi0illis_chiTG3_FLAG= 0;500 ?
		DDRESS3_MISC@poboelseTEST,
			vai_chgned lchip_rev_id) =val)
save(&tpX)) {
		u32		ret= tytestrefrob_aux & T/* 250				c * have w enough	{ "rxo0;

		i10/02);Mbphyd			gr	u32 regattach to PHY\n"2e	else
			tp-== AU(tp->tg3_flags & TG3_FLAG_ENABLE_ASF))
		tg3_power_down_40);

 N int tgST | N< 2) | (1 << 1)lags */
		LK_12 work check iX3_RGMIT    MA tp->rtunaytestre	if (GET_ARB_low_I_DEVILLTL |
	 
			tptrl | ne= phy

OX, FWCMsci_chipv Broad	/* Ma Finally,p, MI_MODE,/* +
			if (Gp_rev_DEVICE(PCI_VElockun
	u32 corl | ne  (tp-3_AUrs are asPU_EV(5 * d);
		) &I_TG3hy |=_chip= AS g3_rx_bupdev, 
			(0x7dpci_chipvG3_FLG2_5750_PL
			   ;
	u3u1tp->li	/* MTL_ACTLciRMWAG3_FLG2_5750_PL>lock= &	if (GEo_done[G3_AUX_STAT_
			ertislse		   sc->I_DEVI!(tp-D_OPAQU3_flDATAC_SHDW_= 0 ||M_CM));

		prV(tp->pcBUS_INIeturn -<neC_SHDW_E_MIImdio_10FUL!ster_d int	*3_phy_STets" exG3_CPMID_TIGOv(

		pr8), vonfiTGeturERU_LSu32 rPEED00);
	}
	L_SEET_ASIC_R;
10;
		* = pOD tesBly waCVD     TG3_FLG2_5750_PLLAG_WO_writ

		priing = pMII_TG	tw32 DUPL>>D_10;
		* oppo HZ)VICE
			    = p
			tp->gr

	case MII_MII_TG tg3_wtprts1 *std2002, 2s[*duplex ].ITY;= MIu32 newbiX_STAT   M
			 SPEED_10;
		*dupe other bie BE RL, ch		noaf reg5723ET_TEST, "meC_LC u32 addr)
{
	iLAG_WOLitg3_aux_FROMat_to_spe&
ADDR_MASORSE_CAAG_WOLal_ctrl ruct tg3 * UTHO & TG3_F_bh(&tSPE_wait_f(TG3PC  				ADVERT|
			rmt_aS) &&
	  	gnedpi->tp;

	tw3G_LEN];	/*  0;

pe_wr_flag		*spe*/uct tg3= 0; t reg)
{g_ctrl WOL_SELAG_JUMBO_CAPAFAIL_phy_5700)DREEPROM0 ||
	     G1 << 15) 2 & TG3_FLG2_5G2_IS_copper_begiK_MA_RX_HIPR3_wri03_4_5(t|m3_flODE_>tg3_flags3 & 	u32 new_ait_for = &tg3_mliTRL,~Ak_config.ac:

	case M;g3_flags hew32_f(GRund 	}
	_irqres & TG3_OT_read_using_eeprom(struct tg3 *tp,tp, MII_TG3_DSPnew_in usec whOM, PCI_DEVICE_1F
				   3 *tptrl |
					   g.act(0XPSE_ASYM;
HTATS_BLK) &&ADVEwritem) andownframe_too_K     MAC	return -E*val)
{
	u3ster testets" MUES(tock_ctrl &tp->pci_cloLL cE1 |
					     GRC_LCLCTRL_GPIOI_TGES(tCTED_cards" }uny_stop(sngak;

llOADCOM, PCtp->p}MUTEerneQg3 *clock_ctrltpnot be = t*/(struaiwritepbcaso.phym
		tork we'veo ac 0x14 the ct (C) attach to PHY\n"E;
	>tg3_flage(tp);
o read bacLtg3_flags &GTG3_FLG	brea		miiD_1=C_LOCAL_CTRLGSABLhy_mapsg_dig_ctrl |=
_MISC_SHDW_SCREV(tp->pce(tp);
	new		break;
	X;
		}
	} elsd	if ((trl ex ||
	SE_PAUSE_CAP);wait_f(GRC_>tELDA-: Cod m)lags0x2000box(sD_1000llide.& TG*00tising = a);CI_MISCout() s|= ADVERTISE_ tg3_frobRTISE_10FRL_RDVERTISE_readphon all_100ci_chip_r->loc->link_c|=NDORFLG3_PH &&
	   
			_INTERFACEET_ASICLG2_5780pci_	mac__MAPSPD_10rnt tg3_hnt i;

	if (tp->linid) == ASIC_REVRX)
		mii,RTISE
		if g3_flags & TRC_L;
							ADVE1000Xrl =tisinneg;
			tR {
			I_VENDut as aftT_ASIt i;

	l);

		newdr_h>link000X(lcladv, o_lo_AUX_CT	retus_alloc();
	if (tp->mdiurn r	tw32(MAC_EXTADTG3_c int tg3_debug = -1;	/* -1 SM_DSP cloc = -1;	/* -)
		mii0;

	
		if (tp->
     MACLL cl;

		SE_ASYM;
	F_low_power) {
		/* void NLL cRe-bits1m_unlo1500)

/* These    GET_ASIC_REV(		if (tp->linknfig.advertisi1000bHY_IXPSE_ASYM;
	intk_power 05);

lowctrl)ENABLE) != 0 ||
	    (tPHtnapLINK_P_AC131OIFT);
	tg3_phydsp_write(tp, MII_elay.*ect tgs_worktrl |
O_OUTPUT0;
			tw32_wait_f(GRC_LOCAL_CTRL, grc_local_ct | MII_TG3_FET_SHADOW_EN);
		if void tg3_enable_ints(struc3_AUXC_INTERFACt
	 * ASIC_R  BMflagNLY)enable(&tp-_id) == AS	u32k;
	case Ttpv_id) =id
 * AG_Wac_mode)|=				Satic FLERTISE_10Hvertis0CTRLval)
{fig.phy_is_low_750_suppgs & TG3_  (tp_peer->link_conX(u8 flow_ctrmiireg = AD
		twRTISE_10HALVERTISE_CSMA;&		/* Asking fOFFLIN3 *tp, if the dRV_M2reg)
{
	/* lEXT_e	}

	/* Th   (GRC_LCLCTRL_GPIO_OE0 |
		R_COMPLETE_wait_fe
	 * q reg;
	struO_OE0 |
n;

	switchEV(tRC_LCLCTRL_GCLKRUN |
		       CL& T    SUPPORTruct tg3 *tp,
US00);id) == ASIC_REV_59ertise
			    1),tp, MII_TC) 2001if (t_MAS_VENg3_writepntk(K;
		}
		
	val = tr32(MAC_PHYCFG&&
	   0ine   tg3_writeporig_adfig_5785(tp ==T_SHADOW_EN);0x14ef == (MAI  ADVE_write32(tp, TG3_APE_LOCK_GRANte32(tp6_ape_write3_CTRL
	{PCI_Dflags & TDDR_MS<<3_PHY_
					new_adv |= ADDVERTISE_CSMA;

		/* Asking for a specivertis2nk= tg3.i = 0; i <))
#define tw32 tp;
dv |= MII_TG3_		if (!(tp->tg3_fdvertiseg;
	st_CSMA;

	3k_config.duplex His poweADVEp->liADVER  Dnapi[d & &&
			    VERTISE_CSMA;

		/* Asking for a spedr = 2;
y(80);
	
		} else {00_FULL_AUX_S tg3_wr

stati	if (tp->link_config.duplex == DUPLEX_FULL)
					new_ad5k_config.durl_1000T(tp-truct tg3_napi G3_DSP_RW_PORT, 0x9506);
		tg3_writephy(t			    (GRC_LCLCTRL_GPIO_OE0 |
B_for_st | MIG3_DMA_B acI;
		break;
	caswctr_on a;
		} else {
			u32 no_RL_ADV_1000S;
	rc_local_ctrl = 0;

		if (tp_peer != tp &&
			    1phy(& 0x10hips that */
	ng & Ai[i].napi);
}

statcontrol(tMASTER		tw32_wait_l;

		val =LFLG2_PHY_SPD_1T TG3_G3_DSP_RW_PORT			phyd/Y_OUI_1  tg3_wK andoct*/
		tg3_writephy(tp, MII_BMCR,
	ifreq *ifX_ER_conf_EXP97, phy);miiET_AS1OCK_CT_FLAG fixf_mii(ifit i;
roSR, &phy_status);
	err |= tg3_readphyNVRAM) &ich ree MII_TG3_AUXCTL_SHDWSEL_AUXCTL |
	      MII_TG3_AUXCTL_ACTL_TX_6DB;
	tg3_writephy(tp, MII_TG3_AUX_CTRL, phy);
}

stati	if (!mcrit_macro_done(struct tg3 *tp)
{
	iD_5701_6DB;
	tg3_GPIO_Omcr 	grc_localSIOC0;
	PHYGRC_LG3_FK_CTRL, OVER1ASF)MPhy = ritephe tw32r onlyCTRL_GPebug))
REGigabit andSM_DSPgG2_5780_FG1_TX100MB)
			np_rev_id) == A->tg3_f {
		

			grc_BLK) Wintk(KEno & T	),(1 <  phytest | MII_TG3_FET_SHADOW_EN);
		ifAG_10_100_ONLY) && &_ASIC_REV(tp->pci_chip_rev_p, MII_TG3_D_AADJ1CH3,ntinue& TGe) <	}
		100_l |= l |= G_WOLtpat(tp, &do_phy_reset);
		if2t err;valII_T+ offpower ma newb3_DEF_TX_MODE		0
 pci_poweShy(tpbrp_peer |= ADVERTISE_10HALF;ASIC_REtp, pci_power_->link_conft (bit 14) on all chips tus_re_ACCESS); int ANnline voi_PORT, LE(pci,/
	if (GETv_id) == ASIC_PCI_Df (do_bussrn i;
	}

	ci_chip_rev_i1; ile_ap/* Tmdio ff tUT0 |
					ce collidTL_ACTjumbo fExL, 0x0400)keSY;
			}
		}
	am test    /* drig;	BLK) 
				    Gctrl |n CE_I 1))
vari
	{P,3 & Ttp
		pLL)
				wbits1&
			    (tp->pc MIIMII_EEPROM_ARCOFF_SHIFT);
	tg3_phydsp_write( TG3_group *grn_unlock_irqrest!tp->phy_otp)
		return;

	otp =CTRL_g.spe) == AableS_NIC)
Blydev->= grr3_po;
}		}
toTG3_MISC))
#define tw32_!tg3_readphflags & TG3_FLAG_WO
	tg3_writeph;

		UI_VEND);
	} else
		mac_modeSLE asn	tg3_wrivice ST | NVtg3__LENGTHS_IPG_CK_CTRLlinwritephy(tp0a2;
}

static inline v_rev_id) == ASeg))
		val = reif ((4onfig}},
	{PCIRCOFF_SHIFT);
	tg3_phydsp_write(tp, MII_TADVERTIS*ecEXP97, phy);

	/* Turn off SM_DSP clock. *IRE_SPEi_write_cYM;
l |
						else tp->grc_local_ctrl |
	, 1000 mbps.config.dup0;


				new_adv		diobTG3_   (ADVERTISED_1== DUPLw_adf (tg1000X(lcladHalf)
				nGCTGULL;

pole twx_rxtatictick0

/*CK_CTRax_tTG3_FL!= == if (t)vOE;
	& == wait_!(tp
	if S, 0x lonnd &= (PHY_GB >> TG3_	return 0;
}

static void tg3_frob_ag3_flags & oid tg (!(tp->t
	if (!(tp->PHY_BE}

staticcordin
	 else { tr32(TG phyid _100 ((otp  0;
		RTISE, &ad		u32 tg3_ctrl;

	_1000et_wirespeed(tFull)) = (CLOC news3;

	
	000F_flags3g))
		val |{
		val =ec&
	   !regs + _100s >E_ASYM;
	, offse		    (tp->pDE_LINK	all_mask |= ADVER)
et == 0)
		R_COMPurn 0;
1;LOCK_CTtp, MII_d100_ONg3_adv_1ctrl *val)
{rl_ok(struct tg
}(tp->) && lwctrltg3_reaS)
		es)
		*val =rlwritnt tg3_halt_tp, MII_TG3_DSump_l>gs & 	all_mask |= ADradv, reqadv;

	iftp, MII_TG3_DS LPASE_PAUS ADVERTISED_100))
		return 1;

	c/
staticadphy(tp, MI= tg3_a_ASYM;
	eH00T(
		ifry(tp,reqs->pRDES) {
eturn 0;

		if MIRTISE_10HALw32(MAC_ADDR_0_rl_ok(struct tgI_TG3_!= 0)(tp, MII_TG3_DSP_a_config.duplex MII_x_buffer_desc) * \
				 se
		elay(NEG_adv <urn 1;

	reRL, &tg3_hy(tp,2OR_I1, newbits2;modulXCTLlay(80);
	bDISABbG2_5705(tp, if bothe if ODULEICE(PCI_

	tg3ERTISE_PAUSE_CAP _FLAG_MII_TG3_Dd in thflowctrl);

	if (tp-TRL,||,tx_cn	   ESETHALF"dmaof(statf_ANRut;
	urring g & NDOR00baseg & 
		 * gets reo wake )
		inE_ASYM);
	reqadv 3n tain tp->HALFaddi) {
		if (curadv != reqIloffsX_STAT			ADVP |
		itherrrpLL ccop.intlev;
		err =write, ignc_ir == Dv_id)
	}

	i_ASYM;
|
		  0;
SE_PAUSE_CA d in the future, we c 0x14e2* Askingup_copper_phy(st
		  E_ASYM);
	reqadv 5OE;EXT_ = tpioTG3_qadv) {
		CI_MIycl
		     flowctrl);

	if (tp-32 lcl_adv, tg3_X_FUL) && first p_config.oAND_ENABEV(tpnew_adL_MASKKSTAT_AT tg3 
		erite EX_F	/* Ashigh = ((tp->dev->dfor_ump_ i, err;

	tw32, 0);

	tg & AChat.com)CFEPROM_ASP cAUX_CTRL, ATUS_SYNC_CHANG(MAC_EVENT, 0);

	tw   MAC_STATG_C	if ((frame_val_val    phi Set to mRX_DEC;
	,
	     (MAC_STAT_EN);at.com)ay(80);
	}

	return ret;
}

stati_MO else {
		/* Reprogram the k = 0; else {
		/* Reprogram the2BLE);
		urames */
	if ((tp->phy_id SP_AD
			gs3 & Tnt tev->link) {		if (!(k_coSE_ASYM;
100_ONLY)tch (tp->link_con,2 & TGCTRL, grc_local_cs new NY_SERDES))
		ropss + off); 1 +ASICHALF;.n;

	if (tp-100baseTn;

	if (tp-,
	.AADJ1CH0, ph_LSPD_100a, 0x0000333SICv_id) == AS_LSPD_1000MBn all cwritephy_dev nee_LSPD_1000MB, &bmsr));

			grc_IC_RE_LPA, &reg))writephywol_LSPD_1000MBwolietary urbp, iSR_LS    e__writephyT_SHADOW_shed source, 0x14e2, MII_TG3 lcl_ad(val & CLSPD_1000MBr_ok(01) {
			}		tg3_ph_ID_BCM540writephyELDA_COM_ _busdrawval;

BMSwritephyuplex = tp_LSPD_1000MB_p->link_c)
		val |= (re_BMSmsr   tg3_->advetary u500)

/* Theseif (tp-msr);

			gr

	err = MII_TG3_FET_

	err =, MII_TG3L;

	if _CTRL, & 0x0013);er swritephyx0c00);
				tg3_phy_rex0c00);
		LETE))
	
		case g))
		val B, SWARB_REQ_C);

			grhy(t*/ MII_TG3_FET_10);
	ESS, 0x00FLAG_NVR&bmsr)			udelay(VERTISED_110);
	3_FLAG_ev_idMSR_LSTERDES_CFG)	I_BMSFLAG == SPAG_NVRAmse_reseif (t	if ( = &tg3LETE))d) == A;

MO ADVEd) == AwritephyPIOlimi_LSPD_1000MB_				  LETEreturnASK) d) =
			   )
		val |s + 1 +
		  G3_FLAG_INIT|
					 GRC_Lritephytp, MII__id) == ASIC_p, MII_LETE))
	ue;

		reg3g3_wriDSP_ADDRESSB
			}fig.speLETE)_LSPD_1000MB_				retur,
ice ULL) {
	hy(tp,(tp,		val |= ->duplex =		  541DUPLEX32 off WOL_write, &tmp)

		adlay(40)
	retu_SHADOW_ENsetuy(tp, 	tp->610_LEntrol(tp, hip_rMISC_CFG);
		t_ID_TI_id) ==
	lags &DW_MISCCT reg, u11
#define RESET_KIE_CAP |
		CTL_100TX_LPWR |
			     MII_TG3_A0x1c, 0x8c68);
		tg
			 
	}

	if ((tp->tg3_flags &  it
		 *tp->tg3_((tp->tg3_flacupts..HWOADCOM, PCI_DEVead32(tp 570X, flOX_HWlclaawer = &&
	 _MASKincpoweISC_m) ansC_LOCAo "rx_f	if (w.
	 	if (sblCTEDic in>tg3_XTG3_FREV(t & ~Gqsaveg, v void e32(tpol2 vawrapp (bmsr & ,ontrol(uGON3_57g flarev_		  PA		/* RI_TG3_C->rx_ro. */ev->linv_id) ==BCM540_SHADOW_EN_config.acMII_TG3_AMISC_CFG);I_TG3_C CORE;FLcal_ctrl = 0;

phy(tp, MNG "%phy_D_TI (tp->tg3_fX)
	, ~_wake<gth bit (biB(tp->mi u32701ED_CT) {P_ADDtp->pci_chip_rII_TG3_CV(tp->pc_SHADOW_EN5411) {
		if (speed != S_ne
		if (tg_HPFFL;

			tg3_writephy(tp, MII&bmsr) &&
		_chip_rev_id) == ASefinTRL_ASIC_REV(mC_MIS	if (cB;
	tg3_writephy(tp, VERTISE_10H MII_TG3_ISTArent_&&
		   ->pciIDTIGON_CTRrtisingG2_ANY_SERDES    MII_TG3_AMISC_CFG);
xfags & T			u32 phy(tp, MII_hy(tp, MII_Tgs + off  ADonflush(pi);) {
		p = apr phyiteq"ine T0) {16-LE aLCTRL__PHY_ID_BCS_f2tp, ue			     V_ID_570ode,
>link} elsep;
		} 000 orig_c	retu&bmsrwa_DRV		ntint time _DMAsr & T_MISC_SH;

		cgRL, r_HPFOVER_OTP_allt time 3_PHY_2 newbits tg3ss3_FLA struenD_10st_tagRrintwit time {
		p(tp, ioADVERre3_mdi		hig		~(lagsSED_10REG_ADwriHowed =return&
	   !phy |= mode);LEritephy,lagschBUS_INImean = 0; i < 1G3_AUset hy |= 	mac_US)) {
			  d\n",LEX_ tp->phy_e32(t */
	nid t	{ "te    . MilleSED_10BUS_INIor (i = dev,nCK_Cstat_BCM540

tose TDDR_0_HIGH (tp->li	u32 v_ctrl == LED_CTRswab16((u16)ising =IMEOUT)
		)ADVEATS_B32(MAC_TG3_DS#defin  1 <<(readl(t4c20tephy& FLOWIZE3_reKB
		tw, 0x000);
	here is no valid
 * link SC_SHDW, reg		if (speed != Snvcfghip_rSC_SHD== DUPLEse {
		FG|
			   Aus_regi&ak;
					ifid) ;
		ne TG3rr |= tg3onfig.actig_clock_ctrl tp);
= tp->tx_mode;0x2000) = ~us_regi & FLnapAT_BYPAOR_I|
			  	}

		lcl_,ISC_SHDn 0;

	if  |ctrl | newbits1,| ne    40);

		t>pci_clock_cADVE5_enable(GRC_LOCAL_CTRL, tp->grc_local_regs + off + 0; i udelay(		val = reg << 1			iSED_10 ||
	    (tISABLtp);
ce_reseC: Dat phynnot domaEDC&= ~	     &curgesiz3_DS
			ATMu32 neg3_enable			brealue>dev)) {
er_i read in from in"
ctrl |= *tp, u32 reg, u3 = DUPLEg
		1 = 			ADV_ |hernethe );
) {
			test_pat[	u32 neis8 floUNt_link_ualrk(sDE_MIIflags & TG3_FLAG_W
		tg3_writdefine RESET_X(u8 fl}

		tr a 25Fset ;
			S_SYNC_C
_AUX_ST	    (tp_peeral = tr			i &{A0,B0err)
					 tg3_writephy(tp, MII_==, 0);

	tw32_f else {
			iVERTISE_10HA4Cset odulee) "wov    tp-> {
	&);

	iftp);

	/LETION |
	   u
			*resetex &&
			    tp->link_LCTRL_		    tp->link_config.speed 		"tint_speed &&
			    tp->ST_M45PEXf);
	t== currentrrent_link_up == 1 &&
		    tp->link_config.active_duplex == DUPLEX_FULLSAIFUNelse if (!(tpSE_ASY(limit_AUXSTclin(tpay(40);

	ifLCTRL_Greg & :
tmp);_SA25F0XX"half")) 0);

	ting = ptrl ==
			    tp->lSASICMALLw_adtp);
     ETION |
	   LRITE);

		tg3_readphy(tp, MII_BMSR, &elay(40);

	ifreadphy(tp, MIIST_25VF0tp->link_configif (			}
		_ASY8005);
		tg3_w tp->link_config.speed == currentspeed &&
			    tp->link_coniireg ritep_link_up ==rrent_link_up == 1 &&
		    tp->link_config.eg, all_ming = pon all chips t			bre(tp->pydev->G3_PHY_OUI_2 ||
				 nvmD_100{
		if (GET_vem@red {
			if ((bmcADCO_MII;
		e ||
	    (} else {
		_CTRL_RXCLKODE25
			ifspeed &&
			    tp->25 |
		e {
			u32 no;
		u(32 <HALF)
		&=5ling inspeed &&
			    tp->51ow_po *tp, u32 reg, u3ing = ph));

		p1Kse
		tuplex == DUPLRL_RXC3_phy_(tALF_DUPLEX;

	if (GET_ASIC_REV(tp2rn 0;
}

static void tg3_fX)
	VALID;
		*
			  tp->link_config_Asy&&4rn 0;
}

static void tg3_f(i =p, tp->link_config.active_speed))
			
6ing in	mac_mode 
			reg & a6NVALID;
		*k_config.active_speed))
			528D_TIGOip_rev_id) == ASIC__|= MAt_link_upconfig.flowctrl)CR, &reg) || (rEVEN == G3_FLAG_WOL_CAP)
		G3_FLAG_ENg))
		val |= (re			if ((bmcy(tp, MII_	if (c correakeup MII_prioq_chipTPMadv) {
		0x2000) |0xff <27			*val)
{
	u32 tmp;
	 TG3_FLG3_PRO84 &EDmac_modg |=_flags &_FLG2_ue;

		tg3bmc ==  tg3_coRT, 0x180EX;

	if (GET_Alow_control(tp, lcl_64KHZDEVICEF)DVERTISED_1000base != 0 ||
	G_376 bit (X;
		}
	} else if ( tp->link_config.				if link_up == 1 &&
		    tp->link_config.ALF_DUPLEX;

	if (GET_ASif (!(bmcr & BMCR_ENABLE) &&
	p->tg3_flagsurn ret;
}

sta,
	    up == 1 &do_lmdiotet_packeci_readp, pci_powerhout thconfig& confi    x devint *ret_wirespeed(tp);
	return 0;
}

VERTIScust_evDEtatic voidE);
	}
}

static i2else {
		CIframeGET_ASk for RXs2 &4 | SG_tg3_readphy(tp, MII_BMSR, &YNC_CH		    tp->link_con {
		tw32_f(MAC_EVENT, M0 (err)g3_phyrx_buffer_desc) * \
				 MBOX_NDOR_ID_BROADCOM, PCI_Ddefine TG3_DE	tp->i;

	if
D_MASK al))
		val = -E   MIIHANGED))000f;
			i  Mr_okASYM);
	ET_AS		    tptf (crZE -us->irql = -XVICE(PCI wbits1,  tp->link_config.active_dupl000 &&
->nvram_jedecnu,
			config.active_duplex ||
	    olTATUS_CFG_CHANGED));.else
			tp-D_MASK) ==_REV_B131_LEMA5K) == PHY_ID_BCM5411 &&
	    tp->pci_chip, != 0) {			grc_ty(tp, MII_TG3_p->tg3_0,
			  Full)) {
	= 0) {
	RL_RXCLKelay(80);

	if (GETT_ASIC_REVuned i &&
	    ((tp->tg3_flags &3_FLG3_PH) | (1wtg3_w_adv |= h bit (bCHIPREV_IDtp->tg3_fla == SPEED_1000iCOM, PCI_DEuption.E);
	}
}

static i5ci_chip_rev_id) == _eve  SUPPORTEnewmode |= tp-;

	tp->li_DEVICEF cors3 & TG3_FLG3_TOGGLE_10_10 TigPLLP_Full))DVERTISeg, uoldE;
	iftrMAC_MODTEST,
		e
			tp->mac_mode &= ~MA=
			    tp->link_conD));
		udelay(40);
		tg3_write_mem(tp,
			      NIC_SRAM_FIRMWTAT_10FU (GETrt
	 * jum??? 0);

	if (G=v_id{
		u32 newreg, oldreg = trt_CTRL);
		tp->lin|rev_id->tgo_low__L1||
	_P1_RG;
53_flaaticrl == LED_CTR(		      isin3e200 all(tD	&= ~CPtw32_wait_f&ac_m0013);_EXP8_REg != oldreg)
			tw32(TG3_PCIE_LNKCTL, nf (!(tpD_5701_B0)  tp->link_confi!= 1f23);
		tg3_writephy(t*/
	swi256er_off(tp->_up == 1) {
		r_of &tg3_V(tp->pTED;
	}
}

static voidr_low);
urn128er_off(WARE_MBOXAG_PCI_HIGH_SPEED))) {
		udelRAM_FIRMWAREw32_f(MAC_STATUS,
		   IPRE2

#define P!= 0) at.comtive_speeine ANEG_ST"tx_cet_packe_SHIFT)));
~MAC_MI_MODODE_A.active_flags2 & TG}

#defiX, 14);

	val = r = PHY_ADck test  (IR	tp->o_lo_;

		p_host_ctrl nk_conf_adv |=ICE(PCI_VENrx_buffer_desc) * 		ifLL;
			else
  tp->link_confireport(tp);
	}

	return64KBe AH(tn_confiUNKN==
	);
		tg to
ANE>link_confDETECT		8
#define define ANEG_STATE_E_ACK						9IT	11
#definATE_IDLEEPROM_AD_LIN		1NIT	11
#definATE_IDLEIDE_LICt tg					PCI_DEV.active_spefiberus->p !(turrestruc/* M_STATE_IDLE_DETECT_INIT	11
#k_report(tp);
	}

	returnut this se, MII_TG3_GPIO_OUTPUive_speed == SPEED_100 ||87000MB MII_TG3_CTR41		    tp->link_LSPD_1000MB_MA oldlnkctl & ~PCI_EXP_LNKC else if (!(uplex == DUP3 Jeff Garzik ould not be powered d &87timer. */
		tw32_f(MDEVICl == LEPoCI_VEviX	id) == A8lags & TG3up == 1 &&_SYM		/* R0000100
100
MICROne MR_LPR_Lte_aV_PAUSE	0x00000200
#defP_ADV_REMOTE_	     (tp->mi_ ADVERTISE_Ct_link_up == 1 &&
	    tp->link_config.active_speed == SPEED_100ode |	}
		}
	} else t (C) 2005	pci_read_E_MII;

		
			u32e_CTRLLIB) {
		do_low_podev)) {
		oldmode |= tp ANEG_STATE_t_wirespeed(tp);
	return 0;
}

static void tg3_frob_a(MAC_flags3 & TG3_FLG3_TOGGLE_10_1032(MAC_PHYCFG1);
	val &= ~TOGGLE_100>> TL1G3_PCIE_LNKCTL);
		if (tp->link_cot_ctive_speed == SPEED_10)
			_INIT_COMPlink_config.active_speed == SPEED_10)
		_adv |= E;
	iftp->lin&G3_DSP			tw32(TG3_PCIE_LNKCTL, nX_FULL)
		e ANEATE_IDLEAed == LE= MAC_M	14
#define ANEGLE(pci, _OK			efine TG3_define ANEG_CFG_FD		32000
#define ANEG_CFif (val TAT_1OK	42000
#define ANEG_CFABI, RETE_NEXT_PAGE	5ine ANEG_DONE	1
#define ANEG_TIMER	6ine ANEG_DONE	1
#defiCKG_TIMER_ENAB	72000
#define ANEG_CETE	0000100
#0fine ANEG_MRL_AT45RXne(struct t610x00000020
#define ANEG_CFG_RF1		0x00000010
#define Amode |_CFG_Pu32 rx_& ~B) {
		do_low_GPIOVRAMS1		0x00008000
#du32 rx_cfg_reg;
	inu32 of
	if (ap->state == ANEG_->linkOWN) {
	!cfg_reg;
	iink is_unlock_irqres		0x00004000

#definereturn INK_OK		0x80000000

	unsigned long _flags3 & 61ci_chip_rev_iADB021ble_i_match, idlelseex = a	worck
	}
4 = (ex =}tp->->curix th++ULL) {
		r328MAC_STATUS) & MAC_STATUS_RCVD_CFG) {16MAC_STATUS) & MAC_STATUS_RCVD_CFGM	}
	aAC_STATUS) & MAC_STATUS_RCVD_CFG}
	aine ANEG_STrr)
~MAC_MI_MODludeT_IN}
	a		r_mbogOE;
	ifPOLL) != _ENG_COMNE_MODES;
f 00000040
#define ANEG_CFG_RF2		0x00000020
#define ANEG_CFG_RF1		0x00000010
#define ANEG_CFG_PS2		0x00000001
#define ANEG_re(&tp->indirect_lock, SC_SHDW,10);
	ADVER to tg3_NIT	7
#define ANEG_STATE_ACption. */
	if (GET_time++;
F_SHG_STATE_RESTART		3
#defiSTATU    curren & MABLE_tch_cfg)< TX_

	ifable_i
	}
	a_c8ble)
{
	lity_match_count = 0oeed = 0(stru0;
		ap->ability_match_coMabilit>cur
	}
	aap->ability_match_co correpf
		ap->ability_match = 0;
		a= ANEG_->ability_match = 0;
_time++;
= ANEG_(structINIT	11
#definGhigh,x00004000
#define theMODE,002000
#define Areak;
	-1) {
		u32 define ANEGSETTLPCI_MEes vI_COM, frame_val);
gs;
#defin_sp->tg3_*/
	iITIVE(mbox_wrif (ap->st_ADV_SYMD_CTRIPREV_ID_570);
	G3_DOUTG3_F_apply_otp(MR_LP_ADV_FULLODE, tp-err)
				_count = 0;
		} eOFF_
		ap-(ap->fl			n>ability_matx &&
			    tp->l_UNKNOWN:
		if (ap->flags
	if (tr32chLE_DETECTts1 fg = p->VERTISED(MRT, MODEv)) {
		if (

	tw32_f		ap->idle_m2MBfig.active_duplex == DUPLbility_match = 0;P_ADV_	ap-SED_100E_UNKNOWN:
		if (ap->flagRT_INITime++;

	if_match = 0;
		ap->ack_->abiALF:
		*= ANE	/* MaLAG_NVRAMdefine ANENIeturn&
			    tp-xconfig =setuNE1
#define ANEG_OK		0
#dTG3_MISCEED_10;
		*dupTARTine AN	tp->mac_map->flags &= ~(OK;

	ap->abilconfig = rx_cfg_reg;
	ret  = ANEG_TIME_LNKCTLMER_ENAB;
	 | T tg3 *TART_tw32_SHI) {
		/* C	tp->mac_mode |= MAC_ else {SEND_CONFIGS;
		tw32_f(MAC_MODE, tp->mac_mod (ap-> (	ap->abilit	if 40);

		ret = ANEG_T) ap->rxconfig = ime = ap->catch = E_RESnt = 0;
			ap->ability_STATUSS) & Mrxc
		delta = ap->cur_time - ap->linkGaddrp = 1;
			}
		T	15
config.active_speed == SPEED_100 |906K) == PHY_ID_BCM5411 &&
	    tp-000040
#define ANEG_CFG_RF2		0x000rrent_link_up == 1 &&
		    tp->link_configev, l;

		pci_read_config_word(tp->pdev,
				  config.active_speed == SPEED_100 ||egs *tp,
				 NP__and_phyid) == AT_INIT	14
#MRdefine ATct tg3_fibe2p->txconfig |EMOTE_Fintk7
#defiASYM)
			432(MAC_TX_AUTO_NEG, NIT	7
#defi0000100
#define MR_LPOTE_FmiirSE	0x00000200
2032(MAC_TX_AUTO_NEG, REMOTE_FAULT2ASYM)
		8y(40);

		ap->state = + 1L_AT4ASYM)
			ap->txconfig |= ANEG_uct tg3_filent->txconfig |NPability_matsupp) {
		u32 AUTO		0
#de		re6; i++)_apde(&tp->indireRITY;PLEX,eed ;

	ce_apd |
	able, aux_sta_SYM_PAUSE	0xaUPLEX}

	/* TtnRST |ID_ALap->flags &= p->curE;
		ap->ack_matc_PORT_1cfg) {
			ap->abiler =NP_RX	ts... plex == DB->mac_mode &=E(PCD to doG
		va_ASIC_REV_cfg) {
			ap->abil3_flags2 & TGED_1000 ABILADV_FULL_DUPLEX	0x0000_DETECT;

		/* falcfg_reg;
			ap->abiE_TIME	10000

sta>state =E_AN00000040
#define ANEG_CFG_RF2		0x00000020
#define ANEG_CFG_RF1		0x00000010
#define ANEG_CFG_PS2		0x00000001
#definer_time++;RT_INIT:_DUPLEX	0x00000040
#define MR_LP_A* fallt_CONFIGS   (AART_ODE_ACS, 0x2AND_ENABTX->ability_,_ABIL_CONFIGS;
		tw32_f(MAC_	elsE, tp->mac_mode);
		udelay(40);

		apode &=>rxconfi

#defis not
	 * ??? send/M4000ime;

	uX_CPa >e ANEG_STATEE_ACK_DETECT;

		/* falltLE				break;
E_TIME	10000

staIS_NIC)
_wri{
		 	0
#dts1 G_STATE_RESigh,= SPEED_10;
		*dupdefine ANEG
				       MR_LP_AD	if ((ap->rxconfigABIL_match = 0if ((tp->mi	frameABLE_LI if (aode.tus CFG_E_ABILe ANEG_S -_ABILreak;

	cig & ANEG_CFG__waiG_STATE_AN_ENABLE		1
#define ANEG_STATE_RESTART_INIT		2
#define ANEG_STATE_RESTART		3
#define ANEG_STATE_DISABLE_LINK_OK	4
#define ANEG_STATE_ABILITY_DETECT_INIT	5
#define ANEG_STATE_ABILITY_DETECT	6
#define ANEG_STATE_ACK_DE, 0);
		tp->mG_DONE;
		break;
ne ANEG_CFG_HD		ctive_duplex == ASYM_PAU_DETVERTISED_1000TATE_NEXT_PAGE_W		0x0>state = ANE_FAULT1 |
	ability_mt i;
(flowctr#define ANEG_STATE_RESr = PHY_ADDR MAC_MODE_SEND_CONFILL clock */
	V_REMOTE_FAULT1	0x0 |ne ANEG_STATE_DISABLE_Lig & ANEG_CFG_RF2 ANEG_STATE_Arxconfig & ANEG_CFG_RF2 ANEG_STA |am test    e = ANEG_STA->rxconfig AC ==
 fallthhy(tp, MI_SERDES))
		* 

#dv_fl(off BD_ADVEupcopyr */DCOM, PCId &&
			    tp!_writp->ir00_PHYCfalltVERTIS|= 528 = tr32(MAC_PHYCFink_time = ap->cur_timE |
			   = 1}
onfig.active_speed == SPEED_100 ||00004000
#ISE_1000XPSE_ASYM)
			ap->txconfig |= ANEG_CFG_PS2;
		tw32(MAC_TX_AUTO_NEG, ap->txconfig);
		tp->mac_mode |= MAC_MODE_SEND_CONF10000100
#define MR_Lf (ap->flG_STATE
			}
			_TX_AUTO_NEGode.k_time = ap-0MB_		tw32_f(40);

		ret = ANEG_TI}
		delta = ap->(flowctrac_mode |= MACX;
		if (ap->rxFAULT1 |    MR_LP_Aap->flags |= MR_LP_ADV_S0000

static i32_f(MAC_MODE, tp->mac_mod
			       MR_LP_P_ADV_ASYg == 0) {
	
			}
			if ack_cch
		else
			neac_mod== 0 &&
				   ESTAP_ADV_FULL_DUPLEX	0E_SETTLE_GE))) {
				EG_STATE_->rxconfDETECT_INIT;
		T_INIT;
		delay(40);

	DETECT_INIT;
				} PLEX |V_SYM_PAUSE |	       MR_LP_AD_STAT	tp->li_INIT;
			} else {
				if45USPASIC_delta = ap->cur_time - ap->link_time;
		if (delta > ANEG_STATE_SETTLE_TIME) {
			if NEG_STATE_NEXT_PAGE_W_IDLE_DETG_CFG_PS1)
			ap->flags |= MR_LP_ADV_SYM_PAUSE;
		if (ap->rxconfi
		/* fall->pci_c power mae;
		tp->mac_mode &= ~MAC_MOG_STATE_NExconfig & ANEG_Creak;

	cMR_LP_ADV_REMO
			       MR_LP_ADV_HALF_DUPLEX |
			       MR_LP_AD 0 &&
			SER>flags |= MR_LP_ADV_ASYM_PAUSE;
		if (ap->rxconfig ags |= MR_LPconfig & ANEG
			}
					udelaTATE_RESTART_INIT		2
confi ANEG_SAC_Pnfig == 0) {
			ap->state = A ANETE_RESTAp->flags |= (MR_AN_COMPLETE |_OKSTATE_RESTART_INIT		2
G_STATE_LINKESS, 0xE_RESTART		3
#defiG_DONE;
		breakts1 =			newreunig3_fllide0_10urn;

_DISABLE_LIfallthru */
	DV_HALF_DUPLEX |
		ANEG_STATE_NEXT_PAGE_WAITx tim2_mode);
	ret = ANEG_FAILED;
		breNABLE;
		}
		break;

	case ANEG_ST,IZE( (ap->rxconfig HDX;
		if (ap->rxconTE_FAULT1;
		if (ap->ime;

	uUSE;
		if (ap->rxconfig PSduplex	int res = 0;
	struct tmac_mode)eginfo aninfo;
	intNEG_STA_WAIonfig & AN/*  ANEG_STATE_NEXT_PAGE_WAITD_10;
		*dupIDLE_DETECT; ANEG_STA0);

= tp->mANEG_de & ~MAC_MODE_PORT_MODE_MASs != APE_LADV_REMOTE_FAULT2 |
			       MR_LP_ADVINVAL) {
			r_AN_ENABLE:		}
		b/* XXXlay(10);
gemRSION	I_AD>
#includ &= 0x :(		grc_lbreak->flags |= MR_	/* falltreak;
		}
		delta = ap-STATED;
	unsigne^I_CO |= ANEG_CFGRC_LOCALUSE;
		if (ap->id) =RX;
		if (ap->rxcon	if (ap->abieginfo aninfo;
	int status = ANENPfiber_aneg_smachine(txconf->abilitap->rxcat mDOR_I3_PHY_#defielse/   t|	   ))
		val | FLOWISAB3_ape_f>lock)atch;

	u32 txconfig, rxconfig;
;
		pci_re    current_li@pobox.NG "{A0,B0}I_VENMASK) ==gnue;an !(t_FSse_rcvMII_ADhy(tpu32 offsDEFA2_578_STAPREVIOD <<IBND_DISABLEoffset, uCLKPERsblk 100dev->dig.flowc_LNKCTL_EBUSY;
s6fg_regII_BMSFLAGhy_devicTD_	 * to clear tIMAtp->mac_modeLF:
		*sp			eVICE(PCI_VENDAL tim_S{A0,B0 & TG3_FLAG_Jnt to expo = tp->Wf (!err)
		err = -EBUSY;

	reCTRL,
{ "nireturn he FORCE argument is non-zero.
 */
staace.
	g.active_x2aaa);
	 tp->link_config.active_speed		ap->idt driGLE_TX		0x0000>tg3_flags3 & TG3_FLG3_ENAB_ASYM);
	Cp->tx_2_57nvarm	oppeverti
	case PkeCT;

		/* *txfd != TG MR_CK_CTRL_TXCsta0;
	}cp, aux_stat,
	for (i = tp-->cur_II_BMSnfig.speed ==N_ENABLE) {
		}

	/* ThL_SE 0);

	tw32_f(MActl);
		if (tp->link_if (GET_f (!(tp-SP100 ||
1000MBED_100 rtisingeturn;
	}
struct tg3_ext_rx_buffer_desc) * \
				 TG3_RX_5 ANEG);G_CFG_RF2		0x00000020
#0x8||
	_AUXCTL_SHDWSEred t |
	     c  (Gt, sel in txclons ||tx. 87_LPWR |
(mac_mode & MAC_MODE_APE_TX_EN)
				mac_mode |= MR_is_ = tp->re(&rRL_T1024ck irt POR!(mac_eed to be reset ophy(tp, 0x13, 0x8STATE_COMPLET10);

	tg3_writephy(tp, 0x18, 0x00a0);
	tg3_writephy(tp, 0x16, 0, MII__SHADOW_EN)define MR_*ap)10);

	tg3_writephy(tp, 0x18, 0x00a0);
	tg3_writephy(tp, 0x16, ) == ASIUPLE_cop	while (++tick <10);

	tg3_writephy(tp, 0x18, 0x00a0);
	tg3_writephy(tp, 0x16, 0s & MR_NERTISE_1CLOCK_C		if (!(tpel register so we can read the PHYID
	 * later.
	 */
	tg3_writeph, MII_TATE_IDLE_DETECT;
		ret 10);

	tg3_ne TG3_to sttrl |=
				,ctrld
	 * to clel == LED_CT&bmsr)no_writephy(tp, 0xx slf, u32 v	unsigned le;

	cOUT			..delay100;
			break;

		= aninfSIC_REV_570T_SHADOW_EN);
			i= ASI
		    tp-v_id) == 		pci_read_config>link_confnot be powered down on		if (GET_ASIC_REV(t = ((tp->dev->denchip9ydev;
 ((tp->phy
	}

	ifnclude <l &tg3_mdio_reset;
	tp-	}
		brADVERTISED_jW_POlink_config.flowctattach	u32PHY
	}
	if (tp->>n

	case M BMSR_TG_IFDOWN, u32 of		}
eT apreserve o do no |
		   ||
			hy_regED_100tp, |ANEG_SC_PH
	if ;
 in thf_DEVexpec_APDhy(tp, MII_Uoid ATal)	t}
		loANEG_Socknua		bmDDR_0_Hritephyr */
	:
		mpltimes"},
	lclad(us)32(tK:
		_A old_t */
	_ABILstersw
	elsN3_5785_"	"3.p->so_loerOR_ID_B_>pdevrent_said) ==_FULL)) {
		u32 nvacce10);
	t_link_up =xVERTIS (st.h>
,MODE_g3_f
			     MI != Sdev->d
/* usec_wai0000;
				e !(tET_ASIt i;_CFG, val);
			->drv))
	break>state int i;
ould_wak|=l;

	INK_OK |
			GRANT ex =lay.MMICE(PTUPat_t_clockAX) {
INK_OK |
			mpa(PCI_ ni(structING_HW_AUT,(tp->tgTATUSOADED11 &&
	    g3_read;
		b00, staterve &X_CTRLys lutoS
	{ "tx_nPORT, 0NK_OK |
			tp->uct tg3 NK_OK |
			Wp, M_mask hy_is_low_power  Thes, MII_TG3_fine AN = tr0)
				ref(TG3PCI_		val |= 0xcted_trl |=
			_TG3_MISCsg_dig_ctrl |=
;

		p)	tp-ERTISE_10F!_1000X(lcladv, rmtae
		->cueEG_ADDR		porread(ASIC_RPEED_100 ||
	conf= tr32|N3_57},
		}
		breunimpeset(DVER_hw)f, u3dio_bVER_MASK) >st;
2 tgN_PLL13tl & ~PCI_4nCTL,
		  = tg3w3

	return 0;l = tr32(MAC_PHYCal_ctrl |
		AL_GPIOe;

		FG_NP)
				    tp->aninfo);
		if (stapi)
{
E_REAomdix(t (MAC_STA = (CLRL_RXC		bmenciexconfigE as D_MAP_GRC_MISC		val = 			retTUS_MB != 0)
	D_MAP_IDDQine ANEG_STATE_ABILN		0ev->lin(tp, MII_ ANEGATE_UNKNmp & BMS,ch =  ADVE		}
			}LE_AS expec| ne 20-2g3_pANEG_STLL:
		*spe_low_power NEG_STA	expect_link_up X;
			else
				phy &= ~MII_TG3mp & BMSk = 0
#define (TEST, &
ch (vup+ jollisio	retur_HPFFLecVERTISE_100->r32(NVRAounerr = tg3ec when ink_confude c01 tg3 y(40);

		 {
		tg3_ed_sg_dig_flo_LPA, &reat */
	/{
		tg3_bdes_co3 & TG3_0)
			serCTRL, grc, +link_config TG3_ASIC_002, 2003, 2USfig.speedMEOUT_57t -RL) & D= >link_confsconst cs_cfg = 0;
	expec
	OK;

) {
B_hc_irw != (tp-
		if (tfl(sg_		vaSK, ~ML_GHW_AUTO 0);s& (1x = ci i, 3_phy_->tg3_"owctrx"AC_MIt_linkne ANEG_S_chip_rev_graWRdonel |=
				S&GO_IS_	/* ReCAPD TG3 CHIPREV;
			hiif ATA)t.co (ap->r			if (sp->tg_cnt; i_cfg =E & TG3_F(taCE_ID		vat_link_up =E_UNKNOWN;40);

		SGtch_co rmt_adv)trl |=
				S&rawing_localII_TG3_				local_advWRPORT, _DIG_PARTN *tpufferDIG_PARTNLAatus & SG_DIGstERAS ADVEec when _ASYM;
	else
		mstatus & SG_DIG_PARTN(tp->tg3>mac_mode)Ix_col) 0x1erADVERTnfig.acphy(tp, reATUSlude <D_1000_status & SG_DIG_PARTN(tp->tg3local_advER_PAUSE_CAPABLE)
	readphy(tp, MIe_adv |= LPA_1000XPAUSE_ASYM;

			tg3_setup= 1;
		}
	0f06fVERTISE_1000XP;
		DIG_ISYTESGE_WAoq_fuesl_ctrl |*(NEG_COMPLETE)) {
match_round_chi 0 || WRadv & &phy94LITY_lse {
writephy(tpl);
		omiireg if (rmta_HALF ap->napi)
{
des_counterRTNE	   cal_advMB)
	LP_AD

				tp->serdes, 0x8011);j2 phyf (GE&phy9cLITY_eceiv) 2001, |= LPg3_flags & TG30)
		x ==fset, u=4et_drvd		    , tp->gr		if (_DIBUSY, 0x8011);YNCE MR_TOGGLE_TX |= LPA_1000XPAUSE_ASYM;UTONEG_COMPLETEESTART_reset);gs2 
		va 0;
		a_up = 1;
			tp->serdesDIounter = 0;
			tp->tg3_flags2 &= ~ 0;
	while  |= LPA_1000XPAUSE_ASYM	valhy |= ASYM_P		val = if (tp->l2 tmp;

	OMMON_Srl |=
				S!=AUTONEG_COMPLETE)) {
if (old_rx__100_ONLY) &&
	    (tp->pSIC_RELTE_NEXTgs2 & TG3_FBMSR,TIMEOUT_57de &ev_id) ==ATE_tw32_staountercfg | 0xc00-11,13,14_RX_hy(tp, MIce_shne TG3_Tink_config* Link par~MAC_MI_MODPLG2_PARALLEL_DETEtg3 *treguly(tp,GE_WA" },
	{ENODEV;
		FG, val); Sun Mi*duplex610_LEseEND_C_CTRL) & DTG3_DSP_AD2_%
			{ "tx_cole if (mar32(NVRAM_		ne_1000XPAUSreturak;

tephy(tp,  packe5PAUSE) {
		if (lstatus & SG_DIG 0xc010000;
					else
						val |= 0x40100	 REStePABLE)>mac_mode)rawing(NVRALEL_DE(tp->tg3_fla{
(tp->tg;

				/* Link parallel1;
		}
		statupdev, stat
 tg3_fiber_aneg_smacff Garzik;
			hi/* LequiCI_Di))
-EBUS3PCI (!(t{
		
static(gs;
#de old_rE_MII;t0000;
se
	0000;
E_SETT500)

/* These 				EEPROM_ADeld. */
statTUP);
		52"link test      (online) " },
	{ "regiace this ngoto out;
	  ne tp->link_configtg3_reatxf   (ADVERTIS2 phay(useev_id == _ENABLE)SYM;
nable- requi tg3p		grc_lg(tponcmd(tp,
				NVRAM_CMD_WREN |  Broadcom GO |g3.c:ethernet dDONE)))
r.
 *break;
		}
		if (!(tp->tg3_flags2 & TG3_FLG2_FLASH)) {r.
 /* We always do complete word writes to eeprom. */r.
 nvram_cmd |= (ethernet dFIRSTn3 ethernet dLAST), 20033nc.
 4(ret =  S. x.com)exec)
 * * t05-2009
 *(C) 01nc.
 2, 2}
	return ret;
}

/* offset and length are d003 Jalignedik@pstatic int)c.
 5-2009eff G_block(struct 
 *
* Coru32 propri *
 *	len, u8 *buf)
{
	ght rived
opyriavid 200Mille (davem@rAG_EEPROM_WRITE_PROTadcomCtw32_f(GRC_LOCAL_CTRL, utiogrc_local_ctrl &
		 	notic~lent CLt, p_GPIO_OUTPUT1Inc.
udelay(40

#il orpyri Dution of this firmwaredcomthertion. ChoratioC) 2000-2 southernet_using_ (jgarm.
 *Permissis hergranisheiselseparaCon id thmode disude <linux/kcroslrnet tp

#inpyriretion  :on iDethe d or g3_enable 2000-2accessude .h>
#inc
#include <linr firmwareedh5750_PLUS) &ton inot>
#include <linlude <linux/pdeciECTEDram <lin <li equyrifor n hex1, 0x406)nux/l
#includ
 *
r32ais aMODE

#include <linux/,>
#includ |  <linux/evice._WR_ENABL<lin#includioport <linux/ux/modulepah>
#incBUFFERED) |er.
 x/netdevice.h>
#include <linatoration.
#inclu#include <lin <linux/linubuffereos.h>
ude <lin
#in20 * Fx/, 20003, rnclude <lh>
#include <lininclude <lin cludprefetchre.h>

#inclu#includdma-mappinclude <cludethtooare.h>

#incluude <asmiim.h>
#include &pile aphe <linux/#include <asbrlude discmphy.niinclude <nclude <nux/kernelu <netlude <lux/if_.hution of thisux/modulepar	data ix/skbaux/emalncluequivl.h>
 formampanrovide <liis cope <l
>
#include <asm/ioodunux/in.h>
#infr.
 *porsubsys_tbl_ende < u16VLAN_TAGvendor,#define devid; <linuphy_0
#e};ine || deedavem#define _USED 1
define id_to_IG_SPA[] =
#el/ram.h>net dboardsz,
 *o{ PCI_VENDOR_ID_BROADCOMff.h1644, PHYRSIONCM5401 }, DULECM95700A6"tg3DRV_MO	"Se_VE_MODU	"3.102"
#d00012009"

#defin7ELDATE	"Septembe1A5nc.
 9"

#def"tg3avemDEF_MAC

#dE	0
22009"

#defi8002X_MODE		0
#defrT* FirmDEF_TX_MODE		0
#define TG3_DEF_3, 0DATE<linuF_MSG_DRV		 9\
	 NETIF_MSG_PROBE	| \
	 NETIF_MSG_5X_MODE		0
#defiRXTG3_DE	0_TX_MO| \
	 NETIF_MSG_PROBE	| \
	 NETIF_MSG_6MSG_IFUP		| \
	 NETIF_ timRX_ERR8\
	 NETIF_MSG_PROBE	| \
	 NETIF_MSG_7INKfore we decide theT1A7\
	 NETIF_MSG_PROBE	| \
	 NETIF_MSG_8MSG_IFUP		| \
	 NETIF_MSG_RX_ERn10, 2009"

#define TG3_DEF_MAC_MODE8MEOUT			(5 * HZ)from harddule minimuware NETIF_MSG_PROBE	| \
	 NETIF_MSG_9X_MODE		0
#defi3X_MODE		0
#def3Axould be called to hardw
#defi unpu8vid S. Mille (davem@rAG_JUMBO_CAPABL		60

#def3E	": MODE_MODE	09"

#define TG3_3AC_MODEe000an't change tR_RX_MODE	3C996T can't change the ring sizes, buf time before we ux/ede ou plBaceparathem_0	0the NIC on "
#d me4) shoulde haRINGSXce
 * them in the NIC onboard me7ory.
 */_TX_MODE		0
#200
 butSIZE		512
#define TG3_DEF_RX_RINGta payload */
#define TG3C940BR0ore w
#defDELL.
 * You can't change the rinentrG3_DEFdTX_MODE		0
#dege where yVIPEReightoine Ttp  Corporitself,
 1emefine TG3_DEFge where yJAGUAse constantarzikGCC sstanat ux/ifeem to be hard41RX_MODE	MERLOSIZE		512
#define TG3_Dks insteadaof with
#elhw multiply/SLIM_ux/ifo ins
#defCompaq.
 * You can't change the rinCOMPAQG3_DEF7cMSG_IFUP		| \
	 NETIF_MSANSHEE 1)'ne TG3_DEF_RX_JUMBO_RICB0
#de9 would be to
 David S. Mille & _rmwarTX_MODE		0
#MAX_ME) && \
	  !_d) should bo' wHANGELINGASS)) ? 1024 : 512)F_TX_MODE		0
#8e thee before we decide NC778metarymaximumined a siE) &&e we  !(eem to be hard c_DEF_RX_JUMBO_CL TG3
#defIBM.
 * You can't change the rinIBTG3_DE28uff. }E		5IBM???,
 *e "tg3.h"

inline* You can't change tNAM*lookup_by_defines.h>
poram/pr.
ntdefior i/uacizeo(i = 0; i < ARRAY_SIZE(E		desc * You canPF); i++#inclumphy.
				 TG3_RX_RCB_RIXi].X_MODE		0
#VL =RV_Mnoticutiopdev->definetem(structnclude <asmMODE		0
#T00
#de_BYTES	(nboaof(ED 0
  S. tx_ux/pre_descion.\g3.c: T& (Tce<linuxnux/in.&define NEXT_TX(N)		(((ompilenux/in.NULLYou can|| devoid _	0
#dnioratiogetux/type_hw_cfgne TG3_T)		(((tp) (s3_txval#end16 pmcsr/uac/* On some early chipsine TSRAMce
 inuxbe linux/ed_0	0D3hot MBO_e,
	 * masneeNG_Bke sudefin'reX_STD0.inim/
	pci_read_config_ souasm/p- 1)G_VLANpm_cap +"

#dPMCONFIG_&NAB)
, 20iptor &= ~ free TX de_STATE_MASK reqciare.h>
X_JMB_DMA_S/
#defiN_MTU	m nuG_TIMofwake up TX sciptors remsleep(
F_TX_MODMMB_MAP_SZregister_SZ(avems (indirectncluoMicrwise)_MODEwill funcm/procor u64lyA_TO2

/* n(ine NEXTWAKEUd souRESH(tnapi) : 51ODMISC_HOSTCONFIGSPARnoticeeutiomisc_host| You caYou cThe mlo et arbi->txhaarzikbde <asm/R_RX_order rporBO_RDETHTOOL__MODEto succeed.  Ned(Clly on powerup		TG3BYTE
/* ux/modisheUM_STWAKEX_RCB_R_SZit isn/_TX_R

#iftTG3_DE entitiesigonh as efine  netboot_MODEcude m<linuh>inedf itt tg3_etvalsm/systeMEMARBIG_SPARC
PARC
>
#includ a,EUP_ | arzik (jgarude <asm/uac		((tnclude=n't chanINVALIDd Jep->led| defh= LEDpup Tp);-rinRV_1binMODEAssume an
staMODE 0
#dMAetaryWOL cap"
#inby/tg3ault. g3_etNFIG_SPARC
#in|=if

#define BAR_0	0
#define |/moduleparWOL NIC.h>
tribGET_ASIC_REV/
#defci_
/* _rev0
#d ==/tg3.bi T_5906E((x)))) ?!(systePCIE_TRANSACTION_CFG) &"

#-1ECS	u#defiLOFIG_SPAR3SE("GPefinFIRMrzikf

#define BAR_0	0
#definep.h>e_param(tg3_de2MWARE(_debG2_IS_NICp.h>

nglude <linuxVCPU#defSHDWe <linux/ne D& ue");

	Copy_ASPM_DBNClinux/param(tg3_debitmapped WARci_tbWORKAROUNcrosdebugf_SZ(ED 0ce_id3_TX_pR_IDde <asmTX(N) * )
#TIGON3_mber)},
	{_MODDEMAGPKT<linux/PCI_DEVICE(PCI_VENDOR_ID_CE_IVICE(PID_Bgoto doneG_SPARCncluJUMBOmees <liNIC_BO_R_DATA_SIG, &va3.binrpor_SZ(==EVICE_ID_BROAMODULID_TIC#inclu3_tx_icMA_Tude dCE(Pinux/EVICE_IN("Bro, ver, cfg23_rxERSIO4BROADC_JUMBO_IG_SPARC	sizeo_DEVICE_ID_serdesIGON__SPARC
#ADCOM,2PCI_DEVICE_IE_ID_TIGCFLE_VCI_DEVIIG_SPAp->CI_Ds2009 BARCE(P =PCI_DEVIIC3_570_BROADCOM,2FE02FE)},
	{PCI_DEVERE_VEedefin	ver >>
	{PCI_DE,
	{PCIVER_SHIF( S. mphy.;F_TX_MODE		0
#RSS_MIN_NUM_MSIX_VE!S	2id tg3_c 700MODULE_VERSIULE_VERSION	"3.102,R_ID_BROADC,
	{PCI_DEVICE5M)},1)},
	{PCI_DEVICEDULE_VERSION	"3.102ADCOM, PCI_DEVICE_ID_TIGON3EVICE(PCI_VENDEVI _DEVII_DEVI<ard me<linux/{PCI_DEVICE(PCI_VENDOR_ID_BROA(PC_2, &
	{Pm/uaccmphyVENDOR_ID_BROADCOM, PCI_DEVICE_ID_CIGON3_5705M)}85ENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGOX)}4_DEVIC4DEVICE(PCI.CI_DEVI &DOR_ID_BROADCOM, PCPL")TYPss TG3ID_T3_tx_buM_2)},
	{PCI_DEVICE(PCI_VENFIBERlinux_VENDOR_ID_BROADCOM,E("GPR_ID_BROADCOM, PCI_DEVICE_ID_TIG09"

#VICE(PCI \
	  !_ID_TIGD_TIGON3N	{PCI0#incluoN	"3id1_BROADC, PCI_aFE)},
	{PCI_DEVI09"

#1_BROA#de{PCI_DEV_BRO02FE)},
	{PCI_DEVICEDULE_VERSION	2ADCOM, CE_ID_TIGON3NDOid  = (DCOM>> 16) << 10ADCOMR_ID_BROADCOM,CopyI_DE& 0xfcI_DE59016TIGON3_5789)},
	{PCI_DEVICE(PCI03ffD_BROCI_D		}g.h>
FE)},
	{PCI_DEVICE PCI_DEVI TG3{PCI_Ddc_DEVICE_ID_TIGON3Nf (_VENDOR_ID_BROADCCE_ID_Te <Z)

/garz<line#include <aspci80_ING_Slinuxinuxg, "T DRV3 bit.h>
ed debMII_SERDESCI_DEV_VERSIO_VENDOR_ID_BROADCOM, PCI_DEVPL")
	{PCI_DEVon.
 | deh>
#include <N3_5705F)},
	{P <linux/V_MODULE_VE =COM, P&(PCI_VD_BROADCOM, PCefinSE("G TG3vnux/itx_buSHASTA_EXT702FE)},
	{PCIVERSI72 PCI_DNDOR_ID_BRICE(A3_TIGON3_5789)},
	{PCI_02FE)},
	{PCIRSIONswitch (DULE_VE#incluENDOR_I:
		caseROADCOM, PCII_VENDOR_ID_BROAPL");5703systthe rce_iram(tg3LICENSE("GCE_ID
M * FirmwarVENDOR_ID_BROADCOM, PCI_DEVIOM, PCI_DEV2_ID_TIGON350_570GON3_5789)},
	{PCI_DEV2I_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TI51NDOR_IDne TVICE(PCI_VENDOR_ID_BROADCOM, PCI_ne TSION	ng eNDOR_I	"tiVENDORDEVI if 0 (MA721)},
 isMODE	*ON3_5d tgUMBO_olM, PDOR_/
#def ")\ram(.E_ID_T@pobo PCI_DEVICE_ID_TIGON3_5751M)},
	{PCI_D_TMODE	FIRD_TIGON3_570
rcmphy.linu
	{PCI_DEVICE(PCI_VDEVICE(PCI_VENDOR_ID_BRLE_VERSION	"3.1(PCI_VENDO750M)},
	{PCI_DEVICE(PCI_VENDOR_ODENDOR_ID_BROADCOMBROADCOM, PCI_DSHAREDEVICE(PCI_VENDOR_ID_BROADCOM, PCI_DI_DEVERSIOCE_ID_TIM, PCI_DEVICE_IOADCCHIP705MIDE(PCI_A0nclude	{PCI3_5751F)},
	{PCI_DEVICE(PCI_V4NDOR_ID_BR_BROADCOM, PCI_DEVICE_IDCI_DEVICE(PCI_VENDOICE(PCI_3_5750)},
	51M)},
	{PROADCPCI_VENDOR_ID_BROADCD_BROADCOM,_TIGON3_5751F)},
	{PCI_DEVICE(PCI_V2)BROADCOOR_IDGON3_5789)},
	{PCI_DEVICE(PCI_ENDOR_BOOADCOM, PCI_DEVICE_ID_TIGON3_5752)CE_ID_ID_TIGON3_5751F)},
	{PCI_DEVICE(PCI_VE_ID_TIGON3CI_V5DOR_ID_BROADCOM, PCI_DEVI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGO5BROADC)},
	{PCI_DEVICE_ID_TgICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIG4S)DOR_ID_Bnotic, PCI_DEVICE_ID_TIGO87FCI_VENDOR_ID_BROADCOM, PCI5M_702FE)},
IZE - 1))

#define Te NEXT51M)		(sis.  An/
#de sDCOM,1)},ICE(PCI_VENDOR_ID_BROADCOM, PCI_DE(PCI_VENDADCOID_BROADCOM, PCI_DEVICE_ID_TI5787)},
5784_AXE(PCI_VENDOR_ID_BROADCD_BROADCOM, PCI_ PCI_DEVICE_ID_DEVICE(PCI_VENDOR_ID_BRe BAR_0	P
ux/if__vlan.EVICEdebI_VENDOR_ID_E_SZ	TSO},
	{PCI_DSION	"3.RESH(tnapCOM, PCI_DEVICE_ID_TITIGON3_55714SDOR_ID_BROARIMAPCI_DEVDOR_IDIZE - 1)ENDING		(TG3_TXk@poPCI0x205a_TIGON3_575) * )},
	{PCI_DEVICE(PCI_VEPCI_DEVI63<linuxPCI_DEVICE(PCI_NDORint, 0},
	{PCI_DPARM_DESCEVICEENDOR_},
	{PCI_DEVICE(PCI_COM, PCI_DEVICE_ID_TIGON3_5906M)},_VENDOR_ID_BROADCOM, PCI_DEVuggIC omessa PCI_DEVICE_ID_TIGON3_5787M)},
	{PCASFCI_DEVICE_VENDOR_ID_BROADCOM, PCI_DEVICE_e <asEVICEVENDOR_ID_BRDCOM2I_VEID_TIGON3_5703X)_VENDOR_ID__VENDOR_ID_BROADCOM, PCI_DEVD_BRNEW_HANDSHAK},
	{ADCOM, PCI_ PCI_DEVICE_ID_TIGO64VICE(PCPram(tg3DDEVICE_I_BROADCOMDEVICE(PCI_VENDOR_ID_BROADCOCE(PCI_VEVENDOR_ID3DCOM, PCI_D3I_DEVICE_P_ID_TIGON3netdevice.h>
#include <linAN,
	{PCI_#include <as{PCI_DEVICE(PCI_VENDOR_ID_BREVICE_OR_VENDOEVICE_ID_T1S)}ID_BROADCOM, PID_BROADCOMIGON3_5721)},
	{PCI_DEVICif_vlaID_BROA_BROADCOM, P{PCI_DEVICE(PCI_VENDOR_ID_BR	{PCI_DEVICICE_TIGON3_5761S)}ICE(PCI_VENDOR_ID_BROADCCERSION	51M)},
	1_TIGO7 PCI_DE_BROADCOM780ADCOM, PCI_DEVNIC CITIVPCI_UPMODEPCI_D/* TIGON3_signal pre-emphasis_RX_napi)->tx0x590 prieby,
	{PC/*ENDOR_ID_},
	bit 18 ".cCI_VD_BROVENDOR_ID_BROTG3PC8DCOM, PC_TIGON3_576 PCI_DEVICE_IVCE_ID__PREEMPHASISID_TIGON3_VICE(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGOADC4#include <D_BROAVICE(PCI_VENDOR_ID_BROADCOM, PN3_5787)},
1M)},
	{P_VENDOR_ID_D_BROADCCE_ID_TIGON3_5722,_APD_ENCE_TIGON3_5785_G)},CE(PCI_VENDOR_PL")ID_BROADCDI_VENDOR_ID_BRODEVICM, PCI_DE_BROADCG3_DXPRESS_DEVICE_ID_cfg3N3_572I_DEVICE(PCI_VENDOR_ID_BROADCOM, PC3VENDORDEVICE(SION	"33 PCI_DEVICE_BROADlEBOUNR_ID_BROADCOM, POR_ID_VICE(PCI_VENDOR_ID_BROADCOM, PC,
	{PCI_DEVIC)VICE_ID_ALTRR_ID_IT(N) ND_DISONNECTALTIMA, PCI_DEVICE_ID_ALTIMA_A	{PCICE(PCe_id tg3_c cERSION	"IMA, ABLE(pci,,
	{PCciVICEe_id RXROADnsto GCC so{
	 withthemro GCing[ETH_GS] =	{ "{ "rx_/syste_	Cops_keys[ID_T_DEVroce_ucast_paTketsoctets" ICE_Icketsfragments" },
	{ "rx_ucast_palign_1)},
I_DE:
	PCI_VE_9046_wakeup(&IZE - 1))
napi)		((VENDOR_ID_ALTIMA, PCI_DE5785_;x_xon_pausercvd" },e <asm/
	{ "rx_xoff"rx_D_TIGOPCI_VENDOR_ID_ALTIMA, PCI_DE5#include <JUMBO_RJMfor _SZ		se_rF_TX_issue_otp_commandCB_RING_BYTES(tsion iES(t) +  + 1) sE(PCI_VE_B J003 BOTPCONFIG_inclu tegn_eck_	{ "rMDprocR 

#isspacket"rx_6ets" .
MODULEWai\n";

mento 1 ms_MODEerrors"	"ti xecutefine Drpor S. rING_SIZ10ING_ * \g3_dlude <linux },
roceUSco GCC soTIGON3},
	{ "rx_|| defde <
#elF/module},
	{PCI1defi8021Q

#defi1522ts" },
	{ "rx_12},
	{ " ? 0 : -EBUSYdefinom RVICE vergphy X_WAKEura(N) +096_91_ocOTPN3_57prom.8192_
/* ./tg3.b },

;

MO95_octet_pacyteo 32-ID_Avalun tha_THRraddlGarzh
	{ ignm_NAMboundary.packWelinutwolis4S)}"TOOL_ION);then shifiION);merg
	{ (tnasultsICE_ *	E_ID_TVICEts" },
	{ "rx_ROADCgth_phct tg3_ethtoox)		((x) +ndersbhalfoctet	{ "sive_cr_lekets" },
typezi_late_co5#defTHRU_GRC8_to_VICE_EVICE_ unpub_x_256_t _col	{ "rx_6},
	{127_INIOADCOMnux/in.VENDOkets" },
ADDD_ALimede_35ry.
s_TIGO)AW_IP_2"tx_cctet_packtx3timlide_3" },
	{ "tx_collide_READ_4" },
	{ "tx_collcontrol 4095_octet_p8tim703X)ESCRIPide_7time "tx_c	{ "tx_collide_7time80)},collide_11times" },
	7"tx_collide_11times" },
	x_co_collide_11times"#incons" _collide_11times" },
	10titet_pack(7time9	{ "N3_57000ff3_5755M)N3_| (_collide_1_BROADCunder" },o_4095_octet_packrx
	{PCprobex_deferred" },
	{ "tx_exhwVENDOR__1,	{ "tx_collpci_rdcollide_11tcollide_},

maske_ID_PCI_DrF_TX_CE(PCI_VENDOR_ID_VICEID_ALTIMAUSVENDOLIBs" },
	{ "tier_sensse_rlude <You co_81ingollisPHY IDN3_5761S)}ca + 1nflictuld b ASFg3.bin"vef CdaSZ(TG3_ts"
	{ "d	{ne Tll" t tg3_eterr	{ "txID_BROADCOM, TG3PCI_DEVICE_TIGO_DEVICE_IDude <arl_rcvd1)rxbds_empty" },
	{ "rxID_BROADCO
#inclu{ "tx_col =lctet_packdma_wriAME	":ID_BROA MicrosVENDOR_ID_BtatiowDEVICE" },physicaln't charier_8192_[] __6_toverifyOR_I*x_collMEICE_sane.  If "nvdoes * t_ID_ good,finefall backSTucasto eine TG" },e_fu-ram(d tCE(PCIa_DEVI}tructTG3_failingonline)
	{ " },mp_quefors"_RX_e)" }/typelishaOR_IOADCOM },
|linux/ID_T
#ins" }_pciPHYSID1, &{ "tx_colliI_DEVInull"upt test (off			 )
	{ "};

N3_5" },
	{ "d	m/uaccc_RINthres_BROA "tx_colli PCI_Dtx_malignID_BRp->regs + E_ID_{
	writel(_VENDOR_ID_BROADCOM,s" },
 *tp, 
 *	ofanted :
 *	DTIGON3_5703X)regs +tatic) u32 	Copyr->regs + &n't chan TG3_DEclude <a>e32(&& KNOWNERSION	
static voi};

st
#incluCT, PCI_DEVIC;
}

statERSION	"s + off));
}

stattic const	 #def(PCI_VENDOR_ID_SYSKONNECT, PCI_DE)},
	{PCI_DEVID_TIGON3netdevice.h>
#inOM, PCI_D_reg32m Corport{ "t_ID_ALTIOR_ID_BROIGON3_575c const structCE_ID_TpyrDo PCI_ing,
#de" }
alID_Ty, PCIup inE_ID_TI3_DEF_RX_JUMBO_R4_orTO)NDOR_ID_BROVENDOR_ID_BRersize_ the _TX(NNDOR_I;
}

ents" }st  *tp,	{ "tture?  Try "umber ot tg3E_ID_TIdefinefine TG3rqrestDOR_ID_BROA_colUMBO_CAPABLE) && ude <li>aperep_TIGONnux/in.-ENODEVspin_lo_TIGON3_5704ve(&avidiff
}

stva tg3 *;oid TIGON3_575}

static u_578 tg3 *ted fr	Copyr3_57780)},
	{PCI_DEVICE(PCI_VEND_TIGON3_5755nux/if_OADCOM, TG3PCI_DEPCI_VENDOR_ID_SY61SCI_V_ID_ALde <asnetN3_570OADC{ "nic_aB_DMed_irq,
	{ "H_GSeff GRX_JMB_Dd003 (t
	{ "rlinu	Copus_updatctet_p_ID_Abmsr, adved l,Ee_5t PCI, };

I_DEVICE_ID_T*tp, u32 offBMS_TIGE(PCROADCOM, !r(tp->regATS u64his k, MilleaticI_VENDOR_IDE(PC & 3_wr_Lrrier_1u32 oIGON3skiICE_I_res721)},
_dworderrors"  codelude <linux/err)		(de <l		6q_full"	&vaING_VENDADVERTISE_10HALF | Wm)
 
	P_SZ(FULLICE(PCENDOnfig_dword(DR, off);
	pci_read_DaviddescENDOR_ID_RCV_RECSMAff);
	pci_reaPA_dis_enter
	{ "tI_DEVICID_BR_lock_irqsave(&tp->linux/if_vla10RELD_ONLY/)},
	{PC(MAILBOX_R(_pcirmwa},
	{ADVRELD0DR, off(PCI_VEND   TG3_64BIT_RESTD_DATavidos + off);D_BROADCOM, PCI_DEVIte_flush_reVICE_01ID_A32 },
	{G3_Rindn;
	}

	sp},
	ockI_REGal(tp->regndBTIGON3_5ig_dwordpci_   TG3_64BIT_Rde_7STERTIGON3_5NECT_   TG3_64BIT__DEVICE_Iaticpci

#inon.
 };

G_LOonfig_dwoD_um an)T_Halfff);
	pci_reaestol;
}
FullO_BROX void tg3_wrilags);

static void tg3_wriite_ind
	/* In TATS u64'nux/swh_macdisabtest, PC(strucsnk te also needs:
 *	Deval;
}copper_is_advertiude <allfline};

stoff);
	pci003 B*tp, u32 offT_REG_LOWNDOR_ING_m/uaccTDadecIn in "tx_ex64BIT_REin_unlock_irqADDR, BIT_REGEG_unloc&&g3_r  (vaG3_64BITwrite_unloDR, offRELD;
	}
sabld this cBMCRo_longags)i },
	AN_DEVIC | voidmboREs" },
	{ ppingerrors" rame_irespt/checm/u>
#inc.     tp->grc_localECS	0x1nfig_dword(wrff);
	pci_read_c    TG3_64BIT_REUM_TE format, prg3.c);
	}
}

static u32opyrig|lent CLt, p_CL}

slnted u (off =:ors"d_prod_ff);
rd(tp-G3_64l(vaID_TIou can change ID_BROwrite_o nese_rcoper->pddsp=, PCdword(tCVRET_CONn in_0 "tx_}

stapetruct tg3TG3PE(PC) {
		pciRC_LCLCTRLnfig_d, flags);
p->tatic void tg3_write_indier.
	 */
	if  <linuI_DEVICE_ID_TIGOatic void tg3_write_indi Corporlinkts)/siz.;
}

/* INT TG3POtain registers32(structCI_Di, PCI_ID_TIGtic c, valDEVIneed
	 * tTRLx_colPCI_AutonegLOCKat, p frequencFIBR,
	{Phout some delay.
ndifF_TX_MODct_lock, fla exa2002ADCOthe ing slishetoggl&ENDOR~ RL is another exampleCI_C changefine TGRC is co yrighumber oftx_colliu + TGndersize_pKEUP_THR_mult_collisionspartnox_deferred" },
	{ "txn	{ "edagmentvpID_BRO[256];e deciin little-t tganinimmaROADCOting tel },
	es" },
	magicTG3TSO5));
	pci_write_config_dword(NOevice.hctet_packeC) 2000-2ID_Tfline0x0, &	cludI_REGIGON3out_not_"loopTG3TSO5) postn rermwae BAR_0llideICE_IDts" },
	{ "rx_51256NG_S+= 4_DEVICE_ID_tm to certaiinimVENDlide
		 S. G3_6432 * ttatiM, Ptruct te_flus Uscollisbigush(struEVICEroutinnt" ikp codrvERSIOcast_R_IDteMODULEeas (onexist_mail
}

stati, u_TIGON3_5in after the N_be32ea(size10fiesi, &tmpI_REG_Bthodrzikguarante
	{ "t		memcpy(&eed
Postei]M, PCI,onboaof(PCI_" },ck, frite_confignt	eed
ca to ce off);
eadlci_finff);
abilitp, uH(tnapi)EVICCAP wheVPDCorporausec_wait)
	 postayf (tp->tg3_t tg3 *tp, j* use_ST	__lePCI_IGON3_16	Cop{ "txd lodefine NEXT/sizeP{ "tESH(tnapi)	B_DMA__>tx_peVPDx_col_read_I_VENMWiRDER) whiis fj++_512_tID_BROAthtooROUND))
		/* Non-ESH(tnapi)R_fla, fl51M)},
	 PCI_DEVID_MBOX_H, PCIbaligff)) _rx_;
} ,
	{ 800EVICE(PCFirmware0	D_BROADC_6ti	(tp->p_config_sh(s *tpxc intD)_flahere, flval, tptruct tg) ETHTOOLD))
		/*of(u64	{PCI_DEVI->regs + l(, tp);
03X)Copyri
 *	tg3 PCI_off);v_TIGpu_ID_al, BASEsoff);t tg3 *tp,  B_DMAbox_fv(valg3_rbv flags;

3egs + _dwoparsMODU);*mbo_GSTRIart(tnapi)" },
	{ "tx_carrier_51254;;
}

sPC
		if (ueig_dw firmh(tp, (reg)o_2043, 2004 (tp-},
	{Peruct tg)},
	{PCI_D0x82 ||lush(s GRC9static 	,
	{(i + 3 tg3  RCMBOite32, (reg + 1]g, &vaoff +->flush(structr2]_TIG8) Corpo"rx_tinuVEND1E)},
	{PVush(_DEVx9TIGON3f + GRCMBOX_BASE);
}

#e trval,ae  equ();
	val)
#defne tw32_f(reg,eg,, &val You can equivaval), (us)_ equivl#defiPCI_RE703Xflush * t(>clud32->tg3g)

static,us)oid tg3l, tp->x/pr + off));
}- 2
#incluoI_DE
 tw32_f(reg,0]ruct'P',
	{PCI_DEVefine tr32(reg)	defiN' + off))RDER/ off 
	{ l))
#defASE);al),  S. MiG3_64Bmemreg), (r&& *tp, < N> 24al, te_config_dw+ i)E_REORDER) off + GRCMBOX_BASE);
}

#dl tp-flu32(s_MODU*&& (_SE);
}k, flags)ir  sh(tp, (reg), n_lock_irqDR, offnts"Son/t
	{ "r D;
	spin_uoff);
rd(tp		ret +->grc_lock_i>=ER_D),(&tp)ic vRAM_TX_B, off) ntee fine Dbtp->read32_mbox(tp, rx_mead32_mbox(tpBIT_REG_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5704Sint _pcitrMDULEait FIGct_lock, flait "	0
#d906"(tp,.h>
#E_ID_TIGON3_5715S)},
	{PCI_DEVICE(PCECT, PCI_DEVICE_w32_ude <l "rx_frame_toN3_5761GON3_5NALTIMA, PCI_D TG3		fu32 (tp->EM_WIN_BASspin_R
}

#d, 20twlock_qrestore(&tp->iwhen w
		pci
, u32A (C) 2leavn thyteos zerozik@ags);quivaqrestore(&tp->indirect_locDEVI003,o certain 6egisters
 * whereatic void tg3_write_ind}
f (tp(tp, (reg), , flamemg flags;

ff));
}

stati
}

st* &val);
>pdev, T ead_ect_lock, 2004(;

#define Te_confci_AP_Sid tg3w) == ASIC_REV_5906) &&
	    (off >= N9C_SRAM_STATS_BLK) && (off < NIC_SRAM_TX_BUFFER_DESC)) {
		*val = 0;
		return;
	}

	spin_lock_irqsave(&tp->indirect_lo88v_id) == ASIC_REV_5906) &&
	    (off >= N
8qrestore(write_indi2004& TG3_FLG2_5780_ID_TnonisioP_packets" },
	{ "tx_carrier_fw_img==e(&tpi" },
	{ "rx_to g unpubPermis	{ "tx_ex4_or_llide_7tim>tg3_flagsG3_FL#includeROADCctet_packnTIGON3te_flu32 *Iimes0x0 u32 *v _oct aga(tp->tg3_flagsG3_FLPermise+ BROADCuct tg3 *tpread32(ts" },
	{ "tx_colnux/in.1M, PCI_s + of/* Non-poid RINGtp->s *bc_verx_deferred" },
	{ "tx_exDMAd tg3_readstart80)},REG_p3_64Bprio	{ "bool new(reg=tg3 s/slab3_writvid S. MilleG3_FLG2c
	{P>indirf(TG3PCI_MEM_WIN_BASE_ADDR, orCE_I_5750es" },
	{ ", 200);
	}
)},
	{PCI_DEVIgLEN]_addr_ASIC_REV(t_collide_7timr32long flags;

	ifE(PCstruc_DRIVERD_MBOX_e this as zff, u32 *val2(regunsigned tg3_debugted 32_fict tg*/
	nteriv p->pci_chipET_CON_IDX_PCI_DEVICASE);
}r_REG_
	uct tg3tr

st)spin_locE_LOCKE_APE + of:
 *	De0ck, fID_TIGO wainum)8TIGON3 * tanysGON3_5APEv, TK_GCopyrightf = 4 * lumunlo_rx_ -3_5750 */
		pci_write_conf1& TG3_FLAG_TXD_M_r (daox(tp, 8; i++)
		tg3_ape_r (davem@rf = 4 * lOM, veg32(stite32(tp, T__RX__MEM_WIfwgs;
_SIZE {
al_debug32(tp, regrxspin_lock__fla	uj#defmino(tp->preturn -EINVAL;
	}

	ofrmwaNVM_PTSRAMBMODULE_DEV, APE_Lape&
	    (off >Ttp, Ttp, R_DESte32(tctet_pspintus !_MAJMain >ait _est
{
	unsi_GRANTS80)},
NT_DRl))
hetp->k#defu, TG3_APE_LOCK_INM_TIGONsnprintf_wait)_write[0], 32, "v%d.%02d"fiesK_GRAff,
		pci_r

static void tg3_ape_lock_init(sthwsbIC_SRAM_TXnted rn 0;o need
MMB_MAtp, Tlock(ststat/off);
nativR, oflush(s
		pcnw32_on tp->E_LO 2002, 20clude <1_REV(tp
	iHWSB03X)	Cop;
	
 *	d(tp->	spin* Revoke tleave (tp, MEMICE(_GRCRANT_DRIVER) { 4 *_LOCKnoff,_LOCK_GANT + fftg3.c:
	ck_i=tp, TG3_APE_LOCK_GRt (C)RIVER) {
		/write32(tp, TRINT + o
USY
	pci_w:
 *	Derived fr	Copyrsb p, (reg), 2(tptain rm Corp

static void tg3_ape_lock_init(st004 David S. Mille3 &sion iERSI{ "tx_ex#include   (tp->misc64BIiler so
 *	Derived f = 's'rosyst	DerivedTG3_D'b001ct tg3 *tp, 2B_DMA\0' TG3_FLAGCK_GRANT_De BAR_0SB_FORMATertain !itory.
 is me_now3_rx_
_TIGOite32(tp,  this asgs3 & TG	defacoal_noREVIShe retain {M_WIN_D;
	}
++)
	G3		"tigopyri906(sCopyright     (tp->miscw1R0_EDH_OFOADCOFirmwareEST		at, ps */
_IGON3NTuct 
80)},tg3_rx_buffesablirq_cnt; i2OCK_G	e examplVICE(api *REORD = where 24)[i], 203gned_WRISRAM__REORDREV_3 *tpx,3tnapi->last_tag << 2TIGON3_5703G3_64BIT_ without eturn 0;

	switch (lockn);

		default:
			retg3_flts(structock(sc_now;
	wmCK_GBLcertain sh(struct
	un2004 David S. SH32(strr;
	pci_wK_GRANT_DG_TAGGED_STATUMAJto be hard coded iTAGGED" },
USD_S->grc_lo, APE_Lflagshwrd(tp->->d(tp-> & SIrighter soefine, APE> 99al,  ef Ci> ed lonord(tp->pde{
	int i;

	tw32(TG3ON3_50, "*tnapi llisCTRL_MASK_PCI_IN 4 * lDE,napi-Lt:
			G3PCI_MISC_8B_DMAa' +Od int twork_worom Corpor9< NIC_SRAMC_SRAM_TX_Bh (l = 4 * lflags3 &	swit
mgmtble_inp->tg3_flags3 & TG3_FLG3_ENABP_SZAPE__5750aultle l, vle(tp, t_tagmailbox_f(tnaK_MEDIRpacket;&
	    901)0; <G3_FLG2_5780
PCI_VEd coded iUSE+TG3_FLG2_5780ENT))

#t:
			return -EINVAL;
	}

	off = 4 02, 20E(FIRus =L,
	    3_flags3 >>G3_FLG2_5780ERSIOID_ALable_or RX/TX2003k toASFINIe <li523_to_2 i 4 * lCopyrighTG3_FLG2_5780
ENect_ult:
			ret004 x/netdevice.h>
#include <linGON3ID_ALTIMA,_5750* usx0c in00et_sore(&tp->rn -EINVAL;
	}

	off = 4 *-rite_APE))
		returwork(streturn -EINVAL;
	}

	off = 4 * lapi-able_ints(struct
	if C_REV(t )	tp->wrtruct tg3_h long flags;

	if ((GET_ASIC_REV(tp->3_APE
	}

	/* Force ancoded iPOL[0].CK_REQ_DR
	 eve =_fralen_disable_int10timTIO
static eve++B_DMA,g3_enable_ints(SI)
			tp;
 RAM_Tts" },
	{ "rx_514o_1023packeecon>readacqirE_LOCK
{
	unuct tg3_rx_buffeG_USnapi->laatd tg3_disable_ints+=ANT + off)structC:
	g_dworrmwaPCI_REZEeena>read32(tD_BROAG3PCI_MI_SRAM_TXts =nap),e(&tpellZ		TG3AP_S wabovVICE_ID_ALTIM_mem(stridx[0]we'vne w2002, d.lisi/
	NT + off);
		ilagsePE_Lligntag w	if ((GET_ASg3_hw_status *sblk = tnapidash(!(tp->tg3_flags3 & TG3_R_DESeventte32(ap
	{ " !leted.i->rx_rcb_ptr)
    TG3_64BIT_REGEGindirek pt tgng2_flush(struct ;
	pci_write_config_dworlescegs le |
pi
	ifah (ltg3ap Mic rdavem@rrmwa761ESEG_VENDMA_ignelush(tp,_BRO(tp->tg3_fet_worki].EORDEbox_flush(tp, (reg), truc <linuxg flags;

FW	{ "rx__TX_le.h>
lush(tp,&_now;	tw32			 llidenow);
_SIZE(		api[i].napi)->tg3_flags2; i++)
		napi_enabVER));
fineG3_AP << 24);
		if s *sbw32_flu}

staST		C TG3_D_ENhas_wor2004 David S. Milletp->DASH*tnapiSRAM_TX"_rea& TG3_izeoopm Corpok(tnK_GRANT_DRIVERdim/pra*/
	tif)
#dwSFToid tOTE: unb();queues is only
	rite32(tpng32 *t   cah>
#collap't anyate masturn; to
	 * have REVre as = 0>reaefineh2 ofREVe tx slots (such as after tg3_initBLDte32_INT));

l | UM_TEST		napi = &tp->naprd(tp->;
	} 0;
		reh (ldx[0_G_MBOTG3_ },
	{ "rxs" },
	{ "nic_avoideREV(tp-/* asruct tg3 *tp 0x0lock_c1uct tg3 *tp, p, (reg), p whble offsf < NIC_SRAM_TE, tp->c<< 24structirq_cnt; Copstruct;

	;
	}

	/* Force an004 ASE);
}_sync = 0;
>irq_cTIGON3_5755Mst_tag ude <lore(&tp->now);
}

static inllideorporB_DM	return -EIunlocloc_FWkpyrigh=lush(004 DavbuffERSION	at, pstrucoPCI_CLOCTRL_FORCEOENABHpi[0; (CLOCTRL_FOR&= (_HWBUHapi =FORCE_CLKAGGED_STAT_HWBUG) REG_DAidx)ble(struct tg3 *tp)
{
	onfig_dword(tp->pdev, T
}

stati>pdev_REG_BASElwaysdev, TG3PCI_REG_BASE	p->esc) transkpleted.i->ctrl | GRnfig_;G3_TX compleif (!(tp->tg3_frl;
	0p->napi[0].hB_RING_Bgs dev *tp->indirect_loc_maipeS_UPDATED;
	tg3fineTG3PCI_MEM_WIN_BASE_ADDR, ineinvariantine TG3_T(x)		((x) + yright g3_flags2L_44M"rx_xless_oc_reODULEPCI_DsetsXn't chan9"

#dks(struct gs;
	u32 valMDoid tCLK)ALTCLK),SIC_REV(tp-|k_ctr_FE_G#inc},
	) }_reaCI_REG_BASE_A	tp->pci_cl
	}
	t_ALTCLK)LTCLK),
G_BASE_A4_REV(tp-tw8131_BRIDGEARGET_HWBUG)  pro	tp->pci_c,5000

VI_mem(s		tp-X_BAS	5000

stVIA_8385_0ARGET_HWBGET_}tg3_fla	3		" PCIING_ct tg3 *ciurn (#ing toe <as_loctROADCR, off);
T_CON_, PCmy stale  + TG3_/* ForcPCI_VENMW003 B }

slidaMillffg3_wCLOCKu32 NAME oe <a++)
		(DRV_VENDORX

/* n	wmbervee) "le iux a.
 *karu32 oORCE0_ALIG},
}ect_llidemem(eu32 ofT_ASIC_RMA_RWamplef"loopt" TSO5_VENma, PCest_kacheICE_IDizk_iroff)SIZE)how.drMIN_E
#inadh thicketASE_ADDMI_am(tx/ins MWIREV( 	udelvackemRGEThe reAPEuDCOM, 5	i_BROThnst semO		   uggesore( u32 ofBASE_ADDMI_COMinsufficienn of thisefine tw32_mailbP_THRESH(tnapi)spin_OMMANICE_IDG3_D,efinq_val &>readvd" in_locft structAT	{ "be hard coded iu32 fn hexaREORn_unloc
			udrae_&tp-& MDOR_IDdnvramabsoluteD_MArit(str;_LOOPADDR, 0UM_TEST		{ "rxops -SO		"x_sinpi)->tx},
	(str'}

sta_CPMoopsI_D_ID_A/
#d"

#dewe tS u6o "tx_colany },
	{ "MMIOTAs */
;
k_ircoal_nolson"octeff =opsble(0 off, CI-X hw	_mbox_5delayitux_collideux/eite_i *de-= 1A(80);
a++)
llt tg3_ethtooe tw32_mailbox(reg, val)	tp- {
		*val = frame_valoff);
AC_MI&n_locMASK_P_mtruct tg3N;
	pci_write_congs);lags3 & TG3sh(struine PHYal = frame_valPCI_
SRAM_do */, (reg)_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5704
	ifcRODEV(tREGICE_ID_ALTEG_Df));asicEVICERSION	"3.102flags;

	if ((GET_ASIC_REV(tp->pci_chip17
	{PCI_VENDC_Me_val _AUTO3_inLuct case TG3_8PE_LOCK_MfraSoops -VICEe_conf;
	frdrif ((tp-ps = Pect_lPCI_DT)ht
 *8m    MI_COM_PHY_ADDR_MASK);
	frame_val |= ((reg << MI_COCI_DEV*val =32 fraM_REG_PDATED;
	tg3_val)
#defineADDR, 0GEN64BITi_moDSICll_qtp->read32(tple(tp->gs3 & TG3I_DEVICE_ID_T|= (val & MI_COM_DATA_MASK);
	f3_hw_opsn hex].hw_rame_STARTstrucigned lne TG	udel,& MIlast_tagIS_FE<< M->grc_lo2_f(MAC_MI_COM, odite_M_TXWrad_icast_IDf));,
	{ A0.m(struram(t TG3b(tnamo
#insionr Corpas A0lide(C) in= 1;_u	(N) t tg3_etOM_P_ctrl &=pci_write_config_dword(tp->p52_A0		      val 	if ((frame_val &lags3 & TG3);
}& M1)
		retADDR_ADDR_ICE(/03 A1's *A2&
		cert_MEMICH

/* netstp-> 0wl, mRX->napR(_FIRM(tp->tgAnd	udekets" }. cyclOM, fraonlddr << MI_ !=llal = tr3G3_Doff)M_PHY_AD
/* nde);
	an;
		takeple deram(tet =speci600)ontrotool_test Corp(TG3PCI_MISCops ~ OK, reset we time, cae <astx_mbupble_DATAinftnapi)->txE
}

sAC_MIspace. Ople :
 *	DerivedbridgOM, frats" }DR_MAearsncluMODE_me 
};
rnon-val)(regw3dur_hit" 03, looddr_col_rcve inich_RESEODE, 
};
ackt tg_BRO'se timeltw32_frm in tint M ODE, taYunloliugops int i;
imit- neeowywe tiK) &&phy(f ((phy;II_ tg3mit--) {
		err. Howe TG,e (lo int x0;se0) {
	mi->mi_itdaknownrsign}

sise TG3_APE_LOCK	OM, frafuct  int<ci_cops != 0 Cor CorpS)},
t re--fig_dwerr d_64BI cross000

c int tn-EBUurn -EINVif ((tpMI_Cret;
}

stati,
	{ "avem@is&
		 cleaecoreG_CPMMI_bus },
	{ se {
		tw32_ft{
	i780_oopsps != 0)
		ret = 0;

	if ((tp->mi_modi3" },nt_mb)},
3_mdiL_CLEARm Corpormiilock *bde |
		 tg3e(&tp->isc) * \
	if ((	spihpin_loc_HOST_CTstructID_BROADC	{ "rxox_flushp->ind)	} ich32 va_TARGET_HWBUG_REG_DATA,)		(((N)NTEL_GRC:
S	5000

stps !=_82801AA_8if ((tpCLOCK576IDRGET_H_bhjiffietp->struc;

	tw32(TG3PCI_MISC_;
}

sg, u1Bod(strt
	struct tg3 *tp = c void tg3_;

	u32 orig_clock_ctrlu16 vX_JMB_D57BA_11if ((tp0xaval &M_DATA24_to_15hyON3_570 *phydevunlorv->phRCE_ASK_16 v6truct tg3 *tp)
{
	u32 val;GET_HerivENDOR_IDp->p)
{
	re*k;
	resho&= -EIOck, fpi0tp->iTRL_ALTCLK),
	HZc int t=
	if ((turn;
	}

FG2_50->E_ID_TI PCI_DEVICED_BCMAC13k;
	 intY_LOOPPHYCHYCF;
	c1_Ly_cops211CY_LOOP MI_COM_GRC:
	, val);
		a_LEDethtSRAM_TX_BUid++rl &= 3 *tp, 			b), (ul);
	s= ne TPHrevATS utruct tg3" },
	_BLcode int 01E_Liswrit> _RTL8201MAC_tic inEICE(&tp->RTL8201TL821tICE(retsubordinMI_CCE_ID_TIGONDE_RGMIIfig_dweg),(->ct tg3 VENDOR_ID_B_rcvd0EVICbus2al = PHude <linu784)},
	{PCI_DEVICE(PCI_VENDOCH,
	{PCI_DEVICE() {
oo)
{
put_mem(st, (reg)C_SRAM_TX_pingi_write_conM, PCI_DEVICE_ID_TIGOON3_5714)},
	{PCI_DEVICE(PCI_pi->E "._now;
_write_confi)
{
	return 0;
}
		pci reg, valhyr32(reg)}return _MODES;
		ertain re)
{
	return 0;
}

static void tg3_mdio_confPXH_shouRTL8211),(vss */
s */
#|writile (loTERFACE_MO_F
		 RX_M3_val PCI_CM50610e != PHY_INTERFACE_MO_2_ACT_keys + of&tp-|=  2002, 2PCI_x; i++AC_PHYCFGC:
		_MASK_MASK   |
		     C:
		va;
	caDESOM_PK_MASK |
		       MAC_PHRTL8211te aQUAL__ID_ALSK   |
		     
	val &=CFG2, val);

	val = tr32(MAC_PHYCFG1);
	val01ce != PHY_INTERFACE_MO val);

		val = tr3E_MASK_MASK_mem(struct PHY_IBMCR_RTERFAC<FG_enaags3 & to N);
	if (!(1al););
	if (!(tp->tg3_flags3 & TG3_FLSND_STAT_EN)>_STD_IBND_DISABLE)) {
		if (tp->II_ITPHYCF	DE_MASK_MACE(PCI_VENDOR_REV_5 MI_BUGG1_TXCLK tg3_eSKD_IBND_DI|_INTERFACE_M1X_RINK_TIdataint voiEPBreturn 80);idLED_14,X_RI5, TG3_I_REi_writ_Msuppor\de);
DMA = bp->pri > 404S)}TG3_FLG_INTERFmayMAC_);ine TGaddne TGal Corp57xxfine TGs beh3_64"nvrNDOR_ID4-D_STollis,
	{ "_wait)exCOM_2(reg <Anyrontrtp->indntee bstrucvaid S.DE_MASts" }f(MA3_bmcr_heRFACE_MAT_EN;
		e_val |= ((reg  int tgPCI_DEVICE(PCI_VENDOR_ID_SYSKONNECT, PCI_DEVICE_IO;

	spin_PCI_DEVICE(PCI_VENDOR_ID_SYSKONNECT, PCI_DEVICE_1AG_TXD_ENDOR_ID_SYSKONNECT, PCI_DE3_57780)},1f); TG3ADCOM, PCI_DEVICAPPLE_40BITRX_DEEXT_IBNROADCsi		reaiomem writxdev->drtruct tg3 f (!spin_unll, MSI  tgile.h>
#incl |
		       MAC_PHYCFG2_INBAND_ENAdol);

	val = tr32(MAC_PHYCFG1);ck_ctrl, 40);
SERVER2(MASFG1_RGMI32(TG3PCI_MISC
	err  harN_EPB | MAC_PHYCFG1_TXCLK_TO_ags3 & &&lags3 &I_SN(GRC_L_EN */
		pc));
	for (i = 0; i avem@re3T_RX_DEBIT_IB

statPHYCFGapi->		pci_write_confiGMII_MODE_TX_LOWPWR3_FLG3_D & TG3;

	i= MAC_PHYCFG1_RGMII__RGMII_MOFIRMWC_IBN		pci_write_cMODE_TX_RESET;
	}
	tw32(MAC_EXD_TX_EN)
			val |= MAC_PHYCFG1_RGMII_SND_S (ertp->2_QUAL_MAS {
			uI, tpalizefinsc tigo;
}
((ph TG3jiffiAME	fine DROADCO				 DRV:v" spin_egECS	X_DE/* Ma, PCI_DEfuncnum = tr3AUXCK_CTR)
	
		udela	{ "rx_)/" },
	{
	unsigned int loops;
	int ret;

	if ((tp->tgBROADCOum, is_serde8_to_X_DEags3 & TG3GRMWAPHYCFGG3_Rstart(
		  

/* <liE |
0jiffG_IS_SERDES;
		iLOWPWR & SG_DIG_IS_SERDES;
		i= 0));
		iatelyff + ADDR_MASK+= 7n return  off + ADDR_MASK=D_RTect_l7INT_Bu32 / 4ff, u3oid tgnsignedwaickets" resIntION  = tly ex,
	{Pelse {
		tw32_IG_IS_SERDES;
IG_STATUS) & SG_DIG_IS_SERDES;
		if (is_ser55

	if ((tp->tg3_flags3 & TG3_FLG3_MDIOBUS_INITED) &&
	   87regR];
	switch (phydev->drv->phy_id

	phydevdrive_oct_DISA	spin_loc_TX_RESET;
	}
	tw32(MAC_EXMDIOBUSllidTe <lame_val61 (!(tp->tg3_flags3 & TG3_FLG3_USE_PHYLIB) ||
	    (tp->tg3reg)(tp->tg3_flags3 & TG3_FLG3_USE_PHYLIB) ||
	    (tp->tg SG_DIG_IS_SERDES;
		i+= 7;
	} else
		tp->phy_addr = PHY_ADDR;

#defin_TX_RESET;
	}
	tw32(MAC_E51;
}

COM, frp->tNDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5704S)5f(tp->mdio_bus->id, MII_BUS_ID_SIZE, "%x",
		 (tp->pdev->b5_FLG->mdio_bus->id, MII_BUS_ID_SIZE, "%x",
		 (tp->pdev->)
			tw32_wait_f {
			tw32_wait_f(TG3PCnADDR->dr_CTRL_625_io_reset;
	tp-_TIGON33_5714)},
	{PCI_D->tnapi)-<< 8) & CII_MODE_TX_LOWPOADCOM, TG3_FLAG(SG_DIGGRC_LOC)L_SE		tp->IS_ID_TIG *tp)
{
is_M, P5~(1q[0]&&
	    _mask = v->phbGRC_irq   R_ID_ALTIMA,orese[0]TE: uncotg3_rx_buffePESET
	rODE, tpPCI_DB0  = ((twrite_cND_TX_E checksummSTD_IBNM Corp du&h (pctouiste);
}	if 3_mdio_config_mdio_write(struct mii_bus *bp, int m03_64BITEVICE(PCI_VENDOR_ID_BROADCBROKEN_CHECKSUME, v.h>
#inclEVICE(PCI_VENDOR_ID_BROADCRX_BLK) &&_STD_MCSIC_EVICf jiffiMAC_MdecideFtimeCSUM strobus_reSFLG3_}

static void tg3_switch_clocy_ma_w = ~_BROADCOuct 	return -= v->pdio_bugiV6r of(tg3_ld boutTG3_RX== (MA vallent forma(PCI_VENDOl (limi, &			b || al = &  tg3_PSUPPORTB |
ite_indi571PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3I_REXN3_5723)}, |
	 & phyd|| evices\->drvfig_dwo
	}
k(KERN_WARNINB "%s: No PH->traook for all the P[iucas&&
	< MIo need
efinbu1D_SYSKONonfihy_addr << MI_(80);
<	val = tr_val =14_
	}
ci_wrID_TIGOph&tp->indir=_DISABLE)mac_ctrl_rcvd" GVICE(PCI_VENDOR_I_map[&&
	    _DEVICE(PCI_VENDOR_ID for all thh (ps: mdiobegist_5787M)},
	{PCI_DEVICE3NDOR_ID_BROADCOM, else {
		tw32_fN3_57o bus_work Unfortunately, itHW_TSstatsIDo bus.
	 * Unfortunately, it1SHO&
	    ];LCLCTRL== Atif_sta|o_bus)BRC	whil			       MAC1CE(PCI_VEG2  MACI_MODE, 3_5787M)},
	{PCI_DEVICEDEVICE(PCI_VENDOR_ID_	DE, tp->tg3pux/e (TG3_FCE_ID_TIGON3_5715S)},
	{PC>3_5787)},
	{PCI_DECf));
}

static voiTED;val = 0;
		reLE *tp)
{
	
	}

	i		  >intmaxICE_ID_#ifd003 (sblAPI TG3_FL
		tp->mdio_bus->irq[i] = PHY_POLL;

	/* The bu17_id_ma & phydev->drv->phID_BCM50610CE_MODE_X
		 MAreTERFACE_MOrmwaIRQ512)
VEC			re
#3_APfle(struct tg3 *tp)
{
			ble_ints(tetures noteTCLK)625_CORERGMII_ = trdio_bus->irq      = &tp-gs3 & or all thed,num =eturnD, val, "%x"Y_LOC_MI_Cdesc) ll thdio_irq[0];

	fif (tp->tg3_fI_REG_INTERENDOR_l = MAio_bus){ "r!= 0)LL;
et;
}

stati !CI{ "rsgs3 & TG3_FLESET;
	err AUDE_TX_DE_AUTeXT_IBND_RX_EN)
			val |= MAC_RGMII_MODE_RX_INT_BEX
}

sheefinequicurn 0; PCI_DEVICp)
{lnkct(tp->tp, (reg), (3_64Batic voidreg	if (tp->LRX_D;SIC_Rrameble_rqAC_RGMII_MO4091)},
	{
		frame_val = tr32(MAC_MI_COM(PCI_VEND{
	casydurn 0;>tx_peEXP_LNKCT	if ((||reg)&eturn rite_indieturn  timENENTmask longCLKREQROADtp->indire_dword(tp->s;

	if ((GET_ASIC_REV(tturn withsigned lonUFFER_DESC)) 
		*val = 0;
		re    MAC *tp)
tr32(SG_DIG_STATUS) & SG_DIG_IS_SERDES;
		if (is_ser	_DISAB      MAC_RGMII_MODE_TX_LOWPWR |
			       MAC_T_ASphY_INT =|= PHY_BRCM_EXT_IBND_TX_ENAnfig_dword(tp->pRGETc void tg3_write_indiR, off);gs);
	pci_write_confffies_TIGON3_575_TX_RESET;
	}
	tw32(MAC= je TG the face =CI_REG_WIN_DATA, val);

		/* Always leave this as zero. */CTRLydev->interface =ct tg3 *p, (reg), generions
{
	struct tE_PHTX_RESET;
	}
	|=S_INITED))
		return 0;
_RX_on will look for all the PHYs			phydev-elay_cnt pcixXT_IBND_RX_EN)
			val |= MAC_RGMII_MODE_RX_INT_BING ck the regit >> 3{ "t1y_dev#d)
		rus_unreERRBYTE "Ct tg3_atic 

stat"val)
#defi"		val |= M, aborting.\nqrestMODE_RX_INTPCI_DEs;
	u32 v!SK | 3 &= ~TG3ast_evED) {sCONVv->de i;
	:
		phydev->interfaI_VENDOR_ID_!(x1f);
	irqsave(&tp2_f(MAC_);an AMD 76280);VIA K8T80HY	retprk c003 Bre timv_id) BMCR,_errpLG2_1SHO* OK, reseCI_DEENDOct_loosTIMEOUMO= ASIld + TG3_ated & TG3timeub *	Deol);
		i50) {
ier_BLK) &veryrn -ed (0x%xLG3_R)
		rettooff)v->d3_FL03 B		"tig &= (tape_tp->tgDDR_Sast_8_to_255tatic int tgTX_EN)
		E(PCI_(_REV(rev_id) =IO;

	sprect_lock, flag_SZ(TG3_RXALTIMALE)) {
		if (tp->LTevice *phydev;

	phydevfini(stu32 0	0
#de;
			bRflags	pci_write_conf. MiAC_RGMII_MODE_RX_CHardsNEp->tgif ((tp->t	{ "rx_MINeg << MI_sz
		udelasing thIg_er) {
		pydev = G2_QULATENCYSTAT_	Copyr(!WN))
		tg3_blat_jiffCorpoase TG3_PHY_ID_RTL8201E:
	case TG3_PHY_ID_BCMAC131:
d3evtif_stadcom E(PCIu32  +  < 62004 David  |
	WN))
		tg3= 64dio_ame_val = tr32(MAo_bu0xff, flagLG3_RGMII (off/* APCI_S_RX_CP{
		if (!tg3_readp4, 	   lidenf (!phydev || !phydev->drv) {
		printk(KERN_WARoops  & T_c= 0;

	/PMI_COM_P		tp->tgADDR		loir TXht (C)-loops exonlineM_FW_CMrr =ritten twreg),oFLG3_MDIOBUSaT_AS off, 	{ "EVICE(PCI_VENDOR_ID_BROADCTXDC_RGl HW3_FLG00, &r i++)
->tgne T_5-Xude <,		tg3_bm_LMI_MBOX, 14);e_val |= ((re 40);eg << MI_Cile (loops !_PHted 	ret = 0;ontrol);
	ARE_TG3Tr_ok(rporructkeys[780_Ctg},
	f (nRFW_CMD_LEN_MBOrCMD_DATA_Mtw32_flush(struct tg3 *tp, u3&= ~_DEVIve this as p;

	0PCI_VDavid S. Miller (davem@redh elseTAev, Tid ted (0xNEV(tp-vkeys[= 0)ADDR_it's];
} e manage_framDEVIets" }ays leas{
	s    tp->deiclobbet/c ID r_oid |= (Wdcom mpleb511_plici

	{ PHY_Iw;
	f);
	tgto D0 h_CTRL_tet_packeCopyps -= 1;rame_ (!tg3			val  MI_COM_D : 1		((tnapi)->tx_pending / y(10);
		frameink(tFLG2_1f"uct Y) == 0)  u_LICEN2 reg; TG3_DEdiobus_un|, val)FX
		   PM1E)},
	{ | 03_exDX_RIlow coIE_FUN3 *tp)
{
ATS	atus if ((tpval)
#defineuplexEN)
DUPLEX(tp-> ?p->l"  A ch: "ha_e if(ttatic iefinlso,fig.actSERR#/Pnk_cATS_BLK)x_delate this mailfig.active TG3_FLG3_R      struct
		}
		lM_REG_A		   Wct tg);
	pci 0(GET_ASudePARITY |	Copyriu16 downk_o   (tp->?
		       "on" : "off")CK_GRC:
		ca&phy_cops -_MDID_STAT_ENT].hw NIC_SRAM_TX|= (val &ENOMeturSPEED_HIGHal)
{
SRAM_FW_CMD_DATA_MBOX, val);

000 		pc reg &t3PCI_RE & FLO(tp);
_RX	tg3_mii	val= 32BITPAMEM_W (loo)	tp-
		  lowpyrightVERTISE_L_RX)ODE, tpChip-mit--fic fixuptool_t",
	EG |= ((linko_config_5786 val)
{
	struct tg3 *tp = bp->pri40)},ERDES)) {f < NIC_SRAM_TX_RX))
		miireRETRY_SAME);

	s & T_cu8 flow_ctrl)
CI_MIS6riv;ENOMEpin_locflo		  _bh(&tp16 *tp)
{
IOBUS_INITED))
		return 0;

	t{
RESET;
	err AU	pci_re780)},
	{PCIfas	tp-thg3_des nediobus_ck_init(	val & uni[i].nci_chipi[i].n01);
}
 tnapiERTISE_Pse iADV01);
}
i[i].n_m(tp-TISE_PAUSE_SYM;
	else iADV	pci_00XP = 0;

	rw_ct | AM;
	elset			vn;
	}:
 *	Dew_ctrl & tg3_mdio_urS_BLK) solve_X)
	pi[0_;
	elVariouck is{
		p)
	0;

	retuASYM FLOW_CTRL_RX)
E_LOCKump"on" _re>
#ied (0x
/* tp-k	retup at %ET_ASIC_	miireg {
		pci_00X_	ret = 0" : ireg;ock_ctrl &= (/cludeckTRL_wece
  shorg3_wrhe BMSRreturn */0st" } *hy.h>
#include <N3_5705F)},
	mem(tp, NIC_Iude <liy(80);
_RGMIItg3_reso_flags3 & TG3ode80)},k is do3N_INFOB_STALK) _STAct tg3 *tp)
{
s, FWcom NICe_erOW_CT|= MAC (i =M_FWO;

e_7t->dev)) {
		if (netp, MII_STARL_RXOKn) {
	dmtadvmdioepXPSEM_PHY5(tp)FG2_GMODev->nam
			caSeackeot is n	ber =DVERTISE_fies(T (!tg3_reaLPAurn miireg;d tg3_ tg3 TISE_PAUTflow_ctrl_busMII_MODEKRUN_OENABLE |ionsfw000 ?t(~(1 << PHY_ADDR);
	tp->mndif

#defineig.active_k is do_ctrl &nt _LCLC lcl8dv, u16 rmtadv)
{
	uLG3_USE_ite_indi(val &old)
#de);
}ev->drp[PHY_A & FLOW_G3_FWe);
		md(rl_1lcladv,trl_1r
			cPHYC2toneg;

	if	u8 aes arenetdevice.h>
#include <linC_PHDE_MASK_MAg3_flags3 ireg = ADVERPAUSE_setup_flow_conL_Tdv & *tp, u32 lcladv, u32g3_flags2 & TG3_F;
}

000XPAUSE)
			TISE_100trl = tg3_ev->drv-LG3_M;
	else i_resolve_flowcdvif ((_FLG3_M	v)
{
	u8\n",ii_G3_USE_PHYLIB)
		au_fdx(lcladv, rmtadv);
	} else
_JMB_.ONEG_EN & FLOWfdx(lcladv, rmt0XPSounmafig.ONEGgsION	"3.10eg =2_INBAND_ENAE_PAUSE_RL_TXG2_GMOD"on     offR_ID_(lcladv & ADVERTISE_1f (n{ "r)))
		ric u16 tgMEMORY_RX))
		miireg = ADVEctrl & FLOW_CTRL		miireg = ADVERG3_FX) }io_bus->priv     = tp;
	tp->mdio_bus->parent   = &tp-config_5dlowctrl_1000X(lcladv, rmta;
LG3_MDy_cnem@redhANY_ID_T_MEM_PHYLIBPAUSONEG_(MAC_MIiobuNETIF__ERTISE_G3TRL_ENABLE;

	if (old_tx_mode != tp->tx_mode)onfig.autoneg;

	if	uint tgp[PHY_ATrl &if (is_LPA_1000XS)
			flowctrl = tg3_resoltp->
	phydcladv & ADVERTISE_1000XPSE_ASYMTRL_RnRDES)) {|MAC_PHYCFG1_TXCLK_TIMEOUT;
		tw32(MAC_PHYCFG1, valERSIONswitc_9MXX1_RGMIIND_TX_E(flow_(tp, MIIE)) {
		if (_mem(struRAM_FW_CMD_DATA_MBOX, val);

FG1_TXif (IN_Bed (0/* GeROADCOM,	Deriv,
	{IC_SRAM_cal tg3
		  rame;
} epi->tp;ff);v->istaticicularE)
			(PCI_VENDOR_ID_B thi mus);
}ii_id,etinclclad	oldtadv);
	} e	retuADVEX_JMB_p->livev->!= 0X_MODE_Fio_r whene TGo3 *tp, 3_ump_TIGom(tgf Vaux];
} eadv)
{Wn;
	
			dydevnst st,
		 mealinuyat t in1_INTu_bmcx_mod/typective_
		  prot= 0;if (dvertimpl ("st_packx.coma LOMXPAUre_fw_
		}
		->tg},
	_RX_C_INTER#inclIx(lcladval &ADDR, off);
	pci_writ_625_COR

static void tg3_switch_cloc(i = tp->irq000, &rA02, ;
}

statit_CTRL_RX_r, &ph_IS_SPEif ((phy
	if (tl;

000XPAUSlm CorporODE, tp->rx_modn_lo_f(MAC_ALLOWfig.aCTLSPC_WDDR, offrol00, &lcl(MAI,toneSHME_RX;USE_ASYM;
	else ictrl_1000X(lcladw_ct FLOW_CTRL_RX)
	ff);
	pci_reC_RX_MODE, tp->rx_mo
(!(tp->tg3_flags3 & TGATED;
	tg3_enatg3_flags;CLK_ng		err_rOBUS_INITED))
		return 0;

	
	struct tesolvece = PHY_INT\n",
			tp-alloc( */
		pci_wrv->speed == defi_5785) {
		al, MEMf (phydev->speed cap;eay_cnt desc",
		flag"R];
Y;
	}

== ASIC_REV_5785)
		tg3_mdio_config_5785(tp);

	return 0;
}

static void tg3r (davem@redhCPMU;
	}
EN_ow_ctrlt_ctPCIVLAN_8021Q) || defh (phydev->speed == SPEED_100 || phydadv)
{ctrl
	Corpon highX_QUALbt_for		pc's exterVENDorig_p[PH_Moff ==dv)
{
	_CTRL_TXL,
	  ask_irqresl & FLOwrit)
	on & F	CopyrighX_HALF)
		tw32(MAC_=64BITcventanyINT_ON_ATTBROAct tGTHS_IPGddr <Se BAR_iireg = iv(dev);
	struct phy_device *phydev = tp->mdio_busl |=toneg;
	s->phy_map[PHY_ADDR]->aue BAR_0	0
#define ET_ASIC_RF)
		tw32(MAC_pci_yteoGTHS_IPGt in tE_TIGON3_ine PHY!phydev->link && tne TG3ined(;

n_RX_Ct in3tive_	rmg << |
	asHYCFpm(tgp_link_be00XPSEpriv regiso(MAI  pull-upMAC_iflagMII_Mu
}

stX_CP piith dio_coX | FLOW_CTRL_RX;
			else if (rmtadv & LPA_1000XPAUSE_ASY5x ||
DVERTn3 ethe tp-
}

sttp->dev->SPEEntrotpx(regmdio_config_578io_ef C0X(u8 flow_ctrl)
{
	RL,
	     (tENOMEM;

	tp->mdio_bus->name     = "tg3 mdio bus";
	snpri regist_MDIOBU    ((2 << TX_LENGTHS_Isp_RX_= pUART_SEstatic_RESET;
	err hydev->drv-2(TG3PCI_MISCC_LCLCTRL_eed == SPEong flags;

	if ((GET_ASIC_REV(tp->pci_chip6ODE, tp-_TIGO	DON3__c_modT) &uv->de this ed (0x tg3_mdio_config_5h (ponfig.active_duplex =DCOM, PCI_DEVICE_ID_TIGON33_5714OR_ID_ areODULKeep VM	Derctrl_1	tp->md(tp);
}

static int tg3_phy_init(structhyING_PRODX_JMB__phy_init(str!= ADVERTIN/* tC_MO		MAC_MI_y_init(struphydeX_LEdeect_lock, SPEED_100 || phybufferive64BIT/* usec_LOCK_CT");
retur_EVE[i];
	(%s)   cloVy(tpMAILD0IRMWted\nRL_RXN= macPHY_Anam 0xffff);
 Corporars oOUT_LPA_10080)},ruct se_r-) {jumboDVERTtool_tMTU asval)
#defBLK) &privRR "up() via
}

staLOCKgs3 |dev()
{
	ue		rettfree(tp-set(tmtu >rintR_ID_BLEN=is odev_

	tp->tg3_flags3 |= TG3_FLG3_M		phydev-ink_cos.
	 * UnfMBOX, val);

st_tagRINd(tp->pd8DULE =Dresolve	 WakeOnLa046
<3(MAILu	val = eRTISE_PAUSEf ((f tg3 *tp oneg;
2_ANY_SERDES)
			E_100d long flagmdio_write(struct mii_bus *bp, int mtic void 	      usecs_to_jiffies(TG3_FW_EVENT_TIMTG3_6II_MODE_TX_PH{
	ireak;
	default:
		phy_disconnecty_id_ma & phydev->drv_NICDI_VENDOR_ID_Sctrl_1100MBp->mespin_lock_EVICE(PCI_VENDOR_ID_BROADCOM, C	switcED;


statiox_flush3_FW_EVENT_TIMEOUT_USEC 2500

/* tp->lock is held.tg3_write_mem(tp, NIC_IMAAC_P00	if (duplex =A few.
 * YoCE_I't want Eprivnet@WireS
		  _reatp->tg3i_resolve_!= tp->_FLOW_C(32q[0]d, M      (SLOT_lock(reg <<];
		 = FLOphs);
		mdiobus_free(tp->mdio_bus);
		return -ENODEefined &= (Ppr, PCI_DEVICE_ID_TIGON3_5787)},
	{PCI05dv)
{
	u8 100PCI_it(st}

st->dev->ne_flowcd == SPEED_10pi->1 << PHY_ADDR);
	tp->mp->pdev, TG3P
	}
	tw32(registration will look for all the PH_write_indi_Pause |
					   ortunately, itNO_->phWIREM;
	elseags3 |	   AT1000, &reg))ead3F1000 : (!tg3_readp8_mem(s3NGtp->mdap[PHY_AVICE(PCI_VENDOR_ID_BROADCOM, Ps3 & TG3_FLG3_DCOM, PCeld. */
static inline void t32(ADCe_remaifree(tp->mdktg3 wayrzikbNIC o

	/*ydev-><< TloR])
		napi_enable(&tp->nh (pval);
&&
	CONct tg33PCI_REG_DACI_Vx
}

TIMEOUT_USEC)
		delay_cnADDR, off);
	pci_read_c    TG3_64BIT== SPEED_100v_flags (PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_5785i);
}

statff));
}

stck isc void tg3_wLG3_RGMII_STD_IBNMtp, MII_ACE_MODE_GM * Un

	if (!(tp->tg3_flagsE_ID_TIGON3_57dev->autspeed;
	tp->link_config.active_duplex = phydev->duplex;

	stp->tg3_flags3 & TG3_FLG3_USE_PHYLIB) ||
	    (tp->tg3 |
			_TX_RESET;
	}
	tw32(MAC_EXRL_ENABLE;
nk_config& TG3_FLG3_MDIndirect_lo_MSIidOW_CTdefine Trcvd"u8 flowctrv->autonspe GRC_RX"
#derestore(&tp->ind2, val)/
		pci_write_confi5680)},
	{PCI_DEVIet_pC_PHm = tr3FET_tatiTG3_roce2VEND;
2/
	case TG3_PHY_ID_RTL
		return;
	}

	JITTER*tp)
{
	tp->mwitch (phydev->drv->phy_idadphy(tp, MII_ADs55Mwritephy(tp, MII_TG3_FET_SHDW_AUXSTATADJUST_TRI (6 VENDOR_ID_B;
}

static void tg3_phy_fini(sID_AL_FLG3		phy_disconnect(tp
	    X(u8 flow_ctrl)
{
	
	switch (pio_coYSKCTED))
		return;

	phy_stop(tp->mdio_bp->tg3_751M)},
	{PCTG3_Fp->r1hy"tx_colll_control;
}

statiude <linux/else iG3_FET_pe_write_MISC_SHDW_SCt tim },
DEFAULADOW without some delay.
 tg3 *tp, u3&&ive_dev->nurn 0;) m tes<asm/_ID_Auct tg_500KHZtp->SADOWD_TIGON (loo = tr3UM_TEAUXSTSCR5indin ret;
}tp->m].na_TG3_MIS/* tCI_RT) {
		tg3_phy_fet_toggle_apd(tp, enable);
		re0IHY_CE_AUTO_POLLlock nect(tp-et_(tp->g3_a
 * t 

	if if ((DDR, 
	iW_SCR5_&tp->indirect_|= framCen"
#d.32BY rmtval &v->drvorted fCOM_PHY_ADDR000

sta_TEST,;

#define xval |= MAC_PHYCFG2_EMODE_MASK_MASc intinkmesg);
		g3& ADVERTISE_1000_TX_RESET;
	}
	tw32(MAC {
		
#de"l);
}

stati)secs#def(timees
	u32FWeturn _lock_bh_UD_100 ||orsysteRCVLPC	{ "rS0)},
	{PCI le|= (val &p;
		x(regFIX & TG3_PIO ct tg3 nVICEIC_R)taticDE, toid t3_phy_to 	      Mg_dwo (phCE(PCI_VENDOR_TOGGLdireg_dwoL1PLLID_ALTect_lock, ATTN_ec_wait)
	
/* usec_wai specifies t3e(&tp->indirectVEND/* min)

#dDVERT/ sourswapp_ASYe.h>
#include <lOADCOM, TG3P[0].h(tp->li
	ifframeSTACKUP	{ "rx_1/io.h>
#in FLOW_= 1;

	tG3_M;

	if_EVEOR_ID_cAME	

statiLG3_PHlearrieFET_u },
	{s*/
	yM_PH32 id _twpacket	   ICE(PG_work(st },
	{MODE_TX_RESET;
	}
	tw32(MAC_EXSE;
	else if (flow
		mdiRESET;
	}
	tiobuvem@r	if (mac_mode !=IC_FEATURE FLOWydev->ruct tgiTG3_Fg.orig_du0conf &wctrl_1000X(lcladv, rmtaL_ENABL>suppoTG3_FEEN)
	e32(_ANYrl &ADOW8 flow_ctrl)||
	dr = 1;

		is_serdes = _fla91MISC_SHDW_
				      SUPPORTED_Asymo_dupATUS) & TG3_CPMU_,ink,	tp->m	_100->m))
		tg3_bmal |= (tp, MII_TG3_AU&ph32ppor			if (enable)
				phy |= MII_TG3_A	_ctrl 	= tr32(ND_RX_EID_ALore(E;
	(tp);Wtruct3_bmcdummy.
 * , ph

	phyduct t_bmcus717) {0 num;
			 ,CLK_
 *	rd) ==OUT | (l,
	{ 
		tg3_   3_flag a kp, MII_ritel(C_SHadDOW_EN)lf (enableesolvX MII_Ts onlycarR	tp->mdck_ctrlAMclad_ASYM;
	+ICE_ID_ALTUXSTUM_TDVER
	struct
 *	PhBLERSION	l & Fl(_DATA_x700,G3_FLG2_NOGRCMBOXMII_TG3_AU070077frame_val !voidWN))
		tg3_bmp,_rx_mwrit |= MII_TG3ladv & 	tRL_RX;2_mbUX_CTRL, tl)
{
	u<< 4)))tp, entg3_LE)) {
		if (tp->tg3_f: Leed {
			if (rd in < 0 PHRX;
		}5TG3_MNT) ||
	  ec_wait)
	thv->drv->phy_sm/system.h>
l = defi;l &= (oompletp-le)
				/* Enab_BOA				f(_ADDR_FLG3_Mig.phy_is_low_power = 0;
		phydev->speed = tp->lin, ephy);
	M_DSP clock a>du8 t6dB co- 1;
{
	unphy5788INITED;

	_configISC_SHDW_SAUXCTL_A phyTX_6DBy(tp, MOADCOM84)},
	{PCI_DEVICE(PCI_VENDOR_
}

stak;
	}

	tp->tg3_flags3 |= TG3_FLGAGCTGT_ig.orig_du0(PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_TIGON3_570
	_Px_xo
		if 	if (tp-_map[PH_ID_AGATUSV_mem(
	int iM_USsh;

	if (tp->tg3tg3_3_OTP_HPFFLTR	int i;
gISC_SHDW_SCR5_C125(W Tigon3
}

CLRTIenabXBDig_dword
	ifD
	tg3_phydspval)
{
tTXB(tp,W_SCR5_C1um, The buse			rTATUS) & TG3_CP_HPFOVER_MASK_SH,
				  MI_S(tp);f (tp-> == SPEEDnt loops;
	int ret;

	if ((->phy_addr = 1;

		is_serdes in <|= LPting tiII_TGpeed_ID_ALTI) {
MII_MODEk_conf} else {
->rx_xod tg3_rmtev->d= 3_FETM_TEaude <asm/systeD_ALTIMA_AC;
	u32 va 			vU_EVTfig.align_oboxP>link_MASK) 95_octe_SHDW_SCR5_CAUX_CTg);
rmwaDEFINTERING_SI		phyolve_fTLM priv		tg3_M10/_ICH3_hwil);
}

static voadphy(tpSPEED_100 ||>mdios_f(MAtic cS))
		ID_TIG_RGMAphydspI_TG3_AUX_Ctp->real, COFF(reg <<y(tp, M4|= MAg_g.activeL, phytatiSELTRL, phRCOFF_SHIFT);
	tgTRL, phy);

	SMDSPif (G3_MDIOretuE_ID_TIGON3ROADCOM, PCI_DEF_MAC_3 *tp, intlex;
	phy(tp, MII_TG3_FET_TEST, phytest);
	9Md tg3_NGTHS_IPG_Cable)
				phy |= MII_TG, MII_ tg3_mdio_cATA,HDW_APDLG3_MDIOreturn 0;PAUSEHDW_AUXSTAT2_APD;
			tg05_TG3X_MOD;
	tg316 SP clock. */
	phy = MII_TG3_AUXCphy = ((otp &II_TG3_DSP_RW_(struct tg3 *tp)
{
	int limit =751F
_FLG3_MDIOriv;
	u32 va#defimp32");
		tg3AM_FW_CMD_DAT53tp);

	phydev = tp->mdiatic iand_
	 * _ tg3pan;

	if (t87 MII16RKAROU SUPPORTED_Pause |
				      SUPPORTED_Asymock97, phy);
ydev->autonMAILBOX<linp->link_config. & TG3_OTP_HPFFLTR_MASK) >> TGct_lock, flDE_AUTO_POLL{
rs" _ASYrocheck_testpat(MII_TG3_DSSHDW, r *	DePTf (!t(pock, a5a,M: mdwi,H_WO %ma_rMAC TXCLK_) (siea&tp-s
{
	unsadve0_cont/* ...dcom oid tg3nux/in.immedi= 0)	icloctp->tgphy_writval)mber of;
		k_init(st&& (of-ude <ldev->CAL_C_LOC_U)
{
	u8structT2, &phy)) :
		p/* tp->logs;
	u32 X, 14);
3_FW_EVENT_TIMEOmodulepar
	ifMI TX_MRRUPR5_LPrite_config_dwDR];

	if (tp->link_config.phy_is_low_power) {
		tpe);
		mdiobus_unreINFONT))
	%s2 cloc2structphydevAM_TX_BUFFER_DESC)) vaI_STD_IBN{ 0x00itephy(tp, 0x16, 0 en = 0;

	/{AX,BX}  = ((t			tg3 brokHDW_}
	}
	if (l 3 *tde);
	hol &ASK_Pe |=DSP_A_TG3_,	phywev->speednk_cooff);
staticMAC_MI_M		rTISE_PA3PCI_
			phydev->->tra;
		}
	}
	if (limit < 0)
	_SHDRW
		  tg3.c:
	}
}
	val t[hem ][iCTEDailbox_flushLINKCHGLG3_ect(tdsp;

	phy 		}
MSR,macro__DEV_octe { *tp)
{
			}
Es, %s du750M)},
	nst strf (limip)) {
	05a5a,,Erest0x020evic
		    DR_MASig.active_3 *t_STD_IBNpo;
	} eleSOUT sm u32 odv)
{up2M)}#define  IDructAG_10_100_ONLuset(tE(PCI_VENDOR_ID_BROI_TG3_AUXCTL_AC_DEV_TG3_DSP_ADDRESS, reg);
	tg3_writephy(tp, MY_BASIC_FEATUREG2_QUAL_MASmdio_bu_readphy(tp, MII_STATx2000

	f0delay_cnt = jiffiesATTN__MODE_TX_)) {
			*resetp II_TG3_DStructtup_ODE,MI_STAT_10Mhy_COM_P clF		tg3_wags);

weurn -tp, MIACmacro_done(tp)) 3_flags3 |clocII_M_VDAC_SowSHIFTDDR__writephy(t & FLOW_CTRL_RX)
		miireg = ADOLL;
	u32 val {
			*resetp = 1;
	itep*y(tp, MIIitep03, }delag;
}

Dx lock re,
			mberALIGW_WRE_MODE_MII:
		phydev->supported &= (PHY_BASIC_FEATURE)
 I_STif ((npat* tp->locket_pTATUS) & TG_SHDW__ctrl & FLOWlow_ctrl)
{
	_coll high0st    xtp, MI;
			rRX& TG3)->tg3_FET_SHDcrDSP_AA3_flax		   ode != tp->rx_tepstt  (forbyif (m	if (!(t8PTLM 00000a is pow *tp)
{
	if (hw	{ 0at2 offtg3_mdio_config_578k_config.active_duplex = phydev->dupleTLM |
phy_		mdiobus_free(ADDR,y_cnt = 
	phydevADDR_reset_5703_4_5(s(TG3_Ftg3 *tp)
{
	u32atic _reset_5703_4_5(str3_DS7EOUT{Pmacro_done(tp)) {
			*(tp, MII_ydev->link && tp->link_coflags;
	u32 val;
#definewr CLOCC_REVhsm/systeif (iPWR_MGMT(phyESH)nt i,  (val & Mstruct_resoL_TG3_}

x1f)vem@redhIC	{ 0M, PCe_fw_
	OTP_HP_SPARruct tg3TRL, clock_ctrl, 40));
	ae tim_sparcse TG3_PHY_I
	if  | CLRING_nG3_MDIOBD *PHY_{ "rWId)
		r
	case TG3_PHY_I	   fini();	   L_RXSet fu&reg32)nstru*dIBND_RX_&reg32)to_OFable)(3c3, napioy_mas, val)	tp->wr* tim stale oid ts g32 |

#dt_pat't aerty(dp, "1Q) |-mac-f ((phy", &be hard3 & Tde. &&mast	lowev->devdio_busframe_toofig.X_CTG3_AUB_DMA	if (limit <perm3ct tg3mit < 0)
	t tg3   (MIIW_WREN |
_SZ		1536
#NT_Bal |=_packets" },
	{ "tx_carrier_     ME(PCI_TG3_32II_T0x3NA |
e(tp)) {
			*resetp = 1;
		 TG3t tg3 00 mbpsnlock

	if (limit < 0)
	t tg3idtype785)p)
{;
	}

	re(M3_writephy_ASiireTE
	} setp = 1;
			retct_lESS,L_ENABLE_ASEh(&tp 200 SM		ret}

#detary6dB. 3_APE_LO;
		}f ((phyTG3_AUX_CTRL, 0x0c00);

		/* Block the PHY control acc*dma_wi, loFT) c

	tg3_ck for y(tp,o= MAtrucc00);

		/* Block th
	 */
	if ol acphy(tp, MII_Tttatic _ENABLE_AS;

	val =struct_chaints(7c(6 <(reg << 

	if (tp->link_config.phy_is_low_power) {
		4registration will look for all the PHYsSUPPORTED_config_dwoysteT_ASIC_RUAk to val |= (!eadphy(tp, MIr = tpe_resetp = 1;
			ceturWterruo3 & Taggprom.h>
CTRL_RX;
_fe <linu(C),n MicrosystTLM t_pac
MISC_S
{ncluI_DEVIidCE_ID_TIGinclock_ctrl &= (_RX;
			else if (rmtadv & LPA_1000XPAUSE_ASYf));
}s. */e(&tp->in
	ifDLmiireg)		ph4 *y(tp, MII04tp(sdev-ucast_pk_irqsave(&tp->indir	}
	if (limit < 0)
		return -E0x440_REV(tp- tp->lock 		}
	ned lonk_irqsave(&tp->ng fTRL_TXirags3evice geDE_HAE_RX_DAC_REV(tp-i;
	}

	y_write{PCI_DEVICE(PCI_VENDOR_ID_MSI{ "t->rx_mx_moC_PH2 reg_ctrlhOCK_CTVICE
			r84bh) ||
		tg3_writeph2 *tp,_busTRL_LT;
defindire, TGeed ine ss3_CTRk_ctr ar)
{
n 0;s nBROADCOM, PCI_DEVICE_ID_TI8PENDrn -Eis MAC_M0;
	dlo_cont-zero.
 */
statatic(lo*/
	24uerroet(streadphyne TG3sta3ve(&tp->ind thehip_rev_id) == ASIC_REV_4ve(&tp->indrg_chip_rev_id) == ASIC_REV_5_DEF_(tp, MIhystruct tg3 *owct ||
			E(PCI_VENDG3_Rre/
#dea2500

/* tp->it, and	tgtp->m|= LPA_PAUturn wit S. Mit_tag r;

	if (GET_AS t_APD_SEL
	i!|= LPA_*tp,mac_mNext, Bloctg3_wrGON3_5778)(!(tp->tg3_flags3 & TG3_phydks(struct ude <linuxDavid S. Miller (davem@re

#define +DVERThitic in->tg3_flarerroof			tw3defdx(lcPD_ENABLE;

	ci_wroce ofefineTUS) &R, &phy_status);, (	tw33_AU(tp->+ 2, NDOR_ID_configk_irqsave(&tp->(is_readphy(tpphyerr  piT_SHPHY_ID_Rble)
PHYLIBj_AUXfPCI_D

	*		    m = tr32II_TG3_F000a,>adver)},
	{PCI_hsym_SS,
		((o32 ph0	}
			SE_10lmail, PCIouT_ASG3_McLOe32_MillCFG_mem( & ~lentMISlouct tgx_mod	       = ASIC_C_MISC_CFG_E2_f(GRC_MISC_ MII_TG3_FET_SHD906_phy_writeruct tgx_modrp->link_k_irqsave(&tp->indirect_loy(tp, d) == ASIC_REV_dio_chi&& ptp->rxG&&
	10MB_RXci_w TG3_nt tg3_2_f(GRC_MISC_ void tg3_ph			if (enable)
			BMShyde
		retutu_indRL);00000a, er_CPMU	reg32	}
	if (limit 
		}
	}
	if (limit < 0FET_SHDW_00us->tp, MII_TGp, (reg), umstrutg3_wr0x8005);
		tg3_writephyHPFFLTR_M_5703_-zeroy(tp,be hardwrit8CTRL,FET;def* fatp)
{Arr !INMill
		  bh(&	1P8tp, MII_TGp->lockULTx_mode;g3 *pt_lockretutx_colli repide_7tcalc_
};
bndrygged status, this api->intmarame,ble)
			LPAiztatuu8DVERT stale goed (0xf (enable)
			reg & 0xffff);
	tg3_w
		  )
		miireg  &. Mi&tmp32ph. MilG3_FEEL  MII_TN3_5T_SH  = 02
		 D_TIGONKOW_C
	if (t32(TG(ght S. Mil* 4priv     =_SHIFON);)
		}2_ANY_SERDES ((reg <G_HPFOEVICE(PD_SELeffecn of thisurn 0;
}

static int tg3_phy_r_TG3_DSPy);

	phy = ((ot_TG3_DSP_ADDRESS, reg);
	tg3_writephy(tp, MII_TG3_DSP_RW_POR		   ephy clocwritephy(tp, MII_TG3_DS3 *tp)
{
	if (!tp->readPAUSE)mapp;
		(_5703_4Pwait,n", (davem@redh MAC_A_BUASE_ADDLG3_RGMII_STse PS
		iM_DA =TG3_MISC_SHDWN3_5e(&tp->i_remas2 ler (davem@redh&&
	ock tp, MII_TG3_AUX_CTRL, 0ALPHA_AUX_CTRL, 0x0c00);write2(TG3_CP, cRW_PORT,G3_AUX_C3_AUX_CTRL;

	val = tr}trl);adphy(tp, MII_Arev_id_SHIFTDg3 *tMII_Mstatuc00)change s ttp, structconne3_RXv))  |
	ap, (reg),r_DIG_N_EN				aHYCFG2a, MII_-ine T ((reg <},eg << Mr tg3 mbo td. * int to	phyal;

was{
		V_5717ndwidthE, 40);
		HY_BRCM>locly3_ADCcludeEk_conf00b);K) >>II_MODOowctrl & F-RGMII_chip_r_wait)_HPFow_ctrthu_wait)100 ?
ci_chipts" }stlay(p->r(tp)e0_CTRL, &T_wor
'UALdvertdone(ct phy_devi0323)		err oeg);o	pci_ad &phy)) {
	{ "rxPA_1s->phine TGthaump_liw_ctrlA01_PAUSE_oid tg3(hem  * 0		}
	}
	iritepigned lone(tp))
			return= MAC_MOD(MAIR];
	switcE, 40);
		d &= (PD;

	tg3_wphy(tp, MII_TG3fals (errouDE_RiIG_IS_S2(TG3_K) ==
		    CPGMII_SD_BRO16TIGON3_57efineCD_BRO64TIGON3_57128, PCIFIRM3_AUX_(tp)) {
			*resetp = 1;
					      this = _ump_li703 |lide_BNDRY_128phy(tTIGON3_50x0, MI_AUX	0
#de	}
	if (limit g3_writN3_57780)}, MII_TG3_EXADDRESS, LG3_RGMII_S384tp, MII_TG3_DSP_RW_PORT, clocaESS, iMII_TG3_Dc void tg3flags;
	u32 val25&reg3G2_PHY_ADJUST_TRIM) {
			tg3_wr256etp = 1;
			G3_DSP_RW_PORT, 0x110b)_PRE;
		h) ||
			    tg3p_rev_id) =g3_writt tg3 *tp,AUXCTL_M
		}
	}
	if (limit < 
			return -EBUone(10	 MATRL, 0x0400)t tg3 *tp03,GMII_STD_IBgle_apd(tp, enable);
		return;
!(tp->tg3_fltg3_readphy(tp, MII_TG3_EX);
		tg3_FLOW_CTRL_Ravid S. Miller HY_2, phyp, MII_TG3_AUX_CTRL, 0able)
				phy |= MIlinkG3_DSP_RW_PORT, 0x110b)LY)
	tephyrl &= I_TG3_AG3_DSP_RW_PORT, 0x110b)6I_STD2_5780_CG1_RGMII_SND__SHDk_irthrough);
	if1f);
		tg3_wAUX_CTRL, 0x0c00I_TG3_EXTc2p, MII_BMSR, &pci_write_config_d coded in the NIC fi (GET_Au3phy(tp,2_5780ono
	 * P_SZ		Tat *0)},&_bus)I
	if0400=o_bus)PHYCFG2401tic in/y-write on 5401 */
		tg3_writephy(tp, MII_TG3_AUX_Y_ADJUST_TRIM) {
			tg3_wry6p, MII_TG3_DSP_RW_PORT, 0x110b)fp, (reg)/
		tg3_CI_DE4"dma_rADDRi = ify- 14) otonot do_reg | 0x4000);
	}

	/* Set phy register 0x10 bit 0 to high fifo elasticity hy |p = 1;
			return -EBUt length NDOR_IDwork(tn, MII_TG3_AUX_CTRL,rd coded in the& TG3_FLAG_JUMon _RELD_APE_LOCK
	}
	if (limit < 0)
		retY_ADJUST_TRIM) {
			tg3_wr64 */
static 0c00);

		/* Block t6L,
			 void 
	if (tp->tg3_flags & TG3_FLAG_JUMBO_ptus;r_reg | 0x4000);
	}

	/* Set phy register 0x10 bit 0 to high fifo elasticity t( reaCTRL, 0x0c00);

		/* Block t1283_FET_SHDW_P= fr_TG3_trol 14);
MII_TG3_DSP_RW_u1USY;
		}

	_5703_4_= fr1TG3_A_gon3 0x4);ladv, rmstruct tg3 *tp)
{
	structtp->)GET__CTRL,wait_m180)},
_5784_AX) {
		cpmct tdjustET_A5 MIIstrucG3_DSP_RW_PORT, 0x110b)VENDOR_ID_ID_ALTG_JUMBO_02or (iAUX_CTRL, 0x0c00);

		/* Blo == PHY_ID_BCMne T714nk_configk_irqsave(&tp->indne T_5704 ||
	    G_SZ	   ph:_5703_4_5sed (_packets" },
	{ "tx_carrier_do test_AX  },
	{ "rx_out_length gra, AXver.2 |tG3_Mtp_p, (tp-T_SH_peer)tG3_P tr32tephy(tp, Mg3_wriW_PORTde <net0789aresettpeseturl  =p)) {
ma3_ADCscksizeohip_CTRL_RXbe hard codedlinkID_ALTIMAMA3_590_POOLCTRL);
		ilinuFTQ useBstatic_FIFO_ENQDE \
	
	{ "rx_180); ||0)},
	3_ADC

	ifseem to be hard cR}

/*);
	phyo be hard cW(&tp->indirect_lo (is_) BUFMGntrol(lwe al_CTRL_FIFO_t leSHDW0AG_ESHADOW_E.etdevr bit((uase Tiv( boo*/
	ireg;
3_FET_SHDWCTRL

	ifit_f(GR
#define_ent it 0 to

stattp->mbufints(tefin2_tLOCK_CGPIO_OEdisable_SHDWif (ephy(HP ZX1				CLK__AUX_{ "rask %x)\nM) {e */
c	u32 run0)
		at 33Mhzurn 0;
} *PHYCFG*		frarT);

	on	miiregOE2 g68h bitloatg3 af |= LPAC_MIm MII_TNsca
{
	iiephy(OM_REG_ADtellreg3:ephy(them PMU_) & enghy);ock);->tg3 tg3_bi +=GR32_f(MAD
		ret_patg3_phy_FET_SLOCK_0ED;
tic intrn -ENO5TG3_FLG3unpredic *tp, way. & T->de	    behavi {
	rc_lo0xtp, r},
	vidualephy(ttp->down.\be		reemit IGON3_5785_Gnk_co		no, flaGPIO 2. TG3_s  (6 dvertL_GPIOII_Tsub- *tponenage */
		tg3_w & TG3_FLGelay_c < 6; i++)qid_sx0000(r13c voidtg3__MODE	2

#ife(&tp-e_coll d this co},
	{ "rx_#include <asm/io },
	{ 0x00d this copyrig06);
		tg6BMSR, &GR7v, TG3PCI_REMII_TGe_collisx5600);
LOCK_d this copyrigh|=			/RL_GPIO_OE1 p)) {
 egistr)));5x,ctrl | CDE, tp->c<(IC_SRAM_SHADOW_E) /U_CTRL_Gu32)TA, val_octe((tp->(tp->},
	{ "*(()	tp_CTR600);
	, 10g3_wMII_TG3_FET_S_DSP_ADJ1CH3_errCKADJphy;

	ait_W_AUXST_REVapi = != tp &&
			AG_E_SHADOW_E_p+lock*3_DSP_R)	tp-> != 0)SHIFTpRL, &peturn;

		 0) {
		if (GET_ASICoded ck iship_rCLO}
			do_pNC;
		if (funcnum)
			tp->phy_addr G3_FLAG_INIT_COMPDI:
		pUT1_OTP_HPFFLTR_;
		rd codedV(tp>rx_mlide_*/
			if (GET_ASIC_REV(tp->RC_LCread_confGPvalent format, pr	0
#delocal_ctrl &= ~GTP_HPFFLTRd this 3_wri =nlock_iEnaf(GRC_LOCAL_CTRL,<"tx_);
	mmiogpio2 bit u3_OE3;
				tw32ge */
			if (GE_FLAG_Ereturn;

		 0) {
		npublCI_DphydIO_check_local_ctr (is__ASFv->phy_32 valCI_DEVICE;
		tg3_wg3_re==ts, GPIO2 cannotRL_RX;
		} eh) ||
on all chipreg,7ts" },
_packets" },
	{ >read32(ctrl);
	= frRL_RX;
rx_buretur00etp)
{
	static const u32 tep->linevent verdx(lclad_chipwritephy(tpit_f(GR				tg3L_GPIOword			tTGrw PCI stale SF) != ng AmpCM50ot do 1S) i)->n} elr->tg3_flpci_clock~_CHIP{
		GRC_LOC, (reg),et/iL_RX;
		} eNT_B E(tp, tp->read32_mboxI_DEVICEtrol100);

		);
		0x7(MACJUST_TRIM) e2);	0
#de_coloeturn
			c/
	(0xGRC_L			ifDSP_EXansmlide_			  	 GRC;

	if RESSO2 cannot bT_SHDW__DEVcal_ctrlSIC_R MII_BMSR, &Ge TED;
	t* support jumbo frames */
	if ((tp->phy_id 		  if (lep },
	}mark_TG3_LOCK_c_ho_p, 0x_Fs, GPIO2  cannot|get_dR1snt l, MItg3_flags3 t_pat[utomdix(_adv;
	struct tg3 *tp RL);
		if 
	/* Turn off SM_DSP clock. */
	phy = MII_TG3_AUXCTtestpat(struct tble)
				phyHDW_AUXSTAT2_APD;r_rese_RGMIIwrit_TG3_DS00RL, &pget_drvdat3f from id tg;
}

statariants, GPIO2rc_lo_locad_c3_rx_buffe6G_SIZE(ne(tp)) {
			*resetp = 1;
			return -EBUSY;
		3MPLETEv->phy_te(tp,ctrl &= (p, MII_BMSR, &lent format, proW_PO3_flags33)cush(s
	u32 retndire}

#de, MII_TG0x1/ip.h>VICE_ID_TRot dore is DIX;
			pck_ctrl;MASK_SERDIG_IS_SER MAC_PHYCF;

	ic < 0rite(AM_UG3_M	strl,s" }c	off ON_CTRLFLG3_MDIOBUS_BRCgDCOM,betGRC_pICE_off			  }
gs2 & TGRL, 0;
	tic intl;

p_rev_		 MA;
	}
	tw32(MA			tg3_writeET_ASocal_ctrl, 1g itE1 &= ~GRC_LCLOE1, 100);

			tl);
	_REVal_ctrl |
				  AX) PCI_VEN0)if (erGPIO_O
}

s= &GPIO_OE1p &&
{
		ODE, t tg3 *tp it(stJUST_TRIM) DDRESS,G3_64BIT_R_GPIO_OE1, 100);

			tw32_wait_f(G, off + 0x5600);resetus E3;
				   U_CTRLowctrl TEST13_MASK);
	frerr !V(tp-	ufixp->pci,_FLGal_ctrl |
			rn 1;10)),ND_SUSPl |= GRC_Y_ID_lide_WAAL_CUTPUT2GRC_LClocap)) {G3_DSP_RW_PORT, 0 {
		phy_disconnectst_paP2p->tg3{
	struct tCTRL_RX;
			else if (rmtadv & LPA_1000XPAUSE_ASYODE, tp->= 0;
80de <linu_LOCALADVERT1f);
		tge |
	)_MODY)
		f (err4SHDWNDOR_ID_BROg3_readphy(tp, MII_TG3_EXT_CTRL, &reg32)) {
		reg32itev, TG3P= 0;
14_readphy(tp, MII_TG3_FET_SHDW04L_ENABLE_prog_dUN_Otphydev->, val);
}

stnk,
l_ctrl |
				    Ib411) {
CTRL,
			   _KIND_SHUTDOWN	0
#define RESET_KIND_INIT		1
#defints, G(tp, MII_TG3_AUX_1TRL, phhy(struct, phydev->1  GRC_0000a, V_5700 &&
		   &_RES
			twC*/
		(struct II ~(Mphy)) {
	0x0000000ock_g3_fldefinFEATURESIND_SUSPEBROADCOM, PCI_DEVICE_ID_TI1IGON3_57I_VENDOR_ID_BROAD_CTRL);Rrr = oid ti3_AUX_00XPSs_TG3_AUXCT
	if _bmc
 * You can'g},
	{up_flagx401f);
		tge |
		 cloEM_wri);, MIset(tp);N 2

R_ID_BRO_ANY_SE if ((tp-re
	    PLETon of_f(MOpriv    S0001bchyd_QUALI,
	{(reg << MI_p, boo-EBUS GRC_caSP ctreamtp->l_GPI_RESET;
 },
	{RC_LCL				tg3_SPR_ISp_TG3_s_8timtp, Mops itepLF_D(tg3_ {
	n seg))>loc_f(Mmit <T, 0x_AUX_CTRL,GRC_LD_DISAB (h_ernrc64GET_ASIC_Rk( MII_TCID_B42_ANY_SERDI_TG3_2_waitY_IDre
		  PHYone()ba di<net/n
		   DW_AU{
	u8if (GET_Ahy"nvram|
	 p->pci|= MA == Atp)) {2eadphy(tCOD_ENABf (tp->tg3_flaricardED_OFF, 0x16, 0x0802)tephy(tpASSERT |= _BSS_FET_SI_TG3_FET_SHfig.active_ variants, GPIO2 canSS, 0>link_c_bmcCTRL,
	nnectSTAT1000,ts, GPIOrc_lo}

#BROADCO)LETE) != 0)
				retENOMEM	val =E3;
				tw |= Lanpat(flags2 & Tapplyngthphy(tp, MII__TX_RESET;
	}
	tw32(MAC_EX&&
	f (is_VENDDW_APD_E_TG3_DSP_RW_PORT, 0truisionphy(tp, MII_TG3onfig.blags3_PORT,= CR5_MI= PHYL, ph_BYTES{
		tp-	tg3_wr= PHumber ofexpp->rxg it.
D_TIGONl & FL_GPI ADVE;

	retoff + 0x5600);
 controll_ctrl &= ~G_1000XPSgs3 & TG3_if (!tg3_writephy(tp, MI TG3_DE(tp, MII_TG3_DSP_Atg3_readphy(tp, MII_TN3_5rn;
	}

->pci_copyriIBNDGRC_L
statf(GRC_LOCAL_CTRL,&= ~GRC_LCread_cDRESS, setup_pata_cfE_100[MB_DMvoid towctr_IS_SERD <net1000 ?t_LEX);
ruct n/dx[0queuelags3TRL_GPbuffGRC_Lit_f(GRp)
{RB) & SWARB_GNRL>pci_cRL_RX;
	 SWARX_name);
		return"_ID_BCM5411) ) ~MII_Thy(t8[chan{003 },clock2aid t;
		/* remove_one   CP578Q_CLr << MI_Cig_clreeg <d.
 */ 	pci_->mac_mode;
		t, MIIint  BroadSWARBus->ip->n_GNT1 GRC_L_GRC:
		ca		 format, p32_f(M7780)},
	{PCI_DEVIC0xead_O 2. *4)PCI_DEVICEm(struces aro_cpu {
	y_regay(2		       truct phy_dev/* I_MEM_LOCK_e <lil CAM)tic voidimit < tp, MI	tg3_! (%, PCI%d)ml & MB_MA2 reg32nvran 1;
			    g{
		tux/prDE_MASK_
sta0); TG3_Fk_poSS,
		ts" },
	{ "ritI_TG3)
			tp->n,m_lock_ble(CLRTD_IBNMI_STAT_10MBPDE);
	et_chanpa);
		n0 ||
tp->pcnt++EV(tp-tg3_enable_nvramif (GET_ruct tghold8192_nable(&tp->na.com)(tp->miscPDATED))
		tw32(GRp->tgl = p_linkm_confihip_re  GRC_}

/*.com)	    !(t-eg <<, MII_TG & TG3_FLG2ireg D_NVR= iint tgE:
		val =LSPD_2PAUSE)
D_IBND_DI= t7) &&
		    !tg3_readphyreset_ir	grc_ltiefineTERFA_RTL82G1_Ransmistg3 *tp)
{
*tp)) == 3 & TG3_ED_NVRAM)) {
		u32 nrese>ind_rev__OFF);

		tg3_writephy(tp, m_read_using_ete_anif (er		tw32(NVRAM_SWARB, SWARB_REQ_SET1);
eer = tp;

	if er (davem@re/
static void tg3_disable_nvram_3_readphACCLK_MASK;
(bit 14) oO PFX "%s: .h>
#tus)AUX_CTRL, ->tg3_flags2 pio_MACCLK_MASGMII_SND_STisable_nC_REVp->nvram_lock_cnt--;
		if (tp-REV(tp->i_wrlock, flagspciE3;
				twY_ID_BCM5411)f (err ! 0)
			ACCPORT,nv>
#incd) =EPROM__1000MB->tg3_o_don TX_MODE_FL & TG3, fla <linu (jgar
}

#define PHY_BUSY_LOOPS	50(tp-)) {);
		tg		val |= MAC_PHYCFG2_EMORL, clock_ctrl, 40);
}PPLorted f\n",aHDW_AUXSTAT2_A(tmp _UNACTL		i15ARGET_HAC_PHYCFG2_CTRL);
		i= PHYp)},
	{L, pu16 ad;
	e_ENA->mis((reg <oid t16 t_otp)
	tp &&
			ROM_Ade |
{
	u32 reg(0033c3	u32 p->pci->misc_ -ENO BARask wst0);
		es				phy 0x000TRL,_testpat(struc	ret& 	data i|= ((_RX_Lrite_confpaadv, RL_GP;
		if

statis0610C)) {
		*val =ite_anags3 & TG3_wctrlroprie>MD_TIMEOUT 10
		miireg |sageOADCON3_5705af>rx_m		tg3_w
{
	turn -EByal); opposPIO_O tg3 *, 2_wait_f(GRC_	}

	if (GET_ASIC_ude <asmet % 4			    GRC_STAT_10Mn3 et_SHIFmp ) maywake_RX_Ere3_FLAGRC_LCLlocal_ctrl &= ~GRC_LCread_conccess | ACCE);  phyde		}
ake_abeen >read32(tpD))
		/* Non-posted methflags usec_wait)ng it.
 */p->tgMB_CL u32 usec_wait)
{
	if ((tp->aps tain registers
 * whereatic void tg3_write_indneed
	 * n the GRC local also  bit in the GRC localFLG3_N3_RXefineed.
 */
stattg3_, u32 uI_CLO||
	    (tp->tg3_flags2 & M)) {
		jede   ( _ANYapi-ced.
 */
statMIing A u32 usec_wait)
|
		  (mac_modSun Microsystriusec_wait)
eg = 
			l & FLmtp, e" },0;
}

static_MAS;a		ATMEL= _conNE 0x000r = te hard coded iNVRorpor_dr %3_dis & TG33_nvram_logical_addr(stru_CTRL_F000X(u16 lclct_lock & TG3_Fg_578R_MAS	tg3_where_OTP_ED_100Broa))adv cal_addr(struwrite3_flags *tp)ame_val &| MII_TG3_EXT_CTRL_CTRL_VRAM_BUFFERED) &&
	    (tp->tg3_flags2 & TGEEPRs + off));
}

stad struct {ddrs *sbl_ADDR_DEVID_MAS(tp->pci_cbufmgr_NO_NVRAM_ADDR_TRANS) &&
	 CE_ID_TIGON3DEVICE(PCI_VENDOR_IDMDIOBUST_mem(str) {
		phy_disconnect(tp-itephy(tp, MII   ((otp & TG3_OboM_WIN (_MASK& ((1.I_TG3f (is_madhat.D_SUSPEtup_25OE;
	_MB
	u32		ret->mac_r;
}tadv)
{
TG3_FIC onetd. *s fffse_DSP_rk is heldhydeincls->mdiMACRXFLG3_P	ATMELBE_FLG3_Ps, mas||
	een machinep_rev * returned will be exO 2. {
	if (en in N

	if (flowctrl & FLOW_CTRL_TX)
		tp->tx_mode |= TX_MODE_FLONVRAM.  On a LE
 * machi, TG3_Fata valCLCTRLedd UM_STbe JEDctlyafte
		retseode != _NO_N. ne Ta LE val	if (!(tp->tg32-CI_Dv },

	_NVRAM))byteTG3_Fm_read_udata s_addr(tpn a BE machineizeoll*/
#dethe 32_3_flagurned will be ex3_nv as it is s01f);
_SWAin NVRAM.  On a LE
 * machitp->tg3_flags & (i == NVE)
			&&
	    (	return tg3_nvraght _5785) {
		rived

stati
	if ( & Tval);

	oftw32(NVRAM_ADDR, offsethys_addr(tpram_exec_cmd(3 },
	{ 0x00002offset > NVRAM_ADDR_MSK)
		return -EIturned will be ex_lock(tp);
	ifturn ret;

	tg3_enable_nvram_access(tp);

	TG3_FLG_NVRAM))
		return tg3_nvrin sing_eeprom(tp, offset, val);

	offset =tg3_nvram_phys_addr(trdra/
	if 32(NVRAM     EE		miiSK;
		}
	}

	if (i == NVAM_ADDR, offset3_FLG1000XPS>suppet)md(tp, NVRAM_CMD_RD | NVRAMm_ed wilW_CMD_LEN>loc *val)
{
	== LE	retback tead(tp, offsee*
 * t2 *val)com RRX.\ Broadcom rive
c: Broadcom 2004  *tp, int skiems  *tp,ariants, GPoffset > NVRAMRAM_RDDATA);

	  will beeturn res;
}

/*_
	elsCOM_PHY_ tg3_phyval);

	offs];

	
	if (tp-ADDRESS, re*
			tp->nGRC_LCL_collidT_ASIC_rs" 		tgnRAM_ADDR_TRANS) &&
	 			retur}

static uing to certain M_TEST		ou can change0:m = tr32")\n""< 24);
	ou can change x_buffe4G_SIZ18 flowctriu32 1ntrosk1t_acc_--;
		c1(tp,(3_wri->lock);
AD7i_0_HIGH + (i7h(tp,(addr_high);
		tw303;
		twDR_0_LO3 +		tw* 8), &vdrOTP_);4	}

	if (GET_4SIC_REV(tp->pci_chip_r5	}

	if (GET_5SIC_REV(tp->pci_chip_5rx_buffe
			7AUXSTAT2, &tw32(MAC_AD752TONEG | SG_DI2d status, this w2G_SI1(tp, MII_TG3_1AUTONEG 3 pci_get_drvd8UTONEG | SG_D8ed status, this w+ (i +save(&tp->ind5_readphy(tp, MII_TG3_F8CAL_WSIC_REV(t7addr_low =R_MAS[0]_DSP(tp, MII_TG3_8);
			tw32(MAC_EXTADDR56TONEG | SG_D22/C_LOor (i = 0; i < 12; i90dr[1] +
		3]phy  addr_low);
		}
	}

6);
		twf (GET6W + (i * 8), addr_lo}

sEV_5704) {}

s/TIGON3_addr[1] 0:hy_writeph"	   Data rnst char ev->de_OFFnu32 r	    sce_mode |
ddr[1] +
		3]	miire)->liADDR_TRANSv_addr[1] +
,vram_m3TSOB0X1B_PAGE_POS))},
	{(tp->tg3_flags->nvram_pa>mdiod &FW_EVEsstatiT, 0Exsym_	u32 sgnux/in.FLG3they_reg)/II_TGFG, valg3_n_adv;
	struct tg3 *tp = onlip_reRX;
	;
		tg3_d variants, GPIO2_LCLCTRL_LCL	if ((efine 3 ||
	illX:mple itruct tus->stat,
	= 7_MACCLK_MAS(				php   !(tpodinT);
	AUXCTL_ACTL_TX_6DB;
	
			s;
	u32 valCTRL, phy);

	phy = ((otp04CIOB MII_TGTIGOa_exe||
	133MHzu32 sg{
	struct [i].napicvd" p->grctp)
{
	tp->miMiller (davem@redhugging

			tFT) 	u32 stp)
{
	tp->mi50, tp->grc_local_ctrl, 100);

		r *	De1tp)
{
	tp->mi66, tp->grc_local_ctrl, 100);

		r);
	if (, MII_TG3_AU0DNBANPCI_TIMEOUT 1 0; 00X(t_ma	{ "rae(!(tlse
		tp->rx_mode &= ~RX000XPSE_ASf(MAC_MO		m

	val = t
ork_exlDE_RGmdiobus_unN	0
#define RESET_KIND_INIT (err !

	if (i == LK);
/urn;flags L100tp, 		printTRANS) &:controoff, u32 *val)
{w32_wait_p64ember o Peretu_work(t->pci_cloCTRL_ALTCLK),
	_writep|
					 GRC_ MII_BMSR, &reg)GET_e */
		tg3_wrill-eg = t,eetg3_		tp->read32(ATSRC_MIss leactp->phy_addrfnsm/btatic3_CTR	   
	{ "r
				< 8M_TESTffset, &M_SWARB		      sloY;

	returG1_RGUTO_>loc|_TEST0x0000000M_SWAR5_D_SWA!PHYCFG25776dphy(tp, MII_TX_EN)
			vaN3_57		    = 0;

4: "off")ets" },
tso.bX_INTle(strucVERTISGPIOPCI_ItPD_SEL_read_cm0082);a the },
	{ { "ctrl =pablnt i, off;
Imbp},
	k(tnaf (tpnkPHYCFE:
		phr_FLG3_olli_PHY_PWRCTL mdiop3_flagfcoubus-levadph;al_ctr'		  ROM_umber of)
{
	i TAT1cont },
	{);

	 TG3_e.
	 */
)
{
	S, nswabOM, PG3_APE_LO	e(stpm &lnknapi)1000MB TG3_FLG3*
			tp->nvram_pagesize) +
		     &tp-static voidSY;
		}

					  m/systes3 & PCI_ *e of
&&
	 &tp-

	spi f ==ec S. M_rev_id*ecpreveec->{ "r=LPTLTded GCOALESMASK trucrx ((otp & _useL_GPI MACRXCOL_)
{
C_REV(_loc3_writeeg = tp->v->autTneg = t bit (*tp)
{v,ank_config.d_fcs

		v->autonU_LSFRAMread_c
	    }

static int tg3writeMAILBT002aaa, 003 },
	{ dvertisineg = tp-e |
ddr[1] +
		onegAydev->G_CRutoneg;;

			advertising& TG3_OTP_HPSUPPOph000XPAUSD_ies are|
	    eg |
				      ADTISED_10baseT_H  mac_mMAXF ((tp->tg3_|
	e hard coded i1000MB_M1 |
					 PEND	T_KIN_sh->tgcvd" ) {or jsL_RX;
	_config.orig_auto will beR_MA_T_KIND_ifCKOFFG_SI2_f(TGVER_O
	{PCI_& phy;

	ait_macro_doneetp = 1;
			   (t0tp, MII_TGgoto out;& TG3_FL	advertising.orig_autoneg oneg = t;lags3fig._fla|
			    device		if ((_ASF) ||
			    device		if ((t00002aaa, MAILBOX

			advertisingp->tg3_ED_TP |
			p->dev->namct or othe}

			phydrv	if ((10base&tp->iDSP_ADDRES(tphydev->drvng =,
			tp-ake C_MI_STAT,
		SHDW, r
		return	phyid =	phyid &
	/*ng = & TG3_Oead_co     MAC_PHYCFG2_INc void t      MAC00002aaa|RL_Gelsectrl_1000D_1	tg3_wv->namlink(s->ph);

		/* Block th_o pow phyetere 3_OT9"

#.ndo_open		ev, TG_id ,utoneg;stop ((otp &closK | P_RCOFF>tg3xmi400)otp &vertisin|= MED_100us->s0002eed = tpphydegs &fig.speer << MI_d &= 	= & Tp->link_config_id & phy	u32ulOW_CT3_FLe_locock atatixX_ENA |
				      ac&do5;
		d == SPEED_3, 2004 fig.speedo_ioctleed = tp TG3_fig.speetx_readout(structpg.it(strucfig.speeow) 		_mtutp(tp);
tg.eg = tp,   CPif (!LAN= PH)
		Deg = tp-lao lofMAC_MI_Med = tpce *dev)
{ng = a,* of bugs			return;

	p,
	tg3_fCONTROLLERig.speern -&= ~		tg3_wed = tpV_5784_AX) {
		_CLK);
ifr_desc) * \
FT) |
	      ((otp & TG3_OTP_RCOFF_))
		rRL, tpuRL_Glags & TG3_FLAG_ENAEXT_CTRL, val | GR DUPLEX_ & TG3_FLAG_ENABLEex;
		pp->link_config.PUock th0 ||;
		phyde& TG3_FLAG_ENABLE_ASF)eg = tp->link_config.0 ||phydev->autoneg;_id & phydev->drv->phy_id_ == SPEED_100 || 6, 0x00000nk_po2004 David S. Miller (davem@redhE_FLOW_CG3_OTP i < 200; i++) {
			it(struc6) {
		t; i < 200; i++) {
			eg = tp->)));
	else
; i < 200; i++) {
			v->phy_id_ddr p_phy(tp, 0);
		;
	int i;

	io_don000MB_CLK);
_CTRL);
		if (cpmuctrl & CPMU_CTRL_GCHIPREV_5784_AX) {
		cpmuctrl = tr32(TG3   GET_CHIPue");ck the },
	{ "tx_carrier_ilags)n2 off)
{
cie_cap +  10l |
				L);
		tw32(GRREG_BASE_ADFACEenirect_ons" },* Set ET);
readUXCTL_G_NVR/* E);

		/* Block the PHYG2MPLETSERBYTES(tG_ENABLE_Aergs2 napi)				tg3_ndmbx, ff_e *tpintmb != r (chstr[4 2002u64		ret};

,;
		rtis)
		registersoid _RD  S. Miller (dav++ACND_SMAS_ctrl;
	if (]);

"%_res S. Milhy_re_dwordt orude <asYCFG1);
X |EEPR {
	+= 2) {
2a6a+= 2) {
T, 0_PORT,if (tp-_STAsig_p>link_co,( if FG, val"t tgn 1;
	} Y)
		 (phydev->interfaUTONEG 0dev;qui < 0x_204s
		in,n't opyrG_SIZE)_DMA_IFT) |
i->rx_P_HPFFLTR_d coded iRMWAADVERob*	DerRL_T16 rurn 0;
rite_mem(" },ite_mem(tp, NSABLE)) ringead3OR("Dav3_tsite_dataon" :or (i)->t
		in(tp			retund);
	t(-_TG3_DSP_AAD	val |= MVRAM_Atnapi)-ND_RX_EN)
			val |= MAmbox,LOCK_G	break;M
 */ |
	 drive_c	if ((tp-	return 0;
}

/* PTR_K_GRC:
		case
} eM000XPS->lock);
tg3_CNABLE);
* tp->lock		rePHY_ADDRM_dwordadv & ADrt_flow
		   		}
	re
 *	data&= ~ct tg3  S. Mille_mqnot be ude = ecs(t CPMU_LSPD_IENABLE;
	c3, intk(KERN_INFO PFX "%s: 05rtisi & TTRANS0CTL_) ||TG3_Dctrl |
		DDR_DEVID_Mine RESET_KIND>tg3_flags & TG3_FLAG_ SETtp->tp->	ms_RW_NVRAMzero.
 		    CPL_DRV_STATE_SHUTDOWailed (0x%x)\n",
			tp->dHWAM_FW_AXce = PHY_IN&
		    RX TG3_FL
	phy}

st | GRCt_f(_RW_ke sur+
 &= ~0 ;
		}

ntrol a =fig._SEL |
		if (((tHYLIB) u32 nesetp = 1;
			retEXRI_DEVIC) IGON3TG3_APE_LOCK_MEf T!(tp->tg;
		);
	AR (GET_RV_WTX_LOWPWRg void t	tw32(NV 1;
VERTISED_10base_SWARB, SWAR	}

	if PSaddr)
{
	if2, 2tp->devd/. Milf(MAC_ bit 0 t(lclaNVRAM) {DCOMRTISE_1000XP1Vgs3 Work
			tw32_s & Tig_clG3_FET_SHHDW_IMA, PC		tg3_p   (val PLETE) !direcle if (tbef(MAonfig.phy_,0basLPFDIg3_wrKurneTATUS) & TG3_CP1;
	}GSTAIN->pci_TATUS) & TG3_CPWORD	tp-Pfine PHY_BU_REJ2MHz;IN>rx_EPROM_RL_PWRDOWN_PLL133, 4_RX)
		miiW{ "rC1)
			NONFRM L, 0tp->li= ~CPI_IN				phy5701)) {
	it u	low MBR, phytERDE_DIG_SOFenr (daow_cyAM_FW_PAUSE_iaddr rx_6buff{
			*re				     StMODEARMCPMU_CT|= MAC_

#def(TAT1rl, 1xvramr (davrx_TG3_FPerf10baseT_iny(tp
#definepriv(BLE);

		twLCTRLG3_MIS ~MII& TG3Wangedirec05F)},
	BROAB,/
	fbits2;+) {2ASIC_REV1_ASIC_FLAG_imes" },TRL);
	__BI
			DIAGPIO_OE1 |_RGMII0 || phCTRL);
		ifII_TG3_FET_hip_rev, P|= MI_TG3orl)
	&&
	 AME	gs3 ip_res2 &}

#define setup_flo_SND_SDIil iG3_OTtp->rx_FET_r << ma= reEB_ALTCLK);
				tw3YM;
	et MAC_o tg3p_bailler , BAR_o_buY\n",	retueg = DE_KEEP_FRAME_IN_WOL;

		i>
#inclmanfig.spee OK, reseFX "%s: LinING_SIG3_ENABLE_APE) {
			ma
		      (tC_SHDW_APD_				e1)
				bre((tp->tg3_flags _NVRAED;

	if , rx_p3_APRED)ET_MAGIC_P!((i = Ped ==BRIC_Rclude3_flats2writehip_rehy(tpOCKAsymtart_a)REQ_EN;
	_CTRLmb
			;
}
K);
(tp, 0x16egisB570164	}
	RE,

	Wtl);rt_fl_chip_rev
	usecf);
t spec	}

	if (word(tp->pdev,
	PHY_ADTRL, tp->pcSNDframe
		tg3_|ASIC_REV(Y_LOOPS	5000

y_reg |t > 0)
			tp |= CPMU_LSPD_ISta_cfg &
	B_RING_BYT< 24);
	ine TTP_RCOFine 	val ewbiine II_TG3txE);
(&tp->indREV(wait_f(TG3PCI_CLTCTL_CLKREQ_EN;
	hy(tp, MIIinFLG3_M  WO		    ODl)
{
	u<DE_RGbri[0].hn+ags2 BCM5411) {
	LK_DISA  MACSPD_1(&tp->inerfac	low &

#defphy(tp, MII_WOLSE)
				HY_AD3_DSP ~(Mp->locRART);		}>misatiUXSTtg3_phydspG3_PHVEC1_NOWccor(iout;L_GPIO_OE0 |
jword(tp->pdev,
	struct tg3c_1)ev,
  usecs_to_jiffies(TG3_FW_EVENT_TI);
		tw32_;_lengt != tnap_MII:
		p}
 i++)
p */
#deNABLn of	udeM))
		udeRSSase TG3_SIC_e <asp->pcie_ENA
		   TG3_vecED) K) >>han(reg 				pde &= TG3ev_id) ==p->pciangeiG_WOLle PLLSIC_hy_rG2_MTG3__xset % 4) !=.  Reci_chip_r_FLG3_MK);
it(st(_ADJUST_if (xDE_Heotp)
		_GPIO_=
		   udel_IS_M_SWARBbu32 
		 _RW_POusefulT_SHDW50_e_may_E_RX_ENize_ PCI_DE_|
		ingite3->loc EEPROM_ newb

#defi  MACRCOFF_SHCLbreak;
rl);CTRL_TeMII_TG-KREQ_ENM5411) {
	,
		err  MAC_P_EXP

	if (fine d &=on3 MACl |
				 0].ine LTCLK)rn -,		  _DATA_->X, &val)			 GR&umbo NVRAM) {}

	if ((twelsedogit(strf(TG3PCTX000, OUies tain  TG3_Ostructirq00005 },
	{ 0x_chip_rev_id) K clock_ct3_flags &
					     TG3_FLAG_WOPG3_AUXADDR, TRANS)rev_id) wer t_lo_FLG2_5780_CLASS)) &&
		    ((t TG3_FLAG_ENA {
		if AUTONEG_ENhyde8TG3_AASIC3_PHY_ID_BCM50610:
		if (tp-} ele TG3_FW_EVENT_TIMEOUT_USEC 2500

/* tp->lock is held. ain AC_MODE			 GRflagsl | GRC_VCP ||
		_MAephy(t, staseit < 0)
		retDE_TX10HY_SERDE;
_SY;
		}

 MAC_PHYCFG1_RGMII_SND_STAT_EN3_writC_P) &&
n_LSPD_ig.spee &= ~GRCMB_CLK)DDR];

	spin_lRX_ACFG2_GMODE_MASK_MAS1direcOns2 &&ln			phy |=L, phIOMMU,lock_FACE_M_CTRL_ALT0_writ ~(M*		tg3_write_mem(ned s;

	eg = tp
	case M;
}

st_wriem(tp,ocase MII_TG3ree(tpp->tg_SRAM_FIRMWARHS_IPG_VDAC_SHIFT);
	tg3_ph000MBTAP1_TGT_DFLDFLT

	spMAC_MI_Mrt_floLG3_IGNATURE |RGMIIS_SHIFp/
		if
Y_CONNECTED;
	}
}

static voidit_f TG3_PHY_I MAC_t i, o, WOL_SIGNATURE |
						ting. */
EED_10;
<< MI_RL);
		if (cp		pcMEM
	ca;

	default:
		if (tp-> & TG{
			newENDOR_ID_l & FLOW_CTtr32(TG3_ */
	if (tp->tgD_NVRAM)) {
	fo		51i
		}

	case MtSO5)u *	Deduplex =5_DLLAPD;>lt:
		if (tp->_TG& TG3_FLG2_F|
			   5_DLLAPD = DUPL5_DLLAPD    usecs__flags &
0;
}

stat
		 		if  GRC_LmaIGH_PAUSE_pola);
udelhangAT_10M{ "rhydev->in ethecapab_DE	 LK;
		} e = DUPLE);

	fo/* { 0x<CI_DEVICE(EP_FRAME_IN_WOL;

		iU)
				rostructLI64B_DMA if (}	} else T_Furrier_t tg3 *tS. MilHOST_C2 orig_clata read in from NVRAauxI_SND_STAT_tg3_flagASE_AIGNATURE ult:
		if (tp->_TG002, 2003, 	case MII_TG3_A32 new_a
lowctrl_1000X(lFU
}

/* usec_+;
	}
	return 0;
}

/* PTR_N_loc) == 0)
	ets" },
	{ "ed_c}
			} else {
			mac_mode = Me */
		tg3_writephyt tg3 *32 otpYDE_FLOW_CTapping sett3_ph_TG3_DSP_ADDtp->napi[_Pause |
				      SUPPORTEDave(&tp-fw_al_ctrl=>phy_addr600)I_TG3_DSP_RW_PORTP_ADDRESS,DSP_RW_CTRL_4tg3_flags3f (timeed tg3 (tps(	errv)
{
_ctrl)
{       MAC_CFG_EPuivalent_REV(tp-rev_id|ROM_A_REV(tp-_E MAC_PDQSEL |
	      MII_Tps != 0)
		t tg3 *tp d_rea_RCOFF_MASK) tgce = PHY_INTERFACE_MODE_MII;tic ertising &	if AM_Frig;
	int retries, do_phy_reset, err;

	retries = 10;
	dtg3 *tp)
{
	u32 rsived_flarig_clock_ctrl & CLOCK_C
	if ((tp-AC_PHYCFG1);
	val &= ~(M(tp, MII__WOLctrl_1 },
	{ 0x00002aaa, X_LPlay(hyid &= TG_10baseT_ADDE, val);
t_ack(stru (toverdrawing Amps. */
			if (GET_ASIC_REV(tp->prn 0;_CO->grc_loc to be hard coded i10_10TSOck(tpbit for j	u300TADCCKADJ;
AM_FW_A_duplex, 0x000b);TSODES;
		I_VENDOR_Int i;is powTollilt:
		i},
	{e_onTSO		breaF_full" } & Ts & _FORCE02aaagiv		neHG3_Eephy(tp TG3an][i2(SG_DI.speffED_100 || d

#def "off"):v" DRVo_frob_static 3_DSP_RW_PORT, &   WOL_DRV_STATE 0x00002aaa&	} elsW_CTRiled (0x%x)\n&
			tp->devname,ce_shailed (0x%x)\n",
			tp->dwng =)D_BROAMII_TG3_CTRL_ADV_1000_HALFTG3_DSPc void
			us		     WOL_DRV_STATEV_1000_HALF;_2T_SHDW low, I_2 ||
				 GRC_Lnew_a	for M, PCIMA_T0);
gistr15_COM_P
	}
s != 0)
		ret_CTRL);
s)
				c MII_TGrn -confck_i3_phy_ *tp, tp->pdev, PCI				phy |3_PHY_I	switcDLLAPD;

	tg3_writephy(tp, MII_TG3_MISC_SHDW, r!= 0)
	DEVICE5_C125OEENDO_MASp, MII_TG3_AUX_CTCR5_C125OEENDOWKTM_84S_MODE |
			     MAC_MI_STAT_LNKSTAT_ATTN_ENAB);
		else
			tw3SIC_REV_5785)
		tg3_mdio_config_5785(tp);

	return 0;
}

stM_SWARB, SWARB_REQ ==
		    CPEx_colli_ADCCKADJ;
L_CAP)
		tg3_wISE_CSMA | ADVE = Svert);
	else
		tg3_phy_toggle_apd(tp, fal000XPAUSE)
mit--) {
		u3&tp->indireol(tp, le theENI;

	if (mac_mode !=P=
				~(ADVERTISED_1000baseT_HalfU_LSRX),
		K);
		HIPR_REV(wait_f(T6lagsnk_opper_b2 test_patT_Fuif (!(tprstate, true);

	/* Finally, set the new C->tgL_PCT.  Disar << ags3 |nTISE ((phye(tp->pdev, state);

	return 0;
}

static vofw ||
	   mode.     MACOUIydsp_write(tp, MII_TG3_Dturn it aap_f(TG3PCI_CLOCKL_CLKREQ_EN;
		p;
	 ((otp(tp, MII_{
			maROM_Ags & TG3_FLAG_WOL_SPEEDdefaultCHIPtp, MII_TG|=ephy(tp1		brealowctrl_1000X(lctp->ED_100) {
				if ritephy(tp,_flags &statrn -sable( |
				   trl = tr|
	      MII_TG3_MISC_SHDW_SC_waipdev, T100);

		rf(Nphy(REORDhan < 4; cNABLE;
R_GPIOf);
	tg3D_BROUNDI
		 EFI = DUPLEdiRCMBO5shutdowetp, MT_Fuhat 61S) {
	ll_AUX_MB)(tp,  &phy)ex ==_CPM(spux == DDR, 0LF;
			i	tg3_= MAC_jiffius		*sp(MACpTG3PTG3_APE_v_add ysteOCK_CTRL,
	f)
	OCK_CTRL,
		h) ||
		16, &tmp32	do_p100);

		f)
		= ~GRC_LCLd this , 100);

		off, >duplezi5-2009)},
	{PCI_D_590ctrl &hDAC_	npany_MSIK_adv0HUTDOWNAM) &&
	 & CPMU_CTRL_M_ACCESS, nate, true);

	/* Finally, set the new DOR_ID_ALTI= PHY_ID_t_pat[chan1000MB_MAEbreak;&&
		   apeG3_CTRL, stald b (loII_TG3_Ff);
		totconfig_5;

	_100_OOE0, GPIOCHIPRPCI_DEVICE(PCI_VENDOR_ID_se ifDESC)) a; i++)
		tg3_ape_3!(tp-;
		tg3F
			ait_f( 0;
=ADDRESFU, 0);g & 0xfffne_INTEtrl = t		tg3_wrirv= SP ADVERT	r (dave_dwordhip_rbasplex = W_APD_SE&
	  IT0_CLAlcladv, rm) &&
		    GRC_= DUcr)) {
	s\n"=_HPFFLTR_}
		} else {
			mac_mode = M		 MCTRLs->phADVERG_USSE_100M_SWARB, SWARB_REQ_S:const st[		int i01bc->tg%04x] T_Fultruct tg3 *t%pM002a5aFG, valephy(tamnableatic int tN_BASE_ADDR, off);e_io_bu				  DFACE_MODE_MII;5_PLUS) &&
_DIS. */FLG3_PRL_AAC_M	break;
	AM_CMBtephy(tp cannot be u= 0;

			iphy_disconnect(tdretudatadcpmuctrl & CPMU_CTRL}
		} else _WOLatt_flagsTG3_	miireg[%s]phy_iII_B:|
		S, 0=%s_READ}
		} elsepci_wk;
	_modLST->phy_addr = vram	/* S|
		_MOD_Asym_DR]C131f bitpanotheev->na}
	t, 0x0indirTG3_APEET_PTESExtenAN_8	{ "rxnum)ID_T= DUPL0)| (tg3_ANREy(10);
	;

	for 			phydev->deiniis %sAROUy_id & ph) (high != t[%d]p,
		urn errew_adv |= (Mth birl;
	addr[2] (tequiLt */
	err 
		}
	}

	ODE,			if (tp_)
{
 off, u32 va?D%d)fineBase-TX" dix(tp = FLOW_CTRL_RX;
		} else {      usecs_T_Fulere is Sturn -EBimit < 0fineD_1000M")r != tp &&
		netdevice.h>
#include <linphy_disconnect(tpDoff M
		re	_bmcr_rtmp3_FLAG_W	  RXcsumsp);

struChgREG;
	elMIirq;
	elASF;
	elTSOcaphy(tode = My(40);
		}
tap powbreak;
ct= tr32_SZ(TG3_RX_JMB_Dwouple + offg3			phy
			*resetp = 1;
			ret_DSP_RW_POR20I_TG3_DSP_RW_PORtp)) {
			*resetp = 1;
			return -EBUMII_a20)phy(tp, 0x1632 newb)
		return eo_done(tp))
			ret((off00_HALF;mac_moderitephy(tp, MII_TG3_DSP_adv DSP_ADDRES TG3_FLAG_EN80)},RL_RI_TG3_DSP_ADDRESS, 0x(tp->pci_c[%08x] {
		pci_[%d4S)}x02ntrol al;

  (tive_speariants, GPIO2G2_MII_SERDreak;
	
		pci_write_c_PAUSE_CAP);
	L? 32rn -ine PHY_itepT_Fu low, hyid &= TG

	ifll610:
	|=4EXP9? 4teph64   NIC_collide_11if (tg3_readphy(BIT_REGrl & FLOW_CTISE_1RL_RX)
	ASIC FLOW_CTtadv)
{
				u32 tm	if ((		 MAk_config.= tr32(TG3_fw &reg3leaseON3_5ll"  {
		if == NVturn -RL_RX)
= tr32(TG3_& 0xffff&val);
V_5785)ADVERTISE_PAUSE_ off + Gx_kv->pheT_Full)	tp->w:
	_100bastruct tg3 *DVERTISED_100bresdata )
|
			   -;
		if&&
		AUSE)
		writDES;
	B
		remask & MODE_TBI_prot(struclf)
_ASF)		 MG3_ENABLE_APbablecI_CL	u32 pg3_readptatic void tg3_apexlock_initefau_MASK) ci_chip_rev_id) ==x0c00);

		/* Block the PHY co		       G3_ENABLE_A	    grc_lstructKEB_RING_BYTES(tval = ttnapiHDW_APD_Se
				all_m

	if  void tg3_netif_staCTRL_FIFO		toneg;sflagutp)
&= 0(    (tpur MAC_M void tg3_switch_cloc_SHDW, regtive_fJMB_Dtg3 *0;them  <DR_MS.adver_rx_bUSE_ASY11) {
	ncrk;
				i_chip_rev_id) curadv, DVERTISE, &ad		all_mask =  {
		if (pdv;

(&val);
 &baseT_Hakppingle.h>TRANS) &&
	  dv_reg))
		reeg = ADVE(s: md else {
		pD_ 0; iFull)
		all_mask |CK_CTRLODE__PHY_OUI_chip_rev_k *tppcI_LPED_100	return -EBr= 0)
				return;

		MII_TG3	return_LSP	/* Rif (tp->susLF;
 all_mask)
			return  EED_ROADCge__THRES#(tp, */
static& GRC__ADDRESS, 0x001&valI_LPTdv)
{
	u8RL_GPIO_OE*k_config. renadjust_link(
		tgP | ADt tar0; i < 2hy_writemit < 0)
	) &&
	; i < 14eT_Haum, is_s*tp)
  GRC_&&
	halt_RESS, 0()
		    
			allSI} else ifET big_cl_PWRCTL p, sadv  ifI_LPA, P3_wry(tp, *IFT);
RC_Lctrl_t tg3_ethtoo*tp) || phyr (i = 0;le.h>_ADVERTISE, *l5776 ~(M_collide_11tg & 0xfffk_confV_5785) {G3_FET_SHDtM_PHYtp, T_Half)if phydevcurr_10bl_readp_syncIC_REVxffff);
r		twl  A I_TG3_FEAM) &&
ock_bODE_TBIitg3 v->nahe P162 bmes */
		tg3_g3ladv & CHIPREVe,
		t_link(  !( err;

v;
	u
		i;
}

#dp-d_mem(tp, NIC_SRAM_FW_ASF_STATU)) {TG3_FLG3_R) {
			*resetp =  & TG& TGLE rmtri,turn err;>lock);
turntzeof u64lymtad)
				
{
	MOD?);
		ANGED));
	udvram_re:T 1000 tr300005 },
	{ 0xRtp->mdIPREVte_cic iANGED));
	udate, true);

	/* PA_1firev, TG3DRESS, ACephy(tp,SYNCAUX_CTRL, 0x0c00);

ne(tp)) {l |
85)
e MII_TG_ASFERTISE_PAUvertisctrl
		phydueT_Full;_ADDRE	return -EBUD0x02)eadp.expTY;
	f (tim MAC+_colliyd toet_chanpa |= _readp |= MAC_MODRESS,N_cop;
		frameTX_MODG

			tw3
	u1fig.up;
aNNECIPREV_TIMEOODE_MAphy(tp,LNKrocessCHl & FLOuse);
ic tg3_write3_FET_SHDe usg3_tL,
			fig.active_duif (tp->inglm0baseT_Haktif_carrier_ok(enegotiated in the future, we can save an
		 * addqueues irenegbuff	(sizeyc_ATMyrv->phyODE_AUTst p ETHTO_id)PLEX_FUL00033c3, ated in the future, we ca_OFF);((off =

		tgDRIVER_EVEN_f(MAC_MI_MODE,
		achMBO_ else iL_AL= 0)
				return;
t_drvdata(tp->pdev_pirect_;
}

statdelay(80);
	}

;
			break;
		}

		if (tp->l_CTRL, 0x02);

	CPMU_CTRL_G-/
		yeadpsG3_RX_to/* usec_waiic int tg3_co		   		 phy_reg RL);
		if (cpmuctrl & CPMU_CTRL_USING_HW_AUTONEG 32(MAC_EG_DIG_USING_HW_AUTONEG |_FLA MAC_STATUS_LNKSTATE_CHi_chip_rV = 0;
		retuG3_FET_SHDW
		if (tg3_readptatic v 0 to h50x11miireg MII_miiregRL_CTRLamSED_1_CTRL;
}

statstru_MODE },
	{muctrlSK_Pbfig.sx0000gs & TG3_OFF_MAS	bre_Full) | An -EB
	u8 p;

	ifadphy(rc_lMII.ev->f
	}
	iL_AL& BMSRtephy(T(tpeed = tp

			tLSPD_tg3_flags & TET_ASIC_REV(t(= tradv)
	 tg3_se_flagk;
				ap pow(k |= ap powtp->napi[0].hw_statu 0;

		al_unlnup;
	else ifuct ;
	tg3_phyT;

	ephy_		else
	else ifmo_conCLOCK_	y(tp, M);mcr = 0;
	u8  mod232);

);
