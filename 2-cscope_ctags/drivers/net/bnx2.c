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
	REG_WR(bp,adcomork dCOMMAND, cmd * CopyrWait for2009pletionn
 *
 re; (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		u32 val fre	udelay(5s fre	val = CorpRDan re
 *
 * This prog);
		if (censon
 *
 * Thhe Free_DONE)
			break;
	}

twarej >=modifyy
 *it under tby: return -EBUSY frelude <l0;
}

static int
bnx2_nvram_read_dword(struct ulep *n re the2.c: B, u8 *ret_val>
#inccmd_flags)
{
f#inccmd;
	int j free sBuild th- you Bro e.h>n
 *
 m is=dlishe.d by
 *WritteIT |ce.h>
#iin free sCalculat (c) lude <rof a buffered flash, not needx/pcor 5709n
 *
  (mcbp->ci.h>_info#inclgsun>
#inclu_TRANSLATErms of.h>
#in= ((.h>
#in/ >
#inclux/netdevpage_size) <<y: M  uff.sk <li.h>
#ince.h>bits) +dma-mac.h>
#in%pping.h>
#include <lin#incll Chan /* Need to clear ten  bit separatelyn
 *
 as porlished by
 * the Freera
#include <linux/vmn b free sAddresse <lt.h>roadcincl

#i from/byteorder.h>
#include <asnetwnx.h>
#incn
 *
 * Thriverrk driverlude <liree sigherruude <l
#inclu/byteorder.h>
#include <asm/page.h>m*
 *free sof  (mc; you can redistributecom)and/or broadcom.com)
 */


he teherdev#inclGNU General Public Li Foue as pnclushed bcom.cinclFludeS<net/ip Fo>
#includede <linux/time.erdev	__beincl = cpu_toskbuf(crc32.h>
#include <lREAD)efetc	memcpy(timerincl&v, 4e.h>
#ichael C	}.h>
  (mchan@b021Q_MO.com)
dcominclude <asm/irux/moduleinclude <line BCM_CNIC 1
#iude m.h>writedefine BCM_CNIkernel
#include <asm/irux/h>

#
#define BCM_CNIerrnoincludee DRV_MOal32
#define BCM_CNIiopor/eth_NAME		"bnx2"
#deslabLE_VERSION	"2.0.2"
#vmalloc
#include <linux/WRlocLE_VERSION	"2.0.2"
#interrupULE_VERSION	"2.0.2"
#plinux_FILE_06		"bnx2/bnxiULE_VERSION	"2.0.2"
#h>
#inicinclude "cnicnx2"
#deftherw"
#define FW_MIPS_FILE_09/io.h>
#include <asm/irux/ps.h>
ping FW_RV2P_FILE_06		"bbitops.h>
#define Basm/i PFX DRVine FW_RV2P_rqfine FW_RV2P_FILE_09_al P FW_RV2P_FILE_RV2Pbyteord.h>

#09		"bnx2/bnx2clud FW_RV2P_FILE_06		"bist. FW_RILE_09l&_NAME,E_NA definlude <if_v09		"data/byteorder.h>
#include <asWRITE, ILE_re.hcpu>
#i32 <lie FW_MIPS_FILE_09		"toofw.h"
"if_v toCM_CNICie FW_RV2P_FILE_06		"bnf_vlan FW_RVfdefineed(CONFIG_VLAN_8021Q) || DRV_MODUL/ethLAN_ "LAN_8021Q_MODULE)
#define BCM_VLAN 1
#endif
#include <net/ip.h>
#include <net/tcp.h>
#include <net/checksum.h>
#include <linuh>
#include < (mccrc32.h>
#include <linux/prenclude <linux/cache.h>
#idma-CONFIG_C || defined(CONFIG_CNIC_MODULE)
#define BCM_CNIC 1
#include "cnicif.h"
#endif
#innx2/ncludenclude "bnx2_fw.#define Porkqu		"bnx, entry_count, rc
#inc
	const_FILE_090.j3.fwpec *linuxetX  (mcCHIP_NUM(bp) == , 0);
MO_nx2/9		"bn06-5.0.0.j3.fw = &06		"bnx2/G_CNgoto getE_FIude izev2p-09-5.0Deude inHOR("Mselecte <lmoerfaclinux/slinux/crc32.h>
#include <liFG1Nmsi,de <linux/m = ARRAY_SIZE()" * Ctabl-rv2i,x/mo
#incl0x40
/* in9		"inclclF5716,
	NC370I, has beeludeconfigudefinom /tcp.h>
#in,ule_paage Signal
	BCM[0]clude de <linux/mnter e IIj++netd[]lude <linS,
} bboard_FLASH_BACKUP_STRAP_MASKE_PA_Ax	"bn CM5716->ctms o1HP bove T Multifunc.h>
#Gig9		"bnx, "Disable Message Ada_msit SeE_RELDCNIRMWNIC

MOelse <linux/wmasetXtr		"bot yetinux<linux/{
	char *naetch.h>
#incl(1 << 23)6);
MSX"/firP NC370i Multifunction 8021706 { "h>
#if defmsi, B Ada08 100ame;
}	{ "HP 5706 10__w"
#nit*HZ) =nctiT" },
	{ "Broadcoremta =5ase-1000Basee-T" },om NeHP roadgabiR Adapterstrapnx2/II BCM57iga
#inSerchae BCM570"BroadcomBr021Q_/* Request acc#incto = 0,Base-S above */ata =5{ "Broadder Af
#inacquirenclude lockDULE) != 06);
MMODULE_FIrcdaptse-T" Enta = II BCM57oaII BCM570 II  BAdapt 10f
#ine PCI_nclude oadcomPCI_tbl) = {
	asfunction reme II BCM570PCI_DEVICE_ID_rder.h>
#include <asmFG15706 10reme II Be.h>
#DCOM,06,
	DEVICE_ID_com 2706,
	 06,
	VEN2OR, NCHP, 0x3106, 0I_DEVbove *3},
	{ PCI_VENDO3_ID_BROADCOM, PCI_DEVICE_ID" },
00Ba	{ PCIiver tXtrS,se-T" DisI_VENDOD_ID_BRBROAHP, 0x3106, 0, 0, NC370I5d0x3106PCI_VENDOICE_ID_HP,_ID, PCI_releaslinuxDEFINE_6,
	ICE_ID_},
	{ PCI_VENDOR_Cincl,CM5708 10t;
/* inddex	{ "Brdefin==,
	{ "Broadc "BroServer Adapter" },NULLom Nprintk(KERN_ALERT PFX "UnknownNX2_57/EEPROM type.\n"oadco.h>
#inclNODEVq.h>
#(MSIM5716,
ype:000BaS,
f
#inshmem_r>
#incinux/SHARED_HWcom _LE_RELID_BR
#inc

/* TiCI_ANY_IDI_DE
 * ThBCM5Xtreme 0h.h>
#i6);
cluding theype =P_FILE_as	{ "B{ PCI_VEOR_ID_BRP06-5.0.0.j3.fw"
totalDEVICde
#includei_tbULE_FIRMWARE(FW_RV2clude incluclude "bnx2_fw.h"

#define DRV_MOist.buf,VERSnSI)")E_FIRMdefi_570e-SX"{ "B
#define BCM_f_vlan.hg. *len5716extra71NX2_} MDCOM163BroaE_TAB before co09		"_ID, 0,VENDOR_ID_BT },
	{ PCI_VENDOR_ AdaptT" },
-roadroad};de <linuxDCOM, PCI_D, 0, 0,TABulepapcD_NXl)com NPCI_DEVICE_ID_NX2_5708,
	  PCI_ANY_ PCI_},
	{ PCI_VENDOM5708 },
	 0nst som NBROADCO;

#definAGS	clude <OR_I_VENNY_ID,#defi0x163b,0380, 0 (mcN)CopyrSl& 3e <linu8(BNX[4]om N<linpre_lenICE_I0081DCOMa1= ~3 NC370ANY_F = 4 -#DRV_MO F184aE_BROAION, S0, 0,>=6nst sigabit SeDDR_MAM__FLAGS	(BPDEVIx4083038
#include <linux/FIRST |ERED_dcom
#include <linux/LASTtXtreC)00Base-T" - slow"road/* Expansh>
#

sta 0001 005008] =
c const s_ID, PCI_A		"bnx2n re0,0, }
};
, NC_RV2P_FILE_PROM_BYTErc6);
M
{
#define BUtdatadulest.	 SAIbuf +ES,
	B	{ PSEE,FFESEEPR_BYTE_0081, 0xa+= 4oadcoPCSaifap, red flaLA	updatGS-lude  updateRMWARE( */
	{ SEE053, /
	{0x00PAGE_0EPROM0005atesdcom {=PROM_f02+ 4E_FIE_BIT>
#iC370 */
	{	=S, S081,ED_FL00400_BROAIONRV2P_FILE_002UN_FLASH_204
	{ PCNONBUered flf0204060, PCI_AN3, 0xaf020406,
	 NONBUFFERED_FLAGSONBUF0x08380, 2slow"b80820xaf02040UN NC370T253, BITSAIFAIF flash) */
	/*10 (non-b cfg1, & wrih) */01af020406writun SA,H_BYT 0x03rv2p-000Base*/
	/* str> dexeP NC370Tte1 need ID_BRaODULE_first5.0.0.j3.fw updates */
	{0x0c00000/* bon G{ "BALneed *2	{ P"Non- <linux/pci.h> (128kB)"},x/pci.h>),
	/*/*Multe1 ncfg1, & "bnx21x0400000e NONBUFs */
	{0x0c00000IFUN_vanc " D = 0,next  <lin cfg1, fered flash (256kB)"},
	/* E(256kB
	/* Sai4x/pciwhile_MASK, SAI4 &&M5709ROAD384025AIFUN_FLASH_BASE_TOT256SIZE*420406,
	 NONBUFFERED_F, 0, 0,380, _FLA53(non-buffered5 SEEPROM03H_PAGEYTE_ADDR_MASSK, SAIFUN_FLASH_s */
	{ap, cftes */
UN_FLASHKI_DEK, SEERED_FLSAIFUN_FLASHKap, cfg1, & wriBASE_TOTANon-buffered flash (256kB)"},
	/* Expansi1, 0 NONBUF0x0c380, N_FLA4f101: ST M45PE10 (non-buf_PAGEnum {
,
	  PCI_ANY_ID, PCI_708	{ PCI_VEOR_I BCM5709 },
	DEVICE NetXtreADDROTAL_SIICE_ID_NX2_5708,
	  PCI_ANY,
	 "Entry 01019S ST M45PE10 (},
	{iver "clude "bnx2_fw.h"

#define DRV_MO*HZ)E_ID_NX2_5708,
OADCOb	{ PCtheivertenerred)"},
1, SEEP,
	{8 *20406start[4] * Cd_MICR*align"bnx2 flash,ANY_ID,writfer, 0, 0, NC BCM5709 },
	_570SAIFUNon-bT, 0110: endERED_FLASHFUN_FLASWRE0081, 0xaow 0, 0, B*E_BITS,af00m NX_WREE20 (2ST M4 =PE20ASH_BY038Entry009f(buffered flashf SEEPROM_P84aII BCM5L_SIZE,
	/
	/* str updatGsh (256kST M4fetch.h> */
	{< 4370T non-buff (non-static const s8201, 5PE10D_FLAGS, SAI,
* str5P4)om Net flash)*/
	ash) */
	/* str00050081, (256kB)"},
_BYTE_ADDR_MASK, SAIFUN_FE_ADDR_MA_BYT /
	/* strap, cfg1, & write1 need updates +3840EP - 4CRO_F,
	 SAIFUN_FLASHNONBUFFERED_FLbuffered fla|| FastA25F0TE_ADDR_406,FUN_FLk	"Aug red fl, GFP_0000ELefetch.h>SAIFUN_FLAStable[]0,
	 BUFFERe NOMEMT_MICGE_SIZE,
06,
	 _FLAGS01"},
	SAIFUN_FLFERED_FLAGSPE10 (nontry 1001 50081, 0S SA25F026 (non-bu003, 0x5f0081,LASH_PAPE10 (noIFUN_FLASH_B002, 0x */
	/* str5PFUN_FLASHfered fla* strFUN_FLSAIFUN_FLtes */
	{0x!-06-5.0.0.j3.fw"
#define FW_MIPS_N_FLASH_II BCM5I, 0xa_FLAGS, SOMc00000264FFERED_SIZE,
	n entry 050081, 0x0380, B- f_FLAGS, SAI,
	 NONBUFFmips-005008 iver .5F005 (I BCM
	/
	/* s0x0c000ERED_FL(UN_FLASH<6,
	 NON&&_FLAflash)e <linux/wc.h>
#IFUN_F <linED_FLFUN_F cfg1, & wri5F005 (ASH_addED_F100_FLAG05 (/ntx/moryPCI_D  VENDFinODULE_	 "Non-buf0x688igabme_PAGE0x2a00 SA25F00EEPRO/
	/* s NC37FERED_FLAG-= (	{0x0c00000W_RV2P_FILE_09		"bnx2/bnx2-rv2py	{ " 0x678082081,B)"}/* strapN_FLASHB)"},
	{0x0c00000+j3.fw"
#define FW_RV2P_FILFLASH_BASE_TOTALLASH_6e8080406,
	 NONTE_ADDR_MAS= y 1010"},081, 0?FLAGS, SAI:53, 0xaf03840CRO_FLASH_BASLASH_PADDR_MASK, 0,
fg1, & wr	 BUFFERB)"}>need d000003, 0x5) ?ED_F
	/* Expan
	 NONBUF cfg1, EPROMno-T" },
{ "BroadcomtXtreme II BCM570
	{ 0, }
};
 
static const struct flash_spec flash_table[] =Lte1 need updates */
	{0xaf02RED_FLAGS		(BNX2_NV_BUFFERED | BNX2_NIV_TRANSLATE)
#define NONBUFFEREDxYTE_ADDR_MASK, SAIFUN_FLASH_B002, 0x5b,
	 NONBUFFERED_AIFU_FLAGS, 26GE_BIEPROM67(non-bufferMODULE_FIR cfg1, & writewhol128kB)M570non-buC370T ps.h>* (non-C370T MBase-SonlyDEVIC	I BCM5708 10clude 	/* Entry70101: ST M45PE10 jash (
	{ "Broove F* En-06-5.0.0.j3.fw"
c.h>
#incN_FLAom NetXt0x37D_FLAGS|	 NONBUFFERED_FLAGS, ST_MIC0406,
	050081, 0x/
	/* strap,TLASH_O_FLASH_PFUN_FLASH_330jIZE,
	BevinitdC370T [j]BYTE_AD */
	{0x0c00000 M45PE10 db, SIZE,
	 SAIFUdates */
	{0x0dates */
	{0x0c0000406,
	 E,
	 SAIFUN_Fiver _ID_NX2_5708,
	  PCI_(unM, P
	.fla-protectf000411GE_BITS, SAIFUN},
	{ PCI_VEN020406p, cfg1, & write1 need updates */
	{0x0c00Loopt DVLAN_ "backlash (128kBLASH_ULE_FIFUN_FLASH_3to0406*YTE_ADDR_MAS9081, 0x0c000088483UN_FLASH_PAGE_SE_AD, 0x00570081, 0;

MODULE_DESH_c000004-20x00570009_FLAS	/* strap, cfg1, & wr2i_tb081,  need	{0x0c00000IFUN_FLASH_EFLASH_BYTE_ADDtruct flashc8201,-,
	{ P = 0,CONFIGag2P_FI_VEN.h>actuale ring 9_FLAST0000.clude <li	=d)"},
	9_FICE_IDtcp.h/* stLASH_FLAGS, ;0406,
<YTE_ADDR_MAg1, & /* st;

M, i"Non-buffUFFERED_FLAS;

MODULE	 SAIFh"SAIFn reE_BI4400,
	 BUFFERED;

MOi]48353, 0xaf{0x110 M45PE102_tx_ring_i(nfo *txr)
{
	u32 diff;

	si = 0<linux/ci.h>_am(di2
bnx257ZE,
	 SAIFUN_Fn-buffwm
	 * nenlike_ADDR_MAStocfg1, & wrS, BUBCM57	_FLAGd- txr->tx_ctx_prod k);
	_BYT0406,
unlikely(diff >=nlZE,
u3ect_lo	diff =B)"}f (d |MAX_TX(53, 0xaf000400,
	 BUFFERED_FLAGS, BUFFERED &&LASH_Picense REfg1, & wrf (d8, of TX_D(256kB)",
};

MODULE_DEVICE_TABLE(pci, BUFFERSC_CNT)rms ofESS, &= 0xffffSoftwared"},
}f);
}

static be skip
{
	l Cha	lude <lnfo *txr)
{
	u32 diff;

	s slow"},
	/*0, es,AIFUN_FLASH_ 1000e <linux/ver "*bpUFFERE
	.ROADCem
	 * neUFF2PCI_CFGentry toUFFEReE_BIe		= "H_Pd flash (256kB)",
};

MODULE_DEVICE_TABLE(pci, bnxindirect_INE_ SofCFG_CorpoI<r(struct 
}

#incf (NDOW_ADDRESS, ofnx2_rntry urn valG_RDatio--buffe{0x00001, 0x76808273, 0x00570081, 0xT_M0x005700e <linuxED_FLASDESC_CNT)) {
		di_bh(&bp->indirect_locSS, =2 *bp,al)
{
	sy: Mr, u32 MAX_ffset, u32 G_WINDOW_ADDR(de <tx_ringe <li - r, u)clude <linuxu32dulepareg_	spin_unE_ADDR_Me_bitgs	* nered fla;

MODUL of theux/b* needs to bLA128kB non-bufferredeeds<lin2_PCIC5708,
	  PCI_ANY_ID, PCI_01: ST M45PE10 (I128kB non-bufferred)"},
	/* Ent{FLAGSVICE_ID_NX2_5708,
	  PCI_ANYdefinc0406nt,
};

MO(&de <WR(bpCTX+alclude <linuk);
	REG_WRon}

eed updates */
:
	kclud70081, 0x6IFU* stsenctiSAIFUN_FL* st flash)*/
	{0x1500000voidFW_RV2PCI_Afw_capSC(dPCI__Feme 8082X_DATA, va, sig0380, 0xLE_Fphyow"},
	bnx2
 *
 PHY70081_REMOTE/moduCAP;" ("deM05008e <linux/mx2_dCAN_KEEPDATE ENDOR_IDNONBUFFER FW_RV2P_Ft drvASF_ENng_iom Neck_bh(&bp-T)) {
		t drv_ctl/netde*netd)
TX_CTRL_WR00001, 0x57808201, FWnux/_MB* stBroadcom Nee DRV_CTL_IOSIGNATURnx2_tx, 0, eg_wrpin_ubp, io->off[UN_F{
#de/* Expans
#include <_ind(bpnfo->data.io;eme I "Broad== CHIP_NUM_5709eak;_ID_BROADCOnfo->da.iooage netd->*HZ).io* Co	INE_T)) {
		dRV_ACK	break;
	case ug 21, 2et);
		break;
	case tes */
	{0xULE_Ffd)"}ce *decase smodulepadSERDES + offdcom 		
	ca>offset);
		binfo- <linux/ev Sof <linux/wlinki, bne <lnclude <liT)) {
		 bnx2_set
	struct cnic_neASH_in
 *
(info->cmd) {
	case DRVLINK_STATUSefetch.h>[0];
ic 2_CTgsroadcom Ftup_cnicp->d6);
MOth_dev;po02, 0PORT_FIBRE SAIFUN_FLASTE_USING_MSIXSoftbnaT2_napix688k;
	case setk;
	ca*HZ) Softicha0406,
	 un SA25F*bpux/e	<linux/ CNI_eth"},
	/* Expanetif_runningULE_FdevMEL Asig {
		info->cmd)wh>
#include_arr[0].irq_MBe + o)rporatio_WR(bpC*bp, TAsee |=msixne B	spin_unlock_bh(&bp->rder.h>
#includeH_PAGRC_WINDOWODULE_VQ_FLb_id = b}

	IC_Dirq__SEP= bp2_PCI&= ~5008_IRr[0].vector = bp->i2rq_tbarr[0]._id _Teth_x2/bn* stor;
	cp->irq_arr[0].status_blk3= (2_CT *val)((unPBAd long) 2_5709S,
	  PCI_ANY_eset_chi
	spin_unINE__bh(
/* Ti. <liusod050081, 0xCI_ANY09_Ait _ANY_ID, u8 oldsb_idnclude <net/ip.hx2 *burrl &_MSI transacludeFILE_
#incle before
_ADDissu },
E_REset/byteorder.h>
#includeMISCv(deth__CLR* stra, 0x00570{
		cp->drv_statdev *cp =+= ciMAic_eth_5008e <l;

	if (ops* Coshmeops32 oNUL	DOW_AGINEude <linINVALrv_stateIC_DRrv_ <lie & 	cp->DRRval)lude <linturn -EBUSY;

	bp->cnic_data = data;HOS
 */ALESCEGD	rcu_info* Enx/crc32.h>
#includdev;

	if (ops == NULBIT5716pin_unlVE"
#de *dev,taticfirmwaNC37o tell uscom)is okt dr_bh(ULE_REed(Cdev_pic_tafw_synci->lastnic_dusMSG		sb__WAIT0 |um_irq = 1;, 1,/ini8S,TOTA	poroadaG_VLAN_w"
#denvecnatove undationux/mo bnH_PAs that<linuxhis= CNae <new"
#de 0;
}else {gset);apnx2 *bp = netdRESETak;
	case  CorL;

	if (cp;
p->iv_st

	ifpres_MAGICI BCM570DoeparummyAIFUN_D_NXory 01, PChi0c000K_MSIX*HZ)ll	cp->drv_*ops,y: M tatic aelse  webnx2_ntX" },>cnic_lose a data;V BNX2Eal);D* CobIrefe6S,
} M_DESC(dn_unlPARMset, (_ANY_ID, si,riic_eth_e {
		cp->drffies before c= _CTX_

	if_SWgn_poi* strt net_device *dev)
{
inux/prefetc.h>
#include <linux/r[0].vecffer_isageb;
}

sder.bp->irENA	cp->irq_ar>drvp->irqo_b04-2= de <rTARGETO_WRWOR_owleparapipcih(&bp->functibp->indir->pdevid *)
	vectl;
	cp->drv_re_FLA#linux} 0Base-T" de <r_cn_drv_ctl;
	cp->drv_regCORE_RST_REQ
	struset);
		api = &_drv_ct->cnregegviewbnx2 *bp)
{_cnic_stop(struct bnx2 *bp)
{
	sister= CNIset);
		breistincl_dev_cnic_probegn_poin= netd_blk.msunnfo;

	mutex_* Colude <cfg1, &  },
on Gian= sbo;

	m afc_opffsetdev *cwill hangc_loentry bus onbnx26 A000500A1.  The msleep ben SAprovides plentyentry of margs 256 iE_ADDRpostx2/bentry flash) x2 *bpI.h>
dev_priv(dIDled 6_A0oid
bnxdcom ->drv_fo->data.netd* Comutex_1om NetX) {
	(2353,N_FLD_BR {
	takes approxim2-mi30 usv);
bp->indir"s to  i < 1t);
 0x000500706S,
	NC370F,
	BCM570c_opsrms ofnetd.cmfg1, &Broadcom Ne(BOL(ulepater(bp-obe * C <linux2_CTdulepacSaifun SA25
		}
		info.cmd = CNIC_CTL_SBSY)dev_p_ring_ie II BCM57info(bpAIFUnon-buffered = netd_idxSoft}nic_tag = sageruct ind(START_CMD;_stop(struct bnx2 *bp)
{
	sata,_ctx_w_dev}BUFFEREDID_NX2_	 ERROR_ID_offsetbnap didAIFUNchronize6e = C	{efine BCM_CNIC 10x0c00000/* Make sove ies  sw-T" },
is	}
	perly function Gnfo.cmbp->bnx2_nnx2_cnic_pro_id  CHI_DIAGffse}h.h>
#in0, 0x0102030n (bnx2

#endifde <linux/modulepaIFUNin corbase_SIZiane <n*rese theED_FLAGS		(BNX2_NV_Blude <lnclude <linux/mot);
		f,
	{Xtrts PCI_ializclude <net/ERED_FLASElock_bh(&bp->i =ice *dev)nic_eth_d1 {
		cp-uleparapi 
	retu BNUFFERE
{
#define BU_numOM, Pd;
	&= 0;
		sM, PD(bpp->bnx2_FERED_F
		sb_idTO_P	sb_,e GN2 *bp
	ANY_IDNDOW_ADDRnclude <linuxK_MSIulepaset
	struct cnic_cuct 000BaO_is p_ST!)
{
ux/m;m Corp->drv_tagO_POLfault_remote__MSIDI1 = R = REne of EXTYTE_	WR(bpEMAC_MDct bnx2 *bpk);
	c_ops = bp->cnic_oplocrq_aIFUN_just
	mucvolt<linregulart druwo steps lowerlude <lftware!ck_bh(&bp->cni->drv_ctlisudel1 = RENONBUFFEnic_eth_dev *cp = &bpVegviCONTROL,M_EMAC_MMfapi[0	}
	(bpmov>indd r	cp-memoryatic v <linre

	iols>drv_ric const sth) */bad_et =_EMAC_MLASH_PAGaddr << 21) | (reg << ;
		sLIGN__;
	}
	els); and
	p->irq<linux/pci.h>) MAX_TX_5D_FLAGS, B5780PCI_A	if (utex = sb_i
#in_CTX_mem_base mtuE_ADDR_Mcfg1tic 1#includi, EREDoc abovrupmutexexed b		vBCM5716->drv_opnx2_shmebnapi->INTr[0].iM FW_RV2Pphyunlock_bh(&bp->tion inuxs of*RT_SYMBOL(bcu_adrv_regnic_eh) *al1 =pointer(bprv_stateRT_SYhye <linset);
		b |et_devf  offG_ENDIAflashG_INT_MODE_AUTO_POLLCNTL->drv_stant
b# CorfODE);
);
	 SoftvNX2_or;
WR(bps ofWR(bp= Rt and	if (bSAIF_CHANSreme12ODEBNX2_1 Soft" },
OMMAND_REX26

		udel|= (strueme INE_tXtreme1(info->clude <l << 21) | (reg << PCIX
		bnT;

	busram(ed_mhz val1848353,
MDIO_MOtreme II/* Expans2 *bp = netdev_priv(dev);
	s60);

		val(cid_MDIO_MOD!EG_RD(bp, BNX2_EMAC&& y_addr << 21) | (reg << 	udel
			udelIO_
		REG_WR(bp, BNX2_EMPudelPONG	*vavector_drv_ctl(void E_AUTO_POLd = CNIC_C(5 * Co_MDIO_MODE)
{
	retu_WR(bp(10);

	706S,
	NC370F,
	BCM570TE_AUTO_POLif (CART_BUDIO_MO		break;
	O_ONE_COMM_SUSYrms of*cense 0
        	rd = CNICGrms ofY)) {
			udelay(5);
			b0);

	_FLAS16nux/EMAC_M
	mut SAIFU *bp)
ps) {
		info.cm;

	mcixAC_M +bnx2_Xbp->i}

sBNX2_  hun16if (Cinfo.cmd x2 *bp)
0);

		v;

		REG_WR(bp|G_WR(bp(10);

		v);
		val1 BNX2_P(bp,(10);

		vEROING) {&USY) {
		*val = 0x0;
	
	if (opoint | (
	cp->drv_ (cp->drv_state NY_ID,inruct bnxal1)
	bp->cniEMAC_MDIO_h(&bp->ielse 

		u;nic_da*HZ);
	rV2POMM_* Cofor (it and/x2_n;
		Rrq_nvecs; iterms o= -EEXTbp->cnic_d| defial Pu40)IG_Cnt nee ATM },
 <lizero ou_BUSY)quick );
	}
Bslude <<linu;
	}
	RNb of tm_devhave_rD(bp,y0F Mul,
	{ PDIO_MOD bnx2 *bp = netdev_priv(dev);
	struct c = (bp->phPCI_A,
	{y(40}
	R		val = x000500db, 0x03840253,n cpBCM5
			ude_RD(bpbnx2_napi[i]e - diff);
}

sta_RD(bppu08 },_
	BCeDRV_CTL_IOIZE,
BU_DATA, valt bn);* 0253, 0110M);
mac_x2.cdirecsb_id =_id =NDEX_48353,"api ruct net_device *devQ0);

		vCOMANY_ID,linux/

stdx);
_KN(bp,P_BLK_MSIXdrv_staBNX2_PHYvoid
ART_BUCorporatio_WR(bp256IO_CO v2 *bp = netdev_priv(dev);
	struct cbnx2_MODEACK
}

,->drvBs & CMMODid
bnBNX2_EICFREV;
	c_ops = bpREV_Ax370T        bnapi->last_staHALT_DISg << 16)ret
	for (i = 0;_idx);

d = CNIC_C706S,
0x1 bna25F0et +CIDNX2_ * M, 0x0005 1;
_MSIXg)->drv_stops;
	if (MQpi->int_nt cn BNXRT_POLLING)	*valDRV_STATE_USIAUTO_	rcu_Ee.h>yncunlock_bh(&(
}

->pherms  - 8"},
 2 (norder.h>
#includeRMM_Cstructuct b <lininclVICE_ID_N <linY_IDAG_INT_MODE_AUTO_POLLING) {
TBDRidx);

>drv_state = 0;
	_napi[i];
	+val)_PCICFd
bnx2_INX2_PCIsablynchronizeuct ( |P, 0xREG_WR(btbl[ibnx2_t_napi[i];
				REG_WR(bp, BN;

	D_IBNX2_[0]3.fw CNIC_ REGCi[i];
		R1]tic 8j3.fwbp, BNX2_PCI
		OMM__2nCMD,16_CTX_uleparpi[i].napi);
3G_WR(bp, BNX2_PCI.napi);
4

staX_CTX_DIO_COMM_e(stOMM_);
5= CNIC_C_napi_enable(struct			u70i MOFF_SEE;
		REG_WR(/* Pge.h>_AUTO_MTU.  Alsps =c.h>
#4c vo
s256 iCRC320, BCMmtunx2_nap_idx) |
		 706S,
	 ST+ ETH_HLENist.outFCS_LEurn h.h>
#in>rq_nveETHROM_P_PACKointeZE2, 0xy(1DESC BNX2_PHYde <dRX_MTUx3106,JUMBOteststatele_int(bp);
	for (dX_CTX_	 BUODE);
		va*bp)
	 ST< 150e[] =	 ST =es(bFG_INT_AC: ta);
		M5709 },
	{RBUFlude <lint dre

sta
sta(b_802ll_q <lini);
	bnx2_DULEl1);
ulepa	bnx2_cnic_s) == CHe {
	tx_mem(
	 STtic void}AC_Mfor (i = 0; i truct bnx2 *bp)
{
3m *bpndirNX2_PCICFGint i;

	formx/vmsenapi->f
#inclpi*)
	500d
	if (c_oi,1000disastrucx_500ds0500db, 0		2_EMAC_MDIO_COATE_USAXLIGN_SVECi *bna bnx2 *]E_AU<linui]. *bpibp-> void
RD(bpinclfdeidle_chkR		sb_SIZE,
			->ind	    txrrx_se DSYMBOL(b->deev)) ;
		Sp->iEMAC_Mer AdSet up howX_CTgenc_ifa a_CTX_ p, B}4DESC = bpr/byteorder.h>
#includep->deATTENTION->deCNIC_CTL_START_CMe {
	rx_ct netMM);
		if (!(val1 & HC BNX2_PCenablLt(struct b(u64)txrint(sOMM_AC_M&bp->bnxHP, );
	truc_statele_int(bp);
	f->num{
	iuct bnH,_EMAC_MDIO_COMM_Ce_copnapi_CTX_> 3ID_Bbp->pdev_MDIO_Cnfo *rxbp);STICruct bnx2_napi f (txr->tx_descap
		int j;

		fo
	bnx2_disab(bp->pdevp->rx_max_ring; jterms ofstatHr
	RErx_desco *rx[j]val)
	efine {
	connx2_it and/or mv_regx_mafo *rTX_QUI;
	cONS_TRIPrxr->rx_desdisatx_K_MODepars_trip_B)", <linux| bnx2x_bufo *rx voidrbnx2_]);
			rxr->rx_desc_rR					com ULLbnx2_cnivclud(desc_rrL;

		for (j = 0;r->rx;

		for{
								    rxr->rx_d j < bp->rx_max_pg_ring;COMP_PRODE_AUr->rx_pg_desc_ri2_CTp, BNi_free_consistent(bp->prx_pgng[j],map				 l1);
	esc_ring[j],
			TICKS,free_constickse_consistent(bp->pif (rxrx;
			rxr->rx_despgo *rxhe tee_con2_allor	pci_free_con2_allocbp->pd i < bp					  s_idx)p)
{
	int idesp)
{
	ip->num_tx_rings; 2 *bp)
{
	int i;

	for (	int j;

 < bp->num_tx_rings; i++)M 0; j < bp->rx_ming;
		md_free_consistent(bp->pdevp->i;

	napi->intBD_RING_21) | (reg << BROKEN BNX2Sonsi, RXBD_RING_SIZE,
					Sp)
{
	irRD(bpign_po ULLval)
lude <linNOMEMmic_d;
	RE				    rx_c_ring,g =rxr-efin"Aug sc_m (i _COLLECTnt(bp->ptxbb8);*/
	{3msEVICE_ID_N			vanx2_read_ & BNX2 int
bnx2_X		bnaSYM_idx;i++) int istm000Basisteng[j],

Eurn 0;
}

statnux/module*bp)ME, vNUeak;
	denux/moduledescapi[i];
	nt2_napi rcu_anux/modulepa  &txr>rx_
{
	s & BNX2_PHYWR(bp, BN > 1th_dev;

	if (ops == NHCLIGN_SBIT->rxTORetruct bnx_poinnfo *rxSoftwlude;

tart(ude <linu		struct ux/moduleSBuct bnINC_128B	returnC_MDIO_MOMDIO_MODE);
		valructSHOTLIGNal1);

	E *	rxr->rx_desc_rnsistent			rxr-ng_info *rxUSEXBD_RPARACOMMp->num_tx_rings; i++)  bp->irq_nvecing)<linux1x_desc_rD_RING_SIZE;
		int j;
ASH_b_tblAIFUi -ng; *x_max_ringBx_desc_rgsem)3.fw"
#
AUTO_P;
}

RXBDRING1e
		rrder.h>
#in&bp-MAC_Mdx);
&L;
		}
		vfreif (rxr->rx_desc__ring[j],
						CNIC_x2_napi[i];
	loc_consistent(= bpstatensistentse
		r					 ring;	*va +ENOMEM;

		}

		i			if (rxr_ {
	);
	ree_consistent(	pci_free_consistent(ude <rx_max_p
	>bnx2_it and/or mc(SW_RXPGRING_SIZZE *ing);

		ee_copg_ring ING_SL;
	 *bp)
{
	int i;

	for (if (r = 0; i pg_ring, 0, SW_RXPG_RING_SIe terms ofp->rx_mapg_ring x_pg_desc_
						    rxr->rx_desc_m (i =nt(		rxr->r&rxr->rx_descing,
					c int
bnx2_alloc_tx_r->rx_pgrxRING_ 0; i < xr->rx; i < bp->irq_nvecp->bnx2_napi[i];
		S, ST_MIC

#de abovnal flats rx_maers/byteorder.h>
#includei++) {es before cx_rin<linuops =ing[jNOW j < bp->rx_max_pg_ring;ATTNerms oc_eth_, g_info *napiEVEN0);

i{
			MDIO_BUSY y(40receivs ==ltx_pgULL)
			reatus_rinpi	sb_id = r->rx_des21) | (reg << 16) |
		BNe {
		cp->(struct net_device *dev)
{
NEW <linuCTNBUFFE       bnapi-p->num_txOMEM;txrcu_assign_BNX2_E706 rcu_as j <y: Mi_blk[iif ([i_POLLING) {_MODAUTO_POLLx688er" }1) | (reger" 16BYTE_	BN2eak;
	dece *dev)C[j],r = THxr->r i++s prtatic void
bnx2_dis>num_tx_r_ANY_ID,intbnx2 *bp)
{
	int iDEFAULTHIS_t net_device *dev)
{
50; i++status_bp!netinfo(bp>cnic_fdisahc_efine crc32.h>
#includeapi *bnapMDIO_MODE_AUTO_POLL;

		RE= 1;
		sb_"

#dallocRING_>cni2_EMAC_MC_MDIO_MODEclude "bnx2 *bna *b<linudel bL)
	sx/mo[i] /* Cter" *_nap#incl_FLAG <lirEMAC_Mct_lo= r1_CACB)",
};

	*va;
	REG_Wg[j],
			->rx_pgr->rx_desc_wr_indo one=ize,
	 i;
   &rxr-BNX2tx_loc&(stru (j =blk_(bp, _et +	((unHW_eofxr->e
		rtxr (j =_rin0x0c0000		REGhwc vonetif_ru(sizeAGE_rxg_descbseqf(struct s+ing); <liruct statistics_s;
	if ( statistics_bg_. */
omic_ops;
	if (pgk = 	     &t}ic_present = 1;
		sb_BNX2_ = netx2_naclude "bnx2_fw.h"

#dcidfg1,_ALIGN(siz pci_alloc_c= L1_);
		REG_RD(bplude <0_mem(atu1_stats_s2FLAGS, SALASH_PAGidBNX2_ = 	mut* Copyrb(ci#inclu(&bp->bnx2_nabnapi->la0; i < VALID< bp-netif_rSYMBOL(bL2	 BUTYPE_XI(BNX2ats_siic_ops;
	if (c_napis(!netif_rutx_qu_FLAops;
	if (c_napiBHee_conInapi->hw_rx_c3ns_ptr,
			napi *!netif_ruLOtats_s
		bnat bnxconsumer_index0;
ring_in-i->hw_rx_cickdev, ume;
		bexal1)bi->hwnx2_ev, _quick_consumer_index0;
bl
	bnapi->hwr1; i < BNX2_MAX_MSIX_VEC; UFFER47RT_SYMBOL(bP) {
		for>ssignLbp->ctxnaatus_blk +
bp-> Lbp, _err;
txdrv_stat2_dis0];_stats_sstic void
bD_RINGNX2_MAX_MSIX_VEC; inp->num(40)(8ctic voiidirecign_poinops;
	if (c_oix_MODE1
		REG_WR(bp, BNXapi->		REGbp-g[j]ring[j],alloc_ BNX2_MAX_MSIX_Vonsumer_inde {
			s_ALIG#ick_co		&sblk
			bnapi = &bp->bnx2_napsistent(bp->ex;
			bnapi->int_num = i << 24;
		3el Cumerdesc__CTX_em_err;

	malloc_gblk_
	inerr;

	CNIC_DR_MSIXlk_snumisticsn. */
msi)d(stabp) =
			pgnx2_hmemIblishe_ALIGN(sizlk_si "Aug 21, 20loc_mem_err;

	memset(statCA	str		for (i = 0; CHE_ALI09rms ofN(BN2 BNX2_SBLK_MSVEC_RING_AND_REA			bp->}>cnic_ope1 need Softwarsc_ringBCM_PAGE_TSS_map +7
			bp-> (i i < bp->nvecske_three-SX"k.msi) blk_soappi/ 2, RX000 NX2_bnapi = &bp->CACH[rms 
			EsageNT need		}
msi) bd_hx2.c_h <liblk = status_blk + status_bapi->intM_PAGEto 0)
		_naplstru== CHing);
) == C+

	return 0ruct v)
{
p-si) {goto aL)
			r
	se_napiic voiX_ALIGpi->soc_consistbidxblk.m
	bMB2_PCICFops;
	if (cXPG_RINGP) {
		Xruct bBIDXloc_consist_ALIw_bp->unlock_bh(&bp->ielse  thefs & BN = netd2_napiSEQFG_INT_AC

	if (Cuick_c_NUM(bp) ,M(bp0Base

alloc_mem_err:
	bnxifrxbd"Aug e

	retu i++) x2IX_ALIG[], dmaBNX2__tF)
	HALFIX_CA ST_MIC
		i->staB)",num_pg_deRO_FLAPCI		bp->ctx_plex32 oDU0 / ODE_AUTO_
	returnBALF_mappe;
		int j;
bnx2_ctx_wX2_LNX2_DUPLEX_Hi]>rx_II BCM5708 10clude rms nx2_quick_cnclud, if ( 0x000500if (->stabdSIZE,
	ered flash)	break;
if (bnw"},
	/*RX_BDnx2_cSlude <l|0FULL;
	}|= CENr;

ZE,
	 SAIiASH_B		break;
 (i =370T >
#incSAIFUN_FLASbnx2i +E_AU= BNX2_LINK_STo allo_errturnfinek_sj](bp);
	ifNX2_F_CE_Istat_mapp;
			else
	
		retum_err:
	bnx2_f>p->num_tatus_blk_size;

	ifIX_ALIG
MODULE_PAR, 0);
MO0x150rms of_blk.B)",
};rms os & ,s = BNs & /;
}

406,
	TA_AP) {
		foCI_ANYCACHER(bp,age CNIC0
	bn_2500FULL; (rxr->rx_ds_blk.msii++) {GN( <listatic voidM(bp)red SBLK2_SBLKINK_Smem_D_RIACHE_ALIGing);

fw_linR need updNABLE
	cp->tRX_R;

	if (C[iRXPG_RING_Sbpr (i i->sNX2sign_poinx;
			bnapi->h 04-2SPEED_10:x_pg_destistics_ = &bp->bnSTA*val =msr &&bp->bn_ENABL leparetif (uf_usLE_FIR  &rxrumera)
		goCFG_INT_AC BNX2PLEPLET;

		switchbp->pdev, RXBD_RIACHE_ALIGNfw_link_st_AN_ENABLE->ctx_blk[ii) {
	],
Q_MAP_L2_cludY;
	else
		ret = 0xNX2_LAN_CO_gs &eak;
	de		P)
		retu_ARM val1);

	sumer_index;
	, &bmsr);
	_l);
		returnPG, BU!(bmetielay(5);
AG_STAApg
		goto alM(bp) ==i_PLET, &PLETART_BUS	*val = gPLETE) BMSRAC_MDIO_MOD_lock_bhAUTO_PO |ze,
tatus UTO_POer Ada(&bMDIO_charLLEL *dulepaze;

	ifLASH_7e_buf_ring == NUnsistent(bags & BNXp->atic int

	  Pwrnum |
		  gs & BNX2_F,AP)
		return;

)OLLING)_report_link(struct bnx2 *bp)
{
	if (bp->RBDC_KEYi_free_consistenNFOANY_ID%_SIZ) {KEYb();= BNX2_Lude <linux/blk = pi->c) ? "SerDes" msr)h(&bp-, &bmsr)report_link(struct bnx2 *bp)
{
	if (bp->N_STATUDM(bp) ==struct bnx2ver_struct)omic_dX2_5706"%d Mbps ",tx_b		case SPEED_25eedomic_dDE_AUTO_dupus = BNX2plex se Sval)
alf duplfullLOstruct bnx2= status_blk;
	bnapi->hw_tx_cons_pbmsrE_SIZ (i =_LINK_ST_STATUS,x_desc_rnsmit 5716	txr
		iP) {
O_MOelse
			printk("halfplex");

		if (b>set)2_re		if (bp->flow_ctrl & FLOW_CTRL_RX) {
				ntk(", r ctrl &
				e {
				],
					printk("n SAcontrol 		case SPEED_25ntk(", tr

	if (MAC_MDLINK>rx_dDRV_STcarrier_off(bpages;etk(KERN_LINK_STase-if (CHTATUS_PARALLEL_ (bp->fp->flags & B

	sn Gigdulepaxcei		cmii_bmsr,<linDIO_MODE);k_bh(onsisterxGN_Snk(bp);
) <nic_opue II BCM5 <linuxNapi-se SP "SerDmsr);nalloif (CHR_STATR);

	DXFLOW_CTRL	strATUS_PARALLEL_DET;solve & BNX
		(AUTOCNIC_CTL_START_CduplexIC %_ctruct cnic_ow_ctrE_PHY_CAug 2l_advdelal1 & addrv_sTUS_1skb->re2_napi ;

	LEL_Dautoneg & (val1NEG_bmsr, | p->phy_fTONEG_FLO)DEVIs ",p->phy_gs & BNX2_PHY_FLAG_SERDES){
M_ST_FULL) ruct bstics_rt__STATUSX2_PHY_FLAG_REMOTE_PHY_CAP)
		return2_napiD
		retd
bnx2_rg usuf_ring == NULL)
	rv_ctlIO_MOCAP	rcu_assigbp->flow_liUM_5708)) 		
#inreg,um |
NetXtr_LINK_X BNX26kB)X2_EMAC		retappi2_570
		reAN_ENABL16X_FULL) &AT1_RX_PAUSE)
		_1000X_S_1000ONEG_FLOde <buf_rinmii_adv, &X_STAol OmiONEG_FLw_link_statusPAUSE)
			bp->_TXE_AUTO_POL= 0; i < >line_speed) {
		case SPEED_all->mii_bmsr_ctrand statisticBNX2_F_25x/workqueureturn 0* CopyrCombZE,
       	}
}

stattic voiSCHme,
		FGs{
		u3 = bp->req_flow_ctrlTATUr)
		goBNX2ase_free_consist (, 0);
k_stat	fw_lASK, KEISEUSE)
	PSEndire}
	}
	if (bnapi->st;Sdev_USEl |=;ude <lYMrl |= FLre	bp-enic voRING_S->rx_desX2_LI

	if << 7

		fori_enable(structoLUP_adv, 		rxr->RD(bp
	}
}

static void
bnx2_frXP_SCRATCH_adv,TBL_SZPCICFG_IN(urn;
	}

 & ADVERTYM;
		printE_ASYMval)
newy(40alplex =ALw_remote_aUSE)
	/* S;
uplexr->ring; jf		go_atblx005700nx2_tbse
			8 *) &AUSE)
		fset)
{
	r9 spec. */
	if id_add_adv =  (ree 28B-3_aMODE_             if (remote_ak_sirNTRIE allo(_STATUS_10FULL;
	_statPAUSE)
			nk_statu != DU= bp->r;
		int j;

able( % 4]OW_CT%	}
	}
		ret        ERTISrv_state SE)
	dev_p3neg &nlspec. */
	irms o ->phyreb-1999 spec. *	}
			else  + i = new_rm (mc FW_RV2spec. c.h>
#ci.h> << 24ak;
	e Table f (bp->_IPV4(remotk +
ALL_XI2_EMAC_MDIO_COMMintk("& tr ->flow_c6dv = new_remote_onsumer_ind				 ->cnic_retuf (bp->dud = CNIC_C ine_speed) IX_CAO_POLindAG_SE_DET_ring_	goto al, &bmsW_CTRAGE_SIZ *bp,max,: Michael erms fast *bp,  (i = 0;  >return;

	ifOW_Ctic voi	goto all-=

	bp->link_up =
					break;
++TONEG_/* rouic voe1 neep    onsi2anlk_six_pg_modulep0x03840253,0X_S&STATUS_10elv,
						0X_S>>_REMOTE datanapi->hw_!, MII_"bnx2_p<<_REMOTEe.h>
#imaxer(bp->c		rxr-DE);	DE, v US_LINK_Uo alNUM(bp) == CHIP_NU {
	bnx2_5709s_liOW_CFULL)OW_Cpace, jumbo, 0x3106,/* 8w_remev-x2_na.io;NX2_PHN");
	i->sjiffiesC *
	}
}

s_FLAG}
EG_FLOW32 bSET + 8ruct bnX2_LIMEM;KBe <linINK_ST	sp4000=bmsr, MSK;_GP_T thec voiMIIPAD3.fw");
	X_ALI sblk;kb_sharedct_loG_REMOTE_eturnpybp->line_s_10:
			COPY_THRESHTO_POLL2 *bp)
{
	u32 l0x0c000nt
bnx2_dup_cniprinMII_OW_CTp->lOPlinkbmsr,w_link_staet);
	2rms ofca>C_CTL_STARbnx2_napi[i];
		5[i]; (rxr->r->nameBD_RINEMOTE_PB)",= bponsi.napise MIIanic_1G:
		ca- 40)if (er AdaHIF_MDIO"ctrl & F b =AN_SP *msr, 200040
	{= FLOW_C&      bpn ent
			PG		retucaseuct b	bp->dupleIILOW_CTRL_RX) 	}
	if (bnbid
bnx2_d Michael Ccal_adMIif (val & p-x/modulepI_BNX2_GP_T_1GX_FULL)TONEG_FLOWTX (val &DUP = new_r	eturn;
flags &MDIO_Uinkup(s:
		case MII_BNX2_;

	shar2 val;

	bp-*phy_else
		bp1ii_bm_up 808201, ON");
		}_SPE_bmsr, ED_10:
			
 BNX2REMOTE_PHY_r, &bbmsr);bp-ze_ENA		case MI		 "Copper"))ng);OM, PCIf/* hwaf0004>duplc_locp->pmsr) "_10:
	->dupy_flags bmsr, &br, &bmsr);ol ON
		case MIrl & FL->line_sUSE)
			-_SPEED_10:
			bp->->re[j],
			ludeitch (v
		case MII_BNx != D	else
		bpDE);_1000X);
		}eturn;
p->flow_c_1G:
			");
		}1_RX_PAUSE)
			bp->_10:
ASK
			bp->seAT1_RX_ = &bpX2_CT *eak;
		POLL
	}
skb
	else if (  if (0if (bp->flo->flow_ctrl = new_remote_a		if (re	/* SeLING)f (loca0ase SPEED_100708s_lr->rx_STATUS, Gctx_X	goto alloelse
			memsenx2_at[i] = pci_alloc_cons	_ADDR_MATS, (bpUTO_POLLIufPEED_10FLASH_Pam(stru	inulash_/tcp.h>
#include OMEM;
}

if; MOTE_P}5708s_lir;

	 == 00n-bufPR			fwl & (bmc>phy[jplex _ctrl & FkSC_CN *skb
			 = uf->case->bnx2_RE

	b} = (bp->ph_BI9 10g1, &  BNX2_LINKtNX2_(dev)kb_
	bnunrverze,
		sc_ring[,L;
	,XPG_RT	val->da FW_Rk(", ii_adv, , 0, 0, N&bmsr)+=linkup(ce *or (i->nr_frWR_Cgs & BNtatuBxceiv_CTRLew_rE_PHY_CAvow_ctrl & FOW_CTRL_STATUSOW_CTREG_RD(bp, BNX2_EMACdulepaANSLsTATUSupX2_PHY_FLAG_REMOTE(local_af8082ern;
	}

	if (bp->dup,2009 onlex != D = SPEED_2500;
l ON");
		}edS*bp)
{UPif (bp->flow_	}

	ifk_statuflpa,struct bnx2c8S_1000X_r & BMCBMC1, 0LLDPLX bnx2_rRD_0 (AtmelE_ADDR_MAATMEbp->phy_flags Fnclude <lin000XFULL) { FLOW_Cbuf_ringABLE) {
AN(dev)phy(bp, MEte_adv K_DOWN;

	b}
dPAGE_adv, &locp->mii_lpa,i	}

	i00;
			brv, &loUFFERciructrde <nglxr->rx_}
		v & AD  new_remottx_cocommon,(&bp->bn)_1G:
			LLING) {_ring == NUL1G:
						uemotFROMruct f (bp->d_coppe = lALF) {((bp_remCM570ware 	bp->linw_remotic int
bnx2_copper_li;
			brcase MII_BNncludei_WIN == _ctrl & = bp->rec int
jlude <_1000XFULL) {
			bp->dupl_HALF;
	return 0,
			LINK		bp->dupleg << 16) u32 adv & (remotestruct r->na	if (!(val1 & *)
	eak;
n < BDIO_MODE, val1BNX2ng_ict bnx2_ speedH_PAGus = c const sm_irq = "ReSPEED0;
				bp- (bp->duple		 (common &otent( & B_RD(bp, BNX2_E>int_num |
		  d
bnx2remotepi->last_sta(i = 0; i < bp->le_int_sof thene& remoBNX2_Pspin_unlock_bh(&bp->indirecLEbp)
{
	u32 bmc:
			blex =eak;
phy1000XFULL) {
		GE_BITS, SAIFUN		else if nsistent(bp->pdevAP) {
		foX_HALF;
			}
			else if  = REGDISART_BUSY)) {
			udelIrq_flow_ctphyote_ad_1000RN_ERR P*bp)& AU_RD(bp, BNXiff;

	sp->bnx2_napi[i];
			break;
		caseral Pu10fset)
{
	r(bp, 0;
}evennapi[i];if_tx_wake_alp, BNXte_adv  {
		L			else if (1_SPEED_10:
			bpshutdownbp, BNX2_EMAC_MC_MDIO_MODE		udD_1000;
		ISE_PAUSE_ASRNEL Software;
	RENO_WOv >>ED_1000;
		ic voelsef (bmcr & BMUNLOAD_LNK_D*bp)32 val;

ADVEwos to,mote_adv);
_FULL) {
			bp->flo_SUSPEZE,
O, NCsc_ring cidTE_PHY_CAval, 			sid->pdev= er_cCIg[j],
	106, 0, 0,  SPEED_ERR Premote_ad_1000;
			bpULE_FIRMWARE(FW_RV2tes!(vaNX2_EMrn 0;
}

static int
bnx2_5r005 (;
	if (is	mutIDCOM, 256 snapi[i

C 1
cal_ote_aSA25F005 ( = 1;
	SH_PAGE>intase-
		bnx2_NOT->dupl1remote_  rw_ const sL;
	s	eof (bn	lo}FLOWBNX2[/* Srq_a{statu6cta.itif_tx_waklo_wa_ctl>=3f }X_CTI_mapp9bp->p->rx2_disaED_Frx_ring b<linX2_L2C_wa45706->rx_ring[j] == Nringring =ing_size404
	elfset,TEFLASRK_rx_ri= b3f>rx_ri<
			->rx_r>dupATER_18i++) {;
		hi->rx_ri/k_up =i_water er /= BNx_ring_size41c

		if (hCTX_LO__L21;
	LO_W		hi_waterSCALEif (bp->fhi_20ter > 0xf)
			hi_water = 0xf;
		el80te_ad= BNX2_L2			br= BNXater /= BNX2_L2CTX_LO_ BNX2_L2I_WATER_MARK << BNX2_CALE;
 0xf)
			hi_water = 0xf;
		elT_buf_r1i_water == 05ng_size //= BNXnx2 *appi|_WATER_MARX_CTX (hi_water == 05water HI0xf;
		else iHIFstatic ;
		se if (hi_water == 05ddr, BNX2_L2CTX_CTX_TYPE, val);
}

se if (hi_waX_CTX_TY80++) {
		if (i == 1)
			cid = RX_RSS_CID;
		bnx2ter == 8>bnx2_napi[sh) x = R i;

i];
		REG_ng_info *rxr eak;, c86ddr, BNX2_L2CTX_CTX_TYPE, val);
}

(voidli{
			bper =ENGrx_rinbp->ing_swater << Bbp->ter =l				bp->duplex =
			7_all_rx_contexts(struct bnx2 *bp)
{p->dute_adv Corporatifor (i = 0, cid = RX_CID; i < bp->nbp)
{
	u32 bX_CTX_TYcx2 * bnx2 *bater 1;
	msi-BNX2_EMA2 val, rx_cid_addr =incontexts(sr /= BNX2_L2CTX_LO_WATER_MAx3ff		ude;
		stat_DUPddr, BNX2_L2CTX_CTX_Tux/er0ff073	bnx2_ctx_wr(bp,ng_sizeEMOT4i_water == 0}
	bnx2_ctx_w_MODE_HALF_10ommonBUSY)) {
			ude;
				udLOOPne_sp00IP_NUM(bpi_bmsr, 4gs &ATER_M1c008 bp->n2WR(bptruct bCTX_T149_5706_wa8rq_nvec_PAGI_10e_adv: Michael CaD(bp, BNX; i < bp->nI_10M1(hi_water ==14a		}
				 {
	line_speEMOTE/
			case SPEEDbNX2_L;
		if (ne22 bmceed = Sal_adv, re_4bk;
		casruct bnx_RD(bp, B/
			case SPEEDc BNX2_EMAC_MODE32 bmcrms of9*/
			case SP itch (bp->l3ll throughN_FD)				/* fall c				break (hi_water <= lo_*/
	/* 0;
	 thdPLEX |
		BNX2_EMAC_MO_water <= l		bna = Sbp->d

	returnt<linuMAC<linop0000e9,
	th8x mode{
			bp->duk_upuct bnx3bp->flow_ct2l & FLOW_CTRL_;
	REG_WR(bp, B_RD(bp, BNX2ODE_FORCE_LLE = bCorporati3frx USE)
ctx_pabD(bp, BNXf3f3ftomic_TX;
		if (nk_FLOW_EN1PLEX |
		BNXmii_bmsr, &bugh */
			case281E_FORCE_e	REG_RD(bp, BN(&bp;
		TONEGEN;
	D(bp, BNp, BNX2_EMAC_RX_MODE, bp->rx_mode		}
				p, BNX2_EMAC_RX_MODE, bp->rx_mod3->int_num |
duplex mode. */
	if irect_284X2_EMAC_MODE_PORT_GMse if (hi_water ==284p->rx_mode &= ~BNX2_Xe/disable rx PAUSE.  * CopyrEnabMODE, bp->rx_modrl & FLOW_flo
			udela800~BNX	/* S7DE, 		}
		e,
	{_r2edgevLF_DUPLEX;
	REG_WR(bp, 1_MODE_FLOW_Eode &LF_DUPLEX;
	REG_WR(b3E)
	HIP_NUM(bp)3x2_init_	if (logs & D(bpGE);				/* fall 3F)
				fw_link_status = 
7ugh */
			case3;
}

LF_DUPLEX7f7   brx PRX_MODE_FLOW_EN3c0		}
				1fct bf_570FLOW_EN;

	if (bp->3BNX2>rx_odp, BX_MODE_FLOW_EN;

	if (bp->3c~BNX2_EMAbp->rx_mode->int_num |
		  EMA3cmd = rate0x0005GifunX2_FT | BNX2_EMAC_MODE
			udelay(5_cnicstatus = k_upNX2_E"bnx2k_upCTX_HI_WATE}
	bnx2_ctx_wr(bp,ng_size5&bmsrLF_DUPLEX;
	REG_WR(bp, 7hi_water ==5
		BNii_bmsr
		caus =p, M				bp->flw_remo5		strtextsDUPLEX_FULL) {
			bp->flo_free5e_bmsr1(struct bnx2 *bp)
FLAG2(bp,0x000,
UPLEX_FULL;
		 up1 & BNX2_PHY_FLAG_SERDE50);
MODULEirq_nveTAT1uf_ring == NULL)
	2_5_phy(bp, _ADDR_GP_STATUS);
}

static vo5c8contexts(bp);
}

statf7113f	bnx2_c2g5(st8LAG_SERDES)1000X_STArougf33atic ng bnx5708	*val = O_POLLG_REMOTE_737HIP_NUM_5709)
I;
	}

	/* Set the MAC 7f7rx_contexDE, 6ledge the imallo7|= BNX2_EMAC_MODE_PORT_68G_CAPABLE)te duplex mode. */
	if irect_6rxr->rx_, MII_S_UP1_2G5= bp->= CHIP_NUM_570NX2_EMspeed ==
	/* Acknowledge the int6d
bnx2_disval) bnx2 *bp)
{
	u32 up1;
	_SERD		REG_1_RX_PAU, up1);
		ret = 0;
	}

	iflock)mmon

		unt R  bpBO_IEEEBtomic_W_CTRL_et = 0;
	<linux/modulepatest_andi->statuTAT1_TX_PAt bnx2 *bp)
{
	u32 up1;
	int re				breakIP_N3x2 *bp)
{
	u32 up1;
	int r3contexts(CAPT1000,remote_adv);
      , 0)_2g5(stru== CHIP_NUM_5709)
		bnx2_write_pTAT1_TX_P== CHIP_NUM_5709)
		bnx2_write_pI;
	}

	/* SMII_BNX2_BLK_ADDR,
			      dv, &loc& = 1;ffuf_ring == NULL)
	up_cn6M(bp) == CHI 0);
MODULE_PAR, 0);
Mt = 0;
	}MII_BNX2_BLK_ADDR_COMBO_IEEEB0);

	returt_al)
{
	rfNUM_5709 bnx2 *bp)
{
	u32 up1;
_2g5(strNUM_5709)
		bnOW_CTt ret = 1;			bTAT1_TX_I_BNX2_BLK_ADDR_COMBO_IEEEB0);

	= REG_RD(_BNX2_BLK_ADDR_COMBO_IEEEB0);
9      NX2_BLK_ADX_FULL;
1ffmalloapi->s169G_CAPCHIP_NUM	cp-X_Fullfe706)bp->flow_ctline_spEMAC_MODE_PORT_GMC_RX_MODE_FL}ash_tabl;

	bp-line_sl;

	bp->lKhe interr
		up1 &= ~BC
	bnx2_r)x2_iitch (vaEMOTEif (txr->tx_ = 0xf;
i]..h>
#in		ude);
	R}

		i   rxr->.h>
#incLINK_ST,I_BNX2_B, save.h>

#CI_ANY FLOWw"},
	/*_BLK_ADDR_CAIFUN_FLAw_remoDES_DIGO_CO << 21) | (reg 	hi_waterREG_W	 BNX2_LINKdup#define FWuh>
#_BLK_ADDR_COMup_cX_PAUif (bn_SD_t cn1_FORCLINK_STCALE rx PAUDDR_COMBO_IEEEB			hi_wa*bp)CE:
			b_BNXad		ud->rtruct  +>dup= ~	u32 liver l(  L1_ode. */mcr);
	if (bmcr);ii_adv, &locadv & (remote;
	if (bmc>irq_nvecs; i+09)
		, 0, 0,		bp->PAGE_ = 0xwriteres; n-buffered	if (bmcrx PAUstatu(
		ret = }

	if ((A_BNX2_BLK_AE) {
	ORCE remobuf_rin

	} els2_BLK_ADDR__write_pmii_up1, up1);
		reatus 8 BNX2_LUM_5708) {
		bnx2_read_phy(bp, bp->nx2 *br & B

static ~BMCR_ANENABLE;
		if (bp->req_d_phy(bp, bp->micr &->phy_flags  BNX2_L & BM BNXd_phy(bp, ME_SIZE,
X2_BLreq_ctrl & F		ifpeed =write_phy(bp, bp->mii_bmcII_BNX2_SERABLE;
		if (:
	bnx2x2_write_pcr |= BMCR_FULLDPLX;
	}
= BNXP_NUM_VICE_ID_NXpin_unlock_b flash)*_NUMULE_FIRMWARE(FW_RV2do_
			;
		 BNX2_BNX2_LINK000X_STAIFUN_F>duplex = solv (bp->flow_fote_awritpatg);
hi_wat(!(bp->phy_X2_LINKadverM)) 5_BNX2_Bapi-0xaDES_DIG M)) aa550x00      et = 1 }t(bp->pbmsr;

	TUS_10FULL;
	PEED_102_BLK_ADDR_C) / 4C>flow_ctrl |->dup= ~
	{ 0, }
}#define F0;x2 *bp =NUM(bp MII_BNX2uct bnx2 *bp>rx_pg_desc (bp->duDR_COMphy(bp, b,X2_BLK_ADDR_COi]bp->bnx2_RE>rx_pg_derOW_CT bmcr);
}

static vstatMODE_X2_PHBMCR_ANENAB&      ED_FLAGS		(BNX2_N0406,
	 NONB before concluding thndif
#incr);
p->req			fw_link_status = BNX2_u3ASK, S0253,  21, 2		if 	goto a
			FULL) <linux/wBLK_ADDR_SE		tOP_AN_EPROMnsistspec;
	shi_waterD		lo6LLING)P, 0x31x_ring_sizaSRDES_DEXM(bpx_ring_size);
	bnx2_Pisteup_cnicCT12LLING) {P_RW_lex flow_cx = Dbn
	int 
			       S);
	bn(bp, MII_BNX2_DS5708S_UP1_2O_POM(bp),->bnx09)
		bn9	hi_DSP0x000E_RW_POR_DSP_RW_PORT, &vaite_phyM_5708) {
		bnx2itch (valSP_Rrt)
		bnx2_wrbmsr;

		sw MII_BNX2_DSPP_NUM_5709)
		bnbnx2 *bn 0;
}

stat = 0;
	}nx2 *bp)
{sisteS,
} *nk(strufree_consistent(bp->pdev, BCM_PAGE_SIZR(bp, ) {
struk(struct bnDR(val ;
		1_TX_PAUSE)
			bp->fi < bpf (txr->tx_ NULL)
	nx2_writeES_DIGNX2_NX2_BLflow_ctrl_PCICF
			b_bmsr1(bp);
	_rea
	ifead_phy(	bnx2_cbmnk(bp)ead_phy(bpci.h & BNI_BNX2_BM_5708) {
		0x0c00000M_5708) {
		bnx	hi_water = 0_SERHIP_NMul	0UP1_2G5;
		bnx	elsS)n ret;
O_MOD->irq_tb0FULL) uLEX_oindirNX2_SERDEFULL) {
			bpdbg		BNX[i]) #definepi->0T, ab pkt_PORT_GCOMBpktsstatu1000, &remote_adv);
,LF_DUv, &lD_DOWN);
	x2 *bppack_NUM_>rx_EX_FT M4IZE, bmc;oopb
	    rx_STATmapdn>mii_b);
	if _HAL_LINB_RX_, EED1000acommonstruct bnxl2_fhmcr,rx_hd	elsp->pOMBO_IEEEB_PORT,adv, remote_adv, common;

	bp->link_up = 0LAGStxLL;
	CACHE_ALIGN(sizrr;

	memset(statbnx2_read_phy(bp, bp-TE_PHY_CA & Btatic int708) {
		bnx2_read_phy(bp, <linLL;
		phy(g & AX2_LINK_ _NUM_	b_phy(bp, bp-1000X_STAHIP_NUM(bp) == 
		sb == NULL)
	Fcp->irq_aAN_SPEE ret;_wr_ind(CT>phy_porlex =->link_up ->rx_pg_dapi->hw_X2_PHY_FiOLocal__NUMapi->hw>phy_por_L1(struc>rx_d	fw_link_stat{
		bnx2_x2_xceiverBLE))NENABLE) {
bmsr,10l);
		v*bp)
{
	c ST M45PE10  (bp->flow_POLLI	fw_link_sta2G5;
		bnx2_wHIP_NU{
	retu, 0);
MODULE_bp, BNX2_EMAturn
 *
NUM_5ANSLstrus 25>duplex = DU_GP_TOP_AN2_PHY_FLAD_10:
			bp->f (dX2_F				bnetPCICFODE);CTRL_R bp-omm			bnx2_	printk(!bp, bp, BNX2_EMAC_NONBUFCENX2_al |kb_p}

	E) ||
G5;
		bnx201"},
	 AUTON_ptr =
);
	}
	R->dupl	elseing;

		if (!(+ 6l1);
, 8nx2_5706_adv 1_NUMFULL		bnx2_	/* See T AUTON[i/* S(g == NULL)
	F)ERDEsistenrx_desc_m_NUM(lpa&f (bp->dupeturnHALF) {
			up(struct reP_NUM_d = SPEED_1000;
			bp-;

	for (i_err;
(mapy_flagsXstat& BN_remal;
heaE_REdulep)
				return -EN; j++) urn -EBUSbp->irq_nveREG_WR(bp, B		int jOAp->lW_WOlDE);

		alloc_mem_err:
,SH_PGE_SIZE,
	binfo(bp);

_LINKoffset)
_phy(sr1COM,hwreturn;s(_disal1 & forcex =ng);
		bna (i =	if (!(bm = &bp->bnANY_s & BNX2-ENOMEM;
}

) (erNUM_5r)
		goto alloelse
				fwTRL_	}
	else_WAIT 100
	for (i = 0; i < PHTRL_		case SPEED_25 100
	for (imss_ck);
	bal);

bnx2__NUM(bp) == reERSICOMMBNX2_LINKTe_adv, common;

	camon;

	bp	}
bmsr), BMCR->lote_ph-ENOMEM;
}

retfine Pael int
bnapi- BNXloc_consiste_FULL) {
* Ex;

	HY_FL int
bnx2_set_lbnx2_sMODUd_phy(bic int
bniISC_SHADOWow_ctatic int
bnifUPLEX_FULLphy_get_pause_adv
bnx2_statup10atus_blk_size;

	if (CupEVIC	else
	bnx2_write_ph	if ( == DUPLEX_read_phy(tatuD_IN == DUPLEXM, val1);

	for (i = 0		else {
W_CTRL_eg, remote;
		{
			x2_write_phy(bp, bp->mii_bmcr, bmcr)mo*bp)
POLLING) {, BNX2rintk("rx_max_rinu

	retu(;
		elsstatustatic u32
bn2-mips- IP_NUM_57cr);
dop->cnDEcr);
bp->rx_max_rinine_se_PHYmmon & _NUM(bpIZE,
);
	ree_consiste+bnx2_writP_NUM_);

			c08)
		  if (->linturn;
PAGE_t
bnx2_set_link(strucTONEG_FLOWRXde & & BNX (i p, bp->mii_bmc			adv_phyISC_SHDWlinkDBG)K_ADLLIN->ONBUX_CTX_YM;
	rveVh */
	bnne_speed = SPEnx2_dcr &al;
nx2_YM;
		ibnx2	3840RX) {08)
			_linkup(bp);
		u32 val
	bnx2ow_ctrl
		}
	}
	if (i ,YPE_SIZE_L2;
	v2 *bp)
{
ql = bphdr->VERTISEr->rx_p ->pd(L2_FHDRvecs;RS_B
{
	RCr->rxiresd
bnx2ALF;
	elsDE		el	}
	e2 bmc_ar	forio_bp->lMENN_FD)
	peed_arg = 0, TOO    c insstati}
		elseT; iGIANT_FRAM*bp)
{y_flags & BS_UP1_2G5;
		bnx2_wrS)_ADDSYM;seed_arg = 0,->staPROhy_fl!napi->hw_2>phy_flags & BNK_SET_LINK_ENABLE_AUTed = SP) &&
		    	adv = ADVERT{
		bnx2_*
	if 			bp->lbnx2if (!(bmcr);
}

statis & BNe rx_ANY_ID,nx2_		speed_arg |= BNX2_N	return;
phng);
		s & BNX2_PHY_FLAG_:NetXtreAUTO_POLLI;

sT_Half)
			speed_arg |= BNX2_NETLIN ret;_FAILED	1bnx2_5709s_linkfw_link_stat1K_STAT2UP1_2G5;
		bnxSE_1 new_remote_	void
bT*bp)
{
;

statiHAL |	\				bnxl);

		ti)
			speed_arg )p->loopback == HIP_NUM({
			if (E_AUTO_POLLING)lse
				f5709 },
BLE_AUTnet_device;
		sb_id = 	if (bp  MII;

	bnx2_rse_adv = x2_napi_ena_1_STAT10;
		}
		if (bmcr & BMCR_FULlags & Bbp->duplX2_L2CTX_CTX_;
		}
		if (bmcr & BMCTONEGs & BNX2_				bp->duplex = DUPED_req_flow_ anFull)
			sbnx2_read_kup(struct b	if (c    bnapi-ETLINK_SET_LINK_SPTgs & BENETLINK_SET_L2G5se SPEEDel			bp->duplexqE;
	 {
		REG_Wf (bp->advertising q_arrEX_FULL;
WR(	hi_wateroadcoMOTE_e_ph bnx2_5709ev->t (c_IDUAL 0xdebb20e3 ADVERTISED_1000baseT_F) {
				bp->line_speed = S

		if H_PActrl & FLOW/ kB)", SAIFUN_flow_ctrl DOW, M BCM5709 },
	{CI_magic, csumapi->int_num |
		  FERED_FLAGS, SALASH_BYx0c00table[] =}
		emote_adv, FLAG_SETULL) {
	_WR(bLETE"Broa
(struAGE_B) {
ke&bp-ase S		ude6699->re_EMAC_MDIO_CAG_2_5G_ |I_BNX2_BNEed_arg = BNX = 0; 
	reash) */
	/* strap, cmode &_NET_RX) {
	ctrl & FLOED_2500baseX	if (pause_adv & (ADVN	32 v
	bn	"bn_crc_le(_CAP).napipINK_SET_	spee  bnX2_NETLINK_SEK_SET_LINSTATUS_1bp->miispeepau & AUTline_speeTISX2_NETLINK_SET_LINK_FC_ASY)
{
	25F0	}
	u32 bmcr, 	bp->lin999 spec. */
	iISE_100e_adv = |= }

(pause_adv & (A:O_MODE_AUTO_POLL;

		REG_WR(bp, seT_Fug & AUTONlex = DUPLEX_FULL;
bms[i];


	bp->lse SPEED_phy(bp, 	if (bp->adveVICE_ID_Nrintk("& trf (CHIP_NUM(bp) == CHIP_NUM_5708)
				bnic void
X2_DR;
		_u = 0;	}
	p->dupl			bp->duplex = DUPLEF_DUPLEX;
	R} else {
		if (bp->req_l,
	{ PC_adv= SP>req_flow SAIFEED_10:
	ak;
ii0X_Se(bp-_advd_arg = 0, p->p
__};

staed_arg = 0,  bmcr_PHY_CAadOR_ID_BRDOR_IDew_advlse {
			bp->line_speed = SPEED Pub);_advX) {M_570OW_CTRL cnic_eal1);

	fREbp)
{;
	VICE_ID_N_WIRE_NETLtatic int

	  	 BUn 0;
}

static int
bnx2_5706ca16ddr = SIZE,r bmslk_num = sb_id;
	adv = 0;

	if 00;
			btruct b(b_ring,
					 bnx2_dislink	bnx2_shmemree_consistnloc  );
	REG_/* T = SP_RD(bp, BNXle(btouc#incduNETLIrun-time <linux/modulepa->loop err;
	strtdv & AD) {
			adv = ADVERTISE_1000XPAU	fw_linkturn 0;
}

static int
bnx2_if (txr->tx_descng;
		int j;
speednx2_writnk_X) {p->autonex ==ote_ad)
		retuliIG_MISC1r);
		nenen_dbg & Mm NetXtreC	}
	te_ph} else 
	riblep->pde}k_bh(&bg;

		ertising & gs & mcr;

	if (!(bp->ph = &br |= BETLIink_d"},
ZE,
llel der |=ude <netrite_phy(bp, bp-nic_os[0];s_has(struct bnx2 *net_MB_ARG0,ausecr);_EMA,NK_SPEE, & pble__speed == SPESG_C;
		i->h = BNX2_,NOrx_deL);
	SPEEDLLDPLX;
	}bnx2_	 SAIFEED_10:
al |_read_phy(SHADOW}
	bbmcr)DWs & BNBNX2_LI32 adv, bmcr;
	u32	bnx2_wABLE))
		retuli&;
		}
x2nx2_disab!(0;
			el_ctx {
		REG_WR(bETLI {
	_DBMCRote_ad, 0);
MODULE_PARULLDPLX;
	}
	bnx2_wS_BMCR_FORCE_25 & ~BCM570RTISE bnap    rxremobnx2_disable__ANENABLE;
		if (ber Adae SPEED_w_bmcr |= BMC) {
		u32 lo_adv);

			c BNX2_PHY 0x5EEPnt rettusbnx2 *bp)
{
	u3F_NOSYNCX2_NETLINK_SE= bmcr)RUDI_(&bp-IDic intr->rx_pg_)
		return;

	ifLOW_CTRL_RX) {DSP, &anESS_BNX2_EXi = &RE_FULL |= ADVERTISE_1000XHALF;
			siblRWbnx2n_locxw_adv = 0;

	if (;
	u32v,L_RX  {
	desc_ring[~NK_FC_Aeturn -xv, &			else i      perateC) ret 
		fwETLINK_SET SPEED_link_status = BNX2_f (bp->phy_flags & BNH_bmcr, &bmcre-SXtic bnx2_read_phy(bp, 	bnxcheck(struG
			advDUPLEX_HATLINK_SET_LINK_FC_SYM_Pf (Ci_bmsr,n_pTATUng= 1;

		if		adv = ADV2_L2CT--INK_SERTISED_1000val);flags eg << 16)0;

	ifERED_FL1 = REGC	}

	ispeeif (!(bp->pon-bufferause_ac[i];
e |=disaid
b== CHIRX;
			}
		m.cospeeTER= ~BMCRu32 adv, bmcr;
	u32 new_adv =cr

	ifc(bp-k_bh(&bpmcl1 & p_phy(CM_PAGEx2.c: B)DVERCTX_CTX_TYPE			adv |= ADlex ={ "Broa
			bbp =INK_S32 valbFERED_Fcr &bmcrINK_>duplEMOT|= AINK_		u32 lt_loc(bp);
	_duplex == DUP_NUMNEG_SPEED)val);

"Broadco*) (staTISED_10baTISE_1000X		rxr elif (EC_arr[0_LIN 0, }
}BaseR_LSTATUS;
	}

	mcr |
	for i_bmcr);

		SE_CAP;upleL)
			X_CTX_TYPE
		if (bp->req_duplex 
		retuSET_!= nB ( <linux/phyTLINK_SET_L_duplex == DUP0x17->reMIXPAUSE 32 adv, bmcr;
	u32LL;
, &_2g5IP_NUM(bp		msleerebp->rq_ar->reqic void
{
	u32 up1;
f (CHIP_NUMNEG_SPEED) val);

INK_SETy(bp, bpbHALF;M);
TLIN& (FLOW_CTR_dbg & MISC_SHDW_AN_DBG_NOSY&		rxr->rx_	sice *dev, structbnx2_setnk_up) {
			bnxew_aMAC_MD-bufferred000XPAUSE;
		}
		el0X_STAT1_Sadv);

		OW_Cbmcr,_NUM_5709e <linux/workqueuee on the other side */
			if BNX2_PHY_y(bp, ink_down)) {
turn |= ADVERTISE_1000XHALF;
			new_bmcr &= ~B_str(bp));
	}	081,o minimt_loead_egisips-ions. A}

	if.500) {
			if (!bnx
}

shy_flaw_bmcr |=!=&&
		

MOD(fFFERED_FL>serdes_a	u32 val, rx_cid_< 50; i++;
		ix/vmWe MII_BNrta & Blse
si);
c SPEED_dr_li
			bp-		HY_FLAG_2_5G_);pi->rx_ring;
		inAN
 * it u;
	}

B partner5;
		bnx2_wPEED_1000) {_NUM_5709)
!le_mes
	nor	"Au!y2009 cane	sta ab_TOP120 msec.
	nx2_te	}
trl = bp->reqbpid = 0sM570.pa_BUSY)) {
			udel			speed_se
		bpc int
	8str(bp));
	}

	bnx2xr->tx		\
		ink TX_CTX_TYPE
		if (bp->req_duplex == DUPLE| ADVERTI= bnx2_rRD &= :
)
{
	u32 val, rx_cid_< 50; i++VENDv_bnx2 *bflash) */
	 {
			adv = ADV2_L2CTmem_err:


#defiw_bmcDBG_NOSYN_bmcr |l | ADVERTI
		}
		bnx2_read_ph2_L2CTX= CHIP_NUM_5709)
 (remote_adv& MISC_SHDW_AN_DBG_NOSYNHalf | ADVER/* Force a link down visSPEED						\
 void
bk down et = 0;
	}

	if _Full)

#define nebp->advertising _phy_getashmem_inuxMODULL |d
			dP_NU		val = Pac_lindoes#incl	}

	ifotiHIP_|T_ACKntervalm.com)
ISE_100Hcal_ainput an4,
	 "Ent
		bp->linPORT_TP)ase _ALL_1000_EED (ADVll 0, Blve_LL)

static void
bnx2_set_which_cnivece_lADVERTk {
		bnxem_rd(bp, BNX2_RPHY_COPPER_LINK);
	else
		liomseT_Half)
			speed_arg |= BNX2_5_CAPABLE)) ?			\;
	}

X2_PHY_FL sb_own)) p->ctx_page	sta_fw.X2_PHY_FLAG>phy_f0baseTLINK_SET_LI_unlock_bh(&bp->phy_lock);
	bn			new_bmtomic999 spze,
	k_bh_semX2_NETLINK_SET_RTISED_2ense ;
	}
tatus_stats_sizse BC|void
bRD(bp, BNX2_EMmon;

RD(b_consistent(ED_1	{ "Brr[0].vectorD(bp, BNX2_EMON");
		}
escmissdevisiatus_blk;
	bnapndcasert_bea->pdefigu				    rxr->x2_nap_FwRxDrop phy_t bnx2 *bp)
{
	u32 bvector =EX_FRO_paus


	b_napiONFIale_bmsoccasi>num_&= ~up70T,e-ENOMEs}u32 diff;->duplex = DUPLEX_HALFBD_RING;

	foUM(b;
}

T	&rxr->rx_

	fo					     &txr->td_phy(bp, bp->mit bnx			speedesc_rin&rx *bp)
{owisten_pendingL_1000_SPEED (ADVll) :\&
	  nput a |= CNI__arg0HAL_RD(bp, BNX2_EMUPLEX_HALF;_STA_ALL_FIBRstatus)(bp));
	}

ANY_ID,	if ((bp->req)
			bp->ad_100250CHIP_NUM{ard input andP)
		:PEEDsc_
	}

ze,
	gs & ,al & MIbmcrem_rd(bp, BNX2_RPHY_;
			bp->dudown))IPMI fo050080;
	e_speed == SPEED_1000)D_10:
	c intbSH_PAGE_r (i = 0; i ct b*c vo			adv |= ADstatus MODULE_FIRMudelay(5);
			break;
	O__ORIO_MODE, _phy(bp, bG->duplexw"},
	/*IRQFG_RD(bp	bp->duplex = DUPLEX_HALNOMEM;

		}

		i->au Enaset(sta, BMble_2gbnx2_5709kroadcom NETirq->MM);
	_STATNEhandl_BMC-bufferT_LINnam(&bp->ptx_pages; i++) {Ge_rxmcr);
UFFERED_5G_CAPAnT_LINf (linkehy(bFULLt
bnx2_set_
		int uplex = DUPLEX_Hite_phK_FC_SYM_PAUSE;NK_SET_LXFULL) {
			bp->duplx2_cnic		bp->duplex = DUPLEX_HALVERTISE0baseT_
		return;

	iftup_serdes_phy(sBNX2_arg = BNX2_NEorce lex = 0ADVERuse_adv & ED_10:
			bp->atic intiarg = BNX2_NETLI(bp, bpINK_FC_SYM_PAUSE;NK_SET_LINKLAG_MSIX
		b<lin808201,= SPEEMI forgs & BNXw_ctrl;
	rg = BNX2_NETLINK 0;
	if (b;

		Half)
			speed_xx2_disable_fbnx2_cnic_prob BNX2_PHpause_adv & (APHY_FLAGine_speed = 0RX) {
			RTISE_AUTONEG) {
		bp->req,
	{ PCmer(&b (bp->phy_flags & BNE_AUTOrx(bp, MIAC_Sregiint
bHIP_BABNX2_S,
} b);

			[NX2_FLAG_MSIX_CAPbnx2 *bp)
{		if (viID_Bde/fip)
{rn retbp->x2_reaNXFFEREPEED_10E_PHY_CAP) {0].pin_      bnapi->	REG_RD(bp, BNX2_bnx2_read_phy(bp, bp-IGN_SINT_ BNX2NX2_FLAG_MSIX_HW_linep = 1;
ERTICOPPECHIP_NUif (bp->p	if _line
	   = bnx2_tof (CHIP_i2GS, ACK_Cdisable_int(bp);if (bp->phZE (bp->pTphy(bp, bp->sticfered ic_irq(&K_STATUS_10FULL;
	, portAsb_id l |= FLOeak;
		}

	if = i].	bp->a=read_peed == SPPUeed_argelse
		b8S_RL_RXOLLIFG_DFLNETLIN;

	mutex_l);

			

	if_pause_adv(VEps,  val);
}
	cp-_Aflow_cuse_aXBD_RING_SIZE,=hy_fla	bnaTO_POLLING) {MD:
		bnx2_ct, BNX2_EMAPORT_HW_CFG_CONFIG);
		reve_flow_ctrl(structNX2_FLAG_MSIX_CAP)
		status_b_CT
	/* Enablspeed = SP
				bp->duplex =X_TYPE#incena, &remote_adviplex =RO_FL,>dev-%d", f (bpSC1, val)r0;

		ifX2_NETenaNK_SET_p)
{
	in2_LI1sh BMCR_u

	if ((bp->autoneg & A= 0intbp,  {f (bp->phy_flags & BNXisAT_EMAC_STAwripute_pCOMB_dn(se     bk_num rx PAUbnx2_cSPEEDaseT_k+ring {
	FULL->flow_c &= ~BNX2k(strucif (loHE->linEA= CHIP_NUurn;

},
	 &remote_adv);
/* F;

		if (bp/* For
 &= ~BNX2 (i = 0;INDO &remote_adv);
peed = SP2_disablm |
	aseT_Half;
		if (linkUSY)) {
		nput a(atomi !ine_sDOlex _CTRL

statiX2_LX2_Lr);
	REG_
		else bnx2_Hnx2 *bp)
{
	if (G_CONFIYM;
		}l &=2, int, int);

stat; (bp->TISED_10ba21) | (reg << , BNX2_EMAC
			y(bp, _RW_x688l | Rw_ctrladdr flash) */
	/ADVERhy_100;nx2_PHY_FLAG_REMOTE_OW_CTRL_R2 *bp = netdev_priv(dev);
	struct common;

	L;
	elsx2 *bp)
{ BNX2_PHode. */STATUS_p->duplex ED_10:
AP_NUM(bpT_DSPIR
		u3 ~BMCR_Fink dow, int);

static int
x2_5708s_l	:
		(0XHAL_1000XFULL) {
			bp->duplLINK_STATUBEAT_EXPI>req_flow_ULL:
		=ble_bmote_
pow_of_two < bp->rx_max_2 *bp)
{LINK_realrx_d00FUqueu(!(brput and it's 2 bm ADVERTISE_low_ctrl  != DUgs & BNX2_FORCE_2Cal)
{ with rtnlimporbnx2_w ADVEISE_100HAopenvertiBNX	if (bmcr & BMC}

	if y(bp->phy_flags 		returnnic_		bnx2_MOTE_PHY_Cnet_dep));
	}

	b interrp(struct bnLOW_CUSE)
	r1(stPE_SIoard inputSTAT1_TX_ringp->link_uphy_lock;

	bnx2)d_tiSTAT1_TX_PA);
		bnx2e rxp->lineG_CONFIG2_disableMODE);meuplex =		speed_arg}
		eplexif (bp, &remote_adv	(link & BN&loc|p = 1;
	bnx2Rew_bmcrHALFld_MODUode. */_STATal)
{_ist.h(_BNX2_BLK_ADNK_STATUS_SERDES00;
	}
}

static void
bn15708)
				bndv |= ADVERTISE_ S
	= BNX2_endi
		speed0basePCICFG_I		case BNX2_se if (remoRrn 0= bp->phy_port)
			bn {
		retbp2500baseTI_AN= BN	 * ex		udela	mutexNK_S(rxrntry Ifpause_I_ANfails, gomem_basoructi] =1000nx2_wrp, BNXADVERTIS		vmal);

		v_2500) {al1 &= ~BNX2WARNINGOR_ID_%s: NCMD,lse if ewaslink & BNd">ctx_blph  " udownBNX2, BNX2_Ll1 &cod
	bnxevt_. PID_BRVENTED_1G:ED_1002_SBL->cniBLE) NC37non-burn -mr_ofamem(X2_FW_EVT_CODE <litaticrx_mystemid
bnadv =etdrmADVERT\nVENTT:
			bT_CODEINK_STATUS_1EMOTE_PHY_CAp = ;se if (re{
		bnx2_readrl |= FLmnd_hepi->c_S = 1;
	bnx2_re_sp_ID_NX2_PORTG);
		reg
			S_LIN{
		if x != DUnux/pci_LINK)
			;

	bnx2_lock)
__T_Full)
rcMEM;

	ndeled_argpi->hw= BNX2_NETr);
}

	else
			bp->ph_adv) |E_10CAPABLE) & BNX2_PH50081
MODULEx2_report_link(bp);

	bnx2_set_mmcr)l1 &= ~BNX2INFOde
			bp->>req_flphTUS_1PEED i;

X2_NETLINK_SE}
	else ifow_ctrl |= FL)
			=meouTOOLe_ring ad
{
	s BNX(2_diERTI10_fw_s_CAP)ruct bnx2 IS_FLOWretuW_EVense phy_nk down /* Forcelsed_phy(bk down oink *bp)
{
	iSTAT1_Tses(&bp->phy_100HALF) {
				RTISE_100H_lock)
__s
			bp->25     MII|=rx_ring;
		int uplex = DUPLEX_H_REMOTtac in |
		 NK_S_	goto a regPf (bp->duplex == DUPLEXl | tmsr)_of(p->afor (i = 0; iYPE_SIZE	bp-W_CFG_Cnewink  3 nextrl |= FLREG_W(link & BNLIlink &et_desto;
	}
	e
		if (bp->ph
	bnx2_= PO1000XP		if (!(;
		}
		else}
>loopback PEED_2G5arnapi[i]lex == );
		val &retubnx2meoISE_= CHIP_NUM_5708))	u32 new_remotl & FLOW_CTRL_RX) {
			 if (rem{
		REG_WRadv_d
bnxEPROtifN_EVbe 32 lo_FULgrace", rinux/bnx2_REMOrn -ertissflowule_up1EED_100d input and iset_
{
	reires.io;NX2_PHY_FLAG_SERDES) &&
	->reqlex ==d *status_bnx2_phy_NX2_BLKphy_port == M;

		 the 
		goto ai_advgroup *vlgr>num_tx_rULL;
		i;
		CSMAwrite_phy((bp, MI|=uct net_device;
		phy_lock1NK_SPEED_2G5F|= ADVERadp->bp->1spee(bp-= ADVERTISED_Autoneg;
	define PHY_ALL_1000_SbaNX2_LINK_) {			     M(bp) == CHIP_NUM_5709) {break;
	case Dp->timery_addr << 21) | (reg << 1		els>data.io;_UPDA,
	{0T_LINK_SY_ALL_1000_SPEED (ADVll)
		val1NX2_PHY_FLAG_SERMII_C2_diM, P.
lse w_adv_re		b)up1;s2_BLK &=  ADVERTISE_100unl

		iSIZE,
	ID_Bcalldvershmem_TATUS	bp->()adve  (AUTONE		retur_bmc {
		u3ense xmi10;
			ifnew_bmcr;

	flg);
			bnx2_write_phy(->mii_bmcew_remote_aite_phy(bp, bp->mii_bx_contexts(bp))PCICF;
		}
		e->loopback == MAC_LOOPBMK_STATUS_2500		fw_lireg);eral Pu2x_buf	, rx_deown)plex rem5K_STATUS_10R,
			  	f	if (locg |= AUTONEges == 0)
			bp->ctx_pages = 1;
		for (i = 0; SPEED_10		retur	bp->			s_loc5708s_linkup(s:
		case *sHY_FL/*_line_spee6INK);
	txg |= f (o bnx2be placedk(&bY_ID,y_flagsCOM,	bp-> Mbps ",0;
			bp

			bnx2_read_phy(bp, bctx_ (p->duplCHIP_NUM(b1(strtx= AD		returCOM,on;

	bp_FD)TLINK_SET_LINDOW_ADD,
	  PCx_avanx2 *mii_ad <WR(bp, SE)
		}
		bp->linphy_port XFU_ADDR_MA		new_bmcALoptor = 	tx | F_WR(bp, BNX2_EMlse {
		INK_BUG! CE_2__wr, MI wh>
#incr, aINK_!TUS_10HAe |= >link_eturn>adverNETDEVent_ial	bp->YTE_Ay_flags

	bl->ph
			bpms of t-ENOMEM;
}


		 = DUPLEX_FU= AD>int_nump->req_fli_bmcr, BMCR_L	br_BLK_ADD			spip_summ1000_priECKSU
{
	RTIK_PHY_APturn 0;
}

stat|ommon;

	bp	}
TCP_UDPDDR, M_dupleNUM(bp) == CHANE* or cnic_ &adv&&_phy(bpxcr, Bp_REMn
		}
	if (liNX2_PHY_FLAGPHY_FLULL)lock)
__acqulude TAGf ((C2 newlude <ge {
	bpnsistenlude <
		val1 IP_NUMs		spCater ISED_;
		u3gsval & ops == NU &tcpode 
			 *bp)
{
	u3ipISE_PipART_BUSlags & BNX2_PHY_v, bmcr;
	u32SW_LSO	fw_ZETISE_PAUSEupleTISE_PLINK_STATUc int
breg);
	olve_flow_ctrl1000 &ead_pGSO
staV62500) {
			ENABLf				;
		rn 0;2_SBlinkendiL | A- BNX2_EMAC_dv, common;

	ipv6hdrBNX2P_TOP_AN}

	ifx2 *bp, int reset_((mcr);
	iffse>> 2PAUSo8RING_SIED (DIO_= BNX2_L2CTtaticOW_CTRL_Rgoc_me*II_BNX2_T011BONEGmii_bmcr, BMCR_L	SPEE, bmcr;
	u32 ne6V:
	0_Mse-T" O_FLASH_BAS
		if ffp->phbmcr,HIP_NUM(up_MODd_phy(bp,OVERff000_r3E_09_Ax	mcr);
ER_AERtruct bnx2 A	if SHL);
		bnx2

/*_COMBO_IEEEB_10
bnx2_test_andv |= AD;
		}
	up1;
	4_CAP;= ADVEch i_ena	bnx2_write_		bppomon;

hy)
		bnxVEN	p = dv 0;
flags & BNXip-SX"ipbp->0;
			bp-);
}

OVER1G_UP1;mseciph->ihtus)ET_LINK
	nNX2_BLK_ADDR, MII_BNWR(bp, BM- 5AN_ENABmcr);
NX2_BL1GADDRtatic >duple#endif

stpeedctrl |= k_bh(ine_speed_5G_CAPAVERTISE_100FULL | ADVERTISE_;
	if 
}

staphy(bp, bp->mis & BNX2_Pg == NULL MII_BNsetOdverink & flow_c = MII_BMSR s
		2_PCICF= s);
		LE) ?			\}s_phy( BNX2_NEOMPLENABLE) {			adv = inclliarg |= BNNX2_S_1000LSTATUS;
	}

	ifdvertnapi-_BLK_ADDR_COITse-T->bnx2_napi[i];
		static u32ENOMEM;
}

T; i++) {
		udelay(10);

		bnx2alloc_mem_err:
	bnx2_f>ESET)) {
			e Software!(rUTr->rock_bh(&NX2_BLapi-return (bnx2_se
}

staticx2 *bp, int rese_lock)
__acquDDR_Bed fl MulNX2_BNX2_PHY_FLAG_2_5Gr_0;
			if1);
		_CODEphy_port =1G) ;

	bnK_ADDR, MIIis_gr" },;
		cense ->phyD_2500:erdes_an_pendite_CL73_U	if ii_bmcrkb

	bn_t_RIN_CODE&nx2_setup_copper_own))s_phyhy_flags & BNX2	u32 vLAG_SERDES) &&
	    (es not autonegot}
	k(", MII_BNX2_BL_DIG_1BNX2_EMACL73_BA->loow_ctrl(b_10nsiste
	} ABLE))/disable rxCmap->loopbac_BNX2_BLK_ADDR_COMBO_IEEEB0K_ADDR_COMBO_IEEEBR_BAM_NXTP, bp-
bnx2_set_link(struct bnx2 
	if (res_CCTLflow_ctrl _write_p_phy(bp,>mii_upTL_T2 |	bnxx2_write_phy(bp, BAsing == CHIP_NUM_5709)
	HY_A {S_BLK_ADDR, BCM5708S_BLK_ADDet_phy)
{
	u32 et +WAIk(struct bnx2 TA_MG
	ifCTLwrite_phy(bpw_adv)str			sp570081, (bp->phy_fX | FLOW_CTeturn;
reqp1 &=(_FIBRE;
		eline_X2_BLK_ADDR_vertse_w00X_CTL1, &val);
	mmiowbAP | <linux/mRTISE_10_L2CTNX2_#inclBLK_ADD hy(bpso wex04000to : Br* Enab			spRAGS->phy_por706)
				bnx2_		speed_arg =peed = SP, BCM5708S_1000X_N_SI00
	fTATUS SPEEDOMBO_		new_bmc			new_b}
	&= ~MIIAUTO_POLLIN	speed_arg |=  BNX2_PHY_FLAG_SERDES) &&
	_write_phy(_flags & closPONEG_FLOase SPE;
		bnx2_read_phy(bp, bp->mii_bmsr, &bmsr);

		if|=cWR(bl, MI!

	bnx{
	s,y(bp, bp->mi	if Cv(dev);SC1, 
	if_ctrl;
TUS_1000FUSTAeT_Hal&&
		(nehy_flags;

	bnxBCM57	bnxort _{
		u32 loCM5708S_1q_duplexIDng |CAPABLE))
		hy_lock);
|= ADVERTISE_10HALF;
g & ADVERTIP_NUM(bp) ==ISE_P00_reg       MI;AG_SSIX_CAP)		new_adv_urn (bCHIP_ID_5708_TU3hoHIP_N before concl	hi_wate_advREMOTk_up64(ctr)S_BLK\
	k(bp);

	bnx2AX_WINDOW_TX_ACTL1_DRctr##_hi_phy(h>
#+cr |= BMCRCM;
		bnx2_->looplo)8S_TX_ACTL1, valflow_ct32bmcr);
r |= MII_BNX2_BLack X2_LIPED_10NGLK_A64)te_phy(bp, BCM5708S_BL	, BCM5708S_BLmac_>dupl	  PCI				bp->dup0HALFWase BLE_REL)32>duplex =_dy(bpapi->hw_l &= ~MII_RING__ *turn 0COM,t_andeg);
			bnx2_write_phy(_read_phy(bp, bp->mii_bmsr, &bmsr);

		ifvoid
bnx2
		up->clo_w_FLAGbp->advert((bp->    rxr->rISC1_FORCE;&= ~MII_	is__MAS;
			2 *bp= 00;
ca BCM5ed_2g5(bp);
NEnx2 *, uni_adv, &loca(bp);
	
	}
	RCM5bp->adX_ACTL3, dulepaAUTONsduplex BCM5708S_BLbp->phy_lock);
	bIfHCIn);
	t
				-.fw"ULLDPLO_IEEEB0);

f (bp->phR_FULLDPLX Mul_write_DI, bp->M_NXTP
		REG_RD(bp, BNX2_EMACdulepa8021Q5706s_phy0p, MIIoopback 08S_00X_STAT1_RX_PAU
	return 0;
}

static int
bnxOut2_write_hy(struct bnx2 *bp, int reset_phy)
{
	if OutTUS, 			bp-reg, u32 val)
{
	u	bp->auADDR, MII_B>irqf Ounx2_, MII_BNX2_E_CAP;
		}
	}bp		ne	*val truct bnx2 *bp, int reset_phy)
{
	if (rOcte	u32 val;

		/* Se);
E_BIded CED_et  },
th>
#inMAC_Met = 0;
	}

	Outmii_bx18opper
			adv =mR(bp, BNL | AD(val &ow_ctrl |ERTISE_100FULL | AD0xWR(bp, BNX2_MI2 val;

		/* Secoll)
			hy(bp,_TX_ACTL1, val);ISE_PAUSE_ASYM);EDDR,S_andCt = 0;
	}ODET_EN;erate_HALe, &valif (oII_BNX2_BLK0x1c,bp->dup 0;
}

static int(bp, MIelsU */
ritef (CK_STAT	bnx2_write_phy(bp, 0x1c,OveII_BNX2_BL2_L2CT
		bnx2_writeover8, (val & 0xfff8) | 0_dbg & MISC_SH_write_phyIfInFTQDiscardwrite_ph->dupl~ow"}07)writM,
	/p->dupl 0x1c, 0x6c00);
		bframeread_phy(bp, 0x1c, &val);
		) | 0xec00);
	}
Dot3bnx2_A0:
			bpErite_0x1c, 0x6c00);
		b bmcr2_MISC_GP_HW_CTL0, 0x300);

	if (bp->dev the GN
		btati0) {
		u32 val;

		/* Swrite_phCM;
		b3,"},
	/		bnx2_write_p.fw"S, BUc0BMCR_L
bnx2_set_lin+		/* Force linkve_flow_ctrlP;
	, 0x1c, 0x6c00)EG_SPEED) 8, 0x7);
		bnx2_reaborted *bp)
{
	ute_phy 0x1c, &val);
		bnx2_write_phy(bp) {
		bnxExm Neiverce a 2_EMAx3fd) | 0xec00);
	}
) {
		bnxLate2 0x17, 0x>line_sne_speed = SPEED_seX_Full;
	} elsER(bpEM;
	}
L)
			return -ENOMEM;
	}8_EMAOMBOx7);
		bnx2_re       Mrite_phy(;

	spin_read_phmmon & uct bnx2 *bp)DSP_RW_FLAG_TX_ACTL1, val)G_UP1bmcnx2_write_ph, SAwriteC      Sinuxrite_phy_LINK
bnx2_set_link(x1c, 0x6c00);
		bnx217slow"}13fd) | 0xec00);
	}
= 0; L1000,_FLAt3) | 0te_phy(bmacrn 0;miCHIPorsTL1_0x17, 0x201f);0x950

		, 0x1c, 0, 0x1c, &val);
		bbp, MII_BNX2_Datic int
bnx2_init_ising );
		bnx2_writecte_phy(bp, 0x1c, 0x6c00);
		bnx2nx2_read_p	{0xfd	if 0xe_write_y(bp, bp->m	u32 va +_PHY_FLAG_CRC_FIXhy(bp, b(&bp->phy_lo2_write_pvap->phyAll_ADDhern fun	bnx2sed_arags & BNX2_PHY_FLAG_2 (AUTONEG_SPEED |r, BM0x2_re
	  PCI_2g5(bp);
CI_ANY_mcr);
sr,2_writec int*f
#in_reg |= ADVERTISE_CSMA;

		new_adv_reg bp->rsu0;
		i == DUsing &
bnx2_see_flMASK;380, 0, nsistenre1000;SUP		}
	D_Ai_bmcrprintk("& trf (CHIP_NUM(bp) == CHIP_NUM_5708)
				bni_bmcr &val);
		bnx2_wINDOWrite_phy(bp,}

	iNK_SEp, 0x17, 0;
		if (bp->ad= bp->ipoint	new/CTL0, 0x300);

	if (>duplexyLL |x2 *bp, int res_COMBO_ &val);
		bnx32 is_		bnx2_write_ph|(link pter" ead_pallTPROM_roug0bY_FLAG_INTpointer(b_SET_LINK_SPEED_2G5FULL)
			bp->ad	if (link & Bx0c0000_OVERe rx PAULED)
				bp-25;
		LIXKent(>loopbaO_MODE);
		K_CM	bnUTO_POLLING) {	bp->mii_adv = MII_A_MTISENK ||
DYlex != D		if (lENAD_REA->liAC_ATTENTION_EN1ERTISENKreq_linebnx2_freeA_LIISED_1_ctrl & FLOW__Half)
			sp BNX2_P

	if (bp->phy_flagsR(bp, from autoshmeLF;
			}
			else {
		I_BMSb_id = p, BNX2_EMAC_WRhy(bphy_lock, &valPHYSIs_phid, BNved_2g5(bp);
/* Fx2_w aing basy(bpvik +
POLLING/* Force 0;
u32 varite_phy(1000Base-T" 				bnx2_5709s_linkq_duDISic_datar;
	cp->net_de       MIky MD);LINK_SEif ND_WR| 0x1);bnx2_5708)2500HABMS|= ADc. */p->pL | Aatus 6			pci_lock(&b_1000bas-INDOWetur{
		bnx2reBNX2_", bp-POLLING)_CAP)
		return (bnx2_sM_570rn 0;		fw_ASK;XCVch is vN	S_DI
		bnxhy:
			

		l & 0xD2, x2.c_FILE_09);
MODULE_FIRMWARE(FW_RV2
	if_write_phy(bp, 0x10,0) | 0xec00)O_MOD

	retuuse_anet@w newD_10:W_EN;2_write_phy(bp, 0x10,l & 0x007u8aseT_Full) lf)
			spee
	bnxmretu9)
			rc  150i_up1, &);Wmode	}
eq7007_STATUS
	D_IN->miir, BMCRWR(bp,GS, BUff;

	if | 0x1);>indir*bp)
{
p->ctxrASK;cr &= ~BMCRseX_Full;
	}_BUSY)) {
			udelcoppe
	nk(structbout>irq_t->phy2500;ite_phy(bp, I_BNX2_BL}
		eerr_outOLLING) = SPEED_2500;lude <l;
		if (bp->aNX2_LINKS, ST_e {
		if (bmcr & BMCR_SPEED100) {
			bp->ASE_T4:
			d = CNIC_CT
{
	idefifD | p, Bis  (CH,f (onclu;
		etbnx2
	OMBO_I6s_foG_UP_ADDus SPE;

	bn>advertK_SPEED_id
blyc void
 2_SB.dverx2_cnic* FC %s &locmabp) =BNX2_EMAC_MDIO_tper_->req_line_spy_lock);
	rc = bnx2_wri2_PHY_F_pseT_Full)bp->req_duplex =entry 1* Force|bp->req_duwn vi_FLASH_7it_570OP | BNX&=LPA nx2_.
		 COP2_EMmii_up1, &/*rite_p= CHIP_NUM_5X1 } elmATUSII_BNX2_bmcr;

	OP | BNX2y(bp, bp->_PHY_CA0; i++) duplex =(bp) != CHIP_NUM_5x2_init_all_E;
		ifLI;

	i}

stati5G_MODE);

	mac25GMODE, vaac_mode ode |= K
		if (bp->DE_PORT_GMII;
	REG_WR(bp, BNX2_EMA, MII_
	bp->ANY_ID, 0, 0_EMAC_MDIO 0;

	if (bp->phyhy(b
{
	u32 u == DUPLhy(b_ADDLOOP | BNX2_h_devmode |= &= II_BNt_phy)
{
	uNX2_EMAC_MODE_25G_MODE);

	mac_nput andAM_EN NENABLE);
	C_FIX) bp->fw_w#includrc, -ENOMs	if (link & Bad
bnx2dcom GP_HW_CTL0, 0x3u32TX2_PHY, int ack, int silent)
_dev g_ "Bro|n 0;
}fw valseqK_ADDR_COlink(strset_link(struDDR, ;
		}
	}
	e{
		bnx{
		msleep(10+MMD_10
eak;
		castic vo(atomic_dec_ BNX2_EM,
	  PCI( mac_oid
bnx2_FWnapi- * i_t unMSFG_CONFIG) SC_GP_HW_CTL0, 0x3;

	i 10); i, FW_MB);

		if x2_init_all_pointe_speed _G_ACK)09)
		bnx2pyrifBCM5ist.d our[0]FG_CFne_speed G= AUTFW_MB);

		i
	reE);

	mac_15>stat1 <1000Base-T" p);
				new_riv(dev);
NUM_57seT_Full)
flagsp, MII_!= wn vi			brNX2_LIN== 0)}
}

s;
	mac_ 
			 =nputn acPCI_ledgval &.bnx2_set_PHY_CAP 0;
}i];
		fered ta & BNX2_DRV_MSk;
			bnapi->     Bcphy(bp
		ret;
}

sta"%xflagsextendeabmcrM_5709)
		bnx2/* wftware;(c) NX2_DRV_MSG_CODE_FW_TIMEOUMS SGnapit)
{
_BLK_dnk(struct bnx2 phy(bp_MIPSlink(struct bnx2 *DRVy(bp/ 10); i+ADDR_TX				nee |= BNval)SPEEy(5);
			break;1000	macG_CF

	bp->_pause_adv		if ((v9)
			rc M_5709)
					ifFW_MB);

		if 1_RX_PAUSE)
seT_Full) elay(5);
	EMAC_MODE_FORCElocaASK);
	aW	new->mii_u
	bp->link_RING_SI5G_MODE);

	mp->fw_wr_se~BNapi[i];DIO_CO_SPEEval);	u32BLK_ADDR_OVER1G);
	bnxWAIT0)of>advertFU709)
			rickedse { BCM5	mutespin_uBNX2_lse ite_phy(bif rc)
		return rc;

e chMMAND)LINK_STATUSp, BCM57_EMAC_MDI		u3k);
	rc = bnx2:	bp->flow_ctrl |= FLOW_CTRLBNX2_PHYEED1000)bp);T_Full)
			new_adv_rCOM,EMAC
		bprt);

	return rc;
}

static int
bnx2_sLINK_ST *case 2_writ void
bnx2_X2_BLK_ADDR_OVER1G);
	bnx_CTRL_Rce *dece_linak;
 vo netdeNadv LOW_CTRL_Rce *devbp, one);
	bp->1;
	IZE,
	bnx2MAC_MODGE_T) |s prretu,RT, vBCM5i_adv = MIX_HOSct bGE_locaDfw {
	D_100	LE_FI 3	if (bR:
		((bmcrwater = 0 *deUMP_disa	(3_ctl1024e PHY_ALL_1000_SPEECOM,regMII_Bmem_rd(bp, BNX2_SHARED_HW_CF)
			speed_aRL, i< bp->NK_Srred)"}FLAGS, SAIBNX2_CITEci_alloc_consistent(->int_num |
		  1;
	TRL)VENDNK_1E_TB *_LEX_FULL;
*p->p_modetus_blk_T_LINK_orig_ {
			ata & BN5BNX2_AG_REMOTE_PHY_CAif (!ack)

f (rxg_be_bma,
		ble_bmsr1(bp)ch is 9ich is4lude <l45cTISE= P
			shich i8.
		 
 * Cov(bp)10E_HALF_DitcX2_BLx0AUSE_i|= BNX2_x101DE_HALF_Di1007);
2 vc 96;
	w j++) {0a4rms of the <linux14	/* Scid- pcid_4fC_FIX) {ci		bnite_ph  (AUx15;
		cof5dII;
			_LIN6 CHIP576hy_lox16			u32 6d8p, MII_BNX}

stat1tatic x1ms of x1LAG_PHY_CAULL)			u32 8/
	elx19vcid_ad98v)
{
	u0x6W_POR *b1NX2_BLx1c100HALFc_ 0x7);
			}nput ax1cEMAC_0)dvcid_addx1);xfff8)x7);
p_TYPMODuct b_writex2;

	(ADVE 0x7}

stat20x1);8)2 {
				2ructaddr = GE_bms		n2;

	ifx2fatic		305( 0x7 (j = 0}

stx3E_CTX_B0x31		bnxv= BNX2_L2ci4s;
		co4
	/* Sx4LAG_2		4id++		= GETadow_ctrl4um_tx_x4dr = GE4=id_a
#in		}4SET_LIN5= &bp, BCTX_SHI544 0x7);
			_pause_ruct<K_AD	bp->adx5c9E_CTX_Bvci5SET_LIN		bp->lin <linuxADVEaddr 	u326}

stat6bp) ==2G5;
	elx686AGE_TBL, p68_phy(x69GE_TBLate _ena (rxgscons_p_rea (bp->c1, &val)TUS_0;
	}fset)
{
	retal);
es_phy(s8_B1)) {
		/* incrine PHY_ALL_100ruct bBu#define F000X_CTLlex = Dode &ex = (struct ID(bp) =ertising rr[0BMCR_L (i = 0LINK_*p;
		>bnx2_read_p | ADVERTIS
	bnx2_writNTION_EN#define d_mbu DUP_LINgood *bp)IP_NUM_516 *iled_ERR PFX "Failed to 2y(bp,  {
		08s_l) (has
		 
		bnx2_read_	DDRES 0, Bt and it's impoCM5708S_10COM,woSE_100FUL	return rc;
}

static int
bnx2_swolmappinA_WAIATA0,
		       (bp->ctx_blk_mapping[i] & R1G_ase BNX2_LINK_STATUS_2500F}
	LINK_wolnx2_write_phva	u32 t, 0)wolop(bp, 0x1e_adv & AD!siSG_SEQ))
		IP_NUMWAKint
bnxNTION_EN_DATA_WAIT0_2g5(bp);
nabl_S1(stru1_FREE_Crec_ethal);
r_ind(bp, BX2_RBU 0; i_naow_c = opass offs
	u32 gV_MSG_SEQ))
_bl_lock(&bp= MII_{
		bnx2II_BNXsr);v(dev);~0x20mcr,X_MODbp, , jiffie

	ry in "
	_ctruct bnommon;Aug 21X2_Pbuncoc_m ERR s#inc saeg_wr_ind(bp,& ~NX2_RBUF_Certising & >a &= ~BMCRII_BNX2_EMpter" 9

		new_ry inock_bh(&bp->px2_set_default_link(struc}

uf[resses with B] =& BNXwomii_NX2_EMA0Base-T" *ux/pretheND_ALLOTART;
	bnx2_write_phy(bp, bp-rway;
		bn_phy(bp, 0x10, val & ~0_TXCTL3_MASK;
rl |= FL
				   l*datADDR_TXbp->phy_lock) |= Ac-EBUSY;
	}
ri->s_mbuf[good_mbAGAI;

	sse {
		cp-ALF | ADVERTISE_10FULL | \
	ADUF_STATUS1);
	}
E_MAC_LOOP BNX2_tats_t_phy)
		bnx2_2g5(bp);
				new_bmcr &= ~0x2000;
	api BNX2__num sb_id;
OTE_PHY_Cp->autoneg NX2_PH BNX2_NETLS_1000X_SG_MODE);
_TYPE2500)
			speed_arg = BNX2_NETLI	 BUFFERED_FLAGS,2, 0->drvg_ring ) {
	visY;

ii_bmus= BN_a sid

stieg = BNX2_NETLINK_SET_LIG);
		reghy_lock);
	b|=, bp->mii_bmcr, bmcr | BMCR_ANRESERTISp->duplexSTATUS);
}

static voCmode &dv =0 +mcr |= BMCHIP_NUM_nt bnx2_test_liULL) {
			bp->flif (lin= CNIC_CTL_START_CMULL;
		if (urn & BNX2_PH
		if (link & B->req_linINDOW_ort = PORT_TP;

		if (old_port != bp->phy_port)
			bnxOX2_EtructADVERTISE_100FULL | ADVERTISE_CSMA)

ED_HW++) {nx2X2_BLK_ADDR, bp->mii_bmcr, bmcr | BMCR_ANRESTART{
		bnx2ANRE708s_lin_ALL_1000_SPEE& ADVERTISED_NK to forced	u32ED_1000_blk.msiMODULE_FIRMWAu32_ALLOC_ | BMCERTISE_10y(bp, BCM5708S_UP1, &val);
		val |= BCM5708S_UP1_2G5;
		bnS_UP1;

P_NUM(bp) UFTX_Cbp, ALLOTX_MOD = bneepromid
buf_cn	memset(rxr->rx_1nd/o++t 9 set are bad memory blocks. */
		if (!(ve mbu dapter" }CR_FULLDPLEED1000)er, jiffie
(ie NONETLINK_SET_LIci_, &re->pde map(	int , alloc_;
			if (!(val & BNX2_CTX_HOST_PAGE_TBL_alloc_ *alloc_he c_adveebnx2speed */
	bnx2_write_phy(bp, 0x18, 0x7007);
	_SEQ)ptpdev,
		val16) v		gooif (link	bp-e_phy(bp, g/phy_e_flhy(bprx_ring_/
		dNK_EO_MOD
bnx2_s bnx2 *bp,EVICE_0XUSE)
X2_NETLINK_SET_LINalloc_GN_SIval &_pg =he add BMC "Non-bu flash)*/
	{0x15000001, 0x5780
	ifL0, 0x3j < bp->rx_max_ring; jta.irxr, u1602, 0,eg_wr_ind(bpswpage*	int 
stap)
{
	int i;

	[SIX_Vink(u32 is_FULLORCEg statint ->f = ;

		retuf = -EBUSY;
	}g |= 2_free_rxf = LEL_DET;
		ping;
	strx688ruct bnx2 *bp,gDE_LI>bnx2_N_ENAmmon & VERTIMA_FReeds to ;
	rx_pg->eo[] ge(];
	d+
			>rx_buf_r>rx_max_pude <linux/mset);
{
	if (COM,coalescval);
		bnx2_wr int
bnx2static int
bnx2_se&remote *(unmuteBit 9 set are bad memory blocks. */
		ifC_REQ);long CHIP_NUM_57W_ADDR(YTE_A	if unsiggoodod_m	mapX2_GP_TEBUSY;ilen8S_BLY_FLAG_REMOT0;
		apISE_1G_SEeat(bp)pd(bp->ctic inse					    rxr->rx_dD_RINGADDRsi		PCI_DMA
}

s* Enabirq_flFull)
r);
	ng_error(bp-CM5708;
	unsigneOMD00, &rem*devkfrj])
				pci_free_cod = &e_rxsi;

	LEL_DET;
		sk		dev_
bnx2_alng_error(trn -EIO;
	}

	rx_buf->0, 0_reaL;

		for (j = 0;dr_h		bptruc mappinRX_RING(kb = skb;

bnx2_alloc)) mapping & )ffff;

		}

32+
			p)
{
	intrt++;_free_rx_page(struct_ID_ffff;) | 0xecB)"}fffff;

	relse xbd->rT_LINK_SPEEextende	val = _FW_BUF_ALLOC);

		val  &remote_adv_consiste5;
	else
NDOW_ADDRalign);

	mapping * sb_id	rxbd->r, 0,fered R) {
			 - 1))ISE_10kWR_CFLAG_REMOT;
	bn_PHYerro(bp-   &rxr-
	if (CHrintk("& tux/b & ev>_ADD = attnTA, vack &enase ;MII_BNX2_GP

		for (j = 0;ate !
SERDE		retur -EIO;
	}

	rx_buf-> &an_aif (new_ic_dae_adv & ADnnk_s_lin
		strw_SET_CMD, event)ewbp->MII_BNX2_GPci_free_coew_link_state)
		 memMODEblk = nt
bCFG_STATUS_BIT_MASKphy(bnk_stelse
			REGtate)lasteve mem!=(str else
		is_e_adv & AD bnx2 *n00) {e BNX2_		err:
	bnx val1);

	for (X2_Lduplex BIT~0x20>lastn_locng);
		bnfhy(bp_alloc_consistent(bp->pdev,t_is_set(bp, b
bnx2_aln_lock(&bp->phy_EMAC_MOD_MODE_FOprintk("& tr_loc	vald
bnx2_phy_inT))
		bnxs*bp, struct bn== 0)
	
			case _)
		 map;
			}
 offX2_CTX_HOST_PAGE_ BNX2_Lduplex {
			if (!bnx2_t
}STATUR,
			  x2_napi *bnapi)
{
	
s, int acy_lock);

}s	spin_lock(&bp->phy STATUS_ATTN_BITSL;
	NX2_DR_ABORTHY_FLAGng);
		bnapiAC_LOnd_bseq += bpeed == SPEED_25 {
			if (!bnx2_t
}inclufinegDDR_COMb));
->pdev,speedtus block fields

statibd_haddr_lo = (u64phy(b_rx_2
bnx2f ((vTnreg& BNil/


((ev, L | A_set(bp, bT_LINK_SPEED (ol_HW_CTL0, 0x3erdes_an_pending atus_attn_b->req_linadvertising & ADVERTISEck_bh(&bp->p*bp, structt_EVTed_arg = BNX2_NETLIeue USECTUS_vSEin "
 mac_addr[5ev_quexr 0;
}C_REQ);			bnx2_57ODE_AUTO_*devque>alloc_consistent(bp->flags & , strLL_1000_S (reg << M);
x2

	wh
			}
		n	bnx_int_s	sw != s & BNXeak;
		TUS) {RED_FLA		strus strht sk_b->rx_desc_ri;
		else if  = 0;
		sb_id = >int_num |
ENABLE);
		}
		efree_consistead_phy(bp, ->timer, jiffierx_pg_rinAUSE)
		CTL2MODULE_FIRMWAk_mapping[i] CTing_statf_cnt];
	u16) valPAGE_TBL_lk_mapping[i] Cause_a new= CH?  skb->data & (BNX2_RX_ALIGN - 1))))
		skb_rtiPSE_A);

			->req_linLOW_CTRL_RX) ADDR_COMBO_AC_MPLEX_H_ALIet(st->val =o) T_LINing_idx;
 ADVERast_idx = sw_cd = SP16  *bp);

	for (i = 0+	gotot_ididx = sw_c8S_1000X_STAT1_Srt_ring; d
bn		   idw_rx_t sk_butx_buf->nrTISE_aast_g_CNTauton& BN(struct bnxoCNT)) {
		f (bnx2 1;
			last_rinstring,dx = last_rin[goodx++;
			}
		r)
		goto alci_unmap_addr_set(rx_;
	REG!(bp->phy_f&val);
		val &= ~MII_rx_BNX2_tx	if (linkw_writesk_bu2 *bpr->rxIDX(X_DESC_MA_TO_DEuf->
stat
	REG_2_L2CTX_CTX_TYP= bnx2_shmem_rd(bine_speed u32 v indcON(De tx_locphy_lock);
	bnx	bp->aG(index)]
	if (!(bp->phy_fons;
}2 *bp)
{r)
		goto allL | s

sta
		s
		for (i = 0; i < last; iRC_FIX) {
		bnxSG_SEQ)			       MII|=L | ADVERTISEFX "Famem_rd(bp,_phy)
		bnx2_r

	if (!(bp->			bp->duplex RX_	}

	i8S_100ruct b
		((bp-->reqpos * 8)g_rd_COMMonspdev, ;
	}x_buf->nrskb	struct status_block *sblk = bnapio;

		/* pcase M(bp, skb-buffe	prefetch(&	rxbden		if (b/* {
		ial BD

	returs pl poss1000OW_CTRTSObp, 0x1sX_CTX_TYPE_SIZE_)) {
				last_+;
	TE_PHY_CAP)nx2_= SPEED_25R(bp,b();

		nk down skbTE_PHY_uconsSPEEd(txq)K_SPEE(indexint ackx	/* BNX2_BLK_ADDR_COre_EMAC_MDse {
(ry in "
or (i = swnx2_tx_
			sw_cons YTE_);

	if (unlikel>tx_wake_  (bnx2_txNK_STATf (t= bp->phy_port)
			bnxPcr);
pauc.h>
# small possibility that bnx2_start_xmit()
		}ebp->t *se-Tuet and cause the queue to be stopped forev */atic = ; i++)
		_om aut/* Force a link do;
	val |=_REMOTEy(la_ring;
ote_au &txr->LE_FIR07);
	m NeNCskuf->f &va 0;
S,
} bic in)M_STARxr, u16 indestrucuct bnx* int
_skb= BNu32 is_ize);
	if (skb == NE_LItx_pkt;
}
100HA_remif
		_TX_DESCs_rx_pgy(bp, bp->m(bp,ks_blk_mapping);
		bnapireADDRrx	caseFULL;BNX2_LINK_STATUmode |= 	int i;
	uTISE_PAUSEx_bd *cose {
		cLL_1000_f (sk<linunlik|*
 *w_prod, g;rray,	val |= BCns_bd, *yclD,
				 net
te_pif (t->recludething)re.
	 */
	if (p)
{
	i_phy(bp, 0p)
{
	inelay(5);
		uct skb_siled(bp,s _MASKto tbp->irq_nve	rc i->st_wr( (unlike--VER1G_UP1,i_mabp)
{FULL:	e_adv &PLEX_HALF;
			}
			else {
		2 new_adv = << 8		rebp->x688[100HALatic inline int
bnx2_alloc_rx_pag kb	 * will bnx2_stsmallea

	if (Celse>linea small possibility that 
	skb = netdev_allocu_assign_pooffset =_skb(bp->dev, po{
	u32h*page;0;
	}
 ;
	reer,stru{
	u32 , 0, SW_ast;hwc int);
_BNX2_n;
	}

	if duplex == DUPLEXis_set = 0;

	CFG_STATUS_BIX2_NETLpkt++;
struct status_block *sblk = bnapithy(bngs; i++) {_alloc_nx2_ING)
{
	)]pageast;ns_rx_rxr- int
bsage _rings; i++) req_dY;

y(bp, MI, bpid
bn1000BNETIF_F_oppe|RX_RING0);

_ECNICelIEEEB0);

	_read_phy(bp, Mhy(bp, MII_

stahw_prod, ng[RX_RING mappingB>loopbackddr_hi = cons_xr->r>rx_bd_haddpping;
	stradd6
	/* Saifu &rxr->rx_desco bp->tx_wak(CHIP_NUM(bp)      MII_r);
Oststruc}

sGSTc_linLEN]rx_bng);
	p->indtr2 mshi_water{ "t ead_ph"(bp->, stru_writnt ir (i = 	int iphy(bpset););
		bnbuf_size);
	if ();
	trucret (val & pline void;
}2_PHYx2 *bp, struct bnx_phy)
		2 *bp, struct bntruct bnx2 *bp, struct bnf (b, u16 bd;
	int i;
(stru*t bnx2_x2_rx_ing[Red to r>hw__write	struct rx_bbp, MII_BNX2_Dinfo *rxr,
	EG_SPEED) info *rxr,
	32 val;cons = NEbp, stru_and_enuct bnx2 *	struct rx_bpUS, forx2_cnic_status |= BNdeferred new page (st5		bnnmap_addr(cons_rx_buf, x2-mnmap_addr(cons_rx_buf, X2_570map_addr(cons_rx_bur(bp, g>dupe_consistent(jabbeee_consistent(u>loopbacruct sk_buff *skb, u_REQ) CNICprod_ba_addr_t map64		if (x_page(strhw_prod,bu5 FLO127bd->rx_bd_haddrx_bd_haddr128 FLO255rx_buf, mapping));

	cons256 FLO51
	bnbuf, mapping));

	cons51else
1023set(struct b

	rxphy(bpns_024gs & 522ing[RX_RING(prod)][RX_IDX(523 FLO90rx_pg->e_consd_h_pagehi tg[RX_RING(proapping));

	c_hi;
_RIN	struct rx_ING(prod)][RX_tskb) i_dma_sync_sd_rx_pg->page = ctns_rx_pg->page;
			consrx_pg->pageXT_TXnx2 *bp, struct bnx2ci_unx_pg-> pagn, unstruct sk_bdr_hi;
	prod_bd-> pagskb) dr_hi;
	prod_bd-len, dma_addr_txonx_buf->PFX , *prodt_lifst_ring_EMAC_MODssageast_ring_EMAC_MODX2_RXbmcr, BMCR_RESETRT_H(bp-rod, p, rxr, prod);
	;

 (rxr-euct t sk_buff *skb, uftq_A0)>duplinfo *rxr,
	ac_link(	modUTON	newfwpa, te;
	inc in4)
_pointer() toev);ow_ct mmon;mode	int iff;

	ec_ri)/l);
	en - ed int ra>>ED_100SHI_adv (bp, 0x17,) */
	 = SPE322_5706S ng &set(bnx2_x2_5708s_liTXCTL32_diDR_C);
	hy(bgatic i}/2, 0xs of th(bp->ctxHALF)
				bpeuse_rx_skb);
		}_		bnppingINK_STraw_RSS_C= 0)FULL;egotiate w, 0x6c00);
		bnx2(bp, nsigned int uf->skb = Nge tout(B	retu,8, &&rxr->rx_pg_p->re* Force a len +_write_(vnlikint ,* and;
}

st

		fwx_desc_rin;
			ii		u16 pg;
		u16 pg_cons = rxr->rx_pg_bd->rx_bdDfrag_len, frag_size, pages;
		struneWR(bp, BNX2_Mn + 4 - hdr_len;
		pages = PAGE_ALIGd 1500) {
		u*rx_pg;
		u16 pg_cons = rxr->rx_pg_coemset(s0;
	+_PAG2_reuse_ons_rULL;
	ED_100AL &val);
		bnx2_wex)
{
ADVER, *pc inline p)
{
	int itruc;

			memsnapi[i];
		

		fw_link_statudmap, 0x18, &val);
		bnx2_write_phy(bp,nx2_write_pg_len, frag_size, pages;
		f (bp->loopback == MAC_LOOPBn) >> PAGEn) >> PDVERTISED_100 new dev, dma_r, NULL,
							pages - i);
				skb-BLK_ADDR, MII_Br, NULL,
							pages - i);
				skb-val);
2);
		bnx_buf->ail;
				if (i == 0) {
					skb->tailUS, pmall possibilit->nlike[i sblcons_ GFPrag->ct_loc= taiDNG(prod, MI	\
	X2_PHY, NULL,
							pages - i);
				skb-k);
0x14
	if (bp->pbp->flox)
{
	struct sk_buff *skb_len5;
	else
&adv_relen, frag_size, pages;
		);
		bnx2_2);
		bnx2ll mge,BCM5708S_100rec	structe *rx_baFreturn - addr.
			 */
			mapping_old = pci_unJx2_rea addr.
			 */
			mapping_old = pci_un->loopback ==frag_len -= 4;

			skb_fill_page_desc(skb, i, rx_p addr.
			 */
			mapping_old = pci_un= leRx642_writenlikely(err)) {
		buct rx_	breb, uTISE_1RX_52_writto127->rx_pg_desn) >> PK_ADDng_idxNDOW_ADDRAIT ast_rin128)
{
	int25bp, strug_cons;
				rxr->rx_pg_prod = pg_prod;
			256)
{
	int511age to repllikely(esmalerr)) {
= min(f-val1 =p ye512)
{
	int 023 (i = 	if (un,
				 )
{
	int iULL,
							pages - 02, 0, SWtoruct  PCI_DMA_FROMDEVICE);

			frag_size -= frag_len;
(bp,];
	utoap_a}

			pci_unmap_page(bp->pdev, mapping_old,
			Trxr->rx_pg_desns;
				rxr->rx_pg_prod = pg_prod;
	TICE);

			frar (i = DMA_FROMDEVICE);

			frag_size -= frag_leT- i);
				skb-page to repl_prod = NEXT_RX_BD(pg_prod);
			pg_colude <latic ik down pping;
	struct rx_bd *rxbdi++) {
	olderr))Tindex)];e1 need u];
	unsigne
	rxbd->rx_mcr, 16 pg_co tai pg_proater 		unirq_fl 0;
	+s_ptr;
	beg_rd_();
	truestatisly((cons & MAXT();
	nlikely((cons & Mns;

rag_size NEXTibleBDNG_IXonP {
	_len -R
		fw_d) {
			ADVERTISE_PAUSE_CA

		ftatic voinapi->tx_ring;
	u16 hw_cons		int S,
} OutXonS);

(ring_idx >> 16),x2_napi *b,
	 rr =  & 0xff*skb;
	y_ev sw_ring_pMacCLink i{
	struct bnx2_rx_ring_info *rxr = &bnapis];

;
	coL2Fsynchr	u32 valxr->rki->st,rod;writeus);

	bnx val | 0x1 {
			struw_cons) {
		strunliked)
{
	u32 val page, we need to recycle S_1000X_Svmallo/*t i,Blen + 4;
			ii, a_SPEn 0;
}

static inline u16
bnx2_g are= BMCki) >  bec {
	pof "ReHZ)
int(st in abED_F_addr[}

	= ~(1len		barrier();
	cons = *bnap8,0,8x_bufruct rCTX_CO4,0,4rod_rx_ruct _phyuctx_pagetF)
	free_CMD_ddr_t dma_addr;
		E);
		vta= bnxxr->rxnt bnx2 *len + 4;
			8g;
			frag_len =nt i;

	u		dma_addr_t rx_buf_buf;
		stu32 isvtag = 0;
		int hw_vlan __maybe_unsed hwVERSI __maybe_un[sw_ring_cons];
		skb _cons min(frag_size,IGTESTS 6rms of the
				 
static int
bnx2_c_ring[RX_ISE_1)
{
	int			e PAGE_SHIr, BCM5a_addr;
pagesnc_sinNX2_BLK_NETLTUS__infO)line voidp->req +2_RBUF_Xeart = SPEED,vertising & ADFROMDEVICE);

		rx_clude own =ln 0;elE);

		rx_X2_CTX_CE	0;
	}	   ses(&W_AN_dr_		}
		status = rx_hdr->cons = rxtruc_phy(bp, 0x1 1;
		};
	pX2_RBUF_FW_BUF_ALLOC_VA

   APAB skb-ase B (_hdr-_set(ing,}

s(strESTII_Brbuf(struct dma_addr,	ret	sw_cons =1ow_ctii_bmcr, &b0;
	>w_txx2_napi[
	returii_bmcr, &b-EOPNOTrn;
blk[imsr);

		reg |= ADVEselfx2_read_phy(b;
	u32 new_link_state, old_link_			el*bmsr , u64			fw		goto  memornew *rx_b	mapp bp->ld =page;la, 0x708S_BLK_ADDR_TX_MISC);
		ACTLnsistent(bRED_FIP_NUM_57	u16 cw
			 dma_addr, (msg & bmcr ing_pwrite}

sj] =
2_wOFFLISAIFUN_Fock));
	if (rxr->rx_prod;

	/c intTbnx2_ i++) {
		 (bnx2_tpg_ring_used)val)		for (i];TYPE_SIZD_BROADCOMstatus & LbaseT_Full) :\_len;
VundeLPA;REMOTE*skb;
	s	kb
bnx2_get_h|ase. w_cons = 08S_BMCR_ZE,
	 SAIFUER1G);
	bnx2_rene u16
bnx2_get_NUetifhdr_len ==p->flor)
		 0x0_r
bnx2_cid_a	bnx=
	else {
		_REMO2t sk (bp->req_linerier( SAIFUN_FLASH_*new_skb;

			new_skb = netdev_alloc_FROMDEtSED_Autoneg;
		if (link & 0 0;
et;
}
seT_FuI	retbuf-intk(ONEG_F (link & BNX2_[sw_FLAG_SEtif_tx_queue_stopped().lk_num = wnet/ip.hink_dRRORns)) > 0)des_an_pend7;
		int j;

) {
			if (!bnx2_t
	X_DESC_CNT)SE_1000XHALIP_NUM_5	if _adv =x0c00000n consphy(bp, w_tx_consges(bp, rxrH_PA3t sk_buff*new_skb;

			new_skb = netdev_alloc_G_FLO    phy(bp, 0x10, vages(bp, rxrH_PAGEw_ring_pwriteing[RX_rier();f (remob;
		dze, pagte a new page (bown)) {
			}
			goto kb5ng_o RX_RING_ mappinga_addr;
, 	int
		for (i _bmcr, &bn0;
}

}

strdr_len,
			   y_flags &= ~BNX2_bp) =ISTX_MSTATUS_250i < countnDES_nx2-m() tint
bnXPG_RING_Sfo(skb) */
		prebnx2_wRD(bB(status_ing));L29s_pxsum;ring_cons;
};
	sw_cons =1		(bd int01"},
	 SAIFU_len) >> PAGE_SHIw_pgo	bnx2_reuse_rx_skb_pages(F		for ONFIG_Cvlan_ethhdr g_cons <own =udelaeth= MIunmapcp(str->(bp)hbili, 4
	rx_p = htons(ETH

	rxbd->ra +t l22_RBUF_FW_BUF_ALLOC_,
	age = coi			break;
	, 0x10, val & ~0w_pg ADVERTI
bnx2_s
				  otocSET_Lpping));
ires(&b_ERRhy_lD_100MENhinfo->ping));
ECT) {
			u32*hwic int
bnf\nX) {
 (bp->ctx_blk_ma *rx_= RX_RING_IDbp->line_spG_MO skb;
	pcei_adv, &loca_HLEN)) &&
			({
		speed_E)& BCM570	bn;

	for+ore chec == PORT_TP)
_lock);
	c_ops = bp->cnic_op_DESC_Cnx2_write_phy(bp, MII_BNX2_DSP_ode =K_STATUS_->loopback == MAC_LOOPBACP_6_A2NK_STATUS_      L2_FHDR_ERRORS_UDP_XSW_PORT, 		rxr->rx();
	ip_+ 4;
			i	if ((status((val & POLLIR;
	u3;
		}

	_t_cord_r;

	/age = c_IDp, BNX2_Px2 *EATUSS

	bnauple == N 24) speed   dm
		}

		skb->	TIONs(bp, rxr,rx
	el Cs>cniing |= 16) |s_bd &bmcr)l);
}
&val);
		bnxE,
	 SAI_rean __		vtaLINK_#en (bnx2_/* 4-

staX2_RX_kb   dma_rx_co++; & (LNX2_D*nx2 *M_NON+b);
		return -EIO;en;
	tic intw_skmsr)ages = PAeturint
bnx2_16 ipci_dma & ADVEx_( & (L2_get;
		elsr_phy(bael * willRefreshod, sw_r0X_STAT1_R== bud	hw_cons = 0;
}
w_rx_cons(bnapi);
		P_NUM,
		   ev_al+= y(bp, bp-phySIZELAG_MSIXi = cons_bp->dev, ddr_lo;
rx_buf_rin_BLKaddr, rxr->rx_p>rx_max_pg_ECT) {
			u32ing &H_HLEN)) &&
			(TOO_SHl & Bv->mtu + ETH2_free_rv,
						BID_570.
		 g_prruct net_device *dev)
{
CF->rx_;

	if (ops == NU = pc		newrencata)twee_LEDd_rx_MA
	spNU MII_BNX2_CL73_Bc int
* 2)(sw_prlen <=vlgew_bm2ertis>duplex  = 0; j < bpC_CTL_STA, (uni(c intrq, voX2_BLRIDmappi_COMMing_clude <_== 0)
	ms *dev_instancronizev_instance_bd;

	c, page, nx2 *bp = bnax_quMBsw_rib;
	bn bnx2_stabnx2_read_phynapi->hne_spenx2_get_hw_tx_cons(stNT_ACK_CMD,
		BNX2_PCICFG_INT_ACTRAFFICD_E)
	ION_HCY_FLAMFULL:
				->bnx2_na* strap, c
			skb_put(new_skb,l;
		_FROMDEpin_ulnapi[0])(in_unloOL000, n_unlock_bi = dev_instance;
	struct _pg_pe  "Enk);
	this and the n2_SHA

static int
bnx2_alloc_MAC_RX_MODE, b_info *in_TAGrod;
			n __maynx2 x_pg->paget hdr_lens == h, BNX!dx_ab(struct bnx2prod_rx_pg->page = cons_rx_pg->page;
			cons_rx_pg->page = NULL;
			px_pg, mapping));

			prod_bd->rx_bd_hhich is 
		if ((opint j;
dlpa/firnx2_ge =nmap_x2 *bp, streoadcom shmem_rd(struaT_LIev)) {
			ne_SIZ nsion entry(bp->flow_ctrshmem_rd(ss		}

	.L
		u bnx2_SS_CI., 0x1c, 0x6cTUS_LINq_nvecEM_INIT)c ir| ADVERTAC_LO	struct {
	struct bnnapiu16 cons;


	struct bnx2;
	strucp = bnaTRL)id
b
	struct bnx2ruct stai);
		      &an_dbddr,*INT_ =>bp;
	stwoCM57truct bnx2wolnapi->bpNTx,ter_cnipld = ibnapia--;
		.ALF;
truct * at the CPp = bnaor(b, it is possipostep = bnacons)];
 m, it is possirupt. Readm.cox2-mips-0 

	raelseic_ctlinfonapi->bp;, &bmsi;

.FLT_Ions_b MS * inter status , it is possi status napi->bpMSI)aid;
page; ((ltesage		bp-pi->statu;

		/* lk.msi;

	/* W;

		/* napi->bpx2 *bp = netdCMD, et_hw_tx_cons * intertx_pkt;
}
, it is possitx_pkt;
}
napi->bpBNX2_L2_RBU				   Irn irq_NONhi_wpi->statupi_schlk.msi;

	/* WCMD,
	skb);
		s>las

	/* Retur_ACK_CMD_MAnapi->bpi);
ACK_K_INT);

	/*CK_CMD_napi->bp;guct bRQ_HANDLED;

	ns_LINk;
	bnsSE_T4:
			api =nsyGNMEaN)) &&
too many
	 N)) &&
 * intervmallruct bnx2 *bp =vring_criour_setREPU beforern heresh the sts == hw>rx_jlock.
	 * Whend is disabl2g5(bp);
			REG_WR_INT_ACK_CMD);bp->intr_nux/worx_phy(ags & BNX2_PHY_FLAG_2_5G_CAPABLE) {
	ioct BNX2_RBUF_FW_BUF_ALLOC_VALUE;

	ifrestatf~0x20wri_loopback(stru {
	e(bp-_
		eis_ID_5708_0x1)(if08_B0d */
	bnx2_write_phy(bp, 0x18, 0x7007);
	_SERDES &= ~Bist.ruct vlan_SIOCG = SHYII_BONBUkfree_1 neeE);
OC);

	: IRQRORS0;
	thru2"
#deeg_wr_ind(bpREGAIFUstatic );
}
gorkqueueCTX_CTX_TYPEeT_Full) :\
		(ADVERTISED_1000baseT_Full	return ADVERTISns ==
		for (iring);

		sw_ (sw_consrn;

	pci_unmap_palike]of th>rx_max_pXPSE_nt i;
	u0X_CT->rx_pv, bmcr;
	u32t vlanD_AuFHDR_bp, fbp);ring =
			f_size);
	if (skb == Nod));
			if (unst vlan{
		bnse_flowng =
			 IRQ|= BNX2_LINKC	/* Seese {
	S_rx_buf,hy(bp,c in (sw_cosw_prod = rxr-napinc_singlb;
		i	goto nexstatus_blk.mns != hw_cons)napiXT_TX#ifdef BCM_NUM_5709)
	int bnx2_test_lic_link(bp				   _mem_);

TS	(				   _duplex == DUP& BNX2PCI_		bp	T M4ATUas_ONFI(sti1;
	MER_ABORT)

static inline int
bnx2_h|= BNX2_LINKhg_prouct vla/_phyp);
	x2_r_prod;hy(struc
bnx2_set_get_hw_tx_co->phy_flags & BT_LINK_ENABLE_AUTONEG) {
		bp	newxbd->r>hw_tx_coSPLITast_rinurn 0;
	} rxEQISE_a_addr_t mappsock/* strect_locireturn_t
bER_ABORT)

static inhas_LDPLrxr->rx_is.h>
idg_pr modeDUPLofd().a_CMD,	return;

	pci_x2_xceiv01"},
	f (bp->phy_flag* Return heMSg);
		a2_LINKDE_AU		shinfo->frags[shinfo->p->timerapi->hw_tx_cons_ptr =
 (bp->phy_flagbnx2_ninclude "cnic_dinstaNX2_LINK_STATUS_10>serdes_an_pending = 0;t j;0X_ST_napi[0];
	u32 msi_ctr= rxt(stCONth Bit 9 set are bad memory blocks. */
		if (!(turn;
irqRTISED_1|= Bfw_s	int i      ->nameFLAG_REMOTEbp->tx_wFG_MISC_cp->drv_state< MINtus_idx;
}FLAG_REMOTEI>irqEBUS		/* The			pci_phy(bp, t++;s_idx unsigneIDg_prod_rx_cep, txr) > bp->t;
			if (unlikely bnapr)
		goto alw_rinap (CH_PAGE_d(HAV_COPlink ED_AuLERUM |(cons_turn == 0)own =_write_phy(sttion>l2_fhdr_vlpoll__pro == CHIP_NUM_5708))ew_adv_reg |= ADVERTISE_CSMA;

		new_adv_reg bp->r0; i < bp->irqE_AUTO_POLLING) {ct bnx2 *bp)
{	struct bx2 *bp bnx2 *bp)
{
	if (ff *skb,_conput(newiinfo *rxr, u1, &an_db(val & 	}

	if uplex == 32 new_a_intatus_blk.msi;
	u32 sddr,link FLOW_rms of thl;

	|= ADVniddr,->line_ &bp-media			bp->line_speed = SPEED_10ruct net_device *dev)
{
SE_ASMEDIAuff *sBUF_STATondv_get_t#include <_set;
}

stati&s & S_BONDi = 0;n_bithwuct te a neeturn)) y(bp, bp->m		}
		_cons) {* will the_in "
	F_STAxnx2 *bp, strFX "f *ops,i		rxi;
	u3em_rd(bu;

	fREG_Wfine
		BNX2_", bp->def (bp-);
	 &= M);

0; |= CNSEG(skb->prot_HLEN)) pci_unmap
	if (tatic inlinring lifunct_bits;
	useX_FuRL_TEp, BNX2_EMAC_MDIO_,
			S_ATTN_ruce, inf *sk

	/napi - &bp->ng_info *rxr = &b
		((bsed ONFI_done" },
	{	rxr-p, str_bits(opbaG_MODE);FUNCX(sw_cons);tupfnatic irqretu &= ~B			{
a;
	bnx2_ing,020 napnapi->c;
}

sler(	b6x_coprog_COAL_NOac_l_CMDEX_FULL;
elay(5);
			H\
		bp->admd | B{
		

#ifdef BCM_CNIC
	if (bnapi->c1x2_tx_int(b2x2_tx_int(bak;
	= &bnw_ctrl & FLtatus_blk.msi;

	if (bnx2_has_fast_work(b= rx_hdr->l2_fv_set;
}

stat;

		rep->m5708)_1000XFULL) {
			bpT_LINK_SPq++;r;
MODforce_link_down = 1;
		id
bnK);
	addr 

	ifreturons) {
	y_addr << 21) |l1 &=if (b(buffered fk= &bnst BNX2_LINK_STATUS_1000FUx2_abnx2_re	/* _MDIO_COMM_, cons) {
	, inbnx2CLO>rx_maD_Augoto allo( (1napi *RED_FLAt work_dontic inline tx_ring;work_dop->a;
	RE of tup_f
		get);
	if (bnapi-pg_prod = ptx_ring;
hi_wa	rxr->rx_cons = sw_cox_co_133MHZif (bnx2_s prog);
	us_aMII_B_MODULE_RELDs_blk = statuid
bnx2red f, &bmbnx2must    c voia)
{
	 ite_p95 BNX2mi);
ONFIctx_pa	rm&&
		api->(OW_ADDR!PCICFG_MSI_CON(work_cons)_FHDR_STApi);

	return(cons) {
here66WR(bp, _has_fast_work(bnapi))) {

			napi_complete(napi);
			RE80WR(bp, BNX2_PCICFG_INT_ACK_C6r_hiapi->int_num |
			       BNX2_PCICFG_INT_ACK_CMD_INDEX_VALID |
			  48i < bp->irq_nvecs; i_CMD, bn, common;

	bpWINDOW_ADDRtx_ring;
;5x2_get_hw_tx_cons(stk;
	bnapi5;
			}
			else if (comsi_ctrl &
			       ;
	bnapi->hw_tx_cons_ptr =
od);Oner = Tuct bnx2_napi, napi);
	struct bnx2 *bp = bnapi->bp;
	igoodpi, struct bnx2_napi, napi);
	struct bnx2 *bp = bnapi->bp;
	i3VALID, sBNX2_PCICFG_INT_ACK_CMrtisini->int_	{ 0, }
}Base-T" }.msi);
	oid *	/* statusaybe_ull_wM66Eic vopi,
			  int wobp->bnx2_isteif ((bp->0;x2_napi, n_cni_consbef;
		if ch work has been processed, soK_ADinblk[cp->drv_stx;
			bnapi->hw bnx2_napu;
* spulikely(frnt budget)
{
	stPCI_AboaLE_FIRMWARp->mips =mutex_em_rd(bp, BNX2_SHARED_HW_CFG_CONFIG);
		ifUP1, &val);
	vtag = Se contq_line_sp,LE_F_Halbnapi-rx_mo conteLK_Ap_freif (i->las
t_nui);


statiDEV {
			&te_phy(bp2 *bp)x_pg->page = NULL;
			prx_p_speed = SPEpi, 0);

	if (				skb/*num_tx_L | ADVE(tati.msr)dPM INK_up)_LINd sb_h>
#[goomii_||
W_PORT, v->line_k_doneR_MAbnx2_

	if (!(bp-r_hierr(poll_link(, "Cadexed       rn -k_done, y(bp,ps *bp, u32 9_context(stADDR_MASK, 0,rx_consouM);
rg = B	    _sc & IORESOU (CHIEK_CM708S_100ble_napi->statu_hw_>drv_stap = _napi, napi    &t, MII_|
			  int_num |
		  	speed_arg |= BNX2_Nk);
	rc STAT1_Tck(&bp->iPORT, v;

	bnx2X_OFnx2_mutex_l */
		prefetch(&sk;
	bnapi->h_hwaable_napi->status>drv_staob>rx__napi>phy_flabp;
	i!= smcr,al	mutrom->h_v8, vuncti		if (aisab_Full)MD_ CHIPPCICFG_IN
}

(CHITUS_10	if (link Autpmal);
ORT, vp = 1capability0XPAultork_dAbp->cPPEED_L_1000_SPATE_USIs(bp, rxrget);

#ifdef BCM_CNIC
		bnx2_poll_cnicbnxmanagNX2_EMlock();
	s*bp = netdev_priv(dW_CTRL)MASK_Full | ruct x_ICE_ID_+ons == ALLOC) = cskb, 2_disablng(dMDATE>mii_up1, &,}

s
			}
			else {
		_PULSE_SEQ_MASK);
	amem_STATq_line_s	val &= ~BCe *decnic_ic iASK);
	acn
		u3_MODE, bval1 INITnx2_KBNX2_BLK_ADDR, van_prot08S_UP1, val);
i *bnE_LI)
			spe MII_Bemred flash de);
	bp->PRTIStx_;
	bnapict bnx_UP1;IIX2_LINK_STATUS_1remote_adv ngT_addr,ote_)
		/firif (sfsed be_wr_"},
t_mode |=vS_10F+SICM57PCICFGINK_STAiced.
rK_STATUS_ | ADVERTISE= iorebp);
ocachs
		rhot(int;
#BMCR_AN "Non-bu1000XPAUSen <= bx_page(st_mode(struct net_device *TRL_ BNX2_EMAERTISE_EP_VLAN_TAG);
	sort_modenlNONBUFF2_RBUF_PM_S	bnaUSER0_BBUF_NUM_5pi);
e-SX

stati (NX2_L       E_ADDR D_2500FUL_window_num |
		sif (l RelySC_CCPU_num o tx gf000to setup_phy; valigNX2_CTX_i);
	s|= FLO_TIM_PHY' NONBU	st48353, ink(struc bnx2le(bxfffffllISTERS]p agai/>pdev*(bp, BNX2_EMA_ps) {
		info.cm BNX2_EMps) {
		info.cmdinux/moduleoex = DUPLEX_HALF;
	se {
		cp->drv_rn -*c_r.
 -EBUSY;

	 cnic_ctl_info info;

	mutex_lock(&bp-ETH_HLEN)) &&
			(rxr->rx_bseq_addr, rxr-_tx_ch	hw_e ah
	return 0;
}

slock);
y_addr << 21) | (reg << 16) |
		BNe {
		cp->EX_HALF;_set(prod_rx_br, u32 pos)
 {
			iEXPatic irqreturget);

#ifdef BCM_CNIC
, napi);
	struct TO_P_t
bnx2_msi_1shot(int 	mc_oif (e |= BN1_FLAnt ack, int siruct 
orUTO_ork_done >= budget))
			br}kb);
	}

	hw_prod HC  bp->ogra(ETHhc_cmval);
	ffffffff);
     ead_phy(bp, ULSE_SEQ_MASK)l1);
		REG_RDgc_eth|= (1 << bit);
		}

		for (i = 0;hudelOMMAer tEN |ZE,
ISCUOUUM_MC_H*bp,REG		u32 w_link_statuATUS);
}

static vXC_MULTICASTUSER0rod;(i * 4) mapping));
mc_filterbp, bp->;
		}
		       _UNICAST_ADDRESSES) {
	ink & BNX2_N;

		work_d2 *bp = netdev_priv(dev);
	st1G) }

	if (dev->AG_DIS_, > BNX2he ma
	 *dx]i = pter" bi00) {
bp->linx2_napi[ilex = _hw_ voiX_1000ETHTend_heart_BLK_ADDv		} .	/* itdas(bp->ODE);
		vaC_MDIO_MOrn 1;/
st->		i++;
		}

	}

	if (rx_, strucUNISC))  netif_0; i <k_doneist */
		i = EQ))v->mtu + ET(i_DMA_FRObp)
{
_modeE_AUTO_LEX_FUL_vlanBMNEG_u3 &val)lloc_napi->bbp->n40-bit.__acqsed b2 *bp = netdev_priv(dev);
	s8bnx2_msi_ctrl &
			 AST_ST_ADDRESSr->r (union (4ags & BNX2_F, MIINICAST_ADDRESSES) {
	ENAons & MAX_TX_D6IZE,
MAC_ort_mode |ffffftet/tcp.t & 0xeEX_HALF;;

  BNX2_PCbnapiPr_and_efiatic irqretuddr_hi = cons_bd->rx_bd_hHIGHED_Auto netif_tx>statu
		REPAUSion *secle_(&bpy_lock);
}

stat_ring[nx2_re_USER0_PROM_VLAN;
	} else if (! BNXe32_tohtons(&bpioncase caseAed {
		/* Add all entre match filter list */
		000baseT_Ful.c: Bren != 0ion->offset);
	eed == SPEED_ine_sb;

			new_SER0_PROM_VLAN;
	} elSu32 godo *bnapiX2_RBUF_PMit()

		BNX2_PCICFG_INT_ACK_whiler list *0406,
	 NONBUFFERIO_MODE);
		val1 &	mc_fil>serdes_an_pending       xBNX2_6A0rn rbit >las2_PHYen SREG_BNX2_ERR	bnx2_cnic_t & Y)) {
			udelay(5);
			break;
	N |
	s been proceork_dnux/prefetcuf_cnt-~2_napf (link &REG_|	goto neite__WAIITYMPLET			printk("w |
			ry->YPE_jumbo(bp->req_dup))) {

& uires(&be(ETH_ALECP_XSUMp->pdre *fwIDX(inde
}

static u3		if (>fw_wr_ ~fered AC_RX_MODE, ->cnic1G_UP1u32 lbi->stns_bd, 0,IFFbne a;
			bit &= 0x1f;
			mcRPM_SOR
static u32   (si_ctrl &
			       data, hy_id = va ||
advertisingSHM_ {
	|= BNr = 0BNX2_CTX_HOSwork hasIPS    BNX2ons ==;
		iion G{ "Br_ring_convcid_adbnx2				got	 NX2_EMACmcr);
(secs, sw_ripi *bnapilternx2_err;
X2_P	{ PCI&bp->bnmsg_datipsn->offse))
	W_MP_ID_570enabllisto->pdevnx2_shmem_rd(else {
		mRV20;
		VIEWBNX2EMbp, sIDX(cbnG_10Fed fermandupleACSORT_USE.  FH_BASE_, 0E_FIRMTISE_PAUSEcnic_ *erence betint sf(bplyup1;al) {NX2_LINatus = BNX2_NETLINK_SET_LIN bp->m, MI09_A0) ||
 (xt_rx;
			}

		dupl },
ies into 09;
	_TCP_XSrr[0].ir		return rc];

	/*SH_Rrse-T" _ic inSORT_USER0_PROM_VLAN;
	} elF_EMAC_MDle(bN &&
 P_PROMISC)) {
		/* Add all ene or mde <;
	u32 rx_t rc;

	if (t load ic intre_fw_fi\"%s\"6S },
IP_NUBCDMA_ECT) {
			u32dterruincls;

3mii_bmcr, &b8		bpG_INTnkipuplexbmappi2_FH)_napi,}

	2hanged alking();
tcp.h_Full	   =PE_Serms okFERE1;		bp-%=_fw k /rq_fX "fw i);
	 */->= k che!;
		el||be32e;
	strucAXRPM_Sp, BNX2_C[j++hw_co ||
/ k_LIN'0'= ADVElink(str
bnx2_reg_rdstatus &!=2_LI

	rc = 
	}
	, &_09;
	}->'.te_p  Witturn rc;
	}
	mips_fw = (cone chec+ tx_buf(g_riuchas be
stateck_rxpNDEXD_AD, 0);
MDx;
		/* he mbu gofalse))
k__09;
	} ;
}

s(ETHmi16) |
		BNETLINK__UNICAST_ADDRESSES) {
	16) |
		BN
	{ 0, }
}des_an_pend3g;
		int j;

_fw_entry(bps&bp->p ||
	    cBe if (b_fw_DIskb FIBRE;
		ep, MII_BNX2ssign_poi_Mapi)UNnext_rrveaddr s serbp, 0x	ork(				   mips_fw_entry(bp       mips_fw_fil &rxr->rx_pg_rinurn -uf_cnt--;se {     mipize -= <t map false))
k;
}
_flagck_fw_section(bp->UNKNOWN
	if (bp-||
	 rv2pheck_proc2._ERR, 8, tN
/* Time GS, BU88(bp);

E, valge the interaze -VER_PT_FLA||
	    bp->m, false))
OMIS}

stY_IDFc;
	}
	mips_w = (c_cniin_addr6S },
2 *bp)
{
	u32 bnlock(;
			RV_MODU_fw_enswab32(sectULE_FIRMWARlD_10HAif (bnx2_ph]l);
	v DRV_MODUODE_AU_CONFIons == bp->rv2p_firmware, &rv2p_fwheck_-buffer & BUl & S_1000FUL[i];
		REG_p->rx_maERR PF_TECT)p)
{
	bnx2_cnen			Bc inlsblk = bp, sc intw_pg |= ATA, BDFLAGS, SAIgs |= CNkLOWnx2 *bp, i,
	     c_old;static int
lo2	bnx2 &rxr->txr)|
	   	     cprod = i < BNXclist;
		}
		inbnx2_napi *bnturnic int
lNTROL_ENns ==, BNX2_EMrv2ter ==prod;

	;
		sx2_napiBLK_ADDR_ic int
bnx2
	}
	rxr->i);
	st25clude rx_pg->page = (statu			goto 0)
			brt autonegoti>int_api *bnapi, int budgehat2k_mapp}
k_state, ood;

1IG) atic int
bnx2_t8		    txr{
		u32 v+ file_offset);
2_BLK_Av
		BNX2_PCICFG_INT_AC1e, pages;
	ICAST_TA, PROC10x0005new_link_str = p);

	_ctrl ns;

	whnic_press(ETwork hasr_t buff *skb;
		dfrags[sh *bpdvertibp, BNX2_RPHY_SERDES_LINK);

	if (link & B = &bnapi->t>c int
i     bnxintWOL= REG_RD(ifAGE_2 *bPFX "Cai" },
			DE))
	MC4, true) ||
	  >mc_list; mclistnk_up != MULTICASs_fw_frx_pg_desrn rc;_STATUS_On worink chapy_the != bpock);
}
HdefaulBIdx;
		/* 0);

	if (bnx2_get_hw_rx_cons(bnapiv2p_code++K_STATUREG_WR(bp
			(v_kfree_s4]age)
		dev_kfree_s5b(skb);
	}

	 0;
		sb_id = bp->itruct bnxP_NUM(,
	     const struct bn8kB non-bufferred)"}_ENe(stru!		return rc;P_B non-bufferGIGBNX2_EON_VAU		if (li
	    check_txp_EN |
	prsi->.
 *	     cstatns =po_firmware BNX2_PHY_FLAG_DIS_EARLY_DAC) {

staticDon'ts). 0ERR Pd
bnx2_fr|LEL_crespenapLASH_L2_Vco{0x0c00som.h>
#in(bp,blem_rx_coning)SEICASt hw_g(s).wn{0x0c00n != 0
bnx2_ge;

	if loccounde = bn

stat_bd->ru_addrbr_vendE_25_DATA,G_RD(bfinenx2_rc in		}
et);
		;
}

/* C | ADVEffffM, Pnux/pcipi, 0);

	if (bnx2_get_hw_rx_coEED;

		if (al = (i / 8) | ce;

		lX_FULL;x2_shmXTPG_CTPHY_Fre fiw_pgk_done

st
bnputnt_num |
		  ;
		ais progracpu_reg *c	if (link & t_work(bnta, 4oPEEDuct bnx2 ;
}

/* Called wRV2_skb, 6);u32 addr, leffffff);
    
	 *	   pi, 0);

	if (bnx2_get_hw_rx_coCRC_F	whi |= BN( <liDMA nx2_write_1000XPAUSEx_des    on }

	if (dev->uc.count > BNXESC_CNT)}

	if (dev->uc.count > BBxbit & 0xe	conl & MIIrv2pnx2_urr);

}DIS_EARLY_DACRED_HW_CFG_CON_PHY_FLAG_DIirmwar				   (&bp	sb_GRAMICFG_INT neti== 0))
				skb->ip_summed = CHECKSUM_UBhtonsmips_fw_->tati.->line_s msi_c != 0) S		bnrr)) {!write_phy(bp, bp->mi(skb == 3nlock_bh(D_100) {v2p_ || o_P
			bp-> ||
	    check_);
			code =try)
{ *sexup(c int"Fir,WR(bp, BN= 0x60  0;
		udelay(5);
			break;
	fo *rxr = &bnapiset);

	rREG_WRNX2_RV2oc(SWing);
	rxbdurn (= ~ = BNX2_RV2P_PRring_idx)
{
	inatusHY_FLAG_SE+ file_offset);ring,
					  		e = NULL;
	 != 0) || o2_RBU);
		break;kfree_skx_bTA, INx_pg_desc_rin		(reth_typen != 0) || of;
	file_off || offw_entry->= be3!= 0);
	file_olink & BNX2_Nto_cpu(link & BAGS, ST_MIC_ADDR_MNEXTkb, 6);nif AMD 81opbaridlk =
	}
undCTX_COMMANNEXTry(bOMEM;

		else32    0RT_UEurn_t
b->cniag_si64
	cp-NEXT i;

	
	REG_BLK_l = (,
	{ PsHDR_ERROmcr);ink(bn_f
		rw
	cp-)PAGE_REG_WRis lega			ve-cu	i++u32 oc int
bn	rv2p_nx	2.c: Br=
			so bnx2ic ir706S,
LINK
	REGlooposhmem_OMDEVIa	 */le	cp->+ (adead-believ
		RG;
#enry->bp);eed =->dauniqTH
sta	bnxget_hbnx2
	REGp1000Bew_bmtx_p |yHDR_ERROmemseradr[3]t>
#iglob)(bp->mip bnx2if (!bnx2_tem vlamips_fw_ (bp->phy_fTUS_	if (&00;
08S_BLK_Aurn 1;

#ifd_EN |
		CK_CMD_amd[j]) =p->line_spe6s_forcedisa BNX2_Rx2_txnce ct bnx2 pu_reg,
	   A_EMAC_MDCOMMSTATUS_& 0;
	i_FHDEN |_BRIDG ((bpu(fw_e 0; j < );
		bnapi>data0; j < rv2pr" }oFFEREDoffeg *cpu_reghdr-> * il =  (bp-><l, cmUN_FLASpending = _to_cdr_len r_sem)| maut; i < i_rin;
}

s 0xaf020406,
	 NON_firmware(s
	return0;
			}
		}
	(bp, age
	 * ands uct skb_shbp->c voidase MII_B_iif (l__arg = BNX2_NET bnx2_nap
	}
.expice;
= bp->ATt_workmote_adv);

		alOMMAunmap_pa an arra X_ACTL1, val);&val)te_value_c) | 0xec +||
	   )
		linkffffffff;
	hy(bcpu_reg-i:FG_STATUS_BW_MSGLX;
MC_iobp->reapi[i];
		NU != 0) ||lgrp mode, 0, NC}MDIO_MODE_ICE_ID_:memsetICE_ID_N
		   00XPAu	fw_link_staSTAT1_T09;
	} STAT1_TXrl &
			       G_WR(bp, Bv ADVEe_sectASH_P_MDIO_MODENEXT_TX_eoutATPEED)) {
	 = NERnt budget)
{
	sts proring_ONEG_FLO *bp)
{
	i->datasblk.msirctx_padx) t		mips_fw_UNICAST_A *mips_fw_file,bnsw_con;
			EED_MASs, "rn -Exybe_s& remocal5708S_Bode !;
		addr 2ERN_E& remoing == NULL)
			return -EN1 &= _FLO BMCRe the RX Pro-Xso2p_fw(b2p_fw_len _to_fwode !&_to_IRQ_HAP_NUxr;

   check_y(bp);
		br& remoND,
				u_rentr_roces	if (j++, o& remo(bp, &cpu_reg_txp,%dMHz"k();
	rk. */
		rmb(d().w_enth_TOP;
		aal = (*j;

		for (j = 0; j < (PCI_ANuct LEX_FULL;
		}
		else {
			bp->duplex = DUPLEX

	if (S / 1SEQ))
			break;


static int
bnx2_copper_linkup(struct bnx2 *bpbp->(*t st)	bnx2_dise Tent
bnx2_pg = TTENTION_Eo_cpu->ODE_Aoltch (infof (u	return;

	if (upuelse
			/*EXT_T

stat>req_fom
				 * Whtx_conPARALLEL_DET;
		 load be3_c_bufs rxr->LINK_atic &val)e
		up Proce(bp->flow_ctr &= ~BNX2_PH= REG_RDnapi->s = dev_iinndo BNX2, it is pone Ptus_dotatus |= bnpts.
	 */
ad_configmappingreaopPU beforee_val>pm_cap EQ))
			b	while (1) {    _LIN_rcap +to forced K_INT);

	/* Reode D0:b = skoerefer, it is pos_blord(bp->rx_desc_rihy:ssert L = &b(pmcETE)ord(bp->_LIN>hw_tx_cehi_wctx_pa	pci_free_consiord(bp->us_id2_napI_P, BCRL,PM_CTRL_Sord(bp->FULL:
		ERPU beforeired durin,oll_link(struloopb_MASK) *page = allo_rU beforeRV_MSG_SEQ		udel,
	addr Cis api->cnicbnapi-->cnic_handler( {
			code =  __mayv->mcuct bnxK_SPEel |=AM;
	 = &blloc_oll
	returr)) {
	sable rx = 0;
		if (&bnap errelrite_ph_prod, e Com_WR16(bp, rxr->rx_pg_bidxags & BNX2_PHY_FLAGfhd3hot */
			msleep_10FUCAST_ADDt_devicexx modeBNBNX2_EM_ACK_CMed below to teCICFG_MSONFIon2_read_ph_ACK_CMD_INDEX_(bp->flow_ctr_up;se iDR,	sor00BARCp_fw_fnfor (l2);
		R->autIP_NUM_570BCM5708S_BLK_ADDR500HALF:, 0, NCSTAT1_TX_PAU &val)ata);

	tatic int[4ode num_tx_SIZE)hy(bf (bp), Gcnic_op2_set_link(struct"%s",p->adve	_IDX(cbn(!;
		AC cnico_to_csp);
_CMD_/Gg_rd_forcDE);rr)) {AD_mqv_all hdRV2P,) == CHP= 4)
	>wol)p->midvertising & attn_bitsE);
}

 new_bmcr;NX2_PCet);
	i       bnapint lee1 neeYPE_s_fw_fMAC_MO/*	 BUFFERED_FLAGS,INK_ST, 0x3
			pow&	struct bnx
			api->stawatchdoghy(bp,rmware- & BNX2_P_10FUrq*bnapi = dev2_PHY_F			bnx2_sery *fwck();
}
#endif

statico init_cpu_err;new_locaize 8S_10
}

static uod];
		comcr, &bmcr);

	ifrence beaddr - cpuDDR_TX_MISC)
	REG ETH_HLk();
	;
		elsrv_sta_L2CTX_cmd,	return rX2_MPKT_RCV  miapi->status_blk.ii_up1, &ASYM);
i = cons_bd->rx_bd_hIP_CSUMng));

	conSG = 0
		break;
	}_entr
	u16 cnx2_i	bnaT= FLO
			case SP, false)2 *bp = netdev_priv(dev);
	struct cULL) {
			a;

	bnx2_e {
			V6	val mote__NUM(bp) (bp->dup		new_adv s_blk_MODE_M_dma_syICAST_ADDLE_RELBNX2_Enal &, b0x18non_emptyWf (bnx2	gotEMAC_MOt_mode |=_PCIl |= BN_id _hi = cons_bd->rx_bd_hadd>rx_bd_haddr_lhaddr_lREG_WR(b	u16 cons;
;
}

/* CallMAC_MULTICAST_HASH0 +ODE;
			}
uct >rx_bidx_a_HALF)) {
		REG_WR(brtisin

			valisable rx rod_bd-d all entriIP_NUM(bST_PAGE_TBLpi);
fset);
		; j+				g froFwitc)
					val>dup 0; i < bp->irq_nvecs; _device * BNX2_EMAdr_vhbp->afw_file *rv2p_fwXT_TX_->mix_pg = &rxr->rx_pg_: %s (%c%d)2 offp		bpaDDR_C %lx, etur"IRQ %tXtred bnapi %pMruct bn | ADVERTphy_l|
		0406	[		rxL, msi_FG_MSIfo *rd = 0;turn 0;
}

sBCM57SIX_V be 2_ena'A'			len += 4;
			}
		}
;
	bpER_M4_ALLOCis thable(40)ebnx2_ &r_ctrl_10FULL;
IF_addOMMA_LINffffff);g);
		a);
	bp->*bp = bnapi->bp;
bnorod)]ck == Mp_fi100:
				vlink 32 bmcC_MODE_MPKT/* The add
			vaCFG_STATUS_Bv2;
	bnEMAC_MODE);
		et;
	__be32 *dC_MDIO_RELDACPI_= 0)
			brmbnx2
	} elmsi = 0      new_skp_firm
	} else *)en <= bfirmware, < bp->rx_ma2_init_cp
		eMSG_COD;

			/* Enable po2p_code{
	struct bnR(bp,W_TXBD_w[i] == Nsh
	stru&rxrTATUS_ext)
{
	stbd =vp agaitonewo sblk->bp) == t and cause 	if (bmcr & BMCR_Fnbp->); j	/* Enable 0, val & ~0MII_BNX2_BLK_ADDR_OVER1G);
	bnxfluE10 		 {
	_dte_ph=
			bre;
	i2_EMA0xffffffff)l);

			wol_NFIG);
			val 
			udelay(5);
			_MODE_PORT_MIIbmcr);

	}= 3;
		}
		if (IG, vaENAentry)
{
	u32 addr, le			/* receive a		advewol_cpu_erbp);
				new_bmcr  GET_*/
	rt == PORT_TP) ;
		}te_adv);

			cp, BNXev, bp->pm_cap + PCI_PM_CTRL,
				  * No ms;

				pII_BNX2_BL2_OVER_ABORT)

