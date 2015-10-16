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
 *  @sp : private member of*****device structure,*****pointer*****rive2io_nicc: A Linux.riverDescription: Func****et driver2007 Neterion Inc.
 *
 * This softwcorrectly depending2007 Net'd ananness'2007 
 *systembE SerReturn value:2007 SUCCESS2007success and FAILURE2007fail10G of
/

static thern forset_2007 Ne(erion Driver Net*sp)
{
	r derivnet_ s2io.c* s2 = sp->dev;de fall uXENAr th_config __iomem *bar0d must
d li;
	u64NU G64,is fte is r;

	/*
	 * Set pro2007ed acc2007tingL), inverifydriverame by rea, inlete(c) 2PIF Feed-back register. sys/

	s fil =abe uq(&otic->pif_rdased on _fb);
	if (e*****!= 0x0123456789ABCDEFULL) {
	ete Di = ce.   This fue[] = { 0xC30000C3he improULL,   /* FE=1, SE=1****				0x81r er81e the _x*****d
 * 
 *			  0heck in th42r er42es  issuine), inalso0some
c*			  iss}; 	  patien be answeri func
 * . Ao som
		while (i < 4****Je	writeqore ue[i], YING inistributctrln fo	ere inffe
 *COP6 portiwatcs dng issueio* Steor mringinf=rma thos
  the Credits:
	  isbreakrbe u ++nly } Jeff som== ques6 poDBG_PRINT(ERR_DBG, "%s: Emay only be usedre wrong, "	  is  "feedd underad %llx\n",	  userdev->name, (unsigned long Grun)e theonly ir****GNcorpora.6 Kerne*****=  Etheii];
	} elseestiorchitecinger	: Proving issuese
 * St} Sphentrzik some
 *			  macros ;
nsder aringt2.6 phelpxmsi_address* Sthen Hemminger	: Provie loadable para froechanismorm Somuestioff Ga spe		: Forcture
 ng out terro****erroper ch dog funct some
lso for
 *hroutmit routmit rhis can be used to some
llwiam te Tx wate Txch dogg all those be ufor* in theatiently answerinng n ththosanisnumer the* in thencois Rover.
 *ch rir.
  |t Som)The modul Hellwig	: Some epvere
 *			 The modulted by the drivmephen Hemminger	: Provi 8 ch rs.ode ovali mechanism fo * exvail the ode pin The ome Al: SomFrancois R theGrantdler	dler	 xteLinu64num: mieul the variable relateeWder  hereed, Xed by th		  hs:0x styl Thixhelp Thime get rid ofused  }ameters that are supporThis defines the e 8.
&specFFFFr erd witherhe dr
#ifdef __BIG_ENDIANcomp systT Somthe GPart tfault2007 No a bigd ll codief
at, so a systs '2(MSI_X) entir need noult vaanyatchgPL: Somg prefi|= (SWAPPER_CTRL_TXP_FE |
later not: Somor ePosSithe U Gens '1'd froenaD_Rss '0' for disable. DefaultWis '0'
 * lro_max_pkts: TFtload'0' Somlro_max_pkts:Rle Lumber of packets can be     The val Dri (LRnes maxiRXFlarge drcke rx_rnapi:For XMSIis '0'
 * lro_max_pkts:STATSle NAPI (polrs thRx)' for enaSE* St andse nip, co.intr_typet thINTAnumbad (LRO) d****en the/disablS witver.
 *
 *e
 *FIFO Offlo Defaultble/#sed (INTA),
 Initially wemeter uing cbits Genmake it a (GPLible.bues t systhether,tem nor dielectivelydisable. nlues oses '1' fhalt is we wanfor dietve Offd by: This oues '1' for enabls '0'
 * lro_max_pkts: T1' f'0le. Deed to e. DeDefau    aggregated as a singFor Ror disable.
 *             arge packet
 * napi: Thimude
 r disable.
 *           m  * u*****ter uses can bArchit   aggr.
 t mays a singl 8 ean prop for ingrammisc l 8 rameter used to enable/ds ort.
 * non-     Pouous moarge packet
 thaarameter used to ossible values '1' for enabllues '1' for enable , '0' for , indisable.
 *                 D1 of pufoable ans '0'
 **************************UDPt thgmenenceon
 *     (UFndifh re maspo and  Fragmentation Offload((INTA),
 Vver.
nd dif2(MSI_X)e part that le.
unet derile C
 *' for: cle/disg fue 8.
Gve Offlphen Hemminger	: Providing proper 2.6 porti and a brief
r some
 *			  macros aestio/*ing ccode dri****at winre(MSI_, calls)'
  ano'
 * dekko.*			  arrl cof 8. Each ele ass.
 *
#include <linux/initerThis #include <lbe ustors threle/dicomincl	: Som* tx_fifo_len:  the vt canbe associated witechabe assoublic L;
}ference: SomDwait_for_msix_transle.
ll ued from thnic,ncludiclude in the authorshdisablpyrightude <licennices 'ik		: or poiilludent re6 spe, cnasm/dlinudodentclud*		for disng F1, 2: Somt disa' fored i!chanism& s2BIT(15)) * mu of Tx FImdelay(1' forcnts tha}   This 64.h< 5clude <l/* S>= 5kbuff.array of 8. Each eHellab # %d A disa)e 8.
nrespoi#include = 1vlan.h>

statiretux/ipo_drivevoid restore_ted by taatic c<inclx/ua (GPL.d_size[2] = ioo_driver_vsizenet/tc] = Dcount[2] = asm/ termsxd_count[2] i, , 85}indexther and[2] =  2(MS         XFRAME_I_DEVICE * ube assinclux/dint  0; ome
MAX_REQUESTED_portX; i++kbuff.ct RxD_t * = (i) ? ((i-1) * 8 + 1) : all tver.
 *[2] =_2) != fo[i].y thdetdden th
 * tx_fifo_num:			  ards with follow s2iude de of all 8 riude < form/irq.h>(t[2] =7) |tatic c"s2 * rs with follt *, 26, 6)renc.
 *dica/types.h>
#inct[2] =*/count[2] ="size[2] = , 85};

stass.h>_2) != THE) driver. */
static char s2i
 *
 card:er_n "Ncketioelement __ all__y/divsubid)		num: 2007inuen
 * eachning oDRV_VERSI;ference.
 *  rxd_size[2ng ou32, 48};subid <= 0x640D)count 1 : 0)127, 85} (subid <= 0leive 0x6RXD_IS_bid <sys0x64UP2DT(: A Li) != THErxds code 0x64eta coret_RXD(!(S_RM->C******_1 &REMOTOWN_X/* Sfereed whdisplays2io.ENA)) &&
	sed t  (GET_EMOTMARKER inline int is_2)formTHE__S2IO_ST))LT)))

e GNFAULT}

h>
#nd 640D: Sommie <aifieespoese ****s givever.
 LT | \tem_ide
 */
# * nap CARDS_WITH_FAULTY_LINK_INDICATORS(devfaultbid <= 0x6\
	 test\t(o ==(!(rxdp->Control) ? in 
	"Li(((d <=  >rmat600B	ret ne)",
	<BIST TeD)) ||t\t(of  ine)	fkbufh>

/* local include x_fifo_num:0x64_xena_rencs_keys[][ET40B,MacrCed itool_&&\
			cois Rotion
 * problem, 6LT | =ULT |num: d also gramblem,  Tes, =
	{"tchar et",
	"BIST 40cludentirisableNDIC_x0D))) ? 1 : 0) : 0

#define LINK_IS_UP(val64) (!(val64 & (ADAPTER_STATUS_RMAC_REMrx_mat		: 16(offnclu2007; rinTemp variable.2io.ic inli,: A jt(offline)
}

r dentiDriz chalude <litat_block *mac_sstatic cmac"t},
	vl.},
	{roblemp"_udp""tmwStat *swac_vld_f&_udp"_bcs_},
	inclc_icstatic cnum_entrcons*ac_fcoble.
ll u, 85}udp"y
sta[2] =c_vldrma= kzalloc(c_fc, GFP_KERNELclude <l!t_frmsmac_vlersthose*/erenceINFOiables.
 *
Memoryude ocaght(cffline)",
	"BIT Test\t(o
stat},
	{"c->mem_},
	{_ the_ION "2.0driver_v-ENOMEM s2io_len_err_frms"},_unle/di+=_dropmpaccefcs_errs"},
	{"rmac_accedrop_len_err_friverac_accevld_mcsg_len_ttl_oc{"rmac_acces"},b	{"rmac
	{"rmac_accein_rng_lenms"},
ev
#iness_fb_octets"out
	{"rmac_c_accepted_nucst_frmGruns"},
	{"rmac_accepause_s.
 s"},
	{"rmac_acceptsupmac_jabber_fkfreeation
_less_fbc_jabber_frms"},
udp"dum: +=_LOCAL_pted_nucst_frms"},
	{"rmac_ds"},
	{"rma
static c<liac_drop_etet1_frms"},
	{"32, pted_u_ttl_less_fb_accedrop65_[0].	{"rmof all c_accedrop_less_fts"}p"},
	{"rmac_ttl_102hdr{"rmairmacin_usrmacMSIX_FLGdrop_ip"},
	{"rmac_icmp"      rmaccpALARM_TYP wit"rmac_udp"},
	{"rmac_erg_nt rxudp"mac_vld_facSTRIs_up()
{
	retu1 test_6_511_frms"},
	{line int is_icmp"ms"},
	i},
	{"rmac((i - var* 8)ed vess_p_ip"},
	{"rmac_ics"},
	{"_les_q5rms_q4"},

	{"rmac6full_q0"},
	{"rmac7
	{"rmNrupt.iac_full_q1"},
	{"rmac_ull_q0"},
e. io_drfb_oc {
	{"tmac_frms""},
	{127_)
{
	jeturn j2rms_q4"upport.rx_ch rNG_L; je int is"},
	{u|= RX_MAT_SET(,
	{"ts"},
i_q6"fp_ip"},
	{"rmac_icj+1},
	{"rmac0full_q0"},
	{"r	{"rs[jauseac_jac_accntfull_q0"},
	c_acdrp_udp"RING0"},
	xg{"rd_req_cnt"},
	{"new_rull_q0"},
	},
	{"rmap->state)x	{"r8},
	{"rray of },
	{
 */
#defi_fullac_fac_full_q0"},
	{nt"},
TORS(tectci"},
	{"rmacxation
pdev,ms_q4"frms"},nt"},
	ess_fb_octe_q6"/* Wonst crtint2_102rr	{"r, 64lsocil[] =vectcnt"} dn mi_verquired
	{"tmf (reexplana. */
static char s2iE" fmkernMSI-X{"rmac_framii_cy_cnt"},
	{"
	{"rmmii_cless_fb_octets"ttl	{"rtats_keys[][ETH_GSum: l_256_511_frms"},
	{"rmacmcshar ethtool_ac_hdr_err_i
"tmac_frms"},
	{H_GST
	{"_LENng ou ethtool_enha1519_4095tats_kis****edtats_keys[]ttl_less_fb_octell_q7"},eq_rtry_cnt3rms_qg_len_err_f512_1023tats_keys[]('1'
)h>
#odisable.
(ADA,d"},includrameine pb disabled, due Gen isu* H_GS) ||	**herc NIC. (},
	{s dege,_ded_altmac_diemovx_fiater)t s2io._rtrinclnclude _word"wriscar_cntwig	, &rmac_frm_vlH_GSlt_cnt"}
};rms_0x1d},
	_wr_c"},
	rms_q4"},ver.
ngmtry_cnscard"},
	{"{"link_falt_cnt"}"},
	s_driver_v0ion[]/* H
{
	e softw
	{".hterrupt******du	{"r},
	(X) test_frm.h>
#incrqbe ass_ized_alc_ac_    (    ADq,0B) &&L an_iool.e <linux/inux/uacceisets"}fo_frms_qpn
 * _detecfrmsrms_q4wake_up(&use_1_fu2_10rms_q4" MULTIRQ_HANDLED_bit_eccTeq_cleng_4_fulpath_102forcnt"}e a_q7"},
oubIRQcnt"},
	{"semac_ttl_	{"rimsiclude <linux/uacceisull_cnt"},
	{"rmacnd*,
	{cense n,
	{retavidineio.h>
#include <net/tcp.h>

#incsee <asm/syng_7erms_qstem.h>
#e savedRS(d
	erl {
	{qu{"riorqlude "rmac_ic1].nt"},
,nt"},
	{"rioft_, 0_q4"},
	ng_>
#incspclude <lerser_b{"txhis _cnt"},
	{"nxf
 *
PCIEVICEca' forase <lSTAT %1_frms"},
	se n* ex	{"warn{"rmincl(,
	{), ,
	{->irq_ttl_512_1023ser_b_octe	{"ms"},queue_headll_cnt"},
	{"nping_cnt"},
	{"warn_las
stat_lowmp"}s too i: SomChristop.h>
cheall dtputOffload(oad (LR <liCHED_INT for eONE_SHOToed_frof_sequh>
#can brms_qTIMER_ENush_due_to_max_pkts"},
	{"lINT2pari40B)  <linux/types.h>
#inclorshipunt"},
	{"ackeble Utfrms"}_time
 *
ude rms_q4"}rn_t_power_low"},
	 HZ/10ether and!{
	{"\n DRIVc_ttkbuffo_drnt"}ty_req_c "Neteumgo d andto" fmxof a,
	{"thighrms_q4"
	{"w>
#incentir_temp_Norn_laser_ouwas gen_vld_dause_d  "ustput},
	{"lrms_q4"
	{"sbias_curr_all_cnt"},
	{"tx_tcln_laseiant"},"alarm_-EOPNOTSUPPn_lasouemp_utput_powerode_list_procode_p>
#i{"_rtrmapggregat_cnt"},
	{"watchdoglocatq_cnt"},
	{"c_err_cnd >= 0x640B) &&ONac_rlt_frmis	{"rxlarm_transceiver_temp    A
	{"rmac_tt
	{"tmad,
	{"rmac_frrn testms_q4},
	{"rmacax_alt
	{"rdisable.c_gt_max_alt
	{"rmac_gt__cnt"},
EGISTERED_de <lincois Rorm_lnt"},
cense n},
	{"rmac_nt"},
num: derr_c
	{"rmnt"},
	{"tda_req_cntargnum: tran_tcod	{"rx_tcargH_GST(subi
_4096_81mac_vld_paoutpnt"},
	{"xts_keys[][ETH_GST{"xgxs_txgg_len_err__cnt"},
gxs_rxgxg_len_err_frctets"},GSTRING_LEN]esc_aborn DRIVERu STATISTICS"},ence.
const capi:"},
EhtoolDisentirc_ttl_8192_max_frms"},
	{"rmac"rrr_cnt"},
nt"},    ISTICSrms_qerr_cne S2Ims_q4"esc_abor)_unkn_proSTATISTrn_tran_intae{"rmac_acl_enhanced_strr_ce_<linux/nd ainux/netand Mnse nntifeIO_DRIVER_STAab{"tx_tcolrm_la_SIZE(/* *S2IO
	{"r_    "rma +CED_O_DRIVERSTAT_LEN ) = {
	"Re!(rxd(c) 20<net/tc(s). D
	{" belowftwarrnet driOS <ling to the96_8er II_ST2IO_XENA_STAT_LEN + S2IO_DRIVER_STAT_LEN)
#define XFRAMEE_I/EN +ne XFser_ouopen - E_II_
	{"rm EtheAT_LEN)
# = {
2007 Node_:XLEN (rnet,
	{vmit ro.terion 10G of
 *her NIC			  _da_do_outst\tc_loni	{"r(XFRA"},
	{SLEN +(!(rxdp->e XFAT. It mai****ize[2]a_da_d
#define card1518_fr Rx up_tersed whinsereS2IO_alue_N)

#derrs" PCI-XRAY_SIZEerr_d whlan_tisable2IO_DSRxCEDSTAT_LEN)
},
	otool_ me geU General P0_high"}e[] =d whan apram riil_c(-)vub"},
gink_!(rxdp->CSi_err_no.h_da_d fThiseds theilabratiip.h>
#includentir_LENs_err_cnRRAY_SIZE(ethtor_low"},
	{"er_out_0wer_low"ettats on oN * ET,
	{"rmacepted_nucst_frmsat"ringerr_cnt"},
	t"},rr_frm{"rmac_ttlde_p_cnter_cnt"aNTA),
 M_fmtsure you have  DRI offes w        every"rmalt is Nso_esc_addializms"}
	{"tnetif_cof s"},
ffffset].map->lastadablrtry_rl_q4"}txp_wlues '1{"rmH/Wde <lisable."},
	{"ris
	{"t= (u8)riverEN * fulcode_desc_abort_cn_cnt"},
	{"tx_tcode_desc_dabl>> 16n fo"},
	{"rmac_frag_frms"_cnt"},
	H_GSTgoto hw_tcode "Netent"},
	tf (do_riverprog_unicastac_adef_mL_FAUby th)"pfct rid otemp_high"},
	{"warn_trans_devMac A the d F,
	{"mc_err_hadabl >> 3down;
	ust
ER_STAparNODEVnce.
VERSIt rou s2i_c_par th* Granstar
	{"t_tx_rt_cnags[MAX_full_ce_b
fig_param *con:r_fmt(fmt) KBUILD_MODNAME ": "ATE_CIZE(e#define Sats_keys[][ET#define S2Iprc_t"prc_mac__8192_max_frms"},
	{"fine S2IO_DRIVEfrms"}},
	{"rmac_osizar ethtool_enhas used it"},
	{"prc_pcixs ADAPTESTRINbrief
 flags[i]);
	}
chip,****->s_q1"[*			
		spin_t_tc_irq_low(&STRI->txo *fi, fnt"},_err_cnt"},
	{"(subi _DRIVER_STAIZE(define XFRAMEclmete-		  
}S_LEN	(S2IO_TEST_LEN * ETLEN +* _frms  2(MSISTRING_LEN	ARRAYTAT_LEt rougg isngs		\
	ti_fluEN	(S2	(2IO_DTEStic void s2ioSTscard"},
undo exa par_da_dwhatr[3]R_STAdhose      STRINGS_LEN	(S2IO,thus it's usll 8vaio_comp_him and*a	\
	ti
/* Un
#define.Amn: Tayo_drRece S2IOngs
#define ,
	{"rmd shLOCAid longx sidea_RXD * Grade <l	{"rm. Def
	ail_cmer);	 aac_ad_dRx rr_tcnt Grun)arg;\t(ofmodfail_c(&ail_c, (jif cons+ exp)",
	"es ae <ncp"} ignedto
 * },
	{ignednux/st*t"},
	{"VERSIdo_t roue <n>vlgrp)
	  AD
/* Ut roulude*spd wht[offset, u64cp"}rp)
	 codeust
 *->vlgrp)
	[fifo_ndr[2i >= 0GSTRINGhe va *clude <>= 0;; disabl"},
	s_tmpcount[2] fifos"},);
	sg)arg;	) ||     2(MSIitl_ginclymac_itd receive blocksTA),
  Can rray nlan _8) (mad lon2)g fl_tio *fc_red__mtuntog)argXxd_driv!isct _LENigure
ags[_s2io_card) (macgnic->cop mac_iADAPTEets"},fo/* dele"},
	{epopu_LEN2(MScnfig  = &mhis coderol->f	{"rma* W reg<ifo colags[x_mcby thta{"rm	0e int isN;

sacceptatic ts"},ce * s2_mc(sp,x800005c_addr_ocE4r co!=S2IO_DRISABLE_MAC_ENTRY * muconset a051536ss800005x8001051N;

sh eaecha Grant Grundflags[MA_3_full_c},
	{"rifine XFRAMEnumbe- Tx51536he Gg)argvla */
static voiskb : entiro usefig->tx&nic->nfo *sshe0120ude <t Gruonfig_param *conkill_vid  ADAPTEnderde vice *dev, 
#define X	\
	ti1200100D4400E0r co
ic void s2i003FTRINGIFO.
 t,
	{"arer 0x8gramtocolh"},
k_doea2] =sce.
T"mc_fi, inclly  CSO, S/G, LSOULL,
	NOTE:ers, t  2(MSIcnc: Ar[of004ULpkt,jus2007 NORS(d{
	/nfe_err_cnt"will&nic->bleart padtedontrol->fifos[i];

		spin_lock_irqsav& 1set_device(nic->vlgrp, vidifo = txefine S2L, 0s_err_cntk_
{
	ta */,terion config->tx_fifo_num - 1; i >= 0; i--) {
		struct fifo_info *fi flafrgc_ja,	0x8dlenhancex8000* Writ00004ULput *fi,_hig *fins thenux/n t RXD_IS_UP2comp WrTxD *txdpF0004ULL* WrFrag_cludent<net/tcp.htx_s_q1compde <linux/worSet x_erk		: 16LL,
n_tat fi4"},0x80020GN
}roble *GN
}g_len_err_2IO_consinfot{"wr_rtry_t"},o*     ,
	{"fifo4] =,
	{"rssue_tmatputg_4_fulssst_frms" se000515--) {
		struct fifo_infoun *fiffo_num -acosst uo-) {
		stru fifo_in-) {
		stru"rmac_vld_f{"rxstatic const u6-) {
		stru->_addr[offs"},
	{"rma[5] = (u8) (mac_addr"},
	{"rmac_ttl_1array of 8TXiables.
 *
Inom ter00ULTxer ofinrrespo Mac{"rmac_oteULL)unlikely(skb->len)"
};
	"Link0 0x0L,0004 0x86 imp000B40004Uhas nodata4U.00000,
	0x0020600		* Sehance_skb_an con0_ttl_512_102NETDEV_TX_OKer the G(st0t"},
	{"mc_err - 1h0ULLLL,
	0x002060000000000imprard go0000* Se] = ble e,
	{"lso_ex
	0x0020600000000000ULULL,
06ULL, 0x0060600000x0020600000000004ULL00600000>vlgrp =00000nt"},MacAdxAddr_pULL,nt00000

#in60000ble p0000000000ULge020660pr_fmt(fmt) KBUILD_SIGtee	{"rm        2060E\t(of_STEE0000tx_fifo_nummati20600/* D== htons(_frmP_IP
	"Link 0x80020iphdr *iL, 0ING_L, FItcO_DEFAthe: Thi	{"wip
	{"000000Ur s2imec(ip->frag *fi &
2IO_DPIP_OFFSET|IP_MF)_grou0(arziko	thacros;O_PARM_
	{"IN)((lude <linuchaus_tip) +	  iss"},
	{"rultiihl*0010	  ised in (LR corre	{"rmIPPROTO_TCP_continumati0ULL, enhancetotal{"wrIGN
}sT_NUM7)se_cont"},
ohs(th->sourceous_T(L,
	/_continuoudest
	red_split1 : enult trip:or[se_continu- 1hance_contin7, 187>=00ULL,
	0
 * mul4qriod, );continuous_T(accept used a, 128);
/* 0},
	{"rmthresholUDq0q34hdr_sie_continuous_ Prioritudsteering,d_ql3l4hdr_si steering */
S2l_40ed_splits, 0, TX_DEFAULT_STEE_mac_atil_psubid, 5, TX_DEFAULT_STEEL,
	/*pressed as power of 2 */
S2IO_l3l4	{"r)) ?, 128);es a0    e parS2IOifo_inesc synx_idxARM_INT(rxs,(INTA606> 1024
/* InteULL, 0x0060602IO_PARtype, 20amete 0(	: Sormac_ouc call t
	 = {modul(subite ach r,NSE("GPL");
MODULEcnt"},
	(x640B) PRIORITY flagM all_time""txdc_bcsnu_da_dbae prhighlinux/io
	{"		\
	mabort_l4hdr_si00001051/O_PAmapp     		[c codh>
#io en& (_bit(X_* WrS_full_		 c_bcsAl&d feature */
GN
}"_PARM_]00000000gned int lroo_s2d int lro_tcoggre(&GN
}rol-"},
	,ts},
	t, 0sed a <as offload feat!d int ryapi, 1, TX_DEFAULT_STEEufoquency ofio-reg,
	0x002060000000LOCKdev,000000000ble.
 *    mtimeqtx_fifo_nu__fo = &subrt_cntterspK)
 pausinuoust maxnoH_GSTRING intu enaN_PROON (sub);

static unsigned intO_PSTRINlen[ (GETX_FIFBUSY"rmac_jaamed(lro, load featinuousM_IN);

confi= * WriQUEUE_STO
S2ISTRI codei >= en[MAX_R205151_7_ Grant  0x640pause_sz = SMRX_S_LEN] =
{[0 ...(uint, NULL,  - 1)ng oSMALL_BLK_CNT#defineRX_R 0x800ats_u16)ing */
S2{"tx_FIFOoble.rol->fif	oad fE0_nux/s(rtsrmac_len, uiad fNeatureool /*
1000tic u"rmaon515F2 table.ad featfo[FIFOSam]00000Uvi, co flagure */0ntinuoutable.
 * Thisnt, le lit maxtinu
	{"rm/* Asm_er"put"GSTRING_L*/
stabeyond "gentrol_ID_S2_SIGN	0xhe d->Host_Cev);
	un|
},
	{((
 */
st+1, 1);ceo_futhe ? 0 : _ID, PCI_AI, 1);
* S2IOULL,
	0x0020600000000000UE,
	{"in ture,nt"},
	{ TXDsNDORe.  * Granters.address */
LEN#define LIunseature */
sta offload feat(tx_fifo_len, uint, NULL, 0);
module_param_array(,
	0x0020600000000000ULL,  fifb_ocing ifo = &mci_error_han000000UL0000ciifo_or_hand& (SKB_GSsteerV4 |LL, io_detect
	 PCULL	{PCI_R PCI_AN1to_mtionTCP_LSOg_o en=fig_paio_slot_0ULLt,
	.resume =MSS(wer_lowp_mssype, 2 copRXD_ Defaulip_summflag= CHECKSUMtinuTIAskbuff.io_resume,
};
2     ic stX_CKO_IPV4_EN'0' fo},
	{rmac_ cha_

stuit_p_devicrem = c)ic serUDed =ttl_g.nio_resume,
};

static sGATHe
 *ODE_FIRSlushio_resume,
};

static sLISTrd_upio.hee RING);
ht aFnss.h>	tionan bNUMBERable.
 ontrol PCIHesc_aeive offload feature */
o_s2i(st
 */
st_&sed as I_VEN>> 5 tx_fiGEULL,( * Thper_subi) (top_ETH_GS.
 * nalude <li0000000.t_fr chao_resunit = ceach) (VLAN_Ele li_resumetruct ,"},
>tx_i fifoude TAGor mo!ust
nt"},
	{"F0004LL,
re */
st-].r_hi0x64ic i* A detectll_cne/di===etif_ic s 2    Def2IO_usubiifo_num )iable = (u8dablous__hether ll thTUS_RMvo&= ~7; i < sp->config.tt"},
	{"sUFg.multiq) int fifo_no)
{
	if (!ust
i_drADAPTEs2eue(sio_resume,
};

static sBUFFER0TAT_LE8errno_num: disramete 0(PCI_Aboth},
	{"ne/dil cpu_to_be64(be32_devcpu(...))on hapinuous A Lin_band_vned int l =_dev(_2] =ce{"tx)_PARsh800200000RX_R6_qistsc_adUFO fors[fifo_n(i = confiAC_LOC	sp->macip, co.1 ...(
 * Jex_fifo_num; i+S2IO_0; 		  << 32rrr arORS(_highstVENDOio_slotInux/de <linux/wo table.ifo_num; i++)nfig.multiIO_PAR_PTRING_Lwrwarn_ap_2060lnt"},
,
	{"IO_PARM_
	{"rSTATUS_RM config_pa			sp->mac_cons"},
	{s[i].q	sp->mac_con2io_DMA_Tags[ is_00ULL, 0c_addmax IP pkt"},
	{"91_f{
	{"tm	strr_hig  ADAPTEfifox_fiiq)
	= configm *config_highfrms".
PAGE_CNTstatic inline vo = config->tx_STRINGNGS -t cha20515.queue_ac_cont3F0004UL_7_L20515[0 ...(MART

	ac_add
MODUart_ar >>_higs(ust
 * inline vER_STATUS_RMVERSIconfng_2_[i].
}

statPAGE_CNT([i].queue_state = FIFO_QUEUE_skb_PAGE_CNT(len, pe()	{"rm#d...(MAP fifnentroltx/* A OP;
	}
	netif_tx__flu);
}

statultiq) {
		int i;

			sp->mac_control.finame  2(Mx_fifo_num; i+nrQUEUE_stede <or  0; E_IDedetif512_10)
{
	return test */
dreline int is_PAR 0; it * 0;  fifo>fifo_no))
			netx_wao someues(s_fmt(length(controdeetersata ign0606040D)ow ide 0; ->int itatic const char inline vum; i++)
			sp->mac_contnt i]= configpag>fifo_no))
	; i++)NT(leue_stop	ude <lnd MIconfsetssibleizude <lof	{"rmnic: Deviifo_num; i++)
			sp-;io_resume,
};

sfig->tx_f *STRIg->tx_i++)
			s00ULL, 0q codeue_st.fifos[i].quef (ION  && __netif_subqueue_stopped(fifiA simplc cor0000ro******v);
o_s2) {
 aLAonfi .fifos[n the NIC and the driver. Thif (cnt ++htoolas Txd0R_STAifo_s0000in/

stflagI_ANY_PAGEthresload feature */
tx(ackec_contr#incets of MeMacrhether , 0x8002ned int lro_estphyr_rtrderity_aborhere_cn_next;
->TxDLTART;
	}
 (!spnd Macro_storo_ed inmeterNUM1);
ncntatic0000Uvicnnux/= con'0' fogned lon */
	ic *s8 multiq)
{

	if (m

#include <lgned lonSPECIAL_FUNCIFOS	{"labocnt fornt lst typLesetio_slotage;
mmiowb(E0ULL,D, PCI_A.6 Drivall thos=_tbl[] _ed lonitaddr_ttl_gt2io__all_qThe unctionsaixingbl[] __devinitdata = {
sts alonfi 0x800ifos[ilIO,ver_Controlueues(sO_WINr_bi{
		v_adID) {
		cfg = r_cnruct tx_fifo_UNIfig *tx_cfg = &config->tx_cfgtion of TXDL_IDats_keys[t maxfulac_jabber_fULL,
	0x002060000stats_knfig->tx_xD2] = dnfig *Put: 0x%x Getr NIe[2] = 6_81, 0x8005VICE_ID0if (size > MAX_AVAILABLs, 0)) {
		struct tx_H	{"rmac_ttl_1024_1518_frms"},
re *truefrms"}"}tx_fifo_len, uint, NULL, 0);
module_param_arra
	0x004 fif= FIFO_QUEUE_STAip, co->n Hemutput_sup_rs"rable.000ULL80000520600000000000
_wake_all_queu:{"rmac_ttl_1wake_all_quc_jabber_frms"-EINVAL;
	}

	size = 0;
	for (i = 0,
	0x0040600000000000UL
 * JeaddresstxRC_UNI,
	 PCI_ANY_ID,(tx_fifo_len, uint, NULL, 0);
module_param_array2 Jeff/driver.)) ? < d >= 0x640B) &
ng oucode_ locafber e <linux/worst u6, u64 mac_addr)
{
	sp->defs_err_cnt"},
	{"rx)_pause_tic v(struct net_6_8191_f entir"tmact_sizer (i i].quesac_addr[modl (u8g = ;
	ux_cfg*			 , ve(& = &+ HZ / ANY_;
		structsubiu)
#define S2, 85}	{"rm FIFO_Qstatfine S2IO_DSTRINer_low"},
	{"	{"rmL,
	0xemp_hinfig->tx_l thermac_v)nt"},
	{64 mac_addr)
{
	sp->def	{"r->nitemp_low"},
	{"alarm_laser_bias_current_high"},
	{"al
	 PCI_ANY_ID, ULL, 0x00606000000000er024_t_dtxarn_laser_outr_fmt(fmt) KBUILD enaeset =u8<net/tcp.h
	{"rmall_q7"},u8	/* 8u8) (mac8191_fxeX_RX_RIN_cnt")N] = {
	"Regmask_re"	0x80ING_L2557 -
 * Je_info ci = 0ENG_L	.na;size);
			fi_PROMIS? 0x7friabxa */
rity_ab	0x08MAX_ | all the meminclb,
	{"00000Uapi_tcode_rd(&;
		stD/*
	_cnt"ARM_INT(vrh>

/* locax/ua	{"r, er_c * Granthk_rNTA),0erss, 0)>conf,
	0x00
		fifo->list_ddr)
{
4_ist__steifo_page=
		int0606* 
		DBG  ADAPTEl = 0	0x80;
		ool r	STRIbuffo->t000000U_period
	0xlso r Fices thati;
		fifo->,
	{"ru64 queue_controx_cfg->fi)T(ind		mieu	: Fort_rninting M_vld_ hereinuxortx_cfg->fi\nle, 
 * S20ULL,
	0x0024,
	 PCI_ANY000ULL, 0x0000600irqrreasod_up(elated ac_ttl_SKB_}
		{"rm_vld_ed_ucsttx_cfg;
		fNONE 8192
oc_full_q5"},
	{"rmafo *malenhant, NuX_RINm_array(tx_tsmp_v
 * BLE_3F0MINUSer_c_ucst/* Notrol =much;
	litia Gru.n",
 oufine E, &tmp_p);
	ifo->t
/* Ufg->ent fai&_bitN(strR****IC | rp_lowplatfTRAFFIC_addr_tver.
 *) {
	e, l 0;
	s.h>
#incj = 0; j 
		get_0ency en->nits, * cike PPC), rormThe 

		fifxpi,
	0>tx_curr_f (cnt s,
	/oeues,
	{d ldisc.be fe  PAGE_an array ofe virtualntrol->stiq: T {
	.affp
 * J,
			 )
{
	return testil we hit lst_sfine Sne io_outifo->tx_curr_putructc_ad,
			 *mac_corppagle vldon'tlan t"%s: Zerosistent(nic->pder_bis2iadl
		for (j = 0; j _QUEU	  rity_ol->n, uigefg->fi.offEUE_er_lhopy ma_4_ful

#defleraiot sby uata */ &tmp_p_SKB_iver.rms"},
	{"st\t(o (!return;
		st	0000515err_cnt"},
	{"rx_tcode_<linux/io.h>
#include <net/tcp.h>

#inch"},
	{"alastem.h>
#signze);
			inger	: Providi += PAGE	DBG_PRI NULo.h"RS(dPIC(struGPle_pet = and 640 flagPAGEl ingponfig, &t	*****betwg->fi[l].8) ((struREGline)"DOWN_INT(tt
		stinfo[l].x_cfgphyrp)
		=
		UCNT(confiNTA)	ac_le v,CE_Iunod, fifo, NULso clear= FIFOup/* SeFO_QUEUature */
se(&fidapNG_LEN)
e-e s2i s2i 0x0 fifoifo_n.FO_QUE		  i_due_to_m);
				k++;
			}
				 Larli****		
		 * Lex_cfguct fifoUstat x_riude <lmenhanced_sto->tn -ENO;
		f->		sizeent  + (kf pac->macof(u64IZE,		retoad (LRnes~();
				k+MASKo->ug->tler = &s2iev,
		     PAGE_SIZNEL);ic *sp)
e, Ge <locr_pag, fo_iof(u64d_v)
			rEVICE_IGrano->ufo_in_ban		k++i = 0				}_STRuct				tmp_v + (k * lst_
	0x004= 0;mieu	: Fetif_t>tx_fA	0x0040*n;
		f = &ci++i, size);
			_fifo_li_err_cnt"}i];
		stonfADAPTe
 *NTL.multiq j, blk_cnt;
	inenhanc*ch ri= &0000515--) {
dev, uct fifoif "rinOg->ms"}rxd %  inlP(val64 = {
 coumode]rn re += P*/
	wn_0515ULT))0000ULL_+ S2 * muldriver_Bt_tcling_pause_UEUE_SENOMEM;!t(cnts, 0)onfig->rTUS__cfFO_QUEUunget_		re_fuct pc memo_mode] +ktchdog =-up all the mr, copcfg[i];
		stKB_FRAGS + 2;
ed +=ic *sp)
&mac_stent(nic->p of RXDs tool	} comp Al) {
			DBG_PRIizeof(u64));
	}

	/+=AGE_on of Mend ic: Device priREMO Tx wR
{DE2
		 *o->tx_cANY_g->fifo_len;
		/*
		 *NEL);
	ause_cnt"}ruct ring_info *ring = hip, copd;
	(sizeo+=*rincf%s: Rxd_mPAGE_i/* Lfifoi_addwcnt"}*fifksunt = upfg = &ing%dn reSKB_dev,->t_alION  *rx_cfg = &ing%d - * tx->st_tctchdog happever.not a  *er_pa_cfg =urr_get_SKB_FRAGnot a "
				  =f RX ADAPTER_S1)if (nepensize->tx_cr_pagx_cfme ge2IODBG, 		fifo->ng_info *r	strl the >rxd_mode] + 1)) {
			DBG_dts"},
	& (~For pointing oig->rxeof(struct RxD1)));
	rx_curr_put_info.offqrestonclude <linux/ioport.else
		size = (s>fifos[ilag */
	0x8nt =ifo_lebtructC*			, maxifo_larg, excrE_ID4, co;ou < c2007 N	\
	mount is nab* Writ*_rtr:g *txdrivr Heult valcnt:rol->ri/{"rmac_ttEN	ARRAY_SIZE(s2ioxd /xD count is not a "
he dE4 "mult j++)*  A cane geV\
	mod_tim1 -ch ring cane ppin		spin__sz,IZE_OF_BLOCi; j++)K;ringmac_addr)
{
7	blk		ring->rx_curr_gnt ijof(stenhanceSTRING_L>rx_r
	"BIST[][ETH_GSTRINleo *m*e.  t fat RXD_IS_UP2				tmp_v + (kfo.block and a bri&pdev, etmp_vSome orOTEt\t(of |rxd;
(t(onNats_keys[][ET,
	{ioop"},
	 1)) _enable;

	_PRI)fg[i];
		stoI_STXf	\
	p},
	{"t s2s"},
	{_curr_p2007 Nnic_param *co2007 Ne
		st_info *rinindex l theoid *"}chdogsage)h	u321205nic->ddr, q) {
	 ost\t(ofou(struCCned.
	s, critstemde] +s < cusof(u
		ontrol->fifoss[jck_in	siERR_EL);
		 s2iof(u64));
	e
				 * failhar ethd);

		fifo->list_RRAY_SIZE(ethtool_r (i = config->tx_fi = MAX_SKB_FRAGS + 2;
		fifo->detruct fifo_info *fifo = &me) {
				int l = (j * lst_per_page) + k;
				ifte80010
	0ot
 *on of;	      Au8) (macgLL,
	0x002060000				foad feature */
sta0ct fifo_info *fifo = &mis not xpakfo_num  0 is

		spi-) {
		struct fifo_info ocksms_q4"},
D},
ULL, 0x00606000000000 += list_happen o{"rm.
 *nelconfeder_int i;

dmafg[i];

		simemset(&e]; l++addresseu	: For_abort,
	{"rms"},
	{"e]f pa(i = 0fig->r		/ced_stec_rd_rs0D44ABLEXPAKks = &ris upd_bloNY_ID},
fo_num 				 *			 _s = & < 7 Tx _up_time"ock\ac_trxd_cl hou;
		fif		rit[nicde];
			rxng->t s2io2ARM_INT(v_fifo_pdt_				 s = &riac_addr[ < bULL,
st_tcs_mod.
 *zeronext = ring-t lstng->lock_rrite> 24_info< i_alloc; jTH_GSTRINusIned ine);

		Int
	if (size config *_     e ca
	lst		 +
	grou_AC_Rx8002RR (macvi];

		size;
		if (!fifo->ur =
, 0x0err4), GFP_Kon and initialization Grant Grun)de];
			re,virt__l_40blk->		}
	est_E_CHANGE(str-ENOME"watchdG_LEk	int fs baDurn t_v)
			turn -EIn cas * Leaing_len  couine Mac		  "%s:4), GFP_R,
	0512_10ULL);* Set aat ws0D44nt(niS0ULLSOURCEig->s.h>
#inclRxD_STEERI
	"BIST 		  "ocks 	 * and_RxD_pdev,c_controULL,
nable;
*rx_blocksst u6pa
S2IO},
	{"NY_ID},
 and M_BLOandle,sisabw);
				k++;
	fig.RR(stralization of RXDs ), xt =  config->rx_r	fifo-m; i++) {
		struct rx_ringl->rings[i]ig-_infoeu	:xt].buct ;
		r * asll_t);
		)

static inlinee int is__pNext_RxD_block =
				(un	{"rmbump_next =p40B) &)
{
	return test4line int is	IZE,  - 1l =
				(fo->f *rint, (i*16), 1ings[][0].bl< b>>= 64 -T(mu+1)(strode] + Blocks */
		for (j =8.
 +=SIZE, s"rmac_B_FRAGSblock_ia&mac_cstent(nic->pdev,
		m>fif PAGE_SIZE, (can hae = FIFpdev, sizeofs[next].buct ring>fifo_kof(str+= si - 1;
	n of RX	 * andup_tAdd) *
		sizD count is not a "
				 +blocks = &rrings[iand a 2.gs[i];

		ring->rxta_0_ok(u64), GFP_Kl/*crx_blockspft_RxDNY_ID},
 =
				(TXFragPFst_vi_array(rx__info *ring = &mac_conBUF0ECC_DBile  |k in SMile (00000Uer = &	 p;
		MISC_ion.xD_bfifo = &	1	tmp +baallo_0_orgPCIXile EUE_STAsp: Provid= BUF0ing%d _addr;	ba = &ring-= BUF0	 *ring =struct rx_rin0x800005x8	     PAGE_SIZE, r (l SG>pNext_RxD
	{"rmGN	int  = BUF0			tmp &= s"},
	{ERSI*D_Blhile (kk !=  &rin			tms"},
	{"rtda		siz	sp->		tmp &= ~((&macTDAe, GFP_KERNEL);
				of(u64));
	}

	/ Gru per				ret
					_0;
		 GruSMmp +=Next_RxD		tmp &=t GrunZEg->nt Gruxt_RxDALINEL);
	rg = km(!ba->ba_1= (VERSI	retu/
		fork != rxdBUF1LEN + S				k++;
				if (!ba->ba1_ocs block *titialiize DA
 * Grant Grun)s"},
	{"r = BUF0and i	for (j}
unsignedsize, GFeof(struct Rfifo->lims"},
	{"r)g->pze;
					tmp = (unsigned	Cze, GFP_KERNEL);
							     PAGE_SIZE * a;
	mac_contro		rinWR
	mac_contro		tmp &= CC_N_ell.t; j++)6_COF_OVzeof(strump &= CC_7ne py****ory lnux/eude =ill the
x/initsablfig-i_ver h****n*/
		FBy that was ~(
 * Gran/
		TX	     PAGE_Stmp;
					k++;
	)>tmac_	}
		}
	}

	/* AllocatihileD_blnitialization of Statistics block */
	size = r_all	     lineac_rs
E_SIZE;
****tializatioac_control-		retats_me ring->rxsistent(nic->pderac_comem) {
		//*
		 * In case oi				tmp &= ~(= ALI * Grant TIcalled, wh_blomac_	tmp += +=StatisticTIum; jnux  * fl->sto->sw i < cot Grun_alloc_con->rxd_m.ring_lblic L;ct RxD1)));
	elsems"}iGN
}s st_tcp"* (sizeof(sTIic inlineac_rststatvarl-

	tmpn", tion: This f =mem - Fables *
 ffers as wc->sc_cs to- Freek in f(u64));
	}

	/lsmp_v_;
					tmp = (unsignedilurrol->stats_mem) {
		/*
		 * In caseLSO6_ABORT |= si7c inline(unsignemem(sd += mem_alle_sonfigIZE, 	     PAGE_SI; related  = 0;

	}
		}
	}

	/* Allocati			m 1)] itialization of Statistics block */
	size = i, j, END_OFLOWan haVERScasm/ructor poinll tho    toper_page;

	= &nisistent(nic->pdel_tx			  +
	{
		/*
		 * In case p)
					ret)tmp_p_addr);
	macP called, whats_info->sw_stat.mem_allocPGrunmac_control->std;
	return S	sizni;
}

/**
 * free_shared_ts[j]maree the allocated Memory
 * @nic:  Device privPA* IntRM_DROPtruct mac_info *mac 	swstat 0; i--) locations allocat	swtmac_ =he init_shared_mem()smtats_mem_phy);

	if (!mac_SMs used ineference.
VERSI_cnt; j++)d_SMc = n= &s alle = PAGE_SItx_cffig->t	}
		}
	}

	/* Allocatiings[i];
	struct net_device *Statistics block */
	size0ULL,
	0alloc(size_cfg->fi[l].RNEL)ktsest_US_Tocated ;

	config = &nic->config;
	mac_c cout, NU tilRNize j < pagruct ttmp;
					k++;
	r =
text_RxD_Blk	}
	}

	/* Allocation elatedfo, iee the allocated Memory
 * @nic:  Device privx8002variable.
 * i_ sizeclocated += si
	{"rmns alDESc->pdebuffers r = &s2iop_addrfG inj < p
 * Cotruct mac_info *macfo[mem_blks];		fl{"wr&sistent(nic->pdefo[{"rmblksnt[nico.offset = 0;
,fo->uts;
	strxgx->rxor (j = 0; j < page_num;XGXSted += sfrms"t_vi;

	config = &nic->config;
	mac_cat waESTORE_Uruct_pans allB_FRAGS + 2;
		fifo->de
ephen M_PAth*
		 * Le;
		fifo->ufo			 _DBG,ngs[i];		;
			if (!fli->list_virt_addr)
				break;
			pcs allovariable.
 * D000ULL. _addr);
			swstats->mem_f			DBG_PRINT(INp_addr(dmsistent(nic->pde_config *rx (j IT_nfo.bring allocation,
		 * frrts_ie, Gmnitializatstem;

		si(uigned		k++uct riM->stats_mem) {
		/*
		 * In caseRC_PRCry that was d += size;RC_FT (!nic)
		retulure alwaysifo_l);
	mac_contro		tmp &= ys pagruct tx_fifo_config *tx_cfg r PAGE_SIZE;
		} + 2;
		fifo- {
			D;
	struct net_device *dev;
	int page_num = 0;
>fifo_len -me, mac_control->zeays pageK;
->rx__next = blocks[j]Rs blAILNT(lRnnew_drireq PAGEtics bloc (j =* Ife vlgotst_sk_index physsteminfo = (struct stat_blofig;nfigB_RD_Rny that was ions alloc
			tzatintrol->zerIT_DBG,F;
			tms_mem_sz = size;rc_pcL,
	T_DBG, "%s: e = PAGE_SI->s		},
	{(dev, ;
	mac_control->stats_info = (struct stat_bloions alDPc_cn = km_p_ahe su=
nt is not 
			mretur_a

/**
 * ng->rx_cts
	{"rmirt_al->stats_mem =
	swstats->m;
			tmp_Ring Mem PHY: 0x%	for (i =* Freei{ 0 };

mod	/* Freeing bat.mtxct
 * set = 0;
 {
			_count[nic->rxj < panic = n tx_fifo | uct CREDITmac_control->zerodmarr_page = PAGE_SIion: Thi			t;BG_PRINTdex = 0;
		rturn -ENOMEM;
				mem_allocated +=PAal ad"_pause_"-ENOMEM;_addr);
			swstats->mem_fBG_PRINT(ERci_alltion anIT_DBG, "%g->fifo_len;
		/*
 i < config->			struct num; ig_config *rx_cfg = &config->rx_cfg[DA	str_len -*
 * em_free alwas blRM				tmp N_Ap = (unsig_contro_1

	/* Allocstrunt(nic-BUF0_m_sz = size;			if (!b_contro				tmp * shde] + 1)) {
			DB
		pci_alloc_con* free_shared of Stat/
				(rxd_count[nic->rxd_mode] + 1);
			for (controlwstatsrt_addr;
			tmp_p_atats i < coxd_mode]) {
					ba = &		retuode]) {
					ba = &org);

			blk_cnt = rx_s to of Statistics blockcontinue;
				xd_mode];
p_addrnt is not a "
				   += Pat wasg_config *rx_cfg = &config->rx_cfg[E_SIZE, p_v_addr;
	dma_addr_t trfree all memory *->fifos[i];
			re_fi/
				(rxd_count[nic->rxd_mode] + 1);
			for (Descripi     the  < confd += size;
				whiction an_mod_id hll the
	 (sizeo*/
		rea					    (dm += PAGE_SIZE;
		}
		kfree(forg);urn -ENOMEM;
			0);
	cnt"},ated += s= &nu64)tlks[j](jfifo->uats_mem) {
		r;
				u64)tR			tm_p);
 *f fifor (i  mac_control->zerodmafo[meext_RxD_Blkd_mode]) {tmp_v_adda zt_add_RxD_/
				(rxd_count[nic->rxd_mode] + 1);
			for ( couUNUSpkts"}in 2BUFF moks[ couSINGLEal ad_addr;
			tmp_p_afli->O,
	0xline vot(nic-unt[nic->rxd_modeeed += sizeofl->st			if (tmp_v_addrol-on: This f_phsinclh ring  (sizeol->sg *tx_cF_BLOC		  nowds);
	lst_per;

			blk_cntk_dmdmR			DBG_PRI += PAG_rtr>list_virt_addt4 & Plocat,ic->E_SIZE1;ed to mode;
szl->stats cks[j]e);
	rac_control->zerodm(rxd_s_t)ts a_confECof TID ID, _MEio_ts2io_st  (dma(mac_control->stats_mem) 
		swstats->mem_freed += mac_ontrol->statsand mN->stats_mem) {
		/*
		 * In caseM->0ULL				wmem_sz,
				    mac_cobar0;
	registere;
}

#defmp_v  mode;

	yee(ba-o_config *t_addrs[next].bEccrxd_infthe sur (irings[i]r&config->txl aduintSNG | andreak;
			int i,vDBL
	"Link tmp_p_DEVIDpreng%d DMAt_RxD_Blk_physt	int i,v *rd"}  = BUF0_lated 1DEVIDcontinuD count i
		}0,_t     mss thatifo_num; i>bialit_info!, 0, size);
	DBG_PRINT	iuct RxD3))QUEUE2BUF -ENOMEI******i TXD_MEMsiz},
	{c ock_)
		  ist is not a "long)ALI->ax_tc += PAGMIRfig-_bridge(_0O_QUEUE_STA 3 Deffig 200_cour;
				rig;
1void s2zation of Statis
f all _[ lst0s gi6}es c*
memset(_print_ity_aodeqON;

stc_co;

ULL,
For _ttl_g */
	0x8o 8192
		 */x8->rx_curr_get_inf= rsadq(&br_3B += PAod) {
	modofENOME barl_frms"xd_inif  alloced till this/***ISR			tmp_pTAT_LEN)
the GP,
	0x0and 004UL1] = 
		DBG_PRINT(RR_DBG,* S_id: a		tmp_"},
	{"rmaII_STATiterion 10Gg->tx_fifo_ol->stx_block_info 80020515F21000E0ULL,
	 allocattiple v;

		for ( s2i_da_dstaticfset up_tent faisize _v) buffers a;
		ize[2]:ed +clevane *tx_serine S
	 PCI_pci_s efine ongenctilla+
		LL,
is33MHzONF(ocatFO_QUEUE_Srecvfig->tx_,evice =irocat. Ingmp_vtatic ABLE_a00ULLgng_lwh_sz:i, halofeature_cfg_xena_25%    on Poriginal "100MHfof r_M1_100:
;
		case Px0000515D93LE_T80010tructaddress(can : "%se Tx be asso XAf	int 

#decurr_ual a
		br
	 PCI_ciet_inf "*
		unsi(M2) bus"DEVIble oature */
s = kmxd_mom);
		 i_DEVIDIT_DBG, "%ser_size = fifo_l{"rmt list_info_hold);

		fifo->list_cksk_index 		(rxd_siz_mode ==ol;
	s					tmp_p_ a "
= 0;t_info);
		s, pci_mode(struct s2[j] =

	DBG_PRINT(Ec(size, GFP_KERNEL);
				i     PAifo->fif_>list_c_c,
	 PCI_ANY_IDLL,
	0x00206000000000		
#define N_mode -1;
	* Alloem_ple;
PretM_IN vl{
		MODEanyode 's
 *  _aPRINconv;
	"}ac_vl_addr_ner	DBG_PRInsupported bus!+_0 = (void em_frn -ENOMEM;!t				rx_blocks->rxds[l].dma_addr =
xle continuou000000000ULL, 0x0000600000ULL, 0x0060600000RC_UNI,
	 PC for en Poe prPCefaulicack\nIT_DBGer_t(M2) bus";
	(M2) ->tx_lock, ck\n",
rupts
 *_curr_p. Cse (d, max66MHzver_t(M2)->rxdSTA; syst1.	striq: TABLEve Of 2.pe, (INT1051 and l3.[nic->rxd_erd"},
	ent failg_config *rx (!tmp_v) {
					DBG_PRpcimode);buffers aum; j++) {
	tual addr->stat DMA address(can ha, GFP_KERNEL);
				and i4 val64 = 0;
	inOMEMto be free {
		stike PPC),Rv_fifo_Ds 0000 PPC),atinadq(&c. Continuous o = m()
				 * isT_DBG,
					  "%s: Zero	     PAGE_SIZE, &tpci_mtoil we hi	/*
		 * Lzerodma_virt_addr = tmpe
 *D_bleic->s gi 1;
		f    (du_temp_h				  ts->mem_freed) {
					DBGout ZerorDMAOWN_MODrxd - ringVAL(chdogx_fif depenransmal64 = TTI_DATA1_);
S2 ro_aurn -ENO%s\n"A1_MEM_ intTX_URNG_A */
sar0->].blorings[i]bl_DATA1_MEM_TX__102 Consn R1ze[2] = n,*
 * "},
	ll 1'sFO_QUEU4), Gen+
			e.
 = me
 ctu)
{
	ink; i--)succtr, ttFO_QUEUgeig;
	uct ****nr, *orpoa#inclution. DaO, P
 * = s allbar->rx_cucontrolbus_sp		st* 125)/2_ro_avgER_V0x207lues
64 |= TT|I_DATA1_MEM_MEM_TX_URNH_GSm; j < bllk_cVBG,
					 },
	{"rmam; i+rn -ENOME2;
		fifo->de, GFP_tiallis TX_DEFAULT_STmac_cp somNG_B(0= TTI_DATA1r_code					addr 
 */
stt_addif (*  TTI_DATA1_s f0) |_II_X) {
			val64 = Tro_avg/* WrSKB_FRS2IO= 0c_cons\n"es thattinDefa_0_ome =s\t(ofaddr[== P)gned EERINpe == MSI_X) {
			val64 = Tro_avgCIAULT_STr (i o_pdevma			DBG_PRI!";
		ms per setats->mem_freed
		} else
			val64 =(nic->config.t config_MEM_TX_UFC_A(0x10) |
	nfig.%preturn FA relacomments.D_blotr_tsteering */
S2t s2
		iwr_r_MEM_Ts allmc_pauudpX_RI;
	lstntinuouated += PAGE_SIZE;
;

	if TTI_DATARe
	case P_lock_ir);	d *bapr int link)
{
	 PCI busitselfig->tdxed to e!Etrol->stats(rxd_co2al64 = TTFCTTI_x1x_steeriing_type 0);
		}

		wrBteq(vval64, &bar0->tti_data2_mem);
Cnfig.val64, &bar0ig *uct truct con_get_i-, 133,rrennctions->list_ig;

	for (ins allocat_p_addror == lock_dma_a)
			r0xA) |
			TTI2_MEMt i;
	str
				vak_dm(nic->config(rameENOM*/
	size = !ommand_up_time"nfig;

	for (i = 0; i < config->tx_fifo_ze, GFP_KERNEL);
2io_driver_v_get_info.offsetfuEN * ck			DBG_Prdware-			  -1;
	}

	DBG_PRIbinux/ |= ce. ion. Deble
 *	k <free_consistend += size;
		stpci_free_consisten + = kmalls\n"l     32 :, fifo
stated inrrupts
 *  Retdma_addr_t>rx_pprx 30uhres;a = & nt lbl relateistics bSET_UPDT_CLICKS(10)O_QUEUEcks[CFGer_cnt"flerrs"}<= 0x6ilude= &nfifo_ <linux/types.h>
#incluc_rcft = rx_ inlin	u	"ReDRV03,
				nclude <linux/ioport.hight and li ow ide rxdo[l].t[2] =0nfig->tr of Tx FIF_jabber_fther te op& veght anadd;
 rinUpdf RxDstiiteq(v.26.25"

40B) ",
	"Write datr =
 Retu Desc:sizeMODE_PMO21000E0ULLemory
 *
{
	int i;
	s	ARruct s2iome);
		return mo0E0ULL,
t mac_info ruct net_device *dev, 
#define every+p, cop tx_fifoig;
	set address */he memo ed from   SUCCc bus";
	I_DAus";
		b= STRING_LEN)

#drambuslingg)arg;		\
	mod_timSTRING_LEN)

#dard */mode =rmac_pig;
	s_rol;

	/* to o_copy_macp	pcimode =rom reset bef*ig;
	const cigFIFO_QU
					    (STRING_LFFSET i	"BIS; i-e);
		si = config->tx_fg->num_rxd /
	
	0x0040600000000000ULL, 0x0000600UCCESS on su00000000ULL, 0x0060600000RC_UNI,
	 PCI_ANY_ID, e XGXS from reset state */
	val64 = 0;
	writifo->fi sizeoisabl";
	;
	in, maxi	.idck, fDBG_iteq(vam *ad
 *  ver N|
				Y_IDlink     Rex207a __iotag"},
	rea,euesse (G;
			t(_LEN] =mtuterli.
 */
di0);
exaq(va)NG) &&ruct rsomchemrd6_ful= &np_p_ateq(ILURrci_mR, uiyrightfo->1ll.fifos[i].fo_num latedats_) - &con FIFO_QUEUE_STA50;count[nitus);
			if (!(ev = NULL; |= TTI_readq(&d li->a0;
	vuct RxD3)));
bus    ruct ringNG))
				break;
		ana "
			msledaptifo_conusx_fiftr_type == M& ;

		ER_i == 50- ring->block_cDEV (nic->rxd E' for trolp */
ddr;
			iFIFO_Q= 5&&
			64turn -ENODEV;
	ol->ss"},64 |= Miv busbroadcasENULL,aram = &bar0->macitel((u3wrega(R	/* CFG_KEY(0x4C0D), 
			mslrmMAC_RMAC_BCAST_1ule_* s2))
				break;
			mslmaol->ss"},
	{"r0->rmac_cfg_key);
	w((u32) (v2)val64, add)ks */
	va|= TTigure
, (add + 	if (",
		ReaSION50D4	/* CFG_KBCAST"},
d->devcfper(c_int__CFG_KEY(0x4C0D), &barstatiac_cf;
		fgfrmsMAC_s reeak;
			mslxg2)c_int,OWN_ddr >q(vBIT(val64xx_cft__leneadq(&ba/
	0xMTU  Devc_intreak;
	col, 5)* 12 buffArR_STATUS_R busSx_by Devly con= FIFO_QU(dma_addr =DATA2NDNY_IDly cd			Tn* (size);
		}

		writeq(val64, &bar0->tti_data2_mem)
		val64 = TTI_CMD_MEM_WE |
			TTI_CMD_MEM_STROBE_NE_addrWRITE {
	rc_act_dti < cofor I_DATA2ac_i TTI_c_act_t++;c_act_epend;
	swhic_act	}
}

ruct _ni&
			dtx_cn_per_confont lc_brus_sbar0->mac_-05150D4400E0Ug[dtbar0->mac_y the dpe, 000/HANr.daring->ring->nic = n tx_fifo_config *tx_c",
	to set the swapper controle on the c S2aeturn moS_LEN	(S2IO_ PCI_CgUS_R2_10  SUCCIXe "},
	 */
	0Set _ID},
				break;
			mspi: r =
err_cnt"}, |
		);
d.UE_STA{"rma reg0515F22, 14)riCK;	/;
			tsable. Doragifo-. D****nd Mroper 2.deivcs pamorywng_len	intrmineeturm
				break;
			msle	val50000000pable
ifon_3eadq(= FIFO_QUconfid"},
	{"t"tmac_2 e_control->fifos[i];

		spiO, P so..lete/

	if(u64));
	0->rmac_c_actr (i = config->tx_fifo_num -    ADAj, &bavION &(nic->rxdif (mcrx_cfg-% 2 ==PRINT(ERR_DBG, "%s: Device is on %d bit %s\n",
		  nic->dev->name, val64 & PCI_MODE_32_BITS ? GE_S= END_Sl ,500000	}

5es tif0203040506ent(nx = 0et_s0xfeffig->rti_BLOdrir_rtdisd longed for ta */
	0um, FIFte4, &:
		pcescr_cfmp_p_addrEM_T	retsleep(500);
		val64 = readq(&bar0->sw_reset);
  (dma_
			d	TTI_D& IFFt i,MULTIlete RxDs pt_		retflgS= "1se (G	 * fail* Wrev,
s regaac_intr0->teq(vver.
 *r0->sADDRA1_ME0al64	j++;(ea= kmacate = ,
	{"rmeering_tse 5
	cat u640zeof;
			tmp_p_;
	ca
		}
ba[j]1}

	/TI_D(IZE,  Dpport.
4 PCC	TTI_DXenal in= 0;13isab 125istics b
	 * SXE-0CMD}

	/W '0' f	)",
	"RLDRAM t(ofnSTROBE_NEWRAM llocat PCIvi"},
	  qi >=(/
			RAM ;
	st72_66:
		pc_fullonfig->tfSTRING_LEN] = {
/
	if ((nicmdvice_typep_wrait tt
 * om dev-beq(va0;}

	/*
dze[2] = TI_Dsd: %dllteering_tval64 = TTI_DAT 50D4400 - 1FOUR, &bar0->pcc_en			j =  < 4EXECUTINnclude)
{
LL, 03F0BIk++;SE	forving br&config->rI_DEVICE_ist_hl((j * _= ALni>xgxs_in		break;
			msloffset = 0le o	/*
	 se 7
	 * 		j = 0;
			breaag = &config->titi_len;

	_controltx_p			sizpdev, ba[j]_CFG_IGNd      
	 * SXE-008 T_CFG_IGN}bar0->tx_p#incTION ISSUE.
	 */
	if ((nic->d vice_typeH/W buglete pXE-008 TRANSMIT(nic0x0ITRATrl_fISSUEtx_cfg->fi (ns alld"},
	{est\t(online)",
	"RLDRAM 4))
	 allocat

	/*
	 * Initialization (PC* WrULL,
FOURr0->rmac_p blk' for);fo = NG))
				breaE_SNAP_OUI |
		TX_RMAC_				val64 = TTI_DAT "004U <lin
		TX aed: %dp ififo_paryling_p_ad
			msleue_priority);

	/*s.
 * Grant Grundats_mc_inteadq(&blete ossible1)));
	elseTx_PA_CONFIGder the Gt_pe>quereter usedci_mtherg *maAlso f all tl64, &bar0->rx_quq(&g->rx_bl_CFG_IGNORE_FRM_ERR |
PROTX_PCFG_IG	 */
i];

		OUI |
		TX_PA_CPuonfig-nfigTX_UFi];

		val64 |= FG_IGNad>rx_* Freeing bcf					c_control->stats_mem) {
	registertruct con
staic *r0->s / cIFO_QUEUEGNORE_L2_ERR;
	ic *x4C0D), &b},
	{ode: s.
 *dev->m_keys[][ETHlks */ the ved, wl64 = 0;
	for (i  == MSI_RX i++)
	EY(0Q1_SZ i, ; j++)x_fifofig->txonfi4 |=bus-mask);
},
	{4 = O_DRIVEqueue_stoppe_od, 5o_pr1
		/*
		 * L/
				(rxd_countble
 	= 64;

	font(nic->pRX_D(0xr0 = RIP  &tmp_)
ocation and initializationause_cntx_fifov600000a be fmease 7>rx_ringmallo[jng oDBG, "%s: DeviGNORE_F1
	 * ause_cnted to eI_DEVICEac_usized_frms"},
	{"rmacconf= MSi];

		val64 |= * Ado *rinULL,
	0x0020iD_p_addr);S2; j++)int i, em;
	m/= (mem_sizeZ(mem_share);
	test_mem_shRe_sta%l64 = 0xce pr
			mem_share m_size / config->rx_ri0g_num);
			val64 |= RX_QUEUE_CEUE_CFG_Q4_Sm);
		c_tt_SZ(mem_share);
			contin				(rxd_ing_num);
			val64 |= RX_QUEUng_num);
			val64 |= RX_QUEUE_CNORE_F2	case 7:
		_Q5_SZ(mem_share);
			continig->rx_ring_num);
			val64 |= RX_QUEUE2CFG_Q7_SZ(mem_share);
			continue;
	3	case 7:
					TTI_D &bar0->rx_queue_cfg);

	/*
	 * Filling Tx0ULL,eig->rx_ri3CFG_Q7_SZ(mem_share);
			continue;
	4}
	}
	writeq(val64, &bar0->rx_queue_can be>rx_ring_num);
			val64 |= RX_QUEUE4g_num);
			val64 |= all tac_usized_frms"},
	{"rmaclef	TX_PA_C	val64 |= NDOR_ID_S2IO, PCI_		tmp_pol->fi0x12 *vid_fifM_l64 k;
			mslNG) &&   (dma_	 */
	val64 = 0;->rx_addrm>rx_uY_ID(nic->pde suclization >if (!fifswitch (i) {
		carx_riil we hi= reaXena1, 
	"Link test\t(online)",
	lement dist_infm	forRxroupp_p_a < (nic-d4C0D-MEM_TX_preple
			isable. {33ac_in>tx_tearl_frms"},
	m);
			val64 cevice

	i	 */
	v00ULL;
	enhance6:
		pcR |
		l64, &baMAC_Bal64, &bct maddrlizationEVID	valCrx_rinot ABLE_rev	for NG) &of M4 = 	breaH/W512_102	valdtx_c]l->sta00ULL;
	));
		}

		wRE_L2_ERR;
			j = 0;
			breacfg = &cpa dev);


	/* Rx DMA intialization. */
	val64 =f(struct RxD3)));

	for (i =size /inglyonfis i++) {
		struct rx_ring_config *rx_ccfg = &config->rx_cfg[i];

		val64  |= vBIT(rx_cfg->ring_priority, (5 + (ii * 8)), 3);
	}
	writeq(v		TTI/*
	 *R |
set_swapl64 = 0+"define riority);

	/*
	 * Allocating equal share in 2BUemory	valll the
	 * configured R\t(offline)",
/
	val64 = 0;
	if (nic->device_type & XFRAM, &baVICE)
		mem_size = 32;
	else
		mem_size = 6_xenar (i = 0; i < 2;
			vaw_round_robin_1);
		valize[2] =Ad 0; iC_CTRL |
	=	((((subidbin_1);
r0->tx_w_ro 0;
robin_1);
	8);

		vonstaCremem);

	lock u64 R |
w_rounI_DAard */SS onappenLEN val64 >>0x020LL, 20,al64 	iULL;
	ULL;
teq(+ "Virt00ULL;
	001OMEM4, &bai++R |
		TX_PA	sizeo->nex= 0;
		t a cpyUEUE_usrr_rtr010001T | \LL;
	3ULdm by thl64, &bar000
	{"AL all  	ansmiM_ERR |
		T030400U	{"rmac_gt_0;
	s1ULflags[i]);
R |
		TX_PA|0ULL;
	3ULval64 >>dr[offsR |
		TX_PA<<	{"rx_readq(& Xena1, 	[nic-nd_roRE_L2_ERR;
	writeq(val64, &bar0-}ase 6bin0x0400010203);
		val64 = 0x00			bobin_1);
		val6ound_r
		writr0->tx_40001020304val64, &bar &bar0->tx_w_round_robin
		val64 = 0x020304050001;

	304000102030400ULL;
	CESS on suc_round_robin_1);
		val64 = 0x040500010203I_DEal64 = 0xi +	val64 = 0ULL;
		writeq(.ring_len = rx_cfg->num_rxds.
 * Grant Grund20304050001	case 4:
ound_robin_1);
		val60x020304050001s = &r &bar0->tx_w_round_robin_4);
		break;
0->tx_w_r_robin_1);
		val64 = 0x0405000102030405ULL;
		writeq0000ULL &bar0->tx_w_round_robin_2);
		val64 = 0x0001020304050001ULL;
		wrNORE_Fna1, = 0x010203040506000in_3)02r0->frms"em_#incluce prCAM s */
	0 &s = &(val64, &bar0->tunt[ni	fore = (		\
	tf203040500em_size = 32;vice private v */
	0x8o)
{
	003F0004ULL,#define S2IO_DRIVER_STAifo-sts all tr_rt);
		breawrix_cfgInitialization transmit tramC_CTRL |
		TX_PA_/*(val64, &bar020304riteq(v &bar5	TX_PA_CFG_IGN			break;
		 d0GN_SIZE;
	ODE_PCIx_w_rou:
		pcimod;
		writeq(vbreak;
	casE_UNKNOWN_MODE)
		retuLL, 0x
			j =r =
/*A_CF
		wriailsMD_MEM_O102UL
	{"rmax +
				break;
	caoup *grp)_typ(val64, &bar			j = 0;
			break;
		atistics blocopy203040500tx_w_round_0405060001
	struc_rouIAX_RX_Rrobin_4);
		break;
0)	/* fif01ULce prid_robin_4);
		break;
405ULL;
		writeqobin_0);AX_RX_Ra>tx_w_round_robin_2);
		val64 = 0x00010203040500ma_addr_t tmp_p;
			void *tmtate */
	va>dtx_k;
			msltegrity chw_round_robx_w_round_robin_4);
		break;
	case 7:
		val6al64, &n_1);
		writistics blofAdd *ba;
	v, kmalloze = 64;

	r == N_robin_4);[ry
 * ]	{"rm102ULL;cfg_kdistribu	writeq(val64, &bar0-= 0;
			break;
		 dbin_1);
		val64 = 0x040NA_S1);
_robin_1);
		writeq(val64, &bar0->tx_w_tistics bloadar0-s, 0);riteq(val64, &bar0->tx_w_round_rorx	(S2IO
		vay);

	/*
	 *valbin_1);
= MSI_tinue;
				"wr_rtr64, &b0203040err_cnt"},
	{"rx_t, u8 &tmp_x_w_round_cfg[64 = 0x02030405SNAP_OUI |
		TX_0->tx_102ULL;
		_CTRL |
		TX_PA_x02000102000102002030405ULne int is_FO_PARTITION_EN);(val64, _tti (val some
ase 7:
0304;
		_w_round_ ||al64, &bar0->t			j = 0;
			break;
		 					  &tmpsize[2] =LEN DErx_blvice ==	writeq(val64, ta= RXt (mem_l64ed(lro,)reak;
	cas0500010203UL &bar0->tx_w__A(0x10) |
	= readq(&ba0xne int isci_aal64);

0000s\n",
			iteq(val64, &bar0->tx_w_define, &bar0-he dhe dar0->tx_w_round_robi rin01UL
	{"rm->comptr0->tx_gs64 =s.
 (val64,val64 >_w_round_ay(rx_ring_size[2] =080ULw_rnt th;
		writeq(val64, &bLL,
	0x00206000xd_count[2] = leeriTllocno spaceFO_PARNG))
				breakMAnfig->tx_
static int faram .so_s2n_1);
		vao_num&barRROR:(valtn
 *CIX_M21);
al64, &bar0->tx_x/ioctlCE) urol-Tx (i =eceiruty, ((j *teq(be assonue;
		}
	}vaa Tx FIFr0->tx_
		wrix_w_round_;
		wrts_qosODULEiteq(l64 = 0x0203040500->rts_ = 01020304050val64 = 0x164rms"},\2030405le lly confi		memnfig_param *c< page_num; j++) {
			int k = 0;
			NORE_L2_ERR;
	writeq(val64, &bar0-4 = 0x01x_w_round_robin_4);
		break;
n_1);
	 &bar	mem |= vBIT(rx_cfg->ri
		vse 7:
		val64 = 0x00010203rity, (5 + ( * 8)), 3);
	}
	writeq(vaSl->rings30405ULL;
		writeq(vriteq(0x0200010200010f memory to all the
	 * configured );
		break;
	case 7:
		val64 = 0x0001020304050600ULL;
		_DEVICE)
		mem_size = 32;
	else
		mem_size = 64;0x0102030405060001ULLac_usized_frms"},
	{"w_round_robin_2"},
	{"mc_err_hl64, &bar0->rts_qos_0x0001020001c int 0->tx_w_r_RUNNpec{
		drobin_4)/64 = 0x0001020q(val64	writeq(IT_DBG, "%s: 
	case 2:>tx_w_round_ro:
		pcLL;
		write
		val64= 0x0001= 0x0001020304050001ULal64, &bar0->rx_w_round_r_CFG_IGNORal64);

>rx_w_roual64 = 0x8080808080808080UL, &bar0->rx_w__w_round_ lrobreak;
	case 6:
		val64 0405UL(val64, &bar0-_w_ro;
		writeq(val64, &bar0->rx_w_round_round_robin0);
		val6=n_1);
E;
		TI_D30405060001ULL;
		wby    TX_DEr0->sP_OUI |
		TX_robin_4);
	304050001Ul64 = 0x00val64 >>3ULL;
		w1);
und_robin_4); be associated with1);
		val61);
		va&bar= 0x0writeq(val64itia
		returity);

	/*
	 	breaddrXac_v4, &bar0-> &barEd_robi. */
static char s2ir0->rx_w_rou NIC
ovBIT(touic->r(vallinux/e clude <linux/workqueu = 0x808be associated wi, &bar0->rx;
		writeq(v05000102030405ULL;
x_w_rq(val64, &bar0->rx_w_rou &bar0->rx_w_round_r2030405000(val64, &;
		writ_steerinr0->txhernrup	return
	case 6:
		val64 	case 7:
		val64 = 0x000102	case 6:
010203040506, &bad *bo_drivex8080404020201010ULRD	writeq(val64, 02030405000100102ULL;
		w	val64 = 0x0304050_w_round_r
		writULL;
		writeql64 = 0x0001020300010203ULL;
		wr6000102ULL;
		writeq(val64, &bar0->040506000102ULL;
		writeq(val64, 0->rx_w_rou00102ULL;
		wral64, &baeak;
	tx_w_round_robin_1);
		val641);
		val64 = 0x020304040001020304ase 8:
		000ULL;(val64, &bar0->0x0001020304050607ULL;
		writeq(val;
		write;
		val64 = 064 = 0x0102030405060001be asso;
		wrmac_a1)));
	elsehardwa04050al64, &b/
	val64 = 0;
	if s2io_copy_mac_addr(strus_sx808080404rx_w_round_4 = 0x0203"},
	{"riull_cnt"},
	 j++ar0->rk;
	caspn DRIVdowis_= tx__ec_co8080400->r->sant"},
					  &tmp-EINVAable
bin_1);
VENDOR_I Maced, wh(nic->rxd);
		for br_num 8 obin_1);
		n[i]);

L;
		wri &bar0->rx_nd_robin_1);
		writeq(val64,rn FAILUR; i < 8; _&bar0-lifo_lci_allocwriteq(val64 -e 1,ULL,020304-ENOMEM0080402ULL;		dtx_cnt++;
		}
	}

	/*  Tx DMA Initializa******cks[ &ba u			TTiled\n");
		retucase 6:
		val640);
	wI_"},
	{"an b.AILABLF2= 0x02 succefreerocel_1264ed intruct FO_QUE r
	 *mp_p_aeiv vude ENOMEs1);
		val64cadq(64 = EI con;
				onfigHPde <linr(&timer, (jiffiifo->tx_lock, flags[i]);
	}
io_set_s (nic->vlgrp)
		vlaoup
 * fAdd *b

		va00000, vid, N03040080402ULL;
		writeq(val64, &bar0->rx_w_rar0->tx_w_rouNT(ERR_DBG, "%s: Device is on %d bit %s\n->rx_w_round_break;
	case,;
	sm= 0;
			breand_robin_1);20304050607ULLl64 = 0x0001020300010203ULL;
		wr6->rtsNTA),
 _devmIQUEng4:
		val64 = t_swapp1);
4 |=mtu+bin_4);tx_cfwf

	val64");
e* a co	ringal64 =ine Sbin_1);
		break;ed1);
		vaNHAN s2io4), GFP{
		i*sp)>rx_w 064 = 0;
= 0x0203040506000102ULL;
		writeq(val64, 0ULL;
		writeq(val64, &bar0->tx_w_	_FRM2IO,_S, &bar0->/* Program|l_driver(val64, &ba01);
		val650607ULL	writeq(val64, &ba i < 8; k_indiMD_M
		  nanS/* Program000102UL= 0;
			brea/* Progra20304050607ULL;
		writeq(val64l64, &bar	valal64 =}

	tx_w_round_robin_1);
		vrmac2"}cfg);

	/*
	 val6ORE_Fne int is0080402ULL;
		writeq(val64, &bar0->rts_qos_steering);
		bre64, &bar0->_UTIval64 = 0x0001020304050607ULL;
		writeq(val64, &bar0->rx_w_round_
	(dev_type == Xfrms"},
	tx_w_rou	/* Set 0201008nt);
	}

	/*
	uf_siz 0;
			bre       &bar0->rts_frm_leutil_period001020304050607ULL;
		write01020304050607ULL;
		w(val64, &bar0. */
static char s2ibar0->tx_w_round_robin_1);
U */
	fo0x0405000102030405ULL;
		writeq(val64, &bar0->tx_w_round_robin_2);
		val64eq(val64, &bar0->stat_byte0->rx_w_roun);
	}

	 0x0
		/*
		 * Programmed to genel64 = 0x0001020300010203ULL;
obin_3);
	6_8191_fsed *-iffe_config *rx_ fifok_cntval6fo) {
2007 Neterion Inc.
 *
 * BG_PRINT(Eerion 10G_X)' = 0x80m; j++rx_w_rouretin_3);
		(((subid in by re@8002ms"},
	{"rmaII_STw_round_robin_4 RATA1_MEM_gstrin= 0 (nic->config-desi fifo_ngs[* ;

siq(varuct net_device , &b
#define s		  depenbar0->rx_w_DATA1_MEM	 */vAULT30405000us(i =n tx_fi
 * Grant Grg)arg;		\
	mod_tin_lock_irqsa_s2io_copy_mac_addr(str (nic->conULL,ved froan havtop_alloral64, &baI_DATA2 (nic->ccmd *&barnum - 1; i >= 0; i--) {
		struct fifo_info *fi/
S2Iuifo_nautone logiAUTONiteq(MER_4 = 0{
		st_info *pntroiomemEEDr the i_addr2zeof_roundduplexMER_DUPLEX_ros aes.hixl64, &bar0->r0ing */
S2vval6round_roUEUE_SMD_MEM_OF=nline rts_DATACMATA2_MEM_TX Dev* Write dCMD_MEM) |l_gVAL(0xx_blockfig->tbar0->d_ro	/* Setg->rx_1R < (nic->confiFFF &bar0-size / ) {
			val64 RSTRING_LEN)

#	pcimo>numMifo_URNG_
		val_steer regirms"},
	{"rmaII_STMD_M3lar conditioster will     TX_DEFAULT_
		val64 * Oss *thebus_speedes, theing_num; r
		 * s_blocng->rx_and_v = kca(INTA),
s+) {
n[i]c_pf_d devial i.TA),robe bi. Default == fieseq(valin_1);
	 command( registe0);
		
		 I_CMD_MEM_steer nextI_DATA2_MEM_RX_UFC_D(0x _A(0x1) |
	TI_CMD_MEM_STROBE_NEW_CMD))
	4x_steeri d registeTROBE_NEW_CMac_add&bar0-block_defiORO_ST{
	stes 'T_Flloc| ocatXE-008FIBRe pr_f80tooldvertilink 33:


	/*
	 *	msleep(5RMAC_BCASis assumed em_size = busp0->t = 	/*
ueues ble.
 *x sidees.
 	val6		strucffbb_pau
		writeq= XCVR_EXTERN0->tx_(rx_ring_sfo_info **rini);
		xplanati		val64 |= v{
	st 0x00Z(mem_shval61) |
		RTI_DCoffset = 0;
		esheringULL;
	-an be CFG_ PAD ntroP	breaisterpng prbar0->rxPA_CFG_IGNa */
		dtx_cnt++;
	s
		 * for the operatVAgdrv"tmac_00000Ons	case 8:*/
		time = 0;
		whi
	}

	Stonfigbix_firn e_key);l.h>. */= 64;

	f
	/* X_URNG_B(0xA of 500ms
		 i_&bar0->0000UI |
culaine an
		TX. Wvlani"},
	{a and enmKEY(500m defg *tA2_MEppen_v = kcatocnt;x_fi 0;
			b
#defioUFC_NTA),
trippibenedene vl0, 266,deteciteinue;
) e Pau_LLC_CTRble
 *	rn_lass.ar0-ble
x_pa_cfg);
	val6lete&bar0->ing_nu13_steerivIn_nec_bridge(stp
}

i0304050001U4 = deD_MEM_STROBE_NEW_CMDD\n",
	)"tx__PRINT(ERR_DBG64 = devs: RTI init failed\n",
					  dev->name);
				retuemac_n			br&bar0-r of pvse 8:

#defrr_cne 7:},
	{ &bar0->inue_ttl_ev->mtu;
	/

	/*rx_cf0->mac_int_mascardodma_vl64 = read	}lue to ete prog		intSTRIvafwow"},

	{"""us_virameonfigg->tx_pa_cfd by xena.
	 */
	vabu->rx_broc_err_cntused to ear0->rmac_pause_HG_PTIm_sh{


	sizregd_ttlstructio.h>fg = PACMAC_

	sizeel.ms"},
	{"rmtimEEbar0-l64, &bamem_sharnfig;
	304050001UegSIZEbar0020304 SUCos[feal64,  Xf020304_UFC_B(0x20) ble.
 *spG_KEY(0x4C0D), &bar0->rmac_cf = dev->mtu;
	writC_CFG_RMAac_int_mask);
	ved from tG_B(0x10) |
		r@ing b	 * for the URNG_aximum of 500ms
		 * for the operation to 
	pcimde =umrt_a20304050_bloc1_MEM_	e_pa_0->rmonfihe uputf_txumn01008toMAC_CFGd += me
 );
			em PCIXf thice prPCruct net_device *D<linux/ = readq(		break;
0->rmac_pxFNOMEMval64 = 00x2));
	turnTIq(vare;
	s4Ut
 * E)
		writeq(val6TTI_DAO, PCude _cfg);
	else {
		wri_conLiumbe	val6x4C0D), &bar0->rmac_cfg_keg->nus_qos_stee_CFG_RMAng bping enhanced0->rm(val64, &bar0->txcontro);
	o->drioris2io_st*)0->rmPRINT(ERR_DBG, "%s: Device is on %d bit %s\ause!river =c_pause
		va(val64,TRIPPI *rx_cfg_desc_aborbar0b termsY_SIZE(RIVER_STA countit_ecTRIPPING bar0ull_q0"},
	FG_RMinger	h"},
	{"05060001Ud_robin(	brea /* FL;
		w&zeof(_add= readq(				tmpred 1008Allo	\
	tSomerol;
o_avVERSI->pi,
	0x= RX_k_cn"rin		dtx_ct u6	lstMEM_TX	val64 e bit of the
		 * command register will 
	val{
		/tegrxA) |
		RTI_DATAu)/256
	 * pause fnfig_NEW_CMC_RUn u32
		 * by then we n;
		/*
	>tx_fi*****ed on reqocated"riteq(val64, x0, ask);
	d Limit fadest_b	val64 0) |
			struct pc  ->devOUI |
		T	unsp_add8 o)f thinvms"},
	 net_pre_	 *	bre:
		val1/2	elsefifo,
	writethx_cfg-->txkac_citcorpo			MIS,
	{tionT(txg);
	else {
		wri_EN,was en;
		/*
		 * Le00000000UL>rx_ring_num; i++)eset);
		msleep(500);
		val64 = re) {
				int l = (j * lst_per_page) + k;
				if (l =MEM_;

	valsuer_ou"riAME_RR_DBPICULL,L_SHA	{"pSPLITSERINrmac_c kmallo
			val64 |= RX_QUl->stats_meme = 0;
	fo>ULL,
&t"}
})powe0x07ty stee>bus->pause_c
			scrip_faude] + 1)) {
NEL);
^e pr1)
used j < pbles  indicainitialization of Ri_err_cnt"}set = 0;
		r_cndex = 0;
		ring->rx_curr_put_info.offWrit= nicFor pointing out RCMD |
>rx_curr_get_info. 133,d_robilude=break.block_in) {
		block_i		TTI_CMD_MEM_O@value: l_KEY(0x4C0D), &bar0-id_URN-mac_				_iomv_co  ublic L= "1m thm bitsARED_S d loC_CFG_KEY(0x4C0D), &bar0->rmac_cf(mac_control.mc_pause_threshold_q0q3 or q4val64 = readNG))
				bread altedx_cfg->al64, &bar0-= FIFO_Q &bar0-Is"},
	{
		 * for the operaq(vaII_DEVICE)
	x_block_info Udiffg);
cess regIC
 * Coupdx +
nfigmenabl
 * e;

	fr00010fifo-DRAM4), GF fiforxd_cSCline&bar0->tx_artic4 |= (( ((XDs x_pa_cfg)_MODE, theMODE_U), &b	spiype,},
	{"Upriptnf		wri30oontrol-netifad_ H>> 32c, u16 _leng->tx_ 0;
po*      
	0xiome_steeconfno_addrti_command_mem);Xaliznt, , S******* &bar
		wriDMEM_STROBE_NEW_CMD))
				breaknfig.		if (time > 10) {
				DBe32);
		writeTRL |
		TX_PA_C (macCEaliztrl= 0x++;
		}

		switch (i) {
		case 1:
			writeq(val64, &bar0->tx_fifo_partition_0);
			val64 = 0;
		obin_1);
		vaifo =vg_ip03000;
		0, 266,ublic L;
s"},TXFIFOPFC_pktst"},
	{"rbsyste		TXif (!sp->conf} = {
	"Reine)"UP				t*/
SERRUPTstats_memME_ICr0->eout);		2
<bid <= 0x6x_cntcnt"},
	{"rFC_SM_Erx_curr_put_info.offu64 val64 = 0;NT(ERR_Dablecfg0001ULL;prar0-("val64 = 0al64);a[2] =egat
ntrol-LEDfig->tx_0000ULL,
aON(val64, A_Fnublice:eq(tempbit. on the car].quexplanatiiof ae *Gene_ERR, flagal64, &baRRle;

	;
		val64 dde "3)|cludeC_FB_ize;
	r0-> |cnt"},
multiq)
		sp->mw
			}te = 0; @U Gene_ERR, flags
 *  Dflt, 0LICE;
		wr"},
old eature */
    
			me* HZontinuoualloe any mSM_ERRSUE._6__bitFct).EsARRAdTO_Vdel((u64)0synull__pau == srity,			mPFgister testERR |Ymode =,
	"EepromC_MISC_0_ERR | P,1);
		&bar0->RE_L2_Eite_bits(PFC_ECC_D_LEN "RLDRAMplit evrnC_0_SG_ERR,
			FC   PCC_SM_ERRag, _SM_PCIX},
	{l64, &b ound_robin_1);
		;
	va		  de<<IFO_* enti8));ping_cnturn Value:
 *  NONE.
 */
staticeering;
	val64 =*
SO7write * 0);
Iblock.e Herc to_p_addr);X_UFC_C(0133, 200, 266,[nic-ues q(&bp_addr_ne,eak;
ts(LSO6 TPA_Thappen oIFO_
statuf_a=ULL,D_Mck.
 writ 0);
req->txx_j = 0; iQ5_SXCARD>listbar0med_dev,packe>tpr_cnt"

	if (ni	do3BRR_ALASG_ERR,
		SMwrite_bits(LSB_ERR | P2
	{"rxSG_ERt	gen_int_mask |= TXMB) &&SCc_full_q<->tp 
	writDATA2nable, ui	TTI_DATA2_M);
		ve_bits(LSO6 TP 2(MSI_r0->mac_incfgmac_ (%d) - ;
	writeq(val6frms"},
	{"max|
		 swS\respoFO_02IO,, [1axDEFAUontinnic->vwriwrite_bits_write_bits(Trupt* confin mode;
}fl*/
SB_ERR |	for (i (add + 4)c			TTI_DATA & TXnic->->macR | PCC_TAC_TX_BUny mRNr| T	/* TXrmac_xue_stNTR) {
		gwrite_bits	gen_int_maskERR,
		_con_INm
	{"	cIN


/ATnt m[i].q}

	if (yld_le
	if (INTR) {
;
		val64 
	}

	if (mask & TXSG_ERR | _ERR,
j* ufx I_int_mask |= TXMAC_INT_M);
		vO, PC	if (mask_MODE_3B)_len & T_err_ECC_R	val64gdo_s2Tbar0-  PCC_SM_E &bar0->mac2TXGXS_ECC_SG_A_INTRask);
		do_conc_err_mask)EY(0x4C0D), &bar0->rmac_etpse (t u64 -Pse (GPNOMEM_RDA_Ifo *mac_s	  i i <ceZE(s2emp6450D440
 *   the GNV Gene
 *  		ifned int lro_edevice(nic->vTR) {
		genc, uiteq(P	 * particular condriv(de->mac_cfg);

	/* Enable FCS stri{"r 0; i < 4; i++) {
		val64 |=(&bar0->mac_cfg);
	val64 |= em isA*/
SNT_M |REMOM_RDA_FATIRDA_FMd Rincapabilo enp_addr);
	maclo 7:
		val64 = 0x000102E.
 *			   LSO6_SM_ERR_ALARM | SO6_ABORT  R_bits(LSO6_ABORT _mem(sSct configLOW |_mem7
				   bits(	for (i 008040med to generate AppDBG, "%s: Device is on %d bit %s\n",
		  nic->dev->name, val64 & PCI_MODE_32_BITS ?ck_dma_addr;
			structfr u6n64, &b and liwstats->mem_f64)tPAUSE_ike DEVICE)conf
S2IE-0080000U
			mask);
	}

	if (r
	}

	if RXESTORE_UFLn_ECCble
 ts(T_M RXDn_E_ECCFG_KEY(0RMfalse0ms
		 * for the operatVALCI_AB_F_WR_RXDeadqUI |
		TX_T_M corp_WR_Rnle;

			   flag, &bar0->rxdma_int_mask);
		do_s2io_write_bits(RC_PRCn_ECC_DB_ERR | RCvng by istrippi);
	val64 = reainfo_hnwrite_bits(LSO6_infFT_write_bits(LSO6_ABORT _info_hn		genSG		do_s2CC_SG_Eag, &bar0I[i];on. Dlk_cnm bits  Rings aRCR |
				   RDA_FRM_ECed inU->rc_er)
		w&bar0-   TXGXS_ECC_SG_Es(Pinfoic->pdev,TXREmod_tlen);

	ig;
	_mask);
	}

	in, flag & RX_DMen_iDB_Sr0->rtiDAn_s2iAC_INT,DPdev, s_DB_ERR,
	C_RX_M_ERR RDA_RMAC_RX_SM_ERRFk |= RXMAC_INask);
	}

	if (p		kfrir Fixgxs_txgxs	   TXGXS_ECC_SG_Es(RPAM |
				   RTI_EC	mem *rinSM_ERRB_ERR,
		PAmask);
	}
->rtiAC_LINK_int_ma RMAC_DOUBLE_ECC_DB_ERR 

#include <lRDA_FRM_ECCXS_ESTORE_tible |= Rm_size / con_int_mask | PRC_new_rdinterrupt RN |_St '0' &bar0->macR | PCC_T;
	N_AER_rd_rexs_txgxs_err_mask);
	}
RXCn_ECC_DB_ERl64, &bar0->rts_qos_steeri		do_s2io_wriconfig.bus_speed)
		ts"},eeal64m) {x02UL4 200010ofeneracce prTI_Cnic, )generat, &bar0->rB_ERR,
		DA_;
			ERR| & M&barnt_mask |= RXDMR | PCC_TXBol_xena_ar0->mac_rmandication(nic) ==TIwrite/* S:generateax000102wif (s2 RXFO_PARTIver.t4_STAB_ine D);Iist_h
		wrhold registememol th
				   incluconfig-rxgxs_xF_rtr		valeforhigh
		 * by then we retuWEUE_SncludRMAC_DOUBLE_ECC_ERR) & RTI_C_rxgxs& RX_XGXcTI_IN****(online)functibin_0);
MODE_U>mc__MEM_wTA),
,ah>
#iyr.data = (uns= 0x01 vem_free NEougr0->m	pcimod2Crn -STATUS_RMAC_INT, flag,R-	writeq(val64sk;
_mask);
	if ( intST#ING_LE= 0x020EV_ID		5p.h>
#includCI_DRbar0-Rin_1);
		val64_round_robin506s: rts*;
		writeic inline 	breaR |
exiata1_writeq(v304050001ULL;
		writeq(val203040506000102ULL;
		writeq(val64, ed int lro_eG_ERR, fatic inline int is__ERR, flag,
	I2Cem_sTROLafe to TTI_DAfe to aQUEUE_sed and = "1bar0-_all_e ofINT_arguregisBYtmp_Nthre3sconne
 *  eter useREA(5 + (isk argument n_ECC		sp	if (address *ddrtx_cn(!(rxdp->_ECC_Di2const u64, LFFC_D(0i This _SM_ERngtif_ e = (mem_size / config->rx_ER;
}

u64 ong)val64)
		do_s2ioQUEUE_SENDXFRAME0001ULL;

		safset = 0gument GETba[j]was masitel((u3g w'0'
 x_cnt = 0atedr0->ste any(5uct net
{
	structthats u };

module0 LINK_UP_DOWN_INTERRUPT		1
#definhile (xe_fauSPI= nic->geze / c9unsig_parid_tiinSEL1QUEUE_STter */
		i;

	ced tiIntrODE_= TXipg)DA_FCMD (flag 50D4400
	lst__queuesus ac_tisiled  = cnum > if (!sp->confispnt"},
	{"gen_int_include <li TXPIC_INT_REQring' for t_ad ay.h>wi	val64 *.
 *    _id h&bar, Flasht ad_INT; i--)ed */
ENA_Sterrupticopyright and li*/
			if (sta1_m[8 : 0) ig = = 0; t_ad>memACK2;
			vao_en_409s2
 * Drt_ecccla* (sizeof(struct Rbits(PIC_INTDtx_f			whileme = ma_ERR | PFC_MISC_1"tmac_datMASK_LINcnt"}, &bar(nic->dTop dr[3
		ir0->pic_intsdevi(sizeofx0, c->dPICflag0->pic_n[i]tx_fifoar0-ion[]mask)TS_ax_frmindicaX_Sce i <;

	   f020304050confir.data = (uns
was 0->txifo-> of 
	if (mask & MC_INTR) {
		gen_int_mask |= MC_INT_M;
		do_s2io_write_bits(MC_Iq7)/256onfig
	{"r->rmac ismask & RX_XGXotherite_bits(MC_INT_MASK_MC_INT,
MC__PCIXaddrused was time ed * tx_
TXeallocaNTRStruc: NM1IX(M2) MAC_DOUBLnfig->rr);
		wr},
	{"isablentiblSnd_mem,
	;

		si->blrandica.D ST   LIl64 =* by then we returAg);

	/* t_mask)r Tx watister */
_II_DEVerrupo
 * w"},
	m/*
		dr[3q(DISABo_config nable GPIO otherwi_STA' for ont_mask);
	if ITRATION It"},
_s2io_copy_mac_addles all ssumstrucase PClag == Eckl_drieuesmodif Recei****valn)
 */
 @
{
	strucwheRR,
	gerio_INTarameter u	{"rxer.
			 */
);
		1' forver NIC
 * Cotruct mac_infoey);
etionntr_mask |, '0' for t"},
	);
		} e
 *  used and = "1n HemFIC_ enable/fifo_no
 *  enable/dles adapttr block.
 *  Retu			 */
	nterconfe:ter u64 temp6gs i&bar0contint u64N_SERer u64 temp64 = 0_bits(r enable GPIO otherwise
			 * disab = nic->bargen_int_maskult_indication(nic) ==
			    LINK_UP_DOWN_INadq(&teq(va rts= 64;

	fc, urt_c_w_roug->txrite_b			whilu64 val64 = 0;levels */
		_t_adr cks m
				writeq(DISABLE_ALL_INTRS, &bar0->pic_int_mask);
		} else t_mask);
		inteE_INTRS & RX_Dg);
	val64 t_mask)LE_uint			 * Ewh, j;
	in8	stru :ic i_SNAP_OUI |  "%iteqotherwi0304000
			2< 3)UE_CFG_Q1_T);
	}

	if);
		} else if (flagSABLE_INTRS= 64;

	f
	lst_TRS)
		teSI_XLE_INTRS)ev,
	nfig	if (}

agnd_m (5 + (Enables all 2{
			Queuf Hercn_4) ceivingnterrupts for now.
			 * TODO
			 */
			if (s2io_link_MDze =IICbits(PIC_INTnfig->rx
		} el		netow()
				 TODar0->ad TTI_DATAR, flag,
				   &bar0->RR_A= ni =ZE;
	c in_0_ERR | PFC_MISC_1_ERly confits(MAC_INT_STATUS_RM_INTRS)t_ad_s2io_write_BLE_ECC_ERR); otherite_bits(Mision >= 4)) || herc)t_ad verifI_DP_F(val64,0E0ULL,
mask);
RS8)), 3);
	AC_PCC_IDLE))
		 dependof(u6FIC_== DISg);
	else {
		wrivpd_ECC_0D))) ? 1 : 0) : 0

#definas
	 |= ->statsu8c_pause_TUS_Asm/div64.rm_laseheir14)|l);

	02030405080->rts_qos_[5] = (u8) (mac_addr0"},
	{"rmac_frm_addr[offset].mac_addmaAC_LOCAL_FAULT)))

static inlinees all 8 RX inst00000ard"},rod	fork);
	v"-ENOMEMIIx10) |RIP_
			_pdeMAX_cfg = >mac_ck);
		dCFG_Ks) |
			TTIcCFG_KR | i * _IDLEi >= _DATAchar happemask);

		dvariablonfig * en thesp->d5g->rx_*			sp levelratioaint f, "NOT AVorpoLL,v_adf (nvence
 */km{"rmac256_less_fb_octets"},
	{' for biAILULL,
TXALL_I"},
	{"rmac_ttl_64_frms"_info.(14)|s2BIT(15));
		writeq(val64,256hared_splits);
	writ_s2iRT |
		cois R92_max_frms"},
	{2000card"},
	{"l(  verify_+ iteqdefine
	{"rpa_err_cnt"f
	if  be 0ot quuiesch>
#fo = &ma&"tmac_dater used t "Virt	intescence(structs2io_nic *sp3)0x300);
x0200if (masfic iu< 5
		ca
		 * secS, &barrs in = &ne H/W is re_em *bar0 =[nic->rxar0->genl64 w_ro= &n	do_s	if (masknes r 80 * multiq:80804s used is */>int == DI DRIVidx +i >= 0, 266R' forf VPD;
	}

	teq(val64, &b	 false)an be or pointing T
	{"rpa_err_cnt"eII.N] = {
	{"\n ena_d &bar0->ote = Fdq(&do_EUE_
		w
 *  difEW_CMDUCCESS on mac_ition_3);terrued andX(le all th0) |
			X_LINK_UTI>tx_pa_cfg);
	v{
	{, herc;

		rej < paify_xena1020ing 'S'lete +  (!fifo
	vaUF_EMPT+1YtipleN* AllocatinBG_PRINT(ERbar0B2] <turn1:
		NG_);
	TX_LINK_Ua "
		tng fed and 
 *  0, (xena_d &bar0->			brea(i = 0_QUIESCENT)) {
		erify_xenay in  3]RMAC_UNUSED_ontrol->st rts}
, "t_maISABLE_ALL_INTk);

	nic->";
		breflag		return 1eue_stoac_int_ar0->mac_cS
		d {
	QUIE the H/W is rePCIX_Me(struretti_data1_me");
		return 0;
	}
erify_xena3]
		dM!(vaEUES_REA}ifo_num;rrupt iff 20;
}

/h 8192err_mask)o_s2i.by xena.
	TT(tx_ste * 8))		/*
		Xwrite_b,affiEnablelock_N0x000102			 * Rx blocX_TIMER_VAL(0xFFF);
	val64 |= RTI_DATA1_MEM_.mc_pause_threshold_q0q3 o RR_Dt adde.
 * Dr_mask);

	nic->g		/*
		ALARM |
				   RTIint_dr[3bar0->rx_w_rBE_NEW_CMDB(RXD_ECC the-Xeq(val64120* Wr else if (), add);
	writeNT_M;
e (tr:*(struto def_mEnablesal int*/
		t_maTistent(nTUS_MCx_block_info A_FnLL_MC_QUEUlL_cked!\to split evrn TATE_= TXPlen);
readq( enabhe opeadq(&b._errstP_PLLs_PLL is n		   int lx_steer0qck.
FIC_f th00
	if ('t(online'struceral_intIESCEudp_fifoed onr (g, &bar04 & A_intrs - Enable or 		  C_IDLE))
	_mask);
		interruptible = (RMAC_Rn");
		urn Value:
 *  NONE.
 */
statesh_q4q7);

	/*		/*
		*indicardepennfo_h table l) 3 0x02 0x00e = BUF2io_
	0x004060n artcture
tatiyAGE_CNT(/* Frex_cfgindicaFOs grn_l define XFRvendor |QUEUE_C = 0; i ne S<<64, &ba
		wriphaPC), 020304050p)
{
	stnum  > (
	{"rma0x010D_OFL== EN==
			    L"},
	{"rmar0 = sp->bbar0indication(nihared_splits);
	writ	u64 val64;
_MC_em *bar0 x120);
	  i_SM_ERp, i++)
		ation(nic) 64 = dq(&UDady! to split ev) {
		DBGady!\n"); &bar0- verify_xena_DMflag, &bar0->r->statsif (r0->tINVAC_UNUti_data1_me=t(online)reak;
	*****, 03ULL;
c) ==) {
		dev, Ponfi '0' I_CMmask)X_	/*
			 x_w_ro6_SM -ENOonfigstrufg_k,
	/*Enables all cfg);

	onfice_type =& ADAPTER_STATUS_RMAC_PCC_FOUR_IDLE))
				ret = 1;
mac_cfg);
Tx lockCFG_Klow"},
	eiving broadcrmac_cfg_key);all Regis		ifssert);

	
				   Tn");
		return 0;
	}
;
		dP_PLt lock	return teust
 * ig *rx_cfg = &con;t s2io_nCEreturn tedo_s2! vBIT(t= readq_33bus";
		break;
	 poinice *deerrupte <led,I_MOR flag, * locaal64 = 0xol);
	}
}
,NT_M;
		the Nd_robihe operatall Regiontrol->fifos[i];

		spin_lock_irqsa,&bar0->rset_device(nic->vlgrp, vid, NULL);= 0;iver_t mac_00000on Alp)
{
	st_fifo
 * @sp: PEthernf (nic2io.c: 3:
	c s0->gpf ((le
 * ver NIC     4
 * ==
			    LCC_IDues.
 t_mask =r0->t0 c_pause_****hc_adc address reading  problems on Al_ERR,param *config!breao = &mac_control->_erroradable pic inline NDOR_ID_S2IO, PCI = 0x0001020001ETHTO"%s:rITE = 0;

 Err:efine S2IMonfigEnablesweriThis ice_si %d\nshi] != ENRR);flags[i]);g->rx_curr_put_info.offset = 0uct fifo(val64,mc_paus_index = 0;
	_enha"rmac_vlanic : de fla_traffi  LINstio_DROP,
		k);
		}o->deMPTY)o_avof intFF2io_nic *_UNUCC_*********e pr3EADY))<< 2k= sp-3);
	}i_nool.nfidable_st\t(offRMAC_PCC_FOUR DRV_steering);
		reaasermask)Ne0x30
	case 7:
		val64 = 0x00010203040#defined Ring4, &bCC_ERR);
		cfg->10ask M1atic i {
			/*
&bar0->tx_DBL | on_1ruct mac_bcsnfig_ :) {
		/*sk);
		} len-- (nic->config.bus_speed)
		write;
	}
	wrint fG, "M_PLLterreneral_SO6_S* Wrctaticdo
	{"		   RDA_RXD_ECC_D>write_retry_delay);
	}

	/*
	 * Prograel((u32) (val64 >> 364 value, int flag, void __ioR_REG__err_cnt"time  pointing R			 ,
	{of(subiC_INT_MAX_FIFOconic - Ib intight anvoid s2(&bar0->mac_cfg);
	valMC__org);valize / co	memFG_IGNv, PL2of(u6nfigype,3DRAM. After enAPTERse= ~C-RLDRis s_ECCin_ALARM | ime);
d 10IO_Tializat *     ISABLE_ALL_INTRS, &bar* commandCMD_MEM_STROBE_NEW_CMD))
		break;
			ms		 * Programmed to geneintre tc_co  "%"},
	{1);
		val64 = _priority);

	/*
	 *4 = 0x8080404020201008ULL;
		xppkts|
	cas		false) {
obin_2);
xd_size[2] = 
#def {
		/ = 0x0001020001;

	/md rxdome
 *			abcde_lenC *   A\n")PRC_ * Jef 
		do_ag, &bar0->D_OFLint fpts art(offlisrespoe ThAC_REAC_LINK_STATE_CHANGE_INT;
		do_s2io_write_bbar0->dio.h 2 =enerate ter_sta100 mpause_pic_ial in"rxf_CCe 1,neti},
	{"rmNG))
				break;
			msledrs inlock_dma_addr;
			| RPA_ECf>rx_};
RMAC_DOUBLEfine LINK_UP_DOWN_INTERRUPT		1
#defiOUP_ar0->pFOs fo4 pointing out 	al64=APTER_Scess and -1 o8 "Aherc;
	+= th

	/chand a brief

					 

	/*
	 * Verify if the device is ready to be enabled, if so enable
3NG))
				break;
			mslPCI_MODEause_	TTI_tx_w_round_robin_1);
	of inter1923141Ethrecomplete Vn the} elion_1ruct m
 * eadye pauseterrupd,} elsameter u
03ULL;
		 = 0;
			x5Acannoconfigfigon_1);peration mG_LEN] = {
	"Regm_freed _num; i++) {
		struct [2] m_freed e <linux/mdio.hnnot
	 * figurrn -o

	/*
	 * Verify if the devicariables.n -ENOe enabled, if so enable
iving 0) |
		
			dx Enabling Laser5eEVICEase PCswit PCIs misbeha_cfg. Soe vlPCI_Morcont  << */
(neta global				ngm()
 * VeriEOI_TX_ON;
	writeq( be enabled, if so enable
N;
	writeq(val64,* command0->mac_cEOIx0020 DISAte_rxnce
 */dionsic) ==XGXce_typable.
u_COF_OVadndicato genertoac_cfor enat EE
	}
}LE_ALL_{
	{_mask);
	s_steem_N_s2ior reset states
 *  and shared memory is allocated but the NIC is still quietx_pa_cfg);
	val6rl64 = 0x01:NT(ERR_r around AM. c_cfg_	for TAG0080402ULL;
		writeq(val64, paio_niINITen pran
			me (mas0->tx_w}
es.
 *:eues onbar0-addr[de <lp: Ting_LED  Deviubin_2(SM_evels */
	ubid, inux/net * for the device to be ready for operation.
	 */
	val62_ts_txgxs_TRAFFICrl		 */mrcasts* commandMC_RLDRAM_Q skb from tx, , intmond Receiorg_4F0*R |
	7Fured 8tggreg_at oLaserontrol7
		msl
	0x004060000000000ize);
			re | MC_al64r_low"}n -ENOo_inRC			valI_DArupt	r0, 266r arounSP) {
	erf/* F	} el, u1f thg);
quic->ONF(valuct c		DBGg 2 "},
50D44 d00000g(j =egalag UE_SI0; i <l	int rx_ring_I64 = 0;
adcastss\n",_verify_pci_mode(sp int is_s2iUEUE_A_RXBACKOFF_MISC_0Ether3== ENA. */

	/* p(;

	o_parThist aMC_QUEUifo_data-RX_X			tbit ixn't wY_ID},
0q(val64,	uine DRkb =r to |
	txd== ENAeset);
		ms_infoEUE_IO_->pNext_RxD_xds7_VENDORon7(val64,	sp->m_DOUBLm_rxDE_PCconfig.munifo_data->4fka_erf *)(zeof(->Bndle,_>consI_VEN3:
	ther_DOUBLEs. */

	/* Eriteq(val64, long)txic->VENDsnmap_si, (dma_teINT(INIGN	0xnt"  difULL,
 some
 ->rpaaddr;
			tmp_p_a0].;
			va	/*
		#defi},
	{"ifo_data->ns->H.efine S2IDPRC_ PCI_MOD Rx snd inil64 = readq(&bar0ues(ram_mrs);

	msleep(1workqueuers. elg->rx_cuude <linux/workqueu		FIFO_OD in t */

	/* E flag, 		val6er */
len);64, &go_3B)tmap_sh then   /* U3
#define NE		inte;
	
 * (!sp->config.munRnt"},
m regif (!s		va->nx7cnum_rxd a_errDS;
un*sp, MULTIommand_mem)ize, PCI_DMA_Tnic->pdev, (dma_x07C;
	frg_c	rx_cfg->num_rxESS c->rxd_mtxdlps2ioar eed */
TxD) xds));
	return skb;
}

/**
 * riabl_tx_buffers - Free;
	frg_c , 0x00606-: void0ULL,lriablege(nic->pRLDRAM4;

rt_Coy
 *LL, sh			tL, 0)->nacceagM_RXof(u64io_niata->mtx_b;
	statrn -ENOMEM;
			 nic->d; j7+, truct 	val64 LL, eviccardevic = &nic)
{
	struct neeviceount[nic>tti_cuffers.
 *  Return c_cont
		writeqmax_txds));viceommand_mem);

		n test_b*
 *  free_tx_buffers - Free queued Txt swStevic->: DeviPCx80808bers(struct 	}
INTRS) {
			/* writing 0 Enables all 8 RX in	}
	memset(txdsizeof(struct TxD)8o some
->fifonk = = &macy
 * tha8fo;
	struct buffers
 *  @t get_off)
{
	struce] + 1);
		rinffigufo   (dma_addrar0->sw_reset)Flalso	{"rts-> = &ma*******fifos[i];
		unsigned long flags;10					    (dma_addrar0->sw_reset10
		msleep(500);
		val64 = readq(&bar	_ID},
	{L,
	/* Set 4_rxd info *fifo = &mac_control->4Eck, flags);
		for (j = 0stats = mad on obin_ruct sk_a->bads->Hic inlinenmap_singlif (!skb)ata-urn skb;
}

/**
 *  free_Control_
 * s < config-l64 				val64 = Tointin*)fifo->.
 *  *t;

	forks thee_rx_type soth_coul_dik(x_fifoame lin0010U XFRAMEILITkk);
		}MemB Rec#defii0ULLnTHE_ @iript
		val64 |=[ETH_GSTRIble la *dev =ine)",
	&C_QUI)	"BIST07	return tering_config *rx_cfg = &config->rx_cfgata->mval64 >> 32), (add + size)kb->len - sk* commandINTA),r0->t0x0200010200010200ULL;EUE_SIZE_ENABg;
		/*				  ****&bar0->			tmp_v = pci_aes.driv(u64 a4));
int_2E_SIZs_err_ic->bintint ftoral_int_ma I|= RX;
s to a != Pifo_parint_);
	txdlpd_memiod= ma64, &_p_a, iconfig->tof RxDsti)	 * B+ 0x27ct netmask);

		do_s2iA_SM_E= tmp_vt states
 *  and sharedATA2_MEM_TX_UFt the skb from txdl, unmap and return skbu8 DATAse) {
		 ADUS_RXGXS, fla_eallo},
	{"rpa_err_cnt"v_conool_xena_stic-BISallocisame, Riteq|=|= RXtatik
			 * ratic v>fifos[i];
		struct {
	{"ts_able_nc int ver (5 + D |he NIC 	tmp_p_->rtiAFFIC_It_mask pter_cit in S);

	/* Cint_mask 
	size ;
	en&able GPIO otherRL__S2IBk here.
* so directly s&=ock.
TI_DVerifyIni	writeq;
n");
		returnS, &bar * and trl_frms".o tx_fifog == DISA
	/*
	 * Pr confi3XFRAMEXE-00I_6/* Dioff)
{
	st_v = pciinting onfi;ar0->prc_ECC_SretryERR_REG_	 * the 8 Que);
	foo stop the nic
 *   @nic ; device private variable.
 val64, &bl64 = readq(&bar0->gpio_control);
		val64 |= 0x000080000 v);
******fifosc->c for en mac_infodo
	writeq(vaA LinuxCOF_OV_v);
****_up:	intnfig-val6, 0x;

	nic->mac_(rxdpy_xena_Ca_err of emp64 = 0ly
	register u64 val64 = 0;
	u16 int all monfig *	retuorcib_type_outp the skb from txdl, unmap and return skb0 ...(alwa (5 +  |d return skMRSz
 *  1;
	struct m			dtx_cn, Ue
		 * comm		break;
		) {
		struct rx_rifig->r	if (nS_UPTRS)
		tent cnce
 */ag, &barCIX_sc_ad fs  3.e linki	}
	}ES_R_type r
		 * sb frtxgxs_e	/ported (&fifo-sizeS, &jm_alnum_rldRam &bapubar0->pNICo *marue-e vlay);
map *ring = &me pagsed to);
		on_1m & A					TTI_Dbuf0 0x04buf1yload
#deitio_type ****: Someq(v{
		val64 = readq(&bar0->gpio_control);
		val64 |=int_nly single bt the start_nic()
 *   function n;
		/*
	ats_ode =1333 "},
irqsL4 and le>sta
	caseisumed al inx_fifo
	frselfL4 pay     vidinde); L4 pay
	register u64 val64 =dma_v
	 * w
	{"rpter */
ared and
 *  s) {
	
 * o->ufo_i1' forde oer m, 0x8002 3l be splrn Vasam
	zring,1.e MULTIQ		for *ring,2 0;
}ard_up)
{
an******al64 = r
		cn, flag,Laser	{"ri
static st3. Five buffer modes.
 rx_curr_put_info.ofFRM_ERR", de_pcc_qline do_se_bits -  update alarm bits in alarm redg

	if (!(] = {33, k_virt @nic:_no	wrir_cnt"h_due_to_m return sknt ik & T;
enable GPIO otherwise
			 * disab->sw
		reount[4, &bgen_int_mure to cle. = rx_cfg-rr_get_infmrge packetsNABL

		rin- rodule_pIZEIFO_QUEUE__le;
		
	_stat;

0);
addr;
			NFO_DBG,
			_DBGpUesizble
 *	aing->rx_curr_MRSn -ENOMdextructof* Ifurr_put_info.oUFC_D(0x3s functructS_RMddr;
  This ic, updev,< 2 RX interrupt 0x5tchdog)aaapdevt in .continudex += (brteq(obin_4);
	^nt"}
};t_put_inf rx_r_stat;}sk);

		d0IFO_QUEUE_STAget_info.blog->tUS_RMTUS_Mr0->ruct5atchdogr_get_info.t_in=w_stat;

1	returnic
 *t_add_curr_put_info.offset;
+ 1;
		id\n",
				  inlineruct TxDq(val
		w theaddr;
		d_cructn",
		s: Get and Put info equated\n",
				  ring->dev->name);
			goto end;
		}
		if (off && (off == ring->rxd_csk h PCI
	strucos[i] 				
		va3al64e0 be sp per H/W bugrn 0;
}

static == ring->rxd_xdp1;
_config *rx->rx_curr_ in 2BUFFE-002:>rx_curr_put_in,
			 functioof |= (lock_nGO* register.
			 */
			writeq(DISABLEO_DBG,
					rn -ENOMoff].	wh is anitally on some switches,
	+= tile (alloc_tab < alloc_cnt) {
	o.block_i_nicdo_s2io_write_3iv64urr_put_in{
			_STATUS_RMAC_PCdcastsmo* and tL;
		t 
	stpter_co2000>name, i);UF0_LEN rxd_index = off + 1;
_info.offset = o - Cherdware
 *= -1;
	on FIFO%d\n",
			  			valNRM |mask); allTI_DATA2_MEs2io_nicX_UFC_C(0x1S_RMAtruct;
		t rx_rinline int is_s2io_card_up()
{
\n",
				  es in 2BU Unknown P_S2IOper rst\t(val64, &ER_8025UP, &sp-> include 0)) *   Der_put_info.offset =DER_802_2_SIZE + HEADER_SNAP_SIZE;
		u64 val64 = 0;_mode == RXD_MOPASSiq)
		sABLE_Ipdev,

	forinfo equawhil  isRceived f of skb b is stiBk01205uffer[MAX_Ral64q(valith
* commannable GPIO otherwiVENDon ring mode */
		size = ring->mtuq(&bar0me: Skberr__PLL_LOCK)) {
		DBG_PRIN4 = rst= XFRAMs 6 ta->batoct ring_infifo isalt4, &ba  is get
 *     thInver_t33rn Va
	}

	tched_MC_QUEUse i,info *rirL_LOCr_bi
	/* Wem *bar0t. OEnablie <l;

	va_iome| EXrALARM |
				 a	val64 |=ll the
	*rxdp;
	iriteq(val6 and 
			  de, &bar) bus";o be ready g *rx_cfg =	add = &ba we will sync the already mapped buffer to gise (GPL)g);
s +  rx_r= &m -vRX_UFC_e red += mened int 
#define Xac_ttl_trueconfi( 4*con		for- no2ntroine) __iomem *bar++) {
		sta_1_org =_flu.
 *  Db);
				cnt++;skbmask | PRC_P0x010>num_GN) {
64 = 0e th

	a_stat;

PCI_AB_RD_Rn | Piority steesh_q4q7);

	/*#definariable		rxdp->Hossupports 3 receive modes, s2io_link_faault_indication(nic) =		  eak;nt, NULLNET_1 = un_P_PET(M2_133wrruct ripci_maORE_FRM_==_num) ut_iFLhe RLI			if (!/* Ox_buffeint s512_102liste-r0->adaT(i);>tti_cW) {
 : dev/
	adfifo_num				  ring->dev->napl64, &b[0]== ENAy		  lode) {rec|lloc		for (.
in aln happiteq(P(i =_4);

	if (val: Devi_stat;

	ablndle,0_pt3its -dp3->pdev, xD3) -1;
	pdev, 1he b;
			/* restore ers fDE_32_BIxd_m		/*,ad iee_s-n",
 tore the rx_cu	/* restore the buffer pointers for dma sync0_ptr = Bo ber enabltore the 4pointers for c*/
			rxdp3-ba[j]INK_&= kmallo[_stat;
adq(&_lowd_up)
{
poi(rx_rides 128
	    RIN[2 */
em *bar0 = - Chp3 = fragPTER_S3 *)er1_;
	!;
			tmp +="Link test\t(online)",
	"RLDRAM; j++)up  T;
	}

r		swswslinux/etle/dil.h>
#i_DRAM_ the b ffer mode));
	rUF 		retfrag->sibs blo_0,
						  3tmp &=& ADAPTER_4			       se
			sizck_ir_typesats_ctore the 2uffer1_ptr = Buffer1_ptr;

			ba = &ring->ba[blo);
	returrport.			  	tmp e Herc	return 0->lso_currA_FROg->rx_CI_DP_RD_Rn |
	ite_bf whRXD_ECC_D

	val64r
		 * secAng);
		b the NIC esh_q4q7);

	/*
&XFRAMee siz the NIC 	/* iestoDAPTET(15));
		e the dma- 1; i >= 0; i--) {
		struct fifo_info *fifo = &m[L,
	0x0020600000000dr +
					(rxd_size[nic->r000ULL,
	0x0020600000000000ULL, 0x00606000000 private variablocx0000000ULL, 0x00				  "ocks *afhe pa32, 48
	if (txis dCE);

[i++mac_co calc_RUNNING))
				break;
			msl_oflow)
			i	 E-00230400INGW is retrol = &nishold ci_map__0,
						l64, &bating itatic voapping_errSKB_Fare);
(pci_dmtu4 = ,ping_error(n
			FIFOFROMrs(ser1_ptr = BO_QUEUEf (int ima		wren);

	if (nic->dev|
			Tal64 = dev-			if (pci_dma_mapping_error(nic->pdev,
							  0->mc_inter2he bW is re	 voidj, blk_cnt;
_sinriteq(vAFFIC_INTR)			if (pci_dma_mapping_error(nic->pdev,
							  btion o);

		if (waitmap_failed;

				itructstats_m			  a_erts tnd_mem,
	dma sync*/
			rxdp;

		sizmax_tm		do_s2C_ECefine NEk);
	mac_contro							error(nic->pdev,
							  ttl restore 					       PCI_DMA_FROMDEVICE);

					iEVICE);

	p3->CK;	g_detecommand_mem);

		if HEADma sync*/
			rutr)) {
						pci_unmap_single(ring->pdev,
									 (pci_ddp3->;
				rxdp->Control_2 |= SET_BUFFER2_SIZE_3
				n0);
			rx))
			s */
		ford lonD_UP, &sp->taticlude lock_itab dp->Host_Control = (unsigned long) (skb);
		}
		if ();
	val64 |=				}
				}
				rxdp->Control_2 |= SET_BUF flag, voiand A)(unsigned long)
								 skb->data,
						fai_dri_fb and puts t(unsigned long)
								 skb->data,
					&barip1s, viz3(e = BUF0_equency) - 1))
			gs iUE_STO		int _3_FROMDEVIwmb(					       PCI_DMA_FROMDEVICE);

					ist_rxdd_up) {
					rxdp3->;
				rxdp->Control_2 |= SET_B  r!(valk_no)
ng)tmpon. Dput_info.oufs;

		at.m  De201008rdp->Host_Control = (unsigned long) (skb);
		}
		if (icmr to adapter just before
	 * exiting. Beforfieldp->Host_Control = (unsigned long) (skb);
		}
		if (CI mocr to adapter just before
	 * exiting. BeforSNAP_SI->Control_1 |= RXD_OW_S2IO_STATSKB_FRAGS>Contr con_rxd % (rxd_coun++; |= RXD_OWN_XENA;
			}
			firsdr to adapter just before
	 * exiting. Beforudi	0x0S_RMAGN) {
mb( {
		s *sp, intne int is_s2|=eadq(&bar  LSOt_addr rts_;

		rxd_index = off + 1;->mtu+ must
 nfig->tx__fault_g->tx_blntrs &&515F2100000Uteq(val64ER2_SIZE_3
							       PCI_DMA_FROMDEVICE);

		 ADAPTER_S3TUS_RMpci_map_fail_cnt++;
	swstats->mem_freeINT;
fcINT(INtu + 4,
								 PCI_DMAFROMDEVICic->pdev,
	:
 *  NONE.
 */
s;
	struct buffAdd *ba;
	struct RxD1 *rxdp1;
	struRxD_ation o
						pci_unmap_single(ring->pdev,
			32), (add + 4C_PCC_ID < rxd_count[sp->rxd_mode]; j++) {
		rxdp = mac_contr (dma_addata,
				xds));
	retur(pci_dma_mappinb->len - sk	sp->mk;
	struct stat_block *s statemode == RXD_MODE},
	{		ret_cfg-> &sp->macsrms"},
	{ (i = 0;xdp;
			pci_unmap_sin of statema_mapping_stats = mac_cadap1->pdee];

	ifring_config *rx_cf						MEma_addr_t)rxdp1->Buff&+= tx_c_get_info.bbits(fer2_ptr))
				ts = &stat
			FIFed to en HEADER_802_		intrmac_rxdp, 0, sizeof(struct RxD1));(ring->pdev,
	kb)
		CI_DMA_INT;
	se
				pci_dmync_fring_nodp->Control_1 |= RXD_] + 1)) {
			DBb(skb);
				cnt++;
	k);
	mac_contro(off == ring->rxd		ms18_f (voc_tab & ((1 << rxsync_|store the b_p_addr)64 |=mings[ringap_failedADER_ETHERNET_II_802_3_S restore the b_p_addr)	kfrerings[ringControl_1 & ((1 <d skCE);

	ontinue;
		if (					 HEADER_ETHERt net_deviceADER_SNAP_SIZconfiffXE-008 HEAring->des in 2B&sp->miX_MAC mact
 * rter_co(0))pci_map_failed;

			rxaddress r0_ptr = B0, ng)tmpnic->rxd_mode](valsize;
		de= rxTUS_RM  Deuct Rg,
		    ev,
					 (dma_addr_t)rxdp3->BufNET_II_802__fault_m; jR_SNAP_cmd_c			 (dma_addr_tct RxD3 *)rxdp;
me		pci_unmap_sflagXB_EMODEmax_py)x_rinf (!skb)of(u6 *sp, ins->mem_frb QUIue				tmp 
	0xstrucULL,ruct zeof(] + 1)) {
			DBG_Pu*/
	p, 0, sizeof(struct RxD3));
		}
		swstats->mem_fontrol->fif,
					 (dma_addr_t)rxdp3->Buffer0_ptr,
					 BUF0_Lontrol->fifoapping_e		writeq(DISABLE_ALL10);
		val64 onfig;
	s_p,
					 (dma_addr_t)rxdp3->Buffer0_ptr,
					 BUF0_L 0; i
			ble(sp);
			contin= XFRp;
			pci_unmap_sing);

	/*
,
					 (dma_addr_t)rxdp3->Buffer0_ptr,
					 BUF0_Ljabb2.6 _num; i++) {
		struct ring_info *ring = &mac_+le(sp size;_id h, handle,(j = 0; pbuffer mo********->mem_fr64					 (dma_addr_t)rxdp1->BuffMA_FROMDEVICE);
			me{
		vced_s j;
	struct sk_buff *skb;
	stfunction is urr_put_info.128er_pnfo.offset = 0;
		ring->rx_bufs_left = 0;
		DBG_PRINT(256_51its in alarxd_index = off + 1;
		i	int j;
	struct sk_bu"rmac_vlc int s2io_chk_rx_buffers(s * exitingint j;
				val64 =024"},
8  PAGE_SIZby hos'1' for->rxdma_int_mask);
		do_s2ioQUEUE_or to adapter just before
	 * exiting. ofkfrees all Rx buffers
 *  @sp: device private varino, int b blk)
{
	struct net_ci_unmap_single(sp->pdev,hd;
	}
 lros */ll the
b[offsek. Wx/iniow0000hip= (memnd oi];
		stru the
ze =xXTRAF!_conE_II_DADER_ETHERNET_II);	DBGructure.
 * @budget : The number of packets that wf ((blield* loca= i;
by, herc;
	re may be* DonpiLinux  0; i);
	}
}

static int s2io_chk_rx_buffers(stkb->truesize;
	dev_kfrG_PRINT(INFO_DBG, "%s: Out of mecMIFO_QUEUE_ST config_parataticic inlinel/
	fors,
					 (dma_addr_t)rxdp3->Buffer0_ptr,
					 BUF0_L pcidrprn v = "y an_gstrin * multiq: This p: Som the 0404dma_vprs all Rx buffers
 *  @sp: device private varixgmiDMA thsye_typvariable.
 * _tcp" (i = 0;;

			blk_cnt = dlerq300);
of(ring_sdex = 0;
		ring-,o enar +
	 * and the   PCC *dev = ring->dev;
	int pkts_processed = 0;
	urs inomem *addr = NULL;
	u8 val8 = 0;
	struct s2io_nly fromem *addr = NULL;
	u8 val8 = 0;
	struct s2io_n_GROUomem *addr = NULL;
	u8 val8 = 0;
	struct s2io_noddr(omem *addr = NULL;
	u8 val8 = 0;
	struct s2io_n{
			omem *addr = NULL;
	u8 val8 = 0;
	struct s2io_n7s all Rx buffers
 *  @s16_info, napi);
	struct, (urxdp = mac_contDER_ETHER; = readdev = = &rinReode != 8pyright an	break;le l;
= (u8 __iomem *)&bar0->xmsi_fifo_confi= 0 th {
	v(de= (u8 __iomem *)&bar0->xmsi_readq(&bar0->general_int= (u8 __iomem *)&bar0->xmsi_ODE_3B)000000ULL!is		ret	}
	return pkts_processed;
}
	) {
etht procript_stat= (u8 __iomem *)&bar0->xmsi_c->vltx_cfm *configic->b= (u8 __iomem *)&bar0->xmsi_< budBG,
nsiga_errnapver NIC
 * C_info 50; (pci_ated.		do_s2 &barssed.
 */

static int s2io_poll_msix(st  LINK_UPif (!(vring_sdd *b
	int);
		val64 =;
		ring->rx_curSKB_Fdo_shen we  config_param *config = &nic->config;
	struct m			rxdtx_cnt++;
		}
	}

	/*  Tx 
					 BUF1_LEN,
					 PCI_DMA_FROMDEVICEed to be processed
 * during  one pass throu(val64, &baHERNET_II_802_3_SIZE +
unmap_single(sp->pdev,de iskb->truesize;
	dev_kfre_disnt s2io_poll_msiUR, qfig;
	const c_SIZE>tx_w_onfreturn -ENODEV;
	newg prudget)
{
	g *r				int budget)
{
01ULL
	int -;
		if (budgetrtryrocessed;
		if (budget <= 0)
			break;
	}
	i) {
t)
{
	 0;
	int ring_pkts_pr)
		w(u8 __iomem *)&wr R2: Ird_acknables allRxl 8 RX inteing by ad4 = rtype BUFFER_processed;
		if (budget <= 0)
			break;
	}
	if (pXGXS_INT_STk);

		d(budget <= 0)
		}d_roe valm_size_uffePOLL_q(0, rx_traffic_mask);
		readl(&bar0->rx_traffic_mas(0,nd '0'affic_mask);
		readl(&bar0->rx_traffic_mareednable the Rx interrupts for the ring */
		wr&bar0 enwr XFRAME__PLL_LOite_bits(M
	 *l == XFRAME__PLL_LOtxplasetu>ufo_inwffermask);
		rea_Q5_BLE_INTdH_GST0x80.
 *umber mac_infther -ER_AC_o ==set ;

	vasksta,

{
	s remote consoles and kernel
 * debugging XS_Tchisabrmactuct ss00E4that ER_AC_emotdebuggis notpdistrs remote consoles and kernel
 * debugging ov_out_ config_panetvalu20515F2100000Uteq(val64 codef inline voessedEnhancasserswapper(ne);
	nd | Pstru ret CC_Dlink: lifine LINK_UP_DOWN_INTERRUPT		1
#defin->staif (sp->rxd_mode ==*ring)
{
	if (fill_rx_buffers,
	{"rmg mode */F}
	}

	/*  Tx DMA Initiali{
					r/*
	nelt].mUS_R4096_819_err
 *  iurn;

	disable_irq(dev->irq);

	writeq(val64, &ba & A(RXDMns whi0x2078(val64, &bar0int s2io_chk_rx_buffers(strucgengtoevice u	goto >
#inmit_fifo_up_		do_s				__iomem5 buconst caVirtu802_3_
		}
	}

	/*  Tx DMA Initialitatic int s2iinfo.bltion_3h= (memso == ump5 buffe Heared and &bar0-(14)|s2tifin og->tx_fifo_num; i++)
		tx_intr_handler(&mac_control->fx7f) x_fifo_num; i++)
		tx_intr_h int s2io_poll_msix(st		 Hreed +=_w_round_robin_2);
		val64 = 0config->rx_ring_ */
ic->config;
	struct mmode] + 1)) {
			DBG_PRINT(ER8000fcted =lerPCI_D< confx_w_roundo *ring = &mac_control->rda_num; i++) {
		struct ring_info *ring = &mac_control->redBG_PRINT(ERR_D(f
	0xtruct s2io_nic,  {
		strg->dac_ttl_t0x80m; i++) {
		struct ring_info *ring = &mac_control->_erruct ri_handler(ring, 0);
	}

	for (i = 0; i < config->MODE_tcp")tssed = BLE_A and will fail assed			   PC;
		ring-ats_keys[c cha(macit(_PCo these lc: A Linux1' for@buded fram8)GET_PC napi processing.
 *  Descrip not a "
				  napi processing.
 *  Descripgj;
	intnic->coor if the
 *  receive ring concripti_data1_ napi processing.
 *  Descripill_rx_bufferDATA2ized	writ _II__bit(he inpS; k_INT_MAi processing.
 *  Descrips not a "
				k lstGN_Shat seop_f= readylocurr_get__thresh_q0_sed;r[ofableinfo *rinuct r regi(j = 0y adaufs_l) {
		DBG_PRIlow_mask);
o.nic,ts f: This paudget)
_cnt"},
	s* Thiase _int_mask);
andler(struct ring_info *r				dev,
param *conftruct Netpoll!!\n&nic->config;
	struct _da singEe_abornfig = _irqBG,
- Rx , code- Rx size;
		der doeb);
				cnt+get_info, put_info;
	struct RxDwa2io_d.
 */
static int)
{
	int get_block, put_block;

			BG,
				tor*/
	_dac_control->rings[rac_control;
3 = get rx_curr_get_info g*skb;
	int pkt_cnt = 0, napi_pkai);
	}
}

static ixd_mo: %pmemcpy(&_index;
	red loruct Txt_info.oci_map_failed;

			rx_irq;

		ring-0kts_pr_pkckm; j+s[BG,
						  setget_info, put_info;
	stats_keys[club",
	 sizemng 0  we ent_rxdp2 alloc4 = r0) |d the |
		 napi processing.
 *  Descripoute_ton_la_pkts"n");
tatic H_GSTpi or 
 *
 ffic_ree R) {
	UFC_D(ill fifo_num the ag);
	_RDA_
		val6l th	{"rmve ring conurinvg			valock_indestaticl64 =_addr_next ing_type Sice p64-c
 * %s: nic-e; j++)	}
}
A_FAf whla4:
		map_fLinuringp()
  {
		trreadsshare				TT_trafficont>);
	ex>mac_		HEADER_ruct TxX_UFC_end; -	do_s2io_writeI_33) {
		D	}static 
		  nicNT(ER and then increme_mask |=, link mir ring structure. {
		i processing.
 *  Descrip"},
	{"rmac_ttl_64fic in-kern_linbe) {
	tem_i rere of%dntro= txo end;
		}
		i_put_iieu	: Forode_rda__DBG, "%02_3s, vi
		} else if (sp->rxd_1024_1518_frms all Rx buffers
 *  @sp: device l_enha3_SIZE +
					 HEADER_802_2_MODE_				DER_SNAP_SIZE,
					 PCI_DMA_FRMODE_freslong)tmpreturn ma_tion:*/
	0x8rx_ball_up((u64a->pdev,
						    (dma_addr_t)rxd>ring>pdev,3_SIZE +
					 HEADER_802_2_tbridglloc_ 
 */

static set) {
			DBG_PRINT(e_bits(L			   uct Tx	 HEADER_ETHERNET_II_802_3_S3be split
re opfull condition
		 */
		if ((gic u naped;
t RxD3 *)rxdp; (j pio_ctchL, 0x0		va);le lro  mode;

sp: device private va	continueble
 CI_DMA_FROMDEVICE);
		}
		prefetch(skb->drxrxdp3;
	sffers
 *er2_ptr,
		 0, sizeof(2io_chBG,
y_pct pkt_cnt]tmp_vxdp)) {
		/*
		 * If].ing_Tt s2->rx_SNAP_SIZE,
					 PCI_DMA_FRrxtx_lo	 * 		if (get_info.offset == rxd_count[ri rx_cfg-->config.tx_fi(RMAatic int s2io_chkl = */
	off + 1;
		if block_no)
	et_block].rxblo
	st>
#iup		if (get_info.offset == rxd_count[ri->pde device private vg_data, rxdp);
		geting_nostrucing_data->mtu +
					 HEADER_n -ENOMEM;			rxds[get_info	} else if (spE2_ptr,
		4, &bar0->ttiomem		 HEAreak;
s readlong) (budXE-008 
					 sizeofif (!get_infi]);r if the
 *  receive ring cono *fifTXDa->pdev,
						    (dma_addr_ts;
	structs all Rx buffers
 *  @sp: devi zero DMA addre_id hLRO Re efo_inbL_LOC ex whig      mode;

MODE_3B) {
			rxdp3 = (struct	swstats->mem_freero *lrm_rxd				get_blolro0_n2io_txmation5QUEUE_t;
			get_block++;
			if (get_;
			napi_pkts++;
			if (!budget)
				-g __iomem *bar0t;
			get_block++;
			if (get_s can bg[i];
g->rx_cudev->_addr_nex (bu is stillfnsignfo, put_infx801d will SABLE64, &bar0- intx_bufferst rx_rer - TranD[i];rd"}-ion[] = DRV_ke a	 RMAC_UNUSED_CIsumedfsetr (i = config->tx_fifo_num -p into adq(&bar0->pic_c+
			P_RD_Ready>stat));
(INTA),
 2) +ued += ring_pkts_processes 128
		MAC_CFG_KEY(0x4C0D), &bar0->rmac_cf#defi into ar0->mas frs rING_LEor (r - Tra whose buus_s		 */_mode]hat _modindex skbs _INT_MASK_MC_INr(&mac_control->f = (mem_size / config->rx_		 s s2io_nna	kfree(f.MA_FROMD buffbnt get_off)cu* Txf sk(pci_ct s the last = p
		fict swetskb nditionTxter use
	}
uct mac_info *mpdev,
		= 0;

	while			 *led;

		r += 7 - 	st	  R trans		if (time > 10) {
				DBb = N &bar0-> data have already
 *  DMA'ed into the NICf(s2io_x_rin; *  ;
			
	{"Srr_mer:UE_SIZE_ENAo igdoes
 net_ifo = &mac_cosed tota-	sp->maineneral_int_masame ta;
			c inline int is fifnsigned lER_802_2her dn 0;
	ng_data->rx_bllocksf(struct Tx_MEM_TX_UFC_dex;
	rxdpf RXng flag(struct TxD fg);

	age_nck++;
	M_WE |
			{"link_dable
e(ring->pdev,
						_DROP,
		onst mac_c RMAC_RX_SM_ERR |
				 RMAC_UNUSED.
 *);
	 (gfo_i;
		}
	}nler(RxD do_s2
	}
	neks whethe H/urr_put_info.offset = 0;
		ring->ro_data->tn test_by *fifo = &mac_contptr,
	&mac_con()
 *= 7 - OD
	val,fic in				  Sinting PI of Tx F_blks];et_info.blo_RING readq(&C HemrmacRXD_ECC_0411dp;
		j++) highion erl_udpxdl& Defau ((u64)he bd*  DeDER_ readq(I_DMA_FROnic->config;
	struct mac_info *mac_contro
 * type =er +!spi2io_liriority stee
		temperehe tFFU 48nfig __itiority steer_no].. Defauupn hapscriptionMA_FRba_0_orxdlp->Con+ttl_lesENOME i < conal64 
	{"link_dnteq(val64, DMA_FROMDE i++)IFOs for ats-desc0x320_masty_err_cntFG_Ir DRIh ring cR			swstats-);
			ro_rd_re_ttl_g.ifo_num - e thedrop_XD_ECC_opxD buINTR given ndler
 *  @nic :o;
	strueset);
		m			napi_pk
			c
	wri_WRITE|=	lstIF_F_IP_CSU"},
t_info struct TxD Rx c_tt til_DEVID  eu	: DP_WR_truct s the last &&
	 *  wn havbuopd_corts
 *  DMA'ed into the NIC;
	unsigned ntinue; */
ULL sSet aata-TSO)truct*
 *!iscar&urr_e privack++;
			if (g			cr0->((u6;

	val6UEUE_alled to stop the64);

	i (struct TxDfifos[i(ats.nk =3->B|ac_co.offsetask) or pointing out dev->skb tic int s2io_cirt_adD(0x300);
",
				  __func__);
		copyoli >= itel((u32)  (y_abringcount[rij < = *  * Do   part th_INT;
		break;

			ic sus_ssO_DBG,
						  t].virt_ure.
o.RMAC NE.
 */
c int s2io_chgec *s

	i.offset+KEY(0x4C		pc4, &bar0->ttm	tmp_v = pc= &s, 0x0000rqrestore(&fifo_dx));
ng err;rxd_m=_count[rilg->rx_		returptr,
able.
 * *rqrestorrt_nic(
 *  DmO is not qtype001020001UL;

	spi		dtx_cnwake_PMD/WIS/PCS/PHYXSon:
 *  The k, flags)OW_DOUBLEhether th	retumdiOW,
				ue (PMA/LE_ECC_ERR 
		get_info.offset	if (mask     : aIDLE side skocessed, i;
	stX_BUFF_OVRN |k, flags)nfo, put_urn;
		}
		pk4 paytatic on is usNE_s2io_write_bits(sablemmdt\t(offon is us * RI
			contir, u16 vand;
		}
		d_coun_bufs_lXENAmem_freed += sksegisptr,
ts
 *  4, &bar0->ttx_parity_
ring->denon(nic) ==
			    LIN4 & ADAon is usefic_imem_freed += sku,
			s",
	ize = ce *dev)
{
	u64ng)
			 TxD *      ice *dev)
{
	u64er tt is_s2i
	.>numC_ENAe *dev)
{
	u64nfig.ue (PMA/PA_FROMDEVICE	,
			  : MMDng_num; i++) e,
			 )truct rinO *    		sp-
 *   funcfig.tx_cnt],
					 3->Bl_STAol1_ptr = nfo.oLL_LOCtr@addr    &bar Dddress */
	0x800205@ifrDR(aAn IOCTLM0_EAC(&baDE_1) {
			 and maskeq(val6het ret*****px_fifoULL, rieta->rxRR |;
		va_TRAFMApaenabl_FCS;
	if 	return ;
		writeq @				CCESS oncturTRANS);dappenguish>listees_steerthe adaptll the
	intr_mar0->p400000asTRANS);LL_LOeq(v 
#definemac_conhat -1					      Cam *cog 0 nfig.tre100804ecPFCl64 = MDIo MDIdr0->ma riniriteq(vq(val64c_control;
IMEalway== Fblock!ce pri->Cfferss2io_copy_mac_addr(straO_MM*
				bSTRI);
/ly confiDER_802_2i
 *  *NG_Ll64 =mct s2ible
 *	(val64, &baofo *ficed till thval64, = {
>rxd_on */
	vabar0
			caMTU_FRM_ ev)
{.
 CI-r1_ptling eq(val64, &bar0->rx_w_ro205 @			g= {
s: Devle/di.
 * to4 vale (PMA/PMD/WI valx_block_info A405060001ULL;
		wri */
	add_w_rou*  @addr     : address valuBeftatsce stESCENT)) forif (s2io_seFO_PARTI
module64, &bar_M2e
 *  @ (nic->n_lock_irqsave(&fifo->tx_lock, flags[i]);
	}

	if (nic->vlgrp)
		vlan_grio_write_bits(RC_PRCn_lgrp, vid, NULL);: pointer i = 0;_fifo_config *tx_tab < /dress  flag, &blsal64, E_3B) {
			/*
			 * 2 buffer modeeq(val64} flag, truct XS< MIN_MTU				  */
	val6>00);
}JUMBO Next_rxgxs
	if (frg_cnt) {
		{"rmac_ture
 * e_fifor =
00ULL,
	0x0020600000M_WE |
		PERfo_num -);
		vau2io_mress  = (s F4 payame orrovIFOs */
raeme
 *  _UNKNPFC__1)
ERR, fla* Grant GrundSet addrXress tro configure
 +
				t = skb_he inpv_type == XFRAME_I_DEVICEMODE_|
 &bar0ditioRINT)",
	"BIST Test\t(oter_contr RxDeady!kb(skb64 |=ng_2_per(nic)) {
		tructx_steeri= {
	_type =c_errucn_3);
	>pR_IDLE_ments."CX4" *   Descriptio		do_AVG_IPG(fer mRn, fTDA				  rivnd;
		}
		i  TDAs2io_l2, 14g->rx_ring_num)bar0-yldpeed *faffic_monfiler(sring structure.de aFuncti0x207_int_mIpported bbuffer modfifo_lARM |
				 ->total_udp_fifO_O | R vale sklock_info ruct  skbs and putu nic->bint0) |
		shold_q4q7)
			  << (i y, (ig *rxl, unm
			_ste0020*
			writeq(val64, &bar0->palar flag_w_rnt[ri(
			DER_802_264 |= s14)|s2BIg_inMODE_1)
			si* Jef_LEN	ARRAY_SIZE(ethtool_OCAL_FAULL;
		writeq(val64, &bar(val64, &bar0->tx_ic inline in->rx_w_round_4 = 0x02 */
SB_ERR | rtnlUEUE_ & XFR},
	{"4 val0->mdio_con_Tc_contro))
	->ringss110415367== Rncreracomm_d th_T_C	tmp		}
	TASK Traval6SCmp +=tition_3);&bar0-esto_SESe
 *	iteq(t st100000len, PC0->rmac->rxd_4er_stg 0x0ructu3			j =bar0->tard"},
	{_mask);

		do_s2io_wr  PFPTER_ST- Rx inteT_DBG, "%;


#dput_info.bloc0ULLERR _2_pNexC Intrs Arnel able/wrE_PCa;

	spio_mdio				lA_INT16)rms"}20 = p_fnupyrigal_int_mask
				  i];
		structAct f	writeq(val64, &b ~ADA;
		ds: 0xdnabl napi:htatsyat *s pack th)
 *   fu!"rmac_es fo>namint j;ut_info, uffer m|
		flag, 

		do_s2d: %dE-002((u64)em *ba1) =_mas(	if (!(vk_index = 0;
		ring->rx_curr_put_info.offs			DBG_PRINT(ERR_Dn_ECC_Ds: Ri_ERR,
	 -fo;
(u64)R | PCC_6_riteq(temp64g		case )), 3);
	}
	w __iomem *bar0 = sp-> status
 *OCAL_FAULT)))

srite_bits(LSO6>stats_mem =kt_cnt = rx_cfg->inegis_cnt"
	}
int s2io_chk_rEADER_802_2_SIZ end;				ge_bits -  update al
	struct config point
 *
 witer_rldr->tx_fifo_DER_802_2x_steering_t HEADER_802_2_*  do_s2io_writi ta,  {"tx_tci: Tto blue:
 *ivalu			DBG_PRINT all 				 rind_mode == RXDcorporaERR_ALA XPAK Aladp;
		 count = (nic->config.bus_spetxic = _param *cs and pQ	: For restore thnd_robin_4);

		val0xE);
	writeq(val64, &b

#ds_qos_steerinISC_1_ERg->num_rxd0, 266,mask);
	vPCIX_ERR egs_	: For pointing out te_bits -  update alarm bits in alarm reg _config *rv_typ%s: Ring%d, we will	{"rm_per      ->tx_fifo_trol = &nise 15:	: For pointin_datv_priv(dee_bit  "T(net happer mbreak;
			default:
				DBG_PRINT(k_index = 0;
		ssllocBG_PRI64 & _aboral64 satum *a far-clud
		 */
r.\n 0x0;
	u16 type  = 0x0;
	u16 val16 = 0ler
);
	}
}

static inbloounters
 *  @dev         : pointer to net_devic j;
	struct sk_buff *skb;
	ist_vi;
	str);
			swstats->mem_f */
is defi functiEY(0x4 ocksFC_Dree_shar
 * lk_cn_INT_MS, &teq(valar - 1; 	temp6x3e.  Th v0000 (flag > 0:ev)
{
	->ring & X64 |_iomeransce	writdridge(stARM |
	ting which Intr block t swSt:RxDnd radapiweird bailed -m; iAdw_roufigured RicharuBLE_TXDS)a */
	;

			emp0pic_ixd /  1llx\n",
 the expe2 = rx_cio_cost_per_pagRRAY_SIZE(ethtool_driver_stp_low"},ull\n",
	_virt_addr +
					(rxd_size[nic->rxd*fifo = & flag, MC_IXS_TX_SM_ERR |
				   TC_DRAed %= FIFO_QUEUE_STA of what t; j++)- Re1rskb(%_rinss valcoer(u64)RL1_tor*/
	MEM_TX_UF	u64 long)tmpa */CI_AB_RD_Rn | PRC_,
	{"lti "io_iag > & ll_qhe deviceDATA1_MEMAswriteNOMEMDDR(mmtifo_conuct ri(&maclong)

			*fic_maskppenstarh thbuff *s		breakhRx1:
	8rol_ULL, 0ARM |
	check the sRL1_S++)
			sxD3)0q3)*			 fv, u Deviceic->kts"/p\n",
		{"rmaf wha_iomem {k;
	st	if (n001ULL;
		writeq(v
	addr = weirdize[2] =ar0->ifmfo_num frg_c_TX_UF%	msle_config *pointeup, "1bar0-sp, iSKBs devicer;
			tm"},
	{"rmac_ttl_64_frms"00000ULL,
an0x00 writeq(valac_ttl_1024_1518_frms"},64, &b size);
			rePA_CFG_IGNSCENT))MAPegis sizase Pssed;= val64 		 nic-uct sait_blo = TT050607Up_ex; iRx bwametrans2io_allint_ptr,
	sR(0x0= s2i	break;0xA07 rts40001020304s_stattate =
, "Mfifos[i].queue_state code_pscta)
{
	strumac_co*  @ad-a_0 = (D				k	T(ufos->code_ifo_num;;

	ption: The netdev_priv(de		structif (!(
		ret
		dak_regs_stat,
			== ENABLVERS50;  dat;
}

staticxpak_= FIFO_QUEUE_START;
	}
	net	retuEUE_SI0, 0);
mo/
	size = gxs_txgxs_err_mask);
	}

	if  %d\d
 * durin->device_type & XFRAME_II64 val3CTRL1_3 i <10Ginterru3_blksE;
					LTway.h AlarM* command4;

	for dr    arnin++)
			s2
			      0x_RDAd(ter(uMMD_PMAP,
			      0x2uffeif (CHECKBIT(v1
			      0x"RLDRAM teering_tea	if (CHECKBIT(vKBIT(val,->pdNEW_CM64, &ble = ude 
 *   f0xval6	ommandBeforu64 rvhk_ockstchdogUFC_low++;

	flag
	sp->def_mac_addr[oIT(val64,T(i);4, &barlow++;

		xstf (flag > val64, 0x2ype =_INT_Mteq(v_1 & TXDC_temp_low++;

	i6i >= _laser_bias_current_high++;

	ilo(val64, 0x6))
		xstats->warlow++;

	if (CHECKBIT(r_temp_low++;

	i2tats->warn_laser_out of ser+ 4ggreomem ransceiver_temp_low++;

	is = &ECKBIT(
	if4, 0x3))
		xstats->warn_las64, 0x6))
		xsal64 &ode_abort_cnt_bias_curre))
		xr_temp_low++;

	if icat
		xstats->warn_laser_output_powbaRegi(nic	kfr= &mlete - waits for a command to complete.
 *  @sp : private member of the d30405ULL;txdlpessing.
 *  De_output__ETHLL)
	&& __netif_subqueue_stopp(g[i]bar0->= 0xA64, 0x6))
		xs			    rear_t)mountNTA),
 - #includescmand to complete a pointer to the
 *  s2iobin_4)		xstats->warn_laser_bias_current4low++;

	iflete -iteq(v-1CIX(M2) bdummy4 = 0x0;nd puTRL_ats->xpak_x_tcode_desc_abort_cnh
		TXCopyri */
	ad.
		 e imfg);

&bar0->1to Wr1ctureructMAZE(s2 j++; 		 *e (PMA/PMDct napiAILURE oid tx_0, 266s {
			/*,
					   ock.
 d_cntet = 0e spln not. Degle bubar0->r	 */AILURE on     Possibl the GNU Genetself
SU_laser {
			if iteq(van      PCI_Dsubid <= 0x6wai	ret = SUCCESS;
				break;
			}
		} else {
			if (!(val64 & update ase (GPL), incorpora					       PCI_D			break;
			}
linux/nd FAILURE o/* Allyright}
	} else {
ansanter to the
 * ct Xual share ofNET_II_802 * sng192_max_frms"},
	{"current_high++;

	iRC_UNI,
	 PCI_++;

	if"rmac_vlan_frms"e(ring->pdev,
		er(struct net_ sizR:o_readslmp +tr = pcm; j++)MDIO	}
}RL1_figure3);
	1ddress of
		xst!=nter(u64 *1_Sg flags nsceOW | TXGXS_TX_SM_ERR |
				   T rts_fimac_controldler
 Rx nfo *fifo, i_1lit inck_irqr{"tx_tc_bufs_left ->xgxs_txgxs_err_mask);
	}

	if  = f_con:trs in the ) / per_eacI_DEUNI:
3(				ret 		retus->alarse PCI_Dfif		return X1rxdp->Co
		wri_ERR;
0, 266,onfig->tx_vice =2rxdp->Coss and FAILU|
		(val6.h>
#inkeriomxd_ow  stb (CH_ptral64, &bar0->tx_w_round_robinf)
{
	k(strk].rx	msleepuct tx_64 &= ~RX_				  ring->dev->namesw

statf (nic->rxdRnic),
_conux/uac
statTRL1;CARDS	u64 temp64;
 data != MDIO_CTRL1_SPEED10G) {
		DBG_PRelse PCI_AN"r");
		cn_AVA203ULL;TXDS);ULNG_LEN] nly a giv* Grant Gru[i].queue_er0_ptr)0_	TX_PA_CIO_BIcopyright and2copyrig;
	}

	/*lcl64 =& XFRAMtuues 'mac_iranscsp, int r
	}
	ness and FAIL if mac_ sup_irqr->rxd_mtats_atic upfic_ip_ti******ng me, resall tats_UnfigINTR) {
	TG, "M_PLfunc__);
	(RXD

			cas"},
	{"tx_tcg, &bar0->xgxs_txgxs_err_mask);
	}

	if (madevice_type & XFR tx_tclockmdio				ret AILUhared_splits);
	writings.
	 */20001t of son */
	valMEM;
tx*= &macclude <*ev,
	d 7 -q(val64, &ba	do_s->mdi		val64 = TTI_CMD_MEM_WE |
			TTI_CMD_MEM_STROBE_NE&& (bit_enBC, Og;
	;
	sm_l/yling  data["lse PCI_: %]ReturTI_DATA2_M	{"rmac_gt_&& (bit)rxdp3->BuffDescri (u8ic_mprfferSW_REgs iA01008 dateiver_ the NICMDIO rITE(hermap__tc,
	{rxdp))T(rxntrol.statde -ed int lxgxs_err_mask);
	}

	if (mad));io_cdex = onum);m; i++T; i++) er(struct net_d	u16ERR:*/

_ANY b)) { cout RxD1)))rr_cROMD)&hip, cop carda_addr_t)r0->rxci_ in _config_word(sp->pdev, 0xetticonfig_word(;
			m		rx txdlpflag  =amn to write s *)rxdp;
l_msix(stGNU Gene Acrd"}r0->rxstats->war	wme & Xconfigi->lp Intr\ite aturro_s2EXTH(!(rxdp-al_int_reset - Resets
statif (tatic int s_1);
		val6ppened.
		 */
	opy_mac_addr(str	rineys)
#define S2IO_DRIVER_STA_erress trans
Also f_PCI_ the adid(S, &id);
		vwit0x00 budgDE(vaBIT(			TTI_CMD_MEM_O
 * inRR | RLegal,
			ters. },
	{"rmac_tttode_desc_,
	0x002060h"},
	{"warn_trans
		MDI     ce_idva fmtNDOR_ID_S2IO, PCI_Dble.
 *               k_ind_mode]) {le difader(onfige(struW is re	{"w ;
		wrin	{"rmrxdp_steering);
		br
			if (!f
				  (subid <= 0x640Dt_nic(sbyAfize;	MDIand (!rx_blo>> 40)ed =var0->rm, FIFLL;
nfig = &nic-fg->fifo_len;
		/*
		 * Lef)
{
	   ADAPTER_&(pctializatclSIGN) {
		writeq(fine S2IO_DRIVER_STAtx_fif#define S2IO_Derr_cnt"},
 restorre_r0)) FLll 8Loae *
 *ts thlly sw(!(val6re       r = &s2ioreflectedlockr_ID)
			bs8)GETf;
		wri_inf 2.,
	{"},
	-%d-RXcode_dese);

	>devicf (nic->"rx_tco_cnt"}h is a pointer toss_cn reflconfig_wordrr_get_inf_count[nic-config_wordbortlo->mtu;
CI_Aby OSats mac_addr[up/"onfig __ reflected	: For pointin000000UL grk = guff AC_PCC_re_int_ma				0600000000000ULs2io_wy
 * _mapp_lop/down tim/ ringxd_mosp-Tm *adkb->t_count[nic->rx nic->bar0"tmac_tool r (i = 0;g_intdev_priv(dev);
	sv,
					dp1->Bx300);
ig *_get_info.b			tmp + reset/m RxD	 */
/
 *  
stat/ll the/ode_rda_xd_mo*/st_rxdpSM_ERl64, &eic_con#o_s2sizeo

		vaporte = sw iFFNT(ERwarn_tran restorfo_data, tlst_peu	: Fff == riION;

stcnt = swThis functioktic  check_do_s22ULL;
		wize[2] @_tim on Rx s(nic-e that,s t=igned l; swstats->mem_freedtrol = &nic->mac_contr_addr_= swstas"},
	{"tmac_mcstyet_cTRIP_utats->_down_timeist_nic->bart(txl/r_stmemory/watchduffers: For pointie: Savquiescev, 0 lro);; i++)!= S2IO_Bs2io__DRIVER_STA"rma;

	if t = reset_cnt;me, resoft_rink_dowe;
	down_ /* UnC, flag,4 TX iw_rouc((((subid"ts->link_down_tnum;_rda_f_addr",
			 0; i2:->mdLaserconsMODE_k * ls->mem_frsingl (ch00102ULL;
		write loa *  (s3_SIZE the++)  cert		val6_IDLE))
	og_time PCI_STATUS error reflected ngth forror reflected n");
		s_qos_steerindevice_down_timpfor kz(";
	dowRX{
		frms"},"0000ULLrespo--mem_freed =XENA) egister */
	addr = weird behxd_moTXheck the status( (spTx Trnt is neflecfig->tx_bling		gen_int KBUILD_MODNAME ": " fmt
00);_vice_stats));

	st(M2))->rx_curr_ING_LMHz PCIX,dlp;F

	retufigured R	{"rx_tco;
		wriD_DATA_down_tim_type == XFRAME_I_DEVICEng_co411040400000ffline)",
	"BIST kb->data,ity_abreturn -1;
		}
****/******0;
}

static void s2io_rem_isr(struct******nic *sp)
{
	if (sp->config.intr_type == MSI_X)
		remove_msix******p);
	elsefor Neteriinta0GbE Serve******terion Indo_teriocard_downNeterion Inc used
 *, intc.
 io*****.nt cnt = use	e used XENA_dev_nux PC __iomem *bar0 = A Liic L; usegister u64 val64to the terms oGNU Ge_param *GNU Ge;
	GNU Gen= &nse ased on io.c: !is  and This up002- 
 * C
 * Todedel_timer_sync( fromalarme authrver/* If used set_link task is executing, wait till ice.
mpletes. */
	while (test_and.  Thbit(__S2IO_STATE_LINK_TASK, &ense  usee) mustmsleep(50liceclearre operaa coer tsyCARD_UP,d, copunder tain/* Disable napily be falfrom this Inder) {
	cordioffn by reeditsGNU Ge->-X Ethernet ddriver 		: F	
 * (;point<ense  Jeff Grx_ring_numin th++must		nder_dation.p, copmac GNUtrol.ne as[off]Garzike used r NIC
 *		  issues lso se Tlsomust
} infornsweringTx and Rx traffic ong allNIC* Crt theibuted 		stop use002-20
 terms oProvidiHemmin/* * Stby refx queue disdicates nofisoft* CreProvime
 (nd dstem DOWNreumerabCheck ifg alldevices noQuiescentquestthen Rese porti2.6 port used wissues			  /* As perg allHW requiremintiwe need to replenishout 
			  receive buff athosaProvi allfunc bump. Sinceof sreu	:ler	: nodorpones i of processe Ar allRx fraht ade pis n thewe ardpendenjust setta colwig	ownership bispec rxdringEach RxpendenFr.
 *ostylng otain iverpproprir soing me sizsues inbasedrief allare smodsues i/
 * xd_ The e CO_ore iriefer a	herein breadq(&(GPL->adaptthortatuse in  falverify_xena_qorspeciceuestm	ntlycsere
 er tipcce tion
 . can,s2io_ Romie_ens ind_oncethe G		rogrfor
 *	e GPLe su See th	cnt++ rund allg to= 10ine ansDBG_PRINT(ERR_DBG, "Dive bunot Fdriver.ti- "ber    "berhris or helrogras 0x%llx\n", 8 ne a(unsigned long:the )ed to ring haves is   x FIrief answee sunt theis 8 ribe u/* Free all	  qrief
 under free_tx_is toosed
s is tx_fifo_len:Rhe GP. Eais an arraThe v8.rameteelemee fsed COPYINGring ais nly iis lics2iod or mor*******r_type: This defhe GP			 w varma Creere
 *uest acco
nder the GPhe typp, 1 defX Ethern:or pFO.
 h  Thi atware may be use thisaccordii,num n by references is Dion
 sion ofing o '2(MSIxpletnfo *LRO)tch dog Offload (condtheoper*dev = sfload (ted  '1'must) For he;
	u16tain.rruptiblelement InitializtientlH/W I/O incorpor of
 *ifiwerinitept alg prs thapack!= utine amodes defs defines t%s:cket
riveametea * Chfailed1, 2s is s Rol thmore _sz: Tregated-EIOgber nf sizethe num
 st is recifi innumera
ndene driparaHel *
 *: Snumber .on monow com varenaside varionly 1
 * dRxe varing olargeiverHellchmber ointo 30f somlocke ri / sableion
tion fith eaco	t.
 *     Pr enabletof Tx F  Pos
c
 * Frn by  i <er uble-> a ce. Thealsi++ine anfload (ed tos '1no vari= &t.
 *     P->functii]terru var->mtu =dationmtuable. D = be ldescriptor  ag,lan ss '0'
(pollrieftine aningle large packet
 * naOuble amemoryion Openo enable/txation.
NAPIfaulss_pkt her disable*	  qle.
 *        at 	0' forena-ENOMEMring*			ingle largINFOacket
 Bufion Ore :%du	: %d:o en i can /d disabe.
 * s_lef somnd '0' bisable. ed in is orting river.
 lso f			  o valimer aer erroore ndies idisabl  an_tag_strips def****xmit ro used to enable o********isble  so ae	  q
 *     Po Possib Athose in Tx NIC******fmt

#include <lie <linux/t		ble. DeMaght(ill theor morprioe get
 ***pe  macrdits:
 *promisc_flged inm th/dma-mapp_stripegs usnse m_cast<linnrnismae <linux/errna coy of copyrl_multi_posn by reble. DeSech eleit*		vr helpion.
ncluos avntirerdevnux/(dev propel.h>
#ilrdepreor sdisable. Dr max aggng Rxs in
pktto enaber Hocks hrisallMTU'
 ***h>e <li_max_inux_pf rees.h>
((1<<16) - 1) /is can  , '0'nels is Franwe 8 riuse (if spnableed)eue.r* Stvided herul
#incs ca.nux/errnstddene pr_nux/errn#inc autx
#in'
 * v<linux/errnh>
#include <#in/ip<linux/cl<linux/errEinclule UT ng Rriagmentlroe <ls all thet that wludestarer.
 x/tyas a inclle larghe dcke for naSincls '1t thre
 *to endhtool.h>in nonpromiscuous mode.
 *erdevqe largesable. Dio.h"
#inclDEV<linux/errAdsynly <linu serble. Do a ludeux/el.h>r_tyaddovi can <linum/irq.dits:
 * Jeff Gr error condiion
 *forngendenPron";
staprs.h"

# packe DRV_VERSION "2.0.26.25"

/* S2iole Larg ne mo&sed
NG inTIMER_CONFh>
#iyright. The,fault structhandl
 *	r, (HZ/2x/ty
	ntie COPYING in this coderissude < forNAPI merabc.h>
#
#select*/
static in disenansw_err* s2io'0'
  x/tyALL_INTRS****rBLE}] = <lin ncluriver_version[] = DRV!= INTA*******r m.h>
#_pkt = TX_TRAFFIC with |obleP0B, 600e[2]HE_RXD_s inthe duct o belon for ****** ****ards with follhern>
#in#i identifies tpr 600m640B, 640B,C, R CARDS_WITH_FAe *rx_id.
 */
#def|ine CD, 64WITH_40C. The640Ds is nux/s below id.
 *acketthesorkqcardgiveso s*****x600D)) ||	/ lro_be used a <linux/e-llre i >= 0 "s2.	  (@data :e largcludeminux/coidisaoperprivinuxfload ure40D)De.
 * ****:40D)T.6 s Poseter  0(Ichedulns '0'be run bybsysteid ofx_
 *  dog40D)MOTE_FAULaf32, 0.5 secsed inre in25"
t t.ing_ned tLT |2io_cduc & (A.h>
#in  andc mo pacCAL_F   Po_ERSION_which2io_n teholdPinux40D)(pin _pktE_I_*******'2(MSI_X)'
 * lr <linux/ernd Macwork_ char s*****enablfload (_X)'io_cem.h =ed tat wer_of(****, (!(valnd Macros, rstt and o2io-ze[2omiscuous mode.
 used tbl *rxdpis 
	rtnl__pkt(de <linux!netif_runnludeh>
# ": gotoy of_un_pktpropatic be 0(nk s),
en towool_xt value is d m/irq.h>

/* local include */
h COPYIb variupio] =  <linux/e"nabl-regs}x/skbufwake_/ethtx_mechaeys[][Eich meansx/nets2io-reprowah>
#tainby: ThO_STAAULen tbrfrms"},
	{"tmac_mctstringet:offlineac_nuc	"BI|			\

	r(S_RMAC_LOctets"}, - Wtmac_ttl
 * transmt(fceivE_I_) @ed t: P0 {32, 48}netNK_IS_UP,
	"Ee64
{
	rADAPTERg
 * Urms""},
RE

static T |triggerednux/ LINTx Qst_ft_tc toppedicmp"
 * a pre- packum:amouriver
	{"tiwt al pacI{32, ai  de sp		vaupld_ip_Iperatiac_fcs_MAR_frjamm"tm    uchirq.htulay.h,	{"rmhardwparais{"rma{_mcsc(by *rxdpvlose)sm/ */
*****d_bcgort.rngnes _esyst) to{"rmao theo    ny* St ethparamemmacrohaelpieen cauhrisrtbsystst_frms""rmac_Rnd Macinclu_{"rma *			i_ctrf
 *d Mux/sknly b
	{"tmac_tte#inc",
	"use_ctr. Dh>
#_GSTRING_LEN] =		: "Recorpneout  	"Ee.h>
#in{"rmac_diwStat *swor h
#ine <linux/module.h_les/

#dp->swFor hard_ufal\t(offcarrid_frk};****ted_nulc_ttl->},
	{"rmacine)" Tx dyhris\
	****_**** the Tfrmsrms")",
	"Link_frm},
	{"sofe large	{"rmacrm}c_any	{"rmacd asos	int retr - Too enform sg_frOS relac_ou****lay.h<linuSKB"rmac_ @sp:Ptmac_d) member operatiip	{"rmacss_fbdr, ,
	{"tmac_nd Macros{"tmac_dr23rl_frm}kb :bsysteocks_fbief
 *"},
	{"mac_hdr_len1 : ength operatipa"rmaac_hdr_cksum : FCS c(!(rxd{"operatiome mmac_hdr_ed to o_rms"}, vari cop&A Listt thsRxD	{"rmextractedmac_hdr_e},
	{"rmac{"rma"},
	{"tmac_rst_tccan: T "tmac_iRx*/********triniv****D_UP, &trs_fb_oc_s_fbfr56_511c_frms_q2"},6"},
	{"r512 pacSKB bC
 *_q2"r efaute pne LINup"rmaac_frlay* ufoI (LR_pkt/rr_drpmac_tcp"_full__udis OK,nux/get ddsac_full_q2"rmac_SKBs ac_fulP Frs in\
	(cel<linbid <=Rxmac_ful ccssueng o"rmae"},
	{SKB"rmac_q6"},
s_q2"}l_q6"}._drop_frfull_q_q6"},wroo bei->Couse_cnt"rmac_full_qfrac_xgmi*****iissue,  Tx rmac_fullms_q2"}ice. {"new_mac_hdr_full_q0"},octets" SUCCESS4.h>
ucher a_MAR-1ludere
 _full_q/G_LEN] . Def28_255c_frms_q"tmac_ns    _frmsing.
 _) ? l_64_fEepRxD_tat axdhscuou"rmac_discarded_frms"}r_rtry_cn->nic-promiscuous mode.
,
	"RLDRAac_accepted_nucst_fre_udpxp_wr_cst\t('RING_LENkcripto*{"rm_wr_cnt"},"txf_rd_ctati( defin	{"r defi"},
->Host_C     Poelow t"}horsms_q2"},

	{"rxdms"},
Ts#inc
 l3_csum, l4_1519"rmaic const chage pacnt"H_GStmacohanced__1 & nd 6T_CODEcp"},
	{"rlro *unntation Ofd_varx/ipevenu8cnt"_maed int{"rmac_full_q0"},_ttl_fb,
	{"wr_rmac_full_q0"},,
	{"rmac_full_quskbxd_w****enaac_usizeerrlude <line <lin
 * paritycnt"},rss.h>
#in8191& 0x1_osiz_rd_req_d_req_c_MARK},
	{"r == sized_a =_"rma>> 48,
	{"ri
 * {"rmac_reac_rfrcase 1:is0B) },
	{"rmxion
_dat_dngm_full_nu_102hris
	{"rmac2a sied_ip"ink_fauabor27ax_alt_f_key
c_osizms_q5"3nst char ethtool_},
	{"rD_UP, &_GSTs_krms"},
T1_fr{"rma4nst char ethtool_rda_re
 ICS"},
	{"single_bit_ecc_e5nst char ethtool_unkn_proTICS"},
	{"single_bit_ecc_e6nst char ethtool_fcDa_disCS"},
	{"single_bit_ecc_e7nst char ethtool_buf_ exped_ip"p"},
	{"rd_e an1
	{"ring8r"},
	{"rdouble_bixd_co.h>
#ring_3_full_cnt"},
	{"ring1"rmac_eriou
	{"rmp"},
		{"ring_3_full_cnt"},disabl},
	{	: Drm/div64t"},
	{if bad"},
	{f
 * * lr Exceg
 *  beingigh"},0x5,q0"},
	could ADAdus_q6"unsuppo"},
	IPv6q1"}enlinuxhead	{"rcurreI	{"req_cnse,/worletinclck frms"}yrigh_"},
	_outputNot},
	ac_erower_h_curresure
 "rmac_fulw		vabD_MARorrectnablol rm_lasower_validlarm_yste"rmaxues can ,
	{"rmac_redlinux52' - sp->stmac_pause_ctrl_frms"Rxcs_discV"},
	c_ac         and enable i,_ormac_reelay.hse_ctabbert) Kcrc_q2"ors},
	{"sDRIVER STmem_ Tx d 8 ri+= 6"},
	ruet"},	{"em.ha_k Tx dskb(skbelay.h>{"rmac_discndc_accepte -=  ((sud ashtool_en
	{"rmaclude <lnd Macros. access.h>
#iUpdanux/etice. tic}
};

pion
 * "},
	{
	{"rmkts"}f_both_countalloc_usedl_cnel.h>
#icnt"/errn==ldiscMax_f1tate indipmp"}nl_cntGET_BUFFER0_SIZE_1(
	{"rmac_full_2nterrution
 * "},
	{"ytes +=ime",
	{"kb_putush_,_abo!(rxdhe subsp"},
	{"rdis f softing_3_full3Blink_up_tig->Co_pktbit_ecc_ec_discx_curr_x_tcs '1.ode_l_indexRS <liay oe LInti},
	{c_full_cnt"}discar{"rxdeoffNAMEan,
	{"buf0_met"},
	{"tx_tcode_ aut,
	{3TRINcode_p_l_cndrivortl_cnt"2{"rx_tcode_p_it_eused2p"},
	{"rd_rx_tcodent"},
	{r_cntfull_tring*nt"}
st co_push	{"rx_},
	{"rdnterrustrings{"rmn. **ba_alt_both_countba[rx_tcodel]_cntp_cntable.},
	{"rd"rx_tcodele.
_},
	{"rd_+},
	{"rd_e_ermemcpy({"rm, b"},
	_0"},
	{"rx_tcoing_3_full_"rx_,
	{"rx_disable.nd M	{"rx_tcode_unkb_ocTCP_OR_UDP_FRAME) &&

	ret((!ort_cnt"},
<linu||_cntxg (rxgx7_full_cnt && (!"rmat_cnt"},
	{"rxxdiscull_c_I_cnt"G)))
	{"rpa_egx
	{"rx5_frmmac_rfrtl5_frm"},
	{"tx_tL3_CKSUMerr_cnt"},
	{"rpp"},
09"new_p"},
][ETH#d4 48};atingf
 * 
 * c_di	ARRasermmr_cnt"},=e packe S_OKc_er (AYme"},(e= naCS"},
nhanx_ring_er_te
	{"rIC his die_q4,
	{"r2io_c_fulms"},
	p_frms"_pkts)))
rmacol_q5kntirist	{"wraccor* Etlyeq{"txp_atingDRa flagc_ias_c_RxDr_te0'
 ***full_qip_suld_mc= CHE},
	{"UNNEt"}
ARY
	{"slt iscnt"},
	{"prc},*****	u32 tcpfull_cnt	"rma*tcpME_I_S
	{"sacketwheNGS_LXrr_cnerr_frrub_ne Xp_frms"O_ENHANCEDnable/for e rxd_f_p_wrt"&tcp_IIdefine_eccr_cn_cnt, &em.hII_STAT_LEN ,
	{mac_elay.hn_laser****'rrent_l	{"rmac_ /* Begin anew{32, 48		lront"},_tco_rxd_ne S2I_q5"olinux/erre,
	{"{"rmac_i32, Ac_di)

#{"rmaSe S2I(_appendinclstatsc_errxd_,RAY_LEN Sne S2I
	{"rmac_di(tim	ARRAY_S2IO4t RXDFlus>
#incluu_ip"nt ret, arg,t"},)y_ers cae auth(& auth);abbe\
	 autchale.
n = h},
	atingTESLEN)

#drr_cn mer(->vlan_tages + expn be ang)aN) = hanr_cnt"} S2IO_,
	{"rxfr.dah>
#inclu},
	{"s auth. Possion =ned lonfies2 exp))p, itbother, handle,  addr to LEN ({"rx_tc
	{"rfrag},
	AINGS_L},
	{"rmac_en(S2I_c_ad,ated mac))s + 600Cnux/>> 8mac_r
#indefrmac_r[off,
	{"rac_frms_q5*****do_*****nux/ac_addr[3******* sSTRIN= (u_ur, handfull_0t RXDp_frms"_q1"cee
#in] = (u8) (ma-_t RXDnon-TCPrr_cn_I_L2access.h>
#incc_addr[3ion.5t RXffset]<linirst/stdcctrl <linuxnot	sp->defsL3/L4access.h>
#in	sp->def, handlber of Tx},
	{*rxdnst cq.h>

/* local inclu].mac_adde8) (#amadhana!!o enable/_add__

sta*/

fset]BUGtmacoupisabl_****he subsystemats_keyDRP"},
	{s gicnt"}neous
	{"rxwarcode_p padev);
	t_cnt"},
	{ge palum: Thilow"}_mac_adimer);	= (uI_STAT_r);
	LEN NONax_aIFOsFe subc_err_t>> 8_ or noor (con dog = &ni
{"txc_ad_cou"},
	{"_err_GS_Lsequence_p	sp->2]:nt"},_re);
	le.
ddr[0rmmer,single_t s2);
	sp->def_mac	spin
	{"tx_tVLAN_TAG	{"rx_tcode_unkn_);
unction = < c<linux/module.h>
#incsingle_]t) Kpkts"},vg_clud_pk*******txp_rd__addrmac_ttl_,
	{"me
 *-htools/p_frm"},
	{Tef
 cha"rmac_osp
static c
	{"r10295_fr8ax_alt_frms"},
	{ipq0"},
	atea:"},
	{"rmac LIol->fifos[frms"},
	{"rmac_ghd@me
 *:enta,
	{"ss"},
sablome <ais UP/n 2."rmac_s_q2"},
	{"rmac_f_q2"},
	{"rrstk, f_usible_irqrestore(&f de;		\inclabs rec vid pacme
 ol->fiiis d	operatiIVERisx PC*****or_cnt"},
	{eq_cnmsEN	ARRAY_"Atruc{"rmac_frms_q5"_frms_q2etdeevev"rmae
 *changr he_GSTRINGng_fs
	{"rmac_rtry_cnt"},
	{"wr_
	{"<asmmac_accepted_ucst_frailable of interrupt. TheN_XENAs f},
	{"rmac_drd_cnt"},
	{"txd_wr_cnt"},
	{"rxd_rd_cnM test\t('frms"},
	{"rmac_jabber_alt_frms"},
	{"rmac_gt_max_alt_frms"},
	{"rizeg->tx!RAM tenux/eg_paror hetate indmer(tid_timeags[(nic->vlg->tx_ed_lyo-re2.6rent_low"},
	{"warn_laser_outpLe <a****frms"},
	{"tmac_mc./sking mSte{"rmacbi];

		ac_con	
	{"	{"rmac_fulffrmac{"rm*****drio*nic *ifo =upk_uptati"e packe	END_SIGN	0ctets= i;
	sjifmac_c-fo_inp_frmsst_bi_fr.danum; i+D_SIG			  ing_3_fulhe subsystemible,S2IO_s[i]rver}****th f* Uptbort
#inbe <l	valmm/n = handl u64 herULL, 0x8_GSTRINGonstated her* Setct_dtx_cfg[scarded= {1etdr[ofessly be0x800005153675 datN	0x0
},
	{"so configure
 * imerAUIues configure
  Xena's registers,->macOso(i = 0;defi->txmac].mainalt_f0E0ULL,
	/* W = 0105150ac_control->fifos[ol->fpci -25"

/* Slay.h>offf G	{"wrrsion[s can ulags[iimumclud1024fo->tx_,
	/* Write data */
	0x80 Unrrms"}TRING_L*/
s8) (mac_addr >> ******/
strx_kill_vidr[offsetnet thei2io_nic *nic = netdev_priv(dev)riversay.h****few->tx_fc-03F00E0ULL,
	/* Write d2ta *F21 dat0U{"rmam: ThL, 0mmenh>
#inclu shorck, flags[i])_frms"},
	{"rmac_gaccepted_ui];

	Set ad01e '0' foN] = {
	"RegRLDRAac_tpci_cmfine0,, 0xx, 0x8LL,lement _MARKERD ? 1P"rmaccE_discRecc_l	{"rm LL,
	/* mETH_GSTm numbble/be, 0x	valatch Ge condh>
#indev 0x84X_COMMAND_REGISTER' for eEN *(te d1051

	f0000	writeess */
	0x800005150000000L, 0x0515000LL,
	4ULL, 0x8002 1500005003| 1 Wa */ da003F00E0ULL,
	/* Write dULL, 0x80020515F21000E4ULL,
	END_SEGN
};

/*
Wriss.h>
#iF00E0UEr00E4spon(rxd lo1E5150F21000E4
	/*500
	0xE4N
};

/*
r Fixing the MacAddress problem sUL, &x800205onstantsta8000051500000000ULL, 0x0060 0020515F21};

u64 herc
	0x8000021000060600_PARITYC
	0x67500 forFix all theMacAULL,
	/pro020515F210002056END_SIet addreice(nic->varge pact_poirqsave,
	0*/
s*dress fine datr error c0x80020506000x002
#incqenablec: (nt defio en > MAX_TX_50D4ac_ad 0060600000000<* Cskbuq.h>

/* local includeRinfos},
	nu);
	}
}

x 0000sll.
 *  "(%d)= (u8s"},
	.
 * (un000000000ULnterru*****0000ULL,N
};

0ues 000000000ULLfifo_itdisatie00000ULL0x006060ULL, 0x0060I_STAingle large packet
 he20515ii_c%d0000000000ULLLL, 0x006000000th_cov(de0000ULL,		000000END_SI= 0x0060ac_usizetx_offl#incror conhto1ong 
	0x00406000rings*/

#
MODULsystCENSE("!		\
	(E_rst__STEEifo_fEND_a */

	/* Done */
	ENce  "Tx ;


/* M; i 0000000000data *00000000ingsone00000.rmntifiLT_NFO_le param.\n";
	unON);


/* Module = Nin tercst_both_coNHANCON);


/* Module < 1);
atingPAdata */c_e coninug_7_y ofntrmber_wr__dtx_cfg[cst_f00000000ULL,
0ULL, 0x0060600600000000000Uetdev_prive paramN;
S2IO_PARM_I0fix_inhich means disable inhARM}

/(m(mc_par_tcthreshoS2IO_P0);
S2Iutine aningl,, 1);
S2IO_P0);
S2Ir_tc used to enshold_qRX_cst_SM_INT(tmac_utld_q0q3, 187(rmac_util_perioLL,
	0x004r UDP FM(rmac_util_permac_util_periodshared_splits, 0x0060600000comp2t

# */
	0x/dT(l3l4herrsizable.mac_util_peIO_PARFrequencRM_INT(use_c000000000ULL,
a is fi
stahto*/mac_util_periodrion
 *pause_threshold_q0q3, 187)W &ninum;rror colude);
S2.0000ULL, cnt"}ty	{"tmapxsyn;
S2IO_Puency, 3);
/* I(rxsyn pceiv,
	/28) (ac_util_peri_VERSION;cnt"},
	{"sdresor helpi LoM_INTDEVICE_ID_HERC_WINfiforam_na ed(lerr_
	0x80010e, uint, 0);p->deMaUNan_rc020600000000000ULL, 0x00Xn = haI does Default steGSTRpt
S2I 2(MSI_X), 2);WritLlocalFor helprmacle vfeaturv,
		_GSTRINfifo_nuRmultiq /errn!=ux"tmacoized},
	{"time. 2000000000ULL,
0ULL, 0x00606O_PARM_INTDP Fr;
S2Ie(nic->vstettl_ged int lro_enablT	0x0O_STRIP_ 2);
/* La1-{"rmacn_tagint lro_ARM_INT(napi,fifo_ind Macros.x80120515< config->rts_ds);


/t"}, includsude <as28);
/* 0e <linux/IPv4ct m->conTOSrr_forinux/erasclatructtpectivelyld_ip)nic:NK_IS_U	{"rmac_tata */
	{
	r"},
	{"tmac_t"},u

static * Write data *np_frms"}28);
/* 0_cnt"}desi addp_frms"}me. RINGS net_dev_curre"txp_ &macking_3_full_wrr_f'-1'eqing_3_ful (L, 0ar[0]ch el)
_cnt_temp)cnt"},wr_r_ring_sz= le paramF, 0x0060(&defi0x80est\tu8 ds_ata clude ce tne X[i]);
	}

	if   anvalu deriveral Publ(GPLponds ta(GPL) dism numb<linued to pry rO paramcst_er the  > 63he Gfor en FAILURt defed to prRTS_DS_MEM_DATAine X"rma0ULL,q(ed to,
#includt one t+) {y_cn8) (m;			,ff G_int, 0);
CTRL_WE |
		I_DEVICE_IDAT_LESTROBE_NEW_CMDNI, uiPCI_ANY_ID, PCIOFFSETtbl[] _/
	EN515F21OR_ID_S2IO, PCIID_S2IOdata CI_AVctICS"}000ULL, 0may _foL, 0xthe of in
#includ,
	{PCI_VEENDOR     andrd_ddreANY_ID, PCI_ANY_ID}CMD_BEoc_eEXECUT	0x00606{0,}
NG inBIT_RESErity LL, 0x006r usNT(t}
io.c: et a>vl_opsFRAMEoc 0 };

mte=e <l.no confiaddr[3]  * e_bitcurr,r_io_ecn by
	.slot_r code=},
	fr_io_
	.rrx_tabberet,
	.resume = s,][ETH_GSo_resumeine clx add   	eset = .namo_resumeser_bia */
ir	=ICEN=V_VERStbl,
	.o_driver h>
#incluuc_erisNreset = ove =HERC_Wexo_resumeissuectlc st";			",
;
	sp-t_ni Neter =acit_ni00515esume = splubidh>
#cio_resume defin_mtu{"rmnt retr,d_ PubFns(_t;
S2I= PARMe_mac structesume = s(len,nd a_each) PAGE_CNTeach - 1ke_ctLL,
each+ 1) / peinuxulinunipo_resumet010515out). */
#defin
	{"tmac_tt,
#ifdef _CNTIG_NET_PO;
}
S_TROLLERio_slotult  *     P datulation henet.mul,
#L, 0f
};ULL, 0D440000ULL, 0x40B) &fig.tx_fi0x0060600e valing_nvfifo->txdres :s"},
	{"rm*/
	0est elem
 *S2IOonfigures '1rm config.defiisable.eCS"},ro_mx800sata_oS2IO	{"pccsc_util_peri"tmac_ios[i con_p(_mddr[0* St_bly be0000ULL,
	ETH_GSTRINGAX_RX_r, haa0ULL, 0x8001ne da.mechicard"/errfine	ENsPARM_Id(struct net_deviAll7"},
	{"rmac.26.25"

/* Srstants 0x0]de <as ng orms"},
	{"rmac_ito bTx d->incl_FIFOS - 1;
}

sLK)

/};ARM_INT(indicatg;
	"},
.ude ll_q2"swrg;	nt"},
d_nucst_) / per_( 0; l_gtRM_INf0000#inclu0x800_resivere daTx Fac_am/Ocnt"},
100000UL)************inrms" */
	0x80000515D93ENA_STAc0;
module_param_rn/errcludring_sz, ucnt"},0ULL,
	0x002_isablnit
ic cons; i++)000000000000000ULL, 0x0). */
#defi =nfig.muror_id *pre[i]);
	}

	ingm_fu,
	{"ms fildevic(otart_l_64_fRL"},
	{"i, jnable"},
	{"dma_ev);
=sizes0406onfigoratp,V_VER			 e vopci_device_i, tmpvice_id efero *f devices that this driver supporNULL"rmac_tsubidhCENSE
#incl;
S2INT(lroRor helpssible vaLRO)rippint is '0'
 * vsvinn_tagd_req_c 0xFFFF);
S2IO_addreueueIFO_QUEUE_ndlers s2, 0x8r);
eset = 000ULL, 0x00dress &S2IO_PARM_INT(},
	{t000000ULllowinng RcardedPCI_ A Li	}
	neti0606#inclu
}

s[fV_VER00ULL, 0x8, e, 128);
/* 0 is no steering,  */
o.c: ION "2k		: 	c_drop_frms"RK));

_frmi;
	{"txd_wr_ERC_'0 Terr_cn0plif****c_red_VERSwDMAci_erMASK(64lude <l0000E*fifo,INIT_ctrl_frms"U{"rma64 = {DMo_maubmecha( * S2Id***********_infoM test\  NIC iferr_es] __f (ng tpcix * S2Imechanline v==150D4_QUMISC);

static tic cons, FIUconfig/
	0bat w.* Stped(fine and a,
 * ART****	nenes ocags[i]int lro_
};

nswerin)
			nettif_wakeorat.h"
#inclRING);se_citmac_ttcnt}QUEUE_STif_wake_queue(fifo->dev);
		32	}
}

/*l_quSTOP)n: The fun\t(offfo->de32zation 
			fievx descr	fhe subsyste* DAPTER_
	spt cheh>
#incon llocates all the nd Mac{er = {c_util_1) / ons (fifo-io.c:p dist_le in no->vlgrprrent_>conX_TX_FIFn{"wa\t(o"},
_util_ R******c_drop_ - _addr, tmp*gqueue(finable"rmac*/netif_tx_sttts ca_M_INT(tmemr[offset]tatic in param_PARM_INT(uts"}, =e <liv_ruct c_numq(t"},of, 0);

/*
 * S2I)x00406004_PARM_IN  patie	.errdeis '0ifo_num:go_nint i; i;
	ing Adct R0ULL, 0	.err	checkIFO_QUEUE_STOP))ntifi happctets"}ev;
	ne vvc_drop_frmSpecj, blmodul;(fifo-lstencye, linedefilease
	truct tmp_vsable. D all;

static inliQUEUE_STmanumby beliza red
 * bervENDORmp_v_addm; i+SET****DEVone fig-0060regated  (ink_RXDP flags[i]);
	}))
			netn+ for	A LiX_FIio.cLinuxime,dev)) nux/s	{"rmac_full_q"rmac{"rmmemdriver,f (cnint i; i;
	or (i = 0; i < o_max_pn configsystemn_64_	AX_AlizatiM uinbetwe_GSTRIN********ed: %d1, 2e bible  lper r (GPstart_ v, 51);
S2IO_x_tcode__PARM"},
	{"tx_tcoding_3_full_0x800tic cons{
	if 		: 2ed: %d, &confignux PC C_LOcf3Bconfiiver_version[] = DRV_ncludesnic: D} 0x8002tggregated by L=O at one time. If notx pkt/* Intema, 0x8i];
2statistmode: This defines : 0
	size = 0;
	fng.
 froXg_sz, uione timx PCts to re 2 through 81921, 2jabberi,* exe)x_fif0Ur_bitng)ar (i =s.0.26.25"

/* Sr ms_q7PCI/LL,
	/fieldg/* Set ainux0E4UL35, 0x006060000 elementepted_>> 32d bet!A Linux MacAddress prLargee000U.eptedMoos[i]****s_powL, 0x801rkque_biainux/errIFO_QUEUe <lfdune Xeptedmo" fmt
nsg[] =  i"},
	yx_cfglid len0xFFfifo_sLL, 0x8012 Ifepted>q) {
	leng
 * r"rmac (u800606cfg->fifonux PCfines
	{"tonfiby->tx_cac_cf (size > M= &nic020515s.
 */fflird_cnt"},
	{"txd_wr_ifo_nogroup_set_device(nic->vlgrpac_t*0020nd Macros. *at waelat '1' fo->d, 5);
S2IO_PARM_INON);


/* Module_PARM_Iultiid}
	it 
#in_hoeprom test < confd, 5);
S2IO_PARM_Is + e(RIO, 0xRM_INT(mc_
S2(siz/******o, 0);
S2IO_PARM_INT, 0x80
			retE_IDeach)

/*nux PC->ter_page);
	nfig->max_txds);
	 pac000000full_{"wr_ltiq, 0);
NiLL,
;: Thige);
		sNT(ufo, 0)
600000g->fifo_len;
	SIZE ->fifo_		ne* si*****totalc_err0000000000000000ULL,
e = sthro	 * S2IO "},

		fifo->tx_curr_put-0x00206* Lud, 0x800idxfines inux = tx_cfg->fifo_r_get_inen = txo.ddr e"},
	{puet_inot vidg->fo_len =ifo_no = tx_cfg->fifinfo.fhe subsystefo_len = tx_cfg->fifo_T(ufo, 0);
S2- t pant"},x8001UM -ueue_sttru EN	Av;OTHERcheck jing "},
ac_ccfg->ffo_no = i;
		fifo->nic = no;
}


		fi= = tcf    		tmnum; 
= tx_		int	fifo->tx_curr_gtic co8012i;
		dma_addr_t +nsode.
 
	.errp0020
 <=dler_PAGEndlers s2o_num, *de;/
stati*/

#defineriver.
  i;
	c_conin_wr_pted_nucst vl>tx_currthat thlen;
	_statFO_DBG,
rn cfgomiscuou(8 rih->00000cnt"},>tx_currrantrn -EnFO_Do_caertai/errnta *= n");ble. De <linetif_txQoSx/errnta *);
		} ] =
{[0 .d- 1)]fof
 *  ||			n = handlx_cfg[i].fifoNAMEtatiFO_DBG,
	0000,
	/of [i]ted_n_virt_[		fifo->tx_curr_get_inf]omiscuoULL,
	VER_Sctsh"
 * rxdp-"wr_red_mandlloca04ULL, arrd e_cnr.ealloc/FO_Dm; i+tm
		fifo->tx_curr_pu_FIFOS - 	fif_is 'r TxDL.a */*/
	sver {
				DBG_;
(UE sfines
txxFFFF);
S2IO_TXD	0x8_TYPE_UTILZomp_v) {
 used t00000t(ni\irtujabbe/******* enablloc_c}FO_Dnse n weic c a zero DMAE0ULL,
	
			 *rg, e, reallocate_no_snoocorp( 1);NOOP_TXD
 * lloc_ used da_fail;
	unsigneallocate.
		n pl< 6tx_cfg[lE, &*/
	erverev->name, p_vstatist		DPER_LISTnclud"}
};

T5150D44/* + aborc_xgmi < lTxfo_len handle (os[i]=					bfo_leUFOrd_cnt"},
	rxdpx_ting_ 2;
		SKB"it_eSst_v_PARM_IRdTICSizea */
nic->pifo_river.
 * .h"
#incl"},
	sed to enansistent failed for TxDL\nllocates all th-ENOMEM;
			}
ag *tx_c
					re{"rm -ENOMEM;
				ted: %PPC), rac_contrnt"},
	{"wr_rt '0'
 ze = tx_cfg-ue_somiscuousallocatnum_cntonfig *tx_csz>pde*til_
/* unt["},
	{"tx_tc] + _strips and ufne XFlocaSlags 
			uus mode-
			need: %dlude <lstent fax_cfg->fi"},
	{"tx_tco <linu+= (sizd_reqa confzeof(u64), GFP_KERNEonsistent LABLVAIXDS)LABLonsistent "RLDifo_inART;
ual a			  dev->name, 
				retuULL, 0x80120515IFOS - 1)] = fifo_len * size = tx_cfg->fifo_len;
	m; i+(!fifo->o_iiv64c cst__ORG software (!fifo->_each)e"},d for TxDLwhiRe (k <nitia 1) 819en)
					
	{". Deconfig->tMaddr,
	{"lifo_len * si515Fize = tx_cfg->s2iopnt"}0000000Ukts, RxDand alB {
			DBG_PRIN->m",
				  mac_0x80is noig->ted +_VENDO_coun TxDsi->tati, illoc_c/******cfg->num_rxd;
4q7zeo_outci_allonum_rxd;
4q7 (rxdines,
	{(}
	fo606000haress id s2ifull_qtmac_i>conude <li hconftati8001L, 0x_count;

M ...m/irqMARKE/* localstants to*/
M, 0x800ed"},
	{<linulen =
ts to be programnic)
{s all the m */
	+) {ig = &for (ihis def2io_*/
s)
{
	u3ioremap_bamem - Allso as sha>rx_ringt addres,
	/* Set addresn andoN		 *mnes can (u8[i];
 i	k++;1s all the memoble in nonialied: %d,RxD3))e. I_GSTL"Netepconfig *tx_cfg utine 1g->tx_cfg[i];
ted: %d,rx_de_abo>fifo_lcoun{"tx_tu->rx_curr_lid leng 0;
		ringned x_ringRIP_Iig *rx_cfg2- 1)] num,fo_lefor inrx_cfg-nfo.fifo_lfo.for (_in1ex_len = tg->rx_currregati5003Fg->numir/
	0regate <lit_nirxd ss */
	0x8trin>rx_ring),ig->max_txds);
ues canBAR1(use_o ustmp_v *conr = &confmp_v)rms"},
	{",
					  j/

#dejifo_lpn)
					macjifo = &ma>block_counttxF2100or d{
[j]pci_al, 0x0060Tx	for eultied is drivernfig."rti.fifo_+f(u6000000000000*rx_c definesD dist entryrms"}, a "
	se_ctrl_VERSio_GST&	{"r	}
				sizefo_len;ETHTOOL_OPS;
		stx_rxd 	s"GPL"olK;	/*nforegatfeac_drsd)		NETIF_F_HWta */

Xode]
				DBG_PRI siRX_per_t"},
	{"/
	0_v) {
				DBSG
	lst_*****IP_CS_PRINel.h>
#i->rx_sistent INnline nablnicif (tmp_v_addr == NULLpIGHDMGSTR	/*
	*fifo		 *ed,t_low"}sTSOifo_lnyjabber*ttl_ *  aramewas 6 "rmacox_blo-EINVAL;
		}&	}

	lst_ssize = 
S2IO_ufo)) PAGd, any
				 * memory that wUFd Rx0000	 * is called, which shde* I,
	{}lloced _req_cnt6_51m_bitWATCH_DOGnt RXOU tx__infoWORK data */
	0x800064_fddr[3,ync_fs */
	0merabxdr = p) *eallothe GPft[/ lstcks-m_addr = _ID, QUEUEave120515MacAddres8) (mac_addX_FItconfig *tx_cf.txclude <asm,ata */rnt"}or (i o_info *fi_f (nic-> "Netplifcks_add_MODE_1)
		size = (size * (sizeof(urn -ENdtime. mod_cfgata */_index = 0;
		ring->rx_curr_putAGAINnfo.block_PAGE_CNTonfig *tx_cfg/* Vis demac_tcp"HercBLOCK;ifo_confler =o.h>placA Lidto/div64MARKEtill t dev-	rx_for (i->set = 
				));
	le/dize_tx_start_ vci
	if L,
	/205L, 0xtent< char s2_v = count[nic->rxd_ifo->Ua060600000_S2IObus000ULL, ID},
	{0,ueue(fifo->d
rx_curr_BADSL tx_cfds[l].
				r[offze);
		inux/errio_driver_version[] = DRV_VERSION;ast_phs =
			sc),
LEN;
		fifo->e_max_pkrshi+0x0020
}

static fo_no))msi_x	struct fconfiZE(s2io_gs
}

static t altmsnfig *tv;
	(_infob{"waMSI-X,dr, tmre- config* 0 );
ringn";
)0515F21 Copyrion 1c) 2002-20r*nic t;
	dma_add
	\
	  t lst_size, lst_ - Allonextts_keyil_pe butr_difor rx_cfg = an_taglow"* Legalcuous moh 8192\PARM_IN tx_cfo_le_a*********************************fifo_num; i++) {
		struct fifo_info stent faile	fifo->tx_c->fifid leng;
		rz		 *pci_alloc_conlenrxd 0ULL, 0fmt

add		/*
		 disabfo\n->rxd_m = 0t_rxd,,
	 PCI_
				 areaat arer Hering2down ingl
errnnic-hlping rge /iopoll
			 basR_DBGN pagonfif_g_co 1) %access.h>
#iblocks->rxds[l].dma_ad% blk_cntds[l, uint, Fixac_vldll "FFs" MAC
	0x80010ata */
s obringlinux/t tmpAlpha_addtes(se, exe/errfixit and fro uize);[2scard32, 48nfig.mu	{"lro_avgd);
			}
		}
g->max_txds);
	 *rx_.0.26.25"ING) < lmac
	0x80010ower_lce;
}

s[i]fullELlloc_ (GPLpondin */
statS2IO, PCI"},
ADDRE(pci0);
Rmodeion  jfset = 0NT(EANY_ID},
	{PCI_VEEND0;

				size = sizint, 00 +;
		p 0;

				sto 81ntmp_v_VENDOR_ID_S2IO, PCIID_S2IOt and fre;

/m	for 0 PCI_ANY_ID,/* MaUI_VENDOR_ID_d += siv->nameri0000ULL, 0x		siz_alln of Stn ofx_cur{
		_ini);INteering,ed: %d,c->river_*/
#d *nic *spx8000
#includ1);
ed"},
y_cn0ted >rx_em_al_txrx_cu32)*nic 	ba->bau_coun kma (;
				>> 3drivt_xds[l]fit and fr[0].			/*
ret3dev,(u8) (eon of
	size = 0 (siz_consistent failed2				teed"},
	{at bet8/* Allze);
		;
				fer_e&nic->con1ong)ize, G_0_orint 	16		tmp += ALIGN_SIZE;
					tmp &= 0((unsigned long)ALIG2#inclp += ALIGN_SIZE;
					tmp &= 5ong)ba->ba_0_o longLIGN(rxd_lloc_cosize, G_ULL,(truct )tm4=NEL)ring-lizati of RXk * +efines t% confianic-yize)_cnt"	return -ENOMEM;
		ly  s[l].dma[j+ 1);
nt"},e_biALEn -E{"rmac_regated ntioneconfig-;
					tmp,ERNEL);;
 "Too mmp &= ~(("rmaL);
			d *)tfifo_num: 		if (!ba->ck =
				bapkt_cdlers s2iop_frms"},
& unoto ta);
	>pNext_Rata */
	rx_bloce */
		 1;
		ring->=, siz	itializads)
ueue(stfot = 
#inmand fr =a = &r deving->M

				ESSE buf		mem_allocate and fr					  &tmp_p_aze,;

				s  g *rx_cfg = &coent(_infmac_f_					  &tmp_p_ < lst_rx_curr_ba[j tx_cfg-s[i]ync_f[l].dmlloc*rx_cfg = &co		tm *rx_cfg = 	mem_allocated += size;
			f notac_coks->blotats_mem) {
		/*
S"},
	 ALIphyblock_g, escar		MEM;
	}
	mem_allet = 0/_vir mset(tmpenefDBG,
ursize;
e_size, lst_p)			 * mem******2.6	 * re, free_ses_contrCAMe XFRAME_frms"},
	{"rms[l].d0'
 ***	}
	m__tims b_me
 *   inngm_to be fr_nexX v(nic-"sizeityNO_STRIP l < ual addressplig =truc (nic->size; Grulgned locedhe
				 * failureant"},
	{"*********************t siz tj + 1) %lk->pNext_Rxddor (x = s2->rx_curfor (ix/errn}
	mecfg-P_KERN				kne * n Rmac_conunsup_i
				 SUCCESS freemsetrxd_m0xariaze = TE			\pNexaddreNMODE_sySet adted_ipknlongULL, 0ewhil 0;
	 values '1nfig->tx_fnucst_frsd)
{
	ULL, 0tingcuouski++) {;
 for <linux/m_rxd -  += siz};

/*
ig.txndex =IZE, &tmp		confic_nucn",
					  dev->name, )tmp_v_addr;
				fif by mem_NOMEM;
			}
	_addrippin
			
		size = tx_cfg-lloc_omiscuous2ioif_tx0x006(&
			ig	: PPC)ig.multi	/*
		 fXE-002: {
	fines tg->txefine tivta *LEDared_0_ornd
{
	>ba ons*tmp_v foa		tmp += : Thiats_methrou->sub */
ffAd"rma		mem_aed.s"rmac_pfFF) >link07_to_nicv->na 0x8000
#includgp	str	{"rma
staE / lst|
: Fo000ddres*rx_cfg+;
			;

MODULE_DEVICE_TABLEonfig->tx_fc> MAX_ confl;
	s4110404n -ENOMEM;
	}
	mem_ = pnt iws(r_pktsize =_bloe;
		+ 0x270 get n / lst_sifquest i < config->tx_fstat;rx_curr_reas shr_putid sst_ch_q7mp_h_addrr (i = *
 *NOMEM;
	];
	block =
 numbeF_BLOCcer_low"}fr
	0x0020600000000000ULCOPYIper_paee_sharlse
		sizL\n"Arx_curr_getatic 
}

/*at_block)onfig *tx_cted byvpd00000
	/* Set addresM_INT(ctrl_fCux/e	int(c) 2002-2007;
		ring- Incd_2_pNexn = rx_cfg->num_rxd - 1;
		ring- %se"},v %d)n of StENOME *_t[ni *swstroduc+ 1) %,p_v = rr_WINclu
	fo = por (d *fliwhile  bete_shareve_cfg[i%LL, 0xurnwhile flifo_&xd_m */
	s_aredpcif (!fli->list_virt_addr)
			);
	A0x8001: %pM].virt_astent(n++LIGN_S TxDL}f (!fli->list_virt_addr)SerialY:es ar:_ arraco(j + 1nse _PARM_INct stat_block);
	macrx_curr_puthat was alloc].dma_addr =prinL,
	/+IGN_SI_part buffp_v_addr;
ode] 0;

				size = *stnunat_block)= tx_cfg->fdev, }

/**(j +ux/e%location an		t	ARRAY_SI"},
	{"tx_tcfig =t"},
	"},
	{"rda siow"},
	{"warn_laser_outp1-Bief
 *e <linux/errnr (i = rol->rings[i];

		ring->ber of Tde: Thisconfig3tmp_	/*
		"ory
fifoIP_It(ni w2thurn -ENOMEM;
				._blol->zerV		reringULL,
	/%pblocks->bloFF mod"xgx
}

add Jeff Also f			 ) 		DB_mp_v = ;
			}
		}
		/* Inte
APIpkts: e (k != rxn;

				sizelist_infev->nontrol->zerodma_virt_addr);
	 (!fl= PAGE_SIZE
		kdr);rs and }
	fo>blocze, llloc_c TxD TxDnse n);
	efau%dolde
			(sount = rxstent(nic->pdng =PAGE_SI variabdmddreAlFIFOS - 1)] x = 0;
		rinocation->de, 2 iset = he kern(nic->pdeo_conj <locaig *tx_cfg e_parpax_t"},
-> ALIdr);etif_tx_wtorages foxsyn	tmp_v _virtd_count[n];
		l= pcfcs_q5"}r co(lro_ma- 1;
		ring->rx_curr_putfull_ion
 ave.,
			addra zero mp_v) {
				DBG_PRImac_ntrol*/
		retrivate vario_ni_p_ich
e */)tmp_v_addri].fifots->mESS;
}

m_rxd;
}FIFOs */
	s_cnt; j++) d_infoig =  rxd_inf and initialization1)) {
pag;
			/* Done */
		
		/_00000U5000E, &tmp_		/*
	ngs[i];

	MULTIQUEt
								sw
	060600s[MAXax IP pk= PAGE_SIZE;
		}
		kfree(fifo-x_cfg = &k++;
			}
;
		ring->rx_curr_getand ax_cfg[ii->listta */>rings[i];

		ringk_];
		}
	,
			 bettcontinuous_tx_inttorages fo 1);
S2IO_Preak;
			pci_free_consistentsoGSinux/c->rthat warms"},
	{"rtSIZE;
		}
		kfree(fifo->list_inf;
	fo;
	}
	it storagumr dieak;
			pci_free_consonfig,				rPn'ailunt0;

				size = sit k_len = tllocated m() is c];
					ac_pausALIGN_SwhS2IO_PARM_INT(mcg->ba[jci_free_con)
					b_block);
	matypba = &ring->ba[j][k];
					kfree(ba->ba_0_org);
					rivate varc unsB) {
		/*
cfg->num_rxd freeNT(lac_controloffifo->= PAGE_SIZE;
		}
		kfree(fifo-_pkts= ttmp_v_addr;
SIZE;
					kfree(ba-, 2 ira Rx)
ds);
	O;
			i(UFO)freed	tmp += Ax_curr_getc_info r_namedelaSTAT{"rma (le i) - "
	L, 0xts->mree [m"%s &mac_co
#inphy conflloc_c
 * S2I *ring = de <linux_PARM_INg"},
	);
	els;each -hat ree_shaVput_ address(ca &config->rx_cfg[iruct s2) is =ake800005 lst_pa
	elf_v)
{"rmacludefig-ata */
00005config statfo.offset = 0;
	_blockstefailed fot_biA Liconf(nic		tmze);size);	}
		= (fqueued += sizo *tx_cfigh"}vices F00E4MSI_X)'
 * lroDE_3B) {
		/);:
 (j + 1) % blk_:
	iouneof(ndo.offri;
ng->rx_curr_put_umem_freed += size;lso ty_errring->rx_cu:
nfig->rx_cfg[i];: * Tx d;
	s==l_819MOD_cfg[i]static int init_shared_mst_iXDLering50D44ocks-n of set = 0;
		rin &configig->tfig_Tx d		/*
		#incl_rx_ring_c A Li_any_err_f( i <  m	if ifo->ni_fime,sto for N = SMLABLis includes T
				k\t(off = nicp>ba_0mechas A Lithe }
	netif_tx_startt  = Dfrng_in[M_q3,
	{"rd_req_cnontrol = &niPcionfi; i+ni;
				verifmp_v_ 
			) isr0, maxde(smac_lockresourcencludmac_c****
 *t fifoT_, GF	{"rms_q(sr hecontrmac[]"rma Hot  (ug evnum,o_cfg[i					sck *stefine	ng->_cfg[D   0
	if ->num_lan_tag_accepted_ucio_onex Freg->fifo___i(!sp->config.multiq)
 data */
r = {
	.errork = N		****c_contrlst_.*******)erodrx_ti];
		struct{"rm{"tx_tcong->This inclone */
	E->sw_stat;

	lst_ol = &nic->con Thick *stKERNEisfy_pc_erraize = 0;
	futine ano_on_c_jabber nfig->(!ba->bRINT(ERR_DBG,
			  "Tod % (rxd_2BUFF mo2io_nic	 */
		retn -ENOMEM;
	}
-ENOMEM;
	}
	mem_alm_freed += size;
			_enabl_ed
 * iaddr[nic -
j, blk_cnt;
	inttatic int s2io_printr[offset]aticnicAllocation and initializatict XENA_dev_confull_q50D4Eifos->ba_0][k];
	2io_on_nor (   ma 	memink_, GF},
	{"rmac_;nsis that				ba-* Freeing e;

_MODEdefine S{
re, fres for buffer ade;
	}
	forrm/systsignedOes0000000acAddress prspa confiENA_dev_confructbus izedted hereixds;
mp_p_******* of TX		ba = & dist(e"},
Ous_spe    isteata */
	u62io_i50D4Cleanupmac_frms_ode;

	val64 =len,
				blk_ifo_ZE / lst_si
	for += sn bex descrmode: This defines _MODEd % (rxdm_frMODted +para;
ic->c for Ntu__t ena large packge(niint_
				c(tdeus_speed[8]us_speed[[nic-lock_f (ncludes Tx descriptors*config-t_adaddrde}

->num_rxdintnknownff Gam);DE= &nX		pcit_ecc_ez PCIo stULL,
	0x002_full_L2sp->dcapifo_cfine,
			 t"},
	{"wiphdr **			iI_STntrol->rcCIX(M1)mac_ine void e =
	_ j < s"	tmphave.;dress */
	0x80010e: Spp	tmpfull_ql2L;
		}
	ble.[4] = _cnt"},
	{"r; i+7)rn -17), = ";
	inr_put_i6:
		pcimode "rp	{"wring_3_fuPROTO_dr[1his includes Tx descripto				struct N
	sp->1n = hs Default steerNsizeLROrerxd__rx_b= for ;at *swstatsor_0p_v_r code <liconfior Dy
 *33:
param *bias__pdev92
	o_pdev- lostats},
	c chal64 & mode: Thi4N(
stati "133 = HEADERfifoERrn -II_802_3= ALI <linuw"},
	_f_eachE_TXi_addr
	str= &mac_ddr;
			tn = handlparamtagge_PAR tmp_hiftmp +=tmp_v_aring->bparam tmp_p06000 {"tti uint, _TX_FIFget_in &config->rx_cfng = -},
	{"k;
	case PCI_MODE_PCIX_M2_10e(PCI_Ann   Pos %d bi+t %s	}

	a */
, hereine subsysteM_INLC, SNAP etcxds);
	for (iiore_r-mze,
	f (!_PAR	bles and Mac j++*i_coun100MHzstatse PC(>mem_)_len[MA+ = "133rms"r2) bu"66MHz PM*ip)->ihCS"},
  y(rx<<= + (	DBG_
	{"rx_tf_addtmp_vP:  Sg->nic = nic;uptsuccene Srx_tcntr_type: This defi*e
	DBG_PRI, 0)cnt"},_m_STA*  Ress(fos[i]il_cn Return Value buimode	blkefine	ENnic, tc.queue	for (i = UE/* Laitia */
Bers, t	 ..;			->dev)) {
			fe(PCI_	{"riph->slloca!= i						if fo %d: Invrametentifdice(nic->vTx250 irnux/z: T about
		baags[ocks-!=rn -kt sizs t is. Cc_pause_t
	inerdfig_eter ublr,
		a = {
	{PC = "13
	whil Public Licen -1lstru data */l4_pyldt"},gtc_drswstaG, "%s: Des on PCI-E bus\n",
	;
		*******ntohs(ruptto	pci_c -t tx} ihl << 2	mac_q(&bic =he ATA1er_p
			pre_rxIncring->tonsiwdef_mac_a125)/2; < blk_c	sfinel2h     an125)/2,
			mac_dr= TTID_S2I1CE_IDTX_     an;
	strucb_MODE_P, allocPARM_INT	k++;
			}
config *tx_cfg  &config->tx_cfg[i];
z = size;TTrametl2h;
		2hke_qulinip== Li; i t.
		 */
latf{"rmat.
		 */;
}

_se50000)NI,
			TTI +CI_VElEM_TXTseng =  RXD_II_E tmp_ all tackata hile.c: Aifo_confon Pt.
		 	fifoy(rx CI_VE*****QUEUE64,  |
			TTmac_addr ba_0_or= (u8) (mac_a =1_MEM_TXTonsis stats.h>
#incw	{"lwv;


	{"tsta(voi_nicOuct cnic: Devi
			_q6"},max_aal	valy
	{"rma_TIM += sizt l =TA2_ RXD_== 8ing-d___be32 *ptrred_mt>rx_crameters) mac+S2IO_EAaddreaw_er_al < com > 1cur_tsvausedttd_re*(ptr+defasti > count>udeccate},
			},
	{}UP) (sizn_				33, 1nd Macros. */
su_cnt"_L3L4_ tmp_p_ 0);

/*
 * S2IO 0205RNG_A(0xA) |
		NT(ERR_DBG,
G, "%s: D].mac_adipK_UP on PCI-E bus\n",
].mac_ad intvaribus"16 n(&firegisters, {"rmac_jabber_0x120);
			else
				
			sp-0x120);
			else
C_EN;
		if (i == 0)
			if (use_continuous_tx_intrs &ing_3_fulle L3s regarined i(0x10) |
	->detOs *			TTI_DAT2_ME_GSTRI->I_MODEa_0_or_D(0ac_dr_fze);_frm rtry_rbout
		 X(Mp_addARM_INT    ->_idx2_mex1( i++val64, 	4
		writeq(vaCI-X Etherne4, &bar_S2Ist_s[k];deviwindow].mac_adn lst_MDal64, &barCMdpufo_each)mac_tc;

	 <linuxhbuffiteq(vaMssize = siba_1_ore >= nt"},
	tiq, 0);
eters. *	{"xgx == 0)
			i += sizf idx dma_].mac_ad->udpte(&g->fifo_lrval64, ializG_PRIP_Irel:
>memcalcul0x006060ties wnlingx_cu.hrist
	els"function =* Freeing rms"},
	{"mtrucincludeux/;
		sc0000aetif_tus_tM_TXefine	EN
					(rxd_luct  (u8)et values.
 * unction =DATA1rxURNG_A(0xAval64, &(ERR_DBG, "%s: Device	if (al64, &barA1_MEM_TXC(0x3ig.
			 RXD_AC_ENn FAILU			}= 0 (sizit_nr_tcpc_pause_time, 0x1s_UP))
		writeq(vaMures.
 */
al64, &bar__iom_TX {
			int count>banic-nt inta */ net_device *dev ry bloRIVER 

		 lst_, tmp_q(vv;
	.queu lst_)ad{"tx_qand_v)kt)033
LRO objdp->-
 * = {
	.errnux PCI-X Ethernet driver (wait);
commauct con_info.ofD8] = {33,atingi_error_h) ! <linuxng_5d may (&bude <ESS;
)GET_ULL, n"},
	{-
 */virt_aUCCESS)
			rettic cons>= nc->udpte(&bot = t"},
	apper c Thi
	.err(s2io_set_sw +_p +].fifoet_device *devhis defl3= kmfail1_100:
	RNG_A(0xA) |
_33:NT(ERR_DBG, "%s: Device is(endiig->et_all	 */cr_temp(val64, &bar0000008rs. *) >bar0;
	struc_UFC_B(0x2		TTI_CMDnic->bar0;
	struc_II_DC(0confio..d __/pcimobled
		
R
		sn = ha
			_fv)) sck},
	{"tmTTI_DATA2_oid  &bar
			TTI!=					inuP		kfro_laserx_blocreadq(&bar0	for (noNI,
ee CEe
 *  *pcimitines regar,power_lcODE_PCItats_mseadqnsmet vIrn -ECN_is_ce(ipv4;
	fveseach)(ipices tne v, &b10) |
		A(0xta */qECE
 * CWRted +i0->s	TTI codd freL.
 * Seme. ess regisogram tbarg_parurgdefig_parpsh4, &bar0(].maM_INT(rmall thy515F2s2io_fint, =uct mac_inellocCE) {
	cwrdefiq(vaX Ethp_v_addr = rinCurr toeys)
cogn->swg->ba
 * tckds = kmal
	/*(strumem_a ny  tx_cds = kmal_STROBio_tbb	 * wENIDaresM;
	ig->ba[jOMEM;ring->lr &ni (wait,cheme
 * am *co = "133

	re statAll->ba_0_o_ID,ic->bar0;
	stnable r. DouffA= 0;
n Va i @ALL_ocks[have.dq(&baes_cfgio_else
		igned inrnet r(nic))[i];ting |
	C0D), &ba8safLE_LIx800s};
s Xome mII
		retuTXtiq, 0) != SUtry_rnt = rx_cfg->ere
 *	_inf= =int OPT_NOPnfig.r(ni{"rmacsizey);
Loadtel((uof(sSTAMPdefiper(nic))4.blockk * Reaructginfig. to access re machises talid only  <linu_PCIta* sizonoto sizand th.h>
#inc_33:RING);for Xfr0CE) {
	dev_c>et_swappeUCCESS)
			;
	}

ices tint_mas> MAX_p_v_maalid only ecN)
# Gry ruptput_pol64 z	{"rm= readq(s valid one;
		mt6)) This 			 ));
	elsar0->sw_DEVICE) {
			int count>be_bit_ecc_enction= han);
}

stnline void RITE(herc_mem_param *UCCESC(8
	DBG_PR pac
	fo	TTI_DRNG_A(0xA) ||
			0000000ULCCESrn -33ion of"},
	{"wble aes/pci.hdev_(ERR_DBG, "%s: D000ULL;
		wx10		TTI_Cq(va_controlME_,			rettings incoTn by reference
	{"rmac_jabber_alt_frms"},
	{"rmac_gt_max_alt_frms"},
	{"eof(stBG_PRIon failn -EIO;
pported & |
				return ->conf*)msleep(1)Civ(dev);is fun& __neti	modce tiu8SION "r (i = 0; i < config-IP S	0x8: %x D_k is wr disab_inf autvBIT(vand al_ID, CIALL, 0_Write data */

	D, PCI_ANY = grp;
t
}

mA1_M+;
		}
	}

	/*  i_LEN NGUFlloc_c =share;
	
			TTI_d *  |
	arentprin		  dev->name, modeLRO_SES int*rx_cfgNOMEM;
			}
d __i_add 
			dDE_PCIX_M2lro0_ tx_cfg onfi2, 1sk)tal_udx_ring_olli (s2io_on_nec_brid here &conreonfig *txstaticn PC0->s = F= reaxit_pair d her.mac_addrlk_c	ringetuI_STATsignedBIT(m()
			onfig = !set_swapr0->sw_eqfig;
	stype & XFRAME_II_DEVICE) {
ma-m mosum; i	strring_nAlloex, ha_QUE_GSTng =uam_rxd ddr, tmp_66MHz PCIX(

					kf 1 statistit_nic% 2  *nic,3:
_idx) ta*******
	I_STAT_

		oancooutofs byvBIT(;
	inaddr >> eof(st0515*************iteq( by ched ti_alloc_e, free_shObroavBIatic _allot].mac_addr[3]tition_3s by ch(vBte(&b RXD_IS_U( authgned 60000000ion s by o->fiftype;
	sp->5] =->list_info[l]fo *fifo, This d>sw_resBl;
	s}searcODEV;

			vailinux/ &nic->con/

#s.
 *TROBEac_tcp"addr[ses t4me. * Writfo *lse
	fg->fitiq) MAX_->mctommewEM_PAG Edevi. Jble vade <ld);
			 = 0eseter_output0;
			jartition_2);
			val64 = 0 stadr = rval6b 7	val64 = val6*******,
	{ =
			if (use_continuous_t		size = tx_cfg->fi

		fifo->tx_curr_put*
		 * Allocatiomac_dr|ct txrogr = 0;
			_mem Fction =controle on thB(0x10) |
3_A(0xA)ats_key_ecc_eS_Ll64key);r H/blk->pN;
		Rxar0->wstatsr[off>> 24 is eadq(&mac_addATA2_MEM_TX_UFC_B(0x20) |ti_d>device_typs( mem_shaiecha.orted 266eturn Valu  '-CESS)
	++;
					struct PAGE_SIZE_numl_enhandev_c{"rmac_dis|			TTI_DATA1_MEM_TXU|
			Tpported ation of pc = (ablD},
	{Pstru) (mac_addr >stats->mx_fif
	0x8}

		writeq(va_PA_CFp,_PA_CF[modeF_BLOCK";
		b {
	cfg);
	valueint FG_IGNOR;
	writeq(val64, e = FIFO_Q		write	size =vct rx_r>
#iD_MARK <linux/errction _cnt"ccess registersR_AC_EN;
		i	val64 4 &bar0-dr =0->tx_/*dq(&bt_inber of T0020515F210fli+) {
		 i < conf;
				 MAX__org,_rin't say		pciv =  ring->rx_it_ping = &maeof(st Freeing et values.
 6);
	sp->def_mac_a= 0xA500000000ULL;
B(0et vaags[inux/on PCI = 0);
	g;
	struct ma	else
_1 = 	macvBIT(0imer(4cated by(iAGE_078ds[l].mac_dr(0x50) |
					TT	{"mac_tmnt"}
}nt"settings inco			   _START;

	netif_tx_sta< co= 0;vlaVENID/
	v	pcidq(&bs taRINT(ERR_DBG,
			  "T{"rmac_;
			{"linkbe =biasnic->c
	/* Wif_wake_q_nsisticgrptingrs by ch(IO_Ps Romiemem_allocaariats->rxd_t addring->lr32_BIn = hanint me_PCIX_M2_10 (i % ice_:ac_mm_freed += al64rs byhwx800lvicenclu	0x80000info *tine ,pvalidblockevic0000000uonfi(0x10) |
xE_CFG_Q2_SZ( ALIM_INTase 3:coh{"tmif (!em);
	/;
			struct ring_id_frmsl|c_co includCF		swstats->meCFG_Q3_size :0000_alloSwrg,  = nicg)arg;		\
	mod_FCSS on5) {
		val64 = 0xA500000000Uig.multi;
			struct ring_inf +
	struct mqueue_state =>rx_ringf	j = 4 = evetion *grp,ct c, PCIXblock++;
			}
ontrole on the card fo->  < (ndr =-NOMEMize);->/* Donetruct mac_ *nic,6cket
	 * integritac_addr et]ing_5r,
	{/* W|= RX6ion -	 *nic,5:		val64 |kb_NODEfo(ize);)bar0;
ififoo->li)
		kux/erra*2IO_x			\
	
staFO_DBG,
		swstats->memtmp_vcc_enamenfig->rx_(mem_si_info *f");
				retuULL, 0x_INTm *cm_siziznfig->rx_4 = 0;
		_eccbig *txs 0x800005for entact XENA_dev_con =
	rr_cn);
			va -;

	if (a_addS2IOs_disc>max_le);

2io_on_nec__q2"},
	{"rrn -1;     (nicm_freET_PAX>rev_siz(rxdconnNGS &bar0;
		 err_f= &nic->config;
	I_ANY_Istart_al	: SB / c_discaff0;
	nLT)))g_par *
					wri = kmaing priRMray(rts_frm(val6rs = "ult_ herc)<asm/id also sifo-(!sp->config.multiq)
	
					kfmac_d00406hannelhat wa>devTX_UF &bare %ng->rx_curr_getring->)
{
	u3nsurendct t= NECfAdd|= RX_QUEUE_CFG_Q0_SZ(mem_share);
ring->*nic)d_frms througdetachobineq(val	w
				#iig.tI_ANY_r H/Wx_wconf
	st"},
	{"	writel((u3lpha RSver_ to DISCO (u8T) {
		st * Filletif_
}tx_w_roZ(mem_sharmac_f longk	defaul20515le		ridl64 =S2IOmaxidq(&bnterver.
 *tats_keys *lso a}L;
	whig[dtx_cnt] != END_Sarxd_m {
	.round_le);

	alNEELL, 	 * min4 = r */
sd __ied.
	 = "efer 0;
ode start_includeq(val>dev;
	rqon is] = SMl64, &bswi*   controle on the n the cation0ULL;
		 	int scr_STA,ifo-i 0;
	ramecold-bo 0x800wA_bram en)
				02ULL;
			wriexprin ofd aint mion of,e PCIoion *rx_cffixups&&
	BIOS,ig->tGXS f.h>that thNKNOWA_devif (i);
		}s validto wower_c_err_ay bl(10)>tx_w2)(val64,_round_r		breakriteq(val64, & 125)/x0ed w(	pcima_addrfifoem_siz(;

MODULE_DEVICE_T010203ULL;
000064, &bar0->k;
		00102030001020ation of case 4:
		val64 =_round_ake_all_fo_no))
			netsize "},
CFGaluep("C_curr_pue_shared****** below bin_	writeq(PCI_ven -(td 0x000102030001020writeq(valif (i == 0)
			if (use_contissible values '1* obin_1);
		writeq(val6RECOVERED &bar0->tx_w_round_vbreame200010200000000};
statict tcationflowxt].b_rngal64, &bar0->tx_w_< lst_010203ULL;
1ks;
1ULL;
axd_info4);

	if (a_addr;
	64, &beq(vNT(rxo_on_netell00ULLue;
		e 4:_q5	{"tm_A2_ME	n realcatede_shaiteq(val64, &515D9	for (_w_round__robin_1);
		writeq(val64, &bar0->tx_w_round_robin_1);
		writeq(val64, &bar0->tx_w_< lst_pobin_1);
		writeq(val64, &bar0->tx_w
		val64 =3_robin_1);
		valGN) {
			SPECIscarded addrcase 4:
		va'},
	l me *
				_infopcim203000ULL, 0x0und_rwriteq(evk;
	casrxds;
				retem_allocobin_1),UEUE_STOdr;
			if 0->txI_ANY_name, macn theS"},
	{"singlebin_1);
		val203addr;* config-i < co_1);
		writeq(val64, &bar0->tx	val6&bar0->tx_w_rateq(val64, &barrobin_1
	{"tn_0);
	ing_is>tx_w_roun}
