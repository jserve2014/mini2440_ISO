/*
 * Copyright (c) 2006 - 2009 Intel-NE, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/if_vlan.h>
#include <linux/inet_lro.h>

#include "nes.h"

static unsigned int nes_lro_max_aggr = NES_LRO_MAX_AGGR;
module_param(nes_lro_max_aggr, uint, 0444);
MODULE_PARM_DESC(nes_lro_max_aggr, "NIC LRO max packet aggregation");

static int wide_ppm_offset;
module_param(wide_ppm_offset, int, 0644);
MODULE_PARM_DESC(wide_ppm_offset, "Increase CX4 interface clock ppm offset, 0=100ppm (default), 1=300ppm");

static u32 crit_err_count;
u32 int_mod_timer_init;
u32 int_mod_cq_depth_256;
u32 int_mod_cq_depth_128;
u32 int_mod_cq_depth_32;
u32 int_mod_cq_depth_24;
u32 int_mod_cq_depth_16;
u32 int_mod_cq_depth_4;
u32 int_mod_cq_depth_1;
static const u8 nes_max_critical_error_count = 100;
#include "nes_cm.h"

static void nes_cqp_ce_handler(struct nes_device *nesdev, struct nes_hw_cq *cq);
static void nes_init_csr_ne020(struct nes_device *nesdev, u8 hw_rev, u8 port_count);
static int nes_init_serdes(struct nes_device *nesdev, u8 hw_rev, u8 port_count,
				struct nes_adapter *nesadapter, u8  OneG_Mode);
static void nes_nic_napi_ce_handler(struct nes_device *nesdev, struct nes_hw_nic_cq *cq);
static void nes_process_aeq(struct nes_device *nesdev, struct nes_hw_aeq *aeq);
static void nes_process_ceq(struct nes_device *nesdev, struct nes_hw_ceq *ceq);
static void nes_process_iwarp_aeqe(struct nes_device *nesdev,
				   struct nes_hw_aeqe *aeqe);
static void process_critical_error(struct nes_device *nesdev);
static void nes_process_mac_intr(struct nes_device *nesdev, u32 mac_number);
static unsigned int nes_reset_adapter_ne020(struct nes_device *nesdev, u8 *OneG_Mode);
static void nes_terminate_timeout(unsigned long context);
static void nes_terminate_start_timer(struct nes_qp *nesqp);

#ifdef CONFIG_INFINIBAND_NES_DEBUG
static unsigned char *nes_iwarp_state_str[] = {
	"Non-Existant",
	"Idle",
	"RTS",
	"Closing",
	"RSVD1",
	"Terminate",
	"Error",
	"RSVD2",
};

static unsigned char *nes_tcp_state_str[] = {
	"Non-Existant",
	"Closed",
	"Listen",
	"SYN Sent",
	"SYN Rcvd",
	"Established",
	"Close Wait",
	"FIN Wait 1",
	"Closing",
	"Last Ack",
	"FIN Wait 2",
	"Time Wait",
	"RSVD1",
	"RSVD2",
	"RSVD3",
	"RSVD4",
};
#endif


/**
 * nes_nic_init_timer_defaults
 */
void  nes_nic_init_timer_defaults(struct nes_device *nesdev, u8 jumbomode)
{
	unsigned long flags;
	struct nes_adapter *nesadapter = nesdev->nesadapter;
	struct nes_hw_tune_timer *shared_timer = &nesadapter->tune_timer;

	spin_lock_irqsave(&nesadapter->periodic_timer_lock, flags);

	shared_timer->timer_in_use_min = NES_NIC_FAST_TIMER_LOW;
	shared_timer->timer_in_use_max = NES_NIC_FAST_TIMER_HIGH;
	if (jumbomode) {
		shared_timer->threshold_low    = DEFAULT_JUMBO_NES_QL_LOW;
		shared_timer->threshold_target = DEFAULT_JUMBO_NES_QL_TARGET;
		shared_timer->threshold_high   = DEFAULT_JUMBO_NES_QL_HIGH;
	} else {
		shared_timer->threshold_low    = DEFAULT_NES_QL_LOW;
		shared_timer->threshold_target = DEFAULT_NES_QL_TARGET;
		shared_timer->threshold_high   = DEFAULT_NES_QL_HIGH;
	}

	/* todo use netdev->mtu to set thresholds */
	spin_unlock_irqrestore(&nesadapter->periodic_timer_lock, flags);
}


/**
 * nes_nic_init_timer
 */
static void  nes_nic_init_timer(struct nes_device *nesdev)
{
	unsigned long flags;
	struct nes_adapter *nesadapter = nesdev->nesadapter;
	struct nes_hw_tune_timer *shared_timer = &nesadapter->tune_timer;

	spin_lock_irqsave(&nesadapter->periodic_timer_lock, flags);

	if (shared_timer->timer_in_use_old == 0) {
		nesdev->deepcq_count = 0;
		shared_timer->timer_direction_upward = 0;
		shared_timer->timer_direction_downward = 0;
		shared_timer->timer_in_use = NES_NIC_FAST_TIMER;
		shared_timer->timer_in_use_old = 0;

	}
	if (shared_timer->timer_in_use != shared_timer->timer_in_use_old) {
		shared_timer->timer_in_use_old = shared_timer->timer_in_use;
		nes_write32(nesdev->regs+NES_PERIODIC_CONTROL,
			0x80000000 | ((u32)(shared_timer->timer_in_use*8)));
	}
	/* todo use netdev->mtu to set thresholds */
	spin_unlock_irqrestore(&nesadapter->periodic_timer_lock, flags);
}


/**
 * nes_nic_tune_timer
 */
static void nes_nic_tune_timer(struct nes_device *nesdev)
{
	unsigned long flags;
	struct nes_adapter *nesadapter = nesdev->nesadapter;
	struct nes_hw_tune_timer *shared_timer = &nesadapter->tune_timer;
	u16 cq_count = nesdev->currcq_count;

	spin_lock_irqsave(&nesadapter->periodic_timer_lock, flags);

	if (shared_timer->cq_count_old <= cq_count)
		shared_timer->cq_direction_downward = 0;
	else
		shared_timer->cq_direction_downward++;
	shared_timer->cq_count_old = cq_count;
	if (shared_timer->cq_direction_downward > NES_NIC_CQ_DOWNWARD_TREND) {
		if (cq_count <= shared_timer->threshold_low &&
		    shared_timer->threshold_low > 4) {
			shared_timer->threshold_low = shared_timer->threshold_low/2;
			shared_timer->cq_direction_downward=0;
			nesdev->currcq_count = 0;
			spin_unlock_irqrestore(&nesadapter->periodic_timer_lock, flags);
			return;
		}
	}

	if (cq_count > 1) {
		nesdev->deepcq_count += cq_count;
		if (cq_count <= shared_timer->threshold_low) {       /* increase timer gently */
			shared_timer->timer_direction_upward++;
			shared_timer->timer_direction_downward = 0;
		} else if (cq_count <= shared_timer->threshold_target) { /* balanced */
			shared_timer->timer_direction_upward = 0;
			shared_timer->timer_direction_downward = 0;
		} else if (cq_count <= shared_timer->threshold_high) {  /* decrease timer gently */
			shared_timer->timer_direction_downward++;
			shared_timer->timer_direction_upward = 0;
		} else if (cq_count <= (shared_timer->threshold_high) * 2) {
			shared_timer->timer_in_use -= 2;
			shared_timer->timer_direction_upward = 0;
			shared_timer->timer_direction_downward++;
		} else {
			shared_timer->timer_in_use -= 4;
			shared_timer->timer_direction_upward = 0;
			shared_timer->timer_direction_downward++;
		}

		if (shared_timer->timer_direction_upward > 3 ) {  /* using history */
			shared_timer->timer_in_use += 3;
			shared_timer->timer_direction_upward = 0;
			shared_timer->timer_direction_downward = 0;
		}
		if (shared_timer->timer_direction_downward > 5) { /* using history */
			shared_timer->timer_in_use -= 4 ;
			shared_timer->timer_direction_downward = 0;
			shared_timer->timer_direction_upward = 0;
		}
	}

	/* boundary checking */
	if (shared_timer->timer_in_use > shared_timer->threshold_high)
		shared_timer->timer_in_use = shared_timer->threshold_high;
	else if (shared_timer->timer_in_use < shared_timer->threshold_low)
		shared_timer->timer_in_use = shared_timer->threshold_low;

	nesdev->currcq_count = 0;

	spin_unlock_irqrestore(&nesadapter->periodic_timer_lock, flags);
}


/**
 * nes_init_adapter - initialize adapter
 */
struct nes_adapter *nes_init_adapter(struct nes_device *nesdev, u8 hw_rev) {
	struct nes_adapter *nesadapter = NULL;
	unsigned long num_pds;
	u32 u32temp;
	u32 port_count;
	u16 max_rq_wrs;
	u16 max_sq_wrs;
	u32 max_mr;
	u32 max_256pbl;
	u32 max_4kpbl;
	u32 max_qp;
	u32 max_irrq;
	u32 max_cq;
	u32 hte_index_mask;
	u32 adapter_size;
	u32 arp_table_size;
	u16 vendor_id;
	u16 device_id;
	u8  OneG_Mode;
	u8  func_index;

	/* search the list of existing adapters */
	list_for_each_entry(nesadapter, &nes_adapter_list, list) {
		nes_debug(NES_DBG_INIT, "Searching Adapter list for PCI devfn = 0x%X,"
				" adapter PCI slot/bus = %u/%u, pci devices PCI slot/bus = %u/%u, .\n",
				nesdev->pcidev->devfn,
				PCI_SLOT(nesadapter->devfn),
				nesadapter->bus_number,
				PCI_SLOT(nesdev->pcidev->devfn),
				nesdev->pcidev->bus->number );
		if ((PCI_SLOT(nesadapter->devfn) == PCI_SLOT(nesdev->pcidev->devfn)) &&
				(nesadapter->bus_number == nesdev->pcidev->bus->number)) {
			nesadapter->ref_count++;
			return nesadapter;
		}
	}

	/* no adapter found */
	num_pds = pci_resource_len(nesdev->pcidev, BAR_1) >> PAGE_SHIFT;
	if ((hw_rev != NE020_REV) && (hw_rev != NE020_REV1)) {
		nes_debug(NES_DBG_INIT, "NE020 driver detected unknown hardware revision 0x%x\n",
				hw_rev);
		return NULL;
	}

	nes_debug(NES_DBG_INIT, "Determine Soft Reset, QP_control=0x%x, CPU0=0x%x, CPU1=0x%x, CPU2=0x%x\n",
			nes_read_indexed(nesdev, NES_IDX_QP_CONTROL + PCI_FUNC(nesdev->pcidev->devfn) * 8),
			nes_read_indexed(nesdev, NES_IDX_INT_CPU_STATUS),
			nes_read_indexed(nesdev, NES_IDX_INT_CPU_STATUS + 4),
			nes_read_indexed(nesdev, NES_IDX_INT_CPU_STATUS + 8));

	nes_debug(NES_DBG_INIT, "Reset and init NE020\n");


	if ((port_count = nes_reset_adapter_ne020(nesdev, &OneG_Mode)) == 0)
		return NULL;

	max_qp = nes_read_indexed(nesdev, NES_IDX_QP_CTX_SIZE);
	nes_debug(NES_DBG_INIT, "QP_CTX_SIZE=%u\n", max_qp);

	u32temp = nes_read_indexed(nesdev, NES_IDX_QUAD_HASH_TABLE_SIZE);
	if (max_qp > ((u32)1 << (u32temp & 0x001f))) {
		nes_debug(NES_DBG_INIT, "Reducing Max QPs to %u due to hash table size = 0x%08X\n",
				max_qp, u32temp);
		max_qp = (u32)1 << (u32temp & 0x001f);
	}

	hte_index_mask = ((u32)1 << ((u32temp & 0x001f)+1))-1;
	nes_debug(NES_DBG_INIT, "Max QP = %u, hte_index_mask = 0x%08X.\n",
			max_qp, hte_index_mask);

	u32temp = nes_read_indexed(nesdev, NES_IDX_IRRQ_COUNT);

	max_irrq = 1 << (u32temp & 0x001f);

	if (max_qp > max_irrq) {
		max_qp = max_irrq;
		nes_debug(NES_DBG_INIT, "Reducing Max QPs to %u due to Available Q1s.\n",
				max_qp);
	}

	/* there should be no reason to allocate more pds than qps */
	if (num_pds > max_qp)
		num_pds = max_qp;

	u32temp = nes_read_indexed(nesdev, NES_IDX_MRT_SIZE);
	max_mr = (u32)8192 << (u32temp & 0x7);

	u32temp = nes_read_indexed(nesdev, NES_IDX_PBL_REGION_SIZE);
	max_256pbl = (u32)1 << (u32temp & 0x0000001f);
	max_4kpbl = (u32)1 << ((u32temp >> 16) & 0x0000001f);
	max_cq = nes_read_indexed(nesdev, NES_IDX_CQ_CTX_SIZE);

	u32temp = nes_read_indexed(nesdev, NES_IDX_ARP_CACHE_SIZE);
	arp_table_size = 1 << u32temp;

	adapter_size = (sizeof(struct nes_adapter) +
			(sizeof(unsigned long)-1)) & (~(sizeof(unsigned long)-1));
	adapter_size += sizeof(unsigned long) * BITS_TO_LONGS(max_qp);
	adapter_size += sizeof(unsigned long) * BITS_TO_LONGS(max_mr);
	adapter_size += sizeof(unsigned long) * BITS_TO_LONGS(max_cq);
	adapter_size += sizeof(unsigned long) * BITS_TO_LONGS(num_pds);
	adapter_size += sizeof(unsigned long) * BITS_TO_LONGS(arp_table_size);
	adapter_size += sizeof(struct nes_qp **) * max_qp;

	/* allocate a new adapter struct */
	nesadapter = kzalloc(adapter_size, GFP_KERNEL);
	if (nesadapter == NULL) {
		return NULL;
	}

	nes_debug(NES_DBG_INIT, "Allocating new nesadapter @ %p, size = %u (actual size = %u).\n",
			nesadapter, (u32)sizeof(struct nes_adapter), adapter_size);

	if (nes_read_eeprom_values(nesdev, nesadapter)) {
		printk(KERN_ERR PFX "Unable to read EEPROM data.\n");
		kfree(nesadapter);
		return NULL;
	}

	nesadapter->vendor_id = (((u32) nesadapter->mac_addr_high) << 8) |
				(nesadapter->mac_addr_low >> 24);

	pci_bus_read_config_word(nesdev->pcidev->bus, nesdev->pcidev->devfn,
				 PCI_DEVICE_ID, &device_id);
	nesadapter->vendor_part_id = device_id;

	if (nes_init_serdes(nesdev, hw_rev, port_count, nesadapter,
							OneG_Mode)) {
		kfree(nesadapter);
		return NULL;
	}
	nes_init_csr_ne020(nesdev, hw_rev, port_count);

	memset(nesadapter->pft_mcast_map, 255,
	       sizeof nesadapter->pft_mcast_map);

	/* populate the new nesadapter */
	nesadapter->devfn = nesdev->pcidev->devfn;
	nesadapter->bus_number = nesdev->pcidev->bus->number;
	nesadapter->ref_count = 1;
	nesadapter->timer_int_req = 0xffff0000;
	nesadapter->OneG_Mode = OneG_Mode;
	nesadapter->doorbell_start = nesdev->doorbell_region;

	/* nesadapter->tick_delta = clk_divisor; */
	nesadapter->hw_rev = hw_rev;
	nesadapter->port_count = port_count;

	nesadapter->max_qp = max_qp;
	nesadapter->hte_index_mask = hte_index_mask;
	nesadapter->max_irrq = max_irrq;
	nesadapter->max_mr = max_mr;
	nesadapter->max_256pbl = max_256pbl - 1;
	nesadapter->max_4kpbl = max_4kpbl - 1;
	nesadapter->max_cq = max_cq;
	nesadapter->free_256pbl = max_256pbl - 1;
	nesadapter->free_4kpbl = max_4kpbl - 1;
	nesadapter->max_pd = num_pds;
	nesadapter->arp_table_size = arp_table_size;

	nesadapter->et_pkt_rate_low = NES_TIMER_ENABLE_LIMIT;
	if (nes_drv_opt & NES_DRV_OPT_DISABLE_INT_MOD) {
		nesadapter->et_use_adaptive_rx_coalesce = 0;
		nesadapter->timer_int_limit = NES_TIMER_INT_LIMIT;
		nesadapter->et_rx_coalesce_usecs_irq = interrupt_mod_interval;
	} else {
		nesadapter->et_use_adaptive_rx_coalesce = 1;
		nesadapter->timer_int_limit = NES_TIMER_INT_LIMIT_DYNAMIC;
		nesadapter->et_rx_coalesce_usecs_irq = 0;
		printk(PFX "%s: Using Adaptive Interrupt Moderation\n", __func__);
	}
	/* Setup and enable the periodic timer */
	if (nesadapter->et_rx_coalesce_usecs_irq)
		nes_write32(nesdev->regs+NES_PERIODIC_CONTROL, 0x80000000 |
				((u32)(nesadapter->et_rx_coalesce_usecs_irq * 8)));
	else
		nes_write32(nesdev->regs+NES_PERIODIC_CONTROL, 0x00000000);

	nesadapter->base_pd = 1;

	nesadapter->device_cap_flags =
		IB_DEVICE_LOCAL_DMA_LKEY | IB_DEVICE_MEM_WINDOW;

	nesadapter->allocated_qps = (unsigned long *)&(((unsigned char *)nesadapter)
			[(sizeof(struct nes_adapter)+(sizeof(unsigned long)-1))&(~(sizeof(unsigned long)-1))]);
	nesadapter->allocated_cqs = &nesadapter->allocated_qps[BITS_TO_LONGS(max_qp)];
	nesadapter->allocated_mrs = &nesadapter->allocated_cqs[BITS_TO_LONGS(max_cq)];
	nesadapter->allocated_pds = &nesadapter->allocated_mrs[BITS_TO_LONGS(max_mr)];
	nesadapter->allocated_arps = &nesadapter->allocated_pds[BITS_TO_LONGS(num_pds)];
	nesadapter->qp_table = (struct nes_qp **)(&nesadapter->allocated_arps[BITS_TO_LONGS(arp_table_size)]);


	/* mark the usual suspect QPs and CQs as in use */
	for (u32temp = 0; u32temp < NES_FIRST_QPN; u32temp++) {
		set_bit(u32temp, nesadapter->allocated_qps);
		set_bit(u32temp, nesadapter->allocated_cqs);
	}

	for (u32temp = 0; u32temp < 20; u32temp++)
		set_bit(u32temp, nesadapter->allocated_pds);
	u32temp = nes_read_indexed(nesdev, NES_IDX_QP_MAX_CFG_SIZES);

	max_rq_wrs = ((u32temp >> 8) & 3);
	switch (max_rq_wrs) {
		case 0:
			max_rq_wrs = 4;
			break;
		case 1:
			max_rq_wrs = 16;
			break;
		case 2:
			max_rq_wrs = 32;
			break;
		case 3:
			max_rq_wrs = 512;
			break;
	}

	max_sq_wrs = (u32temp & 3);
	switch (max_sq_wrs) {
		case 0:
			max_sq_wrs = 4;
			break;
		case 1:
			max_sq_wrs = 16;
			break;
		case 2:
			max_sq_wrs = 32;
			break;
		case 3:
			max_sq_wrs = 512;
			break;
	}
	nesadapter->max_qp_wr = min(max_rq_wrs, max_sq_wrs);
	nesadapter->max_irrq_wr = (u32temp >> 16) & 3;

	nesadapter->max_sge = 4;
	nesadapter->max_cqe = 32767;

	if (nes_read_eeprom_values(nesdev, nesadapter)) {
		printk(KERN_ERR PFX "Unable to read EEPROM data.\n");
		kfree(nesadapter);
		return NULL;
	}

	u32temp = nes_read_indexed(nesdev, NES_IDX_TCP_TIMER_CONFIG);
	nes_write_indexed(nesdev, NES_IDX_TCP_TIMER_CONFIG,
			(u32temp & 0xff000000) | (nesadapter->tcp_timer_core_clk_divisor & 0x00ffffff));

	/* setup port configuration */
	if (nesadapter->port_count == 1) {
		nesadapter->log_port = 0x00000000;
		if (nes_drv_opt & NES_DRV_OPT_DUAL_LOGICAL_PORT)
			nes_write_indexed(nesdev, NES_IDX_TX_POOL_SIZE, 0x00000002);
		else
			nes_write_indexed(nesdev, NES_IDX_TX_POOL_SIZE, 0x00000003);
	} else {
		if (nesadapter->phy_type[0] == NES_PHY_TYPE_PUMA_1G) {
			nesadapter->log_port = 0x000000D8;
		} else {
			if (nesadapter->port_count == 2)
				nesadapter->log_port = 0x00000044;
			else
				nesadapter->log_port = 0x000000e4;
		}
		nes_write_indexed(nesdev, NES_IDX_TX_POOL_SIZE, 0x00000003);
	}

	nes_write_indexed(nesdev, NES_IDX_NIC_LOGPORT_TO_PHYPORT,
						nesadapter->log_port);
	nes_debug(NES_DBG_INIT, "Probe time, LOG2PHY=%u\n",
			nes_read_indexed(nesdev, NES_IDX_NIC_LOGPORT_TO_PHYPORT));

	spin_lock_init(&nesadapter->resource_lock);
	spin_lock_init(&nesadapter->phy_lock);
	spin_lock_init(&nesadapter->pbl_lock);
	spin_lock_init(&nesadapter->periodic_timer_lock);

	INIT_LIST_HEAD(&nesadapter->nesvnic_list[0]);
	INIT_LIST_HEAD(&nesadapter->nesvnic_list[1]);
	INIT_LIST_HEAD(&nesadapter->nesvnic_list[2]);
	INIT_LIST_HEAD(&nesadapter->nesvnic_list[3]);

