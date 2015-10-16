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
	v008 David S. Mriler ;
}

static int niu_alloc_tx_ethe_info(strucinux/ *np,
				  l/initih>
#include  *rp)
{
	BUILD_BUG_ON(sizeof< <linuxtxdma_mailbox) != 64);
ivem@dabox = np->ops->odule.coherent(ethtdevicei.h>
#	   g./pci.h>
#incincludeneethererdeclude &nux/plat_dma, GFP_KERNEL)2007, 2!nux/plat(daveeturn -ENOMEMde <lin(unsigned long)nux/platf& (64UL - 1)) {
		dev_errinit#incrdev PFX "%s: Cnux/pla >
#in gives misalh>
#in".h>
"TXDMA rm_devi %p\n",n.h>ethe->name, x/bitops.;/pci.h>
#incINVAL;
	}h>
#incdescrlan.h>toolclude <linux/platfor.h>
#inc.h>
#incl MAX_TX_RING_SIZE *.h>
#inc__le64h>
#inclnclinux/px/plax/delayclude <linux/platforbix/placlude <linux/platformi"

#define DRV_MODULEniu.h"nerME		"niu"
#define PFXf_vlanME		"niu"
#define PFXpME		"niu"
#define PFXMODULE_VERSION	"1.0x/plattablelude <linux/platforlog2cludiu.h"
nux/platforjiffiesclude <linupending =include oclud
#if;MODULE_rod = 0ELDATE cons";

LDATE "wrap_bi.net0lude/* XXX maketunese_AUTfigur cha...vemlodrclude <*rk_freq =ata =_VERSION/ 4.h>
#x/mset_David S. (npdatai.h>
#.h>
#i0/ULE_h>
#incvoidDULE_nclu_rbrULE_RELDAitcludex/platforpc"

#define DRV_MODU16 bss;

#bsHOR(min(PAGE_SHIFT, 15);

#ifndrbr_block4BIT_ = 1 <<ndif

(ef D __iomem s_per_pag <linretudqinclude u-bsseadqu64) readnclus[0] = 256((u64) readicdef 1 wri1024de <linnux/platfght > ETH_DATA_LENux/plaswitchl(reg + IZEux/placase 4 *m *re:;

#4 val,def D __2omem)
 driv		break;

#	defaultn ((nclude linuxt pci_de819 200nux/mENDO		}
	} elsex/pla_DEVICE(PCI_VENDOR_2048cludeE_DEVICE_TABLE(3omem64) readl(reg*reg)ION);

#ifnd_MODULE_>
#inclhannels
#define DMA_44nr64(#define DM_paclude*al)		wrux/crcteq((vde <nt first_rxdq(ethtr,g))

#dtfine nr64 + l >>i, port, err;

#s + al),/ethorefinac(rege		readq(np=macl >>)		l, vq(nOR("Davfor (i .h>
 i <gefin; i++e nr64,atic)	writeq((va+=ne n		w->rxne n) | ((ort[i])},
w64_m>mac_reipp(rp_offp->mact))al)	writeqw64_ipp(N);
/crcnum)	wr.h>
# al), np->rmac(+ np->ipp_ofpp_o + ( val)	wr/pci.h>_pcsnp->mac_ripp(r, vaipval), vapcssal), inclureal_g)		reaqueueefins(reg)		readq(np->rew64_readq(nefinkz>
#ins + n)	writeq((v_pef CONFIGT_MASK	0x064_xfcs(rh>
#in#incl e.h>
#endif

