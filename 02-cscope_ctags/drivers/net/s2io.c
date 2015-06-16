/************************************************************************
 * s2io.c: A Linux PCI-X Ethernet driver for Neterion 10GbE Server NIC
 * Copyright(c) 2002-2007 Neterion Inc.
 *
 * This software may be used and distributed according to the terms of
 * the GNU General Public License (GPL), incorporated herein by reference.
 * Drivers based on or derived from this code fall under the GPL and must
 * retain the authorship, copyright and license notice.  This file is not
 * a complete program and may only be used when the entire operating
 * system is licensed under the GPL.
 * See the file COPYING in this distribution for more information.
 *
 * Credits:
 * Jeff Garzik		: For pointing out the improper error condition
 *			  check in the s2io_xmit routine and also some
 *			  issues in the Tx watch dog function. Also for
 *			  patiently answering all those innumerable
 *			  questions regaring the 2.6 porting issues.
 * Stephen Hemminger	: Providing proper 2.6 porting mechanism for some
 *			  macros available only in 2.6 Kernel.
 * Francois Romieu	: For pointing out all code part that were
 *			  deprecated and also styling related comments.
 * Grant Grundler	: For helping me get rid of some Architecture
 *			  dependent code.
 * Christopher Hellwig	: Some more 2.6 specific issues in the driver.
 *
 * The module loadable parameters that are supported by the driver and a brief
 * explanation of all the variables.
 *
 * rx_ring_num : This can be used to program the number of receive rings used
 * in the driver.
 * rx_ring_sz: This defines the number of receive blocks each ring can have.
 *     This is also an array of size 8.
 * rx_ring_mode: This defines the operation mode of all 8 rings. The valid
 *		values are 1, 2.
 * tx_fifo_num: This defines the number of Tx FIFOs thats used int the driver.
 * tx_fifo_len: This too is an array of 8. Each element defines the number of
 * Tx descriptors that can be associated with each corresponding FIFO.
 * intr_type: This defines the type of interrupt. The values can be 0(INTA),
 *     2(MSI_X). Default value is '2(MSI_X)'
 * lro_enable: Specifies whether to enable Large Receive Offload (LRO) or not.
 *     Possible values '1' for enable '0' for disable. Default is '0'
 * lro_max_pkts: This parameter defines maximum number of packets can be
 *     aggregated as a single large packet
 * napi: This parameter used to enable/disable NAPI (polling Rx)
 *     Possible values '1' for enable and '0' for disable. Default is '1'
 * ufo: This parameter used to enable/disable UDP Fragmentation Offload(UFO)
 *      Possible values '1' for enable and '0' for disable. Default is '0'
 * vlan_tag_strip: This can be used to enable or disable vlan stripping.
 *                 Possible values '1' for enable , '0' for disable.
 *                 Default is '2' - which means disable in promisc mode
 *                 and enable in non-promiscuous mode.
 * multiq: This parameter used to enable/disable MULTIQUEUE support.
 *      Possible values '1' for enable and '0' for disable. Default is '0'
 ************************************************************************/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/mdio.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/stddef.h>
#include <linux/ioctl.h>
#include <linux/timex.h>
#include <linux/ethtool.h>
#include <linux/workqueue.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <net/tcp.h>

#include <asm/system.h>
#include <asm/div64.h>
#include <asm/irq.h>

/* local include */
#include "s2io.h"
#include "s2io-regs.h"

#define DRV_VERSION "2.0.26.25"

/* S2io Driver name & version. */
static char s2io_driver_name[] = "Neterion";
static char s2io_driver_version[] = DRV_VERSION;

static int rxd_size[2] = {32, 48};
static int rxd_count[2] = {127, 85};

static inline int RXD_IS_UP2DT(struct RxD_t *rxdp)
{
	int ret;

	ret = ((!(rxdp->Control_1 & RXD_OWN_XENA)) &&
	       (GET_RXD_MARKER(rxdp->Control_2) != THE_RXD_MARK));

	return ret;
}

/*
 * Cards with following subsystem_id have a link state indication
 * problem, 600B, 600C, 600D, 640B, 640C and 640D.
 * macro below identifies these cards given the subsystem_id.
 */
#define CARDS_WITH_FAULTY_LINK_INDICATORS(dev_type, subid)		\
	(dev_type == XFRAME_I_DEVICE) ?					\
	((((subid >= 0x600B) && (subid <= 0x600D)) ||			\
	  ((subid >= 0x640B) && (subid <= 0x640D))) ? 1 : 0) : 0

#define LINK_IS_UP(val64) (!(val64 & (ADAPTER_STATUS_RMAC_REMOTE_FAULT | \
				      ADAPTER_STATUS_RMAC_LOCAL_FAULT)))

static inline int is_s2io_card_up(const struct s2io_nic *sp)
{
	return test_bit(__S2IO_STATE_CARD_UP, &sp->state);
}

/* Ethtool related variables and Macros. */
static const char s2io_gstrings[][ETH_GSTRING_LEN] = {
	"Register test\t(offline)",
	"Eeprom test\t(offline)",
	"Link test\t(online)",
	"RLDRAM test\t(offline)",
	"BIST Test\t(offline)"
};

static const char ethtool_xena_stats_keys[][ETH_GSTRING_LEN] = {
	{"tmac_frms"},
	{"tmac_data_octets"},
	{"tmac_drop_frms"},
	{"tmac_mcst_frms"},
	{"tmac_bcst_frms"},
	{"tmac_pause_ctrl_frms"},
	{"tmac_ttl_octets"},
	{"tmac_ucst_frms"},
	{"tmac_nucst_frms"},
	{"tmac_any_err_frms"},
	{"tmac_ttl_less_fb_octets"},
	{"tmac_vld_ip_octets"},
	{"tmac_vld_ip"},
	{"tmac_drop_ip"},
	{"tmac_icmp"},
	{"tmac_rst_tcp"},
	{"tmac_tcp"},
	{"tmac_udp"},
	{"rmac_vld_frms"},
	{"rmac_data_octets"},
	{"rmac_fcs_err_frms"},
	{"rmac_drop_frms"},
	{"rmac_vld_mcst_frms"},
	{"rmac_vld_bcst_frms"},
	{"rmac_in_rng_len_err_frms"},
	{"rmac_out_rng_len_err_frms"},
	{"rmac_long_frms"},
	{"rmac_pause_ctrl_frms"},
	{"rmac_unsup_ctrl_frms"},
	{"rmac_ttl_octets"},
	{"rmac_accepted_ucst_frms"},
	{"rmac_accepted_nucst_frms"},
	{"rmac_discarded_frms"},
	{"rmac_drop_events"},
	{"rmac_ttl_less_fb_octets"},
	{"rmac_ttl_frms"},
	{"rmac_usized_frms"},
	{"rmac_osized_frms"},
	{"rmac_frag_frms"},
	{"rmac_jabber_frms"},
	{"rmac_ttl_64_frms"},
	{"rmac_ttl_65_127_frms"},
	{"rmac_ttl_128_255_frms"},
	{"rmac_ttl_256_511_frms"},
	{"rmac_ttl_512_1023_frms"},
	{"rmac_ttl_1024_1518_frms"},
	{"rmac_ip"},
	{"rmac_ip_octets"},
	{"rmac_hdr_err_ip"},
	{"rmac_drop_ip"},
	{"rmac_icmp"},
	{"rmac_tcp"},
	{"rmac_udp"},
	{"rmac_err_drp_udp"},
	{"rmac_xgmii_err_sym"},
	{"rmac_frms_q0"},
	{"rmac_frms_q1"},
	{"rmac_frms_q2"},
	{"rmac_frms_q3"},
	{"rmac_frms_q4"},
	{"rmac_frms_q5"},
	{"rmac_frms_q6"},
	{"rmac_frms_q7"},
	{"rmac_full_q0"},
	{"rmac_full_q1"},
	{"rmac_full_q2"},
	{"rmac_full_q3"},
	{"rmac_full_q4"},
	{"rmac_full_q5"},
	{"rmac_full_q6"},
	{"rmac_full_q7"},
	{"rmac_pause_cnt"},
	{"rmac_xgmii_data_err_cnt"},
	{"rmac_xgmii_ctrl_err_cnt"},
	{"rmac_accepted_ip"},
	{"rmac_err_tcp"},
	{"rd_req_cnt"},
	{"new_rd_req_cnt"},
	{"new_rd_req_rtry_cnt"},
	{"rd_rtry_cnt"},
	{"wr_rtry_rd_ack_cnt"},
	{"wr_req_cnt"},
	{"new_wr_req_cnt"},
	{"new_wr_req_rtry_cnt"},
	{"wr_rtry_cnt"},
	{"wr_disc_cnt"},
	{"rd_rtry_wr_ack_cnt"},
	{"txp_wr_cnt"},
	{"txd_rd_cnt"},
	{"txd_wr_cnt"},
	{"rxd_rd_cnt"},
	{"rxd_wr_cnt"},
	{"txf_rd_cnt"},
	{"rxf_wr_cnt"}
};

static const char ethtool_enhanced_stats_keys[][ETH_GSTRING_LEN] = {
	{"rmac_ttl_1519_4095_frms"},
	{"rmac_ttl_4096_8191_frms"},
	{"rmac_ttl_8192_max_frms"},
	{"rmac_ttl_gt_max_frms"},
	{"rmac_osized_alt_frms"},
	{"rmac_jabber_alt_frms"},
	{"rmac_gt_max_alt_frms"},
	{"rmac_vlan_frms"},
	{"rmac_len_discard"},
	{"rmac_fcs_discard"},
	{"rmac_pf_discard"},
	{"rmac_da_discard"},
	{"rmac_red_discard"},
	{"rmac_rts_discard"},
	{"rmac_ingm_full_discard"},
	{"link_fault_cnt"}
};

static const char ethtool_driver_stats_keys[][ETH_GSTRING_LEN] = {
	{"\n DRIVER STATISTICS"},
	{"single_bit_ecc_errs"},
	{"double_bit_ecc_errs"},
	{"parity_err_cnt"},
	{"serious_err_cnt"},
	{"soft_reset_cnt"},
	{"fifo_full_cnt"},
	{"ring_0_full_cnt"},
	{"ring_1_full_cnt"},
	{"ring_2_full_cnt"},
	{"ring_3_full_cnt"},
	{"ring_4_full_cnt"},
	{"ring_5_full_cnt"},
	{"ring_6_full_cnt"},
	{"ring_7_full_cnt"},
	{"alarm_transceiver_temp_high"},
	{"alarm_transceiver_temp_low"},
	{"alarm_laser_bias_current_high"},
	{"alarm_laser_bias_current_low"},
	{"alarm_laser_output_power_high"},
	{"alarm_laser_output_power_low"},
	{"warn_transceiver_temp_high"},
	{"warn_transceiver_temp_low"},
	{"warn_laser_bias_current_high"},
	{"warn_laser_bias_current_low"},
	{"warn_laser_output_power_high"},
	{"warn_laser_output_power_low"},
	{"lro_aggregated_pkts"},
	{"lro_flush_both_count"},
	{"lro_out_of_sequence_pkts"},
	{"lro_flush_due_to_max_pkts"},
	{"lro_avg_aggr_pkts"},
	{"mem_alloc_fail_cnt"},
	{"pci_map_fail_cnt"},
	{"watchdog_timer_cnt"},
	{"mem_allocated"},
	{"mem_freed"},
	{"link_up_cnt"},
	{"link_down_cnt"},
	{"link_up_time"},
	{"link_down_time"},
	{"tx_tcode_buf_abort_cnt"},
	{"tx_tcode_desc_abort_cnt"},
	{"tx_tcode_parity_err_cnt"},
	{"tx_tcode_link_loss_cnt"},
	{"tx_tcode_list_proc_err_cnt"},
	{"rx_tcode_parity_err_cnt"},
	{"rx_tcode_abort_cnt"},
	{"rx_tcode_parity_abort_cnt"},
	{"rx_tcode_rda_fail_cnt"},
	{"rx_tcode_unkn_prot_cnt"},
	{"rx_tcode_fcs_err_cnt"},
	{"rx_tcode_buf_size_err_cnt"},
	{"rx_tcode_rxd_corrupt_cnt"},
	{"rx_tcode_unkn_err_cnt"},
	{"tda_err_cnt"},
	{"pfc_err_cnt"},
	{"pcc_err_cnt"},
	{"tti_err_cnt"},
	{"tpa_err_cnt"},
	{"sm_err_cnt"},
	{"lso_err_cnt"},
	{"mac_tmac_err_cnt"},
	{"mac_rmac_err_cnt"},
	{"xgxs_txgxs_err_cnt"},
	{"xgxs_rxgxs_err_cnt"},
	{"rc_err_cnt"},
	{"prc_pcix_err_cnt"},
	{"rpa_err_cnt"},
	{"rda_err_cnt"},
	{"rti_err_cnt"},
	{"mc_err_cnt"}
};

#define S2IO_XENA_STAT_LEN	ARRAY_SIZE(ethtool_xena_stats_keys)
#define S2IO_ENHANCED_STAT_LEN	ARRAY_SIZE(ethtool_enhanced_stats_keys)
#define S2IO_DRIVER_STAT_LEN	ARRAY_SIZE(ethtool_driver_stats_keys)

#define XFRAME_I_STAT_LEN (S2IO_XENA_STAT_LEN + S2IO_DRIVER_STAT_LEN)
#define XFRAME_II_STAT_LEN (XFRAME_I_STAT_LEN + S2IO_ENHANCED_STAT_LEN)

#define XFRAME_I_STAT_STRINGS_LEN (XFRAME_I_STAT_LEN * ETH_GSTRING_LEN)
#define XFRAME_II_STAT_STRINGS_LEN (XFRAME_II_STAT_LEN * ETH_GSTRING_LEN)

#define S2IO_TEST_LEN	ARRAY_SIZE(s2io_gstrings)
#define S2IO_STRINGS_LEN	(S2IO_TEST_LEN * ETH_GSTRING_LEN)

#define S2IO_TIMER_CONF(timer, handle, arg, exp)	\
	init_timer(&timer);				\
	timer.function = handle;			\
	timer.data = (unsigned long)arg;		\
	mod_timer(&timer, (jiffies + exp))		\

/* copy mac addr to def_mac_addr array */
static void do_s2io_copy_mac_addr(struct s2io_nic *sp, int offset, u64 mac_addr)
{
	sp->def_mac_addr[offset].mac_addr[5] = (u8) (mac_addr);
	sp->def_mac_addr[offset].mac_addr[4] = (u8) (mac_addr >> 8);
	sp->def_mac_addr[offset].mac_addr[3] = (u8) (mac_addr >> 16);
	sp->def_mac_addr[offset].mac_addr[2] = (u8) (mac_addr >> 24);
	sp->def_mac_addr[offset].mac_addr[1] = (u8) (mac_addr >> 32);
	sp->def_mac_addr[offset].mac_addr[0] = (u8) (mac_addr >> 40);
}

/* Add the vlan */
static void s2io_vlan_rx_register(struct net_device *dev,
				  struct vlan_group *grp)
{
	int i;
	struct s2io_nic *nic = netdev_priv(dev);
	unsigned long flags[MAX_TX_FIFOS];
	struct config_param *config = &nic->config;
	struct mac_info *mac_control = &nic->mac_control;

	for (i = 0; i < config->tx_fifo_num; i++) {
		struct fifo_info *fifo = &mac_control->fifos[i];

		spin_lock_irqsave(&fifo->tx_lock, flags[i]);
	}

	nic->vlgrp = grp;

	for (i = config->tx_fifo_num - 1; i >= 0; i--) {
		struct fifo_info *fifo = &mac_control->fifos[i];

		spin_unlock_irqrestore(&fifo->tx_lock, flags[i]);
	}
}

/* Unregister the vlan */
static void s2io_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid)
{
	int i;
	struct s2io_nic *nic = netdev_priv(dev);
	unsigned long flags[MAX_TX_FIFOS];
	struct config_param *config = &nic->config;
	struct mac_info *mac_control = &nic->mac_control;

	for (i = 0; i < config->tx_fifo_num; i++) {
		struct fifo_info *fifo = &mac_control->fifos[i];

		spin_lock_irqsave(&fifo->tx_lock, flags[i]);
	}

	if (nic->vlgrp)
		vlan_group_set_device(nic->vlgrp, vid, NULL);

	for (i = config->tx_fifo_num - 1; i >= 0; i--) {
		struct fifo_info *fifo = &mac_control->fifos[i];

		spin_unlock_irqrestore(&fifo->tx_lock, flags[i]);
	}
}

/*
 * Constants to be programmed into the Xena's registers, to configure
 * the XAUI.
 */

#define	END_SIGN	0x0
static const u64 herc_act_dtx_cfg[] = {
	/* Set address */
	0x8000051536750000ULL, 0x80000515367500E0ULL,
	/* Write data */
	0x8000051536750004ULL, 0x80000515367500E4ULL,
	/* Set address */
	0x80010515003F0000ULL, 0x80010515003F00E0ULL,
	/* Write data */
	0x80010515003F0004ULL, 0x80010515003F00E4ULL,
	/* Set address */
	0x801205150D440000ULL, 0x801205150D4400E0ULL,
	/* Write data */
	0x801205150D440004ULL, 0x801205150D4400E4ULL,
	/* Set address */
	0x80020515F2100000ULL, 0x80020515F21000E0ULL,
	/* Write data */
	0x80020515F2100004ULL, 0x80020515F21000E4ULL,
	/* Done */
	END_SIGN
};

static const u64 xena_dtx_cfg[] = {
	/* Set address */
	0x8000051500000000ULL, 0x80000515000000E0ULL,
	/* Write data */
	0x80000515D9350004ULL, 0x80000515D93500E4ULL,
	/* Set address */
	0x8001051500000000ULL, 0x80010515000000E0ULL,
	/* Write data */
	0x80010515001E0004ULL, 0x80010515001E00E4ULL,
	/* Set address */
	0x8002051500000000ULL, 0x80020515000000E0ULL,
	/* Write data */
	0x80020515F2100004ULL, 0x80020515F21000E4ULL,
	END_SIGN
};

/*
 * Constants for Fixing the MacAddress problem seen mostly on
 * Alpha machines.
 */
static const u64 fix_mac[] = {
	0x0060000000000000ULL, 0x0060600000000000ULL,
	0x0040600000000000ULL, 0x0000600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0000600000000000ULL,
	0x0040600000000000ULL, 0x0060600000000000ULL,
	END_SIGN
};

MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);


/* Module Loadable parameters. */
S2IO_PARM_INT(tx_fifo_num, FIFO_DEFAULT_NUM);
S2IO_PARM_INT(rx_ring_num, 1);
S2IO_PARM_INT(multiq, 0);
S2IO_PARM_INT(rx_ring_mode, 1);
S2IO_PARM_INT(use_continuous_tx_intrs, 1);
S2IO_PARM_INT(rmac_pause_time, 0x100);
S2IO_PARM_INT(mc_pause_threshold_q0q3, 187);
S2IO_PARM_INT(mc_pause_threshold_q4q7, 187);
S2IO_PARM_INT(shared_splits, 0);
S2IO_PARM_INT(tmac_util_period, 5);
S2IO_PARM_INT(rmac_util_period, 5);
S2IO_PARM_INT(l3l4hdr_size, 128);
/* 0 is no steering, 1 is Priority steering, 2 is Default steering */
S2IO_PARM_INT(tx_steering_type, TX_DEFAULT_STEERING);
/* Frequency of Rx desc syncs expressed as power of 2 */
S2IO_PARM_INT(rxsync_frequency, 3);
/* Interrupt type. Values can be 0(INTA), 2(MSI_X) */
S2IO_PARM_INT(intr_type, 2);
/* Large receive offload feature */
static unsigned int lro_enable;
module_param_named(lro, lro_enable, uint, 0);

/* Max pkts to be aggregated by LRO at one time. If not specified,
 * aggregation happens until we hit max IP pkt size(64K)
 */
S2IO_PARM_INT(lro_max_pkts, 0xFFFF);
S2IO_PARM_INT(indicate_max_pkts, 0);

S2IO_PARM_INT(napi, 1);
S2IO_PARM_INT(ufo, 0);
S2IO_PARM_INT(vlan_tag_strip, NO_STRIP_IN_PROMISC);

static unsigned int tx_fifo_len[MAX_TX_FIFOS] =
{DEFAULT_FIFO_0_LEN, [1 ...(MAX_TX_FIFOS - 1)] = DEFAULT_FIFO_1_7_LEN};
static unsigned int rx_ring_sz[MAX_RX_RINGS] =
{[0 ...(MAX_RX_RINGS - 1)] = SMALL_BLK_CNT};
static unsigned int rts_frm_len[MAX_RX_RINGS] =
{[0 ...(MAX_RX_RINGS - 1)] = 0 };

module_param_array(tx_fifo_len, uint, NULL, 0);
module_param_array(rx_ring_sz, uint, NULL, 0);
module_param_array(rts_frm_len, uint, NULL, 0);

/*
 * S2IO device table.
 * This table lists all the devices that this driver supports.
 */
static struct pci_device_id s2io_tbl[] __devinitdata = {
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_S2IO_WIN,
	 PCI_ANY_ID, PCI_ANY_ID},
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_S2IO_UNI,
	 PCI_ANY_ID, PCI_ANY_ID},
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_HERC_WIN,
	 PCI_ANY_ID, PCI_ANY_ID},
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_HERC_UNI,
	 PCI_ANY_ID, PCI_ANY_ID},
	{0,}
};

MODULE_DEVICE_TABLE(pci, s2io_tbl);

static struct pci_error_handlers s2io_err_handler = {
	.error_detected = s2io_io_error_detected,
	.slot_reset = s2io_io_slot_reset,
	.resume = s2io_io_resume,
};

static struct pci_driver s2io_driver = {
	.name = "S2IO",
	.id_table = s2io_tbl,
	.probe = s2io_init_nic,
	.remove = __devexit_p(s2io_rem_nic),
	.err_handler = &s2io_err_handler,
};

/* A simplifier macro used both by init and free shared_mem Fns(). */
#define TXD_MEM_PAGE_CNT(len, per_each) ((len+per_each - 1) / per_each)

/* netqueue manipulation helper functions */
static inline void s2io_stop_all_tx_queue(struct s2io_nic *sp)
{
	if (!sp->config.multiq) {
		int i;

		for (i = 0; i < sp->config.tx_fifo_num; i++)
			sp->mac_control.fifos[i].queue_state = FIFO_QUEUE_STOP;
	}
	netif_tx_stop_all_queues(sp->dev);
}

static inline void s2io_stop_tx_queue(struct s2io_nic *sp, int fifo_no)
{
	if (!sp->config.multiq)
		sp->mac_control.fifos[fifo_no].queue_state =
			FIFO_QUEUE_STOP;

	netif_tx_stop_all_queues(sp->dev);
}

static inline void s2io_start_all_tx_queue(struct s2io_nic *sp)
{
	if (!sp->config.multiq) {
		int i;

		for (i = 0; i < sp->config.tx_fifo_num; i++)
			sp->mac_control.fifos[i].queue_state = FIFO_QUEUE_START;
	}
	netif_tx_start_all_queues(sp->dev);
}

static inline void s2io_start_tx_queue(struct s2io_nic *sp, int fifo_no)
{
	if (!sp->config.multiq)
		sp->mac_control.fifos[fifo_no].queue_state =
			FIFO_QUEUE_START;

	netif_tx_start_all_queues(sp->dev);
}

static inline void s2io_wake_all_tx_queue(struct s2io_nic *sp)
{
	if (!sp->config.multiq) {
		int i;

		for (i = 0; i < sp->config.tx_fifo_num; i++)
			sp->mac_control.fifos[i].queue_state = FIFO_QUEUE_START;
	}
	netif_tx_wake_all_queues(sp->dev);
}

static inline void s2io_wake_tx_queue(
	struct fifo_info *fifo, int cnt, u8 multiq)
{

	if (multiq) {
		if (cnt && __netif_subqueue_stopped(fifo->dev, fifo->fifo_no))
			netif_wake_subqueue(fifo->dev, fifo->fifo_no);
	} else if (cnt && (fifo->queue_state == FIFO_QUEUE_STOP)) {
		if (netif_queue_stopped(fifo->dev)) {
			fifo->queue_state = FIFO_QUEUE_START;
			netif_wake_queue(fifo->dev);
		}
	}
}

/**
 * init_shared_mem - Allocation and Initialization of Memory
 * @nic: Device private variable.
 * Description: The function allocates all the memory areas shared
 * between the NIC and the driver. This includes Tx descriptors,
 * Rx descriptors and the statistics block.
 */

static int init_shared_mem(struct s2io_nic *nic)
{
	u32 size;
	void *tmp_v_addr, *tmp_v_addr_next;
	dma_addr_t tmp_p_addr, tmp_p_addr_next;
	struct RxD_block *pre_rxd_blk = NULL;
	int i, j, blk_cnt;
	int lst_size, lst_per_page;
	struct net_device *dev = nic->dev;
	unsigned long tmp;
	struct buffAdd *ba;
	struct config_param *config = &nic->config;
	struct mac_info *mac_control = &nic->mac_control;
	unsigned long long mem_allocated = 0;

	/* Allocation and initialization of TXDLs in FIFOs */
	size = 0;
	for (i = 0; i < config->tx_fifo_num; i++) {
		struct tx_fifo_config *tx_cfg = &config->tx_cfg[i];

		size += tx_cfg->fifo_len;
	}
	if (size > MAX_AVAILABLE_TXDS) {
		DBG_PRINT(ERR_DBG,
			  "Too many TxDs requested: %d, max supported: %d\n",
			  size, MAX_AVAILABLE_TXDS);
		return -EINVAL;
	}

	size = 0;
	for (i = 0; i < config->tx_fifo_num; i++) {
		struct tx_fifo_config *tx_cfg = &config->tx_cfg[i];

		size = tx_cfg->fifo_len;
		/*
		 * Legal values are from 2 to 8192
		 */
		if (size < 2) {
			DBG_PRINT(ERR_DBG, "Fifo %d: Invalid length (%d) - "
				  "Valid lengths are 2 through 8192\n",
				  i, size);
			return -EINVAL;
		}
	}

	lst_size = (sizeof(struct TxD) * config->max_txds);
	lst_per_page = PAGE_SIZE / lst_size;

	for (i = 0; i < config->tx_fifo_num; i++) {
		struct fifo_info *fifo = &mac_control->fifos[i];
		struct tx_fifo_config *tx_cfg = &config->tx_cfg[i];
		int fifo_len = tx_cfg->fifo_len;
		int list_holder_size = fifo_len * sizeof(struct list_info_hold);

		fifo->list_info = kzalloc(list_holder_size, GFP_KERNEL);
		if (!fifo->list_info) {
			DBG_PRINT(INFO_DBG, "Malloc failed for list_info\n");
			return -ENOMEM;
		}
		mem_allocated += list_holder_size;
	}
	for (i = 0; i < config->tx_fifo_num; i++) {
		int page_num = TXD_MEM_PAGE_CNT(config->tx_cfg[i].fifo_len,
						lst_per_page);
		struct fifo_info *fifo = &mac_control->fifos[i];
		struct tx_fifo_config *tx_cfg = &config->tx_cfg[i];

		fifo->tx_curr_put_info.offset = 0;
		fifo->tx_curr_put_info.fifo_len = tx_cfg->fifo_len - 1;
		fifo->tx_curr_get_info.offset = 0;
		fifo->tx_curr_get_info.fifo_len = tx_cfg->fifo_len - 1;
		fifo->fifo_no = i;
		fifo->nic = nic;
		fifo->max_txds = MAX_SKB_FRAGS + 2;
		fifo->dev = dev;

		for (j = 0; j < page_num; j++) {
			int k = 0;
			dma_addr_t tmp_p;
			void *tmp_v;
			tmp_v = pci_alloc_consistent(nic->pdev,
						     PAGE_SIZE, &tmp_p);
			if (!tmp_v) {
				DBG_PRINT(INFO_DBG,
					  "pci_alloc_consistent failed for TxDL\n");
				return -ENOMEM;
			}
			/* If we got a zero DMA address(can happen on
			 * certain platforms like PPC), reallocate.
			 * Store virtual address of page we don't want,
			 * to be freed later.
			 */
			if (!tmp_p) {
				mac_control->zerodma_virt_addr = tmp_v;
				DBG_PRINT(INIT_DBG,
					  "%s: Zero DMA address for TxDL. "
					  "Virtual address %p\n",
					  dev->name, tmp_v);
				tmp_v = pci_alloc_consistent(nic->pdev,
							     PAGE_SIZE, &tmp_p);
				if (!tmp_v) {
					DBG_PRINT(INFO_DBG,
						  "pci_alloc_consistent failed for TxDL\n");
					return -ENOMEM;
				}
				mem_allocated += PAGE_SIZE;
			}
			while (k < lst_per_page) {
				int l = (j * lst_per_page) + k;
				if (l == tx_cfg->fifo_len)
					break;
				fifo->list_info[l].list_virt_addr =
					tmp_v + (k * lst_size);
				fifo->list_info[l].list_phy_addr =
					tmp_p + (k * lst_size);
				k++;
			}
		}
	}

	for (i = 0; i < config->tx_fifo_num; i++) {
		struct fifo_info *fifo = &mac_control->fifos[i];
		struct tx_fifo_config *tx_cfg = &config->tx_cfg[i];

		size = tx_cfg->fifo_len;
		fifo->ufo_in_band_v = kcalloc(size, sizeof(u64), GFP_KERNEL);
		if (!fifo->ufo_in_band_v)
			return -ENOMEM;
		mem_allocated += (size * sizeof(u64));
	}

	/* Allocation and initialization of RXDs in Rings */
	size = 0;
	for (i = 0; i < config->rx_ring_num; i++) {
		struct rx_ring_config *rx_cfg = &config->rx_cfg[i];
		struct ring_info *ring = &mac_control->rings[i];

		if (rx_cfg->num_rxd % (rxd_count[nic->rxd_mode] + 1)) {
			DBG_PRINT(ERR_DBG, "%s: Ring%d RxD count is not a "
				  "multiple of RxDs per Block\n",
				  dev->name, i);
			return FAILURE;
		}
		size += rx_cfg->num_rxd;
		ring->block_count = rx_cfg->num_rxd /
			(rxd_count[nic->rxd_mode] + 1);
		ring->pkt_cnt = rx_cfg->num_rxd - ring->block_count;
	}
	if (nic->rxd_mode == RXD_MODE_1)
		size = (size * (sizeof(struct RxD1)));
	else
		size = (size * (sizeof(struct RxD3)));

	for (i = 0; i < config->rx_ring_num; i++) {
		struct rx_ring_config *rx_cfg = &config->rx_cfg[i];
		struct ring_info *ring = &mac_control->rings[i];

		ring->rx_curr_get_info.block_index = 0;
		ring->rx_curr_get_info.offset = 0;
		ring->rx_curr_get_info.ring_len = rx_cfg->num_rxd - 1;
		ring->rx_curr_put_info.block_index = 0;
		ring->rx_curr_put_info.offset = 0;
		ring->rx_curr_put_info.ring_len = rx_cfg->num_rxd - 1;
		ring->nic = nic;
		ring->ring_no = i;
		ring->lro = lro_enable;

		blk_cnt = rx_cfg->num_rxd / (rxd_count[nic->rxd_mode] + 1);
		/*  Allocating all the Rx blocks */
		for (j = 0; j < blk_cnt; j++) {
			struct rx_block_info *rx_blocks;
			int l;

			rx_blocks = &ring->rx_blocks[j];
			size = SIZE_OF_BLOCK;	/* size is always page size */
			tmp_v_addr = pci_alloc_consistent(nic->pdev, size,
							  &tmp_p_addr);
			if (tmp_v_addr == NULL) {
				/*
				 * In case of failure, free_shared_mem()
				 * is called, which should free any
				 * memory that was alloced till the
				 * failure happened.
				 */
				rx_blocks->block_virt_addr = tmp_v_addr;
				return -ENOMEM;
			}
			mem_allocated += size;
			memset(tmp_v_addr, 0, size);

			size = sizeof(struct rxd_info) *
				rxd_count[nic->rxd_mode];
			rx_blocks->block_virt_addr = tmp_v_addr;
			rx_blocks->block_dma_addr = tmp_p_addr;
			rx_blocks->rxds = kmalloc(size,  GFP_KERNEL);
			if (!rx_blocks->rxds)
				return -ENOMEM;
			mem_allocated += size;
			for (l = 0; l < rxd_count[nic->rxd_mode]; l++) {
				rx_blocks->rxds[l].virt_addr =
					rx_blocks->block_virt_addr +
					(rxd_size[nic->rxd_mode] * l);
				rx_blocks->rxds[l].dma_addr =
					rx_blocks->block_dma_addr +
					(rxd_size[nic->rxd_mode] * l);
			}
		}
		/* Interlinking all Rx Blocks */
		for (j = 0; j < blk_cnt; j++) {
			int next = (j + 1) % blk_cnt;
			tmp_v_addr = ring->rx_blocks[j].block_virt_addr;
			tmp_v_addr_next = ring->rx_blocks[next].block_virt_addr;
			tmp_p_addr = ring->rx_blocks[j].block_dma_addr;
			tmp_p_addr_next = ring->rx_blocks[next].block_dma_addr;

			pre_rxd_blk = (struct RxD_block *)tmp_v_addr;
			pre_rxd_blk->reserved_2_pNext_RxD_block =
				(unsigned long)tmp_v_addr_next;
			pre_rxd_blk->pNext_RxD_Blk_physical =
				(u64)tmp_p_addr_next;
		}
	}
	if (nic->rxd_mode == RXD_MODE_3B) {
		/*
		 * Allocation of Storages for buffer addresses in 2BUFF mode
		 * and the buffers as well.
		 */
		for (i = 0; i < config->rx_ring_num; i++) {
			struct rx_ring_config *rx_cfg = &config->rx_cfg[i];
			struct ring_info *ring = &mac_control->rings[i];

			blk_cnt = rx_cfg->num_rxd /
				(rxd_count[nic->rxd_mode] + 1);
			size = sizeof(struct buffAdd *) * blk_cnt;
			ring->ba = kmalloc(size, GFP_KERNEL);
			if (!ring->ba)
				return -ENOMEM;
			mem_allocated += size;
			for (j = 0; j < blk_cnt; j++) {
				int k = 0;

				size = sizeof(struct buffAdd) *
					(rxd_count[nic->rxd_mode] + 1);
				ring->ba[j] = kmalloc(size, GFP_KERNEL);
				if (!ring->ba[j])
					return -ENOMEM;
				mem_allocated += size;
				while (k != rxd_count[nic->rxd_mode]) {
					ba = &ring->ba[j][k];
					size = BUF0_LEN + ALIGN_SIZE;
					ba->ba_0_org = kmalloc(size, GFP_KERNEL);
					if (!ba->ba_0_org)
						return -ENOMEM;
					mem_allocated += size;
					tmp = (unsigned long)ba->ba_0_org;
					tmp += ALIGN_SIZE;
					tmp &= ~((unsigned long)ALIGN_SIZE);
					ba->ba_0 = (void *)tmp;

					size = BUF1_LEN + ALIGN_SIZE;
					ba->ba_1_org = kmalloc(size, GFP_KERNEL);
					if (!ba->ba_1_org)
						return -ENOMEM;
					mem_allocated += size;
					tmp = (unsigned long)ba->ba_1_org;
					tmp += ALIGN_SIZE;
					tmp &= ~((unsigned long)ALIGN_SIZE);
					ba->ba_1 = (void *)tmp;
					k++;
				}
			}
		}
	}

	/* Allocation and initialization of Statistics block */
	size = sizeof(struct stat_block);
	mac_control->stats_mem =
		pci_alloc_consistent(nic->pdev, size,
				     &mac_control->stats_mem_phy);

	if (!mac_control->stats_mem) {
		/*
		 * In case of failure, free_shared_mem() is called, which
		 * should free any memory that was alloced till the
		 * failure happened.
		 */
		return -ENOMEM;
	}
	mem_allocated += size;
	mac_control->stats_mem_sz = size;

	tmp_v_addr = mac_control->stats_mem;
	mac_control->stats_info = (struct stat_block *)tmp_v_addr;
	memset(tmp_v_addr, 0, size);
	DBG_PRINT(INIT_DBG, "%s: Ring Mem PHY: 0x%llx\n", dev->name,
		  (unsigned long long)tmp_p_addr);
	mac_control->stats_info->sw_stat.mem_allocated += mem_allocated;
	return SUCCESS;
}

/**
 * free_shared_mem - Free the allocated Memory
 * @nic:  Device private variable.
 * Description: This function is to free all memory locations allocated by
 * the init_shared_mem() function and return it to the kernel.
 */

static void free_shared_mem(struct s2io_nic *nic)
{
	int i, j, blk_cnt, size;
	void *tmp_v_addr;
	dma_addr_t tmp_p_addr;
	int lst_size, lst_per_page;
	struct net_device *dev;
	int page_num = 0;
	struct config_param *config;
	struct mac_info *mac_control;
	struct stat_block *stats;
	struct swStat *swstats;

	if (!nic)
		return;

	dev = nic->dev;

	config = &nic->config;
	mac_control = &nic->mac_control;
	stats = mac_control->stats_info;
	swstats = &stats->sw_stat;

	lst_size = sizeof(struct TxD) * config->max_txds;
	lst_per_page = PAGE_SIZE / lst_size;

	for (i = 0; i < config->tx_fifo_num; i++) {
		struct fifo_info *fifo = &mac_control->fifos[i];
		struct tx_fifo_config *tx_cfg = &config->tx_cfg[i];

		page_num = TXD_MEM_PAGE_CNT(tx_cfg->fifo_len, lst_per_page);
		for (j = 0; j < page_num; j++) {
			int mem_blks = (j * lst_per_page);
			struct list_info_hold *fli;

			if (!fifo->list_info)
				return;

			fli = &fifo->list_info[mem_blks];
			if (!fli->list_virt_addr)
				break;
			pci_free_consistent(nic->pdev, PAGE_SIZE,
					    fli->list_virt_addr,
					    fli->list_phy_addr);
			swstats->mem_freed += PAGE_SIZE;
		}
		/* If we got a zero DMA address during allocation,
		 * free the page now
		 */
		if (mac_control->zerodma_virt_addr) {
			pci_free_consistent(nic->pdev, PAGE_SIZE,
					    mac_control->zerodma_virt_addr,
					    (dma_addr_t)0);
			DBG_PRINT(INIT_DBG,
				  "%s: Freeing TxDL with zero DMA address. "
				  "Virtual address %p\n",
				  dev->name, mac_control->zerodma_virt_addr);
			swstats->mem_freed += PAGE_SIZE;
		}
		kfree(fifo->list_info);
		swstats->mem_freed += tx_cfg->fifo_len *
			sizeof(struct list_info_hold);
	}

	size = SIZE_OF_BLOCK;
	for (i = 0; i < config->rx_ring_num; i++) {
		struct ring_info *ring = &mac_control->rings[i];

		blk_cnt = ring->block_count;
		for (j = 0; j < blk_cnt; j++) {
			tmp_v_addr = ring->rx_blocks[j].block_virt_addr;
			tmp_p_addr = ring->rx_blocks[j].block_dma_addr;
			if (tmp_v_addr == NULL)
				break;
			pci_free_consistent(nic->pdev, size,
					    tmp_v_addr, tmp_p_addr);
			swstats->mem_freed += size;
			kfree(ring->rx_blocks[j].rxds);
			swstats->mem_freed += sizeof(struct rxd_info) *
				rxd_count[nic->rxd_mode];
		}
	}

	if (nic->rxd_mode == RXD_MODE_3B) {
		/* Freeing buffer storage addresses in 2BUFF mode. */
		for (i = 0; i < config->rx_ring_num; i++) {
			struct rx_ring_config *rx_cfg = &config->rx_cfg[i];
			struct ring_info *ring = &mac_control->rings[i];

			blk_cnt = rx_cfg->num_rxd /
				(rxd_count[nic->rxd_mode] + 1);
			for (j = 0; j < blk_cnt; j++) {
				int k = 0;
				if (!ring->ba[j])
					continue;
				while (k != rxd_count[nic->rxd_mode]) {
					struct buffAdd *ba = &ring->ba[j][k];
					kfree(ba->ba_0_org);
					swstats->mem_freed +=
						BUF0_LEN + ALIGN_SIZE;
					kfree(ba->ba_1_org);
					swstats->mem_freed +=
						BUF1_LEN + ALIGN_SIZE;
					k++;
				}
				kfree(ring->ba[j]);
				swstats->mem_freed += sizeof(struct buffAdd) *
					(rxd_count[nic->rxd_mode] + 1);
			}
			kfree(ring->ba);
			swstats->mem_freed += sizeof(struct buffAdd *) *
				blk_cnt;
		}
	}

	for (i = 0; i < nic->config.tx_fifo_num; i++) {
		struct fifo_info *fifo = &mac_control->fifos[i];
		struct tx_fifo_config *tx_cfg = &config->tx_cfg[i];

		if (fifo->ufo_in_band_v) {
			swstats->mem_freed += tx_cfg->fifo_len *
				sizeof(u64);
			kfree(fifo->ufo_in_band_v);
		}
	}

	if (mac_control->stats_mem) {
		swstats->mem_freed += mac_control->stats_mem_sz;
		pci_free_consistent(nic->pdev,
				    mac_control->stats_mem_sz,
				    mac_control->stats_mem,
				    mac_control->stats_mem_phy);
	}
}

/**
 * s2io_verify_pci_mode -
 */

static int s2io_verify_pci_mode(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 val64 = 0;
	int     mode;

	val64 = readq(&bar0->pci_mode);
	mode = (u8)GET_PCI_MODE(val64);

	if (val64 & PCI_MODE_UNKNOWN_MODE)
		return -1;      /* Unknown PCI mode */
	return mode;
}

#define NEC_VENID   0x1033
#define NEC_DEVID   0x0125
static int s2io_on_nec_bridge(struct pci_dev *s2io_pdev)
{
	struct pci_dev *tdev = NULL;
	while ((tdev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, tdev)) != NULL) {
		if (tdev->vendor == NEC_VENID && tdev->device == NEC_DEVID) {
			if (tdev->bus == s2io_pdev->bus->parent) {
				pci_dev_put(tdev);
				return 1;
			}
		}
	}
	return 0;
}

static int bus_speed[8] = {33, 133, 133, 200, 266, 133, 200, 266};
/**
 * s2io_print_pci_mode -
 */
static int s2io_print_pci_mode(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 val64 = 0;
	int	mode;
	struct config_param *config = &nic->config;
	const char *pcimode;

	val64 = readq(&bar0->pci_mode);
	mode = (u8)GET_PCI_MODE(val64);

	if (val64 & PCI_MODE_UNKNOWN_MODE)
		return -1;	/* Unknown PCI mode */

	config->bus_speed = bus_speed[mode];

	if (s2io_on_nec_bridge(nic->pdev)) {
		DBG_PRINT(ERR_DBG, "%s: Device is on PCI-E bus\n",
			  nic->dev->name);
		return mode;
	}

	switch (mode) {
	case PCI_MODE_PCI_33:
		pcimode = "33MHz PCI bus";
		break;
	case PCI_MODE_PCI_66:
		pcimode = "66MHz PCI bus";
		break;
	case PCI_MODE_PCIX_M1_66:
		pcimode = "66MHz PCIX(M1) bus";
		break;
	case PCI_MODE_PCIX_M1_100:
		pcimode = "100MHz PCIX(M1) bus";
		break;
	case PCI_MODE_PCIX_M1_133:
		pcimode = "133MHz PCIX(M1) bus";
		break;
	case PCI_MODE_PCIX_M2_66:
		pcimode = "133MHz PCIX(M2) bus";
		break;
	case PCI_MODE_PCIX_M2_100:
		pcimode = "200MHz PCIX(M2) bus";
		break;
	case PCI_MODE_PCIX_M2_133:
		pcimode = "266MHz PCIX(M2) bus";
		break;
	default:
		pcimode = "unsupported bus!";
		mode = -1;
	}

	DBG_PRINT(ERR_DBG, "%s: Device is on %d bit %s\n",
		  nic->dev->name, val64 & PCI_MODE_32_BITS ? 32 : 64, pcimode);

	return mode;
}

/**
 *  init_tti - Initialization transmit traffic interrupt scheme
 *  @nic: device private variable
 *  @link: link status (UP/DOWN) used to enable/disable continuous
 *  transmit interrupts
 *  Description: The function configures transmit traffic interrupts
 *  Return Value:  SUCCESS on success and
 *  '-1' on failure
 */

static int init_tti(struct s2io_nic *nic, int link)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 val64 = 0;
	int i;
	struct config_param *config = &nic->config;

	for (i = 0; i < config->tx_fifo_num; i++) {
		/*
		 * TTI Initialization. Default Tx timer gets us about
		 * 250 interrupts per sec. Continuous interrupts are enabled
		 * by default.
		 */
		if (nic->device_type == XFRAME_II_DEVICE) {
			int count = (nic->config.bus_speed * 125)/2;
			val64 = TTI_DATA1_MEM_TX_TIMER_VAL(count);
		} else
			val64 = TTI_DATA1_MEM_TX_TIMER_VAL(0x2078);

		val64 |= TTI_DATA1_MEM_TX_URNG_A(0xA) |
			TTI_DATA1_MEM_TX_URNG_B(0x10) |
			TTI_DATA1_MEM_TX_URNG_C(0x30) |
			TTI_DATA1_MEM_TX_TIMER_AC_EN;
		if (i == 0)
			if (use_continuous_tx_intrs && (link == LINK_UP))
				val64 |= TTI_DATA1_MEM_TX_TIMER_CI_EN;
		writeq(val64, &bar0->tti_data1_mem);

		if (nic->config.intr_type == MSI_X) {
			val64 = TTI_DATA2_MEM_TX_UFC_A(0x10) |
				TTI_DATA2_MEM_TX_UFC_B(0x100) |
				TTI_DATA2_MEM_TX_UFC_C(0x200) |
				TTI_DATA2_MEM_TX_UFC_D(0x300);
		} else {
			if ((nic->config.tx_steering_type ==
			     TX_DEFAULT_STEERING) &&
			    (config->tx_fifo_num > 1) &&
			    (i >= nic->udp_fifo_idx) &&
			    (i < (nic->udp_fifo_idx +
				  nic->total_udp_fifos)))
				val64 = TTI_DATA2_MEM_TX_UFC_A(0x50) |
					TTI_DATA2_MEM_TX_UFC_B(0x80) |
					TTI_DATA2_MEM_TX_UFC_C(0x100) |
					TTI_DATA2_MEM_TX_UFC_D(0x120);
			else
				val64 = TTI_DATA2_MEM_TX_UFC_A(0x10) |
					TTI_DATA2_MEM_TX_UFC_B(0x20) |
					TTI_DATA2_MEM_TX_UFC_C(0x40) |
					TTI_DATA2_MEM_TX_UFC_D(0x80);
		}

		writeq(val64, &bar0->tti_data2_mem);

		val64 = TTI_CMD_MEM_WE |
			TTI_CMD_MEM_STROBE_NEW_CMD |
			TTI_CMD_MEM_OFFSET(i);
		writeq(val64, &bar0->tti_command_mem);

		if (wait_for_cmd_complete(&bar0->tti_command_mem,
					  TTI_CMD_MEM_STROBE_NEW_CMD,
					  S2IO_BIT_RESET) != SUCCESS)
			return FAILURE;
	}

	return SUCCESS;
}

/**
 *  init_nic - Initialization of hardware
 *  @nic: device private variable
 *  Description: The function sequentially configures every block
 *  of the H/W from their reset values.
 *  Return Value:  SUCCESS on success and
 *  '-1' on failure (endian settings incorrect).
 */

static int init_nic(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	struct net_device *dev = nic->dev;
	register u64 val64 = 0;
	void __iomem *add;
	u32 time;
	int i, j;
	int dtx_cnt = 0;
	unsigned long long mem_share;
	int mem_size;
	struct config_param *config = &nic->config;
	struct mac_info *mac_control = &nic->mac_control;

	/* to set the swapper controle on the card */
	if (s2io_set_swapper(nic)) {
		DBG_PRINT(ERR_DBG, "ERROR: Setting Swapper failed\n");
		return -EIO;
	}

	/*
	 * Herc requires EOI to be removed from reset before XGXS, so..
	 */
	if (nic->device_type & XFRAME_II_DEVICE) {
		val64 = 0xA500000000ULL;
		writeq(val64, &bar0->sw_reset);
		msleep(500);
		val64 = readq(&bar0->sw_reset);
	}

	/* Remove XGXS from reset state */
	val64 = 0;
	writeq(val64, &bar0->sw_reset);
	msleep(500);
	val64 = readq(&bar0->sw_reset);

	/* Ensure that it's safe to access registers by checking
	 * RIC_RUNNING bit is reset. Check is valid only for XframeII.
	 */
	if (nic->device_type == XFRAME_II_DEVICE) {
		for (i = 0; i < 50; i++) {
			val64 = readq(&bar0->adapter_status);
			if (!(val64 & ADAPTER_STATUS_RIC_RUNNING))
				break;
			msleep(10);
		}
		if (i == 50)
			return -ENODEV;
	}

	/*  Enable Receiving broadcasts */
	add = &bar0->mac_cfg;
	val64 = readq(&bar0->mac_cfg);
	val64 |= MAC_RMAC_BCAST_ENABLE;
	writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
	writel((u32)val64, add);
	writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
	writel((u32) (val64 >> 32), (add + 4));

	/* Read registers in all blocks */
	val64 = readq(&bar0->mac_int_mask);
	val64 = readq(&bar0->mc_int_mask);
	val64 = readq(&bar0->xgxs_int_mask);

	/*  Set MTU */
	val64 = dev->mtu;
	writeq(vBIT(val64, 2, 14), &bar0->rmac_max_pyld_len);

	if (nic->device_type & XFRAME_II_DEVICE) {
		while (herc_act_dtx_cfg[dtx_cnt] != END_SIGN) {
			SPECIAL_REG_WRITE(herc_act_dtx_cfg[dtx_cnt],
					  &bar0->dtx_control, UF);
			if (dtx_cnt & 0x1)
				msleep(1); /* Necessary!! */
			dtx_cnt++;
		}
	} else {
		while (xena_dtx_cfg[dtx_cnt] != END_SIGN) {
			SPECIAL_REG_WRITE(xena_dtx_cfg[dtx_cnt],
					  &bar0->dtx_control, UF);
			val64 = readq(&bar0->dtx_control);
			dtx_cnt++;
		}
	}

	/*  Tx DMA Initialization */
	val64 = 0;
	writeq(val64, &bar0->tx_fifo_partition_0);
	writeq(val64, &bar0->tx_fifo_partition_1);
	writeq(val64, &bar0->tx_fifo_partition_2);
	writeq(val64, &bar0->tx_fifo_partition_3);

	for (i = 0, j = 0; i < config->tx_fifo_num; i++) {
		struct tx_fifo_config *tx_cfg = &config->tx_cfg[i];

		val64 |= vBIT(tx_cfg->fifo_len - 1, ((j * 32) + 19), 13) |
			vBIT(tx_cfg->fifo_priority, ((j * 32) + 5), 3);

		if (i == (config->tx_fifo_num - 1)) {
			if (i % 2 == 0)
				i++;
		}

		switch (i) {
		case 1:
			writeq(val64, &bar0->tx_fifo_partition_0);
			val64 = 0;
			j = 0;
			break;
		case 3:
			writeq(val64, &bar0->tx_fifo_partition_1);
			val64 = 0;
			j = 0;
			break;
		case 5:
			writeq(val64, &bar0->tx_fifo_partition_2);
			val64 = 0;
			j = 0;
			break;
		case 7:
			writeq(val64, &bar0->tx_fifo_partition_3);
			val64 = 0;
			j = 0;
			break;
		default:
			j++;
			break;
		}
	}

	/*
	 * Disable 4 PCCs for Xena1, 2 and 3 as per H/W bug
	 * SXE-008 TRANSMIT DMA ARBITRATION ISSUE.
	 */
	if ((nic->device_type == XFRAME_I_DEVICE) && (nic->pdev->revision < 4))
		writeq(PCC_ENABLE_FOUR, &bar0->pcc_enable);

	val64 = readq(&bar0->tx_fifo_partition_0);
	DBG_PRINT(INIT_DBG, "Fifo partition at: 0x%p is: 0x%llx\n",
		  &bar0->tx_fifo_partition_0, (unsigned long long)val64);

	/*
	 * Initialization of Tx_PA_CONFIG register to ignore packet
	 * integrity checking.
	 */
	val64 = readq(&bar0->tx_pa_cfg);
	val64 |= TX_PA_CFG_IGNORE_FRM_ERR |
		TX_PA_CFG_IGNORE_SNAP_OUI |
		TX_PA_CFG_IGNORE_LLC_CTRL |
		TX_PA_CFG_IGNORE_L2_ERR;
	writeq(val64, &bar0->tx_pa_cfg);

	/* Rx DMA intialization. */
	val64 = 0;
	for (i = 0; i < config->rx_ring_num; i++) {
		struct rx_ring_config *rx_cfg = &config->rx_cfg[i];

		val64 |= vBIT(rx_cfg->ring_priority, (5 + (i * 8)), 3);
	}
	writeq(val64, &bar0->rx_queue_priority);

	/*
	 * Allocating equal share of memory to all the
	 * configured Rings.
	 */
	val64 = 0;
	if (nic->device_type & XFRAME_II_DEVICE)
		mem_size = 32;
	else
		mem_size = 64;

	for (i = 0; i < config->rx_ring_num; i++) {
		switch (i) {
		case 0:
			mem_share = (mem_size / config->rx_ring_num +
				     mem_size % config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q0_SZ(mem_share);
			continue;
		case 1:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q1_SZ(mem_share);
			continue;
		case 2:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q2_SZ(mem_share);
			continue;
		case 3:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q3_SZ(mem_share);
			continue;
		case 4:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q4_SZ(mem_share);
			continue;
		case 5:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q5_SZ(mem_share);
			continue;
		case 6:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q6_SZ(mem_share);
			continue;
		case 7:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q7_SZ(mem_share);
			continue;
		}
	}
	writeq(val64, &bar0->rx_queue_cfg);

	/*
	 * Filling Tx round robin registers
	 * as per the number of FIFOs for equal scheduling priority
	 */
	switch (config->tx_fifo_num) {
	case 1:
		val64 = 0x0;
		writeq(val64, &bar0->tx_w_round_robin_0);
		writeq(val64, &bar0->tx_w_round_robin_1);
		writeq(val64, &bar0->tx_w_round_robin_2);
		writeq(val64, &bar0->tx_w_round_robin_3);
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 2:
		val64 = 0x0001000100010001ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		writeq(val64, &bar0->tx_w_round_robin_1);
		writeq(val64, &bar0->tx_w_round_robin_2);
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0001000100000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 3:
		val64 = 0x0001020001020001ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		val64 = 0x0200010200010200ULL;
		writeq(val64, &bar0->tx_w_round_robin_1);
		val64 = 0x0102000102000102ULL;
		writeq(val64, &bar0->tx_w_round_robin_2);
		val64 = 0x0001020001020001ULL;
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0200010200000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 4:
		val64 = 0x0001020300010203ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		writeq(val64, &bar0->tx_w_round_robin_1);
		writeq(val64, &bar0->tx_w_round_robin_2);
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0001020300000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 5:
		val64 = 0x0001020304000102ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		val64 = 0x0304000102030400ULL;
		writeq(val64, &bar0->tx_w_round_robin_1);
		val64 = 0x0102030400010203ULL;
		writeq(val64, &bar0->tx_w_round_robin_2);
		val64 = 0x0400010203040001ULL;
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0203040000000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 6:
		val64 = 0x0001020304050001ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		val64 = 0x0203040500010203ULL;
		writeq(val64, &bar0->tx_w_round_robin_1);
		val64 = 0x0405000102030405ULL;
		writeq(val64, &bar0->tx_w_round_robin_2);
		val64 = 0x0001020304050001ULL;
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0203040500000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 7:
		val64 = 0x0001020304050600ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		val64 = 0x0102030405060001ULL;
		writeq(val64, &bar0->tx_w_round_robin_1);
		val64 = 0x0203040506000102ULL;
		writeq(val64, &bar0->tx_w_round_robin_2);
		val64 = 0x0304050600010203ULL;
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0405060000000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 8:
		val64 = 0x0001020304050607ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		writeq(val64, &bar0->tx_w_round_robin_1);
		writeq(val64, &bar0->tx_w_round_robin_2);
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0001020300000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	}

	/* Enable all configured Tx FIFO partitions */
	val64 = readq(&bar0->tx_fifo_partition_0);
	val64 |= (TX_FIFO_PARTITION_EN);
	writeq(val64, &bar0->tx_fifo_partition_0);

	/* Filling the Rx round robin registers as per the
	 * number of Rings and steering based on QoS with
	 * equal priority.
	 */
	switch (config->rx_ring_num) {
	case 1:
		val64 = 0x0;
		writeq(val64, &bar0->rx_w_round_robin_0);
		writeq(val64, &bar0->rx_w_round_robin_1);
		writeq(val64, &bar0->rx_w_round_robin_2);
		writeq(val64, &bar0->rx_w_round_robin_3);
		writeq(val64, &bar0->rx_w_round_robin_4);

		val64 = 0x8080808080808080ULL;
		writeq(val64, &bar0->rts_qos_steering);
		break;
	case 2:
		val64 = 0x0001000100010001ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		writeq(val64, &bar0->rx_w_round_robin_1);
		writeq(val64, &bar0->rx_w_round_robin_2);
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0001000100000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		val64 = 0x8080808040404040ULL;
		writeq(val64, &bar0->rts_qos_steering);
		break;
	case 3:
		val64 = 0x0001020001020001ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		val64 = 0x0200010200010200ULL;
		writeq(val64, &bar0->rx_w_round_robin_1);
		val64 = 0x0102000102000102ULL;
		writeq(val64, &bar0->rx_w_round_robin_2);
		val64 = 0x0001020001020001ULL;
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0200010200000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		val64 = 0x8080804040402020ULL;
		writeq(val64, &bar0->rts_qos_steering);
		break;
	case 4:
		val64 = 0x0001020300010203ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		writeq(val64, &bar0->rx_w_round_robin_1);
		writeq(val64, &bar0->rx_w_round_robin_2);
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0001020300000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		val64 = 0x8080404020201010ULL;
		writeq(val64, &bar0->rts_qos_steering);
		break;
	case 5:
		val64 = 0x0001020304000102ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		val64 = 0x0304000102030400ULL;
		writeq(val64, &bar0->rx_w_round_robin_1);
		val64 = 0x0102030400010203ULL;
		writeq(val64, &bar0->rx_w_round_robin_2);
		val64 = 0x0400010203040001ULL;
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0203040000000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		val64 = 0x8080404020201008ULL;
		writeq(val64, &bar0->rts_qos_steering);
		break;
	case 6:
		val64 = 0x0001020304050001ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		val64 = 0x0203040500010203ULL;
		writeq(val64, &bar0->rx_w_round_robin_1);
		val64 = 0x0405000102030405ULL;
		writeq(val64, &bar0->rx_w_round_robin_2);
		val64 = 0x0001020304050001ULL;
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0203040500000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		val64 = 0x8080404020100804ULL;
		writeq(val64, &bar0->rts_qos_steering);
		break;
	case 7:
		val64 = 0x0001020304050600ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		val64 = 0x0102030405060001ULL;
		writeq(val64, &bar0->rx_w_round_robin_1);
		val64 = 0x0203040506000102ULL;
		writeq(val64, &bar0->rx_w_round_robin_2);
		val64 = 0x0304050600010203ULL;
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0405060000000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		val64 = 0x8080402010080402ULL;
		writeq(val64, &bar0->rts_qos_steering);
		break;
	case 8:
		val64 = 0x0001020304050607ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		writeq(val64, &bar0->rx_w_round_robin_1);
		writeq(val64, &bar0->rx_w_round_robin_2);
		writeq(val64, &bar0->rx_w_round_robin_3);
		val64 = 0x0001020300000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		val64 = 0x8040201008040201ULL;
		writeq(val64, &bar0->rts_qos_steering);
		break;
	}

	/* UDP Fix */
	val64 = 0;
	for (i = 0; i < 8; i++)
		writeq(val64, &bar0->rts_frm_len_n[i]);

	/* Set the default rts frame length for the rings configured */
	val64 = MAC_RTS_FRM_LEN_SET(dev->mtu+22);
	for (i = 0 ; i < config->rx_ring_num ; i++)
		writeq(val64, &bar0->rts_frm_len_n[i]);

	/* Set the frame length for the configured rings
	 * desired by the user
	 */
	for (i = 0; i < config->rx_ring_num; i++) {
		/* If rts_frm_len[i] == 0 then it is assumed that user not
		 * specified frame length steering.
		 * If the user provides the frame length then program
		 * the rts_frm_len register for those values or else
		 * leave it as it is.
		 */
		if (rts_frm_len[i] != 0) {
			writeq(MAC_RTS_FRM_LEN_SET(rts_frm_len[i]),
			       &bar0->rts_frm_len_n[i]);
		}
	}

	/* Disable differentiated services steering logic */
	for (i = 0; i < 64; i++) {
		if (rts_ds_steer(nic, i, 0) == FAILURE) {
			DBG_PRINT(ERR_DBG,
				  "%s: rts_ds_steer failed on codepoint %d\n",
				  dev->name, i);
			return -ENODEV;
		}
	}

	/* Program statistics memory */
	writeq(mac_control->stats_mem_phy, &bar0->stat_addr);

	if (nic->device_type == XFRAME_II_DEVICE) {
		val64 = STAT_BC(0x320);
		writeq(val64, &bar0->stat_byte_cnt);
	}

	/*
	 * Initializing the sampling rate for the device to calculate the
	 * bandwidth utilization.
	 */
	val64 = MAC_TX_LINK_UTIL_VAL(tmac_util_period) |
		MAC_RX_LINK_UTIL_VAL(rmac_util_period);
	writeq(val64, &bar0->mac_link_util);

	/*
	 * Initializing the Transmit and Receive Traffic Interrupt
	 * Scheme.
	 */

	/* Initialize TTI */
	if (SUCCESS != init_tti(nic, nic->last_link_state))
		return -ENODEV;

	/* RTI Initialization */
	if (nic->device_type == XFRAME_II_DEVICE) {
		/*
		 * Programmed to generate Apprx 500 Intrs per
		 * second
		 */
		int count = (nic->config.bus_speed * 125)/4;
		val64 = RTI_DATA1_MEM_RX_TIMER_VAL(count);
	} else
		val64 = RTI_DATA1_MEM_RX_TIMER_VAL(0xFFF);
	val64 |= RTI_DATA1_MEM_RX_URNG_A(0xA) |
		RTI_DATA1_MEM_RX_URNG_B(0x10) |
		RTI_DATA1_MEM_RX_URNG_C(0x30) |
		RTI_DATA1_MEM_RX_TIMER_AC_EN;

	writeq(val64, &bar0->rti_data1_mem);

	val64 = RTI_DATA2_MEM_RX_UFC_A(0x1) |
		RTI_DATA2_MEM_RX_UFC_B(0x2) ;
	if (nic->config.intr_type == MSI_X)
		val64 |= (RTI_DATA2_MEM_RX_UFC_C(0x20) |
			  RTI_DATA2_MEM_RX_UFC_D(0x40));
	else
		val64 |= (RTI_DATA2_MEM_RX_UFC_C(0x40) |
			  RTI_DATA2_MEM_RX_UFC_D(0x80));
	writeq(val64, &bar0->rti_data2_mem);

	for (i = 0; i < config->rx_ring_num; i++) {
		val64 = RTI_CMD_MEM_WE |
			RTI_CMD_MEM_STROBE_NEW_CMD |
			RTI_CMD_MEM_OFFSET(i);
		writeq(val64, &bar0->rti_command_mem);

		/*
		 * Once the operation completes, the Strobe bit of the
		 * command register will be reset. We poll for this
		 * particular condition. We wait for a maximum of 500ms
		 * for the operation to complete, if it's not complete
		 * by then we return error.
		 */
		time = 0;
		while (true) {
			val64 = readq(&bar0->rti_command_mem);
			if (!(val64 & RTI_CMD_MEM_STROBE_NEW_CMD))
				break;

			if (time > 10) {
				DBG_PRINT(ERR_DBG, "%s: RTI init failed\n",
					  dev->name);
				return -ENODEV;
			}
			time++;
			msleep(50);
		}
	}

	/*
	 * Initializing proper values as Pause threshold into all
	 * the 8 Queues on Rx side.
	 */
	writeq(0xffbbffbbffbbffbbULL, &bar0->mc_pause_thresh_q0q3);
	writeq(0xffbbffbbffbbffbbULL, &bar0->mc_pause_thresh_q4q7);

	/* Disable RMAC PAD STRIPPING */
	add = &bar0->mac_cfg;
	val64 = readq(&bar0->mac_cfg);
	val64 &= ~(MAC_CFG_RMAC_STRIP_PAD);
	writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
	writel((u32) (val64), add);
	writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
	writel((u32) (val64 >> 32), (add + 4));
	val64 = readq(&bar0->mac_cfg);

	/* Enable FCS stripping by adapter */
	add = &bar0->mac_cfg;
	val64 = readq(&bar0->mac_cfg);
	val64 |= MAC_CFG_RMAC_STRIP_FCS;
	if (nic->device_type == XFRAME_II_DEVICE)
		writeq(val64, &bar0->mac_cfg);
	else {
		writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
		writel((u32) (val64), add);
		writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
		writel((u32) (val64 >> 32), (add + 4));
	}

	/*
	 * Set the time value to be inserted in the pause frame
	 * generated by xena.
	 */
	val64 = readq(&bar0->rmac_pause_cfg);
	val64 &= ~(RMAC_PAUSE_HG_PTIME(0xffff));
	val64 |= RMAC_PAUSE_HG_PTIME(nic->mac_control.rmac_pause_time);
	writeq(val64, &bar0->rmac_pause_cfg);

	/*
	 * Set the Threshold Limit for Generating the pause frame
	 * If the amount of data in any Queue exceeds ratio of
	 * (mac_control.mc_pause_threshold_q0q3 or q4q7)/256
	 * pause frame is generated
	 */
	val64 = 0;
	for (i = 0; i < 4; i++) {
		val64 |= (((u64)0xFF00 |
			   nic->mac_control.mc_pause_threshold_q0q3)
			  << (i * 2 * 8));
	}
	writeq(val64, &bar0->mc_pause_thresh_q0q3);

	val64 = 0;
	for (i = 0; i < 4; i++) {
		val64 |= (((u64)0xFF00 |
			   nic->mac_control.mc_pause_threshold_q4q7)
			  << (i * 2 * 8));
	}
	writeq(val64, &bar0->mc_pause_thresh_q4q7);

	/*
	 * TxDMA will stop Read request if the number of read split has
	 * exceeded the limit pointed by shared_splits
	 */
	val64 = readq(&bar0->pic_control);
	val64 |= PIC_CNTL_SHARED_SPLITS(shared_splits);
	writeq(val64, &bar0->pic_control);

	if (nic->config.bus_speed == 266) {
		writeq(TXREQTO_VAL(0x7f) | TXREQTO_EN, &bar0->txreqtimeout);
		writeq(0x0, &bar0->read_retry_delay);
		writeq(0x0, &bar0->write_retry_delay);
	}

	/*
	 * Programming the Herc to split every write transaction
	 * that does not start on an ADB to reduce disconnects.
	 */
	if (nic->device_type == XFRAME_II_DEVICE) {
		val64 = FAULT_BEHAVIOUR | EXT_REQ_EN |
			MISC_LINK_STABILITY_PRD(3);
		writeq(val64, &bar0->misc_control);
		val64 = readq(&bar0->pic_control2);
		val64 &= ~(s2BIT(13)|s2BIT(14)|s2BIT(15));
		writeq(val64, &bar0->pic_control2);
	}
	if (strstr(nic->product_name, "CX4")) {
		val64 = TMAC_AVG_IPG(0x17);
		writeq(val64, &bar0->tmac_avg_ipg);
	}

	return SUCCESS;
}
#define LINK_UP_DOWN_INTERRUPT		1
#define MAC_RMAC_ERR_TIMER		2

static int s2io_link_fault_indication(struct s2io_nic *nic)
{
	if (nic->device_type == XFRAME_II_DEVICE)
		return LINK_UP_DOWN_INTERRUPT;
	else
		return MAC_RMAC_ERR_TIMER;
}

/**
 *  do_s2io_write_bits -  update alarm bits in alarm register
 *  @value: alarm bits
 *  @flag: interrupt status
 *  @addr: address value
 *  Description: update alarm bits in alarm register
 *  Return Value:
 *  NONE.
 */
static void do_s2io_write_bits(u64 value, int flag, void __iomem *addr)
{
	u64 temp64;

	temp64 = readq(addr);

	if (flag == ENABLE_INTRS)
		temp64 &= ~((u64)value);
	else
		temp64 |= ((u64)value);
	writeq(temp64, addr);
}

static void en_dis_err_alarms(struct s2io_nic *nic, u16 mask, int flag)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 gen_int_mask = 0;
	u64 interruptible;

	writeq(DISABLE_ALL_INTRS, &bar0->general_int_mask);
	if (mask & TX_DMA_INTR) {
		gen_int_mask |= TXDMA_INT_M;

		do_s2io_write_bits(TXDMA_TDA_INT | TXDMA_PFC_INT |
				   TXDMA_PCC_INT | TXDMA_TTI_INT |
				   TXDMA_LSO_INT | TXDMA_TPA_INT |
				   TXDMA_SM_INT, flag, &bar0->txdma_int_mask);

		do_s2io_write_bits(PFC_ECC_DB_ERR | PFC_SM_ERR_ALARM |
				   PFC_MISC_0_ERR | PFC_MISC_1_ERR |
				   PFC_PCIX_ERR | PFC_ECC_SG_ERR, flag,
				   &bar0->pfc_err_mask);

		do_s2io_write_bits(TDA_Fn_ECC_DB_ERR | TDA_SM0_ERR_ALARM |
				   TDA_SM1_ERR_ALARM | TDA_Fn_ECC_SG_ERR |
				   TDA_PCIX_ERR, flag, &bar0->tda_err_mask);

		do_s2io_write_bits(PCC_FB_ECC_DB_ERR | PCC_TXB_ECC_DB_ERR |
				   PCC_SM_ERR_ALARM | PCC_WR_ERR_ALARM |
				   PCC_N_SERR | PCC_6_COF_OV_ERR |
				   PCC_7_COF_OV_ERR | PCC_6_LSO_OV_ERR |
				   PCC_7_LSO_OV_ERR | PCC_FB_ECC_SG_ERR |
				   PCC_TXB_ECC_SG_ERR,
				   flag, &bar0->pcc_err_mask);

		do_s2io_write_bits(TTI_SM_ERR_ALARM | TTI_ECC_SG_ERR |
				   TTI_ECC_DB_ERR, flag, &bar0->tti_err_mask);

		do_s2io_write_bits(LSO6_ABORT | LSO7_ABORT |
				   LSO6_SM_ERR_ALARM | LSO7_SM_ERR_ALARM |
				   LSO6_SEND_OFLOW | LSO7_SEND_OFLOW,
				   flag, &bar0->lso_err_mask);

		do_s2io_write_bits(TPA_SM_ERR_ALARM | TPA_TX_FRM_DROP,
				   flag, &bar0->tpa_err_mask);

		do_s2io_write_bits(SM_SM_ERR_ALARM, flag, &bar0->sm_err_mask);
	}

	if (mask & TX_MAC_INTR) {
		gen_int_mask |= TXMAC_INT_M;
		do_s2io_write_bits(MAC_INT_STATUS_TMAC_INT, flag,
				   &bar0->mac_int_mask);
		do_s2io_write_bits(TMAC_TX_BUF_OVRN | TMAC_TX_SM_ERR |
				   TMAC_ECC_SG_ERR | TMAC_ECC_DB_ERR |
				   TMAC_DESC_ECC_SG_ERR | TMAC_DESC_ECC_DB_ERR,
				   flag, &bar0->mac_tmac_err_mask);
	}

	if (mask & TX_XGXS_INTR) {
		gen_int_mask |= TXXGXS_INT_M;
		do_s2io_write_bits(XGXS_INT_STATUS_TXGXS, flag,
				   &bar0->xgxs_int_mask);
		do_s2io_write_bits(TXGXS_ESTORE_UFLOW | TXGXS_TX_SM_ERR |
				   TXGXS_ECC_SG_ERR | TXGXS_ECC_DB_ERR,
				   flag, &bar0->xgxs_txgxs_err_mask);
	}

	if (mask & RX_DMA_INTR) {
		gen_int_mask |= RXDMA_INT_M;
		do_s2io_write_bits(RXDMA_INT_RC_INT_M | RXDMA_INT_RPA_INT_M |
				   RXDMA_INT_RDA_INT_M | RXDMA_INT_RTI_INT_M,
				   flag, &bar0->rxdma_int_mask);
		do_s2io_write_bits(RC_PRCn_ECC_DB_ERR | RC_FTC_ECC_DB_ERR |
				   RC_PRCn_SM_ERR_ALARM | RC_FTC_SM_ERR_ALARM |
				   RC_PRCn_ECC_SG_ERR | RC_FTC_ECC_SG_ERR |
				   RC_RDA_FAIL_WR_Rn, flag, &bar0->rc_err_mask);
		do_s2io_write_bits(PRC_PCI_AB_RD_Rn | PRC_PCI_AB_WR_Rn |
				   PRC_PCI_AB_F_WR_Rn | PRC_PCI_DP_RD_Rn |
				   PRC_PCI_DP_WR_Rn | PRC_PCI_DP_F_WR_Rn, flag,
				   &bar0->prc_pcix_err_mask);
		do_s2io_write_bits(RPA_SM_ERR_ALARM | RPA_CREDIT_ERR |
				   RPA_ECC_SG_ERR | RPA_ECC_DB_ERR, flag,
				   &bar0->rpa_err_mask);
		do_s2io_write_bits(RDA_RXDn_ECC_DB_ERR | RDA_FRM_ECC_DB_N_AERR |
				   RDA_SM1_ERR_ALARM | RDA_SM0_ERR_ALARM |
				   RDA_RXD_ECC_DB_SERR | RDA_RXDn_ECC_SG_ERR |
				   RDA_FRM_ECC_SG_ERR |
				   RDA_MISC_ERR|RDA_PCIX_ERR,
				   flag, &bar0->rda_err_mask);
		do_s2io_write_bits(RTI_SM_ERR_ALARM |
				   RTI_ECC_SG_ERR | RTI_ECC_DB_ERR,
				   flag, &bar0->rti_err_mask);
	}

	if (mask & RX_MAC_INTR) {
		gen_int_mask |= RXMAC_INT_M;
		do_s2io_write_bits(MAC_INT_STATUS_RMAC_INT, flag,
				   &bar0->mac_int_mask);
		interruptible = (RMAC_RX_BUFF_OVRN | RMAC_RX_SM_ERR |
				 RMAC_UNUSED_INT | RMAC_SINGLE_ECC_ERR |
				 RMAC_DOUBLE_ECC_ERR);
		if (s2io_link_fault_indication(nic) == MAC_RMAC_ERR_TIMER)
			interruptible |= RMAC_LINK_STATE_CHANGE_INT;
		do_s2io_write_bits(interruptible,
				   flag, &bar0->mac_rmac_err_mask);
	}

	if (mask & RX_XGXS_INTR) {
		gen_int_mask |= RXXGXS_INT_M;
		do_s2io_write_bits(XGXS_INT_STATUS_RXGXS, flag,
				   &bar0->xgxs_int_mask);
		do_s2io_write_bits(RXGXS_ESTORE_OFLOW | RXGXS_RX_SM_ERR, flag,
				   &bar0->xgxs_rxgxs_err_mask);
	}

	if (mask & MC_INTR) {
		gen_int_mask |= MC_INT_M;
		do_s2io_write_bits(MC_INT_MASK_MC_INT,
				   flag, &bar0->mc_int_mask);
		do_s2io_write_bits(MC_ERR_REG_SM_ERR | MC_ERR_REG_ECC_ALL_SNG |
				   MC_ERR_REG_ECC_ALL_DBL | PLL_LOCK_N, flag,
				   &bar0->mc_err_mask);
	}
	nic->general_int_mask = gen_int_mask;

	/* Remove this line when alarm interrupts are enabled */
	nic->general_int_mask = 0;
}

/**
 *  en_dis_able_nic_intrs - Enable or Disable the interrupts
 *  @nic: device private variable,
 *  @mask: A mask indicating which Intr block must be modified and,
 *  @flag: A flag indicating whether to enable or disable the Intrs.
 *  Description: This function will either disable or enable the interrupts
 *  depending on the flag argument. The mask argument can be used to
 *  enable/disable any Intr block.
 *  Return Value: NONE.
 */

static void en_dis_able_nic_intrs(struct s2io_nic *nic, u16 mask, int flag)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 temp64 = 0, intr_mask = 0;

	intr_mask = nic->general_int_mask;

	/*  Top level interrupt classification */
	/*  PIC Interrupts */
	if (mask & TX_PIC_INTR) {
		/*  Enable PIC Intrs in the general intr mask register */
		intr_mask |= TXPIC_INT_M;
		if (flag == ENABLE_INTRS) {
			/*
			 * If Hercules adapter enable GPIO otherwise
			 * disable all PCIX, Flash, MDIO, IIC and GPIO
			 * interrupts for now.
			 * TODO
			 */
			if (s2io_link_fault_indication(nic) ==
			    LINK_UP_DOWN_INTERRUPT) {
				do_s2io_write_bits(PIC_INT_GPIO, flag,
						   &bar0->pic_int_mask);
				do_s2io_write_bits(GPIO_INT_MASK_LINK_UP, flag,
						   &bar0->gpio_int_mask);
			} else
				writeq(DISABLE_ALL_INTRS, &bar0->pic_int_mask);
		} else if (flag == DISABLE_INTRS) {
			/*
			 * Disable PIC Intrs in the general
			 * intr mask register
			 */
			writeq(DISABLE_ALL_INTRS, &bar0->pic_int_mask);
		}
	}

	/*  Tx traffic interrupts */
	if (mask & TX_TRAFFIC_INTR) {
		intr_mask |= TXTRAFFIC_INT_M;
		if (flag == ENABLE_INTRS) {
			/*
			 * Enable all the Tx side interrupts
			 * writing 0 Enables all 64 TX interrupt levels
			 */
			writeq(0x0, &bar0->tx_traffic_mask);
		} else if (flag == DISABLE_INTRS) {
			/*
			 * Disable Tx Traffic Intrs in the general intr mask
			 * register.
			 */
			writeq(DISABLE_ALL_INTRS, &bar0->tx_traffic_mask);
		}
	}

	/*  Rx traffic interrupts */
	if (mask & RX_TRAFFIC_INTR) {
		intr_mask |= RXTRAFFIC_INT_M;
		if (flag == ENABLE_INTRS) {
			/* writing 0 Enables all 8 RX interrupt levels */
			writeq(0x0, &bar0->rx_traffic_mask);
		} else if (flag == DISABLE_INTRS) {
			/*
			 * Disable Rx Traffic Intrs in the general intr mask
			 * register.
			 */
			writeq(DISABLE_ALL_INTRS, &bar0->rx_traffic_mask);
		}
	}

	temp64 = readq(&bar0->general_int_mask);
	if (flag == ENABLE_INTRS)
		temp64 &= ~((u64)intr_mask);
	else
		temp64 = DISABLE_ALL_INTRS;
	writeq(temp64, &bar0->general_int_mask);

	nic->general_int_mask = readq(&bar0->general_int_mask);
}

/**
 *  verify_pcc_quiescent- Checks for PCC quiescent state
 *  Return: 1 If PCC is quiescence
 *          0 If PCC is not quiescence
 */
static int verify_pcc_quiescent(struct s2io_nic *sp, int flag)
{
	int ret = 0, herc;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64 = readq(&bar0->adapter_status);

	herc = (sp->device_type == XFRAME_II_DEVICE);

	if (flag == false) {
		if ((!herc && (sp->pdev->revision >= 4)) || herc) {
			if (!(val64 & ADAPTER_STATUS_RMAC_PCC_IDLE))
				ret = 1;
		} else {
			if (!(val64 & ADAPTER_STATUS_RMAC_PCC_FOUR_IDLE))
				ret = 1;
		}
	} else {
		if ((!herc && (sp->pdev->revision >= 4)) || herc) {
			if (((val64 & ADAPTER_STATUS_RMAC_PCC_IDLE) ==
			     ADAPTER_STATUS_RMAC_PCC_IDLE))
				ret = 1;
		} else {
			if (((val64 & ADAPTER_STATUS_RMAC_PCC_FOUR_IDLE) ==
			     ADAPTER_STATUS_RMAC_PCC_FOUR_IDLE))
				ret = 1;
		}
	}

	return ret;
}
/**
 *  verify_xena_quiescence - Checks whether the H/W is ready
 *  Description: Returns whether the H/W is ready to go or not. Depending
 *  on whether adapter enable bit was written or not the comparison
 *  differs and the calling function passes the input argument flag to
 *  indicate this.
 *  Return: 1 If xena is quiescence
 *          0 If Xena is not quiescence
 */

static int verify_xena_quiescence(struct s2io_nic *sp)
{
	int  mode;
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64 = readq(&bar0->adapter_status);
	mode = s2io_verify_pci_mode(sp);

	if (!(val64 & ADAPTER_STATUS_TDMA_READY)) {
		DBG_PRINT(ERR_DBG, "TDMA is not ready!\n");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_RDMA_READY)) {
		DBG_PRINT(ERR_DBG, "RDMA is not ready!\n");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_PFC_READY)) {
		DBG_PRINT(ERR_DBG, "PFC is not ready!\n");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_TMAC_BUF_EMPTY)) {
		DBG_PRINT(ERR_DBG, "TMAC BUF is not empty!\n");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_PIC_QUIESCENT)) {
		DBG_PRINT(ERR_DBG, "PIC is not QUIESCENT!\n");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_MC_DRAM_READY)) {
		DBG_PRINT(ERR_DBG, "MC_DRAM is not ready!\n");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_MC_QUEUES_READY)) {
		DBG_PRINT(ERR_DBG, "MC_QUEUES is not ready!\n");
		return 0;
	}
	if (!(val64 & ADAPTER_STATUS_M_PLL_LOCK)) {
		DBG_PRINT(ERR_DBG, "M_PLL is not locked!\n");
		return 0;
	}

	/*
	 * In PCI 33 mode, the P_PLL is not used, and therefore,
	 * the the P_PLL_LOCK bit in the adapter_status register will
	 * not be asserted.
	 */
	if (!(val64 & ADAPTER_STATUS_P_PLL_LOCK) &&
	    sp->device_type == XFRAME_II_DEVICE &&
	    mode != PCI_MODE_PCI_33) {
		DBG_PRINT(ERR_DBG, "P_PLL is not locked!\n");
		return 0;
	}
	if (!((val64 & ADAPTER_STATUS_RC_PRC_QUIESCENT) ==
	      ADAPTER_STATUS_RC_PRC_QUIESCENT)) {
		DBG_PRINT(ERR_DBG, "RC_PRC is not QUIESCENT!\n");
		return 0;
	}
	return 1;
}

/**
 * fix_mac_address -  Fix for Mac addr problem on Alpha platforms
 * @sp: Pointer to device specifc structure
 * Description :
 * New procedure to clear mac address reading  problems on Alpha platforms
 *
 */

static void fix_mac_address(struct s2io_nic *sp)
{
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64;
	int i = 0;

	while (fix_mac[i] != END_SIGN) {
		writeq(fix_mac[i++], &bar0->gpio_control);
		udelay(10);
		val64 = readq(&bar0->gpio_control);
	}
}

/**
 *  start_nic - Turns the device on
 *  @nic : device private variable.
 *  Description:
 *  This function actually turns the device on. Before this  function is
 *  called,all Registers are configured from their reset states
 *  and shared memory is allocated but the NIC is still quiescent. On
 *  calling this function, the device interrupts are cleared and the NIC is
 *  literally switched on by writing into the adapter control register.
 *  Return Value:
 *  SUCCESS on success and -1 on failure.
 */

static int start_nic(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	struct net_device *dev = nic->dev;
	register u64 val64 = 0;
	u16 subid, i;
	struct config_param *config = &nic->config;
	struct mac_info *mac_control = &nic->mac_control;

	/*  PRC Initialization and configuration */
	for (i = 0; i < config->rx_ring_num; i++) {
		struct ring_info *ring = &mac_control->rings[i];

		writeq((u64)ring->rx_blocks[0].block_dma_addr,
		       &bar0->prc_rxd0_n[i]);

		val64 = readq(&bar0->prc_ctrl_n[i]);
		if (nic->rxd_mode == RXD_MODE_1)
			val64 |= PRC_CTRL_RC_ENABLED;
		else
			val64 |= PRC_CTRL_RC_ENABLED | PRC_CTRL_RING_MODE_3;
		if (nic->device_type == XFRAME_II_DEVICE)
			val64 |= PRC_CTRL_GROUP_READS;
		val64 &= ~PRC_CTRL_RXD_BACKOFF_INTERVAL(0xFFFFFF);
		val64 |= PRC_CTRL_RXD_BACKOFF_INTERVAL(0x1000);
		writeq(val64, &bar0->prc_ctrl_n[i]);
	}

	if (nic->rxd_mode == RXD_MODE_3B) {
		/* Enabling 2 buffer mode by writing into Rx_pa_cfg reg. */
		val64 = readq(&bar0->rx_pa_cfg);
		val64 |= RX_PA_CFG_IGNORE_L2_ERR;
		writeq(val64, &bar0->rx_pa_cfg);
	}

	if (vlan_tag_strip == 0) {
		val64 = readq(&bar0->rx_pa_cfg);
		val64 &= ~RX_PA_CFG_STRIP_VLAN_TAG;
		writeq(val64, &bar0->rx_pa_cfg);
		nic->vlan_strip_flag = 0;
	}

	/*
	 * Enabling MC-RLDRAM. After enabling the device, we timeout
	 * for around 100ms, which is approximately the time required
	 * for the device to be ready for operation.
	 */
	val64 = readq(&bar0->mc_rldram_mrs);
	val64 |= MC_RLDRAM_QUEUE_SIZE_ENABLE | MC_RLDRAM_MRS_ENABLE;
	SPECIAL_REG_WRITE(val64, &bar0->mc_rldram_mrs, UF);
	val64 = readq(&bar0->mc_rldram_mrs);

	msleep(100);	/* Delay by around 100 ms. */

	/* Enabling ECC Protection. */
	val64 = readq(&bar0->adapter_control);
	val64 &= ~ADAPTER_ECC_EN;
	writeq(val64, &bar0->adapter_control);

	/*
	 * Verify if the device is ready to be enabled, if so enable
	 * it.
	 */
	val64 = readq(&bar0->adapter_status);
	if (!verify_xena_quiescence(nic)) {
		DBG_PRINT(ERR_DBG, "%s: device is not ready, "
			  "Adapter status reads: 0x%llx\n",
			  dev->name, (unsigned long long)val64);
		return FAILURE;
	}

	/*
	 * With some switches, link might be already up at this point.
	 * Because of this weird behavior, when we enable laser,
	 * we may not get link. We need to handle this. We cannot
	 * figure out which switch is misbehaving. So we are forced to
	 * make a global change.
	 */

	/* Enabling Laser. */
	val64 = readq(&bar0->adapter_control);
	val64 |= ADAPTER_EOI_TX_ON;
	writeq(val64, &bar0->adapter_control);

	if (s2io_link_fault_indication(nic) == MAC_RMAC_ERR_TIMER) {
		/*
		 * Dont see link state interrupts initally on some switches,
		 * so directly scheduling the link state task here.
		 */
		schedule_work(&nic->set_link_task);
	}
	/* SXE-002: Initialize link and activity LED */
	subid = nic->pdev->subsystem_device;
	if (((subid & 0xFF) >= 0x07) &&
	    (nic->device_type == XFRAME_I_DEVICE)) {
		val64 = readq(&bar0->gpio_control);
		val64 |= 0x0000800000000000ULL;
		writeq(val64, &bar0->gpio_control);
		val64 = 0x0411040400000000ULL;
		writeq(val64, (void __iomem *)bar0 + 0x2700);
	}

	return SUCCESS;
}
/**
 * s2io_txdl_getskb - Get the skb from txdl, unmap and return skb
 */
static struct sk_buff *s2io_txdl_getskb(struct fifo_info *fifo_data,
					struct TxD *txdlp, int get_off)
{
	struct s2io_nic *nic = fifo_data->nic;
	struct sk_buff *skb;
	struct TxD *txds;
	u16 j, frg_cnt;

	txds = txdlp;
	if (txds->Host_Control == (u64)(long)fifo_data->ufo_in_band_v) {
		pci_unmap_single(nic->pdev, (dma_addr_t)txds->Buffer_Pointer,
				 sizeof(u64), PCI_DMA_TODEVICE);
		txds++;
	}

	skb = (struct sk_buff *)((unsigned long)txds->Host_Control);
	if (!skb) {
		memset(txdlp, 0, (sizeof(struct TxD) * fifo_data->max_txds));
		return NULL;
	}
	pci_unmap_single(nic->pdev, (dma_addr_t)txds->Buffer_Pointer,
			 skb->len - skb->data_len, PCI_DMA_TODEVICE);
	frg_cnt = skb_shinfo(skb)->nr_frags;
	if (frg_cnt) {
		txds++;
		for (j = 0; j < frg_cnt; j++, txds++) {
			skb_frag_t *frag = &skb_shinfo(skb)->frags[j];
			if (!txds->Buffer_Pointer)
				break;
			pci_unmap_page(nic->pdev,
				       (dma_addr_t)txds->Buffer_Pointer,
				       frag->size, PCI_DMA_TODEVICE);
		}
	}
	memset(txdlp, 0, (sizeof(struct TxD) * fifo_data->max_txds));
	return skb;
}

/**
 *  free_tx_buffers - Free all queued Tx buffers
 *  @nic : device private variable.
 *  Description:
 *  Free all queued Tx buffers.
 *  Return Value: void
 */

static void free_tx_buffers(struct s2io_nic *nic)
{
	struct net_device *dev = nic->dev;
	struct sk_buff *skb;
	struct TxD *txdp;
	int i, j;
	int cnt = 0;
	struct config_param *config = &nic->config;
	struct mac_info *mac_control = &nic->mac_control;
	struct stat_block *stats = mac_control->stats_info;
	struct swStat *swstats = &stats->sw_stat;

	for (i = 0; i < config->tx_fifo_num; i++) {
		struct tx_fifo_config *tx_cfg = &config->tx_cfg[i];
		struct fifo_info *fifo = &mac_control->fifos[i];
		unsigned long flags;

		spin_lock_irqsave(&fifo->tx_lock, flags);
		for (j = 0; j < tx_cfg->fifo_len; j++) {
			txdp = (struct TxD *)fifo->list_info[j].list_virt_addr;
			skb = s2io_txdl_getskb(&mac_control->fifos[i], txdp, j);
			if (skb) {
				swstats->mem_freed += skb->truesize;
				dev_kfree_skb(skb);
				cnt++;
			}
		}
		DBG_PRINT(INTR_DBG,
			  "%s: forcibly freeing %d skbs on FIFO%d\n",
			  dev->name, cnt, i);
		fifo->tx_curr_get_info.offset = 0;
		fifo->tx_curr_put_info.offset = 0;
		spin_unlock_irqrestore(&fifo->tx_lock, flags);
	}
}

/**
 *   stop_nic -  To stop the nic
 *   @nic ; device private variable.
 *   Description:
 *   This function does exactly the opposite of what the start_nic()
 *   function does. This function is called to stop the device.
 *   Return Value:
 *   void.
 */

static void stop_nic(struct s2io_nic *nic)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 val64 = 0;
	u16 interruptible;

	/*  Disable all interrupts */
	en_dis_err_alarms(nic, ENA_ALL_INTRS, DISABLE_INTRS);
	interruptible = TX_TRAFFIC_INTR | RX_TRAFFIC_INTR;
	interruptible |= TX_PIC_INTR;
	en_dis_able_nic_intrs(nic, interruptible, DISABLE_INTRS);

	/* Clearing Adapter_En bit of ADAPTER_CONTROL Register */
	val64 = readq(&bar0->adapter_control);
	val64 &= ~(ADAPTER_CNTL_EN);
	writeq(val64, &bar0->adapter_control);
}

/**
 *  fill_rx_buffers - Allocates the Rx side skbs
 *  @ring_info: per ring structure
 *  @from_card_up: If this is true, we will map the buffer to get
 *     the dma address for buf0 and buf1 to give it to the card.
 *     Else we will sync the already mapped buffer to give it to the card.
 *  Description:
 *  The function allocates Rx side skbs and puts the physical
 *  address of these buffers into the RxD buffer pointers, so that the NIC
 *  can DMA the received frame into these locations.
 *  The NIC supports 3 receive modes, viz
 *  1. single buffer,
 *  2. three buffer and
 *  3. Five buffer modes.
 *  Each mode defines how many fragments the received frame will be split
 *  up into by the NIC. The frame is split into L3 header, L4 Header,
 *  L4 payload in three buffer mode and in 5 buffer mode, L4 payload itself
 *  is split into 3 fragments. As of now only single buffer mode is
 *  supported.
 *   Return Value:
 *  SUCCESS on success or an appropriate -ve value on failure.
 */
static int fill_rx_buffers(struct s2io_nic *nic, struct ring_info *ring,
			   int from_card_up)
{
	struct sk_buff *skb;
	struct RxD_t *rxdp;
	int off, size, block_no, block_no1;
	u32 alloc_tab = 0;
	u32 alloc_cnt;
	u64 tmp;
	struct buffAdd *ba;
	struct RxD_t *first_rxdp = NULL;
	u64 Buffer0_ptr = 0, Buffer1_ptr = 0;
	int rxd_index = 0;
	struct RxD1 *rxdp1;
	struct RxD3 *rxdp3;
	struct swStat *swstats = &ring->nic->mac_control.stats_info->sw_stat;

	alloc_cnt = ring->pkt_cnt - ring->rx_bufs_left;

	block_no1 = ring->rx_curr_get_info.block_index;
	while (alloc_tab < alloc_cnt) {
		block_no = ring->rx_curr_put_info.block_index;

		off = ring->rx_curr_put_info.offset;

		rxdp = ring->rx_blocks[block_no].rxds[off].virt_addr;

		rxd_index = off + 1;
		if (block_no)
			rxd_index += (block_no * ring->rxd_count);

		if ((block_no == block_no1) &&
		    (off == ring->rx_curr_get_info.offset) &&
		    (rxdp->Host_Control)) {
			DBG_PRINT(INTR_DBG, "%s: Get and Put info equated\n",
				  ring->dev->name);
			goto end;
		}
		if (off && (off == ring->rxd_count)) {
			ring->rx_curr_put_info.block_index++;
			if (ring->rx_curr_put_info.block_index ==
			    ring->block_count)
				ring->rx_curr_put_info.block_index = 0;
			block_no = ring->rx_curr_put_info.block_index;
			off = 0;
			ring->rx_curr_put_info.offset = off;
			rxdp = ring->rx_blocks[block_no].block_virt_addr;
			DBG_PRINT(INTR_DBG, "%s: Next block at: %p\n",
				  ring->dev->name, rxdp);

		}

		if ((rxdp->Control_1 & RXD_OWN_XENA) &&
		    ((ring->rxd_mode == RXD_MODE_3B) &&
		     (rxdp->Control_2 & s2BIT(0)))) {
			ring->rx_curr_put_info.offset = off;
			goto end;
		}
		/* calculate size of skb based on ring mode */
		size = ring->mtu +
			HEADER_ETHERNET_II_802_3_SIZE +
			HEADER_802_2_SIZE + HEADER_SNAP_SIZE;
		if (ring->rxd_mode == RXD_MODE_1)
			size += NET_IP_ALIGN;
		else
			size = ring->mtu + ALIGN_SIZE + BUF0_LEN + 4;

		/* allocate skb */
		skb = dev_alloc_skb(size);
		if (!skb) {
			DBG_PRINT(INFO_DBG, "%s: Could not allocate skb\n",
				  ring->dev->name);
			if (first_rxdp) {
				wmb();
				first_rxdp->Control_1 |= RXD_OWN_XENA;
			}
			swstats->mem_alloc_fail_cnt++;

			return -ENOMEM ;
		}
		swstats->mem_allocated += skb->truesize;

		if (ring->rxd_mode == RXD_MODE_1) {
			/* 1 buffer mode - normal operation mode */
			rxdp1 = (struct RxD1 *)rxdp;
			memset(rxdp, 0, sizeof(struct RxD1));
			skb_reserve(skb, NET_IP_ALIGN);
			rxdp1->Buffer0_ptr =
				pci_map_single(ring->pdev, skb->data,
					       size - NET_IP_ALIGN,
					       PCI_DMA_FROMDEVICE);
			if (pci_dma_mapping_error(nic->pdev,
						  rxdp1->Buffer0_ptr))
				goto pci_map_failed;

			rxdp->Control_2 =
				SET_BUFFER0_SIZE_1(size - NET_IP_ALIGN);
			rxdp->Host_Control = (unsigned long)skb;
		} else if (ring->rxd_mode == RXD_MODE_3B) {
			/*
			 * 2 buffer mode -
			 * 2 buffer mode provides 128
			 * byte aligned receive buffers.
			 */

			rxdp3 = (struct RxD3 *)rxdp;
			/* save buffer pointers to avoid frequent dma mapping */
			Buffer0_ptr = rxdp3->Buffer0_ptr;
			Buffer1_ptr = rxdp3->Buffer1_ptr;
			memset(rxdp, 0, sizeof(struct RxD3));
			/* restore the buffer pointers for dma sync*/
			rxdp3->Buffer0_ptr = Buffer0_ptr;
			rxdp3->Buffer1_ptr = Buffer1_ptr;

			ba = &ring->ba[block_no][off];
			skb_reserve(skb, BUF0_LEN);
			tmp = (u64)(unsigned long)skb->data;
			tmp += ALIGN_SIZE;
			tmp &= ~ALIGN_SIZE;
			skb->data = (void *) (unsigned long)tmp;
			skb_reset_tail_pointer(skb);

			if (from_card_up) {
				rxdp3->Buffer0_ptr =
					pci_map_single(ring->pdev, ba->ba_0,
						       BUF0_LEN,
						       PCI_DMA_FROMDEVICE);
				if (pci_dma_mapping_error(nic->pdev,
							  rxdp3->Buffer0_ptr))
					goto pci_map_failed;
			} else
				pci_dma_sync_single_for_device(ring->pdev,
							       (dma_addr_t)rxdp3->Buffer0_ptr,
							       BUF0_LEN,
							       PCI_DMA_FROMDEVICE);

			rxdp->Control_2 = SET_BUFFER0_SIZE_3(BUF0_LEN);
			if (ring->rxd_mode == RXD_MODE_3B) {
				/* Two buffer mode */

				/*
				 * Buffer2 will have L3/L4 header plus
				 * L4 payload
				 */
				rxdp3->Buffer2_ptr = pci_map_single(ring->pdev,
								    skb->data,
								    ring->mtu + 4,
								    PCI_DMA_FROMDEVICE);

				if (pci_dma_mapping_error(nic->pdev,
							  rxdp3->Buffer2_ptr))
					goto pci_map_failed;

				if (from_card_up) {
					rxdp3->Buffer1_ptr =
						pci_map_single(ring->pdev,
							       ba->ba_1,
							       BUF1_LEN,
							       PCI_DMA_FROMDEVICE);

					if (pci_dma_mapping_error(nic->pdev,
								  rxdp3->Buffer1_ptr)) {
						pci_unmap_single(ring->pdev,
								 (dma_addr_t)(unsigned long)
								 skb->data,
								 ring->mtu + 4,
								 PCI_DMA_FROMDEVICE);
						goto pci_map_failed;
					}
				}
				rxdp->Control_2 |= SET_BUFFER1_SIZE_3(1);
				rxdp->Control_2 |= SET_BUFFER2_SIZE_3
					(ring->mtu + 4);
			}
			rxdp->Control_2 |= s2BIT(0);
			rxdp->Host_Control = (unsigned long) (skb);
		}
		if (alloc_tab & ((1 << rxsync_frequency) - 1))
			rxdp->Control_1 |= RXD_OWN_XENA;
		off++;
		if (off == (ring->rxd_count + 1))
			off = 0;
		ring->rx_curr_put_info.offset = off;

		rxdp->Control_2 |= SET_RXD_MARKER;
		if (!(alloc_tab & ((1 << rxsync_frequency) - 1))) {
			if (first_rxdp) {
				wmb();
				first_rxdp->Control_1 |= RXD_OWN_XENA;
			}
			first_rxdp = rxdp;
		}
		ring->rx_bufs_left += 1;
		alloc_tab++;
	}

end:
	/* Transfer ownership of first descriptor to adapter just before
	 * exiting. Before that, use memory barrier so that ownership
	 * and other fields are seen by adapter correctly.
	 */
	if (first_rxdp) {
		wmb();
		first_rxdp->Control_1 |= RXD_OWN_XENA;
	}

	return SUCCESS;

pci_map_failed:
	swstats->pci_map_fail_cnt++;
	swstats->mem_freed += skb->truesize;
	dev_kfree_skb_irq(skb);
	return -ENOMEM;
}

static void free_rxd_blk(struct s2io_nic *sp, int ring_no, int blk)
{
	struct net_device *dev = sp->dev;
	int j;
	struct sk_buff *skb;
	struct RxD_t *rxdp;
	struct buffAdd *ba;
	struct RxD1 *rxdp1;
	struct RxD3 *rxdp3;
	struct mac_info *mac_control = &sp->mac_control;
	struct stat_block *stats = mac_control->stats_info;
	struct swStat *swstats = &stats->sw_stat;

	for (j = 0 ; j < rxd_count[sp->rxd_mode]; j++) {
		rxdp = mac_control->rings[ring_no].
			rx_blocks[blk].rxds[j].virt_addr;
		skb = (struct sk_buff *)((unsigned long)rxdp->Host_Control);
		if (!skb)
			continue;
		if (sp->rxd_mode == RXD_MODE_1) {
			rxdp1 = (struct RxD1 *)rxdp;
			pci_unmap_single(sp->pdev,
					 (dma_addr_t)rxdp1->Buffer0_ptr,
					 dev->mtu +
					 HEADER_ETHERNET_II_802_3_SIZE +
					 HEADER_802_2_SIZE + HEADER_SNAP_SIZE,
					 PCI_DMA_FROMDEVICE);
			memset(rxdp, 0, sizeof(struct RxD1));
		} else if (sp->rxd_mode == RXD_MODE_3B) {
			rxdp3 = (struct RxD3 *)rxdp;
			ba = &mac_control->rings[ring_no].ba[blk][j];
			pci_unmap_single(sp->pdev,
					 (dma_addr_t)rxdp3->Buffer0_ptr,
					 BUF0_LEN,
					 PCI_DMA_FROMDEVICE);
			pci_unmap_single(sp->pdev,
					 (dma_addr_t)rxdp3->Buffer1_ptr,
					 BUF1_LEN,
					 PCI_DMA_FROMDEVICE);
			pci_unmap_single(sp->pdev,
					 (dma_addr_t)rxdp3->Buffer2_ptr,
					 dev->mtu + 4,
					 PCI_DMA_FROMDEVICE);
			memset(rxdp, 0, sizeof(struct RxD3));
		}
		swstats->mem_freed += skb->truesize;
		dev_kfree_skb(skb);
		mac_control->rings[ring_no].rx_bufs_left -= 1;
	}
}

/**
 *  free_rx_buffers - Frees all Rx buffers
 *  @sp: device private variable.
 *  Description:
 *  This function will free all Rx buffers allocated by host.
 *  Return Value:
 *  NONE.
 */

static void free_rx_buffers(struct s2io_nic *sp)
{
	struct net_device *dev = sp->dev;
	int i, blk = 0, buf_cnt = 0;
	struct config_param *config = &sp->config;
	struct mac_info *mac_control = &sp->mac_control;

	for (i = 0; i < config->rx_ring_num; i++) {
		struct ring_info *ring = &mac_control->rings[i];

		for (blk = 0; blk < rx_ring_sz[i]; blk++)
			free_rxd_blk(sp, i, blk);

		ring->rx_curr_put_info.block_index = 0;
		ring->rx_curr_get_info.block_index = 0;
		ring->rx_curr_put_info.offset = 0;
		ring->rx_curr_get_info.offset = 0;
		ring->rx_bufs_left = 0;
		DBG_PRINT(INIT_DBG, "%s: Freed 0x%x Rx Buffers on ring%d\n",
			  dev->name, buf_cnt, i);
	}
}

static int s2io_chk_rx_buffers(struct s2io_nic *nic, struct ring_info *ring)
{
	if (fill_rx_buffers(nic, ring, 0) == -ENOMEM) {
		DBG_PRINT(INFO_DBG, "%s: Out of memory in Rx Intr!!\n",
			  ring->dev->name);
	}
	return 0;
}

/**
 * s2io_poll - Rx interrupt handler for NAPI support
 * @napi : pointer to the napi structure.
 * @budget : The number of packets that were budgeted to be processed
 * during  one pass through the 'Poll" function.
 * Description:
 * Comes into picture only if NAPI support has been incorporated. It does
 * the same thing that rx_intr_handler does, but not in a interrupt context
 * also It will process only a given number of packets.
 * Return value:
 * 0 on success and 1 if there are No Rx packets to be processed.
 */

static int s2io_poll_msix(struct napi_struct *napi, int budget)
{
	struct ring_info *ring = container_of(napi, struct ring_info, napi);
	struct net_device *dev = ring->dev;
	int pkts_processed = 0;
	u8 __iomem *addr = NULL;
	u8 val8 = 0;
	struct s2io_nic *nic = netdev_priv(dev);
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	int budget_org = budget;

	if (unlikely(!is_s2io_card_up(nic)))
		return 0;

	pkts_processed = rx_intr_handler(ring, budget);
	s2io_chk_rx_buffers(nic, ring);

	if (pkts_processed < budget_org) {
		napi_complete(napi);
		/*Re Enable MSI-Rx Vector*/
		addr = (u8 __iomem *)&bar0->xmsi_mask_reg;
		addr += 7 - ring->ring_no;
		val8 = (ring->ring_no == 0) ? 0x3f : 0xbf;
		writeb(val8, addr);
		val8 = readb(addr);
	}
	return pkts_processed;
}

static int s2io_poll_inta(struct napi_struct *napi, int budget)
{
	struct s2io_nic *nic = container_of(napi, struct s2io_nic, napi);
	int pkts_processed = 0;
	int ring_pkts_processed, i;
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	int budget_org = budget;
	struct config_param *config = &nic->config;
	struct mac_info *mac_control = &nic->mac_control;

	if (unlikely(!is_s2io_card_up(nic)))
		return 0;

	for (i = 0; i < config->rx_ring_num; i++) {
		struct ring_info *ring = &mac_control->rings[i];
		ring_pkts_processed = rx_intr_handler(ring, budget);
		s2io_chk_rx_buffers(nic, ring);
		pkts_processed += ring_pkts_processed;
		budget -= ring_pkts_processed;
		if (budget <= 0)
			break;
	}
	if (pkts_processed < budget_org) {
		napi_complete(napi);
		/* Re enable the Rx interrupts for the ring */
		writeq(0, &bar0->rx_traffic_mask);
		readl(&bar0->rx_traffic_mask);
	}
	return pkts_processed;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/**
 * s2io_netpoll - netpoll event handler entry point
 * @dev : pointer to the device structure.
 * Description:
 * 	This function will be called by upper layer to check for events on the
 * interface in situations where interrupts are disabled. It is used for
 * specific in-kernel networking tasks, such as remote consoles and kernel
 * debugging over the network (example netdump in RedHat).
 */
static void s2io_netpoll(struct net_device *dev)
{
	struct s2io_nic *nic = netdev_priv(dev);
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	u64 val64 = 0xFFFFFFFFFFFFFFFFULL;
	int i;
	struct config_param *config = &nic->config;
	struct mac_info *mac_control = &nic->mac_control;

	if (pci_channel_offline(nic->pdev))
		return;

	disable_irq(dev->irq);

	writeq(val64, &bar0->rx_traffic_int);
	writeq(val64, &bar0->tx_traffic_int);

	/* we need to free up the transmitted skbufs or else netpoll will
	 * run out of skbs and will fail and eventually netpoll application such
	 * as netdump will fail.
	 */
	for (i = 0; i < config->tx_fifo_num; i++)
		tx_intr_handler(&mac_control->fifos[i]);

	/* check for received packet and indicate up to network */
	for (i = 0; i < config->rx_ring_num; i++) {
		struct ring_info *ring = &mac_control->rings[i];

		rx_intr_handler(ring, 0);
	}

	for (i = 0; i < config->rx_ring_num; i++) {
		struct ring_info *ring = &mac_control->rings[i];

		if (fill_rx_buffers(nic, ring, 0) == -ENOMEM) {
			DBG_PRINT(INFO_DBG,
				  "%s: Out of memory in Rx Netpoll!!\n",
				  dev->name);
			break;
		}
	}
	enable_irq(dev->irq);
	return;
}
#endif

/**
 *  rx_intr_handler - Rx interrupt handler
 *  @ring_info: per ring structure.
 *  @budget: budget for napi processing.
 *  Description:
 *  If the interrupt is because of a received frame or if the
 *  receive ring contains fresh as yet un-processed frames,this function is
 *  called. It picks out the RxD at which place the last Rx processing had
 *  stopped and sends the skb to the OSM's Rx handler and then increments
 *  the offset.
 *  Return Value:
 *  No. of napi packets processed.
 */
static int rx_intr_handler(struct ring_info *ring_data, int budget)
{
	int get_block, put_block;
	struct rx_curr_get_info get_info, put_info;
	struct RxD_t *rxdp;
	struct sk_buff *skb;
	int pkt_cnt = 0, napi_pkts = 0;
	int i;
	struct RxD1 *rxdp1;
	struct RxD3 *rxdp3;

	get_info = ring_data->rx_curr_get_info;
	get_block = get_info.block_index;
	memcpy(&put_info, &ring_data->rx_curr_put_info, sizeof(put_info));
	put_block = put_info.block_index;
	rxdp = ring_data->rx_blocks[get_block].rxds[get_info.offset].virt_addr;

	while (RXD_IS_UP2DT(rxdp)) {
		/*
		 * If your are next to put index then it's
		 * FIFO full condition
		 */
		if ((get_block == put_block) &&
		    (get_info.offset + 1) == put_info.offset) {
			DBG_PRINT(INTR_DBG, "%s: Ring Full\n",
				  ring_data->dev->name);
			break;
		}
		skb = (struct sk_buff *)((unsigned long)rxdp->Host_Control);
		if (skb == NULL) {
			DBG_PRINT(ERR_DBG, "%s: NULL skb in Rx Intr\n",
				  ring_data->dev->name);
			return 0;
		}
		if (ring_data->rxd_mode == RXD_MODE_1) {
			rxdp1 = (struct RxD1 *)rxdp;
			pci_unmap_single(ring_data->pdev, (dma_addr_t)
					 rxdp1->Buffer0_ptr,
					 ring_data->mtu +
					 HEADER_ETHERNET_II_802_3_SIZE +
					 HEADER_802_2_SIZE +
					 HEADER_SNAP_SIZE,
					 PCI_DMA_FROMDEVICE);
		} else if (ring_data->rxd_mode == RXD_MODE_3B) {
			rxdp3 = (struct RxD3 *)rxdp;
			pci_dma_sync_single_for_cpu(ring_data->pdev,
						    (dma_addr_t)rxdp3->Buffer0_ptr,
						    BUF0_LEN,
						    PCI_DMA_FROMDEVICE);
			pci_unmap_single(ring_data->pdev,
					 (dma_addr_t)rxdp3->Buffer2_ptr,
					 ring_data->mtu + 4,
					 PCI_DMA_FROMDEVICE);
		}
		prefetch(skb->data);
		rx_osm_handler(ring_data, rxdp);
		get_info.offset++;
		ring_data->rx_curr_get_info.offset = get_info.offset;
		rxdp = ring_data->rx_blocks[get_block].
			rxds[get_info.offset].virt_addr;
		if (get_info.offset == rxd_count[ring_data->rxd_mode]) {
			get_info.offset = 0;
			ring_data->rx_curr_get_info.offset = get_info.offset;
			get_block++;
			if (get_block == ring_data->block_count)
				get_block = 0;
			ring_data->rx_curr_get_info.block_index = get_block;
			rxdp = ring_data->rx_blocks[get_block].block_virt_addr;
		}

		if (ring_data->nic->config.napi) {
			budget--;
			napi_pkts++;
			if (!budget)
				break;
		}
		pkt_cnt++;
		if ((indicate_max_pkts) && (pkt_cnt > indicate_max_pkts))
			break;
	}
	if (ring_data->lro) {
		/* Clear all LRO sessions before exiting */
		for (i = 0; i < MAX_LRO_SESSIONS; i++) {
			struct lro *lro = &ring_data->lro0_n[i];
			if (lro->in_use) {
				update_L3L4_header(ring_data->nic, lro);
				queue_rx_frame(lro->parent, lro->vlan_tag);
				clear_lro_session(lro);
			}
		}
	}
	return napi_pkts;
}

/**
 *  tx_intr_handler - Transmit interrupt handler
 *  @nic : device private variable
 *  Description:
 *  If an interrupt was raised to indicate DMA complete of the
 *  Tx packet, this function is called. It identifies the last TxD
 *  whose buffer was freed and frees all skbs whose data have already
 *  DMA'ed into the NICs internal memory.
 *  Return Value:
 *  NONE
 */

static void tx_intr_handler(struct fifo_info *fifo_data)
{
	struct s2io_nic *nic = fifo_data->nic;
	struct tx_curr_get_info get_info, put_info;
	struct sk_buff *skb = NULL;
	struct TxD *txdlp;
	int pkt_cnt = 0;
	unsigned long flags = 0;
	u8 err_mask;
	struct stat_block *stats = nic->mac_control.stats_info;
	struct swStat *swstats = &stats->sw_stat;

	if (!spin_trylock_irqsave(&fifo_data->tx_lock, flags))
		return;

	get_info = fifo_data->tx_curr_get_info;
	memcpy(&put_info, &fifo_data->tx_curr_put_info, sizeof(put_info));
	txdlp = (struct TxD *)
		fifo_data->list_info[get_info.offset].list_virt_addr;
	while ((!(txdlp->Control_1 & TXD_LIST_OWN_XENA)) &&
	       (get_info.offset != put_info.offset) &&
	       (txdlp->Host_Control)) {
		/* Check for TxD errors */
		if (txdlp->Control_1 & TXD_T_CODE) {
			unsigned long long err;
			err = txdlp->Control_1 & TXD_T_CODE;
			if (err & 0x1) {
				swstats->parity_err_cnt++;
			}

			/* update t_code statistics */
			err_mask = err >> 48;
			switch (err_mask) {
			case 2:
				swstats->tx_buf_abort_cnt++;
				break;

			case 3:
				swstats->tx_desc_abort_cnt++;
				break;

			case 7:
				swstats->tx_parity_err_cnt++;
				break;

			case 10:
				swstats->tx_link_loss_cnt++;
				break;

			case 15:
				swstats->tx_list_proc_err_cnt++;
				break;
			}
		}

		skb = s2io_txdl_getskb(fifo_data, txdlp, get_info.offset);
		if (skb == NULL) {
			spin_unlock_irqrestore(&fifo_data->tx_lock, flags);
			DBG_PRINT(ERR_DBG, "%s: NULL skb in Tx Free Intr\n",
				  __func__);
			return;
		}
		pkt_cnt++;

		/* Updating the statistics block */
		nic->dev->stats.tx_bytes += skb->len;
		swstats->mem_freed += skb->truesize;
		dev_kfree_skb_irq(skb);

		get_info.offset++;
		if (get_info.offset == get_info.fifo_len + 1)
			get_info.offset = 0;
		txdlp = (struct TxD *)
			fifo_data->list_info[get_info.offset].list_virt_addr;
		fifo_data->tx_curr_get_info.offset = get_info.offset;
	}

	s2io_wake_tx_queue(fifo_data, pkt_cnt, nic->config.multiq);

	spin_unlock_irqrestore(&fifo_data->tx_lock, flags);
}

/**
 *  s2io_mdio_write - Function to write in to MDIO registers
 *  @mmd_type : MMD type value (PMA/PMD/WIS/PCS/PHYXS)
 *  @addr     : address value
 *  @value    : data value
 *  @dev      : pointer to net_device structure
 *  Description:
 *  This function is used to write values to the MDIO registers
 *  NONE
 */
static void s2io_mdio_write(u32 mmd_type, u64 addr, u16 value,
			    struct net_device *dev)
{
	u64 val64;
	struct s2io_nic *sp = netdev_priv(dev);
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	/* address transaction */
	val64 = MDIO_MMD_INDX_ADDR(addr) |
		MDIO_MMD_DEV_ADDR(mmd_type) |
		MDIO_MMS_PRT_ADDR(0x0);
	writeq(val64, &bar0->mdio_control);
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	writeq(val64, &bar0->mdio_control);
	udelay(100);

	/* Data transaction */
	val64 = MDIO_MMD_INDX_ADDR(addr) |
		MDIO_MMD_DEV_ADDR(mmd_type) |
		MDIO_MMS_PRT_ADDR(0x0) |
		MDIO_MDIO_DATA(value) |
		MDIO_OP(MDIO_OP_WRITE_TRANS);
	writeq(val64, &bar0->mdio_control);
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	writeq(val64, &bar0->mdio_control);
	udelay(100);

	val64 = MDIO_MMD_INDX_ADDR(addr) |
		MDIO_MMD_DEV_ADDR(mmd_type) |
		MDIO_MMS_PRT_ADDR(0x0) |
		MDIO_OP(MDIO_OP_READ_TRANS);
	writeq(val64, &bar0->mdio_control);
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	writeq(val64, &bar0->mdio_control);
	udelay(100);
}

/**
 *  s2io_mdio_read - Function to write in to MDIO registers
 *  @mmd_type : MMD type value (PMA/PMD/WIS/PCS/PHYXS)
 *  @addr     : address value
 *  @dev      : pointer to net_device structure
 *  Description:
 *  This function is used to read values to the MDIO registers
 *  NONE
 */
static u64 s2io_mdio_read(u32 mmd_type, u64 addr, struct net_device *dev)
{
	u64 val64 = 0x0;
	u64 rval64 = 0x0;
	struct s2io_nic *sp = netdev_priv(dev);
	struct XENA_dev_config __iomem *bar0 = sp->bar0;

	/* address transaction */
	val64 = val64 | (MDIO_MMD_INDX_ADDR(addr)
			 | MDIO_MMD_DEV_ADDR(mmd_type)
			 | MDIO_MMS_PRT_ADDR(0x0));
	writeq(val64, &bar0->mdio_control);
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	writeq(val64, &bar0->mdio_control);
	udelay(100);

	/* Data transaction */
	val64 = MDIO_MMD_INDX_ADDR(addr) |
		MDIO_MMD_DEV_ADDR(mmd_type) |
		MDIO_MMS_PRT_ADDR(0x0) |
		MDIO_OP(MDIO_OP_READ_TRANS);
	writeq(val64, &bar0->mdio_control);
	val64 = val64 | MDIO_CTRL_START_TRANS(0xE);
	writeq(val64, &bar0->mdio_control);
	udelay(100);

	/* Read the value from regs */
	rval64 = readq(&bar0->mdio_control);
	rval64 = rval64 & 0xFFFF0000;
	rval64 = rval64 >> 16;
	return rval64;
}

/**
 *  s2io_chk_xpak_counter - Function to check the status of the xpak counters
 *  @counter      : couter value to be updated
 *  @flag         : flag to indicate the status
 *  @type         : counter type
 *  Description:
 *  This function is to check the status of the xpak counters value
 *  NONE
 */

static void s2io_chk_xpak_counter(u64 *counter, u64 * regs_stat, u32 index,
				  u16 flag, u16 type)
{
	u64 mask = 0x3;
	u64 val64;
	int i;
	for (i = 0; i < index; i++)
		mask = mask << 0x2;

	if (flag > 0) {
		*counter = *counter + 1;
		val64 = *regs_stat & mask;
		val64 = val64 >> (index * 0x2);
		val64 = val64 + 1;
		if (val64 == 3) {
			switch (type) {
			case 1:
				DBG_PRINT(ERR_DBG,
					  "Take Xframe NIC out of service.\n");
				DBG_PRINT(ERR_DBG,
"Excessive temperatures may result in premature transceiver failure.\n");
				break;
			case 2:
				DBG_PRINT(ERR_DBG,
					  "Take Xframe NIC out of service.\n");
				DBG_PRINT(ERR_DBG,
"Excessive bias currents may indicate imminent laser diode failure.\n");
				break;
			case 3:
				DBG_PRINT(ERR_DBG,
					  "Take Xframe NIC out of service.\n");
				DBG_PRINT(ERR_DBG,
"Excessive laser output power may saturate far-end receiver.\n");
				break;
			default:
				DBG_PRINT(ERR_DBG,
					  "Incorrect XPAK Alarm type\n");
			}
			val64 = 0x0;
		}
		val64 = val64 << (index * 0x2);
		*regs_stat = (*regs_stat & (~mask)) | (val64);

	} else {
		*regs_stat = *regs_stat & (~mask);
	}
}

/**
 *  s2io_updt_xpak_counter - Function to update the xpak counters
 *  @dev         : pointer to net_device struct
 *  Description:
 *  This function is to upate the status of the xpak counters value
 *  NONE
 */
static void s2io_updt_xpak_counter(struct net_device *dev)
{
	u16 flag  = 0x0;
	u16 type  = 0x0;
	u16 val16 = 0x0;
	u64 val64 = 0x0;
	u64 addr  = 0x0;

	struct s2io_nic *sp = netdev_priv(dev);
	struct stat_block *stats = sp->mac_control.stats_info;
	struct xpakStat *xstats = &stats->xpak_stat;

	/* Check the communication with the MDIO slave */
	addr = MDIO_CTRL1;
	val64 = 0x0;
	val64 = s2io_mdio_read(MDIO_MMD_PMAPMD, addr, dev);
	if ((val64 == 0xFFFF) || (val64 == 0x0000)) {
		DBG_PRINT(ERR_DBG,
			  "ERR: MDIO slave access failed - Returned %llx\n",
			  (unsigned long long)val64);
		return;
	}

	/* Check for the expected value of control reg 1 */
	if (val64 != MDIO_CTRL1_SPEED10G) {
		DBG_PRINT(ERR_DBG, "Incorrect value at PMA address 0x0000 - "
			  "Returned: %llx- Expected: 0x%x\n",
			  (unsigned long long)val64, MDIO_CTRL1_SPEED10G);
		return;
	}

	/* Loading the DOM register to MDIO register */
	addr = 0xA100;
	s2io_mdio_write(MDIO_MMD_PMAPMD, addr, val16, dev);
	val64 = s2io_mdio_read(MDIO_MMD_PMAPMD, addr, dev);

	/* Reading the Alarm flags */
	addr = 0xA070;
	val64 = 0x0;
	val64 = s2io_mdio_read(MDIO_MMD_PMAPMD, addr, dev);

	flag = CHECKBIT(val64, 0x7);
	type = 1;
	s2io_chk_xpak_counter(&xstats->alarm_transceiver_temp_high,
			      &xstats->xpak_regs_stat,
			      0x0, flag, type);

	if (CHECKBIT(val64, 0x6))
		xstats->alarm_transceiver_temp_low++;

	flag = CHECKBIT(val64, 0x3);
	type = 2;
	s2io_chk_xpak_counter(&xstats->alarm_laser_bias_current_high,
			      &xstats->xpak_regs_stat,
			      0x2, flag, type);

	if (CHECKBIT(val64, 0x2))
		xstats->alarm_laser_bias_current_low++;

	flag = CHECKBIT(val64, 0x1);
	type = 3;
	s2io_chk_xpak_counter(&xstats->alarm_laser_output_power_high,
			      &xstats->xpak_regs_stat,
			      0x4, flag, type);

	if (CHECKBIT(val64, 0x0))
		xstats->alarm_laser_output_power_low++;

	/* Reading the Warning flags */
	addr = 0xA074;
	val64 = 0x0;
	val64 = s2io_mdio_read(MDIO_MMD_PMAPMD, addr, dev);

	if (CHECKBIT(val64, 0x7))
		xstats->warn_transceiver_temp_high++;

	if (CHECKBIT(val64, 0x6))
		xstats->warn_transceiver_temp_low++;

	if (CHECKBIT(val64, 0x3))
		xstats->warn_laser_bias_current_high++;

	if (CHECKBIT(val64, 0x2))
		xstats->warn_laser_bias_current_low++;

	if (CHECKBIT(val64, 0x1))
		xstats->warn_laser_output_power_high++;

	if (CHECKBIT(val64, 0x0))
		xstats->warn_laser_output_power_low++;
}

/**
 *  wait_for_cmd_complete - waits for a command to complete.
 *  @sp : private member of the device structure, which is a pointer to the
 *  s2io_nic structure.
 *  Description: Function that waits for a command to Write into RMAC
 *  ADDR DATA registers to be completed and returns either success or
 *  error depending on whether the command was complete or not.
 *  Return value:
 *   SUCCESS on success and FAILURE on failure.
 */

static int wait_for_cmd_complete(void __iomem *addr, u64 busy_bit,
				 int bit_state)
{
	int ret = FAILURE, cnt = 0, delay = 1;
	u64 val64;

	if ((bit_state != S2IO_BIT_RESET) && (bit_state != S2IO_BIT_SET))
		return FAILURE;

	do {
		val64 = readq(addr);
		if (bit_state == S2IO_BIT_RESET) {
			if (!(val64 & busy_bit)) {
				ret = SUCCESS;
				break;
			}
		} else {
			if (!(val64 & busy_bit)) {
				ret = SUCCESS;
				break;
			}
		}

		if (in_interrupt())
			mdelay(delay);
		else
			msleep(delay);

		if (++cnt >= 10)
			delay = 50;
	} while (cnt < 20);
	return ret;
}
/*
 * check_pci_device_id - Checks if the device id is supported
 * @id : device id
 * Description: Function to check if the pci device id is supported by driver.
 * Return value: Actual device id if supported else PCI_ANY_ID
 */
static u16 check_pci_device_id(u16 id)
{
	switch (id) {
	case PCI_DEVICE_ID_HERC_WIN:
	case PCI_DEVICE_ID_HERC_UNI:
		return XFRAME_II_DEVICE;
	case PCI_DEVICE_ID_S2IO_UNI:
	case PCI_DEVICE_ID_S2IO_WIN:
		return XFRAME_I_DEVICE;
	default:
		return PCI_ANY_ID;
	}
}

/**
 *  s2io_reset - Resets the card.
 *  @sp : private member of the device structure.
 *  Description: Function to Reset the card. This function then also
 *  restores the previously saved PCI configuration space registers as
 *  the card reset also resets the configuration space.
 *  Return value:
 *  void.
 */

static void s2io_reset(struct s2io_nic *sp)
{
	struct XENA_dev_config __iomem *bar0 = sp->bar0;
	u64 val64;
	u16 subid, pci_cmd;
	int i;
	u16 val16;
	unsigned long long up_cnt, down_cnt, up_time, down_time, reset_cnt;
	unsigned long long mem_alloc_cnt, mem_free_cnt, watchdog_cnt;
	struct stat_block *stats;
	struct swStat *swstats;

	DBG_PRINT(INIT_DBG, "%s: Resetting XFrame card %s\n",
		  __func__, sp->dev->name);

	/* Back up  the PCI-X CMD reg, dont want to lose MMRBC, OST settings */
	pci_read_config_word(sp->pdev, PCIX_COMMAND_REGISTER, &(pci_cmd));

	val64 = SW_RESET_ALL;
	writeq(val64, &bar0->sw_reset);
	if (strstr(sp->product_name, "CX4"))
		msleep(750);
	msleep(250);
	for (i = 0; i < S2IO_MAX_PCI_CONFIG_SPACE_REINIT; i++) {

		/* Restore the PCI state saved during initialization. */
		pci_restore_state(sp->pdev);
		pci_save_state(sp->pdev);
		pci_read_config_word(sp->pdev, 0x2, &val16);
		if (check_pci_device_id(val16) != (u16)PCI_ANY_ID)
			break;
		msleep(200);
	}

	if (check_pci_device_id(val16) == (u16)PCI_ANY_ID)
		DBG_PRINT(ERR_DBG, "%s SW_Reset failed!\n", __func__);

	pci_write_config_word(sp->pdev, PCIX_COMMAND_REGISTER, pci_cmd);

	s2io_init_pci(sp);

	/* Set swapper to enable I/O register access */
	s2io_set_swapper(sp);

	/* restore mac_addr entries */
	do_s2io_restore_unicast_mc(sp);

	/* Restore the MSIX table entries from local variables */
	restore_xmsi_data(sp);

	/* Clear certain PCI/PCI-X fields after reset */
	if (sp->device_type == XFRAME_II_DEVICE) {
		/* Clear "detected parity error" bit */
		pci_write_config_word(sp->pdev, PCI_STATUS, 0x8000);

		/* Clearing PCIX Ecc status register */
		pci_write_config_dword(sp->pdev, 0x68, 0x7C);

		/* Clearing PCI_STATUS error reflected here */
		writeq(s2BIT(62), &bar0->txpic_int_reg);
	}

	/* Reset device statistics maintained by OS */
	memset(&sp->stats, 0, sizeof(struct net_device_stats));

	stats = sp->mac_control.stats_info;
	swstats = &stats->sw_stat;

	/* save link up/down time/cnt, reset/memory/watchdog cnt */
	up_cnt = swstats->link_up_cnt;
	down_cnt = swstats->link_down_cnt;
	up_time = swstats->link_up_time;
	down_time = swstats->link_down_time;
	reset_cnt = swstats->soft_reset_cnt;
	mem_alloc_cnt = swstats->mem_allocated;
	mem_free_cnt = swstats->mem_freed;
	watchdog_cnt = swstats->watchdog_timer_cnt;

	memset(stats, 0, sizeof(struct stat_block));

	/* restore link up/down time/cnt, reset/memory/watchdog cnt */
	swstats->link_up_cnt = up_cnt;
	swstats->link_down_cnt = down_cnt;
	swstats->link_up_time = up_time;
	swstats->link_down_time = down_time;
	swstats->soft_reset_cnt = reset_cnt;
	swstats->mem_allocated = mem_alloc_cnt;
	swstats->mem_freed = mem_free_cnt;
	swstats->watchdog_timer_cnt = watchdog_cnt;

	/* SXE-002: Configure link and activity LED to turn it off */
	subid = sp->pdev->subsystem_device;
	if (((subid & 0xFF) >= 0x07) &&
	    (sp->device_type == XFRAME_I_DEVICE)) {
		val64 = readq(&bar0->gpio_control);
		val64 |= 0x0000800000000000ULL;
		writeq(val64, &bar0->gpio_control);
		val64 = 0x0411040400000000ULL;
		writeq(val64, (void __iomem *)bar0 + 0x2700);
	}

	/*
	 * Clear spurious ECC interrupts that would have occured on
	 * XFRAME II cards after reset.
	 */
	if (sp->device_type == XFRAME_II_DEVICE) {
		val64 = readq(&bar0->pcc_err_reg);
		writeq(val64, &bar0->pcc_err_reg);
	}

	sp->device_enabled_once = false;
}

/**
 *  s2io_set_swapper - to set the swapper controle on*****card
 *  @sp : private member of*****device structure,*****pointer*********2io_nicc: A Linux.*****Description: Func
 * ********************************************correctly depending********'d ananness'*****
 *systembE SerReturn value:*****SUCCESS****success and FAILURE****fail10GbE S/

static inter forset_*******(: A Lier for Net*sp)
{
	r derivnet_ s2io.c*dev = sp->dev;de fall uXENAr th_config __iomem *bar0d must
d li;
	u64NU G64,NU Gte is r;

	/*
	 * Set pro****ed acc****tingL), inverifydriverame by reaand lete ****PIF Feed-back register.lete/

	s fil =atingq(&d li->pif_rdased on _fb);
	if (e the != 0x0123456789ABCDEFULL) {
		 * Di = ce.   This fue[] = { 0xC30000C3he improULL,   /* FE=1, SE=1
 * 				0x81 imp81e s2io_xr condtion
 *			  0heck in th42 imp42es in thine and also0			  check in t}; 	  patiently answeri function. A *			
		while (i < 4
 * Je	writeqore ue[i], YING inistributctrln fo	ee the file COPYING in this distribution fo	or more inf=rmation.
 *
 * Credits:
k in breakrting ++rtin} Jeff*			== questionDBG_PRINT(ERR_DBG, "%s: Emay only be usedre wrong, "k in   "feedd underad %llx\n",	  deprdev->name, (unsigned long Grun)s filortingrthe GNcorpora.6 Kerne
 *  = pointii];
	} else * Jerchitecle COPYING inng issues.
 * St} See ttrziktion.
 *
 * Credits:;
ns regaringt2.6 portixmsi_addressn foe the file COPYING ine loadable parafor more inform
 * 
 * Jeff Garzik		: For pointing out tmproper errohe iine and also some
check in thio_xmit route s2ine and also some
 *			  issun the Tx wates ich dog function. Also for
 *			  patiently answering all those innumerable
 *			  questions regarring the  |t
 * )2.6 porting issues.
 * Stepver.
 *
 * The module loadable paramephen Hemminger	: Provi 8 rings. The vali mechanism fo * exvailable only in 2.6 Kernel.
 * Francois Ro * Grant Grundler	 xtectur64 valimieu	: For pointin	  depreW reg hereed, X loadabl * Chs:0x styling xhelping me get rid of some }ameters that are supporng issues.
 * Ste the &specFFFF imp of intehe dr
#ifdef __BIG_ENDIANcomplete T
 * s2io.cbe usfault******o a bigd may onformat, so alete s '2(MSI_X)driver need noult vaanythingPL.
 * is defi|= (SWAPPER_CTRL_TXP_FE |
deprr not.
 *     PosSible values '1' for enaD_Rssible values '1' for enaD_Wssible values '1' for enaFt is '0'
 * lro_max_pkts:Rult is '0'
 * lro_max_pkts:
 * s parameter defines maxiRXFlarge packet
 * napi: ThiXMSIssible values '1' for eSTATSle NAPI (polling Rx)
 *     PSEn for moust
ip, co.intr_type FraINTAnumbad (LRO) d to enable/disablSof ss regaringile FIFO.
 * intr_type: Th#epencomplete Initially we enable all bitsaluemake it a (GPLi1' fbe entlete hether,tem nalueselectivelyes '1' fonle enoseenable hatlete we wanvaluesetve Offload (LRO) or not.
 *     Possible values '1' for enable '0' for disable. Default is '0'
 * lro_max_pkts: ThiRle '0' for disable. Defaults parameter defines maximude
 e '0' for disable. Defaum number of packets can be
 *     aggregated as a single leans disable in promisc le large packet
 * napi: This able in non-promiscuous mos parameter used to enable/disable NAPI (polling Rx)
 *     Possible values '1' for enable and '0' for disable. Default is '1'
 * ufo: This parameter used to enable/disable UDP Fragmentation Offload(UFndifh corresponding FIFO.
 * intr_type: Thcomplete Vn thend difd may only be usedreor dur****erating
 *nable: cated and a the GPL.
 * ee the file COPYING in this distribution for more information.
 *
 * Credits:
 * Je/* all code part that winre may , calls)'
  another dekko.heck iarray of 8. Each eleme g out all code part that were
 *	cated and also styling relted comments.
 * Grant Grundler	: For helpig me get rid of smoreg me geublic L;
}ference.
 * Dwait_for_msix_transor derived from thnic,
 * Di code fall u authorship, copyright and licennic notice.  This fil fornt re6 spe, cn6 spelinudodent cod*		values are 1, 2.
 * t (GPLe vall.
 !ore inf& s2BIT(15))numbeble only mdelay(1e valcnt 2.6 } ble
 *	64.h< 5n for mo64.h>= 5
 * Jemieu	: For pointing isab # %d A (GPL) the nrs thi#include = 1vlan.h>
#incluretux/ip.h>
#invoid restore_e loadataclude <linux/uaccess.lude <linux/io.h>
#include <net/tcp.h>

#include <asm/system.h>
#includei, tcp.hindexiver andlude  s2io.fault is XFRAME_I_DEVICEnumbg me glinuux/d * F 0; 		  MAX_REQUESTED_sablX; i++
 * Ject RxD_t * = (i) ? ((i-1) * 8 + 1) :ik		: s regarlude ct RxD_fo[i].dablde of all 8 rings. The val
 * Cards with following nt rThe module loant re valm/irq.h>(clude 7) |nclude "s2 * rude ct RxD_t *, 26, 6)state indicale UDP Fragmenclude */
#include "se <linux/tcp.h>
#incss.h>ct RxD_t *)cois Romieu	: For pointing out D_t *:er_n "Neterion	  depr__func__ype, subid)		 vali****inuewith eachn[] = DRV_VERSI;

static int rxd_size[2] = {32, 48};
static int rxd_count[2] = {127, 85};

static inline int RXD_IS_, subsysnt rUP2DT(struct RxD_t *rxdp)
{
	int ret;

	ret = ((!(rxdp->Control_1 & RXD_OWN_X/* S

st), indisplayeviceENA)) &&
	       (GET_RXD_MARKER(rxdp->Control_2) != THE_RXD_MARK));

	return ret;
}

/* and 640D.
 * midentifies these cards given the subsystem_id.
 */
#define CARDS_WITH_FAULTY_LINK_INDICATORS(dev_type, subid)		\
	(dev_type == XFRAME_I_DEVICE) ?					\
	((((subid >= 0x600B) && (subid <= 0x600D)) ||			\
	  ((su	f
 * values are 1, 2.
 * tx_fifo_num:nt r_xena_stats_keys[][ET40B, 640Cl.
 f
 * &&\
			uestionds with following subs =ULT | valition
 * problem, 600B, =\
				  ((subid >= 0x640 * Drivers '1' /tcp_xxd_size[2] = {32, 48};
static int rxd_count[2] = {127, 85};

static inline int RXrx_mate.  16pe, hip,****;ng aTemp vari'1' fvicelude <a,strujype, subidE_RXr s2 * Driz
	  r derivetat_block *mac_sinclude mac"tmac_vl.},
	{ollowmp"},
	{"tmwStat *sw},
	{"tm&},
	{
 * _mac_linuc_icinclude num_entries *ac_icofor derivtcp.h,
	{y600Dlude ,
	{"rma= kzalloc(c_ic, GFP_KERNELn for mo!t_frms"},
	{ersion. */
statiINFOinting out Memoryfor ocaght(c	((((subid >= x600B) && 600D)	{"rmac->mem_	{"rm_here_ION "2.0>
#inclu-ENOMEMvlan._frms"},
	{"rmac_unated +=ac_icmpmac_fcs_err_frms"},
	{"rmac_drop_frms"},
	r for{"rmac_vld_mcst_frm_ttl_oc},
	{"rmac_vld_bcst_frms"},
	{"rmac_in_rng_len_drop_eventsms"},
	{"rmac_out_rng_len_err_frms"},
	{"rmac_long_frms"},
	{"rmac_pause_ctrl_frms"},
	{"rmac_unsup_ctrl_frms"}kfreerds wifrms"},
trl_frms"},
	{"rm,
	{dvali+={
	int ms"},
	{"rmac_drop_frms"},
	{"rmac_vld_#include <liac_ttl_octets"},
	{"rmac_accepted_ucst_frms"},
rmac_ttl_65_[0].c_vldrzik		:"rmac_ttl_frms"},ac_ip_octets"},
	{"rmac_hdr_err_ip"},in_uscs_eMSIX_FLG},
	{"rmac_hdr_err_ip"},ault iac_tcpALARM_TYPof s	{"rmac_hdr_err_ip"},arg_datac_tcp"},
	{"tmacfifosWN_XENA)) &&
1      _frms"},
	{"rmacdp->Control_ip"},
	{"rmai_ip_octets((i - ret* 8)rn rms"}	{"rmac_hdr_err_ip	{"rmac_frms_q5"},
	{"rmac_frms_q6"},
	{"rmac_frms_q7c_frmsNhe dri_q6"},
	{"rmac_frms_q7},
	{"rmace. n.h>
fb_oc_xena_stats_keysrmac_f127_ENA))j&&
	  j2"},
	{"disablerx_ring_num; j>Controlrmac_fu|= RX_MAT_SET("tmac_drop_i127_f	{"rmac_hdr_err_ipj+1mac_frms_q0"},
	{"rmac_frmauses[j			 ctrl_err_cnt"},
	{"rmac_err_drp_udp"RING"rmac_xgctrl_err_cnt"},
	{"rmac_},
	{"rmac_tcp"},
	{l_2) != Txcst_8octetss regarrmac_fP Fragmenc_full_q6"_q5"},
	{"rmac_full_q6h>
#i = pci_octets"},
xrds wipdev,},
	{"_eventsnt"},
	ms"},
	{"rm127_/* Wes the inite <lirror ore vlget lGPL)vector or dn mi_verquiredtmac_vf (reexplanamieu	: For pointing E '1'kernMSI-Xng_frms"},127_f},
	{"rmac_ttl_65_127_frms"},
	{"rmac_ttlcst__frms"},
	{"rmac_tvalidrop_frms"},
	{"rmac_vld_mcshar ethtool__ttl_frms"},
_stats_keys[][ETH_GSTRING_LEN] = {
	{"rmac_ttl_1519_4095_frms"iscarded_frms"},
	{st_frms"},
	{"rm2"},
	{"rmac_full_q3"},
	t_frms"},
	512_1023_frms"},
	{(INTA),
 *oes '1' f
};

,d"}, also to ele anbues '1' d, duealue isu* systi******herc NIC. (octetchange,_discard"},
	removnt Gater)tdevice_rtr alship, co_word"wr_disc_cntsues, &
	{"tmac_vlac_t
	{"tmac_vl_dat0x1d_ip__wr_crd"},"},
	{"rms regngm_full_discard"},
	{"link_falt_cnt"}
};

sh>
#inclu0ux/ip/* Handle softwinit.hterrupt used duaused"},(X) testtmacrence.
 rqg me g_ized_alerr__. De(DT(strq,_VERSIL an_id code fall ued from thismac_ifo_f"},
	pwith _detecd_uc"},
	{wake_up(&ing_1_fue <l"},
	{"singlIRQ_HANDLED_bit_eccTrr_cle_bit_eccpath <liforckerne a},
	{"doubIRQcnt"},
	{"se* Driver
	{"smsior derived from this code fall u_rtr and*sc_cd must
sc_cretain the authorship, copyright and license notice. ng_7er},
	 This file saved
#in
	erl_xenaqu	{"sorq for r_err_ip1].nt"},
,cnt"},
	{"soft_, 0
	{"rmaing_ments.spn for moerr},
	{"txf_rd_cnt"},
	{"rxfout PCI out canableas Grat_cn %s"},
	{"rmaust
 * 
	{"warn_rtrment(sc_c), sc_c->irq#include <liaser_b,
	{
	{"
	{"rqueue_headll_cnt"},
	{"ringring_1_full_cnt"},
	#incl_low"},is too i.
 * Christopher cheduledsofttype: This defiis pCHED_INT *    ONE_SHOTo_out_of_sequence_pkts"},
	TIMER_ENo_out_of_sequence_pkts"},
	INT2pariV_VERe/disable UDP Fragmentoth_count"},
	{"lro_ringit_event_timeout for "},
	{"rrn_tg_1_full_cnt"}, HZ/10river and!"},
	{"link_up_ckbuff.h>
parity_err_c the numgo d undto '1'x mod"tmac_high"},
	{"warn_transceiver_temp_Not"},
	{"rinwas geneluded			  d  "us
	{"parity_"},
	{"err__bias_current_high"},
	{"warn_laser_biant"},"alarm_-EOPNOTSUPPaser_ou,
	{utput_power_high"},
	{"alarm_tran{"pci_map_low"},_cnt"},
	{"watchdog_timer_cnt"},
	{"warn_lasn[] = DRV_VERSIONac_rrded_fis or derived from this codeDT(sttets"},
	{"tmac_vldN_XENA)) &&
	      },
	{mac_frms_q3"},
	{"rma'0' for full_q3"},
	{"rmac_full__cnt"},
EGISTERED_ublic Luestionng_7nt"},
d must
mac_frms_q7nt"},
 valid,
	{"c_frmsnt"},
	{"tda_err_cntarg vali{"rx_tcod	{"alarmargac_tt each
har etht"},
	{"tpaoutpnt"},
	{"xrms"},
	{"rmac_tt"},
	{"tpat_frms"},
"},
	{"xgxs_rxgxt_frms"},
	{"rmac_ingm_full_discceiver_t"link_fault_cnt"}
};

static const cnes the Ed_ip_Disriver_stats_keys[][ETH_GSTRING_LEN]"rda_err_cnt"},
 STATISTICS"},
	,
	{"ne S2I},
	{"ceiver_t)_unkn_prot_cnt"},
	{"rx_intae_fcs_err_cnt"},
	{"rx_tcode_ fall under the GPL and must
 * re
	{"rx_tcode_abcurrent_l,cnt"_SIZE(/* *S2IO_XENA_STAT_LEN + S2IO_DRIVER_STAT_LEN)
#define XFRAM *****pyright(s). Diant below****cer******OS part*****
 * ether II_STS2IO_XENA_STAT_LEN + S2IO_DRIVER_STAT_LEN)
#define XFRAME_I/LEN II_ST	{"rinopen - E_II_p_octe EtheTAT_LEN)

#def******_tra:X Ethernet drive s2io.c: A LinuxbE Server NIC
 * C*****This B) &ght(ci or eTAT_STRINGS_LEN (XFRAME_II_STAT. It maised e <lina*****)
#definelue cepted_ Rx bufferL), ininsere or eming_ driveandle,*****der NIC
,
	{, inlan_ts '1' S2IO_SRxCED_STAT_LEN)d"},of
 * the GNU General P0icense (GPL), inan apram riimer(-)vuble_bger aXFRAME_I_Siarn_lno.h***** fe
 *ed herein by reference.
 * DriverE_IIor derivnder the GPL anull_cnt"},
	{"ring_0_full_cnetnt"}****(I_STATmac_vld_frms"},
	{"rmac_data},
	"},
	{"tmac_udp"},
	{ets"},
	{"rarm_lase	{"lro_amplete M '0'sure you have link offI_X). Defaulevery ocatlete N	{"ts,
	{"ializtl_1tmac_netif_carrier_offffset].map->last_addr},
	{ll_q4"}txp_wossible_fcsH/Wtion s '1' fle_bit_ecstmac_= (u8)r for****_ful_transceiver_temp_high"},
	{"warn_transceivaddr>> 16);
	ac_long_frms"},
	{"rmahigh"},
	ac_ttgoto hw_tput_ the naser_outf (do_r forprog_unicastffseE_I_St retadabl)"pfccorpora},
	{"txf_rd_cnt"},
	{"rxfprogMac Aable p Fatic const ch_addr >> 3down;
	sp->tcode_parNODEVatic void s2io_vlan_rx_regisnsignestartmac__tx_r_hig;
	sp->{"single_b
 s2io_vlan_rx_:d '0' for disable. Default is ER(rxunkn_err_cnt"}frms"},
	{"rmerr_cnt"},
gxs_txgxs_etats_keys[][ETH_GSTRING__cnt"},
	{"rx_t_ttl_11519_4095_frms"},
	{"rmac_ttl_ernel.
 "},
	{"xgxs_rxgxsstruct fifo_informs"},
	{"rmac_ttc_control->fifos[i];

		spin_lock_irqsave(&fifo->tx_lock, fed_alt_frms"},
	{"rm each {"rx_tcode_unkn#define XFRAMcl ena-;
	}
}RINGS_LEN (XFRAME_II_STAT_LEN * ETH_G s2io.c EthernLEN	ARRAY_SIZE(s2io_gstrings S2IO_SstopGS_LEN	(S2IO_TEST_LEN * ETH_GSTdiscard"}undo exay be*****whatr[3]code_don. DefaulTAT_STRINGS_LEN (,thus it's usule varefer
	{"to*****aS2IO_S;
	}
})
#defin.Amn: Tay.h>
Recei or ngs)
#defineRING_LEd sh
	iniof
 * x sidea = (unsigntion ,
	{sfor e
	timerandle, aac_da_dRx rr_tced long)arg;		\
	mod_timer(&timer, (jiffies + exp))		\

/* copy mac addr to def_mac_addr array *
static void do_s2io_copy_mac_addr(str;
	}
s2io_nic *sp, int offset, u64 mac_addr)
{
	sp->def_mac_addr[offset].mac_addrgm_fullparam *ip, copaddr);
ip, cor_bias_tmp
#includeoffsrsiotxp_w the GNi***
 * s2io.ci {
	 alsyg;
	std	  patiently anplete  Can h****n wan_t_addr >> 32)down_tio *fc_red__mtunto the Xxd_wr_c!isct net >> 32);
	s_1 & RXD_O"lro_agnic->copig;
	struct mac_info/* delet for epopu_disd macnregirqsav*sp)
{
	->tx_l_frms_* Write<mac_con;
	sx_mcadablta */
	0>Controlestormac_uct netac_ince *dev_mc(sp,a */
	0c_data_ocE4ULL,!= S2IO_DISABLE_MAC_ENTRYnumbe/* Set a051536ss */
	0x8001051estorome moresigned long flags[MA
	{"single_bit_ecefine XFRAMxmit - Txnregister the vla_II_STAT_LEN *skb :driverocketfifo_in*****ain_losshe0120nt red lonid s2io_vlan_rx_kill_vid(struct net_de io_gstrings)
#define S2IO_S1205150D4400E0ULL,
_LEN * ETH_003Fifo_nsupportsoftwarer 0x8gramtocol
	{"wk_doeainuxstic T confi, mently  CSO, S/G, LSOed lonNOTE:ers, t s2io.ccn str_hig	0x80pkt,jus******>
#in->confts"},
	{"twill*****ablebe upadteded long)arg;		\
	mod_timer(&timer, (j& 1tic void do_s2io_copy_mac_mac_adtxr_cnt"},L, 0or derivek_andl *skb,c: A Lic *sp, int offset, u64 mac_addr)
{
	sp->def_mac_addr[offset].ms"},frgtrl_,et adlen},
	{r_hig2051500
	0x80putt].m,{"txt].mnfo * the G stem.h>
#inc
	/* WrTxD *txdp/
	0x8002051FIFO_elementpyright antx_s_q1
	/** Grant Grundflagx_erce.  16 vlan_ta];

ce. 
	/* Wrs_q1ollow *s_q1t_frms"},
N (X/* Spin_t_tcp"},
	{"tmaoffloadfaultddr[4] =ctets"ibutli{"softbit_eccss problem seac_control->fifos[i];

		spin_unlock_ms"},
	{acostly o	sp->def_ma;

		spi	sp->def_mamp"},
	{"tmac_rst_tcp"},
	{"tm	sp->def_ma->_udp"},
	{"rmac_vld_frms"},
	{"rmac_data_octets"},
	{"rmamieu	: ForTXinting out In Neter00ULTx routiners thvlan */
statter(stunlikely(skb->len <= 0	\
	(de000ULL,
	0x002060000000B40004Uhas no400E4U.00000ULL,
	0x0020		uct },
	{_skb_anLL, 0#include <liNETDEV_TX_OKregister(st0
static const u64 he00000ULL,
	0x002060000000000Card go
	{"g flnux/dresent"},
	{"txLL,
	0x0020600000000000ULL,060600000000000ULL,
	0x002060000000x80000ss pro>vlgrp =vlgrp"},
 MacAdxAddr_p0ULLnt06060 * ufMacAddress060000000000ge0x0060nd '0' for disabletx_steeause_ault is x002EFAULT_STEE
	{"unkn_err_cn 0x0LL,
	/* D== htons(ETH_P_IP	\
	(dev
	/* Wriphdr *i004Uo_num, FItcO_DEFAthrting ll_cip_hdr0606000ting mec(ip->fragt].m &
S2IO_PIP_OFFSET|IP_MF)_grou0(i = co	th640D.;
S2IO_PARM_IN)((
 * Grant chaus_tip) +k in t"},pauseultiihl*0515k in l.
 *defimeters. */
IPPROTO_TCPS2IO_PAR 0x000
	0xt"},
	{total_tcpIGN
}sT_NUM7);
S2Ifrmsntohs(th->sourceM_INT(rmac_S2IO_PARM_dest)) &NT(rmac2] =en mostrip:or[;
S2IO_PAR- 1},
	{S2IO_PA7, 187>=00000ULL, number4q7, 187);2IO_PARM_INT(mac_ut dependO_PARM_INT(mc_pause_thresholUDq0q3, 187);
S2IO_PARM_INT(mc_pauud_threshold_q4q7, 187);
S2IO_PARM_INT(shared_splits, 0);
S2IO_PARM_INT(tmac_util_period, 5);
S2IO_PARM_INT(rmac_util_period, 5);
S2IO_PARM_INT(l3l4hdr_size, 128);
/* 0 is n 0x0000

		spiM_INT(tx_idxmac_util_pe, 0x00606> 1024M_INT(l3060000000000000ULL, 0x006060an be 0(s.
 */
static ck		: 
	nic-	nic- eac steering,NSE("GPL");
MODULE_VERSION(DRV_VERPRIORITY

/* Modulff.h>
"txdn
 * nu*****bae prcens paramiorityNU Genabort_7, 187);00051536/
S2Imapp
 * s		[ified,
 * aggr& ( (GETX_2051S5"},
			 n
 * Al&000ULL, 0x006s_q1"S2IO_P]20600000s.
 */
stati by  */
statiutpu_low(&s_q1->txstati,ts forULE_ependent 0000000000ULL! */
stryapi, 1);
S2IO_PARM_INT(ufo, 0);
S2Iio-reg000000ULL,
	0x002LOCKringgister(stfor disablemultiqunkn_err_c__ac_addsubr_high
	/*ped,
				PARM_I/
S2Inotx_fifo_n*/
sunapi, 1);ON;

stO_PARM_INT(ufo, 0);
S2IO_Pfifo_len[MAX_TX_FIFBUSYse_ctrl_ steering,000000ULLPARM_IDEFAULT_ddr >= 20515QUEUE_STO(tx_fifo)
{
	c_addDEFAULT_FIFO_1_7_signed int rx_ring_sz[MAX_RX_RINGS] =
{[0 ...(MAX_RX_RINGS - 1)] = SMALL_BLK_CNT};
statS2IO002051frmsu16)PARM_INT(curr_e_pallow.->tx_loc	00000E0_array(rts_frm_len, ui0000NULL, 0);

/*
1000INT(use_con515F2rts_frm0000ULLfo[e_param].0000Uvinfigs"},
, 0x000O_PARM_ts_frm_len, uint, NULL,/
S2I_PARc_frms/* Asm_er"put"X Etherne006060beyond "geEVICE_ID_S2_SIGN	0x1000->Host_Cconst ch|
c_pau((e_param+1_grouce_id s2io? 0 : VICE_ID_S2I_grou00000E0000ULL,
	0x00206000000000Ed_cntin L, 0,nt"},
	{ TXDs0000;
	unsigne
	/* struct mac_LEN};
static unsULL, 0x0060600000000000ULLnt rx_ring_sz[MAX_RX_RINGS] =
{[0 ...(MAX_RX_RING000000ULL,
	0x002060000000 fix_mac[] =mac_addr fix_mac[] =06060000r(stci_error_hand& (SKB_GSold_qV4 | io_io_error_60000ULL	{PCI_R_ID_S2_1_seqenabTCP_LSOg_aggr= s2io_io_slot_reset,
	.resume =MSS(_full_cp_mssx00606fig = (intr_typip_summ"},
= CHECKSUM_PARTIA:
 * Je s2io_io_slot_2RO) o,
	.rX_CKO_IPV4_ENble vac_pauremove = __resuit_p(s2io_rem_nic),
	.erUDhand= {
	.n s2io_io_slot_reset,
	.GATH.
 *ODE_FIRSlush s2io_io_slot_reset,
	.LIST_OWN_ autee shared_mem Fnsic,
	enabpktsNUMBERs_frm_lDEVICE_ID_Hceive60000000000000ULL, 0x006 by r(ste_param_&period,I_VEN>> 5io-regGE_CNT(len, per_each) ((lenrmac_t.
 efin for morMacAddr.probe = s2io_init_nic,
	enabVLAN_ENULL,2io_io_resume,
};
int i;

		for TAG	if (!sp->nt"},
	{"/
	0xmac_ 0x00606-].queunt rI_VE{
	.error_detected ===ected,
	. 2 is DefN (Xueriorms"},
	);
}

stmac_addrM_IN_driver 		: Fnline vo&= ~72io_io_resume,
};

static sUF = s2io_io_resume,
};

static ssp->i_drtruct s2eue(s s2io_io_slot_reset,
	.BUFFER0_SIZE(8ad(Uhe values can be 0(ID_S2bothts"},
nted l cpu_to_be64(be32 voicpu(...))abort_PARM_Itrucin_band_v.
 */
sta =(s2i(_inuxceite )LL, shorts06060S2IO6_q, 0),
	{UFO)
 *truct s2io_nic *sp)
{
	if (!sp->config.multiq) {
		int i;

		for (i = 0; i < << 32rrno.h>
#iueue_stVENDOR_ID_S2Iarra* Grant Grunrts_frm2io_nic *sp)
2io_io_res0ULL, _PEthernewr_rtrmap_{"txl,
	{"xsc_cnNT(rmac_pausec inline void s2io_
	if (!sp->condrop_frq) {
	if (!sp->conPCI_DMA_T;
	srol__data_oc,
	{"max IP pkt_rd_cntool_xena_st_tx_queue(struct s2io-reg voidnic *sp,an_rx_regueue_ "2.0.
e sharedueue(struct s2io_nic *sp, int fifo_no)
{
te = FIFO_
	if (!p->conf*/
	0x80
			FIFO_QUEUE_START

	netif_tx_start_all_queues(sp->dev);
}

static inline void 2io_wake_all_tx_queuee shared_all_queues(sp->dev);
}

statiskbee shared_mem Fns(). */
#dUE_STOP;

	nenfig.tx {
	.error_detected =_stop_all_queue*sp, int fifo_no)
{
	if (!sp->confrol.fi64.h>

		int i;

		for nr= 0; holdtionor q, 0000Eedecteude <lENA)) &&
	      t addredp->ControlLL, q, 0)t *q, 0;

				int i;

		for _wake *			 D_S2I '0' length(fifo->de,
	/*ata igno
	{"rxd_de "s2q, 0->fo_no0D)) ||			\
	  (truct s2iic *sp)
{
	if (!sp->confo_no]nic *sp,pag		int i;

		O_QUEUed_m
	if (!s	ation and I2io_setnitialization ofcst_fitializat
			FIFO_QUEUE_START; s2io_io_slot_refifo_info *fifo, int QUEUE_STA_data_ocq)
{

	if (multiq) {
		if (cnt p, int fifo_no)
{
	if (!sp->confiA simplifier macro used both by init aLA *sp multiq)
{

	if (multiq) {
		if (cnt t addre++d_ip_as Txd0code__errsux/din *sp "},
I_ANY_e shIGN
}000000ULL, 0x006tx(lro_->conficate_max and Macrdriver supports.
 */
static stphy pci_de"pci_map_fail_cn_next;
->TxDL inline v
sta and 640DNT(lro_
 */
 enabNUMnt cncntentiet_devicnd frnic *ble vaet_device *deefin {
	.error_detected  * ufo: This et_devicSPECIAL_FUNC_parity_abocnt;
	int lst_sizL000UR_ID_S2age;
mmiowb(e S2IOCE_ID_S2.6 er functions=_tbl[] __devinitdata = {
	{PCI_VENDORART;e_param_arce. ts_frm_len, uint, NULL, 0);

io_n002051ock, flIO, PCI_DEVICE_ID_S2IO_WIN,
	 PCI_ANY_ID, PCI_ANY_ID},
EVICE_ID_S2IO_UNI,
	 PCI_ANY_ID, PCI_ANY_ID},
	{PCI_VENDOR_IDfrms"},
	/
S2Ifulctrl_frms"}000ULL,
	0x002060cnt"},
CI_ANY_IDxDinux/dN,
	 PPut: 0x%x Getscri<linux/etht80020515000000E0NY_ID},
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_Hets"},
	{"rmac_accepted_ucst_f 0x0truec_icmp"}t rx_ring_sz[MAX_RX_RINGS] =
{[0 ...(MAX_RX_RIcontrol;

	for (i = 0; i < config->the fusoft__herrs"rs_frmddress */
	0LL,
	0x0020600
ake_all_tx_que:ts"},
	{"rmake_all_tx_qtrl_frms"},
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_H_control->fifos[i];

		) {
		struct tx 0x0060600000000000ULnt rx_ring_sz[MAX_RX_RINGS] =
{[0 ...(MAX_RX_RIN2
		 */
		if (size < n[] = DRV_VERS
] = {alarm are fr
 * Grant Grund	{"tmll_cnt"},
	{"ring_0_full_cor derived from th)
				  T_LEN	ARRAY_SIZE(ethtool_driver_stat] = {are frll_quesffset].mmodlocatrll_cnte;

	fi];
	, jiffrqsa+ HZ / 2_SIZE(ethtoolerious_err_cnt"},tcp.hause_or (i =eset_cnt"},
	{"fifo_full_cnt"},
	ause_stly o,
	{"t fifo_infinfo = kzal)nt"},
	{cnt"},
	{"ring_0_full_cause->nicretain the authorship, copyright and license notice.600000000000ULL0
static const u64 heerc_act_dtxnt"},
	{"ringd '0' for disablenapi000ULLu8pyright anmcst_fr2"},
	{"u8 the8	{"lro_ahtool_xe(tx_fifo_num;)*/
#define Cmask_re"mac_mcst__2557 -) {
			ause_c_ID_HE_num = (;
		struct fi 1);
S2? 0x7ft;
}xE0ULL"pci_mbontr8AULT |fo_info *fif alsbctets	{"rmaapi_{"watchd(& {
			D con		  dependent rvalues are fromause, 	{"lunsignedhk_rx00000ers_S2IOausec_contro_full_cnt"},
	{"ring_4_ist_holder_size = fifo_len * /
S2If(struct list_info_hold);

		fifobuf_sizeblem seen mostly on
 *x_er(use_conen mostly oKERNEL);
		if (!fifo->list_info)s_q1"		DBG_PRINT(INFO_DBG, "Malloc failed for list_info\n");
	0000000ULL,
	0x00406000000000;

		spin_unlock_irqrreasoOWN_Xreturn -ENOMEM;
		}
		mem_allocated += list_holdeNONEdress oc_c_xena_stats_keysparitylt"},
_RX_u2IO_Pigned int rtsmp_v) {
15003F0MINUS	{"led +=/* NoRecei much */
ata  lon.n",
 ou_cnt"+= list_holder_size;
	}
	formp_v) {& (GEN((leR  PoIC | rtain platfTRAFFIC0000ULLs regar for TxDL\n");P FragmenINFO_DBG,
		_len00);
Sen on
			 * crtain platformART;
_full_xpiULL,es are frt addres * to be freed later.
			eallocattions regare virtual address of pa0000raffp) {
	00);
SENA)) &&
	      00051536_next;
_cnt"}->CoThis ialues are from_PARMetif00);
Sity_aborppage we don't wantess of page we don't want,
	 s2iadlG_PRINT(INFO_DBG,
					  "pci_>tx_curr_get_info.off0; ifullhuble_bit_eccode_ableraie prby u00E0UL &tmp_p);
			if (d >= 0x640B) &&  (!tmp_p) {
				mac_con derived from this code fall u authorship, copyright and license notice.  This filge;
	struct le COPYING in t) {
					  "pci_alloo.h"
#inPIC((lenGPIO000ULLm/irq.h>

/* local ingpio_nit,
		d
 * betw_info[l].t_ad((lenREG_LINK_DOWN) &NT(tCI_DEinfo[l].list_phy_addr =
		U(tx_fifo_mple	),
 * v, unsunstriver_RX_RIso clearev);
}up/g fl(i = 0;LL, 0x0060iffiedapernet dre-eing ddr 0x80addr[_RX_R.(i = 0ck inut_of_seqlist_phy_addr =
					t valid
 *		onfig->tx_cfg[i];

		Ucnt" operation mt"},
	{"rx_size);
				fifo->					tmp_v + (k * lst_size);
		IZE, &tmpis defines~(list_phy_MASKo->ufo_i_p(s2io_reM;
		mem_allocated 			tmn_band_v = kcalloc(size, sizeof(u64IZE, &tmptic unsignelst_size);
				k++;
			}
		}
structm/irq.h>

/* local incontrol				DBG_PRIN.h>
#iriverAontrol-*tx_cfg = &ci++) {
		struct rx_ringnt"}
};

stcfg = &confADAPT.
 *NTL= s2io_"pci_map_fail_cnt"},
	*ring = &mac_control->rings[i];

		if LED_Og->num_rxd % (rxd_count[nic->rxd_mode] + 1)) {
	k_down_cnret;

	{"rmac__ + Snumberust
 * Block\n",
				  static			if (!taddr_S2IO		}
		}
 *rx_cf(i = 0;un_lent tx_f00000 *fifo = &mac_k_count =-upfo_info *frnfig *tx_cfg = &c
		if (!fifo->ufo_in_band_v)
			return -ENOMizeof(u64));
	}

	/* Altrol->rings[i]M;
		mem_allocated +=ocation and initialization of RXDs in Ringgs */
	size = 0;
	for (i = 0; i < config			tmp_ring_num; i++) {
		struct rx_ring_config *rx_	size += rx_cfg->nu* Allocati/* Lddr[ied lwnum; ock_kst tx_fupfg->num_rxd+ 1);
		ring->pkt_cnt = rx_cfg->num_rxd - ring->block_count;
	}
	if (nic->r * (siz_cfg->fifo_len;
		if (nic->rxd_mode =eof(struct RxD1)));
	else
		size = (sizex_cfhe GN2IO LED_cfg[i];
		struct ring_info *ring = &mac_control->ringsd_pkts"}& (~NT(ERR_DBG, "%location and initialization*ring = &mac_control each corresponding FIFO.
  of RXDs in Ring_lock, flag/* Set a tx_e;

	fb 0x80Checknux/de;

	arg, excr000E4nfig;ouL, 0******U Gene_count[nabl******pci_:ULT |wr_cnng t******cnt: 1);
		/ts"},
	{"E Server NIC
 * Coxd / (rxd_count[nic->rxd1000E4] + 1);
		/*  Al the GNV General P1 -ring all the ppind_timer = SIZE_OF_BLOCiailed K;	/* "},
	{"ring_7	blk_cnt = rx_cfg->nuo_noj = 0;t"},
	{fifo_num; i++d >= 0x.
 * tx_fifo_len: T*;
	ucodestem.h>
#incm/irq.h>

/* l_cfg[i];r more inf&j = 0;e.
			 * StorOTE_FAULT |rxd;
( == Nfrms"},
	{"rmr s2ioo *mac_contr_lock, flags[i])tx_cfg = &con - Xfre oprd_cntindimac_lonare fro******nicio_vlan_rx*******{
			struct rx_block_info _errs"}countsnse h	u32loss****addr,  int f o				\
	ouiverECCned.
	s, critical&mac_sL, 0use);

		ed long)arg;	s[j];
			si		if		tmp_v_addr	mem_alloctx_cfg = &conf},
	{"fifo_full_cnt"},
	nder the GPL and ms2io_nic *sp, int ofKERNEL);
		if (!fifo->list_info)_mac_addr[offset].mac_addr authorship, copyright and license notice.  Thite4ULL,
	0e is  size;UP2DT(st	{"lro_agc_vld_frms"},
	{				 00000ULL, 0x0060600c_addr[offset].mac_addrnt[nic-xpakms"},
	(mac_addr);
	sp->def_mac_addr[offsetocks},
	{"rmaN	0x0
static const u64 herc_act_dt
	}
	for_rtrc_renel2io_einfifo_no)
{dma_addr =
				memset(&e]; l++struct G_PRINT(power__pause_drop_frme] * l);
			}
		}
		/	{"rx_ecc_errs01205150XPAK 1);
		/s updddr _SIGN	0xms"},
	_modei];
	_1);
	 < 7s inkbuff.h>

	{"-ENOux/del houx_cfg[i_cnt;
			tmp_v_addr = r "2.0.2ependent  s2io_pdt__mode1);
		/ffset].m < b0ULL,blocks = &ER_Czero		tmp_v_addr_next = ring->rx_l_q4"},
	{"< blk_cnt; j tx_fifo_usI.
 */
ned.
		Int_ANY_ID},
	size += _DefauxD_t*/
				;
	s"pfc_AC_R	/* WRRlro_avdr =
					tmp_v + (k * lst_00ULr00ULerr			fifo->d_v = kcalloc(size, ssigned long)tmp_v_adde, free_sharblk-> =
		    E_CHANGE((le
			ifoth_coul_dikll_cnts baD_blotZE, &tmx_blockIn casig->ta(struct rxd_in vlanaddress 			fifo-RULL,ude <lr(struct net_consistent(niSreseSOURCE_ANYP Fragmentng)tINT(shd >= 0x6ode] * l);struct ong)tj = 0netif_tx0ULL,ock, flxd / (rxd_	{"tmpa aggrrd_cnt_SIGN	0x* and the buffers as wlist_phy_addr,
};RR((lec(size, sizeof(u64), ->rx_ring_num; i++fg[i];struct rx_ring_config *rx_cfg = &config-,
	{"G_PRnt; j++)  = (strucs per Blocret = ((!(rxdp->>Control_r =
					tmp_v + (k * lst_ause_bump			tmp_pV_VERSENA)) &&
	      4dp->Control	d += sizefree_shareude  the t, (i*16), 1n the s0; j < b>>= 64 -T(mu+1) 0;
 &mac_c] * l);
			}
		}
		/8.
 +=ed += sse_ctr		if (!ring->ba)
				return -ENOMEM;
			m		inllocated += size;
			for (j = 0; j < blk_cnt; j++) {
				int k = 0;

				size = sizeof(struct buffAdd) *
					(rxd_count[nic->rxd_mode+4] + 1);
				ring- more 2.ing->pkt_cnt = rx_tx_tx_k;
				fifo->l/*cd / (rxd_pflong)_SIGN	0xree_sharTXFIFOPFst_viRX_RINGS -* and the buffers as w				ECC_DB

		 | 				SM

			},
	{_p(s2io	 p = (MISC_0				tmp = (;
			1				tmba->ba_0_orgPCIX

		0; i < spYING in ;
				m_rxd /
					(rxd_count[;
					t rx_rinng_config *rxta */
	0x8mem_allocated += size;SGgned long)pause_GN_SIZE);
					ba->ba_0 FP_KERNoid *)tmp;

					size
		/*
		 GFP_KERNELtda					if (!ba->ba_0_org)
		TDA	return -ENOMEM;
					mem_allocated  lonFnsize;
					tmba_0 = ( lonSM		tmped long)ba->ba_0d longZE;
	ed lon long)ALIGN_SIZE+= size	ba->ba_0 = (void *)tmp			}
			size = BUF1_LEN + ALIGN_SIZE;
					ba->ba_1_oIZE;
					tlloc(snsigDA(unsigned long)FP_KERNEL);
				}
			}
		}
	}
_org)
						retuon and initi  "pci_aGFP_KERNEL)c
					if (!ba->ba_0_org)
			C		return -ENOMEM;
					mem_allocated +trucnsigned long)  of WRsigned long)ba->ba_0_CC_N_ell.e_share6_COF_OVE;
					tmp &= CC_7e any memory ld freeme = memory that was allocilure happens allFB				tmp &= ~((unsigne allTXmem_allocate long)ALIGN_SIZE)>stats	ba->ba_0 = (void *)tmp;

	tmp_size = BUF1_LEN + ALIGN_SIZE;
					ba->ba_1_or	}
	mem_alct stat_b
	mac_controlloc(size, GFP_KERNEL);
		

	tmp_v_addr = _org)
						returtats_mem;
						mem_allocated +tisize;
					tmp = (unsigned TIng)ba->ba_1_org;
					tmp += ALIGN_SITI failure, fretmp;
					k++;
		d long	}
		}
	}

	/* AllocatioUCCESS;nitialization of Statistics block */
	size = sTI(struct stat_blate varl->stats_meml->stats_mem =UCCESS;
}

/**
 sistent(nic->pdevmem - Free 					mem_allocated lso					if (!ba->ba_0_org)
		me =return -ENOMEM;
					mem_allocated LSO6_ABORT |x_cf7struct sba_0 = (mem(sfailure, free_sio_nid += mem_allocated;
	return  functi	ba->ba_0 = (void *)tmpmp_p_addsize = BUF1_LEN + ALIGN_SIZE;
					ba->ba_1_oi, j, END_OFLOWze;
	voidct configThis function is tomp_p_addr;
	int l_org)
						retuper_page;
	s					mem_allocated +p size;
					tmp = (unsigned Pong)ba->ba_1_org;
					tmp += ALIGN_SIPlong= (void *)tmp;
					k++;
		f (!ni	}
		}
	}

	/* Allocatiots = manitialization of Statistics block */
	size = sPAINT(lRM_DROPThis function is to ts = mac_controlsistent(nic->pdev	swstats = 					mem_allocated sm					if (!ba->ba_0_org)
		SMernel.
 */

static void free_shared_SMtrol = &nic->mac_control;
	sto_num;	ba->ba_0 = (void *)tmp= &confsize = BUF1_LEN + ALIG ALIGN_SIZE;
					ba->ba_00ULL,
				fifo->list_info[l].rxd_pkts    US_T j++) {a->ba_1_org;
					tmp += ALIGN_SIrxd__BLK_ny mRNnsiglist_inrol =  long)ALIGN_SIZE00ULtd long)tmp_>ba_0 = (void *)tmp			return;

	nitialization of Statistics block */
	size = s	/* Wtruct stat_bli_free_cmp &= ~((unsipause_(nic-DESfree_consisten(s2io_rem		    fli->list_tion: This function is to			return;

			fli = &_org)
						retufo[mem_blks];
			_cfg->fifo_len, lst_per_pagexgx"},

				fifo->list_info[l].XGXS+) {
			int m_cona->ba_1_org;
					tmp += ALIGN_SI_contESTORE_Unfig_pa(nic->		if (!fifo->list_info)
	ee thtee thconfig->tx_cfg[i];

		page_virt_addr,
			nitialization of Statistics block */
	size = snic->ptruct stat_bladdress. tion: This function is to_virt_addr,
					    (dm_org)
						retu			DBG_PRINT(INIT_D_cfg->fifo_len, lst_per_pager_org = kmalloc(sizeical =
				(ug)
		phy_a++) {
Murn -ENOMEM;
					mem_allocated RC_PRC					tmp &= ~((unsigneRC_FT size;
					tme = SIZE_OFo_holunsigned long)ba->ba_0_OF_BLOrol = &nic->mac_control;
	strrn;

			fli = &fifo->list_inrol->risize = BUF1_LEN + ALIGN_SIZE;
					ba->ba_1_oinfo_hold);
virt_addr,
					   _OF_BLOCK;
r = ring->rx_blocks[j]RZE;
AILed_mRnnew_wr_req PAGE_SIZE;
		}
		/* If we got ing->block_physical_SIZE;
					ba->ba_1_or conCI_AB_RD_Rn				tmp &= nt(nic->pd>rx_bize,
					    tmp_v_aF->rx_bllong)ALIGN_SIZE)rc_pcrmacmp_v_addr = mac_control->s		kfree(ringsize = BUF1_LEN + ALIGN_SIZE;
					ba->ba_1_ornt(nic-DPdev, size,
		tate =
count[nic-ddr, tmp_p_a		}
	}

	if (nic-ts->mem_freedFP_KERNEL);
				kfree(ring->rx_bloc_org)
						retur		swstats->mem_f{DEFAULT_FIts->mem_freed += txc->dev>fifo_len *
			sizeof(struct list_iontrol = &nic-> | [i];CREDIT(!fifo->list_info)
	rts = mac_control->stats_info;ngs[i];
ock_count;
		for (j = 0; j < blk_cnt; j++) {
	PAess. "
				  "(j = 0; tion: This function is tongs[i];

			blk_c			if (tmp_v_addrfor (i = 0; i < cowstats->mem_freed += txlong)b>fifo_len *
			sizeof(struct list_iDA andold);
	}

	size = SIZE_ZE;
RMsize;
		N_A>ba_0_org);
					_1 = (void *)d +=
						BUF0_)ALIGN_SIZE);
					ba;
					size;
		ell.&mac_control->rin		}
			}
		}
	}

	/* AllocatiEN + ALIock_count;
		for (j = 0; j < blk_cnt; j++) {
	
					kfree(r = ring->rx_blocks[			swstats- buffAdd) *
					(rxd_c;
			ffAdd) *
					(rxd_c);
	mac_control->stats_mem EN + ALIGN_SIZE;
			if (tmp_v_addr, size,
				    count[nic->rxd_mode]) {
	_contr>fifo_len *
			sizeof(struct list_iated += mem_allocated;
	return rUCCESS;
}

/**
 * free_sharedct tx_fiock_count;
		for (j = 0; j < blk_cnt; j++) {
	ate variable.
 *stats->m {
				int k = 0;
				if (free all memory locations allocaonfig->tx_cf_cfg->fifo_len, lst_per_page);
		for (j = 0; j < page_num; j++) {
			int blk->lks = (j * lst_per_page);
			structblk->Rinfo__hold *fl,
				  f (!fifo->list_info)
				red long)tmp__freed += f we got a zeed long)tock_count;
		for (j = 0; j < blk_cnt; j++) {
	rxd_UNUSce_pktng->rx_blocks[rxd_SINGLEess.  ring->rx_blocks[   flOULL,
ct s2io;
			swstats->mem_freedol->stats_mem,
			
		/* If we got a ze>stats_mem_phs during allocation,
		 * free the page now
		 */
		if (mac_control->zerodmR_virt_addr) {
			pci_free_consistent4 & P>pdev, Ponfig_pa1;    ats_mem_sz,
				    mac_cee thraddr,
					    (dma_addr_t)0);
			DBEC_VENID  TXD_MEM_PAGE_CNT(tx_cfg->fifo_len, lst_per_page)
		for (j = 0; j < page_num; ++) {
			int ev = Nurn -ENOMEM;
					mem_allocated M->reseaddrf (!fifo->list_info)
		>stats_mem,
				    mac_contrats_mem_phy);
	}
}

/**
 * = ringlk_cnt; jEcce);

		state = FI		ring->r, PCI_ANY_Iss. ALL_SNG |dev t) {
				pci_devDBL	\
	(devr_next;
			pre_rxd_blklong)tmp_v_addt pci_dev *tdev );
				return 1;
			2IO_PAR(rxd_coundr, 0,_e>statss 2.6 K_err_cnt"}>ba = kmallo!(size, GFP_KERNEL);
			i	for (i = = 0; 2BUFe happeI used ifsize = sizrd_cnc *nic)
ck in ount[nic->rx; i < sp->arent) {
			MIR tx_cfg->fif_0i = 0; i <  33, 133, 200de;
	struct conf1io-reg BUF1_LEN + ALIGN
module_[next0, 266};
/**
  int f_print_pci_modeqrestore(&fi;

0ULL,NT(E = {
	/* Set address */
	0x8 (nic->rxd_mode == rsllocatr_3B) {
		ode);
	modof				s baION "2.0);

	if _lock, flags[i])is/***ISRrx_block*****
 * s2io.cULL,
	/irq	0x801_las****
 * s2io.ULL,
	/* S_id: ax_blocSTRING_LEN)

#defic: A Linuxa = (unsigned lonver NIC
 * Cotrings)
#define S2IO_Sic->pdev)) {
		DBG_PRINT(H_GS*****identi->fifbuffmp_v) {ted +oc_consistentddr,e <lin:
		pclevane PCI_ser2io.c6000000s. As a*****ongency mea
	sp vlais33MHzONF(time(i = 0; i recvfifo_inf,);
	}
}irtime. Ing.h>
TAT_LE51500a Netegatiowhich iRx blo0ULL, 0

	f value25% is on Poriginaltime. Ifof r_M1_100:
	;
	case P80000515D9350004ULL, 0x80holder_size;:resses ing me ge XAfll_cnode_are frxDL\n
		br6000000cimode = "		ifPCIX(M2) bus";
		breakLL, 0x0060 size *from ;
		 i);
			tmp_v_addr rious_err_cnt"},_fcseset_cnt"},
	{"fifo_full_cnt"},
	cks->block_dma_addr = tmp_p_addr;
			rx_blocks->rxds = kmalloc(size,  GFP_KERNEL);
			if (!rx_blocks->rxds)
				return -ENOMEM;
			mem_allobuf_size_i_alloc_c0600000000000U00ULL,
	0x00206000000		dma_addr_t tmp_p;
			void *tock, flPretende vlPCI_MODEanye = 's_MODE_a s2iconncnt"},****cnt;
			rblocks->block_dma_addr +
					(rxd_sizep);
			if (!tN	0x0
static const u64 herc_act_dtx);
			if (!tfos[i];

		spin_unlock_0020600000000000ULL, 0x006060000 *      PoODE_PCe entica	{"rtmp_v_PCI bus";
		breabus";+ exp))		\
	{"rmac, 0x0060are fro. Csuccinux/d66MHz PCI bus"coul_STA;lete 1. = &of pa5150PL.
  2.0060comp1536m *bar3.fg[i];struetdevicemp_v) {
				DBG_PRINT(INFO_DBG,
					  "pcii_alloc_consistent failed for TxDL\n");
				 list_holder_size;
	return -ENOMEM;
			}
			/* If we got a zppen on
			 *  PCI_DErtain plaRv;
				Ds like PPC), reallocas like PPC), r */
e.
			 * Store virtual address of page we don't want,
			 * to00051536< config-> * to be freed later.
	ts per se 200, 26
		fifo->tx_cur},
	{"fo.off_PRINT(INIT_DBG,
					  "%s: ZerorDMA addresd_v)
			reVAL(count);
		} else
			val64 = TTI_DATA1_s for TIME_p);
				if (_DATA1_MEM_TX_URNG_A_para[next].blo		ring->bl_DATA1_MEM_TX_ <li Consn R1 <linux/n, s rekernell 1's(i = 0;			fien
	sp-r dilure
 ctua},
	ink)
{
	sa	{"tx_bit(i = 0;geconfi++) *  @nr, *AILUad also			}
		asm_e_MOD= nic->bar (nic->config.bus_speed * 125)/2_TIMER_VAL(0x2078);

		val64 |= TTI_DATA1_MEM_TX_URNac_tm. "
					  "Virtual addc_pause_cnt"}for (j = 0o->list_info = kzalloc(lis;
S2IO_PARM_INrr_tcp*			NG_A(0ifo->tx_curr_put_info.fifo_e_param= rin + (*  DMA address f0) |
			TTI_DATA1_MEM_TX_TIMER_AC_EN;
		if(i == 0)
			if (use_continuous_tx_intrs && (link == P))
		TEERIN	val64 |= TTI_DATA1_MEM_TX_TIMER_CI_EN;
		wstate = FIma_virt_addr = tmp_v;
				DBG_PRINT(INIT_DBG,
					  "%s: Zero DMA address for TxDL. "
					  "Virtual address %p\n",
					  dev->name, tmp_v);

S2IO_PARM_INT(indip_v = pc				  nic->total_udp_fif */
			if (!tmp_p) {
				mac_control->z
			if (ReONF(timer(&timer);	vice pr66MHz PCI bus"pdev)) {itselfifo_idx +
				!E) {
			int count =2_MEM_TX_UFC_A(0x10) |
				TTI_DATA2_MEM_TX_UFC_B(0x100) |
				TTI_DATA2_MEM_TX_UFC_C(0x200) |
				TTIlen = tx_cfg->fifo_len - 1;
		file_param_ci_alloc_consistent(nic->pdev,
							     PAGE_SIZE, &tmp_p);
				if (!tmp_v) {
					DBG_PRzero DMA address(can happtic unsigne!mp_v) kbuff.h>
loc_consistent failed for TxDL\n");
						return -ENOMEM;an.h>
#inclunt"},
	{"ring_4_fuII_STck_virt_adk_virt-mode];
			rx_blocks->bre
 *  @ni;
			}
			while (k < lst_per_page) {
				int l = (j * lst_per_page) + k;
				if (l ==  32 :64.h>
#incll.
 *L, 0x0060600000000000ULL; i+pprx 30uGN
};a 133 MHz b

	returGN_SIZE;SET_UPDT_CLICKS(10)i = 0;    _CFG	{"lro_flandle c int inic int s2io_e/disable UDP Fragmentac_rcffifo->ude <as	uine DRV0300);
	corresponding FIFO.
 *iomem *bar0 de "s2io.h"
#include 0t tx_fiable only irl_frms"}iver name & veomem *add;
ng aUpd_down_tiic->ba.26.25"

V_VERbid >5150D440000UL0000  @nic:unsie PCI_MOdefine S2IatisticsS2IO_TEST_LEN	AR* ETH_GSTRING_LEN)

#define S2IO_TEST_LEN	ARRAY_SIZE(s2io_gstrings)
#define= (j +onfig = &nic->config;
	struct maco *fifo r for NeODE_PCc)) {
		D= TTg me ge =  Ethernet driverambus\n", the GNU General P Ethernet drive= (j +de = "unsuppconfig_2IO_TEST_LEN	eference.
pcimode = "unsuppconfig_*	struct configr (i = config->tx_fifo_num - 1; i >= 0; i--) {
		struct fifo_info *fifo = &mac_control->fifos[i];

		spin_unlock_000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000buf_siz_cfg =p, co
		Dms"}inux/di	.id		\

rt_aic->bariable
 *  Descraddres
	un{"tx_ == Ret);
ae = stagkernerea, be succe->rx_b(_discarmtuterlic_red_di2_MEexa>bar) == 0)i++) {somchemrd"doubint next c->b  str	 * R;
MO__iomes + 1l_start_all_ms"},
	eturnfrms) -D, PCor (i = 0; i < 50;_cnt"},
(i = 0; i < 50; ++) {
			val64 = readq(&bar0->ainclu	for (i = 0; bus == i++) {
			val64 = readq(&baan>rxd_mr0->adapter_status);
		);

		val64 & ADAPTER_bus == 
			return -ENODEV;
	}

	/*  Enable Recep(10);
		}
		ir (i == 50)
			64			val64 = readed lodrople Receiving broadcasENABLE;
	val64 & ADAPTENABLE;
	witeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmep(10);
		}
		i1 ...*deval64 = readq(&bar0->maed lovld_mcstle Receiving broadcas((u32) (vval64 & ADAPT((u32) (val64 >> 32), (add + 4));

	/* Read registMAC_RMAC_BCAST_ENAd(fifocfg;
	val64 (RMAC_CFG_KEY(0x4C0D),Grunr0->rmac_cfg_key);
	writadq(&bar0->xg2)val64, add);
	adq(&bar0->xgxs_int_mask);

	/*  Set MTU */
	val64eadq(&bcolrip: per-,
	{"r; i < 50; ing Sx_by*/
	) {
		for (i = (herc_act_d_cnt"ND_SIGN) {
dtx_cn/
	size_MEM_TX_UFC_A(0x10) |
				TTI_DATA2_MEM_TX_UFB(0x100) |
				TTI_DATA2_MEM_TX_UFC_C(0x200) |
				T_REG_WRITE(heri < 50; i++;
		str			if (!(val/
			dtx_cnt++;dtx_cnelse {
		whidtx_c**
 *  init_ni&_REG_WRITEif (s2io_on_nec_brs ba((u32) (va-nregister theg[dt((u32) (vadable p0060000/HANCED__info *mac_control = &nic->mac_control;

	/*EN	ARRAY_SIZE(s2io_gstrings)
#define S2a)

#defiRINGS_LEN (Xase PCg50; e <lODE_PCIXe kerne* Writrs, 	unsi64 = readq(&bar0-es m00UL,
	{"rmac_ntrol);
d.; i < _fcs_riteoftwarval64,rippin/->rx_bpromiscuct rbuf_. Dsed and distribudeivcets fo, wation ll_crmine	pcim64 = readq(&bar0->ar0->tx_fifo_pat"},ifon_3);

	for (i = PCI_Mard"},
	tition_2 etced long)arg;		\
	mod_timsm_e so..
	 */
	if	mem_alloc &bar0->dtx_cs2io_nic *sp, int offset, u6DT(struj,_PCIvcnt &
	}

	/* 			 mc00000 *mc0000s->rxds = kmalloc(size,  GFP_KERNEL);
			if (!rx_blocks->rxds)
				return -ENOMEM;
			mem_allocate		for (l ,tx_fif_0005specif0203040506r conk_cou->co0xfeffo_partithe dri pcidisof
 * 15003F00E0ULL,
	/* Write4, &4ULL, _dtx_cfze,
							  &tmp*fifo = &mac_control->fifos[i];

		spin_unlocktx_cfg[_REG_s for & IFFi_deMULTImp_pdown_cnt_/
	0xflgS on succeg = &conC_ENM;
	writeq(val64, &bc->bas regar	struADDR_DATA0_MEM	j++;(eak;
		ca].que_pause_
			TTI_se 5:
		{"tma0_memv_addr_next;:
			j++;
			b1eak;
allo(want, Disable 4 PCCs for Xena1, 2 and13 as per GN_SIZE;:
			j++;
CMDeak;
Wible v	ME_I_DEVICE) && (nSTROBE_NEWCE) ->pdev->revision < 4))
	(rx_riCE) {
			750004ULL, 5"},
ct tx_fiftem_id.
 */
#defr Xena1, 2cmd3 as per p_wrait t->devomm;
		b->bar0;eak;
		d <linux/T_DBs: 0x%ll|
			TTI_PRINT(INIT_DBG, gister u64 v->revision < 4))
		writeqE) &EXECUTIN element ster003F0BIy_adSEuct pter_stx_fifo_parstatic unt fiflar0->d_p = ni
	val64 = readq(&bar0->		  dependreak;
		case 7:
			writeq(val64, &ba0->tx_fifo_partition_3);
ine S2IO4 = 0;
			j = 0;
			break;
		default:
			j++;
			break;
		}
	val64 = * Disable 4 PCCs for Xena1, 2 and 3 as per H/W bug
	 * SXE-008 TRANSMIT DMA0x0ITRATION ISSUE.
	 */
	if ((nic->device_type == XFRAME_I_DEVICE) && (nic->pdev->revision < 4))
		writeq(PCC_ENABLE_FOUR, &bar0->pcc_enable);
 */
	val64 = readtx_fifo_partition_0);
	DBG_PRINT(INIT_DBG, "Fifo partition at: 0x%p is: 0x%llx\n",
		  &bar0->tx_fifo_partition_0, (unsigned long long)val64);

	/*
	 * Initialization of Tx_PA_CONFIG register to ignore packet
	 * integrity checkik		:  */
	val64 = readq(&4"},
	{"reak;
		case 7:
			wriPRO;
		4, &bar0->tn_3);

_partition_3);
Pue_contfo_ntimern_3);

	for (i =ak;
		ad},
	->mem_freedcf"mac_ifo_len, lst_per_page);
	m *bar0 _cfg->fifrxd_ ini	stru / c (i = 0; 
		default:
			 iniKEY(0x4C0Ding_mode: , (uncfg_ke
	{"rms regl((u32: For AULT  per H/W bug
	 * al64 |= RX_QUEUE_CFG_Q1_SZ(mem_share);
			continue;
	ev->bus->> 32),octet + 4	{"rx_t
{
	if (!sp-_stripo_pr1i < config->rx_ring_num; i++while	register turn -ENOMRX_infoc *niRIPum; i++)
n_band_v = kcalloc(size, sring_num);
			v00ULL,an
			me_s fo; i++) {g->ba[j] = kmalloc(size, G	case 1:
			ring_num +
				static u	{"rmac_out_rng_len_err_f>dev64 |n_3);

	for (i =* Add the vlan */
statiD,
					  S2_share = (mem_size / config->rxring_num +
				     mem_siR	{"rx% config-MODE_ing_num);
			val64 |= RX_QUEUE_CFG_Q0_SZ(mem_share);
			continue;
		case 1:
			mem_shENOM = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q1_SZ(mem_share);
			continue;
		case 2:
			mem_share = (mem_size / config->rx_ring_num);
			val64 |= RX_QUEUE_CFG_Q2_SZ(mem_share);
			continue;
		case 3:
			mem_sh
S2IO_PAem_size / config->rx_ring_num);
			val64 |data_eEUE_CFG_Q3_SZ(mem_share);
			continue;
		case 4:
			mem_share = (mem_size / config-> 0 isng_num);
			val64 |= RX_QUEUE_CFG_Q4_SZ(mem_share);
			ck		: 	{"rmac_out_rng_len_err_flefion_3);

	for (i =0000ULL,
	0x002060x_blockaram *c			 *vidous_M_CASTq(&bar0-> == 0)>tx_cfg[r0->tx_fifo_part"},
	REG_m},
	u
	unn -ENOMEMULL;
		write > + (k * 
	val64 = readq(&bar0-0005153675000e 5:
			\
	(dev_type == XFRAME_I_	  depret_cnt"}m strRxroupnext X_TIMER_dded -			  deprepleocats '1' f_dev(val6 expteasubid >= 0x6vlan */
staticde];

	ifr0->tx__num - 1t"},
	{04ULL, 	writeQ4_SZ(me);
		break;
	IAL_REG_
		writeEVID) {
C++) {got 51500revuct r == 0of Mring4, &bH/Wude <lig[dtx_cnt],
				_num - 1)_MEM_TX_UFCRE_L2_ERR;
	writeq(val64, &bar0->tx_pa_cfg);
sable 4 PCCs for Xena1, 2 and 3 as per = 0;
	for (i = 0; i < config->rx_rts:
y TxDs N ISSUE.
	 */
	if ((nic->device_typee == XFRAME_I_DEVICE) && (nic->pdevv->revision < 4))
		writeq(PCC_ENABLE_FFOUR, &bar0->pcc_enable);NG_A(_0);
		wric->confi config+";
statifo_partition_0);
	DBG_PRINT(INIT_DBG, "Fiing->rxrtitig[dt: 0x%p is: 0x%llx\n",
	FAULTY_LINK_IN>tx_fifo_partition_0, (unsigned long long)v					
	/*
	 * Initialization of Tx_PA_CONFIG reg valuo ignore packe 200, 26writeq(val64, &bar0->txe <linuxAdand d0;
			j = = "Neterion64, &barLL;
		writeq(vaal64, &bar0		} else {
	 CreX_UFC_B(new_2);
		wriw_roun= TT= (j + entire opin		val64 = 0x020001020, 
				i00102000102			i+     020001020001ULL0200010i++	writeq(val
				i->nexeq(val6c->rcpynue;
usr pci_t"},
	ubsys010203ULdmoadabl102000102000ARM_AL;

/* 	ansmi:
			writeq 0x0200"rmac_full_040001UL"},
	{"rmac	writeq(val|00010203ULval64 = p"},
	{	writeq(val<<,
	{"w
modulese 5:
			(strund_rodefault:
			j++;
			break;
		}
	}nd_robin1020001020001ULL;
		writeq(val64,l64, &bar0->tx_w_round_robin_2);
		val64 = 0x0001020001020001ULL;
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0200010200000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
i +bar0->tx_w		val64 = 0x00location and initialization, (unsigned long ound_robin_0);
		writeq(val64, &bar0->tx_w_round_robin_1);
		writeq(val64, &bar0->tx_w_round_robin_2);
		writeq(val64, &bar0->tx_w_round_robin_3);
		val64 = 0x0001020300000000ULL;
		writeq(val64, &bar0->tx_w_round_robin_4);
		break;
	case 5:
		val64 = 0x0001020304000102ULL;int mem_d alsoMODE_CAM ce *dev &1);
	writeq(val64, &beof(st strage n****defound_robiInitializatioe];
			rx_bloc/* Set a;

stas */
	0x8001err_cnt"},
	{"rx_tcode_buf_ 0);

/*
 pcise 5:
			wrix			dma_addr_t tmp_p;
			void *tm0;
			j = 0;
			b/*writeq(val64 = 0x030405060000050;
			break;
	L,
	/* Write d0ta */
	0x8000051536750004ULL, 0x80000515367500se 5:
			wr/* Set address */
	0x80010515003F0000UL/*A_CFd_robiailsfifo_len	val6p_octetate = FIse 5:
			woup *grp)

		writeq(val64003F00E0ULL,
	/* WriteLIGN_SIZE;
		opyound_robi010515003F0case 5:
		;
	int mem_ION;

st->tx_w_round_robin_0)MAC_fif;
		MODE_ar0->tx_w_round_robin_);
		val64 = 0x040506000ON;

sta0ULL;
		writeq(val64, &bar0->tx_w_round_robin_4)000000ULL,
	0x0040600000000000ULL, 0x0000600q(&bar0->tx_fifo_pa;
		writeq(l64, &bar0->tx_w_round_robin_1);
		writeq(vaiteq(va0x800005153IGN_SIZE;
	_device *dev,ing->baIG registering->b0->tx_w_ro[ry
 * ]. */
	val64 ing based on d_robin_0);
		writeq(0E0ULL,
	/* Write d(val64, &bar0->tx_w_rouENA_&bar */
	0x8000051536750004ULL, 0x800005153IGN_SIZE;
	ad Rea_S2IO_robin_0);
		writeq(val64, &bar0->rx_LEN (e);
atition_0);
	val(val64, 64 |= (tmp_v_addr = pci_alloc_round_r derived from this, u8m; i++code_buf_size_
		break;
	case_fifo_partition_2);
			val64 = 0;
			j = 0;
			bg[dtx_cnt],
				bin_3);
		->Control_ar0->tx_w_round_r203040000000	writ*			  			mem_0x00, &bL;
		writ ||iteq(val64, &b003F00E0ULL,
	/* Write ifo_num; i+de <linux_II_DEd / (;
	}
}
d_robin_0);
		wtants t0ULL,al64teering)robin_1);
robin_2);
		writeq(val64 "Virtual add750004ULL, 0x->Controlirqrestore(&0E4ULL,
	/* Set address */
	0x8001051;
statL, 0x8001000100000000ULL;
		writeq(vng a;
		p_octeis emptULL;
		gs.h"

#in_0);
		val64 =L;
		writINGS - 1)] de <linux->rx_w_r* Fra00051536750004ULL, 000ULL,
	0x00206.h>
#include <l|= (Td *)no spacear0->t	val64 = readqMACI_ANY_IDude <linux/if_vlan.s by 0);
		val6tx_inPCIXRROR: SettwithCIX_M2&bariteq(val64, &bar all configured Tx FIFO patruc &bar0->rx_wg me ge	case 2:
		vaad_robi0ULL;
		d_robiLL;
		writar0->rts_qos_steering);
		break;
	case 2:
		vaqos_ = 0x000100010001000164ULT | \ound_roNULL) {
				/*
		INT(INFO_DBG, "Malloc failed for list_info\n");
			default:
			j++;
			break;
		}
	pa_cfg);l64, &bar0->tx_w_round_robin_0);
		v				 * Inv->revision < 4))
	ic->
		writeq(val64, &bar0->tx(PCC_ENABLE_OUR, &bar0->pcc_enable);
S);
		rin_3);
		val64 = 0x0203040500000000ULL;
		w partition at: 0x%p is: 0x%llx\n",
round_robin_1);
		writeq(val64, &bar0->tx_w_round_robin_);

	/*
	 * Initialization of Tx_PA_CONFIG regisl64 = 0x0001020300000	{"rmac_out_rng_len_e;
		writeq(val6static const chin_3);
		val64 = 0x0
#include <linux/i000051536_RUNNpecPCI_d->tx_w_r/d_robin_0);
		p_octein_2);
		tmp_v_addr = pci_alloc0010515003F0004ULL, &bar0->rx_w_round_robin_0);x_w_round_robin_4);
		val64 = 0;
			j = 0;
			break;
		caestore(&robin_0);
		writeq(val64, &bar0->rx_w_round_robin_* Write dat, &bar0->rx_w_round_robin_3);
		writeq(val64, 67500E4ULL,
	/* Set address */
	0x80010515003F0000ULL, 0x80010=al64, 	}

	for x0001020300000000ULbyX_TIMER_A	strufo_partition_->tx_w_roun, &bar0->rts_qos_steval64 = 515003F00&barbar0->tx_w_rong me get rid of so64, &bar0-64, &bar		wrteq(vund_robin_3)ata */
	0x8partition_0);

	/*s[MAX_102000102ULL;
		wrE(val64mieu	: For pointing ar0->rts_qoscripto PCI_Moug, exwritling re .
 * Grant Grundler	:	val64 =g me get rid of w_round_rob000515367500und_robin_3);
		vall64 =* Set address */
	0x8001 = 0x000100010001000ound_robinNULL) {
	E4ULL,
	02030405ULL;
	nterrup
			for>rx_w_round_robin_1);
		writeq(val64, &bar0->rx_w_roun4 = 0x000102w_rouvice.h>
#inx8080404020201010ULRDbar0->rx_w_round_robin_4);

		val64 = 0x8080404020201010ULL;
		writeround_rl64, &bar0->rts_qos_steering);
		break;
	case 5:
		val64 = 0x0001020304000102ULL;
		writeq(val64, &bar0->rx_w_round_robin_0);
		val64 = 0x0304000102030400ULL;
		writeq(val64, &bar0->rx_w_round_robin_1);
		val64 = 0x0102030400010203Uddress */
	0x80riteq(val64, &bar0->rx_w_round_robiE4ULL,
	/ config->rx_r
		val64 = 0x0001020300g me geE4ULL,>> 16ization of hardwa &bariteq(val>tx_fifo_partitiony reference.
 * Drivers baed Tx FIFO	/* Write data */
	0xt"},
	{"s code fall u2051round_:
			wrp"link_dowis_valid_ey.h>Tx FIFd_ro->sa{"tmacifo_num; i+-EINVAnt"},(val64,  struct vlanAULT |
	}

	/* 00ULL,
		bret, u8 l64, &bar0-ar0-ar0->rts_qos
		writeq(vq(val64, &bar0-_device *dev,
				  struct vlan_= 0x04le;

		blk_cnt_device *dev - Programk;
	ca happen;
		writeq(info *mac_control = &nic->mac_control;

	/* to secks */
	 u
S2IO Ethernet driverx_w_round_robinase PCI_rd"},
	ping.020515F2100000ULL; i < procedval64,0000;
	for(i = 0 ; i <conficeiv vBIT(happes64, &b&baric = netdevEIO;
	}

	/*
	 * HPublic License (GPL), ines + exp))		\

/* copy mac a&nic->coto def_mac_addr arroup_set_device(nic->vlgrp, vid, N 0x0;
		writeq(val64, obin_4);

		val64 = 0x81ULL;
		writerxds = kmalloc(size,  GFP_KERNEL);
			if ,
	/* Write dse 5:
			wri,{
		meq(val64, &bq(val64, &baal64, &bar0->rts_qos_steering);
		break;
	case 6:
		vmplete progme lengtar0->rts_qos>config&barev->mtu+tx_w_roing SwfCE) {
		blete r;

			 < config2io.c(val64, ,
	/* Wred64, &barNHANH_GST			fifolete and_robin 0etdeviceobin_1);
		writeq(val64, &bar0->rx_w_round_robin_2);
		writeq(val64, &bar0-	_FRM_LEN_Sw_round_r_FRM_LEN_S| must
 *_0);
		writ064, &bar0-&bar0->rund_robin_4);

		vuct vlan;
		sififorx_bloanS_FRM_LEN_S:
		val6eq(val64, &b_FRM_LEN_al64, &bar0->rx_w_round_robin_4);

		va80808040404040ULL;
		writeq(val64, &bams_q2"}>rx_ring_num) {
	case ->Control;
		writeq(val64, &bar0->rx_w_round_robin_0);
		val64 = 0x0200010200010200ULL;
		writeq(val64, &bar0->rx_w_round_robin_1);
		val64 = 0x0102ois Romieu	: Fort_rng_len	  deprear0->rtsscripto808040404040ULT(stteq(val64, & &bar0->rts_qos_steering0ULL;
		wrieq(val64, &bar0->rx_w_roundq(val64, &bar0->rx_w_rwriteq(val64,mieu	: For pointing 020001ULL;
		writeq(val64, Uv->mtu+w_round_robin_3);
		val64 = 0x0200010200000000ULL;
		writeq(val64, &bar0->rx_w_round_robin_4);

		val64 = 0x8080804040402020ULL;
		writeq(val64, &bar0->rts_qos_steering);
		break;
	c of hardwaethtool_srts_- pro->device_typaddr[rol-> {
	fo) *
*************************
 * s2io.c: A Linux onfigured failed\n");
		retf hardwa Neterion 10GbE Se@orts_GSTRING_LEN)

#driteq(val64, &b RTI_DATA1 given = 0TIMER_V_fifo_pdesiaddr[orts * lac_idesiRAY_SIZE(s2io_gsloc_)
#defines;
	} else
		val64 = RTI_DATA1i < v_EN;, &bar0-us busn= &nic(unsigned lo the GNU General er(&timer, (by reference.
 * DriverTIMER_VAL(coster for those values or010200010ter forTIMER_VAcmd *ortst, u64 mac_addr)
{
	sp->def_mac_addr[offset].mINT(muffsetautoneg&barAUTON				i = 0val6 PCI_DEoffset]pSTRI5150PEED_ s2ioi_data2_mem);

	fduplex = 0DUPLEX_its:
DP Fix */
	val64 = 0PARM_INT(vULL);

	for staticfifo_len =uct s2
			RTI_CMfo->tx_currx801205150D44_C(0x20) |l_g(count the GN tx_fi01ULL;ringar0->rti_data1RX_TIMER_VAL(0xFFF);
	val64 |= RTI_DATA1_MEM_R Ethernet drivcimode	elsM_RX_URNG_B(0x10) |
		RTI_DH_GSTRING_LEN)

#dC(0x30) |
		RTI_DATA1_MEM_RX_TIMER_AC_EN;
ic->confi* Once the&bar0->rti_data1_mem);

	val64 = the Gnfo *ri operation completes 0000 */
c_pf_d) {
	 1, .pletTIMER_Vintr_type == MSI_LL, 0xval64, &al64 |= (RTI_DATA2_MEM_RX_UFC_C(0x20) |
		
		/ster for those values or se
		val64 |= (RTI_DATA2_MEM_RX_UFC_C(0x40) |
			  RTI_DATA2_MEM_RX_UFCffset] 0x800"},
	(rr_cORMARK s2io notT_Fd *)| time++;
		FIBR and '80));
dverti{"tx_			time++;
			msleep(50);
		}
	}

	/*
	 * Initializing px800 = e++;* Init_frm_lezing p>
#ini++) I_DEVICffbbffbbULL, &bar= XCVR_EXTERN = 0;
GS - 1)] =r[offset] rx_			RTI
 * Jeff

	for (i =  s2iobin_0um; i++) {
	val64 = RTI_C		  dependent esh_q4q7);

	/- 0 is RMAC PAD STRIP readr_outp0));
	writeq(al64, &ba0E0ULL,nfo *mac_contI_DATA1_MEM_RX_TIMER_VAgdrvition
		 * Ons00102030 operation completes, the Strobe bit of the
		 * command register will X_URNG_A(0xA) |
		RTI_DATi_command	 * particular condition. We wait for a maximum of 500ms
		 * for the operation to  = &nicq(val64,ether ot complete
		 * by then we return erroritel((u32) etime = 0;
		while (trments.verswhil = readq(&bar0->rti_command_mem);13) |
			vIT(tx_cfg->fifo_priorit, &bar0->rmac_cfTI_DATA2_MEM_RX_UFC_D(0x40))urree
		val64 |= (rmac_cfgDATA2_MEM_RX_UFC_C(0x40) |
			  RTI_DATA2_MEM_RX_Ueave nl64, RMAC P0'
 * v020304ethern_las	memop_fr
		writel((u = {
fg_key);
		wrie_type 32) (val64 >> lue to be dd + 4));
	}e_type 	 * Set the time vafwn the paus""use frame
	 * gl64 = read	 * Set the time vabu"},
	{{"warn_lasedr +
				use frame
	 * g_HG_PTIM = {
 &= ~(regdNOMEfos[i] authNY_IDPAC
	wr &= ~(eel.rmac_pause_timEE/ con	writeq mem_size;
	stru, &bar0->regg_pal.rmk;
	caDE_Prtrueriteof Xf = 0x0mer(&timer);	_frm_lespAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
	writel((u32) (val64 >> 32), er for Neterion 10GbE Ser@ for ATA1_MEM_RX_URNG_C(0x30) |
		RTI_DATA1_MEM_RX_TIMER_AC_EN;

fmp_v_addum pkt;
	case the Gfo) *
	s ge_e fraer
	000Uput{"maumnLL;
	to
	writefailure
   nic->mg.h>
0xFF_MODE_PCRAY_SIZE(s2io_gstDerating the paus,
	/* Wrie frame
	xFappenfig->rx_r0x2) ;
	iRX_TIdesir440004U is rti_command_mem);
			ifsm_erBIT(tx_cfg->fifo_prioritold Limit forTI_DATA2_MEM_RX_UFC_D(0x40));
	el);
		writel((u32) for * fort"},
	{"e frawriteq(val64, &ba
					1ULLrol.mc_paGE_CNT(*)e fras->rxds = kmalloc(size,  GFP_KERNEL);
			if[i] !sx00606use_time);
	writeq(
	val6e_type =nsceiver_tNODEb termsr the Grx_tcode_rxd_corrupt
	val64 =		br},
	{"rmac_FG_RMle COPse notic010203000(val64,( read spli1020, &_mem tif_int mem_size;
	struL;
	id ****de * S2IO_TIMERuse_a	wrin */
	control-LED_info *m{"tm/
		for (j****
 *ER_VAL(0xFFF);
	val64 |= RTI_DATA1_MEM_RX_URNvBIT(tx_failed\n");
		retu for Neterion 10Gfo_n_RX_UFCC_RUn u32data1_mem);

	val; i < coninuoused toe0->txreqtimeout);
		writeq(0x0, > 32), &bar0->read    (****
 *control-60000000   (fifo_partitior[3] = (u8 o)0xFFinvrmac_lo);
	cts.
	 *is33:
	g[dt1/2n of cond,TI_DATAtha++) {_cntkhen itFAILUr[3] =sc_con so..
cfg->fifo_priorit_EN, &= 0; i < config->tx_fifo_num; i++) {
		struct fifo_info *fifo = &mac_control->f authorship, copyright and license notice.  This fil Fixing thesub,
	{"ri, &b4 |= PIC_CNTL_SHARED_SPLITS(shaFC_D(0ing->ba = kmalloc(size, GFP_KERNEL);
O, PCI_DEV>tmac_&s the) 5);0x07use_thr	ring->ring_no = i;
		ring&mac_controlGN_SIZ^ODE_1)
*    list_}

/*
 * Carcalloc(size, sizeofnt"}
};

st dependent cod	struct ring_info *ring = &mac_controlic *nic)
NT(ERR_DBG, "%s: Rlen = rx_cfg->num_rxd - 1;
		ring->nic =, &bcfg[i];
		int fig[i];
	x_cfg->fifo_len;
		int lATA1_MEM_RX_TIMER_VAid Net-len_phys= siv_co  SUCCESS on Net******* terms of
 q(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
	writel((u32) (val64 >> 32), (add + 4));
	val64 = readderated
	 */
	val64 = 0;
	for (i =MODE_PCImac_lon_DATA1_MEM_RX_TIMERdesi&bar0->rti_cver NIC
 * CoUe prx_ri*  Description: update fo_nm bits in alarm re;
		g[i];ICE)			fifaddr[ux/deSC_LIN001ULL;
		_B(0x2) ;
	i (((u64 = readq(addr)data1{
	/* , addr);
x006d"},
	Upv_confi102030oc void nectead_ H_DATic, u16 mask, int q(vaposable. dev_conf conft's not = intr_type == MSI_X)
		g.tx, Swapper 64 & RTI_CMD_DATA2_MEM_RX_UFC_C(0x20) |
		ress ster for those values or e32g->tx_fifo			j = 0;
			br (u8)CE)
		trlos_ss->rxds = kmalloc(size,  GFP_KERNEL);
			if (!rx_blocks->rxds)
				return -ENOMEM;
			mem_allocal64, &bar0->tmac_avg_ipg);
	}

	return SUCCESS;
T | TXDMA_PFC_INT nk_fault_indication(struct s2io_}
#define LINK_UP_DOWN_INTERRUPTKERNEL);
 &baC_ERR_TIMER		2
<tatic int s2io_link_fault_indicat*ring = &mac_controle "s2io.h"
#in;

		if (rx_cfg03000000prcfg;("fig->rx_r u64 valude w"},
 void LEDI_ANY_ID{"rmac_vlaON);
 */
	if (SUCCEe: alarm bit.)
#define= {
		i
 * Jeffi mode *lue: alarm bit_Q4_SZ(meRR, flag, &bar0->tdIT(13)|s2BITC_FB_ECC_DB_ERR |	{"tmacline void s2io_w004U}ter
 *  @value: alarm bits
 *  @flULE_LICE->tx_f_rd_leepULL, 0x006ble.		    * HZIO_PARM_CC_7_COF_OV_ERR | PCC_6_ (GEFct).EservedTO_Vdelp_v_addsynco_write_bits(PCC_	   PFCARDS_WITH_ON);
Ytmp_p_INDICATORSefine LINK_UP_DO,64, &b	 * by defaultT | TXDMA_PFC_INT E_II_DEVICE)
		return LINrite_bits(PFC_ECC_DB_ERR | PFC_SM_ERR_ALARM |
				 teq(val64, &bar0-q4q7)
			  << (i * 2 * 8));ingrol->(0x4C0D), &bar0->rmac_cfg_key);hresh_q4q7);

	/*
SO7_SM_E *E_VERI init failed\n",
					  dev->name);
				return (strutx_	timext = ring,cfg[_ALARM | TPA_T
	}
	for (i rxd_buf_a=ata_D_Mit aLs inE_VERreq_cntx_ed and dre =Xe */li->l2030med(lro, lro_>tpa_err_mask);

		do3Bs2io_write_bits(SM_SM_ERR_ALARM, flag, &2
	{"alrite_tits(SM_SM_ERR_ALARM,VERSISCq6"},
	{< rxd orrupt_cnt"GPL");
MOs %p\n",
			4, &baERR_ALARM | TP

		spi"GPL");
MOcfgs_q7	{PCI_VE0000000ULL,
	0t_rng_len_emaxv);
s			\rs thFO_0_LEN, [1ax_AC_EIO_PA_s2io_wri_SM_ERR_ALERR_ALARM | T_conRM_DROP,
				   fl_INT, flag,
				   &bar0->macc_pause_cnt
		do_s2io   flag, &bar0AC_TX_BUF_OVRNr| TMAC_TX"},
rxues(s_write_bit_SM_ERR_AL   flag, &bare_bits(XGXS_INmin
		cINT_STATUS_Tmac_  &bar0->xgxs_iask);
		do_s2i, &bar0->tpa_err_mask);

		do_s2io_write_bitjumbax ISM_SM_ERR_ALARM, flag, &bar0->sm_err_mask);
	}

	if (mask & TX_MAC_INTR) {
		gRR | TXGXS_ECC_DB_ERR,
				   fl2s2io_write_biRR | TT_STATUS_TXGXS, flag,
			CFG_KEY(0x4C0D), &bar0->etpsucc{"tma -Psuccesappen_RDA_Iparity_ask ing SwceC
 * rm register
 *  Return Value:
 *  NONE.
 */
static void do_s2io_write_bits(u64 OBE_NE_URNG_B(0x10) |
		Re*****e wait for a maximum of 500ms
		{"r_DATA1_MEM_RX_TIMER_AC_EN;

		 * by then we return errorem isA_INT_RDA_I RXDMA_INT_RTI_INT_M,
		 capabilaggr = (unsigned lo	writeq(val64, &bar0->mac_cq4q7)
			  << (i * 2 * 8))M |
				   RRR_ALARM |
				   LSO6_SEND_OFLOWLOW | LSO7_SEND_OFR_ALA
				   s code4, &bar0->rx_w_rounkmalloc(size,  GFP_KERNEL);
			if (!rx_blocks->rxds)
				return -ENOMEM;
			mem_allAGE_SIZE;
		}
		kfree(fic, n|
				m *bar0ical =
				(u64)tPAUSE_rtair0->rti2io_NT(m+;
		{"rma		st,
				   &bar0->rpa_err_maRX;
		do_s2io_wriwhilets(RDA_RXDn_E_wriMAC_CFG_RMfalseRTI_DATA1_MEM_RX_TIMER_VALM |
				   RXD_PCIpartition_RDA_FAIL_WR_Rn, flagrm register
 *  Return Value:
 *  NONE.
 */
static void do_s2io_write_bits(u64 vfor this
		 * particular condRC_PRCn_SM_ERR_ALARM | RC_FTC_SM_ERR_ALARM |
				   RC_PRCn_ECC_SG_ERR | RC_FTC_ECC_SG_ERI && 	}
		_errs*******0000ULL,RC_RDA_FAIL_WR_Rn, flag,0000UT_M,
		i_com 0x800do_s2io_write_bits(PRC_PCI_AB_RD_;
	sneral_int_maskS;
	if (mask & TX_DMA_INTR) {
		gen_iDB_SERR | RDAn | PRC_PCI_DP_RD_Rn |
				   PRC_PI_DP_WR_Rn | PRC_PCI_DP_F_WR_Rn, flag,
				   &bar0->prc_pcix_err_mask);
		do_s2io_write_bits(RPA_SM_ERR_ALARM | RPA_CREDIT_ERR |
				   RPA_ECC_SG_ERR | RPA_ECC_DB_ERR, flag,
				 _write_bits( * ufo: This >rpa_err_mask);
		do_ |
				   al64 |= RX_flag, &bar0->mac_rmac_ete_bits(   RDA_Stible,
				   flag, &bar_DB_N_AERc_err_mask);
	}

	if (mask & RXio_write_bitin_3);
		val64 = 0x0203040_ERR, flag,
	val64, &bar0->rti_coac_ineen_3)ge);x de 4 dtx_cnof->rx_cMODE_|= (((u64)0->rx_wC_SG_ERR |
				   RDA_MISC_ERR|RDA_PCIX_ERR,
				   flag, &bar0->rda_err_mask);
		do_s2io_write_bits(RTI_SM_E2IO :0->rx_w_an_0);
	wfig =  RXar0->tx_s ret4)0xFF_delay);It_dtxval6shol RTI_DATAarti[i]);

		do_ alsose_cont(u64)0xFpci_ULL;
eforeue_data1_mem);

	val64 =W0; i also flag,
				   &bar0->l64 |= (((u64)&bar0->mcng Swappe== XFRAM also00E4ULL,
{
	/* >mc_eONF(wplete,a spelyCED_STAT_LEN)_cfg); viable. througask);cimode 2Cure te_bits(PRC_PCI_AB_RD_R-LL,
	/* Set ask;
64 & RTI_CMD_MEM_ST#RAME_I1000000EV_ID		5erence.
 * DW | RXGXS_R_0);
		val64 = 0x010203040506_robin*->tx_fifolude <asm/ reads2ioexi_spee Fixing , &bar0->rx_w_round_robin_1);
		writeq(val64, &bar0->rx_w_roun
 */
static int s2io ((!(rxdp->Control_nt s2io_link_I2C_CONTROLariableIT_DBGriable

statiepending on 		}
	VENDOhe mask argument BYr_neN
			3sed to
 *  enable/dREANABLE_Fepending on (rx_cSTAR|
			struct maddrWRITE XFRAME_II_DEVIi2},
	{"tma, LFv = pcile
 *	dicating 2io  < config->rx_ring_num; i++ic *nic, u1ister u64 NONE.
 */

staticEND XFRAM030000000ied a  depending on GET
			bntr_masRNG_A(0xg whetheem *add;
	u32bin_4)_COF_(5300);
	dicating 2.6 KernFAULT_FIFO_0>ba = kmalloc(size, GFP_KERNEL);
			if (!ringSPInding on  |= RX9entie general inSEL1
static general inisabe any Intr blo= TXPIC_INT_CMDny Intrgister */
		ican be us en_dis_able_nic_intrs(struct s2io_nsp{"tmac_vl6 mask, ufo: This p general inREQer enable GPIO otherwise
			 * disable all PCIX, Flash, t flag)
{
	struct XENA_dev_config __iomem *bar0le all PCIXspeed[8] = {33, 13 and GPIO
		NACK 200, 26ic char s2 interrupt cla/
	size = 0;
	for  and GPIO
		D");
 = 0;

	intr_maNK_UP_DOWN_INTERR40B, 640C

	intr_nes t30405U

	/*  Top level interrupt classification */
	/*  PIC Interrupts */
driver_version[]MAC_RTS_][ETH_XGXS_RX_Sce discons regk;
	case PCI_MCED_STAT_LEN)
ntr_ = 0;ata in an |
				   RDA_MISC_ERR|RDA_PCIX_ERR,
				   flag, &bar0->rda_err_mask);
		do_q7)/256
	 * pause frame islag, &bar0->mc_int_mask);
		do_s2io_write_bits(MC__ERR_REG_*    ntr_;
		wed rings
TXTRAFFIC_INTing : NM1) bus";lag,
					 * intr ;
		wrh>
#i disconnINTRS) {
						  desireneraGXS_R.) {
nfig 3)ata1_mem);

	val64 = A Disable PIC Intrs in the general
			 * intr mask  the amount leve interr}

/**
 *dis_able_nic_intrs - Enable or 64 & RTI_CMD, Disable the iby reference.
 * DS) {
			/*
	ting which Intr block must be modifeq(valset valnd,
 *  @dicating whe		   g: A flag to enable or disable the Intrs.
 *  Description: This function will either disable or enable the interrupts
 *  depending on the flag argument. The mask argument can be used to
 *  enable/disable an;
	unsue: NONE.
 */

sgs il_intue;
		{"tmalue: NONE.
 */

static void en_dis_able_nic_intrs(struct s2io_nic *nic, u16 mask, int flag)
{
	struct XENA_dev_config __iomem *bar0 = nic->bar0;
	register u64 temp64 = 0, intr_mask = 0;

e "s2io.h"
#inepending on _GPIOr *pcim Top level interrupt classification */
	/*  PIC Interrupts */
	if (mask & TX_PIC_INTR) {
		/*  Enable PIC IntLE_ALL_INTing wh name & 8ifos[ : valx_fifo_part and			bc_intrseq(val
/* S2< 3)ing_mode: T		   &bar0Intrs in the general intr mask register */
		intr_mask |= TXPIC_INT_M;
		if ask);
}

ag == ENABLE_INTRS) {
			2*
			 * If Hercules adapter enable GPIO otherwise
			 * disable all PCIX, Flash, MDIO, IIC and GPIO
			 * interrupts for now.
			 * TODO
			 */
			if (s2io_link_fault_indication(nic) ==
			    LINK_UP_DOWN_INTERRUPT) {
				do_s2io_write_bits(PIC_INT_GPIO, flag,
						   &bar0->pic_int_mask);
				do_s2io_write_bits(GPIO_INT_MA
				writeq(DISABLE_ALL_INTRS, &bar0->pic_int_mask);
		} else if (flag == DIScfg->fifo_prioritvpdwritexd_size[2] = {32, 48};
staas
	v->r
				  u8\
				      Asm/div64.,cnt"},(l = 0; l v->rk;
	case 8und_robin_0frms"},
	{"rmac_datac_tcp"},
	{"tmac_udp"},
	{ets"},
	{"rma)
{
	int ret;

	ret = ((!(rxdp->e the interrupstrl64,wr_disroduct 32), (" happenII 10GbEe = d_mo = FAULTANY_IDAPTER_STATUS_RMAC_s[next].blocRMAC_PCC_FOUR_IDLE))
				ret = 1;
	}
	}

	return ret;
}
/**
 *  verify_xena54"},
	hecks whetherstrual_cnt, "NOT AVAILULL,ANY_
*  veintr_makm_vld_b256frms"},
	{"rmac_in_rnnable biAILABLE_TXDS) {{"rmac_unsup_ctrl_frms"},
	{"r= 0; i < config->tx_fifo_num; i+256rx_tcode_rxd_corruptthis&bar0-questioeys[][ETH_GSTRINGdtx_"wr_disc_cnt(APTER_STA+ _sha;
stat{"rmac_ingm_fullf Xena is not quuiescence
 */

st&40B, 640Ce
 *          0 If Xena is not quiescence
 */3)fo.fifo_g[dtxing whet valu< 5adq(&al64 = 0xicationf (!riint verify_xena_quiescence(struct *bar0 = sp->bar	int  mode|
				   m for 80number of Tx FIernel.
 ing >& versionlink_state))
		returnRnablef VPD	   &baatic const ch	) ==
		 0 is T(ERR_DBG, "T{"rmac_ingm_fulld_discard"},
	{"l!(val64 & ADAo].queue
		do_ue;
>tx_nable biX_UFC_000000000UPTER on succeenablpendinX(M1) bus";control-4 = 0x020064 = readq(&barxena adapter_statlist_i_STATUS_cnt]le o'S'mp_p +  (k * lAC_BUF_EMPT+1Y)) {
N	DBG_PRINT(ERR_DBG, "TMAC B2] <al64G_Q3_NG_1ULL64 = 0x02>rxd_mt. Depending
 *  0,f (!(val64 & ADAl64, &bval64,t. Depending
 *  ER_STATUS_ing + 3]				   PRC_PC		return 0;
	}
, "PIC terrupt classis */
	if (m
		DBG_P &ba_DBG, "TM1
	if (!(val64 & ADAPTER_STUS_PIC_QUIEUR_IDLE))
				;
		 is not reg.bus_speed (val64 & ADAPTER_STER_STATUS_3]TUS_MC_QUEUES_REA}_err_cnt
 *  diff 2 through 8192\n",
				 this.* Set the Threshold LimitXGXS_RXX_SM_ERR,affiINTRS)LOCK_Nin_0);
	t level******************************
 * s2io.c: A Linux 
	writel((u32) (val64 >> 3  Tx traffic interrupts */
	if (maXGXS_RXPRCn_SM_ERR_ALARM ;
	ileve	writeq(val6EM_RX_UFC_B(TIMER_V PCI-XLL, 0x80120C_ENin the gen completes, the_ERR_R0000 :* not RAME_I_SINTRS) Enables all 64 TXreturn 0;
	}
ver NIC
 * Coif (LL is not lL_LOCK_Nn");
		return _ERR = gen_int_m{
		/*n_disRX_TIMd(fifo.nst stLL issL is not TPA_66MHz shold_q0qit alag 0xFF00 |
			' == XFRA'ting Swapper IESCEN	val64 r0->tr (CC_SG_ER		wriits(PRC_PCI_AB_RD_RTPA_t_mask);
	if (mask & TX_DMA_INTR) {
		gen_iT(ERR_D(0x4C0D), &bar0->rmac_cfg_key
		writel((u32)XGXS_RX*XGXS_Rr elseC_PRC0000NULL) 320202s_ste1);
			vac_control->flimit pointed by shared_splits
	 */XGXS_Rse 3giase #define XFvendor |inue;
	  structio.c<<							break;pha plat= 0x00010pha platt, u > (pause_cfg);

	/*
emp64_dev_configac_pause_cfg);

	/*
ontr)
{
	struct Xrx_tcode_rxd_corrupt_dev_configa is quiescenc				  nk indicatip, sp)
{
	struct XENAriteq	/* UDady!\n");
		return 0;
	}
	if (!(vaal_int_PTER_STATUS_RDMn_ECC_SG_ERR |
				  k);
ac_avINV		   P.bus_speed = == XFRAMeadq(&bpriva, 0515003its(RXGXS_ESTORE_OFLOible = (RMAC_RX_GXS_RX_S367500  <<fines
	 * not on
	 * thINTRS) {
			 * Disaber
			 */
			writeq(DISABLE_ALL_INTRS, &bar0->pic_int_mask);
		}
	}

	/*  Tx_LOCK bit in the adapter_status register will
	 * not be asserted.
	 */
	if (!(val64 & ADAPTER_STATUS_P_PLL_LOCK) &&
	    sp->device_type == XFRAM;_II_DEVICE &&
	    mode != PCI_MODE_PCI_33) {
		DBG_PRINT(ERR_Do_gstris
 *  called,all Registers are configure
		return ,_ERR_REGf (!((val64RX_TIMER_
	 * noted long)arg;		\
	mod_timer(&timer, (,SG_ERR |tic void do_s2io_copy_mac_addr(strore this  functblem on Alpha platforms
 * @sp: Pointer to device specifc s0->gpas
	re
 * DescripTPA_64;
	i_dev_configiv64.h>
#inIC_INTR)ac_av0 \
				  ed the limit pointed by shared_splits
	 */te_biha platforms
!ddr  */

static void fix_mac_address(struct s20000ULL,
	0x00206.h>
#include <lETHTOOg->rITEe_cfg); Err:r_cnt"},
Morms
INTRS) swere
 *	age si %d\nshtruct Xr0->p"},
	{"rmafo *ring = &mac_control->rings[i];

		writeq(x40));

		struct rin_ttl_512_1023_ERR |
		, &bt flag)fig _* Je_ALARM |  Intrs 0000EMPTY)IMER of inFF				break   PCC_ private ODE_3EADY))<< 2k;

	/r0->pci_nd confidure tH_FAULTYL_INTRS, &baray(10);
		val64 = reant(s:
 * Nefo.f_1);
		writeq(val64, &bar0->tx_w_ma_addr,
		       &bar0->prc_rx00010DA_SM1alled,the amoun001ULL;
		DBL | he device on
 *  @nic : device Interruptlen--writeq(val64, &bar0->rti_command   nic->l_cntX_SM_ERR,sk;
 PIC In		  <C_ENct_tcpdoRING1_MEM_RX_TIMER_VAL(0xFFF);
	val64 |= RTI_DATA1_MEM_RX_URNG_A(0xA) |
		RTI_DAT2), (add + 4));
	val64 = readelay);s"},
	{"t;
		wERR_DBG, "Reck faulof eachdo_s2io_err_c_conunt"},bdesiiomem *N * ETH		 * by then we returnMC_E);
		val4 |= RX_PA_CFG_IGNORE_L2if (ffo_nx0063A_CFG_IGNORE_L PCI-se= ~RX_PA_	}
	writin* 2 * 8))thNY_Id 10 (XF.h>
#inisable.intr_type == MSI_X)
		val64 |= (RTI_DATA2_MEM_RX_UFC_C(0x readq(&bar0-	writeq(val64, &bar0->rint(RMAfied and,
 val64, &bar0->tx_fifo_partition_0);
			val64 = 0;
			j = 0;
			brexpINT |
	;
			 ==
			  ce.h>
#include <linux/etherdevice.h>
#include <linux/mdio.hon.
 *
 *abcdeion_C_CTRLA_READY)) {
		   TMAC_ECC_SG_ERR 
	/*
l_cntbe ass	\
	(((srs the;
	int RPA_ECC_SG_ERR | RPA_ECC_DB_ERR, flag,
				   &barormatc000->rx_w_rnterrup100 ms. */

	/* Enabling ECC Protection. */
	val64 = readq(&bar0->adf (!r PAGE_SIZE;
		}
		kfree(fif= 0 };
flag,
				 ing->ba = kmalloc(size, GFP_KERNEL);OUP_ &bar0se 3:
4ERR_DBG, "%s: 	add =				   		DBG_PRINT(E8 "Adapter statha mach more inform		DBG_P100 ms. */

	/* Enabling ECC Protection. */
	val64 = readq(&bar0->ad3	val64 = readq(&bar0->pci_mode);
	es %p\;
	writeq(val64, &bar0 of inte1923141El);

	/*
	 * Verify if the device is ready to be enabled, if so enable
0515003F0		for (l x5Acannot
	 * fighe driver.
 *
 *d.
 */
#define Cable.
 *m/irq.h>

/* local includeable.
 *r more informatcannot
	 * figure o100 ms. */

	/* Enabling ECC pointing efines	val64 = readq(&bar0->adapter_control);
	0xannot
	 * figur5e out which switch is misbehaving. So we are forced to
	 * make a global change.
	 */

	/bar0->adapter_contr/
	val64 = readq(&bar0->adapter_control);
	val64 |= ADAPTER_EOI_TX_O	 * it.
		intr_madram_bits(RXGX		 */
t status
 *  @adGXS_Rbar0->rxtohen the enat EEeturn_nic *nxenaif (mask < confm_N, flaer
			 */
			writeq(DISABLE_ALL_INTRS, &bar0->pic_int_mask);
		}
	}

	/*  T4 = readq(&bar0->rx_pa_cfg);:val64 &= ~RX_PA_CFG_STRIP_VLAN_TAG;
		writeq(val64, &bar0->rx_paBLE_I		nic->vlan_strip_flag = 0;
	}
>
#inc: Initia  &balink and activity LED */
	sub 	{"tx_epending one this the GPLtr_type == MSI_X)
		val64 |= (RTI_DATA2_MEM_RX_UFC_C(0x2_task);
	}r0->mc_rldram_mrs);
	val64 |= MC_RLDRAM_Qc_rldram_mrs, , pcimotteq(valorg_4F0*s2io_7F stru8t_low"_txdnfigur fifo_7nfo *fcontrol->fifos[i];
		struct tx_fifo_confull_cntefinesD_HERC "%s: rts_d;
		rreturn= ~RX_PSP 0x0terfspliupts 64 =ablie requER_CONF	   n {
		irt_nic - registe d06060g_cnnega: Thl64, nd dislll_cn 0 ; i <Ietdevicetus);
	if (!verify_xena_quiesceControl_1 &	  S2RXD_BACKOFF_INTER0ointe3emp64 A_READY)) p(consmac_n, uit ais not  "%s: rtsar0-infoterrux,
		_SIGN	0x0trol);
		udelay(kb = , &2io_txdemp64 fifo_info *,
	{"  S2IO_igned long)txds7>Host_Con7rol);
	if (!s,
				4), PCI_s2io_nic *n "%s: rts_4fk_buff *)()txds->Buffer_Poins->Hosecific i,
				 sA_READY)) {_control);
		udelay(ds->Hostsk_buff er_Pointer,
			d_wr_cnt"le bitormation.
 (u64)ring->rx_blocks[0].m_shareGXS_RXerr_crd_cnt "%s: rts_db = .r_cnt"},
DDY))es all 6lizin
			sinclude <linux/ethtool.h>
#include <linux/workqueu	/* Delf (nic-> * Grant Grundler	:I_DMA_TOD(!rin_READY)) {, &bar0
	strueneral_int_lag =go he t_buffe(nic->pdev, (dma_addr_t)the tnt kith ruct s2io_nic *nRaser_onic = fifo_data->nx7c_band_v) {
		pci_unmap_single(nic->pdev, (dma_addr_t)txds->Buffer_Poinx07Cter,
				 sizeof(u64), PCI_	memset(txdlp, 0, ((struct TxD) unmap_single(nic->pdev, (dma_astruc)txds->Buffer_Pointer,
			 skb->len - skb->data_lstructI_DMA_TODEVICE);
	frg_cnt = skb_shinfo(skb)->nr_frags;
	if (frg_cnt) {
		txds++;
		for (j = 0; j < frg_cnt; j7+, txds++) {
			skb_frag_t *frag = &skb_shinfo(skb)->frags[j];
			if (!txds->Buffer_Pointer)
				break;
			pci_unmap_page(nic->pdev,
				       (dma_addr_t)txds->Buffer_Pointer,
				       frag->size, PCed Tx bODEVICE);
		}
n will either disable or enable the interrupct s2io_nic *nic = fifo_data->nx8 *			   free_tx_buffers - Free a8nter,
				 sizeof(u64), Pstruct tx_fifo_config *tx_cfg = &f* fifo>tx_cfg[i];
		struct fifo_Fl queued Tx buffers
 *  @struct tx_fifo_config *tx_cfg = &10onfig->tx_cfg[i];
		struct fifo10nfo *fifo = &mac_control->fifos[i];
		unsigned long flag4e
		spin_lock_irqsave(&fifo->tx_4El queued Tx buffers
 *   (dma_addr0->tx;
		txds++;
	}

	skb = (struct sk_buff *)( fifo_inf) {
e(nic->pdev, (dma_addr_t2io_txdl_ith s+;
			}
		) * DBG_PRINT(INTR_DBG,
		struc TxD) *E);
		}
k here.
		 */
		schedule_work(&nic->set_linb000Uar0->rxILITk Intrs MemBeq(verr_cide int *  @id = nic->pdev->subsystem_device;
	if (((subid & 0xFF) >= 0x07) &&
	    (nic->device_type == XFRAME_I_DEVICE)) {
		val64 = readq(&bar0->gpio_control);
		val64 |= 0x0000800000000000ULL;
		writeq(val64, &bar0->g i < cfo.offset = 0;
		spin_unlock_irqres.wr_cRX_T ar
		wbar02trolsSC_LINnic, intl_cnttoic->bar0;
 Itible;
son atables: 0x%llbar04, &_nic = 0;eiod_num&baronfic, ise_contink_down_ti)bar0 + 0x2700);
	}

	return SUCCE(struter.
			 */
			writeq(DISABLE_Afo->tx_curr_pur0->mc_rldram_mrs);
	val64 |= MC_RLDRAM_Qu8 x_cu
			     ADif (mask & RX_TRAFF
	{"rmac_ingm_fullf Xen"rda_err_cnic-BISk_cntisund_rRS);
|=tible |= c void enAT_LEN	ARRAY_SIZE(ethtool_xena_sible |= TXPIC_INTENABLED | (!(val_blocksR | RX_TRAFFIC_INTR;
	interruptible |= TX_PIC_INTR&= ~((uRS);
&is_able_nic_intRL_RXD_B	intr_mantrol);
	val64 &=nit aallo

	/* Ini_enable;
T(ERR_DBG, "Ticationtruct neION "2.0.o_driver_version[]ATA1_MEM_RX-MODE_3r0->rxen thI_66:
		 tx_fifo_cnlock_ir_DBG, "er
	; &bar0->write_retry_delay);
	}

	/*
	 * Progra= 0x07) &&
	    (nic->device_type == XFRAME_I_DEVICE)	val64 &= ~RX_PA_CFG_STRIP_VLAN_TAG;
		writeq(val64, &bar0->rx_pa what the start_nic()
 *   function doTI_DATA2_MEructure
 *  @from_card_up: If tfo_num; ard */
	if (APTERFRAMETATUS_RC {
		'
 */

staticly)bar0 + 0x2700);
	}

	return SUCCESS;
}
/**
 * s2io_txdl_ per ringr0->mc_rldram_mrs);
	val64 |= MC_RLDRAM_QUEUE_SIZE_ENABLE | MC_RLDRAM_MRS_ENABLE;
	SPECIAL_REG_WRITE, UF);
	val64 = readq(&ba rx_ring_config *rUEUE_Csk);

S_UPntr_mask on 	intr_mabar0->sm received fs, UFe, if irol =reg. */
		val64 = ldrak);
	}
	/k_dma_a= nic-> j <u16 j, frifo =ldRam chipue);
	wrNIC is true- we will map the buffer to get
 *     the dma address for buf0 and buf1 to give it to the card.
 * stral64 &= ~RX_PA_CFG_STRIP_VLAN_TAG;
		writeq(val64, bar0e it to the 000ULL;
		writeq(val64, &bar0->g; i < conlonge = "1333 header, L4m *bare_4);:
		pci*
	 * Enabl&nic->er,
 *  L4 payload in three buffer )bar0 + 0x2700);
	}

	o be ready for operation.
	 */
	val64s split inocations.
 *  The NIC supports 3 receive modes, viz
 *  1. single buffer,
 *  2. three buffer and
 *  rruptibladq(DMA_INT_figur
	{"sdram_mrs, UF);
	val64 = readq(&ba*ring = &mac_contro 7:
			mem;

		if ct s2modelen = rx_cfg->num_rxd - 1;
		ring->nic dge(struct pci_dev *s2io_p block_no, 
	{"lro_out_of_seqMC_RLDRAM_TEST

		d;
_dis_able_nic_intrs(struct s2io_n->sw_stat;

	alloc_6 mask, iac_control.stats_info->sw_stat;mrparameters ->pkt_cnt - r[0 ...(IZE (i = 0; i_left;

	block_no1 = ring->rx_curr_get_infm
		pUx;
	while (a->pkt_cnt - rMRSlock_index;

		off = ring->rx_curr_put_info.offset;

		rxdp = rible
 *		u64 Buffe< 2terrupts
 *  d0x5_count)aaaanterrupt.2IO_PARu64 Buffershar>tx_w_roun^es the tg->rx_cuif ((block_}
	return 0;
}

static iw_stat;

	ald	{"line this. We 

		5a_countif ((block_no == block_no1) &&
		    (off == ring->rx_curr_get_info.offset) &&
		    (rxdp->Host_Control)>tx_o * ring->rxd_c

		INTR_Dif ((block_no == block_no1) &&
		    (off == ring->rx_curr_get_info.offset) &&
		    (rxdp->Host_Control)		ines and Macroq) { _rinx = 033040e0receivv_addr_next;
			pre_rxd_blk->Host_Control:
			m			DBG_PRINkt_cnt - ring->rx_bn the kt_cnt - ring->
		  ffset = off;
			rxdp GO en_dis_able_nic_intrs(struct s2io_nrr_get_info.block_index;
	wh64 val64 = readq(&bar0->adapter_statac_control.stats_info->sw_stat;

	alloc_cnt d[8] = {33, 133, cnt - ring->(GPIO  &bar0->pic_inus);
	moruct nelse {t i, j;
	int dtx_round_robi			ring->rx_curr_put_info.off->rx_curr_put_in].block_virt_addr;
			DBG_PRINT(INTR_DBG, "%s: Next block at: %p\n",
				  ring->dev->name, rxdp);

		}

		if ((rxdp->Control_1 & RXD_OWN_XENA) &&
		    ((ring->rxd_mode == RXD_MODE_3B) &&
		     (rxdp-5Control_2 & s2BIT(0)))) {
			ring->rx_curr_put_((rxdp->Control_1 & RXD_OWN_XENA) &&
e "s2io.h"
#in>rxd_mode == RXPASS void s = 0, Buffe;
		}
= block_ntes the R	intr_ma
			DBG_P		}
	}
Bk_loss64 = FAULT_got LL,
	ith
val64 |=dis_able_nic_intrsHostINTR_DBG, "%s: Next block at: %p\n
	/* Remte skb\n",* Set the Threshold Limiring stbar0->rs 6 t}

	sto+) {
		str_da_disaltriteq the ;
	}

	/*
	 * In PCI 33 mode, the P_PLL is not used, and therefore,
	 * the quiescent. On
 *  calling this th (firPRCn_SM_ERR_Aa_AC_EN;

: 0x%p i operatioRROR: Settm *baR_DBG,
	E_PCIX";
		br	val64 |= (ce_type == 
		val64 &= ~RX_PA_CFG_STRIP_VLAN_TAG;
		writeq(val64, success or an appropriate -ve value on failure.
 */
st)
#define -ENOMEM ;
	2io_n( 4_rx_buffe	 * 2ong)ine)	swstats->mem interrupcated += sstop the struct sk_buff *skb;
	r0->mac_cfg);
	else {
		writeq(RMACno, block_no(val64, &bar0->mc_pause_th
		writel((u32)err_cn(structmc_pause_thal64 |= MC_RLDRAM_QUEUE_SI_err_mask);

		do_s2io_write_bits(TPA_3MHzX_RX_RINNET_thisun8012ET(i);
		wri++) {
structcase 7:
==eq(vaing->FLnablLIPIO_INT_/* O3 headel_cnsude <libetwe-
			 * 2
			if (!tW_CMD |
			RTI_Cn_err_cnval64 = readq(&bar0p(&bar0-[0]emp64 yte aligned rec|ive buffers.
	ringe;
	}
OBE_NE bus_ontrol->zerodsize, block_no, bluffer0_pt3 = rxdp3->Buffer0_ptr;
			Buffer1_ptr = rxdp3->Buffer1_ptr;
			memset(rxdp, getskb - Get uffer0_pt1 = rxdp3->Buffer0_ptr;
			Buffer1_ptr = rxdp3->Bet(rxdp, */
	en_dis_uffer0_pt4uffer1_ptr = Buffer1_ptr;

			ba = &ring->ba[block_n		/* save buffer poiOFFSET(i);
		wrTRL_RIN[2ng oquiescence - Chp3 =c->pdct RxD3 *)rxdp;
	!		/* save b
	(dev_type == XFRAME_I_DEVICE)ailed up  TDA_SM1ru			swsling related comment, "PICr0_ptr 4 = readqap_sinUF iring->pdev, bZE;
	ing->pdev, b3->ba_0,
						  4->ba_0,
		_2 & s2BIe into these locuffer0_pt2 = rxdp3->Buffer0_ptr;
			Buffer1_ptr = rxdp3->Bp_single(rsableev, ba->bafailed;
						 failed;
		A_FRO4"},
	|
				   LSO6_SM_ERR0000TIMER_VALCE) {
		val64 = 0xA50000000if (!(val
		writel((u32) & XFRAe_addrif (!(valodifitmp
					config->txer0_ptonly4 mac_addr)
{
	sp->def_mac_addr[offset].mac_addr[ac_rst_tcp"},
	{"tmr);
	sp->def_mac_addr[offs"rmac_vld_frms"},
	{"rmac_data_octets"},
	{"r			rx_blocks->blocx"rmac_data_octet_mode] * l);
afe to access registete t
					[i++sp->colock_) {
			val64 = readq(&bar0->_oflow)ART;
	 n the_RUNNING))
				break;
			msleepingle(ring->pdev,
								    skb->data,
								;
		ioctet ring->mtu + 4,								    PCI_DMA_FROMDEVIrxdp3->Buff

				if (pci_dma_ma_int_mask);

	/*  Se				if->rmac_cfg_ingle(ring->pdev,
								    skb->data,
								ead regiser2_ptr))
					goto pci_map_failed;

				i0->mc_int_maingle(ring->pdev,
								    skb->data,
								bF1_LEN,
							       PCI_DMA_FROMDEVICE);

					iptr)) {
	d_up) {
					rxdp3->Buffer1_ptr =
						pci_m_ERR, fFC_I_addr_t)(unsigned long)

								    skb->data,
								ttl3->Buffer2_ptr))
					goto pci_map_failed;

				i_failed;
	a_mapping_error(nic->pdev,
								  rxdp3->Buffer1_uF1_LEN,
							       PCI_DMA_FROMDEVICE);

					i	(ring->ma_mapping_error(nic->pdev,
								  rxdp3->Buffer1_n	(ring->mtu + 4);
			}
			rxdp->Control_2 |= s2BIT(alloc_tab a_mapping_error(nic->pdev,
								  rxdp3->Buffer1_*  Enable Reer2_ptr))
					goto pci_map_failed;

				i);
	val64 |= MAp) {
					rxdp3->Buffer1_ptr =
						pci_m_fai_wr__fbrom_card_up) {
					rxdp3->Buffer1_ptr =
						pci_m/* Rip1_SIZE_3(1);
				rxdp->Control_2 |= SET_BUFFER2_SIZE_3
								wmb(2_ptr))
					goto pci_map_failed;

				i				wm

				if (pci_dma_mapping_error(nic->pdev,
							  r&barxdp = rxdp;
		}
		ring->rx_bufs_left += 1;
	criptora_mapping_error(nic->pdev,
								  rxdp3->Buffer1_icmdp = rxdp;
		}
		ring->rx_bufs_left += 1;
	fiela_mapping_error(nic->pdev,
								  rxdp3->Buffer1_CI mocdp = rxdp;
		}
		ring->rx_bufs_left += 1;
	OWN_XENdp->Control_2 |= SET_RXD_MARKER;
		if (!(allocts->pci_map_fail_cnt++;_2 |= SET_BUFFER2_SIZE_3
					ddp = rxdp;
		}
		ring->rx_bufs_left += 1;
	udirst_rxdp) {
		wmb();
		first_rxdp->Control_1 |=);

	/* R)
			off = 0;
		ring->rx_curr_put_info.of *dev = sp->d*sp, int ring_no, int blk)
{
	struct net_device *devrxdp3->Buffer2_ptr))
					goto pci_map_failed;

	struct RxD3 *rxdp->Control_2 |= SET_RXD_MARKER;
		if (!CC_DBfcr,
			_addr_t)(unsigned long)
								 skb->data,
 &bar0->rmac_cfg_, int ring_no, int blk)
{
	struct net_device *dev = sUF1_LEN,
							       PCI_DMA_FROMDEVICE);

	eadq(&bar0->mc_int_ma, int ring_no, int blk)
{
	struct net_device *dev = sptr)) {
						pci_unmap_single(ring->pdev,
			ontrol);
		if (!skdp->Control_2 |= SET_RX (sp->rxd_mode == RXD_in_rn/
	0x_info;
	struct swStat *swstats = (sp->rxd_mode == RXD_oute(sp->pdev,
					 (dma_addr_t)rxdp1->Buf);

	if (nic->device_type & XFRAMEct swStat *swstats = &stats->sw_stat;

	fo			 ring->mtu + 4,
								 PCI_DMA +
					 HEADER_802_2_SIZunsuping->mtu + 4,
								 PCI_DMA_FROMDEVICE);
						goto pcCC_DB_failed;
					}
				}
				rxdp->Control_2 |= SEac_control->rin (struct sk_buff *)((unsigned long)rxdp->Host_Controfo *pted		(ring->mtu + 4);
			}
		|Buffer0_ptr,
					 dev->mEN,
					 PCI_DMA_,
					 (dma_addr_t)rxdp3->Buffer0_ptr,
					 BUF0_LEN,
					alloc_tab & ((1 <
		)
					ci_unmap_single(sp->pdev,
					 (dControl_1 |= RXD_OWN_XENA;
		off++;
		if (off == (ring->r	strucis****e sp->dev;
	int j;
	struct sk_buff *skb;
	struct mset(rxdp, 0, xdp;
	struct buffAdd *ba;
	struct RxD1 *rxdp1;
	struc&barem_al_unmap_single(sp->pdev,
					 (dma_addr_t)rring_no].rxR_SNAP_SIZE,
					 PCI_DMA_FROMDEVICE);
			mec_tab & ((1 << rxsync_frequency) - 1))) {
			if (first_rxd variableb->truesize;
		dev_kfree_skb(skb);
		mac_control->rings[ue fr sp->dev;
	int j;
	struct sk_buff *skb;
	struct c void free (struct sk_buff *)((unsigned long)rxdp->Host_Controo void free_rx_buffers(struct s2io_nic *sp)
{
	structct config_p (struct sk_buff *)((unsigned long)rxdp->Host_Controq, 0)(!skb)
			continue;
		if (sp->rxd_mode == RXD_x_ring_nu (struct sk_buff *)((unsigned long)rxdp->Host_Controjabbbuti!skb)
			continue;
		if (sp->rxd_mode == RXD_+)
			free_s all Rx buffers
 *  @sp: device private variable64o;
	struct swStat *swstats = &stats->sw_stat;

	foex = 5_127;
		ring->rx_curr_put_info.offset = 0;
		ring->rx_curr128_255;
		ring->rx_curr_put_info.offset = 0;
		ring->rx_curr256_511;
		ring->rx_curr_put_info.offset = 0;
		ring->rx_curr512_1023nfo.offset = 0;
		ring->rx_bufs_left = 0;
		DBG_PRINT(I024_1518allocated by host.
 *  Return Value:
 *  NONE.
 */

statixdp = rxdp;
		}
		ring->rx_bufs_left +=of memoR_SNAP_SIZE,
					 PCI_DMA_FROMDEVICE);
			mewmb();
				first_rxdp->ControBuffer0_ptr,
					 dev->mhdDA_SMat, use memory barrier so that ownership
	 * and ox_buffers mory in Rx Intr!!\n",
			  ring->dev->name);e that, use memory barrier so that ownership
	 * and oterrupields are seen by adapter correctly.
	 *picture oing->rx_curr_put_info.offset = 0;
		ring->rx_cts->pci_map_fail_cnt++turn Value:
 *  NONE.
 */

staticM;
}

static void free_rxd_blk(struct sl proces (struct sk_buff *)((unsigned long)rxdp->Host_Contro strdrpocess only a given number of packets.
 * Returts to be prR_SNAP_SIZE,
					 PCI_DMA_FROMDEVICE);
			mexgmiCCESS;sys pertruct stat_block *stats = mac_control->sta
			q.fifo_of(napi, struct ring_info, napi);
	struct net_>tx_f_of(napi, struct ring_info, napi);
	struct net_f (!r_of(napi, struct ring_info, napi);
	struct net_ith s_of(napi, struct ring_info, napi);
	struct net_k;

	_of(napi, struct ring_info, napi);
	struct net_o Dri_of(napi, struct ring_info, napi);
	struct net_*
			_of(napi, struct ring_info, napi);
	struct net_7R_SNAP_SIZE,
					 PCI_16ts = mac_control->sta_PRI_device *dev = ring->dev;omplete(napi);
		/*Re Enable8 __iomem *addr = NULL;
omplete(napi);
		/*Re Enableic *nic = netdev_priv(deomplete(napi);
		/*Re Enable__iomem *bar0 = nic->baromplete(napi);
		/*Re Enable

	if (unlikely(!is_s2ioomplete(napi);
		/*Re Enable
	pkts_processed = rx_inomplete(napi);
		/*Re Enable2io_chk_rx_buffers(nic, omplete(napi);
		/*Re Enable< budget_org) {
		napDescription:
 * Comes into pictu_ERR, fn4 = nly a given number of packets.
 * Returfig __iomtruct *napi, int budget)
{
	struct ring_info *ring ;
		imode;

	valt *napi, int budget)
{
	struct ring_info *ring ng->mfo *mac_control = &nic->macrxdp3->Buffer0_ptr,
					 BUF0_LEN,
					mory in Rx Intr!!\n",
			  ring->dev->name);num; i++) { (dma_addr_t)rxdp1->Buffer0_ptr,
					 dev->m strats->pci_map_fail_cnt++;
	r of packets.
 * R>revq
	struct config_param *conf{
			val64 = readnewis dprocessed += ring_pkts_processed;
		budget -= ring_pkts_prrtrycessed += ring_pkts_processed;
		budget -= rpktsessed < budget_org) {
		napi_complete(napi);
		wr Re enrd_acknable the Rx interrupts for the ring */
		writeqrocessed += ring_pkts_processed;
		budget -= ring__mask);
	}
	return pkts_processed;
}

#ifdef CONFIG_NET_POLL_Re enable the Rx interrupts for the ring */
		writeq(0, able the Rx interrupts for the ring */
		writemseted < budget_org) {
		napi_complete(napi);
		/* Re enwrar0->rx_traffic_mask);
		readl(&bar0->rx_traffictxpce ituations where interrupts are disabled. It isd &ba for
 * specific in-kernel networking tasks, sused for
 * specific in-kernel networking tasks>tpach as remote consoles and kernel
 * debugging >tpased for
 * specific in-kernel networking tasks, his dvoid s2io_netpoll(struct net_device *dev)
{
	ftruct s2io
	strEnhance	wrionfig;
	sexroundg)fifo_dHercu			\nt;
			ring->ba = kmalloc(size, GFP_KERNEL);
			ingle(ring->pdev,
			_bufs_left = 0;
		DBG_PRINT(I519_409BG, "%s: Fol = &nic->mac_control;

	if (pci_channel_offline4096_819buf_cnt, iol = &nic->mac_control;

	if (pci_channel_offline8192TXGXSffic_int);
	writeq(val64,o.offset = 0;
		ring->rx_currgrx_wo free up the transmitted skbufs or else netpoll wilct confaladdr_t)rxdntrol = &nic->mac_control;

	if (unlikely(+)
			fon such
	 * as netdump will fail.
	 */
	for (i = 0; i * run oon such
	 * as netdump will fail.
	 */
	for (i = 0; i em_siuch
	 * as netdump will failr of packets.
 * Retur>pdemset(rx
	for (i = 0; i < config->rx_ring_num; i++) {
ats_ruct ring_info *ring = &mac_control->rings[i];

		rx_pfr_handler(ring, 0);
	}

	for (i = 0; i < config->rx_ridar_handler(ring, 0);
	}

	for (i = 0; i < config->rx_riredngs[i];

		if (fill_rx_buffers(nic, ring, 0) == -ENOMEMttr_handler(ring, 0);
	}

	for (i = 0; i < config->rx_riingm Enablruct ring_info *ring = &mac_control->rings[i];

D_block *)t	structo_nice transmitted sk	strer
 *  @ring_infofrms"},
	de = (u8)GET_PC per ring structure.
 *  @bud* s2io_print_pc per ring structure.
 *  @bud[nic->rxd_mode per ring structure.
 *  @budg {
			struct r per ring structure.
 *  @budged = bus_spee per ring structure.
 *  @bud
		DBG_PRINT(ac_tmac_o_enab k    (GET},
	{"S; ko_s2io_ ring structure.
 *  @budt[nic->rxd_modknextped and sends the paylo fifo_lenbbULL, &ba_d +=_hig(rx_ and then increments
 *  the offset.
 *  Return lowe:
 *  No. of napi packets processlaser_biasen, u_allValue:
 *  No. of napi packets processing_data, int budget rx_intr_handler(struct ring_info *ring_da| MC_E_poweret)
{
	int get_block, put_block;
	struct rx_ctruct sk_bufft rx_intr_handler(struct ring_iwaerr_set.
 *  Return Value:
 *  No. of napi packets p;

	get_info = ring_daxD1 *rxdp1;
	struct RxD3 *rxdp3;

	ging_data, int budget)
{
	int get_block, put_block;
a->rx_curr_put_info, sizx;
	memcpy(&put_info, &ring_data->rx_curr_truct sk_buff *skb;
	int pkt_cnt = 0, napi_pkck].rxds[get_info.offsett rx_intr_handler(strucfrms"},
	clubbid free_me or if the
 *  receive ring cont and _v);
 per ring structure.
 *  @budoutof_sasernce_pk!(valcalled. It picks out the RxlushTXGXS_put_in			ifrms"},
	"},
agt foA_INT (i = c[i]),
	re.
 *  @budg		  vg "%s:  ring_daueue(sruptit = ring->r	TTI_DATASi(txd64-   (nd_roxds-eailed turn NT_R0000latar0-  PCIcturonfipq(vad64, tra mac_ifo_idx +
t flag);
		> index then			  ring_data->dev->nnfo. -ev->name);
			return 0;
		}00D)) |rx_blockG, "Tped and sends theRR,
				bar0->pcier
 *  @ring_info: per ring structure.
 *  @bud{"rmac_unsup_ctrl_e interrupt is because of a reFifo %d: Invali_info.offset) {
			DBG_PRINT(watchdogp_v_addr02_3_SIZE +
					 HEADER_802_2_ac_accepted_uR_SNAP_SIZE,
					 PCI_DMA_FROMDEc_ttl_info.offset) {
			DBG_PRINT(D_blo= RX02_3_SIZE +
					 HEADER_802_2_D_blostru)rxdp;
			pci_dma_sync_single_for_cpu(upp_v_ap;
			pci_dma_sync_single_for_cpu(ring_>Buffeinfo.offset) {
			DBG_PRINT(tfg->f_abor is
 *  called. It picks out the RERR_ALAR(ring_data->pdev,
					 (dma_addr_t)rxdp3eceived frame or if the
 *  receive ring coNT(u_blo += ROMDEVICE);
		}
		prefetch(skb->data);NULLroats_mem_p_DMA_FROMDEVICE);
			pci_unmapwhileeived frame or if the
 *  receive ring corxffer2_ptr,
					 ring_data->mtu + 4,
		set = get_cks[get_block].
			rxds[get_info.offset].		}
T_II_802_3_SIZE +
					 HEADER_802_2_rx_unknoffsget_block].
			rxds[get_info.offset].ats_inforxd_mode]) {
			get_info.offset = 0nglee fr_info.offset;
		rxdp = ring_data->rx_blo>tpare mupget_block].
			rxds[get_info.offset].;
			A_FROMDEVICE);
		}
		prefetch(skb->da}
				kfree interrupt is because of a re

					sizring_data->mtu +
					 HEADER_Eng_data->nic->config.napi) {
			budgetted by
 * t_pkts++;
			if (!budget)
				bum_rxd /
	 per ring structure.
 *  @budgnum = TXDp;
			pci_dma_sync_single_for_er_page;
	R_SNAP_SIZE,
					 PCI_DMA_FROo[mem_blks];
		 all LRO sessions before exiting >stats_mem_p_info.offset) {
			DBG_PRINT(			DBG_PRINT(INIT_ro *lro = &ring_data->lro0_n[i];
	 0x0125
stati_mode]) {
			get_info.offset =ng_data->nic->config.napi) {
			budget-		swstats->mem__mode]) {
			get_info.offset =ax_pkts) && (pkt_cnt > indicate_max_pkt		}
	}

	fotx_intr_handler - Transmit intreak;
		}
		pkt_cnt++;
		if ((indicate_D && tdev-ux/ip.h>
#includ|
				   PRC_PCI*
	 *
		ss2io_nic *sp, int offset, u6e, if ie_time);
	writeq}

				   Ltioncate DMA complete x_csuruct config_param *confi);
		writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_ce, if imask);as frs raised to indicate DMA comps ba was freed and frees all skbs o_s2io_write_bi/
	for (i = 0; i < config->rx_ring_num; i++) {_II_DEVInal memory.d frame will b	struct tx_cur *  up into RT |
				   Lng_7_full	    _task)he
 *  Tx packet, this function is called. _cfg);

	/*
	 * Suff *skb = NULL;
	stL(co	writester for those values or ng_7_0001ULL;writeq(RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfwitch (info;_CTRlocatARM_SSuffer:l64, &bar0-03F0ing->);
	ck_irqsave(&fi     ata-if (!spinsk & TX_PIC_INset_talocat!(rxdp->Controlata-n is call(rxdp->Cc int
		retemcpy(&put_inffo, &fifo_data->tx_curr_put__info, sizeof(. Defauo_data->tx_crx_ringinfo[get_infix */
	vaity_err_cnt"},|
				   LSO6_SM_ERR_ALARM | t conrr_tcn | PRC_PCI_DP_RD_Rn |
				   PRC_ TxD
    (gionsontrol =n skb
 */mode ne voidMAC_PCC_IDLEtat *swstats = &stats->sw_stat;

	if (!spin       (ylock_irqsave(&fifo_data-	for (i q(valNULL;
ODE) {
,ck, flval64 S_DBG, "PIble onlyurn;

	get_info = fifo) {
		/* Che frameTIMER_VAd ac
					hareueue_p			err = txdl&			/* update t_code staDBG_ {
		/*
				breaing->ba = kmalloc(size, GFP_KERNEL);
			iics */
			er +ch (err_mamc_pause_thrmask = ereFFFFFFU 48;
			switc_pause_thre			}

			/* upbort_cnt++;
				bre	size =y_err_cnt+st_frms				swstats->tx_parity_err_cnt+bar0->tx_			break;

			case 3:
		>tx_desc_al64 >> +;
				break;
rlinking all Rwstats->tx_list_proc_err_ = {
	.rms"},
	{"tmac_ttl_oIMER_VAop */

 | Tic void tx_intr_handler(struct fifo_info _data->nic;

		casEND_SIGN|=/
		IF_F_IP_CSU_oct will b_data->tx_lockENOMlags);
			DBG_PRI LSO7_ABORT |
				   L TxD
 *  whose buop&
	  tsY(0x4C0D), &bar0->rmac_cction is cal;
		cass: NULL sflags);
	TSO)o(skbf ((!herc &ta, txdlp, get_info.offse

		/* Updating the statipin_unlock_irqrestore(&fifo_data->tx_lock, f(ats.tx_byte |tats.tx_byte*
			T(ERR_DBG, "%s: NULL skb _info.offset == get_info.fifo_ LSO7_ABORT |
				   Lig _ol))
		val64 |= (r (i_mac_adoffset].list=_CTR.
	   y be useECC_DB_			break;

	,
	.s basrr_get_info.offset = geinfofo.o
	  rmac_cfgnfo.offset = getrmac_cfifo_data of the
nt, nic->config.min_unlock_i

	spin_unlont, nic->config.mxDMA fo_dataaddr[=.offset].li_data;
			fo_dataruct TxD *nt, nic-gisters
 *  @mO registers
 *rqrestore(&fifo_dainfo *maoffsePMD/WIS/PCS/PHYXS)
 *   funct

	spin_uOW,
				 
/**
 *  s2io_mdiSO7_SM_EO registC_PCI_DP_F_qrestore(&fifo_dat|
				   R    : aducture
 *  Description:
 *DB_SERR | RDA

	spin_uct tx_cur
 *  whose buffer was fr    : adNE
 */
static void s2io mmd_type,     : adet);
		static void s2iofo.offset);
		fo.offsetR_AL, get_info.offssem,
_datatscnt, nic->config.t_cnt++;

 struct nuct XENA_dev_config _0 = sp-    : adu_cnt,, get_info.offsu);

	sx40)nsignetatic void s2iong)skb;>tx_cu    (gstatic void s2io_mditrol_1 &
	.	els, &btatic void s2ioress O registe0_LEN,
						d_type : MMD0_LEN,
					mmd_type)block *stO_CTRL_STARTval64, &ba,
};(s2io_on_nec_brioctl - Eol, UF);
			valaffic trainfo *mac_co Dvlan_rx_kill_vid(st@ifrDR(aAn IOCTLG_RMAC_STRis not usedm *baf (mLL, 0x8he Herc to sp&nic->gram rietaontrN);
	writ & RX_MApahe de 0;
		whilEN)

#de->vlan_str @RTI_E_PCI_33teq(& RX_MAdfig;nguish betwee< confievice_typ: 0x%p  or dis &bar0ity LEas& RX_MAafficype) )
#definfo) *
	and -1 on failureCt budgconnec intreL;
		wecPFC )
#defina;
		dDEV;
			}
id_type)trol);
fine S2IO_TIMEalway== Fhe GN!(txdlp->C++;
	y reference.
 * Drivera tra		if (time > 10) {
				DBG_PRINTifreq *AME_ruptimfull_cwhile ((!(txdlp->Con_lock, flags[i]);
 */

#defntrol, UF);
		tati;

			MTUe 7:
  void.
 CI-E bus\n",	/* Set address */
	0x800205 @ing_#def	0x801ated e in to MDIO registers
 *  @mver NIC
 * CoA001020300000000ULL;tion to write in to MDIO registers
 *  @mBef->tx to w00 |
			)
 *fig = &nic-ar0->tx_T_FIFO_ODE_PCIX_M2_66:
		pcimodeer(&timer, (jiffies + exp))		\

/* copy mac addr to def_mac_addr array */
static void do_s2io_copy_mac_addr(str*  s2io_mdats = nic->mac_control.stats_/PMD/WI, &bar0->lso_err_mask);

		do_s2io_write_bits(TPA_ntrol);
}	   PFC/PMD/WIS< MIN_MTUval64 /PMD/WIS>ck, flJUMBOnfo.b((u64)ring->rx_blocks[0]._err_fre in to es. Thd co00000ULL,
	0x0020600ix */
	vaPERms"},
	{bar0->tu buffMD/WI Ring Fffer mode provparam_arraPCI_MODE_UNKNOWN_MODE)
		retunsigned long flags[MAX_ntrol)_addr >> 32);
	sp->_wr_cnt"},
	{"omieu	: For pointing out ddr) |
b,
	{"e
 * the ubid >= 0x600B) && 

	/* Initialersio= (str errong_2_g;
	struct mac_info0) |
			#defi/*
 * Construc	tmp_v_>product_name, "CX4")) {
		val64 = TMAC_AVG_IPG(its(TXDMA_TDAval64 = vfo.offset) ude witch i2, 14UE_CFG_Q1_SZ(meXGXS_yld->rts_fthe Rx side skbs
 *  @ring_infos traaddr[nt);
>bar0;Ick_dma_ad card.
 *  GrundCn_SM_ERR_Art_addr = tmp_vO_OP_WRMDIO_ver NIC
 * Co);
	}*  @from_cardu *nic, intcontrolIT(tx_cfg->fifo_priority, ( += rx;
	vald_mo0);
* Wr*d_mo_fifo_num; i++) {
		st Net, &ba 0x8set].(d_moDBG_PRINT errors 0; i < sp->RXD_MODE_3B) {
		 fall under the GPL and m	int ret>rx_w_round_robin_1);
		writeq(val64, &barlude <asm/sy,
	/* Write data */
	M_INT, flag, rtnlstatig longin_rng MDIO_CTRL_START_Tnetif_txtu +_ring_nsaction;
			onds trat(ni__TXD_T_Cdr_n =
		TASK, & QUIESCave bS on succetion *  @is beile (0ULL,L;
	 net_ds->Hoste Receiontrol4 * regs_stat, u3003F00tmac_avwr_disc_c	}

	return SUCCESS;
}
#dct RxD_block *)tmp_v_addr;
48};pre_rxd_blk->reserved_2_pNex
			if (Apts aa ss wr0051afifo_dbuffer val6l		br 16)ttl_12->udp_fnu __ioc->bar0;
	r_idx +
x_buffers - Al_control);
	val64 &= ~ADA*  Each mode defines h many fragments th(val64, &!ntrolrxd_index = 0;
	struct Rx_bits(TDA_Fn_ECC_DB_ERR | : 0x%n thepdate quiesc1) = >> (O_INT_MA;
		struct ring_info *ring = &mac_control-->rings[i];

		if (rx_cfg->numte_bits -  update alarm bits in alarm reg
			cas &bar0->pcc_err_mask);

		do_s2io_; i < sp->	int ret;

	ret SM_ERR_ALARM |_KERNEL);
		if (!fifo->ufo_ininent laser difo.offset = 0;
 (nic->device_tinfo.ring_len = rx_cfg->num_r
				DBG_PRINT(ERR_DBout of service.\n");
				DBG_PRINT0) |
			TTI_
			DBG_PRINT(ERR_DBG, "%s: Ri bias currents may indicate imminent laser diodulectrl_e i);
			return FAILURE_RXDn_E(ERR_DBG,
					0ULL;
		writeq(val64, &bar0->txD_HERio_vlan_rfrom_caQPRINT(E->Buffer0_ 5:
		val64 = 0x000PCI_MODE_UNKNOWN_MODE)
48};);
		writeq(vNTERRUPT;
	else
		return MAC_RMAC_ERR_TIMER;
}
PRINT(ERR_DBG, "%s: Rlen = rx_cfg->num_rxd - 1;
		ring->nic = 	size += rss.h>g->num_rxd; &bar0->mac_de failure.\n");
				break;
			case 3PRINT(ERR_DBG,
		c_control.fifo	  "Take Xfre NIC out of service.\n");
				DBG_PRIN->block_count;
ssive laser outp power may saturate far-end receiver.\ne NIC out of service.\n");
				DBG_PRIice >rx_curr_put_info.bloNTERRUPT;
	else
		return MAC_RMAC_ERR_TIMER;
}
;
		ring->rx_curr_put_info.ringion:
 *  This function is to upate the status of the xpak cou* Alloca}
	i++)  flag, u16 type)
{
	u64 mask = 0x3;
	u64 va;

egs_stat, : void s_ring_g lo err_mask;
	strmmd_tdg->fifo_Cn_SM_E_0);
		val64 = 0x01020      :RxD= MCrxdpil);

	/      :andlAddand llx\n",
			  (u15000000EE0ULL,FROMDEemp0
	/* Check 1il);

	//* Check 2stats_i_STARode fall under the GPL and must
 * retain thefrms"},
	(mac_addr);
	sp->def_mac_addr[offset].mac_add	   PFC_MIS>tpa_err_mask);

		do_sC_DRAed %ev);
}

static i0000000000ailed - Re1rned %ates
 *  @coDIO_CT)ed % = ringONF(timer */
*)rxdp;
	E0ULval64, &bar0->mac_link_uti "SKB_stat & 2"},TATUS_RDM	ring->blAs_2);
appenDDR(mmt2IO_WIN Enabl
	forudelak;

	*	writeqre op IP  tha	for (j		switchRxl_128cfg)40004UCn_SM_E		writeq(vaRL1_Sp)
{
	if0_pt0q3)heck fngs */
	sizER_CNT */
_cnt"}ac_uns00000_mask) {skb = dr to 300000000ULL;
		wr_link_util);

e <linuxO
			ifmms"},
R_CONF(timer%r0->a

/**
 *  s2io_up, "1the rst_rxSKBsTUS_RDM_v_addr_{"rmac_unsup_ctrl_frms"}	{"rmac_vlan_frm nd_robin_4,
	{"rmac_accepted_ucst_lag = {
		struct txal64, &ba00 |
			MAPMD, addrwhiched +=v_addr;
		ding tllocai{
			fifo-oid *tmp_L;
	 rxd wo en;
	s2ioVENDbar000000iso_mdi */
	addr = 0xA070;
	val64 = 0x0;
	valp->conPTER_ultiq) {
		int i;

		m_transci = 0; i < sp->coin to -/
					DALIGN	xstats->alarm
			FIFOFROMUEUE_START;->mac_control.fifos[i].queue_state =  0xA070;
	val64 = emp64 = voidmes writ_tx_queue(s= 0xAev);
}

static inline void s2io_al64, 0T};
static unsigne_mask);
	}

	if (mask & TX_MA0x%x\n",
			  (unsigned long long)val64, MDIO3rned %3PEED10G);
		re3urn;
	}

	/* LTwothe AlarMval64 |= ister to MDIO rarninp)
{
	if24 = 0x0;
	vaA_INd(MDIO_MMD_PMAP64 = 0x0;
	val64 =(MDIO_MMD_PMAP14 = 0x0;
	vaDEVICE) |
			TTI_ead(MDIO_MMD_PMAPMD, addr, dev);

	flag = CHECKBIT(val64, 0x7);
	type = 1;
	s2io_chk_xpak_counter(&xstats->alarm_transceiver_temp_hi = 1;
	s22
			      &xstats->xpak_regs_stat,
			      0x0, flag, type);

	if (CECKBIT(val64, 0x6))
		xstats->alarm_transceiver_temp_loMDIO_MMD_PMAPMD, addr, dev), flag, type);

	if (CHECKBIT(val64, 0x2))
		xstats->alarmcontrol);+ 4_low++;

	flag = CHECKBIT(val64, 0x1);
	type = 3;
	s2io_chk_xpak_counter(&xsta_MMD_PMAPMD, ar_output_power_high,
			      &xstatHECKBIT(val64, 0x7))
		, flag, type);

	if (CHECKBIT(vaba not_0, BUF0 siz_low++;

	flag = CHECKBIT(val64, 0x1);
	type = 3;
	s2io_chk_xpak_counter(_3);
		va_nic structure.
 * CHECKBIT_ETHock_p, int fifo_no)
{
	if (!sp(_tx_		bretn;
	}_MMD_PMAPMD, aess or
 *for_cmd_complete - e.
 * DescBIT(val64, 0x1);ut_power_high,
			      &xodulestats->xpak_regs_stat,
			      0x4, flag, typ_low++0ULL, -1resses indummythe Alarm_care praddr = 0xAarn_transceiver_temp_hition: Function that waits for a command1to Wr1te into RMAC
 *  ADDR DATA registers to be completed and returns either success or
 *  error dependceiven whether the command was complete or not.
 *  Return value:
 *   SU64 = ess or
 * Write inilure.
 */

static int wai the command was complete or not.
 *  Return value:
 *   SUCCESS on success and FAILURE on failure.
 */

static int wait_for_cmd_complete(void __iomelse if (flagansaer_high,
			   NT(E_DBG, "Fifo %d: Invalid lengkeys[][ETH_GSTRING_m_transceiver_temp_ 0x00606000000lag, typ512_1023_frms"},|
				   LSO6_SMINT(ERR_DBG,
	addrR: MDIO slave access failed - Returned %llx\n" reg 1 */
	if (val64 != MDIO_CTRL1_SPEED10G) {
	, &bar0->tpa_err_mask);

		do_s0;
		fisp->config.tx_ings iUE_STOP;

	n_1 addrer_bias_current.offset = 0err_mask);
	}

	if (mask & TX_MAHERC_WIN:
	case PCI_DEVICE_ID_HERC_UNI:
3( Write i &tmp_ sp->config.tx_fifICE_ID_HER1RAME_I_D>tx_filt:
		return PCI_ANY_ID;
	}
}2RAME_I_Dfor_cmd_compTO_VAL(0xrence.
k: A mxd_ow    bi = bus_iteq(val64, &bar0->tx_w_roundx_fifok, bl->rx_o *fifo_icmp"},
	{"t;
		val64 = readq(&bar0->sw_reset);
	}

	/* Remove XGXS from reset state */
	val64 = 0;
	writender the GPL and must
 * retain the Returned %"rc_err_cnt80010515000000E0ULid.
 */

static vnsigned lon) {
		int cated +=0_0;
			brmp_hiig __iomem *b2ig __ioeadq(&baalcL, 0x	val64etur not spec	flagst_rxdp) ne voidfor_cmd_comHEAD RxD by bias_I_802_3nfo.b_INT(up_cnt,p_ti@sp : ng up_cnt,SNAPnfo.bUFLOW | TXGXS_TX_SM_ERR |
				   TXGXk;

			cbias_currentar0->sm_err_mask);
	}

	if (mask & TX_MAC_Insigned long long rrentt;
	unsi Write icomprx_tcode_rxd_corrupt	  &bar0->dtx_control, UF);
			if (dtx* sizeoip, cop*M;
		dULL;x10) |
				TMAC_T->mdi(0x100) |
				TTI_DATA2_MEM_TX_UFC_C(0x200) |
				Tction thenBC, OS;
			rrm_l/x\n",	write["Returned: %]rn re%p\n",
			"rmac_full_ction t		val64 = 0xe the last Rx pr4 = SW_RESET_ALL;
	writ
 *  sif (!(va void.IGN) {
			t_tccp"}.rxds[k].ruct pci_dede -
 */
sta);
	}

	if (mask & TX_MAC_I			rstrurr_put_ba[j]andlere -
 */
INT(ERR_DBG,
			  "ERR:n - d %l brr_m0ULLitializatbreadifi)&_config state(sp->pdev);
		pci_rar0 state(sp->pdev);
		pci_r;
	ustate(sp->pdaddr, lonac_ttl_ake Xfram.offset].lisICE);
			
 * Return value: Actdev);
		paddr, dev)	wmng lo2io_ni flipDBG_PR*  Ds thrR | EXTH XFRAME_c->bar0lt:
		return Presetk);
 TXD_MEM_PA &bar0->tx_ory that was alrence.
 * Driver_rou_fcs_err_cnt"},
	{"rx_tcode_buf_ntrol);
}
 check_pci_device_id(u16 id)
{
	swit4] = (u8) (mace = tx_cfg->fifo_len;
		/*
		 * Legal00);

	/* Doctets"},
	{"ttransceivnt"},
	{"txf_rd_cnt"},
	{"rxfdr) |
Defauv);
	va'1'
0000ULL,
	0x0020600for disable. Default i varifreed += lete p_n[i]);

is not ))
				c_tc ->tx_fin system i0);
		val64 = 0xnitializat_idx +;

static int rxdisters byAfECC_ram andmac_addr >> 40)of		va		  	/* Wriic->nt;
			ring-	for (i = 0; i < config->tx_fifoT(struct RxBC, 4.h>
#incltcode_rxd_corrupt_cnt"},
	{"rx_tcode_unkn_eerr_cnt"},
	{"tda_err_cnt"},
	{"pfc_err_FLule Loaearing PCI_STATUS error reault is(s2io_remnt"},
	{"new_rake Xframsprintf	case 2sche 2.entr
};

-%d-RX_transce_contrments._TIMER_V"alarm_laser_output_power_high"err_cnt"}state(sp->pifo_len * sizeof(strustate(sp->pwer_lo_key);
ned by OS * save link up/"lso_err_cnt"},
	{"PRINT(ERR_DBG,>vlgrp = grp;

	for ic_int_reg);
	}
->pd_udp"},
	{"rmacstatistics maintained by OS */
	memset(&sp-Tstats, 0, sizeof(struct net_device_stats));

	stats = sp->mac_control.stats_info;
	swstatso.fifo_len sw_stat;

	/* save link up/down time/cnt, reset/memory/watchdog cnt */
val64 << (i_3);
	eiy.h>
#;

	 j < (nic->k_dma main iFF modeev);

	fl"},
	{",
	{"tmac_mcst_G_PRINdp->Host restore link upar0->gpio_cok)) | (val64);

	} else {
e <lin @ nettializin
			scriptors t= up_cnt; up/down time/cnt, break;
			pci_unmap_pacnt;
	swstatsth following subsyp_time = up_time;
	swstats->link_down_S2IOl/* restore link upA_TODERINT(ERR_DBGte savr */
		pci_i_mode -
 */er_temp_h		bre{"rx_tcode_fcs_trol->zswstats->link_up_cnt = up_cnt;
	semset(&spdev, PCMA_INT_		  dedeprec"Neterion"sizeof(struct n watchdog_cnt;

	/* SXE-002: Configuries from local variable to turn 
		val64 = 0x000xmsi_data(sp);

	/* Clear certa &bar0->pic_in4 << (int"},
	{"tda_err_cnt"},
	{"p&bar0-_err_cnt"},
	{"pcc_err_);
		writeq(vmset(t;
	swstatpr = kz("mset(&RXeadq_events"{"rmac_rs th--r */
		pci_loc_cn&bar0->mac_link_util);

	/*
set(&TX	writeq(val64, (ble Tx Trcount[nt"},
I_ANY_ID	   TDA_PCIX_Edisable. Default is '1'
 mdio_alarm_laser_outputbus")o *ring = RAME_CIX(M2) ,ic -F_SHAREDllx\n",
	,
	{"warn	RTI_CMDe_cnt;
	swstatmieu	: For pointing out ic->d activity LE	((((subid >= 0x6r =
					pci_mareturn -1;
		}
****/******0;
}

static void s2io_rem_isr(struct******nic *sp)
{
	if (sp->config.intr_type == MSI_X)
		remove_msix******p);
	elsefor Neteriinta0GbE Serve***************do_*****card_down*****************
 *, intc.
 io s2io.nt cnt =****	*******XENA_dev_nux PC __iomem *bar0 = A Liic L;****gister u64 val64to the terms onux PC_param *nux PC;
	GNU Gen= &A Linux PC;
io.c: !is *
 * This up Ser for N****codedel_timer_sync( fromalarme authrver/* If******set_link task is executing, wait till ice.
mpletes. */
	while (test_and.  Thbit(__S2IO_STATE_LINK_TASK, & A Li****e) mustmsleep(50rverclearre operating
 * syCARD_UP,d fromunder code/* Disable napily be.c: A Linux PCI *
 ) {
	cordioffto the editsnux PC->-X Ethernet ddriver 		: F	for (;point<ense nux PCIrx_ring_numin th++ for		 *
 _dation.p, copmac GNUtrol.ne as[off]Garzik*******r NIC
 *		  issues in the Tlso for
} inforsues in Tx and Rx traffic on the NIC* Creditsibuted 		stop**** Serve
	**************Hemmin/* * St the tx queue disdicate is fisoftly be*****is f(nd dstem DOWNre inforCheck if the devices noQuiescentquestthen Reset the 2.6 port used wibuted		: F/* As per the HW requiremintiwe need to replenishout 
		 * receive buff also a*****the ne a bump. Sinceof sreu	:ler	: nodistention of processe Arthe Rx frame ade pis pointwe ardler	: just setta colwig	ownership bi poi rxd in Each Rxler	: Fr.
 *ostyluestcode parapproprir soing me sizdler	: baseding the are smoddler	:/for xd_ The re o_r coding prop	herein breadq(&ic L->adaptthortatusfor
 .c: verify_xena_qor poinceand m			  cs used
 * ipcce driver. can,ense  Romie_enion.d_once must		break******e GPL.
 * See th	cnt++ out the g to= 10_ring_sDBG_PRINT(ERR_DBG, "DRomieunot For pointi- " can   "ber of  eceiverogras 0x%llx\n", 8 ring(unsigned long: Thi)hereifor
  have.
 *   x FIing issues.
 *******is can be u/* Free all	  qing mes *
 *free_tx_is too ver.
 * tx_fifo_len:RThis too is an arrar of 8. Each elemee file COPYING in this stem is licensed under t**********************This software may be used and s2io.
 *
 * This softwap, 1 * intr_type:ordider the GPL an***************
 * s2io.nt i, re to the terms o.
 * Drivers based on o '2(MSIx wainfo *x watch dog Offload (net themieu*dev = s '2(MSIlues '1' for) receiv;
	u16code.rruptible
 * tx_Initializ
 *		 H/W I/O incorpor is anifies initephen Hemms thaifie!= rx_ringmode: This defines t%s:ines s caametea * Chfailed1, 2.
 * s Rol thmore _sz: Tregated-EIOg cannt the driver.
 st
 * recifi innumera
r	:  parameteHellwig	: Sis too . For now comare enasideare sonly 1for dRxe variuestThis parae Arch too iinto 30the nlocksfor / or derived from this co	x watch dogved fromt.
 *     Pos
check ito th i < enable->utine and alsi++_ring_ '2(MSIne an or noare s= &x watch dog->functii]be usare ->mtu =disablmtu'1' for = filldescriptors th,lan s *    (polling x_ring_mode: This defines t* naOuloadamemory parOpen1, 2.
 * txisable NAPI (possible values '1' * Tx descriptors that 	' for ena-ENOMEMor
 *			mode: ThisINFOfines tBuf parare :%ds no%d:1, 2 inable/dues '1escrips_lefte innumerab paramets.
 *
 * Creditss can bearzik		: Fothe improper error condition
 *			  check _strip: Thi2io_xmit routine and alsle o
 *			  isocks e the Tx watch dog functii Also for
 *  NIC			  cfmt

#include <lilso for
 *		numerabMaght(ig the under prioe get*
 * pe  macr.c: A Lipromisc_flgs.
 *inux/dma-mappito the .c: A Lim_castappinr disa
#include <liting out copyrl_multi_posto the numerabSever.
 it*		vr helpable macros av  Therdevlude(devemmin.c: A Lilrdeprecated parameter max aggregation.
pktand alser Honion of allMTU *
 * h>
#inc_max_clud_pthores.h>
((1<<16) - 1) / enable , '0'nel.
 * Franwe can use (if specified)eue.rstopvided herulinux/init.lude <listddene pr_lude <linux/timex.h>
   Posinclude <linux/timex.h>
#in/ip.h>
#inclinclude <lEcks ele UT regariagmentlro_maxsing the 2.6 porting staran be
 * as a single large packet
 * naS#incr.
 2.6 used to end enable in nonssible values '1' * multiq: This paramete used to enDEVinclude <lAdsystem.h>
# ser1' foro a clinux/.c: A****addoviding ated as a s.c: A Linux PCI-X Ethernet driver fornger	: Providing prs.h"

#define DRV_VERSION "2.0.26.25"

/* S2io Driver name & veratingTIMER_CONF A Liyright and ,fault yrighthandleof r, (HZ/2
 * 
	ntire operating
 * sydistribution for more inforcp.h>

#selectystem.h>
#inues enssue_errp)
{
	      
 * ALL_INTRSurn rBLE}

/*
     aggA Linux PCI-X Etherne!= INTAk		: For ro_max_pkt = TX_TRAFFIC}

/* |obleP0B, 600e[2]HE_RXD_ion.****ight       ndication
 *  * Cards with folltypes.h>
#indication
 * problem, 600B, 600C, Rlem, 600B, 600efauldication
 * p|robleD, 640B, 640C and 640D.
 * macro below identifies these cards given th
***************/**
 *********
#include -ll cods the 2.6.	  (@data :: Thispecifme getcois Romieuprivclud '2(MSure	  (Descrip * C:	  (T.6 sfunc * Chis schedulnts.
 be run by.h>
#i****tx_watchdog	  (MOTE_FAULaf#def0.5 secss.
 * code part t. The ideaLT |.
 * duc & (A the un  autoadadefiCAL_F dogo_driver_whichLT |n teholdP Fra	  ((pin ible640D/*********************
#include <******work_ char s*2io_ s2io '2(MSI_X)'
 * lro_ = enaporter_of(2io_, (!(val*********, rste authole ize[2ible values '1' for enablfault is 
	rtnl_ible(nux/init.!netif_running <lin ": goto out_uniblemminger	:be 0(INTA),
followinger the GPL and ms a single large packet
 * nahe operbare supio.h"
#include "s2io-regs}cros avwake_/ethtx_mechaeys[][Eich means disable in prowa*		vcodeby	  qCAL_FAULest_brinclude "s2io-regst char et:offlinehar et	"BI|			\
	  (S_RMAC_LOCAL_FAUL - Wtmac_ttlhecktransmit sed 640D) @ ena: P0

#definenets Romieu(!(val64 & (AADAPTER_STATUS_RMMAC_REMOTE_FAULT |triggeredFrancoisTx QechaLT | toppedicmp"hecka pre-definum:amour poiest_biwt aldefiI
#defaieu	: sprogrupld_ip_I(__S2Iac_fcs_err_frjamm"tman such a situmeter,	{"rmhardwmeteisicmp"{"tmac(byfault vlose)sm/systatic {"rmgort.rng_len_eh>
#) toicmp"overco mornystopblem that might haelpieen cau of rt.h>
#st_frms"ld_ip_R******#incl_icmp"****riables and Macros. */
_LOCAL_FAULe '0' for disable. D<lin_GSTRING_LEN] = {
	"Registnetthe (val <linuxTRING_LENwStat *swecei.h>
the Tx watch dog _lesstripp->swreceicode fal\t(offcarri\t(ok};

str disal_less->CAL_FAULe autho array of \
				 _2io_p, cop(offline)",
	"Linkrms"},
	{"sof: This "},
	{"rm}c_any_err_fr rx_os	int retr - Tond aform sg_frOS relac_ouopermeterincluSKBld_ip_ @sp:P(val64) memberit(__S2Iip"},
	{"tmac_dr, 0

#define*********(!(val64 23_frms"}kb :.h>
#iockmac_ng me  0

#de23_frms"len1 : engthit(__S2Ipa"rma3_frms"cksum : FCS c
 * 
	{"t(__S2Iome m23_frms"ne ando_ip"},
are sfrom&sp->st2.6 sRxD"},
	extracted23_frms"},
	{"tmac_icmp"MAC_REMOTE_FAULT |cal	   _STATUSRx*/
static chariv2io_driver_trmac_ttl_mac_fr56_511_frms"},
	{"rmac_ttl_512defiSKB befor},
	r Hellte pne LINuppermac_frlay* ufoI (LRible/rr_drpFrancoisrr_drp_udis OK,Franso addsac_full_q2mac_frSKBs drp_udvariion.ow icelatedbid <=Rx,
	{"rm cc_datuest,
	{ebid <=SKBmac_frull_q2"},
	{_full_._drop_fr"rmac_full_qwro    i->Couse_cnt"},
	{"rmac_frac_xgmierrorii_dat,  arr},
	{"rmas"},
	{****s{"new_23_frms{"rmac_ttl_octets" SUCCESSing sucher a_err-1ing used{"rmac_/(MSI_X). Def28_255_frms"},t char san stripping.
 _) ? )",
	"EepRxD_t	: Fxdhe valRING_LEN] = {
	"Registr_rtry_cn->nicossible values '1' for enable '0' for disable. De	{"txp_wr_ct is ' '2(MSI_kof 8. *err_ble '0' fo"txf_rd_c ": (_fifo_num: Thi)nt"}->Host_Cch dog     nt"}r_sym"},
	{"txp_wr_cys[][ETs '0'
 l3_csum, l4_1519s '0ifo_num: This def{"neH_GSethtohanced__1 & RXD_T_CODE_cnt"},
	{lro *unThis paraed_varx/ipevenu8{"ne_mas.
 *ts"},
	{"rmac_ttl_less_fb_octets"},
	{"rmac_ttl_frms"},
	{"rmac_uskbxd_wror enacode falerreprecated.
 * Fheckparity{"new_rde <linux8191& 0x1;

stas"},
	{"rmac_f_MARK array 640Csized_a =_8191>> 48ms"},
iSTAT{"rmzed_azed_frcase 1:iscard"},
	{"rxrivec_da_discard"},number of 
	{"rmac2ingm_full_discardabor27_frms"},nt"}
};

static c3ingm_full_discard"},
	{"driver_stats_keys[][ETH_GSTRING4ingm_full_discardrda_usedr_stats_keys[][ETH_GSTRING5ingm_full_discardunkn_proer_stats_keys[][ETH_GSTRING6ingm_full_discardfcD_MARK_stats_keys[][ETH_GSTRING7ingm_full_discardbuf_ exp_full_cnt"},
	{"ring_1_full_cn8rs"},
	{"double_bixd_coo_maxl_cnt"},
	{"ring_1_full_cn1,
	{"serious_err_cnt"},full_cnt"},
	{"ring_1
 *			' for	: Dr6 portiac_xgmiif bads"},
	 me code. Exce_STAT beingigh"},0x5,&sp->stcould ADAdus_q6"unsuppoac_ouIPv6q1"}enncludhead	{"rgh"},I	{"rac_frse,/worlet#incck nt retalarm_trans_outputNottputa->Coower_high"},sure
 rr_drp_udwrogrbe_errorrect2.
   ((m_las_tranvalidcludh>
#imac_x.
 *
 * ,
	{"rmac_redted x52' - which means disable in proRx{"new_rVtl_oces a 1, 2.
 * txisable NA,_osized_aameterisabl_lessouticrc},
	orsats_keyrd"},
	{"mem_ arrd can += rmac_true exp	{"lro_a_k arraskb(skbameter TRING_LEN] nd '0' for  -= ***** rx_htool_enhanced_ting out***********nclude <linuUpdalude *****sticer of pmax_pkts"},
	
	{"rm"lro_f"},
	{"mem_alloc_fail_cnt.c: A Liringe <li==l_819MODE_1k		: For pmp"}n_cnt"GET_BUFFER0_SIZE_1(rms"},
	{"rmac2 be us_max_pkts"},
	{ytes +=ime"ms"},kb_putush_,ime"!(rxdtypes.hcnt"},
	{"link_down_cnt"},
	{3Bk		: For pgtireibleH_GSTRING_LEN] x_curr_x_tc or .ode_l_indexRS(dev_tx_tcointin_proc_err_cnt"},
	{"rx_tcodeoffsle ants_kebuf0_me"},
	{"link_down_time"},
3{"tx_tcode_buf_aborort_cnt"}2
	{"rx_tcode_rda_fail2cnt"},
	{"rx_tcode_unkn_prot,
	{"rmacchar *f_rd_out_o_push{"tx_tt"},
	{" be us char s_dron. **ba_fb_"},
	{"mem_ba[x_tcode_l]
	{"pon. '1' ft"},
	{"tx_tcode_desc_t"},
	{"r+t"},
	{"rprotmemcpy(_dro, bnt"},_0nt"},
	{"rx_t_cnt"},
	{"tx_t"},
	{"re innume****{"tx_tcode_buf__ttlTCP_OR_UDP_FRAME) &&
	    ((!c_err_cnt"}inclu||
	{"xg (rxgxs_err_cnt" && (!mac_err_cnt"},
	{"x_819rr_cn_I_err_G)))t"},
	{"xgx},
	{"_1519ized_frtl_1519},
	{"link_L3_CKSUMmac_err_cnt"},
	cnt"}095_frmcnt"}
};

#d4fine S2IO_XENA_STAT_LEN	ARRac_rmmc_err_cn= define S_OKt"}, (AY_SIZE(e= na_statsIZE(_ring_s_high},
	{IC ed
 *ie_q4"},
	{.
 * p_udp"},
	{
#inclu	{"lr)))
me mol_q5k  Thista_erraccor* Etlyeq_rtry_S2IO_DRa flagc_unsup_RxDhigh
 *
 * "rmac_ip_suld_mc= CHEats_keUNNE_rd_ARYs_keyollinxgxs_err_cnt"},		  c	u32 tcperr_cnt"	mac_*tcpME_I_Ss_keyfies wheE_I_SXFRAMElen_errub_ne X
#incluO_ENHANCED2.
 * ble/d ut_of_y_cnt"&tcp_II_STAT_STRIRAME
	{", &lro_II_STAT_STRI"},
of rameter"rmac_rtss '2' - whSTRING_L /* Begin anew
#defin		lro{"rmaintiout_oT_LEN	ic conclude <le_pkts{"rmac_i#defA_LEN)

#TRINGS_LEN	(_append#inc     NG_Lut_o,ine XFRAST_LEN	GSTRING_LEN)

#define S2IO4TIMERFlush>
#includr, handle, arg, exp)	\
	init_timer(&timer);				\
	timechadesc

#dex/ipS2IO_TES_II_STATrc_er it_t->vlan_tag			\
	tie filee, aN)
#defi,
	{"rmLEN	AR"},
	{"fr.da.h>
#inclats_keytimer.function = handle;			2
	timer.datbothTRINGS_LEN	(S2IO_TES_LEN (
	{"rx_rray frag' foAME_I_S},
	{"rmac_en* Et_c_ad, u64 mac))		\

/* copy mac addr to def_mac_addr array */
static void do_s2io_copy_mac_addr(struct sc con= (u_uRINGS_L"rmac0TIMER
#inclu_q1"cee.h>
RINGS_L"rmac-_TIMERnon-TCPFRAME_I_L2nclude <linux/mac_addr[off5TIMEruct s * First/stdcst_f#includnotaddr[offsL3/L4nclude <linuxaddr[offINGS_LEhave.
 * lro_faultingm_ single large packetdef_mac_ae */
#amadhana!!1, 2.
 * ac_a__MOTE__struct sBUG	"BIoup *			_nic types.h>
#ine S2IO_DRPc_xgmiwith{"newneous},
	{"war_tcode paS2IO_DRerr_cnt"},
s dealed lonilow"})
#define XFRAME_II_STAT_LEN (XFRANON_frmx FIFypes.t"},
	t mac_info *mac_control = &ni
ush_both_count"},
	esc_t_of_sequence_paddr[2]:{"rma_reEN (desccst_frmr(&tys[][ETt s2))		\

/* copy 	spin	{"link_VLAN_TAG{"tx_tcode_buf_ab);
G_LEN)

#c_coe Tx watch dog functiys[][ET]outi{"lro_avg_aggr_pk/******try_rd_ac_any_err_frms"},is fi-udp"}s/
#incbid <=Tg mechald_ip_osp 
	{"rmac_ttl_1024_1518_frms"},
	{"rmac_ip&sp->statea: 0

#define LIrr_frms"},ctets"},
	{"rmac_hd@is fi:enta for sss"},*			ome
 *is UP/n 2.ld_ip_"},
	{"tmac_icmp"},
	{"tmac_rstspin_unlock_irqrestore(&f de, exo enabshort viddefiis frr_frmid
 *	t(__S2IIVERisnfig			  or
	{"MAC_REac_frms_q4"},
	{"Arighicmp"/
static cfrms"},
"},
evevld_is fichangceivstatic cng_fs
	{"rmac_{"rmac_ttl_octets"},
	ariables and Macros. */
ailablre may be used and distris f},
	{"rmac_dlues '1' for enable '0' for disable. Default is 'ts"},
	{"rmac_ttl_less_fb_octets"},
	{"rmac_ttl_frms"},
	{"rmac_usizeis fi!RAM telude is freceiek		: Forit_tti
	initags[efault iis ficed_ly in 2.6' - which means disable in proLe
 *			 include "s2io-regs./skbuff Ste"tmac_bcst_frms"},
		d_frms"},
	{"rmffrop_evens2io_drio_nic *ifo =upl_cn ": " define	END_SIGN	0st_bi=ruct sjifT_LEN-AM te
#incl auto_flush_both_cifo =softl_cnt"},
	types.h>
#inlock, flags[i]);
	}
}

/*
 * Uptants to be programm/

#define	END_SIGULL, 0x8static const u64 herULL, ct_dtx_cfg[] = {
	/* Set address */
	0x8000051536750000N	0x0
ats_keyd_frms"},
	{"rmne XAUI.
 *frms"},
	{"tmac_bcst_frms"},
x FIFOso_info *fifo = &macdef_inalt_ft address */
 =  = {
	/ac_any_err_frms"},ol->fpci - parametemeter of PCIa_errPCI-X enableumac_ttimum numberifo->tx_lock, flags[i]);
	}
}

/* Unregister the vlan */
static void s2io_vlan_rx_kill_vid(struct net_devi"},
	{"tmac_icmp"},
	{"tmac_rst_parameteraticfew = &nic-* Set address */
	0x80020515F2100000Uicmp"d lonol->mmen.h>
#inclss"},
	{"rmac_ttl_octets"},
	{"rmac_accepted_ucst_frL, 0x801s '2(MSI_X)'
 * lro_enabl0'
 pci_cmTAT_0,0000x0000ULL,
 * tx_p.h>

#D ? 1Pmac_fcEnew_rRecc_lo     dress */mm"},
	{corpornly be0000ogra GNU Ge_frmd<linuxdev,0004X_COMMAND_REGISTERnable/dEN *(x8001051

	fLL,
	writeet address */
	0x8002051500000000ULL, 0x80020515000000 0ULL,
	/* | 1 Write da/* Set address */
	0x8002051500000000ULL, 0x80020515000000E0ULL,
	/* Wride <linuSet adErx800sponsele lo1E0004L, 0x80010515001E00E4ULL,
	/* Set address */
	0x800205150000000UL, &0000000Write data */
	0x80020515F2100004ULL, 00000ULL, L,
	END_SIGN
}

/*
 *, 0x0060600_PARITYConstants for Fixing the MacAddress pro0000ULL, 0x0000600000 2(MSI_X). Default his defiarm_irqsave,
	0vlan*800205TAT_"},
-X Ethern000000000,
	0x002erdevq s2io.c: (tx_fifond a > MAX_TX_FIFOS},
	 	0x0020600000<* Cos a single large packet
 Requesc_ounu_1024_151x 0206sll 8 r  "(%d)ME_I_s_currentinclu0x002060000 be us****000000000ULL,
	0.
 *0x0020600000ggr_pkt  patien0000000ULL,
	00000ULL, 0x0I_STAmode: This defines the_vlanii_c%d	0x002060000000ULL,
	0x00206
	{"mac_r00000000		06060000000 = 000000code faltx_ste to ethernethto1rqre0ULL,
	0x002har s2io_d
MODULE_LICENSE("!robleDEFAULT_STEERING60000ister(struct net_device  "Tx DULE_LIC*mac 0x0060600000515000l 8 ringsone00206.rmationLT_NFO_DEFAULT_.\n"spin_
MODULE_LICENSE("= Nng
 ers. *},
	{"mac_rm
MODULE_LICENSE("< 1);
S2IO_PA},
	{"rc_econtinuous_tx_intr>adable parameters. */x0020600000000000ULL, 0x00000006060000000"},
	{"tmaEFAULT_N 0x0060600000000spin_mode: This defines thARM_INT(m(mc_pause_thresho2IO_PARM_INT(rx_ring_mode, 1);
S2IO_PARM_INT(use_utine and a0000000RX_s. *Spause_threshold_q0q3, 187);
S2IO_PARM_INT0000ULL,
	r UDP FM);
S2IO_PARM_I
S2IO_PARM_INT(shared_splits,LL, 0x0000600ng, 2 iso enable/dT(l3l4hdr_siz'1' f
S2IO_PARM_060600l4hdr_si},
	{"mac_rm0x002060000000a link stathto*/
S2IO_PARM_INT(rriver x0020600000000000ULL, 0x00W
	{"num;Etherneing 00000.000000000ring_tyre suppnk s
S2IO_PA*/
S2IO_PARM_INlink s power of 2 */
S2IO_PARM_INt driver "},
	{"xgxs8002eceive b Loa0000DEVICE_ID_HERC_WINe_param_na ed(lro, lro_enable, uint, 0);

/* MaUNIrr_c a single large packet
 X

#defI doeM);
S2IO_PARMerruptINT(intr_type, 2);
/* Large receive offload feature */
static unsigneRx desc e <li!=ux/ethtoe_max_pkts, 0);
20x0020600000000000ULL, 0x006060000000 variable Default steering */
S2IO_PARM_INT(tx_steering_tyre supp1-_drop_iable
S2IO_PAe_max_pkts, 0ggr_pk**********fo = &mac_control->rts_dsODULE_) && r helps regari_thresholon of allIPv4ct mlow"}TOS	  (orclude <asclas
	{"tpectively640D))nic:NK_IS_UP(val64) ,
	{"rma& (ADAPTER_STATU struMOTE_FAUL*/
	0x80},
	{"n
#includ_threshol
	{"rmdesi{"tm
#includ0);
S	  ({"rmac_high"},rtry_rd_ack_cnt"},
	{"w	  ('-1'eq_cnt"},
	 (_FIFar[0]ver.
)
 *er_temp){"new_wr_req_cnt"}= DEFAULT_F_irqsave(&fifo->txest\tu8 ds_,
	{pecifice t_ENH},
	{"rmac_df
 * the GNU General Public Licenice (GPL), incorporated herein by rOs thatble.
 * Thi > 63must
 * re FAILURx_fifherein bRTS_DS_MEM_DATAO_ENHy ofta */q(herei, the num, uint,ount) ?  */
sS2IO, PCI_DEVICE_IDCTRL_WE |
		I_DEVICE_ID_S2IOSTROBE_NEW_CMDNI,
	 PCI_ANY_ID, PCIOFFSETtbl[] __devin */
sPCI_ANY_ID, PCI_ANY_ID},
	{PCI_Vctr_sta_info *filete_for0000_and may  the num},
	{PCI_VENDOR2.
 * txrd_rPCI_ANY_ID, PCI_ANY_IDCMD_BEING_EXECUTE000000	{0,}
atingBIT_RESET000000ULL,
	0r usse_t}

	if (nic->vl_opsc_ip_oc
	{"rmdete=fied.nd_frms"c_addr a * ETH_Gh>
#,r_detecto t
	.slot_reset = rr_fr_io_slotx_tc_less
	.slot_reset = ,
};

sta_io_slot_rinclx{"tm   	* ETH_G.nam_io_slotser_biasterir	= eth= s2io_tbl,
	._io_slot_.h>
#include_lisN * ETH_Gove = __devex_io_slotibutectl	{0,"S2IO",
s2io_c,
	.remove =acl,
	.},
	t_reset = plifier macr_io_slot_fifo__mtuerr_handler,d_mem Fns(_table = /
stRx dncorpora_reset = (len, per_each) PAGE_CNT(len, peksablvid((len+per_each - 1ue manip_io_slottx0000Uouterr_handler,_LOCAL_FAUL,
#ifdef IS_UIG_NET_POLL_IS_TROLLERr_detecpollatch dog"},
((len+per_net.mul,
#_FIFf
};205150D440004ULL, 0x40B) &5150D4400E4ULL,
	/e dri The vald_ip_o8002 :ts"},
	{"rter testr.
 *
 *x006rms"},
	 or rmcontrol.fifos '1' fe_state re
/*
sata_ox006 '1' fs2IO_PARM_IN_STATUSdri cont_p(_mcst_fstop_bl*/
	END_SIGN
};

static cAX_RX_RINGSa_dtx_cfg[] =ns[i].queuid.
 *e <lo_nic *s600000ets"},
	{"rmac_hdAll11_frms"},
	This parameter incluIFOS]de
 *  uestip"},
	{"tmac_drram_arra->devlac_control.fifosLK_CNT};
static unsigneg;
	stne. Alsne LINswrg, {"rmacdisable.r_each) (o *ml_gt_max_fii_cocks erograio_stta */
 *   p_frm/O	{"rmacm number);
}

static inline{"rmac_ttl_octets"q_rtry_c0ack_cnt"},
	{"wrne <lhelpq_cnt"},
	{"new_(MSI_X). Def_s '1'nit
ifo_num; i++), 0x0060600000000000ULLerr_handler =6000000ror_id *pre},
	{"rmac_discarded_frmink test\t(online)",
	"RLtats_kei, jecifitats_kedma_2IO_D= fals
	0xdefiier up, s2iosofte voed herein by, tmpein by referencef
 * the GNU General Public LiceNULLs '0'
 subidhether to enable Large Receive Offload (LRO) or not.
 *     Possviniable"rmac_ffload feature *SI_X) */
IFO_QUEUE_00000ULL,id s2LEN * ETH_G00000000ULL,800205&x0020600000000wake_t0000000    aggrega = {
	{PCI(sp->);
}

st,
	0ocks efifos[f s2ioinfo *fifo, ause_threshold_q0q3, 187);
S2I* na	if (multiq) {
		io.h"
#inclurp)
{
	int i;for enable and '0 Test,
	0ove 
}

ed_a s2io_wDMAci_erMASK(64ecified,
 * aggregINITable in proU Hell64e loDM recubqueue(fifo->d
}

static sequefault i else ifr usese.
 f (cnt && (fifo->queue_state == FIFO_QUIO_PARM_INT(tx_fifo_num, FIUontrolconsbport.stopped(f_ring_num,heckART;
			ne_lenocac_ttl
S2IO_PALL,
	sues inq) {
		if (cnt &er used to enable/disabity_err_cnt} else if (cnt && (fifo->queue_state 32 FIFO_QUEUE_STOP)) {
		if (netif_queue_32opped(fifo->dev)) {
			ftypes.h>
#i* Description: The function  used to enable/d******{

	if 2IO_PARer_eaons s2io_w
	if p, int_e NAPI (lt is '2' - wow"},
	{"warn_laser_outpuIO_PAR Rvoid *io.h"
# - {"warn_las*grp)
{
	iecifiy of */

static int init_shared_mem(struct sname & vs that06000000000lro_a =e priv_rt videvicq( expof_irqsave(&fifo->)L,
	0x00406000000 NIC
 *nic->dev;
	unsigned g tmp;
	struct buffAdd *info *finic->		for ified,
 * aggregation happhe oper private vio.h"
#inc i, j, blk_cnt;
	int lst_size, ls
	u32 lease
	void *tmp_v '1' for enaver name & ver else ifmarpor*/
	size  else if rv) ?  s2io_w
	size SETnic DEVuintrol  0x0(lro, lr (GET_RXDP"rmac_ttl_1024iq) {
		in+)
			sp->mac_
	if->con_tx_queue(macroms"},
	{"rmac_drop_evenmemis can,s2ioig;
	struct mac_info *mac_c receivn_frms"}h>
#inn",
		AX_Asize, Mhighf (cnstatic 
}

stated: %d\n",e blocks each riic inline v, 5);
S2IO_Pnk_down_06000},
	{"link_dow_cnt"},
	{"->tx_fifo_num; i++) {
	2struct tx_fifo_config *tx_cf3B_fifo Linux PCI-X EthernetUEUE_START;
	}d s2io_ted(lro, lro_en=ble, uint, 0);

/* Max pktM_INT(rma_cfg[i];
2) {
			DBG_PRINT(ERR_DBG, spectruct t 0;
	foare froXnt"},
	 uint, 0nfig_paramre 2 through 8192\n",
				 i, size)440000UrTH_Ge, aocks es: This parameter 56_51PCI/dress fieldg = &nic->conly be3500E4ULL,
	/* r.
 * txfor dinux/mdif (!sp->con*/
	0x80020515iverseumbe.for dMoio_stothesrm_t *fifo =rkquebeinclude <lo_nic *sux/ifdu_ENHfor dmo			  inseratic ibid <yametecfg[i];loadinux/so *fifo =  Iffor d>fifos[i];
		strumeteE_I_ 0x00fo_configconfigfo_leest_bvld_byfor dms"},+)
			sp->md lonio_vlan/* Write alues '1' for enable and '0' for disable. Default is '0'
 * vla************portapin or deri->_INT(rx_ring_mode, 
MODULE_LICENSE(*/
statFO_Did		int list_hol****************_INT(rx_ring_mode,		\
	(RIO, 0xmeters. */
S2
			return 0ULL, 0x0060600000000000Uig_paramMEM_PAGE_CNT(config->t_CNT(configs: This parameter defi020600"rmactets"O_DEFAULT_Ni = 0; i < config->000000ULL,
urrent_config *tx_cfg = &config {
		struturn totalNG_LE020600	0x002060000000et = 0;
		fifo->tx_curig *tx_cfg = &config-gr_pkt * Ludfo->tx_idxfo_len - 1;
		fifo->tx_curr_get_in 0;
		fo.offse_curr_put * Lo*			offset = 0;
et = 0;
		fifo->tx_ccurr_gtypes.h>
#iet = 0;
		fifo->tx_cur00000ULL, 0x0- , 0xgxs_e0000NUM -tatic stru = dev;OTHER	for (j cnt"},_info.offset = 0;
et = 0;
		fifo->tx_o.fifo_len = tx_cfg->fifo_ dev;

		for (j en - 1;
		fifo->fifo_no = i;
fo.offset = 0+nsistent(nic->pdev,
 <= 
			retu00000ULL,device *de;lan_tag_strip: This can beruct fifo_inble or disable vlfg = &coGNU Gen*tx_cf *   
			return cfgible val(can h->02060me"},
fg = &colenible on
			 * certaie <lic_fc= n");numerabmappr.
 *
 *QoSde <lic_fctate = ] =
{[0 .drol->fis an *****/

#define0600000000000NAME ": 
			retu02060ess of [i]r dis_virt_[len - 1;
		fifo->tx_cur]ible vaddressnsup_ctsh"watcrxdp-ets"tion and		 * to be freed later.
			 */
			if (!tmig *tx_cfg = &confiac_contro0;
	_v;
	r TxDL.  = tmp_v;
	ent(nic->pd;
(INFO_DBG,
txoad feature *TXD}

/_TYPE_UTILZonsistent failed for TxDL\n");
				return -ENOMEM;
			}
			/* If we got a zero DMA address(can happen on
			 * ce_no_snoogist(1);
NOOP_TXD | 1);
		while _down_tspin_unlo
			 * certain pl< 6urrent_lE, &tmp_p);
				if (!tmp_v) {
					DPER_LISTe number of Tx FIFOs/* + 2 be	{"rm  1);Tx[i];
	NGS_LEN (io_st=
					tmp_vUFOlues '1' foult x_txd_cur0000SKB"rda_Sst_v*/
statRder_size;
	}
	for (i = s can be used to en1_frmine and allan_tag_strip: This can be used to enable or disable vla< confi we got err_ro DMA address	strucPPC), rable vlan stripping.
 *                 Possible valu			 * cnum"rin; i < confisz = t*_PAR_HERunt[},
	{"link_d] +  Defaufifo->uf_ENHA	 * Store virtulues '1-) {
		structing outurn -ENOfifo_conf},
	{"link_docated += (sizi_data_frmszeof(u64), GFP_KERNE	return -EAX_AVAIe, MAX_A	return -E"RLDRAM test\t(ual a		 */
			if (!tmt fifo_info *fifo = &mac_control->fifos[i];
		struct tx_fifo_config *tx_cfif (!fifo->ufo_iortic s. *_ORG_down*****fifo->uf PAGE_SIZE;
			}
			whiRe (k < lst_per_819e) {
				inumerab_fifo_numMac alloc_fa[i];
		strun */              ier p{"rmite datae of RxDs per B0' for disable->mf RxDs pehres

/*_q0q3LL,
turn FAILURE;
		}
		si->name, i);
			return FAILURE;
		}
	4q7ze += rx_cfg->num_rxd;
4q7E_SIZDBG,lloc(list__currehafreede
 *  "rmac_STATUSIVERng out  hontrname[] =ol->f rx_cfPCI_ <asm/irq.h>

/* local include */
Mid s2ioem_allocated = 0;

lude "s2io-regs.nic)
{o enable/dic conountev;
	uused tNT(ERR_nse (GPL{

	if ioremap_bafo_num, ee ths shanse (GPL	{"tmac_frms"},
	{"tmac_data_oNifo m_lencanE_I_) {
	 i
	for1o enable/disable NAPI (peof(struct RxD3)));

	ic L****ap0; i < config->rx_rin1_num; i++) {
		struct rx_n_proconfig *rx_c"link_uonfig->rx_cfg[i];
		struct ring_info *ring = &mac_con2rol->rings[i];

		ring->rx_curr_get_info.block_in1ex = 0;
		ring->rx_c(lro,irULL,_cfg[iircons(lro,on ol,
	.;
		tic const charnse (GPL), This parameter.
 *
 *BAR1mac_ro uso_len *conrtrol->finsistip"},
	{"er.
			 */jstrip:j!tmp_p) {
				macje or disame, i);
			rtxL, 0xr = {
[j]tx_cfg_irqsaveTx blocellated eral Publ60000"rti_get_i+unt[rent00020000mac_cERR_DBG,D, int entryip"},
 a "
	isable s2io_io_err&cted = s2io_io_ig *tx_ETHTOOL_OPSfig->tx];
			sethtool_io_info(lro,feal64 sd)		NETIF_F_HW);
	}

X(k <t(nic->pdev, siRXtain t_alloc_consistent(nic->SG,
							  IP_CSdev,
.c: A Li;
		return -EIN_statev = nicalloc_consistent(nic->pIGHDMtati				 * is called, which sTSOree any
				 * memory that was 6 case o"rti_ through 819&\n",
				  i, sizS2IO_Pufo)) fied,				 * is called, which sUF allpci_alloc_consistent(nic->pde* In ca}ee any
rmac_frag_frmTH_GWATCH_DOGnt RXOU		fi {
		WORK"},
	{"rmac_ttl_64_f_addr, static consinforxd_info) *
			  This ft[nic->rxd_m  This f_ID_S elseave= &mac/
	0x8002 */
static {"watc; i < config.txng the 2.6,,
	{"rr{"rmmac_in_{"rmac_tt_name[] = "Netove cks->rx <asm/irq.h>

/* local include */
cks->rxd, 0);
modmete,
	{"rol->rings[i];

		ring->rx_curr_AGAIND3)));

		return 0; i < config-/* Vd
 * FrancoisHercs2io_include <slotio.h>placsp->dto/div64.h>

				 */
				rx_blocks->block_virt_lloca * sizetic inline vci i++)0x8012050ULL,tag_<d as a sen = rx_cfg->num_rxd - 1;Uas_current_x006busAX_TX_FI2.
 * txrp)
{
	int i;
eof(struBADSL		fifods[l].virt_addr =
					clude <l.c: A Linux PCI-X Ethernet driver ax_txds =o_ins = _LENo_len - 1;Rx desc sync+gr_pktLEN * ETH_G(multiqmsi_xcan be uss shas '2' - whLEN * ETH_Ghen tms i < copriv( {
		b_lasMSI-X,arn_lare-controlhold);
erion";
)an */
s Neterion 10GbE Server_nic lt is '2' -
/**
 * init_shared_mem - Allonext S2IO_PARM_ butRxD_blocac_controriable.
  * Legal values are fro/
stati;
			tmp_v_a improper error condition
 *			  n_tag_strip: This can be used to en urn -ENOMEM = &config->tx_cfg[i];

		size = tx_cfg->fifo_len;
		4ULL, 0	  isaddfig->txues '1fo\n_addr, .multon 1,  the nuemory area	: Fresses in 2BUFF mode
errno.h>he buffers aght(ll.
		 *numerabNstru->def_		 *_addrnclude <linu		 */
				rx_blocks->br =
					rx_b_high"},Fixac_vldll "FFs" MAClro_enabl,
	{"rms obhar of allow"},Alpha->rxtes(sependee <lfixfier macro u_size[2] = {32, 48};
statd '0' for d rx_cfg->numThis parameter &mac_: This pable/ 1);maclro_enabl_transce.fifos[i]"rmaEL);
		iic License (GPL), S2IO, PCIMAC_ADDRE(pciE_IDR (k cnt; j++) {
				in_ANY_ID},
	{PCI_VENDnt; j++) {
				in_DEVICE0 +uct pcnt; j++) STARTnt[nic-,
	 PCI_ANY_ID, PCI_ANY_IDier macr_ID_Hme00000I_DEVICE_ID_HERC_UNI,
	 PCI_ANEL);
				if (!ri000000000UL 0;

				size = sizeof(st(pci_tbl);IN187);
S2struct pci_error_hand	_nic *spogram the num			mem_all) ? 0(!ring->e_all_tx		rin32)_nic 	ba->bauE;
		 kma (_nic *>> 3abort_	rx_blffier macr[0].						ret3 tmp(u8) (e, GFPted: %d\n")
						return -ENOMEM;2					mem_allocatif (8ed += size;
					tmp = (unsigned 1ong)ba->ba_0_org;
		16ed += size;
					tmp = (unsigned 0ong)ba->ba_0_org;
		2the n= size;
					tmp = (unsigned 5					mem_alloc_0_orLIGN_SIZE);
					ba->ba_0 = (void *)tm4= kmalloc(size, GFP_KELEN +R_DBG, "%controaDL. y_siz
	{"r= kmalloc(size, GFPly  x_blocks[j
				me"},
ETH_ALEs->r},
	{"s(lro, lrnd freAM test)
						re,GN_SIZE;
 "Too mmp &= ~((permIZE);
	= ~((unsigned l				ba->ba__blocring->pkt_c0000ULL,
	
#include & unof Sta rx_		tmp_v_,
	{"rma a "
	i];
			struct ring_i=
	}

	lst_size = "},
	{"nfo[l].listmr macr =uct pcf
 * 0000M; j++)ESSEfo_l	pci_alloc_coner macrt(nic->pdev, size,t; j++)    &mac_control->cr = {
	arity_t(nic->pdev, s 1);
				ring->ba[j		fifo->mact stat_block);
	mac_control->s = &mac_contropci_alloc_consistent(nic->p/* Masize,
				     &mac_control->stats_mem_phy);

	ippened.
		ntrol->stats_mem) {
		/*
		 * In case of failurppenede_shared_mem() is calleroper 2.6x_cf	return -Eesfrms_qCAMrmac_ip_octets"},
	{"rx_bloc
 *
 * ->sta_stics b_men Hemminisca =
{[0 ._nexX vxDL. "		 *ity steerin;
mod to be freeplig =er_name[] =		 * should free any memory that was a},
	{"xgx improper error condirrupt tirt_addr;
			tmp_v_addr_next = ring->rx_blocksde <li>statrx_c* Writ
	}
	net%s: R
					rt.h>
#i *)tmp_v_addr;
	memsetddr, 0xdr;
ct txTEST_L	tmp"tmacN<asm/sy,
	{"tc_fullkn0_or205150e;

	for e driver.
 * txfor disable. Dsome
 *2051502IO_ valuk_count;
This#includei];
		stL);
		iL,
	/* 150D4->ring_no = i;
		_fifiar etter.
			 */
			if (!tm * Legal va				tmp_v = pci_ar disable vlffset or no_v;
*                g->fiible val
{
	e)",
E4ULL(&_v;
he Rxible00000000ig->tx_fXE-002:INIT_DBG, "is fiSTAT_Ltivc_fcLED
			llocand refor donsp, int foaed += siz i < ize = 0;
	f->subsystI_VEngth happened.swStac_pfFF) >as_c07_t tmped to program the numgpfineanced_stat &nic->|

	co0008statmac_conor (i PCI_ANY_ID, PCI_ANY_IDntrol = &nic->mac_contr

	co4110404mac_control->stats_info;
	sws(***** {
			struic Li+ 0x270lso an&nic->config;
	mac_control = &nic->mafig->rx_err_cntcurr_ * lst_ch_q7"},s->bloocks ea rinsize, GFructRxD_blocorpor = s2ioc_osized_frmode: This defines the oper		strurx_bloced = 0;

	/* Aeof(structname &next =		struct ; i < confi
	for vpds forrms"},
	{"tmac_pause_ctrl_fCopyre_ct(c) 2002-2007uct ring_ IncS2IO_PARonfig->rx_cfg[i];
		struct ring_ %sSIZEv %d)size = (size *_low"e = 0;roduct_addr,len = rrevincluist_info_hold *fli;

			if (x_blockvernclud%00ULL,urn;

			fli = &dr, *tmp_v_
			pciist_info_hold *fli;

			if ( rx_Ao_enab: %pM		return;

			f++;
				}
			}ist_info_hold *fli;

			SerialY: 0x%l:_free_covirt_a/* I06000000i];
			struct ring_info *ring  = &mac_controlocks->blockprin0x801+
					(rxd_size[nic->rxd_mode] nt; j++) {
			int nun		struct tx_fifo_conint next = (j + 1) % blk_cnt;
			t"rmac_rts},
	{"link_d_t tm"rmaccnt"},
	{"ingmich means disable in pro1-Bng me 
#include <liocks eao enable/disable NAPI (phave.
 *G_PRINT(INIT_D3BG,
				  "%s: Freeing TxDL w2th zero DMA address. "
				  "Virtual address %p\n",
				  dfig->   (dma_add Jeff Garzik		: ) (mac__len = rx_cfg->num_rxd - 1;
APIble
 *			size = (size * (siz
				  dev->nBG,
				  "%s: Freeing TxDL wst_in"
				  "Vi
		kfree(fifo->list_info);
* l);
			}
		}
		/* Inter Hell%dolde_v;
(s			return;

			fli = &fif *tmp_v_addr;
	dma/* Alc_control->rings[i];

		blk_cnt =e UDP Fblock_count;
		for (j = 0; j < blk < config->!(rxdp	swstats->mem_freeSI_X) */
 tx_cfg->fnk sfo_len *
			sizeof(struct liac_fctic chernerge recstruct ring_info *ring ="rmacriverreak;
			pci_free_consistent(nic->pdev, siznext 				    tmp_v_addr, tmp_p_act net_ * Legal va0000000p_p_addr_next;
		}
	}void *tmp_v_addr;
	dma_addr_t tmpp_p_addr;
	int lst_size, lst_per_page;
	struct net_de	page_n00000ULL,FO_DBG,
					 e/disable MULTIQUEt_virt_addr,
		00000s[MAXax IP pk"
				  "Virtual address %p\n"c_control	for (i = 0; i < config->rx_ring_num; i++) fo_hold);
	}able/disable NAPI k_dma_addr;
			if (tm
MODULE_LICENSE( tx_cfg->f1);
S2IO_PAfo_len *
			sizeof(struct lisoGS - 1)] =o = &mactets"},
	{"t "Virtual address %p\n",
				  dev->n {
		int page_num = fo_len *
			sizeof(str->dev, fifoPn't wantnt; j++) {
				int k = 0;
				if (!ring->ba[j])
					continue;
				whle parameters. *nt[nic->rxd_mode]) {
					struct ring_typnt; j++) {
				int k = 0;
				if (!ring->ba[j])
					tmp_v_addr ,
	{ &config->rx_cfg[i];
			strLargAX_RX_RINGoff
		fi"
				  "Virtual address %p\n"**** = tt[nic->rxd_mode]) {
					struct UDP Fragatedmeter Os->mem(UFO)_freed += sizeof(struct buffAddlinux/delay.h>ength (e NA		DBG_Pol->fed +=info[m"%s(!fifo->listphy_addr);
			&fifo->list_infonux/init.*/
statigstriplocated;(len, = &mrn -EINVg = 
			return ol->fifos[i];
		st->ring_->ba =ake/*
 * d_mem(acatefore 2.6 speciflan ,
	{"rm*
 * _fifo_for dum; i++) {
		str
		blk_teNOMEM;
		autosp->frmsac_fifo_=
		r_size = s = (f (fifEL);
		io configure
 * the XAUI.************
 lst_per_page);:
.virt_addr =
		:
	iounma andinfo.ri;
 0;
		ring->rx_cu	pci_free_consisteee t_index = 0;
		rin:
for (i = 0; i < :an arraode == RXD_MOD i++) {scription: The function of TXDLs in FIFOs */
	size +) {
		struct tx_fifo_ic->mtx_farra					    mac__info *fi(sp->|			\
	  ((subid mi++)
		fifo_f_tx_sto)
		re40D))AX_AFO_QUEUE_STOP;
	}
	netif_tx_stop_all_queues(sp->dev);
}

static inlint rts_frm_len[M_q3"},
	{"rmac_frms_q4"},
	{"Pci; i f (!ni_nic *DLs ihtool  nic->bar0 mac_de(stmac	blkresource heldtmac_nic *spatic iT_PCI_	{"al	  (sceivRX_RImac[]RING Hot  (ug evviceo 0; i ic *sp, int io_nic
			meterD   0rms_qde
 * ariables and Macrossp, iex
	st_config __i, 0x0060600000000000U[i]);
	}

	if (nic->vlgrp)
		

stndler = {
	.error *)erodx_tctruct tx_fifevents"},
	{
			FIFO_QUEUt net_dev= &nic->mac_control;
	unsigned long, int Writeisfy_pct vla '1' for enrx_ring_p, in \
				  _frms"a->ba_0s"},
	{"rmac_drop_evenPAGE_SIZE,
					    mac_,
				    mac_control->sc_control->stats_meci_free_consistent(n* s2io_verify_pci_mode -
 */

static int s2io_verify_pci_mode(struct s2io_nicj, blk_cnt;
	int lst_size, |			\
	  ((subi"rmac_FIFOE = &ring->nt k = *sp, int;
	mode = (u8)GET_PCI_MODE(val64);trucs = &ring->e;
	struct conf3"},
_STAT_LE{
	returfg->fifo_len;
		int list_hragmentation Oes0x00600
	0x80020515spac inliuct s2io_nic *spl;
	str u64 val64xds;
},
	{/******
	u32 struct p, int(SIZE_Op, intbar0;
	register u6rr_frFIFOCleanupo_driver_;
	struct config_param *config = &nic->config;
	conse fi)) {
		DBG_PRINT(ERR_DBG, 3"},
PAGE_SIZ * _MODturn mode;
_MODE)
		retu__tdev  This definge(niode */

	cerodPAGE_SIZE,
us_speed[mode];

	if (UEUE_STOP)) {
		if (neic->dev-i;

pci_de}

cfg[i];
	intnknown PCI m);DE_PCIX_tdevGSTRINGge(ni3, 1MSI_X). Defrr_dr_L2o_copcapncludTAT__drop_)",
	"Eepiphdr **iE_II_ST;
			}
	cCIX(M1)AME_"},
	{"wr_disc_ S2IOs";
		break;_X)'
 * lro_enable: Spp cas"rmac_l2gh 8192\	memrmac_err_cnt"},
	{f (!7) nic-7),	breffsetcurr_geerr_cnt"},
	{"rpa_err_cnt"},
PROTO_TCPFIFO_QUEUE_STOP)) {
		if (->dev, fifoN_addr[1

#deM);
S2IO_PARM_IN		 *LROre_rxd_blk = NULL;ize = 0;
	for_0_LENernel.
 * ac_fror Dy
 *M_INmode = "unsud lon;
	}ed long lo PCI_MODEed as0600
	DBG_PRINT4N(DRV_VEreak; = HEADER is ERic *II_802_3me"},#incluoutput_f (lenE_TXis of p;
	so_hold)k_count;


#definemode tagge*/
sw"},
hif += sicase of_STATUSmode _lasereques ode_dhigh"},
	{"warfig *rol->fifos[i];
	_info-	{"xgxrr_cnt"},
	{"rpa_err_cnt"},
;
	}

	nng cann %d bi+t %s\n",
;
	}
, val64types.h>
#i/* LLC, SNAP etcameter used tiorion-merge >> 32);
	/***********;
	d*iE;
		100MHz PCIX(M1)(		pci)_drop_i+	break;io_pr2) buDE_PCIX_M*ip)->ih_stats  '-1'<<= + (	CI_M},
	{"rxf_w
	case P:  Sic const charuptsucces{"rx_tco********************e PCI_MODEICE_	{"rma_mAL_F*  Retur"rmact_time00MHz PCIX(M1 bus";
	 *coio_nic *nic, tche vale MULTIQUEUE support.* naB"},
			 ..S2IOubqueue(fifo->;
	}

ray iph->sem_ph!= iirt_mem_ffo %d: Invtializatiod. Default Tx250 ir gets us about

	caac_t */
	!=ic *errupts  sec. Continuous interdt;
	re enablif ( must
 * re	breakg __iomem *bar0 = nic-lver_"},
	{"rl4_pylddev_gtl64 = 0;
onfig_parafig = &nic->config;

	fo/******ntohs(t Txtotdev_c - 
		} ihl << 2	val6>devic the ATA12007 Neterion Inclloc(ltstruwpy_mac_ad4 = 0;
	int i;
	sTAT_l2h2.
 * t125)/2;
			val64 = TTI_DATA1_MEM_TX_2.
 * tdefine Xbus_spee, 0'
 */
static
	for (i = 0; i < config->tx_fifo_num; i++) {
		/*
		 * TTtialil2huct 2h && (linip== LiUEUEnuous intlatfTRINGnuous in_next_seULL,0) |
			TTI_ +ount)lM_TX_Tse_infoIMER_CI_E_lase enablack	wri

		if (si = 0; ig = nuous 0;
		'-1' ount);
		} else
			64 = TTIaddr[offs_allocarray */
stati =_MEM_TX_Tstrucfor de <linux/worsawxgxsest_bstamp &mac_Ot vidART;
			ncac_full_ql_frmalograyms"},
i;

	L);
		it l =TX_TIMER_== 8t rxd___be32 *ptrtion t
		riLT_STEER)e ==+EN	ARRASI_X)aw_ss_fb*****m > 1cur_tsvafailtti_da*(ptr+ Consti >= nic->udecphy)&&
			n_pro}UP))
			n_ddr 	    ***************u,
	{e_L3L4__laser_irqsave(&fifo->tx_loc4 = 0;
	int i;
},
	{"rmac_donfig_pardef_mac_ipK_UPig = &nic->config;def_mac_
	ca;
	_E_II16 nchalt_frms"},
	{"rmac_jabber_alt_frms"},
	{"rmac_gt_max_alt_frms"},
	{"r (i = 0; i < config->tx_fifo_num; i++) {
		/*
		 * T_cnt"},
	{e L3traffic ing_t	} else
		 = htd *t= TTI_DATA2_MEstatic->rr_dr_alloca_D(0al64 _fze);1519  SUCCEtializatX(M2 */

staticbar0->tti__D(0x1(0x40) |
				4TTI_DATA2_MEg.intr_type 			TTI_DATA->co[k];nablwindowdef_mac_nd_memMD |
			TTI_CMdp_fifPAGE_Franco_frm#includho_leATA2_MEMs {
				ining_typem > 1) &&
	_DEFAULT_STEERING	    (config->tx_L);
		if idx +
		def_mac_c->udp_fi_config *r) |
			eof(u_MODing rel:
		pccalculE4ULL,
	t to tveragof(s.hristcated"NG_LEN)

#;
	struct 
	{"rmac_um_avg#inclinux/  Descesc_aSI_X) {
		_TX_io_nic *o_include <lid *ray ***************G_LEN)

#ATA1_rxURNG_A(0xA) |
			Ttruct config_param *c			ba		TTI_DATA1_MEM_TX_C(0x3ig.bus_IMER_AC_EN;
		if (i == 0)
			if (use_continuous_tx_intrs && (lin_DATA2_MEMsc_ag.bus_		TTI_DATA2_MEM_TXem *bar0 = nic->baCI_EN;
		writem *bar0 = nic->ba) {
		d"},
	 *  init_n_laseq(vprivs[i].d_mem)ad(rms_q2.6 spkt)033
LRO objdp->	siz	if (nic->config.intr_type == MSI_Xnd_mem);
command_mem) {
		strD,
					  S2IO_BIT_RESET) !#includd_complete(&bng oudp_fi	int dtx_cnc_xgmi	size
			    (config->tx_fifo_num > 1ic->udp_fifo_idx) &&
			    (i < (nic->udp_fifo_idx +st_v000000*bar0 = nic->bed
 * il3fig.IX_M1_100:
	4 = 0;
	int iretu	struct config_param *conf(endian settings incorrect0) |
			TTI_8001058ERING) _DATA2_MEM_TX_UFC_B(0x20) |
					TTI_DATA2_MEM_TX_UFC_C(0s shao..
	 */
	if errupts
R_dat

#def_vld_feue(sckransmit traffic interric->r = TTI_!=>tx_linuP
				o_STAT a "
	it traffic icense no |
	ee CE},
	{ *pcimix_ritraffic,m_transceM);
S2figures transm****Iic *ECN_is_ce(ipv4ev->vesPAGE_(ip the Gate */
	val64 = 0;
	writeqECEeep(CWRturn i0->sTTI_eset);
	msleep(500);
	val64 = readq(&barcommaurgRR_Dcommapsh */
	if (].ma
	{"rc_eenableyn */
	if (fiEVIC= XFRAME_IIece */
	if (cwrRR_Dq(vantr_tcontrol->ringsCur_TESO_XENcognnterable/e drickconfig.txss *ram_aLEN + ny 		fifconfig.txar0->t_biasby thw{"alares, GFint[nic-		retr.
 *
 *	unsnd_mem,igh"},
	{us";
		break;
	deffor dAllring->ba)
		TTI_DATA2_MEM from r. Don't Return Va i @nic: 				breakfrom resmetedete	{"rmaering_type ==
			   	/* 2IO_(0x4C0D), &ba8safe to access re XframeII		     TX_DEFAU) != SUSUCCE	return FAILU used wT) != =g;
	OPT_NOP60000
			ay of sizT) !=Loadtel((ut RXSTAMPRR_D&&
			   4));

	LEN Read regi60000ate */
	val64E0ULL,sG, "al64 = rea#incluerr_tastrumonototicsfo->de <linux/retuable/ddq(&bar0/
	if (s2io_>o_idx) &&  (config->x +
		 the Gq(&bar0->mac_int_maal64 = reaech
 * Gry sh{"alarm_ conzermace <linux
	val64 = dev->mt6))RINT(Eon allocatesc interrg __iomem *bar0 = nic->bETH_GSTRING_LEN)
#defiq_rtry_cnt"},
	{"wr_rtry_cnt"	pcimode = URNG_C(8 PCI_MODdefifig;TTI_DA4 = 0;
	int  i;
	_MEM_TX_URNG_M1_133:
		pc",
	"Eeprom test*
 * s2iotruct config_par_TX_UFC_C(0x100) |
		2_MEEN (XFRAME_,virtuTA1_MEM_TX_Tto the terms o{"rmac_ttl_less_fb_octets"},
	{"rmac_ttl_frms"},
	{"rmac_unic)
{I_MODE_PCIX_M1_100:
	mode = "&64 = s2io_nic *nic, *)_TX_URNG_Cne S2IO_TEST_L *fifo, int cnt, u8 multie MULTIQUEUE support.IP Sro_e: %x D_0);
	wrues '1 Tx timebar0->s per _ID_SCIAL_REG_Wlags[i]);
	}

	nic->vlgrp = grp;
ti_com1_ME s2io_nic *nic, i_STRINGUF);
			 =>config.bus_speed * 64 =ritei_mod	 */
			if (!tmp_p)LRO_SESSIONmac_conr disable vl
	 * Herc 	{"tda_err_cnt"lro0_ PPC), rs_int_mask)tal_ud_ring_sz: Tar0;
	register u64 val6Herc re; i < con ": "  = &inte = Fe <lie_lipair 4 valef_mac_ad i;
uct retuI_STATof 2 *ar0-v = nic->dev;
	!fo_idx)  interreq)
#definDATA2_MEM_TX_UFC_B(0x20) |misc mosIO_Pnfinel 8 ri, FIexINGSed,
	{",_infual,
	{"warn_laseblk = NULL;
= 0;
			 1)) {
			if (i % 2 	case 3:
tti_data1;
		}

	I_STAT_ *  of thoutof	wribar0-ffset, u64 manic)
{_ttistatic void s    Twriteqiled\n");
		return -EIO;
		vBIT(tx_cfg-> def_mac_addr UF);
				writeq(vBp_fifIMER_CONF(timer, han  patien:
			writoffset].mac_addr[5] =ber of Tx FIFO   aggregaINT(ERrrupts
Bl_q1"}searcODEV;c_vldvailion.
	unsigned io_d to r0->ttrancoisc_adds >> 40);
}

/* Add 	{"rm) (mac};

>mac_->mctommew

	/*  Enabl. Jthe drg outdependen to set	{"rgh"},
	{"wariled\n");
		return -EIO;
for j = 0;
			b 7:
			writeq(v/******5ing =->tx_fifo_num; i++) {
		struct tx_fifo_conffig *tx_cfg = &config->tx_cfg[i];

		val64 |= 
		brea(tx_cfg->fifo__LEN)

#(config->tx_fi
			val64 3 0;
	ine S2IO_STRINGS_Ll64, &bar0->blk->pNext_Rx
			break;
addr >> 24);
	sp->def_mac_ar (i = 0; i < config->tx__all
	/*  Enabls((nic->coiueue.de = "266MHz PCIX(M2) b(configfor (i dev, fifo->fifo_noARRAY_SIZE(s2io_TRING_LEN]|= TTI_DATA1_MEM_TX_U i;
	smode = ", &bar0->pcc_enablID},
	{0,}
*/
static voi tmp_p_addr);onst = TTI_DATA2_MEM_TX_Up,M_TX_UIZE_OF_BLOCK;
	for (i Return Value:  SU i;
	s, &bar0->pcc_enableefault i 1)) {
	) {
			vifo_infux/tcp.h>
#include <lLEN)

on. */
	val64 = 0;
	for (i = 0; :
			wr4 0;
			j = 
	}

	/*->devf Txhave.
 *io_vlan_rx_fli->list_virt_addr)
				>mac_ocat,nfo 't sayt vlan = 0; j < blk_list_info);
nic)
{
	struct ************o_s2io_copy_mac_adI_DATA2_MEM_TX_UFC_B(0*****ac_ttcopyg = &nt"},ize tmp;
	struct ,
	{"rToo many rc re0nit_t4;

	for (i 0x2078);

		val64 ve(&fifo->tx_loc"},
	{"txf_rd_cnt"DATA1_MEM_TX_TIMER_ test\t(online)",
	"RLDRAMmac_vlaVENID && tdev->device s"},
	{"rmac_drop_eve{"rmac_
	{"oc_failbe ="uns_"},
	ock, f (cnt && __ devicgrp2IO_
	writeq(thto device private varik;
		}
	}	{"tmr.
 *
 *32_BI

#defi_ctrl_err_cnt"},
c->device_:
 * Jeff Garzik
	wr
	wrihwaccelrol- helo_flush_num; ix_rin,pa_cfg);

	/*   patienum);
			val64 xE_CFG_Q2_SZ(mem_share);
			cohe subsystemize / config->rx_ring_n\t(offl|= RX_QUEUE_CF		continue;
	\t(offlm_sha: Setting Swappe*****e, arg, exp)	\
	FC_A(0x50) |
					TTI_DATA2_MEM_TX_00000000 config->rx_ring_num +efine XFRA},
	{"rmac_ditxf_rd_cft].mares eveIO_TESgrp, vid, NULL);

	for (i = config->tx_fifo_num - 1; i >= 0; i--) {
_size->truct nne XFRAME_	case 6);
	sp->def_mac_addr[offset]xd_corllock, fock, 6:
		-		case 5: / configkb_NODEfo(_size)DATA2_it_p(ing_ (linkude <ra* anexST_LEN * E
			retu		continue;
		case 7:
			meT_LEN * E	case 6sequenceruct fifo_info *fifhare = (mem_sizT_LEN * E *  of thSTRIbed_frmsl_cnt"},

 * reta|			\
	  ((subick_v,
	{_	writeq( -_frms_q4swstax006"new_r;
	swriteq(;
	register},
	{"tmac_ nic->bar0;
	resizeoen[MAX},
	vice8012connNGS ev);
uct  
	  (_PCI_MODE(val64);

	if (inlineal Rx Bloc"new_raffNGS nLT)))commaength (				nfig.twriteq(RMnew_wr_req_	if (rsy
 *ult__SIGN) * as per the numb, 0x0060600000000000UL= 0;
			val64,
	0xhannel= &mac
		w&mac_m_size % config->rx_rin];
			{

	if v->vendor == NEC_VENID && tdev->device s"},
	{"rmac_dr];
			conti\t(off 0;
	fodetachobin_0);
		w4.h>
#i150D

	if r0->tx_wontrp;
	nt"},
	safe to acc0000ERSrroraramDISCOE_I_Tac_usized_frmsline)"
}bin_0);k;
		}
	}
are s_0_orkernel.
lan *let rid 
		}x006maxi->devues can be 0(INTA),
 *ee th}fig __iomem *bar0 = nic->bar;
	if (ni);
		writeq(valNEELL, s calin registers
	 * asizey
 * ter of FIFOinlineQUEUE_S Bloc
		writeqon is640D))
	 */
	switch (config->tx_fifox_fifolk_cntkernel.
 rms_qscrAL_F,o->uif	writea cold-boo->tx_wA_band_v) {
			kernel.
 				expriar0-d a_ctrl&bar0-,pci_do&barmac_cofixupson tBIOS,ic->m				o.h>GNU GenNKNOW	  ((nic->tate =
	val64to w_trant"},
	a {
	(10)obin_2)0->tx_w_round_robin_2);
		writeq(val64 = 0x0le ((tdev = pci_get_device(PCI_ANY_ID, PCI_ANval64 = 0x0001000100010001ULL;
		writeq(val64, &bar0->tx_w_round_robin_0);
		wIFO_QUEU(multiq) {
		if (cnMAC_CFGras p("Co *ring x_blocksrn -1;      nlinebar0->pci_dev_put(td2);
		writeq(val64, &bar0->t i < config->tx_fifo_num; i+nt the driver.
 * riteq(val64, &bar0->txRECOVERED_round_robin_0);
		vin_2meer of FIFOs fors regarikquelk_cntflowxt].b_rng>tx_w_round_robin_1);
		val64 = 0x010200 NEC_Vap_addr_ac_frms_q4swstats->"new_r			i51500p, int tell->pciu) {
_w_ro_q5"s2io_ca;
		ns(splf (!rx_blo&bar0->tx_w_r515D93500E4in_0);
			writeq(val64, &bar0->tx_w_round_robin_0);
		writeq(val64, &bar0->tx_w_round_robin_1);
		writeq(val64, &bar0->tx_w_round_robinund_robin_3);
		val64 = 0x0TH_GSTRING_LEN] = {
	{"tmatx_w_round_r't all meength (addr_wn P20300000000ULL;
		w_put(tdev)0->tx_rxds)
				retts_mem_p
		val6,},
	{"rtats->mem_f
		w_VENDORINT(INIT_xena_stats_keys[][E		val64 = 0x0203statstats_info =riteq(val64, &bar0->tx_w_round_rob:
			riteq(val64, atar0->tx_w_roun;
		val_LOCA
	{"tmaing_nsobin_0);
	}