	if ((!nesadapter->OneG_Mode) && (nesadapter->port_count == 2)) {
		u32 pcs_control_status0, pcs_control_status1;
		u32 reset_value;
		u32 i = 0;
		u32 int_cnt = 0;
		u32 ext_cnt = 0;
		unsigned long flags;
		u32 j = 0;

		pcs_control_status0 = nes_read_indexed(nesdev,
			NES_IDX_PHY_PCS_CONTROL_STATUS0);
		pcs_control_status1 = nes_read_indexed(nesdev,
			NES_IDX_PHY_PCS_CONTROL_STATUS0 + 0x200);

		for (i = 0; i < NES_MAX_LINK_CHECK; i++) {
			pcs_control_status0 = nes_read_indexed(nesdev,
					NES_IDX_PHY_PCS_CONTROL_STATUS0);
			pcs_control_status1 = nes_read_indexed(nesdev,
					NES_IDX_PHY_PCS_CONTROL_STATUS0 + 0x200);
			if ((0x0F000100 == (pcs_control_status0 & 0x0F000100))
			    || (0x0F000100 == (pcs_control_status1 & 0x0F000100)))
				int_cnt++;
			msleep(1);
		}
		if (int_cnt > 1) {
			spin_lock_irqsave(&nesadapter->phy_lock, flags);
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1, 0x0000F0C8);
			mh_detected++;
			reset_value = nes_read32(nesdev->regs+NES_SOFTWARE_RESET);
			reset_value |= 0x0000003d;
			nes_write32(nesdev->regs+NES_SOFTWARE_RESET, reset_value);

			while (((nes_read32(nesdev->regs+NES_SOFTWARE_RESET)
				& 0x00000040) != 0x00000040) && (j++ < 5000));
			spin_unlock_irqrestore(&nesadapter->phy_lock, flags);

			pcs_control_status0 = nes_read_indexed(nesdev,
					NES_IDX_PHY_PCS_CONTROL_STATUS0);
			pcs_control_status1 = nes_read_indexed(nesdev,
					NES_IDX_PHY_PCS_CONTROL_STATUS0 + 0x200);

			for (i = 0; i < NES_MAX_LINK_CHECK; i++) {
				pcs_control_status0 = nes_read_indexed(nesdev,
					NES_IDX_PHY_PCS_CONTROL_STATUS0);
				pcs_control_status1 = nes_read_indexed(nesdev,
					NES_IDX_PHY_PCS_CONTROL_STATUS0 + 0x200);
				if ((0x0F000100 == (pcs_control_status0 & 0x0F000100))
					|| (0x0F000100 == (pcs_control_status1 & 0x0F000100))) {
					if (++ext_cnt > int_cnt) {
						spin_lock_irqsave(&nesadapter->phy_lock, flags);
						nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1,
								0x0000F088);
						mh_detected++;
						reset_value = nes_read32(nesdev->regs+NES_SOFTWARE_RESET);
						reset_value |= 0x0000003d;
						nes_write32(nesdev->regs+NES_SOFTWARE_RESET, reset_value);

						while (((nes_read32(nesdev->regs+NES_SOFTWARE_RESET)
							& 0x00000040) != 0x00000040) && (j++ < 5000));
						spin_unlock_irqrestore(&nesadapter->phy_lock, flags);
						break;
					}
				}
				msleep(1);
			}
		}
	}

	if (nesadapter->hw_rev == NE020_REV) {
		init_timer(&nesadapter->mh_timer);
		nesadapter->mh_timer.function = nes_mh_fix;
		nesadapter->mh_timer.expires = jiffies + (HZ/5);  /* 1 second */
		nesadapter->mh_timer.data = (unsigned long)nesdev;
		add_timer(&nesadapter->mh_timer);
	} else {
		nes_write32(nesdev->regs+NES_INTF_INT_STAT, 0x0f000000);
	}

	init_timer(&nesadapter->lc_timer);
	nesadapter->lc_timer.function = nes_clc;
	nesadapter->lc_timer.expires = jiffies + 3600 * HZ;  /* 1 hour */
	nesadapter->lc_timer.data = (unsigned long)nesdev;
	add_timer(&nesadapter->lc_timer);

	list_add_tail(&nesadapter->list, &nes_adapter_list);

	for (func_index = 0; func_index < 8; func_index++) {
		pci_bus_read_config_word(nesdev->pcidev->bus,
					PCI_DEVFN(PCI_SLOT(nesdev->pcidev->devfn),
					func_index), 0, &vendor_id);
		if (vendor_id == 0xffff)
			break;
	}
	nes_debug(NES_DBG_INIT, "%s %d functions found for %s.\n", __func__,
		func_index, pci_name(nesdev->pcidev));
	nesadapter->adapter_fcn_count = func_index;

	return nesadapter;
}


/**
 * nes_reset_adapter_ne020
 */
static unsigned int nes_reset_adapter_ne020(struct nes_device *nesdev, u8 *OneG_Mode)
{
	u32 port_count;
	u32 u32temp;
	u32 i;

	u32temp = nes_read32(nesdev->regs+NES_SOFTWARE_RESET);
	port_count = ((u32temp & 0x00000300) >> 8) + 1;
	/* TODO: assuming that both SERDES are set the same for now */
	*OneG_Mode = (u32temp & 0x00003c00) ? 0 : 1;
	nes_debug(NES_DBG_INIT, "Initial Software Reset = 0x%08X, port_count=%u\n",
			u32temp, port_count);
	if (*OneG_Mode)
		nes_debug(NES_DBG_INIT, "Running in 1G mode.\n");
	u32temp &= 0xff00ffc0;
	switch (port_count) {
		case 1:
			u32temp |= 0x00ee0000;
			break;
		case 2:
			u32temp |= 0x00cc0000;
			break;
		case 4:
			u32temp |= 0x00000000;
			break;
		default:
			return 0;
			break;
	}

	/* check and do full reset if needed */
	if (nes_read_indexed(nesdev, NES_IDX_QP_CONTROL+(PCI_FUNC(nesdev->pcidev->devfn)*8))) {
		nes_debug(NES_DBG_INIT, "Issuing Full Soft reset = 0x%08X\n", u32temp | 0xd);
		nes_write32(nesdev->regs+NES_SOFTWARE_RESET, u32temp | 0xd);

		i = 0;
		while (((nes_read32(nesdev->regs+NES_SOFTWARE_RESET) & 0x00000040) == 0) && i++ < 10000)
			mdelay(1);
		if (i > 10000) {
			nes_debug(NES_DBG_INIT, "Did not see full soft reset done.\n");
			return 0;
		}

		i = 0;
		while ((nes_read_indexed(nesdev, NES_IDX_INT_CPU_STATUS) != 0x80) && i++ < 10000)
			mdelay(1);
		if (i > 10000) {
			printk(KERN_ERR PFX "Internal CPU not ready, status = %02X\n",
			       nes_read_indexed(nesdev, NES_IDX_INT_CPU_STATUS));
			return 0;
		}
	}

	/* port reset */
	switch (port_count) {
		case 1:
			u32temp |= 0x00ee0010;
			break;
		case 2:
			u32temp |= 0x00cc0030;
			break;
		case 4:
			u32temp |= 0x00000030;
			break;
	}

	nes_debug(NES_DBG_INIT, "Issuing Port Soft reset = 0x%08X\n", u32temp | 0xd);
	nes_write32(nesdev->regs+NES_SOFTWARE_RESET, u32temp | 0xd);

	i = 0;
	while (((nes_read32(nesdev->regs+NES_SOFTWARE_RESET) & 0x00000040) == 0) && i++ < 10000)
		mdelay(1);
	if (i > 10000) {
		nes_debug(NES_DBG_INIT, "Did not see port soft reset done.\n");
		return 0;
	}

	/* serdes 0 */
	i = 0;
	while (((u32temp = (nes_read_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_STATUS0)
			& 0x0000000f)) != 0x0000000f) && i++ < 5000)
		mdelay(1);
	if (i > 5000) {
		nes_debug(NES_DBG_INIT, "Serdes 0 not ready, status=%x\n", u32temp);
		return 0;
	}

	/* serdes 1 */
	if (port_count > 1) {
		i = 0;
		while (((u32temp = (nes_read_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_STATUS1)
				& 0x0000000f)) != 0x0000000f) && i++ < 5000)
			mdelay(1);
		if (i > 5000) {
			nes_debug(NES_DBG_INIT, "Serdes 1 not ready, status=%x\n", u32temp);
			return 0;
		}
	}

	return port_count;
}


/**
 * nes_init_serdes
 */
static int nes_init_serdes(struct nes_device *nesdev, u8 hw_rev, u8 port_count,
				struct nes_adapter *nesadapter, u8  OneG_Mode)
{
	int i;
	u32 u32temp;
	u32 sds;

	if (hw_rev != NE020_REV) {
		/* init serdes 0 */
		if (wide_ppm_offset && (nesadapter->phy_type[0] == NES_PHY_TYPE_CX4))
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_CDR_CONTROL0, 0x000FFFAA);
		else
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_CDR_CONTROL0, 0x000000FF);

		if (nesadapter->phy_type[0] == NES_PHY_TYPE_PUMA_1G) {
			sds = nes_read_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL0);
			sds |= 0x00000100;
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL0, sds);
		}
		if (!OneG_Mode)
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_TX_HIGHZ_LANE_MODE0, 0x11110000);

		if (port_count < 2)
			return 0;

		/* init serdes 1 */
		if (!(OneG_Mode && (nesadapter->phy_type[1] != NES_PHY_TYPE_PUMA_1G)))
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_CDR_CONTROL1, 0x000000FF);

		switch (nesadapter->phy_type[1]) {
		case NES_PHY_TYPE_ARGUS:
		case NES_PHY_TYPE_SFP_D:
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_TX_EMP0, 0x00000000);
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_TX_EMP1, 0x00000000);
			break;
		case NES_PHY_TYPE_CX4:
			if (wide_ppm_offset)
				nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_CDR_CONTROL1, 0x000FFFAA);
			break;
		case NES_PHY_TYPE_PUMA_1G:
			sds = nes_read_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1);
			sds |= 0x000000100;
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1, sds);
		}
		if (!OneG_Mode) {
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_TX_HIGHZ_LANE_MODE1, 0x11110000);
			sds = nes_read_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1);
			sds &= 0xFFFFFFBF;
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1, sds);
		}
	} else {
		/* init serdes 0 */
		nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL0, 0x00000008);
		i = 0;
		while (((u32temp = (nes_read_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_STATUS0)
				& 0x0000000f)) != 0x0000000f) && i++ < 5000)
			mdelay(1);
		if (i > 5000) {
			nes_debug(NES_DBG_PHY, "Init: serdes 0 not ready, status=%x\n", u32temp);
			return 1;
		}
		nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_TX_EMP0, 0x000bdef7);
		nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_TX_DRIVE0, 0x9ce73000);
		nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_RX_MODE0, 0x0ff00000);
		nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_RX_SIGDET0, 0x00000000);
		nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_BYPASS0, 0x00000000);
		nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_LOOPBACK_CONTROL0, 0x00000000);
		if (OneG_Mode)
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_RX_EQ_CONTROL0, 0xf0182222);
		else
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_RX_EQ_CONTROL0, 0xf0042222);

		nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_CDR_CONTROL0, 0x000000ff);
		if (port_count > 1) {
			/* init serdes 1 */
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1, 0x00000048);
			i = 0;
			while (((u32temp = (nes_read_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_STATUS1)
				& 0x0000000f)) != 0x0000000f) && (i++ < 5000))
				mdelay(1);
			if (i > 5000) {
				printk("%s: Init: serdes 1 not ready, status=%x\n", __func__, u32temp);
				/* return 1; */
			}
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_TX_EMP1, 0x000bdef7);
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_TX_DRIVE1, 0x9ce73000);
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_RX_MODE1, 0x0ff00000);
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_RX_SIGDET1, 0x00000000);
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_BYPASS1, 0x00000000);
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_LOOPBACK_CONTROL1, 0x00000000);
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_RX_EQ_CONTROL1, 0xf0002222);
			nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_CDR_CONTROL1, 0x000000ff);
		}
	}
	return 0;
}


/**
 * nes_init_csr_ne020
 * Initialize registers for ne020 hardware
 */
static void nes_init_csr_ne020(struct nes_device *nesdev, u8 hw_rev, u8 port_count)
{
	u32 u32temp;

	nes_debug(NES_DBG_INIT, "port_count=%d\n", port_count);

	nes_write_indexed(nesdev, 0x000001E4, 0x00000007);
	/* nes_write_indexed(nesdev, 0x000001E8, 0x000208C4); */
	nes_write_indexed(nesdev, 0x000001E8, 0x00020874);
	nes_write_indexed(nesdev, 0x000001D8, 0x00048002);
	/* nes_write_indexed(nesdev, 0x000001D8, 0x0004B002); */
	nes_write_indexed(nesdev, 0x000001FC, 0x00050005);
	nes_write_indexed(nesdev, 0x00000600, 0x55555555);
	nes_write_indexed(nesdev, 0x00000604, 0x55555555);

	/* TODO: move these MAC register settings to NIC bringup */
	nes_write_indexed(nesdev, 0x00002000, 0x00000001);
	nes_write_indexed(nesdev, 0x00002004, 0x00000001);
	nes_write_indexed(nesdev, 0x00002008, 0x0000FFFF);
	nes_write_indexed(nesdev, 0x0000200C, 0x00000001);
	nes_write_indexed(nesdev, 0x00002010, 0x000003c1);
	nes_write_indexed(nesdev, 0x0000201C, 0x75345678);
	if (port_count > 1) {
		nes_write_indexed(nesdev, 0x00002200, 0x00000001);
		nes_write_indexed(nesdev, 0x00002204, 0x00000001);
		nes_write_indexed(nesdev, 0x00002208, 0x0000FFFF);
		nes_write_indexed(nesdev, 0x0000220C, 0x00000001);
		nes_write_indexed(nesdev, 0x00002210, 0x000003c1);
		nes_write_indexed(nesdev, 0x0000221C, 0x75345678);
		nes_write_indexed(nesdev, 0x00000908, 0x20000001);
	}
	if (port_count > 2) {
		nes_write_indexed(nesdev, 0x00002400, 0x00000001);
		nes_write_indexed(nesdev, 0x00002404, 0x00000001);
		nes_write_indexed(nesdev, 0x00002408, 0x0000FFFF);
		nes_write_indexed(nesdev, 0x0000240C, 0x00000001);
		nes_write_indexed(nesdev, 0x00002410, 0x000003c1);
		nes_write_indexed(nesdev, 0x0000241C, 0x75345678);
		nes_write_indexed(nesdev, 0x00000910, 0x20000001);

		nes_write_indexed(nesdev, 0x00002600, 0x00000001);
		nes_write_indexed(nesdev, 0x00002604, 0x00000001);
		nes_write_indexed(nesdev, 0x00002608, 0x0000FFFF);
		nes_write_indexed(nesdev, 0x0000260C, 0x00000001);
		nes_write_indexed(nesdev, 0x00002610, 0x000003c1);
		nes_write_indexed(nesdev, 0x0000261C, 0x75345678);
		nes_write_indexed(nesdev, 0x00000918, 0x20000001);
	}

	nes_write_indexed(nesdev, 0x00005000, 0x00018000);
	/* nes_write_indexed(nesdev, 0x00005000, 0x00010000); */
	nes_write_indexed(nesdev, NES_IDX_WQM_CONFIG1, (wqm_quanta << 1) |
							 0x00000001);
	nes_write_indexed(nesdev, 0x00005008, 0x1F1F1F1F);
	nes_write_indexed(nesdev, 0x00005010, 0x1F1F1F1F);
	nes_write_indexed(nesdev, 0x00005018, 0x1F1F1F1F);
	nes_write_indexed(nesdev, 0x00005020, 0x1F1F1F1F);
	nes_write_indexed(nesdev, 0x00006090, 0xFFFFFFFF);

	/* TODO: move this to code, get from EEPROM */
	nes_write_indexed(nesdev, 0x00000900, 0x20000001);
	nes_write_indexed(nesdev, 0x000060C0, 0x0000028e);
	nes_write_indexed(nesdev, 0x000060C8, 0x00000020);

	nes_write_indexed(nesdev, 0x000001EC, 0x7b2625a0);
	/* nes_write_indexed(nesdev, 0x000001EC, 0x5f2625a0); */

	if (hw_rev != NE020_REV) {
		u32temp = nes_read_indexed(nesdev, 0x000008e8);
		u32temp |= 0x80000000;
		nes_write_indexed(nesdev, 0x000008e8, u32temp);
		u32temp = nes_read_indexed(nesdev, 0x000021f8);
		u32temp &= 0x7fffffff;
		u32temp |= 0x7fff0010;
		nes_write_indexed(nesdev, 0x000021f8, u32temp);
		if (port_count > 1) {
			u32temp = nes_read_indexed(nesdev, 0x000023f8);
			u32temp &= 0x7fffffff;
			u32temp |= 0x7fff0010;
			nes_write_indexed(nesdev, 0x000023f8, u32temp);
		}
	}
}


/**
 * nes_destroy_adapter - destroy the adapter structure
 */
void nes_destroy_adapter(struct nes_adapter *nesadapter)
{
	struct nes_adapter *tmp_adapter;

	list_for_each_entry(tmp_adapter, &nes_adapter_list, list) {
		nes_debug(NES_DBG_SHUTDOWN, "Nes Adapter list entry = 0x%p.\n",
				tmp_adapter);
	}

	nesadapter->ref_count--;
	if (!nesadapter->ref_count) {
		if (nesadapter->hw_rev == NE020_REV) {
			del_timer(&nesadapter->mh_timer);
		}
		del_timer(&nesadapter->lc_timer);

		list_del(&nesadapter->list);
		kfree(nesadapter);
	}
}


/**
 * nes_init_cqp
 */
