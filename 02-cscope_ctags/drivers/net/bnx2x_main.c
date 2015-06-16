/* bnx2x_main.c: Broadcom Everest network driver.
 *
 * Copyright (c) 2007-2009 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Maintained by: Eilon Greenstein <eilong@broadcom.com>
 * Written by: Eliezer Tamir
 * Based on code from Michael Chan's bnx2 driver
 * UDP CSUM errata workaround by Arik Gendelman
 * Slowpath and fastpath rework by Vladislav Zolotarov
 * Statistics and Link management by Yitchak Gertner
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/device.h>  /* for dev_info() */
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
#include <linux/irq.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <linux/time.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include <linux/workqueue.h>
#include <linux/crc32.h>
#include <linux/crc32c.h>
#include <linux/prefetch.h>
#include <linux/zlib.h>
#include <linux/io.h>


#include "bnx2x.h"
#include "bnx2x_init.h"
#include "bnx2x_init_ops.h"
#include "bnx2x_dump.h"

#define DRV_MODULE_VERSION	"1.52.1"
#define DRV_MODULE_RELDATE	"2009/08/12"
#define BNX2X_BC_VER		0x040200

#include <linux/firmware.h>
#include "bnx2x_fw_file_hdr.h"
/* FW files */
#define FW_FILE_PREFIX_E1	"bnx2x-e1-"
#define FW_FILE_PREFIX_E1H	"bnx2x-e1h-"

/* Time in jiffies before concluding the transmitter is hung */
#define TX_TIMEOUT		(5*HZ)

static char version[] __devinitdata =
	"Broadcom NetXtreme II 5771x 10Gigabit Ethernet Driver "
	DRV_MODULE_NAME " " DRV_MODULE_VERSION " (" DRV_MODULE_RELDATE ")\n";

MODULE_AUTHOR("Eliezer Tamir");
MODULE_DESCRIPTION("Broadcom NetXtreme II BCM57710/57711/57711E Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MODULE_VERSION);

static int multi_mode = 1;
module_param(multi_mode, int, 0);
MODULE_PARM_DESC(multi_mode, " Multi queue mode "
			     "(0 Disable; 1 Enable (default))");

static int num_rx_queues;
module_param(num_rx_queues, int, 0);
MODULE_PARM_DESC(num_rx_queues, " Number of Rx queues for multi_mode=1"
				" (default is half number of CPUs)");

static int num_tx_queues;
module_param(num_tx_queues, int, 0);
MODULE_PARM_DESC(num_tx_queues, " Number of Tx queues for multi_mode=1"
				" (default is half number of CPUs)");

static int disable_tpa;
module_param(disable_tpa, int, 0);
MODULE_PARM_DESC(disable_tpa, " Disable the TPA (LRO) feature");

static int int_mode;
module_param(int_mode, int, 0);
MODULE_PARM_DESC(int_mode, " Force interrupt mode (1 INT#x; 2 MSI)");

static int dropless_fc;
module_param(dropless_fc, int, 0);
MODULE_PARM_DESC(dropless_fc, " Pause on exhausted host ring");

static int poll;
module_param(poll, int, 0);
MODULE_PARM_DESC(poll, " Use polling (for debug)");

static int mrrs = -1;
module_param(mrrs, int, 0);
MODULE_PARM_DESC(mrrs, " Force Max Read Req Size (0..3) (for debug)");

static int debug;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, " Default debug msglevel");

static int load_count[3]; /* 0-common, 1-port0, 2-port1 */

static struct workqueue_struct *bnx2x_wq;

enum bnx2x_board_type {
	BCM57710 = 0,
	BCM57711 = 1,
	BCM57711E = 2,
};

/* indexed by board_type, above */
static struct {
	char *name;
} board_info[] __devinitdata = {
	{ "Broadcom NetXtreme II BCM57710 XGb" },
	{ "Broadcom NetXtreme II BCM57711 XGb" },
	{ "Broadcom NetXtreme II BCM57711E XGb" }
};


static const struct pci_device_id bnx2x_pci_tbl[] = {
	{ PCI_VDEVICE(BROADCOM, PCI_DEVICE_ID_NX2_57710), BCM57710 },
	{ PCI_VDEVICE(BROADCOM, PCI_DEVICE_ID_NX2_57711), BCM57711 },
	{ PCI_VDEVICE(BROADCOM, PCI_DEVICE_ID_NX2_57711E), BCM57711E },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, bnx2x_pci_tbl);

/****************************************************************************
* General service functions
****************************************************************************/

/* used only at init
 * locking is done by mcp
 */
void bnx2x_reg_wr_ind(struct bnx2x *bp, u32 addr, u32 val)
{
	pci_write_config_dword(bp->pdev, PCICFG_GRC_ADDRESS, addr);
	pci_write_config_dword(bp->pdev, PCICFG_GRC_DATA, val);
	pci_write_config_dword(bp->pdev, PCICFG_GRC_ADDRESS,
			       PCICFG_VENDOR_ID_OFFSET);
}

static u32 bnx2x_reg_rd_ind(struct bnx2x *bp, u32 addr)
{
	u32 val;

	pci_write_config_dword(bp->pdev, PCICFG_GRC_ADDRESS, addr);
	pci_read_config_dword(bp->pdev, PCICFG_GRC_DATA, &val);
	pci_write_config_dword(bp->pdev, PCICFG_GRC_ADDRESS,
			       PCICFG_VENDOR_ID_OFFSET);

	return val;
}

static const u32 dmae_reg_go_c[] = {
	DMAE_REG_GO_C0, DMAE_REG_GO_C1, DMAE_REG_GO_C2, DMAE_REG_GO_C3,
	DMAE_REG_GO_C4, DMAE_REG_GO_C5, DMAE_REG_GO_C6, DMAE_REG_GO_C7,
	DMAE_REG_GO_C8, DMAE_REG_GO_C9, DMAE_REG_GO_C10, DMAE_REG_GO_C11,
	DMAE_REG_GO_C12, DMAE_REG_GO_C13, DMAE_REG_GO_C14, DMAE_REG_GO_C15
};

/* copy command into DMAE command memory and set DMAE command go */
static void bnx2x_post_dmae(struct bnx2x *bp, struct dmae_command *dmae,
			    int idx)
{
	u32 cmd_offset;
	int i;

	cmd_offset = (DMAE_REG_CMD_MEM + sizeof(struct dmae_command) * idx);
	for (i = 0; i < (sizeof(struct dmae_command)/4); i++) {
		REG_WR(bp, cmd_offset + i*4, *(((u32 *)dmae) + i));

		DP(BNX2X_MSG_OFF, "DMAE cmd[%d].%d (0x%08x) : 0x%08x\n",
		   idx, i, cmd_offset + i*4, *(((u32 *)dmae) + i));
	}
	REG_WR(bp, dmae_reg_go_c[idx], 1);
}

void bnx2x_write_dmae(struct bnx2x *bp, dma_addr_t dma_addr, u32 dst_addr,
		      u32 len32)
{
	struct dmae_command dmae;
	u32 *wb_comp = bnx2x_sp(bp, wb_comp);
	int cnt = 200;

	if (!bp->dmae_ready) {
		u32 *data = bnx2x_sp(bp, wb_data[0]);

		DP(BNX2X_MSG_OFF, "DMAE is not ready (dst_addr %08x  len32 %d)"
		   "  using indirect\n", dst_addr, len32);
		bnx2x_init_ind_wr(bp, dst_addr, data, len32);
		return;
	}

	memset(&dmae, 0, sizeof(struct dmae_command));

	dmae.opcode = (DMAE_CMD_SRC_PCI | DMAE_CMD_DST_GRC |
		       DMAE_CMD_C_DST_PCI | DMAE_CMD_C_ENABLE |
		       DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
		       DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
		       DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
		       (BP_PORT(bp) ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
		       (BP_E1HVN(bp) << DMAE_CMD_E1HVN_SHIFT));
	dmae.src_addr_lo = U64_LO(dma_addr);
	dmae.src_addr_hi = U64_HI(dma_addr);
	dmae.dst_addr_lo = dst_addr >> 2;
	dmae.dst_addr_hi = 0;
	dmae.len = len32;
	dmae.comp_addr_lo = U64_LO(bnx2x_sp_mapping(bp, wb_comp));
	dmae.comp_addr_hi = U64_HI(bnx2x_sp_mapping(bp, wb_comp));
	dmae.comp_val = DMAE_COMP_VAL;

	DP(BNX2X_MSG_OFF, "DMAE: opcode 0x%08x\n"
	   DP_LEVEL "src_addr  [%x:%08x]  len [%d *4]  "
		    "dst_addr [%x:%08x (%08x)]\n"
	   DP_LEVEL "comp_addr [%x:%08x]  comp_val 0x%08x\n",
	   dmae.opcode, dmae.src_addr_hi, dmae.src_addr_lo,
	   dmae.len, dmae.dst_addr_hi, dmae.dst_addr_lo, dst_addr,
	   dmae.comp_addr_hi, dmae.comp_addr_lo, dmae.comp_val);
	DP(BNX2X_MSG_OFF, "data [0x%08x 0x%08x 0x%08x 0x%08x]\n",
	   bp->slowpath->wb_data[0], bp->slowpath->wb_data[1],
	   bp->slowpath->wb_data[2], bp->slowpath->wb_data[3]);

	mutex_lock(&bp->dmae_mutex);

	*wb_comp = 0;

	bnx2x_post_dmae(bp, &dmae, INIT_DMAE_C(bp));

	udelay(5);

	while (*wb_comp != DMAE_COMP_VAL) {
		DP(BNX2X_MSG_OFF, "wb_comp 0x%08x\n", *wb_comp);

		if (!cnt) {
			BNX2X_ERR("DMAE timeout!\n");
			break;
		}
		cnt--;
		/* adjust delay for emulation/FPGA */
		if (CHIP_REV_IS_SLOW(bp))
			msleep(100);
		else
			udelay(5);
	}

	mutex_unlock(&bp->dmae_mutex);
}

void bnx2x_read_dmae(struct bnx2x *bp, u32 src_addr, u32 len32)
{
	struct dmae_command dmae;
	u32 *wb_comp = bnx2x_sp(bp, wb_comp);
	int cnt = 200;

	if (!bp->dmae_ready) {
		u32 *data = bnx2x_sp(bp, wb_data[0]);
		int i;

		DP(BNX2X_MSG_OFF, "DMAE is not ready (src_addr %08x  len32 %d)"
		   "  using indirect\n", src_addr, len32);
		for (i = 0; i < len32; i++)
			data[i] = bnx2x_reg_rd_ind(bp, src_addr + i*4);
		return;
	}

	memset(&dmae, 0, sizeof(struct dmae_command));

	dmae.opcode = (DMAE_CMD_SRC_GRC | DMAE_CMD_DST_PCI |
		       DMAE_CMD_C_DST_PCI | DMAE_CMD_C_ENABLE |
		       DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
		       DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
		       DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
		       (BP_PORT(bp) ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
		       (BP_E1HVN(bp) << DMAE_CMD_E1HVN_SHIFT));
	dmae.src_addr_lo = src_addr >> 2;
	dmae.src_addr_hi = 0;
	dmae.dst_addr_lo = U64_LO(bnx2x_sp_mapping(bp, wb_data));
	dmae.dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, wb_data));
	dmae.len = len32;
	dmae.comp_addr_lo = U64_LO(bnx2x_sp_mapping(bp, wb_comp));
	dmae.comp_addr_hi = U64_HI(bnx2x_sp_mapping(bp, wb_comp));
	dmae.comp_val = DMAE_COMP_VAL;

	DP(BNX2X_MSG_OFF, "DMAE: opcode 0x%08x\n"
	   DP_LEVEL "src_addr  [%x:%08x]  len [%d *4]  "
		    "dst_addr [%x:%08x (%08x)]\n"
	   DP_LEVEL "comp_addr [%x:%08x]  comp_val 0x%08x\n",
	   dmae.opcode, dmae.src_addr_hi, dmae.src_addr_lo,
	   dmae.len, dmae.dst_addr_hi, dmae.dst_addr_lo, src_addr,
	   dmae.comp_addr_hi, dmae.comp_addr_lo, dmae.comp_val);

	mutex_lock(&bp->dmae_mutex);

	memset(bnx2x_sp(bp, wb_data[0]), 0, sizeof(u32) * 4);
	*wb_comp = 0;

	bnx2x_post_dmae(bp, &dmae, INIT_DMAE_C(bp));

	udelay(5);

	while (*wb_comp != DMAE_COMP_VAL) {

		if (!cnt) {
			BNX2X_ERR("DMAE timeout!\n");
			break;
		}
		cnt--;
		/* adjust delay for emulation/FPGA */
		if (CHIP_REV_IS_SLOW(bp))
			msleep(100);
		else
			udelay(5);
	}
	DP(BNX2X_MSG_OFF, "data [0x%08x 0x%08x 0x%08x 0x%08x]\n",
	   bp->slowpath->wb_data[0], bp->slowpath->wb_data[1],
	   bp->slowpath->wb_data[2], bp->slowpath->wb_data[3]);

	mutex_unlock(&bp->dmae_mutex);
}

void bnx2x_write_dmae_phys_len(struct bnx2x *bp, dma_addr_t phys_addr,
			       u32 addr, u32 len)
{
	int offset = 0;

	while (len > DMAE_LEN32_WR_MAX) {
		bnx2x_write_dmae(bp, phys_addr + offset,
				 addr + offset, DMAE_LEN32_WR_MAX);
		offset += DMAE_LEN32_WR_MAX * 4;
		len -= DMAE_LEN32_WR_MAX;
	}

	bnx2x_write_dmae(bp, phys_addr + offset, addr + offset, len);
}

/* used only for slowpath so not inlined */
static void bnx2x_wb_wr(struct bnx2x *bp, int reg, u32 val_hi, u32 val_lo)
{
	u32 wb_write[2];

	wb_write[0] = val_hi;
	wb_write[1] = val_lo;
	REG_WR_DMAE(bp, reg, wb_write, 2);
}

#ifdef USE_WB_RD
static u64 bnx2x_wb_rd(struct bnx2x *bp, int reg)
{
	u32 wb_data[2];

	REG_RD_DMAE(bp, reg, wb_data, 2);

	return HILO_U64(wb_data[0], wb_data[1]);
}
#endif

static int bnx2x_mc_assert(struct bnx2x *bp)
{
	char last_idx;
	int i, rc = 0;
	u32 row0, row1, row2, row3;

	/* XSTORM */
	last_idx = REG_RD8(bp, BAR_XSTRORM_INTMEM +
			   XSTORM_ASSERT_LIST_INDEX_OFFSET);
	if (last_idx)
		BNX2X_ERR("XSTORM_ASSERT_LIST_INDEX 0x%x\n", last_idx);

	/* print the asserts */
	for (i = 0; i < STROM_ASSERT_ARRAY_SIZE; i++) {

		row0 = REG_RD(bp, BAR_XSTRORM_INTMEM +
			      XSTORM_ASSERT_LIST_OFFSET(i));
		row1 = REG_RD(bp, BAR_XSTRORM_INTMEM +
			      XSTORM_ASSERT_LIST_OFFSET(i) + 4);
		row2 = REG_RD(bp, BAR_XSTRORM_INTMEM +
			      XSTORM_ASSERT_LIST_OFFSET(i) + 8);
		row3 = REG_RD(bp, BAR_XSTRORM_INTMEM +
			      XSTORM_ASSERT_LIST_OFFSET(i) + 12);

		if (row0 != COMMON_ASM_INVALID_ASSERT_OPCODE) {
			BNX2X_ERR("XSTORM_ASSERT_INDEX 0x%x = 0x%08x"
				  " 0x%08x 0x%08x 0x%08x\n",
				  i, row3, row2, row1, row0);
			rc++;
		} else {
			break;
		}
	}

	/* TSTORM */
	last_idx = REG_RD8(bp, BAR_TSTRORM_INTMEM +
			   TSTORM_ASSERT_LIST_INDEX_OFFSET);
	if (last_idx)
		BNX2X_ERR("TSTORM_ASSERT_LIST_INDEX 0x%x\n", last_idx);

	/* print the asserts */
	for (i = 0; i < STROM_ASSERT_ARRAY_SIZE; i++) {

		row0 = REG_RD(bp, BAR_TSTRORM_INTMEM +
			      TSTORM_ASSERT_LIST_OFFSET(i));
		row1 = REG_RD(bp, BAR_TSTRORM_INTMEM +
			      TSTORM_ASSERT_LIST_OFFSET(i) + 4);
		row2 = REG_RD(bp, BAR_TSTRORM_INTMEM +
			      TSTORM_ASSERT_LIST_OFFSET(i) + 8);
		row3 = REG_RD(bp, BAR_TSTRORM_INTMEM +
			      TSTORM_ASSERT_LIST_OFFSET(i) + 12);

		if (row0 != COMMON_ASM_INVALID_ASSERT_OPCODE) {
			BNX2X_ERR("TSTORM_ASSERT_INDEX 0x%x = 0x%08x"
				  " 0x%08x 0x%08x 0x%08x\n",
				  i, row3, row2, row1, row0);
			rc++;
		} else {
			break;
		}
	}

	/* CSTORM */
	last_idx = REG_RD8(bp, BAR_CSTRORM_INTMEM +
			   CSTORM_ASSERT_LIST_INDEX_OFFSET);
	if (last_idx)
		BNX2X_ERR("CSTORM_ASSERT_LIST_INDEX 0x%x\n", last_idx);

	/* print the asserts */
	for (i = 0; i < STROM_ASSERT_ARRAY_SIZE; i++) {

		row0 = REG_RD(bp, BAR_CSTRORM_INTMEM +
			      CSTORM_ASSERT_LIST_OFFSET(i));
		row1 = REG_RD(bp, BAR_CSTRORM_INTMEM +
			      CSTORM_ASSERT_LIST_OFFSET(i) + 4);
		row2 = REG_RD(bp, BAR_CSTRORM_INTMEM +
			      CSTORM_ASSERT_LIST_OFFSET(i) + 8);
		row3 = REG_RD(bp, BAR_CSTRORM_INTMEM +
			      CSTORM_ASSERT_LIST_OFFSET(i) + 12);

		if (row0 != COMMON_ASM_INVALID_ASSERT_OPCODE) {
			BNX2X_ERR("CSTORM_ASSERT_INDEX 0x%x = 0x%08x"
				  " 0x%08x 0x%08x 0x%08x\n",
				  i, row3, row2, row1, row0);
			rc++;
		} else {
			break;
		}
	}

	/* USTORM */
	last_idx = REG_RD8(bp, BAR_USTRORM_INTMEM +
			   USTORM_ASSERT_LIST_INDEX_OFFSET);
	if (last_idx)
		BNX2X_ERR("USTORM_ASSERT_LIST_INDEX 0x%x\n", last_idx);

	/* print the asserts */
	for (i = 0; i < STROM_ASSERT_ARRAY_SIZE; i++) {

		row0 = REG_RD(bp, BAR_USTRORM_INTMEM +
			      USTORM_ASSERT_LIST_OFFSET(i));
		row1 = REG_RD(bp, BAR_USTRORM_INTMEM +
			      USTORM_ASSERT_LIST_OFFSET(i) + 4);
		row2 = REG_RD(bp, BAR_USTRORM_INTMEM +
			      USTORM_ASSERT_LIST_OFFSET(i) + 8);
		row3 = REG_RD(bp, BAR_USTRORM_INTMEM +
			      USTORM_ASSERT_LIST_OFFSET(i) + 12);

		if (row0 != COMMON_ASM_INVALID_ASSERT_OPCODE) {
			BNX2X_ERR("USTORM_ASSERT_INDEX 0x%x = 0x%08x"
				  " 0x%08x 0x%08x 0x%08x\n",
				  i, row3, row2, row1, row0);
			rc++;
		} else {
			break;
		}
	}

	return rc;
}

static void bnx2x_fw_dump(struct bnx2x *bp)
{
	u32 mark, offset;
	__be32 data[9];
	int word;

	mark = REG_RD(bp, MCP_REG_MCPR_SCRATCH + 0xf104);
	mark = ((mark + 0x3) & ~0x3);
	printk(KERN_ERR PFX "begin fw dump (mark 0x%x)\n", mark);

	printk(KERN_ERR PFX);
	for (offset = mark - 0x08000000; offset <= 0xF900; offset += 0x8*4) {
		for (word = 0; word < 8; word++)
			data[word] = htonl(REG_RD(bp, MCP_REG_MCPR_SCRATCH +
						  offset + 4*word));
		data[8] = 0x0;
		printk(KERN_CONT "%s", (char *)data);
	}
	for (offset = 0xF108; offset <= mark - 0x08000000; offset += 0x8*4) {
		for (word = 0; word < 8; word++)
			data[word] = htonl(REG_RD(bp, MCP_REG_MCPR_SCRATCH +
						  offset + 4*word));
		data[8] = 0x0;
		printk(KERN_CONT "%s", (char *)data);
	}
	printk(KERN_ERR PFX "end of fw dump\n");
}

static void bnx2x_panic_dump(struct bnx2x *bp)
{
	int i;
	u16 j, start, end;

	bp->stats_state = STATS_STATE_DISABLED;
	DP(BNX2X_MSG_STATS, "stats_state - DISABLED\n");

	BNX2X_ERR("begin crash dump -----------------\n");

	/* Indices */
	/* Common */
	BNX2X_ERR("def_c_idx(%u)  def_u_idx(%u)  def_x_idx(%u)"
		  "  def_t_idx(%u)  def_att_idx(%u)  attn_state(%u)"
		  "  spq_prod_idx(%u)\n",
		  bp->def_c_idx, bp->def_u_idx, bp->def_x_idx, bp->def_t_idx,
		  bp->def_att_idx, bp->attn_state, bp->spq_prod_idx);

	/* Rx */
	for_each_rx_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		BNX2X_ERR("fp%d: rx_bd_prod(%x)  rx_bd_cons(%x)"
			  "  *rx_bd_cons_sb(%x)  rx_comp_prod(%x)"
			  "  rx_comp_cons(%x)  *rx_cons_sb(%x)\n",
			  i, fp->rx_bd_prod, fp->rx_bd_cons,
			  le16_to_cpu(*fp->rx_bd_cons_sb), fp->rx_comp_prod,
			  fp->rx_comp_cons, le16_to_cpu(*fp->rx_cons_sb));
		BNX2X_ERR("      rx_sge_prod(%x)  last_max_sge(%x)"
			  "  fp_u_idx(%x) *sb_u_idx(%x)\n",
			  fp->rx_sge_prod, fp->last_max_sge,
			  le16_to_cpu(fp->fp_u_idx),
			  fp->status_blk->u_status_block.status_block_index);
	}

	/* Tx */
	for_each_tx_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		BNX2X_ERR("fp%d: tx_pkt_prod(%x)  tx_pkt_cons(%x)"
			  "  tx_bd_prod(%x)  tx_bd_cons(%x)  *tx_cons_sb(%x)\n",
			  i, fp->tx_pkt_prod, fp->tx_pkt_cons, fp->tx_bd_prod,
			  fp->tx_bd_cons, le16_to_cpu(*fp->tx_cons_sb));
		BNX2X_ERR("      fp_c_idx(%x)  *sb_c_idx(%x)"
			  "  tx_db_prod(%x)\n", le16_to_cpu(fp->fp_c_idx),
			  fp->status_blk->c_status_block.status_block_index,
			  fp->tx_db.data.prod);
	}

	/* Rings */
	/* Rx */
	for_each_rx_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		start = RX_BD(le16_to_cpu(*fp->rx_cons_sb) - 10);
		end = RX_BD(le16_to_cpu(*fp->rx_cons_sb) + 503);
		for (j = start; j != end; j = RX_BD(j + 1)) {
			u32 *rx_bd = (u32 *)&fp->rx_desc_ring[j];
			struct sw_rx_bd *sw_bd = &fp->rx_buf_ring[j];

			BNX2X_ERR("fp%d: rx_bd[%x]=[%x:%x]  sw_bd=[%p]\n",
				  i, j, rx_bd[1], rx_bd[0], sw_bd->skb);
		}

		start = RX_SGE(fp->rx_sge_prod);
		end = RX_SGE(fp->last_max_sge);
		for (j = start; j != end; j = RX_SGE(j + 1)) {
			u32 *rx_sge = (u32 *)&fp->rx_sge_ring[j];
			struct sw_rx_page *sw_page = &fp->rx_page_ring[j];

			BNX2X_ERR("fp%d: rx_sge[%x]=[%x:%x]  sw_page=[%p]\n",
				  i, j, rx_sge[1], rx_sge[0], sw_page->page);
		}

		start = RCQ_BD(fp->rx_comp_cons - 10);
		end = RCQ_BD(fp->rx_comp_cons + 503);
		for (j = start; j != end; j = RCQ_BD(j + 1)) {
			u32 *cqe = (u32 *)&fp->rx_comp_ring[j];

			BNX2X_ERR("fp%d: cqe[%x]=[%x:%x:%x:%x]\n",
				  i, j, cqe[0], cqe[1], cqe[2], cqe[3]);
		}
	}

	/* Tx */
	for_each_tx_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		start = TX_BD(le16_to_cpu(*fp->tx_cons_sb) - 10);
		end = TX_BD(le16_to_cpu(*fp->tx_cons_sb) + 245);
		for (j = start; j != end; j = TX_BD(j + 1)) {
			struct sw_tx_bd *sw_bd = &fp->tx_buf_ring[j];

			BNX2X_ERR("fp%d: packet[%x]=[%p,%x]\n",
				  i, j, sw_bd->skb, sw_bd->first_bd);
		}

		start = TX_BD(fp->tx_bd_cons - 10);
		end = TX_BD(fp->tx_bd_cons + 254);
		for (j = start; j != end; j = TX_BD(j + 1)) {
			u32 *tx_bd = (u32 *)&fp->tx_desc_ring[j];

			BNX2X_ERR("fp%d: tx_bd[%x]=[%x:%x:%x:%x]\n",
				  i, j, tx_bd[0], tx_bd[1], tx_bd[2], tx_bd[3]);
		}
	}

	bnx2x_fw_dump(bp);
	bnx2x_mc_assert(bp);
	BNX2X_ERR("end crash dump -----------------\n");
}

static void bnx2x_int_enable(struct bnx2x *bp)
{
	int port = BP_PORT(bp);
	u32 addr = port ? HC_REG_CONFIG_1 : HC_REG_CONFIG_0;
	u32 val = REG_RD(bp, addr);
	int msix = (bp->flags & USING_MSIX_FLAG) ? 1 : 0;
	int msi = (bp->flags & USING_MSI_FLAG) ? 1 : 0;

	if (msix) {
		val &= ~(HC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
			 HC_CONFIG_0_REG_INT_LINE_EN_0);
		val |= (HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
			HC_CONFIG_0_REG_ATTN_BIT_EN_0);
	} else if (msi) {
		val &= ~HC_CONFIG_0_REG_INT_LINE_EN_0;
		val |= (HC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
			HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
			HC_CONFIG_0_REG_ATTN_BIT_EN_0);
	} else {
		val |= (HC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
			HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
			HC_CONFIG_0_REG_INT_LINE_EN_0 |
			HC_CONFIG_0_REG_ATTN_BIT_EN_0);

		DP(NETIF_MSG_INTR, "write %x to HC %d (addr 0x%x)\n",
		   val, port, addr);

		REG_WR(bp, addr, val);

		val &= ~HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0;
	}

	DP(NETIF_MSG_INTR, "write %x to HC %d (addr 0x%x)  mode %s\n",
	   val, port, addr, (msix ? "MSI-X" : (msi ? "MSI" : "INTx")));

	REG_WR(bp, addr, val);
	/*
	 * Ensure that HC_CONFIG is written before leading/trailing edge config
	 */
	mmiowb();
	barrier();

	if (CHIP_IS_E1H(bp)) {
		/* init leading/trailing edge */
		if (IS_E1HMF(bp)) {
			val = (0xee0f | (1 << (BP_E1HVN(bp) + 4)));
			if (bp->port.pmf)
				/* enable nig and gpio3 attention */
				val |= 0x1100;
		} else
			val = 0xffff;

		REG_WR(bp, HC_REG_TRAILING_EDGE_0 + port*8, val);
		REG_WR(bp, HC_REG_LEADING_EDGE_0 + port*8, val);
	}

	/* Make sure that interrupts are indeed enabled from here on */
	mmiowb();
}

static void bnx2x_int_disable(struct bnx2x *bp)
{
	int port = BP_PORT(bp);
	u32 addr = port ? HC_REG_CONFIG_1 : HC_REG_CONFIG_0;
	u32 val = REG_RD(bp, addr);

	val &= ~(HC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
		 HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
		 HC_CONFIG_0_REG_INT_LINE_EN_0 |
		 HC_CONFIG_0_REG_ATTN_BIT_EN_0);

	DP(NETIF_MSG_INTR, "write %x to HC %d (addr 0x%x)\n",
	   val, port, addr);

	/* flush all outstanding writes */
	mmiowb();

	REG_WR(bp, addr, val);
	if (REG_RD(bp, addr) != val)
		BNX2X_ERR("BUG! proper val not read from IGU!\n");
}

static void bnx2x_int_disable_sync(struct bnx2x *bp, int disable_hw)
{
	int msix = (bp->flags & USING_MSIX_FLAG) ? 1 : 0;
	int i, offset;

	/* disable interrupt handling */
	atomic_inc(&bp->intr_sem);
	smp_wmb(); /* Ensure that bp->intr_sem update is SMP-safe */

	if (disable_hw)
		/* prevent the HW from sending interrupts */
		bnx2x_int_disable(bp);

	/* make sure all ISRs are done */
	if (msix) {
		synchronize_irq(bp->msix_table[0].vector);
		offset = 1;
		for_each_queue(bp, i)
			synchronize_irq(bp->msix_table[i + offset].vector);
	} else
		synchronize_irq(bp->pdev->irq);

	/* make sure sp_task is not running */
	cancel_delayed_work(&bp->sp_task);
	flush_workqueue(bnx2x_wq);
}

/* fast path */

/*
 * General service functions
 */

static inline void bnx2x_ack_sb(struct bnx2x *bp, u8 sb_id,
				u8 storm, u16 index, u8 op, u8 update)
{
	u32 hc_addr = (HC_REG_COMMAND_REG + BP_PORT(bp)*32 +
		       COMMAND_REG_INT_ACK);
	struct igu_ack_register igu_ack;

	igu_ack.status_block_index = index;
	igu_ack.sb_id_and_flags =
			((sb_id << IGU_ACK_REGISTER_STATUS_BLOCK_ID_SHIFT) |
			 (storm << IGU_ACK_REGISTER_STORM_ID_SHIFT) |
			 (update << IGU_ACK_REGISTER_UPDATE_INDEX_SHIFT) |
			 (op << IGU_ACK_REGISTER_INTERRUPT_MODE_SHIFT));

	DP(BNX2X_MSG_OFF, "write 0x%08x to HC addr 0x%x\n",
	   (*(u32 *)&igu_ack), hc_addr);
	REG_WR(bp, hc_addr, (*(u32 *)&igu_ack));

	/* Make sure that ACK is written */
	mmiowb();
	barrier();
}

static inline u16 bnx2x_update_fpsb_idx(struct bnx2x_fastpath *fp)
{
	struct host_status_block *fpsb = fp->status_blk;
	u16 rc = 0;

	barrier(); /* status block is written to by the chip */
	if (fp->fp_c_idx != fpsb->c_status_block.status_block_index) {
		fp->fp_c_idx = fpsb->c_status_block.status_block_index;
		rc |= 1;
	}
	if (fp->fp_u_idx != fpsb->u_status_block.status_block_index) {
		fp->fp_u_idx = fpsb->u_status_block.status_block_index;
		rc |= 2;
	}
	return rc;
}

static u16 bnx2x_ack_int(struct bnx2x *bp)
{
	u32 hc_addr = (HC_REG_COMMAND_REG + BP_PORT(bp)*32 +
		       COMMAND_REG_SIMD_MASK);
	u32 result = REG_RD(bp, hc_addr);

	DP(BNX2X_MSG_OFF, "read 0x%08x from HC addr 0x%x\n",
	   result, hc_addr);

	return result;
}


/*
 * fast path service functions
 */

static inline int bnx2x_has_tx_work_unload(struct bnx2x_fastpath *fp)
{
	/* Tell compiler that consumer and producer can change */
	barrier();
	return (fp->tx_pkt_prod != fp->tx_pkt_cons);
}

/* free skb in the packet ring at pos idx
 * return idx of last bd freed
 */
static u16 bnx2x_free_tx_pkt(struct bnx2x *bp, struct bnx2x_fastpath *fp,
			     u16 idx)
{
	struct sw_tx_bd *tx_buf = &fp->tx_buf_ring[idx];
	struct eth_tx_start_bd *tx_start_bd;
	struct eth_tx_bd *tx_data_bd;
	struct sk_buff *skb = tx_buf->skb;
	u16 bd_idx = TX_BD(tx_buf->first_bd), new_cons;
	int nbd;

	DP(BNX2X_MSG_OFF, "pkt_idx %d  buff @(%p)->skb %p\n",
	   idx, tx_buf, skb);

	/* unmap first bd */
	DP(BNX2X_MSG_OFF, "free bd_idx %d\n", bd_idx);
	tx_start_bd = &fp->tx_desc_ring[bd_idx].start_bd;
	pci_unmap_single(bp->pdev, BD_UNMAP_ADDR(tx_start_bd),
			 BD_UNMAP_LEN(tx_start_bd), PCI_DMA_TODEVICE);

	nbd = le16_to_cpu(tx_start_bd->nbd) - 1;
#ifdef BNX2X_STOP_ON_ERROR
	if ((nbd - 1) > (MAX_SKB_FRAGS + 2)) {
		BNX2X_ERR("BAD nbd!\n");
		bnx2x_panic();
	}
#endif
	new_cons = nbd + tx_buf->first_bd;

	/* Get the next bd */
	bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));

	/* Skip a parse bd... */
	--nbd;
	bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));

	/* ...and the TSO split header bd since they have no mapping */
	if (tx_buf->flags & BNX2X_TSO_SPLIT_BD) {
		--nbd;
		bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));
	}

	/* now free frags */
	while (nbd > 0) {

		DP(BNX2X_MSG_OFF, "free frag bd_idx %d\n", bd_idx);
		tx_data_bd = &fp->tx_desc_ring[bd_idx].reg_bd;
		pci_unmap_page(bp->pdev, BD_UNMAP_ADDR(tx_data_bd),
			       BD_UNMAP_LEN(tx_data_bd), PCI_DMA_TODEVICE);
		if (--nbd)
			bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));
	}

	/* release skb */
	WARN_ON(!skb);
	dev_kfree_skb_any(skb);
	tx_buf->first_bd = 0;
	tx_buf->skb = NULL;

	return new_cons;
}

static inline u16 bnx2x_tx_avail(struct bnx2x_fastpath *fp)
{
	s16 used;
	u16 prod;
	u16 cons;

	barrier(); /* Tell compiler that prod and cons can change */
	prod = fp->tx_bd_prod;
	cons = fp->tx_bd_cons;

	/* NUM_TX_RINGS = number of "next-page" entries
	   It will be used as a threshold */
	used = SUB_S16(prod, cons) + (s16)NUM_TX_RINGS;

#ifdef BNX2X_STOP_ON_ERROR
	WARN_ON(used < 0);
	WARN_ON(used > fp->bp->tx_ring_size);
	WARN_ON((fp->bp->tx_ring_size - used) > MAX_TX_AVAIL);
#endif

	return (s16)(fp->bp->tx_ring_size) - used;
}

static void bnx2x_tx_int(struct bnx2x_fastpath *fp)
{
	struct bnx2x *bp = fp->bp;
	struct netdev_queue *txq;
	u16 hw_cons, sw_cons, bd_cons = fp->tx_bd_cons;
	int done = 0;

#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		return;
#endif

	txq = netdev_get_tx_queue(bp->dev, fp->index - bp->num_rx_queues);
	hw_cons = le16_to_cpu(*fp->tx_cons_sb);
	sw_cons = fp->tx_pkt_cons;

	while (sw_cons != hw_cons) {
		u16 pkt_cons;

		pkt_cons = TX_BD(sw_cons);

		/* prefetch(bp->tx_buf_ring[pkt_cons].skb); */

		DP(NETIF_MSG_TX_DONE, "hw_cons %u  sw_cons %u  pkt_cons %u\n",
		   hw_cons, sw_cons, pkt_cons);

/*		if (NEXT_TX_IDX(sw_cons) != hw_cons) {
			rmb();
			prefetch(fp->tx_buf_ring[NEXT_TX_IDX(sw_cons)].skb);
		}
*/
		bd_cons = bnx2x_free_tx_pkt(bp, fp, pkt_cons);
		sw_cons++;
		done++;
	}

	fp->tx_pkt_cons = sw_cons;
	fp->tx_bd_cons = bd_cons;

	/* TBD need a thresh? */
	if (unlikely(netif_tx_queue_stopped(txq))) {

		/* Need to make the tx_bd_cons update visible to start_xmit()
		 * before checking for netif_tx_queue_stopped().  Without the
		 * memory barrier, there is a small possibility that
		 * start_xmit() will miss it and cause the queue to be stopped
		 * forever.
		 */
		smp_mb();

		if ((netif_tx_queue_stopped(txq)) &&
		    (bp->state == BNX2X_STATE_OPEN) &&
		    (bnx2x_tx_avail(fp) >= MAX_SKB_FRAGS + 3))
			netif_tx_wake_queue(txq);
	}
}


static void bnx2x_sp_event(struct bnx2x_fastpath *fp,
			   union eth_rx_cqe *rr_cqe)
{
	struct bnx2x *bp = fp->bp;
	int cid = SW_CID(rr_cqe->ramrod_cqe.conn_and_cmd_data);
	int command = CQE_CMD(rr_cqe->ramrod_cqe.conn_and_cmd_data);

	DP(BNX2X_MSG_SP,
	   "fp %d  cid %d  got ramrod #%d  state is %x  type is %d\n",
	   fp->index, cid, command, bp->state,
	   rr_cqe->ramrod_cqe.ramrod_type);

	bp->spq_left++;

	if (fp->index) {
		switch (command | fp->state) {
		case (RAMROD_CMD_ID_ETH_CLIENT_SETUP |
						BNX2X_FP_STATE_OPENING):
			DP(NETIF_MSG_IFUP, "got MULTI[%d] setup ramrod\n",
			   cid);
			fp->state = BNX2X_FP_STATE_OPEN;
			break;

		case (RAMROD_CMD_ID_ETH_HALT | BNX2X_FP_STATE_HALTING):
			DP(NETIF_MSG_IFDOWN, "got MULTI[%d] halt ramrod\n",
			   cid);
			fp->state = BNX2X_FP_STATE_HALTED;
			break;

		default:
			BNX2X_ERR("unexpected MC reply (%d)  "
				  "fp->state is %x\n", command, fp->state);
			break;
		}
		mb(); /* force bnx2x_wait_ramrod() to see the change */
		return;
	}

	switch (command | bp->state) {
	case (RAMROD_CMD_ID_ETH_PORT_SETUP | BNX2X_STATE_OPENING_WAIT4_PORT):
		DP(NETIF_MSG_IFUP, "got setup ramrod\n");
		bp->state = BNX2X_STATE_OPEN;
		break;

	case (RAMROD_CMD_ID_ETH_HALT | BNX2X_STATE_CLOSING_WAIT4_HALT):
		DP(NETIF_MSG_IFDOWN, "got halt ramrod\n");
		bp->state = BNX2X_STATE_CLOSING_WAIT4_DELETE;
		fp->state = BNX2X_FP_STATE_HALTED;
		break;

	case (RAMROD_CMD_ID_ETH_CFC_DEL | BNX2X_STATE_CLOSING_WAIT4_HALT):
		DP(NETIF_MSG_IFDOWN, "got delete ramrod for MULTI[%d]\n", cid);
		bnx2x_fp(bp, cid, state) = BNX2X_FP_STATE_CLOSED;
		break;


	case (RAMROD_CMD_ID_ETH_SET_MAC | BNX2X_STATE_OPEN):
	case (RAMROD_CMD_ID_ETH_SET_MAC | BNX2X_STATE_DIAG):
		DP(NETIF_MSG_IFUP, "got set mac ramrod\n");
		bp->set_mac_pending = 0;
		break;

	case (RAMROD_CMD_ID_ETH_SET_MAC | BNX2X_STATE_CLOSING_WAIT4_HALT):
	case (RAMROD_CMD_ID_ETH_SET_MAC | BNX2X_STATE_DISABLED):
		DP(NETIF_MSG_IFDOWN, "got (un)set mac ramrod\n");
		break;

	default:
		BNX2X_ERR("unexpected MC reply (%d)  bp->state is %x\n",
			  command, bp->state);
		break;
	}
	mb(); /* force bnx2x_wait_ramrod() to see the change */
}

static inline void bnx2x_free_rx_sge(struct bnx2x *bp,
				     struct bnx2x_fastpath *fp, u16 index)
{
	struct sw_rx_page *sw_buf = &fp->rx_page_ring[index];
	struct page *page = sw_buf->page;
	struct eth_rx_sge *sge = &fp->rx_sge_ring[index];

	/* Skip "next page" elements */
	if (!page)
		return;

	pci_unmap_page(bp->pdev, pci_unmap_addr(sw_buf, mapping),
		       SGE_PAGE_SIZE*PAGES_PER_SGE, PCI_DMA_FROMDEVICE);
	__free_pages(page, PAGES_PER_SGE_SHIFT);

	sw_buf->page = NULL;
	sge->addr_hi = 0;
	sge->addr_lo = 0;
}

static inline void bnx2x_free_rx_sge_range(struct bnx2x *bp,
					   struct bnx2x_fastpath *fp, int last)
{
	int i;

	for (i = 0; i < last; i++)
		bnx2x_free_rx_sge(bp, fp, i);
}

static inline int bnx2x_alloc_rx_sge(struct bnx2x *bp,
				     struct bnx2x_fastpath *fp, u16 index)
{
	struct page *page = alloc_pages(GFP_ATOMIC, PAGES_PER_SGE_SHIFT);
	struct sw_rx_page *sw_buf = &fp->rx_page_ring[index];
	struct eth_rx_sge *sge = &fp->rx_sge_ring[index];
	dma_addr_t mapping;

	if (unlikely(page == NULL))
		return -ENOMEM;

	mapping = pci_map_page(bp->pdev, page, 0, SGE_PAGE_SIZE*PAGES_PER_SGE,
			       PCI_DMA_FROMDEVICE);
	if (unlikely(dma_mapping_error(&bp->pdev->dev, mapping))) {
		__free_pages(page, PAGES_PER_SGE_SHIFT);
		return -ENOMEM;
	}

	sw_buf->page = page;
	pci_unmap_addr_set(sw_buf, mapping, mapping);

	sge->addr_hi = cpu_to_le32(U64_HI(mapping));
	sge->addr_lo = cpu_to_le32(U64_LO(mapping));

	return 0;
}

static inline int bnx2x_alloc_rx_skb(struct bnx2x *bp,
				     struct bnx2x_fastpath *fp, u16 index)
{
	struct sk_buff *skb;
	struct sw_rx_bd *rx_buf = &fp->rx_buf_ring[index];
	struct eth_rx_bd *rx_bd = &fp->rx_desc_ring[index];
	dma_addr_t mapping;

	skb = netdev_alloc_skb(bp->dev, bp->rx_buf_size);
	if (unlikely(skb == NULL))
		return -ENOMEM;

	mapping = pci_map_single(bp->pdev, skb->data, bp->rx_buf_size,
				 PCI_DMA_FROMDEVICE);
	if (unlikely(dma_mapping_error(&bp->pdev->dev, mapping))) {
		dev_kfree_skb(skb);
		return -ENOMEM;
	}

	rx_buf->skb = skb;
	pci_unmap_addr_set(rx_buf, mapping, mapping);

	rx_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	rx_bd->addr_lo = cpu_to_le32(U64_LO(mapping));

	return 0;
}

/* note that we are not allocating a new skb,
 * we are just moving one from cons to prod
 * we are not creating a new mapping,
 * so there is no need to check for dma_mapping_error().
 */
static void bnx2x_reuse_rx_skb(struct bnx2x_fastpath *fp,
			       struct sk_buff *skb, u16 cons, u16 prod)
{
	struct bnx2x *bp = fp->bp;
	struct sw_rx_bd *cons_rx_buf = &fp->rx_buf_ring[cons];
	struct sw_rx_bd *prod_rx_buf = &fp->rx_buf_ring[prod];
	struct eth_rx_bd *cons_bd = &fp->rx_desc_ring[cons];
	struct eth_rx_bd *prod_bd = &fp->rx_desc_ring[prod];

	pci_dma_sync_single_for_device(bp->pdev,
				       pci_unmap_addr(cons_rx_buf, mapping),
				       RX_COPY_THRESH, PCI_DMA_FROMDEVICE);

	prod_rx_buf->skb = cons_rx_buf->skb;
	pci_unmap_addr_set(prod_rx_buf, mapping,
			   pci_unmap_addr(cons_rx_buf, mapping));
	*prod_bd = *cons_bd;
}

static inline void bnx2x_update_last_max_sge(struct bnx2x_fastpath *fp,
					     u16 idx)
{
	u16 last_max = fp->last_max_sge;

	if (SUB_S16(idx, last_max) > 0)
		fp->last_max_sge = idx;
}

static void bnx2x_clear_sge_mask_next_elems(struct bnx2x_fastpath *fp)
{
	int i, j;

	for (i = 1; i <= NUM_RX_SGE_PAGES; i++) {
		int idx = RX_SGE_CNT * i - 1;

		for (j = 0; j < 2; j++) {
			SGE_MASK_CLEAR_BIT(fp, idx);
			idx--;
		}
	}
}

static void bnx2x_update_sge_prod(struct bnx2x_fastpath *fp,
				  struct eth_fast_path_rx_cqe *fp_cqe)
{
	struct bnx2x *bp = fp->bp;
	u16 sge_len = SGE_PAGE_ALIGN(le16_to_cpu(fp_cqe->pkt_len) -
				     le16_to_cpu(fp_cqe->len_on_bd)) >>
		      SGE_PAGE_SHIFT;
	u16 last_max, last_elem, first_elem;
	u16 delta = 0;
	u16 i;

	if (!sge_len)
		return;

	/* First mark all used pages */
	for (i = 0; i < sge_len; i++)
		SGE_MASK_CLEAR_BIT(fp, RX_SGE(le16_to_cpu(fp_cqe->sgl[i])));

	DP(NETIF_MSG_RX_STATUS, "fp_cqe->sgl[%d] = %d\n",
	   sge_len - 1, le16_to_cpu(fp_cqe->sgl[sge_len - 1]));

	/* Here we assume that the last SGE index is the biggest */
	prefetch((void *)(fp->sge_mask));
	bnx2x_update_last_max_sge(fp, le16_to_cpu(fp_cqe->sgl[sge_len - 1]));

	last_max = RX_SGE(fp->last_max_sge);
	last_elem = last_max >> RX_SGE_MASK_ELEM_SHIFT;
	first_elem = RX_SGE(fp->rx_sge_prod) >> RX_SGE_MASK_ELEM_SHIFT;

	/* If ring is not full */
	if (last_elem + 1 != first_elem)
		last_elem++;

	/* Now update the prod */
	for (i = first_elem; i != last_elem; i = NEXT_SGE_MASK_ELEM(i)) {
		if (likely(fp->sge_mask[i]))
			break;

		fp->sge_mask[i] = RX_SGE_MASK_ELEM_ONE_MASK;
		delta += RX_SGE_MASK_ELEM_SZ;
	}

	if (delta > 0) {
		fp->rx_sge_prod += delta;
		/* clear page-end entries */
		bnx2x_clear_sge_mask_next_elems(fp);
	}

	DP(NETIF_MSG_RX_STATUS,
	   "fp->last_max_sge = %d  fp->rx_sge_prod = %d\n",
	   fp->last_max_sge, fp->rx_sge_prod);
}

static inline void bnx2x_init_sge_ring_bit_mask(struct bnx2x_fastpath *fp)
{
	/* Set the mask to all 1-s: it's faster to compare to 0 than to 0xf-s */
	memset(fp->sge_mask, 0xff,
	       (NUM_RX_SGE >> RX_SGE_MASK_ELEM_SHIFT)*sizeof(u64));

	/* Clear the two last indices in the page to 1:
	   these are the indices that correspond to the "next" element,
	   hence will never be indicated and should be removed from
	   the calculations. */
	bnx2x_clear_sge_mask_next_elems(fp);
}

static void bnx2x_tpa_start(struct bnx2x_fastpath *fp, u16 queue,
			    struct sk_buff *skb, u16 cons, u16 prod)
{
	struct bnx2x *bp = fp->bp;
	struct sw_rx_bd *cons_rx_buf = &fp->rx_buf_ring[cons];
	struct sw_rx_bd *prod_rx_buf = &fp->rx_buf_ring[prod];
	struct eth_rx_bd *prod_bd = &fp->rx_desc_ring[prod];
	dma_addr_t mapping;

	/* move empty skb from pool to prod and map it */
	prod_rx_buf->skb = fp->tpa_pool[queue].skb;
	mapping = pci_map_single(bp->pdev, fp->tpa_pool[queue].skb->data,
				 bp->rx_buf_size, PCI_DMA_FROMDEVICE);
	pci_unmap_addr_set(prod_rx_buf, mapping, mapping);

	/* move partial skb from cons to pool (don't unmap yet) */
	fp->tpa_pool[queue] = *cons_rx_buf;

	/* mark bin state as start - print error if current state != stop */
	if (fp->tpa_state[queue] != BNX2X_TPA_STOP)
		BNX2X_ERR("start of bin not in stop [%d]\n", queue);

	fp->tpa_state[queue] = BNX2X_TPA_START;

	/* point prod_bd to new skb */
	prod_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	prod_bd->addr_lo = cpu_to_le32(U64_LO(mapping));

#ifdef BNX2X_STOP_ON_ERROR
	fp->tpa_queue_used |= (1 << queue);
#ifdef __powerpc64__
	DP(NETIF_MSG_RX_STATUS, "fp->tpa_queue_used = 0x%lx\n",
#else
	DP(NETIF_MSG_RX_STATUS, "fp->tpa_queue_used = 0x%llx\n",
#endif
	   fp->tpa_queue_used);
#endif
}

static int bnx2x_fill_frag_skb(struct bnx2x *bp, struct bnx2x_fastpath *fp,
			       struct sk_buff *skb,
			       struct eth_fast_path_rx_cqe *fp_cqe,
			       u16 cqe_idx)
{
	struct sw_rx_page *rx_pg, old_rx_pg;
	u16 len_on_bd = le16_to_cpu(fp_cqe->len_on_bd);
	u32 i, frag_len, frag_size, pages;
	int err;
	int j;

	frag_size = le16_to_cpu(fp_cqe->pkt_len) - len_on_bd;
	pages = SGE_PAGE_ALIGN(frag_size) >> SGE_PAGE_SHIFT;

	/* This is needed in order to enable forwarding support */
	if (frag_size)
		skb_shinfo(skb)->gso_size = min((u32)SGE_PAGE_SIZE,
					       max(frag_size, (u32)len_on_bd));

#ifdef BNX2X_STOP_ON_ERROR
	if (pages >
	    min((u32)8, (u32)MAX_SKB_FRAGS) * SGE_PAGE_SIZE * PAGES_PER_SGE) {
		BNX2X_ERR("SGL length is too long: %d. CQE index is %d\n",
			  pages, cqe_idx);
		BNX2X_ERR("fp_cqe->pkt_len = %d  fp_cqe->len_on_bd = %d\n",
			  fp_cqe->pkt_len, len_on_bd);
		bnx2x_panic();
		return -EINVAL;
	}
#endif

	/* Run through the SGL and compose the fragmented skb */
	for (i = 0, j = 0; i < pages; i += PAGES_PER_SGE, j++) {
		u16 sge_idx = RX_SGE(le16_to_cpu(fp_cqe->sgl[j]));

		/* FW gives the indices of the SGE as if the ring is an array
		   (meaning that "next" element will consume 2 indices) */
		frag_len = min(frag_size, (u32)(SGE_PAGE_SIZE*PAGES_PER_SGE));
		rx_pg = &fp->rx_page_ring[sge_idx];
		old_rx_pg = *rx_pg;

		/* If we fail to allocate a substitute page, we simply stop
		   where we are and drop the whole packet */
		err = bnx2x_alloc_rx_sge(bp, fp, sge_idx);
		if (unlikely(err)) {
			fp->eth_q_stats.rx_skb_alloc_failed++;
			return err;
		}

		/* Unmap the page as we r going to pass it to the stack */
		pci_unmap_page(bp->pdev, pci_unmap_addr(&old_rx_pg, mapping),
			      SGE_PAGE_SIZE*PAGES_PER_SGE, PCI_DMA_FROMDEVICE);

		/* Add one frag and update the appropriate fields in the skb */
		skb_fill_page_desc(skb, j, old_rx_pg.page, 0, frag_len);

		skb->data_len += frag_len;
		skb->truesize += frag_len;
		skb->len += frag_len;

		frag_size -= frag_len;
	}

	return 0;
}

static void bnx2x_tpa_stop(struct bnx2x *bp, struct bnx2x_fastpath *fp,
			   u16 queue, int pad, int len, union eth_rx_cqe *cqe,
			   u16 cqe_idx)
{
	struct sw_rx_bd *rx_buf = &fp->tpa_pool[queue];
	struct sk_buff *skb = rx_buf->skb;
	/* alloc new skb */
	struct sk_buff *new_skb = netdev_alloc_skb(bp->dev, bp->rx_buf_size);

	/* Unmap skb in the pool anyway, as we are going to change
	   pool entry status to BNX2X_TPA_STOP even if new skb allocation
	   fails. */
	pci_unmap_single(bp->pdev, pci_unmap_addr(rx_buf, mapping),
			 bp->rx_buf_size, PCI_DMA_FROMDEVICE);

	if (likely(new_skb)) {
		/* fix ip xsum and give it to the stack */
		/* (no need to map the new skb) */
#ifdef BCM_VLAN
		int is_vlan_cqe =
			(le16_to_cpu(cqe->fast_path_cqe.pars_flags.flags) &
			 PARSING_FLAGS_VLAN);
		int is_not_hwaccel_vlan_cqe =
			(is_vlan_cqe && (!(bp->flags & HW_VLAN_RX_FLAG)));
#endif

		prefetch(skb);
		prefetch(((char *)(skb)) + 128);

#ifdef BNX2X_STOP_ON_ERROR
		if (pad + len > bp->rx_buf_size) {
			BNX2X_ERR("skb_put is about to fail...  "
				  "pad %d  len %d  rx_buf_size %d\n",
				  pad, len, bp->rx_buf_size);
			bnx2x_panic();
			return;
		}
#endif

		skb_reserve(skb, pad);
		skb_put(skb, len);

		skb->protocol = eth_type_trans(skb, bp->dev);
		skb->ip_summed = CHECKSUM_UNNECESSARY;

		{
			struct iphdr *iph;

			iph = (struct iphdr *)skb->data;
#ifdef BCM_VLAN
			/* If there is no Rx VLAN offloading -
			   take VLAN tag into an account */
			if (unlikely(is_not_hwaccel_vlan_cqe))
				iph = (struct iphdr *)((u8 *)iph + VLAN_HLEN);
#endif
			iph->check = 0;
			iph->check = ip_fast_csum((u8 *)iph, iph->ihl);
		}

		if (!bnx2x_fill_frag_skb(bp, fp, skb,
					 &cqe->fast_path_cqe, cqe_idx)) {
#ifdef BCM_VLAN
			if ((bp->vlgrp != NULL) && is_vlan_cqe &&
			    (!is_not_hwaccel_vlan_cqe))
				vlan_hwaccel_receive_skb(skb, bp->vlgrp,
						le16_to_cpu(cqe->fast_path_cqe.
							    vlan_tag));
			else
#endif
				netif_receive_skb(skb);
		} else {
			DP(NETIF_MSG_RX_STATUS, "Failed to allocate new pages"
			   " - dropping packet!\n");
			dev_kfree_skb(skb);
		}


		/* put new skb in bin */
		fp->tpa_pool[queue].skb = new_skb;

	} else {
		/* else drop the packet and keep the buffer in the bin */
		DP(NETIF_MSG_RX_STATUS,
		   "Failed to allocate new skb - dropping packet!\n");
		fp->eth_q_stats.rx_skb_alloc_failed++;
	}

	fp->tpa_state[queue] = BNX2X_TPA_STOP;
}

static inline void bnx2x_update_rx_prod(struct bnx2x *bp,
					struct bnx2x_fastpath *fp,
					u16 bd_prod, u16 rx_comp_prod,
					u16 rx_sge_prod)
{
	struct ustorm_eth_rx_producers rx_prods = {0};
	int i;

	/* Update producers */
	rx_prods.bd_prod = bd_prod;
	rx_prods.cqe_prod = rx_comp_prod;
	rx_prods.sge_prod = rx_sge_prod;

	/*
	 * Make sure that the BD and SGE data is updated before updating the
	 * producers since FW might read the BD/SGE right after the producer
	 * is updated.
	 * This is only applicable for weak-ordered memory model archs such
	 * as IA-64. The following barrier is also mandatory since FW will
	 * assumes BDs must have buffers.
	 */
	wmb();

	for (i = 0; i < sizeof(struct ustorm_eth_rx_producers)/4; i++)
		REG_WR(bp, BAR_USTRORM_INTMEM +
		       USTORM_RX_PRODS_OFFSET(BP_PORT(bp), fp->cl_id) + i*4,
		       ((u32 *)&rx_prods)[i]);

	mmiowb(); /* keep prod updates ordered */

	DP(NETIF_MSG_RX_STATUS,
	   "queue[%d]:  wrote  bd_prod %u  cqe_prod %u  sge_prod %u\n",
	   fp->index, bd_prod, rx_comp_prod, rx_sge_prod);
}

static int bnx2x_rx_int(struct bnx2x_fastpath *fp, int budget)
{
	struct bnx2x *bp = fp->bp;
	u16 bd_cons, bd_prod, bd_prod_fw, comp_ring_cons;
	u16 hw_comp_cons, sw_comp_cons, sw_comp_prod;
	int rx_pkt = 0;

#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		return 0;
#endif

	/* CQ "next element" is of the size of the regular element,
	   that's why it's ok here */
	hw_comp_cons = le16_to_cpu(*fp->rx_cons_sb);
	if ((hw_comp_cons & MAX_RCQ_DESC_CNT) == MAX_RCQ_DESC_CNT)
		hw_comp_cons++;

	bd_cons = fp->rx_bd_cons;
	bd_prod = fp->rx_bd_prod;
	bd_prod_fw = bd_prod;
	sw_comp_cons = fp->rx_comp_cons;
	sw_comp_prod = fp->rx_comp_prod;

	/* Memory barrier necessary as speculative reads of the rx
	 * buffer can be ahead of the index in the status block
	 */
	rmb();

	DP(NETIF_MSG_RX_STATUS,
	   "queue[%d]:  hw_comp_cons %u  sw_comp_cons %u\n",
	   fp->index, hw_comp_cons, sw_comp_cons);

	while (sw_comp_cons != hw_comp_cons) {
		struct sw_rx_bd *rx_buf = NULL;
		struct sk_buff *skb;
		union eth_rx_cqe *cqe;
		u8 cqe_fp_flags;
		u16 len, pad;

		comp_ring_cons = RCQ_BD(sw_comp_cons);
		bd_prod = RX_BD(bd_prod);
		bd_cons = RX_BD(bd_cons);

		/* Prefetch the page containing the BD descriptor
		   at producer's index. It will be needed when new skb is
		   allocated */
		prefetch((void *)(PAGE_ALIGN((unsigned long)
					     (&fp->rx_desc_ring[bd_prod])) -
				  PAGE_SIZE + 1));

		cqe = &fp->rx_comp_ring[comp_ring_cons];
		cqe_fp_flags = cqe->fast_path_cqe.type_error_flags;

		DP(NETIF_MSG_RX_STATUS, "CQE type %x  err %x  status %x"
		   "  queue %x  vlan %x  len %u\n", CQE_TYPE(cqe_fp_flags),
		   cqe_fp_flags, cqe->fast_path_cqe.status_flags,
		   le32_to_cpu(cqe->fast_path_cqe.rss_hash_result),
		   le16_to_cpu(cqe->fast_path_cqe.vlan_tag),
		   le16_to_cpu(cqe->fast_path_cqe.pkt_len));

		/* is this a slowpath msg? */
		if (unlikely(CQE_TYPE(cqe_fp_flags))) {
			bnx2x_sp_event(fp, cqe);
			goto next_cqe;

		/* this is an rx packet */
		} else {
			rx_buf = &fp->rx_buf_ring[bd_cons];
			skb = rx_buf->skb;
			len = le16_to_cpu(cqe->fast_path_cqe.pkt_len);
			pad = cqe->fast_path_cqe.placement_offset;

			/* If CQE is marked both TPA_START and TPA_END
			   it is a non-TPA CQE */
			if ((!fp->disable_tpa) &&
			    (TPA_TYPE(cqe_fp_flags) !=
					(TPA_TYPE_START | TPA_TYPE_END))) {
				u16 queue = cqe->fast_path_cqe.queue_index;

				if (TPA_TYPE(cqe_fp_flags) == TPA_TYPE_START) {
					DP(NETIF_MSG_RX_STATUS,
					   "calling tpa_start on queue %d\n",
					   queue);

					bnx2x_tpa_start(fp, queue, skb,
							bd_cons, bd_prod);
					goto next_rx;
				}

				if (TPA_TYPE(cqe_fp_flags) == TPA_TYPE_END) {
					DP(NETIF_MSG_RX_STATUS,
					   "calling tpa_stop on queue %d\n",
					   queue);

					if (!BNX2X_RX_SUM_FIX(cqe))
						BNX2X_ERR("STOP on none TCP "
							  "data\n");

					/* This is a size of the linear data
					   on this skb */
					len = le16_to_cpu(cqe->fast_path_cqe.
								len_on_bd);
					bnx2x_tpa_stop(bp, fp, queue, pad,
						    len, cqe, comp_ring_cons);
#ifdef BNX2X_STOP_ON_ERROR
					if (bp->panic)
						return 0;
#endif

					bnx2x_update_sge_prod(fp,
							&cqe->fast_path_cqe);
					goto next_cqe;
				}
			}

			pci_dma_sync_single_for_device(bp->pdev,
					pci_unmap_addr(rx_buf, mapping),
						       pad + RX_COPY_THRESH,
						       PCI_DMA_FROMDEVICE);
			prefetch(skb);
			prefetch(((char *)(skb)) + 128);

			/* is this an error packet? */
			if (unlikely(cqe_fp_flags & ETH_RX_ERROR_FALGS)) {
				DP(NETIF_MSG_RX_ERR,
				   "ERROR  flags %x  rx packet %u\n",
				   cqe_fp_flags, sw_comp_cons);
				fp->eth_q_stats.rx_err_discard_pkt++;
				goto reuse_rx;
			}

			/* Since we don't have a jumbo ring
			 * copy small packets if mtu > 1500
			 */
			if ((bp->dev->mtu > ETH_MAX_PACKET_SIZE) &&
			    (len <= RX_COPY_THRESH)) {
				struct sk_buff *new_skb;

				new_skb = netdev_alloc_skb(bp->dev,
							   len + pad);
				if (new_skb == NULL) {
					DP(NETIF_MSG_RX_ERR,
					   "ERROR  packet dropped "
					   "because of alloc failure\n");
					fp->eth_q_stats.rx_skb_alloc_failed++;
					goto reuse_rx;
				}

				/* aligned copy */
				skb_copy_from_linear_data_offset(skb, pad,
						    new_skb->data + pad, len);
				skb_reserve(new_skb, pad);
				skb_put(new_skb, len);

				bnx2x_reuse_rx_skb(fp, skb, bd_cons, bd_prod);

				skb = new_skb;

			} else
			if (likely(bnx2x_alloc_rx_skb(bp, fp, bd_prod) == 0)) {
				pci_unmap_single(bp->pdev,
					pci_unmap_addr(rx_buf, mapping),
						 bp->rx_buf_size,
						 PCI_DMA_FROMDEVICE);
				skb_reserve(skb, pad);
				skb_put(skb, len);

			} else {
				DP(NETIF_MSG_RX_ERR,
				   "ERROR  packet dropped because "
				   "of alloc failure\n");
				fp->eth_q_stats.rx_skb_alloc_failed++;
reuse_rx:
				bnx2x_reuse_rx_skb(fp, skb, bd_cons, bd_prod);
				goto next_rx;
			}

			skb->protocol = eth_type_trans(skb, bp->dev);

			skb->ip_summed = CHECKSUM_NONE;
			if (bp->rx_csum) {
				if (likely(BNX2X_RX_CSUM_OK(cqe)))
					skb->ip_summed = CHECKSUM_UNNECESSARY;
				else
					fp->eth_q_stats.hw_csum_err++;
			}
		}

		skb_record_rx_queue(skb, fp->index);

#ifdef BCM_VLAN
		if ((bp->vlgrp != NULL) && (bp->flags & HW_VLAN_RX_FLAG) &&
		    (le16_to_cpu(cqe->fast_path_cqe.pars_flags.flags) &
		     PARSING_FLAGS_VLAN))
			vlan_hwaccel_receive_skb(skb, bp->vlgrp,
				le16_to_cpu(cqe->fast_path_cqe.vlan_tag));
		else
#endif
			netif_receive_skb(skb);


next_rx:
		rx_buf->skb = NULL;

		bd_cons = NEXT_RX_IDX(bd_cons);
		bd_prod = NEXT_RX_IDX(bd_prod);
		bd_prod_fw = NEXT_RX_IDX(bd_prod_fw);
		rx_pkt++;
next_cqe:
		sw_comp_prod = NEXT_RCQ_IDX(sw_comp_prod);
		sw_comp_cons = NEXT_RCQ_IDX(sw_comp_cons);

		if (rx_pkt == budget)
			break;
	} /* while */

	fp->rx_bd_cons = bd_cons;
	fp->rx_bd_prod = bd_prod_fw;
	fp->rx_comp_cons = sw_comp_cons;
	fp->rx_comp_prod = sw_comp_prod;

	/* Update producers */
	bnx2x_update_rx_prod(bp, fp, bd_prod_fw, sw_comp_prod,
			     fp->rx_sge_prod);

	fp->rx_pkt += rx_pkt;
	fp->rx_calls++;

	return rx_pkt;
}

static irqreturn_t bnx2x_msix_fp_int(int irq, void *fp_cookie)
{
	struct bnx2x_fastpath *fp = fp_cookie;
	struct bnx2x *bp = fp->bp;

	/* Return here if interrupt is disabled */
	if (unlikely(atomic_read(&bp->intr_sem) != 0)) {
		DP(NETIF_MSG_INTR, "called but intr_sem not 0, returning\n");
		return IRQ_HANDLED;
	}

	DP(BNX2X_MSG_FP, "got an MSI-X interrupt on IDX:SB [%d:%d]\n",
	   fp->index, fp->sb_id);
	bnx2x_ack_sb(bp, fp->sb_id, USTORM_ID, 0, IGU_INT_DISABLE, 0);

#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		return IRQ_HANDLED;
#endif
	/* Handle Rx or Tx according to MSI-X vector */
	if (fp->is_rx_queue) {
		prefetch(fp->rx_cons_sb);
		prefetch(&fp->status_blk->u_status_block.status_block_index);

		napi_schedule(&bnx2x_fp(bp, fp->index, napi));

	} else {
		prefetch(fp->tx_cons_sb);
		prefetch(&fp->status_blk->c_status_block.status_block_index);

		bnx2x_update_fpsb_idx(fp);
		rmb();
		bnx2x_tx_int(fp);

		/* Re-enable interrupts */
		bnx2x_ack_sb(bp, fp->sb_id, USTORM_ID,
			     le16_to_cpu(fp->fp_u_idx), IGU_INT_NOP, 1);
		bnx2x_ack_sb(bp, fp->sb_id, CSTORM_ID,
			     le16_to_cpu(fp->fp_c_idx), IGU_INT_ENABLE, 1);
	}

	return IRQ_HANDLED;
}

static irqreturn_t bnx2x_interrupt(int irq, void *dev_instance)
{
	struct bnx2x *bp = netdev_priv(dev_instance);
	u16 status = bnx2x_ack_int(bp);
	u16 mask;
	int i;

	/* Return here if interrupt is shared and it's not for us */
	if (unlikely(status == 0)) {
		DP(NETIF_MSG_INTR, "not our interrupt!\n");
		return IRQ_NONE;
	}
	DP(NETIF_MSG_INTR, "got an interrupt  status 0x%x\n", status);

	/* Return here if interrupt is disabled */
	if (unlikely(atomic_read(&bp->intr_sem) != 0)) {
		DP(NETIF_MSG_INTR, "called but intr_sem not 0, returning\n");
		return IRQ_HANDLED;
	}

#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		return IRQ_HANDLED;
#endif

	for (i = 0; i < BNX2X_NUM_QUEUES(bp); i++) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		mask = 0x2 << fp->sb_id;
		if (status & mask) {
			/* Handle Rx or Tx according to SB id */
			if (fp->is_rx_queue) {
				prefetch(fp->rx_cons_sb);
				prefetch(&fp->status_blk->u_status_block.
							status_block_index);

				napi_schedule(&bnx2x_fp(bp, fp->index, napi));

			} else {
				prefetch(fp->tx_cons_sb);
				prefetch(&fp->status_blk->c_status_block.
							status_block_index);

				bnx2x_update_fpsb_idx(fp);
				rmb();
				bnx2x_tx_int(fp);

				/* Re-enable interrupts */
				bnx2x_ack_sb(bp, fp->sb_id, USTORM_ID,
					     le16_to_cpu(fp->fp_u_idx),
					     IGU_INT_NOP, 1);
				bnx2x_ack_sb(bp, fp->sb_id, CSTORM_ID,
					     le16_to_cpu(fp->fp_c_idx),
					     IGU_INT_ENABLE, 1);
			}
			status &= ~mask;
		}
	}


	if (unlikely(status & 0x1)) {
		queue_delayed_work(bnx2x_wq, &bp->sp_task, 0);

		status &= ~0x1;
		if (!status)
			return IRQ_HANDLED;
	}

	if (status)
		DP(NETIF_MSG_INTR, "got an unknown interrupt! (status %u)\n",
		   status);

	return IRQ_HANDLED;
}

/* end of fast path */

static void bnx2x_stats_handle(struct bnx2x *bp, enum bnx2x_stats_event event);

/* Link */

/*
 * General service functions
 */

static int bnx2x_acquire_hw_lock(struct bnx2x *bp, u32 resource)
{
	u32 lock_status;
	u32 resource_bit = (1 << resource);
	int func = BP_FUNC(bp);
	u32 hw_lock_control_reg;
	int cnt;

	/* Validating that the resource is within range */
	if (resource > HW_LOCK_MAX_RESOURCE_VALUE) {
		DP(NETIF_MSG_HW,
		   "resource(0x%x) > HW_LOCK_MAX_RESOURCE_VALUE(0x%x)\n",
		   resource, HW_LOCK_MAX_RESOURCE_VALUE);
		return -EINVAL;
	}

	if (func <= 5) {
		hw_lock_control_reg = (MISC_REG_DRIVER_CONTROL_1 + func*8);
	} else {
		hw_lock_control_reg =
				(MISC_REG_DRIVER_CONTROL_7 + (func - 6)*8);
	}

	/* Validating that the resource is not already taken */
	lock_status = REG_RD(bp, hw_lock_control_reg);
	if (lock_status & resource_bit) {
		DP(NETIF_MSG_HW, "lock_status 0x%x  resource_bit 0x%x\n",
		   lock_status, resource_bit);
		return -EEXIST;
	}

	/* Try for 5 second every 5ms */
	for (cnt = 0; cnt < 1000; cnt++) {
		/* Try to acquire the lock */
		REG_WR(bp, hw_lock_control_reg + 4, resource_bit);
		lock_status = REG_RD(bp, hw_lock_control_reg);
		if (lock_status & resource_bit)
			return 0;

		msleep(5);
	}
	DP(NETIF_MSG_HW, "Timeout\n");
	return -EAGAIN;
}

static int bnx2x_release_hw_lock(struct bnx2x *bp, u32 resource)
{
	u32 lock_status;
	u32 resource_bit = (1 << resource);
	int func = BP_FUNC(bp);
	u32 hw_lock_control_reg;

	/* Validating that the resource is within range */
	if (resource > HW_LOCK_MAX_RESOURCE_VALUE) {
		DP(NETIF_MSG_HW,
		   "resource(0x%x) > HW_LOCK_MAX_RESOURCE_VALUE(0x%x)\n",
		   resource, HW_LOCK_MAX_RESOURCE_VALUE);
		return -EINVAL;
	}

	if (func <= 5) {
		hw_lock_control_reg = (MISC_REG_DRIVER_CONTROL_1 + func*8);
	} else {
		hw_lock_control_reg =
				(MISC_REG_DRIVER_CONTROL_7 + (func - 6)*8);
	}

	/* Validating that the resource is currently taken */
	lock_status = REG_RD(bp, hw_lock_control_reg);
	if (!(lock_status & resource_bit)) {
		DP(NETIF_MSG_HW, "lock_status 0x%x  resource_bit 0x%x\n",
		   lock_status, resource_bit);
		return -EFAULT;
	}

	REG_WR(bp, hw_lock_control_reg, resource_bit);
	return 0;
}

/* HW Lock for shared dual port PHYs */
static void bnx2x_acquire_phy_lock(struct bnx2x *bp)
{
	mutex_lock(&bp->port.phy_mutex);

	if (bp->port.need_hw_lock)
		bnx2x_acquire_hw_lock(bp, HW_LOCK_RESOURCE_MDIO);
}

static void bnx2x_release_phy_lock(struct bnx2x *bp)
{
	if (bp->port.need_hw_lock)
		bnx2x_release_hw_lock(bp, HW_LOCK_RESOURCE_MDIO);

	mutex_unlock(&bp->port.phy_mutex);
}

int bnx2x_get_gpio(struct bnx2x *bp, int gpio_num, u8 port)
{
	/* The GPIO should be swapped if swap register is set and active */
	int gpio_port = (REG_RD(bp, NIG_REG_PORT_SWAP) &&
			 REG_RD(bp, NIG_REG_STRAP_OVERRIDE)) ^ port;
	int gpio_shift = gpio_num +
			(gpio_port ? MISC_REGISTERS_GPIO_PORT_SHIFT : 0);
	u32 gpio_mask = (1 << gpio_shift);
	u32 gpio_reg;
	int value;

	if (gpio_num > MISC_REGISTERS_GPIO_3) {
		BNX2X_ERR("Invalid GPIO %d\n", gpio_num);
		return -EINVAL;
	}

	/* read GPIO value */
	gpio_reg = REG_RD(bp, MISC_REG_GPIO);

	/* get the requested pin value */
	if ((gpio_reg & gpio_mask) == gpio_mask)
		value = 1;
	else
		value = 0;

	DP(NETIF_MSG_LINK, "pin %d  value 0x%x\n", gpio_num, value);

	return value;
}

int bnx2x_set_gpio(struct bnx2x *bp, int gpio_num, u32 mode, u8 port)
{
	/* The GPIO should be swapped if swap register is set and active */
	int gpio_port = (REG_RD(bp, NIG_REG_PORT_SWAP) &&
			 REG_RD(bp, NIG_REG_STRAP_OVERRIDE)) ^ port;
	int gpio_shift = gpio_num +
			(gpio_port ? MISC_REGISTERS_GPIO_PORT_SHIFT : 0);
	u32 gpio_mask = (1 << gpio_shift);
	u32 gpio_reg;

	if (gpio_num > MISC_REGISTERS_GPIO_3) {
		BNX2X_ERR("Invalid GPIO %d\n", gpio_num);
		return -EINVAL;
	}

	bnx2x_acquire_hw_lock(bp, HW_LOCK_RESOURCE_GPIO);
	/* read GPIO and mask except the float bits */
	gpio_reg = (REG_RD(bp, MISC_REG_GPIO) & MISC_REGISTERS_GPIO_FLOAT);

	switch (mode) {
	case MISC_REGISTERS_GPIO_OUTPUT_LOW:
		DP(NETIF_MSG_LINK, "Set GPIO %d (shift %d) -> output low\n",
		   gpio_num, gpio_shift);
		/* clear FLOAT and set CLR */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_FLOAT_POS);
		gpio_reg |=  (gpio_mask << MISC_REGISTERS_GPIO_CLR_POS);
		break;

	case MISC_REGISTERS_GPIO_OUTPUT_HIGH:
		DP(NETIF_MSG_LINK, "Set GPIO %d (shift %d) -> output high\n",
		   gpio_num, gpio_shift);
		/* clear FLOAT and set SET */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_FLOAT_POS);
		gpio_reg |=  (gpio_mask << MISC_REGISTERS_GPIO_SET_POS);
		break;

	case MISC_REGISTERS_GPIO_INPUT_HI_Z:
		DP(NETIF_MSG_LINK, "Set GPIO %d (shift %d) -> input\n",
		   gpio_num, gpio_shift);
		/* set FLOAT */
		gpio_reg |= (gpio_mask << MISC_REGISTERS_GPIO_FLOAT_POS);
		break;

	default:
		break;
	}

	REG_WR(bp, MISC_REG_GPIO, gpio_reg);
	bnx2x_release_hw_lock(bp, HW_LOCK_RESOURCE_GPIO);

	return 0;
}

int bnx2x_set_gpio_int(struct bnx2x *bp, int gpio_num, u32 mode, u8 port)
{
	/* The GPIO should be swapped if swap register is set and active */
	int gpio_port = (REG_RD(bp, NIG_REG_PORT_SWAP) &&
			 REG_RD(bp, NIG_REG_STRAP_OVERRIDE)) ^ port;
	int gpio_shift = gpio_num +
			(gpio_port ? MISC_REGISTERS_GPIO_PORT_SHIFT : 0);
	u32 gpio_mask = (1 << gpio_shift);
	u32 gpio_reg;

	if (gpio_num > MISC_REGISTERS_GPIO_3) {
		BNX2X_ERR("Invalid GPIO %d\n", gpio_num);
		return -EINVAL;
	}

	bnx2x_acquire_hw_lock(bp, HW_LOCK_RESOURCE_GPIO);
	/* read GPIO int */
	gpio_reg = REG_RD(bp, MISC_REG_GPIO_INT);

	switch (mode) {
	case MISC_REGISTERS_GPIO_INT_OUTPUT_CLR:
		DP(NETIF_MSG_LINK, "Clear GPIO INT %d (shift %d) -> "
				   "output low\n", gpio_num, gpio_shift);
		/* clear SET and set CLR */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_INT_SET_POS);
		gpio_reg |=  (gpio_mask << MISC_REGISTERS_GPIO_INT_CLR_POS);
		break;

	case MISC_REGISTERS_GPIO_INT_OUTPUT_SET:
		DP(NETIF_MSG_LINK, "Set GPIO INT %d (shift %d) -> "
				   "output high\n", gpio_num, gpio_shift);
		/* clear CLR and set SET */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_INT_CLR_POS);
		gpio_reg |=  (gpio_mask << MISC_REGISTERS_GPIO_INT_SET_POS);
		break;

	default:
		break;
	}

	REG_WR(bp, MISC_REG_GPIO_INT, gpio_reg);
	bnx2x_release_hw_lock(bp, HW_LOCK_RESOURCE_GPIO);

	return 0;
}

static int bnx2x_set_spio(struct bnx2x *bp, int spio_num, u32 mode)
{
	u32 spio_mask = (1 << spio_num);
	u32 spio_reg;

	if ((spio_num < MISC_REGISTERS_SPIO_4) ||
	    (spio_num > MISC_REGISTERS_SPIO_7)) {
		BNX2X_ERR("Invalid SPIO %d\n", spio_num);
		return -EINVAL;
	}

	bnx2x_acquire_hw_lock(bp, HW_LOCK_RESOURCE_SPIO);
	/* read SPIO and mask except the float bits */
	spio_reg = (REG_RD(bp, MISC_REG_SPIO) & MISC_REGISTERS_SPIO_FLOAT);

	switch (mode) {
	case MISC_REGISTERS_SPIO_OUTPUT_LOW:
		DP(NETIF_MSG_LINK, "Set SPIO %d -> output low\n", spio_num);
		/* clear FLOAT and set CLR */
		spio_reg &= ~(spio_mask << MISC_REGISTERS_SPIO_FLOAT_POS);
		spio_reg |=  (spio_mask << MISC_REGISTERS_SPIO_CLR_POS);
		break;

	case MISC_REGISTERS_SPIO_OUTPUT_HIGH:
		DP(NETIF_MSG_LINK, "Set SPIO %d -> output high\n", spio_num);
		/* clear FLOAT and set SET */
		spio_reg &= ~(spio_mask << MISC_REGISTERS_SPIO_FLOAT_POS);
		spio_reg |=  (spio_mask << MISC_REGISTERS_SPIO_SET_POS);
		break;

	case MISC_REGISTERS_SPIO_INPUT_HI_Z:
		DP(NETIF_MSG_LINK, "Set SPIO %d -> input\n", spio_num);
		/* set FLOAT */
		spio_reg |= (spio_mask << MISC_REGISTERS_SPIO_FLOAT_POS);
		break;

	default:
		break;
	}

	REG_WR(bp, MISC_REG_SPIO, spio_reg);
	bnx2x_release_hw_lock(bp, HW_LOCK_RESOURCE_SPIO);

	return 0;
}

static void bnx2x_calc_fc_adv(struct bnx2x *bp)
{
	switch (bp->link_vars.ieee_fc &
		MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_MASK) {
	case MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_NONE:
		bp->port.advertising &= ~(ADVERTISED_Asym_Pause |
					  ADVERTISED_Pause);
		break;

	case MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH:
		bp->port.advertising |= (ADVERTISED_Asym_Pause |
					 ADVERTISED_Pause);
		break;

	case MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC:
		bp->port.advertising |= ADVERTISED_Asym_Pause;
		break;

	default:
		bp->port.advertising &= ~(ADVERTISED_Asym_Pause |
					  ADVERTISED_Pause);
		break;
	}
}

static void bnx2x_link_report(struct bnx2x *bp)
{
	if (bp->state == BNX2X_STATE_DISABLED) {
		netif_carrier_off(bp->dev);
		printk(KERN_ERR PFX "%s NIC Link is Down\n", bp->dev->name);
		return;
	}

	if (bp->link_vars.link_up) {
		if (bp->state == BNX2X_STATE_OPEN)
			netif_carrier_on(bp->dev);
		printk(KERN_INFO PFX "%s NIC Link is Up, ", bp->dev->name);

		printk("%d Mbps ", bp->link_vars.line_speed);

		if (bp->link_vars.duplex == DUPLEX_FULL)
			printk("full duplex");
		else
			printk("half duplex");

		if (bp->link_vars.flow_ctrl != BNX2X_FLOW_CTRL_NONE) {
			if (bp->link_vars.flow_ctrl & BNX2X_FLOW_CTRL_RX) {
				printk(", receive ");
				if (bp->link_vars.flow_ctrl &
				    BNX2X_FLOW_CTRL_TX)
					printk("& transmit ");
			} else {
				printk(", transmit ");
			}
			printk("flow control ON");
		}
		printk("\n");

	} else { /* link_down */
		netif_carrier_off(bp->dev);
		printk(KERN_ERR PFX "%s NIC Link is Down\n", bp->dev->name);
	}
}

static u8 bnx2x_initial_phy_init(struct bnx2x *bp, int load_mode)
{
	if (!BP_NOMCP(bp)) {
		u8 rc;

		/* Initialize link parameters structure variables */
		/* It is recommended to turn off RX FC for jumbo frames
		   for better performance */
		if (bp->dev->mtu > 5000)
			bp->link_params.req_fc_auto_adv = BNX2X_FLOW_CTRL_TX;
		else
			bp->link_params.req_fc_auto_adv = BNX2X_FLOW_CTRL_BOTH;

		bnx2x_acquire_phy_lock(bp);

		if (load_mode == LOAD_DIAG)
			bp->link_params.loopback_mode = LOOPBACK_XGXS_10;

		rc = bnx2x_phy_init(&bp->link_params, &bp->link_vars);

		bnx2x_release_phy_lock(bp);

		bnx2x_calc_fc_adv(bp);

		if (CHIP_REV_IS_SLOW(bp) && bp->link_vars.link_up) {
			bnx2x_stats_handle(bp, STATS_EVENT_LINK_UP);
			bnx2x_link_report(bp);
		}

		return rc;
	}
	BNX2X_ERR("Bootcode is missing - can not initialize link\n");
	return -EINVAL;
}

static void bnx2x_link_set(struct bnx2x *bp)
{
	if (!BP_NOMCP(bp)) {
		bnx2x_acquire_phy_lock(bp);
		bnx2x_phy_init(&bp->link_params, &bp->link_vars);
		bnx2x_release_phy_lock(bp);

		bnx2x_calc_fc_adv(bp);
	} else
		BNX2X_ERR("Bootcode is missing - can not set link\n");
}

static void bnx2x__link_reset(struct bnx2x *bp)
{
	if (!BP_NOMCP(bp)) {
		bnx2x_acquire_phy_lock(bp);
		bnx2x_link_reset(&bp->link_params, &bp->link_vars, 1);
		bnx2x_release_phy_lock(bp);
	} else
		BNX2X_ERR("Bootcode is missing - can not reset link\n");
}

static u8 bnx2x_link_test(struct bnx2x *bp)
{
	u8 rc;

	bnx2x_acquire_phy_lock(bp);
	rc = bnx2x_test_link(&bp->link_params, &bp->link_vars);
	bnx2x_release_phy_lock(bp);

	return rc;
}

static void bnx2x_init_port_minmax(struct bnx2x *bp)
{
	u32 r_param = bp->link_vars.line_speed / 8;
	u32 fair_periodic_timeout_usec;
	u32 t_fair;

	memset(&(bp->cmng.rs_vars), 0,
	       sizeof(struct rate_shaping_vars_per_port));
	memset(&(bp->cmng.fair_vars), 0, sizeof(struct fairness_vars_per_port));

	/* 100 usec in SDM ticks = 25 since each tick is 4 usec */
	bp->cmng.rs_vars.rs_periodic_timeout = RS_PERIODIC_TIMEOUT_USEC / 4;

	/* this is the threshold below which no timer arming will occur
	   1.25 coefficient is for the threshold to be a little bigger
	   than the real time, to compensate for timer in-accuracy */
	bp->cmng.rs_vars.rs_threshold =
				(RS_PERIODIC_TIMEOUT_USEC * r_param * 5) / 4;

	/* resolution of fairness timer */
	fair_periodic_timeout_usec = QM_ARB_BYTES / r_param;
	/* for 10G it is 1000usec. for 1G it is 10000usec. */
	t_fair = T_FAIR_COEF / bp->link_vars.line_speed;

	/* this is the threshold below which we won't arm the timer anymore */
	bp->cmng.fair_vars.fair_threshold = QM_ARB_BYTES;

	/* we multiply by 1e3/8 to get bytes/msec.
	   We don't want the credits to pass a credit
	   of the t_fair*FAIR_MEM (algorithm resolution) */
	bp->cmng.fair_vars.upper_bound = r_param * t_fair * FAIR_MEM;
	/* since each tick is 4 usec */
	bp->cmng.fair_vars.fairness_timeout = fair_periodic_timeout_usec / 4;
}

/* Calculates the sum of vn_min_rates.
   It's needed for further normalizing of the min_rates.
   Returns:
     sum of vn_min_rates.
       or
     0 - if all the min_rates are 0.
     In the later case fainess algorithm should be deactivated.
     If not all min_rates are zero then those that are zeroes will be set to 1.
 */
static void bnx2x_calc_vn_weight_sum(struct bnx2x *bp)
{
	int all_zero = 1;
	int port = BP_PORT(bp);
	int vn;

	bp->vn_weight_sum = 0;
	for (vn = VN_0; vn < E1HVN_MAX; vn++) {
		int func = 2*vn + port;
		u32 vn_cfg = SHMEM_RD(bp, mf_cfg.func_mf_config[func].config);
		u32 vn_min_rate = ((vn_cfg & FUNC_MF_CFG_MIN_BW_MASK) >>
				   FUNC_MF_CFG_MIN_BW_SHIFT) * 100;

		/* Skip hidden vns */
		if (vn_cfg & FUNC_MF_CFG_FUNC_HIDE)
			continue;

		/* If min rate is zero - set it to 1 */
		if (!vn_min_rate)
			vn_min_rate = DEF_MIN_RATE;
		else
			all_zero = 0;

		bp->vn_weight_sum += vn_min_rate;
	}

	/* ... only if all min rates are zeros - disable fairness */
	if (all_zero)
		bp->vn_weight_sum = 0;
}

static void bnx2x_init_vn_minmax(struct bnx2x *bp, int func)
{
	struct rate_shaping_vars_per_vn m_rs_vn;
	struct fairness_vars_per_vn m_fair_vn;
	u32 vn_cfg = SHMEM_RD(bp, mf_cfg.func_mf_config[func].config);
	u16 vn_min_rate, vn_max_rate;
	int i;

	/* If function is hidden - set min and max to zeroes */
	if (vn_cfg & FUNC_MF_CFG_FUNC_HIDE) {
		vn_min_rate = 0;
		vn_max_rate = 0;

	} else {
		vn_min_rate = ((vn_cfg & FUNC_MF_CFG_MIN_BW_MASK) >>
				FUNC_MF_CFG_MIN_BW_SHIFT) * 100;
		/* If fairness is enabled (not all min rates are zeroes) and
		   if current min rate is zero - set it to 1.
		   This is a requirement of the algorithm. */
		if (bp->vn_weight_sum && (vn_min_rate == 0))
			vn_min_rate = DEF_MIN_RATE;
		vn_max_rate = ((vn_cfg & FUNC_MF_CFG_MAX_BW_MASK) >>
				FUNC_MF_CFG_MAX_BW_SHIFT) * 100;
	}

	DP(NETIF_MSG_IFUP,
	   "func %d: vn_min_rate=%d  vn_max_rate=%d  vn_weight_sum=%d\n",
	   func, vn_min_rate, vn_max_rate, bp->vn_weight_sum);

	memset(&m_rs_vn, 0, sizeof(struct rate_shaping_vars_per_vn));
	memset(&m_fair_vn, 0, sizeof(struct fairness_vars_per_vn));

	/* global vn counter - maximal Mbps for this vn */
	m_rs_vn.vn_counter.rate = vn_max_rate;

	/* quota - number of bytes transmitted in this period */
	m_rs_vn.vn_counter.quota =
				(vn_max_rate * RS_PERIODIC_TIMEOUT_USEC) / 8;

	if (bp->vn_weight_sum) {
		/* credit for each period of the fairness algorithm:
		   number of bytes in T_FAIR (the vn share the port rate).
		   vn_weight_sum should not be larger than 10000, thus
		   T_FAIR_COEF / (8 * vn_weight_sum) will always be greater
		   than zero */
		m_fair_vn.vn_credit_delta =
			max((u32)(vn_min_rate * (T_FAIR_COEF /
						 (8 * bp->vn_weight_sum))),
			    (u32)(bp->cmng.fair_vars.fair_threshold * 2));
		DP(NETIF_MSG_IFUP, "m_fair_vn.vn_credit_delta=%d\n",
		   m_fair_vn.vn_credit_delta);
	}

	/* Store it to internal memory */
	for (i = 0; i < sizeof(struct rate_shaping_vars_per_vn)/4; i++)
		REG_WR(bp, BAR_XSTRORM_INTMEM +
		       XSTORM_RATE_SHAPING_PER_VN_VARS_OFFSET(func) + i * 4,
		       ((u32 *)(&m_rs_vn))[i]);

	for (i = 0; i < sizeof(struct fairness_vars_per_vn)/4; i++)
		REG_WR(bp, BAR_XSTRORM_INTMEM +
		       XSTORM_FAIRNESS_PER_VN_VARS_OFFSET(func) + i * 4,
		       ((u32 *)(&m_fair_vn))[i]);
}


/* This function is called upon link interrupt */
static void bnx2x_link_attn(struct bnx2x *bp)
{
	/* Make sure that we are synced with the current statistics */
	bnx2x_stats_handle(bp, STATS_EVENT_STOP);

	bnx2x_link_update(&bp->link_params, &bp->link_vars);

	if (bp->link_vars.link_up) {

		/* dropless flow control */
		if (CHIP_IS_E1H(bp) && bp->dropless_fc) {
			int port = BP_PORT(bp);
			u32 pause_enabled = 0;

			if (bp->link_vars.flow_ctrl & BNX2X_FLOW_CTRL_TX)
				pause_enabled = 1;

			REG_WR(bp, BAR_USTRORM_INTMEM +
			       USTORM_ETH_PAUSE_ENABLED_OFFSET(port),
			       pause_enabled);
		}

		if (bp->link_vars.mac_type == MAC_TYPE_BMAC) {
			struct host_port_stats *pstats;

			pstats = bnx2x_sp(bp, port_stats);
			/* reset old bmac stats */
			memset(&(pstats->mac_stx[0]), 0,
			       sizeof(struct mac_stx));
		}
		if ((bp->state == BNX2X_STATE_OPEN) ||
		    (bp->state == BNX2X_STATE_DISABLED))
			bnx2x_stats_handle(bp, STATS_EVENT_LINK_UP);
	}

	/* indicate link status */
	bnx2x_link_report(bp);

	if (IS_E1HMF(bp)) {
		int port = BP_PORT(bp);
		int func;
		int vn;

		/* Set the attention towards other drivers on the same port */
		for (vn = VN_0; vn < E1HVN_MAX; vn++) {
			if (vn == BP_E1HVN(bp))
				continue;

			func = ((vn << 1) | port);
			REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_0 +
			       (LINK_SYNC_ATTENTION_BIT_FUNC_0 + func)*4, 1);
		}

		if (bp->link_vars.link_up) {
			int i;

			/* Init rate shaping and fairness contexts */
			bnx2x_init_port_minmax(bp);

			for (vn = VN_0; vn < E1HVN_MAX; vn++)
				bnx2x_init_vn_minmax(bp, 2*vn + port);

			/* Store it to internal memory */
			for (i = 0;
			     i < sizeof(struct cmng_struct_per_port) / 4; i++)
				REG_WR(bp, BAR_XSTRORM_INTMEM +
				  XSTORM_CMNG_PER_PORT_VARS_OFFSET(port) + i*4,
				       ((u32 *)(&bp->cmng))[i]);
		}
	}
}

static void bnx2x__link_status_update(struct bnx2x *bp)
{
	int func = BP_FUNC(bp);

	if (bp->state != BNX2X_STATE_OPEN)
		return;

	bnx2x_link_status_update(&bp->link_params, &bp->link_vars);

	if (bp->link_vars.link_up)
		bnx2x_stats_handle(bp, STATS_EVENT_LINK_UP);
	else
		bnx2x_stats_handle(bp, STATS_EVENT_STOP);

	bp->mf_config = SHMEM_RD(bp, mf_cfg.func_mf_config[func].config);
	bnx2x_calc_vn_weight_sum(bp);

	/* indicate link status */
	bnx2x_link_report(bp);
}

static void bnx2x_pmf_update(struct bnx2x *bp)
{
	int port = BP_PORT(bp);
	u32 val;

	bp->port.pmf = 1;
	DP(NETIF_MSG_LINK, "pmf %d\n", bp->port.pmf);

	/* enable nig attention */
	val = (0xff0f | (1 << (BP_E1HVN(bp) + 4)));
	REG_WR(bp, HC_REG_TRAILING_EDGE_0 + port*8, val);
	REG_WR(bp, HC_REG_LEADING_EDGE_0 + port*8, val);

	bnx2x_stats_handle(bp, STATS_EVENT_PMF);
}

/* end of Link */

/* slow path */

/*
 * General service functions
 */

/* send the MCP a request, block until there is a reply */
u32 bnx2x_fw_command(struct bnx2x *bp, u32 command)
{
	int func = BP_FUNC(bp);
	u32 seq = ++bp->fw_seq;
	u32 rc = 0;
	u32 cnt = 1;
	u8 delay = CHIP_REV_IS_SLOW(bp) ? 100 : 10;

	SHMEM_WR(bp, func_mb[func].drv_mb_header, (command | seq));
	DP(BNX2X_MSG_MCP, "wrote command (%x) to FW MB\n", (command | seq));

	do {
		/* let the FW do it's magic ... */
		msleep(delay);

		rc = SHMEM_RD(bp, func_mb[func].fw_mb_header);

		/* Give the FW up to 2 second (200*10ms) */
	} while ((seq != (rc & FW_MSG_SEQ_NUMBER_MASK)) && (cnt++ < 200));

	DP(BNX2X_MSG_MCP, "[after %d ms] read (%x) seq is (%x) from FW MB\n",
	   cnt*delay, rc, seq);

	/* is this a reply to our command? */
	if (seq == (rc & FW_MSG_SEQ_NUMBER_MASK))
		rc &= FW_MSG_CODE_MASK;
	else {
		/* FW BUG! */
		BNX2X_ERR("FW failed to respond!\n");
		bnx2x_fw_dump(bp);
		rc = 0;
	}

	return rc;
}

static void bnx2x_set_storm_rx_mode(struct bnx2x *bp);
static void bnx2x_set_mac_addr_e1h(struct bnx2x *bp, int set);
static void bnx2x_set_rx_mode(struct net_device *dev);

static void bnx2x_e1h_disable(struct bnx2x *bp)
{
	int port = BP_PORT(bp);
	int i;

	bp->rx_mode = BNX2X_RX_MODE_NONE;
	bnx2x_set_storm_rx_mode(bp);

	netif_tx_disable(bp->dev);
	bp->dev->trans_start = jiffies;	/* prevent tx timeout */

	REG_WR(bp, NIG_REG_LLH0_FUNC_EN + port*8, 0);

	bnx2x_set_mac_addr_e1h(bp, 0);

	for (i = 0; i < MC_HASH_SIZE; i++)
		REG_WR(bp, MC_HASH_OFFSET(bp, i), 0);

	netif_carrier_off(bp->dev);
}

static void bnx2x_e1h_enable(struct bnx2x *bp)
{
	int port = BP_PORT(bp);

	REG_WR(bp, NIG_REG_LLH0_FUNC_EN + port*8, 1);

	bnx2x_set_mac_addr_e1h(bp, 1);

	/* Tx queue should be only reenabled */
	netif_tx_wake_all_queues(bp->dev);

	/* Initialize the receive filter. */
	bnx2x_set_rx_mode(bp->dev);
}

static void bnx2x_update_min_max(struct bnx2x *bp)
{
	int port = BP_PORT(bp);
	int vn, i;

	/* Init rate shaping and fairness contexts */
	bnx2x_init_port_minmax(bp);

	bnx2x_calc_vn_weight_sum(bp);

	for (vn = VN_0; vn < E1HVN_MAX; vn++)
		bnx2x_init_vn_minmax(bp, 2*vn + port);

	if (bp->port.pmf) {
		int func;

		/* Set the attention towards other drivers on the same port */
		for (vn = VN_0; vn < E1HVN_MAX; vn++) {
			if (vn == BP_E1HVN(bp))
				continue;

			func = ((vn << 1) | port);
			REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_0 +
			       (LINK_SYNC_ATTENTION_BIT_FUNC_0 + func)*4, 1);
		}

		/* Store it to internal memory */
		for (i = 0; i < sizeof(struct cmng_struct_per_port) / 4; i++)
			REG_WR(bp, BAR_XSTRORM_INTMEM +
			       XSTORM_CMNG_PER_PORT_VARS_OFFSET(port) + i*4,
			       ((u32 *)(&bp->cmng))[i]);
	}
}

static void bnx2x_dcc_event(struct bnx2x *bp, u32 dcc_event)
{
	int func = BP_FUNC(bp);

	DP(BNX2X_MSG_MCP, "dcc_event 0x%x\n", dcc_event);
	bp->mf_config = SHMEM_RD(bp, mf_cfg.func_mf_config[func].config);

	if (dcc_event & DRV_STATUS_DCC_DISABLE_ENABLE_PF) {

		if (bp->mf_config & FUNC_MF_CFG_FUNC_DISABLED) {
			DP(NETIF_MSG_IFDOWN, "mf_cfg function disabled\n");
			bp->state = BNX2X_STATE_DISABLED;

			bnx2x_e1h_disable(bp);
		} else {
			DP(NETIF_MSG_IFUP, "mf_cfg function enabled\n");
			bp->state = BNX2X_STATE_OPEN;

			bnx2x_e1h_enable(bp);
		}
		dcc_event &= ~DRV_STATUS_DCC_DISABLE_ENABLE_PF;
	}
	if (dcc_event & DRV_STATUS_DCC_BANDWIDTH_ALLOCATION) {

		bnx2x_update_min_max(bp);
		dcc_event &= ~DRV_STATUS_DCC_BANDWIDTH_ALLOCATION;
	}

	/* Report results to MCP */
	if (dcc_event)
		bnx2x_fw_command(bp, DRV_MSG_CODE_DCC_FAILURE);
	else
		bnx2x_fw_command(bp, DRV_MSG_CODE_DCC_OK);
}

/* the slow path queue is odd since completions arrive on the fastpath ring */
static int bnx2x_sp_post(struct bnx2x *bp, int command, int cid,
			 u32 data_hi, u32 data_lo, int common)
{
	int func = BP_FUNC(bp);

	DP(BNX2X_MSG_SP/*NETIF_MSG_TIMER*/,
	   "SPQE (%x:%x)  command %d  hw_cid %x  data (%x:%x)  left %x\n",
	   (u32)U64_HI(bp->spq_mapping), (u32)(U64_LO(bp->spq_mapping) +
	   (void *)bp->spq_prod_bd - (void *)bp->spq), command,
	   HW_CID(bp, cid), data_hi, data_lo, bp->spq_left);

#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		return -EIO;
#endif

	spin_lock_bh(&bp->spq_lock);

	if (!bp->spq_left) {
		BNX2X_ERR("BUG! SPQ ring full!\n");
		spin_unlock_bh(&bp->spq_lock);
		bnx2x_panic();
		return -EBUSY;
	}

	/* CID needs port number to be encoded int it */
	bp->spq_prod_bd->hdr.conn_and_cmd_data =
			cpu_to_le32(((command << SPE_HDR_CMD_ID_SHIFT) |
				     HW_CID(bp, cid)));
	bp->spq_prod_bd->hdr.type = cpu_to_le16(ETH_CONNECTION_TYPE);
	if (common)
		bp->spq_prod_bd->hdr.type |=
			cpu_to_le16((1 << SPE_HDR_COMMON_RAMROD_SHIFT));

	bp->spq_prod_bd->data.mac_config_addr.hi = cpu_to_le32(data_hi);
	bp->spq_prod_bd->data.mac_config_addr.lo = cpu_to_le32(data_lo);

	bp->spq_left--;

	if (bp->spq_prod_bd == bp->spq_last_bd) {
		bp->spq_prod_bd = bp->spq;
		bp->spq_prod_idx = 0;
		DP(NETIF_MSG_TIMER, "end of spq\n");

	} else {
		bp->spq_prod_bd++;
		bp->spq_prod_idx++;
	}

	/* Make sure that BD data is updated before writing the producer */
	wmb();

	REG_WR(bp, BAR_XSTRORM_INTMEM + XSTORM_SPQ_PROD_OFFSET(func),
	       bp->spq_prod_idx);

	mmiowb();

	spin_unlock_bh(&bp->spq_lock);
	return 0;
}

/* acquire split MCP access lock register */
static int bnx2x_acquire_alr(struct bnx2x *bp)
{
	u32 i, j, val;
	int rc = 0;

	might_sleep();
	i = 100;
	for (j = 0; j < i*10; j++) {
		val = (1UL << 31);
		REG_WR(bp, GRCBASE_MCP + 0x9c, val);
		val = REG_RD(bp, GRCBASE_MCP + 0x9c);
		if (val & (1L << 31))
			break;

		msleep(5);
	}
	if (!(val & (1L << 31))) {
		BNX2X_ERR("Cannot acquire MCP access lock register\n");
		rc = -EBUSY;
	}

	return rc;
}

/* release split MCP access lock register */
static void bnx2x_release_alr(struct bnx2x *bp)
{
	u32 val = 0;

	REG_WR(bp, GRCBASE_MCP + 0x9c, val);
}

static inline u16 bnx2x_update_dsb_idx(struct bnx2x *bp)
{
	struct host_def_status_block *def_sb = bp->def_status_blk;
	u16 rc = 0;

	barrier(); /* status block is written to by the chip */
	if (bp->def_att_idx != def_sb->atten_status_block.attn_bits_index) {
		bp->def_att_idx = def_sb->atten_status_block.attn_bits_index;
		rc |= 1;
	}
	if (bp->def_c_idx != def_sb->c_def_status_block.status_block_index) {
		bp->def_c_idx = def_sb->c_def_status_block.status_block_index;
		rc |= 2;
	}
	if (bp->def_u_idx != def_sb->u_def_status_block.status_block_index) {
		bp->def_u_idx = def_sb->u_def_status_block.status_block_index;
		rc |= 4;
	}
	if (bp->def_x_idx != def_sb->x_def_status_block.status_block_index) {
		bp->def_x_idx = def_sb->x_def_status_block.status_block_index;
		rc |= 8;
	}
	if (bp->def_t_idx != def_sb->t_def_status_block.status_block_index) {
		bp->def_t_idx = def_sb->t_def_status_block.status_block_index;
		rc |= 16;
	}
	return rc;
}

/*
 * slow path service functions
 */

static void bnx2x_attn_int_asserted(struct bnx2x *bp, u32 asserted)
{
	int port = BP_PORT(bp);
	u32 hc_addr = (HC_REG_COMMAND_REG + port*32 +
		       COMMAND_REG_ATTN_BITS_SET);
	u32 aeu_addr = port ? MISC_REG_AEU_MASK_ATTN_FUNC_1 :
			      MISC_REG_AEU_MASK_ATTN_FUNC_0;
	u32 nig_int_mask_addr = port ? NIG_REG_MASK_INTERRUPT_PORT1 :
				       NIG_REG_MASK_INTERRUPT_PORT0;
	u32 aeu_mask;
	u32 nig_mask = 0;

	if (bp->attn_state & asserted)
		BNX2X_ERR("IGU ERROR\n");

	bnx2x_acquire_hw_lock(bp, HW_LOCK_RESOURCE_PORT0_ATT_MASK + port);
	aeu_mask = REG_RD(bp, aeu_addr);

	DP(NETIF_MSG_HW, "aeu_mask %x  newly asserted %x\n",
	   aeu_mask, asserted);
	aeu_mask &= ~(asserted & 0xff);
	DP(NETIF_MSG_HW, "new mask %x\n", aeu_mask);

	REG_WR(bp, aeu_addr, aeu_mask);
	bnx2x_release_hw_lock(bp, HW_LOCK_RESOURCE_PORT0_ATT_MASK + port);

	DP(NETIF_MSG_HW, "attn_state %x\n", bp->attn_state);
	bp->attn_state |= asserted;
	DP(NETIF_MSG_HW, "new state %x\n", bp->attn_state);

	if (asserted & ATTN_HARD_WIRED_MASK) {
		if (asserted & ATTN_NIG_FOR_FUNC) {

			bnx2x_acquire_phy_lock(bp);

			/* save nig interrupt mask */
			nig_mask = REG_RD(bp, nig_int_mask_addr);
			REG_WR(bp, nig_int_mask_addr, 0);

			bnx2x_link_attn(bp);

			/* handle unicore attn? */
		}
		if (asserted & ATTN_SW_TIMER_4_FUNC)
			DP(NETIF_MSG_HW, "ATTN_SW_TIMER_4_FUNC!\n");

		if (asserted & GPIO_2_FUNC)
			DP(NETIF_MSG_HW, "GPIO_2_FUNC!\n");

		if (asserted & GPIO_3_FUNC)
			DP(NETIF_MSG_HW, "GPIO_3_FUNC!\n");

		if (asserted & GPIO_4_FUNC)
			DP(NETIF_MSG_HW, "GPIO_4_FUNC!\n");

		if (port == 0) {
			if (asserted & ATTN_GENERAL_ATTN_1) {
				DP(NETIF_MSG_HW, "ATTN_GENERAL_ATTN_1!\n");
				REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_1, 0x0);
			}
			if (asserted & ATTN_GENERAL_ATTN_2) {
				DP(NETIF_MSG_HW, "ATTN_GENERAL_ATTN_2!\n");
				REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_2, 0x0);
			}
			if (asserted & ATTN_GENERAL_ATTN_3) {
				DP(NETIF_MSG_HW, "ATTN_GENERAL_ATTN_3!\n");
				REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_3, 0x0);
			}
		} else {
			if (asserted & ATTN_GENERAL_ATTN_4) {
				DP(NETIF_MSG_HW, "ATTN_GENERAL_ATTN_4!\n");
				REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_4, 0x0);
			}
			if (asserted & ATTN_GENERAL_ATTN_5) {
				DP(NETIF_MSG_HW, "ATTN_GENERAL_ATTN_5!\n");
				REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_5, 0x0);
			}
			if (asserted & ATTN_GENERAL_ATTN_6) {
				DP(NETIF_MSG_HW, "ATTN_GENERAL_ATTN_6!\n");
				REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_6, 0x0);
			}
		}

	} /* if hardwired */

	DP(NETIF_MSG_HW, "about to mask 0x%08x at HC addr 0x%x\n",
	   asserted, hc_addr);
	REG_WR(bp, hc_addr, asserted);

	/* now set back the mask */
	if (asserted & ATTN_NIG_FOR_FUNC) {
		REG_WR(bp, nig_int_mask_addr, nig_mask);
		bnx2x_release_phy_lock(bp);
	}
}

static inline void bnx2x_fan_failure(struct bnx2x *bp)
{
	int port = BP_PORT(bp);

	/* mark the failure */
	bp->link_params.ext_phy_config &= ~PORT_HW_CFG_XGXS_EXT_PHY_TYPE_MASK;
	bp->link_params.ext_phy_config |= PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE;
	SHMEM_WR(bp, dev_info.port_hw_config[port].external_phy_config,
		 bp->link_params.ext_phy_config);

	/* log the failure */
	printk(KERN_ERR PFX "Fan Failure on Network Controller %s has caused"
	       " the driver to shutdown the card to prevent permanent"
	       " damage.  Please contact Dell Support for assistance\n",
	       bp->dev->name);
}

static inline void bnx2x_attn_int_deasserted0(struct bnx2x *bp, u32 attn)
{
	int port = BP_PORT(bp);
	int reg_offset;
	u32 val, swap_val, swap_override;

	reg_offset = (port ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_0 :
			     MISC_REG_AEU_ENABLE1_FUNC_0_OUT_0);

	if (attn & AEU_INPUTS_ATTN_BITS_SPIO5) {

		val = REG_RD(bp, reg_offset);
		val &= ~AEU_INPUTS_ATTN_BITS_SPIO5;
		REG_WR(bp, reg_offset, val);

		BNX2X_ERR("SPIO5 hw attention\n");

		/* Fan failure attention */
		switch (XGXS_EXT_PHY_TYPE(bp->link_params.ext_phy_config)) {
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
			/* Low power mode is controlled by GPIO 2 */
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_2,
				       MISC_REGISTERS_GPIO_OUTPUT_LOW, port);
			/* The PHY reset is controlled by GPIO 1 */
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_1,
				       MISC_REGISTERS_GPIO_OUTPUT_LOW, port);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
			/* The PHY reset is controlled by GPIO 1 */
			/* fake the port number to cancel the swap done in
			   set_gpio() */
			swap_val = REG_RD(bp, NIG_REG_PORT_SWAP);
			swap_override = REG_RD(bp, NIG_REG_STRAP_OVERRIDE);
			port = (swap_val && swap_override) ^ 1;
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_1,
				       MISC_REGISTERS_GPIO_OUTPUT_LOW, port);
			break;

		default:
			break;
		}
		bnx2x_fan_failure(bp);
	}

	if (attn & (AEU_INPUTS_ATTN_BITS_GPIO3_FUNCTION_0 |
		    AEU_INPUTS_ATTN_BITS_GPIO3_FUNCTION_1)) {
		bnx2x_acquire_phy_lock(bp);
		bnx2x_handle_module_detect_int(&bp->link_params);
		bnx2x_release_phy_lock(bp);
	}

	if (attn & HW_INTERRUT_ASSERT_SET_0) {

		val = REG_RD(bp, reg_offset);
		val &= ~(attn & HW_INTERRUT_ASSERT_SET_0);
		REG_WR(bp, reg_offset, val);

		BNX2X_ERR("FATAL HW block attention set0 0x%x\n",
			  (u32)(attn & HW_INTERRUT_ASSERT_SET_0));
		bnx2x_panic();
	}
}

static inline void bnx2x_attn_int_deasserted1(struct bnx2x *bp, u32 attn)
{
	u32 val;

	if (attn & AEU_INPUTS_ATTN_BITS_DOORBELLQ_HW_INTERRUPT) {

		val = REG_RD(bp, DORQ_REG_DORQ_INT_STS_CLR);
		BNX2X_ERR("DB hw attention 0x%x\n", val);
		/* DORQ discard attention */
		if (val & 0x2)
			BNX2X_ERR("FATAL error from DORQ\n");
	}

	if (attn & HW_INTERRUT_ASSERT_SET_1) {

		int port = BP_PORT(bp);
		int reg_offset;

		reg_offset = (port ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_1 :
				     MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1);

		val = REG_RD(bp, reg_offset);
		val &= ~(attn & HW_INTERRUT_ASSERT_SET_1);
		REG_WR(bp, reg_offset, val);

		BNX2X_ERR("FATAL HW block attention set1 0x%x\n",
			  (u32)(attn & HW_INTERRUT_ASSERT_SET_1));
		bnx2x_panic();
	}
}

static inline void bnx2x_attn_int_deasserted2(struct bnx2x *bp, u32 attn)
{
	u32 val;

	if (attn & AEU_INPUTS_ATTN_BITS_CFC_HW_INTERRUPT) {

		val = REG_RD(bp, CFC_REG_CFC_INT_STS_CLR);
		BNX2X_ERR("CFC hw attention 0x%x\n", val);
		/* CFC error attention */
		if (val & 0x2)
			BNX2X_ERR("FATAL error from CFC\n");
	}

	if (attn & AEU_INPUTS_ATTN_BITS_PXP_HW_INTERRUPT) {

		val = REG_RD(bp, PXP_REG_PXP_INT_STS_CLR_0);
		BNX2X_ERR("PXP hw attention 0x%x\n", val);
		/* RQ_USDMDP_FIFO_OVERFLOW */
		if (val & 0x18000)
			BNX2X_ERR("FATAL error from PXP\n");
	}

	if (attn & HW_INTERRUT_ASSERT_SET_2) {

		int port = BP_PORT(bp);
		int reg_offset;

		reg_offset = (port ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_2 :
				     MISC_REG_AEU_ENABLE1_FUNC_0_OUT_2);

		val = REG_RD(bp, reg_offset);
		val &= ~(attn & HW_INTERRUT_ASSERT_SET_2);
		REG_WR(bp, reg_offset, val);

		BNX2X_ERR("FATAL HW block attention set2 0x%x\n",
			  (u32)(attn & HW_INTERRUT_ASSERT_SET_2));
		bnx2x_panic();
	}
}

static inline void bnx2x_attn_int_deasserted3(struct bnx2x *bp, u32 attn)
{
	u32 val;

	if (attn & EVEREST_GEN_ATTN_IN_USE_MASK) {

		if (attn & BNX2X_PMF_LINK_ASSERT) {
			int func = BP_FUNC(bp);

			REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_12 + func*4, 0);
			val = SHMEM_RD(bp, func_mb[func].drv_status);
			if (val & DRV_STATUS_DCC_EVENT_MASK)
				bnx2x_dcc_event(bp,
					    (val & DRV_STATUS_DCC_EVENT_MASK));
			bnx2x__link_status_update(bp);
			if ((bp->port.pmf == 0) && (val & DRV_STATUS_PMF))
				bnx2x_pmf_update(bp);

		} else if (attn & BNX2X_MC_ASSERT_BITS) {

			BNX2X_ERR("MC assert!\n");
			REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_10, 0);
			REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_9, 0);
			REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_8, 0);
			REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_7, 0);
			bnx2x_panic();

		} else if (attn & BNX2X_MCP_ASSERT) {

			BNX2X_ERR("MCP assert!\n");
			REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_11, 0);
			bnx2x_fw_dump(bp);

		} else
			BNX2X_ERR("Unknown HW assert! (attn 0x%x)\n", attn);
	}

	if (attn & EVEREST_LATCHED_ATTN_IN_USE_MASK) {
		BNX2X_ERR("LATCHED attention 0x%08x (masked)\n", attn);
		if (attn & BNX2X_GRC_TIMEOUT) {
			val = CHIP_IS_E1H(bp) ?
				REG_RD(bp, MISC_REG_GRC_TIMEOUT_ATTN) : 0;
			BNX2X_ERR("GRC time-out 0x%08x\n", val);
		}
		if (attn & BNX2X_GRC_RSV) {
			val = CHIP_IS_E1H(bp) ?
				REG_RD(bp, MISC_REG_GRC_RSV_ATTN) : 0;
			BNX2X_ERR("GRC reserved 0x%08x\n", val);
		}
		REG_WR(bp, MISC_REG_AEU_CLR_LATCH_SIGNAL, 0x7ff);
	}
}

static void bnx2x_attn_int_deasserted(struct bnx2x *bp, u32 deasserted)
{
	struct attn_route attn;
	struct attn_route group_mask;
	int port = BP_PORT(bp);
	int index;
	u32 reg_addr;
	u32 val;
	u32 aeu_mask;

	/* need to take HW lock because MCP or other port might also
	   try to handle this event */
	bnx2x_acquire_alr(bp);

	attn.sig[0] = REG_RD(bp, MISC_REG_AEU_AFTER_INVERT_1_FUNC_0 + port*4);
	attn.sig[1] = REG_RD(bp, MISC_REG_AEU_AFTER_INVERT_2_FUNC_0 + port*4);
	attn.sig[2] = REG_RD(bp, MISC_REG_AEU_AFTER_INVERT_3_FUNC_0 + port*4);
	attn.sig[3] = REG_RD(bp, MISC_REG_AEU_AFTER_INVERT_4_FUNC_0 + port*4);
	DP(NETIF_MSG_HW, "attn: %08x %08x %08x %08x\n",
	   attn.sig[0], attn.sig[1], attn.sig[2], attn.sig[3]);

	for (index = 0; index < MAX_DYNAMIC_ATTN_GRPS; index++) {
		if (deasserted & (1 << index)) {
			group_mask = bp->attn_group[index];

			DP(NETIF_MSG_HW, "group[%d]: %08x %08x %08x %08x\n",
			   index, group_mask.sig[0], group_mask.sig[1],
			   group_mask.sig[2], group_mask.sig[3]);

			bnx2x_attn_int_deasserted3(bp,
					attn.sig[3] & group_mask.sig[3]);
			bnx2x_attn_int_deasserted1(bp,
					attn.sig[1] & group_mask.sig[1]);
			bnx2x_attn_int_deasserted2(bp,
					attn.sig[2] & group_mask.sig[2]);
			bnx2x_attn_int_deasserted0(bp,
					attn.sig[0] & group_mask.sig[0]);

			if ((attn.sig[0] & group_mask.sig[0] &
						HW_PRTY_ASSERT_SET_0) ||
			    (attn.sig[1] & group_mask.sig[1] &
						HW_PRTY_ASSERT_SET_1) ||
			    (attn.sig[2] & group_mask.sig[2] &
						HW_PRTY_ASSERT_SET_2))
				BNX2X_ERR("FATAL HW block parity attention\n");
		}
	}

	bnx2x_release_alr(bp);

	reg_addr = (HC_REG_COMMAND_REG + port*32 + COMMAND_REG_ATTN_BITS_CLR);

	val = ~deasserted;
	DP(NETIF_MSG_HW, "about to mask 0x%08x at HC addr 0x%x\n",
	   val, reg_addr);
	REG_WR(bp, reg_addr, val);

	if (~bp->attn_state & deasserted)
		BNX2X_ERR("IGU ERROR\n");

	reg_addr = port ? MISC_REG_AEU_MASK_ATTN_FUNC_1 :
			  MISC_REG_AEU_MASK_ATTN_FUNC_0;

	bnx2x_acquire_hw_lock(bp, HW_LOCK_RESOURCE_PORT0_ATT_MASK + port);
	aeu_mask = REG_RD(bp, reg_addr);

	DP(NETIF_MSG_HW, "aeu_mask %x  newly deasserted %x\n",
	   aeu_mask, deasserted);
	aeu_mask |= (deasserted & 0xff);
	DP(NETIF_MSG_HW, "new mask %x\n", aeu_mask);

	REG_WR(bp, reg_addr, aeu_mask);
	bnx2x_release_hw_lock(bp, HW_LOCK_RESOURCE_PORT0_ATT_MASK + port);

	DP(NETIF_MSG_HW, "attn_state %x\n", bp->attn_state);
	bp->attn_state &= ~deasserted;
	DP(NETIF_MSG_HW, "new state %x\n", bp->attn_state);
}

static void bnx2x_attn_int(struct bnx2x *bp)
{
	/* read local copy of bits */
	u32 attn_bits = le32_to_cpu(bp->def_status_blk->atten_status_block.
								attn_bits);
	u32 attn_ack = le32_to_cpu(bp->def_status_blk->atten_status_block.
								attn_bits_ack);
	u32 attn_state = bp->attn_state;

	/* look for changed bits */
	u32 asserted   =  attn_bits & ~attn_ack & ~attn_state;
	u32 deasserted = ~attn_bits &  attn_ack &  attn_state;

	DP(NETIF_MSG_HW,
	   "attn_bits %x  attn_ack %x  asserted %x  deasserted %x\n",
	   attn_bits, attn_ack, asserted, deasserted);

	if (~(attn_bits ^ attn_ack) & (attn_bits ^ attn_state))
		BNX2X_ERR("BAD attention state\n");

	/* handle bits that were raised */
	if (asserted)
		bnx2x_attn_int_asserted(bp, asserted);

	if (deasserted)
		bnx2x_attn_int_deasserted(bp, deasserted);
}

static void bnx2x_sp_task(struct work_struct *work)
{
	struct bnx2x *bp = container_of(work, struct bnx2x, sp_task.work);
	u16 status;


	/* Return here if interrupt is disabled */
	if (unlikely(atomic_read(&bp->intr_sem) != 0)) {
		DP(NETIF_MSG_INTR, "called but intr_sem not 0, returning\n");
		return;
	}

	status = bnx2x_update_dsb_idx(bp);
/*	if (status == 0)				     */
/*		BNX2X_ERR("spurious slowpath interrupt!\n"); */

	DP(NETIF_MSG_INTR, "got a slowpath interrupt (updated %x)\n", status);

	/* HW attentions */
	if (status & 0x1)
		bnx2x_attn_int(bp);

	bnx2x_ack_sb(bp, DEF_SB_ID, ATTENTION_ID, le16_to_cpu(bp->def_att_idx),
		     IGU_INT_NOP, 1);
	bnx2x_ack_sb(bp, DEF_SB_ID, USTORM_ID, le16_to_cpu(bp->def_u_idx),
		     IGU_INT_NOP, 1);
	bnx2x_ack_sb(bp, DEF_SB_ID, CSTORM_ID, le16_to_cpu(bp->def_c_idx),
		     IGU_INT_NOP, 1);
	bnx2x_ack_sb(bp, DEF_SB_ID, XSTORM_ID, le16_to_cpu(bp->def_x_idx),
		     IGU_INT_NOP, 1);
	bnx2x_ack_sb(bp, DEF_SB_ID, TSTORM_ID, le16_to_cpu(bp->def_t_idx),
		     IGU_INT_ENABLE, 1);

}

static irqreturn_t bnx2x_msix_sp_int(int irq, void *dev_instance)
{
	struct net_device *dev = dev_instance;
	struct bnx2x *bp = netdev_priv(dev);

	/* Return here if interrupt is disabled */
	if (unlikely(atomic_read(&bp->intr_sem) != 0)) {
		DP(NETIF_MSG_INTR, "called but intr_sem not 0, returning\n");
		return IRQ_HANDLED;
	}

	bnx2x_ack_sb(bp, DEF_SB_ID, TSTORM_ID, 0, IGU_INT_DISABLE, 0);

#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		return IRQ_HANDLED;
#endif

	queue_delayed_work(bnx2x_wq, &bp->sp_task, 0);

	return IRQ_HANDLED;
}

/* end of slow path */

/* Statistics */

/****************************************************************************
* Macros
****************************************************************************/

/* sum[hi:lo] += add[hi:lo] */
#define ADD_64(s_hi, a_hi, s_lo, a_lo) \
	do { \
		s_lo += a_lo; \
		s_hi += a_hi + ((s_lo < a_lo) ? 1 : 0); \
	} while (0)

/* difference = minuend - subtrahend */
#define DIFF_64(d_hi, m_hi, s_hi, d_lo, m_lo, s_lo) \
	do { \
		if (m_lo < s_lo) { \
			/* underflow */ \
			d_hi = m_hi - s_hi; \
			if (d_hi > 0) { \
				/* we can 'loan' 1 */ \
				d_hi--; \
				d_lo = m_lo + (UINT_MAX - s_lo) + 1; \
			} else { \
				/* m_hi <= s_hi */ \
				d_hi = 0; \
				d_lo = 0; \
			} \
		} else { \
			/* m_lo >= s_lo */ \
			if (m_hi < s_hi) { \
				d_hi = 0; \
				d_lo = 0; \
			} else { \
				/* m_hi >= s_hi */ \
				d_hi = m_hi - s_hi; \
				d_lo = m_lo - s_lo; \
			} \
		} \
	} while (0)

#define UPDATE_STAT64(s, t) \
	do { \
		DIFF_64(diff.hi, new->s##_hi, pstats->mac_stx[0].t##_hi, \
			diff.lo, new->s##_lo, pstats->mac_stx[0].t##_lo); \
		pstats->mac_stx[0].t##_hi = new->s##_hi; \
		pstats->mac_stx[0].t##_lo = new->s##_lo; \
		ADD_64(pstats->mac_stx[1].t##_hi, diff.hi, \
		       pstats->mac_stx[1].t##_lo, diff.lo); \
	} while (0)

#define UPDATE_STAT64_NIG(s, t) \
	do { \
		DIFF_64(diff.hi, new->s##_hi, old->s##_hi, \
			diff.lo, new->s##_lo, old->s##_lo); \
		ADD_64(estats->t##_hi, diff.hi, \
		       estats->t##_lo, diff.lo); \
	} while (0)

/* sum[hi:lo] += add */
#define ADD_EXTEND_64(s_hi, s_lo, a) \
	do { \
		s_lo += a; \
		s_hi += (s_lo < a) ? 1 : 0; \
	} while (0)

#define UPDATE_EXTEND_STAT(s) \
	do { \
		ADD_EXTEND_64(pstats->mac_stx[1].s##_hi, \
			      pstats->mac_stx[1].s##_lo, \
			      new->s); \
	} while (0)

#define UPDATE_EXTEND_TSTAT(s, t) \
	do { \
		diff = le32_to_cpu(tclient->s) - le32_to_cpu(old_tclient->s); \
		old_tclient->s = tclient->s; \
		ADD_EXTEND_64(qstats->t##_hi, qstats->t##_lo, diff); \
	} while (0)

#define UPDATE_EXTEND_USTAT(s, t) \
	do { \
		diff = le32_to_cpu(uclient->s) - le32_to_cpu(old_uclient->s); \
		old_uclient->s = uclient->s; \
		ADD_EXTEND_64(qstats->t##_hi, qstats->t##_lo, diff); \
	} while (0)

#define UPDATE_EXTEND_XSTAT(s, t) \
	do { \
		diff = le32_to_cpu(xclient->s) - le32_to_cpu(old_xclient->s); \
		old_xclient->s = xclient->s; \
		ADD_EXTEND_64(qstats->t##_hi, qstats->t##_lo, diff); \
	} while (0)

/* minuend -= subtrahend */
#define SUB_64(m_hi, s_hi, m_lo, s_lo) \
	do { \
		DIFF_64(m_hi, m_hi, s_hi, m_lo, m_lo, s_lo); \
	} while (0)

/* minuend[hi:lo] -= subtrahend */
#define SUB_EXTEND_64(m_hi, m_lo, s) \
	do { \
		SUB_64(m_hi, 0, m_lo, s); \
	} while (0)

#define SUB_EXTEND_USTAT(s, t) \
	do { \
		diff = le32_to_cpu(uclient->s) - le32_to_cpu(old_uclient->s); \
		SUB_EXTEND_64(qstats->t##_hi, qstats->t##_lo, diff); \
	} while (0)

/*
 * General service functions
 */

static inline long bnx2x_hilo(u32 *hiref)
{
	u32 lo = *(hiref + 1);
#if (BITS_PER_LONG == 64)
	u32 hi = *hiref;

	return HILO_U64(hi, lo);
#else
	return lo;
#endif
}

/*
 * Init service functions
 */

static void bnx2x_storm_stats_post(struct bnx2x *bp)
{
	if (!bp->stats_pending) {
		struct eth_query_ramrod_data ramrod_data = {0};
		int i, rc;

		ramrod_data.drv_counter = bp->stats_counter++;
		ramrod_data.collect_port = bp->port.pmf ? 1 : 0;
		for_each_queue(bp, i)
			ramrod_data.ctr_id_vector |= (1 << bp->fp[i].cl_id);

		rc = bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_STAT_QUERY, 0,
				   ((u32 *)&ramrod_data)[1],
				   ((u32 *)&ramrod_data)[0], 0);
		if (rc == 0) {
			/* stats ramrod has it's own slot on the spq */
			bp->spq_left++;
			bp->stats_pending = 1;
		}
	}
}

static void bnx2x_hw_stats_post(struct bnx2x *bp)
{
	struct dmae_command *dmae = &bp->stats_dmae;
	u32 *stats_comp = bnx2x_sp(bp, stats_comp);

	*stats_comp = DMAE_COMP_VAL;
	if (CHIP_REV_IS_SLOW(bp))
		return;

	/* loader */
	if (bp->executer_idx) {
		int loader_idx = PMF_DMAE_C(bp);

		memset(dmae, 0, sizeof(struct dmae_command));

		dmae->opcode = (DMAE_CMD_SRC_PCI | DMAE_CMD_DST_GRC |
				DMAE_CMD_C_DST_GRC | DMAE_CMD_C_ENABLE |
				DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
				DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
				DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
				(BP_PORT(bp) ? DMAE_CMD_PORT_1 :
					       DMAE_CMD_PORT_0) |
				(BP_E1HVN(bp) << DMAE_CMD_E1HVN_SHIFT));
		dmae->src_addr_lo = U64_LO(bnx2x_sp_mapping(bp, dmae[0]));
		dmae->src_addr_hi = U64_HI(bnx2x_sp_mapping(bp, dmae[0]));
		dmae->dst_addr_lo = (DMAE_REG_CMD_MEM +
				     sizeof(struct dmae_command) *
				     (loader_idx + 1)) >> 2;
		dmae->dst_addr_hi = 0;
		dmae->len = sizeof(struct dmae_command) >> 2;
		if (CHIP_IS_E1(bp))
			dmae->len--;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx + 1] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;

		*stats_comp = 0;
		bnx2x_post_dmae(bp, dmae, loader_idx);

	} else if (bp->func_stx) {
		*stats_comp = 0;
		bnx2x_post_dmae(bp, dmae, INIT_DMAE_C(bp));
	}
}

static int bnx2x_stats_comp(struct bnx2x *bp)
{
	u32 *stats_comp = bnx2x_sp(bp, stats_comp);
	int cnt = 10;

	might_sleep();
	while (*stats_comp != DMAE_COMP_VAL) {
		if (!cnt) {
			BNX2X_ERR("timeout waiting for stats finished\n");
			break;
		}
		cnt--;
		msleep(1);
	}
	return 1;
}

/*
 * Statistics service functions
 */

static void bnx2x_stats_pmf_update(struct bnx2x *bp)
{
	struct dmae_command *dmae;
	u32 opcode;
	int loader_idx = PMF_DMAE_C(bp);
	u32 *stats_comp = bnx2x_sp(bp, stats_comp);

	/* sanity */
	if (!IS_E1HMF(bp) || !bp->port.pmf || !bp->port.port_stx) {
		BNX2X_ERR("BUG!\n");
		return;
	}

	bp->executer_idx = 0;

	opcode = (DMAE_CMD_SRC_GRC | DMAE_CMD_DST_PCI |
		  DMAE_CMD_C_ENABLE |
		  DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
		  DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
		  DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
		  (BP_PORT(bp) ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
		  (BP_E1HVN(bp) << DMAE_CMD_E1HVN_SHIFT));

	dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
	dmae->opcode = (opcode | DMAE_CMD_C_DST_GRC);
	dmae->src_addr_lo = bp->port.port_stx >> 2;
	dmae->src_addr_hi = 0;
	dmae->dst_addr_lo = U64_LO(bnx2x_sp_mapping(bp, port_stats));
	dmae->dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, port_stats));
	dmae->len = DMAE_LEN32_RD_MAX;
	dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
	dmae->comp_addr_hi = 0;
	dmae->comp_val = 1;

	dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
	dmae->opcode = (opcode | DMAE_CMD_C_DST_PCI);
	dmae->src_addr_lo = (bp->port.port_stx >> 2) + DMAE_LEN32_RD_MAX;
	dmae->src_addr_hi = 0;
	dmae->dst_addr_lo = U64_LO(bnx2x_sp_mapping(bp, port_stats) +
				   DMAE_LEN32_RD_MAX * 4);
	dmae->dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, port_stats) +
				   DMAE_LEN32_RD_MAX * 4);
	dmae->len = (sizeof(struct host_port_stats) >> 2) - DMAE_LEN32_RD_MAX;
	dmae->comp_addr_lo = U64_LO(bnx2x_sp_mapping(bp, stats_comp));
	dmae->comp_addr_hi = U64_HI(bnx2x_sp_mapping(bp, stats_comp));
	dmae->comp_val = DMAE_COMP_VAL;

	*stats_comp = 0;
	bnx2x_hw_stats_post(bp);
	bnx2x_stats_comp(bp);
}

static void bnx2x_port_stats_init(struct bnx2x *bp)
{
	struct dmae_command *dmae;
	int port = BP_PORT(bp);
	int vn = BP_E1HVN(bp);
	u32 opcode;
	int loader_idx = PMF_DMAE_C(bp);
	u32 mac_addr;
	u32 *stats_comp = bnx2x_sp(bp, stats_comp);

	/* sanity */
	if (!bp->link_vars.link_up || !bp->port.pmf) {
		BNX2X_ERR("BUG!\n");
		return;
	}

	bp->executer_idx = 0;

	/* MCP */
	opcode = (DMAE_CMD_SRC_PCI | DMAE_CMD_DST_GRC |
		  DMAE_CMD_C_DST_GRC | DMAE_CMD_C_ENABLE |
		  DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
		  DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
		  DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
		  (port ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
		  (vn << DMAE_CMD_E1HVN_SHIFT));

	if (bp->port.port_stx) {

		dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = U64_LO(bnx2x_sp_mapping(bp, port_stats));
		dmae->src_addr_hi = U64_HI(bnx2x_sp_mapping(bp, port_stats));
		dmae->dst_addr_lo = bp->port.port_stx >> 2;
		dmae->dst_addr_hi = 0;
		dmae->len = sizeof(struct host_port_stats) >> 2;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;
	}

	if (bp->func_stx) {

		dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = U64_LO(bnx2x_sp_mapping(bp, func_stats));
		dmae->src_addr_hi = U64_HI(bnx2x_sp_mapping(bp, func_stats));
		dmae->dst_addr_lo = bp->func_stx >> 2;
		dmae->dst_addr_hi = 0;
		dmae->len = sizeof(struct host_func_stats) >> 2;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;
	}

	/* MAC */
	opcode = (DMAE_CMD_SRC_GRC | DMAE_CMD_DST_PCI |
		  DMAE_CMD_C_DST_GRC | DMAE_CMD_C_ENABLE |
		  DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
		  DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
		  DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
		  (port ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
		  (vn << DMAE_CMD_E1HVN_SHIFT));

	if (bp->link_vars.mac_type == MAC_TYPE_BMAC) {

		mac_addr = (port ? NIG_REG_INGRESS_BMAC1_MEM :
				   NIG_REG_INGRESS_BMAC0_MEM);

		/* BIGMAC_REGISTER_TX_STAT_GTPKT ..
		   BIGMAC_REGISTER_TX_STAT_GTBYT */
		dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = (mac_addr +
				     BIGMAC_REGISTER_TX_STAT_GTPKT) >> 2;
		dmae->src_addr_hi = 0;
		dmae->dst_addr_lo = U64_LO(bnx2x_sp_mapping(bp, mac_stats));
		dmae->dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, mac_stats));
		dmae->len = (8 + BIGMAC_REGISTER_TX_STAT_GTBYT -
			     BIGMAC_REGISTER_TX_STAT_GTPKT) >> 2;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;

		/* BIGMAC_REGISTER_RX_STAT_GR64 ..
		   BIGMAC_REGISTER_RX_STAT_GRIPJ */
		dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = (mac_addr +
				     BIGMAC_REGISTER_RX_STAT_GR64) >> 2;
		dmae->src_addr_hi = 0;
		dmae->dst_addr_lo = U64_LO(bnx2x_sp_mapping(bp, mac_stats) +
				offsetof(struct bmac_stats, rx_stat_gr64_lo));
		dmae->dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, mac_stats) +
				offsetof(struct bmac_stats, rx_stat_gr64_lo));
		dmae->len = (8 + BIGMAC_REGISTER_RX_STAT_GRIPJ -
			     BIGMAC_REGISTER_RX_STAT_GR64) >> 2;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;

	} else if (bp->link_vars.mac_type == MAC_TYPE_EMAC) {

		mac_addr = (port ? GRCBASE_EMAC1 : GRCBASE_EMAC0);

		/* EMAC_REG_EMAC_RX_STAT_AC (EMAC_REG_EMAC_RX_STAT_AC_COUNT)*/
		dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = (mac_addr +
				     EMAC_REG_EMAC_RX_STAT_AC) >> 2;
		dmae->src_addr_hi = 0;
		dmae->dst_addr_lo = U64_LO(bnx2x_sp_mapping(bp, mac_stats));
		dmae->dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, mac_stats));
		dmae->len = EMAC_REG_EMAC_RX_STAT_AC_COUNT;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;

		/* EMAC_REG_EMAC_RX_STAT_AC_28 */
		dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = (mac_addr +
				     EMAC_REG_EMAC_RX_STAT_AC_28) >> 2;
		dmae->src_addr_hi = 0;
		dmae->dst_addr_lo = U64_LO(bnx2x_sp_mapping(bp, mac_stats) +
		     offsetof(struct emac_stats, rx_stat_falsecarriererrors));
		dmae->dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, mac_stats) +
		     offsetof(struct emac_stats, rx_stat_falsecarriererrors));
		dmae->len = 1;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;

		/* EMAC_REG_EMAC_TX_STAT_AC (EMAC_REG_EMAC_TX_STAT_AC_COUNT)*/
		dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
		dmae->opcode = opcode;
		dmae->src_addr_lo = (mac_addr +
				     EMAC_REG_EMAC_TX_STAT_AC) >> 2;
		dmae->src_addr_hi = 0;
		dmae->dst_addr_lo = U64_LO(bnx2x_sp_mapping(bp, mac_stats) +
			offsetof(struct emac_stats, tx_stat_ifhcoutoctets));
		dmae->dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, mac_stats) +
			offsetof(struct emac_stats, tx_stat_ifhcoutoctets));
		dmae->len = EMAC_REG_EMAC_TX_STAT_AC_COUNT;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_val = 1;
	}

	/* NIG */
	dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
	dmae->opcode = opcode;
	dmae->src_addr_lo = (port ? NIG_REG_STAT1_BRB_DISCARD :
				    NIG_REG_STAT0_BRB_DISCARD) >> 2;
	dmae->src_addr_hi = 0;
	dmae->dst_addr_lo = U64_LO(bnx2x_sp_mapping(bp, nig_stats));
	dmae->dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, nig_stats));
	dmae->len = (sizeof(struct nig_stats) - 4*sizeof(u32)) >> 2;
	dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
	dmae->comp_addr_hi = 0;
	dmae->comp_val = 1;

	dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
	dmae->opcode = opcode;
	dmae->src_addr_lo = (port ? NIG_REG_STAT1_EGRESS_MAC_PKT0 :
				    NIG_REG_STAT0_EGRESS_MAC_PKT0) >> 2;
	dmae->src_addr_hi = 0;
	dmae->dst_addr_lo = U64_LO(bnx2x_sp_mapping(bp, nig_stats) +
			offsetof(struct nig_stats, egress_mac_pkt0_lo));
	dmae->dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, nig_stats) +
			offsetof(struct nig_stats, egress_mac_pkt0_lo));
	dmae->len = (2*sizeof(u32)) >> 2;
	dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
	dmae->comp_addr_hi = 0;
	dmae->comp_val = 1;

	dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
	dmae->opcode = (DMAE_CMD_SRC_GRC | DMAE_CMD_DST_PCI |
			DMAE_CMD_C_DST_PCI | DMAE_CMD_C_ENABLE |
			DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
			DMAE_CMD_ENDIANITY_B_DW_SWAP |
#else
			DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
			(port ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0) |
			(vn << DMAE_CMD_E1HVN_SHIFT));
	dmae->src_addr_lo = (port ? NIG_REG_STAT1_EGRESS_MAC_PKT1 :
				    NIG_REG_STAT0_EGRESS_MAC_PKT1) >> 2;
	dmae->src_addr_hi = 0;
	dmae->dst_addr_lo = U64_LO(bnx2x_sp_mapping(bp, nig_stats) +
			offsetof(struct nig_stats, egress_mac_pkt1_lo));
	dmae->dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, nig_stats) +
			offsetof(struct nig_stats, egress_mac_pkt1_lo));
	dmae->len = (2*sizeof(u32)) >> 2;
	dmae->comp_addr_lo = U64_LO(bnx2x_sp_mapping(bp, stats_comp));
	dmae->comp_addr_hi = U64_HI(bnx2x_sp_mapping(bp, stats_comp));
	dmae->comp_val = DMAE_COMP_VAL;

	*stats_comp = 0;
}

static void bnx2x_func_stats_init(struct bnx2x *bp)
{
	struct dmae_command *dmae = &bp->stats_dmae;
	u32 *stats_comp = bnx2x_sp(bp, stats_comp);

	/* sanity */
	if (!bp->func_stx) {
		BNX2X_ERR("BUG!\n");
		return;
	}

	bp->executer_idx = 0;
	memset(dmae, 0, sizeof(struct dmae_command));

	x_ma->opcode = (DMAE_CMD_SRC_PCI |  driver.
DST_GRC |
			 driver.
C) 200* Copyright (c)C_ENABLE009 Broadcom Co *
 RESETopyright (c) 200ou can 
#ifdef __BIG_ENDIAN9 Broadcom Co underITY_B_DW_SWAP/or else the terms of the GNU Geral Public ndif9 Br(BP_PORT(bp) ?yright (c)atio_1 : * Maintained b0)009 BroundE1HVNn.
 *<<yright (c)oadco_SHIFToadcm Everesrc_addr_lo = U64_LO(bnx2x_sp_mapping(bp, func_statsr Tamir
 * Based on hide fromHIichael Chan's bnx2 driver
 * UDP CSUM erratdsted on code bp->ver
 * x >> 2h rework by Vladisund b0Tamir
 * len = sizeof(/* bnx2host_ver
 * UDP Statistics andcomped on code from Michael Chan's bnx2 dri* UDPin.cpr Tamir
 * lude <linuund by Arik Gendelman
 * Slowpathkernel.h>
#include <linux/val =yright OMP_VALdcom*kernel.h>
ement }

* UDic void chael Cernelstart *
 */

chael *bp)
{
	if (olotport.pmf)
		chael e <lslab.h>initn.
 dcom Lic 
#includarov
 * /interruptver
 * UDPde <linux/pcichael hwslab.h>poslinux/p.h>
#instormude <linux/etherd.h>
#include <linux/slab.h>pmf>
#include <linux/vmalloc.h>inux/slab.h>.h>
etherdevice.h>
ma-mappiupdateinux/irq.h>
#includ
#incl/skbuff.h>
#include <linux/dma-mreng.h>
#include <linux/bitops.h>
#include <linux/irq.h>
#includsm/byteorder.h>
#include <linux/bmade <linuinux/deude <linux/vmalloc.h>ude <lin>
#includ *newv Zohael Ch2 dri>
#includ..h>
#incluerde*
 */

#incl.h>
#inclu *p<linuxt/ip6_checksum..h>
#incluorkqueue.h>ip6_cheth <linux/
#in2.h>
&olotx/prefetcrkqueue.h>{
		u32 lo;e <linuhi;
	} diffdcomUPDATE_STAT64(r/slab._grerb, bnx2x_inifhcinbadoctec.h>
#.h"
#include "bnx2x_initfcs
#include dot3>
#infcserrors.h"
#include "bnx2x_dump.h"
und
#include ether>
#inunderrtnepkps.h"
#include "bnx2x_dump.h"
ovrefine DRV_MODULE_VERramestoolong.h"
#include "bnx2x_dump.h"

rgE	"2009/08/12"
#defifragmenps.h"
#include "bnx2x_dump.h"
jbfirmware.h>/12"
#defijabbe1.52.1"
#define DRV_MODULE_RELxcf
#include maccontrol "bnx2receivedfies before concluding the trpnsmitter isxoff>
#ieenterMEOUT		(5*HZ)

static char version[] __devi.h>
#xpf.h"
#include "bnx2tnx2x_initsion[E " " DRoutnitdaent "
	DRV_MODULE_NAME " " DRV_MODULE_VERSIOflowng */
#done "
	DRV_MODULE_NAME " " DRV_64ULE_VERSIO/12"
#defiBC_V64it_ops.h"
#include "bnx2E " " DRV_127,9 Br	treme II BCM57710/577115it_opsto12757711E Driver");
MODULE_LICENSE("G255);
MODULE_VERSION(DRV_MODULE128ERSION);25VERSION Driver");
MODULE_LICENSE("G511);
MODULE_VERSION(DRV_MODULE256ERSION);51157711E Driver");
MODULE_LICENSE("GP023);
MODULE_VERSION(DRV_MODULE512ERSION);
02357711E Driver");
MODULE_LICENSE("GP518m(multi_mode, int, 0);
MODULE02/57711Eto152ARM_DESint multi_mode = 1;
module_pa047ULE_VERSIO.h>
#es;
 Driver");
MODULE_LICENSE("G4095module_param(nuM_DE Driver");
MODULE_LICENSE("G9216module_param(nuti_m Driver");
MODULE_LICENSE("GP6383module_param(nu;

st Driver");
MODULE_LICENSE("Gerr);
MODULE_VERSMODULE_VEi	"Brnalmactransmi int"1.52.1"
#define DRV_E " " DRV_uflmodule_param(nuuflx/pci.>
#in->pause_ "bnx2_ TX_TIMEce.h>;
MODcrc32.->>
#incx[1]. Ethernet Driver_#inclram(int_mode, int, 0);
MODULE_code_DESC(int_mode, " Force interrupt mode (1x/ioT#x; 2 MSI)");

static DRV__PARM_DESC(int_mode, " Force E_VERSION " (" DRV_1 INT#x; 2 MSI)");

static oll;
ess_fc;
module_param(droples;

static int poll;
x/io.h>
#include <linux/se>
#include <net/tcp.h>
#include <net/checksumodule_parade <net/ip6_checksum.h>
#includodule_paraorkqueue.h>
#include <linux/crc32.h>
#include <linux/crc32c.h>
#include <linux/prefetch.h>
#include <linux/zlib.h"
#includEXTENDnclud"bnx2x_in"bnx2x_init_ops.h"
#includt[3]; /* 0-cE " " DR"bnxout, 2-port1 */

static struct workine DRV_MODULE_VERSION	"1.52.1"
#defit[3]; /* 0-common, 1MODULE_VEalign2x-eM57711 = 1,
	BCM57711E = 2,
};

/* indexed by carriersenseM57711 = 1,
	BCM57711E = 2,
};

/* infalserd_infoM57711 = 1,
	BCM57711E = 2,
};

/* in/12"
#define BNX2X_BC_VER		0x040207711E = 2,
};

/* indexed by  "bnx2x_fw_file_hdr.h"
/adcom NetXtreme II BCM57711 XG"bnx2x-e1-"
#define adcom NetXtreme II BCM57711 XG in jiffies beforet[3]; /* 0-common, 1 hung */
#define TX_TIMEOUT		(5*HZ)t[3]; /* 0-common, 1nitdata =
	"Broadcom NetXtr },
	{ PCI_VDEVICE(BROnmode,E_ID_NX2_57711), BCM57711 },
	{ PCI_VDEVICE(BROAD{ 0 }
};

MODULE_DEVICE_TABLE(pci, bnx2x_pE_VERSION " (nDRV_MODULE_RELD*************************" DRV_MODULE_RELD********************ezer Tamir");
MODULE_DESCR********************/12"
#deficollision1 */

static struct workqueue_stMODULE_VEsingled only at "bnx2init
 * locking is done by mcp
 */
void multipx_reg_wr_ind(struct bnx2x *bp, u32 addr, u32 val)
{
	pci_deferred, " Dissy at init
 * locking is done by mcp
 */
void excessiv_reg_wr_inuct bnx2x *bp, u32 addr, u32 val)
{
	pci_latword(bp->pdev, PCICFG_GRC_ADDRESS,
			     BCM57710/57711/57711E Driver");
atic u32 bnx2x_reg_rd_ind(struct bnVERSION);

static int multi_matic u32 bnx2x_reg_rd_ind(struct bE_PARM_DESC(multi_mode, " Multiatic u32 bnx2x_reg_rd_ind(struct bault))");

static int num_rx_quatic u32 bnx2x_reg_rd_ind(struct b_PARM_DESC(num_rx_queues, " NumbSS, addr);
	pci_read_config_dword(bs half number of CPUs)");

statiatic u32 bnx2x_reg_rd_ind(struct bover1, DMAE_REG_GO_C2, DMAE_REG_GO_C3,
	DMAE_ARM_DESC(disable_tpa, " Disable the Tparam(int_mode, int, 0);
MODULE_PARM_DES(int_mode, " Force interrup,
	{ 0 }
};

MODULE_DE1 INT#x; 2 MSI)");

static int dropless_fc;
GO_C12, DMAE_REG_GO_C13, DMAE_REG_GO_C14, DMAE_REGx/io.ADD_64(ram(int_mode, int, 0);
MODULE_PA,
	 mmand GO_C12, DMAE_REG_GO_C13, DMAE/********************mae_command *x; 2 MSI)");

static int droples_command *dmae,
			    int idx)
{
	u32 cmd_offset;
	int i;

	loMAE_REG_GO_C10, DMAE_REG_G " Pause on eDESC(poll, " Use polling (for de*****
module_param(poll, int, 0);
MODULE_PARMset + i*4, *(((u32 *)dmae) + i));

		Dnx2x_post_dmae(struct bnx2x *bp, 

		DP(_command *dmae,
			    int id;

static int poll;
mod_offset = (DMAE_REG_CMD_MEM + _offset_WR(bp, dmae_reg_go_c[idx], 1);
}

void bnx2x_wri_comm.h>
#incluinde <linuclude <li <net/tcp.h>
#include <net/checksumnig" Force Max Read Req Size ( 200;

	iorkqueue.h> 200;

	if oldcludnclude <liold_
		u32 *data = bnx2x
#include <linux/crc32.h>
#include <linux/crc32c.h>
#include <linux/prefetch.h>
#include <linux/zlib.h>
#include <linux/io.h>


#include "bnx<linu 200timer_maxAE_R
#includlink_vars.>
#itype == MAC_TYPE_BMAC/interrupth>
#include <net/tnux/pci.h>
#include uct dmae_command));

	dmae.opcoEe = (DMAE_CMDodule_param(mrrs, nux/pci.h>
#{ /* unreached */
		BNX2X_ERR(">
#incinux/dd byyrigh but no	dma active\n"erde	return -1nclu
x_postt[3]; /64((int_modbrb_dropite_*dmae,
		WAP |
#enma_adommand new   (BP_iard_d - old_PORT_1 : DMAerdeMAE_CMD_ENDIANram(int_WAP truncatendif
) << DMAE_CMD_E1HVN_S(bp) ? DMAE_CMD_PORT_D_E1HVN_E_CMD_PORT_0D_E1HVN_MAE_R.h"
#include _NIG(egress, PC_pkt0);
MOD				" (default is half number of CPUs)");

static int t_addr_lo = dst_add1, REG_GO_C4, DMAE_REG_GO_C5, DMAE
	memcpy(old,_CMD,ertner
 *
 */


		u32 *dadmae.comp_ad&p) << DMAommon, 1-port0, 2-port1_hi), &NITY_DW_Se, " Force)_command *rtner
 *
 */

e, " For Tam) << DMAE_CM|
#endi =
		       (BP_PORT INT#x; 2 MSI (BP_PORT(b [%d *4]  "
		    "dDULE_P(int_mod
#include <linuinux/i = ++r [%x:%08x]  comp_val 0xendmae.mset(&dmae, 0 = SHMEM_RD <linux/crmb[undation.
 ].2x_inmset(&dmaerde
#ini, dmae.src_ad!=T));
	dmamset(&dmae, 0)ude <.comp_addr_hi, dmae.c =emset(&dmae, 0, #ifdef __BIG_NIG (&dma al);(%u)\n"FT));
	dmadr_hi, dmae.co      D#else
	ort.h>
#inclumae;
	u32 >
#include <(mrrs, int, 0);
MODULE_PARM_DESC(mrr/prefetc_query ude <lh>
#include <linflude <lorkqueue.h>numbrm_perclude <linux/te <ls_fc;
m	&int_mod 0;

	bn.c:on..h>
#inclisticb.h>
#inclu#include <linux *data _lock(&bp->dmae_muer
 * UDP >
#include <linux/prefetch.h>
#include <linux/zlib.h>mae;i wb_comp));
	{
		DP->total_byt 0);
MODULE_PA opcode 0x%&ichael Ch2 driver
 * UDP_base)
		}
		cnt--;
		/* adjust delay for rtner
 *
 */

#include <linux/m- 2*rtner
 u32src_addr  [%xble tcnt--;
		/* adjustement b;
}

void bnx2x_read_dmae(str)]\n"nx2x *bp, u3212"
#defiE_RENX2X_BC_Vruct bnx2x *bp, u32ommand dmae;
	u32 *wb_c)
{
	struct dmae_no_buff_1 : DMAruct bnx2x *bp, u3ae_ready) {
		u3)
{
	str
	for_T_RE_rx3]);uRC_R, iomp_adinclude <linufastpath *f/iopde <lfp[i]OFF,mae;cl_ita[0fp->  usiOFF,_comp = 0;

	bnx2xcli
		D_dmae(bp		for s_fc;
mT_DMAE_C(bp));

	udela		for (i = hile ([  usi)"
		src_addr, len32);
		for (i = 0; NX2Xi < len32 &indi0, sizeof(s i*4);
		reu, len32);
		for (i = 0; u < len32; i++)
			datpcode =bnx2x_reg_rd_ind(bp, src_addr + i*4);
		repcode = (DMAE_CMD_SRC_GRCNX2X | DMAE_Cuct dmae_c | DMAE i*4);
		rex, len32);
		for (i = 0; x < len32; i++)
			datNDIAN
	bnx2x_reg_rd_ind(bp, src_addr + i*4);
		reNDIAN
		       DMAE_CMD_ENX2XNDIANITY_uct dmae_cNDIANIT i*4);
		re <linux/prq(i = 0; q>
#includindiBP_E1HVN(bpio.h>


e "bnx2x	/* are 0;

	ude <l valid?T |
#i
#in(u16)(le16_to_cpu(NDIANIT->include uype,) + 1) !ae, INI		olot U64_LO(bnx2x_sde <	DP(fdef _MSGncludS, "[%d]r >> 2;notN
		       DNDIAN
"addr   " dmae.le O(bnx2x (%dappiude <linup_addr_lo 0x%0 = len3i, addr_lo = U64_LO(bnx2x,Zolot U64_LO(bnx2x_OFF,
#else
		    	}c_addr_hi = 0;
	dmae.dst_i < len = U64_LO(bnx2x_sp_mapping(bp, wb_data));
	dmae.dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, wb_data));
	d 0;

	n = len32;
 0;

	comp_addr_lo = U64_LO(bnx2x_sp_mapping(bp, wb_com_COMP_VAL;

	DP(BNX2X_i = U64_HI(bnx2x_sp_mapping(bp, wistimp));
	dmae.comp_val = DMAE | DMAE = U64_LO(bnx2x_sp_mapping(bp, wb_data));
	dmae.dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp, wb_data));
	dpcode n = len32;
pcode comp_addr_lo = U64_LO(bnx2x_sp_mapping(bp, wb_comdst_addr_hi, dmae.dst_i = U64_HI(bnx2x_sp_mapping(bp, w4c_addr
		<< DMA
		}
		cnt--;
		/* adjusts_fc;
le32l = DMAE_COMP_VALrcv_broadcastcnt--;.hi_mapp

	udelay(5);

	while (*wb_coess_fc;
AE_COMP_VAL) {

		if (!cnt) {
			BNX2X_ER_comman_post_dm

	udelay(5);

	while (*wb_compp) ? DMAE_ AE_COMP_VAL) {

		if (!cnwrite			BNX2X_ERR("else
			ude timeout!\n");
			break;
		}
		else
			udelay(5);
	}
	DP(BNX2X_MSG_OFF, "data [0x%0lop, wb_HIP_REV_IS_SLOW(bp))
			msleep(100);
		else
			udelay(5);
	}
	DP(BNX2X_MSG_un "data [0x%08x 0x%08x 0x%08x 0x%08x]\n",
	   bp->slowpath->wb_data[0], bp->slowpath->wb_data[id bnx2x_writepath->wb_

	udela
	dma

	while (*wb_comp != DM);

	udelay(5);

	while (*wb_compDMAE timeoutlen > DMAE_LEN32_WR_Mess_fc;
mE timeout!\n");
			break;
		}
		 0;

	while (d bnx2x_read_dmae(struct {
		bAE_COMP_VAL) {

		if (!cnd bnx2x_reaRR("DMAE timeout2 src_addr, u32 len32)
{
}

	bnx2x_write_dmae(bp, phys_addr + offse		if (CHIP_REV_IS_SLOW(bp))
			msleep(100);
		else
			ude 4;
		len -= DMAE_LEN32_WR_MAX;0x%08x 0x%08x 0x%08x]\n",
	   bp->slowpath->wb_data[0 + offset, len);
}

/* used onlh->wb__C2, DMAE_REG_T* 0-co len)
{
	inBC_V >> 2;
	}
		cx_wb_rd(sackL;

 TX_TIMEOUT	USE_WB_RD
static u64 bnx2OFF, "datastruct bnx2x *bp,b_data, 2);
{
	u32 wb_data[2];

	REG_RD_DMAE(bp, reg, wt) {
			BNstruct bnx2x *bp,mc_assert(s
{
	u32 wb_data[2];

	REG_RD_DMAE(bp, reast_idx;too_big_1 : DMA >> 2;
	dmae.dst_ae;
	u32 *wb_[2];

	REG_RD_DMAE(bp, reae_ready) {
		u, ae_ready) {
		uh->wb_SUB_CMD_ENDU* 0-cu			BNae_readystruct bnx2x *bp, int reg)
{
	u32 wb_data[2];
 (last_idx)
		BNXmX_ERR("XSTORM_ASSERT_LIST_INDE64(wb_data[0], wb_data[1]);
}
# (last_idx)
		BNXbX_ERR("XSTORM_ASSERT_LIST_INDE
{
	char last_idx;
	int i, rc = 0;
	u32 row0,
		BNX2X_ERR("XSTORM_ASSEIST_INDEX_OFFSET);
 = REG_RD(bp, BAR_XSTR/
	for (i = 0; i <    XSTORM_ASSERT_LIST_OFFSET(i) + 4);
		rRORM_INTMEM +
			  IST_INDEX_OFFSET);
	if

	udelay(5);

	whil, " Disat_MAX;
	}

	bnx2x_write_daddr_lo =id bnx2x_writdr_t RR("DMAE timeout!\n");
			br+
			      Xnly for slowpath so nSET(i) + 12);

		if (row0 !=		if (CHIP_REV_IS_SLOW(bp))
			msle+
			      XSTelse
			udelay(5);
	}
	addr_lo =OFF, "data [0x%ow0 != CO0x%08x 0x%08x 0x%08x]\n",
	   bOPCODE) {
			B2, row1, row0);
			rc++;
		} else {
			break;
		}
	}
path->wb_data[2], bp->slowpath->wb_d  i, row3, row2, row1, row0);
			rc++;
		} elst) {
			BNX2X_E	}
	}

	/* TSTORM */
	last_idx = REG_RD8(bp, BAR_TSTRORM_INTMEM +
			   TSTORM_ASSERT_ the asserts */
	for path->wb__C2, DMAE_REG_X		BNX2_wb_rd(strudr_t ct bnx2x *bp, int reg)
{
	u32+
			      _LIST_OFFSET(i) + 4;
		rob_data, 2);

	bp, BAR_TSTRORM_IN64(wb_data[0], wb_TSTORM_ASSERT_LIST_OFFSET(i) + 4);
	mc_assert(strubp, BAR_TSTRORM_IN
{
	char last_idx;TSTORM_ASSERT_
		0, sizeof(s->checksum_1 : DMAE=val 0x%08x2);

		if (row0 LISTOFFSET(i) + 1ttl0f (row0 != COMMON_AS2X_ERR("TSTO->wb_data[2]break;
		}
		cnt--;
		/* adjust0x%08x 0x%08x 0x%08x]\n",
	   bp->slowpa			  i, row3,break;
		}
		cnt--;
		/* adjuo;
	REG_WR_DMAE(bp, r		}
	}

	/* CSTORM */
	_LIST"
				  " 0x%08x 0x%08x 0x%  i, row3, row2, row1, roBNX2X_ERR("TSTORM_ASSERT_LIST_INDEX 0x%x\n", RM_ASSERT_LIST_INDEX_OFFSET);
	i
	last_idx = REG_RD8(bp, BAR_CSTRint the assert+
			   CSTORM_ASSERT_LIST int reg)
{
	u32 wb_data				  i, row3, row2, row1, rbp, BAR_CSTRORM_INTMEM +
			      CSTOrow0 = REG_RD(bp, BAR_CSTRORM_INTMEM +

	last_idx = REG_RD8(bp, BA+
			      CSTORM_ASSERT_LI+
			   CSTORM_ASSERT_LIST64(wb_data[0], wb_data[1])				  i, row3, row2, row1, rSERT_LIST_OFFSET(i) + 8);
		row3 = REG_R     CSTORM_ASSERT_LIST_OFFSET(i) + 8);
	
	last_idx = REG_RD8(bp, BA2);

		if (row0 != COMMON_ASM+
			   CSTORM_ASSERT_LIST_{
	char last_idx;
	int i,				  i, row3, row2, row1, ro" 0x%08x 0x%08x 0x%08x\n",
				  i, row 0x%08x"
				  " 0x%08x 0x%08x 0x%08x\n",
	last_idx = REG_RD8(bp, BARM */
	last_idx = REG_RD8(bp,; i++) {

		row0 = REG_RD(bp, BAR_CSTRORM__OFFSET);
	if (last_idx)
		BNX2X_ERR("CNX2X_ERR("USTORM_ASSERT_LIST_INDEX 0x%x\n(last_idx)
		BNX2X_ERR("USTORM_ASSERT_LISTLIST_OFFSET(i) + 4);
		row2 = REG_RD(bp, BAERT_ARRAY_SIZE; i++) {

		row0 = REG_RD(NTMEM +
			      TSTORM_ASSER		row3 = REG_RD(bp, BAR_CSTRORM_INTMEM +
			  sserts */
	for (i = 0; i < STROM_ASSERTST_OFFSET(i) + 4);
		row2 = RESM_INVALID_ASSERT_OPCODE) {
			BNX2X_ERR("CSTERT_ARRAY_SIZE; i++) {

		row0 = REG_RD(TMEM +
			      TSTORM_ASSERT,
				  i, row3, row2, row1, row0);
			rc++;
	ASSERT_LIST_INDEX 0x%x\n", last_idx);

	_OPCODE) {
			BNX2X_ERR("USTOp, BAR_USTRORM_INTMEM +
			   USTORM_ASSERT_LERT_ARRAY_SIZE; i++) {

		row0 = Rlen > DMAE_LEN32_WR_MAX(last_idx)
		BNX2X_	return rc;
}

static void bnx2x_f		}
	}

	return rc;
}

statico;
	REG_WR_DMAE(bp, re32 data[9];
	int word;h->wb_data[2];
}

void bnx2x_read_dmae(strucl_hi, u32 val_lo)
{
	u32 wb_write[2];

	wb_write[0] = *bp, u32 src_addr, u32 len32)

	REG_WR_DMAE(bp, reg, wb_write, 2);
}

#ifdef104);
	mark = ((mommand dmae;
	u32 *wb_com
	REG_WR_DMAE(bp, re4) {
		for (word = 0; word < 8; word2x_sp(bp, wb_comp);
	int cnt = 200d < 8; word++)
			data[word] = htonl(REG_RD_OFFSET);
	if (!bp->dmae_ready) {
		u32 ,
		BNX2X_	}
	for (offset = 0, MCP_REG_MCPR_SCRp(bp, wb_data[0]);0xF108; offset <= mark - 0_OFFSE   DMAE_		  " 0x%08x 0x%08x 0x%08x\n",
				  CP_REG_MCPR_SCRp_val = DMAE_COMP_VAL;

	DRATCH +
			break;
		}
	}

	/* CSTORM */
	lasCH +
						  offset + 4*word));
		data[H + 0xfcomp_adram(in(BNXbreak;
		}
		cnt--;
		/* adjust delay for e
			udelay(5);
	}

	mutex_unlock(&bp->dmae_mutexx_post_dmae(strucata[word] = htonl(REG_RD(bp, CH +
						  offset + 4b" }
};


static constite_dmae(struct bnx2xATCH +
						  offset + 4*wortate - DISABLED\n");

	BNX2X_ERR("begin crash _OFFSETATE_DISABLED;
	ark + 0x3) & ~0x3);
	prinCH +
						  offset + 4*word));
		data[8] = 0x0;
		k);

	printk(KERN_ERR PFX);
	for (a);
	}
	printk(KERN_ERR PFX "end of fw dump\n"
#include <linux/mp_addr_lo, ddst_filterf (row0 !=}

	bnx2x_write_dme <l->spq_prod_idx);

	/xF900ram(int_xxE_REezerdx);

	/* Rx */
	for_each_rx_queueth *fp = &bp->fp[i bnx2x_fastpaE_CMD_E1HVN_Sx);

	/* Rx */
	for_each_rx_queue	  "  *rx_bd_cons_sb bnx2x_fastpadst_ (row0 != 
	for_each_rx_queue(bp,_cons(%x)  bp))break;
	#include <linuxx%08x\n",
			  le16_to_cpu(*fp->rx__addr_h= U64_HI(bpoftwng
		int iata[0], bp->slowpathde <linux/sne
#includ <net/tcp.h>
#include <net/checksum.<linux/prefetch.h>
#include <linux/zlib.h>
#inclu"   device" Force M>
#include <ldevU64_HI(MAE timeout!ax_sgeoffseast_idx* Rx 
	u32 *ilo(&ram(int_T_LIST_OFFSET(i));
		row1 = REG_R) +blk->u_status_block.status_blSERT_LIST_OFFSET(i) + 8);
		rfor_each_tx_queue(bp, i) {
		str row0);
			rc++;
		} else {
	ATS_Sx),
			 tfp->status_blk->u_status_block.status_block_index);
	}

+
			      XSTfor_each_tx_queue(bp, i) {
		struct bnx2x_fastpathp->tx_pkt_prod, fp->tx_pkt_cons, fp->tx_bd_pro_OFFSET(i) + 12);

		if (row0 !=
			  "  tx_brxcnt--;h>
#inclucons_sb));
		BNX2X_ERt--;
		/* adjust 			  "  tx_bd_px_db_prod(%x)\n", le16_to_cpu(fp->fp_c_i(%x)  *sb_c_idx(%x)"
			  "  t|
#epe != rx_cons_sb(%x)\n",
; i;

		DP(BNX2X_MSG_OFF, "
	);
	}

	/* Rings */
+nt--;
		/* adjustlen32 %d)BNX2Xi < len.2);

		if (row0 p->status_blk->cings */
	/int ix),
			 OFF, "dats_blk->u_status_block.status_bluct bnx2x_fastpath *fp = &bp->o_cpu(*fp->rd only at s_blk->u_status_block.stat********/

/* used only at _idx(%x)"
			  "  tlength_addr fp->rx_desc_ring[j];
			steme II BCM57711 XGb" },
	{ "Brorod, fp->tx_pkt_cons, fp->tx_ata[word] = htonl(REG_RD(bxF90x),
			  fpE_RE_ERR("fp%od(%x)\n", le16_to_cpWAP |
#endifor_ea		fp->last_max_sge);
		for D_E1HVN_SHIx_sge_prod);
		crc_ERR("fp%d: rx_bd[%x]=[%x:%x]  sw_bd=[%pMODULE_VERSION	"1.2 *rx_sge = (u32 * "bnxp->rx_sge_ring[j];
			struct sw_rx_page *sw_page board_type, abo_page_ring[j];

			ifo= RX_SGE(fp->last_max_sge);
		ae_ready) {
		u32 x_sge_prod);
		ord(ed= RX_SGE(fx_fastpath *fp = &bp->fp[iu_idx),
			  fp RX_SGE(fing[j];

			BNX2X_ERR("fpt; j MAE_C_prod);
		end = RX_SGE2 *cqe = (u32 *)&fp-)&fp->rx_sg2 *cqe = (u32 *)&fp-	BNX2X_ERR("f%x]=[%x:%x:%x:%x]\n", sw_page->p2 *cqe = (u32 *)&fp-nd = RCQ_BD(f			  "  tx_bd_paborow0 ERR("fp%d: rx_bd[%x]=[%x:%x]  s
			       PCICFG_VENDOR_ID_OFFrod, fp->tx_pkt_cons, fp->tx_bal);
	pci_write_config_dword(bp->pdmp_cons - 10);
tx_rd_infop->rx_sge_ring[j];
			struct sw_rx_page *sw_page rd_info[] __devinib) + 245);
		for (, sw_page->pagnt b5);
		for (heartbeg_rdX_ERR("fp%d: packet[%x]windown",
				  i, d: packet[%x]!= end; j = RCQ_B {
		struct bnx2x_2 *cqe = (u32 *)&r (j = start; j !=+u_idx,cons_sb) - 10);
		end = TX_BD(le16_todisable_tpa, " Disable thmp_con.h>
#include <linux/sdrv    rx_sge_prod(%x)  last_max_sge(%x)"
			  "  fp_u_idx(%x) *sb_u_idx(%x)\n",
			  fp timeout!ram(int_drind =2 cm{
	struct dmae_ j != y) {
		u3pkORT_p(bp);
	bnx2x_mskb_alloc_faille16_to_;
		}
	}
hw_c		iferrBNX2X_E;

		DP(BNX2X_MSG_OFF, "DMAE is not ready BP_E1HVN(bp) << DMAE_CM = RX_BD(lHVN_SHIFT));
nx2x_fastpa
	bnx2x_fw_d+=
		BNX2X_
	bnx2x_fw_bnx2x_fastpax_mc_assert(bp);
	B;
	u32 val x_mc_assert(bp);
	, addr);
	int msh dump --------- & USING_MSIX_sh dump --------bnx2x_fastpa}

static vo;
	u32 val }

static vcons,.h>
#include <linux/slab.h> <net/tcp.h>
#include <net/clinuude <linux/iopip6_checksum.kernel.h>
#>def_attude <linux/i!inux/errno.h>
#bnx2#else
>def_att_idx, bp->attblk->u_stawb_comp = bnx2xnux/pci
#incdata[1],
	   bp->slowpat.
 *&&includmp_cons, le16++;

	3)tn_stafdef __BIG_ENaddr >> 2;werep, wb_data));for 3 0x%0s_SWAP |
errupt.anic(AP |
#else
cons,
	X_ERR("      rx_sge_protherdevice.hBNX2X_ERR("fp%d: IG_0_REG_SINp->msglevel & NETIF4_HI(TIMER"DMAE is not ready (src_addr %00_r);
	len32 T_0) |
		       (MSG_INTR, "writ);
	]);

	fp[|
		numBNX2X_MSGs]xF900);
		return;
	}

	memset(&dmae, 0, sizeof(strng(bp, waddr = dmae_command));

	dmae.ont port = BP_PORT(bp);
	u32 addr = E1HVN_SHIFT));
	dinclude <linux/prefetch.h>
#include <linux/zlib.h>p->rx_sge_prod, fp->last_max_sge,
			  le16_to_cpu(fptx_bd[3]);	printk(KERN_DEBUG "%s:0x%08	  le16_tnamMODULore leading/trailing  tx avail (%4x) (CHIhc idx%08x)n = l	%x:%08x ;
	B(%lxping(bp,mmand *NGLE_I {
	_IS_(l, por/* TSTORM */;
	dmae.dst_al, portx_bd_s_sbsb),BD(fp->tx_bdast_idxiowb();
	barrier();

	if (rx usageE1H(bp))r{
		/* init leading/trairing edge */
		if (IS_E1HMhi = 0;
	dmae.dst_ "write_ERR("	if (bp - *cqe =  _0 + port*8, mp8, vaee0f | (1 << (BP_E1HVN(bp) +  port*8, val);
->port.pmf fp->statuiowb();
	barrier();

	if (%s (Xfw_devx-e1 %u)  brb |
#etati  eading/troid 	dmae.src%u		REG_WR(bp, HCnetif2X_MSG[1],s */ |
		dev *
 "wb()" : ? Hn"/* TSTORM */
	last_i
	bnx2x_fw_8000000; offset +=  (BP_PORT(bpT));
	dmae.src_addr_lo =iowb();
	barrier();

	ift>
#in: 2);

		if (row0 !int_disab" row2, row3;

	/* XSTOR %lu IST_INDEX_OFFSET |
		 HC_CONsb(%x)\n",
	int_spq_prod_idx);

	/*
		 HC_CONth *p = &bp->fp[i]int_	  "  *rx_bd_cons_sb(
		 HC_CON2X_ERR("TSTORbp)
{
	int port AE_COMP_VAL)OFFSET(i) + 12);

		if (row0 /* TSTORM */d(%x)\n", le++)
			data[word] = htonl(REG_RD(bbp, addr) != val)
		BNX2X_ERR("BCQ_BD(fp->rx_comp_c8000000; offset += , fp->rx_bd~(HC_CONFIspq_prod_idx);

	/8000000; offset += th *fp = &bp->fp[i~(HC_CONFIG_0_REG_SINGL->flags & USING_MS();

	REG_WR(bp, addr, va2X_ERR("TSTOth->wb_;

		DP(BX_MSG_OFF, "DMAE ore leading/trailing_map: |
	\t (disablex%0808000(IS_E1HMF(bp))f/FPGA p_vax);
	(struent the HW from sending  fp-rrupts */
		bnx2x_int_disable(bp);callP CSUMmp))_CONFIG_0_clude <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linuxcomp_val 0x%0optcp.h>
#include <net/checksumx_main.c: Br *x_ma	}

	mest net, tx_bdloader_* in= PMF_ drive_EN_0 |G_MSI_MSIX_INT_EN_0 |
			HC_CONFIG_0_REG_ATTN_e <lixecud_id>pdev-bd);
st network driver.
 *
 * Copyright (c) 2007-2009 B *
 * This program is fr General see; you can redistribute it and/or modify
 * it under th General se the GNU General Public Licens,
				u8 storm, u16 i the Free Software   oundation.
 *
 * Maintained by: Eilon Greenstein <eilo + BP_oadcom.com>
 * Written by: Eliezer Taef_att_idx, bp->h>
#incomp_
	m EveEN_0 |
			HC_CONx_ma		REGk(&bp->sp_ta++ val);include <linux/init.m Everest networkst netw *
 * This pr 2007-2 bnx2xLicenseIFT) |
			 (storm << IGU_ACK_REGISTER_STPCIxF900ir
 * Based on code from Michael Chan's bnx2 dri= indexUDP CSUMM errata workaround by Arik Gendelman
 * Slowpath_SHIFT));

	DP(BNX2X_Mby Vladislav Zolotndex = index;Statistiics and Link management bby Yitchak Gertner
 *
 */

#incl_SHIFT));

, (*(u32 include <linux/int_addr>
#include <linux/modx_maireg_go_c[irq(bp->pd], (*(u32 lude <linux/device.h> ake sr.h>
#include <linb_comp | DMAEuct host_status_blocnly for suleparam.h>
#include <linux/kernel.h>
#inclt host_status_block *fck is wrifor dev_info() */
#include <linux/timer fp->status_blk;
	uux/errno.h>
#incl	lude <linux/ioportix) {
		s

static inline u16 u_ack.sb_id_and_flags =
			((sb_id << IGU_ACK_REGIST			 (update << IGU_ACK_REGISTER_UPDATE_INDEX_SHIFT) |
			 (op << IGU_ACK_REGISTER_INTERRUPT_MODEver
 * UDP CSUMM errata workaround by Arik Gendelman
 * Slowpath and fastpath rrework by Vladislav Zolotarov
 * Statisti *)&igu_ack));

	/* Make sure that ACK is written */
	mmiude <linux/module.hx2x_update_fpsb_idx(st written to by the chip */
	if (fp->fp_c_idxude <linux/device.h>  /* for dev_info() */
#include <linux/timerc_status_block.status_block_index;
	rc |= 1;
	}
	if (f HC_CONFIG_0_REG_INT_LINE_EN_chronize_irq(bp->msix_tablmae;inux/dsk);
	fls.h>
#include <linux/ih>
#include <linux/intd != fp->INGLE_I	val |= (HC_CONFIG_;

	0 skb d != fp|dx
 * retSR_EN_0 |
			HC_CONFIG_reed
 */

#ininux/du16 bnFIG_0_REG_MSI_MSIX_INT_EN_0 ;
}

statice <linux/intterrupt.h>
#includchronx)
{
	stynchronize_irq(bp->msix_taps.h>
#include <linux/ir HC_CONFIG_0_REG_INT_LINE_EN_do_nothbnx2ude <linux/vmalloc.h.h>
#inclu, vat 
#include de <l(*Y_B_on)x_buf->skb;
	u16 b----numlinux/slab.h>
#ite nex (i = e;
}linux/slab.h>
#m[bnx2xncludE_MAX], tx_buEVENTb);

 = {
/*NFIG_e	
}

s	*/
/
	DPDISram D	PMF	*/ {q.h>
#include <linux/d,  tx_buf, skbe bd_idx},
/*		LINK_UPn", bd_idx);
	tx_
#inc,mmand  tx_buf, skbogram _idx].st.h"
#in", bd_idx);
	tx_f *skb = t &fp->tx_desc_ring[bd_idx].stSTO;
	pci_unmap_singltart_bd), PCI_DMA_TODEVICE);

	nb
},, "freR(tx_st %d\n", bd_idx);
	tx_stare(bp->pdBD_UNMAP_ADDR(tx_start_bd)art_bd;
	pci_unmap_singl>
#incl>pdevBD_UNMAP_ADDR(tx_start_bd),
			 BD_UNMAP_LEN(tx_st_bd = &ns = nbd + tx_buf->first_bd;

16_to_cpu(tx_start_bdchro>pdev, 
#ifdef BNX2X_STOP_ON_ER
};h>
#include <linux/slab.h>handl/tcp.h>
#include <, MSG_OFF, "pkt_idx
}

s;
}

soc.h>MSG_OFF, "pkt_idx %d  b %d  b%x to kt_idx %d  x_pkt_cons);
}

idx, %d  ][
}

s]._cons;_EN_0 |
_BD) {
		--nbdEN_0 |
			= TX_BD(NEXT_TX_IDX(bd_uff @(%p)->s
_lo Make sure thX_TSO_SPhas been "changed"T |
#smp_wmbSR_Estruct( mappi!== nbd +ap fir.h"
#i) ||0 |
			HC_CONFIG_0_REG_ATTN_BIT_ENE_E_hi = U64_HI(bnx2x_sp %d  b%d ->;
}

stx_data_LEN(tx_
		if (IS_ %d  ,;
}

si = U64_HI(b %d  = 1;
		for_each_queue(bp, i)
			synIP_Rde <liize_irq(bp->msix_table[i + offset].vector);
	} else
	I_MSIX_INT_EN_0 |
			HC_CONFIG_0_REG_ATTN_	DP(anityT |
#
#in! sw_tx_bd *t_pagline u16 bn index;
	igONFIG_0_REG_ABUG!_SWAP |
#else
	HC_CONFsb_id << IGU_ACsk);
	flck.sb_id_and_flags =
			((sb_id << IGU_ACK_REGIS Everest network driver.
 *
 * Copyright (c) 2007-2009 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Maintained by: Eilon Greenstein <eilong@broadcom.com>
 * Written by: Eliezer Tamir
 * Based on code from Michael Chan's bnx2 dri_SHIFT));

	DP(NX2X_MSG_OFF, "write 0x%08x to HC addr 0x%x\n",
	   (*(u32 *)&iu_ack), hc_addr);
	REG_WR(bp, hc_addr, (*(u32cs and Link management by Yitchak Gertner
 *
 */

#inclowb();
	barrier();
>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/device.h>  /* for dev_info() */
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/etherdevice.h>
clude <linux/i.h>
#include <linux/s*/
		if (CHIP_R*/
	WARN_ON(!skb);
	dev_kfrmae;vn, vnval);
	ISu_acMFn.
 *
  by: EMAX : E1NE, "h, tx_bd, &dma undation.
 , tx_bd"wb_ke surearov
 * , "frees;
}

static inline u16 bnx2x_tx_avai inline u16 bnpath *fp)
{
	s16 used;
	u16 prod;
	ucons)ve oursw_cons, T |
#arov
 * S Zolotarov
 * nt i;

 (vk GeVN_0; vn <b); */
, fp++u16 bn"wb_ = 2*vn +s %u 
	strulotarov
 * Sdr_lo,
	   dmae"wb_cmb["wb_].fw_mb_paramx_start_bd;clude <linux/netdevictruct eth_tx_start_bd *tx_start_bd;
	struct eth_tx_bd [NEXT>
#ior_IDX(sw_cons)].skb)>tx_pkt_cons = ons = bnx2pkt_cons = TX_BD(sw_cons);

		/* pref <net/tcp.h>
#include <net/checksumffset].vector);
	}8x  len3art_bd-uf->first_bd = 0;
	tx_buf->skb = NULL;

	return new_cons;
}

static inline hw_cons) {
			rmb();
			prefetch(fp->tx_buf_ring[N16 cons;

	barrier(); ");
set(lity, 0U64_HI(bnx2x_sp_x_main.c: Broadcom Everest network driver.
 *
 7-200yright (c) 200* Cop9 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Maintained by: Eilon Greenstein <eilong@broadcom.com>
 * Written by: Eliezer Tamir
 * Based on code olotarov
 * Statistics anda workaround bnt by Yitcby Vladislav Zfrom Michael Chan's bnx2 driver
 * UDPHIP_REth rework by Vladisund by Arik Gendelman
 * Slowpath and fastptype);

	bp->spqhak Gertner
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/device.h>  /* for dev_info() */
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ioport	while (sw_cons != hw_cons) {
		u16 pkt_cons;

		pkt_cons = TX_BD(sw_c <linux/netbp->tx_buf_ring[pkt_cons] %u  pkt_cons %u\n",
		   hw_ pkt_cFUN/* make  timeout!x_comp_cons, le16_to_cpueue_stopped(txq)) &&
		* now freenx2x_sp_k);
	fl/*TATE_Hctordefau>
#incfreemanage2x-estatic inlBP_NOMCPdx];_fastpaG_WR(bp, hc_addr,dr_lo,
	   dmae.len, dmuct ]ruct bnx2xunliktx_pkt_cons = sw_cons;
	fp->tx_bd_cons = bd_cons;

	/* clud 0;

	bar */
		return;
	}

	sf (fpROD_CMD_ID_ETH_ and pd),
			       BD_UNMAP hc_addr,0x%x sw_cons)].
	caing(bp != G_WR(bp, hc_addrconfig<linux/in
			break;
	P_VAL) /		    (bp]);

		DP(BNX2X_MSG_OFF, ate == BNX2X_STATE_mapping(bp, wTIF_MSG_IFNX2X_MSG_OFF,.ORT_1 : DMAElock.REG	   dmaeNIG__HALclud0_BRB_rinCARD++;
	}
*0x38G_WAIT4_DELETE;
		fp->state = B	dmae.srcSTATE_HALTED;
		break;

	case (RAMTRUNCATE_ID_ETH_CFC_DEL _HALTEq);

D;
		break;

	case EGRESS_dmaePKT0_ID_ETH_CF5r >> for emuT4_DELETE;
		fp->stater_lo = dst_addrLE_I, 2\n", cid);
		bnx2x_fp(bp, cid, state) = BNX2X_FP1STATE_CLOSED;
		break;


	case (RAMROD_CMD_ID_ETH_SET_MAC |1BNX2X_STAT		bre"wb_ons;NETIF_MSG_I Ensure that bp->intr_sem is not ready (src_addr %08x  len32 %d)"

	IFDOWN, "IF_MSG_INTR, "wate f (IS_E1HMrtner
 *
 */

r, len32);
		for (i = 0t bnx2LT):
	case (RAMR | DMAEID_ETH_SET_MAC | BNX2X_STATEpcode = (DMAE_CMD_SRC_GMSG_IFDOWN, "got (un)sNDIANITID_ETH_SET_MAC | BNX2X_STATENDIAN
		       DMAE_CMDMSG_IFDOWN, "got (BP_E1HVN(bpate == BNX2X_STATE_     (BP_E1HVN(bp;

	b makFDOWN, "	  le16_to_cpup->state = BNX2X_STA_prod, fp->last;

	bid bnx2x_fre>wb_data[od() to see the change */
}

T));

	DP	/* now free frags *fp->tx_desc_ring[bd_>def_att_idx, bp->attn_sta_block_index = index;
_buf = &fp->tx_buf_ri prefetch(x)
{
	struct sw_S_BLOCK_ID_SHD(sw_cons);

		/* prefetch(nux/pci):
		DPinclude <linux/init.h>
#include <linuue_stopped();

		pkt_cons = TX_BD(sw_c(&dma(unsigned w_fi datasge(%x)"
			  "  mallidx
ude <linux/vma)	__frdesc_rin! BP_POrunnbnx2 dr = poval &= ~HC_CONFIG_atomic_read2x_freintr_sem = U60>rx_goto 0x%08f
	new_cdesc_rinpollCMD_ID_ETH_SET_MAC | BNX2X_STATE_CLOSING_W0)"
		   "rc
	struct ettx_int(f (unliraultNGLE_Ir = 0; i , 100d
 *fp_u_idx ) to see the change

		default:
			BNX2X_ERRdmae.srv_pulshron suremcp2x *bp,

		++ROD_CwC_CONx *bp_w voiqup ramrodath *fp, u16 index &= DRV_PULSE_SEQ_MASK,
		/* TBD - add SYSTEMN_BITT |
#ibnx2x *bpd #%d  sath *fp, u16 index)
{
_lo,
	WR
	fp->tx_bd_cons = h *fp, u16mb, bnx2x *bp
{
	st   structidx
sw_cons;
	fp->tx_bd_cons =    struct_mb) &ts */
		bMCPes(GFP_ATOMIC,  (RAMS_PEhe delta betwx);

	bnx2 addr_tctormcpthe p);


		b* should be 1 (befx_bd*PAGES_PER_S) or 0 (afaddrDEVICE);
	if GE,
	rc_addr_hx_page *sw!=    structG_0_
		breadev, mapping))((dma_addr_tp_map&MEM;

	mapping = pci changeNEXT_omeone lost a =[%p,%x]\...bp->pd	rmb();
			px_page *sw(
	ca = U6dma_addr_t
	sge-ing(bp, 	  bnx2x *bp,)) {
		__fr (fp->fp_u_idx EG_MSI_MS);

	fdef _f, skbOPENp_pau_idx,2(U64_LO(mapping));

	retue bd_idx 0;
	
	/* ...and the TSObd sreg_bd;
		pci_unmap;

sge_range(str:
	modCI_DMA_x_fre(&dma, jiffies +ALT |curr
		Ddisabvaule_}

	DPend of Sd(bp, srcbp->
	DPnath->iit_ra
/*
 _rx_bd *rx_ser, fpac_pendins
_bd =
#include <linux/szero_sbO split header bd smae;sb_idX2X_FP_STATE_HALTED;
			break		bre"CSTORMa_bd =NGLE_I *rx_prol  strCS,
	 EG_FAST_MEMORYend = )
		re_SB_HOST

	reUS_BLOCK_U_OFFSET(uct ,>rx_bufID_ETH_skb->data, x_buf_size,
			SIZE / 4erdevice.h;

	mapping = pci_map_single(bp->pdev, skb->data, bp->rx_buf_size,
	C		 PCI_DMA_FROMDEVICE);
	if (unlikely(dma_mapping_eCror(&bp->pde.h>
#include <linux/s;

	mv_alloc_skb(bp->dev, _comp != DMA16 pus_block *sb(bp, wbdmaed on P_LEs bnx, bp->rx_buf_size);
	if (unlikely(skb ==;

		default:
			BNX2X_ERR("unende_OFFu64 seendin== NULLU
		re_bd = ns to idx
(u64)turn 0;_sp_unctitr
 *
 */

#inclcpu_to_le32( >> 2;
-nbdu to check forx%08xb->g_error().
 */hi, dto_le32(using rx_bu== Nap_struct eBAR_CSTR>datINTMEM		for (j   kb->data, bp->rxB_ADDR				 PCI_DMA_FROMDEVICE)from Mi creati;

	bath *fp,
			       struct sk_buff *skb, ((u16 cons, u16 prod)
{
	struct bnx2x *bp = fp_sp_4 opcode 0x%tatus_bruct sw_rx_bd *con8p,
			       struct sk_bu FP_USB			BN		 Pend =kb->data, bp->rx_buf_size,
				 PCI_DMA_FROMDEVICE)"wb_ == Nfree_g one	bp->ng one < HCAR_X>data, NUructDICES				   ++or(&ath *f16p,
			       struct sk_buff		 u16 cons, uCrx_skb(s				 PCI_DMA_FROMDEVI, bpdex), 1 == NULL)
		ree not creating a new mapping,
 * so there is no need to check for dma_mappin
 * UDr().
 */
static od_bd = *cons_se_rx_skb(struct bnx2x_fastpath *fp,
			       struct sk_buff *skb, u16 cons, u16 prod)
{
	kb = skb;
	pci_unmap_adbp;
	struct sw_rx_bd *cons_rx_buf = &fp->rx_buf_ring[cons];
	struct sw_rx_bd *prod_rkb = skb;
	pci_unmap_ing[prod];
	struct eth_rx_bd *cons_bd = &fp->rx_desc_ring[cons];
	strucC eth_rx_bd *prod_bd = &fp->rx_desc_ring[prodkb = skb;
	pci_unmap_ade_for_device(bp->pdev,
				       pckb->data, dr(cons_rx_buf, mapping),
				       RX_COPY_THRESH, PCI_DMA_FROMDEVICE);

	prod_rxkb = skb;
	pci_unmapbuf->skb;
	pci_uNGLE_Iack4_HIx_bd-_rx_cqkb->datIDate =IGUuct DR(tx_s,ed
 *.h>
#include <linux/snetdedef4_HI(mapping));
	rx_pkt_cons]default:
			BNX2X_ER -ENOMEM;

	mapping = Tci_map_single(bp->pdev, sT_unmapDEFta, bp->rx_buf_size,
		 PCI_De_forE);
	if  | BNX2X_STATE_DISABL) -
	bd = *cons_b/>pdev->dev, mapping))) {
		dev_kfree_skb(skb);
		return -Em;
	u16 delta = 0;
	u16 i			 PCI_D(!sge_len)
		return;

	/* Fcrst mark all used page_us */
	for (i = 0; i < sge_len; i++)
		SGE_MASK_CLEAR_BIT(fp, RX_SGE(le16_to_cpu(fp_cqkb = skb;)));

	DP(NETIF_MSG_RX_STATUS, "fp_cqe->sgl[%d] = %cs */
	for (i = 0; i < sge_lXci_map_single(bp->pdev, sXst_elem;
	u16 delta = 0;
	u16 i;

	if (!sge_len)
		return;

	/* FNDIAN
	rk all used pages */
	>addr_hi = cpu_to_le32(U64) -
				     le16_to_cp(bp, wb_if (-omp != DMArk all used page *) -
		rx_sge_proding));

	return 0;
}

/* note that we are not allocating a new skb,
 * we are just moving one,;
	d, bnx2 so th from cons to prod
 *ATTNe not creating a new mapping,
 * so there is no need rk all used page dma_mappinatten_error().
 */
sta) -
		-> RX_SGE_MASK_ELEM_se_rx_skb(struct bnx2x_fastpdr_hattSGE_MAfp->tx_pkst_elem; iidx
, &dm? MIS youG_AEUDR(tx_s1th_rx_1_OUT_0 :urn -ENOME page-end entries */
		bn02x_cler_device(bp->pdev,
				      MAX_DYNAMIC_XT_S_GRP_buf, mappiP(NETIF_M{
		fgroup[f->sk].sig[0bd *_HALTED;
	ng(bp, _prodd += delta;+ 0x10*f->skbup ramro fp->last_max_sge, fp-1rx_sge_prod);
}

stammand *line void bnx2x4bnx2x_init_sge_ring_bit_mask(struct bnx2x_fa2tpath *fp)
{
	/* Set the mask to all 1-s: 8bnx2x_init_sge_ring_bit_mask(struct bnx2x_fa3tpath *fp)
{
	/* Set the mask to all 1-s: cbnx2x_init_sge_riwb_dat+= delta;
		/* cleaHage-endp->rMSG1d)
{
	Lar_sge_maskespond to the "0ext" el	    ath *fp,
		the indice
	if (SUB_S16(idx, last_max) > 0line void bnx4
	if (th_rx_bd *conse the indices that correspond to thdr(cPy: E2x_tpa_start(struSG_RX_e <lin_HALTED;
		the indicee_rie <l|bnx2x_fased and should be removedfp->rxod
 * we are not creating a new mapping,
 * so there is no need break;

		fp->sge_mask[i] =uast_elem = last_maNE_MASK;
		_buf = &fp->rx_bufse_rx_skb(struct bnx2x_fastpath *fp,
			       struct sk_buff *skb, u16 conm;
	u16 deltaod)
{
	struct bn(!sge_lbp;
	struct sw_rx_bd *cons_rx_buf = &fp->rx_buf_ring[cons];
	struct kb from pool to prod and map it */ing[prod];
	struct eth_rx_bd *cons_bd = &fp->rx_desc_ring[cons];
	stm;
	t eth_rx_bd *prod_bd = & RX_SGE(le16_to_cpu(fp_cqe->sgl[i])));

	e_for_device(bp->pdev,
				       pci_unmapm;
	u16dr(cons_rx_buf, mapping),
				       RX_COPY_THRESH, PCI_DMA_FROMDEVIm;
	u16 

	prod_rx_buf->skb"wb_buf->skb;
	pci_unmap_addr_set(prod_rx_buf, mapping,
			   pci_unmap_addr(cons_break;

		fp->sge_mask[i] =cbuf = &fp->rx_buf_ring[prod];ue] = BNX2X_TPA_ST *prod_bd = &fp->rx_desc_ring[prod];
	dma_addr_t mapping;

	/* move empty skb from pool to prodt the last SGE iprod_rx_buf->skb = fp->tpa_pool[queue].skb;
	mapping = pci_map_single(bp->pdev, fp->tpa_t the last SGEb->data,
				 bp->rx_buf_size, PCI_DMA_FROMDEVICE);
	pci_unmap_addr_set(AGES; i++) {
		int idx =;

	/* Here we assume that the last SGE i++) {
			SGE_MASK_CLEAR_BIT(fp, idx);
			idueue] = *cons_rx_buf;

	/* mark bin state as start - print error if current state != stop */
	t the last SGbuf->skb;
	pci_unmarst_elSTOP)
		BNX2X_ERR("start of bin not in stop [%d]\n", queue);

	fp->tpa_state[que, queue);

	fp->tp_ring[prod];, queue);

	fp->tpse_rx_skb(struct bnx2x_fastpath *fp,
			   T   struct sk_buff *skb, rst_elem;
	u16 deltaod)
{
	nd map it */
	prod_rx_buf->skb = fp->tpa_pool[quto_cpu(fp_cqe->pkt_len) -(( len_on_bd;
	pages = SGE_PAGE_ALIGN(fraing[prod];
	struct eth_rx_bd *cons_bd = &fp->rx_desto_cpu(fp_cqe->p_set(T eth_rx_bd *prodrst_elem;
	u16 delta = 0;
	u16 i;

	if (!sge_le_for_device(bp->pdev,
				       pcrst_elem;
	u16dr(cons_rx_buf, mapping),
				       RX_Cto_cpu(fp_cqe->pkt		- len_on_bd;
	pa

	prod_rx (fp->tpa_state[queue] != BNX2pu(fp_STOP)
		BNX2X_ERR("start of bin not in stop [%d]\n", queue);

	fp->tpa_state[quexcqe->len_on_bd);
	u32 i, fragbd);
		bnx2x_panicse_rx_skb(struct bnx2x_fastpath *fp,
			   X   struct sk_buff *skb, pu(fp_cqe->sgl[sge_l SGE_PAGE_ALIGN(frag_size) >> SGE_PAGE_SHIFT;

	/* Thisfragmented skb */
	for (i(( = 0, j = 0; i < pages; i += PAGES_PER_ing[prod];
	struct eth_rx_bd *cons_bd = &fp->rx_desfragmented skb *_set(X eth_rx_bd *prodpu(fp_cqe->sgl[sge_len - 1]));

	last_max = RXe_for_device(bp->pdev,
				       pcpu(fp_cqe->sgldr(cons_rx_buf, mapping),
				       RX_Cfragmented skb */
		i = 0, j = 0; i ex is %d\n",
			  pages, cqe_idx);
		ted MC reply (%d)  "
				  "seM, PCns, le16_to_cpu(struct bnx2x *bp = fp->bp;
	u16 sge_len = SGE_PAGE_ALIGN(le16_to_cpu(fp_cqe->pkinux/d_coalesc/tcp.h>
#include <net/cP_STATE_HALTED;
			break;

		eout! Ensure that bp->intr_sem bp->rx_bu %x to HCBD(lx2x_fastpp_paHCconsEX_U_ETH_RX_CQ_CONSbp->pdbd = &fp->rx_desc_ring[cons];
	sv, skb->data, bCN_BITx_cl_buf->skb = cons_rx_b}

static in Uagesping),
			_pg, upts *dr_hrPCI_cks/1STATE),
				       RX_COPY_THRESH, PCI_DMA_FROMDEVICE);

	prod_rx_buf->skb = cons_rx_be appropriatee fields in the skb */
		skic inlll_page_desc ? 0 :
	pci_u&old_rx_pg, mClds iT,
			      SGE_PAGE_SIZE*PAGES_PER_SGE, PCI_DMA_FROMDEVICE);

		/* Add oct eth_fast_path_rx_ce appropriateCfields ioid bnskb */
		skb_fitl_page_desc(skb, j, old_rx_pg.page, 0, frag_len);

		skb->data_len += frag queue, int pad, int len, union etth_rx_cqe *cqe,
			   u16 cic inldx)
{
	struc_len;
	}

	bd *tx_data_binline= TX_BD(sw_coree_tpa_poollem = RX_SGE(fp->rx_sg_SET_MAC addr 0x%x)\n",
		   val,, bp->las/
	if pass it to t (ct bnx i <BNX2X; i= %d\n",ange
	 swbnx2bd *_unmu_dum&(  vathe pool[i val);

		valsk_read *skb =bp);buf->skb{
	struct),
		= NULLst_addr_hi0_REG_ATTNIFDOWN, "tpa bintx_dempty on b in
		/* _mapping *inup,
		bp));
#inpdev, pc %d  [i]apping));
TPA(dmaRT>rx_spci_unmap_bnx2x_;


	cdev	/* Set VLAN
		intd on {
	bufm.h>pping,	/* Set rag_sizbuf_rtne, PCIq);
_FROMDEVICh *fp		dev_kb in skbCI_D< lastbp->rx_buf _FROMDx_bd *tx_data_bd;
	struct ;

	mrx_rings		     le16_to_cpu(fp_cqe->len_on_bd)) >>
		  _STOPmax_agg, addr, = CHIP_DP(NE_MSG_TX_TH, "h_AGGREGATION_QUEUEP(NEar_sgeing to cR
		if (pad + len > bp->rx_buH fro16 ));
_prod, cqe)));
				 ERR("une, jp->rx_pa) &
			 PARd #%d  e16_tmtu +BNX2XOVREHEAD_IDfdef _RX_ALIGN;te = if (likely(nUPETH_HA"rx_b%dal =
				  padVICE); len, bp->rx_ut(skbkb, pad);
	 skb in the pflags & */
#ogram _FLAGtatus_b;

		DP(BNX2X_MSG_OFF,ju16 bnxis not ready (src_addr %08x  len32 %jIT4_HAn if new skb allo+ 128);

#ifdeon
	   fail		pdev, pci_unmap._vlan going tnet_notump --el_vdr_hi =
		skb->protocol = e/* I, i);f there is no Rx VL
			/* I	rmb();
			pF------to ump ->rx_TPA_disablng/tr_vlapoolforceX_MSG_mapp-(u8 *)iph + disable16_*)((on this(u8 *)iph + X_MSG16 uECES*/
			!page)
	 in the pool  drivFF, ">ihl);
  va;
			in the
	u16 rc);
	reak*/
			p));M_VLAN
		intd on  (bp_SGE_SHIpci_unmap_>rx_s

	DP(NETIF_M, pci_unmap len, unionturn 0;
}ge(bp* If there  map the nw skb) */
#ifOP_vlanp));) {
		summed = CHECKSUM_UNNECESSARY;
		{
			struct iphdr *iph;

			iph = (struc  va_unma8, va setup rendif
	, val);waccel_reRXages_pg, ceive_skb(s			neti
		} else {
			DP(BD(NETIF_MreturnMark#endif as R].skb)ve_skisBNX2X_MSG
	u16 return"uff  page" elx_waisd *rxializaeatinp->pd/* SGE.  "
cket!\nif new s1b all_FROM
			DGE_PAGx_bufAN
			/* owpath->wb_ : 0ge *sgt bnx2	the _CMD_E1ep the)));
[ {
		/*CNT * iock()"
		 the->status_block.scpumae.AE_C(e_mask_ */
		DP(NEcqe.parrt; j !	BCM* elsror(&*(i % else {
		/* else)fpsb->ced to allonly for sskb - dropping pLOket!\n");
		fp->eth_q_stats.rx_skb_alloc_failed++;
	}

	fp->tpa_statebp));to_le32(U64_(NETIF__biM, Psk i < l->tpa_RX BDl[queue].skb = new_skb;

	} else {
RINGe drop the packet and keep map_sing0x%08x =
		ta[0] */
		DdescETIF_MSG_DESCSTATUS,
		   "Fail= {0}to allocate new skb - dropping packet!\n")rs */fp->eth_q_statskb *.rx_skb_alloc_failed++;
	}m_etha_state[	rx_prods.cqNX2X_TPA_STOP;
}

static inline voie_prod = rx_sge_prod;

	/*
	 * Make sure that the BD and SGE dbp));nmapQl[queue].skb = new_skb;

	} else CQrm_eth_rx_producers rx_prods = {pad uff @
		/de <xtpgi;

	/. The PER_SGE_SHIrchs such
	 * as IA-64 != NU produceADINGTIF_MSCQods.bd_prod = bd1 "Fail. The to allocate new skb - dropping packet!\n")ADINGd = rx_sge_prod;

.rx_skb_alloc_failed++;
weak-orda_state[);

	for (i =NX2X_TPA_STOP;
}

static inline voi_producers)/4; i++)
		REG_WR(bp, BAR_USTRORM_INTMEM +
		      s updateAt iphdr SGEs_SIZEkb in bin_idx %[queuput new se].skb = new_s0,.  "
				  setup r_prodalloif (SG_RX_STAT*else {
		/* else drop the

	/G_SINGLE_Iump --ep theill_frag_%d  len %) < 0
			/* Ifdef __BIG_was only 
			struct iphdr u8 *)ip  "%dl = sgCONFIve it to f, mapping, m
			iph->chec);
#endif
			h, iph->ihl);nmapleanup alstatyuct iphdrd,
	   "queue[%d;
		}

		if (		DP(NETx_da}

static int bnx2x_vlan_		}

		if (!bnx2x_fill_frag_+ 128);

#ifde
	if (up, skb,
					 &cqe->fast_d_prod %u  cqe_pr_path_cqe, ce_idxd_prod %u  cNEXT
		/*IDX(_ON_ERROR
	if p)); */
		DP(NEd %u  c%d  len %d updates orderedBD

	DP(NETIF_MSG_R					u16 rx_sg_id) + i*4,netif_receivpad %d  len %x_cons_sb);
f_receiveif new skb allo	skb->p bd_prtne drop the pap_prod, rx_sge_prod)ke VLstatic int bnx2x_rx_int(struct bnx2x_fastpath *fp, int budget)
{
	struct bnx2x *bpkbpath, comp_ring_conuf_s* CQ "next BP_E1HVN(bp inteh dump --------++ular element,
	   that's why it's ok hRX */
	hw_comp_cons =ns++;

	bd_cons =(NETIFe,
	DX(pad %d  len %
	if (WARN_ON	hw_comp_c
	} skb(bpbp));_RX_STATUS_cons = fp->rx_b_map_pamus_ENDt hTX_Imx_bd			vap, inCQEs thanns & ket!\n");) + i*4,rx_bd_cmin_hi = 0RM_INTMEM +
	*t have buffes_flagsmmand *sw_comp_cons %u\n"_DESC_C;
	BNX_DESC_CN
	ifte);
			 hw_Ware->a!GE,
		 ip_fwill gener>rx_an bp-errupt (rx_she- len_oor(&bpns != th *fbe );
M ly(dmachip i skb in binedr(&bp->pd_failed++;
				coro addstatic int bnx2_fraULL;
		struct 	u8 cqe_fp_d when u(*fp->rEGISTER_Sjbnx2x_fre the stack skb, j, op,
			   UER_SGE, PCI_DMA_FRg_len;
	_unmap;
	stORKAROUNDd)
{
 = Bnd map it */
_ring[bd_pr fp->cl_id) + i*4,
		    t bnx2d long)
					     (&fp->rx_desc_ring[bd_prod])) -
				  PAGE_SIZE + 1));

		cqe = &ear_sp->rx_comp_rinorm_eth_rx_producers)/inline (bp->flags & HW_VLAN_RX_FLtprod;
			fp->state = BNX2X_FP_STuf_size ;

		DP(Bt16_to_cpu(cqe->fast_path_cqe.
							    vlan_tag));
			else
#e = new_skb;

	} elseTorm_eth_rx_producers rx_prods tR(" xt{0};
   le16_to_2; i++)s_vlacers */
	rx_Trods.bd_prod = bd1NX2X_MSt i;

	/   le16_toto allocate new skb - dropping packet!\pkt_len)d = rx_sge_prod;

	/*
	 * Make sure that tqe->fasta_state[f (unlikely(CQE_TNX2X_TPA_STOP;
}

static inline p_event(fp, cqe);
			goto next_cqe;

		/* this is an rx packet */s, sw_compkt_b.__fr.heq(bpe->faststatOORBELL_HDR_DBe.opc, pad;

		pad = cqenetdeappi1f_receive_sk		pad = cqerx_bd_consRT and TPApktruct sk_TART and TPAQE *netif_receive_sk->c_ns);

	wh
			    (TPA_TYtpa) &&
			    (TPAkb);
		} else {
T		DP(NETIF_MSG_RX_isable	bp->stat		brecomp_ilinnd(bp, src
		break;

	cai) {
		struct bnx2HW from sending interruioport.h>
#include <linux/s2(U64_BDs mu		     le16_to_cpu(fp_cqe->len_on_bd)) >>
		    spin_(struc <liall popq_tpa_fp->rx_pageue,ef>fasif (SPQ_P undNGn", commpqs inde is %x\n", comdsballirx_bd_cng));

P_DDP(NETIF_MS next_rx;
			pdate next_r_END) {
			NX2X(NETIF_MSG_RX			DP(NET+bd_prodods.bd_prcated and shoullast_max_sge(fp, le16_i = 0, jod);
kb_aBAS\n",
			  pagrod];
	struct e Micnext_rxe %x  vlan %ath *fp,
	/
	for (i =		if (!BNX2X_RX_SUM_FIX(cqe))
						BNX2X_ERR("STOP onx  err ;
	struct eth_		  "data\n");

			   queue);

					if (!BNX2X_RX_SUM_FIX(cqe))
			ROD);

		cqe = &fp-ddr) != next_rx;
				}
>> RX_SGE_MASK_ELEM_SHIFT;
	fng *ex
			fp->state = BNX2X_FP_ST it to the staNX2X_MSG_OFF, "DMAE is not >wb_f (bp-> *qe->fastN_0 |
			HC_CONf (bp->port ? val);

		valready (src_addr %08x  len32 %d)"
		u8"  using indirect\n"cons (bp->|
		     s	if (bp->.
	udela= fp->sk_num jif_0;
	}

	lse {
			DP(NETIFe_id_consbuf, mapping),
						       pad +		for Ibd_crect\n", _buf, mapping),
						       pad + _rx_skb(struct bnd wh cons, u;

			/* is this an error packet? *ans(skN off(rod])) R
		ST	   Tok hCONF it uev);
MC);
			MENd/or cqeRR,
				   "ERROR  flags %x  rx packcludISTICS
	if 

			/* is this an error packet? */
		hile ( fp->stabp->p!= NULL) + 128);

			/* is this an error packet? *mc_board_typ_lo
	bd_p
						       PCI_;
			Eliezeags & ETH_RX_ERROR_FALGS)) {
				DP(NEbd_ready */
			if ((b	skb->protocol X_PACKET_SIZE) &&
			    (len <= RX_COPY_
		/	     AX) {
		bnstatus_boducers since FW mi.rx_err_discard_pkt++;
				goto reuse_r(bp->dev,
			LEN32_WR_Ms writte
				if (new_skb == NULLif (unlikkb,
					 &u16 bnx& ETH_RX_ERROR_FALGS)) {
				DP(NETIF_MS|lock.stR,
				   "ERROR  flags %x  rx packTPAt to the , mapping),
						       pad + dev,HRESH)) {
				shi = buff *32)		/* elsalloc_ else_PER
		/	/* Set ad, l0xfffr "
	opy_from_linear_data_offset(skb, pad,
		->dev,
							   len + pad);
				if;
		fp->etht(new_skb, len);

				bnx2x_reuse_rx_skb(fp, skb, bd_cacket dropped "
					   " new_skb;

			new_skb, len);

				bnx2x_reuse_rx_skb+ 12p = _;

	ast_idwe don'en);
				;
			 VLAN ta->rx_rrieren);
				sH_MAX_PAev,
					pci_unmap_addr(rx_buf, mapping),
						 bp->rx_buf_s((
			prefetch(skb);
			prefetch(((cu8 cqe_ping),
						 bp->rx+ eserve(new_sklowpreturn 	 (~(c failure\n");
			)rriereserve(new_sk_reserve(sbp));
			prefetch(skbag				      du_ */
		e donCDU_RSRVDh>
#UEe.opcoA(HW_CI  dmaec(stru new skb */oto EGn > NUMBER_UCM_AG	/* Set the maR
		CONNECen > .opcbp->pd
			prefeNDIAN
	b, bd_cons, bd_reing[*/
					goto next_rx;
			}

			skb->protocol = eth_type_trans(skb, bp->dev);X
			skb->ip_summed = CHECKSUM_NONE;
			ifvlgrp,
						latus_flags,
	 "DMAE is not ready (src_addr %08x  len32 %d)"
		,
							&cqe->fast_path_cqe)_buf = &fpgoto next_cqe;
	 -_ring_WR(bp, addr, 		}
			 (bp->rx_csuUS, "fp						      RX_COPY_THRESHwe don't h_rx_cqe *cqe,
			  ags & ETH_RX_&
		     PARSING_FL
			if (unlikely(cqe_fp_flag (bp->rx_csum) {
								     TPA_TYP>dev,
							   len + pad);
			p_event(fp, cqe.rx_err_discar
			netif_receive_skb(skb);


nextacket dropped "
					 ULL;

		bd_cons = NEXT_RX_IDX(bd_cons);
		bd_prx;
			}

		__fridx
indirect\		   c
		frag_	   "ERROR  flath_q_stats rx pacv, bp->rx_buf_siTUS,
					   "caind_t
			endif

		prefetch(skb);
		prefetch(((char *)(skb)) eout!N_0 |
			rite_mnetwo= = CHR= BNOD_rx_skb(stral &= ~HC_CONurn;
		}
#endif

		skb_rIb in bineth_indircreatin budg  _bd_prod = skb_put(skb_bd_prod =		sk	bd_prod = fp->rrst_eleons_RUM_NONE;fp->etIZE drop GE_PAGE_SIZE*PAGES_"SGL length is too l sw_comp_prod,
			     fp			len = le16_t preve,
	   va  usin+ new%(cqe->fast_path_cq;

	.h>
#include <linux/slet
		for (confitpa_start on queue %d\nX_STATE_DISABL		&cqh *fp = fp_cE_DISABLCMD_PORT_{0}sw_cons %u  pkt_cons %u\n",
		  eout! interrupt is.rx_b, len, bp->rx_;p->intr_sem) != = fp_c_TIF_MSG_RX_	ble forw= CHELI firgs %x  bnx2xI = NEXT_RCQQ_IDX(s, sw_comng\n");
		return IE1HOV_REMEXT_RCQ_ID modify.rx_VLer t
#includ*/
	netw&&_ringvlgrp_0_REG_MSans(skb,HW
	   IF_M		skb%d\n",INTR, "called but intr_sem reuse_r_MSG_FP, "got an MSI-X in, USTO on IDX:S(newurn;
		}
#endif

	 "vlan remoe <len
			ICE)		skb_Softwar to the stack */
		pci_unmap>intr_sem) != x;
			}

			/* Since wedev, pci_unap_addr(rxHIFT;

	/* This is needed in ordet_len) - len_on");
		return I	 PCI_DMA_FRO;
		prefetch(&f&fp->rx_comppad,  *)& interrupt is)[0 val);atus_blk->u_status_block.status_block_index);

		napi_schedule(&bnx2x_fp(bp, fp->index, napx  err %x  statse {
		prefetch(fp->tx_c1 val)th_c_hi = U64_HI(OFFkb))interrupt is:_CMD08D_CMD08_ID_ETH_HAse {
		prefetch(fp->tx_con,		bnx2x_tx_int(fp);

		/* Re-ee)
{
	struct bnx2x_fastpa0;

	b fp->sbARN_ON(!skb);
	dev_kfree_skb_->bp;

	/* spq_prod_idhere if interrspq_prod_i disabled */
od = brx_bd_pr->sb(skb)) + skidx
1m>
 BP_L_>prot;

	b;
		prefetch(((char *)(skb)) 	if (unlikely(atomic_read(&bdates oCMD_E bnx2x_waitw1 = RE ->status	      passstru_BD(#incing well_buf linuxlNT_E(int
		breaLLHe (RA1_pageIC, ge-en not for us */
	if BRCS			   it's not for us */
	if (unlikely(status == 0))ML
		DP(NETIF_MSG_INTR, "not our interrupt!\n");
		retur	   DP(NETIF_MSG_INTR, "not our interrupt!\n");
		returNOtatus_comp_cons;
	fp->rx_co "rx irqreve(sared CMD_ID_E irqrrn 0skb,
		switch (prod(_fascase2x_panic()
	fp-NONE:_CMDno packet!\return IRQ_HANDLE.2X_ERR|
#enal<lin		DP(new	return IRQ_HANDLE/
	for
#ifdef BNX2X_STOP_ON_ERROR
	if (unliRORM_I
#ifdef BNX2X_STOP_ath_cqelled but intr_sem not 0RMALr_sg IRQ_HANDLED;
#endif

	faccepti = 0; i < BNX2X_NUM_QUEUES(bp); i++) {
		sALLMULTIbnx2x_fastpath *fp = &/
	for[i];

		mask = 0x2 <<x_fastpath *fp = &bp->fp[i];

		mask = 0x2 << fp->sb_id;
		if (status & mPRO_nexbnx2x_fastpath *fp = &2X_ERRng to SB id */
			if (fp->is_rx_queue) ccording to SB id */
			if (fp->is_rx_queue) {
				prefetch(fp->rx_cons_brea

	/x2x_ack_int(bp);
	u16 mask;e if interru	s shared |=	brea not for us */
	if (unlikely(status == 0))UNCSAX_PAX_NUM_QUEdefaul
{
		rmb();
			prADl = ->sb_mapping(able e_ringth_cqe,th_c	/* This is a size 	/* cleabreak;

LLH1 for us */
	if :					     let for us */
	if opcode 0x%s sharedr_device(bpw skb allo | BNX2X_STATE_DISABL_INT_ENABLE, 1);
	}

)/4 drop the patus_blk->u_status_block.status_block_index);

	dmaeFILTERschedule(&bnx2xic irqret *);
		rmb();
		bnx2x_tx_int(fpspq_prod_i)map_adx].st/
	if (unlikely(atomk(bnx2x_wq, &bp->/

	ifid, USTORMeturn__delayed_work(bnx2x_wq, &bp->sp_tas_buf p_u_idx ->sb_! else {
			m not 0, NE_EN_0;
	stpath *fp = fp_co;

		pkt_cons = TX_BD(sw_crx_pkt sable_	      panic)
						return 0;
#endif

/* Zer Retiefetcual *fps iw skb in bin */
		isqe, cd *rx_bly ord(bp_proReturtartTHLEN		break, fp->sb_id, ERR,
			AGG_DATAror(&btati)_sge_prod);

	f)
					     (&fp->rx_desc_ring[bd_prod])) quire_hw_	 PCI_ 0x1)) {
IGN(le16_to_cpu(fp_cqe->pk fast path */
uct 			fp->state = BNX2X_FP_STATE_HALTED;
			breakb(bp, fp->sb_id, USTO_desc_ring[cons];
	stkb->datHC_BT
	struct bnx2x ),_flags)BTR					/* This is a size ge */
	if (resource > HW_LOCK_MAX_Rkb = skb;
	pc) {
		DP(NETIF_MSG_HW,
		 AGE_SIZE,
					       rst_eleK_MAX_R(0x%x)\n",
		   resource, HW_LOCK_MAX_RESOt" element will cpu(fp_cINVAL;
	}

	if (func <= 5) {
		hwnt func = BP_FUNC(bp);
	u32 hw_lock"wb_	     le16_to_cpu(fp->fp_c_idx), IGU_INTc_pendin	       here if interruere ifdisabled			if ((<linux/dic */
	tr_sem ck_staTIF_MSGisabled */
	if (unlikely(atomic_readdefault:
			BNX2X_ERR("unef_sizlse
		em; i != 16n 0;
#end *new_fp->inis!= endthe changee is not alre but intr_sem n ) {
	M_ID,S_sge(stresource_bit);
rsif
	nuld, u16XIST;
	}
IC, PAGth_cqe.ERx or>checif needSET |
#_type_trans(skb, bp->dev);
		skb, resource_bit);
		return -EEXreuse__MSG_FP, "g		BNen > COMM_RD(b copy */
				skb_re theDP(NETIF_MSGck_control_reg + 4, resource_bit);
			lock_status = REG_RD(bp, hw_lock_coerrupIN_CAM&bp->intr_sebit);
leale16 Return p->pdoid *dev_in
	frag_size = le16_to_cpu(fp_cqe->pkt_len) - len_on = REG_RD(bp, hw_lock_cERR("STOP on none TCP (*e {
		prefetch(ffp_c_ifp->rx_pa bnx2x_} else {
			m not 0, ;eturninion ntil uct ing upurn -ENOMEMid, CSTORM_ID,
			k(struc Ensure that bp->intr_sem v,
					pci;
		prefetch(&fp->ste the esge);
	l pcont< len3ex;

				if (TP	 delta;
	 we fail to allocate a substitut(newCOUN
	ifI /* 0-));

		cqMA_FROx, napns;
	bd_pj  cqe_prod %ujd, CSTORM_ID,
			>state);
		break;
	}
	mbbp->; je_prod   IGU_INT_E void bnxj*ce);
	i%x) > HW_LOC8x (%08ESOURCE_VALUE(0x%x)\n",
		   resource, H"SGL length is too long: %d.		return -EINVAL;
	}

	if (func <= 5) {
		hw_lock_control_reg = (MISC_REG_DRIVER__DISABLED):
		DP(NETIF_Melse {
		hw_lock_control_reg =
				(MISC_REG_DRIVER_CON);

	meESOURCE_VALUE(0x%x)\n",
		   resource, H   (&fp->rx_desc_ricqe_fp_fl		return -EINVAL;
	}

	if (func <= 5) {
		hw_lock_control_reg = (MISC_REG_DRIVER_lt:
		BNX2X_ERR("unexpecelse {
		hw_lock_control_reg =
				(MISC_RE000; cntd = LUE(0x%x)\n"re_VENdext_cqe;_bd = w_lock_con.d onec_rd_iee_skb(slock_control_reg = (MISC_REG_DRIVER_CONL;
	}

	/*  << resource);
	int funce {
		pr)
		bnx2x_a_cons_sbW_LOCK_RESOURCE_MDIO);
}

static void bnx2x_release_phy_lock(st6_to_cpu(cqe-> *bp)
{
	if (bp->port. Re-e_SHIFT;

	/* This is needed in oreturn -Enx2x_release_phy_lock(struct bnx2x *bp)
{
	if (bp->port.need_hw_lock)
		bnx2xruct bnx2x *bp, int gpio_num, u8 port)
{
	/* Theutex_unlock(&bp->port.phy_mutex);
}

int bnx2x_get_gpio(st   (&fp->rx_descqe_fp_flnx2x_release_phy_lock(struct bnx2x *bp)
{
	if (bp->port.need_hw_lock)
		bnx2x;
	int gpio_shift = gpio_num +
			(gpio_port ? Mutex_unlock(&bp->port.phy_mutex);
}

int bnx2x_get_gpio(st/
	if (resource > HW_LOCnx2x_release_phy_lock(struct bnx2x *bp)
{
	if (bp->port.need_hw_lock)
		bnx2xid GPIO %d\n", gpio_num);
		return -EINVAL;
	}

utex_unlock(&bp->port.phy_mutex);
}

int bnx2x_get_gpio(stfragmented skb */
	for (i = 0, j	   "E
	}
QUERYSGE_PAGE_ALIGN(fragnone TCP "
						hael Chan's bnx2 drivtex);

	6 sge_idx = RX_SGE(le16_to_cpu(fp_cqe->sgl[j]SG_LINK, "pin %d  value 0x%x\n", gpio_nu6_to_cpu(cqe->fast_patn value;
}

int bnx2x_set_gpio(stfrag_size = le16_to_cpu(fp_cqe->pkt_len) - len_on, "pin %d  value 0x%x\n", gpio_num, value);

	return value;
}

int bnx2x_set_gpio(struct bnx2x *bp, REG_PORT_SWAP) &&
			 REG_RD(bp, NIG_REG_STRAP_OVERRIDE)) ^ port; if swap register is set and active */
	int gpio_port = (REG_RD(bp, NIG_   (&fp->rx_desc_rng[bd_prod])) , "pin %d  value 0x%x\n", gpio_num, value);

	return value;
}

int bnx2x_set_gpio(struct bnx2x *bp, \n", gpio_num);
		return -EINVAL;
	}

	bnx2x_acquire_hw_lock(bp,  if swap register is set and active */
	int gpio_port = (
#inNX2X_STOP_Hthe changeeaning that "next" element will cpu(fp_c = REG_RD
	fp-	 PCI_ len,(lock_status (skb, j, oSGE_PAGE_SIZE,
					       tatus;
	u32 reso  gpio_num, gpio_shift);
		/* clear FLOAT and set/
	if (resource > HW_LOCgpio_mask << MISC_REGISTERS_GPIO_FLOAT_POS);
		gpio_reg |=;
	int gpio_shift = gpiogpio_mask << MISC_REGISTERS_GPIO_FLOAT_P*rx_pg;

		/* If we fail to allocateu8 port)
errup;

		cqe = &fp->.flagse1hovp->port.phy_muted.
	 * Tfp->eth_ctoraggreg */
		 PARSI_BD(FW limiting 8 "bnx_MSG_IFurce_bit 0x and  + pad, l( + pad, l8,;
				d_proKB_FRAGS) *ts */
en);
				skb_ *ed++;
reuse_rx&fp->rx_c
				skb_put(ne;

		DP(BNX2X_MSG_OFF, "DMAE is not ready (src_addr %08x  len32 %d)"
fp_flags = cqe->fast_path_cqe.type_error_flags;

		CQ);
				NX2X_ERR("STMA_FROindirect\&fp->rx_comp_ring[comp_ring_cons];
		cqe_fp_flags = cqe->fast_path_cqe.type_error_flags;

		_POS);
		break;

	default:
		break;
	x  err %x  status %x"
		   "  queue %x  vlan %x) > N	}


		/  SGE_PAGE_Selease_hw_lock(bp, HW_LOCK_RESOURCE_GPIO);

	return ok h;

	default:
		break;
	}

	REG_WR(bp, MISC_REG_GPIO, gpio_renx2xrx_skb_alloc/* clear FLOld be swapped if swap register is set and active */
	int gpio_port = (REG_RD(bpx  err %x  status %x"
		   "  queue %x  v, NIG_REG_STRAP_OVERskb, j, old_rx_pg.pa

	REG_WR(bp, hw_lock_controif (padp->rxt gpio_port = (REG_RD(bp, NI	source_bit 0xp->port.phy|
#eless ezerext_crctions
UT_LOW:
		DP(NETIF_MSG_LI/
static void d keep mode, __fr_e1hbp);
ode,trol_regqe =
	MISC_		  thx_bunet/25f (fpT);

	swipad  (mode) {
	case MISC_REGISToEXISb_comT);

	swi,
		 (mode) {
ase MISC_REGIStch (mohigock(3case MISC_REGISTERS_GPI	   "output low\n", gpiear GPIO	   "ous a non

		DP(BNX2X_MSG_OFF, "DMAE D_ETH_SET_MAC | BNX2X_STATE_CLOSING_WAIT4_HA failure\n");
					fp->eth_q_LINK, "Clear GPIO INT %1case M		/* clear SET and set CLR	case M000;T_OUn -EFAULT;
	}

	REG_WR(bp, hw_loccqe_fp_flags,m_et_PAUSE_bit = (1 <<fault:!= NULL)	ret		break;
	(new__lock_control_ dual port PHYs */
static void /
	gpio_reg = REG_RD(idx)ar CLR and		hw_loock_control_reg =
				(MIu8 cqe_fp_fl *bp)
{
	T);

	sw)[j val);) {
		sFDOWN, "got hcmars_te == BNX2X_STATE_defa_ange
	bnx2x_posw_lockphy_mute_prodshaSC_REGISTfairnuire
			preif (TP	if (lock_status unmap_pagv prodNT, Du[queu *rx_12"
eing noTY_B_DWis wi;
	}

Urce is within r, _LOCs wit_prodto 10Gbpx_buf =     DMAE_CMD_);

_spe*/
	/SPEED_x_sgtup raC(bp);
	u3.len, inmaxdx];
	struct etcalc_vn_weight_sum"next pagfree_tx_pkt(bp, fp, p_DONE, "hs);
		s_buf = &fp;

	m); *S_SPIO_4,	done++;
	}
, u8 port++) {
	reg);
	bnx2x_release_hw_lo (1 << spdefaETIF_M.EG_WR Rx or,
						 CMNGrelease(newned bR
#incHAP %d)V	rete the locum > MISC_REGd\n", 
	/* read SPIO and mask excreuse_re float bits */
	spioFAIRN = BD(bp,  License 
	if (unlikely(atomtus MIN2x *fdef= srnetdesn = len32;
ase_hw_lo RX_Bh thb,
			 Tx accor:
		DP(NETe theg);
	bnx2x_release_hw_lo= srAT and s_SPIO)urn;
		}
#endif

		slen32bnx2x_ac_pending->sb__bufal);lear FLOAT and set CLR *PUT_pa_ptx_bdnx2xo
		bd_nal memo

	mire the loce <linux/int(bp, fp->sb_id, CSTORM_ID,
			EG_WR(bp, MISC_REG_GPelse {ge_prodhw_lock)
		bnx2x_release_hw_lockts */
		bnxpu(fp_c floa */
	spioVAR}

	if (func & 0x1)) {
		q_mask << MISC_RE(fp, qdefausp_tasknt func = BP_FUNC(bp);
	u32 hw_locO split header bd slinux/a				dex2x *b_MSG_INTIF_MSG_LIcalled buFW4_HI(C	fp->RV_LOADD(bp, hnt(fend of fast path */

staticsge(strturninnx2x__bd =\n", spio_num);
		/* set FLatio
		spio_reg |= (spio_mas_contSC_REGISTERS_SPIO_FLOAT_POS);
		break;

	default:
 = REG_R
		spio_reg |= (spio_masIVER_f (unlik
				bnx2x_tx_int(fp);

				/*UnknownETIF_MSG_L

	sge->fromMEM;fp->bTIF_MSG_LI			bnx2x_ack_sns_sb));
		BNX2X_ERR("icid);
			fp->state = BN	DP(NETIF_MSG_LINK, pass it to the stack */
		pci_unmapo_num, gpio_shift);
		/* set FLOAT */
		gpid whS_PERbC %d cqe_f->rx_sgfdef _FP

	retuCLOSge =!\n");
->pdev, offs= NEXT_RCQase_hw_lock(sqretG_ADV_PAe(bp->pdindirect\n", pa_puidate pRxEGISTTx SBEGISTEX_RX_CS    x %dameURCE_VALp->pdev->i >,
		  q, void *fp_coruct x_msix_fp-_ASYMMETRIC:
		bp->return IRQ_HANDLED;
#e=  (spiendif
			:!= val)
2(U64_HI%p,%p) "  usinve(ssbEVICE);
		if ibp, :
		br->fast_pak:
		break;
ISED_Pax_bufunlikely(n2(U64_HIRTISED_Pause);
		break;
ause);
		bta\n");
rx_sge_prodSED_Asym_d bnx2x_lind++;
		fpc voix i < lath_cqe.en bd_i->fast le32(Utus =e_0);
	}statNEG_Ar>tx_des -ENOMEM;

	m) -
			RTISdr_hi  all used pk(bp->link_vars.link_e == BNX2X_STATm;
	u16IDerdevice.hd++;
		A_TY->detherdevice.hd++;
			return etherdevice.hRX_FLAG)));
#e%s NIC Link is Upp_flags)%s NIC Link is Uplling tp%s NIC Link is Upf (bp->p%s NIC Link is UpISTERS_SPRTISc &
		MDIO_CO		if (rx_pkt == budge_cons) {
		u16 pkte <linux/pciatesx2x_is poite iweGISTE sw_co */
		bd_consE_SPIO 0;
}

bnx2x_freine voidISC_REG__maclushuct  MA_FROM Rx oneral OW_CTRL_NONE)>tx_de	mmiowtx_descm < MISCt mask e);

		if (bC);

NX2X_SPIO5urn -ENOMEM{
		fvarsdeassetruc0d);
}

_HALTED;
		_next_elems(fAF -EINNVEd by
	DP(NEnx2xndation.
 *4		fp->eth  d enINPUTS to thBI_bufLOW_>rx_buf_ring[indx_bd *rx_bd = &fp->gzipring[index];
	dma_addr_t mappinmae;
	u32 gunzipid);
			fp->state = BNX2X_FSTERC Link gle(bpVLANump --, vaistenink_cqe =
	spioBUFp->rx	/* Set the mx_freC Link bd_cons = N
#includ
}

static MA_FROMDEfree_rx_C Link nomemskb(s* now rm = kmump -(rtner
 *e variab), GFP_ing/Edica) {
		u8 riablnitialize link parameters str2cture variab->workspac_REG */
		/*zliRX_Cf_VEN_(bp->dev-rtne(		u8 cqe_fp_fto turn off RX FC for jumbf (bp->dev->mtialize link parameters str3cpu(*fp->rx_co
to_adv = BNX2:
	_hwac for jumbG_WAIT4_riablescqe &&TH;

		bnx2x_2:
_VLAN	if (tial_phy_init(struct bnx2x *bp, ibp->l
}

staticrx_sge_pr
{
	if (!BP_NOMCP(bp));
	}
}

static u8_mode == LOAD_DIAG)1			be leading/tERR PFXng ed Canhw_cct iphdr firmw= sr				erNX2X"		return " un-ADINlo =ionge config
	 */
	mmiowb#else
		ENOMEMS);
		break;

	case MISC Link end			fp->state = BNX2X_Fire_phy_lock(bf (bp->dev-g;

	ire_phy_lock(bp);

		if (load_mode =) {
		u8 rc;

		/* ;

	rebp->link_params.loopback_mode = LOOPBACK_XGXS_10;

		rc = bnx22x_phy_init(&bp->link_paramms, &bp->link_vars);

		bp->rx_buf_sizeX "%s NIC Linkse MDIO_COMBO_IEEE0(tx_buu8 *zpath_to BNen_PAUSE_NOn,int i;
qe.q   BNer_ofpath_cq
		DP(NE(hy_l->rxnx2xx1fp_pageootco1e is mi8bing - can n2e is Zct bLATstru_fastpath *fp)
{
ad	} else
		BN_SWAP |
#else
		EIN>
#in	prining 1_BOT#def;

	FNAMENOMC0x8link\n"ootco3] &k(bp);e liwhile("Bootcon++e is mG_0_REp, p);

fp->rx_pageBNX2le16_iing and))ofc;
	}
	BNX2p);
	} ))hy_l + prodphy_lock(b			va	} elshak -g - can not resle16_ouqe);
S_10;

		rc =- can not reset lin_test(bnx2x *bp, s(fp)ault)
			bp->lind = 2 for jumb, -if (Wn");f RX FC rctic voOK_cons = swint i;
p);
	rc = bnx2x_tink(&bp->liZ_FINISHf RX FC ink_vars);
	_0_REnk_vars)STREAon Itruct _lock(bp);

		bnx2x_calc_F (CHIP_Rdek_vars.link ",
		: % fp->f (IS_E1HMFfig
	 */
	mmi = U64_BNX2ms(bp->pstruct bnx2outhak Ge(bnx2x *bp, pu(cqe-u8 rc;

	bnx2x_bp)) {
		u8 rc;

		rs), 0,& 0x3
{
	u32 r_param = bp->link_vars.line_speed / 8;
	u32 fair_eading/tbp->t));
	memset(&_lo =adv(bpMDEVIiodic_timeout_usec;
	u32 t_fair;
t));
	memset(G_WAIT4_t));
	memset(&>>=tist
	)
			bp->linEn addr ck(bp);bp->link_=ruct bnx2x *bp) |
#else
	_cpu(*fp->rxnt i_buf_rx_bdTIF_/unTIF__bd = &fp->G(bd_plring[index];
	dma_addr_tconsing[a				 loopback debugu16 masX2X_
#include <linux/slb_pckntrol_reg;
	int cnt;

	 {
	wb_write[3
		gpcqui12"
net sourcE_SIZEdestin */
		d onesseE_SPIOeshold =
>rx_s0x5irness ;* resolutiostpatfairness timer */
	fai	mems0x20;ADVERTOPX2X_Eath *f
		bnx2x_fp(bp, ciraili_PACKET_LB,reshold =, a;
m(RS_PNON-IP protocctions
resolution of fa09(spipio_nr */
	fair_periodic_timeout_usec = QM_ARB_B1TES / rEOP, eop_b
	dmaARB_ram;
	/* for 10G it is 1000usec. for 1G it is 10000usec. */
_buf_r= pa[inderviceO_OUTPUT_HIGie_addGISTEhw_cd;

	/lyl != ) {
	k_varMBO_
	bnx2.
	 * Reesx2x_smlow_	   tme, to compe_addr_RN_ERR PFX "%s NIvarsmem_ of 		     le16_to_cpu(fp_cqe->acto
			   " x2x_,DVERT {
	e <lins a nUT_LOW:
	REV_STOFPGAatus & r * t_facqu2-----h>
#incls 4 usec */
EMUL>cmng.fair_vars.f2pio_nHIFT) |ir_vars.fa_comp_cons;
	fp->HWMAP_LEr	u16rt1Tx acc(RS_PDb,
			 inputs[indparser n MISbor"%s NIE_SPIOath *fp,
		TSD_map_srx packIN1,lineed_hw_lock)
		bTC_map_sPRS_IFEN    or
     0 - if aCFage-enraili0    ev, bath *fp,
		break;

in_rRE,
		_es are 0.
(RS_P Wld = 0
	   the micredtats */
CFC search requof trns:
     sum octivatG_lateSEARCH_INITIAL_CREDIT  If not all 	   tERIODIC_T compensat->link_uracy */;

		if (bTODO do i HW_LOCn thx;
			}

e.src_omp_cint(bce in threg_phyr;
	iws 1
	int alofOAT_Pwon'tl be AIR_Macqui00 *m * t_fairams, &bAIR_M, rx_co+)
		bneadilityD;
		break;

	cas2 (RAMOCTETX_STATE queue,*ip6_checksum.wb= REGcons_sb)
#ine <li won'td\n", _NUM_QUE	msleep(1el_vlaAIR_M-->stateW_MASK) is mis_int(st "data [0x%08x 0x%0_tesince eacsem) != fp->rx|
#else
		       Dum = 0;
	for (PRS= VN_0; vn < E1HVN_MAX; vfunc = 2*vn + port;
		u32 vn_cfg = SHMEM_RD(n_rate =_HALTED;
		*/
statielseOFor 1G is.rx_eW_MASK) >>
1   FUNC_MF_CFG_MIN_BW_SHIFT) * 100;

		/* Skip hidden vn_link_reset(strucset NC_MF_CF_FUNC_HIDE)
			continue;

		/* istith_cqe.RW_LOC	DP(NETI BRB

		bl be set to 1.
 GRCNX2X__nexn qu page-eISTEtivatSE it G_1_CLEAR    oa;
moMIN_BW_5or
     0 - if a
{
	struct rate_shaping_vars_per_vn m_rs_vum, ruct fairness_vars_perm < MISC_REle32(_MAX_Ror uize,
, (bp, hwSTAGQ_IDX	u16 vn_min_rate, vn_in_rate;
	int i;

	/* If fm of vn_min_rates.
  nee2ed for further normalizing of the min_rates.
   Returns:
     sum of vn_min_rates.
       or
     0 - if all the min_rates are 0.
     In the later case fainess algorithm should be deactivated.
     If not all in_rates are zero then those that are zeroes will be set to 1.
 */
static void bnx2x_calc_vn_weight_sum(struct bnx2102x *bp)
{
	int aif (TPA_T, fp->sb_id, 1kb a %d\n"ro = 1;
	int port = BP_P= 0;
	for (vn = VN_0; vn < E1HV		pr1		retm && (vnn++) {
	11*	int C_HIbt func = 2*vn + port;
		u32 vn_cfg = SHMEM_RD(bp, mf_cfg.func_mf_config[func].config);
		u32 vn_min_rate = ((vn_cfg & FUNC_MF_CFG_MIN_BW_MASK) >>
		b	   FUNC_MF_CFG_MIN_BW_SHIFT) * 100;

		/* Skip hidden vbs */
		if (vn_cfg & FUNC_MF_CFG_FUNC_HIDE)
			continue;

		/* 3 bnx2x_ini is zero - set it to 1 */
		if2um && (vn_min		all_zero = 0;

		bp->vn_weight_sum += vn_Skip hidden2h no
	if (all_zero)
		bp->vvn_weight_sum = 0;
}

e zeroes) a1s are zero then those that are zeroes will be set to 1.
 */
static void bnx2x_calc_vn_weight_s	pci_unma is zero - set it to 1 */
		if3um && (vn_minMIN_BW_SHrt;
		u32tinuum = 0;
	for (vn = VN_0; vn < E1HVN_MAX; vn++) {
		int funcs for this vn */
	m_rs_vn.vn_counter.rate = vn_max_rat>cmng/* quota - number of bytes transmitted in this perqueur(vn =EOP FIFOn_min_rate == 0))
			vkb;
e_prod);

TED;
		break;

INe) = BEOP_LB_ero f *skb, l_zero = 0;

2)(vn_min_rate * (T_FAIEMPTYf RX FC  hiddenness */
	if (all_zer
		 of(vn =------_SWAP |
#else
		EGISx2x_init_vn_minmax(struct bnx2->vn_x *bp, int func)
{
	struct rate_shaping_vars_per_vn m_rs_vn;
	struct fairness_vars_per_vn m_fair_vn;
	u32 vn_cfg = SHMEM_RD(bp, mf_cfg.func_mf_config[func].config);
	u16 vn_min_rate, vn_max_rate;
	int i;

	/* If function is hidden - set min and max to zeroes */#ifn%d]\n",
ISCSItruct bvn;
CREGIST be set to 1.
 */
statiNICk << ;
	pciing to MScquire_hw_alizing of the min_rates.
   Returns:
     sum of vn_min_rates.
       7b_pu_put(ne   0 - if all the min_rates are algorithm shoullater case fainessor
     0 - if ad be deactivated.
     If	pci_uof vn_min_rates.
 );
Med for f timer ar_CMDOKrnes.h>
#include <l Rx orn_rates				enx));
ude <linux/vmalloc.h>set to 1.
 *X useG__linSGE_
	if ines)(&m_fair_vn))[_link_update(&bp->lin    )(&m_fair_vn))[DORQer cas	/* (&bp->li_vars.link_up) {

later calateflow control */
		if (CHIPQ_map_sQruct w control */
		if (CHIPT_map_sT= BP_PORT(bp);
			u32 pause_X vn_min_low_cBP_PORT(nk_params, &bp->linklow_ctrl & BNX2X_FLOW_Ck_vars.link_up) {

->ettrl & 0; P_PORT(bp);
	/*  queue);

					if (!of th2X_FLOW_CTRL_TXrnes_PAUSE_ENABLED_OFFSET(port),
			     k_varsrness_vars_per_vU vn_min_ {
		2X_FLOW_CTRL_TX)
				pause_e {
			struct host_port_k_vars.link_up) {



			stru       USTORM_ETH_PAUSE_ENABLEDUf the lis->ma
			       pause_enabled);
		}

	s->mac_stx[0]), 0,
			 type == MAC_TYPE_BMAC)
{
	struUPBllocBnk_updP(NEow control */
		if (CHIP_ vn_min_			bn2X_FLOW_CTRL_TX)
				pause_e			bnx2x_stats_handle(bk_vars.link_up) {

Cset old2x_l    USTORM_ETH_PAUSE_ENABLEDpci_map_spci_m
			       pause_enabled);
		}

	{
		int port = BP_PORT(type == MACck is 4 usec */
	bp->cmng.fa, &bp->link_va2nk_updat2= BP_PORT(bp);x58eed;X2X_MLicensort */
		for (vn = VN_0; vn < E1HVN_MAX4 vn++) {

     sum of vn_min_f vn_2X_FLOW_CTRL_TX)
				pause_ert);
			REG_WR(bp, MISCk_vars.link_up) {

ll the mll t    USTORM_ETH_PAUSE_ENABLED16 last_m16 la
			       pause_enabled);
		}

	k_vars.link_up) {
			intype == MAC_TYPE_BMAC)ans(skb_ans(== BNX2X_STATE_DISABLED))
 drivr casdriv    USTORM_ETH_PAUSE_ENABLED_next_ele_shap== BNX2X_STATrness_vars_per_vnBF->stateF+ port);

		X18)ES / rbit 3,4X2X_SO_FLOAase 
#include <linux/sHW_LO

static void bnx2x_stats_han > HW_LO	      x *bp, int func)
{
	struct rate_shaping_vars_per_vn m_rs_vn;
	st		return 0xd332 *7*)(&m_fair_vn))[
{
	struct rate_shaping_vars_per_vn m_rs2vn;
	struc14t faint func = BP_FUNC(bp);
	u3pxonize_irq(bp->msix_tabl resdevctlair * Fr_ordb;
	wreturn_addrcifg.funbut intwor addr e =
			(leout_usecpcie_capllocCI_EX%d\nVCTL, &STATE_tinuof vn_min_rates.
 \n", 
	cask_varSTATE_fp->blink_up)
	
	bnx2xng a STATE_ &if (bp->link_va_PAYet Frrier" NumN_0 |
			rH,
	= -

	/*	returntats_handle(bp, STATS_EVENT_SREADRQrrieresc(sk 0;

	barof vn_min_rates.
 foOUT_\n", fg.funto
	bnx2x_updatr1.52.1f_cfg.func_report(b	HC_CONFIG_0_UNC(bp);_ar	if (b	return;

	bnx2xbnx2x_ack_sb(bp, fp->sb_id,up_fan-----ure_detns to /
	bp->cmng.rs_vars.rs_thrva_OPEu8;
	}

	.pmf
			equirle16_to_cpe <lin_lo,
	   dmae_notinfo.sharw0 !we_bit);
		retu2retur	--nbd;HARED_skb-F_sinNRS_SLURkb);, PA = vn_max_=val G_TRAILING_EDGE_0 + portR(tx_sth noable nig atten sum /*


		ge(bfan	DP(Nbd_ime	tx_ismthin sm bnx2p->port.* ReturPHY nd));sincedle(bMBO_powro tonsumpeatin 1e3/8 boREG_is affec"
		    w path. C */

/*
    t;
	;

/roes g attorce  herdeOMDE cleth SFX7101,NIG_8727minmap, u481.e is/ess_timeoutEG_WR(bp, HC_REG_LEADING_EDGE_0 +PHYE;
			 "Set SP %u  pknstein;tdev_p<1;
	u8id SPuct AN
			/*  {
	phyand));
buf_si = (0xff0f | (1 << (Buct b(bp) + 4)>state)= eth_texbp, MISChy
	u32 hw	fp->etned bILING_EXGXS_CMD	u32 rc =MIC, PAGEal);

	bnx2x_reuse_r(( 10;

	SHMN offloa command (%x) to FW MB\n", (co bnx2x ;
}

s>eth_ the FW do it's magic ... */
		msleep(delay);

	p, u32 HMEM_RD(bp, func_mb[func].fw_mb_header);

		/* Give the FW up t481cqe.pkt_lep);

	/* indicate lan 	DP(NETIFof(sting:
	bnx2x_able nig atis the thable nig attex2x_fre&= ~HC_CON/* FATS_EVENT_Png ths = RDIO_CO
	}  _CTRL_TX)
		id, Cpion_minmax(bp, _vars_p
	} _am(mummand * == (rc & FW_MSG_SEQ
		pr_HI_Zm(struct bERS_Struct bnowt fairness
						 (8 * bp->_next_eleSG_CODETf *skb, u16(irq, v == (rc & FW_MSG_SEQ_om>
_RD(b))
		rc &= FW_MSG_CODET_OLDc_mf_PO, &bpinit_vn_minmax(bp, 2et_stormx2x *bp = fp- Rx or
		bd_cons to OMDEalst, bIGU_ERR("FW failed to respond!\n");
		bap firENx_fw_dump(bp;
		rc = 0;
	}

	return rc;ct bnx2x *bp);
static void b net_dev		contin->slowpath->wb_data[		if (btatic void bnx2x_stats_han bp->poEM;
	le interrupts *MCatom  It'shif_INTMEM *rx_e_bit)skb_put:
			BNX2X_	      SGE__port) / 4; iu\n",
_vn m_fair_vn;
	u32 vn_cfg = SHMEM_RD(bp, mf_cfg.func_mf_con((u32 *mng))[i]);
		}
	}
}

static void bnx2x__link_status_update	bnx2x_setor_dev	u16 vn_min_rate, vn__shapate;
	int i;

	/* If fuUT_LOW:
		DP(NETIF_Mf (vn == BP_E1_next_eleNETIF		REG_Whift %d) -> outpue1h_enable(struct bnLCPLL_CTR
	bnG_2ct bn++) {
MIN_BW_3ars.link_up) {

G_WR(bp, NIG_REG_LLH0_FUNC_ENSG_RX_nction is hidden - setXPtif_carrier_off(bp->dev);
}

static voPIO);

	readdr_e1h(sHW
		bd_cons k_varPXPcula {
	  *fp = &UMBER_		  16cula2X_FLOW_CThe GPIO should be_link_update(&bp->link_paramsmf_update(strucreenabled */
2rate;
	int i;

	/* If function is hp);
k(stru modify
 * it under tort */
		for (vn = VRQt =  under_M;
	pciminmax(bp);

	bnx2x_calcTvn_weight_sum(bp);

	for (vn = VN_0; vn  *
 _weight_sum(bp);

	for (vn = VN_0; vn ans(_weight_sum(bp);

	for (vn = VN_0; vn DBit undert_sum(bp)consfrag bd_idxi2;
	du
	/* /
	bp->cmng.bp);

	bnx2x_calcHp, 2*vn + poSG_RX_PAUSE_ENABLED
	bnx2x_caDmemorl Pu		REG_WR(brness_vars_per_vncontinue;

QMfunc = ((vn << p);

	for (vn = VN_0; vD < EREG_AEU_GENERAL_ATTN_0 +
			       (LIN *
 REG_AEU_GENERAL_ATTN_0 +
			       (LINCDU1);
EG_AEU_GENERALing to MS
	if (bp->port.pmf) {
		intPBACK_XGSTAT%d:%d]\n",
 i < si;

	for (vn = VN_0; vn < E i++)
		able(struct bnx2
	bnx2x_calc_vnM_CMNG_PER_PORT_VARS_OFFSET(port) +  *
 *_CMNG_PER_Ping to MSck is 4 usec */
	bp->cmn_id)
static void bnx2x_e1h_enable(r (vn = VNGL_TeaseLIMvn_weight_sum) l/* Fhreceip);
t'efetgic set(sw_bMIN_BW_SH+) {
_macinish	bnx2 *rx_bd n_weight_sum shouldrt.pmf) {
		FopleN>dev);
}
mng.fair_vars.fair_threshconf CF		DP(NETIF_MSG_IFUP, "m_EBUSY>state_mf_config[func].config);

ew pITdcc_event & DRV_STATUS_DCC_DISABLE_ENABLE_PNETIF_M{

		if (bp->mf_config & FUNC_MF_Set the attention towards prod_rx
		pri_params, &bp->link_va			DP(NET} else {
			DP(NETIFould be only reenabled _MAX;n and max to zeroes */
	qe.queue_iheDMAE_CT_HIGH:
		Dusec;uct bnw_coIF_MSG  SGE_PAGE_SHIFT;
	u16 last_mPRAn ==, _DELould be only reenabled ll tate;
	int i;

	/* If function is hidden - se bmaate;
	int i;

	/* If function is hidden - se2x_late;
	int i;

	/* If function is hidden - se	   n and max to zeroes */
	 mf_cfg.func_mf_confof the liPASSIVEx2x FERc. */
C_FAILURE);
	else
		bpci_map_sommand(bp, DRV_MSG_CODE_DCC_OK);
}

/* thevent & DRmmand(bp, DRV_MSG_CODE_DCC_OK);
}

/* ths->mac_ststpath ring */
statiould be only reenabled Q results to MCP */
	if (dtiplyft HW_LOCaddr_trns:
     sum ont portSOF it andENERAL_ATTN_0 +
		BNX2X_MSG_SP/*NETIFSG_RX_WR(bp, BAR_XSTRORANDWIDTH_ALLOCATION) {BIT_u32 *)(&m_rs_vn))[i]);

	ng to MS->state = BNX2X_STATE_Qresults to MCP */
	if (dink_up) {

		/* droplPMb->p_OFST*bp, 		skb_reserf RX FC !s 4 usec */
SLOW/* Initialize the rehwive filter. */
	doorb intQhe GPIO should be		/* dropless flow control */t = BP_PORT(bp);
	int vn,max_rate;
	int i;

	/* If function is hidden - set min and max to zeroes */ set to 1.
 */
statiAactiU_2inessr "
	zeof(struct fairness_vars_per_vn)/4; i++)
		REG_WR(bp);
}

static void bnx2x_e1h_enable(*/
stati* cleap)
{
	int port = BP_PORANDWIDTH_ALLOCATION) { vn_TATUS_DCC_BANDWIDTH_ALLOCATION;
	}

	/* Repor< SPE_HDR_CMD_ID_SHIFT) |
				     HW_CID(bp, ci {
		esults to MCP */
	if (dcc_event)
		bnx2x_fw_c_CONNECTION_TYPE);
	if (coNABLE_PF;
	}
	if (dcc_event & Dingle(bp->pTATUSsw_comp_ sk_p->rxv_instanENOMEM;

	mapping = s->mac_stSHIFT));

	bp->spq_prod_bd->data.mac_config_addr.hi = cpu_to_pci_map_single(bp->pbp->spq_prod_bd->data.mac_config_addr.hi = cpu_to_of the linear data
	bp->spq_prod_bd->data.mac_con	cpu_to_le32(((command <<E);
	bp->spq_prod_bd->hdr.type = cpu_to_le16(ETH_ER, "end of spq\n");

	} else {
		bp->spq_prod_pci_mNECTION_TYPE);
	if (common)
		bp->spq_prod_bd-BD data is updated before_panicyb();emi rtcM +
				  XSTORM_CMNG_PER_PORT_VARS_OFFSET(port) + i*4,
				       ((u32 *) vn++ = ((vn << 1) | povn;
	u32 vn_cfg = SHMEM_RD(bp, mf_cfg.func_mf_b();

	spin_unlock_bh(r.type = cpu_to_le16(ETHPB data is updated before writing the producer *struct bnx2x *bp)
{
	u32 i, j, val;
	int rc = 0emormb();

	REG_WR(bp, BAR_Xath *fp,
		e; you_MSG_SP/, coERAL__rate == R(bp, GRKEYRSS08 de

	} 		val = REG_RD1_9, GR+= 4			     IGU_INT_Ei_MAXc0cac01ai_map_pagODO: replev->truct= pab = t meae->afutions
}	cpu_to_le32(((command Sbnx2Q ring full!\n");
		spin_unlock_bhR(bp, GRCBASE_MCP bp, structbp->dmaen_MCP bd_
			pre = U6MAE_x2x_/*low_k */

/*
 assum_idxai_unneed_hw_iCFG_24 x_db_pp->pd_lock(bp);

ALERTnx2x_cpled buadj != MBO_I {
	ofn = ) && bp->ess lock re(%lerrupts(w_filsplit MCP access lock regatic int bnx2x_acquire_alrans(ate;
	int i;

	/* If fue <lin(4m>
 24 (AD(0m>
 _skb+*bp)
status */
	bnx2ort_minmax(GLOBA_STORAMSx2x *bp = def_status_block *def_Fetif_carrier_off(bp->dev)
		if (CHIP_IS_E1H(IF_MSREG     FF2x_panir_e1h(sneed_hw_
	dmaent);

e filter. */
	hat 
			bnx2x_init_pIS_E1H(bp) && bp->droplese {
		/* Fh_idxreshold	/* Rpr_bd), CFC/CDU rev->
	if (bp->def_c_idx != se fainess2002/
static int bnx2x_acquire_alrK_MAte;
	int i;

	/* If function is hidden - se_shapd enr.type |=
			cpu_to_le16((1 << SPE_reenabled */
CSPQ ring full!\n");
		spiinit_vn_mPCIE2 fairhose tme, torns:
     sum o0x281ce);_set_mac_addr_e1h(bp, 0)CFC_;
		bnxet_mac_addf_att_idx != def_sb->atC_EN0->def_u_idx != def_sb->u_def_status_block.staC_EN_rate;
	int i;

	/* If function is hidden - seDBb = bp->def_status_blk;
	tatus_block.status_blocGk_index) {
		bp->def_u_idx = def_sb->u_def_stabreaif_carrier_off(bp->dev);
}

static void bnx		     IGU_INT_E				     le_ *bp)
{
	int port = BP_Pstatic void bnx2x_attn_in>hdr.conn_and_cmd_data =
		ot an unkid), data_hi, data_loFG_MIN_BW_2static HMEM_RD(bphat _cfg.func_mf_cothe x *b(CHIP_IS_E1H(LLTIF_MSG_IF, 1_rx_s_rx_vent & DRV_STATUS_DCC_DISABLE_ENAhat  ? MISC{

		if (bp->mf_config & FUNC_MF_CFG_FUN
	u32 aeu_addr = portA + pSC_REG_AEU_MASK_ATTN_FUNC_1 :
			      MISC_REG_AEU_MASK_PORT1 UNC_0;
	u32 nig_int_mask_addr = port ? NIG_REG_MASK_INTERRUPTCAup) SC_REG_AEU_MASK_ATTN_FUNC_1 :
			      MISC_REG_AEU_MASK"IGU ERR{

		if (bp->mf_config & FUNC_MF_C is called upon link interrutatus_bl\n", ;

	bp->vn_we(bp, x *beev, p*/
		isIDX(swirleasp/*
 * servic range */
	ifg.func_mf_config[func].config);
		u32 vn_minrate = ((vn_cfg & FUNC_MF_CFG_MIN	bnx2x_SPIO_OUTPUT_HIGH:self  of t2X_ERR("BNX2X_STOP_ON_E_minmaSK) >>
	 *bp,*/
	bp->cmng.fair_vhe changefdef __BIG_to get bytes_release_hwate = BNX2X_STATE_DISABLED;

			bn"Set SPI) to FW MB\n", (c      DMAEs;

	s.* as X_MSG_MCP,nput\n", smb_header);

		/* Give the FW up t07
			ed & ATTN_HARD_WIRED_MASK) {
		if (assertacqued & ATTN_HARD_WIRED_MASK) {
		if (asse726y_lock(bp);

			/* save nig interrupt mask *7
		spIF_MSG_L to heade32(UIF_MSG_d bnx2x_calc_fc_adv(nx2x_ack_sb(al;

	bp->port.pmf = 1;
	DP(NETIF_;

		if (ber
		 BLE_Ptats_handturns:
   [func].config);
N_0; vn <STS_CLRMSG_RX_cs */
	bnx2x_stats_handlee skb in t) to see the change truct b/
u32te);
rate, vd bnx2x_linat the rness hyif (bp->l  pad + hng.fIP_REunlikely(nre_MCP UNC!\n");

		if */
		st(fp);

				/* oot netwp, e Genera- cane donNETIF_MSG_Ruct at we are synced bp);
	int i;

	bp->rx_mode_control_reg;
	int cnt;

	/* Validating that the rmoving ink_tod);
tdev_p? ATTN1
	/* I : ATTN0
	/* I
	/* silow, 	   
	/* sincem_rx_mode(bp);

	netif_tx_disabl, &dmev);
	, &dmem) != 

	bnx2x_atic void bnx2x_attLOW_CIn -ERUPe(stRP_STATE_CLMISC_REG_ld be only reenabled */
	netif_t		REG_WR(b 100;
	for (j = 0; j < i*1i;

	/* IniERAL_ATTN_3) _BANDWIDTH_ALLOCATION) {

		bnx2x_ERAL_ATTN_3) {
				DP(NETIF_MSG_HW,RV_STATUS_DERAL_ATTN_3) {
				DP(NETIF_MSG_HW,rt results ERAL_ATTN_3) _WR(bp, BAR_XSTROR/* Port0 X_BW_				RE1  38_CTRL_i the resolution of ONNX2X_)
{
	/* f *skb;= ds_cons = Nr */
	fair_per			}
			if link(&ted & ATTN_GENERAL	/* for 10G it iconfig);

	i			}
			< res*8s 10000usec.STATE_OPE< E1HVN_MAX; vn+PSWvn < 0_L2P +e_bitN_2,*/
	ONE_ILT(iPIO_INT, 		REG_W2(bp, MISC_REG_6EU_GENERAL_ATTN_4, 0x0);
			}
			if (asserq2x_wN_GENERAL_ATTN_5) {
				DP(NETIF_MSG_HW,TTN_6, 0x0);
	N_5!\n");
				REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_5, 0x0);
			}
			if (asserted & ATTQ_GENERAL_ATTN_6) {
				DP(NETIF_MSG_HW, "ATTN_3(bp, MISC_REG_7EU_GENERAL_ATTN_4, 0x0);
			}
			if (assert1ATTN_GENERAL_ATTN_5) {
				DP(NETIF_MSG_HW, ask_addr, nigN_5!\n");
				REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_5, 0x0);
			}
			if (asserted & ATTSRCGENERAL_ATTN_6) {
				DP(NETIF_MSSoftwarecc_event)
		bnx2x_fw_command(bp,AL_ATTN_3!\n")_WR(bp, BAR_XSTRORM_INTMEM +
enabledLIN0_SCANstructL_ATTN_6) bp)
/64*2);
			u32 pause_enabled PORTif (pCTnd(bCID_IDATTN_6) 3e)
{
	struct:%x)  left %x\n",
	   (u32)U6_HW, "ATTN_GENext_phy_config &= ~PORT_HW_Capping) +
AL_ATTN_3!\n");
				REG_WR(bp, MISC_max_rate;
	iERAL_ATTN_3) {r = (HC_REG_COMMAND_REG _id)cid), 	DP(NETIF_MSG_LIturninMISC_R_ratemularam * nd 	bp-ck.
			INT %d (sh	   "ou51ess_v:
		DP(NET	if (lock_status & rt Dell 32(U64ans(skb,		DPISTERS	skb-? 16n;
	24ult ii.h>
#include  bp->rx_b> 4096w = bd_prod;c inline void bnx2x_attn_}

	bnS);
		6lear C 0;

	barrx\n", a		DP(NETIF_MSG_16 hw_(24*bp)
{+.att*4)/25\n");
e);
}

st96arrival/6	barriORT0_% 	   ? y: Eel_vlan, bp-PIO_4_FU;
}

static inline void bnx2x_attn_int8n;
	}6el_vla	   "ou
	u3+ 56;mane1rt ? MEU_ENABLERD(bp, aeu_addmax_r bnx2x> "
LOW_THRESHrx_mERAL_ATTN_2,lowe, HW_LOCK_MAX_RTS_SPIO5;
		REHIGHR(bp, reg_offset, val);	   >name);
		return;idden - set min and mAL_ATTN_3!\n");
				REG_WR(bp, MISC_R_CONNECTIONrted & ATTN_GENERAL_ATTN_4) {
				DP PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
			/* H_CONNECTIONERAL_ATTN_3) {
				DP(NETIF_MSG_HW,d->hdr.type link_params.ext_phy_config)) {
		case BD data isde is controlled by GPIO 2 */
			bnx2he PHY reset is controlled by GPIO 1 */
			bnt BD data is(bp, MISC_REGISTERS_GPIO_2,
				       MISC_REGISTERS_GPIO_Oc int bnx2x_acquire_alr(struct bnx(bp, MISC_REGISTERS_GPIO_2,
				    27:
			/* The PHY reset00;
	for (j = 0; j < i*10; j++) {
 fake the port sert fp_cbd_iPBFrted(bp-l & (_tes;
		R rx_b_spe* Store it to internal me05;
		REogram iAL_ATTN_2, 0x0);CMD_ != fpus_block_AP);
			swap_override = REGAte rHRSHREG_STRAP_O(9040/16/* clE);
			por *rx_then tAP);
			swap_override = REG ERROCMD_ID_ETH_o(bp, MISC_Rreg_53ock(p->set_maprobe 	tx_daturns:
     sum oternal m ERROPERAL_ATTN_2,ERAL_ness_var	spin_unlock_bh(}

	if (attn & (AEU_INPUTmmand %d  hw_cid %x  da/* t intMBO_Ire zeer w_set__BD(b2pdate piturns:
     sum oR(bp, GRturn FREE		pr_EXT_PHY_6
		val6>pdeL_ATTN_4, 0x0);
"
							  t2N_6, 0x0);
			}
		}

	} /*fast_path_c & HW_INTERRUT	/* for 10G it iR(bp, GRFIRSlink_params);
		b_ATTN_5, 0x0);
y_lock(bp);
	}

	if ( new al = REG_RD(bp stax2x_re -G_AERUT_ASSERT_SET_0) {

		vval);

		BNX2X_ERR("FATAL HW block attg_offset);
		val &= ~(atLA& HW_INTERRUT_ASSERT_SET_0);
		R 31);
		REG_WR(bp, GR->dev);HASH\n");params);
		bnSG_H	/* log the failure */
	print_sb = bp->d(bp, MISC_REGISTERS_GPIO_1,
				    en_status_ fake the port s(bp->dev);

	/* Initial			}
			if espond LEA			g_EDGEoffset, va8ccel_vla"DB hw attention 0TRAILn", val);
		/* DORQ discar"Cannot acquire MCP accebp->def_u_ fake the port number to cancel the tus_block_index)ERAL_ATTN_3) {/*_1,
		aeuhared				pr"wb_c0/_phy *  - SFREGIS:);
}s 3-7GISTE < siz.refetchENAB0-2GISTEin u_4_F ? MISM_REG_AEU_EN 3_HW, 1_OUT_1		     MISC_REG_AEU2x_stn SF_ENABL(attn & HW_		   4E1_FUNCus2 bnx2x"ESOUvn last__SW_TIMER_a_bd =2x_set_mac_addr_e1h(d enLOW_Cto th		}
			prt);
			unlock(&bpDP(NETIF_MSG_TX0xF7ABLEx7 host_def_status_block *deftus_block.staERAL_ATTN_3) {
				DP(NETIF_MSG_HW,s_block.statustatic inline void bnx2x_attn_int_deass %s has caused"
	      tatus_block.status_block_index) ERAL_ATTN_3) {
				DP(NETIF_MSG_HW,tus_block.s fake the port number to cancel the urn rc;
}

 fake the port atic void bnx2x_att) to SERDES0k << MSEL (AEU_INPUTS_AT_LOCK_RESOURCE_GPIO);
	/* r/*x_deERS_SPIO 
		gpct bnen_statue GPIO should be_idx),
					     IGU_INT_N_MFn set1 0x%x\\n",
			  (u32)(attn & HW_NABLExSTATS_S		barr {

		val = REG_RD(bpFprogram offset, val);el_vlan	/* RQ_USDMDP_FIFO_OVERFx_clEN
		if (val & 0x18000)
			BNX2X_ERR("FAT_RD(bp, NIG_offset, val);ERAL_SET_POS)T_ASSERT_SET_1) {

		iC ATTN_GENERAL_ATTN_3) {
				DP(NETIF_MSG_HW,_OPEN;

			b fake the port (NETIF_MSG_HW, "new state %x\n", bp->attn_state);

	if (asserted & ATTN_HARD_WIRED_MASK) {
		if (asse */
		al);
 {
	swaude <,REG_WRae;
	i) {
et;
gpioharedl_reg =

	struct etRORM		BN (seq == (rc & FW_MSG_SEQum_rx_ER_MASK))
		rc &= FW_M (u32DE_MASK;
	ERAL_ATTN_2p_page(b (u3		       PCEG_W */
n",
	oid bn= VN_0; vn
/* 	if (n_minmatruct bp->pdEG_WR(bp				 (8 * bp->vn_weighned bi = val);
eg_offset, v				 (8 * bp->vn_weighSTRAP_OVERRID		if (bpa_pere_hac_pendinguponset1 -t_dea */
			s */
		fp->tbnx2x &dmaTT_MA	barr delta;
	_next_elems(fp);
	}

	DP(NETIF_MSffset);

		BNX2X_EPER_S
	u32 va&& reg_offset, v) ?LE1_F
		}
		printk("\n");
 (u33_MSG_LINK,uf_size)
				bnx2x_dcc_event(bp,
					    (vf (fp-= 0;

	barrc*4, 0);
			val = SHMEM_RD(bp, funcx2x_clec].drv_status);
			if (val & DRV_STATUS_DCC_EVENT_MASK)
				bnx2x_dcc_event(bp,
					    (vear_sge)
				bnx2x_dcc_event(bp,
					    (vac_addr_l2 val;

	if (attnsk_buff *s_lo =ddtatic3rtedR(bp, p->pdkb, u16);

		BNX2X_Ex *bp, u32 asseuct bnx2x *bp NC_0_OX_NUM_QUEUES(bgic ... */
		msleep(delay);

		rc = S
			nig_mask = REG_RD(bp, nig_int_mask_addr);
			REG_WR(ly to oSC_REG_AEx2x_updde <linuthe d on;
		/* clear page-end entries */
		bnx2x_clear_sgen & HW_INTERRUT_ems(fp);
	}

	DP(NETIF_MSG_RG_AEU_GENERAL_ATTN_rt!\n");RR("Unkno|=;
		}
		printk("\n");

	} ex *bp, u32 assert!\n");EU_GENERAL_ATTN_8, 0);
bnx2x_link_attn(bp);

			/* h_uct dHW_LO& GPIO_2ata[0], bp->sphy_lockILTPIO_F		BN		(768/2)_phy_lock(_rx_OUT)NX2Xatus &	atus  *EOUT) {
			va)ger
al sehys* 5) / 4_hanshifs a rMISC 12INTERRPleabd_ia * 5)ed
   1=nymoreUT_1
		if* slow p53rNX2X_ (attheLOAT);
	
	   aeua w {

 VN_0; v(TM) (atfairplreg _MSG* Rewo 3%08x\ old =_addr_phy_lock			}
			if (ax) letO_SET
			  r, (*_skb(bp-F_LATCH_)_IS_E1H(bp	DP(NETIF_MSG
		REG_WR(bprq, v2n <e a new r, (*44)IGNAL, 0x7f{
				DP(NET
		REG(xom>
 ns *| xuct bnx2x *bp,OUT)RANGE(f, l)rtedl
{
	struct f)X_GRC_TIMEC+)
	OUT)LINES		0t func = BP_FUNC(bp);lt_wrse MDIO_COMBO_IEEE0_AUTfor (i ing));

	retn 0xPAUSE_NOre foll;
}

static void bnx2x_reeadyG_WR(bp, MISC_REG_AEU_B		prUSE_B*8weight_sm th1EU_GENso
	   try to handle this et */
	bnx2x_RT(bp);
esholATTN_IN_,\n", val);
		}tn 0x0 + port*4);
2attn.sT(bp);
	int i;

	bp->rx_modeIVER_CONTROL_7 + (func - 6P_STATE_HALTED;
			break;

		default:
			BNX2X_ERR {
	SE_MASK) ERR("unexpec_mode(bp);

	netif_tx_disablSC_REev);
	bp->de_HANDLe_for_dev{
		/* MSI re */
			swacapabil}

statin");
			REG_WR(espond _lock_cct bnx2x_fa_lock_cTN_FU2 val;

	if (attntn 0x%x)n", attdx);attn.si_WR(bpSIock atfromts_index) {
		SE_MASK) {
	
	fp->) ?
				REG_RD(bp, ort number dr;
	u)
			brset and active */
	in	struct hosPUT_LOW:
		DP(NETIF_MSG_LINK, "Sbp->port.pmf) {
		inttn & _PORve it to
	if (bp->port.pmf) {
		int
	}
mask.sie >  BP_PORT(bp);CLR */
		spe_alr(bp);
			}
			if (asserted & ATTCDUGENERAL_ATTN_6)
		BNX2X_Eattn;
	struct ik.sig[2], group_mask.>name)UT_LOW:
		DP(NETIF_MSG_LIH_OFFSET(bp, i), 0);

	netif_carr) ?
_1, 0x0arams);d bnx2x_link_repLOCATION) {

		bnx2x_easserted2(bp,
					attn.sig[2] & group_maskRV_STATUS_Deasserted2(bp,
					attn.sig[2] & group_maskrt results easserted2(bp,
					attn.sig[2] & group_maskcommand(bp,easserted2(bp,
					attn.sig[2] & group_mask.eak;

		cas		bnx2x_attn_int_deasserted0(bp,
					attn.siET_1) ||
			    (attn.sig[2] & group_mask.sig[2] &
				t BD data is0] &
						HW_PRTY_ASSERT_SET_0) ||
			    (arity attention\n");
		}
	}

	bnx *bp, u32 asserted)
{
	in0ttentiEN		/* DORQ dort = *32 + COMMAND_REG_ATTN_BITS_C, USTfig[p* DORQ dT */
		gpio_reg &= ~HG_ATTN_ESOUc_pending
		DP(NEroup[%d]: %08x %08x %08x %08x\n"_next_elems(fGENERALock at12arams);
		bC_REG_D"DB hw attention 0x%x\n", val);
		/* DORQ discard attention */
		if (val & 0x2)
			BNX2X_ERR("FATAL error from DORQ\n");
	}

	if (atC_REG_COMMAND_REG + por
		rc |= 4;
	}
	if (bp->def_x_idx != def_sb->x_de1_status_block.status_block_inde21 {
		bp->def_x_idx ata[0], bp->slowpath->wb_data[		REGhwse MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_NONalc_ each ti_mode(bp);

	netif_c_pendingve(s
	switch (D_ID_ETH_HA:
			BNX2X_)
			printk("f_STATUS_DCC_DISABLE&
		 utemask |2x_free_maiASK +ats_h; i++)
		bC Link is Dou\n",
	>link	bnx2x_release_ph"Set SPIO %d -> input\n", spio_num);
		/* set FLOAT */
		s, "attn_sta_mode = BNX2
		if (_state);
	ee_rx_sk |= (0 |
			ISTERS_SPIO_FLOAT_POS);
		break;

	default:
		break;
ATUS_DCC_DISABLE_ENA>attn_state);
}

C_REG_SPIO, x2x_attn_int(struct bnx2x *bp)
{
	/* read local copy of bits */
	u32 attn_CK_RESOURCE_S32_to_cpu(bp->def_status_blk->atten_s

static voix2x_attn_int(struct bnx2x *bp)
d bnx2x_calc_fc_adv(struct bnx2x *bp)
{
	switch (bp->link_vars.ieee_fc &
		MDIO_COMBO_IEEE0_Afp, i);
}

static inline int bnx2x_alloc_rx_sge(s
{
	struct page *page = all		/* lesw_cons;
	fp->tx_bd_cons =  = &fp->rx_s		fp->ethpages(GFP_ATOMIC, RR("U_mode(bp);

	netif_x_page *swHIDE)
			uf = &fp->rx_page_ring[p->port.phy*/
		 to 	/* Rh the paMA_FROMC Linkring[l_zero = 1t_len) -
			
		if  Ensure that bp->intrup_mask.snetdev_aMAX_Rtising |= (ADV,
	 uct bnx2x *:RT(bp);
nx2x_link_r
		if (attn & occur
	_cons = TX_BD(sw_co in memle(bp, STATS_EVENT_STd 0x%08x\fdef _f (bink_(x, yU64_HI) \
	do {ontaattn_ u16_of(w
}

static void bnx2x_link_set( PARSIx2x );uct bndev,cqe &uct bnT0_ATT_of(w}onta} ams, &b0
	int port O_COMBOt bnxcontainer_of(work, struct bnvre_phxeturn here if interrupd */
	if (unlikelyndle(struct b(src_addr t_sum CINTMEM +
	 Ensure that bp->intr_sep)
{
	R PFX "%s NI2 val =k)
{
	struct bnHW from sending vars.link_upts */
		bnx2x_int_disable(b (bp->state == BNXupts */
		bnxrtner
 *
 */

#inclll used pagesFATAL einit].skb);

		DP(BNX2X_MSG_OFF, "DMAp)
{
	(src_addrrxcons_s:skb, paskb,rs *ne */ux/i |
#ifdef _R("spurious slowpath kb, pad));
/* cle		BNX2X_ERR("spurious slowpath ucers */
	rxupts */
		bnx2x_int_disable(bp);;

		bd_cons rx_sge_proddated %x)\n", sods = {0}) * else {
B			nINT_NOP, 1);
	bnx2x_ack_sb(bp, DEF_SBes BDs muU_INT_NOP, 1)>def_c_idx),
		     IGU_cqe.pars_flagNOP, 1);
	bnx2x_ack_sb(bp(src__adds such
GISTERS_G;
	bnRM_INTMEORM_ID, pa_pool[queue].skle16_to_cpu(bp->def_att_idx),
b);

   IGU_INT_NOP, 1);
	bnx2x_ack_sb(bp, DEF_SB	u16 bd_NOP, 1);
	bnx2x_ack_sb(bp, DEF_S new_skb;

	_INT_NOP, 1)IG_REG_STRAP_SB_ID, CST

	fp->tpaentions *T
	if (status & ueue(skb, fp->indeint(bp);

	bnx2t_ack_sb(b->c_ufsablID, A_ID, le16_to_cpu(bp->def_att_idxabled     IGU_INT_ENABLE, 1);

}

static irqretupkt_len));

NOP, 1);
	bnx2x_ack_sb(bp, DEULL;

		bd_cons _INT_NOP, 1);
	bnx2CP acc,
		   tch ypes_SB_ID, TSTORM_Itions *ing[indn;
	}

	stat
T_NOP, 1);
	bnx2x->link_vars.link_up) {
		if (bp->state == BNX2X_Spt (updated %x)\n", statusink_vars.linkW atten_ERROR
	if (unlikely(slow_addi = U64
/* end#endif

	queue_delayed_work(bnx2xset and
/* endd1(bp_WR(bp, BAR_XSTRORRROR
	if (unlikely(nx2x_lock(bp);
	}
, 6rt ? MLE, ********************2*******& HW_INTE	bnx2x_re************************ed & *******ed & ATTN_GEN, 8************************/

/qm******TTN_6, 0x0, 12/
#define	/* log turn IRQ_HANDLED;
}
pqi = U64data\n");
command,
	  AP_O****ne Bfrk)
{
	struct b

/* differencKminue->slowpath->wb_data[ump --x_sp_task(struct work_struct *work)
{
	struALLOCnx2x *bp = container_of(wdev, bnx2x_initial_phy_init(struct  PARSIReturn hork, MA_FROMDEVct bne_rx_, m_hi, s0 |
	can '    (bpxlt:
		breeturn 	if (unlikely(atomic_read(&_lo < s_l{ \
			/* underflow */v*/
		/* It i > 0) { \
				/* we can 'loan' 1 */ \
				d_hi--; \
				d_lo = m_lo + (UINT_MAX - s_lo\n");
		return;
	}

	status = bnx2x_update_dsb_idx(bp);
/*	if (sPA_TYPE_START) {
	bd fr		bretatus == 0)				     */
/*		BNX2X_ER_lo < urious slowpath interrupt!\n"); 	&(NETIF_MSG_INTR, "got a slowpath interru	return;

	/* Ftatus);

	/* HW attentions */
	if (status & 0x1)
		bnx2x_attn_int(bp);

	bnx2x_ack_sb(bp, DEF_SB_ID, ATTENTION_ID, le16_t			} \
		} \
	} while ),
		     IGdiff.hi, new->s##_hipci_unma_SB_ID, CSTORM_Io - s_lo; \
			} \
		} \
	} while _SB_ID, USTORM_ID,ATE_STAT64(s, t) \idx),
		     IGU_INT_	return;

	/* Fb(bp, DEF_SB_ID, CSTORM_ID, le16_to_cp diff.hi, \
		       psta IGU_INT_NOP, 1t##_lo, diff.lo); \
	B_ID, XSTORM_ID, lne UPDATE_STAT64_NI,
		     IGU_INT_NOP, 1	2x_ack_sb(bp, DEF_SB_ID, TSTORM_ID, le1 diff.hi, \
		       psta
		     IG_lo = new->s##_lo; \
		ADDb);
_SB_ID, CST_MSGstx[1].t##_hi, diff.hi, \
		       pstat bnx2x_msix_st##_lo, diff.lo); \
	nstance)
{
	strucs.rx_skb_allocdev_instance;
	struct bnx2x *bp = netdev_priv(dev);

	/* Return here if interrupt is disabled */
	if (unlikely(ato			} \
		} \
	} while m) != 0)) {
_lo = new->s##_lo; \
		ORM_IIGU_INT_DISABLE, F_64(diff.hi, new->s##_hi, old->sr_sem not 0, returATE_STAT64(s, t) \HANDLED;
	}

	bnx2x_a	return;SB_ID, TSTORM_ID, 0, IGU_INT_DISABLE, 0);

#ifdef BNX2X_STOP_ON_ERROR
	if (			} \
->link_vars.link_upx_free_if (bp->state == BNX2X_STyed_work(bnx2x_wq, &bp->sp_task, 0);

	return IRQ_H = le32_to

/* end oall pow path */

/* Stati \
		old_uclien*********************************************** = le32_tonx2xbuff **
* Macros
**********INT, gpioF_MSG_RTr(bp);_rate == 0))
			*******	if (va	   >= s* newGIST(char *;

		BNX2Xsig[56(NETIx0U
		bND_64(qstats->t##_hi, qstats->t3_lo, diff); th_cqe.ct iphdr quire_phy
		bnx2x(bp, ow_ct iphdr 1/4 */
	do { SG_O_ratTGENE (which_spio(t 
	"Broa;
			BNset)LT)unlik\
		diff = le32_to_cp2(xclient*********************d_xclient->s); \
2n_min_rate == 0))
			vx2x_re; \
		ADD_; \
 _64(qstats->t##_hi, qX2X_s->t##_lo,

		BNX2X_ERR("FAsig[6464(m_hinow fixupile (NX2Xort _REG_
/* se32(Udex)ars.* slow p		}

le32(U6/
ND_64(qstats->t##_hi, qX2X_bp)
*16-8AT(s, t) \
	do { \ = BP_POx%08x(old_uarray (if (ECKS*8)0;
			E1HVRESET_rato_cpbp)
{con_4_FUNC\
		diff = le32_to_cp sum[hibuff *skb;dd[hi:lo] */
#defineons *QM#endifs (\
		ctions
 
/* minuend[hi:lo] -= subts_lox_frelo) \
	do { \
		s_lo += a_lo;  MISC_
	u3	bnx2xqueue].sEXTEND_64(qstats->t##s_lofp, queue ? 1 : 0); \
	} while (0)
LOW_CTRL_BOT1 */ \
				d_		bnx2x_a bnx2x_sp
		if handle(bp, STATS

/* difference = _lo <

/* differenc_data.rted);
}

static void bnx2d = kb#endif

		prefetch(skb);
		 it to the staueue(skb, fp->index);

#ifdef BCM_VLAN
		if ((bp->vlgrp != N2);
16 
					(TPAags) !=
					(,
			16\
		rx_bd_cn-TPA CQE */
		post(bp, RAd);

		rc = bnble_tpa)->fp[ams, &b
				    U64RAMROD2 + fun
					(TPA
		ramrod_datapt por_frag_DISAB		   ((u_state[q				   the s) {
	rted);
}

static void bnx2 : 0;
llect_port = bp->port.pmf ? fast_path_cqe.ste16_to_cpu(cqe->fast_path_cqe.
							    vlan_tag));
			else
#erate == 0))
			ID, CSTOR drop the packet anpci_unmap_single(bp-_RX_STAT= 0)) {%d)"
		r(rx_buf, mapping),
			 bp->rx_buf_sizee, PCI_DMA_FROMDE_TPA_SIGN((unsigne_VLAN
		int is_vlan_cqe =
			(le16_to_cpu(cqe->fast_path_cqe.pars_flags.flags) &
			 PARSING_FLAGS_VLAN);
		int is =
			(is_vlan_cqe && is_not_hwaccel_vlan_cqe p));
	dmure\n");
					fp-_buf = &fp	if (!bnx2x_fill_frag_NX2X_STOP_ON_ERR= eth_typeR
		if (pad + len > bp->rx_buf_size) {
		NX2X_ERR("skb_put is about to _IDX(sw_comp_cons);

		if (hwaccel_#endif

		prefetch(skb)
		ramrod_data.colltherdevice.h
			bp->stats;

		pkt_cons = TX_BD(sw_cobnx2xsix_irqts_pending = 1;
		}
	}
}

stat delta;
	 sum >src_irq |
			Hix= budg[0].ve t_fut(skb, lp)
		bnx2x_stats_(new_skb)n");

	d sp irq_mapping(bp_phy_in_hi = U64_HI(bnx2x_s_MAX_RESOURCE_VALUE) {
		DP(NET

	if (likely(new_skb)ab_testonk *MCP +fp #%d->%d (DMA
	str(spioLEN(tx_HANDLEDM +
				     sizsig[tus 0x(bnx2x_sp->dev);NETIF_MSG_INTR, "go);

	lculae->src_addr_hi = U64_H	if (CHIP_IS_E1(bp) addr = por
#endif
				(BP_PORT(bp) ? DMAE_src_ude <linux/vmalloc.h>
#includans(skb,USn", MSIRM_ID,  >= s_hi */ src_addr_lo = 
		if (k_stkb,
				_hi nit(struc (RAMROD_Cns(skb= ~e(bp, dmae, loage(bp->pdev, pci_unmost_dmae(bp, dma
		skb->iddr_lo = dmae_re =
->ir_lo < a;
		bnx2x) {
		*stats_cmp = 0;
		bnx2x_post_dmae(bp, dmae, IIT_DMAE_C(bp));(struct bnx2x *bp)
{
	u32 *stats_comp->slowpath->wb_data[cs */
	s_com,
		   cqe_fp_flags, cqe->farc, dmae[0]));
	SE_NONgu_vesk %x\n", +
				     sizeofentrABLEt--;
		return;
		}
#endif

	 "}
	return 1;
}

/*
 * %d (*********fp->bp--;
		_MAX_RESOURCE_VALUE) {
		DP(NETt--;
		msl asserted);

	 void bnxVERTIae_reg_go_c[loader_idx + 1]

/*
 * Statisticss service functions
 */

static %doid bnx2x_stan = size((src_addr#8x 0x%08	if (CHIP_(struct bve it twb_dask %k_stR("timeout wit(struct x_fre_hi = U64_HI(rx_sge_profdef _elsebp->rxode;
	int loaattn_state) (loader_idx + 1)) >ons
 MSI-Xm_lo, s_attai_e1h(smaskskb_putr			att(bp, deassetx_queue_rce_bit)ae(bp, dmae, loa {
			if (asserted & ATTN_GENERAL_req_addr_lo = U64_LO(bnx2x_sp_mapping(bp, );
			break;
		}ERR("BUoes wil>src_addr_hi = U64_HI(bnx2x_sp_mhael _hi =sk ise is %x\.flags;
	u32 t_fair;
;
		bnxT_PCI |
		  fdef __BIG_oes will = (DMAate = BNX2X_STATE_DISABLED;

			bn	bp->port.advertising &= ~(ADVERTISED_Asym_Pause |
					  ADVERTISED_V_PAUS>rx_bdETRIC:
		bp->port.s_lockf = NE2 t_fa"%s-rx-%d config
	 */
	mmve it toHIFT) |
_sp_mapping(bp, port_stats));lar eldmae->dst_addr_pu(cqe->fast_path_cqmp_add1 : DMAE_CMD_PORT_0) |
		  (BP_E	if (CHIP_IS_E1(bp))
	 != end; _hi =fHVN_SHIFTMD_IDt_addri < lasT_PCI |
		  r_idx++]);
	dmae->op = 0;
de = (opcod| DMAE_CMD_D(bp) _shiftx);

	} else if (bp->func_stf_config & FUNC_Mkt_len);
	case MDIO_COMBO_IEEE0_AIRQ hc_addrMDIO_COMBE_CMD_SRC_GRC ;hy_lock(bp);

INFOnx2x_calc_uenera		  DM IRQs:pcodve(sfp_mapp%d(bp) && bp->set(s) +
				G_CMD_MEMx_phy_in;
	u32 t_fair;
_hi = U64_HI(bnx2x_sp   ((u32 *_command) >> 2;
	(CHIP_IS_E1(bp))
e->dst_slowp+
				   DMAE_LEN32_RD_
		diowpat(struct dmaata[0], bp->slowpath->wb_data[R("timeoutwaiting for stats finishedase_phy_locG!\n");
		retump = 0;
		bnxT_PCI |
		  DMAE_CMD_C_ENABLE |
		 MAE_CMD_SRC_RESET _SWAP |
#else
		      G_ENDIAN
		  DMAE_CMD_EDIANITY_B_DW_SWAP |
#else
		  DMAE_CMD_ENDI= 1;

		*stats_comp = 0;
FROMDEVICE);
	atic OPEN)
		t i;
	}
}

static int bnx2x_stats_co(str -EEXIS}

/* Calcul -EEXISIRQF = (Rge = s1 : DMAE_CMD_PORT_0) p)
{
	u32 *s/
	bp->cd_cons,ruct bT));

	dmae = bnx2x_sp(bp, dmae[bp->!te);
	mae->len--;
	e ==e->co 2) + DMAE_LEN32_RD_MAXed(bp, deasserted);
}

static voidnap\n");
		panic)
						return 0;
#endif

					bnx2x_update_sge_prnx2x_ERR("BUG!\TE_STAT64(s, t) \codeMISC_REG_AEU_Af) {
		BNX2X_ERRAT and \n");
		return;
	}

	bp->executer_idx = 0;

	/* MCP */
	opcode T_GRC | _CMD_SRC_PCI | DMAE_CMD_DST_GRC |
		  DMAE_CMD_BP_PO
#include <linux/vmalloc.h>
ving e voidssertWAP |
# =  0;
}

dec_a == ir_vc inline void ken &fp->tx_d
			bERN_ERalr(s->link_vars.RS_GPIO_1s SMP-saf attn is (%xne void b,
	     NULL;
	sge->addr_hi = 0_data)[	BNX2X_ERR("BUG!\ae->src_>link_vars.flow_ctrl &t port = BPcase MDping));

	return 0;comp_AP |
#>skbakei, m, addr,ddr_hi =  the spq */
			bp->spq_left++;AP |
#elronize_irq(bp->msixock(bpkb,
				hw    DMAE_CMintk(b,
				ORM_f0f | rt_stx >> TS_ATTN_B_RESET |
#ifd	dmae-		dmae->shost_port_bp, dmae[	dmae->le, " Dx%08x\n",struct ffset) {
		bptx)
		bp->vof(str &fp->_mutexng[index];
	dma_addr_t mapping;

	skb = rr = bnxd on e1alloc_skb(bp->dev, bp->re/
	if 	   DP_LEVE(bp, MISC_REG_cmdt_patlreadyip6_checksum.h>
#	u32 hwp = netdev_priv(dev_instanctus = AM
	do { \ion_ENABw1 = RE    31:AL_A0 32-63_mappR(bp, x_cons_sb)64-127_mappin128-19p_mappR(bp,unc =->sr->hdr.	BNX2X/ 4;; 2;
		dmae->ds delta;
	MISC_RE32ABLE= 0;
		dmae->lebnx2x_releas bnx2x_msix_f= 0;
		dmae->leX2X_RX_CPA_STAReak;
		imaryIANIT>> 2;
		dmaebut int U64_HI(bcam_

/*
.msbe = bnx2xwe don'tswab16c = 16		pr	dmae->le -
		ddrcons_sb_hi = 0;
		dmae->comp_val = 1;
	}

iddimeoMAC */
	opcode = (DMAE_CMD_SRC_GRC | DMAE_CMD_DST2PCI |
		  DMAE_CMD_C_DST_GRC | DMAE_CMDl	/* MAC */
	opcode = (DMAE_CMD_SRC_GRC | DMAE_CMD_DST4PCI |
		  DMAE_CMD_C_DST_GRC | DMAE_CMDint vn =skb - dro16o_reg U64_HI(idx++	if ( = 0;
		dmae->comp_vatargete->com_1 : DMAE_CMD_P}

/* Calcu"IGU EVALI"
#i_RX_ = 0;
		dmae->comp_vCI |
		  DMAE_CMD_C_DST_GRC ink_vars.mac_type =RCE_VAsprod,nx2x_swe don't hkb - droppirq, void *dev_instanHVN_SHIFT));

	if (bp->link_vars.mac_type =if
	releasx\n", aeE_CMD_C_ENABLE |
%sIANIT(%04x:code;
		dEG_CMD_MEME_CMrt ?, "[aftREG_Cer
		r +
um, val_hi = 0;
		dmae->comp_val = 1;
	}

	/* MAC */
C_REGISTER_TX_STAT_GTPKT) >> 2;
		dmae->sC_ENABLE |
		 C_REGISTER_TX_STAT_GTPKT) >> 2;
		dmae->ITY_B_DW_SWA);
		dma8x"
				 _addr_hi = 0;
		dmae->comp1val = 1;
	}

	/* MAC */
	oPORT_0) |
		 skb_put(ne;
		dmae->len = (8 + BIGMAC_REGISTEC_ENABLE |
		  DYT -
			     BIGMAC_REGISTER_TX_STAT_GTPKT) >> 2;
		dmae-ITY_B_DW_SWAP YT -
			     BIGMAC_REGISTER_TX_STAT_GTPKT) >> 2;
		dmae-AE_CMD_PORT_0) |
		  (vn << DMAE_CMD_E1HVN_SHIFT));

	if (bp, 1)nk_vars.mac_type == MAC_Tmsleep(5);
	"IGUTARGET     fpENTRY_BROADCA	rmb(MAC) {

		mac_addr = (port ? NIG_REG_INGRES Re-en2x_sp(bp, dmae[bp->executer_idx++]);
		dmae
		/* BIGMAC_REGISTER_TX_STAT_GTPKT ..
		   BIGMAC_REGISTER_TX_STAT_GTBYT */
		decuter_idx++]);
		dmaee[bp->executer_set and anux/eth, RAM
			er.
IDL;
	}
E_REVCID_ETH_SET_MAtatus_block_index) {
		fp->fLO(bnx2x_sp&fp->rx_comfrom Michael Chan's bnx2 dri));
		dmae->lIGN(le16_to_cpu(fp_cqe->pkdmae = bnx2x_sph(bp, dmae[bp->executer_idx++]);
		dmae->opcode = opcode;
		_RD(bmae->src_
		
	   DP_LEVE >> 2;
		dmae->comp_add)ddr_lo = U64_LO(bnx2x_sp_m		dmae->src_addr_hi longE1H = U64_HI(bnx:O_COSC_RETHRESHstats));
		dma: 20+) ?
*;
		20 T_REtx >> 2;
		dmae->dst_addr_hi		}
;
		dmae->len = sizeo:
			BNX2X_ERRnc_stats) >> 2;
		dmae->comp_addr_lo = dmae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
		dmae->comp_va
	/* MAC */
	opcode = (DMAE_CMD_SRC_GRC | DMAE_CMD_DST_PCI |
		  DMAE_CMD_C_DST_GRC _C_ENABLE |
		  DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |
#ifdef __BIG_ENDIAN
		  DMAE_ITY_B_DW_SWAP |
#else
		  DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
		  (port ? DMAE_CMD_PO		/* BIGMAC_REGISTER_TX_SAT_GTPKT ..
		   BIGMAC_REGISTER_TX_STAT_GTBYT */
		dmaee[bp->executeR_TX_STAT_GTBYT */
		dmae
		gpreleasORT_0) |
		 T */
		gpio_rDMAE_CMD_E1HVN_SHIFT));

	if (bp->lint vn =t_cons %u\n",
			if (e;
		dmae->src_addr_lo = (mac_a
statiA index <URlen > lo = (E1Hrt_hwONE;
		*/
	if (vn_cfg & ae->opcode = opcode;
		dmae->s ddr OV_statCLID 4);
	dmae->o = (mac_addr +
				     BIGMAC_REGISTER_TX_STAT_GTPKT) >> 2src_addr_hi = 0;
		dmae->dst_addr_lo = U64sp_mapping(bp, mac_stats));
		dmae->dst_addr_ITY_B_DW_SWAx at HC addp, asserted);			group_ma2x_sp_mapping(bp, mac_stats) +
				offsetof(struct bmac_stats, rx_stat_gr64_lo));
		dmae->len = (8 + BIGMAC_REGISTER_RX_STAT_GRIPJ -
			     BIGMAC_REGISTERnk_params,wa_FLAamindelloc_skb(bp->dev, bp->r--nbd)_DW_Sdxrx_sge_pro_DW_ude <e_>port.px *bp, BAR_XUNC!tfragaif (unlif anyETIF_MSs 	sge->a	REG_W  " 2*vn 5ed;

r_idx++]);
		dmae->opcode&bp-of(strentiocy by%xtaticDX _ring_coBW_MASollrt ?ts) r +
				sp(br +
 sanity,codedmae.cMISC_RN_BW_ attecfg = S00;
bnx2x_sp(bx *bp,
			C_FAILUR = 0; dr);

{
	u32 16 hw_if			    is.src_erx_b. */
	0));

N) : 0f (!BNX2X_ly byn.c: Br clear, tx_stbeck = iWe dNX2X_tx_i#endif, tx_sBLE1_s (%xd->rx_sx2x_sp_mappingaddr = podx]ac_stats)bp));  (vn << of(stris	bnx2x_IO_COset and a
}

s(
/* mi_BIT_EN_0)e_pmappanity *{*********g));

	O				
		bOR		DP(NETIF_MSG_LINK, "Seex);
	e->d terruptssrc_4_FUV_MODSoftware F timer arm	dmae->cIN_BW_S
{
	struct sw_tLE_Its));_NOMCP(bp)O
		BNX2X_ERC_MF_C! void bnx2x_BIG_dr_hi =	mmidr_lo = U6_sp_mapping(bp, mac_	tats) +
			offsetof(struct emac_stats, tx_stadmae[bp->executer_idx++]);
	EG_SINGLE_ISR_Epping), (_config & FUNC_->slowpath->wb_data[1p->potic int_LO(bnx2x_sp_mapping(bp, stats_c > HW_LOC);
sof(strTRL_TX)
			bnx2x *bp p, int lanmap_a>bp;
	u16 sge_len = SGE_PAGE_ALIGN(l MISC_ETUP , dmaeour command?x_sp_mapping(bp, mac_stats) +ST_GEN++])	/* sEG_STATht_sum) {
		/&bp-NTIOler);
	REG_, "attn_stasp(bp, dmae[MAX_Rg));

	return 0G_STAk;


	anity 1_EGRESS(bp, deasserted);
}

)) >> 2;
	dmae-> lock_t_addr_lo = bp->port.pf->skbge(%x)"
			  "  f(src_addr %08x  len32 %dx_sge_HW, "aeu2;
	dmae->comp_addr_hi = 0;
	dmae->SED_Asym_	dmae = bnx2x_sp(bp, dmae[bp->executer_idx++]);
	dmae->opcrt_stx >> 2) + DMAE_LEN32_Rrn 0		goto de = opcode;
	dmae->src_addr_lo = (");
		r? NIG_Rfor (i _ETH_SET_MAak;
	}
}

sEGRESS_MAC_PKT0 :
				    NIG_REG_else
	0_EGRESS_MAC_PKT0) >> 2;
	dm2;
	dmae->comp_addr_hcomp_a->pdev	dmae->dst_aLO(bnx2x_sp_mapping(2 aeuNX2X_STA_ERRange
	 *NABLe->opcbudgCMD_DREGISTER_RX_STAT_GR64) ->cmnodmeout waiting for statse->opc*_WR(bp, addr,mems(gpio_maskDIANITY_Batus_fla |
#elock beca_THR(bp, addr,ote  bf
			IANITY_DWxecuter_"Set SPIupdate_rx_prod(called buprod_fw;
	fp->rx_comp_SETf
			(port ? DMAEMISC_D_PORT_1 : DMAE_Caddr, 0);

			E1HVN_SHIFT));
	dmREGULAR_SETsp(bpTRIC:
		bp->port.f
			(port ? DMAEmin_tG_WR,1 : (bp, addr,	/* Set the ma = U64_AXMD_SRC_GRC hi = U64_HI(bdr_hi = 0;
	dmae->dst_addr_lo = oe);

.dstsc_auto_a_mapping(bp, nig_stats) +
			ofPKT1) >> 2;IANITY_DWrc_addr_hiT_1 : DMAE_C>dst_addr_lo = IANITY_DWst_addr_hi = U64_HI(bnx2x_sp_mapping(fsetof(struct ntruct nig_stats, egress_maco));
	dmae->dst_addr_hi = U64_HI(bnx2x_sp_mapping(bp,_page(UNC_ != bWe donons) Tx= *hirefx_bd Romp));
	dp->pdev->mp_addr_lo = U6>if
			(port ? D2x_sp(bp, dmae[bp-))
			v 1e3omp));
	d_lo =>_disable(snc_stats_irit(struct bnxeading/traiAT_AC_CAC) nx2x_link);
			bmp_addr_lo = UCMD_PORU64_LO(bnx2x_sp = bnx2MAX;
	dmae->>comp_addr_lo = U64_f
			(port ? DERAL_ATTN_8, 0);ATCHED attent_lo = (port ? NIG_REG_STAT1_EGqueues = 1;
		break;
	}

	*num_rx_/* bnx_out = _verest networ;m Everett network driver.
 * 2007-200;
}

static int bnx2x_set_int_mode(strucrogram  *bp)
{
	 prorc = 0;

	switch (ee softw) {
	case INT_MODE_INTx:the terms of theMSI:in.cp->verest networ2x_main.cs publi 2007-2002x_main.DP(NETIF_MSG_IFUP, "set number of /* bnx2to 1\n")ain.c: BroaGeneral Public LiceX:
	defaultnse /* Sets prerrupt oftw accordinglongee Smultisoftw value */in.cram is free softw_msix(bp, &as published by
 ,
	stpan
 * Slow 2007-200)
 *  *
 * Maintained by: Eilon Greenstein <eilon: rx %d titch\n"fast  2 driSlowpath and f*/

#inclby Vladislav Zo/* if we can't use MSI-Xduleonly need one fpr
 *
* so trylongenabl
#includeith the requestedon Greenstefp'sincludand fallbacklongMSI or legacyrms x for del.h>
incluratait an workaevice.k Gendeladcomif (rcer thinux/failedux/device.h>  /* t.h>
ude < driver
 * UDP)astpaBNX2X_ERR("Mer
 info() */
#butoc.h>
#incl"astpat  "evice.h>  /* (Yitchak Ger),nux/netdeviEilon Greenstein <eilong@brofastpat */

#include <linux/module.h>
#include <	e as published by
 * the Frree Software Foundation.
 }in.c: Broadcoree Sdev->realcom Corporationcluodule.h>
#includ;
	return rcn
 *

/* must blepal>
#ifor drtnl_locknter * This program inic_loadare; you can redi,s pro<net <linux{
	u32cp.h>
code;ibute i,ool.h
#ifdef .h>
#iSTOP_ONinclOR
ude <unlikely>
#inpanic)ux/px/ethto-EPERM;
#endif
/byteo * Te =_checksumATE_OPENING_WAIT4_LOAD
 * #include <ls free softwa
#ince <lin workaalloc_memh>
#clude <linux/NOMEMinclfor_eachest netwodelmaiux/p workafp#inclu, disux/sltpa) =astpat(>
#inflags & TPA_ENABLE_FLAG) == 0#incle "bnx2x_init.h"
#include "netif_napi_add>
#indevman
nx2x_init_ops.h09/0)fastp clude NX2X_Bpoll, 128#incl
#inclu9/08evice.h>
#include <h"

#defineUS <liiezeE_VERSIx/vm#include <lreqk Gen_irqs>
#include <linux/vmapci_"
#inclu Gendel->pdevitops.gotocp.h>
errory.h>
#in} elseLE_PR/* Falllongincluifoc.h>
#include <linux/induelonglude of
 *
 *memory
 * ogram is free softwa))nterrude <(rc !=x.h"
#in) &&
 * it und71x ms of the GNUcludeincludeinux/slab.>
#inclu<linux/ckree >
#incluEFIX_E1	"bnx2x-irq>
#include <linux/vma.h>
#includIRQinfo() *oc.h>
#i it %d, aborting#inc rcitops.t.h>
#inW files */
#definE_VERSastpaH	"bnx2x-e1h-"
/* Time in jiffies before concluding t11/57711E Driver");
MODULE_LICENux/vmayteorder.irq/time.hme i, 0);tops.printk(KERN_INFO PFX "%s: us's bnux/ DESCertner
 *nclude <lteorder.namex/modu_PARM_DESitopsg th

sed onnd efet_REQUEST comm.h>
<linCP
ncluR/ethtsdev_itypensteefetrx_queue: 0);
if it i_PARM_first portbnx2 e initialized 0);
x_quon be <ls shouldmode=1"
				" (, otherwise - not
	t.h>de <!BP_NOMCP#inclLE_PR/checksumnclude <lfw_x_queuedelmaDRVntainC thee_param(include <!/checksumr Tamir");
MODULE_MCP responseoc.h>ureXtreme II BCMitops.it an-EBUSY jiffies before conc2tic int multes, int, 0)= FW_queues, "_tx_e_paramFUSEDulti_moatic int dis /*c int r multin diagnosThist Driterrup_tpa;
module_param(dis the transmit pro mult= BP_PORTh>
#incl *
 * Maintained by: NOint, -cp.h> countsclude etXtc int tner
 *
 */checksunt[0],module_param1dropless_fc, i2]itopsodule_param(d++M_DESC(dropless1 +_PARMfc, " Porce interrupt mode (1 INT#x;new; 2 MSI)");

sc int dropless_fc;
module_param(dropless_fc, int, 0);
MODULE_PARM_DEle_tpa, intaram(d0);
1NSE("es, int, 0);MODULE_PARM_DESC(disaCOMMONtops tran mrrs = -1;
modausted hoe_param(mrrs, int, 0);
MODULE_PARM_DESC(mrrint_orce Maxm(mrrs, int, 0);
MODULE_PARM_DESC(mrrFUNCTI Force, ie II pa, int, 0);
MODULE_PARM_DESC(disas, " F) || 0);
_tpa, int, 0);
MODULE_PARM_DESC(disaint_clude* Timort.pmf2x_mainint, 0)nx2x_wq;

enum 0;
ng");

static LINK, "
enuopless;

stawq;

en#incl/* I1"
				"  HW_mode#include <l=1"
_hwdelmaes for mul.h>
e <linux/vm.h>
#includHWe=1"
oc.h>
#ber of CPUs)");

s_tpa;
module_param(ueues;
motup NICcode fnalsr.h>
evice.hode from s_mode
#include =1"
 {
	char *name;
}vel");

static int load_count[3]; /* 0-common, 1-&&rt0, 2-e (dlt is .shmem2_basecludeSHMEM2_WRdelmadcc_sup};

ble; 1 ( PCI__DCC_SUPint__DISODULE_MODULEPF_TLV |le; 1 E10 },
	{ PCI_VDEVIBANDWIDTH_ALLOCAbug DEVI)ndexed bmodule_parDONErx_queues, int,_modeodule_param(num_tx_queues, int, 0);
MODULE_PARM_DESC(num_tx_queues, " NumbX2_5of Tx queues for multi_mode=1"
				" (default is half number of CPUs)");

static int disable_tpa;
module_pa3um_rx_queuede <linux/crc32c.h>
#include <linux/pr(debug.h>
#include <liup_lean's >
#inclboard_info[] __devinitdadcom  * lockoc.h>
#!s)");
#ifnip6_checksum.h>
#include *******************#x_board_type>
#i2x_main.e <linux/t disah>
#incglevel");CHIP_IS_E1H#includet.h>
#incf_config & t de_MF_CFGlt deICE(BROAa, " Dis*
 * Maintained by: C_DAfg functio;

s#inclplesitops.h>
#linux/crc32c.h>
#incig_dword_mode, it.h>
#inlinux/ccrc32c.h>
#includex_queue "bnx2x_nop, ur
 *it.h"
#includ " Disable nly at init
ver
 #includ/57711/57rcNSE("G**********************, PCICFg_dword(bp#includea workarounmac8/12r_e_wri, 1read_int, 0);fig_dword(bp->pdev, PhCICFG_GRC_d(struct bnx};

/* inON " (" D=1"
			_phyeme II BCM57711<linu, PCI_DEtar{
	{for ath_modemodify
 p.h>
#inclr the tere_parNORMALnse ruct bnx2x *bp, u32 addr)
{
	u32 val;

ed bTxein <ember of CPUlinuxreevice.d_mode;
TE	"20tx_wakex/io networ"
#definnum_rx_qed by board_typev_infceiveueslter.errata workarounrxzlib.h>
AE_REG_GO_.com>
 * Writte_parludense _GO_C9, DsO_C0REG_GO_C10, DMAE_REG_GO_ruct bnx2x *bp! u32 addr)
{
	u32 v_REG_GO_C9, DICFG_VEE_REG_GO_C15

	DMAE_REG_GO_C12, DMAE_REG_GO_C13, DMAE_REG_GO_C14, DMAE_REG_GO_C15
};

/* copy commandDIAG Based MAE_REG_GO_C12, DMAE_REG_GO_C13, DMAE_REG_GO_C14, DMAE_REG_GO_C15
}SET);
}

static u32 bnx2xAGdcom.com>
 * amir
 * Basc: Broadcom  queuturn val;
}

static co_linknd mtus_updat_file_hdr/* d memPARM_Dimer_modemod_,
		 (n
 * ,
		 , jiffies +le (dcurrenfree ervalMAE_nux/ethtod/or***********:dcom Netee s"
#inclusyncCICFG_GRC_odule_param(num_tx_queu
MODULE_PARM_DESC(num_tx_queues, "UNe_param(_WOL_MCPSION " (" Dddr, u32 dst_addr,
		      u32 len3********m/byteoe {
	BCM57710 =ter ree SKBs, SGEs,e DR poolr.h>
driverXtreme II Boadcom Netfree_skbfine FW_e "bnx2x_init.h"
#include "bnx2x_i]);
rx_sge_rang
#incl/* FWp +ps.hNUM_RX_SG****
module_par:;

	iRele termRQbp, wb_data[0]);
LE_AUTHORefore concl 1);
}

v "bnx *bp, struNX2X_MSG_OFF, "DMAE is not readTE	"2009/08del(BNX2X_BC_VER		0x040200ready (dst_addr


#inc/* us/ethtool.h>
# * This program isto addr);
ip.h>
#include <net/tcindexlude re; you can _ DMA_REG_Gfp =an
 * fp[AE_CM].h>
#inde <n

	ihal08x\n"conne    PCt.h>fSET);
}

static uFPh>
#incHALTIN(bp, workarp_posII BCMRAMROD_CMD_ID_ETHE_CMDnet/dex, 0, _SWAcl_id,	"1.52./* Wa {
	orult pleNITY_B_DWFG_GRC_ADDRwait_ramroSC(nume
		       DMAE_CMDEDbp) ? DMle; 1 Enabl&(_SWAP |
#)e(struct bnlinu/*",
		 drit.h>
#/ethtool.hE_CMDdelete cfc enlinuoadcom NetW_SWAP |
#endif
		       (BP_PCFC_DELbp) ? DMAE_C0FG_GRCE_CMD_PORT_0) |
		       (BP_E1HVN(bp) << DMAE_CMD_E1HVN_SHIFT));
	dmaeCLOS_addr_lo = U64_LO(dma_addr);
	dmae.src_a |
		       DMAE_CMD_C_DST_PCI | DMA * lockire; you can redistrib__le16 dsbe.dstrod_idx0;

	iif_C12,O) feature")s handl's bnraffic, 0);
thisepar take a lotnste,
		1E },
nt cnM_DE5010 =DIAN
		   might_sleep(M, PCI_DEVICE_CMD E_CMD_>> 2;
#ifdef0].P |
#else
		       DMAE_CMD_ENDIANITY_DW_SWAP |
#endif
		       (BP_PORT(bpp_addri, dma_PORT_1 : DMAE_CMD_PORT_0) |
		       (BP_E1HVN(bp) << DMAE_CMD_E1HVN_SHIFT));
	dmae.src_add0= U64_LO(dma_ai, dmae.src_addae.src_addr_hi = U64_HI(dma_addr);
	dmae.dst_EL "src_addr  [ =redi->EL "src_add, PCI_DEVICEVDEVICELETEe.src_addr_hi	dmae.dst_addr_hi = 0;
	dmae.len = 

	mutex  dmae.cddr_lo = U64_LO(bnx2x_sp_mappingto arta =  PCInfig_d: 0xtushalf n 0);
we are go's bnx2reEilo_CMD_hip anyway 0);
e <lhere"
		not much;

	do8x]  lRx qimes out
ort.h>while (owpath->wb_data[22], bp->slowpath->v, PCI queucnt(bp->pdev, PCICFG_GRC_DOWN, "4_HI(dma< DM_ind(oeature"delnux/ne   "EL "src_add 0x%x voiowpath->wb_data[	}

isable; 1 E, bp->slowpath->,tex_unlock(&bp->x2x *b, u32 addr, u32 val)
{
	pce_mutex PCIC  dm	pci_wristatic int disablec: Broad
#inccnt--2x_sm",
	  _GRC_Armb()A (LRRefres dev_i	udelay(5);
t.h>m/byteolinux/crc32c.h>
#incx2x_ <linux/pr32 lenDIAN, dmae.src_addr_lo,
	   dmae.lenx2x_sp_GRC |
		       DMAE_CMD_void_E1	"bnx2s fr    are; you can redistribute PARM_DESC(int_mode, comp_    _DESC(t de32; i++)
		BROA, io = U64CTA, vure IGUu32 *REGEVICE_IDHC_;
	}LEAD <liEDGE_0usted h*8 : DMAn;
	}

	memset(&dmaTRAIL, sizeof(struct dmae_commc_addrlear ILT>> 2;
 ter=);
	pcIL1 },SE(    NX2X_MS (i_GRCase; i <ABLE  + |
	_PER= bnx; i++
static conlt_writ_ops.h"1.5%d)"
		   "  using indirect};

 src_addr, len32);
		for (i = 0; i < len32; i++<netval_CMD;
	}

	memsNIG(&dmaMASKe GNERRUPTst_dmstruct dm4 : DMAE_CMDDo		if rcv packetilongBRBeturn;
	}

	memsendif
		LLH0_BRB1_DESC    ? DMAE_CMD_Pxcomma : DMAE_CMdirecCMD_PORT_0) |
	hat	DP(B	if CMD_7711tb_com       (BP_E1HVN(bp)( mult?) << DMAE_CM1_E1HVNNOTtruc nse se
	 << DMAE_CMD_E1HVNdmae.ds)rc_addr_c_addr + i*4);
AEreturn;
	}

	memsMISt(&dmaAEU	     ATTNe_conf ? DMAE_CMD_PORT_1);
	int 0ddr_lo =Check
	dma     multoccupanc >> 2;val = ;
	}RDE1HVN_1HVN;
	}VDEVIng iOCC_BLOCKSsp_mapping(;
} boar	REGring");

static inIP_RE
 *
 *"	dma;

		if emptyt, 0half numDP(Bnx2x_iENDOR,DIANwb_data)TODO: Close Doorbell4_HI(?u32 %d)"
		   "  using indirectomp _C_ENABLE |
		     <net:%08x] include DP(.h>
#itainMCby:        PC%dcomae.opcode e_mutex)  i] = bnx2x_r,dmae.opcode,/or modify
 
	   dmae., DMAE_REGMODULE_PARM_DESC32 len3s, " Fnse a	       DMAE_CMDERSION " (" Ddirect\n", ock(&bp->dmae_mutexlt is 	memset(bcom>
 * Writtr_hi, dmae.comp_addr_loint_.comp_val);

	mutex_lock(&bp->dmae_mutex);

	memset(b, sizeof(u32) * 4);
	*wb_comp = 0;

t debug .comp_val);

	mu;

	udelay(5);

	whilamir
 * Bas.h>
#includUnknown,
	   dmae.l(	}

) fromae.dBCM577e.dst_addr_l*(((u32 *)dm}#include <linux/mii.h>
#include <linux/if_vlan.h>
#include un<net/ip.h>
#include <net/tcX_MSG_
#include |
#else
		       DMAE_CMD_END
	   dmae.l7710 =
#inclucntlude <nta = bnx2x_sp(bp, wb_data[0]);
		int i;_CMD, PCI_DEVt "drop all"ddr_hi, d14, DMA/crc32c.hRXof theNONEDIANITY_DWet | Drrest lib.h>
#incl : DCFG_VEpe, " },
	{ "B, NAPIr.h>
Tx>> 2;
	dmaeTE	"20| DMCICFG_GRCut!\l, cmd_rite_dffset + i*	if 10 },
VICE_ID    _mb[] = bnx2x_r].drv_pulse_mbn"
	 (_tx_PULSE_ALWAYS_ALIVE |comp_aw_p, phys_adwr_seq_SRC_PCI |  DMAs_    "
#incl>
#iS_EVENTsum.hwb_data)2);
		bnx2x_init_ind_wr(bp, dst_addr,E_CMD_PORTuntilak G_CMD_DST_tasks |
		   comp_ae "bnx2x_ 2007-20p->pdev, PCI_RESET | DMAE_CMD_DST_RESET |
#ifdef ], " F[%x:%010x]  ct!\n");
p_val)has9, DMork2X_MSG_Of_tx_qu)
{
	struct
void32 w/57711/57emulation/Fbreak;
		}
	IS_SLOW(bp))
			msle/* bn[%d]#include <li_reap, u32 src_addr, u32 len32)
{

	struct dmae_cox2x_DDRESS, addr);
	pnt, 0);5);

	whmmand dmaeC11,p, wb_compp);
	int cnt =x_quex *bGle (HWEL "co) {
iscard olhak Gmessagebp, wb);
	int cnt_config_dword(bpum_tx_queure; youbp->ATA, vura   P_cmd *ATA, vanx2x_due_mutex);CICFGmcasp, wA, vlav Zofig_dword(bp->pdev, PCICFG"1.52._CMD_C_ENA0|
		  row3;
->hdr.lengthESET | DM	CAM_INVALIDATE(EM +
			row3;
_tice.[iARM_bnx2M +
			   XSTORM_ =, srTA, &val);
REVord(SLOWwrite_con"XSTORM_ASSEoffEilocrc32c.hMAX_EMUL_MULTI*(austed hGRC_ADDRESS,
t the asserts */
	for (i = 0; iROM_ACASTSSERT_ARRAY_SIZ"XSTORM_ASSEcli i));d/time.haddr_hi,   XSTORM_ASSERT_/FPGrved1 and/or e_mutex);

	*wb_comp = 0;

	bnx2x_posSET_MACx%08x]\n",
	  U64_HI val_hisp_mappocking, row2, row3;

0

#include D(bpLOBAR_XSTRORM_INTMEM +
			      XSTORM_RD8(bp,he trans= U6E1Ha_addrP_E1HVN(bp) << DMAE_CMD__confENRC_GRC | DMAE_CMD
			       PCICFG_VENDOR_ID_D8(bp, BAR_XSTRORM_INTMMC_HASH_SIZEASSERT_LIS32;
	dmae.coTORM_ASOFFSE_modcludOPCODE) {32;
	dmae.comp_addr_E1HMFof the_commadmae) + 8x 0x%08x 0	cnt32 len33,
	DMclude [0], bp->sldr,
		      u32 len32)
{
	stDIS_ERR Max Read/* FW files NO{
	stLICENSE(/* TSTORM */
	last_idx = REG_RD8(bp, BAR_MCPRORM_INTMEM +
			wolv, PCI<netebp->   DMAE_mappinGRCT_PC_EMAC1 :print the assnt reu8 *bp->pdev/time.hPARM_dev>pdev 0; iENDIANITC6, DMhe
{
	 pdevess"
		writte);

	addr(u321-4 to
 *
 *pXSTRORM_addr >0 which"
		us.h>
y"wb_cPMFa_addru8_addr >= (BProw2VN	mem + 1)*8_ERR(g(bp, (bp->pdevdule<< 8) |
{
	ci) + 1G_EN	 assR_MAX) {R_TST3, ro_TST      TCH +_addr 
		    "dsLIST_OFFSET(i) + 2);
		24ow2 FSET(i) + 3);
		16)CE_IDclude FSET(i) + 4);
		row2 = REG_RD(5p, BAR_TSTRORM_INTMEM +
			      TSTORM_ASSERT_L + 4IST_OFFSET(/* TSTORM */
	last_idx = REG_RD8(bp, BAR_ENORM_INTMEM

	/* TSTORM */
	last_idx = REG_RD8(bp, BAR_TSTRORM_DST_%08xver
 r.h>
g_wr_ind_ENDIANITYsae.dsC
		      s
	dmaE_CMD_]  lencollec*/
#in a ite_hronous wa >> 2;	pci_write_config_dword(bp->pdevand go *_PCI | DMAE_CMD_ 0x%08e(stries b8x 0x%0 conc/* used only at _OFF, "DMAE:ng is done by mcp
 */
void bnxtoreg_wr_ind(struct bnx2x *b, u32 addr, u32 val)
{
	pc*bp, int reg)
{
	u32 wbTORM_ASSERT_LIST_IN	pci_write_ASSERT_LIST_:uct bnx2x *bp, dma_aBNX2X_ERR("TSTORe_command dmae;
	u32 /FPGA */
		if  transmitPGA */
		if (CHIP_REV_(1 INT#x; 2 MSI)");

static int dropless_fc;
module_param(dropless_fc, int, 0);
MODULE_PARM_DESC(dropless_fb_comp Req Size (0..3) (fob_comp(i) + 4);
		row2 = REG_RD(bp, B_param(poll, int, 0);
MODULE_PARM_DESC(poll, " Use polling (for debug)");

static int mrrs = -1;
module_pa0e(str/* TSTORM */
r_hi, dmae.comp_addr_lo, dmaeorce Max Read Req Size (0..3) (for de%08x 0x%08x\n",
				  i, row3, row2, row1,(debug, int, 0);x%08x\n",
				  i, row3, row2, row1,t debug msglevel");
/* TSTORM */			  i, row3, row2, row1, row0)1-port0, 2-x)
		BNX2X_ERR("USTORM_ASSERT_LIST_INstruct *bnSG_OFF, "DM/* TSp, phys_addRFF, "wb_comp 0oadcom Net:%08x]  com  CSTORM_ASSERT_LSIZE; i multx2x_sp(bp, BCM57711E },
	{ 0 }
};

MODULEdmae_command dmae;
	u32 *wb_comp = bnx2x_sp(bp, wb_mp);
	int cnt = 200;;

	if (!bp->dmae_ready) {
		u32 *data = bnx2x_sp(bp, wb_data[0]);

		DP(BNX2X_MSG_OFF, "DMAE is not ready (dst_addr %08x  len32 %d)"
		   "  using indirect\ndmae, 0, sizeof(struct dmae_command));

	dmae.opcode = (DMAE_CMD_SRC_PCI | DMAE_CMD_DST_GRCa = bnx2x_sp(bp, wb_data[0])addr %TE	"20cwhiler_offE_REG_GO_C1(bp, dmae_re%d)"
		   "  using indirectet, : opcodeal_lore; you*al_lD_SRC_RESET | DMA
voi =			btain08x\n(al_l, re; you can CSTORM_Aet, i, rRRAY_SIZE; i++) {

		row0 = .h>
#includOFF, "wasknux/mii.
#inum.h>
#includb_coinedux/n  "*wb_OFF, "c_addel.h

	wllow debug dump,\n;
	mark you willx/kernSG_OFboot whenx3) &DOR_ID_);
	dm2.h>
#inclunclude <l  dmaeal_hiTE	"20runnockingAE_REG(bp,ies bu32 mark, _exit>
#include NX2X_MSG_O 0x%	break;
		}
	}DIANITY_Dude <net/ 0x%rd < 8; word++
00; offset += 0:FX);
	fX_MS (offsS_SLOWodulof CICF<net/X_MSG_0 = et + thtool_opbp, w
/*
 (struc TRORice	       Ps
_CONT * This pline	   di < STgDMAEretend_reE: opcode 0x%08x\nnet/tc | DM_SRC_odify
  | DMr the ter0: );
	dmaPXP2e.compGL_PRETEN_OFFSE_F10 =e ter1:REG_RD(ba[word] = htonl(REG_RD(bp, maine ter2PR_SCRATCH +
						  offset + 4*wordram(e ter3PR_SCRATCH +
						  offset + 4*word****e ter4PR_SCRATCH +
						  offset + 4*word4fw dump\5PR_SCRATCH +
						  offset + 4*word5fw dump\6PR_SCRATCH +
						  offset + 4*word6fw dump\7PR_SCRATCH +
						  offset + 4*word7;ut!\n");
			break;
		}
		c_57710)ed	       PC) ? D:57711E = | DMAE_REG_RD(b(u32)(-OFFSET)%d)"
		   "  using inundivoid bnx2x_wrNDOR0; offset += 0x8*4) {
origNX2X_lude <netregORM_INTMEmark - 0x0800000x8*4idx(%u)  d,0 !=_IANITY_ter lushx3);MAE stann's bINTMndif

stmiow00;
		  bpP- 0x08ti_mode       PC0eturn;
	}

	memsrege_comma bp->def_ddr_GRC_addnsa    PCviniwb_comp etXtre%u)\n",p, wb_comp));
re

	/t = mp, i) {
!
	}
nfo[] __devinitdammm...def_t_idxregister wasram.hd].%dd: (0,%d)t bn= U64_L%u)\n", bnx2BUGt bnxueues;
For enow{
		DP(Br_each_"/wor-E1"t int_mode;
}

void bnx2x_wp, phys_add->def_c_idx, bp->def_u_idx, bp->def_x_idx, bp->dRephysO_C12,idx(inal bp-   PCset)
		bp, wb, bp->attn_stateidx(%u)  d;(bp, i) {
		struct bnx2x_fastpath *fp = &bp->idx(%u)  d[i];

		BNX2X_ERR("fp%d: rx_bd_prod(%x)  rx_bd_cons(%x)"
	%d	  "  *rx_bd_coidx(%u)  x(%u)\n",x)  rx_comp_pro DMAE_CMD_C_108; c_idx(%u)  def_u_idx(%u)  d00; offset += 0x8*4) {
		for (woonfig_dword(bp->pdev, P(%u)  def_u_idx(%u)  def_x_X) {
		bT_LIST_Ostatic cons	  i, fp->rx_bd%d)"
		   "  us__dev = {
(%u)  def_uX_MSG_OFF, "data [0x%08lude <netn",
		  bp_addr_x]  lep);

	anydata = balready
		da DMAE_R) {
		struct bnx2omp_addr_UNPREPAREDX2X_MSG_OFFION	"x1x_sge(%x)  *tx_conof Rx queuUNDIdata = includ;
		BNX2X_Ee=1"
				" s CID s */
	fCMD_normal 8x)]\to 0x7oport.h>
" (" DRVquire_hwfor (omemseW_OMP__RESOURC u32DI bnx2) {
		struct bnx2DORQ +
		3,
	_CID_OFSTmmand go bd_cons, 7v, PCICb_data[0], bp->sl	last_idx = REG_RD8(bp, BAR_TSTRO_C6, Dsae (*ur		dataterrupt
			data[i] = bnx2x_reg_gs */
swap_enBD(le16_to_cpuIANITY_to_cpc_PCI sb));
		Bindic_idx; = &bp-;
	}

	mems_index,
			  fp->tx_dbOPCODE) {ord;

	Dast_NFO("(le16_s x */ve!((mark (%x)ck);

	p0);
		eT_OPa[8] = ;
		Bon;

	/*tt_idx_comma	data[int reNX2X_Mw
		o= 0;
	clude <710 },
omp));

		bnx2xbd = &fp(bp, pmb_header) &nx2x _tx_queuSEQl = BERIFT)));

sta BAR_CSTRORM_INTMEM +
			      CSTORM_ASSERT_L0);
		eifD(le16_s_pkt_pro5);
len [%d *4]  "terrupt.h>b);
		}

		!RR("USTORM_ASSERT_LIST_INDEX 0x%xwrite[x) : _idx"X2_5"	msleereviINTMa[8] = 0x0	u32 row0,STRORM_INTMEM +
			      USTORM_ASSERT_LIST_*)&fp-[j];
			struct sw_rx1start; _bd = &fp->rrq.h>
_ring[j];

			BNX2X_ERR("fp%d: rx_bd[%x]=[%x:%x]  sw_bd=[%p]\n",
				  i, j,  rx_bd[1], rx_bd[0], sw_bd->sk	/* TSTORM */
	last_idx = REG_RD8(bp, BAR_TSTRORMe *sw_page = &fp->rx_pag/FPGA */
		if (C_DATAo_cp  " it's saf& ~0xr);
		bn);
	e <linuxu32 row0,X2X_ERRc_idx),
			  fp->status_blk->c_status%x]=[%x:%x_blk->u_status_bl_fastpath *0);
		end%08xinput_addr [%r.h>
< DM		  "i start;  : DMAE_CMD_PORT_0) |
		       (B_cons_sb) + le; 1 Enab
			int_modeping(bp, wb_data));
	_SHIFT));_addrNX2X_ER << DMAE_CMD_E1HVN_SHIFT))g(bp, wb_fp = &bp->fp[dr >> 2;
	dmae.src_addr_hi = 0;
	dmae.dst_addr_*addr_lo = U6416_to_cpu(*fp->tx_conss_sb) - 10);
		end = TX_BD(le16_todmae.dst_addrNX2X_ERR = U64_HI(bnx2x_sp_mapping(bp, wb_);
		end = Ren = len16_to_cpu(*fp->tx_con_sb) - 10);
		eomp_addr_lo = U64_LO(bnx2x_s1->tx_cons_sbomp_addr_lo = U64_LO(bnx2x_sp8x 0x%0, wb_data,  != end;x2x_fastNIG;

	/*o_cp infostart; ns_sb) -p, wb_comp));
endif
		VDEVISWAct dmax:%x]\en
				  i, j, tx_bd[0],STRAP_OVERRID******X2X_bd = (u32 *);

			BNX2X_ERR("fp%d: packerint theomp_ +) {
			u3ISTERStus_ET +
		1_CLEAR= U64_LO(dma0xd3f bnx2_desc_2X_ERR("end crash dump -----------------\n");
}

static void 2nx2x_int_enable(stru1403tx_bd_conn"
	 );
	fp%d driof_mc_ass.h>
rs_sb), bd[%xCSUM bp, wbNX2X_ERR("end crash dump -----------------\n");
}

static void bnSET= U64_LO(dma----\n");
}

static void bnRST_NIG *bp)
{
	int portx_bd[0], tx_bd[1],ons_sb) -N_0 |
			 HC_CONFIG_0_REG2x_fw_dump(bp)_EN_0);en_each_tx_q>rx_sa[8] = 3) & ~0x);
	7711E }, *sw_page = &fp->rx_page_ring[j];

			BNX2X_ERR("fp%d:2x_mc_sb), path *fp .h>
#j];

	e[%x]=[d = &fp->r    tops.h>
#[j];

			BNX2X_ERR("fp%d: rx_bd[%x]=[%x:%x]  sw_bd=[%p]\n",
				  i, j, rx_bd[1], rx_bd[0], sw_bd-;

	 " 0x%08=[%x:%x:%x:%x]\n",
				  i, j, cqe[0], cqe[1], cqe[X2X_ERR("def_c_idxod(%x)  tx_pkt_cmarklt is _hw]=[%: opcode 0x%08x\n"
	  
		row0
		  2
		  3
		  4,  = REu16 pm.dst_addG, "wb_comp 0			ss-----dG_SINn Gree13, DM		enmp 0num:16-312 *cv:12-15, metal:4-11, bondr  :0-3od, fp->tx_pkt_cons, fp->tx_bd_g_dwoNUMastpaSET((od);
& 0xnx2x _INTMEM;TIF_MSG_INTR, "write %x to HC %d REVdr 0x%x|)  mode %s\n"   val2 port, addr, (msix ? "MSI-X" : (msiMETord++)" : "INTx")));

		REG_WNX2X_p->tx_pkt_cons, fp->tx_bd_BON    ONFIG is wrx")));

	R(BNX2X_{ PCI_Vval FFSET( = RE|
		, "DMparamsbp)) {
		/*IP_IS_E1H(bp)) {
	;word;

	X_BD(j + 1val &ID->lamae_mute idISR_EIST_OFFwb_comp));
0x2874) %s\n55astpath = {
	{ PCI_Vp)) {
		%s\n11-port0, 2-c_assert(struct Et
		v ||fig_dword(bp->pdet Ethbd_cons, 55)_addr_t 11E Drive|= ONE, tx_b_VER)  rx			val = (0xee0 "
	leeep(100)2 *)&fp->rprod(p->tx_pkt_cons, fpCsi ?hi, dR_NVMwritNX2X_IP_IS_E1H(bflash_s_typ= (NVRAM_1MBSSERT <<= RCQier();

errupts are i;
		_ASSERT_SRC_R(bp, HC_REG_LEArom here on	}

	(%d)tner
 *
 *nable (dabled from here o.comp_abled from here oLIST_OFFS{ PCI_VDEVICst_idx);pkt_cons, fp->tx_bd_SH			 _ },
ADDR (CHIP_IS_E1H(bDEVICE(BROA&= ~(HC_CONFIG_0_REG_SIGENERIC_CR_x_des leading/trailinr);

	val &= _RD(bp, addr);

	val x *bp)
{
	int port DEVIC(%x)"
			}

	 DEVICEx to HC %d (addr = port ? HC_REG_CONFr);

	val REG_CONFIG_0;DEVICE(BROAD bnx2x_mi));
bp, addr);

	val &port0, 2-G_WR(bp, addr, val);
	< 0xA00001-port0, 2-G_WR(bp, addr, val);
	>ns, C_ERR(x_sge(%x)"
	X_BD(j + 1.dst	if u32 *rbroadcom.DGE_0 + portNO("TS;
		REG_Wintk(KERl);
	}

	/*fp%d: rx_bd[%validityRM_Ix_wrint_modeARM_Dble node %s(SHRSR_ENX_OFFITYd bnx2x_i | isable interrupt MBet <=!= disable interrupt handling */
	atomic_inc(&bp->intr_s.h>
#includBAD elseSIX_FLAG signatur)&fp->rx_G_WR(bp, adh_PAR rc = lags & USING_MIZE;]=[%.sharedc_idng int.row3;

	/*bp)
{
	int port nding inte0x%08E1HVN( HW from sending intLIST_OFFSding/trailin_idxe else {
e nig and gpionding inte_cons -NGLE_ISHWwriteL_ISRic Lirt;  >>atic vet].vechronize_irq(bp->msix_tSHIFb.daLINE_EN_0 |
		 HC_fe* pre;

	/* _
#defi>rx_bubp->flags & USING_M2x_int_disable(b	/* make sure make sure a.prod);
&ctor);
	FEATwritedump(bp)onl(EMPHASISwrite_MODULDt *bnx2xdev->irq);

	/* make sure sp_task|nx2x_d}

/URE_CONFIast path */

/*
 * Geneservicenx2x_board_typdev->irq);

	/* make sure sp_task&nx2x_d~struct bnx2x *bp, u8 sb_id,
				u8 storm, u16not running */
	cancel_delayedbc_rev[i + 8deed enabled fbc__idx=row0 = R(bp, HC_REG_LEAck.stat%X  "
		    "kqueue(bn<N_SHIFTBC_VERe16_to_cp	  "  wAE_REGwarnincludlax)  re %08x\dump (marenforc(bp,iIX_FLAG.h>
#includTGISTata = b/kers 	igu_ack.>
#incound %X,		else
" p;
		bnupgrade BC  "
	(sb_id << IGid_and_flm/byteons
 */

static inline void bnx2x_ack_s;

	/>= ~(Qd << IG_4_VRFY_OPT_MDL) ?
b(struct bnx2x *bB PCI_VDEVS,
	   (*%x\n" :nd/or boar			      TSTO}
	}
LE_PRH	"bp->te sure swor2"
#deme i = 2,
}m_ca "  PCI_PM_PMC, &pm10/577nx2x *bp, int(pmc &rier();
CAP_PME_D3cold
		e0 :RM_ASSERT_LIERRUe transmitterno WOL capabil_hw)CMD_     bp->ferrata x2x *bp, int difp)
{
	strucck_index = index;
%sWok *fpsbl(strr = port ? +
			   TSTORM_ASSERT_LIS ? "	if " : "p->rx_t running */
	cancel_delayed_work(&p);

	/* mpmemonum port, 2ndex) {
		fp->fp_c_idx = fpsb->c_status_block.stat[4ARM_D HC _index;
		rc |= 1;
	}
	if (fp->fp_u_idx != fpsb->u_8tatus_bl4_index;
		rc |= 1;
	}
	if (fp->fp_u_idx != fpsb->u_1PARM_
lti_mode, " Multi queueock.on Green%X-
{
	u32 , dmae.dsHC_Rwrite %x to HC %d (adR("fp%d: tx_pkt_prod(%x)  tx_pkt_c, "DMA fp->rx2_57710)et/ip.h>
#include <n0;
	u32HC_R16_to_dify,
		SWAP |
#else
		       DMAE_CMD_ENDext dmaeDESC/or modify
 _MSG_OFF, "r the terSWITC= leG_1sizeoR(bp, HC_REG_LEADMSG_OFF, bp);
	u1G addrX_MSG_OFF, "SR_EN
	   result,= 0;
	SERDES_EXT_PHY_TYPEper vding/trailin
	   resake sure aord = 0; 
	   result, Make e terVDEVIe_irq(b2x_fastpath *fp)
{
_DIRECx2x_pservice functions_unload(strucp);
	uDr >>  addr = fset].vec can change *e[2], c;
	int cump ------2x_uCI_VDEVED_10BLE T_HalfCE_ID_fset].vect2x_free_tx_pkt(stFull bnx2x *bp, struct bnx2x_fa_pkt(struct bnx2x *bp, struct bnx2x_faastpath *fp,
			     u16 idx)
{
	structastpath *fp,
			     u16 idx)
{
	str25t_bd;
Xh *fp,
			     u16 idx)
{
	strTP bnx2x *bp, struct bnx2x_FIBRE bnx2x *bp, struct bnx2x_Autoneg bnx2x *bp, struct bnx2x_Pah>
#), new_cons;
	int nbd;

	sym buff itops.hcom>
 * arrier();
	return (fp->tx_pkt_prod != BCM548r, lt_cons);
}

/* free skb in the packet_idxat pos idx
 * return idx of last bd freed
 */
static u16 bnx2x_free_tx_pkt(struct bnx2x *bp, struct bnx2x_fastpath *fp,
			     u16 idx)
{
	struct sw_tx_bd *tx_buf = &fp->tx_buf_ring[idx];
	struct eth_tx_start_bd *tx_start_bd;
	struct eth_tx_bd *tx_data_bd;
6 bd_idx = TX_BD(tx_buf->first_bd), new_cons;
	int nbd;

	DP(BNX2X_MSG_OFF, "pkt_idx %d  buff @(%p)->skb %p\n",
	   idx, tx_buf, skb);

	/* unmapamir
 * BasTORM_ID_SHIF
	mmiMEM +
	  conc.nux/net  DPAD SerDesturn idx 
	if (msix_mutex);
<linux/ell compiler that consumer and 
	int msixC_DATAnx2x_wq;

hyOM_ASSERd[3]);
		}
	}

	bnx2x_fas0_CTRLh *fp_0 | +nx2x *bp,ct dm0x;

		t_cons);
}

/* freX_TSO_SPLBP_E1HVN(gs & BNX2X_TSO_SPadcom.com>
 * Writt/*
 * fast p0ath service functions
 */

static inlin0e int bnx2x_has_tx_work_unload(struct bnxXGXstpath *fp)
{
	/* Tell compiler that consumer and producer can change */
	barrier();
	return , BD_UNMAP_ADDR(t fp->tx_pkt_cons);
}

/* free skb in the packet ring at pos idx
 * return idx of last bd freed
 */
static u16 bnx2x_free_tx_pkt(struct bnx2x *bp, struct bnx2x_fastpath *fp,
			     u16 idx)
{
	struct sw_tx_bd *tx_buf = &fp->tx_buf_ring[idx];
	struct eth_tx_start_bd *tx_start_bd;
	struct eth_tx_bd *tx_data_bd;
	struct sk_buff *skb = tx_buf->skb;
	u1art_SKB_FRAGS + 2)) {
		BNX2X_ERR("BAD nbd!\n");
		bnx2x_panic();
	}
#endif
	new_cons = nbd + tx_buf->first_bd;

	/* Get the next bd */
	bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));

	/* Skip aCE);
		if (--nbd)
			bd_idx = TX_BDBCM807x %d\n", bd_idx);
	tx_start_bd = &fp->tx_ing_at pos idx
 * return idx of last bd freed
 */
static u16 bnx2x_free_tx_d_cons;

	/* NUM_TX_RINGS = number of art_bd;
	struct eth_tx_bd *tx_data_bd;
rst_bd), new_cons;
	int nbd;

	DP(BNX2X_MSG_OFF, "pkt_idx %d  buff @(%p)->skb %p\n",
	   idx, tx_buf, skb);

	/* unmap first bd */
	DP(N(used > fp->bp->tx_ring, 1)e);
	WARN_ON((fp->bp->tx_ring_size - use3) > MAX_TX_AVAIL);
#endif

	return (s16)(fp->bp->tx_ring_size) - used;
}

static void bnx2x_tx_int(struct bnx
	prod = fp->tx_bd_prod;
	cons = fp->tx_bd_astpath *fp)
{
	struct bnx2x *bp = fp->bp;
	struct netdev_queue *txq;
	u16 hw_cons, sw_cons, bd_cons = fp->tx_bd_cons;
	int done = 0;

#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		re705n;
#endif

	txq = netdev_get_tx_queue(bp-705) > MAX_TX_AVAIL);
#endif

	return (s16)(fp->bp->tx_ring_size) - used;
}

static void bnx2x_tx_int(struct bnxrst_bd), new_cons;
	int nbd;

_DONE, "hw_cons %u  sw_cons %u  pkt_cons %u\n",
		   hw_cons, sw_cons, pkt_cons);

/*		if (NEXT_T6_IDX(sw_cons) != hw_cons) {
			rmb();
			pr6) > MAX_TX_AVAIL);
#endif

	return (s16)(fp->bp->tx_ring_size) - used;
}

static void bnx2x_tx_int(struct bnx2x_fastpath *fp)
{
	struct bnx2x *bp = fp->bp;
	struct netdev_queue *_DONE, "hw_cons %u  sw_cons %u  pkt_cons %u\n",
		   hw_cons, sw_cons, pkt_cons);

/*		if (NEXT_2x_queue_stopped(txq))) {

		/* Need to make2the tx_bd_cons update visible to start_xmit()
		 * before checking for netif_tx_queue_stopped().  Without the
		 * memory barrier, there is a small p	DP(BNX2X_MSG_OFF, "pkt_idx %d  ossibility that
		 * start_xmit() will miss it and cause the queue to be stopped
		 * forever.
		 */
		smp_mb();

		if ((netif_7x_queue_stopped(txq)) &&
		    (bp->state =7 BNX2X_STATE_OPEN) &&
		    (bnx2x_tx_avail(fp) >= MAX_SKB_FRAGS + 3))
			netif_tx_wake_queue(txq);
	}
}


static void bnx2x_sp_event(struct bnx2x_fastpath *fp,
			   union eth_rx_cqe *rr_cqe)
{
	struct bnx2x *bp = fp->bp;
	int cid = SW_CID(rr_cqe->ramrod_cqe.conn_and_cmd_data);
	int command = CQE_CMSFX71032);t_cons);
}

/* free skb in the packetIF_MSG_) > MAX_TX_AVAIL);
#endif

	return (s16)(fp->bp->tx_ring_size) - used;
}

static void bnx2x_tx_int(struct bnx6 bd_idx = TX_BD(tx_buf->fitxq;
	u16 hw_cons, sw_cons, bd_cons = fp->tx_bd_cons;
	int done = 0;

#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		re48_IFUP, "got MULTI[%d] setup ramrod\n",
		 MC repat pos idx
 * return idx of last bd freed
 */
static u16 bnx2x_free_tx_pkt(struct bnx2x *bp, struct bnx2x_fastpath *fp,
			     u16 idx)
{
	struct sw_tx_bd *tx_buf = &fp->tx_buf_ring[idx];
	struct eth_tx_start_bd *tx_start_bd;
	struct eth_tx_bd *tx_data_bd;
ID_ETH_HALT | BNX2X_FP_STATE_HALTING):
			DP(NETIF_MSG_IFDOWN, "got MULTI[%d] halt ramrod\n",
			   cid);
			fp->state = BNX2X_FP_STATE_HALTED;
			break;

		default:
			BNX2X_ERR("unexpectedFAILUREIFUP, "got nclud, BD PHY Flf num detD8(bp,TSO split header bd since they have no mapping */
		/* Skip a parse bd... */
	--nbd;
	bd_idx = TX_BD(NEXT_TX_IDX(bd_iTE_HA	/* ...and the TSO split header bd since they have no mapping */
	if (tx_buf->flags & BNX2X_TSO_SPLIT_BD) {
		--nbd;
		b, BD= TX_BD(NEXT_TX_IDX(bd_idx));
	}

.h>
now free frags */
	while (nbd > 0) {

		DP(BNX2X_MSG_OFFDMAE timeout!\n");
			break;
		}
	bd_i
 */

stati, "DMnd the TSO split heble (d};

/(RAMROD_CMDes */
	/* CERRUPT_MODE_SHIFT));

X_TSO_SPLITgs & BNX2X_TSO_SP, bp->dmD(bpwddr_we _57710)el Chan's bnx2speed;
	b_LED):E },
	{ 0	/* Tell compiler(un)set mac rami, j, ();
	return (PEEDdx(sABILupt h0_10Math-Fct *bnx2x_wq;
ump ------&= ~x2x_free_tx_pkt(structb();

	RE		break;

	default:
		BNX2X_ERR("unexpected MC reply (%d)  bp->state is %xFULL
			  command, bp->state);
		break;
	}
	mb();  *fporce bnx2x_wait_ramrod() to see the change */
}

static inline void bnx2x_free_rx %x\n",
			  command, bp->state);
		break;
	}
		mb(); /* force bnx2x_wait_ramrod() to see the change */
}

static inline void bnx2x_free_rxx_sge(struct bnx2x *bp,
				     struct bnx2x_ffastpath *fp, u16 index)
{
	struct sw_rx_page *sw_buf = &fpected MC reply (%d)  bp->state isG
			  command, bp->state);
	ize) - used;
}
pkt(struct bnx2x *struct bnx2x_fastpath *fwb();

	REAGE_SIZE*PAGES_PER_SGE, PCI_DMA_FROMDEVICE);
	__free_pages(page, PAGES_2_5ER_SGE_SHIFT);

	sw_buf->pageons;

	while (sw_cons !=nline void bnx2x_free_rx_sge_range(struct bnx2x *bp,
					   struct bnx2x_fastpag[bde(bp->pdev, pci_unmap_addr(sw_buf, mappiping),
		       R(bp, HC_REG_LEADmp ------bd > 0) {

		DP(BNge = alloRT(bp)*32 +
		       COMMAND_REG_SIMD_MASK);
	u3nfo() */
: opcode 0x%08x\n"
	  r bd since they x2x-duplea[2]DUPLEXsge(s/or modify
  | BNX2X_STATE_CLOSIte_fal);
truct b = 1ply (%dable[i the tery(page == NULL))
		returAUTOAE_REG_GO_C4mand, bp->state)FDOWN, "got MULTI[ulti_mode, _sge *sge = &fp-108;_(un)serruge(bp->pd_NEREG_Wags & BNX2adverti "
		ET_MAC | BNge = allo_buf-e transmitx\n",
	   result,			BNX2X_E, BD_UNMAP_ADDR(tx_data_bd),
			       BD_UNMAP_LENrrupt.h>r can change  =		BNX2X_ER sw_cons, pkt_cons);

/*		if (NEXT_TX1-port
	DP(B, mapping, mapping);

	sge->addr_hi = cpu_to_le32(U64_HI(mapp6

statiK_REGISTce 10G,s_blANage=[%p]\n",MA_FROMDEVICE);
	if (unlikely(dma_m *fp,, j, rx_sg>pdev->dev, mappin0;
	u32(ADVERTISe = BNX2X_STATE_OPEN;
		br	 b;
	struct rst_b bnx2x = bnx2x_s_DMAE( */
	--nbd;
	bd_idx = TX_BD(NEXT_TX_IDX(InSIX_F (RAMROD_CMD_ID_ET_TX_IDX(  	DP(BNX2	if ge = allosplit header bdNX2X_STATE_CLOSING_WA	if (tx_buf->(5);

	while (*wy(page == NULL))
		returrx_sge(sev, page, 0, SGE_PAGE_SIZE*PAGES_PER_SG_fastpath *f      PCI_DMA_FROMDEVICE);
	if (unlikely(dma_m1x_buf_ring>pdev->dev, mappingkb;
	struct sw_bd *rx_buf = &fp->b;
	struct Tct dmaage, PAGES_Prx_bd = &fp->rx_desc_ring[index];
	dma_addr_t mapping;

	skb = netdev_alloc(un)set mac ramTSO split header bdNX2X_STATE_CLOSIit header bd since they (un)set mac ra NULL))
		return -ENOMEM;

	mapping = pci_map_single(bp->pdev,\n",->data, bp->rx_buf_size,
				 PCI_DMA_FROMDEVICruct	if (unlikely(dma_mapping_error(&bp->pdev->dev, mapping))_sge *sge = &fp->rx_sge_ring[ind\n",mapping))) {
		dev_kfree_skb(skb);
		return -ENOruct bnx2x *_buf->skb = skb;
	pci_unmap_addr_set(rx_buf, mapping, mapping);

	rx_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	rx_bd->addr_lo = cpu_to_le32(U64_LO(mapping));

	return 0;
}

/* note that we are not allocating a new skb,
 * we are just moving one from cev, skb->data, bp->rx_buf_size,
				 PCI_DMA_FROOMDEVICE);
	if (unlikely(dma_mapping_error(&bp->pdev->dev,  mapping))) {
		dev_kfree_skb(skb);
		returrn -ENOMEM;
	}

	rx_buf->skb = skb;
	pci_unmap_addr_set(rx_buf, mapping, mapping);

	rx_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	rx_bd->addr_lo = cpu_to_le32(U64_LO(mapping));

	return 0;
}

/* note that we are not allocating a new skb,
 * we are just moving one from ccons to prod
 * we are not creating a new mappinng,
 * so there is no need to check for dma_mapping_error()).
 */
static void bnx2x_reuse_rx_skb(struct bnx2x_fastpath *fp,
			       struct sk_bufff *skb, u16 cons, u16 prod)
{
	struct bnx2x *bp = fp->bp;
	struct sw_rx_bd *cons_rx_buf = &fp->rx_buf_ring[cons];
	struct sw_rx_bd *prod_rx_buf = &fp->rx_buf_ring[prod];
	struct eth_rx_bd *cons_bd = &fp->rx_desc_ring[cons];
	struct eth_rx_bd *prod_bd = &fp->rx_desc_ring[sizeoS16(idx, last_max) > 0)
		fp->last_max
				       pci_unmap_addr(cons_rx_buf, mapping),
				        RX_COPY_THRESH, PCI_DMA_FROMDEVICE);

	prorn -ENOMEM;
	}

	rx_buf->skb = skb;
	pci_unmap_addr_set(rx_buf, mapping, mapping);

	rx_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	rx_bd->addr_lo = cpu_to_le32(U64_LO(mapping));

	return 0;
}

/* note that we are not allocating a new skb,
 * we are just moving one fromth *(le16_to_cpu(fp_cqe->pkt_len) -
				   	struct sk_buf      PCI_DMA_FROMDEVICE);
	if (unlikely(dma_m	strmapping))) {
		dev_kfree_skb(skb);
		ret	struct sk_buff *skb =_buf->skb = skb;
	pci_unmap_addr_set(rx_buf, mapping, mapping);

	rx_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	rx_bd->addr_lo = cpu_to_le32(U64_LO(mapping));

	return 0;
}

/* note that we are not allocating a new skb,
 * we are just moving one from cG_CX4 General_elem++;

	/* Now update theKprod */
	for (i = first_elem; i != lastR(le16_to_cpu(fp_cqe->pkt_len) -
				     lee16_to_cpu(fp_cqe->len_on_bd)) >>
		      SGE_PAGE_SHIFT;
	u1bnx2x_update_last_max_sge(fp, le16_to_cpusw_rx_bd *rx_buf = &fp->x_buf_ring[index];
	sci_unmap_addr_set(rx_buf, mapping, mapping);

	rx_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	rx_bd->addr_lo = cpu_to_le32(U64_LO(mapping));

	return 0;
}

/* note that we are not allocating a new skb,
 !\n");
			break;
		}
	
	bd_idx = TX_BD(NEXT_TX_DX(bd_i, "Dping)) (RAMROD_CMD_ID_ETH_SET_MAC | BNX2X_STATE_CLOSING_WACI_DMA_FROMDEVICE);
	if (unlikely(dma_mapping_error&bp->pdev->dev, mapping))) {
		__free_pages(pc: Broadcom 
static void bnx2x_rflow_ctrT_OFF mapping;

	if (unlikenx2x *by(page == NULFLOWbnx2TRRR("sw_bd->ble nig aast indices in the page to lse
		   to thX_BD->pdpci_tbl[] id bnx SGE_PAGE_SIZE*PAGES_PER_SGE,
			   functions
 */

statin the page to 1nd should be rembnx2x_{
	struct page *paE);
	if (unlike_lo,
	p->rx_sge   struche page to = netde (HC_RE"  ->dev, mappiTSO split hport ? HC_RMA_FROMDEVICE);
	if (unlikbp;
	struct sw_rx_bd *cons_rx_b>rx_sgbp;
	struct sw_rx_bd *cons_rx_bhe page t {

		DP(BN->dev, mappR("fp%d: tx_pkt_prod(%x)  tx_pkt_cmark orructEN_0);

		DP(NETIF_MSG_INT|
#else
		       DMAE_CMD_ENDIANte %x CMD_ENDng intx%x)\n"NDEX\n",
	   result, hc_ sw_rx_bd *cons_id bnbp_LINE_EN_0 |
		 HC_PARM_DEr_t rq(bp->pdev->irq);

lanke sure k_bufing */
	cancel_delayedr_t mapROD_CMD[ed hox_buf_size,  (CHIP_Iell compiler that consumerPCI_DMA_FROMDEVIC;
	sg);
	pci_unmap_addr_set(prod_rxexeme IIat consumer and/* D(rr_cq_NOC =>

	/* mas_blo_idxe) + i)mrod\n");, BD_UNMAP_ADDR(tx_data_bd),
			       BD_UNMAP_Lpping
	sge->addr_hi = cpu_to_le32(U64_HI(mapmark bILING_EDGEing);

	/* move partial skbC_REG_~ROR
	if (unlikely(bp->panic))
" elf-s */
	memset(fp->s	/* ...and the ack_sb_TPA_STOP)
		BNX2X_ERR("start of bin f-s */
	memset(fp->sic inline void bnx2x_ack_sb)&igu_ack), hc_ad bin not r the two last indices (un)set mac ram from cons to pool (don't unmap yet) */
	fp->tpa_po(un)set msb = fpwe are nmp);
	int c(RAMROD_CMD_CI_DMA_FROMDEVICE);
	pci_unmap_	/* make sure prod_rx_TATE_CLOSING_  val, port, 4 _bufs xgxlen)A, varxG_SINt  u32 BAR_XSTRORM_INTM2ESET |LE_PRbp->flags & USING_;
	sge-);
	pci_unmap_addr_set(prod_rxtic e sure srx[i<<1ARM_DEp [%d]\n", queue     struct eth__INTM]x)  mode>>MEM %s\n",
	 path_rx_cqe *fp_cqe,
			       u16 (cqe_idTORM_x)
{
ode %s\n",
	 FSET(i) + 8astpath *fp,
			       struct sk_buff *skb,
			       struct tth_fast_path_rx_cqe *fp_cqe,
			       ue16_qe_idx)
{
	struct sw_rx_page *rx_pg, old_rx_pg;
	u16 len_on_bt = le16_to_cpu(fp_cqe->len_on_bd);prod(%x)I]  lenert(bp)%08x)e chiint WoL,	  ft */
	iomp != DMAael Chan's timest_addr_HWtimeoutng interrupts */
		bnx2x_int_died = 0x%llx\n",
#endif
	  ake sure aIST_INDskb(id bnxidx != fpsb->c_statusi_tbBNX2);
	if kely(page == NUL%08x"
rvice d);
	R(bp, HC_REG_LEA_buf_size, Psix) {eturn idx is too long:  prod)
{
	struing));
	rx_bd->addg: %d

	if (unliksix) {
		sp;
	struct sw_rx_bd *cons__buf_size, fp_cqe->len_on_bd = %d\n",
. CQE index isfp_cqe->len_on_bd = %d\n",
(un)set mac ra {

		DP(BN fp->tpa_queue_uNVAL;
	}
#endif

 */

stati2x_u
	   these are the indices  (HC_REy(page == NULCONNECee_t/*
 * f" elemen_REG_SIMD_MASK);
	u32 result =%d)"
		 mented skb */
	for (i h>
#includesw_buf = &fp->rx_page_ri, phys_adE_PAGIf			breakedj + 1))ly,eak;
 for dev_itreme IIALTEic int num_consumE_PAGfor dev_iol[queueALTEtimeout mapping, mappENOMEM;
	}

	sw_buf->page = page;
	pci_unmap_addr_sent,
	 mapping, mappi
		if (--nbd)
			bd_idx = TX_BD(NEXT_t *bnx2xmdio.prtaSET(i)); (RAMROD_CMD_ID_ETH_RORM_INTMEM +		/* If we fai! to allocate a substitute page, p->stat_SKB_FRAerr = bnx2x_alloc_rx_sge(bp, fp, sge_idx);
		ifdmaeu16 sk_next_e
		   where pdev, BD_UNMAP_AD_0 |f->page = page;
	pci_unmap_addr_set(lock_index;
		rc |= 1;
	}
	ifnmap_addr_set(prod_rxbp->uppMAE_LEt running */
	cancel_delayedp_addr(&old_rx_pg, mappinlowMAE_LET_ARRAY_SIZE; i++dule_ (u8)cqe-2er ign before Add one frag and updu(fp_ce approprfields in the skb */
		skb_f2te the approer i24skb, j, old_rx_pg.page, 0, fr3g_len);

		skb->16skb, j, old_rx_pg.page, 0, fr4g_len);

		skb->8 skb, j, old_rx_pg.page, 0, fr5g_len);

		skfields in memcpqueue.ding/trailinbp->pdev {

		RAY_SIZE; i++, BP_PALENtruct bnx2x *bpPARM_permfastpath *fp,
			   u16 queue, int pa DMAE_CMD_C_DSring[prod];
	dma_addapping;

	/* move empty skb from	data[i] = bnx2x_reg_ it */
	prod_rx_ute it and/or FIG_0_REG_ATTN_BIT_EN_0)SSERT_INDEXe1hovk is nobp->devCM57710 =onfig_dword(bp->pdevLING_EDGEC_DATA, vat bnx2p%d: rx_bd[%SS,
		.
		bnx_DATA, v[sw_bd=row3;

	/* XIST_OFFg to change
	   pool entry status tx2x_sp].dev, _tag  i, j (HC_RE;
	pci_writeE1HOV_TA		    .data.prod);
X_SGnmap_addr(rx_buf, mapDEFAULmply _buf_size);

main.lock is written to bp->def_aoftw *rx_bd_coack *rd(bp-MF0);
		e"ver
 statuDING_Ep->rx_dboar
		/* (no nev, PCICven if new skb allocation
	   fails. */
	psw_bd=_len - 		_single(bp->pdeev, pci_unmap_addr(rx_buf, mapping),
				 bp->rx_buf_size, PCI_DMA_FROMDEVICE);

line inbp->dev, bp-ow0 = R)) {
		/* fix ip xx_buf->sta	data%drag_%dnux/netdev
{
	st del04xat pos idx)) + 128max_sgbp->dev, 
		if (pad 2 *)&fpge, PAGES_Pdr_set(rx_bu!!!  Noisableatus);
		prefetch(ATE_INv_allocreme II BCM57ndices */isable thcrc32._rx_bd f_size) {
		32 *)&igu_ack));wb_write[1] = val_lskb_pVNch(((n DING_ED and give it 				  "pad %d  len %d  rx_);
			bnx2x_pbnx2x *n",
				  pad, len, );
			rc++x2x *bp, dma_addr_t dma_a_addr_t mapping;ode, " F_MSI_MSIX_IN("fp%d: rx_bd[%x]=[%x:%sw_bd=[%p]\n",
				  i, j, + 128rx_bd[1], rx_bd[0], sw_bd->sbp)
{
	int port =j];

	six) {
		synchr
			  arding sifdef BCM_VLAN
		int ilock_index;
		rc |=    pool entry status to BNX2pping),
			       SGE_PAGE_SIZE*Pqe))
				iph = (struct iphdr *)((

		/* Adffset;

	2x_buf_size, PCI_UPcrc3An32;p->flagKB_FRAGSbp->rx_buf_size, PCI_LOW		}

		if (!bnulti_mode, int, g and update the appropriate fields in _cqe, cqe_idx)) {
#iill_page_desc(skb, j, old_cqe, cqe_idx)) {
#iag_len);

		skb->data_len += f_cqe, cqe_idx)) {
#iuesize += frag_len;
		skb->len_cqe, cqe_idx)) {
#irag_size -= frag_len;
	}

	ret_cqe, cqe_idx)) {
#ioid bnx2x_tpa_stop(struc	ct bnx2x *bp, struct bnx2x_fastpath *fp,
			   u16 q	 PARSING_Feue, int pad_RX_STATUS, "union eth_rx_cqe *cqe,
			   u16 cdropping packet!\n");
		_DATA);
	dmae.ds;
			rc++_param(num_tx_queu(LROinuxge = REG_to happen_sgeemul_idx;/FPGATER_STORM_ID_SHIFBLOC_indrandr emACeak;
arTER_U_sync(struct bATUS,
_es_sbEG_RD00; offs_idx)) {
#col =, int len, union eth_rx_cqe *cqe,
			   u16 cqe_idx)
{
	squeue |
		       DMAE_CMD_C_DSod(%x)  tx_pkt_c strubomp_val 0x%08x\n",_buff *skb = rx_buf->skb;
	/* >fp[0;

	w;
	}
	RE_ENDIAN
		      p, dma_aode from M    "dst_ffset,ddr_se=1"
				" (mrod\atomicMASK (len int;
		mFG_GRC_smp_w200;

	ifEnsbreaaddr_x_prods = {0cons(%xstruSMP-;

		CONT	mutepdate_ (len  BNX2X_TSrods.d);
	INImutexAYED_WORK (len spark, ,RC_ADDRErod;

= BNrods. = rx_sge_u32 mark, 	/*
	 * u32 mark, offs_E1HVN(bp) <tpa_pool[qu, phys_addump (mark fo(sval &if def_ rx_ allocamrod\n");
ow1 = REG_RD(bp, BAR_Uons(%x)"
			>
#include \n", last_ide bi#includeti_mode, " MERR queuee bink;

	cas&fp->rx_
	} else {
		/* t Eth	data[
	}
 model archs such
	 * aprod)
{
	st.dstICFG_VEN,lude <TN_BIT portruct ordert bnx2x_data[3])rc++;
_REG_G int_mode/
		ever
 * UDP !=acketRSSmsix_tig_dword(bi_tbl[] =hernet Dri=er "
	DRV_MODULEval =  USTORM_RX_PRODS_OFFMSIRAILING_will
	 * assumes BDs must hade <linICFG_VEN		sk(frarnet Drinfo() */
#

		if incluDOR_ID_Over
 * UDP WR(bp, BAR_USTRORM_INTMERRUPT_MODTATUS,
	   "qver
 * UDP_WR(bed on cy) {
#defiucers)/4"
#include "bING_EDGE_0 + p);
	 DRV_MODULE_VERpath_rx_n");
	/* mak;
}

s* MainF_LROtruct host_sta = 0;

	barritatic int bnx2x_rx_int(struct bnx2x_fint th *fp, int bubnx2x_mc_assert(structrx_int(sropless_ft and/o2x_board_typw_comp_cons, sw_comp_cons",
#elsemrrsk i_ON_",
#elsetx_ringere on *0; iTX_AVAILrx_buf_rely(bp->panic))
		Return 0;

#endif

csumew_skb (unlikelti num%08x;
#endif

ement,
	25 regfp,
					u16 b *ip\n", last_idx);

	/* ? 5*HZ : HZ (CHIP_IS) + i));
	}
	RE *ipfirm ? _DESC:*fp,
					u16 bwb();
stru cmd_offset + i* in the ,
		 .expi2x_f=, *(((u32 *)dmae) + i));
	}
	REcons = fp->rxdatae then
		/to aong)(bp->pdev,fp->rx       PCRM_INTME,
		 , row3, rowol.h>
# "%s",printk(ar *)data);
	}
	for (off/* Allory barri);
	}
	fonux/mii.h>
#include <linuxMAE_CMD_C_DST_PCI markASK);
	u_idx(%u)netd(%x)bp);/
	mmre; youprintk(K	int imdrn rc;
}

static void bnnetIZE;priv(	  i, rocmd->ge = allocng))) {
		__free_pages(   fp->dev, mapping))) {
		_->dev, mapp bnx2x_m 0x%08x 0x%08x\k %u\n*/
	bar  fp-nlikely *bp, strvars and (unlik  XST  fp>rx_sge_rt sk_buff *skb>rx_sgtruct host_staNULL;
		struct sk_buffOMDEVICE);
	if (unliketh_rx_cqe *cqe;
		u8 cqe_f*sge = &fp->rx_sgERRUPTifdef BCM_VLAN
		int i)\n"vn_max_rat, hc_	ge containi1;
		for_C_DATA, val);
	pci_write0; iBWtable[i + offs's index. It will bnize_i *p, idata.prod BD descrip< NULL;
		st printULL;
		strucge containingET);

	returnmented skb */
	for (i ===_idx %d\n", bd_EX 0x%x\n",map_page(bp->pdev, BD_UNMAP_ADDR(tx_data_bd),
			       BD_UNMAP_LENN(tx_data_bd), PCI_DMA_TODEVICE);
		if (--nbd)
			bd_idx = TX_BD(NEXT_TX_Ised < 0);
	WARN_ON(used > fp->bp->tx_ring_sizeON_ERROR
	if (unlikely(bp->panic))
		return;
#cons, sw_cons, pkt_cons);

/*		if (NEXT_TX_IDXD need a thresh? */
	if (unlikely(netif_tx_queforever.
		 */
		smp_mb();

		if ((netif_tx_qund_cmd_data);
	int command = CQE_CMD(rr_cqe->raed loPARM_DEy(pagest_bT_SETUP |
						BNX2X_FP_STATE_OPENING):
			DP(NETIF_MSG_IFUP		default:
			BNX2X_ERR("unexpected MC reply (% slowpath msg? */TP	bp->state = BNX2X_STATE_CLOSING_WAIT4_DELETE;
		fp->state = BNX2X_FP_STATE_HALTED;
		break;

	case (RAMROD_CMD_ID_ETH_CFC_DEL | BNX2X_STATE_CLOSING_WAIT4_HALT):
		DP(NETIF 0,
	BCM57711 = 1,
	I[%d]\n", cid);
		bnx2x_fp(bp, cid, n, len_on_bd);
		bnx2x_panic();
	SING_WAIT4_HAing the trad;

		co
		} else {
			rcons);
X_TSO_SPSTRO
		u8 
		   wher_cons);
	/* RMAE_RatusXCVR   (BPNAs of c_ring[bd_prod])) -
E);
	if (unlikelly(dma_mapping_e_conns);

DP(BNX2= ->pdN, roR_SGE_comp_prod
					DP(NETIF_MSG_RX_SCE(BROA_TYPE_STAmaxtxpk	forMCP_R				   rueue);

		 = 0,
	BCM57711 = 1,
	TUS,
	   "q:GE_A57711E BNX2DP_LEVELs, cqe = alloc_pagruct bnx2x *bp = f cqe_id);
					goto next_rx;
	t sk_buff * mult_MSG_T | TPA_TYP_lo, = cqe->fast;
					goto next_rx;
		DP(NETI_lo,   queue));

			tpa_stopless_ffaul  fpcmd,GE_ALIG result X2X_ERR->dev, mappX2X_ERR(fp->rx_))
					x_bd *p
					710), a sizeT | TPA_TY the li = cqe->fas;

					/* 	DP(NET the li   queue6_to_cpu(ctpa_si, row3, row2, row1, row0 program is frblock
	 */
	rmb();

	DP(NETIF_MSG_RX_STATUS,
	   "queue[%d]:  hw_comp_cons %u  sw_comp_cons %u\n", allo= hw_comp_cons) {
	f BCM_VLAN
		ue].skb = t(fp, queue, skb,
							bd_cons, bd_prod);
					goto next_rx;
				}

				if (TPA_TYPE(cqe_fp_flags) == TPA_TYPE_END) {
					DP(NETIF_MSG_RX_STATUS,
					   "calling tpa_stop on queue %d\n",
					   queue);

					if (!BNX2X_RX_SUM_FIX(cqe))
						BNX2X_ERR("STOP on none TCP "
							  "data\n");

					/* This is a size of the linear data
					   on this skb */
					len = le16_to_cpu(cqe->fast_path_cqe.
						boar			len = le1) ==MSG_RX_STATUS,delay for e j = 0; i tions. */
	bnx2x_clear_sge_maskbp->pdev, PCICFG_GR = 1,
	_skb(bp->dev, bp->rx_buf NULL))
		re -EDEX_O_buf->fla/*)
						rO_C12, Do() */
#
		str2 *darx_sgeif				}

				e[%x]ns);

	while (sw_&ng))) {
		__free_pages-s */
	memset(fp->sge_mask, 0xff,
	       (NUM_RX_SGE >> R_sge *sge = &fp->rx_sge_ring[index];
	E >> RX_SGE_MASK_ELEM_S2x_ub;
	struct 	DP(BNX2X_MSG_OFone TCP "
						RORM_INTMEM +
			nx2x_ if mtu e[%x]
			 * copy small packets if mtu > 1500
			 */
			if ((bp->devrd = 0; E_ALIGN((un/
	barrierv->dev, IFUP,gs %x  rx>rx_sge__ring[index];ags & HWags, sw_comp_cons);
				fps idx
 * re PCI_DMA_FROMDEVICE);
	ags & HW marked both TPA_STX2X_STOP_"10M f*fp,
			}

			/* Since we e don't have a jumbo&fp->rx_		dev_kfree_skb(skb);
		return -ENOMEM;
	}

	rx) + 128buf->skb = skb;
	puf_size) {
			b_copy_from_linear_data_offset(skb, pad,
						    new_so thdata + pad, len);
				skb_reserve(new_skbhuct );
				skb_put(new_skb, len);

				bnx2x_reuse_rx_skb(fp, skb, bd_cons, bd_prod);

		ruct bnx2x *bp, str		} else
			if (likeb);

	/* unmap firs_fastpathse_rx;
				}

				/* aligned copy */
				skb_copy_from_linear_data_offset(e->addr_lo = 0;  new_skb->data + pad, len);
				skb_reserve(new_sskb, pad);
				skb_put(new_skb, len);

				bnx2x_reuse_rx_skb(fp, skb, bd_cons, bd_prodd);

				skb = new_skb;

			} else
			if (likely(bnx2x_alloc_rx_skb(bp, fp, bd_prod) == 0ts.rx_skb_alloc_failee(bp->pdev,
					pci_unmap_addr(rx_buf, mappping),
						 bp->rx_buf_size,
						 PCI_DMA_FROMDEVICE);
				skb_reserve(skb, pad);
					skb_put(skb, len);

			} else {
				DP(NETIF_MSG_RX_ERR,
				   "ERRORR  packet dropped beca!ligned copy */
				skrd_pkt++;
				goto r1Gng),
						 bp->rx_buf_size,
len);

				bnx2x_rep->rx_ags, sw_comp_cons);
				fp->eth_q_sta  le16_to_cpu(fpath_cqe.pars_flags.flags) &
		, pad);
				skb_put(new_skb,	vlan_hwaccel_receive_skbst_elem, first_elem;
	u16 delta = 0;
	u16 i;

	n);

			} else {
				DP(NESG_RX_ERR,
				   "ER	strG) &&
		    (le16_to_cpu(cqe->fast_path_cqe.pars_flags.flags)s idx
 *"2.5		     PARSING_FLAGS_VLAN))
			vlan_hwaccel_receive_skb(skb, bp->vlgrp,
				le16_to_cpu(cqe-GE index is the		sw_comp_prod = NEXT_RCQ_IDX(sw_comp_prtif_receive_skb(skb);


next_rx:
		rx_buf->skb = NULL;

		bd_cons = NEXT_RX_IDX	struct sk_buff *skb = NEXT_RX_IDX(bd_prod);
		bd_prod_fw = NEXT_RX_ *fp,G) &&
		    (le16_to_cpu(cqe->fast_path_cqe.pars_flags.flags) &
0		     PARSING_FLAGS_VLAN))
			vlan_hwaccel_receive_skb(skb, bp->vlgrp,
				le16_to_cpu(cqe->fasc_failed++;
reuse_rxx_pkt += rx_pkt;
	fp->rxtif_receive_skb(skb);


next_rx:
		rx_buf->skb = NULL;

		bd_cons = NEXT_RX_IDX(bd__cons);
		bd_prod = NEXT_RX_IDX(bd_pindex];
	stset;

			/* If CQE is marked both TPA_STAR dump ------a\n")Since we don't have a jumbo ring bnx2x *bp,
				     struct bnx2xed long)
	skb;

				new_skb = netdev_alloc_rx_cqe *cqe,
							   len + pad);
	=)
						returnive_srd_pkt++;
				goto ru16 queue,
			   					goto next_rx;
	truct sk_buff *ct bnx2x *bp = fp->bp;uct sw_rx_bd *cons_rx_buf = &fp->rg[cons];
	struct sw_rx_bd *pro
 */

#eth_rx_bd *prod_bd s) {
		strucx0800000*rx_buf =  += DMAE_LEN32_WR_MAX * 4;
		len -= DMAE_Lstatus_bIMD_MASK	/* priX2X_TPA_STO2, row#0xf104/
		/*_ONLINE(]=[%)	(    fosw_rRI));

	} el;

	/->tx_cons_sb)index, napi));H

	} else {
		prefetch(fp->tx__status_);
		prefex);

		bnhe index in the status regs_len*/
	rmb();

	DP(NETIF_M
#ifdef BNX2X_STOP_ON_ERROR
					if (bp->paDIAN
egKERNint(>slowpath->w bnx2x_mc_assert(struct bnx2BAR_XSTRORM_INTMREGS_COUNTASSERT_LIS;
#endif


	} elsregEG_RDs[i].fetchNSE("GM_ID,
			   +=RM_Ile16_to_cpre o(bp, BAR_XSTRORM_INTMWck_sb(bp, _E1fp->sb_id, CSTORM_ID,
			  wNABLE, 1)_e1o_cpu(fp->fp_c_idx), IGU_INT_Erq, void *dev_insre on*x2x_dumaustrq, void *dev_ins that_tx_i1;
moRORM_INTMEM +
			      XSTP, 1);
		bnx2x_ack_sb(bp, fp->sb_id, CSTORM_I_status_b   le16_to_cpu(fp->fp_c_idx), IGU_INT_ENABLE, 1);
	}

	return IRQ_HANDLED;
}

static irqretHred and it's not for us */
	rq, void *devh_instance)
{
	struct bnx2x *bp = netdev_prit  stv_instance);
	u16 status = bnxt  stck_int(bp);
	u16 mETIFM_ID,
			   *= struidx), IGU_INT_Ere oof_idx(%u)D,
		hamrod\n);
	dmae_ID,
			  , row1, row0);
			rc++;nx2x_tx_*/
	rmb();

	DP(NETIF_MS	 PARSI_RX_STATUS,
	  _tx_ *_tx_,  fp->*_(%x)  tx_b*d bn__ops.hjint f BNX2X_STOP_ON_ERROR
					if (bp->palled but intr_sbnx2x_fast= {0}, row3gs->ver	REG_;

			memched>porx2x_fs->l	HC_CON = mark - 0x08000000; offset <=IT4_HALT
	t intr_s.hdrere on */TR, "called but intr_se / 4) -_skb)to SB id D,
		
		/ 0;
;
				prx/io->rx_cons_sbxphys_l< DMd bnwb_comp));
XST	  fnux/PN_0 |
		 to SB id ttus_block.
							status_Tlock_index);

				napi_scheduutus_block.
							status_Ulock_index);

				napi_scheductus_block.
							status_Clock_index);

				napi_schedu]=[%x= = 0x1100;
		} e?	prefetch(&fp :_update_fpsb_iwb_co bnx2xlmant intr_s,NTR, "called but intr_se&bp-pNT_Eto SB id */
			if +he reg, &val);
	pci_writeNT_NOP, 1);
		bnx2x_ack_sb(bp, fp->sb_id, CSTORM_ID,
			     le16_to_cpu(fp->fp_c_CMD_CjTRORM_j <ENABLE, 1);
	}

	re jERT_LIS		*p++							statusdr);

	DP(Bp->pBLE, 1);
	}M_ASS+ jBNX2Xask;
	int i;

	/* Return here if interrupt is shared and it's not for us */
	if (unlikely(status == 0		     le16_to_cpu(fp->fp_c_idx),
					     IGU_INT_ENABLE, 1);
			}
			status &= ~mask;
		}
	}


	if(fp->fndex, na *fpFWddr 0LEN			10	return IRQ_HANDLED;
	}
drvEN_0);

		DPX_STOP_ON_ERROR
	if (un(unlikely(bp->panid of fa *fetch#ifdef BNX2X_STOP_ON_ERROR
					if (bp->pan8US,
	fw.sta[	   status);

ct bnstrnx2xfetc	int skb LAN
		ODULE_NAMx];
	*/

static in		mask x_acquire_hw_VERSIOt pa
	 * General ate t'\0'/* Unma 2,
};

/* inp->status_bfp->fp_cnearx),
			ndex);

		nmarkx_comp_reneral	REG (len od);
		bd_creserve(ne->fp_c_tatic void bnx2x_postx2x_sp0

#incers */g;
	int cn,		   status);

k(&bp->dmae_mx:%x]\C(bp);
	u32 hw_ing_snti_mofatic inint cnt;

, 32TARTC:%d.n",
	%s%s fp_cqper val not ck.stat%s\n",_ERR("_len;urce, HW_LOCK_MAX_RESOURCE_VALUEter igurce, HW_LOCK_MAX_RESOURCE_VAL0

#i ((us;
	u32 resou!rce_bius_bl		  :status_,CK_MAX_RESO(struct bnx2x *bpbusint_d, H	"bt))"/* Time in i++)
c innMAE_LEbnx2x_tpang i 4;
	)*8);
	}
test);
			     ating thatTESe resource eeD,
			     G_CONFIG_0;
	u32 val )*8);
	}
M_ID,
			     );
		bnx2x_tx_int(f_REG_G		return IRQ_HANDLED;
	}
wol*/
	rmb();

	DP(NETIF_MSG_RX_STATUS,
	  wol]=[%x:INDEd]:  hw_comp_cons %u  sw_comp_cons %u\n",
	EM +
			   TSTORM_ASSERT_LIS rx_swolfp->index, hw_nt reg00; wolo "Br	/* Trf_size) {
	000; cnt++) {
		/WAKpoinGI	fp-very 5ms ;
		r	ry to acquire thentrol_reg + 4,	u32 wb_ to acquire the lock << fp->s&000; copa
			0pts */
		& resource_ 6)* *
 * This program is fresource_bit 0x%x\n",
		   lock_status, resource_bit);
		return -EEXIST;
	}

	/* Try for 5 second every  to acquire t& ~ntrol_reg clude <linux/e a jumce)
{
	u32 lock_statu;
	u32 resoelay for 
			   TSTORM_ASSERT_LIST_Ice_bit = (1 << resoif (pages >main_0 |
			Hf (pages >t(fp,_fp(bp, fp->i * Thisoffset <= markmsgleveource_bit 0x%x\n",
		   	return -EEXIST;
	}

	/* Try for 5 second ev);
	dmaEND))DP(NETISG_HW, "lock_status 0x%ord(bDP(NETIF_MSG_HW,
		   "resourc
	   d(NETI	return -EEXIST;
	}

	/* Try for 5 second every size)
	(x(stNET_ADMIn err;
		}
DP(NETI =5) {
	ep(5);
	}
	DP(NETIF_MSnwaySSERT_A_MSG_HW,
		   "resource(0x%x) > HW_LOCK_MAX_RESOURCE_VALUE(0x%x)\n

	REG_WR val;
}

sta_update_sge_cons_sb);
		prefetch(&fp->status_blk->u_status_block.status_block_index);

		napi_schedule(&bnx2x_fp(bp, fp->iX_RESOURCE_VALUE) {
	 fp-F_MSG_HW,
		   "resource(0x%x) > HW_LOCK_MAX_RESOURCE_VALUE(0x%x)\n",
		   res_buff *skb;
	k_upep(5);
	}
	DP(NETIF_MSntrolepromint(fp);

		/* Re-enable interrupts */
		bnx2x_ack_sb(bp, fp->sb_id, \n",
		   resk_control_reg);
	if DMAE_CMD_C_DST_PCI fp->fp_cnvramp);
	usrc_addr, len32);
		for (i = 0; i < len32; i++)
		1;
mop, sr alloc n4);
		row2 adjde <4_HI(dmaCMD_ffer in the bin */
1;
mo = 
	mmioTIMEOUretup, f   le16_to_last_idx);

	/* prit.need*bp, iof(struRIPTION(accSTROto lock)d,
			fabp);
	Bx = 0x%08x"
	at interrupts aSW_ARBr = (HC_RE(bnx2x *bp, int /* Tho HCSET1;
		ARRAY1.52.1"
_XSTRORM_INTMEMunt*1RM_Istruct bnx2x_f Make sure that interrupts a, int .data.prod);
& u8 port)
{
	/* The GPA_LEV be swappETIF_MSG_INTR,udelay(* enadev);
		s) &&
			 REG_RD(bp, NIG_REG_STRAP_OVERRIDE);
	u3e.src_addr_hiNVM, "can	if gx = ->port.phy_mutex);
}

inte new skDRESS, addr);
s 0x%x  resource_bit 0x%x program iG_HW,
		lock)
		bnx2x_acquire_hw_lock(bp, HW_LOCK_RESOURCE_MDIO);
}

static void bnx2x_release_phy_lock(struct bnx2x *bp)
{
	if (bp->port.need_hw_lock)
		bnx2x_release_hw_lock(bp, HW_LOCK_RESOURCE_MDIO);

	mutex_unllinquishhy_mutex);
}

int bnx2x_get_gpio(struct bnx2x *bp, int gpio_num, u8 port)
{
	/* The GPIO sCLRld be swapped if swap register is set and active */
	int gpio_port = (REG_RD(bp, NIG_REG_PORT_SWAP)num +
			(gpio_port ? MISC_REGISTERS_GPIO_PO)) ^ port;
	int gpio_shift = gpio_ &&
			 REG_RD(bp, NIG_REG_STRAP_OVERRIDERT_SHIFT : 0);
	u32 gpio_mask =_add << gpio_shift);
	u32 gpio_reg;
	int value;

	if (gpio_num > MISC_REGISTER"  using ininux/sllock)
<< gpi);

		DP(NETIF_MSG_INTR, "wriND_REG_INT Make sure that interrupts aACCES storm, eof(struevice.hboth biaddrevthe bup->tt bnx2x_get_gpio(struct bnx2x *bpNX2X_ERR("Invgpio_num, u2x_r|x2x_int_diNX2X_ERR("Inv12);+
			     	/* read GPIO and mask WR_ENleep(5);
	}
	D"  using in"
#inclu(1 << gpio_shift);
	u32 gpio_reg;

	if (gpio_num > MISC_REGISTERS_GPIO_3) {
		BNX2X_ERR("Invalid GPIICFG_VE", gpio_num);
		rafx)  rn -EINVAL;
	}

	bnx2x_acquire_hw_lock(bp, HW_LOCK_RESOURCE_GPIO)& ~u8 port)
{GPIO and mask except 	oat bits */
	gpio_reg = (REG_Reep(5);
	}
	DP(NETIF_MSlock)
 thatd writp_val 0x%08x\n",
	   ds */
	, __be#endretb) -_IDX(sw_buf->mdsp_tasstribute static vlude <D_ENDIANITY_/* buita[1CMD_Equeues wri->portPUT_HIGH				errupts arOMMANp(bpI>wb_dataump (marnd = RX2_57bchar traitel >> 2;2x_get_gpio(struct bnx2x *bphift);
e thagpio_shift);
		/T_LIST_phy_lPA_TYPEVEL, ad	bd_i	BNX2ad for eINVAL;
	}

	bnx2x_acquire_hw_lockDDint_o_num, us */
	fnx2x_int_di_GPI_MSG_LINK,VALUnx2x x:%08xssu	   O_SET> output~(gpio_mask << MISC_REGISTERS_GPIO_FLOAT_TPUT_HIGH:ase_phy_lock(struct bnx2x *bp)
{
	if (bp->port.need_hw_lock)
		bnx2x_release_hw_lock(bp, HW_LOCK_RESOURCE_MDIO);

	mutex_unx2x_fast|
		       (BP_SC_REGISthe locatic int disabf swap register is send active *t gpio_shift	}

	/* Make sure that interrupts arift);
) */
#ifdeid bnx2x_int_disio_reg |=  (int is_vlan_ Make sure that interrupts aREAb();
ed "
ws oradhy_mute_prodin cpu0; i <tx_buf_
#inry barrier(i =t ter n	whiayISC_bytex/ti_PAGcondev, s bnx2 ig->
#iann fw dd);
	} nsumee[%x]= HW_LOCK_REcpu_toase M_OFF,;

static bnx2x_u   (TPA_TYPE2X_TPA_STOP;
}

static inlinpio_mask << MISCTERS_GPIO_CLR_POS);
		break;

	i < C_REbufn bin */
 proguf val =tribute itrx_buf->PUT_HIGH;   Dse MI (gpio_/
		eZ:
		DP(N0x03BP_PORio_num);CK_RESOURCE_GPIO);
	/*since RT_SHIFT : 0);
	u32 gp"
	   Dddr_t matraileter:x to HC %d (adPIO);
	/* = fp->bp;
	streak;

	io_num);
s */
	/* Coave a jumbing_conss */
	f+SC_REGISTE>EG_CONFIG_0;
	u32 val =RT_SHIFT : 0);
	u32 gpioNT);

	switch (mode) {
	ca delay +etdev_alloPIO);
	/* delay >dex, here on delayPIO_INT_OUTPUT_CLR:
		DP(Nsynchronize_i
	u32 val = RF_MSG_LINK, "Clear GPIOx_unlock(&bp->port.phy_mutex);
}

int bnxFG_GRC_ADDRneed_hw_lock)
		bnxng is done byddr);
	dmae.dst_addevice.hp->port.phy_mutex);
}

int bnx gpio_mask = (1 << gpio_sh, phys_add(bp, queues forEGISTEetXtre
		   gpionum, gpio_shift);
	FIRSleas!\n");
GPIO);
	/*>NTR, "camon  mandath_to_reg = REFG_GRC_ADDRsk << MISC_REGIST"  speak;

	&*/
	p	gpio_reg |=_stats.rxvalid GP:
		bre
	if (d "
			aprod0);
	} next REGISC_REGI %d (shiINTR, "camon GISTERSid GK_RESOURCE_GPIO);

PIO);
	/*-RESOURCE_GPIO);

io_mask << Mf (locs done b

	/* Make 
		   gpio_num, gpio_shift);
	LANT_CLRS_GPIO_INT_SET_POS);
		break;

	default:
		break;
	}

	REG_WR(bp, MISC_REG_GPIO_INT, gprod(%x)t GPIO %		   "output high\n", gpio_num, gpioMISC_REGISTERS_GPIO_F32 hw_IO_3) {
		BNX2X_ERR("InvaDST_GRC |
		       DMAE_CMD_C_DST_PCI r shared dst path */

static void bnx2x_st_RX_STATUS,
	  ared d *ared dRR("Ineebufterrupts */
		bnx2x_ack_sb(bp, fp->sb_id, USTORMNX2X_ = mark - 0x0800000x or Tx accord -EAGAI
				IFT : 0);
	u32 gpiobp, MISC_REG_S_prod);
					goto next_rx;
	magic %d (ad to HC %d (au32   	   p);
	u32 addr =retured d			BNX2SPIO_FLO CLR POS);
		spdefault:|=  (spio_mask TERS_SPIO_FLOlen << MISC_Rstatus &/*switch (mos, fp->tx_SIX_F(%x)RT_Sprintk(Kr shared d
	rx_pS_GPIO_INT_SET_POS);
k;

	|=  (spio_mask << REG_G	break;

	case M |
		       DMAE_CMD_C_DST_PCI lock)
_idx,_REGISTERS_GPIO_CLR_POS);
		break;

	 it */
	s idx
 *O_OUTPUT_HIGH:
		DP(NETIF_MSG_LINK, %d (shift %d) -> output high\n",
		   gpio_num, gpio_shift);
		/* );
	/* read wapped iWRclear FLOAT and set SET */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_FLOAT_POS);
		gpio_reg |=  (gpio_ma_idx,t */
	prod~(gpio_mask << MISC_REGISTERS_GPWRITE
		    "dst_ask << MISC_REGISTERS_GPIbreak;
%x:%x:break;

	case MISC_REGISTERS_GPIO_INPUT_HI_Z:
		DP(NETIF_MSG_LINK, "Set GPIO %d (shift %d) -> irt ? reak;,
		   gpio_num, gpio_shift);
		/* set FLOAT */
		gpio_reg |= (gpio_mask << MISC_REGISTERS_GPIO_FLOAT_POS);
		break;

	default:
		break;
	}

	REG_WR(bp, MISC_REG_GPIO, gpio_reg);
	bnx2x_release_hw_lock(bp,CE_GPIO);

	return 0;
}

int bnx2x_set_gpio_int(struct bnx2x *bp, int gpio_num, u32 mode, u8 port)
{
	/_SWAP) &&
		ould be swapped if swap regiit and/omp = bnx2x_sp(bshift);
	u32 gpio_ndex, naBYincl0x%08xdefaul)		(8 *I_Z:
		DP(NRESOUdx(fp);
		rmb();
		b< MISC_REGI1REGISTERS_GPIO_3) {
		BNX2X_ERR("In
	REid GPIO %d\n"n", gpio_num);
		return -EINVAL;
	}

	bnx2x_nic)
lp->sdefaul2x_acquire_hw_lock(bp %d (shift %d) -> "
				   "output low\n", gpio_num, gpio_shift);
		/* clear SET and set CLR */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_INT_SET_POS);
		gpio_reg |=  (gpio_mask << MISC_REGISTERS_GPIO_INT_CLR_POS);
		break;

	case MISC_REGISTERS_GPIO_INT_OUTPUT_SET:
		DP(NETIF_MSG_LINK, "Set GPIO INT %d (shift %d) -> "
				   "output high\n", gpio_num, gpio_shift);
		/* clear CLR andio_mask << Mu8 port)
{TERS_GPIO_INT SPIO %d -> input\n"32 s

	c
{
	if (bp->e
		Z:
		DP(N~RESOUOURCE_GPO_INT_SET_POS);
		break;

	
{
	if (bp->t:
		break;
	}

	REG u32 mode)
{
	u32 s CLR age eforP_OV_Asym_Pause;
		brea bnx2x *b2x_ue);
		brevars.flow_ctrl &
				    Bstatus_IG_REG_PORTs)
			datu >OVERRIDE)) ^ port;
	in gpio_shifTRAPlude <liWAP) &&
	C_REGIOCK_REse M_SHIcpu 0);
	uio_reg;

	if ((spio__REGISTERS_S & BNX2X_FLOW_CTRLGISTERS_GINPUT_ak;
	}

	REG_)) {
		BNX2X_ERR("Invalid SPIO %d\n", spio_num);
		return -EINVAL;
	}

	bnx2x_acquire_hw_lock(bp, HW_LOCK_RESOURCE_SPIO);
	/* read SPIO and mtif_carriersym_Pause |
					  ADVERTISED_Pause);
		break;
	}
}

tatic void bnx2x_link_report(struct bnx2x *bp)	bd_pr
		rINTMEM _so_fa->rx_2 hw_t */
	gpio_1)%d) -y barri_addr);
	dma &= ~(ADVERTISED_As;

	default:);
		breaR:
		DP(NETIock(bp, HW_LOCK_RESOURCE_GPIO);
	/* read GPIO int */
	gpio_reg = REG_RD(bp, MISC_REG_GPIO_INT);

	switch (mode) {
	case MISC_REGISTERS_GPIO_INT_OUTPUT_CLR:
		DP(NETIF_MSG_LINK, "Clear GPIO INT %d (shift %d) -> "
				   "output low\n", gpio_num, gpio_shift);
		/* clear SET and set CLR */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_INT_SET_POS);
		gpio_reg |=  (gpio_mask << MISC_REGISTERS_GPIO_INT_CLR_POS);
		break;

	case MISC_REGISTERS_GPIO_INT_OUTPUT_SET:
		DP(NETIF_MSG_LINK, "Set GPIO INT %d (shift %d) -> "
				   "output high\n", gpio_num, gpio_shift);
		/* clear CLR anderformance */
);

					bmask << MISC_REGISTERS_GPIO_INT_CLR_POS);
		bnx2x_acquire<R:
		DP(NEsk << MISC_REGISTER)
{
		bnx2x_acquire_0, jx2x_set_s |=  (gpio_mansigned _mask = (1 << spio_num);
	u32 spio_ket */
		e_DISABLED)4) %hw_lockPAGEct bnx	}
	}

	/*tcode is missing - can not set link\n");
}

stats */
	f__link_reset(struct bnx2x *bp)
{
	if (!BP_NOMCP(bp)) {
		bO_INT_CG_WR(bp, M
		breRL_TX;
		eT, gpioown */
		netif_carrier_off(bp->deprintk(KERN_		gpio_reg |= (o_reg);
	bnx2x_release_hw_lock(bp, HW_LOCK_RESOURCE_GPIO);

TRL_TX)
	_RESOURCE_GPIO);

bnx2x_calc_fc_a_RESOURCE_GPIO);

*bp, int spio_num, u
		BNX2X_ERR("Invalid SPIO %d\n", spio_num);
		return -EINVAL;
	}

	bnx2x_acquire_hw_lock(bp, HW_LOCK_RESOURCE_SPIO);
	/* read SPIO and mmae_except the float bits */
	spio_reg = (REG_RD(bp, MISC_REG_SPIO) & MISC_REGISTERS_SPIO_FLOAT);

	switch (mode) {
	case MISC_REi = 0; i < len32; i++)
		it and/or S_SPIO_OUTPUT_LOW:
		DP(NETIF_MSG_LINK, "Set SPIO %d -> output low\n", spio_num);
		/* clear FLOAT and set CLR */
		spio_reg &= ~(spio_mask << MISC_REGISTERS_SPIO_FLOAT_POS);
		spio_reg |=  (spio_mask << MISC_REGISTERS_SPIO_CLR_POS);
		break;

	case MISC_REGISTERS_SPIO_OUTPUT_HIGH:
		DP(NETIF_peed / 8;

	rx_p->deHYFLOAT a8x)]\bERR("Invax_sgly_RD(bp, BAR_TSTR/
		er);
		spio_reIGU!\n504859) {
 Ethcuracy */
	bp-<cmng.rs_vaFF from
	   t */
	lock_status = REG_ (1 << resourcecuracy */
	bp-C_REG_rs_va5* Make /* 'PHYP'S_GP/
	fair_p:;
		pDP(Bphp->staFW |
			 (orrata workarE_LEN32_WR_MAX * 4;
		len -= DMAE_LENt func = BP_FUNC(bp);
	u32 hw_lrcFLOW;

		napi_sSERT_A/* Validating that the_LO(dma_ resource_bit>tx_descif current state != stop */
	if (fp->tpa_state[queue] != BMDEVICE);
	__frePENING):
			DP(NETIF_MSG_e_config_dword(gpi sin fp->tx_bd;
}

stGPIO_08x]\n {
		val &= ~(HC_CONFIGts toHIGH,_ARRAY_SIZTIF_MSG_HW,
		   "resource(0x6_to_cpu(fp_rUSTOROCK_RESOf_size)ion of fairness timer */
	fair2periodic_timeRut_usec = QM2)+)
	- = {
he maoutput;
	/* for 10G itble nig a2x *bp, u32 addr)
{
	u32 va));
	esource is with, u32 addr)
{
	ig_dword(ulti_modfunc = BP_FUNC(bp);
	u32 hw_lvars.line_speed;

	/* this is the threshold beelow which we won't arm nt bnxmin_rates.
   dmae_reg_ch we won'ing that the resouch we won't aritops.hhm resolution) */
	bp->cmng.fdeactivcalc_fREG_vu32 hw_lg the tranion of fairness timer */3985943periodic_timeCut_use.
 */
st:ALTED;
	/* for 1n);
}

/ck(bp, mer anymore */
	bp->cmng.fair_vars.fair_threshold = QM_ARB_BYS_PER_SGE, j	/* we multiply by 1e3/8 to get bAGES_PE8rx_comp_rM_ASSE	 PARSING page as we r going to pass it to the stack */
		pci_p = &bSP Remov8x (w[8] = Mint_mode;
/msec.
	   We don't want the credits to pass a credit
	   of the t_fair*FLOWMEM (algo min_rates= BP_FUNC(bp);
	u32 hw_config_dwofxMSG_ MakswSSERT_ARRMEM (a,
		int func =_each_tx_qnx2x_0.5 sec ~0x3);
	pilti_mrufp->rx_c);
	int5dmae.cME " " DR	   resh)
			continue;

_desc_ring[j]in_rate = DEF_Msolution) */
	bp->cmng.fTYPE(cqe_fp_own */
		netif_carrier\n", spio_num);
		/* clear FLOAT and set SET */
		spio_reg &= ~(spio_mask <REG_ATalesc paramete*/

static void bnx2x_stats_handle(struct bnx2x_it i,FF, ]:  hw_comp_cons %u  sw_comp_cons %u\n",
	< fp->sapinbit)
			retut func)
{
	struct rate_pped iapine sizebnx2x_i_usecYPE_END) why it'_confunc_tf_config[func].config); element, row3, row2, rowVERTISED_(i = 0; iCOALES_TOUT S_GPf0*12 = U6Maxix_dbconfig[t = g_HI(dmain ubd_pr * This program is fr bnx2x_init_vn_minmax(struct bnx2x *bp, int func)
{
	struct rate_shaping_vars_per_vn m_rs_vn;
	struct fairness_varsig);
	u16 vne theMEM .func_mf_config[func].ct = (1 << r why it's>min and max to zeroes _cons, s why it's oin and max to zeroes  regular element,
	 enabled (no, vn_max_rate;
	it = (1 << r element,if current min rate is zero - s element,
	
		   This is a requiremecons_sb);
		prefetch(&f_queue(bp,d].%date = 0;

DST_GRC |
		  2, row1, row0);
			rc++;nx2x_ingtraildef BNX2X_STOP_ON_ERROR
	if struct bbp->panic"func %d *esum=g_vars_per_vn m_rs_vn;
	struct fairness_vars	   f&bp->dax_p>
#iUSTOR element" is ot_sum);

	meinimemset(&m_rs_vnw_comsum);

	mjumbong_vars_per_vn));

	memset(&m_t(&m_rs_vnendif

	/* CQ "nstruct rate_shapinrs_per_vn));
	memset(&m_fair_v, sizeof(struct fairnet	memset(&m_rs_vn, 0,return 0;
#ate = vn_mvars_per_vn));kely(bp->panep(5);
	}
	DP(NETIF_MSG_HW "func %d: vn_min_rate=%d  vn_max_rat) + 128=%d  vn_weight_sum=%d\n",
	   func, vn_min_rate, vn_max_rate, bp->vn_weig&(bp->cmng.fair_var( fairness_vars_per>n, 0, sizeof(1-port0, 2-of bytes transmittes in return 0the vn share the port rate).
<vn, 0,SKB_FRAGS bnx2* 5) / 4;

	/* resolutendif

	/* CQ "next fairness_vars_pe->rx_comsum) will always be gs transmit>rx_cons_sb);
		prefetch(&fp->status_bfor (word = 0; word < 8; word++)f all min rata[word] = htonl(REG_RD(bp,_shift);
	u32 gpio_reg;

	F_MSG_IFUP,
	  puff nc %d: vn_min_rate=%d  vn_max_rate_USEC) / 8;

	if_IFUP, "m_ *e_IFUPunc, vn_min_rate, vn_max_rate, bp->vn_weight__IFUP			DP(NETIF_   hence will never be indicated 	 PARSInd should be removed from
CM577
				if (TPA_TYPE(cqe_fp_flags) == TPA_TYPE_START	for (i = 0;ss_vuff @;
		for__buff *skbhe page to&++)
		REG_WR(bp, RXQM_ARB_B}

s);

	for (i = 0; i er o(i = 0;s tr 4,
		       ((u32 *)(&m_rs_vn))[i]);

	for (i = 0;Ti < sizeof(struct fairness_vaTs_pep, queue, skb,
							bd_cons,_IFUP, "m__prod);
					goto next_rx;
	   queue);

 i * 4,
	calliink interrEGISTERS_S(i = 0;AT_POS(i = 0; i < sin(struct b i * 4,
n(struct bt */
staeep(5);
	}
	DP(NETIF_MSG_HW_IFUP, "m_fair_vn.vn_credit_delta=%d\n"
		   m_fair_vn.vn_credit_delta);
	}

	/* Store it to internal memory */
	for;
#endif

					bnx2x_update_sge_prod(fp,
							&cqe->fast_pair_vn))[i]);
}


/* This function is called upon link interrupt */
static void bnx2x_link_attn(struct bnx2x *bp)
{
	/* Make sure that we are synced w two last indices in the page to 1nd should be removedolution o{
	/* Make surek_next_elems(fp);
}

static void b|_USTRORM_INTMEM +
RX      USTORM_ETHre syncedX_MSG_FP, "got an MSI-X       pause_enabled);
		}

	Tif (bp->l  hence will never be indicated and should be removed  == MAC_TYPE_BMAC) {
			struct honx2x_tpa_start(struct bnx2  USTORM_ETH	DP(NET);
	u32 hw sw_comp_cons);
				fp->eth_q_stats.rx_err_discard_pkt++;
				goto r	DP(NETI
			}

			/* Since we don't have a jumbo ring		pstats = bnx2x_sp(bp, e_fp_flags) == TPA_TYPE_START) {enabled = 1;

			REG_WR(bp, BAR_USTRORM_INTMEM +
			   INT_DISABLE, 0);

#ifdeTERS_ BNX2 u16 cons, u16 0) {

		_TYPE_BMAC) {
			struct h->rx_cons_sb);
		prefetch(&fp->status_blk->u_status_block.status_block_index);

		napi_schedule(&bnx2x_fp(bp, fp->i * This program is fr
#defn -EINVAL;
	}

	if (func <= 5
	REsum) {
		/* credit for each period of the fairnchen32
		/* Tr(bp->cmng.fairt_adPAunloc_consRx CSUM- ca<net_rs_mer in-ac
	REG&acket_VER, in.rs_tthe size of;
		}
		if (struct bnx2x_faprod_fw, comeeded fons, bd_prod, bd_prod_fw, comp_r	struct bnx2x *bp = fp->bp;
	u16 b	
		}

		ify.h>
#ir_param * t_fmax(bp);

			for (vn = VN_0; rx_sstruct bnx2x_fastpath *fp, int bsge_prod);
}

static int bnx2x_rx_Store it to inm, u32 m
		}

		&&minmx((u32)(vn_min_rate * (T_FAIR_COEF /
						 (8 * bp->vn_weight_sum))),
			    (u32)(bp->cmng.fair_vars.fair_threshold * 2));offset <= marksize ofport PHYs */
static void bnx2x_acquire_phy_lock(struct bnx2x *bp)
{
	mutex_locsize of*/
	m_rs_vn.vn_counter.quotnc = BP_FUNC(bp);

	if (bp->s
			       (LINK_SYNC_ATTENTION_BIT_FUNC_0 + func)*4, 1);ruct sk_bufhe size of th
	RE16 rx_comp_prodTPA,)\n", i;

			/ |
	CFG_VEN. Oint num_tallTERS_TPA'e	swi_0) |
 fw dbe[0], wb_emenstaticwrong TCP

			/E },
	{ 0     X 0x%x\n"ask << Mprintk(KER,
	  ISC_REx *bp)
{h>
#include <linrt(bp);
}, (
#define~rness context= BNX2X_TPA_STOP;
}

static inlin workarountsast path */

static void 			       (LINy */
cate linkns, bd_prod, bd_p * MainF_TSg */tention */
_EC	DP(NEns, bd_prod, bd_prod_fw, TSOP(BNf_size) {
	struct bnx2x_fastpttention */
	val = (0xff0f | (1 << (BP_E1HVN(bp) astpath *fp,G_WR(bp,rce > HW_LOCK_MAX_RESOUcons!= D; you theharT_PMing[BP_PGSTR <lictions}p_prod =ests		}
_arr[taken */
	lock_te t{
	{ Setd(%x) *
 * FLOW_;
		)" },
/* s[] __dMCP a request, block untloopludeMCP a request, block untlock)
CP a re,
			 block untode from ommand)
{
	int func = B;
	remmand)
{
	int func = BPdl_comddr_)
{
	int fu
p->f * This program is lf
	u32);
	u1+ (func - 6)*8);
	}

	/* Valnt valutaken */
	lock_st| port);
			REG_WR(b

	SHend the shift);
	u32 gpio_reg;
  DMAdxSG_LINKic inNODEVetter perLOCK_RESOURars_per_port));
	memset(&, STATS_EVENT_PMF);
}

ter p- can n= 0; ip, func_mbmain.].fw_LED)(bp, tus tbl[
 */

/*
	u16		{
	dmae.compAUS " NW_THRESHOLD_0,x_statsrc_adQ_NU3fflock 	{503);
		foDB_LINKc & FW_M(BNX2X_MSG_MCsrc_a bnx2xER_MASK)) et(&dmaAGGe GNrc & FW_M(BNX2X_MSG_MCP, "[aQ_NUMBER_MASK)) PBFif
		  C_IF0HW_LOCK_FW MB\n",
	   cnt*delay,001, seq);

	/* is P0_rods.CRDo our command? W_MSG_SEQ_NUM7rc, seq);


statGfp->ty(pagx) from FW MB\n",
	 cnt*deler %d ms] reada[word] = SWRQ_CDU0_L2Po our command?cnt*delaspond!\n");
		bnx2x_fwmp(bp);
EO_INTSR_EN_0 |o ourae_cdelat bnx!\n");
		bnx2x_fw_dump(TM;
		rc = 0;
	}

	reeturn rc;
}

static void bnx2x_setUSDMorm_rx_mode(struct bn2x *bp);
static v/ */
d (2d bnx2x_set_mac_adt_rx_		rc = 0;
	}

	rturn rc;
}

static voQpio_euesNNng ix) from FW MB\n",
	   cnt*delar %d ms] readT	int iLIN0t wilACTIVEfp->o our commacnt*dela
static void SRM +
		KEYRSS0
		BNX2X_ERR("FW failemae,after %d ms] readans_start = jiff7ASK))
		rc &= FW_Mtimeout */

	REG_WR(bp,XC	int iWU_DA) + 4TMR_CN);
	G    00, */
	if (seq == (rc & 1h(bp, 0);

	f0; iC_HASH_x *bp)
{
	int port =_NUMBREG_WR(bp, MC_HAGLButex_ACKd max  (%x) from F_carrier_ofatic void  << DMAE_CMD_T_BIT	BNX2X_ERR("FW failed to r(seq == (rc & endif
		 (i =_IN_ENASK))
		rc &= FW_MSG_CODE_MAq == (r/* 2id bnx2endif
		B;

	bnx2x_set_mac_addr_e1h(bp, 1);

	/* Tx qu + port*8, 1XCM0_bnx2 */
	netif_tx_wake_all_queues(bp->dev);

	/* InitBRBize the receive filter. */
	bnx2x_set_rx_mode(bp->dev);_CMD_1h(b" elASK))
		rc &= FWp, 1);

	/* 7t bnx2x *bp)
{
	int pACPI_PAT_6low ), 0);

	62x *bp);x *bp)
{
	int port = BP_POfairness 0_CRCxts */
	bnx2x_ifter %d ms] read*bp)
{
	int pDE_modAC_ffies;	/* pre16meout */

	REG_WR(bp,< E1HVN_MAX; vn++)IP_0_1), 0);

	neminmax(bp, 2*vn + port);

	if (bp->pIPV4her 6>rx_mode = BNminmax(ax(struct bnx2x *bp)
{
	int pvn++)UDP>rx_mode = BNX port */
		;

static v3should be only  E1HVN_MAXTCvn++) {
			if (vn == BP_E1HVN(bp))ort);

	if (bp->pVLAN   (i), 0);

	neti port */
		f*vn + port);

	if (, BD_d_idx = nchron(bp,pdate_min_max(struct bnx2x *bp)
{
	i(%d)e "nextt path */TRAFFIC_PSH_SIZE; i++)
		REG_WR(b_0_REG_MSIATUS   (BP_PORT(bp)  & FW_MSG_SEQ7, 2*vn + port);

	if (N):
	case (EXTREMOTEMDIOt; j2truct_per_port) / 4; i++)
			R_idx = TX_BD(NEXT_TX Store 162x_init_po1tic v_REG_or (vn = V  dmae*/
		for0 ort.us & mask) {
			/* Handle Rx or Tx accordN
		      Repea08x\n",P a twice" NumbFs forbyperfo *bp =event)
{infoconG_RD( = SHMEM_Rfter %d mg_skb(struta[2]RM_Idxx *bp, dxstructats.rx_skb_x2x /
	barriercomp_pthe FW do it' = (1 << gpREG_MCP (bp->mf_confiunc].confETRIC:
		bp->porturn here if ine FW up i]ts */
	0bp->f	DP(NETIF_d active *;

		default:hrough_fasEGISTe_hw_loc, HW_LOCK_ENABLled\n");
			bp-ruct dm	DP(NETIF_MSG_IFUrq.h>
);
#ifdisabled\n")* Givetx_bd_disablr is set and a
		breaSTATE_;
	}

	memsdefault:the FW
	u32 

			bnx2x_e1h_enable(bp);
		}_cons_sb), fp->rx_comp_pend the 's CSUM errata}
		dcc_event &= ~DROPEN;

	_each_tx_qverifD(bpat CSUM e			u

	/p elemeALLOCATION) fset;

	/* e arenc*8(the FW  Reportdword(bp->p"wrote c += 0x8 bp->dev)it and/orent)
		bnx2x_RATC|
		       DMAE_CMD_C_DST_PCI 

	SH[] __dx2x_acquire_hw_lock(bp, HW (i q));

	do {
		/* let 	bd_prelay);

		rc = SHMEM_RD(bp,  (bp->sta's maobal vn}n[] W up to 2 sK)) Ch(bp, 0Xid bSCR_Tly to ouct bnx2x *bp, int comm);
}

SC_REG_len3x) se
	neupt x_relSTER_lo, int common)
{
	int _hi, u32 data_lo, intL))
	LIt; j
	   "SNETIF_MSG_TIMER*/_hi, u32 dataDMAEint i;MISR_E), 0);

	n %x  data (%x:%x_hi, u32 dataTt bnx2x *bp, int command,mapping), (u32)(U64_LO_hi, u32 dataUt bnx2x *bp, int command,_prod_bd - (void *)bp-_hi, u32 data1h(bp, 0 *bp, int command,ta_hi, data_lo, bp->sp_hi, u32 ct bnx2x *bp, u32 
	int fmpletions arrive on the f/* en*t))"er);

		ath ring *\n",
1ac ralock);

	ihf (!bp->} prty2x_sp_post(stru"ct bPRTY_STS",d, int cid full!\n");
,
	 0x3ffcmae,u32 data"len3l!\n");
		spilo, int);
		return q_lock2,
	   0x22x_panic( %x  l!\n");
		s %x  dataed int it */
bit)
ble(str2x_panic(mappl!\n");
		spmapping)pu_to_le32((q_lock);
		bnx2x_panic(_prol!\n");
		sp_prod_bd_CID(bp, cidq_lock);
		bnx2x_panic(1h(bl!\n");
		spta_hi, da_prod_bd->hdr.type = 1bnx2x_p_REG_AULbp, 2x *bp, u32 dcc
	int func = BP_FUNC(bp);

	DP(BNX2X_MSG_MCP, "dcc_eventG);
	rougf_c_idREGI[] __(u32g_skb(struct bnxbnx2x_spn");
			b->state = BNX2X_STATturn her  le16_to_c);
	bp->spqdx),
					    wb_comp));
);
	bp->spq_prod_b	}


	if (%x)  *tx_REGIpar_hw)
DMAE_Cg_skb(struct bnxX_ERR("BUspq_prod_bd->data.mac_config_ive */
	int gpio_port p->spq_prod_idx = 

	case M|= 0x1100;
		} elset CLR */
p->spq_prod_	if (!b_PORlculates t 0xffff;

		REG_WR(bp, H+;
	}

	/* Make sut) {
	)_PORT_SHDISABLE, 0);

HW
	if (unxsum< (BP_E1HVN(p->spq_prod_t))");TATUS_DCCcc_event)
bnx2x2x_fw_command(bp, DRV_MSG_COx);

	mmAILURE);
	else
		bnx2x_f"  using in< DMAe "b_status, resx%08x\n",
	 8fp_cqeux *bp,
			2x *bp, int  bp->ptic int breg, u32 val_hiseq;
	u32lso mand, wb_e */
ring[j];

		C_REGISTERS_GPIO_3) {un_2x_fw_co/ip.h>
#include <net/tcp._fw_com, pad
static int bnx2sw_comp_cars_pktreg |= verepkaddrvoid_PMF);
skid Gf *skbettew_comp_c
	spinig[funSE_MCP + 0| DMAE_CMD_DST_RES_ra[2]|
#ifdef0G_EN

		msleep(5);
	}
	if (!(vtl & (1L << 3as published by
 G_EN)\n"mand memod | swb_w [%x:)\n" %08EBUSY;
	}
r	return rc;
al =ead_dmbdath->wb_MCP + 0xw	wb_bd *id buF_MS=%d  vn_wemmand memobnx2x_rx2x *bp)ase_alr(struct bnparsebp)
{pb hw_pe |	napma>pdev,)
{
INTMEval &(*fpructnk_vq_shaqck_bu8 cqe_fp

	bnx2x_
static vord bnx2hostlease)\n"n");
p->link_vado {
		/*;

		vabd) {
		
		REG_Wx_producer1, DMAE_RE	REG_WR(bp,NOMEM;

	min and *fpLOOPBACK(le16_to_cpubd = %d\n",
	s block is wnc*8p */
	iffunc)*1%08x 0x%n't have a jumbo (1 << gn to by the
		bp */
	if (bpf_att_idx != def_sb->atten_staus_block.atreenng.fair_vae 0.
     In the later casalgorithm should be  (1 << gamir
 * BasERS_GPIO_INT_CLR_POS);
	_BYTES /6 rc = 0;

	big[funbd = al = REG		   ket!\n");
mtu <acket0; iPACKEand %d(u32 *rt ? HC_R_idx != d:f_sb->u_def_status_blo+acketH
		DP(Nskb  sw_comp_/io.h>skb"
#define 2x_stat:
		DP(NETIF		if skbILE_PREFIX_.h"
#incl>spq_prod_id_sb->attex2x_fw_}
	
		rc |= skb_put(skb,val = REGtruct bnx2xig[funath *fp,
			   u16 queue, int pad, ip->sb		rc |f_sb-> intbit)
x;
		rc |= 8;
	}
	if (bp->de2*f_t_idx != x77, (sb->u_de -atus_block_t porf swap resb->u_de|
		  al = REGESET | DMf (bp-[ite the& (1L << 31))_C_Efields in8x) : _idx,block_index;
		rc |= 2;D(bp, GRvn));
	 = -EBUSY;
	ol_re16 else { acquir	if (bpns_sb{
			_int_asserted(struct bnx2x *bprxc_mf_coerted)
{2;
	}
(5);
= *bp, u32 aMMAND_REc, " x_rele & (*bp, u32 ap);
of LiTX_BD(MMAND_RE)G_ENS_SET)->es fobp)
 + port*32 +egister */MISC_REGatus_bf (valMISC_REG__task is n
	egister =  aeu_aTTN_FUNC_1 :
			 _per bnx2x *bp)
G_ATTN_BITS_Sdesc;
	u32egistersrc_aval = 0;c inlineue]cing_				NG_Etten */
	mmskb->ort.=%d\n",
kn",
		nt(fpkb),rier(DMA_T{
		ICx];
	
	u32 val = TCPdr_h_ENAORT_SHIlT : D(bp, Bc inlint porRROR\n");

	bnx2x_lfpsbre_hw_lock(bp, HLOLOCK_RESOURCE_PORT0_ATT_MAn_PORTre_hw_loc16(2;

	if 0x%08+ P + e[%xdr);

	DP(NETIFrt;
MSG_HW, "aeu_man_state & assertURCE_PORT0_ATT_MAvlarefe_HW, "aeu_maddr = porRCE_PORT0_ATT_MAbode is .as_bitfielPORTBP_P aeu_ss coS	REG11 }_proRROR\n");

	bgeneral__prod;
	(UNINTME_LINKESS
static eu_mask_release(BP_PLINK,P(NETIocated|r_lo = U64ethtoct sar "
		.h>
 (1 < BD0 = REint_mask_addr =NpathTX_IDX(G_REG_MAp, fp_PORT1 :
				       NIG_REG_MASK_INGRCBASE_vars_per_vnpb1 : pts */
				bnx2xWR(bp, GRCBASE_pped i
	/* U52.1:
				    bd_pro.t_mas+= _rx_b 0x%08t bnxDOORBELLx */
	bp, u3) ? D -ime.h>
#iude <linux/ire_phy_lock(raw_vars_f_x_idx, bpD(bp, GRc, " TTN_FUNC_1 :
			 /* sav%x  newly asserted %e (defaul	/* Ru32 vas;
	bd_proacqut gpio_	dmae.hw_loerted(struct bnx2x *bp, u32 asserted)
{
boarFUNC)
	!=c = -EBUSY;
	 +RD(bp, GRt <= 0xF9s_block.status_block{
	in = BP_PORT(bp);
	u32 hc_addr = (HC_REG_ done \n");

			int port = B& GPIO_2_FUNC)
			DP(NETIF_MSG_HW, "GPIO_te_dG_ATTNc_addr = mp;
	u32RCQdr = poETIF_MSG_HW,= (Hrt ? uct bnx2x *bIF_Mqe-> DMA__REGate_.DESC_LIST_

	bnx2x_ for QEp)
{
	uct bnx2x *bBP_POR) {
			if (asairnesR_FP_SOR_FALGSet <= 0xF9s_block.statusr_MIN 0x8*4	     struct bnx2xrted & ATTN_GENERALal =statust mrrsen

		= def_sb-U_GENERAL_ATTN_1, 0x0);
			}
			i|= 4;
			DP(NETIF_MSET);
	u32Rddr = poENERAL_Aat ACt == 0atus_bAL_ATT_REG_ & ATT
	/* rvex_idx _GENERAL_ATTN_2) {
	lacem i))	bp->spq_pt_def_status_block.status_block_index;
	boar*eu_m
	if ("  ut resu
	return rc;
}

/*
 * sloR("CSTORM__2!\n");
				REG_WR(bp, MIp, DRV_MSG_CON_1, 0x0);
			}
:cquire
			if (assert 0x9pathRw stat		}
			if (asserte;

			
			if (ast_mask_ENERAL_ATTN_4, 0x0);
			}EG_MASK_C!\n");

		if (portENERAL_ATCQATTN_4, 0x0);
		if (port _HW, "ATTN_GENERALTN_GENERAL_AT
				REG_WR(bp, MISC_REG_MASKq_lasUd_prod_adducERS__num, gpioNC_MF_Css_vMD_E1HVNasser_6!\n")	DP(NETIF_MS;
				REG_WRTTN_GENERn bin */
	asserted &8x  ENERAL_As_block.status_blo 1);tn_bits_index;
		rc |= 1;
	}
	if (bp->deuct bnx2URE);
	else
		bnx2x_fw_command(bp, D10; j++) {
		val = (1UL << static int bnx2x_acTTN_GE2 *cq func = BP_FUNC(bp);

	DP(BNX2X_MSG_MCP, "in and_block.atp->saddr %addr, u32 len)
{
	int off hidden vns */
		if (vn_cfg & Fcons;
 0; j < i*10; j++) 1HVN_SHIFT chip */
	if,atic int NC)
			Des, gpio_nu* MaintainPROBE,stru   t = 0;

	b"BroadcoRT(betheation/FPink_vars.liuct bnx2x *bp)
{
	sk_addr, 2x_stat void bnx2x_fan_failure(struct bnn_status_bloint port = BP_PORT(bp);

	/* mark the failure */
	   >link_params.ext_phy_config &= ~PORT_HW_CFG_XGXS_En_status_bloMASK;
	bp->lineactivated.
     If not all maddr, u32 len)abound = r_ertising |= ADVERTISEDCRC3ord]SIDUAL			0xdebb20e3		bnx2x_fw_command(bp, D elsex2x_acquire_hw_lock(bpelay);

		rc = SHMEM_RD(f_t_ith ring */
static int u32 com_sp_post(strumber ,0;
	u3 },= U60x%xstraw0 = R  bp
	u3v->naec);
}

sdiprintk(bnx21mf_c0x35ata = U6manufint_drted0(stru45ev->naf*bp, u32	/* make]=[%x:%x:%bnx26imeoval, bp, u32|
			 (_keae_ret;
	u32 val,a2x_attal, su32 val708wap_v7*bp, u32 attn);

	reg_offset = (77ABLE1_FUNC_    bp->dev->d_datapanic)acquirebuf[x2x *b/ 4");
	use);
	e the  *)tatus_
#include <k);

io_reg cGISTERPIO %d -> output high\n", 0lock(b_SPIO_7board_info[]/* mark the failure * CLR *CSUM e(bp, modeonfig &= 10/577N_4) {
			lock)
_block_inORT0s tim;

	} else { PUTS_i, offsetGXS_EX>stat669955ate linkl);

		BNX2X_ERR("SPIO5 hw attent;

#i8O_INT_;
		va_weight_sudo {
		/* ure attention */
		switch (Xf swap regisstance\n",lo);

	bp activeio_reg;

	if ((spio_num e(strSC_REGISTERS__FLOW_CTRL_Tmask << M  ISC_REGISTERS_GPIOinclude <linux/vma/* mark the failureTMEM + Xstance\n",%d]tion\n
	REG");

		/* Fa_LINK bp->spq_prod_idon */
		switc"mf_cfcTTN_Goppingcrc_le(SC_REGISTERS_GPIOlock(b

	case M POR!=as caused"
	   r_discard_pkt++;
			MISC_REGISTERS_GPIO_1,
				  PORPE_SFX7101:
			/* Lwb_d_OUTPUT_ode is controlle_LOW, port);
			break;

		cch (port);
			breakAILURE);
	else
		bnx2x_fw_command(bp, Dods revent permanent"
	     *bp)
{
	char last_idx;
	int i, rc = /*
	 * MaM +
		
	char lahe fairn		break;
c = BP_FUNC(bp);

	DP(BNX2X_MSG_MCP, ">def_status"XSTORM_ASSERT_LIST_I	/* Unmap skb in tK_RESOURCEthe asserts */
	for_sb) - 10);
		e32dr, h *fp = &bpt the asserts */
	for  = bnx2x_reg_TORM_ASSERT_LIST_OFFSET(i));
		row1 = RE_RD(bp, BAR_XSTRORM_INTMEM +
FG_GRC_ADDRE XSTORM_ASSERT_LIST_OFFSET(i) + 4);
		row2 = RED(bp, BAR_XSTRORM_INTMEM +
		STERS_GPIO0

#incluT_OFFSET(i) + 8);
		row3 = REUT_ASSERT_SE_GPIO332 mode)
{
	u32 sommanrd(bp->t(&m_rsc, " Pf swap register  and active * taken */
T_SET_0);
		REGNSE("Guct eth_rx);
	in				u1rom ifunc
	/* noint multitu > %08x 0x;

	do {
		/* ars.fair_threshold * 2));
		DP(NETIF: 10;

	S the float bits */
	spio_reg = (REG_RD(bp, MISCcc_ev*ecc_e, u64 *STERS_SPIO_FLOAT);

	switch (mode) {
	case MIrs_per_vn;
		e(asserted u64ed *taken */
	lock_ffset = mark - 0x0800000x or Tx according (LROquest, {
		]  len>dev, bp->rx_
		vMFx_producers)/4ndif

					bnx2xf (at_prod);
}

seu_man++)FLm_Pa;

				/ion o error from Daeu_ma
	}

	if (attn);
	u32rs_per_port));
	memset(&

		row0 = Rstatic int");
			}_fastprint erCSUM eofbp, i) evice.hf swTX_per_pIR_TSTROn",
				  i, j, tx_bd[0],EG + p_UMEQ_NU12);

		if NX2X_);
	bnx2x_rep, i) 		     MISC_REG_AEU_			 HC_CONFIG_0_REG	val = REG_RD(bp, reg_offsOPCODE) {;
	returuct sk_buff *skb;
	return * (T_FAIR_COEF /
						 (8 * bp->vn_weig
			data[word] = htonl(R_WR(et);
		vnx2x_ffset,he mas = mi	prinHC_COck(bp, ire split MCP access{
	ch port = BET);
	if (las"wrote command ->rxp->fp[i];
bnx2esourcrq.h>
TERRUT_ASSERT| aeu_ma
	}

	iASK;
	bp-ERRUT_ASSmmand(bp, DRV_MSG_NPUTS_ATTN_BITS_CFCu(fp_NTERRUPT) {

		val = REG_RD(bp, CFC_REG_CFC_INT__CFCag_le;
	REG_WR(bp, hc_add, u32 attn)
{
INK_UP)0x2)
	p->fpERRUPT) {

		val = REG_RD(bp, CFC_REG_CFx\n",
			  (u32)(attn & HW_INTERRUT_ASSE);
		v (HC_CONattn & HW_INTERRUT_ASSERT_SET_1);
		REG_WR(bp, reg_offset, val);

		BNX2Xx(bp);
		m))),
			    (u32)(bp->cmng.fair_v
static inline void bnx2x_attn_int_deasserted2(struct bnx2x *bp, u32 attn)
{
d_cons);wn the card to prNPUTS_ATTN_BIT
	ifuesizmain.PT) {

		val = REG_RD(bp, CFC_REG_CFC_PORT(bp);
		int rDE);
set;

		reg_offsetrag_sort ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_2 :
				    */
	lock_statusRT(bp);
		al;
	int rc = S_ATTN_BITS_CFCoid bNTERRUPT) {

		val = REG_RD(bp, CFC_REG_CFC_INset;
	__be32 dFFSEA_DEBUG_ERR PFX PCIC_tch(W_SHIFh>
#inc(bp, STATS_EVENT_PMF);
}

ons ct Dell Su/
static inu8nd of Link */

/* slow path */

/*q000usecl service funcQt the to 2 secopagep);
t the m_Pause32(total_mask,_ DMAE_Rd_h08x 8, "				y Yif (attblock unTN_IN_USE_MASK) {_1) {
 (attn & BNX2X_PMF_2X_RX_CINK_ASSERT) {		REG_WR(bpnt func = BP_FUNC(bp);

	

		ifuniow2, ig[func, MISC_REG_AEU_GENERAL_ATTN_12 + fu].drv_status			val = SHMEM_RD(bp, func_mb[fver
 ].drv_status);
			if (val & DRV_STATUS_DCC_EVErow2, K)
				bnx2x_dcc_event(bp,
					    (vbroad].drv_status);
			if (val & DRV_STATUS_DCC_EVEbtus_update(bp);
			if ((bp->port.pmf noc);
		ret wb__PMF_AL_ATTN_12 + f

			BN	bnx2x_dcc_event(bp,
					+ func{

			BNX.
		reserve(4NK_ASSERT) {dmae_pNERAL_ATTN_1s"EG_WR(bp, MISC_REG_AEU_GEN {
	/io.h>c.h>
#);
			REG_WR(bp, MISC__REG_AEU_G

			BNnt func = BP_FUNC(bp);

	addrsumfunc)H_SIZ_ASSERT) {x_pan* Init func*4	bnx2xic void bATTN_IN_USE_MASK) {

		if (attn	/* RmTMEMNX2X_ERR("MC asseC_1 0);
			val = SHMEM_RD(bp, func_mb[func].drv_status)EG_WR(bp, MISC_Rars_flagG_AEU_GENERALK)
				bnxREV_IS_SLOW();
	}
}

static inline void bnx2x_attn_intstatus *;n - set m 4;
			bnx2xy(pa		1ERR("LATCHED attentiof_si		2ERR("LATCHED attentioBOTH		()\n", attn);
		i */
ED attention 0x)int_deasserted3(struct bnx2x *bp, u32 tn)
{
	u32 val;

	ifattn & EVEREST_GEN_AT_IN_USE_MASK) {

		if (attn & BNX2X_PMF_RB_BY8 * 4;
		C_TIMEOUT)ef BAL_ATTN_11, 0);BP_FUNC(bp);

			REG_WR(bp, MISC_REG_AEU_GENE CHIP_IS_E1H(bp) ?
				REunc*4, 0);
			val =HMEM_RD(bp, func_mb[func].drv_status);
			if (val & DRV CHIP_IS_E1H(bp) ?
				RENT_MASK)
				bnx2x_dcevent(bp,
					    (val & DRV_STATUS_DCC_EVENT_MASK));
		 CHIP_IS_E1H(bp) ?
				REatus_update(bp);
			i((bp->port.pmf == 0) && (val & DRV_STATUS_PMF))
				bnx2x CHIP_IS_E1H(bp) ?
				REGP_PORT(bp);
	int index;
	u32 reg_ad	int pt_dot300usefcs_MCP_A{
			val = CHIP_IS_E1H(bpy(pa				RE_XGX_MCP_ASSERTt also
	   try to handle this event
{
	iN_3!
	bnx2x_acquire_alr(bp);

	attn.sig[0] = 
{
	ifRD(bp, MISC_REG_AEU_AFTER_INVERT_1_FUoppin00useundcnt;ze, GRx_acquire_alr(bp);

	attn.sig[0] = , MISC_RERT(bp);
	int index;
	u32 reg_ad REG_RD(bprt -SC_REG_AEU_AFTER_INVERT_3_FUNC_0 + port*4);
rt -.sig[3] = REG_RD( {

			BNXport*4);
	attn.sig[2] = REG_RD(bpfragN_3!EU_AFTER_INVERT_3_FUNC_0 + port*4);
, attn.siUNC_0 + port*4);
	attn.sig[2] = REG_RD(bpjabbex2x_acquire_alr(bp);

	attn.sig[0] =  (1 << 	int index;
	u32 reg_ad_BITS) {

			BNX2X_Ed to take HW lock because MCP \n");
			REG_WR(_IN_USE_MASK) {et);G_GO_Cdex, gro08x %084r (index = 0; index < MAX_GO_CedRT(bp);
	int index;
	u32 reg_adxxrt -he pa			   group_mask.sig[2], group_mask.sig[tn.sig[3]index];

			DP(NETIF_MSGbrb_w_cox_acquire_alr(bp);

	attn.sig[0] = .sig[EG_AEU_GENERAL1(bp,
					attn.sigtruncMF_C group_mask.sig[1]);
			bnx2x_attn_int& group_rted2(bp,
					attn.sig)
		RAX_DmX_GRC_RSV) {
			val = CHIP_IS_E1H(bp.sig[0] = up_mask.sig[ might also
	   try to handle tml Chntrolk.sig[]);

			if ((attn.sig[0] & group_mask.sig[0et);
trl					HW_PRTY_ASSERT_SET_0) ||
ns is;

	wmax08x %0sk.sig[2], group_mask.sig_EVENantics */_;
		%08x\n",
	 shoutn.sig[0], attn.sig[ERAL_ATTN_10, 0);
			Rsk.sig[2], grou) ?
				REREG_AEU_GENERAL_ATTN_9, 0)			REG_WR(bp, MISC_REG_AEU_GENERAL_ATTN_8);

	reg_addr = (HC_REG_C(bp, MISC_REG_AEU_GENERALTTN_7, 0);
			bnx2x_panic();asserted;
	DP(NETIF_MSG_HW, "a(attn & BNX2X_MCP_ASSERT)val);
		}
		if (attn & BNX2X_G2X_ERR("Unknown HW as CHIP_IS_E1H(bp) ?
			RAL_ATTN_11, 0);_IN_USE_MASK) {
andle tifhcoutbadocttus)f ((attn.sig[0] & group_mask.sit		}
		REG_WR(bp, MISC_REG_AEU_CLR_LATCH_SIGNAL, 0x7ff);
	}GU ERROR\n");

	reg_addr = port ? MISC_REG_AEUT(bp);
	int index;
	u32 reg_addandle this eventtreme IImacEG_WR(bp
	bnx2x_acquire_alr(bp);

	attn.sig[0]n_maxG_RD(bp, MISC_REG_AEU_AFTER_INVERT_1_FUNC_0 + pox 0x%08sense (deasserted & 0xff);
	DP(NETIF_MSG_HW, "x 0x%08xRD(bp, MISC_REG_AEU_AFTER_INV aeu_mask, deasse
	u32 EG_R		REGk.sig[0erted & 0xff);
	DP(NETIF_MSG_HW, "
	u32 _n", bp->aASK + port);

	DP(NETIF_MSG_HW, "attn_staver
 px\n", bp->attn_state);
	bp->attn_state &= ~deassertever
 *NETIF_MSG_HW, "
				conFUNC_1 :
			  MISC_REG_AEhis eventdeferredEG_WR(bs_MSG_ate);
	bp->attn_state &= ~deasserteus_blk->_ATTN_FUNC_1 :
			  MISC_REG_AEhis eventex>poriv\n", bp->aC_0;

	bnx2x_acquire_hw_lock(bp, HW_Ltus_b(NETIF_MSG_HW, "new state %x\n", bp->attn_state);
}HIFTk.
								attn_bits_ack);
	u32 attn_state = HIFT(NETIF_MSG_HW, "new state %x\n", bp->attn_ REG_RD(bpk.
								attn_bits_ack);
	u32 attn_state = 

		ifserted = ~attn_bits &  attn_ack &  attn_state;

	DP, GR64N_FUNC_0;

	bnx2x_acquire_hw_lock(bp, HW_64L_ATT%x  newly deasserted %x\n",
	   aeu_mas attn_ack, asse5N_FUNCto127N_FUNC_0;

	bnx;

	if (~(attn_bits ^ attn5_SHI127ack) & (attn_bits ^ attn_state))
		BNX2X_ERR("BAD attentio128state\n"25 state\andle bits that were raised */
	if128_SHI255ack) & (attn_bits ^ attn_state))
		BNX2X_ERR("BAD attentio256state\n"511
	/* handle bits that were raised */
	if25uct b511ack) & (attn_bits ^ attn_state))
		BNX2X_ERR("BAD attentio512state\n")023
	/* handle bits that were raised */
	if51} elsead(ack) & (attn_bits ^ attn_state))
		B
	if (deasserte02rted, dto152(atomic_deasserted(bp, deasserted);
}

sta024, "ca522ack) & (attn_bits /* 42 attn_bits = le32_to attn_ack, assrt -nx2x_update_dsb_idx(bp);
/*	if (status == 0)	523_SHI90/
/*		BNX2X_ERR("spu	attn.sig[0] & group_mask.sig[0s i))erted & 0xff);
	DP(NETIF_MSG_HW, "] &
						HW_PRREV_Index, napi) tx_bdTAT(i) \
ump.: 0;
			BNX2X_Ei]k <<			RE
	reg_addr = (HCct bnH(bp) ?
				REG_RDndex, napi)f_sizpu(bp->		tt_idx),
		     IGU_INT_NOP, 1);
	bnx2x_f_si->status_blk->c_, row1,pu(bp-->rx\BITS		BNX2X_ERR("PER_ sw_cock_controi]);

	fd[1],4;
	ort.advertiF_MSG_IFUP,
	  d of LREG_AEU_GENERAL_ATTN_0 +
			  ORM_ID,_ERR("InSTERS_SPIO_FLOAT);

	switch (mode) {
	case MISC_RElow pa_STATaddr);

		     IGNOMEM;

	mT(i) 2x_reTS(le16_toisaddr);
	peeded fo	bp-METRICe "bnx2x_init.h"
#includline inr.lo = cpu_to_le val;

	if (attn 
					     IsHW_LOCKurn 0 (k== b)us_bl/

/* slow  HW asse, u32 attn)
{
	u32jsrc_of Lci_read_c	k/* snet_device *dev = h_rx_bd *)
			BNX2X_ECSTORM_ID, le1tion set0 0x%x\nance)
{
	struct net_devicedev = dev_instan*/

stact bnx2x *bp = netdev_priv(dev);

	clude <linux/here if interrupt id entries */
		bnf swap reg,   le16_		  ating that the r

		BNX2X_Ebp->intr_sem) != 0)) {
		DPPER_e16_to_cpu(bp->	     In.siinuck_bh");
		return IRjED;
	}

	bnx2x_ack_sb(bp, DEF_SB_ID, TSTORM_Ilo);, IGU_INT_		jc, " Pkb, bp->5);

	while (*wINT_ENAD(bp_DISt bnx2x 
		el/

/*
 * General spts */
		*******************   BNXCHIP_REV_IS_S index in the status bE_LENMEM_WR(bp, func_mb[func].drv_mb_h
	bnx2x_ack_sb(bp, DEF_SB_ID, TSTORM_ID, le16vere*****struct bstatic irqreturn_thi, s_lo,dy taken */
	TN_IN_UEG_R

#include <linushould bentr_sem) != 0)) {
		DP(NETI a_lo; \
	/
	if (unlike the resnt, 0);
MODU->intr_sem) != 0)) {
		DP(N_BITS a_lo; \
		sx_msix_sp_XSTRORM_INTMERROR
	if (unlikely(b>panic))
		rto_cpu(bp->d	     Ihi, s_lo,c, " P_0 |
			HC a_lo; \
		s_hi += a_hdefine DI Network Chi, s_lo, aold * 2));
		DP(NETIF_MSGold to beE_LEfair_vn.vn_credit_delta=%d\n",UT_USEC) / 8;

	ifo; \
	*\
			tn & AEU_INPUTS_ATTN_BITS_DOORBELLQ_HW_INTERRUPT) {
;
#endhw= 0; \, *e void bnx2x_e16_to_cpu(o) \
	do { \
		s_lo +=bnx2x_msi_sp_int(int irq, void *dev_ins{ \
				gorit#end)b_wr(struc.ructattn)
{INTR, "called but intr_sem not  *dev = dev_ip->panic))
 Return here if interr
	gpio_reata + paMISCkipx (%08xs se	printk(ITS_CFCx *bpte t*fp, u1rk(bnx2x_wq, &bTIF_M else {
		bnx2d - sublude <linReturn here if inter	bp->spq_prSSERT_SET_2)F_64(diff.hi, new->s##_4i, pstats->m4-_ATT.t##_hi, \
			diff.lo, new->s#Q_REG_Dt Dell Sup pstats->mac_stx[0].t##/* 8tats->mac_stx[1].t##_f.lo, new->s#HILO_U64(    pst, *_DISABLED)1ocol = TIF_M */
	if (unlikely(atomic_rint multntr_sem) != 0)) {
		DP(NETIif (tx_bufi; \
				d_lo = m_lo - ruct \
		} \
 "called but intr_sem not 0, returninNX2X_ERR("t_idx),
		     I, new->s##_hi, pstat->mac_stx[0].t##_hi, \
			dif.lo, new->s##_lo, pk(bnx2x_wq, &TIF_M_lo); \
		pstats->maF_SB_ID, TSTORM_ID, ; \
		pstatshile (0)

/* sum[hi:lo] += add  \
		ADD_4(pstats->mac_stx[1].t##_i, diff.hi, \
		       pstats->m+= a; \
		s_hi += f.lo); \
	} while (0)

#efine UPDATE_STAT64_NIG(s, t) \
	do { \
		DIFF_64g the transmit\
		ADD_64(estats->t##_hi, diff.hi, \
		 def BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panc))
		return IRQ_HANDLED;
#endif

	queue_delayed_wok(bnx2x_wq, &hile (0)

/* sum[hi:f interrdd */
#define ADD_EXTEND_64(s_hi, s_lo, a) \w->s##_lo, pics */

 += a; \
		s_hi += (s_lo < a) ? 1 : 0; \
	} while (0)

#{
		bp->spq_pr0)

#define UPDATE_EXTEND_USTAT(XTEND_64(pstats->mac_stx[1].s##_hi, \
hi, \
		       pstats->m32_to_cpu(old_uclient->s);  new->s); \
	} while (0)

#dDATE_STAT64_NIG(s, t) \
	do { \
		DIFF_64(ics */
RT_SW********************phys_i_REGISTER(bp->link_vars.link_up)
		bnx2x_stats_handle(bp, STATS_EVENT_LINK_UP);
	i = 0; i < len32; i++)
		cpu(fp->fIO_OUTPUT_LOW:
		DP(NETIF_MSG_(struct bEC * r_param * 5) / 4;

(struct b_prod;
	}

	/_prod;
2ed if swap register uend[h* sk % active *od_bdi % 2ct bnx2x *b workarounl		/* FWe;

		p->msix_tOPSTERod_fw, sw_ PHY reset ible[0].vector);
		offset = XTEND_USTAT(s, t) \
	do { \
p)) {
	GRC_ADDRESS,
			       m_lo, s); \
	} while (0)FFx%08x]\n_USTAT(s, t) \
	do { \
		diff = le32_to_cpu(uclient->s) - le32_to_cpu(olomp);
	in  (u32)(attn & in_rate , m_
		/*l);
		REG(_comp_cve */
	int gpx_desc_ring[bd_protion set1 0x >>
				FU 0, m_lo, s); \
	} while (0)

#d	 PARSING_t sk_buff *skb;
		union e
	return lo;
#endifdo { \
		diff = le32_toGeneral service functions
 */

staats_handle(bp, STATS_EVENT_PMF);
printk(KERN_ gpio_mrintk(KERN_*/

/.tus block
	 		ource_bit) {ASK);
	u,
	.top(bp, fp, nt i, rc;
top(bp, fp, ta.d* end of fant i, rc;

		rd of fa		ramrod_tx_int(nt i, rc;

		r_tx_int(t.pmf ? 1 : 	for_each_queue(bp		ramrodwolnt i, rc;

		rwo_masdrv_c bp->fp[i].cl = bnx2		ramrodck_contrnt i, rc;

		rck_contr		rc = bID_ETH_STAT_QUERY, *)&ramrod_d		rcCONTROL_7 nt i, rc;
CONTROL_7 		ramrod2 atnt i, rc;

		r2 at		ramrodared dual nt i, rc;

		rared dual it's own slot he spq */
			bp->spq		rc = bbp->stats_pendin}

static 		ramrodconfig[fnt i, rc;

		rconfig[f		rc = buct bnx2x *bp)
{
	and *dmae = t.pmf ? 1"func %d	for_each_queue"func %d		rc = bomp = bnx2x_sp(bp, tats_comp = D		ramrod_IFUP, "m_nt i, rc;

		r_IFUP, "m_		rc = bSLOW(bp))
		return;
 (bp->executert.pmf ? 1 val);	for_each_queue val);
	*stats_AE_C(bp);

		memzeof(struct		ramrodURCEC(bp);
bnx2x_link_repoode = (		rc = bode = (DMAE_CMD_SRC_PCT_GRC |nx2x_pa		rc = bINT_N dmae_command)INT_N		ramrodLE |
				bnx2x_link_report(bp		ramrodsg	__BIG_ENDIAN
				DMs		redrv_coTY_B_DW_SWAP |
#eAE_CMDmae->opcoscolle_CMD_SRC_PCI | Dsrt.pmrt.pmf  dmae_command)CMD_PORT_10;

	SHMEM_Wdmae_comman10;

	SHMEM_WMD_PORT_0) |
 dmae_comman10;

	SD_ENDIANIRM_ID,nt i, rc;

		raRM_ID,		DM##_hi, nt i, rc;
##_hi, lo = U64_L*********x_sp_mapping(bp*********it's own 	d_lo = 0; \e spq */
			bp	d_lo = 0; \,REV_It + 4*wordprintk(KERN_CONT "			     (loader_idx + 1)) >> 2;
		dmae->dst_addr_hi = 0;
		dmae->len = size
al, CK_REar *)data);
	}
	for			     (loader_idx + 1)) >> 2;
		dmae->dst_addr_hi = 0;
		dmae->len = sizeothe index in the sta DMAE_w
	wh = mTERS_GPIO_CLR_POS);eu_mmae->cize = mlude <\n",
	s->rx_sure that ACK is written */
	mmiowb();
	barrier();
e restaticsate isbp->def_t_

		OMEM;

	ma
		Bcomp_eu_m_REGISost_dmae(bp, dmae, loader_idx);

	} else if (bps idx
 * re(updasURCE~ else if (bh>
#incable[iroducers */
	x *bp)
{
	u3ructREG_WRt ");
	, m_ruct bnxx *bp)
{
	u32 *stats_cog[func] gpio {
			in_cals_coling ti   PC;
	int D3ho start; );
	int2_rate OMEM;

	mapping
		BERR(post_uct bn=x2x *bp)
{
	u32 *stats_cic_rk;
		}|= 3{
	u32 valurce_bit);
urn 1;
}
bp, stats_comp);TATUS,
	ost_dmae(bp, dmae, INIT_DMAE_C(bp));
	}
}

static int bnx2x_stats_comp(func_stx) xcliNo mb), [] __de<< gpiooutputEGISTpo0x%08xtil
		!= Df (frag_bac_control ON")D0parsrrata _init_sge_ring_bit_MSG_LINK, "Clear GPo_num > MISC_REGISTERS_108;  program i, u3r val_lTERS_GPIO_CLRE_CMD_DST_RES	*stats_cdr = (HC_R "dst_adx)]\G_HWil(bp);a!= DMAE_COMP_V _addr08x)]\
		}

	      g interrupdr = (HC_Red(struct bnx2x *bd & GPIO_3_FUNC)
			 hanCMD_SRC_&n, 0, CQbp, ie1h_ct bn  DMAE_CMD_ENDIANMF(bIG_ENDIANc, " 
	/* ComD_DST_RESRAL_ATTNF_MSG_= (HC_REG_
	/* Memo);

	DP(NETr *)data);
	}
	for (offset = 0xFstats->t#olource_bit 9/08	}
	}

	N_SHnet/tcbudgetD_SRC_RESET | DMAE_CMD_DST_RESET x2x_fw_dump(sdmae =		return;
	}

	bp->ex);
			}
	40200)) {
		BNX2X_ERvoid bnD_DSbp->pN(bpal_lo3) & esourcet/ip6_checksum.h>
#include <linux/workqueue.h>
#includies bMAE_RUT_AS2.h>
#incluprefetchW_SWAP |ATTN_2, 0x0);
					if (asserted.sert, fp_addr_hiRM_Cr *)i = U64_HI(bnx2x_sp_mapping(bp, port_stats))stat5, poSG_HW, "ATTN_GEfpsbFUNCte[0] PORT(bp);
		("BUG!\n");
2 wb_wrcontdmae->src_eactivab_write[	els_sp(bSTS_CLR_ude <	if n);
}

/*moduleponsum-----onfiexecu{
	int all	dmae = bn>=x2x_sp(bpval = RErt_stagaimsix = (

st 0;
	dmae->comp_v)tion\_PARM_ DMAE_COMP_V, thNTMEleas and E_PAGe producers AE_CMD_DST_PC_to_ce		  ve be-> octualREG_Ga(def*le (0)

 >> 2;
	dmae->c) priorx2x_re%08xu8 del_LEN32_RD_MAX;
	d)RD_MAwb_co	DP(NEworam.break;
	}
"newer"ABLE1_FUNCaddr_hi = 0;
	dmhung GURD_MAXcons_sb(%fter p->s r	 (stoutput_LEN32_RD_MAX;
	dstatRD_MAcons_sb(%x)\no rmbae->er_idx = N32_		ifX * 4);
	dmae->dst_addrRD_MAmayfig)WAP po", tvn_m	 (stbefb), " (" DRV_Msb). Ige);%08x)e_fpuf_rin>> 2fw dumfastl tinO) feaode from Mffset, _sb(%x)\n"O) feaNC_MF_COMP_len = (sizeof(struc);

n");(bp);
	bnstfw dun32_WR_ut hik.timeout 200;
ly taken  0;
	dmae->comp_val = 1;RRAY_SIZE; i++) {

		row0 =rt_stats)):;

	REG_RDN_SHIn);
}

/= (op/
	t_fair = T_e->cTTN_6!\->mae->			prefetIDe
	return struct bnx2xddr;fp_u_addr,ost_is (%NOPFG_GRC_AC(bp);
	u32 mac_addr;
	u32 *_block_omp = bnx2x_sp(bp, stats_comp);c
	/* sanity */
eply to OFFSET);ort_stx >>AILURE);
		dmae = b.h>
#incl(NETpl (!vueues forBDx2x_o ,
				 BCM57
	REGBDor (<< IGERR("fp%pa= 0;fNFIG_0e);
	pmicromae.lengineer DMAEwe.h>
#el.hT0;
	u32o < , gpi | DMAESo uirel = Dhascompens			 obTRORM_nd keep thDMAEinRD(bp, Opet_idng Systems(TM)r (of * ThisnoxF108; o16********_intliMD_ENDIANITY_B_DW__stats_co		return;
	}

	bp->executDMAE_CMD_E1HVN_S void bnx2x_releDMAE_CMD_E1HVN_Sruct bnx2x *bp)
{2x_red,T_1 :hleft++k << MI1 : bp, MISCN(bpnb[%d]:  hw_comruct bnx2x *bp)
{ct bn_PORT;
		dmase_alr(struct bnertedaddr_hval);
}

static inline uf_t_ild		     struct bnx2x_addr_heu_mask,AL_ATTN_es forfixDST_GRC |
e[%xddr_lo = bp_MSG_HW, "aeu_mar_lo;_addr_hi = 0;ask, asserted);
	aecode) + i * 4,
		     TX_QUEUED,	"*/
	DMAE_C,
				tatic((char *)(128);%x:lay 0;
	7711E =ddr_lo = bp->po->comp_val nx2x_ac
		/* 
	if (bp->funclo->comp_val = RAL_ATTN_R_ST |= as_parT_GRC |RD_MAXE_C(bp);esser)G_SINGfw dpath *f
	DP(NETIF_MSG_HW, "new state %x\n", bp-= U64_H			DP(		       NIG_REG_MASK_IN		bnIRED_MA0;
	u32 aSTAT64_NIock(e->dst_addr_lo = nx2x_ac0

#inclu
		dmae->dst_addr_lo = bp-lo)g_gocodeing taddr_lo = bp->fquire_hw_lock(bp, HW_LOCK_RESOURCE = sizeof(struport);
	aeu_mask = REG_RD(bp, aeu_ad = sizeoft_port_stats) >> 2;
	stats));-dmae-attn_stat = Dmark_PARM_BD_DCC) & ~			 ITY_noport_vidualfdef __BIed %x\nN_FUNC_0;
	FG_XGXS_Ef0f SPLIease_hcomp_addr_lo = dmae_reg_gomp_adc[loader_i
	REG2;
		dmae->dr_hi =EGISTERS_e->comp_addr_hi, DMAE_CMD_>func_stTY_B_DW_SWAP |
loAL_ATTN_bd_prodddr_hi bp,  port_st2 vn_cfg = t bnx2x *bp)
{)= U64_HI(\n",
		   gister *ort.port_stx) {
	_1 : DMAE_x_panfixif (asserted & *t",
				ae->opE_CMD s8> 2;n", bp->p 2;
>n & AEe of th enabl~ (bp->old(x_pansub:
			 = opcx_panock.ial( == MAC_ -> 2;,BIGMAC0;
		gRM_INTMEM + 2;
< ? NIG_REG_INGRESS_BMAC1_MEM :
				/12"IG_REG_INGRESS_BMAC0_MEM);

		/, -_REGISTER_TX_x) {
		swab16		dma = 100;
	for (j108; offset <= xmw_coypaddr_hi = 0;
		dmae-MCP + 0x9c);
		if (def_att_id    MISC_		}
	ip_sum DMAesetHECKSU_resRTI
	}

	/EXT_XMIT_PL, "Set F_64(d_hi, m_		}
	protocges = htons
		bpPriver s_lo) \ U64_LO(bn
			_VP(BN;

}

spv6tr_sssert->ase_*fp == IPPROTOvn <>fp_c_irs.lip_mapping(TTSTORM, bp->rx_buf_x2x_sp_mapping(bstruac_stats;
		dmae->ls));
		dmae-IGMAC_REGISTER_TX_STAT_GTBYT -
			     bp->dev);
		 {
	shEN_0);ae->lgsog, map&bp->_GSREGISV4lo = U6AT_GTBYTGMAC) >>RM_INTMEM +;
		dmae->comp_val = 1;

		/* BIGMAC_REG6STER_RX_STAT_GR64 ..6>rx_comp_prod;

	/#, m_0000, thus
		 >Y_B_DWFE * fBD - 3)eof(x2x_spif;
		rc |{
			int ;
		arizpu(*fpif (bp->Rx qoo , attn.s(unst isoFLOAT and addr_h attn.spu(*fpr +
	geT |
#i> 8K (VAL;

	*stabrd ac_advior in tng(bFW_0);
ri	}
	foetXtrTEND_64(qstats->t#keue(e lin +
				     BIGMAC_REGISTER_TX_STAT_GTe
	return o = (mac_addr
		returnelseopEM + it's macodebmac_stats,_AEU_MAS_sz_vars.link_ock.1 (o < _REGIS	dmae->og_go_MAC_REP (DM	} eDMAEAT_Gmer in-a;
		dmae->comp_vanrAX_DYs
		dcode >src_addr_lo  (u32 *, m_(mac_addr &TAT_GR64 ; vn++) w_comp_cshMSG_lso_mTYPE_;
		dmae->comp_val = obal vnto_cpu(*fp->txLSO;
		rc |date <i_modeco *4]_addr_hi+ BIGMAC_REMAE_CMD_AT_GRIPJ -
			     BIGMAC_REGISTERP_PORTwnd->panic))
		>src_addr_lo->link_vN Greenstewindow_EMAC		dmaeAC_REG_EMAverewndcomp_val = 1;

	} els		dmae->c-AC_RX_STA>> 2;
EMAC_RX(dcc_eve_REG_EMA->dsrc_addr_lo = er pe_RX_of thEM +
	DMAEHAE_CMD_RT_LISTe[%x]= rx_staRT(b)_STAT	/* Rr_t ma
				comp_lse
	}
		} e)mac_stxtcae->c& asserteach_tx_qAm.needof  MISC_w/ DMAE_CMD)
	bpGISTER_t bnxofbp->e[%x]=o));
		dmae->ln_state & assert = 1;
	STATE__REG_EMAt.po));
		dmaeeach_tx_qCalcer ius_bloes for_EMA-ing[j];pecia->link_ \
		 _addr +
				 c_addr +
	<AC_RX_STAT(fp-c_addr +

			d_hi_REG_EMA+ARB_BYT;
		dmae->comp_vamae->[_addr +
	}

	returnsupport */>> 2) -
	REGp_mapping(atus
	REG-mac_addr start; j !=o));
		dmae-t ? p->panic))
ux/workqu_REG_EMA<	dmae->c->pdev,
		struct bmai, j, rbp, po		}
_lb));
#ense_rx_s_REG_EMA-->comp_addr_lo )&fp->rx_comD(bp,]  leneasimodemin_.mac_conMD_DSrae (R(&bplo =storm		dmaec_id*/
		dma_addr_hi = ->src_addr_le->src_ad<=mae[bp->ebp, mac_s, t) \
	do_28 */
		dmae  p_val = 1;

	} els[bp->e->src_ad+

		/* EMAC_Ridx++]);
		dTAT_AC_28) >> 2;
		dmae->src_addr_hi = 0;
		dmae->dst_addr_uct eth_rx[0].t##ng(bp, macmae = bnx2x_sp(bp, dmae[bp->e->src_a	}

	retd, len, bp->rx_buf_ RX_n non-ype > 2;
		dmae->s;
		rc |ber of alway
	int   mac_REGISTE DMAE_REG
		dmae->dst_adRT_SWAU64_LO(b(i));
	ux/workqustruct 			if
		  DMAE_CMD_SRC_RESET |mp_adLREGISTER_RX_SISto HUIREDIG_EN%s;
		rc EXT_TXTERS_bp->ae->c_lo, rx_s_lo,o));
		dmae-opless_fc;
mder_idx] >> 2;
		dmae->? "LSOstatu		dmae  fp_cqe-uter_idx++]);
		dmae->opc,pcode stats) +
			bp)
{
	mutexstruct  DMAh>
#incl (max/mii.h>
#i_GO_C9, DMP_VAEG_R];

	wb_writ) rx_c].cth driat_ifhcoutoct_ATTSTRORC_TYPE_EMACstat_CMD_POifDMAE_Rlowpat (port ? DMAE__comp_ %x F_SB_ID, Trt (mac2 vn_cfg2x_sp_mapping_REGISTE/* Re-enable interrupts */
		bnx2x_ack_sb(bp, fp->sb_id, UE1HVN_SHIFT));

	if (bp->p (!(vts_c]);
	dmae-> 2;
		_REG_G*txSC(m
static void bnx2x_release_alr(struct bnx2x *bp)
{
	u32 val = 0;

	REG_WR(bp, bnx2x_r);
		bd, *

		ifo)); + 0x9c, val& ATTN_NIG_FOR_FUNC) {MCP + 0x9c, val access lock register */_addr_l_6!\n) ? Dval);
}

static inline u1tats) +
				nx2x_sp(b(mac_addr MAC_Rs));
	_lo) \
nt_d rx_stat_gr DP_LEV
	}
	if (bpaddr_hi = 0;
	dmae->dst_addr_lo = U64_LO(bnx2x_sp_mapping(bx) {
		NETX_BD aeudr);
	pci_wrHW, "k = RE= EMACmark/* bnRM_INTMEM(u32)) txr *i> 2;
		I | DMAlowpatate(se[bp->ex_acquir & (1L << 3e[bp->exe *)dmack register\n");
ae->srcTAT1_EGRESS_MAC_PKTct bn<linux/workque];

	wb_availval B_EXuter_idx++]);
		dmae->opco+_regp_u_idxe->src#_hi, attn)
{.t bnx2_xoffc, " PAE command opaddr_lotx num_rding = 0;
		UG!    
		ifE_CMD\n", _REG_GaMAE_t bnx2xval = 1;

	dmae = bnx2x_sINT_DISABLE, 0);

dmae_reg_g "SKB:] >> DMAflags));
		dm= (2*sizeof((%x,%x)comp_ad  ge <lructflageof(struct;
#endif
	/src_addr_hi = = 0;

	s));
		d, ts));
		dmae->len = (8T | DMmae->comp_addr_lo = dof(u3	dmae->comp_val = 1;

,ts) +
				oddr_hipcode = opcode;
		dmae->src_addr_lo =->spqs fo,mac_addr +_addr_lo =mae->opcodn = (skb (alc_vn_F				o,
	 ruct bmac_.nt lhi = 0;
		dmae->dst_addr_lo = U64_LO(bnx2x_spDMAE_mapping(bp, mac_ss) +
				offsetof(struct bmac_stat0)

#defineo));
		dmae-zeof(u3code = (DMAEperiodic_Sddr_c inMISC__REGISTER_RX_S16 rc = 0lin_cntc, " P_RX_STATae->opcodAC_RX_S_ATTN_BITSts, egress_mac_pkt0_lo));
	d1HVN_SHIFT));
	c.h>
#i-
		else
		silentl,
		o;
	u32AC */SKBSince we IZE;k0]);

		_anypcode = val = 1;

	dmae = bO
	retRT_SWh>
#inclung tP;
		bnut\n",areE_CMy.>mf_conRESET |
#ifBD;
		row|
		addr_nit(a0), Btn", wets) +
ap->attn_sreg_oBD ( REG_		   SO/errx = o,
	  BIompenbp, nig_statsev_infs_LEVELhestatsBDs.
	(dAE_LEforDST_tobnx2x_ce fuDMAEel.has_LO(b_mac_pkRM_ASmapkt1_loAFTERbegin CE_SPIO)de = (D...)
	Ato ab);
	armwac_idpdbT |
#]  leni = (DdcodeNOT DWORDS!ues;
COMMAND_REG + p*32 +
		       COM_int_mask_addr = p ? NIG_REG_MASKCMD_C |= asMAND_REe->srcdmae->dst_aMAND_REG_ATTITS_SET);
	u32 aeu_addr = port ? MISERRUPT_PORT1 :		       NIG_REG_MASK_INTERRUPT_PO", aeu_mask);

	REG_WR(bp, aeu_addr, aeu_mask);
	bnx2x_release_hw_lock(bp, HW_LOCK_RESOURCE_POUNICAST_ADDRESS <<
				 adcoETH_TX_START_BD_ Evex2x__TYPE_SHIFT);
	/* header nbd */
	tx_start_bd->general_data |= (1 <<m Everest networkHDR_NBDS* Copyrigght (remember the first BD ofnd/orpacket09 Broadbuf-> modi_bd = fp->ms od_prod;erms of thskb = skbnse as publflags = 0distDP(NETIF_MSGeresQUEUED,
oadc"sending pkt %u @%p  next_idxnste bdnstein \n"ned bypkt Lice, ms of ,ral Publby: Eliezeic Licezer Tcom Corpedis#ifdef BCM_VLAN
	if ((bp->vlgrp != NULL) && vlan_tx_tag_present(skby Ared by rrata Softw& HW UDP eresFLAG)) {
	roadcom Corpork Ge = cpu_to_le16(k Gendelman
gewpath righroadcom Corporbd_ Soft.as_bitfieldThis EveresBD ZoloSladislaAG;
	} else
#Eilofv
 * Statistics and Link managementby: Elieedistribturn on parson Gand get afy
 9 Bric Lice = nux/m(NEXTeresIDX(ic Lice
 *
 pGener&al Publdesc_ring[ic Lice].nclue_bddistmemset(pbd, 0, sizeof(struct ethndel#include
 *
CSUM exmit_type & XMIT_CSUMarov
 hle Linpath_network_c) 200path a- by
->
 * ) / 2ncluht (for now NS  Sof is not used in Linuxh>
#iiopo->glob
 *
 * T=: Br(x/ini| (path->protocol =ink managbment EveP_8021Q))n.c: Broa EveresPARSEx/moLLC_SNAP_EN* Copyrincluincludip_x/init.h>
#itransport<linux/netdevi: Bro>
#include <linux/netdelude <linux/ini+= der.h>
#inclu+ tcp_hdrlenpath ade <linuncludtotal#includek managementx/in *
 *x/init.x/in* <linu/

#include <linux/module.h>
#include <linux/modulepaL4cludeyteorde <linux/pci.h>
#include_V4): Br/

#include <linux/module.h>
#includ: Bro		 <linux/modulepaIPde <lin		de <lc.h>
#include <linux/prefetch.h>
#include <linux/zlib.h>
#includV6inux/crc32.h>
#include <linux/cTCParov
 
#inclucp_pseudo_csumed bwab16(.h>
#inpath ->checkbyteorlude <efine s8 fix = SKB_CS_OFFpath ; /* signed!h>
#ine DRV_Me <linux/dmaude <linuxnux/delayUDde <_FLGyteorundation.
 *
 * Maintainedroadc"x/ini%d defin	"bnON	"1beforor mx %x Writtroadcemenanagcpualloclude <net/t),_FIL, BNX2X_tner
 *
X_E1/* HW bug:_FILupnd/orlude.h>
#i DRV_MODULE_VERSION	"1.: Brobnx2xION	"_fix <linux/time.h>
#include <IX_E1roadcom fore concludies bing thendation.
 *
 * Maintain "ON	"1after_FILE_PREFIX_E1H	"DRV_MODULE_VERSION	" *
 *}
	}<linappon G= pci_map_ude lerratapdev,ce.h>
#incm NetX by
<linuclude <n, PCI_DMA_TODEVICEing toadcom Corporaddr_hicp.h>
#inclu32(U64_HI(ODULE_A
 *
 711E Driver");
MODloE_LICENSE("GPL");
MLOULE_VERSION(D-200d by
_shinfoRV_MODUnr_froftw+ 2x04020om Corp +" (" + ;
MODU9 Broadcom Corpor_paramk managementns bnxueue mode "
			 ytetwark managementBroadcom NetXtreux/iokt_#inc =hael Chan's ))");

s Foundation.
 *
 * Maintain " modifbdein <e;
MO (%x:%x) 7-200%d"ed by:mult;

st	"bnxSoftw%x ik GeE_PREFIX_  hael Chan's chael Chan's ");
MODULnt num_tx_queues;
moloPUs)")bnx2x-e1h-"
eue mode "
			  ),, 0);
MODULE_PARM_DESC(num_t;

som N)");

static i <linux/module.h>
#incqueues, " Number of Tx queuk Ge#include <linux/pci.h>
#incGSOarov
 FW files */
#define FW_FILEPREFTSOnder theX_E1	"bnIX_E1	"bnude < Disable tso
#inc %dREFIX_E  ce.h>
len, the DULE_adcom NetXtremnt_mode;
lti_mode, int,gsonum_ring th/

#include <linux/module.h>
#include <linux/modulepaSW_LSOinux/crc3unlikelyrx_queues;
module > the )32c.hnclude <lichar vael plit(bpr
 *zer Tamir
&;

static inde <lin_param(from Mich++ 1 Enah>
#incllso_msstatic int num_rx_quARM_DESC(int_mode, " Fore TX_TIMEOU Eil_seq1.52.1"32define DRV_MODUseq debug)");

st Softwarpbd_);
MODULEER		0xnux/crc32.h>
#include <liGSOcrc3efine DRV_Mip_iaram(.1"
#d>
#i DRV_MODUi Enabe TX_TIMEOUT		(5*HZ)

static int deb~ersiotcpudp_magicbug;
module_pas;
MO
static 	  ug;
module_padatic int load_c0, IPPROTO"

#, 0/byteorlude <lebug, int, 0);
MODULE_PARM_DESC(debug, " Deipv6debug m&type evel");

static int load57710 = 0,
	BCM5-common, 1-por, 2-port1 */

static strinux/firmware.h>
#include "bnx2x_fw_PSEUDOe_hdWITHOUT_LENinclable 
 *  Generde <linux/inter2009)ael Chan's rce ther(LE_L0; i <m(multi_mode, int, 0);
MOD; i++r debuLE_P;
MO_t *;
MOrt.hLE_PARM_DESC(int_;
MOD[i]rce Mnclude <linux/errno.h>
#include <linux/i"Broadcom NetXh>
#include <linux/slab.h>
reg "Brox/crc3ude <nam(nparaound by>


#
	{ PCI_VDEVIDCOM, PCI_DEVICE_ID_NX2_57710), BCM5
	MODULE_AUTHOR("Eliepagamir");
MODU;
MO->VICE7711E), BCM5_offsetPTION("E_DEVI11E), #incme II BCM57710/57711/57"Broadcom N");
MODULE_LICENSE("GPL");
MODULE_VERSION(D******************;

static int multi_mode = 1;
module*************");

static int num_rE_TABLE(pc debubnx2xadd1h-"
&am(num_r7711E), , " Force int, 0);
MODULE_PARM_DESC(disa7711E	"bner of Rx queues for mult (defaunt int_module_paadcom Nddr, u32 values;
module_pa****************);
MODU 0);
MODULE_PAR***************e_par;

MPARM_DESC(num_rx_queues, laumber of  Wridword(bp->porce nclude <linux/errno.h>
#include <linux/ght (devi Eil a tx doorbell, counton Gd/orilon BD
	 * i it under thecontains or ends with itreg_/CSUM enux/moPC_VEude <lin <7-20ADCOnbd++nclude <, PCI_DEVICE_round by****
	{ PCI_VDEclude <nCI_VD;

statam(num_rnclude <pp->pdeint, 0);
MODULE_PARM_DESC(disaPBDein <eip*
 * Tlf n>
#incluadco
statiadco, 0);
MO%u"SC(disa n.h>
 is half nxN	"1lf nint adco     PCnt int_modlloc.nclude <linux/dm_REG_GO>
#inclEG_GO_C1, i

staN " (" D, 0);
M_REG_GO val;
}

AE_REG_GO_CE_VERSION	" = {
	DMAE);

static intqueues, " Numb
/* Time in jiff     PARM_DESC(num_rx_queues, _OFFSET):ulti_mocom.comci_wrnal)
ude <lin     Preg_rMake sure thatc u32BD 
 * Tis updatedfine FW_copy atic u32Liceucerreg_rsince FW might readG_GO_C15rcomma " DRVommand memor
/* copy co.reg_rThis
/* only applicableetherweak-ordered memory model archs suchreg_ras IA-64. T/or ollowon Gbarriae(stralso mandat
{
	 set DMAEwill	int i;sumesnder ths must havO_C1s2x *b/
	wmb(, DMA>
#inclub.
 * .ude <+=AE_R;
	REG_CMDae_c	DOORBELLse on e->index - ratanum_rx_queuesr
 * Basedb.raw, DMAmmiowmae_command)/4nclude <EG_WR(bpCSUM earam(dropless_fc, avail(fpord(MAX_BNX2FRAGS + 3tarov
 netiffc, "top(BNX2X(txrrs, i/* We wantpless_fc, int toy: Ee"x2x_pcopy com 0x%08x\n", u32 af we put Tx into XOFF_DESte..h>
#ismp_dmae_c		fpl Cht->x/inqcommas.driver_xoffPCICx/crc3t + i*4, *(((u32 *)>=mae) + i));
	}
	REGrkqu, dmae_rewake_c[idx], 1);
}}ommacommandased oPCICFGre() */NETDEVeresOK;
}

/* calleddr)
{
rtnl_locktrucommaicr,
	pless_foplude <linunet_device *dev)
{
	using ichar  *bp =indidev_priv(dst_idx,, dmaecEG_CMD0 }
bp, dst_achar vset_powercommaamirme II B0dst_a		DP(BNchar vnic_load 0, sLOAD_OPEN) "DMAE is not ready (dst_addr %08x  len32 %d)"
		 close using indirect\n", dst_addr, len32);
		bnx2x_init_ind_wr(bp, dst_a/* Un

	dx2x_p*wb_co, release IRQlti qucommand));uDST_R 0, sUNpcodeCLOS711//crc3atomic_nd g(&ir");
MO->ennd *_cnt)VICE1>dmaUM e!CHIP_REV_IS_SLOW(bpE_PARM_;
	}

	memset(&dmae, 0, sizeof3hotstruct dmae_0 "DMAE is not ready , dmae_readdr from _indmcast.c %08x  len3void_comman
	merx_u32  using indirect\n", dst_addr, len32);
		bnx2x_init_ind_wr(bp, dst	u32 dmae.sr = BNX2X_RX_MODE_NORMAL |
#nt me.hdr_hP_PORT(bp)idx, i, ratau32 ldr);hi = 0STATEe = (Drov
 ndation.
 *
 IFUP, "x2x_spihalfef _() *ingci_wrO(bnx2x_st dmasp_mapg_dword(bp->pdev, PCr_hi = D_END Softwar_PREFIdmaeNX2X_MSo = U64_LAE: opcode & IFF_PROMISC>dma.dst_addr_hi = 0;
	dmae.rc_addridx,009/0UM er"
	   DP_LEVEL "sALLMULTI) ||MAE_:%08x (mc_}

st >apping(ae) P_LEV/* by Ari	    IS_E1(bp)   [%x:%08x]  len [%d *4]  "
  DP_LEVdst_addr {04020ome multirc_alti qundif
mae.opcode, dmaefine 32;
i, onumb }
};
(debuusing imae.sr_list *mc, dm, dmae.comp_max:%0nfiguration_cmd *MSG_OFstatic DMAE_CMD_p 0, ssrc_a_MSG_OFing thecom NetXtrn",
, dma=MAE: o_lo, dm(debuE_DEV bp->sl&& Net<wpath->wb}

stm(debuE_DEVi++], bp->slow bp->s->ilone_tpa, 		MSG_OF->MSG_OF_tnd *[i].8x 0x%cam_entry.msb_X2X_ queuude <liint deb*(u16 *)&
	mutex_dmi &dma[0]m(debuae_mutex);

	*wb_comp = 0;

	bnx2x_post_dmiddle(bp, &dmae, INIT_DMAE_C(bp));

	udelay(5);

	while2(*wb_comp != DMAE_COMP_VAL) {
		DP(BNX2X_MSG_OFFlae(bp, &dmae, INIT_DMAE_C(bp));

	udelay(5);

	while4(*wb_comp != DMAE_COMP_VAL) {
		x2x_post_d Softwaint loadx/device.h>  orp->slowae_mutex);

	*wb_comp = 0;

	bntargetb_comp
			udelay(5);P_E1e_mutex);
}

void bnx2x_readstruct bnx2x *bp, u;

	bnxlientle.h>_vectoae, INIT_utex_unloc32 progrBP_L_ID(bp) dr, u32 len32)
{
	struct dmae__dmae(struct bnx2x *bp, uk Genatic e Fouernet Driver "
	r_hi : Broadc"setMAE cM/* b[%d] (%04x:en32 %d)"), "DMis not reae_mutex);

	*wb_comp = 0;

	bnnx2x_post_dmae(bp, &dmang indirect\n", src_addr, len32);
		for (i = 0; i < "wb_comp 0x%0ng indirect\n", src_addr, len32);
		for (i = 0; i 	/* adjust ddmae_r}BNX2ol    "_mutex)hdr.lengthdr, uUM eMD_S> idr,
	  ->wb_deme IIoldadcom NetXtPCI |
	CAMopcoINVALID(_GRC | Dint load_cC_GRC |b_comp = ddr,
	  the tralnd gy invalipy comefine x%08reakdr, u3DMAE_C}

voN
		      DMAE_CMD_|
		    DMAATEE_CMD_SRC_RESET E_DEVICAE_CMD_DST_RESETdr, u3MAE_C;

M.dst_addr_l   (BP_PORT(bp) ? DM	 }
};
dr_hi = 0ae) EMULx%08x\*(1ti_mp->dmae_mt workqu  (BP_E1HVN(bp) << D%08x\n",
HVN_SHIFT));
RT_1_GRC | DMAE_CMD_DS = idr, u_GRC | DMAE_ (BP_E1HVddr_hi, dma_GRC | DMAE_mp = bi;

		th rep->cl_iCM577;
	dmae.dst_a* Slrved1
		DP(BNX28x 0x%08_posuse onRAMROD_CMD_Irk driSET_MACc.h>: Broadc);
MODUen32;
	dmODULE_Ax]\n",
	   bp->sloom NetXp, wb_cLOp));
	dmae.comp_addr_hi = U64_HI(bnx2x_sp_map   (strE	"2009/080402E1HDMAE_CM/* Accept one, u3m FW__addr_hi, dmae.dae.comp_addr_lo, dmae.comp_val);dmaemc_filter[MC_HASH_SIZE]	    "dstcrc, bitef _gidxT_PCI dmaei = 0;nux/vma_addr [%xc.h>
4G_GO08x (%08x)]owpath->wb_data[0], bp->slowpath->wb_data[1],
	   bp->slowpath->wb_data[2], bp->slowpath->wb_data[3]);

	mutex_lock(&bp->dma= DMAE_COMP_VAL;

	DAdlon Gsrc_a MAC: %pMREFIX_E1_mapdelay(5);

	whilhi = 0;	crctatirc32c_le(0], bp->s5);

	whil,lude AL (DMAMD_ENiE1HV(p, w>> 24) & 0xff = 0;
mp_addU64_itst_d5 = 0;

	bn&= 0x1dmae, I_addr [%x:mp_add]This progrbip->slowPORT_1com NetXtreme II.opcode, dmaadcom = 0;
REG_WR 0, s.opcode,OFFSEdr_l, i_COMP_VAL;

	t_addr [%x:p) ? DM\n";

Mrata.dst_addr_dmae.srbp, _addr);
	stor		DP(e.src_lo =E_CMD_SRC_PCI | DMAE_CMD_DST_GRC |
		       DMAE_hang_comp 0x%0 using indirect\n", dst,_LO(dm*p_addr, len32sock queu*&dmae, p;dr, len32);
		bnx2x_init_ind_wr(bp, dst_adif
	is_		   _ether\n",
	(u8 *)(;
MO
sta;
	pcmae.src	DP(BN-EENDIA <linuxcpy%08x (_ind* 4);
e_mutex);
}

DMAE: o******et/cheUM e, dmaerunppinbp, ddr,
	 st_addr_lo, dst_add = len32;
	et(bp, &dma_ee, d, 1p = 0t workquen > DMAE_LEN32_WR_MAhX) {
		bnx;

M    (BP_E1HVN(bp) << DMAE_CM(dst_addr %08x  len32 %d)"
		 mdio     Dusing indirect\n", nit_in,32 %dprta

statth->np_addad, ));
t(bnxaddr, len32);
		bnx2x_init_ind_wr(bnit_in2;
	d16 valumsle_dmarc;
	dmaephyx/pci.= XGXS_no.hPHY
 *
 LO(bnlinkrrupams.longinedbp->slowpatndation.
 *
 LINK, "_MAX * 4;:

	bnx 0xx2x_e(bp,];

	wb queu;

	 Written b	bnx2ae(bp, pt(bnx2x_sval);te[2]!64_HI(_MAX.p, rep));
	dmae.comp_addl_lo)
{p, regmissmatch (cmd:;

	g, wb_ct bn "  uMAE_REG] = vab_write, 2);
}
;
	dmae.corite_dmae_		 ad/bp, eransexpects differedmae(bp, [%xCL22
/* clinuructa[0], =\n"
	stat= MDIO_DEVAD_NONE) ? DEFAULd bnx2_MSGx2x_ :b_writeturn;
	}
acquire, intaddray(5);	, wb_  DMAE_C45     D) {
omp_addr_lo,lined */
,

	bnx2x_write al_lo;
	REG_, &r sloow1,ep(100 __BIG_	u32 row0, row1,al_hi, u32 val_lo)
{
	u32 wb_;

	hi;
	 , wb_i;
	wb_wor slo, rco = U64_L!rc  [%x wb_r slowpanx2x_wro not, DMAE_LEN32_WR_MAX);
		offset += DMAE_LEN32_WR_MAX writrc_addr_hi = U64_HI(d_WR_MAX;
	}

	bnx2e_dmae(bp, R_XSTRORhys_addr phys_  XSTO+ offset, addr + offset, len);
}

/* used only32 *bp, int */
static void bnx2x_wb_wr(struct bnx2x *bp, int reg, u32ath so no	if (last_idx)
		BNX2X_ERR( REG_write[2];

	wb_write[0] = val_hi;
	,;

		returr sloT_INDEX 0x1] = val_lo;
	REG_0x%x\n"o = U64_Lp, reg, wb_write, 2);
}

#ifdef USE_WB_RD
static u64 bnx2x_wb_rd(struct bnx2x *bp, int reg)
{
	u32 wb_data[2];

	REG_RD_DMAE(bp, reg, wb_data, 2);

	return HILO_U64(wb_data[0], wb_data[1]);
}
#endif

static int bnx2x_mc_assert(struct bnx2x *bp)
{
	char last_idx;
	int i, rc = 0;
	u32 row0, row1, row2, row3;

	/ REG_RORM */
	last_idx SSERT_LIST_O8(bp, BAR_XSTRORMT_LIST_OFFSET(i) + 12)_ASSERT_LIST_INDEX_OFFSET);
	its */
	for (i = 0; i < STROM_ASSERT_ARRAY_SIZE; i++) {

		rioctl	   bp->slowpath->wb_datausing iifreq *ifrXSTORMcmdaddr);
	dmae.dst_addr_lo = dst_addr >> 2;
	
	DP(BNXiiTSTROR*
 * T*_MAX = Uf_mii(ifnx2x_sef USE_WB_RD
static STROR:line ite[0] = reg];

	wbval_inhi;
	wb_write[_MAX->inedid,SET(i) 0), num		row3 =ORM_ASst_idx);

32 addr, u32 len)
{_DMAE(bp, reAGAINtruct dmae__MAX TSTORM_ASDMAE_C_MAX		row3,));
	);
	}
	DP(BNX2X_MSG_OFF, "data [0x%08x 0x%08x 0x%08x 0x%08tu	   bp->slowpath->wb_data_dmanew0x%0addr);
	dmae.dst_addr_lo = dst_addr >> 2;
	th so 
		DP(BNUM er08x\n", >lude ae) JUMBO_PACKET, dmaeL "coastpaak;
		}
	+lude Hcomp gram iMIN/
	last_idx =id bnx2x_write_dmae_ph2);

is doeclude racedr)
{
der thealloc "datreg_rbecausMAE_e actual_ASSERstaticisreg_r dmaecopy coma	forrt * i

	d(structpath->tux_inix\n",idx, i, 32 addr, u32 len)
{
	int      DMAE_CMD_ENDIANITY_B_Dlen = RD_DMArow2, row3d));

	dmae.opcodeSTORM_ASSE	 addr + offor (i  = U64_LO(dma_addrdelmimeout using indirect\n", dst_addr, len32);
		bnx2x_init_ind_wr(bp, dst_ driver
ping(bpOP_ON_ERROR_data[3ir");ani/* prep(100
		roae_cinux/kerif (lastASSEwsc u32 btd_ino be shutdown gNX2Xfullyfine FW_* Sl(src_aructschedule_ude DMAE_Cif (r_tasELDA}x2 driver
 * UDP CE is not ready (dst_addr %08x  len3LO(dma_addrk GenrRT_Lgistx/ne  bp->slowpath->wb_dat: Broadc,
				 k Gengroup * work_addr, len32);
		bnx2x_init_ind_wr(bp, dst_arata worka=  work     PCSett is haaccorlon Gto\n", re= 0;
d capabilitieAN
		  h rework b= ~( VladislRv Zolo |y Vladislav Zolot0x%08x\n"
	   eatureLEVEtion.
F_ Vladislavw3 = h rework |=y Vladislav Zolo 0x%x\n", last_idx);

	/* print the asseRts */
	for (i = 0; i < STR("USTOR i++) {

		row0 = REG_RD(bp3 = REG_RRT_Oddr_hi _GRC |ay(5);
	}inux/ke     defined(HAVE_POLL_CONTROLLER= RE	      USCONFIG_NET_ASSERT_LIST_OFFSM +
			      poll_ep(10 using indirect\n", dst_addr, len32);
		bnx2x_init_ind_wr(bp, dst_adisnx2x irqmir");
MO->irrrs,  BAR_TSnterrupuse     USTORM_val_lt thIANITY_
			      USTORM_ASS}_INTMEM +x  len3constow0);
		ndirect\n"_ops_commandit_indDEX =rov
.ndo   " 		w2, row3  " , " 0x%0g_gox%08x 0x%0_CMD_",
				  im Colinux%08x 0x%0;
			rc++;",
				  AE_Laddr_hi,o, dm		} else {;
	dmae.sr;
		}
	}

	rep, &dmaessow3, row2,x 0x%08x]\n",
",
				 		      nx2x 	=ux/ina[9];
	int wo",
				 doTSTRORx%08x 0x%0STROR",
				 x%x = 0x%0row3, row2,%x = 0x%0",
				 _ASSERT_LIx%08x 0x%0_ASSERT_LI,2 driver
 * UDP CS2 data0x%08x 0x%08x\n%08x 0x%0ntk(KERN_ERR PFX,_INTMEM 
			      USTORM_ASSERT_LIST_OFFSET(i) + 4);
		row2 = REG_RD(bp, BAR_USTR

	pri
			 nx2xrollFX);

			      0x0800000}; {
			BNX_dma_rect\nip, BAR_TSniirect",
				 OR("dev *;
MOD: Broadcousing indirect\n", dst_addr, len32);
		bnx2M +
			      x2x_X2X_MSGDEV%08x, &  USTO(row0 !	/* USTORM */
	last_idx = REG = 0owpatset +");
MO  bp< 8; word+32 src_addr,
	forunelse II FUNC(00000; offORM_INT_LISOR("IANITY_ect\n"SCRATP |
#elsrcdr,
	 printk(KERST_OF PFX "Canude IANITYREG_b_wrice, aborMAE \n"RD_DMgoto err_outa[1;

Mx);

(OR("resource(mrrs, ;
MODU0e(bpIORESOURCE_MEM
{
	inttk(KERN_CONT "%s", (char *)dfind	}
	printk( bIG_Ex *bp)
M_INTE_DEVIC"ERN_ERR PFX "end T_LIS-EN710/end of fw dump\n_INTMEM ");
}

static void bnx2x_panic_dump(st2uct bnx2x *bp)
{
	int i;
	u16 j, start, end;

	bp->stats_ssecos_state = STA;

	retP(BNX2S_STATE_DISAB2X_MSG_STATS, "stats_state - DISABLED\n");

	BNX2X_ERR("begin cre
		       DMMD_ENDIANITY_DW_SWAP |
rov
  offset +RT_Lest 0x%0on_dump(stDRV	dmaULE_NAMAP |
= 0x0;
		prin
	u16 j, start, end;

	bp->stobx *b	}
	pd bnx2x_ABLED;;
	DP(BNX2X_MSG_STATS, "stABLED\n");

	BNX2X_ERR("!cnt) OR("MAE_LE8x\n"ata[8] =BNX2X_ave&dmae, ata[8] =HIP_REV_pm_ca(bp,OR("ats_rx_bX_OFFSydump(st II CAP64_LPMP |
#els  "  *rx_bd_=truc i;
	u16 j, start, end;

	bp->stats_smset(izeoagement def_x_idx(%u)  rx_compf_t_idx(%u)  def_att_idx(IODISABLED\n");

	B __BIG_x)"
			  "  cierx_bd_cons_sb(%x)  rx_comp_prod(%x)"
			  " EXPx_comp_cons(%cpu(*fp->_cons_sb(%x)\n",
			  i, fp->rx_bd_prod, f}
	pEx * Sscpu(*fp->rx_LED;
	DP(BNX2X_MSG_STATS, "stats_sta_prod,
			  fp->rx_comp_cons, le1
		ifX2X_ERRdmaR("fk_t_idx,
MA_BIx_spSK(64)SWAP ons_sb(
	for (i = 0;USING_DAC    USTdx, bp_status_2X_E08x\ns_block.status_block_index);
	}

	/! Tx */
	f
	u16 j, start, end;

bnx2x_fastpath *fp = &bp->fueue(bp, i) {
faile;
	R	struct bnx2x_fato_cpu(fp->fp,
			  fp->rx_comp_cons,"
			"2009/0k->u_status_block.status_block_index);
	32%d: tx_pkt_prtk(KERN_CONT "%s", (cSystemt_idx)
		Bsup	dmaeDMA, fp->last_max_sge,
			  le16_to_cpu(fp->fp_u_idx),
			  fp->status_blpath->em {
			bp->def_xbnx2x_p;
			dump(stru;atus_blS_ST &dmae,wpath->->c_stat		  fp->
	}
VENDs_block.status_end_index,
			atus_blirt mr  USTORM_dx = REGregview */
	foioreEliebad: rx_x,
			 (i) + 8);= &bp->ns_sb(%x)\n",
			  i, fp->rx_bd_prmapomp_a8x\n spak(KERN_ERR PFX "end ats_stateMEinux/
			  fp->rx_comp_cons, le16_to_OFFSET)				
		startnocachec void bnx2x_pblock_index,2om NetXtmin_t(u64,ORM_ASSDB08x)]m NetXtreme I void bnx2x_pcluduct sw_r>dmae(i) + 8);32 *rx_bdns_sb) - 10);
		end = RX_BD(le16_to_cp_OFFSET)x_cons_sb) + 503);
		for (j = start; j != end; j = RXunmaet =HIP_R;
	}

	memset(&dmae, 0, sizeof(strucE islean indirectATE_DISAT);
	if_sta REG_bp, BAR_dworSTOR");
MODUPCICFG_GRCnx2x_mai0x%x\n", la		structVENDOR64_L	/* ad			 eak;
		}
		PXP2_eak;PGLiver.
88_F0 + */
	last_id*16x,
			 NX2X_ERR("fp%d: rx_sge[%x]=[%xC%x]  sw_page=[%p]\n",
				  i, j, rx_sge[1], rx_sge[0], 90_page->page);
		}

		start = RCQ_BD(fp->rx_comp_cons - 104%x]  sw_page=[%p]\n",
				atus_blwb_rddogSSERT_<linuxTIMEOUT + 1)) {
 0x%08x"
				&0x%x = 0x%08x"
			  fp->ethtool

			BNX2X_ERRx:%x:%x:%x]		  fp->t_idx);

|=/* print S		st cqe[1], cqe[2], cqe[3]);HWe <linuxmp_cons( DP_LEVEeue(bp, i) {
	));
	}

	/* Tx */
	for_each_txIGHDMA	}
	}

	/* Tx */
	foation.
F_ble_|/* print TSO_ECmp =  cqe[1], cqe[2], cqe[3]);TSO6;2 driver
 * UDP CS6_to_cpu(*fp->tx_cons_sb)  Vladislav0);
		end =REG_RD(bp, word] = htonltx_cNX2X_ERR("USTORM_ASSERT_LIST_INDEX 0x% USTOntk(K1], cqe[2], cqe[3]);
		}
	}

	%x]\n",
				  i, j, sw_bdx_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bbd->first_bd);
		}

		starTX_BD(le16_to_%x]\n",
				  i, cons_sb) - 10);
		end = TX_BD(le16_to_c%x]\n",
				  i, j, sw_bd+ 245);NTMEM += RXuct me.h>
w_mode)omman P_E1p, reglinummdsand perly;
	if (laite, 2);
} x2x_mc_PRTert(strG_RD(bpite, x_bd[(REG_RD(bp_dump(ode_sb_c_idxx2x_mc_SUP_addS_C45 |2x_mc_MAE_, wbC22nx2x_mc_asse word < 8; word+_dump(MAX * 4;ow2, row3nable(str bnx2x_int_enable REG_uct bnx2x *bp) REG_	       (BP_E1
RX_SGE(fp->la:bp, i) {
	->rx_cons_sb)iop->la0;
	u32 val =ons,*fp = &bp->fp[nd bbnx2x_mp_cons(p]\n",
				  i,RD(bp, addr)p]\n",
			six = (b32 *rx_bd =  USING_MSG_1 : HC_ __BIG_ONFIG_0(%u)\n",
		  bp->def_c_idx, bp->def__prod(%_LIST_INbp->def_t_idj + n");

	BNX2X_ER:ge = (INTMEM +));
		data[8] =_status_brv
 * d[%x]=[nd by		val |= (:R_CSTRORM_INTMEM +
			      						  offset +d[%x]cpu(width_spee;
		len -);
		bnx2
static i_dma*;
		v8x 0x%*l |=  REG_RM_ARM_A= eak;R!bp-			struct	/* ad +		structl_loRT_LIST_j + 1_0 |
	aticRM_A&REG_ATTN_BIT_WIDTH)st_dC_CONFIG_0_REG_SI* Copydistribut() */			   of 1=2.5GHz 2=			HCructONFIG_	val |= (HC_CONFIG_0_RSPEEDNGLE_ISR_EN_0 |
	FIG_0CONFIG_0MEM +
			 H +
						  offset +LE_RE_firmwarrc_addr_h);
		bnx2 REG_2X_ERR("USTORr 0x%x)\ *R(bp, add64_HI(b 0x%x)\->slowpath->wb__fwddr e = 0 *fw = 0ONFIG_0_REG_MSI_MSIX_INTsec"dat *te %x t[0],RM_A }
};

 Dis, 

		qe[0],));

ops0 }
};
[0],:%08x] ddr);

p->dfw_veridx, i, &= ~HC_Cused o II include <linuG_MSI_MSIX_INT_ENT_LIST_INDEX_OFFSET);
	;
	}

etXtreme IIG_MSI_MSIX_INT_EN_0)x")));

	R
 * 
			C %d (are leading/trailing edge te %x to );
	}

	D tx_bO_C14, DMAn\n"
	 it unddr, (m j, tp, as m_C14usand gobeyon< STR\n", VEND
		if (R(bp, addERT_LI/adcom NetXtreme II includ0;
	}

net/enable n	barrier)adcom NetXt (BP_E1HVbe32x-e1h-"
	barrier = 0 }
};
******* Lin} else
			val = 0xffff;
net/chec |
		(BP_E1+ Disa> 4)));
		used onkt_prod(%x)  tx_pkt_cons(%xSe %x to%d Dis_lo uct u = 0;ueue(e(bp, "bounds"  usi",
			 E(bp, reg, wb_daf (CHIP_/* Likewis*dmae,n",  4*woDEX IS_E1HMFp->px1100;
		} else
			va;
	}

*)dme(struaddr, (m

		REG_WR(ddr = port atic));

	Tx")));

	R
 * T+ct bnx2dule_,
	   t port = BP_PORT(bp);
	u32 addrort*8gpio3 atte,
				 raw_olo = Ucom NetXtreme IIort = BP_PORT(bp);
	u32 addr = port ?_ISR_EN2adcom NetXtmp_conx2x-e1h-"
ddr = port  */
 >n",
	   DGE_0 + port*8, val);
	}

	/* Make s		REG_W%dinterrupts are indeed enabled from here on */
	mmiowb();
}

static voCE_REDMAEversx to bp)
{
	int port = BP_PORT(bp);
	? "MSI
	if

		REG_WR(? "MSInera0;
	u32 val = REG_RD(b(bp, i)RT(bvele (p_mapCM_5710_FW_MAJOR_VERSION= REG_RD8(bbnx2x_i1t_disable_sync(strINt bnx2x *bp, int disable_hw)2t_disable_sync(stREVIx *bbnx2x *bp, int disable_hw)3t_disable_sync(stENGINEERe(bpnx2x *bpns_sb(%x)\n",
			  i, fp->rBad, val);
	if:%d.s SMP-safare inP(BNX2Shoule0f  s SMP-safe  reg)
{
	uEVICEnx2x_int,ending i1terrupts *2] HW from sending i3],sable_sync(struct bnx2x *b HW from set msix = (bp->flags & USI*/
	if (msix) {
		syncht;

	/* disable */
	if (msix) {
		synchp->intr_sem);
	smp_wD_DMAE(bp, reg, wb_data,     (BP_E1HVNx  len32 line		  " 0 else
			v_nE_CM: (msi _bnx2x_,;

	/*struct, RM_Ant, addr);

___irq *bnx2x_nx2x_g */
	cancel_)* make ddr 0x%*struct_REG__task)structddr 0x%x]  ccom NetXtreme IIn/4\n");
			struct[i]HC_REG_TRAILING_nx2x_ */
		DMAE 
   Ops arrayinte	elsinux/sd/or  = (DMAE_format:(str{op(8

		_addr_hi(24L "cobigE1HViaffie0_REG32{
	u32 hc_addr =}
 %08x  len32 ynchronize_REG_RDrepstruork(&bp

	/* make sure sp_task is not running */
	cancel_delayed_work(&bp->sp_task);
	flush_ HC_CONFIG_0_kqueue(bnx2xd_and_flags =
	
}

/* fast path, j, tm_USTR>wb_data[0],j General ser8adcomrm <+=\n")ov
 *m(bp,atic inline void bnj) ? DMions
 */
.o(bp,(FT) t_dmae(bp, &dmae,ISTER_UPDAT(BP_E1HV_SHI(bp, &dMODEop << IGU_ACK_FIG_x/dma-
			 (update << IGU_A+1) ? D};
	} else
		synchronize_ix2x-e1h-"ev->irq);

	/* make sure sp_task is not running */
	can);

delayed_work(&bp->sp);

	);
	flush_w);

ueue(bnx2x_writte

/* fast path */

/*
 * General serCONFIG_nctions
 */

statx2x-e1h-"
void bnx2x_ack_#      ORM_ASSALLOC_ANDnx2x(arr, lbl, , MC) \
	do { 0;
r 0x%p, HC_REG_TRAILINGT(bp);
	arE_CMD);rriering[arreadkmASSER( %s\nGFP__CONELthe chi(i) + 8);arr)barrier));
		BNX2X_ERR("     F *tx_ORM_ASSERT_		/* , PCIC"lock_iif (disther"#arr"EX 0xby the chi!= endlblhe chi}rrier, MC) {
		0;
	u32 val = REtus block is written to G_RD(bp,rrierom sebp->dmk.statu
	}
	if (fp} while (0)PR_SCRATCH +
						  offset + 4*wor 0x%x)\n",
		   val, porORM_ASSERect\n", dst_addrcharendiX_INTname[40

st{0}ONFIG_0_REG_MSI_MSIX_INT_EN_0;
	}

	DPth so _addr_hi, R(bp, retus_aDMAEX_IN G +  val;

	pset = 0;

	while (REGISTER_stk(KEfs wrND_REG + , FW_FIL"
		EFIX_E		bnxt workq from HC addr 0x%x\n",
	   result, hc_addr);

	rHj + 1addr 0x%x\n",
	   re REG_RD(b, "s SMP-safe *fweg)
{e sure all ISRs are done */
	x) {
		synchronize_irq(bp->mse[0].vector);
		offset = 1;
		foqueue(bp, i)
			synchronize_irq(bp
));
		BNX2X_EINFOs", (cLoalon G%d fromx\n",
	   re				  offs_x_idx, r 0x%x)\nMAE_Cock.statng at pos idx
f (row0 ! 0x0;
		printk(KERN_CONT "%s", (char't; i < 4)));
			r);

et rinED;
	DP(BN at pos idx
 * ->fp_u_idx of last bd f_exin");
}

T_LIST_OFFSd (addr 0x%x)\n row1, 0x0;
		printk(KERN_CONT "%s", (choFSET(p,
			     u16 idx)
{sw_tx_bd *tx_buf = &fp->tx_buf_ring[idx];
	struct en before leading/trailing edge configs_block.status_blo= REG_RInitialG_WRommanoT_OFFsORM_ASSE	  ofbnx2xx *bp)/* Blob (REGk *fpsb = fp->status 4*worr_t p>tx_buf_ring[idx];
	s,e_irq(bp->pdev      PCOpcodT);
	ifX_MSG_OFF, "free bd_idx %ops8x 02 addr ASSER_e_bloOMMAND_REG_INT_ring[bd_ bnx2x *bp)bd;
	pci_unmap_single(bp->pdaddr, (mev, BD_Udr, (mAP_ADDR(tx_eue(bp, &igu_ack), hc      PCSTORMsWR(bp, addr/
	INIT_TSED_ENT_TABLE_DATAbd;

		val &= ~HC_C val = Rite_dus block is writtentsemST_O bnx2x i++) G_RD(bp, a > (MAX_SKPRAM 2)) {
		B nextNX2X_ERR("BAD nbd!\n");
		bnx2x_panic();
	}
#endif
pram = nbd + tx_buf->firsUX_SKB_FRAGS + 2)) {
		BNX2X_ERR("BAD nbd!\n");
		bnx2x_panic();
	}
#eudif
	new_cons = nbd + tx_buf->firsd;
	b

	/* Get the next bd */
	bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));

	der bip a parse bd... */
	--nbXX_SKB_FRAGS + 2)) {
		BNX2X_ERR("BAD nbd!\n");
		bnx2x_panic();
	}
#exdif
	new_cons = nbd + tx_buf->firs now 

	/* Get the next bd */
	bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));

	bd_idip a parse bd... */
	--nbCX_SKB_FRAGS + 2)) {
		BNX2X_ERR("BAD nbd!\n");
		bnx2x_panic();
	}
#ecdif
	new_cons = nbd + tx_buf->firs BD_U

	/* Get the next bd */
	bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));

	 TX_Bip a parse bd... */HC_REG_CONFIG_to_cpu(tx_start_bd->n:
	kfreamir"), BD_UNM);turn neNMAP_ADDR(txc inline u16 bnx2x
}

v;
>tx_buf_ring[idx];
	slse iIST_INx_start_bd;
 */
statikb = NULL;

for (i R_SCRATCH +
						  offset + 4*woonrc_addr_ha[8] = 0x0;
		printk(KE2X_ERR("USTORa[8] = T_IN], bent REG_RD(bp, ndirect\n", dst 0;

	if (;
	}
	for (offset = 0xF_EN_0;
		v, SUB_Sl |= M +
			      /*2 hc zeroinux/s, BD_ex_un = 0xOM_ASS =>c_staTOP_ON_ER_mq(enable n_idx ae) T_LIEX
			Bx);

0);
s_sb(%x)\n",
			  i, fp->rx_bd_prc_status_ne32 hc_ad);
		for E(bp, retart; j HIP_RE_lo = dst_addr >> 2;
	x2x_isgleveSIX_debug_consHC_CONFIG_0_REG_ATTN_0);
		veth_tx_start 4*word))_fastpath *ftruct e <Tx */
	fline= 0x%08 >> 2;
	< STROM_ASSEuct eth_tx_start, BD_b add8] = 0x0;
	f = &fp-_bd_prod];
	strTRORM_INT* unmap first bdons = fp->tx_bd_x_start_bd;08000000; offset 0x0;
		printk(KERN_CONT "%s", (cError; i <torm, 0x%x)\FX "end of fwfdef BNX2X_STOPuct eth_tx0x%08x\ntxq;
	u16 hw_co 0x0;
		prin;
	Werr  bp->de_BD(lchar *)du(*fp->rxTX_AVAIL);
#endif
= le16_to_cpu(*fp->tx_conG_INT_LINE_EN_0;
		val |= (if

	tUB_S16(prod].skb)NFIG_0
			);
}

/* free sk"%s: %s (%c%d)_ISR-E x%dsw_cfable atidx) %lRM_IN_x_idx(%uIRQ %d, "DMAE: ox_freeboard__mod[enf(u3wb_comd\n"].x_fresw_cons, DMAE_CMD_{
		B>> 12) + 'A',		prefeMETA*(((->tx_4 multi_NX2X_ER_S16(prod bnxNETIF_Me(%x2uct "			HC(Gen2)" : "|
			Hritten , last_->tx_db.dat_data[  USTORM_ASSEX_DONE, "hw_T_LI "n_add queuk(&bp->	done+t bnx2x kb = NULL;

	return neNX2X_STONFIG_0;
	u32 val =EG_RD(bp, addr);
	int msiSIX_FLAG) ? 1 : 0;
	t msi = (bp->flags & USING_
eue *txq;
	u16 hw_cHC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
			 HC_CONFIG_0_REG_INT_LINE_EN_0);
		v0_REG_MSI_MSIX_INT_EN_0 |
			HC_CONFIG_0_REG_ATTN_BIT_EN_0e if (msi) {
		val &= ~HC_CONFIG;
	sstart_bremoveprod;
	cons = fp->tx_bd_cpage" entries
	   It will be uint(uct IG_0_REG_ATT+
			      or (offset _size);
	WARN_ON((fp->bp->tx_ring_sizBADcons;

		pk));
	dfp->tx_bd_prod
#endif

	retubnx2x_x2x_init_ind_wr(bp, dst_aun;
	sw_cons = fp->tx_pk inline u16 bnx2x_txcpu(tx_sSG_Tnline u16 bnx2x_tx_avtpath *fp)
{
	s16 used;
	barrier(); /* Tell compiler that tx_queue_stopped(txq))) {

		/* Need to make the tx_bd_cons update visible to start_xmit()
		 * before checking for netif_tx_queue_stopped().  Without the
		 * memory barrier, there is a small possibility that
		 * start_xmit() will miss IF_MSG_INTR, "sge);
	usp_que
	cons = fp->tx_bd_co pm_mess	{ 0RR("mp))(netif_tx_queue_stopped(txq)) &&
		    (bp->state == BNX2X_STATE_OPEN) &&
		    (bnx2x_tx_avail(fp) >= MAX_SKB_FRAGS + 3))
			netif_tx_wake_queue(txq);
	}
}state - DISstatic void bnx2x_sp_event(s(dst_addrae_comod(%x)  rx_bd_cons(%x)INTMEM +
			      TSTORM_Au_idx,dst__CMDETH_HAons, sw_cddr,;

M, dmaember ofdetach;
		return;
	}
DMAE_CMD_ENDIANITY_B_DW_SWAP |x_sge);
		for (j = start; j int(choos rx_bd_cons(ORM_mp))MROD_CMD_IULTI[%d] het].vector);
	} else
		s
		 * forex);
*/
		smp_mb();

		if ((netif_tx_queue_stopped(txq)) &&
		    (bp->state == BNX2X_STATE_OPEN) +
			      NETIF_MSG_IFUP, "got MULTI[%d] setup ramrod\n",
			   cid);
			fp->state = BNX2X_FP_STATE_OPEN;
			break;

		case (RAMROD_CMD_ID_ETH_HALT | Bre, u8 _FP_STATE_HALTING):
			DP(NETIF_MSG_IFDOWN, "got MULTI[%d] halt ramrod\n",
		sge);
		for (j = start; j != end; j		   cid);
			at->state = BNXT_LIST_OFFSET(i));
		row1 = R = (DMAtate is %x\n", command, fp_INTMEM +
			 break;
		}eeh_STATE_HALTE",
		   val, port, ad:%08x]  cO(bnx2x_spmapping(bp, wb_OFFSath *fp =dst_addr_hi = 0;
	dmae.le	bnx	       D dmaeg_go_cons(j + 1))lSSERTr_syncreed
 BNX2Xd = &fp-
	u32&dmae,ne BTATS(bp, wbDISGS +D
	if (N(bp) <SG(bp, S = U64_
	case (- ETH_SET_FX "en_RINGR__BIG_ENDIAN
		       ue *t
			  ALTING):
ddr_lo, dst_addr,
	 
	DP(BNX2X_MSG_OFF, "data [0x%08x 0x%08x 0x%08x 0x%08x]\n",
	   bp->slowpathcom NetXtreme II_GRC | DMAE_CMD_DSTn");
			bCMD_ENDIANITY_DW_SWAP |   (BP_PORT(bp) ? Datic voFree BNXs, SGEs, TPA pool j, t != hwRAMRernalATE_DIAG):
		DP(Nskbsns;
	infor_each	DP(BNX2Xjust de3 = REG_Rue *trx_sge_r 0x%_cons_HI(bn +OCK_NUM	   SGAP |
BNX2X_ERR("unexpected MC rex2x_fpnap8] == COly (%d, cid,i, 2x_w>dmaeply (%d)  bmemF_MSG_IFLT):
		DP(NETIF_MSG_IFDOW_SWADst_addr, data, len32);= 0; wo command, fp->state);
			LO(dma_addrID_Erecov\n",
				  val, port, ad_MSI_MSmd[%dute>tx_bdDMAE_CMort.inedx];
	ne void bcommon.sh
	}
S_STAX_INT_EN_0 |
addr rx_sSHARED
{
	har ld = &fp-truct bnx2x  eth_rx_sge *ge;
	struct eth_rx_sg;art_bd),_MSGee s(" eth_ val, poisT_INDEX 0xge;
	struct eth_rx_sgALTING):
	ge;
	struct eth_rx_sgeREG_RD8(bge;
	struct eth_rx_sge< 0xA0000= REG_RD8(bge;
	struct eth_rx_sge>ST_ICge, POWN, "_unmap_page(bp->MCPlude activueue(txq)
	for (i = 0;NO_MCP) {
		strmae.comp_val _MSIX_SHx];
EN_0 |
		   itye.co[omp_addr_lo) ? DUM er |= (H(SHRex];
  DMAIT
{
	chee sk| t)
{
	int i;

	foMBi));
!=ast)
{
	int i;

	for (i = 0; i < last; i++)
		bnx2x_frRM_ASSERR(SKB_Fge->				   s200

idx);ETH_SET_x);

BP_NOMCPt_addr,
	 ll cow int mr( bnx2x *bp,
	, MC_mbt bnCPR_Sastp.drv_mt_modeerle (l++;
		&,
		  TATEEQ_NUMBERex);
] hal_unmap_page(bp->
	struc0x%08r(sw_buf, 
	stru addr 0x%/**
INDETCH + 0(sw_or	fp-ec com-is not rehe/
	forndex]map_
	dma_adrx_s@ata[: P_buf, ORM_}
	printk( NULL	  "f:;

	rcurb_datpciOSINne %x to	  "f
 >rx_s(last, MC%x toi>rx_not r " DRVaMEM;
bus (unlikaffe %xngrx_stlast_ct\n",has beeny(page ==.PORT(bp)*32 et + rs}
		mlt_p, BAR_TSTindex];
	dma_ad);
		data[8] = 0x0;
		printprod(% ~0xnel&dmae,) {
		case (RAMROD_CMD_ID_ETH_CLIENT_SETUP |
						BNX2X_FP_STATE_OPENING):
		break;

		case (RAMROD_CMD_ID_ETH_HALT	   cid);
			fp->state = BNXUM ecase (R)) &&
sw_buf->io_perm_  *turEDGE_0 got MULTI[%d] halt ramro II ERS_RESx *bDISCONNECT");
}

statSERT_LIST_OFFSET(i));
		row1ID_ETH_CFC_DEL _MSG_IF_REG_MSI_MSIX_INT_EN_0 |
	tate is %x\n", commMAC |_idx, a sl
		u1P_E1ly(bpnx2x *bp,
				     stN;

	RESENETIF_p->rx_sge_ring[i *rx;
		etdr_t mappiid bnx2x_pA_FROMDEg_error(&lloc_ NULL))
		return -ENOMEM;

	mappingrx_sRe_statun", card));
	dscrb_rd,	forif));
	da cold-boot->dev, mapping))) {
		__free_pages(pagedev_alloc_*/
		smp_mb();

		if ((netif_tx_queue_stopped(txq)) &&
		    (bp->state == BNX2X_STATE_OPENbreak;

		case (RAMROD_CMD_ID_ETH_HALTuct bnx2 4*word));
		data[8

	while (sw_cons != hw_cfp->ns) {
		u1-ata);
	}
	printk(ev, bp-lloc_);
		for c_rx_skb(struct bnx2x *bp,
				     struct bnx2x_fastpaint(stru("fp%d: rx_bd_pPEN;
		break;

	case (RAMROD_CMSERT_LIST_OFFSET(i));
		row1 = Rmset(&dmae, 0, sizeof(structe is %x\n", command, fpbp,
				     stRECOVERbnx2apping;

	skb = net
		mb(dr_t mapping;

traffBNX2a SGE_rNTME(DMAE_againly(skb == NULL))
		return -ENOMEM;

	ma(last		  backE,
			     b(struP_E1bp->nrx_pageyn)set matx_bd u    aely(sits OKORM_nx2x_ren u16l , tx "dat->dev, mappinLO(dma_addrd bnx2x_rma_mapping_error(&bp->pdev->dev, mapping))) {
		dev_kfree_skb(skb);
		return -ENOMEM;
	}

	rx_buf->skb = skb;
	pci_unmap_addr_setstruct sw_rx_page _MSG_IFUP, SERT_LIST_OFFSET(i));
		row1ET(i));
		row1 = REG_RD(bp,);
		bp->state = BNX2X_STATE_CLe is %x\n", co
	} else
	_RINGS = nundex];handl, skct sk_brrx_buf->s			  " ndex];
	dma_addCRATCH + 0index];
	dma_ad",
	dev_alloc_sext bd );
	if (unlikely(d",
	nx2x_restatic ing));
	*pro
		mb(,_MCPR_SCRATC_RINGS = numset maOMMAND_ruct bnx2x		  " 0as_tatic inli
		  bp->def_at",
	idb_compc inline voiath tb104);prob16 idx)
 = fp->tx_bd_on_dumprever. next bd stopped
_pp));
	drever.
		 0);
.->indexmapping));
	*->indexns_bd;
}

staticng));
	*nx2x_updprod_map_addr_se,
				  1; i <= NUpdate_last_maH +
			  offset + 4*w(LO(dOSING_WAIrt = REX_DONE, "hw_cons %u  0x%("BUG!default:
		wt mrc(bp, ezer Tath"XSTOude nexpec"));
	 "endsp(bp, wb_		idound by  i;
	u16 j, start, end;

	bp->st--;
		  void bnx
#endif

	return (s16)(fp->bpr(bnx2PEN;
		sw_cont bnx2ramrod()ath *fp,
	;
	structek(&bpstate, bp->spq_prod_idx);

	/*u(*fp->rxt bnx2FX "end destroyc void bnx2od(struc bnx2x_D;
		bre fas
		val &= ~HC_CONped
		 * fo_SGE(up
		for (j ath truct bnx2xn = SGE_PAGE_ALIGN(le16_to_x]=[		      SGE_PAGE_SHIFT;
	u1}

mo_ASM_ 1;

 fp->tx_bd);sge_len;;
	s)
		SGEa = 0;
 all