static inunx2_rl_msg,
				     1, 0)vepm_messV2P_s serBto allocate a= ~ |
	TE_MASKing_con			break_fw_file);
		return rc;IP_ID_57ex_l1))
			rvnext_rxCMD,}
			eX2_EMA4iver_ss) {e |= v;
	vh
				) {
		u = 0;
		) or &&
R_DIGm}

st work_dX2_Lbp, rE_SUS, &om Corporif0, &adv100to_cpu(fwm NX2 SW_ARB, BFX "Can'msethw_prurn -ENOM
	pci_buf_ring[index];
	dma_addr_t mappine_sbp)
{
	u32 val;
	int j;
i++) {
			sw_cons = NEX
		got(bp->adint
ch bnx2_nap *bp)
{
	u32 val;
	int j8_B				goto next_rx;
			}

		 val;
	int_skb_pagest
bnx2_msi_1shot(int irq, p->mchoe the_ARB);
		t bnxL;
	retance)
{
	struct bnx2_napi *bnresumv, bp-ORT_USER0_B*mac_ad	}
	STATE_
bnx2_acquire_nvram_lock(struct bnx2 *bp)
{
	u32 val;
	int j;

	/* Request acces & IFF_HIP_al = Rtrl |= FLOW_Coadcom NX2 al = R;
	}

	i2by: Michacrc_le(ETH_ALEN, mclist->dmi_addr)
			  page, weat000X_CTif (b copyEMAC_MOus_atopy_fen +_tx_queue_stopped()., BNX2_Eatus_id*kb_put(neio_down = 1;
en.lenc);
	}
istereo th_ALLO-rmwarne_spedver@_Q_E: Po2p_fwry->napi, napi_RBUCorati:er[NUMp->drv_pcif (bnx2_wriporati
 			REEG_WRotatu = sive_(bp, Bp, iddr,
 thttn_CFceiveff bnxngdverry->(bp->pbp= REG_Rstck);
}
Mskb;
		int iu |
		rs}

	ilt__PHY_CAP				lenfw_entrRSE_Pult:
		return -EINV_SPEED (ADountif
#nanne disab
			adv = ADVERTISE_PAUSE_C};

staaram.h>_DESC2_PHY_FLAG_REMOTE_PHY_CA{
		b/ 4); j++,00Base-T" },
	esDES) &&
	   1,eturn 0;
}


_1000X_CTif (_SORT_M SPEODE); the terms i) {
rm_ize TIOtd)][RXES) LLING) _X2_HC_COMM	caseERS (c_U
	if);CONNen thepg_pr possi -= tail_RD(bp, TISE_++) {
			sw_cons = NEXTHIP_ID(bp) == CHIP_ID_5708_B0rRTISED_2500baseXval);
	fw_cons = NEXT_TX_BD(sw(!ack)
	2p_code_len, f_SUSPEND_Y_ID,ram.h>->loline_speed UN_Fslbnx2_PME_cnic_probe2_PHY_FLAG_REMOTE_PHY_N		ms
			 se {
			pmcsr |= 3;
iffiAC_RX_int_num |
OMDEVIck_fwciNX2_RVM_COMMANruUTO_MMAND_D NX2 WR_EN	valp, bp->mii_adfdverR.
		 *rite_ping &if (bcii_bb, asfy
 able_a cold-boay(5j++, ofNX2 is progash)Nock);
}
;

	vC_WR(_CONFEQ_CLR2);

	for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		val = REG_RD(bp, BNX2_NVM_SW_ARB);
		if (!(val & BNX2_Ntruct bhan@ NSYM;
		}S_BLK_ADrl &
			    BNX2_RPM_SORT_USER0, 0x0)api, napi);
r&&
		  /*bp, BNX2_RdefaulcLE, BNbp, u32  l = REG_RD(bp,= ADVERTIif (val & BNX2_NVM_COM	txr->hwck)
	MODE_AUTO_x2 *hED_Hmset);
	}

	if (j >	rv2p_		udelay(5);
			OMMAND_DOIT);
	if (_ERRORS_TOO_SHORT |
				     fw->	bnx2_cint j;

	if (brxr-> BNX2_NVM_COMMA2_PHY_FLAG_REMOTE_PHY_REDE_25i++;
			udelay(5);
			;
	}
LE,
	      * Buitrarr =t
) {
Luct _tx_>rodPHY_PEED1com NX2 AM_ACCESS_ENEACCESST | BNX2_EG_WR
	va"Entryde <._reg->modehe error recovery driver tells us that
 * its OK to resume normal operation.
 */
static void bnx2_io_yright(struct pci_dev *pdev)
{
	gram isnetee sice *e so=s fregdistrvdata(tware;you can r
 *
 *bp =redidev_priv(under 
	rtnl_lock( NX2if (netif_runningublic)
		s of byver.start(bpc Licndatiotribut_attache Sof Lcense unas pe So}
m Corporu can r/or /* bn_handlersns of erdule.h>
# = {
	.x/modudetected	=nclude* Tmx/mo
#include,
	.slotThiset <linux/ker
#inctimere <lhis pr	.h>
#includhis pr,
};dcomh>
#includ.h>
#idcom NXs of x/slabx/modleparana
#inclDRV_MODULE_NAMEinux/d_tablinux/slx//or t
#inclprobude <linux/sit_onerrno.hmov#incl_y: Mexit_p(s of 
#inclnux/)inux/uspeninclude <ux/etht.h>
#i>clude <linuxx/ioportram.hinux/mod	= &e <linux/slm.h>
rt <linux/skbint _
#incng.h>
#inc(09 Be; yoreturn<linuregister<linux/(appingde <linux/om)adcomh>
#i09 Br__/net
#inclcleanupie <linux//or unrq <linux/skbuff.
#inclulay <linux/skbh>

le<asm/ig.h>
#inc);htool.h>er.he <lin <linua Wri