int nes_init_cqp(struct nes_device *nesdev)
{
	struct nes_adapter *nesadapter = nesdev->nesadapter;
	struct nes_hw_cqp_qp_context *cqp_qp_context;
	struct nes_hw_cqp_wqe *cqp_wqe;
	struct nes_hw_ceq *ceq;
	struct nes_hw_ceq *nic_ceq;
	struct nes_hw_aeq *aeq;
	void *vmem;
	dma_addr_t pmem;
	u32 count=0;
	u32 cqp_head;
	u64 u64temp;
	u32 u32temp;

	/* allocate CQP memory */
	/* Need to add max_cq to the aeq size once cq overflow checking is added back */
	/* SQ is 512 byte aligned, others are 256 byte aligned */
	nesdev->cqp_mem_size = 512 +
			(sizeof(struct nes_hw_cqp_wqe) * NES_CQP_SQ_SIZE) +
			(sizeof(struct nes_hw_cqe) * NES_CCQ_SIZE) +
			max(((u32)sizeof(struct nes_hw_ceqe) * NES_CCEQ_SIZE), (u32)256) +
			max(((u32)sizeof(struct nes_hw_ceqe) * NES_NIC_CEQ_SIZE), (u32)256) +
			(sizeof(struct nes_hw_aeqe) * nesadapter->max_qp) +
			sizeof(struct nes_hw_cqp_qp_context);

	nesdev->cqp_vbase = pci_alloc_consistent(nesdev->pcidev, nesdev->cqp_mem_size,
			&nesdev->cqp_pbase);
	if (!nesdev->cqp_vbase) {
		nes_debug(NES_DBG_INIT, "Unable to allocate memory for host descriptor rings\n");
		return -ENOMEM;
	}
	memset(nesdev->cqp_vbase, 0, nesdev->cqp_mem_size);

	/* Allocate a twice the number of CQP requests as the SQ size */
	nesdev->nes_cqp_requests = kzalloc(sizeof(struct nes_cqp_request) *
			2 * NES_CQP_SQ_SIZE, GFP_KERNEL);
	if (nesdev->nes_cqp_requests == NULL) {
		nes_debug(NES_DBG_INIT, "Unable to allocate memory CQP request entries.\n");
		pci_free_consistent(nesdev->pcidev, nesdev->cqp_mem_size, nesdev->cqp.sq_vbase,
				nesdev->cqp.sq_pbase);
		return -ENOMEM;
	}

	nes_debug(NES_DBG_INIT, "Allocated CQP structures at %p (phys = %016lX), size = %u.\n",
			nesdev->cqp_vbase, (unsigned long)nesdev->cqp_pbase, nesdev->cqp_mem_size);

	spin_lock_init(&nesdev->cqp.lock);
	init_waitqueue_head(&nesdev->cqp.waitq);

	/* Setup Various Structures */
	vmem = (void *)(((unsigned long)nesdev->cqp_vbase + (512 - 1)) &
			~(unsigned long)(512 - 1));
	pmem = (dma_addr_t)(((unsigned long long)nesdev->cqp_pbase + (512 - 1)) &
			~(unsigned long long)(512 - 1));

	nesdev->cqp.sq_vbase = vmem;
	nesdev->cqp.sq_pbase = pmem;
	nesdev->cqp.sq_size = NES_CQP_SQ_SIZE;
	nesdev->cqp.sq_head = 0;
	nesdev->cqp.sq_tail = 0;
	nesdev->cqp.qp_id = PCI_FUNC(nesdev->pcidev->devfn);

	vmem += (sizeof(struct nes_hw_cqp_wqe) * nesdev->cqp.sq_size);
	pmem += (sizeof(struct nes_hw_cqp_wqe) * nesdev->cqp.sq_size);

	nesdev->ccq.cq_vbase = vmem;
	nesdev->ccq.cq_pbase = pmem;
	nesdev->ccq.cq_size = NES_CCQ_SIZE;
	nesdev->ccq.cq_head = 0;
	nesdev->ccq.ce_handler = nes_cqp_ce_handler;
	nesdev->ccq.cq_number = PCI_FUNC(nesdev->pcidev->devfn);

	vmem += (sizeof(struct nes_hw_cqe) * nesdev->ccq.cq_size);
	pmem += (sizeof(struct nes_hw_cqe) * nesdev->ccq.cq_size);

	nesdev->ceq_index = PCI_FUNC(nesdev->pcidev->devfn);
	ceq = &nesadapter->ceq[nesdev->ceq_index];
	ceq->ceq_vbase = vmem;
	ceq->ceq_pbase = pmem;
	ceq->ceq_size = NES_CCEQ_SIZE;
	ceq->ceq_head = 0;

	vmem += max(((u32)sizeof(struct nes_hw_ceqe) * ceq->ceq_size), (u32)256);
	pmem += max(((u32)sizeof(struct nes_hw_ceqe) * ceq->ceq_size), (u32)256);

	nesdev->nic_ceq_index = PCI_FUNC(nesdev->pcidev->devfn) + 8;
	nic_ceq = &nesadapter->ceq[nesdev->nic_ceq_index];
	nic_ceq->ceq_vbase = vmem;
	nic_ceq->ceq_pbase = pmem;
	nic_ceq->ceq_size = NES_NIC_CEQ_SIZE;
	nic_ceq->ceq_head = 0;

	vmem += max(((u32)sizeof(struct nes_hw_ceqe) * nic_ceq->ceq_size), (u32)256);
	pmem += max(((u32)sizeof(struct nes_hw_ceqe) * nic_ceq->ceq_size), (u32)256);

	aeq = &nesadapter->aeq[PCI_FUNC(nesdev->pcidev->devfn)];
	aeq->aeq_vbase = vmem;
	aeq->aeq_pbase = pmem;
	aeq->aeq_size = nesadapter->max_qp;
	aeq->aeq_head = 0;

	/* Setup QP Context */
	vmem += (sizeof(struct nes_hw_aeqe) * aeq->aeq_size);
	pmem += (sizeof(struct nes_hw_aeqe) * aeq->aeq_size);

	cqp_qp_context = vmem;
	cqp_qp_context->context_words[0] =
			cpu_to_le32((PCI_FUNC(nesdev->pcidev->devfn) << 12) + (2 << 10));
	cqp_qp_context->context_words[1] = 0;
	cqp_qp_context->context_words[2] = cpu_to_le32((u32)nesdev->cqp.sq_pbase);
	cqp_qp_context->context_words[3] = cpu_to_le32(((u64)nesdev->cqp.sq_pbase) >> 32);


	/* Write the address to Create CQP */
	if ((sizeof(dma_addr_t) > 4)) {
		nes_write_indexed(nesdev,
				NES_IDX_CREATE_CQP_HIGH + (PCI_FUNC(nesdev->pcidev->devfn) * 8),
				((u64)pmem) >> 32);
	} else {
		nes_write_indexed(nesdev,
				NES_IDX_CREATE_CQP_HIGH + (PCI_FUNC(nesdev->pcidev->devfn) * 8), 0);
	}
	nes_write_indexed(nesdev,
			NES_IDX_CREATE_CQP_LOW + (PCI_FUNC(nesdev->pcidev->devfn) * 8),
			(u32)pmem);

	INIT_LIST_HEAD(&nesdev->cqp_avail_reqs);
	INIT_LIST_HEAD(&nesdev->cqp_pending_reqs);

	for (count = 0; count < 2*NES_CQP_SQ_SIZE; count++) {
		init_waitqueue_head(&nesdev->nes_cqp_requests[count].waitq);
		list_add_tail(&nesdev->nes_cqp_requests[count].list, &nesdev->cqp_avail_reqs);
	}

	/* Write Create CCQ WQE */
	cqp_head = nesdev->cqp.sq_head++;
	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];
	nes_fill_init_cqp_wqe(cqp_wqe, nesdev);
	set_wqe_32bit_value(cqp_wqe->wqe_words, NES_CQP_WQE_OPCODE_IDX,
			(NES_CQP_CREATE_CQ | NES_CQP_CQ_CEQ_VALID |
			NES_CQP_CQ_CHK_OVERFLOW | ((u32)nesdev->ccq.cq_size << 16)));
	set_wqe_32bit_value(cqp_wqe->wqe_words, NES_CQP_WQE_ID_IDX,
			    (nesdev->ccq.cq_number |
			     ((u32)nesdev->ceq_index << 16)));
	u64temp = (u64)nesdev->ccq.cq_pbase;
	set_wqe_64bit_value(cqp_wqe->wqe_words, NES_CQP_CQ_WQE_PBL_LOW_IDX, u64temp);
	cqp_wqe->wqe_words[NES_CQP_CQ_WQE_CQ_CONTEXT_HIGH_IDX] = 0;
	u64temp = (unsigned long)&nesdev->ccq;
	cqp_wqe->wqe_words[NES_CQP_CQ_WQE_CQ_CONTEXT_LOW_IDX] =
			cpu_to_le32((u32)(u64temp >> 1));
	cqp_wqe->wqe_words[NES_CQP_CQ_WQE_CQ_CONTEXT_HIGH_IDX] =
			cpu_to_le32(((u32)((u64temp) >> 33)) & 0x7FFFFFFF);
	cqp_wqe->wqe_words[NES_CQP_CQ_WQE_DOORBELL_INDEX_HIGH_IDX] = 0;

	/* Write Create CEQ WQE */
	cqp_head = nesdev->cqp.sq_head++;
	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];
	nes_fill_init_cqp_wqe(cqp_wqe, nesdev);
	set_wqe_32bit_value(cqp_wqe->wqe_words, NES_CQP_WQE_OPCODE_IDX,
			    (NES_CQP_CREATE_CEQ + ((u32)nesdev->ceq_index << 8)));
	set_wqe_32bit_value(cqp_wqe->wqe_words, NES_CQP_CEQ_WQE_ELEMENT_COUNT_IDX, ceq->ceq_size);
	u64temp = (u64)ceq->ceq_pbase;
	set_wqe_64bit_value(cqp_wqe->wqe_words, NES_CQP_CQ_WQE_PBL_LOW_IDX, u64temp);

	/* Write Create AEQ WQE */
	cqp_head = nesdev->cqp.sq_head++;
	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];
	nes_fill_init_cqp_wqe(cqp_wqe, nesdev);
	set_wqe_32bit_value(cqp_wqe->wqe_words, NES_CQP_WQE_OPCODE_IDX,
			(NES_CQP_CREATE_AEQ + ((u32)PCI_FUNC(nesdev->pcidev->devfn) << 8)));
	set_wqe_32bit_value(cqp_wqe->wqe_words, NES_CQP_AEQ_WQE_ELEMENT_COUNT_IDX, aeq->aeq_size);
	u64temp = (u64)aeq->aeq_pbase;
	set_wqe_64bit_value(cqp_wqe->wqe_words, NES_CQP_CQ_WQE_PBL_LOW_IDX, u64temp);

	/* Write Create NIC CEQ WQE */
	cqp_head = nesdev->cqp.sq_head++;
	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];
	nes_fill_init_cqp_wqe(cqp_wqe, nesdev);
	set_wqe_32bit_value(cqp_wqe->wqe_words, NES_CQP_WQE_OPCODE_IDX,
			(NES_CQP_CREATE_CEQ + ((u32)nesdev->nic_ceq_index << 8)));
	set_wqe_32bit_value(cqp_wqe->wqe_words, NES_CQP_CEQ_WQE_ELEMENT_COUNT_IDX, nic_ceq->ceq_size);
	u64temp = (u64)nic_ceq->ceq_pbase;
	set_wqe_64bit_value(cqp_wqe->wqe_words, NES_CQP_CQ_WQE_PBL_LOW_IDX, u64temp);

	/* Poll until CCQP done */
	count = 0;
	do {
		if (count++ > 1000) {
			printk(KERN_ERR PFX "Error creating CQP\n");
			pci_free_consistent(nesdev->pcidev, nesdev->cqp_mem_size,
					nesdev->cqp_vbase, nesdev->cqp_pbase);
			return -1;
		}
		udelay(10);
	} while (!(nes_read_indexed(nesdev,
			NES_IDX_QP_CONTROL + (PCI_FUNC(nesdev->pcidev->devfn) * 8)) & (1 << 8)));

	nes_debug(NES_DBG_INIT, "CQP Status = 0x%08X\n", nes_read_indexed(nesdev,
			NES_IDX_QP_CONTROL+(PCI_FUNC(nesdev->pcidev->devfn)*8)));

	u32temp = 0x04800000;
	nes_write32(nesdev->regs+NES_WQE_ALLOC, u32temp | nesdev->cqp.qp_id);

	/* wait for the CCQ, CEQ, and AEQ to get created */
	count = 0;
	do {
		if (count++ > 1000) {
			printk(KERN_ERR PFX "Error creating CCQ, CEQ, and AEQ\n");
			pci_free_consistent(nesdev->pcidev, nesdev->cqp_mem_size,
					nesdev->cqp_vbase, nesdev->cqp_pbase);
			return -1;
		}
		udelay(10);
	} while (((nes_read_indexed(nesdev,
			NES_IDX_QP_CONTROL+(PCI_FUNC(nesdev->pcidev->devfn)*8)) & (15<<8)) != (15<<8)));

	/* dump the QP status value */
	nes_debug(NES_DBG_INIT, "QP Status = 0x%08X\n", nes_read_indexed(nesdev,
			NES_IDX_QP_CONTROL+(PCI_FUNC(nesdev->pcidev->devfn)*8)));

	nesdev->cqp.sq_tail++;

	return 0;
}


/**
 * nes_destroy_cqp
 */
int nes_destroy_cqp(struct nes_device *nesdev)
{
	struct nes_hw_cqp_wqe *cqp_wqe;
	u32 count = 0;
	u32 cqp_head;
	unsigned long flags;

	do {
		if (count++ > 1000)
			break;
		udelay(10);
	} while (!(nesdev->cqp.sq_head == nesdev->cqp.sq_tail));

	/* Reset CCQ */
	nes_write32(nesdev->regs+NES_CQE_ALLOC, NES_CQE_ALLOC_RESET |
			nesdev->ccq.cq_number);

	/* Disable device interrupts */
	nes_write32(nesdev->regs+NES_INT_MASK, 0x7fffffff);

	spin_lock_irqsave(&nesdev->cqp.lock, flags);

	/* Destroy the AEQ */
	cqp_head = nesdev->cqp.sq_head++;
	nesdev->cqp.sq_head &= nesdev->cqp.sq_size-1;
	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];
	cqp_wqe->wqe_words[NES_CQP_WQE_OPCODE_IDX] = cpu_to_le32(NES_CQP_DESTROY_AEQ |
			((u32)PCI_FUNC(nesdev->pcidev->devfn) << 8));
	cqp_wqe->wqe_words[NES_CQP_WQE_COMP_CTX_HIGH_IDX] = 0;

	/* Destroy the NIC CEQ */
	cqp_head = nesdev->cqp.sq_head++;
	nesdev->cqp.sq_head &= nesdev->cqp.sq_size-1;
	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];
	cqp_wqe->wqe_words[NES_CQP_WQE_OPCODE_IDX] = cpu_to_le32(NES_CQP_DESTROY_CEQ |
			((u32)nesdev->nic_ceq_index << 8));

	/* Destroy the CEQ */
	cqp_head = nesdev->cqp.sq_head++;
	nesdev->cqp.sq_head &= nesdev->cqp.sq_size-1;
	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];
	cqp_wqe->wqe_words[NES_CQP_WQE_OPCODE_IDX] = cpu_to_le32(NES_CQP_DESTROY_CEQ |
			(nesdev->ceq_index << 8));

	/* Destroy the CCQ */
	cqp_head = nesdev->cqp.sq_head++;
	nesdev->cqp.sq_head &= nesdev->cqp.sq_size-1;
	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];
	cqp_wqe->wqe_words[NES_CQP_WQE_OPCODE_IDX] = cpu_to_le32(NES_CQP_DESTROY_CQ);
	cqp_wqe->wqe_words[NES_CQP_WQE_ID_IDX] = cpu_to_le32(nesdev->ccq.cq_number |
			((u32)nesdev->ceq_index << 16));

	/* Destroy CQP */
	cqp_head = nesdev->cqp.sq_head++;
	nesdev->cqp.sq_head &= nesdev->cqp.sq_size-1;
	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];
	cqp_wqe->wqe_words[NES_CQP_WQE_OPCODE_IDX] = cpu_to_le32(NES_CQP_DESTROY_QP |
			NES_CQP_QP_TYPE_CQP);
	cqp_wqe->wqe_words[NES_CQP_WQE_ID_IDX] = cpu_to_le32(nesdev->cqp.qp_id);

	barrier();
	/* Ring doorbell (5 WQEs) */
	nes_write32(nesdev->regs+NES_WQE_ALLOC, 0x05800000 | nesdev->cqp.qp_id);

	spin_unlock_irqrestore(&nesdev->cqp.lock, flags);

	/* wait for the CCQ, CEQ, and AEQ to get destroyed */
	count = 0;
	do {
		if (count++ > 1000) {
			printk(KERN_ERR PFX "Function%d: Error destroying CCQ, CEQ, and AEQ\n",
					PCI_FUNC(nesdev->pcidev->devfn));
			break;
		}
		udelay(10);
	} while (((nes_read_indexed(nesdev,
			NES_IDX_QP_CONTROL + (PCI_FUNC(nesdev->pcidev->devfn)*8)) & (15 << 8)) != 0));

	/* dump the QP status value */
	nes_debug(NES_DBG_SHUTDOWN, "Function%d: QP Status = 0x%08X\n",
			PCI_FUNC(nesdev->pcidev->devfn),
			nes_read_indexed(nesdev,
			NES_IDX_QP_CONTROL+(PCI_FUNC(nesdev->pcidev->devfn)*8)));

	kfree(nesdev->nes_cqp_requests);

	/* Free the control structures */
	pci_free_consistent(nesdev->pcidev, nesdev->cqp_mem_size, nesdev->cqp.sq_vbase,
			nesdev->cqp.sq_pbase);

	return 0;
}


/**
 * nes_init_phy
 */
int nes_init_phy(struct nes_device *nesdev)
{
	struct nes_adapter *nesadapter = nesdev->nesadapter;
	u32 counter = 0;
	u32 sds;
	u32 mac_index = nesdev->mac_index;
	u32 tx_config = 0;
	u16 phy_data;
	u32 temp_phy_data = 0;
	u32 temp_phy_data2 = 0;
	u8  phy_type = nesadapter->phy_type[mac_index];
	u8  phy_index = nesadapter->phy_index[mac_index];

	if ((nesadapter->OneG_Mode) &&
	    (phy_type != NES_PHY_TYPE_PUMA_1G)) {
		nes_debug(NES_DBG_PHY, "1G PHY, mac_index = %d.\n", mac_index);
		if (phy_type == NES_PHY_TYPE_1G) {
			tx_config = nes_read_indexed(nesdev, NES_IDX_MAC_TX_CONFIG);
			tx_config &= 0xFFFFFFE3;
			tx_config |= 0x04;
			nes_write_indexed(nesdev, NES_IDX_MAC_TX_CONFIG, tx_config);
		}

		nes_read_1G_phy_reg(nesdev, 1, phy_index, &phy_data);
		nes_write_1G_phy_reg(nesdev, 23, phy_index, 0xb000);

		/* Reset the PHY */
		nes_write_1G_phy_reg(nesdev, 0, phy_index, 0x8000);
		udelay(100);
		counter = 0;
		do {
			nes_read_1G_phy_reg(nesdev, 0, phy_index, &phy_data);
			if (counter++ > 100)
				break;
		} while (phy_data & 0x8000);

		/* Setting no phy loopback */
		phy_data &= 0xbfff;
		phy_data |= 0x1140;
		nes_write_1G_phy_reg(nesdev, 0, phy_index,  phy_data);
		nes_read_1G_phy_reg(nesdev, 0, phy_index, &phy_data);
		nes_read_1G_phy_reg(nesdev, 0x17, phy_index, &phy_data);
		nes_read_1G_phy_reg(nesdev, 0x1e, phy_index, &phy_data);

		/* Setting the interrupt mask */
		nes_read_1G_phy_reg(nesdev, 0x19, phy_index, &phy_data);
		nes_write_1G_phy_reg(nesdev, 0x19, phy_index, 0xffee);
		nes_read_1G_phy_reg(nesdev, 0x19, phy_index, &phy_data);

		/* turning on flow control */
		nes_read_1G_phy_reg(nesdev, 4, phy_index, &phy_data);
		nes_write_1G_phy_reg(nesdev, 4, phy_index, (phy_data & ~(0x03E0)) | 0xc00);
		nes_read_1G_phy_reg(nesdev, 4, phy_index, &phy_data);

		/* Clear Half duplex */
		nes_read_1G_phy_reg(nesdev, 9, phy_index, &phy_data);
		nes_write_1G_phy_reg(nesdev, 9, phy_index, phy_data & ~(0x0100));
		nes_read_1G_phy_reg(nesdev, 9, phy_index, &phy_data);

		nes_read_1G_phy_reg(nesdev, 0, phy_index, &phy_data);
		nes_write_1G_phy_reg(nesdev, 0, phy_index, phy_data | 0x0300);

		return 0;
	}

	if ((phy_type == NES_PHY_TYPE_IRIS) ||
	    (phy_type == NES_PHY_TYPE_ARGUS) ||
	    (phy_type == NES_PHY_TYPE_SFP_D)) {
		/* setup 10G MDIO operation */
		tx_config = nes_read_indexed(nesdev, NES_IDX_MAC_TX_CONFIG);
		tx_config &= 0xFFFFFFE3;
		tx_config |= 0x15;
		nes_write_indexed(nesdev, NES_IDX_MAC_TX_CONFIG, tx_config);
	}
	if ((phy_type == NES_PHY_TYPE_ARGUS) ||
	    (phy_type == NES_PHY_TYPE_SFP_D)) {
		/* Check firmware heartbeat */
		nes_read_10G_phy_reg(nesdev, phy_index, 0x3, 0xd7ee);
		temp_phy_data = (u16)nes_read_indexed(nesdev, NES_IDX_MAC_MDIO_CONTROL);
		udelay(1500);
		nes_read_10G_phy_reg(nesdev, phy_index, 0x3, 0xd7ee);
		temp_phy_data2 = (u16)nes_read_indexed(nesdev, NES_IDX_MAC_MDIO_CONTROL);

		if (temp_phy_data != temp_phy_data2)
			return 0;

		/* no heartbeat, configure the PHY */
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0x0000, 0x8000);
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xc300, 0x0000);
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xc316, 0x000A);
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xc318, 0x0052);
		if (phy_type == NES_PHY_TYPE_ARGUS) {
			nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xc302, 0x000C);
			nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xc319, 0x0008);
			nes_write_10G_phy_reg(nesdev, phy_index, 0x3, 0x0027, 0x0001);
		} else {
			nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xc302, 0x0004);
			nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xc319, 0x0038);
			nes_write_10G_phy_reg(nesdev, phy_index, 0x3, 0x0027, 0x0013);
		}
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xc31a, 0x0098);
		nes_write_10G_phy_reg(nesdev, phy_index, 0x3, 0x0026, 0x0E00);

		/* setup LEDs */
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xd006, 0x0007);
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xd007, 0x000A);
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xd008, 0x0009);

		nes_write_10G_phy_reg(nesdev, phy_index, 0x3, 0x0028, 0xA528);

		/* Bring PHY out of reset */
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xc300, 0x0002);

		/* Check for heartbeat */
		counter = 0;
		mdelay(690);
		nes_read_10G_phy_reg(nesdev, phy_index, 0x3, 0xd7ee);
		temp_phy_data = (u16)nes_read_indexed(nesdev, NES_IDX_MAC_MDIO_CONTROL);
		do {
			if (counter++ > 150) {
				nes_debug(NES_DBG_PHY, "No PHY heartbeat\n");
				break;
			}
			mdelay(1);
			nes_read_10G_phy_reg(nesdev, phy_index, 0x3, 0xd7ee);
			temp_phy_data2 = (u16)nes_read_indexed(nesdev, NES_IDX_MAC_MDIO_CONTROL);
		} while ((temp_phy_data2 == temp_phy_data));

		/* wait for tracking */
		counter = 0;
		do {
			nes_read_10G_phy_reg(nesdev, phy_index, 0x3, 0xd7fd);
			temp_phy_data = (u16)nes_read_indexed(nesdev, NES_IDX_MAC_MDIO_CONTROL);
			if (counter++ > 300) {
				nes_debug(NES_DBG_PHY, "PHY did not track\n");
				break;
			}
			mdelay(10);
		} while (((temp_phy_data & 0xff) != 0x50) && ((temp_phy_data & 0xff) != 0x70));

		/* setup signal integrity */
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xd003, 0x0000);
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xF00D, 0x00FE);
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xF00E, 0x0032);
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xF00F, 0x0002);
		nes_write_10G_phy_reg(nesdev, phy_index, 0x1, 0xc314, 0x0063);

		/* reset serdes */
		sds = nes_read_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL0 +
				       mac_index * 0x200);
		sds |= 0x1;
		nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL0 +
				  mac_index * 0x200, sds);
		sds &= 0xfffffffe;
		nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL0 +
				  mac_index * 0x200, sds);

		counter = 0;
		while (((nes_read32(nesdev->regs + NES_SOFTWARE_RESET) & 0x00000040) != 0x00000040)
				&& (counter++ < 5000))
			;
	}
	return 0;
}


/**
 * nes_replenish_nic_rq
 */
static void nes_replenish_nic_rq(struct nes_vnic *nesvnic)
{
	unsigned long flags;
	dma_addr_t bus_address;
	struct sk_buff *skb;
	struct nes_hw_nic_rq_wqe *nic_rqe;
	struct nes_hw_nic *nesnic;
	struct nes_device *nesdev;
	u32 rx_wqes_posted = 0;

	nesnic = &nesvnic->nic;
	nesdev = nesvnic->nesdev;
	spin_lock_irqsave(&nesnic->rq_lock, flags);
	if (nesnic->replenishing_rq !=0) {
		if (((nesnic->rq_size-1) == atomic_read(&nesvnic->rx_skbs_needed)) &&
				(atomic_read(&nesvnic->rx_skb_timer_running) == 0)) {
			atomic_set(&nesvnic->rx_skb_timer_running, 1);
			spin_unlock_irqrestore(&nesnic->rq_lock, flags);
			nesvnic->rq_wqes_timer.expires = jiffies + (HZ/2);	/* 1/2 second */
			add_timer(&nesvnic->rq_wqes_timer);
		} else
		spin_unlock_irqrestore(&nesnic->rq_lock, flags);
		return;
	}
	nesnic->replenishing_rq = 1;
	spin_unlock_irqrestore(&nesnic->rq_lock, flags);
	do {
		skb = dev_alloc_skb(nesvnic->max_frame_size);
		if (skb) {
			skb->dev = nesvnic->netdev;

			bus_address = pci_map_single(nesdev->pcidev,
					skb->data, nesvnic->max_frame_size, PCI_DMA_FROMDEVICE);

			nic_rqe = &nesnic->rq_vbase[nesvnic->nic.rq_head];
			nic_rqe->wqe_words[NES_NIC_RQ_WQE_LENGTH_1_0_IDX] =
					cpu_to_le32(nesvnic->max_frame_size);
			nic_rqe->wqe_words[NES_NIC_RQ_WQE_LENGTH_3_2_IDX] = 0;
			nic_rqe->wqe_words[NES_NIC_RQ_WQE_FRAG0_LOW_IDX] =
					cpu_to_le32((u32)bus_address);
			nic_rqe->wqe_words[NES_NIC_RQ_WQE_FRAG0_HIGH_IDX] =
					cpu_to_le32((u32)((u64)bus_address >> 32));
			nesnic->rx_skb[nesnic->rq_head] = skb;
			nesnic->rq_head++;
			nesnic->rq_head &= nesnic->rq_size - 1;
			atomic_dec(&nesvnic->rx_skbs_needed);
			barrier();
			if (++rx_wqes_posted == 255) {
				nes_write32(nesdev->regs+NES_WQE_ALLOC, (rx_wqes_posted << 24) | nesnic->qp_id);
				rx_wqes_posted = 0;
			}
		} else {
			spin_lock_irqsave(&nesnic->rq_lock, flags);
			if (((nesnic->rq_size-1) == atomic_read(&nesvnic->rx_skbs_needed)) &&
					(atomic_read(&nesvnic->rx_skb_timer_running) == 0)) {
				atomic_set(&nesvnic->rx_skb_timer_running, 1);
				spin_unlock_irqrestore(&nesnic->rq_lock, flags);
				nesvnic->rq_wqes_timer.expires = jiffies + (HZ/2);	/* 1/2 second */
				add_timer(&nesvnic->rq_wqes_timer);
			} else
				spin_unlock_irqrestore(&nesnic->rq_lock, flags);
			break;
		}
	} while (atomic_read(&nesvnic->rx_skbs_needed));
	barrier();
	if (rx_wqes_posted)
		nes_write32(nesdev->regs+NES_WQE_ALLOC, (rx_wqes_posted << 24) | nesnic->qp_id);
	nesnic->replenishing_rq = 0;
}


