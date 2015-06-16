/* Copyright 2008-2009 Broadcom Corporation
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available
 * at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 * Written by Yaniv Rosner
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mutex.h>

#include "bnx2x.h"

/********************************************************/
#define ETH_HLEN			14
#define ETH_OVREHEAD		(ETH_HLEN + 8)/* 8 for CRC + VLAN*/
#define ETH_MIN_PACKET_SIZE		60
#define ETH_MAX_PACKET_SIZE		1500
#define ETH_MAX_JUMBO_PACKET_SIZE	9600
#define MDIO_ACCESS_TIMEOUT		1000
#define BMAC_CONTROL_RX_ENABLE	2

/***********************************************************/
/*			Shortcut definitions		   */
/***********************************************************/

#define NIG_LATCH_BC_ENABLE_MI_INT 0

#define NIG_STATUS_EMAC0_MI_INT \
		NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_EMAC0_MISC_MI_INT
#define NIG_STATUS_XGXS0_LINK10G \
		NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK10G
#define NIG_STATUS_XGXS0_LINK_STATUS \
		NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK_STATUS
#define NIG_STATUS_XGXS0_LINK_STATUS_SIZE \
		NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK_STATUS_SIZE
#define NIG_STATUS_SERDES0_LINK_STATUS \
		NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_SERDES0_LINK_STATUS
#define NIG_MASK_MI_INT \
		NIG_MASK_INTERRUPT_PORT0_REG_MASK_EMAC0_MISC_MI_INT
#define NIG_MASK_XGXS0_LINK10G \
		NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK10G
#define NIG_MASK_XGXS0_LINK_STATUS \
		NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK_STATUS
#define NIG_MASK_SERDES0_LINK_STATUS \
		NIG_MASK_INTERRUPT_PORT0_REG_MASK_SERDES0_LINK_STATUS

#define MDIO_AN_CL73_OR_37_COMPLETE \
		(MDIO_GP_STATUS_TOP_AN_STATUS1_CL73_AUTONEG_COMPLETE | \
		 MDIO_GP_STATUS_TOP_AN_STATUS1_CL37_AUTONEG_COMPLETE)

#define XGXS_RESET_BITS \
	(MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_RSTB_HW |   \
	 MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_IDDQ |      \
	 MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_PWRDWN |    \
	 MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_PWRDWN_SD | \
	 MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_TXD_FIFO_RSTB)

#define SERDES_RESET_BITS \
	(MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_RSTB_HW | \
	 MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_IDDQ |    \
	 MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_PWRDWN |  \
	 MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_PWRDWN_SD)

#define AUTONEG_CL37		SHARED_HW_CFG_AN_ENABLE_CL37
#define AUTONEG_CL73		SHARED_HW_CFG_AN_ENABLE_CL73
#define AUTONEG_BAM 		SHARED_HW_CFG_AN_ENABLE_BAM
#define AUTONEG_PARALLEL \
				SHARED_HW_CFG_AN_ENABLE_PARALLEL_DETECTION
#define AUTONEG_SGMII_FIBER_AUTODET \
				SHARED_HW_CFG_AN_EN_SGMII_FIBER_AUTO_DETECT
#define AUTONEG_REMOTE_PHY	SHARED_HW_CFG_AN_ENABLE_REMOTE_PHY

#define GP_STATUS_PAUSE_RSOLUTION_TXSIDE \
			MDIO_GP_STATUS_TOP_AN_STATUS1_PAUSE_RSOLUTION_TXSIDE
#define GP_STATUS_PAUSE_RSOLUTION_RXSIDE \
			MDIO_GP_STATUS_TOP_AN_STATUS1_PAUSE_RSOLUTION_RXSIDE
#define GP_STATUS_SPEED_MASK \
			MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_MASK
#define GP_STATUS_10M	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10M
#define GP_STATUS_100M	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_100M
#define GP_STATUS_1G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_1G
#define GP_STATUS_2_5G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_2_5G
#define GP_STATUS_5G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_5G
#define GP_STATUS_6G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_6G
#define GP_STATUS_10G_HIG \
			MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_HIG
#define GP_STATUS_10G_CX4 \
			MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_CX4
#define GP_STATUS_12G_HIG \
			MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12G_HIG
#define GP_STATUS_12_5G MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12_5G
#define GP_STATUS_13G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_13G
#define GP_STATUS_15G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_15G
#define GP_STATUS_16G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_16G
#define GP_STATUS_1G_KX MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_1G_KX
#define GP_STATUS_10G_KX4 \
			MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_KX4

#define LINK_10THD			LINK_STATUS_SPEED_AND_DUPLEX_10THD
#define LINK_10TFD			LINK_STATUS_SPEED_AND_DUPLEX_10TFD
#define LINK_100TXHD		LINK_STATUS_SPEED_AND_DUPLEX_100TXHD
#define LINK_100T4			LINK_STATUS_SPEED_AND_DUPLEX_100T4
#define LINK_100TXFD		LINK_STATUS_SPEED_AND_DUPLEX_100TXFD
#define LINK_1000THD		LINK_STATUS_SPEED_AND_DUPLEX_1000THD
#define LINK_1000TFD		LINK_STATUS_SPEED_AND_DUPLEX_1000TFD
#define LINK_1000XFD		LINK_STATUS_SPEED_AND_DUPLEX_1000XFD
#define LINK_2500THD		LINK_STATUS_SPEED_AND_DUPLEX_2500THD
#define LINK_2500TFD		LINK_STATUS_SPEED_AND_DUPLEX_2500TFD
#define LINK_2500XFD		LINK_STATUS_SPEED_AND_DUPLEX_2500XFD
#define LINK_10GTFD			LINK_STATUS_SPEED_AND_DUPLEX_10GTFD
#define LINK_10GXFD			LINK_STATUS_SPEED_AND_DUPLEX_10GXFD
#define LINK_12GTFD			LINK_STATUS_SPEED_AND_DUPLEX_12GTFD
#define LINK_12GXFD			LINK_STATUS_SPEED_AND_DUPLEX_12GXFD
#define LINK_12_5GTFD		LINK_STATUS_SPEED_AND_DUPLEX_12_5GTFD
#define LINK_12_5GXFD		LINK_STATUS_SPEED_AND_DUPLEX_12_5GXFD
#define LINK_13GTFD			LINK_STATUS_SPEED_AND_DUPLEX_13GTFD
#define LINK_13GXFD			LINK_STATUS_SPEED_AND_DUPLEX_13GXFD
#define LINK_15GTFD			LINK_STATUS_SPEED_AND_DUPLEX_15GTFD
#define LINK_15GXFD			LINK_STATUS_SPEED_AND_DUPLEX_15GXFD
#define LINK_16GTFD			LINK_STATUS_SPEED_AND_DUPLEX_16GTFD
#define LINK_16GXFD			LINK_STATUS_SPEED_AND_DUPLEX_16GXFD

#define PHY_XGXS_FLAG			0x1
#define PHY_SGMII_FLAG			0x2
#define PHY_SERDES_FLAG			0x4

/* */
#define SFP_EEPROM_CON_TYPE_ADDR		0x2
	#define SFP_EEPROM_CON_TYPE_VAL_LC 		0x7
	#define SFP_EEPROM_CON_TYPE_VAL_COPPER	0x21


#define SFP_EEPROM_COMP_CODE_ADDR		0x3
	#define SFP_EEPROM_COMP_CODE_SR_MASK	(1<<4)
	#define SFP_EEPROM_COMP_CODE_LR_MASK	(1<<5)
	#define SFP_EEPROM_COMP_CODE_LRM_MASK	(1<<6)

#define SFP_EEPROM_FC_TX_TECH_ADDR		0x8
	#define SFP_EEPROM_FC_TX_TECH_BITMASK_COPPER_PASSIVE 0x4
	#define SFP_EEPROM_FC_TX_TECH_BITMASK_COPPER_ACTIVE	 0x8

#define SFP_EEPROM_OPTIONS_ADDR 		0x40
	#define SFP_EEPROM_OPTIONS_LINEAR_RX_OUT_MASK 0x1
#define SFP_EEPROM_OPTIONS_SIZE 		2

#define EDC_MODE_LINEAR	 			0x0022
#define EDC_MODE_LIMITING	 			0x0044
#define EDC_MODE_PASSIVE_DAC 			0x0055



/**********************************************************/
/*                     INTERFACE                          */
/**********************************************************/
#define CL45_WR_OVER_CL22(_bp, _port, _phy_addr, _bank, _addr, _val) \
	bnx2x_cl45_write(_bp, _port, 0, _phy_addr, \
		DEFAULT_PHY_DEV_ADDR, \
		(_bank + (_addr & 0xf)), \
		_val)

#define CL45_RD_OVER_CL22(_bp, _port, _phy_addr, _bank, _addr, _val) \
	bnx2x_cl45_read(_bp, _port, 0, _phy_addr, \
		DEFAULT_PHY_DEV_ADDR, \
		(_bank + (_addr & 0xf)), \
		_val)

static void bnx2x_set_serdes_access(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u32 emac_base = (params->port) ? GRCBASE_EMAC1 : GRCBASE_EMAC0;

	/* Set Clause 22 */
	REG_WR(bp, NIG_REG_SERDES0_CTRL_MD_ST + params->port*0x10, 1);
	REG_WR(bp, emac_base + EMAC_REG_EMAC_MDIO_COMM, 0x245f8000);
	udelay(500);
	REG_WR(bp, emac_base + EMAC_REG_EMAC_MDIO_COMM, 0x245d000f);
	udelay(500);
	 /* Set Clause 45 */
	REG_WR(bp, NIG_REG_SERDES0_CTRL_MD_ST + params->port*0x10, 0);
}
static void bnx2x_set_phy_mdio(struct link_params *params, u8 phy_flags)
{
	struct bnx2x *bp = params->bp;

	if (phy_flags & PHY_XGXS_FLAG) {
		REG_WR(bp, NIG_REG_XGXS0_CTRL_MD_ST +
			   params->port*0x18, 0);
		REG_WR(bp, NIG_REG_XGXS0_CTRL_MD_DEVAD + params->port*0x18,
			   DEFAULT_PHY_DEV_ADDR);
	} else {
		bnx2x_set_serdes_access(params);

		REG_WR(bp, NIG_REG_SERDES0_CTRL_MD_DEVAD +
			   params->port*0x10,
			   DEFAULT_PHY_DEV_ADDR);
	}
}

static u32 bnx2x_bits_en(struct bnx2x *bp, u32 reg, u32 bits)
{
	u32 val = REG_RD(bp, reg);

	val |= bits;
	REG_WR(bp, reg, val);
	return val;
}

static u32 bnx2x_bits_dis(struct bnx2x *bp, u32 reg, u32 bits)
{
	u32 val = REG_RD(bp, reg);

	val &= ~bits;
	REG_WR(bp, reg, val);
	return val;
}

static void bnx2x_emac_init(struct link_params *params,
			   struct link_vars *vars)
{
	/* reset and unreset the emac core */
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u32 emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
	u32 val;
	u16 timeout;

	REG_WR(bp, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_CLEAR,
		   (MISC_REGISTERS_RESET_REG_2_RST_EMAC0_HARD_CORE << port));
	udelay(5);
	REG_WR(bp, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_SET,
		   (MISC_REGISTERS_RESET_REG_2_RST_EMAC0_HARD_CORE << port));

	/* init emac - use read-modify-write */
	/* self clear reset */
	val = REG_RD(bp, emac_base + EMAC_REG_EMAC_MODE);
	EMAC_WR(bp, EMAC_REG_EMAC_MODE, (val | EMAC_MODE_RESET));

	timeout = 200;
	do {
		val = REG_RD(bp, emac_base + EMAC_REG_EMAC_MODE);
		DP(NETIF_MSG_LINK, "EMAC reset reg is %u\n", val);
		if (!timeout) {
			DP(NETIF_MSG_LINK, "EMAC timeout!\n");
			return;
		}
		timeout--;
	} while (val & EMAC_MODE_RESET);

	/* Set mac address */
	val = ((params->mac_addr[0] << 8) |
		params->mac_addr[1]);
	EMAC_WR(bp, EMAC_REG_EMAC_MAC_MATCH, val);

	val = ((params->mac_addr[2] << 24) |
	       (params->mac_addr[3] << 16) |
	       (params->mac_addr[4] << 8) |
		params->mac_addr[5]);
	EMAC_WR(bp, EMAC_REG_EMAC_MAC_MATCH + 4, val);
}

static u8 bnx2x_emac_enable(struct link_params *params,
			  struct link_vars *vars, u8 lb)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u32 emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
	u32 val;

	DP(NETIF_MSG_LINK, "enabling EMAC\n");

	/* enable emac and not bmac */
	REG_WR(bp, NIG_REG_EGRESS_EMAC0_PORT + port*4, 1);

	/* for paladium */
	if (CHIP_REV_IS_EMUL(bp)) {
		/* Use lane 1 (of lanes 0-3) */
		REG_WR(bp, NIG_REG_XGXS_LANE_SEL_P0 + port*4, 1);
		REG_WR(bp, NIG_REG_XGXS_SERDES0_MODE_SEL +
			    port*4, 1);
	}
	/* for fpga */
	else

	if (CHIP_REV_IS_FPGA(bp)) {
		/* Use lane 1 (of lanes 0-3) */
		DP(NETIF_MSG_LINK, "bnx2x_emac_enable: Setting FPGA\n");

		REG_WR(bp, NIG_REG_XGXS_LANE_SEL_P0 + port*4, 1);
		REG_WR(bp, NIG_REG_XGXS_SERDES0_MODE_SEL + port*4,
			    0);
	} else
	/* ASIC */
	if (vars->phy_flags & PHY_XGXS_FLAG) {
		u32 ser_lane = ((params->lane_config &
			    PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK) >>
			    PORT_HW_CFG_LANE_SWAP_CFG_MASTER_SHIFT);

		DP(NETIF_MSG_LINK, "XGXS\n");
		/* select the master lanes (out of 0-3) */
		REG_WR(bp, NIG_REG_XGXS_LANE_SEL_P0 +
			   port*4, ser_lane);
		/* select XGXS */
		REG_WR(bp, NIG_REG_XGXS_SERDES0_MODE_SEL +
			   port*4, 1);

	} else { /* SerDes */
		DP(NETIF_MSG_LINK, "SerDes\n");
		/* select SerDes */
		REG_WR(bp, NIG_REG_XGXS_SERDES0_MODE_SEL +
			   port*4, 0);
	}

	bnx2x_bits_en(bp, emac_base + EMAC_REG_EMAC_RX_MODE,
		    EMAC_RX_MODE_RESET);
	bnx2x_bits_en(bp, emac_base + EMAC_REG_EMAC_TX_MODE,
		    EMAC_TX_MODE_RESET);

	if (CHIP_REV_IS_SLOW(bp)) {
		/* config GMII mode */
		val = REG_RD(bp, emac_base + EMAC_REG_EMAC_MODE);
		EMAC_WR(bp, EMAC_REG_EMAC_MODE,
			    (val | EMAC_MODE_PORT_GMII));
	} else { /* ASIC */
		/* pause enable/disable */
		bnx2x_bits_dis(bp, emac_base + EMAC_REG_EMAC_RX_MODE,
			       EMAC_RX_MODE_FLOW_EN);
		if (vars->flow_ctrl & BNX2X_FLOW_CTRL_RX)
			bnx2x_bits_en(bp, emac_base +
				    EMAC_REG_EMAC_RX_MODE,
				    EMAC_RX_MODE_FLOW_EN);

		bnx2x_bits_dis(bp,  emac_base + EMAC_REG_EMAC_TX_MODE,
			     (EMAC_TX_MODE_EXT_PAUSE_EN |
			      EMAC_TX_MODE_FLOW_EN));
		if (vars->flow_ctrl & BNX2X_FLOW_CTRL_TX)
			bnx2x_bits_en(bp, emac_base +
				    EMAC_REG_EMAC_TX_MODE,
				   (EMAC_TX_MODE_EXT_PAUSE_EN |
				    EMAC_TX_MODE_FLOW_EN));
	}

	/* KEEP_VLAN_TAG, promiscuous */
	val = REG_RD(bp, emac_base + EMAC_REG_EMAC_RX_MODE);
	val |= EMAC_RX_MODE_KEEP_VLAN_TAG | EMAC_RX_MODE_PROMISCUOUS;
	EMAC_WR(bp, EMAC_REG_EMAC_RX_MODE, val);

	/* Set Loopback */
	val = REG_RD(bp, emac_base + EMAC_REG_EMAC_MODE);
	if (lb)
		val |= 0x810;
	else
		val &= ~0x810;
	EMAC_WR(bp, EMAC_REG_EMAC_MODE, val);

	/* enable emac */
	REG_WR(bp, NIG_REG_NIG_EMAC0_EN + port*4, 1);

	/* enable emac for jumbo packets */
	EMAC_WR(bp, EMAC_REG_EMAC_RX_MTU_SIZE,
		(EMAC_RX_MTU_SIZE_JUMBO_ENA |
		 (ETH_MAX_JUMBO_PACKET_SIZE + ETH_OVREHEAD)));

	/* strip CRC */
	REG_WR(bp, NIG_REG_NIG_INGRESS_EMAC0_NO_CRC + port*4, 0x1);

	/* disable the NIG in/out to the bmac */
	REG_WR(bp, NIG_REG_BMAC0_IN_EN + port*4, 0x0);
	REG_WR(bp, NIG_REG_BMAC0_PAUSE_OUT_EN + port*4, 0x0);
	REG_WR(bp, NIG_REG_BMAC0_OUT_EN + port*4, 0x0);

	/* enable the NIG in/out to the emac */
	REG_WR(bp, NIG_REG_EMAC0_IN_EN + port*4, 0x1);
	val = 0;
	if (vars->flow_ctrl & BNX2X_FLOW_CTRL_TX)
		val = 1;

	REG_WR(bp, NIG_REG_EMAC0_PAUSE_OUT_EN + port*4, val);
	REG_WR(bp, NIG_REG_EGRESS_EMAC0_OUT_EN + port*4, 0x1);

	if (CHIP_REV_IS_EMUL(bp)) {
		/* take the BigMac out of reset */
		REG_WR(bp,
			   GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_SET,
			   (MISC_REGISTERS_RESET_REG_2_RST_BMAC0 << port));

		/* enable access for bmac registers */
		REG_WR(bp, NIG_REG_BMAC0_REGS_OUT_EN + port*4, 0x1);
	} else
		REG_WR(bp, NIG_REG_BMAC0_REGS_OUT_EN + port*4, 0x0);

	vars->mac_type = MAC_TYPE_EMAC;
	return 0;
}



static u8 bnx2x_bmac_enable(struct link_params *params, struct link_vars *vars,
			  u8 is_lb)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u32 bmac_addr = port ? NIG_REG_INGRESS_BMAC1_MEM :
			       NIG_REG_INGRESS_BMAC0_MEM;
	u32 wb_data[2];
	u32 val;

	DP(NETIF_MSG_LINK, "Enabling BigMAC\n");
	/* reset and unreset the BigMac */
	REG_WR(bp, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_CLEAR,
	       (MISC_REGISTERS_RESET_REG_2_RST_BMAC0 << port));
	msleep(1);

	REG_WR(bp, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_SET,
	       (MISC_REGISTERS_RESET_REG_2_RST_BMAC0 << port));

	/* enable access for bmac registers */
	REG_WR(bp, NIG_REG_BMAC0_REGS_OUT_EN + port*4, 0x1);

	/* XGXS control */
	wb_data[0] = 0x3c;
	wb_data[1] = 0;
	REG_WR_DMAE(bp, bmac_addr +
		      BIGMAC_REGISTER_BMAC_XGXS_CONTROL,
		      wb_data, 2);

	/* tx MAC SA */
	wb_data[0] = ((params->mac_addr[2] << 24) |
		       (params->mac_addr[3] << 16) |
		       (params->mac_addr[4] << 8) |
			params->mac_addr[5]);
	wb_data[1] = ((params->mac_addr[0] << 8) |
			params->mac_addr[1]);
	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_TX_SOURCE_ADDR,
		    wb_data, 2);

	/* tx control */
	val = 0xc0;
	if (vars->flow_ctrl & BNX2X_FLOW_CTRL_TX)
		val |= 0x800000;
	wb_data[0] = val;
	wb_data[1] = 0;
	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_TX_CONTROL,
			wb_data, 2);

	/* mac control */
	val = 0x3;
	if (is_lb) {
		val |= 0x4;
		DP(NETIF_MSG_LINK, "enable bmac loopback\n");
	}
	wb_data[0] = val;
	wb_data[1] = 0;
	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_BMAC_CONTROL,
		    wb_data, 2);

	/* set rx mtu */
	wb_data[0] = ETH_MAX_JUMBO_PACKET_SIZE + ETH_OVREHEAD;
	wb_data[1] = 0;
	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_RX_MAX_SIZE,
			wb_data, 2);

	/* rx control set to don't strip crc */
	val = 0x14;
	if (vars->flow_ctrl & BNX2X_FLOW_CTRL_RX)
		val |= 0x20;
	wb_data[0] = val;
	wb_data[1] = 0;
	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_RX_CONTROL,
			wb_data, 2);

	/* set tx mtu */
	wb_data[0] = ETH_MAX_JUMBO_PACKET_SIZE + ETH_OVREHEAD;
	wb_data[1] = 0;
	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_TX_MAX_SIZE,
			wb_data, 2);

	/* set cnt max size */
	wb_data[0] = ETH_MAX_JUMBO_PACKET_SIZE + ETH_OVREHEAD;
	wb_data[1] = 0;
	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_CNT_MAX_SIZE,
		    wb_data, 2);

	/* configure safc */
	wb_data[0] = 0x1000200;
	wb_data[1] = 0;
	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_RX_LLFC_MSG_FLDS,
		    wb_data, 2);
	/* fix for emulation */
	if (CHIP_REV_IS_EMUL(bp)) {
		wb_data[0] = 0xf000;
		wb_data[1] = 0;
		REG_WR_DMAE(bp,
			    bmac_addr + BIGMAC_REGISTER_TX_PAUSE_THRESHOLD,
			    wb_data, 2);
	}

	REG_WR(bp, NIG_REG_XGXS_SERDES0_MODE_SEL + port*4, 0x1);
	REG_WR(bp, NIG_REG_XGXS_LANE_SEL_P0 + port*4, 0x0);
	REG_WR(bp, NIG_REG_EGRESS_EMAC0_PORT + port*4, 0x0);
	val = 0;
	if (vars->flow_ctrl & BNX2X_FLOW_CTRL_TX)
		val = 1;
	REG_WR(bp, NIG_REG_BMAC0_PAUSE_OUT_EN + port*4, val);
	REG_WR(bp, NIG_REG_EGRESS_EMAC0_OUT_EN + port*4, 0x0);
	REG_WR(bp, NIG_REG_EMAC0_IN_EN + port*4, 0x0);
	REG_WR(bp, NIG_REG_EMAC0_PAUSE_OUT_EN + port*4, 0x0);
	REG_WR(bp, NIG_REG_BMAC0_IN_EN + port*4, 0x1);
	REG_WR(bp, NIG_REG_BMAC0_OUT_EN + port*4, 0x1);

	vars->mac_type = MAC_TYPE_BMAC;
	return 0;
}

static void bnx2x_phy_deassert(struct link_params *params, u8 phy_flags)
{
	struct bnx2x *bp = params->bp;
	u32 val;

	if (phy_flags & PHY_XGXS_FLAG) {
		DP(NETIF_MSG_LINK, "bnx2x_phy_deassert:XGXS\n");
		val = XGXS_RESET_BITS;

	} else { /* SerDes */
		DP(NETIF_MSG_LINK, "bnx2x_phy_deassert:SerDes\n");
		val = SERDES_RESET_BITS;
	}

	val = val << (params->port*16);

	/* reset and unreset the SerDes/XGXS */
	REG_WR(bp, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_3_CLEAR,
		    val);
	udelay(500);
	REG_WR(bp, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_3_SET,
		    val);
	bnx2x_set_phy_mdio(params, phy_flags);
}

void bnx2x_link_status_update(struct link_params *params,
			    struct link_vars   *vars)
{
	struct bnx2x *bp = params->bp;
	u8 link_10g;
	u8 port = params->port;

	if (params->switch_cfg ==  SWITCH_CFG_1G)
		vars->phy_flags = PHY_SERDES_FLAG;
	else
		vars->phy_flags = PHY_XGXS_FLAG;
	vars->link_status = REG_RD(bp, params->shmem_base +
					  offsetof(struct shmem_region,
					   port_mb[port].link_status));

	vars->link_up = (vars->link_status & LINK_STATUS_LINK_UP);

	if (vars->link_up) {
		DP(NETIF_MSG_LINK, "phy link up\n");

		vars->phy_link_up = 1;
		vars->duplex = DUPLEX_FULL;
		switch (vars->link_status &
					LINK_STATUS_SPEED_AND_DUPLEX_MASK) {
			case LINK_10THD:
				vars->duplex = DUPLEX_HALF;
				/* fall thru */
			case LINK_10TFD:
				vars->line_speed = SPEED_10;
				break;

			case LINK_100TXHD:
				vars->duplex = DUPLEX_HALF;
				/* fall thru */
			case LINK_100T4:
			case LINK_100TXFD:
				vars->line_speed = SPEED_100;
				break;

			case LINK_1000THD:
				vars->duplex = DUPLEX_HALF;
				/* fall thru */
			case LINK_1000TFD:
				vars->line_speed = SPEED_1000;
				break;

			case LINK_2500THD:
				vars->duplex = DUPLEX_HALF;
				/* fall thru */
			case LINK_2500TFD:
				vars->line_speed = SPEED_2500;
				break;

			case LINK_10GTFD:
				vars->line_speed = SPEED_10000;
				break;

			case LINK_12GTFD:
				vars->line_speed = SPEED_12000;
				break;

			case LINK_12_5GTFD:
				vars->line_speed = SPEED_12500;
				break;

			case LINK_13GTFD:
				vars->line_speed = SPEED_13000;
				break;

			case LINK_15GTFD:
				vars->line_speed = SPEED_15000;
				break;

			case LINK_16GTFD:
				vars->line_speed = SPEED_16000;
				break;

			default:
				break;
		}

		if (vars->link_status & LINK_STATUS_TX_FLOW_CONTROL_ENABLED)
			vars->flow_ctrl |= BNX2X_FLOW_CTRL_TX;
		else
			vars->flow_ctrl &= ~BNX2X_FLOW_CTRL_TX;

		if (vars->link_status & LINK_STATUS_RX_FLOW_CONTROL_ENABLED)
			vars->flow_ctrl |= BNX2X_FLOW_CTRL_RX;
		else
			vars->flow_ctrl &= ~BNX2X_FLOW_CTRL_RX;

		if (vars->phy_flags & PHY_XGXS_FLAG) {
			if (vars->line_speed &&
			    ((vars->line_speed == SPEED_10) ||
			     (vars->line_speed == SPEED_100))) {
				vars->phy_flags |= PHY_SGMII_FLAG;
			} else {
				vars->phy_flags &= ~PHY_SGMII_FLAG;
			}
		}

		/* anything 10 and over uses the bmac */
		link_10g = ((vars->line_speed == SPEED_10000) ||
			    (vars->line_speed == SPEED_12000) ||
			    (vars->line_speed == SPEED_12500) ||
			    (vars->line_speed == SPEED_13000) ||
			    (vars->line_speed == SPEED_15000) ||
			    (vars->line_speed == SPEED_16000));
		if (link_10g)
			vars->mac_type = MAC_TYPE_BMAC;
		else
			vars->mac_type = MAC_TYPE_EMAC;

	} else { /* link down */
		DP(NETIF_MSG_LINK, "phy link down\n");

		vars->phy_link_up = 0;

		vars->line_speed = 0;
		vars->duplex = DUPLEX_FULL;
		vars->flow_ctrl = BNX2X_FLOW_CTRL_NONE;

		/* indicate no mac active */
		vars->mac_type = MAC_TYPE_NONE;
	}

	DP(NETIF_MSG_LINK, "link_status 0x%x  phy_link_up %x\n",
		 vars->link_status, vars->phy_link_up);
	DP(NETIF_MSG_LINK, "line_speed %x  duplex %x  flow_ctrl 0x%x\n",
		 vars->line_speed, vars->duplex, vars->flow_ctrl);
}

static void bnx2x_update_mng(struct link_params *params, u32 link_status)
{
	struct bnx2x *bp = params->bp;

	REG_WR(bp, params->shmem_base +
		   offsetof(struct shmem_region,
			    port_mb[params->port].link_status),
			link_status);
}

static void bnx2x_bmac_rx_disable(struct bnx2x *bp, u8 port)
{
	u32 bmac_addr = port ? NIG_REG_INGRESS_BMAC1_MEM :
		NIG_REG_INGRESS_BMAC0_MEM;
	u32 wb_data[2];
	u32 nig_bmac_enable = REG_RD(bp, NIG_REG_BMAC0_REGS_OUT_EN + port*4);

	/* Only if the bmac is out of reset */
	if (REG_RD(bp, MISC_REG_RESET_REG_2) &
			(MISC_REGISTERS_RESET_REG_2_RST_BMAC0 << port) &&
	    nig_bmac_enable) {

		/* Clear Rx Enable bit in BMAC_CONTROL register */
		REG_RD_DMAE(bp, bmac_addr + BIGMAC_REGISTER_BMAC_CONTROL,
			    wb_data, 2);
		wb_data[0] &= ~BMAC_CONTROL_RX_ENABLE;
		REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_BMAC_CONTROL,
			    wb_data, 2);

		msleep(1);
	}
}

static u8 bnx2x_pbf_update(struct link_params *params, u32 flow_ctrl,
			 u32 line_speed)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u32 init_crd, crd;
	u32 count = 1000;

	/* disable port */
	REG_WR(bp, PBF_REG_DISABLE_NEW_TASK_PROC_P0 + port*4, 0x1);

	/* wait for init credit */
	init_crd = REG_RD(bp, PBF_REG_P0_INIT_CRD + port*4);
	crd = REG_RD(bp, PBF_REG_P0_CREDIT + port*8);
	DP(NETIF_MSG_LINK, "init_crd 0x%x  crd 0x%x\n", init_crd, crd);

	while ((init_crd != crd) && count) {
		msleep(5);

		crd = REG_RD(bp, PBF_REG_P0_CREDIT + port*8);
		count--;
	}
	crd = REG_RD(bp, PBF_REG_P0_CREDIT + port*8);
	if (init_crd != crd) {
		DP(NETIF_MSG_LINK, "BUG! init_crd 0x%x != crd 0x%x\n",
			  init_crd, crd);
		return -EINVAL;
	}

	if (flow_ctrl & BNX2X_FLOW_CTRL_RX ||
	    line_speed == SPEED_10 ||
	    line_speed == SPEED_100 ||
	    line_speed == SPEED_1000 ||
	    line_speed == SPEED_2500) {
		REG_WR(bp, PBF_REG_P0_PAUSE_ENABLE + port*4, 1);
		/* update threshold */
		REG_WR(bp, PBF_REG_P0_ARB_THRSH + port*4, 0);
		/* update init credit */
		init_crd = 778; 	/* (800-18-4) */

	} else {
		u32 thresh = (ETH_MAX_JUMBO_PACKET_SIZE +
			      ETH_OVREHEAD)/16;
		REG_WR(bp, PBF_REG_P0_PAUSE_ENABLE + port*4, 0);
		/* update threshold */
		REG_WR(bp, PBF_REG_P0_ARB_THRSH + port*4, thresh);
		/* update init credit */
		switch (line_speed) {
		case SPEED_10000:
			init_crd = thresh + 553 - 22;
			break;

		case SPEED_12000:
			init_crd = thresh + 664 - 22;
			break;

		case SPEED_13000:
			init_crd = thresh + 742 - 22;
			break;

		case SPEED_16000:
			init_crd = thresh + 778 - 22;
			break;
		default:
			DP(NETIF_MSG_LINK, "Invalid line_speed 0x%x\n",
				  line_speed);
			return -EINVAL;
		}
	}
	REG_WR(bp, PBF_REG_P0_INIT_CRD + port*4, init_crd);
	DP(NETIF_MSG_LINK, "PBF updated to speed %d credit %d\n",
		 line_speed, init_crd);

	/* probe the credit changes */
	REG_WR(bp, PBF_REG_INIT_P0 + port*4, 0x1);
	msleep(5);
	REG_WR(bp, PBF_REG_INIT_P0 + port*4, 0x0);

	/* enable port */
	REG_WR(bp, PBF_REG_DISABLE_NEW_TASK_PROC_P0 + port*4, 0x0);
	return 0;
}