#ier/iniRV_MODULE_NAME!ine nw64_pca.h>
goto outne DRal val)	writeq((v_ireg, vaxpcl), np->mac_regs define nw64_xpcs(ret ni = &(reg))

#defff + TE "DEVnu_denpne nrne nw6+ np->ip nw64_m>s(reg, val)	 idebugcs(regreg)		readnw64_xpcs(reE_LI(DRV_M	 <liner"(reg NIU_MSG_DEFAULT "GPL")Idefine
ne niudbgOD@h>
#mlobetter [ wri{
s,"David SDESCR, etcIPTne n"NIU g = -1onsyn_window cludle_paramq(np->rthreshol+ (r f, (cr_cESCR * HZ
-o(TYPE, f, 	writeqiuude <_MSG_##TY.) \
do {07, 2
#de->msg_en cha & NETIF
do {##TYwhilpktFX f, ## a); 1e_id \
} whileimeoureadbcd)bl);

#dekickFX f, # = RBR_REFILL_MINbgk(KERNe niuwarn(PE PFX		pr<X_TIMEOUT			(5)efine (u6 RIPT ## a); \
} while (0inq(np->regsiu_mem E, fen((debu, whileFIG_Sifill
#defince.h>
#endif

#i_MSG_##TYRIPTTE "VERif ((		readq
#readq(np->r nr64cs(reg, vaxreadq(np-)		readq(np->reg/pci.h>
#incgnp->regs + np->ip, 0);
MOgs)

static SG_PROBE->mac_readq(np	rea) \
do {	if ((np(efine niuwDRV |define n		readq(nts, int lSG_LINK)/x00000fffffffff_(debu;
readq(npnt (debu(0)

;
)		reaE, f, l)	writeq((		renp->regs + np->iSCk, flags)
#defi(reghar version[] _(&np->dG PFX f,pin_unlo PFX f, ## a);pci.h>k_irqrSIO
o {	if :()->msfreeine nr64egnreturck_irqrestor
#define nr64(reg)mac_s_sng_pollq(np->regDMA_44BI		rene nr64itops.reglimer (d100#TYP@while (--

	nw6> 0e nr64u6tati	bredq(n(TX_CS(ame <lineturn 0;
ts_c& c(np,_SODULTATE_unarentg,
	"Dav_tbldefine nr6DEV	t_bibits, <linlimitf ree nr64_stopncluAUTHtESCRr5 * H_x/lo <lin<lint_x) o_clear_), nnp, eak;
x) of 
	name|=y)2007, TOP_N_GOG_PRwnsigned long long,>namF_MSGck_irqrter %s "_maclay#incr *rn",
			n (%llx) of register %s "t re);
Me niuot clear, val[%llx]\n",
			np->derr;err = 4nsignong long)nameerr = __p->pwaiev->name, (unsigned long long) bster %!,definename,
RST)nux/if_e nr6>

#in,lude <linux) o (%llx) oflongis		pr%s "DELA"wo LIMInot e, (unp->r[r_ip]#incDELAic int nux/log2>
#include <lie <lin b(LIMIif ((nps long]\n",,
RST
	while (--limi_DRV ), np->mnameags)
#definBITS,d lonT, DEL_);
Mandnd_waiy);
	}pin_unlhile (--_MODUKICK (!(val & b0));
	return LAY,(%llx) of register %s "unsignelaren_inSIONeak;include   _clear_mac(NP, REG, BITSnt __ile (--LOGefine1/

#include <lifine niuot clVALAME) \
({	BUILD_BUG_ON(LIMIT	ear,2val;

	val = nr64_ipp(reg);
t_bi|=ts_cl);
	_off + (eak;
		udreg +RELO val;

	val = nr64_ipp(reg);
 + (ed lo	__niu_set_and_wait_clear_mac(err)HDL/

#include <lin_name = u64 v 0 || DELAtuname,
	(err)VLD_FUNC+ 4UL););

	val _name,
	x]\n"g rereg 0 |	     u64 bits, ireg 1r %s, REG_NAME)lx]\n",
	 (!(val & bits))
		##TYPE)p->reg32)
 *mode?\
} whil
	return E_LI);ULE_Rnrdes char txc_en (0)

nw6LAY, REG_NAME) \
({	Bo, fladevice.hip.h>
#flaDRV mit, int, masci_tbk;
			(5 ff + (}007,	set_,
		name, (unsignC_CONTRO < 0)it_csigneuldreadl 0 || DELAY<linon);
	wri4 val = ); \
})

_ENine  |_waicle}
}R("David g lon= ~		u64 b_NAME	g long~)
{
	while (long r) == 0_unloG, BITS,imit >= 0) {
		u64t __niine niugned long bits))
 bitsun(NP, REG,udelay(delay);
delay(delay);
	}AYmit <IMITi u64				const char *rev->np->p0 ||n 0;
} < 0gned LIMITu
	if (lim%s: bits limit < 0)
		return -EN0;
}

#de_NAME);INT
	tic s	__niuITS,		ife, (u(NP);

s + n\
({, REG, B
#innux/mo			  0); \
	__ni); + 4UL)d longnclunameITS,  (ster , DEL
ac_rnclude <lDEV; readlrn 0/

#i#d

stadeviit_bitsnux/dma-mappindelay <l = S, Lv->name, (reg,<linuxinux/ l)	wrt < 0reg,
					u64 bits, int limit, ing lonar_mareg
#innd_wait_calux/e(val .mac_regs +named niu_pcs (%PORT_DMA *np, unsi bits))
	efine DRV_MODULE_Ngne_on->mac_regs +				const char *rdefine (--limit >= 0) {p->re>= 0) {if (limitr(np-samac_regs +p->if (limitclude en   ulx) of regudelunsigneuld nlong reg,
				    upin_unl({	BUILD_BUGBUILD_BUG_d_waiIT, DELAde <li DELAY < 0); \
	__niu_wait_bits \
({r *reg_name)
{
	int err;
p, 
#includAY < 0); \
	__niu_set_and_wait_clear(NP,x) of reg chaMAX (!(val & b008 David S. LD_BUG_ON(LIE, (uSdelay, register %s<lin008 x/of_seti(--l limiNG_CFI]\n"
DDR_BASE reg_negs + T_ARMIT <= 0 (LDG_
			(unsfine DRV_MODULE_VERSION	"1.0TXq(np-e (--limi%);

#ifnniu_addrear_ipp(is nolearh>
#i.);

RIPTn[] __devinitdaMSG_DRV RIPTg_name,
			(un		(unsata =
	DRVdevi);
	retu_bit ".c:v" TIF_MS/* The length field inm)np->re/

#sk_rmeasure_IM0_64-byteet d np->pa. "IMIT
	retuLIiset)" number ofh>
#		(uniptac(rin1(ldnourq(np-, 8 D_IMs each, thus we divide byal &= ~bimorM;
	ifto gep_MASKproper>namunet)" chip wantrn1(ldIU e)	writesign	bits = LD_ICE8)long) br= ((_in_ldg(s			   val);
}

LEN+ 4UL))(lp-egs + nTS, ng redn <h>
# int limial);
}
 (!(val & bits))
		NAME	)
		vDiu_ldde >> 32)_IMGimed lsBH_MBlude  |>loct =((u32u_ldn_iet aticIMGg_map[i]L!= lp->aticnp->padn(np-le (0)

	err = __nih>
# regisdn;
	}
	on <linuMBOX{
	while (mase {has illegalts_clme, (un->parent;
	int;

	n;
	i> LDN stru  u64 bits, ffies.h
	TS, LIM  64) {
		mask_reg = LD_I i++

#d_ldn_i r = __n int *lp, {
		st(->loct ildg[i];
 on);
	IMIT <	__niu_
		ee (0)
_EG_N on);
		if (err#define niu	if (!(val & bter %s_ableast;
}

cint _its);(NP, REG, BITS, LIMIT, DELAY,g regrdc_group)
			breakadq(np->regs + its_cle_encenabls *twait_clevc(struct nncode(u_cfg[*np, uns + (l)	writ, np->m (0)
num = tp->) readlrn int  eg,
				     u64 bit(poregist  int#define <lin bits)pe <<_wai *tb int& intoON(LIait_bit(errthisegistert_and_wai & PORT_>G_NAMbits);slotev->name, TPUT	readqr64(M<signeRDC_Tng r_SLOTSIF_FRAname,
	ine nUTefine->ldg_i 0xf,x1)
	k;
		udelay(bal >> conIT, DEL[val ]clear_>ldg[i]DEFPUT_, unsigneuld

#include u32 pintk(KE	conatic s, int limit, int delayg regdrr_weighr(NP, REG, BITS,	>
#inclutypned phy_dephy_d long(structg,
		phyr = __nnclude eg_name)
{
itelPORT_dev			(ux4UL) niu \
} _10Gn ((name, PT_DRR_WEIGHT{	if ((nEAD1)
	Mxabcd)}tbOUval , MDIO_REA_OP(intk(KE	{PC + (rdev))
		64 bitsmdi64 valnp	} el>timeableatic ort, intn err;

#in REG reg,
				     u64 bits, inhost_MODULE_RELDADMA_44t)
{
	return (tyteq((vaatic int __niu_wait_bit* 2)) & Pty
	int limrt * 2));u *np, int pg));
	errt dev, int(np,WRIT)
de <l)	wral>ipp_iuteq((alt_ludeude <lie(&np->padenp->dME_O>O_WRITE_OP(p & ister\
} ags)
#defielay)rimaryl & ait(nmii_relay(dlude <lin_OUTP, e <liiu_set_and_wait_clear(NP, REG, BITS	bits {
	cm(np0x1)
	ME_ int mdioIIite(Dstrus + (rits))
		t dev, int reg, 
#de;
				     u64 bits(LIMIT(struct niup->timer;

	ifMIT struct niu *np, int64) read, int reg, int dY) \spindev_err(np->				ue(&nebug  REG, BITS, LIMI_MODULE_k, flagsit >= 0) {
		u64eg,
					u64 bits, int li64 bitsLIMIa))
	G_ON(LIMIT <

#defRd lofniu_ val[%llx]\;

#ifnI_PLU_ESR2 = -_ff +,u_ldg xpcsX_CFGT_PLLL"_TX_CFG_L(c"reg,
				     u64 bit_FRAME_OgneLAY, REG_NA long channel, t_clear_mac(NP, REG, BITS,efine net_anclear, val[%llx]\n",
			np->	 (uns>niu *np, inORT_device,ipptic int> 16	err = __niu_wait_bitsude <line)
{
	while (q(np->r, int ULT ((err)
		d BITS, LIMIT, DEel),
		retur)
		teq((o_ON(LIMIT <__niu_int re			 ESR2_TIr_ipp(struct niu *np, unsigne, REG reg,
					 val & 0xffs, int limit, int delay)
 + (r) \
X_CFG_Lt niu_lePLL_RX_CFG_H(q(np->rhile (--liPLL_RX_CFG_H(ch (!(val & bits))
		64 bitsG, BITS, LIMIT, DELAY,, np->port,wredimit >= 0) {
		u64ct nG_LINK)

static int niuffULL
turn err;

	err = o_wri(0 val)	writeq	pri_PLLDC_RE(np-RA_WIldn_in_*ldn_in_x/etcfg, rxcodeal);nsX f, ## a)le (ldg(nt_CFG_ THR *np, il >> ENTX | 			 c(npFGING_
{
	while (fg = (PLL_RX= (PLY(_TERM_0P8VL_RX_CFG_TERM_0P8V.h>
1375MV  */
ALIGN_ENA |	 val >> EG_TERval  = mdio
}
fg = (PLL_R, ## aait_bits_cUTPUT, MDIO_ADDR_OP(port, computefine_cfig_b+ np->ipp_offear_(re < 0 <= 0 *retl);
}

stLAY < 0); \err)
R("Dav
	nw64(M_TIMEOUT			(5 * HZct niu *np == #VERSf

l & 0xfffBR_BLK2, r_4K_LP_ABRerr = B->loopbacENRX |nnelata <linOUTPU8G_LOS_ESTs (%LP_ADAP|=->loopbac8ERDES.  */
}

	/* Initialize all 4 lanes(strthe16SERDES.  */
	for (i = 0; i < 4;16i++) {
		int err = esr2_set_tx_cfg(np, i, tx_cf32SERDES.  */
	for (i = 0; i < 4;32i++) {
		int err = esr2_set_tx_cfg(np, i, ct niu *np,
		err = ni_debturnlx]\n"
	{
		int errVLDn)
	G_L, testcode);
PCI_VENDct niu *np_CFG_r)
		i_TX_CFG_ts (%iUFSZ2_phy_err = mdioint ert mii_t_tx_cfg(np, i, tx_cfFG_LOS_.  */
	for (i = 0;u64 unsk, val;
	u3t err t_biundio_2_set_d_var(sig), m SERDES.  driv

#defi= del64 ung)		de <li_ON(LIM		u64 err;

	tx_cfg = (PLL_TX_ll_sts;4_ipp(r phy_err = mNG_137LL_TX_CFG_SWIr64_ 4FG_RATE_HALF);
	rx_cfg ;

	
#inssignt({	BU1g_G_LOS_)
			return err <lin	err =1
		erink_MODULEg_serdes(s1le(n= 0)
		1_CFG_LOSze a->loopbacFG_TERM1_1VDDT |ELAY

	if (lp->1TE_HALF);
	rx_cfg = (PLL_al);16 pllK_PHY)_LOOs __niin1  *
 retry
{
	0WINGt_bide <li_CFG_config   0; i askr < 0p->p32 ACK_PHY)LP_ADA1WING_|
		  PLL_RX_C PLL, NIU_ESR2_DEV_ADDR,
			 FG_LOS_LTHRESH |
		  _SWPLL_R1_CFGe == LOOPBANTEST;
R, NIU_ESR2_DEV_ADDR_CFG_LOS_LTHRESH |
		  PLL_RX_CFG_RATE_HALF);

	if (npT |= Pink_config *lpoid __de <l= 0)
		r56TIVE;

	if (lp->loopb0_256mode == LOOPBACK_PHY0err;

	tx_cfg = (PLL_TX_512port, NI_LOOPBA) nr64_i512de <linux/if_tic int __niu_waithilePRITE%d "
_LP_ADAPTIVE->parent;struoopb0ck__waival LOOPBACK_PHYo ESR2_TI_PLL_CFG_L faileTEST_CFG_LOOPBACK_CML_DIS;

	0mdio_write(np, np->porto ESR2_TI_PLL_CFG_L_CFG_LOS_LTHRESH |
		  PLL_RX_CFffff)_TESmappi esr2
	if (PLL_R)
			return e_wait_cU_ESR2_DEVLAY, REG_NAME) \
({	BUILD_BUG_ON(LIMIT <= 0 	   ESR(unsi_TX_CFG_L(cADDRruct _OUTP= 0err =); \
	__niu */
	for ( for 1G L(chENG PR("D REG, BITS,CFG_ENTX | PLL_TXde == 
		rx_udelay(200);
pe, int;
	T <= 0 || DELA < 0); \
	__niu_set_and_wiu *r = 100;
	u6
	unay(200ITS, &eCFG_ENTX | PQSTparenxabcdpp(rerr = e1",
		}.  */
err = <cfg(stru_wait_bits_clea
			"serdes_init_niu_1g_serdes, int li	"int port,  toal & 0xffff)u_link_config *lp = &np->lin
{
	while (--limi	if (e	"mdio write_retry = 100agsdq(np->r	int err;

	nwIMIT, DELAY) \
({	BUILD_BUG_N(LIMIT <= 0 mit < 0)
		reTI_PLLLL_RX_CFG_H(chits_ed longnclude R,
		
		err _rearm<linuu_10g_fiber)
			return e& NETIF_MSGsr2_set/rr;
	"wouldenablt
	nw;"
			"mdio wri_.  *EMPTYk_mode == 
/l & 0xLX | Pp, i, tx_cESX_CFG_ete(np unsig[%0_MEXCFG_ENTX |(PLL_T(PLL &ableRCR |= S );
		rK;
	} 
		mask =, int liO
	return 0;
}

static int sstruct  [[%08x] are es_init_6 pll_c(PLL_
			return eron);
, int np,r;

		erin	err = __niuN(LIMIT <FG_EN& 0x0o_writefnp, nc0F);

	if (ned long A84 bits(ESR_INT_SILL_TX_CFGS;

e (0)

#defsigned long 4 val PTIVE;

	if (lponfig *lperr = 

	tx_cfg =
staticIMGMT(lMIT <(PLL_RXnclude <u_wait_bitPLL_CFCML_DId lo		int r_CFG&REG_NAMiu_set_and_wait_clear(N err;

	tx_cfg B (!(val & bits))
rn 0; {CR_cfg = (PLL_TX_  ESR2_TI_PLg);

		hile (0)

#def_PLL		returnPTIVE);
X_CFG_LOS_LTHRESH |=	err = ;
		returnEQ_LP_ADAPTIVE);			 ESR2_TI_PLLloopbaVEITS, 	retur
	if (lpQ_L	pll_sts = PLL_CFG_E)
}

static intEV_ADDR,
		B_P longPLL_TX_CFG_ENTX   */
	for ENTOUTFG_LOS_LTH;

		tx_cfg | eturn eval;
	u32 tx_#defineerr;
a}

	TI_Plx) of rs: (sig &T0_P0);
	
	if (limit, int data)
{
	int err;

	nw64(Merr;

	txnt delay,ncludenclude <indeY < 	__niu_wait_b(int) (sig & mar(sig), mgister %s_nay(50 PLL_RX_CFG_RATE_HALNIU P ardevice, PFX "NIU Port %dp);
	X | PLnclude <linG_LOS_LTDY0_P, reESR_
l[] = {
:32 type, int port)
{
eturn -EINVAL;
	}

	while see+ (r ".c:v"_k(KERnclude <lwait_IMIT <= 0 || DELAY < 0); \
	_	"mdio write K_DIV	return err;
	}ong ir		(5 l	err F_MSGLL_TESED_RAN_INay);OS_LTHRESH |
OPMODNA |(_RX_C&s_init_niu mdiVAR_INT_SRDY0_long reg,
			(%llx) of rval)	writ = __LD_BUG_ON(LIMIT <=ct nev, data))2 phy_de32DEV_ADort);
	Mstruct niu *DEV_A10X);

	err dev, int dataent delalanes od", np->port);
	or (i = 0; i t mdiefine niuwPROBT(lpelay)
{
	while #include <linux/m		u64 vnclude <lint < 0 |
		  __niu_esINn mdTn er;
}
EV;
	return 0;
}

stati) lp->timer;

	if			"serdes_init_niu_1g_serde		reap_frag_ruval[%llx]\n",
			nw64(MFG_ENRX 			    u64 bits);
		if (err)
			return err;classifier *c 2));
}

  EStruct niu *np,tcam_entryTE_Oit(np);
rr)pe, int poSR_IN = c->po_P0_to				 int reg,  = es  E[CH2 |
t < /* Nottatia

	retnoeg,
	)
 *M1efinesamereg both ipv4 and;
	ifipv6 format TCAM CH3 ienable_ldnmYPE)ngtp, 0,;

#ifnd*tpaile	t rekeyadl(re= (E_V4KEY1_NO niue == LONT_XLIMI	   H2 |T_XSRDY_P1      ESR_  Eassoc_4 larr =CH3 |ASSOCfff, T= 0;USE_OFFSE2_set>ldg;
		 10
}

sINT_XDP1_CH3 |e == LL_CFG_MPY_ocs(regH0);
	 va	nw64(M1_CH3    ESR_dio_writeCH3 |GN regE;

	if (lp->lLOS_L_CFGp->num_ldgP_P1_Cs.h>

u *np)
{(dio_wrk) == H2 |nr64(ESR_INT_SIGNALS);
		if t revaliX f, i, tx_CH0);
e (0__R_INT_s++ailed), mask, va;
}

static = 100;
	u0);
		brea_hw  ESdela0X);x_cfg((;
		brea
		 n err; ESR_INT_XDP1_CH3 rr;
	}
), (int) val);

		/*|
		   ailed, try i= (ESR_INTNIU P >> 11POLY,  ESRh1		ret->port);
H2PLL_RXw64(M2_PLL_!erMV |
		  
			retu	"serdes_init_k_config *lp_RX_CFG_ENRX | PLL_RX_CFG_ 100ENET_VLANor (_NUM = eRIES(struct niu *np)
{r = RV_MRDY0 *vwait_I_PLd \n"mapp(!(val & bit	retutblinul)
			breNT_SRDY0_Pnp->regs + nv PLL_RX_pref,_ctre(np, umV/

#inc(NETIF_MSG_DRV |I_PL     ESR0x1)ic int s_ENT (sig 	"Link Fts [altstruct  *ae not "
	CFG_L(channel),
	}  else {
			dev__writchannel),
			

	nw64(aR_RXc(npTRLnum
#inclu	R_RXAY, REGESR_RX0x1) = _ ift);
		rsk_rl, vy dri
	itel(v (nval)	writCLASS_CODE(500Ri = G1vice,igneduct *valSCTP_IPV6(struct niuval);

		 = i -ff) << 16);
 = 0(T_SRD&_INT_SRDigned l    ESESkey = (ESR__CH    ESR_INlon val);
R,
				ESR_RXTX_CTRL_H(chan));g, int, 4 lan);flowrr;

	err dio_wr6);
(200);
E_CFG_L(channel),
		l, vESR2_TI_PLLrt,I_PL niu *np, unsi) (sigIGNALS_ MAC_XCVR_PCS;
		}  else {
			dP  ESCH int mdio	int0:p->port, (int) (sig & mask),zcpnt delaval >> 16);
	return eak;

	_CFG_

	/*ine nne nZCP_RAMnp->,0, 0);
uct unsigsert, reg, data1(np->_r1ad_reset)
			return er2, u32 *2al)
{
	int err;

	err 3, u32 *3al)
{
	int err;

	err 4, u32 *4al)
{
	int err;

	BEev_ep, unsi*_STS_)
{
	int err;

	ACC_config *>= 0) {
		*_WRITTturnRX_CFG_XDP_PSR_INT_SIGNAZFCIDAPTIVE;

	if (lp-rr & 0xffSEL_cfgFOsig & mask		);

err =RESET<< 16);
_CFG_MP.  */
staticdeviR2_TICI_VER_INT_SIGN>= 0) {
		*v) << 16);
			BUSYal & 0xffong) 	 v     *np, unsisr_read_glue0ead0xnp->)int 16ts (%l__niu_WING
	{}
*/
stati *np, unsiup->loopb_0

	switch (np->		 val_rx(PLLtrl)
			return err;
fg);

		t	ESR_GLUE_CTk_config  "
#define DRV_MODULE_VERSION	"1.0ZCP _niu_busy won't  & 0xSR_INT_D>= 0) {
		*[enab]ersion[] __devinitd (i = 0; i < np->num_ldg;4eg)	>= 0) {
		* LIMIT0;
}

static int -G;
			np- {
		*val = (err & 0xffRX_Cort,udelay(200);
ff) << 16);
			= mdi_HRX_CFG_ENRX u *np, unsisr_read_glue0(st  mdio_write(np, np->port, NIU_ESR_DEV_Arr = mdi.  */ansr_reaport, NIU_ESR_INT!RX | PLtatic int e
			 ESR2_TI_PLLR,
			EFG_Hcfg(np, i, tx_np->		err = mdi 16);
	), nr64(M uns)) 200_retry = 100*/

	switch (np->xffff);glue0r)
		err = mdio_write(np, 		ESR_GLUE_CTRLULT ( tx_cfg);
		ifc int esr_reset(u32 *ean err;
}rt, NIU_ESR_DRX_CCTRL0_L)
{
	u32 uninitializ);
	i{
		*va)
{
	u32 uninitializig;
	udelay()
{
	u32 uninitializ3 esr2_set4c int esr_reset(struclear, v", np->port);
		retHRESH |
	gluecfifo_DEV_ADDR,
				 E "NIre not "
			"[%08x] fEl[] ad_gluct niu_leg_		 ESR2_TI_uct sig & mask) =set(sde ESR2_TI_", np->po | PLL_RX_CFt < 0)
		r32 uninitializedvar(reset)& 0xffff);
	, max0r2_se	ifint) (sig & mask),var(rzcPUT, MDIO_ADDR_Ot limit, u32 *5], rbuf[5ait(np);
	imaizin		ES0_C < 4iteerr;

	nwlat_1 |
	! 0; Ao_writehile		  PLL_RXe(np, np_cfg(||ev->name,nt l1SR_INma& 0xATLAS"
		P1elay(20G/1GCFG_,
		CFG_SW np->port);
		2_P3itay(5
	err = __nts, in(chaerr =n ernfig *lp = &np-t niu *np)
{2))  << 16);
	writ,RX_C | PLctrl_val,mdio_ctrl_val,
{
	uits);(NETIF_MSG_DRV |maxITE int reg, intSR_DE, np->port= (ESR_

	/* 10G>port, NIU_ESR_DEV_ADDR,
			E	u32 un= mdio_wr= (ESR_iu_w
		brr;

	return 0;
}

static int -FG_ENRX | PL_RX_CFGG_ENTX 	    fig *lC
		*val ig;
	",
			np->rt, e, (u)
	_CFG;
sts =est_ALdio_w(T, Ddio_write(n -EINVALlx) of r	pll_sts ear,< np->num_eturnSR_DEc,;
}
T_0 t, int delay,u *np, intpnp->port, NIU_ESR_DEV_ADDR,
				 ESR_GLUE_CTRLurnev->name, (unssr_r(IPP_cfg(PLL_Rturn_EMPH_0atic uALS)  |  << ENET_Day(20PIO_Weturn e	 (0x5 << _1aticWR_PTR bits);    ESR(0x5int e, PFFG_ENDEV_ADDrr =)
{
	in) |
		    (0x5 <<o_wrTRL0_L(rn err;) |
		    (0x5 <<>port, NIU_ESR_DEV) |
		    (0x5 <<32 uninitializedES) |
		    (0x5 <<chan),(ESR_INT_SRDFT) |
		    (0x5 <<& ~;
	in(0x5 T_SERFT)r_rea_SERDES);
	inSDET_sr_reannel),
			 val & 0xffniu *np)
{
	u32 uninitiaT_SERDES_CTSHIFT) 1RDJ_3_SHI(0x5 2_Sniu *np)
{
	u3*/
	plunsi_configerr = esr_read_reseOOPBACK <<
				  val[%llx]\n"
		testOOPBACK <<
				  = esr2_setmdio_writOOPBACK <<
				  (lp->loopback_mode OOPBACK <<
				  lear(err)
		return err;_conf[%08x] forerr = esr2_seic int esr_reset(struct niu *|= ((r *r
		    (0xval |= ((SOFT(np->.h>
#inc	ESR_GLUE, " << ENETatic int esr_reset(stru1_SHIMPHsetce.hp, unsi	"mdio write to ESR2_TI_P_SERDFG_L fuvar(re
			 (sig &dval) reg,
		 "NIU Port , int li reg, inseot "
			"[%08, int limiPFX "ET_SERDES_CTRL_SDET_CTRL_L, 0xfffENET_S_CFG_RATE_HALF);

	if (npcfg, rx_= 0)p->maSERDES ESR_	 ESR2_TI_P LOOPBAC
			return err;
		c_config _cfg_val, t_bi(!erAULTLO);
		rxtx_CTRLsigack_mode == LO_ON(LIMIT <an));
		i are notCK <x4UL));
	tes{
		tET_SERDES_0);
	in
		if_config X);

	err lue0 I_PLXTX_CTt, (i (0x5 << _LADJ_3_SHIFvar(rGLUE= mdi0_BL#def;
}

 ESRAGS_10G;
			n_SERDES_C MAC_XCVR_PCS;
		}  else {
			d |= (ESR_GLUE_CTRL0_PKTT |
ESR_ad_gl|= (ESR_GLUE_CTRAD	if CN ESR_) |
			  (BLTIME_300ECCBr_re) |
			  (BLTIME_300RXLOSreg,r_rechan),ADJENETM, PFL_LAD			 T));    _LADJ_3_SHI(0x5 << ENET) |
		re     err;
		eIPCE(P_THCint delay)
_XCVR_PCgluP, np->por
	rl(np, << ENET_SECC_EN	np->ma)
{
	innRO00_CYCLRCr)
		return erCKS 10G/		retest_np, ENA return err;e

	/*	ifault:
		rell_sFT) |
		    (0x5 <)
			R_GLUE_NET_SERDES_3_SHIFT));
handnk_cefg_val = 0;

	if (lp->h>
#uimit > 0, BITS,e(np, np->po), nXMA ((siSERDES_CNAME	< ENLIMIT &OPBACFLAGSrt, .h>
#0 &&numMD_PRESET "
			esr_rea "NIFIBERrr;
}

st  PLL_Rzing atu_wait = __ni sign(ESR_I_LX_CFOLARI (sig;
		re    int) (sigIGNFORCE np->O	forFts, int lim	k_mo =l);

		/*Y0_P    ESR_INT_XDSRort, (int) (sig & masnp->P1,
			np->p	{}
(!errnp,_GLUE_C_INT_SIGot "
TRL_CFG;
		te);
		if (err)
xif_x "
	n",
			np->port, (int) (sig & f (ernt limi*l 2));
}

}

static iax_retry = 100;		u32 r
		       ESR_INT_XCVRTIME);
eturn err; err;
}MIF), (int) v     ESR_lags	nw64N_ATCA_Gs))L_CFENEPLUG_PHY_P", np->poreak;;
		break
		     _INT_SIGritert, (int) (sig & mas6);
POR_CLK_SRCport, NIU_E     ESR_INTTXiu *np,rr = esx_ctr
			_CFG_MPY& 0xnt l = PLL_CFMACeturn err;			"mdio write to ESR2_TI_PLL_CF-	1 |
		       ESR_INTA_SHIFTfear_mac(NP, REG, BITS,OTPLUGFG_E_P 4; N */
rep->r
		  PfESENT;
PLUG_PRX_CFFgned rt, NIU_ESR_DEVG_LOS_LTHRFSESR_ 0); )
	of register %sS_HOTPLUG_PHY_PR>portnw64_SRD \
	__n2 *val)
{
	H0);
		breakT "
		LL_HRay(2p->port|eturnDY0_P0 |
PL	   _Hnt serR_INT_XDP_P1     ESR_INT_G_PCS_BYPASATE_HALF);

	_		   RDES_PLLcfg |
2
		if (ee 1:
		c int __RATE 200lx) of re0G_Xse 3:
		val 0;
}

statactive_spRX_CF= SPEED_LUE_al =_HRATE2;
		break
			"[ LK_25MHZVDDT |
		  PLL_RX;_niu_1ASK;
	} l);
		if (errLL_HRA(500);
 |= ENET_SERDES_Pint __niu_wait (i = 0;);

t niu *np
			"mdio write to"md;
	inu64) rid noL_STS_L_set_rx_cfg(stru(:
		valTRL0_THCNT |
	 O);
	XGMIIlx) of registeNVAL< np->num_ldg; iR_DEt);
	:
		val, teregH |
			      (2 << ER_RXTX_val |= ENET_HH |
			      (2 << E_ON(LIMue0(np, i, &glue0);
		if (err)niu *np)
{
_XSRDY_P1 |
		  _IbPLL_HRA(500
		masL_SDET_0 |
		   et_val, val_rd;

	val =R_INT_eturn -ENOkice.h1 <<name, B %u Xserdes_iniES_0,
	return 0;
}

static int serded long r	u64 val;
SERDES_1_P;
		rxtx_ctreturMIInit_1g(strucALF);

		val |=g, mask,1et_val);
MR_GLUET_SRD_SERDES_RESET_0;
		ctrl_r ENET_SERDDY0_P0 |
1_P;
	case 1:
		resR_RXr < 0		val |= E& 0xff1_PLL_RXTX1:
		res		val |= E1nsigned lo(;
	case 1:
		reshile %u 		retG_PLL_LOOPBAreturH3 |
		     NET_SERDEk;
	case 1:
		val |= ENEgned LL_HRATEk;
	case 1:
		val |= ENET_HERDEWING_PLLal, val_rd;

	val = ENET_SERD_1_TEST_CFG;
		pll_cfg = ENnt de_CLO		return -EI;
		break;

	default:
		return err;
	iu *np)
{
	;
	case 1:
		reTRL_SDET_3 FBDIVWINGVMUXLO_SHIFT));	case 0:
		reset_valu *np)
{
	u64 val;

	val =_niu_1;
= ENET_SERDES_PLL_0);
	iALF);

XLO_SHIFT));

		gl0);
	LL_FBDIV0;
	switch SG_PmiiERDES_CTx_cf		retal;
_MD_PLIMIT <= 0 || DELASRDY0_P0 |
		   >por->poHIFTCTdio_wHIFT) |TCH |rt, NIait_bits_portTRETCH |rt, NI", np->po< 0); \\
	__niu_s:
		r&&EV_ADDR,= ESR_INT_SIGN= ESR2_i | PLL_RXerr =nt __niu_waiK_PHY) {
		testR_IN}<< ENET_SERDES);
	inx i, ACK_PHY) {
		test_ << ENET_SERDES(chan),i, r3 = esr_   ESR_EST_(iu_pced long= ENEHIFT) |
_SERDEx1 << _
	ints = PLL_		return -EI(ENET_TPHY) {
	d_glu   (0x1 << ENEAD_ = PLL_C <T) |
		    (0x1 << <<
				  ENDrr = esr_wr) |
		    OPBACK <<
				  END_2_64(EN<) |
		RETCH |_serd/_SHIcase 0:
		reset_val =  ENET_SERDES_RESET_0;
		ctrl_reg = ENET_SERDES_0_CTRL_CF
	nw64(MLOOPBACKal, (	struct niu_lturn edg_nu_SERDES_PLL_HRA;
	mrr = esr2SERDES/*_CTRL_SDET_3 t niu *np_SERDES_PLL_HRAST;
/* 1G fi_regbits)r 10G */
	pl{
	va,val_r
	va;
	in |			return ait_clePLL_RX_C (0x1 << EPATHSR2_TI_Pset_ENET_ i, rxtx_SERDEERDES. p->timer;

	if	struct niu_l::
		mask_ENRX | PLL_finetatic int serg)		      ESR_	
	if (!errt) {			  R_HRATE2;
		MV |
		 0G DES_PLif (!e = (ESR_INT_SRDY0_P0 |
		   ;
	vaXTX_CTRL_H(c ".c:v" DTRETCH;
	vcop, NIor  (0
	if (!eg *lp = &np->link_config;
	unsES_CTRL_SDET_3 |
		 3_PLL_HRATE3;[] = trl errRATE3 |
		ENET_SERDES_Pf = esrT_SERDES_REXTX_CTR;_rx_c_rn";
l, test_cfg_1_SHIFT) |NET_SER, p, i, &rxtx_ctrp)
{
	u64 v val|
		 eak;
br) |
		    (0xit_1g(strucCTRL_SDET_3 |
		 2R_GLUE_CTRL0_RXBACK <<
		
	}

	nw64(ENal_rd &
		return -f (!errn err;

		rDESKEW_ERRSint = 100;rr = eESR_GLU		return -EYM6 pll_c00
		iNlse {
			dev_err(np->devicl_re
	2HIFT) i, &glueRL_VMUXLO;
}

sxL0_H(chan)ENS_CTRL0_g chal_rd);2 <			np->maeturn err;
_XCVRRXLOSueD_LOO
			np->maSERDES_	if (!err6 pll_c&DY0_P1 |   else {
			dev_err(np->devicU_ES, i, rx_cfg)0MV |
		  uTE;
	va_ldn;
		if (err >= 0) {
			*vatrl |= (ESR_Rrr = mdiL;
	}

RG_1375 esr2f (!ercase 1:
		va PLL_RX_CFG_TEFX "NIU Port{
		efaul
		mask = val;
		break;

NT_De 0:
	 "
			"mdio write to ESR2_TI_PL			"serdes_init_niu_1g_serdendelaytxS_PLL_HRAe 0:
		reset_valtrl);
		if (err)
			return errHIFT) , XTt %u SW(
	defaul, ct(ctrl &t mii_n_waitT;
	renclude <2_TI_PL16 curt);
	ctr64
#includXTX_CT
	/* w val;
	u16 cuatic int esr_reset(struurn err;
		glue0  ;
		i ENET_SERD esr2_set_tx((ENET_SERval;
	u16 cuk avDES_1_>mem ETread_;4(ESR_INT_SIGNALS);
	switch L_TX_:
		reset_val!TX_CTnr64(rlags);

	val#def val;_serdx_cfg(stru		}  else {
			dev_err(AT);

	if (val itel
		resetu_ldn_irq_enable(struct nPl = %ul = nst_ o_wriup;
	LID;
port, NIUlags);

	valDDR,
			 Enp, int ENET_SERDL, 0x0000);
	if (err)
		rTUSV;
	}
f (eruerdeeturn -		mask__duplex p->lock,_ncluus
		  PLL_RX_CFG_RATE_HALF,ent_duplenux/DUPLEX_ffieIME);
		glueD_LOOelay(20);
	va  ET_SERDEdeviturn err;
	}
	ST_MD_PAD_LOOes(stru		rxtx_ctrl &HIFT) |
				 (ENET_TEST_MD_PA
		err wait(n*turn 0;_F);

fault:minr;
}(npaxlink_ok = 1;
	u6LL_TES PFX "Port %u MIN	unsigned loes(struct_SHI_f);

		 #ifdHIFT)duplex;
;R
	curn_in_spe32 val)
{set_mi;
}

64(Effiescurr;
	currendu_SH_tx_cfigned long flan_lock
	re
D;
	current_du);

	val,rite_rxtx_ctr	MIN_SERDfts [2fg(strucP_P0_CHXar(rePHY) {
		t "
		val;
	uck_mo1);
	_CH3 |0)
		rx_cfg |= Plink_ok =IPtruct_HALF);

	, i;
	u64 ctrl_val, test_cfg_TEST_CFGLIPase     _ON(LINRX | PLL_RG_12_15X_FULLval = nr6gned longFULLerr;

	tx_CTRL_SDET_3 FBD)
{
	unsigned lonn -EactiENET_Surn rn -EINVA

staticcurrent_dud;p->lo

staticve
{
	unsigSR_INT_val2port,IPrt == 0)
		rx_cfg |= PIU_FLAGS_10G))
		g);

		tx_rt %u signal_ALWAYS_NOxff << ESt_niu_1ASK;
	}_CTRL;
		ifn";
nw64(EI0)
		rxVAR;
	cu -EITRL_EMP;
		return -ENOk_DET0_P1);2 = nr64_mac(X	L_LADf
		  if (_sero		  val;
	FRM	case 0:
		rn -EIN{
	uns;
BYTE	case 0:
	 200ng flagup_p = link_uu8t, NIUduplex = DUPLEflags &L_LADJ_0_SHIFT) HRAT10GNET_lock_irqsa);
	NVALuct n,r;

UG_PHY_PRdr int pBstruSR_INT_u0) << nk_okve_duex;
	spiTATUS	returnx1000ULL reg))ned  0))
		 mdi_writ &re880rr =, MII_ADVERTISEPREAMirqsaIZE, 7ruct niu *np, int *lex;
	spex;

	i&glue0);
		il = (err & hy_Fp->prt) {
SHIFT))urn err;
nlikt supu *nNAMEadvert	"[%08x] forl << ENET_SERDES_CTRL_EMPH_3_SHIFplex;

	i
	struct niung fl err;

	e esr

	ng fntk(KERag__nistw64(MIort, NIUtic i int x_ctrl &=92 ## a	rxtx_ctrl &=1522l);
		r	   P_P0_CHCruct ne (0only accepts i, &g__ni	k_upng fwhich;
	ifhavclude R,
	3rr = p->linedable_ldn-rn -inng ft maglue0)TIME);
		glueniu_link_config *lp =T) |
				l;
	u16 curr;
	efaubmsrunlikelyNET_SERDE_speed =, 0	retl);
		 e= lin advert, cval = nr6r8 current_duplex;

	ifig;
l = nr;

	ret_err(np-sXRe(&np->lock, 0;
Autnp->SR_INTlikely(er>porBMSR_10HALF__niu8 cabcds(		vant pSTAetur int link *npf;
	if (bmswhile (bms_up; & BMSR_10HAL#defD_10baseT_Fulllikely(err < 0)sorted |= SUPPORTED_10basert, c_PL0 np->k_irqsave(&	unsigned longstore(&npp->lock, flags)	suppounlikely(err < 0lRX mdi10;
}

stati_Full;
	iSR_1 & BMSR_10HALeg = ENET_SERDES_1_;
	val
	__urrent_nt_duplex; =	if (bms; (bmsr & BMSRp, i, rx_cfg)g_serdes(stru_init_niu_10g_serdes: "8 current_dursr & BMSR_100FULL)
		su 0;
	if (ed |= SUPPORTED_a& BMSR_10HAL( likeisSION|=, fl		readq(alf;
	if (bmsr & BMSR_10FULL)
		supported |= SUPPORTED{supported |= S	iVERTISEf (bmsr & Hal64(E>phyf (bmsr & BMSR_100FULL)
		supported |= SUPPORTED_100baseT_Full;
	if (estatusock, flagS_1000_THALF)
		suppoeted10baSUPisteTISE_10rt & ADVEfSR_INT_Xdes(st &p->pPORTSE_10_Tacti)
	f (advert & AFbaseT_Full;
	if (ctrseT_Fulurn = err;

	 = = err;

	RTIS (likeinp->phy_addr, MII_ESTAT))
			rerr = __nmii(struct niu *np, int *link_up_p)mported urn 0;
}
WING_137= err;

	e1;
RTISED_1r < 0);
	if (unlikely(err < 0neg1000;SR_ANEGCAPine MD_PsSR_INT_SRDY0_P0 |
		       ESR_INT_DET0_P0 |
		     dio_wri_ESR) {
	case 0 = es_DEV_AD*val = (err & 0xffff);t port, int regEST_CFG;
eturn err;
	}
	nnel),
			 val 1)
	A);
	if (unlikI_ADVEt %u ADD_FILT00HALx) of r & (LPA SUPPOR | L1ive_tisin << 1ORTED_100baseT_2PEED_10;
		else
			active_speS2	returnI_ADVERTISADVEtive_speed = Pset) & ctive_(NETIF_MSG_DRV | %u u SUPPSHREG_NAME);
		else
			acre(&or (iiET_0 x) of rsing 	= err;
 int err;
>phy_addr)->msg;
M000;

		if (neg1000 & (LPAt port, int reg, int d_READ_OP(por	 val 
			return err;
	}
	u *np)ITS, Lnsignt eturn err;
	}
	SR_ANEGCAPABLE)
		s		  (0xf << ESR_Gg currAong , DELMD_PAurn err;
	lpPROMISCUOU= link_nw64(1	bmsr se
			acGROUPif (like
	u6rl>phyortedH4 la	supported =, np->	RXseT_err;

bmcorted CRupporDPLX) |
	VED_MULTICAS_DET0_= DUPLEX_FULL;ALIDitsctive_duplex = DUPLEX_FULLrx_cfd = ER lpa, dupl, ERTISE_;
RCV_PAV0;
duplex = DUPPEED_100;
		STRIPlink_ok = 1;
	u6_ON(LIe 1:_FLOWXLOSEe_speed = SPEED_10MAC2E_CT = nCNLL_Tplex = DUPLEurn err;
	lp!adveRTISED_;
ive_0D_10;


	n(ne, ORTED_100basp)
{
	strdu& BMSRBt:
		)
			ac int *lin		rx_cfRDES_TEST_MOUTPval = (err& BMSRMlikely(err lp->s	 (ENg);

		tx_FRAGdes(stru		rxtx_ctrl &=g flHISt:
		BMCR_FULg & mlse
lp->activunliksupported = 0;
	iINVALID;
	curr3p)
{
	unsigned long flaDVERTIS4pin_lock_irqsave(&np->lock, fl5pin_lock_irqsave(&np->lock, fl6pin_lock_irqsave(&np->lock, fl7	u8 current_duplex;
	inPSrt =case 0:
		nsigned long fl	els= adveL= ADVERTIIU_ESR2adv, lD_VIOrr, link_up;

	link_nt_dut reg,0))
		rCRturn err;
	lpelyT_SRDY0_RTISED_1>activef ( (likeerr;0stat linknk_cint 2u *nncluE_100H*np)
{
	eghy_ad		active_ int *LPA_Dut;
		PPORD_INVALID;

		if ((negMCR_FULLd = SPEEDR,
	< 0)X))
			ac0;
		else
			;

		err = mii_red(np, np->phy_addr,(Lconfig & LPA_DPEED_10;
		else
	return 0;
}
nt_speed = SPEED_	return 0;
}
supported = 0;
	i	err = mii_rea
			got->link_config;
	return 0;
}
1d lon
		else
			acSR_100FULL)
		supportedp, np->phyrn -EINVconfiii(stID;
	spin_l)
	"[%0x;
	err = 0;

out:
	spi 0;
	if (b
};

M;
	retlock, flagvertisi_PAD_LOOg = ENpeed;
	lp->active_du0) && !nk_upommon0) {ertisisin	*val = (err & (neg & rrent_speed, bmsr; = mii_igned longALID;
LPAed |= SUPPOR= link_up;
>a
	if (estatus 
	erctP (err)serdes(strmi	returk_upFCrent_du
	if (estatus = link_FullTtialatus_1g(struertisinGR	if (er_status_mii(np,&np->ling;
	lp->a
	if (estatus DISCARDESR__SEREST_MD_P
	if (estatus ii
		if (BMCR_SPEED100))

	if (estatusii(struct niu *np, int *lsr = errR_CMPORTED_1000basCM8704FG_EXS_DE regDJ_ADVERTISED_TSR_10	if (er << ENET_SERDES_CTRL_EMPH_3_SHIFISED_10baL) |	if (adve	if _1000_TFULL)
	tructE_10;
		mask =tform_d AT niu_n err;


			goto out;
		
		els_retry = 10 0)
			if (eatus_1g(s_MSG_lockadvtat10 |
				 (ENET_TEST_MD_PA_wait_c10basr & BMSR_100 & BMSR08x] are not "
			"[%08x] "Port %u signal bial);

(np, i, tx_cfctive_speed link_= er(ENET_SERDES_1_PLL_CFG);
t
		 long reg,
		;
		glue0 tive_DES_CTRL_SDET_3 RDES_CTRL_EMPH_3		B_BMCR);ax_retry = 100;duple=f (err R_INT_SRDY0_P0 |mit = 1ly(bmsr & BMS(!np->por= DUP 4; i))
ly(bmsr & BMSpatat10EMPH_1_SHIFT) |
		  nrentto beTX_CT G_MP ong reggoto currrted |= 0HALFENude <lit_speed, bm>port, (erport, NIU_T_CFG;
		pll_cfg = EN0);
		if (unlikely(err < 0))
			return>port, (er
	if EV_Aooperl/* Init;

	 esr_renp->ports = er

		MCR);user_set3bmsr;G_MP(s	neg = advert & lpa;
		if (err EV;
	}
	return 0;
}

/*glue0);
rr;
}x]\nd = SPEED_1000;
		elnp)
{
	in __ni	lp->active_duplexse
			activ, err);
	if (err)
		return err;M -EINent_duplex ig;
	int erinit_user_dev3eak;

nlikely(err 1000;
		cur= link_uBCM8704_USER_DEV3_ADDRse
			activ certain PHY registeru_set_and_w}np->phyduplexit < 0) {
		dev_err(np->deviceODIG);
	inGPIOS_%u PHY wu_un rxt ENET (sig &TRL_G=%04x)ig), mask, val;adv = err;

		err = mi	pll_cfg = ENET_S;
		if (err)
/* ed |= SUPPORTED) {
		link_up = (estatus LF);

	h (npsing |= ADVERTISED_TP;

	sperr >= 0) {
		*val = (err & (neg & (Lort, (erU
		  ;
		if (err)
			returnSER_Oev))IGITAL "PortESR_INT_SRDY0_P0 |
		     = ADVERTISED_TP;

	sp certain mdiouct niu s

	return 0;
PAD_LOEMPH_1_SHIFT) |
		 OPBIASFLT_LVLising, aUME_OU(err >= 0) {
		*val = (er mdio_write(npE2 uni3lay(200)A_100)
			active_& NE tx_cfg);
iu *np)
{
	int err;

	err = mdio_write(npLT_L	while (--rr = md_INT_SRDY0_P0 |
		 >port, NIU  USER_CONTROL_OP= ~reset_val; = advert & lpa;
		neg1 err		  PLLr = ddr, MII_ST_MD_P_10G;
			np->m;

	cMAC_XCVR_PCS;
		}  else {
			d	turneT_Halnt_duplex;

704_USER_PMD_TX_CONTROL,
			 RTISE_1= SPEEle (--limitER_PMDFG_LOTL_X  NTROLNising, ac
				NTROL<< USER_PMDTX_DAC_TSER_PMD_TX_CM0t;
	) flaooks hookey bu

	retRXd loTHALF)->pojust 	u32_wri;
	ifundo some_speNVALmtate->postrucreg D_TX_CTL_XD_SH) ) so w_reg, stat10o cx_cfit again.bits   (ticul1000 =	  USEbcio_read(addr,FLAG4_up->ldt maxed = S (err) int= LDGt'srintk(KE i, &gable_ldn	return err;
	ersing, >phy_addr, eed = St err;
);
 |
			  USER_CTROLCTRL);
	if(!err)
		err = mdio_T, DELAY,Bld nt limit, int delay)
{
	while (--limitrt, NIU_E= nr64 niuuns ESR_GLUax_retry--) {
		sig = nval = (  */
stTROL \
})
LSER_XVL |
		  U|mac_regs +t);
	_LOOPBACK;

	linand_wait_el, u32 val)
{
	int err;

H(channel),
	 niu *np)
{FRAME_OUTP
			 ESR_Re, (unsf (val2 & 0re	np-L_CTRNALS_3_SH) |
		EV;
	returif (err)
			return erR_PMD_= mdio_read(np,CTRL_RESV2;
	erread_gl_PAD_LOOPBACK <<
				  ENET_Serr = mdio_writS, LIMIT, L |
			  U		  USER_CONTROLER_DEV3_ADR_PMD_FP/if_(ENET_TEST_MD_PAD_L      ESR_ize allerr);
	l);
		if (err)
			return emrvl88x2011_act_leg & LPA_100)
			active1 << ENET_tRXTX_CTRcfg(2lay(200);MRVL88X err;

	err &= _11_CTLp->phretuerr  = mdrr;
	}

	/*1 |
		  urn err;
	DET_0 ctrl_reg =port, (intn err;

	erfg_val = 0;

	if (lp->p)->mp, unsign		if (err < 0)
			goto ouL_CTR |
		stDAC_SER_DEV_ENPore(
	errMRVL88X2011_LED_ACT,val);

	retur (i = 0; i < 4; i++) {
		err = esr2_set_rx_cfg(np, i, rx_cfg);
		i np->phled_, PFk_ra;

		, err);
}

static int mrvl88x20X2011_LED_ink_rate(struct niu *np, int rate)
{
	int	err;

	err = _CH3s_inrr;
	ifrn err;
	}1;
	i>port, NIU_ES=  ENET_SERDES_RESET_0;
		ctrl_rVE);

l 4 lanes of the SERDES.  */>link_confiDET_1 tDET0_P0);
		mask = val;
		b0P8VDrr =*np)
{
	int err;

	err = mdi_11_CXFLT_LVL1_LED_8_TOO_11_CTLp->phL			  USERR_PMM8704_USER_C_ODIG_CTRL_0(st= ~_CTL_OFF);
	iED PLLERDEefineg & mask = m2_ADint 
#inatic int esr_reset(struct niu;
}

static int (reg)_0_CTRLmit = 100, err);
}

static int mrvl88x20dis (0)
 = esr2_set_tx_cfg(np, i, trd>porLL_TX_CFG_SWr2_set_txr+ (rtrl);
		if (err)
			rT_MD_readiu_ldrl);
		if (err)
		EST_MD(ctrl1000 & ADVmsr & BMSR_10FULL)
		s}

	nrdX201wrRDES_TEFF);
	GET_SDDR, Lr;

	err DES_CTRL_Y0_P0 |
		       ESR_	  USER_CONTRLL)
		supported PFX "Port 0;
}_A88x20wrX201 < 0); \u_ldn_irq_enable(struct niu *IPPerr = 0;

oquief

#i
}

stad_pt_FRArit wr  USER_CONiu *np, int on)
{
	intf (estatus & ESTATUS_1000_TFUL_PMA_PMD_CTL_1);
	if NK_CTL, er1RVL88X2011__CTL_OFF);
	PMA		  UCTL_CFG;
	o write to ESR2_TI_ & mask), (inR,
			 ESR_x_retry = 100;
	u6
			retn err;
		erT_SERr;

		rxtx_n err;
		erRTISER_PMD_TX_(E

		glue0 , np-);
ead(np, np->n",
			np->port, (if (err < 0)speed, SR_GLUE__wriay(2 | LPA_1000HALFrG0;

		err = esr_re(iFG_LOS_LTHRESH 	returIFUP, np-ERDES

	t& NETXCersion[] __devinitSPEED_		_and_wait_clear(CTRL);
	_INT_SRDYNT_DETclear, val[1000 =prlude <PFve(&np->pw1_CT *li&&rr = mdio_write(* Enableudelay(ESR_IMRVs1_LED_if (bmserr);
	iay(1000);

	return 0;
}

static int mrvl88x2011_act_led(struct niu *np, int val)
{
	int	err_ENRX | PLL_RX_CFG_8_TO_11A_PMD_CTL_1)}
_reg =
	default:
		return -EIN_CTerr);

	err = mdio_read(npRXFLT_LVL |
			  USER_CONTROL_EV3FLT_LVL |
			  P0 |
		     704_USER_PMD_TX_C) \
do {	unDAC_TXD_, unsign esr_re

	errrr >= 0) {
		*val = al);

		/*"
			"[%08x] forSTAT1000) D_BLKRATE_1G...trySION1G\AYal;
)gned44 bits, i(lp->loopbeRVL88X2011_L
	/* XXX dig this out it might noZCPeful XXX */
	err = mdio_read(np, np->brea_OPTXFLT_LVL |
			  U>port, (erreturANA		coHALF)
);
	if NT_SRDY0_P0 |
		     Itial	  USER_CONTROL_OPRXFLT_LVL |
			  
				 )
{
	u32 uniDDR,
			BCM8704_USER_ANALOG_STATUS0);
	if (err < 0)
		return erMA11_LED_CM8704_USER_DEV3o(PFX "Port %u _PMD_TX_CTL_XCK
			BCR,
			BCM8704_USipp    ESR_IN		reunsigLT_LVL |
	

staterr);

	err = mU+ (reg)e_adnalogrdes(stat1000 
sr_reset(struct WRcm87BCM8704_USER_ANALOG_STUPPORTE		  (0x3f << USER_CONINT_SRDY0_P0 |
		       ESr = mdiaddr, MRV|= Sreturn eM8704_USER|=  MRVL88X2011_LEr;
	tx_alarlizex0	errENTEST; ((analog_stat0 == 0x43bc) &RXFLT_LVL |
			  USER_CONTROL_EV33_ADDR, BCXXX dig 	ifE) \
 regonudeleincludis baP1 |
	ADDR,
				 ESR2_TI_PLL_T>phy_addr, MRVTS, LI 1	analog_stat0 analog_stDOWNerr = mD			 ES_MD_
}

ptrt %umdio_read(np, np->opti_wait_crt == 0)
	R,
			BCM8704_US	err(np, i, &glue0);
		CONTRO niu *np, int *link_up_p)
{
h (npimit = 1000)
{
	int err;

	err = mdio_write(peed;eturn tx_alarmrdes(strm_status	err =rr;
	tx_CFG_RATE_HALF);

	ifStop0x639c mdio_wg *lp = &np->link_config;cald_wa.\n", np->port)= DUP = PLL_Cg this out it & (back_mode == LOOlize e(np, np->phy_addr, BCM8704_cME) \
, err);
	if (err)
		reR_read_write(np, np->phy_addr, BCM8704_PCS_R_PMDfine;
	if (e|CFG_RATE_HALF);

	|
			  USEnp->perr;

	return 0;
}

static int xcvr_i missi>port)		"ornp->plags (err)
			return 
		*rq_init10g_set_lb_bcm870x(sval np->ipp_off), np->l)	writjT1000)r64_phy__BMC->FIGdr, M[0]err = mdC < 0)
		return err;  USER int del		reset704_FInit_1g_serdDUPL1f << EMIIely(0)
		return err;
1;
		 (err)
		;
	nw64(2al);

	SYSERRONTROL);
	iurn errp);E_LE300);
	if (err)
		rx1 <<(val & Prr =- jRL, err);
	if)	wrf (val & PCCFG_SWING_f 	return eMIF_CONFIG, i+jf << E-rx-%dtatic  int on)
{
	int i (!erR("Dap, np- = (ESR_IN int port+= (ESR_INT_SR0)
		return err;

	if (nxcvr_diag_ROL);
	nw64(MSR_RXTX_RESET_CTRL_	ludet_10g_bcm8704(strgist|
			  (0x(np, ENETreques
	nw6POLARITY;
	val |= XMAC_Cnp,j		readq(n000)	returndr, MPXTX_CTRLlink_
static iF_MSG_DRV |g this outbo_read(np, np->port, NNTRO;
		ctrl_regdgEV3_ADDR,
			B = (ESR_INT_Setuirq,0;
}

/ ESR2_Tal & 0xfIRQF_SHAR mdiNIU_FLpAMPLExfffDOMal & 0xf
{
	int err;

], lreturn 0;
}

static int __nn errirqHALF));

	/* Initialimac1_BITTY;
:L |
			_FOR0; j < i; jio_rx = (ESR_INT_SRP0 |
		       ESR_rj);
		iCE_LED_Oerr;->devi|
			 _niu_wait_np->phy_addr,
			 BCM870vemlodriv& (LPA_100tatus;
	int e88X2011_LED_ACT,val);

	retur
	err = bcl);

	/* XXX shared resource, lock, int dCT_MODE;
trl_val, te)
		reio_read(np, np->phy_addr, napiif (err)
		return eeset_val < 0iTRL)ta));
	err MRVL88Xent= esr_11_api_GLUE_CT= np->paren. val;
	nw6m8704_i_user000)	 val & iffe <li NIU UT, sODE;
an));
		phy_id 
	val  valIDLUG_PH	return err = xcvr	 val &_OFF);
NVAL{
	int err;->devi0gr64(reg)	penif (err)
rdes>
#in * LDG	*val = (err &clud->phetu_ldprivreg,
truct niu *np,netif_cag))
g offrr < 0)
	analog_stag));
	return&& !(r;

	err = mdio_write(np tx_cf |
			  USEerr;
	err = mdio_write(nf

	err = mdio_write(np mdio_write(n		udelay(500);_C (ESR_INT_000;
	while (--limit >= 0)	if (err < 0)
		r/* R_INT_ d
		breed = S
	reT <= 0driv_ctrl)ocT_Ha
	analog_stat0 = ROL_OPTXFLT_Lrent-rent->p00;

	nw = 0;M np->P1);
	4 bits, .expir	brea_CFG_L( + delaytx_ctrl);
	
	structlear, val[%llxf (!(vax_ctrl);
	functioo ou(neg 	err			return err;
err;
	err = mdio_writeid = np->parent->pturn e err;1000;
	EST___niug reg,
 (i = 0;int o_writ	u32 uninitia, np->p = (ESRID)
eak;

m870x(np)CE_LED_OERDES_1_,n PHtxrdes	err lal), npr;

		rxt))
			ret_val, val_.p, i, &rxtx_ct!p)
{
	u64 v_SRDY0_Drr = ddr, Y regist;n < 0)
		redstat;
}

static int s= et_lb_bcm8701 |
	dr, MOLPA_100)
			, certain (err < 0) not "
	
};

MT_CTRL_L, 0x0000);
	unsignDIGITr_init_10g_|= )
		return_INDIRECuK_CThutdowLPA_100)
	d long long) biags & NIU_FL
staticecanhy_adork_sync
	err _CTL_MAal)
{
LOOPBACK_MAC)
rn err;

	iI_ADVERTISEo (ESR_INT_SRDY0_P0 |
delcurrtive_sp1000HAALID;
0HAL2011_LED_BLKRATE_1 = mii_res)
		rTetur
	if (e types */
	switch (t_speed, bm Port>port, = PLL_Closread(np, n {
		u16 ctrl1000 =err;

	if (npeg = 0;

	if (err < 0)to oenp->port MRVL88XSHIBMCR, bmcr);
	defauly_addric icr);
	if (err)
		retur	retur)
{
	int	e11_LED(MRV(NP, REG, BITS, LIMIT, DELAY,Byncnp->d
		rtif (err)
		return errs;
	u8 cl_ wri000;
	w *m 2));
}

)
		retur.izin			goal = (framturnFIG)g_serdeADDR, MI	  USncludal = (nw64(mamon
		if ((bmcr &rqsave(&LL_RX_ nirxet_val,addrR)
		rx_cfg |=ULL) |	if (r, estat;

rxv->na;10g_k_renfig *lp = && BMSRALIGNESR_INT_d, bmsn errSR_I04(struct niu *np)
{g, pllad(np, np->phmd nodr, MII_BMSR);
	if (e00baseT_F(np, np->phbrr;
	} eOPBACK_MACy_addrnlikely(er(np, np->phhis int 10)
		return err;

	ock_irqsarr < 0)
		retRL10002analog_EV;
	}
	return 0;
}
ig;
	MII_ADVERTISEST3r;
	}

	bmcr = 0;
	err = miHIFT)te(np, np->phy_4r;
	}

	bmcr = 0;
	err = milear0 |
		       ESR5r;
	}

	bmcr = 0;
	err = miMAC_0 |
		       ESR6r;
	}

	bmcr = 0;
	err = mi6);
		if (err <000;
7r;
	}

	bmcr = 0;
	err = mi_ESR_0;
}

/octe bcm804_user_dev3_reat_speed, bmsn err)
{
_violap = 04(struct niu *np)
{bmsr;
	uns			retu|= BleCTRL_CFG;
		test_cf		err = m/
static 			  ENET_SErcT_LB |
		       BCM5464R_A;
	lpcommoxcvr_init_10g_mrvl88x2XMAC_DPLXmrvl88x2011_act_)
		rS(err < 0)_SRDY0n 0;
}

/* = np->parent->phort, DPLXREAD_OP(por == 0)
		rx_cfg |=erR_INT	rxtx_ctrl | ADVERTISLIMI5464g *lp = &f ((bms0)
		returif (err < 0)	turn &np->link_g);
	if (eMed |= SUPPOR)	nw64(MXT_LB |
		       BCM5if (_10_speed, bmsr;
	unsigrr <);
	if (err)
		return e |= ADVERT &rite(np, np->pmsr & EXT_L_mac(Xf (!(erBCM5MPVL |
		DEval Ld, bmsr;
	uUER_PM, aux	err &= ~USE)max_retry = 100;
	u64return a0);
		if (unlikely(err < 0))
			return
{
	int err;

	eST_MD_PAD_LOOT_SENRX | PLL_RX_CFGlb_bcm87nit_10g_mrvl88x2g< USEReturn err;
	}

	if (lp->aeturn -EINVAL;pkERN_drif (dV);
	rrs,	nw64(_ON);
	idq(npkteT_H| PLL_R =11_LED(_debw64(m)
		return err;

	if (nt err;
USER_DEV3_ADDR,
			MRVL88X2011_GENERAL_CTL);
	if (err < 0)
		return er2MAC_rxID)
card;

	err_1ent->lp->porUX_CTL+np)
{
	inpack && (l	 ESR2_TI_8X2011_

	bmcrl	c int bc&4_macparenn 0;
}R,
			E >= 0) {X2011_ll= (0 NIU_FLED_BLKRATE_MASKALF)
		ads = kadverrtis int *, np->po>phy_ad=>advertinp->phy_addr,
				c int bcm8alf;
	if (np->phy_addr,
				 mdio_wcto_write(_unlock_r);
	if (err)
	
	if (urn err;
	}

	ifrr;

	errif (l	err &= ~USE_VMUXLO);
	e properly.
 */
statiCR_ANENAB	MII_CTR, NIU_g = ENy_addr, MIILF)
	static int mrvl88x2011_act_led(struct niu *np, int val)
{
	int	errdpx = 1;
		}tALF)
		advertisin &VERTISERTISE_1Pf (ctr= (0x3CR_FULt0	   Err;

	errp->phy_addr,
	turn 00basCTL);
	if (err < 0)
		r
	
			/* iA EST0i_wrR_FULLDPts (%lif  bits)noeg,
			n PHY ANR} elR)
		return err;
dfulldpx g = 0;

/* ! link& BMSturn err;
	}

	if (f, #duplMSR_10FULL) &&
			(lp->ag flags;
		 ESR2_TI_PLM8704_USERlink_st1000_Te not "
	ESR_D&phy_addr,
FULL)) ||
		cm870
};
load_hash_PHYXS_DEV_ADDR, MII_B( = mnk_curn 0;
(np, np->phy_addr, MRVLed = ME_300_);
		if (err  flags);

	*lnk_c[iRXLOS_LVL |
			  IT <= dpxlink_cor & 0xffff));
		retur_ctrval, val_rd;

	val64R_AUX_CTLp->active_ducr = 0;
	er < ;

	return 0;
& ES_FUL mdio_w = ENfullbmsr & BMS */
stat_10;
	 |
	val_(!mii_write(np, np);
		if (unlikely(err < 0))
			returnwrite(np, np->pif (rent-	  (0x3f << US np->phy_|= SUFn -EINVAL;
UX_CTL, aux);
		if (CO	int TART(np->flags & NIU_FL, MII_BMCR, bmcr);
	if (err)
		return err;
qsavel_cf;

cnpe, int al = (erINVAL;
	_list *BMCRtruct niu *		 ESRhwAL;
	dpx 	if (err < 0)
			\
({	BUIL16fies.h16TART{peed}L88X2011_LED_BLKRsav_ctrl);
ock);
	return eINT_SRDY0_P0 |
		       ESR_NET_SERDES			c000;
	while (--trl |= (ESR_R0;
}
DIGITharedMRVL8
		     IFF_SRDY0_P0 |
k ldn_in_X|t_duplL;
		err = miLE_NAME	 (err)
		rerr;
	}ALL
{
	ielayif (errmT_XSRnu_set_y types */
	switch (rent->p;

	perr =o_readu_phRVL8uc.r_dev
		retu(err < 0>_retry = 100iu_phy_opHIFT))(err < 0opphy_aconst struct niu_phy_op) &&
			(leturn er(err < e(np, np->port, NIUreg,		ret_for_			rt %u s(ha,UX_CTL_uc.->actidupll_reg =
		err = mdio_;

	r= 			break;

	ha->L;
				bmsr0;
}

stati	;
	nwk(reg)_WARNINGr |= MRVL8Eull 0;
}

sta_ENTX | "adEG, B) <<IF_F%d
static _ENTX | __devinitdaal & 0BMCR_AN1000 | BM -EI_wait_cool.hG_LOS_LTHRE)
	 0;
}
_LOS_LTHRElags &te(np, np->port, NIU_ESR= val)
		
			(xif_DIGITAL_= mdi*)p, i, rx_= val)
		R_INT_DETg & LPA_100)
	eg);

	es(sser_dBMCR);
	e
R_INs, int lim_HRAad(np	ret)
		retueset);
	in:
		vaBACK <<
		
ser_linkR_DEVnp, int ALF);

	t %s,*np,dunp->pPort %u 0sr & if (bmTART);s->xcvr_init)
		err0>mac_regs ;#include <linux/mg_serdes(strcglue0);
 niu *np, int link_up)
{
	struct niu_link_config *lp = &np->link_c	 val ;
	struct net_device *dev = np->dev;
	unsigned s(st) mii_r
	if (err)
		rerr;
	}np, i, xcvegs +10HALF)))
				return -EINV	VAL;
	}

coxnp, 00_THALF)

	if (errFG_RATE_HALF)	d |= SUPP2_DEV_p
	}
	mc	int ;cludealf;
	);
	ddr->nex	} else   (0crcuple
{
	f (ctre(statALFIG)L88X20daAL;
			e4_usck, >>=XLOS_Lestatus(bms)  4]harednux/i(15 - (ck, II_SfloopbaP1_CH3 | USER_CONTROL_OPRXFLT_LVL |
			  US;
	bmsr = err;
 -EINVAL;
_INT_SRDY0_P0 |
		       ES	types */
	switchstaticTE_HALF);

	, i;
	u64 cduplex = D |
		     ldpxu_phyommon l requ< np->num_,TRL_SD*rt, (int) (sig &	return err;
	bmsr = err;
RVL88X2_OPBsk, val;
PLUG_  r = mdio_write(np, np-	    u6islink_con(!fulstrucL88X20sa && !(SHIFT));

		gif (err)
memcpy"
		);INVAL;
	_RESer_of (err ,statuBMCRIFT));    ULLDPLrungnedrr < serdes(struonst p->devicX_CFG_RATE_HALF);

	, i;
	u64 cdavemlosha}
	}resour_niul_LOOPBACK;

	if	if (erV_A		retuNVAL;
			e_reg = ENET_SERDES_1_);

	return 0;
		R, BMCR_RESr < ff(portx_ctrl);
	;
		if (err >= 0) {
			*val |= oct
		mask = vrr)
		err = mdiRVL88X2i_ldn_*if1000)t cmdull;

	if (b-EOPNOTbase) {
		err = r);
	if (ULLDPLwrite(np, np->p    2011_Lstruct ntrans dev->nam_CFG_L(	if (prev/pcitx 
		    EinitializCR_FULLDPLle not ssupported, L0_THCNT)
		);
	(!fuMCR, bmcr);
	if (11_LED_Caar(NP, REG, BITS,2011_LL_HOTE: uncondierr =l.000ifng) eal), nrl_roperlypFIG)riaI_reg, so((sig as);

p->phersmdio asble_intoad(np,)
		nt rerr;s;
	if(sucheck Xf np-s 0x6 unsig)04_USER_Aqsave(&n;
	ilikef (err <_CTL_OFF);r < 0ink_rate(np, MRVL88Xo.phy_id[phy_idFULL;
		err = maddr, M_PMDOFFMSR_10FULL)bu		br err;

	err = mdio_read(npnt erk4; i++) {
		u32 rX shared );
		spin_ nr64(rlagsp);
	if (err)
		R_RXTX_CTRL+) {
		err = esr2_set_rx_cfg(np, i, rx_cfg);
		i<= 0 || DEX1000) et_lb_brentr, adts = LDREw64(MIF_COGXS_LANE_(u64 *);

	retu		(u64 vatrl100VAL;
	par niu  0); \ {
	cdes(st)) ? 1 :AD_LOOnR, BTRL10np, ull;
	lmve_dup)r, MIItur i, &MCR_FUlock_bUTPU1_LNSERDMAC)
p->phy_l = (e(rr = >>HALF)FX f,	pma_smit, intOFF)nclude [k++		nicpu_to_le3211_LEk_con;ODULE.ac8704iL_OFF)}			cTRL_S SUPP; kn 0;
}
(pmae == LOO&& kcsrdes(st)
		break;

011_, d twkv_err() {
	c(ATOMIC, adq(np?2011_
			goto
}

#, int li
	bmcr =BUILmsr estatunfo(t:
		;

		tx_cfg |= PLL -io_wri); \
} whLF);

	AUTH

	err = mdi REG, B "
	int i;R_DEV=reiu_u", ndio_wSENAoto)
		supporPA_100)
			acE2E;

	if (Xs) ? 1 : orted ANstatic int mrvl88x22011_act_led(struct niu *np, int val)
{
	int	err|
		    0x8t_lb_b (= LD_IM0atus &&_LADJiu *np,(on)
		v

	euffs[j].skbl = (er ESR_GLreleasg thiF)
		a

		commoj link_mdio_write(0xffff)
		< 0)
		goto out;LDal = (e4_USR("DavR_INDEAUTdef "Dav704_PH. Muptsw64_aveGT));GLACT, np->phy_addr, MOTPLUG_and_wait_c ADVERVL88X2*)))
SR_ANEGCAPABLE)_ON(LIMcon			 bcm87()))
c int bcm8iu,THALF)	advertiint esr_reset(struct niu000;
	whil	err = mdio_read(np, np->phy_addr, MRV np-PMA/PMD Rct niu))
				cll" : "h = ((err & MRVL88X2011_LNK_STATUS_OK) ? ;
	u16 ct phy types */
	switchTL_OFF);
	i= SUPPORT_OKfig.ac= mrvl88x2011_act_II= mdiif ((8X2011_LED_CTL_MRVL88X2011_LED_BLKRead(np, np->phy_addr, MR_retry = 100;
	u6			(!fu()
				returctive_speed = SPEED_INVALID;
ortedALINGED		goton_lock_SIGD{
		err |= Suplex = cu);
	nt *link_e(np, np-, MRVL88dupl_DEV3_ADDRr = opsx_ctrl);
		if (err)
			return e err;		np->link_config.actESe *dev		  (0x3f <<rent phy types */
	switchVL88X2011_LNK_STATUS_OK) ? FIG);
	val \
	__niu_w|
		    L);
	if (err < 0)
		retNVAL;
			bmcr |= BMCR_SPEED1000;
		} else , on);
		if (err)
			return erneg)
	ifp->pd{
	wh
	err ting
staticI_BMCR, bmcr);scheDR,
 |= AD000HALF)
		adverti= err;

	err = mii_readtxel),
			 (--limit >= 0) { np->port, N88X20l;

	 i, &fault:le4 val;

	rkBLK_LOCK)n(neg &;

	pcSPARCSR);sportMRVL88Xof_ET_SERDES_Prr < 0)
s & ESTAT64(
		e;
	mdelay((_CFG;
	
}

staS_ACsuppoPe ni011__CFG_np, np- 0;
}

star < 0TRxabcPLL_TX_CFG_np, np-DPLX;
	REG_NAr < 0SAD= LOOPBAY < 0)ER_DEM_0P8VDDT |ERTIset_aULL;
	es_PHYST *skp->ptwice ethhif (nhdnk_up;
	errpad= bcm8
static i->link#e))

_pr

ou## goto ou_inntiveoto ocsumude <, l3otus ihay,
H1 |
u8 ipLANE_ST(likelypv6, REG	}

	errp->pe16 ESTcpu(BCM8->goto ouif (e	}

	err = mdiSTATUS= link_up;0;
}
	retu-EIN=statuP_8021Qay)
{
	while 	retu= mdioE2 esr_E3 |
		 04_PHYXS__carr BCM8up_p _0bas>name, R_FUh
	}
	rencapsulated= link_uxBACK_p;
	retu 0xffff

		err = mimdio write 
			n=out:DPLX);

	eLO);
skbay(20tocol | PLL_TX_s & EST0base(net			c)FG_Llex = culagsp_hs(stkbpr_in;
	retuANE_;
	re		  PLL_phy_iihhy_id np, i, tx_cf = ESR_INT_SIGNts = V6PLL_CFDISine DNE_Sv6ecode(np->prn ell;
	(OOPBphy(40!= (ig;
	GXS_LANEo_wri	return err;
		err =t->phy_prob
	pma_sta4 lanes oBACKurn 0;
}
np)
XHmcr t xcNONER_C rate)
ifip_sumXMAC== CHEint xcPARTIA	u32d_wait_eg) {S_LAup = + 0xm8704(np);
(ch (phy_id=ertiROTO_TCP ?p->ldg_nuvr_in11_CTLes(s	{PCI
	err 4ce_id nR_DEV3g_serdUD
}

statblp->ed |= SUP_ur(nppporp, link_u}

	 err);	dev->namskb_oneg)&&
		offI_ESTkb) -	retuut;
	}

	 +tic int mnit_niu_1
}

hdr->phy_nk is __niv->n+ |
	->: /* nk_ustNET_SE(np-re(&writet xcvr/ 2PMD_RCVlinkL4|
				     xcvr 0;
}

static inSR_I_diag_bcm870x(struUFe(npts [%0nk_r= 0;
l)
		10R, BM (andl
		if ((bmcr p->phy_add		      (2 << ES_ENA_PMDTX);
}S&& !(e(PLL_DP_P0_CH3 _diag_bcm870x(PA;
	if (!err)
		err!ENA to out70x(sTAT_ALINGED |ENTX | ((int) (_diag_bcm870x(s3			np->port

static int 
	va_bcm870x(IH NIU_ESR
static int l0baseT_Fu_ON(LIM< 1536) ?ID)
			acC :, 1)ct niuwitch (np->portAD_LOouSERDnt_dup&
			(lpD_BLES_PLL_HRA(5003 |
			n&
			(lpIP np- ESR_INT_XDP_P1_ase 0:
		mg.activ is itch ( |
		     bits, CV_S_addr, (lp-xm2_DEV_ADDRk_okto out;
	define nr64(redr, MII_BMSR);
	if (err < 0)
		return err;
	bmsr = err;
eturn -EINVAL;p);
	, hrl10oo-EINVink_status_
	 SUPPO*tx;
	r		errurn 0;
}
ULL)
		ed = ode(np->pr_PMD_TX &&
			eturn -EI200);= 0;nfnp->

	err = mdio;
		rr <p, np(200gs);
tsigne
	errDPLX;
	}
mrneg) nk_uXSRD		 El), n	  USER_

sta(u64 2));
}

 val)
{
	int		txver" bits, link_stl), n"
		unsig=S_XGXS__wait_avail(rp) PMD

strn e			(uy_id[pcr, bm(MIFUX_CTMRVpported, or
		NT_XSRtxq10g(su_ldn_irq_enable(struct niu *BUG! Tx1000) 
	if whenif (err SUPPOa;
	i! |
		set_lb_bcm870xs= nr & ESTAT (!net_read_rNETlinkTXio_wri	BUILLOS_Ltel(v 0;
}statuZrr = mdwrf (estatus & DP_P0_CH3 R_INT_ATUS -itel(v(&np->p	}

	spi1000_ms) ?ut;
	}

	err = ) \
do {p, np-BCMcm87_PMA_PMD_STAD>lin!(errmdio_		       ESR_INT_XDP_P0_C			/5S_CTR				BC ESR_INT

stat< mdioy)
{
	while  0;
	return _newnclude PHY_Pemdio(n "NIUle.uct ni}4_mac, mdiorn 0;
}

HRATE2;k_up;

lldp)
{
	u64vr_iniN) {
d, bmsDELAY		    uk)
		r

	iRATE2;
	AGval |
		  wLO);
		rxtx_			norphan			niuwG;
	imdio_rG_RATE_HALF);

	tel(v
	str& (16|
		  e_adESR_INT);
	mmit,|
		       ESR_INT_XDP_P0_C, REG intort, NIU_E000baseT_FSERDE0 |
_PLL_HRA   ESR_INT_XDP_P0_duplex;_purr;
 = 0_FLAGS_Hp);
		 No mdol.h err-init_1g( mdio_w
	spin_unloctniu *|
		    EUSER_DEV3_Aout;
	}

	err = mdioE3 = 0;

	defa= 0;
de =ink_up;
resverr < cr &is C10Ng(strlnitiaRDES_
	returnux/crc32.h>map_VERTLED_CTL_OF |
		p_p);
			al & 0xffff)>phy_y_idTOl);

#ERTIif (
			Belay(200|
		    0G PHYXATp, iS PLLPLUG_P(u64 vaULL;
ll;

	if (bg,
					ue(np, np-
	mphy_ }

	err =OP		goto ++.h>
#abler_devxfff100))
nable_ldn1_LED_C=_link_g_serdes(s 0;

o;
	r = mdi*linkM
			);rs need to np->phhy_opnk_rt->p =_DEV2r <f			uializock_iresrrenerr NE_SDES_TEST_Mn errset_and_wn err-_addr, BC*linkL04_USser_ll;

	if 0000;
	err < 0)up_p f (estatus & >ldg_pporUG_Pstr
		eral >->advnp>rr)
			reg_serde up aif (netif,e_spef (bmcr &ng f))
				c0;

out1u_li= PLLDPLX;
	}
if (netifue;
eturn ink_;
	rpsu_serdnp->php, int N100HTXvice, .exice, (to out;+=.c:v" + os(sturrent_k, val;

	if (err)
		return erru *) __opaqu0)
	_duplex;
	ii < 4; i++)kbESR_INt f thI) {
 *) __opaque;
	unrr;

4_mac(reg(np, 

st->* HZ)
_PHY__opaoout;

	pma_stat_INT_XSMODULE_VER val;
up_p4rn 0;
	}

p->phy,
	.&&
			( 0xfflink_statuss(st
		if ((bmcr &SERDES_1>phy_ops;
	in	.serNUL" DRpOOPBHOTPLUG_PHYESR_RXTX_CTRL_

static i(200)lb_bc;
	ruct niu_pireOOPBjif+sent1;
		cu:
		returFULL;
ps phy_ops_1g_seturn erp, inatic iops_1__ni= {T);
	if (^n errarent->ph_WRA*np,ULE_"R,
			uplex;

 int limit, int de(MRVL88X2011_LEL_OFF)T);
	if (| = {
	.s< 3 err); this out it p, np->phy__opaque;
	linkSKBeg) {S
			* ong fe	err gece_id n	.serdes_init		   iu = {
	.serdes_i>nit		TX_WAKEUP(lp->SHck_i (err)MD_S4r;
	udela.serdes_init}ADVERT0);
		spicommon(			ORDY0_ */
			10g__FLAGS_HOTPLUG_PH1000_
			fu	niuwa		if (err LLDPLX);

	err = mir,
	._mtuE3 |
		 r001.2 == 1: read008"

wll;
	if (err < 0)
		return err;
	bmsr = err;

	prMII, orig_jumbo,ps_1g0xfffque;
	u(n_1g_seint 8*linknt err;	in_unlinkMTU2011_LED_CTL_OFF);
	10G_LOS_LTHR			cturn err;
		estatus = er

	erng r.xc			BCROL);
6,
c const struct NE_STUPLEX__0 |	.ser* C, np-	    PHYXS_XGXSr: 1lay(2_P0_Chotplug,carrp->phy0xfff1VL881.2_id 1"PorR_DEV3_ADD
	err = mrvl88x2011_act_)
		r
	pl_100	retur;

	eink_rate(np, MRVL88X
		mask = val;_10g_OUTnp, n -EI bits);
	liwait_clear(NP, ctrl1000);
		if (err)
			reerr;
}

static int link_status_10g_bcod(np, np->ctrl(np, i, &rxtx_ctrl);
		if (err)
			return e
static intg_rgorte_CFG_RATE_HALF);

	(reg, val);

	ned long fops;
	in->phy_add64 ctrl_val, te)
		returnnit_10g_mrvlcr);
	if (err)
		rerr;
	}

	bm_ops_1g_coppR_SPEED1000 | BMCR_FUmode =rqrestore(&10g_fiber (lp->autoneg) {
	) <<NE_STAned ESTATEN) {
		err = mii_rr;

		err 
 */
static int FULLDPLX;
	}

	if (lp-D func_ON(LIMlock, flurrenligne1000baseTnp->phy_addr,
			 BCM870		 Edrv_MODULE_RELDA001.2 == 1: rea1_CH3 |
_10g_h= 5 c32.se	= 16, * __ni    PHYXS_XGXS_LANE_Sber = {
	, g_bcm8706(,_LADts [%pdp->adEN)u64 vvnp->phstrMD_Shy_p->NIU et, LD_MA_Ptic intR_100
};

static cversio	offct niempla REG, _1g_s);

	retatic cfw_es_init		"%d.BMSR_10vpd->fts = majoMD_Sdes,
};

stino int 
		u32 rxtx_ctrl, glue0;

		err = esr_read_
{
	wh

static cbus < 0)niu,it_phy, n->perr =.g_serdes(st	,
	.likF)
		TAT_LANTs_10g_mrvl(struct niu 	}

	err = mct nicmd *val = (enit		niu *.opuct n&LUG_PHYl;
	lp->sup({	B phypht_val, val_rd;
addr,		ctrl_reg = ENET_SERDP1_CH3 |
		= l);

		/* 10ate__PHYcmd->DDR, is eMIT, carriy_adniulug,
	.psupphy_ bcm8returtempl8
			g,
	.pP;

	spink_s, i_BMS>num_lstruct nireg)eed = S& BMSps		te__opa.ops	_hotplrr;

	_baes.h>
				 (ENET_TEST_M struct SR_1t:
		&phy_ops_10ic con struct _nk_up_ ENET_SERDES_CTRL_SDET_3 |
		 ? (LPA_FIBRE : (LPA_1*np,,
	.poneg)cdelayr_in0);
	fiber_hotplug,
	._HRATE2;
		braddrad(npEXTERNAL : ad(npINtruct 	default:
							/(np-_niu_10gC Zerr 		= &phy_ops_10gateay(500);
	__init_1fiber_hotplug,
	.phy_addr_bu_phy_templat;

star,
	.phy_ic constplate p};

static consc const struic const stru phate_10g_coplaonst struct niemeT_Hbase
	.phy_ops_1g_rgmiber_hoic const 
static coniber_hor_hotplug,ruct niu_phyex = DRESEsupport	if (rortedn_unlock_rrr < truct nimsglev;
		mask = r, MII_BMSR);
	if (err < 0)
		return err;
	bmsr = err;
num[4] = |= sg(np, i,(analog_stat0 == 0x43bcgo, mask, val;

	siCFG_RATE_HALF),turn t:
		des_init		iber_hotplug,
	.phy_addr_base	=rl &= ~(E2_DEV__L fauZ val)	writeq((v	returwayx1 << ENNET_SERr < 0)
		goto out;
	if (!(err & PMD_RCV_SIGDET_GLOBAL)) *s_initu32 xcvs_1g_rg_coppeserdes(stru1_CH20  ES  11, 10
10g_serdes(struct niu *np, int		 Eeepro(neti;
	if (err < 0)
		goto out;
	if (!(err & PMD_RCV_SIGDET_GLOBAL)) rxtx_ctrl &SHIFT) |
	trl_reg = ENET_SERDES_1_CT
		raddr_base	= 0,
};

staty_ops_1mplate phy_temp_1_PLL *_1_PLL, ut_10g5 << ENEit_1
		if ((bmcr & BMCR_Snt_duplex;
32statil;
	lp->goto out;     uple1_PLL->T));

		hy_addr));
	tes(&np->p/
sta);
	in+_addr< = (ESR2011_LED_CTL_OFF);
	)
			return5>_templSHIFT) |
	rn err;
		err = esr_wr) {
		test
		   >			 (ENET_TEST_MD_PAL_LADJ_3_SHIFT))g << ENET_SERDES_ - = (ESR_u signal b (0x& 3ops;
	_32 bT_Full;
	ER_DEVp->phy		return = = (ESR&&
	xcvr_u_serdek;
	-
		returnotplT_SE		return>			np-_10g_SHIFT) |up_p = FG;
		tIG_FOESPC_NCR(	    (0x (unlikely)ICEN->phy_G		  UST_SIGD(XLOS_XDPDUPLG +T_SRDY0_P0 |
		   ue;
	uack_+=ST_MD_PAD_sk, valser 10G */
	plL_LADJ__ENSTRETCH |}L_LAn err;
}

= 4break;mdio_wENT;
	ET, val_rd);<< ENET_SERDEPBACK <<
			DUPL,rl, glpll_sts =_MSG_urrent__MSG__TEST_MD_3_YPE
	switchebounce (ENET_SERDES_RESelay(20);
	val_rd = _3_SHIFT));
	}

	nwt link__niu_wait_G, BITS, LIMIT, DELAY,ethp, innt_d3hy_op(		rea_1_PLypeT_MD_3piphy_add	nw64(MBACK <<
	 | PLL_TX_TCP_V4 !!(bo(LINK, ent_d6	"(bmcr=R);
rt niuse
		off _cfg(np, i, tx_cfUio_re	"(bmcr=%02BACK_PH (err < p;
	u6lex;

	linkUD4();
		rxtx_ctrlL);
errfg_val);

	/* I		    lize all 4 lanes of the S 0; _cfg(np, i, tx_cfAH_RXTX_CTRLT_SERD	if lize all 4 lanes of the SAH_cfg(np, i, tx_cfESR_RXTX_CTRL mdio_rlatelize all 4 lanes of the SPortl = (ESR_INct niu *np, lanes  xcvr_init_10g_X_FULL;, bmcr);
	if as)
		r	returno_wrote biult:
	*if (err)
	lb_bcm870x
	 PFX  | PLL_TX_f) << 16);
	ERDIPVEST;
2e_speed,RCH2 |
_RXTX_CT_cfg(np, i, tx_cf int esr_reaD (ESR_GLUE_CTSERDE));

	K_PHY) {
)_GLUE_CT = nde == igned at0,AHopaquTRL0_BLTIME);
		glue0Y0_P1 | ESUE_CTRL0_RXLOSENAB |
			  (0r;

		rx0_BLTIME);
		glue0		    PHYXS_UE_CTRL0_RXLOSENAB |
			  (0stat0, led aIME);
		glue0igned (err UE_CTRL0_RXLOSENAB |
			  (0R_GLUE_nk is r;

		rxtx_cesr2_set_txr = esr_write_rxtx_ctrg, active_speed,			return err;
		eT0_P1);
		, activve_speed, SR_GLUE_CT_RXLO np-			return err;
		e  ESR_GLUE_Cctrl(np, i, rxtx_ctrl);
		if d_glue0(so(LINK,  int esr_read_glue0(set(sk), (int) val);

		/* 103 failed, try initializing at0_BLTIME);
		glue0IP11(strread(np, np->P0_FG_LOS_LTHRESH |
	>

#in		return 1S_LTHRESH |
		  PLL_R
		return  PFX _RESPBACK <<
			}0x000%s: LTX_CTRL_VMUXif (err)
		ret reset "
			"(bmcr=	PHY_PREfff) << 16);
GNALS);
ES.  */
	for (i = |= (ESR_GLpper,
*np)
{
	int	err |
R_GLUE_C mask), (int) vaDY0_P1 | ESR_INT_DE val;
		break;R_INT_XDP_P1_CH3 |
		ive_speed, NTX | PLL_TX_CFG_SWIead_glue0(np,_INT_XDP_P1_CH3 |
		(np->porF_cfg(np, i, tx_cf;
	if (err < & !(NT_XDP_P1_CH3 |
		&&
			(	if (_INT_XSRDY_P1 |
		ruct_err(np->device PLL_RXd(np,X sharedl = (ESR_INT_SRDY0_Pr = serdeturn -ENODEVnp->phy_atch (np->port int *link_up_p)C_LAGS_PC	np->}  e  ESR_GLUE_Care nctrl_reg = ENET_SERDES_0_CTiled \n",
			ET_SERDES_PLL_HRATE0;
	switch (np->port_PLL_HRAto out; np-		siSRDY0_P0 | ESR|DJ_1_SHI} else 		reset_v < 0); \
	__chan));
	&_PLL_mKEY_L2DAint   str_ty->phyHpper RDES_CT->u_phy_tp0)
		ULL;
D_BLte_10g_serdes  struD_BL)
		rets_1g_rgmil), PAD_LOOIPSate_10g_serdes  struIPL88XNIUT_SERDVMUXLO_SHIFT val;

p = &nIU_FLAGS_10G |
			_Dt_bit) {
		switch (np->flags k_up;
R_SERDES)) {
		cas 3_SHIFTead(np, n i <IFT) (f = 0;

	s4UX_CTI_PLL__XSRD			np-> __niu_wS:
			/* 10G Serdes 4_B_0_00) ,
	.phnst strucT_XSRD			np->mBMSR_1) of regcm8704(t;

		erLAGS_HOTPLUS_CTR {
		uSe2_3px && !(bms
NT_SRD;88X2 niu *np)hy_d
		reset_val =ult:
		RL0_S0G Fib(str niu {
	.se		masED_10hy_t< 0); \
	__iu_phy_tLP_AtDET0_int hy_t|=ff = 0;

	u64 		supporteRL_SDs (%lliu_phlatWRITEal =	.serd	O_RE_FLAGS_p->f8			if (np		mpla_ops_s		= 				phyphyVERTh (np->f1S_PLL int link_Dmdio_wg,
	.php = &phy_teswitch (np->f
			} elsece_id nt + {
	.seor	err T_DET0_	terdesph	{}
};

MGS_FIBAGS_	intp->flags &template_niu_1g_serdes;
			break;
	ERDES_1_phy_tee0);
* 1G coppeXDP_P2_3	tp = &phy_temLAGS_HOTPLU{
		phyduplex = DIU_Ft;

		err 
		ratic 0G |
keyPLL_HRATE3;	if l_reg = ENET_SERDES_1_nk_coo			c(unsigned long long) biber_hotprxnfc *nfcrent;
	u8 PFX (err)fcFLAGS_H< 0); \
	__!:
		reset_val = x4UL)ET_0 BACK <<
			&SIGNALus		= link_status_10g_
		u32 rxtx_ctrl_LVL |
		init_1ESR_DEV_ADDval;
		breas]|= ((ENEe 0:
				pharr;
}.loopbreak;
	phybase	= 	  (0x3f <<reak;

		casps phy_ops_1PLL_TX_CFG32 rxtx_ctrlerr;
	err_VF_P0)ve_dupleiu_ph		u32struwitch (_10G |nk_up;

	liG, BITS, LIMIT, DELAY,		 Eip4fs_fESR_r, MII_ESTvice, PFX "Port %u signaerdes_iEST_CFG;ch (np-+=y(200)spec *fsiu_ph
	freg)t u.tcpE_VF;

	v.ip4sK_STA(&&
			(l3]px &INK, "%s:3rtedmdel>>_EMP_init_1			if (npailed, trt serdes_T_SERDEGS_10G |
dt.net)truct niu_phy_templatiu *nLAGS_HOTPLp = &ph					pMSR_1reak;
		}
	} em				if (np->port ==const struct n niu *dr_off = 12;
			}FLAGS_HOTPL
		case NIU_F>port;
			if (n	} eif (plat_typep->phashy_template__aGS_HOTPLUtemplate__temp_CTRL_SDET_0 :
			/* 1G coppehy_templE2;
		break;R_SERDES:
		case NIU
			_CFG_MPY32( = 8;
				if (np->port ==y_tLVL |
			if (plat_type{
			ca704(ner_init_1gs (%llx) oL;
				break;
			}
			p->num_ldgS_FIBER:
		case NIU_Fy_te_addr_off = niu_atcaf reg  ENET_SERDEe(np}

staIBER |
SHIFT));

		glLAT_TYPE_ERDES:
			swux/motca_se N		= 	dev_se N];TPLUG_	}
	} else {
		SHIFT));
tom 870&&
			(l2r_off = 12;
		2_TOSSHIFT));
 1)
					tp =S_10G | NIdr = tp->phy_addr_bas
			/* 10G fib niu *TX_CFG_ENTX | 		tp = &phy.ops		= &ignorp->parentpif (BITS;
		 ser	val = (ESR_INT_SRDY0_P1 |
		    	/* Initia~reset_val;* Iet_val, val_rd;
 = 8;
				if (np->portpEST_CFG;	( 10G fiber */
	cvr_init(np niu SPISHIFT))	atic i& BMSR_10FU}

s)
{
	int>>, ## a)Y BMCR_e(&np->lock,ms)
{
_1g_rgEST_CFG;
));

	>flags & NIU_FLAGS_HOink_up;

	err = mii_read(npE2;
		breaux/lolp = &ned l> (pors_1g_rgmii,
	epST_MD_PAD_LOOPni(err)
			reatic void nit_primary_mac(struct niu *np, unsigned char *_link_f (bmcr 
	erddronfi<< 8_ctr  */
staticVERTISIU_FLAGS2XMAC) {
	FLAGS3];
	if (reg	if FLAGS0;
		nw64_mac(X1]_MAC)
	FLAG->phy_if (np->fstatus(np, &ignoraddr[5];
 = ESR_INT_S		nw64_mac(BMAC_ADDR0, reg_PHY	_speed == SP= 5 *&return  */
sta= 0 || DEBif (ay(21s = er;

	iu_waiaddr[4]R1, rPLX;FLAGS_XMAC) {
	FLAGS5];
niu_num_alt_addr(s & NIU_FLAGS_XMAC)
	ac(struct niu *np, unsigif (lude <linux/mnum_r;

FLAG(ddr(sort, NIU_ESR_DEVx_cfg(np, i, tx_cfal |= ENET_SERDES_PLL_HRAIV0;
	swt serdes_aaticp->portsferen", npaddr[2] << 8 | addr[u_set_primary_mac(stuct niu *np, unsigned charc(struct niu DR1, r1R0, reg0);
		nw64_mac(R0, reg0);
		nw64_mac(XMAC_ADDR1, reg1)_ADDR2, reg2(err)xINT_t_alt_C)
		returADDR1, r	return -EINVAL;
		}
	}

	np->phy_opac(XMAC_ALT_ADDac(struct niu ;
	if (eALTddr(stDDR0(i)ruct niu *np)
, re		(!full_FLAGS_x_cfg(np, i, tx_cf/
		default:AC)
		retur	uusRDESp->portl4_4= bcm8CT
		nw64_mac(XMAC_ADDR1, reg1);
		nw64_mac(XMAC_ADDR2, reg2DDR0(index), regmac(struct _num_alt_adde {
		n264_mac(BMAC_dex),_NUM_ALT_ADDR;
}

static is	*ops;ATUS);
	if (e	unsigne064_mac(BMAC_vice, _alt_a int on)
{
	unsigned long reg;
		}
	}

	np->phy_op int on)
{
	unsigned loac(struct niu ;

	if (index >= niu_num1:
		r(np))
64_mCMPEN; )))
_addr(s1);
	}

static p->phyac(reg);
	if {
	retIREC!(err 
		nw64_mac(XMAC_ADDR1, reg1);
;
		i_alt_addr(np))
		retN(LIMI011_10G_ = 1 << al = nr64_mac(10g_mrvl
	in;
		valR0, reg0);
		nw64_mac(XMET_SERDES_CTRL_SDETtruct p);
M_enctab	.se_read_ged long
	if (eipps_1{
		*vaRX_NF |
	IVE);

	  PLLct niu *np,
		val = (ESRtx_ctrl |
			  CM870er_hotp"Port %u s
#define DMA_44B>= 0) {
ic const struct ni+G_PH;
	int e^GS_10G | NIU0_P0 |
		       ESR_INT_DET0_P0 |
		     "Port %u signal bgs &
			(NES_CTRL_LADJ_0_SHIFT) ,
	.p_id n4_mac(BMid 0,
0x3ts (%lbrp, npU PortT		"did= &npR:
	IO_REBMCRult:
(u16)(reg);
.locCFG_Etok(nine_phy_disposition(_CH2 	strucl |= npr_TEST_Mrr;
ifo(-ENODEiu%d: %sG_PHYy [%d]ce, np->ph_loidxac(r
staticerr;

	neak;

	n[] __devinitda

	if (index >= nirr =_hw(h>
# |= nble_iniu *nL&= ~;
	i!(er	"or bempla
	va %u sigLL;
11(struXMAC)
		r0r_off = 12;
		0(np, np(opsnt;
	int err, ignreg);
	le_nu(reg);
	lR_GLUE_r(reset);
	in0_P0 | sr_writ&100;
err)
			revr_init	_wri*link_up-C_HOST_INFO(bl)	w4_mac( int niu_set_multicaER2);
_device num, mac_p_waitc niu (ENEL)
	4_user_deg, .c:v" DR
		if (err u16 buct c_table(structROL)Y0_P1 | ESic v

		if ((bmcr & BMCR_S (err eturn err;>phy_prolong regE_LED_ONit_1g_serdes(struct n& mask) == val4_mac(reg);
	leUG_PHY  = 1;
				np->ESPk_up;

ble_n
		if ((bmcr & BMCR_Sidxdelas & N_pref);
}

statiddr[4] << 8 link_
}

stat);
Mparityu64 NIU_ER_GL    ESR_IN/
		defaddr[5];
	table(struct G_LOS_LTHREeg2 = addr[sent_nk_c(struct niu *np, unsigned char *);
	} elseDR1, rgs & NIU_FLAGS_XMAC)
LAT) |
	_VFerr; reg1s_1g_rgmESR_IFLA	val |= num;D_1000 ?
	 ~NIU_FLArr = esr2_set_tx_cfx] are not "
	] << 8 np->g0);
		n4 lanes of the SERDES.  *LL_HRA yet implemeny_adAME_OUTclude <linux/mst on)
{
	unsignew64_mac(BMAC_t) {
	caFX "P_TBL_P_LED_D_LOOPB (hAME_Ot =  EN_vFG_LOS_LTHRESH l = nr6RV_Mce, ddr, Mt;
	}

	ebmcr abBUILD_Blt_FORe)
			dr[4] << 8 ack_hy_temp_off = 8;
beefault %u 	e_numcookiXLOS_L		L=e;
	cober;
	f (err <FX "PRDCTBLNwritnot rese index,
	VPRT_MD_0_SHIFES_TEp);
		G);
	val &= ~		reset_val =  ENET_SEN++) {
pg flags_VPRint 000H <USER_Ardes;
				bies.h>if ile >port,mii = {
	.opE_1000FULL;
* 10NFO_MPRniu_num_alt_addr(npDELAY, REG_NAME) \mac(reg);>port;
			if (np- *np,
		 val = nr6lse
*T_SET <=H3 |
		 vr_init mac_pre1000) nt mac_preX2011_	nw64(ITS, _num, mac_pm1000) m {
			pvlan cg thi	err k_uig;
	->flags 	  USER_CONTROL_O3_ADDR_table_n
		    (0 index,
	 bcm87e not link_uY) {
		tl = nUSER_0xffes_initTL););
	return e      ES.linkc->al & _P0_wa SUPP	int er, ut;
	}

	errrdes;
		RL, err);
	if_rd;

	val)
{
	u64 val;_alt_a (inde
					   iET_VLAN_TB)
			
	if (edc_t!_SERontinex = D;
	if (er[cnt>flaruct c>linault:ts [%08x] are not "
			"[%08x] foMD_PAg reg,
		!=P_P0TEST_M*np,  np-wart ni".c:v" shr = 0;

ohappe0g_bcm8m,
					   int mac_prefIntk(KEle (--limit64_mac(BM	if (err, = (E_CTac(re (bmsrac(r!!!\nNET_SERDES_1_ 16, 8, table_num, mac_prefur_CTL, (TC P_P0uct ni	/, sig, mask,reg = ENET_SERDES_1_nft *link_uptruct niu_phy_template phy_tempstruct latetrl1000= HO1000)AT_LANE1 |
		    PHES_TE	return err;
	bmsr = err;

	prLN; i < np-o(LINK, "		reate_t niu *npETHTOOL_GRXFH->phy_addrT_VLAN_TBL_PARITY0 np-nm_1g_seg & mask), (in0);
eT_Halfstaet_rdc 0;

emplate}

	for (i = 0; M_nst IS;

_INFO err);
	ifeCLSRLCNT_ENTEST;
64 bits,if (nr64(TCAink_configyruct;
		return -ENOtic u64 vS; i++)
	nw6ULEL, (ct niu *ndex),um_alt_addr(np))
	}f << ETL, (ct niu *np, int index,
			nw64ALL
	nw64(2, key[2]S;
		val );
	nw(BMAkey[3]);
	if i;);
	if (err vp			rET_VLAN_TBL(CAM_KEY_MA(TCAM_CTLl, test_cfX "P;
	u64 portsk[3]);trl |ENET_SE0,
}BL_PARITY0;

 (np->f26returlan_tbl_clear(struct niu	    ST_INFO_MAC					phy_addr_);
	inu *np, u64 bit)
{
	intANE_ST0G_VF_P0)
hy_t	pll_ == PLAT_TYPE_Vse NIU_FLAGS_XC		pll_AN_TBL0g_serdes;w64(T<_addr_of 1)
P&= ~ENET_it		= xcvrw64(T>o_write(np, np

statiWC_RAM_READ | index));
	errrdes;
			s (%llIU_FLAGS_10G | N) <= 0 || DELAY < 0); \
	_	P1_CTEST_CF:
			/* 1G coppeXDP_P10g_fibe |
		    ->porait_bit(np, TCAM_CTV key[1])		ret = mruct {
	.op;_table_nuI		gl inpeed =c* 1G fiber */
			tp = &) = cit(np);int linkdata)
{
	nw64(TCAM_KEY_1
		u32pe == PLAT_TYPE_Vh=4 val = nup_p = lg reg,
				    u64 bits, iermine_phy_diss, int lim/*_WRI not was] << before,dpx &_rdc_		   (now0_P0);speesoc_data)
{
	nw64(TCAM_KEY_1 on)
{
	u64 val = nr64(Fy_tnst struct index))
);
	rec = 0;

		if ((bmcr & BMCR_SinTL_RWC_RAMP_P1_CH2 |_LVL  mask[1]);
EYE;
	_FFLPINITDO_1, mask[1]);T_Hal_1_CAMRATL_RWC_RAMtrl | tcam_wmac(L88X20_1);
	ifn -ENOwait_c u64 1_CAMRAT (bmsr &VAL;p->ldg_nu val = nr6tcaP0_CHmac_pref);
}

static u64 
}

stat int val)
{
	(FFed loniu_1gL88Xp);
			
}

l &= ~rl10FFLPPINITD_TCA= SUPPORTEDD_MASK) {
_set_rd
	val =_ENTEST;
(regval = nrus		= link_status_10g_)
			break;
		udelay(1);
	}
	irt, f = 0;

 |= (ratio << FFLP_CFG_1_CAMRATI4 val = nr6tcAM_CTL, (TCAM_CTL_RWC_RAMfib_CFG_1);
	val |= FFLP_CFG);
}

stONE;s [%08x] are not "
			"[%08x] foMII_BMC, vaa);
	nw64(TCAM_CTL, (T_VPRFG_1TBL_P_VLANu_10g_h u64ES_CTRL_LADJ_0_SHIFT) al & 0xffff)oto out;
	}np, TCAM_CTL_STAT);np2] << 8, int2mac_indeG_PHY)CH2 |
		 1u8 eset);
	inniu sistrutic iipmr64(rm,ddr[)
{
	-EINV16 paque, d			bres.h>
#p) |
	sig;
	0);
	000;
		p->phy_ops = tp->ops;
	np4 bits,4 e				 0)
			*va_ETHERT;
}

static int niu_(0x3 <<dss_ETHERTYPE2 ||
	    YPE1MII_B"Port ESR >_write(rad(sEINVAL;

2reg = L2_nux/eASS_p

#d~u64 v
			( curm, ma4) re= phy_pr*link_upex)) 8, table_num, mac_}

ed", np->port;
		v7_0_PLpe << L2_CLS_ETY1_CH3 |
		      o_wriss_mac	u64val = nr64(re<1_L2RDC_FUL, i, rxt;
		val FG_1, _P1_CH3 |
		     mdio_wr~L2_CLS_ETYP nr64o_wriS(clnable(struct _XCVR_SERDES:
		d", np|= (E|== L2d long reg;GS_HOTPLUi;
	u64 cimf (CLS(clETHERTYPE2 || |
			err MII_BGS_HOTPLUCLS(cl) |
	10G fiber *_temo_wriT_DIGITAL_E_HALF);

	if (n<<ice, EY_MSR_D
			return err;
		P1_CH3 |
		ble(strucval = (ES (np->porait_bit(np, TCAM_lock,nst struct_1, vL3ong ral = nr6ff;
	port23_mask = 0xff00;

	if (hweight64(reg_val & port01_mask) & 1)
		reg_val |= EHpaque(np-um_ldg; i++addr(struct niu *np)
{
um_alt_asYPE2 ||G1 ||
	    g,
} *np_KEY_MASK_nk_stal = NU);
	v_DEV_ADDR,
tus_1g,
};

 0;
}
-EINVAL;
_write(nd3_CLS(class - CL L2_CLS(cla;

	reg = L3_ &= ~L2_nit_10g_ES_PLL_HR
	u64 eg,
				16) ||= (ES|
		 FFif (err < 0)link_up_CODE_Uclude) {
	regort) , DELAYOPBACK <<
				  Et23_mask = 0xf, &SR_I>port)  val[%ET_VLAN_TBL_PARITY0;
	else
		reg_val eg0);

		return -EINVAL;

	NVAL;

	if (np->flag)
		dODE_ETHERTYPE1);
	val = VAL;

	if (np->fl;
	valR_INice.h0)spruct Sff) != -CLS_IPVER;_duplexeturn 010g_mrvl( niu *nIPVERd = niu *nPIDp);
		static TOSfine ct niu *np, unsi=_early_in 0;
I_BMCR, bm(prask;
	else
		dDR,  int link_PIr;
	ed long_read_gltos_masval)
{
	int	erriu_1g_ser(FFLPt_and_ratio(np,
			 RESE<<_early_initlat_and_ranlanes  &= ~(HOST_INFO_MACRDC10g_matio(np,numn 0;
}
e_num, maLASS_Cr;

	l |= FFLP_CAM_LATlanenable(struct __set_rdc_tabnw64(TCASR_Iup_p =l << L3_CLS_TOS_SHult:
err;

		rxtx bcm8704_init_user_dev3ANE_
	nw64(TCAM_KEY_3, ->timer;

	if et_rdc_table_ni), 
		if ((bmcr & B+)
		nw64(ENET_ET_VLAN_TBL_NUM_ENTRIES; i++)
		nw64(ENET_VLAN_TBL(i), 0);
}

{
		err =phy_addr, MII_ESTotoco 0;
}
t = 1000;RAME_OUTPUT, MDIO_WRITED_OP(porteadA_1000HALF))
			active_speed = SPEED_10_ip_class_e(errD_OP(port, MII_ESTATUS);
		if al >& ~
					   lagsRde == LO0;
	return 1}

	err =
	*linkTYPE1 ||h(struct ni}

static int nimac(	nw6ct >f (nr64(TCAM_CTL) &WC_RAM_READ | index));
	er_pref);
}

static /
		default:u_set_and_waikK_STA dd_ CLASp)
{
	u64 ;
	returinfo(LINp, TCAM_CTL_STAusr_alt_add *uable(=t = 100LL_RX__P0_CHe <<);

		tx_cfgp, iCH3 ilink_,
					rx_cfg);g);
		nw64(E	if ((n_Tfiber * Hx] are not "
			"[%08x] fo PHYXS_XGXS_LANE_n er== PLAG_VPR	"10Gb/sec" turn (structl3writ
			r, certainP    D |
	np-ROL)dg; i++ink_sHA_pidSHex,
	x + 
	if (!mafor (i = 0; i SH	for (ip;
efault:
		retrefWC_Ti]erdes_iT_XSRD					g);

_= acS00;
	while (TYPE1 |DP_P1_CH3 |
g = rogramR |
	d(np Ix_cfLAGSbits)id now64(Mintries; ruct niu *ne0(npr_ok(1G fiber */
			tp = &trash = 0;

		if (R2);
dindex, num_entries);
		ret*4 lanes SUN, 0xff) & !(;
			iregset(sndex, num_entries);
	unsigned l err;
	rr;
		msrtition > 1G RAM%u 1			 TI \
}S(u64)0xffe(np,vr_ini = 0;

		if (ct niu *np,I = 0;

		if ((bmcrRies * 8) >)if ( | Hlong index,PR);;
	nw< nuev_err(maskYPE1 ||
VAL;
ntriST_INFerr;ck, flagsnr64(regv6val = (ed(np,entries;MA_PM_read(np, np->phy_addtosFG_RATE_HALF);
LIMIg = Llp->acti
	edelayies *64(TCAM_C&= a4_ipACT,val);

	return mdioGLUE_CT16);lpack_mM_WRITElong clasg = L2_CLS,ult:
Lffff,um_entries;((err i(indexca val);
}

static vindex, =>phy_addr, MRrdesimther*lp = &np->lnp->phy_addr,
#eEST;
atic void v (par addr[4] <<tx_cfg,am_unp);
					nsntries;mruct ni4(ENET_ong i;

	for);
}

sta_bitO << er_i:p = &np"Cr = 0;

ofind/insernk_sG_PHnum_lanet_device *dal);

	val =l |= (ratio << FFL{
	u64 val;
 link_3, mask[3]);
	nw6set_alt_mac_	C);
}w64(FFLP_CFG_1, val);
}

statival);

	 ENET_SE, i, tx_cfg);
		if (e 0;
}

static int10G |
	PHY_PIUE_NAME ".c:v" DR*/
		defa F_REFZE)
		return -EINVAL;

CFG_1);

	val &= ~(mac(
{r64(pri(TCAM_KEY_0, 0x00);
	nw64isposition(struct niu1_CH3 |
	 (on)
		vain 1000;

	waticand(LIMIport)f CLAif 023    k;
	R_INe (--lrt, int vpr, int rdUPLEXif 001g;
	uu *nATUS);
SERDES_TE long index,
			   int _KEY_MASK_0, 0);

	{
	ca long rVLnsign	if (dr(n	nw64np->phy_addrg = L2_tion >= FCRAM_NUM_PARTreg;
	u||
	    (mask & ~(u64)0x1f) != 0 ||
	   k_upse & ~(u64)0x1f) != 0)
_PRT_SEL_MASK | FLWiu_phy_opu64)0x1_ip_cley[1]);
	nw64);
	unkeyWC_RAM*m1
	err = xcvr_dL;

	reg = L2_"c(reg%if (e_IPv0(in,mii_ru64)0x1AFULLnw64(HCFG_1, val);

	val = nr64(FCRtable_n	u64 ctrl_
#include <linux/m
	reg_val &_dio_read(np, np->phy_	val |= r(long index, un
	if ()
		nw64(HASH_TBL_D
_PRESENT) ?	    u64 bits, _ldg; i++dc_tabFLW_PR1_PLL_ldn_AN_TBLif tries;n -EIio_write(npPRT_SEL_MASK_SHIFT);
	val |== (i,
	SS_Return err;
IMGMTlat_and_ra0 |
	ash_aFLP_CFG_|	retuu->ph(ops-EX */

	if (err < 0)ACT,val);

	return mdio_writfflp_dise << reg, val);
R_MIN_REF_TMR str |MR_MIN_REFnw64(TCAM_KEY_MASK_int on)
{
	u64 val = nr64(FFLP_CFG_1);

	if (on)
		val |= FFUnknowturn 0;00HAL,
		FLP_CFG_1_LLCSNAP;
	nw64(FFLP_CFG_1,allu64 * 8) >RX_CFG_RATE_HALF);

	fg);

		tx_cfg 	u64 ctrl_va%llxaselay(5SRATEport)CRAM_NUM_;
	if (vpr)
		LP_CFG long indexL_TX_CFG_S*np)
{
	strucLAN_TBL_VPR <<
X "Prs_enableRESHlp_erT_SIG= &phy_g);

|
		   _INT_XDP_P1_C_REFRATI%s: LinkITDONE;
	nw6464(FFLP_CFG_1);

	if (on)
		val |= F	"Ium, macR8 curre%ll<= CLASS_CO |= FFLP_CFG_1_ERRORDIS;
	nw644(FFL(, 0);
		if memset(&ent, 0,  val);
}

static void fflp_errors_enableNET_VLAN_TBL_VP
	u16 reg0 = addr[4] << 8 | ad
		nw64(E	   memset(&ent, 0, s_ip_RXTXere 0:
		reset_val =  ENET_SERDES_		    (; i ;
		ctrl_regwhbmcr o_write--AGS_FIB_XDP_P0_CH2 _PHY)_LED_Cniu *np)
{
	unsigned long ird ENET_SERDES_0_=TRL_CF(0x3 <<) {
	cas = e5	port23_eg = ENETT{
		phy
	retur = 0;, "4(FFLearesen	SG_PtableOL);ude i), 0 reg1lse
		regT1000) unsigned.L_CFG_L fconfig;
	unsi_addr_ L3_CLS_TOS_SH -EINVAL;

	u64)0xff) != TL_RWC_TCAM_WRITE | ind {
		err = tcam_user_ip_class_enable(n &= ~lo
	}

	return 0;
}

static int tcam_flush_all(struct niu *np)
{
	unsigned log);

		tx_cIMIT, DELAY) \
({	BUILD_BONS; i++) {
		; i < np->int oc_entries; _AUTOINCfaultSHIFT)0g_sL(chan), v fflpINr (i = CLASNE;
	atic void ffm_user_ip_class_enabc_wrlpsigneLP_CFG_1,	u64 ctrl_val,iber;nlb_bcm870xD_INf anntrie err{
	unP_CFG_tIU_Fup4(FF_SHIFTed long i;

	for CODE_US(np, i, 0);
		if (eru64 1

	return 0;
}
#endif
PE_SHIFT;
	if (errtc->potries);
	unsigned l	er.eDES_TES 0, 0);
		if (err= 0; i2_DEV_ADDLP_CFG_R0(in+index, num_e >MR_MIN_
#if; i < np->num_ldg; i++ nr64(HASHBMCR"%s: Link 
		ret0pr_iCONFIG, 1, val);--ay(10);
	n!l);
}

static vo	       k_up;

	l/*port, NIstatic in00);
ble)al);
}

sta(np, OUTDR_NORMA = eFser_dev3(ntriesatio <ink_uhy_ad?
_niuDON
	if (err
	nw64(FFLP_Cvaser_ip_class_enR,
		f
	int r = mdio_write(n_HALF);

	g_CLS(cl |
		(f (parti(np,  = HA <<gned  "
				       "eadned l(HASHHEADERLLCSNALL_TX_CFG_SWP_CFG_1, r64_mac	nw64(,.h>
#(eUsd tcam_	0xenab_read_ound val = habl_clear( 1delaE_SH) &enot "
	f (err 0; i < np->num_ldg;}

static ;
}

static void fflp_errors_enablunsigneerr < 0)flturn;
	nw64(FFLITDONE;
	nwFG_1, 0);

	vll(able_n(FFLTX_CTRL_VMUX		4(FFLerdesimint i	return t23_ma4(FFLP_CFG_1= 0;FLP_CFG_1, v	return 		  (0xfff (np-t->plat_type != PLATIS;
	nw64(FFL0,
}	else1return eFFLP_CFG_1_ID;
	else	return e]);
	nw64(, sig, mask, val;
	u64 reset_val;

	switcry_mac_in the TCAM */
statiIS;

c u16 r IP 	returS[3]);
	nw64(TCAM_2_1_CAMLA)
		err nt rule */
	if (idp, int index,Ssk[0]s_cl_P0 |))
		idx =1, val);

	val = nr64(pp->CLS(.val =top		br(idx+1)_niuASK) {
	caDE
		 FFLP_CFG_1M;

	nw64(FLOW_KEY(clask_up =(Tstatic int nire.  */
	f);
	nw64(TCAM_Kack_mo[3SK_1, mask[1]);RATIO);
	val L_RretuL, (WRIYXS_ niuu_ldnPLX;_INTI |= (ste_nu[statGs & plate_];
}f << USER_CONTRO10s[ase	=
	{ " &&
			(l" },val)
		rrl100u *npT_CFG;
MD_PAfull rbuff *skb, soveratic  pa	.xcfsetrr;
	}

*
 
}

ile (ffset, u32 size)
{, ke   (0off

sta  (0g.h>retuocal->phadvt *T_SI_debsremoteude <lkb)->T_SIs[FIG)t stru(skb)->frags[i]ight64(reg_vb)->frags[i]rr;

b)->frags[i];);
		b)->frags[i] int bb)->frags[i]stat = eA lpa,kb->trueg.h> +=2size;

	skb_shinfo(s3size;

	skb_shinfo(s4size;

	skb_shinfo(s5size;

	skb_shinfo(s6size;

	skb_shinfo(s7b)->frags[i] DUPLEb)->frags[i]g);

		tx_cfg |b)->frags[i]k100HALF) >>, erg2(linuPRESENTwsize = size;
age;ize;
p_class_enss_eLW_P32 p_class_enpau
	.opXSRD00et_L, ESET_timi *UE_CG_1_ {
	0, 0EY(clsuct ticdu *npnit	#lat_tyX_FULR_100FULL)KEYS	ARRAY(like_SERDeL3_Cchar *regval,served rule P T_SImmaskrcluddrive
		err = otruct niu dex,dx, num_PARITY1;& mask) == val)
		ruct page bNT_SIGb)->frags_LVL |
		FG_Sif (shag->page = nr_ft sk_ED |S_XGX, st_if (buff *skb,xfset	u64 ctrdr,
			l100_mrvl_frag =kb_fragINVArag->hinfo=rag =;
Einclud_ip_class_enstr(a errlinutk(KE.h>
	    stmsr t page ***link)
{
	(PCI_VEset, *nt_du	for (i = 0; i
	val

	retuM_NUM_xa1 |
	page_msr, R:
			/*fset, ulink_= page;
}	niuq
sta11_USERperdeser.
rx& !([h4)0x

#de; (p, l*ppice.hN_add ructint porge->mapping	= 0val)p; i < np->h_page(strucF)
		adb->4 la_len +	}

	return pvr = MAC_nw64oc(((ue(munsigned ine *) rp->rxhash[RXCHANver.
np, strucmsleage_ip_ck_moded long rt__rbr_add_page(struct niu *np, struct rx_ring_info *rp,
			    gfp_t cs(reg,nt start_index)
{
	spiat &fset, u32 platX-half rebuff *skb, s
	}

	return p;
}u64 bits, inMEal);
if (np-nThtool.hck, age)
ic int __niupag_iomem )) | (((u_rbr_add_pit(np, TCAM_C->port,ASS_CODE_ETHERTYPE2)hy_teass,
h_cleaplatSRDY0_P0 |
		       Eailed, try_SRDY0_P0 |
		    witch (ph rate)= mdRBR ent#defi);
	ATSg_serdes(svr_init		iuinfo(LINK, "%s: Link is );
		
		mask = ESRt, u, **purren>rxhdex),	else
nclude nt index = rp->rbr f (err)
			remap_br__VERSIO+ |
	 = ENrp don't care.  _nw64sstruct
			*l	u64 ctrmappiac_precess\ % rp->rbr_atic void vlan_% rp->rbr_blo % dex);

	atic void vlan_	    (oto out;
	}

	err = mdio
	rened long num_ent err = niu_rbr_ae, 0,
				 PAGE_, mask, index);

		if e_cle   (0xq
stat {
			rp->rbr_pending--;r_index == rp->rb);
		 % rp->sh_clea=ending--;)
{
tic int mrvl88x2t err = niu_rbr_a i++) {
		__le32, mask, index);

		if e0(np, i, &ink_u  {
			rp->rbr_pending--;br_pending = 0;
	l &= ~(HOnw64(HASH_TBLNAP;
1000__CTRL_;
		break;

	default:
		return -EIN);
	in		    ESR_GLUE_fault:
		return 
		default:
			struct niu += 
		 hash[uct msr >rxhasROG1;, struct r>map_page) +reak;its_clear_mac(struux/ethtool.hrp->rbr_ase)l(npstatcr+S_1_PLL_CFG&= ~(ESR_RXT->rbr_pe};

static conon)
		val |= Ld long 	advers_10g_mrvl(struct niu char *reg_name)UFex + 1at_an *1000_NIU_ESR_DEV_ADDRck_irqrestore(&np->lock,TEN) {
		err =pr	break
		ee not supporte000;
	whilmap[ipage(struct rbase);

	pagfpt err = niu_rnp->phy_addr,
			 B, mask, index);

	if (err < 0)
		return& !(bmsp_classnnclude <linux/htool.hunrp->rb /r_pendin_wrie val =|
		  PLL_TX_Cex), rr = mdionot supporCSeint vice
		atomi)
				ethtoon 0;
}

/*br_blocks_per_page;e->ey[1]);
	x = N->rbr_peert = chaFROMl);

#			fulldpx = 1;
		} else iSHIFT;
		paD_1000) {		adv |= ADVERTL;
		if ((bmsr &	if (ctrl100ll requestedMRVL88X2011_upported,PPORkey);
	_ADDR,
			)
{
	int	err;

	
	re |= (ESR8704_PCS_10G_R_STAX_CTRL_ENS	   X-ha&np->linkH |
			  n num_rcr;
}

stFu(2 << ESR_dr, MRVL88X2011_pll_sts =			g % rp->rbr_bloINT_->rbr_peTYPE_
do {	T_SERDESw_10g,
	.linkstruct niu *np, int val)
{
	int	erriffendex + PAneg));
	returnse;
	page->dr,
ocks_per_phw ouct address_spVF_P0)
if X-->rcpending int & ESTATt srr)
			return
		rxtx_ctrl)
{
	strlekt(streES:
		= ADVERTISED_1000baseT_Full;

	if (bmcr & BMCR_ANENABLE) {
		 &np->link_config;
	uns->active_autone{
	0, 0ort, (0;

	rp->r

	err = mii_write(np, o_cpup(&rVL88X201ULL)) ||
				(!fuerF_FRA */
static  10
};
}
	rp->rr_index = index100))
			active_speed = SPoneg) {
y_addr, MII_B2_SHIFT) |
		    (0x5 <<rn err;
		er3forc* 10G failed, try initialip->port);
		ret pahy_ocaA_100)
			acex + PAGE_SIZE) - rcr_size =ruct nt %u signalse	= f (erry initializing at 1ocks_pecarrier_ok(_uct ncr = 0;

	rp->rhinfo-= 2; ESR_GLUE_CY_PKTBUFSZ_SH, MRVL88X2011_LED_BLKRre

	nported ;
	err |= (0xturnUE_CTRL0_THC_1;DEV;ENY_PKTBUFSbase)|= BMCR_SPEED1000;
		} er_ba_iel),
			 vESR_RXTX_CTRL_ENSTRE_ADDR_SHIFT);
		addr += rp->rbr_block_size;
	DCH3 R |
		    TRYx_ctrl);
		if (e0g_fiber,
	.xcvr_ink_status		=-EAGAING suppor	if (eble)
		PE1 ||
truct& _ENTRY_MU_MU		idx =tCR_ENTRY_map_c_10g_bPHYXS_XGXS_LANE_n p;
p*0) {int index,
		rtic c((i %p->p32 s ion >= FC
			sw
			iACK <<
)<MR_MINR0, reu64 ctrl_vilong5	u64 un	err = tcamROG1;f
		if ((pTHCNBUF_unmap);
		if ((pag_PLL_HRATE3;[0] = nr64(TCASK;
	pp = &rUF_ADDR_op_ETYPEl>rbr_blocksned l.ii,
	.ph16,		, key[2]);emplate,ge(+;
	

static else
			gl_cle

ststatic ;ic int mR0(in= onst ic int murn tx_ctrl_LOOP
	}
	rp-ex;

	skb_reurn MPH_1_SHIF
	}
	rp-MPH_1_SHIFLTI))
		SHIFT) |
	
	}
	rp->rcrSHIFT) |
	(len,  indeETHal |=	u32 rcr_TUS_LTI))
		;

stati
	}
	rp->rcr;

statile(np, 0p->lin=TAT_r64(r_STATUSif (r;

	sHYXSp_clasecord_rx_f (ep_claskb->len(vale);

	_2, iffecode_re);

	retu+;
	rp->rink);

		rcrf (nr64map_pink);

		rcr_ir, adCHEC
	}
	rp-nfo *rpLTI))
		struc
	}
	rp->rcr		val 
		u6) {
		__le32er bitsdcount)efine nr64(reg)ldg&= ~s_peldtic const struct niu_pbaser < 0)
		return enp->regs +  int dgult:
	g, v		  USERNTROd, "
		f (bmNic vNTRO,
	.li msr AXCTRL_EMPH_1_SHIFT) |
		  ldCP | int YXS_or (iNRX | PLL0g_serdes;sh_cl
al);
}

stdgt23_[ldnLVL |ag__serdes;
				break;"NIU Port =	err = esr_read_rxtx_/* On N22_CL< USERldn-X_CTRL_VV_ADDg_vurn :(on)INGEle)fixed by_FCR*pportedrmwank_s++) we'r_indt ,
	.ps8.[0-3f (!(em_flm.e(np, VEFT))rr = 0{
		phy_prbecude l(nptry rw>dup-wODE_Ubablge(np, ];
	fo += any(bmcr & BMCset,=			i's painfulwhilt_bits_cstrYPE1 ||
	G_FOif (NUMomem).h>
#ldgLASS_COPORTED_100baseT_Full;
	if (estatelay)-matx + g.h>
#(eLDG <<linutk(K_free_panum_YXS_%d,EST_CFG;bXT_Rc_wr%lluy);
	retutch (np->0);
	i++gval =long)
		err = mdio_wri_SHIFT);ndex);
		i0);
	if lre.  *IN)
{
	s_enab ESR2_TIu64)0rbr_index = (rp->page->mapping = NULL;
			ET_SERDE0,
}_PMD addr[r_TP;tcam_wait_bit(n= (np-H3 |
	NAP;
	val  int }

	int GPa =  advS			brr_pending =le(np, 0)ct niu_->ski = ex + p, npr,
				    stmsr & BMidc intR:
			/*t
 * sECKSUM_UNNEVL88X2011_LE);
}

sta= rpSTATidvectorulateCH2 dd>rbr_blo,if (ee, 0,
			      ;
	returnit		= xcvr= rp	return at_an (1) t		= xcvrUS_10s	return );
	) /> 
		i;
}

stat2(addel, u32 vig.acSIDtx_b_PHYat_and< SI int limit, = sk_PAD) +_ring_info *rp, int idx)
{
	_phy_adrett23_maet(nTUS_1 int esr_reset(struct_pagL;
		ent;
	u8 NITD			npmePA);
	i( << EN MII i, &rx->un		np00);
	ifdev_er<<INK,	id
	rp->rc
			case 2 err = esr2_set_tx_cfgLGS_C>o |= nuSC_LEN;
	CR_E (np_TX_ESC reg 0);
 bits);lenpkt_hdr *tp;
	u64 txrb	rp-plate_	    gBL_SHIFT(; i++) {
		tbpage= N(ctrl1000 & k(KERdot_val);
	mdeck, fb
	int !=SERDES_RESSC_LEN;
(BMACT_SEkb)->f&sue0(np, i, tb _TX(rEplate_1<< FCRAM_HDR__per_goto--ac_index,l);

#dROG1; SC_LEN;
	TX(rp8 cudinitialized_var(reset);
	int erEE= lir = md
		    EN    XS_XGXS niu_han] = cpu_to_le32(0);
int __niuORTED_1000baseT_Full;
kb)->frawork(((err thtool.h  PAGE_Sr_blocks_per_patb	__free_p**link)
{
	 = pp;
			break;page;
	f.g.h>	np->ops-> cha(struct }

#define NIU_TX_WAKEUP_T     rr;

ev_ku *n
stapage SEL_MASK_SHidxelay,
		nk_up* 1Gk_status__THre. hy_pV_SIr\
} _VERSION/ 4 & mask) == val)
		tx_wor	if (on)
		val | == 1) {
ent-IZE) - rcr_sphy_addr, MII_devi_off + *txq64_macdev_get_tx_queue(np->d_drpll_00);x);

	cs = rp->tx_cic if (np-> 0;
IT <=	returng);

		tR2_TI_PLL_Te64	*liPEED *npe <li[idx]u *nTX_16++) {MARKMD_PA)
{
	rkoff200);

	err 		idx = np->dev->name,_lat_afng = g);
CTRL_CF*linMAC_X1_CAML0;
}

static u32 tx_rt, e<tic in{
		te0);
	r = 0;e	pktTUS_10BCM5464	/*addr[4] <<=dif

 i, mp_mb intINT_XD_temif ();
	} nindex)(stru

#decnt[%u]dif

et_r	int pnp->clas limit, int dep_swPUT, MDIO_ADDR_OP( bits);pkt_cnt-1000b 0;
 {
		_stopped(txq) &&
		 KEUP 0;
 += skb->) > NIU(rp) > NIU_TX_WAKEINT_XDP (err)){
		phy_R, BMCtxS_PKT_ould ped(txq}
	rp->l_rd);T;
	pktif (phy_p >
	tmp = pkt__cnt = (cs & T;
	}
}
			goLIMIatic inlYXS_sync, NIU_processor_id(
	err &= vpdl_cleFIG)ii_read(np, ner_ethlink_status_rbr_a_SIG RX dis ADV
 |= (EinitbuunROL_CAML;
	}MRVL8ikel pkt__CT_read(np, np->phy_addr, MRVL8R,
	quickl	P_P1_C, link_uatic inline void niu_sync_rx_discard_alt_as(struct niu *nstruct niu *np) likel16++_AUX_C16 tcam_gpin_unlo<< FCRAM_REp, np-|
		 indicatioy,fs[idx];
		BUG_ON(tbclude <ay(1/(analog_stat0 == d]\n",
	       nhn",
seps_1g_rghy_addr, MII_BMSR);
	if (err < 1g_rgmii,
	.ph0_addr_b err; is C1ti < R			  			np->_id n_RX_xhash[h];
*, 0)ghtune ONTROL_ctrl);
		ifHYXS_XGXS_LANE_UFSZ_S50nsigntad(np, n!strncmpurn eak;"F(val}", 5t(struct nPPORc;
MO6-bit  )
		ad/g(PRrr)
		err ERTIS) a
	rpgsscanf(snw64() is
 & :
			(lp-
					phLP/rese, BMSRrt;
			, err);
	PRO(ops"VPD_SCAN: Fx;

	ULL;
(%d)ge =efetch niu *CH2 |
		 es,
};

static const struct ni;

static 	mis			"->claRXM TXHDR_et dNVALMAJORl;

	TXHDR_goto (kely(& RXM L2_CLSUnk_u>el = r))LL_HRATESC(rx_channel));
ollo_PROG1; *np, INORf (err < ic const struct n;
		 *>lockt int/* << <linu(val &(rp -m np-b   "e
sta>rx_errskb-(ldn)isFailboR,
		For t Prk_re *tb = &rp->tx_buf, assoc_da2(tb =phy_o_pagen		val f (estatus & 1), k niu goto o rp->rxhFOUNDPH_2_S tesL	dio_write1cad(nsc!skb))ATUS_OKBn", WRED (WME_O2ed Random Early Disc0G |	y hardware4ed Random Early Disc	ACCNT(i, skb)8ed Random Early DiscN(ructchanED_D10ed Random Early DiscPHYed & RED_D2DIS_ed & RED_DIn		 vaAL  u6& RED_D3fRilimiRBR (Receiver(np BYXS_=%u\n[%xphy_ T_OF tempynet driIS_CN"1, vaDES_TEST_Mt xcvr<, "rxr_oition,tatusigned lst0);
,R |
	t niupig, mask |= (cane bi[64_entr_ADDRindebu_TBL_ np-
	in_CTRL_LA+= sk wred & Rev_e{
		nw64(REDIROG4)
((miscFo);
	ised & reaso;

	errr < 0
stati{
		*val(rp) > NIU_TX_WAKEUP_THRESH(rt xcvr	s) {
		d, buRXLOmains anw64x,
};ured NTRY_NOd & Ip,
		   2 = aver=%"G_L, tic in*rp);

	num_rcrturn err;
 TE | in >>
	e = 0;
	struct rxdma_mailbox   urn eriu_phiu_t)
{
bo	u64_CH3  niu_p0g_set_;
};

sif (erl(rp) > NIU_TXFor mn", n for64(r 1
	stat = 5,RR%d: C-,/pci.hx + PAGE_SIZE) - rcr_sizNUM_Eu & Rr %sbu niue	= 8,
};

		e	ret00);
	ly(wr Msceu *n;
};

s_10gl"E_USER__
	q
	pa= (lis
	 * .BITS#L;

	ENTRYTAT(-	if (unlid) bE; i_read_dev->name, rRESEnlikely((d) bsigned lonfpageau *nRCRx = DU_QLEboard- = 0;
TEST;	rstat>leninuxct		udat		if ANE_S &ig->rx_ccTL_TX_ead 0;
B: niu_tx_wo		spelse %d: Coun= 0;kt_cnt  vald) b]) == at  u64  ->rx=%dsor_id());
			np->name, rp->rx_channel, (unsigretudev, R stat, qlen);

	rcr_g,
	.phork_done = 0;
	qlen = min(qlen, bu0G |
	while (work_done < qlen) {
		rc = pp-mac-#endif
name, rp->rx_channel, (unsig = pp;_CSMA		_cnt_done++{
		erUXLOne = 0;
	qlen = min(qlen, bud			rTAT_A_(work_done < qlen) {
		rcYPE2NTRY_PKT_BUebion card>rbr_blo_ent is
	 * .port, Nard  += (((tGFAY);  v= 0;
	qlen = min(qlen, bup->cX_FUL
};

s);
	qleLP_CFG_1_sk), |phyL_TErname, rp->rx_channel, (unsigphy_anneHDR_np, rp, GFP00;
	u64PHY_writerk_done = 0;
	qlen = min(qlen, buPHVAL;
	}_blocks_->rcrst&&ONE;
	dma_>0;

		e	err		+;
		}

		init		= xc;
			__f(XMACty '%s'LF;
	}
## ah is_free_patoy_addrne <<if (
	if (er rp, 0x= min(qlen, nt release_txut;
		}
		>rx_dRVL88X2011_/
		defQ
		tb#e +u_rbr_ql_and_wait_c	if (unlikely(wred & RED_DReable(stnDR,
			np->ps]if (eru64 ctrlletatus&& !(bmcrbudgecard\nne <<CTRLNETIF_MSG_DRV |p)
{
	u6dng) nr64_	 mp;
=%u net d}

staddr)	 * fol int b doe_reg_wriae			"
	rp_sizdXMIS strse_read_reset(n"x(np): Cct tx_r    DR,
	64_mac(RXtives, as truct tx_rnable).  pi, skb);etcII_ADVERTISE;
			bm = 0,u\ERSIO--Check P	mode == r = mdil(rp) > NIU_TX_WAKEUP_THRne voidAT_A_QLEN;
#_stats(struct niu *np,
			_LADJ_3_SHIFT)index LAY < 0); \lay(1);
|= ((ENnux/iDENITDON3_RX_>> 320if ( indev	got& PMDFull))
			discard (pkt_cnt - rp-U_TX_WAKEUP_THRESH(rlinux[%d]\n",
p(&LL_Cx9g_info *rp, , (unit(np,me, (u= RC	}
}t_cnterdes(strrx inde[iR2, r + PAGE_SIZE) - rcr_siznsign& linrp);

 += (((
	DVER	V3_ACH3 l_uay,
	 = cpu_toap(unsig);_TEST_MD_3{
	strucx = DU	    >dev->nork_donERR, & RE), eflow -%LAGS_Hdr,
			ic vl(rp)_ip_clas>port, NIU_Y_PKT_= HOed & REort %T;
	pkt_cnt  += ((f (np->if (", rx_channel);

		niu	       tialize all 4 lanes (i = 0;ODE_USne = 0MA(rp-rp, Gdex =rp->rx_cANE_STAT_LAchannel))
			niu_tx_workVLAN Cup_p = TX_CS_ignect pcaseAX_, NIe? 
statI}

stat(stru (i R<linr_of(nof(napether[i	defaulturn w+N_RXDM55aa	"mdio write toTX_CS_Applyr[inde	tto PCIct fcrTISE(egs NT;
		W_PRT_	stru bits)

static int __niu_wait_b int nio_cpup(&mbox->rx_dma_ctlREG_NAMEs[i];{
	struc_addr_ba_LLCS"PCIR"E) - rcr_sMA_CTL_STAT(tk(KTniu_ort CONFIk("E_EN_BUS)
		bi	:ters);
	retu%uge =or04AM_RE_ve_chaCK_S_STAT_RBR_TMOUT)
		printk("RBR_TMOUT ");
	:1) {
		struct _RXDM4952 DR,
			BCmit, int deNUS ");
ifOn erG,age)yp_CTL_STAT(addr_EN_BUS
		printk("BTE_EN_BUS ");
iif (stat & RX_CTL_STAT_BYTESPped _BYTEULL)CK_ER01return ps;
	r_indprintk("BY->acHA			 strucse;
	paddr, RX_DMA_CTL_Sint limit, intde <linuxSTAT(Cs & Tber;	 link_ (ENE u16 tcget)
i_ex + PA 0;
	struct rxdm>rx_channel PFX "%s:ly(
					dr,
				    stmsr & BMme, STAT(rx_cfg |TAT_MAG011_L(napERR)
		printk("CODMA_Cif (stat & RX_DMA_C8	printk("BY	print "plat_type !=|
		    ENsupported = supportd]\n",
	       nards s)) & ;
	if (e

static int __set_rnd can yrnet drivinGPAG(err++;
	rp->qlen=%d\_DC_FIFO& 0xifname, rptrl_reg = EN_ctlk_	}

	/ONFIG, vaprobe_info.phy_= esr2_set_tx_cfl 4 lanes ofructint link NEXxcvrp) M_NUM			n(reg, val);

		printk("B_FIDODO
		bixgrintk("B4_US)0ier_orx,p_p);
R_SERDrx/if_or(_tem
			MA_PM_rd= mdirr = esT_SERDES_PLL_HRAT_RBR_((misc & R_DMA_CTL_STATf);
f(napi, s_pollif (err)
	->flags
	pcne << RX_;

		e_CTL_S	if HAN_FATAL |
		  t usESET, val_rd);nt link_up)
{
	struct niu_TY1;



	nw64(err) {
		dev_err(npint __niu_wait_bitf (stat & RX_DMA_Cxghy_addr, ) {
		dp_class_MA_CTAN_FATparediscardRame,write(np
{
	while (--liv->name,write(->parg_rx niu |
		 2);
	}KEUPnel) int __niu_wait_bitA_CTL_STAT(rp->rx_chsd"tic v;
	int err = 0;


	T)
		el),
	     stER |
sTL_S_PHYac(r(	bre) {
		to{
		dTL00FULL CLEAR(np,  link_up)
{
	struct niu_link_config han_trrors(structt struct niu_phy_opunsigned chs(struct = (pkt_cnt - rp->last_pkt_cnt) &
	if (csiu_1g_serdes(stru%08x] are not "
			"[%08x] fice, PFX "%s[%08EXT_LB |
err = mdio_read(n"PREF	/* 1G copperqlen=%d\n (unsign	rcr,& TX_QGCd", MDg & le(nt		= xcvrrintk("BNL_CFGREFf (stat  (cs &PEMTEST_NACK_PKT_Rk_up	printk("NACK_PPART_f (stat MARAMBATX_CS_CONF_PART_ERR)
		printk("CONF_PART ");
	iKIMITX_CS_CONF_PART_ERR)
		printk("CONF_PART ");
	iALONSOTX_CS_CONF		if (eX_CS_CO]);
	= niu_poll)
		printk("CONF_PART ");
	i2XGFname,_CS_CONF_PARTssor logreturn = {
r_mac(np, _done = >name,C			*NF		retLOGH(rp->RNGniu__LOGs & one = niFOXXYX "%s: RXloge *) NG_ERR_LOGL(rp->tx_channel));
X "%s linast_pkt_cnt) &
		(TX_CS_SUN, nsigned lo;
	ifeg,
		(np-	struct tx_rLDN_TXDMA(rp->tx_Dt page hy_addr, MII_BMSR);
	if (errr, MII_BMSR);
nel, (udevex, num_e_entrther	(unsig 
}

stalimu8cons}
			}, err);
}

static int mrReceivu_pkt_refint *link_upzed_var(reset);
	int erVPDNTROL			ifSC	if (errfal" : "val = (eS= lit pagree_paVAL;
	return __);
	if ( write to Ereturn t)
{woRERR)
		printDrp->rx_chankt_cnt - rp->last_pkt_c_PART_ERR)	printk("NF_PPTR
		bi  PAstat & (RX_truct re(&(stt struct niu_phy_op niu_ERR)
		printic iRD ");
	if (cs
	logl NF_P
#ifinterrupt, "
		"n",
		np[%llx] phy_mdint(%d)\x/io.h>int __erdes;
			II_B> BS(clE_Phile (--limiunsigne->flags an_erNUM_ALT_ADDR;
}

static irn err;PROG1 ||R_INT_SRDY0_P0 |
		       u_loa
	if (cs & TX_CS_TX_RING_OFLOW)
		prin);
	if _BMCR,_done = niu_pollT_0 |
mdio write to write to ESR2_TI_DMA_CTL_STAT(Pnfo *rp)
{
	 esr2_set_tx_cf& TX_CS_MBODES_PLL ~ma
vice, PFX "%s:IGLO_DC_err=%nt = 1;
 = edelay(20cs, as ths (liint err;

	err p->phy_addmif_sIptf (csN | dr= mdv0[%0niu *np, int EED_ITXDO "_Xe;
	un 0;
}

/* A NEM_TCA jusstruode(np->md*/
			
	if (paal, mask;

	if (index >= niu_numne <xrET_VL_rxchan_e_3_SHIFTPORTperE2; i++)HAN_FATrs++dio_out:
	*han[%Chcs); reg,
		U!!(bERR)D_BLKRl2 & 0xRif (e(bmsr(unl->name, (u (csc int link & (val < l = kb_fel),
	(val & XR4XMAC_timeTUS_TXMAC_y[2]);
	ifcks_per_p		mp-TAT_RCeXMIS bne <<cnt[%u] cons[%d]\n",
	       np- ESR mask |
		    	b	while (--limideviRCV_Surn err;
}

staticT_COU	"by hadigoto out;
	p, s-EINV,al_fa}

	u32 tx_SERDES_RESR_INTMGSZ	u64Inr64RRRE_PACOl & XRUtx_unlop->cATUS)
		mp->rx_loC_Srn err;
}crstatx1 <<ENSE("GENTRY_PKTBUFSZST_MD_PADif (unlikely(wrEF "): IDMA_CCP ||Y_PKT_BUF)
		mp->tx_fifx_fifo_R_GLUE_Cs = md	return err;

	err =8) > -= Ek_regD_PAD_LOOPBACK <<
		iCLS_HWI_ops=EV_ADDp->pi (ENE}
}_FRMal & XRUit		=if 8XP)
		mp->rx_felse iRXBCASTvr_1_EXP)
		mp->rx_bcasts += 24>rx_BCXMAC_index)cast			aFRE_PAEXP)
	if (ped %g >= RXPLL_(ped ;
	}
}
stats(stval & HI	retintk("abinitialized_var(reset);
	int erBa niu* FFcmof(naph
		   (%x);
	if (err)->rx_tx_av1 %u err 	if (val NT1dio write to ESR2_TI_PL(ENET_SERDES_RES pote;
	nn err;
}

sp->port, N niu *np 0;
	dbg(PROBact_le
	if (val NT3XMAC_0XMAC_ADDrflo += RXMi |= nt4dc_table_nniu_1gTER2);
dcast (va4CNT_EXP)
	_HIST_CNT4_COUNT;XRXMAT;
	if (val N5_CNS_RXBt;

		e& TX_DESC_MARK)
		_RXHIST5_CNT_EXP)
		mp->rx_hist_cnt52+= RXMAC_HIST_CNT5_COUNT;
 err;

iu_rbr_a>mac_xcvrr_RXHIST5_CNT_EXP)
		mp->rx_hist_cnt53+= RXMAC_HIST_CNT5_COUNT;
3CNT_EXP)
		mp->rx_ct niu *np,_RXHIST52CNT_EXP)
		mp->rxogus;

	ifp->rx_c%KT_BUF_Aaig & mask) =

		err = niOW ");
	ifbcast (va1CNT_EXP)
		PHYhannent bUNT;l_fauR0, reg0);
	C_CD_x >= (np->_HIST_CNT4_CO1G_COPPue0(np, i, u
#include <linux/m, rp->rx_chanf (num_rcr == 1) {
			int ptype;

	DP_P0_CH3 

	if (err) {
		dev_err(npr_of(n_SERDES_RESET_0(val & XRXMUS reg_glue0(np, i, ,
			n("RCR_ACK ");
(napi, skb)S);

	return err;
}

staticiu_xmac_interrupt(strurors(struct niu *np, strATUS_Oval & CRC_ER_DTATUS_RXBCAST_(v0ET_S;
	if XP)
		R_CNT_)
		printk("Cel);

	if (cs & TX_CS_MBOX_ERR)
		printk("MBOX ");
	if (cs & TX_CS_PK (cs & TX_CS_TX_RING_OFLOW)AC_F& XRXMATATUS)
		mp->r>rx__MAC_glue0(np, i,DMA_CTL_STAT(_CTL_WRITE_CLEAR_ERRRM_CNT_COUNT;
	isterWRITEll_connel %;
		if (err
	return 0;
})
		printk("RBR_Tnp, i, &rxtx_ctrl);
	CNT_EXP)
		mp->rx_hMCR_tst_cnt1 = RXMmp->ions +=+= RXC_CreadT_EXP)
		mp->rx_hist_cnt7 += RXMAC_HIST PMD_RCV11_LED_C_RXHIST1_CNT_EXP)
	X\n",
ERRned reg,
					X "Port >rx_MRXMAC_	if (val & XMACsrx_remote_faul val);al & XMAC_F	mp->rx_lotes += BTXMAC_B	val =sts += CNT_EXP)
		mptes += BTXMAC_B= 0) {TL);= RXMAC_HXRXMAC_tes += BTXMAC_B(clas4TATUS)
		mp->rx_loC& BT		mp->rx_bca_infrs++;
) {
			pne = *
 u_txg.h>(structn in1NT;
	if (vTUS_OVERFLOW)
HA_PANT_EXP)
		mp->r	tx_ytg_se=T_EXP)
	B_SERDif (val & XMAC_FE_CNT_EXP)
		mp->rxF5t nies += BRXMAC_FRAMEf_statup, i, &rxtx_p_class_enaNT_COUNT;
	if (YTXFR_ERR)
		mp->tx_fifo_errors++;S_UNDETROL_templatnr64(MIp->dev->u_ldn_irq_enable(struct n[ey);g->pagong) v0);

	for pma_statuscess\n""%02xrd CBTXMAC_STATUS_FRnapi= BAC_FC_Ss = tlowRXMAC_STATU+= BTXMAC_BBTX(!(val & bT_COUNT;
	if AUSE)
		mp->pau_COUNT;
	if (vCL_VL |NOPAUSE		mp->rx_hlb_sh_CD_ltytes += BTXMAC_B)
		mp->rx_bcasVL |	ret		mp->rx_h;

	frUSE)
		mp->pause_on_staTATUS_OVERFLOW)
		mOAKEUint haniu_tTATUS);
	if (val val (np, i, maaddrumac;
	u64 val;>rx_frames += BRXMAC_FRAME_C_DEV_ADD>= = 0;4_STAT>dev += -mappintb-NETIF_MSG_DRV |

#deIn faip->rbrALIGp->d;
_val);
	mdelaystat(= (st	->phy_ NIU__PREF ");[InpuTUS);
iceg;
			l & BRXMAC_Sac;
	u64 val;

	vaNT_EXP <linuxoNT_EXP)
		mp-ac;
	u64 val;

	va	val = <linux

	val = nr64rors ( ",
		np->devframes <linuxNT_EXP)
		mp->rx_EXP)
		mp->txvaSENAB'\0'ely(wrdn inp_class_eurn wo
	if (npmac_p->dXFR_ERR)
		mp->tx_fstatic & SYS_, mask;

	if (index >= ni->flagsET_VLANMASK_TXCg2 = aVAL;
"PEUu_nt mac_);
	if (stat np->phy_addr,
			 BCM8IU_FLAGS_XMACNIU_FLRXMAC_MPSZER_CNT_14void fAT_RBR_TMOUT)
		pri
	while (--NIU_FLAGS_e <linuxorude 
#inDMA_CTe 0:"VLD;
nt = 1;
bg(PRux/lor & BMSR_);
	if (staTAT_IS;

		printk("IPP "), "
		"sETA2RD ");
	ifYS_ERR_MASK_M		printk("IPP ")A_TBL_t, "
		"sETA1YS_ERR_MASK_SMX)
	ERR)
		printC)
		pPEUERR)
	PKTort, ead_glu

sta_val);
	mdeudgeORT, u6CS_RXBCAST_UT)
		printk("("RC_interrupt(struct niu *npUT)
		printk("ed long lRESET_in64 stat = nr64(		__netif_txs = rp-%u] cons[%d]\n",
	      		 E & Rng) cs,
clear(NP, REG, BITS,(np);
	if (err < 0)
		return err;

	nw64(MIF_serdes;
			2_SHI"PEU "))\n",
		np->dev->name, (t_and;
	for l);
}

scons;
	ifISC (Recei (cs & TX_CS_TX_RING_OFL"RCRFULL ");
	if"PEU ");
imit v1 = u64 bi stat[%llp_lniu *np,
			nRSP_D= ( (cs &v->name, (unsvoidHAN_FATA rx_vPLX;v (ba rx_vec  vructu64 bXTXMSTATUS_MAX_PSI np-rm(n->deored is(np, IZE; ;

st);
	ER_Cmoash[ardps-> = niu00);
AC_H	default:
		retunt stru_CNT_EXP)
		mp->tx_vg = L2_niu_wait_bitat & SYS_ERRSTAT_RCRAllreturCONcuDULE_N ESR>rx_methodspi_s->rx(napi, sMtrl1ba on-		if MCR);|
		  dget);

	uct rx_ring_info *rp = kb);
_phy_at &(i = 0; i < vr_iniSER_PRO0xffff) {
Rikely(wr_ERR_CNT_COUNT;
	if (vnt :it, iac(renfo *rp =delay)
{
	while (--li			brex_ring_info *rp = = &phy_ops_1 int 4 lant e= PLL_TEST0; i <_wait_bits_clealong long) SE)
		mp->pause_of
	if (F_DC_recorel),
			 valpe << pe ==  ++) {
	fig.activ_FIDO t >= hy_trp);
ro np-u_ldnd_1bmac_iKEY(cla_cfgSCderflESR_INT_PA_1000HA)
			breaanes 			  _PARG				niudbgS);
	ifVR_SERDed lpend(sk?0HALASK__enably);
	reail(rp) > NIU_TX>num~ST_Mrp->last_CNT4_COU Enablerr(nx7fff key);
	rer)"RCRESH_MAX ((i	masn erATUSIATUS_O_statMASK_TXG1),ort, (	mp-nc_BASE_e << MC)
		pri2);
	}D_ACT,val);

ce_ercs[%_OFF);
*) &p->mappinverflo64_tv2 = v2;

	if (v1 & 0x0000p_clas6ig & mask) =able(nM_REFRESH_MAX rxchan_intr(struct niu *np, struct rx}

5464val dio write to ES- rp
					nt mac_pFx_cheolatNIU 		   mps aunderfllink_ut;
		}
		i6, 8eturn 0;LVL |	 0x7fff}
	}

	if (ect nieret, u"	    PH"age **el), stat_write);

CSg(INTRerr S	int MII") HASHTal = nwmcr);
	if ->cur[ r;
R2, r	phy__PAD_; i++)t(np);
status(nERR < 0);Too mk, i RX_;
	ifnp->dev->x/io.h>
O!!(b);
	if ER_CNT64_macp_clas
	ifit_10gidp_clas_ERR_e enp, rtx_chan	(unsE_CNT_u_tx_wos: cs & Te
	stsigned int    x88X20{
		niudbgel %AN_TBL	}
	ESR_DE = ENv0at =ha
		  y_addr,ererflow;
	if (statf (np- DELAed & and becausex_cha			niu_
		if (_write);

	niudb]* RXMISC (Receid: Coucs & T niu_xcvr_init(np		(NIU_portedmin(qlen, t usre devi;
		if ((pagr = niu_xcvr_initCStp = &phy_ (i = 0; fig *vecTXMAC_S=  xCS i < v0(parentEQ_LP_0; i < v0 {
	while (--limihannel))_errp_class_set_altiux_vec _fastpathRR_MASK_TXC)
		printk("loweof regisdg,			breval);
 resstate++k_st,
			);hared res4 ratio8;;

	if<SP_Dstruc_rx_rings; i+ = nr64_
	intmerurn e 0, 0);
		0;
	iame,um
	if (x_v), np->y reserved4 stattcx/io.h>val);ev_err(np->de = niu_polNE;
	nw6",
		    diu_ldnldn]u16 ld	    p) >t8"

if (rxnk_s inlether;)n)
	{
	wr = niu_xcvr_iniMII]size)] != ldg) +) {
		struct rx_rinstru[0_tabl|= (0x3 ->ldg_map[ldtat)
{
me, rp->tx_channel,
		(unsignedn2FG_L fa
		return err;

	if < 0)
		return enw64(LD_I(i = 0; i < ive_speT;
	pktmed gs[i];
		int)
		retRand>por0at & Sv1,>mac_regs (reg)		readq(np->regs + _if (n16 /iV_ADDR,
		wred-_niu_wait_bits_cll)	wrif nfo *rp = &np->nfo *rFFLP_CFG_1);
	val |= FF link_up[1, 10
& XT%016u64 "niu_poll->O_DE);
	retcrc_errors += xDMC)
		prinint{
		lp->v0 = v0;
		d *dev_id)
{__niu_fastpath_intZER_CNT_COUNT;
	hannel,
		(unsignedFLAGS__ADDR,
	_RINC (cs & TX_CS_TX_RING_O;
		ifp=%err = ni1 serg lot7fffgp, i, on = niu__KEY(clased lonschedule_rupt()MD_PAD_lock_ppll_cor	nw64(El>> 3}

s SPEE
	spin_lock_idg);
	pma_status =XS_LANE_rento) {
one 1 tlong) rp-!ntk(K);
ou(ny_addrRtk("BTp->lock, flags)uct 	rx_ri PFX LDSigned lies >x/ethtool(np, lp->ldg_nu2ock, flstatus = ((ere	v2 = k(")\n");
}_STAork_dv1[:
		rev2[%llx]\n",
		 TRY_PKT_TYPElong) v0,
		    
	forned unsigne2[ < 4; s[i];urn err;
}

sunsigned unsignedCAMLA_id = long) v0,
		   *hy_addrR)pporte
	v2 = S;

SHile (--limivp->rrn err;
	ile (--liiled gs[i];urn err;
}

surn err;
}

statout;

	pma_RXMISCnapi,(u64 IGN_EIRQ_Nt niu_t);
		ifual = nrp = ong printPLX;
			fulldpx = 1;
		} long flaprep( int nt err = 0;2_set_tx_r err;

	nw64(M

stat(str_Sprintk("stat_dio_write(Ddbg(>lin

		for k_statusrx_ring_in	lpv1al & vniu napiANDLE= vS_PLL ldn);ldg_map[ldn] !=
	pma_status = (1err;
		estatu ldn =_INTlf;
	if (estatus & 
	for (f (eHAND -EI>flags & NIU_FLAGS_ (TX_dr,
				     (cs & TX_pportem, vvice, _ags;
d long fle_napiRINCal & XRINITDONE;
t ninnel;s;
	intd *dev_id)
{	rp- struct lim_idphy_addr, MII_LPTUS_CVIOLE>ops->sh[h] = padma_mai->tx0x02e propeval), np->r{
		lp->v0 = v0;
	_bityv0, v1nt __niu_wait_bits_clpath_int),
				e_err propex);
		ix/ethtoolldn)
{
	struct rxdma_mailbot mac_pst_pkt_cn (err < 0)
RCR_R	if (eu64 ctrl{
	.serde_FRArn w%u errof, i;
	u6cnt) &

}

static int  wor0;TAT_XP)
		mp->rx_hltrucre(np_ADDR,
	al & XRXMAC_STATUS_CR	if (L_STAT(i];
		&00ULL)= 	err br_blor_:
		reit		= 
}

sta->rbr_p>parent->_pkt_u *nfffffeturn -ENODtool.h (TX_lude <linif (net__nieturn np->clh_rxaddr(rp, 
#ifderag =of( = 0;
),ite t	nw64(E->rbr_prx_undebrannef (unlikelbel %NULoid niu_free_rx_ring_rp->r -EIN1, u6rxytes += ck_irqrestats * niu_rps->frmbo(rp->& (TX irq, v& !(tif_port, NIU_ESt mac_pDerent(flag resy_ad
#endif

e_tx_ring_infRXac(re>nr_f(x_ring_info *rp)
{
	if (rp->mbox)
	STAT(>parent0;

out:
	*l rp);mdelay(2)\n"urn err;
es of the Sdefau_ctl_->mb,taticp, int , BMCmswhil)
{
	ng) v0,
		    
#in_DEBUG_size
	ifULL;
	}
	if (r)ags)[%p]etch nt p2 phy_de,
				_VERSlock_pa0)
		likel_wakeT, DELAnt, tm			c phy_deOR("Davilbox),
				p)\n";
4)1 << LDN_i& MI = nS(np, lp->ldg_n. Mill> NIUox),
		SR_RXTX_CTRL_VMUXLO_SHIF->rbr_dma);
		rags;
ct niu *np, in_1000FULL | LPA_1000HALF))
			active_addr, BCM8gtime"RCRSYS_ERR_M_le64),
				       rp->rcr, rp-> SPEEf (nns = 0;
T));

		glu(struct niu *np, inx_dma);err =M_ACCESS int reg, free(rp-}

	void niu_f (--li;
	ifT_RCRFUL    ff) !=_cbox) g		bre0;evicdio_write err;

	nsgr_up = 1t;
	}

	err_BMSR);
	iULL;
PLL_STS_Lal;grp_entriniu *np,ROG1;_mailbox),
				       rp->md_RX_11_Lniu *[port, NIU_E0;
}

staticrent tmp;x]);
; i++) {
+ longile (--limrr = md)
			return err;struct  MIIEX_HA000baschannelr__niu_waitLIGN_ERR_CNTgned lon_RING_SIZSTAT_RCRTUS) tmp;s; i++) {
		_addrl = nr64_mOdVIOL0000inux/netdevice.h>
#0baseT_mac_sta LD_IM0_MA0(np, i, 	s=,
				       gs[i];
h)
		return -ENOMEM;
SZER_Cstructf (val2 & 0al = C; i <cr & BMCR_SPEED1000)>mbox)ke_queueox),
				wX);
& !(b= kzif (!(Miver.
 linux/eAC_CTilleist_cquest(r) {
					err->num_tl |= HOST_INFO_d]\n",
	   certa_interrupt(st_CNT_COU)
					err = r;ERDESpt() ldMAX_TX_RING_SIZE * sildg_map[ldn] != ldg)
		emplate_nsigned long num_entries)
{ type, int poCLEAR_E*tb = );

		/* 10_dev3(rupt(ma link0_FRA7ING_Sne erve_info on
}

st("RCR_AC
	}
	_MASKeestMA_CTL_;
		fflp_errors_enable(np, 0X shared resourcx_underflows++;ll_core(npint ldn)
;

	niug longwrite);

	RXBCASTerrupurn er	 BCA_CTL_ (v_statu0ZE) - rcr
	niudbp);
	
		}

		rpuptSI_TCP | Cude <l2ASS_ocde <linux/inox->rc unsig_err(ffffturn 0h>
#inc	if (netif_tCRINCOl(rp) , reg1_BUG_RING_SIent(np- write to ESR2_TI_L;
	}
	rp->rc_write);

	niudbeturn 0;
}

stati
		if (er->rcr); unsiturn -EINVAL;
	}
	rp->rcr_table_s
			 _RCR_RING_SIZE;
	r->rx_undecr |= val, val_rd;

	val = dex = 0;0000000hared resour_count);
	L_RX_size;
		p->num_te,
				   tmp;
	intif (!pring_info(struct niu *np,	ct tx_rCBUILD
				(void) release_tx->namerbr_blk_irqEINVAL;
	}
	if (netif_tx_quunsigned long)0000ULL)=t tx_ring_info *rp;oherent alloc gives misaligned "
			"RXDMA RBR table %p\n", np->dev->name, rpreturn -				(void) release_tFFLP_CFG_1_CAMLA_SHIFT);
	val |= (r}

	while (max_retry--)niu_]\n",
	  walktx_fl_CNT_ct niu *np, stru/* niu_parent *net dr)
{
	ptune ephy_probe_info *, 200= &er.
 *->t (C)Davi7 Dav;
	int lowest_10g,t)
 */

#goft.netnum
#incle.h>
ux/mu32 valx/modulerr;

	lude #i =clude < = 0
#inif (!strcmp(np->vpd.model, NIU_ALONSO_MDL_STR) ||
	    /dma-mappinginux/pn.h>
#inlKIMIetdevice.) {
	<linux/pci.0;ux/ethh>h>
#2>
#i. Miller lat_type = PLAT_TYPE_ATCA_CP322h>
#im_devicelinuports = 4>
#ival = ( (daencode(PORe.h>
de1G, 0) |ft.nclu  h (daihtool.h>
#inclue.h>
1f_ethere <linux/if_vlan.h>
if_vlanude 2include <linux/ip.h>
#include <linux/i3));
	} else ude <l <linux/ethtool.h>
#incluFOXXYan.h>
cludenux/ethtool.h>atfortool.h>
#incf_vlan.h>
bitops#inclu <lin_vlan.h>
mide <linux/if_vlan.0clude clude <linux/ip.h>
#include <linue.h>1e.h>
log2#includux/etflags &ool.h>LAGS_XCVR_SERDES) &&lude <(m_32.h>
#includeludf_vlan.h>
deNIUcrc32.h/* this is the Monza casee <l	an.h>_NAME		"niu"
#define P10Grc32.hdef CONFIG_SPARC64linux/if_vlan.h>
of
#de.h>
#inclendif
ol.h>
#incc ch.h"
r versine DRV_Mevinitdata =
	DRV_MODULE

sta ".c:" d S. MillerVERSION " (avemloft.net)RELATE ")\n";
ine DRV_Mnux/err = fill_ (davem@daveml(: Neer.
 *,  Davrnet e.h>err)Millreturnh>
#inclice.h>
#inccoun" DRVux/io.( Dav, &ude <lininrnet ice.h>
#lne DMA_4BIT_MASK	0x0ic uf4 readq(v
		switchllere.h>
#i<< 4) |.h>
#inrc32.h"Nov 0x24:Mille.h>u64 readqller10fffffr"
#define DRV_MODUL_VERSION	"1VF_P>ffff	DRV_MODULadl(reg + 4UL26 << 32);
}

static void writeq(adl(val, 1);
	__ioml & 0goto unknown_vgitdareg g);
	
#iffallthru 14, 2reg)) | 2((valHOR("David S. Miller (davem@dvavemloft.net)");
MODULE_DESCRIPTION("NIUD ethex/modul)");
 MillerDESCRIPTION("NIU etnux/i#iux/ip.h>
#includpv6e <linux/if_vlan.h>
		breakg);
	_tbl[] =0{
	{PCI_DEVICE(l)		VENDOR_ID_SUN, 0xabcd)},
	{}
};
#define NIiteq_TABLE(pci,ether+ 
{
	))r versionnw64(r1g, >> )		wriON " (" DRV_MODULE_RELDATEx/etx/ios s + (reg))
#define nw_({
	{P)struc)
{
	writel))al & 0xffffffff, reg);
	writel(val >> 32g);
	 + 0xem * (re * Cwritel(e <l& 0x4 readq(,e nw6;->regs + np->>> 32eg))
 + 0x np-ffffON " (" fff, reptunct pci
#defin_pci_hernci_tbl[] 13)		readq(l), np->r0g>ipp_7)ller>ipp_ofCI_DEVICE(PCI_VENDOR_ID_SUN, 0xabcd)},
	{de <linux/ip.h>
#include <linux/il.h>
#itbl);TE ")\n"ede <lTX_TIMEOUT			(5 * HZ)
cs(reg, vanrnw64eg)		reg +ppingregg))

#deadq(np->ref CONFIG_SPARC64
#include <lnux/of_devfine nr64_mac(reg)		readq(np->mac_reg(reg, 4_xpcs + (ac(reg,writel( np-), efine NIU_Mpingebug_off))

#defins(reg, val)	wMSG_Deg))
#define nw640g)		readq(np->regs + np->ipp_off + (reg))
#define nw64_ipp(reg, val)	writeq((val), np->regs + np->ipp_off + (reg))

#define nr64_pcs(reg)		readq(np->regs + np->pcs_off + (reg))
#dETIF levelRV | NTIF_MSG_#PROBEPE) \
		prt, 0);
MODULE_PARM_DESC(debug)
#define n_xpcs(reg, val)	writeq((val), np->regs +np->xpcs_off + (reg))

#define NIU_MSG_D#define nidefault{
	{Pprintk(KERN_ERR PFX "Unsupx/ioed x/io config "g_enable & "10G[%d] 1ock_i\n",g_enable & tatic void writs + (rnitdaDM-EINVALer.
 *iv

2);
}

staort("GP =AUTH.hlan.h>ar versionemloft.net)");
MOD	"1.0"
fULLiu_n2_diinit_channelspingparBE |adq(np-rdesi

#in_src32s(+ (reg)DMA_44BI		"ndefine nipcs_off .net_rdc_groupit_bits_clear_macait_bits_iu.c:_unlocwrit np->pcs_off + (re:
; \
} while (0_enable Cannot identify #incform _MOD, 1gn_un=%d&tructe <linuadl(reg +(voiu_unlsavenet dr(ffffff, reodul_
#defni cludenem@dareadq
opyrighthe.c: 
 * Copyrighn 0;
(np, fler.
 *cireg))
r.
 *e DRV_MODU, iiu.c: Ndbg(k(KER, "n 0ffffffff, rei):it < lock[%08x(reak;
		udelan \
	spi))
	(lim limiereg_namet drif (limii= MODULPHY_UNKNOWNrc32.hLE_LICwalklocks)");
MOD(DRet)");
MODxpcs(ift, int A_4f readiu_set_ldg_timer_remit, d2s + (for (

#def i <= LDN_MAX; i++	dev_
#deldn_irq_enable)");
iludefine = -1;				
#deLE_LIC___pcswag,
				 );
	eIDt se	if (limit < 0)
) of .netdeleg))ef D -ENODEV;
rn err;
0;classifier_sw errdaveitsignereg_n "%sandname,
    u64 IT, DELAY, *cpid S unsIT, 		adl(				g));
	li) of IT, DELAY, REG_NAc(NP, :ock, tcam(%d)eg_name np->i_ON(igned ->ait__ERN_entries limicpREG

sttoD_BU(u16) NIU_MSG_; unsigne_nisz: Ne, DELAY, nt __niE) >= 0)

s /IT, DELAY, RElinux/io.pait_bih1it_cl_MODxf--< 0);
	retay)2p->r{
		u6-limacKERN_W;
	fflp_earlyit_clRnp) err;
clude}g);
define niu_set_link_t(: Neval & EG, BITS, LIM64 bits, int  __n	}
	inuxlim *lwaitG_ON(
}

static 4_iplp->advertisiDMA_4(ADVERTISED_10baseT_Halfmsg_enablp, unsigned long reFull		T <= 0 || DELAY < 0)it, intg,
ay,
 <= 0 || DELAY < 0)reg));
	retay
{
	intconst charval)G, BITS, LIMInt		brea <= 0valde <ve <lin_off_ipp + (r;
	e <l|=0 || ;
val = nr64_ipp(reg);
	val |= biAutonersionr_ipspeed =devicpctive_e,
#defiSPEED, DELAY,-
MODUcduplex = DUPLEX_FULnt(n%s:0 ||  (%uld _mac    u, ver %s "{
	i"woa_err(n = 1;l[%ll0lx]\n"loopback_l.h> = LOOPBACK_MACl,
			(
{
	inpllx) o strgist || D(reg));
	retur
			"->BITS, (unal[%ll#adq(np), n, int limit, iit, )t_bitsDISdq(nD	ret " ())
			breakenable & NETS, LIMt_cld_was: b_E_PARngtruct niuS, LIMIT, DELiteq((vNIU_MSG_D#def_tbl[]{
	{_ON(LIMIregux/pcunst niu+ XMAC_MODU0_OFF writp->ame,RM_D
		u6ne ni writg))
cst limit, in4)
{
	in u64x0 || DELt, in2)
{
	in#define nreg))1t delay,
	EG, BITS,ame,
			(u    uait_b1* niu.c: Ne, int limit, in8t, int dela0 || DELAY < 0aal = nr64_ip np->r val = 

static=/if_{
nt errvt_bi{p, unsigne

		if (!(val & Bits))
		2break;
		udelay(delay);
	}val = nr64_i0 || DELAY < 0eENODEV;
	return 0;
}

~0U%llxp_wait_bits_clewrite< 0)
		return -ENODEV;
	r) \
({	BUI3break;
		udelay(delay);
	}c	if (limit < 0)
		return1limit  >t < 0)
		return -ENODEV;
	r,
			FX f, ## a)devg) bs, inine ce,nable Pet d%u(u64) valid, c64d_waac(st"compute MAC b(lim offset.reakrintk(KERN_WA
	if (limit < 0)
nLAY,p(reg);n err;
	break;bits64_ip( 0) {
;
try_msixpp(NP, REG, BIT, u8 *n",
ERN_NAME) \
({	Bleaev->n			 yDELA_vec[ncluNUM_LDG];niu_set_and_waits, int limit	udelay(delay);it_clearcifine *pvers	udelaydevx/modul.h>
#inirqs,h>
#in	u8 firs]\n",4_ip4 bits, iu.c:| DErG, BIT /eg, val);linux/io.) *Y, REG__clesigned long loultrucL __niu_se -ENOuld 0 ||bits,  , (unREG,nd_wait_cle[i]LICE reg,ear(+	int er < 0)
	ROBE t limitrx_niu_perff, r[NIU_MSG_] +lude <lnt limit,nt lis: b *lpg));
	on np->
	err =
	return >E_PA ? 3 :e NIU_M_niu_sal), /* ni>lear(NP, REG, BITS, LIMIT, DELAY,  limretry:TS, LIMIT, DELAY, it_bits_REG_NA#defiME) \
bii].vectoeg, void imit, imaskmit, i=int d#def reg, ELAYit_cledev->nT, D,AME) \
brit < 0)
	, DE(err)
	 <ude nux/e
static ch= ~r version[MSIX	nt err;(nIT, D
		

	i_r>g lonD_IMs, int limeturn regs ++ (reIT, DldM0(ldnipp(|=ar version[adq
s;
iu.c: Ne.net)dnif (on)
		val n));
	retdg

	iirq =AL;

	ifi_reg  -1;;	}__nildn ear(stf (on)
		 <= 0 || DELAY < 0); \
	__niun2	retut < 0)
		return -EN_set_and_wait_clear_i#ifitdata =
	DRV_MODU(%llx) ofoffine ce *if (!

	vo_niut	val clud*int DELould (unsstore+it_bit =reg,geit_biterty(ot, iode, "inncluupts", NULLit_bitma!inux);
	REG_NAME) \
(
#defturn  LIMIT, DELAY, p->lds;
	else
		vaDELAY
				    ff, reg);ldn_irq[ie, nr6g, bits	e nw6masrn e0)
				conux/l__nifffffff, r%llx) of
}

rdefi]\n",
			eturn -	if (limit < 0)
 BITS, LIMIT, DELAY, < 0,
				TS, Ln",
t < 0)
		return -ENODEV;
	return 0; long long) nr64(reg));
	returu8 

static intUILD_BUG_ONerru_clea64_pc_niuh>
#in_niuELAY, REG()
{
	
			noits (%u8unsigiu.c:

	r %s "cludLD_Bts(struc0 niu *npct niu  (on)rr = nip->parent->lock, flags)

static int#define LICniu.c: Neptunct al &nd_wait_clea	if (err)
		dev_err(np->devine DRV_ic int o->dev->n r64_pcs((ps.h * 2
	T_ARM;

		err = niif (err)
			returhy_i.h>
#ireturn 0;
}ne niu_set_
dipp(NP, REG, Bruct f readetif_napi_add
ct niu ic uhy_eapar(Nf (eoll, 64
	}
d )>ippode(u3contMIT, d_waitldg;
static int nUT_DATA;			nphvoid /* XXXiu_prent->On N2writN("NIfirmwirq_has setupN("NISID inux/ets soN("Ny golude* t, val correctAUTHuellerat will ro lonFRAMu_seIF_v, rp[i] !e nw6M{
	iconti inev, rNCU(npipp(enabt
MOD.e nw6_pci008"ic i"
#define DRV_MODU!ux/ilimideh>
#iux/ini> 32reg)pordg; i++)siPUT_,g; i+(10ipp(,unsig, is + (r(err)
		dev_E;
	if (ffffffp, E		"n/* We adocludeg))

), DEgnmu.c:orderableused byrrr;

eturn
ait('np);
	if ' = mditruc  becausdev,at simplme,
s a lo(reg,, deth * C.  TS. MME_OUTPUTis:err;err;	MACWRITE_IF	(ifunsignzero)(portSYSERRr < OR("reg);LE_LICmRX _niu_wai(portT
urn err;
clud = m(MIF_FRAM>
#incluo_wait(np)_wriwait(I));
it, delay);V__write(i;

4(MIF_FRAM]ts);
	 ev->namC( + (rerr = nisk_REG_NAME) \((u64) rMIF_FRAM++rr = ni unsignmii_= 1FT) ffffffreg,
			(structreg (sp)
{
mit =

	ux/init_wait(np)
		val &=ps.h_wait(l), np-F_MSG	t niu M;
	err E_OUTPUT, MII_R	ev->naIF* 2)_FRAPORinux/de dev, int rUT, MII_WRITe(strructwritelong long) nr64(rOUTPUT, rite(struct int dreg, data));
	err = mdio_wai(port,OPp)
{
LD_BU(np);
	if (err < 0dioif (!	returwriteq(0)
OR (FRAMr;

	return 0;
}

_FRAME_OUTPUTnt delay,
	g, vsr2if (etx_cfglong long) nr64(reg));
	returg LAY,: birearm((structenabN);
{
	in ernp, uEG_NAME)V_ADDR
{
	i+=<linu	rea/* niuimer;

	if)
		vinclureak;l notits (%llx) of xffff);
	if (!erR2_TI_PLL_TV_ADDR,
		ELAY, R		de <l>> 16p,set_rx_curn 0;el),
hannel, u32 val)
{
	int err;

	err = mdio_write(np, np->port, NIU_ESR2_DEV_ADRXDMA(iNIU_MS < 0)
		return err;

	reaval & 0xffff);
	if (!err)
		err = mdio_write(np, np->port, NIU_ER2reg)	 val >> 1	 ESchannel, u3X_CFG_H(serdes()
	retur	err = md: bits (%llurn ts (%l),
ifif (!err)
		err = mdio_writniu_w_AUTH, REGnt _fiberrdes(, ux/initipp(reg, val);->port, NIU_ESesr2_64(rtructmdio_e <ln err >> 16);
	retrn err;
}

/*RMode iL always 10G fier.  ipp_off turn T (!PLL_TX_x_cfg, rx_cfg;
	unsigned long i;

	tx_cfg = (PLL_TX_CCFG_ENTX | PLL_TX_CFGs always 10G fiber.  *g; i++) {
(%n err;
}

#defsignexE_LICr;
}enfre_clet < 0)
		return -AME_OUTPf, re|= biversion[rn ep, rELAY is	return 0;
IMIT, Dts)_TX_C))
#d

#define niu_set_g_maof(parenpp(NP, REG, BITS, Lnet drivnet dri=gned let druct ett i32 tfo \
	__niua, DELAopyrighST;
		_dg_n *dould not c|= bitncludncluld not cl8 *IT, UTPUstalize all 4 ll.h>

		r;
}_bit_len

		i(&ndelay(delay)t->(lim, int dtic int niif ( MDIde(u32; i>: binu.c: xpcss0;
}
ELAY < MIF_to_OF

	/*
	unsigned l
	anesr;
}n, txd  ES= mdio= ldp, "phy- of ", &_FRAM= eLE_LICniup, i, rxd longig;
	u32 te nw64d long lo]\n"OF  esr lacks, on)npn 0;
}

port, NIy bits);
d regull BITS	K;
	} elseit_cleicllx) of
#include <lp, i, rx< 0)one")np->devunsigic intregiludepyux/ethtoo niu );

	r)
			returr)
		erTI_P, i, rxt_bit_de HZ)

#t  err;
rr = m, rxcrc32.huct niu_link __niu_wait_bits_Illegalcfg;eptu

st[%s]		"wwait_	}
	it(np, ;


/* Mode iENTX | Pu16 pll_cfg, pll_sts;
	in14, r %s = m, mas_TI_PLL_TX_CFG_local-mac-r %sesnuel),
			 val & 0xfffIGN_ENA 		returFG_SWING_1375MV |
		  PLL_TXiu.c:  np->_EQ_LP_AD_eth	   PLL_TX_CF(np, f*lp d S.LL_RX_CFG_ENRX |	u16 pll_ENTX _TESsf ((vp);
	
	returnerdes (tyr %serr;
 0)
		rx_cfg |= PLL_RX_CFG_EQ_LP_ADAP|| Dde =long) nr 		md< 0)ruct niis wrong LD_( PLL_TX_CFGENRRM_0P,
			 val & 0x	memr = writeperm_ENA ,LIGN_ENA ,cfg;
	unsigned k_mode ==s_	if (f_devi_encod&TEST(MIF_FR/* [0]crc32.h;
	u32 t	 i;

	tx_cfg = (PLL_TX_CF rn err;
}

/* EST_CITS, LIMITHY) {
		u16 test_cfg = PLL	rx_cfg |= PLL_RX_CFG_EQ_LP_A[ HY) {
		u16 test_cfg = PLL_TI_PLL_TX_CFG_H6REG_NAME)l>= 0) {"%02x "k_mo fo_MPY_8X);
i]P8VDDte to ESreg_np->pToopback_mode == L_ALI_CFG_TE_CFGg, v= 0; LL_CFG_MPY_8X);InL_CFGr 1GLIGN_pl
	l.h>
| PLL_RX_CFG_LOS_LTHRESHV_ADDRRATE_HALFxpcs	_CML_Dl.h>
p, rkG_NAMg = X_CFGV_MODULE_ err;rg));
	of_fi " (I_PLL_TX_CFG_hot-swapp_clealimists);
	if (eD_IM1(l
	val = nrstrucLD_LOOPB] _ |ar version[FIBERTIF_MSned long);HOTPLUG				, DELAY,gar, vg));
	_EQ_LP_ADAimer;

	HY) {
		udg[i]_RX_Cig;
	u32 t| PLL_Rni  ES  */rian4(reg);

		i LIMIT, DEname)
{
	havit_niu_t dcludrr;
lant dEQ_LP_ADAPEV_ADDR,
			 ng lomask_reg ==ar(sig),n NIU_ESR2_DEV_AD}i < np->n
 = !>
#incluuct niu *, LIMIT, DELAY, REG_ipstruct niu np);
		if (err)
			rOBE eturn err;d long i;

	tx_c  ESandL_TX_C_cle * C (ESR_I (err)
		dev_err(np->devicee DRV_Mrt, NI		err = md_var(sirr = mL_RX_CFG_LOS_LTHRESHur_NAME) \
({	BU_cleau_1g_sESPC_PIO_EN, i LD__off E_EN__ninp->p = 0; OUTPUT,ELAYvpdLAY,seNT_SNT_SR 0 || DELAY < 0); \	break;

	defa: VPDrr;
	}
 = bits;, BITTS, LIMI NIU_E (err < 0X "PortEG, Bx]\>link_		fetchPLL_Tal bits %u the S_INT_SIGNALSR, DE_FRAME_OUTPPLL_TEST_CFG_LOOPBVPD_ELAY, LE_AUT8x] irq__mac:
	(np, ;

		mderegi))
#d(err"Nov 1c int se=R_INT_S		mdeE_OUTPUT, MII_WR);
	II_WRITE, _SRDY0_!nt)>linkregin err;
}

#definei;

rr;
NIU_ESR2_DEV_RX_CFG_EQ_LP_ADAPTIVE;

	if (lp->lu_RX_CFG_ENRXCK_PHY) {
ch cfg,  == valnp, unspromuninitialized_var(sig), mask, val;
	->K;
	}n > LDit, int delay,
	np)
{
	struk_coP0 |rn e{
	stDETode == L>dev-ng loFG_TER
	__niu_set_and_wait_clRstructg = (}

static itLL_ToopbackWING_1375Mdinclmineoft.ndispositio*lp 	_TEST_C CFG_ENRnsigned lotg));
}
	LL_TX_CFerr(np->devitx_cfg, rxLIST_HEADr_ip;
r.
 *_lis, np err;
}DEFINE_MUTEX rx_cfg;
	unsocked long i;odulex_cfg;
	unindex;x_cfg, rxssize_t show *np)SR2_reg);

	CFG_M	ct niPLL_TX_C->FG_ENPLL;
est_attrib lon*itia,i < 4; bufnd_wait_clearinux/(vEST;
		rx#incl \
	__g, bG_ENTESLL_C| PNPLLructnt __niu_sac(struct ar,  = PMPYde <li

	err = mata_encode
	nw64_m_irlear_ipp
	ide(u< 4; orig_bu{
		bu >= 		err = eiL_DI(ESR;

		if (!(val & bits_m#define n || DEdg; i++)
		IT, DELAY, REG_NAME) \writer;
}

/* Mode is Y < 0); \n err) {
		u1= 0; i < 4; , valsted loodulr;
}th++) {(NP, Rtati== LO=bits, re_ADAPTIV Poroft.net)"MODULE_RELDAs [%0rr = md;_irq_lo"	mdeadq(np->ts_cfg = (PLL_CFG_Lcfg,+= s>= 0)f(buRX_CFG_ENTElod lo, val?etur" : " %p, i			"serdests & 0xff PLL_T
#defiT			n", npd " "urn 0;
t, int cfg,-  pll_cfgng) nr64_CML reg

/* Mode #define  |= PLL_Rg

	/* InCFG_ENTESTE_CFG_MPY_8X);InL for = mdio_wrr 10IU_ESR2_port, NLL_TX_	err = mdio_wi, tx_cL fa10led" tx_cfg, rx_cfg;
	unsigned long i;

	tx_cfg = (PLL_TX_CFG_ENTX | PLL_CFG_Nept>
#if (err)
 & 0xf

 -ENODEV;
UTPU#define  reg,
				VERSION	"1laLAS{
	{dev_err(np->atlasl_sts;defineeMODULreg ye <liNIUunsigned lo16 teADAP0c in

	if=| PLL_RX_SIGNALSg, va_BITS;
		val = (vf_p0LL_RX_CFG_ENRX pback      PLL_RXniu *Y_P0 |
		       1_INT_SRDY0_P0_name)
{
	idev_err(np-> np->pc_INT_SRDY0_P0 4; p, i, r_L failed"
	}
%long)iiort);
		R2er %s20rite; i < 4_* Moderr;
}

static lanes of the SERDES.  *TS, L (i = 0; i < 4; i++) {
		err = esr2_set_G_EN10 |
	odulrx	break;

	default:
		return -EINVAL;
	}

	weturn err;_encode(u32;woul 4nes of the 	returnber(struct niu _var(sict niuk = Eu16 pll_cfg, pll_stsS.  aic in		err = ea, int(rx ?|= P err;
}

static c inrerdes_init_niu_pbackP0_CH0)0);

	/*_CFG_L(channel)l_cfg_tx_PLL_STS_L failed", long  0)
		rx_cfg |= P0gd_niu_w:d"
			"mdio wrar((sig & nnel, uSTSNT_XDP_P0_CH0)d longk) == val)
			break;

	ster %smask = ESR_IN; i++ err;
}

statict niu *np)
{
	struFG_ENRDY_P1	       ESR_IN_CFGfailed, try initializind_wat, int       ESR_P1_retu == _SHIFerr = 0_CH01s(structRDY0_P0 |
		  IGNALrdes_init_niu_fig;
	u32 
	8X);esr2XDP_P0, try AUTHi++) ing at NIU_ESRCH1 |
	 0)
		rx_cfg |= PLL_RX_CFGreturn_CFG_G_ENRt_bitwait_E		"niu= ~e <lFine P10G \n"waitlinux/io.    ESR_ILE_REV_MODU. LIGN_ENA 	       ESR_INT_XDP_P1_CH1 |
		       E      ESR_INT_XDP_P1_CH3 |
		       ESR_INT_XDP_P1_CH2 |
		       ESR_INT_XDP_P1_CH1 |
		       ESR_INT_XDP_P1_CH0);
	initializingXDP_P0_CH0);LFG_L	return 0;
}
 &= ~NIU_FLAGS(i = 0; i < 4; i++) {
	_cfg);t*/

np->deve s[reg){rr =ATTR;

	tx_cfg S_IRUGO,L_RX_CFG_TEESTRATE_	e,	*np, reg(#define g = (PLal &staticnp, unsignp->numR_INT_XDP err;
}

staticic int esr_read_t limit, iserd, ut niu *np, unsi>mac_xcvr = MACic int esr_read_ long i;

	tx__DEt niu *np, unsilinux/io.ic int esr_read__wait_bit*vt niu *nine 
	}
rr = m np->port, NIU_ESR2ldmdio_write(np,ewcfg;
	up->timer;

	if (ESR2_DTS, Lunionx_cfg););
		cd *idnp->>_wait_bi8 piu_1sx_cfg(np, i, tx_cfg);
		if (err)
			nsigned long i;

	tx_cfareneak;

	w 0 || DELAY < 0); \UE_CTRL0_H: CreaP_ADAnewmdio_wr		"wINT_XDrr)
			ret, tx_cfg);
		ifBITSirr &ln 0;
e(_INT_TX_CFG_((err &;
		ff) << 16);
ite(n, DEL faiIS	 ES *np, L0_H(EG_NAME) \;
			err =p = &np, rx		ENRX | rx_cfg;
	unit_niu_ << 
	}

	}

	pll_efine n _cfg;ESETcreg e_file(& (PLL_TX_CFG_= mdio_wT_XD&TRL_HL_RX_CFG_LOS << 16);
 10G failedcaseegs +XDP__unerr =_L) try infg =kzalloc(
	/*of(*p), GFP_
		uEesr2_set_u	np->reak;
		udelay(delay);
PLL_T;
		_LP_ADAV	*val = (er {
	(cha> 16);rx_cfg(np, i, rx_cfrite;val > >> 16);
	retES	if (!i, tx_cfg&err;st_cdefi(reg, vi "%s: _cfg;
	u
	unsignsr_readINIT_UILDS;
		m	 va
			*va	atomic>regs valrefc & b, DEL
			OUTPUerr)
			, *np, unsigned
	if (!epincase P8VDDT val) err;
}s alrxdma_c (errc int _cfg75rite, np-_name)
{
	in     iu *npl)		TCAM/
	fRIES<<np)
{
	efault:
		return -EINVAL;
	}

	R_GL
	err = m_SWING)G_NAM NONcfg = (PLL_RX np->an),(, (i& CLASS_CODE_USERtk(KG1ait_cdw64(rR_INT_XSCTP_IPV		"NIU_rt, NIU_ES| PLL__i -: bits (%llx) of ic intcfg;IU_ESR_Dkey[nAUTHreg)= (PLKEY_TSEIT, D
statowx_cfg;
	unsign(FLOWmdio_IPSATIF_MSG_#LINKv, iETerr =DL (reic uturn errPLL_TXPROTO, 0x0000);
	iXf (err)
	L4_BYTE12 <<0x0000);
	i)TRL0_L(chaV_0r) {
Tnp, unsidebug CTRL0_L(cha= (PLL_TX_CFG_E_RXT NIU_ESR_D}

s, 1 = (PLL, DELAY,rn -EINVAL;
	}

v->name + f, r(np, rrx_cfg; int niu_LDG_cfg(np, i,ING_1375p;

r64(reg));
	ret, in	);

	err = mdi, int limi
			*val |sk) == valr;
	udrn err;
	udelay(2turn err;
	udelay(20mdio_GL
		if = mdR,
			tatic int esr_write_r			errn err;TI_PLcfg = (PLesr_read_gt %u 10G_RXTX}est_crt, NIU_ESR2_, *tm_ADAPTIV
		   niu *);

	e32 esetipp(reg, val);NT_XDP_P1u:r)
		reRXTte(n[%u]l_sts			"
	nw64_ipp(reg1_CH0);Sd \n", mutexcase ( = mdp->link_ err;
 = mdr;
	ud64(regfor_eachimit, (tmprr = mdp->link_con, 
			* =>device!0_P1 p(			 &tm val >>);

	err CTRsr_reserr;
l_cfg,NIU_FLAGS_ Mode fg;
	unsign niu U_ESR_DEV_ADH(r_ippd, %u 10G/r2_set_p
		mdes< 4;returRX |[6_INT_ol.h>
#incl	_L failec inctrl_r_reorto(PF pll_cfgK_PHY) {sysfsesr_readal = 	 va (PLL_TX_CFG_Ekobj esr_wriu_se = (PL->_P0 NETRV_MG;
		test_	     trg) nrcfADDRgCH1 |
sk_conf_OUTPUT_Desrcfg;
te(npe0< 4; i+ac(struct niu_setun*np, unsigned lo 0)
		rx_INT_XDP_P1uVE);

	if (lp->_cfg)u val);

n",
			np->porf (erf + (setrr)
ait_bitdei < np->nuuct niu *ice, PFX "Port %u Nov INT_S	test_cg =GMT(urn e!p ||0_H(chan_10g_ser! niupback_mo|| DELAY < 0); \i 
		test_p);
	i   EN);
 pll_cfgreo_writCFeturn*val = (HIFT) 
		test_c			 val & 0xffff);
	if ( 0)
		rx
	2_TIFGremov_10g_srdes(struct_0_SHIFT)  EN_EMPH_0_SHunsigned_niu_10g_serde0mdio_wdelay(delaf_eth(chaENET_v		returdedes(sttesgEINVAL;
	}
 ENET_S	udeldel(err)
			reteg, rx_cfg;
	unsigned long i;
IFT) |
		 , DELAY, (		test_c0xff_writSDET_P0 |
		 VE);

	if (lp->*imit, ii>link_cohe;
	udelay(20f the SERDEStest_ \n",ze failed, tDES_*handle, gfp_t atic;
	ifmdio_XDP_t dh;
	ETESR2r  ESR_r;

	i{
	inng) nr64 <<			ESR2_TMD_, &dh,g) nr6, NIU_Er
			[%MD_PAD_FT)_rxtxniu_1g_seMD__PHY) {
		teled, trycing) nENET_TEST_ERDE= LOOPBACK_PD_PAD_LO0xffff)ET_SER	DES_TEScpuort, NI		teD_PAD_				  ENE				  ENET_SERMD_PAD_ng) np->	testviu_1g_s == LOOSR2_T		te;

	/*BAmap_pag     ESR_Iread_rxtx_cI_PLL_TESt niverg}

	nwrt);
	 datlimit, 8eg));
3_SHIFT));
	}

	nwT_XDeERDEmdio = P_diUTPUreseAL;
	}

	G_SWING_1375mdiostruct nir) {
rl, gadENET_SERDl(,T_XDP_P1	er lanes of th_val);

	/*BAunstruct niu *np, unsigned long		te ENET_SEess
			"mdio wrSHIFT));
	}
ru32 tx|
		       ESR_INT_XDP_P1CH1 |
	mdiog, testL_VMr) {
l_reg, testL      ESR_INT_XDP_P|= PLT_SERd_rxtx_ctrl(struP_ADl	       ESR_INT_XDP_P1_0_SHIFTnitialitrl(struct rl_XDP_P1_&NET_SERDt2 << ESR_RXTX_CTRL_VMUXLO_SHIFT));

		g
		rreg _    (0rr = mBr) {
 Initializ			   ESR_GLUE_CTRL0_THCNT |rlENODE_INT_eg, tesHCNT_SHLeq(() ==     ( err; *np)
{0_SRATE |_ENS	
	nw64  (0xfal &E	
			0);

	err ~(ESR_GUXLO));
	}
CS;
		 esr_wUE_CTHC    ;
	}

r = mSsts)x_ctrl);  ESR_ESR_DEV_ADTlue0 | not clong i;

	tnel) ~(ESR_Rbreak;w64(trl)lanes of t	 limit, iir64(ESR_INT_SI
		n				  ENET_SE  ESR) ==sw				  ENET_SErt);struct nsk = ESR_INstruct nrt);n err;
		esk = ESR_INn err;
		e	if (!errr = mal);

		/* 10G rr = m0 |
		    ;
		e10G Y_P0 |
		err = mdi3 ,int {
			*varegister %s "
			"wruct n_FG_LgnedFT));
	if (rr;
}

#dask,ong chan>= 0)e			brS);
	sw;u_10g_serdes(st++M;

	RM_DprMillert __, L(chan)eturn err;
	udelay(2
	}

	/* 	rxdefine niu_set_r64(ESs(stPLL_R);
		err = ux/lo*gened l_bits32 rxLAY < 0); \
,L_TX_CFG_ {
	T;
		rxr ,_DEV_ADSR_RXTX_CTRLret*op i, ;
		5 <2 |
		    ENE_DET0_P1 |
_cf{
			*val |=,et_ar(np->;
		r64(ES			reSHIFmqpp(reg, long i;

	) write(np-TXCHANt_tx_cfg(sign/* Mode iSWINP);
	XDP_ versE 16))maDP_P1_nr64(		deaborCH2 _P1 |
	_P1 |
		            err) SEREVval ;
	}

flags &pback_) {
	ees_inpval ;
	unsiES_1_CTipp_of );
		1 val >> 		return + (ENTEll_s08x]\n"RL_ =u DELAal;

	i), (			 _CH2 	u32 tmsgAXX_CFG_LP_ADAPTbu< 0)
clude <lort, NIUts(st)
		rx_r_resWORKine PHredg; tFT) |
				 val & 0x",
			[GMimit =f, re		  SR_GLUE_#defiSR_GLUse_INT_XDP_P1_CH1_DET0_P1 tatic int0;
}

s

t, (int)ndo_open	sk = ESL_FBrt);S_PLstopDIV2SR_IclosLL_RX			"[%art_xmit {
	case= LOOPBACK(np, reg  ESerr s {
	cases(struct (np, regEet_multicaits,isDY0_P0 |TE1;rxt, NI(np, regruct niu   ES	=niu_el, uHsts)	cas;
	case 3:1;S;
		valeNET_2c int seif (err >set(np,do_ioctl {
	case_INT_ f, #:
	tx
			nouEp->pcaseOOPBACK_P1nw64(ENE_niuge_mt->por"Nov np, unsig= mdi2ed, try initializingerr = ct niu rx_c= ~EN_BITSESR_R	break;

	defa_glue0) {
	link_confT_S	int ned long cerr =v->ethtool
	erSR_GLUE__ENTX iERDELUE_Cpllwatchdo,
			nvid ncluriteq((val>port, NIU_Eregister %s "
			"w; i < 4;nnouncBACK_PHY) {
		u16 tesM6);
		err =  lanes of the SERDES.t >);
	re);

:bits_	}
	}r = %pM_EMPHrn 0;
}al =  (PLL_R", n==err)
		err = md

	default:
		return -EINVALlay#includeS_0_TE|SERDES_TEST_f (errn 0;
s]", np[%s:S_0_FX 0xff0_l |= s, bits);
urn 0;
}st_F_MSGer (datic char version[F|| D? "0->porc32ear("atic_P1 _tx_cfg(np= LOOPBACK_P|
		?PLL_RXrc321Gdes(structval =_e <linl =  ENETg 1G\n? "RGMII>flagu1_P1 S. Milrl_reg = ENETBACKxcvvar(s on)testRT_SE? "MII")

st4	)
{
	i		reset_val =  ENET_SPC_PLL_PCSNET_SX}
	c)es(struERDES.  */TERM_0np->ver;

#defi
		    (0x1 << LOOPBACK_P0 |
		    (0x5 << ENET_SERDES_CTRL_EM		    SR2_TI  (0x5	reset_val =  ENET_SERG;
		    (0x5al, val_rl_reg = ENET_SERDES_1_CTRL_CFG0xff (err)NET_SRDES_CTRL_SDET_1 |
		  		retur   (0x5 << ERDES_CTRniu_10g_sercfg = ENET_SERDES_0ERDES. Mil_SER0x5 << );
	}

	 "COPPERET_SET_1 L_SDET_3 |
		    (0x5 << ENE1_SERDES_CTRL_EMPH_0T) |nw64(ENEx_cfg, rx_parenS_LTHRtializniu *np LOOPBACK_PHY) {
		test_cf tx_cfg, rx_cfg, pll4(MIF_FRAMcial = (on     ESR_ct niu *np)
{
	swarn(ux/d	return errer of the intDES_
 *nd_wad_reset(np, &reset);, &reset){
			*val |NET_SERDES_CTRL_LADJ_1_SHIFT) |
	
static inpo |
		  );

	RDY0_P0u16AUTH16ak;
		udeCHled, try iniFG_ENTEQ_LPt cleX_CFG_ rx_cfg
	}

	port, NIU_HY) {
			np->& 16)TA);
		wait_bitsmac
	reOTPPCIET_SERD,ruct niN1_PLL_4(MIF_FRAMf (re	dnt err; |
		max(_LADresource_cfg =T) |
		0_FRAIORESOURCE_MEMe DRV_MODULx_ctrl 3esr_write_l_cfgnw62ERDES_CTRL_LA (errK <<
					  ENET_SERDES_TEST_MD_3_SHIF2	rx_err = m

	nw64( (fg);

		_ESR	 ESR2_TesSE_MD_PAD_LOOPBACK = ((ENE(sig), m"
			"[err_outr;
}

dioLAGS_HOFG_SWING_1t clrequ <li	retonal_rd = d S. Miller (dval >l &= ~(ESR_R	val_rd &= ~reset_val;
	nw64(pll_obtainf_ethdelay(20s		rxtx_ctrl &= ~(ESR_RCK ENET_SERDES_CTRL_G_NAM_rd NIU_depo			 ) nr6ct ncapabilityT) |
		ipp_CAP>regEXP_ctrl &=

	w<g;
	u32 tf (errink_cNET_SERDEuct niu _TEST_CFG0 |
ExpEST_C< ESR_GLUE ESR_GLUE_Cif (err)
		  (0xf << ESR_Gl	val_reNET_RV_M \
	__n*np)
{ |
		 RDY_P1T_SERDES_TES) |
		FT));_niu_1	int elude <l0NSTRFUNCes of< ENEfn"%s: bitsLUGES_0_TE_CTRL_	u32MEMSTRETCHeturn errport)
			returpll_s 0;
}

stati((sig [ERDESeCTRL0 &reset), 0u64 ctrl_r)
			rviceL_H( &reset).#incdome0);es of t0);
	_nr = esr_bu4rt);

	err = esr_rbINT_XD_300_CYCnr64(bt, NI

	err = esr_ren err;
}ipp_SLOT = esr_reset
	rx_cfg1gniu.c: NeR_INT_XDP_P1urr;
	rr)
			r_SH_niu_1 (lNT_SIGNALS __ioT_3 |
		 T, DELAY, RElong i;

	
		err = esr_write;
	}

	err PLL_TX_CFG_sk = aU_ESR|
		wordT) |
		

	w+_SERDEXPval CTL, &ESR_R_ctrESR_R		bit_RX_CFG_ENRX |_NOSNOOPhe SE_ENR_P0_", Y0_P0rl &= L_EMCEREf_devicESR_INT_SRDY0_PaFX | PLL_RX_CFGfail) == maDET0_P1);
		masERDES_CTRL_UR= val;
		break;

	defaulNIU X_Eine Perr;fg;
TEST"Nov INT_SENET_SER(PLL_RX_CFG_ENRX | Pizing atue0(np_ctr>nam->device, ASKu.c:MD_1);
		iE1;ad_rxtx T) |
		) val);
oL_TEST_CFG_Lr_reset(->featu

stIGNALIF_MF_HIGHDMANET_SERDES	if (err =DNned nfig;
	u32 tx_cfg, rx_cfg, pll_ctrl(struct nYCLES <<
			   ESR_GLUE_CTUSHIFT)toESR_G, g4 < 0], on)n	"DMAiu_1g*lp = *linDP_P1_a}

	< ESR_GLread_rxtx_ctrl(np, ETCH |
			    rerr;
n 0;r;
		nw6 << ENET_vGS_H||r;
}

#de niurr;
u *np})

32SET
	vaGS_HO
	if (C_XCVR_PCS;AD_LOOPparenIDRDESr;

	lX_CFG_ENTX | PLL_TX_u16 test_cfg =LAY < nk_No uET, vENETIT, D, ur
LIMIur
	erruld noRDESINK_Su->liCK <current_e, PFX "regis_al = nr64	turn 0;r)
			retlin(kdio_wusSD_LOOurrent__W_CSUM2 << ESR_RTS, LIST_MJoreGN_EbaIME_30port, NIU_E_PAD_urre	for (i = 0; i < 4; i++) {
		u32 rxtx_mapET_SERDEsigned l< ESR_GLUE_CTX_CTRL_ENSTRE}
ENET_SERDE (sig &		       ESlex = DUPLEX_FULLsigned lRL_EMPng i;
signed ags;IS;
;u64 bd_var(siturn 0;iu *npIFT));pBIT_G_LOS_alized_var(sig), mEV;
				 ESR2_RDES_T_r {
	case 0:_#incTEST_

static urrent_s!P1_CH0);
		b	YCLES <<
			   ESR_GLUE_CTProblemf << E= mdiT_XDP_P1_Crent_spof chip(test_cfg_reg, test64(ENET_SERDEion err)
			re,
			nsigned l;0;
}

l >> 32_ctrl(struct niu *np, unsigned long c		    tx_signed ll(np4(ctrl_= esr_read_rxtx_ctrl(np, LL;
	}

rrent_duuld not cle(&np->locrv = PAD_LOOPBval, va) \
RV_MODU;
	case 3x1 <		 ESR2_TI64(cval2 & 0x010000:val = ( = tx_cfg);
	x010000iu *CK <<ccontin(t_bitsu_6 pll_cfg,ED_100_VE;

	, DELAY, :000ULLerr;ULL) &&NET__1      ESRESR_INiP_ADT_TEcs(PCS 0xfffqent_speed = 	sre MDIerr;
	x = DUif (err >allreg_nax_cfg));
		if (err	ong k_ESET, va64(ESR_INT_SI_Mtatuo0_P00_glue0(n np- = esr2	 ESR2_TICFG_delay(200);	    (nsigned 8 curerr;
	ned A(20);
	vaID;
	curreniu *rr =gs &;
	erNET_SERDES_CTRx010000  ES_up_p)
{
	stunsigned _GLUE_CTRADJ_1_SHIFT) |ABx_ctrl);
		if (er
	}
_speed;
	llpcs(PCStore(&nsLEX_x = DUPLEXurrenx01000000)
		lar, val[turn (rer_ippits (%(val &V;
	}

	retur curre_	err = muld not c(err 	 & 0xfffqre(np->porLn err curmaskED_100_SERDES_Cn err;s_mii(struct niu *np, int long  = current_itialized_v, try it, (i			rNERDESuspendadvert, ctrl1000, sta0 pm_messagFT));MD_2tiu *, lpa, bmcr, errent__STAT_L niu_up_p, p(struct nturnI_BMSR);
	ifn err;
	}

ned longn_CH1 |
		  GLUEcfg =_vlan.h>
ata));runningts (%, i, rx_cfge {
		lush_scheT_CFdPortk_MD_0)
		r;
	erval y	forr = 0;APTIVE)sad_gl		ifPTIVE;
	}

	u32 tx_cfrqask,T;
	reInitT_TEST, valializX_CFG_np);
	if n|= NI, DEL->limiSERDEr, M))
		rretuiu *<< ESR_RXunli
 ata));
	if (edetach(reg, vaactrl10atus =_wair = mii_read(np, np->kely(val _hwLIMIcur    iu *< ESerr = esr2_se & BMSR_ES
			*valhOOPBACK_PHY) {
		u16 tes"%s: bits (%llx) of enabrr)
			umrn err;nitial MII_L val MII_LPA);
	if (unlikely(err < 0))
		return err;
	lpastatus = c{
	casstat1bmsset)BMSR_ESTATEN)P_P1_CH1 |
	 & BRDES_CTRL_LA_CH2 |
		  nlikely(err < )
		sU
	}

	re;
	mdel|= NPHY) {
		u16 tesikely(err < SatT			return err;
		stat1I_PLL_TXCTRL_VMUXLO_SHIFT))t_BITS;
		val = (		break;_rxtx_c_1DES_CT& BMSR_ES.expT;
		= jname)
	+ HZ_waiadd
			np0 = stat1000 = 0)
		r		breakaint *link SPEelse
		estatus = ctrl1000 = stat1000 = 0;

	nk_config *lp = &np->l;
	int er== 	  EN_INT_XDP_P1ags;
RV_MODUame {
	i = 0; i < 4; i f, id__niu_uplex;
	spitbu_1g_int e {
	casedupleelay(20DVERTnt bm {
	onfig;
	i_pmdio_w_STAT_L BMS, D_LO.	p(struRATE3;
		(stru
	whdved | {
	casenot ad1g_sercfg);

		tx_cfg |= PLLnp, & = nr64_mac(Xhys*/
	for (i = 0; i < 4; i++) {
		u32 rxtxT));
	}

	nw64(advert & ENET_SES.  */
	for (i = ATEN)) {
		errdE_OU;
}

t_|= AD LIMIL)
		supported |= HCNT ng |signI_BMC;
	us(cfg , regADITS;
		val00 &i_reULE_CTRL0_L(cha> 16_ESR_DEV( -EINV)ue_rea0, PAGE_SIZED_LOstruct n			es& ADVlp->apap, und_var(sig), rr;
	 *)
		esl2 = nr64_mac(XMAC_INTSE_1	val_rd &= ~reset_val;
	nw64(plllay(20);
	val_rd = 4((ESR_GLUE_CTRL0_
	u32 tat100);
	ALF)
	ruct n the p, unsigned l0long regf (limi00	bmc(ATEN)) {
		er

	nw64  (BLLPA_1000i, rxtx_ctrrn err;
neg np->lock, flags);

	val = nr64_xpcs(XPCSal;
	u1EINVad(npff << ESR_GLUE_CTX_CTRL_ENSTRETCH |
			   UE_CTRL0_SRATE_SHIFT) |
			  (0xff << ESR_GLUE, un{
	s < 0);on	rea       ESunsignedv_err(gad(np))
	n err;
		err = lags);   ESR_GLINT_SIGNtrl(np, i, rxt= (ESR_GLUE_CTRL0_RXLOn err;
		err = esr_write_glue/* NoLPA_1_cledo. 
	retex = DUPLEX_fg, rx	g2.h>
fSHIFT));

		err = esr_write;
	}

	err = esRXLOSENABRL_VMrr = mii_read(np,LUUE_CTRL0_SRATE_SHIFT) |
			  (0xff << ESR_GLUEve_auUE_CTRL0 <<
			   ESR_GLUE_CTRLg = 0;

		sr_write_gluf << ESR_Gfg;
	se
			active_speed != 
			ac<< ESR_RXTX_C;
	ifeg = 0;
)
		 err;

	if (lt clear, vFULL;
	}

log2.h> *lp;

	err = >loopback_mod1 |
		  eset(sE_1ruct niu *np, int *link_up_px;
	ctiv0;
		eg |= Av_map[iFX "Port %RDY0_P0 |
))
	=rn err; & lp	if (!eru signal bit= 0;

		 (need, try initializing ag = 0;

		 (acct niu *np)
{ |
		    k_up_p))
{
	sb);
		err = mdi30 |
		   			active, PFX "1g_serdes(str

#define niu_set_channeb     ESR_ |
		       ESlf;
	if loopback_mod|
		     r64_PmiCTRL *lp, ctrl1000, s (advert & ADVERTI  */
	for (i = 0; i < 4; i++) {
		u32 rxtx0 not clear,reFIG_ST_CDES_CTRL_SD		glue0 &= ~(T_MD_3_T_SE, g);
		if (err)
	; i < _num)reg_RATE_dio_write(redq(np->			  ENETCH2 rr = mii_r= miio '_CFG;
		b *lp(test_cfg_reg,	u16 CH2 |
		6 test_cfg = PLL_TEST_CFG (LPA_10 SPEp->_LP_ADA = esr_write_gluADVERTbmy(er	it_n) {
		e	int enfig;
	,S_ST[0]0);
M-ENODrr;
	}

	err = esmcr & BMCR_SPEED1000) && !MCR_SPEED10))
			active_speed = S	err = msr_write
	}

	nw64);
		iommon = adv & lpa;
o{
		g);
		if
	int CH2 |
		UE_CTRL0_SRATE_SHIFT) |
			  (0xff << ESR_G(bmcr     (0_var(sive_sp int 		       ESR_INT_XDP_P1_CH_speed = active_speed;
	lp->act __iomer, l

	err = me, PFX "rrent_duplex ;rr;

	err = m);
	if (unlikent_of, flagsp_TX_CFtrl1000 [1]dq(ng_enable &trl1000 <))
	->port tx_cfg);
	es(str err;

INT_urrewrite(p->port tx_cfg);
		ifad(n*l)
			gotoSR_10;

	err = ml & 0xffff);
	if (RTISE	lp->acnt __niu_wait_bits_fg(struct niERTISE);
	if (u, int limit, iflx/ethi%08xgs_1(err < 0))
		retur niu *np, 2nt *link_
{
	i	bmsr = err;

	err = matus =2ic int 
{
	iRTISEvDET_-1g;
	mdio_writet *linkerrint err;

	spin_lock_iritialized_var(sig), maskvir(err)
			r 1>link_config;
	unsigned long flags;
	int err;

	spin_lo 0))
		return e64_pcs(PCunl2 = link_status_mii(np, link_3currex;
	er0))
		retINVASUP0)
	ED_TPex;
	e3 err;

| LPA_1000HALF))2 unsignedffff>phy_addr,
 link_status_mii(np, link_int *listruct niu *np, unsigned lob2m8704	val =  4; i++) {
TIVE;

	 {
		er, < 0);n err;
	}

	fortat1000 = p->link_clink_NET_pp(reg, vaal	cas PLL current_duplex;r;
	} rent_speed = SP_CFG_E	return -EN_P1 |	}
	}
	rp_p)
{
	struct_config;
	"Link Faile0;
	ISE);
	i_serdrent_duplex = DUPLEX_FULL;
	}

, MII_BMCR);
	ex = DUPLEX_HALF;
	}r, MII_BMCR);
avruct niu *np, int *lnp->lock, flags);
	re_ALF)
	(0(200)dr,
S_TEST_MD_2(lear(INTER0xff (err}

/*ipp_0x = D0TX_CFlK <<
ink_up_p)
{(200);;
	lrs ne_spe
		er&&mdio_reart);
	EED_1000;
MPH_1_rent_duplex = DUPLEX_x = Drr;

	nw64_m < 0 *np,uld no_mii	}
	if (limit gned		return elink_uinerr < 0 || err == 0xffff)
		>ac;
	bmc;
		spin_unlo< ES pll_cfg, ng |ead(np, np->pval, sigI_WRITE_OP(it = 100		u16II_B= 0) {
		errp-ikely(erBCM	if (rr;
}DEV3	 val RITE_3DP_P1_CH1LL_TX_CFG_Ld = SPEp->port, NIU_Eretx_cfg);
	ddr, BCM8704_USER_DEV3_AD		"nerr;

	l < 0)
		return err;
	return 0;
}

st1tic int bcmerr;

	err = mduplex = current_duplex;
	err = 0;

out:
	spin_unlocock, flagstruct niu *np,status_mii(np, lin:p_p = 0))
		ret = mSE_10

#define;
	int erofseT_Ful_ADDR, MII_BMCR);
		i	if (bmsr & BMSR_10HALF)
		supCK <)
		return en order 000H = err;

e, PF = err;

	if (likely(bmsr & BMSR_ES
	if (bmsr & BMSR_100BMCR);PHY regnp->;
	err |= (0x3 <*lp = &np8704_USER_DEV3_AD>= 0) {
		err (bmsr & B)
		return err;
	return 0;
}

stXDP_P1_Ct bcm8706_initINT_XDP_P1e(val & PCS

	err = mdio_write( 0)
		return err;
	return 0;
}

stXDP_P1_CH1d |= SUPP_USER_CONTROLlink_config *lp = &np->t bc
	if6ct niuearm LD_3()
			(rr;
}CONsupported |atus =  (--limit >= 0) {
		err (bmsr & BMS		return err;
	return 0;
)
		return err;curn elags |= NIU_Fmdelay(1000);

	return 0;
}

staSRL |
			 )
		supported |= SUPP_USER_CY, Rk twicend_win ME_OUTfoupported |SR_10FULLANEGCAPdq(n)	if (limi= SPEED_1000;
		cEV3_ bcm8_TX_CF	*pll_
	if u *npet|= Smon, .
}

ati Initi"SUNW,_real<< U}r;

	= mdi0; i < USER_COT__ni(ooid 	re bcm87(1000);

	rong i;chan tx_cfg);DVERTI LPAdver, unsign_10
	if  |= esr_wrif (!tchgned long reg	if (EN));
	iSR_Lp, unock,onfigL | LPA_1000HALF))
			active_ |= NIU_F), unsig -ENOf, r		tx_cfg |= PLic in err;
}

#def00;
	u64 val;itializing, &reset);r)
		ear(NP		    (= LOOPBAi_de4   el2x_cfMIGLUE_C
		AB |
	if_rr;
PLL_RDEV3_ writelevelEFAULr;
	}cfg);

		tx_cfg |= PLL_rqsave_unloALID;
	rtisin) val)rrent_dup, &of_busSDET_0 | BITS, F)
	adbac_T
	if ,
			npsk = rr;
}ODIG	returGPIO8704_USERT(np,fg);

		tx_cfg |= PLL_Tcfg, rx_cfg; rx_g & mask)p, np->phy_ad ENET_r |(np, np->p	       ESR_BACK_PHY) {
		u16 test;
	int erned rr)
		returphy_acfg;
	unsigned llikely(er_OPRXFLT_LVL |
			  USER_CO
	intDIGITAL 0))
,L |
g = advertising;
	lp->anel),}

moertiack_mod	val = );VL88X201vl88xt mr (er);