/**
 * nes_rq_wqes_timeout
 */
static void nes_rq_wqes_timeout(unsigned long parm)
{
	struct nes_vnic *nesvnic = (struct nes_vnic *)parm;
	printk("%s: Timer fired.\n", __func__);
	atomic_set(&nesvnic->rx_skb_timer_running, 0);
	if (atomic_read(&nesvnic->rx_skbs_needed))
		nes_replenish_nic_rq(nesvnic);
}


static int nes_lro_get_skb_hdr(struct sk_buff *skb, void **iphdr,
			       void **tcph, u64 *hdr_flags, void *priv)
{
	unsigned int ip_len;
	struct iphdr *iph;
	skb_reset_network_header(skb);
	iph = ip_hdr(skb);
	if (iph->protocol != IPPROTO_TCP)
		return -1;
	ip_len = ip_hdrlen(skb);
	skb_set_transport_header(skb, ip_len);
	*tcph = tcp_hdr(skb);

	*hdr_flags = LRO_IPV4 | LRO_TCP;
	*iphdr = iph;
	return 0;
}


/**
 * nes_init_nic_qp
 */
int nes_init_nic_qp(struct nes_device *nesdev, struct net_device *netdev)
{
	struct nes_hw_cqp_wqe *cqp_wqe;
	struct nes_hw_nic_sq_wqe *nic_sqe;
	struct nes_hw_nic_qp_context *nic_context;
	struct sk_buff *skb;
	struct nes_hw_nic_rq_wqe *nic_rqe;
	struct nes_vnic *nesvnic = netdev_priv(netdev);
	unsigned long flags;
	void *vmem;
	dma_addr_t pmem;
	u64 u64temp;
	int ret;
	u32 cqp_head;
	u32 counter;
	u32 wqe_count;
	u8 jumbomode=0;

	/* Allocate fragment, SQ, RQ, and CQ; Reuse CEQ based on the PCI function */
	nesvnic->nic_mem_size = 256 +
			(NES_NIC_WQ_SIZE * sizeof(struct nes_first_frag)) +
			(NES_NIC_WQ_SIZE * sizeof(struct nes_hw_nic_sq_wqe)) +
			(NES_NIC_WQ_SIZE * sizeof(struct nes_hw_nic_rq_wqe)) +
			(NES_NIC_WQ_SIZE * 2 * sizeof(struct nes_hw_nic_cqe)) +
			sizeof(struct nes_hw_nic_qp_context);

	nesvnic->nic_vbase = pci_alloc_consistent(nesdev->pcidev, nesvnic->nic_mem_size,
			&nesvnic->nic_pbase);
	if (!nesvnic->nic_vbase) {
		nes_debug(NES_DBG_INIT, "Unable to allocate memory for NIC host descriptor rings\n");
		return -ENOMEM;
	}
	memset(nesvnic->nic_vbase, 0, nesvnic->nic_mem_size);
	nes_debug(NES_DBG_INIT, "Allocated NIC QP structures at %p (phys = %016lX), size = %u.\n",
			nesvnic->nic_vbase, (unsigned long)nesvnic->nic_pbase, nesvnic->nic_mem_size);

	vmem = (void *)(((unsigned long)nesvnic->nic_vbase + (256 - 1)) &
			~(unsigned long)(256 - 1));
	pmem = (dma_addr_t)(((unsigned long long)nesvnic->nic_pbase + (256 - 1)) &
			~(unsigned long long)(256 - 1));

	/* Setup the first Fragment buffers */
	nesvnic->nic.first_frag_vbase = vmem;

	for (counter = 0; counter < NES_NIC_WQ_SIZE; counter++) {
		nesvnic->nic.frag_paddr[counter] = pmem;
		pmem += sizeof(struct nes_first_frag);
	}

	/* setup the SQ */
	vmem += (NES_NIC_WQ_SIZE * sizeof(struct nes_first_frag));

	nesvnic->nic.sq_vbase = (void *)vmem;
	nesvnic->nic.sq_pbase = pmem;
	nesvnic->nic.sq_head = 0;
	nesvnic->nic.sq_tail = 0;
	nesvnic->nic.sq_size = NES_NIC_WQ_SIZE;
	for (counter = 0; counter < NES_NIC_WQ_SIZE; counter++) {
		nic_sqe = &nesvnic->nic.sq_vbase[counter];
		nic_sqe->wqe_words[NES_NIC_SQ_WQE_MISC_IDX] =
				cpu_to_le32(NES_NIC_SQ_WQE_DISABLE_CHKSUM |
				NES_NIC_SQ_WQE_COMPLETION);
		nic_sqe->wqe_words[NES_NIC_SQ_WQE_LENGTH_0_TAG_IDX] =
				cpu_to_le32((u32)NES_FIRST_FRAG_SIZE << 16);
		nic_sqe->wqe_words[NES_NIC_SQ_WQE_FRAG0_LOW_IDX] =
				cpu_to_le32((u32)nesvnic->nic.frag_paddr[counter]);
		nic_sqe->wqe_words[NES_NIC_SQ_WQE_FRAG0_HIGH_IDX] =
				cpu_to_le32((u32)((u64)nesvnic->nic.frag_paddr[counter] >> 32));
	}

	nesvnic->get_cqp_request = nes_get_cqp_request;
	nesvnic->post_cqp_request = nes_post_cqp_request;
	nesvnic->mcrq_mcast_filter = NULL;

	spin_lock_init(&nesvnic->nic.rq_lock);

	/* setup the RQ */
	vmem += (NES_NIC_WQ_SIZE * sizeof(struct nes_hw_nic_sq_wqe));
	pmem += (NES_NIC_WQ_SIZE * sizeof(struct nes_hw_nic_sq_wqe));


	nesvnic->nic.rq_vbase = vmem;
	nesvnic->nic.rq_pbase = pmem;
	nesvnic->nic.rq_head = 0;
	nesvnic->nic.rq_tail = 0;
	nesvnic->nic.rq_size = NES_NIC_WQ_SIZE;

	/* setup the CQ */
	vmem += (NES_NIC_WQ_SIZE * sizeof(struct nes_hw_nic_rq_wqe));
	pmem += (NES_NIC_WQ_SIZE * sizeof(struct nes_hw_nic_rq_wqe));

	if (nesdev->nesadapter->netdev_count > 2)
		nesvnic->mcrq_qp_id = nesvnic->nic_index + 32;
	else
		nesvnic->mcrq_qp_id = nesvnic->nic.qp_id + 4;

	nesvnic->nic_cq.cq_vbase = vmem;
	nesvnic->nic_cq.cq_pbase = pmem;
	nesvnic->nic_cq.cq_head = 0;
	nesvnic->nic_cq.cq_size = NES_NIC_WQ_SIZE * 2;

	nesvnic->nic_cq.ce_handler = nes_nic_napi_ce_handler;

	/* Send CreateCQ request to CQP */
	spin_lock_irqsave(&nesdev->cqp.lock, flags);
	cqp_head = nesdev->cqp.sq_head;

	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];
	nes_fill_init_cqp_wqe(cqp_wqe, nesdev);

	cqp_wqe->wqe_words[NES_CQP_WQE_OPCODE_IDX] = cpu_to_le32(
			NES_CQP_CREATE_CQ | NES_CQP_CQ_CEQ_VALID |
			((u32)nesvnic->nic_cq.cq_size << 16));
	cqp_wqe->wqe_words[NES_CQP_WQE_ID_IDX] = cpu_to_le32(
			nesvnic->nic_cq.cq_number | ((u32)nesdev->nic_ceq_index << 16));
	u64temp = (u64)nesvnic->nic_cq.cq_pbase;
	set_wqe_64bit_value(cqp_wqe->wqe_words, NES_CQP_CQ_WQE_PBL_LOW_IDX, u64temp);
	cqp_wqe->wqe_words[NES_CQP_CQ_WQE_CQ_CONTEXT_HIGH_IDX] =  0;
	u64temp = (unsigned long)&nesvnic->nic_cq;
	cqp_wqe->wqe_words[NES_CQP_CQ_WQE_CQ_CONTEXT_LOW_IDX] =  cpu_to_le32((u32)(u64temp >> 1));
	cqp_wqe->wqe_words[NES_CQP_CQ_WQE_CQ_CONTEXT_HIGH_IDX] =
			cpu_to_le32(((u32)((u64temp) >> 33)) & 0x7FFFFFFF);
	cqp_wqe->wqe_words[NES_CQP_CQ_WQE_DOORBELL_INDEX_HIGH_IDX] = 0;
	if (++cqp_head >= nesdev->cqp.sq_size)
		cqp_head = 0;
	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];
	nes_fill_init_cqp_wqe(cqp_wqe, nesdev);

	/* Send CreateQP request to CQP */
	nic_context = (void *)(&nesvnic->nic_cq.cq_vbase[nesvnic->nic_cq.cq_size]);
	nic_context->context_words[NES_NIC_CTX_MISC_IDX] =
			cpu_to_le32((u32)NES_NIC_CTX_SIZE |
			((u32)PCI_FUNC(nesdev->pcidev->devfn) << 12));
	nes_debug(NES_DBG_INIT, "RX_WINDOW_BUFFER_PAGE_TABLE_SIZE = 0x%08X, RX_WINDOW_BUFFER_SIZE = 0x%08X\n",
			nes_read_indexed(nesdev, NES_IDX_RX_WINDOW_BUFFER_PAGE_TABLE_SIZE),
			nes_read_indexed(nesdev, NES_IDX_RX_WINDOW_BUFFER_SIZE));
	if (nes_read_indexed(nesdev, NES_IDX_RX_WINDOW_BUFFER_SIZE) != 0) {
		nic_context->context_words[NES_NIC_CTX_MISC_IDX] |= cpu_to_le32(NES_NIC_BACK_STORE);
	}

	u64temp = (u64)nesvnic->nic.sq_pbase;
	nic_context->context_words[NES_NIC_CTX_SQ_LOW_IDX]  = cpu_to_le32((u32)u64temp);
	nic_context->context_words[NES_NIC_CTX_SQ_HIGH_IDX] = cpu_to_le32((u32)(u64temp >> 32));
	u64temp = (u64)nesvnic->nic.rq_pbase;
	nic_context->context_words[NES_NIC_CTX_RQ_LOW_IDX]  = cpu_to_le32((u32)u64temp);
	nic_context->context_words[NES_NIC_CTX_RQ_HIGH_IDX] = cpu_to_le32((u32)(u64temp >> 32));

	cqp_wqe->wqe_words[NES_CQP_WQE_OPCODE_IDX] = cpu_to_le32(NES_CQP_CREATE_QP |
			NES_CQP_QP_TYPE_NIC);
	cqp_wqe->wqe_words[NES_CQP_WQE_ID_IDX] = cpu_to_le32(nesvnic->nic.qp_id);
	u64temp = (u64)nesvnic->nic_cq.cq_pbase +
			(nesvnic->nic_cq.cq_size * sizeof(struct nes_hw_nic_cqe));
	set_wqe_64bit_value(cqp_wqe->wqe_words, NES_CQP_QP_WQE_CONTEXT_LOW_IDX, u64temp);

	if (++cqp_head >= nesdev->cqp.sq_size)
		cqp_head = 0;
	nesdev->cqp.sq_head = cqp_head;

	barrier();

	/* Ring doorbell (2 WQEs) */
	nes_write32(nesdev->regs+NES_WQE_ALLOC, 0x02800000 | nesdev->cqp.qp_id);

	spin_unlock_irqrestore(&nesdev->cqp.lock, flags);
	nes_debug(NES_DBG_INIT, "Waiting for create NIC QP%u to complete.\n",
			nesvnic->nic.qp_id);

	ret = wait_event_timeout(nesdev->cqp.waitq, (nesdev->cqp.sq_tail == cqp_head),
			NES_EVENT_TIMEOUT);
	nes_debug(NES_DBG_INIT, "Create NIC QP%u completed, wait_event_timeout ret = %u.\n",
			nesvnic->nic.qp_id, ret);
	if (!ret) {
		nes_debug(NES_DBG_INIT, "NIC QP%u create timeout expired\n", nesvnic->nic.qp_id);
		pci_free_consistent(nesdev->pcidev, nesvnic->nic_mem_size, nesvnic->nic_vbase,
				nesvnic->nic_pbase);
		return -EIO;
	}

	/* Populate the RQ */
	for (counter = 0; counter < (NES_NIC_WQ_SIZE - 1); counter++) {
		skb = dev_alloc_skb(nesvnic->max_frame_size);
		if (!skb) {
			nes_debug(NES_DBG_INIT, "%s: out of memory for receive skb\n", netdev->name);

			nes_destroy_nic_qp(nesvnic);
			return -ENOMEM;
		}

		skb->dev = netdev;

		pmem = pci_map_single(nesdev->pcidev, skb->data,
				nesvnic->max_frame_size, PCI_DMA_FROMDEVICE);

		nic_rqe = &nesvnic->nic.rq_vbase[counter];
		nic_rqe->wqe_words[NES_NIC_RQ_WQE_LENGTH_1_0_IDX] = cpu_to_le32(nesvnic->max_frame_size);
		nic_rqe->wqe_words[NES_NIC_RQ_WQE_LENGTH_3_2_IDX] = 0;
		nic_rqe->wqe_words[NES_NIC_RQ_WQE_FRAG0_LOW_IDX]  = cpu_to_le32((u32)pmem);
		nic_rqe->wqe_words[NES_NIC_RQ_WQE_FRAG0_HIGH_IDX] = cpu_to_le32((u32)((u64)pmem >> 32));
		nesvnic->nic.rx_skb[counter] = skb;
	}

	wqe_count = NES_NIC_WQ_SIZE - 1;
	nesvnic->nic.rq_head = wqe_count;
	barrier();
	do {
		counter = min(wqe_count, ((u32)255));
		wqe_count -= counter;
		nes_write32(nesdev->regs+NES_WQE_ALLOC, (counter << 24) | nesvnic->nic.qp_id);
	} while (wqe_count);
	init_timer(&nesvnic->rq_wqes_timer);
	nesvnic->rq_wqes_timer.function = nes_rq_wqes_timeout;
	nesvnic->rq_wqes_timer.data = (unsigned long)nesvnic;
	nes_debug(NES_DBG_INIT, "NAPI support Enabled\n");
	if (nesdev->nesadapter->et_use_adaptive_rx_coalesce)
	{
		nes_nic_init_timer(nesdev);
		if (netdev->mtu > 1500)
			jumbomode = 1;
		nes_nic_init_timer_defaults(nesdev, jumbomode);
	}
	nesvnic->lro_mgr.max_aggr       = nes_lro_max_aggr;
	nesvnic->lro_mgr.max_desc       = NES_MAX_LRO_DESCRIPTORS;
	nesvnic->lro_mgr.lro_arr        = nesvnic->lro_desc;
	nesvnic->lro_mgr.get_skb_header = nes_lro_get_skb_hdr;
	nesvnic->lro_mgr.features       = LRO_F_NAPI | LRO_F_EXTRACT_VLAN_ID;
	nesvnic->lro_mgr.dev            = netdev;
	nesvnic->lro_mgr.ip_summed      = CHECKSUM_UNNECESSARY;
	nesvnic->lro_mgr.ip_summed_aggr = CHECKSUM_UNNECESSARY;
	return 0;
}


/**
 * nes_destroy_nic_qp
 */
void nes_destroy_nic_qp(struct nes_vnic *nesvnic)
{
	u64 u64temp;
	dma_addr_t bus_address;
	struct nes_device *nesdev = nesvnic->nesdev;
	struct nes_hw_cqp_wqe *cqp_wqe;
	struct nes_hw_nic_sq_wqe *nic_sqe;
	struct nes_hw_nic_rq_wqe *nic_rqe;
	__le16 *wqe_fragment_length;
	u16  wqe_fragment_index;
	u64 wqe_frag;
	u32 cqp_head;
	unsigned long flags;
	int ret;

	/* Free remaining NIC receive buffers */
	while (nesvnic->nic.rq_head != nesvnic->nic.rq_tail) {
		nic_rqe   = &nesvnic->nic.rq_vbase[nesvnic->nic.rq_tail];
		wqe_frag  = (u64)le32_to_cpu(
			nic_rqe->wqe_words[NES_NIC_RQ_WQE_FRAG0_LOW_IDX]);
		wqe_frag |= ((u64)le32_to_cpu(
			nic_rqe->wqe_words[NES_NIC_RQ_WQE_FRAG0_HIGH_IDX]))<<32;
		pci_unmap_single(nesdev->pcidev, (dma_addr_t)wqe_frag,
				nesvnic->max_frame_size, PCI_DMA_FROMDEVICE);
		dev_kfree_skb(nesvnic->nic.rx_skb[nesvnic->nic.rq_tail++]);
		nesvnic->nic.rq_tail &= (nesvnic->nic.rq_size - 1);
	}

	/* Free remaining NIC transmit buffers */
	while (nesvnic->nic.sq_head != nesvnic->nic.sq_tail) {
		nic_sqe = &nesvnic->nic.sq_vbase[nesvnic->nic.sq_tail];
		wqe_fragment_index = 1;
		wqe_fragment_length = (__le16 *)
			&nic_sqe->wqe_words[NES_NIC_SQ_WQE_LENGTH_0_TAG_IDX];
		/* bump past the vlan tag */
		wqe_fragment_length++;
		if (le16_to_cpu(wqe_fragment_length[wqe_fragment_index]) != 0) {
			u64temp = (u64)le32_to_cpu(
				nic_sqe->wqe_words[NES_NIC_SQ_WQE_FRAG0_LOW_IDX+
				wqe_fragment_index*2]);
			u64temp += ((u64)le32_to_cpu(
				nic_sqe->wqe_words[NES_NIC_SQ_WQE_FRAG0_HIGH_IDX
				+ wqe_fragment_index*2]))<<32;
			bus_address = (dma_addr_t)u64temp;
			if (test_and_clear_bit(nesvnic->nic.sq_tail,
					nesvnic->nic.first_frag_overflow)) {
				pci_unmap_single(nesdev->pcidev,
						bus_address,
						le16_to_cpu(wqe_fragment_length[
							wqe_fragment_index++]),
						PCI_DMA_TODEVICE);
			}
			for (; wqe_fragment_index < 5; wqe_fragment_index++) {
				if (wqe_fragment_length[wqe_fragment_index]) {
					u64temp = le32_to_cpu(
						nic_sqe->wqe_words[
						NES_NIC_SQ_WQE_FRAG0_LOW_IDX+
						wqe_fragment_index*2]);
					u64temp += ((u64)le32_to_cpu(
						nic_sqe->wqe_words[
						NES_NIC_SQ_WQE_FRAG0_HIGH_IDX+
						wqe_fragment_index*2]))<<32;
					bus_address = (dma_addr_t)u64temp;
					pci_unmap_page(nesdev->pcidev,
							bus_address,
							le16_to_cpu(
							wqe_fragment_length[
							wqe_fragment_index]),
							PCI_DMA_TODEVICE);
				} else
					break;
			}
		}
		if (nesvnic->nic.tx_skb[nesvnic->nic.sq_tail])
			dev_kfree_skb(
				nesvnic->nic.tx_skb[nesvnic->nic.sq_tail]);

		nesvnic->nic.sq_tail = (++nesvnic->nic.sq_tail)
					& (nesvnic->nic.sq_size - 1);
	}

	spin_lock_irqsave(&nesdev->cqp.lock, flags);

	/* Destroy NIC QP */
	cqp_head = nesdev->cqp.sq_head;
	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];
	nes_fill_init_cqp_wqe(cqp_wqe, nesdev);

	set_wqe_32bit_value(cqp_wqe->wqe_words, NES_CQP_WQE_OPCODE_IDX,
		(NES_CQP_DESTROY_QP | NES_CQP_QP_TYPE_NIC));
	set_wqe_32bit_value(cqp_wqe->wqe_words, NES_CQP_WQE_ID_IDX,
		nesvnic->nic.qp_id);

	if (++cqp_head >= nesdev->cqp.sq_size)
		cqp_head = 0;

	cqp_wqe = &nesdev->cqp.sq_vbase[cqp_head];

	/* Destroy NIC CQ */
	nes_fill_init_cqp_wqe(cqp_wqe, nesdev);
	set_wqe_32bit_value(cqp_wqe->wqe_words, NES_CQP_WQE_OPCODE_IDX,
		(NES_CQP_DESTROY_CQ | ((u32)nesvnic->nic_cq.cq_size << 16)));
	set_wqe_32bit_value(cqp_wqe->wqe_words, NES_CQP_WQE_ID_IDX,
		(nesvnic->nic_cq.cq_number | ((u32)nesdev->nic_ceq_index << 16)));

	if (++cqp_head >= nesdev->cqp.sq_size)
		cqp_head = 0;

	nesdev->cqp.sq_head = cqp_head;
	barrier();

	/* Ring doorbell (2 WQEs) */
	nes_write32(nesdev->regs+NES_WQE_ALLOC, 0x02800000 | nesdev->cqp.qp_id);

	spin_unlock_irqrestore(&nesdev->cqp.lock, flags);
	nes_debug(NES_DBG_SHUTDOWN, "Waiting for CQP, cqp_head=%u, cqp.sq_head=%u,"
			" cqp.sq_tail=%u, cqp.sq_size=%u\n",
			cqp_head, nesdev->cqp.sq_head,
			nesdev->cqp.sq_tail, nesdev->cqp.sq_size);

	ret = wait_event_timeout(nesdev->cqp.waitq, (nesdev->cqp.sq_tail == cqp_head),
			NES_EVENT_TIMEOUT);

	nes_debug(NES_DBG_SHUTDOWN, "Destroy NIC QP returned, wait_event_timeout ret = %u, cqp_head=%u,"
			" cqp.sq_head=%u, cqp.sq_tail=%u\n",
			ret, cqp_head, nesdev->cqp.sq_head, nesdev->cqp.sq_tail);
	if (!ret) {
		nes_debug(NES_DBG_SHUTDOWN, "NIC QP%u destroy timeout expired\n",
				nesvnic->nic.qp_id);
	}

	pci_free_consistent(nesdev->pcidev, nesvnic->nic_mem_size, nesvnic->nic_vbase,
			nesvnic->nic_pbase);
}