static u32 bnx2x_get_emac_base(struct bnx2x *bp, u32 ext_phy_type, u8 port)
{
	u32 emac_base;

	switch (ext_phy_type) {
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
		/* All MDC/MDIO is directed through single EMAC */
		if (REG_RD(bp, NIG_REG_PORT_SWAP))
			emac_base = GRCBASE_EMAC0;
		else
			emac_base = GRCBASE_EMAC1;
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
		emac_base = (port) ? GRCBASE_EMAC0 : GRCBASE_EMAC1;
		break;
	default:
		emac_base = (port) ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
		break;
	}
	return emac_base;

}

u8 bnx2x_cl45_write(struct bnx2x *bp, u8 port, u32 ext_phy_type,
		  u8 phy_addr, u8 devad, u16 reg, u16 val)
{
	u32 tmp, saved_mode;
	u8 i, rc = 0;
	u32 mdio_ctrl = bnx2x_get_emac_base(bp, ext_phy_type, port);

	/* set clause 45 mode, slow down the MDIO clock to 2.5MHz
	 * (a value of 49==0x31) and make sure that the AUTO poll is off
	 */

	saved_mode = REG_RD(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE);
	tmp = saved_mode & ~(EMAC_MDIO_MODE_AUTO_POLL |
			     EMAC_MDIO_MODE_CLOCK_CNT);
	tmp |= (EMAC_MDIO_MODE_CLAUSE_45 |
		(49 << EMAC_MDIO_MODE_CLOCK_CNT_BITSHIFT));
	REG_WR(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE, tmp);
	REG_RD(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE);
	udelay(40);

	/* address */

	tmp = ((phy_addr << 21) | (devad << 16) | reg |
	       EMAC_MDIO_COMM_COMMAND_ADDRESS |
	       EMAC_MDIO_COMM_START_BUSY);
	REG_WR(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM, tmp);

	for (i = 0; i < 50; i++) {
		udelay(10);

		tmp = REG_RD(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM);
		if (!(tmp & EMAC_MDIO_COMM_START_BUSY)) {
			udelay(5);
			break;
		}
	}
	if (tmp & EMAC_MDIO_COMM_START_BUSY) {
		DP(NETIF_MSG_LINK, "write phy register failed\n");
		rc = -EFAULT;
	} else {
		/* data */
		tmp = ((phy_addr << 21) | (devad << 16) | val |
		       EMAC_MDIO_COMM_COMMAND_WRITE_45 |
		       EMAC_MDIO_COMM_START_BUSY);
		REG_WR(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM, tmp);

		for (i = 0; i < 50; i++) {
			udelay(10);

			tmp = REG_RD(bp, mdio_ctrl +
					 EMAC_REG_EMAC_MDIO_COMM);
			if (!(tmp & EMAC_MDIO_COMM_START_BUSY)) {
				udelay(5);
				break;
			}
		}
		if (tmp & EMAC_MDIO_COMM_START_BUSY) {
			DP(NETIF_MSG_LINK, "write phy register failed\n");
			rc = -EFAULT;
		}
	}

	/* Restore the saved mode */
	REG_WR(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE, saved_mode);

	return rc;
}

u8 bnx2x_cl45_read(struct bnx2x *bp, u8 port, u32 ext_phy_type,
		 u8 phy_addr, u8 devad, u16 reg, u16 *ret_val)
{
	u32 val, saved_mode;
	u16 i;
	u8 rc = 0;

	u32 mdio_ctrl = bnx2x_get_emac_base(bp, ext_phy_type, port);
	/* set clause 45 mode, slow down the MDIO clock to 2.5MHz
	 * (a value of 49==0x31) and make sure that the AUTO poll is off
	 */

	saved_mode = REG_RD(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE);
	val = saved_mode & ((EMAC_MDIO_MODE_AUTO_POLL |
			     EMAC_MDIO_MODE_CLOCK_CNT));
	val |= (EMAC_MDIO_MODE_CLAUSE_45 |
		(49L << EMAC_MDIO_MODE_CLOCK_CNT_BITSHIFT));
	REG_WR(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE, val);
	REG_RD(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE);
	udelay(40);

	/* address */
	val = ((phy_addr << 21) | (devad << 16) | reg |
	       EMAC_MDIO_COMM_COMMAND_ADDRESS |
	       EMAC_MDIO_COMM_START_BUSY);
	REG_WR(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM, val);

	for (i = 0; i < 50; i++) {
		udelay(10);

		val = REG_RD(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM);
		if (!(val & EMAC_MDIO_COMM_START_BUSY)) {
			udelay(5);
			break;
		}
	}
	if (val & EMAC_MDIO_COMM_START_BUSY) {
		DP(NETIF_MSG_LINK, "read phy register failed\n");

		*ret_val = 0;
		rc = -EFAULT;

	} else {
		/* data */
		val = ((phy_addr << 21) | (devad << 16) |
		       EMAC_MDIO_COMM_COMMAND_READ_45 |
		       EMAC_MDIO_COMM_START_BUSY);
		REG_WR(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM, val);

		for (i = 0; i < 50; i++) {
			udelay(10);

			val = REG_RD(bp, mdio_ctrl +
					  EMAC_REG_EMAC_MDIO_COMM);
			if (!(val & EMAC_MDIO_COMM_START_BUSY)) {
				*ret_val = (u16)(val & EMAC_MDIO_COMM_DATA);
				break;
			}
		}
		if (val & EMAC_MDIO_COMM_START_BUSY) {
			DP(NETIF_MSG_LINK, "read phy register failed\n");

			*ret_val = 0;
			rc = -EFAULT;
		}
	}

	/* Restore the saved mode */
	REG_WR(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE, saved_mode);

	return rc;
}

static void bnx2x_set_aer_mmd(struct link_params *params,
			    struct link_vars   *vars)
{
	struct bnx2x *bp = params->bp;
	u32 ser_lane;
	u16 offset;

	ser_lane = ((params->lane_config &
		     PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK) >>
		     PORT_HW_CFG_LANE_SWAP_CFG_MASTER_SHIFT);

	offset = (vars->phy_flags & PHY_XGXS_FLAG) ?
		(params->phy_addr + ser_lane) : 0;

	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_AER_BLOCK,
			      MDIO_AER_BLOCK_AER_REG, 0x3800 + offset);
}

static void bnx2x_set_master_ln(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u16 new_master_ln, ser_lane;
	ser_lane =  ((params->lane_config &
		     PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK) >>
		     PORT_HW_CFG_LANE_SWAP_CFG_MASTER_SHIFT);

	/* set the master_ln for AN */
	CL45_RD_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_XGXS_BLOCK2,
			      MDIO_XGXS_BLOCK2_TEST_MODE_LANE,
			      &new_master_ln);

	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_XGXS_BLOCK2 ,
			      MDIO_XGXS_BLOCK2_TEST_MODE_LANE,
			      (new_master_ln | ser_lane));
}

static u8 bnx2x_reset_unicore(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u16 mii_control;
	u16 i;

	CL45_RD_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_COMBO_IEEE0,
			      MDIO_COMBO_IEEE0_MII_CONTROL, &mii_control);

	/* reset the unicore */
	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_COMBO_IEEE0,
			      MDIO_COMBO_IEEE0_MII_CONTROL,
			      (mii_control |
			       MDIO_COMBO_IEEO_MII_CONTROL_RESET));
	if (params->switch_cfg == SWITCH_CFG_1G)
		bnx2x_set_serdes_access(params);

	/* wait for the reset to self clear */
	for (i = 0; i < MDIO_ACCESS_TIMEOUT; i++) {
		udelay(5);

		/* the reset erased the previous bank value */
		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
			      MDIO_REG_BANK_COMBO_IEEE0,
			      MDIO_COMBO_IEEE0_MII_CONTROL,
			      &mii_control);

		if (!(mii_control & MDIO_COMBO_IEEO_MII_CONTROL_RESET)) {
			udelay(5);
			return 0;
		}
	}

	DP(NETIF_MSG_LINK, "BUG! XGXS is still in reset!\n");
	return -EINVAL;

}

static void bnx2x_set_swap_lanes(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	/* Each two bits represents a lane number:
	   No swap is 0123 => 0x1b no need to enable the swap */
	u16 ser_lane, rx_lane_swap, tx_lane_swap;

	ser_lane = ((params->lane_config &
			 PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK) >>
			PORT_HW_CFG_LANE_SWAP_CFG_MASTER_SHIFT);
	rx_lane_swap = ((params->lane_config &
			     PORT_HW_CFG_LANE_SWAP_CFG_RX_MASK) >>
			    PORT_HW_CFG_LANE_SWAP_CFG_RX_SHIFT);
	tx_lane_swap = ((params->lane_config &
			     PORT_HW_CFG_LANE_SWAP_CFG_TX_MASK) >>
			    PORT_HW_CFG_LANE_SWAP_CFG_TX_SHIFT);

	if (rx_lane_swap != 0x1b) {
		CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				    MDIO_REG_BANK_XGXS_BLOCK2,
				    MDIO_XGXS_BLOCK2_RX_LN_SWAP,
				    (rx_lane_swap |
				    MDIO_XGXS_BLOCK2_RX_LN_SWAP_ENABLE |
				    MDIO_XGXS_BLOCK2_RX_LN_SWAP_FORCE_ENABLE));
	} else {
		CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_XGXS_BLOCK2,
				      MDIO_XGXS_BLOCK2_RX_LN_SWAP, 0);
	}

	if (tx_lane_swap != 0x1b) {
		CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_XGXS_BLOCK2,
				      MDIO_XGXS_BLOCK2_TX_LN_SWAP,
				      (tx_lane_swap |
				       MDIO_XGXS_BLOCK2_TX_LN_SWAP_ENABLE));
	} else {
		CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_XGXS_BLOCK2,
				      MDIO_XGXS_BLOCK2_TX_LN_SWAP, 0);
	}
}

static void bnx2x_set_parallel_detection(struct link_params *params,
				       u8       	 phy_flags)
{
	struct bnx2x *bp = params->bp;
	u16 control2;

	CL45_RD_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_SERDES_DIGITAL,
			      MDIO_SERDES_DIGITAL_A_1000X_CONTROL2,
			      &control2);


	control2 |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL2_PRL_DT_EN;


	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_SERDES_DIGITAL,
			      MDIO_SERDES_DIGITAL_A_1000X_CONTROL2,
			      control2);

	if (phy_flags & PHY_XGXS_FLAG) {
		DP(NETIF_MSG_LINK, "XGXS\n");

		CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				MDIO_REG_BANK_10G_PARALLEL_DETECT,
				MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_LINK,
				MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_LINK_CNT);

		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
				MDIO_REG_BANK_10G_PARALLEL_DETECT,
				MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_CONTROL,
				&control2);


		control2 |=
		    MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_CONTROL_PARDET10G_EN;

		CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				MDIO_REG_BANK_10G_PARALLEL_DETECT,
				MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_CONTROL,
				control2);

		/* Disable parallel detection of HiG */
		CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				MDIO_REG_BANK_XGXS_BLOCK2,
				MDIO_XGXS_BLOCK2_UNICORE_MODE_10G,
				MDIO_XGXS_BLOCK2_UNICORE_MODE_10G_CX4_XGXS |
				MDIO_XGXS_BLOCK2_UNICORE_MODE_10G_HIGIG_XGXS);
	}
}

static void bnx2x_set_autoneg(struct link_params *params,
			    struct link_vars *vars,
			    u8 enable_cl73)
{
	struct bnx2x *bp = params->bp;
	u16 reg_val;

	/* CL37 Autoneg */

	CL45_RD_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_COMBO_IEEE0,
			      MDIO_COMBO_IEEE0_MII_CONTROL, &reg_val);

	/* CL37 Autoneg Enabled */
	if (vars->line_speed == SPEED_AUTO_NEG)
		reg_val |= MDIO_COMBO_IEEO_MII_CONTROL_AN_EN;
	else /* CL37 Autoneg Disabled */
		reg_val &= ~(MDIO_COMBO_IEEO_MII_CONTROL_AN_EN |
			     MDIO_COMBO_IEEO_MII_CONTROL_RESTART_AN);

	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_COMBO_IEEE0,
			      MDIO_COMBO_IEEE0_MII_CONTROL, reg_val);

	/* Enable/Disable Autodetection */

	CL45_RD_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_SERDES_DIGITAL,
			      MDIO_SERDES_DIGITAL_A_1000X_CONTROL1, &reg_val);
	reg_val &= ~(MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_SIGNAL_DETECT_EN |
		    MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_INVERT_SIGNAL_DETECT);
	reg_val |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_FIBER_MODE;
	if (vars->line_speed == SPEED_AUTO_NEG)
		reg_val |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET;
	else
		reg_val &= ~MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET;

	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_SERDES_DIGITAL,
			      MDIO_SERDES_DIGITAL_A_1000X_CONTROL1, reg_val);

	/* Enable TetonII and BAM autoneg */
	CL45_RD_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_BAM_NEXT_PAGE,
			      MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL,
			  &reg_val);
	if (vars->line_speed == SPEED_AUTO_NEG) {
		/* Enable BAM aneg Mode and TetonII aneg Mode */
		reg_val |= (MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL_BAM_MODE |
			    MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL_TETON_AN);
	} else {
		/* TetonII and BAM Autoneg Disabled */
		reg_val &= ~(MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL_BAM_MODE |
			     MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL_TETON_AN);
	}
	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_BAM_NEXT_PAGE,
			      MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL,
			      reg_val);

	if (enable_cl73) {
		/* Enable Cl73 FSM status bits */
		CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_CL73_USERB0,
				    MDIO_CL73_USERB0_CL73_UCTRL,
				    MDIO_CL73_USERB0_CL73_UCTRL_USTAT1_MUXSEL);

		/* Enable BAM Station Manager*/
		CL45_WR_OVER_CL22(bp, params->port,
			params->phy_addr,
			MDIO_REG_BANK_CL73_USERB0,
			MDIO_CL73_USERB0_CL73_BAM_CTRL1,
			MDIO_CL73_USERB0_CL73_BAM_CTRL1_BAM_EN |
			MDIO_CL73_USERB0_CL73_BAM_CTRL1_BAM_STATION_MNGR_EN |
			MDIO_CL73_USERB0_CL73_BAM_CTRL1_BAM_NP_AFTER_BP_EN);

		/* Merge CL73 and CL37 aneg resolution */
		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_CL73_USERB0,
				      MDIO_CL73_USERB0_CL73_BAM_CTRL3,
				      &reg_val);

		if (params->speed_cap_mask &
		    PORT_HW_CFG_SPEED_CAPABILITY_D0_10G) {
			/* Set the CL73 AN speed */
			CL45_RD_OVER_CL22(bp, params->port,
					      params->phy_addr,
					      MDIO_REG_BANK_CL73_IEEEB1,
					      MDIO_CL73_IEEEB1_AN_ADV2,
					      &reg_val);

			CL45_WR_OVER_CL22(bp, params->port,
					      params->phy_addr,
					      MDIO_REG_BANK_CL73_IEEEB1,
					      MDIO_CL73_IEEEB1_AN_ADV2,
			  reg_val | MDIO_CL73_IEEEB1_AN_ADV2_ADVR_10G_KX4);

		}
		/* CL73 Autoneg Enabled */
		reg_val = MDIO_CL73_IEEEB0_CL73_AN_CONTROL_AN_EN;

	} else /* CL73 Autoneg Disabled */
		reg_val = 0;

	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_CL73_IEEEB0,
			      MDIO_CL73_IEEEB0_CL73_AN_CONTROL, reg_val);
}

/* program SerDes, forced speed */
static void bnx2x_program_serdes(struct link_params *params,
			       struct link_vars *vars)
{
	struct bnx2x *bp = params->bp;
	u16 reg_val;

	/* program duplex, disable autoneg and sgmii*/
	CL45_RD_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_COMBO_IEEE0,
			      MDIO_COMBO_IEEE0_MII_CONTROL, &reg_val);
	reg_val &= ~(MDIO_COMBO_IEEO_MII_CONTROL_FULL_DUPLEX |
		     MDIO_COMBO_IEEO_MII_CONTROL_AN_EN |
		     MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_MASK);
	if (params->req_duplex == DUPLEX_FULL)
		reg_val |= MDIO_COMBO_IEEO_MII_CONTROL_FULL_DUPLEX;
	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_COMBO_IEEE0,
			      MDIO_COMBO_IEEE0_MII_CONTROL, reg_val);

	/* program speed
	   - needed only if the speed is greater than 1G (2.5G or 10G) */
	CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_SERDES_DIGITAL,
				      MDIO_SERDES_DIGITAL_MISC1, &reg_val);
	/* clearing the speed value before setting the right speed */
	DP(NETIF_MSG_LINK, "MDIO_REG_BANK_SERDES_DIGITAL = 0x%x\n", reg_val);

	reg_val &= ~(MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_MASK |
		     MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_SEL);

	if (!((vars->line_speed == SPEED_1000) ||
	      (vars->line_speed == SPEED_100) ||
	      (vars->line_speed == SPEED_10))) {

		reg_val |= (MDIO_SERDES_DIGITAL_MISC1_REFCLK_SEL_156_25M |
			    MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_SEL);
		if (vars->line_speed == SPEED_10000)
			reg_val |=
				MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_10G_CX4;
		if (vars->line_speed == SPEED_13000)
			reg_val |=
				MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_13G;
	}

	CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_SERDES_DIGITAL,
				      MDIO_SERDES_DIGITAL_MISC1, reg_val);

}

static void bnx2x_set_brcm_cl37_advertisment(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u16 val = 0;

	/* configure the 48 bits for BAM AN */

	/* set extended capabilities */
	if (params->speed_cap_mask & PORT_HW_CFG_SPEED_CAPABILITY_D0_2_5G)
		val |= MDIO_OVER_1G_UP1_2_5G;
	if (params->speed_cap_mask & PORT_HW_CFG_SPEED_CAPABILITY_D0_10G)
		val |= MDIO_OVER_1G_UP1_10G;
	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_OVER_1G,
			      MDIO_OVER_1G_UP1, val);

	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_OVER_1G,
			      MDIO_OVER_1G_UP3, 0x400);
}

static void bnx2x_calc_ieee_aneg_adv(struct link_params *params, u16 *ieee_fc)
{
	*ieee_fc = MDIO_COMBO_IEEE0_AUTO_NEG_ADV_FULL_DUPLEX;
	/* resolve pause mode and advertisement
	 * Please refer to Table 28B-3 of the 802.3ab-1999 spec */

	switch (params->req_flow_ctrl) {
	case BNX2X_FLOW_CTRL_AUTO:
		if (params->req_fc_auto_adv == BNX2X_FLOW_CTRL_BOTH) {
			*ieee_fc |=
			     MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH;
		} else {
			*ieee_fc |=
		       MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC;
		}
		break;
	case BNX2X_FLOW_CTRL_TX:
		*ieee_fc |=
		       MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC;
		break;

	case BNX2X_FLOW_CTRL_RX:
	case BNX2X_FLOW_CTRL_BOTH:
		*ieee_fc |= MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH;
		break;

	case BNX2X_FLOW_CTRL_NONE:
	default:
		*ieee_fc |= MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_NONE;
		break;
	}
}

static void bnx2x_set_ieee_aneg_advertisment(struct link_params *params,
					   u16 ieee_fc)
{
	struct bnx2x *bp = params->bp;
	/* for AN, we are always publishing full duplex */

	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_COMBO_IEEE0,
			      MDIO_COMBO_IEEE0_AUTO_NEG_ADV, ieee_fc);
}

static void bnx2x_restart_autoneg(struct link_params *params, u8 enable_cl73)
{
	struct bnx2x *bp = params->bp;
	u16 mii_control;

	DP(NETIF_MSG_LINK, "bnx2x_restart_autoneg\n");
	/* Enable and restart BAM/CL37 aneg */

	if (enable_cl73) {
		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_CL73_IEEEB0,
				      MDIO_CL73_IEEEB0_CL73_AN_CONTROL,
				      &mii_control);

		CL45_WR_OVER_CL22(bp, params->port,
				params->phy_addr,
				MDIO_REG_BANK_CL73_IEEEB0,
				MDIO_CL73_IEEEB0_CL73_AN_CONTROL,
				(mii_control |
				MDIO_CL73_IEEEB0_CL73_AN_CONTROL_AN_EN |
				MDIO_CL73_IEEEB0_CL73_AN_CONTROL_RESTART_AN));
	} else {

		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_COMBO_IEEE0,
				      MDIO_COMBO_IEEE0_MII_CONTROL,
				      &mii_control);
		DP(NETIF_MSG_LINK,
			 "bnx2x_restart_autoneg mii_control before = 0x%x\n",
			 mii_control);
		CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_COMBO_IEEE0,
				      MDIO_COMBO_IEEE0_MII_CONTROL,
				      (mii_control |
				       MDIO_COMBO_IEEO_MII_CONTROL_AN_EN |
				       MDIO_COMBO_IEEO_MII_CONTROL_RESTART_AN));
	}
}

static void bnx2x_initialize_sgmii_process(struct link_params *params,
					 struct link_vars *vars)
{
	struct bnx2x *bp = params->bp;
	u16 control1;

	/* in SGMII mode, the unicore is always slave */

	CL45_RD_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_SERDES_DIGITAL,
			      MDIO_SERDES_DIGITAL_A_1000X_CONTROL1,
		      &control1);
	control1 |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_INVERT_SIGNAL_DETECT;
	/* set sgmii mode (and not fiber) */
	control1 &= ~(MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_FIBER_MODE |
		      MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET |
		      MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_MSTR_MODE);
	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_SERDES_DIGITAL,
			      MDIO_SERDES_DIGITAL_A_1000X_CONTROL1,
			      control1);

	/* if forced speed */
	if (!(vars->line_speed == SPEED_AUTO_NEG)) {
		/* set speed, disable autoneg */
		u16 mii_control;

		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_COMBO_IEEE0,
				      MDIO_COMBO_IEEE0_MII_CONTROL,
				      &mii_control);
		mii_control &= ~(MDIO_COMBO_IEEO_MII_CONTROL_AN_EN |
				 MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_MASK|
				 MDIO_COMBO_IEEO_MII_CONTROL_FULL_DUPLEX);

		switch (vars->line_speed) {
		case SPEED_100:
			mii_control |=
				MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_100;
			break;
		case SPEED_1000:
			mii_control |=
				MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_1000;
			break;
		case SPEED_10:
			/* there is nothing to set for 10M */
			break;
		default:
			/* invalid speed for SGMII */
			DP(NETIF_MSG_LINK, "Invalid line_speed 0x%x\n",
				  vars->line_speed);
			break;
		}

		/* setting the full duplex */
		if (params->req_duplex == DUPLEX_FULL)
			mii_control |=
				MDIO_COMBO_IEEO_MII_CONTROL_FULL_DUPLEX;
		CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_COMBO_IEEE0,
				      MDIO_COMBO_IEEE0_MII_CONTROL,
				      mii_control);

	} else { /* AN mode */
		/* enable and restart AN */
		bnx2x_restart_autoneg(params, 0);
	}
}


/*
 * link management
 */

static void bnx2x_pause_resolve(struct link_vars *vars, u32 pause_result)
{						/*  LD	    LP	 */
	switch (pause_result) { 		/* ASYM P ASYM P */
	case 0xb:       			/*   1  0   1  1 */
		vars->flow_ctrl = BNX2X_FLOW_CTRL_TX;
		break;

	case 0xe:       			/*   1  1   1  0 */
		vars->flow_ctrl = BNX2X_FLOW_CTRL_RX;
		break;

	case 0x5:       			/*   0  1   0  1 */
	case 0x7:       			/*   0  1   1  1 */
	case 0xd:       			/*   1  1   0  1 */
	case 0xf:       			/*   1  1   1  1 */
		vars->flow_ctrl = BNX2X_FLOW_CTRL_BOTH;
		break;

	default:
		break;
	}
}

static u8 bnx2x_ext_phy_resolve_fc(struct link_params *params,
				  struct link_vars *vars)
{
	struct bnx2x *bp = params->bp;
	u8 ext_phy_addr;
	u16 ld_pause;		/* local */
	u16 lp_pause;		/* link partner */
	u16 an_complete;	/* AN complete */
	u16 pause_result;
	u8 ret = 0;
	u32 ext_phy_type;
	u8 port = params->port;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
	/* read twice */

	bnx2x_cl45_read(bp, port,
		      ext_phy_type,
		      ext_phy_addr,
		      MDIO_AN_DEVAD,
		      MDIO_AN_REG_STATUS, &an_complete);
	bnx2x_cl45_read(bp, port,
		      ext_phy_type,
		      ext_phy_addr,
		      MDIO_AN_DEVAD,
		      MDIO_AN_REG_STATUS, &an_complete);

	if (an_complete & MDIO_AN_REG_STATUS_AN_COMPLETE) {
		ret = 1;
		bnx2x_cl45_read(bp, port,
			      ext_phy_type,
			      ext_phy_addr,
			      MDIO_AN_DEVAD,
			      MDIO_AN_REG_ADV_PAUSE, &ld_pause);
		bnx2x_cl45_read(bp, port,
			      ext_phy_type,
			      ext_phy_addr,
			      MDIO_AN_DEVAD,
			      MDIO_AN_REG_LP_AUTO_NEG, &lp_pause);
		pause_result = (ld_pause &
				MDIO_AN_REG_ADV_PAUSE_MASK) >> 8;
		pause_result |= (lp_pause &
				 MDIO_AN_REG_ADV_PAUSE_MASK) >> 10;
		DP(NETIF_MSG_LINK, "Ext PHY pause result 0x%x \n",
		   pause_result);
		bnx2x_pause_resolve(vars, pause_result);
		if (vars->flow_ctrl == BNX2X_FLOW_CTRL_NONE &&
		     ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073) {
			bnx2x_cl45_read(bp, port,
				      ext_phy_type,
				      ext_phy_addr,
				      MDIO_AN_DEVAD,
				      MDIO_AN_REG_CL37_FC_LD, &ld_pause);

			bnx2x_cl45_read(bp, port,
				      ext_phy_type,
				      ext_phy_addr,
				      MDIO_AN_DEVAD,
				      MDIO_AN_REG_CL37_FC_LP, &lp_pause);
			pause_result = (ld_pause &
				MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH) >> 5;
			pause_result |= (lp_pause &
				MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH) >> 7;

			bnx2x_pause_resolve(vars, pause_result);
			DP(NETIF_MSG_LINK, "Ext PHY CL37 pause result 0x%x \n",
				 pause_result);
		}
	}
	return ret;
}


static void bnx2x_flow_ctrl_resolve(struct link_params *params,
				  struct link_vars *vars,
				  u32 gp_status)
{
	struct bnx2x *bp = params->bp;
	u16 ld_pause;   /* local driver */
	u16 lp_pause;   /* link partner */
	u16 pause_result;

	vars->flow_ctrl = BNX2X_FLOW_CTRL_NONE;

	/* resolve from gp_status in case of AN complete and not sgmii */
	if ((params->req_flow_ctrl == BNX2X_FLOW_CTRL_AUTO) &&
	    (gp_status & MDIO_AN_CL73_OR_37_COMPLETE) &&
	    (!(vars->phy_flags & PHY_SGMII_FLAG)) &&
	    (XGXS_EXT_PHY_TYPE(params->ext_phy_config) ==
	     PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT)) {
		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_COMBO_IEEE0,
				      MDIO_COMBO_IEEE0_AUTO_NEG_ADV,
				      &ld_pause);
		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
			MDIO_REG_BANK_COMBO_IEEE0,
			MDIO_COMBO_IEEE0_AUTO_NEG_LINK_PARTNER_ABILITY1,
			&lp_pause);
		pause_result = (ld_pause &
				MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_MASK)>>5;
		pause_result |= (lp_pause &
				 MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_MASK)>>7;
		DP(NETIF_MSG_LINK, "pause_result 0x%x\n", pause_result);
		bnx2x_pause_resolve(vars, pause_result);
	} else if ((params->req_flow_ctrl == BNX2X_FLOW_CTRL_AUTO) &&
		   (bnx2x_ext_phy_resolve_fc(params, vars))) {
		return;
	} else {
		if (params->req_flow_ctrl == BNX2X_FLOW_CTRL_AUTO)
			vars->flow_ctrl = params->req_fc_auto_adv;
		else
			vars->flow_ctrl = params->req_flow_ctrl;
	}
	DP(NETIF_MSG_LINK, "flow_ctrl 0x%x\n", vars->flow_ctrl);
}

