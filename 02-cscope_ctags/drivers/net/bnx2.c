/* bnx2.c: Broadcom NX2 network driver.
 *
 * Copyright (c) 2004-2009 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Michael Chan  (mchan@broadcom.com)
 */


#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/dma-mapping.h>
#include <linux/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <asm/page.h>
#include <linux/time.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define BCM_VLAN 1
#endif
#include <net/ip.h>
#include <net/tcp.h>
#include <net/checksum.h>
#include <linux/workqueue.h>
#include <linux/crc32.h>
#include <linux/prefetch.h>
#include <linux/cache.h>
#include <linux/firmware.h>
#include <linux/log2.h>
#include <linux/list.h>

#if defined(CONFIG_CNIC) || defined(CONFIG_CNIC_MODULE)
#define BCM_CNIC 1
#include "cnic_if.h"
#endif
#include "bnx2.h"
#include "bnx2_fw.h"

#define DRV_MODULE_NAME		"bnx2"
#define PFX DRV_MODULE_NAME	": "
#define DRV_MODULE_VERSION	"2.0.2"
#define DRV_MODULE_RELDATE	"Aug 21, 2009"
#define FW_MIPS_FILE_06		"bnx2/bnx2-mips-06-5.0.0.j3.fw"
#define FW_RV2P_FILE_06		"bnx2/bnx2-rv2p-06-5.0.0.j3.fw"
#define FW_MIPS_FILE_09		"bnx2/bnx2-mips-09-5.0.0.j3.fw"
#define FW_RV2P_FILE_09_Ax	"bnx2/bnx2-rv2p-09ax-5.0.0.j3.fw"
#define FW_RV2P_FILE_09		"bnx2/bnx2-rv2p-09-5.0.0.j3.fw"

#define RUN_AT(x) (jiffies + (x))

/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT  (5*HZ)

static char version[] __devinitdata =
	"Broadcom NetXtreme II Gigabit Ethernet Driver " DRV_MODULE_NAME " v" DRV_MODULE_VERSION " (" DRV_MODULE_RELDATE ")\n";

MODULE_AUTHOR("Michael Chan <mchan@broadcom.com>");
MODULE_DESCRIPTION("Broadcom NetXtreme II BCM5706/5708/5709/5716 Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_FIRMWARE(FW_MIPS_FILE_06);
MODULE_FIRMWARE(FW_RV2P_FILE_06);
MODULE_FIRMWARE(FW_MIPS_FILE_09);
MODULE_FIRMWARE(FW_RV2P_FILE_09);
MODULE_FIRMWARE(FW_RV2P_FILE_09_Ax);

static int disable_msi = 0;

module_param(disable_msi, int, 0);
MODULE_PARM_DESC(disable_msi, "Disable Message Signaled Interrupt (MSI)");

typedef enum {
	BCM5706 = 0,
	NC370T,
	NC370I,
	BCM5706S,
	NC370F,
	BCM5708,
	BCM5708S,
	BCM5709,
	BCM5709S,
	BCM5716,
	BCM5716S,
} board_t;

/* indexed by board_t, above */
static struct {
	char *name;
} board_info[] __devinitdata = {
	{ "Broadcom NetXtreme II BCM5706 1000Base-T" },
	{ "HP NC370T Multifunction Gigabit Server Adapter" },
	{ "HP NC370i Multifunction Gigabit Server Adapter" },
	{ "Broadcom NetXtreme II BCM5706 1000Base-SX" },
	{ "HP NC370F Multifunction Gigabit Server Adapter" },
	{ "Broadcom NetXtreme II BCM5708 1000Base-T" },
	{ "Broadcom NetXtreme II BCM5708 1000Base-SX" },
	{ "Broadcom NetXtreme II BCM5709 1000Base-T" },
	{ "Broadcom NetXtreme II BCM5709 1000Base-SX" },
	{ "Broadcom NetXtreme II BCM5716 1000Base-T" },
	{ "Broadcom NetXtreme II BCM5716 1000Base-SX" },
	};

static DEFINE_PCI_DEVICE_TABLE(bnx2_pci_tbl) = {
	{ PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_NX2_5706,
	  PCI_VENDOR_ID_HP, 0x3101, 0, 0, NC370T },
	{ PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_NX2_5706,
	  PCI_VENDOR_ID_HP, 0x3106, 0, 0, NC370I },
	{ PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_NX2_5706,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, BCM5706 },
	{ PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_NX2_5708,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, BCM5708 },
	{ PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_NX2_5706S,
	  PCI_VENDOR_ID_HP, 0x3102, 0, 0, NC370F },
	{ PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_NX2_5706S,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, BCM5706S },
	{ PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_NX2_5708S,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, BCM5708S },
	{ PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_NX2_5709,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, BCM5709 },
	{ PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_NX2_5709S,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, BCM5709S },
	{ PCI_VENDOR_ID_BROADCOM, 0x163b,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, BCM5716 },
	{ PCI_VENDOR_ID_BROADCOM, 0x163c,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, BCM5716S },
	{ 0, }
};

static const struct flash_spec flash_table[] =
{
#define BUFFERED_FLAGS		(BNX2_NV_BUFFERED | BNX2_NV_TRANSLATE)
#define NONBUFFERED_FLAGS	(BNX2_NV_WREN)
	/* Slow EEPROM */
	{0x00000000, 0x40830380, 0x009f0081, 0xa184a053, 0xaf000400,
	 BUFFERED_FLAGS, SEEPROM_PAGE_BITS, SEEPROM_PAGE_SIZE,
	 SEEPROM_BYTE_ADDR_MASK, SEEPROM_TOTAL_SIZE,
	 "EEPROM - slow"},
	/* Expansion entry 0001 */
	{0x08000002, 0x4b808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 0001"},
	/* Saifun SA25F010 (non-buffered flash) */
	/* strap, cfg1, & write1 need updates */
	{0x04000001, 0x47808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, SAIFUN_FLASH_BASE_TOTAL_SIZE*2,
	 "Non-buffered flash (128kB)"},
	/* Saifun SA25F020 (non-buffered flash) */
	/* strap, cfg1, & write1 need updates */
	{0x0c000003, 0x4f808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, SAIFUN_FLASH_BASE_TOTAL_SIZE*4,
	 "Non-buffered flash (256kB)"},
	/* Expansion entry 0100 */
	{0x11000000, 0x53808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 0100"},
	/* Entry 0101: ST M45PE10 (non-buffered flash, TetonII B0) */
	{0x19000002, 0x5b808201, 0x000500db, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, ST_MICRO_FLASH_PAGE_BITS, ST_MICRO_FLASH_PAGE_SIZE,
	 ST_MICRO_FLASH_BYTE_ADDR_MASK, ST_MICRO_FLASH_BASE_TOTAL_SIZE*2,
	 "Entry 0101: ST M45PE10 (128kB non-bufferred)"},
	/* Entry 0110: ST M45PE20 (non-buffered flash)*/
	{0x15000001, 0x57808201, 0x000500db, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, ST_MICRO_FLASH_PAGE_BITS, ST_MICRO_FLASH_PAGE_SIZE,
	 ST_MICRO_FLASH_BYTE_ADDR_MASK, ST_MICRO_FLASH_BASE_TOTAL_SIZE*4,
	 "Entry 0110: ST M45PE20 (256kB non-bufferred)"},
	/* Saifun SA25F005 (non-buffered flash) */
	/* strap, cfg1, & write1 need updates */
	{0x1d000003, 0x5f808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, SAIFUN_FLASH_BASE_TOTAL_SIZE,
	 "Non-buffered flash (64kB)"},
	/* Fast EEPROM */
	{0x22000000, 0x62808380, 0x009f0081, 0xa184a053, 0xaf000400,
	 BUFFERED_FLAGS, SEEPROM_PAGE_BITS, SEEPROM_PAGE_SIZE,
	 SEEPROM_BYTE_ADDR_MASK, SEEPROM_TOTAL_SIZE,
	 "EEPROM - fast"},
	/* Expansion entry 1001 */
	{0x2a000002, 0x6b808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1001"},
	/* Expansion entry 1010 */
	{0x26000001, 0x67808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1010"},
	/* ATMEL AT45DB011B (buffered flash) */
	{0x2e000003, 0x6e808273, 0x00570081, 0x68848353, 0xaf000400,
	 BUFFERED_FLAGS, BUFFERED_FLASH_PAGE_BITS, BUFFERED_FLASH_PAGE_SIZE,
	 BUFFERED_FLASH_BYTE_ADDR_MASK, BUFFERED_FLASH_TOTAL_SIZE,
	 "Buffered flash (128kB)"},
	/* Expansion entry 1100 */
	{0x33000000, 0x73808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1100"},
	/* Expansion entry 1101 */
	{0x3b000002, 0x7b808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1101"},
	/* Ateml Expansion entry 1110 */
	{0x37000001, 0x76808273, 0x00570081, 0x68848353, 0xaf000400,
	 BUFFERED_FLAGS, BUFFERED_FLASH_PAGE_BITS, BUFFERED_FLASH_PAGE_SIZE,
	 BUFFERED_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1110 (Atmel)"},
	/* ATMEL AT45DB021B (buffered flash) */
	{0x3f000003, 0x7e808273, 0x00570081, 0x68848353, 0xaf000400,
	 BUFFERED_FLAGS, BUFFERED_FLASH_PAGE_BITS, BUFFERED_FLASH_PAGE_SIZE,
	 BUFFERED_FLASH_BYTE_ADDR_MASK, BUFFERED_FLASH_TOTAL_SIZE*2,
	 "Buffered flash (256kB)"},
};

static const struct flash_spec flash_5709 = {
	.flags		= BNX2_NV_BUFFERED,
	.page_bits	= BCM5709_FLASH_PAGE_BITS,
	.page_size	= BCM5709_FLASH_PAGE_SIZE,
	.addr_mask	= BCM5709_FLASH_BYTE_ADDR_MASK,
	.total_size	= BUFFERED_FLASH_TOTAL_SIZE*2,
	.name		= "5709 Buffered flash (256kB)",
};

MODULE_DEVICE_TABLE(pci, bnx2_pci_tbl);

static inline u32 bnx2_tx_avail(struct bnx2 *bp, struct bnx2_tx_ring_info *txr)
{
	u32 diff;

	smp_mb();

	/* The ring uses 256 indices for 255 entries, one of them
	 * needs to be skipped.
	 */
	diff = txr->tx_prod - txr->tx_cons;
	if (unlikely(diff >= TX_DESC_CNT)) {
		diff &= 0xffff;
		if (diff == TX_DESC_CNT)
			diff = MAX_TX_DESC_CNT;
	}
	return (bp->tx_ring_size - diff);
}

static u32
bnx2_reg_rd_ind(struct bnx2 *bp, u32 offset)
{
	u32 val;

	spin_lock_bh(&bp->indirect_lock);
	REG_WR(bp, BNX2_PCICFG_REG_WINDOW_ADDRESS, offset);
	val = REG_RD(bp, BNX2_PCICFG_REG_WINDOW);
	spin_unlock_bh(&bp->indirect_lock);
	return val;
}

static void
bnx2_reg_wr_ind(struct bnx2 *bp, u32 offset, u32 val)
{
	spin_lock_bh(&bp->indirect_lock);
	REG_WR(bp, BNX2_PCICFG_REG_WINDOW_ADDRESS, offset);
	REG_WR(bp, BNX2_PCICFG_REG_WINDOW, val);
	spin_unlock_bh(&bp->indirect_lock);
}

static void
bnx2_shmem_wr(struct bnx2 *bp, u32 offset, u32 val)
{
	bnx2_reg_wr_ind(bp, bp->shmem_base + offset, val);
}

static u32
bnx2_shmem_rd(struct bnx2 *bp, u32 offset)
{
	return (bnx2_reg_rd_ind(bp, bp->shmem_base + offset));
}

static void
bnx2_ctx_wr(struct bnx2 *bp, u32 cid_addr, u32 offset, u32 val)
{
	offset += cid_addr;
	spin_lock_bh(&bp->indirect_lock);
	if (CHIP_NUM(bp) == CHIP_NUM_5709) {
		int i;

		REG_WR(bp, BNX2_CTX_CTX_DATA, val);
		REG_WR(bp, BNX2_CTX_CTX_CTRL,
		       offset | BNX2_CTX_CTX_CTRL_WRITE_REQ);
		for (i = 0; i < 5; i++) {
			val = REG_RD(bp, BNX2_CTX_CTX_CTRL);
			if ((val & BNX2_CTX_CTX_CTRL_WRITE_REQ) == 0)
				break;
			udelay(5);
		}
	} else {
		REG_WR(bp, BNX2_CTX_DATA_ADR, offset);
		REG_WR(bp, BNX2_CTX_DATA, val);
	}
	spin_unlock_bh(&bp->indirect_lock);
}

#ifdef BCM_CNIC
static int
bnx2_drv_ctl(struct net_device *dev, struct drv_ctl_info *info)
{
	struct bnx2 *bp = netdev_priv(dev);
	struct drv_ctl_io *io = &info->data.io;

	switch (info->cmd) {
	case DRV_CTL_IO_WR_CMD:
		bnx2_reg_wr_ind(bp, io->offset, io->data);
		break;
	case DRV_CTL_IO_RD_CMD:
		io->data = bnx2_reg_rd_ind(bp, io->offset);
		break;
	case DRV_CTL_CTX_WR_CMD:
		bnx2_ctx_wr(bp, io->cid_addr, io->offset, io->data);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void bnx2_setup_cnic_irq_info(struct bnx2 *bp)
{
	struct cnic_eth_dev *cp = &bp->cnic_eth_dev;
	struct bnx2_napi *bnapi = &bp->bnx2_napi[0];
	int sb_id;

	if (bp->flags & BNX2_FLAG_USING_MSIX) {
		cp->drv_state |= CNIC_DRV_STATE_USING_MSIX;
		bnapi->cnic_present = 0;
		sb_id = bp->irq_nvecs;
		cp->irq_arr[0].irq_flags |= CNIC_IRQ_FL_MSIX;
	} else {
		cp->drv_state &= ~CNIC_DRV_STATE_USING_MSIX;
		bnapi->cnic_tag = bnapi->last_status_idx;
		bnapi->cnic_present = 1;
		sb_id = 0;
		cp->irq_arr[0].irq_flags &= ~CNIC_IRQ_FL_MSIX;
	}

	cp->irq_arr[0].vector = bp->irq_tbl[sb_id].vector;
	cp->irq_arr[0].status_blk = (void *)
		((unsigned long) bnapi->status_blk.msi +
		(BNX2_SBLK_MSIX_ALIGN_SIZE * sb_id));
	cp->irq_arr[0].status_blk_num = sb_id;
	cp->num_irq = 1;
}

static int bnx2_register_cnic(struct net_device *dev, struct cnic_ops *ops,
			      void *data)
{
	struct bnx2 *bp = netdev_priv(dev);
	struct cnic_eth_dev *cp = &bp->cnic_eth_dev;

	if (ops == NULL)
		return -EINVAL;

	if (cp->drv_state & CNIC_DRV_STATE_REGD)
		return -EBUSY;

	bp->cnic_data = data;
	rcu_assign_pointer(bp->cnic_ops, ops);

	cp->num_irq = 0;
	cp->drv_state = CNIC_DRV_STATE_REGD;

	bnx2_setup_cnic_irq_info(bp);

	return 0;
}

static int bnx2_unregister_cnic(struct net_device *dev)
{
	struct bnx2 *bp = netdev_priv(dev);
	struct bnx2_napi *bnapi = &bp->bnx2_napi[0];
	struct cnic_eth_dev *cp = &bp->cnic_eth_dev;

	mutex_lock(&bp->cnic_lock);
	cp->drv_state = 0;
	bnapi->cnic_present = 0;
	rcu_assign_pointer(bp->cnic_ops, NULL);
	mutex_unlock(&bp->cnic_lock);
	synchronize_rcu();
	return 0;
}

struct cnic_eth_dev *bnx2_cnic_probe(struct net_device *dev)
{
	struct bnx2 *bp = netdev_priv(dev);
	struct cnic_eth_dev *cp = &bp->cnic_eth_dev;

	cp->drv_owner = THIS_MODULE;
	cp->chip_id = bp->chip_id;
	cp->pdev = bp->pdev;
	cp->io_base = bp->regview;
	cp->drv_ctl = bnx2_drv_ctl;
	cp->drv_register_cnic = bnx2_register_cnic;
	cp->drv_unregister_cnic = bnx2_unregister_cnic;

	return cp;
}
EXPORT_SYMBOL(bnx2_cnic_probe);

static void
bnx2_cnic_stop(struct bnx2 *bp)
{
	struct cnic_ops *c_ops;
	struct cnic_ctl_info info;

	mutex_lock(&bp->cnic_lock);
	c_ops = bp->cnic_ops;
	if (c_ops) {
		info.cmd = CNIC_CTL_STOP_CMD;
		c_ops->cnic_ctl(bp->cnic_data, &info);
	}
	mutex_unlock(&bp->cnic_lock);
}

static void
bnx2_cnic_start(struct bnx2 *bp)
{
	struct cnic_ops *c_ops;
	struct cnic_ctl_info info;

	mutex_lock(&bp->cnic_lock);
	c_ops = bp->cnic_ops;
	if (c_ops) {
		if (!(bp->flags & BNX2_FLAG_USING_MSIX)) {
			struct bnx2_napi *bnapi = &bp->bnx2_napi[0];

			bnapi->cnic_tag = bnapi->last_status_idx;
		}
		info.cmd = CNIC_CTL_START_CMD;
		c_ops->cnic_ctl(bp->cnic_data, &info);
	}
	mutex_unlock(&bp->cnic_lock);
}

#else

static void
bnx2_cnic_stop(struct bnx2 *bp)
{
}

static void
bnx2_cnic_start(struct bnx2 *bp)
{
}

#endif

static int
bnx2_read_phy(struct bnx2 *bp, u32 reg, u32 *val)
{
	u32 val1;
	int i, ret;

	if (bp->phy_flags & BNX2_PHY_FLAG_INT_MODE_AUTO_POLLING) {
		val1 = REG_RD(bp, BNX2_EMAC_MDIO_MODE);
		val1 &= ~BNX2_EMAC_MDIO_MODE_AUTO_POLL;

		REG_WR(bp, BNX2_EMAC_MDIO_MODE, val1);
		REG_RD(bp, BNX2_EMAC_MDIO_MODE);

		udelay(40);
	}

	val1 = (bp->phy_addr << 21) | (reg << 16) |
		BNX2_EMAC_MDIO_COMM_COMMAND_READ | BNX2_EMAC_MDIO_COMM_DISEXT |
		BNX2_EMAC_MDIO_COMM_START_BUSY;
	REG_WR(bp, BNX2_EMAC_MDIO_COMM, val1);

	for (i = 0; i < 50; i++) {
		udelay(10);

		val1 = REG_RD(bp, BNX2_EMAC_MDIO_COMM);
		if (!(val1 & BNX2_EMAC_MDIO_COMM_START_BUSY)) {
			udelay(5);

			val1 = REG_RD(bp, BNX2_EMAC_MDIO_COMM);
			val1 &= BNX2_EMAC_MDIO_COMM_DATA;

			break;
		}
	}

	if (val1 & BNX2_EMAC_MDIO_COMM_START_BUSY) {
		*val = 0x0;
		ret = -EBUSY;
	}
	else {
		*val = val1;
		ret = 0;
	}

	if (bp->phy_flags & BNX2_PHY_FLAG_INT_MODE_AUTO_POLLING) {
		val1 = REG_RD(bp, BNX2_EMAC_MDIO_MODE);
		val1 |= BNX2_EMAC_MDIO_MODE_AUTO_POLL;

		REG_WR(bp, BNX2_EMAC_MDIO_MODE, val1);
		REG_RD(bp, BNX2_EMAC_MDIO_MODE);

		udelay(40);
	}

	return ret;
}

static int
bnx2_write_phy(struct bnx2 *bp, u32 reg, u32 val)
{
	u32 val1;
	int i, ret;

	if (bp->phy_flags & BNX2_PHY_FLAG_INT_MODE_AUTO_POLLING) {
		val1 = REG_RD(bp, BNX2_EMAC_MDIO_MODE);
		val1 &= ~BNX2_EMAC_MDIO_MODE_AUTO_POLL;

		REG_WR(bp, BNX2_EMAC_MDIO_MODE, val1);
		REG_RD(bp, BNX2_EMAC_MDIO_MODE);

		udelay(40);
	}

	val1 = (bp->phy_addr << 21) | (reg << 16) | val |
		BNX2_EMAC_MDIO_COMM_COMMAND_WRITE |
		BNX2_EMAC_MDIO_COMM_START_BUSY | BNX2_EMAC_MDIO_COMM_DISEXT;
	REG_WR(bp, BNX2_EMAC_MDIO_COMM, val1);

	for (i = 0; i < 50; i++) {
		udelay(10);

		val1 = REG_RD(bp, BNX2_EMAC_MDIO_COMM);
		if (!(val1 & BNX2_EMAC_MDIO_COMM_START_BUSY)) {
			udelay(5);
			break;
		}
	}

	if (val1 & BNX2_EMAC_MDIO_COMM_START_BUSY)
        	ret = -EBUSY;
	else
		ret = 0;

	if (bp->phy_flags & BNX2_PHY_FLAG_INT_MODE_AUTO_POLLING) {
		val1 = REG_RD(bp, BNX2_EMAC_MDIO_MODE);
		val1 |= BNX2_EMAC_MDIO_MODE_AUTO_POLL;

		REG_WR(bp, BNX2_EMAC_MDIO_MODE, val1);
		REG_RD(bp, BNX2_EMAC_MDIO_MODE);

		udelay(40);
	}

	return ret;
}

static void
bnx2_disable_int(struct bnx2 *bp)
{
	int i;
	struct bnx2_napi *bnapi;

	for (i = 0; i < bp->irq_nvecs; i++) {
		bnapi = &bp->bnx2_napi[i];
		REG_WR(bp, BNX2_PCICFG_INT_ACK_CMD, bnapi->int_num |
		       BNX2_PCICFG_INT_ACK_CMD_MASK_INT);
	}
	REG_RD(bp, BNX2_PCICFG_INT_ACK_CMD);
}

static void
bnx2_enable_int(struct bnx2 *bp)
{
	int i;
	struct bnx2_napi *bnapi;

	for (i = 0; i < bp->irq_nvecs; i++) {
		bnapi = &bp->bnx2_napi[i];

		REG_WR(bp, BNX2_PCICFG_INT_ACK_CMD, bnapi->int_num |
		       BNX2_PCICFG_INT_ACK_CMD_INDEX_VALID |
		       BNX2_PCICFG_INT_ACK_CMD_MASK_INT |
		       bnapi->last_status_idx);

		REG_WR(bp, BNX2_PCICFG_INT_ACK_CMD, bnapi->int_num |
		       BNX2_PCICFG_INT_ACK_CMD_INDEX_VALID |
		       bnapi->last_status_idx);
	}
	REG_WR(bp, BNX2_HC_COMMAND, bp->hc_cmd | BNX2_HC_COMMAND_COAL_NOW);
}

static void
bnx2_disable_int_sync(struct bnx2 *bp)
{
	int i;

	atomic_inc(&bp->intr_sem);
	if (!netif_running(bp->dev))
		return;

	bnx2_disable_int(bp);
	for (i = 0; i < bp->irq_nvecs; i++)
		synchronize_irq(bp->irq_tbl[i].vector);
}

static void
bnx2_napi_disable(struct bnx2 *bp)
{
	int i;

	for (i = 0; i < bp->irq_nvecs; i++)
		napi_disable(&bp->bnx2_napi[i].napi);
}

static void
bnx2_napi_enable(struct bnx2 *bp)
{
	int i;

	for (i = 0; i < bp->irq_nvecs; i++)
		napi_enable(&bp->bnx2_napi[i].napi);
}

static void
bnx2_netif_stop(struct bnx2 *bp)
{
	bnx2_cnic_stop(bp);
	bnx2_disable_int_sync(bp);
	if (netif_running(bp->dev)) {
		bnx2_napi_disable(bp);
		netif_tx_disable(bp->dev);
		bp->dev->trans_start = jiffies;	/* prevent tx timeout */
	}
}

static void
bnx2_netif_start(struct bnx2 *bp)
{
	if (atomic_dec_and_test(&bp->intr_sem)) {
		if (netif_running(bp->dev)) {
			netif_tx_wake_all_queues(bp->dev);
			bnx2_napi_enable(bp);
			bnx2_enable_int(bp);
			bnx2_cnic_start(bp);
		}
	}
}

static void
bnx2_free_tx_mem(struct bnx2 *bp)
{
	int i;

	for (i = 0; i < bp->num_tx_rings; i++) {
		struct bnx2_napi *bnapi = &bp->bnx2_napi[i];
		struct bnx2_tx_ring_info *txr = &bnapi->tx_ring;

		if (txr->tx_desc_ring) {
			pci_free_consistent(bp->pdev, TXBD_RING_SIZE,
					    txr->tx_desc_ring,
					    txr->tx_desc_mapping);
			txr->tx_desc_ring = NULL;
		}
		kfree(txr->tx_buf_ring);
		txr->tx_buf_ring = NULL;
	}
}

static void
bnx2_free_rx_mem(struct bnx2 *bp)
{
	int i;

	for (i = 0; i < bp->num_rx_rings; i++) {
		struct bnx2_napi *bnapi = &bp->bnx2_napi[i];
		struct bnx2_rx_ring_info *rxr = &bnapi->rx_ring;
		int j;

		for (j = 0; j < bp->rx_max_ring; j++) {
			if (rxr->rx_desc_ring[j])
				pci_free_consistent(bp->pdev, RXBD_RING_SIZE,
						    rxr->rx_desc_ring[j],
						    rxr->rx_desc_mapping[j]);
			rxr->rx_desc_ring[j] = NULL;
		}
		vfree(rxr->rx_buf_ring);
		rxr->rx_buf_ring = NULL;

		for (j = 0; j < bp->rx_max_pg_ring; j++) {
			if (rxr->rx_pg_desc_ring[j])
				pci_free_consistent(bp->pdev, RXBD_RING_SIZE,
						    rxr->rx_pg_desc_ring[j],
						    rxr->rx_pg_desc_mapping[j]);
			rxr->rx_pg_desc_ring[j] = NULL;
		}
		vfree(rxr->rx_pg_ring);
		rxr->rx_pg_ring = NULL;
	}
}

static int
bnx2_alloc_tx_mem(struct bnx2 *bp)
{
	int i;

	for (i = 0; i < bp->num_tx_rings; i++) {
		struct bnx2_napi *bnapi = &bp->bnx2_napi[i];
		struct bnx2_tx_ring_info *txr = &bnapi->tx_ring;

		txr->tx_buf_ring = kzalloc(SW_TXBD_RING_SIZE, GFP_KERNEL);
		if (txr->tx_buf_ring == NULL)
			return -ENOMEM;

		txr->tx_desc_ring =
			pci_alloc_consistent(bp->pdev, TXBD_RING_SIZE,
					     &txr->tx_desc_mapping);
		if (txr->tx_desc_ring == NULL)
			return -ENOMEM;
	}
	return 0;
}

static int
bnx2_alloc_rx_mem(struct bnx2 *bp)
{
	int i;

	for (i = 0; i < bp->num_rx_rings; i++) {
		struct bnx2_napi *bnapi = &bp->bnx2_napi[i];
		struct bnx2_rx_ring_info *rxr = &bnapi->rx_ring;
		int j;

		rxr->rx_buf_ring =
			vmalloc(SW_RXBD_RING_SIZE * bp->rx_max_ring);
		if (rxr->rx_buf_ring == NULL)
			return -ENOMEM;

		memset(rxr->rx_buf_ring, 0,
		       SW_RXBD_RING_SIZE * bp->rx_max_ring);

		for (j = 0; j < bp->rx_max_ring; j++) {
			rxr->rx_desc_ring[j] =
				pci_alloc_consistent(bp->pdev, RXBD_RING_SIZE,
						     &rxr->rx_desc_mapping[j]);
			if (rxr->rx_desc_ring[j] == NULL)
				return -ENOMEM;

		}

		if (bp->rx_pg_ring_size) {
			rxr->rx_pg_ring = vmalloc(SW_RXPG_RING_SIZE *
						  bp->rx_max_pg_ring);
			if (rxr->rx_pg_ring == NULL)
				return -ENOMEM;

			memset(rxr->rx_pg_ring, 0, SW_RXPG_RING_SIZE *
			       bp->rx_max_pg_ring);
		}

		for (j = 0; j < bp->rx_max_pg_ring; j++) {
			rxr->rx_pg_desc_ring[j] =
				pci_alloc_consistent(bp->pdev, RXBD_RING_SIZE,
						&rxr->rx_pg_desc_mapping[j]);
			if (rxr->rx_pg_desc_ring[j] == NULL)
				return -ENOMEM;

		}
	}
	return 0;
}

static void
bnx2_free_mem(struct bnx2 *bp)
{
	int i;
	struct bnx2_napi *bnapi = &bp->bnx2_napi[0];

	bnx2_free_tx_mem(bp);
	bnx2_free_rx_mem(bp);

	for (i = 0; i < bp->ctx_pages; i++) {
		if (bp->ctx_blk[i]) {
			pci_free_consistent(bp->pdev, BCM_PAGE_SIZE,
					    bp->ctx_blk[i],
					    bp->ctx_blk_mapping[i]);
			bp->ctx_blk[i] = NULL;
		}
	}
	if (bnapi->status_blk.msi) {
		pci_free_consistent(bp->pdev, bp->status_stats_size,
				    bnapi->status_blk.msi,
				    bp->status_blk_mapping);
		bnapi->status_blk.msi = NULL;
		bp->stats_blk = NULL;
	}
}

static int
bnx2_alloc_mem(struct bnx2 *bp)
{
	int i, status_blk_size, err;
	struct bnx2_napi *bnapi;
	void *status_blk;

	/* Combine status and statistics blocks into one allocation. */
	status_blk_size = L1_CACHE_ALIGN(sizeof(struct status_block));
	if (bp->flags & BNX2_FLAG_MSIX_CAP)
		status_blk_size = L1_CACHE_ALIGN(BNX2_MAX_MSIX_HW_VEC *
						 BNX2_SBLK_MSIX_ALIGN_SIZE);
	bp->status_stats_size = status_blk_size +
				sizeof(struct statistics_block);

	status_blk = pci_alloc_consistent(bp->pdev, bp->status_stats_size,
					  &bp->status_blk_mapping);
	if (status_blk == NULL)
		goto alloc_mem_err;

	memset(status_blk, 0, bp->status_stats_size);

	bnapi = &bp->bnx2_napi[0];
	bnapi->status_blk.msi = status_blk;
	bnapi->hw_tx_cons_ptr =
		&bnapi->status_blk.msi->status_tx_quick_consumer_index0;
	bnapi->hw_rx_cons_ptr =
		&bnapi->status_blk.msi->status_rx_quick_consumer_index0;
	if (bp->flags & BNX2_FLAG_MSIX_CAP) {
		for (i = 1; i < BNX2_MAX_MSIX_VEC; i++) {
			struct status_block_msix *sblk;

			bnapi = &bp->bnx2_napi[i];

			sblk = (void *) (status_blk +
					 BNX2_SBLK_MSIX_ALIGN_SIZE * i);
			bnapi->status_blk.msix = sblk;
			bnapi->hw_tx_cons_ptr =
				&sblk->status_tx_quick_consumer_index;
			bnapi->hw_rx_cons_ptr =
				&sblk->status_rx_quick_consumer_index;
			bnapi->int_num = i << 24;
		}
	}

	bp->stats_blk = status_blk + status_blk_size;

	bp->stats_blk_mapping = bp->status_blk_mapping + status_blk_size;

	if (CHIP_NUM(bp) == CHIP_NUM_5709) {
		bp->ctx_pages = 0x2000 / BCM_PAGE_SIZE;
		if (bp->ctx_pages == 0)
			bp->ctx_pages = 1;
		for (i = 0; i < bp->ctx_pages; i++) {
			bp->ctx_blk[i] = pci_alloc_consistent(bp->pdev,
						BCM_PAGE_SIZE,
						&bp->ctx_blk_mapping[i]);
			if (bp->ctx_blk[i] == NULL)
				goto alloc_mem_err;
		}
	}

	err = bnx2_alloc_rx_mem(bp);
	if (err)
		goto alloc_mem_err;

	err = bnx2_alloc_tx_mem(bp);
	if (err)
		goto alloc_mem_err;

	return 0;

alloc_mem_err:
	bnx2_free_mem(bp);
	return -ENOMEM;
}

static void
bnx2_report_fw_link(struct bnx2 *bp)
{
	u32 fw_link_status = 0;

	if (bp->phy_flags & BNX2_PHY_FLAG_REMOTE_PHY_CAP)
		return;

	if (bp->link_up) {
		u32 bmsr;

		switch (bp->line_speed) {
		case SPEED_10:
			if (bp->duplex == DUPLEX_HALF)
				fw_link_status = BNX2_LINK_STATUS_10HALF;
			else
				fw_link_status = BNX2_LINK_STATUS_10FULL;
			break;
		case SPEED_100:
			if (bp->duplex == DUPLEX_HALF)
				fw_link_status = BNX2_LINK_STATUS_100HALF;
			else
				fw_link_status = BNX2_LINK_STATUS_100FULL;
			break;
		case SPEED_1000:
			if (bp->duplex == DUPLEX_HALF)
				fw_link_status = BNX2_LINK_STATUS_1000HALF;
			else
				fw_link_status = BNX2_LINK_STATUS_1000FULL;
			break;
		case SPEED_2500:
			if (bp->duplex == DUPLEX_HALF)
				fw_link_status = BNX2_LINK_STATUS_2500HALF;
			else
				fw_link_status = BNX2_LINK_STATUS_2500FULL;
			break;
		}

		fw_link_status |= BNX2_LINK_STATUS_LINK_UP;

		if (bp->autoneg) {
			fw_link_status |= BNX2_LINK_STATUS_AN_ENABLED;

			bnx2_read_phy(bp, bp->mii_bmsr, &bmsr);
			bnx2_read_phy(bp, bp->mii_bmsr, &bmsr);

			if (!(bmsr & BMSR_ANEGCOMPLETE) ||
			    bp->phy_flags & BNX2_PHY_FLAG_PARALLEL_DETECT)
				fw_link_status |= BNX2_LINK_STATUS_PARALLEL_DET;
			else
				fw_link_status |= BNX2_LINK_STATUS_AN_COMPLETE;
		}
	}
	else
		fw_link_status = BNX2_LINK_STATUS_LINK_DOWN;

	bnx2_shmem_wr(bp, BNX2_LINK_STATUS, fw_link_status);
}

static char *
bnx2_xceiver_str(struct bnx2 *bp)
{
	return ((bp->phy_port == PORT_FIBRE) ? "SerDes" :
		((bp->phy_flags & BNX2_PHY_FLAG_SERDES) ? "Remote Copper" :
		 "Copper"));
}

static void
bnx2_report_link(struct bnx2 *bp)
{
	if (bp->link_up) {
		netif_carrier_on(bp->dev);
		printk(KERN_INFO PFX "%s NIC %s Link is Up, ", bp->dev->name,
		       bnx2_xceiver_str(bp));

		printk("%d Mbps ", bp->line_speed);

		if (bp->duplex == DUPLEX_FULL)
			printk("full duplex");
		else
			printk("half duplex");

		if (bp->flow_ctrl) {
			if (bp->flow_ctrl & FLOW_CTRL_RX) {
				printk(", receive ");
				if (bp->flow_ctrl & FLOW_CTRL_TX)
					printk("& transmit ");
			}
			else {
				printk(", transmit ");
			}
			printk("flow control ON");
		}
		printk("\n");
	}
	else {
		netif_carrier_off(bp->dev);
		printk(KERN_ERR PFX "%s NIC %s Link is Down\n", bp->dev->name,
		       bnx2_xceiver_str(bp));
	}

	bnx2_report_fw_link(bp);
}

static void
bnx2_resolve_flow_ctrl(struct bnx2 *bp)
{
	u32 local_adv, remote_adv;

	bp->flow_ctrl = 0;
	if ((bp->autoneg & (AUTONEG_SPEED | AUTONEG_FLOW_CTRL)) !=
		(AUTONEG_SPEED | AUTONEG_FLOW_CTRL)) {

		if (bp->duplex == DUPLEX_FULL) {
			bp->flow_ctrl = bp->req_flow_ctrl;
		}
		return;
	}

	if (bp->duplex != DUPLEX_FULL) {
		return;
	}

	if ((bp->phy_flags & BNX2_PHY_FLAG_SERDES) &&
	    (CHIP_NUM(bp) == CHIP_NUM_5708)) {
		u32 val;

		bnx2_read_phy(bp, BCM5708S_1000X_STAT1, &val);
		if (val & BCM5708S_1000X_STAT1_TX_PAUSE)
			bp->flow_ctrl |= FLOW_CTRL_TX;
		if (val & BCM5708S_1000X_STAT1_RX_PAUSE)
			bp->flow_ctrl |= FLOW_CTRL_RX;
		return;
	}

	bnx2_read_phy(bp, bp->mii_adv, &local_adv);
	bnx2_read_phy(bp, bp->mii_lpa, &remote_adv);

	if (bp->phy_flags & BNX2_PHY_FLAG_SERDES) {
		u32 new_local_adv = 0;
		u32 new_remote_adv = 0;

		if (local_adv & ADVERTISE_1000XPAUSE)
			new_local_adv |= ADVERTISE_PAUSE_CAP;
		if (local_adv & ADVERTISE_1000XPSE_ASYM)
			new_local_adv |= ADVERTISE_PAUSE_ASYM;
		if (remote_adv & ADVERTISE_1000XPAUSE)
			new_remote_adv |= ADVERTISE_PAUSE_CAP;
		if (remote_adv & ADVERTISE_1000XPSE_ASYM)
			new_remote_adv |= ADVERTISE_PAUSE_ASYM;

		local_adv = new_local_adv;
		remote_adv = new_remote_adv;
	}

	/* See Table 28B-3 of 802.3ab-1999 spec. */
	if (local_adv & ADVERTISE_PAUSE_CAP) {
		if(local_adv & ADVERTISE_PAUSE_ASYM) {
	                if (remote_adv & ADVERTISE_PAUSE_CAP) {
				bp->flow_ctrl = FLOW_CTRL_TX | FLOW_CTRL_RX;
			}
			else if (remote_adv & ADVERTISE_PAUSE_ASYM) {
				bp->flow_ctrl = FLOW_CTRL_RX;
			}
		}
		else {
			if (remote_adv & ADVERTISE_PAUSE_CAP) {
				bp->flow_ctrl = FLOW_CTRL_TX | FLOW_CTRL_RX;
			}
		}
	}
	else if (local_adv & ADVERTISE_PAUSE_ASYM) {
		if ((remote_adv & ADVERTISE_PAUSE_CAP) &&
			(remote_adv & ADVERTISE_PAUSE_ASYM)) {

			bp->flow_ctrl = FLOW_CTRL_TX;
		}
	}
}

static int
bnx2_5709s_linkup(struct bnx2 *bp)
{
	u32 val, speed;

	bp->link_up = 1;

	bnx2_write_phy(bp, MII_BNX2_BLK_ADDR, MII_BNX2_BLK_ADDR_GP_STATUS);
	bnx2_read_phy(bp, MII_BNX2_GP_TOP_AN_STATUS1, &val);
	bnx2_write_phy(bp, MII_BNX2_BLK_ADDR, MII_BNX2_BLK_ADDR_COMBO_IEEEB0);

	if ((bp->autoneg & AUTONEG_SPEED) == 0) {
		bp->line_speed = bp->req_line_speed;
		bp->duplex = bp->req_duplex;
		return 0;
	}
	speed = val & MII_BNX2_GP_TOP_AN_SPEED_MSK;
	switch (speed) {
		case MII_BNX2_GP_TOP_AN_SPEED_10:
			bp->line_speed = SPEED_10;
			break;
		case MII_BNX2_GP_TOP_AN_SPEED_100:
			bp->line_speed = SPEED_100;
			break;
		case MII_BNX2_GP_TOP_AN_SPEED_1G:
		case MII_BNX2_GP_TOP_AN_SPEED_1GKV:
			bp->line_speed = SPEED_1000;
			break;
		case MII_BNX2_GP_TOP_AN_SPEED_2_5G:
			bp->line_speed = SPEED_2500;
			break;
	}
	if (val & MII_BNX2_GP_TOP_AN_FD)
		bp->duplex = DUPLEX_FULL;
	else
		bp->duplex = DUPLEX_HALF;
	return 0;
}

static int
bnx2_5708s_linkup(struct bnx2 *bp)
{
	u32 val;

	bp->link_up = 1;
	bnx2_read_phy(bp, BCM5708S_1000X_STAT1, &val);
	switch (val & BCM5708S_1000X_STAT1_SPEED_MASK) {
		case BCM5708S_1000X_STAT1_SPEED_10:
			bp->line_speed = SPEED_10;
			break;
		case BCM5708S_1000X_STAT1_SPEED_100:
			bp->line_speed = SPEED_100;
			break;
		case BCM5708S_1000X_STAT1_SPEED_1G:
			bp->line_speed = SPEED_1000;
			break;
		case BCM5708S_1000X_STAT1_SPEED_2G5:
			bp->line_speed = SPEED_2500;
			break;
	}
	if (val & BCM5708S_1000X_STAT1_FD)
		bp->duplex = DUPLEX_FULL;
	else
		bp->duplex = DUPLEX_HALF;

	return 0;
}

static int
bnx2_5706s_linkup(struct bnx2 *bp)
{
	u32 bmcr, local_adv, remote_adv, common;

	bp->link_up = 1;
	bp->line_speed = SPEED_1000;

	bnx2_read_phy(bp, bp->mii_bmcr, &bmcr);
	if (bmcr & BMCR_FULLDPLX) {
		bp->duplex = DUPLEX_FULL;
	}
	else {
		bp->duplex = DUPLEX_HALF;
	}

	if (!(bmcr & BMCR_ANENABLE)) {
		return 0;
	}

	bnx2_read_phy(bp, bp->mii_adv, &local_adv);
	bnx2_read_phy(bp, bp->mii_lpa, &remote_adv);

	common = local_adv & remote_adv;
	if (common & (ADVERTISE_1000XHALF | ADVERTISE_1000XFULL)) {

		if (common & ADVERTISE_1000XFULL) {
			bp->duplex = DUPLEX_FULL;
		}
		else {
			bp->duplex = DUPLEX_HALF;
		}
	}

	return 0;
}

static int
bnx2_copper_linkup(struct bnx2 *bp)
{
	u32 bmcr;

	bnx2_read_phy(bp, bp->mii_bmcr, &bmcr);
	if (bmcr & BMCR_ANENABLE) {
		u32 local_adv, remote_adv, common;

		bnx2_read_phy(bp, MII_CTRL1000, &local_adv);
		bnx2_read_phy(bp, MII_STAT1000, &remote_adv);

		common = local_adv & (remote_adv >> 2);
		if (common & ADVERTISE_1000FULL) {
			bp->line_speed = SPEED_1000;
			bp->duplex = DUPLEX_FULL;
		}
		else if (common & ADVERTISE_1000HALF) {
			bp->line_speed = SPEED_1000;
			bp->duplex = DUPLEX_HALF;
		}
		else {
			bnx2_read_phy(bp, bp->mii_adv, &local_adv);
			bnx2_read_phy(bp, bp->mii_lpa, &remote_adv);

			common = local_adv & remote_adv;
			if (common & ADVERTISE_100FULL) {
				bp->line_speed = SPEED_100;
				bp->duplex = DUPLEX_FULL;
			}
			else if (common & ADVERTISE_100HALF) {
				bp->line_speed = SPEED_100;
				bp->duplex = DUPLEX_HALF;
			}
			else if (common & ADVERTISE_10FULL) {
				bp->line_speed = SPEED_10;
				bp->duplex = DUPLEX_FULL;
			}
			else if (common & ADVERTISE_10HALF) {
				bp->line_speed = SPEED_10;
				bp->duplex = DUPLEX_HALF;
			}
			else {
				bp->line_speed = 0;
				bp->link_up = 0;
			}
		}
	}
	else {
		if (bmcr & BMCR_SPEED100) {
			bp->line_speed = SPEED_100;
		}
		else {
			bp->line_speed = SPEED_10;
		}
		if (bmcr & BMCR_FULLDPLX) {
			bp->duplex = DUPLEX_FULL;
		}
		else {
			bp->duplex = DUPLEX_HALF;
		}
	}

	return 0;
}

static void
bnx2_init_rx_context(struct bnx2 *bp, u32 cid)
{
	u32 val, rx_cid_addr = GET_CID_ADDR(cid);

	val = BNX2_L2CTX_CTX_TYPE_CTX_BD_CHN_TYPE_VALUE;
	val |= BNX2_L2CTX_CTX_TYPE_SIZE_L2;
	val |= 0x02 << 8;

	if (CHIP_NUM(bp) == CHIP_NUM_5709) {
		u32 lo_water, hi_water;

		if (bp->flow_ctrl & FLOW_CTRL_TX)
			lo_water = BNX2_L2CTX_LO_WATER_MARK_DEFAULT;
		else
			lo_water = BNX2_L2CTX_LO_WATER_MARK_DIS;
		if (lo_water >= bp->rx_ring_size)
			lo_water = 0;

		hi_water = bp->rx_ring_size / 4;

		if (hi_water <= lo_water)
			lo_water = 0;

		hi_water /= BNX2_L2CTX_HI_WATER_MARK_SCALE;
		lo_water /= BNX2_L2CTX_LO_WATER_MARK_SCALE;

		if (hi_water > 0xf)
			hi_water = 0xf;
		else if (hi_water == 0)
			lo_water = 0;
		val |= lo_water | (hi_water << BNX2_L2CTX_HI_WATER_MARK_SHIFT);
	}
	bnx2_ctx_wr(bp, rx_cid_addr, BNX2_L2CTX_CTX_TYPE, val);
}

static void
bnx2_init_all_rx_contexts(struct bnx2 *bp)
{
	int i;
	u32 cid;

	for (i = 0, cid = RX_CID; i < bp->num_rx_rings; i++, cid++) {
		if (i == 1)
			cid = RX_RSS_CID;
		bnx2_init_rx_context(bp, cid);
	}
}

static void
bnx2_set_mac_link(struct bnx2 *bp)
{
	u32 val;

	REG_WR(bp, BNX2_EMAC_TX_LENGTHS, 0x2620);
	if (bp->link_up && (bp->line_speed == SPEED_1000) &&
		(bp->duplex == DUPLEX_HALF)) {
		REG_WR(bp, BNX2_EMAC_TX_LENGTHS, 0x26ff);
	}

	/* Configure the EMAC mode register. */
	val = REG_RD(bp, BNX2_EMAC_MODE);

	val &= ~(BNX2_EMAC_MODE_PORT | BNX2_EMAC_MODE_HALF_DUPLEX |
		BNX2_EMAC_MODE_MAC_LOOP | BNX2_EMAC_MODE_FORCE_LINK |
		BNX2_EMAC_MODE_25G_MODE);

	if (bp->link_up) {
		switch (bp->line_speed) {
			case SPEED_10:
				if (CHIP_NUM(bp) != CHIP_NUM_5706) {
					val |= BNX2_EMAC_MODE_PORT_MII_10M;
					break;
				}
				/* fall through */
			case SPEED_100:
				val |= BNX2_EMAC_MODE_PORT_MII;
				break;
			case SPEED_2500:
				val |= BNX2_EMAC_MODE_25G_MODE;
				/* fall through */
			case SPEED_1000:
				val |= BNX2_EMAC_MODE_PORT_GMII;
				break;
		}
	}
	else {
		val |= BNX2_EMAC_MODE_PORT_GMII;
	}

	/* Set the MAC to operate in the appropriate duplex mode. */
	if (bp->duplex == DUPLEX_HALF)
		val |= BNX2_EMAC_MODE_HALF_DUPLEX;
	REG_WR(bp, BNX2_EMAC_MODE, val);

	/* Enable/disable rx PAUSE. */
	bp->rx_mode &= ~BNX2_EMAC_RX_MODE_FLOW_EN;

	if (bp->flow_ctrl & FLOW_CTRL_RX)
		bp->rx_mode |= BNX2_EMAC_RX_MODE_FLOW_EN;
	REG_WR(bp, BNX2_EMAC_RX_MODE, bp->rx_mode);

	/* Enable/disable tx PAUSE. */
	val = REG_RD(bp, BNX2_EMAC_TX_MODE);
	val &= ~BNX2_EMAC_TX_MODE_FLOW_EN;

	if (bp->flow_ctrl & FLOW_CTRL_TX)
		val |= BNX2_EMAC_TX_MODE_FLOW_EN;
	REG_WR(bp, BNX2_EMAC_TX_MODE, val);

	/* Acknowledge the interrupt. */
	REG_WR(bp, BNX2_EMAC_STATUS, BNX2_EMAC_STATUS_LINK_CHANGE);

	if (CHIP_NUM(bp) == CHIP_NUM_5709)
		bnx2_init_all_rx_contexts(bp);
}

static void
bnx2_enable_bmsr1(struct bnx2 *bp)
{
	if ((bp->phy_flags & BNX2_PHY_FLAG_SERDES) &&
	    (CHIP_NUM(bp) == CHIP_NUM_5709))
		bnx2_write_phy(bp, MII_BNX2_BLK_ADDR,
			       MII_BNX2_BLK_ADDR_GP_STATUS);
}

static void
bnx2_disable_bmsr1(struct bnx2 *bp)
{
	if ((bp->phy_flags & BNX2_PHY_FLAG_SERDES) &&
	    (CHIP_NUM(bp) == CHIP_NUM_5709))
		bnx2_write_phy(bp, MII_BNX2_BLK_ADDR,
			       MII_BNX2_BLK_ADDR_COMBO_IEEEB0);
}

static int
bnx2_test_and_enable_2g5(struct bnx2 *bp)
{
	u32 up1;
	int ret = 1;

	if (!(bp->phy_flags & BNX2_PHY_FLAG_2_5G_CAPABLE))
		return 0;

	if (bp->autoneg & AUTONEG_SPEED)
		bp->advertising |= ADVERTISED_2500baseX_Full;

	if (CHIP_NUM(bp) == CHIP_NUM_5709)
		bnx2_write_phy(bp, MII_BNX2_BLK_ADDR, MII_BNX2_BLK_ADDR_OVER1G);

	bnx2_read_phy(bp, bp->mii_up1, &up1);
	if (!(up1 & BCM5708S_UP1_2G5)) {
		up1 |= BCM5708S_UP1_2G5;
		bnx2_write_phy(bp, bp->mii_up1, up1);
		ret = 0;
	}

	if (CHIP_NUM(bp) == CHIP_NUM_5709)
		bnx2_write_phy(bp, MII_BNX2_BLK_ADDR,
			       MII_BNX2_BLK_ADDR_COMBO_IEEEB0);

	return ret;
}

static int
bnx2_test_and_disable_2g5(struct bnx2 *bp)
{
	u32 up1;
	int ret = 0;

	if (!(bp->phy_flags & BNX2_PHY_FLAG_2_5G_CAPABLE))
		return 0;

	if (CHIP_NUM(bp) == CHIP_NUM_5709)
		bnx2_write_phy(bp, MII_BNX2_BLK_ADDR, MII_BNX2_BLK_ADDR_OVER1G);

	bnx2_read_phy(bp, bp->mii_up1, &up1);
	if (up1 & BCM5708S_UP1_2G5) {
		up1 &= ~BCM5708S_UP1_2G5;
		bnx2_write_phy(bp, bp->mii_up1, up1);
		ret = 1;
	}

	if (CHIP_NUM(bp) == CHIP_NUM_5709)
		bnx2_write_phy(bp, MII_BNX2_BLK_ADDR,
			       MII_BNX2_BLK_ADDR_COMBO_IEEEB0);

	return ret;
}

static void
bnx2_enable_forced_2g5(struct bnx2 *bp)
{
	u32 bmcr;

	if (!(bp->phy_flags & BNX2_PHY_FLAG_2_5G_CAPABLE))
		return;

	if (CHIP_NUM(bp) == CHIP_NUM_5709) {
		u32 val;

		bnx2_write_phy(bp, MII_BNX2_BLK_ADDR,
			       MII_BNX2_BLK_ADDR_SERDES_DIG);
		bnx2_read_phy(bp, MII_BNX2_SERDES_DIG_MISC1, &val);
		val &= ~MII_BNX2_SD_MISC1_FORCE_MSK;
		val |= MII_BNX2_SD_MISC1_FORCE | MII_BNX2_SD_MISC1_FORCE_2_5G;
		bnx2_write_phy(bp, MII_BNX2_SERDES_DIG_MISC1, val);

		bnx2_write_phy(bp, MII_BNX2_BLK_ADDR,
			       MII_BNX2_BLK_ADDR_COMBO_IEEEB0);
		bnx2_read_phy(bp, bp->mii_bmcr, &bmcr);

	} else if (CHIP_NUM(bp) == CHIP_NUM_5708) {
		bnx2_read_phy(bp, bp->mii_bmcr, &bmcr);
		bmcr |= BCM5708S_BMCR_FORCE_2500;
	}

	if (bp->autoneg & AUTONEG_SPEED) {
		bmcr &= ~BMCR_ANENABLE;
		if (bp->req_duplex == DUPLEX_FULL)
			bmcr |= BMCR_FULLDPLX;
	}
	bnx2_write_phy(bp, bp->mii_bmcr, bmcr);
}

static void
bnx2_disable_forced_2g5(struct bnx2 *bp)
{
	u32 bmcr;

	if (!(bp->phy_flags & BNX2_PHY_FLAG_2_5G_CAPABLE))
		return;

	if (CHIP_NUM(bp) == CHIP_NUM_5709) {
		u32 val;

		bnx2_write_phy(bp, MII_BNX2_BLK_ADDR,
			       MII_BNX2_BLK_ADDR_SERDES_DIG);
		bnx2_read_phy(bp, MII_BNX2_SERDES_DIG_MISC1, &val);
		val &= ~MII_BNX2_SD_MISC1_FORCE;
		bnx2_write_phy(bp, MII_BNX2_SERDES_DIG_MISC1, val);

		bnx2_write_phy(bp, MII_BNX2_BLK_ADDR,
			       MII_BNX2_BLK_ADDR_COMBO_IEEEB0);
		bnx2_read_phy(bp, bp->mii_bmcr, &bmcr);

	} else if (CHIP_NUM(bp) == CHIP_NUM_5708) {
		bnx2_read_phy(bp, bp->mii_bmcr, &bmcr);
		bmcr &= ~BCM5708S_BMCR_FORCE_2500;
	}

	if (bp->autoneg & AUTONEG_SPEED)
		bmcr |= BMCR_SPEED1000 | BMCR_ANENABLE | BMCR_ANRESTART;
	bnx2_write_phy(bp, bp->mii_bmcr, bmcr);
}

static void
bnx2_5706s_force_link_dn(struct bnx2 *bp, int start)
{
	u32 val;

	bnx2_write_phy(bp, MII_BNX2_DSP_ADDRESS, MII_EXPAND_SERDES_CTL);
	bnx2_read_phy(bp, MII_BNX2_DSP_RW_PORT, &val);
	if (start)
		bnx2_write_phy(bp, MII_BNX2_DSP_RW_PORT, val & 0xff0f);
	else
		bnx2_write_phy(bp, MII_BNX2_DSP_RW_PORT, val | 0xc0);
}

static int
bnx2_set_link(struct bnx2 *bp)
{
	u32 bmsr;
	u8 link_up;

	if (bp->loopback == MAC_LOOPBACK || bp->loopback == PHY_LOOPBACK) {
		bp->link_up = 1;
		return 0;
	}

	if (bp->phy_flags & BNX2_PHY_FLAG_REMOTE_PHY_CAP)
		return 0;

	link_up = bp->link_up;

	bnx2_enable_bmsr1(bp);
	bnx2_read_phy(bp, bp->mii_bmsr1, &bmsr);
	bnx2_read_phy(bp, bp->mii_bmsr1, &bmsr);
	bnx2_disable_bmsr1(bp);

	if ((bp->phy_flags & BNX2_PHY_FLAG_SERDES) &&
	    (CHIP_NUM(bp) == CHIP_NUM_5706)) {
		u32 val, an_dbg;

		if (bp->phy_flags & BNX2_PHY_FLAG_FORCED_DOWN) {
			bnx2_5706s_force_link_dn(bp, 0);
			bp->phy_flags &= ~BNX2_PHY_FLAG_FORCED_DOWN;
		}
		val = REG_RD(bp, BNX2_EMAC_STATUS);

		bnx2_write_phy(bp, MII_BNX2_MISC_SHADOW, MISC_SHDW_AN_DBG);
		bnx2_read_phy(bp, MII_BNX2_MISC_SHADOW, &an_dbg);
		bnx2_read_phy(bp, MII_BNX2_MISC_SHADOW, &an_dbg);

		if ((val & BNX2_EMAC_STATUS_LINK) &&
		    !(an_dbg & MISC_SHDW_AN_DBG_NOSYNC))
			bmsr |= BMSR_LSTATUS;
		else
			bmsr &= ~BMSR_LSTATUS;
	}

	if (bmsr & BMSR_LSTATUS) {
		bp->link_up = 1;

		if (bp->phy_flags & BNX2_PHY_FLAG_SERDES) {
			if (CHIP_NUM(bp) == CHIP_NUM_5706)
				bnx2_5706s_linkup(bp);
			else if (CHIP_NUM(bp) == CHIP_NUM_5708)
				bnx2_5708s_linkup(bp);
			else if (CHIP_NUM(bp) == CHIP_NUM_5709)
				bnx2_5709s_linkup(bp);
		}
		else {
			bnx2_copper_linkup(bp);
		}
		bnx2_resolve_flow_ctrl(bp);
	}
	else {
		if ((bp->phy_flags & BNX2_PHY_FLAG_SERDES) &&
		    (bp->autoneg & AUTONEG_SPEED))
			bnx2_disable_forced_2g5(bp);

		if (bp->phy_flags & BNX2_PHY_FLAG_PARALLEL_DETECT) {
			u32 bmcr;

			bnx2_read_phy(bp, bp->mii_bmcr, &bmcr);
			bmcr |= BMCR_ANENABLE;
			bnx2_write_phy(bp, bp->mii_bmcr, bmcr);

			bp->phy_flags &= ~BNX2_PHY_FLAG_PARALLEL_DETECT;
		}
		bp->link_up = 0;
	}

	if (bp->link_up != link_up) {
		bnx2_report_link(bp);
	}

	bnx2_set_mac_link(bp);

	return 0;
}

static int
bnx2_reset_phy(struct bnx2 *bp)
{
	int i;
	u32 reg;

        bnx2_write_phy(bp, bp->mii_bmcr, BMCR_RESET);

#define PHY_RESET_MAX_WAIT 100
	for (i = 0; i < PHY_RESET_MAX_WAIT; i++) {
		udelay(10);

		bnx2_read_phy(bp, bp->mii_bmcr, &reg);
		if (!(reg & BMCR_RESET)) {
			udelay(20);
			break;
		}
	}
	if (i == PHY_RESET_MAX_WAIT) {
		return -EBUSY;
	}
	return 0;
}

static u32
bnx2_phy_get_pause_adv(struct bnx2 *bp)
{
	u32 adv = 0;

	if ((bp->req_flow_ctrl & (FLOW_CTRL_RX | FLOW_CTRL_TX)) ==
		(FLOW_CTRL_RX | FLOW_CTRL_TX)) {

		if (bp->phy_flags & BNX2_PHY_FLAG_SERDES) {
			adv = ADVERTISE_1000XPAUSE;
		}
		else {
			adv = ADVERTISE_PAUSE_CAP;
		}
	}
	else if (bp->req_flow_ctrl & FLOW_CTRL_TX) {
		if (bp->phy_flags & BNX2_PHY_FLAG_SERDES) {
			adv = ADVERTISE_1000XPSE_ASYM;
		}
		else {
			adv = ADVERTISE_PAUSE_ASYM;
		}
	}
	else if (bp->req_flow_ctrl & FLOW_CTRL_RX) {
		if (bp->phy_flags & BNX2_PHY_FLAG_SERDES) {
			adv = ADVERTISE_1000XPAUSE | ADVERTISE_1000XPSE_ASYM;
		}
		else {
			adv = ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;
		}
	}
	return adv;
}

static int bnx2_fw_sync(struct bnx2 *, u32, int, int);

static int
bnx2_setup_remote_phy(struct bnx2 *bp, u8 port)
__releases(&bp->phy_lock)
__acquires(&bp->phy_lock)
{
	u32 speed_arg = 0, pause_adv;

	pause_adv = bnx2_phy_get_pause_adv(bp);

	if (bp->autoneg & AUTONEG_SPEED) {
		speed_arg |= BNX2_NETLINK_SET_LINK_ENABLE_AUTONEG;
		if (bp->advertising & ADVERTISED_10baseT_Half)
			speed_arg |= BNX2_NETLINK_SET_LINK_SPEED_10HALF;
		if (bp->advertising & ADVERTISED_10baseT_Full)
			speed_arg |= BNX2_NETLINK_SET_LINK_SPEED_10FULL;
		if (bp->advertising & ADVERTISED_100baseT_Half)
			speed_arg |= BNX2_NETLINK_SET_LINK_SPEED_100HALF;
		if (bp->advertising & ADVERTISED_100baseT_Full)
			speed_arg |= BNX2_NETLINK_SET_LINK_SPEED_100FULL;
		if (bp->advertising & ADVERTISED_1000baseT_Full)
			speed_arg |= BNX2_NETLINK_SET_LINK_SPEED_1GFULL;
		if (bp->advertising & ADVERTISED_2500baseX_Full)
			speed_arg |= BNX2_NETLINK_SET_LINK_SPEED_2G5FULL;
	} else {
		if (bp->req_line_speed == SPEED_2500)
			speed_arg = BNX2_NETLINK_SET_LINK_SPEED_2G5FULL;
		else if (bp->req_line_speed == SPEED_1000)
			speed_arg = BNX2_NETLINK_SET_LINK_SPEED_1GFULL;
		else if (bp->req_line_speed == SPEED_100) {
			if (bp->req_duplex == DUPLEX_FULL)
				speed_arg = BNX2_NETLINK_SET_LINK_SPEED_100FULL;
			else
				speed_arg = BNX2_NETLINK_SET_LINK_SPEED_100HALF;
		} else if (bp->req_line_speed == SPEED_10) {
			if (bp->req_duplex == DUPLEX_FULL)
				speed_arg = BNX2_NETLINK_SET_LINK_SPEED_10FULL;
			else
				speed_arg = BNX2_NETLINK_SET_LINK_SPEED_10HALF;
		}
	}

	if (pause_adv & (ADVERTISE_1000XPAUSE | ADVERTISE_PAUSE_CAP))
		speed_arg |= BNX2_NETLINK_SET_LINK_FC_SYM_PAUSE;
	if (pause_adv & (ADVERTISE_1000XPSE_ASYM | ADVERTISE_PAUSE_ASYM))
		speed_arg |= BNX2_NETLINK_SET_LINK_FC_ASYM_PAUSE;

	if (port == PORT_TP)
		speed_arg |= BNX2_NETLINK_SET_LINK_PHY_APP_REMOTE |
			     BNX2_NETLINK_SET_LINK_ETH_AT_WIRESPEED;

	bnx2_shmem_wr(bp, BNX2_DRV_MB_ARG0, speed_arg);

	spin_unlock_bh(&bp->phy_lock);
	bnx2_fw_sync(bp, BNX2_DRV_MSG_CODE_CMD_SET_LINK, 1, 0);
	spin_lock_bh(&bp->phy_lock);

	return 0;
}

static int
bnx2_setup_serdes_phy(struct bnx2 *bp, u8 port)
__releases(&bp->phy_lock)
__acquires(&bp->phy_lock)
{
	u32 adv, bmcr;
	u32 new_adv = 0;

	if (bp->phy_flags & BNX2_PHY_FLAG_REMOTE_PHY_CAP)
		return (bnx2_setup_remote_phy(bp, port));

	if (!(bp->autoneg & AUTONEG_SPEED)) {
		u32 new_bmcr;
		int force_link_down = 0;

		if (bp->req_line_speed == SPEED_2500) {
			if (!bnx2_test_and_enable_2g5(bp))
				force_link_down = 1;
		} else if (bp->req_line_speed == SPEED_1000) {
			if (bnx2_test_and_disable_2g5(bp))
				force_link_down = 1;
		}
		bnx2_read_phy(bp, bp->mii_adv, &adv);
		adv &= ~(ADVERTISE_1000XFULL | ADVERTISE_1000XHALF);

		bnx2_read_phy(bp, bp->mii_bmcr, &bmcr);
		new_bmcr = bmcr & ~BMCR_ANENABLE;
		new_bmcr |= BMCR_SPEED1000;

		if (CHIP_NUM(bp) == CHIP_NUM_5709) {
			if (bp->req_line_speed == SPEED_2500)
				bnx2_enable_forced_2g5(bp);
			else if (bp->req_line_speed == SPEED_1000) {
				bnx2_disable_forced_2g5(bp);
				new_bmcr &= ~0x2000;
			}

		} else if (CHIP_NUM(bp) == CHIP_NUM_5708) {
			if (bp->req_line_speed == SPEED_2500)
				new_bmcr |= BCM5708S_BMCR_FORCE_2500;
			else
				new_bmcr = bmcr & ~BCM5708S_BMCR_FORCE_2500;
		}

		if (bp->req_duplex == DUPLEX_FULL) {
			adv |= ADVERTISE_1000XFULL;
			new_bmcr |= BMCR_FULLDPLX;
		}
		else {
			adv |= ADVERTISE_1000XHALF;
			new_bmcr &= ~BMCR_FULLDPLX;
		}
		if ((new_bmcr != bmcr) || (force_link_down)) {
			/* Force a link down visible on the other side */
			if (bp->link_up) {
				bnx2_write_phy(bp, bp->mii_adv, adv &
					       ~(ADVERTISE_1000XFULL |
						 ADVERTISE_1000XHALF));
				bnx2_write_phy(bp, bp->mii_bmcr, bmcr |
					BMCR_ANRESTART | BMCR_ANENABLE);

				bp->link_up = 0;
				netif_carrier_off(bp->dev);
				bnx2_write_phy(bp, bp->mii_bmcr, new_bmcr);
				bnx2_report_link(bp);
			}
			bnx2_write_phy(bp, bp->mii_adv, adv);
			bnx2_write_phy(bp, bp->mii_bmcr, new_bmcr);
		} else {
			bnx2_resolve_flow_ctrl(bp);
			bnx2_set_mac_link(bp);
		}
		return 0;
	}

	bnx2_test_and_enable_2g5(bp);

	if (bp->advertising & ADVERTISED_1000baseT_Full)
		new_adv |= ADVERTISE_1000XFULL;

	new_adv |= bnx2_phy_get_pause_adv(bp);

	bnx2_read_phy(bp, bp->mii_adv, &adv);
	bnx2_read_phy(bp, bp->mii_bmcr, &bmcr);

	bp->serdes_an_pending = 0;
	if ((adv != new_adv) || ((bmcr & BMCR_ANENABLE) == 0)) {
		/* Force a link down visible on the other side */
		if (bp->link_up) {
			bnx2_write_phy(bp, bp->mii_bmcr, BMCR_LOOPBACK);
			spin_unlock_bh(&bp->phy_lock);
			msleep(20);
			spin_lock_bh(&bp->phy_lock);
		}

		bnx2_write_phy(bp, bp->mii_adv, new_adv);
		bnx2_write_phy(bp, bp->mii_bmcr, bmcr | BMCR_ANRESTART |
			BMCR_ANENABLE);
		/* Speed up link-up time when the link partner
		 * does not autonegotiate which is very common
		 * in blade servers. Some blade servers use
		 * IPMI for kerboard input and it's important
		 * to minimize link disruptions. Autoneg. involves
		 * exchanging base pages plus 3 next pages and
		 * normally completes in about 120 msec.
		 */
		bp->current_interval = BNX2_SERDES_AN_TIMEOUT;
		bp->serdes_an_pending = 1;
		mod_timer(&bp->timer, jiffies + bp->current_interval);
	} else {
		bnx2_resolve_flow_ctrl(bp);
		bnx2_set_mac_link(bp);
	}

	return 0;
}

#define ETHTOOL_ALL_FIBRE_SPEED						\
	(bp->phy_flags & BNX2_PHY_FLAG_2_5G_CAPABLE) ?			\
		(ADVERTISED_2500baseX_Full | ADVERTISED_1000baseT_Full) :\
		(ADVERTISED_1000baseT_Full)

#define ETHTOOL_ALL_COPPER_SPEED					\
	(ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full |		\
	ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full |		\
	ADVERTISED_1000baseT_Full)

#define PHY_ALL_10_100_SPEED (ADVERTISE_10HALF | ADVERTISE_10FULL | \
	ADVERTISE_100HALF | ADVERTISE_100FULL | ADVERTISE_CSMA)

#define PHY_ALL_1000_SPEED (ADVERTISE_1000HALF | ADVERTISE_1000FULL)

static void
bnx2_set_default_remote_link(struct bnx2 *bp)
{
	u32 link;

	if (bp->phy_port == PORT_TP)
		link = bnx2_shmem_rd(bp, BNX2_RPHY_COPPER_LINK);
	else
		link = bnx2_shmem_rd(bp, BNX2_RPHY_SERDES_LINK);

	if (link & BNX2_NETLINK_SET_LINK_ENABLE_AUTONEG) {
		bp->req_line_speed = 0;
		bp->autoneg |= AUTONEG_SPEED;
		bp->advertising = ADVERTISED_Autoneg;
		if (link & BNX2_NETLINK_SET_LINK_SPEED_10HALF)
			bp->advertising |= ADVERTISED_10baseT_Half;
		if (link & BNX2_NETLINK_SET_LINK_SPEED_10FULL)
			bp->advertising |= ADVERTISED_10baseT_Full;
		if (link & BNX2_NETLINK_SET_LINK_SPEED_100HALF)
			bp->advertising |= ADVERTISED_100baseT_Half;
		if (link & BNX2_NETLINK_SET_LINK_SPEED_100FULL)
			bp->advertising |= ADVERTISED_100baseT_Full;
		if (link & BNX2_NETLINK_SET_LINK_SPEED_1GFULL)
			bp->advertising |= ADVERTISED_1000baseT_Full;
		if (link & BNX2_NETLINK_SET_LINK_SPEED_2G5FULL)
			bp->advertising |= ADVERTISED_2500baseX_Full;
	} else {
		bp->autoneg = 0;
		bp->advertising = 0;
		bp->req_duplex = DUPLEX_FULL;
		if (link & BNX2_NETLINK_SET_LINK_SPEED_10) {
			bp->req_line_speed = SPEED_10;
			if (link & BNX2_NETLINK_SET_LINK_SPEED_10HALF)
				bp->req_duplex = DUPLEX_HALF;
		}
		if (link & BNX2_NETLINK_SET_LINK_SPEED_100) {
			bp->req_line_speed = SPEED_100;
			if (link & BNX2_NETLINK_SET_LINK_SPEED_100HALF)
				bp->req_duplex = DUPLEX_HALF;
		}
		if (link & BNX2_NETLINK_SET_LINK_SPEED_1GFULL)
			bp->req_line_speed = SPEED_1000;
		if (link & BNX2_NETLINK_SET_LINK_SPEED_2G5FULL)
			bp->req_line_speed = SPEED_2500;
	}
}

static void
bnx2_set_default_link(struct bnx2 *bp)
{
	if (bp->phy_flags & BNX2_PHY_FLAG_REMOTE_PHY_CAP) {
		bnx2_set_default_remote_link(bp);
		return;
	}

	bp->autoneg = AUTONEG_SPEED | AUTONEG_FLOW_CTRL;
	bp->req_line_speed = 0;
	if (bp->phy_flags & BNX2_PHY_FLAG_SERDES) {
		u32 reg;

		bp->advertising = ETHTOOL_ALL_FIBRE_SPEED | ADVERTISED_Autoneg;

		reg = bnx2_shmem_rd(bp, BNX2_PORT_HW_CFG_CONFIG);
		reg &= BNX2_PORT_HW_CFG_CFG_DFLT_LINK_MASK;
		if (reg == BNX2_PORT_HW_CFG_CFG_DFLT_LINK_1G) {
			bp->autoneg = 0;
			bp->req_line_speed = bp->line_speed = SPEED_1000;
			bp->req_duplex = DUPLEX_FULL;
		}
	} else
		bp->advertising = ETHTOOL_ALL_COPPER_SPEED | ADVERTISED_Autoneg;
}

static void
bnx2_send_heart_beat(struct bnx2 *bp)
{
	u32 msg;
	u32 addr;

	spin_lock(&bp->indirect_lock);
	msg = (u32) (++bp->fw_drv_pulse_wr_seq & BNX2_DRV_PULSE_SEQ_MASK);
	addr = bp->shmem_base + BNX2_DRV_PULSE_MB;
	REG_WR(bp, BNX2_PCICFG_REG_WINDOW_ADDRESS, addr);
	REG_WR(bp, BNX2_PCICFG_REG_WINDOW, msg);
	spin_unlock(&bp->indirect_lock);
}

static void
bnx2_remote_phy_event(struct bnx2 *bp)
{
	u32 msg;
	u8 link_up = bp->link_up;
	u8 old_port;

	msg = bnx2_shmem_rd(bp, BNX2_LINK_STATUS);

	if (msg & BNX2_LINK_STATUS_HEART_BEAT_EXPIRED)
		bnx2_send_heart_beat(bp);

	msg &= ~BNX2_LINK_STATUS_HEART_BEAT_EXPIRED;

	if ((msg & BNX2_LINK_STATUS_LINK_UP) == BNX2_LINK_STATUS_LINK_DOWN)
		bp->link_up = 0;
	else {
		u32 speed;

		bp->link_up = 1;
		speed = msg & BNX2_LINK_STATUS_SPEED_MASK;
		bp->duplex = DUPLEX_FULL;
		switch (speed) {
			case BNX2_LINK_STATUS_10HALF:
				bp->duplex = DUPLEX_HALF;
			case BNX2_LINK_STATUS_10FULL:
				bp->line_speed = SPEED_10;
				break;
			case BNX2_LINK_STATUS_100HALF:
				bp->duplex = DUPLEX_HALF;
			case BNX2_LINK_STATUS_100BASE_T4:
			case BNX2_LINK_STATUS_100FULL:
				bp->line_speed = SPEED_100;
				break;
			case BNX2_LINK_STATUS_1000HALF:
				bp->duplex = DUPLEX_HALF;
			case BNX2_LINK_STATUS_1000FULL:
				bp->line_speed = SPEED_1000;
				break;
			case BNX2_LINK_STATUS_2500HALF:
				bp->duplex = DUPLEX_HALF;
			case BNX2_LINK_STATUS_2500FULL:
				bp->line_speed = SPEED_2500;
				break;
			default:
				bp->line_speed = 0;
				break;
		}

		bp->flow_ctrl = 0;
		if ((bp->autoneg & (AUTONEG_SPEED | AUTONEG_FLOW_CTRL)) !=
		    (AUTONEG_SPEED | AUTONEG_FLOW_CTRL)) {
			if (bp->duplex == DUPLEX_FULL)
				bp->flow_ctrl = bp->req_flow_ctrl;
		} else {
			if (msg & BNX2_LINK_STATUS_TX_FC_ENABLED)
				bp->flow_ctrl |= FLOW_CTRL_TX;
			if (msg & BNX2_LINK_STATUS_RX_FC_ENABLED)
				bp->flow_ctrl |= FLOW_CTRL_RX;
		}

		old_port = bp->phy_port;
		if (msg & BNX2_LINK_STATUS_SERDES_LINK)
			bp->phy_port = PORT_FIBRE;
		else
			bp->phy_port = PORT_TP;

		if (old_port != bp->phy_port)
			bnx2_set_default_link(bp);

	}
	if (bp->link_up != link_up)
		bnx2_report_link(bp);

	bnx2_set_mac_link(bp);
}

static int
bnx2_set_remote_link(struct bnx2 *bp)
{
	u32 evt_code;

	evt_code = bnx2_shmem_rd(bp, BNX2_FW_EVT_CODE_MB);
	switch (evt_code) {
		case BNX2_FW_EVT_CODE_LINK_EVENT:
			bnx2_remote_phy_event(bp);
			break;
		case BNX2_FW_EVT_CODE_SW_TIMER_EXPIRATION_EVENT:
		default:
			bnx2_send_heart_beat(bp);
			break;
	}
	return 0;
}

static int
bnx2_setup_copper_phy(struct bnx2 *bp)
__releases(&bp->phy_lock)
__acquires(&bp->phy_lock)
{
	u32 bmcr;
	u32 new_bmcr;

	bnx2_read_phy(bp, bp->mii_bmcr, &bmcr);

	if (bp->autoneg & AUTONEG_SPEED) {
		u32 adv_reg, adv1000_reg;
		u32 new_adv_reg = 0;
		u32 new_adv1000_reg = 0;

		bnx2_read_phy(bp, bp->mii_adv, &adv_reg);
		adv_reg &= (PHY_ALL_10_100_SPEED | ADVERTISE_PAUSE_CAP |
			ADVERTISE_PAUSE_ASYM);

		bnx2_read_phy(bp, MII_CTRL1000, &adv1000_reg);
		adv1000_reg &= PHY_ALL_1000_SPEED;

		if (bp->advertising & ADVERTISED_10baseT_Half)
			new_adv_reg |= ADVERTISE_10HALF;
		if (bp->advertising & ADVERTISED_10baseT_Full)
			new_adv_reg |= ADVERTISE_10FULL;
		if (bp->advertising & ADVERTISED_100baseT_Half)
			new_adv_reg |= ADVERTISE_100HALF;
		if (bp->advertising & ADVERTISED_100baseT_Full)
			new_adv_reg |= ADVERTISE_100FULL;
		if (bp->advertising & ADVERTISED_1000baseT_Full)
			new_adv1000_reg |= ADVERTISE_1000FULL;

		new_adv_reg |= ADVERTISE_CSMA;

		new_adv_reg |= bnx2_phy_get_pause_adv(bp);

		if ((adv1000_reg != new_adv1000_reg) ||
			(adv_reg != new_adv_reg) ||
			((bmcr & BMCR_ANENABLE) == 0)) {

			bnx2_write_phy(bp, bp->mii_adv, new_adv_reg);
			bnx2_write_phy(bp, MII_CTRL1000, new_adv1000_reg);
			bnx2_write_phy(bp, bp->mii_bmcr, BMCR_ANRESTART |
				BMCR_ANENABLE);
		}
		else if (bp->link_up) {
			/* Flow ctrl may have changed from auto to forced */
			/* or vice-versa. */

			bnx2_resolve_flow_ctrl(bp);
			bnx2_set_mac_link(bp);
		}
		return 0;
	}

	new_bmcr = 0;
	if (bp->req_line_speed == SPEED_100) {
		new_bmcr |= BMCR_SPEED100;
	}
	if (bp->req_duplex == DUPLEX_FULL) {
		new_bmcr |= BMCR_FULLDPLX;
	}
	if (new_bmcr != bmcr) {
		u32 bmsr;

		bnx2_read_phy(bp, bp->mii_bmsr, &bmsr);
		bnx2_read_phy(bp, bp->mii_bmsr, &bmsr);

		if (bmsr & BMSR_LSTATUS) {
			/* Force link down */
			bnx2_write_phy(bp, bp->mii_bmcr, BMCR_LOOPBACK);
			spin_unlock_bh(&bp->phy_lock);
			msleep(50);
			spin_lock_bh(&bp->phy_lock);

			bnx2_read_phy(bp, bp->mii_bmsr, &bmsr);
			bnx2_read_phy(bp, bp->mii_bmsr, &bmsr);
		}

		bnx2_write_phy(bp, bp->mii_bmcr, new_bmcr);

		/* Normally, the new speed is setup after the link has
		 * gone down and up again. In some cases, link will not go
		 * down so we need to set up the new speed here.
		 */
		if (bmsr & BMSR_LSTATUS) {
			bp->line_speed = bp->req_line_speed;
			bp->duplex = bp->req_duplex;
			bnx2_resolve_flow_ctrl(bp);
			bnx2_set_mac_link(bp);
		}
	} else {
		bnx2_resolve_flow_ctrl(bp);
		bnx2_set_mac_link(bp);
	}
	return 0;
}

static int
bnx2_setup_phy(struct bnx2 *bp, u8 port)
__releases(&bp->phy_lock)
__acquires(&bp->phy_lock)
{
	if (bp->loopback == MAC_LOOPBACK)
		return 0;

	if (bp->phy_flags & BNX2_PHY_FLAG_SERDES) {
		return (bnx2_setup_serdes_phy(bp, port));
	}
	else {
		return (bnx2_setup_copper_phy(bp));
	}
}

static int
bnx2_init_5709s_phy(struct bnx2 *bp, int reset_phy)
{
	u32 val;

	bp->mii_bmcr = MII_BMCR + 0x10;
	bp->mii_bmsr = MII_BMSR + 0x10;
	bp->mii_bmsr1 = MII_BNX2_GP_TOP_AN_STATUS1;
	bp->mii_adv = MII_ADVERTISE + 0x10;
	bp->mii_lpa = MII_LPA + 0x10;
	bp->mii_up1 = MII_BNX2_OVER1G_UP1;

	bnx2_write_phy(bp, MII_BNX2_BLK_ADDR, MII_BNX2_BLK_ADDR_AER);
	bnx2_write_phy(bp, MII_BNX2_AER_AER, MII_BNX2_AER_AER_AN_MMD);

	bnx2_write_phy(bp, MII_BNX2_BLK_ADDR, MII_BNX2_BLK_ADDR_COMBO_IEEEB0);
	if (reset_phy)
		bnx2_reset_phy(bp);

	bnx2_write_phy(bp, MII_BNX2_BLK_ADDR, MII_BNX2_BLK_ADDR_SERDES_DIG);

	bnx2_read_phy(bp, MII_BNX2_SERDES_DIG_1000XCTL1, &val);
	val &= ~MII_BNX2_SD_1000XCTL1_AUTODET;
	val |= MII_BNX2_SD_1000XCTL1_FIBER;
	bnx2_write_phy(bp, MII_BNX2_SERDES_DIG_1000XCTL1, val);

	bnx2_write_phy(bp, MII_BNX2_BLK_ADDR, MII_BNX2_BLK_ADDR_OVER1G);
	bnx2_read_phy(bp, MII_BNX2_OVER1G_UP1, &val);
	if (bp->phy_flags & BNX2_PHY_FLAG_2_5G_CAPABLE)
		val |= BCM5708S_UP1_2G5;
	else
		val &= ~BCM5708S_UP1_2G5;
	bnx2_write_phy(bp, MII_BNX2_OVER1G_UP1, val);

	bnx2_write_phy(bp, MII_BNX2_BLK_ADDR, MII_BNX2_BLK_ADDR_BAM_NXTPG);
	bnx2_read_phy(bp, MII_BNX2_BAM_NXTPG_CTL, &val);
	val |= MII_BNX2_NXTPG_CTL_T2 | MII_BNX2_NXTPG_CTL_BAM;
	bnx2_write_phy(bp, MII_BNX2_BAM_NXTPG_CTL, val);

	bnx2_write_phy(bp, MII_BNX2_BLK_ADDR, MII_BNX2_BLK_ADDR_CL73_USERB0);

	val = MII_BNX2_CL73_BAM_EN | MII_BNX2_CL73_BAM_STA_MGR_EN |
	      MII_BNX2_CL73_BAM_NP_AFT_BP_EN;
	bnx2_write_phy(bp, MII_BNX2_CL73_BAM_CTL1, val);

	bnx2_write_phy(bp, MII_BNX2_BLK_ADDR, MII_BNX2_BLK_ADDR_COMBO_IEEEB0);

	return 0;
}

static int
bnx2_init_5708s_phy(struct bnx2 *bp, int reset_phy)
{
	u32 val;

	if (reset_phy)
		bnx2_reset_phy(bp);

	bp->mii_up1 = BCM5708S_UP1;

	bnx2_write_phy(bp, BCM5708S_BLK_ADDR, BCM5708S_BLK_ADDR_DIG3);
	bnx2_write_phy(bp, BCM5708S_DIG_3_0, BCM5708S_DIG_3_0_USE_IEEE);
	bnx2_write_phy(bp, BCM5708S_BLK_ADDR, BCM5708S_BLK_ADDR_DIG);

	bnx2_read_phy(bp, BCM5708S_1000X_CTL1, &val);
	val |= BCM5708S_1000X_CTL1_FIBER_MODE | BCM5708S_1000X_CTL1_AUTODET_EN;
	bnx2_write_phy(bp, BCM5708S_1000X_CTL1, val);

	bnx2_read_phy(bp, BCM5708S_1000X_CTL2, &val);
	val |= BCM5708S_1000X_CTL2_PLLEL_DET_EN;
	bnx2_write_phy(bp, BCM5708S_1000X_CTL2, val);

	if (bp->phy_flags & BNX2_PHY_FLAG_2_5G_CAPABLE) {
		bnx2_read_phy(bp, BCM5708S_UP1, &val);
		val |= BCM5708S_UP1_2G5;
		bnx2_write_phy(bp, BCM5708S_UP1, val);
	}

	if ((CHIP_ID(bp) == CHIP_ID_5708_A0) ||
	    (CHIP_ID(bp) == CHIP_ID_5708_B0) ||
	    (CHIP_ID(bp) == CHIP_ID_5708_B1)) {
		/* increase tx signal amplitude */
		bnx2_write_phy(bp, BCM5708S_BLK_ADDR,
			       BCM5708S_BLK_ADDR_TX_MISC);
		bnx2_read_phy(bp, BCM5708S_TX_ACTL1, &val);
		val &= ~BCM5708S_TX_ACTL1_DRIVER_VCM;
		bnx2_write_phy(bp, BCM5708S_TX_ACTL1, val);
		bnx2_write_phy(bp, BCM5708S_BLK_ADDR, BCM5708S_BLK_ADDR_DIG);
	}

	val = bnx2_shmem_rd(bp, BNX2_PORT_HW_CFG_CONFIG) &
	      BNX2_PORT_HW_CFG_CFG_TXCTL3_MASK;

	if (val) {
		u32 is_backplane;

		is_backplane = bnx2_shmem_rd(bp, BNX2_SHARED_HW_CFG_CONFIG);
		if (is_backplane & BNX2_SHARED_HW_CFG_PHY_BACKPLANE) {
			bnx2_write_phy(bp, BCM5708S_BLK_ADDR,
				       BCM5708S_BLK_ADDR_TX_MISC);
			bnx2_write_phy(bp, BCM5708S_TX_ACTL3, val);
			bnx2_write_phy(bp, BCM5708S_BLK_ADDR,
				       BCM5708S_BLK_ADDR_DIG);
		}
	}
	return 0;
}

static int
bnx2_init_5706s_phy(struct bnx2 *bp, int reset_phy)
{
	if (reset_phy)
		bnx2_reset_phy(bp);

	bp->phy_flags &= ~BNX2_PHY_FLAG_PARALLEL_DETECT;

	if (CHIP_NUM(bp) == CHIP_NUM_5706)
        	REG_WR(bp, BNX2_MISC_GP_HW_CTL0, 0x300);

	if (bp->dev->mtu > 1500) {
		u32 val;

		/* Set extended packet length bit */
		bnx2_write_phy(bp, 0x18, 0x7);
		bnx2_read_phy(bp, 0x18, &val);
		bnx2_write_phy(bp, 0x18, (val & 0xfff8) | 0x4000);

		bnx2_write_phy(bp, 0x1c, 0x6c00);
		bnx2_read_phy(bp, 0x1c, &val);
		bnx2_write_phy(bp, 0x1c, (val & 0x3ff) | 0xec02);
	}
	else {
		u32 val;

		bnx2_write_phy(bp, 0x18, 0x7);
		bnx2_read_phy(bp, 0x18, &val);
		bnx2_write_phy(bp, 0x18, val & ~0x4007);

		bnx2_write_phy(bp, 0x1c, 0x6c00);
		bnx2_read_phy(bp, 0x1c, &val);
		bnx2_write_phy(bp, 0x1c, (val & 0x3fd) | 0xec00);
	}

	return 0;
}

static int
bnx2_init_copper_phy(struct bnx2 *bp, int reset_phy)
{
	u32 val;

	if (reset_phy)
		bnx2_reset_phy(bp);

	if (bp->phy_flags & BNX2_PHY_FLAG_CRC_FIX) {
		bnx2_write_phy(bp, 0x18, 0x0c00);
		bnx2_write_phy(bp, 0x17, 0x000a);
		bnx2_write_phy(bp, 0x15, 0x310b);
		bnx2_write_phy(bp, 0x17, 0x201f);
		bnx2_write_phy(bp, 0x15, 0x9506);
		bnx2_write_phy(bp, 0x17, 0x401f);
		bnx2_write_phy(bp, 0x15, 0x14e2);
		bnx2_write_phy(bp, 0x18, 0x0400);
	}

	if (bp->phy_flags & BNX2_PHY_FLAG_DIS_EARLY_DAC) {
		bnx2_write_phy(bp, MII_BNX2_DSP_ADDRESS,
			       MII_BNX2_DSP_EXPAND_REG | 0x8);
		bnx2_read_phy(bp, MII_BNX2_DSP_RW_PORT, &val);
		val &= ~(1 << 8);
		bnx2_write_phy(bp, MII_BNX2_DSP_RW_PORT, val);
	}

	if (bp->dev->mtu > 1500) {
		/* Set extended packet length bit */
		bnx2_write_phy(bp, 0x18, 0x7);
		bnx2_read_phy(bp, 0x18, &val);
		bnx2_write_phy(bp, 0x18, val | 0x4000);

		bnx2_read_phy(bp, 0x10, &val);
		bnx2_write_phy(bp, 0x10, val | 0x1);
	}
	else {
		bnx2_write_phy(bp, 0x18, 0x7);
		bnx2_read_phy(bp, 0x18, &val);
		bnx2_write_phy(bp, 0x18, val & ~0x4007);

		bnx2_read_phy(bp, 0x10, &val);
		bnx2_write_phy(bp, 0x10, val & ~0x1);
	}

	/* ethernet@wirespeed */
	bnx2_write_phy(bp, 0x18, 0x7007);
	bnx2_read_phy(bp, 0x18, &val);
	bnx2_write_phy(bp, 0x18, val | (1 << 15) | (1 << 4));
	return 0;
}


static int
bnx2_init_phy(struct bnx2 *bp, int reset_phy)
__releases(&bp->phy_lock)
__acquires(&bp->phy_lock)
{
	u32 val;
	int rc = 0;

	bp->phy_flags &= ~BNX2_PHY_FLAG_INT_MODE_MASK;
	bp->phy_flags |= BNX2_PHY_FLAG_INT_MODE_LINK_READY;

	bp->mii_bmcr = MII_BMCR;
	bp->mii_bmsr = MII_BMSR;
	bp->mii_bmsr1 = MII_BMSR;
	bp->mii_adv = MII_ADVERTISE;
	bp->mii_lpa = MII_LPA;

        REG_WR(bp, BNX2_EMAC_ATTENTION_ENA, BNX2_EMAC_ATTENTION_ENA_LINK);

	if (bp->phy_flags & BNX2_PHY_FLAG_REMOTE_PHY_CAP)
		goto setup_phy;

	bnx2_read_phy(bp, MII_PHYSID1, &val);
	bp->phy_id = val << 16;
	bnx2_read_phy(bp, MII_PHYSID2, &val);
	bp->phy_id |= val & 0xffff;

	if (bp->phy_flags & BNX2_PHY_FLAG_SERDES) {
		if (CHIP_NUM(bp) == CHIP_NUM_5706)
			rc = bnx2_init_5706s_phy(bp, reset_phy);
		else if (CHIP_NUM(bp) == CHIP_NUM_5708)
			rc = bnx2_init_5708s_phy(bp, reset_phy);
		else if (CHIP_NUM(bp) == CHIP_NUM_5709)
			rc = bnx2_init_5709s_phy(bp, reset_phy);
	}
	else {
		rc = bnx2_init_copper_phy(bp, reset_phy);
	}

setup_phy:
	if (!rc)
		rc = bnx2_setup_phy(bp, bp->phy_port);

	return rc;
}

static int
bnx2_set_mac_loopback(struct bnx2 *bp)
{
	u32 mac_mode;

	mac_mode = REG_RD(bp, BNX2_EMAC_MODE);
	mac_mode &= ~BNX2_EMAC_MODE_PORT;
	mac_mode |= BNX2_EMAC_MODE_MAC_LOOP | BNX2_EMAC_MODE_FORCE_LINK;
	REG_WR(bp, BNX2_EMAC_MODE, mac_mode);
	bp->link_up = 1;
	return 0;
}

static int bnx2_test_link(struct bnx2 *);

static int
bnx2_set_phy_loopback(struct bnx2 *bp)
{
	u32 mac_mode;
	int rc, i;

	spin_lock_bh(&bp->phy_lock);
	rc = bnx2_write_phy(bp, bp->mii_bmcr, BMCR_LOOPBACK | BMCR_FULLDPLX |
			    BMCR_SPEED1000);
	spin_unlock_bh(&bp->phy_lock);
	if (rc)
		return rc;

	for (i = 0; i < 10; i++) {
		if (bnx2_test_link(bp) == 0)
			break;
		msleep(100);
	}

	mac_mode = REG_RD(bp, BNX2_EMAC_MODE);
	mac_mode &= ~(BNX2_EMAC_MODE_PORT | BNX2_EMAC_MODE_HALF_DUPLEX |
		      BNX2_EMAC_MODE_MAC_LOOP | BNX2_EMAC_MODE_FORCE_LINK |
		      BNX2_EMAC_MODE_25G_MODE);

	mac_mode |= BNX2_EMAC_MODE_PORT_GMII;
	REG_WR(bp, BNX2_EMAC_MODE, mac_mode);
	bp->link_up = 1;
	return 0;
}

static int
bnx2_fw_sync(struct bnx2 *bp, u32 msg_data, int ack, int silent)
{
	int i;
	u32 val;

	bp->fw_wr_seq++;
	msg_data |= bp->fw_wr_seq;

	bnx2_shmem_wr(bp, BNX2_DRV_MB, msg_data);

	if (!ack)
		return 0;

	/* wait for an acknowledgement. */
	for (i = 0; i < (BNX2_FW_ACK_TIME_OUT_MS / 10); i++) {
		msleep(10);

		val = bnx2_shmem_rd(bp, BNX2_FW_MB);

		if ((val & BNX2_FW_MSG_ACK) == (msg_data & BNX2_DRV_MSG_SEQ))
			break;
	}
	if ((msg_data & BNX2_DRV_MSG_DATA) == BNX2_DRV_MSG_DATA_WAIT0)
		return 0;

	/* If we timed out, inform the firmware that this is the case. */
	if ((val & BNX2_FW_MSG_ACK) != (msg_data & BNX2_DRV_MSG_SEQ)) {
		if (!silent)
			printk(KERN_ERR PFX "fw sync timeout, reset code = "
					    "%x\n", msg_data);

		msg_data &= ~BNX2_DRV_MSG_CODE;
		msg_data |= BNX2_DRV_MSG_CODE_FW_TIMEOUT;

		bnx2_shmem_wr(bp, BNX2_DRV_MB, msg_data);

		return -EBUSY;
	}

	if ((val & BNX2_FW_MSG_STATUS_MASK) != BNX2_FW_MSG_STATUS_OK)
		return -EIO;

	return 0;
}

static int
bnx2_init_5709_context(struct bnx2 *bp)
{
	int i, ret = 0;
	u32 val;

	val = BNX2_CTX_COMMAND_ENABLED | BNX2_CTX_COMMAND_MEM_INIT | (1 << 12);
	val |= (BCM_PAGE_BITS - 8) << 16;
	REG_WR(bp, BNX2_CTX_COMMAND, val);
	for (i = 0; i < 10; i++) {
		val = REG_RD(bp, BNX2_CTX_COMMAND);
		if (!(val & BNX2_CTX_COMMAND_MEM_INIT))
			break;
		udelay(2);
	}
	if (val & BNX2_CTX_COMMAND_MEM_INIT)
		return -EBUSY;

	for (i = 0; i < bp->ctx_pages; i++) {
		int j;

		if (bp->ctx_blk[i])
			memset(bp->ctx_blk[i], 0, BCM_PAGE_SIZE);
		else
			return -ENOMEM;

		REG_WR(bp, BNX2_CTX_HOST_PAGE_TBL_DATA0,
		       (bp->ctx_blk_mapping[i] & 0xffffffff) |
		       BNX2_CTX_HOST_PAGE_TBL_DATA0_VALID);
		REG_WR(bp, BNX2_CTX_HOST_PAGE_TBL_DATA1,
		       (u64) bp->ctx_blk_mapping[i] >> 32);
		REG_WR(bp, BNX2_CTX_HOST_PAGE_TBL_CTRL, i |
		       BNX2_CTX_HOST_PAGE_TBL_CTRL_WRITE_REQ);
		for (j = 0; j < 10; j++) {

			val = REG_RD(bp, BNX2_CTX_HOST_PAGE_TBL_CTRL);
			if (!(val & BNX2_CTX_HOST_PAGE_TBL_CTRL_WRITE_REQ))
				break;
			udelay(5);
		}
		if (val & BNX2_CTX_HOST_PAGE_TBL_CTRL_WRITE_REQ) {
			ret = -EBUSY;
			break;
		}
	}
	return ret;
}

static void
bnx2_init_context(struct bnx2 *bp)
{
	u32 vcid;

	vcid = 96;
	while (vcid) {
		u32 vcid_addr, pcid_addr, offset;
		int i;

		vcid--;

		if (CHIP_ID(bp) == CHIP_ID_5706_A0) {
			u32 new_vcid;

			vcid_addr = GET_PCID_ADDR(vcid);
			if (vcid & 0x8) {
				new_vcid = 0x60 + (vcid & 0xf0) + (vcid & 0x7);
			}
			else {
				new_vcid = vcid;
			}
			pcid_addr = GET_PCID_ADDR(new_vcid);
		}
		else {
	    		vcid_addr = GET_CID_ADDR(vcid);
			pcid_addr = vcid_addr;
		}

		for (i = 0; i < (CTX_SIZE / PHY_CTX_SIZE); i++) {
			vcid_addr += (i << PHY_CTX_SHIFT);
			pcid_addr += (i << PHY_CTX_SHIFT);

			REG_WR(bp, BNX2_CTX_VIRT_ADDR, vcid_addr);
			REG_WR(bp, BNX2_CTX_PAGE_TBL, pcid_addr);

			/* Zero out the context. */
			for (offset = 0; offset < PHY_CTX_SIZE; offset += 4)
				bnx2_ctx_wr(bp, vcid_addr, offset, 0);
		}
	}
}

static int
bnx2_alloc_bad_rbuf(struct bnx2 *bp)
{
	u16 *good_mbuf;
	u32 good_mbuf_cnt;
	u32 val;

	good_mbuf = kmalloc(512 * sizeof(u16), GFP_KERNEL);
	if (good_mbuf == NULL) {
		printk(KERN_ERR PFX "Failed to allocate memory in "
				    "bnx2_alloc_bad_rbuf\n");
		return -ENOMEM;
	}

	REG_WR(bp, BNX2_MISC_ENABLE_SET_BITS,
		BNX2_MISC_ENABLE_SET_BITS_RX_MBUF_ENABLE);

	good_mbuf_cnt = 0;

	/* Allocate a bunch of mbufs and save the good ones in an array. */
	val = bnx2_reg_rd_ind(bp, BNX2_RBUF_STATUS1);
	while (val & BNX2_RBUF_STATUS1_FREE_COUNT) {
		bnx2_reg_wr_ind(bp, BNX2_RBUF_COMMAND,
				BNX2_RBUF_COMMAND_ALLOC_REQ);

		val = bnx2_reg_rd_ind(bp, BNX2_RBUF_FW_BUF_ALLOC);

		val &= BNX2_RBUF_FW_BUF_ALLOC_VALUE;

		/* The addresses with Bit 9 set are bad memory blocks. */
		if (!(val & (1 << 9))) {
			good_mbuf[good_mbuf_cnt] = (u16) val;
			good_mbuf_cnt++;
		}

		val = bnx2_reg_rd_ind(bp, BNX2_RBUF_STATUS1);
	}

	/* Free the good ones back to the mbuf pool thus discarding
	 * all the bad ones. */
	while (good_mbuf_cnt) {
		good_mbuf_cnt--;

		val = good_mbuf[good_mbuf_cnt];
		val = (val << 9) | val | 1;

		bnx2_reg_wr_ind(bp, BNX2_RBUF_FW_BUF_FREE, val);
	}
	kfree(good_mbuf);
	return 0;
}

static void
bnx2_set_mac_addr(struct bnx2 *bp, u8 *mac_addr, u32 pos)
{
	u32 val;

	val = (mac_addr[0] << 8) | mac_addr[1];

	REG_WR(bp, BNX2_EMAC_MAC_MATCH0 + (pos * 8), val);

	val = (mac_addr[2] << 24) | (mac_addr[3] << 16) |
		(mac_addr[4] << 8) | mac_addr[5];

	REG_WR(bp, BNX2_EMAC_MAC_MATCH1 + (pos * 8), val);
}

static inline int
bnx2_alloc_rx_page(struct bnx2 *bp, struct bnx2_rx_ring_info *rxr, u16 index)
{
	dma_addr_t mapping;
	struct sw_pg *rx_pg = &rxr->rx_pg_ring[index];
	struct rx_bd *rxbd =
		&rxr->rx_pg_desc_ring[RX_RING(index)][RX_IDX(index)];
	struct page *page = alloc_page(GFP_ATOMIC);

	if (!page)
		return -ENOMEM;
	mapping = pci_map_page(bp->pdev, page, 0, PAGE_SIZE,
			       PCI_DMA_FROMDEVICE);
	if (pci_dma_mapping_error(bp->pdev, mapping)) {
		__free_page(page);
		return -EIO;
	}

	rx_pg->page = page;
	pci_unmap_addr_set(rx_pg, mapping, mapping);
	rxbd->rx_bd_haddr_hi = (u64) mapping >> 32;
	rxbd->rx_bd_haddr_lo = (u64) mapping & 0xffffffff;
	return 0;
}

static void
bnx2_free_rx_page(struct bnx2 *bp, struct bnx2_rx_ring_info *rxr, u16 index)
{
	struct sw_pg *rx_pg = &rxr->rx_pg_ring[index];
	struct page *page = rx_pg->page;

	if (!page)
		return;

	pci_unmap_page(bp->pdev, pci_unmap_addr(rx_pg, mapping), PAGE_SIZE,
		       PCI_DMA_FROMDEVICE);

	__free_page(page);
	rx_pg->page = NULL;
}

static inline int
bnx2_alloc_rx_skb(struct bnx2 *bp, struct bnx2_rx_ring_info *rxr, u16 index)
{
	struct sk_buff *skb;
	struct sw_bd *rx_buf = &rxr->rx_buf_ring[index];
	dma_addr_t mapping;
	struct rx_bd *rxbd = &rxr->rx_desc_ring[RX_RING(index)][RX_IDX(index)];
	unsigned long align;

	skb = netdev_alloc_skb(bp->dev, bp->rx_buf_size);
	if (skb == NULL) {
		return -ENOMEM;
	}

	if (unlikely((align = (unsigned long) skb->data & (BNX2_RX_ALIGN - 1))))
		skb_reserve(skb, BNX2_RX_ALIGN - align);

	mapping = pci_map_single(bp->pdev, skb->data, bp->rx_buf_use_size,
		PCI_DMA_FROMDEVICE);
	if (pci_dma_mapping_error(bp->pdev, mapping)) {
		dev_kfree_skb(skb);
		return -EIO;
	}

	rx_buf->skb = skb;
	pci_unmap_addr_set(rx_buf, mapping, mapping);

	rxbd->rx_bd_haddr_hi = (u64) mapping >> 32;
	rxbd->rx_bd_haddr_lo = (u64) mapping & 0xffffffff;

	rxr->rx_prod_bseq += bp->rx_buf_use_size;

	return 0;
}

static int
bnx2_phy_event_is_set(struct bnx2 *bp, struct bnx2_napi *bnapi, u32 event)
{
	struct status_block *sblk = bnapi->status_blk.msi;
	u32 new_link_state, old_link_state;
	int is_set = 1;

	new_link_state = sblk->status_attn_bits & event;
	old_link_state = sblk->status_attn_bits_ack & event;
	if (new_link_state != old_link_state) {
		if (new_link_state)
			REG_WR(bp, BNX2_PCICFG_STATUS_BIT_SET_CMD, event);
		else
			REG_WR(bp, BNX2_PCICFG_STATUS_BIT_CLEAR_CMD, event);
	} else
		is_set = 0;

	return is_set;
}

static void
bnx2_phy_int(struct bnx2 *bp, struct bnx2_napi *bnapi)
{
	spin_lock(&bp->phy_lock);

	if (bnx2_phy_event_is_set(bp, bnapi, STATUS_ATTN_BITS_LINK_STATE))
		bnx2_set_link(bp);
	if (bnx2_phy_event_is_set(bp, bnapi, STATUS_ATTN_BITS_TIMER_ABORT))
		bnx2_set_remote_link(bp);

	spin_unlock(&bp->phy_lock);

}

static inline u16
bnx2_get_hw_tx_cons(struct bnx2_napi *bnapi)
{
	u16 cons;

	/* Tell compiler that status block fields can change. */
	barrier();
	cons = *bnapi->hw_tx_cons_ptr;
	barrier();
	if (unlikely((cons & MAX_TX_DESC_CNT) == MAX_TX_DESC_CNT))
		cons++;
	return cons;
}

static int
bnx2_tx_int(struct bnx2 *bp, struct bnx2_napi *bnapi, int budget)
{
	struct bnx2_tx_ring_info *txr = &bnapi->tx_ring;
	u16 hw_cons, sw_cons, sw_ring_cons;
	int tx_pkt = 0, index;
	struct netdev_queue *txq;

	index = (bnapi - bp->bnx2_napi);
	txq = netdev_get_tx_queue(bp->dev, index);

	hw_cons = bnx2_get_hw_tx_cons(bnapi);
	sw_cons = txr->tx_cons;

	while (sw_cons != hw_cons) {
		struct sw_tx_bd *tx_buf;
		struct sk_buff *skb;
		int i, last;

		sw_ring_cons = TX_RING_IDX(sw_cons);

		tx_buf = &txr->tx_buf_ring[sw_ring_cons];
		skb = tx_buf->skb;

		/* prefetch skb_end_pointer() to speedup skb_shinfo(skb) */
		prefetch(&skb->end);

		/* partial BD completions possible with TSO packets */
		if (tx_buf->is_gso) {
			u16 last_idx, last_ring_idx;

			last_idx = sw_cons + tx_buf->nr_frags + 1;
			last_ring_idx = sw_ring_cons + tx_buf->nr_frags + 1;
			if (unlikely(last_ring_idx >= MAX_TX_DESC_CNT)) {
				last_idx++;
			}
			if (((s16) ((s16) last_idx - (s16) hw_cons)) > 0) {
				break;
			}
		}

		skb_dma_unmap(&bp->pdev->dev, skb, DMA_TO_DEVICE);

		tx_buf->skb = NULL;
		last = tx_buf->nr_frags;

		for (i = 0; i < last; i++) {
			sw_cons = NEXT_TX_BD(sw_cons);
		}

		sw_cons = NEXT_TX_BD(sw_cons);

		dev_kfree_skb(skb);
		tx_pkt++;
		if (tx_pkt == budget)
			break;

		if (hw_cons == sw_cons)
			hw_cons = bnx2_get_hw_tx_cons(bnapi);
	}

	txr->hw_tx_cons = hw_cons;
	txr->tx_cons = sw_cons;

	/* Need to make the tx_cons update visible to bnx2_start_xmit()
	 * before checking for netif_tx_queue_stopped().  Without the
	 * memory barrier, there is a small possibility that bnx2_start_xmit()
	 * will miss it and cause the queue to be stopped forever.
	 */
	smp_mb();

	if (unlikely(netif_tx_queue_stopped(txq)) &&
		     (bnx2_tx_avail(bp, txr) > bp->tx_wake_thresh)) {
		__netif_tx_lock(txq, smp_processor_id());
		if ((netif_tx_queue_stopped(txq)) &&
		    (bnx2_tx_avail(bp, txr) > bp->tx_wake_thresh))
			netif_tx_wake_queue(txq);
		__netif_tx_unlock(txq);
	}

	return tx_pkt;
}

static void
bnx2_reuse_rx_skb_pages(struct bnx2 *bp, struct bnx2_rx_ring_info *rxr,
			struct sk_buff *skb, int count)
{
	struct sw_pg *cons_rx_pg, *prod_rx_pg;
	struct rx_bd *cons_bd, *prod_bd;
	int i;
	u16 hw_prod, prod;
	u16 cons = rxr->rx_pg_cons;

	cons_rx_pg = &rxr->rx_pg_ring[cons];

	/* The caller was unable to allocate a new page to replace the
	 * last one in the frags array, so we need to recycle that page
	 * and then free the skb.
	 */
	if (skb) {
		struct page *page;
		struct skb_shared_info *shinfo;

		shinfo = skb_shinfo(skb);
		shinfo->nr_frags--;
		page = shinfo->frags[shinfo->nr_frags].page;
		shinfo->frags[shinfo->nr_frags].page = NULL;

		cons_rx_pg->page = page;
		dev_kfree_skb(skb);
	}

	hw_prod = rxr->rx_pg_prod;

	for (i = 0; i < count; i++) {
		prod = RX_PG_RING_IDX(hw_prod);

		prod_rx_pg = &rxr->rx_pg_ring[prod];
		cons_rx_pg = &rxr->rx_pg_ring[cons];
		cons_bd = &rxr->rx_pg_desc_ring[RX_RING(cons)][RX_IDX(cons)];
		prod_bd = &rxr->rx_pg_desc_ring[RX_RING(prod)][RX_IDX(prod)];

		if (prod != cons) {
			prod_rx_pg->page = cons_rx_pg->page;
			cons_rx_pg->page = NULL;
			pci_unmap_addr_set(prod_rx_pg, mapping,
				pci_unmap_addr(cons_rx_pg, mapping));

			prod_bd->rx_bd_haddr_hi = cons_bd->rx_bd_haddr_hi;
			prod_bd->rx_bd_haddr_lo = cons_bd->rx_bd_haddr_lo;

		}
		cons = RX_PG_RING_IDX(NEXT_RX_BD(cons));
		hw_prod = NEXT_RX_BD(hw_prod);
	}
	rxr->rx_pg_prod = hw_prod;
	rxr->rx_pg_cons = cons;
}

static inline void
bnx2_reuse_rx_skb(struct bnx2 *bp, struct bnx2_rx_ring_info *rxr,
		  struct sk_buff *skb, u16 cons, u16 prod)
{
	struct sw_bd *cons_rx_buf, *prod_rx_buf;
	struct rx_bd *cons_bd, *prod_bd;

	cons_rx_buf = &rxr->rx_buf_ring[cons];
	prod_rx_buf = &rxr->rx_buf_ring[prod];

	pci_dma_sync_single_for_device(bp->pdev,
		pci_unmap_addr(cons_rx_buf, mapping),
		BNX2_RX_OFFSET + BNX2_RX_COPY_THRESH, PCI_DMA_FROMDEVICE);

	rxr->rx_prod_bseq += bp->rx_buf_use_size;

	prod_rx_buf->skb = skb;

	if (cons == prod)
		return;

	pci_unmap_addr_set(prod_rx_buf, mapping,
			pci_unmap_addr(cons_rx_buf, mapping));

	cons_bd = &rxr->rx_desc_ring[RX_RING(cons)][RX_IDX(cons)];
	prod_bd = &rxr->rx_desc_ring[RX_RING(prod)][RX_IDX(prod)];
	prod_bd->rx_bd_haddr_hi = cons_bd->rx_bd_haddr_hi;
	prod_bd->rx_bd_haddr_lo = cons_bd->rx_bd_haddr_lo;
}

static int
bnx2_rx_skb(struct bnx2 *bp, struct bnx2_rx_ring_info *rxr, struct sk_buff *skb,
	    unsigned int len, unsigned int hdr_len, dma_addr_t dma_addr,
	    u32 ring_idx)
{
	int err;
	u16 prod = ring_idx & 0xffff;

	err = bnx2_alloc_rx_skb(bp, rxr, prod);
	if (unlikely(err)) {
		bnx2_reuse_rx_skb(bp, rxr, skb, (u16) (ring_idx >> 16), prod);
		if (hdr_len) {
			unsigned int raw_len = len + 4;
			int pages = PAGE_ALIGN(raw_len - hdr_len) >> PAGE_SHIFT;

			bnx2_reuse_rx_skb_pages(bp, rxr, NULL, pages);
		}
		return err;
	}

	skb_reserve(skb, BNX2_RX_OFFSET);
	pci_unmap_single(bp->pdev, dma_addr, bp->rx_buf_use_size,
			 PCI_DMA_FROMDEVICE);

	if (hdr_len == 0) {
		skb_put(skb, len);
		return 0;
	} else {
		unsigned int i, frag_len, frag_size, pages;
		struct sw_pg *rx_pg;
		u16 pg_cons = rxr->rx_pg_cons;
		u16 pg_prod = rxr->rx_pg_prod;

		frag_size = len + 4 - hdr_len;
		pages = PAGE_ALIGN(frag_size) >> PAGE_SHIFT;
		skb_put(skb, hdr_len);

		for (i = 0; i < pages; i++) {
			dma_addr_t mapping_old;

			frag_len = min(frag_size, (unsigned int) PAGE_SIZE);
			if (unlikely(frag_len <= 4)) {
				unsigned int tail = 4 - frag_len;

				rxr->rx_pg_cons = pg_cons;
				rxr->rx_pg_prod = pg_prod;
				bnx2_reuse_rx_skb_pages(bp, rxr, NULL,
							pages - i);
				skb->len -= tail;
				if (i == 0) {
					skb->tail -= tail;
				} else {
					skb_frag_t *frag =
						&skb_shinfo(skb)->frags[i - 1];
					frag->size -= tail;
					skb->data_len -= tail;
					skb->truesize -= tail;
				}
				return 0;
			}
			rx_pg = &rxr->rx_pg_ring[pg_cons];

			/* Don't unmap yet.  If we're unable to allocate a new
			 * page, we need to recycle the page and the DMA addr.
			 */
			mapping_old = pci_unmap_addr(rx_pg, mapping);
			if (i == pages - 1)
				frag_len -= 4;

			skb_fill_page_desc(skb, i, rx_pg->page, 0, frag_len);
			rx_pg->page = NULL;

			err = bnx2_alloc_rx_page(bp, rxr,
						 RX_PG_RING_IDX(pg_prod));
			if (unlikely(err)) {
				rxr->rx_pg_cons = pg_cons;
				rxr->rx_pg_prod = pg_prod;
				bnx2_reuse_rx_skb_pages(bp, rxr, skb,
							pages - i);
				return err;
			}

			pci_unmap_page(bp->pdev, mapping_old,
				       PAGE_SIZE, PCI_DMA_FROMDEVICE);

			frag_size -= frag_len;
			skb->data_len += frag_len;
			skb->truesize += frag_len;
			skb->len += frag_len;

			pg_prod = NEXT_RX_BD(pg_prod);
			pg_cons = RX_PG_RING_IDX(NEXT_RX_BD(pg_cons));
		}
		rxr->rx_pg_prod = pg_prod;
		rxr->rx_pg_cons = pg_cons;
	}
	return 0;
}

static inline u16
bnx2_get_hw_rx_cons(struct bnx2_napi *bnapi)
{
	u16 cons;

	/* Tell compiler that status block fields can change. */
	barrier();
	cons = *bnapi->hw_rx_cons_ptr;
	barrier();
	if (unlikely((cons & MAX_RX_DESC_CNT) == MAX_RX_DESC_CNT))
		cons++;
	return cons;
}

static int
bnx2_rx_int(struct bnx2 *bp, struct bnx2_napi *bnapi, int budget)
{
	struct bnx2_rx_ring_info *rxr = &bnapi->rx_ring;
	u16 hw_cons, sw_cons, sw_ring_cons, sw_prod, sw_ring_prod;
	struct l2_fhdr *rx_hdr;
	int rx_pkt = 0, pg_ring_used = 0;

	hw_cons = bnx2_get_hw_rx_cons(bnapi);
	sw_cons = rxr->rx_cons;
	sw_prod = rxr->rx_prod;

	/* Memory barrier necessary as speculative reads of the rx
	 * buffer can be ahead of the index in the status block
	 */
	rmb();
	while (sw_cons != hw_cons) {
		unsigned int len, hdr_len;
		u32 status;
		struct sw_bd *rx_buf;
		struct sk_buff *skb;
		dma_addr_t dma_addr;
		u16 vtag = 0;
		int hw_vlan __maybe_unused = 0;

		sw_ring_cons = RX_RING_IDX(sw_cons);
		sw_ring_prod = RX_RING_IDX(sw_prod);

		rx_buf = &rxr->rx_buf_ring[sw_ring_cons];
		skb = rx_buf->skb;

		rx_buf->skb = NULL;

		dma_addr = pci_unmap_addr(rx_buf, mapping);

		pci_dma_sync_single_for_cpu(bp->pdev, dma_addr,
			BNX2_RX_OFFSET + BNX2_RX_COPY_THRESH,
			PCI_DMA_FROMDEVICE);

		rx_hdr = (struct l2_fhdr *) skb->data;
		len = rx_hdr->l2_fhdr_pkt_len;
		status = rx_hdr->l2_fhdr_status;

		hdr_len = 0;
		if (status & L2_FHDR_STATUS_SPLIT) {
			hdr_len = rx_hdr->l2_fhdr_ip_xsum;
			pg_ring_used = 1;
		} else if (len > bp->rx_jumbo_thresh) {
			hdr_len = bp->rx_jumbo_thresh;
			pg_ring_used = 1;
		}

		if (unlikely(status & (L2_FHDR_ERRORS_BAD_CRC |
				       L2_FHDR_ERRORS_PHY_DECODE |
				       L2_FHDR_ERRORS_ALIGNMENT |
				       L2_FHDR_ERRORS_TOO_SHORT |
				       L2_FHDR_ERRORS_GIANT_FRAME))) {

			bnx2_reuse_rx_skb(bp, rxr, skb, sw_ring_cons,
					  sw_ring_prod);
			if (pg_ring_used) {
				int pages;

				pages = PAGE_ALIGN(len - hdr_len) >> PAGE_SHIFT;

				bnx2_reuse_rx_skb_pages(bp, rxr, NULL, pages);
			}
			goto next_rx;
		}

		len -= 4;

		if (len <= bp->rx_copy_thresh) {
			struct sk_buff *new_skb;

			new_skb = netdev_alloc_skb(bp->dev, len + 6);
			if (new_skb == NULL) {
				bnx2_reuse_rx_skb(bp, rxr, skb, sw_ring_cons,
						  sw_ring_prod);
				goto next_rx;
			}

			/* aligned copy */
			skb_copy_from_linear_data_offset(skb,
							 BNX2_RX_OFFSET - 6,
				      new_skb->data, len + 6);
			skb_reserve(new_skb, 6);
			skb_put(new_skb, len);

			bnx2_reuse_rx_skb(bp, rxr, skb,
				sw_ring_cons, sw_ring_prod);

			skb = new_skb;
		} else if (unlikely(bnx2_rx_skb(bp, rxr, skb, len, hdr_len,
			   dma_addr, (sw_ring_cons << 16) | sw_ring_prod)))
			goto next_rx;

		if ((status & L2_FHDR_STATUS_L2_VLAN_TAG) &&
		    !(bp->rx_mode & BNX2_EMAC_RX_MODE_KEEP_VLAN_TAG)) {
			vtag = rx_hdr->l2_fhdr_vlan_tag;
#ifdef BCM_VLAN
			if (bp->vlgrp)
				hw_vlan = 1;
			else
#endif
			{
				struct vlan_ethhdr *ve = (struct vlan_ethhdr *)
					__skb_push(skb, 4);

				memmove(ve, skb->data + 4, ETH_ALEN * 2);
				ve->h_vlan_proto = htons(ETH_P_8021Q);
				ve->h_vlan_TCI = htons(vtag);
				len += 4;
			}
		}

		skb->protocol = eth_type_trans(skb, bp->dev);

		if ((len > (bp->dev->mtu + ETH_HLEN)) &&
			(ntohs(skb->protocol) != 0x8100)) {

			dev_kfree_skb(skb);
			goto next_rx;

		}

		skb->ip_summed = CHECKSUM_NONE;
		if (bp->rx_csum &&
			(status & (L2_FHDR_STATUS_TCP_SEGMENT |
			L2_FHDR_STATUS_UDP_DATAGRAM))) {

			if (likely((status & (L2_FHDR_ERRORS_TCP_XSUM |
					      L2_FHDR_ERRORS_UDP_XSUM)) == 0))
				skb->ip_summed = CHECKSUM_UNNECESSARY;
		}

		skb_record_rx_queue(skb, bnapi - &bp->bnx2_napi[0]);

#ifdef BCM_VLAN
		if (hw_vlan)
			vlan_hwaccel_receive_skb(skb, bp->vlgrp, vtag);
		else
#endif
			netif_receive_skb(skb);

		rx_pkt++;

next_rx:
		sw_cons = NEXT_RX_BD(sw_cons);
		sw_prod = NEXT_RX_BD(sw_prod);

		if ((rx_pkt == budget))
			break;

		/* Refresh hw_cons to see if there is new work */
		if (sw_cons == hw_cons) {
			hw_cons = bnx2_get_hw_rx_cons(bnapi);
			rmb();
		}
	}
	rxr->rx_cons = sw_cons;
	rxr->rx_prod = sw_prod;

	if (pg_ring_used)
		REG_WR16(bp, rxr->rx_pg_bidx_addr, rxr->rx_pg_prod);

	REG_WR16(bp, rxr->rx_bidx_addr, sw_prod);

	REG_WR(bp, rxr->rx_bseq_addr, rxr->rx_prod_bseq);

	mmiowb();

	return rx_pkt;

}

/* MSI ISR - The only difference between this and the INTx ISR
 * is that the MSI interrupt is always serviced.
 */
static irqreturn_t
bnx2_msi(int irq, void *dev_instance)
{
	struct bnx2_napi *bnapi = dev_instance;
	struct bnx2 *bp = bnapi->bp;

	prefetch(bnapi->status_blk.msi);
	REG_WR(bp, BNX2_PCICFG_INT_ACK_CMD,
		BNX2_PCICFG_INT_ACK_CMD_USE_INT_HC_PARAM |
		BNX2_PCICFG_INT_ACK_CMD_MASK_INT);

	/* Return here if interrupt is disabled. */
	if (unlikely(atomic_read(&bp->intr_sem) != 0))
		return IRQ_HANDLED;

	napi_schedule(&bnapi->napi);

	return IRQ_HANDLED;
}

static irqreturn_t
bnx2_msi_1shot(int irq, void *dev_instance)
{
	struct bnx2_napi *bnapi = dev_instance;
	struct bnx2 *bp = bnapi->bp;

	prefetch(bnapi->status_blk.msi);

	/* Return here if interrupt is disabled. */
	if (unlikely(atomic_read(&bp->intr_sem) != 0))
		return IRQ_HANDLED;

	napi_schedule(&bnapi->napi);

	return IRQ_HANDLED;
}

static irqreturn_t
bnx2_interrupt(int irq, void *dev_instance)
{
	struct bnx2_napi *bnapi = dev_instance;
	struct bnx2 *bp = bnapi->bp;
	struct status_block *sblk = bnapi->status_blk.msi;

	/* When using INTx, it is possible for the interrupt to arrive
	 * at the CPU before the status block posted prior to the
	 * interrupt. Reading a register will flush the status block.
	 * When using MSI, the MSI message will always complete after
	 * the status block write.
	 */
	if ((sblk->status_idx == bnapi->last_status_idx) &&
	    (REG_RD(bp, BNX2_PCICFG_MISC_STATUS) &
	     BNX2_PCICFG_MISC_STATUS_INTA_VALUE))
		return IRQ_NONE;

	REG_WR(bp, BNX2_PCICFG_INT_ACK_CMD,
		BNX2_PCICFG_INT_ACK_CMD_USE_INT_HC_PARAM |
		BNX2_PCICFG_INT_ACK_CMD_MASK_INT);

	/* Read back to deassert IRQ immediately to avoid too many
	 * spurious interrupts.
	 */
	REG_RD(bp, BNX2_PCICFG_INT_ACK_CMD);

	/* Return here if interrupt is shared and is disabled. */
	if (unlikely(atomic_read(&bp->intr_sem) != 0))
		return IRQ_HANDLED;

	if (napi_schedule_prep(&bnapi->napi)) {
		bnapi->last_status_idx = sblk->status_idx;
		__napi_schedule(&bnapi->napi);
	}

	return IRQ_HANDLED;
}

static inline int
bnx2_has_fast_work(struct bnx2_napi *bnapi)
{
	struct bnx2_tx_ring_info *txr = &bnapi->tx_ring;
	struct bnx2_rx_ring_info *rxr = &bnapi->rx_ring;

	if ((bnx2_get_hw_rx_cons(bnapi) != rxr->rx_cons) ||
	    (bnx2_get_hw_tx_cons(bnapi) != txr->hw_tx_cons))
		return 1;
	return 0;
}

#define STATUS_ATTN_EVENTS	(STATUS_ATTN_BITS_LINK_STATE | \
				 STATUS_ATTN_BITS_TIMER_ABORT)

static inline int
bnx2_has_work(struct bnx2_napi *bnapi)
{
	struct status_block *sblk = bnapi->status_blk.msi;

	if (bnx2_has_fast_work(bnapi))
		return 1;

#ifdef BCM_CNIC
	if (bnapi->cnic_present && (bnapi->cnic_tag != sblk->status_idx))
		return 1;
#endif

	if ((sblk->status_attn_bits & STATUS_ATTN_EVENTS) !=
	    (sblk->status_attn_bits_ack & STATUS_ATTN_EVENTS))
		return 1;

	return 0;
}

static void
bnx2_chk_missed_msi(struct bnx2 *bp)
{
	struct bnx2_napi *bnapi = &bp->bnx2_napi[0];
	u32 msi_ctrl;

	if (bnx2_has_work(bnapi)) {
		msi_ctrl = REG_RD(bp, BNX2_PCICFG_MSI_CONTROL);
		if (!(msi_ctrl & BNX2_PCICFG_MSI_CONTROL_ENABLE))
			return;

		if (bnapi->last_status_idx == bp->idle_chk_status_idx) {
			REG_WR(bp, BNX2_PCICFG_MSI_CONTROL, msi_ctrl &
			       ~BNX2_PCICFG_MSI_CONTROL_ENABLE);
			REG_WR(bp, BNX2_PCICFG_MSI_CONTROL, msi_ctrl);
			bnx2_msi(bp->irq_tbl[0].vector, bnapi);
		}
	}

	bp->idle_chk_status_idx = bnapi->last_status_idx;
}

#ifdef BCM_CNIC
static void bnx2_poll_cnic(struct bnx2 *bp, struct bnx2_napi *bnapi)
{
	struct cnic_ops *c_ops;

	if (!bnapi->cnic_present)
		return;

	rcu_read_lock();
	c_ops = rcu_dereference(bp->cnic_ops);
	if (c_ops)
		bnapi->cnic_tag = c_ops->cnic_handler(bp->cnic_data,
						      bnapi->status_blk.msi);
	rcu_read_unlock();
}
#endif

static void bnx2_poll_link(struct bnx2 *bp, struct bnx2_napi *bnapi)
{
	struct status_block *sblk = bnapi->status_blk.msi;
	u32 status_attn_bits = sblk->status_attn_bits;
	u32 status_attn_bits_ack = sblk->status_attn_bits_ack;

	if ((status_attn_bits & STATUS_ATTN_EVENTS) !=
	    (status_attn_bits_ack & STATUS_ATTN_EVENTS)) {

		bnx2_phy_int(bp, bnapi);

		/* This is needed to take care of transient status
		 * during link changes.
		 */
		REG_WR(bp, BNX2_HC_COMMAND,
		       bp->hc_cmd | BNX2_HC_COMMAND_COAL_NOW_WO_INT);
		REG_RD(bp, BNX2_HC_COMMAND);
	}
}

static int bnx2_poll_work(struct bnx2 *bp, struct bnx2_napi *bnapi,
			  int work_done, int budget)
{
	struct bnx2_tx_ring_info *txr = &bnapi->tx_ring;
	struct bnx2_rx_ring_info *rxr = &bnapi->rx_ring;

	if (bnx2_get_hw_tx_cons(bnapi) != txr->hw_tx_cons)
		bnx2_tx_int(bp, bnapi, 0);

	if (bnx2_get_hw_rx_cons(bnapi) != rxr->rx_cons)
		work_done += bnx2_rx_int(bp, bnapi, budget - work_done);

	return work_done;
}

static int bnx2_poll_msix(struct napi_struct *napi, int budget)
{
	struct bnx2_napi *bnapi = container_of(napi, struct bnx2_napi, napi);
	struct bnx2 *bp = bnapi->bp;
	int work_done = 0;
	struct status_block_msix *sblk = bnapi->status_blk.msix;

	while (1) {
		work_done = bnx2_poll_work(bp, bnapi, work_done, budget);
		if (unlikely(work_done >= budget))
			break;

		bnapi->last_status_idx = sblk->status_idx;
		/* status idx must be read before checking for more work. */
		rmb();
		if (likely(!bnx2_has_fast_work(bnapi))) {

			napi_complete(napi);
			REG_WR(bp, BNX2_PCICFG_INT_ACK_CMD, bnapi->int_num |
			       BNX2_PCICFG_INT_ACK_CMD_INDEX_VALID |
			       bnapi->last_status_idx);
			break;
		}
	}
	return work_done;
}

static int bnx2_poll(struct napi_struct *napi, int budget)
{
	struct bnx2_napi *bnapi = container_of(napi, struct bnx2_napi, napi);
	struct bnx2 *bp = bnapi->bp;
	int work_done = 0;
	struct status_block *sblk = bnapi->status_blk.msi;

	while (1) {
		bnx2_poll_link(bp, bnapi);

		work_done = bnx2_poll_work(bp, bnapi, work_done, budget);

#ifdef BCM_CNIC
		bnx2_poll_cnic(bp, bnapi);
#endif

		/* bnapi->last_status_idx is used below to tell the hw how
		 * much work has been processed, so we must read it before
		 * checking for more work.
		 */
		bnapi->last_status_idx = sblk->status_idx;

		if (unlikely(work_done >= budget))
			break;

		rmb();
		if (likely(!bnx2_has_work(bnapi))) {
			napi_complete(napi);
			if (likely(bp->flags & BNX2_FLAG_USING_MSI_OR_MSIX)) {
				REG_WR(bp, BNX2_PCICFG_INT_ACK_CMD,
				       BNX2_PCICFG_INT_ACK_CMD_INDEX_VALID |
				       bnapi->last_status_idx);
				break;
			}
			REG_WR(bp, BNX2_PCICFG_INT_ACK_CMD,
			       BNX2_PCICFG_INT_ACK_CMD_INDEX_VALID |
			       BNX2_PCICFG_INT_ACK_CMD_MASK_INT |
			       bnapi->last_status_idx);

			REG_WR(bp, BNX2_PCICFG_INT_ACK_CMD,
			       BNX2_PCICFG_INT_ACK_CMD_INDEX_VALID |
			       bnapi->last_status_idx);
			break;
		}
	}

	return work_done;
}

/* Called with rtnl_lock from vlan functions and also netif_tx_lock
 * from set_multicast.
 */
static void
bnx2_set_rx_mode(struct net_device *dev)
{
	struct bnx2 *bp = netdev_priv(dev);
	u32 rx_mode, sort_mode;
	struct netdev_hw_addr *ha;
	int i;

	if (!netif_running(dev))
		return;

	spin_lock_bh(&bp->phy_lock);

	rx_mode = bp->rx_mode & ~(BNX2_EMAC_RX_MODE_PROMISCUOUS |
				  BNX2_EMAC_RX_MODE_KEEP_VLAN_TAG);
	sort_mode = 1 | BNX2_RPM_SORT_USER0_BC_EN;
#ifdef BCM_VLAN
	if (!bp->vlgrp && (bp->flags & BNX2_FLAG_CAN_KEEP_VLAN))
		rx_mode |= BNX2_EMAC_RX_MODE_KEEP_VLAN_TAG;
#else
	if (bp->flags & BNX2_FLAG_CAN_KEEP_VLAN)
		rx_mode |= BNX2_EMAC_RX_MODE_KEEP_VLAN_TAG;
#endif
	if (dev->flags & IFF_PROMISC) {
		/* Promiscuous mode. */
		rx_mode |= BNX2_EMAC_RX_MODE_PROMISCUOUS;
		sort_mode |= BNX2_RPM_SORT_USER0_PROM_EN |
			     BNX2_RPM_SORT_USER0_PROM_VLAN;
	}
	else if (dev->flags & IFF_ALLMULTI) {
		for (i = 0; i < NUM_MC_HASH_REGISTERS; i++) {
			REG_WR(bp, BNX2_EMAC_MULTICAST_HASH0 + (i * 4),
			       0xffffffff);
        	}
		sort_mode |= BNX2_RPM_SORT_USER0_MC_EN;
	}
	else {
		/* Accept one or more multicast(s). */
		struct dev_mc_list *mclist;
		u32 mc_filter[NUM_MC_HASH_REGISTERS];
		u32 regidx;
		u32 bit;
		u32 crc;

		memset(mc_filter, 0, 4 * NUM_MC_HASH_REGISTERS);

		for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
		     i++, mclist = mclist->next) {

			crc = ether_crc_le(ETH_ALEN, mclist->dmi_addr);
			bit = crc & 0xff;
			regidx = (bit & 0xe0) >> 5;
			bit &= 0x1f;
			mc_filter[regidx] |= (1 << bit);
		}

		for (i = 0; i < NUM_MC_HASH_REGISTERS; i++) {
			REG_WR(bp, BNX2_EMAC_MULTICAST_HASH0 + (i * 4),
			       mc_filter[i]);
		}

		sort_mode |= BNX2_RPM_SORT_USER0_MC_HSH_EN;
	}

	if (dev->uc.count > BNX2_MAX_UNICAST_ADDRESSES) {
		rx_mode |= BNX2_EMAC_RX_MODE_PROMISCUOUS;
		sort_mode |= BNX2_RPM_SORT_USER0_PROM_EN |
			     BNX2_RPM_SORT_USER0_PROM_VLAN;
	} else if (!(dev->flags & IFF_PROMISC)) {
		/* Add all entries into to the match filter list */
		i = 0;
		list_for_each_entry(ha, &dev->uc.list, list) {
			bnx2_set_mac_addr(bp, ha->addr,
					  i + BNX2_START_UNICAST_ADDRESS_INDEX);
			sort_mode |= (1 <<
				      (i + BNX2_START_UNICAST_ADDRESS_INDEX));
			i++;
		}

	}

	if (rx_mode != bp->rx_mode) {
		bp->rx_mode = rx_mode;
		REG_WR(bp, BNX2_EMAC_RX_MODE, rx_mode);
	}

	REG_WR(bp, BNX2_RPM_SORT_USER0, 0x0);
	REG_WR(bp, BNX2_RPM_SORT_USER0, sort_mode);
	REG_WR(bp, BNX2_RPM_SORT_USER0, sort_mode | BNX2_RPM_SORT_USER0_ENA);

	spin_unlock_bh(&bp->phy_lock);
}

static int __devinit
check_fw_section(const struct firmware *fw,
		 const struct bnx2_fw_file_section *section,
		 u32 alignment, bool non_empty)
{
	u32 offset = be32_to_cpu(section->offset);
	u32 len = be32_to_cpu(section->len);

	if ((offset == 0 && len != 0) || offset >= fw->size || offset & 3)
		return -EINVAL;
	if ((non_empty && len == 0) || len > fw->size - offset ||
	    len & (alignment - 1))
		return -EINVAL;
	return 0;
}

static int __devinit
check_mips_fw_entry(const struct firmware *fw,
		    const struct bnx2_mips_fw_file_entry *entry)
{
	if (check_fw_section(fw, &entry->text, 4, true) ||
	    check_fw_section(fw, &entry->data, 4, false) ||
	    check_fw_section(fw, &entry->rodata, 4, false))
		return -EINVAL;
	return 0;
}

static int __devinit
bnx2_request_firmware(struct bnx2 *bp)
{
	const char *mips_fw_file, *rv2p_fw_file;
	const struct bnx2_mips_fw_file *mips_fw;
	const struct bnx2_rv2p_fw_file *rv2p_fw;
	int rc;

	if (CHIP_NUM(bp) == CHIP_NUM_5709) {
		mips_fw_file = FW_MIPS_FILE_09;
		if ((CHIP_ID(bp) == CHIP_ID_5709_A0) ||
		    (CHIP_ID(bp) == CHIP_ID_5709_A1))
			rv2p_fw_file = FW_RV2P_FILE_09_Ax;
		else
			rv2p_fw_file = FW_RV2P_FILE_09;
	} else {
		mips_fw_file = FW_MIPS_FILE_06;
		rv2p_fw_file = FW_RV2P_FILE_06;
	}

	rc = request_firmware(&bp->mips_firmware, mips_fw_file, &bp->pdev->dev);
	if (rc) {
		printk(KERN_ERR PFX "Can't load firmware file \"%s\"\n",
		       mips_fw_file);
		return rc;
	}

	rc = request_firmware(&bp->rv2p_firmware, rv2p_fw_file, &bp->pdev->dev);
	if (rc) {
		printk(KERN_ERR PFX "Can't load firmware file \"%s\"\n",
		       rv2p_fw_file);
		return rc;
	}
	mips_fw = (const struct bnx2_mips_fw_file *) bp->mips_firmware->data;
	rv2p_fw = (const struct bnx2_rv2p_fw_file *) bp->rv2p_firmware->data;
	if (bp->mips_firmware->size < sizeof(*mips_fw) ||
	    check_mips_fw_entry(bp->mips_firmware, &mips_fw->com) ||
	    check_mips_fw_entry(bp->mips_firmware, &mips_fw->cp) ||
	    check_mips_fw_entry(bp->mips_firmware, &mips_fw->rxp) ||
	    check_mips_fw_entry(bp->mips_firmware, &mips_fw->tpat) ||
	    check_mips_fw_entry(bp->mips_firmware, &mips_fw->txp)) {
		printk(KERN_ERR PFX "Firmware file \"%s\" is invalid\n",
		       mips_fw_file);
		return -EINVAL;
	}
	if (bp->rv2p_firmware->size < sizeof(*rv2p_fw) ||
	    check_fw_section(bp->rv2p_firmware, &rv2p_fw->proc1.rv2p, 8, true) ||
	    check_fw_section(bp->rv2p_firmware, &rv2p_fw->proc2.rv2p, 8, true)) {
		printk(KERN_ERR PFX "Firmware file \"%s\" is invalid\n",
		       rv2p_fw_file);
		return -EINVAL;
	}

	return 0;
}

static u32
rv2p_fw_fixup(u32 rv2p_proc, int idx, u32 loc, u32 rv2p_code)
{
	switch (idx) {
	case RV2P_P1_FIXUP_PAGE_SIZE_IDX:
		rv2p_code &= ~RV2P_BD_PAGE_SIZE_MSK;
		rv2p_code |= RV2P_BD_PAGE_SIZE;
		break;
	}
	return rv2p_code;
}

static int
load_rv2p_fw(struct bnx2 *bp, u32 rv2p_proc,
	     const struct bnx2_rv2p_fw_file_entry *fw_entry)
{
	u32 rv2p_code_len, file_offset;
	__be32 *rv2p_code;
	int i;
	u32 val, cmd, addr;

	rv2p_code_len = be32_to_cpu(fw_entry->rv2p.len);
	file_offset = be32_to_cpu(fw_entry->rv2p.offset);

	rv2p_code = (__be32 *)(bp->rv2p_firmware->data + file_offset);

	if (rv2p_proc == RV2P_PROC1) {
		cmd = BNX2_RV2P_PROC1_ADDR_CMD_RDWR;
		addr = BNX2_RV2P_PROC1_ADDR_CMD;
	} else {
		cmd = BNX2_RV2P_PROC2_ADDR_CMD_RDWR;
		addr = BNX2_RV2P_PROC2_ADDR_CMD;
	}

	for (i = 0; i < rv2p_code_len; i += 8) {
		REG_WR(bp, BNX2_RV2P_INSTR_HIGH, be32_to_cpu(*rv2p_code));
		rv2p_code++;
		REG_WR(bp, BNX2_RV2P_INSTR_LOW, be32_to_cpu(*rv2p_code));
		rv2p_code++;

		val = (i / 8) | cmd;
		REG_WR(bp, addr, val);
	}

	rv2p_code = (__be32 *)(bp->rv2p_firmware->data + file_offset);
	for (i = 0; i < 8; i++) {
		u32 loc, code;

		loc = be32_to_cpu(fw_entry->fixup[i]);
		if (loc && ((loc * 4) < rv2p_code_len)) {
			code = be32_to_cpu(*(rv2p_code + loc - 1));
			REG_WR(bp, BNX2_RV2P_INSTR_HIGH, code);
			code = be32_to_cpu(*(rv2p_code + loc));
			code = rv2p_fw_fixup(rv2p_proc, i, loc, code);
			REG_WR(bp, BNX2_RV2P_INSTR_LOW, code);

			val = (loc / 2) | cmd;
			REG_WR(bp, addr, val);
		}
	}

	/* Reset the processor, un-stall is done later. */
	if (rv2p_proc == RV2P_PROC1) {
		REG_WR(bp, BNX2_RV2P_COMMAND, BNX2_RV2P_COMMAND_PROC1_RESET);
	}
	else {
		REG_WR(bp, BNX2_RV2P_COMMAND, BNX2_RV2P_COMMAND_PROC2_RESET);
	}

	return 0;
}

static int
load_cpu_fw(struct bnx2 *bp, const struct cpu_reg *cpu_reg,
	    const struct bnx2_mips_fw_file_entry *fw_entry)
{
	u32 addr, len, file_offset;
	__be32 *data;
	u32 offset;
	u32 val;

	/* Halt the CPU. */
	val = bnx2_reg_rd_ind(bp, cpu_reg->mode);
	val |= cpu_reg->mode_value_halt;
	bnx2_reg_wr_ind(bp, cpu_reg->mode, val);
	bnx2_reg_wr_ind(bp, cpu_reg->state, cpu_reg->state_value_clear);

	/* Load the Text area. */
	addr = be32_to_cpu(fw_entry->text.addr);
	len = be32_to_cpu(fw_entry->text.len);
	file_offset = be32_to_cpu(fw_entry->text.offset);
	data = (__be32 *)(bp->mips_firmware->data + file_offset);

	offset = cpu_reg->spad_base + (addr - cpu_reg->mips_view_base);
	if (len) {
		int j;

		for (j = 0; j < (len / 4); j++, offset += 4)
			bnx2_reg_wr_ind(bp, offset, be32_to_cpu(data[j]));
	}

	/* Load the Data area. */
	addr = be32_to_cpu(fw_entry->data.addr);
	len = be32_to_cpu(fw_entry->data.len);
	file_offset = be32_to_cpu(fw_entry->data.offset);
	data = (__be32 *)(bp->mips_firmware->data + file_offset);

	offset = cpu_reg->spad_base + (addr - cpu_reg->mips_view_base);
	if (len) {
		int j;

		for (j = 0; j < (len / 4); j++, offset += 4)
			bnx2_reg_wr_ind(bp, offset, be32_to_cpu(data[j]));
	}

	/* Load the Read-Only area. */
	addr = be32_to_cpu(fw_entry->rodata.addr);
	len = be32_to_cpu(fw_entry->rodata.len);
	file_offset = be32_to_cpu(fw_entry->rodata.offset);
	data = (__be32 *)(bp->mips_firmware->data + file_offset);

	offset = cpu_reg->spad_base + (addr - cpu_reg->mips_view_base);
	if (len) {
		int j;

		for (j = 0; j < (len / 4); j++, offset += 4)
			bnx2_reg_wr_ind(bp, offset, be32_to_cpu(data[j]));
	}

	/* Clear the pre-fetch instruction. */
	bnx2_reg_wr_ind(bp, cpu_reg->inst, 0);

	val = be32_to_cpu(fw_entry->start_addr);
	bnx2_reg_wr_ind(bp, cpu_reg->pc, val);

	/* Start the CPU. */
	val = bnx2_reg_rd_ind(bp, cpu_reg->mode);
	val &= ~cpu_reg->mode_value_halt;
	bnx2_reg_wr_ind(bp, cpu_reg->state, cpu_reg->state_value_clear);
	bnx2_reg_wr_ind(bp, cpu_reg->mode, val);

	return 0;
}

static int
bnx2_init_cpus(struct bnx2 *bp)
{
	const struct bnx2_mips_fw_file *mips_fw =
		(const struct bnx2_mips_fw_file *) bp->mips_firmware->data;
	const struct bnx2_rv2p_fw_file *rv2p_fw =
		(const struct bnx2_rv2p_fw_file *) bp->rv2p_firmware->data;
	int rc;

	/* Initialize the RV2P processor. */
	load_rv2p_fw(bp, RV2P_PROC1, &rv2p_fw->proc1);
	load_rv2p_fw(bp, RV2P_PROC2, &rv2p_fw->proc2);

	/* Initialize the RX Processor. */
	rc = load_cpu_fw(bp, &cpu_reg_rxp, &mips_fw->rxp);
	if (rc)
		goto init_cpu_err;

	/* Initialize the TX Processor. */
	rc = load_cpu_fw(bp, &cpu_reg_txp, &mips_fw->txp);
	if (rc)
		goto init_cpu_err;

	/* Initialize the TX Patch-up Processor. */
	rc = load_cpu_fw(bp, &cpu_reg_tpat, &mips_fw->tpat);
	if (rc)
		goto init_cpu_err;

	/* Initialize the Completion Processor. */
	rc = load_cpu_fw(bp, &cpu_reg_com, &mips_fw->com);
	if (rc)
		goto init_cpu_err;

	/* Initialize the Command Processor. */
	rc = load_cpu_fw(bp, &cpu_reg_cp, &mips_fw->cp);

init_cpu_err:
	return rc;
}

static int
bnx2_set_power_state(struct bnx2 *bp, pci_power_t state)
{
	u16 pmcsr;

	pci_read_config_word(bp->pdev, bp->pm_cap + PCI_PM_CTRL, &pmcsr);

	switch (state) {
	case PCI_D0: {
		u32 val;

		pci_write_config_word(bp->pdev, bp->pm_cap + PCI_PM_CTRL,
			(pmcsr & ~PCI_PM_CTRL_STATE_MASK) |
			PCI_PM_CTRL_PME_STATUS);

		if (pmcsr & PCI_PM_CTRL_STATE_MASK)
			/* delay required during transition out of D3hot */
			msleep(20);

		val = REG_RD(bp, BNX2_EMAC_MODE);
		val |= BNX2_EMAC_MODE_MPKT_RCVD | BNX2_EMAC_MODE_ACPI_RCVD;
		val &= ~BNX2_EMAC_MODE_MPKT;
		REG_WR(bp, BNX2_EMAC_MODE, val);

		val = REG_RD(bp, BNX2_RPM_CONFIG);
		val &= ~BNX2_RPM_CONFIG_ACPI_ENA;
		REG_WR(bp, BNX2_RPM_CONFIG, val);
		break;
	}
	case PCI_D3hot: {
		int i;
		u32 val, wol_msg;

		if (bp->wol) {
			u32 advertising;
			u8 autoneg;

			autoneg = bp->autoneg;
			advertising = bp->advertising;

			if (bp->phy_port == PORT_TP) {
				bp->autoneg = AUTONEG_SPEED;
				bp->advertising = ADVERTISED_10baseT_Half |
					ADVERTISED_10baseT_Full |
					ADVERTISED_100baseT_Half |
					ADVERTISED_100baseT_Full |
					ADVERTISED_Autoneg;
			}

			spin_lock_bh(&bp->phy_lock);
			bnx2_setup_phy(bp, bp->phy_port);
			spin_unlock_bh(&bp->phy_lock);

			bp->autoneg = autoneg;
			bp->advertising = advertising;

			bnx2_set_mac_addr(bp, bp->dev->dev_addr, 0);

			val = REG_RD(bp, BNX2_EMAC_MODE);

			/* Enable port mode. */
			val &= ~BNX2_EMAC_MODE_PORT;
			val |= BNX2_EMAC_MODE_MPKT_RCVD |
			       BNX2_EMAC_MODE_ACPI_RCVD |
			       BNX2_EMAC_MODE_MPKT;
			if (bp->phy_port == PORT_TP)
				val |= BNX2_EMAC_MODE_PORT_MII;
			else {
				val |= BNX2_EMAC_MODE_PORT_GMII;
				if (bp->line_speed == SPEED_2500)
					val |= BNX2_EMAC_MODE_25G_MODE;
			}

			REG_WR(bp, BNX2_EMAC_MODE, val);

			/* receive all multicast */
			for (i = 0; i < NUM_MC_HASH_REGISTERS; i++) {
				REG_WR(bp, BNX2_EMAC_MULTICAST_HASH0 + (i * 4),
				       0xffffffff);
			}
			REG_WR(bp, BNX2_EMAC_RX_MODE,
			       BNX2_EMAC_RX_MODE_SORT_MODE);

			val = 1 | BNX2_RPM_SORT_USER0_BC_EN |
			      BNX2_RPM_SORT_USER0_MC_EN;
			REG_WR(bp, BNX2_RPM_SORT_USER0, 0x0);
			REG_WR(bp, BNX2_RPM_SORT_USER0, val);
			REG_WR(bp, BNX2_RPM_SORT_USER0, val |
			       BNX2_RPM_SORT_USER0_ENA);

			/* Need to enable EMAC and RPM for WOL. */
			REG_WR(bp, BNX2_MISC_ENABLE_SET_BITS,
			       BNX2_MISC_ENABLE_SET_BITS_RX_PARSER_MAC_ENABLE |
			       BNX2_MISC_ENABLE_SET_BITS_TX_HEADER_Q_ENABLE |
			       BNX2_MISC_ENABLE_SET_BITS_EMAC_ENABLE);

			val = REG_RD(bp, BNX2_RPM_CONFIG);
			val &= ~BNX2_RPM_CONFIG_ACPI_ENA;
			REG_WR(bp, BNX2_RPM_CONFIG, val);

			wol_msg = BNX2_DRV_MSG_CODE_SUSPEND_WOL;
		}
		else {
			wol_msg = BNX2_DRV_MSG_CODE_SUSPEND_NO_WOL;
		}

		if (!(bp->flags & BNX2_FLAG_NO_WOL))
			bnx2_fw_sync(bp, BNX2_DRV_MSG_DATA_WAIT3 | wol_msg,
				     1, 0);

		pmcsr &= ~PCI_PM_CTRL_STATE_MASK;
		if ((CHIP_ID(bp) == CHIP_ID_5706_A0) ||
		    (CHIP_ID(bp) == CHIP_ID_5706_A1)) {

			if (bp->wol)
				pmcsr |= 3;
		}
		else {
			pmcsr |= 3;
		}
		if (bp->wol) {
			pmcsr |= PCI_PM_CTRL_PME_ENABLE;
		}
		pci_write_config_word(bp->pdev, bp->pm_cap + PCI_PM_CTRL,
				      pmcsr);

		/* No more memory access after this point until
		 * device is brought back to D0.
		 */
		udelay(50);
		break;
	}
	default:
		return -EINVAL;
	}
	return 0;
}

static int
bnx2_acquire_nvram_lock(struct bnx2 *bp)
{
	u32 val;
	int j;

	/* Request access to the flash interface. */
	REG_WR(bp, BNX2_NVM_SW_ARB, BNX2_NVM_SW_ARB_ARB_REQ_SET2);
	for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		val = REG_RD(bp, BNX2_NVM_SW_ARB);
		if (val & BNX2_NVM_SW_ARB_ARB_ARB2)
			break;

		udelay(5);
	}

	if (j >= NVRAM_TIMEOUT_COUNT)
		return -EBUSY;

	return 0;
}

static int
bnx2_release_nvram_lock(struct bnx2 *bp)
{
	int j;
	u32 val;

	/* Relinquish nvram interface. */
	REG_WR(bp, BNX2_NVM_SW_ARB, BNX2_NVM_SW_ARB_ARB_REQ_CLR2);

	for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		val = REG_RD(bp, BNX2_NVM_SW_ARB);
		if (!(val & BNX2_NVM_SW_ARB_ARB_ARB2))
			break;

		udelay(5);
	}

	if (j >= NVRAM_TIMEOUT_COUNT)
		return -EBUSY;

	return 0;
}


static int
bnx2_enable_nvram_write(struct bnx2 *bp)
{
	u32 val;

	val = REG_RD(bp, BNX2_MISC_CFG);
	REG_WR(bp, BNX2_MISC_CFG, val | BNX2_MISC_CFG_NVM_WR_EN_PCI);

	if (bp->flash_info->flags & BNX2_NV_WREN) {
		int j;

		REG_WR(bp, BNX2_NVM_COMMAND, BNX2_NVM_COMMAND_DONE);
		REG_WR(bp, BNX2_NVM_COMMAND,
		       BNX2_NVM_COMMAND_WREN | BNX2_NVM_COMMAND_DOIT);

		for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
			udelay(5);

			val = REG_RD(bp, BNX2_NVM_COMMAND);
			if (val & BNX2_NVM_COMMAND_DONE)
				break;
		}

		if (j >= NVRAM_TIMEOUT_COUNT)
			return -EBUSY;
	}
	return 0;
}

static void
bnx2_disable_nvram_write(struct bnx2 *bp)
{
	u32 val;

	val = REG_RD(bp, BNX2_MISC_CFG);
	REG_WR(bp, BNX2_MISC_CFG, val & ~BNX2_MISC_CFG_NVM_WR_EN);
}


static void
bnx2_enable_nvram_access(struct bnx2 *bp)
{
	u32 val;

	val = REG_RD(bp, BNX2_NVM_ACCESS_ENABLE);
	/* Enable both bits, even on read. */
	REG_WR(bp, BNX2_NVM_ACCESS_ENABLE,
	       val | BNX2_NVM_ACCESS_ENABLE_EN | BNX2_NVM_ACCESS_ENABLE_WR_EN);
}

static void
bnx2_disable_nvram_access(struct bnx2 *bp)
{
	u32 val;

	val = REG_RD(bp, BNX2_NVM_ACCESS_ENABLE);
	/* Disable both bits, even after read. */
	REG_WR(bp, BNX2_NVM_ACCESS_ENABLE,
		val & ~(BNX2_NVM_ACCESS_ENABLE_EN |
			BNX2_NVM_ACCESS_ENABLE_WR_EN));
}

static int
bnx2_nvram_erase_page(struct bnx2 *bp, u32 offset)
{
	u32 cmd;
	int j;

	if (bp->flash_info->flags & BNX2_NV_BUFFERED)
		/* Buffered flash, no erase needed */
		return 0;

	/* Build an erase command */
	cmd = BNX2_NVM_COMMAND_ERASE | BNX2_NVM_COMMAND_WR |
	      BNX2_NVM_COMMAND_DOIT;

	/* Need to clear DONE bit separately. */
	REG_WR(bp, BNX2_NVM_COMMAND, BNX2_NVM_COMMAND_DONE);

	/* Address of the NVRAM to read from. */
	REG_WR(bp, BNX2_NVM_ADDR, offset & BNX2_NVM_/* b NX2 netwoVALUE);

	/* Issue an erase command. */
	REG_WR(bp,adcom NX2 COMMAND, cmd * CopyrWait for2009pletionadcom re; (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		u32 val* Co	udelay(5 * Co	val = CorpRDation
 *
 * This prog);
		if (censoadcom NX2 is prog_DONE)
			break;
	}

twarej >=modify
 * it under tby: return -EBUSY* Colude <l0;
}

static int
bnx2_nvram_read_dword(struct ulep *tion the2.c: B, u8 *ret_val>
#inccmd_flags)
{
f thecmd;
	int j* CopyrBuild th-2009 Bro ludeadcom cmd =dation.
 *
 * WritteIT |clude <lin* CopyrCalculat (c) 2.c: Brof a buffered flash, not needx/pcor 5709adcom warebp->ci.h>_info <lingsundation.
_TRANSLATErms of2.c: Br= ((2.c: Br/ de <linux/netdevpage_size) <<y: M  nux/skbuff.h>
#includebits) +dma-maclude <l%nux/skbuff.h>
#include <linl Chan /* Need to clear ten  bit separatelyadcom Corporation
 *
 * This prograation.
 *
 * Written b* CopyrAddressnclut.h>odify<lin

#i fromadcom Corporation
 *
 * Th/* bnx2.c: Broadcom NX2 network driver.
 *
 * Copyright (c#inclu009 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written bms of	__bethe  = cpu_to<linu(as published by
 * tREAD) Soft	memcpy(timer.h>
&v, 4lude <ichael C	} Cha  (mchan@broadcom.com)
 */


#include <linux/module.h>
#includee <linux/moduleparam.h>writenclude <linux/kernel.h>
#include <linux/r.h>
#include <linux/errno.h>
#in <linux/al32#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/vmallocation.
 *
 * WritWRloc.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/dma-maping.h>
#include <linux/bitops.h>#include <asm/io.h>
#include <asm/irq.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <asm/page.h>
#include <linux/time.h>
#linux/l&_NAME,E_NA definree sobnx2x/ethdataadcom Corporation
 *
 * ThWRITE, linure.hcpue Fo32clud
#include <linux/ethtool.h>
#"bnx2 tolinux/mii.h>
#include <linux/if_vlan.h>
#if defined(CONFIG_VLAN_8021Q) || defined(Ct.h>iver "009 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of ral Public Liwareas published by
 * the Free undation.
 *
 * Written by: Michael Cha  (mchan@broadcom.com)
 */


#include <linux/module.h>
#include <linux/modulepainitaram.h <linux/kernel.hux/errno. GNU nclude, entry_count, rct and
	const <linux/linux/spec *ci.h>etXtwareCHIP_NUM(bp) == , 0);
MO_initincludde <linux/netd = &linux/initG_CNgoto gete <lparaizeq.h>
#inclDeterminHOR("Mselectedx/moerfaceadcom cense as published by
 * thFG1NetXt

static in = ARRAY_SIZE()");

tablirq.i, int Found0x40
/* inincl
	inclF");
,
	NC370I, has been reconfiguux/pcom ibute it an,ule_paage Signal
	BCM[0]d/or m

static inG_CN e IIj++info[] terms of, int board_FLASH_BACKUP_STRAP_MASKE_PAdma-map CM5716->ct {
	1HP NC370T Multifunction Giginclude, "Disable Messageable_msit SeONFIG_CNIRMWNIC) ||elsems of themasIG_CNncluot yettic struct {
	char *naoftware Found(1 << 23)by: MSX"  = NC370T Multifunction GiBroa706 { "Broadcom NetXt BCM5708 100ame;
} board_info[] __devinitdata = {
	{ "Broadcom NetXtrem	BCM5706 1000Basee-T" },
	{ "HP SX" E_PAR Adapterstrapping NetXtreigabit Server Adapter" },
	{ "Brroadc/* Request acc <litoOR("Mfo[] _
	NC370I,
	BCM5-T" },
	disabulepaacquirearam.h>lockDULE) != 0by: Mnclude <lrc5716 1000BEn	BCM},
	{ "Broa NetXtreme II BM5716 10ulepae PCI_aram.h>,
	{ "DULE5716 1000Basct {
	chaadcom NetXtreme II BM5716 10Corporation
 *
 * ThiFG1info[] er" },
	{lude <DCOM, PCI_DEVICE_ID_NX2_2706,
	  PCI_VEN2OR_ID_HP, 0x3106, 0, 0, NC370I3706,
	  PCI_VEN3OR_ID_HP, 0x3106, 0, 0, NC3devin5706,
	  P"bnx25708S, 1000BDisPCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_NX2_5d, PCI_  PCI_VENDOR_ID_HP,_ID_NX2_5releasatic DEFINE_PCI_5716 10om NetXtreme II BC /* ,
} board_t;

/* indexgabit (mcha==roadcom NetXe_msi, "Disable MessageNULL
	{ printk(KERN_ALERT PFX "UnknownBROADC/EEPROM type.\n",
	{ lude <linNODEVl Chan(MSI)");

type:M5706S,
ulepashmem_rdation
 *
 SHARED_HWNX2__CONFIGR_ID_ Founh>
#incCI_ANY_ID, 0,m NX2 ,
	B708 1000ware Foby: de <linux/type =P_FILE_ase-T" 
	  PCI_ANY_ID, Pde <linux/netdevtotal
typedeule.h>
#ii_tbude <linux/moduleparam.h>

#i <linux/kernel.h>
#include <linux/timebuf,VERSnte <le <linx/erADCOdisable_m#include <linnx2.c: Bg. *leng. *extra716S,
} M, 0x163 },
E_TABe.h>
#incl#inclase-T" },
	{ "Broadcom NetXtreme II BCM571600Base-SX" },
	};

static DEFINE_PCI_DEVICE_TABbnx2_pci_tbl){
	{ PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_NX2_5706,
	  PCI_VENDOR_ID_HP, 0 },
	
	{ , 0x163;
vice.h>AGS	(2.c: BANY_CI_Vsable_/slabe <linx0000000wareN)
	/* Sl& 3rms of 8(BNX[4]
	{  thepre_len5716 0081, 0xa1= ~3ID_NX2RED_F = 4 -#define F184aE_VERSION, SEEPRO>=6 },
	include, SEEPROM_ },
	
	{ P0, 0x4083038ation.
 *
 * WritFIRST |	{ PCrveration.
 *
 * WritLASTG_CNIC)5706 1000B- slow"},
	/* Expansion entry 0001 G_CNIC] =
-SX" },
	ram.h>

#include tion0, BCM5716_ID_clude <linuE_VERSIONrcby: Mbnx2_pci_tbl) linux/list.	 SAIbuf +E_SIZE,
	 SEE,FFERED_F_BYTE_N)
	/* Sl+= 4,
	{ PCSaifap, FERED_FLA	D_FLAGS- need updateRMWARE(_FLAGS184a053, /
	{0x00PAGE_01, 0x0005ates */
	{=, 0xaf02+ 4E_FIE_BIThan  (mc_FLAGS	=S, S53, 0xaf000400,VERSIONlude <linux0253, 0xaf020406,
	 NONBUFFERED_ 0x00050ase-T" } slow"},
	/* Expansion entry 0001 */
	{0x08000002, 0x4b808201, 0x000UN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_01"},
	/* Saifun SA,0253,/
	{0rq.h>
5706 1_PAGE_BITS> dexed_FLASH_PAGE_SIZE_ID, aort.h>firstde <linux/sE,
	 SAIFUN_FLASH_BYTE_ADDR_MASle_mAL_SIZE*2,
	 "Non-buffered flash (128kB)"},ed flash) */
	/* strap, cfg1, & write1 need upNDOR_ID_IFUN_FLASH_BYTE_incluvanc " DOR("Mnext cludeAIFUN_F
	/* strap, cfg1, & write1 ncfg1, */
	{0x04ed flwhile20406,
	 N4 &&I_ANY 0x1384025Non-buffered flash (256kB)"},
	/* Expansion entry 0DEVICE_00000, 0x53808201, 0x00050081, 0x033840253, 0xaf0204006,
	 NONBUFFEREDD_FLAGS, SAIFSAIFUN_ADDR_MASK, 0,
	 "Entry 00YTE_ADDR_MASK, SAIFUN_FLASH_BASE_TOTAash) */
	/* strap, cfg1, & write1 need updates */
	{0x0c000003, 0x4f808201, 0x00050081, 0x0384025num {
, PCI_DEVICE_ID_NX2_5708,
	  PCI_ANY_ PCI_ANY_ID, 0, 0, BCM5708 },
	 (128kBDOR_ID_BROADCOM, PCI_DEVICEVICE_ID_NX2_5709S,
	  PCI_ANY_ID, P"bnx2 <linux/kernel.h>
#include <linux/*HZ)R_ID_BROADCOM, 0x163b,
	  the"bnxten, 0, BCM5716 },
	D, P8 *x0005start[4]);

d_MICR*alignwriteEVICE_,isable_SaifferDEVICE_ID_ PCI_ANY_ID, ADCOYTE_AD
	 ST, 0110: endntry R_MASONBUFFERWREN)
	/* Slow EEPROM *_FLAGS	(BNX2_NV_WRE0110: ST M4 =PE20 (256k0380, 0x009f( */
	/* strap,f0081, 0xa184a,
	{ "B SEEPROM_PAGE_BITSED_FLAGp, cfg1,
	 STSoftware_FLAGS< 4ASH_B_FLAGS	(b808200Base-SX" },
	_ID, PCI_ANSH_PAGE_SIZE,
ST M45P4)
	{ "BEVICE_ID_NXFLASH_PAGE_BITS184a053,  cfg1, & wri253, 0xaf020406,
	 NONBUF 0xaf020456kB PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAI +	 "EEP - 4CRO_FSH_BYTE_ADDR_MASK, SAIFUN_FLAS */
	/* stra|| Fast EEPRTAL_SIZE,
	 n-buffkmallocFERED_, GFP_S,
	EL SoftwareYTE_ADDR_MAEVICE_TE_ADDR_MASKNDORMEM_TOTAL_SIZE,
	
	 STSH_PAGElinux/lYTE_ADDR_FUN_FLASH_B0050081,AL_SIZE,
	OM_PAGE_S0002, 0x6b808201,, 0xa184a053, 0xaf0000050081,NONBUFFERED_FLAGS, 0110: ST M45PONBUFFERE(BNX2_NV__BITSn-buffYTE_ADDR_SAIFUN_FLAS!ude <linux/netdevice.h>
#include BUFFERED,
	{ "BICRO_FLASH_BASEOM_BYTE_264MASK, SEEPROM_TOTAL_SICRO_FLASH_BASROM - fH_PAGE_SIZEExpansion errupt clude "bnx2.EPROM *IC) |
	PAGE_BIASH_BYTFLASH_P(ADDR_MAS<, SEEPRO&&DR_MN_FLASrms of theclude T M45Pclude0xaf0ONBUFAIFUN_FLASH_EPROM * theaddry 0100 */
	OM */nt intrye II   I_VEFinort.h> flash) */0x688 *nameD_FLASH_PAGow EEPRO81, 0PAGE_BIID_NX_FLASH_PAG-= (FLASH_BYTE_ <asm/io.h>
#include <asm/irq.hy boa BUFFERED_FLA & wE_BITS, BUFFERE & wriFLASH_BYTE_+nux/skbuff.h>
#include <liffered flash (123, 0x6e808	/* Expansi, 0xaf02040= y 1010"},_FLASH?_PAGE_SIZE:100 */
	{0x0081, 0x03840253, 0xa},
	/* ExpansiFUN_FLASHDDR_MASK & w>x009f0081, 0xa184) ?ASH_f0081, 0xaxpansion AIFUN_F56kB no000Base-T" },
	{ "Broadcom NetXtreme II BCM5716 00Base-SX" },
	};

static DEFINE_PCI_DEVICE_TABLPAGE_SIZE,
	 SAIFUN_FL01, 0x{ PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_INX2_5706,
	  PCI_VENDOR_ID_HP, 0x3, 0xaf020406,
	 NONBUFFERED_FLAGS, SAExpansion entry 1010 */
	{0x26000001, 0x67808201, 0x0nclude <liAIFUN_FLASH_PAwholRED_FLtrem08201,LASH_Bdma-m* (non-LASH_BAfo[] _only0, 0,	me;
} board_d/or m00000, 0x73808201, 0x00050 jap, cigabit SC370F },
ude <linux/netdevclude <lia053,
	{ "Bro0x37000001,|ASK, SAIFUN_FLASH_BASE_TOTAreme IICRO_FLASH_PAGE_BITS, ST_MICRO_F3, 0xa00 */
	{0x330jZE,
	 B SignalLASH_B[j]ZE,
	 BFUN_FLASH_BYTE_ 0x000500db, H_PAGE_SIZE,
	 SAIFUN_FLASH	 SAIFUN_FLASH_BYTEeme II ASH_BYTE_ADDR"bnx2_ID_BROADCOM, PCI_DEV(unINE_
	.fla-protecttry 11100Base-SX" },
	706,
	  PCI_V0x0005 SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYLoopt Driver "back BUFFERED_F3, 0xude <100 */
	{0x3toreme*3, 0xaf020409_FLASASH_BYTE8848353, 0xaf000400,
	 BUFFERED_FLAGS, BUFFERED_FLASH__BYT004-2FERED_FLry 1110GE_BITS, SAIFUN_FLASH2004-_FLASE_SIZFLASH_BYTE_DEVICE_TABLEsh (256kB)"},
};

static c_ID, -706,
	OR("Michaelagaininux/t.h>actualMichaely 1110TS,
	.page_size	= BCM5709_F5716 1bute E_BIT1100 */
	{0x;	/* Ex<3, 0xaf0204
	{ PCE_BITBUFF, ilash) */
_PAGE_BITS, BUFFERED_"bnx2.h"
#intionx6884YTE_ADDR_MASK, BUFFEi]y 0100 */
	{0x110 0x00050EVICE_TABLE(sh (256kB)"},
};

static const struct flash_spec flash_57LASH_BYTE_ADDR201, 0wsize	= BUFF, 0xaf02040toIFUN_FLASH*name;
} b	 */
	d- txr->tx_ctx_prod - txr56kB	/* Exunlikely(diff >=nline u3	 */
	1100 */ & w6884 |/
	{0x(sion entry 1010 */
	{0x26000001, 0x6780820 &&
	{0x0	val = REFUN_FLASH68848f >= TX_Df000400,
	 BUFFERED_FLAGS, BUFFERED_FLAH_PAGESC_CNT)) {
		diff &= 0xffff;
		if (d
	 SAic const strucbe skip_CNT;
	}
	return sh (256kB)"},
};

static c0, 0x40830380, es, NONBUFFEREDrd_ind(struct bnx2 *bp_MASK,
	.total_size	= BUFF2_PCICFGTAL_SIto	{0x2e00009_FLASH_P353, 0xaf000400,
	 BUFFERED_FLAGS, BUFFERED_FLASH_indirect_lock);
	CFG_REG_WI<	{0x2e000cons;
	if (unlikely(diff >= TX_DAL_SIal = REG_RD(bp,-) */
	{0xxaf020406,
	 NONBUFFERED_FLAGS, ST_MFERED_FL
static ASH_PAGE_BITS, BUFFERED_ff &= 0xffff;
		if (diff == TX_DESC_CNT)
			diff = MAX_TX_DESC_CNT;
	}
	return (bp->tx_ring_size - diff);
}

static u32
bnx2_reg_rd_ind(sAL_SIZE*	.flags		= BNX2_NV_BUFFERED	.page_bits	= BCM5709_FLA PCI_ANY_ID, 0, 0, eeds to be skiDCOM, PCI_DEVICE_ID_NX2_5708,
	  PCI_ANY_I PCI_ANY_ID, 0, 0, BCM5708 },
	{ I_VENDOR_ID_BROADCOM, PCI_DEVICEpyrincrement
	 BUFFE(&bp->BNX2_CTX+al;
}

static- txr->tx_con}

SIZE,
	 SAIFUN_:
	kfree_FLAGS, SAIFU_BITse {
	YTE_ADDR__BITEVICE_ID_NX2_5709S,
	voidFW_RV2P_FILfw_cap;
MODULE_FIRMWARE(FW_RV2P_FI, sigx0000000de <phy0x40830PAGEdcom PHY_FLAG_REMOTEint
bCAP;
#ifdeM_CNIC
static inx2_dCAN_KEEP_VLAN716S,
} nsion ent.h>
#inclunx2_dASF_ENABLE
	{ "uct bnx2 * BUFFEREt drv_ctl_info *info)
D_NX2_5708S,
	  PCI_ANY_ID, PFWuct _MB_BIT },
	{ "HP e DRV_CTL_IOSIGNATUR_DEVICDEVICeg_wr_ind(bp, io->off[] =
{
#dee1 need u Foundation_CTL_IO_ctl_info *inE_PARdata = bnx2_reg_rd_ind(bp, msi, "Disab_ctl_io *io = &info->data.io;

		lock BUFFEREDRV_ACKnd(bp, io->offug 21, 2= bnx2_reg_rd_ind(bpSAIFUN_FLASude <f BCM_CNIC
->offsnt
bnx2_dSERDES_lock)rver 		io->data = bnx2_rv_ctl(struct ev);
	ms of thelinkFLASHurn 0;
}

stat BUFFEREnt
bnx2_drv_ctl(struct ne0384indcom5708S,
	  PCI_ANY_ID, PLINK_STATUS Software[0];
ic voidgs & BNX2_Ftup_cnicp->dby: Mirn 0;
}poLAGS,PORT_FIBREBYTE_ADDR_MATE_USING_MSIX;
		bnaT2_napiaddr, io->offset, io->data);
		breareme II 000002, *bp)
{
	struct cnic_eth_SAIFUN_FLASnetif_runningude <devMEL Asig	stru708S,
	  Pwration
 *
 fset, io->daMBt_loc)G_WR(bp, BNX2_CTX_DATAsetup_msix_tbl;
MODULE_FIRMWARE(FW_Corporation
 *
 PCI_GRC_WINDOWnux/if_Q_FL_MSIX;
	}

	cp->irq__SEP}

	be sk&= ~CNIC_IRQ_FL_MSIX;
	}

	cp-2>irq_arr[0].MSIX_Tev);nux/i_BIT&= ~CNIC_IRQ_FL_MSIX;
	}

	cp-3= (void *)
		((unPBAd long) ude <linux/modulepareset_chi
	spin_unlock_bh(>
#inc.statusodCRO_FLASH__FILE_09_Ait disable_mu8 oldG_MSI free software; t.h>
urrl & PCI transacn re<linuou cane before
L_SIissuBaseONFIsetadcom Corporation
 *
 MISCv(dev);_CLR_BITS,UFFERED_Fstruct cnic_eth_dev *cp =_TX_DMAv(dev);CNICp->cnic_eth_dev;

	if (ops == NUL		retuGINEeturn -EINVAL;

	if (cp->drv_state & CNIC_DRR)
		return -EINVAL;

	if (cp->drv_state & CNIC_DRHOSundeALESCEGD)
		r708S },
e as published by
t cnic_eth_dev *cp =_BIT");
MODULE_VEevice *dev, strufirmwa370To tell us it is oknx2_t bn(CONFI netdev_pic_tafw_synci->last_statusMSG_DATA_WAIT0 |um_irq = 1;, 1, 5708S,m {
	posit a driverdevicenapinatC370soadcom c int bnPCI_s thatstructhis_cnia softdevice *dev)
{
	sg = bnapi->last_statusRESETp, io->off	REG->cnic_eth_;
	bnapi->cnic_pres_MAGICNetXtremDox2_nummy#incluD_BRorx538.h>
hiH_BYTvoid *datalluct cnic_*ops,
			  struca)
{
	 weuct net_device *dev) = CNIC_DRV_STATE_REGD;

	bIe Soi, int, 0);
MODULE_PARM_DESC(disable_msi,riv(dev);
	struct cnim/page.h>
#inc= &bp->cnic__SWbnapi-_BITSC_DRV_STATE_REGD;

	bhe Free Softeral Public License Q_FL_MSI 0, _id = b BCM_Corpo
	cp->ENACNIC_IRQ_FLev;
	cp->io_base = bp->rTARGET_MB_WOR_ownx2_napipcibnx2 *bct {
	 0xffff;
->pdevarr[0].vec>io_base = bp-> */
#truct} 706 1000Bbp->pdev;
	cp->io_base = bp->rCORE_RST_REQrv_ctl = bnx2_drv_ctl;
	cp->drv_regegview;
	cp->drv_ctl = bnx2_drv_ctl;
	cp->drv_register_cnic = bnx2_regist<lin);
	evice *dev)bnapi->status_blk.msunregister_cnic;

	returnIFUN_FLABase_MASKanunlogister af_ctlk);
	;
	strwill hangc_loTAL_SIbus on/ini6 A0 cludA1.  The msleep below provides plentyTAL_SIof margs 256 iiver "posting.TAL_SUN_FLASHt, 0);IblisE_PARM_DESIDled 6_A02_PCICFrver  cnic_ctl_info info;

	mutex_1
	{ "Bric vo(20100"},_ID, 	strtakes approximterr30 us(disname;
} b"5709  i < 1 bnx00Base-T"cense as published by
c_ops) {
		info.cm,
	{ P },
	{ "HP (BOL(bnx2_cnic_probe);

static void
bnx2_c0x08000002,nx2_cnic_probe);

static voiBSY)E_PARE_TABLEom NetXtre");
MOD101008201, 0x000_status_idx;
		}
		info.cmd = CNIC_CTL_START_CMD; bnx2_drv_ctl;
	cp->drv_regata, &info);
	}M_TOTAL_5706S,
	 ERRANY_IDck);
	c_op did#inclvoid *da6S },
	{clude <linux/modASH_BYTE_/* Make sC370byte sw000Baseis2_cnperly ct {
	char_probe(struct net_device *deMSIXx2_r_DIAGk);
}ware FouVICEx0102030) */
	{

#endif

static int
bnx2_inclin correctRO_Fian mod*bp, u32  PCI_VENDOR_ID_BROADreturn 0;
}

static int bnx2_finitXtrts P_FIializan redistriFLASH_PAGEtruct bnx2 *bp = netdev_priv(dev);1	struct bnx2_napi RD(bp, BN_MASK,bnx2_pci_tbl)spinFINE__bh(&TE_USINGINE_);
}struct n
	{ PCISING_MSInet_DATA, val);
	}
	 },
	{
	return 0;
}

static void bnx2_setrv_ctl(struct c_irq_infoO_COMM_ST!ART_BUSY;
	REG->cnic_taget_default_remote_[0];DIO_COM_COMM,
	.paEXT |
		BNX2_EMAC_MDi, int, 0);_ctl_info info;

	mutex_locoffsinclujustc_locvolttatiregularnx2_uwo steps lower
}

sta		if (!uct bnx2 

	mu>cnic_ctlisC_MDO_COMMBCM5716 riv(dev);
	struct cniVCorpCONTROL,MDIO_COMMf0384if (!(bpmov>indd rNDOWmemory= BUFFstatirecnicols = bp-e-SX" },
	}BYTE_bad_et =DIO_COMhan  (mcuct bnx2 *bp = netdev_USING		((u_MDIO_COMM);= 0;
		cp->iuffered flash)*/
	{0x15000001, 0x5780P_FILs_blk_num = sb_id;
	&bp->indirect_mtuAL_SIZE*4,
	 BUF1;
	int i, re_loc
	NC3rupr_cniincl
			v,
	BCM5>cnic_ops;
	if (c_ops) INTt, io-M.h>
#incphy(struct bnx2 *bon Giruct{
		*bp->pdev;
			re= bp->rriv(dBYTEal1 =EINVAL;

	;

	if (bp->phy_flags = bnx2_r |
#ifdef __BIG_ENDIAN_FLAG_INT_MODE_AUTO_POLLCNTLs & BNX2_PHY_#	REGf_MDIO_MODE);
		val1 &= ~BNX2_{
		val1 = R = 0;
			re
#in_CHANSer" 12ODE, val1);
		devinRD(bp, BNX26int i, r|= (0x2er" }lockapter" 15708S,
	return bnx2 *bp = netdev_PCIXMEL ATistebusram(ed_mhz= RE13,
	{ "
	val1 =ter" },
e1 need u, 0);
MODULE_PARM_DESC(disab6c_irq_info(

			val1 = !o info;

	mutex_loc&& truct bnx2 *bp = netdev_AC_MD_EMAC_MDIO_DE);
		val1 &= ~BNX2_PC_MDPONGif (vector;
	cp->irq_arrf (bp->phy;

	return(5);

			val1 = REG_RD(bp, BNX2_EMAC_MDIcense as published by
Tf (bp->phy);
}
);

		val1 =2_EMAC_MDIO_ONEO_COMMBUSY) {
		*val = 02_EMAC_MDIO;

	retuG) {
		val1 = REG_RD(bp, BNX2_EAC_MDIUFFER16ic i);
	}
ter_c

#incp->drvunregister_cnicistercix
	}
 +_ops_X2 *bpCMD;
		c_  hun16);
}
er_cnic;
	cp->drvAC_MDIO_MODE);
		val1 |= BNX2_EMAC_MDIO_MODE_AUTO_		val1SAIFEMAC_MDIO_EROflags & riv(dev);
	struct cnic_eth_dpi->cp = &bp->cnic_eth_dev;

	if (osable_inm_irq = 0;
	cp->drv_sODE, val1)bnx2 *bp)
{
	int i;_statedata;
	rV2Pnapi;

	for (i = 0; i < bp->irq_nvecs; i++) {
	= -EEXT->drv_statCopyrielay(40)el Cnt0x00m000Basecludzero ou
			vaquick       Bs
}

ststruc      BNb	.pagm);
	have_rc

#iytic st706,
	LAG_INT int, 0);
MODULE_PARM_DESC(disable_msi,FLASH_PAGEP_FILinit BNX   B8 },
	{ ADDR_MASK, 0,
	 "Entryn cp;
} = REG_RBNX2_Efor (i = 0; ;

static const sBNX2_Epu_ID_H_table[] =
{
#define BUW_RV2P_FILE_09);* Entry 0110M);
mac_ offf;
		G_MSIX;MSIX;NDEX_y 0100"} = CNIC_DRV_STATE_REGD;QAC_MDIO_COM },
	{ atic i		       _KN2_EMP_BLK PCI_napi->l		val1 =s_idx);

		REG_WR(bp, BNX2_256COMM, v, 0);
MODULE_PARM_DESC(disable_msi,ICFG_INT_ACK_CMD, bnapBINK_CMMODcnic_NX2_PCICFREVtl_info info;REV_AxASH_BICFG_INT_ACK_CMD, bnapHALT_DIS	return ret;
}

static v	       ;

	returncense 0x1O_CO25F0MAX_CID~BNX * MB SEEPRO_CTX PCI_g) bnapi->status_blkMQ		REG_WR(ew;
ecs;RT>phy_flagif (!netif_running(bp->)
		rEograync(struct bnx(BCM_PAGE+) {
 - 8nux/ 2b808Corporation
 *
 Rapi le_int_sync(stru<lin 0, NC370statitype_probe(struct net_device *deTBDR       bnapi->last_statu(i = 0; i <+)
		sBNX2_PCICFG_IN; i++)
		synchronize_irq( |_t;

bp->irq_tbl[i].vect(i = 0; i < < bp->irq_nvecsisteD_INDEX_[0]tops
statiOMM_C 0; i < b1]e_ir8itops_nvecs; i++)
		napi_2nable16&bp->bnx2_n i++)
		napi_3->irq_nvecs; i++)
		napi_4nable(&bp->bnx2_napi[i].napi);
5

staticbp->irq_tbl[i].vectEMACT MulOFF_SEEi < bp->irq/* Progra (bp->MTU.  Also include 4et;

sinux/CRC32adcom mtuor (i =	    , BNX2cense tart+ ETH_HLENtimeoutFCS_LENVALware Fou>	int iETHEEPRT_PACKi->cnZEAGS, y(10);

		val1 =bp->dRX_MTU PCI_DJUMBOp->d	if (netif_running(bp->d(&bp->intr_MDIO_COMM_STARtart< 150E_TABtart =es(bntry 0110: g_wr_inI_ANY_ID, PRBUF
}

statinx2_enable_int(br.
 ll_qcludeapi_enable(bp);
			bnx2_enable_int(b2
bnx2_free_tx_mem(start(bp);
		}
	}
}

static void
bnx2_free_tx_mem(3m_tx_rings; i++) {
tart(bp);
		mittesei = ->uleparapi[0]. <lius_blk.msi,d_inpi[inx2_tx_ <lisR_MASK, 0			struct bnx2_nrunninAX		((unVECi *bnaic_eth_];
		struci].lasti->txus_idxk);
}

#ifdeidle_chkRING_SIZE,
				xffff}

#ifderx_(bp,>pdev;
	p->dev)) ODE_S	bna);
	}
isablSet up how(&bpgenee <a a&bp-> c;
	}40);
	}

	radcom Corporation
 *
 bp->dATTENTIONp->dstatic void
bnx2_free_rx_IC_DRVvector;
	cp->irq_arrHCecs; i++q_tblL &bp->cnic(u64)txr = &bnapiblk_X2_PCICFd_t; txrapi[
	if (netif_running(->num_rx_rings;H,struct bnx2_napi *bnapi = &bp->> 3R_ID
		struct bnx2_rx_ring_inISTIC_rings; i++) {
		struct bnx2_nap *bnapi = &bp->bnx2_napi[i];
		struct bnx2_rx_ring_in++) {
			if Hrxr->rx_desc_ring[j])
				pci_free_conor (j = 0; j < bp->rx_max_rinTX_QUI io-ONS_TRIPi++) {
		stpi[itx_K_INTx2_ns_trip_00,
static |txr =x_buf_ring);
		rxr = 0; j < bp->rx_max_rinRng[j] = NULL;
		}
		vfree(rxr->rr_buf_ring);
		rxr->rx_buf_ring = NUng[j])
				pci_fre = 0; j < bp->rx_max_rinCOMP_PROD;
		}
		vfree(rxr->rvoid_prod		rxr->rx_buf_ring = NUrx_pg_desc_mapg[j]);
			rxr->rx_desc_ringTICKS,rxr->rx_bticks->rx_buf_ring = NULL;
		rx j < bp->rx_max_pg_ring; j+r->rx_pg_rinr);
		rxr->rx_pg_ring = NUnx2 *bp)E,
						    rxr->rx_pg_des_mem(st					    rxr->rx_;
		rxr->rx_pg_ring = NUbnapi = &E,
						    rxr->rx_pg_dMD
		struct bnx2_napi *bmdpi = &bp->bnx2_napi[i];
	>tx_buf_

		REG_Wuct bnx2 *bp = netdev_BROKENecs; Sonsi	struct bnx2_rx_ring_inS_mem(strk);
}	bnapi ULL)
			return -ENOMEM;

		txr->g[j])
				_RING_SIg =
			pci_alloc_consist_COLLECT
		txr->txbb8);FLAGS3ms, 0, NC370c_lock);
	c_ops = bp->cnic_ops;XPORT_SYMBOL(bpg_detart(stmappinMEM;
_desc_r
EXPORT_SYMBOL(bic int
bnxtx_mMR = NUug 21, 2ic int
bnx(rxr = 0; i <nt = 0;
	rcu_aic int
bnx2_alloc_rx_mem(s) {
		val1 =irq_nvecs > 1ruct cnic_eth_dev *cp HC		((unBITci_fTORent = 0;
	api->rx_ring;
		int j;

r.
 lic Licens		val1 =ic int
bnxSBrings;INC_128Blags & BNX2_PHY_FLAG_INT_MODE_AUTt = SHOT		((turn 0;
E * bp->rx_max_rinOMEM;

	 < bp->num_rx_ringUSEruct PARAM,
						    rxr->rx_pg_debp)
{
	int i;
			struct1bnx2_nruct bnx2_rx_i *bnapi =  theb04-2#inci -ng_i*x2_rx_ringB_max_ringsem)tops.h>
(bp->pdev, RXBD_RIN1
		*vCorporation_all_MODE     &rxr->rx_desc_gs; i++) {
		stru>rx_desc_ring[j] == Nr (i = 0; i <rn -ENOMEM;

		}

		if (OMEM;

	{
		*vng[j]);
			if ( +rx_desc_ring[j] = NULL;
		}_OFF_MODExr->rx_buf_ring);
		rxr->rx_buf_ring}

st= NULL;

		for (j = 0; j < c(SW_RXPG_RING_SIZE *
						  r->rxing);
			if (rxr-
		rxr->rx_pg_ring = NULL;
	}
}

stac(SW_RXPG_RING_SIZE *
					 j++) {
			if (rxring);
		x_pg_desc_ring[j])
				pci_free_consistent(ent = 0;dev, RXBD_RING_SIZE,
		->rx_max_pg_ring; j++) {
			rx*
			       bp->rx_nx2 *bp)
{
	int i;

	for (i = 0; i < BASE_TOTCx/del
	NC3nalUN_Fts ic inersadcom Corporation
 *
 pg_desage.h>
#inc i;
	strucdev *x_desNOW = 0; j < bp->rx_max_rinATTN+) {
	(dev);, um_rx_rimem(EVENc_irqi->int_num |
		 _locreceivv *clt
			c_lock);
	et_c_mappiNG_MSIX;
ruct bnx2 *bp = netdev_priv(dev);
	struct c = CNIC_DRV_STATE_REGD;

	bNEW(strucCTOM_TOTICFG_INT_ACK_					    bp->ctx
		return -USY;
	else
		ret = 0
			bp->ctx_blk[i>phy_flags 1 = (bp->phy_addr << 21) | (reg << 16) |
		BN2ug 21, 2netdev_pCesc_napi-_MODE MM_COMMArn ret;
}

static void
bnx2_disable_intc_eth_dev;

	if (oDEFAULTHIS_C_DRV_STATE_REGD;

	bnx2_setsable_inp->st");
MOD {
		ifpi[ihc_lab.h>as published by
 i;
	strucfered flash)*/
	{0x15000002_CTX_DATAux/de_ringi->txes_MDIO_MODE, val1);
		<linux/kern	stru *bstruC_MD blocks inttx
	/* CMessa*txration. */
	statrs_blk_size = r1_CAC00,
	 BUFif (txr->tx_desc_ring) {
			pci_free_coRV_CTLo one= |
		Bt(bp->pdev, USY;tx/
	d&blk_sNULL;	/* 2_EMA_MAX_MSIX_HW_eof(str
		*vtxrNULL;g);
ASH_BYTEE);
	hwtus_>status_statr);
	rxg_descbseq_blk_size +
				size_blk_size +
				atus_blk_size +
				sg_block);

	status_blkpgk = pci_allo}_WR(bp, BNX2_CTX_DATA, val statur (i <linux/kernel.h>
#inccidFUN_. */
	status_blk_size = L1_&bp->indirect_2.c: B0p->statu1p->statu2_PAGE_SIZD, PCI_AidNDEX_ = er_c;

	/* b(ciis freNX2_PCICFG_INT_ACK_CMD_INDEX_VALID |
		>status>pdev;
	L2intrTYPE_XIUSY;tats_sii->status_blk. &bnasi->status_tx_quUFFEstatus_blk.mi = BHr->rx_I>status_tx_qu3ns_ptr =
		&bnapi->status_LO>statu cp;
}
EXPO		&bnapi->status_blk.msi-atus_tx_quick_consumer_index0;
	bi->hw_rx_cons_ptr =
		&bnapi->status_blsi->status_rx_quick_consumer_index0;
	i, 0x47bp->pdev;
	s_blk.msi->;
	bnL		    bnas_blk.msi->
		  LSH_Patus_btxnapi->lanapi[0];p->status_sync(struct bnxconsumer_index0;
	bn					 BNX2(8c(bp);
	if;
			bnapi->status_blk.msix = sb1 < bp->irq_nvecs;ruct E);
	bp-descrx_desc_mappink_consumer_index;
			bnapi->hw_rx_c. */
#r =
				&sblk->status_rx_quick_consume>bnx2_napi[ik_consumer_index;
			bnapi->hw_rx_c3k;
			bn			  &bp->status_blk_mapping	/* (status_blk == NUL PCI_/* Cnumistics blocktx_bd= L1b32
bL)
		go = _indIDation. */
	stato one allocation. */
	status_blk_size = L1_CA_conk_size = L1_CACHE_ALI09) {
		N(BN2_MAX_MSIX_HW_VEC *
			AND_REA9) {
		}
	mutex_AGE_SIZE;
		if 	bnapi AGE_SIZE;TSS;
		 +709) {
		sist2 *bp)
{napiake_thre] __d>ctx_bl	/* Co
		 / 2nsis000 AX_Matus_rx_quick(bp-[) {
L)
	Ed = NTE_SIZ000 tx_blbd_h off_hsizelk->status_rx_quick_consumer_index;)
		goto alloc_memlsageblk = status_blk + status_blk_size;

	bp-x_blktx_blblock);

	se_mem(bp);
	of(struct sn -ENOMEM;bidxi[0];
	bMB_napi->status_blk.IZE *
		s_blk.mXm_irq BIDXrn -ENOMEM;f(stw_link(struct bnx2 *bp)
{
	u32 fw_link_status = 0;
SEQntry 0110_mapping);
	if (status_,tatuping + status_blk_size;

	ifrxbdalloce status r = 0x2eof(str[], dmaNDEX__tF)
	HALF	pci_, 0x163j] =
				00,
num			if b,
	  PCIcation. */
lex == DU0 / 	if (bp->flags & BALF;
			ei *bnapi = PAGE_BITS,X2_LAX_MDUPLEX_Hi] {
	ame;
} board_d/or m) {

	rcp);
	ifhe te, X2_L00Base-T"X2_L
				bdEEPROM_BNX2_NV_WRELF;
			else
		x4083038RX_BDo = &Sreturn |0FULL;
			breEN	if x03840253i003, ALF;
			esisteASH_Bit andYTE_ADDR_MAit ai +f (bLF;
			else
		oc_mem_err;

	errnk_sj]er_index;TATUS_1000HALF;
			c_mem_err;
_link_slk_size;

	bp->					  &bp->status_blk_mappeof(strNUM(bp) == CHIP_NUM_5709) {
		bp->c00,
	 B) {
	_des,709) {_des/ BCM_PAGE_STA_us_blk.msi_FILE_(bp->ctx_pages == 0)
		>ctx_pages; i++) {
			bp->ctx_HE_ALIGN(sizeof(struct statusNX2_SBLK_MSIX_ALIGN_SItent(bp->pdev,
						BCM_PARE_SIZE,
						&bp->ctRX_Rk_mapping[i]);
			if (bp-atus = BNX2
	bnapi->status_blk.msi ase SPEED_10:
			if (e +
				nx2_alloc_STAf (!(bmsr &X2_PCIC,
				 nx2_netlse
uf_usde <li>pdev,c_maaoto alEntry 0110== DUPLEbmsr;

		switch_free_consistent(bp->pdev, BCM_PAGE_SIZE,
					    bp->ctx_blk[i],
Q_MAP_L2_blicBUSY) {
		*val = 0xATUS_AN_CO_LINKug 21, 2		fw_link_s_ARMreturn 0;
			bnapi->statatus = BNX2_Lfw_link_staPG 0x6			netiRD(bp, BNAG_PARApgto alloc_mtatus_bli_bmsr, &bmsr);

			if (!(bpgbmsr & BMSRMODE_AUTO_	return ((bp->ph ||
			    bbp->phisable(&bFLAG_PARALLEL *
bnx2_blk_mapp3, 0x7eags & BNX2_PHY__buf_ring isable(&bp->

	bnx2_shmem_wr(bp, BNX2_LINK_STATUS, fw_link_status)hy_flag

	bnx2_shmem_wr(bp, BNX2_LINK_STATUS, fwRBDC_KEY		rxr->rx_buf_riNFO PFX "%sem)) {KEY -709) {
		bic License lk->stFIBRE) ? "SerDes" :
		(bp->tatus = B
	bnx2_shmem_wr(bp, BNX2_LINK_STATUS, fwNfw_linDtatus_bld = CNIC_CTver_str(bp));

		printk("%d Mbps ", bp-lk_size;

	bp->eed);

		if (bp->duplex == DUPLEX_FULL)
			printk("fullLOd = CNIC_CTNX2_PCICFG_INT_ACK_CMD_INDEX_VALIDs = E;
		}
	}
	else
		fw_link_
		strucnsmit ");
	p->stats_blk1);
ver_str(bp));

		pri("%d Mbps ", bp->line_sped);

		if (bp->duplex == DUPLEX_FULL)
			ptk("full duplex");
	it ");
			}
			printk("flow control lk_size;

	bp->stats_blk_mappin;
	}
	else {
		netif_carrier_off(bpeceive ");
			else
		
	di);
}

_consistent(bp-
		if (txr->tx_descic char *
bnx2_xcei		case SPEEDtatiLAG_INT_MOrn (bnx2 *bprxr
			else
		) <mutex_uom NetXtrstatic NACK_FULL;((bp-> = BNnk(bp);
}

Rfw_liRC_MDIDXFLOW_CTRL_conconsistent(bp->pdesolve_link(bp);
}

static void
bnx2solve_flow_ctrl(struct bnx2 *
{
	u32 local_adv, remote_adv;

	bp->fskbctrl = 0;
	if ((bp->autoneg & (AUTONEG_SPEED | AUTONEG_FLOW_CTRL)) !=
		(AUTONEGEED | AUTONEG_FLOW_CTRL)) {

);
}

statbmsr, +
				rt_fw_link(struct bnx2 *bp)
{
	u32 fw_link_sta = 0;
D
	if (		bnx2_relags & BNX2_PHY_FLAG_REMOTE_PHY_CAP)
		return	if (bp->liCTRL)) {

		ead_phy(bp, BCM5708S_1000X_STAT1, &val);
		if (val rintk
	if ZE,
				16ctrl = 0& BCM5708S_1000X_bnx2_read_phyLOW_CTRLturn;
	}

	bnx2_read_(bp, bp->miLOW_CTRZE,
					    8S_1000X_STAT1_TXf (bp->phy
}

statiping + status_blk_size;

	ifall			if (bp->dupDE, val1);
		STATUS_25the GNU Geatus_blk;

	/* Combine o be skpi_enable(struct SCHblk_maFGs);
}

flow_ctrl(struct bnxALF;goto al
		casepi = &bp->bnx (CHIP_NP_NUM_SIZE, GFP_KEISE_1000XPSEring_SY;
	else
		ret = 0;SE_PAUSE_CAP;eturn YM;
		if (remisten_irq(_RING_) {
		stx_blk_mappi<< 7 &bp->>irq_tbl[i].vectoLUP2_read< bp->rk);
}api_enable(bp);
			bnx2_enXP_SCRATCH2_reaTBL_SZy 0100"} (local_adv & ADVERTISE_;
		}
	E_ASYM)
			new_localPLEX_HALADVERTISE_PAUSE_ASYM;
dv;
	}

ring_info al_atbl_EPROM AGE_tbr_str(8 *) &_PAUSE_C = REG_RD(SE_PAUSE_ASYM;

		local_adv = new_local_aCMD;
;

		local_adv = new_loca_starNTRIEc_mem(if (bp->flags & BNX2_FE_PAUSE_CAP) {
				bp->flow_ctri *bnapi = &tbl[i % 4]BNX2_%c. */
	if (local_adistenapi->lastAUSE_E_PAR3tex_unl_PAUSE_ASYM) {
	  nx2_reVERTISE_PAUSE_CAP) {
				 + idv & ADVmware.h>
#in_PAUSEclude flashi->hw_tx_co
			new_remote__IPV4 new_lsi->ALL_XI	struct bnx2_nap}
	}
	else if (loca6_adv & ADVERTISEdesc_mapping[j])) {
		if ((remote_ad;

	return ng + status	pci_->phyindALLEL_DET(->num alloc_matus =LLEL_MICRO_FLASH_max,			break;
+) {
TE_ALASH_P}
	}
}

s >nk_status = BNX2struct  alloc_me-=nk_status = BNX2_
		ALF;
			e++FLOW_C/* rouBUFFo, 0x00p;

	nx2 2ans_sta
			int
bnx20,
	 "Entryy(bp&HALF;
			el}
	mutex_y(bp>>2 *bp)
NIC_Dtruct bnx!, MII_write_p<<2 *bp)
lude <lmaxnic_present = 1;
		sb_id zeof(struoc_m(status_blk == NULL)
	_MICRO_FLASH_ DUP

sta DUPpace, jumboOM, PCI_D/* 8bp->dev-FG_IN *in	val1 line_st = jiffies;	/*timeout */
	}
W_CTRL_ing)SET + 8bmsr, &
		bp = SKB_flagsALIGN(	speed =SPEED_MSK;_GP_Tu32 ruct MIIPADtops.ne_sof(status_skb_sharedsize x2 *bp)
{k_stapy == NULL)
EED_MSK;COPY_THRESHnet_devhar *
bnx2_xceiASH_BYTPHY_FLAG_SERDES) ? MII_BNX2_GP_TOP_AN_SPEEDZE,
					 ine u32) {
		ca>tic void
bfor (i = 0; i < 50; i++) {
		em)) {tx_buf*bp)
{
00,
low_nx2 )
		s_GP_TOal & MII_BNX2- 40)->liisableHIF4,
	 "duplex = b =peed  *PEED_2OM */
	{if (val & M;

	bpTOTALMSK;PGlink_up = 5709 f (val & MII= DUPLEX_FULL;
	else
		bcnic_eth_		break;
		case MIduplex = bp- int
bnx2OP_AN_SPEED_1Gctrl = FLOW_CTRL_TXplex = DUPdv & ADV	k_statu_SPEED FLAG_Ucase MII_BNX2_GP_TOP_AN_Stic charOP_AN_SPEED_*ONEGink_up = 1if (b BNXNY_ID, P>line_speed = SPEED_SPEED_MSK;
	swit *bp)
{
	u3ED_100:
			bp-ze,
		BNX2_GP_T		 "Copper"))c vo
typedef/* hwntry 1al & dev)
er" :
		 "eed = val &X_STAT1_SPEED_10ED_10:
			bp->l_BNX2_GP_Tplex === NULL)
_1000X_S-ine_speed = SPEED_ctrl;
		}
		retuMII_BNX_BNX2_GP_TOP_A
	bp->link_up = 1;
	bnx2_rne_spek_statuAT1, &val			bp->line_speCM5708S_1000X_STAT1EED_MASK) {
		case BCM570napi;
	void *status_phy_l_adskbnew_remote_adv = 0;

		if (loif (local_adv & ADVERTISE_1000XPSE_ASYM)LING)ATUS_2500FULL;
			break;
		}

		fw_link_sGN(BNX alloc_mem_err;

	memset(statAX_MSIX_HW_VEC *
					PAGE_BITS,emotbp->phy_fufe_speedROM - fast");
	inuPCI_Dibute it and/or mem(bp);
	if; bp)
{
}eak;
		c = st 0x200	 SEEPR
	err = b(bmcr & [jPLEX_>duplex =kK, BU *skb =es = uf->skb_PCICFG_RE0;
	}FLASH_PAGE_BI9 10
	{ PC) {
		bp->t, u3ENABLkb_)
		unmap |
		BN	       ,	cas,);
		TO_DEVICe.h>
#ats_bnx2_readDEVICE_IDus = B+=	case Mnetdbp->)->nr_frWR_CLINK_ST	   Bse {
UPLEX ADV
{
	u32 vp->duplex = DUPLEX_HALF;
 DUPLErn 0;
}

static int
bnx2_5706s_linkup(struct bnx2 *bp)
dv;
	}

	/* Seocal_adv, remote_adv, common;

	bp->link_up = 1;
	bp->line_speedS_LINK_UP;

		if (bp->autoneg) {
			f->mii_bmcr, &bmc		bnx2_rebmcr & BMCR_FULLDPLXCTL_IO_RD_0 (Atmel)"},
	/* ATMES_1000X_STAT1_Fhe terms of>duplex = D == DUP;
	}

	r & BMCR_ANENABLR_ANENABLE)) {
		return 0;
	}
dv);
	_read_phy(bp, bp->mii_adv, ;
	bnx2_read_ph_TOTAcia, &re_singl {
			r_cninx2_re  & ADVERTIDEX_Vdv);
	,NX2_PCIC)			bp->ly_flags & BNX2_PHY_F		bp->lEMAC		reFROMdv & remote_acommon = lommon & (ADVER{

		if (common & ADVERTIemote_adv, common;

		bnx2_rX2_GP_TOP_Ahe teries, one >duplex low_ctrl = 0;
jreturnp->duplex = DUPLEX_HALF;
= DUPLEX_FULL;
		}
		elseLEX_HALF;

	return },
	{(bp, bp->mii__adv & rem));
	cp->irq_arr[0].statunick_num = sb_id;
	cp->num_irq = 1;
}

s PCI_Alex =-SX" },
	.status_blk	if (_irq = 1;
emote_adv;
		_adv & remoD_READ | BNX2_EMAC_MDIOEG_WR(bp, BNX2_PCICFGelse iCK_CMD, bnapi->int_num |
		       ;
		u32 ne },
	{E_09);
MODULE_FIRMWARE(FW_RV2P_FILEbp->line_speed = SPE PCI_statuphy->duplex = DUPL00Base-SX" },
	 {
				bp-x2 *bp = netdev_pus_blk.msiCK_CMD, bnapi->int_num |_COMM_DISEXT |
		BNX2_EMAC_MDIr(struct bphye if (commo			}
				bp-& AUBNX2_EMAC_Mstatic c

	for (i = 0; i < 50; i++) {
		udelay(10 = REG_RD(l1 & f BCeveni = 0; iDIO_COMM_START_BUSY)) {
			udelL) {
				bp->line_speed = SPEEshutdown_EMAC_MDIO_MODE, val1);
		REG_(common & SIZE, GFP_KERNEL);
		if (txr->NO_WOv >> (common & ret;

	if>line_speed UNLOAD_LNK_D	bp-, 0xaf020;
		wo5709,	return 0;
}

static void
bnx2_SUSPEine O_ID_	bnapi  cid)
{
	u32 val, rx_cid_addr = GET_CID;
		}
CI_DEVICE_ILL;
			}
			else if (common & ADVude <linux/modulepates!(vanic_ctnew_remote_adv = 0;

		if rROM *bnx2_reisled Inte <linuxsi = 0;

moduING) {
	 w EEPROM * FLOW_C 0xaf00#def706 o *io =_NOTapi;
	1VERTISE  rw_SX" },
	LT;
		eose
			lo}& BNcp->[ASYMoffs{MDIO_6cfo *tDIO_COMM_lo_water >=3f }	elsIS;
		9lo_wo_wa_napi[iry 0ater >= bize)
			lo_wa4 (lo_water >= bp->rx_ring>rx_ri)
			lo_404ase DRV_ATER_MARK_ater = b3fwater <= lo_water)			lo_wa18r = 0;

		hi_water /= BNX2>= bp->r

		hi_wize)
			lo_41clo_water /= BNX2_L2CTX_LO_WATER_MARK_SCALE;

		if (hi_20lo_water /= BNX2_L2CTX_LO_WATER_MA80 if (hi_water == 0)er = 0;

		hi_water /= BNX2i_water <= lo_water)ter == 0)	lo_water /= BNX2_L2CTX_LO_WATER_MAT);
	}
1;

		if (hi_5
			lo_water = 0;
		val |= lo_water	elseALE;

		if (hi_5L2CTX_HI_WATER_MARK_SHIFT);
	}
	bnxRK_SCALE;

		if (hi_5	lo_water /= BNX2_L2CTX_LO_WATER_MARK_SCALE;

	, rx_cid80	lo_water /= BNX2_L2CTX_LO_WATER_MARK_SCALE;

		if (hi8	for (i = 0, cid = RX_CID; i < bp->num_rx_rings; i++, c86	lo_water /= BNX2_L2CTX_LO_WATER_MA7bp->lin_EMAC_TX_LENGater > 0xf)
			hi_water = 0xf;
		elbp->line_speed == SP7
			lo_water = 0;
		val |= lo_water_HALF)) {
		REG_WR(bpL2CTX_HI_WATER_MARK_SHIFT);
	}
	bnxbp->line_spe, rx_cidc bp-, BNX2_L2CTX_CTX_TYPE, val);
}

static void
bnx2_incter = 0;

		hi_water /= BNX2>= bp->rx3ffMAC_MODE_HALF_DUP	lo_water /= BNX2_L2C)
{
f0ff073ter <= lo_water)
			lo_*bp)4;

		if (hi_water <= lo_wvoid
bnx2_i10PLEX |
		BNX2_EMAC_MODE_MAC_LOOP | BN00_EMAC_MODe SPEED_4LINKlo_wat1c008}
	bnx2_ctx_wr(bp, rx_ci149f (lo_wa8	int i;T_MII_10M;
					break;
	aX2_EMAC_MT);
	}
	bnx2_ctx1LE;

		if (h14af (lo_wat
		hi_water*bp)
;
					break;
	bwitch (bp->line2speed) {
			case SPEED_4b100:
				val |= BNX2_EMAC;
					break;
	cwitch (bp->line_speed) {
		9				val |= BN 4;

		if (h3ll through */
			case SPEEDcf (lo_water >= bp->rx_ring				/* fall thdter = 0;

		hi_water = bp->rx_riPORT_MIIPORT_M}

	/* Set the MAC to operate in th8ter = DUPLEX_HALF)
		val |= 3	if (bp->du2lex == DUPLEX_HALF)
		val |= BNX2_EMAC_M2DE_HALF_DUPLEX;
	REG_WR(bp3frx PAUSE. */
	bX2_EMAC_Mf3f3f0);

	if (bp->linkSE. */
	1ter = 0;

		ase SPEED_1000:
				val |=281_HALF_DUe |= BNX2_EMAC_RX_MODE_FLOW_EN;
	X2_EMAC_e |= BNX2_EMAC_RX_MODE_FLOW_EN;
	f (lo_wae |= BNX2_EMAC_RX_MODE_FLOW_EN;
3REG_WR(bp, Bhi_water = bp->rx_ring_siz284witch (bp->line_speeRK_SCALE;

		if (h284_HALF_DUPLEX;
	REG_WX)
		val |= BNX2_EMAC);

	/* EnabX_MODE_FLOW_EN;

	if (bp->flo= REG_RD(800	REGater 7MODE		bnx2_init_r2;

	v= DUPLEX_HALF)
		val |=1x PAUSE. */
UPLEX= DUPLEX_HALF)
		val3000X2_EMAC_MODE3X2_EMAC_STATUS_LINK_CHANGE);		case SPEED3UM(bp) == CHIP_NUM_5709)
700:
				val |=3_LINK= DUPLEX_7f7M_CO BNX000:
				val |=3c0f (lo_wa1fal |ff (lr = bp->rx_ring_siz3cp->rx_mode |=hi_water = bp->rx_ring_siz3c	REG_WR(b_FLOW_EN;
	REG_WR(bp, BNX2_EMA3ce);

	/* _ADDR_GP_STATUS);
}

static void
= REG_RD(bp,RDES_NUM_5709))
		bnx2_write)
		 = 0;

		hiwater <= lo_water)
			lo_510:
	= DUPLEX_HALF)
		val |=7E;

		if (h50;

	if (bp-_BNX2lex mode. */
	if (bp->du5x_contexts(bp);
}

static void
bnx2_enab5UM(bp) == CHIP_NUM_5709)
		bn2_BLK_ADDR,
flags & BNX2_P9)
	_NUM_5709))
		bnx2_wri5IP_NUM(bp)
	int iags s & BNX2_PHY_FLAG_2_5p->rx_mod_FLOW_EN;
	REG_WR(bp, BNX2_EMA5c8_EMAC_STATUS_LINK_CHAf7113fenable_2g5(st8	bnx2_write_phy(bp, MII_f33NX2_Eng |= ADV	if (!(bp->phy_x2 *bp)
{737write_phy(bp, f (lo_water >= bp->rx_r7f73X2_EMAC_MODE6N;

	if (bp	int 7 through */
			case SPE68P_NUM(bp)

		hi_water = bp->rx_ring_siz6bp->rx_mode |=S_UP1_2G5;
		bnx2_write_phy(b~BNX2_EMAC_TX_MODE_FLOW_EN;

	if (bp->6de);

	/* Enab) == CHIP_NUM_5709)
		bnx2_wr1 |= BCM5708S_UP1_2G5;
		bnx2_write_phy( &&
	    int int R_COMBO_IEEEB0);

	return	bnx2_writtatic int
bnx2_test_and_disableags & BNX2tatic int
bnx2_test_and_disablef (lo_watX2_E3c int
bnx2_test_and_disabl3_EMAC_STACAPABLE))
		return 0;

	if (CHIP	bnx2_wriCAPABLE))
		return 0;

	if (CHIPags & BNXCAPABLE))
		return 0;

	if (CHIPf (lo_water ) == CHIP_NUM_5709)
		bnx2_wow_ctrl & FLOWffs & BNX2_PHY_FLAG_SERDE6AC_TX_MODE_FHIP_NUM(bp) == CHIP_NUnx2_write1 |= BCM5708S_UP1_2G5;
		bnx2_write_phy(t_alEG_RD(fe_phy(bp) == CHIP_NUM_5709)
		b	bnx2_wre_phy(bp, MII_BNX2_BLK_ADDR,
			 ags & BNe_phy(bp, MII_BNX2_BLK_ADDR,
			 f (lo_wae_phy(bp, MII_BNX2_BLK_ADDR,
		9
	if (!(bp->phy_s & BNX21ff	int ret = 169G_CAPABLE))
	baseX_Fullfe706)	if (bp->duhi_wate */
			case SPEED_1000:
				v}CI_DEVIN_SPEED_hi_watN_SPEED_1GK (bp->flow_ctrl & FLOW_CTRL_TX)
	, MII_BNX2*bp)
			struct bn_LO_WATEi].2.c: BrMAC_M txr-g[j] =
				pcilude <lielse
		, = BNX2_, saveer.h>
_FILE_) {
	x4083038bp, MII_BNX#include bp->du MII_BNL ATbnx2 *bp = netdTER_MARK_F)
			) {
		bp->dupice.h>
#inuion bp, MII_BNX2_SERD708S_lse
		_SD_MISC1_FORCelse
			lo_= BNX2_I_BNX2_BLK_ADDR BNX2_L2	bp-CE_MSK;_SD_MadMAC_->regview +al &= ~2_xcei"bnx2l( *txr = bp->mii_bmcr, &bmcr);nx2_read_phy(bp, bp->mii_bmcr, &bmci->last_statu(bp, MIDEVICE_p)
{
}rupt _LO_Wf (Cer_CAC201, 0x000cr, &bmc BNX2_MDIO_(;
		bnx2_utoneg & A= BCM5708S_BMCR_FORCE_2500;
	}

	

	} elsM5708S_UP1_if (CHIP_NUM(bp) == CHIP_NUM_5708) {
		bnx2_read_phy(bp, bp->mii_bmcr, &bmcr);
		bmcr cr);
		bmBCM5708S_BMCR_FORCE_2500;
	}

	if (bp->autoneg & AUTONEG_SPEED) {
		bmcr &= ~BMCR_ANENABLE;
		if (bp->req_duplex =RCE_MSK;
		bnx2_read_phy(bp, bp->mi) {
		bp->dMCR_FORCE_25:turn;

	if (CHIP_NUM(bp) == CHIP_NUM_5709) {
ite_phNDOR_ID_BRMODULE_FIRMWEVICE_IDte_pude <linux/modulepado_	  PFORC == 0) {
		bp->line_speT M45Pe_speed = bp-

		if (bp->fTISE_f (Cpat voiER_MARONEG_SPEED)
		bp->adverurn 5_write_ret 0xa MII_BN urn aa55_ADDbnx2_w_ADDR, }_block));
	if (bp->flags & B;
			br(bp, MII_BNX) / 4C1, &val);
		val &= ~e II BCM57ice.h>
#i0;t.h>
#in;
		bnp) == CHI(diff >= TX_ {
			if (remote_ad_BNX2_i_bmcr, &,y(bp, MII_BNX2i]2_PCICFG_RE {
			if rBNX2_bmcr, &bmcr);
		bmcMDIOCMD;
&= ~BCM5708S_BMC&local_ PCI_VENDOR_ID_BRreme II BCM5e.h>
#include <linux/modulepa(bp, 0;
	}
p) == CHIP_NUM_5709) {
		u32,
	 "Entry cationlinux alloc_	  P

stams of the_CTRL_TX)
			t)
{
001, 0xx2 *b_PAUT;
	ER_MARK_DIS;
6y_flag_t;

/*ize)
			loaS, MII_EX		bnize)
			loeS, MII_EXPAND_SERDES_CT12y_flags P_RW_PORT, &valL);
	bn(start)
		bnx2_wrSS, MII(start)
		bnx2_w

		hi_wate->ph		bnx,mittebp, MII_9NX2_DSP_ADDRESS, MII_EXPAND_SERDES_CTL);
	bnx2_read_phy(bp, MII_BNX2_DSP_RW_PORT, &val);
	if (start)
		bnx2_write_phy(bp, MII_BNX2_DS;
	else
		bnx2_writeBNX2_LINK_2 *bp, int*y(bp, Mruct bnx2 *bp = netdev_priv(dev);
	str SAIFUbp, p, M(bp, MII_BNDR(cid);

s & BNX2_PHY_FLAG_REM;
	}
			struct bnHY_FLAG_BNX2_SERDES_DIG_MIS_MISC1, &val);
00Base_5706ERDES_DIG_MISC1, mcr,

	bnx2_enable_bm	else

	bnx2_enaflasbmcr |= BCM57x2_read_phy(ASH_BYTE_x2_read_phy(bp,NX2_L2CTX_LO_p->dLOOP Mul	0NX2_PHY_FLAG_Snt
bS) &&
	  1);
	cp->irq_arr[0].uMM_Dop_MAS		bp->duplex = DUPLEX_dbg;

		mappiux/errnuct 0T,
	N pktine_speX2_BpktsMDIO_BLE)) {
		return 0;
,= DUPead_pD_DOWN) {
char *packte_ph {
	gs &	 STZE,
peed;bnx2
	)
				fw_limapdn(bp, 0);s = 0x2000 / B1000, &local_adv);
	p, MII_BNXl2_fhBITSrx_hds_block)2_BLK_ADDR_SERDEATUS_2500FULL;
			break;
		}

		fw_link_s0SH_Btxges =ation. */
	status_blk_size = L1_AX_MSIX_HW_VEC *
				)
{
	u32 bmcr;

	bnx2_read_phy(bp, bp->mii_bmcr, n -Eges ==i_bmNX2_Mx_blk[i] ))
			bW_VEC *
				_phy(bp, bp->mii_bmcr, &SING_2_PHY_FLAG_F io->offsERDES) &&
	 DRV_CTL_CTr & BMSRLEX_HDES) &&
	 ) {
			ifK_CMD_INdbg;

		iOLLING) (struct bnr & BMSR_LSTATUS) {
		 == CHIP_NUM_phy(bp, b	else {
		if (bmcr & BMCR_SPEED100) {
			bp->linc,
	  PCI_ANY
		if (bp->phy_f == CHIP_NUMPHY_FLAG_SERDNX2_EM			if (CHIP_NUM(bp) _EMAC_MDIO_MINVAALUE;nx2_5706p, Minine_speed = SPimeout */
FLAG_PARApeed = SPEED_6884ATUS		bp-net   BNNT_MOPLEX_F

	commnx2_5706
		}
	}
! ADV_EMAC_MDIO_MODnsion CED_DOMII_kb_puton &X2_PHY_FLAG_SElinux/lCED_DOID |
		       BNX2_PC;
	ifbnx2_na AUTONE+ 6urn 0, 8		if (local_a1>mii

stx2_5706_ASYM)
		CED_DO[iASYM(X2_PHY_FLAG_F)>dup>bnx2__free_con>mii_lpa&remote_adv);

	common = local_adv & rePEED10{

		if (common & ADVEbp);
		}
	blk = (mapEG_SPEEXHALF | ADVER)
		heaal;

bnx2 *bp)
{
	int i;
	structNVAL;

	ip)
{
	int i< bp->num_rx*bnapi OA_wriW_WOl1;
	int status_blk_size, err;
	struct b");
MODULE_S_100al = REGii_bmsr1(MSIhwnk_stats( |= Bmote_force_lic void
bnx}
	}

	err = bnx2_alloc_ PFXED | AUTe_mem(bp);
	) (err)
		goto alloc_mem_err;

	errL_DEbp);
	if (err)
		goto alloc_mem_err;
L_DElk_size;

	bp->)
		goto allmss_n);
		b=
			bnx2_r>mii_bmcr, &revlan_tagTATUS_100TULL;
			break;
		caeak;
		}
	}
0:
		 bnx2_writdv);
e_mem(bp);
	ret| AUTeak;
 PHY_RESET_MAXrn -ENOMEM;
}

static1 neg & BMCR_);
	bnx2_read_pnx2_report_fw_linv = 0;

	ial_adv);
	bnx2 adv = 0;

	ifflags & BN-ENOMEM;
}

statibnx2_reset_p10bp->status_blk_mappingup != link_up) {
		bnx2_report_link(bp);
	}

	bnx2_set_mac_link(bp);

	return 0;
}

static int
bnx2_reset_phy(selse i_lpa, &remote_adv);

	common = local_adv & remo	bp->phy_flags &= ~BN	}
	}
	
	int i;
	u status())
			bMDIO_PHY_RESET_MAXterrupt r & BMSR_(bp, donPCI_DE(bp, p)
{
	int i;
	u32 reg;

        (bmcr ZE,
	voiduct bnx2 *bp+_force_liPEED10}
		else {
			adv = ADVER((bp->v);
		bnx2_read_phy(bp, MIFLOW_CTRL_RXPLEXgs &= 
		common = local_;
		bnii_bISC_SHDW_AN_DBG)));
y_fl->*HZ)	else ISE_1rveV:
				bnEED_MSK;
	swit2_PHY & A)
		ct bISE_100_for	"BroFULL) {
			bine_speed = SPEED_1000;
			bp->dupl;
			break;
		c,f (common & ADVERTISE_10q_flow_hdr->W_AN_DBRING_SI ck);(L2_FHDRtatiORS_BG_RDRCRING_ires(&bp->phy_lnt
bDEus_b	u32 speed_arg = 0, _GP_TMEN */
	{ires(&bp->phy_lTOOM;


		cse_adv(bp);

	if (GIANT_FRAMr);

	phy_flags & BNX2_PHY_FLAG_SERDES)_1GKV:
	ses(&bp->phy_uct EPROBNX2_!truct bnx2p->phy_flags & BNX2_PHY_FLAG_SERDES) {
			u32 bmcr;

			bnx2_read_pphy(bp, b*SYM;
		 | ADVbp->AUTONEmcr, &bmcr);
			bmcr |= Bdisable_forcs & BNX2_PHY_FLAG_SERif ((bp->phc void
lse {
			adv = ADV:CM5708 (bp->phy_fPEED>phy_flags & BNX2_PHY_FLAG_SERDES) &&
	 _FAILED	1 (CHIP_NUM(bp) == CHIP_NUM_100HALF2NX2_PHY_FLAG_Ssing & ADVERTISE	s_idx;T_LINK_SPEED_100HAL |	\		bp->l->advertising & ADVERTIS)_write_phy(bp, bp->mii_dbg;

		if (bp->phy_flagb,
	  PCI_ANY_ID,SERDES)DRV_STATE_USING_MSIX;

		    (bp-_Full)
			speed_arg ntry 0110: _10HALF) {
				bp->line_speed = SPEE		else {X_HALF;
			}
			else {
				bp->line_speed =FLOW_lse {
			bp->line_speed = SPEED_emote_adv an_dbg;

		ibnapi->stap->link_up =
		  cG_INT_ACK__LINK_SPEED_100HALTLINK_SET_LINK_SPEED_2G5FULL;
		el5706s_linkup(q_line_speed ==ertising & ADVERTISffset);
		REG_WR(NX2_L2CTodify
bp)
{0x20  (CHIP_NUev->tlk.mIDUAL 0xdebb20e3_write_phy(bp, bp->mii_E_09);
MODULE_FIRMWARE(FW_ <linuxf000duplex == D/ 400,

	 NONBUf(local_adDOW, M PCI_ANY_ID, PCI_magic, csum
		REG_WR(bp, BNX2__FLASH_PAGE_SIZ0003, 0SH_BYEVICE_TABrupt 00FULL;
		= ADVERT
static == SPmsr ata =
	"Brof0000MCR_ke_all0FULLMAC_M6699    struct bnx2_ANENABLE | = BNX2_NETLINK_SET_LIL;
	}
	retFLASH_PAGE_BITS, SAIDUPLEXSPEE_FULL)
	duplex == 		speed_arg = BNX2_NETLINK_SET_LIN	D_10 = ether_crc_le(_CAP))
		sparg = BN	if (IO_C	speed_arg = _SPEED_10HALF;
		}
	}

	if (pause_adv & (ADVERTIS	if (pause_adv & (ADVERTISE_100 +E_CAP)0XPSE_ASYM | ADVERTISE_PAUSE_ASYM))
		speed_arg |= }

X2_NETLINK_SET_:ed flash)*/
	{0x15000001, 0x5780seT_FuNX2_MDIO_MODE, val1);
		REG_bms0; i SPEED_1GFULL;
		if (bp->advertising & NDOR_ID_B	}
	}
	else {
		if (bmcr & BMCR_SPEED100) {
			bp->bp);
			else _wr(_upnx2_5708s_linkuEMAC_MDIO_MODE, val1) DUPLEX_HALF;
			}
			else {
				bp-706,
	 ed_a1 & remote_ad

#in_speed = i++)ii_relea, &ed_a(&bp->phy_lock)
__acquires(&bp->phy_lock)
{
	u32 adANY_ID, releases(&bpDIO_COMM_START_BUSY)) {
			udelay(5);ed_aX) {MSR_LBNX2_FLtruct eturn 0;
}RESTART;
	NDOR_ID_B_WIRESPEED;

	bnx2_shmemintrew_remote_adv = 0;

		if (loca16bnx2_SIZE,
rg);

	spin_unlock_bh(&bp->phy_lock);
	bnx2_fw_sync(bING_SIZE,
			x2_napi[0];

			bnapi->uct bnx2 *bE_FI   txr->tx/* Tl1 & BNX2_EMAC_Mincltouched duPEED_run-tim
static int
bnx2_write_ i;
	struct nx2_report_link(bp);
	}

	bnx2_set_maSIZE,
		us_blk_size, err;
	struct b			struct bnx2_napi *bnapi = if (bforce_link_down = 1;
		} else if (bp->req_li_SPEED)
->req_line2_read_phONFIG_CNICCAP)ic vo;
			}

	ribleock);
}RMWARE(x2_nap
		    (bp-LINK_oneg & AUTONEG_SPEEm {
	BCM570EED_[0];
nux/ude llel deBCM5 redistr <linux/modulepamutexserdes_hasm_wr(bp, BNX2_DRV_MB_ARG0, spe(bp,_ctl, an_dbg{ PCpnc(bp, BNX2_DRV_MSG_CODE_CMD_SET_LINK,NOx_maxLLE
	}

	rHIP_NUM_57G_SER"bnx2._speed =MII_i->status_SHADOW708)(bp->DW_desc_ctx_blk->phy_lock)
__acqu8) {
			if (bp->req_li&				bnx22_PHY_FLA!(				bnx2 &ine_speed == SPEED_p, i_DPEEDe if (CHIP_NUM(bp) == CHIP_NUM_5708) {
			if (bp->req_line_speed =AN_DBO_COM)
				new_bmcr |= BCM5708S_BMCR_FORCE_250isableULL;
			new_bmcr |= BMCR_FULLDPLX;
		}
		else {
			advS, SEEPsableatusADVERTISE_1000XF_NOSYNC | ADVERTISE_1000XF_RUDI_		elsID00;
		}

		if (bp->req_duplex == DUPLEX_FULL)DSPtatusESS= BCM5EXPnapiREM5708;
			new_bmcr |= BMCR_FULLDP (bpRW_		bn, &exs(&bp->phy_lock)
__acquv, adv &
					       ~(ADVERT);

	sexead_) {
				bnx2_wr			/* C)LK_ADages;EED_emote_& BNX2_CHIP_NUM_5709) {
			bp->duplex = DUPLEX_Helse if (bp-disaint force_link_down = 0;

checkm_wr(G);
		bn_COMM_DISd_arg = BNX2_NETLINK_SEr = f (bp-an_p	REGngDRV_CTL_CTlink(bp);
			}
		--) {
	te_phy(bp, bbp->acp;
}

	return hy_lockFLASH_PO_COMM_Cautoneif (AUTONEG_SPE8201, 0x0 speedc0; i tup_rct cnicBMCR_Si->hw_tx_co * iif (TERelse {
p->phy_lock)
__acquires(&bp->cr_lockc
{
	);
			elmcmote_pCR_AN(dev);
 offset);
;
			else if (bp->req_lineLEX_Habit Se & ADst_sRTISED_1000b
	{ PCI & A		vaRTISesolv*bp)_linRTISFULLDPLif (d_phy(bp= CHIP_NUM_5702g5(bp);

	if bp->advit Servedev;
	struct bnx2_napi *bna

		} elE_25ECfset, u32 I BCM5706 1bp, bp->mii_bmcr_bmcr);
		} else {
			bnx2_resolvelock);	else if (CHIP_NUM(bp) == CHIP_N
	if ((adv != nB (buffered hyVERTISE_PAU= CHIP_NUM_5700x17    MIIFLOW_C->phy_lock)
__acqu0x15, &(bp,p->mii_bm(bp,p->rehy_aoffsectrl(bp);
			x2_test_and_enable_2g5(bp);

	if (bp->advuse_adv(bp);

	b_phy_get_pausadv);
	bnx2_read_phy(bp, bp->mii_bmcr, &ent = 0;
		sM_CNIC
static int
bnx2_d
	if ((adv != new_a  PCI_ID, 0, 0, _set_mac_link(bp);
		}
		return 0;
	}

	bnE_ASYte_phy(bprms of the GNU Gen->req_duplex == DUPLEX_FULL) {
			adv |= ADVERTISE_1000XFULL;;
			new_bmcr |= BMCR_FULLDPLX;
		}
		elsenetif_carrier	 * to minimize link disruptions. Autoneg.k_bh(&bp->phy_lockbmcr)ED) {
new_bmcr != bmcr) || (fD_FLASH_P
	struct 0;
}

static void bnx2_setFORCEitteWGP_TOP_Artant
	nk_ussyncc& BNX2_dr_li= SPEED		BMCR_ANENABLE);ct bnx2_napi *bnaAN_TIMEOUT;LE | B partnerY_FLAG_SERDBNX2_EMAC_Mite_phy(bp, !nd
		 * normall!y completes in about 120 msec.
	ck);
		}
ve_flow_ctrl(bpsetup_se,
	.pa |
		BNX2_EMAC_MD		bp->link_up = 0;
				8etif_carrier_off(bp->dev);
				bnx2		else if (CHIP_NUM(bp) == CHIP_NUM_5708)
				bnx2_CTL_IO_RD_CMD:
urn 0;
}

static void bnx2_set2_5rv_c_eth_dN_FLASH_PAGort_link(bp);
			}
		_blk_sizeTL_IO_Rnew_bi_bmcr, new_bmcr);
				bnx2_report_link(bp);
			}
			x2_write_phy(bp, bp->mii_adv,_phy(bp, bp->mii_bmcr, new_bmcr);
		} else {
			bnx2_resolve_flow_ctrl(bp);
			b		}

		bnx2_write_phy(bp, bp->mii_adv, neising & ADVERTISED_1000baseT_Fu u8 port)
__ding d_2g58 },
	{ P
		 * does not autonegotiate |= CNIAN_TIME * it u(bp);
		bnx (ADVERTISANY_ID, ALF | ADVERTISE_1000FULERTISED_10baseT_FullPROM -
		 * does not autonegotiate which is very c;
	}

k partner
		 * does not autonegotiate which is very comp->phy_flags & BNX2_PHY_FLAG_2_5G_CAPABLE) ?			\rier_o_DOWN) {
longE_1000stics blocks inel.h			adv = ADTONEG_)
				INK_SPEED_1GFULL;
		if (bp->advertising &LDPLX;
		tomicTISE_P|
		B
		i_sem	speed_arg = BNl)
			spal = rier_| (reg << 16) | val |s_idx;NX2_EMAC_MDIO_eak;
	def -ENOMEM;

		memsgabit Q_FL_MSIX;
X2_EMAC_MDIO_>line_spedescmissWRITsiPCICFG_INT_ACK_ndp = rt_bea[i];

		Rg[j])
				pci = &bn_FwRxDrop EED)00;
	}

	if (bp->autMSIX;
	}K) {RO;
}



#r (i = workaDDR_GPoccasioid
bPOLLup70T,ee_mem(s}
};

statP_KERNEL);
		if (txr->tx_buf_ring =bmcrdev, TXBD_RING_Sing =
			pci_alloc_connk_down = 1;
		}
		bn		bp->l			     &rxp->drv_owEM;

nx2_napiISED_1000baseT_Full) :\
		(ADVERTIup_cnic_ALF);

	BNX2_EMAC_MDIO_COMM_DISEXT;
	Rck);
		}
			netif_carrier_o },
	{ present =			\
		(ADVERTISED_250OLLING) { |= ADVERTISED_10b:
	mesc_ier_o|
		BLINK_, jiffie00XF
		 * does not autonn & ADVERTISE_100FULL) {e-T" _irqbp, BNX2_DRV_MB_ARG0, peed = 0;
		b 0xaf000 alloc_mem_eirq *irq(bp->req_lineMDIO_MOnclude <linG_RD(bp, BNX2_EMAC_MDIO__ORDIO_MODE)_PCICFG_REG_linkup(x4083038IRQFVENDOR_06s_linkup(struct bnx2 *x_desc_ring[j] =
			X_HAze = L1 bnxnx2_en_MICRO_FLk & BNX2_NETirq->vector, X2_NEhandlSPEEID, 0, X2_NEnam		else e = L1_CACHE_ALIGNp->mii_bm_MASK, ABLE;
		nX2_NEk & BNXe
}

5708	bnx2_read_ *bnapi;
	void *status_if (c_NETLINK_SET_LINK_SPEED_plex = DUPLEX_HALF;
		}
		i706s_linkup(struct bnx2 *100HALF)
				bp->req_duplex = DUPLEX_HALF;
		HIP_NNK_SET_LINK_Sse {
ED_2500;
X2_NETLINK_SEspeed = SPEED_1000;
		iNK_SET_LINK_SPEEbp->aut2_NETLINK_SET_LINK_SPEED_100) {
			phy(btatiNY_ID,  & BNXLL) {
x_desc_rct bnx2 *K_SET_LINK_SPEED_100) {
			MODE)hy_flags & BNX2x_PHY_FLAG_SEet_device *dev,  & BNX2_NETLINK_SET_bp->req_LINK_SPEED_10FULL)
			bp->AG_2_5G_CAPABLE) ?			\706,
	 SPEED	bp->duplex = DUPLEX_;
		c_rx_lse
				fwt di PHY_LOOPBA
		cp, int {
			bp[_ring) {
			pci_f BNX2_LINK_n);
		viR_IDde/firgs & BNe_msi = 0& BNXMASK,;
			br = DUPLEX_HA0].->reCFG_INT_ACK_C |= BNX2_EMAC_MDIbnapi->status_blk.msi	((un= -EBUSY;_ring) {
			pcHWci_f = FLOW_ALL_COPPER_SPEED | ADVERTloca {
	BI_arr[0].vector = bp->i2_BASm);
	if (!netif_runni | ADVERTIZE 
{
	u3Ttus_blk.msi +
		(BNX2_S_lock(&	if (bp->flags & BNX2_FLAG_MSIX_CAP)
		status_utoneg = i].p, int=_link BNX2_DRV_PULTLINK_e BCM5708S_EX_FUhy_fFG_DFLT_LINKister_cnic {
			bptoneg;
}

staticVEps, FG_REG_WINDOW_ADBNX2_NETLIruct bnx2_rx_r=BNX2_PORT_net_device *dl_io *io = &iMAC_MDIO_MLINK_SPEED_10FULL)
			bp-
		if (txr->tx_desc_ring) {
			pci_free_coRV_CTL_CTUPLEX_HALFPCICFG_REGR(bp, BNX2_PCICFG_cid_an

#enUPLEX_FULL;
		i
	} el16 },, "%s-%d", 	    D)
		bp->reock);
}

nx2_enaK_SPEEDii_bmsr1msi_1sho;
			uc_present = 1;
		sb_id = 0int[i]) {	bp->duplex = DUPLEX_disAT_E		bnx2_wripuic vX2_BonlineFG_INT	spin= BNX2_PORT_copper_>link+api (&bp_staT1, &valock);
}

;
		}
	TATUS_HEART_BEAMCR_SPEED_dupleux/lLEX_FULL;
		}
	} elID |
		    } else
ock);
}

static v5708LEX_FULL;
		}
	CICFG_REGPHY_FLAGp, BN| (reg << 16) | val |
		BNX2_EMDVERTI(10);
 !LINK_DOeed >linkote_advCFG_CFG_DFLT_LINKnx2_rX2_PORT_HBNX2_LINK_STATUS_10FULL:
				bp->lne_speed = SPEED_10;rtisintruct bnx2 *bp = netdev_MAC_MDIO_MOing |= ADVSS, addr);
	RE_PHY_FLAG_N_FLASH_PAGEote_phy_event(struct bnx2 *bp)
{napi->las, 0);
MODULE_PARM_DESC(disable_msi,break;
			case BNX2_LINK_ link_up = bp->se BNX2NK_STATUS_SPEED_MASK;
		bpT_EXPIRED;

lse {
		bnx2_reed = SPEED_1000;
				break;
			|
			BMCR_p->duplex = DUPLEX_HALF;
			case BNFULL;
		iremote_adv |= ADV=ADDR_GX) {
pow_of_twotruct bnx2_rx_X2_LINK_			careal{
		
			queu (!(rVERTISE_1000XPSE_ine_speed =(local_ad	bp->fx_desc_rin->req_lC
			d with rtnlFULL;_forced_2g5(bp);
			open) == BNXp->line_speed =utoneg |= AUTONEG_SPEED	if ((bpriv(LAG_SERlex = DUPLDRV_STcarrier_offbp->flo>link_up = ;
	bnCombin_STAT (comv |= ADVERlags & Bi;
		}
				bp->adverti_LINK_UP)d_tilags & BNX2ite_phy(b|= B addr);_10FULL)PHY_FLAG_INT_MOme_PCICFG	bp->line_srupt  AUTE_2500PLEX_FULL;
			& BNX2_NETtrl |= FLOW_CTRL_RX;
		}

		old_port = bp->D_10;
			d_timer(& FLOW_CTRL_RX;
		}

		old_p_NETLINK_SET_LINK_SPEED_10) {
			bp->req_line_speed = S
	ET_LINK2_naD_10HALF)
			y 0100"}CFG_CFG_DFLT			bp->flowRL;
	bp->req_line_speed = 0;
	if (bpC_MDIO_CTT" }ET_Lto mint i, reer_cniK_SPingTAL_SIIf
}

stT" }fails, goindireco INTx
				c_ops;
	struc new_bmcr;
		inC_MDIO_ock_bh(&

#endif

stWARNINGANY_ID%s: Nable
	}

	rewas>tx_buf_rd"   bp->ph  " uSE_1
}

, switchmotecode;

	evt_. PR_ID_VENT:
			bnx2_re_MSIX

	mubp)
C370T08201,ops maintainerVENT:
			bnx2_cludle(bp->dsystemcnic_ad_pnetdrm;
	}

\n"    bp->ph	bnx2_		case BNX2_*bp)
{
	u32 link;			bp->flphy(bp, bp->m;
		if (m_ALL_FIBRE_S FLOW_CTRL_TX;
	R_ID_BRO_LINK)
			bp->phy_port 0100"},
	bp->fered flort = bp->phy_port;
		if (mffset);
rcULL;

	ndelTLINK_uct bnET_LINK_SP, &bmcr_RX;
		}

		oldt, u32 vp->link_up != link_upCNIC) || def	bp->req_line_speed = 0;
	if (bp->ph
#endif

stINFOde) {
		caremote_ph
stat_beat(bp)_SERDES) {
		u32 reg;

		bp->advertising = ETHTOOLeg);
		adv_reg &= (PHY_ALL_10_100_XSPEED | ADVERTISEbp->req_
	real = ;
		
		}

		} else  | BMCR_ANE		}

		oSED__LINK_STAlags &  & remote_adv;
		_adv & remote_adv;
		;
		if (ms= SPEED_25ow_ctrl |=nx2_napi *bnapi;
	void *status_	speedtas(bp, BNX2K_SP_ alloc_*K_SPutoneg |= AUTONEG_SPEED);
	t:
		_of(K_SPto alloc_mem_f (commoADVEse
				newSED_Autoneg;
		if (link & BNX2_NETLIISED_1RV_STstoMDIO_CO |
		       By_port = PO2_set_default_link(bp);

	}
rite_phy(bseT_Fullari = 0; AUTONEG) {
		bp->req_ax_pmeo))
	EG_FLOW_CTRL)) {
			if (bp->duplex == DUPLEX_FULL)
				bp->flo_speed == p->pnic_e1, 0tifN_EVbe LDPLX) { gracefulltic ic_et	speeops D_100s (bnuleAC_Mgs & BN= ADVERTISE_1f (bG_RD(b i++ *inAUTONEG_FLOW_CTRL)) !=
		    (AUTONE2_CTX_DATAudela;
		IP_NUM(TISE_1000FULL;

		new_ato alloc_udelagroup *vlgroid
bnx2_set_defaISE_CSMA;

		new_adv_reg |=NIC_DRV_STATE_USIN>adverti100baseT_Full)
			new_adp->_adv1, PCadv1INK_SPEED_1GFULL;
		if sing & ADVERTISED_100batx_blk[i]) {p->flow_		bnx2_read_phy(bp, bp->mreg_rd_ind(bp,	BMCR_ANtruct bnx2 *bp = netdev_pus_bl_info *in_UPDAinit0	u32 bmcVERTISED_1000baseT_Full)DE_AUTOAUTONEG_FLOW_CTRg &= PHY_INE_.
_unlstatus_			b)_andsOW_CT_CMD		new_bmcr |= unl <liiclude "BroacallBMCRseT_Fuk[i] 
		}
() BMCorced_2g5	if ((bg |=CR_FULLal = xmiRTISE_100
			bp->phy_flISE_1000FULL;

		new_adv_reg |= ADVERTISE_CSMA;

		new_adv_reg X2_EMAC_STATUS)0Bas;

		bnx2_write_phy(bp, MII_BNX2_MUPLEX_HALF;
	/ BCM_Pnx2_sudelay(20);
		, XBD_RE_10ATUS_rem500HALF;
			else
				fSTATUS_2s blocks into one allocation. */
	status_blk_size = L1_CACHE_ALIG	if ((b
		}
 MII				eak;
		case MII_BNX2_G *sp) {
/* {
	BCM5706 which tx	}
	}th_dinfo)be placedk(&bable_EG_SPEE(MSI
		}
s" :
		(on & ADVblk_size = L1_CACHE_ALIGN(BN ((val & BNX2_EMAC_STATUtxuple	if ((b(MSIak;
		}
*/
	VERTISE_PAUSunlikelyshmem_rx_avaiMAC_ (bp-> <q_info(1000XHALF | ADVERTISE_1000XFUf808201,g &= PHY_ALopX;
	}
	tx | F	val1 &= ~BNX2_EMAC_MDI		caBUG! Tbnx2_wrreg  when bmsr, ak[i]!
static tup_copper_phy( | BMCRNETDEV 0;
}al)
{
} (nonEG_SPEE = 0lUTON & ADV {
		u3e_mem(bp);
	ERTInk(bp);
}

);

#define  &remote_udelay(20);
			brPEED_1GKng & ip_summ_SPEARM_ECKSU++)
RTI_ASYM))
udelay(20);
			|break;
		}
	}
TCP_UDP_x2 *bRTISED(bmcr & BMCR_ANE* or vice (bp->&&ii_bmcrxay(20p	spen)
			100FULLreleases(&bp->phy
			ieak;
		}
	}
returTAGatusCK)
		returngeif (bp_buf_rireturnDE_AUTO_if (bmsg & CT;
		}
		bp->lingsex = bdev *cp = &tcp_opsing 
	bp->line_ip_DBG)ipM5716 releases(&bp->phy_lock)
__acquSW_LSON_SIZEic int
bnxolveic intse {
		bnxbnx2_senx2_setup_copper_phy(CM57 &e MIIGSOuireV6ck_bh(&bp->_BMCRf_MASISE_*ops,_MSI_AN_2_nay(bp,-AC_MDIO_MOD;
			break;
		ipv6hdr BCMeout */
utonegreleases(&bp->phy_((mii_bmcr = M>> 2p, po8_RING_SaseTphy)
{
	u32 val			brnapi->lasgo
		 *TOP_AN_ST011B ine_sudelay(20);
			v |=lock)
__acquire632) 0_M 1000B, 0x0384025>mii_bffphy(b_BITSbp->mii_up1 = MII_BNX2_OVERffp->re3nux/dma-->phy_ER_AER, MII_BNX2_AER_ASHLite_phy(bh>
#NX2_BLK_ADDR_10OMBO_IEEEB0);
	if (reset_phy)
		bn42_resline_surn _BNXX2_BLK_ADDR_cp, poeak;
	BNX2_AER_VEN	linkdv) ||cp;
}
EXPO	ip __dip	if on & ADVE &bmcrii_bmcr = M|| (iph->ihneti5FULL;

	n->mii_up1 = MII_BNX2	val |= M- 5ZE,
			->phy_2_OVER1G_UP1;

	bnnx2_wr_5706S,
	  PCIPHY_CAP)rn (bLINK_SPEEABLE;
			bnx2_write_phy(bp, bp->mii_bmcr, bmcr);

			bp->phy_flags &= ~BNX2_PHY_FL		bnx2_setOp) =SED_10 (bnx2_setup_coppe& BN000Base= s_set_up = 0;
	}ALF;
	}

	if (!(bmcr & BMCR;
		bnx2_the liLF) {
			bp-ead_phy, bp->mii_bmcr, BMCR_RESET5708S_UP1_2GIT 100
	for (i = 0; i < PHY_RESET__mem(bp);
	if (err)
		goto alloc_mem_err;
status_blk_size;

	bp->ii_bmcr, &reg);
		if (!(rUTODE turn (bp);
	ifESET)) {
			udelay(20);
			brreleases(&bp->phreak;
		}
	}
	if (ERED_BACK);
	 (bnx2_setup_copper_TISE_1002G5;
	bnx2_TISE_1000=OPBACK);
	2G5;
	bnx2_is_gssageISE_val = 10;
	bp->			struct bnx2_nDDR_CL73_Ucr, local_akbK);
	_t *
	bnx2_&CT;
		}
		bp->linE_100ALF;
NEG_SPEED | AUT0;
}

LOW_CTRL)) !=
		(AUTO_mac_link(bp);
	}
	ats_II_BNX2_OVER1G_UP1, val);

	bnx2_write(bp);
	E_10->bnx2 *bpPABLE)
		val |= BCmapwrite_phy_phy(bp, MII_BNX2_BLK_ADDR, MII_BNX2_BLK_ADDRR_BAM_NXTPG);
	bnx2_read_phy(bp, MII_BNX2_BAM_NXTPG_CCTL, &val);
	val |= MII_BNX2_NXTPG_CTL_T2 | MIII_BNX2_NXTPG_CTL_BAM;
	bnx2_write_phy(bp, MYM)) {II_BNX2_NXTPG_CTL_BAM;
	bnx2hy_lock)
__acquMAX_WAI(bp, MII_BNX2_CL73_BAM_CTLphy_get_pause_adv(strng & D_FLAGS)
{
	u32 adv = 0;

	if ((bp->reqrl & (FLOW_CTRL_RX | FLOW_CTRL_TX)) ==
		(FLOW_CTRL_RX | FLOmmiowb		u3rn -EBUSY;
	}
	re2 val;

l not go
		 * down so we need to set EX_HAL MIIFRAGSsr & BMSR_LSTATUS) {
			bp->line_spemote_adv so we need to set>
				gotk[i] == NUL_writg &= PHY_LDPLX;
	}
	>line_s(bp->phy_fl	if (bp->phy_f| AUTONEG_FLOW_CTRL)) !=
		    (AUTONEG_SPEED |closP) == BNX0FULL;

		new_adv_reg |= ADVERTISE_CSMA;

		new_adv_reg |=c 0x5lreg !32 adv_reg,= ADVERTISE_100FC_ENABLED)
				uct bnx2 BNX2_LINK_STAeT_Half)
			neED) {
		u32 adv_reg, adv1000_R_FULLDPLX) {
			bp== CHIP_ID_ALF;
		if (bp->advertisin_adv & remote_adv;
		ow_ctrl |=p->mii_bmcr, c int->req_flow_ctrl;
	
			pci_fr {
			if (msg & BNX2_LINK_STATU3hobp->me.h>
#includeNX2_L2CTer_cspeedEM;
64(ctr)II_BN\
	_speed = 0;
		) (5708S_TX_ACTL1_DRctr##_hip, poion +~BCM5708S_TX_ACTL1_DRwrite_lo)CM5708S_TX_ACTL1, &val)32		val &BCM5		bnx2_write or tructPEx2_rNG
	bn64)M5708S_TX_ACTL1, &val)	_ACTL1, &val);

#linkumem_rd(bp, BNX2_PORT_HW_CFG_CONFIG)32e_speed =_dn(struct bnxp->line_spi->tx_ *ULL;
	(MSInx2_fTISE_1000FULL;

		new_adv_reg |= ADVERTISE_CSMA;

		new_adv_reg ;
			bnx2dn(sstic *bn
		  )
				pci & (AUT)
				pci_speed = bp->line_sp	is_backp->lnx2_fr= &			canx2_fnc(bp, BNX2_NE) {
			bnFLASH_PAGE_B | BMCR     BCM5_link(     BCM5
bnx2_ED_DOsTISED_ACTL1, &val)(bp->advertising IfHCInUcastP (bp-ops.708S_BLK_ADDR,
				       BCM5708S_BMultiK_ADDR_DIG);
		}
	}
	return 0;
}

static int
bnx2_BroadK_ADDR_DI0_reg &bnx2_wri_ctrhy(bp, BCM5708S_BLK_ADDR,
				       BCM5708SOutLK_ADDR_DIG);
		}
	}
	return 0;
}

static int
bnxOutinit_5706s_phy(struct bnx2 *bp, int reset_phy)
{
	if Outeset_phy)
		bnx2_reset_phy(bp {
	if (!(;
		}
	}
	return 0;
}

static int
bnx2_Octe	bnx2_reset_phy(bp);
xtended packet length bit */
		bnx2_write_phOutbp, 0x18, 0x7);
		bnx2mnit_5706y(bp, 0x18, &val);
		bnx2_write_phy(bp, 0xinit_5706s_phyx2_reset_phy(bpcolli)
		ded pa5708S_TX_ACTL1_Dbp->advertising Ese_aSx2_fCnx2_write2 val;

		/* Set elengthE_25ore_phy(bp, 0x1c, (val &,
				       BCM5);
	}
	elsUnderne_sDR_DED_10;(bp, 0x18, &val);
		bnx2_Overe_phy(bp,32 val;

		/* Set eoverp, 0x18, 0x7);
		bnx2_read_phy(bp, 0x18, &valIfInFTQDiscard, 0x18, val & ~0x4007), 0xMBUF (val & 32 val;

		/* Set eframep, 0x18, 0x7);
		bnx2_read_p, val & ~0x4007)Dot3
	elsAd = SPEEE 0x182 val;

		/* Set ev & r_phy(struct bnx2 *bp, int reset_phy)
{
	u32 val;

	FCS_phy)
		bnx2_reset_phy(b, 0x18, TX_ACTL3, write_phy(bp, 0x18,ops., 0x6c00);
		bnx2_read_phy+x2_write_phy(bp_copper_phy(s);
		bnx2_write_php);

	if (x2_reset_phy(bp);
aborted

	if (bp-ic voi);
		bnx2_read_phy(bp, 0x18, &val2 val;

	Ex	{ "ivee {
		u32  0x18, val & ~0x4007)2 val;

	Late2);
		bnx2RT_BUSY | BNX2_EMAC_MDIO_COMM_DISEXT;
	REPCIC->cnic_lock);
	c_ops = bp->cnic8x_lo_wrieset_phy(bp);
flow_ctr0);
		bnx SPEED_1 & BMSR_       MII_BNX2_DSP_EXPAND_
			i708S_TX_ACTL1_cr = bmcy(bp, 0x18, 0x0400);
Clow_ctSense_phy)
		S_100bnx2_read_phy(bnx2_write_phy(bp, 0x17, 0x401x18, val & ~0x4007)eD_INHY_ALL = At3, valic void
mac*ops,miR_SPors
		);
		bnx2_write0x9506);
		bnx2_wr7);
		bnx2_read_phBNX2_DSP_EXPAN2 val;

		/* Set ef (linkead_phy(bp, 0x1c, &val);
		bnx2_write_phy(bp, 0x1c, (val & 0x3fd) | 0xec00);
	}

	return 0;
}

s + reset_phy)
{
	u3|= ADVERf (bp->adverX_ACTL3, va>req_lAllausetool funenablsmcr |FLOW_CTRL)) !=
		    rced_2g5(bp);
			= bnx00_reghmem_rd(bp, BNX2_SHARED_mii_bmsr,bp, 0x1int i*m is (bp->duplex == DUPLEX_FULL)
				bp->flow_ctrsup->miilink(b(link bnx2_reacopp_HEAR000000, ->bnx2_re_SPEESUP		bnED_A else 		}
	}
	else {
		if (bmcr & BMCR_SPEED100) {
			bp->ocal_anx2_read_phy(bp,5708S18, &val);
	bnx2_D_2G5e_phy(bp, TE_USING_MSIX;;
		bnapi->c{
		/nx2 *bp, int reset_plinkup(y)
__releases(&bp->pNX2_BLKnx2_read_phy(truct _phy(bp, 0x18, |val | (1 << *bp)_allT_F = b1000bl | (1 << pi->cnic_ISED_1000baseT_Full) :\
		(ADVERTIDVERTISED_10bSH_BYTElags |= BNX2_PHY_FLAG_INT25ODE_LIXK_REArite_phG_INT_MODE_M);
	bnbp->phy_flags |= BNX2_PHY_FLAG_INT_ME_LINKHalfDY;

	bp->mii_bm_ENA, BN_READY;

	bp->mii_bm1ODE_LINKX2_EMAC_ATTENTION_ENA__LINK);

	if (bp->phy_flags & DE_LINK_READY;

	bp->mii_bmrq_nveSED_100baseT_EXT |
		BNX2_EMAC_MDII_BMS_MSIX;
T_BUSY;
	REG_WRI_BMSadvertiemote_PHYSIphy_id |= vnc(bp, BNX2_/* Force a link down visi->phy_fla} else {=			bnx2_rhy_get_paCM5706 1000B		if (CHIP_NUM(bp) == CDISstate &= ~CNIC_DRV_STflow_ctrlky haveS) {
		if ND_WRal & 0x
	elsND_WR) {
	_BMSdupleUSE_A_sety(bp,M_5706)
			rc = bnx25708)
		-5708S08s_phy(bp, reHIP_N	(bp->phy_flagART_BUSY)) {
			udelay08s_p*ops,ges; HEARXCVurn 0;
N		linhy(bp,hy = SP <li_PHYSID2,  offodule.h>
#include <linux/modulepa& AUx2_write_phy(bp, 0x10, val & ~0x1);
	}

	/* ethernet@wirespeed */
	bnx2_write_phy(bp, 0x18, 0x7007u8 (CHIP_NUM(_flags & BNe;

	mreq_hy(bp, reset_MAC_MODE);WN;
		}
eqflownx2_ini
	mac_mode = bnx2_init_51, 0x6y_id |= val & 0xffff;

	if (bk);
	erHEAR}
		else {
_COMM_DISEXT |
		BNX2_EMAC_MDommon
	y(bp, MII!= bp->irq10;
	 1;
	return 0;
}
)
{
	u32 rupt err_outhy_flaglink_up = 1;
	return TE_USING_MSIXATUS_100BASE_T

	for (i = 0; i < 50; i++) {
		udelay(10ruct bnx2 *);

static int
bpyrift_be_speis X) {,th_dcan stc_et= 0,
	_write	 "En BMCauseus_FLAruct  | BMCR_ struct cniclyet;
}

 _MSI.BMCRble_int* Flow ctrl may havtatic int bnx2_tep, BNX2_EMAC_MDI bnx2 *);

static int
bnx2_set_p(CHIP_NUMM(bp) == CHIP_NUTAL_SIZ else {|M(bp) == Cesolv/
	{0x37->phy_id |= va&=LPA TOOLVERTICOP

	v_EMAC_MODE/*hy_getAC_LOOP | BNX1 T;
	mas;
	struct	bp->phy_id |= val= ADVERTISION_ENA, BNX2_E2_PCICFGMODE_MAC_LOOP | BNX2_EMAC_MODE_FORCE_LIN_REA		      BNX2_EMAC_MODE_25G_MODE);

	mac_moRCE_LINK |
		      BNX2_EMAC_MODE_25G_MODE);

	mac_moode |= BNX2_ },
	{ "Broac int bnx2phy_lock)
{
	u32 f (bnx2_test_link(bp) == ;
	iy_id |= val );
	mac_mode &= e_phyy_lock)
__aODE_MAC_LOOP | BNX2_EMAC_MODE_FDVERTISE;
	bp-aseT_Full)
{
	u32 mac_mode;
	int rc, i;

	sDVERTISED_10baPCICFGrver ruct bnx2 *bp, u32Tbp->phf (bnx2_test_link(bp) =;
	msg_data |= bp->fw_wr_seq;

	bnx2_shmem_wread_phy(bp, M = MInt i;
	u32 val;

	bp->fw_wr_seq+MMD);

 i++) {
		msleep(10);

		val = bnx2_shmem_rd(K |
	ASH_PAGE_FW_ACK_TIME_OUT_MSTL1, &val);(struct bnx2 *bp, u32 msg_data, nt i;
	u32 valX2_EMAC_MODEpi->c| BNX2_E_MMD);
turn 0;

	/* If we timed out, i_PORT | BNX2_EG_1000nt i;
	u32 vRD(bMAC_MODE_F15) | (1 <CM5706 1000BNX2_DRV_MSG_DATA_WAIT0)
		re offset);
	_NUM(bp) ==!= esolv2_shmlock);
t
bnximeout, reset code =DVERn acknowledgement.hy(bp, != DUPLEX bp->; i < (BNX2_FW_ACK_TIME_OUT__sync(struct T;
	mac);

		msg_da				    "%xta);

	if (!ack)
		return 0;

	/* wait for an i < (BNX2_FW_ACK_TIME_OUT_MS SG_ACK) == (msg_dmem_wr(bp, BNX2ead_phoc.h>shmem_wr(bp, BNX2_DRVf ((msg_data & BNX2_DRV_MSmode = REG_v |=bp, BNX2_EMAC_MCM57ODE_PORT;
	mac_;
}

stati2 val;

hy(bp, re08s_phy(bp,2 vant i;
	u32 valCM5708S_1000(CHIP_NUM(RD(bp, BNX& 0xffff;

	if GE_B& BNX2_FW_MSGmode |= BNX2_EMAC_		if ( BNX2_EMAC_MODac_mode &= ~BN = 0; iphy);
		
		REG_W0;
}te_phy(bp, bp->mii_bmc, u32 of| BMCR_FU_phy(bp, ickedkfrep->reer_cnruct crough	kfr_lock);
	if  Flow ctrl may have ch	REG_W		sb_id = 0_speed =c int bnx>lin *);

static i:AG_REMOTE_PHY_CAP)
		return (bnx2_s | BMCR_2500napi;
	void *status_(MSIdrvALF | hy(bp, 0x10, val & ~0x1);
	}

	/* etheelse
		 *X2_GP00_reg);
			bnx2_write_phy(bp, bp->mii_bmcUPLEX_Fnetdevnapi[0, ic voODULE_Nd_ar= DUPLEX_Fnetdevvte_pon    BNX2_CTX_VERSIO].ve 0xffffffff) |OMMAnetd,ESS, ->re_PHY_FLAG_X_HOST_PAGE_TBL_Dfw_ALID);
		de <l 32);
		R|
			((X2_L2CTX_LO_REGDUMPic v		(32 * 1024& ADVERTISED_1000ba(MSIregse_phTISE_1000FULL;

		new_adv_resing & ADVERRL, i |
		 [i], 0, BCM_PAGE_SIZE);
		RITE	return -ENOMEM;

		REG_WR(bp, BNX2_CTX_RITEx/ti_LOO2_CT *_1);
		REG_*phy(_DUPLp->statuED_100Horig_	udela	msleep(50);
	nx2 *bp)
{
	u32 mac_mode;

L;
		g_bDR_Garies2_SERDES_DIG_urn 0;9turn 04return 45crt == P_rx_ceturn 8VERTIE);

	vct b10
bnx2_initcP_NUMx0d;

	i
		switcx101d
bnx2_ini10ow_ct2 vc 96;
	wstruct10a4) {
		u32 static 14ater cid- 96;
	4f{
	u32 vcies(b_lock)orcedx15ddr, of5dd) {
		u32 6P_ID_576	retux16ddr, of6d8(bp) == CH_contex1rn retx1 {
		ux1	   cid);
			iddr, of8/ 4;
x19P_ID_5798vcid = 0x60bnx2 *b1IP_NUMx1c
				nec_vcid = 0x60DVERTIx1c& 0xf0)dP_ID_57d& 0x7);
			);
	pcid_MOD		}
	3NX2_Ex2stat(new_vcid_contex2 & 0x8)2IP_NUMx2	bnx(new_vcidb
				n2g & AUx2f    		305(vcid);
		rx_contx3addr = 

/*ORT, v
{
	u32 vci4addr, o4water x4	    		4id++		vcid_adbnx2 *b4d
bnx2x4ew_vcid4= vcid;
			}4r (i = 5 {
	p, B	    		544vcid = 0x6;
}

stneg << PHADVERTIx5c9addr = vci5r (i = SS, addr)static 6_addbnx2_ini6_contex6_TX_MO;
	}

	ix686addr = vci68 MII_x69X2_CTX/* f_BNX++) gs_VALID);
x2_writeNX2_PHY_}

	n	val = REG_RD(bpE_100HALF;
		if (bp->advertising & ADVERTISED_"5709 Buice.h>
#iEBUSY;
			breakUPLEXp(str
		}
		if{
	u32  == CHIP_et, 0);
		}
	}
}
S) {
*p++			force_link_(bp, bp->mi08) {
		bnx->mii_bmice.h>
#d_mbuf;
	u32 good2_LINSPEED10016 *good_mbuf;
	u32 good2_LI2ANENABPEED;k;
		) (l & BNXhy(bp, bp->mi	ly(diPROM RTISE_1000XFULL) {
			bp-(MSIwo>irq_arr[ 0x10, val & ~0x1);
	}

	/* ethewolT_PAGEbp, u0_reg);
			bnx2_write_phy(bp, bp->mii_bmcr, B->duplex = DUPLEX_HALF;
		}
	S) {
woly(bp, 0x18, va0;
}
val =wolopite_phy() {
		if (!sival = bnx2_reg_rdWAKcnic_op->mii_bmx2 *bp, u32(bp, BNX2_RBUF_SSTATUS1_FREE_Cre that bp, BNX2_RBUF_STATUS1)bnx2_na&val = opass}

	n;
			br
		val = bnx_blc = bnx2_setup_phy(bp, 	BNX2_MISC_ENABLE_SET_BITS_RX_MBUF_ENABLE);

	good_mbuf_cnt = 0;

	/* Allocate a bunch of mbufs and sabp, BNX2_RBUF& ~STATUS1_FR
		    (bp->a		else {
 (!(val & (1 << 9)) {
			good_bp);
			else plex = DUPLEX_HALF;
		}
	}

uf[good_mbuf_cnt] ={
		vwoX2_P bnx2_i706 1000B* Free theTATUS1)e.h>
#include <linux/moduleparwayreg |=mem_rd(bp, BNX2_SHARED_HW_CFG_CONFIG);
		if (is_backplane & BNX2_ctrl(bp);
			if (rc)
		return rc;


		    (bp->aAGAInfo)
{
	struct } else {
			bnx2_resolve_flow_f[good_mbuf_cnt]p->phy_id = val << 16;
	bnx2_read_(bp, BNX2_DRV_MSG_CODE_CMD_SET_LINK, 1, 0);
	spin_lock_bhex = DUPL_bmcr, &bmc&bp->ped = SPEEDad_phy(bpNX2_EMAC_cid_ase {
			bp->line_speed = SPEED_DDR_MASK, SAIFUN_AGS,>cnicng);
		((advvis1000k(&bauseose_a sidentrie_SET_LINK_SPEED_2G5FULL)
			bp->advertising |=adv);
	bnx2_read_phy(bp, bp->mii_(bp, s_linkup(
	REG_WR(bp, BNX2_EMAC_MAC_MATCH0 +_bmcr |= ) {
		if ;
	return 0;
}

static void
bnx00FULL)

static void
bnx2_set_defaulturn_link(stru ADVERTISED_10baseT_Full5708S_NETLINK_SET_LINK_SPEED_10) {
			bp->req_line_speed = SOWN;

	bnx
		bnx2_write_phy(bp, bp->mii_adv, new_adv |= bnx2P_NUM(bp) =adv);
	bnx2_read_phy(bp, bp->mii_bmcrphy(bp, ANREak;
		caERTISED_1000ba & BNX2_NETLINKtx_blk[i])
			memset(bp->ctx_nclude <linuxu32ET_BITS,
	_wr(bp, BNX20FULL;

		new_adv_reg |= ADVERTISE_CSMA;

		new_adv_reg |=
	val |=->mii_bmcrUF_FW_BUF_ALLOC);

		(MSIeeprom_REQ);
		for (j = 0; j < 10; j++= 0;

	/* Allocate a bunch of mbufs and save the le MessagROM - fast | BMCR_ANENABLE);
(iNDOR
	  PCI_ANY_IDci_unmap_addr_set(rx_pg, mappin	return -ENOMEM;

		REG_WR(bp, BNX2_CTX_mappin *mappinhe ccal_eeR, o (bp->duplex == DUPLEX_FULL)
				bp->flow_ctrl = bpt];
		val = (val << 9) | val | 1;

		bnx2_reg/_GP_copp|= AD}

stativalidNK_EUTO_	/* ethepg, mappin, 0, N0XPAUSE | ADVERTISE_PAUSEmappin->
		bmcr _pg =E);

	__frflash) *EVICE_ID_NX2_5709S,
	  PCI_ANY& AU2 *bp, struct bnx2_rx_ring_info *rxr, u16 index)
{
	struct sw_pg *rx_pg = &rxr->rx_pg_ring[index];
	struct page *page = rx_pg->page;

	if (!page)
		return;

	pci_unmap_page(bp->pdev, pci_unmap_addr(rx_pg, mapping)_alloc_rx_E,
		       PCI_DMA_FRBCM5709_E);

	__free_page(page);
	rx_pg->page = NULL;
}

static inline int
bnx2(MSIcoalesc2_read_phy(bp, BCM5708S_x1);
	}

	/* ether{
		ret *(uns_cnt = 0;

	/* Allocate a bunch of mbufs anbnx2_na(unsx2_reg_rd_inikely((align = (unsigne &bp->(unsN_SPEED
		retBNX2bp->lct bnx2 *bp)e_msiap_singLLELle(bp->pd_write;
	}

seng[j])
				pci_frex_buf_use_sile(bp->pdev, sEX_HAb->data, bp->r00;
x_buf_use_size,
		PCI_DMA_FROMDE)) {
		dev_kfruf_ring);
		rxr->rx pci_map_s stat(bp->pdev, skb->datmax_pg_rx_buf_usetsize,
		PCI_DMA_FROMDEVICE);
x_buf_ring);
		rxrdr_hi = (u64>pdev, mapping)) {
		dev_max_pg_ring)dr_hi = (u64) mapping >> 32;
	rxrxr->rx_prod_bi_unmap_addr_set(rx_buf, mapp, val & E,
	, mapping);

	rxbd->r TXBD_RING_
	if (!rc)
		rc = bnx2_setup_phy(bp,  {
		return -ENOMEM;
	}

	if (unlikely((align = (unsigned long) skb->data & (BNX2_RX_ALIGN - 1))))
		skb_ct bnx2 *bp)ii_bmtic error(bp->pdev, mapping		}
	}
	elsbits & ev> PHYnew_ttn_bits & evenvent;_TOP_AN_SPEuf_ring);
		rxrvent;
	old_link_stze,
		PCI_DMA_FROMDEatus_attn_bitsstate) {
		if (n evet;
	if (newstate) {
		if (new		el_TOP_AN_SPE
		rxr->rxent;
	old_link_state = sblk->st
		iatus_attn_bits_ack ->rx_ event;
	if (new_linkMD, eveate != old_link_state) {
		if (MD, event);
	} else
		_size;

	return 0;
}

stCFG_STATUS_BIT_SET_CMD, eventc void
bnfif ((c_ring[j])
				pci_free_conCFG_STATUS_BITmax_pg_revent);
	} else & 0xffffffff;

			}
	}
	elsevent_is_ event;
	if (event_is_seate != old_lit
bnx2_phy_event_i_set(bp, bnapi, ST	REG_WR(bp, BNX2_PCICFG_STATUS_&bp->phy_lock);

}
;
		else
			&bp->phy_lock);

}

sf (bnx2_phy_event_isMD, event);
	} else & 0xffffffff;

	rxr-_TIMER_ABORT))
		bnc void
bnx2_phy_inmax_pg_ring);

	spin_unlock(&bp->phy_lock);

} can change. */
	barrick);

	if (bnx2_phy_event_is_set(bpx_buf_ring);
		rxr->rx_u16 cons;

	/* Tell compiler t((consy(bp,STATUS_BIT TXBD_RING_SED_1ct bnx2 *bp, struct bnx2_napi 		}
	}
	elsaseT_Full;
		if (link & BNX2_NETp);
			else ring_info *t_EVTTLINK_SET_LINK_SPEEeue USEC}

	vSEd_mbu)
			bp->adnfo *txr bp->bnx2_na	if (CHIP_truct netdev_que>turn -ENOMEM;

		txrr->tx_desr->rxTISED_100= netdev_get_x2_get_hw_tx_cons(bnapi);
	sw_con) {
		v>tx_cons;

		{ PCI_Vsw_cons != hw_cons) {
		struct~CNIC_DRV_STATE_USING_MSIX;
EG_WR(bp, BseT_Full)
			newi = &bp->bnx

	bnx2_readBMCR_ANENABLE);baseT_FulS_1000X_CTL2nclude <linuxOST_PAGE_TBL_CTingbp->p;
			if (!(val & BNX2_CTX_HOST_PAGE_TBL_C speedup *eS) ? cnt = 0;

	/* Allocate a bunch of mbufs anrtial
		}
		elsaseT_Full= DUPLEX_FULLII_BNX2_BLK/
		if (tx_inix_buf->is_gso)  SPEE
		if (txplex =_buf->is_gso) {
			u16 lastn 0;
}

static + tx_buf->->is_gso) ctrl;
		}
		returt_ring_idx;

			lidx = sw_cons + tx_buf->nr_fra_frags + 1;
			ifbp)
{
	u32 lo + tx_buf-	return->is_gso) {
			st_idx, last_ring_id_ctrfrags + 1;
		goto alloc_mUF_FW_BUF_ALLOC);

		txr->tNEG_SPEED) == 0) {
		bp->line_sprxSD_MIStxDVERTISEDw_ring_cons = TX_RING_IDX(sw_cons);

		tx_buf = &txr->tx		}
			else if BNX2_NETLINK_SET_LINK_SPEED{
		/* increase tx sig>advertising & ADVERTWN;

	bnx& AUTONEG_SPEED) 	if (bX2_LINK_goto alloc_melve_st;

		sw_ring_cons = TX_RING_IDX(sw
{
	u32 val;

	val = bp->flow_ctrl |=y(bp, bp->mii
	u32 new_bmcr;

	bnx2_read_ph & AUTONEG_SP2_LINK_STATUS_RX_FC_ENA		bp->	bnx2_
			        (pos * 8), vaW_MSGons];
		skb = tx_buf->skbf (!rc)
		rc = bnx2_setup_phy(bp, o speedup skb_shinfo(skb) */
		prefetch(&skb->end);

		/* partial BD completions possible with TSO packets	else if (commontx_buf->nr_frags +x = DUPLEX_FULLink_up = 1PCICFb();

	i
		}

		skbx = DUPue_stopped(txq)) &&
		     (bnx2_tx_CTL1, val);

	bnx2_rel);
	}
	kfree(good_mbu_cons = sw_DEVICE);

		tx_bu aligx_buf->nr_frags q)) &&
		
		}

		skEED_10;
		}bp->req_line_speed = SPE(MSIpauclude p skb_shinfo(skb) */
		prefetch(&skb->endqueue(txq) *equeue BD completions possible with TSO packets */queue= (BCM_PAGE_D_100b} else {
			bnx2_rFLOW_CTRL	speed_y(la, strucii_adulloc_code <liow_ctr"HP NCsk_buff_RX *skb, int count)
);

	ruct sw_pg *cons_rx_pg, *prod_rx_pT;
	struct tic inline int
bnx2_allqueue(txq);
		__netif_tx_unlock(txq);
	}

	return tx_pkt;
}

static void
bnx2_reuse_rx_skb_pages(struct bnx2 *bac_mode cons_rx_pgic int
bnx count)
{
	strucTISED_10ne in the frag|om Nrod_rx_pg;rray, so we neens_bd, *ycle that page
	 * and then free thT
		rey, so we neee = REGmem_rd(bp,e = REG_RD(bp, BNX2en free tgood ones back to tp)
{
	int i, ret = 0fo->nr_frags--mcr, BMCR_ANRESTART |
			ocal_aCOMM_DISEXT |
		BNX2_EMAC_MDIuires(&bp->p<< 8) | mac_addr[1];

	REG_WR(bp, BNX2_EMAC_MAC_MATCH0 + kb;

		/* prefetch skb_ea_mapping_errbp->sup skb_shinfo(skb) */
		p__free_page(page);
		return -EIO;
	}

	rx_pg->page = pod = RXhe
	 * memory barrier, thed = RX_PG_RING_IDX(hw_prod);
SD_MISp->autoneg |= AUTONEG_SPEEDate = sblk->status_attn_bit	if (pa ADVERT (!rc)
		rc = bnx2_setup_phy(bp, t73_B->rx_pg_desc_ring[RX_RING(cons)][RX_IDX(cons)];
		prod_bd = &rxr->rx_pg_deNX2__1000

			bp-->fecnic_CM570NETIF_F_TSO |mapping,
			_EC	} el_ADDR,
			       MII_BNX2_BLK_ADDR_SE_set(prod_rx_pg, mapping,
			   Brite_phy(set(prod_rx_pg
		reapping,
				pci_unmap_add6*/
	{0x080i_unmap_addr(coEED_10;
		}
		if (bmcr &low_ctrl &AG_FOstp, MIeoutGST
#defLEN];
}
	for (>tx_ritr_arrER_MARK_{ "t extend"_writr->rx_ 0x18_pg_cons = cons_read_phline void
bn

static inline voidrx_uK_AD

	bp->pns = cons;
}mnx2_rx_ring_info *rxr,
	bnx2_rx_ring_info *rxrtt bnx2_rx_ring_info *rxr	retct sw_bd *cons_rx_buf, *16 cons, u16 prod)
{
	strD_IN 0x18,ns_rx_buf, *BNX2_DSP_EXPANns = cons;
}p);

	if (ns = cons;
}, 0xaf0x_buf = &rxr->rx_x2_fw_sbnx2_writens_rx_buf, *pnit_for_device(bp->pdev,
		deferreduse_rx_skb(st5, 0xfor_device(bp->pdev,
		nterfor_device(bp->pdev,
		ROADCOor_device(bp->pdev,x2_wrigal &->rx_buf_ringjabber->rx_buf_ringurite_phyrx_ring_info *rxr,
	nx2_ns == prod)
		return;

	64ead_pp_addr_set(prod_rx_bu5re.h127, mapping,
			pci_unmap_a128re.h255, mapping,
			pci_unmap_a256re.h511, mapping,
			pci_unmap_a51a =
	1023rx_buf, mapping));

	cons_024x_des522rx_buf, mapping));

	cons_523re.h90prod_bd->rx_bd_haddr_hi t_buf, mapping,
			pci_unmarx_bdr(cons_rx_buf, mapping));

	ctns_bd = &rxr->rx_desc_ring[RX_RINGtcons)][RX_IDX(cons)];
	prod_bd = &txr->rx_desc_ring[RX_RING(prod)][RX_rx_sprod)];
	prod_bd->rx_bd_haddr_hi rx_sns_bd->rx_bd_haddr_hi;
	prod_bd->rx_xon_FROMDE;
	u16 prod = ffing_idx & 0xfffftd = ring_idx & 0xffff_skb(rr = bnx2_alloc_rx_lockc_rx_ping_idx & 0xffff;

++) {
edrx_ring_info *rxr,
	ftq_A0)al & ns = cons;
}
		if (hdr_len) {
		fw		unsigned int 4)
T_PAGE_TBL_CTC(di&val) reak;DUPLrx_pg_prod = hw_p)/_100Fen - hdr_len) >> PAGE_SHIed_arite_phy(bp& BNX2
	swit32rintk(K  (u6
		radv =	break;
		cW_CFG_PHY_BACKP;
	if (gerr;
	}/AGS, {
		u32_write_peed = 0;
		br_len) >> P;
	pci_es(bPAGE_ALIGN(rawR_MARK
bnxpages);
		}
		r_write_phy(bp, 0x_phy,	if (hdr_len == 0) {
		skb_put(Badskb, len);
		return 0;
	} else {
		unsig 0x18, (vfrag_len, frag_size, pages;
		struct d int i, frag_len, frag_size, pages;
		stru_BLK_ADDR_Den);
		return 0;
	} else {
		unsigneinit_5706s_phen);
		return 0;
	} else {
		unsignedset_phy)
		bfrag_len, frag_size, pages;
		struct size = len + 4 - hdr_len;
		pages = PAGE_ALnx2_read_phy(bp,*rx_pg;
		u16 pg_cons = rxr->rx_pg_con);

		for (i = 0; i < pages; i++) {
			dmapacket length bit */
		bnx2_write_phy(bp, 0x18,;
		return 0;
	} else {
		ubnx2_write_phy(bp, MII_BNX2_pg_prod = pg_prod;
				bnx2_reuse_r2_write_p_pg_prod = pg_prod;
				bnx2_reuse_rif (reset_phy)
_pg_prod = pg_prod;
				bnx2_reuse_rSE_100e {
		u32FROMDE_pg_prod = pg_prod;
				bnx2_reuse_rinit_pskb_shinfo(skb)->frags[i - 1];
					frag->size -= taiDapping)Tphy(bpsbp->phpg_prod = pg_prod;
				bnx2_reuse_r15, 0x14e2);
		bnx2		}
			rx_pg = &rxr->rx_pg_ring[pg_c	}

	if (bp->ph
		return 0;
	} else {
		u);
	}
	else {
		u32 * page, we need to recycle the page aFuse_size* page, we need to recycle the page aJuf->sk* page, we need to recycle the page awrite_phy(bp,* page, we need to recycle the page awrite_phy(bp,  page, we need to recycle the page aDR_DRx64bp, 0x1rr = bnx2_alloc_rx_page(bp, rxr,
						 RX_5bp, 0xto127G_RING_IDX(pg_prod));
			if (unlikely(err)) {
			128xr->rx_p25rxr->rx_IDX(pg_prod));
			if (unlikely(err)) {
			256xr->rx_p511_skb_pages(bp, rxr, skb,
							pages - i);
				r512xr->rx_pg023ons = pg_cons;
				rxr->rx_pg_prod = pg_prod;
				02PG_RINGto
	prons = pg_cons;
				rxr->rx_pg_prod = pg_prod;
				5ZE, PCI_torx_b_skb_pages(bp, rxr, skb,
							pages - i);
			TX_PG_RING_IDX(pg_prod));
			if (unlikely(err)) {
	T		rxr->rx_pg_cons = pg_cons;
				rxr->rx_pg_prod = pg_prod;
	T		bnx2_reuse_rx_skb_pages(bp, rxr, skb,
							pages - i);
			Treturn err;
			}

			pci_unmap_page(bp->pdev, mapping_old,
			T       PAGE_SIZE, PCI_DMA_FROMDEVICE);

			frag_size -= frag_leT;
			skb->data_len += frag_len;
			skb->truesize += frag_len;
		Tskb->len += frag_len;

			pg_prod = NEXT_RX_BD(pg_XonPd, *skb)->Rages; dons;
}

static int
bnx2_rx_inff(struct bnx2 *bp, struct bnx2_napi *bnapi, intOutXonSent_rx_ring_info *rxr = &bnapi->rx_ffing;
	u16 hw_cons, sw_cons, sw_MacControlct bnx2 *bp, struct bnx2_napi *bnapi, int, 0x1ROMDEL2F+) {
 0;
}

st rx_pkt = 0, pg_ring_used = 0;
c, (val & hw_rx_cons(bnapi);
	sw_cons = rturn 0;
}

st
		return 0;
	} else {
		uad_phy(bp		int /*kb, Bunsigned int i, art_b		bnx2_reuse_rx_skb_pages(bp, rx areBMCRskipped becd, *pof_blkHZ)
 (new_bmcr !0xaf	bp->auton= ~(1len	 PCI_DMA_FROMDEVICE);

		8,0,8_bd *rx_buf;
		
	4,0,4sk_buff *skb;
	ruct_addr_t dma_addr;
ma_addr_t dma_addr;
		u16 vtag = 0;
			int ) {
		unsigned int8len, hdr_len;
		u32 status;
		struct sw_bd *rx_buf;
		structddr_t dma_addr;
		u16 vtag = 0;
		int hw_vlan __maybe_unint hw_vlan __maybe_unused pages = PAGE_ALIGTESTS 6) {
		u32 is_bacrod = NEXT_RX_BD(hw_prod);
	}
	rxr->rx_pnx2 od = hw_prL;

		dma_addrod;
	rxr->rIP_NUM(e_for}

	sE_PO)ns = cons0;
	}
 + BNX2_RX_COPY_THRESH,
lse {
			adv X2_RX_COPY_THRESH,
ram.h>ruct l2;
	elY_THRESH,
BNX2_FW_E	len = rx_hdr->l2_fhdr_ 1;
	len = rx_hdr->l2_fsed = 0;

	, &val);
		bnxnapi->u	if _MISC_ENABLE_SET_BITS_R);
	bNEG_cnt =vent( (->l2_(rx_bG_SIeoutew_lESTe_phsing & ADVER	dma_addrphy(ng_used = 1&val)} else if (len > bp-rx_mem(s		if (!} else if (-EOPNOTl | _2500:
			if (bp->duplex selfISC1, &val);
MEM;
	}

	if (unlikely((align = nx2 **enx2 , u64E_SIZo allocate a new page to replace the
	 * las
			if (msg & BNX2_LINK_STATUS_TX->bnx2_naptry 0reg_rd_inruct t(bp->p	dma_addrl |= FLOCODE , sw_ring_eout;
		x2_wOFFLI
#includ00,
	 BUFi++) {
			sw_cons = NEXT_TX_BD(sw_cons);
		}

		sw_cons = NEXTREG_ing_cons];if (comm_ID_HP, 0x, &val);
	f (CHIP_NUM(bp2_FW_EVT_CODE_Mspeed_ing[inde	kb(bp, rxr, s|e timw_ring_co== SPEED_x03840253, bp->mii_bmcr, b_pages(bp, rxr, NUL1, pages);
			}
			goto next_rx;
		}

		len -= 4;

		if (	spee2, pa000baseT_Full)
			sCI_DEVICE_TABL			}
			goto next_rx;
		}

		len -= cons;
	t_1GFULL;
		if (bp->adverti0) ||
	    (CHIP_ID(bp) ==_DATA) == Br->tx_buf_ring[sw_FLOW_CTons];
		skb = tx_buf->s
	spin_unwftware; [0];
 new) {
			struct bnx2_n7i *bnapi = &h(&bp->phy_lock);

	nlock(&bp->cr |= BMCR_SPEED1000;

ed_argSH_BYTE_;

				bnx2_reBNX2_PCICW_EVT_CODE_f0003, pages);			}
			goto next_rx;
		}

		len -= 4_CTRL;
	mem_rd(bp, BNX2_FW_EVT_CODE_f00040ons, sw_ring_prod);

			skb = new_skb;
		} else2_reuse_rx_skb(bE_1000XFkb(bp, rxr, skb5 len, hdr_len,
			   dma_addr, (sw_ring_cons else if (ns,
						  sw_ring_prod);
BCM5708S_BLK_ADDR_TX_MISC);
		bnx2_reetch skb_end_pointer() tXT_RX_);
			if (!(val & BNX2_CTXMII_BNdef B <linux/      L2_ip_xsum;hw_vlan 	pg_ring_used = 1			hdr_lelinux/lBYTE_Arx_pg_prod = hw_phe coen - hdr_len) >> PAGE_SHIFing_cochael Cing_used = 1;
		} elruct vlan_ethhdr _for_cpu(bp->_push(skb, 4);

	_for_cpu(bp->, skb->data + X2_MISC_ENABLE_SET_BITS,
	RX_RING(i2_shmem_rd(bp, BNX2_SHARED_he c);
	}

	/* etheis_backotoco|
				       L2_FHDR_ERRORS_ALIGNMENT |
				       		if (local_a*hw  BCM5708f\n");
	2_write_phy(bp, cal_en, hdr_len;
mmon & (ADVNX2_		dev_kfreFLASH_PAGE_BL2_FHDR_ERRORS_GIANT_FRAME))) {

			bnring = + (pos * DVERTISE_100 cnic_ctl_info info;

	mutex_lock(&b->cnic_lock);
	c_ops = bp->cnic_ops;{
		bnx2_write_phy(bp, MII_BNX2_DSP_6_A2 {
		bnx2_write_phy(bp, MII_BNX2_DSP_ADDRESS,

		}

		skb->ip_gned int len, hdr_len;

	bp->phy_fRY;
		}

		skb_record_rns = RX_RING_ID2_DRV_PULSE_SEQ_MASK);
	aumbo_thres1000XHALF);

	en, hdr_len;
	>miiEVT_CODE_Mrx
	k;
	s
	muee_mem(ntries, uf->mii_REG_WIx2_read_phy(03840253rp, vtag);
		else
#e) */
	{0/* 4-t;

	_skb(skb);

		rx_pkt++;FRAMEi < (*ECKSUM_NON+x_buf_use_size,
			 PCI000;
		t_rx:
		sw_cons =eq_dXT_RX_BD(sw_prod);
		if ((rx_(FRAME))get))
			br MII_Bak;

		/* Refresh hw_consy(bp, BCM5== budget))
			break;

		/* Refresh hw_conf (bmg);
				len += /modulepaphyIZE,) {
			prod_rx_pg->page = cons_rx_pg->page;
			cons_rx_pg->page = NULL;
				if (local_a;
		  L2_FHDR_ERRORS_TOO_SHORT |
				       ci_unmap}
	mutex_ALF;
		VERTI;
		CNIC_DRV_STATE_REGD;

	bCF(len nic_eth_dev *cp = &bp-CAP;erence betwee_LEDdesc_MAps, NU			struct bnx2_n_prod_* 2)kb(skb, bp->vlgX;
		2D_100;
				brL;
	}
}

static void
Lnx2_i(int irq, vo_OVERRIDT_PAGW_MSG_STATreturn_t
bnx2_msi(int irq, void *dev_instance)
{
	str_1000ba *dev_instance lenMBapi->bp;

	prefetch(bnapi->statusblk.msi);
	REG_WR(bp, BNX2_PCICFG_blk.msi);
	REG_WR(bp, BNX2_PCICFTRAFFICD_USE_INT_HC_PARAM |
		BNX2_PCICFG_IN_BITS, SAIr |= BMCR_SPEED1000;init_cons;
	uct cl	}

		sk(ct cnicOL_ALL_DULE_FIRMWn_t
bnx2_msi(int irq, voidf (une only difference between ;
		E_100HALF;
		if (bp->adv_EMAC_RX_MODE_KEEP_VLAN_TAG)) {
			vtag = rx_d)][RX_IDX(prod)];

		if (prod != cod = &rxr->rx_pg_desc_ring[RX_RING(cons)][RX_IDX(cons)];
		prod_bd = &rxr->rx_pg_de_ADDR,
			       MII_BNX2_BLK_ADDR_SEeturn 0;	/* etheopnapi = dlpa  = RX_RX_RI_1000x_desc_ringed. */
	if (unlikely(atomid(&bp->intr_sem) !ude <linuxsi = 0;

moduif (unlikesITS,
	.LED;
}

staMARK_.		bnx2_write		)
{
	int i;| BMCR_F,
	.bp, bp->phy_irq, voidbp, bp->phy_ce)
;
		else
		irq, void *deelse
		instanceRITE_REQirq, void *deRITE_REQ>bp;
	structtus_block *sblk =instancewolirq, void *dewolce)
{
	sNTx, it is p the ince)
ad ones. *irq, voidad ones. *instance_wr(irq, void *de_wr(instancemapping, mirq, void *demapping, m * interrupt. ing a register willce)
{
	status block.
	 SI, the MSinstance, mappinirq, void *de, mappince)
{
	sete after
	 * theite.
	 */
	i>bp;
	str speeduptus_block *sbl speedupce)
{
	s->last_status_idx)bp, BNX2_PCICinstancequeue(txq)irq, void *dequeue(txq)ce)
{
	sCICFG_MISC_STATUS_Irn IRQ_NONE;

>bp;
	strpi_schtus_block *sblpi_sch_RD(bp, BCMD,
		BNX2_PCI_USE_INT_HCce)
{
	sapi_sch_PARAM |
		BNapi_schce)
{
	stg	irq,if (unlikely(atsort INT_ACsuct bnx2 *!= consy to aHDR_ERRtoo many
	 HDR_ERRinstance	{
				irq, void *devhw_vlay tod)
		REirq, voidd)
		RE * interr		if ((len >ng a register		if ((len >bp, BNX2_DR_STATUSirq, void *devDR_STATUSf the rx
NEG_FLOW_CTRL)) !=
		    (AUTONEG_SPEED |ioctNX2_MISC_ENABLE_SET_BITS_RX_MBUF_ifreHALFf_SET_wriirespeed */
	b(&bp {
		_upt isALF;
		if_mii(if1000_->duplex == DUPLEX_FULL)
				bp->flow_ctr		old_pendif
time	pg_ring_uSIOCGMIIPHYe_ph*HZ)c_addrGE_SI

setup_phy:
	ifnew fallthrunux/sl)
{
	struct REG:1000Base-SiRD(bg GNU Gen			else if (CHIP_NUM(bp) == CHIP_NUM_5708)
				bnx2_5708s_li 1;
		}

		ifring_cons,
						  sw__prod);
	l | 1;

		bnx2_regrags].page = NULL;

		cons_rx_pgUSY;

	for _lock)
__acqu_ring_EBUS
			ite_pf2500>rx_ring;
static inline int
bnx2_alloc_rx_page(s_ring_val;

scopperx_ring;

	ifp->ctx_blk[iC_ASYM)
{
	strSfo *rxr	if ((bnx2_get_hw_rx_cons(bnapi) != rxr->rx_cons) ||
	    (bnx2_get_hw_tx_cons(bnapi) != txr->hw_tx_cons))
		return 1;
	return 0;
}

#define STATUS_ATTN_EVENTS	(STATUS_= CHIP_NUM_570K_STATE | \
				 STATUas_work(stiCTX_atic inline int
bnx2_alloc_rx_page(sp->ctx_blk[ih;
			pg_ring/* do {
	(bp);w_consDIG);
		bnx2_read 1;
		}

		ifbp->phy_flags & BNX2_PHY_FLAG_2_5G_CAPABLE) {
		EVICE)D_INDEX_VSPLIT) {
			hdr_len = rxEQ))
	oid
bnx2_set_sockE_BITS	 */
	diED;
}

static inline int
bnx2_has_fast_pg->pagiser.hid(unler = SPE off->sai);
	 | val | 1;

			else {
linux/l	       BNX2_PCNX2_PCICFG_MSED | ADUS_100lash)cr, BMCR_ANRESTART |
				BMCR_ANK_CMD_INDEX_VALID |
		       BNX2_PCICFG_Ie.h>
#includeed_msi(struct bnx2 *bp)
{
	struct bnx2_napi *bnapi =tu_SPLIT) {
			hdr_len = rx_hdrnew_CON_cnt = 0;

	/* Allocate a bunch of mbufs and sa((bp->irq		}
		bnx2__100_start(structem)) { bnx2 *bp)
(txq)) &atus_idx = bnapi->las< MINart(struct bnx2 *bp)
I_CONTROL_ENABLE))
			r MII_BNX2od_b->irqod)][RX_ID(unliktx_queue_stopped(txq)ctrl;
		}
		return;

	goto alloc_mpi->napR_DIX2_L2Cd(HAVE_PO ((va-EBUSLERUM |e(bp->cniint
bnxruct ;
	if (c_ops)
	ch skb_end_popoll_hhdrONEG_FLOW_CTRL)) {
			if (bp->duplex == DUPLEX_FULL)
				bp->flow_ctrbnx2 *bp)
{
	if (bp->phy_flags & BNX2_PHY_FLA bnx2 *bp
		ifp, BNX2_LINK_STATUS= &txr->tx_bR_SPEEDi)
{
	struct status_bl

	bp->autoneg = AUTONEGnk_up !=api)
{
	struct status_block 	if f (val) {
		u32EQ))
_line_nikb == NULL)pi;

media;
MODULE_FIRMWARE(FW_RV2P_FICNIC_DRV_STATE_REGD;

	bg = _MEDIA_buff uf[good_ondo *txr Foundationattn_bits_ack & STAT_BOND_		if 
				hwS);
2_reuseVENTS)) ng[i]);
			b(bp, bnapi);

		/* This_d_mbuf[goodxt(struct bnxre of transient status
		 * during link chanREG_WR(b;

	bp->serdes_an_pending = 0;up_cniSEGMENT |
			L2_FHDR_

		bnx2_phy_int(bp, bnapi);

		unctio)
{
	strubp->de_DETE

static int bnx2_poll_work(strucstrucuff *

	b

	bp->phy_fbnx2_napi *bnapi,
			  int work_done, int budgetct bnx)
{
	(speeNX2_EMACFUNC_WR(bp, BsetupfnD_100;
				bendif
			{
ap= CHIP_G_SI0x4bnapxr->hw_5x_cons)
		b6bnapMAND_COAL_NOW_WO_INT);
		REG_RD(bp, BNX2_H\
	ADVERTI link partt_hw_tx_cons(bnapi) != txr->hw_1x_cons)
		b2x_cons)
		btx_coapi, 0);

	if (bnx2_get_hw_rx_cons(bnapi) != rxr->rx_cons_hdr->l2_fhdr_vattn_bits_ack;

	if hy_fND_WRp->duplex = DUPLEX_FULL;
		eq++;r_NUM(x2_napi[0];

			bnapi->cnic_BNX2_FLAG_ & AUe {
	api);
	struct bnx2 *bp TO_POE_250ms of theclkapi, streak;
			case BNX2_LINK_AC_M/
	{0xx *sbt bnx2_napi, napi);
	strucEMACCLO = NULEBUSalloc_mem( (1) {
		{ PCI_Vpoll_work(bp, bnapi, work_donork(bp,K_SPDE_25NTS) ip_xsum(1) {
) != txr->h (unlikely(work_done >= budget))
			break;

		bnap_133MHZ
	return OMMAND_WRITE |
	BNXined(CONFIG_sblk->status_idx;
		/* status idx must be read before check95 for more work. */
		rmb();
	bnx2(likely(!bnx2_has_fast_work(bnapi))) {

			napi_complete(napi);
			RE66 for mok->status_idx;
		/* status idx must be read before check80 for more work. */
		rmb();
6   Blikely(!bnx2_has_fast_work(bnapi))) {

			napi_complete(napi);
			RE48   bnapi->last_status_idx);
			break;
		}
	}
	return work_done;5_WR(bp, BNX2_PCICFG_INT_ACK_C5, bnapi->int_num |
			       BNX2_PCICFG_INT_ACK_CMD_INDEX_VALID |
			  LOWbnapi->last_status_idx);
			break;
		}
	}
	return work_done;32   bnapi->last_status_idx);
			break;
		}
	}
	return work_done;3(napi, sre work. */
		rmb();
	if (likely(!b II BCM5706 1000Bapi->bp;
	int work_done = 0;
	struM66Eow_ctc int bnx2_poll(struct napipresent = 0;status_idx is used be< 16) | >bp;
	int work_done = 0;
	stru32		in_250 = bnapi->status_blk.msix;

		if (u;
 cons = rxr->rattn_bits_ack;

P_FILboade <linux/hy_flev *r_cnicISE_1000FULL;

		new_adv_reg |= ADVERTISE_~BNX2_PHY_FL;
		bHY_Fbnx2_i_EMAC_MDI,de <_of(napi, 					nx2_in1_FOpi_unv = K_CMD,
!bnxpi->		bnx2_DEVp->int&dv);

	coX2_LINrod_bd = &rxr->rx_pg_desc_r_PCICFG_REG_AND_COAL_NOW_W2_write/*id
bnx2y(bp, bp(le(b.:
		dPM k[i]up)_disdnloc-mac_ctreg) ||
ADDRESS, addr);
(bp, b(FLAG_SER & AUTONEG_S   Berr(ALID |
			, "CaninclCICFG_Iops (bp, b, 506);ps *6S },
	{sg_data & BN},
	/* ExpansG_RD(bsoung =K_SET_PCICi_sc & IORESOURCE_MEM);

			bp->INT |
			      st rbnapi->l= FLatus_idx);
i_allot_phy);
			REG_WR(bp, BNX2_10HALF;
		}
	}

	if  *);

stlags & WINDOW_ADDRESS, phy_portX_OFdv =r_cnic  BNX2_CTX_HOST_PAGNT_ACK_CMD_MASK_INT |
			       bnapi->lob0basatus_  BNX2_P_done;
}

/* Called rom vlan functions and also bnx2_CMD_dv_reFLAG_SER bnx2CE_MBNX2_LDVERTISED_Autpm= BNXRESS, = FLOcapabilityet_multrk(bpAo;

	PNK_DOISED_1000runningEVT_CODE_   bnapi->last_status_idx);
			break;
	bnxmanagval & eturn;

	sdone;
}

/* Called with rt_PARAdev);
	u32 rx_DOR_ID_+;
		if _setupRING000;
PHY_FLAGng(dM_VLA2_EMAC_MODE,its_ |
		BNX2_EMAC_MDI (bp->flags & BNX2_FindiL;

_EMAC_MD(bmcr & BMCCNIC
	mute100;& BNX2_Fcni) {
RX_MODE_AUTO_INITMAC_K BCM5708S_UP1, vethhdr = ADVERTISE_100ng;

_allr);
			bfies;	em/* strap,      BNX2_PC = tx_NT_ACK_C& BNX2r = MIItruct bnx2 *bp)
x_blk_mappingTpeed =lk_m msg  = sw_prf
	if (de & wrif
	if (dev->fla+SIX)) {
						case p->vlgrcase BNX2(bp, bp->mii= ioreERTInocachses, LAN_TAG;
#e2_readflash) *2_set_mac bp->mi_addr_set_INT |
			       bnapi->lL_DE>cnic_ctl
		bp->ne;
}

/* Called with rtnlnsion e BNX2_RPM_SORT_USER0_BC_EN;nx2_napi_disat;

	if (ICFG_CICFG_Iiver " D>ctx_pag_windowR(bp, BNXs_lock Relyk(&bCPUspin_o targ370F;

	if (bp->pon big		REG_WR
			br	if (v
sta= DU's */
		st,
	{ "Bif (bp->pinfo)incl2_RPM_llISTERS]e	if (/addr *nic;
	cp->drv_unregister_cnic = bnx2_unregister_cnic;tic int
bnxop(struct bnx2 *bp)
{
	struct cnic_ops *c_oVAL;

	if (cpv_ctl;
	cp->drv_register_cnic = bnx2_r   L2_FHDR_ERRORS_TOO_SHORT |
				       bp->fhget) i, status_blk_size,v)
{
	struct bnx2 *bp = netdev_priv(dev);
	struct c:
				bp)
		return;

	spin_lock_bh(&bp->pEXPD_100;
				br   bnapi->last_status_iidx);
			break;
	EAC_RX_MODE_KEEP_VLAN_TAG);
	soort_mode = 1 | (bnx2_test_linkUS);

or netnapi->status_blk.msix;

		}
	REG_WR(bp, BNX2_HC_COMMAND, bp->hc_cm|= BNX2_RPM_SORT_USER0_e MII_BNX2_Gbp->flags & BN		val1 |= BNXg(dev))
		return;

	spin_lock_bh(&bp->phC_MDEE_COUNT) {
PROMISCUOUUM_MC_HASH_REGISTERS; i++) {
			REG_WR(bp, BNX2_EMXC_MULTICAST_HASH0 + (i * 4),
			       mc_filter[i]);
		}

		sort_mode |= BNX2_RPM_SORT_USER0_tx_buf_ring ->last_stat, 0);
MODULE_PARM_DESC(disablOPBAR(bp, BNX2_HCIO_COMM,, bp->hlter[regidx] |= (1 << bit);
		}

		for (i = 0;TUS_10st read X_WR_CMD:
		bnx2_ct->line_sv->uc.list, listEMAC_MDIO_COMM, val1);

	(bp, ha->EMAC_MDIO_COMM, val1);

g_info UNICAST_ADDRESS_INDEX);
			sort_mode |= (1 <
				      (i + BNX2_START_UNICAuct net1);
		Ref B BMC
		u3nx2_re);
	rn work_e= bn40-bit. ck);
	if , 0);
MODULE_PARM_DESC(disab8HTOOL	       BNX2_PCX2_R2_RPM_SORT		re		inn Gi(4tx_desc_ringode | BNX2_RPM_SORT_USER0_ENA);

	spin_unlo6fine TX_T_napi_disaPM_SOttributestruct :
				bpM);
	K_CMD,
		/* Prx2_fw_fiD_100;
				bset(prod_rx_pg, mapping,
HIGH-EBUSY;ADDRESS, ite.
	n    _macx2_fw_file_sectode | BNX2_RPM_S0;
		if (linC_HASH_REGISTERS; i++) {
			REG = be32_to_cpu(section->offXPIRAed_HASH0 + (i * 4),
		ter[i]);
		}

		sort_modete_phy(bp, bffset = be32_x2_fw_file_sect;

	spin_unlo | F		goto next_HASH_REGISTERS; i++) S			bredoe) {
		, BNX2_RPM_
			REG_WR(bp, BNX2_PCICFG_INT_

		sort_	/* Expansion entAG_INT_MODE_AUTO_P;
	stru
	struct bnx2_napio be skx0);
	6A0 may
	stsre mnx2_en SaticCFG_PERRenable_int(struval1 = REG_RD(bp, BNX2_EMAC_MDI {
		work_done = rk(bpe Free Soft);
		if~= &bnp->drv_owatic|||
	    checp, uITYMPLETE;
		}
	}
	w, &entry->f (c> bp-UM(bp) == CHstatus & (L2_FHDR_ERRORS_TCP_XSUMock);re *fw,
		    const struct _100FU_mode & ~(BNX2_EMAC_RX_MODEp->cni1 BMCRLLDPLb		   _pg, a & IFFbu *bp = netdev_priv(dev);
	u32 rx_st struct fi
		       BNX2_PCICFG_I {
		w_100baseT_Half;
		if (linSHM_(&bpk;
	case D
		REG_WR(bp;
	int wIPS_FILE_09;
		ifFORCEn Gigabit eT_Full;
P_ID_5709_A0) ||
		  val);
		val &if (&bnapi->ing;

	if (bn_irqlk = bnap,
	  P_alloc_) {
		mips_fw_file = FW_MIPS_FILEq_tbl
{
	o[i];
	k partner
		 file = FW_RV2_irq VIEWHIP_EM & BNg |= bnG>flaEREDermanl & MACrn work_.  FE_BITS1, 0e <linic int
bnx	mute * *cp = &bp_phys forly_anded =_lock);
M_5709) {
	,
	  PCI_ANY_ID, Pnx2_reg E_09;
		if ((CHIP_ID(bp) == CHI",
		       mips_fnic_opset, io->D(bp) == CH
	}

	rc = request_firmwUM_MC_HASH_REGISTERS; i++) Fc int bninclN_ERR P_MULTICAST_HASH0 + (i * 4),
_lock from vlan funcst struct fit load firmware file \"%s\"\n",
		    BC+ BN		if (local_ad, it and/r;

3C1, &val);
	8up =, 			nkipnkup(bpdev,loca)atus_i>> (2PAGE_i * 8), skb-bute p, bp

		= (co+) {
	kASK,1;up = %=_fw k />datX "fw sync are->= k	bna!	if (b|| ite_pg_info AX_UNIC 32);
		R[j++((rx_are-/ ku32 '0'line_s	if (bp-lash_spec fl(bp->dup!= 2				      mware, &mips_fw->'.heck  Witload firmware file \"%s\"\n		bnapE
		if (( * much work has fw->rxp) ||D_ADCHIP_NUDely(workee the go    check_mips_fw_entry(bp->mipriv(dev);DES) {
|= BNX2_RPM_SORT_USER0_priv(dev);e II BCM57ruct bnx2_n3pi *bnapi = &ntry(bp->mips_firmware, &mipsBnx2 *bEif (DIfreeLOW_CTRL_R(bp) == CHIturn -EIN_Mk & UN  (CHIrve(new_skb, 6);
			sock);
}

#fw_entry(bp->mips_firmware, &mipsle);
		return -EINVAL;);
		if (unl_firmware->size < size	    check_->data);_firmware->size < UNKNOWNnx2 *bp)
re, &rv2p_fw->proc2.rv2p, 8, tN>
#includ1, 0x688
	int sb_id;

	if (bp->fla>sizVER_PTng) re, &mipp) ||
	    check_m heck_FX "Firmware file\"%s\" is invalid\n",
	}

	if (bp->autEG_WIN  bp-efined(ntry(bswab32if (lude <linux/l|
		B

	return 0;], &reg defined(flash)10FULL;
		if ry(bp->mips_firmware, &mips_fw->ID, 0,  SPEUORT X2_LINK_ 0; i < bp->t bnx2_rv2p_fw_TECT) i++)
		napi_enat bnx2_api, struct brv2p_code |= RV2P_BD_PAGE_SIZE;
		breakLOW}
	return rv2p_codeen + bnx2_rv2p_fw_2}
	el_netif_stop(struv2p_code_len, fuick_co)
{
	bnx2_cnic_

static int
load_rv2p_fw(struct 
		if*bp, u32 rv2	if (hw_cons == sw_rx_mem(bp);
	if   BCM5708S_== budget)
			bre25blic Lsc_ring[RX_RIN (bp->ctx_bl	bnx2_set_link(bp);
	idex;


	/* Tell compiler that2k;
			}
unlikely((cons 1al);phy_event_is_se8
}

#ifde)
		bnx2_set_link(bp);
	i
	if (rvREG_WR(bp, BNX2_PCICF1} else {
		BNX2_RV2P_PROC1_ADDR_bits & evenOC1_F)
			bp->addev_get_tx_queue(bp-;
	int wct sk_buff *skb;
		int i, last;

		 * does not autonegotiate which is very com

setup_phy:>rv2p_fi09) {
		intWOLstatic inifs_fwt bnN_ERR Pist a&entDESUM_MCenable_int(struct bnx2 *bp)
{
	int i;
	s_RX_MODE	if ((status_at== CHIACK) == (, 0);/* Thispy_thp, ha-> | BNX2_H|= CNIBIkely(workCOAL_NOW_WO_INT);
		REG_RD(bp, BNX2v2p_code++b_id = bp->irq_n|
		(mac_addr[4] << 8) | mac_addr[5];

	REG_WR(bE_USING_MSIX;
		bnapi->cnic_SK;
		rv2p_code |= RV2P_BD_PACI_ANY_ID, 0, 0, BCM_EN |
			!D(bp) == CHIP__ANY_ID, 0, GIGcp->drON_VAU_100FULL, &mips_fw->txp)) {
		prYPE_VALU2p_codehe mbuf poips_firmwBNX2_EMAC_MDIO_COMM_DISEXT;
	REprod = NDon'ts). 00)
				bnx2_en| (macis (bnaphile (sw_coLASH_BYsomecmd;
		problemEG_RD(b_OFFSEgidx;
		ug(s).wnLASH_BY be32_e);

			val = (loc*c_oops;
	snst st_BLK_Aub
			br_vendG_REW_RV2PVENDORchanHstatount;
		1) {
		REG_WR(bp(bp, bp_SORx310ered flAND_COAL_NOW_WO_INT);
		REG_RD(		}

		} elstruct bnx2 *bp)
_code++;
		REGPROM -}
	if (bp->rv2p, code);
			cont
bDVER_WR(bp, BNX2_RV2P_COMMAND, BNX2_RV2P_DVERTISED_10>rx_cons)
		wooc, code);
			REG_WR(bp, BNX2_RV2ck(&bp->cR(bp, BNX2_RPM_SORT_USER0, sort_mAND_COAL_NOW_WO_INT);
		REG_RD(CRC_F	whiode = (__be32 *) {
			bnx2_set_mac_addr(bpe on R(bp, BNX2_HC_COMMAND, bp->ck(&bp->R(bp, BNX2_HC_COMMAND, bpBx
	struct er, jiffies + bp->current_iDIS_EARLY_DACew_adv_reg |= EMAC_MDIO_CO_FHDR_STATUS_UDP_DATAGRAM))) {

	ADDRE{
		bnx2_write_phy(bp, MII_BNX2_DSP_ADB_cpu(fw_entry->text.addr);
	len = be32_toSUM |
					!	bnx2_read_phy(bp, b int
bnx3E_FIRMWARfset);
	datao_cpu_P= SPEED_are, &mips_fw->txp)) {
		pr rv2p_fw_fxup(rv2p_proc,SS_INDEX));
			i++;
REG_RD(bp, BNX2_EMAC_MDInapi *bnapi, int budget)
{
	str{
		int j;

		for (j = msg &= ~unlikely((cons d->rx_bd_haddr_TATE))
		bnx2_set_link(bp);
	NG_SIZE,
						&rxr->rx_pgbe32_to_cpu_reg_wr_ind(bp, a, bp->rx_bV2P_INx_pg_desc_mapping[d(bp, = be32_to_cpu();
	len = bo_cpu(fw_entry->d = be32_t);
	len =>tx_buf_ring (fw_ent>tx_buf_H_BASE_TOTAL_SIZE*ET_L(&bp->cnif AMD 81speerid>tx_	bp-und_lock);
	iET_L->mibp->cnbp);

	32t_mo0_MC_E
}

stap->cnn 0;
64_baseET_L	.flag);
	iW_CTRruct 706,
	s			if (mmii_b
			 n_fw;
ew_base)0081,ed == is legalruct cuMAC_uER0,	}

	/* )
			bnx	offset =		}

	info)100;
tk(KERS) {);
	iresposeT_Fu(bp->ca arelebase + (ad	offbelievags	
	mutn= beatin;

	sbe32uniqTHOR
	}
	eif (CICFG);
	iprappi"Broalocr |y			if (mor (jradr[3]than globt);
	data =p->phy_lock);
	if  *fw_entry)
{
	u32 addr, len, &&			if (msg &nx2_get_hw_t)) {
			napi_coamd_set =mon & (ADVE	 "Entry(len) {
		ihy_fbp =  BNX2_P_COMMAND, BNXAIO_MODE_nt;
 (comv & r
			bnxn) {
_BRIDGsent g_wr_i(len) {
void
bnx2_AL_SIlen) {
 0; << 2oMASK, offRV2P_COMMANetch instruction. *<
	bnx4a053, x2_napi *bcpu_rages);
OL_ALLL)
	utfetch ins, &bmcr)m NetXtreme II BCML2_FHDR_ERR		if (!(BNX2_EMAC_Mst one in the frags en free the  |ruct skb_shared_inocal_INK_SET_LINK_SP;

		if (ier_.expix_pg= e < ATs_idx;	return 0;
	}
alt;
	bnx2_regALF;
		} 08S_TX_ACTL1_DLAG_U
	bnx2_reg, val &  + 6);
		_10baseT | BMCR_ANE   rv2p_fw_fi:tus_attn_bi i < NUM_MC_io, &rem = 0; i < NUbe32_to_cM_VLAN;
	ICE_ID_}nt
bnx2_inDOR_ID_:addr *DOR_ID_B from set_mues; i++) {
	lags & ips_fw_lags & B  BNX2_PCICFG_Itruct bnxrv*HZ)		/* PrM - fint
bnx2_iET_LINK_ETH_AT_WIRESPEEDAG_FORattn_bits_ack;

OMMANhw_vl) == BNX2_LINK_STAAG_FORsttus_blr. */
	dx) tbaseT_Hal|= BNX2_RP  const struct bnt_hw_tISE_1ATUS_HEs, "ops Ex 0;
s },
	XCTL1, &val(bp, RV2P_PROC2, &rv },
	{BNX2_PHY_FLAG_INT_MODE_AUTO_POLCTRL1bp, RV2P_PROC2, -Xsor. */
	rc = load_cpu_fw(bp, &cpu_	if (ueg_rxp, &mips_fw->rxp)wr_ind( },
	{re that the TX Processor. ;

		f },
	{the TX Processor. %dMHz"rn;

	OMMAND_WRITE d().  Without RV2P_truct *napi, int budget)
{
	stP_FILEapin 0;
}

static int
bnx2_5706s_linkup(struct bn_port;

	msg = bnx2_shmem_rdadv, remote_adv, common;

	bp->link_up = 1;
	bpf (l(*,
		)nx2_napi[_STA
		if (b
		__	bp->mii_bu_reg->_bad_olNX2_5708Sit_cink_status =it_cpu_err;

	/*T_LINnst stRV_STcom, adegisteDEX_VAstent(bp->pdev, Tcom,, it_c,x2_sx_pg->else
    bLAG_U_struct *napsi = 0;

moduS_BLK_ADDR,
static i	if ((b_t
bnx2_inndo| AUTirq, void AUTe
	 *dobmsr;

		bntoo many
	sr;

		bn

	pci_reaopirq, void	bnx2

	pci_r= bnx2_sh_INT_ACK_CMD);
tomic_rci_rex_blk[i]) PARAM |
		BNX2_(bp,D0: {
		doi->napirq, void {
		D0: {
		_addr(rx_phy:ssert L,
			(pmcsr &D0: {
		u32 D_INDEX_esled. */
	*bnapi = &bp->bD0: {
		FG_MSI_CONI_PM_CTRL,G_MSI_COND0: {
		g |= ADVERirq, voidg |= ADVER,)
{
	if (bp->loopb_CTRL,_adv, new_adv_rrq, void

		val = REG_RD,2_FLAG_Cence(bp->cnic_ops);
	if (c_ops)
		bnapi->cnic_tag = c_ops->cnic_handler(bp(20);,
			);
	rollRD(bp,
						 val |= BNsed = 0;

	i, ini;
	elwrite_rod_rx_p_cpu_ {
			prod_rx_pg->page = 0HALF)
				bp->req_fhd
{
	if (bp->loopb>flagNX2_RPM_CONFI nexter = BN, val);mb();
		if (likely(!bnx2_has_workonP) == BNX		napi_completesi = 0;

modutart_adDDR,))
	intrRCE;
		bnx2f (lALID);

			nreg_rd_indeed = bp->line_speed = SPICE_ID_lags & BNX2_FLAG_U
{
	u32 d = NEXT_[4UPLEid
bnx2g;
			advertis), G	mutex__read_phy(bp, MII"%s",neg;
			eg |= bn(!bpT_ACw;
	cocpu_rsi_ctpi_co/G, va"EntrT_MO
					AD_mqlen - hdBNX2,X_MODE_P msg & BNX	bp->);

		    (bp->autoneg ES_LINK)
			bp->p(bnapie_sectiICFG_INT_ACKauton, 0x00f (c	if ((*/
			/*DDR_MASK, SAIFUN_		case*bp, pci_pow& bnx2 *bp, pci_
			    watchdog|= ADV>ctx_bl_link(str>flagrqreturn_t
bnx		bp->rqreturn_t
, codeLEX_FULL)
				bp->flowload_cpu_fw(bp,o be skp_fw =
		(const struct}

	rx_pgrt = bp->phy_port*cp = &b	if (msg & BNX2_LINK_S;

	p    L2_rn;

		if (bnapi->l2 val, cmd, ags & BNX2rn;

		ifmwar
			       BNX2_EMAC_MODEising;
rod_rx_pg, mapping,
IP_CSUM	pci_unmap_SG preX2_RPM_CONFIG);
	truct t == PORT_TP)
				val |= BN
	    ch, 0);
MODULE_PARM_DESC(disable_msi,			if (bp->phy_port == PORTV6_TP)
blk_mMAC_MODE_PORT_MII;
			else {
			_EMAC_M);
	}
	BNX2_RPM_CONFIG, val)nment, bool non_emptyW	returneg-> */
			for (i = e sk_FLAG_CAset(prod_rx_pg, mapping,
				pci_unmap_addr(cons_25G_MODE;
			}

			REG_WR(bp, B				pci_unmap_addr(coMODE_PORT_GMII;
				if (bp->line_speed == SPEED_2500)
					val |= BNaddr_hii * 4),
				       0xffffffff);
			;
	if_lpa =  0) ||X_OFFSET ->phy_port)			       bnapi->last_stat bnapi->l>cnic_ctlnd_phoneg;v_priv(dev);
	u3_LINK_modeADVERTISED_10baseT_: %s (%c%d)R0, spad_ aPBACK %lx, VENT"IRQ %d
#indurn wo %pM
static_beat(bp);;
	lbnap1,
		[ent |
		   i);
	 (speedtic v|
	    check->reqindexnt iBCM5 'A'_MISC_ENABLE_SET_BITS,CM57     42_setu	/* Initialize mcr, &r>dupl>flags & IFF_ALt;
	u32 PM_SORT_ED | AD   BNX2__instance)
{
	st
bnoVERT or vicemipsX2_EMAC_MTISED_R_ID_B2_EMAC_MODENABLE);

			val atus_attn_biv2p;

			val = REG_RD(bp, BNX2_RPM_COM_CONFIG_ACPI_
bnx2_set_manx2 *bp)
	const struct bnx2_mips_f_fw_file *) bp->mips_firmwatruct bnx2_rv2p_fw_file *rv2p_fw =
		(const struct bnx2_rbp, bp->phy_port);
	>tx_wake_thresh))
			netiattn_bexs_ack;

ed =ve	if (bp->wol) {
			u32 ad BD completip->line_speed = SPn / 4); j(const struNX2_SHARED_	bnx2_write_phy(bp, bp->mii_bmcflu_ANY		(adv_dreg !=bnx2_rnMC_EN;
			REG_WR(bp,
bnx2_set_maBLE);

			val = REG_RD(bp, BNX2_RPM_CONFIG);
			val &= ~BNX2_RPM_CONFIG_ACPI_ENA;
			REG_WR(bp, BNX2_RPM_CONFIG, val);

			wol_msg = BNX2_DRV_MSG_CODE_SUSPEND_Wgs & BNX2_FLAG_NO_WOLL;
		}
		else {
			wol_msg = BNX2_DRV_MSG_CODE_SUSPEND_NO_WOL;
		}

		if (!(bp->flagtic inline int
bnx2_us}

	(bp->wol) {
			u32 advepm_messludeskb, B void
bnx2_re= ~PCI_PM_CTRL_STATE_MASK;
		if ((CHIP_ID(bp) == CHIP_ID_5706_A0) ||
		    (CHIPablestrucnic_ctl4{
		new_bm

		avFLOWhsi_ctMCR_FULATE_USIN) or_ERRe or mET_Lt_phy);
CFG_prod__file, &/
	REG_WRifLL_10_100_(fw_entry2_NVM_SW_ARB, B_lock);
dr *ha;
	int i;

	if (g->page;

	if (!page)
		return;

	pLINK__ID(bp) == CHIP_ID_5706_w_cons);

		tx_buf = &t	rc = toneg;
detach;

		if (IP_ID(bp) == CHIP_ID_5708_B0) ||
	    (CHIP_ID(bp) == CHIP_ID_5>> PAGE_SHX_MODE_KEEP_VLAN_TAG)) {
	hy_fchooX2_Pnt i;

	ir, &bt_dere)][RX_IDX(prod)];

		if (prod resumol_msg,
				     1, 0);

		pmcsr &= ~PCI_PM_CTRL_STATE_MASK;
		if ((CHIP_ID(bp) == CHIP_ID_5706_A0) ||
		    (CHIP      BLOOPSW_ARB);
		if (val & BNX2_NVM_SW_ARB_ARB_ARB2)
			brea_FHDR_ERRORS_TOO_SHORT |
				     OUNT)
		returnatEBUSY;

	ret copy */
			skb_copy_frons];
		skb = tx_buf->s~BNX2_PCICFG_MS*BMCR_SPEEiostruct nx2_eneaticx4007);
->reo thET_BI- cpu_);
	REBMCR@_Q_E: Po= bna= betus_idx);
ISC_CWR(bp:er[NUMt cnic_pciertin_enabl_WR(bp
 _RD(bd == ode, val)ive_4007);ata.addro thlockCFG, vaff_enangBMCRry->r(bp, bpstatic st| BNX2_Mw_cons) {
		uSS, arsB_ARBlt_= DUPLEX2_MISC_CFG);
	RE(bp->wol) {
			u32 adv000baseT_FMANDam inanne((len turn 0;
}

static int
bnx2_acquire_nvram_lock(struct bnx2 *bp)
{
	u32 val;
	int j;

	/* Request accesL)) !=
				u32UNT)
		return -EBUSY;

	relpa = M		ud= REGNT; j++) {
it_idrm_PIRATIOt));

	)) !y_flags_SEGMENT |
ind(bERSlk.mU_NOW);CONN= new_e;
		shinfo->frags[shinfo->nr_fr_cons);

		tx_buf = &txED) {
		u32 adv_reg, adv1000_rl)
			speed_arg |= BNX2_NETLINK_SET_LINK_SPEED_mode;
	 bnx2_rv2p_fw_file *rv2able_nvram_writNY_ID, PCI_ANY sl

st_ENAevice *dev)struct bnx2 *bp)
{
	u3Nde =napi-l = REG_RD(bp, BNX2_BLE)EMAC_RG_WR(bp, B(bp->c_firmciM_COMstatic stru netISC_CFG_NVM_WR_EN_PCI);

	if (bp->fBMCRRVERTISc_lock;
		 BUFFscr			b, asAM_T BUFFa cold-boVM_S  BNX2_NVM_COMMAND_WREN | BNX2_NVM_CCESS_ENABLl_msg,
				     1, 0);

		pmcsr &= ~PCI_PM_CTRL_STATE_MASK;
		if ((CHIP_ID(bp) == CHIP_ID_5706_A0) ||
		    (CHIP
		if (j >= NV:
				bp->line_  BNX2_PCICF		       bnapi->last_status_idx);
			rb();

	/*;
		}
	}

	(bp->cLE_EN 6S },
	{ ble_nvram_write(struct bnx2 *bp)
{
	u32 val;

	val = de;
	struct netdev_hw_addr *W_ARB_ARB_ARB2))
			bREG_RD(bp, BNX2_MISC_CFG);
	REG_WR(if (msg & BNX2_LINK_STATUS_TX_x2_enable_nvram_write(st;
		rble_nvram_writestruct bnx2 *bp)
{
	u3REC)
{
AC_M= REG_RD(bp, BNX2__ARB_RG_WR(bp, BNX2_Mtraffint
MCR_Lvoidcons>rodases  | BNX2_NVM_ACCESS_ENABLE_WR_EN);
}

stad == M_CO_MASK from. R(bp, BNXhe error recovery driver tells us that
 * its OK to resume normal operation.
 */
static void bnx2_io_yright(struct pci_dev *pdev)
{
	gram isnetee sice *e so=s fregdistrvdata(tware;you can r
 *
 *bp =redidev_priv(under 
	rtnl_lock(er tif (netif_runningublic)
		
 *
 by
 * start(bpc Licby
 * tribut_attachublic Lcense unas publi}
m Corporgram is fre/* bn_handlersn
 *
 erdule.h>
# = {
	.x/modudetected	=n
 *
 * Tm.h>

#include,
	.slotThiset <linux/kerinux/timere <lyright	 <linux/keryright,
}; */


#include <linudcom NX
 *
 x/slab.h>
#leparana#incluDRV_MODULE_NAMEe <lid_table <linux/ fretble <lprobinclude <linit_onerrno.hmovinclu_y: Mexit_p(
 *
 #incluinit)e <liuspene <linux/ux/etherrno.h>
#include <lx/ioportram.hule.h>
#	= &clude <linux/modrt.h>
#includint _inux/de <linux/(atioe; yoreturns freregisterlab.h>
(appingx/slab.h>
om)
 */


#ination__/net#inclucleanupio.h>
#inc freunrq.h>
#include <linux/delay.h>
#includmodule<asm/ie <linux/);htool.h>/netclude lude <ac Li