/**
 * nes_napi_isr
 */
int nes_napi_isr(struct nes_device *nesdev)
{
	struct nes_adapter *nesadapter = nesdev->nesadapter;
	u32 int_stat;

	if (nesdev->napi_isr_ran) {
		/* interrupt status has already been read in ISR */
		int_stat = nesdev->int_stat;
	} else {
		int_stat = nes_read32(nesdev->regs + NES_INT_STAT);
		nesdev->int_stat = int_stat;
		nesdev->napi_isr_ran = 1;
	}

	int_stat &= nesdev->int_req;
	/* iff NIC, process here, else wait for DPC */
	if ((int_stat) && ((int_stat & 0x0000ff00) == int_stat)) {
		nesdev->napi_isr_ran = 0;
		nes_write32(nesdev->regs + NES_INT_STAT,
			(int_stat &
			~(NES_INT_INTF | NES_INT_TIMER | NES_INT_MAC0 | NES_INT_MAC1 | NES_INT_MAC2 | NES_INT_MAC3)));

		/* Process the CEQs */
		nes_process_ceq(nesdev, &nesdev->nesadapter->ceq[nesdev->nic_ceq_index]);

		if (unlikely((((nesadapter->et_rx_coalesce_usecs_irq) &&
					(!nesadapter->et_use_adaptive_rx_coalesce)) ||
					((nesadapter->et_use_adaptive_rx_coalesce) &&
					 (nesdev->deepcq_count > nesadapter->et_pkt_rate_low))))) {
			if ((nesdev->int_req & NES_INT_TIMER) == 0) {
				/* Enable Periodic timer interrupts */
				nesdev->int_req |= NES_INT_TIMER;
				/* ack any pending periodic timer interrupts so we don't get an immediate interrupt */
				/* TODO: need to also ack other unused periodic timer values, get from nesadapter */
				nes_write32(nesdev->regs+NES_TIMER_STAT,
						nesdev->timer_int_req  | ~(nesdev->nesadapter->timer_int_req));
				nes_write32(nesdev->regs+NES_INTF_INT_MASK,
						~(nesdev->intf_int_req | NES_INTF_PERIODIC_TIMER));
			}

			if (unlikely(nesadapter->et_use_adaptive_rx_coalesce))
			{
				nes_nic_init_timer(nesdev);
			}
			/* Enable interrupts, except CEQs */
			nes_write32(nesdev->regs+NES_INT_MASK, 0x0000ffff | (~nesdev->int_req));
		} else {
			/* Enable interrupts, make sure timer is off */
			nesdev->int_req &= ~NES_INT_TIMER;
			nes_write32(nesdev->regs+NES_INTF_INT_MASK, ~(nesdev->intf_int_req));
			nes_write32(nesdev->regs+NES_INT_MASK, ~nesdev->int_req);
		}
		nesdev->deepcq_count = 0;
		return 1;
	} else {
		return 0;
	}
}

static void process_critical_error(struct nes_device *nesdev)
{
	u32 debug_error;
	u32 nes_idx_debug_error_masks0 = 0;
	u16 error_module = 0;

	debug_error = nes_read_indexed(nesdev, NES_IDX_DEBUG_ERROR_CONTROL_STATUS);
	printk(KERN_ERR PFX "Critical Error reported by device!!! 0x%02X\n",
			(u16)debug_error);
	nes_write_indexed(nesdev, NES_IDX_DEBUG_ERROR_CONTROL_STATUS,
			0x01010000 | (debug_error & 0x0000ffff));
	if (crit_err_count++ > 10)
		nes_write_indexed(nesdev, NES_IDX_DEBUG_ERROR_MASKS1, 1 << 0x17);
	error_module = (u16) (debug_error & 0x1F00) >> 8;
	if (++nesdev->nesadapter->crit_error_count[error_module-1] >=
			nes_max_critical_error_count) {
		printk(KERN_ERR PFX "Masking off critical error for module "
			"0x%02X\n", (u16)error_module);
		nes_idx_debug_error_masks0 = nes_read_indexed(nesdev,
			NES_IDX_DEBUG_ERROR_MASKS0);
		nes_write_indexed(nesdev, NES_IDX_DEBUG_ERROR_MASKS0,
			nes_idx_debug_error_masks0 | (1 << error_module));
	}
}
/**
 * nes_dpc
 */
void nes_dpc(unsigned long param)
{
	struct nes_device *nesdev = (struct nes_device *)param;
	struct nes_adapter *nesadapter = nesdev->nesadapter;
	u32 counter;
	u32 loop_counter = 0;
	u32 int_status_bit;
	u32 int_stat;
	u32 timer_stat;
	u32 temp_int_stat;
	u32 intf_int_stat;
	u32 processed_intf_int = 0;
	u16 processed_timer_int = 0;
	u16 completion_ints = 0;
	u16 timer_ints = 0;

	/* nes_debug(NES_DBG_ISR, "\n"); */

	do {
		timer_stat = 0;
		if (nesdev->napi_isr_ran) {
			nesdev->napi_isr_ran = 0;
			int_stat = nesdev->int_stat;
		} else
			int_stat = nes_read32(nesdev->regs+NES_INT_STAT);
		if (processed_intf_int != 0)
			int_stat &= nesdev->int_req & ~NES_INT_INTF;
		else
			int_stat &= nesdev->int_req;
		if (processed_timer_int == 0) {
			processed_timer_int = 1;
			if (int_stat & NES_INT_TIMER) {
				timer_stat = nes_read32(nesdev->regs + NES_TIMER_STAT);
				if ((timer_stat & nesdev->timer_int_req) == 0) {
					int_stat &= ~NES_INT_TIMER;
				}
			}
		} else {
			int_stat &= ~NES_INT_TIMER;
		}

		if (int_stat) {
			if (int_stat & ~(NES_INT_INTF | NES_INT_TIMER | NES_INT_MAC0|
					NES_INT_MAC1|NES_INT_MAC2 | NES_INT_MAC3)) {
				/* Ack the interrupts */
				nes_write32(nesdev->regs+NES_INT_STAT,
					(int_stat & ~(NES_INT_INTF | NES_INT_TIMER | NES_INT_MAC0|
					NES_INT_MAC1 | NES_INT_MAC2 | NES_INT_MAC3)));
			}

			temp_int_stat = int_stat;
			for (counter = 0, int_status_bit = 1; counter < 16; counter++) {
				if (int_stat & int_status_bit) {
					nes_process_ceq(nesdev, &nesadapter->ceq[counter]);
					temp_int_stat &= ~int_status_bit;
					completion_ints = 1;
				}
				if (!(temp_int_stat & 0x0000ffff))
					break;
				int_status_bit <<= 1;
			}

			/* Process the AEQ for this pci function */
			int_status_bit = 1 << (16 + PCI_FUNC(nesdev->pcidev->devfn));
			if (int_stat & int_status_bit) {
				nes_process_aeq(nesdev, &nesadapter->aeq[PCI_FUNC(nesdev->pcidev->devfn)]);
			}

			/* Process the MAC interrupt for this pci function */
			int_status_bit = 1 << (24 + nesdev->mac_index);
			if (int_stat & int_status_bit) {
				nes_process_mac_intr(nesdev, nesdev->mac_index);
			}

			if (int_stat & NES_INT_TIMER) {
				if (timer_stat & nesdev->timer_int_req) {
					nes_write32(nesdev->regs + NES_TIMER_STAT,
							(timer_stat & nesdev->timer_int_req) |
							~(nesdev->nesadapter->timer_int_req));
					timer_ints = 1;
				}
			}

			if (int_stat & NES_INT_INTF) {
				processed_intf_int = 1;
				intf_int_stat = nes_read32(nesdev->regs+NES_INTF_INT_STAT);
				intf_int_stat &= nesdev->intf_int_req;
				if (NES_INTF_INT_CRITERR & intf_int_stat) {
					process_critical_error(nesdev);
				}
				if (NES_INTF_INT_PCIERR & intf_int_stat) {
					printk(KERN_ERR PFX "PCI Error reported by device!!!\n");
					BUG();
				}
				if (NES_INTF_INT_AEQ_OFLOW & intf_int_stat) {
					printk(KERN_ERR PFX "AEQ Overflow reported by device!!!\n");
					BUG();
				}
				nes_write32(nesdev->regs+NES_INTF_INT_STAT, intf_int_stat);
			}

			if (int_stat & NES_INT_TSW) {
			}
		}
		/* Don't use the interface interrupt bit stay in loop */
		int_stat &= ~NES_INT_INTF | NES_INT_TIMER | NES_INT_MAC0 |
				NES_INT_MAC1 | NES_INT_MAC2 | NES_INT_MAC3;
	} while ((int_stat != 0) && (loop_counter++ < MAX_DPC_ITERATIONS));

	if (timer_ints == 1) {
		if ((nesadapter->et_rx_coalesce_usecs_irq) || (nesadapter->et_use_adaptive_rx_coalesce)) {
			if (completion_ints == 0) {
				nesdev->timer_only_int_count++;
				if (nesdev->timer_only_int_count>=nesadapter->timer_int_limit) {
					nesdev->timer_only_int_count = 0;
					nesdev->int_req &= ~NES_INT_TIMER;
					nes_write32(nesdev->regs + NES_INTF_INT_MASK, ~(nesdev->intf_int_req));
					nes_write32(nesdev->regs + NES_INT_MASK, ~nesdev->int_req);
				} else {
					nes_write32(nesdev->regs+NES_INT_MASK, 0x0000ffff | (~nesdev->int_req));
				}
			} else {
				if (unlikely(nesadapter->et_use_adaptive_rx_coalesce))
				{
					nes_nic_init_timer(nesdev);
				}
				nesdev->timer_only_int_count = 0;
				nes_write32(nesdev->regs+NES_INT_MASK, 0x0000ffff | (~nesdev->int_req));
			}
		} else {
			nesdev->timer_only_int_count = 0;
			nesdev->int_req &= ~NES_INT_TIMER;
			nes_write32(nesdev->regs+NES_INTF_INT_MASK, ~(nesdev->intf_int_req));
			nes_write32(nesdev->regs+NES_TIMER_STAT,
					nesdev->timer_int_req | ~(nesdev->nesadapter->timer_int_req));
			nes_write32(nesdev->regs+NES_INT_MASK, ~nesdev->int_req);
		}
	} else {
		if ( (completion_ints == 1) &&
			 (((nesadapter->et_rx_coalesce_usecs_irq) &&
			   (!nesadapter->et_use_adaptive_rx_coalesce)) ||
			  ((nesdev->deepcq_count > nesadapter->et_pkt_rate_low) &&
			   (nesadapter->et_use_adaptive_rx_coalesce) )) ) {
			/* nes_debug(NES_DBG_ISR, "Enabling periodic timer interrupt.\n" ); */
			nesdev->timer_only_int_count = 0;
			nesdev->int_req |= NES_INT_TIMER;
			nes_write32(nesdev->regs+NES_TIMER_STAT,
					nesdev->timer_int_req | ~(nesdev->nesadapter->timer_int_req));
			nes_write32(nesdev->regs+NES_INTF_INT_MASK,
					~(nesdev->intf_int_req | NES_INTF_PERIODIC_TIMER));
			nes_write32(nesdev->regs+NES_INT_MASK, 0x0000ffff | (~nesdev->int_req));
		} else {
			nes_write32(nesdev->regs+NES_INT_MASK, ~nesdev->int_req);
		}
	}
	nesdev->deepcq_count = 0;
}


/**
 * nes_process_ceq
 */
static void nes_process_ceq(struct nes_device *nesdev, struct nes_hw_ceq *ceq)
{
	u64 u64temp;
	struct nes_hw_cq *cq;
	u32 head;
	u32 ceq_size;

	/* nes_debug(NES_DBG_CQ, "\n"); */
	head = ceq->ceq_head;
	ceq_size = ceq->ceq_size;

	do {
		if (le32_to_cpu(ceq->ceq_vbase[head].ceqe_words[NES_CEQE_CQ_CTX_HIGH_IDX]) &
				NES_CEQE_VALID) {
			u64temp = (((u64)(le32_to_cpu(ceq->ceq_vbase[head].ceqe_words[NES_CEQE_CQ_CTX_HIGH_IDX]))) << 32) |
						((u64)(le32_to_cpu(ceq->ceq_vbase[head].ceqe_words[NES_CEQE_CQ_CTX_LOW_IDX])));
			u64temp <<= 1;
			cq = *((struct nes_hw_cq **)&u64temp);
			/* nes_debug(NES_DBG_CQ, "pCQ = %p\n", cq); */
			barrier();
			ceq->ceq_vbase[head].ceqe_words[NES_CEQE_CQ_CTX_HIGH_IDX] = 0;

			/* call the event handler */
			cq->ce_handler(nesdev, cq);

			if (++head >= ceq_size)
				head = 0;
		} else {
			break;
		}

	} while (1);

	ceq->ceq_head = head;
}


/**
 * nes_process_aeq
 */
static void nes_process_aeq(struct nes_device *nesdev, struct nes_hw_aeq *aeq)
{
	/* u64 u64temp; */
	u32 head;
	u32 aeq_size;
	u32 aeqe_misc;
	u32 aeqe_cq_id;
	struct nes_hw_aeqe volatile *aeqe;

	head = aeq->aeq_head;
	aeq_size = aeq->aeq_size;

	do {
		aeqe = &aeq->aeq_vbase[head];
		if ((le32_to_cpu(aeqe->aeqe_words[NES_AEQE_MISC_IDX]) & NES_AEQE_VALID) == 0)
			break;
		aeqe_misc  = le32_to_cpu(aeqe->aeqe_words[NES_AEQE_MISC_IDX]);
		aeqe_cq_id = le32_to_cpu(aeqe->aeqe_words[NES_AEQE_COMP_QP_CQ_ID_IDX]);
		if (aeqe_misc & (NES_AEQE_QP|NES_AEQE_CQ)) {
			if (aeqe_cq_id >= NES_FIRST_QPN) {
				/* dealing with an accelerated QP related AE */
				/*
				 * u64temp = (((u64)(le32_to_cpu(aeqe->aeqe_words[NES_AEQE_COMP_CTXT_HIGH_IDX]))) << 32) |
				 *	     ((u64)(le32_to_cpu(aeqe->aeqe_words[NES_AEQE_COMP_CTXT_LOW_IDX])));
				 */
				nes_process_iwarp_aeqe(nesdev, (struct nes_hw_aeqe *)aeqe);
			} else {
				/* TODO: dealing with a CQP related AE */
				nes_debug(NES_DBG_AEQ, "Processing CQP related AE, misc = 0x%04X\n",
						(u16)(aeqe_misc >> 16));
			}
		}

		aeqe->aeqe_words[NES_AEQE_MISC_IDX] = 0;

		if (++head >= aeq_size)
			head = 0;

		nes_write32(nesdev->regs + NES_AEQ_ALLOC, 1 << 16);
	}
	while (1);
	aeq->aeq_head = head;
}

static void nes_reset_link(struct nes_device *nesdev, u32 mac_index)
{
	struct nes_adapter *nesadapter = nesdev->nesadapter;
	u32 reset_value;
	u32 i=0;
	u32 u32temp;

	if (nesadapter->hw_rev == NE020_REV) {
		return;
	}
	mh_detected++;

	reset_value = nes_read32(nesdev->regs+NES_SOFTWARE_RESET);

	if ((mac_index == 0) || ((mac_index == 1) && (nesadapter->OneG_Mode)))
		reset_value |= 0x0000001d;
	else
		reset_value |= 0x0000002d;

	if (4 <= (nesadapter->link_interrupt_count[mac_index] / ((u16)NES_MAX_LINK_INTERRUPTS))) {
		if ((!nesadapter->OneG_Mode) && (nesadapter->port_count == 2)) {
			nesadapter->link_interrupt_count[0] = 0;
			nesadapter->link_interrupt_count[1] = 0;
			u32temp = nes_read_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1);
			if (0x00000040 & u32temp)
				nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1, 0x0000F088);
			else
				nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1, 0x0000F0C8);

			reset_value |= 0x0000003d;
		}
		nesadapter->link_interrupt_count[mac_index] = 0;
	}

	nes_write32(nesdev->regs+NES_SOFTWARE_RESET, reset_value);

	while (((nes_read32(nesdev->regs+NES_SOFTWARE_RESET)
			& 0x00000040) != 0x00000040) && (i++ < 5000));

	if (0x0000003d == (reset_value & 0x0000003d)) {
		u32 pcs_control_status0, pcs_control_status1;

		for (i = 0; i < 10; i++) {
			pcs_control_status0 = nes_read_indexed(nesdev, NES_IDX_PHY_PCS_CONTROL_STATUS0);
			pcs_control_status1 = nes_read_indexed(nesdev, NES_IDX_PHY_PCS_CONTROL_STATUS0 + 0x200);
			if (((0x0F000000 == (pcs_control_status0 & 0x0F000000))
			     && (pcs_control_status0 & 0x00100000))
			    || ((0x0F000000 == (pcs_control_status1 & 0x0F000000))
				&& (pcs_control_status1 & 0x00100000)))
				continue;
			else
				break;
		}
		if (10 == i) {
			u32temp = nes_read_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1);
			if (0x00000040 & u32temp)
				nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1, 0x0000F088);
			else
				nes_write_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_CONTROL1, 0x0000F0C8);

			nes_write32(nesdev->regs+NES_SOFTWARE_RESET, reset_value);

			while (((nes_read32(nesdev->regs + NES_SOFTWARE_RESET)
				 & 0x00000040) != 0x00000040) && (i++ < 5000));
		}
	}
}

/**
 * nes_process_mac_intr
 */
