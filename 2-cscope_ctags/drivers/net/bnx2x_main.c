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
			right (c)C) 200* Copyright (c)C_ENABLE009 Broadcom Co *
 RESET
 *
 * This oratou can 
#ifdef __BIG_ENDIAN free softwareunderITY_B_DW_SWAP/or else the terms ofense GNU Geral Public ndif fre(BP_PORT(bp) ?*
 * This atio_1 : * Maintained b0)is freeundE1HVNn.
 *<<*
 * This e sof_SHIFTe som Everesrc_addr_lo = U64_LO(bnx2x_sp_mapping(bp, func_statsr Tamir
 * Based on hide fromHIichael Chan's chaeright (rratUDP CSUM erratdstorkaro netwbp-> and fax >> 2h rework by VladisunteinSUM erratlen = sizeof(/* Slowhost_ and fastpaStatistics ansoftpVladislav Zy Ar M Gendelman
 * Slowpathfastpin.cp CSUM erratlude <linuagemey Arik GendelmanrratSlowpathkernel.h>
#incinux/devicx/val =*
 * ThOMP_VALsoft*de <linux/ement }

fastic void Gendelme <listarte; y*/

Gendel*bp)
{
	if (olotport.pmf)
		Gendelx/deslab.h>initm.cosoftwLicor mmer.harovrrat/interrupt and fastph>
#includpc Gendelhw#includposetdevicnux/timstorm.h>
#includetherdnux/timer.h>
#includude <linmfx/timer.h>
#includemalloclude inux/dma-mnux/i/skbuevicenux/ma-n's bupdates.h>
irqnux/timer.hinclud/skbuffnux/timer.h>
#includdcludrengnux/timer.h>
#includbitopsnux/timer.h>
#includh>
#include <asm/byteordernclude <linux/ethtoolmaux/devicnux/tie#include <linux/bitopnux/devix/timer.h *newv Zoendelmawpathx/timer.h.nux/timer.x/irude <lincludnux/timer. *p#inclut/ip6_checksumde <linux/workqueu>
#i#includth
#includ>
#i2nux/&cludx/prefetc>
#include{
		u32 lo;x/devichi;
	} diffsoftUPDATE_STAT64(rux/dma_grerb, Slowx_inifhcinbadocteitop
#.h"/timer.h>
"include "tfcs/timer.h>
dot3x/timfcserrors#include "bnx2x_dumpdump#incunasm/bytnux/inux/x/timthe Grtnepk
#in.1"
#define DRV_MODULE_RELovrefine DRV_MODULE_VERramestoolo#inc.1"
#define DRV_MODULE_REL
rgE	"2009/08/12"
#defifragmen_VER		0x040200

#include <linujbfirmwar>
#iPREFIX_E1	jabbe1.52.1FIX_E1	ware.h>
#incluRELxcf/timer.h>
maccontrolx2x_dureceivedfies before co2009/ingense arpnsmitter isxofping.eet.h>MEOUT		(5*HZ)

* UDiclinur version[] __irq.s.h"
xpfle_hdr.h"
/* FW fit_dump.h"
gabitE " " DRoutnitdaport"
	e.h>
#incluNAME_VERSIOh>
#include SIOflownge <l#doneMODULE_RELDATE ")\n";

MODUL64UTHOR("EliPREFIX_E1	BC_V64it_>
#innclude "bnx2x_dun";

MODUL127, fre	treme II BCM57710/DRV_1557711Eto127DULE_E Dght (");

#incluLICENSE("G255i_mode = 1R("EliN(ODULE_AUTH128e, int);25de, intint multi_mode = 1;
module_p511m(multi_mode, int, 0);
MODUL256ARM_DESC511tatic int multi_mode = 1;
module_pP023m(multi_mode, int, 0);
MODUL512ARM_DESC
023tic int num_rx_queues;
module_param518m(multi_mode, int, 0i_mode = 02ODULE_Eto152ARM_DESint ti_mode=1" = 1;
module_pa047UTHOR("Elis.h"
es;
int multi_mode = 1;
module_p4095_tx_queueram(nu CPUint multi_mode = 1;
module_p9216SC(num_tx_queuemodet num_rx_queues;
module_param6383SC(num_tx_queue;eme int multi_mode = 1;
module_perrm(multi_mode, 
#includei	"Brnalmactraon[]				"iffies before conclun";

MODULuflSC(num_tx_queueuflvice..x/tim->pause_x2x_du_ TX_TIMEh>
#i_modecrc32.->x/timex[1]. E/skbnea;
modul_nclud_queintde=1"
				" (default i_ netCPUsC2 MSI)");

" Force				h>
#in ic in(1x/ioT#x; 2 MSI)ti_mme II 57ODUL_Pof CPUsmodule_param(droplesmode, int " (NSE("GP IN_PARM_DESC(dropless_fc,oll;
ess_fcum_tx_queue_quedroplm_txme II 57)");p;
MODDULEff.h>
#include <linuxex/timer.h>
#net/tcdevice.hparam(mrrs,lude <liDESC(poll,am(mrrs,#include <linux/timer.hDESC(poll,h>
#include/timer.h>
#includ(int_mint debug;
module_param(ps.h"
imer.h>
#includux/zlibhdebug, int, 0);
MODUzlib Driver");
EXTEND
MODU2x_dump.h2x_dump.h"
711E Driver");
t[3]; /* 0-cE_VERSIO2x_dout, 2-e <l1e <lie II 57/* bnx2s anmware.h>
#include  int	 the TPA (LRO) struct workommon, 1
#includealign2x-e(DRV_1nt n,
	ON(DRV_1E = 2,
};

/* indexsteiy carriersenspe, above */
static struct {
	char *nafalserd_infoe, above */
static struct {
	char *naPREFIX_E1	ne BNX2X_5771ER		0x04020c struct {
	char *name;
} boax2x_dumpfw_file_hdr Dri/ softwNetXULE_VERSION(DRV_1 XG2x_dum-e1-7711 XGb" _device_id bnx2x_pci_tbl[] = { in jifOUT		(5*HZ)7711E = 2,
};

/* in hu Tamir") XGb" 
MODULEdcom NetXtr7711E = 2,
};

/* in (" Dta =
isabe softwe_id b },
	{ PCI_VDEVICE(BROne=1"
E_ID_NX2_bl[] ),ci_tbl[] =7711E), BCM57711E },
AD{ 0 }
	cha
#inclu57711E_Tram (pci
#includp;

static intnncluding the tD*****
* General service f
MODULE_AUTHO********
* General servieze CSUM eti_mode = 1n exR****
* General serviPREFIX_E1	colligabi

enum bnx2x_board_type #incl_st
#includesinglorkarly atx2x_du.h"
e <liock chais );
MOby mcpde <lde <lti_mopx_reg_wr_ind(/* bnx2chaelmall, linud on_GRC_Avaloc.h>pci_deferredam(dDissr_indct bnx2x *bp, u32 addr, u32 val)
{
	pci_excessivonfig_dwor>pdev, PCICFG_GRC_ADDRESS, addr);
	pci_wrlatword(olotpdev,), BCFG07-2_ADDRESS,9 Broeg_rON(DRV_MODULE_half nuint multi_mII 57linuchael nfigBCM57d(bp->pdev,,
	BCM5ropless_fc,)");

stati
{
	u32 val;

	pci_write_config_dwEause on exhti_mode=1"
	" Mi_mo
{
	u32 val;

	pci_write_config_dwault)(dropless_fc,)");num_rx_qu
{
	u32 val;

	pci_write_config_dwause on exh
			     euesATA,NumbSS,ADDRE);pci_wrread_config_dNDOR_Is half,
		berblisCPUs(dropless_f
{
	u32 val;

	pci_write_config_dwover1,  drivREG_GO_C2_GO_C5, DMAE_RE3,
	 drivse on exhdisable_tpaig_dwor_C8,ense Toll, " MSI)");

static int dropluse on eodule_param(dropless_fc, in711E)*****************
module_param(poll, int, 0))"); Use poE_PARME_RE1G_GO_C6, DMAE_RE13ory and set DMAE4_GO_C5, DM

staADD_e "bG_GO_C10, DMAE_REG_GO_C11,
	DMA,
	 c: Br nd memory and set DMAE comman/****
* General servi_main.c: Br *15
};

/* copy command into DMAEd_offset =x_max2x_reg_r)");idxoc.h>linucmd_offset;
	;
	fodcomlo and set DMAE0_GO_C5, DMAE " Pode,karoen exhug)"ATA,Usebug)" cha(for de****
M_DESC(poll, " i*4, E_REG_GO_C11,
	DMAE_set + i*4, *(((linu*)x_ma) : 0oadcom	D******inclx_ma(bp->pdev, PCICFG_G_offsP(ct dmae_command) * idx);
	forlling (for debug)");mo (sizeofwork driv DMAer.
MEM + (sizeof_WR2 drix_mainfiggo_c[idx],    "}

	pci_chael wriin.c:debug, intname/devicer.h>
#in(mrrs, int, 0);
MODULE_PARM_DESC(mrnig(droplesMax Read Req Size (oratdcomih>
#include
		u32 *df old009/mer.h>
#inold_e <linu*DCOM, 
{
	stt debug;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, " Default debug msglevel");

static int clude <linux/mii.h>tatich>
#inc"bnx2x_ddevicorattimer_maxnx2xESC(debulink_varsram(itype == MAC_TYPE_BMACnit.h>
#inbp, dst_addr,rrs, device.(bp, dst_addrbnx2x_main.c: Broadcomx_ma.st nEtwork driver.DESC(poll, "mrrs, D_DST_GRC |
{ct wunreachede <l		},
	{ ERR("DESC(dnux/ti} bo*
 *  but no DMA active\n"x/ir	return -1	ret
+ i*4, struct64(GO_C10, brb_ Useite_ommand) *WAP |
#enma_adoffset new   oundiard_d -_datdatioby: EDMAx/irdriver.
 underae(struc (BPtruncateftwa
) <<GO_C5,P_E1Hadco_Sn.
 *
	dmae.src_ORT_0c_addr_ldma_addr);
0c_addr_lbnx2x Driver");
MO_NIG(egressT);
_pkt(defaul				" (defv, P32 a_REG_GO_C1, DMAE_REG_GO_C2, DMand inted on code dsaddr_1,  DMAE_REatic void bAE_RE5_GO_C5
	memcpy(old,.src,eX2X_nd fde <li_MSG_OFF, DMAE_lude_ad&p));
	dma;

/* inx_wq;0nx2x_wq;
_hi), &NNU Geralram(drople)d_offset =4_HI(bnx2x_sp_ram(dropCSUM));
	dmae.sr_PORTdi, PCreg_rdPORT_1atiomodule_param(
		    "d(b [%d *4] MODUreg_r"d1,
	DMGO_C10, ESC(debug, " Demii.h> = ++r [%x:%08x]  comp)e <l0xenDMAE_mset(&mmand 0 = SHMEM_RDmodule_parmb[undned m.co].lude i, dmae.srrdeESC(i u32 d.Based !=Toadc DMAi, dmae.src_a)nux/d_comp));dr_hst_addr,c =ei, dmae.src_a,  modify
 * itNIG mae.s al);(%u)\n"Fe.comp_admae.comp_val)o4]  "
D# Lic
	 <libp, dst_amae;= 0; iDESC(debug, CMD_SRCE_REG_GO_C11,
	DMAE_REG_CCMD_ault deb conry nux/debp, dst_addr, dafinux/deh>
#includeGO_Crm_perer.h>
#includt.h>
#_PARM_	& MSI)") u32 *bn.c:onde <linux/le.h>r(bp, dst_aSC(debug, " DefFF, "DM_ *bp(&olotx_maimuand fastpaindirect\n", dst_addr, len32);
		bnx2x_init_ind_wr(bp->wbi wbin.cp.compde <DP->total_bytG_GO_C11,
	DMA st netw0x%& Gendelmawpath and fastp_base/int}intent--;
		/* adjust delay  + i4_HI(bnx2x_sp_SC(debug, " Defam- 2%08x\n"
u32Based on   dm_C9, OW(bp))
			msleep(ioportb32 len32)
{
	str	DMAE*(((u32 )]\n", PCICFG_GRC_REFIX_E1	x2x ,
	{ "Bro->pdev, PCICFG_GRC_offset x_mab_data[*ut!\oc.h>/* bnx2x_maino_der.0) |
		 ->pdev, PCICFG_GRC dst_ady) de <li00;

	if
	for_T_RE_rx3]);uRC_R, iomp));imer.h>
#inclfastnclu *f/iope;
	ufp[i]OFF,);
	cl_ita[0fp->  usi"
		!\n") =(bp));

x2xclireaku32 lebp	i;

 mae, INT_ driven.
 adcomu00); < len(\n",hile ([rect\)_LEVBased on,chak32compg_rd_ind(b0; ,
	{i <rn;
	} &indi0,ertner
 s 0x%

	mereuurn;
	}

	memset(&dmae, uzeof(str; i++/int	datt netwoal;

	pci_write_cFG_Gutex);
}
: 0x%	dmae.ot network driver.
 *
 GRC,
	{ |	dmae.sbnx2x_main| DMAE__C_ENABLE |xurn;
	}

	memset(&dmae, x| DMAE_CMD_DST_PCI |
underen32);
E_CMD_C_DST_PCI | DMAE_CMD_C_ENABLE |else
		 *4]  "
 driver.

	u32underNU Gbnx2x_main_CMD_PO_C_ENABLE |, " Defaulq(&dmae, qDESC(debut dmBP_addr_(bp len32);nx2x_dum			msre(bp));nux/deddr)id?TP_POi);
	(u16)(le16_to_cpu(_CMD_PO->		       ype,, i,1) !src_INI		clud from Michael Ce;
		REGodify
MSG	retuS, "[%d]rStati;not      (BP_POelse
	");
}

 "_addr,le Michael (%de <lie;
	u32 o, dmaelofor addrn;
	ieg_go_ code from Michael,Zb_data));
	dmae.ds"
		
ata[0],reg_r	}sed on h&dmae,| DMAE_4_LOizeof(sde from Michael Chan's bnx2 driwb_DCOM.comp_ad= DMAE;
	dmae.cofromHIBNX2X_MSG_OFF, "DMAE: opcode 0x%08x(bp));VAL;MAE_CM
(bp));r_lo, dmaecode from Michael Chan's bnx2 driut!\n"_Cno.h>
#dcomDP(},
	{ EL "src_addr  [%x:%08x]  len [%d le.h");
			, wb_comp)e <li|
#ifd |
#ifdL;

	DP(BNX2X_MSG_OFF, "DMAE: opcode 0x%08x\n"
	   DP_LEVEL "src_addr  [%x:%08x]  len [%d *4]  "
		   t netwaddr [%x:%0t netw8x)]\n"
	   DP_LEVEL "comp_addr [%x:%08x]  comp_v4_LO(bnae.comp_valDMAE_C "src_addr  [%x:%08x]  len [%d 4sed on
		;
	dmaV_IS_SLOW(bp))
			msleep(mae, Ile32n, dmae.al 0x%08xrcv_bDEVICasx2x_rea.hDRESpp bnx2x_ry(am(m
	wp, srct = 2oAE commaL) {

		if _dat
	>
#in!cnt_data[ifdef __Bin.c: B i*4, *( timeout!\n");
			break;
		}
mp U64_LO(dm 
		/* adjust delay for emwriteon/FPGA */
R("g(bp, wdr >>(&dmout!\nti_m			break,
	 S_SL0x%08x 0x%0ut!\n");wpat
	   dmaeMSG_mapp "DCOM,[ing(lo]  comHIP_REV_IS_SLOWa[i] 
	  msleep(10(defth->wb_data[0], bp->slowpath->wb_data[un	   bp->slow8xping(_dmae_phys_len(s
{
	_com Zolots
#inclu->opcode [0], dma_addr_t phys_addr,
2)
{
	structte_t phys_a bnx2x_ro,
	 			msleep(100);
		 ! dma = bnx2x_rbp))
			msleep(100);
		
#ifdx 0x%08P_VA>PORT(bLEN32_WR_MULE_PARM_set,
				x]\n",
	   bp->slowpath(bp));		break2 src_addr, u32 len32bnx2de <b
		/* adjust delay for em2 src_addr,08x offset,
				2 | DMAE_C_GRC_An;
	}

{2 le	       offsi = 0; i, physMAE_CMD_sizeoay forC_data[2], bp->slowpath->wb_data[3]);

	mutex_unloc 4;

	P_VA-P_VAL) set, DMAE_AX;ae_phys_len(struct bnx2x *bp, dma_addr_t phys_addr,
	tic voidturn;
u32 leCMD_workarlphys_aREG_GO_C6, DMAT 2,
};, wb_c.h>
n5771ing(bpslowp	cx_wb_rd(sackx\n"X2_57711), BUSE_WB_RDm bnx2x_u64
{
	s1],
	   bpbp->pdev, PCICFG_pcode , }

	 = 0; is_addr,
2]);
	wb_cRD
			dinlinreg, wulation/FPbp->pdev, PCICFG_mc_assert(s0], wb_data[1]);
}
#endif

static int bnaMAE_dx;too_big0) |
		 ct bnx2xx\n"
	   D
	int cnt =  rc = 0;
	u32 row0, row1, (bp, wb_data[0,  XSTORM_ASSERT_phys_aSUBBP_E1HVNU workuon/FP XSTORM_bp->pdev, PCICFG_G)");reg(i = 0; iata[1]);
}
# (l row2, ath-BNXm __BIG_XSTORM_ASSERT_LIST_INDE64(s_addr,
			 s_addr,
1]u32 l#int the asserts *b
	for (i = 0; i < STROM_ASSERT0], 71x 1t the as(struct , rc.comp_vlinurow0x2x_fdef __BIG_i = 0; i < M_ASSERTX_mapSET);
 =x_sp_RDinlinBAR_XSTR/ i;

t(&dmae, izeo   i = 0; i < STROM_ASASSERT(i, i,ENABLE R 0; INTma_ad2x_reg   XSTORM_ASSERT_LI	ifx2x_write_dmae(bp, pE_REG_GOt

	wbslowr slowpath so nd on coden)
{
	int offdr_t et, addr + offsex]\n",
	   b(i) + 8AR_XS_wr_ len3
#inclu so n   XSTORM1}

	lay forp, B != bnx2x_wb_wr(struct bnx2x *bp, int OPCODE) {
		ST->wb_data[0], bp->slowpd on code1],
	   bp->slo8x"
		 COae_phys_len(struct bnx2x *bp, dOPCODElation/F2,bp, 1M_INT]);

		rc++slowpc Licetion/ bp->slowpat}
_t phys_addr,
2		       u32 addr, u3 		rowow3T_LISRM_INTMEM +
			   TSTORM_ASSERT_ulation/FPGA */ET);
	
			mT = 0;T |
#t the asST_OFFSET8(i) + 4);TST_LIST_OFFSET(i) + 8 0; i < i < STRense 	char sT |
# = R_t phys_aREG_GO_C6, DMAX_XSTROint reg)truow0 !ERT_LIST_INDEX 0x%x\n", last_OPCODE) {
	 +
			      XSTORM_dmae.o64(wb_data[0
	; i++) {

		row0 =_ARRAY_SIZE; i++) AR_TSTRORM_INT+
			      XSTORM_ASSE
{
	char latru; i++) {

		row0 =SSERT_LIST_OFFSET(AR_TSTRORM_INT
		ae_command)->lude <li0) |
		 E=ae.src_phy 0x%x = 0x%08x"
OM_A      XSTORM1ttl00x%08x"
		 COMMON_ASf __BIG_0; ilast_idx)
		 bp->slowpathOW(bp))
			msleep(ae_phys_len(struct bnx2x *bp, dma_addr_tx_regRT_LIST_ " 0x%08x 0x%08x 0x%08x\n",
	o;endif
WRtatic int bSET);
	(i = C; i < STROLIST__LEV wb_"mae_phys_len(strucSERT_LIST_INDEX 0x%x\n", fdef __BIG_STORM_ASSERT_LIST_OSTORMMMONxx2x  0; i < STROM_ASSERT3 = REG_RD(bpROM_ASSERT_ARRAY_SIZE; i++) {CSTRomp_aM +
			  RD(bp, BORM_IN i < STROM_AX 0x%x\n", last_idx);

	 CSTORRT_LIST_INDEX 0x%x\n", < STROM_ASSrow0 = REG_RD(bp, B) {

		08x"
T_OFFSET(i) + 4);i));
		row1 = REGROM_ASSERT_ARRAY_SIZE; i++)OPCODE) {
	

		row0 = REG_R; i++) {

		row0 = REG_RD(_ARRAY_SIZE; i++) {

		row
			      CSTORM_ASSERT_LISTTMEM +
			      XSTORM8	dmae.ow3RM_INTMEbp, BAR_CRM_INTMEM +
			      XSTORMCSTORROM_ASSERT_ARRAY_SIZE; i++) 0x%x = 0x%08x"
		_ASSERT_INM; i++) {

		row0 = REG_RD(_SERT_LIST_OFFSET(i));
		r
			      CSTORM_ASSERT_LISToM_ASSERT_LIST_INDEX08x%x\n  CSTORRT_LISMMON_AS   CSTORM_ASSERT_LIST_INDEX else {
	M_ASSERT_ARRAY_SIZE; i++) < STROM_ASSERT_ARRAY_SIZE; iMD_DST delaySTRORM_INTMEM +
			      CSTOR = REG_RD(bp,int the asserts *f __BIG_ESET x%08x U = 0; i < STROM_ASSERTDEX 0x%xNDEX 0x%x\n", last_idx); the asserts */
	f+
			      XSTORM_ASSERTow2RM_INTMEM +
			  STRARRAY_SIZEFFSET);
	if (last_idx)
		Bw1 = REG_RD(bp, BAR_TSTRORM_IRM_ASSERT_LISTM +
			      CSTORM_ASSERT_CSTOR			      TSTORMEG_RD(bp, BA));
ow0 = REREG_RD(bp, BAR_USTRORM_INTMEM SST_OVALID i < STR(bp, BAR_TSTROlast_idx);
STUSTORM_ASSERT_LIST_OFFSET(i));
		row1 = EG_RD(bp, BAR_USTRORM_INTMEMT{
			break;
		T_INDEX 0x%x\n", last_idx);

	/*M_ASSERT_LIST_INDEX 0x%x\n"t the assG_RD		row3 = REG_RD(bp, BAR_U the) + 4);U));
		row1 = REG_RD(bp the asserts */USTORM_ASSERT_LIST_OFFSET(i));
		r addr + offset, DMAE_AXNDEX 0x%x\n", last_
#else
	rc32 lee II 57n32)
{
	strfbp, BAR_Cruct bnx2x *bp)
{
	u
	last_idx = REG_RD8(e32 ddr,
9](structNDOR;(last_idx)
		 *bp, u32 src_addr, u32 len32ucle.com, addr)_lo", last_idx) offs;
}
#en fw dump 0] =ICFG_GRC_A len);
}

/* used onl

	mark = REG_RD(bp,x2x_fw dumpdata[0}5);
odif10);
		rarkwork(m wb_comp);
	int cnt = 2om

	mark = REG_RD(bp,4_data[ = RENDORdmae, tonl(< 8_RD(bpel Ch8x]  comp_vpRD(bpnt cntruc200p, MCP_REG_DST_PCI |
a[NDORmarkhtonl(dif

stASSERT_LIST_IN!_MSG_OFF,p, wb_data[0]2 AR_XSTRORMwpat = REmae(struc0, MCata[G_MCPR_SCR_SCRATCH ddr,
		);0xF108;(bp, re <= et +=- 0 "%s",P_PORT(bSTORM */
	last_idx = REG_RD8(bp,CSTOR000; offset += 0.len, dmae.al 0x%08x\n",
RATCHi) + 4DEX_OFFSET);
	_CSTRORM_INTMEM las0x0;
		pCSTORword < + 4*NDOR.comp = 0x0H + 0xfcomp));ae(str   d " 0x%08x 0x%08x 0x%08x\n",
			100);
		elsb_data[0], bp->slow
	mutex_un(BNX2X_MSG_OFF, "tex+ i*4, *(((u32 *) 0x0;
		printk(KERN_CONTinlina);
	}
	printk(KERN_ERRb"*******me II 577onst so not ibp->pdev, PC= 0x0;
		pprintk(KERN_ERR PFXtate - DISram D]\n",

RD(bp, BAR_Ubegin crash  "%s", 
#in/
	BNX2X;
	t +=mp\n3) & ~ef_t[] =rina);
	}
	printk(KERN_ERR PFX "end of fw 8mark0xmp_v	kR("deef_atk(KERNx%08 PFX)  d = REa->slowp	  bp->def_c_idx, b "endblisfw DULE\nnclude "bnxtex_unlo]\n"
	   , d4_LOfilter0x%08x"
		for slowpath so no, bp->spq_prodx = 0x%08/xF900ae(strucxxx2x ****		struct* Rx   TSTOR_T_REtic constddr %addrX_MSG2 %drk, offssrc_ae.src_addr_lop->fp[i];

		BNX2X_ERR("fp%d: rx_TORM_ *rx_bAE_REs_sb_cons(%x)"
		4_LO_ERR("CSTONX2X_ERR("fp%d: rx_inlip_con(%x), dm)) bp->sloSC(debug, " Def	} else {
			  ;
	dmae.dst_*indirx_ DP_LEV "src_addrpoftwnglay 
	fodr,
			       u32 adlude <linuxnlo, dncludmrrs, int, 0);
MODULE_PARM_DESC(mr., dst_addr, len32);
		bnx2x_init_ind_wr(bp, dst_a"   irq.h>0;

	if (DESC(debug, "devsrc_addMON_ASM_INVAax_sgesizeo row2, ];

	int cntilo(&ae(struc_LIST_OFFSET(i) STRORM_Iove OFFSE) +blk->u * UDus_b *bp.x_queue(bORM_INTMEM +
			      CSTORM_2X_ERR("ftbd_prod, fp i_data[strM +
			   TSTORM_ASSERT_LIST_ATSd_cox2x_retindix_queue(bach_tx_queue(bp, i) {
		strockite_e 0x%t, e  i, row3, row>fp[i];

		BNX2X_ERR("fp%d: tx_p>pdev, PC%x)"
		thp->tx_add, i) , indis, le16_concpu(*fp->bd, i)	      XSTORMX 0x%x = 0x%08x"
		 + 4);"  		BNrxOW(bp)bp, dst_a_cons(%);
	}
D(bp, BA(bp))
			msleep(1(%x)"
			  "d_px_db, i) bd_cINDEX 
	dmae.dst_indifp_c_ibd_con*sb	  fdxbd_c   CS)"
			_PORpeCSTOrxp_cons(%lock.sta
;t dmaepath->wb_data[1],
	 
	tart, eni];
ing    T+W(bp))
			msleep(f(stru%d)D(bp,izeof(s. 0x%x = 0x%08x"
prod(%x)  tx_bdc*fp = &b	nit. i "  tx_b1],
	   b  tx_bd_cons(%x)  *tx_cons_sb(%d,
			  fp->tx_bd__prod(%x)  rcomp_prod,
	reg_wr_ind  tx_bd_cons(%x)  *tx_cons*********te, 2);
}

#ir_indta.prod);
	}

	/* Rlengthx);
}
od,
			desc_ring[jMCPR tx_nx2x_pci_tbl[] = {BNX2711E)I_DEto_cpu(*fp->tx_cons_sb));
		B	DP(BNX2X_MSG_STATS, "stat bnx "  tx_b fpx2x x%08x fp%_block.status_block_i (BP_PORTdi2X_ERR		indit themx),
		

	memsetc_addr_loHI),
		, i) );

	crc= RX_SGE(d:/* Rbd[%x]= dmaepcodsw_bd=[%p710 = 0,
	BCM577112 rx_csgtworkt cntx2x_dd,
			_sge%x]=[%x:%x]  sod,
	sw	   page *sww_pagebo: DMnd)), abow_pag%d: rx_sge%x] ifo= RX_SGEdex,
d; j = RX_SGE(j +	}
	for (offset = rx_sge = (u32 *DOR_ed_page->pag = RX_BD(j + 1)) {
			2 %due assprod);
		page->pag, rx_sge[0],D(bp, BAR_Ufpt; j word)e = (u32 *_idx_page->p2 *cqng[j];

		)&fp-_ERR(ERR("fg[j];

			BNX2X_ERR( + 1)) {
			u
			struct, cqe[nx2x %x] _pag->pg[j];

			BNX2X_ERR(rx_comCQ_BD(f->status_blk->cabo08x"
->rx_sge_ring[j];
			struct sw_PCODE) {
	);
}

stVENDOR};

OFFto_cpu(*fp->tx_cons_sb));
		BN 0x%pci_wrth so _REG_GO_C0, DMD_OFFmpcons_ - 1(deftx_writefoX_ERR("fp%d: rx_sge[%x]=[%x:%x]  sw_page=[%p]\n",BCM5771t Ethernenib, i,24p->sldef_u_[2], cqe[3])agnx2xring[j];

	heartbci_wr) {
			u3e_ripacket;
		windowe {
			break; j, sw_bd->sk!= end *cqr_each_x_bd_prod,
			  fpg[j];

			BNX2X_Er (D(fp
#inc *cq!=+ (j =,n", le165);
		fop->rx_coTXh_txus_bloc_GO_C8, DMAE_REG_GO_C9, Db) + 2ff.h>
#include <linuxdrvtartRR("fp%us_block X 0x%x= RX_SGrod);
	}

	/* fp_ (j =bd_codb.d j, tx_bdons_sb), ffp8x 0x%08xae(strucdrirx_c i <;

	if (!bp->dm4);
	 b_data[0]pkRT_0_SCR= enchael mskb_ux/bi_failus_blockFFSET);
	hw_c], serrD(bp, Bueue(bp, i) {
		struct b
#ifdes nox%x\ady HVN_SHIFT))));
	dmae.sr_comp_BD(j		u32 *08x]\n	  fp->tx_b("end crfw_d+dx(%D(bp, REG_CONFIG_		  fp->tx_bx_
{
	char l_ERR("BEG_RD(be <lt msix = (bp->flageg_go_c[] )");
sh bp-> - USING_M & USING_MSIX_>flags & USING_M		  fp->tx_b*bp)
{
	u32  & USING_MS*bp)
{
	u32ons_sff.h>
#include <linux/dma-msge_prod(%x)  last_max_sge(%eviceddr, data, lp#include <linde <linux/t>def_attI_MSIX_INT_E!<linuxrrntatic 32);ata[0]TTN_BIT_or (jZolotatttx_bd_cons_WR_MAX)AE is ndevice.);
	}{

		ro *bp, dma_addr_t .com&&   rx_b) + 24atus_bORM_
	3)tn * Uodify
 * it u);
}
ng(bpwereE: opcode 0x% = R3MMON_sal Pu |
h>
#in.anic((BP_PORa[0]SIX_I
	* print(bp, BRR("fp%d: nux/irq.h>
#+ 1)) {
			u3e_riIG_00; ofSINp->msglevel & NETIFc_addDULERx2x *bp)
{
	int por(utex);
}
%00__c[] f(struT_0)009 B*4]  "
	ata[INTR, " off= enow0 
	fp[09 Bnum->wb_datas] bnx2	dmae.olse
art, end	DP(BNX2X_MSG_OFcommand)tr%08x]  c);
}
=AE_CMD_C_DST_PCI | DMAE_Cdebugrtrucundation.
 EG_RD(bP(NETIF
			u32 *08x]\n",irect\n", dst_addr, len32);
		bnx2x_init_ind_wr(bpX_ERR("fp%_to_cpu(*fx]=[%x:%x:%x_sb), fp->rx_comp_pfp		BNX[2X_M->def_x_idx, DEBUG "%s: REG_, fp->rx_nam
#incHZ)
leE isg/trai)dmae tx avail (%4x)2x_wbhcfor EG_R)addr 	dmae.op lags(%lx bnx2 drffset =NGLE_IIST_], b(l, %d  = 0; i < ST	last_idx = R = (0x		BNX2ns(%sb),_tx_);
		BNX row2, iowb(RR("ed_infoe nih>
#inrx usageE1Ha[i] rde <r *nait
	barrier();
%x]= edage=|
#iT_INIS_addMae.comp_val = DMAEal, poex%08x REG_Wbp -[j];

		 _MAE( 4))*8, mp8, vaee0f | (1);
	ound BP_PORT(b+ _REG_LEADdr);;
->e <linux"fp%dx_quenable nig and gpio3 attent%s (XIG_0evCI_V %uconsrbP_POR II   barrier()e <last_idsrc%u	ast_idxinlinHCnetifb_dataISR_    009 Bdevnx2x"ble " : ? Hn" = 0; i < STROM_ASSEREG_CONFIG_80= REG; word < +="
		    "d(bpe.comp_adr,
	   dT(i) + 1able nig and gpio3 attetDESC(: REG_RD= 0x%08x"
	
	}

_GO_"INDEX 0x%x3>fp[i];i = 0 %lu8);
		row3 = REG_(addr HC_CON	for_each_r= (b_(bp, i) {
		struct*TTN_BIT_EN(j +od(%x)  rx_bd]TIF_	  "  rx_comp_cons(%(TTN_BIT_ENNX2X_ERR("CSTlloc.h>
HC %d (a
		/* adjustR("      fp_c_idx(%x)  *sb_c_ = 0; i < STblock.statusata[8] = 0x0;
		printk(KERN_CONT(bRR("_go_cmc_adr);
j + 1)) {
			Bach_tx_%d: cqcomp)c = REG_RD(bp, addr)cpu(*fng[j]~(BIT_ENFI(bp, i) {
		struct = REG_RD(bp, addr)cons + 503);
		forhw)
{
	intLINE_EN_0 |GL->flagsI_FLAG) ? 1io3 at{
	int port);
}

vaNX2X_ERR("CS phys_aueue(bp, e(struct bnx2x *b();
	barrier();

	if"DMA:009 \t G_GO_C8,init0 = RWR(bp, HFa[i] f/FPGA fset		  i-----portARRAHWdulepas= stif (fp->
#in    TS"end cr
		 HC_COld, f);callpath r");
et;

	/* der.h>
#includinux/irq.h>
#in>
#include <linuxrder.h>
#include <linux/e, dmae.src%0op int, 0);
MODULE_PARM_DESC(mr Evel.h>: Br * EveC_CONFIst nreg,		BNXloader_ *na= PMF_right _EN_0 | ? 1 ? 1 : INT* make %x] fset;

	/* disabATTN_, bp-xecu {
	OFFSE-b(u32	synchs andight (c)nx2x_on
 *
 * This + 4*7-W_FI B_wq);
This programbp)
frr de the see; yt and/oredistribute it#incblicmodifyq);
it the G) ? s
 */

stashed by
 *  */

sFree SoLiceC_CONx 0x8 >
#in, u16 i		bnxFree S, le= sr  g@brdst_addrwq);
on Greensteiy: Eilon Greens/del <eilo +dr 0e soft_com>q);
Wr] __nCOMMANli*******ONFIG_0_REG_INT_].vectoomp)
	mir
  not running */
;
	} omicX2X_MSGsp_ta++ that de <linux/mii.h>nit.mir
 * Bsh_workqu
			 (s General servth */


{
	st8 updaet, a009 Bro(>
#in);
	IGU_ACK0; oITROR_STPCI bnx2 errata workarox/moduleparam.h>
#include <linux/=*name;stpath rrework atype ang@br>  /* for dev_info() */
#incluport, addrwpath->wb_da Link manlaet/ilot,
		 _SHIFT);module.hh>
#inc Link managt bnx2xby Yitchaor de
			udelay(5);
	}
	   (*(u32 , (*];

	ER_STATUS_BLOCK_I  DP_L.vector);
		offsetmodffsett_addr,
		rq_cons_s]rier();
}ude <linux/tirq.h>
#i ake s.h>
#include <net/WR_MAX) DMAE_bnx2#inclx_queue(bp,	BNX2X_ERuleoll, (bp, dst_addr, data,de <linux/timerrrier(); /* statusk *fckbp)
wrtartrod,= sta()amir"imer.h>
#includ(&dmaare indeed  tx_EG_Rif (msi) {
		   r	_addr, data, l 4))ixp%d: txling (for dlGb" dr =u_a, i)b_id_and_rrupt dx(%x((k.staU_ACK_REGISTER_UPDdate inux/dU_ACK_REGISTER_UPDATE.h"
#in/* priort, a(update opU_ACK_REGISTER_UPDATEINTERRUPT>
#ir
 *d fastpath rBNX2X_MSG_OFF, "write 0x%08x to HC addr 0x%x\n",
&igu_(src_addrrics and Link man);
	REG_W<linux/imodule.h2X_Eigs_blo] = bn/ +
	b = urtormat ACKk.statuack;


		mmU64_LO(bnxdx(stule.h  fpinux/d_fpk.stax(st 0x%08x fto u32ARRAchipfrom T_INex,
			  fdxhost_status_block *fp(bp,s_block_index) {
		fp->fp_c_idx = fpsb-
 * UD%x)  *tx_cons_sb(%x)\n",
		;
	rc |t numwpatath s_BIT_EN
	/* disabk isLINE* machronize_tpath *fmsix_tabl timeux/tis\n",	fl#include <linux/mii.h>bp, dst_addr, data, ntdmc_adisae inE_I	e <l|= ffset;

	/*32 *0 skb t pos i|dxq);
retSRs not running */
	canreedlay(5);
	d != fdr =bn
	/* disabre sp_task is not x *bp)
{
	uacket ring afc, int(bp, dst_adier() (i = stynier();
	return (fp->tx_p
#include <linux/mii.h>
 producer can change */
	barrdo_noth32);#include <linux/bito->slowpathtr_st#include e x, bp(* Genon)x_rea->skbEG_Rfast USInum <linux/dma-mmmane nexRD(bp,e;
}OFF, "pkt_idx %m[_CONFIcountic v]ronizeuEVENTbu32  = {
/*cer ce	 *bp)	*/
/wpatDISe fuD	PMF	*/ {
#include <a <linux/ti,			  "uf,
 */e b {
		},
/*		LINK_UPqe[2ng[bd_>tx_tx_ndex;(IS_E1Hfp->tx_desc_ice fufunc].st Driver	pci_unmap_singlf * */
= t fp%d:t rx_bd[%x]=[ng[bd__bd)STOTX_BD(lunmap_bnx2x#inc_bd)T);
}
			_TO57711Eu32 *nb
},, "freR(tx_st %dcqe[2i_unmap_singl
#ind, f_OFFBD_UNMAPc u32((nbd >nbd) ->nbd) to_cpu(tx_start_bdindex;
OFFSENX2X_ERR("BAD nbd!\n");
		x2x_reNX2X_ERR(LEN((nbd _bx_co&ns = nbd +fp->tx_->fir
	bd_;

	dmae.dst_nbd!\n");
	ier(OFFSET)r modifyD(bp, STOP_O_c_i
}_prod(%x)\lude <linux/dma-mhandl, int, 0);
MODULE_, ata[1],
	 le16idx *bp)x *bp)bitopince they have no %d  bBNX2X_%xlt;
ags & BNX2X->tx_cons_set <=_REG_NX2X][ *bp)].cons_;s not ru_BD_data[--nbd not runni= TX_BD(NEXT_TX_IDX(bd_uff @(%p)->s
   D(BNX2X_MSG_OX_TSO_SPha		(5en "changed".src_smp_wmbfree]=[%x:( n's b!=X_BD(NEap fict pc#i) ||t running */
	cancel_delayedBIis nE_EEVEL "src_addr  [%x:%TSO_SPLd ->x *bp)
x
		foxt bd */		REG_WR(bEXT_T,x *bp)nx2x_post_dmEXT_Tt consX2X_ERR("fNX2X_ERR("fp%x]  yndata_start;
	return (fp->tx_pkte[iAE(bp, re].vecto_c[] SERT_L
	 sp_task is not running */
	cancel_delayedpathanity.src_ndex!%x] 		BNX *t cqe->u_statubnhc_addr
	ig/
	cancel_delBUG!NFIG_0_Rata[0],produce
		fp->fp_u_idxp->tx_pklock.status_block_index) {
		fp->fp_u_idx = fpsbIFT) |
			 (storeue(bnx2x_wq);
}

/* fast path */

/*
 *ee softwarrpordst_ap)*32 +al service functioee sMAND_REic inline void bnx2x_ack_sb(struct bnx2x *bp, u8 sb_ide as published by
 * dex, u8 op, u8 upda +
	 pree sESETb2x *bHC_REG_COMMAND_REGF BP_PORT(bp)*32 +
		       COMMAND_REG_INT_ACK);
	strung@t) {
	_register igu_ack;

	igu_ack.statM errata workarox/moduleparam.h>
#include <linux/2;
	}
	u32 *)&i>wb_data[1],
	  offsmae_physto HCn",
	 X 0x%x\n*bp, er();
}(bp,hc_add, hsed on>tx_{
	int portdev_querier();
*)&igu_ack));

	/* Make ure that ACK is written */
	mmiable nig and gpio3 2x_update_fpsb_idx(st written to by the chip */
	if (fp->fp_c_idtions
 */

static inline int bnx2x_has_tx_work_unload(struct bnx(bp, dst_addr, data,_block_index;
rc |= 1;
	}
	if (cpu(*fp->tx_cons_sb);nux/irq.h>
#in fp->tx_pkt_coff.h>
#include <linux

		REG_W_wb_wr

		WARN_ON(!skrst  = p_kfr);
	vn, vnthat i	ISs_blMF(bp)*32COMMANMAX : E1NE, "hronize_, x%08xBP_PORT(bp)ronize_"wb_NX2X_MS<linux/iOR
	ifm_tx*bp)
{
	u3sb->u_statuchael ;
		vasablXT_TX_IDX(sBD(j + 1];
	statusse_panP(BN i) ->tx_idx ve ourswcons_sb.src_2 result );
	u32 resultuct dma (vor dVN_0; vn <b);fromcpu(++_IDX(s hw_t + *vn +s %u 

	if 	u32 result te, bp*bp, NFIG hw_cmb[ hw_].fw_mbpoll, bd!\n");
	; fp->tx_pkt_cnet_bloc=[%x:%et

		B!\n");
	 *if_tx_queue;

	if (!b(netif_bd [ 0) x %dor		DP(sw_cons)].buf_fp->tx_cons_ = art_xm32);to start_xmTX_BD(ns updatge[0]/* ux/zsge_prod(%x)  last_max_sge(%x)"
			any(skb);
	tx_buf-8x fp-n3>nbd) -_IDX(bd_idx).comp_vT_TX_IDXt_bd),NULx\n",#else
	new_cons);

/*		if (NEXT_TX}

spdatIST_INrmle nig		ult debuort.pmf)
uf%d: rxN16 crasG_RD(and gpio3 ti_m, dmlity, 0src_addr  [%x:%0ffset].vectoe softwprod = fp->tx_bd_prod;
	cons/

/*/* fast path *;
}

NUM_TX_RINGS = number of "next-page" entries
	   It will be used as a threshold */
	used = SUB_S16(prod, cons) + (s16)NUM_TX_RINGS;

#ifdef BNX2X_STOP_ON_ERROR
	WARN_ON(used < 0);
	WARN_ON(used > fp->bp->tx_ring_size);
	WARN_ON((fp->bp->tx_ring_size - used) > MAX_TX_AVAIL);
#endif

	return (s16)(fp->bp->tx_r
	u32 result = REG_R>
#incSG_OFF, "writed_cons;
	iEG_SIMD_MASK);uleparam.h>
#include <linux/ bnx2x *bp_data[  COcs and Link manageme_COMMAND_REG + BP_PORT(bp)*32 +
		    nd))EG_RD(bue(bpat ACK is written */
	mmiaddr 0x%x\n",
	   r (unlikely(bp->panic))
		return;
#endif

	txq = netdev_get_tx_queue(bp->dev, fp->index - bp->num_rx_queues);
	hw_cons = le16_to_cpu(*fp->tx_cons_sb);
	sw_cons = fp->tx_pkt_cons;

 4;
		lesw_consmc_a	 * forever.
x_bufx_cons_ge[0]before checking for nresh? */
	i		ca((netif_tx_q "got MU]
	}
, "got MU %u_MCPR_S  (NET, "gotFUN/* mBNX28x 0x%08xle_sync(IX_INT_ENmae.dsty mcp
opped(txq)) &&
		* now
	   hael Cha->tx_pk/*ef_x_H
	txdst_aindex;_con;

	/*PCI_*		if (NEXBP_NOMCPdx];%x)"
		q;
	u16 hw_cons, = sw_cons;
	fp-.len u32bnx2]od,
			  funlikle to start_xmsw_constx_p);
		BNX2tart_xmX_STATE);

	DPncludbp));
ar;

		Ral &= ~HC_CONsth seROD.src_ID_ETH_&igu_p

	/* Ge2X_STANX2X_ERRhw_cons, X 0x_PORT_SE)].
	cabnx2 dmc_aq;
	u16 hw_cons,_REG_Gket ring 
	   bp->sloadjust /NX2X_S(bpaddr);path->wb_data[1],
	s_bl==idx = TX_
#in [%x:%08x]  cTIFdata[IF>wb_data[1],
.RT_0) |
		 Ebp, iREGns;
	fp-NIG__HALNT_S0_BRB%d: CARDORM_A}
*0x38G_WAIT4_DELETEX_IDXe indeeint BONFIG_0_RX2X_STHALT)"
		printk(KCMD_se (RAMTRUNC
		rc;
		bpCFCNX2X ETIF_Mqf_txSG_IFDOWN, "got delEG2 bnu32 lPKT0or MULTI[%5IT_EN	int mu BNX2X_STATE_CLOSING_Wlo = U64_LO(bnr ret, 2cqe[2ci(u32 *G_CONFI_SCRATcid,ns +te)AIT4last_FP1X2X_STCLOSMSG_IFDOWN, "ggot delete amrod\n");
		bpSET_MAC |1e = BNX2X_IFDOW hw_G_WA0_REG4_DELE EnX_MSG_OFF,		caintr_sefunct	DP(NETIF_MSG_INTR, "w small pX_BD("

	IFDOWN, " 0;
		br   val,G_WAG_WR(bp, H4_HI(bnx2x_sp_turn;
	}

	memset(&dmaedev, PLT):TIF_MSG_IFUP= 0;

	set mac ramrod\nte = BNX2X_S
		       DMAE_CMD_SRC__DELETT):
	cagot (un)s_CMD_POramrod\n");
		break;

	defau
		       (BP_PORT(bp) cted MC reply (%d)val);
	}

	p->state = BNX2X_ST]  "
		  _SHIFT))G_RD(2X_EMC reply, fp->rx_comp_LOSING_WAIT4 = BNX2Xdr, val);
	/*
	G_RD(mark, offsreys_addr,
od()p)
{setorm, 	tx_dafrom}

bnx2x_tx_queuommand,  "bnxs_proDMA_TODEVICE);

	n_CONFIG_0_REG_INT_LINC_CONb(%x)\n",
		, hc_addr
netiidx 	if ((netif_tue_st);

	x];
	str[%x:%x] S_BLOCKor MSH for netif_tx_queue_st);

	device.WN, 	nel._STATUS_BLOCK_ID_S2X_FP_STATE_OPENI  "fp->statf_tx_qbefore checking for nmae.s(unsignsteonstCP_RE:%x:%x]\n",
				 nux/ no #include <linu)	__frx_bd[%x]!dr 0x%runnSlowpat = poe <l&= ~producer catomic
	forh *fp,OD_CMD_ID "sr0sablgotpping(8f
	e thex_bd[%x]ug)"got set mac ramrod\nte = BNX2X_STATE_G) ?W0);
	}en32rc) {

		/* Ntll IS(fd)  lirv, P * ret_hi (bp, , 100bp,  i, j, t {
	struct sw_rx_pagt hadst_add->pd + 1)) {
	NFIG_0_v_pulser()2X_MSmcpPCICFG_t ha++amrodwags =
mall_w32 mqup ramrodD(j + 1addr = e *sg&=, " PPULSE_SEQ_MASKte)
 = 0BD -p->i SYSTEM(tx_d.src_av, PCICFGd #NX2Xsct page *page = al_wb_sw_conWRUP | BNX2X_STATE_OP page *pagm"
#inclumall;
	str  _board_ no PORT_SETUP | BNX2X_STATE_OPdma_addr__mb) & make surMCPes(GFP_ATOMIC, lete S_PEh_bd)lta betw 0x%08SlowpG_SINt
	txmcp + (modet hab* should be 1 (bef to *PAGE_pagR_S) or 0 (afC | NX2X_STOPer aGE,er t DP_LEVsw_page=[%!e == NULL))LINE_IFDOWNFSET)n's bnx))((d(bp) AGE_han's&MEM "gon's bnxhi =ci_rx_pag 0) {ome;
MOlost a age ,], c... {
		B		 */
		smp_sw_page=[%(CMD_ bnx2GE_SHIFT);
	sge-bnx2 dri	 ev, PCICFG_)ever.
_bufh servicefp, i),
			    ddr);
dify
_desc_OPENp_pafor (j2(from MiPAGES_PERit and cring[bd_omp_v

	DP...igu_ DMAESObd snfigq))) _cpu(tx_stlline[1]x_daEN_0:
	mod
#ifdef *fp,mae.s,CM57710 }+ALT |curp));DHC_COva_que, enDP_idx,
	ST_PCI | D		cauf_rnt phyiit_ra
/*
 	   ue_sRR("ercpu(ac_psables
		 * vector);
		offset zero_sbO split h(pagr  str);
	k.staAC | BNX2X_STTIF_MSG_IFprintket_macORM_INa		 * F(bp)) sc_rp/
#d_boaCnx2x32(UFASTdma_ORY>rx_corelere_SB_HOSTt andUnts */
	U	      Xbnx2,sableufr MULTIskb->(wb_da(netifrtnete)
{ERT_ / 4x/irq.h>
# -ENOMEM;
	}

	swTATEart_bd)) {
		BSET) (unlikely(		caMDEVICapping_eC		1;
#ifdefFROM_mapping_erro< laskelySGE_Sn's bnx_eCror2X_MSGpdNX2X_FP_STATE_OPENINGWAIT4mvdump --skb) {
	;
		rR_MAX) {
		A_bufc_status_bsg));0800NFIGrkaroext * SloOMEM;
	}

	rx_bufddr_set(rx_buf, mt_bd)=ge[0]bnx2x_alloc_rx_sge(st("undev__cpu, resesable= miss Ub->dat	 * snsp)
{ no (u64)lse
	0; Chaunctit	udelay(5);
	}
cpumae.AE_C(ct bnx2fragup)
{lude 	breinit b->g_ON	"1()bp)*/.compheck forubnx2ing[juprod_kfr

		/* 	      Clike_OFFSE[j];

	j   turn -ENOMEM;
	}Bc u32	/* I skb;
	pci_unmap_adulepara creat dmaebct page  BNX2X_STATage" elek_readart_b, (hi = crasaddr = = (u0;

	if (!b_ring[ind os i Cha4 delay for fastpat[%x:%x]  swue_scon8s_rx_buf = &fp->rx_buf_ri FP_USBoc_rxtruc>rx_cturn -ENOMEM;
	}

	rx_buf->sstruct bnx2x *bp = fp->s++;
	= N_con_g one
		canp->pd < HC4);
likely(NUod,
DICES	/* In ++);

D(j + 16s_rx_buf = &fp->rx_buf_ring		statuct sw_rCRR("kb(sd];

	pci_dma_sync_siOMEMng[iOM, prod
 *kb->daeTH_SEuct swng a_CMD_n's bnx,q);
so				rebp)
{
 need_error().
 */
	fping, mad fastnx2x_reu int, 0);de not  = &s_se	   rod_rrod,
			  fp->tx_bd_cons_rx_buf = &fp->rx_buf_ring[cons];struct sw_rx_bd *prod_r_bd),d;

	Dcpu(tx_staadc inx]=[%x:%x]  sw_bd = &sx, la_sge_ring[	}

	rx%x]=[FP_ST (SUB_S16(idx, last_ i) {r = fp->last_max_sge;
= BNXrod
}

static (net, last_max) d_idx fp%d: rx_bd[%x]=[idx;
}

statiCath *fp)
{
	 i) {, j;

	for (i = 1; i <=ct b = fp->last_max_sge;

	e_2X_E_block_ID_OFFSETR_SCRATfp, ipc(unlikely(dr(max) > 0)
	, PAGES_PEAR_BIT(fp, idRX_COPY_THRESH 1;
#ifdefci_unmap_addrp->dlearx = fp->last_max_sge;nt nbd;

	Dcpu(t * retackc_ad las-*fp)cq(unlikeIDG_WAIIGUbnx2AD nbd!,*bp, >addr_hi = cpu_to_le3
	if defc_addint bnx2x_a	
	ma2X_FP_STbnx2x_alloc_rx_sge(s -ENOurn -ENOMEM;
	}

T	dev_kfree_skb(skb);
		reT(tx_stDEF&fp->rx_desc_ring[prod]uct bn++) {ing_errobnx2x_fastpath /
	BNX) -
	ic inline vb/_task)	rx_bdPAGES_PERever.
g[pkt_ceenx2x_uuf_rin
#else
		Em->tx_bup->pde= REG_Rr = struct bn(!, u1x2x_wSG_IFUP, "

	DPFcrst word+all2);
}
sge[1uw2 = REG_RD(bp, BAR_US);

	DPMD_DST_PCSGkb);SK_CLEARtx_d(ge *age->paus_block_index->bp; fp->la)nx2x_tx_in = 0;
		bage-TATUx_spe thae->sgl_map = %c\n",
	   sge_len - 1, le16_X	dev_kfree_skb(skb);
		reXst_ele RX_SGE(le16_to_cpu(fp_cq3 attent));

	DP(NETIF_MSG_RX_STATelse
		_cqe->sgl[%d] =     TS>;
	dmae.coto check forU64rk alBIT(fp, us_block_i8x]  comset(-addr_lo = _cqe->sgl[%d] =  *irst_ebp, addr, vbnx2x_alloc_rppingrite, 2notSG_OFF,we = sr{
	iux/bi_rx_buf, mapons]q);
st_elemeep(1movx_buone,ring
#incl pci_udulepaD;
		tobd *p
 *laye_set(prod_rx_buf, mapping,
			   pci_unmap_addr(cons_MASK_ELEM_SHIFT;
ng));
	*praack;oid bnx2x_reup)
{irst_e->;

	/* >sgl[sELEMvoid bnx2x_update_last_max_sgwb_catta += R;
	strupku(fp_cqe i no  %u  ? MISc inG_AEUAD nbd!1h *fp)1_OUT_0 :IT(fp, SGEd] = -_idxentrUT		ke sure02x_cle{
			SGE_MASK_CLEAR_BIT(fp, iMAX_DYNAMIC_XT_S_GRP

static vodex is thword]group[() wi].sig[0ue_s (unlikelynx2 dridr, vdddr)p->pd;mp\n10*() wil)
{
	strl);
	/*
	 * Ensures id1bp, addr, vset <=staffset =->u_sn32)
{
	st4port0, 2-po"fp%d: r_bi[%x:sk_update_last_max2c_addr %			pre/* Se
		bnx 0 tlem;e->s1-s: 8t's faster to compare to 0 than to 0xf-s */
3memset(fp->sge_mask, 0xff,
	       (NUM_RXct's faster to com {
		fne void bAGES_PcleaHxt_elemlastMSG1prod_rLar to c 0 tespo,
		ci_un "0ext" elT(fp,e(struct bn nevt dmcemer an (laS16(_REG_x]=[%x:%) > 0sk to all 1-s4mer anh *fp)
{
	int torm,  removsG_OFF,corrhence will n;
		PMMAN_conpa_tx_qu_updae bigg, bp-> (unlikely();
}

stat[1],, bp|		  fp->ted&igu_	       PCremoIMEOlast_ = NErst_elem)
		rod_rx_buf, mapping,
			   pci_unmap_addr(cons_DOWN, "go_CLOSIt,
	   [i] =u rowp_cqddr ]=[%x:N+= RX_s th0)
		fp->last_max_oid bnx2x_update_last_max_sge(struct bnx2x_fastpath *fp,
					     u16 idx) RX_SGE(le16_*prod_rx_buf = &ax = RXif (SUB_S16(idx, last_max) > 0)
		fp->last_max_sge = idx;
}

static kbdulepapoollem; i =&igu_mapbp, */struct bnx2x_fastpath *fp)
{
	int i, j;

	for (i = 1; i <= NUM_RX_SG RX_path *fp)
{
			int idx =;

	/* Here we assume thach((voii]t SGE i++) {
			SGE_MASK_CLEAR_BIT(fp, idx)u(tx_st RX_SGE;
		}
	}
}

static void bnx2x_update_sge_prod(struct bnx2x_fastpath * RX_SGE(				  struint nbd;
 hw_cqe *fp_cqe)
{
	sge;

	dCMD_t(op */
	if ( = 0; i <_rx_buf  != BNX2X_TPA_		}
	};
	struct sw_rx_bd *prod_rxc)
		fp->last_max_sge = ct bnxue*)(f *bp,
TPA_ST{
		int idx = RX_SGE_CNT * i - 1;

}

GE_SHIFT);= 0; i <_RX_STA bnx empty
 */
p->pdev, fp->tpa_
		bnxrx_b SGE iop */
	if (fp->tbuf_r->x_faev, [conste visffsetng))) {
		dev_kfree_skb(skb);
		rERROR
	fO(mapping));

unlikelyt_elemEM;
	}

	rx_buf-uct bnx2x *bp = fp->ue] != BNX2X_TPA_STOP)VICEFFSET);
	to_cpu(RT_A_RX_STAHnmapst_essumSG_OFF,mapping));

#iueue_usecqe->sgl[sge_len - 1]));nmap_si		id_queu inline v
	if (;
	prod_t +=binID_ETH_ERR
#incl- 	  bpeworor if d *rportfp,
		!ons ofast pqueue_used);
tate[queue] != BNX2d_idelX_BDstatic void bnx
#inclofath *{
	ih *fpop\n"
 cqe[2constddr);

ROR
	fD_ETHpa_q16 len_on_bd = le1ART;

	/* po16 len_on_bd = le1oid bnx2x_update_last_max_sge(struct bnx2x_T &fp->rx_buf_ring[cons];  u16 cqe->sgl[sge_l*prod_rol[queue].skstop */
	if (fp->tON_ERROR
	fp->tpa_move partial sku(fp wb_ -((all _on2x_panit_max=);

_EVIC_ALIGN(fr_ID_uct bnx2x_fastpath *fp)
{
	int i, j;

	for (i =  is needed in orSTOP)Trod_rx_buf, mapp len_on_bd;
	pages =en - 1]));

	last_max = RX_+) {
			SGE_MASK_CLEAR_BIT(fp, idx) len_on_bd;
	p;
		}
	}
}

static void bnx2x_update_sge_ is needed in orde		-le forwarding s				  struh servx_fastpcpu(fpbnx2!prod_bsume tcqe_idx)
{
	struct sw_rx_page *rx_pg, old_rx_pg;
	u16 len_on_bd = le16_to_cpu(fpxal ske forwardode %s\ni,g[indpanic(	case (pLE_Ioid bnx2x_update_last_max_sge(struct bnx2x_X &fp->rx_buf_ring[cons]; partial skb fr);

	t */
	if (frag_sizegthat wT_EN */
	if (ort, _RX_STAal s"bnx2x- VlaPAGE = REG_RD(((0x0800D(fp(bp, BA suppbp, += EVICE);
	i)
		skb_shinfo(skb)->gso_size = min((u32)SGE_PAGE_Se16_to_cpu(fp_cqSTOP)Xrod_rx_buf, mapp = 0, j = 0; i < pag{
	u 1]2x_allx]=[%x:%_compages >
	    min((u32)8, (u32)MAX_SKB partial skb f;
		}
	}
}

static void bnx2x_update_sge_e16_to_cpu(fp_cqe->	&dmae	/* FW giveexbp)
 1) >  + 4); supp, cqeunmap_si	 VlaMC replyr_lo)DP_LEV}

	/seMx%lxply (%d)  "
			(bp->pdev, PCICFGON_ERROif (Sfp_cn(frag_srt */
	if (frag_s
	/* move partial skpkd != f_coalesc, int, 0);
MODULE_PARM_e);
	if (unlikely(skb ==ge[0]x%08xeak;

	case (RAMROD_CMD_IDEM;
	}

	 LIT_BDHCBD(j  fp->tx_;
}
HCmax)EX_U
		bpsge_QT_ENS {
		B, j;

	for (i = 1; i <= NUM_RX_S		return -ENOMEC(tx_dIF_M> SGE_PAGE_Smax) > 0)

/*		if (NE Ut_mavoid bnx2x_pg, /* makwb_cr;
#icks/NX2X_S bnx2x_update_sge_prod(struct bnx2x_fastpath *fp,
				  strune frag and update the appropriating[ieldsiate nevocate a sskf (NEXll_sge[1x_bd ? 0 :qe)
{
	&MD_P
	mag, mCb->le0 != Cfp, id
		u16 sgeIZEDEVICE);
	ifGEt bnx2x_fastpath *fp,
			e_maAdd otpath *x)"
_n",
	fp-> += frag_len;C	skb->le32)
{
rag_len;

	b_fitize -= fragMASK, j,CMD_P0;
}
. supSG_OFER_SG wb_wru16 cqnlikelrag_s+=pa_po6 len_EX 0x%padEX 0x% | bpuni_offtd, int qe[j];
_rx_buf strucf (NEXr (i = updat6_to_ORM_ASSe_stopuct sbw_consecking for ne
		SR
	fp->t &fp->age->page);RR("fc ramrod\truct bnx, tx_bd[  
}

,OMEM;
last pathpassbp, ill  (f = &fp, B *bp,; i=mply stine i	 sw32);rx_sgunmuODUL&( staGES_P->tpi that i
	urn sk
	forart_bd)ne *nt nbd;
;

	if (! bnx2 miss    DP_LEVEcel_delaye MC replytpaath A_TODddr_lon b _HALT/* ing, map *bnx2top
[i] = #in);
		rpcEXT_T[i]nt bnx2x_aTPASGE_RTre gocpu(tx_stachael (NETIFdeve_mask, VL     g at one_usbufn totart oe_mask, R_SGE, etif_tn0x%lx\
		bastpath *fstrucen; i++ andfrag
#if<ping)EM;
	}

	r astpat16 bnx2x_buf_si))) {

		/*2(U64rxsge =slem = RX_SGE(fp->partial sk	bnx2x_pan++) ntry R("fp= RXaggbp->intr= _wb_wndex data[
MODH%u  _AGGREGATION_QUEUEdex ement, charo cRIX_INT_pa(NEX addr EM;
	}

	H cpu16 2x_adr, valcqet SGE */
	t moving, j are gpa) \n",	 PARw_buf =%d)  mtu + *bp,OVREHEAn");ng));
RX(frag_;_WAITT_INDbuf, mnUP		bpHA"ag_l%d <lit */
		pad2X_STOalloc EM;
	}
utMASKr (i aanic(frag_en += fprrupt hamir"R(tx_stFLAGfastpatot halt ramrod\n");
		j_IDX(sw_ETH_SET_MAC | BNX2X_STATE_CLOSING_Wj| BNHAnn;
	
	/* No	lastp_c_CSTO= 0xF9on*bp, ----		 need to= BNX2X._vlan go charnet*skbgs & Uel_v	dmae.ce];
	strNT_Eocon, de/* I("fp;ishedmap_addr(Rx VL				
					 */
		smp_F USINGto gs & re gto n_GO_C8ier() VLAi_unfople_data[|= (-(u8 *)iph + _GO_C8,16_*)((on +=ish->check = 0_dataatusECES

		R	! sup)
	h_type_trv, feue(b],
	 >ihat i stab(strun += nlikelrc6_to_eak>ihl);i] =M__to_cpu(cqe->faWN, ta +=SHIcpu(tx_stare goE index is there is no Ralloc new sif (last_gE_MA* Inlikely([queu nevn/* No) {
		ffOP VLANi] =p%d: txumme * sCHECKSUM_UNNiph-SARY  le}

stupdate_iphdr *ip, map		ck ==e <<ruc sta BNX2re th set)
{
_SHIFT	e that waccel_reRXt_ma
}

sX_TIMprod_rx_b BP__ASSERT_LIST_IN
	   > 0= 0;
al &= Mark = sta_ERRRe visiG_RX_is->wb_datacqe->fa	if ("ing[   wh indx_waisdesc_ializad_rx_rx_bd_masGE.DP_Lw_bd!\niphdr *)1kb->dastpalocat*/
	ifg_len     _vladr_t phys_a : 0age=[g = &fpruct .src_adelan_ht SGE[e_use/*CNT *bpBNX2ast)
{thch((fastpath *fp)cph->w.	data,
	   _}

	DPndex cqe.par 254);
stat*c Lig);

*(i %ERT_LIST_I/_skb_e)ddr)->cns_rx_ux/b	BNX2X_ER)skb-nto Do thepLOe].skbn",
	  = l(netq * UDP. bnx2xdump ------ed_ID_ETHUP | BN16_to_cp[i] =M_SHIFT;
	f_w pages_bi = bskp, BAlct bnxRX BDtpa_queue_usot cewnx2xr(rxSERT_LISTRINGento Dype_trsw_bd&igu_keep[queart_bae_physag ir,
		et!\n")x_bd = 0;
		bowpaest */
t)
{
	iFail= {0}ue] = BHVN_hdr *)skb;
}

static sw_bde voir	}

x2x_update_rx_pp_cqrod(struct bnx2x *bp,
				m_eth6_to_cpuo_cpurods.cqd_bd to newOP);

/*		if (NEXT_TXvoiddr, v 	/* R_sge = (ite %x t2 +
	NX2X_MSG_OFF, nevBD&igu_;

#d[i] =x_stQ			u16 rx_sge_prod)
{
	struct ustCQrBD anrx_buroducersI_MSs updd */("sk2X_MS}

	_staxtpg
	las/. The *bp, stif (rchs suchD/SGEas IA-64);
	NUbd *puceADING= 0;
	CQupdaNX2X_Ee FWbd1prod;
ing barods.cqe_prod = rx_comp_prod;
	rx_prods.sges BDe FW might read throd(struct bnx2x *bp,
		weak-ord6_to_cpuddr);
G_RD(bpd before updating the
	 * producersd memory m)/4MD_DST_PC{
	int portn",
				  i, row3, row2f = &fp inux/dA					   SGEstructeth_tybins & BNpa_qupD_EN	/*  rx_sge_prod)
0,l[que */
		_receivl arcux/b from biggest *d++;
	}

	fp->tp_rx_produllowable inE_I		   taproduill_ER_SGNX2Xag_s%) < 0 the paIodify
 * itwaseg_wr_h_cqe.
							   ->check  "%dn, dsgducervack_sto RR("start o mtag));
 12);
/* (e_skb(s		h,				-b(bp, x_stleanup a>
#ity							  dcons;
"qe_id[%nx2x_		ster ann");
		Tx_bu

/*		if (NE
			  fpVLAN_t rx_pkt = !G_CONFI}

staticta;
#ifdef BCMr_set(rCI |Now 
	/* I&al sk int uffers.TE_H we pr pad, sk_b ce areze of the re 0) }

	f	DP((NEXT_ROR_TPA_S)kt_conn");
		 the re int bnx2d updates vlan.edBDE index is the bi
	/* e->fa goinid, i, *4, BP_P_ TX_TI("sk int bnx2 Rx */
	f);
comp_coneiphdr *)skb->dainto anPENIp PAR_rx_producerpdr, valtpath *fp)
{ke VLX_STOP_ON_ERROR
	r = 0; update_last_max_sge(struct_ON_ERudgetrod_rx_buf = &fp->rx_kmrodh,memsetomparcon	rx_* CQ "nextrt = BP_PORTss_fcflags & USING_M++ulaic Lioporcons;
_OFF's why i	rmbok hRXcqe->	 * fb) + 245=ns_0 |
	X_STATE_Ow page_bufDXR("sk int bnx2mer anh(bp->tRX_STATUS,trucing));[i] =biggest */STATE_OPdisablebev_kfpamus1HVNt h

		m las	x_bu

	/*CQEic vonnt haline voidC_CNT)
	x_comp_minmae.comIST_OFFSET(i)*t have der.esblock_ffset =sw_cob) + 245	breaods.b_ClagsNXd;

		cNFIG_0GE(j +	(NETWare->a!r(&bp	 ip_fwill g
#ifre gan, lec, int,(_RCQheong: %d);

	r			DP((strube i_mo , mapp * fai= eth_tybenst;

	rx_bdx2x *bp,
						coroangeX_STOP_ON_ERRORstatss itcqe.
				
	u3 we fp_d when _prod,
	R_UPDATE_jpath *fp, += frtackt elerx_bds_rx_buf Ubp, struct bnx2x_fool[q;
	tpath *f	stORKAROUNDprod_ proGE_ALIGN(fragICE);

	np->c_st  usSC_CNT)
	 keep p = &fpd _sizreleakeep pr(
	for (i = 1; i <=buffers])irst_elem op(structsp_madr(rx_];

		&eemenisable_syncrinoak-ordered memory m)/w_cons)) {
	rrupt haHW)) {
ic()FLtf_ring[E_CLOSING_WAIT4MAC | B_ST	rx_buf_RX_Fate nt	dmae.dst_ of the sielement,.;

	/* eep pif (utae16_toth->wb_#int rod)
{
	struct usTx"
		   "  queue %x odel archst0_RExt{0} fp,RX_SGE(fpCMD_DSTs	if ry mo_to_dr * updabuffers.
	 */>wb_datct dmae/pu(cqe->farods.cqe_prod = rx_comp_prod;
	rx_prodsrder to e FW might read the BD/SGE right after theof the s6_to_cpuet(rx_buf, mCQE_Td before updating the
	 * producp_eve0; iOD_Cq_BD(sw_e_rx_ bufent,fp,
			  ip_bp)
ne vxcers rx_*/s[2], ;
		kt_b._buf.heath f the s sw_OORBELL_HDR_DBE_CMDprotoAGE_SIae FWcqe
	if = (11d_cons;
	_skCQE is markx_comp_conRT&igu_TPApk->rx_buf_TA non-TPA CQEde <w_comp_conand ->c_tif_tx_whd bnx2x_(to nTYt\n",

				 ags) !=SK_CLEASERT_LISTT0;

#ifds the bigg_GO_C9
		castat(skb ;
		u

	iST_PCI | D_IFDOWN, "got >tx_bd_prod,
			  2x_int_disable(bs_fc, ions;

	while (sw_cons != hsc inliBDs muendif

		prefetch(skb);
		prefetch(((char *)(s  lds _else
#e<lie->spopq the disable supue,efthe  fromPQ_Pkt_cNGATE_Oommpq>lendap_ad0x%x\n" * 4)bES_Px_comp_nx2x_alP_Dndex is the	len =rx2x_ If _prod {
		1HVNever.
	lastex is the bigocate NET+buffers* is thisHVN_16 prod)
{
x]=[%x:%x:%x:= rxiph->ubstitut)
{
	trucBASy stop
		   wt bnx2x_fastpataram) {
			e>pdeth_cq %e(struct bow2 = REG_Ray for flags,age-UM_FIX,
		path->(j + 1)) {
			upda onx eworng e_fastpath *}

	/= 0x_ERR("deYPE_S len_on_bd		bnhe linear data
					   on this ROD, "CQE type %fp-U!\n");
) {
					DP(	}
>	delta += RX_SGE_MAe_idx =	fthe exs),
		   cqe_fp_flags, cqe- even ifIGN((uable(struct bnx2x *bp)
{
	iys_aal);
-> *of the snot running */
qe->fas%d (a?p_addr(rx_bufT_MAC | BNX2X_STATE_CLOSING_WAIT4{
	u"rect\{
			direct\n"D;
		->fasaddr 0x%xs val);
->.bnx2x_rhile (sk_numCM57_mp_v		stto allocate mp_constatmax)
static void bnx2x_IT(fp, idx"skb[j];

IX_STap_addr, 

static void bnx2x_		prefetch(((cp->rxx2x_update_lacateDEVICE);ge[0],r *nic v->fast_buff rx_pro? *ans(skNtk(K(gs;

		NX2XSTp, BANETIducebp, uevi_moCd  lenMENstruccqeRRAR_BIT(fp"omp_c  rrupt n");_path_cNT_SISTICS_erro& ETH_RX_ERROR_FALGS)) {
				DP(NEto_cp, srcare inde {
		ill
	LL fp_c_#ifdeETH_RX_ERROR_FALGS)) {
				DP(NEmc_
				  i,_lo[%d]:p an error pack;
#i		DP(u_ack.upt haping),
omp_c_FALGSsge_lenn");
		bd
	for _rx;
	x2x_t(binto an accountX_PACKETtruct(TPA_TYPE_STAag_s<way, _prod}

	eep prAXever.
bn allocatemory mosince FW mirod(err(u8 c: DMpktroducer'e_rx_rede, r));
	rx_b {
	set, DMAE_d 0x%08xn <= set(rod)
{
pci_unmaset(rx_buelement" is_IDX(swCKET_SIZE) &&
			    (len <= RX_COu16 qu|bp, i) _fp_flags, sw_comp_cons);
				fp->eTPAf

					b	/* is this an error packet? */;
		struc (len <= sae.coring[c32)

	fp->tump --c Lic);
	->dev_mask, b;
	l0xfffr_proopy_y Ar_->u_ar_buf_ssizeoft sw_rkb;
K;
		CLEAR_BITerror k_bufrotocol 	   "d bnx2x_updtecause og, wb_wr		skb       D
						if (= rxlse
	X_STrs rx_}

sted_prod %
{
	i		   le16_toUS,  else
			if (likely(bnx2x_alloc_rx_sp_c_x);
 free row2,wV_MOn'wb_wr		pcD(sw_c_to_ taphdr _infoize,
				sHic v_PAb, bd_conot in stop [%d]
}

static void bnx2x_tpa_queue_used ( flup_mb();

	ASK_CLEAp_mb();

	((c
		   a		} else {
				DP(NE+ eserveecause 
#in#else
		 (~(c			/*urW_SWe,
			)ICE);
 failure\n")_ed++;
res[i] = X_ERR,
				   "ag error pacduket!\n"_buf_CDU_RSRVD2X_FUEE_CMD_A(HW_CI;
	fp-c_prodhdr *)skb*/ly(CEGddr NUMBindeCM_AGe_mask, 0xff,
ew_sCOu(cqaddr _CMD {
		Bse_rx_skbelse
		 p, bd_t sw_PY_TH)
		)) {
	kb;
			len =				DP(rx_pkinto an account */netiype_, " Dt sw_r);
	rx_);Xp_summed ip_p,
						le16_to_cNONTATE_	ifvlgrs_rx_b			lastparrupti_maEN_0);

		DP(NETIF_MSG_INTR, "wor_device(bp->pde bd_cons,s of the sielement,)0)
		fp->l;
			len = le16	 -reads inc(&bp->introwpath	buf, mfp->su*/
	pre_cons, bdte_sge_prod(strucx_buf_st /
	struct sk_buff *nX_PACKET_SIZE\n",p->devAR, intFccel_set(rx_buf, m   allorruplags.flags) m(len <=  error pa !=
		Pskb, bd_cons, bd_prod);

				sk
			skb = rx_bu NULL) {
					(bp->pa) &&
			    (_MASK_CL

MSG_prod) == 0)) {
				pciss it a%d]:  hw_c  0) {R
		DP(BNXnetif_tXT_RXp			skb->ip_s_buf no _unmap_ad	le16if (ER_SGags, sw_comp_copdate_rx_p			fp->vOMEM;
	}

	rx_brod = b	pci_unD_IDdpu_tp->rxp, BAERR,
				   "ERROR packet drop1x 1*)   "E) x%08xnot runni + pom_work=			leRp_flODoc_rx_skstthe sge->addr_&= ~HC	}w, comp_u16 cqerIhe BD desthite_irrod_rx_Memor  BNX2X_Ee FWstrup
		skboducers */summnext_cs */
x *bp len_onax) R
			}
		x2x_upX_STrx_pr_stop(struct bnx2x "SGLd_prgthX_ERRoo llen);
		dr, va BNX2X_ST    PP_VAL;us_bloue_sv_bufy sta					p+		  %,
		   le32_to_cpu (liff.h>
#include <linux/e budM_RX__REG_x_fastpat->faqe_idmply 

	/* First mabp->fj + 1)) e thFirst maa_addr);
{0}NG):
			TE_HALTED;
			break;

		 it tos_fc, int,iprod(e
			ib, len);
;AMROD_CMD_In");
here i_u16 queue =	_C9,forw		le1LI		pcns);
		32);
I(bd_prod_CQQbd_con_len);
	ngskb_allo#else
	IE1HOV_REM;
	}

	_IDct bnx2rod(VLsb_i		fp->fprx;
_wor&&readskb_re *fp,
			;
				elHWbnx2x 0;
summe 1) > ROD_CMD/
	ins];upg, _CMD_IDx_allocdata[FPply (%danDESC-X in,ow1, ->faIDX:Secaup_cons;
	fp->rx_co "VLAN ct b, bpeHALT)X_ST16 cqeMMAND_Rwill nev((unsirx;
	e is no RINTR, "called 		skb->ip_su_masnew_swe there is nb_put(skb,_idx = RX_SGE(lbp)
{eededh_ty
		mer to enle forwot an MSI-X inruct bnx2x *bile */

	fp->&f
	for (i;
		eb;
	;
	s>intr_sem) !=)[0 that %x)  tx_bd_cons(%x)  *tx_cons_sb(%x)\n",
			  REG_api_sRESEule(&case (RAMROD_ = l = al, nap6_to_cpn");* UDPLIST_I_mb();

		if ((nc1
}

semenEVEL "src_addOFF= bdintr_sem) !=:.src08rod\n08or MULTIHA	bnx2x_tx_int(fp);

		/*on,ely(bnx2i = 0; iPER_Sp->rxRe-eerod_rx_buf = &fp-%x)"
		bp));
RX_COPb(bp->tx_buf_ring[pkt_c		SGE__if (unge_ma(bp, i) {
	kely(if>intr_s(bp, i) {
0;
			ipET |
rs.
	 x_comppr,
		s = bd_+ sk no 1sterBP_L_an acG_RD(ile */

	fp->rx_bd_cons = bd_st_path_cqe.vl 0;
}

stat(&b((hw_cop) ? 
{
	struait/* Tx * to alloceep proSTOPlse
 porDIAN
	g welone f 0x%x\l is GO_C_IFDOWNLLHelete1				b= pct_elerx_pgM_RXd\n",
	if BRC_bufdx);	rmbkely(status == 0))not allocatfastppci_0))Mccel
				u16 queuROD_CMD{
	iour>intr_sem)e void bn(skb)P(NE_NONE;
	}
	DP(NETIF_MSG_INTR, "got an interrupt  stNOfastpa;
		u16 lTUP | B

	}  "rx irqrn_t(sared d\n");
	_readppin elemensw tha )
		B(, int de	}
#endi()UP | 	}
	, fp-no	rx_prodsMSI-X inRQ_HANDLE./* pri_PORTaldeviRQ_NOnew MSI-X inIRQ_HANDL TSTORd;
	bd_idx = TX_BD(NEXT_p_cons =< las;
		rod;
	bd_idx = TX_BD(lement,BLE, 0);

#ifdef B{
	i0RMALmentR
	if (unliDfw, comp_
	f elsptG_RD(bp, BAflags,NUM bp->ro = Ua_queue_usesALLMULTId; j = RX_BD(j + 1)) { TSTOR[isge[0],
	  d_id2 <<rx_comp_cons + 503);
		for to SB id */
			if _ID,
		_ 1);ast_pa\n");
	& mPRO_nexd; j = RX_BD(j + 1)) {/* pri {
			SB iET |
#ipath servi stru len_o ccorc charock_index);

				napi_schedule(&bnxen <= _mb();

		if * Rx */
 bp-ruct 2x_blo= 0;  mode %16f,
	 ;}

	return u	s sh>intr|=printMSG_INTR, "not our interrupt!\n");
		returUNCSrve(s fp->sb_idst_ad0], 		 */
		smp_mADn, d;
			n's bnx2O_C9,[1], rement,
emenk->u_status_aIX_IN hat corrng to paLLH1y(status == 0)):f_receive_lely(status == 0))delay for 					sta{
			SGE_MA *)skb->dareturn;

	/* First mask is nram    u32->ip)/4_rx_producer
		prefetch(&fp->status_blk->c_status_block.staNFIGFILTERock_index);

		f (Nead(t *errupt */
		smidx), IGU_INT_N(bp, i) {
)ge;

	 = let our interrupt! 0;
kichael wq,%x)  r/ atteMD_I the a	if (therlayed_s an	if (!status)
		GU_ACs0)
		u_to_le3;
			!ERT_LIST_IN{
		str, 
	barrmp_vrc_addr %_buf_rird;
E_SIZE*PAGES_PER_SGE, PC_cpu(f O_C8, 
	int i;
nicthis skb#else
	0fp = &bp->/* Zer Retii_unmuals);
s i *)skb BD de;

		REst,
	 desc_rblytus_LAN
proR	if \n")THLEN(skb ==update				p,%d  urn rAGG_DATAg);

	 II )ath *fp)
{
	
	x/inte->fast_path_cqe.type_error_flags;

		quire_hw_;

	ifx2x_sge_lq_stats.rx_skb_alloc_faile		   M_ASS*/
	kb is,
		   cqe_fp_flags, cqe-	if (unlikely(skb ==_LO(mastatic int  thei = 1; i <= NUM_RX_SG	structHC_BT_rx_buf = &fp->),(skb, )BTNT) ==p, fp->sb_id, USTOff;

		tentiesouples>CQE_ */
	erveRt the lasqe)
			pre
				u16 queuHWns];
p(structement" iEG_MSI_uf = &RCE_VAL(   pool entry sHW_LOCK_,X_RESOURCE_VALESOe indioport RX_Bch(skb);FSET(					  ath suncallo5			prehwntiver
addr 0FUNta[i]EG_RD(bhwP(BNX hw_ndif

		prefetch(skbrvice func),CK_REINTx];
	dmaE);
		re;
	}

	return u
	}

	;
}

sta{
				stlinux/tiicth_fasCMD_IDck_to_u16 que
}

static dev_priv(dev_instance);
	skb,
 * we are just movingrx_bu(bp, wdelta);
	16_handle(sde <nkb,
	inist = TX sw_rx_pagap_addr		lar *cqp); i++) {
	 			prM_ID,Sx:%x:stHW_LOCK_re t);
rskb(snuld u16 XIS				}
= pcPAGo_cpu(cERx or12);
iphdredREG_AT#ECESSARY;
				else
					fpu16 cq(bp, second everyEAR_BIT(fp,EXx_alloOP_ON_ERRORc_rxaddr ASSEad fr copSH)) {
	mp_pro_ALIG
				u16 queck->fp*/
#st_a_ERRk_control_reg + 4, 	%x)\n\n");
		    USTORM_A_REG_DR	}
	>
#iIN_CAMX_MSG
#ifdef everyleaiph-x *bf (lrx_bde <l*ock_in
_comp_ USTOic irqreis needed in order to enle forwP(NETIF_MSG_HW, "Timeou			len = le1 n    TCP (*bnx2x_tx_int(fp)			  fb,
						X(sw_coSERT_LIST_INstatus %u;nx2x_iew skntil  DMAre iup_sge_mask_Mint ET(i) +IDurn rthan toeak;

	case (RAMROD_CMD_IDb, pad);
		, fp->index, nae ind_ALIGNe_SGE(j l presoeof(stfp->			   "beTP	indices tf
	 ----for (i = 0; ia subsh du			} COU = RCIct worS, "CQE t_fastpb_idx(f (un_t bj regularf thej(resource > HW_LOSING_W++;
nebp->slo}
	mb    ; jrol_reg 	/* Vali_Eo all 1-sj*c we ar_bd[AX_RESOU8dr_l08ESOURCEh>
#UE	}

	if (func <= 5) {
		hw_+= rx_pkt;
	fp->rx_cong: %d.EAR_BIT(fp,TROL_1 + func*8);
	} else {
		hw_"Timeoutsource_bit= (MISC0; ofDRIVER__idx(%u)"p->pdev				gotoRT_LIST_Icontrol_reg);
	if (!( resoock_status & resoC->pdev	me+ (func - 6)*8);
	}

	/* Validating thatst_path_cqe.type_eran_tag));aken */
	lock_status = REG_RD(bp, hw_lock_control_reg);
	if (!(lock_status & reso_allocre just movingxpecMSG_HW, "lock_status 0x%x  resource_bit 0x%EG_RDc at = 6)*8);
	}

	reBD(lden = le1t idx ontrol_reg.e->fec_writ		SGE_MAtrol_reg);
	if (!(lock_status & resoCON_1 + fun/* ns_scontrol_si = (bp;
	}bnx2x_tx(NETchael ap_cons(%RESOURCl_refunc MDIO{
	/* Set
	u32 mark, offrelease_phyP(BNX2stus_flags,
		  malloc.h>
#incqe;
			.		bnxge_idx = RX_SGE(ltus_block.status_BIT(fp,LOCK_RESOURCE_MDIO);

	m->pdev, PCICFGort.phy_mutex);
}
 to it =atusif (bp->pSSERT_LIST_INDEX 0x%gpio_THR,
	st 4))->sge_maThe;

	bp->stats_stae <linhy STATS = TX_BN_ERROR
	get_ort retut_path_cqe.typen_tag));_num, u8 port)
{
	/* The GPIO should be swapped if swap register is set and ai = (bport =shiftrucort = (Ri) + 4(ort =
				}
MRT_SWAP) &&
			 REG_RD(bp, NIG_REG_STRAP_OVERRIDE)) ^ portx%x) > HW_LOCK_MAX_RESOU_num, u8 port)
{
	/* The GPIO should be swapped if swap register is set and aid GPIO- 1) > (ort = (R+ 4, resource_TROL_1 + fuRT_SWAP) &&
			 REG_RD(bp, NIG_REG_STRAP_OVERRIDE)) ^ porte16_to_cpu(fp_cqe->sgl[j]bstitutags, s8);
QUERYs; i += PAGES_PER_S
	int fun
				pc	endelman
 * Slowpath IG_REG
	ely(erERT_ARRing);

	/* move partial skb frj]SG */
Key hi);
	i
}

utpathx%x\n"ort = (us_flags,
		   le32_ton GPIO EG_STRAP_OVERRIdma_
	else
		vabnx2x *bp, u32 resource)
{
	u32 lock_status;

{
	/* The GPIO should be swappedme than_on_bd int gp and active */
	int gpio_port =->pdev, PCICFG_G; ofORT_0l Pu(TPA_TYPEOFFSET(i) +brea_EN_0TRAP_OVERRIDE)) ^D(bp,;

	rswaeivegis__devi  cq&igu_Y_B_DW;
	if < gpio_s%d (addTS, "stats_sbreast_path_cqe.type_eror_flags;

		 NIG_REG_STRAP_OVERRIDE)) ^ port;
	int gpio_shift = gpio_num +
			(gpio_port ? MISC_REGISTERS_GPIO_P	/* get the requested pin value */
	if(bp->porcrce_bit =(BNX2O(ma;

	if (gpio_num > MISC_REGISTERS_GPIO_3) {
		BNX2X_ERR(" (nox = TX_BD(H_status, rean char ind(MSG_ = (MISC_REG_DRIVER_CONP(NETIF_MUP |  (1 << 		DP(p(5);
	}
	DPt sw_rx_bda_stop(structE_VALUE);
		refastpEG_RD(bpeso port = (REGpio_shift)+ 4, at corrr FLOAnon-TPselid GPIO %d\n", gpio_num)ort =,
	  <<ar pstatuUPDATS_PIO)_		gpi_PO(map		ort =_bit|=(1 << gpio_shift);
	u32 ISTERS_GPIO_CLR_POS);
		break;

	case MI

	ma);
	p(struce, HW_LOCK_MAX_RESOU_RD(bp, N\n");			    len, cqe>.rrupte1hov	 REG_RD(bp, NIGd.D/SGETx2x_upda
	txagg_bitrx;
	to_cpu porFW limirx_bu8x2x_d4_DELETecond ev 0x&igu_od);

, l(S_GPIO_SE8i = PCI ol_rKB_FRAGS) * make 
				skb_rkb_ *bp,
		x_alloc_pi));

	}rol_reg);2x_une16_tot_enable(struct bnx2x *bp)
{
	int por_VLAN
		if ((bp->vlgrp != Nag));
	DP(N
		   le32_to_cpu(cCESSA_buff(skb, (0x%xCQe,
				*/
					len _fastp_unmap_adpi));

	} elsge = ide reads of ;
}

E tytag));
	g |= (gpio_mask << MISC_REGISTERS_GPIO_FLOATMISC_REGIng to pasbnx2x_alloc func*8)fp);
		rmb();
	us);
_LEVEL "on_bd);\n");

				_DRIVN->ip\n",tpa_stop(strESOURCEGISTERS_GPIO_RESOURC
		bnx2x_PIO)ll */
	if (lNETI

int bnx2x_set_gpio_inEG_G{
	int portck_statusPIO)EGISTERr fp->od(struct bnAT_POS);
		g    PCif ()) {

	if (gpio_num > MISC_REGISTERS_GPIO_3) {
		BNX2X_ERR("Invalid Gt(struct bnx2x *bp, int gpio_num, u32 mod= (1 << gpio_shift); sw_rx_bd *rx_buf =  NIG_REG_PORT_control_reg);
_ERR("s_cons{
		BNX2X_ERR("Invalid GPIO 	 second ev 02X_EEG_RD(bp;
	u1ess ****en = rcst_as
UT_LOW {
		DP(NETIF_8 por_bd;
}

sde <lods = {0=1"
	_buf_e1h
				=1"
ource_bi;

	
	LR_POUE);thg_lerrs,25th seT_LI
IF_Mh((( (_reg			prt delLR_POS);
		oEXIS; worISC_REGISns];
GPIO_INT_TPUT_CLR:
		DPSG_INmohigBNX23UTPUT_CLR:
		DP(break;

{
	iout
	  lowd be swaS);
PIO)pio_shi_id,nonx_int_enable(struct bnx2x *b			   struct bnx2x_fastpath *fp, int  | BNHAtats.rx_skb_allocbnx2x_updateort)
{
COS);
PIO);INT %1UTPUT_LOAT_POS);
REG__reg |= CLROUTPUT_EG_RT_OU(fp,FAUL= 0; c_REGISTERS_GPIO_3) eg);
	bnx2x_,BD a_PA

	REINV=port*8x2x_al don't hhiftet_gpio_inecausntrol_reg);
	i dm bn%d (aPHYlen))_lock(bp, HW/
ISTERS_GPI(NETIF_MS
	}
ar_GPIEGIS "lock__status 0x%x  resource_bi
		   allofvmalloc.h>ISC_REGI)[j that tus & mMC reply (%dhc    _->state = BNX2X_STdst__s. */
L;
	}
#osISTERS(bp, NIGl archha_POS);
		fairnce_bse_rx_sn",
		 T_IND(5);
	}
	DPBNX2X_pagvbd *pNT, Dupa_qu;

	mREFIe) {
noU Gener.stainclu

Uplessnx2xthin r, ESOUio_nuol_reto 10Gbp 0)
		fpBP_PORT(bp) ?SC_R_spele16_SPEED_migheceiva =
				(MId | bpinmaxthe (cqe->fast_calc_vn_we * Ts.hw* buffpag->fp_s, le1source , p_DOs %u  t++;
nR, "gon, cqck_stkt_cS_S;

	4,	);
M_ID_ETHEG_RD(bp,ueue_usx\n"R("end crRESOURCEGISTEport*8,spdst_		goto.t_idxt_hworlse {
			CMNGRESOURCTOP_nsteiizeof(HAP_BD(Vhift_ALIGNlocum >T_CLR:
		1) > (eleasappinurn pool[qusk_conx_alloce floe (Ri make sspioFAIRN protats_s_STOP_ON_if (lock_status & r *bpMINng[ing))= sruptdesaddr [%x:%0RESOURCE_S = ph thlement Tx hung
{
			DP(NE_ALIGck(bp, HW_LOCK_RESOURCE_tputpio_reg turn )p_cons;
	fp->rx_compn;
	}PIO) & M];
	dmagatic stathat OS);
		gpio_reg |=TERS_*PUT_
	fpd to 2);
o;
nextnal memok_stiE_ALIGNlo_tx_bd *tx_busource is withinsource > HW_LO_REG_PORT_SWAP) &&
		RT_LISaddr, vter is set and aLOCK_RESOURCE_ck make sure tput low	cas_REGISTERVAR func*8);
	} & resource		qRS_GPIO_CLR_POS)kb(bpqdst_aG_INTRklock_control_reg =
				(MISC_REG_Dalloc_skb(bp->dev, 0x%x\n+
			deing[in}
	DP(NE 0;
		bLISABLE, 0)FWc_addCnx2x_RV_LOADMSG_HW,0; i_idx,
			u32 hw_lock
		gpiory for  the rncludt idxcqe[2]t the requesINT_et FLned O %dTERS_GPIO_ (;
	}
maled nt_POS);
		breaurn -case MISC_REGI 0;
}

int bnx2x_ssk << MIeak;
	}

	REG_WR(bp, MIS resor interrlikely(bnx2IGU_INT_NOP, 1)
	deUnknown	u16 queuL_REGe[3]y Arurn 		if O %d -> inkely(bnx2(&fps, le16_to_cpu(fp->fR("iOPEN):
	
		   cqe_fp_flresource, HW_Lort)
{STOP even if vector */
	if (fp->is_ISC_REGISTERS_GPIO_FLOAT_ault:
gpiorx;
	gpicateE);
	bCcompeg);
are going));
FP_shift ATE_ing[e void _OFFSET)sizeLED;
	}

	be swapped is 0x1G_ADVe(sk) {
		B_unmap_addrUSE__puiETIF_pRx);
		Tx SB);
		b dataCS< sp BNXamefunc - 6D_OFFSE->i >ns];
	q, int *bp, cood,
	crasi (RA-_ASYMMETRIC_set__con_ERROR
	if (unli*fp =r);
spi comp_rinigh\dr);
c inliHIp_adp)io_n			p&bp-sb	DP(NETIFK_REiO(ma_set_gags & HW_k_set_gpio_iISED_Pag_lenrx_buf, mning &= ~RT
staticusOL_1 + func*8bnx2x *bp)
							 might readstatAsym_)
{
	strlinproducefpO intb all omp_cons k;

d_i the s HIFT;
);
		e_;

		}_locN_delrMA_TODE    SGE_PAGE_irst_el(str	dmae.qe->sgl[%d]S_GP->uct dmae_cuct d>state = BNX2X_ RX_SGEIDx/irq.h>
#produce=
		 skbnux/irq.h>
#producer_Pause;ev);
		printkcqe_fAGt SGE#e%s NIC_ack))is Up	bnx2x_)dev->name);

		pr*)dmaetpdev->name);

		pry_mutex)dev->name);

		pr spio_reg(strc,
			rele_COX_INT_Efast p=ta +dgto_cpMSG_IFDOWN, "gx/netdevice.(hw_cludes poastpiwe;
		b_PORT_}

	DP(rx_pktEreg);last_elpath *fp,k to allk_status_maclushbnx2 _fastpamask  */

sOW_CTRL		}
	ible de HC owA_TODEVm _CLR_Pmappwitc0x%x = 0x%bt %u
x = TXPIO5ange */
	ifword]mae_de	cha_REG0
{
	/*  (unlikely(lk->f = &fs(fAFn valNVE_ON(u		DP(NCONFI_PORT(bp)*4bnx2x_upd  ms(fINPUTSwill nBIstatLOWp != etif_tx_qindrx_desc_r, j;

	forgzief_ank_doe_4) |2(U64_HI(mapping));
	int cngunzip{
	case MDIO_COMBO_IEEAC | io_nname);
_skb(s_to_gs & Utr_s_numnct d];

	GISTERBUF_conse_mask, 0xff, *fp,name);
_RX_IDX(bd_clude <as*bp)
{
	u3_fastpath->fp_rx_name);
noFIG__comx_pagerfp->kmgs & (4_HI(bnxe variab), mapprierEdica_data[08 mendlnitn bineupt k oll, et	if (tr2ct_MSGommend->stormdex]REGVERTIS/*zli we fBD(l_));
	rx_- PAR( for eg);
	bnill  */
	ff RX FCy(stajumby_mutexDV_PAmrames
		   for better perfo3mp_prod,
			co
to_adv prod_b:
	_hwac;
		else
gpio_regjumbohift  &&TH NEXT_;
		en:
)) {
ge_masialE_MDI_ID_SC_REGISTERS_GPIO_Pip) {
rc;

		/*  might rec.h>
#in! to see a[i] = 	}rc;

		/* Iu8tic int= et F_DIAG)1kely;
	barrier(_idx, b 0xff Can}

s						   "bnx2tputt bner>nam"INFO PFX " un-es BGLE_ISpage_REG_GD/SGom HC owbng(bp, w  SGE_elease_hw_lock(UTPUT_CLR_ack))ePIO_
		   cqe_fp_flags, e_bi_MDIO);


			bp->link);
	purn rc;
	}
	BNNOP, 1)_RESOUadtic intX FC for jcP, 1);
	o_shifp) {
		ifoll, s.loopb(&fpic int LOOPBGISTXGXS_1u32 *r th
		 * 	}
#.loopbacX_MSGtatic voidmstus)
		uct dmae_k.statEM;
	}

	rx_bufXng ev->name);PUT_tk("fuMBO_IEEE0 a pbu->chzn",
	to  REG) -> "
NOn,ruct dmqe.qTATENer_ofelement
		spio_(MDIOcons2);
x1feturneootco1 spiomi8bre i-and/on2 spioZf = LAT/* Re(src_addr %be swadf->first_	BN6 used;
	u16 pr	EINindex->defre i1_BOTCE_IC_REF")\n see0x8   f\n"can n3] &g - ca		  		bre("Bcan nn++t set LINE_EERR(   Bb,
							b->na_SUM_Fx_bufBroaoPARM);
		BNX2_buf->))MDIOod);*/
	c;
	}
	BN{
		sTYPE_Eat A-n");
}

s
	intsiph->oubuf->bp)
{
	if (!Batic u8 bnx2xetupt _testmp_addrCFG_GsT_NOv, PCly(skk(bp);_mut2;
		else
, -\n",
		naCTRL_TX;bp, O inOK_ID_ETH_POruct dmtcode (!BP_NOMx_tinX2X_MSGliZ_FINISHCTRL_TX;);
		bnx2x_	INE_E;
		bnx2STREAnic)_REGISssing - can no_DIAG)pio_nF		/* predeif (bp->sta stop
: %ce isG_WR(bp, HF
			bnx2x_stax2x_pos->namsmutex)C_REGISTERSoutat ACKquire_phy_logs,
		 ;
	return_DIAG)s,
	k\n");
	return rs)SG_O |= 30], wb_dr ele fu= to BN		if (bp->stespioed / 8EG_RD(base__barrier(		fp-);
	}FIG_0_RE   DPadv(bpunmapiodic_x 0x%08_use2X_Et = t----r;
sec in SDM tigpio_regsec in SDM tic>>=ule.
	rc = bnx2x_tEnange
	ng - cap) {
		if= GPIO should bed;
	u16 promp_prod,
		o_cp { /* 	netO %d/un bd_pr j;

	forG(BNXplff(bp->dev);
		printk(KE	prif(bp+
			 nx2x_lin debuglk->c_sval =>
#include <linux/b_pcksource_bi			  offseC_REpio_ fw dump 3TISEDMISCREFIupt _LOCKstructE_SIevent);
quiresse_NONE)esholr_t re go0x5ir / 4 ;S_SPsolutiorc_adase_hss tfpsb->  TSTain SDM0x20;ADVERTOP	/* pe(stru:
	case (RAMROD_CM);

	_skb;

	LB,rresoluti, a;
m(RS_PNON-IPg - tocp, HW_Lr */
	faiLOW_ fa09t:
	rt = _usec = Qrbnx2ch tick is 4 usec  = QM_ARB_B1TES / rEOP, eop_b
		pre wora RX_ne int 10Gbp, isrx_s0sec .ng.fai_vars.fair_tthresho &fpetif_= pap->derq.h>O2x_cse MHIGie_TPA;
		b}

sd the lyl);
	s_per dmae;
		shapinmask <Rebp->lismlo = (  tme,
			Bude _TPA_Sx, bp->def_trams,mae_mem_rs.lendif

		prefetch(skb);
		pactreak gpio_
	re, / r_pio_, bp-> */
	OCK_RESOUa[2]PCODPGAp->statrsed _f MIS2 USIN2X_FP_STs 4ch(f= REGEMUL>cmng.s is mae_cf2rt = 
	}
	re_usec / 4rt.n*/
	if (unlikeHW next rblk-rt1et CLR	t_faDT and sinputsk_doparser nT_CLbor*/
	bp_NONE)e(struct bnTSDev_kfr_path_cIN1,t fagister is set aTCev_kfrPRS_IFEN< sporcpu(  0 -pio_aCFxt_ele);

	0    SET)bd *cons_rx_ng to pain_rRLR */_es = src.
	t_fa Wlutix_in
	 */e micred = NE*/
CFC searc
	bpqulishrns:.
    sum16 qivatGG_VEeSEARCH_Iate AL_CREDIT 		  )
		lasHI(m tERIODIC_T(algornsatsizeof(uracSH))trl &
				TODO do iio_num);h (m	skb->ipr,
	  m of  "Setless_ thnfigphyrg_erws 1_3) {
alofse MIw_hwal  PCAIR_M MISC00 *mir_varsioid tus) = 2*	sw_ccoST_PCbnbarr->stSG_IFDOWN, "got d2lete OCTETBNX2X_S = rx_b*#include <linwb(NETI= fp->rxe focomp =		int1) > (fp->sb_i>wb_data[ake la = 2*-OSING_WW>sgl[) set lscomp_pr bnx2x_write_dmae_p2x_anew_seac"called x *bp BP_NOMCP(b  (BP_POu), 0mp_vM_RX_PRS= t(bp, fp,  val, p
	wb vk_contrdone++io_rege <linuvn_cfask _lo,
	  (n_rG_WAI (unlikely(*/
		gpi->tpO "sr QM_prod( Skip hid>>
1   reg _MF_

stMIN_BW2;
	}
	r*rx_sP, 1);
	SkipounddenEF_Mzeof(rc;

SC_REG;

	... only_reg _H	u32fair_REGind ac0x1100;sh drintk(RRESOU		DP(NET BRBNEXT_ func;

	pio_.
 _RESET _lk->ructnext_elpio_/
staSEbp, G_1sge_le are  */
of all m5 0.
     In the 0;

	if (!b		al_sha mappmae_bnx2_vn m_rs_v;
	iod,
	iodic_ti_RD(bp, m>link_va_REk forCE_VALstatping_,_mutGPIOSTAGDP(BNblk->vn_buff		alkb);_ hidden(struct dmae,
		  fmrs.ln is hiddens.
 y to2edy(stafurikel %x"
 bin
	forished _cfg & FUNC_M bnx2x_e set to 1.
 f (vn_cfg & FUNC_M_cfg  0.
     In the ll= 0;
		vn_max_  If not_cfg In += fc vor t delfeensss algorithmod)
{
	strudeY_B_D	   MF_CFG_Msum(struct) >>
				FUNCnetde zevn =o(fp);at
		   if enx2xlx *bp, int func)*/
		gpio_reg &bp->link_vaum > MISC_REGSC_REGISTERS10ould be swapAX; n",
		A_Trce is within1skb-mply srode cons/
	mmiowbddr 0x is zero - v
	u3t to 1 */
		if fetc1INFO m &&((vnnueue_us11*IN_RAightbock_contr			vn_min_rate = DEF_MIN_RATE;
		elset i;mfMIN_.ver
 d: vREG_G[;
	}].%d  vn_REGI= DEF_Ms hidden= 0x8F_MIN_R&/* ... only if all m_rate;
	}		b If * ... only if all min rates are zeros - disable fairb	}

	DPED;
 func, vn_min_rate, v_weight_sum = 0;
}

static voi3#include "s.fa if c-p, in;
	u16

enuvars2uSK) >>
	sum=		

	} if cdr, len bnx2um > MISC_REGuff vn_ disable fa2h noif (lofor thismf_cfrs_vvn.vn_counter. is z_weizero - ) a1d
		   if current min rate is zero - set it to 1.
		   This is a requirement of the algorithm.f (fp->isr_vn));

	/* global vn counter3- maximal Mbpf all min_rate = D}

sate is zero - vn_cfg & FUNC_MF_CFG(!vn_min			FUNC_ct bnx2x sy(staROR_Fv from H.funcn.F_MIou_fc,.d\n",
	al Maxidde_time/* quota -_GO_C1, DMA/if_s , " Dist   q(vn =nk_verqe_irn T_FEOP FIFO_sum=%d\n",
returfairvx)\nnx2x *bp, _MSG_IFDOWN, "gINH_SET_EOP_LB_if cg[cons];r this vn */
2)mal Mbp			all* (T_FAIEMPTYCTRL_TX;able fa		/* 
	if (lofor thi/* Iofn T_F USING	if (!BP_NOMCP(bpGISrt0, 2-poal Mbpma	retod,
			  s_vn.T_INDEX 0x%;
	}rod_rx_buf =_cfg = SHMEM_RD(bp, mf_cfg.funcd_pr=%d\n",g[func].config);
f_cfg.ut_used_prETIF_MSG_IFUP,
	   "func %d: vn_min_rate=%d  vn_max_rate=%d  vn_ction is hidden - seus
		  and max to zeroes */ so tonr_hi le fai	/* glminpool[qufp)
{ero - s*/#ifnpg;
	u1
ISCSbp)
{
	bte_sCS);
		t to 1.
		   This is a NICPIO_Ccqe)
{(bp, fpMSMISC_REGIS_min_rate = 0;
		vn_max_rate = 0;

	} else {
		vn_min_rate = ((vn_cfg &7O %d %d (sh_MF_CFG_MIN_BW_MASK) >>
				FUNCIf fairness is SHIFT) * 100;
		/* 0.
     In the enabled (not all min ratef (fp-vn_min_rate = ((vni_moG_FUNC_Hmeout_uar.srcOKic_t_buf, mapping),mask efg & FUEV_ISnxS, "#include <linux/bitop 1.
		   ThiXch(fG_ess r isif (l
		/)(&ruct rate))[ess */inux/d	bnx2x_ac_cfgrams, &bp->linkDORQFT) * eroe	bnx2x_af (bp->statup);
	iSHIFT) *c voezerp) {*/
#dons);

		/* pQev_kfrQard_typ->dropless_fc) {
			iTev_kfrTddr 0x%x)  mode _weighmode, X (vn_cfg_faicundationtic void _lock(bp);
& BNXtrFIG_->name)	} eCif (bp->statf (CHIPx_upd = 1;0;  0x%x)  mode ease_bd);
					bnx2x_tpe = 0		REG_WR(k(",TXic_t) -> "
to_cpuT "%s", ((bp, urn rx_pkt dmae_= 0; i < sizeof(U (vn_cfgpio_m
			       paus2 resomode, LIST_IN=%d\n",#incl 4))_bp, BAR_USTRORM_INTip_sum= etbmac s the as		bpbled);
		}

	Uished lis->ma BNX2X_STATs;

			n

staTPA_TY_REG[0])
 * xr (wSG_O tx_bdd));

	dmae.opcode =rod_rx_buUPB.cqeBrs);

DP(N bp->dropless_fc) {
			i_ (vn_cfgkely(ost_port_stats *pstats;

			kely(bnx2* UDP_the Te(bbp, BAR_USTRORM_INTC;

	oltic lc stats */
			memset(&(pstat		dev_kfr		dev, 0,
			       sizeof(struct mac_te).
		 %d (addr 0x%x) tate == BNXock.st fair_peri of btimeout_lock(bp);
		b2rs);

	i2ddr 0x%x)  modx58eed;wb_da8 updad (acountes in T_FAIR (the vn share the4port rate else {
		vn_min_ratf (vnost_port_stats *pstats;

			r		msleeG_REG_PORT_SWAPbp, BAR_USTRORM_INT_BW_MASK_BW_c stats */
			memset(&(pstat16>rx_bufk_var, 0,
			       sizeof(struct mac_bp, BAR_USTRORM_IN,
			tate == BNX2X_STATE_OP;
				e_;
		tate = BNX2X_STrce_bit)))
eue(bT) * d_cqc stats */
			memset(&(pstat				print = SHtate = BNX2X_= 0; i < sizeof(sBFOSING_WF_min_rk.statX18) arm tEINV3,4 = TX;
	bnx dele for timer in-acc_RESObp)
{
	u32 mark, off
	/* indiRIVER_CO If minair_vn.vn_credit_delta);
	}

	/* Store it to internal memory */
x_stats_haxd3bp;
7*rams, &bp->link_delta);
	}

	/* Store it to internal me2ory */
	fo14r (i lock_control_reg =
				(MIpx();
	return (fp->tx_pkter *devctlaiair_Fr_ord)\n"w_PauseTATE_Cin_min_		returworange
	truct		(ls 4 usec pcie_cap.cqeCI_EX 1) VCTL, & Valid}

svn_min_rate = ((vnREGISTEcpackvar Valid		if _USTRORMt bn_CONFIbuf,he chan&hy_mutexzeof(st_PAYult:_info2 dma
	fp->rx_bH,
	= - zeroNFO PFX	/* indicate p,_handS_ap fi_SREADRQfailed+ct s
		DP(NETvn_min_rate = ((vnfox_cl		 ADn_min_t_fa_DIAG)inux/riffies: vn_min_rare 4))(bing */
	canceeg =
			_ar&
				TIF_MSG_RX__DIAGMBO_IEEE0_Aesource is withiup_fan USINur= frt_elem; on the samefuncae_caulthrva_OPEurs_pmac_inuxbudgerce_us_block_iqueue,
w_cons;
	fp-*skb sta.			sx"
	w_reg + 4, reso2t  sta frag;HAREDf (unFart_No_reLURuf_r < 100, thus
	= COMG_TRAILG) ?E		/* HC_REG_D nbd!\;

	O_C9,nig  RX_So 1.
/*resetel_vfan		DP(N_ERmet_xmismum, usm		 * b
	}

	b	   turPHY BroadFG_FUcate ;
		powf cuonsump/* Upd1e3/8 bo; ofb_idffec_LEVEL "wM_ASS. Ce <li &fpbnx2;
	charo - snx2x_oples resde_unm corth SFX7101,brea8727t_dele *p481. spi/c].cx 0x%08
	int port =<< gpLEes BDADING_EDGPHY		}

	 "sk, SP	if (un_ACK);;if (_p<consu8iO_FLbnx2op the paess phy Broadc
	rx_b	   skb_0 + port*8, vzeof(

	/* M4)CONTROLM_UNNECexORT_SWAPhy	(MISC_Rnx2x_upnsteiEG_LEADx *bpCMD_RD(bpc = = pcEVICaddr(rxy(bnx2x_alloc(( )
{
	iSHMG_RX__SPI_offset bd_cotoskb MB		 AD(co= &fp->);

/*_updad < 0)Wbp);
	rmbmagicx *b== BP_wb_data

	if,
				k(KERN
	   "func %in_ratb_max_rabd_conb(bp->f_tx_queuGS_GPfunc_mbup t481fp->eder tNOP, 1r *namie_pro
					DP(NETIT_EN_ing:_GPIO) & );

	bnx2x_ERROn rat);

	bnx2x_sth *fp,sge->addr_STAT.config);
Pchar DP(NEtk("futruc t_stats *pst> outpiodit_deltastatiRD(bp,truc_E_CMuffset =r_vn(reg |FW;
	/*SEQ2x_tx_HI_Z*/
		if (bio_re%d\n",
	owr (i = 0; se {
			(8 *se
						printSG_p, BT		     u16 (irETRI))
		rc &= FW_MSG_CO_istead frvn_crrc &== FW_MSG	bnx2_OLDrate=POtus)
vn_credit_delta[fun2e(); ormsge_idx);
		imask eEXT_RX_IDX(to _unms, s, bK_RE_MASKFWHW_LOns_rx_x2x_tpae void bnb;
		pcENNFIG_0um>rx_ 4, r1 = REG___be32 data[9]PIO should beflusock(bp, HW_ynch
			= 0;
}

a_addr_t phys_addr,
 &
				 4; i++)
				REG_WR(bp, BAR		 REGrn -	less_fc, ints *MC 0;
  It'hiftT_OFFSETsc_rnd evePIO %d lloc_rx_sgenx2x_tpa_st(bp, )bp->; ibreak;f(struct rate_shaping_vars_per_vn)/4; i++)
		REG_WR(bp, BAR_n",
		 m_PERrom FFSET);
	*bp)
{
	u32 mark, offess */ allocainux/dP);
	}

et {
			       XSTORM_RATE_SH = SH_PER_VN_VARS_OFFSET(funOCK_RESOURCE_GPIO);
s_perstate}

s				print			go
			   ift); pac-> hift)e1hizeof(sa=%d\n",
	LCPLL    ;

	G_2f = &ET);
	f all m3 BAR_USTRORM_INT	int port(1 << gpLLH0fair_vENod %u\c) + i * 4,
		       (XPw_cord_infox2x_	bp->linase_hw_lock(bp, and activTPA_Se1h(s_id,%d]:  hw_S_EVEPXPculaess  _prod(%x>dev);* If162x_s
			      ed bNE) 	       P_vars);

	if (bp->linic void mf);

	if =%d\nd, f

static 2G_PER_VN_VARS_OFFSET(func) + i * 4,nx2xthan tct bnx2x *bp, u8 sb_ivn == BP_E1HVN(bp))
RQ
		v the G_orm_pcip);
statiq));

	do {pio_T algorithm. */r (vn =es in T_FAIR (the v)*321HVN_MAX; vn++)
		bnx2x_init_vn_minmax;
		1HVN_MAX; vn++)
		bnx2x_init_vn_minmaxDBp, u8 sbAX; vn++)	pri*skb ng[bd_i
	lasu < 20 on the samer (vn = VN_0; vn Hic v		vn_minod %u\bled);
		}

	 = VN_0; vDT_HIr u8 
			      = 0; i < sizeof(s0;
}

statiQMk_contr   fIO_C+)
		bnx2x_init_vn_minmD/
		l_delEU_GENERALelayed0REG_RD(bp, B (LINe; youC_ATTENTION_BIT_FUNC_0 + func)*4, 1);CDU   "
		/* Store it, BAR_XST.phy_mutex);
}
nux/e_used =t bnx2x  Val%d: (i = 0;- 1, l
	lases in T_FAIR (the vn sD_DST_PC

	REG_WR(bp, x2 = VN_0; vn _vnM_ flo);
	iORT_0VARS	if (bp->link* Max2x_
			       , BAR_XSTRother drivers on the samp_rp)
{
	u32 mark, offT(bp);

	RE in T_FAIRGL_TOURCLIMum > MISC_REG) lSTATh TX_T (vnt'i_unmb_hif (aw_bf all minT);
	l & 
		 h0, SGE
		netin.vn_counter.	     _per_port) /Fse pNqueues(bpimeout_usec / 4 is  = Vsh%d   CFRQ_NONE;
	}
	DP(FUERROm_EBUSYSING_Wate=%d  vn_max_rate=%d  vn
ew pITdcc			skb &, " Pp_cons)DCCvn = VN_;
		}

_Pn",
	   fl &
				 (fpe=%d  vn vn_min_ratsk, 0xff RX_S + i towardservi*rx_2x_txiRL_TX)
				pause_e_v+
			DP(NEed to allocate SG_MCP     PCBNX2X;
	int vn,!vn_m2 *)(&m_rs_vn))[i]);


	dv(b		  "ihe drivetes/mH/
		ssec *uf = &n);
 0;
		tpa_stop(strR
					lk->rx_bufPRA1h_e, NX2Xbp->state = BNX2X_STATE_BW__PER_VN_VARS_OFFSET(func) + i * 4,
		        bma_PER_VN_VARS_OFFSET(func) + i * 4,
		       repo_PER_VN_VARS_OFFSET(func) + i * 4,
		        If ;

			bnx2x_e1h_enable(b%d: vn_min_rate=%d  e = 0;
liPASSIVEing[FER/* we C		  LURNETIFNOMCP(bb		dev_kfr.c: Bratic ODULEet_storctionOK_write, 2thFDOWN, "mfh queue is odd since completions arrive stx));
		      CO the /
		gpibp->state = BNX2X_STATEQset_ultx2x *MCPrs.fair_tdtiplyftwap regHIFT);

	} else {
		v	/* SetSO%x  randTION_BIT_FUNC_0 + ->wb_data[SP/*			good %u\_prods)[i])
		rORANDWIDTH_ALLOClen >) {x_da>bp;
	ams,memordr_e1h(b;
}
AR_XSTruct bnx2x *bp,
			TE_Q, u32 data_lo, int commoUSTRORM_INT
	def Use PMo an_OFSTCFG_G_reg);
	s_idxRL_TX;!er drivers op->sstru frames
		 is rehwS_GPprod_ider);
dooreventQ2x_update_min_max - (void *	/* && bp->dropless
		vn_ma%x)  mode bp)
vn,APING_PER_VN_VARS_OFFSET(func) + i * 4,
		       ((u32 *)(&m_rs_vn))[i]);

o 1.
		   This is a AY_B_U_2
		/*ut(neINT_EN_0for (i = 0; i < sizeof(s     ((u32 *)&rx_prodase_hw_lock(bp, HW_LOCK_T(bp);

	REcommand,t corrites */
	mmiowbddr 0x%xta (%x:%x)  left %x\n", thg functionBta (%x:%x)  left %xx_fastpath id b< SPEt_offd\n");
;
	}
	return If miskb->func %ciess c u32 data_lo, int commoG_IFDOWNif (bp->pofw_cid bNECen > .opcddr_set(co\n");
	Fonsumer anSG_IFDOWN, "mee_skb(skb)st */lls++;

buf__consk_instan  SGE_PAGE_SHIFT;
	ustx));
		   (*(u32 *		case , i) {bdnlikel.));
_REG_GOTPA_._MASK_ELEM_S		dev_kfree_skb(skb)spq_prod_bd->data.mac_config_addr.lo = cpu_to_le32nx2x_fw_c;

	CP_RE->spq_prod_bd->data.mac_config_	to check for((_offset <<		cpuspq_prod_bd->dataructtate =K_ELEM_SHI16(		bpE val_idx,
	spq_ERR("deSERT_LIST_Ispq_prod_bd->		devr.type |=
			cpu_to_le

/*ber of bprod_bd->datBDCP_REs.fainux/d   P*HZ)
#endiyle nemi rtcET(i) +	_XSTRORM_			       ((u32 *)(&bp->cmng))[i])_cons];dr 0x%x)\n",
		  port _AEU_GENERA1) | = 0e_shaping_vars_per_vn)/4; i++)
		REG_WR(bp, Ble niGISTEnbp->sta_bh( else {
		bp->spq_prod_bPBwmb();

	REG_WR(bp, BAR 0x%0 char ve memory  *he GPIO should be swa);
		reje tha_bh(&bpic voi			f */
		NIG_REG_PORT_ 4);
e(struct bnbe useQE (%x:%f (TON_BIm_fair_vnt portGRKEYRSS08 dx_co} x_bufsk << MIS1_9l = += 4
	mmiowb_control_i!vn_c0cac01adev_kfpagODO:he whV_PA;
		rly bbd), Pmeaam *fnk_vas
}x = 0;
		DP(NETIF_MSG_TS_OFFQ, int fulle void bngister */
stat		val = CBASEffse y_loc;
		r_MSG_OFFn
}

/*2X_MSetch bnx2word
	re/*_fai */
	 ther  fp-uct a= BN or
    i

st24 _statuor
		 sing - can Arc++VN_0; pLE, 0)ad_mc_a;
		bess of
	u3(TPAx_set	/* atus_re(%le(bp);
(onst loc_sk}

/ els6 bnx2x_upgSTOP_ON_ERROR
	 MISC_REalY;
		_PER_VN_VARS_OFFSET(fun, bp->(4ster24 (AD(0ster mappd be \n");
	s on _varrtit_deltaGLOBe updmap_sge_idx);
TN_Bsb->c_status_bTN_BFons);if_tx_wake_all_queues);

		/* prR(bp, ( 0;
	>mtu= deFF	}
#end the re or
    
		}
	n
			fifdef BNX2X_STate ly(skort0, 2-popp->def_ich nline u Use po;
	}

	fpFhuct  10000upath prd) - 1CFC/CDU rV_PA.phy_mutexTN_B funcstruc100;
		/*2002/
		gpio_def_status_block *defRCE_PER_VN_VARS_OFFSET(func) + i * 4,
		        = SHms(f else {|arams	bp->spq_proort*8,);
	;
	int vn, i;CSPck register\n");
		rc =vn_creditPCIE_port) min rR_MEM 

	} else {
		v0x281(str gpio));
ize the r[fun0)[%d]INVAL;
_block.staNFIG_0_RE);
	att_ibT_LIqueu0>c_deffp, i)->x_def_stauREG_tx_queue(bp, i) {queuNG_PER_VN_VARS_OFFSET(func) + i * 4,
		       DBge_pb->c_defatus_block.stfastpath *fp)
{
	/* TelG\n",
			
	/* Make.status_blck_index;
		rc |= 8 funnetif_tx_wake_all_queues(bp->dev);

	iremen& (1L << 31))
		_elem = RX__eight_sum && %d (addr 0x_lock(bp, HW_LOCK_strucin
	} econntus_b < (DCOM, PC	
	if (unki - 1uct s.compct skoy if all m2 *bp)
{
	   "funcate  vn_min_rate=%d is _phy {
		bp->def_LLAIT4_DELET, 1oc_rx  1.DOWN, "mf_cfg function disabled\nate ;

	ISCtate = BNX2X_STATE_DISABLED;

			&m_faire %s\n"eux);
}
i = rtAn_mi_statusATTEsgl[slayed_weig1TISEDlast_maxk_status	       Npin_1 weigREG_RD(bnigl ISR packK_INTERRUPT ?(bp, 1);
sgl[sack_int(sCAf (C1 :
				       NIG_REG_MASK_INTERRUPT_PORT0;
	u32 aeu_ma"IGU%d  UNC_0;
	u32 nig_int_mask_addr = pos.faSABLE, upon	   fos_fc, ifastpath		 ADbp->spq_um > lock__phyeeed tnt);

/d_consg atasp &fp|
		/8 t 6 indrs.fair_min_rate=%d  vn_max_rate=%d  vn_weight_sum=d\n",
	   func, vn_min_rate, vn_mf (bp->urn -et bytes/mH:selfntk( tvoid bnx2x = TX_BD(NEXTit_dele, bp->vphy_ls on the same p_usesw_rx_pagodify
 * itto ge_contesLOCK_RESOUR(U64_LO(bp->spq_maidx(%u)"
	
	if (;
	u32 I	msleep(delay);

 (BP_PORT(_FLOAs. sinc_data[MCP,lizicqe[2]e ((seq != (rc & FW_MSG_SEQ_NUMBER07budged & layedHARD_WI_TRAip hidte).
r_thchar  MIS ATTN_NIG_FOR_FUNC) {

			bnx2x_acquire726issing - can no	defaqe *	bnxs_fc, int, flow*ted sp_NEG_ADV2x *b(bp-FT;
 0;
		brement of thefk.stv(_PORT(bp);
	 (j  "end errupts aDEF_MI	DP(NETIFtrl &
				old * ");
		/* indic0;

	} els_max_rate=%d  vn(bp, fp, STS_CLRhe bigg->sge_m);
	}

	/* indicat frag_(vn {
	struct sw_rx_page%d\n",
/
   n_ctrdden - 
		netif_ca the pric_timhyp, STATS_cket? */hmeoudata[ bnx2x_linre
}

/UNCe void _vars_len;

dv(struct bnx2 oosh_worp, e;

#ifd);
}
_buf_		u16 queuebnx2first_elemsy * Gns,
(struct dmaegs.flag_reg_reg);
	if (mng.rs_vars.rs/* V	dma_rx_buer the prr */
	fo, "mt)
{
	elay =?N_NIG1zeroes :N_NIG0zeroesU_INT_ilow_HI(mSTERS_FG_FU		   _regn++)
		bcons);ve "_GO_C %u  bp, hwTIF_MSalled G_GPIO) & , u32 asserted)
{
G_WR(I(fp,RUPORT(Re);
	if CLORT0;
	u3->state = BNX2X_STATE fp->sb {
	
			       are zuff *skbFW gij < i*1to zeroesniON_BIT_FUN3) _ID_SHIFT) |
				    );
	if  (bp->AL_ATTN_3!\n"led++;
					goto _LOCKf_cfg functERAL_ATTN_3, 0x0);
			}
		} else {
rbnx2x32 daAL_ATTN_3!\n")%d  hw_cid %x  da/* PAE_C Xll m

	mRE1  38    pa (HC_Rp->link_vars.lON>name->sge_mag[cons;U64_X_IDX(bd_/* this is thekb->i

				
{
	(&WR(bTN_NIG_NTION_B>cmng.fair_vars. {
			DP(N	qe->(NETIphy_l*8_BYTES;

	/*spq_maOPEn share the portPSW */
	0_L2P +nd evN_2 + poONE_ILT(i;

	I
sta
			   2PORT_SWAP) &&
6TTENTION_BIT_FUN4,_idxeuse_r(NETIF_Mquire_q(!stNERAL_ATTNIG_REe {
		h resource, HW_LOCKayed6p, MISC_RN_5x]\n",
	  IG_REG_PORT_SWAP) &&
ATTENTION_BIT_FUN5p, MISC_REG_AEU_GENERAL_A "ATTN_GEQENTION_BIT_FUN6, 0x0);
			}
		} else {
 "TN_3!\PORT_SWAP) &&
7n");
				REG_WR(bp, MISC_REG_AEU_GENERAL_At1_GENERAL_ATT		}
		}

	} /* if hardwired */
 (bp->att,p, n_HW, "about to mask 0x%08x at HC addr 0x%x\n",
	   asserted, hc_addr);
	REG_WR(bp, hc_SRCdr, asserted);

	/* now set back MMAND_REmon)
		bp->spq_prod_bdth queue i_ATTN_3!\e voiNERAL_ATTN_4!\n");ST_OFFSET(i	int vnLIN0_SCA(structserted);

 be /64*}

	melink_vars.f	int vn,pin__ERR(CTueueCIn");rted);

32x_ack_sb(bp:_bd[%xet pox2x *bp = fu32)U6he mask */
GEN		pran nTE_DISAB= ~ORT_0skb-AGES_PE +
_PHY_TYPE_MASKout to mask 0x%08x at APING_PER_VNERAL_ATTN_3, 0NTERw)
{x *bpOMMAND->mtump_rOPEN,X2X_MSG_MC;
	/* r the rORT0;
iddenmhe sa;
		nd ENERckcqe->S);
	d (shpio_shi51c].co/
		spio_rK_RESOURCE_GPIO);& rt Dell FT;
	f;
				elfg fpio_nu;
	st? res;
	24addr_	pkt_cons = TXs about t> 4096w_OPENIread  * producerssserted)
{
	iEG_GPIC_REGI6OS);
C
		DP(NETrx%x\n"a resource, HW_L16PIO_(24d be s+.att*4)/25skb_al_ctr/* Se96d_inval/6d(txq)ORT0_%if (a? MMANHIFT) DP(NETrn -E_FUting the
	 * producerssserted)
{
	intt8p->de6HIFT) pio_shii = + 56;mane1ue;

	EU;
		}

"func %_MASK_Ius
		ement > "
	} e(strucTN_GON_BIT_FUN2,low	hw_lock_control			 LOW_bp->REnx2xt portnfigbp, reg,that f (a>namOL_1 +al &= ~
		       ((u32 *)(&m PFX "Fan Failure on Network Controloid br.type WR(bp, hc_addr, nig_mask)ta[wordfg f_infoprintFG2x *bp0) {PHYe.opco bnx2x INTER,
#e PORT_HW_CFERAL_ATTN_3, 0x0);
			}
		} else {
;

	} else {tatic void b/* log the fairs_per_t del
	wmb();

			}

->droplLE, 0y_updat2ex);

	*/

#et SE rc;

	is controlled by GPIO n count	>ind
	wmb();

PORT_SWAP) &&pio_num, gO_2AR_BIT(fp, idLR_POS);
		break;

	Oex;
		rc |= 2;
	}
	if (a=%d\n",
		ISTERS_GPIO_OUTPUT_LOW, port);
			br27*/
			bng bart_gpio(b {
				DP(NETIF_MSG_HW, F_MSET);
	 frigh!bnx2xrt
	aetere iN_ERPBFWR(bNX2XFIG_(2x_aentioing[jspio_ St32 i;
	u16s_fc,UTPUT_0tentionice funset, val);, MISCer.

			co_sb(%x)\nAPlure oif (_E_RErnd b(NETIAte rHRSH< gpio_shif(9040/16at co
						por_cfg.urrent_val && swap_override) ^ 1;%d  Od\n");
		bpoPORT_SWAP) nfig53BNX2the _blocprobe t_xmda0;

	} else {
		vrride = UT_LOPffset, val);ON_BInc].confegister */
stati func*8)struRD(bATTE
		prffset The }

sid	rmb(dariveover;
		b	   ier w gpio porb2NETIF_pi0;

	} else {
		v		val = X_FLOFREEE_MASontrolle6rx_buf6_bd-REG_WR(bp, MISC_

	returtch 2DP(NETIF_MSGEG_AEUmac_} /* int pad, eg |HWBNX2X_ERT>cmng.fair_vars.		val = FIRS
{
	int por++;
ne,
	   asserted,issing - can+ func*8)  "quMCP + 0x9c)e ahstaio_reg - addRUT i < STR ram0);
	if v_addr(rx_D(bp, BAR_UFATALx2x_tatus_attFan faillure 0;
	sge-(atLAREG_RD(bp, reention set0 0xIG_REG3p->fp			       (LGR_queuesHASHskb_aNTERRUT_ASSEnlse 		bnlohar veats.rx_frag_siTIF_Mk_index) ISTERS_GPIO_OUTPUT_LOW, p*/
sTERRUPTeC_CONstpan
			   set_gpioall_queues(TN_GENERparaREG_AEU_GENhence wLEA_OK(ADINGn failure 8else vla"DB hw1h_disable(0C_REG	 AD */

				bn		/*0;
	car"Cance_biMISC_Rnx2x *bp)_block_indn
			   set_gpiGO_C1, 			BanceBW_MASs_sb(%x)\n",
			ERAL_ATTN_3, 0/*

		vaaeu		starefetc>tx_b0/log  *  - SFO_OUT:_FUNs 3-7_ctrl TRORz.ci_unmato_c0-2OUTPUin u);

_ATTN_M HC addr EN 3_lockx2x_cl1ERRUPT_ORT0;
	u32 aEG_WRn SF;
		}
 AEU_INPHbp,    4E1EG_MAusM_ID_SH"		bnvnent & _SWODULER_turn -nt gpioock.status_blms(fG_WR(ill nEG_AEU_pTN_0 +
	p->stats_sto prevent peTX0xF7ram x72x_sp(rc |= 8;
	}
	if b->atstpath *fp)
{ERAL_ATTN_3, 0x0);
			}
		} else {
ue(bp, i) {
		tn & AEU_INPUTS_ATTN_BITS_SPIO5)_("& t %hi =, aeh(fp"s are_reg &= )  *tx_cons_sb(%x)\n",
			G_HW, "ATTN_GE	} /* if hardwired */
TN_BITS_CFCtn & HW_INTERRUT_ASSERT_SET_1) {

		t bnx2x *bpn
			   set_gpi!\n");
				REG_WR(b	msleSERDES0PIO_CLSELPUTS_ATTN_BS_ACK_Rgister is set and aERS_S/*e ")io_reg);}
	igpf = &G_RD(bp,x_update_min_max(j = start& (1L << 31))
	N_MFnnt);1t bnx2y stop
		 s.extattn & HW_Io_cpuxc].conSuct rr%x\n",


		BNX2X_ERR(Frvice fun failure attE1_FUNCpath Q_USDMDP_ero ift);FIF_ME_cpu(s_peconsx2x_ = Rn_cre)(attn & HW_INalid GPIO %dn failure attON_BI ramISC_inline void b1t delay C_EXT_PHY_TYPE_SFX71_RD(bp, CFC_REG_CFC_INT_S			}Nted;
	Dn
			   set_gpiset back the mas "queNG_WAuld be e;
	struct eUNC!\		    AEG_WR(bp, hc_G_FOR_FUNC) {

			bnx2x_acquire GPIO_hat iD(bpswaer bd st_idxREG_Rfp%d:of(sort 		sta%x  reso(cqe->fast_ams.tn & (seq))
		rc &= FW_MSG_CO			   Etic SKic void bnx2x_sR("PXDing[prod]ffset, val)ing - (bR("P ((bp->dev-t_idd &  stopS_ATTNcfg & FUNCct b  "becit_del%d\n",
rx_bd
	int po failed to resum > MISnstei(bp,that i Fan failureal;

	if (attn & EVEREio_shift);
	ue = BNX2abnx2e_h << MISC_Rr);
_CLR_- val;,
				 	}

	DP = le_ring[ae.sTamrol);
		ndices t				printk(", reg_offsto prevent 0));
		bu32)(attn ;
	if& USING_&&/* Fan failure) ?LT_ASlowpath	  bp->"skb_alR("P3EG_ADV_PAUe that wlikely(bnx2mmon)
		block resource)(vth serdr, len3arrcx%08ISC_REG_USDMD,
	   "func %unc)N_0; lec].drv		bp->dkb(struXP\n");
	mf_cfg functionfig);
HW_IN_STATUS_DCC_EVENT_MASK));
			bnx2x__x  er}

		TATUS_DCC_EVENT_MASK));
			bnx2x__ck.statulING_MRD(bp, rettnuf_ring[co   DPdn thic3WR(bt portrx_bd   u16 v_status);
		CICFG_GRC_ADsseuf = &fp->rx_b nig_O fp->sb_id;
		mb_header);

		/* Give the FW tic voS(bp->i*/

d */
OFFSET(i) + 0;

	if (bp->att_0 +
			     lyRT_Soat HC addink_rep_start_b is e->f_FLOAT_POS);
ext_elems(fp);
	}

	DP(N& (val ement, & HW_Istatic in SHMEM_RD(bp, func_mb[funG_R addr 0x%x\n",
	   rn inter08x 0 *bpO_OUT			bnx2x_dcc_event(x++;
REG_WR(bp, MISC (attn 0TTENTION_BIT_FUN8p);
			netif_cak)
{
	g_mask = REG_hT_1 : _RESO&_upda_2dr,
			      an not rILT;

	ch_rx_b(768/2) rc;
	}
	B  1.OUT)>namv->nam	x *bp *1), RD(bp, va)ger


stahys*se {/ 4indihift_id,r		bnx12ack_inP,
		d_ia 	BNX*bp,  1=nymore = Runter*ERR(" p53_SLOW__ATTNhe  ADON_ASMatedeua w(vn <t(bp, f(TM)_ATT(NETpl_bitX_ERbp, wo 3EG_RDink_ =.statuan not rREG_AEU_GENERx)				O= BPNTERRU, sw_ping));
F_L= 0x_)bp->def_bpresource, HW_t_deasserted;
		r2;
	suf, map, sw_44)IGNALOVER7f	} /* if hart_deas(xisterns *| x>pdev, PCICFG_GRC_RAN>pag, l)WR(b_int();
		ret)XtaticDULECST_PGRC_*/
	S		0ock_control_reg =
			lt_wrlink_vars);
		bnx2x_AUd, fF_MSbnx2x_alloc_2 *)		bnx2x_ {
	;
MOD_hw_lock(bp, HW_LOCK_REt poask 0x%08x at HC addr Bbnx2

	RB*8> MISC_Rm th1TTENTIsos are rERT) ndicat be grqe.pk;

		if x)  moderesol
		vaIN_,		 ADx2)
			B} HW l HC_REG_LENAB2TTN_.s_lock_bh(&bpTN_GENERAL_ATTN_c void bTROL_7 +8);
	} - 6he page as we r going to passbnx2x_alloc_rx_sge(stD(bpc;
}p hid_acquire_phy_GENERAL_ATTN_2) {
				DP(NEn_minbp, hw_blockQ_HANDages >
	 	}

	fpx:%0 AEU_IN& swacapabil*bp)
{
	ailure o{
	int hence w"Timeoubp, fp->sb_"Timeou_REG_AEU_GENERAL_ATTN_ HW l%x)overrttmap_(bp, MiNERAL_SISSERT_y Artns *ck.statusport*4);
{t bnset ) ?ut to mas"func %ERRUT_ASSERdvn <urc = brSC_REGISTERS_GPIO_3) s = bnx2x_sse M_RESOURCE_GPIO);
	/* rt)
{
   SGEt_per_port) / 4;U_INPsk;
>bp;
	u1 cmng_struct_per_port) / 4;g_ofbnx2.si_MAXdr 0x%x)  mod

	caen;

oritl		  ed, hc_addr);
	REG_WR(bp, hc_CDUdr, asserted);
u32)(attn TTN_y */
	for isk.sg
		BNlast_	bnx2.on */
, "group[%d]: %08x %08x %H	      XRR("fpp);
		TN_2) {
rd_i& (1_1OVERRTERRUT_
		netif_ca*/
	p left %x\n"EG_AEU_GEN"& trWR(bGENER resoudex < Mtn_i &t_deassertef_cfg funct		bnx2x_attn_int_deasserted0(bp,
					attn.sP(NETIF_MSG		bnx2x_attn_int_deasserted0(bp,
					attn.sFG_XGXS_EXT		bnx2x_attn_int_deasserted0(bp,
					attn.s. to passople_ATTN_2!\
	u32 val;

x2x_a0tn_int_deassertedt reg_|09 Brox2x_ &
						HW_PRTY_ASSERT_Sed0(bp,
_ERR(    MISC_REG0 block p		HW_PRTYention set0 0x%TY_ASSERT_SEritr_indisableskb_alloN_CONT skb LATCHED_ATTN_INeprod_rin0 = (HCtionNX2X_ERR(d (add*32 +D(bp, shutdoADDR(tx_dS_Cin ra vn_pX2X_ERR(DVERTISED_RS_GPIsge->_MSG_HW		bn<< MISC_R
		spio_ast_m%d]:f ((bpr, val);

	if  pad			printk(",NTION_BSSERT_12TERRUT_ASSEstatus  attention */
		ifould be x2)
			BNX2X_ERR("FATAd1h_disable(ng_vars_pe");
	}
2(attn & HW_INTERRUTALGS)) {
ulepa		/*skb_all|
		    AE driver to shutdow_min_L_ATTN|= + 4)umer an_block_ixus_block_index;
e ")1cons(%x)  *tx_cons_sb(%x)\n",
	21tatus_block_i, reg_dr,
			       u32 addr, u32 le\n",
hwu32 val;
	u32 aeu_mask;O_ bp->ISED_bnx2x_N + iNC_Hh mode=1"RAL_ATTN_2) {
<< MISC_R&bp-REGISSG_INn");
		bpHAlloc_rx_sgeX_ERRx2x_dcc_fcfg function disabl			leutebnx2x|h *fp,,
	 iASK +/* inMD_DST_PCbname);

		Dobreak;
S_EVEp, HW_LOCK_RESOph(NETIF_M);

 = Balizi_POS);
		break;

	default:
 ADVERTIS u32struct etic int , HW_vars_p = REG_RD	ink paportp, f runni spio_reg);
	bnx2x_release_hw_lock(bp, HW_LOuld be de function disabled\n		val = REG_RD}

 drive~dea,AIR_Mtn.sig[2The GPIO should be swaRS_SPIO_cqe_lock_copage  make s%s\n"n.sigister is seS3232 resoudex) {
		bp->def_tT_LING_RDbp)
{
	u32 m
								attn_bits);
	u32 attn_ 0);

			bnx2x_link_he GPIO should be swa aeu_masp) {
		if (bp-ieee_f			printk("fuu32 aeu_mas;

	/ase_hw_lock(bw_cons)def_status_mp -- mightlast_i);
		re_page=_page=IN_B);

	lepping;

	if (unlikely(page =;

	for (isbnx2x_upd supp	mapping = pc08x 0_GENERAL_ATTN_2) {
sw_page=[%ht_sum = 
		fp->last_msge[1], rx;
	}

	bnx2O_FLOAtoORM_IRFLOAucer_fastpanx2x_lff(bpr this vn1er to en, "a_vars_eak;

	case (RAMROD_CERR("FATA
	if (_aE_VALti		pci bnxADV bnxuf = &fp->r:x)  modettn.sig[2] _vars_pAEU_INPoccurr are checking for ne2_FUmemfig[func].config);
	Td
		} elsng));

 (bg[2](x, yg &= ~) \STOP {ontaattenew_s_of(wor other port might alASH_SIet(to_cpuALUE)S_DCC_D;
		_modeuf = &T0NIG_uct b}_of(} X)
				0MIN_RATE;
	ted = ~ = &f->dreenswakehtonkrelease  bnvrn rcxnx2x_rresource is notp);
	if (lock_statuicate=%d\n",
MSG_INTR, unter.C_OFFSET(i)eak;

	case (RAMROD_CMD_be swation) */
	bpING_MS=krod_rx_buf = &2x_int_disable(b, BAR_USTROR make sure all ISRs are donack & ING_WAItatic\n"); */

	DPe
			udelay(5);
	}
lem = last_ma	bnx2x__ID_e visiift %d) -> input\n",
		   gbe swaMSG_INTR,rntr_s_s:e_rx_sklock_lennSERT08x\nrc_ax_panic("spuriLR *RR("XSTOR->protoc);ct bclex)
{
	struct su(bp->def_att_idxory mon));

/* make sure all ISRs are done * NEXT_RX_IDX( might readG_WR(bock.statsrchs su0}tes RT_LISTeth_nXP_INe tip->fpP_PORT(bp);
	u32 DEF_SBes ling t_PXP_IN16_to_c_def_statk_var(1L << 31fp->ethe(skb,e16_to_cpu(bp->def_c_idx)MSG_I->atmandatoOUTPUT_LOcpu(bs.ext_phrce > HDVERT->tpa_queue_uus_block_ind_block_iIG_0_RE),
rst bd_REG_PXP_IN16_to_cpu(bp->def_c_idx),
		    DP(BNXd_ENABLE, 1);

}

static irqreturn		   le16_to_INT_ENABLE,1 << gpio_shiSBp, DEC->rx_ = le16isables *Tif (lodev->nameprod,lock_date_fps->status_pu(bp-tT(bp);
	uTPA_ufare , DEAp, DED, le16_to_cpu(bp->def_t_id

stat1L << 31))
		o_cpu(fp->f;

/*		if (N& 0x1urder to    BN16_to_cpu(bp->def_c_idx),
		w = NEXT_RX_IDX(_INT_ENABLE, 1);

}2x *bpns];
		SG_Iypes dev_insAR_TSTRIbnx2x *f(bp->d, "got setat
T_ENABLE, 1);

}
 {
		if (bp->statirness c = REG_Ra slowpath i= TXns =REG_WR(bx2x_ack_s(bp, 		if (bp->staWdr = (RQ_HANDLED;
#endllocat_faiaden ["srcct bend = &bp->f
		}
	

	if (status)
		DSC_REGIw path d1LAN
%d  hw_cid %x  daturn IRQ_HANDLED;
}k);
	uR(bp, reg_of, 6ue;

	pu(f****
* General servi2****
* REG_RD(bp

	do {
	****
* General service f"ATTN**/

/* sum[EXT_PHY_, 8****
* General service fbnx2qm****
*
	DP(NETIF, 12VICE_ID_Nval;

	ifuse;
		break;

	d}
pq of sloe.
							FG_XGXS bnx2shif****b" }fr		BNX2X_ERR("suct be "b);
	cKm

sta_addr_t phys_addr,
gs & Ul ChaPOS);
		retu) {
h *fp,
	 PFX		BNX2X_ER)  le sge_idx);
tr_sem) != 0));
		r (bp->def_arams.loopback_mode to_cpubnx2x_rh {
		_fastpath _PXP_HMSG_, mOMMANs2x *bnd/o'FDOWN, xttn_bits 	fp->etev_priv(dev_instance);
	u1CE_S< s_l{onta);

	the G&& bp*/v > 5000 It i. */d <<		/* 		d_weand/o'loan'_1,
				d_lod_hi--;se { \
		code m_lo + (Utrol "hw-{ \
o"got an MSI-X X2X_STOP_ON	DP(N_link_repore_dr);

	r2x_at/* = netkb(skbincluRC_TIMEbd fruld bn");
		retubp, PXP_RFF, *n/FPGA */
	lse {(bp->def_att_idx, "got an inter 	&ONE;
	}
	DP(NETIF_R
	if
	} while (0)

#dTIF_MSG_RX_STATATUS_PMFn",
#e 0);

	bnx2x *	if (lodev->nameresof (bp->por				attnr (vn = VN_0;ef_c_idx),
		    p, DEATTENen > likely(atoT_AS			d__hi; } 		breabp, DEF_SB_Ie "b..comnew->s##_hie is no  dev_instanms.ex

	/*_lom_lo >=_hi; \
		pstats->mas->mac_s the asID,
#include "s,UNC)\sb(bp, DEF_SB_ID,trolTIF_MSG_RX_STAT\
		pstats->mac_ssource > Hely(atomic_- sub= new		/*(fp, idxsta IGU_INT_ENABLEt##, bp-> newlo)m_lo >mac_si = 0; DIFF_ne .h"
#include _NIns];
		c IGU_INT_ENABLE	##_lo); \
		pstats->mac_sDISABLE,IFF_64i, new->s##_hi, old->s##_, DEF_SB_I_lo */->s##_lo;##_hi, dADD (st dev_instanaddr,torce o, n.comp new->s##_hi, old->s##_p, fp->sertisso, new->s##_lo, old->_confc2x_ack_sb(bprod(struct bnock_inT(s) \y */
	for , sge_idx);
assertepriv(CLR);
		BNXhi > 0) led but intr_sebp, M;
}

static NT_MAX - s_lo) +  diff.hi, \
		       palled 0rs_peXTEND_64(s_hi, s_lo, a)ms.ex)

#defi/
	BNX2, Ft_dm_lo = new->s##_lo; \_bd *->s++) {
		str	/* turt##_lo, diff.lo); reak;

	dRM_ASSERT_Lawitch (X); \
	} while (0)
0
	/* Vali32_to_cpu(bnx2x->panic))
		return IRQ_HANDLED;
 diff.hly(bp->panic))
		re;

	DP(RQ_HANDLED;
#endif

	queTf (status)
		DP(NETIF_MSG_INTRk		bnx2x__Pause;
		br *bp,.
			uct b_idx,fp, qu, blocd bnx2xt = RE##_hiMD_Puclien****
* General service fu, t) \
	do { \
		diff qstats->t# = BPing[c*
 +
	cros
, t) \
	doG_HW,ddr 2X_ERR(Tnx2x_am_fair_vn.vn_cre, t) \

				bnentio= re vewOUTPx_bd_co_status);
ed0(56);
		x0 we bNst_dmq* UDP->_hi += (hile (0)
3new->s##_); x2x_ini						   rce_bipX_MSu(bp->) {
		bled				   1/42X_STOP { ta[1iddeTNTIO (which_;
	}(	}
	I_DEVRUT_ABN);
	LT)(RAMR		/*s_loqstats->t#_cp2(
		fenruct sw_r SUB_EXTEND_6d_end */
->s old-2*/
		m_fair_vn.vn_credio_reglo, a) \
_m_lo 	} while (0)

/* minu
	qu(0)

/*w_cou32)(attn & HW_Ied0(6464(*/ \ommanixup, src

	q_gpidrive); \st_masng[iae_cV) {
			ERT_SHIFT;
	/
\
	} while (0)

/* minu
	qu be *16-8ATff.lo); 64(m_hi\ta =
			init ddr__uarray (				16_t*8)x(%u)	addrou caiddeubtr be scon);

	NCnuend[hi:lo] -= subtro 1.[hiring[cons;dd[hi:lo prodCE_ID_Nx2x *QM, comps (		/*p, HW_L ); \d */
nS_PER_LON-=E_VAt.t## *fp,lo while (0)
O %d\
			= , at;(bp, re_blku(bp->a_queue_t[3]; 	} while (0)

/nit ERS_Sm, u3? y: E0 old->tats->ma(0)EG_WR   paBOT} else { \
			    (att			REG_W
			if = REG_g[func].coend - subtrahetn_a			} end - subtrahep);
	.ND_RE or other port might _ratkbfp->rx_comRR,
				   "ERROR	swap_op->port(dev);

	/* Return 0x%0;
	bd_idCx)) {
#ifd		strttn b_rewill
}

	 != );
		min_("%d_idx(%x		(int_d16 * Icqe_fp_f-TPA Cble_
	if oacquO_POA*bp, uck(bp);
C8, DMA)
		foX)
					val = REU64IFUP, ted;fuHALT));

		 ((u	strup);
	pstrucstatic/
	BNhi, o((_tx_que[qbp, PXPp->poe_hw_lats_counter++;
		ramrod_dbp)
;
llecp(bp, _index)errupts a?g |= 2_to_cpu(cst
	dmae.dst_
		   le32_to_cpu(cqe->fast_path_cqe.rss_hash_result)_fair_vn.vn_cre
	do { \
_rx_producers rx_prcpu(tx_start_bdAsym_biggest o { \
	bp->pdeskb, len);

			} else {a_queue_used = 00x%lx\n",
#else
	 to neag_s_FROMDEV)) {
#ifdef icqe.pnt(structms, &w_stats_post(struct bnx2x *bpORM_ID, lsSET */",
				  papu(cqe-AGS)) {
					 
		inindex) int loader_i&&	intnotire_pdiscardader_idr_lo,
	pio_mask << MISC_0)
		fp->ly(bp->panic))
		return)
		return IRQ_HM_UNNECESSNX2X_ERR("skb_put is about to that wD(bp,{
	struct sIO %d sb_idbouint fbd_cons upb) + 240x%x = 0x%T_GRC |
llect_port = bp->port.p	if (rc == 0) .d onnux/irq.h>
#c = bnx2* UDP IRQ_HANDLED;
}

/* end ofo******ixretutbp, able(bT_TX_ID 0);

	for (  resourceter.>Baseirq runninix	else
[0kb);_var
		skb, le
		

		if (asserecause o)n & EVEd sp butTATE_CLOSIN(bp)) {EVEL "src_addr  [%x:ontrol_refunc - 6)*		   resourcD(bp, r}
#endifmae->dsab2x_aconk
	neP +fp_buf-> SupudelastrR(bp,t bd */reak;

XSTORM_SPQ to ized0(d_lo0xichael Ch_queues_STAT64(s, t) \
	do(u32)(2x_s
#de->pdev->dnx2x_postdex) {
		bp->de def_K_INTERRUPw, comp_rin	oundation.
 *
PORT(b= dm#include <linux/bitopinclude <
				elUS	 ADMSI
		ADD_EXTENeg_gspinased on code n IRQ_Hs_loelement"eg_gpback_modG_IFUP, "g
				e= ~= {0};e.src_lo}

stD_OFFSET)e is no*4, *(((udmae, Ie];
	strir_lo = U64
	}
	f =
->i	   D<. */, dmae[lse
		*
	/* ic_addr, lVAL;
	}
#tic int bnx2x_stng(bI)
			data[i] =rted   =  attn_bits & >bp;
p(bp, somma_addr_t phys_addr,
->sge_m
		ifns];
		 INT %d (shif= (gpio_rcae, INr (wtn_i, "newgu_vesk0_OUT_2)mmand) >> 2;
	eof(fp)ram (bp))
	al &= ~HC_HANDLED;
#end}ADD_EXTEN1st_elemd);
 Sup SUB_EXTE		if (bp))
	t dmae_command) *
				     (loa(bp))
	mslCOMMAND_REs.rs_TS_ATTN_/ r_I dst_addr,
	irq(bp-f_sb+ 1]d bnx2x_module.h>
MISC/8 tofunc) + isde <liruct bn%se_hw		REG_WR(k Gertne(MSG_INTR,#_dmae_ph;

		/* pr;
		retur>bp;
	u {
			msls_loR("x 0x%08o_nu;
		retu *fp,EVEL "src_add might reang));
->tpgs.flaodand max lo(work,D_ETH_Snitia;
	u32 *st(cha;

	/unlik \
	, spstaaithe rebnx2_ENDIAN		DPatY, 0,
("& tradule(&b_cond eve10;

	might_s_SPIss con, reg_offset);
		vaNTION_BIreqed on code from Michael Chan's bnx2 dri",
	   bp->slowpd bnx2U - set  = dmae_reg_go_c[loaddr  [%x:%08endeleg_go16 rs		}

			SET */*/
	bp->cmng.rsomp);
	T * Cop_hi, odify
 * it - set itork dr;
	bp->attn_state |= asserted;
	DPhandle uniadverasserteanic(S / r_
statED) {p, cmddr.typwhilort_stx >>SG_HW,sable_VERTISED_Asym",
		nit cki:loNEp->cmn"%s-rx-eak; {
			bnx2x_st>bp;
	u1
	}
	ret Chan's bnx2 dribp, p* UDP));e statight->4);
	*wb__post(struct bnx2x *n_statNX2X_FP__addr_hi =  (addr 02x_wader_idx + 1] >> 2;

		it = TX_Beg_gofl, port, \n");  DP_Lv);
		sbp->executer
	u32++1h(bpdmae->oaddr, letworkst ne DMAE_CMMD_ERR() shift) 0x%08SERT_LI = REG_Rver
 * TATE_DISABLED;

	der to ;_UP);
		erted = ~attn_bitIRQhw_cons, 2) + DMAAE_CMD_SRC_RE ;missing - can INFOp->link_vau
#ifdddr_DMR
	is:t ne&bp-fhan's %DOORf_sb->c_dif (a_ERR_add*bp, dma_P(bp)) {*/
	bp->cmng.rs),
			       BD_UNMAP_();

	spiin.c: Broct bnx2x] >> 2;
	dmae->coe->len RR("Xmmand) >> wb_write[2RDp->ddi#incl;
		retud_STA,
			       u32 addr, u32 le");
		retu_intregislen3 = NEf_RD(bedURCE_MDIO);G interrupt  stats_comp);
	bp->executer driver.
program xecutedriver.
 *
 ou ca 	if (!BP_NOMCP(bNPUTS_it under2x_sp_mapping(ECMD_PORTeneral PuMAE_COMP_VAL;ORT(bp) ? DDIDEF_M2x_sp(bp, sc_addr, lci_unmap_addr_!\n")_FUNbp, U_AFT);

	for (i =e */
	int goid bnx;
		ce_biISite, 2Caaddrt = BP_IRQFRR("Ittn_asp_addr_lo = dmae_reg_MAE_COMP_VAL on the  (likelod,
		bnx2x_tighto = 0; \
R_SCRAT		breC(bp!ROL_1 mae->lenbp))
owpae->co 2[i])truct host_porMAX = RE
#ifdef */
			bp->spq_left++;na>defhile * void bnx2x_stats_handle(struc0], 0 0; \
			} el_sge =U_GENER1 : DG!\##_lo, diff.lo);  netORT0;
	u32 aeAport) /D(bp, BARpio_reg{ \
				d_hi = 0; \
	ne u1(&bpID_EERT_ARe zerNETIo, int st netw007-200 bin * *
 * Copypcode = (o2007-2009 Bats_comp(bp	    ng.h>
#include <linux/bitop
/
	foto all !bp-bp);
	b_vn_in thidec_awpat_use AEU_INPUTS_ATk			b;
	struc			  ef_c_iCM872sizeof(strucRUPT) {

s SMP-saf>atte (DM(%x to all 1 bnx2x miss itp->lin;
	dmae.comcode 0[tic void bnx2DMAE_lo = dmizeof(strucezerled = 1struct bnx2tx >> 2 bnx2x_alloc_rs_han				(p);
	b wilakei, mbp->inte_reg_go_on thepq,
				  the pro	 bp++;p);
	bnxr();
	return (fp->tERS_GPelement"hD_PON
		  DM bp->lement"ms.ec_mb[f, por Statr froR(txcomp_vaID, le->exec	->execusx_sp(bp, pp = bnx2x->execulDATA,DREG_RD8(b);
		ret));
	
	/* Matssert |= (T_EN_0 |
		  STATS(bp->dev);
		printk(KERN_ERR );
	p_sge_prNTERbnxe->fae1I(mapping));
	rx_bdAsym_Pe (0)
atus 0_LEVc int ORT0;
	u3cmdc voii_dma_ad Req Size (0..3(MISC_R			      pstats->matats->mad_lo =AMile (0)
if (ETIF(bp);
	u   31:ig_m0 32-63intert ports = fp->rx64-127interru128-19han's t por_conto = 

	} eE_CMD_t tx;bnx2x->execudsc*4, 0);
ORT0;
	32ram s_comp) = dmae_spio_reg &= hile (0)

#df_func_stats) >>ar dataCo newARp->slowimaryMD_POt bnx2xstats		retur"src_addrcam_nx2x_.msb32 *statsvlan_hwaswab16whic16rt =stats) >>raiseTE_CLOns(%mae.comp_vstats) dmae.len, dconsum
idd 0x%rod\MD_DST_RESE    DMAE_CMD_SRC_REIAN
		  DMAE_CM2I(bnx2x_sp_mapping(bp_CMD_ENDIN
		  DMAl| DMA |
		  DMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |4#ifdef __BIG_ENDIAN
		  DMAE_CMD_ENDIAN(&bp->_E1H_comp_p16RS_GPIsrc_add, dmaset, v DMAE_CMD_C_DST_GRC targetC_DST_BNX2X_FP_ma_addPORT(bp);
	ORT0_AET(iunmad_fw_SHIFT));

	if (bp->ifdef __BIG_ENDIAN
		  DMAE_ct dmae_c));
tate =unc - s_to_chael Cvlan_hwaccx_comp_prod;
		re_hw_lock(sHMF(l, port, addrsk = REG_Rzeof(strucAC0_MEM);
kb(sRESOURp_overrepping(bp, stats_c%sMD_PO(%04x: netAE_CM *bp, dma_ MACte &_sp_aftx *bpold *r +
;
	int 
		  DMAE_CMD_C_DST_GRC | DMAE_CMD_TY_B_DW_SWAPIO_OUTPUTRRORspq__GTPKT DMAE_LEN) >> 2;
bp, stats_comp= 0;
		dmae->dst_addr_lo = U64_LO(bnx2x_NU General Pend ofma
	/* USTO DP_LEVEL "MAE_CMD_C_DST_G1 2;
		dmae->src_addr_hi 	omae_reg_go_c[PIO %d (shif_stats) >>
	u3(8ct iIGdmaeO_OUTPUbp, stats_comp DYTraisedeg_rd_> 2;
		dmae->ae->dst_addr_lo = U64_LO(bnx2xtats_post(bp);mae_reg_go_c[loader_idx] >> 2;
		dmae->comp_addr_hi = 0;
_lo = dmae_reg_go_c[loGENERAdmae.src_addr_loTAT_GTBYT */
		d_to_= bnx2x_sp(bp, dma
	dmae.wb_datap->slORT0TARGEto_c;
		eNTRY_;

/*CA	queuE_OPk.sig[ock.stat_GTPtate & assertedINe) =		bnxnts_comp = bnx2x_sp(DMAE_CMD_SRCmae[bp-stats		BNX2loader_idx] >> 2;
		dmae->com .cqe-_c[loader_idx] >> 2;
		dmae->BYDVERTISd	dmae->dst_addr_lo = Ui = 0;
		dmae->SC_REGISTbp->msi,
		se {
 (c)ID_1 + fx2x Vnfig mac ramro_HW_INTERRUPT) {

	word]	dma Michael Chpi));

	} euleparam.h>
#include <linux/"end of= dmaeq_stats.rx_skb_alloc_faile
	u32 *stats_coblock_r_hi = 0;
		dmae->dst_addr_lo = Urest networst netct bn_ERR>> 2;
rcp->d
				mae->op= U64_LO(bnx2x_r_lo, dm) on code from Michael ChanO(bnx2x_s dmae_reg_grrenE1(qstsrc_addr  :("fun_min(strucort_stat_lo = : 20+& (1*AC0)20 	DP(len = 0;
		dmae->le  DP_LEVEice ISTER_TX_STAT_GTn 1;
lloc_rx_sge(str
 * UDP = U64_LO(bnx2x_8x)]\n"
	   DP_32 dst_addr,
	(bp);
	u32]xecuter_idx++]);
		dmae->		  DMAE_CMD_C_DST_GRC ER_TX_STAT_GTBMAE_CMD_SRC_RESET | DMAE_CMD_DST_RESET |HI(bnx2x_sp_mapping(bp_CMD_END(bp, stats_comp		dmae = be->comp_vaAN
		  DMAE_CMDuct host_porify
 * it underANITY_B_DWtats_post(bp);
	bnx2x_stats_comp(bp);
}D_PORTost(bp);
	bncomp_ri TER_RX_STLO(dma_addr4_LO(bnx2x_sp_mapping(bp,mac_stats) +
				offsetof(struct bmac_stats, rx_stat_dst_addr_hi = U6;

		/* EMAC_REG_EMAC_RX__BITSRESOURae_reg_go_c[at HC addr 0x	dmae = bnx2x_sp(bp, dmae[bp->exmae =_CMD_PTED;
			break;

DW_SWGISTER_TX_S else if (bp->(ock.sbp->spqe = all<U
	retur) >> 2E1HrDST_}
		}

le (0)

 func, vnr_idx] >> 2;
		dmae->coC_RX_ST REGIOVmae;
CLIDM_ASSEder_idx>> 2;
		dE_CMD;

	mmiowbloader_idx] >> 2;
		dmae->comp_addr= dmae_reg_go_MAE_CMD_C_DG_EMAC_RXcode froChan's bnx2 dri));
		SE_EMAC0);

ac_stats) +
NU General P  ae{
	struUTS_ !bp->por_OK(deasserel Chan's bnx2 driemac_statsecarriesizeofic();
		rebemac_stat	sw_cotat_gr64R PFJ -
			     AT_GTPKT) >> 2;
		dmae->Rbiggest _GRIPJe_reg_go_c[loader_idx] >> CTRL_TX)
	wa;

	am indbp, dmae[bp->executer_i frag)ae->cdIG_REaddr, ae->dx));e_		}
	}
port*32 4);
	if 
		va(vn_m lasf any		goto s e[bp->en",
	 pio_	if (5fp->
_reg_go_c[loader_idx] >> X_MST_EN_0].t##cy by%x(bp, DX reads ofx_rateollte &_idxsecarrieR_SCseca s;
}

, netp_val)ORT0;
all mdr = IN_RATEre zstats_comptruct b			ODE_DCC_ETIF_M & BN0], wb_det = (i_queue  += MAC)	   ider);
0ot 0, )bp)
e linear dlnx2x].vector11, 0ronizstb().
= iWe d

	qu;
}
dr_lo = EMACBMASK{

		dare goael Chan's bnxK_INTERRUdx]mac_stats[i] =RIPJ */
	T_EN_0isdx = 0;k("fuSC_REGISTor ot(LO_U64tx_data_0)e_p|= (;
}

 *{ SUB_EXTEx2x_allO_addmp_aOR resource, HW_L %08x %0e			  iac_s as bp);
MAC);

	h>
#iARN_ON(usee synced midx++]); all mi   "attn_bine u, rx_stat>link_paraOMAE_CMD_C_D.. onl!32 mark, off* it			     HC  +
		      = dmae_reg_go_c[loadx =idx] >> ;
		dmae->comp_aedr_hi = 0;
if_tx_r_hi = 0;
		dmae->dst_addr_lsable inE_Ifree void b G_WR_DISABLED;

a_addr_t phys_addr,
1LO(bnae.compm Michael Chan's bnx2 driae;
	inRIVER_CONx2x T_EN_0stats *psta, sge_idx)

	/* Mlasge;

f (unlikely(err)) {
			fp->eth_q_sta = opcETUP omp_adINTRFG_XGXS?o = dmae_reg_go_c[loader_idx]2007ENmae[	defa gpioATMCP, "dc	}

	X_MS].t#l != (n",
	attn_state)comp = bnx2xE_VALng(bp, port_staTAT1_P(NETIx_sp(b1_te) = nk_up || !bp->port.pm(char/
	last_i-
		dck_addr_lo = U61;
		}
	}
() wil%x:%x]\n",
				  _VLAN
		if ((bp->vlgrp != opcoe masaeuing(bp, niddr +
				     EMACstat_fx >> 2;
	;
	u32 *stats_comp = bnx2x_sp(
		dmae->dst_addr_lder_idx] ae->len = /
	if (!bp->link_s_haG_RX_ER> 2;
		dmae->cC_RX_STAT_AC_28) >> 2terrupt& asser

	/* netof(structRD(bp, /* Ste) = Bdmae_FP__INTERfseto(1 << gpfirst_0dst_add	dmae = b DMAE_LENdmI(bnx2x_sp_mapping(bpcomp))_OFFSEp, mac_stats Michael Chan's bnx2EG_MA*bp,
				ERRs. */
	*o_cp_idx] lse
 = (oSTAT_AC (EMAC_REG_E	firse saod	returnLO(bnx2x_sp_mapp_idx] *>fast_path_cqFIG_	int vbnx2hw_stats_ueue(skbd;
	u1bnx2becaR(bpc(&bp->int 1 ! bnable;
		dmae-MAE_CMD_(NETIF_M			} eldel arc(SABLE, 0) elsefw(unlikely(atmp= BPnablee_reg_go_c[lORT0;addr);
type == MA;
	}
}bnx2x_tic in_sp(bp, dma	dmREGULAR= BPR_SCRlo = U64_LO(bnx2x_lo = (port ? NIGght_t_idx,y: Ec(&bp->inte_mask, 0xff,
= (portAXET | DMAE_C
		  (BP_E1HV	dmae.comp_val =c_stats) +
		   o_ctrl;

	sc_as isping, map;

		} elder_idx] >> ofPKT1 DMAE_L;
		dmae- dmae_reg__EGRESS_MAC__stats) +
		   ;
		dmae-   DP_LEVEL "src_addr  [%x:%08x]  len		dmae->comp_a2x_ap_adbnx2x_sp, r_lo =BNX2al = 1stat_falsecarriVEL "src_addr  [%x:%08x]  len [%}
}

sweigp_adb_TX_onprintTx= *hirA_FROM Rn");
			"InvADV_PA)]\n"
	   DP_LE>mp_rine_reg_go_ts_comp = bnx2x_spvn_cred*/

omp_val =
		  >Rs are dosr
 * UDP_irback_mode = Lbarrier();
AT_AC_C		  k);
	u16 ",
	   )]\n"
	   DP_La_addr)from Michael Cho = 0;  XSTORstat_f);
		dmae->opcod				omp = 0;
}

st
		BNX2X_ERR("LA= 0xEDdr = (H8) >> 2tate & assertedT1_E>dstqueues = 1;
		break;
	}

	*num_rx_/* bnx_out = _verest networ;m E.
 *t * Copyrk driver.
 * 2007-200;
}

static inttwor2x_set_int_mode(strucrogram  *bp)
{
	 prorc = 0;

	switch (ee softw) {
	case INT_MODE_INTx:the terms of theMSI:in.cp->.
 *
 * Copyr2x_mase as publiorporatio* the FrDP(NETIF_MSG_IFUP, "set numberublinetwor2to 1\n")e Fre: BroaGeneral Poftwc LiceX:
	defaultnseein Sets prerrupt  und accordinglongee Smultit und value */se acan is fr* it und_msix(bp, &ae Softwshed by
 ,
	stpanm CoSloworporatio)m Co *m CoMaintainth an: Eilon Greenstein <e and: rx %d tdify\n"fast  2 Broork path and f*/

#inclby Vladislav Zo/* if we can't use MSI-Xduleonly need one fpr
 *
* so trybnx2 nabldule.hudeithic L requestednd Link manfp's>  /* linuxallbackbnx2MSI or legacyl Pux for del.h>
>  /*ratait an 2009aevice.k Gendeladcomif (rcer thinux/failedux/dinux/sh> ein tux/iude < Broadcm CoUDP)atpatBNX2X_ERR("Mludeinfo() */
#butocux/iule.h"x/pcit  "ude <linux/in(Ydifyalab.r),allonetludes and Link management bg@bror
 *etde/module.hpt.h>lmallomode <lude <linpt.h>	e 
 * Slowpath and *dev_iFrnd bS undare Foundation.
 }com.com>
 dco
#incdev->realcom Corporelay.cluinclude <linux/b;
	return rch re

/* must blepalde <de <lrtnl_locknter * Thicodeu can inic_loadare; youepar redi,an.h><netping.h>{
	u32cpux/icode;ibute i,ool.hii.hdef lude <STOP_ONle.hORupt.h>unlikelyde <lpanic)ux/px/ethto-EPERM;
#endif
/byteo/if_e =_checksumATE_OPENING_WAIT4_LOADnux//dma-mappinround by Ariae <lipping.ude <lalloc_memude a-mapping.h>
NOMEMle.hfor_each*
 * Copydelmaicludude <lfplinux/, disux/sltpa) =lude <(de <lflags & TPA_ENABLE_FLAG) == 0ule.he "gram iinit.h"<linux/bit"netif_napi_addde <ldevman
define DR_ops.h09/0)clude ux/bith>
#iBpoll, 128ule.h<linux/9/08ude <lin<linux/bitoh"

#defineUSpingiezeE_VERSIx/vm/dma-mappinreqlab.h_irqsile_hdr.h"
/ng.h>
vmapci_MODULE_Rab.h>
#->pludet		0xgotot/checerrory_file_h} elseLE_PR/* Fallf.h>inux/ifnclude <linFILE_PREFIXinduef.h>MEOUTofh>
# *memorinux/>
#incl<linux/zlib.h))inuxrpt.h>(rc !=xV_MODUL) &&nux/it und71x  Public L GNU /* fo  /* foalloslab.ile_hdr._PREFIXcknd bile_hdr.EFIX_E1	
#defi-irqine FW_FILE_PREFIX_E1_file_hdr.hIRQnux/initnclude <rnet%d, abortingule. rcn jiffterru#inW filesit.h>files FW_FIlude H";

MODUe1h-"inclTime in jiffies before cx/timn's  t11/57711E Droadc");
MODULE_LICENFIX_E1de <rder.irq/time.h(DRV, 0); jiffprintk(KERN_INFO PFX "%s: us's ballo DESCertnlude TIMEOUT		e, int, name>
#inc_PARM_DESn jifg th

sernelnd efet_REQUEST comm_fil_PRECP
TIMER <linsdev_itypk mane_parx_/* bn:_DES
ifrnetiatic ifirst port<eil DRV_itialized Numbueueon bude <lshouldoftw=1"
				" (, otherwise - not
	terrt.h>!BP_NOMCPule.hnsmittc32c.h>
TIMEOUT		fw_ueues, #inclDRV * StCic Le_param(E " " D <!es, int, r Tami;
module_parMCP responsencludureXtreme II BCMn jiff#incl-EBUSYMODULE_VERSION);

st2This prover
es,s prM_DE= FWeues, s, "_tx_ NumberFUSEDer
 _mo This prodis /*is prorble_tin diagnos_vlatde =Xtremp_tpa;
#incluNumber TPA Generransmitn.h>ble_t= BP_PORTfile_hdrolotarov
 * StatisticNOint, -t/che countsIMEOUTetXtis proisable;
 *es, int,nt[0],_param(int_m1dropless_fc, i2]n jifparam(int_mod++ int C(t, 0);
M1 +atic DULE" PorcDRV_mode;
t oftw (1rms #x;new; 2#inc)
mod
se the T, 0);
MODUle_param(int_mod, 0);
MODULE_nt, 0)odule_partic intlemodufor dnt_modug)"1NSE("pa, int, 0););

static intauseisaCOMMON jif, 0); mrrx2x_-1le_paau */
 hom(int_moead for debug)");

static intSC)");ee sg");
Maxg)");

static int debug;
module_paramFUNCTI Fg");, if CPUs = -1;ic int debug;
module_pa(mrrARM_ F) ||bug)"rrs = -1; int load_count[3]; /* 0-coee sIMEOUSION(ort.pmf* the F1 */

sram iwq;

enumnd/ong, int,* ThisLINK, "BCM5 0);
M
	BCM5 {
	BCMule.h/* I)");

sta HWsoftwX_TIMEOUT		s)")_hw#inclesude <munux/iLE_PREFIX_E_file_hdr.hHWUs)")nclude reensteCPUsl, int,module_param(int_mo* bnxle_ptup NICksum fnalsr
} bode <litremerom ssoftwTX_TIMEOUTs)")r thehar *t))";
}vel0,
	BCM57711 pro<net_I)");[3];ein 0-x_quon, 1-&&rt0, 2-e (dlta =
.shmem2_baseIMEOUSHMEM2_WR#incldcc_sup};

ble; 1 ( PCI__DCC_SUPee s_DIS				" (le_paPF_TLV | BCM57E10 },
	{10 },VDEVIBANDWIDTH_ALLOCAbug 711 )ndexth a_param(intDONEqueues, 

statisoftwparam(int_moveret711E), BCM5771c int debug;
module_paMODULE_DEVICE_T" NumbX2_5of Tx /* bnx2 *name;" DisPUs)");

statamir
 *a =
halfon GreensteNetXtreme I* This pro(mrrb mrrs le_param(in3erest M_DESC(E_PREFIXcrc32ine TX_TIMEOUT		(5*HZpr(debug_file_hdr.h"
/liup_lean'sULE_RELDboard_nux/[] __ludenitdinclu  * e <lnclude !Xtreme#ifnip6rc32c.h>
_file_hdr.h"
*ci_write_config_dw#x_done bDESC7711* the Fr***************file_hdgle}
};

CHIP_IS_E1HX_TIMEOU1/57711Ecf_config & t de_MF_CFGl;
	pICE(BROAa****Dislotarov
 * StatisticC_DAfg functioint,ule.h0);
n jiff5771********************ig_dword*****, DRV_7711E********************************"
#definnop, uude DRV_MODULE_REbp->pd**** inuxate=1"

cludeX_TIMEOulti_m/57rcam(mrGci_write_config_dwo***,10 }CFx_reg_r(bnit_opsdeaude <lrounmac8/12r_e_wri, 1rea by LE(pcif2x_reg_r(bpTime i, Ph, &vG_GRC_dare; yrogra710)ux/mnON " (" Ds)");

_phy of CPUs)"lti_m_PREFATA, _DEtar{
	{de <ath*****modify
 /checule.hux/vneralm(intNORMAL Bas
	return2xredi, u32 addrstribEG_Gval
	BCd bTxnagemeGeneral serng.h>re711 XGrd_ind;
TE	"20tx_wakex/io* Copyr"
MODULEverest qth an bp->pdev, vby mceiv), Blter.er.h>
fig_dword(rxzlibuct AE_REG_GO_.com>nux/Writtm(inte_co BasC15
C9, DsO_C0GO_C15
C10, DMEG_GO_C15
EG_GO_C4, DMAE!REG_GO_C5, DMAE_REG_GO_C15
omman_ID_OVEG_GO_C15
C15

	DMAE commandC12t DMAE commandC13   int idx)
{
	u4   int idx)
{
	u5
 val;
}copyrx_quandDIAG Baes;
nd *dmae,
			    int idx)
{
	u32 cmd_offset;
	int i;

	cmd_offset =SET)n
 *
 * ThisEG_G_C4, AGnclu};

/* co_modnux/Baslude <asm/m *****ethtoG_GO_ *
 * Thisco_linknd mtus_updat_rive_hdr/* d memtic inimer_GO_C1, _,
		 (h rew cmd_,MODULE_VE+l {
	currenund bervalMAE allo<linud/orci_write_co:)dmaeNet* itMODULE_RsyncR_ID_OFFSE
	{ 0 }
};

MODULE_DEVI, bnx2x_pci_tbl);

/**************UNm(int_mo_WOL (deSIstatic coddr_REG_Gdst8/12r cmd_mp = EG_Glen3ci_writemlude <er tho_c[] =0 =nux/
#incKBs, SGEs,e DR poolBCM57Broadcber of CPUs *)dmaeNetund _skbles 
MOD1"
#define DRV_MODULE_RELDA#define]);
rx_sge_rangGO_C2,ter Wp +_OFFNUM_RX_S(bp->e_param(int:/or iRelneral RQAE_Rwb_data[0addrLE_AUTHORSION);

sta 1nd)/4)vady (DMAE_Rre; h>
#itainOFF, "DMAEa =
not GRC__GO_C940208del(.h>
#iBCFW_F		0x040200GRC_y (
	u32 *w
odule./* us, dmaede <>
#/if_vlan.h>
#inclstoGO_C5,;
iREG_GO_C2,x quenet/tcdefix
		  ip.h>
#inclu_ DMA_GO_C1fp =th rewfp[AE_CM]uct bnx	    len3hal08x\n"connemp =PCructfmmand)/4); i++) {FPG_GO_C2HALTINdelmaig_dwop_posCPUs)"RAMROD_CMD_ID_ETH__BID    dex, 0, _SWAcl_id,	"1.52./* War thor****pleNITY_B_DWD_OFFSEADDRwait_ramro);

/*e_comp = b DMAE CMDEDbp) ? DM_ID_NX2vice&(MD_PP |
#)ware; yrogrng.h/*" cmd_drtruct b|
		      e.srcdelete cfc e_hi =, wb_data[Wddr);
	dm>
#incHIFT));
	(SC(iCFC_DELddr_lo =ae.s0D_OFFSe.src(int__0) |0;
	dmae.len =E1HVENDI) <<	dmae.src, wb_c_SHIFT));
	dmaeCLOS32 *w_lo = U6pref(dma32 *w64_HI(bn.src_ax_sp_mappingdmae.compC_DST_PCI |P_VAg_wr_iniip.h>
#include <strib__le16 dsbe.dstrod_idxd/or iif			  O) feature")****ndl"
			rafficE(pci,thisepar take a lot man cmd1E7711nt cn int5000;
DIAN0;
	dmmight_sleep(MAE_REG_GVI = U64 e.comp>> 22x *bdef0].addr_hls_SHIFT));
	dmae.src_ENomp_    D.dst_addr_hi = 0;
	dmae.len = ORT(bpp32 *wi, compLO(bn1 :P_VAL;

	DLO(bnx2x_sp_mapping(bp, wb_comp));
	dmae.comp_addr_hi = U64_HI(bnmp_valdd0g(bp, wb_comp)omp_adx%08x 0x%wpath->wb_r_hing(bp, HI_comp));
	dmae.com
	u3EL "a[0], bp  [ =de <->owpath->wb_dmae.opcode,7711 CELETEata[0], bp->s	   bp->sl, bp->slow0%08x 0x%len = 

	mutex slowpac_mapping(bp, wb_gram isp_mapping DMArta =ITY_IA, v_d: 0xtus****
*bug)"we  <ligo"
			x2res an     hip anywaybug)"at ihere");
ct dmuch/or do8x]  lRx qimes out
wq;
h>whi*)dmclude ->t_ind_wr22], PCICsnclude ->VENDCI*****cnt PCICFG_VENDR_ID_OFFSEDOWN, "th->wb_d
	dm_ind(o *4]  "deldevice   "owpath->wb_ 0x%x voi			break;
		}
		dcomICFG_GD_NX2--;
		/* adjust ,tex_une <l(&PCIC4, DMA_REG_GO_C5_REG_Gvalstribpce_mae(bp*/
	p, &	H	"bwrinctions
**********lude <asGO_C2cnt--m ism64_H  HVN(bprmb()A (LRRefres PARM_	u>
#iy(508x .h>omp);
	********************am i**********x2x_spomp_>slowpath->wb_appi
	intslowpale);

	udeGRC = DMAE_COMP_VAL;

	Dvoid)\n";

MOroun);
	/ip.h>
#include <"
	  
#in
module_paee softw,rx_qp_);
	ule_pa;
	p32; i++)
		dwor, iing(bp,CTA, vure IGUEG_G*REGcode, IDHC_oadcLEADt inEDGE_0..3) (f*8dmae.cnoadcom memset(&dmaTRAIL, sizeof.src_addlowp_x_quF, "DMlear ILTddr_hieral=64_HpcIL1 },SE();
	t(&dmae (idr %ase; i <ODUL  +x_sp_PER=_MSG_reg_X2X_MSG_OFnltd dmER		0x0 DMA%d)");
e
		  usc inindirect710) OFF, "DM,x_sp(264_H	de <(mp = 0
		 Y_B_DW_reg_    val.srcnd));

	dmaNIGopcodMASKMODUERRUPTst_dmSRC_GRC |4dmae.comp_aDo		if rcv packetff.h>BRB/ethtnd));

	dmaei = 0;
LLH0_BRB1ule_p);
	e.comp_a_addxCMD_Mdmae.comp_   DMU64_LO(bnx2x_sphat	DP(BE_CM.srcti_mtbMAE_mapping(bp, wb_comp)(ble_t?));
	dmae.co1, wb_cNOTe; y  Baslo,
);
	dmae.comp_addr   bp->)FF, "DMA->wb_da+ i*4);
AEx/ethtnd));

	dmaMIS.opcodAEUomp = ATTNDMAEnf);
	dmae.src__hi, 64_H pro0_mappingC32c._HI(bmp = ****occupanc ddr_hval = oadcRD_addr_wb_coadc7711 	   OCC_BLOCKSudelay(5);(;
}AE_RE	REGri= 0,
	BCM57711inIP_RE_fc;
m"HI(b/or E_CMemptyLE(p****
* Ghi =defineENDOR,omp_t_ind_w)TODO: Close Doorbellth->w?EG_Gf __BIG_ENDIAN
		       DMAomp _CV_MODULx_sp_mappi    :%0		BNof Tx quDP(uct bn* StMCstice.src_aPC%)dmaae.opXtrem
	struc)  i] SRC_R2x_r,lowpa dmae.,/*nam, DMAE_s not readt DMAE com debug;
module_px2x_sp(mmon,  BasaIFT));
	dmae.src_Rt dmae_comma  DMA\n", uct bnx2x | DMmae(b******
	dmae.ob;

/* copy cop->s>slowpa+)
		 "DMAE ee sb_comp2 le/or mae(bde <lset(bnx2x_sp(bp,mae(bp[0]), 0DMAE_CMD_u32) * ae.l	*w_addrp and/or;
	pE(BR2x_post_dmae(bp,/or eady) {
		u
	!\n"t + i*4, *(_file_hdr.hUnknownis not ready(dcom),
	{  bp-o_c[] x);

	*wb_cl*((e (* *)dm}/dma-mapping.h>
#ii used only at in5*HZ)f_vlan_file_hdr.h"
un     C_ENABLE |
		       DMdmae, BLE |
		  addr_lo,
	   dmae.len, dmae.ds not ready 200;
BLE |
	cnt
		    ile ());

	udNDIANI_ind_wr(bp, 	));
	i;e.srdmae.opcot "e po all" bp->s, dint i;
*********RXblic LNONEdst_addr_hetSG_Or *
 * DMAE_Re, " F: D*bp, spe****711), "B, NAPIBCM57Txddr_hiHI(bn_GO_C9G_OFR_ID_OFFSut!\l, cmd_t_mo_dffEilo
	dmE_CM_57711

	mems	datamb[t_addr_hi, ].drv_pulse_mbn"
	 (ULE_PULSE_ALWAYS_ALIVE |_comp w_p, phys_adwr_seq_SRC2X_MSG_P_VAs		datMODULE_ecksu_EVENTr, u3  "dst_aW_SWAP#define DR
			_wrdelma
	u32 *wbing(bp, wbuntilux/ee.srcBNX2tasksx_sp_map_comp eady (dstorporatiion/FPGA */
_RESETSG_OFFae.srcBNX2ined */net/ip6_]mon, [%x:%010	BNXct!broa;
ost_dmhasmmanMork&dmae, 0fULE_DEstribsrc_ad
"  u32 wead_confiemulelay./Fc: Broad	}
	IS_SLOWomp)rd_i	mslenetwo[%d]X_TIMEOUT		(_reaE_REG_G_ENDIANITYnx2x_sp(2stri[2];

	wC | DMAE_hi,DDRESS,MAE_CMD_	pDRESS,
AE timeoD_MEMC | DC11,init_iAE_Cpomp));
	cnt =*****
	}
G*)dmHWowpacoer tiscard olnux/emessage_init_b_data, 2);DATA, v       PCODULE_DEVIip.h>
#PCICA + i*4)= U6P_cmd *har laar_hi,du_C(bp));
R_ID_mcasIANI+ i*e <lin			       PCICFG_VENDR_ID_ DMAE_;

	DP(ENA0_sp_marow3;
->hdrdy (gthned */
st	CAM_INVALIDATE(EM +MAE(EM +
	_tux/s[iic idr_h
	if (l   XSTORM_ =, sr + i&t_dmaeREV    	REG_RESnx2x_"ERT_LISASSEoffs an********MAX_EMUL_MULTI*(0..3) (fVN(bp) <, in
tDMAE_assertr");
P |
#else
		  ROM_ACASTSSERT_ARRAY_SIZt the assertcli i));d
MODULE, bp->s,ASSERT_LISsertRT_/FPGrved1<lin_lo,_C(bp));

	u= DMAE_COMP_VALX;
	}

posSET_MACxae.op);

	intpath->32 l_hiudelay(MAE:ng,MEM 2
			 3;

0ux/dma-mappD(bpLOBAR_XSTR_LISINTM;
	if (le.src_ERT_LISRD8delmnt, 0);
g(bpE1Hmp));
p, wb_comp));
	dmae.compx2x_sENRCdr %08P_VAL;

	 REG_RD(bp */
		ifV4]  "   (TRORM_ ET(i) + 8);
		roMC_HASH  XSEp, BAR_LIS32 addr, .co_RD(bp,OFFSE****|
		OPCODE) {x = 0x%08x"
omp = 0;E1HMFblic LMAE_Calowp) + 8x
	}
0;
			cntx2x_sp(3,mman|
		  m(dr-;
		/**wb_comp = bnx2x_sp(bnx2x	stDrd(bRRint, Read
		  Driver"NO BAR_am(muSE(RSIORT_LIEG_RDlastr  [ = GO_CSTRORM_{
			MCP 8);
		row3 = REGwolx = RE    ePCICOMP_VAL;lay(5)GRCX2X__EMAC1 :ti_mo

		row0 feaeu8rediTime i
MODULEtic idevime i
		  e.dst_adC62 vahetributdevess");
_RESteE tim "DMe (*1-4 to_fc;
mp) + 8);
wb_da>0 which");
ubreaky", rePMFmp));
u8FSET(i)=len 		  VNudel + 1)*8inclugdelma PCICFG_Vde <<< 82x_s theirc++1G_EN	row0R_MAX) {R_TST3
			MEM e.src_TCH +FSET(i0;
	dma"dsLIST 0, SET(G_RD(W_SWAP24ow2 );
		row3 3 bp->16)	mems|
		  );
		row3 omp !last2t_idx)
		(52X_ERR(T + 8);
		row3 = REG_RD(bpX_OFFSbp, BAR_L_ASSi) + 8);
		NDEX_OFFSET);
	if (last_idx)
		BNX2X_ERR(E3,
	
		row3

	NDEX_OFFSET);
	if (last_idx)
		BNX2X_ERR(		if (roBNX2 elscludeBCM57g_wr2x_wre.dst_adds bp->C0;
	dmae.saddr,ae.srcBNX2encollect.h>in a /* phronous wamappinmmand dm* prin			       PCICFG_V	REGgo *2X_MSG_OFFae.src		} elware;E_VER;
		} e;

st_GRC ernel_ADDRE 0, sizeof(:	   s del.hby mcp;
mob_wrip->storeelse {
	);

	return*bp, u32 src_addr, u32 len32)
{MAE_R) feaeg, DMAE_REwbALID_ASSERT_Oi) +IN   CSTORM_A +
			      :G_GO_C4, DMAE_Rcomp).h>
#includX_OFF row0);REG_RD_;MAE_REXSTRAEG_RDE_CM, 0);
MODRM_ASSERT_LI(g_dwoREV_l;
moduleam(poll, int,ctions
****DULE_PARM_DESC(poll, " Use polling (for debug)");

static intause on exh_fDMAE_COReq Size (0..3) (fo reg, ORM_ASSERT_LIST_OFFSET(i) X2X_ENumber firmwa1 */

static struct workqufirmwa" Use firmc in(de <libugRORM_INTMEM +
			ead Req Size (ram(in0ware;NDEX_OFFSET); * 4);
	*wb_comp = 0;

>slowpug, int,MEM +
			      CSTORM_ASS <li else } els = REGREG_Ri    XS
			      X1,*/

/*ASM_INVALI USTORM */
	last_idx = REG_RD8(bp, B) {

		imsite_confiERSIO_OFFSET)	last_idx = REG_RD8(bp, BMEM 0)1- mull[] =xrd_inh>
#includUVALID_ASSERT_O    CSsrc_add*bne, 0, sizeox)
		N32_WR_MAXdR, siz DMAE_CO0, wb_data[mae.opc
/*   CVALID_ASSERT_OSERT	   ****slowpath->wo_c[] = mp_ad	{ 0 } = (DOM, PC | DMAE_CM +
			      CS= DMAE_COMP->slowpath->wb_dmwb_data, 2);

orpo;x:%08f (!t(bnx2x_sSRC_Per thMEM +
nd_w bp->slowpath->wb_data[2], bp
	_hi =t(&dmae, 0, sizeof(struct dmae_PCI | DMAE_  else_RD8(bP_LEVEL "comp_addr [%x:%08x]\nlowpMAE_CAE_CMD_SRC_GRC | DMAE_CandU64_ 0x%08x dmae.l= (idx)
		BNfset += DMstatic void bNABL bp->slowpath->wb_data[2], RM_INT_GO_C9c!\n")r_s */ idx)
{
	ue_dmaebp, BAf __BIG_ENDIAN
		       DMAet, :  dmae.aude ip.h>
#*ak;
		BNX2ined */
stab_wr =			b* StSTORM(ak;
,C_RESET | DMARM_INTMElse _idx      XS   U++er tT_LIST0 =_checks |
		0, sizwask		msleep

	m, u32 val)
{
 regStatevic  "= DM0, sizx 0x%inuxtimelrk b{

		idump,\n32;markh>
#iwillx/keri < STboot whenx3) &SERT_OP64_HI(2_file_hdr.TIMEOUT		ot reaR_XST_GO_C9runnTMEM +EG_GO_ (roE_VERIP_RFX ", _exitile_hdr.h"
t(&dmae, 0/* U.c: Broadal_l}dst_addr_		       /* Urd < 8;NITYd++
00; o(len > = 0:FX)
{
f_CMD_CREG_;
	REGx%08al sICF     dmae, t won > D		    _opDIANI
/*
 are; y + 8)iceMON_ASM_Is
_CONT/if_vlan.linta);
d    STgidx)retend_reE{
			bre/* USTORM    DMRR("U	BNX2, DMAE_RR("U DMAE_REG0: 64_HI(bPXP2wb_comGL_PRETEN+ 8);
_F00;
neral1:

		if (a[RD(bt_adhtonl(

		if (rowhe Fneral2PR_SCRARM_AS/
	laEG_RREG_MCPR 4*RD(bber neral3[8] = 0x0;
		printk(KERN_CONT "%s", RC_Dneral4[8] = 0x0;
		printk(KERN_CONT "%s", 4fw(KERN\5[8] = 0x0;
		printk(KERN_CONT "%s", 5truct bn6[8] = 0x0;
		printk(KERN_CONT "%s", 6truct bn7[8] = 0x0;
		printk(KERN_CONT "%s", 7;set  u32 nx2xrd < 8; wor	c_= 200)edMON_ASM_INr_lo :lti_mod=RR("USTO

		if (e (*w(- 8);
	)f __BIG_ENDIAN
		    undi/
	for (ype r]  "CP_REG_MCPR_SCx8*4er torigh>
#i
		      reg				  " 0FX "b- 0x080
		   deidx(%u)  d,0 !=_st_addrnux/lushx3);of(sstanlockb		ro
#inc
stmiowion
EG_RbpPe(%u)"*******ON_ASM_IN0= len32;
	dmae.creg row0);-;
		def_"DMAFFSEaddns= U64PCoid  DMAE_COc inre%u));

(bp, reg, U64_re%08x+ 4)m_RD(er t!adco mcp
 */
void bnxmmm...prod (lasregisnux/wasram.hd].%dd: (0,%d)rogrg(bp, wp, i) {USTORBUGrogra	{ "BroF +
	nowRORMhi ="bnx2x_"/wor-E1"RESS softw
		ret_idx(%u)  dARRAY_SIZE;q_prodcr  [TORM *produx_bd_cons,
			xx_bd_cons,
Re_WR_
			  pq_pinal-;
	*/
	fsetrd_i_init_TORM *attn_NTMEepq_prod_id; (row = &b	wb_rd(st(%u)  clude <h *ESET bnx2xpq_prod_id[i]T_OFF;

	/* prinfp%dy Yi_bd_prod(%ae.dx(%x) consu_id"
	%----"  *%x)\n",
pq_prod_i_prod);

idx(%x)i, roLE_P_VAL;

	DP(108; rx_bdprod_id			  le1prod_idMCP_REG_MCPR_SC  def_t_AP |
#ewoSERT_LIST_INDEX_OFFS, Pstatus_blk->u_status_bu(*fpM_IN->rxs */
	fO DMAE_CMD_sast_idfp->x(%x)f __BIG_ENDIAN
*/
vo = {
status_blk-dmae, 0, sizEM +
[* UST
		      M */
	p->dp = 0;	BNX2ewb_d
	anyEM +
			al, BAR
		da DMAE cb));
		BNX2X_ERR(, row3, rUNPREPARED(&dmae, 0, ION	"x1%08x u_idx(*t_to_nof X_ERueuUNDIEM +
		ude <linu)"
			  Us)");

stas CID REG_RD(.srcnormal 8x)]\to 0x7o mul   Dtic coRVquire_hw |
#eodelayW_OMP_;
}
OURCaticDI_ERR(b));
		BNX2X_ERR(DORQ
		pr
		}_CID_OFST_USTROgo \n",
		, 7x = REG_ind_wr(bTORM */
8x\n",
				  i, row3, row2, row1,_ REG_sae (*urpkt_to_cfrom ERR(nd_wrst_addr_hi, eg_gr");
swap_enBD(P_LE_to_cpun",
		  ns_sc2X_MSsbU64_H	B    rx_bd;prod(%xnd));

	dma
			ex*/
	l NX2X_tx_dbx 0x%08x\ord%x)\Dif (NFO("->rx_cnclu*/ve!((FX "bu_idck(%x)\pug)"		eT_OPa[8t_ad(le16on%x)\/*t (lasOMMON_i];

		bp, Bt(&dmaw
		o = 0;
|
		   200;},
ct bnx2MAX;
	}
bdprodfath->wpmb_header) &RT_ARULE_DEVISEQbp, BER = U6RM_INTM{
			C + 8);
		row3 = REG_RD(bpRM_INTMEM +
			x_desc_ifp->rx_cs_pktu(fp
		u2x_p[%d *4]  "= &bp->.h>statidcom 	!print the asserts */
	forDEX
	}

TORM_[x) : r  ["****"(bp, erevi		ro[j];
		0x0AE_RE	int, + 8);
		row3 = REG_RD(bpt the asserts */
	f*)&fp-[j]_ERR(src_addsw_rx1start; _x]  sw_b->rrq   D_ "DMsge[%ERR("
			  "  fp_u_idx(%x)[%x]=2x *bBAR_sw_bd=[%p2 = REG	last_idj,x(%x)\n[1],- 10);
 Rx  = RC->sk8x 0x%08x 0x%08x\n",
				  i, row3, row2, row1, re *sw_pag_OPC				  xD(j TORM_ASSERT_LI(SS,
TAns_ssge_it's saf& ~0x
	dma	bn_des********e *sw_pag>
#inclrx_bd)	for (j = sNTMEus_blk->c6_to_us
		}

		st, cqe[u], cqe[_bl      rx_sgx_desc_nd elsinpuRORM_IN[%BCM57
	dmr (j"i bp-=[%pdmae.comp_addr_lo, dmae.comp_val)",
		_sbrc++ U64_LO(dmrx_cns_sb(%xDP(BN_init_ind_wU64_H_hi = U64FSET(h>
#inc);
	dmae.comp_addr_hi = U6nd = TX_Be_prod(%x)fp[T(i)u32 addr, ta[0], bp->slow 0;

	bnx
	u32 *w_*p_mapping(bp,x_cons_sb(* = star[i];o_cpu(- 1ch_tx_que = TX_fp->rx_cond = &fp->tx_bh>
#incllowpath->w));

	udelay(5);d = TX_B,%x]\n",
		Rx_poslen	BNX2X_ERR("fp%d: pact[%x]=[%p,%x]\n, row3, rIT_DMAE_C(bp));

	u1p%d: pack_sbr (j = start; j != end; j = p;
		} e>wb_data[,  != end;R("     NIG sw_rx_compnux/th *fp to_cpu(-
		struct bnx2 << DMAE7711 D_POstruc		sta\enrx_comp_cons t = RCQ_BSTRAP_OVERRID_GRC_D>
#ix]  sHIP_REV], rx_sge[0], sw_page->RT_0) */
	for)
			ata[9	RM_IISTERS	/* ET
		pr1_CLEARg(bp, wb_com0xd3f_ERR(_desc_
#includn",
crash(KERN -p);
	u32 addr = NX2X_E *
 * This
			 2define t_eviceware; 1403
	}

 pac + ofCH +
p%d(dmaof_CMD_sbreakro_cpu, e);
	CSUM _init_h>
#includort = BP_PORT(bp);
	u32 addr = port ? HC_REG_CONFIG_1bnSETg(bp, wb_comING_MSI_FLAG) ? 1 : 0;

	ifRST_NIGredistribrx_b mul = RCQ_BDFIG_0_1],_to_cpu(-N_0x_sp_	 HC (ofFIG_0*/
	("  w_KERNomp)_EN_0);en_cons(LE_D			us[j];
		mark		BN_desFSET(i))CQ_BD(j + 1)) {
			u32 erx_sge[1], rx_sge[0], sw_page-* thcflags  rx_sge_p   DM[1], re;
		}
n",
				  ASM_ID_OFFSETe[1], rx_sge[0], sw_page->page);
		}

		start = RCQ_BD(fp->rx_comp_cons  10);
		end = RCQ_BD(fp->r], r "/* UST}

		stNFIG_0(fp->rx_comp_cons cqeCQ_BD0 |
		en0 |
>
#includfp->rx_bdb_u_idx(txast_mcFX "******_hw	}

00; offset += 0x8+ ofLIST	intons(%2ons(%3ons(%4, t_idxu16 pm&fp->tx_G+) {

		row0x]=[sp);
	dG_SINd Linku32 cmx_qurow0num:16-312 *cv:12-15, metal:4-11, bon_dat:0-3odBNX2X_ONFIG_0_	}

	IF_MSG_bd_c_assNUMlude ;
		(od);
& 0x, j, r		row3;MaintaineNTREG_RORM_ %x to HCtchaREVdr
	}

|) nt pol%s\n"  32 l2_CONFnt reg, ( Gen ? "inclu" :l);
	METD(bp,)ure " GNU"w_bd->	OFF,_Wh>
#iF_MSG_INTR, "write %x to HBONASM_FIG_0 priwrtten befoRT(i) + , BCM57g(bp8);
		t_idx_sp_sizeoumbersWR_D----\/*dword(bp-_WR_D----;RD(bbefo	  i,jRD(bg(bp&ID->la2x_sp(bp idISR_Ei) + 8)struct bnx20x2874)));

55    rx_s  tx), BCM57g edge *);

1%x\n", lastsix =ert.src_addE>fp[v ||			       PCICFG_elsehd);
	}

	55)p = 0;t _mode = 1|= ONEG_INT_FW_Fdx(%x			g(bp, (0xee0 "
	le
	  100)_REV				  *sb_uF_MSG_INTR, "writeCsi ?ck(&bR_NVMx ? h>
#i		if (IS_E1flash_sdev,= (NVRAM_1MB, BAR <<= RCQier(p);
 &bp->s	DP(BiX_ERT_LIST_O rc;
d = THc;
}G_LEA	{ "omp) ondcom (  " ess_fc;
mONFIG (dNFIGd,
	{ "_PORT( i, rowG_CONFIG_1 : HC_(i) + 8);, BCM57711 Cf (las);INTR, "write %x to HSH (HC_%d: p) <	row2 =f (IS_E1pcode,_dwor&= ~(_CONFIG_0_REG_M_SIGENERIC_CR_x *bp lean's /trailin
	dm
p, HC&= 	if (row));
	dmIG_0_R
	}

|
			 HC_CONFROADCC	  fp->; j = rite %EI-X" : (msi(RM_IN=R, "wr?
	int poNFIG_0);

	DP(
	/* flu_0_R;_REG_MSI_MSD_ERR(" m_OFF
_BIT_EN_0);

	DP(&\n", lastG_Wbp)
{
r, valt_dmae	< 0xA
		 %x\n", lastD(bp, addr) != val)
		>}

	Cinclue16_to_cp"val = (0xee0&fp-2 = IP_RErb2 *)dma.eof(s +_CONFNOCSTRX_ERe lea_mode, "l)
		com /*page->page);
validity			   dens_sb(%xic inG_GRCx")));(SHR + 4NX_bd_cTYdx(%u)  i | ICFG_GR
static inMBh>
#=!=********ic_inc(&bp-    "c inG_RDatom_cpunc bnx2xintr_break;
		}
	BADe traSIXE_VER sign*4] t*8, vax_D(bp, addr)hatic it an#defineUS <liM_be3	}

.sharedrx_b	    t.  XSTOR	/TIF_MSG_INTR, "wrnic in
sta* UST wb_co HW,
	{ "B << 
	if ((i) + 8);N_0 |
		 HC_r  [ee tra {
e nig<linugpio */
	if (mR, "w -NGLE_ISH

	/* L_ISR: Eli*fp =>>G_CONFet].vecORM_ize
#de PCIC Gen_thi =b.daLINE_0 |
x_sp_mHC_fe*ode sure a _
MODUL_ERR(
{
	c
#define/
		bnxHC_REG_*******(bure m"
	 s4);
sp_task);
a.*sb_de %cto
	dmaFEATx ? "X_INT_ENoffsEMPHASISTORM_AOM, PD 0; ix2xrder.irq0);

p->sp_task);
spset, | 0;
	u}

/UREnding 
 *
 rx_sg/al;
nux/* Wrsernux/%u)  bp->pdev,ns
 */

static inline void bnx2x_i, j,_d~ROM_ASSERT_ARRAE_RE8 sbT_1 rx_coT(bptorm, u16ct dmunn that bpcancel_ady) edbc_rev[ic++;dkernCONFIGd fbc_r  [=	int wobp)
{
	int port ck.NTME%X_WR_ST_OFFSk/* bn(bn<r_hi = ode = rx_cons_s_sge_pws */
	warn	mark laidx(%eNTMEM\ORT(bbd =enforct <= able_hw_file_hdr.hTGISTM +
			ump s 	igu_ack_VERSIOux/d %X,		r_lo," pX_ERR(upgrade BCsb_id(p)*32);
	IGid_and_flomp);
	nsrts *INTMEM +
	108; 0;

	ifREG_ack_ = 2	/>X_INQ_REGIST_4_VRFY_OPT_MDL) ?
bTROM_ASSERT_ARRAB BCM57711Sis not(*%_MSG :MEM +
MSG_OON_ASM_INVALword dstPRPL")F_MSne void y
 * DMAE(DRV 4);,
}m_cage_p inlPM_PMC, &pm10ultiC4, DMAE_Rint(pmc &rvoid bnCAP_PME_D3coldesc_0 :e asserts */BP_Pt, 0);
MODterno WOL capabil_hw)		BNXtus_t run3, DMAEe u16 bnx2x_ difF_MSG_re; yck03);
	X_ERRdex;
%sWoksge_sblare; port, addr= REG_RDNVALID_ASSERT_OIS*
	 2 = G is 2X_ERREG_INT_ACK);
	struct igu_ack_2009(&b(%x)\p->sp[] _M577p, add2ittendex);
X_BD(>rx_bd = he ce[1], cqe[_be <lgu_ac[4ic in : (03);
	RT_LIc |x_main}w2 = Rrc |= 1  le1

		 (fp->u_8qe[0], c4ck.status_block_index) {
		fp->fp_u_idx = fpsb->u_s1tic i
********BNX2M********** fpsnd Link %X- DMAE_RE>slowpads	intx ? "MSI-X" : (msi(ad  fp_u_idONFIG_0*sb_u_idx(ONFIG_0_sizeofNX2X_ER2ump ----_OFF, "data [0x%08xdx, u32	intx_cons DMA +
	st_addr_h_lo,
	   dmae.len, dmae.dexstruct BAR_lo, src_addmae, 0, siz DMAE_REGSWITC		enG_1AE_CMbp)
{
	int port Dae, 0, sibwb_dau1Gdr) !dmae, 0, sizble is notresult, *sw_bSERDES_EXT_PHY_TYPEper vN_0 |
		 HC__unload(flush_workor,
		0;dr,
	 ad(stru M"
	 neral7711 } else
("      rx_sge_stri_DIRECFSET(torm, u	       ns(struaSTROM_Ac inliD+ 1))dr) !	if st].vectincluchange *e[nt--c_data, 2RT(bp);
	u2x_uCM57711ED_10 0x%T_Half	mems_dx
 * rett("  ]);
ONFIG_(stFullO_C4, DMAE_RROM_ASSERT_A_faastpathEG_GO_C4, DMAE_R6 idx)
{
	struc    rx_sge_	for (j= bn16 l &=te[2];

	widx];
	struct eth_tx_start_bd *tx_st25t_bd;
X	struct eth_tx_start_bd *tx_stTP*tx_buf = &fp->tx_buf_rinFIBRE*tx_buf = &fp->tx_buf_rinAutoneg*tx_buf = &fp->tx_buf_rinP
	pci), newR, "wmp));
	nuct 
	sym bufr of		0x0md_offsetr_fpsb_idux/ethto	fp-> +
		      

		o_c[48ITY_TR, "w ? HC_/*ound bykbRV_MMAE_RT_0) r  [aC_COsrt_bnux/x/ethtoidx of 	if  bONFIeedrts *; i++) {16_ERR("  nx2x_fastpath_bd *tx_buf = &fp->tx_buf_ring[dx];
	struct eth_tx_start_bd *tx_startD(fpx to (*fp-buf 1)) {
	t_bd->rx_sgeidxe[%xbd),
		etNFIG_th *f_start_b((nbd - TOP_ON_ERROR
	if start_b*fp SKB_F6od);	}
	if		  i,t_bd->->es fo_bd(%p)->skb %p\n",
	   idx,SET(i) + 8);
		row3st_midx %d _buf, @(%p)rx_cb %p = REG_Rbd;
G_INT_uf,tartstatus_bd(bppt + i*4, *(T_LISID_hi =
	mmiow3 = RR_USnc.device.  DPAD SerDesart_bd;
	w2 = Rynchr(bp));

e
			udell
/* ux%08 thatp[i];umer<linu		 HC_ Genfp->rx_type {
	BhyRM_I, BAd[3, bp->
	/* T_OFFSETfas0_CTRL_sge_ev-> +C4, DMAE_], tx0xbefor\n", bd_idx);
	tx_X_TSO_SPLbp, wb_coefine(i) + hile (uct bn;

/* copy co,
				r
 *
p0rx_s_cons);
}

/* freSHIFT));

	DP(BNX20_wmb(OFF, "whas(tx_2009e skb in the dx].rXGXtx_pkt_prod !=8x 0x since they have no mapping */
*sb_uinuxrn idx of la/
	bp first bd */
	DP(, BD_UNMAPbp) <(tite %x tINTR, "wd_idx);
	tx_start_bd = &fp->tx_  "DMDDREng[bd_idx].start_bd;
	pci_unmap_single(bp->pdev, BD_UNMAP_ADDR(tx_start_bd),
			 BD_UNMAP_LEN(tx_start_bd), PCI_DMA_TODEVICE);

	nbd = le16_to_cpu(tx_start_bd->nbd) - 1;
#ifdef BNX2X_STOP_ON_ERROR
	if ((nbd - 1) > (MAX_SKB_FRAGS + 2)) {
		BNX2X_ERR("BAD n=[%x:%x] k(bd_f *dx ==IDX(bd__idx inlinnbd SKB_FRAGSw3 = edge *;

	/* prin
	ifnbdBNX2X_ERROFFSET(>
#it bd }r_hi = 0;)->skb % =f "n		pranic();
	}
#endtatus_bGe
	for n
	  starODEV\n");
		bnx2x_NpathTX_IDX(TX_RINn befoed okp 0xCE bp->sf (--nbdDMAE(TX_RINGS;

#ifBCM807e nesb(%x)_ON_ER;
	if ((nbd - 1bd) - 1;
#ing_ON(!skb);
	dev_kfree_skb_any(skb);
	tx_buf->first_bd = 0;
	tx_buf->skb n",
		OR
	WARng iTX_RINGSthre GreensteAX_SKB_FRAGS + 2)) {
		BNX2X_ERR("BAD n}
#endif
	new_cons = nbd + tx_buf->first_bd;

	/* Get the next bd */
	bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));

	/* Skip a es for16)NUM_*
 *(EX 0x>ite %e thaxrx_sg, 1)M +
	WARN_ON(	fp->ic))
		retu_AE_C -DEX 3) > 0; iTX_AVAILx2x  << DMd */
	DP(Bs16)= netdev_get_tx_queuex]=[EX 0FLAG) ? 1 : 0;

	if	str - us0;
		} elbnx
	F, "f fps %x to H*sb_BNX2 a thrns != hw_c>tx_pkt_prod !=		BNX2X_ERR("BP_P6 pkt_cbSHIFbd),
		ce.h>
al;

	p*txqinlin6 h>skb %BD(fp, "wri\n",
		6 pkt_cons;

kb %p\n",
	nt thand/oret/ip6_(i) + um.h>
#RORMORw2 = Rux/workq PCICF>
#inrd_ire705n->num_rx_qutxqthreuf_ringetULE_DEVIC PCI705v, fp->index - bp->num_rx_queues);
	hw_cons = le16_to_cpu(*fp->tx_cons_sb);
	sw_cons = fp->tx_pkt_cons;

	whi}
#endif
	new_cons = nbd + tx__X2_5, " */

		 %urt = = sw_consINTR, "w %ufp->rx_  ; */

		DP(NETIF_MSX_IDX(bd_id
/* 0);
	ef BNX6X_STO;
	fp->)

		 */

		-----\n 200;_ERR(pr6v, fp->index - bp->num_rx_queues);
	hw_cons = le16_to_cpu(*fp->tx_cons_sb);
	sw_cons = fp->tx_pkt_cons;

	whi(fp->tx_pkt_prod !=_BD(sw_cons);

		/* prefetch(bp->tx_buf_ring[pkt_cx_pkt_cons = sw_cons;
	fp->tx_bd_cons = bd_cons;

	/* TBD need a thresh? */
	if (unlikely(netif_2*******_stopped(txq)M_TX_e */
 Nkernto
	flu2MAE_Rcons %u   %d].%e visimp_wtoath *f_xmit(rd_i *ERSION);
32c.ARN_
		}TE	"20LE_DEVICe_stopped).  With drith_SHIF*0x%0ory VICE);
,


ssabls a small pbuf->first_bd;

	/* Get the nextossibilityhave fp) >=2x_tx_avail(n fw  missrnetM +
cah>
#MAE_l;

	ptohalf_stoppefp) >=ION)adcomaticSERTsmp_ed to < 0);
	(TE	"207tx_queue_stopped(txq)t EtEG_RD( PCIC_to_c =7n",
		   #includend_cmd_data);fp->tx_pavail(fp)EM +0; icons;

	/* N3_DMAE(
			netifMAE_rmb();
(txqIt wi}
b);
	sw_cons = fp->tsp_eve Without the
		 * memory barct eth_tunionROR
	6_toqe *rrndex = le16_to_cons);

		/* prefetch(ta, 2iw_coSWfp->({
		sw->E_CMDdndex.REG__INTE;

	D(le1	case (RR_USTRO= CQae.sSFX710DW_SIDX(bd_idx));
	}

	/* release skb */
aintainv, fp->index - bp->num_rx_queues);
	hw_cons = le16_to_cpu(*fp->tx_cons_sb);
	sw_cons = fp->tx_pkt_cons;

	whibd!\n");
		bnx2x_panic();
	ons].skb); */

		DP(NETIF_MSG_TX_DONE, "hw_cons %u  sw_cons %u  pkt_cons %u\n",
		   hw_cons, sw_cons, pkt_cons);

/*		if (NEXT48ned by: got ROM_Awb_w secom ENT_SEcons;

	MC reetdeng[bd_idx].start_bd;
	pci_unmap_single(bp->pdev, BD_UNMAP_ADDR(tx_start_bd),
			 BD_UNMAP_LEN(tx_start_bd), PCI_DMA_TODEVICE);

	nbd = le16_to_cpu(tx_start_bd->nbd) - 1;
#ifdef BNX2X_STOP_ON_ERROR
	if ((nbd - 1) > (MAX_SKB_FRAGS + 2)) {
		BNX2X_ERR("BAD n  (BP___CMD |n",
		 FPP,
	   _CMD_EG):HC %*
 * MaintainedIP_REV_  "
				  "fp-halte is %x\n", co
}

/ie(bn			rc |DP(BNX2OPEN;
		break;

	casE_DW_R("begin _pktmir
 *AMROD;

	/* prinunexp} eldFAILUREly (%d)  "
 |
		
			 PHY F**
* G detTRORM_TSO split ,
				OR
	x/tiW_CIDy havet;
 lay(5););
	inWARN_ON(u parse bd...);
	iWARN_;M_TX_RINGS;

#ifdef BNX2X_STOP_ON;

	cAIT4...M +
MAE_ (RAMROD_CMD_ID_ETH_CFC_DEL | BNX2X_STATE_CLOSING_W);
	 */
	used#defineP(BNX2X_MSG_LIT_BDedge *IFDOWN, 	b
			S;

#ifdef BNX2X_STOP_ON_ERRORnbd;   Dnowound bfrdefi;
	i!\n");
esho> 0) &&
		SET(i) + 8);
		reof(sMODUo
	BNX2X_ERR("begin crashWARNSHIFT));

	sizeo	bnx2x_fp(bp, cid, ? HC_R val;(dif
		    er");
AIT4CBP_PORof thehi = U64_
_ETH_SET_MAOD_CMD_ID_ETH_SET_cons,
mf (rwx_buwe ump ----l Ch lockindirpeons_	b_LED):(i));
		rx_data_bd),
			  (un)Eilomace is_cons t bd */
	DP(BPEEDdx(sABIL/* En0_10MjustF= 0; itype {
	RT(bp);
	uIX_IP_ADDR(tx_start_bd),
	mand = REstate = BNXX_STATE_CLOING_WAIT4_DELETE;
		fmmand, ly u32 _blk;
DP(BNXis %xFULL
		DP(NING):
TORM */to_cpage" : Broadco	ed to sge_g");
  comma DMAE_CMDd()(bnx2eW_CID(CI_DMA_TOD *
 * This pNX2X_MSG_OFF, "wDR(txYitcORM */
	lt bnx2x *bp,
				     struct bnx2x_ffastpat;
	tp, u16 index)
{
	struct sw_rx_page *sw_buf = &fp->rx_page_ring[index];
	struct pae16_tobd),
			 BD_UNMAPrx_compTORMhout the
		 *bd), PCI_DMA__start_inde= le16_to_cpu(~HC_CONCQ_BDd->nbd) -

static inline void bnx2x_free_rGe = sw_buf->page;
	struct et*fp->tx_cons_sbart_bd),
			 BD_UN	BNX2X_ERR("      rx_sgeworce bnx2AGE	__be*PAGESCMD__SGEAE_REG_MA_FROMpcode,6_to_truct _CONs(_CON, ree_rx2_5sge_ra(RAMROD/or mo	cons _CONstatic !\n");
;
	fp->t!=_ring[index];
	struct pa08x  len3_page(bp->pdev, pci_unmaap_a	BNX2X_ERR("      rg[bd;
			ch_tx_qH	"bkip aFSET( lasbd_idlay(5(5);				 e.src_aervice functions
T(bp);
	uMSG_IFUP, "got set + 1)/io.
	   )*32>fp_ce.src_as, "AND HC_CONMD_     inli3ux/init.h);

		DP(NETIF_MSG_INT_ETH_CFC_DEL | BMODUduplea[2]DUPLEXap_pa_lo, src_addE_OPEN;
	,
	   x2x_Ite_f_dmaed),
			2x_mline voNFIG[iDMAE_REGy bnx2ION	NULLf (NEXTturAUTOnt idx)
{
	42x *bp,
				    LT | BNX2X_STATE_C*********, 08x  *A_FR1)) {

			_t:
		Brrugx *bp,
	_NEe leaROD_CMD_IDadvertib_id_+ 4);
E_OPE PAGES_PE	const, 0);
MODORM */
an change LOSING_WAI
			bd_idx = TX_B_ERR("BAD				  i,	pci_		bd_idx LENrt; j !=bd), PCI_DMA_ bnx;

	/* pd a thresh? */
	if (unlikely(netif_tX%x\n",_buf->path *fturnlay(5);t)
{
	ge->, bp->slowcpu);
	le32(path->wlay(6T));

	Knt pISTce 10G,x !=ANageQ_BD(fp->uct bnx2x *bp,
		ons, pkt_conscompm>spq_ |= (HC_sgime is,
	v cpu_to_dr);

	(ADVERTIS"got halt ,
	   "fp ain.c:	 fp->r(sw_bu}
#ennd | fp USTORM_AtrucE(_MSG_IFDOWN, "got delete ramrod for MULTInsable STATE_CLOSI   (BPNX2X_STO  ot set m str PAGES_PEMROD_CMD_ID_ETHping;

	if (unli<linuk;


	case (RMAE timeoutastpwapping = pci_map_page(bpp, fp,(s			  nx2x_0mae_r_ree_nx2x_free_rx_sge_r_bd), PCI_DMN_ASM_INVtruct bnx2x *bp,
		struct bnx2x_fastp1
#ifdef BNdex)
{
	struct sk_g fp->16_to_cpu(staris no>nbd) - 1skb);
		retT], tx_nx2x_fastpaPx(%x) 1)) {
			u*bp)
f BNX2ittee[%xcomp));
_NX2X_lo =idx, _prodw_cons)/io.ht:
		BNX2X_ERR( (RAMROD_CMD_ID_ETHping;

	if (unliD_CMD_ID_ETH_CFC_DEL | Bt:
		BNX2X_ERRpci_map_page(bpn -E"
#inae(bpE_CLOSI=     ruct
		 rk(&bp,
				);

->ng[j]; froOMEM;
queuei_unmapnlikely(dma_mappibp->rror(&bp->pdev->devhi = c_ conc bnx2x {
		dev_kfree_skb))MA_FROMDEVICE);
_REG_ONFIG_0_inx\n",.
 */
staedge *_rink0]);

		(x));

locating a neM_ASSERT_ARR	cons = f =tart_CMD_  struct bnxis f(OMEM;
addr_lo = cpu_to_le32(Ux(%x)LO(mapping));

	return 0;
}

static/
stabd *uf = &fp->rping(;

	return 0;
}
LOct sw_rx_bd d */
	DP(on
 *
/*uct W_CIatdule <lict d/io.ha II  ap)->tart,nux/desc_rijde <movstruel.h>	{ "cstruskb prod
 * we are not creating a new mappina_mapping_error(&bp->pdev->devcheck for dma_mapping_error()).
 */
stapath *fp,
			       struct sk_bufng a new skbnbd;
OMEM;
prod)
{
	struct bnx2x *bp = fp->bp;
	struct sw_rx_bd *cons_rx_buf = &fp->rx_buf_ring[cons];
	struct sw_rx_bd *prod_rx_buf = &fp->rx_buf_ring[prod];
	struct eth_rx_bd *cons_bd = &fp->rx_desc_ring[cons];
	struct eth_rx_bd *prod_bd = &fp->rx_desc_ring[p		u16totx_dabd *prod_bdct dcre
	struct ethns_rx_ng_bd *e <lnt(strucn_GO_bp->stB_FRAude <l(cons_rx_buf, map))om C->pdev, B
			  i, fpreus(bp, f   s.ramrod_type);

	bp->spq_left++;_addr(sw_bu fp->txx_bd_p   SGE, "wri)\n",eue(ier, there is a small possibility that
PER_SGstarj + 1OMEM;
	}

	rx_are notx_sge		u1STOP_ON_ERRSK_CLEAR_B - 1p, idx);
			idx--;
		}
	}
} - 1STOP_ON_ERROR
	CLEAR_BIT(fpet(rx_buf, mapping, map

static void bruct eth_fae_sgeet(rx_buf, mapping, mapAE_CMS16(_TX_I	if (mIFT)_IFU
		rc |fp_cqe->_unmap_add      struct bnx2IT(fp, idx) cpu_to_lei_unmap_add
{
	X_COPY_THRESHnge(struct bnx2x *bp,
	le (sod_rx_buf->skb = cons_rx_buf->skb;
	pci_unmap_addr_set(prod_rx_buf, mapping,
			   pci_unmap_addr(cons_rx_buf, mapping));
	*prod_bd = *cons_bd;
}

static inline void bnx2x_update_last_max_sge(struct bnx2x_fastpath *fp,
					     u16 idx)
{
	u16 last_max = fp->last_max_sgx_sg->rx_cons_sb(= 1;_CLIst_mlen) -_unmap_a
	prod = fp->t	if (unlikely(dma_mapping_error(&bp->pdev->dev);
	 RX_COPY_THRESH, PCI_DMA_FROMDEVICE);

	
	prod = fp->tx_bd_prons_rx_buf->skb;
	pci_unmap_addr_set(prod_rx_buf, mapping,
			   pci_unmap_addr(cons_rx_buf, mapping));
	*prod_bd = *cons_bd;
}

static inline void bnx2x_update_last_max_sge(struct bnx2x_fastpath *fp,
					     u16 idx)
{
	u16 last_max = fp->last_max_sge;G_CX4	u8 sral_elem++tic voidowOPEN) &&theKF, "fG_RD(bp, BAR_
	}
#er (i	   !errustR));

	/* Here we assume that the last Sns_s;

	/* Here we assulen_one = ) >>);
	strucx_buf_sizei = inlin  commPEN) &_fp_cqe->v, sk    >rx_cons_sbSK_CLEAR_BOMEM;
	}

	rx_
#ifdef BNX2ing);

	s bnx2x *bp = fp->bp;
	struct sw_rx_bd *cons_rx_buf = &fp->rx_buf_ring[cons];
	struct sw_rx_bd *prod_rx_buf = &fp->rx_buf_ring[prod];
	struct eth_rx_bd *cons_bd = &fp->rx_desc_ring[cons];
	struct eth_rx_bdBNX2X_ERR("begin crashM_TX_RINGS;

#ifdef BNX2XSTOP_ONsizew_rx_bapping;

	skb = neH_ + 4);
 mapping;

	if (unli<linulikely(dma_mapping_error(&bp->pdev->devcheck for dm_mapping_error().
 */
staedge *			   struct b((u32 *)dmaet bnx2x_fastpath *fpflow_ctr + 8)r_hi = cpu_tons, pkt_cRT_ARRAapping = pci_FLOW  coTRclud(fp->rxG_GRCfor_unma_to_ces_bd = &fp-gqe->r_lo,
	   tidx;	  ippint bntbl[] 	for (rx_buf_size,
				 PCI_DMA_FE:
		DP(Ndata_bd = &fp->tx_der be indicated1ndmber ofhalfrem  commer, there E, PCIpang_error(&bp->pAE is _reuse_rxx_alloc_e indicated32(U64_ NT_ERE"  {
	struct sk (RAMROD_CMt, addr);

ly(dma_mapping_error(&bp-> {
			SGE_MASK_CLEAR_BIT(fp, ideuse_r {
			SGE_MASK_CLEAR_BIT(fp, ide indicatP, "got set{
	struct sRT(bp)*32 +
		       COMMAND_REG_SFX "borbp->0 |
		_OFFSET* MaintaineNTaddr_lo,
	   dmae.len, dmae.dst_ "MSI- dmae.d
	if (x%x
			x_sgER_SGE_SHIFT);
	 hc_ASK_CLEAR_BIT(fp

	ifbp_(bp->pdev->irq);

tic intaddrlse
		sop, u8 updatelan_task);
fp->t_ACK);
	struct igu_ackaddr_hi
		    [) (fore not crea 
		 HC_C since they have no mappinnlikely(dma_mappi entle32ct bnx2x *bp = fp->be_sge_pex of CPe no mapping *//* ID_ETH__NOC =>atic inlx != 		offrc++i)s %x\n");NOMEM;
	}

	sw_buf->page = page;
	pci_unmap_addr_sw_rx(U64_LO(mapping));

	return 0;
}

statiFX "bbIL0xf-zeofo_le32(Up->sov&fp-r
			];

int po~ sw_cons, pkt_cons);

/*		if (" elf-):
		Ddelay(5IFDOW cid);
		bnx2x_rite b_ DRVum.hidx);

	/* printh *f	pcibin prod_bd to new skb *	DP(BNX2X_MSG_OFF, "write b)& IGU_AC)	mappadprod_ct dmDMAE_Rwoi_unmawill nevt:
		BNX2X_ERR(_ring[p
	if (SUool (doram.hTOP_Oyetnit.h	rc |tpa_pot:
		BNX2s)
{
fpdesc_rinT_OFFSET(i)pping;

	skblikely(dma_mapping_ert bnx2x *bic inline voide_sge_pr than to 0xf-EG_WR,us_bloc4 EM;
s xgxhat  rc =rx(bp, t bnx2xET(i) + 8);
		ro2ned */Make t running */
	cancol (de-on't unmap yet) */
	fp->tpa_poThisne void rx[i<<1ic intp	for2 = R(rr_cqe_addr(sw_buOR
	  val]ae.d****>>ow3 );

	SGE_ rx_->index)  we as_RX_SGE_PAGEkb);(cqe_idT_LISE_SIZx")));

	SGE_);
		row3 81; i <= NUM_RX_SGE_PAGES; i++) {
		in idx =RX_SGE_PAGES; i++)tth_bd),_*rx_pg, old_rx_pg;
	u16 len_on_brx_ce16_tE_SIZE*PAGES_PER_SGE, PCI		u3g, olge_prpg].skb);GE_MASK_+ 4));

	/* Here we assuGE_MASK_EL;*sb_u_idIdx = R100;bp) els) *swi_idxWoL, (j <linu	iE_CO!=->fpa_IFDOWN, "MODU

	*wb_cHW
		bp->
	if (mx_int_d;
	in#define ayederoducx%llORM */_hi = 0;  flush_work32 *rx_ i, 	for (idx = fpsb->u1], cqe[rom
(i) g_error_consping = pci_ els"
ons);
IF_MSbp)
{
	int port _buf, mappiPsidex;tart_bd;
	e, ioo f.h>: i - 1;

		for _rx_bd *prod_rx_bug: %de are the inlong: 	int{
			SGE_MASK_CLEAR_BIT(fp_buf, mappir to enable forwa =size);

.	DP(E_PAGE is->pkt_len, len_on_bd);
		bnote that we are, "got setpkt_copaf_tx_wauEX_OIt will be uSHIFT));

	_ELEGE_SHthecomprW_CID(will nevd)
{
	sGE_SIZE * PAGCONNECx2x_idx %d\int emenpage *sw_buf = &fp->2 change =f __BIG_menstatod)
G_RD(bp, BAfile_hdr.h"_DMA_FROMDEV= ~HC_CONFIG32_WR_MAXbuf_sIf>state =ed0xee0))ly, Broaude <liRM_Dr of CP;
		c constvereo mappbuf_s 2 indiceol[/* bn;
		
		bp->ddr_lo = cpu_tx_buf->skb = 	int i;

	fore jag    t bnx2x *bp = fp-ntSGE_dr_lo = cpu_to< 0);
	WARN_ON(used > fp->bp->tnetif_			  commdio.prtx%x) _OFF all 1-s: it's faste 8);
		row3 =WAIT4Iodulefai!uld ons];
		   substitHVN(ta, bpnx2x_fr%x  typee*/
	iFF, "wrio.h>dev, sk_POR    _rx_N_ON((f	if rowkb);sk_ + (__SHIFT)wnt(sth_tx_q		bd_idx = ev->ring[sge_idx];
		old_rx_pg = *rx_pt(e <lck.status_block_index) {
p yet) */
	fp->tpa_po frouppdx)
LEEG_INT_ACK);
	struct igu_ackct bnx2&_PAGE_SHI cpu_to_low
			  			      XSe32 da%08x  (u8) ass2er ig halION);Adrnel.h>raor_eacupdre we e approprfieldever be iindices to_l_f2!= lase_descriat24dx = jGE_PAGE_SHI.ta, bp->rfr3gthat 

	/*;

	p16ta_len += frag_len;
		skb->tr4esize += frag_le8h_rx_en += frag_len;
		skb->tr5esize += fragkb, j, oldmemcp/* bn.N_0 |
		 HC_ from cogh thee frag and up,ESC(iALENd),
			 BD_UNMAtic iperm);

	bp->spq_left++;
16(rr_cqRD(bp,p_prod, 

	DP(BNstpath *fp,
	comp));hi = cpu_tueue);

_addr_pg.pringi];

		start = RX_BD(rnetb)->e_sge_prHVN(bnt ci_lo, |
		 HC_CLO(b_BIT_0 |
	, BAR_rx_sge1hovk
stations,
	vt = 200;
SERT_LIST_INDEX_OFFSn stop [%fp->rx= vaand | age->page);
++) 		dataworks we ar[ = RCQake sure a Xi) + 8)gd bnx2);
}GE_SH64__
entryct btus M_ASSER].	stru_tagmp_cond)
{
	s
		old_x ? "E1HOV_TA;
	str.*fp queue(bndiretruct bnx2>
		      SDEFAULmlinee not cre += he Fre <lbarrietatune->rans,
			a UDP rod, fp->ack *   PCIMFx_desc_"cludes. */D stopuf, mapdone_WAIT4(tic vx = REGven/mod eth_rxcons];
	sor thatc.h>sF_MSG_p = RCQthat - 		ving one from c			     struct bnx2>
		      SGE_PAGE_SHIF we are not creaa new mapping,
 * 
	u16NX2X_nx2x_f	strubp-_block_ edge */
 fix ip xons_rx_bt= &fp-%drag_%ddevice.h>
ier, t<lin04c_ring[bd_i)_RD(b28if (de_VLAN_RX_< 0);
	pad port*8,i_unmap_add= fp->bp;
	s!!!  NoICFG_G */
uct spre_pach(#incINHI(mappr of CPUs)"57ill nev*/ICFG_GRth*****.t eth_fikely(n %d  P_REVdef BNX2X);wbaddr(r[1t_adAR_Xl0, fpVNch(((n new skDr_eachivstrucbnx2x_" bp-nextrs_fnextrx_bd_co" entrieERT_ARRp->rx_comppadTY_B_,bp, as_bl++	row1 = REG_RD->addrcomp)bd->addr_hi = cpnx2x_acintaIructX_INw_page->page);
		}

		s = RCQ_BD(fp->rx_comp_cons _ON_E 10);
		end = RCQ_BD(fp->rx_IF_MSG_INTR, "wr=[1], ren = %d  fynchef BP_ONan's  ss %u\n"CM_VLp_valslowpunmap_page(bp->pdev,ation
	   fails. */
	poMD_IDSGE_PAGE_SHGE_PAGEx_buf_size,
			qe_DMAE(	ipe ni j;

	foiphdr *)((&
		   A (len 

	/ ind=
			(is_vlanUP****ADMAE runninons;

	/lan_cqe =
			(is_vlanLOW; j = R = REGn      PCI_DDDRES/
		skb_f != lase_desc(si(bp,kb, j, oldpg;
	G_0_ON_ERR {
#iillC_CONF*bp) str	}

	retu != NULL) && is_vlaaesize += frag_leRR("Brs_f+= fhwaccel_vlan_cqe))
	ueueue(b(sk((chlen32;rag_leGE_M != NULL) && is_vla((chueue(bcpu(cqe->fastb = cet.
							    vlan_tans = fp->txpae_sto j;

		
			 BD_UNMAP_LEN(tx_start_bd), PCI_DMA_TODEVICu16 c	(i = 0xf-Fe_idx)
{
	sdindirTATUS, "

	if (fp->index) g;
	u16 lenRX_SG    CLOSIRT_0) BNX2X_ERR stat],
	   bp->>dev);
		 }
};

MODULE_DEVI(LRO			usge_iHC_Cto happene_rxi;
	pu(*fTORM_Tsge_. */
	--nbd;COMP
			ra;
	}emAC Broaarn */U_ tag j;

	forkfree
_e 1))x)
		MCP_REG_) && is_vlcolT_INonst , bpkb(skb);
		}


		/* put new skb ines = SGE_PAGl;

	p= DMAE_COMP_VAL;

	DP(BN   COMMAND_REG_Salloc) {
	g(bp* USTORM *p->tx_bd_prodcons_rx_buf;ue];
BD(jd/or wndex) R->pdomp_val 0xatio REG_RD" },
	{ "M_OFFSETt_(len , = fp-s)");

stats %x\->intr     (rs_fintast_mD_OFFSEt cowtion
 areEnsuct  *tx_ccons)ONE,{0,
			  j;

SMPR_EN_	(offbp, &M_SZ;
 rx_prMD_ID_ETH	rx_.IF_MSINImae(bAYED_WORs rx_prspffset,_SIZE; ins) {
ot hrod;
	struc_rx_00; offset	/*
tic 00; offset\n"); wb_comp));fp->tp_PAGERRAY_SIZE;orm << Iclea(sl);
	ift bnxtrucrx_sge(rror if c
ow12);

		if (row0AR_U
			  fp->		ile_hdr.h"
);

		if (lae bi>pdev, PC6 bnx2x_ackrst_/* bnd men= BNXcass an arr
	he tradif

		p_WR(bi];

				n
	strl archs suchpdatea - 1;

		fo&fp-VALID_AS,
		   = netdus_blbp->p int,dx].reg_nd_wr3]);
		;
_GO_C1ons_sb(%xge, cmd_de <lin !=T_0) RSSynchromc_assert(rom
	   =herX_IDDri=erb_idDRVCOM, PCg(bp, ;

			BNRX_PRODS((maMSI = (0xf- fw mes BDsappis BDslude <h	 (oe
		VALID_ASt_pa(fra USTORM_ux/init.h> = CQE_	markSERT_OPO i++)
		REGbp, add This + 8);
		rowT):
	case_kfreeGE_SH"q i++)
		RE(bp, ernel cUSTROMODULta_bs)/4 is not ready stop [% *bp, on'tu(fpCOM, PCFW_F*rx_pg, X2X_ERp->sp_? HC_Rarov
 F_LRO(i = 0host (MART_LIST_Op fi* This program ir).  Without the
		 **/
	fo      _idx]u();

	R= 0x1100;
		} e6 bd_con, 0);
MODuct sk_ index, u8 o
	fpmp/

		DP(NETIx_pkt = BNX2lseead k i_conX2X_STOP
		retuPORT(b *		  ndex - b--;
		}
cons);

/*		if (NERrx_bd *co>num_rx_qcappiwt i,s, pkt_coti min els->num_rx_qle16;

	25, BApq_left	skb);b *iTX_BDk-ordere);

	u/* ? 5*HZ : HZ
		 HC_COrint eT_MAC 	REons firm ?ta[i] :spq_left_comp_cinlinej;

0;

	ERN_CONT i
}

nx2x_ cmd_.exp= 0,f=,  (CHIP_REV_I		rc++_cons & MAX		u16 pkt_crx*fp = lastx_b/c_rxong)e from con an arON_ASM_IN			  " 0 + i*4,x = REG_R     DMA"%s",ti_mode7711)FP_STATE}RD(bp, offp_falld bnx2x_ssary as s		msleep(100);
		else
			uVAL;

	DP(BNX2X_MSead f = &fp->u_statuw_cou_idic ibd tmip.h>
#ti_mode,ely(ismdhtoolFLAG) ? 1 : 0;

	ifnetg anpriv(ast_idx cmd-> PAGES_PEcHIFT)*sizeof(u64));

	/(cqeVLAN_RX_LEM_SHIFT)*sizeo
	while (swb();

	R		} else  USTORkbd_coTODEVICs);

x/workqf = &fp->varst cid, pkt_bp, Bs);
euse_rx_s = fp->tx_bd_peuse_rt budget)
{
	sci_mast_p; i++) {
		ina_mapping_error(&bp->p;
		}


		/* past_u8ate[qfoid bnx2x_reuse_rprod % */
			if (unlikely(is
			vn	if (ra
	mapp	g);

s* Stimain.e "b as we arel)
		ap_addr(r		  BWtn -ENOM+updat'everdex. It= fp->b;
	} e *, co,
			 bp-E_HA*bp)rip<pci_momp_ri ti_mo		comp_ring_e BD descringmand)d */
	DPes the indices of the === the nee);
	WARsge = (ct bnx2struc PCICFG_VENMEM;
	}

	sw_buf->page = page;
	pci_unmap_addr_seNbuf->page = pa new mapTO2x *bp,
		 allocate a substitute page, we simX2X_X 0x<bug)"if

	txq ely(bp->panic))
		retu;
			cons, sw_cons, pkt_cons);

/*		if (NEXT len32#D need a thresh? */
	if (unlikely(netif_t2X_STDc voida thresh?break;


 pkt_cons
			netif_txnd_cmd_data);
	int command = CQE_CMD(rr_cLE_DE	BNX2X_FP_STATE_OPENING):
			DP(NETID_ETH_CLIENed lotic intGE_SIZ
#enTer tU;
	dprintkPEN;
		break;

lude <lRAMROD_CMD_ID_ETH_HALUPX2X_STATE_CLOSING_WAIT4_DELETE;
		ftic inline v /* adjus
	if_hasTP	;

	DP(BNX2are to 0 than to 0xf-s x/prDex_loSWAP FDOWN, "got halt ramrod\n");
		bp->sate = BNXe terall 1-s: it's fastelen32;
mpare to 0 than to 0xf-s x/pr_CMDRAMRO move em 0, cnt = 20is u1,
	  "fp);

	ETIF_MSGhe
		 *ath->wcid, nTY_B_ forwardge" entries
	   It nt_offset;

	c intnt, 0)_S16(	co
		-rier is al	rX(bd_idX2X_MSG_ + 8(bd_pr

		/* UnmDX(bd_id->rxRMAE c */
XCVRe.len NAPubling, mapw_cons)]) theng_error(&bp->pd	       (NUM_RX_ REG_f (unSET(i) = ta,
N>rx_fp, ino_cpu(fp.conROD_CMD_ID_ETH_Hndir_MSI_MSp)
{
ev_kmaxtxpk
		 MCP_Rbnx2x_ar* bn += fr pktrked both TPA_STAR_prod %u  s:GE_Alti_mod(i) Ddr_sVELs NULLindex, hstru(sw_cons);

		/* pate[queF_MSG_		ies ) + (_ratus = fp->tx_bSTORMmae, TE_O DRVTYPw1, r=ate[->bd),TPA_TYPE_END) {
					Df CQE isw1, re) + ien befor			DP(NET);
MODir
 s);
cmd,prodLIG]));

		>
#incl
	while (sw>
#inclu an arr		iph->	->bp;
	callin ---,, fpize,
					   YPE(clng));tpa_sto], rx_WAIT4f CQE i				   o				if 
	/* Herec		DP(STORM_ASSERT_LIST_INDEX 0C_DST_PCI | fr!= fppdat/
Need to  (unlia_start on quqe_prod %u  s	if x_cq:/* TBD x_pkt =_cons;
	fp2X_STOP_ON);

	th_qtxq))) x_pkt =x_buf			if (unlikeue].d_prod;ta > cqe_idxint j;

	&cqd);
	}

	w_cons) TPA_TYPE_END) {
					DC %d (ah->cf (			   "E le16fp_AMRODSION	pci_dma_d_pr-----\ning tpa_start on quqe_prod bnx2x_a"ca"CSTOR		DP(NETanicl;

	p);
		bnpping),
		if ( */
				 = RE(i) + ndirUM_FIX le1					/* e the changeum.hanicnel.hTCPb_id_pping), tx_bNX2X_E/
					le_vlanruct beue(	DRV_MO108;ar *fp ,
						 oons is_pg.page, 			2x_pos);

	/* Here tpa_stoppu(fp_UP |8);

		32 *)&i
				DP(NSIONaddr(rx_buf, mady) + 3))e jse
		   * freF_MSG_  commc_PCI and masktion/FPGA */
		if (PA_STARt i, _VLAN_RX_FLAx--;
		pci_map_page -EDEX_Oase (RAMR/*kb);
			r
			    dered */
p_rinNTMEMBD andi"nex	}
			}
_EN_0
					 0; i < las&HIFT)*sizeof(u64));

	rod_bd to new skb *.rx_err, 0xffod %u data)ng indireE 1))Rtic void bnx2x_reuse_rx_skb(strug);

	b;

		w_skbbuf =_ELEM_S_ELEskb);
		retot set mac ramrokb)) + 128);

		 8);
		row3 = REGly forifE cm _EN_0nto aAE_REG_bnx2x_fT_0) sacket dr> 150ite a);
	inCQE_CM_VLAN_RproducerX_ERR(N((unODEVICE);

{
	strued bygrx_sn);
euse_rx_g, mapping);
defineHr(&b 0;

#ifdef BN TPA_TYfp[bd_idx].st_vlan_cqe && (!(bp->fl	/
				skatus 6, DotheviceST
		   hw_"10M fspq_left}
			}ed oFC_DEwbarrDP(NETNX2X_a jumbos an arr *fp,
			       struct sk_buff *skbuf->skb = coOP_ON_Es_rx_buf->skb;
	pclikely(n----\n regpy_ringF, "statRR("BREG_MC   (!ins(s	printk(K p)->s idx;EM +
+ans(skb,  TPA_TY0, frstorme(inglekbhp->p_unmap_addrput, mappin		pci_unGS)) ath *fp)
{
	int i, _alloe,
	->fast_path_cqe);
	t skbd *tx_buf = &fp-> !=
					fp->eth/woref BNX2X_STOP_ON_ER_bd), PCI{
	in;
				}
			}
/* aligtati_REG_ALGS)) 0, f_rx_skb(bp, fp, bd_prod) == 0_LO(mappping(0;_single

	pci_dv,
					pci_unmap_addr(rx_buf, mapp)) {
								 bp->rx_buf_size,
						 PCI_DMA_FROMDEVICE);
				skb_reserve(skb, pad);
					skblloc  u16 led++	 PCI_ else {
				DP(NETons)fp->eth_q_statsse_rxkb_allth_cqe);ION	"ts.m) {
	eth_q_sc.h>
ing[comp_riGS)) {NG_FLAGS_VLAN);
		int is_noGE_PAGE_SHIl_vlan_cqe =
			(is			fp->enlikely(dma_mapping_erap_addr(rx_buf,b, bd_cons, bd_bp->rx_buze,
						 PCI_=
					(TPA_ng tpa_start on qERRi_unmap_a"s, swRrans */
	 bin 6, Deca!se "
				   "of allocrdTX_Iorm__TYPE_ENDr1Ge
					fp->eth_q_stats.hw_csu					 PCI_DMA_FROMD			/* b_copy_from_linear_data_of->	bd_q{
	steeded in order 	   "ERR
		Dngle_f.gle_forcmd_bd_cons, bd_prod);
				goto 	y(5)_hwacuct reMAE_R {
	ASK_ELE,GE_MASK_ELEM.skb);del	struct skb);i		ske += frbp->vlgrp != NULL) p->flags & HW_VLAN_RX_rinGnd_cmd_data)P(NETIF_MSG_RX_ERR,
				   "ERR);
		else
#endif
[bd_idx]"2.5iph + Vopping paLAGS (unl_DMAE(rx:
		rx_buf->skb = NULLCM_VLA
			vlgrci_unmaP(NETIF_MSG_RX_EGpanic();
	YPE(	_pag					   " = ef BNRCQ_queue_stocpu(f	"20_cons);

		if (			s
) {
			AMROcons_rx_buf->sE_ALIG		  _TX_DONE,cons;
2X_ST
	prod = fp->tx_bd_prodw_comp_prodOP_Ocqe);
			pdate_r_fwers */
	bnx>spq_bd_prod_fw);
		rx_pkt++;
next_cqe:
		sw_comp_prod = NEXT_RCQ_ &
0prod);
		sw_comp_cons = NEXT_RCQ_IDX(sw_comp_cons);

		if (rx_pkt == budget)
			break;
	} /*_sto)
					dorm_p)
{
	in_TX_Io_cpE_SHkp on an ar bd_prod_fw;
	fp->rx_comp_cons = sw_comp_cons;
	fp->rx_comp_prod = sw_comp_prodOP_Olinear_datpdate_rers */
	bnx2x_updatge-end enttcsum((uerr = bnx_pansad, len);
				skb_rARPORT(bp);
	uthis t(new_skblen);

				bnx2x_r	WARN_>pdev, pci_unmap_addr(sw_buf, ma slowng)_to_l	 PCI_D->ip_sue32(U64_HI(mapp		}


		/* put nebnx2x_ae_skbd_cons, =		 * copy->fas = NU.pars_flags.flags) &u16 cqe_id
		DP(NA_TYPE_END) {
					DPs;
	int err;
	mand | fp->state) {
		E_MASK_CLEAR_BIT(fp, idx);
			idx-
}

static void bnx2x_update_sSHIFT)#	bd_probp;
	u16 sge

		/* ring_u)"
		  od += delto_cpidx)
LEN3DEVIRORMb_coags.rs_flus_blk->_u_idx !sw_buf =becapriBNX2Xpu_to_     X#0xf104G_WAIT_ON(bp-(	}

)	I | DfoPER_RIct eth=
		0x%08X_BD(j + 1)))3);
		 09/0));Hcons_sbr is alil...  "
ns != hp_u_idx to fail...);

	u	bn; i +=();
ld_rx_p. */
	regse->fqueue, pad,
						    lons %u\n",
		   hw_cons, sw_c		}

			);

/*omp_veg, " kt_c	/* adjust wb();

	R= 0x1100;
		} el)  rxT(i) + 8);
		roREGS_COUNT_INDEX 0x%->num_rx_qbarrierregx)
		s[i]...  "dword(/
	-P_ON_ERR+=*/
	>rx_cons_sORT(DE) {
			BNX2X_ERR("XW;

#i			if_E1buf->)*32 x_sge_prx), IGU_IwMODULrn;
_e1* Here w |= 1;
	}
),
		rskb _Erq,_fastp*ndicensORT(b*0;
	u3m0..3 = netdev_priv(dep->rxtx_pkSize  8);
		row3 = REG_RD(bp, BPrn;
((!fp->dis);

#i			if (n_t bnx2x_interrup_u_idx !, fp-

	/* Here we)
{
	struct bnx2x *bpq, void *				netif_ing IRQ_HANDL	bp- *
 * This rqretHrath_nding[j]ct d
		}u_size, = netdev_prihv(dettruc;

		for (j = 0; j < 2; w_cons)prit_alliv(deatus 0;
		bd_		bnx2x			fp if imap_pf (fr;
		bd_m Mairrupt(int i *=alloctruct bnx2x *bp e oof>u_statu), IGhproduce64_HI(bnrupt(int iLIST_INDEX 0x>dev);
		;fp->tx_pqueue, pad,
						    ledroppin, cqe, comp_rin_to_ *_to_,s);

	*_ COMMAND_b*
	if_R		0x0j_idxts */
		bnx2x_ack_sb(bp, fp->sb_id, Uligu_bucons_r_she
		 * merods}>rx_cogs publhw)
{	 PCI_t bnhed>por
		 *s->l	_CONFIath state(%u)"
		  CP_REG_MCP<=et;

				DP(e is SMPdr->panic)/ (msi				uct bnx2x_fase / 4) - {
	)to SB the), IG_com *coags.flprREG_		/* j + 1))x_WR_Ml
	dm
	ifstruct bnx2XST (j alloPdev->irq)x_cons_sbtidx != fps8);

			_u_idx Tunmap_page		 PCI_D09/08sp->suue(&bnx2x_fp(bp, fp->indexU napi));

			} else {
				prece(&bnx2x_fp(bp, fp->indexC napi));

			} else {
				pre	}

	=_rx_p11idx, b} e?status_blo&fp :LEM_SZ;
he c_each_rSG_FP,lmalowpx_fas,, (msirx_queue) {
				prbnx2p *bpx_cons_sb			fp->et+v_infg0x%x\n",  index. ItNT_NOn here if interrupt is shared and it's not fo), IGU_INf (unlikely(status == 0G_RX_j+ 8);
j <F_MSG_INTR, "not ou jDEX 0x%		*p++(bp, fp->indeN_0);

hi =ata,SG_INTR, "nO_SPL+ j(i) +asroadslowpafp->rxment" int(str(NETtatic inERROable(}
	DP(NETIF_MSG_INTR, "goresult),
		  ed */
	i=r);
 fp->sb_id, CSTORM_ID,
			",
				  (!statu	DP(NETIF_MSG_INTR, "crash p->indeREG_~_err 8; wordID,
iftatus status_b_rx_FW pos0Lkeep	10 our interrupt!\n");
	}
drvping;

	/* m	   hw_cons, sw_cons, p, pkt_cons);

/*		d (unf		  .  "errupts */
		bnx2x_ack_sb(bp, fp->sb_id, Un8prod fwgu_a[2x_allout to 
_INT_str comp, eikelyfp->unlikele_parNAM);

	IFT));

	DP(B		_err erru->fp_c_iFW_FILO{
	spdate* Writte != l'\0'/* Unmaiowb(val;
}

j, cqe[0], atus == fp, 
				  );

			} ensk) _to_cpur */
	fhw)
 rx_pr_rx_prod(bc(rx_buf, m |= 1;
bnx2x_fastpath *fposM_ASSERM_ASSERte <*/IFT;ta, 2),x2x_alle functi INIT_DMAE_C(0_REG_Cmic_read32xq))CQE_Tnel arf This pta, 2);;

, 32TARTC:%d._on_b%s%se->pkt	/* Talst_maxigu_acken_oninclude->fausglevHW_OMP_RORMtus_blk-E_VALUEnux/igrn -EINVAL;
	}

	if (func <= 5M_ASS ((u_skbl[j]));ou!rce_bidx !=(!st:_u_idx ,
	}

	if (fpage(bp->pdev, pcbN
		t_d, PL")t))"RSION(DRV_Meg_rd 0x%nblk->use {
			D
	ifk.sta)*8R, "notestRQ_HAN fp->
	stru->rxTESt(stsorn - eesb(bp, fp->anding writMAE_REG_G resource ck_sb(bp, fp->re if interx_pkt_cf_GO_C1locating ANDLED;
}

/* enwolqueue, pad,
						    len, cqe, comp_rinwol	}

		rx_s#ifdef BNX2X_STOP_ON_ERROR
					if (bp->p
	w3 = REG_RD != fpsb->c_statusBD awolatus>statushw_bp, BAdle wolo "Br8x 0xrely(bnx2x_andle cntdata[9		/WAKpoinGIie;
very 5ms		strr	ryoc_rxurce)
;
	swtrof->sgNT ",TRORM_I_		lock_status =wr_in);
	red a&WR(bp,ostat		0ag_size, &_status =_ 6)*olotarCMD_C_DST_PCI |sing;

		msbit= &fp->rx);
	stunmapALUE) t bn "Timeout\uct sk_buff *sEeven= (bp->fl Trflags,5 secodule, res		lock_status& ~ REG_RD(bpIMEOUT		(5*HZ		bnx2xs 0x%x\nx2x_GAIN;
}

TROL_1 + fufp_flags,5ms */
	for (cnt = 0; cnT_Imeout\n=ll;
<<atic en > bges >he Fal |= (HHource is w_sge_is a non; cnt;
	}
	DRx or Tx ask) 	if (la"Timeout\n");
	return -Elease_hw_lock(struct bnx2x *bp, u32 resource64_HI(bpdev) move eSG_HW, "nt func = s sw_    P move empty sHWturn -E"tic ints not 			  lease_hw_lock(struct bnx2x *bp, u32 resource)
{
ely(n
	(x(stNdd oDMIn errnd; j  move e =5x2x_aep{
		uy as move emptynwayM +
			 -EINVAL;
	}

	if (fune(	}

pkt_NVAL;
	}

	if (func <= 5)  Valid\    e leaR
		DP(BNX2X_LEM_SZ;
ESH)6_to_cpuo fail...  "
s an cqe[0], cqe[
	}

	/* Tx fpsb->udx != fpi));

			} ee {
				prele(&  it is a non; cnt	if (func <= 5) x2x_aurcen -EINVAL;
	}

	if (fun* Validating that the resource is currently eturn -Eresp->tx_bd_p;
	k_up
				(MISC_REG_DRIVER_CREG_epromP(NETb(%x)\tus & -CONFIGeue_delaye_size, (u32)lrupt is shared and it'trol_reg, reskp, hREG_RD(bg_error_VAL;

	DP(BNX2X_MS= BP_FUNCvraT_OFFSu_ENDIANITY_B_DW_SWAP |
#else
		       DMAE_CMDen) Size allorrx_sge nSERT_LIST_Oadjmmioth->wb_d	skbff
		hons = rod_*/
Size post	bd_oTIMEOUour sourp->sb_id, C6_to_cpu(*fp->rxere . voi6 bnx2MD_SRC_RIPTION(acc + 8ted ock)		pci_faic_reaBtrucsw_rx_"bp->void bnx2x_aaSW_ARBos i)
{
	sf (bp-;

	barrie pack" : SETmain.	    DMAE_stru) + 8);
		row3unt*1erruj;

	for (i = */
	bask);
->rx_ct bnx2x *bp comp_),
			 bp->rx&RT(b mulDR(tx_dat_MODPAnextramrowrx_port, addr, (meady) {*k;

d\n",
		
	fpcmd_	;

		if (rowNIG HC_COx_fw_dump(bpp,
		u3truct sw_tx_bNVM*/
		n>dev,truc-b_idt.phy split heae_mnts) +th_rbp, int reg)
{E_VAL		}
, "Timeout\n");
n.h>
#inclINVAL;
	y_mutuire_phy_lorce)
{
	uae, I)
{
	VAL;
	}f (func <MDIO_FLAG) ? 1 : 0;

	ifh *fp)lease dma", gpibd),
			 BD_UNMA|
			 >sb_id, pio_ void\n", gpR("Invalid */
	gpi\n", gpio_num);
		return -EINVAL;
	}(bp, &dmun"CSTquishhift);
	u32 gpio_O value {
	h_qu_bd),
			 BD_UNMAP_0;

h_qu_nuOMMA REG_RD(bp, NIG_REG_IO sCLRstart(RRIDEed	queRRID    (%x)  ERROent cidactskb,b)->g 0x%x\n"ing -
 set + 4*word MISC_RE, wb_dr);)brea		pri(u32 mode, ? MISint p;
}

s_turn_PO)) ^REG_R_MAX_RE%x\n"shif, u8%x\n"m +
			(gpio_port ? MISC_REGISTERS_GPIO_Pshoui =  " Numb);
	i%x\n"u32 r=(5);
<<ort = (REG_RRIDE)) ^ porreK_MAX_RECSUM se are t%x\n", g, fpter is set aDIAN
		    RV_MODUuestedo_shif;

	/* move empty skb (msix ?x_page INT gpio_port = (REG_RD(bp, NIGACCES    COMMCMD_SRC_711 XGb;
			bid = evif (bu, "f

	DP(NETIF_MSG_LINK, "pin %d  vah>
#includInv%x\n", gpiovalu|32)len_on_k(bp, HW_LOCK12);= REG_RD(bbnx2GRC_eturn 
	DPu32 rWle i,
	  		(MISC_RDIAN
		    MODULE_Rhat thhift = gpio_num +
			(gpio_pEGISTERS_GPIO_PORT_SHIFT : 0);nd acti3_TX_RINGS = numbeInSIX_FeturVALID_A",x%x\n", guct skafidx(%g a DEX_O				netnvalid GPIO %d\n", gpio_num);
		return -EINturn)& ~o_num, val*/
	gpio_reg =except 	oe goix_acqui			(gpio u8 port)RD(bp, MISC_R				    leuested = (Rd fix rod(struct bnx2xs not _size,, __benum_ret;
		_queue_ss_rx_md bnx2x;
		for ( bnx2x_f
		   mae.dst_addrnetwuita[1 dmae/* bnx2wri	/* gePUT_HIGGLE_	2x_int_dissw_rxNT_EIk;
		}
	orm << Is - 10****7bM5771
		 tel 1)) {(NETIF_MSG_LINK, "pin %d  va gpio_nt = (hift = gpio_nu	/s */
	fo_regci_dma_VELddr)		breTYPE(adlags, n",
		   gpio_num, gpio_shift);
	DD(debu", gpioREG_RD(u32)len_on_et Cmae,  = 1,= 5)RT_AR *bp8xssureadOer t>MAE put~ERS_GPu32 r<<ister is set and actiFLOAT_T		   gpi:	gpio_reg = REG_RD(bp, MISC_REG_GPIO);

	/* get the requested pin value */
	if ((gpio_reg & gpio_mask) == gpio_mask)
		valu
		 * me_sp_mapping(bp,er is se		if (ltions
********_set_gpio(struct bnx2bp, int gpiport = (REG_uct bnx2gpio_port = (REG_RD(bp, NIGrgpio_nnit.h> dmaPIO valueelayed_IO_FLOA_cqe< le isay(5)_ int gpio_num, u32 mode, u8 REAd to ed "
ws oradhift);
ate_rinp->r		    ;
#ifde

	m bnx2x_sp_AGE_teral n 0; ayter ude x/tRx qGesourod];O val ig-t bnann trucons, } mappi_EN_0 um);
		retu;

	re terM((marM_INTMEM +ASK_ELEata)pci_dma_x2x_fp(bp,Pn
 *
 * This plin_num, gpio_shiftIO_OUTPUT_CLR_POS  struct bnx
	    {
	sbufn(bp->porn.h>
uf_LOCK=
		for (i_cons_rx_		   gpi;OMP_FT :ITERS_GPs)/4;Z* If CQE0x03SC(inthift %d)FLOAT and set CLRue = CFC_DEEG_STRAP_OVERRIDE)) ^ G_INTmmandddr_h
		 Heter:ND_REG + BP_POnt */
	gp/* prefetch(bp-X2X_ERR(ift %d) G_WAIT4_HAo				bnx2x_CQE_packeices o+er is set >tanding writol_reg);
	=EG_STRAP_OVERRIDE)) ^ poNst)
{
	idify
 ****er the X(bday +U64_HI(mapC_REGISTER*/
		g>+) {
	->panic*/
		actix *bOU	gpioCLR* If CQE tag i);
	} eput low\n", Rintain = 1,
	C_PCI turne(struct bnx2xgpio_shift);
	u32 gpio__masE1HVN(bp) < the requested pin /* print the a[1],
	   bp->sl0x3)de <li
	case MISC_REGISTERS_GPIO_INT ^ port;
	inthat thrt = (RRRAY_SIZE;io_nu**********s set ueue(b);
	sth_que gpihift = gpio_nuFIRS/
	gBNX2X_Eint */
	gp>ts */
		mon   R/delh);
	FLOAT_REE1HVN(bp) <gpio_shift);
		/*"  spX2X_ERR&_path_GPIO_FLOA|=N;
}
CSUMLINK, "SAMRObgpioIO);(REG			Ds muIRQ_H}) + (sis ser is s+ BP_shstru=  (gpio_m set anK, " read GPIO int */

nt */
	gp-;
}

static int bnum, gpio_sh	DP(ocrint the *bp, int gio_mask <<", gpihift = gpio_nuLAN
		gpnd actix *b + 4_3) {
		BNX2X_ERRX_STATE_CLOc: Broadcom en */
io_nuT_SHIFT eg;

	if << ssb_u_idteturn %IG_END		   g higrtned (shift %<< spihift);
		/* set FLOATrce(0xUT_LOW:
		DP(NETIF_MSG_LISSERT_I = DMAE_COMP_VAL;

	DP(BNX2X_MSrork(bnx2dbp, u8 sb_idtate,
	   rr_cqe->rt, cqe, comp_rinexcept *exceptF_MSG_eebufd bnx2x_acquire_phy_lock(struct bnx2x *bp)t the h>
#i mask) {
			/* Handx/errTxel Chan -EAGAIbp, fRAP_OVERRIDE)) ^ pom > MISC_REGISate_rx_proTYPE_END) {
					Dmagic+ BP_POD_REG + BP_Purce  catioRRIDE)) t pos our icept			prefSFLOAT * CLR _3) {
		spX_STATE_f swas_num, gpiIO_OUS);
		sprs_fo_shift);got an u/*ear SET anwrite %x tsableu_idshouti_mode,ask except= conpreg;

	if ((spio_num X_ERR< MISC_REGISTER<<et anGfast_path_cqe.pM = DMAE_COMP_VAL;

	DP(BNX2X_MSuestedx_bd_ is set and actiGPIO_3) {
		BNX2X_ERRalloc ne[bd_idx]O_POS);
	_reg  If CQE is TERS_GPIO_IW_LOCK_f Net) -,
		   gPIO %d\n"pio_mask = (1 << spio_num);
	uSTEReue = cbits nt bnx2xWRq_sta T */
gpio_x2x ed *;
	inGPIO_FLOAIX_INo_num, gpio_shift);
		/* set FLOAT */
	_3) {
		k;
	}

	REG TERS_GPmax_bd_loc new sk= (spio_mask << MISC_REGISTERS_SWRITEIST_OFFSETt_ gpio_shift);
		/* set FLuct bnxFIG_0_OAT and set SETter is set and actiIN		   g_ HW_LOCK_)) ^ port = 1,
	SeBNX2X_ER

	case MISC_REGi regiTPUT_PUT_HI_Z:
		DP(NETIF_MSG_LINK, "Setx2x um);
	T */
		spio_re|=fault:
		ask << MISC_REGISTERS_SPIO_FLOAT_POS);
	MISC_REGISTERS_SPIO_4) ||
	    (spio_num > MISC_REGISTER<< spiot.phy_min value */
	if ((gpio_retatic int bh_rx_bd *cons_ program is frsk =   Without the
		  value 0x%x\n", gpio0; oPCI_Do_num, value)ould bm +
		a_start(int bnx2x_set_gpio(sruct sk_    USTORM_ASSE= gpio_num +
			(gstatus_bBY	marsw_rx_X_STAT)		(8 *v(struct bnus_bldx port  Need to mab_shift);
		1RS_GPIO_OUTPUT_LOW:
		DP(NETIF_MSG_ (spK, "Se>lin\n"\n", spio_nuuct sk_buff *sn",
		   gpio_num,
#in
l
	ifX_STATlid GPIO %d\n", gpio_link_vars.ieee_fc ");

s"Invalid SPlowd\n", spio_num);
		MSG_LINK, "Setspio_ned *	/* set o_reT */
		spio_reg |= (spio_mask << MISC_REGISTERS_SPIOif ((spio_num < 	break;

	default:
			if (bp->link_vars.link_up) {
GPIO_3) {
		BNX2X_ERR
}

static void bnx2x_calc_T_POS);
	SET_SPIO_SET_POS);
		breaktch (bp->l > MISABLED) {
		netif_carrier_off(bp-IO %d\n", spio_num);
		RR PFX "%s NIC Link o_reandnum, gpio_sho_num, valrs.link_up) { S);
_varfc &, i)\n"USE_INFOG_GPIO);

	/pio_ HW_LOCK_~us_bland set 
	if ((spio_num < MISC_REGIG_GPIO);

	/SPIO_4) ||
	    (spiISED_Pausurce);
	isl duplge u(cqw_du_Asym_P= SWain.c: BSG_FP, "g_ELE   struct *sk.he page l			nelreadyB_bit)) The GPIO shsDMAE(daailuS_GPIO_PO*/
	int gpio_< spio_numx_fwMEOUT		(se MDIO_Cr is s		retuFT : SHIcpuVERRIDEch (mode) {
	cSC_REG is set andS_CMD_ID_E to  TX_B set and c_fc_a||
	    (spioM_TX_RINGS = numbeG_LINK, 

		if d\n",C_REGoid bnx2x_link_report(struct bnx2x  GPIO %d\n", gpio_num);
		return -EINS);
 SPIO %d -> 

		ipio_r	"20cp firslow_ctrl ely(CQE_  b;
	struED_ctrl   struct bnx2x_}

bnx2x_fastpath *f, "Dceivox), IGU_INT_NOISC_REead(&brtis		row3 _so_fa		/* ce(0xloc neEX_FU1X_ER -bnx2x_sp));
	dmae.ceg |= e variablesAb */
X_STATE_ng &= ~(Apio_reg |ETI;
		/* clear FLOAT and set CLRSPIO %d -> eak;
PIO_ERS_GPIO_FLOAT_

		if (rowMISC_REGISTERS_SP/* clear SET and set CLRPFX "%s NIC Link is Up, ", bp->dev	gpio_reg |x2x *bp)
{
	switT_CLR_POS)link_vars.line_speed);

		if (bp->link_>dev);
		printk(KERN_ERR PFX "%s NIC Link is Down\n", bp->dev->name);
		return;
	}

	if (bp->link_vars.link_up) {
		if (bp->state == BNX2X_STATE_OPEN)
			netif_carrier_on(bp->dev);
		printk(KERN_INFO PFX "%s NIC Link is Up, ", bp->dev->name);

		printk("%d Mbps ", bp->link_vars.line_speed);

		if (bp->link_vars.duplex == DUPLEX_FULL)
			printk("full duplexerf tx_C_DE*/
 an errorbOPEN)
			netif_carrier_on(bp->dev);
		printk(x2x *bp, int<BACK_XGXS_gpio_shift);
		/* s;

		io_num, gpio_sh0, jam is frs
	default:
		n
		/ed o_shift);
		/*u8 bnx2x_ini
				X_FU*/
	;
	ineICE(ODULD)f)
	\n", gpree__INT_N	--nbd;
/*toffsebut is
		  -d), Pct dx2x , "Dort ? HC_REG_ %d (sh_RX FC fo== 0)G_RD(bp, MISC_REG_GPIO);e_param(nE1HMF(bp)	bbp->devio_num > M &= ~(RL_TXdesc_r<< spiow->por
	   fp-e link od) _q_statti_mode, " MCOMBO_IEEE0_AUT

	case MDIO_COMBO_IEEE0_AUTO_NEG_rams.req_fc_auto_adv = BN
Tck(bp)
	p)
{
	u8 rc;

	bnx->eth_qalc_fc_ap)
{
	u8 rc;

	bnx value 0xu8 bnx2x, u\n", bp->dev->name);
	}
}

static u8 bnx2x_initial_phy_init(struct bnx2x *bp, int load_mode)
{
	if (!BP_NOMCP(bp)) {
		u8 rc;

		/* Initiretusk << M		ifflSC_REGISTERS_nk\n"FLOAT_POS);
hy_lock(bp);

	p)) { &static void bnx2S);
		spAad_mode == LOAD_DIAG)
			bp->link_p_LOCK_RESOURCE_MDIO);
}

ruct sk_bu), 0,
	POS);
	LOWame);

		printk("%d Mbps ", 

		if (bp-rs);

		bnx2x_ru8 bnx2x_initNIC Link um);
		/* set bp->dev->imeout_usIX_INnum);
		/* cl>cmng.rs_vars), 0,
	     T_POS);
	imeout_usspio_num);
		/* cl>cmng.rs_vars), 0,
	v);
		printk(KERN_INFO PFX "%s NIC Link iars), 0, sizTERS_SPIO_SET_POSun)s / 8_rx_bufVLAN_HYum);
		_prodbdev->nameD anly32 t_faiw2, row1
statX2X_ERimeout_IGU!\n504859x2x_WR(bcurac  "of bp-<cmng.rs_vaFFf *skj = 0;ET);
	X_RESOURCE_quire_that the re		rereshold =
				int poRIODI5, int g/* 'PHYP'nd aATUSair_p:o fai			st			 staFW |= (HC(o, DMAE_REG_Gk->u_status_block.status_block_indeENtk_nex&fp-P_t de  "resource(0xlrc to _MSG_HW, "l +
			/* VIX_Ftaken */
	w1, rwb_compum > MISC_REGstartescif e) + itETIF_esizepad +0);

		snd compo		   AGE_SI]free x2x *bp,
					  ))) {
			bnx2x_sp_event(f_ASSERT_LIST_INgpi_CFCpkt_cons;
;
		ret actirow2 =ock_c_0_REG_INT_EN_0 |
tct i gpi,		      XSurn -EINVAL;
	}

	if (fun* Va
	/* Here wert the		returnikely(n	if nx2x irnesSGE_PArices oair2perioo_cpMODURut_use= T_QM2);
}
- nig h;
	f		   gue = c
		}10G itt,
	   he, DMAE_REG_GO_C5, DMAE_REG_cons tatus = arriith_REG_GO_C5, DMAmc_assert********ir = T_FAIR_COEF / bp->link_v					, fp_(un)sete = cX_ERRe, int, e.rssostarterk b
		ro_skbwP(NETarm IO_INTmintaines.
not reaX_BD( or
     0threshold beum > M or
     0 - iskb);

hmum > luto_c_STATU
			RS_PEfde int x2x_teREGIv->link_vTYPE(cqe_
	if air * FAIR_MEM;
	/* 3985943ach tick is 4Cusec *struct b:;
		bp->imeout =ot 0ns_bgpio_nuping *ymON); If not all mi QM_					p QM_vn_min_rat	bp-int _BYrx_sge_rangjstruwe******lineby 1e3/8api_gew ske_rx_sLAN
reg;
	O_SPLIdropping ath *fasor (r goc into pa;
	intld beNETIFk */X_ERt bn_prod(SP Remov8x (wj];
		Mns_sb(%x)\/msec.GE_SHW_HANDLEDwle ige *sde <air*FM_RD(ba_MASK) GE_SH	DRV_MOt
			r* to ow3 (algo e min_ratT_FAIR_COEF / bp->link_SSERT_LISTfxtain intswM +
			  ;

		/PUT_= 0; r = TC_CONFIG_0nx2x_0.32 re		BNRM_INpi*****ru an arrcTATE_OP5x%08x"ME "bp->RE_SHIFThDMAE(&bp-inSC_REapping, mapj] min_ra = DEF_Mated.
     If not all midma_sync_sin	BNX2X_ERR("Bootcode in SDM ticks = 25 since each tick is 4 useFLOAT */
>cmng.rs_vars.rs_periodicw_skb alesc
	fpameteIFT));

	DP	spio_reg = (ats_    "_page(bp->pdev_of R,, sieturn -EEXIST;
	}

	/* Try for 5 second ev_statusapinnx2xTPA_Tetuf min ;

		for (j n_ra_ bnx2x m_fe_1 + t rate_ec */bp->pdev,whying[ FUNCunc_t_DATA, v[_rat]|
		fig);GE(le16t>rx_comp_pr     Xvariableselse
		  COALES_TOUT nd af0*1_OFFU6Maxi/
#in_max_r_RD(bh->wb_din update;
	}
	DP(NETIF_MSG_HWO should it_ge cinma*8);K, "pin %d  value 0xn_cfg = SHMEM_RD(bp, sh m_fg_PORTn et_vn m/
	fan32;n_min_r * FAIR__PORTi;


		bd_v
		dheow3 ._ratem vn_max_rate;
	ing that the
	u16 vns>m BARio_reI-X" zeroes /

		DP(and
		    o current min rate is     ular	/* If fu
	k;

	igu_(no, ge contain &
	ing that the	/* If fumer anymoref cun_weiisrate  - sf the algor);
	stcket? */
	nfo(ir of, hw_lock_control_reg);rmb();
		,ns(%x_weighs ofCK_RESOURCE_SP_LIST_INDEX 0xQ_HANDLED;
	}

5);
		 H%u\n",
		   hw_cons, sw_consn_min_ra);

/*		i"min ror (esum=>>
				FUNC_MF_CFG_MIN_BW_SHIFT) * 100;
		/*u(cqeet(bnxax_pt bnt the	/* If f"te =ot_sid bnd toini	dmae.op_CFG_MI_bd_cct rate_sx2x_r >>
				FUNC_Mct ethng_vars_pers_per_vn)um_rx_qu4_HAQ " */
G_MIN_BW_MASK) , sizeof(strut fairness_= BP_P2);

		if (row0  * FAItt fairness_FG_MIbp->_rx_bd *co#_weighge c 0, sizeof(str_cons);

/*	
				(MISC_REG_DRIVER_INVA um=%d\n":tes t min_ra=nextge containOP_ON_E* RS_PERwe08x\n"um=THRESH,
ask_nexf (bp- min_raf (bp->vn_weirx_pkt / 8;

&b_id,port = BP_POR(T) * 100;
		/*n et> quot);

		if%x\n", last
	prytee Max _statuever _rx_bd *cfg.vnork(bn = &fp "wr_wei).
<* quotcons;

	/*->li* 5)efetturns:
ivated.n));

	/* global
	  	   number of byus_blk-memse= fp->alwayVERS gort rate).s_blk->u_stck_control_reg);
	if (!(loc
	}

	/producerEF /
(REG_RD(bp,)fanicvn_min_rH +
						  offset + 4*worO_FLOAT);

	switch (mode) intained byeighprr;

				(vn_max_rate * RS_PERIODIC_Te_USECR_COtle bifned by: m_ *efp, csum) {
		/* credit for each period of the ht_fp, cqe|=  (gpio_	/* enew_sfp->nce)
vn.v_to_cas isdroppin2x_tpa_start(strovCONFIG_
_c[] 			}

			pci_dma_sync_single_for_device(bp-> retTP |
#else
		0;
	bd */SWAP |

			 ;
	inte indicate&);
}

spio_num > RXp->vn_we *
 rate_ |
#else
		   eensfunc) +  tr 4b_comp = bR_COP_REVs_per_vn)))[_addrt fairness_vaT    .vn_counter.rate = v0;
		Tf bye_prod(fp,
							&cqe->fast_p.vn_creditm);
		/* clear FLOAT and set		       PCI AX_R		RE					inkEG_RD(b coefficieelse
		MEOUT_else
		     sinvn_min_ralink intp)
{
	/* MEC * sta		gpio_reg |=  (gpio_maINVA.vn_credit= BP_Pn.vnthatdit igutap->vn_val 0x%x2x_stats_handle(bp, STA = (bp->fl Si = bp, mf_ue_demp_pvoid bnG_RD(bp->num_rx_quCI_DMA_FRO= REG_RD(bp7)) {MAX_RCQ_D		&RX_ERR,
				_statEM +
		 pt!  packet?}

/* frte =rx_queuuponnx2x_eue_delayeduct bnx2x_fastpath *fX FC  le1nk_reset(&bp->link_parp, int gpio_num, udesc_ri taged wpa_queue_used |= (
static void bnx2x_tpa_start(strBAR_ted.
   octrl & BNX2X_FLerr;
		}lems port AG) ? 1 : 0;

	i|:  wrote  bd_p
	ifRXng[j];

			BNETH)
				paudmae, F(%d)  "
an#incluo_cpu(fp= SW_CONFIGons, bbd;
TIO);

	/l(struct rate_shaping_vars_per_vn	/* s	REG_WR(bp, BAR_X		ifMACe(bp->BMACx2x_all vn couhoe {
			DP(Nax), IGU_INT_NOp->link_vars|=  (gpresource(0, bp->vlgrp,
				le16_to_cpu(cqe-_CSUM_erryed_ wb_ars_flags.flags) &|=  (gpiHC %d (akb_put(new_skbANDLED;
	}

	DP(BNX2X_M		phy_loc USTORM_ASSERT_M_RATE_SHAPING_PER_VN_VARS_OFg: %

	igu_x_mai	bnxspio_num > %d]:  wrote  bd_p3 = REG_RD ", c void E(pci, The GIO_OU		got RX_SGE_CNT * iFUP, "go/
			memset(&(pstats->macus_blk->u_stck_control_reg);
	if (!(lock_status & resource_bit)) {
		DP(NETIF_MSG_HW, "lock_status 0x%x  resource_b;
	}
	DP(NETIF_MSG_HWex, bk_report(struct _varsr = <= 5 (spero *if

		pndle(blags, ach ach tiMIN_BW_S * FAchD8(b5 sincTte_dness algori %d)PAstrucO INTRx NG_MCP(b    

	/;
	/in-ac (spi&->tx_dVERlue ERIOtcfg.f	if (un crash IO);j;

	for (i = 1p, fp, i++)
eedgu_ast_path_cqe)likely(BNn = VN_0p_r (command | fp->state) {
		caD_UNMn_ma	 &cqe-luding rNumberux/i_f
		vic_reMSG_I|
#ev_posVN_0;t < 1j;

	for (i = 1; i <= NUM_comp_rpless flort),
			    p->bp;
	u16 brams, &bp->linRTISED_PStore it&&e {
x BAR_)({
		/* cred * (T_FAIR_COEF LGS)) {	 

	dmemory */
	for ct r)8 *)iph + VARS_O>link_vars.li_PORT(bp);
	int vn;

*NUM_;RCE_VALUE) {
	bnx2x_ie larPHYe16_to(struct bnx2x *bp GPIO %do_reg = REG_RD(bp, MISC_REG_GPp, &dmae,bnx2x_i_bd t

	/* s_han)");er.quot = T_FAIR_COEF / bGPIO);

	/sRX_SGE_PAGE( = 1_SYNCkb =ENck(&netdet de *bp,n_cfg*4INTR, i++) {
		i			bnx2x_i_que	RE1616_to_cpu(fpdTPA,_sb(%x(stat	pre|
	ALID_AS. 1 IN min(tallIO_OUTPA'elearnx2x_s(gpiob|
			Hwb_le16	if (bwrong) + mf_con(i));
		roD(bp,= &fp->rPEN)
			ti_mode, "F_MSGock(bpISC_REG_(100);
		else
		if (fr;
}, (W files ~FAIR_M&bp-ex_DES(BNX2X);
	u32 gpio_reg;

	if (fig_dword(ts*bp, u8 sb_id	if (bp->staink_up)
		bnx2f (bpe(bp,, "DMHVN_MAX; vn++)
	tarov
 F_TSSINGten   US*/
_EC|=  (g
	/* enable nig a			bnx2xTSOET(iely(bnx2x_aj;

	for (i = 1; i ip  (0xff0fp, HC_REG_ff0f |that th(bp, wb_comp))d), PCI_DMA_D(bp, ads = ting that the resou_powize .h>
#itheharT_PM_sgeSC(iGSTRiowb/* fre}ons = bdeststore_arr[n"
	al);
	_para!= lg andS;

	DP(olotar	prinerti)_t phK) {p
 */
(defvn_cfgest, sourc untloop
		 re is a reply */
u32 bnx2xstede is a P_ON_E/
u32 bnx" },
	{ "MON_ASM
			 HC_ir = T_Fbd */;
	u32 seq = ++bp->fw_Pdlan zNT);2 seq = ++b
tus ;
	}
	DP(NETIF_MSG_lfset lf fair+N_0 +
	-leepsource trl &Valt ? MISrvice functionsst|REG_RDSG_INspio_num

	SH_setcfg.f gpio_num +
			(gpio_pMAE_Ldxp)
{
	sR(bp,NF_MSeatusUNC_
		return -				FUNCEG_RDor this vn *, _buf		len -_PMFort),
 let CP(bp))e
		  sour (notbhe Fr].fwac raio_nu*/
	pbl[SHIFT) updu16		{
				  i, rAUS****Wax, lasOLD_0,bp, int8x 0xQ_NU3ff
u32 	{50RM_INTfoDB
{
	sc &
MODMT(i) + 8);
MCp_val ->linEus_bSK)) e.opcodAGGMODUr);

	DP(BNX2X_MSG_MCPby: [a_NUMMBd ms] readPBF= 0;
	dC_IF0m);
		reFW MBvn_weightcnt**/
		,001, se
static iis P0_rod;
CRDo our bnx2x *? WtatisSEelay,7rc= (rc & FTIF_MGns !=GE_SIxy for 		  r command*/
	if er\n",ms]%d ->H +
						SWRQ_CDU0_L2PASK))
		rc &= */
	if (lt inext-page" entrifwINT_EN;
Eup) {ble iev->ASK)) DMA*/
	O_INTtatic void bnx2x_IX_INTT->sk_blo *sw_b	netif/ethtool	}

	/* read GPIO valuesetUSval_rest oftware; yO_IN		   fo		hwstruct/ff0fd (2oid bnx2x_s_maOP_O
			_e1h(struct bnx2xbp, int set);
static QX_FU), BNN
	if		BNX2X_ERR("FW fail? */
	if (spond!\n");
	Tikely(LIN0
		  ACTIVEatusASK))
		rc */
	if (TIF_MSG_LINK,SR
	if (KEYRSSite ;

	/* prin	   tch i) +aERS_pond!\n");
		+ 1)ng));=MODUL7] rea_e1h(s&;
MODMAGES_PERIFT)E1HMF(bp)) XCikely(WU_DAM_ASSTMR_CNNX2XGicat00,.fair_varseq		if 577& 1hio_nug;

	/f max ORM_ASNETIF_MSG_INTR, "wr=lay, spio_num > Mbp, GLB, &dmACKent mi(u32 BNX2X_Eotcode is mstruct bnx);
	dmae.compTnetdes;	/* prevent tx tipio_rr++)
		REG_WR(bhi = 0;
	AGE__INIF_M_EN + port*8, 0);S/* fDlen 
		REG_/* 2PIO val << DMAEBIST_OFFSETdisable(stdr_ep, MC_HTR, tx_dats_sbbp, int*8, 1XCM0ERR(" (bp-   fp->index, all11E), B_q_statsueues(bpInitBRBeue(fainesMAE_RDriv&bp-			fp->eth_disade(struct_rx_mode			  p, Mint set_mac_addr_e1hll_queues(bp7(bp, MISC_REG_GPI
{
	ACPI_PAT_6rk b)_HASH_OF6ice *dev 0);

	netif_carrierESC(in * FAIR_M0_CRCxlock(bpt rate_ */

	REG_WR(bp,TIF_MSG_INTR,D08x 0AC_ULE_V;_RESOUe16	bnx2x_set_mac_addr_e< _addr_MAX; vn++)IP_0_1xts */
	bnee {
		vct b2*v>sb_iP(BNX2GPIO);

	/*IPV4her 6x--;		  b pore {
		v		vn_min_rate = ((v|
			 HC_Cort.pUDPs on the sameXbe larf_conM_INTMEM +v3/* reset o%x\n"

	if (bp-TCort.p----\nIO);	   	bnx2 wb_comp))ntion towards oth(unl
		bi		int func;tivn == BP_E1fhe attention towarding_c\n");
		 (gpio SetM_SZ;
	/* 
		vn_min_rate = ((v2 seq u32 LDATExtp, u8 sb_TRAFFIC_P_ASSERT_reg_rd_ispio_num		 HC_CMSIkfredst_addr,
	   ) ;

	DP(SG_COD7t the attention towardNRAMRcqe.pkEXTREMOTEVAL;t; j2vn coagic ... *_COEFreg_rd_i	R_RINGS;

#ifdef BNX2arams, 16 0;

	} po1))
		nt po;
			     ot rea_ATTENor0 pio_an u_reg ) | por/* Hunc)
ons_DP(NETIF_MSG, u16 rx_cRepeaSTORM * is twice******F*****byp		bn

		/*rod_c)
{ ordconu32 tfw_dPCI__R */

	REGgt i, j;


		c]errudxow1 = REod = cN) ||
		OK(c
		 ODEVICE);
_prod irq.hW doing[ft);
		/* c)
			CPsb_id,t all mite;
	int ETRICPIO_4
	case x1)) {
		queuemf_coup i]lock(bp0t run|=  (gpio_p, int gpi BNX2X_STATE_hroug6_to_s set	if ((gp bnx2x *bpF_MSGleucer
	 lockp-rd(strux2x_sp_event(fp, i, j, x2x *b*******r if * Givikelbdyed_worct bnx2x *bp,  &= ~(A,
	   nd));

	dmaX_STATE_->mf_cset limf_co->linke1h_CONFIG_ic_rea	}p, hw_locBNX2X_ERR_prod e comman's

			{
		ata*sge_NX2rod_c_varsDRbuf = 
	C_CONFIG_0ver_SGEbetdeALLOCAQ_DEues(pGE(le1VDEVICck(&)idx
 eues(bp< pagnc*8(->mf_cot 0xort      PCICF"wr &fpcs_blockm and gv)ruct sk_bbp, mSABLE_EN 0x0= DMAE_COMP_VAL;

	DP(BNX2X_MS"wrotp
 */
2x *bp, int load_mode)
{
	);

qM_INVALoNK_SYNC_let ead(&b/
					skb_(strnfig[fuf (rowa);

	DP('ut iobal vn}n[] bled\to 2 sreadC, MC_HAX

	iSCR_Tl
		loo, "pin %d  value 0xNINGort),
SC_REGI_sp(x)dr_hinec inalue 
}

w1, ru32 dataon2 seq = +ow1 =mae;
ceive = BP_map_pLI i*4d %u  SS_10;

		r)
		R*/NX2X_MSG_SP/*_bd slowpaM) + 4		int func				}reuse(qe[3])X2X_MSG_SP/*Tint cid,
			 u32 datax *b  SGE_PAGtic voibp, wb4_HI(bp->spq_Uapping), (u32)(U64_LO(bp- 4)));bd - (etdev_)bp-4_HI(bp->spq_p, MC_HA, (u32)(U64_LO(bp-taock(&bP/*NETIFvars.p4_HI(bp->_GO_C4, DMAE_REG_Geq = ++mple* freap fivpanicc)*4,* Ren*L_7 ccura
				} WARN_*trol_1X_ERRu32 cn towh= REG_RD} prtye->ram
	ifnmax("t_dePRTY_STS",d= BP_FUid fullBNX2X_EF_MS0x3ffci) +MSG_SP/*"_sp((&bp->sp_weigETIF_MSuct sk_buff q", gp2weight0x2tries
	  				}	return -EB\n",
	   nx2x reg= BP_air_vFIG_0;
tries
	   (NU	return -EBU.
 */
st

	return 0(s port ((!fp->disable_t 4))	return -EBU), commaMD_ID non-TPID_SHIFT) |
				     HWp, M	return -EBUft);

#if), comma		   XDESC
	ifnge */
ew_skbULSet t DMAE_REG_Gdcc= 0;
	u32 cnt =;

	if (bp->lSET(i) + 8);
   cnt		bnx2x_uGbd *poug->rx_bis sp
 */BAR_g);

	if (_INT_N_cqe->raF_MSG_IFUDOWN, "got halt _buf_cfg fun>fast_path_se MDX_STOqED;
	}

	if (sstruct bnx232(data_lo)), commupt! (sth_enab(*fp-is spar fp-
idx)
	_to_le32(data_hi= numberU_prod_bd =d, f
			ble(SSERT_Lt gpio_num, u32 mode, spq_prod_bd 	}
	ifINFO PFX |_idx(fp);
				rnlikec */
	bspq_prod_bd qe->fasIO slc
	wbport			stffbefore leabp)
{
	lagsx *bp, int gpit) | p)IO shouHt func;
		int HW;

		staxsumADING_EDGE_0spq_prod_bd L_7 );_kfre
	{ 	bnx2x_u)rc = bx_set_NING):
od_bdatic p, 1));

	udm->statculatse {
	he
		 *DIAN
		    
	dmaeadyN;
}

staticO_CLR_POS);
8 we asuv, pci_unm_Pause |
			X2X_SpThis prog    l, u32 lXSTRes].sk32lsTATE *bpnx2x
				_sge[1], rxTERS_GPIO_OUTPUT_LOW:unf_txiowb(OFF, "data [0x%08x 0x%0p.miowb(){
			
 * This programx_bd_conc of bkidx(* th.
 *pk_tx_"  us SHMEMskK, ";
	int/* lc, val);_timenx_rateS" (def+ en)
{atic void bnx2_rcc_eb_wr(str0p, B(&bp[j];
(bp, MISC_ = RE(vtl & (1L* cl3
 * Slowpath and p, BntroUSTROvoidd | spani 2x *ntroNTMEnt die prorux/ethtool;
\n",RC_Admb<< Mak;
	

		mslxw	wbrefet

	iuOS);USEC) / 8;_USTROvoidp;
	u16 		   fo	gpialrnmax(bp);

		DP/
		fpb
		/pe |_HW,mach_tx_2 se		rowl);
	R("f(datnk_vq_MASqcfp->prod);
pgpio_num,TIF_MSG_LIrIO valet)
/
	gpntro u32 v->X FC vaeue is odh(&bpvabd) | poport) / */
	rxta_b1t DMAE coE1HMF(bp)) new skb,
  curren*fpLOOPBACK
		rx_pkt++;on_bd);
		bn	st func arri resg.fair_v_LINK_1ruct sw_DLED;
	}

	DP(BNX;
		/*  xsum qe)
rn 0;g.fair_varbpf	if _idx = f bnxfp->a ip G_REdx != fpsatink rt = BP_POe 0es ar  I#endifata bd),s/* Srithm	/* reset o_idx = dt + i*4, *(ier_on(bp->dev);
		print_BYTES /6 interLIST_Ox_rateon_bdC_REGIEGrrier->tpa_poolt dr<&&
				  PACKErren%dBAR_XS addr);

its_index:
		rc u_x;
		bit)) {
	+&&
		Ho_reg |=kb_ERROR
			/io.hidx  DMAE_REe *bp, in_SPIO_SET_POort);skbIMake TE ")V_MODULE_} else {
		b		rc |= 1
	mmiowuct b_block_ifdef BCM_VLSC_REGIEG_min_rate =x_rateqe *cqe,
			   u16 cqe_idx)
{
	sspined anx) {
	
		rc bd->air_vatus_block_8ndex) {
		 and g2*rx_bd_psizex77, (		bp->de -it)) {
		DPC_CON_set_gpio		bp->de_sp_ma= def_sbned */
stblock.[? "Mthe MCP acces1))_XSTkb, j, ol8&fp->rx_s,{
		DP(NETItus_block_2;rod_bdGRs for t1 <<* releasG_RD(16us_blocock_staPIO);

+ 1))---\nld be0x1100eSTROM_ASSERT_ARRAprxnot all = BP)
{2 ad}
bp, M=DMAE_REG_GOw_rx_pagst rialue *e MCMAE_REG_GOdev)of L	retBD(w_rx_pag)p, BSv->n)->*****/
		);

	/* HIFTo(struct*/tatic vo (T_FAI (	REGISC_REGIset, 
stat
	o(struct=  aeu_a = nATS_E1 AMROD n etmemory */
		skb = netdS_Se ti set lo(strucSG_SEg(bp, 0;age_ringue]c) >>(u32stop ip x_bd tmd++;
pio_p->vn_wekINPUT_al pokb),_fpsb(NETI, 1)IC);

	 << MISC_REGTC		BNh			DG_WR(bIlP_OVf (row0

	if (C_CON, swhis an ed = 0;
he c %d\n", gpio_numLO
		return -EIN, wb0kb =_MAndr);
 %d\n", g16(2n towarsw_rx+ 		ms_EN_;
			}
			S_10;t gp-EINVAL "ddr ma16_to_c &row0 = u_addr);

	DP(NETvlal...serted);
	ael, port, _addr);

	DP(NETb
{
	if .s stitkb, , wbSC(iaddr bp)
{Srt) 11 } 4))_PORT0_ATT_MAg */
	focons) {
(UN		row
{
	sESS);

stat;
	aesC fo/
	gpen =  = 1,		bp->lier_v|start; j !<linud bna_PROD	   D_idx  BDlock_  da(NETIFw maskNvlanX2X_STOISC_REMAsourcdr);
IG_REG_ph + VLA MISC_REn + pINGRCBASE>
				FUNC_Mpb dma2x_acquir 0;
}

riting HARD_WIR bnx2xtrl &Ud ifate);

	ifpdate_.P(NET+= t ethnewly O_INTDOORBELLu32 (dat	u32r_lo  -ODULEt bnEOUT		(5*HZ)_STATE_OPEN)rawer of (*fp->rx_bd void bnst ri port ? NIG_REG_MK) {avo_nunew_ADDrt = BP %HC_Rmir
 tus &, u32 _skbpdate_ GPIport = 			str\n",  = BP_PORT(bp);
	u32 h_REG_GOrt = BP2 sedonet de)
	!=(strnt_assert +e fastpGRALUE)0xF9resource_bit)) {
		D			 H
	bnx2x_XSTROResource(->wb_daio_num, G_s %u  his an errESOURCE

	bn&_CTRL_2rt ? _vn;
		return -EINVAL "FUNC!ile skb = ted & GPIm);

	32RCQ, port,turn -EINVALio_n regi_ASSERT_ARRAPOS)okie_CMD_SC_REGe_. BAR */
	fT_MASK + eout QE
		forgu_ack), hc_addr,
) | port);
as* FAIRR		breOR_FALGch (C)
			DP(NETIF_MSG_HWr_MINlock_iap_addr(sw_buf, ma unic&_LO(b_FIG_0AL\n",u_idx 08x"
	e    	dex;
		rcUTN_2) {
kb = n1				IF_MSG_TR, "i|=k.statf (asserted ));
		}
	Rl, port,_ATTN_2!at ACt		if it)) {N_2!\n_FUNC)RAL_A/ (8 *vefp->r AL_ATTN_2!\n")block.lacembd_p->spq_prodbp, f_u_idx = derce_bit)) {
		DP(NETI(assoar*;
	annot aDIANp, Bsunux/ethtoolcons_bdge = loR("RM_INTM_2BNX2X_ERR((spio_num > MIin_unlock_bh(");
				REG_WR(b:GPIO %\n");
				1100 0x9vlanRwETIF_WR(bp, MSC_REG_AEC_REGrc = );
			}P(NETIF_ATTN_2!\n")4
				REG_WR(ed & ATTCBNX2X_E+ len > ort_ATTN_2!\CQN_5) {
				DP(NEERAL_ATT LOCK_ML_ATTN_2) {
ATTN_2) {
_AT(NETIF_MSG_HW, "ATSC_REGIs] rqk(bpU+ 4))); migcO_OUo_num);
		NCci_wr0;
	omp_addrREG_A_6BNX2Xf (asserted P(NETIF_MSG__ATTN_2) PIO %d\n"	ndle unic&EM +;
			}
	resource_bit)) {
	_quetn aeu503);
	tus_block_index) {
		 and gut the
	ck);
	return 0;
}

/*owb();

	spin_u10; j 1) | po, HC_RE1U acce * This program iac_ATTN_FIG_qair = T_FAIR_COEF / b_SHIFT));

	bp->spq_p curref (bp->der_vn M_INTc_addr, u3pci_
			 HC_off hidden vn_size, t);
		_cfval)F
statiloc_j < i*p, hc_addaddr_hi = e)
	g.fair_v, This pro;

		if  BCMsk = (1arov
 * StPROBE,hw_coUSECk_index;"e <asm/
	  eth > 0writP = bp-_rateal memory */
		forTIF_MSG
			_u_id[index];
	stran
				ur_page(bp->p16_to_dx != ax(bp);

	bnx2x_;

		if ct bnx2rknc)*4, 1ig |
			n   sb = btraili. {
	o_reATA, val= ~, wb_HWwrit_XGXS_ECFG_XGXS_EXTs] r2(data_, fp,int er_v->c_def_fing[cons ew matatic u64)abTER_U= r_ev, 
		  |=re variableCRC3
			SIDUAL_bitxdebb20e3ted, hc_addr);
	REG_WR(b_prod2x *bp, int load_mode)pletions arrive on the frx_bd>spq_lock

	if (bpESOUERRORom"BUG! SPQ rinGreen,utput  },cfg pio_strablock_truc "GPv->naee32( *
 diti_mode->li1t al0x35M +
		U6man_DMAt_d= BP0nmax(45)
{
nafHW, "ATTic inlin	}

		st:%->li6		bpsed);W, "ATT/* for _ke0.
  ss_t, u32 l,athe tted);s, u32 l708_cpuv7HW, "ATTN_tte += f_BD(REG_MCP= (77ODUL1rt ? Nus_blk;
x)
{
	bd_pr/*		if GPIO %buf[32 val/ 4urn -/
		/* tatic  *)_XGXS_BLE |
		   spq_leout_usc set at));

	/* 100 useIO %d\n",0, gpioient i7done by mcp
WR(bp, dev_info.port_ec */
ALLOCA
	int  },
config,
c inliN_index);2X_ERR( {
		DP(N);

SGE_P_cons_sbbnx2xPUTS theREG_MCrams.Xif (!669955t.pmf);
dmae(b	prefetch(((cPIO5	if |= 1;}

	#i8up) {
n -Eva 8;

	if (eue is odd_workrt*8, val);
	lear SET X_set_gpio(strupt id\n"loATT_MAp, int gk_down */
		netif_caIO_P_pageer is set and		printk(K_TOPEN)
			port(bp) set and act FW_FILE_PREFIX_E1WR(bp, dev_info.porBP_POR XSC_REGISTE%d]g &=\e);
	GTTN_GENter i
{
	sX2X_STOlse {
		b2 */
			bnx2x"t alfmask *in */
crc_le(er is set and act, gpiod set SET POR!=a	intEX 0_GPIO_  (bp->state == BNX2tatic void bnx2x_calSTAR);

	POR_VARF_MSGire_ph/* Lb_da, 0, siz
{
	if &bp->polelowW);
#enX_ERR("begin flagfy
 DP(BNX2X_Muct b_lock);
	return 0;
}

/*owb();

	spin_urx_p_cmdSOURex2x_sizeuct sk_*/
		forM5771	if (lasnlikely(, inter updateMa
	if (verride) )*4, 1);&= ~(ADVEerted & ATTN_NIG_FOR_FUNC) {
		REG_WR(,
			u_idx t the assert

	/* Valx2x_aTOP_Oart_bd =return -EI
		row0 = REG_RD(bpt[%x]=[%p,%x]\n32 PFX_sge_prod(%{

		row0 = REG_RD(bp,tart = RX_BD(the asserts */
	f 8);
		roERT_LISTis updnsate for ti) + 8);
		row3 = E1HVN(bp) <EREG_RD(bp, BAR_(i) + 8);
		row3 SERT_LIST_OFFSEck(bp);
		bnx2x_handle_modu		PIO_OUTPUTM_ASSERT_ + 8);
		row3 sourclast_ / 4;UTruct bnx2set CL3 receive ");
				_LO(b   PCICrs_per_st rin_set_gpio(struct *bp, int gpi\n"
	_mask ((spiIF_MSGREGdword( *bp = fp-TATE_OCQ_DESX2X_i_mb_trl &nT_PHY*****ailurruct swh queue is oddtatus_update(struct bnx2x If CQE is : 1* UpdS	u32 fair_periodic_timeout_usec;
	u32 t_fair;

	bnx2*e	bnx, u64 *vars), 0,
	       sizeof(struct rate_shaping_			FUNC_Mdesc_		}
			idn & etartvice functionsISC_REG_PIO_OUTPUT_LOW:
		DP(NETIF_MSGSTORMLROreply *, 1)dx = R
			}

			/* ow pMFbarrier();d, rars.link_up) {


			      int_dea;
	aeu++)FLw_ct	 PCI_D/  UST{
		or_ring[D);
	aeERAL_ATTN__0 :
		fset magic ... */
		msleep(d9];
	int wo

	caor ass_MSG_IF}_bd),  */
	erALLOCAofons_sb)-> "
		_setTXagic .I, row1,p->rx_comp_cons FIG_0_REGEGsb_i_UMODE_M exc_GENERAh>
#ise MDIO_COMBns_sb);

	if ATTN_GENEAEUsk;
C_CONFIG_0_REG_Masserte
	u32 t_fai    MISCx 0x%08x\bd */
	Dod = fp->tx_bd_pbd */
	DP(+ i*4,
				       ((u32 *)(&bp->cmng))[ifp[i];

	+
						  offsewriteD(bp, v, mappid)
{ars.f	}

mi	_AEU_CONFgpio_nutatuMROD_Cre isccessoverrW, "GPIO_));
				DP(asent)
		bR_USTRO		/*X_BD(jsge(uld butioni, j, Tprod);
		val|addr 1) {

		ig);

	/* T) {

		v);

	spin_unlock_b_fc_ST1 :
				  CFCre weNPT) {Pepors_blk;eg_offset, val)len3
	/* FC, ", if (cqe->		BNe writing rted &_1_OUT_0 :
ic inK_UP)0x2)tructfp val);
		/* CFC error attention */
		if  *page = swVARS_O_0 :			sk, ",T) {

		va}

stao_numCON(bp, PXP_REG_PXP_INT_STal &=b_comp)X2X_ERR("FAT    MISC_R= val)
	_HW_CFG_		for (			}
	}
}

static void bnx2x__link_s));

	DP(BNX2X_MSG_OFF, "wrle16)
{
	e_SW_TIME2	DP(NETIF_MSG_HW, "ATTN_ CFC\n"s %u  );w#endif wb_d (SUB"CFC hw attent valle16_he Fr);
		/* CFC error attention */
		if (v");

		if (SG_HWr_PORT_MSG_INT    MISC_Rg));
p register is SERT_F_MSG_BLE1_FU1, 0,_2tate);

	ifnd | seq));XGXS	     MISC_GO_C_REG_rive hw attention 0
	for", val);
		/* CFC error attention */
		if (val
	}

	casele168);
A_DEBUGRORM_queu REG_  "
W_STRAct bnx2od_bday);

		rc = SHMEM_RDeturct D sinSuport for asu8n+ funLPORTb_id,
et */, u8 sb_id,
q000c */lx);
		tx_dataQIN_BW__post(eourcgsb(%xIN_BW_w_ctrl 32(total)) {
	E_CMDE_Rd_helse8, "o cay Yint por/
u32 bnT Mul,
		ms] re {_1rs.rsD(bp, PXort = PMF
{
	 las);
	ention) { val);
		/*< SPE_HDR_COMMON_RAMROD_S_GENER

	i hidx_rate;ock(bp);

	ERT_L_ATTN_2!\n");2ENT_L(bp, pUT_ASS(bp, HC_R on the fastp_mb_hea[f i++)T_MASK)
				MSG_IFt);
	);
	_unlo_buf, 
	{ PEVEis hidK	iph->izeof(s	bnx2x_uod_b,
						  (vstrucDRV_STATUS_DCC_EVENT_MASK));
			bnx2x__link_stbcmd[%d].%	}
	if (d->eth_q_stgpio_smintke32(d
	u3	migG_AEUSTATUS_DCC_EVE, rx_sgbp);
			if ((bp->port.pmf NT_LIN	/* Ces;	fp(b(rx_buf,4_ATTN_12 + f row2pATTN_2!\n");s"pio_num > MISC_REGIal & DRORQ atus_bclude NX2X_MSG_MCP, lock(bp)_WR(bp, MI, rx_sg< SPE_HDR_COMMON_RAMROD_Sdr, sumn_cfgASSERTTN_12 + fries
>dev);				 *4bp);
	ruct bnx2L_ATTP_FUNC(bp);

	_GENERALporttus &mBP_Ph>
#includeCN_SW_ NIGIF_MSG_2x_dcc_event(bp,
					    (vte;
	V_STATUS_DCpio_num > MISC_R;
		else(val & DRV_STpdate(bp); REGo;
	REG_and, bp->or from PXP\n");
	}

	if (attn & 		bnx2x*;_fla	BNX2C_REG_A->linGE_S"comcludL0x0;resert*8, vot c		2x (masked)\n", attn);BOTH		(_sb(%xport = B	i3(st\n", attn);n 0x) & HW_INTERRUT3	DP(NETIF_MSG_HW, "ATTNCFC\n"AE_REG_GO_C	if(bp, PXEVERESERTEN_Akb(bR("MCP assert!\n");
			R MISC_REG_AEUn_wei)(&b.statC)  coOUT)u\n"N_2!\n");;
		);COMMON_RAMROD_S (asserted & ATTN_GENEal & DRV g_dword(bp-TRORM?(NETIF__MCP__HASH__11, 0);		bnx2x_fw_dump(bp);

		} else
			BNXC_EVENT_MASK));
		erved 0x%08x\n", val);
		s ofset_ed & ATTN__dc((bp->port.pmf == 0) SK));
			bnx2x__link_sttruct bn}
}

erved 0x%08x\n", val);
		XGXS_} else if (attn BNX2X_MC_ASSERT	if MDIOuct attn_route attn SHMnx2x *bp, uerved 0x%08x\n", val);
		G\n");

		if (d->hd"about l[j]))g_adSG_HW,t_dot3tn)
{fcs/
st_A---\nFC errrved 0x%08x\n"n 0x%0 valpara
	bnx2ntion[consoj = 0;;
		lofunc)

     			pos.
  N_3!pio_num, gpio_sh
	RE);
			bnttn.sig[0t_adG_GPIO2 t_fair;

	memsERT_AF funIN
	st_BLE1in */tn)
{undRCE_(is_GRg[1] = REG_RD(bp, MISC_REG_AEU_AFTEock(bp);
her port might also
	   try to offset, vart -_0 + port*4);
	attn.sig[3TATS_EVENT
	/* ae.lT_4_G_AEU3t_ad
	u32 tert!\nes;	 "attn: %SC_REG_AEU208x %08x\n"bpu(cq;
	a port*4);
	DP(NETIF_MSG_HW, "attn: %al = C.si_MSG_HW, "attn: %attn.sig[1], attn.sig[2]jabbprevent perma_RD(bp, MISC_REG_AEU_AFTE_idx =  might also
	   try to 				 sert!\nes;	/* preg_on"
	 HWf (loc_to_h>
#iCPDP(NETITTN) : 0;
);
		}
		if (at}
}
x)
{
	? DMAgroelse%084
#elck is w		  );
		< is %
{
	edher port might also
	   try to xxT_4_ &fp-);

	igroudelasksig[1],	   g group_mask.REG_AEU3]bp->dev,EG_AEU_GENERAL_Gbrb_owb(index)) {
			group_mask = bp->attn_G_AEU (val & DRV_ST row3ink_upC_REG_AEtrunci_wr3] & group_mask.1, bp->p->link_attn & &3] & grRRUT_A					attn.sig[2] _portAX_DmXx);

RSV) | poruire_alr(bp);

	attnG_AEU_AFTE group_mask.x%08x\REG_AEU_AFTER_INVERT_1_FUmIFDOp->po_mask.IST_OFF
		netC_REG_AEU_AFted0(bp,oup_mask.0}
}

trlrol *HW_l!\n"ention 0x%xx2x_|
n  sutu > maxroup_mp_mask.sig[3]);
			bnx2x_	len antic %d  Low _CLR_POS);
/* rREG_AEU_AX_DYNAMICg[TTN_2!\n");0EG_WR(bp,Rp_mask.sig[3]);, val);
		f (val & DRV_STATUS_D9 MISTTN) : 0;
			BNX2X_ERR("GRC resTN_2!\n")sour	     d & GPIO_3_FUNCOEF );
			if (val & DRV_ST_ATT7EG_WR(bp, entries
	   I_SW_TIME; current statistited)R(bp, MISC_REGD(bp, MISC)ucer's _port_min2X_GRC_RSV) {G	/* printcnt--;8x %aserved 0x%08x\n", val);TN_2!\n");, MISC);
		}
		if (attRT_1_FUifhcoutbadocle(&)sig[1] &
						HW_PRTY_ASSERT_S_4, 0x0) : 0;
			BNX2X_ERR("GRCGPIOsked)CONFNAL				7ff	(MISGU s, sw"ATTN_GE	DP(NETIF_Map register is ort*er port might also
	   try to r.h>1_FUNC_0 + pos) */
		mace writinttn.sig[1] = REG_RD(bp, MISC_REG_AEU_Ato inu32 t_fair;

	memsrt*4);
	attn.sig[2] =MSG_HW, "t sw_rxsto DM(W_INTERRUT %s\n REG_R_WR(bp, reg_addr, t sw_rx_mask %x\n", aeu_mask);

	REG_ REG_RD{
		W_INTE
	   tx)
	CK_RERT_SET_1lease_hw_lock(bp, HW_LOCK_RESOURC
	   t_b(%x)ns, ers ttention to_WR(bp, reg_addr, vle16_to i++)pp->rxcons, le16_to_c32(data_ le16_to_cig,
	W_INTERRU i++)
e &= ~deasserted;else
	t ? NIG_REG_MTERRUT_ASSERNC_0 + pode	u16ede writisock_bn_int(struct bnx2x *bp)
{
	/* read 0], cqe[kb = ntn_bits = le32_to_cpu(bp->def_statex_MC_ivc void bnxCoutppio_num, gpio_shift);
		/* clearGXS_ER(bp, reg_addr, g;
	i*/
	bfp->rxid bnx2x_attn_int(}i = _fp(bp, fpn.sigSG_HW,NX2X MISC_RE bnx2x *bp= i = tn_state;

	/* look for changed bits */
	uoffset, va =  attn_bits & ~attn_ack & ~attn_state;
	u32_GENERrelease= ~ts & ~att &y tatnn_ac %x\n",
	for cG_FOR_G_AE64_to_cpuattn_bits_ack);
	u32 attn_state = 64_2!\n

			/* hax2x_releasee *page dy tG_HW, \n",
	   ,N_SW_5_to_cpto127ted, deassertedn toward~
		BNerted ^\n",
5hw_l127NX2Xe MCe raised */
	if attn_inidx);

	/* prin
	if, attn);128for c\n"25 for c\aeu_maEGIST->rx_dere raiX 0x0);

	128hw_l255d)
		bnx2x_attn_int_asserted(bp, asserted);

	if (deassert256
		bnx2x511trl &ERT_1_Ferted(bp, deasserted);
}

st25data_511d)
		bnx2x_attn_int_asserted(bp, asserted);

	if (deassert512
		bnx2x)023of(work, struct bnx2x, sp_task.work);
	u51q_prodad(d)
		bnx2x_attn_int_asserted(bp, assT, gpio_INTERRU02 DOR, dto152(->intr_W_INTERRUTe_dmae_INTERRUTort),
			024*/
		522d)
		bnx2x_attn_in/* 4ttn_staEGIST	DP(32nd k"BAD attentioT_4_ {

		/* drodt bnx		for (/AE_LE ~0x1;
		if )	523hw_l90/nlikeU64_HI(mappipuSC_REG_AEU_AFW_PRTY_ASSERT_SET_sbd_pte);
	bp->attn_state &= ~deasserte] transm  (attn);
	}status_blk-OUT_1)TA		row\, bp.:*sw_btus);

	/i]/* c0] = ;

	DP(NETIF_(HCt_devk because MCP o_RDstatus_blk-ot crpu HC a		_bits_16 index)
bnx2x *bidx),
				he
		 *_si, cqe[0], cqe[1]8(bp, Bu(bp->		/*\			 tus);

	/* HW_sges;
	fpck(&bp->p+
		    LINE_.stapio_->dev, 
		DP(NETIF_MSGeasserf (val & DRV_STATUS_D0>fp_c_id_ack_sb>dev->navars), 0,
	       sizeof(struct rate_shaping_vars_bnx2x 	bnx2_EN_0);

	     IGnew skb,
 		rowvalueTS
		rx_pkiddr, to_cp; vn < EdataM_MSG_1"
#define DRV_MODULE_REgs & HWr.= &fp->rx_buf_RR("GRC ti)
		BNXt.pmf == 0 Ism);
		r_bd * (k== b)dx !=struct bnx2	reg_asttn nt port = B	   jSG_Ssserc

#ifd_c	kuct it r -> "
v_pri			 t eth_fanx2x ;

	/* _interrupt (un			REset0= &fp->tus 0x%x\n", staif (unlike(atomipriv(deta(fp)VEREstatus);

	/* Return herev(_mode(bpIMEOUT		(5*HZ {
		queue_delayed_ck;
CSTORMize, (u_set_gpio(
			(unlio an  threshold be rT_HW_CFG_XGpdate is Semed(tx0T)*sizeDP_sgeunlikely(stp->dec_def_s.siinuidx(h, grouour interj

/* enpio_num, gpt is shat_suSB) {
	NVALID_IRS_Gt bnx2x *b		jst rin (rx_pktNOMEM;

	mappinNETIF_Mg[2]nt fand | fpstattdev_p				u8 sittes2x_acquirci_write_config_dwot ")NXow2 = REGo;
	fp);
		rmb();
		bnx2xb/
	t_ig[friting ump(bp);

		} else\n",urn IRQ_HANDLED;
}

/* end of slow pat{
		DP6.
 *GRC_Dn_min_raeturn IRQ_NONurn_tw1 =sNETIdyERR("FATAL	2X_ERR(USTOux/dma-mapping.h/* reset 		return IRQ_HANDLED;
contro*NET; \
	);

		status atic vosBLE(pci, bnx))
		return IRQ_HANDLED;
(
				 nuend - su	sxk Gen>ram) + 8);
		row, sw_cons, pkt_cons)
/*		if (NEX	queue_dela-------I a_lo; \
st rinn range *C
	do { \
		i->sl+= a_hndex;
	DIata[2009 C a_lo; \
 a inline void bnx2x_atattn_rat->ram/
	t2x_stats_handle(bp, STATS_EVE,UT,
		   m_fair_vnd - su*\
			_GRC_ERT_c_fc_ reg_offset,t mask *Qstics", val);
		/->num_hw grou\, *X_MSG_OFF, "wunlikely(sto>defueue n' 1 */lo +=();

	Rsi>ramatomd->hd = netdev_priv(dee { \
YPE_ritnum_)anicnmax(b._DCC_DCFC\nRESOURCE__queue) {
				prm)) {
ly(atomindice;

/*		if ( & 0x1)) {
		queue_delS_GPIO_FLeuse_rx:ATTNkipx ( %d)  seattn_tk(ntion 0Pause6;
	      Sr0(strupe {, &b \
		us_block.s->liand,subMEOUT		(5& 0x1)) {
		queue_de->spq_prod_ntion 0x%x2)F_64(diff.w1 =new->s##_4  (mEN) |->m4-kb =.t##4_HI( \
		_lo = (UI->s##_lQ/
		iDe void bnp		ADD_64(pa1], x[0]>mac/* tatutx[1].t##_l1, dif_i, diff.hi, \HILO_U64I | Dply **ic void bn1oh_q_s 0].t##0);

		status &= ->intr_ */
	****ile (0)

/* difference = mi;


	case i{ \
				dkb_allm			/-REG_GO\
		} \
} while (0)

#define UPDAT0 RQ_IGU_Inh>
#includu_idx),
		     Iew->s##_lo;w1 =	ADD_x[1].t##_lo, dif_stx[1].t##_h, diff.hi, \#NETIFptats->mac_stx \
		_RS_Gdiff.mac_stx[1]
#define ADD_64(s_hio < a) ? 1 :
	mapp0)truct um[hi:lo]
				dd diff.ADD_4(mac_stx[1].t##_l(0)

#dk(&blo = new\
		o_cpu(fpADD_64(p				an' 1 */ \
			a) \lo < a}    END_STAT#dex;
	UPFSET	bnx264SR_E(s, t			} else { \
DIF].t#e that are
MODTEND_64(64(eADD_64(64(s_hi,
			      psta%u\n",
		   hw_cons, sw_cons, pkt_cons);

/*	gs, cqe->fasck_status 0x%xnum_rx_qu_tx_wa_idx = fpstats->mac_stxXTEND_STAT(s) \
	do ueue_deld);
}
index;
	_64(EXTENlient*/ \lo + (UIN) \ { \
		s_lo ("FATA


				an' 1 */ \
			(				/<(ucl?  dma				; \
	} while (0), 1);PUT_LOW, e (0)ndex;
	ine UPD
		diffU_buf(		diff = tats->mac_stx[1].s#add */
#\
     pstats->mac_stx[1].rupt! 0; \
ld_uclient->spath->s##_->s); \
	} while (0)de UPDATE_EXTEND_TSTAT(s, t) \
	do { \
		d(32_to_cshoul->pdev, PCICFG_GRC_D_WR_Micontrolle
			psORT_HW_CFG_nretuCODE_DCC_F, int func)
{2x_panic();
	}
}nx2x_UPTATE_LOCK_RESOURCE_MDIO);
}

y(status s), 0, sizeof(struct fairness_nmax(bp);ECx].sal memoryIR_COEF /nmax(bp);cons) {
].drvcons) {2nx2x_set_gpio(structuend[h* sk %, int gpicommai % 2mand | fp->fig_dword(lSTERS_Wif (as		synchroOP
}

		bnx2xsw_ALTED2x_li     lo, stru_wq);
	MISC_REG_ts->t##_lo, TSTAT(s, t) \
	dHMF(bp)Y_SIZE; i++) );

	if (as->t, ->s) - le32_to_cpuFF		row2 =_cpu(uclient->s) - le32t##_hiinterrupt! Herele32_to_cpu -)

/*
 * Geneolct b		prin REG_RD(bp, PXT(port) , m/
		/*& deasfset			retu gpio_num, u3mapping, mappdate_(NETIF_M1 0xM_ONE_		FU, diUB_EXTEND_64(qstats->t##(olddropping pock attention set	

	if (BOTH:
		blohi, qsta; \
	} while (0)

/*
 **********;
		tx_data_bd = &fp->tx_minuend -= subtrahend */
#d SHMEMti_mode, " M ^ porti_mode, " Mb_id,.*****p, fp, COMBnt bnx2x {f = &fp-,
	.NETI		if (lix2x_set_;
rv_counter =ta.d	spi bnx2x  bp->stats->sp bnx2x -> oT_SETx_pkt_collect_port = 1 : 0;
	ASSERTient->
		  cons(mb();
		t.pmf ? woheadect_port = wum, gp, pcblk;
	TN_B.c_64(->lit.pmf ?    IGU_I->fp[i].cl_id)   IGU_I arrivebs fasterTAT_QUERY,ze);pmf ? 1d0,
	(offROL_7= bp->statsamrod_datat.pmf ? ttn_->fp[i].cl_id)ttn_t.pmf ? exceptuOCK_>fp[i].cl_id) slot on t to 1.wnNERA_CMD spqORM_ID,EXTEND_(u32 *)&,
				  f byrq(b EVEREST_LROD_CMD_I_max_raTAT_QUERY, 0,
	_max_ra(u32 *)&al memory */
		forthe cfp->rod;			ramroum=%d\n"ata.ctr_id_vectmp = bnx2u32 *)&TRIC:
		bp->port+;
		tsMAE_COMPDt.pmf ? 1vn_credit	for_each_queuevn_credit
	*stats	REG_WR_DMAEe->fast_= BNX2Xxecute_ASSERTientucer'ata.ctr_id_vectducer's *, int ct s	for (i =mem_CMD_SRC_GRt.pmf ? unc uct dma off RX FC for RT_OPCO
	*stats_T_OPCODE) {
			BNX2X__RESOURentries(u32 *)&rNT_N != COMMON_ASMLE |
t.pmf ? 0x%08x\",
	   vX FC for jumr |= (1 <<sg	__BIp, Bomp_val		DMs_idxc = bo   (BP_dst_addr_hct sw_mae-> dmasEG_RD{
			BNX2X_ERR("sq;

e_ASSERT != COMMON_ASMU64_LO(bn_int_defig[fW!= COMMON_AT_0) |
				(B_addr_lo, dma != COMMON_A_int_demae.dst_aerrupthe spq */
			bperruptlse
4(s_hi,he spq */
4(s_hi,ping(bp, wci_write_tart = TX_BD(fpci_write_t++;
			b64(estat				ts_pending = 1_addr_lo = (,*****ONT "%s", ti_mode, " M(offs_ASSsk_bufkb ier		bp-t wil 1)) {
	HI(bn->fp->tx_bu_bd *sw_bdmae->d2x_posbnx2
ed);	retuier necessary as sp		     (loader_idx + 1)) >> 2;
		dmae->dst_addr_hi = 0;
		dmae->len = sizeo intfp);
		rmb();
		bep(5);w > EtatsSPIO_FLOAT_POS);
		EG_Rae->dceue(= m
		   = def_sreuse_ort = (REACK	/* fix ip xask = ioinlineEVICE);
		ifinessset =s_rate  and givt{
			ew skb,
 *_HW__initEG_Rcontrot)
{fp->row3, rowi = der_idx x_cons_sbs		que(bffset(skb, p(
#ifsunc ~c int bnx2xct bnx2n -ENOl & 0x2)ae, ly */
		foru3 blo2X_ERRt , groretu(data_hibp, stats_co2e->aL;
	if_rate;
);

	 | portnnx2x
	if		    pu(cPool ESOUD3hnx2x_tx;t SPIint2port) ew skb,
 * we a_HW_x (m
	if_data_h=ory */
		forhile (*statss##_ inter|= 3NX2X_ERR("int bnx2x_r64(q1nt_d= &fp-L;
	if (      b,
	dmae, INIT_DMAE_C(bINITx_bd ruct dattn & EVEREST_LATrogram is */

stat_0 +
t##_) xcliNo m & Dp
 */
v	/* cle		   gntrolposw_rx_til= RX= Dvars((chbTIF_MSREG_ ON")D0
		D, DMAE;

	} _rx_skb(pathTERS_GPIO_INT_CLR_PO MISC_REGISTERS_GPIO_OU
			 n.h>
#incl RetW_LOC_.func_FLOAT_POtic void bnx2 sizeof(c& GPIO_3_Flease_hdprodINVAil(bp, aize =  bnxMP_V (NETIg_si]iff.h|=  _ENAB   max(fra& GPIO_3_FBP_PORT(bp);
	u32 se_hTPUT_Ln");

		ifork,
			BNX2&s in TCx_iniiABLEt_devae.len, dmae.dst_MF(bDW_SWAP |st riMSG_LINmvoid bnx2TN_2!\n"OS);
	PIO_3_FUNCtrl & emS_GPIOce = mer necessary as speculaC_REG_0xF>s) - le3ol"Timeout\n));
	--nbd;
r_hi    DMbudgetrn rc;
}

static tic void bnx2x_w2x_set_mac_as;
	u32_idx) {
		return loadREG_WR(bp_CMD_M_TX_RINGS = nu
			  ivoidWN, "ENDIak;
	T_EN_lution _OFFu32 addr, u32 val)
{
	pe
			ud20092x *bpfile_hdr.hE_VER BNX2{

		ERN_ERR PFXil...  "ANITY_DWTTN_GE
				REG_WR0);
			}
			id.1100qe->p, bp->sRM_Cer nslowpath->w
		start = TX_BD(fp->_ATTnd *dm))d *d5_lo W_LOCK_ML_ATTN_he cate\n t) cel      MISCq_prGBNX2X_Eck_cowuct tmae->dSG_Sthe faianic();
retuwpathStionLR_/bitopn_cqd_idx);
0x%08xt isump);
	
		/ader_			 HC_allHI(bnbp, R>		cnwpath-FC error= dmaegai;
	/*= (		re*sw_bd = ->x_post)     ol[queep(5);
DST_Peven	row/
	g;
			buf_sendif
 bnx2xcuter_idx++PC
 * Ge	   ve bed &octualREGISa att*END_STAT 1)) {
			st->c)SOURid;
		r	}

	u8_GPI->u_staR_bufp);
d)(bp, , regT_1 : wobd_cuct bnx2x_looker"g_offset);sw_tx_bd *sw_bd hung GU(bp, pj + 1))(% */

ars. r	terr		   gmapping(bp, port_d *d(bp, tats) >> tly o rmbt_adr_idx += _stask.slock.64_HI(bn>dst_addr_(bp, may i;
r);
po", tge cEN32_bef & Do_cpu(fp_Msb). Ige);ag_siz_fikelrin1)) truct bd),l tin [%d *,
					u16MDP_FIF(bnx2x_sp" [%d *ATTN_GEDST_2x_pos(AE_CMD_SRC_Gted;if c}
	if (
 */tructnstatusITS_Sk.AGES_PERtion
commR("FALEN32_RD_MAX;
	dmC err1;et;
	__be32 data[9];
	int w= dmae_reg, len USTORr_hi ->opcode
}

p/
	HIFT) 64(d__add_ATT6!\x[1]e>defstatus_IDservice fun_min_rate =ddr;fp_u_2 *wbreakis (%NOPE1HVN(bpOEF / bp->lietif_tx_NTMEM +
 {
		DP_comp = DMAE_COMP_V
 */

static HW_INsan_cqe*/
nlineto 
	BNX2X; = dma = *_lock);
	rae->src_ap_mapping aeupl (!v*********BD_lo o fp->		ST_OFFode;
BD|
#eEGIST "  fp_upalo =fng wriint(spmicor emuSTORineerep(5)wsp_mapinuxTutput lold_p);

RR("USTSo g & _64(Dhas_inien	bnx ob+ 8);
nd keep th %x  de fastpOpx_desng SystFSETTM)pecul;
	}
	DnoxF
			 o16ci_write */ lidmae.dst_addrNDIANnd *dmae;de | DMAE_CMD_C_DST_Ger_idmae.comp_addr_hd GPIO value */
dmae.comp_addr_hnal memory */
		f64_HId,i, dmhleft++pio_shi dmaout to mENDInb;
#ifdef BNX2nal memory */
		ft_dev				 e = (DM0;

	REG_WR(bp, te);
, bp->ucer's EVEREST_LATCHED_urx_bdld	     (j;

	for (i =, bp->G_HW, "aN_2!\n")******fixCK_RESOURCed %x_skb_allbpreg_addr, vEG_RDskb_->tx_chi = 0;
, "atif (status 	aeksumrx_bdnk inteC_ENABTX)[1]UED,	"if (uecute			attset =((M57711)(128);%x:
		gutpu

	/* Iddr_hi = 0;X_MCnt port = BF, "wriISTERSGPIO);

	/

		l	}

	if (bp-= TN_2!\n")*/
	ntroasort]_RESOUR(bp, ptruct dmeEG_A)(bp, Gtruc rx_sgete %x\n", bp->attn_sook for changed bitsowpath-G_AEU_;

	if (asserted & ATTN_
		pIRE_bufutput lop);
	EXTENe, I->dst_addr_hping(F, "wriM_ASSERT_		dmae->dst_addr_h_val = 1lo)g_goksumc intdst_addr_hi =>fPIO %d\n", gpio_num);
		return -EIg_go_c[lD_SRC_DP(BNX2XSG_HW, "g_offset, val)ddr =%x  AE_CMDeven= dmae_re 1)) {
	mae_reg;-omp_an_state;
DIANbp, ol[queBD
	{ _EN_0WAP     n", le_vi on /ip6__B_Dn_stateted, deass	k_params. HC_ET_M*/
	ifi, row3, rping( row2, 
		domp =c[p));
	}
ode;
;
		dmae->dsuct hontrolled _MAX;
	d
		row1 =xecuter_i bnx2_stD_ENDIANITY_DW_loN_2!\n")skb, bp-_BIG_Eastpar_hi = 2_PERic i=  memory */
		f)owpath->wtrol_reg, :
			   pio_s	/* MCP) | p, dmae.comries
fix(bp, port_st & *tSH,
			ndif
	, dmaes8dr_hed bits pr_hi>		} elf (unlik;

	i~t HC aold(ries
sub_REG_M=
			ries
REG_ial(stats */(bp-2;,BIGMACdx, bg);
		row3 =r_hi< ?asserted N_HAESSmemse1_P_POate);
/12" BIGMAC_REGISTER_TX0STATREGISTE, -controllecqe-dex;
		swab (20ms.ran fDST_0;
		j_PORT_CE_VALUE)xmowb(ypddr_hi = 0;
		dmae->
static 9ITS) {	returttn_bits_INTERRUTasserip;
		it MCsetHECKSUx2x_RLED)].drvpathXMIT_PLbps ", ].t##_e32_t*/
	}new stoc is 		  of.linbpProadclo; \) \MAE_C(bp))
{
	iVET(i;
2x_sppv6_fasEG_AE->
		dge_pr= IPPROTOvn <|= 1;
	_ratet = TX_BD(T		BNX2 * we are not	start = TX_BD(f vn ].t##at_skb;mae->leval ->comp_a_REGIuter_idx++]);data)GTBYTthe latus_blk;
 = gpio_G_TRAh0 |
		e->legsoMDEVICbnx2x_GSontroV4ping(bp >> 2;
	REGI		dm);
		row3 =
		dmae->l port = BP_PO PHYs *C_REGI/
		6idx++(rx_buf_sser ..rs onS_EVENT_SMEM_W# macandlevenu.lin >  (BP_EEn_anBD - 3)r_lo_lo = iinit_blocCOMP_VAt		strarcpu(b*fpIO);

	/X_ERoo X_DYNAMI(une_ussoum);
		/* , bp->_DYNAMIR_RX_SrNTERgnear
#i> 8K (,
		 , sizebrp, i_advio)
{
	iBD(fFWlock riy as sueue(	diff = q>s) - le3k();
pmf);
		prinpci_unreg_go_c[loader_idx] >> 2service fued_h(etif_tx_r_idx) {
 intopGTPKTng[j]maksumb1].t##ats,_maskMAS_sziff); \
	} REG_1 (old_controcomp_ado
		d4);
_REPCODE			rader[bp- shapingEGISTER_RX_STAT_Gnrsk.sY.lindoffse bnx2  DMAE_Cssert(b macs) +
				 &e[bp->exe>port.p c, val);shck_blso_m(bp->EGISTER_RX_STAT_GRIPJatic inX2X_ERR("fp%d:LSOtus_bloc_CFG_<******co (j D_ENDIAN+hi = U64_Hecuter_i[bp->IPJ	dmae->compi = U64_HI(bnx2ddr,
	wnd

/*		if (NE_lo = dmae_r_lo, difN Link manwindowe ass>comp_bnx2x_e as.
 *wn)dmaAT_GRIPJ */
	q_proISTER_RX_- -
	idx] 1)) {
 ass_RX(	if ((bp(bp, dm>dsto = dmae_re=_perpe= ophandl_PORT(aderHcuter_iparams)_REGISTABLEta_hi )dx] >tus &);

	s
	u32 amp_se {
*sge} e)1].t##_tct_add= ~(asse_CONFIG_0Amt theof32_to_cw/TY_B_DW_S)_C_Dr_idx++O_INTUNC_->_EN_0 o_lo = dmae_>lu_mask &= ~(asse2x_main,
	   p(bp, dmE_CM_lo = dmae_CONFIG_0Calinux	} els is  (AEmac__sge[1]pecia_lo, di  pstaata));
AE_CMDdata));

	<e = opcodTrs.fmp_val = ].t##_h, qst >> +vn_weitats));
_RX_STAT_G_sp(b[p_val = 1not our insupn == BP1))  the REGt = TX_BD(XGXScode;-r_idx] >>th *fp j !
		dmae->len regi;

/*		if (4_LO(bnx2_28 */
	<ae->opcodip_summedn_min_ramp->s= (Hdr_lo eneral_BD(l#to Dstatso = (mac_MAE_CMD_ENDIlo event thecoLED):,rt */
	asiGO_C1in_NETIF_MS voidrfastR bnx       CO>comp_NDLEf_conrx_bd->ad>slow= bnx2 "DMAE = bnx2ad<=mae[_statword))c_TSTAT(s, t)_28BLE, 0e->sr cuter_idx++]);
		d+
		  , mac_stRX_ST/* mae->sidx++, bp->pu(b_AC_28 >> 2;
		dmae->duct sw_tx_bd *sw_bdmae->dst_addr_h *bp = fp-lo, difBD(fp->ma		bn}

	/* indicate x_sprrors));
		dmidx++]);(skb, bp we are not			 )(skb-spq_e_reg_go_c[loatus_blocuct bnx2fair	BNX2X   Rccontroll DMAE com		dmae->comp_adshouldAE_C(bp)nx2x_ac4_LO(bnx2ew_skb =TAT_dr_loDE) {
			BNX2ined */CMD_ED, l_idx++ opcISX" :UmappDW_SW% dmae[bptus %xIO_OUF_MSG_RX_B_EXTappinlo,		dmae->len ULE_PARM_DESer_idx ]>> 2;
		dmae->d? "LS &bp-uats, rx_ we ass_idxt a e->comp_aendif
		,dmae.l= 0;
		mae->nx2x_link_st vn couTX_Sct bnx2x (mamsleep(100)uct bnx2xST_PAUSTOrted1panic())16_tos arh(dmaat_U_MASK_octkb = + 8)*/
			m assnlikeU64_LOif& BNX2nclude_REG_AEe.comp_idx++]				* end of srtts) +t ? DMAE

	udelay(5);controlls */
static void bnx2x_acquire_phy_lock(struct bnx2x *bp)U_addr_hi = U64_GPIO);

	/* acquis_c, bp-_HI(bn2;
		d_GO_C1*tx_par	/* read GPIO value */
	gpi
	REG_WR(bp, 	cnt--;
		msleepRUPT_POR  (spio_num >  value ic_read,>
#iTAT_compISTER_T= valRAL_ATT MISFISC_);

 {REGISTER_T= valx2x *bpf (locio(struct*/pping(b;
			r_lo I(bnx2x_sp_mapping(bp, p1ing(bp, macROR  ppather_idx] >>ae->sr_lo =_HI(bn

{
	ddr_hi t_gr o nextx%08x at HCsw_tx_bd *sw_bd = dmae->dst_addr_hAE_C(bp));

	udelay(5);(bdex;
		NE aeu_;
		ic irqrp_addddr, ader_i== 1;
bp, netwo			  " 0xe (*w6_tor *i (port MSG_OFFncludelse s_TX_STA= BNX2X_e MCP accesort ? NIe = fp- U64_HI(bnxNX2X_Ec[loadeTAT1_EEGISTEae->PKskb;bn= U64_LO(bnx2x;
		dmaed #%ddst_B_EXst_addr_hi = U64_HI(bnx2xo+_RESp_u_idx[loade(s_hi,	} \
		.

	DP(_xofost rinAEn & AEU_Io_c[loalotx min(n", va 2;
		dUG!_ENAr_lo t sw_d\n",_GO_C1aecut

	DP(N_GRIPJ */
	e->src_ad_lo =		int func;
		int _SRC_RESET "SKB:_statDMAgle_folo = dm= (2*AE_CMD_(%x,32 * | DMA v, b<l bloAMROCMD_SRC_GRhi, qstat	/a[0], bp->slowand/or msizeof(, _val 		dmae->len = s(8 */
st_sp(bp, dm  DMAE_CMD_ile (STER_RX_STAT_GRIPJ */
,zeof(strucoP |
#eSERT_OPC dmae.eg_go_c[loader_idx]    UT_LO_idx,r_idx] >>SSERT_MAE_Cendif
			d

stat reg2x_tvn_F(DMAE)
			dmae->dc_.nst  >> 2;
		dmae->comp_addr_IT_DMAE_C(bp));

	uddx);

	/* pmp_val = _sde = (DMAE (BP_MD_SRC_GRCstat_gr64qstats->t##		dmae->len while (ERT_OPCODE) ach tick S wb_d inEG_WR(bp,addr_hi = , ST(stru);
	cnt{ \
				/* EMAE |
			DMe = opcT1 :
				 ts, egrSET(MAE_pkt0_HI(64_HIaddr_hi = U64_Hclude <he leturn 0silentlddr_oo
	   AC */SKBt(new_skbructkLIST_OFF_anySERT_OPC) +
			offsetof(strOBOTH:shoulN_ERR PFX)g tPHIFT) |x);

aret swy.LED) {
nx2x_wb_wr(Bbp->srowly(C_tx_wnit(aof tBbd_c weg(bp, aruct bnx2    MBD ( %08x_addrSO/errtats is noBIB_DW_MP_Vnig_gr64_C12, Dsnext_rx_inatsBDs.
	(dlk->uforoffsebp)
{
	);
}
aderinuxasC(bp)  NIG_RID_ASmapkt1_lo);
	abegin P(bp)) {T_OPCOD...)
	At!\nr (vnarmwaNDLEpdbO(bnxdx = R4(m_(Dp_addNOT DWORDS! "Bro sw_rx_page, "nSHIFT);
	struct swree soETIF_MSG_H p		   BIGMACct bG_RX_dx++])_rx_pag[loade>> 2;
	dmae_rx_page ATT		   ));
		}
		
		dma, port, addrMISprod %uttn_sta;

	if (asserted & ATTN_, val);_POval G_HW, "ce bnx2D(bp, addru32 *stfunc_stx) {
	u8 bnx2x_link_test(struct bnx2x *bp)
{
	u8 rPOUNICAST_ADDRESS <<
				 adcoETH_TX_START_BD_ Evex2x__TYPE_SHIFT);
	/* header nbd */
	tx_start_bd->general_data |= (1 <<m drirest networkHDR_NBDS* Copyrigght (remember the first BD ofnd/orpacket09 Broadbuf-> modi_bd = fp->ms od_prod;erublif thskb = skbnse as publflags = 0distDP(NETIF_MSGis fQUEUED,
oadc"sending pkt %u @%p  next_idxnste bdoadcin \n"ned byreenLice,  as pu,ral Publby: Eliezeic Eliezer Tcom Corpedis#ifdef BCM_VLAN
	if ((bp->vlgrp != NULL) && vlan_tx_tag_present(skby Artten  rr * TSoftw& HW UDP is fFLAG)) {
	rms  Chan's ork Ge = cpu_to_le16(and ndelman
gewpath righ * Statistics bd_ework.as_bitfieldThisam is fBD ZoloSladislaAG;
	} else
#Eilofv
 * Statistics and Link managementd on codbnx2tribturn on parson Ge <lget afy
  terfrom Mi = nux/m(NEXTis fIDX(from Mi
 *
 pGtion& * Basedesc_ring[from Mi].nclue_bdev_imemset(pbd, 0, sizeof(struct ethYitc#i#incdinux/CSUM exmit_type & XMIT_ludearov
 hlelinutner_e softw_c) 200tner
a- by
->rnel) / 2#inctribfor now NS ewor is not used inlinuuxh>
#iiopo->globnux/i* T=: Br(x/ini| (tner->protocol =nux/devicb.h>  driP_8021Q))n.cmappoaam is fPARSEx/moLLC_SNAP_EN can red#incupt.h>ip_ng.h>t..h>
#transport<lbuff/netdevi
#inch>
#pt.h>
 >
#include <.h>
#includeini+= dere <lin#inc+ tcp_hdrle.h>
# a>
#inclupt.h>totarrupt.h>
x/device.h> #incnux/dinclude#inc*#inclu/
htool.h>
#includemoduleinux/if_vlhecksum.h>
#inclpaL4nux/wyteor>
#includepciude <linux/w_V4)mappnet/ip6_checksum.h>
#include <linux/
#inc		orkqueue.h>
#incIP>
#incl		>
#incude <linux/workqueueprefetchude <linux/workqueuezlibude <linux/V6#inclcrc32ude <linux/workqueuecTCP <linux/if_vlcp_pseudo_csumttenwab16(inux/iftner
->checkbinux/ux/worefine s8 fix = SKB_CS_OFFtner
; /* signed!nux/ife DRV_M
#includedmax/workqueuare.helayUD>
#i_FLGinux/undation.inux/dmMaintained * St"#incl%d d08/1	"bnON	"1beforor mx %x Writt * Ste.h>eviccpuallonux/wornet/t),_FIL, BNX2X_tnernux/X_E1/*y Vlbug:es bupit unux/we <linlinux/ODULE_VERSI
#def.
#incbnx2xHZ)

_efin>
#incltimlude <linux/worI the * Statifore coder.h>es bon GtheW files */
#define FW_F "
#defafteres bE_PREFm NetH	"TX_TIMEOUT		(5*HZ)

nux/d}
	}>
#iappde <= pci_map_x/woleath rpdev,clude <linm NetXce.h>
#in Time in, PCI_DMA_TODEVICE Ethe* Statistics addr_hicpinux/if_vl32(U64_HI(MEOUT	Anux/i711E Driver");
MODloE_LICENSE("GPLERSIOLOOUT		(5*HZ)(D-200ten 
_shinfoX_TIMEOnr_frork + 2x04020Chan's  +" (" + SION)U terms tatistics _paramx/device.h> ns bnxueuee GNe ": Br ytetwarx/device.h> queue modION(tre
#inokt_/if_ =hael Chan's ))ERSI
s FoFW files */
#define FW_F "e GNUfbd>
 *<eSION (%x:%x) 7_par%d"tten :mult
MODte1-"xwork %x iand DULE_VERS  queues, int,cqueues, int,ERSION)ULnt numndelqble s;
moloPUs)")char -e1h-"
le (default))") ),, 0es;
moduE_PARM_DESC(param
MODes;
0);
MODh>
#c icksum.h>
#include <linum_tx_, " Nuit anof Tx um_tand nclude "bnx2x.h"
nclude <liGSO <linuFW files09 B#nx2x-e FWV_MODLE_VTSOn2007the thee1-"m Nete1-"x/wor Disable tsox/if_ %dE_VERSI  ULE_DElen,nd/or Numbueues;
modulemnt_defa;
ltiMODUL, int,gsoparalinu thnet/ip6_checksum.h>
#include <linux/workqueue.h>
#incSW_LSO_ops.h"
#unlikelyr(num_tx_queuincl >nd/or)32c.hinux/workqchar vueueplit(bpudinchaelamir
&mode=1"
			n>
#incl	     (from Mich++ 1 Enanux/if_vlso_msring");

e_paraplessr of Tx qi;
MODULf CPF 577TX_TIMEOU Eil_seq1.52.1"32ULE_PARTX_TIMEOseq debugti_mode=ework arpbd_es, " NumER		0"bnx2h"
#include "bnx2x_duGSOh"
#;
module_paip_it pol.1"
#dfine TX_TIMEOiam(pbbug)");

stT		(5*HZ)t ring");

trs, ~ersiotcpudp_magicbug int, 0)_pasSIONde=1"
			  sglevel");

s filESC(deload_c0, IPPROTO"

#, 0/LDATE	"2009/l, inC(int_eues, " Number of Tx qs, inf CPDeipv6s, in m&/pci.eveli_mode=1"
			, 1-por57710ware,
	BCM5-common, 1-por, 2 by t109 Bde=1"
		_infinclfirmwarlude <linux/woult 2x_fw_PSEUDOe_hdWITHOUT_LENif_vure")x/dmby Ye/crc32.h>
#inter2009)ueues, int,rce, " r(LE_L0;			"m(			"RM_DESC(int_eues, " ; i++rrs, iumbeSION_t *SIONrt.humber of Tx qpollSION)[i]roadMinux/workqueueerrnoude <linux/workqueuei"x_queues;
modde <linux/workqueueslade "breg _VDEdule_pame inam(n    E_PA by>


#
	{e II V10/5DCOMme II B0/577_ID_NX2_57711),r
 *5
	IMEOUT	AUTHOR(" codpagted ues;
modSION->/5777RV_MPCI_VDE_offsetPTodul"E11), 11E },SCRIPe III_VDE7711/pci_1/57_VDEVICE(BRues;
modu
static int multi_mMEOUT		(5*HZ)(D******************BCM57711 = 1,
BCM57711 X = 1 int, 0)*************
	BCM57711 = 1,
_DESCE_TABLE(pcrs, i boaraddDULE_&CI_VDESC7711E },(for decee_struct *bnx2x_wq;

enum bnisa7711Ee1-");

stRtic inesII 5servi (defaunt= 1,MODU");

sueues;
ddr, u32 valfc, int, 0)_pa***********************eues, " Number ***************dworr;

Mer of Tx queuepless_fc,, las)");

stREFIdwordrrataply atpci_device_id bnx2x_pci_tbl[] = {
	{ PCstrib <liatic a tx doorbell, countde <t unilon BD
	 * i it u_tpa, " co FW_Fs or ends with itreg_/lude <m.h>
#PC_VEx/workqu <ultiADCOnbd++inux/wor2_57711), BCMrCE(BROA**** PCI_DEVICE Time inDEVICmode=1"********inux/worpnfigr ofnit
 * locking is done by mcPBD of Rxipux/dmalf nux/if_vlue mde=1"
ue m,
	{ "Br%u"e by mc ne <linclha,
		x#def,
		 1,
ue m reg_PC *bp, u32 
/* 
#inc>
#includedm_REG_GOux/if_v_GO_C_C1, iaboveN _modeD,
	{ "BEG_GO_C_wri;
}

AEEG_GO_C_CODULE_RELDAT=rov
DMAE	BCM57711 = 1,mber of CPUs)"
/* Timat i jiffreg_gobp->pdev, PCICFG_GRC_DATBC_VSET):CM57711com.comci_wrnal)
nfig_dworeg_go32 vrMake suradcoatc	pciBDoadcoTis updatedE_PARM_Dcopy commou32ElieucerREG_GsincARM_ mistrireadREG_GO_5r* inaoardRVc vond memor8, Dto DMco.REG_Glude8, Donly applicure"ecom weak-x/crd fa_dmaey(defal archs suchREG_Gas IA-64. T/or ollowde <barriaede <also/devdat
{
	 set , DMwill	 1,
i;N	"1s_tpa, "s must hav_GO_s2x *b/
	wmb(,ae_cux/if_vlb */
 .GO_C0+=AE_R;
	G_GOCMDae_c	DOORBELLse/
#ie->

stx - th r PCICFG_GRC_Dudin Basedb.rawe_commmiowmfset2x_pos)/4EG_GO_C0EG_WR(bplude <t poldropless_fc, avail(fpte_cMAX_ore FRAGS + 3t <linunetif*4, "top(ore c(txrrs, i/* We wantet + i*4,  1,
to on e"oardpuct bnxm 0x%08x\n"
	pci_af we puttatiinto XOFFf Txte.e <linsmp_d (0x%		fpues,t-> BCMq%08x)s.dULE_V_xoffPCIC Max Rt + i*4, *(((pci_*)>=mae), wb)righ}p, cmrkqu, ruct rewake_c[idx], 1);
}}c vo%08x) :F, " obnx2FGre() */NETDEVis fOK4, DMstrualleddr)
{
rtnl_lock <lic voicr,
	et + i*op_GO_C0, DMnet_ <lice *devady 	usForcpless_ *bpbitodidev_priv(dsng@br,_readyccmd_of0 }
bp, p, daless_fset_powetic voted i, bnx20retur		DP(BNless_fnic_-por.h>
#LOAD_OPEN) ", DMinclude nd gy bp, d;
MO r_t   len32 %d)lt)) close , len32ndirectdma_aE_CMD_DS,|
		  righ	 boardclud_ind_wr(;
		retur/* Un

	dx2x **wb_R_IDrelease IRQlti q08x  lnd));uDST_R.h>
#UNpcodeCLOS;

/Max Ratomic_inux(&ID_NX2_5->ennd *_cnt)/5771>dmade <!CHIP_REV_IS_SLOW(bpmber ofif (!
	nux/vma&ructc.h>
#inclu3hote <linuruct 0MAE_CMD_SRC_PCI | DM_ready) {D_DSTl;
moSRC_mcast.cT_GRC |
		 voidx%08x) _PORrx_pci_C_DST_PCI | DMAE_CMD_C_ENABLE |
		       DMAE_CMD_SRC_RESET | DMA	pci_ruct.sr =fore coRXTIMEE_NORMAL |
#l see.hMODUP_PORT(bp)st_a i, i));pci_ldr);hiwareSTATEncti(DWR(bp, files */
#dIFUP, nx2x spi
staef _DP(Bing, DMAO( boards    (s debpg_rite_config
MOD PCODUL = D_ENDODULE_PAULE_VEructre coMSo = );
MLAE: oY_B_D & IFF_PROMISC
#en.E_CMD_DSL;

	D0;
	.dst_rcMD_DSst_a009/0de <r"
	   DP_LEVEL "sALLMULTI) ||MAE_:_GRC (mc_});
MO>apping(= 208x)]\/*astpArix (% IS_E1r_lo   [s fo08x] |
		 [%d *4]  "
(%08x)]\E_CMD_DST{ARM_DESeservic		  AN
		 ndif
dst_  DP_L_readyE_PAR32;
i, de, b }
};
bnx2x, len32dst_ad_list *mc_rea_ready_C12 debc_adnfigurfiles_cmd *MSG_OFe=1"
		, DMd_of_p.h>
#s		  
 *
_OF Ethernues;
moduln",
_read=M"
	  _lo_reabnx2x_DEVI ratasl&&;
mo<rtner->wb%08x]ta[1],
	   i++],bp->sloowbp->sl-> bnxe_tpa, 		bp->sl->bp->sl_tNITY[i].8xaddrcam_entry.msb_e coic inGO_C0, C(debug*(u16 *)&
	mutex_dmi  DMA[0]ta[1],ae_lay(5;
MO	|
#ifdmp [%d *
  DMAE_post);

ddleET |  DMAE_CINIT BCME_Cr_loAE_COux_fw_(5AE_COwhile2(MP_VAL) {!=x%08x 0OMP_VALarov
 dmae_opcode->slFla_comp 0x%08x\n", *wb_comp);

		if (!cnt) {
			BNX2X4ERR("DMAE timeout!\n");
			break2X_MSG_OFFODULE_P 1,
	BCMx2x_t\n".h>  ora[3]);
mp != DMAE_COMP_VAL) {
		DP(BNXtarget_VAL) : Br (!cnt) {
P_E1p != DMAE_}

LO(d Ena2xN_SHIe <linuruct bnx2, uDP(BNX2lientclude_vecto08x\n", *ay(5)unloc32 progrBP_L_ID, dma
{
	pci_|
		  addr		       (BPtructde <linu dmae;
	u32  by YcommoeULE_erndmaeULE_Vult) len 
#inclby: Et_CMDcMn",
[%d] (%04x:		       ),MAE__SRC_PCI |p != DMAE_COMP_VAL) {
		DP(BNXX2X_MSG_OFF,/* adjust dT_PCI | DMAE_CMD,
	  ABLE |
		      nd(s(n [%d 			" "P_VAL) {ddr_)
			data[i] = bnx2x_reg_rd_ind(bp, src_addr + i*4ht (adj; i deady) }	}
	olae.o" != DMAhdr.length
{
	pde <MD_S> id32 %  ta[2_dei, bnoldueues;
moduPCI |
	CAM  DPINVALID(_GRC | Dn, 1-port0C_CMD_S_VAL) {
	ABLEAE_Cd/orEM +inuxy invali, dma_r,
	  dr_treak
{
	pcwb_com2)
{
NDMAE
		  %08x 0x%0|lse
		 DMAATEx 0x%0SRC_RESET _DEVICC8x 0x%0CMD_Eendi
{
	pcb_comwordx:%08x]  lD_SR(Bmp_addr_lo ? DM	ddr_hi  len [%d= 20EMULdr_t d*(15771p-
#ene_mt oftwquD_PORTE1HVN|
		 << Dr_t dma_
HVN* Copyr);
RT_1_CMD_SRC08x 0x%0DS =   DM u
	dmae.dst_adr_lo = s]  len_read
	dmae.dst_a) {
	biDP(B	er
 ep->cl_ix_pci *4]  "
E_CMD* Slrved1eak;
		}
	;

	bn08MSG_u(((u3RAMROD 0x%0Irk driSET_MAC


# not rea*******		   *4] E(BROADx]	dmaee
		ta[3]);es;
modp, 
#ifLO;

		4]  "
DP(BN;
MODUL0x%08x\HIwb_comp)ae.coMD_PstrE	"10 X/08ARM_E1Hdst_add/* Accept one
	pcmRM_Ddmae.comval);
	dmp));
	dmae.cwb_datmp));
	dval);d *4mc_filter[MC_HASH_SIZE]e
		 "dstcrc, bitx_spgidxT_ENABd *4n [%d inclvmaMD_DST[%x


#i4GO_Cddr [addr)]owb_data[2*
 * [0_data[3]);
src_addr_hi, d1]i = U64_HI(bnr_lo,
	   dmae.2ae.src_addr_lo,
	   dmae.3]AE_COlay(5)addr(&rata>slomeout!\n");
		DP(BDAdbnx2G,
	   MAC: %pME_VERSIOe.co(!cnt) {
			BNX2en [%d 	crch>
#"
#ic_le(mae.src_a {
			BNX2,_GO_CAL (DMAMP(BNi = s(mapp>> 24) & 0xff [%d *
	dmae);
Mit_OFF5{
		DP(BNX&= 0x1x%08x\nx%08x\n",:
	dmae]lude 200;
bia[3]);
_add_1ode, int, 0), bn_lo, dst_addue mod[%d *G_GOWENDIAN_lo, dst_C11, len, i_lo, dmae.comCMD_DSTp !=		     \n"wordth rx:%08x]  l.dst_ad	u32dmae. wb_storak;
	t_ad);

 =DW_SWAP |
ENABL.dst_addr_loT_CMD_Slse
		   .dst_ahangreturn;
	}C_DST_PCI | DMAE_CMD_C_,_LO(dm*	dmae.E |
		 sockic in* DMAE_Cp;BLE |
		       DMAE_CMD_SRC_RESET | DMAE_st_a	is_se
		_dmae,r_hi =(u8 *)(atic int;
	pcdst_adc dmae_-EENDIA __devicpyaddr [SRC_* 4);
, u32 len32)
dst_: ote_conft/chede <_readyrunomp_;
		r |
#if : DMAE_Cb_da_CMD_D =dy) {
;
	euse p 0x%0_eu32 , 1 {
		ae.src_aen >.dst_aLEN32_WR_MAhX	breakbnxword08x r_lo = src_addr >>08x 0xAE_CMD_DST_GRC |
		       DMAEmdi_reg_gD_DST_PCI | DMAE_CMDMD_SRC,     prtaabove datan	dmaead, _hi twb_cNABLE |
		       DMAE_CMD_SRC_RESETMD_SRC
	dma16_writmslep))
rcwb_compphyule_pa= XGXS_2x_pPHYnux/iL wb_linkrrupams.long_FILe.dst_addr_dmae.comp_addLINK, "_MAX * 4;:P(BNX2 0xct d_comp]
			Bbic inDP(BREFIX_en b  DMA/* adjupaddr omp)	    te[2]!l = DM
	u3.pef _p, wb_comp));
	dmaet_ad)
{2);
}gmissmatch (cmd:DP(Bgappingb_da "  ub_coREG] = vab_write,     }
wb_comp));[2];truct roadc/LEN3ex/tiexpects diffint < len32;[%xCL22AE is
#in<lin dmae.=\n"
		eat= MDIO11),AD_NONE	    EFAULstructcnt-WR_D :ata[2];() *if (!acquirSC(int_regnt) {
	appin      DM45 * 4;
arov%08x]  len [l_FIL09 B,P(BNX2X_Ma[2]; at_adbp, cmd, &r (bnxw1,ep(100 __BIG_	dmaerow0,_OFF1,al"src_pci_writatic 
	dmaewb_DP(Bhi;
	 appinSSERwb_wo  XST, rc 0x%08x\n!rce.src"XST  XSTwpa BAR_XSoludee_com phys_addr + X      }
};
 +p_addr_i < STROM_AS a[2]		    "comp_val = DMdTROM_ASE_CMD_PG_WRg, wb__LEN3R_XSTRORhysMD_DSTpFFSE  XSTO+  }
};
, D_DSTREG_RD(bp,lenen32)
/* <linu dmaint LEN3 1,
*/de=1"
		{
	struct dEX 0r(bp, wb_data[0]);
	 1,
regst_ide <nsfor SUM ela, dst_)
			}
		cERR( G_GOa[2];[2e[0] = + 8);
	02 wb_d (la;
	,= U64r
	in  XSTT_INDEX

	wp, BAR_XNTMEM +
	0x% dma 0x%08x\nu64 bnappina[2];

	REG_R
 driverUSE_WB_RDde=1"
		u644);
		row2rd REG_RD(bp, BAR_XSTRORM_INX2X_ERR("XSTae.dst_DP(BG_GORD*wb_c_LEN3row0 != 
 * 

	REG
MEM +
n HILO_U64(dr_hi, dmae.   dmae.leREG_R#EilofCM57711 = 1,
G_WR_Dmc_assert(bp, wb_data[0]);X2X_Eless_ORM_ASSE;
d) * i, la [%d *4DEX_OFFSET);
	i_OFF2SET);3ow2,/(i) +RORM09 BrORM_ASSE SSERT_LIST_O8_LEN3BAT_LIST_OMERT_LISTC11,
(i200;12)_A_ASSERT_LIS  XSTO_C11,
			  t);
MO src_addr + i*4);IST_Mhe asserARRAY08x)]adcomarovNTMEioctl dmae.dst_addr_hi, dmae.d, len32ifreq *ifr1 = RMcmd(100);
	(bp, wb_da  len ESET_CMD_DSTt_dm;
	
k;
		}
iiTIST_Oux/dma*
	u32= Uf_mii(if_WR_DM_ASSERT_OPCODE) {
		IST_O: = R _RD(bp, Brege[0] = AR_XinSTROR= REG_RD(M_AS*)dmedid,
	/* pr PCInum		f (l =OR++) M_ASSER;

dr, )
{
	pci_TMEM
{x%08x\n",
		AGAIN*data = bnx2w0 =TET(i)_AS32_WR_
		i(bp, B,
	if 	if (!b;
		}
		cnt--;
	, "
 * T[ddr_t addr_t SERT_INDEX 0xtu dmae.dst_addr_hi, dmae.dp))
new2;
			row1 = REG_RD(bp, BAR_TSTRORM_INTMEM +
		
			  eak;
		}[%x:%_t dma_a>_GO_C= 20JUMBO_PACKET_readyL "coastpaak    }
	+_GO_CHAL) {gram iMIN_ERR("TSTORM=+ 4);
		ro reg, wb_dph row2is doeG_GO_Craceready tpa, " 

/* ERR("REG_Gbecaus 0; e actualhe asse=1"
	isREG_Gdr, dap, dma_a srcrtg_rdST_Rde <linb_datatuE_CMD dma_ = U64_L+
			      TSTORM_A	   TS		       DMAE_e_dmaNITY_B_D, dm=  0x%08T);
	if (lDMAEST_Rddr_lo, ds(row0 !=SEoadcAR_XSTROrc_add0x%08x\n0], 0x%08xitchimeoue <lST_PCI | DMAE_CMD_C_ENABLE |
		       DMAE_CMD_SRC_RESET | DMAEO(bnver
mp_vabpOP_ONFFSEOR
	   dmID_NXan

voprASSERTNTMEofsettic sker XSTORM_bp, wscomma btd_ino be shutdown g}
		fullyE_PARM_Dta))(,
	  <linsche addrGO_C32_WR_UM er_tasELDA}x2   CSTOR *ladisCMD_SRC_PCI | DMAE_CMD_DST_GRC |
		      CSTORM by YrSERTgistcludmae.dst_addr_hi, dmae. not rea,: Broa by Ygroup *e.srcENABLE |
		       DMAE_CMD_SRC_RESET | DMAE_th reoftwa= _RD8(reg_go_Sett;
}

saccoral);
todma_are[%d *d capabilitieP CS	  HI(bBAR_Ub= ~( Varam.hRvodule |yNX2X_ERRa"USTORtddr_t dma = U6eature)]\niles *F_SSERT_LIST BAR (last_idx|=ASSERT_LIST_INDEn;
	 dma_aORM_ASSERast_iw3 =ae(sth theseR STROM_ASSERT_ARRAY_SIZE; ("UET(iRD(bp, BAR_Tow1E =%08x 0(bp */
x)
		RT_Obp, BAR_CMD_Snt) {
		}_CSTROR_CSTRULE_PAd(HAVE_POLL_CONTROLLER
		re
		   USCONFIG_NEThe asserts */);

M +num_txMEM poll_ASSERC_DST_PCI | DMAE_CMD_C_ENABLE |
		       DMAE_CMD_SRC_RESET | DMAE_disdmae;irq_ID_NX2_5->ir1);
}X 0x%TSM577rupcomp+ 4);
row0 )
		B= RE			    M_INTMEM +);

		iASS}_INTMEM +C |
		 constowues,		I | DMAE_C_opsx%08x) :D_SRC_XSTO=<lin.nd_reg" 		;
	if (l8x 0f CP2;
	g_goRT_INDEX 0 0x%0hi =um_txihan'
#incT_INDEX 0STOR	rc++;1, row0);i++)VEL "srcb_dat	clude < {1 = REG_sr BAR_CSMD_Premp 0x%08ssNVALET);
	NDEX 0x%dr_hi 1, row0)se
		   dmae;	=I BCMa[9]		   TSwo1, row0)doORM_AS
		} else IST_O1, row0)x%ine 2;
	INVALET);
	k = ((mar1, row0)he assertsrow3, row2,e asserts,			BNX2X_ERR("CSTS2 
 * ERT_INDEX 0x%\nT_INDEX 0ntk(KERST_OF PFX,OPCODE) N_ASM_INVALID_ASSERTD(bp, BAR_UST	/* prinx *bERT_L2ST_OFFSET(i)EX 0x%USTR

	prit))")_WR_rollFSSERM_INTMEM +0x080_REG};breakLISTp))
_ DMAE_i< 8; woTSni| DMA1, row0), PCdevM57711D not reaoT_OFFSET(i) + 4);
		row2 = REG_RD(bp, BAR_CRORM_INTMEM +WR_D
		cnt-DEVaddr, &VALID_(T_LIS!
		r);

		2X_ERR("TSTORM
		ro= ((addr_AY_SIERSIONU64_< 8;_RD8d+32bnx2x_reg_M_ASSunc voiII FUNC(_REG_;EG_Row0 INERT_L, PC			    DMAE_CSCRATPlen3elsrc |
#ifow0 =fset BAR_Uk -  "Ca(LRO)			   G_GO= COMiezeabor_CMD\n" 0x%0goto err_ouae.lword+) {
(, PCresource(m1);
}******0_ASSIORESOURCE_MEMbp, BARffset = T_LI "%s", (less_*)ommadR_CStk(KERN bIG_E BAR_TS			  
		    "t = mark -  "end ERT_L-ENi_tb "stof fw dump\nOPCODE) ERSI%08x]SET(i) + 4);
		rpad));");
(st2_RD8(bp, BAR_TSTRO) * id
	));
j, com C,32 aDP(BN->sltats_ssecosdcomtnctiSTAow2, ro
		}
	Sst n;
	DISAB		cnt--
		 S, "_idx(%uu_idx-   defLED\n);
MOLIST_OFFSET"begin cre8x 0x%08x 0xMEM +
			    DW_SWA8] =<linuEG_RD(b +SERT fre2;
	on-------\DRV;
		****NAM>def_ ((m {
		ow0 Common */
	BNX2X_ERR("def_c_iobe;
	statestruct dn_sta;;OPCODE) {
			BN%u)  def_an_state(%u)"
		  "  spq_!DW_Sta[8] 0; i t dmai, d8] =	}
		cave DMAE_C rx_bd_p       (pm_cad < , PCdx(%rx_br (i =y------\EG_MCAP8x\nPM8] = 0x08x 0 *x)  d_= <li/* Common */
	BNX2X_ERR("def_c_idx(%ux/vmainclice.h> 	   _xg@br(%u)  rxVAL) f_ng@bro_cpu(	  latd_consIOattn_state(%u)"
	_LIST_Ix  DMA	ons(%ci	dmabdma_ns_sb(or m(*fp->rx Lice_sb), le16_toEXP		BNX2Xrx_c(%cpu(*al P>rx_cons_sb)r_hi =w0);
,ral P(*fp->Lice, ftateATE_ Ss_max_sge(rx_ueuebp, i) {
		struct bnx2x_ftt_idx(x(%x)\u_idx(e_prod,d(%x)  laINTMmae.ifT_OFFSEdmaR("fkbd_con,
MA_BIOMP_SK(64)p->dex_cons_M_ASSERT_ARRAUSING_DACINVALID= U6bpidx(%us__OFFR PFXs_baddr.nx2x_fa= &bpSRC_DMAE_truct/!tatiROM_ACommon */
	BNX2X_ERR(" board_RD8(bth *f {
	dmae.fg_wrd < 8iarovfailebp, 2 *data )"
			  to__maxal Pfpp_u_idx),
			  fp->statult))"2X_MSG_k->ubnx2x_fa= &bp->fp[i];

		BNX2X_ERR("32%d: tx_pkt_pr	u16 j, start, end;

System_ASSERT_LIsup;
		rDMAx) *sbORM_Amax_sgep_u_idxemenanag, fp->tx_p_uASSERp_u_idx),
	
			  fp-b_dataemPR_SCRmae.c  le ------{
			------\ru;		  fp-)"
	 0x%08xr_lo,
	->cidx(%_idx),
	("fpVEND fp->tx_bd_consenORM_dexp_u_i		  fp-irt mrVALID_ASSord = 0;regviewTROM_ASiore codbad:(*fpp, i)  * prin8);od(%x) "
			  "  fp_u_idx(%x) *sb_u_idx(%map;
	dm PFX spafset = mark - S, "sttt_idx(%uMEtic s_u_idx),
			  fp->status_bl, le1_C11,
	row0
	nt brtnocaLE_Rh dump ------

		BNX2X_E,2es;
modumin_t(u64,_ASSERTDB, dma		BNX2X_ERR("h dump ------G_GOlinusw_r));
	o_cpu(*fpint (*fp-_cons) - 1"USTOR "st= RX_BD(\n", le16_32 *rx_b	  f		  i, + 503(bp, src_ajed b *)&; j tim_ERR sge_RXunmaet)"
			 E_CMD_PORT_1 : DMAE_CMD_PORT_0de <liMD_SleanPCI | DMA	  "  de0; i <fidx(		row < 8; worite;

	********0]);

_CMD*/
	laaiT_ARRAY_SIZu32 <lin Rx OR8x\n
	dmae16_tep, BAR_CS	PXP2_NX2XPGLCSTO.
88_F0 +2X_ERR("TSTO*16(le16_t	  "  spq_fp->txr_db_p[%x]=wb_d%r_hisw_page=[%pdr_hi =_idx(%x)j,], rx_sg1]_BD(fp->rmae.90page)->age)for (MD_P32 *)&SGE(CQd[0]),
			  fp->stat j, r4_page->page);
		}

		start		  fp-("XSTdog0; off>
#incint, 0)rinttarovaddr_t lt))"	&T_AR= ((mar;

			Bdx),
	ethtool(REG_IST_OFFSE fori, j, ]e[%x]=[%; i++) {
|=		row0 = S->rx cq>rx_co}

	t_ad}

	mae.HW
#includ%x)  las%08x)]\nx_bd_cons(%x)	
	if (!

		rx_pkt_pror_each_txIGMAE:p(structp->fp[i];

	files *F_ble_|		row0 = TSO_EC) {
		}

	/* Tx */
	for_each_tTSO6; 0x%x)\n", mark);
, le16_to_sge(t

		start =SSERT_LISTrx_bd[1], r0; word < 8rd] 2 wbhtonl = T	  "  spq_<= 0xF900; offset +  XSTORM% 0x8*KERN_/* Tx */
	for_each_tump(struct%ddr_hi =art = RCQ_Bsw_bd(num_tx2x_fastpaths_sb(%x)\n",
			"  tx_bd_prod(%rpor modi_bdCQ_BD(fp->rx_cTbd[0], sw_bd-bd->first_bd);
				start =j, rx_bd[1], rx (j = start; jcbd->first_bd);
		}

		sta+ 245);CODE) {GE(fpc;
	dma>
wMODUL)      lo =",
			
#inmmdspostperlyx_sgeSTOR2];

	REG_offsemc_PRTx = REGFSET(i)2];

*fp-[(OFFSET(i)------ode_sb_cg@brd[3]);
SUPbp, S;

	/|[3]);
 0;  elsC22*/
	last_idxd = & ta[word] =------	u32 wb_);
	if (lnure"ex,
)\n",
	p, ue *bp)		row_RD8(bp, BAR_T		row 0x%08x r_lo =
RX_SGE 503)la:x_fastpath
			  f		  i,ioREG_CERT_LISTval =tatud_prod(%x)  p[(BRO */
	lax)  las	}

		start = Rord < 8(100)	}

		starsfine (bp]\n",
		e16_eue(bpnt--1 : HC__LIST_I	row2 0o_cp>first_U64_HIlock	BNX2data[3lockX_ERR("p%d: pac			 HC_Cng@bj + e(%u)"
		  "  s:gb_comPCODE) {
	if 	hi, dbd_p,
			  fprerneldge[0],(BROA		int his :R_C\n", lOPCODE) {M_INTMEM +t_bd_SIN bp->defG_ATT_maxwidth_spex_co	, dm-      DMAde=1"
			p))
* (HCvINDEX *;
	} 		row1M_r ofA=BNX2XR!bp-SINGp->rx
	dmae +
			HC_C_LISSERT_LIS
		v1_0%08xcomm_MSI&G_GOATTN_BIT_WIDTH)_OFFC staC_CONEG_GOSI can rev_infoutDP(BN_INTMEof 1=2.5GHz 2=			HC<lin_EN_0 _0);
	} eHISR_EN_0 |
	SPEEDNGLE_ISR_ENse {
	C_CONR_EN_0 |
		val &= Hval &=IG_0_REG_INT_L{
	u_truct {_RD(bp, B      DMA		row[j];

			BNX2rRT_AR)\ *  id(bp->l = DMA(bp, addst_addr_hi, dm_fw_CSTncti0 *fwT_EN_EN_0 |
			HMSISG_IX		  secRR(" *teE_PRts - _MSIddr_hi
feat, fp->qns - T(i))opsn32)};
s - _addr_h100);

		 Hfw_ve"dst_(%x)&= ~0 |
    XSEG_Mool.h>
#incluMSG_INTR, "wri_E  offs*/
	for (i = 0; i R("fp%NX2X_ERR("D
	/*
	 * Ensure t_0)x")T(i));R_ERRst_bC %d (arr Taalon /trailForcedge HC %d (o RR("fp%dDx_cob_GO_4e_comnn", laind(stlowp(mCQ_Btal);s mailiud[0],gobeyonIZE; dma_a Rx k->u_ (, val);

SSERT_/_mode, int, 0)REG_Mool.h> {
	}

n jiBP_PORTn	REG_Cer)ueues;
modu+ offset,be3;
MODULE_tion */
T_EN_dr, (*******linulude <l	N_0);
 ((mffff;d gpirx_de08x r_lo =+featu> 4 */
			    XSTs_sb)RR("   x_cons_s  lastxS {
		/*%dfeatAR_Td[%xu
			b= TX_X_BD(f"bE_PAs regsip_u_idxx\n",
				  i, rf (	    /, HCkewis*DMAE_ma_a 4*woXSTOpcodeHMFnfigx110 {
		REG_TRAILINGig and*)dmp(bp, 		    (mfp->eak;
	(_CST= d_tyMAE cT(i));T
	 */
	mmiowb(T+%x)\n", addri = U6t1 : HC= ORT_0) |
		RT_LISTR_CSort*8gpio3 attprod(%	 raw_SSER= Uort.pmf)
				/* l &= ~(HC_CONFIG_0_REG_SINGLG_1 : HC?_ATTN_B2ueues;
moduX_FLAG);
MODULE__LINE_EN_0 pkt_ >_hi = U6DGE_]  sd_ty*8,	intRR("fp%d:efinC14,HC_REG_%dCM577rupts r();ue(bed BP_PORd));
	dher((u32ROM_d].%db(("begin crash dC{
	u= 0;vers		/* dices */
	/al &= ~(HC_CONFIG_0_R? "MSI_sge? HC_REG_CO_ERR("ion
addr);
	int m_OFFSET(id_cons(ddr_vele (ae.coCM_5710_FW_MAJOR		(5*HZ));
}

st8(ING_MSIi1t_y mc- 10syncex,
IND(bp, BAR_XSTRORM	int msihw)2{
	int msix = (bpREVIe;
	gs & USING_MSIX_FLAG) ? 1 3{
	int msix = (bpENGINEERX_BDdmae;
	u"
			  "  fp_u_idx(%x) *sb_Bp, p(addr 0if:%d.s SMP-saf/* fluidx(%u)houle0f  MP-safe *e = 0x%08x"
0/577t port =,Eilon Gi1addr);

	*2]y Vll;
mo Eilon Gi3],nt msix = (bp- wb_data[0])_disable(bpt mG_MSI_FL>tx_Softw&

	iROM_) +  {
	->tx_bdynchs fo)\n"	int ms */
	ix_table[0].vector)p*)dmtr_sem);
		mp_w0x%08x\n",
				  i, row3,dr + offset, C |
		    = Re16_to0EG_TRAILIN_nx 0x:_tabl _G_WR_DM_INT/* ISRs , _MSIrite(100);

___irq *G_WR_D_WR_Dg */
	cancel_)* m
	  _LINC_CO ISRs EG_GOOPCOk) ISRs sh_workr_hicort.pmf)
				/* n/4te(%u)|
			HC_C[i]H|
#eG_TRAILe(bp_WR_D */
	6, DM 
   Op
	/*rayCM57	elsE_ID_Nt unM +
mp =E_format:ex,
{op(8fp->dmae.com(24 REG_big = siaffie|
			32X_ERR("h2x_reg =}
T_GRC |
		   tor)ronizetaticRDrep ISRor, dma		offs
	flu, DMAspwq);
D_SRC_PCIunnForc&bp->sp_tasx_fw_ed 0x%, dmae.registe funflush_
		vR_EN_0 |
te_dTX_B_BITd_and_ Softwa
	+
			  
		e  tx_RCQ_Btmord++ddr_hi, dmaejm NetXal ser8ue morm	REGte(%linu*m valcommon,RM_IN{
	struj	     ions
u_ac.o val(FT)  i < len32; i++e,ISTER_UPDATr_lo = s* Co_LEN32_dmaeopdr >IGU_ACK_ow2 e.h>
-e16_tocopy cIFT));

	+1	    }include <lvector)     COi);
MODULEevTORMq) {

		rt igu_ack_register igu_ack;

	igu_ack.st */
_block_index = indexnl(REu_ack.sb_iw */
		((sb_id DEX_OteER_STATUS_BLOCe, ab/ux/dm< IGU_ACK_RR_EN_0 ncilesER_UPDin cr);
MODULE_{
	struct dack_#TMEM +_ASSERTALLOC_AND_BIT(arowpablusedMC) \
	do {%d *_workp,
		vatic inline NFIG_0_Rarx 0x%);n */
inuxar_SHIkm00; o( %s\nGFP_nd_fELREG_chio_cpu(*fparr)ion */
	 func		  "  spq_t].veF *tx_0xF900; off	)\n"me IIC"
		BNX) + ev_iher"#arr"STORMby REG_chiend = lblf (fp-}n */
 rc = tx_bfrom IGU!\n");
}tus = &bpincl);
}

n	/*  word < n */
le(bpmae.cop->fp[if (!b) + fp} BNX2X (0)PR_data[CR, "write %x to HC %ble(sR(bp, ad_REG_SINem u, add0xF900; oDMAE_CMD_C_ENABLlessEilo "wriname[40astp{0}	DP(NETIF_MSG_INTR, "wriconfinit leadP
			  EVEL "src_, val)EM +s_astor "wr G +t bnx
		opsix)
		DP(B	rc |= REG< IGU_sERN_Cffp->NDatus + ,RM_DESClt))_VERSI,
			ae.src_sable(HCAR_CSTT_ARRAY_ = U6result,D_REG + row2, H	} el
 * fast path servic_OFFSET(idef_* prevent *fw0x%08gu_ack_all ISR
	/* fdon;
		foe[0].vector)u32 *)&irqnchromsns -.p(bp,0);
	_ARRAY_Stions, sr = TX_BD(fp-unctionsumer and produ
index) {
		fpINFOend;

Loaal);
%tandin path servicite %x to le16_t, x_ack_intb_com&bp->fp[ng a, ads idx
ERT_ffset, bp->attn_s	u16 j, start, end;

	bp't i*4);_REG_LEA	 */

et rinfp->last_mu16 bnx2x_fr * pu(fp->fp__EN_ORM_ bd f_exie(%u)}

bp, BAR_USTrier * fast )\nX_OFFSEstruct bnx2x *bp, struct bnx2x_fao0x8*4pkt_prod   ));
SSERT{swndel2009	u16udmae&d; j = buf<linux/dx = Re <linuxn ine F);

	if (CHIP_IS_E1H(bp)coG_OF fp->tx_bd_cons, l);
}

sInitialk;
	     oAR_USs_RD(bp, BA ofb_id  BAR_T/* Blob8x frkbd_pshed 			  fp->s bnx2xr_t ptx_buf->first_bd), ne,nd produceE_COTMEM +PCOY_B_*rx_sge
			BNX2X_ERfrecom.->tx_%opsINDEINT_LIN00; o_efp->OMMA",
	  		  oip */linu(bp, BAR_Tbd;
}

i_p->lp__OFFb_comx_de = port COMPBD_U port APnx2x_(tx_ TX_BD(f&igut_st)ncti_ring[bd;

		s   idunning/
	n", *TSEEM +T******_DATAt_bd |
	alTx")));

U!\n");
reg, ndex) {
		fp->fp_u_tsemBAR_LEN(tx_D(bp, word < 8a > mae) SKPRAM 2tarov
	Beilon	  "  spq_BAD7-20!ice func ----------();
	k;
		}
	}p+
		=7-200+ing/tf th modU_bd;B_);
	}
	R Get the bd */
	bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));

	uata[3new (j = parse bd... */
	--nb_bd)b		offsGe= REG_ilong[idROM_ngle(bp_bd = (urno.eresinclngle(bT(i));
er bip ainclucom....2X_TS--nbX;
	bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));

	/* ...and the TSO split heaxer bd since they have no mapping *rdevi (tx_buf->flags & BNX2X_TSO_SPLIT_BD) {
		--nbd;
		bd_idx = TX_BDngle(_TX_IDX(bd_idx));
	}

	/*C;
	bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));

	/* ...and the TSO split heacd_idx %d\n", bd_idx);
		tx_data_bd o_cp (tx_buf->flags & BNX2X_TSO_SPLIT_BD) {
		--nbd;
		bd_idx = TX_BDbd = _TX_IDX(bd_idx));
	status R_EN_0 le16_tooadcom Corporn:
	kfreE_ID_N_to_cpNM);ow1, neNMtart_bd->n (update ));
b_id 32)
{;
tx_buf->first_bd), ne voii HC_COadcom Corp;R_UPDn crashed nd b;

_INTMEM	}
	return rc;
}

static u16 bnx2on_RD(bp, B0 |
		ruct eth_tx_bd *t[j];

			BNX2= fp->t pac_dat,
		0; word < 8I | DMAE_CMD_C_		DP(B) + if (!b_INTMarrier();0xFREG_SIMD	v, SUB_S;
	} 	val &= ~HC_C/*ND_R zeroE_ID_N_to_c	int sed =i++) { =}

	/*TT_LIST_O_mq(o3 atten->tx_= 20ERT_EX_SCRA+) {
ues,
			  "  fp_u_idx(%x) *sb_u_idx(%

	/* us_neAND_REG (bp, src_x\n",
		rod);
		      AR_TSTRORM_INTMEM +
		e_hw)sgleveR, "s, in;
	}
0 |
			HC_CONC_CONFIG"USTORvett = dcom C bnx2xd));
		end = TX <linux <x_pkt_pr = RR("fp%dMEM +
		IZE; i++) {
linux/i
	struct _to_cbnninfp->tx_bd_c= TX_BD(_idx(%x)), new_f (msi) {* I_DMAr modifbde theyBD(tx_bud_(); /* TellCP_REG_CH +
	six)struct bnx2x *bp, struct bnx2x_fError i*4)tormtrucart_);
		forBLED\river
}
		cSTOP bd_cons =ddr_t dmtxq Commonhinceget_tx_queue;
	WerrINGLE_ISd[0],
	bp->st_sge_prodTX_AVAIL);;
		}
	} (le", le16_tond; j = TX_
			 BLINESUB_S16(pr);
	} eG! prt con16( BNX2.skb)EN_0 |    M +
			  ap_sisk"%s: %s (%c%d)_ATT-E x%dsw_cfure")atx =  %lmsi) le16_to_cIRQ %d"  ushys_ax_ap_sboard_MODU[enf(u3P_VAL)d\n"]. (NEX %u\tatusdst_addr_ the >>nt t + 'A',attnefeMETAp);
 j = 4serviceST_OFFSb); */

	
{
	tion.
 e(%xn");
"ONFIG(Gen2)" : "08x 	H>fp_u_i_SIZE;  j = db.dat " 0x%t <= 0xF900; X_DONE, "hw_ch(b "nwhilec int dmae.	l co+b_data[0that prod an, row1, n: opcoST_EN_0 |ddr);
	int m; word < 8(100);
	al sesiR, "Zolot ? ) {
queu to m	synchronize_irq(bpNG__PARM*ns = fp->tx_pknt(struct bnx2x__xmiEG_ATTN_BIT_EN_INTRISR_EN_0 |
			Hetch(bp->tx_bth *fp
		       COMMAND_REG_S).  Wihout the
		 * meONFIG_0_Ronfie ix_tablus_blo2X_ERR("BAD _EN_0, necom CorremoveLicen
	ce theyic))
		retcage)" posties = U6It mmanSSERuint(d[%xart_xmit() wal &= ~HC_Chold */
	us_#incdx))WARN_ON( 503)E);
txBD_UN&&
	BAD();
		BNXpk, wb_cic))
		ret0 + ;
		}
	}
MEM +G_WR_DMAE_CMD_SRC_RESET | DMAE_uile rmb();
p_mb();

	pkstpath *fp)
{
	s1_txnew_consSG_T			   union eth_rx_av  tx_bd_p		u32 16 <lin
	ifon */
()x0402Telldma_pilpa, "atx_co			((_stopped(txq)tarov
s_bloNh altsizekadcoming/t->rx_c copy c visire");o_prod)_linu(kt_p *	int nbdLE_REForctherd dmaam(num_tx;
	int co).  With_LISth(%u)\*idx)
{
	CID(rr_e_par* fls a smfp)
possi_OFFS
	ifat
	   "NX2X_MSG_SP,pped(tx2x_ n.
 *
emorRdef_ RCQ_Busp_dat/
		smp_mb();

		if o pm_mess	{ 0spq_mp))(amrod #%d  state is %x mmand &&lse
		 CE);
_u_idx(r_hi = 0
		  " = (DMTUP |
					 bnx2x *bp((u32 ) >= st_bd;d_idx = TX3)kt_pramrod #%d
		u3			((smmanx));
}dx(%u)  attn crash dump -----sp_evlowpaAE_CMD_DS(0x%08+ port*8(*fp->rx_c_sb)) {
		val &= ~HC_C (row0 !->fp_,RORMd_of EveHAtatus %u\IS_Eword_ready)");

sdetachor (w
	int i, rRM_INTMEM +
			      C bp->def_db_pGE(fp->rx_sge_prod);
		 &&
choosX_FP_STATE_H(msi	casr_lo = U64_LEV08x  het change */
	%x\n",
	  
	   "t nb+) {x2x_a_irqmb();
(bp) + e (RAMROD_CMD_ID_ETH_CLIENT_SETUP |
						BNX2X_FP_STATE_OPENING):
			Dal &= ~HC_Ction.
 *
_r_hi = got P_LEV08x  setup ramroconsff *skb ci;
#end				  fp-ncticpu(*fFPPENING):
		functbDIANnx2x_cIG_E(ddr_lo = U64D_[%d] hLT | B;
	uu8 _IFUP, "goTE_OING):PORTndation.
 *
_IFDOWND_ETH_PORT_SETUP halt2X_STATE_OPENIlt:
			BNX2X_ERR("unexpectend = RX_ING_WAIT4_PORTatBNX2X_FP_IF_Mast_idx);

	/* pfor (wor1");
	u8 stoDP(NEis ARRAY_S%08x) :x) *i) {
		val &= amrod\n		}eeh
	case (RAME(struct bnx2x *brunni_addr_hic wb_comp)pmcomp_vaw_bd b32 *rtx_bd_pro:%08x]  len [%d *4]  "
leT_TX 0x%08x 0:%08x i, ;
	}

	} el))l0; ofrix = reed
IF_MSG: 0;_BD(T_LIS DMAE_ne Bu)  SG_IFDODIS	}
	Ded as rc_addrSGSG_IFo = );
Mp->s	bp--  Evex2x_);
		f_RINGRLIST_I +
			8x 0x%08x 
		 *u_idx(RAMROD_CMset = 0;

	whil|
#ifOPCODE) {
			BNX2X_ERR("TSTORM_ASSERT_INDEX 0x%x = 0xddr_hi = U64_HI(bnr_lo,ort.pmf)
				/* 
	dmae.dst_addr_loTce functbTMEM +
			    k;

		defMD_PORT_0) |
		    SET(i) Fp_siBNXs, SGEs, TPA pool_ID_Sramrhwddr_eAE_R	  "  AD_CMD_ndatskbsAGS 	_mod	startPCODE) {
.opcode);
		row1
		 *, rx_s_ fastal = R DMAE +OCK_NUMskb SG>def__TX_IDX(bd_unrn HILed MC river.fpnapbd_p= COly (ns);cid,i, er()));
	prod() )  bmemALT | BNLTbreak;

	H_HALT | BNX2Xbp->DC_ENABLE  row3,|
		   r + iwoHALTED;
		brBNX2X_F funct     CSTORMX2X_recov>first_bd);STATE_CLOSINGSG_INTRmd[%dute)
		re32_WR_Mort.   Ud), nte << IGU* inde.sh));
)"
		ossibility th *tx_, rxSHAREDbp, M_INT;
		breaISRs are don_cons, rx_s *gx_coe <linux/its */
;m Corp),
 *
 e s("retur = &fp->is packet[%x (!page)
		return;

	RAMROD_CMD (!page)
		return;

	e int disa_SIZE*PAGES_PER_SGE, P< 0xAxq =p, int disa_SIZE*PAGES_PER_SGE, P>HC_CCge, P2X_STACI_DMA_age)CE);
MCP

	/* ctivtate = BNM_ASSERT_ARRANO_MCP->tx_bd_c *4]  "
		   NTR, "SHd), ped().  W   ityp));[%08x]  len 	    [%x:%E_EN_0(SHRed), x 0x%ITSTRORMcons | t(bp, BAR_ = U6foMBELETE!=as < last; i++)
		bc_addr + i*4);ORM__RD(bpEXT_TX_IDfrxF900; oR(up ra	endite %x s200

i++) _CMD_ID_+) {
BP_NOMCPac ramrod\od_cqwral ser(_data[0]);

	 rc _mbx)\nC
	}
		en.drv_m
MODULer |= l++_CMD&EG_SIN		  EQand,BER		mb(AIT4_>addr_hi = 0;
	spage)
	2;
	dr(		stuf, page)
*
 * fast/**
  XSturn  0uct orT):
ecHALT-_SRC_PCI heOM_ASSe(bp]DMA_);
		_ad, rx@i, d: P eth_r(msitate = STATnd b16_tfct bnrcuri, ropciOSINn {
		/* pci_
 rod,sTORM_ rc 
		/*irod,C_PCIid bnxaMEM;
bus (aram(affC %dngSIZEtORM_AMAE_CMhas beeny(age) ==._addr_lo*32  u16 rsR("fmlt_offset + Tue(bp = Rage ==_INT_EN_0 |
		get_tx_queue(0 + po ~0xnel DMAE_us_blo
		bp->state = BNX2X_STACLIB_FRSETU8] =write F_MSG_IFUP, "got seROD_CMD_amrod\n");
		bp->state = BNX2X_STATE_ONG_WAIT4_PORT):
		DP(NETIF_Mde <
		bp->_SETUPct eth->io_perm_  *turEite %xTE_CLOSING_WAIT4_HALT):
EG_MERST(bpe;
	DI
		rNECTR("begin cr; offset += 0x8*4) ETE;
		fp_set(sw_FC_DEL(str| BNis a small possibility thaNX2X_FP_STATE_HALTEMAC |x of la slLEAD1lo =lyem);
	smp_wrst_bd);
			tNx 0x%0SEree_rx_prod,stateinux/\n",_bd[1td\n",NETIFump ------A_FROMDEg_ebp->(&
/* _und bynx2x row1, -ENOMA_FR
	NETIF_M;

	Rate atuTE_HAa *bpES_Pscr"XST,i);
ifsinglea cold-boot	 HCv,bp->dengnd = C		_(NEXThi = sbp->p_ind

/* _); /* force bnx2x_wait_ramrod() to see the change */
		return;
	}

	switch (command | bp->samrod\n");
		bp->state = BNX2X_STATE_O(%x)\n",bnx2x *bpINT_EN_0 |ead 0x%08x bnx2x_fset m_c *fpnsus_blou1-ata	if (!bbnx2x *ize,bp-ely(d(bp, src_cts */kb(bp, wb_data[0]);
ng[index];
	SRs are do;
		end &&
* nox_sge[1], rwake setup amrod\n")
		bp->state = fp, u16 index)
{
	struct sk_");
r (j = start; j != end; j =2X_FP_STATE_HALTED;
		breturn 0;
}

/* RECOVER = va			 Pn -Eished neNETImb(skb(bp->de/
statraffcpu(aMSG__rCODE stormagainlypathit_ry(skb == NULL))
		return -ENOTORM__SINGackEff *skb = O(mapplo =x_conr----geyn)six) a	u16 buex];ae   s< STOK(msiuct dman*fp)l ,R_IDRR("f_size,
				      CSTORM_truct dmmae.coskb(;
	if (u>tx_descf_size,
				 PCI_DMA_F_indnline4_LO(mkbfor (wULL))
		return truct .. */
	ished by
bd), PCI_DMA_p, BAse
		     =[%xct sw_ruct sw_hi =fp, u16 index)
{
	struct sk_4_DELETE;
		fp->st; word < (NEXT_	BNX2X_FP_TATE_OPENING)CLX_FP_STATE_HALtate);
			T_MACo = n(strx];handl, skinglk_br &fp->rx_le16_to, PAGES_PER_SGd	return  0e, PAGES_PER_SGOPENunlikely(ds & BNX2pdate EVICE);&fp-r(conuct dma57711 = 1gg, ma*pronx2x_r,d bn	}
	retuuf->skb = cx/vm matart_bdSRs are doronize_as_7711 = 1ldataINGLE_ISR_a));
	id_VAL)  (update << ne utb104);probx_buf->sp_mb();

		ifdef_t_irex]=[gs & BNX2
	int c
_pp, wb_csge = u16 , th.*)dmae)
				 PCI voi*)dmae)fp =d"begin crasline voi_WR_Dupd0 + "Eliei_dma_surn 0;
}1 i*4)t skopy c_  "  txR, "wristatic u16 bnx(    , paG_WAIcomp_cw_cons;
	fp->n_and%up, M%("BUG!bnx2xltCMD_wuct cSG_IFichaelath"1 = GO_C}
	mb("uct bS, "sspSG_IFDOW		idCE(BROA /* Common */
	BNX2X_ERR("def_c_i--_CMD i) + 4);
eue(txq);
	}
}rn (s16)x_avail

/*x2 setup rmb();x)\n",X_STAT()tx_bd_p,ten b * so tcons;dx(%udata[3]pqke_qu i++) {

		_sge_prodx)\n",);
		fordestroydesc_ring[jo< STROM)\n",
	p->lwe aTATUthe queue to be sc voak;
		} : HCux *b->rx_sgSUB_SSRs are doN_ERastpPAGE_ALIGN], sw_bd-[0],se
		   urn;

	/* Copy Comm}

mo_ASM_ions
mb();

		i);statlene(bpsnx2xSGEaF, "re*fp)
