/* niu.c: Neptune ethernet driver.
 *
 * Copyright (C) 2007, 2008 David S. Miller (davem@davemloft.net)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/mii.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/ipv6.h>
#include <linux/log2.h>
#include <linux/jiffies.h>
#include <linux/crc32.h>
#include <linux/list.h>

#include <linux/io.h>

#ifdef CONFIG_SPARC64
#include <linux/of_device.h>
#endif

#include "niu.h"

#define DRV_MODULE_NAME		"niu"
#define PFX DRV_MODULE_NAME	": "
#define DRV_MODULE_VERSION	"1.0"
#define DRV_MODULE_RELDATE	"Nov 14, 2008"

static char version[] __devinitdata =
	DRV_MODULE_NAME ".c:v" DRV_MODULE_VERSION " (" DRV_MODULE_RELDATE ")\n";

MODULE_AUTHOR("David S. Miller (davem@davemloft.net)");
MODULE_DESCRIPTION("NIU ethernet driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MODULE_VERSION);

#ifndef DMA_44BIT_MASK
#define DMA_44BIT_MASK	0x00000fffffffffffULL
#endif

#ifndef readq
static u64 readq(void __iomem *reg)
{
	return ((u64) readl(reg)) | (((u64) readl(reg + 4UL)) << 32);
}

static void writeq(u64 val, void __iomem *reg)
{
	writel(val & 0xffffffff, reg);
	writel(val >> 32, reg + 0x4UL);
}
#endif

static struct pci_device_id niu_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_SUN, 0xabcd)},
	{}
};

MODULE_DEVICE_TABLE(pci, niu_pci_tbl);

#define NIU_TX_TIMEOUT			(5 * HZ)

#define nr64(reg)		readq(np->regs + (reg))
#define nw64(reg, val)		writeq((val), np->regs + (reg))

#define nr64_mac(reg)		readq(np->mac_regs + (reg))
#define nw64_mac(reg, val)	writeq((val), np->mac_regs + (reg))

#define nr64_ipp(reg)		readq(np->regs + np->ipp_off + (reg))
#define nw64_ipp(reg, val)	writeq((val), np->regs + np->ipp_off + (reg))

#define nr64_pcs(reg)		readq(np->regs + np->pcs_off + (reg))
#define nw64_pcs(reg, val)	writeq((val), np->regs + np->pcs_off + (reg))

#define nr64_xpcs(reg)		readq(np->regs + np->xpcs_off + (reg))
#define nw64_xpcs(reg, val)	writeq((val), np->regs + np->xpcs_off + (reg))

#define NIU_MSG_DEFAULT (NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK)

static int niu_debug;
static int debug = -1;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "NIU debug level");

#define niudbg(TYPE, f, a...) \
do {	if ((np)->msg_enable & NETIF_MSG_##TYPE) \
		printk(KERN_DEBUG PFX f, ## a); \
} while (0)

#define niuinfo(TYPE, f, a...) \
do {	if ((np)->msg_enable & NETIF_MSG_##TYPE) \
		printk(KERN_INFO PFX f, ## a); \
} while (0)

#define niuwarn(TYPE, f, a...) \
do {	if ((np)->msg_enable & NETIF_MSG_##TYPE) \
		printk(KERN_WARNING PFX f, ## a); \
} while (0)

#define niu_lock_parent(np, flags) \
	spin_lock_irqsave(&np->parent->lock, flags)
#define niu_unlock_parent(np, flags) \
	spin_unlock_irqrestore(&np->parent->lock, flags)

static int serdes_init_10g_serdes(struct niu *np);

static int __niu_wait_bits_clear_mac(struct niu *np, unsigned long reg,
				     u64 bits, int limit, int delay)
{
	while (--limit >= 0) {
		u64 val = nr64_mac(reg);

		if (!(val & bits))
			break;
		udelay(delay);
	}
	if (limit < 0)
		return -ENODEV;
	return 0;
}

static int __niu_set_and_wait_clear_mac(struct niu *np, unsigned long reg,
					u64 bits, int limit, int delay,
					const char *reg_name)
{
	int err;

	nw64_mac(reg, bits);
	err = __niu_wait_bits_clear_mac(np, reg, bits, limit, delay);
	if (err)
		dev_err(np->device, PFX "%s: bits (%llx) of register %s "
			"would not clear, val[%llx]\n",
			np->dev->name, (unsigned long long) bits, reg_name,
			(unsigned long long) nr64_mac(reg));
	return err;
}

#define niu_set_and_wait_clear_mac(NP, REG, BITS, LIMIT, DELAY, REG_NAME) \
({	BUILD_BUG_ON(LIMIT <= 0 || DELAY < 0); \
	__niu_set_and_wait_clear_mac(NP, REG, BITS, LIMIT, DELAY, REG_NAME); \
})

static int __niu_wait_bits_clear_ipp(struct niu *np, unsigned long reg,
				     u64 bits, int limit, int delay)
{
	while (--limit >= 0) {
		u64 val = nr64_ipp(reg);

		if (!(val & bits))
			break;
		udelay(delay);
	}
	if (limit < 0)
		return -ENODEV;
	return 0;
}

static int __niu_set_and_wait_clear_ipp(struct niu *np, unsigned long reg,
					u64 bits, int limit, int delay,
					const char *reg_name)
{
	int err;
	u64 val;

	val = nr64_ipp(reg);
	val |= bits;
	nw64_ipp(reg, val);

	err = __niu_wait_bits_clear_ipp(np, reg, bits, limit, delay);
	if (err)
		dev_err(np->device, PFX "%s: bits (%llx) of register %s "
			"would not clear, val[%llx]\n",
			np->dev->name, (unsigned long long) bits, reg_name,
			(unsigned long long) nr64_ipp(reg));
	return err;
}

#define niu_set_and_wait_clear_ipp(NP, REG, BITS, LIMIT, DELAY, REG_NAME) \
({	BUILD_BUG_ON(LIMIT <= 0 || DELAY < 0); \
	__niu_set_and_wait_clear_ipp(NP, REG, BITS, LIMIT, DELAY, REG_NAME); \
})

static int __niu_wait_bits_clear(struct niu *np, unsigned long reg,
				 u64 bits, int limit, int delay)
{
	while (--limit >= 0) {
		u64 val = nr64(reg);

		if (!(val & bits))
			break;
		udelay(delay);
	}
	if (limit < 0)
		return -ENODEV;
	return 0;
}

#define niu_wait_bits_clear(NP, REG, BITS, LIMIT, DELAY) \
({	BUILD_BUG_ON(LIMIT <= 0 || DELAY < 0); \
	__niu_wait_bits_clear(NP, REG, BITS, LIMIT, DELAY); \
})

static int __niu_set_and_wait_clear(struct niu *np, unsigned long reg,
				    u64 bits, int limit, int delay,
				    const char *reg_name)
{
	int err;

	nw64(reg, bits);
	err = __niu_wait_bits_clear(np, reg, bits, limit, delay);
	if (err)
		dev_err(np->device, PFX "%s: bits (%llx) of register %s "
			"would not clear, val[%llx]\n",
			np->dev->name, (unsigned long long) bits, reg_name,
			(unsigned long long) nr64(reg));
	return err;
}

#define niu_set_and_wait_clear(NP, REG, BITS, LIMIT, DELAY, REG_NAME) \
({	BUILD_BUG_ON(LIMIT <= 0 || DELAY < 0); \
	__niu_set_and_wait_clear(NP, REG, BITS, LIMIT, DELAY, REG_NAME); \
})

static void niu_ldg_rearm(struct niu *np, struct niu_ldg *lp, int on)
{
	u64 val = (u64) lp->timer;

	if (on)
		val |= LDG_IMGMT_ARM;

	nw64(LDG_IMGMT(lp->ldg_num), val);
}

static int niu_ldn_irq_enable(struct niu *np, int ldn, int on)
{
	unsigned long mask_reg, bits;
	u64 val;

	if (ldn < 0 || ldn > LDN_MAX)
		return -EINVAL;

	if (ldn < 64) {
		mask_reg = LD_IM0(ldn);
		bits = LD_IM0_MASK;
	} else {
		mask_reg = LD_IM1(ldn - 64);
		bits = LD_IM1_MASK;
	}

	val = nr64(mask_reg);
	if (on)
		val &= ~bits;
	else
		val |= bits;
	nw64(mask_reg, val);

	return 0;
}

static int niu_enable_ldn_in_ldg(struct niu *np, struct niu_ldg *lp, int on)
{
	struct niu_parent *parent = np->parent;
	int i;

	for (i = 0; i <= LDN_MAX; i++) {
		int err;

		if (parent->ldg_map[i] != lp->ldg_num)
			continue;

		err = niu_ldn_irq_enable(np, i, on);
		if (err)
			return err;
	}
	return 0;
}

static int niu_enable_interrupts(struct niu *np, int on)
{
	int i;

	for (i = 0; i < np->num_ldg; i++) {
		struct niu_ldg *lp = &np->ldg[i];
		int err;

		err = niu_enable_ldn_in_ldg(np, lp, on);
		if (err)
			return err;
	}
	for (i = 0; i < np->num_ldg; i++)
		niu_ldg_rearm(np, &np->ldg[i], on);

	return 0;
}

static u32 phy_encode(u32 type, int port)
{
	return (type << (port * 2));
}

static u32 phy_decode(u32 val, int port)
{
	return (val >> (port * 2)) & PORT_TYPE_MASK;
}

static int mdio_wait(struct niu *np)
{
	int limit = 1000;
	u64 val;

	while (--limit > 0) {
		val = nr64(MIF_FRAME_OUTPUT);
		if ((val >> MIF_FRAME_OUTPUT_TA_SHIFT) & 0x1)
			return val & MIF_FRAME_OUTPUT_DATA;

		udelay(10);
	}

	return -ENODEV;
}

static int mdio_read(struct niu *np, int port, int dev, int reg)
{
	int err;

	nw64(MIF_FRAME_OUTPUT, MDIO_ADDR_OP(port, dev, reg));
	err = mdio_wait(np);
	if (err < 0)
		return err;

	nw64(MIF_FRAME_OUTPUT, MDIO_READ_OP(port, dev));
	return mdio_wait(np);
}

static int mdio_write(struct niu *np, int port, int dev, int reg, int data)
{
	int err;

	nw64(MIF_FRAME_OUTPUT, MDIO_ADDR_OP(port, dev, reg));
	err = mdio_wait(np);
	if (err < 0)
		return err;

	nw64(MIF_FRAME_OUTPUT, MDIO_WRITE_OP(port, dev, data));
	err = mdio_wait(np);
	if (err < 0)
		return err;

	return 0;
}

static int mii_read(struct niu *np, int port, int reg)
{
	nw64(MIF_FRAME_OUTPUT, MII_READ_OP(port, reg));
	return mdio_wait(np);
}

static int mii_write(struct niu *np, int port, int reg, int data)
{
	int err;

	nw64(MIF_FRAME_OUTPUT, MII_WRITE_OP(port, reg, data));
	err = mdio_wait(np);
	if (err < 0)
		return err;

	return 0;
}

static int esr2_set_tx_cfg(struct niu *np, unsigned long channel, u32 val)
{
	int err;

	err = mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
			 ESR2_TI_PLL_TX_CFG_L(channel),
			 val & 0xffff);
	if (!err)
		err = mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
				 ESR2_TI_PLL_TX_CFG_H(channel),
				 val >> 16);
	return err;
}

static int esr2_set_rx_cfg(struct niu *np, unsigned long channel, u32 val)
{
	int err;

	err = mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
			 ESR2_TI_PLL_RX_CFG_L(channel),
			 val & 0xffff);
	if (!err)
		err = mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
				 ESR2_TI_PLL_RX_CFG_H(channel),
				 val >> 16);
	return err;
}

/* Mode is always 10G fiber.  */
static int serdes_init_niu_10g_fiber(struct niu *np)
{
	struct niu_link_config *lp = &np->link_config;
	u32 tx_cfg, rx_cfg;
	unsigned long i;

	tx_cfg = (PLL_TX_CFG_ENTX | PLL_TX_CFG_SWING_1375MV);
	rx_cfg = (PLL_RX_CFG_ENRX | PLL_RX_CFG_TERM_0P8VDDT |
		  PLL_RX_CFG_ALIGN_ENA | PLL_RX_CFG_LOS_LTHRESH |
		  PLL_RX_CFG_EQ_LP_ADAPTIVE);

	if (lp->loopback_mode == LOOPBACK_PHY) {
		u16 test_cfg = PLL_TEST_CFG_LOOPBACK_CML_DIS;

		mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
			   ESR2_TI_PLL_TEST_CFG_L, test_cfg);

		tx_cfg |= PLL_TX_CFG_ENTEST;
		rx_cfg |= PLL_RX_CFG_ENTEST;
	}

	/* Initialize all 4 lanes of the SERDES.  */
	for (i = 0; i < 4; i++) {
		int err = esr2_set_tx_cfg(np, i, tx_cfg);
		if (err)
			return err;
	}

	for (i = 0; i < 4; i++) {
		int err = esr2_set_rx_cfg(np, i, rx_cfg);
		if (err)
			return err;
	}

	return 0;
}

static int serdes_init_niu_1g_serdes(struct niu *np)
{
	struct niu_link_config *lp = &np->link_config;
	u16 pll_cfg, pll_sts;
	int max_retry = 100;
	u64 uninitialized_var(sig), mask, val;
	u32 tx_cfg, rx_cfg;
	unsigned long i;
	int err;

	tx_cfg = (PLL_TX_CFG_ENTX | PLL_TX_CFG_SWING_1375MV |
		  PLL_TX_CFG_RATE_HALF);
	rx_cfg = (PLL_RX_CFG_ENRX | PLL_RX_CFG_TERM_0P8VDDT |
		  PLL_RX_CFG_ALIGN_ENA | PLL_RX_CFG_LOS_LTHRESH |
		  PLL_RX_CFG_RATE_HALF);

	if (np->port == 0)
		rx_cfg |= PLL_RX_CFG_EQ_LP_ADAPTIVE;

	if (lp->loopback_mode == LOOPBACK_PHY) {
		u16 test_cfg = PLL_TEST_CFG_LOOPBACK_CML_DIS;

		mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
			   ESR2_TI_PLL_TEST_CFG_L, test_cfg);

		tx_cfg |= PLL_TX_CFG_ENTEST;
		rx_cfg |= PLL_RX_CFG_ENTEST;
	}

	/* Initialize PLL for 1G */
	pll_cfg = (PLL_CFG_ENPLL | PLL_CFG_MPY_8X);

	err = mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
			 ESR2_TI_PLL_CFG_L, pll_cfg);
	if (err) {
		dev_err(np->device, PFX "NIU Port %d "
			"serdes_init_niu_1g_serdes: "
			"mdio write to ESR2_TI_PLL_CFG_L failed", np->port);
		return err;
	}

	pll_sts = PLL_CFG_ENPLL;

	err = mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
			 ESR2_TI_PLL_STS_L, pll_sts);
	if (err) {
		dev_err(np->device, PFX "NIU Port %d "
			"serdes_init_niu_1g_serdes: "
			"mdio write to ESR2_TI_PLL_STS_L failed", np->port);
		return err;
	}

	udelay(200);

	/* Initialize all 4 lanes of the SERDES.  */
	for (i = 0; i < 4; i++) {
		err = esr2_set_tx_cfg(np, i, tx_cfg);
		if (err)
			return err;
	}

	for (i = 0; i < 4; i++) {
		err = esr2_set_rx_cfg(np, i, rx_cfg);
		if (err)
			return err;
	}

	switch (np->port) {
	case 0:
		val = (ESR_INT_SRDY0_P0 | ESR_INT_DET0_P0);
		mask = val;
		break;

	case 1:
		val = (ESR_INT_SRDY0_P1 | ESR_INT_DET0_P1);
		mask = val;
		break;

	default:
		return -EINVAL;
	}

	while (max_retry--) {
		sig = nr64(ESR_INT_SIGNALS);
		if ((sig & mask) == val)
			break;

		mdelay(500);
	}

	if ((sig & mask) != val) {
		dev_err(np->device, PFX "Port %u signal bits [%08x] are not "
			"[%08x]\n", np->port, (int) (sig & mask), (int) val);
		return -ENODEV;
	}

	return 0;
}

static int serdes_init_niu_10g_serdes(struct niu *np)
{
	struct niu_link_config *lp = &np->link_config;
	u32 tx_cfg, rx_cfg, pll_cfg, pll_sts;
	int max_retry = 100;
	u64 uninitialized_var(sig), mask, val;
	unsigned long i;
	int err;

	tx_cfg = (PLL_TX_CFG_ENTX | PLL_TX_CFG_SWING_1375MV);
	rx_cfg = (PLL_RX_CFG_ENRX | PLL_RX_CFG_TERM_0P8VDDT |
		  PLL_RX_CFG_ALIGN_ENA | PLL_RX_CFG_LOS_LTHRESH |
		  PLL_RX_CFG_EQ_LP_ADAPTIVE);

	if (lp->loopback_mode == LOOPBACK_PHY) {
		u16 test_cfg = PLL_TEST_CFG_LOOPBACK_CML_DIS;

		mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
			   ESR2_TI_PLL_TEST_CFG_L, test_cfg);

		tx_cfg |= PLL_TX_CFG_ENTEST;
		rx_cfg |= PLL_RX_CFG_ENTEST;
	}

	/* Initialize PLL for 10G */
	pll_cfg = (PLL_CFG_ENPLL | PLL_CFG_MPY_10X);

	err = mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
			 ESR2_TI_PLL_CFG_L, pll_cfg & 0xffff);
	if (err) {
		dev_err(np->device, PFX "NIU Port %d "
			"serdes_init_niu_10g_serdes: "
			"mdio write to ESR2_TI_PLL_CFG_L failed", np->port);
		return err;
	}

	pll_sts = PLL_CFG_ENPLL;

	err = mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
			 ESR2_TI_PLL_STS_L, pll_sts & 0xffff);
	if (err) {
		dev_err(np->device, PFX "NIU Port %d "
			"serdes_init_niu_10g_serdes: "
			"mdio write to ESR2_TI_PLL_STS_L failed", np->port);
		return err;
	}

	udelay(200);

	/* Initialize all 4 lanes of the SERDES.  */
	for (i = 0; i < 4; i++) {
		err = esr2_set_tx_cfg(np, i, tx_cfg);
		if (err)
			return err;
	}

	for (i = 0; i < 4; i++) {
		err = esr2_set_rx_cfg(np, i, rx_cfg);
		if (err)
			return err;
	}

	/* check if serdes is ready */

	switch (np->port) {
	case 0:
		mask = ESR_INT_SIGNALS_P0_BITS;
		val = (ESR_INT_SRDY0_P0 |
		       ESR_INT_DET0_P0 |
		       ESR_INT_XSRDY_P0 |
		       ESR_INT_XDP_P0_CH3 |
		       ESR_INT_XDP_P0_CH2 |
		       ESR_INT_XDP_P0_CH1 |
		       ESR_INT_XDP_P0_CH0);
		break;

	case 1:
		mask = ESR_INT_SIGNALS_P1_BITS;
		val = (ESR_INT_SRDY0_P1 |
		       ESR_INT_DET0_P1 |
		       ESR_INT_XSRDY_P1 |
		       ESR_INT_XDP_P1_CH3 |
		       ESR_INT_XDP_P1_CH2 |
		       ESR_INT_XDP_P1_CH1 |
		       ESR_INT_XDP_P1_CH0);
		break;

	default:
		return -EINVAL;
	}

	while (max_retry--) {
		sig = nr64(ESR_INT_SIGNALS);
		if ((sig & mask) == val)
			break;

		mdelay(500);
	}

	if ((sig & mask) != val) {
		pr_info(PFX "NIU Port %u signal bits [%08x] are not "
			"[%08x] for 10G...trying 1G\n",
			np->port, (int) (sig & mask), (int) val);

		/* 10G failed, try initializing at 1G */
		err = serdes_init_niu_1g_serdes(np);
		if (!err) {
			np->flags &= ~NIU_FLAGS_10G;
			np->mac_xcvr = MAC_XCVR_PCS;
		}  else {
			dev_err(np->device, PFX "Port %u 10G/1G SERDES "
				"Link Failed \n", np->port);
			return -ENODEV;
		}
	}
	return 0;
}

static int esr_read_rxtx_ctrl(struct niu *np, unsigned long chan, u32 *val)
{
	int err;

	err = mdio_read(np, np->port, NIU_ESR_DEV_ADDR, ESR_RXTX_CTRL_L(chan));
	if (err >= 0) {
		*val = (err & 0xffff);
		err = mdio_read(np, np->port, NIU_ESR_DEV_ADDR,
				ESR_RXTX_CTRL_H(chan));
		if (err >= 0)
			*val |= ((err & 0xffff) << 16);
		err = 0;
	}
	return err;
}

static int esr_read_glue0(struct niu *np, unsigned long chan, u32 *val)
{
	int err;

	err = mdio_read(np, np->port, NIU_ESR_DEV_ADDR,
			ESR_GLUE_CTRL0_L(chan));
	if (err >= 0) {
		*val = (err & 0xffff);
		err = mdio_read(np, np->port, NIU_ESR_DEV_ADDR,
				ESR_GLUE_CTRL0_H(chan));
		if (err >= 0) {
			*val |= ((err & 0xffff) << 16);
			err = 0;
		}
	}
	return err;
}

static int esr_read_reset(struct niu *np, u32 *val)
{
	int err;

	err = mdio_read(np, np->port, NIU_ESR_DEV_ADDR,
			ESR_RXTX_RESET_CTRL_L);
	if (err >= 0) {
		*val = (err & 0xffff);
		err = mdio_read(np, np->port, NIU_ESR_DEV_ADDR,
				ESR_RXTX_RESET_CTRL_H);
		if (err >= 0) {
			*val |= ((err & 0xffff) << 16);
			err = 0;
		}
	}
	return err;
}

static int esr_write_rxtx_ctrl(struct niu *np, unsigned long chan, u32 val)
{
	int err;

	err = mdio_write(np, np->port, NIU_ESR_DEV_ADDR,
			 ESR_RXTX_CTRL_L(chan), val & 0xffff);
	if (!err)
		err = mdio_write(np, np->port, NIU_ESR_DEV_ADDR,
				 ESR_RXTX_CTRL_H(chan), (val >> 16));
	return err;
}

static int esr_write_glue0(struct niu *np, unsigned long chan, u32 val)
{
	int err;

	err = mdio_write(np, np->port, NIU_ESR_DEV_ADDR,
			ESR_GLUE_CTRL0_L(chan), val & 0xffff);
	if (!err)
		err = mdio_write(np, np->port, NIU_ESR_DEV_ADDR,
				 ESR_GLUE_CTRL0_H(chan), (val >> 16));
	return err;
}

static int esr_reset(struct niu *np)
{
	u32 uninitialized_var(reset);
	int err;

	err = mdio_write(np, np->port, NIU_ESR_DEV_ADDR,
			 ESR_RXTX_RESET_CTRL_L, 0x0000);
	if (err)
		return err;
	err = mdio_write(np, np->port, NIU_ESR_DEV_ADDR,
			 ESR_RXTX_RESET_CTRL_H, 0xffff);
	if (err)
		return err;
	udelay(200);

	err = mdio_write(np, np->port, NIU_ESR_DEV_ADDR,
			 ESR_RXTX_RESET_CTRL_L, 0xffff);
	if (err)
		return err;
	udelay(200);

	err = mdio_write(np, np->port, NIU_ESR_DEV_ADDR,
			 ESR_RXTX_RESET_CTRL_H, 0x0000);
	if (err)
		return err;
	udelay(200);

	err = esr_read_reset(np, &reset);
	if (err)
		return err;
	if (reset != 0) {
		dev_err(np->device, PFX "Port %u ESR_RESET "
			"did not clear [%08x]\n",
			np->port, reset);
		return -ENODEV;
	}

	return 0;
}

static int serdes_init_10g(struct niu *np)
{
	struct niu_link_config *lp = &np->link_config;
	unsigned long ctrl_reg, test_cfg_reg, i;
	u64 ctrl_val, test_cfg_val, sig, mask, val;
	int err;

	switch (np->port) {
	case 0:
		ctrl_reg = ENET_SERDES_0_CTRL_CFG;
		test_cfg_reg = ENET_SERDES_0_TEST_CFG;
		break;
	case 1:
		ctrl_reg = ENET_SERDES_1_CTRL_CFG;
		test_cfg_reg = ENET_SERDES_1_TEST_CFG;
		break;

	default:
		return -EINVAL;
	}
	ctrl_val = (ENET_SERDES_CTRL_SDET_0 |
		    ENET_SERDES_CTRL_SDET_1 |
		    ENET_SERDES_CTRL_SDET_2 |
		    ENET_SERDES_CTRL_SDET_3 |
		    (0x5 << ENET_SERDES_CTRL_EMPH_0_SHIFT) |
		    (0x5 << ENET_SERDES_CTRL_EMPH_1_SHIFT) |
		    (0x5 << ENET_SERDES_CTRL_EMPH_2_SHIFT) |
		    (0x5 << ENET_SERDES_CTRL_EMPH_3_SHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_0_SHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_1_SHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_2_SHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_3_SHIFT));
	test_cfg_val = 0;

	if (lp->loopback_mode == LOOPBACK_PHY) {
		test_cfg_val |= ((ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_0_SHIFT) |
				 (ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_1_SHIFT) |
				 (ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_2_SHIFT) |
				 (ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_3_SHIFT));
	}

	nw64(ctrl_reg, ctrl_val);
	nw64(test_cfg_reg, test_cfg_val);

	/* Initialize all 4 lanes of the SERDES.  */
	for (i = 0; i < 4; i++) {
		u32 rxtx_ctrl, glue0;

		err = esr_read_rxtx_ctrl(np, i, &rxtx_ctrl);
		if (err)
			return err;
		err = esr_read_glue0(np, i, &glue0);
		if (err)
			return err;

		rxtx_ctrl &= ~(ESR_RXTX_CTRL_VMUXLO);
		rxtx_ctrl |= (ESR_RXTX_CTRL_ENSTRETCH |
			      (2 << ESR_RXTX_CTRL_VMUXLO_SHIFT));

		glue0 &= ~(ESR_GLUE_CTRL0_SRATE |
			   ESR_GLUE_CTRL0_THCNT |
			   ESR_GLUE_CTRL0_BLTIME);
		glue0 |= (ESR_GLUE_CTRL0_RXLOSENAB |
			  (0xf << ESR_GLUE_CTRL0_SRATE_SHIFT) |
			  (0xff << ESR_GLUE_CTRL0_THCNT_SHIFT) |
			  (BLTIME_300_CYCLES <<
			   ESR_GLUE_CTRL0_BLTIME_SHIFT));

		err = esr_write_rxtx_ctrl(np, i, rxtx_ctrl);
		if (err)
			return err;
		err = esr_write_glue0(np, i, glue0);
		if (err)
			return err;
	}

	err = esr_reset(np);
	if (err)
		return err;

	sig = nr64(ESR_INT_SIGNALS);
	switch (np->port) {
	case 0:
		mask = ESR_INT_SIGNALS_P0_BITS;
		val = (ESR_INT_SRDY0_P0 |
		       ESR_INT_DET0_P0 |
		       ESR_INT_XSRDY_P0 |
		       ESR_INT_XDP_P0_CH3 |
		       ESR_INT_XDP_P0_CH2 |
		       ESR_INT_XDP_P0_CH1 |
		       ESR_INT_XDP_P0_CH0);
		break;

	case 1:
		mask = ESR_INT_SIGNALS_P1_BITS;
		val = (ESR_INT_SRDY0_P1 |
		       ESR_INT_DET0_P1 |
		       ESR_INT_XSRDY_P1 |
		       ESR_INT_XDP_P1_CH3 |
		       ESR_INT_XDP_P1_CH2 |
		       ESR_INT_XDP_P1_CH1 |
		       ESR_INT_XDP_P1_CH0);
		break;

	default:
		return -EINVAL;
	}

	if ((sig & mask) != val) {
		if (np->flags & NIU_FLAGS_HOTPLUG_PHY) {
			np->flags &= ~NIU_FLAGS_HOTPLUG_PHY_PRESENT;
			return 0;
		}
		dev_err(np->device, PFX "Port %u signal bits [%08x] are not "
			"[%08x]\n", np->port, (int) (sig & mask), (int) val);
		return -ENODEV;
	}
	if (np->flags & NIU_FLAGS_HOTPLUG_PHY)
		np->flags |= NIU_FLAGS_HOTPLUG_PHY_PRESENT;
	return 0;
}

static int serdes_init_1g(struct niu *np)
{
	u64 val;

	val = nr64(ENET_SERDES_1_PLL_CFG);
	val &= ~ENET_SERDES_PLL_FBDIV2;
	switch (np->port) {
	case 0:
		val |= ENET_SERDES_PLL_HRATE0;
		break;
	case 1:
		val |= ENET_SERDES_PLL_HRATE1;
		break;
	case 2:
		val |= ENET_SERDES_PLL_HRATE2;
		break;
	case 3:
		val |= ENET_SERDES_PLL_HRATE3;
		break;
	default:
		return -EINVAL;
	}
	nw64(ENET_SERDES_1_PLL_CFG, val);

	return 0;
}

static int serdes_init_1g_serdes(struct niu *np)
{
	struct niu_link_config *lp = &np->link_config;
	unsigned long ctrl_reg, test_cfg_reg, pll_cfg, i;
	u64 ctrl_val, test_cfg_val, sig, mask, val;
	int err;
	u64 reset_val, val_rd;

	val = ENET_SERDES_PLL_HRATE0 | ENET_SERDES_PLL_HRATE1 |
		ENET_SERDES_PLL_HRATE2 | ENET_SERDES_PLL_HRATE3 |
		ENET_SERDES_PLL_FBDIV0;
	switch (np->port) {
	case 0:
		reset_val =  ENET_SERDES_RESET_0;
		ctrl_reg = ENET_SERDES_0_CTRL_CFG;
		test_cfg_reg = ENET_SERDES_0_TEST_CFG;
		pll_cfg = ENET_SERDES_0_PLL_CFG;
		break;
	case 1:
		reset_val =  ENET_SERDES_RESET_1;
		ctrl_reg = ENET_SERDES_1_CTRL_CFG;
		test_cfg_reg = ENET_SERDES_1_TEST_CFG;
		pll_cfg = ENET_SERDES_1_PLL_CFG;
		break;

	default:
		return -EINVAL;
	}
	ctrl_val = (ENET_SERDES_CTRL_SDET_0 |
		    ENET_SERDES_CTRL_SDET_1 |
		    ENET_SERDES_CTRL_SDET_2 |
		    ENET_SERDES_CTRL_SDET_3 |
		    (0x5 << ENET_SERDES_CTRL_EMPH_0_SHIFT) |
		    (0x5 << ENET_SERDES_CTRL_EMPH_1_SHIFT) |
		    (0x5 << ENET_SERDES_CTRL_EMPH_2_SHIFT) |
		    (0x5 << ENET_SERDES_CTRL_EMPH_3_SHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_0_SHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_1_SHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_2_SHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_3_SHIFT));
	test_cfg_val = 0;

	if (lp->loopback_mode == LOOPBACK_PHY) {
		test_cfg_val |= ((ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_0_SHIFT) |
				 (ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_1_SHIFT) |
				 (ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_2_SHIFT) |
				 (ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_3_SHIFT));
	}

	nw64(ENET_SERDES_RESET, reset_val);
	mdelay(20);
	val_rd = nr64(ENET_SERDES_RESET);
	val_rd &= ~reset_val;
	nw64(pll_cfg, val);
	nw64(ctrl_reg, ctrl_val);
	nw64(test_cfg_reg, test_cfg_val);
	nw64(ENET_SERDES_RESET, val_rd);
	mdelay(2000);

	/* Initialize all 4 lanes of the SERDES.  */
	for (i = 0; i < 4; i++) {
		u32 rxtx_ctrl, glue0;

		err = esr_read_rxtx_ctrl(np, i, &rxtx_ctrl);
		if (err)
			return err;
		err = esr_read_glue0(np, i, &glue0);
		if (err)
			return err;

		rxtx_ctrl &= ~(ESR_RXTX_CTRL_VMUXLO);
		rxtx_ctrl |= (ESR_RXTX_CTRL_ENSTRETCH |
			      (2 << ESR_RXTX_CTRL_VMUXLO_SHIFT));

		glue0 &= ~(ESR_GLUE_CTRL0_SRATE |
			   ESR_GLUE_CTRL0_THCNT |
			   ESR_GLUE_CTRL0_BLTIME);
		glue0 |= (ESR_GLUE_CTRL0_RXLOSENAB |
			  (0xf << ESR_GLUE_CTRL0_SRATE_SHIFT) |
			  (0xff << ESR_GLUE_CTRL0_THCNT_SHIFT) |
			  (BLTIME_300_CYCLES <<
			   ESR_GLUE_CTRL0_BLTIME_SHIFT));

		err = esr_write_rxtx_ctrl(np, i, rxtx_ctrl);
		if (err)
			return err;
		err = esr_write_glue0(np, i, glue0);
		if (err)
			return err;
	}


	sig = nr64(ESR_INT_SIGNALS);
	switch (np->port) {
	case 0:
		val = (ESR_INT_SRDY0_P0 | ESR_INT_DET0_P0);
		mask = val;
		break;

	case 1:
		val = (ESR_INT_SRDY0_P1 | ESR_INT_DET0_P1);
		mask = val;
		break;

	default:
		return -EINVAL;
	}

	if ((sig & mask) != val) {
		dev_err(np->device, PFX "Port %u signal bits [%08x] are not "
			"[%08x]\n", np->port, (int) (sig & mask), (int) val);
		return -ENODEV;
	}

	return 0;
}

static int link_status_1g_serdes(struct niu *np, int *link_up_p)
{
	struct niu_link_config *lp = &np->link_config;
	int link_up;
	u64 val;
	u16 current_speed;
	unsigned long flags;
	u8 current_duplex;

	link_up = 0;
	current_speed = SPEED_INVALID;
	current_duplex = DUPLEX_INVALID;

	spin_lock_irqsave(&np->lock, flags);

	val = nr64_pcs(PCS_MII_STAT);

	if (val & PCS_MII_STAT_LINK_STATUS) {
		link_up = 1;
		current_speed = SPEED_1000;
		current_duplex = DUPLEX_FULL;
	}

	lp->active_speed = current_speed;
	lp->active_duplex = current_duplex;
	spin_unlock_irqrestore(&np->lock, flags);

	*link_up_p = link_up;
	return 0;
}

static int link_status_10g_serdes(struct niu *np, int *link_up_p)
{
	unsigned long flags;
	struct niu_link_config *lp = &np->link_config;
	int link_up = 0;
	int link_ok = 1;
	u64 val, val2;
	u16 current_speed;
	u8 current_duplex;

	if (!(np->flags & NIU_FLAGS_10G))
		return link_status_1g_serdes(np, link_up_p);

	current_speed = SPEED_INVALID;
	current_duplex = DUPLEX_INVALID;
	spin_lock_irqsave(&np->lock, flags);

	val = nr64_xpcs(XPCS_STATUS(0));
	val2 = nr64_mac(XMAC_INTER2);
	if (val2 & 0x01000000)
		link_ok = 0;

	if ((val & 0x1000ULL) && link_ok) {
		link_up = 1;
		current_speed = SPEED_10000;
		current_duplex = DUPLEX_FULL;
	}
	lp->active_speed = current_speed;
	lp->active_duplex = current_duplex;
	spin_unlock_irqrestore(&np->lock, flags);
	*link_up_p = link_up;
	return 0;
}

static int link_status_mii(struct niu *np, int *link_up_p)
{
	struct niu_link_config *lp = &np->link_config;
	int err;
	int bmsr, advert, ctrl1000, stat1000, lpa, bmcr, estatus;
	int supported, advertising, active_speed, active_duplex;

	err = mii_read(np, np->phy_addr, MII_BMCR);
	if (unlikely(err < 0))
		return err;
	bmcr = err;

	err = mii_read(np, np->phy_addr, MII_BMSR);
	if (unlikely(err < 0))
		return err;
	bmsr = err;

	err = mii_read(np, np->phy_addr, MII_ADVERTISE);
	if (unlikely(err < 0))
		return err;
	advert = err;

	err = mii_read(np, np->phy_addr, MII_LPA);
	if (unlikely(err < 0))
		return err;
	lpa = err;

	if (likely(bmsr & BMSR_ESTATEN)) {
		err = mii_read(np, np->phy_addr, MII_ESTATUS);
		if (unlikely(err < 0))
			return err;
		estatus = err;

		err = mii_read(np, np->phy_addr, MII_CTRL1000);
		if (unlikely(err < 0))
			return err;
		ctrl1000 = err;

		err = mii_read(np, np->phy_addr, MII_STAT1000);
		if (unlikely(err < 0))
			return err;
		stat1000 = err;
	} else
		estatus = ctrl1000 = stat1000 = 0;

	supported = 0;
	if (bmsr & BMSR_ANEGCAPABLE)
		supported |= SUPPORTED_Autoneg;
	if (bmsr & BMSR_10HALF)
		supported |= SUPPORTED_10baseT_Half;
	if (bmsr & BMSR_10FULL)
		supported |= SUPPORTED_10baseT_Full;
	if (bmsr & BMSR_100HALF)
		supported |= SUPPORTED_100baseT_Half;
	if (bmsr & BMSR_100FULL)
		supported |= SUPPORTED_100baseT_Full;
	if (estatus & ESTATUS_1000_THALF)
		supported |= SUPPORTED_1000baseT_Half;
	if (estatus & ESTATUS_1000_TFULL)
		supported |= SUPPORTED_1000baseT_Full;
	lp->supported = supported;

	advertising = 0;
	if (advert & ADVERTISE_10HALF)
		advertising |= ADVERTISED_10baseT_Half;
	if (advert & ADVERTISE_10FULL)
		advertising |= ADVERTISED_10baseT_Full;
	if (advert & ADVERTISE_100HALF)
		advertising |= ADVERTISED_100baseT_Half;
	if (advert & ADVERTISE_100FULL)
		advertising |= ADVERTISED_100baseT_Full;
	if (ctrl1000 & ADVERTISE_1000HALF)
		advertising |= ADVERTISED_1000baseT_Half;
	if (ctrl1000 & ADVERTISE_1000FULL)
		advertising |= ADVERTISED_1000baseT_Full;

	if (bmcr & BMCR_ANENABLE) {
		int neg, neg1000;

		lp->active_autoneg = 1;
		advertising |= ADVERTISED_Autoneg;

		neg = advert & lpa;
		neg1000 = (ctrl1000 << 2) & stat1000;

		if (neg1000 & (LPA_1000FULL | LPA_1000HALF))
			active_speed = SPEED_1000;
		else if (neg & LPA_100)
			active_speed = SPEED_100;
		else if (neg & (LPA_10HALF | LPA_10FULL))
			active_speed = SPEED_10;
		else
			active_speed = SPEED_INVALID;

		if ((neg1000 & LPA_1000FULL) || (neg & LPA_DUPLEX))
			active_duplex = DUPLEX_FULL;
		else if (active_speed != SPEED_INVALID)
			active_duplex = DUPLEX_HALF;
		else
			active_duplex = DUPLEX_INVALID;
	} else {
		lp->active_autoneg = 0;

		if ((bmcr & BMCR_SPEED1000) && !(bmcr & BMCR_SPEED100))
			active_speed = SPEED_1000;
		else if (bmcr & BMCR_SPEED100)
			active_speed = SPEED_100;
		else
			active_speed = SPEED_10;

		if (bmcr & BMCR_FULLDPLX)
			active_duplex = DUPLEX_FULL;
		else
			active_duplex = DUPLEX_HALF;
	}

	lp->active_advertising = advertising;
	lp->active_speed = active_speed;
	lp->active_duplex = active_duplex;
	*link_up_p = !!(bmsr & BMSR_LSTATUS);

	return 0;
}

static int link_status_1g_rgmii(struct niu *np, int *link_up_p)
{
	struct niu_link_config *lp = &np->link_config;
	u16 current_speed, bmsr;
	unsigned long flags;
	u8 current_duplex;
	int err, link_up;

	link_up = 0;
	current_speed = SPEED_INVALID;
	current_duplex = DUPLEX_INVALID;

	spin_lock_irqsave(&np->lock, flags);

	err = -EINVAL;

	err = mii_read(np, np->phy_addr, MII_BMSR);
	if (err < 0)
		goto out;

	bmsr = err;
	if (bmsr & BMSR_LSTATUS) {
		u16 adv, lpa, common, estat;

		err = mii_read(np, np->phy_addr, MII_ADVERTISE);
		if (err < 0)
			goto out;
		adv = err;

		err = mii_read(np, np->phy_addr, MII_LPA);
		if (err < 0)
			goto out;
		lpa = err;

		common = adv & lpa;

		err = mii_read(np, np->phy_addr, MII_ESTATUS);
		if (err < 0)
			goto out;
		estat = err;
		link_up = 1;
		current_speed = SPEED_1000;
		current_duplex = DUPLEX_FULL;

	}
	lp->active_speed = current_speed;
	lp->active_duplex = current_duplex;
	err = 0;

out:
	spin_unlock_irqrestore(&np->lock, flags);

	*link_up_p = link_up;
	return err;
}

static int link_status_1g(struct niu *np, int *link_up_p)
{
	struct niu_link_config *lp = &np->link_config;
	unsigned long flags;
	int err;

	spin_lock_irqsave(&np->lock, flags);

	err = link_status_mii(np, link_up_p);
	lp->supported |= SUPPORTED_TP;
	lp->active_advertising |= ADVERTISED_TP;

	spin_unlock_irqrestore(&np->lock, flags);
	return err;
}

static int bcm8704_reset(struct niu *np)
{
	int err, limit;

	err = mdio_read(np, np->phy_addr,
			BCM8704_PHYXS_DEV_ADDR, MII_BMCR);
	if (err < 0 || err == 0xffff)
		return err;
	err |= BMCR_RESET;
	err = mdio_write(np, np->phy_addr, BCM8704_PHYXS_DEV_ADDR,
			 MII_BMCR, err);
	if (err)
		return err;

	limit = 1000;
	while (--limit >= 0) {
		err = mdio_read(np, np->phy_addr,
				BCM8704_PHYXS_DEV_ADDR, MII_BMCR);
		if (err < 0)
			return err;
		if (!(err & BMCR_RESET))
			break;
	}
	if (limit < 0) {
		dev_err(np->device, PFX "Port %u PHY will not reset "
			"(bmcr=%04x)\n", np->port, (err & 0xffff));
		return -ENODEV;
	}
	return 0;
}

/* When written, certain PHY registers need to be read back twice
 * in order for the bits to settle properly.
 */
static int bcm8704_user_dev3_readback(struct niu *np, int reg)
{
	int err = mdio_read(np, np->phy_addr, BCM8704_USER_DEV3_ADDR, reg);
	if (err < 0)
		return err;
	err = mdio_read(np, np->phy_addr, BCM8704_USER_DEV3_ADDR, reg);
	if (err < 0)
		return err;
	return 0;
}

static int bcm8706_init_user_dev3(struct niu *np)
{
	int err;


	err = mdio_read(np, np->phy_addr, BCM8704_USER_DEV3_ADDR,
			BCM8704_USER_OPT_DIGITAL_CTRL);
	if (err < 0)
		return err;
	err &= ~USER_ODIG_CTRL_GPIOS;
	err |= (0x3 << USER_ODIG_CTRL_GPIOS_SHIFT);
	err |=  USER_ODIG_CTRL_RESV2;
	err = mdio_write(np, np->phy_addr, BCM8704_USER_DEV3_ADDR,
			 BCM8704_USER_OPT_DIGITAL_CTRL, err);
	if (err)
		return err;

	mdelay(1000);

	return 0;
}

static int bcm8704_init_user_dev3(struct niu *np)
{
	int err;

	err = mdio_write(np, np->phy_addr,
			 BCM8704_USER_DEV3_ADDR, BCM8704_USER_CONTROL,
			 (USER_CONTROL_OPTXRST_LVL |
			  USER_CONTROL_OPBIASFLT_LVL |
			  USER_CONTROL_OBTMPFLT_LVL |
			  USER_CONTROL_OPPRFLT_LVL |
			  USER_CONTROL_OPTXFLT_LVL |
			  USER_CONTROL_OPRXLOS_LVL |
			  USER_CONTROL_OPRXFLT_LVL |
			  USER_CONTROL_OPTXON_LVL |
			  (0x3f << USER_CONTROL_RES1_SHIFT)));
	if (err)
		return err;

	err = mdio_write(np, np->phy_addr,
			 BCM8704_USER_DEV3_ADDR, BCM8704_USER_PMD_TX_CONTROL,
			 (USER_PMD_TX_CTL_XFP_CLKEN |
			  (1 << USER_PMD_TX_CTL_TX_DAC_TXD_SH) |
			  (2 << USER_PMD_TX_CTL_TX_DAC_TXCK_SH) |
			  USER_PMD_TX_CTL_TSCK_LPWREN));
	if (err)
		return err;

	err = bcm8704_user_dev3_readback(np, BCM8704_USER_CONTROL);
	if (err)
		return err;
	err = bcm8704_user_dev3_readback(np, BCM8704_USER_PMD_TX_CONTROL);
	if (err)
		return err;

	err = mdio_read(np, np->phy_addr, BCM8704_USER_DEV3_ADDR,
			BCM8704_USER_OPT_DIGITAL_CTRL);
	if (err < 0)
		return err;
	err &= ~USER_ODIG_CTRL_GPIOS;
	err |= (0x3 << USER_ODIG_CTRL_GPIOS_SHIFT);
	err = mdio_write(np, np->phy_addr, BCM8704_USER_DEV3_ADDR,
			 BCM8704_USER_OPT_DIGITAL_CTRL, err);
	if (err)
		return err;

	mdelay(1000);

	return 0;
}

static int mrvl88x2011_act_led(struct niu *np, int val)
{
	int	err;

	err  = mdio_read(np, np->phy_addr, MRVL88X2011_USER_DEV2_ADDR,
		MRVL88X2011_LED_8_TO_11_CTL);
	if (err < 0)
		return err;

	err &= ~MRVL88X2011_LED(MRVL88X2011_LED_ACT,MRVL88X2011_LED_CTL_MASK);
	err |=  MRVL88X2011_LED(MRVL88X2011_LED_ACT,val);

	return mdio_write(np, np->phy_addr, MRVL88X2011_USER_DEV2_ADDR,
			  MRVL88X2011_LED_8_TO_11_CTL, err);
}

static int mrvl88x2011_led_blink_rate(struct niu *np, int rate)
{
	int	err;

	err = mdio_read(np, np->phy_addr, MRVL88X2011_USER_DEV2_ADDR,
			MRVL88X2011_LED_BLINK_CTL);
	if (err >= 0) {
		err &= ~MRVL88X2011_LED_BLKRATE_MASK;
		err |= (rate << 4);

		err = mdio_write(np, np->phy_addr, MRVL88X2011_USER_DEV2_ADDR,
				 MRVL88X2011_LED_BLINK_CTL, err);
	}

	return err;
}

static int xcvr_init_10g_mrvl88x2011(struct niu *np)
{
	int	err;

	/* Set LED functions */
	err = mrvl88x2011_led_blink_rate(np, MRVL88X2011_LED_BLKRATE_134MS);
	if (err)
		return err;

	/* led activity */
	err = mrvl88x2011_act_led(np, MRVL88X2011_LED_CTL_OFF);
	if (err)
		return err;

	err = mdio_read(np, np->phy_addr, MRVL88X2011_USER_DEV3_ADDR,
			MRVL88X2011_GENERAL_CTL);
	if (err < 0)
		return err;

	err |= MRVL88X2011_ENA_XFPREFCLK;

	err = mdio_write(np, np->phy_addr, MRVL88X2011_USER_DEV3_ADDR,
			 MRVL88X2011_GENERAL_CTL, err);
	if (err < 0)
		return err;

	err = mdio_read(np, np->phy_addr, MRVL88X2011_USER_DEV1_ADDR,
			MRVL88X2011_PMA_PMD_CTL_1);
	if (err < 0)
		return err;

	if (np->link_config.loopback_mode == LOOPBACK_MAC)
		err |= MRVL88X2011_LOOPBACK;
	else
		err &= ~MRVL88X2011_LOOPBACK;

	err = mdio_write(np, np->phy_addr, MRVL88X2011_USER_DEV1_ADDR,
			 MRVL88X2011_PMA_PMD_CTL_1, err);
	if (err < 0)
		return err;

	/* Enable PMD  */
	return mdio_write(np, np->phy_addr, MRVL88X2011_USER_DEV1_ADDR,
			  MRVL88X2011_10G_PMD_TX_DIS, MRVL88X2011_ENA_PMDTX);
}


static int xcvr_diag_bcm870x(struct niu *np)
{
	u16 analog_stat0, tx_alarm_status;
	int err = 0;

#if 1
	err = mdio_read(np, np->phy_addr, BCM8704_PMA_PMD_DEV_ADDR,
			MII_STAT1000);
	if (err < 0)
		return err;
	pr_info(PFX "Port %u PMA_PMD(MII_STAT1000) [%04x]\n",
		np->port, err);

	err = mdio_read(np, np->phy_addr, BCM8704_USER_DEV3_ADDR, 0x20);
	if (err < 0)
		return err;
	pr_info(PFX "Port %u USER_DEV3(0x20) [%04x]\n",
		np->port, err);

	err = mdio_read(np, np->phy_addr, BCM8704_PHYXS_DEV_ADDR,
			MII_NWAYTEST);
	if (err < 0)
		return err;
	pr_info(PFX "Port %u PHYXS(MII_NWAYTEST) [%04x]\n",
		np->port, err);
#endif

	/* XXX dig this out it might not be so useful XXX */
	err = mdio_read(np, np->phy_addr, BCM8704_USER_DEV3_ADDR,
			BCM8704_USER_ANALOG_STATUS0);
	if (err < 0)
		return err;
	err = mdio_read(np, np->phy_addr, BCM8704_USER_DEV3_ADDR,
			BCM8704_USER_ANALOG_STATUS0);
	if (err < 0)
		return err;
	analog_stat0 = err;

	err = mdio_read(np, np->phy_addr, BCM8704_USER_DEV3_ADDR,
			BCM8704_USER_TX_ALARM_STATUS);
	if (err < 0)
		return err;
	err = mdio_read(np, np->phy_addr, BCM8704_USER_DEV3_ADDR,
			BCM8704_USER_TX_ALARM_STATUS);
	if (err < 0)
		return err;
	tx_alarm_status = err;

	if (analog_stat0 != 0x03fc) {
		if ((analog_stat0 == 0x43bc) && (tx_alarm_status != 0)) {
			pr_info(PFX "Port %u cable not connected "
				"or bad cable.\n", np->port);
		} else if (analog_stat0 == 0x639c) {
			pr_info(PFX "Port %u optical module is bad "
				"or missing.\n", np->port);
		}
	}

	return 0;
}

static int xcvr_10g_set_lb_bcm870x(struct niu *np)
{
	struct niu_link_config *lp = &np->link_config;
	int err;

	err = mdio_read(np, np->phy_addr, BCM8704_PCS_DEV_ADDR,
			MII_BMCR);
	if (err < 0)
		return err;

	err &= ~BMCR_LOOPBACK;

	if (lp->loopback_mode == LOOPBACK_MAC)
		err |= BMCR_LOOPBACK;

	err = mdio_write(np, np->phy_addr, BCM8704_PCS_DEV_ADDR,
			 MII_BMCR, err);
	if (err)
		return err;

	return 0;
}

static int xcvr_init_10g_bcm8706(struct niu *np)
{
	int err = 0;
	u64 val;

	if ((np->flags & NIU_FLAGS_HOTPLUG_PHY) &&
	    (np->flags & NIU_FLAGS_HOTPLUG_PHY_PRESENT) == 0)
			return err;

	val = nr64_mac(XMAC_CONFIG);
	val &= ~XMAC_CONFIG_LED_POLARITY;
	val |= XMAC_CONFIG_FORCE_LED_ON;
	nw64_mac(XMAC_CONFIG, val);

	val = nr64(MIF_CONFIG);
	val |= MIF_CONFIG_INDIRECT_MODE;
	nw64(MIF_CONFIG, val);

	err = bcm8704_reset(np);
	if (err)
		return err;

	err = xcvr_10g_set_lb_bcm870x(np);
	if (err)
		return err;

	err = bcm8706_init_user_dev3(np);
	if (err)
		return err;

	err = xcvr_diag_bcm870x(np);
	if (err)
		return err;

	return 0;
}

static int xcvr_init_10g_bcm8704(struct niu *np)
{
	int err;

	err = bcm8704_reset(np);
	if (err)
		return err;

	err = bcm8704_init_user_dev3(np);
	if (err)
		return err;

	err = xcvr_10g_set_lb_bcm870x(np);
	if (err)
		return err;

	err =  xcvr_diag_bcm870x(np);
	if (err)
		return err;

	return 0;
}

static int xcvr_init_10g(struct niu *np)
{
	int phy_id, err;
	u64 val;

	val = nr64_mac(XMAC_CONFIG);
	val &= ~XMAC_CONFIG_LED_POLARITY;
	val |= XMAC_CONFIG_FORCE_LED_ON;
	nw64_mac(XMAC_CONFIG, val);

	/* XXX shared resource, lock parent XXX */
	val = nr64(MIF_CONFIG);
	val |= MIF_CONFIG_INDIRECT_MODE;
	nw64(MIF_CONFIG, val);

	phy_id = phy_decode(np->parent->port_phy, np->port);
	phy_id = np->parent->phy_probe_info.phy_id[phy_id][np->port];

	/* handle different phy types */
	switch (phy_id & NIU_PHY_ID_MASK) {
	case NIU_PHY_ID_MRVL88X2011:
		err = xcvr_init_10g_mrvl88x2011(np);
		break;

	default: /* bcom 8704 */
		err = xcvr_init_10g_bcm8704(np);
		break;
	}

	return 0;
}

static int mii_reset(struct niu *np)
{
	int limit, err;

	err = mii_write(np, np->phy_addr, MII_BMCR, BMCR_RESET);
	if (err)
		return err;

	limit = 1000;
	while (--limit >= 0) {
		udelay(500);
		err = mii_read(np, np->phy_addr, MII_BMCR);
		if (err < 0)
			return err;
		if (!(err & BMCR_RESET))
			break;
	}
	if (limit < 0) {
		dev_err(np->device, PFX "Port %u MII would not reset, "
			"bmcr[%04x]\n", np->port, err);
		return -ENODEV;
	}

	return 0;
}

static int xcvr_init_1g_rgmii(struct niu *np)
{
	int err;
	u64 val;
	u16 bmcr, bmsr, estat;

	val = nr64(MIF_CONFIG);
	val &= ~MIF_CONFIG_INDIRECT_MODE;
	nw64(MIF_CONFIG, val);

	err = mii_reset(np);
	if (err)
		return err;

	err = mii_read(np, np->phy_addr, MII_BMSR);
	if (err < 0)
		return err;
	bmsr = err;

	estat = 0;
	if (bmsr & BMSR_ESTATEN) {
		err = mii_read(np, np->phy_addr, MII_ESTATUS);
		if (err < 0)
			return err;
		estat = err;
	}

	bmcr = 0;
	err = mii_write(np, np->phy_addr, MII_BMCR, bmcr);
	if (err)
		return err;

	if (bmsr & BMSR_ESTATEN) {
		u16 ctrl1000 = 0;

		if (estat & ESTATUS_1000_TFULL)
			ctrl1000 |= ADVERTISE_1000FULL;
		err = mii_write(np, np->phy_addr, MII_CTRL1000, ctrl1000);
		if (err)
			return err;
	}

	bmcr = (BMCR_SPEED1000 | BMCR_FULLDPLX);

	err = mii_write(np, np->phy_addr, MII_BMCR, bmcr);
	if (err)
		return err;

	err = mii_read(np, np->phy_addr, MII_BMCR);
	if (err < 0)
		return err;
	bmcr = mii_read(np, np->phy_addr, MII_BMCR);

	err = mii_read(np, np->phy_addr, MII_BMSR);
	if (err < 0)
		return err;

	return 0;
}

static int mii_init_common(struct niu *np)
{
	struct niu_link_config *lp = &np->link_config;
	u16 bmcr, bmsr, adv, estat;
	int err;

	err = mii_reset(np);
	if (err)
		return err;

	err = mii_read(np, np->phy_addr, MII_BMSR);
	if (err < 0)
		return err;
	bmsr = err;

	estat = 0;
	if (bmsr & BMSR_ESTATEN) {
		err = mii_read(np, np->phy_addr, MII_ESTATUS);
		if (err < 0)
			return err;
		estat = err;
	}

	bmcr = 0;
	err = mii_write(np, np->phy_addr, MII_BMCR, bmcr);
	if (err)
		return err;

	if (lp->loopback_mode == LOOPBACK_MAC) {
		bmcr |= BMCR_LOOPBACK;
		if (lp->active_speed == SPEED_1000)
			bmcr |= BMCR_SPEED1000;
		if (lp->active_duplex == DUPLEX_FULL)
			bmcr |= BMCR_FULLDPLX;
	}

	if (lp->loopback_mode == LOOPBACK_PHY) {
		u16 aux;

		aux = (BCM5464R_AUX_CTL_EXT_LB |
		       BCM5464R_AUX_CTL_WRITE_1);
		err = mii_write(np, np->phy_addr, BCM5464R_AUX_CTL, aux);
		if (err)
			return err;
	}

	if (lp->autoneg) {
		u16 ctrl1000;

		adv = ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP;
		if ((bmsr & BMSR_10HALF) &&
			(lp->advertising & ADVERTISED_10baseT_Half))
			adv |= ADVERTISE_10HALF;
		if ((bmsr & BMSR_10FULL) &&
			(lp->advertising & ADVERTISED_10baseT_Full))
			adv |= ADVERTISE_10FULL;
		if ((bmsr & BMSR_100HALF) &&
			(lp->advertising & ADVERTISED_100baseT_Half))
			adv |= ADVERTISE_100HALF;
		if ((bmsr & BMSR_100FULL) &&
			(lp->advertising & ADVERTISED_100baseT_Full))
			adv |= ADVERTISE_100FULL;
		err = mii_write(np, np->phy_addr, MII_ADVERTISE, adv);
		if (err)
			return err;

		if (likely(bmsr & BMSR_ESTATEN)) {
			ctrl1000 = 0;
			if ((estat & ESTATUS_1000_THALF) &&
				(lp->advertising & ADVERTISED_1000baseT_Half))
				ctrl1000 |= ADVERTISE_1000HALF;
			if ((estat & ESTATUS_1000_TFULL) &&
				(lp->advertising & ADVERTISED_1000baseT_Full))
				ctrl1000 |= ADVERTISE_1000FULL;
			err = mii_write(np, np->phy_addr,
					MII_CTRL1000, ctrl1000);
			if (err)
				return err;
		}

		bmcr |= (BMCR_ANENABLE | BMCR_ANRESTART);
	} else {
		/* !lp->autoneg */
		int fulldpx;

		if (lp->duplex == DUPLEX_FULL) {
			bmcr |= BMCR_FULLDPLX;
			fulldpx = 1;
		} else if (lp->duplex == DUPLEX_HALF)
			fulldpx = 0;
		else
			return -EINVAL;

		if (lp->speed == SPEED_1000) {
			/* if X-full requested while not supported, or
			   X-half requested while not supported... */
			if ((fulldpx && !(estat & ESTATUS_1000_TFULL)) ||
				(!fulldpx && !(estat & ESTATUS_1000_THALF)))
				return -EINVAL;
			bmcr |= BMCR_SPEED1000;
		} else if (lp->speed == SPEED_100) {
			if ((fulldpx && !(bmsr & BMSR_100FULL)) ||
				(!fulldpx && !(bmsr & BMSR_100HALF)))
				return -EINVAL;
			bmcr |= BMCR_SPEED100;
		} else if (lp->speed == SPEED_10) {
			if ((fulldpx && !(bmsr & BMSR_10FULL)) ||
				(!fulldpx && !(bmsr & BMSR_10HALF)))
				return -EINVAL;
		} else
			return -EINVAL;
	}

	err = mii_write(np, np->phy_addr, MII_BMCR, bmcr);
	if (err)
		return err;

#if 0
	err = mii_read(np, np->phy_addr, MII_BMCR);
	if (err < 0)
		return err;
	bmcr = err;

	err = mii_read(np, np->phy_addr, MII_BMSR);
	if (err < 0)
		return err;
	bmsr = err;

	pr_info(PFX "Port %u after MII init bmcr[%04x] bmsr[%04x]\n",
		np->port, bmcr, bmsr);
#endif

	return 0;
}

static int xcvr_init_1g(struct niu *np)
{
	u64 val;

	/* XXX shared resource, lock parent XXX */
	val = nr64(MIF_CONFIG);
	val &= ~MIF_CONFIG_INDIRECT_MODE;
	nw64(MIF_CONFIG, val);

	return mii_init_common(np);
}

static int niu_xcvr_init(struct niu *np)
{
	const struct niu_phy_ops *ops = np->phy_ops;
	int err;

	err = 0;
	if (ops->xcvr_init)
		err = ops->xcvr_init(np);

	return err;
}

static int niu_serdes_init(struct niu *np)
{
	const struct niu_phy_ops *ops = np->phy_ops;
	int err;

	err = 0;
	if (ops->serdes_init)
		err = ops->serdes_init(np);

	return err;
}

static void niu_init_xif(struct niu *);
static void niu_handle_led(struct niu *, int status);

static int niu_link_status_common(struct niu *np, int link_up)
{
	struct niu_link_config *lp = &np->link_config;
	struct net_device *dev = np->dev;
	unsigned long flags;

	if (!netif_carrier_ok(dev) && link_up) {
		niuinfo(LINK, "%s: Link is up at %s, %s duplex\n",
		       dev->name,
		       (lp->active_speed == SPEED_10000 ?
			"10Gb/sec" :
			(lp->active_speed == SPEED_1000 ?
			 "1Gb/sec" :
			 (lp->active_speed == SPEED_100 ?
			  "100Mbit/sec" : "10Mbit/sec"))),
		       (lp->active_duplex == DUPLEX_FULL ?
			"full" : "half"));

		spin_lock_irqsave(&np->lock, flags);
		niu_init_xif(np);
		niu_handle_led(np, 1);
		spin_unlock_irqrestore(&np->lock, flags);

		netif_carrier_on(dev);
	} else if (netif_carrier_ok(dev) && !link_up) {
		niuwarn(LINK, "%s: Link is down\n", dev->name);
		spin_lock_irqsave(&np->lock, flags);
		niu_handle_led(np, 0);
		spin_unlock_irqrestore(&np->lock, flags);
		netif_carrier_off(dev);
	}

	return 0;
}

static int link_status_10g_mrvl(struct niu *np, int *link_up_p)
{
	int err, link_up, pma_status, pcs_status;

	link_up = 0;

	err = mdio_read(np, np->phy_addr, MRVL88X2011_USER_DEV1_ADDR,
			MRVL88X2011_10G_PMD_STATUS_2);
	if (err < 0)
		goto out;

	/* Check PMA/PMD Register: 1.0001.2 == 1 */
	err = mdio_read(np, np->phy_addr, MRVL88X2011_USER_DEV1_ADDR,
			MRVL88X2011_PMA_PMD_STATUS_1);
	if (err < 0)
		goto out;

	pma_status = ((err & MRVL88X2011_LNK_STATUS_OK) ? 1 : 0);

        /* Check PMC Register : 3.0001.2 == 1: read twice */
	err = mdio_read(np, np->phy_addr, MRVL88X2011_USER_DEV3_ADDR,
			MRVL88X2011_PMA_PMD_STATUS_1);
	if (err < 0)
		goto out;

	err = mdio_read(np, np->phy_addr, MRVL88X2011_USER_DEV3_ADDR,
			MRVL88X2011_PMA_PMD_STATUS_1);
	if (err < 0)
		goto out;

	pcs_status = ((err & MRVL88X2011_LNK_STATUS_OK) ? 1 : 0);

        /* Check XGXS Register : 4.0018.[0-3,12] */
	err = mdio_read(np, np->phy_addr, MRVL88X2011_USER_DEV4_ADDR,
			MRVL88X2011_10G_XGXS_LANE_STAT);
	if (err < 0)
		goto out;

	if (err == (PHYXS_XGXS_LANE_STAT_ALINGED | PHYXS_XGXS_LANE_STAT_LANE3 |
		    PHYXS_XGXS_LANE_STAT_LANE2 | PHYXS_XGXS_LANE_STAT_LANE1 |
		    PHYXS_XGXS_LANE_STAT_LANE0 | PHYXS_XGXS_LANE_STAT_MAGIC |
		    0x800))
		link_up = (pma_status && pcs_status) ? 1 : 0;

	np->link_config.active_speed = SPEED_10000;
	np->link_config.active_duplex = DUPLEX_FULL;
	err = 0;
out:
	mrvl88x2011_act_led(np, (link_up ?
				 MRVL88X2011_LED_CTL_PCS_ACT :
				 MRVL88X2011_LED_CTL_OFF));

	*link_up_p = link_up;
	return err;
}

static int link_status_10g_bcm8706(struct niu *np, int *link_up_p)
{
	int err, link_up;
	link_up = 0;

	err = mdio_read(np, np->phy_addr, BCM8704_PMA_PMD_DEV_ADDR,
			BCM8704_PMD_RCV_SIGDET);
	if (err < 0 || err == 0xffff)
		goto out;
	if (!(err & PMD_RCV_SIGDET_GLOBAL)) {
		err = 0;
		goto out;
	}

	err = mdio_read(np, np->phy_addr, BCM8704_PCS_DEV_ADDR,
			BCM8704_PCS_10G_R_STATUS);
	if (err < 0)
		goto out;

	if (!(err & PCS_10G_R_STATUS_BLK_LOCK)) {
		err = 0;
		goto out;
	}

	err = mdio_read(np, np->phy_addr, BCM8704_PHYXS_DEV_ADDR,
			BCM8704_PHYXS_XGXS_LANE_STAT);
	if (err < 0)
		goto out;
	if (err != (PHYXS_XGXS_LANE_STAT_ALINGED |
		    PHYXS_XGXS_LANE_STAT_MAGIC |
		    PHYXS_XGXS_LANE_STAT_PATTEST |
		    PHYXS_XGXS_LANE_STAT_LANE3 |
		    PHYXS_XGXS_LANE_STAT_LANE2 |
		    PHYXS_XGXS_LANE_STAT_LANE1 |
		    PHYXS_XGXS_LANE_STAT_LANE0)) {
		err = 0;
		np->link_config.active_speed = SPEED_INVALID;
		np->link_config.active_duplex = DUPLEX_INVALID;
		goto out;
	}

	link_up = 1;
	np->link_config.active_speed = SPEED_10000;
	np->link_config.active_duplex = DUPLEX_FULL;
	err = 0;

out:
	*link_up_p = link_up;
	return err;
}

static int link_status_10g_bcom(struct niu *np, int *link_up_p)
{
	int err, link_up;

	link_up = 0;

	err = mdio_read(np, np->phy_addr, BCM8704_PMA_PMD_DEV_ADDR,
			BCM8704_PMD_RCV_SIGDET);
	if (err < 0)
		goto out;
	if (!(err & PMD_RCV_SIGDET_GLOBAL)) {
		err = 0;
		goto out;
	}

	err = mdio_read(np, np->phy_addr, BCM8704_PCS_DEV_ADDR,
			BCM8704_PCS_10G_R_STATUS);
	if (err < 0)
		goto out;
	if (!(err & PCS_10G_R_STATUS_BLK_LOCK)) {
		err = 0;
		goto out;
	}

	err = mdio_read(np, np->phy_addr, BCM8704_PHYXS_DEV_ADDR,
			BCM8704_PHYXS_XGXS_LANE_STAT);
	if (err < 0)
		goto out;

	if (err != (PHYXS_XGXS_LANE_STAT_ALINGED |
		    PHYXS_XGXS_LANE_STAT_MAGIC |
		    PHYXS_XGXS_LANE_STAT_LANE3 |
		    PHYXS_XGXS_LANE_STAT_LANE2 |
		    PHYXS_XGXS_LANE_STAT_LANE1 |
		    PHYXS_XGXS_LANE_STAT_LANE0)) {
		err = 0;
		goto out;
	}

	link_up = 1;
	np->link_config.active_speed = SPEED_10000;
	np->link_config.active_duplex = DUPLEX_FULL;
	err = 0;

out:
	*link_up_p = link_up;
	return err;
}

static int link_status_10g(struct niu *np, int *link_up_p)
{
	unsigned long flags;
	int err = -EINVAL;

	spin_lock_irqsave(&np->lock, flags);

	if (np->link_config.loopback_mode == LOOPBACK_DISABLED) {
		int phy_id;

		phy_id = phy_decode(np->parent->port_phy, np->port);
		phy_id = np->parent->phy_probe_info.phy_id[phy_id][np->port];

		/* handle different phy types */
		switch (phy_id & NIU_PHY_ID_MASK) {
		case NIU_PHY_ID_MRVL88X2011:
			err = link_status_10g_mrvl(np, link_up_p);
			break;

		default: /* bcom 8704 */
			err = link_status_10g_bcom(np, link_up_p);
			break;
		}
	}

	spin_unlock_irqrestore(&np->lock, flags);

	return err;
}

static int niu_10g_phy_present(struct niu *np)
{
	u64 sig, mask, val;

	sig = nr64(ESR_INT_SIGNALS);
	switch (np->port) {
	case 0:
		mask = ESR_INT_SIGNALS_P0_BITS;
		val = (ESR_INT_SRDY0_P0 |
		       ESR_INT_DET0_P0 |
		       ESR_INT_XSRDY_P0 |
		       ESR_INT_XDP_P0_CH3 |
		       ESR_INT_XDP_P0_CH2 |
		       ESR_INT_XDP_P0_CH1 |
		       ESR_INT_XDP_P0_CH0);
		break;

	case 1:
		mask = ESR_INT_SIGNALS_P1_BITS;
		val = (ESR_INT_SRDY0_P1 |
		       ESR_INT_DET0_P1 |
		       ESR_INT_XSRDY_P1 |
		       ESR_INT_XDP_P1_CH3 |
		       ESR_INT_XDP_P1_CH2 |
		       ESR_INT_XDP_P1_CH1 |
		       ESR_INT_XDP_P1_CH0);
		break;

	default:
		return 0;
	}

	if ((sig & mask) != val)
		return 0;
	return 1;
}

static int link_status_10g_hotplug(struct niu *np, int *link_up_p)
{
	unsigned long flags;
	int err = 0;
	int phy_present;
	int phy_present_prev;

	spin_lock_irqsave(&np->lock, flags);

	if (np->link_config.loopback_mode == LOOPBACK_DISABLED) {
		phy_present_prev = (np->flags & NIU_FLAGS_HOTPLUG_PHY_PRESENT) ?
			1 : 0;
		phy_present = niu_10g_phy_present(np);
		if (phy_present != phy_present_prev) {
			/* state change */
			if (phy_present) {
				/* A NEM was just plugged in */
				np->flags |= NIU_FLAGS_HOTPLUG_PHY_PRESENT;
				if (np->phy_ops->xcvr_init)
					err = np->phy_ops->xcvr_init(np);
				if (err) {
					err = mdio_read(np, np->phy_addr,
						BCM8704_PHYXS_DEV_ADDR, MII_BMCR);
					if (err == 0xffff) {
						/* No mdio, back-to-back XAUI */
						goto out;
					}
					/* debounce */
					np->flags &= ~NIU_FLAGS_HOTPLUG_PHY_PRESENT;
				}
			} else {
				np->flags &= ~NIU_FLAGS_HOTPLUG_PHY_PRESENT;
				*link_up_p = 0;
				niuwarn(LINK, "%s: Hotplug PHY Removed\n",
					np->dev->name);
			}
		}
out:
		if (np->flags & NIU_FLAGS_HOTPLUG_PHY_PRESENT) {
			err = link_status_10g_bcm8706(np, link_up_p);
			if (err == 0xffff) {
				/* No mdio, back-to-back XAUI: it is C10NEM */
				*link_up_p = 1;
				np->link_config.active_speed = SPEED_10000;
				np->link_config.active_duplex = DUPLEX_FULL;
			}
		}
	}

	spin_unlock_irqrestore(&np->lock, flags);

	return 0;
}

static int niu_link_status(struct niu *np, int *link_up_p)
{
	const struct niu_phy_ops *ops = np->phy_ops;
	int err;

	err = 0;
	if (ops->link_status)
		err = ops->link_status(np, link_up_p);

	return err;
}

static void niu_timer(unsigned long __opaque)
{
	struct niu *np = (struct niu *) __opaque;
	unsigned long off;
	int err, link_up;

	err = niu_link_status(np, &link_up);
	if (!err)
		niu_link_status_common(np, link_up);

	if (netif_carrier_ok(np->dev))
		off = 5 * HZ;
	else
		off = 1 * HZ;
	np->timer.expires = jiffies + off;

	add_timer(&np->timer);
}

static const struct niu_phy_ops phy_ops_10g_serdes = {
	.serdes_init		= serdes_init_10g_serdes,
	.link_status		= link_status_10g_serdes,
};

static const struct niu_phy_ops phy_ops_10g_serdes_niu = {
	.serdes_init		= serdes_init_niu_10g_serdes,
	.link_status		= link_status_10g_serdes,
};

static const struct niu_phy_ops phy_ops_1g_serdes_niu = {
	.serdes_init		= serdes_init_niu_1g_serdes,
	.link_status		= link_status_1g_serdes,
};

static const struct niu_phy_ops phy_ops_1g_rgmii = {
	.xcvr_init		= xcvr_init_1g_rgmii,
	.link_status		= link_status_1g_rgmii,
};

static const struct niu_phy_ops phy_ops_10g_fiber_niu = {
	.serdes_init		= serdes_init_niu_10g_fiber,
	.xcvr_init		= xcvr_init_10g,
	.link_status		= link_status_10g,
};

static const struct niu_phy_ops phy_ops_10g_fiber = {
	.serdes_init		= serdes_init_10g,
	.xcvr_init		= xcvr_init_10g,
	.link_status		= link_status_10g,
};

static const struct niu_phy_ops phy_ops_10g_fiber_hotplug = {
	.serdes_init		= serdes_init_10g,
	.xcvr_init		= xcvr_init_10g_bcm8706,
	.link_status		= link_status_10g_hotplug,
};

static const struct niu_phy_ops phy_ops_niu_10g_hotplug = {
	.serdes_init		= serdes_init_niu_10g_fiber,
	.xcvr_init		= xcvr_init_10g_bcm8706,
	.link_status		= link_status_10g_hotplug,
};

static const struct niu_phy_ops phy_ops_10g_copper = {
	.serdes_init		= serdes_init_10g,
	.link_status		= link_status_10g, /* XXX */
};

static const struct niu_phy_ops phy_ops_1g_fiber = {
	.serdes_init		= serdes_init_1g,
	.xcvr_init		= xcvr_init_1g,
	.link_status		= link_status_1g,
};

static const struct niu_phy_ops phy_ops_1g_copper = {
	.xcvr_init		= xcvr_init_1g,
	.link_status		= link_status_1g,
};

struct niu_phy_template {
	const struct niu_phy_ops	*ops;
	u32				phy_addr_base;
};

static const struct niu_phy_template phy_template_niu_10g_fiber = {
	.ops		= &phy_ops_10g_fiber_niu,
	.phy_addr_base	= 16,
};

static const struct niu_phy_template phy_template_niu_10g_serdes = {
	.ops		= &phy_ops_10g_serdes_niu,
	.phy_addr_base	= 0,
};

static const struct niu_phy_template phy_template_niu_1g_serdes = {
	.ops		= &phy_ops_1g_serdes_niu,
	.phy_addr_base	= 0,
};

static const struct niu_phy_template phy_template_10g_fiber = {
	.ops		= &phy_ops_10g_fiber,
	.phy_addr_base	= 8,
};

static const struct niu_phy_template phy_template_10g_fiber_hotplug = {
	.ops		= &phy_ops_10g_fiber_hotplug,
	.phy_addr_base	= 8,
};

static const struct niu_phy_template phy_template_niu_10g_hotplug = {
	.ops		= &phy_ops_niu_10g_hotplug,
	.phy_addr_base	= 8,
};

static const struct niu_phy_template phy_template_10g_copper = {
	.ops		= &phy_ops_10g_copper,
	.phy_addr_base	= 10,
};

static const struct niu_phy_template phy_template_1g_fiber = {
	.ops		= &phy_ops_1g_fiber,
	.phy_addr_base	= 0,
};

static const struct niu_phy_template phy_template_1g_copper = {
	.ops		= &phy_ops_1g_copper,
	.phy_addr_base	= 0,
};

static const struct niu_phy_template phy_template_1g_rgmii = {
	.ops		= &phy_ops_1g_rgmii,
	.phy_addr_base	= 0,
};

static const struct niu_phy_template phy_template_10g_serdes = {
	.ops		= &phy_ops_10g_serdes,
	.phy_addr_base	= 0,
};

static int niu_atca_port_num[4] = {
	0, 0,  11, 10
};

static int serdes_init_10g_serdes(struct niu *np)
{
	struct niu_link_config *lp = &np->link_config;
	unsigned long ctrl_reg, test_cfg_reg, pll_cfg, i;
	u64 ctrl_val, test_cfg_val, sig, mask, val;
	u64 reset_val;

	switch (np->port) {
	case 0:
		reset_val =  ENET_SERDES_RESET_0;
		ctrl_reg = ENET_SERDES_0_CTRL_CFG;
		test_cfg_reg = ENET_SERDES_0_TEST_CFG;
		pll_cfg = ENET_SERDES_0_PLL_CFG;
		break;
	case 1:
		reset_val =  ENET_SERDES_RESET_1;
		ctrl_reg = ENET_SERDES_1_CTRL_CFG;
		test_cfg_reg = ENET_SERDES_1_TEST_CFG;
		pll_cfg = ENET_SERDES_1_PLL_CFG;
		break;

	default:
		return -EINVAL;
	}
	ctrl_val = (ENET_SERDES_CTRL_SDET_0 |
		    ENET_SERDES_CTRL_SDET_1 |
		    ENET_SERDES_CTRL_SDET_2 |
		    ENET_SERDES_CTRL_SDET_3 |
		    (0x5 << ENET_SERDES_CTRL_EMPH_0_SHIFT) |
		    (0x5 << ENET_SERDES_CTRL_EMPH_1_SHIFT) |
		    (0x5 << ENET_SERDES_CTRL_EMPH_2_SHIFT) |
		    (0x5 << ENET_SERDES_CTRL_EMPH_3_SHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_0_SHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_1_SHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_2_SHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_3_SHIFT));
	test_cfg_val = 0;

	if (lp->loopback_mode == LOOPBACK_PHY) {
		test_cfg_val |= ((ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_0_SHIFT) |
				 (ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_1_SHIFT) |
				 (ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_2_SHIFT) |
				 (ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_3_SHIFT));
	}

	esr_reset(np);
	nw64(pll_cfg, ENET_SERDES_PLL_FBDIV2);
	nw64(ctrl_reg, ctrl_val);
	nw64(test_cfg_reg, test_cfg_val);

	/* Initialize all 4 lanes of the SERDES.  */
	for (i = 0; i < 4; i++) {
		u32 rxtx_ctrl, glue0;
		int err;

		err = esr_read_rxtx_ctrl(np, i, &rxtx_ctrl);
		if (err)
			return err;
		err = esr_read_glue0(np, i, &glue0);
		if (err)
			return err;

		rxtx_ctrl &= ~(ESR_RXTX_CTRL_VMUXLO);
		rxtx_ctrl |= (ESR_RXTX_CTRL_ENSTRETCH |
			      (2 << ESR_RXTX_CTRL_VMUXLO_SHIFT));

		glue0 &= ~(ESR_GLUE_CTRL0_SRATE |
			   ESR_GLUE_CTRL0_THCNT |
			   ESR_GLUE_CTRL0_BLTIME);
		glue0 |= (ESR_GLUE_CTRL0_RXLOSENAB |
			  (0xf << ESR_GLUE_CTRL0_SRATE_SHIFT) |
			  (0xff << ESR_GLUE_CTRL0_THCNT_SHIFT) |
			  (BLTIME_300_CYCLES <<
			   ESR_GLUE_CTRL0_BLTIME_SHIFT));

		err = esr_write_rxtx_ctrl(np, i, rxtx_ctrl);
		if (err)
			return err;
		err = esr_write_glue0(np, i, glue0);
		if (err)
			return err;
	}


	sig = nr64(ESR_INT_SIGNALS);
	switch (np->port) {
	case 0:
		mask = ESR_INT_SIGNALS_P0_BITS;
		val = (ESR_INT_SRDY0_P0 |
		       ESR_INT_DET0_P0 |
		       ESR_INT_XSRDY_P0 |
		       ESR_INT_XDP_P0_CH3 |
		       ESR_INT_XDP_P0_CH2 |
		       ESR_INT_XDP_P0_CH1 |
		       ESR_INT_XDP_P0_CH0);
		break;

	case 1:
		mask = ESR_INT_SIGNALS_P1_BITS;
		val = (ESR_INT_SRDY0_P1 |
		       ESR_INT_DET0_P1 |
		       ESR_INT_XSRDY_P1 |
		       ESR_INT_XDP_P1_CH3 |
		       ESR_INT_XDP_P1_CH2 |
		       ESR_INT_XDP_P1_CH1 |
		       ESR_INT_XDP_P1_CH0);
		break;

	default:
		return -EINVAL;
	}

	if ((sig & mask) != val) {
		int err;
		err = serdes_init_1g_serdes(np);
		if (!err) {
			np->flags &= ~NIU_FLAGS_10G;
			np->mac_xcvr = MAC_XCVR_PCS;
		}  else {
			dev_err(np->device, PFX "Port %u 10G/1G SERDES Link Failed \n",
			 np->port);
			return -ENODEV;
		}
	}

	return 0;
}

static int niu_determine_phy_disposition(struct niu *np)
{
	struct niu_parent *parent = np->parent;
	u8 plat_type = parent->plat_type;
	const struct niu_phy_template *tp;
	u32 phy_addr_off = 0;

	if (plat_type == PLAT_TYPE_NIU) {
		switch (np->flags &
			(NIU_FLAGS_10G |
			 NIU_FLAGS_FIBER |
			 NIU_FLAGS_XCVR_SERDES)) {
		case NIU_FLAGS_10G | NIU_FLAGS_XCVR_SERDES:
			/* 10G Serdes */
			tp = &phy_template_niu_10g_serdes;
			break;
		case NIU_FLAGS_XCVR_SERDES:
			/* 1G Serdes */
			tp = &phy_template_niu_1g_serdes;
			break;
		case NIU_FLAGS_10G | NIU_FLAGS_FIBER:
			/* 10G Fiber */
		default:
			if (np->flags & NIU_FLAGS_HOTPLUG_PHY) {
				tp = &phy_template_niu_10g_hotplug;
				if (np->port == 0)
					phy_addr_off = 8;
				if (np->port == 1)
					phy_addr_off = 12;
			} else {
				tp = &phy_template_niu_10g_fiber;
				phy_addr_off += np->port;
			}
			break;
		}
	} else {
		switch (np->flags &
			(NIU_FLAGS_10G |
			 NIU_FLAGS_FIBER |
			 NIU_FLAGS_XCVR_SERDES)) {
		case 0:
			/* 1G copper */
			tp = &phy_template_1g_copper;
			if (plat_type == PLAT_TYPE_VF_P0)
				phy_addr_off = 10;
			else if (plat_type == PLAT_TYPE_VF_P1)
				phy_addr_off = 26;

			phy_addr_off += (np->port ^ 0x3);
			break;

		case NIU_FLAGS_10G:
			/* 10G copper */
			tp = &phy_template_10g_copper;
			break;

		case NIU_FLAGS_FIBER:
			/* 1G fiber */
			tp = &phy_template_1g_fiber;
			break;

		case NIU_FLAGS_10G | NIU_FLAGS_FIBER:
			/* 10G fiber */
			tp = &phy_template_10g_fiber;
			if (plat_type == PLAT_TYPE_VF_P0 ||
			    plat_type == PLAT_TYPE_VF_P1)
				phy_addr_off = 8;
			phy_addr_off += np->port;
			if (np->flags & NIU_FLAGS_HOTPLUG_PHY) {
				tp = &phy_template_10g_fiber_hotplug;
				if (np->port == 0)
					phy_addr_off = 8;
				if (np->port == 1)
					phy_addr_off = 12;
			}
			break;

		case NIU_FLAGS_10G | NIU_FLAGS_XCVR_SERDES:
		case NIU_FLAGS_XCVR_SERDES | NIU_FLAGS_FIBER:
		case NIU_FLAGS_XCVR_SERDES:
			switch(np->port) {
			case 0:
			case 1:
				tp = &phy_template_10g_serdes;
				break;
			case 2:
			case 3:
				tp = &phy_template_1g_rgmii;
				break;
			default:
				return -EINVAL;
				break;
			}
			phy_addr_off = niu_atca_port_num[np->port];
			break;

		default:
			return -EINVAL;
		}
	}

	np->phy_ops = tp->ops;
	np->phy_addr = tp->phy_addr_base + phy_addr_off;

	return 0;
}

static int niu_init_link(struct niu *np)
{
	struct niu_parent *parent = np->parent;
	int err, ignore;

	if (parent->plat_type == PLAT_TYPE_NIU) {
		err = niu_xcvr_init(np);
		if (err)
			return err;
		msleep(200);
	}
	err = niu_serdes_init(np);
	if (err && !(np->flags & NIU_FLAGS_HOTPLUG_PHY))
		return err;
	msleep(200);
	err = niu_xcvr_init(np);
	if (!err || (np->flags & NIU_FLAGS_HOTPLUG_PHY))
		niu_link_status(np, &ignore);
	return 0;
}

static void niu_set_primary_mac(struct niu *np, unsigned char *addr)
{
	u16 reg0 = addr[4] << 8 | addr[5];
	u16 reg1 = addr[2] << 8 | addr[3];
	u16 reg2 = addr[0] << 8 | addr[1];

	if (np->flags & NIU_FLAGS_XMAC) {
		nw64_mac(XMAC_ADDR0, reg0);
		nw64_mac(XMAC_ADDR1, reg1);
		nw64_mac(XMAC_ADDR2, reg2);
	} else {
		nw64_mac(BMAC_ADDR0, reg0);
		nw64_mac(BMAC_ADDR1, reg1);
		nw64_mac(BMAC_ADDR2, reg2);
	}
}

static int niu_num_alt_addr(struct niu *np)
{
	if (np->flags & NIU_FLAGS_XMAC)
		return XMAC_NUM_ALT_ADDR;
	else
		return BMAC_NUM_ALT_ADDR;
}

static int niu_set_alt_mac(struct niu *np, int index, unsigned char *addr)
{
	u16 reg0 = addr[4] << 8 | addr[5];
	u16 reg1 = addr[2] << 8 | addr[3];
	u16 reg2 = addr[0] << 8 | addr[1];

	if (index >= niu_num_alt_addr(np))
		return -EINVAL;

	if (np->flags & NIU_FLAGS_XMAC) {
		nw64_mac(XMAC_ALT_ADDR0(index), reg0);
		nw64_mac(XMAC_ALT_ADDR1(index), reg1);
		nw64_mac(XMAC_ALT_ADDR2(index), reg2);
	} else {
		nw64_mac(BMAC_ALT_ADDR0(index), reg0);
		nw64_mac(BMAC_ALT_ADDR1(index), reg1);
		nw64_mac(BMAC_ALT_ADDR2(index), reg2);
	}

	return 0;
}

static int niu_enable_alt_mac(struct niu *np, int index, int on)
{
	unsigned long reg;
	u64 val, mask;

	if (index >= niu_num_alt_addr(np))
		return -EINVAL;

	if (np->flags & NIU_FLAGS_XMAC) {
		reg = XMAC_ADDR_CMPEN;
		mask = 1 << index;
	} else {
		reg = BMAC_ADDR_CMPEN;
		mask = 1 << (index + 1);
	}

	val = nr64_mac(reg);
	if (on)
		val |= mask;
	else
		val &= ~mask;
	nw64_mac(reg, val);

	return 0;
}

static void __set_rdc_table_num_hw(struct niu *np, unsigned long reg,
				   int num, int mac_pref)
{
	u64 val = nr64_mac(reg);
	val &= ~(HOST_INFO_MACRDCTBLN | HOST_INFO_MPR);
	val |= num;
	if (mac_pref)
		val |= HOST_INFO_MPR;
	nw64_mac(reg, val);
}

static int __set_rdc_table_num(struct niu *np,
			       int xmac_index, int bmac_index,
			       int rdc_table_num, int mac_pref)
{
	unsigned long reg;

	if (rdc_table_num & ~HOST_INFO_MACRDCTBLN)
		return -EINVAL;
	if (np->flags & NIU_FLAGS_XMAC)
		reg = XMAC_HOST_INFO(xmac_index);
	else
		reg = BMAC_HOST_INFO(bmac_index);
	__set_rdc_table_num_hw(np, reg, rdc_table_num, mac_pref);
	return 0;
}

static int niu_set_primary_mac_rdc_table(struct niu *np, int table_num,
					 int mac_pref)
{
	return __set_rdc_table_num(np, 17, 0, table_num, mac_pref);
}

static int niu_set_multicast_mac_rdc_table(struct niu *np, int table_num,
					   int mac_pref)
{
	return __set_rdc_table_num(np, 16, 8, table_num, mac_pref);
}

static int niu_set_alt_mac_rdc_table(struct niu *np, int idx,
				     int table_num, int mac_pref)
{
	if (idx >= niu_num_alt_addr(np))
		return -EINVAL;
	return __set_rdc_table_num(np, idx, idx + 1, table_num, mac_pref);
}

static u64 vlan_entry_set_parity(u64 reg_val)
{
	u64 port01_mask;
	u64 port23_mask;

	port01_mask = 0x00ff;
	port23_mask = 0xff00;

	if (hweight64(reg_val & port01_mask) & 1)
		reg_val |= ENET_VLAN_TBL_PARITY0;
	else
		reg_val &= ~ENET_VLAN_TBL_PARITY0;

	if (hweight64(reg_val & port23_mask) & 1)
		reg_val |= ENET_VLAN_TBL_PARITY1;
	else
		reg_val &= ~ENET_VLAN_TBL_PARITY1;

	return reg_val;
}

static void vlan_tbl_write(struct niu *np, unsigned long index,
			   int port, int vpr, int rdc_table)
{
	u64 reg_val = nr64(ENET_VLAN_TBL(index));

	reg_val &= ~((ENET_VLAN_TBL_VPR |
		      ENET_VLAN_TBL_VLANRDCTBLN) <<
		     ENET_VLAN_TBL_SHIFT(port));
	if (vpr)
		reg_val |= (ENET_VLAN_TBL_VPR <<
			    ENET_VLAN_TBL_SHIFT(port));
	reg_val |= (rdc_table << ENET_VLAN_TBL_SHIFT(port));

	reg_val = vlan_entry_set_parity(reg_val);

	nw64(ENET_VLAN_TBL(index), reg_val);
}

static void vlan_tbl_clear(struct niu *np)
{
	int i;

	for (i = 0; i < ENET_VLAN_TBL_NUM_ENTRIES; i++)
		nw64(ENET_VLAN_TBL(i), 0);
}

static int tcam_wait_bit(struct niu *np, u64 bit)
{
	int limit = 1000;

	while (--limit > 0) {
		if (nr64(TCAM_CTL) & bit)
			break;
		udelay(1);
	}
	if (limit < 0)
		return -ENODEV;

	return 0;
}

static int tcam_flush(struct niu *np, int index)
{
	nw64(TCAM_KEY_0, 0x00);
	nw64(TCAM_KEY_MASK_0, 0xff);
	nw64(TCAM_CTL, (TCAM_CTL_RWC_TCAM_WRITE | index));

	return tcam_wait_bit(np, TCAM_CTL_STAT);
}

#if 0
static int tcam_read(struct niu *np, int index,
		     u64 *key, u64 *mask)
{
	int err;

	nw64(TCAM_CTL, (TCAM_CTL_RWC_TCAM_READ | index));
	err = tcam_wait_bit(np, TCAM_CTL_STAT);
	if (!err) {
		key[0] = nr64(TCAM_KEY_0);
		key[1] = nr64(TCAM_KEY_1);
		key[2] = nr64(TCAM_KEY_2);
		key[3] = nr64(TCAM_KEY_3);
		mask[0] = nr64(TCAM_KEY_MASK_0);
		mask[1] = nr64(TCAM_KEY_MASK_1);
		mask[2] = nr64(TCAM_KEY_MASK_2);
		mask[3] = nr64(TCAM_KEY_MASK_3);
	}
	return err;
}
#endif

static int tcam_write(struct niu *np, int index,
		      u64 *key, u64 *mask)
{
	nw64(TCAM_KEY_0, key[0]);
	nw64(TCAM_KEY_1, key[1]);
	nw64(TCAM_KEY_2, key[2]);
	nw64(TCAM_KEY_3, key[3]);
	nw64(TCAM_KEY_MASK_0, mask[0]);
	nw64(TCAM_KEY_MASK_1, mask[1]);
	nw64(TCAM_KEY_MASK_2, mask[2]);
	nw64(TCAM_KEY_MASK_3, mask[3]);
	nw64(TCAM_CTL, (TCAM_CTL_RWC_TCAM_WRITE | index));

	return tcam_wait_bit(np, TCAM_CTL_STAT);
}

#if 0
static int tcam_assoc_read(struct niu *np, int index, u64 *data)
{
	int err;

	nw64(TCAM_CTL, (TCAM_CTL_RWC_RAM_READ | index));
	err = tcam_wait_bit(np, TCAM_CTL_STAT);
	if (!err)
		*data = nr64(TCAM_KEY_1);

	return err;
}
#endif

static int tcam_assoc_write(struct niu *np, int index, u64 assoc_data)
{
	nw64(TCAM_KEY_1, assoc_data);
	nw64(TCAM_CTL, (TCAM_CTL_RWC_RAM_WRITE | index));

	return tcam_wait_bit(np, TCAM_CTL_STAT);
}

static void tcam_enable(struct niu *np, int on)
{
	u64 val = nr64(FFLP_CFG_1);

	if (on)
		val &= ~FFLP_CFG_1_TCAM_DIS;
	else
		val |= FFLP_CFG_1_TCAM_DIS;
	nw64(FFLP_CFG_1, val);
}

static void tcam_set_lat_and_ratio(struct niu *np, u64 latency, u64 ratio)
{
	u64 val = nr64(FFLP_CFG_1);

	val &= ~(FFLP_CFG_1_FFLPINITDONE |
		 FFLP_CFG_1_CAMLAT |
		 FFLP_CFG_1_CAMRATIO);
	val |= (latency << FFLP_CFG_1_CAMLAT_SHIFT);
	val |= (ratio << FFLP_CFG_1_CAMRATIO_SHIFT);
	nw64(FFLP_CFG_1, val);

	val = nr64(FFLP_CFG_1);
	val |= FFLP_CFG_1_FFLPINITDONE;
	nw64(FFLP_CFG_1, val);
}

static int tcam_user_eth_class_enable(struct niu *np, unsigned long class,
				      int on)
{
	unsigned long reg;
	u64 val;

	if (class < CLASS_CODE_ETHERTYPE1 ||
	    class > CLASS_CODE_ETHERTYPE2)
		return -EINVAL;

	reg = L2_CLS(class - CLASS_CODE_ETHERTYPE1);
	val = nr64(reg);
	if (on)
		val |= L2_CLS_VLD;
	else
		val &= ~L2_CLS_VLD;
	nw64(reg, val);

	return 0;
}

#if 0
static int tcam_user_eth_class_set(struct niu *np, unsigned long class,
				   u64 ether_type)
{
	unsigned long reg;
	u64 val;

	if (class < CLASS_CODE_ETHERTYPE1 ||
	    class > CLASS_CODE_ETHERTYPE2 ||
	    (ether_type & ~(u64)0xffff) != 0)
		return -EINVAL;

	reg = L2_CLS(class - CLASS_CODE_ETHERTYPE1);
	val = nr64(reg);
	val &= ~L2_CLS_ETYPE;
	val |= (ether_type << L2_CLS_ETYPE_SHIFT);
	nw64(reg, val);

	return 0;
}
#endif

static int tcam_user_ip_class_enable(struct niu *np, unsigned long class,
				     int on)
{
	unsigned long reg;
	u64 val;

	if (class < CLASS_CODE_USER_PROG1 ||
	    class > CLASS_CODE_USER_PROG4)
		return -EINVAL;

	reg = L3_CLS(class - CLASS_CODE_USER_PROG1);
	val = nr64(reg);
	if (on)
		val |= L3_CLS_VALID;
	else
		val &= ~L3_CLS_VALID;
	nw64(reg, val);

	return 0;
}

static int tcam_user_ip_class_set(struct niu *np, unsigned long class,
				  int ipv6, u64 protocol_id,
				  u64 tos_mask, u64 tos_val)
{
	unsigned long reg;
	u64 val;

	if (class < CLASS_CODE_USER_PROG1 ||
	    class > CLASS_CODE_USER_PROG4 ||
	    (protocol_id & ~(u64)0xff) != 0 ||
	    (tos_mask & ~(u64)0xff) != 0 ||
	    (tos_val & ~(u64)0xff) != 0)
		return -EINVAL;

	reg = L3_CLS(class - CLASS_CODE_USER_PROG1);
	val = nr64(reg);
	val &= ~(L3_CLS_IPVER | L3_CLS_PID |
		 L3_CLS_TOSMASK | L3_CLS_TOS);
	if (ipv6)
		val |= L3_CLS_IPVER;
	val |= (protocol_id << L3_CLS_PID_SHIFT);
	val |= (tos_mask << L3_CLS_TOSMASK_SHIFT);
	val |= (tos_val << L3_CLS_TOS_SHIFT);
	nw64(reg, val);

	return 0;
}

static int tcam_early_init(struct niu *np)
{
	unsigned long i;
	int err;

	tcam_enable(np, 0);
	tcam_set_lat_and_ratio(np,
			       DEFAULT_TCAM_LATENCY,
			       DEFAULT_TCAM_ACCESS_RATIO);
	for (i = CLASS_CODE_ETHERTYPE1; i <= CLASS_CODE_ETHERTYPE2; i++) {
		err = tcam_user_eth_class_enable(np, i, 0);
		if (err)
			return err;
	}
	for (i = CLASS_CODE_USER_PROG1; i <= CLASS_CODE_USER_PROG4; i++) {
		err = tcam_user_ip_class_enable(np, i, 0);
		if (err)
			return err;
	}

	return 0;
}

static int tcam_flush_all(struct niu *np)
{
	unsigned long i;

	for (i = 0; i < np->parent->tcam_num_entries; i++) {
		int err = tcam_flush(np, i);
		if (err)
			return err;
	}
	return 0;
}

static u64 hash_addr_regval(unsigned long index, unsigned long num_entries)
{
	return ((u64)index | (num_entries == 1 ?
			      HASH_TBL_ADDR_AUTOINC : 0));
}

#if 0
static int hash_read(struct niu *np, unsigned long partition,
		     unsigned long index, unsigned long num_entries,
		     u64 *data)
{
	u64 val = hash_addr_regval(index, num_entries);
	unsigned long i;

	if (partition >= FCRAM_NUM_PARTITIONS ||
	    index + num_entries > FCRAM_SIZE)
		return -EINVAL;

	nw64(HASH_TBL_ADDR(partition), val);
	for (i = 0; i < num_entries; i++)
		data[i] = nr64(HASH_TBL_DATA(partition));

	return 0;
}
#endif

static int hash_write(struct niu *np, unsigned long partition,
		      unsigned long index, unsigned long num_entries,
		      u64 *data)
{
	u64 val = hash_addr_regval(index, num_entries);
	unsigned long i;

	if (partition >= FCRAM_NUM_PARTITIONS ||
	    index + (num_entries * 8) > FCRAM_SIZE)
		return -EINVAL;

	nw64(HASH_TBL_ADDR(partition), val);
	for (i = 0; i < num_entries; i++)
		nw64(HASH_TBL_DATA(partition), data[i]);

	return 0;
}

static void fflp_reset(struct niu *np)
{
	u64 val;

	nw64(FFLP_CFG_1, FFLP_CFG_1_PIO_FIO_RST);
	udelay(10);
	nw64(FFLP_CFG_1, 0);

	val = FFLP_CFG_1_FCRAMOUTDR_NORMAL | FFLP_CFG_1_FFLPINITDONE;
	nw64(FFLP_CFG_1, val);
}

static void fflp_set_timings(struct niu *np)
{
	u64 val = nr64(FFLP_CFG_1);

	val &= ~FFLP_CFG_1_FFLPINITDONE;
	val |= (DEFAULT_FCRAMRATIO << FFLP_CFG_1_FCRAMRATIO_SHIFT);
	nw64(FFLP_CFG_1, val);

	val = nr64(FFLP_CFG_1);
	val |= FFLP_CFG_1_FFLPINITDONE;
	nw64(FFLP_CFG_1, val);

	val = nr64(FCRAM_REF_TMR);
	val &= ~(FCRAM_REF_TMR_MAX | FCRAM_REF_TMR_MIN);
	val |= (DEFAULT_FCRAM_REFRESH_MAX << FCRAM_REF_TMR_MAX_SHIFT);
	val |= (DEFAULT_FCRAM_REFRESH_MIN << FCRAM_REF_TMR_MIN_SHIFT);
	nw64(FCRAM_REF_TMR, val);
}

static int fflp_set_partition(struct niu *np, u64 partition,
			      u64 mask, u64 base, int enable)
{
	unsigned long reg;
	u64 val;

	if (partition >= FCRAM_NUM_PARTITIONS ||
	    (mask & ~(u64)0x1f) != 0 ||
	    (base & ~(u64)0x1f) != 0)
		return -EINVAL;

	reg = FLW_PRT_SEL(partition);

	val = nr64(reg);
	val &= ~(FLW_PRT_SEL_EXT | FLW_PRT_SEL_MASK | FLW_PRT_SEL_BASE);
	val |= (mask << FLW_PRT_SEL_MASK_SHIFT);
	val |= (base << FLW_PRT_SEL_BASE_SHIFT);
	if (enable)
		val |= FLW_PRT_SEL_EXT;
	nw64(reg, val);

	return 0;
}

static int fflp_disable_all_partitions(struct niu *np)
{
	unsigned long i;

	for (i = 0; i < FCRAM_NUM_PARTITIONS; i++) {
		int err = fflp_set_partition(np, 0, 0, 0, 0);
		if (err)
			return err;
	}
	return 0;
}

static void fflp_llcsnap_enable(struct niu *np, int on)
{
	u64 val = nr64(FFLP_CFG_1);

	if (on)
		val |= FFLP_CFG_1_LLCSNAP;
	else
		val &= ~FFLP_CFG_1_LLCSNAP;
	nw64(FFLP_CFG_1, val);
}

static void fflp_errors_enable(struct niu *np, int on)
{
	u64 val = nr64(FFLP_CFG_1);

	if (on)
		val &= ~FFLP_CFG_1_ERRORDIS;
	else
		val |= FFLP_CFG_1_ERRORDIS;
	nw64(FFLP_CFG_1, val);
}

static int fflp_hash_clear(struct niu *np)
{
	struct fcram_hash_ipv4 ent;
	unsigned long i;

	/* IPV4 hash entry with valid bit clear, rest is don't care.  */
	memset(&ent, 0, sizeof(ent));
	ent.header = HASH_HEADER_EXT;

	for (i = 0; i < FCRAM_SIZE; i += sizeof(ent)) {
		int err = hash_write(np, 0, i, 1, (u64 *) &ent);
		if (err)
			return err;
	}
	return 0;
}

static int fflp_early_init(struct niu *np)
{
	struct niu_parent *parent;
	unsigned long flags;
	int err;

	niu_lock_parent(np, flags);

	parent = np->parent;
	err = 0;
	if (!(parent->flags & PARENT_FLGS_CLS_HWINIT)) {
		niudbg(PROBE, "fflp_early_init: Initting hw on port %u\n",
		       np->port);
		if (np->parent->plat_type != PLAT_TYPE_NIU) {
			fflp_reset(np);
			fflp_set_timings(np);
			err = fflp_disable_all_partitions(np);
			if (err) {
				niudbg(PROBE, "fflp_disable_all_partitions "
				       "failed, err=%d\n", err);
				goto out;
			}
		}

		err = tcam_early_init(np);
		if (err) {
			niudbg(PROBE, "tcam_early_init failed, err=%d\n",
			       err);
			goto out;
		}
		fflp_llcsnap_enable(np, 1);
		fflp_errors_enable(np, 0);
		nw64(H1POLY, 0);
		nw64(H2POLY, 0);

		err = tcam_flush_all(np);
		if (err) {
			niudbg(PROBE, "tcam_flush_all failed, err=%d\n",
			       err);
			goto out;
		}
		if (np->parent->plat_type != PLAT_TYPE_NIU) {
			err = fflp_hash_clear(np);
			if (err) {
				niudbg(PROBE, "fflp_hash_clear failed, "
				       "err=%d\n", err);
				goto out;
			}
		}

		vlan_tbl_clear(np);

		niudbg(PROBE, "fflp_early_init: Success\n");
		parent->flags |= PARENT_FLGS_CLS_HWINIT;
	}
out:
	niu_unlock_parent(np, flags);
	return err;
}

static int niu_set_flow_key(struct niu *np, unsigned long class_code, u64 key)
{
	if (class_code < CLASS_CODE_USER_PROG1 ||
	    class_code > CLASS_CODE_SCTP_IPV6)
		return -EINVAL;

	nw64(FLOW_KEY(class_code - CLASS_CODE_USER_PROG1), key);
	return 0;
}

static int niu_set_tcam_key(struct niu *np, unsigned long class_code, u64 key)
{
	if (class_code < CLASS_CODE_USER_PROG1 ||
	    class_code > CLASS_CODE_SCTP_IPV6)
		return -EINVAL;

	nw64(TCAM_KEY(class_code - CLASS_CODE_USER_PROG1), key);
	return 0;
}

/* Entries for the ports are interleaved in the TCAM */
static u16 tcam_get_index(struct niu *np, u16 idx)
{
	/* One entry reserved for IP fragment rule */
	if (idx >= (np->clas.tcam_sz - 1))
		idx = 0;
	return (np->clas.tcam_top + ((idx+1) * np->parent->num_ports));
}

static u16 tcam_get_size(struct niu *np)
{
	/* One entry reserved for IP fragment rule */
	return np->clas.tcam_sz - 1;
}

static u16 tcam_get_valid_entry_cnt(struct niu *np)
{
	/* One entry reserved for IP fragment rule */
	return np->clas.tcam_valid_entries - 1;
}

static void niu_rx_skb_append(struct sk_buff *skb, struct page *page,
			      u32 offset, u32 size)
{
	int i = skb_shinfo(skb)->nr_frags;
	skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

	frag->page = page;
	frag->page_offset = offset;
	frag->size = size;

	skb->len += size;
	skb->data_len += size;
	skb->truesize += size;

	skb_shinfo(skb)->nr_frags = i + 1;
}

static unsigned int niu_hash_rxaddr(struct rx_ring_info *rp, u64 a)
{
	a >>= PAGE_SHIFT;
	a ^= (a >> ilog2(MAX_RBR_RING_SIZE));

	return (a & (MAX_RBR_RING_SIZE - 1));
}

static struct page *niu_find_rxpage(struct rx_ring_info *rp, u64 addr,
				    struct page ***link)
{
	unsigned int h = niu_hash_rxaddr(rp, addr);
	struct page *p, **pp;

	addr &= PAGE_MASK;
	pp = &rp->rxhash[h];
	for (; (p = *pp) != NULL; pp = (struct page **) &p->mapping) {
		if (p->index == addr) {
			*link = pp;
			break;
		}
	}

	return p;
}

static void niu_hash_page(struct rx_ring_info *rp, struct page *page, u64 base)
{
	unsigned int h = niu_hash_rxaddr(rp, base);

	page->index = base;
	page->mapping = (struct address_space *) rp->rxhash[h];
	rp->rxhash[h] = page;
}

static int niu_rbr_add_page(struct niu *np, struct rx_ring_info *rp,
			    gfp_t mask, int start_index)
{
	struct page *page;
	u64 addr;
	int i;

	page = alloc_page(mask);
	if (!page)
		return -ENOMEM;

	addr = np->ops->map_page(np->device, page, 0,
				 PAGE_SIZE, DMA_FROM_DEVICE);

	niu_hash_page(rp, page, addr);
	if (rp->rbr_blocks_per_page > 1)
		atomic_add(rp->rbr_blocks_per_page - 1,
			   &compound_head(page)->_count);

	for (i = 0; i < rp->rbr_blocks_per_page; i++) {
		__le32 *rbr = &rp->rbr[start_index + i];

		*rbr = cpu_to_le32(addr >> RBR_DESCR_ADDR_SHIFT);
		addr += rp->rbr_block_size;
	}

	return 0;
}

static void niu_rbr_refill(struct niu *np, struct rx_ring_info *rp, gfp_t mask)
{
	int index = rp->rbr_index;

	rp->rbr_pending++;
	if ((rp->rbr_pending % rp->rbr_blocks_per_page) == 0) {
		int err = niu_rbr_add_page(np, rp, mask, index);

		if (unlikely(err)) {
			rp->rbr_pending--;
			return;
		}

		rp->rbr_index += rp->rbr_blocks_per_page;
		BUG_ON(rp->rbr_index > rp->rbr_table_size);
		if (rp->rbr_index == rp->rbr_table_size)
			rp->rbr_index = 0;

		if (rp->rbr_pending >= rp->rbr_kick_thresh) {
			nw64(RBR_KICK(rp->rx_channel), rp->rbr_pending);
			rp->rbr_pending = 0;
		}
	}
}

static int niu_rx_pkt_ignore(struct niu *np, struct rx_ring_info *rp)
{
	unsigned int index = rp->rcr_index;
	int num_rcr = 0;

	rp->rx_dropped++;
	while (1) {
		struct page *page, **link;
		u64 addr, val;
		u32 rcr_size;

		num_rcr++;

		val = le64_to_cpup(&rp->rcr[index]);
		addr = (val & RCR_ENTRY_PKT_BUF_ADDR) <<
			RCR_ENTRY_PKT_BUF_ADDR_SHIFT;
		page = niu_find_rxpage(rp, addr, &link);

		rcr_size = rp->rbr_sizes[(val & RCR_ENTRY_PKTBUFSZ) >>
					 RCR_ENTRY_PKTBUFSZ_SHIFT];
		if ((page->index + PAGE_SIZE) - rcr_size == addr) {
			*link = (struct page *) page->mapping;
			np->ops->unmap_page(np->device, page->index,
					    PAGE_SIZE, DMA_FROM_DEVICE);
			page->index = 0;
			page->mapping = NULL;
			__free_page(page);
			rp->rbr_refill_pending++;
		}

		index = NEXT_RCR(rp, index);
		if (!(val & RCR_ENTRY_MULTI))
			break;

	}
	rp->rcr_index = index;

	return num_rcr;
}

static int niu_process_rx_pkt(struct napi_struct *napi, struct niu *np,
			      struct rx_ring_info *rp)
{
	unsigned int index = rp->rcr_index;
	struct sk_buff *skb;
	int len, num_rcr;

	skb = netdev_alloc_skb(np->dev, RX_SKB_ALLOC_SIZE);
	if (unlikely(!skb))
		return niu_rx_pkt_ignore(np, rp);

	num_rcr = 0;
	while (1) {
		struct page *page, **link;
		u32 rcr_size, append_size;
		u64 addr, val, off;

		num_rcr++;

		val = le64_to_cpup(&rp->rcr[index]);

		len = (val & RCR_ENTRY_L2_LEN) >>
			RCR_ENTRY_L2_LEN_SHIFT;
		len -= ETH_FCS_LEN;

		addr = (val & RCR_ENTRY_PKT_BUF_ADDR) <<
			RCR_ENTRY_PKT_BUF_ADDR_SHIFT;
		page = niu_find_rxpage(rp, addr, &link);

		rcr_size = rp->rbr_sizes[(val & RCR_ENTRY_PKTBUFSZ) >>
					 RCR_ENTRY_PKTBUFSZ_SHIFT];

		off = addr & ~PAGE_MASK;
		append_size = rcr_size;
		if (num_rcr == 1) {
			int ptype;

			off += 2;
			append_size -= 2;

			ptype = (val >> RCR_ENTRY_PKT_TYPE_SHIFT);
			if ((ptype == RCR_PKT_TYPE_TCP ||
			     ptype == RCR_PKT_TYPE_UDP) &&
			    !(val & (RCR_ENTRY_NOPORT |
				     RCR_ENTRY_ERROR)))
				skb->ip_summed = CHECKSUM_UNNECESSARY;
			else
				skb->ip_summed = CHECKSUM_NONE;
		}
		if (!(val & RCR_ENTRY_MULTI))
			append_size = len - skb->len;

		niu_rx_skb_append(skb, page, off, append_size);
		if ((page->index + rp->rbr_block_size) - rcr_size == addr) {
			*link = (struct page *) page->mapping;
			np->ops->unmap_page(np->device, page->index,
					    PAGE_SIZE, DMA_FROM_DEVICE);
			page->index = 0;
			page->mapping = NULL;
			rp->rbr_refill_pending++;
		} else
			get_page(page);

		index = NEXT_RCR(rp, index);
		if (!(val & RCR_ENTRY_MULTI))
			break;

	}
	rp->rcr_index = index;

	skb_reserve(skb, NET_IP_ALIGN);
	__pskb_pull_tail(skb, min(len, VLAN_ETH_HLEN));

	rp->rx_packets++;
	rp->rx_bytes += skb->len;

	skb->protocol = eth_type_trans(skb, np->dev);
	skb_record_rx_queue(skb, rp->rx_channel);
	napi_gro_receive(napi, skb);

	return num_rcr;
}

static int niu_rbr_fill(struct niu *np, struct rx_ring_info *rp, gfp_t mask)
{
	int blocks_per_page = rp->rbr_blocks_per_page;
	int err, index = rp->rbr_index;

	err = 0;
	while (index < (rp->rbr_table_size - blocks_per_page)) {
		err = niu_rbr_add_page(np, rp, mask, index);
		if (err)
			break;

		index += blocks_per_page;
	}

	rp->rbr_index = index;
	return err;
}

static void niu_rbr_free(struct niu *np, struct rx_ring_info *rp)
{
	int i;

	for (i = 0; i < MAX_RBR_RING_SIZE; i++) {
		struct page *page;

		page = rp->rxhash[i];
		while (page) {
			struct page *next = (struct page *) page->mapping;
			u64 base = page->index;

			np->ops->unmap_page(np->device, base, PAGE_SIZE,
					    DMA_FROM_DEVICE);
			page->index = 0;
			page->mapping = NULL;

			__free_page(page);

			page = next;
		}
	}

	for (i = 0; i < rp->rbr_table_size; i++)
		rp->rbr[i] = cpu_to_le32(0);
	rp->rbr_index = 0;
}

static int release_tx_packet(struct niu *np, struct tx_ring_info *rp, int idx)
{
	struct tx_buff_info *tb = &rp->tx_buffs[idx];
	struct sk_buff *skb = tb->skb;
	struct tx_pkt_hdr *tp;
	u64 tx_flags;
	int i, len;

	tp = (struct tx_pkt_hdr *) skb->data;
	tx_flags = le64_to_cpup(&tp->flags);

	rp->tx_packets++;
	rp->tx_bytes += (((tx_flags & TXHDR_LEN) >> TXHDR_LEN_SHIFT) -
			 ((tx_flags & TXHDR_PAD) / 2));

	len = skb_headlen(skb);
	np->ops->unmap_single(np->device, tb->mapping,
			      len, DMA_TO_DEVICE);

	if (le64_to_cpu(rp->descr[idx]) & TX_DESC_MARK)
		rp->mark_pending--;

	tb->skb = NULL;
	do {
		idx = NEXT_TX(rp, idx);
		len -= MAX_TX_DESC_LEN;
	} while (len > 0);

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		tb = &rp->tx_buffs[idx];
		BUG_ON(tb->skb != NULL);
		np->ops->unmap_page(np->device, tb->mapping,
				    skb_shinfo(skb)->frags[i].size,
				    DMA_TO_DEVICE);
		idx = NEXT_TX(rp, idx);
	}

	dev_kfree_skb(skb);

	return idx;
}

#define NIU_TX_WAKEUP_THRESH(rp)		((rp)->pending / 4)

static void niu_tx_work(struct niu *np, struct tx_ring_info *rp)
{
	struct netdev_queue *txq;
	u16 pkt_cnt, tmp;
	int cons, index;
	u64 cs;

	index = (rp - np->tx_rings);
	txq = netdev_get_tx_queue(np->dev, index);

	cs = rp->tx_cs;
	if (unlikely(!(cs & (TX_CS_MK | TX_CS_MMK))))
		goto out;

	tmp = pkt_cnt = (cs & TX_CS_PKT_CNT) >> TX_CS_PKT_CNT_SHIFT;
	pkt_cnt = (pkt_cnt - rp->last_pkt_cnt) &
		(TX_CS_PKT_CNT >> TX_CS_PKT_CNT_SHIFT);

	rp->last_pkt_cnt = tmp;

	cons = rp->cons;

	niudbg(TX_DONE, "%s: niu_tx_work() pkt_cnt[%u] cons[%d]\n",
	       np->dev->name, pkt_cnt, cons);

	while (pkt_cnt--)
		cons = release_tx_packet(np, rp, cons);

	rp->cons = cons;
	smp_mb();

out:
	if (unlikely(netif_tx_queue_stopped(txq) &&
		     (niu_tx_avail(rp) > NIU_TX_WAKEUP_THRESH(rp)))) {
		__netif_tx_lock(txq, smp_processor_id());
		if (netif_tx_queue_stopped(txq) &&
		    (niu_tx_avail(rp) > NIU_TX_WAKEUP_THRESH(rp)))
			netif_tx_wake_queue(txq);
		__netif_tx_unlock(txq);
	}
}

static inline void niu_sync_rx_discard_stats(struct niu *np,
					     struct rx_ring_info *rp,
					     const int limit)
{
	/* This elaborate scheme is needed for reading the RX discard
	 * counters, as they are only 16-bit and can overflow quickly,
	 * and because the overflow indication bit is not usable as
	 * the counter value does not wrap, but remains at max value
	 * 0xFFFF.
	 *
	 * In theory and in practice counters can be lost in between
	 * reading nr64() and clearing the counter nw64().  For this
	 * reason, the number of counter clearings nw64() is
	 * limited/reduced though the limit parameter.
	 */
	int rx_channel = rp->rx_channel;
	u32 misc, wred;

	/* RXMISC (Receive Miscellaneous Discard Count), covers the
	 * following discard events: IPP (Input Port Process),
	 * FFLP/TCAM, Full RCR (Receive Completion Ring) RBR (Receive
	 * Block Ring) prefetch buffer is empty.
	 */
	misc = nr64(RXMISC(rx_channel));
	if (unlikely((misc & RXMISC_COUNT) > limit)) {
		nw64(RXMISC(rx_channel), 0);
		rp->rx_errors += misc & RXMISC_COUNT;

		if (unlikely(misc & RXMISC_OFLOW))
			dev_err(np->device, "rx-%d: Counter overflow "
				"RXMISC discard\n", rx_channel);

		niudbg(RX_ERR, "%s-rx-%d: MISC drop=%u over=%u\n",
		       np->dev->name, rx_channel, misc, misc-limit);
	}

	/* WRED (Weighted Random Early Discard) by hardware */
	wred = nr64(RED_DIS_CNT(rx_channel));
	if (unlikely((wred & RED_DIS_CNT_COUNT) > limit)) {
		nw64(RED_DIS_CNT(rx_channel), 0);
		rp->rx_dropped += wred & RED_DIS_CNT_COUNT;

		if (unlikely(wred & RED_DIS_CNT_OFLOW))
			dev_err(np->device, "rx-%d: Counter overflow "
				"WRED discard\n", rx_channel);

		niudbg(RX_ERR, "%s-rx-%d: WRED drop=%u over=%u\n",
		       np->dev->name, rx_channel, wred, wred-limit);
	}
}

static int niu_rx_work(struct napi_struct *napi, struct niu *np,
		       struct rx_ring_info *rp, int budget)
{
	int qlen, rcr_done = 0, work_done = 0;
	struct rxdma_mailbox *mbox = rp->mbox;
	u64 stat;

#if 1
	stat = nr64(RX_DMA_CTL_STAT(rp->rx_channel));
	qlen = nr64(RCRSTAT_A(rp->rx_channel)) & RCRSTAT_A_QLEN;
#else
	stat = le64_to_cpup(&mbox->rx_dma_ctl_stat);
	qlen = (le64_to_cpup(&mbox->rcrstat_a) & RCRSTAT_A_QLEN);
#endif
	mbox->rx_dma_ctl_stat = 0;
	mbox->rcrstat_a = 0;

	niudbg(RX_STATUS, "%s: niu_rx_work(chan[%d]), stat[%llx] qlen=%d\n",
	       np->dev->name, rp->rx_channel, (unsigned long long) stat, qlen);

	rcr_done = work_done = 0;
	qlen = min(qlen, budget);
	while (work_done < qlen) {
		rcr_done += niu_process_rx_pkt(napi, np, rp);
		work_done++;
	}

	if (rp->rbr_refill_pending >= rp->rbr_kick_thresh) {
		unsigned int i;

		for (i = 0; i < rp->rbr_refill_pending; i++)
			niu_rbr_refill(np, rp, GFP_ATOMIC);
		rp->rbr_refill_pending = 0;
	}

	stat = (RX_DMA_CTL_STAT_MEX |
		((u64)work_done << RX_DMA_CTL_STAT_PKTREAD_SHIFT) |
		((u64)rcr_done << RX_DMA_CTL_STAT_PTRREAD_SHIFT));

	nw64(RX_DMA_CTL_STAT(rp->rx_channel), stat);

	/* Only sync discards stats when qlen indicate potential for drops */
	if (qlen > 10)
		niu_sync_rx_discard_stats(np, rp, 0x7FFF);

	return work_done;
}

static int niu_poll_core(struct niu *np, struct niu_ldg *lp, int budget)
{
	u64 v0 = lp->v0;
	u32 tx_vec = (v0 >> 32);
	u32 rx_vec = (v0 & 0xffffffff);
	int i, work_done = 0;

	niudbg(INTR, "%s: niu_poll_core() v0[%016llx]\n",
	       np->dev->name, (unsigned long long) v0);

	for (i = 0; i < np->num_tx_rings; i++) {
		struct tx_ring_info *rp = &np->tx_rings[i];
		if (tx_vec & (1 << rp->tx_channel))
			niu_tx_work(np, rp);
		nw64(LD_IM0(LDN_TXDMA(rp->tx_channel)), 0);
	}

	for (i = 0; i < np->num_rx_rings; i++) {
		struct rx_ring_info *rp = &np->rx_rings[i];

		if (rx_vec & (1 << rp->rx_channel)) {
			int this_work_done;

			this_work_done = niu_rx_work(&lp->napi, np, rp,
						     budget);

			budget -= this_work_done;
			work_done += this_work_done;
		}
		nw64(LD_IM0(LDN_RXDMA(rp->rx_channel)), 0);
	}

	return work_done;
}

static int niu_poll(struct napi_struct *napi, int budget)
{
	struct niu_ldg *lp = container_of(napi, struct niu_ldg, napi);
	struct niu *np = lp->np;
	int work_done;

	work_done = niu_poll_core(np, lp, budget);

	if (work_done < budget) {
		napi_complete(napi);
		niu_ldg_rearm(np, lp, 1);
	}
	return work_done;
}

static void niu_log_rxchan_errors(struct niu *np, struct rx_ring_info *rp,
				  u64 stat)
{
	dev_err(np->device, PFX "%s: RX channel %u errors ( ",
		np->dev->name, rp->rx_channel);

	if (stat & RX_DMA_CTL_STAT_RBR_TMOUT)
		printk("RBR_TMOUT ");
	if (stat & RX_DMA_CTL_STAT_RSP_CNT_ERR)
		printk("RSP_CNT ");
	if (stat & RX_DMA_CTL_STAT_BYTE_EN_BUS)
		printk("BYTE_EN_BUS ");
	if (stat & RX_DMA_CTL_STAT_RSP_DAT_ERR)
		printk("RSP_DAT ");
	if (stat & RX_DMA_CTL_STAT_RCR_ACK_ERR)
		printk("RCR_ACK ");
	if (stat & RX_DMA_CTL_STAT_RCR_SHA_PAR)
		printk("RCR_SHA_PAR ");
	if (stat & RX_DMA_CTL_STAT_RBR_PRE_PAR)
		printk("RBR_PRE_PAR ");
	if (stat & RX_DMA_CTL_STAT_CONFIG_ERR)
		printk("CONFIG ");
	if (stat & RX_DMA_CTL_STAT_RCRINCON)
		printk("RCRINCON ");
	if (stat & RX_DMA_CTL_STAT_RCRFULL)
		printk("RCRFULL ");
	if (stat & RX_DMA_CTL_STAT_RBRFULL)
		printk("RBRFULL ");
	if (stat & RX_DMA_CTL_STAT_RBRLOGPAGE)
		printk("RBRLOGPAGE ");
	if (stat & RX_DMA_CTL_STAT_CFIGLOGPAGE)
		printk("CFIGLOGPAGE ");
	if (stat & RX_DMA_CTL_STAT_DC_FIFO_ERR)
		printk("DC_FIDO ");

	printk(")\n");
}

static int niu_rx_error(struct niu *np, struct rx_ring_info *rp)
{
	u64 stat = nr64(RX_DMA_CTL_STAT(rp->rx_channel));
	int err = 0;


	if (stat & (RX_DMA_CTL_STAT_CHAN_FATAL |
		    RX_DMA_CTL_STAT_PORT_FATAL))
		err = -EINVAL;

	if (err) {
		dev_err(np->device, PFX "%s: RX channel %u error, stat[%llx]\n",
			np->dev->name, rp->rx_channel,
			(unsigned long long) stat);

		niu_log_rxchan_errors(np, rp, stat);
	}

	nw64(RX_DMA_CTL_STAT(rp->rx_channel),
	     stat & RX_DMA_CTL_WRITE_CLEAR_ERRS);

	return err;
}

static void niu_log_txchan_errors(struct niu *np, struct tx_ring_info *rp,
				  u64 cs)
{
	dev_err(np->device, PFX "%s: TX channel %u errors ( ",
		np->dev->name, rp->tx_channel);

	if (cs & TX_CS_MBOX_ERR)
		printk("MBOX ");
	if (cs & TX_CS_PKT_SIZE_ERR)
		printk("PKT_SIZE ");
	if (cs & TX_CS_TX_RING_OFLOW)
		printk("TX_RING_OFLOW ");
	if (cs & TX_CS_PREF_BUF_PAR_ERR)
		printk("PREF_BUF_PAR ");
	if (cs & TX_CS_NACK_PREF)
		printk("NACK_PREF ");
	if (cs & TX_CS_NACK_PKT_RD)
		printk("NACK_PKT_RD ");
	if (cs & TX_CS_CONF_PART_ERR)
		printk("CONF_PART ");
	if (cs & TX_CS_PKT_PRT_ERR)
		printk("PKT_PTR ");

	printk(")\n");
}

static int niu_tx_error(struct niu *np, struct tx_ring_info *rp)
{
	u64 cs, logh, logl;

	cs = nr64(TX_CS(rp->tx_channel));
	logh = nr64(TX_RNG_ERR_LOGH(rp->tx_channel));
	logl = nr64(TX_RNG_ERR_LOGL(rp->tx_channel));

	dev_err(np->device, PFX "%s: TX channel %u error, "
		"cs[%llx] logh[%llx] logl[%llx]\n",
		np->dev->name, rp->tx_channel,
		(unsigned long long) cs,
		(unsigned long long) logh,
		(unsigned long long) logl);

	niu_log_txchan_errors(np, rp, cs);

	return -ENODEV;
}

static int niu_mif_interrupt(struct niu *np)
{
	u64 mif_status = nr64(MIF_STATUS);
	int phy_mdint = 0;

	if (np->flags & NIU_FLAGS_XMAC) {
		u64 xrxmac_stat = nr64_mac(XRXMAC_STATUS);

		if (xrxmac_stat & XRXMAC_STATUS_PHY_MDINT)
			phy_mdint = 1;
	}

	dev_err(np->device, PFX "%s: MIF interrupt, "
		"stat[%llx] phy_mdint(%d)\n",
		np->dev->name, (unsigned long long) mif_status, phy_mdint);

	return -ENODEV;
}

static void niu_xmac_interrupt(struct niu *np)
{
	struct niu_xmac_stats *mp = &np->mac_stats.xmac;
	u64 val;

	val = nr64_mac(XTXMAC_STATUS);
	if (val & XTXMAC_STATUS_FRAME_CNT_EXP)
		mp->tx_frames += TXMAC_FRM_CNT_COUNT;
	if (val & XTXMAC_STATUS_BYTE_CNT_EXP)
		mp->tx_bytes += TXMAC_BYTE_CNT_COUNT;
	if (val & XTXMAC_STATUS_TXFIFO_XFR_ERR)
		mp->tx_fifo_errors++;
	if (val & XTXMAC_STATUS_TXMAC_OFLOW)
		mp->tx_overflow_errors++;
	if (val & XTXMAC_STATUS_MAX_PSIZE_ERR)
		mp->tx_max_pkt_size_errors++;
	if (val & XTXMAC_STATUS_TXMAC_UFLOW)
		mp->tx_underflow_errors++;

	val = nr64_mac(XRXMAC_STATUS);
	if (val & XRXMAC_STATUS_LCL_FLT_STATUS)
		mp->rx_local_faults++;
	if (val & XRXMAC_STATUS_RFLT_DET)
		mp->rx_remote_faults++;
	if (val & XRXMAC_STATUS_LFLT_CNT_EXP)
		mp->rx_link_faults += LINK_FAULT_CNT_COUNT;
	if (val & XRXMAC_STATUS_ALIGNERR_CNT_EXP)
		mp->rx_align_errors += RXMAC_ALIGN_ERR_CNT_COUNT;
	if (val & XRXMAC_STATUS_RXFRAG_CNT_EXP)
		mp->rx_frags += RXMAC_FRAG_CNT_COUNT;
	if (val & XRXMAC_STATUS_RXMULTF_CNT_EXP)
		mp->rx_mcasts += RXMAC_MC_FRM_CNT_COUNT;
	if (val & XRXMAC_STATUS_RXBCAST_CNT_EXP)
		mp->rx_bcasts += RXMAC_BC_FRM_CNT_COUNT;
	if (val & XRXMAC_STATUS_RXBCAST_CNT_EXP)
		mp->rx_bcasts += RXMAC_BC_FRM_CNT_COUNT;
	if (val & XRXMAC_STATUS_RXHIST1_CNT_EXP)
		mp->rx_hist_cnt1 += RXMAC_HIST_CNT1_COUNT;
	if (val & XRXMAC_STATUS_RXHIST2_CNT_EXP)
		mp->rx_hist_cnt2 += RXMAC_HIST_CNT2_COUNT;
	if (val & XRXMAC_STATUS_RXHIST3_CNT_EXP)
		mp->rx_hist_cnt3 += RXMAC_HIST_CNT3_COUNT;
	if (val & XRXMAC_STATUS_RXHIST4_CNT_EXP)
		mp->rx_hist_cnt4 += RXMAC_HIST_CNT4_COUNT;
	if (val & XRXMAC_STATUS_RXHIST5_CNT_EXP)
		mp->rx_hist_cnt5 += RXMAC_HIST_CNT5_COUNT;
	if (val & XRXMAC_STATUS_RXHIST6_CNT_EXP)
		mp->rx_hist_cnt6 += RXMAC_HIST_CNT6_COUNT;
	if (val & XRXMAC_STATUS_RXHIST7_CNT_EXP)
		mp->rx_hist_cnt7 += RXMAC_HIST_CNT7_COUNT;
	if (val & XRXMAC_STATUS_RXOCTET_CNT_EXP)
		mp->rx_octets += RXMAC_BT_CNT_COUNT;
	if (val & XRXMAC_STATUS_CVIOLERR_CNT_EXP)
		mp->rx_code_violations += RXMAC_CD_VIO_CNT_COUNT;
	if (val & XRXMAC_STATUS_LENERR_CNT_EXP)
		mp->rx_len_errors += RXMAC_MPSZER_CNT_COUNT;
	if (val & XRXMAC_STATUS_CRCERR_CNT_EXP)
		mp->rx_crc_errors += RXMAC_CRC_ER_CNT_COUNT;
	if (val & XRXMAC_STATUS_RXUFLOW)
		mp->rx_underflows++;
	if (val & XRXMAC_STATUS_RXOFLOW)
		mp->rx_overflows++;

	val = nr64_mac(XMAC_FC_STAT);
	if (val & XMAC_FC_STAT_TX_MAC_NPAUSE)
		mp->pause_off_state++;
	if (val & XMAC_FC_STAT_TX_MAC_PAUSE)
		mp->pause_on_state++;
	if (val & XMAC_FC_STAT_RX_MAC_RPAUSE)
		mp->pause_received++;
}

static void niu_bmac_interrupt(struct niu *np)
{
	struct niu_bmac_stats *mp = &np->mac_stats.bmac;
	u64 val;

	val = nr64_mac(BTXMAC_STATUS);
	if (val & BTXMAC_STATUS_UNDERRUN)
		mp->tx_underflow_errors++;
	if (val & BTXMAC_STATUS_MAX_PKT_ERR)
		mp->tx_max_pkt_size_errors++;
	if (val & BTXMAC_STATUS_BYTE_CNT_EXP)
		mp->tx_bytes += BTXMAC_BYTE_CNT_COUNT;
	if (val & BTXMAC_STATUS_FRAME_CNT_EXP)
		mp->tx_frames += BTXMAC_FRM_CNT_COUNT;

	val = nr64_mac(BRXMAC_STATUS);
	if (val & BRXMAC_STATUS_OVERFLOW)
		mp->rx_overflows++;
	if (val & BRXMAC_STATUS_FRAME_CNT_EXP)
		mp->rx_frames += BRXMAC_FRAME_CNT_COUNT;
	if (val & BRXMAC_STATUS_ALIGN_ERR_EXP)
		mp->rx_align_errors += BRXMAC_ALIGN_ERR_CNT_COUNT;
	if (val & BRXMAC_STATUS_CRC_ERR_EXP)
		mp->rx_crc_errors += BRXMAC_ALIGN_ERR_CNT_COUNT;
	if (val & BRXMAC_STATUS_LEN_ERR_EXP)
		mp->rx_len_errors += BRXMAC_CODE_VIOL_ERR_CNT_COUNT;

	val = nr64_mac(BMAC_CTRL_STATUS);
	if (val & BMAC_CTRL_STATUS_NOPAUSE)
		mp->pause_off_state++;
	if (val & BMAC_CTRL_STATUS_PAUSE)
		mp->pause_on_state++;
	if (val & BMAC_CTRL_STATUS_PAUSE_RECV)
		mp->pause_received++;
}

static int niu_mac_interrupt(struct niu *np)
{
	if (np->flags & NIU_FLAGS_XMAC)
		niu_xmac_interrupt(np);
	else
		niu_bmac_interrupt(np);

	return 0;
}

static void niu_log_device_error(struct niu *np, u64 stat)
{
	dev_err(np->device, PFX "%s: Core device errors ( ",
		np->dev->name);

	if (stat & SYS_ERR_MASK_META2)
		printk("META2 ");
	if (stat & SYS_ERR_MASK_META1)
		printk("META1 ");
	if (stat & SYS_ERR_MASK_PEU)
		printk("PEU ");
	if (stat & SYS_ERR_MASK_TXC)
		printk("TXC ");
	if (stat & SYS_ERR_MASK_RDMC)
		printk("RDMC ");
	if (stat & SYS_ERR_MASK_TDMC)
		printk("TDMC ");
	if (stat & SYS_ERR_MASK_ZCP)
		printk("ZCP ");
	if (stat & SYS_ERR_MASK_FFLP)
		printk("FFLP ");
	if (stat & SYS_ERR_MASK_IPP)
		printk("IPP ");
	if (stat & SYS_ERR_MASK_MAC)
		printk("MAC ");
	if (stat & SYS_ERR_MASK_SMX)
		printk("SMX ");

	printk(")\n");
}

static int niu_device_error(struct niu *np)
{
	u64 stat = nr64(SYS_ERR_STAT);

	dev_err(np->device, PFX "%s: Core device error, stat[%llx]\n",
		np->dev->name, (unsigned long long) stat);

	niu_log_device_error(np, stat);

	return -ENODEV;
}

static int niu_slowpath_interrupt(struct niu *np, struct niu_ldg *lp,
			      u64 v0, u64 v1, u64 v2)
{

	int i, err = 0;

	lp->v0 = v0;
	lp->v1 = v1;
	lp->v2 = v2;

	if (v1 & 0x00000000ffffffffULL) {
		u32 rx_vec = (v1 & 0xffffffff);

		for (i = 0; i < np->num_rx_rings; i++) {
			struct rx_ring_info *rp = &np->rx_rings[i];

			if (rx_vec & (1 << rp->rx_channel)) {
				int r = niu_rx_error(np, rp);
				if (r) {
					err = r;
				} else {
					if (!v0)
						nw64(RX_DMA_CTL_STAT(rp->rx_channel),
						     RX_DMA_CTL_STAT_MEX);
				}
			}
		}
	}
	if (v1 & 0x7fffffff00000000ULL) {
		u32 tx_vec = (v1 >> 32) & 0x7fffffff;

		for (i = 0; i < np->num_tx_rings; i++) {
			struct tx_ring_info *rp = &np->tx_rings[i];

			if (tx_vec & (1 << rp->tx_channel)) {
				int r = niu_tx_error(np, rp);
				if (r)
					err = r;
			}
		}
	}
	if ((v0 | v1) & 0x8000000000000000ULL) {
		int r = niu_mif_interrupt(np);
		if (r)
			err = r;
	}
	if (v2) {
		if (v2 & 0x01ef) {
			int r = niu_mac_interrupt(np);
			if (r)
				err = r;
		}
		if (v2 & 0x0210) {
			int r = niu_device_error(np);
			if (r)
				err = r;
		}
	}

	if (err)
		niu_enable_interrupts(np, 0);

	return err;
}

static void niu_rxchan_intr(struct niu *np, struct rx_ring_info *rp,
			    int ldn)
{
	struct rxdma_mailbox *mbox = rp->mbox;
	u64 stat_write, stat = le64_to_cpup(&mbox->rx_dma_ctl_stat);

	stat_write = (RX_DMA_CTL_STAT_RCRTHRES |
		      RX_DMA_CTL_STAT_RCRTO);
	nw64(RX_DMA_CTL_STAT(rp->rx_channel), stat_write);

	niudbg(INTR, "%s: rxchan_intr stat[%llx]\n",
	       np->dev->name, (unsigned long long) stat);
}

static void niu_txchan_intr(struct niu *np, struct tx_ring_info *rp,
			    int ldn)
{
	rp->tx_cs = nr64(TX_CS(rp->tx_channel));

	niudbg(INTR, "%s: txchan_intr cs[%llx]\n",
	       np->dev->name, (unsigned long long) rp->tx_cs);
}

static void __niu_fastpath_interrupt(struct niu *np, int ldg, u64 v0)
{
	struct niu_parent *parent = np->parent;
	u32 rx_vec, tx_vec;
	int i;

	tx_vec = (v0 >> 32);
	rx_vec = (v0 & 0xffffffff);

	for (i = 0; i < np->num_rx_rings; i++) {
		struct rx_ring_info *rp = &np->rx_rings[i];
		int ldn = LDN_RXDMA(rp->rx_channel);

		if (parent->ldg_map[ldn] != ldg)
			continue;

		nw64(LD_IM0(ldn), LD_IM0_MASK);
		if (rx_vec & (1 << rp->rx_channel))
			niu_rxchan_intr(np, rp, ldn);
	}

	for (i = 0; i < np->num_tx_rings; i++) {
		struct tx_ring_info *rp = &np->tx_rings[i];
		int ldn = LDN_TXDMA(rp->tx_channel);

		if (parent->ldg_map[ldn] != ldg)
			continue;

		nw64(LD_IM0(ldn), LD_IM0_MASK);
		if (tx_vec & (1 << rp->tx_channel))
			niu_txchan_intr(np, rp, ldn);
	}
}

static void niu_schedule_napi(struct niu *np, struct niu_ldg *lp,
			      u64 v0, u64 v1, u64 v2)
{
	if (likely(napi_schedule_prep(&lp->napi))) {
		lp->v0 = v0;
		lp->v1 = v1;
		lp->v2 = v2;
		__niu_fastpath_interrupt(np, lp->ldg_num, v0);
		__napi_schedule(&lp->napi);
	}
}

static irqreturn_t niu_interrupt(int irq, void *dev_id)
{
	struct niu_ldg *lp = dev_id;
	struct niu *np = lp->np;
	int ldg = lp->ldg_num;
	unsigned long flags;
	u64 v0, v1, v2;

	if (netif_msg_intr(np))
		printk(KERN_DEBUG PFX "niu_interrupt() ldg[%p](%d) ",
		       lp, ldg);

	spin_lock_irqsave(&np->lock, flags);

	v0 = nr64(LDSV0(ldg));
	v1 = nr64(LDSV1(ldg));
	v2 = nr64(LDSV2(ldg));

	if (netif_msg_intr(np))
		printk("v0[%llx] v1[%llx] v2[%llx]\n",
		       (unsigned long long) v0,
		       (unsigned long long) v1,
		       (unsigned long long) v2);

	if (unlikely(!v0 && !v1 && !v2)) {
		spin_unlock_irqrestore(&np->lock, flags);
		return IRQ_NONE;
	}

	if (unlikely((v0 & ((u64)1 << LDN_MIF)) || v1 || v2)) {
		int err = niu_slowpath_interrupt(np, lp, v0, v1, v2);
		if (err)
			goto out;
	}
	if (likely(v0 & ~((u64)1 << LDN_MIF)))
		niu_schedule_napi(np, lp, v0, v1, v2);
	else
		niu_ldg_rearm(np, lp, 1);
out:
	spin_unlock_irqrestore(&np->lock, flags);

	return IRQ_HANDLED;
}

static void niu_free_rx_ring_info(struct niu *np, struct rx_ring_info *rp)
{
	if (rp->mbox) {
		np->ops->free_coherent(np->device,
				       sizeof(struct rxdma_mailbox),
				       rp->mbox, rp->mbox_dma);
		rp->mbox = NULL;
	}
	if (rp->rcr) {
		np->ops->free_coherent(np->device,
				       MAX_RCR_RING_SIZE * sizeof(__le64),
				       rp->rcr, rp->rcr_dma);
		rp->rcr = NULL;
		rp->rcr_table_size = 0;
		rp->rcr_index = 0;
	}
	if (rp->rbr) {
		niu_rbr_free(np, rp);

		np->ops->free_coherent(np->device,
				       MAX_RBR_RING_SIZE * sizeof(__le32),
				       rp->rbr, rp->rbr_dma);
		rp->rbr = NULL;
		rp->rbr_table_size = 0;
		rp->rbr_index = 0;
	}
	kfree(rp->rxhash);
	rp->rxhash = NULL;
}

static void niu_free_tx_ring_info(struct niu *np, struct tx_ring_info *rp)
{
	if (rp->mbox) {
		np->ops->free_coherent(np->device,
				       sizeof(struct txdma_mailbox),
				       rp->mbox, rp->mbox_dma);
		rp->mbox = NULL;
	}
	if (rp->descr) {
		int i;

		for (i = 0; i < MAX_TX_RING_SIZE; i++) {
			if (rp->tx_buffs[i].skb)
				(void) release_tx_packet(np, rp, i);
		}

		np->ops->free_coherent(np->device,
				       MAX_TX_RING_SIZE * sizeof(__le64),
				       rp->descr, rp->descr_dma);
		rp->descr = NULL;
		rp->pending = 0;
		rp->prod = 0;
		rp->cons = 0;
		rp->wrap_bit = 0;
	}
}

static void niu_free_channels(struct niu *np)
{
	int i;

	if (np->rx_rings) {
		for (i = 0; i < np->num_rx_rings; i++) {
			struct rx_ring_info *rp = &np->rx_rings[i];

			niu_free_rx_ring_info(np, rp);
		}
		kfree(np->rx_rings);
		np->rx_rings = NULL;
		np->num_rx_rings = 0;
	}

	if (np->tx_rings) {
		for (i = 0; i < np->num_tx_rings; i++) {
			struct tx_ring_info *rp = &np->tx_rings[i];

			niu_free_tx_ring_info(np, rp);
		}
		kfree(np->tx_rings);
		np->tx_rings = NULL;
		np->num_tx_rings = 0;
	}
}

static int niu_alloc_rx_ring_info(struct niu *np,
				  struct rx_ring_info *rp)
{
	BUILD_BUG_ON(sizeof(struct rxdma_mailbox) != 64);

	rp->rxhash = kzalloc(MAX_RBR_RING_SIZE * sizeof(struct page *),
			     GFP_KERNEL);
	if (!rp->rxhash)
		return -ENOMEM;

	rp->mbox = np->ops->alloc_coherent(np->device,
					   sizeof(struct rxdma_mailbox),
					   &rp->mbox_dma, GFP_KERNEL);
	if (!rp->mbox)
		return -ENOMEM;
	if ((unsigned long)rp->mbox & (64UL - 1)) {
		dev_err(np->device, PFX "%s: Coherent alloc gives misaligned "
			"RXDMA mailbox %p\n", np->dev->name, rp->mbox);
		return -EINVAL;
	}

	rp->rcr = np->ops->alloc_coherent(np->device,
					  MAX_RCR_RING_SIZE * sizeof(__le64),
					  &rp->rcr_dma, GFP_KERNEL);
	if (!rp->rcr)
		return -ENOMEM;
	if ((unsigned long)rp->rcr & (64UL - 1)) {
		dev_err(np->device, PFX "%s: Coherent alloc gives misaligned "
			"RXDMA RCR table %p\n", np->dev->name, rp->rcr);
		return -EINVAL;
	}
	rp->rcr_table_size = MAX_RCR_RING_SIZE;
	rp->rcr_index = 0;

	rp->rbr = np->ops->alloc_coherent(np->device,
					  MAX_RBR_RING_SIZE * sizeof(__le32),
					  &rp->rbr_dma, GFP_KERNEL);
	if (!rp->rbr)
		return -ENOMEM;
	if ((unsigned long)rp->rbr & (64UL - 1)) {
		dev_err(np->device, PFX "%s: Coherent alloc gives misaligned "
			"RXDMA RBR table %p\n", np->dev->name, rp->rbr);
		return -EINVAL;
	}
	rp->rbr_table_size = MAX_RBR_RING_SIZE;
	rp->rbr_index = 0;
	rp->rbr_pending = 0;

	return 0;
}

static void niu_set_max_burst(struct niu *np, struct tx_ring_info *rp)
{
	int mtu = np->dev->mtu;

	/* These values are recommended by the HW designers for fair
	 * utilization of DRR amongst the rings.
	 */
	rp->max_burst = mtu + 32;
	if (er.
 *
 * Copy> 4096)
	ver.
 *
 * Copyriler ;
}

static int niu_alloc_tx_ethe_info(strucinux/ *np,
				  linux/ih>
#include  *rp)
{
	BUILD_BUG_ON(sizeof<linux/itxdma_mailbox) != 64);
iver.
 box = np->ops->odule.coherent(ethtdevice#inclu	   g.h>
#include <linux/netdevivice.h>
#i&ude <lin_dma, GFP_KERNEL)2007, 2!ude <lin(daveeturn -ENOMEM2007, 2(unsigned long)ude <linu& (64UL - 1)) {
		dev_errnux/etherdev PFX "%s: Cude <li odule gives misalnclude"incl"TXDMA /netdev %p\n",/ethtdev->name, x/bitops.;h>
#include INVAL;
	}nclude descrux/ethtool.h>
#include <linux/etherdevice.h>
#i MAX_TX_RING_SIZE *include __le64ce.h>
#incllude <e <lix/delay.h>
#include <linux/bie <li.h>
#include <linux/mii.h>
#include <linux/ie <liner.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linue <lintableh>
#include <linux/log2.h>
iu.h"
ude <linux/jiffies.h>
#includpending =<linux/io.h>

#if;MODULE_rod = 0ELDATE cons";

MODULE_wrap_bipyri0incl/* XXX maketunese configur cha...vemlodriver.
 *rk_freq =2.h>
_VERSION/ 4inclux/mset_ *
 * Cop(npg2.h#incluinclud0/

#include voidnux/mg.h>_rbr<linux/init.h>
#e <linuxrpci.h>
#include <linu16 bssinclbsHOR(min(PAGE_SHIFT, 15#include rbr_block4BIT_ = 1 <<ndif

(void __iomem s_per_pag)
{
	retudq
static u-bsseadq(void __ig.h>s[0] = 256((u64) readic voi1 wri10242007, 2ude <linught > ETH_DATA_LENde <liswitchl(reg + IZEde <licase 4 *m *re:incl4 val, void __2 wri)
 */
			breakincl	defaultif

static struct pci_de819 200 niu_pci_		}
	} elsee <litatic struct pci_de2048.h>
#E_DEVICE_TABLE(3 wrivoid __iomem *reg)/

#include <linux/module.channels<linux/init.h>
 <linlinux/init_pa.h>
#*al)		wrx/ethtal)		w2007nt first_rxdq(np->r,g))

#dtfine nr64 + (regi, port, errincls + al), np-ors + ))

#define nr64 =mac(reg)		readq(n";

MODfor (i (dav i <gs + ; i++de <li, val)	writeq((va+= al)		w->rxq(np) | ((ort[i])},
, np->mac_regs + p_off + (ret))
#define nw64_ipp(
#inethtnumdefiether _off + (reg))
#define nw64 nw6ipp()

#defih>
#inc_pcs(reg)		regs + np->ipp_ofnp->pcss_off <linureal_+ (reg)queue_pcsoff + (reg))
#deval), npne nr64_pcskzodulewritedefine nr64_pef CONFIGT_MASK	0x00000fffffce.h>
ude   ay.h>
#include erinuxe <linux/mii.h!cs(reg)		rea(davgoto out/if_val)

#define nr64_i + np->xpcs_off eg)		readq(T_MASK	0x00000fffffffff = &cs(reg)		rea4_ippULE_DEVnu_denpude <s(reg)iteq((val), np->gs + np->ipp idebugegs + ux/module.0x00000fffffSION(DRV_M	07, 2er"

#d NIU_MSG_DEFAULT "GPL")IT_MASK
ION(DRV_MOD@davemlobetter [] = {
s,
MODULE_DESCR, etcIPTION("NIU g = -1onsyn_window .h>
le_param
#definthresholn";
aram(cr_c cha*reg)
-o(TYPE, f, efine niuinfo(TYPE, f, .) \
do {	if ((np)->msg_enable & NETIF_MSG_##TYmsg_pkt\
do {	if ((n1e_id p)->msg_eimeoune n_pciE_DEVICE_kick\
do {	 = RBR_REFILL_MINbg(TYPE,F_MSG_##TYPE) \
		pr<X_TIMEOUT			(5)) | (((u6 a...F_MSG_##TYPE) \
		prin
#define niu_lock_paren(debug, "NIU de __ifill & NETIlay.h>
#include TYPE, f, a...ULE_VEREFAUL(reg))

#eg))
#definnp->regs + np->xeg))
#def (reg))
#define h>
#include g, val)	writeq((val), np->regs + np->xpcs_off + (regeg))
#deine NIU_MSG_DEFAULT (NETIF_MSG_DRV | NETIF_Mfine nr64| NETIF_MSG_LINK)/pci.h>
#include_debug;
eg))
#dent debug = -1;
module_parammac_regs + (reg, val)	writeq((vSC(debug, "NIU debug leh>
#include <ne niudbg(TYPE, f, a...) \
do {	if ((>
#incLE_VERSIO
SG_DEFA:("GPL"freedq(np->regnudbg(ck_irqrestor
#include <linux/m)		rs_sng_poll
#define DMA_44BI(regq(np->r <lin(reglimer (d100avem@while (--

	nw6> 0de <liu64 va+ (rnr64(TX_CS(ame)
{
	dbg(TYPE,ts_c& c(np,_Sh>

TATE_unlock_irqr
MOD_tblinclude <lDEV	u64 bits, int limit, in(np->r_stop				const char *reg_name)
{
	int t_bits_clear_mac(np, reg, bits, 
	ts_c|=y);
	if TOP_N_GOcs_ow_mac(np, reg, bit,its_V_MODULE_VERimit, int delay,
		44BIame)
{
		u64 bits, int limit, int re);
My,
					const char *reg_name)
{
	int err;

	nw64_mac(reg, bits);
	err = __niu_wait_bits_clear_mac(np, reg, bits, limit,!, delay);
	ifRST)dev_err(np->device, PFX "%s: bits (%llx) of register %s "
			"wo LIMInot clear, val[%llx]\n",
			np->dev->name, (unsigned long long) b err;EFAULT s, reg_name,
RSTned long long) nr64_mac(reg));
	g, "NIU de BITS, LIMIT, DEL_set_and_wait_T, DEL f, a.. long loo.h>
KICK) nr64_mac(0V_MODULE_VER				u64 bits, int limit, in
			"wol((u6_ining reg,
				     u64 bits, int limit, intval),long loLOG_MASK1;
}

static intelay,
					coVALchar *reg_name)
{
	int err;
	nst 2har *reg_name)
{
	int err;
	u64 |= bits;
	nw64_ipp(reg, val)q
staRELOchar *reg_name)
{
	int err;
	ipp(np, r	err = __niu_wait_bits_clear_ipp(nHDL;
}

static int _ts_c = (u64)w64_mac(retu);
	if (err)VLD_FUNCatic u;	u64 val y);
	if (err),
		q
st0 | val[%llx]\n",
		q
st1it, delay);
	if (err)VLD) nr64_mac(reg));
	davemlo <linu32ler mode?ION("NIUODULE_VERSION);

#ifndef DMA_44txc_ennable nw6				const char *reg_nonnp->de#include <li flar64_dev->nam, masci_tbg, vmem *al)		w}
	if	__nie)
{ts_clear_mac(C_CONTROlude it_c		"would	retuw64_mac(reg7, 2onde <lis, reg_na); \
})

_ENABLE |ait_cle}
};

MODULE dela= ~ u64 bimii.h> delay~signed long reg,
	) == 0 a...it, int signed long reg,
	evice,lay,
		); \
})

c(reg));r_ippun(NP, REG, BITS, LIMIT, DBITS, LIMIT, DELAY, REG);
Miit_c
#define DMA_44BIt_bi niu_0 || DELAY < 0); \
	__niu_set_and_lear_ipp(NP, REG, BITS, LIMIT, DELAY, REG_NAME);INT
	val, DELAY,);

		ifclear(NP	u64write nw6, DELAY,unsi niu_ar, vaIMIT, DELAY); atic u\
})

stat);
	}
	if (limit < 0)
		return -ENODEV;
	return 0;
}

#de nw6x/deG_NAME)	BUILD_BUG_ON(LIMIT <= 0 || D_bits_cleavem@(struct niu mac_rdebug				     u64 bits, int limit, int delanr64(regunsi_wait_bital = nr64_m.)		readq(n);
				break;
		PORT_DMA\
})

statc(reg));
#include <linux/migne_oneg)		readq(n
#define DMA_44BIT_MASK	/pci.h>
#include <lin>= 0) {et_and_wak_irqsa)		readq(np->et_and_wa#incllen)
			break;
		udel
			"would n
	if (limit < 0)
		 f, a.._niu_set_andu_set_and_wait_clear(NPd long, BITS, LIMIT, DELAY, REG_NAME) \
({	BUILD_BUG_ON(LIMIT <= 0 p, unsigned, BITS, LIMIT, DELAY, REG_NAME) \
({	BUIbreak;
		DMA_MAX) nr64_mac(er.
 *
 * Cope)
{
	int erElearS0;
}

static int _7, 2008 x/of_devi(--lEV;
	NG_CFIerr)
DDR_BASE | val)	writT_ARM;

	nw64(LDG_ude <linux/if_vlan.h>
#include <linuTX
#defng long) %#includelinuaddr (%llx) is no
#innclud.#inca...ude <linux/log2ne nr64_a...>
#include <lie <linata =
	DRVx/de_MODULE_NAME ".c:v" DRV_MO/* The length field inm), val);
}
sk_rmeasure_IM0_64-byteet d  niu_l. ");
MODULE_LIistune number of*np,e <liiptors in1(ldnour
#def, 8 D_IMs each, thus we divide byal &= ~bimorM1(ldnto geptune properits_unet)" chip wantrnet drivedefine 		"w);
MODULE_LICE8) bits, r= ((_in_ldg(sr, valRM;

	nw6LENatic u)(lp-)	write;

	if (ldn < 64)ENODEV;
	M;

	nw) nr64_mac(reg));
	ii.h>2008 Dinux/de >> 32)(--limnp, sBH_MBatic  |arent =((u32nux/if_et LDG_IMGg_map[i]L!= lp->lint niu_ldn_irq_enable(struct niu *np, int ldn, int on)
{
	uMBOXgned long mask_rhas illegal bitsu64 val;

	if (ldn < 0 || ldn > LDN_MAX)
		return -EINVAL;

	;

		if _MODULE_NAME ".c:v" DRV_;

	for map[i] uct niu_ldg *lp,

		if (parent i;

	for _ldn_irrr;

		err = niu_enable_ay);_ldn_irq_enablet delay,
			ng) nr64_mac( int __p->lastE, f,c(val)avem@ULE_VERSION);

#ifndef DMA_44,
			rdc_groupegs + (reg))
#define nw64(reg, v_encc chas *t{
		u64 vff + (regencode(u_cfg[
})

staipp(mac_regac(reg)nablenum = tp->{
	return (val ULT (NETIF_MSG_DRV |(pornt li (port delay)
{
	while pe << (po *tb_ldg&(poro_wait4_ipp(r(regthisnt limireak;
		urn (val >y);
	hile (slott_bits_cleTPUTine nrval >< NIU_RDC_Tg,
	_SLOTSIF_FRA);
	if lay,
UT_TABL(--limit > ,F_FRAg, val)	writbl(reg conclear(N[TPUT]ts (%l>ldg[i]DEFPUT_ "
			"would
}

static u32 p[] = {
(u32 val, -ENODEV;
	return 0;
}
,
			drr_weigh{	BUILD_BUG_ON(L	(unsignetyp 0) phy_decode\
})

f + (re		   phyuct niustatic LD_BUG_ON(Litel(val devde <lx4UL)isterTYPE_10Gif

ts_clePT_DRR_WEIGHT_DEFAULTEAD_FRAMiu_pci_tbOUTPUT, MDIO_REA_OP([] = {
	{PCort, dev));
	return mdio_waitnp);
}

statp->ldg[i]ev));
	ru *np, unsild not clear, val[%llx]\n",
			hostude <linux/init.h>
fine nw64(reg, val)		writeq((val), np->regs + 	return (type << (port * 2));static u32 phy_decode(u32 val, int port)
d londefialne nwiunp->palt_ned ed long(reg))

#de	int limi> (port * 2)) & PORT_TYPEg, "NIU deIMIT,rimary_mac int mii_rITS, Latic int mii_r,  longDELAY, REG_NAME) \
({	BUILD_BUG_ON();
MOulticm(npIF_FRAME_OUTPUT, MII_READ_OP(port, reg));
	return mdio_wait(np);
(NETIF_MSG_DRV |  err;
t delay)
{
}

static int r;

IF_FRAME_OUTPUT, Mt)
{
	retD_OP(port, reg)) \
	spin_unlock_irqrestore(&npULE_VERSION);

#ifnd<linux/m(debug, ined long reg,
				     u64 bits, int limreturn err;a));
nd_wait_cleae & NER	}
	f
	nwchar *reg_nincludeI_PLU_ESR2_DEV_4_ip,_mac(el), ESR2_TI_PLL"U_ESR2_DEV_"t clear, val[%llx]\n"np, unsignep, unsigned long reg,
					u64 bits, int limit, int delay,
R				const char *reg_name)
{
	int	 val >u64 val;

	val = nr64_ipp	 val >> 16 |= bits;
	nw64_ipp(retic int esigned long channel, u32 val)
ipp(np, reg, bits, limit, delaywrite(np, np->point err;

	err = mdio_write(np, np%llx) of register %s "
			"would not clear, write(np, np,
			np->dev->name, (unsiort, NIU_ESR2_DEbits, rePLL_RX_CFG_H(channel long longort, NIU_ESR2_D) nr64_mac(reg));
	return SION);

#ifndef DMA_44np, unsignewredsigned long reg,
					T_MASK	0x00000fffffffffffULL
D_BUG_ON(Liu_ldg *would(0)

#define niuin->poDC_REhannRA_WIparent *parent = npcfg, rx_cfg;
	uns
do {	if (ong i;

	tx_cfg THRtatic u_CFG_ENTX | PLL_TX_CFG	unsigned long i;

	tx_cfg = (PLY(PLL_TX_CFG_ENTX | PLL_TX_CFGING_1375MV);
	rx_cfg = (PLL_RX_CFG_E PLL_RX_Crn err;
}
i;

	tx_cfg, ## aX "%s: bitld not clear, val[%llx]\n"comput_MASK_cfig_b#define nw64_xpcs(relude(NP, R*ret;

	nw64(reg, bits);DEV_";

MODitel(valvoid __iomem *reg)RAME_OUTPU;
}
#endif

ESR2_TI_PBR_BLK2, r_4K	rx_cBR

	nw6B PLL_RX_CENRX |nt data)
{
	x4UL)8FG_ENTEST;
		rx_cfg |= PLL_RX_C8G_ENTEST;
	}

	/* Initialize all 4 lanes of the16FG_ENTEST;
		rx_cfg |= PLL_RX_C16G_ENTEST;
	}

	/* Initialize all 4 lanes of the32FG_ENTEST;
		rx_cfg |= PLL_RX_C32G_ENTEST;
	}

	/* Initialize all 4 lanes o[] = {
	{PCniu_ldg *lp = &np->l(err)
	EST;
	}

	/VLD 200G_L, test_cfg);
ruct pciRAME_OUTPUx_cfg(np, i, rx_cfg);
		iUFSZ2_			return err;
	}

	int lize all 4 lanes of theCFG_ENTEST;
		rx_cfg |= P
	int FG_ENTEST;
	}

	/*u64 uninitialized_var(sig), m SERDES.  */
	for (i = 0;
	int  i++) {
		int err u64 uninitialized_var(sig), mg);
		if (err)
			return e
	int 
	for (i = 0; i < 4u64 uninitialized_var(snt serdes_init_niu_1g_serdes(struct niu *np)
{
	struct1niu_link_config *lp = &np1>link_confi1X_CFG_ENRX | PLL_RX_CFG_TERM1_1VDDT |
		  PLL_RX_CFG1ninitialized_var(sig), mg;
	u16 pll_cfg, pll_sts;
	in1 max_retry = 100;
	u64 ) {
		u16 test_cfg = PLL_ask, val;
	u32 tx_cfg, rx_cfg1
	unsigned long i;
	int) {
		u16 test_cfg = PLL_CFG_ENTX | PLL_TX_CFG_SWING_1175MV |
		  PLL_TX_CFG_R) {
		u16 test_cfg nt serdes_init_niu_1g_serdes(struct niu *np)
{
	structT_CFG_L, test_cfg);
ic void link_config56NRX | PLL_RX_CFG_TERM0_256DDT |
		  PLL_RX_CFG0ninitialized_var(sig), m512_CFG_L, pll_cfg);
	if (512) {
		dev_err(np->device, PFX "NIU Port %d "
_LP_ADAPTIVE;

	if (lp->loopb0ck_mode == LOOPBACK_PHYvice, PFX "NIU Port %d "
g;
	u16 pll_cfg, pll_sts;
	in0 max_retry = 100;
	u64 vice, PFX "NIU Portnt serdes_init_niu_1g_serdes(strI_PLL_TESUG_ON esr2_set_tx_cfg(struct niu *_NAME) 		u16 test				const char *reg_name)
{
	int err;

	nw64(reg, bar_maU_ESR2_DEV_ADDR,
			mit >= 0

	nwts);
	err =T;
		rx_cf_TX_CFG_L(chENG P;

Mlimit, int  SERDES.  */
	forrr;
}

ESR2_DEV_ADDR,
			 (reg));
	

	nw64_mac(re, bits);
	err = __niu_wai{
	wr err;
	}

	udelay(200);

 &e SERDES.  */QST
	if xabcd)},
udelay(1me)
{}));
	r

	nw6<l = nr6 PFX "%s: bits ( esr2_set_tx_cfg(struct niu *,
			np->	"mdio write to ESR2_TI_PLLT_MASK	0x00000fffffffffffULLigned long long) nr64(r		u16 test		return err;
ags)
#definIMIT <= 0 || DELAY < 0); \
	__niu_set_and_wait_clear(NP, REG, BITS, np->port, NIU_ESR2_DME); \
})

static void niu_ldg_rearm(struu_10g_fiber(struct niu *ION(DRV_MODrr;
}

/np, = (u64) lp->timer;{
		dev_err(np_EST;EMPTYrn err;
}

/ESR2_TLrr)
	ADDR,
			 ESTX | Pe not "
			"[%0_MEXparent = n(int) (sig & masRCR_CFGS (int) val);
		return -ENODEV;O (int) val);
		return -ENODal bits [rn err;
}

_tx_cfg(np, i, tx_cf= niu_enable_ldn_in_ldg(np,ct niu_lin |= bits;
	nt err;

		if (& 0x0x_retryf= 100c0p)
{
	strucST;
	}

A8x]\n", np->port, fg, rx_cfS;

enable & NEENTEST;
	}

, reg_ENRX | PLL_RX_Cst_cfg);
LDG_IMtialized_va64(LDG_IMGMT(lr;

	tx_cfg static ip->regs + PBACK_CML_DIS;

		mdio_IU_E&lay);
	DELAY, REG_NAME) \
({	Buninitialized_vB) nr64_mac(reg)); val) {CRzed_var(sig), mask, val;
	unsignedsg_enable & NE->po	if (lp- = (PLL_TX_CFG_ENTX | PLL_|= LDG_IM

	if (lp-;
	rx_cfg = (PLLrite(np, np->poLL_RX_VE);

	if (lpRX_CFG_EQ_Lmode == LOOPBACK_PHY), f, a...) \
dst_cfg = PLB_P;
	}
_ENRX | PLL_RX_CST;
		rx_cENTOUTCFG_ENTX |LOOPBACK_PHY) _enableNTEST;
		rx_cTIMEOUTloopback_ml;
		break;
s: "
			"mdio writ_set_and_wa, reg));
	return mdio_wait(np);
ninitialirn 0;
}

staticigned loindecludDELAY, REG_NAe not "
			"[%08x]\n", ntic int __n_10g_serdes(struct niu *n%08x] are not "
			"[%08x]\n", npiber.  */
static int serdes_iDY0_P0 | ESR_
	default:egs + (reg))
#define DELAY < 0); \
	__niu_set_aseen";
jiffies_(TYPEigned lonit_bi_ipp(NP, REG, BITS, LIMIT, DEx] are not "
K_DIVuct niu *np, in	}

	rmem *l |= bV_MODu32 txED_RAN_INIT, des_init_niu_OPMOD (PL(, NIU&10g_serdes: "
VALigned long reg,
				    u64 bits, i

#definect niset_and_wait_clear_ippc u32 phy_encode(u32d long
	nw64(MIF_FRAME_OUTd lonl;
		break;
dev, reg));
	ern 0;
}ta)
{
	int err;

	nw64(MIF_FRAME_OUTPUT, NETIF_MSG_PROBE | NETIF_MSG_LINK)

static int niu_debug;
static int debug+) {
		err = esINT_DET0_P0);
	ne niudbg(TYPE, f, a...0;
}

static int esr2_set_tx_cfg(struct niu *fine p_frag_ruhar *reg_name)
{
(np);
	if (err < 0)
		return err;

	nw64(MIF_FRAME_OUTPUTclassifier *c{
		u64 v  ESF_FRAME_OUTPUTtcam_entryt * int portrr) (reg))
#dSR_IN = c
	u6_P0_to    _OP(port, dev,   E[SR_IN debu/* Notnet)aptune no clealer M1_MASKsameIM0_both ipv4 and1(ldnipv6 format TCAM CH3 iernet drivmemlongtp, 0,include *tpd "
	(porkey_iomem= (E_V4KEY1_NOiste |
		     _it_c  ESR_INT_XSRDY_P1 |
		       Eassoc_dataldg INT_XASSOCfff, T|= PUSE_OFFSEializal)	 for 10on)
{     ESR_INT_X |
		 loopback_moegs +    ESwritt(np);
R_INT_		     max_retryINT_XGN_ENA | PLL_RX_CFG_LOS_LTHRturn -EINVAP_P1_CL;
	}

	while (max_reP_P1_CH2 |GN_ENA | PLL_RX_CFG_LOS_LTHR(porvali
do {s of      ES		pr__CH3 ies++ailed", np->port);
		return err;
	}

  ESR_INT__hw_P0_BITS;
		val = (ESR_INT_SRDY0_P0 |
		       ESR_INT_DET0_P0 |
		       ESR_INT_XSRDY_P0 |
		      np);
	if (_PLL_CFG_H1POLY,	    h1r = eAPTIVE);
H2erdes(np);
2	if (!eri++) {
		err = esr2_set_tx_cfg(np, i, tx_cfg);
		if (err)
			return err;
ENET_VLANMIF__NUM* InRIESt delay)
{
	while elayvlanr <  *v{
		uX "Nd \n"mapp nr64_mac(red \n"tbstru
	}

	whi (err < 0), val)	writev
			retupref,_ctrlint numV;
}

st

#define nr64_iX "Nn err;

IF_Frn -ENODRDES "
				"Link FailealtIF_FRAM *aport);
		err;

	err = mdifg);
		if (err)
		a));
	err = mdio_wait(np);aR_RXTX_CTRLnumvice.h>	
		e, unsig);
		eIF_Fct n if serdes is ready */

	switch (n

#defineCLASS_CODE1_CHR_PROG1nr64_>= 0)
			*valSCTP_IPV6t delay)
{
   ESR_IN = i -= 0)
			*val |= ((err &if (err >= 0) {
		   ESkey(np);
	i_CH1 |
		    lon    ESR if serdes is ready */

	switct, reg, data));flow long chan, u32 *valDR,
			E err;

	err = mdio_read(np, np->port,X "Nerr >= 0) {
		_INT_SIGNALS__cfg(np, i, tx_cfg);
		if (err)P_P0_CHOUTPUT, M lon0:
		val = (ESR_INT_SRDY0_P0 |zcprn 0;
}			const char *reg_nile (maSR2_D0);
	<linlay,
ZCP_RAMffff,0, H2 |
			
			"ser
}

static i1t esr_r1ad_reset(struct niu *n2t esr_r2ad_reset(struct niu *n3t esr_r3ad_reset(struct niu *n4t esr_r4ad_reset(struct niBE, = 0) {
		*_STS__reset(struct niACCtest_cfg)rr = mdio_r_WRITT(lp-NTX | PXDP_Pp->port, NIUZFCID_ENRX | PLL_RX_CFnp->port,SEL2_DEFO\
})

stat		ESR_RXTX_RESET
			*valoopback));
	return err;p, npuct p->port, NIrr = mdio_reSR_RXTX_RESETBUSY ESR2_TI_),
			 v int>= 0) {
			*val |= ((eread0xffff) << 16);
			err = 0;
		}
	}
	return>= 0) {
		uCFG_TERM_0
static int esr_write_rxtx_ctrl(struct niu *np, unsigned ong chan, u3np, i, txe <linux/if_vlan.h>
#include <linuZCP rr = busy won't ->por,includerr = mdio_r[ng m]#include <linux/logLDN_MAX)
		return -EINVAL4; i+rr = mdio_rts, lick_irqrestore(&np-
		err = mdio_read(np, np->port, NIUREADDEV_ADDR,
				ESR_RXTX_RESET_CTRL_H);
		if (err >= 0) {
			*val |= ((err & 0xffff) << 16);
			err = 0;
		}
	}
	retXTX_CTRL_L(chan), val & 0xffff);
	if (!err)
		err = mdio_write(np, np->port, NIU_ESR_DEV_ADDR,
				 ESR_RXTX_CTRL_H(chan), (val >> 16))2;
	return err;
}

static int esr_write_glue0(struct niu *np, unsigned long chan, u32 val)
{
	int err;

	err = mdio_writeesr_reaturn err
}

static in NIUu32 *va NIU_ESR_DEV_ADDR,
	 longdio_rea NIU_ESR_DEV_ADDR,
	in_ldDEV_ADD NIU_ESR_DEV_ADDR,
	3y(200);

4err = mdio_write(np, 
#inclustatic int serdes_init_niu_1 ((ecfifoed long reg,
				    >port);
		return err;E	def |= () bits, reg_ite(np, np-4_ip\
})

static 	"serdee(np, np-LP_ADAPTIr)
			returnREG, BITS,SR_DEV_ADDR,
			 ESR_RXTX_RESET_CTRL_H, 0x0000);
	ifESR_INT_SRDY0_P0 | ESR_zc not clear, val[np->dev->esr_r5], rbuf[5 int port)
maNT_XDP_P0_C
	writeio_wait(nlat_ dev,!= PLAMDIO_RENIU = 0; i < 4not cleaal = ||t_bits_clODEV1
	if ma

stATLAS_P0_P1V_ADDR,G/1G SERt, N(i = 0atic int serde2_P3it_10g(struct niu
};

M

	structOUTPt_10g(struct ninp->port, NIeturR_RXTX_RES_reg, i;
r)
		_reg, i;
	err _reg, i;
NIU_Eavem@

#define nr64_imaxITE_OP(port, reg, dat ((err & 0np);
	i0);
	}

 = mdio_read(np, np->port, NIU_ESR_DE

	err = np);
	i, PFags) \
	spin_unlock_irqrestore(&np-	if (err)
		return ERDES. longt_10g(sCio_read(strucme)
{
	int
}

clear)
	(strucault:
		_ALffff)(ef D, u32 val)
ault:
			break;

	defaultnst return -EIDELAY
	}
	c, on);

	return 0;
}

static u32p(err & 0xffff) << 16);
			err = 0;
		}
	}
	returnt_bits_clear_m2 |
(IPP2_DEV i < np->_EMPH_0_SHIFTFG_L  | _0_SHIFT_DADDR,PIO_Wfff);
		_EMPH_0_S_1_SHIWR_PTRwhile (
		    (0x5 << ENET_SERDEnt esr_read_reset(0x5 << ENET_SERDEp, u32 *val)
{
	in0x5 << ENET_SERDE= mdio_read(np, np0x5 << ENET_SERDESR_DEV_ADDR,
			ES0x5 << ENET_SERDECTRL_L);
	if (err  (0x5 << ENET_SERDE& ~CTRL_EMPH_1_SHIFT) |
		_SERDES_CTRL_SDET_2 |
	rr = mdio_write(np, np->port, NIU_ESR_DEV_ADDRSHIFT) |
		    (0x1RDS_CTRL_EMPH_2_S->port, NIU_ES_PHY) {
		test_cf		 ESR_RXTX_RESET_C_PHY) {
		test_cf);
	if (err)
		retu_PHY) {
		test_cfay(200);

	err = md_PHY) {
		test_cfnp->port, NIU_ESR_D_PHY) {
		test_cf
#inESR_INT_SRDY0_P0 | test_urn err;
	udelay(200);
	err = mdio_write(np, np->por_EMPH44BI << ENET_SCTRL_EMPHSOFThanneice.h>
#ong chan,, "_0_SHIFT
		err = mdio_write(np,
			EMPHset != 0) {
		dev_err(np->device, PFX "FG_L Port %u ESR_RESET "
			"did not clear [%08x]\n",
			np->port, reset);
		return -ENODEV;
	}

	return 0;
}

static int serdes_ini_1_SHIstruct niu *np)
{
	struct niu_link_co= esr_read_glue0link_config;
	unsig= esr_read_glueeg, test_cfg_reg, i;
	u64 ctrl_val, test_cfg_val, sig, mask, val;
	int err;

	switch (np->port) {
	caseET_2 |
		    ET_SERDES_0_CTRL_));
	test_cfg
		break;
	caseX "Nrl_val = (E_EMPH_0_SERDES_CTRL_S ESR_GLUE_CTRL0_BLTIME);
		gluei++) {
		err HIFT) |
	_cfg(np, i, tx_cfg);
		if (err) ESR_GLUE_CTRL0_BLTIPKT_DISlue0 |= (ESR_GLUE_CTRL0_BAD
	ifCNglue0 |= (ESR_GLUE_CTRL0_ECCB |
	 |= (ESR_GLUE_CTRL0_RXLOSENAB |
	CTRL_LADJ_2_SMNET_  (0x sigDET_1 |
ERDES_CTRL_EMPH_0_SHIFT) |4(reg);

CTRL_EMPHIP str_THCname, (unsi(np, i, gluP reg,
			
		CTRL_EMPH_1_SHIECC_ENr = esr_reset(nRO00_CYCLRCr = esr_reset(CKS 10G/r = e(0x1= 10= (P(np, i, glue0);
		iloopback_mode = (0x5 << ENET_SERD_1 |
		    ENET_SERDES_CTRL_SDET_2handle_le= mdio_write(np, np->pncluusnt limit, int 	return err;mac(XMA); \
IFT) |
	ii.h>turn	__ni &unsigFLAGSEAD_ice.h0 &&num)
		NT_XDP_P0_CH2 |
		    FIBERESR_IN = 0; i < T_XSRDYe <liuct niu _CH3 |
		  _L	tx_OLARI "
		64(reg);

ESR_INT_SIGNFORCENALS_ONG PF
};

MODULE	mask = ESR_INT_SIGN1 |
		       ESRal = (ESR_INT_SRDY0_PALS_P1_BITS;
		v	{}
ctrl(np,P_P0_CH3 |
		  t);
	if (err)
		reerr;

	nw64(MIxif_xP_P0P0_BITS;
		val = (ESR_INT_SRDYlink_MODULE *l{
		u64 v
		return -		return err;
	"did noDP_P0_CH2 |
		    XCVR_SERDESt niu *np,urn errMIF |
		     P1 |
		  lags &= ~N_ATCA_Gs))
 = ENElags &= ~NLP_ADAPTINT_X ESR_INT_XDP_P0_CH3 |
		     al = (ESR_INT_SRDY0_P*valPOR_CLK_SRC bits, reg_ESR_INT_SIGNTX_OUTPUT/
	forr;
	}
}

	oopback__waiODEVLOOPBACK_MACt niu *np,08x] are not "
			"[%08x]\n", np-		mask = ESR_INT_SIGNA (np->f bits, int limit, int OTPLUG_PHY_PRESENT;
	retal) {
		if (np->flags & NIU_F   ES
}

static int serdes_iniFSNT_Sbits))
	s, int limit,  = ESR_INT_SIGNA	val &= ~ENETT, DELAH1 |
		       ESR_INT_XDP_P0__XDP_ADDR
		val |= ENET_SERDES_PLLAGS_HOTPLUG
	if (err)
	ESR_INT_SIGN1G_PCS_BYPAS niu *np)
{
	_FLAGS_HOTPLUG_PHY)
2:
		val |= ENETp->devicRATE2;
		break;
	0G_X		val |= ENEl);
		retuactive_sp NIU_= SPEED_an, IU_FLAGS_HOTPLUG_PHY_P*val LK_25MHZfor (i = 0; i < 4;LL_CFG, val);

	return 0;
}_XDP_P1_CH2 |
		       ESR_INT>device, PFX "Port %u signal bits [%08x] are not "
		"mdCTRL_g)
{
	write
	u64 val;

	val = nr64(ENET_SE		break;
	case  testXGMII bits, int lim:
		return -EINVAL;
	}
	nw64(ENET= nr64(regval, sig, mask, val;	int er_SERDES_PLL_Hval, sig, mask, val;int errniu *np)
{
	struct niu_link_con_XDP_P1_CH1 |
		       ESR_IbT_XDP_P1_CH0);
		break;

	default:
		return -EINVAL;
	}

	if ((sig & mask) != val)ts_cleBCH3 XLUG_PHY_PRask), (int) val);
		return -ENODEV;
	}
	if (np->flags IU_FLAGS_Hest_cfg_reg = ENMIIPRESENT;
	re*np)
{
ET_SERDE;
	case 1:
		reset_val =  ENETlt:
		return -EINVAL;
	}
	nw64(ENETT_SERDES_1_Pest_cfg_reg = EN	int, valET_SERDES_RESET_1;
		ctrl_reg = ENET_SERDES_1its [%08x](est_cfg_reg = ENLINKCH3 r = eG;
		pll_cfg = ENALS_P1_BITS;     ESR_
		val |= ENET_SERDES_PL   ES_XDP_P0_
		val |= ENET_SERDES_PLL_HRATE0;
	I_PLreturn -EINVAL;
	}
	nw64(ENET_SERDES_1_Pest_cfg_reg = ENrn 0;_CLOENET_SERDES_RESET_1;
		ctrl_reg = ENERDES_CTRL_EM_XDP_P1_CH2est_cfg_reg = ESERDES_PLL_FBDIV0;
	switch (np->porDP_P1_CH0);
		break; {
		if (np->flags & NIU_FLL_CFG;
	       ESR_INT_XDd long*np)
{
tch (np->port) {
	d lon_XDP_P1_CH1 |
		   pcs_miiFT) |
				 (ENET_TEST_MD_Perr;

	nw64_mac(reNET_SERDES_CTRL_pcs(		vaT_SECTffff)T_SERDEg_val = 0;
4_ipp(reg);st_cfg_val = 0;
LP_ADAPTI bits);;
	err = _H0);
&&t, delayoopback_mode ==e(np, ir)
			ret		 ESdevice, PFX t_cfg_val = 0;

	if}<< ENET_SERDES_CTRL_xLADJHIFT) |
		    (0x1 << ENET_SERDES_CTRL_LADJ_3_SHIFT));
	tesDES_(break \
})

 longT_SERDESERDES_TEST_M_TRL_H LOOPBACENET_SERDES_TEST_M_cfg_val |= ((ENET_TEST_MD_PAD_LOOPBACK <|
				 (ENET_TEST_MRDES_TEST_MD_0_SHIFT) |
				 (ENEENET_SERDES_TEST_MD_2_ACK <<
				 cfg_val);

	/NET_P_P1_CH0);
		break;

	default:
		return -EINVAL;
	}

	if ((sig & mask) != val)itel(valll_cfg, i;
	(2 |
		       (lp->ldg_nu  ESR_INT_XDP_P;
	mdelay(2000);

	/*T_SERDES_PLL_AME_OUTPU  ESR_INT_XDP_Pif

/* 1G fi}

	hile OPBACK_PHY) {|
		,<<
		|
		CTRL_ | esr_read_
		u64 			returET_TEST_MDPATH, val;
		bre_1_SHLADJ_2_SHIFT) d long
}

static int 2 |
		       :e0);
		if (err)
			r | (i = 0; i < 4; i++rn err;

		rxtx_ctrl &= ~(ESR_RLAGS_HOTPLUi++) {
	0G HOTPLUtx_ctr_1 |
		    ENET_SERDES_CTRL_ |
		s ready */

jiffies.hTRETCH |
	cop 0;
oru32 rxtx_ctrevice, PFX "Port %u signal bitNET_SERDES_PLL_HRATE3;
		break;
	defautrl, gl
{
	struct niu_link_confi_1_SHDES_TEST_MD< 4; i+;
	val_rd = nr64(ENET_SERDES_RESETset_val, rn -ENODEV;
	}
	if (np->flPHY_HRATE1;
		br
				 (ENET_TRESENT;
	re_SERDES_PLL_HRATE2UE_CTRL0_BLTIME_SHIFT));
AD_LOOPBACK <<
				  ENET_SERDEx_ctrl);
		if (errDESKEW_ERRS << err;
		 ESR_GLUE_CTENET_SERDESYMnp, i, 00_THCN		if (err)
			return err;
	}


	2np->p
}

statiRL_VMUXLO);
		rxtXTX_CTRL_ENSTRETCH 
			      (2 <err = esr_read_glue0(np, l, glue0;

		err = esr_read_rxtx_ctrl(np, i, &rxtx_ctrl);
		if (err)
			return err;
		}

static int 0i++) {
		uTE |
		driv0:
		val = (ESR_INT_SRDY0_rl &= ~(ESR_RXTX_CTRL) {
		uR	int  Initix_ctrl, glue0;

		
			return e
			"[%08x]\n"al =r;
		err = esr_read_glue0(np, i, &glue0) {
		dev_err(np->device, PFX "Nesr2_set_tx_cfg(struct niu *nLIMITtxINT_XDP_P1_CH0);
		break;AD_LOOPBACK <<
				  ENET_SERD   (0x, XT_CH3 SW(ctrl_reg, ct(fig;
	int lin_REG_R

	ree.h>
#i val;
	u16 curnw64(ctrce.h>
#incrl_val);
	nwfig;
	int lin
		err = mdio_write(np,niu *np,) {
	case 0:
		reset_val Initialize aPH_2_SHIFTig;
	int link ave(&np->lockET, res;if (err)
			return err;
	}

	for H0);
		break;! 4; i_irqsave(&np->lockt_rxflags);

	val = nr64cfg);
		if (err)
			retturn err;
	}

	swit0);
		brnux/if_vlan.h>
#include <Purn %u*np,est_ wouldreg, LID;
r;
}

stave(&np->lockr_write_glal;

	ift esr_rea unsigned long chan, u32 TUS) {
		link_up = _MODULE_NAME "	case 0:int link_status_1g_serdes(struct niu *np,ent_duplex = DUPLEX_INVASERDES_CTRL_LADJ_0_SHIFT) |
		   return err; niu *np, int ES_CTRL_LADJ_ &np->link_config;
	    (0x1 << ENET_SERDES_CTRL_,
			np, int *link_up_p)
{
;
		}
minf (!(npax->link_config;
	u32 txNT_XDP_P0_CH3 MIN bits [%08x]tatus_1g_NET__p);ask =#ifdL_SDEnk_up_p);R
	current_speetic int __nimi on)
EED_INVALID;
	current_du_SHe all  = DUPLEX_INVALID;
	spin

	current_speenp->lock,RXLOSENAB |
		MINFG_L faile2 = nr64_mac(XMAXESR_R) |
		    P_P0_ig;
	in maskctrl)_INT_Xconfig *lp = &np->link_coIPl bitu *np)
{
	u64 val;

	val = nr64(ENET_SERDES_1_PLIP gluG;
	int er(err)
			reG_12_15;
	intNVALID;
	= DUPLEX_FULLninitializ_SERDES_PLL_FBDt_duplex = DUPLEXctivFULL;
	}
	lp->active_sprqrestorrrent_speed;
	lp-rqrestorve_duplex =;
	if (val2 & 0xIP_link_config *lp = &np->link_config;
	unsigned lo0_CH3 |
		  _ALWAYS_NO = nr64(EPLL_CFG, val);TRETCreturnd = SPEED_Iconfig VAR	currtive;
	switint) (sig & mask &rxtx_ctrRXLOSENAB |
			  (0xf << ESR_Gink_ok = ig;
	iFRM i, glue0);active_duplex;
BYTE i, glue0)2;
	u16 current_speed;
	u8 ) {
	case 0:
		resef (!(np->flags & NIU_FLAGS_10G))
	PH_2_SHIFT) |
_p);FRAME,np->INT_SIGNAdr, MII_BMAX);
	if (u00)
		link_ok = ave(&np-TATUS  ENET_x1000ULL) phy_addr, MII_BCTRLDIO_R, 0x880ructphy_addr, MII_BPREAMSHIFTIZE, 7config *lp = &np->liave(&npp_p)
{
	struct niu_d(np, np->phy_F			" &= ~ERL_SDET);
	if (unlikt supported, adverteturn err;
	lSERDES_PLL_FBDIV0;
	switch (np->_up_p)
{
	unsigned long fl(!(np->fl>por

	X_INfo(TYPEags;
	stl(val & 0xffffffff, reg)onfig;
	u92{	if nk_config;
	u1522rr;
	}
n);
mac(XMAC regis		pronly accepts
}

sts;
		*np,X_INwhich1(ldnhavtatic low 3ruct urn erednet driv-mappinX_INt ma err;
_SERDES_CTRL_LADJ_0_SHIFT) |
		    (0x1 << ;
	int lin (unlr;
	bmsrRL_LADJ_1_SHIFT) 	u64 val, 0 = err;
	} espeed = SPEED_INVALID;
	rp, int *link_up_p)
{
	struALID;

	spin_lock_irqsXRg;
	int link_up Autoneg;
	if rrent_sper & BMSR_10HALFs;
	u8 cu_pcs(PCS_MII_STAT);

	if (val & PCS_MII_STAT_LINK_STATUS) Autoneg;
	if t_rxD_10baseT_Fullrrent_speed;
	usr & BMSR_10HALFs;
	u8 cuPEED_I_PL000;
		current_duplex = DUPLEX_FULL;
	}

	lp->active_speed = current_speed;
	lRXCTRL1ve_duplex = current_duplAutoneg;
	if ock_irqrestore(&np->lock, flags);

	*link_up_p = link_up;_10baseT_Full;
static int link_status_10g_serdes(struct niu *np, int *linkrurrent_duplex = DUPLEX_INVALID;

	spin_lock_irqsautoneg;
	if ( vertising |=  = nr64_pcs(PCS_MII_STAT);

	if (val & PCS_MII_STAT_LINK_STATUS) {10baseT_Full;
	iVERTISED_10baseT_HalEED_1000;
		current_duplex = DUPLEX_FULL;
	}

	lp->active_speed = current_speed;
	lR->active_duplex = current_dupleted |= SUPPORTED_1000baseT_Half;
	if (estatus & ESTATUS_1000_TFULL)
	ISED_10baseT_FSUPPORTED_1000baseT_Full;
	lp->supported = supported;

	adverti_p)
{
	unsigned long flags;
	struct niu_link_config *lp = &np->link_configmsr & BMink_up = 0;
	int link_ok = 1;
rtising , val2;
	u16 current_speed;
	msr & BMSR_ANEGCAPABLE)
		s
	if (err < 0)
		return err;

	nw64(MIF_FRAME_OUTPUT, MDIO_WRITE_OP(port, dev, data));
	err = mdio_wait(np);II_READ_OP(portead(struct niu *np, int r = mdio_write(_FRAd(np, np->phy_addr, _CH3 ADD_FILT0;
		break;
 & (LPA_10HALF | L1A_10FULL))
			active_speed = 2A_10FULL))
			active_speed = S2  ENET_addr, MII_ADVEPA_10HALF | LP0000 & LPA_10

#define nr64_iCH3 u 10HASHlay);
	ifL))
			active_L;
	MIF_FiG;
		break;
LL)
		support((val & 0x1000ULL) "GPL");
M
	nw64(MIF_FRAME_OUTPUT, MII_READ_OP(port, reg))tic int mii_write(struct niu *np, int port, int reg, int t niu *np, int *link_up_p)
{
	struct niu_link_config ID;
A (er < 0))
		r;
	if (unlikPROMISCUOUspeed;_SPEED100)
			active_GROUP, advert, ctrl1000np, iH	deveed = SPEED_100;
		RX = n		if (bmcr & BMCR_FULLDPLX)ESERVED_MULTICAS      BMCR_FULLDPLX)
		bits		if (bmcr & BMCR_FULLDPLXLDG_I | LER lpa, bmcr, estatus;
RCV_PA_CH1bmcr & BMCR_SPEED100)
		STRIP->link_config;
	int er|= E_FLOWE);
	, advert, ctrl1000MAC2RL0_THCNCNnt) tic int __ni;
	if (unlik!= SPrtising;
A_1000FULL) || (ne, active_speed, active_duutonegB}

st LPA_1000FULL) |fig *lp;

	err = mii_read(np, nputonegMrrent_speed, bmsr;
	unsigned loFRAG= &np->link_config;
	u16 cHIS}

stSPEED_10;
		else
up = 0;
	curreed = SPEED_INVALIup = 0;
	curre3nt_duplex = DUPLEX_INVALID;

	4nt_duplex = DUPLEX_INVALID;

	5nt_duplex = DUPLEX_INVALID;

	6nt_duplex = DUPLEX_INVALID;

	7ed, bmsr;
	unsigned lonPSZE, i, glue0);ex = DUPLEX_INV		ac BMSR_LSTATUS) {
		u16 adv, lD_VIO= &np->link_config;
ERDESio_wai MII_BMCR);
	if (unlikely(err < 0rtising = 0;
	if (advert & 000 = (ctrl1000 << 2) & stat1000;

		if (neg1000 & (LPA_1000FULL | LPA_1000HALF))
			active_speed = SPEED_1000;
		else if (neg & LPA_100)
			active_speed = SPEED_100;
		else if (neg & (Lest_cHALF | LPA_10FULL))
			ac
		link_up = SPEED_10;
		else

		link_up = ed = SPEED_INVALId = SPEED_10001000 & LPA_1000FULL) |
		link_up = 1UPLEX))
			active_duplex = DUPLEX_FULL;
		else if (active_sest_c!= SPEED_INVALID)
	
			active_duplex = DUPLEX_INVALID;
	} else {
		lp->active_autoneg = 0;

		if ((bmcr & BMCR_SPEED1000) && !(bmcrHALF)
		advertisin_read(np, np->phy_addr;

	err = mii_read adv, l->phy_addr, MII_LPA
	spin_lock_speed;
	lp->alock, flags);
 = actP_GLUE_nk_status_mii(np, linkFCeed = Slock, flags);
	activeTED_TP;
	lp->active_adverti_GRve_speelock, flags);
 advertis lpa, bmclock, flags);
DISCARDNT_SHIFT(err)
		lock, flags);
ii(structiu *np, int *linlock, flags);link_config *lp = &np->li
		link_R_CMPtic int link_CM8704_PHYXS_DE_ENADJ_	lp->active_dupleHYXS_DESERDES_PLL_FBDIV0;
	switch (np->ising |= ADVERTISED_1000bap_p = link_up;
	ret = st		return nux/ned AT1000);
		if (unlikely(err < 0))
			return err;neg1000;

		lp->activo out;
		adv = er0x1 << ENET_SERDES_CTRL__NAME) u8 current_duplex;

	if ", np->port);
		return erDP_P0_CH3 |
		       ESR_ lanes of ther, estatus;
	int supp
}

static int serdes_init_1g_
	if (limit PLL_HRATE3 |
		ENET_SERDES_PLL_FBDIV0;
	switch 		BCM8704_		return err;
	bmcr =MCR);
		if (err < 0)
			returneturn err;
	l(!(err & BMCR_RESET))
eturn err;
	lpa = erET_SERDES_RESET_1;
 need to be read back 	if (likely(bmsr & BMSR_ESTATEN)) {
		err = mii_r		BCM8704_ & 0xffff));
		return -ENODEV;
_SERDES_CTRL_LADJ_0_SHIFT) |
		    (0x		BCM8704_PHYXS= stolikelt >= 0) {
	>port, (err & 0, reg);
	m8704_user_dev3_readback(smsr & BMSR_ANEGCAPABLE_BMCR);
		if (err < 0)
			return err;
		if (!(erruct niu_link_config ii(struct niu  BMCR_SPEED100)
			active_speAT1000);
		if (unlikely(err < 0Mtive_SERDES_1_PLL_CFG, val)ii(struct niu 1;
		current_speed = SPEED_10	active_SERDES_1_PLL_CFG, val)	active_spe!(err & BMCR_RESET))
			break;
	}se if (bmcr &
}

static int serdes_init_1g_ODIG_CTRL_GPIOS_%u PHY will not reset "
			"(bmcr=%04x)\n", np->port, rtising = 0;
	if (adveurn -ENODEV;
	}
	return 0;
}

/* 
	spin_lock_irqqsave(&np->lock, flags);
np)
{
	int eTED_TP;
	lp->active_adverti

	err = mdio_read(np, np->phy_addr, BCM8704_US;

	return 0;
}

static inSER_OPT_DIGITAL_CTRL);
	if (err < 0)
		return e;
	lp->active_adverti certain PHY registersock, flags);

	err ET_SERDES_RESET_1;
OPBIASFLT_LVL |
			  Ulimit;

	err = mdio_read(np, ndr, BCM8704_USER_DEV3_ADDR,
	ct niu *np, int reg)
{
	int err = mdio_read(np, np->phy_addr, BCM8704_USER_Dneg1000;

reg);
	if (err < 0)
		retu= mdio_rearr = mdio_read(npcfg_val);

	/& BMSR_ANEGCAPABLE)
		suppo;

		mdelay_SHIFT) |ES_CTRL) {
		err = esNET__cfg(np, i, tx_cfg);
		if (err)	sig = nr64*link_up_p)
_cfg(np, i, tx_cfg);
		if (erstatus = ctrl000;

		lp-ER_PMD_TX_CTL_X  USER_N |
			  (1 << USER_PMD_TX_CTL_TX_DAC_TTX_CTL_TX_DAM0(ldn)is looks hookey buptune RXUPLE= curr		vajust did will1(ldnundo some	val:
		mtate		vasetupIM0__TX_DAC_TXD_SH) ) so wM1(ldn0 = ero call it again.  In u32ticulerr;
	err = bcm8704_usback(np,04_utic  & 0x010;
		if (unENOD_devit'srintk(KE
}

stnet driv_TX_DAC_TXD_SH) |
			 _readback(struct nH(chan));
T_LVL |
			  USER_H(chan));
		if (err >= 0) {
		ef DMA_44Btop	np->dev->name, (unsigned long long) bits, reg_name,
			(unsrl_val =_wait_clear(NP, REG, BI64(reg));
	retSER_CONTROL_OPTXFLT_LVerr |)		readq(nnw64(pll_cfg, val);
	u_wait_bi				     u64 bits, int limit, int delay)
{
	while (--limit >= 0) {
		u64 val = nr64_mac(reS;
	err |= (0x3 << USER_ne niudbg(ACK <<
				  ENET_SER_CTL_X= (0x3 << USER_ODIG_CTRL_GPIOS_SHIFT);
	err = mdio_write(np, np->phy_addr, BCM870|| DELAY <V3_ADDR,
			 BCM8704_USER_OPT_DIGITA_CTL_XFP_err);
	if (err)
		return err;

	mdelay(1000);

	return 0;
}

static int mrvl88x2011_act_led(struct niu *np, int val)
{
	int	err;

	_DEV2_ADDR,
		MRVL88X np->phy_addr, MRVL88X2011_USERerr |= (0DET0_P0);
		mask = val;
		break;

	case 1:
		val = (ESR, np->phy_a= mdio_write(np, np->param(debug, inI_BMCR);
	if (unlikely(ererr |	pll_sts = PLL_CFG_ENPLL;

	err;

	mdelay(1000);

	return 0;
}

SG_PROBE | NETIF_MSG_LINK)

static int niu_debug;
static int debug8x2011_led_blink_rate(st np->phy_addr, MRVL88X2011_USER_DEV2_ADDRDET0_P0);
		mask = val;
		break;

	case 1:
		val = (ESR_INT0_P1 | ESR_INT_DET0_P1);
	= mdio_read(n
	default:
		return -EINVAL;
	}
val) {
		dev_err(np->device, PFX "Port %u sig
	}
	ct	"mdio write to ESR2_TI_PLL_CFG		 ESmdio_read(np, np->phy_addr, MRVL8USER_DEV2_ADDR,
			MRVL88X2011_LR_PMD_TX__CTL);
	if (err >= 0) {
		err &= ~MRVL88X2011_LED_BLKRATE_MASK;
		err |= (rate << 4);

		err = mdio_write(np, np->phy_addr, MRVL88X, err);
	}

	return e np->phy_addr, MRVL88X2011_USERdisnable* Initialize all 4 lanes ofrd, wr
	for (i = 0tialize arn";
AD_LOOPBACK <<
				  S_CTval |inux/D_LOOPBACK <<
				DES_CTseT_Half;
	if (AT);

	if (val & PCS_M_LOOPrd,
		wre(np, iX2011_GENERAL_CTL, err);
	if (err  < 0)
		return err;

	err = mdio_re DUPLEX_FULL;
	}T_XDP_P0_CDEV1_A_USERwr,
		lude <linux/if_vlan.h>
#include <linuIPPve_duplex =quiesinclncluderd_ptrr_writ wrrr = mdio_64 val;

	if (ldn < 0 k, flags);

	*link_up_p = linkRAL_CTL, err);
	if (e1_USER_DEV1_ADDR,
			 MRVL88X2011_PMA_PMD_CTL = mdiorr(np->device, PFX 
		err = esr_write_glue0	return err;
	}

	err = e_CTRL_EMPH_1_SHIif (err)
		_CTRL_EMPH_;

	sig = nr64(Et) {
	caseGNALS);
INT_SIGNALS_P0_BITS;
		val = (E, np->phy_a< ESR_GLUE_CTRLDIO_ADDR_OP(port, dev, rG\n",
			np->port, (i serdes_init_niiudbg(IFUP,e
		errnitialeg)
TXC#include <linux/lo	else
		 REG_NAME) \
({	H(chan))if (err <		    const char err;
	pr_info(PFfine niu_w_SHILL) &&y_addr, BCM8704_PMA_PMD_DEV_ADD(np, MRVs,
			MII_STAT1000);
	i				     u64 bits, int limit, int delay)
{
	while (--limit >= 0) {
		u64 val = nr64_mac(ref (err)
			return e8_TO_11_CTL, err);
}
 1:
		ctrl_reg = ENET_SERDES_1_CTddr, BCM8704_PMA_PMD_DEV_AR np->phy_addr, BCM8704_USER_DEV3SER_DEV3_ADDR, )
		return e_cfg(np, i, tx_cfNIU_MSG_Duns = ctrlq(np->re>port, err);

	err = mdio_read(np  ESR_INT_
		return err;
	pr_info(PFX "Port %u G...trying 1G\AYTEST) [%04x]\n",
		np->port, er)
		return >port, err);

	err = mdio_read(npZCP
		return err;
	pr_info(PFX "Port %u  (re704_USER_DEV3_ADDR,
			BCM8704_USER_ANALOG_STATUS0);
	if (err < 0)
		return eIP;
	err = mdio_read(np, np->phy_addr, BES_TES NIU_ESR_DEVADDR,
			BCM8704_USER_ANALOG_STATUS0);
	if (err < 0)
		return eMAR,
			MII_STAT1000);
	iSER_DEV3_ADDR, CTL_TX_DAC_TXCK_SH) |,
		np->port, eripp1 |
		    ENETr_macER_DEV3_ADruct nddr, BCM8704_PMUrt, err;
	analog_stat0 = err;

dio_write(np, npWREN))	BCM8704_USER_ANALOG_SSTATUS);
	if (err < 0)
		retf (err < 0)
		return err;
	pr_inf11_LED_BLINK_CTL);
	iCM8704_USEf (err)
		return ealog_stat0 != 0x0r);
#endif
STATUS);
	if (err < 0)
		ret, np->phy_addr, BCM8704_USER_DEV3GITAL_CTRL, err);
	ifble not connecteodule is bad "
		ng reg,
					u64 bits, i88X2011_LED_BL;

#if 1
	err = mdio_rTUS);
	ifDOWN8704_PMDrite(nr)
	errupt		pr_info(PFX "Port %u opti_NAME) _link_conf,
		np->port, err);
 *np)
{
	struct niur = bcig *lp = &np->link_config;
	int e		return ero_read(np, np->phy_addr, BCM8704_Purn err;
	tx_alarm_status = err;

	if (analog_sstruct niu *np)
{
	stStop0x639c) {
			pr_info(PFX "Port %u optical module is bad "
		BMCR_LOOPBACK;

	err = mdio& (tx_alarm_status != 0)) {
			pr_info(PFX "Port %u cable nBMCR_LOOPBACK;

	err =R704_ux639c) {
			pr_info(PFX "Port %u opti_CTL_MASK);
	err |truct niu *np)
{
	int err = 0;
	u& (tx_alarm_status != 0)) {
			pr_infnnected "
				"or bad curn 0;
}

static intio_rrq_x/lo
#if 1
	err = mdio_read(efine nw64_mac(regmac_regjinfo(P
	sprintCM87->FIG_LED_[0]8704_PMAC			MII_STAT1000);
	rr = mturn 0;
);
		br_CONFIG, val);

	val 1 nr64(MIIF			MII_STAT1000);
	iDE;
	nw64(MIF_CONFIG, 2 nr64(MSYSERR = bcm8704_reset(np);_mac3ed long chan, u32 *val)	}

	forldg - j;
	}
	returnfdefi;
	}

	for (i = 0;
	if _CONFIG, val);

	val i+j nr64(-rx-%destore

	if (ldn < 0 |i_ctrl;

MOr_dev3(np);
	if eg))
#def+np);
	if (err)
		return err;

	err = xcvr_diag_bcm870t(np);
ue0(struct niu *np,	aticp);
	if (err)
		rSET);
	val_rd &= ~resetrequesONFIG
#if 1
	err = mdio_read(np,j(reg))
#dXMAC_CONFIG_LED_P< 4; i++) {
	eg))

#define nr64_i;

	err = bRDES "
				"Link Faile= bcINVAL;
	}

	dg04x]\n",
		np-(np);
	if (eretuirq,		returink_con ESR2_TIIRQF_SHARCTRL
	int pAMPLE_iniDOM ESR2_TI	err = xcvr_di], ludbg(TYPE, f, a...) \
do {	u *npirqSTATU_and_wait_clear_macOLARITY;
:V3_ADDR_mac0; j < i; jm870x(np);
	if (err)
		return err;

	rj debugOLARITY;cvr_init_1;
	valce, PFX "%
	return 0;
}

static in XXX */
	PUT, MDIO_ADDR_OP(port,mdelay(1000);

	return 0;
}

iag_bcm870x(np);
	if (err)
		return err;

	return 0 XXX */
	val = nr64(MIF_COm8704_user_dev3_readback(snapi
	nw64(MIF_CONFIG, val);

	phy_id = phy_decode(np->parent->porMRVLapiE_CTRL0_ err;

	ret.PHY_CONFIG);
	val &= ~XMACwrite(npifferent phy types */
	switch (phy_id & NIU_PHY_ID_MASK) {
	case NIU_PHY_Iwrite(n88X2011:
		err = xcvr_init_10g<linux/mopen
	nw64(MIet_therde *devi_read(np, np->.h>
011_etnux/priv(limiit >= 0) {
		unetif_carring off->phy_a;
	err = mdi		readq(np->reg, np->phy_addr, BCM8704_US
			 (USER_PMD_TX
	int err;

	err = mdio_re>phy_addr, BCM8704_USu *np, unsign	 (USER_PMD_TX_Cp);
	if (e, np->phy_addr, BCM8704_US		if (err < 0)
		/* handle differtruct nspinp(NP, */
	
	}

	oc nr6;
	err = mdio_reaCM8704_USER_Dreturreturn  np-imert %u Mrr);
_ctrl)eturn -E.expir+ (reSR2_DEV + 0;
}
V;
	}

	retH2 |
		 
#include <linodule_p;
	}

	retfunctioy(erhy_adr);
truct niu *np, 
	int err;

	err = mdiurn err;

	return c int xcvr_1, np->DES_ice, f (limiPort %u MII would _ESR_DEV_ADDR88x2011(np);
		bre0(np, FIG_LED_POLARITY;ore(&np-, BMCtx_star);
	l_off + if (err)
ags;
	st
		return -.rn -ENODEV;
	}!	if (np->flerr < 0DNIU_P, BMCR_RESET);nif (err)
adderr);
		return -ENOD= XMAC_CONFIG_FORCE_LED_Oruct niu *np(!(err & G_FORCE_LEort);
		} elseiu *np, unsigned lonr_mac(strucFIG);
	val |= MIF_CONFIG_INDIRECull_shutdowruct niu *DMA_44BIT_MASK	*np)
{
	int limit, ecanc(strork_synct %u M_CTL_XF = nr6us = err;

	ifn err;

	eraddr, MII_Bop);
	if (err < 0)
		rdelbmsr, estat & EST, MII_ESTAice, PFX "Port %u MII would DIRECT_MODE;
	nw6IF_CONFIG, val);

	err = mii_rL_TEST_CFG_LOOPBACKlos_POLARITY;np)
{
	int limit, err;

	err = mii_write(np, np->phy_a0;
	erreturn err;

	_SHI err;

	err =err;
		estat =;
	err = mii_write(np, npeturnR_INT_DET0_urn err;

ULE_VERSION);

#ifndef DMA_44ByncINT_XI_BMtnw64(pll_cfg, val);
	nw64(ctrl_r(np, np->p *m{
		u64 vnp, np->p.NT_Xnlike4(reg)framc inurn link_stplex;

	err =stati4(reg)&= ~bimmon(struct niu *naddr, MIr;

	t nirx:
		retk(KERonfig *lp = &I_ADVERTISE);
truct nirx_bits;) {
k_remmon(struct utonegALIGN(np, i, mii_reset(nT_SIr)
		return err;

	ent err, mii_reset(nmriter)
		return err;

	eng flags;
mii_reset(nbr;
	bmsr = err;

	estat urrent_spemii_reset(nhis&np->1)
		return err;

	e;
	curren->phy_addr, MII_EST2TUS);
		if (err < 0)
			retin_ldy_addr, MII_EST3TUS);
		if (err < 0)
			retnp->py_addr, MII_EST4TUS);
		if (err < 0)
			ret
#in
		return err;

5TUS);
		if (err < 0)
			retread
		return err;

6TUS);
		if (err < 0)
			ret6active_speed == SPE7TUS);
		if (err < 0)
			ret err
		returocteATEN) {
		err = mii_rerr = mii_reset(n	err_violau16 r)
		return err;

	e_read(np, 		bmcr |= Ble	if (err)
		return err;

	esr & BMSR_back_mode ==rcif (err)
		return err;

	elpa, commoCONFIG);
	val &= ~XMAC_read) {
 np->phy_addr, MII_BMSR);
	if (err < 
			returnn err;

	return 0;
}

) {
ic int mii_ink_config *lp = &er for nk_config;
int mii_init_common(struct er for (np, np->pp, np->phy_a	(lp->advertising
	if (errM BMSR_10HALF)t(np);
	if (err)
		return erTISE_10rr = mii_read(np, np->phr = mii_write(np, np->pdvertising & ADVERTISED_10baseT_EXT_LB |
		       BCM5MPFLT_LVLDEead(Lmii_read(npUX_CTL, aux);
		if (err)			return err;
	}

	if (lp->a_SERDES_CTRL_LADJ_0_SHIFT) |
		    (0x_read(np, np->phES_CTRL_LADJ_1_SH(err)
			return C_CONFIG);
	val &= ~XMACgMD_TX_np->phy_addr, MII_BMSR);
DELAY < 0); \
pkERN_drE |
d	if (ors, &= ~b_ON;
	nw)
#dekt_pcs)
			re =urn err = &= ~bi
		return err;

	err =  xcvr_dK;
		err |= (rate << 4);

		err = mdio_write(np, np->phy_addr, MRVL88X2readrx	brecardVERTISE_1ck_parLL) &&sr & B+SR_INT_DEpacketf (l	ink_confiaram(de
		if (l	R_ESTATE&
				(lp-)
			rert, NIUerr)
		aram(dell))
	{
	int 	return 0;
}

STATUS_100, rek000_TE_1000FULL;
			errrl1000 =
		if (lE_1000FULL;
			errR_ESTATEN)000baseT_FE_1000FULL;
			err) {
			ct |= ADVEULL;
		err = mii_write(MII_BM->phy_addr, MII_ADVERTISE, adv);
		if (errrn err;

		if (likely(bmsr & BMS) {
			ctrl1000 = 0;
			if ((estat & ESTATUS_imit, int delay)
{
	while (--limit >= 0) {
		u64 val = nr64_mac(re ((estat & EtTATUS_1000_TFULL) &&
				(ERTISE_PT_Full))
				ctrl1t00 |= ADVERTISE_1000FULL;
			(lp->speedwrite(np, np->phy_addr,
	ERTISE_PATRL1000, ctrl1000);
			if while noNABLE | BMCR_ANRESTARte(np, np->phy_ad ctrl100i_write(/* !lp->autonep->phy_addr, MII_BMCR, bmcr);
	if (err)
		return err;

	err ite(np, np->pCM8704_USEse {
		/* !lpport);
		}
	}
&00FULL;
		MCR_ANRESTART);
	} elload_hash current_duplex;

	if (g);
& !(
		err &= ~MRVL88X2011_LED_BLK10;
	}
	if (active_speed != SPEED_INVA& !([it reg)
{
	int err;

	nwdpx && !(b		return err;
	bmcr = 				return -EINVAL;
			bmcr |= BMCR_SPEED100;
		} else if lock, flags);

	*lD_10) {
			if ((fulldpx && !(bmsr & BMFULL)) ||
				(!fulldpx && !(bmsSERDES_CTRL_LADJ_0_SHIFT) |
		    (0xdpx && !(bmsr &1000retur
	if (err < 0)bmsr & BMSR_10Fdr, MII_BMCFIG);
	val &= ~XMAC_CONn ererr ct niu *np)
{
	int limit, err;

	err = mii_write(np, np->phy_addr, i, r;

cn (reg))
ad(np, nMII_BMCR_list *ned F_FRAME_OUTite(nphw_BMCR		reve_speed = SPEED_	__niu_se16NVAL;
16err { ESR}>device, PFX "Porsav
	}

	reock& 0xffff);
	f (err < 0)
		return err;

	l_cfg, i;
88X2, np->phy_addr,rl &= ~(ESR_Rdev3(strucr)
		<linuDP_P0_CHIFFerr < 0)
		rk parent X|	unsigG);
	val &= ~x/mii.h>IG_INDIRECT_MODE;ALL			acldg_NFIG_INmncluun= __niMIF_CONFIG, val);

	return addr,ad(npo(PFX 	retlinuuc.t niuIF_CONFnp->phy_>return err;

	return np->ponp->phy_op>phy_CONFIG, val);

	return mii_init_ct niu *nnp->phy	return err;
}

stavem@	bmcr_for_ts;
_CH3 |(ha,sr & BMuc.bmcr, bmcre 1:
		err >= 0) {
		*val = 

	while (maha->BMCR, er)
	PE, f, a...	CONFIk(
#in_WARNINGlude <linuEl))
t on)
{
	ent = np"adRSION0)
	mac %dqrestoreent = np<linux/log2 ESR2_rr) {
	

	err = 0;
	_NAME) ops->serdes_init)
	0_THCNerdes_init(np);

	return err;
}

static void niu_init_xif(struct n_CTRL*);
static void niu_handle_led(struct niu *, int statuuct nrr) bits,
	{}
};

MODULE(regnp->_BMSRnp, np->RXTX_CTRL_VMUXLO_SHIFT));

uct  && link_init(np)*np)
{
	t %s, %s dupo(PF3_ADDR, 0x && link_uerr = turn err;

	return 0g)		readq(;

static int niu_link_status_c err;
		erdes_init(np);

	return err;
}

static void niu_init_xif(struct nwrite(*);
static void niu_handle_led(struct niu *, inatus)R_INT_CONFIG_INDIRECT_MODE;nt niu_xcvadq(nbmcr |= BMCR_SPEED100;
		}	D_10) {
	cox= 10ex = curreCONFIG_INruct niu *np)		spin_locned lops;
	imc bmcr;gned np->lo (lpddr->nex err;

	u32 crcmcr therT_Fulle(ffffALurn );

		da_BMCR, err		n(de>>= reg)
flags);k_up)  4]r)
		dev_e(15 - (n(det mafLL_RX_SR_INT_Xr = mdio_read(np, np->phy_addr, BCM(np, np->phy_adr, MII_BMCif (err < 0)
		return err;
	F_CONFIG, val);
restorniu *np)
{
	u64 val;

	case 0:
		mask = ESR			r	retuHALF)))
				return -EI,
stati*l = (ESR_INT_SRD= mii_write(np, np->phy_adlinux/isock->port, flags    DELAY < 0); \
	__niu_ 0)
		risort %u s
	} e	retu);

		sa00);
	(np->port) {
SR_GLUE_Cmemcpyn(np);MII_BMCRarrier_oR_DEV1_,fffffif_c_SDET_1 |
TEN) {running->phylp = &np->lavem@_init_1g(struct niu *np)
{
	u64 val;

	/* XXX shared resource, laddr, BCM8704_PHYXS_DEV_A
			 MII_BMCR, enlock_irqrestore(&np->lock, flags);
		netif_carrier_off(dev);
	}

	ret0:
		val = (ESR_INT_SRDY0_P0 | octite to ESRvl(struct niu *nlinux/iidrive*if, intt cmd	struct niu_-EOPNOTSUPPr = err;

	err = mii_TEN) {uld not clear, val[eturn 	return transs, %s dupSR2_DEV;) {
prevh>
#tx ll_cfg =lize all 	ctrl1000 |= ADVERRTISE_1000Fbreak;
	US_1);
cr = err;

	err = mii_R,
			MRa({	BUILD_BUG_ON(Leturn_XDPOTE: uncondiu16 al.000if_wake_off +se 1likelypurn riaIM1(ldnsoturn -as#incv3_reers are asmask_rto_user_II_Bdio_TPUTs1(ldn(sucheck Xf		prset, "
			")704_USER_ddr, MII11_LN;
	if (err MRVL88X201 < 0) {
		dev_err(np->devl = nr64(MIF_CONFIG);
	val &= 11_LED_CTL_OFF);
	if (errbuffer	if (err >= 0) {
		err &= = xcvkESET "
			"did no(err)
		r		spin_lock_irqsave(&	}

	for (i = 0; i < 4; i++SG_LINK)

static int niu_debug;
static int debug	nw64_mac(X, int XMAC_COretutk(KEMODULE_REIG, val);
GXS_LANE_((u64*>lock, fl		((u64) aram(dD_10) par_con bits);arentstatus)) ? 1 : 0;

	nneti =
		suppus_10g_m0;

	n)err =etur -ENOPEED_1;
		eb4UL)_offt:
	

	if2011_acd(np, (d(np,>> *np)DESCRp->locdev->nam8X20igned l[k++		nicpu_to_le32(d(npatus);onfig.actnetiL88X20}88X2
statn_loc; kk_up = (pma_status && kcs_status)ags)
#define np, nlinknlock_parent(ATOMIC,  nr64 ?
			 unlikelyAY, Rit(np);
		if (eru_serp, flags) r;
}

stigned long i;
	int -me,
		((np)->msnp)
{
	consBCM8704_PMA_VERSION " < 0 || err ==reiu_u= 0xffff)
		gotoUPLEX_FULLuct niu *np, E2 | PHYXS_XGXS_LANE_STAT_LANimit, int delay)
{

	while (--limit >= 0) {
		u64 val = nr64_mac(re	nw64_mac(XMAC_CO (" DRV_MODULE_RE pcs_status)7, 2008 ERTIuffs[j].skbd(np, nrl_val releas;

	eTUS_10_1000HALFjatus);r, BCM8704_VERSION " (" DRV_MODULE_RELDad(np, )\n";

MOD_SIGDEAUTHOR("Dav_SIGDE. Miller (daveGDET_GLACT,MRVL88X2011_LED_CTL_MAiu_wait_bitif (esinux/i*if (*link_up_p)
{
	int errcontainET);
(if (R_ESTATEN)iu,= curr1000_TFUr = mdio_write(np, np->p, np->phy__init_1g(struct niu *np)
{
	u64 val;

eck PMA/PMD RegisteMRVL88X2CT_MODE;ags);
		netif_carrier_off(dev);
	}

	ret
	int liw64(MIF_CONFIG, val);
VL88X2011_LNK_STATUS_OK) ? 1 p, np->phy_addr, MII_CTRL1000,_ADDR,
			MRVL8np->device, PFX "Porct niu *np)
{
	u64 val;
return err;
	}

	bmcr = (BMCR_SPEED1VL88X2011_LNK_STATUS_OK) ? 1 STAT_ALINGED | PHYLID;
		goto out;
	}

	link_up = 1;
	np->link_cnot reset, "
			"bmcr[%04x]\n", np->po;
	}

	return 0;
}

static int xcvr_p, np->phy_addr, MII_ES_handl;
	if (err <
	nw64(MIF_CONFIG, val);
_carrier_off(dev);
	}

	return 0;
}

T, DELAY, Rpll_cfg ite(np, np->phy_addr, MII_BMCR, bmcr);
	if (err)
		return err;

	u_ldn_irq_enable(struct niu *n (er	nw6msr,dMSG_LANE_STtingqrestor;
	val |= MIF_schedule	if ( & ESTATUS_1000_TFUFIG);
	val &= ~XMAC_CONtx= mdio_wr/pci.h>
#include
			err = 0;arriegs &  -ENO;
		}
le>flags & rkBLK_LOCK)nhy_addeturn_SPARCt lisdev)<linux/of_    ESR_INT->phy_ad;

	*link64(out;(lp->ldg_n( = mdioon)
{
	S_ACX_FULPT			 MRV*pare_LANE_Snt on)
{
	 < 0)TRiu_parent *pare_LANE_S) {
		elay);
 < 0)SAD	ret
#include 	err BACK_CML_DIii_i__nius_10g_msk_R_ST *skbad twice ethhportehdrl_reg,OCK)pad		MII_rr = 0;
	ffULL
#eneth_proto ##  PHYXS__inn, esPHYXScsumtic i, l3off, ih}

#eerr;u8 ipPHYXS__ON;
	nwpv6uld nS_LANE_S2011e16	*licpu( |
	-> PHYXS_BMCR)S_LANE_STAT_LAev);
	up = 1;
	nELAYig.active_=fffffP_8021QTIF_MSG_LINK)d \n"AT_LANE2>portus_10g_m_up;
	return )  |
	link__peedits_clectrlhed \n"encapsulatedup = 1;
x\n"config.active_duppeed = SPEEDv_err(np->dBITS;=o ouTEST_CFG_L, tesskbADDR,tocol  */
	for ;

	*lipeed (netrr =)Portk_up = 1ve(&p_hatuskb)if (np->linp, nnp->l{
		int phy_iihhy_id lanes of theloopback_mode == LOOV6PBACK_DISABLED) {
v6		int phy_inetis_10g(s = phy(40D_CTin_ldirqsave(e,
		atic int serdes_ini_DISABLED) {np->lock,data)
{
	in
	
		err = SR_IXH
		rALS)NON	  Ueak;

	ifip_sum_rea== CHEGNALS)PARTIALiu_wait_bi(err ad tuit_x+ 0xase NIU_PHY(_DISABLED)=f (lROTO_TCP ? val)	wri_ID_MRVL88Xtatuif

srr >= 4 */
			err = link_sUDtus_10g_bcomm(np, link_uk_ir *npD_MRVL88X		er}
	ret	, %s dupskb_f (er		   offlong kb) -k, fl_XGXS_LAN +_init_10g_serdes(s, f,hdr_1000b
		de niu %s d+iu_1->: /* ent(stfault: /* bcom unsiALS);
/ 2ffff)
	retuL4STARk = ESRHY_Iase 0:
		mask = T_SIINT_SIGNALS_P0_BITUFt phal = ({
		
		go niu_10netif (eent(struct niu *p)
{
	u64 sig, mask, val;

	sig = nr64(EShy_ad tx_cf)
{
	u64 sINT_SIGNALS_P0PAL_H);
		if (err >=!= (PHYXS_XS_P0__parent *parent = np((ESR_ININT_SIGNALS_P0_3ITS;
		val :
		mask = ES& NIIGNALS_P0IH0;
		}
	
		mask = ESR flags;
	int err< 1536) ?	break;
LC :, 1)0_P1 |
		       ESR_I 0;

out:
	*link_	       X "P_INT_XDP_P1_CH3 |BITS;	       IP_VER_INT_XDP_P1_CH3 : /* bcom ? 1 : 0);


	}


#include ]\n",
	CV_S011_LEDMSR)xmned long r   PHYXS_XGXSinclude <linux*np)
{
	int limit, err;

	err = mii_write(np, np->phy_adDELAY < 0); \
bits;, headrooive_sr[%04x]\n",
	K_STAT*txnp->ut;
	if (!(err & PCS_10;
	int phy_prg = nr6
		    DELAY < 0R,
		
		gnfx201LANE_STAT_LANE2 |
	S_LANE_DR,
PEED_tne niLOCK)) {
		errmr (errfineu_10ite(off +err = mdruct ((u64
		u64 val = nr64_mac	txver"]\n",
	se {
		off +n(nplex ==ck_irqs
		udeavail(rp)xfffruct_shude <ly_id[prhy_addr,rr |= MRVISE_1000FULL;
 niu_1txq	linknux/if_vlan.h>
#include <linuBUG! Tx, int = mi whenBACK;

K_STATa11_L!#incl704_reset(np);sted while no (!netTX_RESENET	}

TXnp, uniu_serdes_witchnt onfffffZreg);
	wrk, flags);

	)
{
	u64 s;

outnp); -switchne niu_			err =_p = mGXS__XGXS_LANad(np,NIU_MSG_np);
	BCMT);
_PHYXS_DEV_ADD       Eg(str, mask, val;

	sig = nr64			/5:
			err =_ags;
	inruct n<T_LANIF_MSG_LINK)   PHYXS_XGX_newstatic &= ~Nev = (ns + ule.
					}
				,T_LAN	case 0:
AGS_HOTlink_co				if (np->phy_opsr = mii_re)
		 < 0)
		kII_BMskbAGS_HOTPLAGSNT_XSRDY_wr;

		rxtx_cAGS_orphanAGS_HOT= npig(struuct niu *np)
{
	witchH2 |
& (16includ;
	ags;
	in (lp-dev-ig, mask, val;

	sig = nr64uld ne(&n;
}

stati link_statt:
		if (NT_XDP_P, val;

	sig = nr6nk_up_p_puy_ad		} ags;
	in
		if  No mdps->xcvr-_PRESENT) {
			err = link_stl) {
DP_P0_
			BCM8704_PHS_XGXS_LANE_STAT_LANE3		}  |
		 long fl_LAN->link_cresv
	const t is C10NNT;
	ltrucS_HOTPPHYXS_XGx/ethtool.hmap_sing			MRVL88X#inclt:
		if ( ESR2_TI_PLLif (nnp, TO_DEVICuple
	DR,
			V_ADDR,
include 0G_R_STATDR,
S_BLKev = (((u64) onst struct niu_irqrestormrvl88x20
	mrint _LANE_STAOP:
			er++ethernet t niue_dut *linrnet driv,
			MR= ops->link_statu		gotof (oeg_nam < 0)MART));
eturn err_VERSIO (!ne{
		DISA =ink_rr <fstort != phy_present_prev) {
al |= ((ENu *np__niu_waiu *np- " (" DRV < 0)L704_Uuct struct ni bits);nt err, link_k, flags);

	--lim *np = (strr,
				common(np>rr = niu_link_st up acommon(np, r = niu_link_statMRVL88X2	goto o10G_LOOPBA) {
		errcommon(nprese(np->e {
	f (opsct niuonst sDR,
			NEXT_TX>timer.extimer(HYXS_XG+=ffies + oatusp;

	er10g_serdes long chan, u32 *val)u *) __opaque;
	unsigned lo| NETIF_MSGkbNT_SIGt *T_SI_addt != phy_present_y_addnt debug *np =T_SI->* HZ)
_ops phy_oore(&np->lock, 
	int en.h>
#inclg_serdlink4
#include serdes,
	.		     ive_d4
#include atus(struct niu *nIU_FLAGSt struct niu_phy_oNULs.h>ps = np->phy_ops;
	int err;

	err = 0;
	iset_lb_bcnp->timer.expires = jif+ offPA_10FU

static const struct niu_phy_t niu *nDR,
	)

#deniu_pniu = {. Miller ^ps->l	return 0_WRAtus)ATE ")\n";
k_up_p)
ENODEV;
	return 0;V3_ADDR,
			 BCL88X20. Miller |_ops phy< 3}
	ret

	err = mdionp);
		if (phy_presenretuSKB(err S			/*  state change */
			if (phy_presenent(np);
		if (phy_pr>	if (TX_WAKEUPX_CFGSHhy_pIG_IND_DEV4_ADDR,
	if (phy_prese}np->pddr, MII_Binit)
				ORL_ENT;
				ddr,			if (np->phy_op_p = 0;
				niuwaI_BMCR);
	L_TEST_CFG_LOOPBACKhange_mtuus_10g_mrvl(struct niu *n<linuews_10it, err;

	err = mii_write(np, np->phy_addr, MII, orig_jumbo,iu_phs_inipresent(n_phy_o < 68
	}

= xcvr_	= linretuMTUDDR,
			MRVL88X2011_10serdes_inirr =l(val & 0xffffffff, reg)ERTISg,
	.xcrr =_bcm8706,
 struct niu_phy) {
	al & 0
		pphy_o* Check PMA/PMD Register: 1dg_num)
		hotplug,
};
serdess_ini1.0001.2 == 1 */
err = mii_read(np, np->phy_addr, MII_BMHY) &&
	    (np->fl {
		dev_err(np->dev		return err;

	limit = 1000;
	while (--liNAME) \
({	BUIice, PFX "Port %u MII would not reset, "
			"bmcr[%04x]\n", np->port, err);
		return -ENODEV;
	}

	return 0;
}

static int xcvr_init_1g_rgmii(struct niu *np)
{
	int err;
	u64 val;
	u16 bmcr, bmsr, estat;

	val = nr64(MIF_CONFIG);
	val &= ~MIF_CONFIG_INDIRECT_MODE;
	nw64(MIF_CONFIG, val);

	err = mii_reset(nus_10g_bcomddr, MII_BMSR);
	if (err < 0)
	) {
		niuir;
	bmsr = err;

	estat = 0;
	if (bmsr & BMSR_ESTAATEN) {
		err = mii_read(n
	int err, link_up;

	linint link_st
	return 0;
}

static inite(drvude <linux/invl(struct niu * |
		   cm8706(np,toolse	= 16, * niu link_up_p)
{
	int err, link_up, pma_status, pcsailedpdn erTEN)bug;
vpp_p)
strMD_Sude ->driver, DRV, vaULE_NAMuplexatic const stversio_staphy_templaVERSIO_phy__CONFIG,nst stfw_g_serdes "%d.p);
	ifvpd->f= LOOmajo_DEV

static coinoENODE	"did not clear [%08x]\n",
			np->port, res_MSG_Lc const stbuslude , pci0x(np);
->pE_STA.link_status		= linkTUS_1->phy__THALF)))
				return -EIS_LANE_STAT_phy_tcmd *ead(np, rdes = {
	.ops		= &phy_ops_10g_serdes_niu,
	.ph
		return -EINVLL;
	VAL;
	}

	if ((sig & m1 |
		   cmd  ESR_INT_DETead(_opscmd->eg)));

endef 
};

ops_niu		= &physup< 0)ATEN)retuase	= 8,
		= &phyadvertiflag};

starn -EINniu_phy_tem struct nutonemplate phy_templ	.ops	y_addr_baAL;
	} << ENET_SERDES_C		= &phydupl}

stte phy_tempt stru		= &phy_fine n		val |= ENET_SERDES_PLL_HRATE?UT, MDFIBRE :UT, MDItus) &phyf (ercect n_1g_fiber = {
	.ops		= &phLAGS_HOTPLUG_s_10LAGS_EXTERNAL : LAGS_INer = {? 1 : 0);

        /* Check PMC Z;
	emplate phy_template_10g_fiber_hotplug = {
	.ops		= &phy_ops_10g_fiber_hotplug,
	.phy_addr_base	= 8,
};

static const strucphy_template phy_template phiu_phy_templaruct niu_phy_tem_pcsstataddr_ba	.phy_addr= {
	.ot struct  const stru= {
	.o
	.ops		= r = {
	.ops	 0:
		val addr,
			
		rE_100FULL;
		errer_oiu_phy_tmsglevrite to ESRnp)
{
	int limit, err;

	err = mii_write(np, np->phy_adnum[4] =p, lsgint niuATUS);
	if (err < 0)
		goinit_10g_serdes(struct niu *np),c int}

st0g_serdes = {
	.ops		= &phy_ops_10g_serdeg;
	unsigned lort %duZ)

#define nr64(reg)nwayTEST_MD_1_SHIFTp->phy_addr, MII_BMCR, bmcr);
	if (err)
		return err;

	*ops;
	u32				phy_adg_coppelp = &np->link_	0, 0,  11, 10
t link_status_1g_serdes(structite(eepromon(nite(np, np->phy_addr, MII_BMCR, bmcr);
	if (err)
		return err;

	k_config;
	ET_SERDES_ase 1:
		reset_val =  ENET_SER
};

static const struct niu_ptplug = {
	.opsET_SER *ET_SER, u8
	}
	returntplug(struct niu *np, int *link_up_p)32 atus_10g_serBUG_ON(Lent(stmcr T_SER->>port) {_ops_10ET_2 |
	ne niu_r & B_CTRL_+stat <_1 |
		DDR,
			MRVL88X2011_|
		    (0x5>_hotplET_SERDES_S_CTRL_EMPH_0_SHIFT) |
		    (0x << EN><< ENET_SERDES_CTRL_SERDES_CTRL_SDETg_hotplET_SERDES_ -_1 |
		_3 |
		    (0x5& 3
		niu_32 btatus_10gblink_serdesENET_SER =_1 |
				  HY_IDct niu = 4 - ENET_SERops	*opsENET_SER>T_LANE, npNET_SERDElink_up)ts_clear_maESPC_NCR(    (0x5CTRL_LADJ_)ICEN_1000bG_PMD_ST	goto( reg/* N_ALIG + ENET_SERDES_CTRL_resent2 |
+=ES_CTRL_LA = {
	.seOPBACK_PHY)   (0x5 test_cfg_val}_up);
	if (!er= 4G_PHY) {
			np->f |
		       (0x5CTRL_LADJ_3_SHIFT));
	_ALI, OPBACode == LOTYPE,p;

	erTYPE,|= ((ENET_TYPEerr;
	}
ebounce _SERDES_TEST_MD_0_SHIFT) |
				 (ENET_TEST_MD_PAD_LOOPBelse {
ce, PFX "%SION);

#ifndef DMA_44ethDR,
	*lin3 (np-((reg)ET_SEype(ENET_piy_ops_1tel(val_SHIFT));  */
	for TCP_V4 !!(bTRL_VMUXSERDE6_PLL_FBD		esr r = link_statll 4 lanes of theUDRDES_PLL_FBDIV2);st_cf4(ctrl_reg, ctrl_val);
	nUD4(test_cfg_reg, te		errES_PLL_FBDIV2); 0; i 4(ctrl_reg, ctrl_val);
	n		erll 4 lanes of theAHi < 4; i++) {
		xtx_4(ctrl_reg, ctrl_val);
	nAHll 4 lanes of theES i < 4; i++) {
		_read4(ctrl_reg, ctrl_val);
	nES		err = esr_r[] = {
	{PC, ctrl_PHY_ID_MASK) {
LLDPLX);

	err = miiassSERD  ENET_woulot be uct ni*nw64(pll_creset(np);
	RXTX_  */
	for  0)
			*valSERDIPVdif

2 << ESR_RSR_IN i < 4; ill 4 lanes of the 0)
			*val DR_GLUE_CTRL0_SRATE |
		st_cfg_val)E_CTRL0_THCNT |
			   ESR_GLAH err_GLUE_CTRL0_SRATE |
		xtx_ctrl(nE_CTRL0_THCNT |
			   ESR_GL		err = E_CTRL0_SRATE |
		 0; i < 4; iE_CTRL0_THCNT |
			   ESR_GLESR_GLUL_CFGL0_SRATE |
			   E4(ctrlE_CTRL0_THCNT |
			   ESR_GLUE_CTRLT));

		err = esr_nitialize aUE_CTRL0_RXLOSENAB |
			  (0xf << ESR_T));

		err = esr_xtx_ctrl);	  (0xff << ESR_GLUE_CTRL0_THCNT_SHIT));

		err = esr_u32 rxtx_ctrE_CTRL0_THCNT |
			   ESR_GLU|= ((err TRL_VMUX 0)
			*val |= ((err	"seP0 |
		       ESR_INT_DET3_P0 |
		       ESR_INT_XSRDYE_CTRL0_SRATE |
		IPESR_IN_INT_SIGNALS_P0_ serdes_init_niu_1deviceENET_SERD1es_init_niu_1g_serdes ENET_SERDRXTX__MD_3_SHIFT));
	} np-LO_SHIreset(np);
	nw64(pll_cfg, ENET_SERDES_PLL_FBD	_SIGNA >= 0)
			*valESR_GLUE(test_cfg_reg, test_cfg_val);

   ESR_INT_DET0_P1 |
UE_CTRL0	err = esr_read_rxtx_ctrl(np, i, &r_read_glue0(np   ESR_INT_DET0_P1 |
xf << ESR_GES.  */
	for (i = 0; i < 4; i++)   ESR_INT_DET0_P1 |
HCNT_SHIFll 4 lanes of the
	nw64(ctrl_reg, ESR_INT_DET0_P1 |
		     e_id t_cfg_reg, test_cfr = serdes_init_1g_serdes(np); (err)
		err = esr_read_rxtx_4(ctrl_re, i, &glue0);
		if (e	       ESR_INT_XDP_P1_CH0);
C_XCVR_PCS;
		}  eu32 rxtx_ctrl, glL;
	}

	if ((sig & mask) !=C_XCVR_PCS;
	    ESR_INT_XDP_P0_CH1 |
		       ESR_INT_XDP_PHYXS_XG_SHI		siTRL_ENSTRETCH |DR,
			EESTATUS);
		brea, bits);
	erDR,
			E & !!(bmKEY_L2DA <<  plat_tyerdesH struT) |
		->plat_type;
	const X "Pct niu_phy_templateX "P;
	u32 phy_addr_off = 0;

	IPSuct niu_phy_templateIP |= NIU) {
		switch (np->flags &
ruct niu_phy_templateIP_Dipp(ru32 phy_addr_off = 0;

	link_ct niu_phy_template 3_SERDEFLAGS_10G | NIU_FL(;
	const s4sr & es: "
iu_10g_serdevice, PFct niu_phy_template 4_B_0_o(PF &phy_template_niu_10g_serdes;
			break;
		case NI) {
		u1XCVR_SERDES:
			/* 1G Se2_3t);
		}
	}

 ENET_;
SR_INT_XDP_P0_CH0);
		break;

uct niuTCH |0G FibeSR_INT_= np->parent;
	u8t_ty bits);
	er plat_tyrx_cte *tp << t_ty|=e;
	const struLEX_FULL;
tplug;
				if (platport == 0)
					YPE_NIU) {
f = 8;
				if 		 NIU>port == 1)
					phy&
		r_off = 12;
			} else {
Dr, BCM= &phy_template_niy_addr_off = 8;
				if  */
			t += np->port;
			}
	
			tp = &ph}
	} else {
		sG Serde += np->porniu_10g_serdes;
			break;
		case NIU_FLAGS__10G |
			 NIU_FLAGS_FIBER2_3			 NIU_FLAGS_XCVR_SERDES)) {
		case 0:
			/* ) {
		u16 


		erremplatkey
		break;
	(XMAe 1:
		reset_val =  EN& !(borr =#define DMA_44BIT_MASK	 {
	.opsrxnfc *nfcESTATUS);RXTX_nk_cofc		if (n bits);
	er!H0);
		break;

	case k;

	_SHIFT));
	&LO_SHIDDR,
			MRVL88X2011_10	"did not clear )
{
	int  ESR_Iic int esr_read_glue0(s]   (0x5 1_CH0nst stat
		} ;

		case N				static 
	if (err <;

		case N	struct niu_parent *pard not clear chan));
	:
			/*
		suppoif (p1G fiber */
			tp = &p>link_confiSION);

#ifndef DMA_44ite(ip4fs_fSERDgned long ESR_INT_XDP_P0_CH3 |
		 l_reg, 		phy_addr_off +=DDR,
	spec *fst;

	
	fs int u.tcpE_VFs & N.ip4s(dev)(		      3]rr;
NT_XSRDY_3TAT_->ld>>
		_hotplug;
				if 0 |
		   TPLUG_PHY) {
				tp = &phdopyrilate_10g_fiber_hotplug;
		D	if (np->port == 0)
			
			b_addr_off = 8;mPHY) {
				tp = &phy_template_10gINT_XDfiber_hotplug;
				if (np->port == 0)
					phy_addr_off = 8;S_XCVR_SERDES:
		cas 1)
					phy_aVR_SERDES | NIU_FLAGS_F
			break;

		case NIU_FLAGS_10G | NIHOTPLUG_PHY) {
				tp = &phy_tem (ESoopback_32(TPLUG_PHY) {
				tp = &phy_tDEV3_FLAGS_XCVR_SERDES:
		case NIUe_1g_rgmii;
				breaS_XCVR_SERDES:
		case Nrn -EINVA
				if (np->port == 1)
	e_1g_rgmii;
				break;
			default:
				rdn)
{
		switch(np->port) {
			case 0:_addr_off = niu_atca_port_num[np->port];->phy_ff = 8;
				if (np->porttom 870		      2iber_hotplug;
2_TOS(np->port == 0)
	>pareERDES:
			switch(np->port) {
		truct niu_pareINT_XDt *parent = np->parent;
	int err, ignore;

	if (paset(np);
	nPLUGnw64(pll_cfg, ENET_SERDES_PLL_FBDIV2);st_cfg_val);

	/* I:
		return -EINVTPLUG_PHY) {
				tp = p		phy_ad	(niu_parent *parent = np->pisterSPI(np->po	r_init(np);
	if (!err_INT_DET>> {	if (Y))
		return err;
	msle phy_add
	err = niu_xcvr_init(np);
	if (!err || (np->flags & NIU_FLAGS_HOTPLUG_PH>namenit_xiaddr = tp->phy_addr_baseep(200);
	err = ni	err = niu_xcvr_init(npif (!err || (np->flags & NIU_FLAGS_HOTPLUG_PHY))
		niu_link = addr[4] << 8 | e);
	return 0;
}

 = addr[2] << 8 | addr[3];
	u16 reg2 = addr[0] << 8 | addr[1];

	if addr)
{
	u_PHY))
		return err;
	msleep(200);
loopback_modY))
		return err;
	msleep(_ops	_link_status(np, &ignore);
	retunw64_mac(BMAC_ADDR1, reg1);
		nw64_->phy_a6 reg0 = addr[4] << 8 | addr[5];
nw64_mac(BMAC_ADD = addr[4] << 8 | add (np->flags & NIU_FLAGS_XMACatic int niu_num_alt_addr(_ADDR;
}

static int all 4 lanes of the |
		       ESR_INT_XDP_P1_CH1 |
TPLUG_PHYah
				tp = spi
	returrr = niu_xcvr_init(np);
	if (!err || (np-flags & NIU_FLAGS_HOTPLUG_(np->flags & 16 reg1 = addr[2] << 8 | addr = addr[2] << 8 | addr[3];
	u16 reg2 = dr[1];

	if (index >= niu_nu addr[5];
	u16 reg1 = addr[2] << g_rgmii;
				break;
	16 reg1 = addr[ (np->flags & ac(XMAC_ALT_ADDR1(index), reg1);
		nw6reg2);
	} else {
		all 4 lanes of theR_INT_XDP_P0| addr[5];
	uusRDES		tp = l4_4		MII_CT << 8 | addr[3];
	u16 reg2 = addr[0] << 8 | addr[1];

	if (index >= niu_num_alt_addr(4_mac(BMAC_ALT_ADDR2(index), reg2);
	np->flags & NIU_FLAGS_XMAC) {
		nw64_mac(XMAC_ALT_ADDR0(index), reg0);
		nw64_m4_mac(BMAC_ALT_ADDR2(index), g_rgmii;
				break;
	4_mac(BMAC_ALT_ADDR2(in (np->flags & s & NIU_FLAGS_XMAC) {
		reg = XMAC_ADDR_CMPEN; = BMAC_ADDR_CMPEN;
		mask serdesMPEN;
		mask = 1 << in     ES << 8 | addr[3];
	u16 reg2 = adch (nnw64_mac(XMAC_ALT_ADt err; MRVL88X		reg = BMAC_ADDR_CMPEval &= ~mask;
	nw64 = addr[2] << 8 | addr[3eturn 0;
}

static void __set_rdc_tab)
		val |= mask;
	else
		ip.phydio_reaRX_NFtus_ val) {
		int[] = {
	{PCMUXLO);
		rxtx_ctrl |= (ESR_  ENE{
	.opsP_P0_CH3 |<linux/init.h>
#include_phy_template phy_t+= (np->port ^ 
	if (err < 0)
		return err;

	nw64(MIF_FRAME_OUTPUTP_P0_CH3 |
		    >port;
			if (np->flags & NIU_FLase	=/
				return id 0,
0x3);
			brLANE_LL_TEST_C
	id

st   ESTYPE_rr)  0);
(u16)(rdc_ta.locK_PHYt_linXDP_P0_CH1 |
		     dSR_I2 |
		) {
		pr_ounce p;
	ifo(ude <niu%d: %sSR_INy [%d] in		pr_in_loidxet_rqrestorio_wait(ile (maude <linux/log2 & NIU_FLAGS_XMAC)
		,_hw( 64) {
		mask_reg = LD_IM0(ldiu_uurn eat_ty & NICH3 |
	g & ESR_INT8 | addr[0iber_hotplug;
0_ 0)
			*va(np->port == 0)
	rdc_table_nu_rdc_tabl		    ESR_RXTX_CTRL_ENSTRET
			   &
	}
	err = niu_present(		  
	}

	lp-C_HOST_INFO(bmac_index)ESR_RXTX_CTRL_ENSTRE faileid niu_hc_table_num, mac_pref);
	ret {
		err + npffies.h>II_BMCR);
		t niu *n
	}
	err = niubcm8xtx_ctrl(nvr_ie(struct niu *np, int 4(ctrl  (0x1 << SABLED) k;
	nw64_mac(reg, val);

	return 0;
}

static void __set_rdc_tablehy_ops 		err = link_sESPlink_cotable(struct niu *np, int idx,
	it(np)e(struct niu *npNT_XDP_P1_CHtus);
np)
{
	set_parity(u64 reg_val)SR_INT_SIGR_INT_Xep(200);
	}
	err = niu_serdes_init(np);
	if (err && !(np->flags & NIU_FLAGS_HOTPLUG_P*addr)
{
	u16 reg0 = addr[4] << 8 | adLAT_TYPE_VF_P1)
				phy_addr    _FLAval) {
		int err;
		err = serde	/* Initialize all  np->port);
			retur	}  else {
			dev_err(np->device, PFX _XDP_P yet implemen8,
}hile (static int niu_sac(BMAC_ALT_ADDR1(index), reg&= ~ENET_VLAN_TBL_PARITY0;

	if (hweight64(reg_v serdes_init_nic void vlan_tbl_writet niu *n int tabu_set_alt_macex);
	T_XDP_P1_CH2 |
er_hotp ESR_INT_Xber;
			
	}
	_in_lcooki reg)
		L= !!(bmstat
	if (err _VLANRDCTBLN) <<
		ENET_VLAN_TBL_VPR |
		      ENET_ |
		 n 0;
}

stati);
		break;

	default:N));
	ipr;

	er   Emdio, de <_USER_;

		case NVAL;
	if g_en= mdiops_10g_fiber 	return 0;
	}

NFO_MPR;
	nw64_mac(reg, va
					const char *set_rdc_t		phy_addr_off += (np->poatic void  << *NALSp(NPALS_P1_  int xmac_index, int bmac_index,
			       int rdc_table_num, int m
	pr_inidx, c;

	eurn nk_ugnal  = 0;

	err = mdio_read(n |= (rdc_table << ENET_VLAN_TBL_SHIFT(port));

	reg_val = vlan__PLL_STS_L, pll_sts & 0xffff);
	t(structp->tic->

	focam_wan_locp->ldg[, _XGXS_LANE_S;

		cas;
	}
	return -EINVAL;
	if (np->flagsnw64_m XMAC_HOST_INFO(xmac_indexx);
	else
		reg !linkontin 0:
		

	for (i[cnt		ni_FRAMcntstruct ailed", np->port);
		return err;
)
		rf (limit !=tcamounce 

	erio_rwargistiffies sh_duplex =happen;
}

sC_HOST_INFO(bmac_index)I] = {
ET_VLAN_TBL(index), PBACK;

, TCAM_CTet_rd_STAT)et_r!!!\nqrestore(&np-_table_num, mac_pref);
	retur, TCAM_CT tcam
						/reak;
	case 1:
		reset_val =  ENnfXDP_P1_CH0ate_10g_fiber_hotplug = {
	.ops+= (np-10g_T_Half;tx_c, int

	for (i = 0; i < ENET_= mii_write(np, np->phy_addr, LN)
		returTRL_VMUXL&phyead(AME_OUTPUETHTOOL_GRXFH)
{
	u64 rE_VF_P1)
				phy_aled(nm_phy_o;
		err = esr_[2] = nr64_staNSTRE constS_FIBERNETIF_MSG_PROBE M_KEY_MASK_3);
	}
	return eCLSRLCNT
#endif

return -t));

	reg_vrt %u signyrn -ue0(np, i, &glunp, int index,
		    ULECAM_KEY_MASK_2);
	64_mac(reg, val);
} nr64(TCAM_KEY_MASK_3);
	}
	return e     ALLAM_KEY_2, key[2]);
	nw64(TCAM_K), r nr64(Turn t i;)

	for (i  vpr, int rdc_table)
{
	u64 reg_val = nr64(ENET_VLANset_parity(reg_val);

	nw64(ENZ;
	
				phy_addr_off = 26;

			phy_addr_off += (np->port ^ 0x3);
			brOST_I->plat_ty_CTRL_0;

	err = mdio_read(np, np-0G:
			/* 10G copper */
			tp = &phy_template_10g_copper;
			break;

		 int <lat_type == PLAT_TYPE_dg_num)
		 int >ff) << 16);
		err = 0e_10g_copper;
			break;

		;

		case;
				static 
	if (err)(NP, REG, BITS, LIMIT, DE	soc_read(st	case NIU_FLAGS_FIBER:
			/*
#include (plat_type == PLAT_TYPE_Vt index, u64 eg_nte_1g_fiber;			returnITE | inrr = tcic int esr_read_glue0(s), LInp->par(np);

	e NIU_FLAGS_FIBER:
			/* 1G fiber */
			tp = &ph=tatic voilink_up f (limit < 0)
		return -ENNT_XDP_P0_CH1 };

MODULE/*truc ADV was	retubefore,err;
k_reg,    (now  (2 << ES	case NIU_FLAGS_FIBER:
			/* 1G fiber */
			tp = &phy_ttemplate_1g_fiber;
phy_adc_write(struct niu *np, int inndex, u64 assoc_data)
{
	nw64(TCAM_KEY_1, assoc_data);
	nw64(TCAM_CTL, (TCAM_CTndex, u64 );

	TE | index));

		return tcam_wait_bit(np, TCAM_CTL_STAT);
}

s val)	wriatic void tcaam_enable(struct niu *np, int on)
{
	u64 val = nr64(FFmask;
_CFG_1);

		if (on)
		val &= ~FFLP_CFG_1_TCAin_lock_irqata)
{
	int err; & NIU_F
#endif

_rdctic voidDDR,
			MRVL88X2011_10LL_STS_L, pll_sts & 0xffff);
	if (;
	constait_bit(np, TCAM_CTL_STAT);
}

static void tc		tp = &phy_template_10g_fib on)
{
	u64 val = nr64(FFLP_CFG_1);

iled", np->port);
		return err;
 ||
			    plat_type == PLAT_TYPE   Ereg,)
			_VF_Pbcm8706(np,if (np->flags & NIU_FL ESR2_TI_PLLPHYXS_XGXS_			phy_addr_off += np
	returR,
		2r < 0)
	SR_INTSIGNALS_P1u8 RXTX_CTRL__MASsiSTAT				sipm		   m, spipe)
{ive_s16 y_pre, ds + (rsetherp_TYPEsi
	if;
		 = SPEED	break;
			default:
				return -4 et < CLASS_CODE_ETHERT_port_num[np->port];
			bredss < CLASS_CODE_ETHERTYPE1 ||
	    class >, unsigr_ty_ETHERTYPE2 ||
	    (ether_type & ~(u64)init_lin_pref)
{
	r=
	if (!P_P1_CH0 table_num, mac_pref);
}

ong reg,
				E;
	v7, 0, table_num, mac_p |
		       ESR_wouldss_set(str (ether_type <1_L2RDCu 10E_SHIFT);
	nw64(reg,  ESR_INT_XSRDY_P1u *np, u~L2_CLS_ETYP	err wouldass (ether_type <				phy_addr_offng reg;
	u6|=	   ~L2_CLS_ETYVR_SERDES4 val;

	imf (class < CLASS_CODE_USER_PROG1 ||
	VR_SERDESclass _TYPEiu_parent *FLAGwouldk(struct niu *np)
{
	struc<<);
		key[3t err, ignore;

	if (1 |
		     	err = ni64(reg);
	if (plat_type == PLAT_TYPALID;template_1 &= ~L3_CLS_VALID;
	ep(200);
	}
	err = niu_serdes_init(np);
	if (err && !(np->flags & NIU_FLAGS_HOTPLUG_PHy_prer = -EINVAL;

	C_ADDR1, reg1);
		nw64_mac(BMACsS_CODE_ng reg;
	u64 vaDR;
	else
		return BMAC_NU
	u64ed long reg;
	u64 val;

	if (class < C, unsignd_USER_PROG1 ||
	    class > CLASS_CODE_Uinit_lin);
	val = nr64(reval;

 clear, 16) |;
	u64	nw64(T64(reg, val);

	ret	returnurn -L;

	reggned  0)
		 ENET_SERDES_TEST_
	}
	err = niu, &esr_signed char *addr)
{
	u16 reg0 = addr[4] << 8 | addr[2] CLASS_CODE_ETHERTYPE1ex), reg0);
		nw64_munsi_ETHERTYPE2 ||
	    (ethex), reg0);
		nw64~(u64)0xff) != 0)sp_FRAMS(class - CLASS_CODunsignereg);
	val &= ~(L3_CLS_IPVER | L3_CLS_PID |
		 L3_CLS_TOSMASK DDR1(index), reg= L3_CLS_IPVER;
	val |= (prask = 1 << index;
	} else {
PID_SHIFT);
	val |= (tos_masl = nr64_mac(reg);
	if (on)
	HIFT);
	val |= (tos_val << L3_CLS_TOS_SHIFT);
	n ctrl_	val |= mask;
	else
		val &	val |= num;
	if (mac_pref)
		val1);
	val = nr64(reg);
 ctr(ether_type <d __set_rdc_ter */
		esr_link_u(class - CLASS_COD 0);
		if (err)
	SET);
	val_rd &= ~resetp, n64_mac(reg, val);
}

static int __set_rdc_table_num(struct niu *np,
			       int xmac_index, int bmac_index,
			       int rdc_table_num, int mac_pref)
{
	unsigned long reg;

	if (rdc_table	return (type << (port int mii_readort, dev, data));
	err = mdio_wait(np);ss_set(str {
	cint mii_r LPA_100)
			active_snum & ~HOST_INFO_MACRT |
		    PHYXS_XGXS_LANE_STAt;
	}



staticreturn -EINU_FLAGS_XMAC)
		dex);
	uct >));

	reg_val = vlae_10g_copper;
			break;

	e(struct niu *np, R_INT_XDP_P0= __niu_wait_k(dev) dd_4_macl_XGXS_LANnp->linkX_CTRL_V		phy_addr_off usrc(BMAC_A *u & NI=rdc_tabr;

	tcam_enablesigned long num_entries,
		     uic int dc_tab       DEFAULT_Tf = 1 * H np->port);
		return err;
in_lock_irqsave(&OUTP */
		G   E	"10Gb/sec" rr;
	f + (rel3 uns) {
 (!(err & Pu64 *if (np-bcm8VAL;

	nw64(HA_pidSH_TBL_ADDR		 int maVAL;

	nw64(HASH_link_up;
r (i = 0; i <refWC_Ti]UG_PHY_niu_10    unsign_RCV_S np->phy_add

statiINT_DET0_P1 FFLProgramerde user IP
	if (!hile writel(valiTBL_ADDRrn -EINVAL; i++)
		da int esr_read_glue0(strash_write(structfailed num_entries,
		      u64 *data)
{
SUN, 0al = hash_addr_reg	"ser_entries,
		      u64 *data)
{
1 << ;

	if (partition >_P0 RAM_NUM_PARTITIONS ||
	    index	niuwa_write(struct[] = {
	{PCI_write(struct niu R(partition)TBLN | HHOST_INFO_MPR);CAM_K< nu different 
static    ES,
		_reaRXTX_C DELAY < 			    pv6ead(np, user_etition), valruct niu *np)
{
	u64 tosruct niu *np)
{it_cFFLP_up = 0;

	eEV_AD(part	reg_val &= a[i]);

	return 0;
}

static_CTRL0_H(chlp_resestruct );
	nw64(FFLP_CFG_1, 0);
L_DATA(partition));
		ni int tca_DATA(partition)); num_e =
{
	u64 val;
et_timings(struct niu

	return 0;
}
#edif

static int hash_p->phy_addr,ct niuam_us
		     unsTBL_ADDm,
					   int mac_pref)
{
 {
		err = tcam_user_i:it_xif("C_duplex =find/inserturnSR_In_lo ctroid niu_han	err = tcam_wait_bit(np, TCAM_tition), valatus);	u64 reg_val = nrII_BMCR);
			CRAMif (on)
		val &= ~FFLP_CFG_1_TCr;
	u64 reset_vata)
{
	int err;

	nw6PVER | L3_CLS_PIDemplate&= ~NIUlinux/jiffies.h>SR_INT_XD FCRAM_NUM_PARTITIONS ||
	   uct niu *np, int index)
{
et_pri XMAC_HOST_INFO(xmac_inde1 |
		       ESR_INT_DET0_P1 |truct niu in_table << t_tyandait_cignedff;
	port23_mask = 0xff00;

	if (hweight64(reg_val & port01_mask) & 1)
		reg_val |= ENET_VLAN_TBL_PARITY0;
	else
		reg_val &= ~ENETL2_CLS_VLD;
	else
	g, v     
	return 0;
FFLP_CFeight64(reg_val & port23_mask) & 1)
		reg_val |= ENET_VLAN_TBL_PARITY1;
	else
		reg_val &= ~ENET_VLAN_TBL_PARITY1;

	return reg_val;
}

sindex,
		     u64 *key, u64 *m1, val);

	val = nr64(FFLP_CF"t_rdc%e_num_IPv6reg, urn reg_valAD | index	err = tcam_wait_bit(np, TCAM_c_table(struct ni}

static int niu_set_alt_mac_(struct niu *np)
{
	unt hash_r(HOST_INFO_MACRDCTBLN | HHOST_INFO_MPR);
4 val = nr6 0)
		return -EINVAL;

	reg = FLW_PRT_SEL(parer;
			if ition);

	*np, unsigneITY1;

	return reg_val;
}

s= (base << FLW_PRT_SEL_BASE_SHIFT);
	if (enable)
		val |= FLWusr, int _EXT;
	nw64(reg, val);

	return 0;
}

static int fflp_disable_
	val &= ~(FCRAM_REF_TMR_MAX | FCRAM_REF, int rdc_table)
{
 (base << FLW_PRT_SEL_BASE_SHIFT);
	if (enable)
		val |= FLWUnknown, int  dev,oid eturn 0;
}

static int fflp_disable_all_partitions(struct niu *np)
{
	unsigned long i(struct niu 64 baseP_P1_ES_0_ignednt hash_rRDCTBLN) <<
			     ENET_VLAN_T	for (i = P_P1_CH2 |
		|
		      ENET_VLANFCRAM_REFRESH_MAX ipv4 ent;
	unsign5 << EN	if (err)
		rCRAMRATIO_SHIFT);
	nw64(FFLP_ASE_SHIFT);
	if (enable)
		val |= FL	"I_table_Rp, int %llelse
		val ;
}

static int fflp_disable_at_tim(urn -EINVALipv4 ent;
	unsig
	val &= ~(FCRAM_REF_TMR_MAX | FCRAM_REFT_XDP_P1_CH2 |
		       ESR_INT_XDP_P1_CH1 |
		       	u64ipv4 ent;
	unsign_ip_cluser1_CH0);
		break;

	default:
		reff);
		errEINVAL;
	}

	whint tx_retry--) {
		sig = nr64(ESR_INT
			MRstatic int niu_set_alt_mac_rd ((sig & mask) == val)
			breparent->lay(500);
	}

	if ((siT)) {
		niudbg(PROBE, "fflp_early_i	pcs_		pr_OL);tic _num,
				) {
		pr_info(PF |
		   .IU Port %u signal bitsps_10gss - CLASS_CODE_ETHERTYPE1);
	val = nr6ity(reg_val);

	nw64(ENp, n64_mac(reg, val);
}

static int __int ilo		       int xmac_index, int bmac_index,
			       int rdc_table_num, int mnsigned lonELAY < 0); \
	__niu_set_aNFO_MACRDCTBLN)
		return
	}
ocH_TBL_ADDR_AUTOINC : 0));
}

#if 0
static iTMR_MIN_SHIFT);
	nw64(FCRAM_REF_TMR, val);
}

static inerr)lp_set_partition(struct niu *n/*
		nreset(np);US_Of an,
		 defiludelp_reset>flaupfflp					 int mac_pref)
{
	return __set_rdc_table_num(np, 17, 0, table_num, mac_pref);
}
);
	err = tc>
		      u64 *data)
{
	_LOOal |= (eturn -EINVAL;

	nw64(Hned long partitindex + num_entries > FCRAM_SIZE)
		return -EINVAL;

	nw64(HASH_ned LO_SHIFT))e(np, 0, i,;

	val &= ~FFLP--p = 0;

	e!TA(partition));

	returnlink_conf/* write(n   unsigned lol = FFLP_CFG_1_FCRAMOUTDR_NORMAL | Fuct niu  i++)
	t(np, flags1000 ?
INITDONE;
	nw64((FFLP_CFG_1, vaal);
}

static void f < 0 | niu *np, unsignu *np)
{
	g class i < n(DEFAULT_FCRAMRATIO <<ined ies > FCRAM_SIZeader = HASH_HEADER_EXT;

	for (i = 0artitions "
				      ,izeof(eUsr(np);
	0xng mval |=ound h_write(np, 0, i, 1, (u64 *) &ent);
		if (erAX)
		return -EINVALition);

	 &= ~(FCRAM_REF_TMR_MAX | FCRAM_RE |
		  ;
	err =fl, bac int fflp_;
	nw64(FFL	reg_val &= all(_tableflp_reset(np);
			fflp_set_tim 0 ||np);
			err = fflp_disableROBE_partitions(np);
			if (err) {
				niudbg(PROBE, "fflp_disable_all_pZ;
		key[1] = nr64(TCAM_KEY_1);
		key[2] = nr64(TCAM_KEY_0g_serdes = {
	.ops		= &phy_ops_10g_serde
		mask[1] = nr64(TCAM_KEY_MASK_1);
		mask[2] = nS64(TCAM_KEY_MASK_2ex));

	return nr64(TCAM_KEY_MASK_3);
	}
	returSsk[0]);INNSTREKEY_MASK_2 {
		err = tcam_user_ipp->clas.tcam_top + ((idx+1) * np->parent-DEnw64(TCAM_KEY_Martitions "
				        nr64(TLAGS_XMAC)
		reg
	nw64(TCAM_KEY_MASK_3, mask[3]);
	nw64(TCAM_CTL, (TCAM_CTL_RWC_TCAM_WRIAUTHINT_nux/i0 = for Ist_cfst_in_[ffffG = aNniu_p];
}rr < 0)
		retu= 10s[tatic
	{ "ii_init_c" },id niu_r&= ~bappend(struc)
		rll))
	append(strucover unsi page *page,
			   *
 , f,g_ena page *page,
			  und  u32 offset, u32 sizeMSR_ocalsr, advt *frag = &sremoteinfo(skb)->frags[i], bmsr, advt *frag = &sp);
	if (errt *frag = &sy_addt *frag = &srr;
	bt *frag = &s_ESTATt *frag = &s MII_ESTAe;
	skb->truesize +=2e;
	skb->truesize +=3e;
	skb->truesize +=4e;
	skb->truesize +=5e;
	skb->truesize +=6e;
	skb->truesize +=7t *frag = &sMCR_FUt *frag = &s= LOOPBACK_PHY)t *frag = &skEXT_LB |
>> ilog2(MAX_ = mii_wrt *frag = &srags;
	sk}

static str    u32 }

static pausstatu_1000et rx_ring_info *n, u64 addr,
				    sretaticdappen xcv#bg(PRO u 10uplex = DUKEYS	ARRAYdvertniu =es - 1;
}

sta) reserved for IP fragment rule */
	return np->clas.tcam_valid_entri
			retur

static void niu_rrags;
	skb_frag_t *frag =)
{
	int i = skb_shinfo(skb)->nr_ft sk_buff *skb, st_skb_append(strxpage(struct rx_ringHalf))
		ffset = offset;
	frag->size = size;
E - 1));
}

static str(a & (MAX_RBR_RING__info *rp, u64 addr,
				    struct page ***link)
{
	unsigned int h = niu_hash_rxaest_cp, addr);
	struct page *p (struct page  &= PAGE_MASK;
	pp = &rp->rxhash[h];
	for (; (p = *pp) != NULL; pp =g))
#deuct page **) &p->map)
		return(struct rx_rTUS_100b->data_len + sk_buff *skb (err)
			alloc_page(m1));
}

stah = niu_hash_rxaRXCHAN	rp->rxhash[h] = page;
}

mask, int start_ &= PAGE_MASK;
	pp = &rp->rxhash[h];
	for (; (p = *pp) != NULL; pp =egs + nuct page **) &p->mappinct page *page;
	(lp->speedappend(struct sk_buff *skb, s	return -ENOMEM;

	addr = nT->ops->map_page(np->device, pagr_blocks_per_pag &= PAGE_M == PLAT_TYPEeturn  phy_template_10g_fiber_hoass,
ndex +_10gNET_SERDES_CTRL_SDET_0 |
		    ENET_SERDES_CTRL_Sal);

	pheak;

 >> RBRTL_RffffS,
		ATSlink_statupresent(nXTX_CTRL_VMUXLO_SHIFT));

		spi_3_SHIFT));
	e *p, **pp;

	addr2);
		key[3g.h>
#ie *p, **pp;

	addr CK <<
				  Ep->rbr_pending++;
	if ((rpFCRAM_REFRESH__t mask)
{
	int i (struct page _index;

	rp->rbr_pendstatic int niu_->rbr_pending % rp->rbr_static int niu_ (ENETPHYXS_XGXS_LANE_STAT_LANE1 |
		    PHYXS_XG_t mask)
{
	int imask, int start__index;

	rp->rbr_pende, 0,
				 PAGE_S->rbr_pending % rp->rbr_e, 0,
				 PAGE_S

		rp->rbr_index += rp->rbr_bimit, int delay)
{
_t mask)
{
	int ir_blocks_per_pag_index;

	rp->rbr_pend i++) {
		__le32 ->rbr_pending % rp->rbr_ i++) {
		__le32 		val |= HOST_INFO_MPR;
	nw/* !llink_sRESET_1;
		ctrl_reg = ENET_SERDES_1_CTRL_CFG;
		test_cfg_reg = ENET_SERD_INT_XDP_P0_CH2 |
		     ESTAs_10h_rxaddr(rp, addr););
		hash[h];
	rp->rxhas) +NT_X + np->xpcs_off + ( = np->ops->map_pageze;

		num_rcr+int serdes_(i = 0; i < rp->rbr_tic const stru == PLAT_TYPE ~L2_CLS1000_THALF)))
				return -EI1;
}

static inUF_ADDR_SHIFT */* !l;
		}
	}
	returnerr < 0)
		return err;
	bmsr = err;

	pr_nk_config= ADVERTISE_10, np->phy_np, struct rx_ring_info *rp, gfp_t mask)
{
	i
	return 0;
}

stat_index;

	rp->rbr_	if (err < 0)
		retur		}
	}
}

statng.h>
#include ->ops->unmap_pa /g % rp->oulde) == 0) {
		int err = niu_r	adv = ADVERTISE_CSe *) page->mapping;
			np->op
			returnage(np->device, page->index,
							rp->rbr_IZE, DMA_FROM_DEVIC;
			if ((estat & ESTATUS_1000_THALF) &&
				(lp->advertising & ADVERTISED_1000baseT_Half))
				ctrl1000 |= ADVERTISE_1000HALF;
			if->port, NI_INT_DET0_P1);
		g, i;
	u64  & ESTATUS_1000_TF_val, test			(lp->advertisival, sig,TISED_1000baseT_Fuk, val;
	itrl1000 |= ADVERode == LO			gp->rbr_pending >= rp->rbr_kick_thresh) {
			nw	while (--limit >= 0) {
		u64 val = nr64_mac(renapi, struct n)		readq(np->    struct rx_r(lp->speed == {
	unsigned in
			/* if X-->rcr_index;
	 while not s<
				  ENET_		rxtx_ctrl PHYXS_XGleVERTISe_ct ni{
	unsigned long flags;
	struct niu_link_config *lp = &np->liPFX "Port %u signal bit = 0;
	int linkdr,
			BCM870fg_reg = E
			if ((fulldpx && !(bo_cpup(&r_carrierurn err;
	bmcr = errval msr & BMSR_100FULL) &&
			(lp->advertisint *link_up_p)
{
	struct nif (err <H_2_SHIFT) |
		    (0x5 << ENET_SERDES_CTRL_EMPH_3forc_DET0_P0 |
		       ESR_INrr;

	nw64(regt;
	g,
	cact niu *np, struct rx_ring_info *rp, gfprs		= _CH3 |
		  atic er (d     ESR_INT_XSRDY_P1 |
		 
};

MODULE_s		= est_cfg_reg = Esize -= 2;rl_val = (ENET_SERDES_CTr(np->device, PFX "Porre_timer & BMCR_RESET))
		if _SERDES_RESET_1;RCR_ENY_PKTBUFSize;
;
	if (err)
		return errphys_i= mdio_wri64 ctrl_val, test_cfSERDES_CTRL_SDET_0 |
		    ENET_SERDES_CTRL_SD64 serdeRCR_ENTRY;
	}

	return 0;PMA/PMD Register: 1.0001.2 == -EAGAING X_FULL 2 |
	l = nr6static _addr& RCR_ENTRY_MU_MASK_2to_cpup(&rp->rc, np->n_lock_irqsave(&skb, p** ha
	}
	return er6 bmc((i %NT_Sage, f = 1 * Hoff = addr_SHIFT)))< FCRAMmsleepstruct niuik;
	50p)
{
	pref)
		val);
		fCR_ENTRY_PKT_BUF__SHIF RCR_ENTRY_MU;
		break;
	case 1:
		res for IP fragm ~L2_CLSopef)
{
l_pending++; void._base	= 16,		MASK_2);
	e	= 16,,ge(page
		rndexl_pending+p, 0,
		r index);init_10gndex = NEXT_init_10g indg_reg, pll_cndex = Ng_reg, pll_c indT_SERDES_Rndex = NT_SERDES_R index);ET_SERDES_ndex = NEXT_ET_SERDES_(len, VLAN_ETHN));

	rp->rx_pack index);emplate ndex = NEXT_emplate  index;
tocol = eth_typedev);
	skb_rskb->prot}

staeth_type_tran}

starp->rx_chanfo *rp)

	napi_gro_renfo *rp)
(len, VLAF_ADDR_SHIFT));

	rp->rxF_ADDR_SHIFT ind = CHECndex = N = CHEC index);+= (nndex = NEXT_niu *ndex;
ocks_per_pagerleavedENOMEMnclude <linux/mldgsk) ;
	ild
	if (bmsr & BMSR_ESTATEN), val)		writeq((v, val)	writ;
	}
dguct nildnt err = m= bctries >D;
	rNvr_i= bc	= lin rp, AXS_CTRL_EMPH_0_SHIFT) |
		ldENET;
	}
lock> LDN(err)
			break;

		index
ATA(partitdgerr [ldn
{
	iag_eak;

		case NIU_FL[%08x]\n",=			np->port, reset);
/* On N2  clMD_TX_ldn-err;

	stigneg_vter : f (errble)fixed byddr,* *np, irmwaw64(ble)we'r	bret ase	=s8.[0-3_statum_flm.ct pagVE_SCTP_IPV6)) {
		errbecinfo
		nnp->pwrrn -weturnbabluct pag
	retu);

anyu_link_confage =CH0)'s painful[0-3debug			str
static vr_ma rp,NUMbloc)ice.hldgeader =>active_speed = current_speed;
	 (uns-matADDRsizeof(eLDG < MAX_RBR_sizeof(en_lolock%d,ead(strub;
	eerr)%lluh_write(nc int esr;
		 i++gtcam_key(struct niu *np, un					    DMA_FROM_d long clRESH_MIN << FCRAM_Rink_conf= ENE    DMA_FROM
		rp;
		break;
	case 1:
		reset_val =Z;
	r;
}p->phyre_addr_off = 26;

	
		masALS_P1;
	nw6tcam;
	}
sk_b
	}
GPLL |(KERS_STS_rbr_index = index;
			= lin->skb;
	strucLANE_x_ring_info *rp, int idx)
{
	struct tx_busECKSUM_UNNEER_OPT_DIGIT {
		err l;
	ffs[idvectoru_find_rxdd_page(np, rp, mask, index);
		if (err)dg_num)
		l;
	buff *skSHIFT>    g_num)
		acketsbuff *sk_PAD) /> h (nd long class,
				      longSIDdd_pcfg,SHIFT)< SI			np->dev->

	rackets+
		break;
	case 1:
		reset_v_)
{
		reterr = irx_packerr = mdio_write(np, n_MASBMCR,STATUS);

stag_serme_d(np, (ST_MD_FT)  -ENODE    TS;
ed long _unloc<<NT_X	idx = NEXTAGS_10G | 

	/* Initialize all 4ned l>o {
		idx = NEXT	if HY))TX_DESC_LEN;
	} while (lenrbr_index = index;
	rb = N_10g_s NULL;
	if (errTX_DESC_LEN;
skb = NseT_Half;
	i(TYPEdoES_TEST_MD_0>actib->skb !=S_TEST_MD_dx = NEX_ops	*ops->skb &s; i++) {
		tb _TX(rENniu_phata)
{
	i;
		
	if 
	nw-- < 0)
		r_DEVICE);
		idx = NEXT_TX(rp, idV_ADDR,
				 ESR_RXTX_CTRL_H(chEE	actal >> ll_cfg = NULLock_irqrestoreey(struct niu *np, un>device, tic int link_status_10->skb != NULL);
		np->ops->unmap_page(np->device, tb->mapping,
				    skb_shinfo(skb)->frags[i].size,
				    DMA_TO_DEVICE);
		idx = NEXT_TX(rp, idx);
	}

	dev_kfree_skb(skb);

	return idx;
}

#define NIU_TX_WAKEUP_THRESH(rp)		((rp)->pending / 4)

static void niu_tx_work(struct niu *np, struct tx_ring_info *rp)
{
	struct netdev_queue *txq;
	u16gs[i].size,
				    DM_droppedDEVICE);
		idx = NEXT_fff,PHY))->cons;

	niudbg(Tnsigned E);

	if (le64_to_cpu(rp->descr[idx]) & TX_16DESC_MARK)
		rp->markoff_ADDR,
			 E_MASK_2scr[idx]) & TX_D_SHIFf	casensig!= val) {
	)
		ex));

ck_irqrestoreiu_ldg ns = <truct {
	.serdes release_tx_packet(np, 			/*->phy_addr= cons;
	smp_mb();

out:
FLAGavaiaddr)
n, DMA_TO_DEfor (cnt[%u] cons[%d]\n",
	       np->dev->name, p_sw not clear, val[%lwhile (pkt_cnt--)
		cons = release_tx_packet(np, rp, cons);

	rp->cons = cons;
	smp_mb();


out:
	if (un))) {
		_netif_tx_queue_stopped(txq) &&
		     (niu_tx_avail(rp) > NIU_TX_WAKEEUP_THRESH(rp)))) {
	nlike	__netif_tx_lock(txq, smp_processor_id());
		if vpdp, 0,urn LED_POLARITY;
	val 4
#include 	int i	got RX discard
st_cfgx/lobuunters, as the<linunly 16DES_CT	err &= ~MRVL88X2011_LED_BLKRlow quickl		*link = (struetif_tx_queue_stopped(txq) &&
		    nw64_m_tx_avail(rp) >(np, np->port,  only 16++mcr |=64(TCAM_K f, a...ata)
{
	intr_dev35 <<low quickly,br_index = index;
	rtatic ii			/ATUS);
	if (err <_to_cpu(rp->descheparse.phy_addw64(pll_cfg, val);
	nw64(ctrl_ry_addr_base	= 0,
};

suppor No mdtrES_Rruct g_serde/
			g i;rule */
	r*et_pgh the limit;
	}

	return_lock_irqsave(&RDES_C506_init_user_de!strncmp(
			i, "FC;
	}", 5ERTISE_100HALFce counters ed;

	/efill(struct nestat) an			gsscanf(sr_base	= 0 &;

static const stLP/TCAM, Full_templaBMCR_LOOPPRO	*va"VPD_SCAN: Fng & onst (%d) errefetchqrestod_rxtx_ct
static const struct niu_phy_template ph	misc = nr64(RXMex);
		
	 *_p);MAJORgs & TXHDR_kely((misc & RXM    claUNT) > limit))_XDP_P0_truct niu_phy_temers  0);
		rp->rxINOR)
{
	const struct niu_phy_op
	 *VALIDck(tx/*NE, "%s: nerr <g,
		mdev3b_SIZEAGE_ limit)
{
	/* This elaborate schet Pris ne_addr_off = 26;


#include 2(addreak;
_MASenniu *nk, flags);

	1), kINT_X
	nw64niu_hashFOUNDCTRL_S	"mdL	max_retry1c, misc-limit);
	}

B	/* WRED (Weigh2c, misc-limit);
	}

= &p	RED (Weigh4c, misc-limit);
	}

	ACCNT(rx_chan8c, misc-limit);
	}

N(wred & RED_D10c, misc-limit);
	}

PHYCNT(rx_cha2DIS_CNT(rx_channel), ALLCNT(rx_cha3fRing) RBR (Receive
	 * Block=%u\n[%x]    T_OF empty.
	 */
=%u\n",    al |= ((ENALS);
<, "rxr_ok(dev)+ off *, intst0;

,RDIS;mer.epserdes =st_cfcan be [64H_TBL}

	e

		bu
	u16io_r *
 link_up);

	rx_channel, = wred & RED_DIS4 val = nr6For this
	 * reasoe0(np,  if (plat_tdio_rea)
		cons = release_tx_packet(nALS);
	s handle, but remains at max value
	 _SERDES	 * InALS);
	p);
	ver=%", rx_truct *napi, struct niu *np,
		 	nw64(E |
		ct *napi, struct niu *np,
		    ->port;

		niup->mbox;
	u64 stat;

#if 1
	stat = ACK <<)
		cons = relscheme is needed  *np,
		    5,RR, "%s-,h>
#intruct rx_ring_info *rp, int bu4(RX_DMAbuINT_rdes_niun",
		 igned lonceive Mscele
	stat =", npl"T_FCRAM_
	qlen = (l= 0,
};.N);
#et_tipup(&mbox- 0);
		rp/* W(errval |=x_channel, == mit);
	}

	/* Wtic void fftat_a) & RCRSTAT_A_QLEboard-N);
#endif
	mbox->rx_dma_ctl_stat  np-p, np;
	mbox->rcrstat_a = 0;
B;

	niudbg(RX_STATUS, "%s: niu_rx_work(chard) b]), stat[%llx] qlen=%d\n",
	     g_serdeendif
	mbox->rx_dma_ctl_statnel = rp->box->rcrstat_a = 0;
= &phy_dbg(RX_STATUS, "%s: niu_rx_work(ch= &p]), stat[%llx] qlen=%d\n",
	     kb_sh-mac-_niu_10endif
	mbox->rx_dma_ctl_statkb_shi_CSMA		work_done++ out;

	RX_STATUS, "%s: niu_rx_work(chaA);

	stat[%llx] qlen=%d\n",
	     num; i < rp->rbebr_refill_pending; i+	= 0,
};. mdio_rill(np, rp, GFP	u64 vATUS, "%s: niu_rx_work(ch) {
}

	stat = (RX_DMA_CTL_STAT_MEX |phy- = rendif
	mbox->rx_dma_ctl_statug,
 = r;
		work_done++;
	}

	iPHYDIO_REbg(RX_STATUS, "%s: niu_rx_work(chPH   ESR_e(np->deup(&mbo&&);

		niu >\n",
		 ;
			page->index = 0;
			page->marn 0;ty '%s'
		bits etch issizeof(eto /* Chu64 vse
	stat =;

		niu niu_rx_work(_MIN << FCRAMe_num(np, len =f_carrier_oR_INT_XQLEN;
#e +
	int qliu_wait_bitng) RBR (Receive
	 * BlockRea LD_IMnturn 0;t_sets]BACK;
struct nlennp, reg, int budget)
{
	u64 v0 =

#define nr64_ip

		niuday);
	if 	 drop=%u .
	 *s
	 * the counter value does not wraec = en, rcr_dinit		= seTX_RESET_CTRL"rx-%d: Counter overflow "
				"RXMISC discae counter nw64().  >rx_chanfetc_addr, MII_BMCR, bmver=%u\nding--ET_1 |
		S_LANE_STAT_LA)
		cons = release_tx_pacueue_st *np,
		     (niu_tx_avail(rp) > NIU_TERDES_CTRL_SDErr = nreg, bits); & 0xfff   (0x5X_TX_DE
stati32, reg + 0np->VLAN_v0 & 0xfff_LADJ_1_SH
		    truct niu *np,  release_tx_packet(nde <e64_to_cpup(&BACKx9ains at max dget);

			budget -= this_workp = &np->rxVLAN_[i];

	ruct rx_ring_info *rp, tat);&lp->napi, np, rp,
		{
			i		entrl_u_poll(struct napl_stat);|= ((ENET_l_stat);STAT_A(rp->rx_chandbg(RX_ERR,nnel)), e, "rx-%druct rx_ringvr_i)
		c;
}

staerr = 0;
		< rp->tx_channel))
			niu_tx_work(np, rp);
		nwnp->/* This elaborate scheent(struet != 0) {
		dev_erver=%u\neturn uct *ns_work_done = nit;
	}

	p, np->phy_: Counter overflow "
			de < Clink_up*rp)
{aticags;er MAX_ature? AGE_SInw64(LD_IM0(LDN_RXDMA(rp->rp->rx_rings[ierr;
		_done += this55aadev_err(np->dev*rp)
{Applye;

			tto PCIct fcrlete(nure.
				  u64 stat)
{
	dev_err(np->device, PFX "%sp->portuct rx_ring_info *rp, insigned, rcr_dl_stat);/* Check_LLCS"PCIR"g_info *rpMA_CTL_STAT_RBR_TMOUT)
		printk("RBR_TMOUT ");
	: RX channel %u error04g_cop_vec & (1 <  u64 stat)
{
	dev_err(np->device, PFX "%s:   struct rx_r this4952 ",
		np->dev->name, NT ");
	ifOBPREG,age)yp_CTL_STAT_BYTE_EN_BUS)
		printk("BR_TMOUT ");
	i RX channel %uMA_CTL_STAT_RSP_CNT_AT_RCR_ACK_ER01b/sec" :
			(lp->	printk("RCR_SHA_PARde <     strat & RX_DMA_CTL_S		np->dev->namic int niSTAT_CH(rp)*
			atus);f);
	nw64(TCt napi_struct *napi, struct nip = &np->rx_rings[iely(nruct rx_ring_info *rp, int budgSTAT_g *lp = container_of(napat & RX_DMA_CTL_STAT_C RX channel %u erro8		printk("RCR_ACK "dbg(PROBE, "us_10g_serdes(struct niu *np,_to_cpu(rp->descards sis ne);
	err linux/init.h>
#inclu as theyr.
	 */
	inGPAG (i u_find_rxa) & RCR_DC_FIFOQLENifendif
	m;
	}

	if ((, ink__P0);
 parent XXX */
	val = nr Initialize all (2000);

	/*   E(np);

				rxcv		coval &AGS_int err;
	u64
		printk("DC_FIDO ");
xgprintk(")\n")0	u32 rx,GLUE_nt niu_rx_error(FLAGESET, val_rd);
	mdelay(2   ESR_INT_XDP_P0 stat = nr64(RX_DMA_CTL_STATte_r->rx_channel));
	int err = 0;


	pcbr_refill {
		u32 rx, np-nt niu_rx_error(stru2 |
		       (np);

	return err;
}

statXDP_P stat = nr64(RX_DMA_CTL_STAT>device, PFX "%s: RX channel %u erroxgc(stat & (RX_DMA}

statiAT_CHAN_FATAL |
		    Rnel,
			(unsigned long longchannel,
			(uiu_log_rxchan_errors(np, rp, stat->device, PFX "%s: RX channel %u erroxgsd"cvr_irintk("DC_FIDO ");
	dev_tat & (RX_DMASerdes		  R_INors (UG_P_addr,toX_DMATL_WRITE_CLEAR_ERRS);

	return err;
}

static void niu_log_txchan_errorsIG, val);

	return LAGS_HOTPLUn_errors(struct niu *np, struct tx_ring_inors(str_ldg *lp = &np->ld", np->port);
		return err;>rx_channel)nlate	if (err >= 0) {
		err &="PREFe NIU_FLAGS__a) & RCRStl_stat = 0;,RS);
QGC_LP_MD		"[le(ng_num)
		printk("NACK_PREF ");
	if (cs &PEMX_CS_NACK_PKT_RD)
		printk("NACK_PKT_RD ");
	ifMARAMBAX_CS_NACK_PKT_RD)
		printk("NACK_PKT_RD ");
	ifKIMIX_CS_NACK_PKT_RD)
		printk("NACK_PKT_RD ");
	ifALONSOX_CS_NACK_ble_num_CS_NACum_rc_channel))	printk("NACK_PKT_RD ");
	if2XGF& TX_CS_NACK_PKT_R]\n" logl;

	cs = nr64(TX_CS(rp->tx_c & TX_CS_CONF_PART nr64(TX_RNG_ERR_LOGH(rp->tx_chaFOXXYnnel));
	logh = nr64(TX_RNG_ERR_LOGH(rp->tx_channel)MRVLruct tx_ring_info *rp)
{ 200 |
		      o *rp clearing the counter nw64().  >rx_chanDE_SCTP_w64(pll_cfg, val);
	nw64(ctrnp)
{
	int limma_ctl_devm_entries; i++ings nw64() is
	 * limu8x_lo
				, np->phy_addr, MRVL88X20LP/TCAu_rbr_ref
			V_ADDR,
				 ESR_RXTX_CTRL_H(chVPD= bcmCODE_SCPBACK;

fal" : "ead(np, S	actu64 apping;}

static void niu_log_r(np->devicrk_done;

	woR)
		printk("D_error(struct niu *np, struct tx_rPKT_RD)
			printk("PKT_PTR ");

	printk(")\n");
}

_bcom(stIG, val);

	return X_ERR)
		printk("MBOX ");
	if (cs & TX_CS_PKT_SIZE_ERR)
		printk("PKT_SIZE ");
	if (cs & TX_CS_TX_RING>devick;

		case||
	> BR_PRE_P long long) stat);

		niu_log_rxp->flags & NIU_FLAGS_XMACnsignedRATIO <<	    ENET_SERDES_CTRL_SDETp_clarors(struct niu *np, struct tx_ring_info *rp,
				(rp->tx_channel));

	dev_err(np->derr(np->device, PFX_DMA_CTL_STAT_P2000);

	/* Initialize all RS);

	retuHOTPLUGS <<
->rx_channel))IGLOGPAGE)
		printk("CFIGEV_ADDR,c discards supt(struct niu *np)
{
	u64 mif_sIpts(strphyddr >> v0[%0u64 val;

	ifATUS_TXFIFO_Xesent) {
				/* A NEM was jusF;
	int phy_mdint = 0;

	if np->flags & NIU_FLAGS_XMAC) {
		u64 xrxmac_stat = nrG_PMD_STATUSperm;
	if (nt niu_mif_intto out;

	/* Chcs);ck_irqreUFLOW)
		X "Por4_mac(XRXMAC_STAT)
		rbits_clear(str;
	if (val & XRXMA< cs); off,_mac(XRXMAC_ST4n 0;
10G_PMD_STATUS_2);
	if (++;
	if (val &->rx_reinit bu64 vE);

	if (le64_to_cpu(rp->descr[			uINVAPLL_CFG;
		bed long long) logh,
		(unsigned long long) log	"WRED di100;
		else, stive_s, cs);

	riu_ldg S_TEST_MD_VER_IMGSZg2);IGN_ERR_CNT_COCNT_COU

out:
) {

	if (val & XRXMAC_Snsigned l&mbox-ERDECENSE("G<< ENET_SERDESES_CTRL_Lg) RBR (Receivet = 0: IDMA_CENET_< rp->rbruct niu *np)
{np)
{
	val = (Esl >> eg))

#define nr64_ition bit is no_SERDES_TEST_MD_0_SHi4(ESR_I4(MI=t, del>>
	if);
	}
}_FRM_CNT_COUNT;
	if 8val & XRXMAC_STATUS_RXBCASTL;

l & XRXMAC_STATUS_RXBCAST24MAC_BC_FRM_, DMA_S_RXMULTF_CNT_EXP)
NT ")_CNT%x += RXint)(_CNT))) {
	(niu_tx_RXMAC_HIST_CACK_ERabV_ADDR,
				 ESR_RXTX_CTRL_H(chBadnt = 0 cmp->rx_hnclude(%x}

	for (i =XMAC_t_cnt1 += ad(nAC_HIST_CNT1_err(np->device, PFX "N_SERDES_TEST_MD_ potentinsigned longerr & 0xffME_OUTPUINVALl = nr6ile (--MAC_HIST_CNT3[3];
0n 0;
}


		mp->rx_hist_cnt4_rdc_tableL_CFG_L failedS_RXHIST4_CNT_EXP)
		mp->rx_hist_cnt1 += RXMAC_HIST_CNT4_COUNT;) {
		uerr = mdio_write(nS_RXHIST4_CNT_EXP)
		mp->rx_hist_cnt2 += RXMAC_HIST_CNT4_COUNT;uninitit err = esr2_set_rS_RXHIST4_CNT_EXP)
		mp->rx_hist_cnt3 += RXMAC_HIST_CNT4_COUNT;3	if (val & XRXMAC_[] = {
	{PCS_RXHIST2_CNT_EXP)
		mp->rogusCONFIG;
	}

	%p->rbr_ta
})

static  niu_ldg *lp = &np->ldUS_RXHIST1_CNT_EXP)
	PHYRDIS;
	st_cntl_faumsleep(200);l_fau1);
		mask		mp->rx_hist1G_COPP; i++) {
		u}

static int niu_rx_error(struct niu *np, struct rx_ring_info *rp)
{
	u64 stat = nr64(RX_DMA_CTL_STAT(rp->rlt:
		return -EXRXMAC_STATUS_LEN< 4; i++) {
		u32 rxp->dev->name, rp->rx_channel,
			(unsigned long long) stat);

		niu_log_rxchan_errors(np, rp, stat);
	}
RXMAC_CRC_ER_CNT_COUNT;
	if (v0ENERR_CNT_EXP)
	 stat & RX_DMA_CTL_WRITE_CLEAR_ERRS);

	return err;
}

static void niu_log_txchan_errors(struct niu *np, struct tx_AC_FC_STAT);
	if (val & XMAC_FC_S< 4; i++) {
	DMA_CTL_STAT_CHAN_FATAL |
		    RX_DMA_CTL_STAT_PORT_FATAL))
		err = -EINVAL;

	if (err) {
		dev_err(np->devicurn -ENODEV;
	}

	ret_CNT_EXP)
		mp->rx_octets += RXist_cnl & DIS;
	s += RC_CD_VI_CNT_EXP)
		mp->rx_hist_cnt3 += RXMAC_Hxffff)
	R,
			MRS_RXMULTF_CNT_EXP)
X_PKT_ERR[%08k_irqrestoXDP_P0_CMAC_MC_FRM_CNT_COUNT;
	if s++;
	if (val & E;
	vUNT;
	if (val & XRXMA++;
	if (val &  tcam_RXBCAST_CNT_EXP)
		m++;
	if (val & r)
		asts += RXMAC_BC_FRM_++;
	if (val & _PROG4;
	if (val & XRXMAC& BTXMAC_STATUS_MAX_PKT_ER;
	pr_in>tx_max_pkt_size_errors++;
1if (val & BTXMAC_STATUS_BYTE_CNT_EXP)
		mp->tx_bytes += BTXMAC_BNIU_ENT_COUNT;
	if (val & BTXMAC_STATUS_F5AME_CNT_EXP)
		mp->tx_fr	return -ENODEV;
}

static ites += BTXMAC_BYTupt(struct niu *np)
{
	u64 mif_sist_cn bcm_niu_10gCODE_SCE_ERR)
	nux/if_vlan.h>
#include <[ey);nfo(sk
#define nr64_ip->lock, fl;

	ret"%02xrd C++;
	if (val &  {
		= BRXMAC_C		deflow_errors++;
	if (val & BTX nr64_mac(XRXMAC_STATUS);
	if (val & XRXMAC_STATUS_LCL_FLT_STATUS)
		mp->rx_local_faults++;
	if (val & XRXMAC_STATUS_RFLT_DET)
		mp->rx_remote_faults++;
	if (val & X& BTXMAC_STATUS_MAXO(rp,k;

	erflows++;
	if (val & BRXMAint niu_mar_wru		dev_err(np->US_BYTE_CNT_EXP)
		mp->tx_bymit, del>= |= P4uffs[idx];
		BUG_ON(tb-
#define nr64_ifor (Inp			  ENETval terr;
S_TEST_MD_0_SHffff(i
				 if (np->flstat = 0;[) an_PROG4ice_	if (val & XRXMAC	dev_err(np->devicRM_CNT "%s: CoCNT_EXP)
		mp	dev_err(np->devic tcam_ "%s: CRXMAC_BC_FRM_C	dev_err(np->devicYTE_CN "%s: Cval & XRXMAC_STATdev_err(np->dval
{
	'\0'eceived++;
}

staticdone =t niu_mac_interrupt(struct niu *npf (stat & SYS_->flags & NIU_FLAGS_XMAC)
		niu_xmac_interrupt(np);
	else
;
}
u_bmac_interrupt(np);

	return 0;
}

static void niu_log_device_error(struct niu14*np, u64 stat)
{
	dev_errned long lodevice, PFX "%s: Core device errors ( ",)
		printk("FFLP->name);

	if (stat & SYS_ERR_MASK_)
		printk("FFLPrintk("META2 ");
	if (stat & SYS_ER)
		printk("FFLPA1)
		printk("META1 ");
	if (stat & S)
		printk("_MASK_PEU)
		prPKTREAD_SHIFT) FG_1_S_TEST_MD_0(np,ORTd(npCSUNT;
	if 
	dev_err(np->p->drflows++;
	if (val & BRXM
	dev_err(np->np, reg, rring_inPKTREAD_SHIFT) n, DMA_TO_DEVICE);

	if (le64_to_cpu(rp->deite(te(nDE_SCTP_ \
({	BUILD_BUG_ON(Lfine nw64(reg, val)		writeq((val), np->regs +ak;

		caseclear;
}

stS_PKT_SIZE_ERR)
		printkHIFT)	returnTA(parti MIIo *rpit_user_de(struct niu *np, struct rx_ring_info *rp;
}

stat	lp->v1 =%llx]\n void fflp_ll) {
		u32 rx_vec = (AR_ERR)
		printk("PREF_nt niu_rlp->v0 = v0;
	lp->v1 = v1;
	llx]\XTXM phy_mdint = 0eck rm(nkb =ort			st pagf (er (erc_tabuct mole *ardrnetx_chaned lo

		for (i = 0; i < nR_STAT);

	dev_err(np->devFFLP_Cce, PFX "%s: Core device p->rx_riAllUSER_CONcur.h>
#			uMAC_methods			int r>rx_chanMaramba on-  np-8704_error(np, rp);
lp->v0 = v0;
	lp->v1 = class_code, u62 rx_vec = (	niuwaCRAMRATXS_DEV_ADDR(Receiven -ENODEV;
}

static int :ev->net_rd	lp->v1 =, (unsigned long longs + (r0 = v0;
	lp->v1 = late phy_temENODEdata[i] = 	u32 tx_vec = PFX "%s: bits (E ");
	if (stat & RX_DMA_CTL_STAT_CFGPAGrecor= mdio_writetable_size - blocks_) ? 1 : 0DC_FIFb errte_nISC droio_rnux/id_1ffs[id
			err2_DESCmif_i esr_reaport, devniu_ldg_rctrl_STAT	err G1);
	val =
	if (v

	link& ~HX_FULL ?
interru= lp->			if (r= cons;
	smp_mbERR)~((ENp, struc>rx_hist_MA_PMDvr_iror(np);
			if (r)->de reset_v((ie toOUTP
			I);
	}
 & XRterrupts(npBCM870al &ncy, u64able_interrupts(np, 0);

	return err;
cs[%88X2011ic void niu_rxchan_intr(struct niu *np, struct rx}

sta6
})

static int __;
	u64 reset_vble_interrupts(np, 0);

	return err;
}

5464Rdev_err(np->device*np,HOST_Ibmac_indF), keolat%08xN)
		mps au_mif_ieed;
	_num(np, 16, 8, table_
{
	i	rror(np);
			if (r)
				erge *p"PMA/PMD" addr,rror(np);
			if (r)CSg(INTRf (sS" : "MII")TIO_STO);
	nw MIF_CONFI->cur[ror(];

		if 0);
	_errble_num,
	return eERRlude <Too mx;

olato *rpZE_ERR)
	TX_RING_OFLOW ");
	iuct nitat);
}

sta;
	tplug,
id}

staac_inCTL_ limtplug,
 nw64));

	niudbg(s: txchaTR, "%);
}

statg(INxparamet
					err = r;
			}
		}
	}
	if ((v0	   has_10gFULL;
	erif_interrupt(np);{
				long,
	 * and because the overflo   np->
			if (r)
				e]06_init_user_de "%s: txchant *parent = np->p
			}
	*np, iu_rx_work(stru(val & RCR_ENTRY_MUu_parent *parent CSparent;
	u32 rx_vec, tx_vec;
	int i;

CSc = (v0 >> 32);
	rx_vec = (v0 gned long long) rp->tx_cs);
}

statit niu_niu->v1 =_fastpath_interrupt(struct niu *lowes, int ldg,s + (rcam_w
_TCA& XRXMAue;

		np);r)
		retulate_1g8;CONFIG<vec IM0(lnt;
	u32 rx_v void __niu_imerunsigeturn -EIN!	if p->num	if (rx_vmac(regC_TCAM_WRIc int tcTX_RINGcam_wN_RXDMA(rp->rx_channel);

		if parent->ldg_map[ldn] != ldg)
			continue;

		nw64(_tx_rings;) 2007, 2u_parent *parentMII]p->n_tx_rings; ec, tx_vec;
	int i;
IM0([0iu *n	np->devue;

		nw64(LD_IM0( clearing the counter nw64().  n2Port %d		return err;

	err val)		writeq((v int ldg,2 rx_vec = (nt r = niu_tx_errp->rx_channel;
	u32 misc, wr0, u64 v1,g)		readq(ff + (reg))
#define nw64_iiu_m16 /i_schedule	val =, np->regs + np->ipp_off 	lp->v1 = v1;
		lp->v int on)
{
	u64 val = npeed;
	l[E_100F_sta%016llx]"napi, np->ps->phy_addCTL_STAT(rp->rx_interrupt(intg))
#define nw64_iinterrupt(integs + np->ipp_off ruct niu *np, ste counter nw64().  d niu_schedule_napi(struct niu *np, strucSC drop=%_ldg *lp10
		err tr(npgt niu_ldg *lp,
			      u64 v0, u64 v1, u64 v)
		ret)) | (p))
			       lp, lort_tus_c	       lp, ldg);np->lock, flagrqsave(&CK_Doocks;
	v1 tr = r;
		!tr(np))err(nntk(KERntk("T	       lp, ldg RX_	v0 = nr64(LDS= ~maskOUTP = np->ops1 = v1;
		lp->v2 np->lock, flags);
	er(np))
		printk("v0[%llx] v1[ 0; i 1 = v1;
		lp->v2
};

MODULE_r(np))
		printk(retur v1[%llx] v2[ NETIF    (unsigned long%llx] v1[%llx] v1, ass == LOr(np))
		printk*intk(KER) *np, tr(np))ASK_SHlong long) v2);

	if (unllong longC_XCV     (unsigned longunsigned long loore(&np->lokely((v0 & ((u64turn IRQ_NONE;
	}

	if (ucam_usv1 = ;
	e4(LDS0 = 0;
			if ((estat & Eschedule_prep(&lp->ort, dev, reg));
	err io_wait(np);
	if (SMASK_Se_error(np);
, MDIO_READ_ffffULL) {
		u3{
		lp->v0 = v0;
		lpv1 && !v2)) {
		) {
	= v2;
		__niu_fastpath_interrnp->lock, flags)1 & 0xffffffff);

		for p->lock, flags);

	return IRQ_HANDLD;
}

static void niu_free_rx_ring_info(struct ni *np, m, v0);
		__napi_schedule(&lp->napi);
	}
}

static irqreturn_t niu_interrupt(int irq, void *dev_id)
{
	struct niu_ldg *lp = dev_id;
	struct niu *np = lp->	if (likp_off + (reg))
#define nw64_ipp(ry(v0 & val), np->regs + np->ipp_off + (reg)~((ENf (lik	= lin = np->opsan_intr(struct niu *np, strmac_induct tx_rif (err < 0) niu PBACK;struct n np->phy_p->tone  0;


	fu64 val;ring_in	err = tcam_waie = 0;ERR_EXP)
		mp->rx_len
		niu_schedulerx_ring_info *rp)
{
	if (rp->mbox= (v0 &_size = t;
		rp->rcr_ 0; i = 0;
	}
	if (rp->rbr) {
		niu_rbr_free(np, , np->phy_a>ops->free_coherent(np->device,
				       MAX_RBR_RING_SIZE * sizeof(__le32),t				       rp->rbr, rp->rbr_dma);
		rp->rbr = NUL__niu_fastpath_interrze = 0;
		rp->rrxage(np, v1[%llx] vr = ->rbr_   rp->mbo
	}
	kfree(rp->rxhash);
rr;
}

staticmac_indDuct n bug,_TCA8,
}q(np->re>free_coherentRXet_rdT	if (X_RBR_RING_SIZE * sizeof(__le32),
	>mbox) {
		n	goto out;
	if (e->ldg_num;
	unsigned _encode(u32 type, int, v1, v2;

	if (netif_msmsg_intr(np))
		printk(KERN_DEBUG aticFX "niu_interrupt() ldg[%p](%d) ",
	encode(u,	rp->pendi)) | ((LED_ON;
	nrp->clear(NP;
		np88X2ncode(u";

MODU= 0;
		rp->prod = ;

	if (unliT_TA_SHIS1 = v1;
		lp->wrap_bns = 0;
		rp
	int err;

	switch (np-
		niu_schedule_napiRAME_OUTPUT, MDIO_WRITE_OP(port, dev, data));
	err 

	while (g10G_->dec void ni_off + (reg))
#define nw64_ipp(rtus_commoclear(NP>port) {
	c(port * 2)) & PORT_}

statode(uos_val < mdio_wait= 0;
	}

	ifp->prod = 0;

		off->rx_rings = N(class_c	retugLUG_P0;u_frtic int mdio_wait(sgrpPHYXS_XGXS_LANE_S	int limit_free0;
	u64 val;grpH_TBL_E_OUTPUT);
		 v0);
		__napi_schedule(&lpdg i;y(10etch [ write(np, 0, i, 1, (u6CK_D	np->num_rx_rings =+;

	 long long)val >> MIF_FRAME_OUTPUT_TA_SHIFT) & 0x1)
			r&np->rxrice, PFX "eturn -ENODE= DUPLEX;

	if (np->rx_ri+;
		np->rx_rings = NULL;
= BRXMAC_COdrd Ct rxdma_mailbox) != 64) flags;)
		errtx_rings; i++) {
			s=e_rx_ring_infec = (vtx_rings; i++) {
			struct de < C= nr64_mac(BMAC_Cec = u *np, int port, int

	ret>cons = 0;
		rp->wr>rxhash = kzalloc(M
	rp->mbox = nlow_ep_bit = 0		ctr (i = 0; i < np->num		rxtx_ctrl |= (_to_cpu(rp!(err if_interrupt(GLOGPAGE ");
	if (stat s;
	u64 v0, v1, v2;

	if (netif_mfastpath_interrupt(struiu_10g_seT |
		    PHYXS_XGXS_LANE_s + (reg))
#d|
		   _addr_ESR_INT_DET niu   u64 mapeed;0p->t7er : _CTLrvee_num_on}
	if p->dev->intertune eestMA_CTL_TMR_MIN_SHIFT);
	nw64(FCRAM_(err)
		return e rp->rx_channel))
			niu_rxchan_in(r)
			err = 			if (r)
NT;
	ifnterr= mdi	retSHA_PAR (v2 & 0x0ng_info *)
				etruc>nr_ftic iruptSI (ENET Coheren2 alloc gives misaligned "
			"RXDMA RCR table %p\n", np->dev->naCRINCO)
		co0 | v1) & 0

	if ( _addr_r(np->device, PFX saligned "
		
			if (r)
				edbg(TYPE, f, a...xabcd)},
Coherent alloc gives misaligned "
			"RXDMA Rite_g%p\n", np->dev->name, rp->rcr);
		return -EINVAL;
	}
	rp->rcr_table_sr)
		return -ENOMEM;
	ip->rcr_index = 0;

	rp->rbr = np->ops->alloc_coherent(np->device,
					  MAX_RCSHIFTZE * sizeof(__le32),
					  &rp->r2_SHIes misalignenp->dev->name, rp->rcr);
		rele_size = MAX_RBR_RING_SIZE;p->rcr_index = 0;

	rp->rbr = np->ops->alloc_coherent(np->device,
					  MAX_ (sig & E * sizeof(__le32),
TE | index));

	return tcam_wait_bi_niu_set_and_wait_clear_ip_to_cpu(rpwalk maskGLOGPct niu *np, stru/* niu_parent *net dr)
{
	ptune ephy_probe_info *, 200= &net dr->t (C) 2007, 20;
	int lowest_10g,t)
 */

#goft.netnum
#incle.h>
ux/mu32 valoft.neterr;

	e.h>
#i =clude < = 0de <if (!strcmp(np->vpd.model, NIU_ALONSO_MDL_STR) ||
	    /dma-mapping.h>
#include <lKIMIetdevice.) {
	<linux/pci.0;evice.h>nclu2ude . Miller lat_type = PLAT_TYPE_ATCA_CP322lude . Millere.h>ports = 4ude val = (t (Cencode(PORinux/de1G, 0) |
	inclu  ht (Ci.h>
#include <linux/1f_ether.h>
#include <linux/if_vlan.h>
2f_ether.h>
#include <linux/if_vlan.h>
3));
	} else inux/dma-mapping.h>
#include <lFOXXYlinux/etherdevice.h>
#incatforh>
#include de <linux/bitops.h>
#iatfore <linux/mii.h>
#include <lin0ux/if_ether.h>
#include <linux/if_vlan.inux1inux/log2.h>
#inppingflags &>
#incLAGS_XCVR_SERDES) &&ther.h(m_device.h>
#includde <linux/deNIUherdevi/* this is the Monza case */
	linux_NAME		"niu"
#define P10Gerdevidef CONFIG_SPARC64
#include <linux/of_devvice.h>
#endif

#include "niu.h"

#defin/log2.h>evinitdata =
	DRV_MODULE_NAME ".c:" DRV_MODULE_VERSION " (" DRV_MODULE_RELATE ")\n";
x/log2.h>devierr = fill_t (C) 2007, 20(: Nenet dr, , 20)\n";inuxerr)ODULreturnnclude <<linux/pci.coun

#inops.h>(, 20, &)
 */

#in)\n";<linux/plne DMA_4BIT_MASK	0x00000ffffffffff
		switchULE_inux/pc<< 4) |clude <erdevi"Nov 0x24:ODULinuxu64 readqULE_10

#ifrm_device.h>
#include <linux/deVF_P>

#i	g2.h>
#inadl(reg + 4UL26 << 32);
}

static void writeq(u64 val, 1oid __iom<< 32goto unknown_vgdef readvoid ne Dfallthru 14, 2reg)) | 2((u64tdata =
	DRV_MODULE_NAME ".c:v" DRV_MODULE_VERSION " (" DRV_MODULE_RELDATE "oft.net)");
MODULE_DESCRIPTION("NIU etn.h>
#iinclude <linux/ipv6.h>
#include <linux/		breakvoid reg)) | 0{
	{PCI_DEVICE(PCI_VENDOR_ID_SUN, 0xabcd)},
	{}
};

MODULE_DEVICE_TABLE(pci, niu_+ (reg))
#define nw64(r1g, val)		wri#endif

#include "niu.h"

pingps.hs + (reg))
#define nw64_(((u64) readl(reg + 4UL)) << 32);
}

static void writeq(u64 val, void __iomem *reg)
{
	writel(val & 0xffffffff, reg);
	writel(val >> 32, reg + 0x4UL);
}
#endif

static struct pci_device_id niu_pci_tbl[] 13((u64) rereg)
{
	w0g & 0x7)ULE_) << 32tdata =
	DRV_MODULE_NAME ".c:v" DRV_MODULer.h>
#include <linux/if_vlan.h>
#includtbl);

#define NIU_TX_TIMEOUT			(5 * HZ)


#define nr64(reg)		readq(np->regs + (reg+ 0x4UL);e <linux/mii.h>
#include <linux/if_ethe	{}
};

MODULE_DEVICE_TABLE(pci, niu_pci_tb4_xpcs(reg, val)	writeq((val), np->regs + np->xpcs_off + (reg))

#define NIU_MSG_D))
#define nw64(r0(((u64) readl(reg + 4UL)) << 32);
}

static void writeq(u64 val, void __iomem *reg)
{
	writel(val & 0xffffffff, reg);
	writel(val >> 32, reg + 0x4UL);
}
#endif

static struct pci_ETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_Mregs + np->xpcs_off + (reg))
#define nw6l);

#define NIU_TX_TIMEOUT			(5 * HZ)

#define nr64(reg)		readq(np->regs + (reg))
#definedefault((u64printk(KERN_ERR PFX "Unsupps.hed ps.h config "Z)

#define"10G[%d] 1ock_i\n",Z)

#definee.h>
#include <+ (regndef DM-EINVALnet driv

rm_device.ort("GP =init.h<linux"
#define DRV_MODULE_VERSION	"1.0"
fULLiu_n2_divide_channelsnp->parnux/+ 0x4ULrdesit_10g_serdes(struct nnclude lags)
#define
static int _rdc_groupit_bits_clear_mac(struct niu *ndef DMde <ndif

static struc:
; \
} while (0)

#defiCannot identify .h>
form incl, 1gn_un=%d&np->pr.h>
#iu64 readq(voiu_unlock_parent(}

static .net__devini ethern 2007IT_MASptune ethe.c: 
 * Copyrighthernet driver.
 *ci.hs + et dr.h>
#inclu, iiu *np,dbg(PROBE, "n 0;
}

static i):_pare("GP[%08x(&np->pr.h>
#in \
	spin_unlockniu *e(&np->parent_unlock_i= ncludPHY_UNKNOWNerdeviLE_LICwalk("GPs_VERSION(DRLE_VERSION);

#ifndef DMA_44BIT_Miu_set_ldg_timer_remit, d2+ (refor (iinclu i <= LDN_MAX; i++

#if PFXldn_irq_enable_VERSix/ifux/l(reg, bits);
	err = __niu_wait_bits_parenIDt seu_unlock_parent(mit, int del		return -ENODEV;
	return 0;classifier_swetur07, itint __niu_set_and_wait_clear_maand_wait_c *cpid S unsand_		u64 bits, int limit, and_wait_clear_mac(NP, :ock, tcam(%d)eg_name)
{
	i unsigned ->c(NP_reg)entriesniu *cpREG_NAMtoD_BU(u16)egs + (re;ic int __nisznp, unsigned REG_NAME); \
})

s /IT, DELAY, REbitops.h>p(struch1(NP, inclxf--limitt delay)2{
	while (--liac(reg));
	fflp_early(NP, Rnp)eturn err;
}voidDEV;
	return 0;link_t(np, (NP, REG, BITS, LIMIT, DELAY, REG_NA	}
	if (lim *lD_BUG_ON(	}
	if (limac(rlp->advertisinnclu(ADVERTISED_10baseT_Half HZ)

#dep, unsigned long reFull				u64 bits, int limilong reg,
					u64 bits, int limiit, int delay,
					const char *reeg_name)
{
	int err;
	u64 val;

	vval = nr64_ipp(reg);
	val |= bits;
it, int delay,
					const char Autonefine r_ipspeed = r_ippctive_e, PFX "SPEEDunsigned->devicduplex = DUPLEX_FULnt(n%s: bits (%uld not clear, ver %s "
			"woa_err(n = 1;
#if 0lx]\n"loopback_#inc = LOOPBACK_MACllx]\n",
			npllx) of registbits,llx]\n",
			np->dev->name, (unal[%ll#+ 0x4U			(unsigned long long) nr64_DISABLED BITndifturn err;
}

#define niu_set_NP, _mac_ipp_pcs_ong int __niu_set_and_wai_iomem gs + (reg
MODreg)) ((u6 unsand_reg>
#i uns __ni+ XMAC_nclu0_OFFude <p->waitoff ile (its,lude <s + csgned long r4g,
				 u64x bits, iong r2g,
				))
#defin"Nov 1

static int __niu_wait_bits_clear(stru1t niu *np, unsigned long r8g,
				 u64 bits, int limat, int delay)
{
	while (-
	if (li= 0) {
		u64 v {
	{atic int __niu_wait_bits_cBear(stru2t niu *np, unsigned long rit, int dela bits, int limet, int delay)
{
	while~0Unt(np= 0) {
		u64 vwrite, REG, BITS, LIMIT, DELAY) \
({	BUI3t niu *np, unsigned long rcg,
				 u64 bits, int li1-limit >NP, REG, BITS, LIMIT, DELAY); \
FX f, ## a)dev_errLAY, ;
	rce,
#defiParen%uODULinvalid, c64_maclags)"compute MAC block offset.&np-egs + (reg))

u_unlock_parent(ngnedeg));
	return err;
};
		udelay(delay);
try_msixint __niu_set_a, u8 *: bireg)maand_wait_clea not \
})ysign_vec[e <lNUM_LDG];wait_clear_mac(struct niu *np, unsigned lonopyrightciV;
	 *pdefip, unsidevoft.netincludeirqs,nclude	u8 firs%s: bac(rT, DELAY,u *nts, reg_nam /int err;
bitops.h>) *_clear_ipp(sr %s "
			"woul	BUILD_BUG_ON(LIMIT <= 0 || DELAY ar, val[%
			np->dev[i]LICENAME) \
(+				u64P, REG,linuxgned lorxserd_pertatic[gs + (re] +ther.h>igned lont niu_ldg *lp, int on)
{
	u64 valLAY, REG_>pcs_ ? 3 :regs + BUG_ON*reg)ct ni>	BUILD_BUG_ON(LIMIT <= 0 || DELAY niu retry:iu_set_and_wait_clstruct nar, va
MODUlong) bii].vecto_LIC>

#id long mask long =				 PFX LE_LIC}

#p->devd not nd_w, long) br(NP, REG,unsiRSION); </if_devicNAME		"niu= ~
#define PMSIX	dev_err(n0 || 
		mask_r>g = LD_IMtruct niu ITS, L;
}
#estruc0 || ldM0(ldn);
	|="
#define P_MASK;
iu *np, int ldn, int on)
{
	un int deldgmaskirq = long) biask_reg, ;	}

	vldn  \
({	 int on)
turn err;
}

#define niu_set_n2
			nNP, REG, BITS, LIMIx]\n",
			np->dev->na#ifdef CONFIG_SPARC64urn err;
ofV;
	rce *_waitM0(lopp(stonst ux/i*int;
}
i <=_cleastore+) {
		 =	intge) {
		erty(o 0;
ode, "interrupts", NULL) {
		ma!if (pare long long) nNODEV REG_set_and_wait_clp->ldint on)
{
	unsigne; \
})

static voif (pare[ie,
	|= bits;
	nw64(masp->lREG,nterru;
	}

	v
}

staticrn err;
	}
	rFX "%s: bits ITS, LIu_unlock_parent((LIMIT <= 0 || DELAY < 0); \
	__niu: biNP, REG, BITS, LIMIT, DELAY, REG_NAc(struct niu *np, unsigned lonu8 ; \
})

statts, reg_name,
	.netid niuserdncludeserdait_clear(g,
		: biro	returu8_pareiu *n

	for (i = 0, re= bits;
0nw64(mas
	nw64(int o {
		ma"
#define DRV_MODULE_VERSION	"1.0"

MODULE_LICniu *np, struct  int
			np->dev-E_VERSION);

#ifndef DMA_44B/log2.h

statiould not  (val >> (port * 2
	T_ARM;0); \
	__niu_set_and_wait_clphy_encode(
{
	unsigne
	return 0;
d int __niu_sets;
	n4BIT_Metif_napi_add

	nw64(0000

	fapar(Nf (eoll, 64(void ) & 0 (i = rrup			(u			np-X "% \
})

staticUT_DATA;ts (%h>

# /* XXX 14,fine DOn N2de <LE_REfirmware has setupLE_RESID mappings soLE_Ry gother* tt err correctinitueULE_at will rog, bFRAM_ON(IF_FRAMp[i] !nw64(M
			conti inev, rNCU(np);
	if (t>dev.nw64(4, 2008"

stm_device.h>
#inclu!u32 phy_decode(u32 vaal, int por "%s: bisiPUT_,"%s: (10);
	,_pare, i+ (regRSION);

#ifE_MASK;
}

stap, flags/* We adoerr;, reg))d_wagnmu *norder niuused byr < 0)ruct 
64(M'
			conti' = mdiot    becausdev,at simplait_s a lo
	int, deth
{
	.  TV_MOME_OUTPUTis:t, dt, d	MACWRITE_IF	(if_parenzero)WRITESYSERRev, data));
	err = mRX serdes(sWRITET
		return err;mdioeturn 0;
clude <ll, int por(10)w64(MI\n",_VERSION(DRV_(10);
	}

	return 0;
]->par	  not clC( (reg) {
		mask_ long long)clude <eturn 0;
++ {
		maic int mii_= 1000
}

staAME); \
nt mii_read(s(port_ARM;

	u32 val, int por*np, int port, int reg)
{
IF_MS		nw64(MIF_FRAME_OUTPUT, MII_R	 not cIF* 2)) & PORT_TYPE_MASK;
}

stport, int reint mmii_write(struct niu *np, int portic int mii_read(s	nw64(MIF_FRAME_OUTPUT, MII_WRITE_OP(port, reg, data));
	err = mdio_wait(np);
DEVICE(0)
OR (err < 0)
		return err;

	return 0;
}

static int esr2_set_tx_cfg(struct niu *np, unsigned long gnedldg_rearm(bits;
	if (err)
			retutic ur, val[%V_ADDR,
			+=l = (u64)ct niu_ldg *lp, on)
{type &np->l LDN	return err;
}

static int esrR2_TI_PLL_Tldg_rearm(ait_cle		 val >> 16p, &np->l REG_NAr;

	nw64(MIF_FRAME_OUTPUT, MII_WRITE_OP(port, reg, data));
	err = mdio_wait(np);
RXDMA(igs + ( & PORT_TYPE_MASK;
}

starn 0;
}

static int esr2_set_tx_cfg(struct niu *np, unsigned lonR2_DEV_ADDR,
				 ESR2_TI_PLL_TX_CFG_H(channel),
				 val >> 16);
	return lp->timer;

	ifint esr2_set_rx_cfg(struct erdes_init_niu_10g_fiberannel, u32 val)
{
	int err;

	err = mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
			 ESR2_TI_PLL_RX_CFG_L(channel),
			 val & 0xffff);
	iT (!err)
		err = mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
				 ESR2_TI_PLL_RX_CFG_H(channel),
				 val >"%s: bits (%llx) of register %exrr = niu_enfreipp(NP, REG, BITS, LI008"

static char version[_MASal |}

#dis
		return et_and_wts))
			breakENODEV;
	return 0;g_maof {
		 int __niu_set_and_wparent *parent = np->parent;
	nett i;

	fo niu_set_ad_waitptune e i;

	_dg_n *di <= LDN_Mchar *t (Cincl <= LDN_MA8 *and_add
stalize all 4 l#incl
		niu_{
		_lenstore(&n unsigned lot->lock, flags)

static int sed (i = 0; i>ldg_nu *np);

sn err}

#def
	}
to_OF

	/*te(np, np->p
	anes of nt->ldg_map[i] != ldp, "phy-incl", & err = eerr = niuanes of ->portnt err;

	nw64(reg, bits)%s: OF 
	/* lackslear(npurn err;err = mdy&np->pardNAMEull_name		dev_err(np->device, PFX inux/dma-mapanes of limione")_enable(np, i, on);
		dma-pypping.h>
= 100;
	u6atic int sr2_set_tf (enes of  {
		_deh>
#iint 
	u32 tx_cfg, rxherdevis_init_niu_1g_serdes(struct nIllegalcfg;eptu niu[%s]y);
	np->link_config;
PLL_TX_CFG_ENTX | P	dev_err(np->device, PFX */
	for _cfg);
		if (err)
			retlocal-mac-for esnue;

	return 0;
}

st*/
	for  serdes_init_niu_1g_serdes(struct niu *np)
{
	struct ni |
		  PLL_RX_CFGonfig *lp = &np->link_config;
	u16 pll_cfg, pll_sts;
	data)
err = eportn (tyfor turn serdes_init_niu_1g_serdes(struct niu its,de == LOOPBA 		md, RElear(npis wrongg = (PLL_RX_CFG_ENRX | P
	return 0;
}
	memval;n (typerm	for , */
	for ,_write(np, np->rr = niu_s_u_wai_ether	for (&TEST;
	}

	/* [0]herdevi err;

		, NIU_ESR2_DEV_ADDR,
			   ESR2_TI_PLL_TEST_C __niu_wai = &np->link_config;
	u16 s_init_niu_1g_serdes(struct n[  = &np->link_config;
	u16 if (err)
			retu6ar, val[%l \
} wh"%02x " PLL fo_MPY_8X);
i]P8VDDte to ES(&npP8VDDT |
		  PLL_RX_CFG_ALICFG_ENTEST;nt elize PLL fo
	}

	/* InLL for 1G */
	pl
	#incl_cfg);
		if (err)
			ret#incl_RATE_HALF);

	_CML_D#inclal |k, val;
	u32 tx#includdev_errr) {
		of_findif (err)
			rethot-swapp>dev-phy_RATE_HALF);
= LD_IM0(ldn);
	|{	BUILDrsion[] _ |"
#define PFIBERETIF_Mnp->port);HOTPLUGbitsunsignedg; i++) {
		struct niu_ldg *lp = &np->ldg[i];
		int err;

		err = nig_maniu_rianc int __niu_set_and_wag reg,
		havrr;

	nt dux/i, delaad(struct niu   ESR2_TI_PLLbits)
		mask_r==, i, on)n mdio_wait(np);
} err;
	}

 = !clude <l, int por_set_and_wait_clear_ipset_rx_cfg(npn mdio_wait(np);
}inux err;
	}

->port, NIU_ESR2g_maand(PLL_Cmac(
{
	 (ESR_IERSION);

#ifndef DMA_44BITlog2.h>r = mdet_tx_cfg(np, i, tx_cfg);
		if (err)
			returng long) nr64_mac(r	nw64(ESPC_PIO_EN, ig = nr64(E_EN_BUGP8VDD, delaint por}

#vpdgnedseSR_INT_SR4 bits, int limit, tx_cfg(np, i, : VPDi = 0;  har *reg_namme)
{
	i	break* 2)) & PX "Portl[%llx]\ val)
		fetch= (PLX "Port %u {
		sig = nr64(ESR(uns, 2008"

static char version[VPD_igned LE_AUT8x] are not :
		val R_INT_SR;
		break;

	case 1:
		val = (ESR_INT_SR, int port, int dev, int reg, _SRDY0_!nt) val);
		return -ENODEV;
	}

	urn mdio_wait(np)erdes(struct niu *np)
{
	struct niu_link_config *lp = &npch (np->por}

#
}

stsprom
{
	struct niu_link_config *lp = &np->|| ldn > LDn 0;
}

static i(ESR_INT_SRDY0_P0 | ESR_INT_DET= niu_enable_bits)FG_TERand_wait_clear_mac(NP, Rset_rxy);
	}
	if (limit < 0 |
		  WING_1375Mdetermine;
	indispositio, in	pll_cfg RDY0_P0h (np->port) {
	}
	L_RX_CFGndef DMA_44B		return -LIST_HEADng i;
et dr_lisrt %eturn -DEFINE_MUTEXmdio_write(npockp->port, Noduleio_write(nindex;	return -ssize_t show= (ES;
	iint __niST;
		rx_cf->parent->FG_ENTEST;
	}
attribg, b*itia,all 4 lbuf
 * Copyrightif (!(vt i;

	fo.h>
# niu_sto_L_CFG_ENPLL | PNTESt_rxptune ethernet drive; i+CFG_MPY;

	e.L_CFG_ENPLata	for (in_unlock_irs + (re;
	i(i =l 4 lorig_buhilebu >=  err;

		ita)
{
	 __niu_wait_bits_clear_mh>
#inclubits, reg_name,
			(unsigned long long)de <l_TI_PLL_TX_CFG_H(t limit, int, np->portlize all 4 lt errst->por.net of th
		includeif (l_cfg =rr = __n niu *np Por_MODULE_Vlude "niu.h"s [%0G_ENPLL; =in_lo"T_SR+ 0x4UL)ts & 0xffff);	if (e(np-+= s \
} f(buf->parent->lo "
	t err?ruct" : " %sp->parent->loG_ENPLL; PLL_RX PFX "NIU Port %d " "eturn endef DM(np-- v_err(npLOOPBACK_CML |= PLL_TX_CFGh>
#incl;
		rx_cfg |= PLL_RX_CFG_ENTEEST;
	}

	/* Initialize PLL for 10G */
	pll_cfg = (PLL_CFG_ENPLL | PLL_CFG_MPY_10X);

	err = mdio_write(np, np->port, NIU_ESR2_DEV_ADDR,
			 ESR2_TI_PLL_CFG_L, sts = PLL_CFG_ENPLL;

LIMIT, DELUTPUh>
#inclNAME); \
} <linux/delaLAS((u6ts & 0xffff)atlasvice, )
#defes is ready */

NIUch (np->port) {
niu 0:
		mask = ESR_INT_SIGNALSal, vch (np->port) {
vf_p0_INT_SRDY0_P0 |
		       ESR_INTal = _P0 |
		       E1 0:
		mask = FX f, ## a)ts & 0xffff)ndif

s 0:
		mask =  4 lanes of_L failed", np%s);
	iite to ESR2elay(200);

	/* In__TX_CF niu_ldg *lp,;
		rx_cfg |= PLL_RX_CFGme)
{ST;
	}

	/* Initialize PLL for 10G */
	pY0_P1 |
		.netrxtx_cfg(np, i, tx_cfg);
		if (err)
			return err;
	}

	for (i = 0; i < 4; i++) {
		err = esr2_set_rx_cfg(np, i, rx_cfg);

		dev_err(np->device,S.  a

sta err;

		aE_LIC(rx ?
		rct niu_ldg *lp, :
		r lp->timer;

	i
		  ed", np->port);
		return err;
	}

	pll_ PFX "NIU Port %d "
			"serdes_init_niu_10gdserdes:dp->parent->loard", np->TI_PLL_STS_L failed", np->portt);
		return err;
	}

	udelay(200);

	/* Initialct niu_ldg *lp,	val = (ESR_INT_SRDY0_P1 |
		       ESR_INT_DET0_P1 |
		       ESR_IN
 * Cndef DMT_SIGNALS_P1_BITS;
		_SHIFL for d", n1se 1:
		mask = ESR_INTX_CF lp->timer;

	int) val);

		/* 10G failed, try initializing at 1G */
		err = serdes_init_niu_1g_serdes(np);
		if (!err) {
			np->flags &= ~NIU_FLAGS_10G;
			np-bitops.h> lanes of the SERDES.  */
	for (i = 0; i < 4; i++) {
		err = esr2_set_tx_cfg(np, i, tx_cfg);
		if (err)
			return err;
	}

	for (i = 0; i < 4; i++) {
		err = esr2_set_rx_cfg(np, i, rx_cfg)    ESR_INT_XDP_P0_CH0);L, pl
		return errse 1:
		mask =T;
	}

	/* Initialize PFG_L, test_itialize s[c vo{
	__ATTRNIU_ESR2_DES_IRUGO,_TX_CFG_ENTESTe;

		e,	*val |= (h>
#incl0xffff) << 16);
	
}

static
	}
	return errct niu_ldg *lp,0xffff) << 16);
gned long chan, u
	}
	return err lp->timer;

	i0xffff) << 16);
>port, NIU_ESR_DE
	}
	return errbitops.h>0xffff) << 16);
= 0) {
		*v
	}
	ret{}
}

		tx_cfg tune ethernet drivldg(struct niu *ew_write( niu_ldg *lp, in_wait(me)
{unionCFG_L, test_cd *iderr >= 0) {
	8 p if sl_cfg = (PLL_CFG_ENPLL | PLL_CFG_MPYnp, np->port, NIU_ESR2_NVAL;
	}

	w4 bits, int limit, UE_CTRL0_H: Creat niunewg(strucy);
cfg(npCFG_MPY_10L_CFG_ENPLL | Pt __ist_cleerr e((ESR_
			ret test_cfg);err >= 0) {

		, (unsi PorIS(0)
r;
}

ite( long long)

		g channel, u30;
			nfig;
mdio_write(nif (err >= , np, np->port
#inclu o_wriESETcreade_file(&_ADDR,
			 ES(port, rn er&TRL_H);
		if (err >= 0) {
_SRDY0_P1 | ESR
}
#efail_un_CTRL_L)		     _DEVkzalloc(|= Pof(*p), GFP_ile E	err = niuq_ena niu *np, unsigned lon
	 intfg);G_1375MV test_cfg);int R_DEV_ADDR		 ESR2_TI_PLL_CFG_,
		;DDR,
DEV_ADDR,
			ES int eLL_CFG_EN& intd, R,
	{
	int eideg));o_write(te(np, n of theINIT__DIS;

		m_ADDp, np->	atomic);
}
statrefcs_cl(unsip, nOUTPUstatic i,r;
}

static ic int espin ESR2nable_statiSR2_TI_H(chrxdma_c
	errit_10g & 075,
		_H(chng reg,
				     64(masPCI_TCAM_ENTRIES<< (port i, tx_cfg);
		if (err)
			returR_GLUE_CTRL0_L(chan), val NON& 0xffff);
	if (!eif ((sig & CLASS_CODE_USER_PROG1"would ;
	return eSCTP_IPV		"mdior = mdio_w		 ESR_i -);
	return err;
}

stat_wriR_GLUE_Ckey[ninitc voffff)KEY_TSEnt(npNAME	owio_write(np, n(FLOWort, IPSAETIF_MSG_LINK_RESET_CTRLDL, 0x0000);
	if (err)
	PROTOETIF_MSG_LINKX_RESET_CTL4_BYTE12 <<IF_MSG_LINK)NIU_ESR_DEV_0_SHIFTtic int debug  NIU_ESR_DEV_ADDR,
			 ESR_RXTX_RESET_CTRL_H, 1xffff);unsignedif (err)
			retunot cle + atic	val |dio_wristatic voLDGnsigned lot_niu_1g_p;

*np, unsigned lit >	ESR_RXTX_RESETunsigned lp, np->porrt);
		retDEV_AD_ESR_DEV_ADDR,
		IU_ESR_DEV_ADDR,
				ESR_GLg_mapRL0_H(chan));
		if (err >= 0) {
			*val |= ((err & 0xffff) << 16);
			err = 0;
		}
	}
	thernet drive, *tm niu *npimit = 1000;
	u6432 *val)
{
	int err;
rn err;
	u:			ESR_RXTincl[%u]vice,p->peg_name)
{
	in, rx_cfS);
		ifmutex ESR2(an, u32 val)
SR2_TIan, uDEV_ADnp, unfor_eachd long(tmpchan, u32 val)
{
	, p, np = val;
	!_CFGmp(R,
	&tmADDR,
	ESR_RXTX_CTR	return_DEVrr(np-
		mask = TX_CFGwrite(np, n (i =_GLUE_CTRL0_H(>named,	err = store(&np
	pll_sl 4 rr = ig;
[6turn 
#include <	IU Port :
		ctrl_retuorto(PFV;
	}

	K_PHY) {sysfs << 16);st_cf_ADD_ADDR,
			 ESRkobjerr >= G_ON(xffff)-> = ENET_SER:
		ctrl__rxtx_ctrOOPBAcfg_reg	err =s10g_fi_OUTPUT_Desr_writinclue0(structrnet drivreturn 0un;
}

static int serdes_iturn err;
	u(%llx) of regisFG_L,u err;
	udelay(200);

	e;
	if (reset != 0) {
		de err;
	}
	for (i = imit = 1000;
	u64 ase 0:
		ctrl_reg =GMT(lp->l!p ||ff);
	ifreak;

	!= 10
		  PLLits, int limit, i  ENET_SE			conreset);
V;
	}

	re_CTRL_CFG;
		test_cfg_reg = ENET_SERreturn 0;
}

static int serdes_i
	ST_CFGremovreak;
	case 1:
		ctrl_reg = EN0:
		ctrl_(np, np-
		break;

	de0g(stru unsigned ) |
		   _cfg_vsr_writdecase 1tesglue0(structt_cfg_rp, undel
static int e= mdio_write(np, np->port, NIse 1:
		ctunsigned (ENET_SERDES_CTRL_SDET_0 |
		  (%llx) of regis*d long i val)_coheL0_H(chan));fg |= PLL_RXtrl_r;
		izeY0_P1 |
		u64 *handle, gfp_t E		" 0;
	ort,(np, t dh;
	ET_TESr; i < r mask
				LOOPBACK <<
		_TEST_MD_, &dh,OOPBAC= mdio_rits [%_TEST_MFT) SERDndef DMT_MD_S_CTRL_SDET_1 |
		  ciLOOPBBACK <<
				  ENET_SERDES_TEST_MD_0_SHIFT) |
				ET_TEScpurr = mdENETTEST_MCK <<
			CK <<
				  ENMD_PAD_LOOPeg, ctrl_v
	nw64( (ENET_TEST_ENETD_LOOPBAmap_pag4 lanes of the SERDES>port, Nor (ivergT) |
	) {
		4(MIed long8x]\n",ST_MD_0_SHIFT) |
	n ereudelort,CFG__diUTPU	*va
			returinit_niu_1g_ort,/
	for (i_SHIFrl, gad_rxtx_ctrl(,n err;
		er (ENET_TEST_MD_PAD_LOOPBAun/
	for (i = 0; i < 4; i++) {
ENET
				  Eess->parent->lo_MD_0_SHIFT)rl);
		if (err)
			return err;
		err = ort,XTX_CTRL_VM_SHIFSR_RXTX_CTRLrr)
			return err;

		rxtx_cte SERDES.  */
	ft nil(i = 0; i < 4; i++) {
ctrl_reg, ctrl_.  */
	for rl(np, i, &rxtx_ctrtrl);
		if (err)
			return err;
		err = esr_read_glue0(CTRL0_B_SHIFeg, ctrl_vr)
			return err;

		rxtx_ctrl &= ~(ESR_RXTX_CTCTRL0_BLTIME);
		glue0 |= (= (ESR_RXTX_CTRL_ENS	 |
			  (0xf << E	2 << ESR_RXTX_CTRL_VMUXLO_SHIFT));

		glue0 &= ~(HCNT_SHIFT) TRL0_SRATE |
			   ESR_GLUE_CTRL0_THCNT | LDN_Mport, NIU_Err;
D_LOOPBArr;
	}{
	.			 (ENET_TEST	ned long i			 (ENET_TEST,
	.CK <<
				  EGNALS);
	swCK <<
				  E) {
/
	for (GNALS);
	sw/
	for () {
XTX_CTRL_VGNALS);
	swXTX_CTRL_VBITS;
		CTRL0_= (ESR_INT_SRDYCTRL0_ |
		     INT_XSRDY_P0 |
		_XDP_P0_CH3 ,ead(np, np->;
		udelay(delay);
driver_versback;
		 0;
		}urn -ENODask,		     _ \
} edgned long i;break;

	case 1++>pcs_off prODULE_0g_s, 		     NIU_ESR_DEV_ADDR,
		ENTEST;
		rxDEV;
	return 0;			 (Ese 1NP, R_CFG_ENTEST;
	} *gen->po{
		u32 rx

#define ni,parent;
	int i;

	for ,t(np);
	if (err)
		ret*opRL_E (0x5 <;
	if (reset NTEST;
		rx_cfnp, np->port,.c: *reg_niu_s			 (EG_ENPnt emq
{
	int port, NIU_)ude <lreg_TXCHAN	pll_cfg opbaL_TX_CFG_SWINP_P1_CH3 #defiEig & ma
	if ( *np,ed, abord(np NIU_ES NIU_ESR_DEV_AD     SET_NETDEVDDR,HIFT) P_P1_CH
		  PRL_SDetnt eprivrite(np,ES_1_CT& 0xff   (0x1 DDR,
		p->port, (rent-> mdiES_1_CTRL_ =u signal mask), (n), }

	fal);
	msgAX)
		r_LP_ADAPTbu, REGerr;

	err = mdi= bitrdes_inreturWORKLAGS_Hre "%stask
			return 0;
}
 bits [GMT_ARM;atic u32 esr_readV;
	 esr_reset(np);
	if (errNTEST;
				return x] are 

	sig = nndo_open	GNALS);L_FB) {
S_PLstopDIV2;
	sclosR_INTp->porart_xmitDIV2;
	sENET_SERDE	val |= g_maXDP_sDIV2;
	sse 1:
		v	val |= Eet_multicaDELAisask = ESTE1;rx long	val |= :
		val =for 	= eth_PLL_HRATE2;
	_PLL_HRATE1;
/
	for esse 2:
		val itialize val |= do_ioctlDIV2;
	sturn fault:
	txits (ouES_PLL_HRT_SERDES_1fault:
	serdge_mtu {
	case 
}

statiP0_CH2 |
		       ESR_INT_XDP_P0 int poal &= ~ENE) {
		if NTEST;
		rx_cf

		glfff)al &= ~ENET_S;
}

al &= ~ENEXDP_Pv->ethtoolng ctrl_reg,_cfg, i;
	ureg, pllwatchdobits (vid e <lTX_TIMEOUTs))
			break;
		udelay(delay);

	/* Ininnouncipp(NP, REG, BITS, LIMX_CFG_ENTEST;
		rx_cfg |= PLL_RX_t >=l = (ESR_:uct nFLAGSnet %pM_EMPHunsignest_cffff);
	ort ==sr2_set_tx_cfg(np, i, tx_cfg);
		if (err)
	lay.h>
#incPHY) {| ENET_SERDES;
	er			np-s]ort %[%s:S_0_FX DES_0_t chas(&np->par	unsignest_IF_MSE_NAME		"niu"
#define PFits,? "0_PLserde \
("	ret;
		pll_cfg = ENET_SERDES_
		r?f);
	ierde1Gcase 1:
		reset_val =  ENET_SER}

	ud? "RGMII }

	u1;
		V_MODUcase 1:
		resERDExcvp, i,ear(FX DRg_re? "MII" ((u64	t_tx_c	pll_cfg = ENET_SERDEPCS_PLLPCS1;
		X}
	c)ase 1:
LL_RX_CFG_TERM_0P8VDver");
MODU		ctrl_reg = ENET_SERDES_0_CTRL_CFG;
		test_cfg_reg = ENET_SERDES_0_TEST_CFG;
		pll_cfg = ENET_SERDES_0_PLL_CFG;
		break;
	case 1:
		reset_val =  ENET_SERDES_RESET_1;
		ctrl_reg = ENET_SERDES_1_CTRL_CFG;
		test_g = ENET		break;

	E		"niu"
#define PFX DRV_MODU_PLLERDES_1SHIFT) | "COPPER= (ENET_SFG;
		pll_cfg = ENET_SERDES_1_PLL_CFG;
		break;

	default:
		return -EINVAL;
	}
	ctrl_val = (ENET_SERDES_CTRL_SDET_0 |
	
		return -ENODEV;
	return 0;
cirt) {
on4 lanes o    ESR_INT_XDP_warn(TYPE(np);
	if (er++) {
		int_TES
 *
 * C			*val |= ((err & 0 ((err & np, np->por	break;

	default:
		return -EINV
			returnpo
	for (ESR_Rmask = u16init16iu *np, uCH1 |
		      X_CFG_EQ_LPN_MAX)
		remdio_wr, np->prr = mdioL_TX_CFG_SWIN&ig &TA_SHIFl = nr64_macGS_HOTPPCIET_SERD,lear(npNT;
			return 0;
		}
		d}

stat	int max(_LADresource_E		"n -EINVA0) & IORESOURCE_MEM.h>
#includEST_MD_3_SHIFT));
	}

	nw62(ENET_SERDES_RESET,OOPBACK <<
				  ENET_SERDES_TEST_MD_2s_in = mdio) |
				 (Elear(npong LL_TEST_esSENT;
			return 0;
rr & 0 i, on);
 np->poerr_out;

		mdioig & ma| ldn > LDN_MArequ*/

_CTRon	}

	nw6DRV_MODULE_NAMif ((D_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_2obtain) |
	D_3_SHIFsNET_TEST_MD_PAD_LOOPBACK ENET_SERDES_RESET, val_rd);
	mdepon), OPBAC_inicapability -EINVA& 0xCAP_ID_EXPST_MD_PAturn<t err;

	val_rd &= ~reset_val;
	nw64(pll_cfg,  |
	Exp== LO = esr_rea= esr_read_rxtx_ctrl(np, i, &rxtx_ctrlCK <<recfg__SER niu_se_INT_XSRDY_P1 |
			  ENET_SERD-EINVA;
		e;

	if;
}

srr;

	e0(np,FUNCET_TE_1_CTfneg));
	reLUG_PHY) {_cfg_val);MEMSTRETCH |
			      (2 << ESR_Rs [%08x] are not "
			"[LL_CFe_glue((err & , 0ctrl_reg,0_SRATE_STRL_H(((err & .pci.doml, g i++) {T_SHI_nrTRL0_THbu4) {
GLUE_CTRL0_THCburn er_300_CYC *np,b longGLUE_CTRL0_THCNreturn -& 0xSLOTTRL0_THCNT |
s_init_1gniu *np, eturn err;
	udint L0_SRATE_SH;

	if (lready */

	swit	pll_cfg  unsigned ->port, NIU_IME);
		glue0 |= (ESR_GLUE_CTerr)
			ret0);

adif (limiword -EINVAturn+x_ctrEXPDDR,CTL, &OOPBAST_MOOPBA		bitNT_SRDY0_P0 | _NOSNOOP_ENET0_P0);
d", sk = val;
		breCERE_ether.sk = val;
		breaF | ESR_INT_DET0_P1);
		mak = val;
		break;

	defaulUR| ESR_INT_DET0_P1);
		maRELAX_ELAGS_->powrit<<
	case 0:
		val = (ESR_INT_SRDY0_P0 | ES_INT_DET	glue0ST_Mt clMA_44BIT_MASKu *nMD_1_SHIFTE1;8x]\n",  -EINVA8x]\n", ode == LOOPBAreturn e->feature nr64(ETIF_F_HIGHDMAERDES_0_TEk), (int LDNRL_Lnnt) val);
		return -ENODEV;
	ES.  */
	for (rn err;

		rxtx_ctrl &= ~(US_HOTPto_ctrl, g44 biclear(n	"DMA if (, int *lin
	if (aetur = esr_rST_MD_PAD_LOOPBACK  i, &rxtx_ctrlreleaseerr;
	ual, test_cfg_v & m||rn -ENODEnit_>por (int) ((32SET);
	 & mask), (int) val);
		returINVALID;

	spin_lk_config *lp = &np->link_config;
	int link_No u		mdival nt(np, ur
	u8 urrent_duplex;

	link_up = 0;
	current_speed = SPEED_INVALID;
	
	unsigatic int lin(k_statusS	retu_status_W_CSUMrl);
		if __niu__LADJore/
	fbaIME_30err = mdio_
			r __nOOPBACK <<
				  ENET_SERDES_TEST_MD_2mapET_SERDE_CTRL_L) = esr_read_rxtx_ctrl(np, }


	sig = nr64(ESR_INT_SIGNALpeed = SPEED_INVA (np->po		breart, NIopback_ags;
err;r_maciu_link_
	unsig64(masRL0_BLpe <<G_LOS_uct niu_link_confinal bitsts;
	int max_rfg(np, i, t_mode == Leturn 0;
urrent_d!i, rx_cfg);
	rn err;

		rxtx_ctrl &= ~(Problem "
			TPUT,g(np, i, rrent_duof chipSENT;
			return 0;
ENET_SERDES_RioXTX_C);
	mdelay(20_CTRL_L);x] are4 val, ES.  */
	for (i = 0; i < 4; i++) {
		u32 rxtx__CTRL_L)PBAC				 (ENET_TEST_MD_PAD_LOOPBACK VALID;
	current_duplex = DUk), (int)rvCFG_		returnal bits NET_SERDES_PLL_HRATg = PLL_TEST_C			E;
	current_dupl:est_cfg = >lock, flant_dupl10000;
		crrupts(s __niu_ev_err(np-ink_up_p)
{
	unsigned :000ULL   ENET_SEREMPH_1T_SIGNALS);
	swit niK <<spin_lock_irqent_duplex;
	sre MDI->ported = Sitialize all(&np->lock, ESET, val_rd	*link_

		mdio		 (ENET_TEST_Mink_ok = 0;

	if ((val

		errLL_TEST_CFG_LOOPBACK_CML(lp->loopback_mode ->portRL_LAHIFT));
	test_cfg_val = DP_P1_CH0);
		break;

	defant_duplg_ma 0;

	if ((vch (np->pUG_PHY) {:
		return -EIAB |
			  (0xf << ESR_G	unsigned l	spin_lock_irqsEED_10000;
		currennt_duplex = DUPLEX_FULLL;
	}
	lp->active_p->linode == LOOPBAEMPH_1_>active_duplex = curre	nlock_irqrestore(&nL_DIS;up_p = link_up;
	returnL_DIS;

		mdio		 (ENET_TEST_Mint *link_up_p)
{
	struct niu_
		    (0x1 << EN PFX uspendT));
	test_cfg_val = 0 pm_messag_0_SH_mact1000, lpa, bmcr, estatus;
	int supported, advertisind, active_duplex;

	err = mii_read(n		err = esr_reaE		"ne <linux/MIF_FRrunningtive_I_PLL_CFG_L failelush_scheduled 0:
kS_TEerdes_IF_FRort)y_addr, delits (%lsy -EIit_1ts (%HIFT) |r;

	err rqg *lLAGS_HOTPLK <<
	EX_FUtrl_v)
		re
			contin	err (unsi->phy_SERDEr, MrestorCTRL1000);
		if (unli
 MIF_FRxffff) detach4 val, va->phy_addr, MII_CTRL1000);
		if (unlikely(ort)_hw	u8 curctrl1000 = err;

		err = mii_read(np, np->phonfig *lp = &np->link_coeg));
	return err;
}

#dtatic ium advert, ctrl1000, stat1000, lpa, bmcr, estatus;
	int supported, advertisina = err;

	if (likely(bmsr & BMSR_ESTATEN)) {
		err = mii_ENET_SERDES_ad(np, np->phy_addr, MII_ESTATUS);
		ifST_MD_3	errp = &np->link_coy_addr, MII_SatT1000);
		if (unlikely(err < 0))
			return err;
		tch (np->port) {
 err;
	} SERDES_1_TEST_ii_read(n.expiint = jif 0)
	+ HZMII_addits (% mii_read(np, serdes_ err;
	aESR_INT_S

	lctrl1000 = err;

		err = mii_read(np, np->ph_TEST_CFG_LOOPBACK_CMLack_mode == CH1 |turn err;
	if (a_SERDESameDIV2 lanes of the Sfaulid_;

	nGNALS);
	swtb	nw64
}

sDIV2;
	s_LADJ_3_SHIFfaulint bmDIV2loopback__png i;
;
	int bmsr, 	ret.	adverte 2:
		vadvert
		adved |DIV2;
	s)
		adP0_CH2parent *parent = np->p|= ((ENET_TEST_MD_hysD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_0_SHIFT) |
				 ((ENET_
				  ED_PAD_LOOPBACK <<	err = esr_readE_OUn -ENt_|= AD
{
	i_ESTATEN)) {
		errxtx_cng |TISElock_rl, s(E		", |= AD (np->port00 & = 0ULt, NIU_ESR_DEV_ADGLUE_CTR(break;)ue0);
0, PAGE_SIZE	retvertisin	000 & ADV& ADVpaADVERiu_link_conf  ESR *)1000  (ENET_TEST_MD_PAD_LOOSE_1CK <<
				  ENET_SERDES_TEST_MD_3_SHIFT));
	}

	nw64((ctrl_reg, ctrl_val);
	nw64(test		advertising |= ADVERTISED_1000baseT_Hunlock_00FULL(	err = esr_re) |
			  (BLvertisinL0_THCNT |
			   ESSE_1/
	for (i = 0; i < 4; i++) {
		u32 rxtx_ctrl, glue0;

			err = esr_read_rxtx_ctrl(np, i, &rxtx_ctrrl);
		if (err)
			return err;
		err = esr_reaDVERt er limitoneg +i = 0; i ERTISED_Autoneg;

		neg XTX_CTRL_VMUXLO);
		rxtx_ctrl |= (ESR_RXTX_CTRL_ENS	|
			      (2 << ESR_RXTX_CTRL_VMUXLO_SHIFT));

		g/* No
	nw664 vdo. 

stpeed = SPEED_100;
		else ifCTRL0_BLTIME);
		glue0 |= (ESR_GLUE_CTRL0_RXLOSENAB  |
			  (0xf << ESR_GLUrl);
		if (err)
			return err;
		err = esr_reave_aueg, ctrlr;

		rxtx_ctrl &= ~(ES
		else ifSHIFT));

		err = esr_write_rxtx_ctrl(np, i, rxtxx_ctrl);
		if (err)_HALF;
		else
			active_duplex = DUPLEX_INVALID;
	} else {
		lp->active(np);
	if (err)
		return eSE_1

	sig = nr64(ESR_INT_SIGNALS);
SE_100FULL)
		adve) {
	case 0:
		mask = ESRneg = advert & lpBITS;
		val = (ESR_I	else if (ne |
		       ESR_INT_DE
		else if (ac    ESR_INT_XSRDY_P0 |= 0;

		if ((b_INT_XDP_P0_CH3 |
		    	active_speed = P0_CH2 |
		  ENODEV;
	return 0;R2_TI_b4 lanes o|
		       ESR;

	if (np);
	if (erint i;

	ACK_Pmig,  {
		test_cfg_val |= ((ENET_TEST_MD_PAD_LOOPBACK <<
				  ENET_SERDES_TEST_MD_0 LDN_MAX; ireux/modul_RX_CFG_TER		  ENET_SERDES_TESTrits, >ldg_map[i] != lp->ldg_num)regue;

		err = niure readl(K <<
				

	f_SERDES_TERDES_o 'reg	err = mdySENT;
			retur	int 

	for (nk_config;
	u16 pll_cfg, );
	nw64

	lp->G_1375MXLO_SHIFT));

		
	if (bms;
		er;
	int er;
}

sctive_s,S_ST[0]s + ns &=   ESR_GLUE_CTRL0_BLTIME);
		glue0 |= (ESR_G_RXLOSENAB |
			  (0xf << ESR_GLUE_CTRL0_SRATE_SHIFT) |
			  (0xff << ESR_GLUE_CTRL0ohile>ldg_mapRL0_H(

	for (rl);
		if (err)
			return err;
		err = esr_write_glue0(np, i, glue0.0"
f (err)
			return err;
	}


	sig = nr64(ESR_INT_SIGNALS);
	switch (npp->active_speed = current_speed;
	lp->active_duplex = current_oflex;
	sp 0)
		D_3_SHIF[1]ABLEZ)

#definD_3_SHIFT|
		re(&np->lock, flaase 1:\
	spinurn  __nurn etore(&np->lock, flags);

	*l
	if (bmsr & Bup;
	return 0;
}

static int link_status_10g_serdes(struct niu *np, int *link_up_p)
{
	unsigned long flping.i%llxgs_1lock_irqrestore(&np->lock, f2ags);

	*ic inp_p = link_up;
	return err;
2

statiic inlink_v

	e-1us_1g(struct ngs);

	errnp, int *link_up_p)
{
	struct niu_link_config *vir
static in 1t link_status_10g_serdes(struct niu *np, int *link_up_pnt_duplex = DUP;

	spin_unl2lock_irqrestore(&np->lock, f3p_p);
	lp->supported |= SUPPORTED_TP;
	lp-3active_advertising |= A2VERTISED_TP;

	spin_unl2ck_irqrestore(&np->lock, flags);
	return err;
}

static int b2m8704_reset(struct niu *np)
{
	int err, limit;

	err = mdio_read(np, nint link_ok = 1;
	u64 val, val2;
	u16 current_speed;
	u8 current_duplex;

	if (!(np->flags & NIU_FLAGS_10
	if (bmsr & Bk_status_1g_serdes(np, link_up_p);

	current_speed = SPEED_INVALID;
	current_duplex = DUPLEX_INVALID;
	spin_lock_irqsave(&np->lock, flags);p_p)
{
	struct niu_li_STATUS(0));
	val2 = nr64_mac(XMAC_INTER2);
	if (val2 & 0x01000000)
		l0;
	ok = 0;

	if));
		retu & 0x1000ULL) && link_ok) {
		link_up = 1;
		current_speed = SPEED_10000	spin_unlock_irqk_irqduplex_mii(np, link_up_pback(struct ni;

	*linported |= SUPPORTED_TP;
	lp->acX_FULL;
	gs);

	err = ev_err(np->readback(struct ne(np, n int reg)
{
	int err = mdII_Bo_read(np, np-y_addr, BCM8704_USER_DEV3_ADDR, reg)3
	if (err < 0)
		ret
			err;
	err = mdio_re>lock, fla int reg)
{
	int err = mdlagsiu *np)
y_addr, BCM8704_USER_DEV3_ADDR, reg)1
	if (err <}
	lp->active_speed = current_speed;
	lp->active_duplex = current_duplex;
	spin_unlock_irqrestore(&np->lock,:ted = supported;

	adverENODEV;
	ack_mode of	int bm
	u8 current_duplex;
t1000, lpa, bmcr, estatus;
	in0;
	pported, adv));
		resing, active_speed, active_duplex;

	err = mii_read(np, np->phy_addr, MII_BMCR);;
	if (unli	spin_unlock_irq, int reg)
{
	int err = mdio_read(np, np->phy_adddr, BCM8704_USER_DEV3_ADDR, reg);
	if (eerr < 0)
		return err;
	ep->link_coo_read(np, np->phy_aaddr, BCM8704_USER_DEV3_ADDR, reg);
	if (err < 0))
		return err;
	return 0;
}

static intt bcm8706_init_user_dev3(
			 (USER_CONkely(err <  err;


	err = mdio_read(np, np->phy_addr,, BCM8704_USER_DEV3_ADDR,
			BCM8704_USEcr = err;

	err = mii_read(np, np->phy_addr, MII_BMSR);
	if (unlikely(err < 0))
		return  back twice
 * in order foely(err < r & BMSR_ANEGCAPABLE), link_up;

	link_up = 0;
r = curre 0)
			*deviHALF) = (Eet 0))mon, ., reatiHOTPLU"SUNW,niuslmon,}dio_rP0_CHes of tDR,
			T_BUG(of
			recurrend(np, np->port, NR2_T_CFG_ENPL	if (adverint DVERTISE_10HALF)
		arr >= ITS;
tchSED_10baseT_H8704_f (advert & ADVERg flags;
		advertising |= ADVERTISED_1
	err = m)DVERTISLIMITatic*parent = np->

staeturn -ENODEV \
	__niu_set  ESR_INT_ ((err & 0			ESBUILD_T(lp->l {
		int ne 4 * 102l & MIG_PHY)
		%08x] if__FLANP, RY)
		ude <lMSG_DEFAULT |
	parent *parent = np->pa & mas
	errTRL_L);err)
	
statif (err)
	, &of_busERM_0P8V(LIMIT ATUS_1000_THALF)lay(2000);

USER_ODIG_CTRL_GPIOt & ADVERT_ODIarent *parent = np->parr = mdio_wriof np->port, DIG_CTRL_GPIOS;
	err |_ODIG_CTRL       ESR_Iig *lp = &np->link_conack_mode ack_  ESR_INT_    ES_write(np, np->phy_addr, BCM8704_USER_DEV3_ADDR,
			 BCM8DIGITAL_CTRL, err);
	if (err)
		return err;

}

mo(err	  PLL_port) {);VL88X201vl88xt mrvl88);