static void nes_process_mac_intr(struct nes_device *nesdev, u32 mac_number)
{
	unsigned long flags;
	u32 pcs_control_status;
	struct nes_adapter *nesadapter = nesdev->nesadapter;
	struct nes_vnic *nesvnic;
	u32 mac_status;
	u32 mac_index = nesdev->mac_index;
	u32 u32temp;
	u16 phy_data;
	u16 temp_phy_data;
	u32 pcs_val  = 0x0f0f0000;
	u32 pcs_mask = 0x0f1f0000;
	u32 cdr_ctrl;

	spin_lock_irqsave(&nesadapter->phy_lock, flags);
	if (nesadapter->mac_sw_state[mac_number] != NES_MAC_SW_IDLE) {
		spin_unlock_irqrestore(&nesadapter->phy_lock, flags);
		return;
	}
	nesadapter->mac_sw_state[mac_number] = NES_MAC_SW_INTERRUPT;
	spin_unlock_irqrestore(&nesadapter->phy_lock, flags);

	/* ack the MAC interrupt */
	mac_status = nes_read_indexed(nesdev, NES_IDX_MAC_INT_STATUS + (mac_index * 0x200));
	/* Clear the interrupt */
	nes_write_indexed(nesdev, NES_IDX_MAC_INT_STATUS + (mac_index * 0x200), mac_status);

	nes_debug(NES_DBG_PHY, "MAC%u interrupt status = 0x%X.\n", mac_number, mac_status);

	if (mac_status & (NES_MAC_INT_LINK_STAT_CHG | NES_MAC_INT_XGMII_EXT)) {
		nesdev->link_status_interrupts++;
		if (0 == (++nesadapter->link_interrupt_count[mac_index] % ((u16)NES_MAX_LINK_INTERRUPTS))) {
			spin_lock_irqsave(&nesadapter->phy_lock, flags);
			nes_reset_link(nesdev, mac_index);
			spin_unlock_irqrestore(&nesadapter->phy_lock, flags);
		}
		/* read the PHY interrupt status register */
		if ((nesadapter->OneG_Mode) &&
		(nesadapter->phy_type[mac_index] != NES_PHY_TYPE_PUMA_1G)) {
			do {
				nes_read_1G_phy_reg(nesdev, 0x1a,
						nesadapter->phy_index[mac_index], &phy_data);
				nes_debug(NES_DBG_PHY, "Phy%d data from register 0x1a = 0x%X.\n",
						nesadapter->phy_index[mac_index], phy_data);
			} while (phy_data&0x8000);

			temp_phy_data = 0;
			do {
				nes_read_1G_phy_reg(nesdev, 0x11,
						nesadapter->phy_index[mac_index], &phy_data);
				nes_debug(NES_DBG_PHY, "Phy%d data from register 0x11 = 0x%X.\n",
						nesadapter->phy_index[mac_index], phy_data);
				if (temp_phy_data == phy_data)
					break;
				temp_phy_data = phy_data;
			} while (1);

			nes_read_1G_phy_reg(nesdev, 0x1e,
					nesadapter->phy_index[mac_index], &phy_data);
			nes_debug(NES_DBG_PHY, "Phy%d data from register 0x1e = 0x%X.\n",
					nesadapter->phy_index[mac_index], phy_data);

			nes_read_1G_phy_reg(nesdev, 1,
					nesadapter->phy_index[mac_index], &phy_data);
			nes_debug(NES_DBG_PHY, "1G phy%u data from register 1 = 0x%X\n",
					nesadapter->phy_index[mac_index], phy_data);

			if (temp_phy_data & 0x1000) {
				nes_debug(NES_DBG_PHY, "The Link is up according to the PHY\n");
				phy_data = 4;
			} else {
				nes_debug(NES_DBG_PHY, "The Link is down according to the PHY\n");
			}
		}
		nes_debug(NES_DBG_PHY, "Eth SERDES Common Status: 0=0x%08X, 1=0x%08X\n",
				nes_read_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_STATUS0),
				nes_read_indexed(nesdev, NES_IDX_ETH_SERDES_COMMON_STATUS0+0x200));

		if (nesadapter->phy_type[mac_index] == NES_PHY_TYPE_PUMA_1G) {
			switch (mac_index) {
			case 1:
			case 3:
				pcs_control_status = nes_read_indexed(nesdev,
						NES_IDX_PHY_PCS_CONTROL_STATUS0 + 0x200);
				break;
			default:
				pcs_control_status = nes_read_indexed(nesdev,
						NES_IDX_PHY_PCS_CONTROL_STATUS0);
				break;
			}
		} else {
			pcs_control_status = nes_read_indexed(nesdev,
					NES_IDX_PHY_PCS_CONTROL_STATUS0 + ((mac_index & 1) * 0x200));
			pcs_control_status = nes_read_indexed(nesdev,
					NES_IDX_PHY_PCS_CONTROL_STATUS0 + ((mac_index & 1) * 0x200));
		}

		nes_debug(NES_DBG_PHY, "PCS PHY Control/Status%u: 0x%08X\n",
				mac_index, pcs_control_status);
		if ((nesadapter->OneG_Mode) &&
				(nesadapter->phy_type[mac_index] != NES_PHY_TYPE_PUMA_1G)) {
			u32temp = 0x01010000;
			if (nesadapter->port_count > 2) {
				u32temp |= 0x02020000;
			}
			if ((pcs_control_status & u32temp)!= u32temp) {
				phy_data = 0;
				nes_debug(NES_DBG_PHY, "PCS says the link is down\n");
			}
		} else {
			switch (nesadapter->phy_type[mac_index]) {
			case NES_PHY_TYPE_IRIS:
				nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 1, 1);
				temp_phy_data = (u16)nes_read_indexed(nesdev, NES_IDX_MAC_MDIO_CONTROL);
				u32temp = 20;
				do {
					nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 1, 1);
					phy_data = (u16)nes_read_indexed(nesdev, NES_IDX_MAC_MDIO_CONTROL);
					if ((phy_data == temp_phy_data) || (!(--u32temp)))
						break;
					temp_phy_data = phy_data;
				} while (1);
				nes_debug(NES_DBG_PHY, "%s: Phy data = 0x%04X, link was %s.\n",
					__func__, phy_data, nesadapter->mac_link_down[mac_index] ? "DOWN" : "UP");
				break;

			case NES_PHY_TYPE_ARGUS:
			case NES_PHY_TYPE_SFP_D:
				/* clear the alarms */
				nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 4, 0x0008);
				nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 4, 0xc001);
				nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 4, 0xc002);
				nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 4, 0xc005);
				nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 4, 0xc006);
				nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 1, 0x9003);
				nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 1, 0x9004);
				nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 1, 0x9005);
				/* check link status */
				nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 1, 0x9003);
				temp_phy_data = (u16)nes_read_indexed(nesdev, NES_IDX_MAC_MDIO_CONTROL);

				nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 3, 0x0021);
				nes_read_indexed(nesdev, NES_IDX_MAC_MDIO_CONTROL);
				nes_read_10G_phy_reg(nesdev, nesadapter->phy_index[mac_index], 3, 0x0021);
				phy_data = (u16)nes_read_indexed(nesdev, NES_IDX_MAC_MDIO_CONTROL);

				phy_data = (!temp_phy_data && (phy_data == 0x8000)) ? 0x4 : 0x0;

				nes_debug(NES_DBG_PHY, "%s: Phy data = 0x%04X, link was %s.\n",
					__func__, phy_data, nesadapter->mac_link_down[mac_index] ? "DOWN" : "UP");
				break;

			case NES_PHY_TYPE_PUMA_1G:
				if (mac_index < 2)
					pcs_val = pcs_mask = 0x01010000;
				else
					pcs_val = pcs_mask = 0x02020000;
				/* fall through */
			default:
				phy_data = (pcs_val == (pcs_control_status & pcs_mask)) ? 0x4 : 0x0;
				break;
			}
		}

		if (phy_data & 0x0004) {
			if (wide_ppm_offset &&
			    (nesadapter->phy_type[mac_index] == NES_PHY_TYPE_CX4) &&
			    (nesadapter->hw_rev != NE020_REV)) {
				cdr_ctrl = nes_read_indexed(nesdev,
							    NES_IDX_ETH_SERDES_CDR_CONTROL0 +
							    mac_index * 0x200);
				nes_write_indexed(nesdev,
						  NES_IDX_ETH_SERDES_CDR_CONTROL0 +
						  mac_index * 0x200,
						  cdr_ctrl | 0x000F0000);
			}
			nesadapter->mac_link_down[mac_index] = 0;
			list_for_each_entry(nesvnic, &nesadapter->nesvnic_list[mac_index], list) {
				nes_debug(NES_DBG_PHY, "The Link is UP!!.  linkup was %d\n",
						nesvnic->linkup);
				if (nesvnic->linkup == 0) {
					printk(PFX "The Link is now up for port %s, netdev %p.\n",
							nesvnic->netdev->name, nesvnic->netdev);
					if (netif_queue_stopped(nesvnic->netdev))
						netif_start_queue(nesvnic->netdev);
					nesvnic->linkup = 1;
					netif_carrier_on(nesvnic->netdev);
				}
			}
		} else {
			if (wide_ppm_offset &&
			    (nesadapter->phy_type[mac_index] == NES_PHY_TYPE_CX4) &&
			    (nesadapter->hw_rev != NE020_REV)) {
				cdr_ctrl = nes_read_indexed(nesdev,
							    NES_IDX_ETH_SERDES_CDR_CONTROL0 +
							    mac_index * 0x200);
				nes_write_indexed(nesdev,
						  NES_IDX_ETH_SERDES_CDR_CONTROL0 +
						  mac_index * 0x200,
						  cdr_ctrl & 0xFFF0FFFF);
			}
			nesadapter->mac_link_down[mac_index] = 1;
			list_for_each_entry(nesvnic, &nesadapter->nesvnic_list[mac_index], list) {
				nes_debug(NES_DBG_PHY, "The Link is Down!!. linkup was %d\n",
						nesvnic->linkup);
				if (nesvnic->linkup == 1) {
					printk(PFX "The Link is now down for port %s, netdev %p.\n",
							nesvnic->netdev->name, nesvnic->netdev);
					if (!(netif_queue_stopped(nesvnic->netdev)))
						netif_stop_queue(nesvnic->netdev);
					nesvnic->linkup = 0;
					netif_carrier_off(nesvnic->netdev);
				}
			}
		}
	}

	nesadapter->mac_sw_state[mac_number] = NES_MAC_SW_IDLE;
}



static void nes_nic_napi_ce_handler(struct nes_device *nesdev, struct nes_hw_nic_cq *cq)
{
	struct nes_vnic *nesvnic = container_of(cq, struct nes_vnic, nic_cq);

	napi_schedule(&nesvnic->napi);
}


/* The MAX_RQES_TO_PROCESS defines how many max read requests to complete before
* getting out of nic_ce_handler
*/
#define	MAX_RQES_TO_PROCESS	384

/**
 * nes_nic_ce_handler
 */
void nes_nic_ce_handler(struct nes_device *nesdev, struct nes_hw_nic_cq *cq)
{
	u64 u64temp;
	dma_addr_t bus_address;
	struct nes_hw_nic *nesnic;
	struct nes_vnic *nesvnic = container_of(cq, struct nes_vnic, nic_cq);
	struct nes_adapter *nesadapter = nesdev->nesadapter;
	struct nes_hw_nic_rq_wqe *nic_rqe;
	struct nes_hw_nic_sq_wqe *nic_sqe;
	struct sk_buff *skb;
	struct sk_buff *rx_skb;
	__le16 *wqe_fragment_length;
	u32 head;
	u32 cq_size;
	u32 rx_pkt_size;
	u32 cqe_count=0;
	u32 cqe_errv;
	u32 cqe_misc;
	u16 wqe_fragment_index = 1;	/* first fragment (0) is used by copy buffer */
	u16 vlan_tag;
	u16 pkt_type;
	u16 rqes_processed = 0;
	u8 sq_cqes = 0;
	u8 nes_use_lro = 0;

	head = cq->cq_head;
	cq_size = cq->cq_size;
	cq->cqes_pending = 1;
	if (nesvnic->netdev->features & NETIF_F_LRO)
		nes_use_lro = 1;
	do {
		if (le32_to_cpu(cq->cq_vbase[head].cqe_words[NES_NIC_CQE_MISC_IDX]) &
				NES_NIC_CQE_VALID) {
			nesnic = &nesvnic->nic;
			cqe_misc = le32_to_cpu(cq->cq_vbase[head].cqe_words[NES_NIC_CQE_MISC_IDX]);
			if (cqe_misc & NES_NIC_CQE_SQ) {
				sq_cqes++;
				wqe_fragment_index = 1;
				nic_sqe = &nesnic->sq_vbase[nesnic->sq_tail];
				skb = nesnic->tx_skb[nesnic->sq_tail];
				wqe_fragment_length = (__le16 *)&nic_sqe->wqe_words[NES_NIC_SQ_WQE_LENGTH_0_TAG_IDX];
				/* bump past the vlan tag */
				wqe_fragment_length++;
				if (le16_to_cpu(wqe_fragment_length[wqe_fragment_index]) != 0) {
					u64temp = (u64) le32_to_cpu(nic_sqe->wqe_words[NES_NIC_SQ_WQE_FRAG0_LOW_IDX +
							wqe_fragment_index * 2]);
					u64temp += ((u64)le32_to_cpu(nic_sqe->wqe_words[NES_NIC_SQ_WQE_FRAG0_HIGH_IDX +
							wqe_fragment_index * 2])) << 32;
					bus_address = (dma_addr_t)u64temp;
					if (test_and_clear_bit(nesnic->sq_tail, nesnic->first_frag_overflow)) {
						pci_unmap_single(nesdev->pcidev,
								bus_address,
								le16_to_cpu(wqe_fragment_length[wqe_fragment_index++]),
								PCI_DMA_TODEVICE);
					}
					for (; wqe_fragment_index < 5; wqe_fragment_index++) {
						if (wqe_fragment_length[wqe_fragment_index]) {
							u64temp = le32_to_cpu(nic_sqe->wqe_words[NES_NIC_SQ_WQE_FRAG0_LOW_IDX +
										wqe_fragment_index * 2]);
							u64temp += ((u64)le32_to_cpu(nic_sqe->wqe_words[NES_NIC_SQ_WQE_FRAG0_HIGH_IDX
										+ wqe_fragment_index * 2])) <<32;
							bus_address = (dma_addr_t)u64temp;
							pci_unmap_page(nesdev->pcidev,
									bus_address,
									le16_to_cpu(wqe_fragment_length[wqe_fragment_index]),
									PCI_DMA_TODEVICE);
						} else
							break;
					}
				}
				if (skb)
					dev_kfree_skb_any(skb);
				nesnic->sq_tail++;
				nesnic->sq_tail &= nesnic->sq_size-1;
				if (sq_cqes > 128) {
					barrier();
				/* restart the queue if it had been stopped */
				if (netif_queue_stopped(nesvnic->netdev))
					netif_wake_queue(nesvnic->netdev);
					sq_cqes = 0;
				}
			} else {
				rqes_processed ++;

				cq->rx_cqes_completed++;
				cq->rx_pkts_indicated++;
				rx_pkt_size = cqe_misc & 0x0000ffff;
				nic_rqe = &nesnic->rq_vbase[nesnic->rq_tail];
				/* Get the skb */
				rx_skb = nesnic->rx_skb[nesnic->rq_tail];
				nic_rqe = &nesnic->rq_vbase[nesvnic->nic.rq_tail];
				bus_address = (dma_addr_t)le32_to_cpu(nic_rqe->wqe_words[NES_NIC_RQ_WQE_FRAG0_LOW_IDX]);
				bus_address += ((u64)le32_to_cpu(nic_rqe->wqe_words[NES_NIC_RQ_WQE_FRAG0_HIGH_IDX])) << 32;
				pci_unmap_single(nesdev->pcidev, bus_address,
						nesvnic->max_frame_size, PCI_DMA_FROMDEVICE);
				/* rx_skb->tail = rx_skb->data + rx_pkt_size; */
				/* rx_skb->len = rx_pkt_size; */
				rx_skb->len = 0;  /* TODO: see if this is necessary */
				skb_put(rx_skb, rx_pkt_size);
				rx_skb->protocol = eth_type_trans(rx_skb, nesvnic->netdev);
				nesnic->rq_tail++;
				nesnic->rq_tail &= nesnic->rq_size - 1;

				atomic_inc(&nesvnic->rx_skbs_needed);
				if (atomic_read(&nesvnic->rx_skbs_needed) > (nesvnic->nic.rq_size>>1)) {
					nes_write32(nesdev->regs+NES_CQE_ALLOC,
							cq->cq_number | (cqe_count << 16));
					/* nesadapter->tune_timer.cq_count += cqe_count; */
					nesdev->currcq_count += cqe_count;
					cqe_count = 0;
					nes_replenish_nic_rq(nesvnic);
				}
				pkt_type = (u16)(le32_to_cpu(cq->cq_vbase[head].cqe_words[NES_NIC_CQE_TAG_PKT_TYPE_IDX]));
				cqe_errv = (cqe_misc & NES_NIC_CQE_ERRV_MASK) >> NES_NIC_CQE_ERRV_SHIFT;
				rx_skb->ip_summed = CHECKSUM_NONE;

				if ((NES_PKT_TYPE_TCPV4_BITS == (pkt_type & NES_PKT_TYPE_TCPV4_MASK)) ||
						(NES_PKT_TYPE_UDPV4_BITS == (pkt_type & NES_PKT_TYPE_UDPV4_MASK))) {
					if ((cqe_errv &
							(NES_NIC_ERRV_BITS_IPV4_CSUM_ERR | NES_NIC_ERRV_BITS_TCPUDP_CSUM_ERR |
							NES_NIC_ERRV_BITS_IPH_ERR | NES_NIC_ERRV_BITS_WQE_OVERRUN)) == 0) {
						if (nesvnic->rx_checksum_disabled == 0) {
							rx_skb->ip_summed = CHECKSUM_UNNECESSARY;
						}
					} else
						nes_debug(NES_DBG_CQ, "%s: unsuccessfully checksummed TCP or UDP packet."
								" errv = 0x%X, pkt_type = 0x%X.\n",
								nesvnic->netdev->name, cqe_errv, pkt_type);

				} else if ((pkt_type & NES_PKT_TYPE_IPV4_MASK) == NES_PKT_TYPE_IPV4_BITS) {
					if ((cqe_errv &
							(NES_NIC_ERRV_BITS_IPV4_CSUM_ERR | NES_NIC_ERRV_BITS_IPH_ERR |
							NES_NIC_ERRV_BITS_WQE_OVERRUN)) == 0) {
						if (nesvnic->rx_checksum_disabled == 0) {
							rx_skb->ip_summed = CHECKSUM_UNNECESSARY;
							/* nes_debug(NES_DBG_CQ, "%s: Reporting successfully checksummed IPv4 packet.\n",
								  nesvnic->netdev->name); */
						}
					} else
						nes_debug(NES_DBG_CQ, "%s: unsuccessfully checksummed TCP or UDP packet."
								" errv = 0x%X, pkt_type = 0x%X.\n",
								nesvnic->netdev->name, cqe_errv, pkt_type);
					}
				/* nes_debug(NES_DBG_CQ, "pkt_type=%x, APBVT_MASK=%x\n",
							pkt_type, (pkt_type & NES_PKT_TYPE_APBVT_MASK)); */

				if ((pkt_type & NES_PKT_TYPE_APBVT_MASK) == NES_PKT_TYPE_APBVT_BITS) {
					if (nes_cm_recv(rx_skb, nesvnic->netdev))
						rx_skb = NULL;
				}
				if (rx_skb == NULL)
					goto skip_rx_indicate0;


				if ((cqe_misc & NES_NIC_CQE_TAG_VALID) &&
				    (nesvnic->vlan_grp != NULL)) {
					vlan_tag = (u16)(le32_to_cpu(
							cq->cq_vbase[head].cqe_words[NES_NIC_CQE_TAG_PKT_TYPE_IDX])
							>> 16);
					nes_debug(NES_DBG_CQ, "%s: Reporting stripped VLAN packet. Tag = 0x%04X\n",
							nesvnic->netdev->name, vlan_tag);
					if (nes_use_lro)
						lro_vlan_hwaccel_receive_skb(&nesvnic->lro_mgr, rx_skb,
								nesvnic->vlan_grp, vlan_tag, NULL);
					else
						nes_vlan_rx(rx_skb, nesvnic->vlan_grp, vlan_tag);
				} else {
					if (nes_use_lro)
						lro_receive_skb(&nesvnic->lro_mgr, rx_skb, NULL);
					else
						nes_netif_rx(rx_skb);
				}

skip_rx_indicate0:
				;
				/* nesvnic->netstats.rx_packets++; */
				/* nesvnic->netstats.rx_bytes += rx_pkt_size; */
			}

			cq->cq_vbase[head].cqe_words[NES_NIC_CQE_MISC_IDX] = 0;
			/* Accounting... */
			cqe_count++;
			if (++head >= cq_size)
				head = 0;
			if (cqe_count == 255) {
				/* Replenish Nic CQ */
				nes_write32(nesdev->regs+NES_CQE_ALLOC,
						cq->cq_number | (cqe_count << 16));
				/* nesdev->nesadapter->tune_timer.cq_count += cqe_count; */
				nesdev->currcq_count += cqe_count;
				cqe_count = 0;
			}

			if (cq->rx_cqes_completed >= nesvnic->budget)
				break;
		} else {
			cq->cqes_pending = 0;
			break;
		}

	} while (1);

	if (nes_use_lro)
		lro_flush_all(&nesvnic->lro_mgr);
	if (sq_cqes) {
		barrier();
		/* restart the queue if it had been stopped */
		if (netif_queue_stopped(nesvnic->netdev))
			netif_wake_queue(nesvnic->netdev);
	}
	cq->cq_head = head;
	/* nes_debug(NES_DBG_CQ, "CQ%u Processed = %u cqes, new head = %u.\n",
			cq->cq_number, cqe_count, cq->cq_head); */
	cq->cqe_allocs_pending = cqe_count;
	if (unlikely(nesadapter->et_use_adaptive_rx_coalesce))
	{
		/* nesdev->nesadapter->tune_timer.cq_count += cqe_count; */
		nesdev->currcq_count += cqe_count;
		nes_nic_tune_timer(nesdev);
	}
	if (atomic_read(&nesvnic->rx_skbs_needed))
		nes_replenish_nic_rq(nesvnic);
}


/**
 * nes_cqp_ce_handler
 */
static void nes_cqp_ce_handler(struct nes_device *nesdev, struct nes_hw_cq *cq)
{
	u64 u64temp;
	unsigned long flags;
	struct nes_hw_cqp *cqp = NULL;
	struct nes_cqp_request *cqp_request;
	struct nes_hw_cqp_wqe *cqp_wqe;
	u32 head;
	u32 cq_size;
	u32 cqe_count=0;
	u32 error_code;
	/* u32 counter; */

	head = cq->cq_head;
	cq_size = cq->cq_size;

	do {
		/* process the CQE */
		/* nes_debug(NES_DBG_CQP, "head=%u cqe_words=%08X\n", head,
			  le32_to_cpu(cq->cq_vbase[head].cqe_words[NES_CQE_OPCODE_IDX])); */

		if (le32_to_cpu(cq->cq_vbase[head].cqe_words[NES_CQE_OPCODE_IDX]) & NES_CQE_VALID) {
			u64temp = (((u64)(le32_to_cpu(cq->cq_vbase[head].
					cqe_words[NES_CQE_COMP_COMP_CTX_HIGH_IDX]))) << 32) |
					((u64)(le32_to_cpu(cq->cq_vbase[head].
					cqe_words[NES_CQE_COMP_COMP_CTX_LOW_IDX])));
			cqp = *((struct nes_hw_cqp **)&u64temp);

			error_code = le32_to_cpu(cq->cq_vbase[head].cqe_words[NES_CQE_ERROR_CODE_IDX]);
			if (error_code) {
				nes_debug(NES_DBG_CQP, "Bad Completion code for opcode 0x%02X from CQP,"
						" Major/Minor codes = 0x%04X:%04X.\n",
						le32_to_cpu(cq->cq_vbase[head].cqe_words[NES_CQE_OPCODE_IDX])&0x3f,
						(u16)(error_code >> 16),
						(u16)error_code);
				nes_debug(NES_DBG_CQP, "cqp: qp_id=%u, sq_head=%u, sq_tail=%u\n",
						cqp->qp_id, cqp->sq_head, cqp->sq_tail);
			}

			u64temp = (((u64)(le32_to_cpu(nesdev->cqp.sq_vbase[cqp->sq_tail].
					wqe_words[NES_CQP_WQE_COMP_SCRATCH_HIGH_IDX]))) << 32) |
					((u64)(le32_to_cpu(nesdev->cqp.sq_vbase[cqp->sq_tail].
					wqe_words[NES_CQP_WQE_COMP_SCRATCH_LOW_IDX])));
			cqp_request = *((struct nes_cqp_request **)&u64temp);
			if (cqp_request) {
				if (cqp_request->waiting) {
					/* nes_debug(NES_DBG_CQP, "%s: Waking up requestor\n"); */
					cqp_request->major_code = (u16)(error_code >> 16);
					cqp_request->minor_code = (u16)error_code;
					barrier();
					cqp_request->request_done = 1;
					wake_up(&cqp_request->waitq);
					nes_put_cqp_request(nesdev, cqp_request);
				} else {
					if (cqp_request->callback)
						cqp_request->cqp_callback(nesdev, cqp_request);
					nes_free_cqp_request(nesdev, cqp_request);
				}
			} else {
				wake_up(&nesdev->cqp.waitq);
			}

			cq->cq_vbase[head].cqe_words[NES_CQE_OPCODE_IDX] = 0;
			nes_write32(nesdev->regs + NES_CQE_ALLOC, cq->cq_number | (1 << 16));
			if (++cqp->sq_tail >= cqp->sq_size)
				cqp->sq_tail = 0;

			/* Accounting... */
			cqe_count++;
			if (++head >= cq_size)
				head = 0;
		} else {
			break;
		}
	} while (1);
	cq->cq_head = head;

	spin_lock_irqsave(&nesdev->cqp.lock, flags);
	while ((!list_empty(&nesdev->cqp_pending_reqs)) &&
			((((nesdev->cqp.sq_tail+nesdev->cqp.sq_size)-nesdev->cqp.sq_head) &
			(nesdev->cqp.sq_size - 1)) != 1)) {
		cqp_request = list_entry(nesdev->cqp_pending_reqs.next,
				struct nes_cqp_request, list);
		list_del_init(&cqp_request->list);
		head = nesdev->cqp.sq_head++;
		nesdev->cqp.sq_head &= nesdev->cqp.sq_size-1;
		cqp_wqe = &nesdev->cqp.sq_vbase[head];
		memcpy(cqp_wqe, &cqp_request->cqp_wqe, sizeof(*cqp_wqe));
		barrier();
		cqp_wqe->wqe_words[NES_CQP_WQE_COMP_SCRATCH_LOW_IDX] =
			cpu_to_le32((u32)((unsigned long)cqp_request));
		cqp_wqe->wqe_words[NES_CQP_WQE_COMP_SCRATCH_HIGH_IDX] =
			cpu_to_le32((u32)(upper_32_bits((unsigned long)cqp_request)));
		nes_debug(NES_DBG_CQP, "CQP request %p (opcode 0x%02X) put on CQPs SQ wqe%u.\n",
				cqp_request, le32_to_cpu(cqp_wqe->wqe_words[NES_CQP_WQE_OPCODE_IDX])&0x3f, head);
		/* Ring doorbell (1 WQEs) */
		barrier();
		nes_write32(nesdev->regs+NES_WQE_ALLOC, 0x01800000 | nesdev->cqp.qp_id);
	}
	spin_unlock_irqrestore(&nesdev->cqp.lock, flags);

	/* Arm the CCQ */
	nes_write32(nesdev->regs+NES_CQE_ALLOC, NES_CQE_ALLOC_NOTIFY_NEXT |
			cq->cq_number);
	nes_read32(nesdev->regs+NES_CQE_ALLOC);
}