static void bnx2x_check_fallback_to_cl37(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u16 rx_status, ustat_val, cl37_fsm_recieved;
	DP(NETIF_MSG_LINK, "bnx2x_check_fallback_to_cl37\n");
	/* Step 1: Make sure signal is detected */
	CL45_RD_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_RX0,
			      MDIO_RX0_RX_STATUS,
			      &rx_status);
	if ((rx_status & MDIO_RX0_RX_STATUS_SIGDET) !=
	    (MDIO_RX0_RX_STATUS_SIGDET)) {
		DP(NETIF_MSG_LINK, "Signal is not detected. Restoring CL73."
			     "rx_status(0x80b0) = 0x%x\n", rx_status);
		CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_CL73_IEEEB0,
				      MDIO_CL73_IEEEB0_CL73_AN_CONTROL,
				      MDIO_CL73_IEEEB0_CL73_AN_CONTROL_AN_EN);
		return;
	}
	/* Step 2: Check CL73 state machine */
	CL45_RD_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_CL73_USERB0,
			      MDIO_CL73_USERB0_CL73_USTAT1,
			      &ustat_val);
	if ((ustat_val &
	     (MDIO_CL73_USERB0_CL73_USTAT1_LINK_STATUS_CHECK |
	      MDIO_CL73_USERB0_CL73_USTAT1_AN_GOOD_CHECK_BAM37)) !=
	    (MDIO_CL73_USERB0_CL73_USTAT1_LINK_STATUS_CHECK |
	      MDIO_CL73_USERB0_CL73_USTAT1_AN_GOOD_CHECK_BAM37)) {
		DP(NETIF_MSG_LINK, "CL73 state-machine is not stable. "
			     "ustat_val(0x8371) = 0x%x\n", ustat_val);
		return;
	}
	/* Step 3: Check CL37 Message Pages received to indicate LP
	supports only CL37 */
	CL45_RD_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_REMOTE_PHY,
			      MDIO_REMOTE_PHY_MISC_RX_STATUS,
			      &cl37_fsm_recieved);
	if ((cl37_fsm_recieved &
	     (MDIO_REMOTE_PHY_MISC_RX_STATUS_CL37_FSM_RECEIVED_OVER1G_MSG |
	     MDIO_REMOTE_PHY_MISC_RX_STATUS_CL37_FSM_RECEIVED_BRCM_OUI_MSG)) !=
	    (MDIO_REMOTE_PHY_MISC_RX_STATUS_CL37_FSM_RECEIVED_OVER1G_MSG |
	      MDIO_REMOTE_PHY_MISC_RX_STATUS_CL37_FSM_RECEIVED_BRCM_OUI_MSG)) {
		DP(NETIF_MSG_LINK, "No CL37 FSM were received. "
			     "misc_rx_status(0x8330) = 0x%x\n",
			 cl37_fsm_recieved);
		return;
	}
	/* The combined cl37/cl73 fsm state information indicating that we are
	connected to a device which does not support cl73, but does support
	cl37 BAM. In this case we disable cl73 and restart cl37 auto-neg */
	/* Disable CL73 */
	CL45_WR_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_CL73_IEEEB0,
			      MDIO_CL73_IEEEB0_CL73_AN_CONTROL,
			      0);
	/* Restart CL37 autoneg */
	bnx2x_restart_autoneg(params, 0);
	DP(NETIF_MSG_LINK, "Disabling CL73, and restarting CL37 autoneg\n");
}
static u8 bnx2x_link_settings_status(struct link_params *params,
				   struct link_vars *vars,
				   u32 gp_status,
				   u8 ext_phy_link_up)
{
	struct bnx2x *bp = params->bp;
	u16 new_line_speed;
	u8 rc = 0;
	vars->link_status = 0;

	if (gp_status & MDIO_GP_STATUS_TOP_AN_STATUS1_LINK_STATUS) {
		DP(NETIF_MSG_LINK, "phy link up gp_status=0x%x\n",
			 gp_status);

		vars->phy_link_up = 1;
		vars->link_status |= LINK_STATUS_LINK_UP;

		if (gp_status & MDIO_GP_STATUS_TOP_AN_STATUS1_DUPLEX_STATUS)
			vars->duplex = DUPLEX_FULL;
		else
			vars->duplex = DUPLEX_HALF;

		bnx2x_flow_ctrl_resolve(params, vars, gp_status);

		switch (gp_status & GP_STATUS_SPEED_MASK) {
		case GP_STATUS_10M:
			new_line_speed = SPEED_10;
			if (vars->duplex == DUPLEX_FULL)
				vars->link_status |= LINK_10TFD;
			else
				vars->link_status |= LINK_10THD;
			break;

		case GP_STATUS_100M:
			new_line_speed = SPEED_100;
			if (vars->duplex == DUPLEX_FULL)
				vars->link_status |= LINK_100TXFD;
			else
				vars->link_status |= LINK_100TXHD;
			break;

		case GP_STATUS_1G:
		case GP_STATUS_1G_KX:
			new_line_speed = SPEED_1000;
			if (vars->duplex == DUPLEX_FULL)
				vars->link_status |= LINK_1000TFD;
			else
				vars->link_status |= LINK_1000THD;
			break;

		case GP_STATUS_2_5G:
			new_line_speed = SPEED_2500;
			if (vars->duplex == DUPLEX_FULL)
				vars->link_status |= LINK_2500TFD;
			else
				vars->link_status |= LINK_2500THD;
			break;

		case GP_STATUS_5G:
		case GP_STATUS_6G:
			DP(NETIF_MSG_LINK,
				 "link speed unsupported  gp_status 0x%x\n",
				  gp_status);
			return -EINVAL;

		case GP_STATUS_10G_KX4:
		case GP_STATUS_10G_HIG:
		case GP_STATUS_10G_CX4:
			new_line_speed = SPEED_10000;
			vars->link_status |= LINK_10GTFD;
			break;

		case GP_STATUS_12G_HIG:
			new_line_speed = SPEED_12000;
			vars->link_status |= LINK_12GTFD;
			break;

		case GP_STATUS_12_5G:
			new_line_speed = SPEED_12500;
			vars->link_status |= LINK_12_5GTFD;
			break;

		case GP_STATUS_13G:
			new_line_speed = SPEED_13000;
			vars->link_status |= LINK_13GTFD;
			break;

		case GP_STATUS_15G:
			new_line_speed = SPEED_15000;
			vars->link_status |= LINK_15GTFD;
			break;

		case GP_STATUS_16G:
			new_line_speed = SPEED_16000;
			vars->link_status |= LINK_16GTFD;
			break;

		default:
			DP(NETIF_MSG_LINK,
				  "link speed unsupported gp_status 0x%x\n",
				  gp_status);
			return -EINVAL;
		}

		/* Upon link speed change set the NIG into drain mode.
		Comes to deals with possible FIFO glitch due to clk change
		when speed is decreased without link down indicator */
		if (new_line_speed != vars->line_speed) {
			if (XGXS_EXT_PHY_TYPE(params->ext_phy_config) !=
			     PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT &&
			    ext_phy_link_up) {
				DP(NETIF_MSG_LINK, "Internal link speed %d is"
					    " different than the external"
					    " link speed %d\n", new_line_speed,
					  vars->line_speed);
				vars->phy_link_up = 0;
				return 0;
			}
			REG_WR(bp, NIG_REG_EGRESS_DRAIN0_MODE
				    + params->port*4, 0);
			msleep(1);
		}
		vars->line_speed = new_line_speed;
		vars->link_status |= LINK_STATUS_SERDES_LINK;

		if ((params->req_line_speed == SPEED_AUTO_NEG) &&
		    ((XGXS_EXT_PHY_TYPE(params->ext_phy_config) ==
		     PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT) ||
		    (XGXS_EXT_PHY_TYPE(params->ext_phy_config) ==
		     PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705) ||
		    (XGXS_EXT_PHY_TYPE(params->ext_phy_config) ==
		     PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726))) {
			vars->autoneg = AUTO_NEG_ENABLED;

			if (gp_status & MDIO_AN_CL73_OR_37_COMPLETE) {
				vars->autoneg |= AUTO_NEG_COMPLETE;
				vars->link_status |=
					LINK_STATUS_AUTO_NEGOTIATE_COMPLETE;
			}

			vars->autoneg |= AUTO_NEG_PARALLEL_DETECTION_USED;
			vars->link_status |=
				LINK_STATUS_PARALLEL_DETECTION_USED;

		}
		if (vars->flow_ctrl & BNX2X_FLOW_CTRL_TX)
			vars->link_status |=
				LINK_STATUS_TX_FLOW_CONTROL_ENABLED;

		if (vars->flow_ctrl & BNX2X_FLOW_CTRL_RX)
			vars->link_status |=
				LINK_STATUS_RX_FLOW_CONTROL_ENABLED;

	} else { /* link_down */
		DP(NETIF_MSG_LINK, "phy link down\n");

		vars->phy_link_up = 0;

		vars->duplex = DUPLEX_FULL;
		vars->flow_ctrl = BNX2X_FLOW_CTRL_NONE;
		vars->autoneg = AUTO_NEG_DISABLED;
		vars->mac_type = MAC_TYPE_NONE;

		if ((params->req_line_speed == SPEED_AUTO_NEG) &&
		    ((XGXS_EXT_PHY_TYPE(params->ext_phy_config) ==
		     PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT))) {
			/* Check signal is detected */
			bnx2x_check_fallback_to_cl37(params);
		}
	}

	DP(NETIF_MSG_LINK, "gp_status 0x%x  phy_link_up %x line_speed %x \n",
		 gp_status, vars->phy_link_up, vars->line_speed);
	DP(NETIF_MSG_LINK, "duplex %x  flow_ctrl 0x%x"
		 " autoneg 0x%x\n",
		 vars->duplex,
		 vars->flow_ctrl, vars->autoneg);
	DP(NETIF_MSG_LINK, "link_status 0x%x\n", vars->link_status);

	return rc;
}

static void bnx2x_set_gmii_tx_driver(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u16 lp_up2;
	u16 tx_driver;
	u16 bank;

	/* read precomp */
	CL45_RD_OVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_OVER_1G,
			      MDIO_OVER_1G_LP_UP2, &lp_up2);

	/* bits [10:7] at lp_up2, positioned at [15:12] */
	lp_up2 = (((lp_up2 & MDIO_OVER_1G_LP_UP2_PREEMPHASIS_MASK) >>
		   MDIO_OVER_1G_LP_UP2_PREEMPHASIS_SHIFT) <<
		  MDIO_TX0_TX_DRIVER_PREEMPHASIS_SHIFT);

	if (lp_up2 == 0)
		return;

	for (bank = MDIO_REG_BANK_TX0; bank <= MDIO_REG_BANK_TX3;
	      bank += (MDIO_REG_BANK_TX1 - MDIO_REG_BANK_TX0)) {
		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
				      bank,
				      MDIO_TX0_TX_DRIVER, &tx_driver);

		/* replace tx_driver bits [15:12] */
		if (lp_up2 !=
		    (tx_driver & MDIO_TX0_TX_DRIVER_PREEMPHASIS_MASK)) {
			tx_driver &= ~MDIO_TX0_TX_DRIVER_PREEMPHASIS_MASK;
			tx_driver |= lp_up2;
			CL45_WR_OVER_CL22(bp, params->port,
					      params->phy_addr,
					      bank,
					      MDIO_TX0_TX_DRIVER, tx_driver);
		}
	}
}

static u8 bnx2x_emac_program(struct link_params *params,
			   u32 line_speed, u32 duplex)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u16 mode = 0;

	DP(NETIF_MSG_LINK, "setting link speed & duplex\n");
	bnx2x_bits_dis(bp, GRCBASE_EMAC0 + port*0x400 +
		     EMAC_REG_EMAC_MODE,
		     (EMAC_MODE_25G_MODE |
		     EMAC_MODE_PORT_MII_10M |
		     EMAC_MODE_HALF_DUPLEX));
	switch (line_speed) {
	case SPEED_10:
		mode |= EMAC_MODE_PORT_MII_10M;
		break;

	case SPEED_100:
		mode |= EMAC_MODE_PORT_MII;
		break;

	case SPEED_1000:
		mode |= EMAC_MODE_PORT_GMII;
		break;

	case SPEED_2500:
		mode |= (EMAC_MODE_25G_MODE | EMAC_MODE_PORT_GMII);
		break;

	default:
		/* 10G not valid for EMAC */
		DP(NETIF_MSG_LINK, "Invalid line_speed 0x%x\n", line_speed);
		return -EINVAL;
	}

	if (duplex == DUPLEX_HALF)
		mode |= EMAC_MODE_HALF_DUPLEX;
	bnx2x_bits_en(bp,
		    GRCBASE_EMAC0 + port*0x400 + EMAC_REG_EMAC_MODE,
		    mode);

	bnx2x_set_led(bp, params->port, LED_MODE_OPER,
		    line_speed, params->hw_led_mode, params->chip_id);
	return 0;
}

/*****************************************************************************/
/*      		     External Phy section       		     */
/*****************************************************************************/
void bnx2x_ext_phy_hw_reset(struct bnx2x *bp, u8 port)
{
	bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_1,
		       MISC_REGISTERS_GPIO_OUTPUT_LOW, port);
	msleep(1);
	bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_1,
		      MISC_REGISTERS_GPIO_OUTPUT_HIGH, port);
}

static void bnx2x_ext_phy_reset(struct link_params *params,
			      struct link_vars   *vars)
{
	struct bnx2x *bp = params->bp;
	u32 ext_phy_type;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);

	DP(NETIF_MSG_LINK, "Port %x: bnx2x_ext_phy_reset\n", params->port);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
	/* The PHY reset is controled by GPIO 1
	 * Give it 1ms of reset pulse
	 */
	if (vars->phy_flags & PHY_XGXS_FLAG) {

		switch (ext_phy_type) {
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT:
			DP(NETIF_MSG_LINK, "XGXS Direct\n");
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706:
			DP(NETIF_MSG_LINK, "XGXS 8705/8706\n");

			/* Restore normal power mode*/
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_2,
				      MISC_REGISTERS_GPIO_OUTPUT_HIGH,
					  params->port);

			/* HW reset */
			bnx2x_ext_phy_hw_reset(bp, params->port);

			bnx2x_cl45_write(bp, params->port,
				       ext_phy_type,
				       ext_phy_addr,
				       MDIO_PMA_DEVAD,
				       MDIO_PMA_REG_CTRL, 0xa040);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:

			/* Restore normal power mode*/
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_2,
					  MISC_REGISTERS_GPIO_OUTPUT_HIGH,
					  params->port);

			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_1,
					  MISC_REGISTERS_GPIO_OUTPUT_HIGH,
					  params->port);

			bnx2x_cl45_write(bp, params->port,
				       ext_phy_type,
				       ext_phy_addr,
				       MDIO_PMA_DEVAD,
				       MDIO_PMA_REG_CTRL,
				       1<<15);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
			DP(NETIF_MSG_LINK, "XGXS 8072\n");

			/* Unset Low Power Mode and SW reset */
			/* Restore normal power mode*/
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_2,
				      MISC_REGISTERS_GPIO_OUTPUT_HIGH,
					  params->port);

			bnx2x_cl45_write(bp, params->port,
				       ext_phy_type,
				       ext_phy_addr,
				       MDIO_PMA_DEVAD,
				       MDIO_PMA_REG_CTRL,
				       1<<15);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
			DP(NETIF_MSG_LINK, "XGXS 8073\n");

			/* Restore normal power mode*/
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_2,
				      MISC_REGISTERS_GPIO_OUTPUT_HIGH,
					  params->port);

			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_1,
				      MISC_REGISTERS_GPIO_OUTPUT_HIGH,
					  params->port);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
			DP(NETIF_MSG_LINK, "XGXS SFX7101\n");

			/* Restore normal power mode*/
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_2,
				      MISC_REGISTERS_GPIO_OUTPUT_HIGH,
					  params->port);

			/* HW reset */
			bnx2x_ext_phy_hw_reset(bp, params->port);
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481:
			/* Restore normal power mode*/
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_2,
				      MISC_REGISTERS_GPIO_OUTPUT_HIGH,
					  params->port);

			/* HW reset */
			bnx2x_ext_phy_hw_reset(bp, params->port);

			bnx2x_cl45_write(bp, params->port,
				       ext_phy_type,
				       ext_phy_addr,
				       MDIO_PMA_DEVAD,
				       MDIO_PMA_REG_CTRL,
				       1<<15);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE:
			DP(NETIF_MSG_LINK, "XGXS PHY Failure detected\n");
			break;

		default:
			DP(NETIF_MSG_LINK, "BAD XGXS ext_phy_config 0x%x\n",
			   params->ext_phy_config);
			break;
		}

	} else { /* SerDes */
		ext_phy_type = SERDES_EXT_PHY_TYPE(params->ext_phy_config);
		switch (ext_phy_type) {
		case PORT_HW_CFG_SERDES_EXT_PHY_TYPE_DIRECT:
			DP(NETIF_MSG_LINK, "SerDes Direct\n");
			break;

		case PORT_HW_CFG_SERDES_EXT_PHY_TYPE_BCM5482:
			DP(NETIF_MSG_LINK, "SerDes 5482\n");
			bnx2x_ext_phy_hw_reset(bp, params->port);
			break;

		default:
			DP(NETIF_MSG_LINK, "BAD SerDes ext_phy_config 0x%x\n",
				 params->ext_phy_config);
			break;
		}
	}
}

static void bnx2x_save_spirom_version(struct bnx2x *bp, u8 port,
				    u32 shmem_base, u32 spirom_ver)
{
	DP(NETIF_MSG_LINK, "FW version 0x%x:0x%x for port %d\n",
		 (u16)(spirom_ver>>16), (u16)spirom_ver, port);
	REG_WR(bp, shmem_base +
		   offsetof(struct shmem_region,
			    port_mb[port].ext_phy_fw_version),
			spirom_ver);
}

static void bnx2x_save_bcm_spirom_ver(struct bnx2x *bp, u8 port,
				    u32 ext_phy_type, u8 ext_phy_addr,
				    u32 shmem_base)
{
	u16 fw_ver1, fw_ver2;

	bnx2x_cl45_read(bp, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_ROM_VER1, &fw_ver1);
	bnx2x_cl45_read(bp, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_ROM_VER2, &fw_ver2);
	bnx2x_save_spirom_version(bp, port, shmem_base,
				(u32)(fw_ver1<<16 | fw_ver2));
}


static void bnx2x_save_8481_spirom_version(struct bnx2x *bp, u8 port,
					 u8 ext_phy_addr, u32 shmem_base)
{
	u16 val, fw_ver1, fw_ver2, cnt;
	/* For the 32 bits registers in 8481, access via MDIO2ARM interface.*/
	/* (1) set register 0xc200_0014(SPI_BRIDGE_CTRL_2) to 0x03000000 */
	bnx2x_cl45_write(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		       ext_phy_addr, MDIO_PMA_DEVAD,
		       0xA819, 0x0014);
	bnx2x_cl45_write(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       0xA81A,
		       0xc200);
	bnx2x_cl45_write(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       0xA81B,
		       0x0000);
	bnx2x_cl45_write(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       0xA81C,
		       0x0300);
	bnx2x_cl45_write(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       0xA817,
		       0x0009);

	for (cnt = 0; cnt < 100; cnt++) {
		bnx2x_cl45_read(bp, port,
			      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
			      ext_phy_addr,
			      MDIO_PMA_DEVAD,
			      0xA818,
			      &val);
		if (val & 1)
			break;
		udelay(5);
	}
	if (cnt == 100) {
		DP(NETIF_MSG_LINK, "Unable to read 8481 phy fw version(1)\n");
		bnx2x_save_spirom_version(bp, port,
					shmem_base, 0);
		return;
	}


	/* 2) read register 0xc200_0000 (SPI_FW_STATUS) */
	bnx2x_cl45_write(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		       ext_phy_addr, MDIO_PMA_DEVAD,
		       0xA819, 0x0000);
	bnx2x_cl45_write(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		       ext_phy_addr, MDIO_PMA_DEVAD,
		       0xA81A, 0xc200);
	bnx2x_cl45_write(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		       ext_phy_addr, MDIO_PMA_DEVAD,
		       0xA817, 0x000A);
	for (cnt = 0; cnt < 100; cnt++) {
		bnx2x_cl45_read(bp, port,
			      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
			      ext_phy_addr,
			      MDIO_PMA_DEVAD,
			      0xA818,
			      &val);
		if (val & 1)
			break;
		udelay(5);
	}
	if (cnt == 100) {
		DP(NETIF_MSG_LINK, "Unable to read 8481 phy fw version(2)\n");
		bnx2x_save_spirom_version(bp, port,
					shmem_base, 0);
		return;
	}

	/* lower 16 bits of the register SPI_FW_STATUS */
	bnx2x_cl45_read(bp, port,
		      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      0xA81B,
		      &fw_ver1);
	/* upper 16 bits of register SPI_FW_STATUS */
	bnx2x_cl45_read(bp, port,
		      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      0xA81C,
		      &fw_ver2);

	bnx2x_save_spirom_version(bp, port,
				shmem_base, (fw_ver2<<16) | fw_ver1);
}

static void bnx2x_bcm8072_external_rom_boot(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	u32 ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	/* Need to wait 200ms after reset */
	msleep(200);
	/* Boot port from external ROM
	 * Set ser_boot_ctl bit in the MISC_CTRL1 register
	 */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
			    MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_MISC_CTRL1, 0x0001);

	/* Reset internal microprocessor */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
			  MDIO_PMA_DEVAD,
			  MDIO_PMA_REG_GEN_CTRL,
			  MDIO_PMA_REG_GEN_CTRL_ROM_RESET_INTERNAL_MP);
	/* set micro reset = 0 */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
			    MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_GEN_CTRL,
			    MDIO_PMA_REG_GEN_CTRL_ROM_MICRO_RESET);
	/* Reset internal microprocessor */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
			  MDIO_PMA_DEVAD,
			  MDIO_PMA_REG_GEN_CTRL,
			  MDIO_PMA_REG_GEN_CTRL_ROM_RESET_INTERNAL_MP);
	/* wait for 100ms for code download via SPI port */
	msleep(100);

	/* Clear ser_boot_ctl bit */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
			    MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_MISC_CTRL1, 0x0000);
	/* Wait 100ms */
	msleep(100);

	bnx2x_save_bcm_spirom_ver(bp, port,
				ext_phy_type,
				ext_phy_addr,
				params->shmem_base);
}

static u8 bnx2x_8073_is_snr_needed(struct link_params *params)
{
	/* This is only required for 8073A1, version 102 only */

	struct bnx2x *bp = params->bp;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	u16 val;

	/* Read 8073 HW revision*/
	bnx2x_cl45_read(bp, params->port,
		      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_8073_CHIP_REV, &val);

	if (val != 1) {
		/* No need to workaround in 8073 A1 */
		return 0;
	}

	bnx2x_cl45_read(bp, params->port,
		      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_ROM_VER2, &val);

	/* SNR should be applied only for version 0x102 */
	if (val != 0x102)
		return 0;

	return 1;
}

static u8 bnx2x_bcm8073_xaui_wa(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	u16 val, cnt, cnt1 ;

	bnx2x_cl45_read(bp, params->port,
		      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_8073_CHIP_REV, &val);

	if (val > 0) {
		/* No need to workaround in 8073 A1 */
		return 0;
	}
	/* XAUI workaround in 8073 A0: */

	/* After loading the boot ROM and restarting Autoneg,
	poll Dev1, Reg $C820: */

	for (cnt = 0; cnt < 1000; cnt++) {
		bnx2x_cl45_read(bp, params->port,
			      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
			      ext_phy_addr,
			      MDIO_PMA_DEVAD,
			      MDIO_PMA_REG_8073_SPEED_LINK_STATUS,
			      &val);
		  /* If bit [14] = 0 or bit [13] = 0, continue on with
		   system initialization (XAUI work-around not required,
		    as these bits indicate 2.5G or 1G link up). */
		if (!(val & (1<<14)) || !(val & (1<<13))) {
			DP(NETIF_MSG_LINK, "XAUI work-around not required\n");
			return 0;
		} else if (!(val & (1<<15))) {
			DP(NETIF_MSG_LINK, "clc bit 15 went off\n");
			 /* If bit 15 is 0, then poll Dev1, Reg $C841 until
			  it's MSB (bit 15) goes to 1 (indicating that the
			  XAUI workaround has completed),
			  then continue on with system initialization.*/
			for (cnt1 = 0; cnt1 < 1000; cnt1++) {
				bnx2x_cl45_read(bp, params->port,
					PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
					ext_phy_addr,
					MDIO_PMA_DEVAD,
					MDIO_PMA_REG_8073_XAUI_WA, &val);
				if (val & (1<<15)) {
					DP(NETIF_MSG_LINK,
					  "XAUI workaround has completed\n");
					return 0;
				 }
				 msleep(3);
			}
			break;
		}
		msleep(3);
	}
	DP(NETIF_MSG_LINK, "Warning: XAUI work-around timeout !!!\n");
	return -EINVAL;
}

static void bnx2x_bcm8073_bcm8727_external_rom_boot(struct bnx2x *bp, u8 port,
						  u8 ext_phy_addr,
						  u32 ext_phy_type,
						  u32 shmem_base)
{
	/* Boot port from external ROM  */
	/* EDC grst */
	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_GEN_CTRL,
		       0x0001);

	/* ucode reboot and rst */
	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_GEN_CTRL,
		       0x008c);

	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_MISC_CTRL1, 0x0001);

	/* Reset internal microprocessor */
	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_GEN_CTRL,
		       MDIO_PMA_REG_GEN_CTRL_ROM_MICRO_RESET);

	/* Release srst bit */
	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_GEN_CTRL,
		       MDIO_PMA_REG_GEN_CTRL_ROM_RESET_INTERNAL_MP);

	/* wait for 100ms for code download via SPI port */
	msleep(100);

	/* Clear ser_boot_ctl bit */
	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_MISC_CTRL1, 0x0000);

	bnx2x_save_bcm_spirom_ver(bp, port,
				ext_phy_type,
				ext_phy_addr,
				shmem_base);
}

static void bnx2x_bcm8073_external_rom_boot(struct bnx2x *bp, u8 port,
					  u8 ext_phy_addr,
					  u32 shmem_base)
{
	bnx2x_bcm8073_bcm8727_external_rom_boot(bp, port, ext_phy_addr,
					 PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
					 shmem_base);
}

static void bnx2x_bcm8727_external_rom_boot(struct bnx2x *bp, u8 port,
					  u8 ext_phy_addr,
					  u32 shmem_base)
{
	bnx2x_bcm8073_bcm8727_external_rom_boot(bp, port, ext_phy_addr,
					 PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
					 shmem_base);

}

static void bnx2x_bcm8726_external_rom_boot(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	u32 ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	/* Need to wait 100ms after reset */
	msleep(100);

	/* Set serial boot control for external load */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_MISC_CTRL1, 0x0001);

	/* Micro controller re-boot */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_GEN_CTRL,
		       MDIO_PMA_REG_GEN_CTRL_ROM_RESET_INTERNAL_MP);

	/* Set soft reset */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_GEN_CTRL,
		       MDIO_PMA_REG_GEN_CTRL_ROM_MICRO_RESET);

	/* Set PLL register value to be same like in P13 ver */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_PLL_CTRL,
		       0x73A0);

	/* Clear soft reset.
	Will automatically reset micro-controller re-boot */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_GEN_CTRL,
		       MDIO_PMA_REG_GEN_CTRL_ROM_RESET_INTERNAL_MP);

	/* wait for 150ms for microcode load */
	msleep(150);

	/* Disable serial boot control, tristates pins SS_N, SCK, MOSI, MISO */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_MISC_CTRL1, 0x0000);

	msleep(200);
	bnx2x_save_bcm_spirom_ver(bp, port,
				ext_phy_type,
				ext_phy_addr,
				params->shmem_base);
}

static void bnx2x_sfp_set_transmitter(struct bnx2x *bp, u8 port,
				    u32 ext_phy_type, u8 ext_phy_addr,
				    u8 tx_en)
{
	u16 val;

	DP(NETIF_MSG_LINK, "Setting transmitter tx_en=%x for port %x\n",
		 tx_en, port);
	/* Disable/Enable transmitter ( TX laser of the SFP+ module.)*/
	bnx2x_cl45_read(bp, port,
		      ext_phy_type,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_PHY_IDENTIFIER,
		      &val);

	if (tx_en)
		val &= ~(1<<15);
	else
		val |= (1<<15);

	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_PHY_IDENTIFIER,
		       val);
}

static u8 bnx2x_8726_read_sfp_module_eeprom(struct link_params *params,
					  u16 addr, u8 byte_cnt, u8 *o_buf)
{
	struct bnx2x *bp = params->bp;
	u16 val = 0;
	u16 i;
	u8 port = params->port;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	u32 ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	if (byte_cnt > 16) {
		DP(NETIF_MSG_LINK, "Reading from eeprom is"
			    " is limited to 0xf\n");
		return -EINVAL;
	}
	/* Set the read command byte count */
	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_SFP_TWO_WIRE_BYTE_CNT,
		       (byte_cnt | 0xa000));

	/* Set the read command address */
	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_SFP_TWO_WIRE_MEM_ADDR,
		       addr);

	/* Activate read command */
	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_SFP_TWO_WIRE_CTRL,
		       0x2c0f);

	/* Wait up to 500us for command complete status */
	for (i = 0; i < 100; i++) {
		bnx2x_cl45_read(bp, port,
			      ext_phy_type,
			      ext_phy_addr,
			      MDIO_PMA_DEVAD,
			      MDIO_PMA_REG_SFP_TWO_WIRE_CTRL, &val);
		if ((val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK) ==
		    MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_COMPLETE)
			break;
		udelay(5);
	}

	if ((val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK) !=
		    MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_COMPLETE) {
		DP(NETIF_MSG_LINK,
			 "Got bad status 0x%x when reading from SFP+ EEPROM\n",
			 (val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK));
		return -EINVAL;
	}

	/* Read the buffer */
	for (i = 0; i < byte_cnt; i++) {
		bnx2x_cl45_read(bp, port,
			      ext_phy_type,
			      ext_phy_addr,
			      MDIO_PMA_DEVAD,
			      MDIO_PMA_REG_8726_TWO_WIRE_DATA_BUF + i, &val);
		o_buf[i] = (u8)(val & MDIO_PMA_REG_8726_TWO_WIRE_DATA_MASK);
	}

	for (i = 0; i < 100; i++) {
		bnx2x_cl45_read(bp, port,
			      ext_phy_type,
			      ext_phy_addr,
			      MDIO_PMA_DEVAD,
			      MDIO_PMA_REG_SFP_TWO_WIRE_CTRL, &val);
		if ((val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK) ==
		    MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_IDLE)
			return 0;;
		msleep(1);
	}
	return -EINVAL;
}

static u8 bnx2x_8727_read_sfp_module_eeprom(struct link_params *params,
					  u16 addr, u8 byte_cnt, u8 *o_buf)
{
	struct bnx2x *bp = params->bp;
	u16 val, i;
	u8 port = params->port;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	u32 ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	if (byte_cnt > 16) {
		DP(NETIF_MSG_LINK, "Reading from eeprom is"
			    " is limited to 0xf\n");
		return -EINVAL;
	}

	/* Need to read from 1.8000 to clear it */
	bnx2x_cl45_read(bp, port,
		      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_SFP_TWO_WIRE_CTRL,
		      &val);

	/* Set the read command byte count */
	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_SFP_TWO_WIRE_BYTE_CNT,
		       ((byte_cnt < 2) ? 2 : byte_cnt));

	/* Set the read command address */
	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_SFP_TWO_WIRE_MEM_ADDR,
		       addr);
	/* Set the destination address */
	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       0x8004,
		       MDIO_PMA_REG_8727_TWO_WIRE_DATA_BUF);

	/* Activate read command */
	bnx2x_cl45_write(bp, port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_SFP_TWO_WIRE_CTRL,
		       0x8002);
	/* Wait appropriate time for two-wire command to finish before
	polling the status register */
	msleep(1);

	/* Wait up to 500us for command complete status */
	for (i = 0; i < 100; i++) {
		bnx2x_cl45_read(bp, port,
			      ext_phy_type,
			      ext_phy_addr,
			      MDIO_PMA_DEVAD,
			      MDIO_PMA_REG_SFP_TWO_WIRE_CTRL, &val);
		if ((val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK) ==
		    MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_COMPLETE)
			break;
		udelay(5);
	}

	if ((val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK) !=
		    MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_COMPLETE) {
		DP(NETIF_MSG_LINK,
			 "Got bad status 0x%x when reading from SFP+ EEPROM\n",
			 (val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK));
		return -EINVAL;
	}

	/* Read the buffer */
	for (i = 0; i < byte_cnt; i++) {
		bnx2x_cl45_read(bp, port,
			      ext_phy_type,
			      ext_phy_addr,
			      MDIO_PMA_DEVAD,
			      MDIO_PMA_REG_8727_TWO_WIRE_DATA_BUF + i, &val);
		o_buf[i] = (u8)(val & MDIO_PMA_REG_8727_TWO_WIRE_DATA_MASK);
	}

	for (i = 0; i < 100; i++) {
		bnx2x_cl45_read(bp, port,
			      ext_phy_type,
			      ext_phy_addr,
			      MDIO_PMA_DEVAD,
			      MDIO_PMA_REG_SFP_TWO_WIRE_CTRL, &val);
		if ((val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK) ==
		    MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_IDLE)
			return 0;;
		msleep(1);
	}

	return -EINVAL;
}