static u8 *locate_mpa(u8 *pkt, u32 aeq_info)
{
	u16 pkt_len;

	if (aeq_info & NES_AEQE_Q2_DATA_ETHERNET) {
		/* skip over ethernet header */
		pkt_len = be16_to_cpu(*(u16 *)(pkt + ETH_HLEN - 2));
		pkt += ETH_HLEN;

		/* Skip over IP and TCP headers */
		pkt += 4 * (pkt[0] & 0x0f);
		pkt += 4 * ((pkt[12] >> 4) & 0x0f);
	}
	return pkt;
}

/* Determine if incoming error pkt is rdma layer */
static u32 iwarp_opcode(struct nes_qp *nesqp, u32 aeq_info)
{
	u8 *pkt;
	u16 *mpa;
	u32 opcode = 0xffffffff;

	if (aeq_info & NES_AEQE_Q2_DATA_WRITTEN) {
		pkt = nesqp->hwqp.q2_vbase + BAD_FRAME_OFFSET;
		mpa = (u16 *)locate_mpa(pkt, aeq_info);
		opcode = be16_to_cpu(mpa[1]) & 0xf;
	}

	return opcode;
}

/* Build iWARP terminate header */
static int nes_bld_terminate_hdr(struct nes_qp *nesqp, u16 async_event_id, u32 aeq_info)
{
	u8 *pkt = nesqp->hwqp.q2_vbase + BAD_FRAME_OFFSET;
	u16 ddp_seg_len;
	int copy_len = 0;
	u8 is_tagged = 0;
	u8 flush_code = 0;
	struct nes_terminate_hdr *termhdr;

	termhdr = (struct nes_terminate_hdr *)nesqp->hwqp.q2_vbase;
	memset(termhdr, 0, 64);

	if (aeq_info & NES_AEQE_Q2_DATA_WRITTEN) {

		/* Use data from offending packet to fill in ddp & rdma hdrs */
		pkt = locate_mpa(pkt, aeq_info);
		ddp_seg_len = be16_to_cpu(*(u16 *)pkt);
		if (ddp_seg_len) {
			copy_len = 2;
			termhdr->hdrct = DDP_LEN_FLAG;
			if (pkt[2] & 0x80) {
				is_tagged = 1;
				if (ddp_seg_len >= TERM_DDP_LEN_TAGGED) {
					copy_len += TERM_DDP_LEN_TAGGED;
					termhdr->hdrct |= DDP_HDR_FLAG;
				}
			} else {
				if (ddp_seg_len >= TERM_DDP_LEN_UNTAGGED) {
					copy_len += TERM_DDP_LEN_UNTAGGED;
					termhdr->hdrct |= DDP_HDR_FLAG;
				}

				if (ddp_seg_len >= (TERM_DDP_LEN_UNTAGGED + TERM_RDMA_LEN)) {
					if ((pkt[3] & RDMA_OPCODE_MASK) == RDMA_READ_REQ_OPCODE) {
						copy_len += TERM_RDMA_LEN;
						termhdr->hdrct |= RDMA_HDR_FLAG;
					}
				}
			}
		}
	}

	switch (async_event_id) {
	case NES_AEQE_AEID_AMP_UNALLOCATED_STAG:
		switch (iwarp_opcode(nesqp, aeq_info)) {
		case IWARP_OPCODE_WRITE:
			flush_code = IB_WC_LOC_PROT_ERR;
			termhdr->layer_etype = (LAYER_DDP << 4) | DDP_TAGGED_BUFFER;
			termhdr->error_code = DDP_TAGGED_INV_STAG;
			break;
		default:
			flush_code = IB_WC_REM_ACCESS_ERR;
			termhdr->layer_etype = (LAYER_RDMA << 4) | RDMAP_REMOTE_PROT;
			termhdr->error_code = RDMAP_INV_STAG;
		}
		break;
	case NES_AEQE_AEID_AMP_INVALID_STAG:
		flush_code = IB_WC_REM_ACCESS_ERR;
		termhdr->layer_etype = (LAYER_RDMA << 4) | RDMAP_REMOTE_PROT;
		termhdr->error_code = RDMAP_INV_STAG;
		break;
	case NES_AEQE_AEID_AMP_BAD_QP:
		flush_code = IB_WC_LOC_QP_OP_ERR;
		termhdr->layer_etype = (LAYER_DDP << 4) | DDP_UNTAGGED_BUFFER;
		termhdr->error_code = DDP_UNTAGGED_INV_QN;
		break;
	case NES_AEQE_AEID_AMP_BAD_STAG_KEY:
	case NES_AEQE_AEID_AMP_BAD_STAG_INDEX:
		switch (iwarp_opcode(nesqp, aeq_info)) {
		case IWARP_OPCODE_SEND_INV:
		case IWARP_OPCODE_SEND_SE_INV:
			flush_code = IB_WC_REM_OP_ERR;
			termhdr->layer_etype = (LAYER_RDMA << 4) | RDMAP_REMOTE_OP;
			termhdr->error_code = RDMAP_CANT_INV_STAG;
			break;
		default:
			flush_code = IB_WC_REM_ACCESS_ERR;
			termhdr->layer_etype = (LAYER_RDMA << 4) | RDMAP_REMOTE_PROT;
			termhdr->error_code = RDMAP_INV_STAG;
		}
		break;
	case NES_AEQE_AEID_AMP_BOUNDS_VIOLATION:
		if (aeq_info & (NES_AEQE_Q2_DATA_ETHERNET | NES_AEQE_Q2_DATA_MPA)) {
			flush_code = IB_WC_LOC_PROT_ERR;
			termhdr->layer_etype = (LAYER_DDP << 4) | DDP_TAGGED_BUFFER;
			termhdr->error_code = DDP_TAGGED_BOUNDS;
		} else {
			flush_code = IB_WC_REM_ACCESS_ERR;
			termhdr->layer_etype = (LAYER_RDMA << 4) | RDMAP_REMOTE_PROT;
			termhdr->error_code = RDMAP_INV_BOUNDS;
		}
		break;
	case NES_AEQE_AEID_AMP_RIGHTS_VIOLATION:
	case NES_AEQE_AEID_AMP_INVALIDATE_NO_REMOTE_ACCESS_RIGHTS:
	case NES_AEQE_AEID_PRIV_OPERATION_DENIED:
		flush_code = IB_WC_REM_ACCESS_ERR;
		termhdr->layer_etype = (LAYER_RDMA << 4) | ts rPopyrOTE_PROT06 - 2009 Interror_cod.  A *
 * ght (cre ibreak;
	case NES_AEQE_AEID_AMP_TO_WRAP:
		flush you und a cC Thiihoice )s ave is availabll-NE, Inc undll  terms reserved.
er a cThis software is available tocensed uer a c chooseof one of two a clicenses.  You mayBAD_PDYINGswitch (iwarp_opense(nesqp, aeq_info)) {
	sourceIWARP_OPCODE_WRITEYING bee tree, d the matLOCfrom f the GNNU
 * General Public License (GDDPVersion 2DDP_TAGGED_BUFFEt modification,OPYING in thefollowing
 UNASSOC_STAGcondif this sn and use in sourceSEND_INVYINGmust retain the above
 SE * st of y forms, with or
 *he  200sthis he GNNU
 * General Public License (GPL) Version 2, available from the  file
 * COPYING in the main CANTlistong
 * the ab code defaultthiscondis arsmust         - Redisist of c disclaimer 2, avaiation- Redistribu the fin binary form   copyrproduce notice, thidisclaimecoother materce, t}y of this source tree, or the
LLailaCEIVED_MARKER_AND_LENGTH_FIELDS_DONT_MATCHYINGy forms, with or
 *atioLENf the GNU
 * General Public License (GMP Version 2    LLory o the fore met:cumentatMPAKIND,
ARE of this source tree, or the
ANTY OF ANY KIPA_CRCf thORNG BUT NOT LIMITED TO TGENERALANTIES OFbutiMERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE ANDbutiNONINFRINGEMENT. CRCEVENT SHALL THE AUTHORS OR COPYRIGHSEGM, O_TOO_LARGls
 NNECTION WITHONNECSOFTWARETH TNNECUSMALLNG BUT NOT LIMITED TORE.
 WARRER LIABILITY, WHETHER IN AN
 * ACTION elowiatowing
 * CATASTROPHIFTH TE AND
 * NONINFRINGEMEcludh>
#includeONNECTLEVENT SHALL THE AUTHORS OR COPY TORTCE<linuxe <linux/ip.h OTHER DEALINGS INRE.
    NO_L_BITNG BUT NOT LIMITED TO TFATux/inLI IN AN
 *WHEux/inIN ANbutiA DEALIincludnetdevice. <linux/ip.h#includether_lro_max_aggr, uint, 0444)e <l_aggr, uint, 0444)tcggr, "NIC LRO max pacINVALID_MSN_GAtati widncludenet_lrogr, linux/ip.tic intffseeRANGE_IS_NOT_set, iOF CONY CLAIM, DAMAGIABIRinux/inax_aggr = NES_LRO_MAX_AGGR;
module_linux/netdevice.UN       disclas anux/etherdevice.h>
#include 1=300ppmht nnt, 0644) LRO max packet aggregation");

staUBEx/ip.MESSAG_LRO USONG_FOR_AVAILABL list ofaggr, uint, 0444)modulmax_aggr, uint, 0444)4;
u32paramgr, "NIC LRO max pacnes_lro_maint_mod_t");

spm_oc u32 crit_ero yount;
 u8 int_mod_the derror_corror_countcq_depth_256al_error_countcq_ffset, i    VERSIOet;
SC(wide_ppm_offset, "Increase CX4 iif (is_taggeder matitions are
 * Npermitted providlinux/netdevice.=300ppm");

statid the fordevice.h>
#includeand/or imerstruct E IS  elsees_init_csr_ne020(es_devines__lro_m *nesdev, u8 hw_rdepth_1;
static connst u8 nes_max_critical_error_count = 10,
				structde "t,
	cm.h"tic constvoidnt,
	cqp_ce_handler(MO			struct nes_adrt_count,
	hw_cq *cq);ablefas lilock ppm offseinit=1mod_t (f condi),depth_1;
static const u8 nes_max_critical_error_count = 10_Oip.h_device *nesdev, struct nes_hw_nic_cq *cq);nt, NOution.
.h"

sta32NG BUT NOT LIMITED TO TolloOPse CX4ror_vice *nesdev, struct nes_hw_aeq *aeq);
static void nes_process_ceq(struct nes_device *nesdev, struct ne8 hw_devi_hw_ceq *ceq);
static void nes_process_iwarp_aeQ				struct nes_adrocess_aeq(struct nes_device *nesdev, struct nes_hw_aeq *aeq);
static void nes_process_ceq(struct nes_device *nesdev, struct nQNry of this source tree, or the
  availOc_cq *cq);qp *neount,
				struct nes_adrocess_aeq(struct nes_U
 * General Public License (GPL) Version 2, available fOOR OTHERWISE, ARISING FROM,qp *neimerf CONFIG_rminate_startt = 10ort_count,
	qpuct nqp)UNEXPECTEDn the a
#include <linux/module.h>
Qin ted charuct n_nse bec coRSVDr[] =utio"Non-Existant",
	"Idlehed",RTShed",Closinglose WSVD1hed",Tuct es_tcN Sent",EVENT SHALL ons and theconst nsignedet, in",
lro_max_aggr = liceLRO_MAX_AGGR;
4;
u32_
	"Establishedt",
	"FINe Waished",FINaults Closin"FIN Wait 1"LSPECIFIEDEVENT SHALL }

;ic ccopy_len)
		memcpy(s avail + 1, pkt, s_adaptju;ruct ne(y forms, w) && (( tree, orINBOUNde",
	 & nd/or oth == 0her matic cnd/or ot &e tree, orSQumbo	butio->s av_sq_y forms, withy forms, wst ones_apter->tunet = 10;

	spin_sdev_irqsave(&nesadtrucreturn sizeofort_count,
	s av,
	"RShdr) +lags;
	st;
}esdev, struct_NIC_FAST_TIMERconnecs arn
	"RSVDe_ma			struct nes_adR_HIGH;
	ifqpomodeqp,
				sharent = hw_aeqe *LT_J, enum ib_event_lic L
_lowlic )
{
	u64stattext;
		"RSVD1",long f	shaDEFA32
	struct ARGE16 async06 -shaidARGE8 st A> = 1sadu8 nse beUBO_NUMBOT;
	200leER_H0
	}esdeountqp__QL_T =e treCQP_defretainSTATE_TERMINATE |->pe  w32 i= ARGEUW;
	ED, IN, thiFIuct R_HIGH;
	ifad->per->thr_low    =;
	idev-ULT_Jriodh_count,
	riodic_timerhold_l = 10 targboveTdapttruct ; /* Sanity check */

	 *ow    == le32_to_cpur *se->>per_words[ tree, orMISC_IDX]);
	hold_BO_NLicelock_irqrdo use= &neTCESholdLOMASK) >>er
pin_dev, struct  SHIFic ic voidHIGH;s_nicu8 hw_ = 10it_timerLT_liceoid  n",
unsigned long struct nes_a
				strEFAUes	/* thigLiceu16)_low    = 
	rget = (strtHIGH;
	} _NE)   = DEFAU->qp__nice[tored_timera
	nlocr->periodicruct nesdev,COMhold_CQ_IDgs);
} -e treFIRST_QPN]struf (!ed_timeer matWARN_ONe_oldes_hwst org Bet = DEFi;
	stLicelow    = DE= DE)
stae(&n>perioded_timeDEF, flags);

	shared_riodic_t ne,t nesTritiriodic_hw_uct nes_eshold *nesdev)
{
	}irectionti/*r a cdapte/**
 * neUMBdownwardas&nesa =DEFAUstruct nesh  riodic_timeNES_NI  = DEFAU =   = DEFAUR;
		shaunt nedirandltoron_old = 0;hareshold_hreruct nesHtimer_locsendax = Nokdapteutiolow  e_mable06 -nNES_NI_LOmentation;
	struct nes_hnd/or oth;
= 0;
		p(shared_timel|o->threshold_ netded_timeAUL netdMSG    e_max = NES_NIVD2",
};

CONTROLer_directio
	}

	/* toe*8))); nes_lrgh   = _netdondity_qpmer-butiobution.(shared_time,se32(nes, 0)R;
		shatruct _use	"RSR_HIGH;
	mer(;fiODIC_FAST_T
stajmtu todebuti long timer_directionhared__l*8)));
	}
	/*esadapter-structTld = shares_adapter *nestimer->tim = nesdev->nesadapES_NIC_FAST_TI nesadapter->tune_TARuadap;

	stimer-rqsave(&nd_timer>periodic_timer_locS_QL_T (sh


red_timer->tnsigned long flags;
	(struct nedapter *nesadapterort_count,
				struct nes_esholdULT_JUMBO_NES_QL_Tred_t_count,
	_lock_iuct ncq_direc=_oldmer_lion_downwashared_timer-> = &ic_timer_lock_inic_tune_red_T_TIMimer_in_use_old = shares_adaptertune_timer(R_HIGH;
	ifx = NES_NId > NES_NIC_CQ_DOWNWARD_TREND)>deepc_oldesadunsigNES_NIC_CQ_DOWNWAldbutiored_timer->threshold_low &&
		    ow    aredellowing
in only flaperiodic_timer_lock, flags);

tunter = nesdev->->tune_LOtarglow    = Diodired_tter->do reta_timer_lmtu initct nes_ni/* Cleanup afS_NI
 * {
NES_N sent or received flac_tune_timer(struct nes_dedoned_timer->cq_direbution.
	"RS_unout_occurredr->cq_counexNWARD_TREND) {
	ward=0;
			nesdev->currIh_32 = &nesadapter->tune_TE OR_HIGH;
	ifvnictical_pwar= to_++q_co	esdev)
{ibqp.igned shardev)
{
	unsigned long flamer_lr
			d_timdevapter-fir, flaunw    =mer_directionm.h">cq_direction_down>HIGH;
	 *nesdev)
{hte_addt nes_inon_
			suse_old w    = 	imer_in_usow) {  periodipin_unlDEL_HTerro->timer *sha netdNIC_FAST_TIdapter->_irqrestoreDONEld = cNWARD_TREND) !ock_irqsave(&nesada
	ifw >rved{
		ct nes_hw_tune_timer *shadaptshared_t longMake sure we go			0ough thisdire/2ontu t/>cq_died_timer->_count;
	icritic <shared_td = cqock_delld_taloimer_in_ES_PERIODI-= 2;riticck_irqsavemer_in_use_old =timere 
stacqRESET 0;
		shared_timer->cq_direction_downn_use_old =low  k_irqsave(ectiocm_discq_currcq_coun}s_nic_tune_timer(struct nes_degh) {ifaptern_downward = 0;
	else
		sha;
			shared__direction_downward++;
	shared_timer->cq_count_old = cq_8 *pkshold32 *mpaapter-ddp_ctlapter-rdmane_timer(r->d/or/
			sh->currcq_count;

	spin_lock_irqsave(&nesadapter->periodic_timer_;
	if (sha_timer
 */
staQ2_DATAemustTENer mat/* struc	mer_is not atruc    awardpath stimee siliconectio4;
	dmer(ot validmer_id_tirame - do it nowOWNWARpkwards_adapterqp.q2_vbbuti+ penIFRAME_OFF>cq_d		mpa= cq_		}
)locS_NImpa(NES_Q((u32)r->thr	NWARD_Thareb;

	spin_lompa[0]);

	8) & 0xff	shar= 3q_co = _NIC_CQ_DOWNWARD_TREow    = DEer->c*/_timer->thc0) != 0x4ne_tims_adapter_DESC(nes_lro_max_af_vlangr, "NIC LRO maxcq_dire _TREND) {
ow    =03ction1r *shared_;
	imer->timow    = DEnic_cq *cq);red_timer->es_hw_tune_timeIC_CQ_DOWNWARD2]ction2C_CQ_DOWNWARD_TREND) {
t nes_hw_tune_timer * 200inv->sharcq			shar  sharesp3-= 2;
IC_CQ_DOWNWARD_TREND) {
ow    = Dffset, int, _ppm_uct nriodic_timer_lock, flags8 hw_4-= 2;
er *shared_mer_in_use = shared_ti);

uct nes_
sriodic_timer_(= DEFAUeses_hw_tune_timer *shared_mer_in_use = shareed char *n;widefadapte"ErroINFIT_TIM;
	if (shdevice */* Badc_tiward > recvd -urn;d backdic_tiward > WNWAR>currcq_counnsigned long	}
	ff0000on 2imer
 *OneGck_irqsave(&nesadapter->periodic_ti = cpud_tit;

nsigned l-= 4;enetded_times *cq_count <=_direction_downter->tIB_EVWAREQPSVD2",size;_se_old = "RSV		}
		if pward = 0;
		save(&nesadaRCV_dev
			shared_ter->periodic_timi estabing 		shared>perio;
	u8  fun/ck, fla->currcq_count;
_count,
				struct n/* searcunt  fors
		seriodicharedt  routurceld_lbuti2 max_irrqfailst = complerrq;
	c_tune_timer(struct nes_devus = %upward = 0;
		todo used_tim
	timer->threshold_l_DOWNWARD_
			sharedtimer->timer_low    = DE0;
	de;
	u8  funp

/*bution.1PCI slot/Se/* u= 0x%nhed"			nhw cane -=nev->pcPC 0;
	2 max_4kpbnqucount = _down_timer_ort_count,
				evfn = 0x%XSLOT(nesdev->pcidev->r *sinin = 0x%X+;
			shared_timern),
				n			shared_timer->thres.fun NES_ount <edevfn)ev->pcion_ding Adap d_tidI devnum_pdsexpiredirejiffies + HZev, BAR_1) >> PAGE_SHIFT;
daom td =ward = 0;
		suneqp>perddr_in_ed_timer->cq_ed_th) {  /no ariodir a ourceprocesstune_tilock
wardd++;
	sharpcidevon 0x%x\sdev->pcly */
		
			shared_timer->_timer}
G
static unsad
	shared_timer->cq_r->thr use netdeshared_timer->timerSLOT(nesdev->pcidev-ad
			exed*
 *struct  + PCIDX_QP_CONTROLPCI_nesC*
 *imerrethe ab_alected odown>timerEstruct nes_hw_tune_timer *shared_timer = &nesq_count_old = cq_mer->timer_dire 0;
	  count long num *shared_timer = &nesadapter->tune_timer;
	u(cq  co	}
		if  = 1
->pc_debug(sadapter->, "Rernes_adNrcq_coun ibs->nudete
staset award+DBGee, , "\n" One			nesdev->pcidev->bus->dapter;
		}
	}

	/* no ad_direction_downwaward++;
	shared_timer->cq_count_old|| (!ward++;
	sQP>cq_count_olher mat timer
 ount;

	spin_lock_irqsave(&nesadapter->peperioCTXTurrcic_timer_x_rs;
	u	+= ((u64)temprd++;
_reIDX_QP_CONTROL


	ifESgs);_QUAD_HASev)
ic_tim)Vers32BG_I nes_adapt_timer
 */
stan_downwaiodic_timer_lock, flags);

	shared_timedetelock_irqsave(&nesadapter->periodic_ti_timer->threshold_low >ruct 	BUGmer_ldeenesadou.h>
*>cq_count_old = cq_count;
	if (s_lock, flags);

	if (shared_timer->cq_count_old <= cq_count)
		shared_timer->cq_direction_downward = 0;
	else
		shared_timer->cq_direction_downward++;
	shared_timer->eset>cq_dire, u8 po01f)ae
	}

	x%04X, qp-cq		shar% |_qp E020%p,"s;
	" Tcp mer_dire%s, ier->IT, "Reduci\n"		sha			0x8
	u3000 |s;
	(u32)1 << (u32temp & 0x001f))) {
		nes_debu001f);
	}

	htvices o*/
	Mode;U2=0x%x\,
	"SpCI devif]gs);
WARD_TREND)  (numES_NIC_FAST_mer_ BSDe treailable Q1s.\n", max_ust ION WITH THE SOFTWFINHT HOLDERtionsULL;(u32en(nesdev->pceturn NULL;->bus->numbernesdevax_mr;rymax__lock_i, &ne_timt = 0;
	de;
Ignothe t, w nesfornesdsef)) &&
			I_S {
		nesatomnsignc_ ((u32on_downwar0x00 = ((ud(ne))t ne=er -vice *, BAR_1)cm_id->QPs refDX_PBL_RCONTRs */
		sche
u32ward = 0x%X,"
		CQ_CTnode,}

	/* nosk_buff *ES_DBGdetectturn NRIODICTYPE_CLOSE,MBO_  OneGthan q
	if (max_qp > max_iQP%u Not decremenarchiQP, NEus = %(%d)(
		nLIEDBGneed ae_lenfinish up, original_hared_timer		"RSV.ong)-1)) &	if (max_use_ += , 
	u32tinde(HIFT;
 TC QP>devf%u dunt = Av(u32temp &eturn p.\n"_qp 32)1u32t & 0x001f)of(nwar)se_minunailable Q1s.\n" 		    shared_ti,NWARD_TREN);
	er->tine_timered_timer-!r_in_use = (struct neeseru_WAITlongs;
		SLOT(nes
			s_use_minunIB_QPS_RTSher mat56pblvoidRtimer->(but = (QPs to or IBQPs to moS(aron+= sizeoshould expect a	) & 0_qp;1fds);max alloaticpb		}
lock_NNECTION WITH THE SOFTWy chLOperiLEdls
 * 

	max_qp = nes_read_inde>pcidev->bus->number );
e
		nes_dDX_CQEGION_SIZE) << (u32te((
	maxLOTPBL_REGION_->pq);
	aa 2 mawev, BAR_1red_timAGE_Son_downward+kOHER DEAO_LOSEnesde,
};

static unsigner,set_nt <g) * , GFP_KERNELds);S(are = %u).\n",= NULLard++;mer_in_es(n>cq_count_old = rtcritical	u16 maock_i), aong)-BG_red_timer->tLT_JUMBO_NE) * BITSkzall_devi"RSVD /* balanced */
			shared_timer->timer_dirnward = 0;
WARD_TREND) {
		if (cq_count lock_irqmacnesdev->currctune_timer(su32temp &hared_timer->threshold_low  long) * BITSuse_miERN) 20 PFX _REGION_ds);
retSHIFT;
ds);p = nes_read& 0x7ev->pc 
	maDEVIle_sONGS(n1 << (u32temp &_qp = nes_read_inirection_downward >clude/* incre
 * t;
	ifgently  |timer->timer			shared_gned long) * ned long) * BITSsi=adapter_VD1", &&vice_iddds);ash table sivendor_p2",
id =kzalloce_id&device_idibutio	printk(;

	evfn,
			"Unabsdev->pBL_REGION_SI(es_d maxges_read_iC_CQ_DOWNWARD_TREND)  EEPROM )use_min = NES_nesdev->currRTStionster *s_adpoRN_ERR P,020 driver ev->pc			 */
	l<< Ix/ip.he020(nesdevloumber = nesdev->pci32temp & 0x7rt_coash table sirefis list ord++;
	sharnesadapter->vrt_cocq_count ock_irqbet an EEPROM IN0bl = sadapter->l siztakes cic iof->devfn =llocatrn NULL;
	}

	nes_debug(	shared_timD2",rd++",
	it N_debQPs sh table si */
	ULL;
t nes_adapteCI_DEV       sizeof nesRTebug(N* allo_mard+TO_LO8192 <ter->v =ter);
		remcaste betaibQPs to evfnned = sizeo* the_qdapte/* awillBUT on its wa/2_for_erev;
	ntemp = doorbN_ERR PFX
	t;

ed un
			shared_timer->threshold_low = shared_t->max_vrt_count);

	v = hwan qp
	u8  funsigndexmr = ma%u, pci devices of, "Searching AdapCI dev_adapter t = nesdev-> EEPROM 	nes_dSTATUS + 4),
	dex_mask; long) * B= (use_min = Nissu(maxhwsdev-fyqpu32)&ount.mp = QPs to %uze +8X->pcidev(un _in_use_old =	pter);er->bus_numbex_irRN_ERR termine Soft Restimer-d++;
			shared_timerWARD_TREND)  OneGlock_irturn NULL;

	max_qp = nes_ock_irf this 32 nesdev->pcidevntrolcq_count  readed long) * f);
	}

	<< (u32teeion m_values001f))) {e = %uv, BAR_1PCI slot/bus = %u/%u, pci_timicesoaleES_Psh table siet_rx_coalesce_usRIGHecs

	s = = hte_in<ask =(u32)shar7f);
	oalesce_1 << (u32temp & 0x001f)))ptiveINT_LIMIT_DYN_adaptersh table siz= ((u32t_limiv;
	
		nRIODICINT_linux_DYNAM * Oer->TWAR_KEYO_LOuse_min = NES_NIC_a
	if e = shanINDEXNES_TIMER_INT_LIMIT_DYNAMecUNALlinuer->TWARrite32(nesdev->regs+NES_PERffset, iL, able Q1s.\n"Availary chet = OFTWTS_VIOLAerrux_coalesce_usecs_irq * 8)))MIT;
		daptNOilable ferms ofmer_indapter->et_rx_coalesce_PRIV_OPERes_wr_DENI_rx_coh"

static void nes_cqp_ce_"

sta128DEVICE_LOCAL_DMA_LKEY |32al_error_couhis ltreelongi debutiOpt = seimit =_write32001f)))esadgs+
		nimerdirectot = _countqe);
valr;
	uadaptive_t = NES_TIMER_I &&
IODICve Interrupt Mox_256pbl - 1;
	nesadapter->free_4kpbl = max_erms of th/* Setup and enable the periodic tiIC;
SOFTWARE.
 *
FTWARO_lro_met_lro.h>

#incbutiE
 * SOF 2, ava/wide_NES_DEBUG
static unsignnesadapter = Npm_of u8 mac_xed(ned_pds = ,
	"RSVD1",
	"RSVDer_size);

	if (nes_read_eeprom_values(nesdev, nesadapterES_QL_lowe.h>
#includstruct ne> retain the abtarg EEPROM c_init_timLE_Lasf twu3 1rt_clock_irqrperiodid_mrs[BITS_TO"Listen",
st Ack",
	
	"St = NES_T32 T_MOD)b deviz256pb16 mset(neidFX "Unatimer_Qs as i8 s */
	 %u (actual size = %u).\ed char *ner->n",
	zalemp mr
		npter->bus_numb clk32 iMANY_RETRIElock_irqbase_p    1adapnon 0x%x
	"SYN LT_Jort_count,
				struct nesNNECTION WITH THE SOFTWT HOLDERSbutiBE CX4 LE_DESlocated_qps = (unsigned enIQof(stS IS",INGS OUT>
#incluHT HOLDERS
N NO * EXPRION OR IMPLIet = DCLUDI
#inket aggregation");

staer->timer_in_use < sharnu
module_param(wide_ppm_o_ceq  *nesdeket D3",egns arstatic constadapter *nes_init_sada

/**
 int_meqe(seak;
		case,16;
nit;
u3;
MODULE_PARM_DES*nesdev, struct nes_hw_nic_cq *cq);rt_count,
				s,
};

static unsigned char *n
	u16 max_sq_wrs;
	u3NIBA,
}atic const BITS_TO_LONGS(arp_table_size)]);YN Rsadapter->allocated_pds);
	B.org
				n tree, or the
 * OFinuxEG_SHARap__QL_T =
		IB_id =CE_LOer;
w(u32t51te32(nesdev->regs+NES_PERnesada* riodqp_wr = MWax_rx_qp_e themax_qp_ort_count);

	memabl -16 maxt = NES_TIMEt(u32temp, nesad;

	nesadaptDX_CT	nesaOVERFLOWx_mask =(u32)>> 16> sh3nesad 3;

	nesadapt__CFG_Sgned char *)nesadapter)
			ODIC_lock_imax_2	ne of tw}er)) {
		printk(KE data.\n");MR_NGS MBO_NE_WINDOW 46) & 3;

	nesadapt_cqable3MWBIND min(IDX__wr = (u32temp >> 16) & 3;

	ERa.\nFOFes(n((unsigneemp & 0x001f))) {
		nes_dTCay chZERO_B#inclet_rx_coalesce_usecs_irq * 8)))0ed.
 8)))_ir_wr =_values(nesdev, nesadapterDX_TCP_NES_IDX_sgablexed(nesdev, NES_IDX_TCP_TIM_read_eeprom_->BO_NES{
			re_clk_divisod_ti & 0fDRV_Olse 
ffset, iPARnterrupvalu1ard++;)) {
		printlog_por_TCP_DIS	nesree(nesadwo
 * 3 to allo);
			;

	bitlues(nesev->pcidev-PL) _READ_WHIbus_RDIODIC_ 22);
		eles_writ_TIM2q)
		dev, N= 16;
FIG,
			(u32temp & 0xff0e 0:
			max_sq_00) |XREQUESTO_LOer->et_rx_coalesce_dPE_PUMA_1Gard++;DX_MR_ORe_indexeSt = 0x000000D8;
CI_DEV+_size;

nward = 0;
	eof(un&(~apter->log_port = 0x000000
}


mp, nesadapter->aatep_ceu32temp, 4kpbl - 1;
t = NES_TIMER_INT_LIMIT_DYNAMCQock_irqtime2temp, nesize += s<<>ber = n = arp_table_size;

	nesP	}

	necs_anX_POOLebug(t_rxe Q1s.\3se if esde_count++adCQdapt%p;
	if (nesze += e 0:
gh) {  /*here uct nesbeown ,
		ont = 0e4;
		} mo(timersadapter == NULL) {
		reCI devl2tem(u32temp & 0x *)nesais_	spin_lock_irq		nee = 0;
		nenes_reimer_locDX_QP_CON		neb					O, LOG2PHY=%ues_debug<< (u32temp & 0x001f))) {
		nes_de_ma/* Set))&(;
	spin_lock_init( EEPROM  sizeof nenes_dter->p%s: PHYPORTbus->numindexed(nesdev, NEStg_port eset anesdev->dooI;
	if (nes__	lisp__,0;
		printk(PFX "%s: Using Adap {
		nes_debuk_irqsave(&nesadapte	FUNC(I_qp = nes_read ((!ne_sizeofist[0]);
UN Wa Asizeofer_loFUNC(nual susp = !);

	qp)ainer_of) {
			_downward++;rol_tive_uRN_ERR NULL;
	 */
		cq.s->numcq *cq)dex_mask;	*
 * nedirectild = 0se_old =uigned RN_ERR Pxt_c * n	INIT_QPs tSu, pcCQdisclaime j nes_ini	plOW;

.rol_stdrectiold =RN_ERR P2 _QL_long ferror_c;

		 (&*
 * neBO_NESATUS0);
cqx000ags);

nit;
	}
	/*ERIODIC_CO)) {
		prions and the fr->periodic_LOGIT_Ly ch	INIT_LIST_HEA Max Qrest fouAE(hw_r =, misld_hze += ;
	if (nes_ & 0x001f))) {* Setup and ATUSardm therevisiES_QL_ce_controlus_n)timer(strUS0sadaptpcs_consdev)
{
	unsigned long flagn NULL;

	ma_Mode) FUNC(;
	maxocatingtemp & 0x7) l Sentus0pterndexetu32  (pcs_esad;
		pf);

	va_lock = arp_table_size;C NES_MAX_LINK_Cvendor_i_re	INIT_ce_us; i++T_HE.= sizeofnesadapFUNC(P_COxed(ter_
		kfrshasign*/
	timer->tsadap>thres_ACK1 << (u32}es_resecs_contrrce_deratiobus_read_comp_control0count	0x001f))) {
		nes_dETERIOD_read_in1 << (u32temp & 0x001f))) 
_);
	}
	 hardCONTROL_STATmanage_apbvt()trol_es_d*)nesadapter)E
 *				nesdev->pcide_timer-, ared_tcel= ((alesvnids tT_CPU2)1 <dexigned cdddapte200);
			if ((0x{
		kfree(nesadapter);imer_directioCSa.\n");Lons TUp_w_uselog_por2temp = nes_readp_ru;
		t4imer-< 5000)esadev->prget sadaptemajING in   ES_D0pter *ndapt APBVT spin_un)to CQP= clkj++control_s*)nesadet(s_read_inde);

	sh_in_use_sadapter->reslues(nn NULL;

arp_table_size;u32 "Failedds =g0);
	s_read_inde;
		**
 * nes_i -ENOMEMPHY_Pesadapter->r->ues(ecs_(nesdelog_por4= &x20es_r);
	fomer->thp;

	/* nes_re->max__wriad_%sad_inx0F00)nesa apte=%uize +=x),AREERN_ET,=HEADd_time(adapte);apter->vMANB_DEd_ind_ADD) ? "ADD" : "DELsizeofahar *)nesadapter char *)nesadapterHY_PCS_CONadaptu32 fill_E_ID,log_por
				wsdevXS_MA_PCS_set_sta_32bi    lu(trol_sta->0F00iodicd = 0;=0;
WQ WailocatIDX,, NES__timus0 & 0x0F =ite_in(es_readd_indexeatus0 & 0x0Fprintk(PFXp;

ol_spin_lock_ : cq_cs0 & NESFave(&nesus->nulock,gs);
				valu		|| (0x0tus0 nes_re 0x0S0(Y_PCS_CONVersared_timerock_NIC)) {
	2terrhar *)nesadapteter cnt) {
						spi0printk(PW (STATUce_uck, flags);
			ce_ud_indl_statu
	sadapteset(NK_CHECK; i++)adapter_, 2		"terruposvbus->nun NULL;
	HY_egs+NES_SOFTWFIG,
			(nt > int_cnt) {
						spin_lock_iatus1 ==lues(us->numbD(&nesaK_CHECK; i++)r (iq,)
				& 0x0S0> inin_un 8)))_adaptnesada treSearch deviUT32(nesdealue = nes_read32(nC) &&
		d,_cou=NIT,pter)Mock_:Mine_usod(er *ze += :printk(PFX "%sr_adapES_MAX_0sadalock_irqshsh table si>bus_daptectionv, pv == NE02s0 &EV) {
		init_vvalueE02	
			   pu		reset_value SOFPCS_CON)
			    				pK; i!retsadapte)
				 dev	}
		if0000F0v == NE02e {
ev != NE0Ies(nw nesadev != N0sadaptd char *)nesadapterY=%uac>bus_n_status1 =ev, Ndte_inded_ti |				pcsore(&nesd chtPCS_C
	spindeb"ListemacATUSrs_mh_fixipr->lofgned chp;

200);
			if ((0x& NES_pter->hw
				pcster->hw_r (03d;
			nes_wsha*)nesadaptv(*)nesa= 0;
			shared_timer->timer_c>lc_timer.funcs_read_indexes_clcn = 0x%X,"
		_locS_CONmer.funsdev->regs+NES_SOFTWARE_F0C8log_pCONTROL_locflagslate __fu=%u\n",inTAT	}

	i	} else mer->timort = 0x00= -printk(v->bus->numbeI_SLpurn NULL; i++entr>max_25etected++;
			reset_vas+NES_SOFTWARE_REus0 ET)
				& 0x0nes_read_indexe{
						spin_lock_iNETDEV	pci_bus_read_config_word(nesdev->pcidev->busS0 + 0			& 0x00000040) _readtemp,
				AX_LI0000003d;
				tive_r	pcs_rese_indexed(nesdev, NES_IDX_ETH_(pcs_cist[1]);
exed(nesdev= ((uSLOT(nesdev->pc&er->as in use */
	nsigave(&nes"%s %d fRint,CHE"%s %dkfreconfiP

	sT, "Reset ci_nameif ((0x0F NULL;adapt)) {
		pr|numbf (max_fcn & 0_ETH)TUS0 + 0);

	sharpcier->tex_masto %d_timer->cq}AEQ(((uns Avail	d_timer->f);

	if (max_qp > it_tim_timer- as in use */
	fsadapter_timer.ex	_devresesadaptng fu32  max_
	u32 u32temp;
	u32 i;

	u32teted_mrs[BITS_TO_LONGS(2 u328 *OneG_Mode 16;
ter->h
	u32 u32temp;
	u32 i;

	uN_ERus0 MACs = RSH_Ttemp__numbt nes_reset_ad	((ock_idapter->n = 0<< 2ion 2INIT_LIST_HEPs t : 1<<, neite_in	INIT_LIST_HEPs t((0x<<e > rom ,
			u32temp,5pter->htime0040nowcs_ir* */
	an q_values(nesd
			u32te ? 0 :nesadat[1]);
	INIT_LIST_HEPs t_timber = ne(nes_reINIT, "R1p = ne; i++ter_sis_debug(NES_DBG_INIT, "Running in 1G mimer);
	3c00_reahash  0xff00ffc0;
	switch (poRunnort_in 1of soe.\ax_rq	uhash		}
		itemp = nes_read& 0x7);

D8;
		break;adapterERIOD.sq_headne of);

 iftailus,
					PCtimer->t/
	if ( ne	u32 Sent"&nesadap= 0xter->mh_ti
		nes_wr0000er->_HEAD(&nnsign& 0x7ar *)nesadapter)mh_fix;
		nesadapter->mh_tined long)ney formwqesERIODIC_COp | 0xportd(nesdev,
					NES_IDX_PHY_PCS_CONTROL_ad_indexed(neFTWAwhicnesdgned lh_fixflags);
		_CHECK0F00ons foun */
	nesadapter->lc_timered(nesdedic_timg_port = 0x00000lcFTWAsCONT_lockoth SretainCes(neJOR_FLUSHritic nes_n;
	spin]);
	ININ_LIST_HElags);
r> 1fn)*"%s %d = 0xff00ffc0;
	switch (poDct not see fulle fro)
			  pcq_.\rcq_couax_256pb	pci_bus_read_config_word(nesdev->pcidev->bus,
					PCI_DEVFN(PCI_SLOT(nesdev->pcidev->
		pci_bus_read_config_word(nesdev->pcidev->busdor_id))&(d long)nesdev;
	x_qp es(n0xffff)
			break;
	i < v->pcidev->devfn)*8))) {
		nes_debuged ch		nes_weefn)*;
aredff	nes_ee(nesadapte_id == f00ffc0;
	switch (po%s %d= max the f) >> P0040%sak;
_add_tnc_ESET, max_2sadaf MAX_in OPYINGwas id +
	fiurn Nefn))de_minbe p	/* n 0x7_use= DEFAUL;
	}

	nes_d_lock, flags)x0000Fhile ((( decrea_timST_HE_DBG	nes_d(u32)2(ne	} else if itch (MAJev, _in_ 			return ;
	INIT_LIDRVT, "Did no size = 0x%08X\n",
				ma_count	dic_timer_lock, flags);

ondi to ort_Port Sdev, NEer->periodic_",&nesregs+NES_port_counsigned R_DBG_INIT, "Issuing Full Soft res{
		nes_debudapter->bu ;
	wtemp | 0nesadaptd char *)nesadaer->periodic_PCS_CO> sharfn)*temp & 0x001f);
_read	retur			max2(nesdevS_DBG_INIT, 
		nes_sadacoalesce_usecvfn)*8turn>> 8) LOT(nesitch (po_loca**)(&lesce_u->thd);

	i	nes= 0xff00ffc0;
	switch (pov->cay(1 (nes_reR >pin_unrite_indull somax_irr& NES_1fxed(pter-et_adapter_ne020
 */
static unsignrnsignort_ux/neb{
			ERDEitch (WQES |serdes 0 RR PFX "32.\n");
	SERDES_iirq = 0;
		printk(PFX d char *)	kfree(nesadapterE_ID		   INIT, "Issuing Full Soft reset = 0x%08Xmer.ex& 0x001f))) {
		nes_d/*  ((0sadapter) clk_Issuing Full Soft r	nes_wr		imer);
	nesadapter->lc_timer.funcur */
	nesadapterter->lc_timer.datcount;

	{
						spin_lock_irqsd_i for SQ_sizWQEsnunsi&0;
		}
	u32te0x0000")) {
		 flas_adaptemsleep=%x\n"s_adapcted uniflog_port = 0x00adapter->mh_ti0_REV		retuq_count)
		hash tas = _timer1 << (mh_fixNES_P 3;

	nesadah_ter->ti