u8 bnx2x_read_sfp_module_eeprom(struct link_params *params, u16 addr,
				     u8 byte_cnt, u8 *o_buf)
{
	u32 ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	if (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726)
		return bnx2x_8726_read_sfp_module_eeprom(params, addr,
						       byte_cnt, o_buf);
	else if (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727)
		return bnx2x_8727_read_sfp_module_eeprom(params, addr,
						       byte_cnt, o_buf);
	return -EINVAL;
}

static u8 bnx2x_get_edc_mode(struct link_params *params,
				  u16 *edc_mode)
{
	struct bnx2x *bp = params->bp;
	u8 val, check_limiting_mode = 0;
	*edc_mode = EDC_MODE_LIMITING;

	/* First check for copper cable */
	if (bnx2x_read_sfp_module_eeprom(params,
				       SFP_EEPROM_CON_TYPE_ADDR,
				       1,
				       &val) != 0) {
		DP(NETIF_MSG_LINK, "Failed to read from SFP+ module EEPROM\n");
		return -EINVAL;
	}

	switch (val) {
	case SFP_EEPROM_CON_TYPE_VAL_COPPER:
	{
		u8 copper_module_type;

		/* Check if its active cable( includes SFP+ module)
		of passive cable*/
		if (bnx2x_read_sfp_module_eeprom(params,
					       SFP_EEPROM_FC_TX_TECH_ADDR,
					       1,
					       &copper_module_type) !=
		    0) {
			DP(NETIF_MSG_LINK,
				"Failed to read copper-cable-type"
				" from SFP+ EEPROM\n");
			return -EINVAL;
		}

		if (copper_module_type &
		    SFP_EEPROM_FC_TX_TECH_BITMASK_COPPER_ACTIVE) {
			DP(NETIF_MSG_LINK, "Active Copper cable detected\n");
			check_limiting_mode = 1;
		} else if (copper_module_type &
			SFP_EEPROM_FC_TX_TECH_BITMASK_COPPER_PASSIVE) {
				DP(NETIF_MSG_LINK, "Passive Copper"
					    " cable detected\n");
				*edc_mode =
				      EDC_MODE_PASSIVE_DAC;
		} else {
			DP(NETIF_MSG_LINK, "Unknown copper-cable-"
				     "type 0x%x !!!\n", copper_module_type);
			return -EINVAL;
		}
		break;
	}
	case SFP_EEPROM_CON_TYPE_VAL_LC:
		DP(NETIF_MSG_LINK, "Optic module detected\n");
		check_limiting_mode = 1;
		break;
	default:
		DP(NETIF_MSG_LINK, "Unable to determine module type 0x%x !!!\n",
			 val);
		return -EINVAL;
	}

	if (check_limiting_mode) {
		u8 options[SFP_EEPROM_OPTIONS_SIZE];
		if (bnx2x_read_sfp_module_eeprom(params,
					       SFP_EEPROM_OPTIONS_ADDR,
					       SFP_EEPROM_OPTIONS_SIZE,
					       options) != 0) {
			DP(NETIF_MSG_LINK, "Failed to read Option"
				" field from module EEPROM\n");
			return -EINVAL;
		}
		if ((options[0] & SFP_EEPROM_OPTIONS_LINEAR_RX_OUT_MASK))
			*edc_mode = EDC_MODE_LINEAR;
		else
			*edc_mode = EDC_MODE_LIMITING;
	}
	DP(NETIF_MSG_LINK, "EDC mode is set to 0x%x\n", *edc_mode);
	return 0;
}

/* This function read the relevant field from the module ( SFP+ ),
	and verify it is compliant with this board */
static u8 bnx2x_verify_sfp_module(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u32 val;
	u32 fw_resp;
	char vendor_name[SFP_EEPROM_VENDOR_NAME_SIZE+1];
	char vendor_pn[SFP_EEPROM_PART_NO_SIZE+1];

	val = REG_RD(bp, params->shmem_base +
			 offsetof(struct shmem_region, dev_info.
				  port_feature_config[params->port].config));
	if ((val & PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_MASK) ==
	    PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_NO_ENFORCEMENT) {
		DP(NETIF_MSG_LINK, "NOT enforcing module verification\n");
		return 0;
	}

	/* Ask the FW to validate the module */
	if (!(params->feature_config_flags &
	      FEATURE_CONFIG_BC_SUPPORTS_OPT_MDL_VRFY)) {
		DP(NETIF_MSG_LINK, "FW does not support OPT MDL "
			    "verification\n");
		return -EINVAL;
	}

	fw_resp = bnx2x_fw_command(bp, DRV_MSG_CODE_VRFY_OPT_MDL);
	if (fw_resp == FW_MSG_CODE_VRFY_OPT_MDL_SUCCESS) {
		DP(NETIF_MSG_LINK, "Approved module\n");
		return 0;
	}

	/* format the warning message */
	if (bnx2x_read_sfp_module_eeprom(params,
				       SFP_EEPROM_VENDOR_NAME_ADDR,
				       SFP_EEPROM_VENDOR_NAME_SIZE,
				       (u8 *)vendor_name))
		vendor_name[0] = '\0';
	else
		vendor_name[SFP_EEPROM_VENDOR_NAME_SIZE] = '\0';
	if (bnx2x_read_sfp_module_eeprom(params,
				       SFP_EEPROM_PART_NO_ADDR,
				       SFP_EEPROM_PART_NO_SIZE,
				       (u8 *)vendor_pn))
		vendor_pn[0] = '\0';
	else
		vendor_pn[SFP_EEPROM_PART_NO_SIZE] = '\0';

	printk(KERN_INFO PFX  "Warning: "
			 "Unqualified SFP+ module "
			 "detected on %s, Port %d from %s part number %s\n"
			, bp->dev->name, params->port,
			vendor_name, vendor_pn);
	return -EINVAL;
}

static u8 bnx2x_bcm8726_set_limiting_mode(struct link_params *params,
					u16 edc_mode)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	u16 cur_limiting_mode;

	bnx2x_cl45_read(bp, port,
		      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_ROM_VER2,
		      &cur_limiting_mode);
	DP(NETIF_MSG_LINK, "Current Limiting mode is 0x%x\n",
		 cur_limiting_mode);

	if (edc_mode == EDC_MODE_LIMITING) {
		DP(NETIF_MSG_LINK,
			 "Setting LIMITING MODE\n");
		bnx2x_cl45_write(bp, port,
			       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
			       ext_phy_addr,
			       MDIO_PMA_DEVAD,
			       MDIO_PMA_REG_ROM_VER2,
			       EDC_MODE_LIMITING);
	} else { /* LRM mode ( default )*/

		DP(NETIF_MSG_LINK, "Setting LRM MODE\n");

		/* Changing to LRM mode takes quite few seconds.
		So do it only if current mode is limiting
		( default is LRM )*/
		if (cur_limiting_mode != EDC_MODE_LIMITING)
			return 0;

		bnx2x_cl45_write(bp, port,
			       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
			       ext_phy_addr,
			       MDIO_PMA_DEVAD,
			       MDIO_PMA_REG_LRM_MODE,
			       0);
		bnx2x_cl45_write(bp, port,
			       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
			       ext_phy_addr,
			       MDIO_PMA_DEVAD,
			       MDIO_PMA_REG_ROM_VER2,
			       0x128);
		bnx2x_cl45_write(bp, port,
			       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
			       ext_phy_addr,
			       MDIO_PMA_DEVAD,
			       MDIO_PMA_REG_MISC_CTRL0,
			       0x4008);
		bnx2x_cl45_write(bp, port,
			       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
			       ext_phy_addr,
			       MDIO_PMA_DEVAD,
			       MDIO_PMA_REG_LRM_MODE,
			       0xaaaa);
	}
	return 0;
}

static u8 bnx2x_bcm8727_set_limiting_mode(struct link_params *params,
					u16 edc_mode)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u16 phy_identifier;
	u16 rom_ver2_val;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);

	bnx2x_cl45_read(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_PHY_IDENTIFIER,
		       &phy_identifier);

	bnx2x_cl45_write(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_PHY_IDENTIFIER,
		       (phy_identifier & ~(1<<9)));

	bnx2x_cl45_read(bp, port,
		      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_ROM_VER2,
		      &rom_ver2_val);
	/* Keep the MSB 8-bits, and set the LSB 8-bits with the edc_mode */
	bnx2x_cl45_write(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_ROM_VER2,
		       (rom_ver2_val & 0xff00) | (edc_mode & 0x00ff));

	bnx2x_cl45_write(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_PHY_IDENTIFIER,
		       (phy_identifier | (1<<9)));

	return 0;
}


static u8 bnx2x_wait_for_sfp_module_initialized(struct link_params *params)
{
	u8 val;
	struct bnx2x *bp = params->bp;
	u16 timeout;
	/* Initialization time after hot-plug may take up to 300ms for some
	phys type ( e.g. JDSU ) */
	for (timeout = 0; timeout < 60; timeout++) {
		if (bnx2x_read_sfp_module_eeprom(params, 1, 1, &val)
		    == 0) {
			DP(NETIF_MSG_LINK, "SFP+ module initialization "
				     "took %d ms\n", timeout * 5);
			return 0;
		}
		msleep(5);
	}
	return -EINVAL;
}

static void bnx2x_8727_power_module(struct bnx2x *bp,
				  struct link_params *params,
				  u8 ext_phy_addr, u8 is_power_up) {
	/* Make sure GPIOs are not using for LED mode */
	u16 val;
	u8 port = params->port;
	/*
	 * In the GPIO register, bit 4 is use to detemine if the GPIOs are
	 * operating as INPUT or as OUTPUT. Bit 1 is for input, and 0 for
	 * output
	 * Bits 0-1 determine the gpios value for OUTPUT in case bit 4 val is 0
	 * Bits 8-9 determine the gpios value for INPUT in case bit 4 val is 1
	 * where the 1st bit is the over-current(only input), and 2nd bit is
	 * for power( only output )
	*/

	/*
	 * In case of NOC feature is disabled and power is up, set GPIO control
	 *  as input to enable listening of over-current indication
	 */

	if (!(params->feature_config_flags &
	      FEATURE_CONFIG_BCM8727_NOC) && is_power_up)
		val = (1<<4);
	else
		/*
		 * Set GPIO control to OUTPUT, and set the power bit
		 * to according to the is_power_up
		 */
		val = ((!(is_power_up)) << 1);

	bnx2x_cl45_write(bp, port,
		       PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_8727_GPIO_CTRL,
		       val);
}

static u8 bnx2x_sfp_module_detection(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u16 edc_mode;
	u8 rc = 0;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	u32 ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
	u32 val = REG_RD(bp, params->shmem_base +
			     offsetof(struct shmem_region, dev_info.
				     port_feature_config[params->port].config));

	DP(NETIF_MSG_LINK, "SFP+ module plugged in/out detected on port %d\n",
		 params->port);

	if (bnx2x_get_edc_mode(params, &edc_mode) != 0) {
		DP(NETIF_MSG_LINK, "Failed to get valid module type\n");
		return -EINVAL;
	} else if (bnx2x_verify_sfp_module(params) !=
		   0) {
		/* check SFP+ module compatibility */
		DP(NETIF_MSG_LINK, "Module verification failed!!\n");
		rc = -EINVAL;
		/* Turn on fault module-detected led */
		bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_0,
				  MISC_REGISTERS_GPIO_HIGH,
				  params->port);
		if ((ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727) &&
		    ((val & PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_MASK) ==
		     PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_POWER_DOWN)) {
			/* Shutdown SFP+ module */
			DP(NETIF_MSG_LINK, "Shutdown SFP+ module!!\n");
			bnx2x_8727_power_module(bp, params,
					      ext_phy_addr, 0);
			return rc;
		}
	} else {
		/* Turn off fault module-detected led */
		DP(NETIF_MSG_LINK, "Turn off fault module-detected led\n");
		bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_0,
					  MISC_REGISTERS_GPIO_LOW,
					  params->port);
	}

	/* power up the SFP module */
	if (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727)
		bnx2x_8727_power_module(bp, params, ext_phy_addr, 1);

	/* Check and set limiting mode / LRM mode on 8726.
	On 8727 it is done automatically */
	if (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726)
		bnx2x_bcm8726_set_limiting_mode(params, edc_mode);
	else
		bnx2x_bcm8727_set_limiting_mode(params, edc_mode);
	/*
	 * Enable transmit for this module if the module is approved, or
	 * if unapproved modules should also enable the Tx laser
	 */
	if (rc == 0 ||
	    (val & PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_MASK) !=
	    PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_DISABLE_TX_LASER)
		bnx2x_sfp_set_transmitter(bp, params->port,
					ext_phy_type, ext_phy_addr, 1);
	else
		bnx2x_sfp_set_transmitter(bp, params->port,
					ext_phy_type, ext_phy_addr, 0);

	return rc;
}

void bnx2x_handle_module_detect_int(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u32 gpio_val;
	u8 port = params->port;

	/* Set valid module led off */
	bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_0,
			  MISC_REGISTERS_GPIO_HIGH,
			  params->port);

	/* Get current gpio val refelecting module plugged in / out*/
	gpio_val = bnx2x_get_gpio(bp,  MISC_REGISTERS_GPIO_3, port);

	/* Call the handling function in case module is detected */
	if (gpio_val == 0) {

		bnx2x_set_gpio_int(bp, MISC_REGISTERS_GPIO_3,
				      MISC_REGISTERS_GPIO_INT_OUTPUT_CLR,
				      port);

		if (bnx2x_wait_for_sfp_module_initialized(params) ==
		    0)
			bnx2x_sfp_module_detection(params);
		else
			DP(NETIF_MSG_LINK, "SFP+ module is not initialized\n");
	} else {
		u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);

		u32 ext_phy_type =
			XGXS_EXT_PHY_TYPE(params->ext_phy_config);
		u32 val = REG_RD(bp, params->shmem_base +
				     offsetof(struct shmem_region, dev_info.
					      port_feature_config[params->port].
					      config));

		bnx2x_set_gpio_int(bp, MISC_REGISTERS_GPIO_3,
				      MISC_REGISTERS_GPIO_INT_OUTPUT_SET,
				      port);
		/* Module was plugged out. */
		/* Disable transmit for this module */
		if ((val & PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_MASK) ==
		    PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_DISABLE_TX_LASER)
			bnx2x_sfp_set_transmitter(bp, params->port,
						ext_phy_type, ext_phy_addr, 0);
	}
}

static void bnx2x_bcm807x_force_10G(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	u32 ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	/* Force KR or KX */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_CTRL,
		       0x2040);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_10G_CTRL2,
		       0x000b);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_BCM_CTRL,
		       0x0000);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_AN_DEVAD,
		       MDIO_AN_REG_CTRL,
		       0x0000);
}

static void bnx2x_bcm8073_set_xaui_low_power_mode(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u16 val;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	u32 ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	bnx2x_cl45_read(bp, params->port,
		      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_8073_CHIP_REV, &val);

	if (val == 0) {
		/* Mustn't set low power mode in 8073 A0 */
		return;
	}

	/* Disable PLL sequencer (use read-modify-write to clear bit 13) */
	bnx2x_cl45_read(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD,
		       MDIO_XS_PLL_SEQUENCER, &val);
	val &= ~(1<<13);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, MDIO_XS_PLL_SEQUENCER, val);

	/* PLL controls */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, 0x805E, 0x1077);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, 0x805D, 0x0000);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, 0x805C, 0x030B);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, 0x805B, 0x1240);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, 0x805A, 0x2490);

	/* Tx Controls */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, 0x80A7, 0x0C74);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, 0x80A6, 0x9041);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, 0x80A5, 0x4640);

	/* Rx Controls */
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, 0x80FE, 0x01C4);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, 0x80FD, 0x9249);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, 0x80FC, 0x2015);

	/* Enable PLL sequencer  (use read-modify-write to set bit 13) */
	bnx2x_cl45_read(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD,
		       MDIO_XS_PLL_SEQUENCER, &val);
	val |= (1<<13);
	bnx2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
		       MDIO_XS_DEVAD, MDIO_XS_PLL_SEQUENCER, val);
}

static void bnx2x_8073_set_pause_cl37(struct link_params *params,
				  struct link_vars *vars)
{
	struct bnx2x *bp = params->bp;
	u16 cl37_val;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	u32 ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	bnx2x_cl45_read(bp, params->port,
		      ext_phy_type,
		      ext_phy_addr,
		      MDIO_AN_DEVAD,
		      MDIO_AN_REG_CL37_FC_LD, &cl37_val);

	cl37_val &= ~MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH;
	/* Please refer to Table 28B-3 of 802.3ab-1999 spec. */

	if ((vars->ieee_fc &
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_SYMMETRIC) ==
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_SYMMETRIC) {
		cl37_val |=  MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_SYMMETRIC;
	}
	if ((vars->ieee_fc &
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC) ==
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC) {
		cl37_val |=  MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC;
	}
	if ((vars->ieee_fc &
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH) ==
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH) {
		cl37_val |= MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH;
	}
	DP(NETIF_MSG_LINK,
		 "Ext phy AN advertize cl37 0x%x\n", cl37_val);

	bnx2x_cl45_write(bp, params->port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_AN_DEVAD,
		       MDIO_AN_REG_CL37_FC_LD, cl37_val);
	msleep(500);
}

static void bnx2x_ext_phy_set_pause(struct link_params *params,
				  struct link_vars *vars)
{
	struct bnx2x *bp = params->bp;
	u16 val;
	u8 ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	u32 ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	/* read modify write pause advertizing */
	bnx2x_cl45_read(bp, params->port,
		      ext_phy_type,
		      ext_phy_addr,
		      MDIO_AN_DEVAD,
		      MDIO_AN_REG_ADV_PAUSE, &val);

	val &= ~MDIO_AN_REG_ADV_PAUSE_BOTH;

	/* Please refer to Table 28B-3 of 802.3ab-1999 spec. */

	if ((vars->ieee_fc &
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC) ==
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC) {
		val |=  MDIO_AN_REG_ADV_PAUSE_ASYMMETRIC;
	}
	if ((vars->ieee_fc &
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH) ==
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH) {
		val |=
		 MDIO_AN_REG_ADV_PAUSE_PAUSE;
	}
	DP(NETIF_MSG_LINK,
		 "Ext phy AN advertize 0x%x\n", val);
	bnx2x_cl45_write(bp, params->port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_AN_DEVAD,
		       MDIO_AN_REG_ADV_PAUSE, val);
}
static void bnx2x_set_preemphasis(struct link_params *params)
{
	u16 bank, i = 0;
	struct bnx2x *bp = params->bp;

	for (bank = MDIO_REG_BANK_RX0, i = 0; bank <= MDIO_REG_BANK_RX3;
	      bank += (MDIO_REG_BANK_RX1-MDIO_REG_BANK_RX0), i++) {
			CL45_WR_OVER_CL22(bp, params->port,
					      params->phy_addr,
					      bank,
					      MDIO_RX0_RX_EQ_BOOST,
					      params->xgxs_config_rx[i]);
	}

	for (bank = MDIO_REG_BANK_TX0, i = 0; bank <= MDIO_REG_BANK_TX3;
		      bank += (MDIO_REG_BANK_TX1 - MDIO_REG_BANK_TX0), i++) {
			CL45_WR_OVER_CL22(bp, params->port,
					      params->phy_addr,
					      bank,
					      MDIO_TX0_TX_DRIVER,
					      params->xgxs_config_tx[i]);
	}
}


static void bnx2x_8481_set_led4(struct link_params *params,
			      u32 ext_phy_type, u8 ext_phy_addr)
{
	struct bnx2x *bp = params->bp;

	/* PHYC_CTL_LED_CTL */
	bnx2x_cl45_write(bp, params->port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_8481_LINK_SIGNAL, 0xa482);

	/* Unmask LED4 for 10G link */
	bnx2x_cl45_write(bp, params->port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_8481_SIGNAL_MASK, (1<<6));
	/* 'Interrupt Mask' */
	bnx2x_cl45_write(bp, params->port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_AN_DEVAD,
		       0xFFFB, 0xFFFD);
}
static void bnx2x_8481_set_legacy_led_mode(struct link_params *params,
					 u32 ext_phy_type, u8 ext_phy_addr)
{
	struct bnx2x *bp = params->bp;

	/* LED1 (10G Link): Disable LED1 when 10/100/1000 link */
	/* LED2 (1G/100/10 Link): Enable LED2 when 10/100/1000 link) */
	bnx2x_cl45_write(bp, params->port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_AN_DEVAD,
		       MDIO_AN_REG_8481_LEGACY_SHADOW,
		       (1<<15) | (0xd << 10) | (0xc<<4) | 0xe);
}

static void bnx2x_8481_set_10G_led_mode(struct link_params *params,
				      u32 ext_phy_type, u8 ext_phy_addr)
{
	struct bnx2x *bp = params->bp;
	u16 val1;

	/* LED1 (10G Link) */
	/* Enable continuse based on source 7(10G-link) */
	bnx2x_cl45_read(bp, params->port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_8481_LINK_SIGNAL,
		       &val1);
	/* Set bit 2 to 0, and bits [1:0] to 10 */
	val1 &= ~((1<<0) | (1<<2)); /* Clear bits 0,2*/
	val1 |= (1<<1); /* Set bit 1 */

	bnx2x_cl45_write(bp, params->port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_8481_LINK_SIGNAL,
		       val1);

	/* Unmask LED1 for 10G link */
	bnx2x_cl45_read(bp, params->port,
		      ext_phy_type,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_8481_LED1_MASK,
		      &val1);
	/* Set bit 2 to 0, and bits [1:0] to 10 */
	val1 |= (1<<7);
	bnx2x_cl45_write(bp, params->port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_8481_LED1_MASK,
		       val1);

	/* LED2 (1G/100/10G Link) */
	/* Mask LED2 for 10G link */
	bnx2x_cl45_write(bp, params->port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_8481_LED2_MASK,
		       0);

	/* LED3 (10G/1G/100/10G Activity) */
	bnx2x_cl45_read(bp, params->port,
		      ext_phy_type,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_8481_LINK_SIGNAL,
		      &val1);
	/* Enable blink based on source 4(Activity) */
	val1 &= ~((1<<7) | (1<<8)); /* Clear bits 7,8 */
	val1 |= (1<<6); /* Set only bit 6 */
	bnx2x_cl45_write(bp, params->port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_8481_LINK_SIGNAL,
		       val1);

	bnx2x_cl45_read(bp, params->port,
		      ext_phy_type,
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_8481_LED3_MASK,
		      &val1);
	val1 |= (1<<4); /* Unmask LED3 for 10G link */
	bnx2x_cl45_write(bp, params->port,
		       ext_phy_type,
		       ext_phy_addr,
		       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_8481_LED3_MASK,
		       val1);
}


static void bnx2x_init_internal_phy(struct link_params *params,
				  struct link_vars *vars,
				  u8 enable_cl73)
{
	struct bnx2x *bp = params->bp;

	if (!(vars->phy_flags & PHY_SGMII_FLAG)) {
		if ((XGXS_EXT_PHY_TYPE(params->ext_phy_config) ==
		     PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT) &&
		    (params->feature_config_flags &
		     FEATURE_CONFIG_OVERRIDE_PREEMPHASIS_ENABLED))
			bnx2x_set_preemphasis(params);

		/* forced speed requested? */
		if (vars->line_speed != SPEED_AUTO_NEG) {
			DP(NETIF_MSG_LINK, "not SGMII, no AN\n");

			/* disable autoneg */
			bnx2x_set_autoneg(params, vars, 0);

			/* program speed and duplex */
			bnx2x_program_serdes(params, vars);

		} else { /* AN_mode */
			DP(NETIF_MSG_LINK, "not SGMII, AN\n");

			/* AN enabled */
			bnx2x_set_brcm_cl37_advertisment(params);

			/* program duplex & pause advertisement (for aneg) */
			bnx2x_set_ieee_aneg_advertisment(params,
						       vars->ieee_fc);

			/* enable autoneg */09 Bbnx2x_set_rporati(params,Copyr,adcom C_cl73-2009 Broadcom Cornd restart ANon
 *
 * Unlecense
 you and Broadcom e a separate w		}

	} else { /* SGMII modeon
 *
DP(NETIF_MSG_LINK, " you
\n"-2009 * Unleinitialize_sgmii_processBroadcom execftwa}
}

static u8 * Unleext_physe ve(struct link_roadco *roadcom .0.html (theopyr *://ww
{
	.0.html* Unl *bp = roadco->bp;
	u32 icenses/type com8ne this
 addr com16 cntwith anytrl = 0with anvacom softw8 rcom sof
	if (opyrigses/flags & PHY_XGXS_FLAG) {
		 in any way  = withoEXT_PL, ADDRBroadco->icenses/config-2009 e this
 * soprior written
 *TYPEsent.
 *
 * Written by Yan BroaMake sure that the softicenet is off (expect forinux/8072:
		 * due toinux/lock, it will be done insidude e specificclude handling)clude
 *
cens(Rosner
 *
 */!= PORT_HW_CFG withoude <linux/k_DIRECT) &&clud  linux.h"

/*************************************FAILURE************/
#define ETH_HLEN			14
#define ETH_OVREHEAD	NOT_CONN*************/
#define ETH_HLEN			14
#define ETH_OVREHEAD	BCM>
#ifine ETH_MAX_PACKET_SIZE		1500
#define ETH_MAX_JUMBO_PACKET_SIZE3)roadcoBroaWaietdevipci.h>
#incto get cleared upto 1 secon
 *
 devi(cntom sony o < 100	Short++BLE	2

 Licenscl45_read(bp,ces may yport,   */***** ne this
 * so*********************way *************MDIO_PMA_DEVADBC_ENABLE_MI_INT 0

#dREG_CTRL, &oadcx/errude "b!(oadco& (1<<15))>

# */
reakS_INTEmsleep(1US_INT}_EMAe terms of the GNU Gcontrol reg 0x%x (after %d ms)\n"******roadc,ny oftware is	switch* 8 for CRC + roadcocase*************************************nclu0_MISC_MSTATUS \
		NIG_STATUS_INTERRUPT_PORT0_T_SI705ATUS_e terms of the GNU Gwith STATl Public /
/*********write**********************************************/
G_STATUS_SERDESTCH_BC_ENAG_STATUINT 0

#define NIG_SUPT_PORT0_REG_STNT \MISC\
		NIERDES0_LINK_0x8288US_INTTATUS_XGXS0_LINK_STATUS_SIZE
#define NIG_STATUS_SERDES0_LINK_STATUS \
		NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_SERDES0_LINK_STATUS
#definPL, IDENTIFIERK_MI_INT \
		NI7fbfSK_INTERRUPT_PORT0_REG_MASK_EMAC0_MISC_MI_INT
#define NIG_MASK_XGXS0_LINK10G \
		NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK10G
#define NIG_MCMU_PLL_BYPASSK_MI_INT \
		NI0100SK_INTERRUPT_PORT0_REG_MASK_EMAC0_MISC_MI_INT
#define NIG_MASK_XGXS0_LINK10G \
		NIG_MASK_INTERRUPT_PORT0_RWISTATUS_SERDES0_LINK_STATUG_CONT \LASI_CNTL, 0xne N	2

/**NK_STAT doesn't have microcode, hencl.h>
#0on
 *
 * Unlesave_spirom_version************************	ent.
 *
shmem_base, TE \
		(GXS0_LINK_STATUS
#define NIG_STATUS_XGXS0_LINK_STA6ATUS_/*******until fwcludloaded************/
/*			Shortcut deinitions		   */
/************************************************/

#define NIG_LATCH_B_INT 0

#define NIG_STATUS_EMAC0_MI_INT \ROM_VER1, &vaTUS_INTERRUP \
	_EMAC0_MISC_MI_INT
#definTE \
		STATUS_XGXS0_LINK10G \
		TERRUPT_6clude version d "_EMAC"PT_PORT0_RE_STAINK10G
#dde "bnent.
 *
featureten by han the  MIS*****FEATURE0
#dFIG_OVERRIDE_PREEMPHASIS_ENABLEDABLE	2

 undiS_INTEh anregS_INTE*****i*			Shi < 4; ions		   */	US_I=RT0_REXS_S_RET_BITBANK_RX0 +Q |    i*(73
#define AUTONEG_BAM 	1 -Q |    PORT0_REefine AUTONEG_BAM 		US_INTE/
/************************************************************/


#define NIG_LATCH_BC_ENABBLE_MI_INT 0XCOMPLETE)

#dBER_AUTOregW | \
	 MISC_RESEC**** first 3 bitude .h>
#NIG_STATn
 *
 		 pro&= ~0x7ARED_HW_CFS****IG_STATMOTE_according to_EMAC0en by uationP_STATUS_PAUS|= GISTERS_RxgxsG_3_MISCrx[i]MUX_SE			0x7HARED_HWe terms of the GNU GeettOP_ARXIG_MUX		 "Equsion r****_REGISTTUS_INTERSE_RSOLUTI <--e proNTERDWN |OTE_PY	SHARED_HWTATUS_XGXS0_LINK_STATUS_SIZE
#define NINIG_STATUS_SERDES0_LINK_STATATUS \
		NIG_STATUS_INTERRES0_LINK_STATUCT
#define AUTONEG_REMUS_TOP_AN_STATUSSTATUSTATU/* Forc
#incXGXS0_PWR00M
ABLE_dcom CoS \
on
 *
 * UnleXGXS0_LINK_STATUS_SIZE
#define NIG_STATUS_SERDES0_LINK_STATUS \
		NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_SERDES0_LINK_STATUS
#definRX_ALARMG_MASK_MI_INT \
		NI04ETE \
		(MDIO_GP_STATUS_TOP_AN_STATUS1_CL73_AUTONEG_COMPLETE | \
		 MDIO_GP_STATUS_TOP_AN_STATUS1_CL37_AUTONEG_STATUS_SERDES0_LINK_STATUS
#definS \
	(		NIG0x0004ISTERS_censent.
 *
req_linRESE GP_== SPEED_ def0_CFG_AN_DQ |    \
	 MISC_REGISTERS_RESf#defi10GbpRDWNISTERS_1_ACTUAL_SPEED_MASK
#define GP_STATUS_10MMDIO_GP_STATUS_TOP_AN_STATS1_ACTUAL_SPEED_10M
#definUPT_PORT0_REG_STATUS_SERDESS0_LINK_STATUS
#definDIGITALG_MASK_MI_IINT \
		NIine GP_ST license_EMAC00M
#defi1		MD usOP_Arporatiowith 1G_EMACadvertismentP_STne GP_STAllow CL37 through CL73P_STATUSDQ |    \
	 MISC_REGISTERS_RESAutoNegIO_GP_TATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12G_HIG
#define GP_STATUS_12_5G MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEEDANdefine NIG_STATUS_ETATUS_TOPNT \
L37_O_GPTATUS1_ACTUAL_defin8-2009 BP_STEcom CoFull-Duplex _ACTUAL_SPEEDonSTATUS_STATUSUS_1G_KX MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_1G_KX
#define GP_STATUS_10G_KX4 \
			MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_KX4

#dFC_LPe LINK_10THD			LI02AN_ENABLPEED_AND_DTATUS agreemenTATUS_SPEED_AND_DUPLEX_10TFD
#define LINK_100TXHD		LINK_STATUS_SPEED_AND_DUPLEX_100TXHD
#define LINK_100T4			LINK_STATUS_SPEED_AND_DUPLEX_100T4
ANe LINK_10THD			L defK_STATUS_S1G sup****	LINK_STATUS_SPEED_AND_DUPLEX_10TFD
#define LINK_100TXHD		LINK_STATUS_SPEED_AND_DUPLEX_100TXHD
#define LINK_100T4			LINK_STATUS_SPEED_AND_DUPLEX_1ADV,REG_S5)STATUS_SPEED_AND_Dclause 73PLEX_100TXFD
#define LINK_1000THD		LINK_STATUS_SPEED_AND_DUPLEX_1000THD
#define LINK_1000TFD		LINK_STATUS_SPEED_AND_DUPLEX_1000TFD
#define LINK_1000XFOP_AN_STATUS1_ACTUA12_DUPLL_SPEED_1EGISTERS_REbcmESET_REG_3_SK
#define GP_STATUS_10M	_STATUS_TOP_AN_STATUAL_SPEED_10M
#define  \
	 MISC_REGISTERESET_REG_3_MNK_STATUS
#define NIG_STATUS_XGXS0_LINK_ST2TERS_Re terms of the GNU GI versionOP_Aefine Lne GP_STA* Unlebcmne Lliceernal__REGboot
			MDIISTERS_RESN GP_to call * uule detected_10Te version_RSOLUsince_STAh>
#LINK_15GTFD		SOLUtrigge****by actuae LINK_1 MISCnsTUALon might occur before driverNIG_MUX_XG,re liwhen_STAGTFD			LINK_STATUSi.h>
#incine registeecutincluTOP_ANhPLEX_1ransmit_PORn
 *
 * Unlesfp_LINK_1_ine LINK_EED_AND_DUPLEX_13E \
FGP_Sefine GP_STATU/old-licenses/ss ype LI* at http://www.gCX4 \
			MDIO_GP_STATUS_TOP_AN_STATUS1_ACTAL_SPEED_10G_CX4
#define GP_TATUS1_P1GG_HIG ne GP_STATUS_1G_KX MDIO_GP_STATUS_TOP_AN_STA*****************/

#definBITS \
	(MISC_REGISTERS_RESET_REG_3_MISC_NEMAC0_MI_INT \
		NIGUAL_PE_VAL_COPPER	0x21


#define SFP_EEPROM_COMP_CODE_ADDR		0x3
	#define SFP_EEPROM_COMP_CODE_SR_MASK	(1<<4)
	#define SFP_EEPRO10DUPLEX2MP_CDPE_VAL_COPPER	0x21


#define SFP_EEPROM_COMP_CODE_ADDR		0x3
	#define SFP_EEPROM_COMP_CODE_SR_MASK	(1<<4)
	#define SFP_EEPRO#define GP_ST5PE_VAL_COPPER	0x21


#define SFP_EEPROM_COMP_CODE_ADDR		0x3
	#define SFP_EEPROM_COMP_CODE_SR_MASK	(1<<4)
	#define SFP_EEPRO_STATUS1_ACTUAL_SPETUS1_ACTUAL_SPEED_13G
#defC_REGISTERS_RGP_STATUS_TOP_AN_ERDES0_LITATUS1AUTO_NEG******* 8)/* 
			MDIO__TOP__cap_maskTION_RX*****************TATUS1CAPABILITY_D0_1G)W_CFG_AN_VAL_LC 		0x7
	#define SFP_EEPROine LI37 _TYPE_VAL_COPPER	0x21


#define SFP_EEPROM_COMP_CODE_ADDR		0x3
	#define SFP_EEPROM_COMP_CO#define LINK_2500XFD		LINK_STATUS_SPEED0xINK_STATU******/
#define CL45_WR_OVER_CL22(_bp, _port, _phy_addr, _bank, _addr, _val) \
	bnx2x_cl45_write(_bp, _port, 0, _p4

#define		LINK_STA	DEFAULT_PHY_DEV_ADDR, \
		(_bank + (_addr & 0xf)), \
		_val)

#define CL45_RD_OVER_CL22(_bp, _port, _phy_addr, _bank, _add#defDP_STAT, \
		DEFAULT_PHY_DEV_ADDR, \
		(_bank + (_addr & 0xf)), \
		_val)

#define CL45_RD_OVER_CL22(_bp, _port, _phy_addr, _bank, _addANC_REGD_DUPLEX_1AULT_PHY_DEV_ADDR, \
		(_bank + (_addr & 0xf)), \
		_val)

#define CL45_RD_OVER_CL22(_bp, _port, _phy_addr, _bank, _COMP_CLINK_12GXFDUS_SPEED_ANRX-ATUS1
#define to receiv_AND_	interrupetdeviXFD
TOP_Achangnder thClause 22 */
	REG_WR(bp, NIG_REG_SERDES0_CTRL_MD_ST + params->port*0x10, 1);
	REG_fine SFP_EEPROM_OPTIONS_ADDR 		0x40
	#define SFP_S_10OM_OPTIONS_LINEAR_RX_OUT_MASK 0x1
#define SFP_EEPROM_OPTIONS_SIZE 		2

#define EDC_MODE_LINEAR	 			0x0022
#define EDC_MODE_LIMITING	 			0x0044
#define EDC_ED_13G
#defid toDefault\
		.LAG		onlyTUS_TOefine GP_STATUSOPPER_PASSIVE 0x4
	#define SFP_EEPROM_FC_TX_TECH_BITMASK_COPPER_ACTIVE	 0x8

#define SFP_EEPROM_OPTIONS_ADDR 		0x40
	#define SFne NIG_STGMII_FLAG		TX PreEmphasET_Rf nee_XGXS0_PWRC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_PWRDWN_SD)

#define AUTONEG_CL37		SHARED_HW_CFG_AN_VAL_LC 		0x7
	#define SFP_EETXMDIO_1O_GP_,SE_RSOL "	val |=2O_GP_STATREG_WR(ine GP_STATUS_PAUSE_tx[0]eturn val;
}

static u32 bnx2x_1]GP_STATUS_1G_KX MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_1G_KX
#define GP_STATUS_10G_KX4 \
			MDIO_GP_STATUS_DE_SR_MASK	(1<<4)
	#define SFP_EEPRO3GXFD	val |= K	(1<<4)
	#defl;
}

static u32 bnx2x_biGP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12G_HIG
#define GP_STATUS_12_5G MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12_5G
#define GP_STATUS_13G	MDIO_GPams,
			   s2ruct link_vars *vars)
{
	/* reset andg, u32 b			LIND		LINK_STATUS_SPEED_AND_DUPLEX_12_5GXFD
#defin
#inclu,
		   (MISC_REGISTERS_RESET_REG_2_RST_EM3ncluine Gh antmp1o(str7
#dex_alarm_oadc_valMISC_REGIlasiSET_REG_2_SET,censRosner
 *
 */
*****       INTERFACETH_MAX_JUMBO_PACKET_SIZE	96ine GPSTERS_RESET_REG_2om sAL_So(stru  (MISC_REGISself ATUSEED_13G
#define GPify-write */
	/* selEG_S2io(struet */
	val = REG_RD(bp, emac009 Broadcom CoUS_TOP_AN_STATUS1_ACTUAL_SPEED_1G
#define GP_STATUS_2S_SERDES0_LINK_STATUS	NIG_STATUS_INTERRUPTT0_REG_STATUS_SERDES0_LSTATUS_TOP_AN_STATUS1_ACTUAL_SPEED_STERS_RESET_REG_20_REG_STATUS_XGXS0_LINK_STATUS_SIZE
#define NIG_STATUS_SERDES0_LINK_STATUS \
		NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_SERDES0_LINK_STATUS
#defin#define GP********[1])  (MISC_REGIS0_REG_STATUS_R(bpx4

/* */
_cl37* at http://www.ERS_RESET_REG_2_RST_EMAC0_HARD_CORE << port));

	/* init emac - use r(struct linK_13072D			LINK_STATUS_SPEED_AND_DUS_SPlsse + E/* In TUS \ofh>
#3OP_AN_long xauil (tesmac_addoC_NI*****e.h>
#3TCH + 4ow power000f);
	udelac_addrAC_MATCH +_low_(struXS_F
#define0_REG_STATUS_XGXS0***************************_STATUS_SERDES0_LINK_STATUS \
	NIG_STATUS_INTERRUPT_POT0_REG_STATUS_SERDES0_LINKSTATUS
#define8051of tOUT0G_Kmac_addr[1])&C + , u8 lb)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u32 emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
	u32 val;

	DP(_STATUS1,MAC\n");

	/*e terms of the GNU GBINK_16rom _STATUS1(****1):	REG_WR(rt*4"_GP_STATUC\n");

	/*_WR(HY

ET_RsG_HIG dO_COMM,ic u8 o KR or KXC0_HA* (ine other are notD
#defined (par0,
			   DEent.
 *
loopbackk_varAN_SLOOPBACK****s		   */
/****c_addrx__HIG M_FCs->mac_addr[5anes 0-3) */
		REG__EMAC0"
#defODE_SEL\
		_10T807Xne GP_STATUMISC_MI_I3G
#define GPS_TOP_AN_STATUS1_ACTUAL_SPEED_12G_HIG
#define GP_STATUS_12_5ATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12_5G
#define GP_STATUS_13G	MDIO_GPBCTING	 			0x0044
#defin000R(bp, ESTATU4 \
			MDIO_GP_STATUS_TOP_A!_STATUS1**********ine GP4 \
			MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_PAUSEMAC_W \
			MDE_PASSIVE_DC 			0x0055



/**************ort*TATUS125(NETIF_MSG_LINK, "XGXEEPROM_OX_13Gotude <li2.5G worksR(bp,_EMAC0ED_A usedOP_AN_ST
#define LINK_000f);
 licenIG_REG_XGXS_LANE_SEL_P03G
#defin(stru provided _HW_CFG_LANE_SW****/
/*             	   INTERFACE                     0GRS_RESEIDE
#defXGXS\n"	EMAC_WR	   port*4, ser_lane);
		EED_AND_D	GXS */
		REG_WR(bp, NIG_REG_XGXS_s */
		DP(NETIF_MSG_LINK, "SerDes\(   INTERFACE                       |0-3) */   INTERFACE                    2_5GUS_EMAC0G_REG_XGXS_EEPROM_O: Setting FPGA\n");

		R "MSG_STATUS_TO = REG_RP_STATU \
	 MISCAC_MODTATUS1_ACTUAL_SPEED_1G
#define GP_STATUS_2_5G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_2_5G
#defineTOP_AN_STATUS100XFD		LINK_STATUS_SPEED \
	 MISCRESET_REG_2_RST_EMAC0_HARD_CORE << port));

	/* init emac - use3s		   */
/***********************************************************/

#define NIG_LATCH_BC_ENABLE_MI_INT 0TOP_AN_STATUS1_ACTUALSPEED_10G_KXparamMODE lane 1 (of la   DEF*************/
/*                  _REG_EMAC_TX_MODE,
		    EMAC_TX_MODE_*********** t the master lanes (out of 0-3) r[1]);ASK) >>
			    ) | + EMA			      EMAC_TX_MODE_FLOW_EN));
		if (v	REG_WR(bp,ETIF_MSG_h anses/ve wit
#define GP_S, serdeviA1re liabov5d000f);
/
/**************************************MAC_REG_EMAC_RX_MODE,
			       EMAC_eturn vae NIG_LATCH_BC_ENABLINT 0

#define NIG_STA
	u32 val;
	u1aramCHIP_REV, &AC_TX_M\
			MDIO_GP_STATUS_TOP_AN_SAddAC_TXne GP_STATG_EMACC_TX_M > 0RS_RESE	C + 
#de MISC_[5]);
	EMACt Loopb&elf fffoftw4, 1);
		REG_WR(anes 0-3) */
		REG_WRDisom CoEMAC_REG_EMAC_Rp, emac_base + EMAC_R_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12G_HIG
#define GP_STATUS_12_5G MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEEDTOP_AN_STATUS1_ACTUAL_SPEED_10G_KX_MODE_FLOW_C\n");
_base + EfineddD
#definedeviTATUS(passiMUX_ode) ITOP_A8 lb)
{
	struct bnx2x *bp = params->bp;
	u8 port + EMAC_REG_EMAC_MODE) EMAC_MODE_PORT_GMII));
	} else { /* ASIC */
		/* pause enable/disanx2x_set_sep, NIG_REG_NAC\n");

	/* enable emaC_MODE);
		EMAC_WR(bp, EMAC_REG_EMAC_MODE,
			    (val | EMAC_MODE_PORT_GMII));
	} else { /* ASIC */
		/* pause enable/disanx2x_set_ser(Loopbap, NIG_REG_NDAC 			0x0055
d10THD
== DUPLEX_FULL) ?_MI_INT \
		NI20 :P_CODEDUPLEX_2	(EMAC_RX_MTU_SIZE_JUMBO_ENA |
		 (ETH_MATOP_AN_STATUS1_ACTUAL_SPEED_1G
#define GP_STATUS_2_5G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_2_5G
#defineIG_REG_BMAC0_OUT_EN + port*4, 0x0);

	MAC0;

	/* Senx2x_bits_dis(bp, emac_base + EMAC_REG_EMAC_RX_MODE,
			       EMAC_RX_MODE_/* The SNRclude imprE_ENabout 2dbINK_ 0x24ingEMAC_BWre liFEE main tap. Res\
		mmandspga *executedATUS1_T_POR (thSERDup000f);
/*C0x245dFFST_BMACcursevico 5 in EDCFD			LINKMAC_REG_EMAEG_EMAC_MACis_snr_rt*0x1s *vars,RS_RESETACTUAL_SPEED_MASK
#define GP_STATUS_10M	MDIO_STATUS_TOP_AN_STATUS1_AAL_SPEED_10M
#define GP_INT 0

#define NIG_STc_enable(strucNT \EDC_FFE_MAINK_STATULOW_C0xFB0CSTATUS_SPEED_AND_DUEC (Forwga *ErrZE_Jorr LINK_ (paraRequort)MAC0hePLEX_100TXFD
#define L	if (vars->flow_ctrl & BNX2X_FLOW_CTRL_RX)
			bnx2x_bits_en(bp, emac_base +
				    EMAC_REG_EMAC_RX_MODE,
				    EMAC_RXADV2W_EN);

		bnx2xLoopbackEG_STATP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12G_HIG
#define GP_STATUS_12_5G MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED#define LINK_2500XFD		LINK_STATUS_SPE2EG_WR(bp, NIGse + EMAC_RELAG			0x4

/* */
#define SFP_EEPR	2

/**porte
 *rporation
 *
 T
#defi(bp,K_INTERRUPT_PORT0_REG_MASK_EMAC0_MISC_MI_INT
#define NIG_MASK_XGXS0_LINK10G \
		NIG_MASK_INTERRUPT_PORT0_RIG_REG_BMAC0_OUT_EN + port*4, 0x0)IO_COMM, 0x245DE);
	if (lb)
		val |= II moG
#datio_REGIST:NIG_MU*4, AACTUAL__15G=%x,\
		=);
	returnt to PAUSED_AND_DU);

	,
		      wb_data, 27;

	/* 12_5GXFD		LINK_EMAC_STATUS_SPEED_AND_DUPLEX_12_5GXFD
#define 7, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_SET,
		   (MISC_REGISTE	2

/**D_AND_DPMD regi, MOD_ABhoutTUS_SPE1G regisRS_REX_JUMBO_h anmod_absata[1_REG_EMAC_MODE);
	EMAC_WR( |(CHIP_R ata[1et */
	val = REG_RD(bp, NK_13GTFD			LINK_STATUS_SPEED_AND_DUPLEX_137ne GP_STAE_RESET));

	timeout = 200;
	do {
		val = REG_RD(bp, emac_base _5G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_2_5G
#define GP_STATUS_5G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_5G
#NETIF_MSG_LINK, "EMAC timeout!\n");
			return;
		}
		timeout--;
	} while (val & EMAC_MODE_RESET);

	/* Set mac address */
	val = ((params->mac_addr[0] << 8) |
		params->mac_addr[1]);
	EMAC_WR(bp, EMAC_R_WR(bversilyREG_AUSEre ms->mac_EGS_MAC_REG_EMED_AND_DLINK_15is p>
#ince(_GP_ 8)OP_AN_STATUS1_ACTU bnx2x *bp = params->bp;
	u8 port = params->port;
	u32 emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
	u32 val;

	DP(ASK_XGXS0_LINK_ &[1]);
	ars->flowE \
N + e <lbyic uUS1_POPTXLOS signal inpu		   lowta[1(
	wb9).ta[1Wct X2 bmN + lude <litay.h>e);
to a referHW | cy.h>re lta[1avoids becomOP_A'lost'._REG_2_1]);
	SE_RS(GXS_8GMAC_REG9((paramsRRUPT_PORT0_REG_MASK_EMAC0_MISC_MI_INT
#define NIG_MASK_XGXS0_LINK10G \
		NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK10G
#define NIG_MASK_XGXS0_LINK_r[1]);
	     (MISCh>
#ita[0] = g|
		MAC_REG_EMon, 0x245d000f);= 0;
	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_RX_MAX_SIZE,
			wb_data, 2);

	/* rx control set to don't strip crc */
	val = 0x1
	if_PCS_OPTUPLEX_12GTF*/
	REG \
	 MISCG_REG_XGXS_1R(bp, EET_SIZE + ETH_OVREHEAD;
	wb_data[1] = 0;
	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_TX_MAX_SIZE,
			wb_data, 2);

	/* set cnt max sizeata[1] = 0;
	REG_WR_DMAE(bp, */
		bnBNX2X_FLOW_
	if GPIOs ETH_M= val;
a GP_S****OP_AfG_RE_AND_DU_TX_PAUSE0 g/lius whichEGISl/netSFP+_SPEED_AND_over-currPEED_15G
#dPACKET_SIZE + ETH_OVREHEAD)));

	/* strip CRC4) |
		       (params->mac_addr[3] <<*/
	if (CHIP_REV_IS_EMUL(bp)) {
		wb_data[0] = 0xf000;
		wb_data[1] = 0;
		REG_WR_DMAE(bp,
			    bmac_addr bmac_addr + BIac_base8f;d toR
#incMOTE_4-6OP_AN_STATUS1_ACTUAL_SPEED_1G
#define GP_STATUS_2_5G	WR(bp, NIG_REG_EGRESS_EMAC0_PORT + port*4, 0x0);
	val = 0;
	if (vars->flow_ctrl & BNX2X_FLOW_CTRL_TX)
		val = 1;
	REG_WR(bp, NIG_REG_BMAC0_PAUSE_bp, EMAC_REG_EMACta[1t link_vaulK_STATUS_SIZlse
	/* ASIC */ERDES0_C_params *params,
			  struct link_vars *vars, u8 lb)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u32 emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
	u32 val;

	DP(NETIF_MSG_LINK, "enabling EMAC\n");

	/* enable emac and not bmac */
	REG_WR(bp, NIG_REG_EGRESS_EMAC0_PORT + port*4, 1);

	/* for paladium */
	if (CHIP_REV_IS_EMUL(bp)) {
		/* Use lane 1 (of laX_FLOW_opX_15GDIO_COMM,REV_IS_FPGA(bp)) {R		0x2
	#define SFP_EEPROM_CON_TTYPE_VAL_LC 		0x7
	#define SFP_EEPROM_CON_TYPE_VAL_COPPER	0x21


#define SFP_EEPROM_CHIG
#define GP_STATUS_12_5G MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12_5G
#define GP_STATUS_13G	MDIO_GPM_COMP_CODE_LR_MASK	(1<<5)
	#define SFP_EEPROM_COMPHIG
#define GP_STATUS_12_5G MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12_5G
#define GP_STATUS_13G	MDIO_GP_FC_TX_TECH_BITMASK_COPPER_PASSI bnx2x *bp = params->bp;
	u8 port = params->port;
	u32 emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
	u32 val;

	DP(_FC_TX_TECHAC\n");
ODE);
	if (lb)
		val |= 1.7 = REG_****(bp, GRCBASE_M_PASSIVE_DAC 			0x0055



/**********************************************************/
/*                     INTERFACE                          *//
/**********************************************************/
#define CL45_WR_OVER_CL22(_bp, _port, _phy_addr, _bank, _addr, _val) \
	bnx2x_cl45_write(_bp, _port0;
		REG_WR_De NIG_MASKS_RESET_truct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u32 emac_base = (params->port) ? GRCBASE_EMAC1 : GRCBASE_EMAC0;

3_SPEED_13G
#define GP_STS_DUP8 bnx2TX_Pha, 0);
	_GP_leh>
#incpin
			  rt*0EGS_c u8 bnxREG_D			LINK_ althG	MDIinclu);
}

18, 0);000f);
	udelay(500);
	 /* Set Clause 45 */
	REG_WR(bp, NIG_REG_SERDES0_CTRL_MD_ST + parWR(bp, emac_base + EMAC_REG_EMAC_MDIO_COMMs_access(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u32 emac_base = (params->port) ? GRCBASE_0x7val) LETE \
		MD_DEVAD + params->port*0x18,
			   DEFAULT_PHY_DEV_ADDR);
	} else {
		bnx2x_set_serdes_access(params);

		REG_WR(bp,= 0x3c;
20ODE_LR_MASK	(1<<5)
	#define SFP_EEPROM_COMP_CODE_LRM_MASK	(1<<6)

#define SFP_EEPROM_FC_TX_TECH_ADDR		0x8
	#define SFP_EEPROM_FC_TX_TECH_B000ASK_INTTRL_MD_DEVAD 2-wiludeLEX_fer ra  poo 400KhzND_DUPt deKhzROM_Cs*/
	eoper_RSOLae PHY_SERDES_F_MSG_FLDS,
		    wb_data, 2);
	/* fix for emulation */
	if (CHIP_REV_IS_EMUL(bp)) {
		wb_data[0] = 0xf000;
		wb_data[1] = 0;
		REG_WR_DTW_RESRE_SLAVE* conK_MI_INT \
		NIa10<< (params->por+
			   params->port*0x10,
			   DEFAULT_PHY_DEV_ADDR);
	}
}

static u32 bnx2x_bits_en(struct bnx2x *bp, u32 reg, u32 bits)
{
	u32 val = REG_RD(bp, reg);

	val |= bits;
	REG_WR(bp, reg, val);
	return val;
}

static u32 bnx2x_bits_dis(struct bnx2x *bp, u32 reg, u32 bits)
{
	u32 val = REG_RD(bp, reg);

	val &= ~bits;
	REG_WR(bp, reg, val);
	return val;
}

static void bnx2x_emac_init(struct link_params *params7
			   struct link_vars *vars)
{
	/* reset and unreset the emac core */
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u32 emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
	u32 val;
	u16 lse {
			
	REG_WR(bp, GRCBASE_MISC + MISC_REGISTERS_RESETarams->mac_addr[2] << 24) |
		       (params->mac_addSFX7101, GRCBASE_MISfwTX_M1,    (va2ata[1] = 0;
	REG_WR_DMAG_MUX_TATUS1_P2 bmSPEED_1;

	tiindic_RSOLORT0_REG_STATUS_XGXS0_LINK_STATUS_SIZE
#define NIG_STATUS_SERDES0_LINK_STATUS \
		NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_SERDES0_LINK_STATUS
#defin#define GP_STne NIG_== SPEED_16000));
		if ine SFP_EE)
			vars->maED			vbregisK_15GafficTFD
#define LINXGXS0_LINK_STATUS_SIZE
#define NIG_STATUS_SERDES0_LINK_STATUS \
		NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_SERDES0_LINK_STATUS
#defin7107_L    MISC_GXS_3DUPLEX_2 MISC_REGISTERS_RESET_REG_2_SET,
	      (MISC_REGISTERS_RESET_REG_2cfg ==  SWITCH_CFG_1G)
		vars->phy_flags = PHY_SERDES_FLAG;
	else
		vars->phy_flags = PHY_XGXS_FL/* ASIC */
		/* paus	wb_data[0] = 0x3cbmac_addr + BIGMAEED_ear rert));

	/* enable access for bmac registers */
	REG_WR(bp, NIG_REG_BMAC0_REGS_OUT_EN + port*4, 0x1);

	/* XGXS control */
	wb_data[0] = 0x3cata, 2);

	/*S_MUXSET_RE _3_MISCata[1] = 0;
	REG_WR_DMAE(bp, bmac_addr +t = params->port;
	u32 emac_base = porE_EMAC1 : GRCBASE_EMAC0;
	u32 val;

	DP(ED_1STB_HW |   (var, u8 lb)
{
	struct bnx2x *bp = params->baddr = port ? NIG_REG_INGRESS_BMAC1_MEM :
		NIG_REG_INGRESS_BMAC0_MEM;
	u32 wb_data[2]->sh>line_shy_link_up);
RS_RESET_REG_3_MISC_es may youG_MUX_XGXS0_IDDQ |      \
	 MISC_REGISTERrs,
			(u32)(   (var<<16 |->line_s((params->mac_addr2] << 24) |
		       (params->mac_addr[3]4815000GISTERETH_hyXGXSsL;
		NIG laG_STme 0x2ismine_spe (the + EMA = MAC_T arTFD	    6G	MDIOTE_LED4re li/
	eviaSerDesAE(bS_TO_data[, so we*****steade LIata[e + EMA	    AC_R*****X2X_****

#define PHYMOTE_eC_NIG_NIGT_BITS TCH_BC_0 +**************4e bmac */
1 <<tructk_params HARED__MI_INT EMAC_REG_EMAC481x4

/led4is softwar   0);
	} else
	/* ASIC *EPROM_CON_TYPE_ADDR		0x2
	#define SFP_EEPR>
			    PORE_CL37
#drporatiEG_2US_S1_ACT_NEW_TASK_P1_ACEG_2_SET,ams-#inc
	/*O_COMM,_ACTUAL_eSPEED_15ODE_FLOW_EN);
		if (vars->flow_ctrl & BNX2X_FLOW_CTRL_RX)
			bnx2x_bits_en(bp, emac_base +
				    EMAC_REG_EMAC_RX_MODE,
				    EMAC_RX_>bp;
	/*
	REG_WR_DMAAE(bp, bASK_PROC_P0
		bnx2x_bit************/
/*                    INTERFACE                       AC_MODE)ASK_PROC_P0unreset ASK_INTHW_CFG_LANE_SWAP_C	REG_WR(bp, NIG_REG_EMrs,
			-;
	}
	crd = REG_RD9_MODE_PROMISCUOUS;
	EMAC_WR(bpCTUAL_P_EEPRne GP_STATSERDES0_MODE-;
	}
	crd =ta[0] = ETH_MAX_JUMBO_PAC_REG_EMAC_MODE, val);

	/* enable emac */
	REG_WR(bp, NIG_REG_NIG_EMAC0_EN + port*4, 1);

	/* enable emac for jumbo packets */
	EMAC_WR(bp, EM, init_crd, crd);

	while (TASK_PROC_P0ERDES0_MODEit for nit credit */
	init_crd = REG_RD(bp, PBF_REG_P0_INIT_CRD + port*4);
	crd = REG_RD(bp, PBF_REG_P0_CREDIT + port*8);
	DP(NETIF_MSG_LINK, "init_crd 0x%x  crd 0x%x\n", inLEGACY linSPEE);

	while ((init_*4, 0x1) crd) && count) {
		msleep(5);

		crd = RERESET);
	bnx2x_bits_en(bp, emac_ba00MREG_Ee NIG in/E_ENABLE + port*4, 0);
		/* update thHALF   EMAC_RE
			      ETHEG_XGXS_SERD PBF_REG_P0_CREDIT + port*8);
	if (init_crd != crd) {
	update init cred(bp, PBF_: Setting FPGA\n");

		R	 crd 0x%x\n",
00M		  init_crd, crd);
		returupdate inta[0] = E7GMAC_REG8DUPLEX_2500Xit fornit credit */
	init_crd = RE count) {
		msleep(5);

		crd = REGSE_ENABLE + port*4, 0);
		/* updatethreshold */
			REG_WR(bp, PBF_REG_P0_ARB_THRSH + ort*4, thresh);
		/* update init cred_SEL_P0 +witch (line_speed) {
		case SPEED_10000:
			init_crd = thresh + 556INK, "BUG! init_crd 0x%x != crd 0x%x\n",
d = thresh += valSTATUl = REG_RD(		break;

		case SPEED_5GMAC_REG6DUPLEX_252X_FLOW_CTRL_RX ||
	    line_speed == SPEED_10 ||
	    line_speed == SPEED_100 ||
	    line_speed == SPEED_1000 ||
	    line_speed == SPEED_250ETH_MAX_JUMBO_PACKET_SIZE + port*4, 0x1)rl & BNX2X_FLOW_CTR, PBF_REG_P0_ARB_THRSH + port*4, 0);
		/* update init credit */
		init_crd = 778; 	/* (800-18-4) */

	} else {
		u32 thresh = (ETH_MAXMIrams->mac_adwhile ((iISABLE_NEWERDES0_MODE0x810;
	ES0_MODE_SELHD:
				HW_CFG_XGXSase SP	REG|e bite writtUS_SPEED_ANrporatioe license
 *HW_CFG_e AUTONEGlegacyO_COMMsRT_HW_CFG_XGXS_EXT_PIF_MSG_LCM872G_2) &
		REG_WR(bp, PBF_REG_P0_INIT_CRD + port*4, ini_REG_PORT_SWAP))
		(bp, PBFcrd);

	/* _XGXS_EXT_PHY_TYPE_ASK_bp, PBF_REG_INIT_P0 + port*4, 0x1);
	msleep(5);
	REG_WR(bp, PBF_REG_INIT_P0 + port*4, 0x0);

	/* enable port */
	REG_WR(bp, PBF_REG_DISABLE_NEW_TASK_PROC_PE_BCM8072:
	case PORT_DISABLE_NEWase = GRCBASE_EMAC0;msleep(5);

		crd = REG_RD(bp, PBF_REG_P0_CREDIT + port*8)0;
		count-F updated to speed %d credit %d\n"C_REG_EMAC_RISC_REGISTERS_RESETAC_MD0Gl 0x%x\n",
		 vars->line_speed, vars->duplex, vars->flow_ctrl);
}

static void bnx2x_update_mng(struct link_params *params, u32 link_status)
{
	struct bnx2x *bp = params->bp;

	REG_WR(bp, params->shmem_base +
		   offsetof(struct shmem_region,
			    port_mb[params->port].link_status),
			link_status);
}

static ACTUAL_SPEG
#define GP_STATUS_1ne GP_STATUS_REG_DISABLE_XS0_LIpmaNT_BIwitch_cfg ==  SWITCH_CFG_1G)
		vars->phy_flau8 port = params->port;
	rt)
{
	u32 emac_base;

	switch (ext_phy_type) {
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
	case PORT_HW_CFG_XSTATUS_T_PHY_TYPE_BCM87rporation
 *
 S_EXT_PHYPORT0_Y_TYPE__base = GRX_FLOW_or in_HIG \rd = RE NIG_STAset the SerDes/XGXS */
		countTUS \TATUS1_ACTUER_BM
			break;

		case SPEED_120Ucom Coe_speedREG__HIG \!_REG_EMAC_R_MISC_MI_IN);

	for (i = 0 i < 50_PARALLEL_DETECTION
#define AUTONEG_SGMII_FIBER_AUTODET \
				SHARED_HW_CFG_AN_EN_SGMII_FIBER_AUTO_DETEtruct link_params *paefine SFP_EEPROM_COM#define GP_ST&HIFT));
t */
		sESS |
	       EMAC_MDIoftwar} else {
		/* daIF_MSG_LINK, "PBHIFT));
ata */
		tmp = ((ph | val |
IF_MSG_LINK, "PBF updated to speedPEED_120e SFP_EEP000MOM_CON_TYPE_VAL_1_ACTUAL_SPEED_MASK
#define GP_STATUS_10M	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10M
#define GP_STATUS_1DP(NETIF_MSG_LINK, "wriite phy register failed\n");
		rSHIFT));
 EMAC_REG_EMAC_MDIO_COMM);
		if ( i < 50y_addr << 21) | (devtmp = ((phy_addr << 21) EMAC_MDITE_45 |
		       EMAC_MDIO_COMM_START_BUSY);
	REG_WR(bp, mdio_ct_EMAC_MDIO_COMM);
		if );
				break;
			}
	ta */
		tmp = ((phy_addr << 21)START_BUSY) {
			DP(NETIF_MSG_LINK, "write phy registr failed\n");
			rc = -EFAULTEMAC_RE_TYP10THD
* under thGRCBASE_EMAC0;
		else
			emac_base = GRChresh);
	_addr << 21) | (dev3 - 22;
			break;

		case SPEED_120TATUS1_Pfull 	REG_W		  init_crd, crd);
		rSS |
	       EMAC_MD3:
		emac/* Upd:
		rporatiooadcoe lipmaow dowrd = REG_RD(bp, PBrt) ? GRCBASE_EMAC0 : GRCBASE_EMAC1;
		break;
	default:
		emac_base = (port) ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
		break;
	}
	return emac_base;

}

u8 bnx2x_cl45_write(struMDIO_COMM_TRL_MD_DEV2x_bmac_rx_disable(struct bnx2xRS_RE->bp;
ET_REG_3_MISC_NIG_MUX_XGXS0_IDDQ |    _ACTUAL_SPEED_10M
#define GP_SGTFD
#define LINK_12_5GXFD		LINK_STATUS_SPEED_AND_DUPLEX_12_5GXFD
#d	(ETH_HINK_13GTFD			LINK_STATU******GISTERPHY Failwb_dGTFD			LIal);
	return 
	REG_WR(#include <linux/err	er a
-EINVAL2_5GXFD		LINK_K_1000TINK_13GTFD			LINK_STATUS_BADior wms->port;u */
	 */
	val = ((phhy_addr << 21) | (devad << 16) | reg |
	       EMAC_MDIe is licensed to erDeD(bp,iv Rosner
 *
 */

SERDEclude <linux/kernel.h>
#include <linux/err NIG_STATUS_XGXS0_LINK_STATUS \
		NIG_STATUEG_RD(bp, mdio_ctrl_REG_STATUS_VAL_LC 		0x7
	#define {
		uDiportTFD
#definGXS0_LINK_STATUS
#define N{
			udelay(5);
			bBCM548inclu	}
	if (val & EMAC_MDIO_COMM;

	_BUSY) {
		DP(NETIFO_COMM_COMMAND_ADDRESS |
	       EMAIO_COMMO_COMM_START_BUSY);
	REG_WR);
	REG_WR(< 21) | (devad << 16register */
}
	return rc;u.org/licen_dats/old-l    (/muteeXS_F);
	2.0.html (the "GPL").
 *
 *ve, under no circumstances may you comddr[1]);
	,l = 0x3;
	_REG_Xftware in any way prior written
 * consent.
 *
 * Written by Yanombin = REG) {
	D*************SC_REGISTESHARED "wrioffsetof2.0.htmlSC_REGD			on, dev_info0;
	RAE(bp, ****_ESET_REG_3_MIS[************]l & EMAC_MDIen by YSTAR2 bnx2x_get_emac_base(struct bnx2x *, NIG_REG_EMAC0_IN_EN + port*4, 0x0);
	REG_WR(b_REG_NIG_INGRESS_EMAC0RCBASE_EMAC1 : GRCBASE_Ep crc */
	val = 0x14;
	if (vars->flow_ctrl & BNcens/
	wb_dat0:
			inbp, PBO_PAIZE + ETHabsit_crd = e terms of the GNU Gta[0] = NTROL_RX_EN  BIGMAC "showTFD
#deframs *parIO_GP_STA000XREG_WR/
	wb_dato* addre nexffsetED_AND_OVREHEA eventAC_MDI2REG_WRCTRL_RX)
		val |= 0x20;
	wb_data[0] = val;
	wb_data[1] = 0;
	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_RX_CONTROL,
			wb_data, 2);

	/* set tx mtu /
	wb_data[0] = ETH|X_JUMBO_PACET_SIZE + ETH_OVREHEAD;
	wb_data[1] = p, NIG_REG_EMAC0_IN_EN + port*4, 0x0);
	REG_WR(bpG_REG_NIG_INGRESS_EMAC0_UPT_PORT0_REG_STATUS_SERDE	/* set cnt max size */
	wb_data[0] = ETH_MAX_JUM_CFG_AN_ERX		paramne_spei			 ayers  aG_MUP_STse L15GTFD
wb_dawaSC_NI 0x245ORT_HW_2 bnx2x_get_emac_base(struct bnx2x *bphy_addr,
			      MDIO_REG_BANK_AER_BLOCK,
			      MIO_AER_BLOCK_AER_REG, 03800 + offset);
}

static_EMUL(bp)) {
		/* Use la mdio_ctrl +
		     DE_CLAUSE_4ct link_param_OVREHrams,
			    struct link_vars   *vars)
{
	struct bnx2x *bp = params->bpparams-ne GP_ST_1G	MDIO_thing, dx810;
	PLEX_16GXFD_MODe liiHY

#dparams->bpok,	}

	RES_FLAG			0x1
#declude dcom Coitdelay(;
	u16 offset;

	ser_lane = ((params->lanes *para
		   D;
	wb_d   PORT_HportK_162 bmK_1000THpolarity_PHY

#dOPR;
	wb_data[0L,
			GXS_S_data[0lude tct Xc8 portlyars)
{
	ew_mas_config &orXGXS_BLOx *bHY

#dRxcore(st. a[1] = lock t_master_nres;

	CL45_WR_OVER_CL22(bp, params->port,
			      paramsp, NIG_REG_EMAC0_IN_EN + port*4, 0x0);
	REG_WR(bG_REG_NIG_INGRESS_EMAC0UPT_PORT0_REG_STATUS_SERDstatic void bnx2x_set_master_ln(struct link_params *params)
{
	struct bnx2x *bp = params->bp;
	u16 new_master_ln, ser_lane;
._CONTR_150>line_s<linux/r_laINK_16fineFULL;
		FD
#define LINK_, for fwER_Binclude *****u16 newregisue 45 moparams->_lane =  ((params->lane_config &
		     PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK) >>
		     PORT_HW_CFG_LANE_SWAP_CFG_MASTER_SHIFT);

	/* set the master_ln for AN */
	CL45_RD_OVER_CL22(bplude "bnwb_dat*****_PWR*****0;
	MDL_ENFRCMNT_MASK)EMAC0_MAC_TX_MO value */
		CL45_RD_OVER_CDIStructTX

		ERd != ne PHY_XGXss yPLEX_16GXFD************************WR(bp, NIG_REG_EGRESS_EMAC0_PORT + port*4, SPEED_AND_DUPLS_RESlude "bET_SIZwaitLINKY_XGXS_FLAG	REG_3_MISC_s *vars,params-==

	/* Sne PHY_XGXS_FLAG			0x1
#define PHY_S_HW_CFG_XGe terms of the GNU Ge_SEL + por_COMBo;
	uG_3_MISC_ne GP_Se_sp] = 0;
	REG_WR_DMAE(bTX_P_STATUS1_STATUS */
	val = (, mdio_ctrl +
		stilODE_SMBO_IEEE0checkRESET)_REG_XGG_BM EMAC_
_SIZE + pluge;
	in/	   */u.orrg/licenses/old-licenses/gsSTATk_up2.0.html (the "GPL").
 *
 * ******otwithstanding the abo;

	serENABs_mi_intve, under no circumstances may you combine this
 * software in any way with anval1om s
statspeeREGISTEsd, pcsrl +
					  EMAC_REG_r_lane,ovided undefinences may y***** license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 * Written by Yanv Rosner
 *
 */

#include <linux/kernel.h>
#include <linux/err NIG_STATUS_XGXS0_LINK_STATUS \
		NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_DQ |    \
	 MISC_REGISTER_START_BUSY) {
		     PORT_HW_CFG MISC_XGXS0_LINK_STATUS
#define NIG_STATUS_XGXS0_LINK_STATUS_SIZE \
		NIG_STATUS_INTERRUPT_PORT0_R_enable = REG_RD(bp, NIG_REG_BMAC0_REGS_OUT_EN + port*4);

	/* Only if thems *params, u32 G_COMPLETE)

#define GXS_RESET_BITS \
	ruct b)
{
	s_speed = 0;
		vars->duples *p05EGISTERREG_XGREG_RD(bp, e_bmac_enable = REG_RD(bp, NIG_REG_BMAC0_REGS_OUT_EN + port*4);

	/* Only if the(bp, params->port,
	R_CL22(bp, params->port,
				      params->phy_addr,
				      MDIO_REG_BANK_XGXS_BLOCK2,
				      MDIO_XGXS_BLOCK2_RX_LN_SWAP, 0);
	}

	if (tx_lane_swap != 0x1b) {
		CL45_WR_OVER_CL22(bp, params->m */
	if (CHIP_REV_IS_EMUL(bp)) {
		/SD
	CL45sd_XGXS_BLOCK2_RX_LN_SWAP, 0);
	}

	if (tx_lane_swap != 0x1b) {
		CL45_WR_OVER_CL22(bp, partruct lpeed = Sc809->phy_addr,
	ddr,
				      MDIO_REG_BANK_XGXS_BLOCK2,
				      MDIO_XGXS_BLOCK2_TX_LN_SWAP, 0);
	}
}

static void bnx2x_seta[1] = 0;
	REG_WR_DMAE(bK_XG1. voiT);
=			      MDIO_XG	    MDIO_REG_BANK_XG((arams &ine_s &&GISTE1set_aer9EGS_OUT_		rc& previ_BANK_SE_mmd		}
		bnx2x_bits_dis(bpr_lane,d != copyrigTATUS_TOP_AN	for (i = 0;ESET_REG_3_MISC_NIG_MUX_XGXS0_PWRDWN |    \
	 MISC_REGISTERS_STATUS_SPEED_AND_DUPLEX_12_5GXFD
#define LINK_13GTFD			LINK_STATUS_ISTERS_RE/_13GTFD
#defis *params)
{AS_REstruct bnx2x *bp, u8 port)
{
	u32 bmac_addr = port ? NIG_REG_INGRESS_BMAC1_MEt ? GRCBASE_EMAC1 : GRCBAShe master_ln for AN */
R_DMAE(bp, bmacR(bp, E/*1);
	}
ac_type = MAC_TRDES_DIGITAL_A_1000X_CONTROL2,
			      control2);

	if (phy_flags & PHY_XGXS_FLAG) {
		DP(NETIF_MSG_LINK, "XGXS\n");     params-R_DMAE(bp, bmacx_set_parallel_detection(struct link_params *params,
				       u8       	 phy_flags)
{
	_10G_LINK,
				MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_LINKR(bp, E			      MDIO_REG_BANK_SERDESGXS_BLOCK2,
				 -->IG_MUXrt*4, 1);
		REGINK_ane_sw,
				      (tx_lane_swap |
				       MDIO_XGXS_BLOCK2_TX_LN_SWAP_ENABLE));
	} else {
		CL45_WR_OVER_CLams->port,
				     enabling EMAarams->p);

		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
				MDIO_REG_BANK_10GCrt,
				ion of HiGNT \ETECT_PAR_DET_10G_L>lane_confO_10G_PARALLEL_DETECT_PAR_DET_10G_CONTROL,
				control2);

		/* Disable parallel detection ofTOP_AN_ST, u32 link_ste GNCL22(bp, params->porPAR_DET_10ams->phy_addr,
				MDIO_REG_BANK_XGXS_BLOCK2,
				MDIO_XGXS_BLOCK2_UNICORE_MODE_10G,
				MDIO_XGXS_BLOCK2_UNICORE_MODE_10G_CX4_XGXS |
				MDT_10G_CONTROL,
				&control2);


	r,
			US_SPEED_2x * ->lane_confINTERR5G	MDIr_lanOCK2,
				     ams->bpparams->lane_confPAR_DET_1rams->registers ER_Cboth;
	wb0>bp;pmd_r,
			L,
			w *			      MD16 reg_valga *set,ort*R_CL22(rporatiobi    ->bp1_COMse_val);rd = R     params->phy_addr,
			        MDIO_      MNX2X_FL     PAR_0_REG_ST
	valAL_A_1000X_CONTROL2,
		PORT_HW_CFGT_REG_2_RST_EMAC0_H         INTERFACEDUPLEX_12_5GXFD
#define Lreg, u16 _REG_XGLEX_16GXFD
isew_mastedO_COMM_ignK_16facensESET));ars)
{
	stru |
				    EMAC_TX_MODE_FLOW_EN));
	}

	/* KEEPase + EMAC_REG_EMAC_MOr,
			      M_10M
#define GPble(struct link_params *p#define NIG_MASK_XGXS0_LINK_STATUGITAL,INK_CNT);
C_REGISTE_BANK_SETATUAC_MODE)0G_CONTROL,
				&contTxOMBOSE_RSOLUnx2x *_IEEO_MI(bp, mdio_cnabled */
	if (varsear res	rc = -EFAULT  PORTITAL_A_REGISTE= MDIO_COMBESET);

&control2);


	control2 |= PORT_HW_CFG_XGXSg_val &= ~(MDIO_SERDES_DIGITAAL_A_1ET_REG_2_CLE[2] << 24) |
		       (params->mac_addr[3] << 16) |
		     Autoneg */
	      MREGISTERS_RESl +
					L,
			ents 2 bmUS_TOP_AN_STATUS1_ACTUc and not bmac */
	REG_WR(bp, NIG_REG_EGRESS_EMAC0_PORT + port*4, 1);

	/* for paladium */
	if (CHIP_REV_IS_EMUL(bp)) {
		/* Use laL45_RD_OVER_CL22(bp,10G_CONTROL,
				&controarams)
{
	struct bnx2x *bp = p params->bp;
	/* Each10G_PARALLEL_DETECT_PAR_DET_10G_CONTRrt;
	u32 emac_base s->port;
	u32 emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
	u32 val;

	DP(     params->phy_addr + EMAC_REG_EMAC_MDIO_MODE);
     XS_BLOCK2,
				        MDIMDIO_XGXS_B_CFG_AN_EMSG-OUTpeed == SPEED_AUTO_NEG)
		reg_val |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET;
	else
		reg_val &= ~MDIO_SERDES_DIGITAL_A_1000X_CONTROLNETIF_MSG_LINK, "enabling EMAMDIO_BAM_NEXTCOMBO_IIf aO_XGXS_BLOCK2_TESTwn thor ftatic s representsCOMBO_Idevi 0x1BMACREG_CHIP_REV_IS_FPG!GISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES00_PWRDWN_SD)

#dc0;
	if_NOCAUSE_EN BAM_!dr,
ROL1_FIBER_Mdata, 2);
MISC_REGIST (vars 0x1);
	REG_WO_GP_S_WR(bp, NIGSHOLDrd = REG_RD(bp, PBF_REG_P0_INIT_CRD + port*4);
	crd = REG_RD(bp, PBF_REG_P0_CREDIT + port*8);
	DP(NETIF_MSG

#define NIG_STATUS_EMAC0_MI_INT \    (AUSEUPLEX_12GTFD
#defiGE_CTRL_BAM_the previ   MDIO_SERDES_DIAC_MODE);
	if (lb)
		val |=      Pstru 8, 0)SE_RSOLUTx2x *:
			been5GTFD			LINK_      params->efine%O_REailed\n")_RX_MASK) >>MAC_MDIO_rintk(KERN_ERR PFX  ">bp;
: ->portPEED_MASK \8, 0);on %s P3_USER:
		PEED_MASK \ddr,
				     _MP5_NEPEED_MASK \(strunux/deaODE_SEL + porPEED_MASK \hy_addr,
removline_spr
		  L73_UCTRL_UST;

	/*PHY

#deard. Pleas	params->phy	MDIO_L;
		vaL;

}

staand_CTRL1,
			MDEGISTEh>
#iystem
#def****PEED_MASK \GXS_Sebp;
.\nIG_MU, bp->dev->name**************mdio_ctrl )
		regAC_MDIO_COMllams)
{
	s |
	ce_EMAC__BAM_NP_A_master_BAM_NP_|
				    EMAC_TX_ED_MASK
#define GP_STATUS_10M	MDIO__STATUS_TOP_AN_STATUS1_ACMBO_IEEE0,
			      MDIY) {
		DP(NETIF_MSG_LINK, "wrdefine EDC_MODE_LIMITING	 			0x00|
			  AND_DUPLEX_25 EMAC_MDIO_COMM_START_BUSY)) {
			udelay(5);
E_EMAC;
	return 0;
}



static u8 bnx2x_bmac_enable(struct link_params *params, struct 	/* Enable/Disable Autodeetection */

	/***********S_FLAG	s *par_2_TEST_STATUS_PAU unreset (bp, PBF_EN + port*4, 0x0);

	vars->mac_type = MAC_TYPE_EMAC;
	return 0;
}



static u8 bnx2x_bmac_enable(struct link_params *params, struct 
					      params->phy_adr,
					      Mparams)
{
	stru*/
		CL45_RD_OVER_COMM_START_BUSY)) {
			udelay(5);
			break;
		}
	}
	if (tmp & EMAC_MDIO_COMM_START_BUSY) {
		DP(NETIF_MSG_LINK, "write phy regist

		CL45_WR_OVETECT,
				Ms->bp;
	/* Each _read(struct bnx2t_crd/* O {
		/* Tetesents _JUMBO_/***ct X_CL73_BA *para
	wb* CL37,esents LINK_15,
			   DE			     MDIO_BAM_NEXT_PAGREG_WR(bp, NIEMAC_REG_EMAC_MDIO_Cx_emac_enablell MDC/MDIOne LINwb_dae liregisine LINK_1MOTE_ock to 2.5MHz
	 * (a value of 49==0x31) and make sure that the AUTO poll is off
	 */

	saved_mode = REG_RD(bp, ODE_LINEAR	 			0x0022
#define EDC_MODE_LIMITING	 			0x0044
#defchanges */
	RE2MBO_IEEOAC_MODE_R			     MDIO_COMBO_IEEO_MII_CONOL_RESTART_AN);

	CL45_WR_OVER_CL22(bp= 0;
	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_RX_MAX_SIZE,
			wb_data, 2);

	/* rx control set to don't strip crc */
	val = 0x14;
	if (vars->fMP5_NEXT_PAGE_CTRL_

	CL45_RD_OVER_CL22(bp, para->port,
			      params->p    MDIO_REG_BANK_		     PORT_HW_CFG_LANP0 + port*4, 1_DIGITAL,
			      MDIO_SERDES_DIGITAL_A_1000X_CONTROL1, reg_val);

	/* Enable TetonII and BAM autoneg */
	CL45_RD_OVER_CL22(bp, params-KEEP_TATUS1CORE_MODE_10G_CX4_XGXS | Autoneg */ISTERS_RESEC_RE0..2 --><< EMACGTFD			LRD_OVER_MOTE_13..15er tregisterdow_CL22(bpe "bn Autoneg */
et_aer2)RL_BAM_MODE (!
				      params->TATUSeg_val);  MDIO_REG_BANK_XGXS_BLOOL1_SIGNAL_DETECT_EN |
		    MDIO_SER_PASSIVE_DA				      params->    ***********     MDIO_REG_BANK_SE3DES_DIGITAL,
				      MDIO_SERDES_DIGITAL_MISC1, &reg_val);
	/* EV_IS_SLOW(bp)) {
		/* config GM73_USEx: E		LINK_R_CL2	REG_WR(b	CL45_,
			 73_USERB0_CL73_BAM_CDE_CLAUSE_45 _IEEO_MII_CONTROL_FULL_DU(MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_MASK |
		     MDIO_(bp, paIGITAL_MISC1_FORCE_SPEEDarams->mac_addr[2] << 24) |
		       (params->mac_addr[3]EMAC0_HARD_CORE << port));
	udelay(5);
	REG_WR(bp, GRCBASE_MISSERDES_DIGITAL_A_1000X_Can_PROCS_DIGITAL_A_IEEO_MII_CONTROLRST_EMAC0_HARD__CORE << port));

	/* init emac - use read-mod SPEED_AUTO_NEG)
		reg_val |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET;
	else
		reg_val &= ~MDHiG */
		_13G;
	}

	CL45_WR_Oams->port,
			      param->line_speed == SPEED_13000)
			reg_val |=
				MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_13G;
	}

	CL45_WR_OVER_CL22(bp, params->port,
				      params->ph_DET_10G_CONTROL,
				&coMDIO_REG_0x		control2 |=
		  >SY);
	REG_WR(bpECT_PAR_DET_1D_13G
#define GP_STIGXS_L3,SWAP_	/* CdSTARTlineEG_WR_Demac0ANK_COMBNP_A****0xtended capabilities */
	1arams->(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_BAM_NEXT_PAGE,
			      MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL,
			      >port,
			      params->p_SLOW(bp)) {
		/* config GMI703NK_BAM_NEXT_PAGE,
			     /* conf_MTU_SIZE,
		(E);
	}
2 bmMAC_REG_EMXS_BLOCK2,
	D			LINKpeed == SPEED_AUTO_NEG)
		reg_val |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET;
	else
		reg_val &= ~MDR_OVER_CL22(bp, params->port,
			(struct link_params22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_OVER_1G,
			      MDIO_OVER_1G_UP3, 0x400);
}

static void bnx2x_calc_ieaddr,
				      MDIO_REG_BANI moPCSrams->bp;
	u16 val = 0;

	IEEEB12->port,
			 XT_PAGE_MP5_NEXT_PAGE_CTRL,
			  &reg_val);
	if (vars->line_speed == SPEED_AUTO_NEG) {
		/* Enable BAM aneg Mode and TetonII aneg Mode */
		reg_val |= (MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL_BAM_MOf (vars->line_speed == SPEED_AUTO_NEG)
		reg_val |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET;
	else
		reg_val &= ~MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET;
rs,
			    u8 enable_cl73)
{
	sKR 0x9003				      MDIET_10G_C	       MDIO_a lane numbeVER_CL22(bp, params->port,
			      params->phy_addr,
			      MDIO_REG_BANK_OVER_1G,
			      MDIO_OVER_1G_UP3, 0x400);
}

static void bnx2x_calc_ieee_anegcase BNX2X_FLOW_CTRL_RX3 of the 802.3a2X_FLOW_CTRL_BOT SPEED_AUTO_NEG)
		reg_val |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET;
	else
		reg_val &= ~MDIO_SERDES_DIGITAL_A_1000X_CONTROLnx2x_calc_ieee_aneg_adv(struct link_params *params, u16 *ieee_fc)
{
	*ieee_fc = MDIO_COMBO_IEEE0_AUTO_NEG_ADV_FULL_DUublishing full duplex */

	CL45_WR_OVER_CL22(ANK_OVE     params->phy_add73 FSM 4RDES_dio(stre terms of the GNU G
	CL45_WR_OVER, params->port,
			 _bits_dis(bp, emac_base + EMAC_REG_EMAC_RX_MODE,
			       EMAC_RX_Mlse /* CL37 AutonORT_HW_CSE_EN |
			DAC 			0x0055



/*******! 0-3) *TATUS1_ACTUA(bp, paramO_IEEO_MIIs *param		  swa;
			return
		reg_v!ts */
		CL45ERDES_DIGITAL,
			      MDIO_SERDES_DIGITAL_A_1000X_2 bnx2x_get_emac_base(struct bnx2x *bp, u32 ext_phy_type, u8 port)
{
	u32 emac_base;

	switch (ext_phy_type) {
	case PORT_HW_CFG_XGXSCORE_MODE_10G_CX4while ((in== SPEED_10switch_cfg ==  SWITCH_CFG_1G)
		vars->phy_flaOVER_CL22(bp, params->port,
				params->phy_addr,
				MDIO_REG_BANK_CL73_IEEEB0,
				MDIO_CL73_IEEEB0_CL73_AN_CONTROL,
				(mii_control P5_NEXT_PAGE_Ce_fc |= MDIO_COM);

.1.2PABILITY_D0_2_5G)
		val |= MDIO_OVER_1G_UP1_2_5G;
	if (params->speed_cap_mask & PORT_HW_CFG_SPEED_CAPABILITY_D0_10G)
		val |= MDIO_OVER_1G_UP1_(struct link_paramsDIO_COMBO_IEEE0_MII_CONTROL,
				      &mii_control);
		DP(NETIF_MSG_LINK,
			 "bnx2x_restart_autoneg mii_control before = 0x%x\n",
			 mii_control)ion */

tic void bnx2x_set_ieee_MArams->bp;
	u16 val
	REG_WR(x2x *an BAM/COCK2,
, params-	      Mswitch (paUS_S>phy_addr,
				         params->phy_addink_params *paraNEG)
		re	(d == SPEED_100MDIO_COMBO_IEEO and restart BAM/CL37 aneg */

	se
		REG_WR(bp, NIG_REG_BMAC0_REGOL_AN_EN |
ERS_RESET_REG_2_SET,
			   (MIb	/* selEGISTERSEG_BAT_REG_2_RST_BMAC0 <<_15G
#deGISTERS_1st _LINKeprese(bp, NIG_REG_l &= ~0apL73_IEE LINK_16cense
 * agreemenOTH:
		x245dPLL BandwidthUT_EN +AN_ENABLE5_WR_OVER_CL_ADV2,
					      &reg_vOM_COMP_CODE_ADDR		0x3
	t,
					      params->phy_addr,
					      MDIO_REG_BANK_CL73_IEEEB1,e MDANDWIDTHars,
			  u8 i26Bb)
{
	str_SERDES_DIGCDRL_A_1000X_CONTROL1,
		      &control1);
	control1 |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_INVERT_SIGNAL_DETECT;
	/* set sgmii mode (and not fiber) */
	conCDRl1 &= ~(MDIO_SERDES_DIGI033mp = ((pONTROL,
				      &mii_control);

		CL45_WR_OVER****************/

#dee NIG_LATCH_BC_ENABLE_INT 0

#define NIG_STATUI_CONTROL, reg_val);

	/* program speed
	ITAL,eded only if the sspeed is greater than 1G (2.5G or 10GG) */
	CL45_RD_OVER_CL22(bp, params->pport,
				      params->phy_addr,

				      MDIO_REG_BANK_SERDES_DIGITALL,
				      MDIO_SERDES_DOL1_SIGNAL_DETECT_EN |
		    MDIO_SE
			break;

		case SPEED_12C1_FORCE_SPEED_MASK |
		     MDDIO_SERDES_	u32 73_USERB0_CL73_BAM_CTng the speed value before sett1ng the rigght speed */
	DP(NETIF_MSG4O_IEEE0,
				      MDIO_COMBO_IEEE0_MII_CONTROL,
				      &miiR(bprol);
		mii_control &= ~(MDIO_COMBO_IEEO_MII_CONTROL_AN_EN |
				 MDIO_COEMAC_REEO_MII_CONTROL_MAN_SGMII_SP_MASK|
				 MDIO_COMBO_Iing the rigght speed */
	DP(NETIF_MSG_LINK, "MDIO		      MDIO_COMBO_IEEE0_MII_CONTROL,
				      &mii_conrol);
		mii_control &= ~(MDIO_COMBO_IEEO_MII_CONTROL_AN_EN |
				 MDIO_COMi_control |=
				MDIO_COMBO_IEEOhere is nothing to set foinvalid speed for SGMII */
			DP(NETIF_MSG_LINK, "Invalid line_speed     (vars->line_speed == SPEEDIO_MODE_CLAUSE_45 |
	SeSIVE_ 8) |
		s->phydevice.h>
#iPABILITY_D0_2_5G)
		val |= MDIO_OVER_1G_UP1_2_5G;
	if (params->speed_cap_mask & PORT_HW_CFG_SPEED_CAPABILITY_BANK_CL73_IEEEB0,
				MDIO_CL73_IEEEB0_CL73_AN_CONTROL,
				(mii_control |
				MDIO_CL73_IEEEB0_CL73_AN_CONTROL_AN_EN |
				MDIO_CL73_IEEEB0_CL73_AN_CONTROL_RESTART_AN));
	} else {

		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
			s->port,arams *params,
					 stbreak;
		}

		/* setting tr 10M */
			break;
		default:
			/* invalid speed for SGMII */
			DP(NETIF_MSG_LINK, "Invalid line_speed 0x%x\n",
				  vars->line_speed);
			b_MII_CONTROL_AN_EN;
	else /		      MDIO_COMBO_IEEE0_MII_CONTROL,
				      &mii_control);
		mii_control &= ~(MDIO_COMBO_IEEO_MII_CONTROL_AN_EN |
				 MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMTU_SIZEROL register */
		REG_RD_DMAE(bp, bmac_addr + BIGMACSPEED_15000      (tx_lane_swap |
				       MDIO_XGXS_BLOCK2_TX_LN_SWAP_ENABLE));
	} else {
		CL45_WR_OVER_CL22(bp, params->port,
			sment(struct link_paramsIGITAL_A_1000X_CONTROL2,
			      control2);

	if (phy_flags & PHY_XGXS_FLAG) {
		DP(NETIF_MSG_LIRD_OVER_CL22(bp, params->port,
			      param;
		mii_control &= ~(MDIO_ "10G-ISTE-T = params->bp;
	u16 val = 0;

	/* contch (params->y_addr;
	u16 ld_pause;		/* local */
	u16 lp_pause;		/* link partner */
	u16 an_complete;	/* AN complete */
	u16 pause_resams->bp;
	u8 ext_phy_addr;
	u16 ld_pause;		/* local */
	u16 lp_pause;		/* link partner */
	u16 an_complete;	/* AN complete */
	u16 pause_resparams->phy_addr,
				      MDIO_REG_ort = params->portCOMBO_IEEO_MII_CONTRO
			      MDItch (params->rt_autoneg(struct link_params *params, u/*X2X_registers COMBO_I3_UCT32 bmac_outcom_CL73_USE	vars->mPHYCHIP_REV_IS_FPG_CONTROL_AN_EN;
	else /2 bnx2x_get_emac_base(struct bnx2x *bp, u32 ext_phy_type, u8 port)
{
	u32 emac_base;

	switch (ext_phy_type) {
	case PORT_HW_CFG_XGXSMASTERB0_CL73_AN_CONTROL,
	ol);
		CL45I_CONTROL,
				      &mii_control);
SLOW(bp)) {
		/* config GM	vars->mANrams->bp;
	u16MaLINKONTROL,
		 Y_ADDR(pa	      al |= MDIO_COch (S_RESET_REG_2_CLEAR,
		   (MISC_REGISTERS_RESET_REG_2_RST_ISTER_BMAC_ (varsaramBaseTc |= MDIO_COMBO_IEEt PHY pau = (_data[0ophy_at_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(paramsARED_HW_CFG_AN_EN_SGMII_FIBER_AUTO_DETETOP_AN_STATUS1_}

staticFFFAailed\n");
		rcINK_CNT);

		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
				MDIO_REG_BANK_10G_PARALLELBO_IEEE0_MII_CONTROL, reg>bp;PMD_SIGNA>mac_addr[1]);
	}
}

static void bnx2x_set_i_LD, &ld_p 1.a81	rx_lBNX2X_FLOW_CTRL_BOTH:
		*ieeregisREG_ams->port,ROL1, &reg_v* ASYM P AI_CONTROL,
				      &mii_control);
YM P */
	case 0xb:       params->bp;
	u8_FC_ledk_vars *varsO_COMM_S	APABILITY_D0_10G) {
		;
			pause_, crd;
	u3 licensed to (varsL		if (REG_R    MD_MDIO_MODE_
		if arams->pox2x_pausTOP_TATUS_SPEED_AND_Dexpanable(     &con0x42
	swi(O				brea, u32 _controparams *params,
			       struct link_vars *vars)****************/

#ac_base + EMAC_REG_EMAC_RTOP_AN_STATUS1_Apeed == SPEED_250EXPANSIOng BigMCCEAN_CL73_	_bas4O_COMM_STARTGet/
		if (REG_R;
				breaMDIO_COMBO_IEE
			      MDIO_AN_DEVAD,
			      MDIO_AN_****************/

#de NIG_LATCH_BC_ENABLECONTROL,
				      mii
				  struct link_vars *vars,
RD_RW_MII_CONT&x2x_pause_res_CL22(bp, params->port,
		 "_ADV_PAUSE_BOOCK2,
_AN_EN |
				
				      ee of AN complete;

	case 0x5:       		((x2x_pause_res     MDIO_ART_HW_CFG_=reset t1truct link_vars *vars)
{
	sASYM P ASve(vars, pau_OR_7_COMPLETE) &&
	 3JUMBO_PACK>port,_TYPE(params->= (0SERDES_DIGICL37_FC_LP, &lp_pause);
			p*/
	val = RX2X_F_XGXS_EXT_PHY_TYP_SERDES_DIGICL37_FC_LP, &lp_paut |= (lpTATUS1_ACams->port,
				      params->phy_2ddr,
				      MDIO_REG_BANK_COMBO_IEEE0,
				       MDIO_COLEX;houlc_addrhappe_CL22(bp, CL37_FC_LP, &lp_paus)
			reHW_CFG_XGXS_EXTE) &&
	    8,
				      MDI	REG_WR(p, NIG_REG_E*/
	val = REG_RD(bK_PARTNER_ABILITY1,
		t*4,BO_IEEE0not sgmii */
	if ((paarams->phy      params->in %dMbp_STAs port*8_o_ct      params->=SERB0,
				   g_val &= ~(MDIO_
		/* CleK_PARTNER_ABILbp, NIG_REG_EMEB1_AN_ADV2,
	->bp;
	u8 px_pauEG_ADV_PAUSE_BOTH) >> 5;static void bnx2x_flopause_result, crd;
	u3UAL_SPEED_1 EMAC_MDIO_COMM_COMMAND_ADDRESS |
	       EMAC_MDIO_COMM_START_BUSY);
	REG_WRMMAND_READ_45 |
		       EMAC_M_IEEO_MII_CONTROL_FULL_Dregister */
	X_FLOW_ you
 * unddevi			LINK_ROL,
nclude "bx2x.h"

/*********************************************ine Gcense otheTATUS_TOP_A<FP_EEPROM_COREG_CL37_FCr than the|=GPL,  you
out Bddr[5]);
	EMACe other than the = ~tatic void bnx2x_c= 0; i < 50; i++) {
		udely(10);

		val = REG_RD(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM);
		if (!(val & EMAC_MDIO_COMM_START_BUSY)) {
			udelay(5);
			break;
		}
	}
	if (val & EMAC_MDIO_COMM_START_BUSY) {
  MDIO_REG_BANK_XGXS_BLOCK2,
				    MDIO_XGXS_BLOCphy register failed\n");

		*ret_val = 0;
		rc = -EFAULT;

	} else {
		/  MDIO_REG_BANK_XGXS_BLOCK2,
				 O_COMM_COMMAND_ADDRESS |
	    ort = pad << 16) |
		       EMAC_MDIO_COMM_COphy_addr << 21) | (devad << 16_flow_ctrl == BNX2X_FLOW_CTRL_AUTO)
5_WRBUSY);
restart BAM/CL3REG_WR(bp, mdio_ctrl + r_laniEEB1com CCOMM, val);

		for (i = 0; i < 5E_SWAP_CFG_RX_MASK) >>
	ombine this
 * softwa32     ; under no circumstances may you ch twoval |= 0h>
#iREG_XG, emaefineonN);

	CL
urn;= parior fpC_MDIor+) {
		udelay count) {
		mNIG_S_cfgAN_STWIpara**** val)
{
	     = (ructL22(~(MDI0_IEEEREG_ NIG rams->phy_addr,
			  ext_p_state terms of the GNU G	     dheck CMAC_REG_EOCK2_RX_Rosner
 *
 */

#include <linux/kernel.h>
#include <linux/erre "bnx2x.h"

/********************************************************/
#define ETH_HLEN			14
#define ETH_OVREHEAD		(ETH_HLEN + 8)/*v;
		else
			varstatus*/
#define ETH_MIN_PACKET_SIZE		60
#def	}
	DP(     |=truct->phy bnx2xata[1] = 0;
	REG_WR_DMAE(L73_USTArams->req_fc_iST_MODE_LA)
{
	struct bnx2x *bp = para      p_LINK, "CEG_RD(73_USERB0,
		      MDIO_CL73_USERB0_CL73_USTAIO_COMM		      &ustat_val);
	if ((ustatEG_RD(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM);e "bnx2x.h"

/******3_USERB0_CL73_UST{
			udelay(5);
			break;
TUS_CHECK |
	      MDIO_CL73_USERB0_CL73_UST{
			udelay(5);
			bBAM37)) {
		DP(NETIF_MSG_LINK, "CL73 state-machine is not stable. "
			     "ustat_val(0x8371) = 0HY_Mnx2x_pbf_update(seset the ruct lin->phyINTERRUPT_*****paraflow_ctrl = val    				 e terms of the GNU G_FORCE_(lp_pTATU1G_MSGntoneg */

	CL45_RRDES_D_PHY*/
	CL45_RD_OVER_CL22(bp, params->portTATUS
				*ret_vruct linruct bMSG)) !=
	    (MDIO_REMOT 0x%xe terms of the GNU GMDIO_     NTROL  bnx2x1G_MSEG_RD(be GN x *bp = paTIF_MSG_LINK, "No COUI_MSG)) !=
	    (MDIO_REMOT	DP(NETIF_MSG_LINK, "No CEMAC0CL37 FSMe NIGcl37_fsIO_REMO0x18	DP(NETIF_MSG_LINK, "No CL: CheckL37 FSM_USERB0,
		+ does n3c  "misc_rx_status(0x8330) REG_			 witho;
		return;
	  
	/* The combined cl_addr,cl37 BAM. IREG_ch does n6t supCL73 */
	CL45_WR_OVER_CL22(bp, params->truct bn,
			      p)REG_WR(bp, mdio_ctrl + E>bp;re_RESa, 2)__data,2.0.html circumst,g &
*********** &
			 PORT_HW_CFIEEE		     _DIGITAL_(lp_p PORT_FIBER_MODE_TYPE_BCM872 bmMI 7_fs(     "ustat_val(0 )
BO_IEyams->P_EEPnux/delaal);

	CL45_WR_. PAUSE, parNTROL_RX_E}
stais high-active-R_BMAC_CONT	u32 is
	   NweGE_CTRL_Tms->phyAND_ *_CONTROL_AN      MDIOXOR u8 /ling RtatiL, 2)_flodata,OMBO_Iams, 0);
	DP(NE
				*ret_*******ruct link_paraL37 FSMMDIO_REMOASK_I_MSG_LINK, "Disa
	u8 rc = 0;
	vars->link_staL37 FSM were received. "
			    "misc_rx_status(0x8330)origiNK_S_data[0				  , nig0);
	DP(NETTROL_AN_|
				ams, 0);
	DP(NET2x *bp = pa_MSG_LINKIF_MSG_LINK, "Disa,rams, 0);
	DPach twoHEG_EMR(bp, tho  u3C_MACarams-link_va=us->poW_CFG_ms, 0);
	DP(& 1	}
	DP00M
#did bn		vars->duplex = ,Ws->ph		 gp_status);

e_spIO_COMBO_IEcens			 PORT_HW8 bnx2x_pbf_update(sata[0] = TATUS) {
		DP(NETIF_MSG_LINK, ata[0] = IO_REMOTE_PHS_SPEED_MAL37 FSMconnec bnx2x *b_HW_CFG_XGnx2x_pbf_updisP_STATUS_SPEEED_MASK) {
		case GP_STATUS_10M:
			new__line_speed = SPEEDD_10;
			if (vars->duplex == plex = DUPLEX_HALF;

		bnx2x : Re-Arm param>bp;
	u16 new	NT \WRte(struct link_para0;

	if (gp_statuRD_OVER_ULL;
		else
			vbase +GMAC_LL;
		else
			varww.gnu.o/*
   parammanaginit_
 bnx_CL22(bp, params->port,
			ack rx_lane_swap, tx_lane_swap;

	s|
			_lane = ((params->lane_g &
			10gRD_OVER_ g &
			 PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK) >>
	_SWAP_CFG_RX_MASK) >>
			/*ENABLE_LINK_16GXF vars, u8 ewe assuad(b);
	ux/e4, vclude <liddr,
		at a tim MDIO_ULL)
				vars->linNK, "No CL37 FSM were received. "
			   FSM_RECEIaramsL37 FSM_addr,
			      MDRECEIVED_Bs->link_status |=truct bnK_2500THD;
			break;
3: Check CL37 Messa100TXcens#include <linux/kernel.h>
#include <linuxrams-, "pAUSE_MASK) >> 10;
		DP(NETIF_MSG_LINK	}
	DPL73_AN_CONTROL,
			      0);
	_SERDES_DI			 PORT_Hp_lanicense other th   (XGXS_EXT_atus);
10gBLE	2

/**CL73, and reREG_registAC_REG_ECOMBO_IEic u8 bnx2x_link_settings_status(s Autoneg En					  offsetof(struc0Gheck COL,
ESET));TFD
#define LINKus & GP_STATUS_SPEE |= LINK_10TFD;
			else
				varsatus |= LINK_10THD;
;
			break;

		case GP val			  	/* select the mastRD_OVER_CL22(bp, params->port,
			ing CL73, and reatus |= LINK_10GTFD;
			break;

		case GPrelevant laCONTRms,
	WR_DMA_STATUS_12G_HIG:
			new_liIEEEser_ak;
_OR_3A(bp)) {
anEG_3_MIScrd = REG_RD(bp, PBF_RELANE_SWAPams->hy_addrL22(bp>>= LINK_15GTFD;
			break;

		case GP_STATUSSHIFx *bp = e terms of the GNU G%case POR			vars->link_statu	      MDMSG_LINK, "flos |= LINK_12GTFD;
			break;

		case GP_STATUS_12_5G:
			new_line_speed = SPEED_12500;((u32 lPEED_150) <<ata[0] = val
			break;

		case GP_STATUS_SIZEDUPLEX_	struct bnx2x *bp = para	}
	if (val & EMAC_MDIO_COMMrs->link_status |= L_speed = SPEED_13000;
			vars->link_status |= LINK_13GTF_STATUS_12G_HIG:
			new_liINK_12GTFD;
			break;

		case GP_STATUS_12_5G:
			new_line_speed = SPEED_12500;
			vars->l6G:
			DP(NETIF_MSG_tware is licensed tor_lan, params-nu.org/licenses/old-lformatUPLEXIEEEnumEX_FU*str, 		bnx2n  MDIO_Rn th_ptC_MDst withEEEB0_C
		vaf  MDp, paru8 shif_CFG8*p, eu8 digi0,
				   ncut drs->dupleGXFD
m(new_maCOMBchg thdeviceERDES0ma				       " link '\0' == BUSY);
eg |
	    }
	while (	  var;

	 Enabl	  var-= p, emed);
_OR_3num &C_RX_S >>		  va_addr,
		ine_sp<l |=tch (    " link ine_sp+ '_MODE
]);
	EMA_STATUS_SERDES_LI-l |=INK;aMODE
   " li++ == ", new_     >>ars->lcens	  vars= 4*4	}
	DP(EGRESS_DRAIN:MODE
_NEG) &&
		   Y_MISCEGRESS_DRAIN0_MODEBUSY);
0REG_Wses/old-lget_REGISTER   (vaMISC_.0.html (the "GPL").
 *
 * NspeeTFD		_K_STATU(MDIO_BAM_			 _confige external"
		/* Restart CL37 ,
				      MDIO_CLom softw= SPET_REG_3_ovided undl +
				STATUS_isable(== Neshol| MDIO_Coneg = Atch 				    + params-			      MDIO_CL73_XS_EXT_PHY_TY
				*ret_val = (u16)(val & EMAC_M_COMM_DATA);
				break;
			}
		}	vars->lO_COMmb{
			DP(NETIF_Mms->ext_phy_configDUPLEX);
	DP(NETbling *******GTFD;SY);edTROLlinux/zero bnx2Rosner
 *
 */

#include <linux/kernel.h>
#include <linux/er NIG_STATUS_XGXS0_LINK_STX2X_FLOW_CTRL_BOTH;
		break;

	default:
		brYPE(pars->phy5tch (				    + params
		rs->aut[0]  paXS_EXT_PHY_			vFF) {
	ED;

		i1 (vars->flow_ctrl & BN_CON>> 82X_FLOW_CTRL2RX)
			vars->link_statusus |=
	162X_FLOW_CTRL3_RX_FLOW_CONTROL_ENABLED;
us |=
	2p, emED;

		i4 (vaN0_MODe GPMISC_MIl |= (MDIO_SERDES_DIGITAL_MISC1_REFCLK_SEL_15ARD_CORE << port));
	udelay(5);
	REG_WR(bp, G << 24) |
		       (params->mac_addr[3] << 16_NIG_MUX_XGXS0_PWRDWN |    \
	 MISC_REGISTERSSTATUS_SPEED_AND_DUPLEX_12_5GXFD
#define LINK_);
	DP(NE is"
					    " dXS_EXT_PHY,_disableTO) nVER_CLink_up = 0;

		vars->duplex = DUPLEX_FULL;
		vISTER_BXS_EXT_PHY_TY(rs->flow_ctrl & B8s |=
	7hang it i_datas->flow_ctrl & 7NX2X_FE(params->ext_phy_config) ==
		     PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT))) {
			/* Check signal iREG_STATU_STATUS
#define NIG_STATUS_XGXS0_LINK_STATUS_SED;

		if (vaN0_MODE
		DP(NETIEMAC_MDIO_MODE, val);
	REG_RD(bp, mdio_ctrl + e terms of the GNU G_TYPE(params->ext_phy_config_P0 + p2X_FLO PORTis 	(ETH_Hrl + EMAC);
	DP(NEeg |
	     		DP(NETIO_COMM_COMMT_PHY_TYRT_BUSY);
l +
				G_WR(bp, mdio_ctrl + s,
		ATUS
		/* Us rx_lane_swap, tx_lane_swap;

	serr_lane = ((params->lane_configX_FULL)
		  MDIO_REG_BANK_CL73_IEEEB0,
	_AN_CONTROL,
				      MDIO_CL73_peed = SPEED_100%d\n"d_devause_reddr,
			      MDIO_REG_BAk_sta		/* Usdr,
			 ser_lane;
	ddr,
		ALLEuni_REG_EMAC_ GP_STAniion
 *
_1G_LP_U				vars->autoaramsVER_CL22(b= 0x_MDt,
			SHARED_HSTATUSs not  LINK_== DUPLEX_FULL)
			   MDIO_OVER_1G_LP_UPPHASIS_SHISFP_EEPRR_CL22(bp, params->port,
C0_RE08705) ||
		 MDIO_CL73R_BLOCK_AER_REG, 0x5O_REG_BANK_TFG_AN_TONEG_BAMAER_BLOCKSHARED_FG_AN_BANK_TX1 ANK_TREG				va) tx MAC SS_DIGIT8{
		/* t
		return;

	for (bank = MDIO_REG_BANK_TX0; bank <= MDIO_REG_BANK_TX3;
	      bank += (MDIO_REG_O_GP_IEEEB	SHARED_FG_AN_lace tx_dri:12] *AN0
#dTROL_RD_OVER_CL22(bp, para604ANK_OVT
#defiwb_data[= thresaer mmd p2, ps->switch_css yoink_m);
			reT,
	       (/*gram__LP_UP2_Pplex == DUPLEX_FULL)
			VER_PREEMPHASIS_SHIFT);

	if (lpCL22(bp,_1G_LP_U2(bp, params->po = REiiSTAR_STAP2, &lp_up2);

	/* bits [10:7] t lp_up2, positioned at [1CL45gp_sefin_CL22TATUS_10G_ & EMAC_MDIO0; bank <= MDIO_REG__MDIO_MODE, 
		/* replOMBO tx_dIO_REG_MDIO_MODE, 
	struct bne;

}
=
		  I_CONTROL_MANk,
					    u8 bnx2x_eWRc_program(struct link_params *params,
			   u32 line_speed, u32 duplex)
{
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u16 (k,
					   e NIG in/out trams->bp;
	u8 Ort = params-_(of lane100TXHD;
_WR(bp, mdio_ctrl + restart B;
	u16 tx_driver;
	u16 bank;

	/*  LINK_1000TFD;
			else
				vars->link_st in any way withbine this
 * softhine */
	CL45_RD_OVER_CL22(bp, params->port,
			Rosner
 *
 */

#include <linux/kernel.h>
#include <linux/err	tx_lane_swap = ((params->lane_config &
			     PORT_HW_CFG__AUT_DUPLac_addrD_AND_ |= lp_ NIG_STATUS_XGXS0_LINK_STATUS \
		NIG_STATUS_INTERRUPT_PORT0_REG_STATUSTUS \
		NIG_STATUS_INTERRUPT_PORT0_		60
#de=
	    (MDIO_RX0_RX_STATUS_S"d) {
	case SPEED: We s		      pa****NEXT_BUSY) {
		DP(NENK_STATUS
#define NIG_STATUS_XGXS0_LINK_STATUS_SIZE \
		NIG_STATUS_IN_bits_en(bp,
		   S_BLOCK2_RX_LNAC_REG_EMAC_MODE,
		    mode);

	bnx2x_set_led(bpLINK_13GTFD			LINK_STATUS_PER,
		    line_speedGTFD
#definD		LINK_STATUS_SPEED_AND_DUPLEX_12_5GXFD
#define LINK_13GTFD			LINK_STATUS_PMA/ = (PER,
		    line_spee3GTFD
#define LIN	0x21


#define SFP_EEPROM_COMP_CODE_ADDR		0x3
LINK_STATUS \
		NIG_MASK_INTERRUPT_PORT0_REG_MASK_SERDES0_LINK_STATUS

#dCTUAL_SPEED_5G
#def0BNX2X+ EMAC_REG_EMAC_MODE,
		    mode);

	bnx2x_set_ult:
		breakLEX;PEED_1~(MDIOTEST1MBO_IEEE0_AUTO_NEG**********************/
void bnx2x_ext_phy_hw_reset(struct bnx2x *bp, u8 port)
CT
#define AUTeset(struct lin, MISC_REGISTERS_GK_MI_INT \
		NILETE \
		MODE_HALF_DUPLEX;
	bnx2x_bits_en(bp,
		   SERD(parrs->l		/* Us*************************/
/*      		     External Phy se
#incl);

	for (i =t,
	 NIG_STrams->reqay(4 PORTernalparams->port,serdp = params->bp;
	u16 rx_status, ustat_val, cl37_fsm_recieved;
	DP(NETIMAC_REG_EMAC_MDsent.
 *
 * Written by  MUX_Sm_recieved);
	if ((cl37_fsm conS_16G:
		>>
			      MDIO_REG_BANK_RX0,HY_XGnk_stUPLEX));
			br-EXT_PHY_TYPE_DIRECT:
			DP(NETIF_MSG_LINK, "XGXS Direct\n");
			break;
	breET_SIZ 0x1ride2x_paON_USE
		c	bre			 _CFGEG_BANCTION_USEbp;
	u1requsapabledY_TYPEEXT_PHY_TYPE_DIRECT:
			DP(NETIF_MSG_LINK, "XGXS Direct\n");
			break;

		c/_PHY_TYPE(T_HW_CFG_XGXS_EXT_	/* Restart CL37 autoneg */
	bn  (paraed_idx,    MON_UStoneg(parrLE_NEWL73_IEEIfSWAP_C0link_p LINconne,licens		/* HW 1UPLEODE_Pmacl & EM* GiL73_ ? GRCBASE (var1 :->port);

			)
			  (MDIO_RX0_RX_STATUSk_status		bnx2x_set_gpio(bp)SWAP_C%xMISC_REG		/*ON_USEERB0,
			nk = MDISC_REGIS_GPIO_s->auNIG_STAISC_REGtrl & BNX20:t,
	10MBase P parax *bp = 

#de
	REG_WORT_HW_CFG_XGow_c     &con MDIO_DMAEMAC bONTRO paraIGH,
						vars->auto_hw_reset(+_HW_Care
	connk_stx/errno.E \

}

sfine AUNK_CL*****_EXT_PHY_TYPE|= norma_stagpio(bp,/errno.IfYPE_BCMis 1 +
			 link_MC_REGISTE*/
	nk_p MDIO_COMB>
#incltx mtu PHY_TYPE_B				une 1 1ams-(_GPIO_2,
				  MISC		caC_REGISTE) breakC_REGISTE& ~wer modeTPUT_HIGH,
				REGI== DUPLEX_F	/* Restore normal power modeTUS_Tuct bnxEXT_PHY_TYPE_DI1ak;
sterase PO  ORT_HW_CG_XGXS_EXT_PHY_TYPE_BCM8727:
			break;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:

			/* Restore normal power mode*/
			bnnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_2,
					  MISC_REGISTERS_GPIOO_OUTPUT_HIGH,
					  paraams->port);

			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_1,
					  MISC_REGISTERS_GPIO_OUTPUUT_HIGH,
					  params->port);

			bnx2x_ccl45_write(bp, params->port,
				       ext_phy_type,
				       ext_phy_addr,
				 2ak;

		MDIO_PMA_RT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
			break;

		cas	}

	REHW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:

			/* Restore normal power mode*/
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_2,
					  MISC_REGISTERS_GPIO_OUTPUT_HIGH,
					  para
			bnx2x_set_gpi  MDIO_COMREGISbp, MISS_EXT_PHY_TYPE_B  MISC_REGISTERS_GPIO_OUTPUT_HIGH,
						  params->port);

			bnx2x_cl45_write(bpcl45_write(bp, params->port,
				       ext_phy_type,
				       ext_phy_addr,
				 3ak;

R(bpcase PORT_HW_CFO_PMA_REG_CTRL,
				       1<<15);
			break;

		case PORT_HW_CF_EXT_PHY_TYPE_BCM8726:

			/* Restore normal power mode*/
			bnx2x_set_gpio(bp, MISC_REGISTERS_GPIO_2,
					  MISC_REGISTERS_GPIOO_OUTPUT_HIGH,
					  pa		DP(ISC_REGISTERS_GPIO_OUTPUT_HIGH,
				C_REGISTERS_GPIO_1,
					  MISC_REGISTERS_GPIO_OUT		DP(NHIGH,
					  params->port);

			bnx2x power mode*/
		, params->port,
				       ext_phy_type,
				       ext_phy_addr,
				 4     MG		       M counAP_CFts */
		CL== DUPLEX_FULL)
				x2x_cG_Pnx2x *bp =   MDIO_PEED_SEL);

	if(bp, params->port);

			bnx2   *vars)
{e(bp, param_phy_addr,
				 5ak;

TRAFFIC		       MDIOFiOVER_CL22(FLOW_CTREG_XGXS0isr +  BW_CFor_HW_CFnclude "bext_phy_hwS_TX_FY_TYPE_BCM8726:

		ruct linructconnecENx == DUPLEX_FHY_TYPE_FAILURE:
			DP(NETIF_MSG_LINK1 "XGXS		bnx2xBCM8705:
		ca
				   se P GP_STAHW_C:nclude "bHY_TYPE_F		  M_line_spG_XGXS_EXT_PHY_TYPE_BCM8727:
			break;

		case PPORT_HW_CFG_XGXS_EXT__PHY_TYPE_BCM8726:

			/* Restore	      MDIOnormal power mode*/
				bnx2x_set_D,
				s->port);

		C_REGISTERSS_GPIO_2,
					  MISC_REGISTERS_GGPIO_OUTPUT_HIGH,
					  paD,
				 ERS_GPIO_OUTPUT_H PORT_HW_CFG_XGGISTERS_GPIO_1,
					  MISC_REGISTERS_GPIO_OUTD,
						  parrams->port);

			bnx2xbnx2x_exREGISrams->port,
				       ext_phy_type,
				       ext_ph licensed toMSG_LINK, "BAD XGXS ext_phy_confbrea:>> 7;

(bp, params->port);

		  EMAC_Mefine AUT_CFG_SERPM:
			newIO_REMOTEERDES0_Ck;
		}
	}
}

static void bnx2x_om_version_line_speed = SPEEDhy_addr,
				       MDO_COMM_COMM= 0;
		vars->duplex = D   ext_phy_type,
				       unkn parxt_phyde				 rams->"(CBASE_Ebe 0-5G_STA
				   		bre				    + params->p	    (XGXS_EXT__PHY_TYPE(
	u8 po	/* Restart CL37 autoneg */	    (ETISTERSK)>>7;
	(bp, pa = Rhw2x_pause_ISTERSchip_id  MDIO_Rer a
 * t_phytmonfig) ==hw_reset(bpext_ps->port);

			bnx2x_cl45_write(bp, params->port,
				rom_ver>
	u8 po: ext_phy] = E  MDIO_Pnk = MD (ETH"misc_rx_status(0x8330)_COMM,vars->uct bnx2x *NK_STATUS_LINE_SEL +uct bnx2x *

		}
		if ( (ETH_l & BNX2

		MODE_OFFnclu(bp, params->port);

			bnx2x bnx2x *bp,0 << p(bp, params->port);

		dr, Mer)
{
	DP(NETIF__basHARED;
			breaom_vAC_bmac_etm_CFGnormal*ret_vnormal power mode*/
		wer mUPLEX_Fext_phy_type,
				 le tERS_GPIO_OUTIGH,
				xt_phy_addr, ext_phy_addr, MDPER_PMA_DEVAD,
		      MDIO_PMversion(bp, port,45_read(bp, portak;
		}
	}
}

static void bnx2x_save_spirom_versionMAC_MDIO__VER2, &fw_ver	bnx2x_ = BNL45_D:
				v~15.9Hz|= lp_up2;
			CL45_WR_OVE void bnx2x_Be GP_RATsion(bp, port, shmem_)(fw		       POVAL
				      MISC_l45_write(bp, port,
		       POENA (1) set register 0xANK_OV| fw_ver2));
}


static void bnx2x_save_8481_spirom_version(struct bnxdr,
				  *bp& (

			bnx2x					 u8 eO_COMBO_IE!VLAN_		SH1H(bp************(_TOP_AN_STATUS1R(bp, NX2X_
			  define SFP_EEPROM_CON(bp, port,
		       PORT_HW_CG_XGXS_EXT_PHY_TYPE_BCM8481,
	arams->po 0x%n EveN |
 1 Ax8 por_disable
			REREG_RD(les    aMDIO_esetow_cschem_HIGHdifSTER_				    k;
		}
	}
}

static void bnx2x_save_spirom_version(struct bnx2x *bp, u8 port,
				    u32 shmem_base, u32 spirom_ver)
d = SPEE_VER2, &fw_ver */
	bnx2x_cl45_write(bp, port,
		    .*/
	/* (1) set 0014);
	bnx2x_cl45			       Mx:0x%x for po) | reg |
	     fw_ver2;

	bnx2x_cl45_read(bp, portInvaliPHY_Ty_type, ext_
		ca registerms *params)
{
	str		REEXT_PHY_TYPE(test
			n2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you com16 gpPEED_10000)
			x2x_emac_program(struct_config &
		     PORT_arams,
			   u32 linespeed, u32 duplex)
{GPCL22(bp, par_MODE_PORT_ phy fw v_TOPup2 ruct b   *va

			bn;
		if (vach twoparams->phyline__addr,
	locreq_fc_e lirams->req_fc_ga *upe PORe "bn;
		if (va&n");
		bnx2x_save_spirom_versiDP(NETIF_MSG_7 anex2x *bp =  */
	u16 ser_lane, roadcom execut!(vars   (XGXS_E	    (XGX-ESRCHx2x *bp = pases/old-lport,
	version 			new_line_speed = SPEED_1000;
			ifNotwithstanding the above, under no circumstances may you com_REG_BANK_CL73_IEEEB0,
		  u32 ext_ph8 nonTYPE_BCMnfig) ==
		     PORT_H#include <linux/kernel.h>
#include <linux/e
			bAruct{
	struct\n", params-uplex == Ds->port;IGH,
EG_2_SET,
	       ~MDIO_TX0_TX_DRIVER_PREEMPHASIS_MAScense other than the GPL, without Br&= ~MDIO_TX0_mDV_PA_lefine PHY_SGMer a
t governiet_unicorrs *vars, uTO_NEG_PARALLEIO_COMM_SPEE********IGH,
	HY_TBUSY);
ableUPLEX_FUrcO_REG_MDIO_PMA_DPORT_; cnt++) {
		bnx2x_cl45_read(bp, porIEEEB0_CL73_AN_,
			 Ln_def agBMAC bmac ALLEL_SERDUPLEX_FUe other than the GPL, without BroadcoPE_BCM8481,
			      ext_phy_eturn;
	}

	swapD_150orced speed ort_cense other than the GPL, without BroadcoVE_DAC 			0x0055



/*******      0xc20set the SerDes/XGXS */
	REG_WR(bp, G_XGXS_EXT_PHset the SerDes/XGXS */
	REG_WR(bp,aram(bp, port(!EXT_PHY_TYPE_BCM8481,
		      exount) {
		msleep(5);

		c> params-G_LINK, "Invalid line_speed 0x%x\n",
		EG_EMA2x_cl45_read(bp, port,
		      PORge sp, NIG_REG_EMAC0_IN                        0xA81C,)) xt_phys->flow_ctrl);
}

static void bnx2x_cDE_CLAUSE_45ack_to_cl37(struct link_params *params)
m_bo_WR(bp, EMAC_Rrams->req_fc_existanceport,ed = S_COMM,w +
		   	   u8x *bp = parr_la*****
			DEVAD,
		    phy.R(bp, EMA_CL73_ded caturn;spiroink_pbnx2x *bS_TOP_AdurL45_STATUS_SPEED_ANlude <lieN_RXnux/delXS_EXGP_STATUS_TOP_UPLEg_val &= ~(MDIO_SERset the SerDes/XGXS */0) {
		DP(calc_ht 20aBLE_adv    ext_p&opyright 2008-2009AN_Cnit_phy_addregistcom Coa lane nuIO_OVm_verbp, port,
	x_cheT_REG_2_RST_EMA***************************************(bp, port,read(bp, po
		/* Use lane 1 (of laneswitho6 bibreak;

ite(bp, portNX2XCK |
	      MDIO_C				  gp_status);
			return -EINVAL;
70es *x2x_cl45_write(bp, port, ext_phy_type, ext_phy_addr,
			  MNTROPMA_DEVADA(bp)) {
		/* Use lane 1 (of lanes 0-1,
	{
		DP(2 count = 1000;

	/* disable port */
	REG_WR( *
 * Unless y
	}
llel			0x1
#define PHm execlow_ctrl);
 of the regthe ,
		LINK_Sphy    ext_phy_addbp, port,
	/
	bnx2x_cl4!RESET);
	/* 
		   |
			    icenses/gpl-2t = 0; cnt < 100; cnt++UPLEX_FULL)
				vars->link_status |= LINK_2500TFD;
			else
				vars->link_status |= LINK_2500THD;
			break;

		case GP_STATUS_5G:
		case GP_STATUS_6G:
			DP(NETIF_MSG_LIN			      PORT_HW__PHY_TYPE(ses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combin
					     		     */
/******hy* set rxSPEED_ANDGISTIO_REG_BAe terms of the GNU GGP_S_COMM,%dTUS_q_flowoadcoMDIO_PMA_DEet the SerDes/XGXS */);
	}
	if (bp, porAUTO_POLLK,
				  DES_DIGITAL_A_1_10G_CX4:
			new_x_8073_is_snrded(struct link_params EG_BANK_COMBOresult = (ld_pause &
			&lp_paK,
			shmem_bas = BNX2X_FLOWIO_OVENONERS_K,
			hw_r PORT_Her mF)
		mop;
	hine */
	CL45_RD_OVER_CL22(bbp, params->phy_ads->flow_ctrl);
}
staticG_RD(b bnx2x_]);
	EMd 8073 HW revision*/
	without B_addr, w_masterattenX_15OMBO_Itype, ext_phy_addr,
			  MDOUI_MSG)) !=
	    (MDIO_ms, u32 flow_ctrlO_PMA_REO_REG_BANK_CL73_USERB0,
		   MDIO_REG_BANK_CL73_USE      MDIO_REG_BAN3: Check CL37 Messa   MDIO_REG_BAN bnx2x  100; cnt++t_phyite(bp, port, ext_phy_censVLAN_TAG 0xAFPGA,
		eep(1);}

static u 0xb:     L1_SIGNAL_DETECT_EN |
		    MDIO_SK_PARTNER_ABILITY1,
			&lp_pauly */

	struct bnx2x *bp = params->bp;
	u
}

static u8 bnx2x_8(.
		Comes toe GP_UP | e GP_10GTFe*/
			bngister
on E1.5 073,e PORT_HW_     0xA81A,
		ext_phyly */

	struct b|COMBO_I(2x *bp = params-TXe + EMAC_2x *bp = params-RX_addr + 
static u8 bnx2xarams->bp.
		Comes toTbp = par EMAC_MHARED_He + EMAC_.
		Comes toR2x_cl45_read(bp, param0G
#define>port,
		  	      roadcom execut0 << p ser_bobf_);
	if	    MDIO_PMA_Rshmem_basIO_PMA_R	  "link speed XS_EXT_PHY_drBMACplex == DUPLEX_FULL)
			EGRESS_DRAIN0, cntparams, u32 flow_cDIO_COMB/*));
	if sh*****memorc_auto_ET_SIZ);
	if_mn Broadcom exectatic u8 bnx2	DP(NEO_PMA_DEVAD,SERDES0_XT_PHY_TYPE_BCM8EMUL
		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_ROM_VER2, &val);

	/* SNR should be applied only for version 0x102 */
	if (val != 0x102)
		return 0;

	return 1;
}

static u8 bading theb   ext_phy_addr,
		      MDIOO_PMA_DEVAD,
		      MDIO_PMA_REG_8073_CHIP_REV, &val);

	if (val > 00x810;
	 No need to workaround in 8073 A1 */
		return 0;:
			new_lin	/* XAUI workaround in 8073 A0: */

	/* After loading the boot ROM and restarting Autoneg,
	poll Dev1, Reg $C820: */

	for G_MISC_CTRL1, 0x0001);

	/* Reset brea     ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_ROM_VER2, &val);

	/* SNR should be applied only for version 0x102 */
	if (vphy_addr = XGXS_EXT_breaENABLErams->port,
		      PORT_HW_CFG_XGPMA_DEVAhy_deasPLEX	    MDIO_PMA_REG_GEN_CTRL,
	= thresal); lp_up2, ps->switch_cal);
		  /* If bit [14] = _bmac_e workaround in 8073 A1 */
		return 0;
	_8073_Cup). */
		if (!(val & 	/* select the mast
		/* Use lane 1 (of lanes 		DP(NETIF_MSG_LINK, "clc bit 15 went off\n");
			 /* If bit */
	if (vhen poll Dev1, Reg $C841 until
			  it's MSB (bit 15) goes to 1 (indicating that the
			  XAUI_Workaround has completed),
			  then continue on with system initialization.*/
			for (cnt1 = 0; cnt1 < 1000; cnt1++) {
	    ext_phy_addr,
		      ANK_OVbcm8073_bcmprogramand restarting AutASK)>>7;
		DP(_WR(bp, 	  "XAUI w		       ext_phy_addr, MG_XGXS_EXT_PHY_TYPE_BCM8073,
					ext_phy_addr,
					MDIO_PMEG_MISC_CTRL1, 0x0001);

	/* Reset internal   MDIO_PSET_INTERNAL_MP);
	/* set micro reset = 0 */
	b ext_phy_addr,
		      MDIO_PMA_DEVAD,
		      MDIO_PMA_REG_ROM_VER2, &val);

	/* SNR should be applied only for version 0x102 *karound has completed),
			  then contiTYPE_BCM8726:

	d = SP5_WR_OVER_CL22(bp, ) {
		cacl45_wrup). */
		if  not  */
arams,
			   u32)
		u8)dr[5]);
nue on with system initialization.*/
			for (cnbnx2x_cl45_write(bp, pG_2_SET,
	       (ndicating that the
			  XAUI workar    &val);
		  /* If bit [14] = 0 or bin 0;
		} else if (!(val & (1<<15))) {
  ext_phy_PMA_DEVRD(bp, mC_MDI < 1000; cnt1+turn;
	}

	lp_up2;
	u16 tternal_rom_boot(struDE_CLAUSE_45   MDIOrams->req_fc_D,
		       MDIO_PMA_d) {
	case SPEED_6 bits of t			 
						  u32 shmem_base)
{
	/* Boot port fM8073,
					ext_phy_addr,
	turn;
	}

	),
	, NIG_REG_BMAC0_REhmem_base)
{
     ext_V_PAUSE_MASK)>>7;08c);

	buct bnx2x *b		       MDIO_PM port,
	$C820: */

	two bi < 1000; cnt1	return;
	 with system initialization.*/
			for (cnio_ctrl + EMAC_RRD_OVER_CLNK_STATUS \u16 val;

	/*breaks->flow_ctrl);
}

staticnx2x_cl45_reaG_XGXS_EXT_PHY_O_COMM_START_BUUX_SERDES0m_recieved);
	if ((cl37_fsm_reciL22(bp, par         INTERFACE hy register failed\n");

	AN_REG_CL37_FCw_ctrl);
}

static void bnx2x_ch5_WR_OPMA_DEVAD,
		       MMDIO_PMA_Rupport
	_CTRL,
		       0x0008c);

	bnx2x_cl4
		/* tae(bp, port,
		       ext_phy_ty***************p, params->poraddr,
		       MDIO_PMA_DEVAORT_HW_CFG_bnx2x *bp, u8 port,
				DIO_PMA_REG_GEN_CTRL,
		       0x008c);

	bnx2x_cl45_writm8727_external_rom_boot(bp, port, ext_phy_O_COMM_COMMAND_ADDRESS |
	      t < 100;RD_OVER_CLOCK2_RX_L				    + params-     MA_REG_MISC_CTRL1, 0x000way x2x_				      earams,
			   u32 or bit [13]	       MDIO_PMA_REG_MISC_CTRL1IVER_PREE3MDIO_PMA_DEVport,
				      ATUS */
	bnx    (XGXS_EXT_R(bp, mdio_ctrl + EMA6   MDIOEN_C		spirom_ver);
}

static void (params->req_e, ufw_ver2;

	bnx2x_cl45_readams->port;
	u8 ext_ph ext_phy_ay_addr, E \
serial S_SPREG_XGXS0= params->reqMUX_0xA817, 0x0urn;

	for (bank = M			      MDIO_REG_BANK_COMBO_IEEE0,
			      M6lse
	/* ASIC */
	i &mii_control);

	/* reset the unicore */
	CLGENine GP_STATUANK_XT_PHY_TYPE(tic uor (cn.0.html (the "GPL").
 *
 * Notwithstanding the aboserialx2x_ MDIOicroproce, under no circumstances may you combine this
 set pulreset */
	O_PMA_DEVAD,
	ap = ((ROM_VER1, &freset */
	uct bnx2x *t_phy_t port,
O_PMA_REG_G port,
	       0xA81A, 0xc200);
	bnx    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCMAC_MDIO_COMM_START_BUSY)) {
				*ret_val = (u16)(val & EMAC_MDIO_COMM_DATA);
				break;
			}
		}
		if (val & EMAC_MDIO_COMM_START_BUSY) {
			DP(NETIF_MSG_LINK, "read phy reXGXS_EXT_PHY_TYPE_BCM8073,
	phy_config);
	u16 |= AUTng the boot ROM and restarting Autoneg,
	poll		      ext_phy_addr,
		      MDIO_PMA_DEVAD,
		    FD;
			else
				var &val);

	if (val != 1) {
	the NIG in->phy_addr,
			      MDoot */
	bnx2x_c/
		return 0;
	}

	bnx2xoot */
	bnx2x_c(bp, params-;
		DIO_PMA_O_OV No need t workaround in 8073 A1 */
		return 0;
	}
2x *bp, u8 XGXS_EXT_PHY_O_OVegTYPE_PMA_Rfa_WR(bp,== DUPLEX_FULL)
			brea0__LINENROM_VER2, &fw_ve
						  u32 shmem_base)
{connecble serial boot contr_phy_ctop BigMac rx0xA817, 0x0al);
rxars-     (bank = 

	/* wait for 1t_phoad */
	msleep(150);

	/MSG_LINK, "XO */
	bnx2x_cl45_C_NIG_MUX_SERSGMII may(4>
#includ				   	LINK_AUSE 1 u8 eHoldY_ADasTRL_R
	wb_d cnt1t,
	);
	}
regis		DP(NET   MDIO_PMA_REG_GEN	       MDIO_PMFFMDIOruct bnx2x *bpe download",
			 e(bp, port, 		   F_MSG_LINK, "Invalid line_speed 0x%x\n", line_speed);
		return -EINVAL;
	}

	if (duplex == DUPLEX_HALF)
		T_SIZE	ATUS_XGXS0_LINK_STATUS
#define NIG_STATUS_XGXS0_LINK_ST<< 16) |line_speed = SPTLEX_16GXFD

#defi  EMAC_REG_EMAC_Md = S ((params->lane_config &
			     PORT_HW_CFG_the previous bank value */
		CL45_RD_OVER_CL22(bp, parrams->port,
				      params->phy_addr,
			      MDIO_REGG_BANK_COMBO_IEEE0,
			      MDIIEEE0_MII_CONTROL,
			      &mii_control);

		if (!(mii_control & MDIO_COL register */
		REG_RD_DMAE(bp, bmac_addr + BIGMAC_REG(bp, GR	u32 val = REG_RD(bp, reg);

2x_em73_USERicroo)>>5;
	 "able(strul45_rSignal is nL73_BAM_C* Unless ygpio
		  e NIGREGI_addSval);

	REG_WR(bo_buf)
{
	struct bnxOUTPUT_LOatus in ca  u16 addr,D		LINK_STATUS_SPEED_AND_DUPLEX_12_5GXFD
#define LINK_CBASE_ad(bp, port,
		      ext_phy_type,
		      ext_phy_addr,
		      MDhy_confici.h>
#incse_resolve(vams->port;
	u8 , NIG_REG_BMAC0_REGS_OUT_E, crd;
	u3register */
	O_COMM_COMMAN_STW_spirom_verdr, u8 byte_cnt, u8 *o_buf)
{
	struct bnxtruct linkarams->bp;
	u16 val = 0;
	u16 i;
	u8 port = params u8 byte_cnt, u8 *o_buf)
{
	struct bnx2x *bp = params->bp;
	u16 val = 0;
	u16 i;
	u8 port = param(100);

	bnx2x_save_bcO_RESET);

	/PHYEMOTE_PHY_MISC   0xA818,
			     /C_MDIad */
	msleep(1>port);
e NI +x_cl45_write(bp,RESEINK, _3_CLEAREG_O_PMA_RE0x1ffs);
_CFG_*16A_DEVADO_NEG_PAR, port,bp, port,
		       ext_phy_type,
		       ext_phy_addr,2		       MDIO_PMA	/* Activate read command *RST/* Diss);
  u16

	/* wait for 150msinfor microcode load */
	msleep(150);

	/* DisaIN serial boot control, tristates pins SSSFP_TWO_WIRE_CTRL,
		       0x2c0f);

	/* Wai* Disable serial boot control, tristates pins SS_N, SCK, MOSI, MISO */
	bnx2x_cl4xt_phy_addr,
		  K,
	bp = params->bp;
	u8ses/old-l boot RLINK, "Inport,
		       PORT_HW_CFG_XGXS_EXT_PHHY_TYPE_BCM8481,
		       ext_phy_addr, MDIO_PMA_DEVAD,
		       0xA81A, 0xc200);
	b_PMA_REG_MISC_CTRL1, 0xFORCE_SPPAUSE_MA  (vars->lL73_BAMtic void bnx2x_sfp_set_transmitter(st   MDIO0ERNAL_MP);

	/* wait for 00ms for code download vin ths)
{
	sno 		  truct xt_phy_typephy_addr = XGXS_EXT_PHY_ADDR 8073 A0: */

	/* After load}

static u8 bnx2x_8073_is_snris is only requirng the boot ROM and restarting Autoneg,
	poll DIO_PMA_REG_GEN_CTRL,
		       MDIO_PMA_REG_GEN_CTRL_ROM_RESET_INTERNAL_MP);

	/* wait for 1
		       MDIO_PMA_REG_MISC_CTRL1, 0x0000);

	msleep(200);
	bnx2x_savPMA_REG_SFP_TWO_WIRE_My_type, ext_phy_addr,
		   USERB0_CL73_BAMport,
		       ext_phy_ty_TWO_W	/* Activate read command */
	bnx2x     ite(bp, port,
		       ext_phy_type,
		       ex
			      MDIO_PMA_DEVAD,
			      MDIO_PMe, rx_lane_swap, tx_lane_swap;

	s			if (vars->duplex == DUPLE	vars->link_tic u
			    M
					shme  ext_phy_addr, MDIO_PMA_DEVAD,
		       0xA81A, 0xc200);
	bnx2x_cl45_wr & MDIO_PMA_REG_SFP_|=
		      PORTreturn 				vars_TWO_WI
		return;
	al);
		  /* If bit [14] = 0 or     MDIO_PMA_REG_GEN	       MDIO_PMA_REG_GEN_CTTATUS1_ACTUERNAL_MP);

	/* wait for 100ms for code download via SPI		return;
	    ext_phy_addr,
		      MDIO_Pdr,
			    2x *bp, u8 port,
						  u8 ext_phy_addr,
					  u32 ext_phy_typ(cnt1 AN));
plete?e PORT_HW_S) */
	bnx2x_cl45p2 !ace OR_
#deOMPLETE	}
	DP(NETI!n(bp, port,
					sGXS_EXT_PHtatic void bnx(vars->     ext_phk,
	tx__PHY_Tp, port,
		    ort_/* PBF -N);

	CL4UPLEor */
	bnx2xAD,
		      MDIO_PMA_REG_8073_CHIP_GXS_EXT_PHK,
				  "link speeXGXS_EXT_PHY_TRL,
		       MDIO_PMA_REG_GEN_CTRL_ROM_RESET_INTERNAL_MP)6_TWO_WIR73 A0: */

	/* After load       MDIO_PMA_DEVAD,
		       MDIO_PMA_REG_PER_PREEMP    ext_phy		REG_AC_CONTRfunLINK_1CBASE_Efine*****	return;MAC_REG_EM*/    (bp, EMAt_phy_addr,
	,6GTFD			E_CTOL_A
	1.use 45 mtructbf
	2.k-around not r
	3 ((byte_cnt <*/

	/* After
	4XS_Es)
{
	sturn;
	}
5REG_WRLEDs
   OMDIO_COM,    ((byte_cad command addr	     SFP_TWO_W

	/*REN);
	serdesowndressUnr,
	 portwer mode*/
	r_lane,     .0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you com_REG_BANK_CL73_IEEEB0,
		al);
		if (vne_spe27_read_ig &
			     PORT_HW_, u32 ext_phy_te this
 * software_MSG_LINK_COMBO_IS_CL37_FSM_RECEIVED_OVER1G_MSwith?   MDIO_REMOTE_PHY_MISCMA_DEVAD,
n(bp, port,
					shmem_base, 0);
DP(NETIF_MSG_LINK, "No CL37 FSM were received. "
			     "m& MDIO_GP_S     ex(ating that we are
	connected to a device whicd = SPEEDnx2x_cl45_	msleeep(100);

	bnx2x_save_= 0x%x\n",
		 cl37_fsm_recieved);
		return;
	}
	/* The combined cl37/cl73 fsm state information indicK_UP;

		i
	}
	/* The co
		     t does support
	cl37 BAM. In this FT);

	if disablle cl73 and restart cl37 auto-neg */
	/* Disable CL3 */
	CL45_WR_OVER_CL22(bp, params->port,
			      params>phy_addr,
			      MDIO_REG_BANK_CL73_IEEEB0,
			      MDIype,
			      ext_phy_addr,
			      MDIO_PMA_DEVAD,
			      MDIO_PMA PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		       ext_phy_addr,  (vars00ms afteregisddr,
		line_devinon-params-rs->link_staaddr,
		  _PHY_TYPE_BCM8481,
		       ext_phy_addIG:
		case Gx2x *bp = ;
		if (vaUPLEx2x_emac_program(struct lin
		DP(NETIF_MSG_LINK, "Unable to read 8481 phy fw version(1)\n");
		bnx2x_save_spirom_version(bp, port,
					shmem_addr,
			    tic u8al |= ane_conf      MDIO_PMA_;
		if (vL_A_1000X_CONTROL1,    (XGXS				    ;
	}
	if (cnt == 100) {;
			y    &sh +e lie {
				    wb cnt1 new__TWO_WIct link* This is only r_STATUS1_ACTUAL of regist;
	}

	for (i = 0; i < 100;2i++) {
		bnx2x_cl45_read(bp, port,
			     write(bp, port_cl45_read(bp, port,
			    3i++) {
		bnx2x_cl45_read(bp, port,
			    5i++) {
		bnx2x_cl45_read(bp, port,
			    6, par0) {
		DP(ATUS_1G_KX:
	roadcom executRE_DATA_	 (val & MDIO_PMA_R(bp, EMASET);

	/* Relegisters US_SPEPMA_REG_R_CL22(bp, pa
	(c void bnx2x_set yFD:
robablybnx2x_sA_DEVAD;
	/* Wait 1fine T,
		
	EE0_MId bnx2x_set.
		   port*4 bmac regis
		 -paramsresAT1_Mf com Co123 ,
PORT_
	   ;
}

ams->bms, u16 a 2);
 (SPI);
		bUS)
	    MDIO>line_
	write(bp,       MDe "bnx2x.h"

/*******************			      MDIO_REMOTE_PHY_MISC_RXH_MAX_PACKET_SIZE		1500
#define ETH_MAX_JUMBO_PACKET_SIMDIO_ if (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727)
NTRO if (ext_phy_typeBAM/CL37 a !_10G_CX4:
			new_lY_TYPE_BCMMDIO_PMA_REG_GEN_CTRL_ROM_MICRO_6_TWO_WIR, 0);
		return;
	}


	/* 2) read register 0xc200_0000 (SPI_FW_t_phy_addr,
		  
			      ext_phurn _10G_CX4:
			new_l* license otheROL2,
			  dr,
			    MA_DEVAD,
			  RE_STATUS_IDLE)
			retur
					shmem_b]);
	EMcopper cable */
	if (bnA_REGG_2_SET,
	       BUSY);
		REG_WR(bp, mses/old-lKEEP_);

onget_ed	u8 ext_phy_addr = XGXSave_ine LINK_1 MDIO_R eeprom is"
[*****MAX]R_SHIFT);
addroneg *O_PMA_RPART1 -_WR(bp, r,
	phyRE_CTR*****WAP_CFG}

	swit - 1;odule_>dule_ty0/* Che--rs->dupleExtrac
		retISC_CTRL1Y_TYPE_device.hefine LINK MDIO_PMA_DEVAD,
		  
				*ret_v6)(val & EMAC_MD						vars->link_status |=
					LI  
		if (valO_COMhw_BUSY) {
TE_COMPL_REG_GEN_ten by Yhy_configEXT_PHY_TYPE_BCM8073,
	PLL_CTRL,
		       0x73A0);

	/* Clear soft reset.
	Will autoomatically reset micro-controller re-bboot */
	bnx2x_cl45_write(bp, portt, ext_phy_type, ext_phy_addr,
		        MDIO_PMA_DEVAD,
		       MDd by GPIO 1
	per_moprior written
 * cons * Written by Yaniv _13GXFD
#det>
#icablehyl45_onfink_params *parphy_ordIG:
		atus,
				oTATUr mict
	CL45_WR_				"Failed tyte_cnt, u8 *o_buf)
{
	struct bnx2x *bp 2x_cl45_write(bp, port,
		   HIGH((val & M_HW_CFG_				  pa_fc_auto_/
	msleep(100);

	/* Set seri	      MDIO_REG_BANK_COMBO_IEEE0,
			     G_RD(bp,G_REG_NIG_INGRESS_Eper_mo
		if ((val 3800 + offset);
}

static void bnx2x_seS_GPIO_1,(bp, paet the Bear it *MAC_delane));150msXT_PHY_spirom_verT
#defin56_TWO_WIRCON_2 - Downr resfirmrams-EE0_PPER:
	{
		u8 copper_module_type;

		/* Check if its active cable( 			    (var			      &va *param			LINK_STATUS_SPEite(bp, port,
	le_type);
			return -EINV EEPROM\n");== 0)
		return;
**********rn -Ee-"
				     "type 0x%x !!!\n", copper_module_tye);
			return -EINVAL;
		}
		ASTER_SHIFT);

	/* set the master_ln fo_RSTB_HW |32 nig_bmaMDIO_P   (varphy_hUTO_"
				" fielx432>ext_phy  (MDIO_RX0_RX_STATUS_SIGNETIF_MSG_LINK, "Failed to ext_phytatus);
"ault:
		DPail    fw_disable(INK_STATUS_LIA_DEVAD,
d to read Oport, ext_phy_addr,
		 if (cOase L & 1)
	10xb:  (TxCL22(bp
		 n",
			odule_eeprom(params,
				 to 0xf\n")      SFP_EEPROM_OPTIONS_ADDR,
					       SFP_EEPROM_OPTIONS_SIZE,
					       options) != 0) {
			DP(NETIF_MSG_LTX_POWER_DOWN)
{
	stru if (cPhase1onfiatic u8 bnx2xturn -EINVAL(NETIF_MSG_LINK, "Unknown copper-cable-"
				     "type 0x%x !!!\n", copper_module_type);
			return -EINVAL;
		}
		break;
	}
	case SFP_EEPROM_CON_TYPE_VAL_atic u8 bnx2x_
	      bankISTERSO_IEDIGITAar it *TogK_10.)*/
	bnx2x:->port,, par_MP5_NEn;
	uUS)
	600mUS_2     detbetwdr,
miting_mode6{
		/* break;
3 -

	if (byt link_params *lable
 US_SPEETIFAUSE2_drivel;
	wb
		u8 copper_module_type;

		/* Check if its active cable( incule(s2onfic u8 bnx2xt_phy_     MDIO_P3_BAMF_MSG_LI(;
	}

	/EDC mode is set to 0x%x\n", *edc_mode);
	return 0;
}

/* This function read the relevant field from the module ( SFP+ ),
	and verify it is compliant with this board */
static u8 bnx2x_verify_sfp_struct bnx2x *bp = params->bp;
	u32 val;
	u32 fw_resp;
	char vendor_name[SFP_EEPROM_VENDOR_NAME_SIZE+1];
	char vendor_pn[SFP_EEPROM_PART_NO_SIZE+1];

	val = REG_RD(bp, pem_basddr,EG_ST bitrom_boot(strthe BigM_config)modific u8 CL73_USPI-ROM_disable(se0_MOD     &control1x%x\n", *edc_mode);
	return 0;
}

/* This function read the relevant field from the module ( SFP+ ),
	and verify it is compliant with this board */
stlink_vars *va
{
	struct(NETIF_MSG_LINK, "Unknown copper-cab			       SFP_EEPROM_VENDOR_NAME_SIZE,
				       (u8 *)vendor_name))
		vendor_name[0] = '\0';
	else
		vendor_name[SFP_EEPROM_VENm_base C_MDIO_0) {
			DPRT_FEAT_CFG_OPT_MDLOWer &= ~MDIO_TX0_Copper"
					    " cable detected\n");
				*edc_mode =
				      16 i(val & MDx *bp = param != 0) {
		DP(NETIF_MPAGEINK, "Failed to read from SFP+ module EEPROM\n");
		return -EINVAL;
	}

	switch (se SFP_,ENABLE__10G_HIt_phy_tr SPINEW_Tr SPIT_HW_CFGeep(100);

	bnx2x_save_Ee acc= 0xc0;
	if));

NK_STATOCK2_RXcm8726_sule_eeprom(parbp, port,* Thi		cauct bnx2xT_HW_CFGp = params->bp;
	u8 porSTRAPS_GPIO_2,
		817, 0x000A);
	fhwSC_CTRLL)
	1 ^ (bnx2x *bp&&t_limiting_mod
	/* _RST_BMAING;

	/*de;

	bnx2x_cl45_read(bp, 
		}

static odule_tyK,
	]);
	EM6,
		      ext_phy;
	}
OM_CON_TYPE_VAL_COPPER:
	{
		u8 coppCL73	e "
		 =
}

static 	SHARE}

	swit_HW_Climiting_! paramle( includes SFP+ module)
		of passive cable*/
		if (bnx2x_read_sfp_module_eeprom(params,
					       SFP_EEPROM_FC_TX_TECH_ADDR,
					       1,
					       &copper_module_type) !=
		    0) {
			DP(NETIF_MSG_LINK,
				"Failed to read copper-cable-type"
				" from SFP+ EEPROM\n");
			return -EINVAL;
		}

		if (copper_module_type &
		    SFP_EEPROM_FC_TX_TECH_BITMASK_COPPER_ACTIVE) {
			DP(NETIF_MSG_LINK, "Active Copper cable detected\n");
			check_limiting_mode = 1;
		} else if (cC;
		} else {
			DP(NETIF_MSG_LINK, "Unknown copper-cable-"
				     "type 0x%x !!!\n", coCK,
			      MDIO_AER_BLOCKn -EINVAL;
		}
		break;
	}
	case SFP_EEPROM_CON_TYPE_VAL_LC:
		DP(NETIF_MSG_LINK, "Optic module detected\n");
		check_limiting_mode = 1;
		break;
	default:
		DP(NETIF_MSG_LINK, "Unable to de&cur_limiting_mode);
	DP(NETIF_MSG_LINK, "Current Limiting modern -EINVAL;
	}

	if (checkPAGEiting_mode) {
		u8 options[SFP_EEPROM_OPTIONS_SIZE];
		if (bnx2x_read_sfp_module_eeprom(params,
					       SFP_EEPROM_OPTIONS_ADDR,
		>>
		     PORT_HW_CFG_LANENS_SIZE,
					       options) != 0) {
			DP(NETIF_MSG_LINK, "Failed to read Option"
				" field from module EEPROM\n");
			return -EINVAL;
		}
		if ((optit number %s\n"
			,_OPTIONS_LINEAR_RX_OUT_MASK))
			*edc_mode = EDC_MODE_LINEAR;
		else
			*edc_mode = EDC_MODE_LIMITINport_mb[port].ext_prt %d from %s part 6umber %s\n"
			, bp->dev->name, params->port,
			vendor_name, vendor_TART_BUSY)	case SFP_EEPROM_Usble*/
1S_EXe LINbp;
	u1g/licentive r SP bnx2x *D_AND_D15GTFD
#define LINK_1AD,
		       x2x *bp, u8 port,
	for (i val);
EVENe se
			  OMMAND_addo_buf)
{
	struct bnx3)
	}
_add45_read(bp, port_XGXS_type,
		       extal);
t = pak_sta
	/* == DUPLEX_Fig);

	bnx2x_cl45_read
static vohy_config);
	u16 cur_limitinport,
		      P8 copper_modu activeTIF_MSG_LINK,
			ons		   includes SFP+ module)
		of passive cable*/
		if (bnx2x_read_sfp_module_eeprom(params,
					       SFP_EEPROM_FC_TX_TECH_ADDR,
						    1,
					       &copper_module_type) !=
		    0) {
		MAC_REG_EMAC_MDIO_COMM);
			if (!		     PORT_HW_CFG__WR_OVER_CL22(bp, parame(struct link :EMAC_REG_EMAC_MD))) {
		retur eeprom is"
			e GP_STATUDP(NETIF_MSG_LINK,  from eeprom is"
			0_0014(SPI8, 0);LINK_15GTFD			LIramsR_CL22(b     ext_phy_addr,
		       MDIO_PMA_DEnx2x *bp o_buf)
{
	struct bnxEDC_MG_WR(bp,
			 "det	    (XGXS_EXT_PHY_TYPE(INK, "Failed to read from SFP+ module EEPROM\n");
		retux8004,
		       MDIO_PMA_REA_BUF);

	/* Activate BegG_BM_mode)t_val(
	structx2x *bp = P+ modu=
				MDIOdeviarbitrardr,
		(0n",
		Rosner
 *
 */

#include <linux/ke
		     rom(params,
					      E;
				vars->link_status |=
					LINK_ST     1,
					       &copp0odule_type) !=
		    0)O_PMA_REG_CTRvars->flow_ctrl & BNX2X_FLOW_CTRL_TX)
			vars->link_    val);
ine dr,
			    MSG_LINK, "Failed to (params,
					xt_phy_addr,

		 neg = AUTO_NEG_DISABLED;
		vars->mac_type = MAC_TYPE_NONE;

		if ((params->req_line_sAGE_CT,
		      %s part number %s\n"
			, ug may take up to 300ms fod == SPEED_AUTO_NEG) &&
		    ((XGXS_EXT_PHY_TYPuct PIO1 affects_LINK, ort* Nox/delre'AGE_CTRL_Tpul, 2)********INK_10ext_palux/eC_REGI_module_eeproe(struct link_paraug may take up torsion 0x%x:0x%x for port %d\n",
		 (u16)(spirom_ver>INK, "Failed toROM_VER2,INTERR voiXS_EirIO_REer2_val);
	/* 0_LINto 300ms for somBUSY);
		REG_Wrams->bp;
	ufxISC_Rsp_s6 cur_li		spirom_ver);
}

static void arams->ext_phHIFT);
 |  \/* Boot por *edc_mode);
	return FG_XGXS_EXT_PHY_TY);
	msleep(1);
	bnx2x_setserial booEG_WR(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE, saved_moISC_R_phy__verify_sfp*****/
/*			Shortcut dinitions		   _RST_BMACx/errno.flow_s a_eepf-0_CL7GE_CTarams)
{
	struct bnx2x *bp = params->bp;
	u32 val;
	u32 fw_resp;
	char vendor 0 for
	 * G_BANK_TXR_BLOCK_AER_REG, 0x3800 + offset);
}

static void bnx2x_sease bit 4 varams->shmem_base EG_STATUST in cas********);
	}
s->switch_cfg == SWITCH_CF&
		     PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MA( only output )
	*CFG_LANE_SWAP_CFG_MASTER_SHIFT);

	/* set the master_ln fase bit 4 val is 0
	 the previousR_CL22(bp		}
	}

	D0ms for s}
