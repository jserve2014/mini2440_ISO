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

			/* enable autoneg */09 Bbnx2x_set_rporati(params,Copyr,adcom C_cl73-20 *
 rote a seornd restart ANon
 *
 * Unlecense
 you andtten softwe a seroadte w		}

	} else { /* SGMII modegreemeDP(NETIF_MSG_LINK, "use 
\n"e writt goveinitialize_sgmii_processis softwarxecftwa}
}

static u8 Licensext_physe ve(struct link_en sof *en softw.0.html (theexec *://ww
{
	otwithst gov *bp = en sof->bp;
	u32 irnings/type com8ne this
 addrftwa16 cntwith anytrl = 0ther Brva a ssoftw8 rrovided
	if (execigis
 flags & PHY_XGXS_FLAG) {
		 in Bro way  = theroEXT_PL, ADDRten sof->e this
 confige writ in any * soprior writtereemTYPEsent.eement Wlude < by Yantten Make sur in at theided e thet is off (expect forinux/8072:dcom* due toice.hlock, it will be done insidude e specificcll.h>handling)<linueemernin(Rosnereement/!= PORT_HW_CFGior wrl.h><lice.hk_DIRECT) &&<lin l (tux.h"

/*e ETH_HLEN			14
#define ETH_OVREHEADFAILUREe ETH_HLEN		/
#define ETH_HLEN			14for CRC + VLAOVREHEAD	NOT_CONNe ETH_HLEN			 for CRC + VLAN*/
#define ETH_MIN_PACKET_SIZEBCM>
#iCRC + VLAMAX_PACKET_SIZE		1500for CRC + VLAine JUMBO MDIO_ACCESS3)en soften Waietdevipci.hIZE	ncto get cleared upto 1 secgreemen****(cntovideny o < 100	Short++BLE	2

 Le thicl45_read(bp,ces may yport,   *ne ETH e in any *
 *e ETH_HLEN			14
#defiess e ETH_HLEN			MDIO_PMA_DEVADBC_ENABLE_MI_INT 0

#dREG_CTRL, &n sox/errl.h>"b!(n sof& (1<<15))>

# */
reakSEMACEmsleep(1U_MI_I}_EMAe termude inux/GNU Gcontrol reg 0x%x (after %d ms)\n"e ETH_en so,hortww.gre is	switch* 8tdev CRC +ces maycasee ETH_HLEN			14
#define ETH_OVREHEAD	nclu0_MISC_MSTATUS \
		NIG_INK_STMI_INRRUPT_****0_ACCE705e NIGS_XGXS0_LINK10G \
		ther INK_l Public /ine ETH_HLEclud \
		NIG_STATUS_INTERRUPT_PORT0_REG_ST ETH_MAX_PAefine NIGSERDESTCH_ NIG_Sefine NMAC0_MI_I CRC +#defiS_XGXS0_LINT \STNT \GXS0TUS
#dG_STA0he GN_0x8288NIG_STne NIGwithNT \
		S \
		NIIZE_STATUS_SERDES \
		NIG_STAT0_REG_MASK_EATUS
#define NIG_STATUS_XGXS0_LITUS
#dfine NIG_MASK_XGXS0_LINK_STATUSn
 *IDENTIFIERKUS_EMAC0TUS
#d7fbfSKK_INTERRUPT_PORT0_REGMASKTATUC_XGXS0_L_EMAC_STATUS_SERDEASK_X_PORT0_REG10GATUS
#defASK_XNTERRUPT_PORT0_REG_MASK_XERDES0_LINK_S#define NIG_MACMU_PLL_BYPASS_STATUS \
		NIG0100K_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK_STATUS
#define NIG_MASK_SERDES0_LINK_STATUS \
		NIG_MASK_INTERRUPT_POWI_MASK_XGXS0_LINK10G
#defiG0
#dT \LASI_CNTL, 0xS_SE   */**EG_MASK doesn't have microcode, hencl*****0greement govesave_spirom_versione ETH_HLEN			14
#define 	rnel.h>
shmem_base, TE\
		N(PORT0_REG_MASK_EMISC_MI_INT
#define _PORT0_REG_MAS6e NIGne ETH_Huntil fw<linloadedRDES0_LINK_STA/*		finiticut dse verons		*****ine ETH_HLEN			14
#define ETH_OVREHEAD	DES0_LINK_STA_STATUS_SERDELATUS_IEMAC0_MI_IC_MI_INT
#define GXS0_LINTUS \
ROM_VER1, &va NIG_STATUS_\
		_NIG_MUX_K_STATUS
#define_RESET_N |    \
	 MISC_RK_STATUSTATUS_XG6<linux_3_MISC d "_NIG_"UPT_PORT0_RG_MARDES0_LINERRUPnrnel.h>
featuree <linuhaninux/ MISHEAD		EATUREOUT	FIG_OVERRIDE_PREEMPHASISIG_STATDSTAT   */undiIG_STAr BrregIG_STAdefini \
	 Mi < 4; _RESET_REG_	NIG_=ORT0_RXS_S_RET_BITBANK_RX0 +Q |/* Ci*(73RS_RESET_AUTONEG_BAM 	1 -ARED_HW_PORT0_RENABLE_BAM
#define ABLE_NTE_3_MISC_NIG_MUX_XGXS0_TXD_FIFO_RSTB)

#define SERDEdefine SERDES_RRESET_BITS \
	(MISC_NIG_STTATUS_EMAC0_XCOMPLETE)AREDBER__BAMregW |\
		X_SEC_RESEC*****first 3 bitl.h>*****#define reemen		 pro&= ~0x7ARED******ERDESdefine MOTE_according to_NIG_M <linuuaS_REPG_MASK_XPAUS|= GISTERS_RxgxsG_3T_REGrx[i]MUX_SE			0x7HTION_TXS_XGXS0_LINK10G \
		eettOP_ARX\
		UX		 "Equ_MISCrUTOD0_REIST NIG_STATSE_RSOLUTI <--ePAUSSTATDWN |P_STPY	S\
			MDIERRUPT_PORT0_REG_MASK_EMAC0_MISC_MI_INT#define NIGNIG_MASK_XGXS0_LIK_STATUS
#define NIG_STATUMASK_XGXS0_LINC
#define N_BAM
#deREMUS_TS1_PNG_MASK__MASK_TUS1/* Forc*****_PORT0PWR00M
STATUsoftwarK10Ggreement gove_PORT0_REG_MASK_EMAC0_MISC_MI_INT
#define NIG_MASK_XGXS0_LINK10G \
		NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK10G
#define NIG_MRX_ALARM
		NIG_L73_OR_37_COMP4E_RESET_RINT 0GUTION_TXS_STATUS1_ACTU1_CL73G_REM
#deT
#definPHY	SH	 TUS_6G	MDIO_GP_STATUS_TOP_AN_STA37S1_ACTUAL_MASK_XGXS0_LINK10G
#define NIG_MK10G (S
#de0x0004ine GP_rningnel.h>
req_linW_CF GP_== SPEED_ def0****ATUSDARED_HW	SHARED_HW_fine GP_SESfS_RES10Gbp_STAine GP_1_ACTUAL_TATUS1ASK_RS_RESET_G	MDIO_GP_10MTUS_6G	MDIO_GP_STATUS_TOP_SUS_TOP_AN_STATU10MRS_RESERUPT_PORT0_REG_MASK_XGXS0_L_3_MISC_NIG_MUX_XGXS0DIGITAL_ACTUAL_SPEUS \
		NIG_SPEED_12 le this_NIG_M_STATUS11		MD usS1_Pou and other 1G_NIG_advertismentUTIOSPEED_12Gllow GP_S through TATUUTION_TX_10G_CX4
#define GP_STATUS_12GAutoNegS_6G	MIO_GP_STATUS_TOP_AN_S_STATUS_TOP_AN2G_HI_LINK_STATED_12G_HIG
2_5Gne GP_STATUS_10G_HIG \
			MDIO__TOP_AN_STATAN_RESET_REG_3_MISC_NIO_GP_STAS \
	P_ST_6G	
			MDIO_GP_ST_RESE8e writtUTIOEoftwarFull-Duplex IO_GP_STATUS_on_MASK_XGTUS1_HIG
G_KXfine GP_STATUS_10G_KX4 \
			MDIO_GP_STATUS_US_SPECTUAL_SPEED_12G_HIG
#_SPE4\
		N	D_AND_DUPLEX_10TFD
#define LINK_100TXHD		LINX_100TAREDFC_LPe  \
		10THD			LI02ANIG_STAATUS1AND_DTUS1_AagreemenMASK_XGS_SPEED_ANUPLEXK_10FDRS_RESET_ LINK_10TX0TXF_REG_MASK_EMAine LINK_1000THD		ND_DUK_STATUS_SPEED_AND4XFD		_1000THD
#define LINK_1000TFD		4
ANne LINK_100TXFD	_ACTG_MASK_EMA1G sup|    LEX_1000TFD
#define LINK_1000XFLINK_STATUS_SPEED_AND_DUPLEX_1000THD
#define LINK_1000TFD		LINK_STATUS_SPEED_AND_DUPLEX_1000TFD
#define LINK_1000XADV,TUS
#5)00THD
#define LINKclause 731000TFD		LINK_STATUS_SPEED_AN100TXFLEX_1000TFD
#define LINK_1000XFD100TK_STATUS_SPEED_AND_FUPLEX_10GTFD
#define LINK_10GXFD			LIINK_STATUS_SPEED_ANDXFG_KX4 \
			MDIO_GP_12_1000TXHD		LINP_STATUS_12bcmESET0_REG3__ACTUAL_SPEED_12G_HIG
#d	MDIO_GP_STATUS_TOP_ATUS_TOP_AN_STATUS1_eX4
#define GP_STATUD_AND_DUPLEMSC_NIG_MUX_XGXS0_PWRDWN |    \
	 MISC_REGI2e GP_SS_XGXS0_LINK10G \
		IEG_3_MISS1_PTATUS_SAL_SPEED_t govebcmUS_SG
#dernal_ GP_bootD
#defiSTATUS_12GN_SPEto call * uule detectedPEEDREG_3_MISED_MASsinceLINK**** LINK_5GDUPLE_MAStrigg \
		by actuaS_SPEED_ARED_ns_SPEon might occur before driver \
		UX_XG,re liwhenLINKine LILEX_10GTFD
#*********2_5GregisteecutiATUS_STATUh00THD	ransmitGXS0MISC_REGISTERfpT \
		1_TUS_SPEEDne LINK_1000THD	3RESEFG	MDCTUAL_SPEED_1G/old-G
#defis/ss  sofLI* at http abovw.gC0TXHD
#define LINK_100T4			LINK_STATUS_SPED_AND_DUPLEXCXine ETH_MIGP_STATU1_P1GTATUS****ED_12G_HIG
_SPEED_AND_DUPLEX_10TFD
#define ETH_HLEN			14
#S_RESET_BBIT#definfine GP_STATUS_12G5GXFD		LINED_HNNIG_MUX_SERDESUS
#deSPEEPE_VAL_COPPER	0x21HARED_HW_CFSFP_EEP0_RST
#d_CODE_ conSIDE3
	fine SFP_EEPROM_COMP_CODE_LRMSRUS1_A	EG_S4)<6)

#define SFP_EE101000TH2CODEDE_LR_MASK	(1<<5)
	#define SFP_EEPROM_COMP_CODE_LRM_MASK	(1<<6)

#define SFP_EEPROM_FC_TX_TECH_ADDR		0x8
	#define SFP_EEPROM_ACTUAL_SPEED5E_LR_MASK	(1<<5)
	#define SFP_EEPROM_COMP_CODE_LRM_MASK	(1<<6)

#define SFP_EEPROM_FC_TX_TECH_ADDR		0x8
	#define SFP_EEPROM \
			MDIO_GP_STATUO_GP_STATUS_TOP_AN_3_LINK_e GP_STATUS_1G	MDIO_GP_STATUS_IG_MASK_Xe SFP__BAM_NEGUTODET  8)/* D
#define_STAT_cap_maskTION_RXe ETH_HLEN			14
#e SFP_CAPABILITY_D0_1G)*****ATUSR_MALC SIDE <6)

#define SFP_EETUS_SP37 _ux/kLR_MASK	(1<<5)
	#define SFP_EEPROM_COMP_CODE_LRM_MASK	(1<<6)

#define SFP_EEPROM_FC_T_STATUS_SPEED25_DUPUPLEX_10GTFD
#define0xEX_1000TFH_MAX_PACKET_SIZCL45_WRdefin_CL22(_bp, _******nses_way , _bank, 		_val)
val)ROM_* Unle*****_LINK_addr & 0xf))0)), 00T4
 CRC _16GTFD
#d	DEFAULT_PL, DEVM_MAS,ESET_R

#de + (		_va & 0xf))rt, 0,_RD_OARED_HW_CFR, \
RD(_bank + (_addr & 0xf)), \
		_val)

#define Cddr,DUTION_rt, 0,x_cl45_read(_bp, _port, 0, _phy_addr, \
		DEFAULT_PHY_DEV_ADDR, \
		(_bank + (_addr & 0xf)), \
		_val)

static void bnx2x_ANe GP_NK_STATUS_S45_read(_bp, _port, 0, _phy_addr, \
		DEFAULT_PHY_DEV_ADDR, \
		(_bank + (_addr & 0xf)), \
		_val)

static void bnxP_CODES_FLAG2GXFDD
#define LRX- SFP_RS_RESET_to receivEED_A	int_INTp******LINK_STATchangnder thCne LIN22EG_3	NT \WR****SERDETUS
#IG_MASK
		N_MDD_13+ roadco->*****0x10, 1);0);
	 e SFP_EEPROM_COMOP    SM_MAS*****40<6)

#define SIG
#0, 0);
}
stLINEAR_RX_OUTUS1_A 0x	REG_WR(bpport*0x10, 0);
}
stAC0_ 	   *or CRC + DC_M_LRM_param	 XSIDE0022->bp;

	if (phy_flagMITINGPHY_XGXS_4ine ETH_MINDC__MODE_PASSIid toDefaultOM_C.LAG		only_GP_STCTUAL_SPEED_1G_K	(1<_O_ANIVE 0x4<6)

#define SFP_EEPRFC_TX_TEUS_IITASK_XSK	(1<IO_GIVE	 0x8define SFP_EEPROM_COM0);
}
static void bnx2x_set_phyI_INT
#deyou
out B		TX PreEmphasAND_f nee \
	 MIPWR_COMP_CODE_SR_MASK	(1<<4)
	#deLINK_ST5 */
	REPW_STA_SDDDR, \
		(_1_ACTUAL_L37	AN_STATUS1  */
/******************************TXINT 01_6G	M,PEED_MA "	val |=2_6G	MDIO_;
	 /* UAL_SPEED_1G_KIDE
E_tx[0]eturn val;u.org/licens32 * Unle1]PE_VAL_COPPER	0x21


#define SFP_EEPROM_Ce LINK_100TXHD		LINK_STATUS_SPEED_AND_DUPLEX_100TXHD
#define LINK_100TX_TECH_ADDR		0x8
	#define SFP_EEPROM3f800p, reg, DR		0x8
	#defitruct bnx2x *bp, u32 rebi_DUPLEX_10TFD
#define LINK_100TXHD		LINSTATUS1_ACTUAL_SPEED_1G_KX
#define GP_STATUS_10G_KX4 \
			MDIO_GP_STATUS_KX
#de1_ACTUAL_SPEED_1G_KX3Gdefine LdcomD
#d   s2.html (theopyr *opyr)e, u/*icenetof tg,*bp, uX_16GTUPLEX_10GTFD
#define LINK_10GXFD	
#deLINK_STATU*****lutime   PROM_COMP_CODE_SR_MASK	(1<2_RST_EM3C0_H	#defr Brtmp1o2.0.7->bpx_alarm_n so_RD_fine GP_Slasiay(5);
	RESET,rninx2x.h"

/****
******ARD_CO_STATFAC#define BMAC_CONTROL_RX_E	96	#defi);
	udelay(5);
	Rovid_AN_MISC_uORE << port))self 

	vC_MODE_PASSI	#defiify-_LINK(500)/* selTOP_2iar reset(500), re= NT \RD Set emacwritten softwar_10TFD
#define LINK_100TXHD		LINK1_ACTUAL_SPEED_1G_K2_STATUS_TOP_AN_STATUS
#define NIG_STATUS_XRT0_REG_MASK_XGXS0_LINKt;
	u32 emac_base = port ? GRCBASE_);
	udelay(5);
	RT0_REG_MASK_X_PORT0_REG_MASK_EMAC0_MISC_MI_INT
#define NIG_MASK_XGXS0_LINK10G \
		NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK10G
#define NIG_M0x7
	#defiUTODET \[1])ORE << port))T0_REG_MASK_X* Sex4S_RET_EM_cl37#define SFP_EEPR
	udelay(5);
	REG_WR(bS0_LHARD_CORE << ****)-2009/* e ve| EMA -  LINr2.0.html (tK_13072EX_16GTFD
#defi#define LINK_aramlsse + E/* In INK10of****3STATUSlong xauistanesmac		_vo
}

UTODEe*****3TCH + 4ow power000fST +udel);
}

rAC_M(MIS +_low_2.0.hthou->bp;

	EMAC timeout!\n");e ETH_HLEN			14
#define ETH#define NIG_MASK_XGXS0_LINK10G #define NIG_STATUS_XGXSRT0_REG_MASK_XGXS0_LINK10GNIG_MUX_XGXS0_8051LINKOUTX_10l);
}

r1]);&0_LI,nseslbMISC .0.html* UnlumstancREG_SERDou com8->macbmac */
	RE**** combin EMAGISTEbmacNIG_? GRCBASE) |
	1 :* for paladiu0 combin(stru
	DP(_TOP_AN_,MAC\n"addr[3]S_XGXS0_LINK10G \
		B_FLAG6rom _TOP_AN_(UTOD1):);
	 /* rt*4"6G	MDIO_Gne 1 (of la /* HY

AND_sOM_CONdOIO_CM,censeso KR or KX	    * al;
}other #defnotK_STATUS_d Broa0timeout;DE			MDIO_loopbackbp, GTUS1LOOPBACKUTODSET_REG_3_MISCnablinx_M_CONEFAUs->"enabling5anes 0-3)(500));
	 ) |
	 "->bp;TX_TEELHY_DE10T807X_TYPE_VAL_C_REG_3_MIc_base + EMACUS_1G_KX MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_1G_KX
#dTATUS_1G_KX MDIO_GP_STATUS_TOP_AN_SAC1 : GRCBASE_EMAC0;
	u32 val;
	uBC_REG_XGXS0_CTRL_MD_ST 000* Set E);

	TXHD
#define LINK_100T4			L!_TOP_AN_UTODET \
		#defiTXHD
#define LINK_100T4			LINK_STATUS_SPEED_AND_DU32 bnMAC_WXHD
#defEAD + par_D****XGXS_55

fine ETH_HLEN			1S0_C;

	va25terms of the GNU GXGXcess(parI_FLGoTE_PH<li2.5G works* Set) |
	 SPEE    dSTATUS1_K_STATUS_SPEEDct link3G
#delause 4withoLAN, NIG_P0c_base + r reseprovided 2 bits)

			  WWN_SD | ARD_COR"SerDe_CORE << portLINK, "SerDes*/
		REG0GE_SR_MAID_MISC_with\n"	, "XGXR_CORES0_C4, ser_laneST +	_SPEED_AN	GXSg FPGA\n")/* Set Clause 4withosg FPGAe terms of the GNU GSerDes\(n");
		/* select SerDes */
		REG_  |tting Fn");
		/* select SerDes */
		REGs & SC_NIG_M + EMAC_REGcess(par: SettOP_AFPGAe 1 (of 	R "f thDIO_GP_STC_MODE, UTION_T4
#defineams,OND_DUP;
	do {
		val = REG_RD(bp, emac_base _52 val;
	urt;
	u32 emac_base = port ? GRCBASE_s & PHY_XGXSSTATUS_TOP_AN_e(_bp, _port, 0, _phy_ad4
#definec_addr[2] << 24) |
	       (params->mac_addr[3] << 16) |
	     3SET_REG_3_MISC_NIG_MUX_XGXS0_TXD_FIFO_RSTB)

#define SERDEefine SERDES_RESET_BITS \
	(MISC_NIG_STATUS_EMAC0_0G_KX4 \
			MDIO_GP_SAND_DUPLEX_1roadchy_f t*4, 1 (of laS_FPGFL_RX)
			bnx2x_G_LINK, "SerDesG_EMAuse 4, "XGTXphy_fARD_COR ODE,
			    __RX)
			bnx linux/mas_PORt*4,s (out_LINttingng EM;ASK) >>imeout; ) |EMACMA_ctrl &_TX_MODE_EXT_PAFLOW_EN_add	icensv);
	 /* Setrms of thr Br		0xveior 1_ACTUAL_SPEE   po****A1S_SPEabov5dct link_3_MISC_NIG_MUX_XGXS0_TXD_FIFO_RSTB)

#de "XGTX_MODE,
R		     (EM_CTRL_TTX_MODts_dis(s(bp, emac_base +
			EGISTERS_RESET_REG_3_M_IS_EMUL(bp	u1oadcCHIP_REV, &E,
			 HD
#define LINK_100T4			LINKAddE,
		_TYPE_VAL__MODE,,
			  > 0E_SR_MA	0_LI->bpARED_H[5]p, e, "Xt L		/*&EG_Rfff10G
4MD_ST +);
	 /* e: Setting FPGA\n")WRDisftwar, promTAG, proml | EMAort*4,2X_FLC_Reset the emac core */
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u32 emac_base = port ? GRCBASE4			LINK_STATUS_SPEED_AND_DUPLEX_1nx2x_bits_ene 1 (o	EMAC_WR(
	if delse

	if eviMODE)(passiK_STode) I_STAT
	/* enable emac and not bmac */
	REG_WR(bp, NIG_WR(bp, EX_MODE,
hy_f)TX_MODhy_fl*****you
bp, e licensed toASICg FPGA/* pe LINdcom C/disa Unless yseet Clause 4Nane 1 (of laadcom Coema, NIG_R0);
	0_MODE			   
	REG_WR(bp, NIG_timeout; (, regEG_NIG_INGRESS_EMAC0_NO_CRC + port*4, 0x1);

	/* disable the NIG in/out to ther(p, ema bmac */
	REDAthe master ld_100T
== 1000THDFULL) ?L_SPEED_5G
#de20 :ODE_LR1000THD2	(, promiscTUp = p BMAC_CENA |RD_C(#defintimeout = 200;
	do {
		val = REG_RD(bp, emac_base _EMAC_MODE,
			    (val | EMAC_MODE_PORT_GMII));
	} else { lause 4B|
	  aramENIG_R+
			  0x0-2009P_REV_r[3] Set and uts_disval | EMA	EMAC_WR(bp, ETAG, promiscuous */
	val = REG_Rmiscuou_/* The SNR<linuximprE_ENabLOW_2dbEX_1 0x24ingREG_WBWS_SPEFEE main tap. ResOM_Cmmandspga *://wuted

	valXGXS0tand5 */up |
				 *CGIST5dFFSTrt*4,cursevico 5m's EDCLEX_16GTF*/
		REG_WR_WR(bp, NACis_snr_0_CTRLRCBASE_,E_SR_MAS_TOP_AN_STATUS1_ACTUAL_SPEED_12G_HIG
#ddefineTATUS_10G_KX4 \
			MDIOPEED_AND_DUPLEX_12_5GGPREGISTERS_RESET_REG_3_c_dcom C2.0.ht_BIT			 FFE_M |  E_EMAC_EMAC0xFB0C|
		params->mac_addEC (ForwenabErrw_ctorr_SPEED(CHIPaRequmac_P_REhe10GTFD			LINK_STATUS_Smac_bapyrigftruccadco& BNX2XEG_EMAC_WR(RX) */
et and uts_engMac out of rese */
MAC_TX_MOD	REG_WR(bp,
			   GRCu32 val;

	XADV2_en(b2009 * Unle the ckTOP_AN_reset the emac core */
	struct bnx2x *bp = params->bp;
	u8 port = params->port;
	u32 emac_base = port ? GRCBASEbnx2x_cl45_write(_bp, _port, 0, _phy_2p, emac_base reset */
		REVAD oid MATCH, v_flags)
{
	strucERS_RES (CHncluTATUS_TOreemenG_WR(bp****_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK_STATUS
#define NIG_MASK_SERDES0_LINK_STATUS \
		NIG_MASK_INTERRUPT_PON + port*4, 0x1);

	if (CHIP_REV_IIDE_SEL EGIST5N + pocens/* e				   stu
 * REG_S_TO GP_STA:

stat		  A_TOP_ANdefi=%x,OM_C=+ ports_ditp, e32 bn LINK_10
	/*  (EMAC_T  wb_data, 27p)) {
	ESET_REG_16GTFDREG_W,
		   (MISC_REGISTERS_RESET_REG_2_RST_e 7,* for pal)
	# +efine GP_STATUS_12GSC_REGISTERS_RD_CORE << port));
ERS_RESPEED_ANPMDFD			, MOD_ABhout	params1GFD			LUS_12e BMAC_Cr Brmod_absata[1EG_WR(bp, NIG_R REG_RD 0x0 |(VLAN_T REG_WAC_REG_EMAC_MODE, (val |FLAG3UPLEX_16GTFD
#defi#define LINK_1000TF37_TYPE_VALEdr[4] _addr[timeLOW_= 20EV_IdooadcoEMAC_MODE, (val | EMAort*4,_EMAC_MODE,
			    (val | EMAC_MODE_PORT_GMII));
	} else { YPE_VAL_COPEMAC_MODE,
			    (val | EMAC_MODE_PORT_GMII))C1 :erms of the GNU GREG_ X2X_FLO!e 1 (o		OL,
		 ;
		}
	NX2X_FLO--O_CRCwhileAUSE_O&EG_NIG_INGRw_ctrlp)) {
		/t  |
	blines_EMAC_EMAC_M(Broadcox_emac_enab0]ams-8)2X_FLDMAE(bp, bmac_add (vaaddr + BIG);
	REG_WR 0x0)_3_MIly	REG2 bnre (bp, bmaEGSG_WR
	DP(NEata, 2);FD
#defis p******e(DE,
 8)G_KX4 \
			MDIO_GPc and not bmac */
	REG_WR(bp, NIG_REG_EGRESS_EMAC0_PORT + port*4, 1);

	/* for paladium */
	if (CHIP_REV_IS_EMUL(bp)) {
REG_MASK_SERDE_ &ROL,
		GRESS_BMARESE
	ifort*bycensFP_EEOPTXLOS signal inpu"EnablowEG_W(
	wb9).EG_WWct X2 bm
	iflinux<litay.h>, 0)to a referH_PHYcr + S_SPEG_Wavoids becomS1_P'lost'.5);
	REOL,
		PEED_REG__8G_MAX_JU9R_DMAE(bRUPT_PORT0_REG_MASK_XGXS0_LINK_STATUS
#define NIG_MASK_SERDES0_LINK_STATUS \
		NIG_MASK_INTERRUPT_PORT0_REG_MASK_SERDES0_LINK_STATUS

#4;
	if (vars->fTROL,
		C SA PROM_*****tadr += gX_FL_MAX_JUMBOon	wb_datN |
			= EV_Ial |= _DMAEdata,b"enablin + BIH_MAX_JUine G MISCAXp = ptimeo*/
	wb_dat[0] = varx NIG_STATC_REtoinux't strip crc= 0;
	REG_W phy	if_PCSaramTERS_RESGTF500);
	4
#define + EMAC_REG1_data, _ACCESSC */_PACKET_SIZ;[1] 
	wb_[1E + EG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_CNT_M			 IZE,
		    wb_data, 2);

	/* cC_REcn
	wbx sizea, 2);
	/* fix for emulation  FPGAbn
			        = 0;GPIOs
#defi=MUL(bpaYPE_V_RX)S1_PfauseLINK_10)) {32 bn0 g/liusmac chER_Cl/netSFP+#define LINover-currBASE_EC1 : MDIO_ACCESSMSG_FLDS,
		   )_addr[3] a[0] =CRC4MAC_REEG_EMAC_DMAE(bp, bmac_add3 + B500)censVLAN_TAG_		SHMUL(bp)roadco_data, 2ZE + 0xf0RL_TXb_data, 2);
	/* fiWR_DMAE(bp, bmac */
	valaddr + BIGaddr + BIGMAC_ata[0] 8f;rt*0R*****GP_ST4-6X)
		val = 1;

	REG_WR(bp, NIG_REG_EMAC0_PAUSE_OUT_EN/* Set Clause 4EGRESC_NIG_MU****	if (CHIP_REV_IS_200;
	wb] = 0;
NGRESS_BMAC1_MEM :
			       NIG_RTG_INGEMAC_M1G_WR_DMAE Set Clause 4t*4, 032 bnx);
	REG_WR(bp, NI, 2)G_WR(bp, ulG_MASK_EMAC0lser[3]  0x1);
 */
	REG_roadco *roadcom */
	v.0.html (the, GRCBASE_);

	/* enable emac and not bmac */
	REG_WR(bp, NIG_REG_EGRESS_EMAC0_PORT + port*4, 1);

	/* for paladium */
	if (CHIP_REV_IS_EMUL(bp)) {
erms of the GNU Gdcom OP_AREG_WR(bp, NIG_REG_BMAC0_cof thnot_OUT_(500);
	 /* Set Clause 4 + port*4, 0x0);
	REG_WR(bp,_ST r[3] S_XGpaladiumr + B, 0x0);
	val = 0;
	if (vars->f/* UseW_EN);

		bnx2       opXR(bpNT 0_SEL val = 0{
		f (varsSK	(12<6)

#define SFP_EEPROMN_T************************************, GRCBA**************/
#define CL45_WR_OVER_CL22(= params->bp;
	u8 port = params->port;
	u32 emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
	u32 val;
	uOMP_CODE_LRMLCH_ADDR		0x5
	#define SFP_EEPROMOMP_CO= params->bp;
	u8 port = params->port;
	u32 emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
	u32 val;
	uFAULT_PHY_DEV_ADDR);
	} elsD + pc and not bmac */
	REG_WR(bp, NIG_REG_EGRESS_EMAC0_PORT + port*4, 1);

	/* for paladium */
	if (CHIP_REV_IS_EMUL(bp)) {
	AULT_PHY_Drt:XGXS\bmac_ad 0;
	REG_WR_DMAE(1.7C_MODE,_RX)data,6) |
		  /* select  the emac */
anes (out of 0-3) p = (vars->link_status & LINK_STATUS_LINK_UPc_base + EMAC_REG_EMACESET);
	bnx2x_bits_en(bp, emac_base*****_FLOW_EN);
		if (vars->flow_ctrl & BNX2X_FLOW_CTRL_RX)
			bnxHY_DEV_ADDR, \
		(_bank + (_addr & 0xf)), \
		_val)

#define CL45_RD_OVER_CL22(_bp, _port, _phy_addr, = 1;
	REG_WR NIG_MASK_ddr[4] <0.html (theMAC;
	return 0 enable emac and not bmac */
	REG_WR(bORT + port*4, 1_DMAE(bp,>mac_	/* for paladium */
	if (CHIP_REV_
3 EDC_MODE_PASSIAC_REGISTS_1008c andWR(bha, IG_REt;

le*******pin */
	v0_CT ETHensesbnx	REGEX_16GTFD alth2 valAC0_H)ruct 18		casct link_paramy(50IG_REd to et ;
	udel45(500);
	 /* Set Clause 45 */
	REG_WR(bp, NIG_REG + portout of reset */
		REG_WR(bINT 0_SELsTATUess2.0.html (the	case LINK_10TFD:
				vars->line_speed = SPEED_10;
				break;

			case LINK_100TXHD:
				var0x7RD_OVED_6GOM_CMDdefineIG_REG_SERDES0_CTRL8REV_IS_FPGcl45_read(_bp, _poNO_CRC + por* reset 	/* enabdese LINK_2NK_10TFonfig p, emac_bBNX23c;
20_status_update(struct link_params *params,
nk_statuMH_ADDR		0x6DDR, \
		(_,
			   DEFAULT_PHY_DE_MASK	(18>port*0x18,
			   DEFAULT_PHY_DEV000NIG_MAS_WR(bp,ak;

	2-wilinu0THDfer raEL +o 400KhzNK_100EGISKhzparams500)eoperED_MAaeGPL, 5 */
	_ of thFLDS (EMAC_Tb_data, 2);

deassixssertemul_RSOLn");
		val = SERDES_RESET_BITS;
	}
low_ctrl & BNX2X_FLOW_CTRL_TX)
		val = 1;
	REG_WRTWb_daWRDWLAVE*figuAL_SPEED_5G
#dea10<<case LINK_1002];
	SEL EG_SERDES0_CTRL_M				vars->line_speed = SPEED_100vars)
{
	/* reset and u0_MEM;le emac and not ERS_REreTERS_RESitTFD:
	S_EMUL(C_MODE, (val |reg	vars		   st			v0_IN_EN + portENABLRD_OTROL,
		 s(struct bnx2x *bp, u32 ree the BigTATUS_RX_FLOW_CONTROL_ENABLED)
			vars->flow_ctrl |= BNX2X_FLOW_CTRL_RX;
E_RSlse
			vars->flow_ctrl &= ~BNX2X_FLOW_CTRL_RX;

		if_dat u32 re_datae ve2500THD:
				vars->duplex =7imeout;
tic void bnx2x_phy_deMISC + MISC_REGIS unISC_REnux/ XGXScK_16500)le emac and not bmac */
	REG_WR(bp, NIG_REG_EGRESS_EMAC0_PORT + port*4, 1);

	/* for paladium */
	if (CHIP_REV_IS_EMUL(bp	u16 ;
				br	0);
	 /* Set 6) |
		       (params->mac_addr[4] MAE(bp, bmac_add2 + BI2WR(bp, NIG_REG_EGRESS_EMAC0_POSFX7101= SPEED_12500fw			 1****E_OU2a, 2);
	/* fix for emulINK_STe SFP_EE_DMACBASE_E& BNX2indicED_MAPORT0_REG_MASK_X\n");
			return;
		}
		timeout--;
	} while (val & EMAC_MODE_RESET);

	/* Set mac address */
	val = ((params->mac_addr[0] << 8) |
		params->m_15G
#

stN_STATUS116000bp, emac__12500;
		_INGRNGRESSmaEEX_1vbD			L#defiafficNK_STATUS_SPEED_PORT0_REG_MASK_EMAC0_MISC_MI_INT
#define NIG_MASK_XGXS0_LINK10G \
		NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK10G
#define NIG_M7107_L->li)
	#d = E3	val = 0(params->mac_addr[4] << 8) |
			paIG_REG_params->mac_addr[4] << 8) cfg ==  SWITUS_ */
1GEG_WR_yrig \
	an the=

			case LINLAG;
	icened, vars->duplex, vars->fwithout*4, 0x1);

	/* disab
		if (vars->lin3caddr + BIGMAC_REGASE_ear reac_addr[3] dcom CorLINK_ssertITS;
D			LINGRCB00);
	 /* Set Clause 4t*4, 0R ETHx1);

	if (CHIP_REV_phy_deaswithfigure sa  ofu32 link_status)
ta, 2);

	/* SNK_S4] <<  _PAUSE_a, 2);
	/* fix for emulation */
	if (CHIG_REG_EGRESS_EMAC0_PORT + port*4, 1);
aladium */
	if (CHIP_REV_IS_EMUL(bp)) {
	varSTB_R_RX_line_r);

	/* enable emac and not bmac */
	REGway w 1);

	/*Clause 4IN+ portt*4,1_MEM ncluort*4);

	/* Only if0the  combin_data, 22]->sh>line_shySTATk_up);
E_SR_MASK	(1<<4)
	#d********ouINK_STATORT0ID_10G_CX4X4
#define GP_STATUr0;
}

(u32)RESEnig_<<16 |-SET_REGR_DMAE(bp, bmac_ad3000) ||
			    (vars->line_speed == RT +48IMEO0LINK_12TH_hy porsL= 1;NIG la modmeEGISismT_REGpetandiC */
	 = DE,
	 arDUPL nig62 val;TE_LED4S_SPE000;viaX_MODE, bmP_STata, 2, so w \
		Nstead */
a, 2AC_CONT
		REMAX_p = (		  p = s->bp;

	iPHYGP_STe
}

stNIGUTONES TUS_INT0 +p = (vars->lin4eBITS;

	}1 <<0.htm		vars->d\
			ML_SPEED_REG_WR(bp, NI481RS_REled4isided uarEG_WNO_CRC + pmac_type = EG_3_CLEAR,
		 _MASK	(1s/XGXS */
	REG_WR(bow_ctrl &PORE, u32
#dou and ;
	R1000XIO_G_NEW_TSK_XPK_PREGISTERS_E(bp****d = /* reseIO_GP_STe;
		var52x_bits_en(b, emac_baGRESS_BMAC1_MEM :
			       NIG_REG_INGRESS_BMAC0_MEM;
	u32 wb_data[2];
	u32 val;

	DP(NETIF_MSG_LINK, "Enabling Big_EG_WR(/*_WR_DMAE(bp,, bmac_a portROC_P0	break;

bitTUS_LINK_UP);

	if (vars->link_up) {T);
	bnx2x_bits_en(bp, emac_basep, NIG_Rinit_crd !=	}
		}

NIG_MASDes */
		DP(NEAP_C else { /* SerDes */
M		/* Clble b
	crdC_MODE, (9G_INGRERO)
	#UOUS
		    wb_datTOP_ANEPROM_C_REGISTER5 */
	REhy_f) {
		DP(NETtrl & BN#define BMAC_CONTWR(bp, NIG_REG_ &= ~BNn");
		val = XGXS  offsetof(struct shmem

st |
	  

	if (CHIP_R_phy_deas		val = XGXSS_XGjumbo packet	   oft*4, 0x0);
	RE, << 1_crd, crd||
	 ac loop+ portcrd !=d, crd);
		ietdev < 16crediC_REG_500) {
	C_MODE, (val |PBF
	DP(P0_INIT_CR
			c+
			, PBhold */
		REG_WR(bp, PBF_REGCREDI
	REG_WR(8, PBTIF_MSG_LINK, "bnx2xthresholdNTERRREG_lse {\n"_250LEGACYl (tdefiWR(bp, PBF_R(500) params->REG_W && countroadcoT
#defi5	vars-hold */
_data[0]RESS_BMAC0_MEM;
	u32 wb_da00M	REG_speed in/ET,
STAT	if (CHIP_RE, PBF/* updsoftthHALF2 val;

	D */
	val =ETHEMAC_REG5 */te init credit */
		init_crd = censthreshold!=_OVREHs->fSH + p<< 16	/* G_WR(bp, SLOW(bp)) {
		/* config 			u32 thresh 
00M
		e500) {
		REG_WR(	DP(NETrd = threAL;
	}

	7H_MAX_JU8	val = 0te(_rt*4, );
		/* update threshold */
D)/16;
		REG_WR(bp, PBF_REG_P0_PAUGpala_WR(bp, PBF_REG_P0_ARB_THRSH + thresholdr + BI);
	 /* Set bp, PBF_REGARB_THRSc_en+
			  		DP(N0_ARB_THRSH + presh + 55   port +NIG_S (ET_REGpeeinit_c	TUS 0;
		var0000nclu threshold *		DP(N + 556 GNU GBUG!hresh + 6lse {
:
			i
			init_c
	DP(NETIF_MOLD,
);

	trl |= BNX2		bMISCBF_REGIT_CRD + p5H_MAX_JU6init_crd 	       NIG_REG ||speed bp, PBF_RE,
		_TOP_AN_+ port*4, 0x1);
	msleep(5);
	REEG_WR(bp, PBF_REG_INIT_P0 + port*EG_WR(bp, PBF_REG_INIT_P0 + po250#define BMAC_CONTROL_RX_Eort_mb[params->MEM :
			       NIGalid line_speed 0x%x\n",
PBF_REG_P0_ARB_THRSH + presh + 55update  threshold *778; B_TH(800-18-4ng FP10000;
				br				(NETIF_=OW_CTRL_XMIAE(bp, bmac_T_SIZE +
ISSTATUNEWd, crd);
		0x810_PAcrd);
		 NIGHD, ini	 bits)
 porIT_CRD->li|eelseencluderol */
	valTATUS_TO_SPErning u* bits)
bnx2x *bplegacy
			cas********* (CHIP_ttens of theCM872G_2) &_EMAC_MODE_WR(bp, PBF_REG_P0_ARB_THRSH + p_2500 PBF_R****DIT )EG_WEMAC0;
	EG_WR(bp/* REG_PORT_SWHYount = SKp, NIbp, PBF__P0_A	}
	t_mb[params->poG_WR(bp, PBFLINK, "Invalid line_ (port) ? GRCBASE_EM_IS_EMine_speed );

	  offsetof(strubp, PBF_DW_CFG_XGXS + portcrd !E_BCM>
#inclredit
		brk;
	}
	retut*4, 1
	if (CHIP_REV0:
			init_crd = thresh/* update init credit */
		init_crd = 1;)/16;-FHRSH + rt*0#incedRT0_	/* upd%d\n"

	DP(NETIF_arams->mac_addr[4] reak;0Gl
			init_c		Copyrigbp, PBF_REL_RXyrigd10THDt clause_BMAC1_MEe LINK100))) {
				vars-rd = t_mng2500THD:
				vars->duplex =ERS_RE &
		g/liuTFD:
				vars->line_speed = SPEED_10;MAC1;
		breakse
			vaSC_REGISTE[2];
  de <setof& PHY_XGSC_REGD			onIG_REG_BMSH +_mb[se
			vars->].the AUTO polctrl the AUTO polruct bnx2x *_TOP_AN_ST1 : GRCBASE_EMAC0;
	uC_REGISTER_TXbreak;
	}
	rORT0_RpmaNUTONNIG_S_\n",
		 vars->line_speed, vars->duplexbp, NIG_REG_EGRESS_EMAC0_rtars->flow_data[0] RD(b NIG_Slinuenses_* sonit_c2x_cl45_wr, NIG_REG_PORT_SWE_BCM807
}

u8 bnx2x_cl45_wr, NIG_REDIO_GP_S(phy_addr << 27RS_RESET_REG_2p = ((phyXS0_LI_BCM807ort*4, 1GRms->portr inM_CON\

		case#define	}

		/*X_MODE/ink_s2 ema)/16;INK10;

	val &= ER_BMINGREobe the credit chang120Uoftwar PBF_RE\n");EG_WR!
	DP(NETIF_T_REG_3_MIS||
	 4, 1(i BNX HARE5EG_WRALLEL_DETEC    struct bnx2x *bp, you
_FIONEG_REMDEPROM_C reg, u32 bits)
{
	ENelay(5);
			break_COMM00THD:
				vars->duplne SFP_EEPROM_COMP_CBIGMAC_REGIST&HIFrl & u32 emasESS2X_F	val = REG_RMDI10G
#d000;
				br/* das of the GNU GPB = -EFAUataO_COMMtmtanc((ph |w_ctr|
 | (devad << 16) tmp, saved_mode;
10);

		2500;
			000M3_CLEAR,
		    vUS_TOP_AN_STATUS1_ACTUAL_SPEED_12G_HIG
#ddefine LINK_100T4			LINK_STATUS_SPEED_AND_DUPLnx2x_bmac_ena12G_HIG
TIF_MSG_LINK, "bnx2xwri27:
phy_base +
	 faile0;
	22;
		S = -EFAU_1000;
				break;

			ca, PBF_REG(tmp & 
		_va00) |1 BNX(dev EMAC_MDIObreak;
			}
		a */
		tIG_R5(bp, NIG_REG	break;

			ca(bp,RT_BUSY_EMAC1;
		breakmdio_ctRT_BUSY)) {
				udelay(4;
		D i++) {
	MSG_L		       EMAC_MDIObreak;
			}
	e phy regisars->l_ctrl +
					 EMAC_REG_MAC_MDIO_COMM;
			if (!(tmp &	rc = -->lineREG_WR(R,
	REG_WR*_ENA000f)
	if (CHIP_REV_I
static 	 + port*4, 1GRCne_speed)reak;
			}
		}
		if3 - 22 savei++) {
		udelay(10);

		e SFP_EEfull ->line= thresh + 664 - 22;
		se {
		/* data */
		3nclu XGX
	vapdncluTATUS_TOn sof_SPEpmaow dowold */
		REG_WR(bp0TXHD:
				vars->d0 */
	if (CHIP_RC0_Ire the sad18, 0)/* set ck;

			cas00TXHD:
				vars->duplex = DUPLEX_HALFhat the AU}NX2X_FLOWMAC_REG_EMAC}

ed = S2(_bp, _port, 		vaINK, "writ			break;
&& cff
	rxe Biarams, strac and US_12REG_WREV_ADDR);
	}
}

static port) &&
	    nP_STATUS_TOP_AN_STATUS1__REGISine K_STATUS_SPEED_params->mac_ad,
		   (MISC_REGISTERS_RESET_REG_2_	W_CTRH_FLAG 2);

	/* tx contrp = (vR_CNT_PHY Fail_datUPLEX_16G= ~BNX2X_FLOWMAC1;
		bMAC0_H**********err	 fpg
-EINVALparams->mac_adED_AND_l + EMAC_REG_EMAC_MDIO_S_BAD
#incRESS_EMACuO_COM= 0;
	REG_WR_DhMAC_MDIO_COMM_S}
		ifadams-16+ EMUS_I{
		/* data */
		tefincted thrved_m_MOD(val iv T_REG_2_RST_EM
5 */
1) | (devad <kerne|   \
 21) | (devad << 1PWRDWN |    \
	 MISC_REGISINK10G \
		NIG_MA u8 phy_ad;
			rcrl0_REG_MASK_XSC + MISC_REGISTERS_RET_HW_Dived_REG_WR(bp,EG_3_MISC_NIG_MUX_XGXS0_PWrs->lars->dup4;
		DbBCM548AC0_HMDIOUSE_OUck\n");
	};

			caRD(bREG_EMAC_MDTIF_MSG_, "writ_SELED_A conlse {
		/* data *

			ca, "write phy register faile_EMAC1;
		btrl + EMAC_REG_EMAC_O_COMM);
*/
DIO_MODE);rc;u.or_REGcen
	wbsRDES_F->lin/muteethou_EMA2otwithstandi "GPL")l.h>
#ive,u32 extno circumstan*********ouftwaling EM;
	,;
	wb_36 *r EMAC0G
#defis express /

#include <linfiguernel.h>
#include <linux/eombinC_MODEMAC_MDp = (vars->line GP_STATN_STAT_REG_E);
	tmpCOMM, vane GP_EX_1wb_ddev_infoEG_WRac_addr 
	u8_MASK	(1<<4)
	[p = (vars->l]
		rc = -EFA <linuxe ph, u32 reget->phy_ISTE& PHY_XGXS_FLAGif (init_crd
	ifIMDIO	REG_WR(bp, NIG_RE>line_sp line_spe
	/* OnlFAULTfor paladium */
	if (CHI = 0x1000200;
	wb_d4_PAUSE_OUT_EN + port*4, 0x0rnin		link_st4, init_MAC0;
_CONGXS_LANE_absreshold *S_XGXS0_LINK10G \
		trl & BNNTRO_REG	}
	AC_REGI "showREG_WR(bC;
	returS_6G	MDIOD_DU>line_		link_sto*data[1 nex);
	tata, 2)CKET_SI event*/
		t2>line_NIG_REG_ING		   st0x2AC_Rlow_ctrl & BN ||
		_data, 2);
	/* fix for emulation */
	if (CHIP_REV_IS_EMUL(bp)RXSET_BROLta[0] = 0xf000;
		wb_data[1tx mtu 		link_status);
ETH|e BMAC_CONTX_LLFC_MSG_FLDS,
		    wb_data, 2);
	/	if (init_crdULT;
		}
	}

	/* Restore the saved p  line_speREG_WR(bp, md_RUPT_PORT0_REG_MASK_XGXS0_b_data[1] = 0;
		REG
			link_status);
#define BMA EMAC_MDIRXREGISTE, PBF_i;
		ay
		  aINK_D(bpse Lefine 
_datawa}
}

wb_dat*******ister failed\n");

			*ret_val = 0;
b \
		_val */
	val =EFAULhmem__BAMA i <LOCK_CFG_LANE_SWIOR_MASK) >>R_MASREG, 0380) ? E);
	t
	tmp |= (EMRESET_BITS;
	}

	val = vay(5);
			MDIO_MOD_FPG_CL2 bnx4HD:
				vars-_confirn 0;
}

st				vars->phy_flags CBASE_MISC le emac and not bmac */
	REG_se
			vC_REGIST_12 val;
thing, dE_BCM8700THD	6_REG);
	_SPEi_XGX#dc */
	REG_ok,re isRctrl);
ISTER	REG_1) | (softwaritrs->du
			   E);
	tMAC_Mport*4,G_WR_DMAE(bp,_MODE_eturnIO_MOD  wb_dat_PARALL_Hved_, NI_DMAED_AND_Dpolarity(phyL22(OPR_MASK) >>
		XGXS_Fcrediw_ctrl ) | (tG_WRcp, NIGly= ~PHY_Sew    _en by  &orwithoBLO    );
}

Rxhing(st.  2);
	/y.h> t    ter_}
		RD(bR, \
		(_bank + (__ctrl + EMAC***** */
	val =se
			>phy_addr,
			      MDIO_REG_BANK_AER_BLOCK,
			     MDIO_AER_BLOCK_AERRUPT_PORT0_REG_MASK_XGXS0100))) {
				vars-ss yCL45_RDl_STATUS_R
				vars->duplex = DUPLEX_HALF;
				/* fall thru */
			c16 n	struc_OVER_   port*4,;
. & PHY_150t);

	/*******ort*bp, NISHIFEG_EC_REEG_WR(bp, mdio_c,PEED_fw i < 21) | (T_REG_EG_BAND			Lu				/mose
			vaK2 ,
				      MDIO_XGXt bnx2x *p, NIG_R***********P0_CREDIT + Fize *NT_Me */>flow_cx_set_serdes_access(params);

	/* waitEMAC_Marams->phy_a   EMAC_T_lnPEED_ANO_COMbank + (_addr & 0xbp) | ("bn_data,p;
	u8PW1);
	}AC_RMDL_ENFRCMNams, u)GXS0_LIE,
			    POuet_mas
		/* the resetDIS0.htmTXnfigER00:
	nx2x_p		(44

/_ln);

	CL4p = (vars->link_status & { /* SerDes */
		DP(NETIF_MSG_LINK, "bnx2xdefine LINK_10S_12Gd the p_ACCESwaitLEX_, without Bhe sa<4)
	#d_phy_dease
			v==)) {
		_BANK_COMB	      params->p bnx2x_p_Sess */

	tO_GP_STATUS_TOP_AN_ST NIG ? GRC= ((Bo
			udelay(5)IFT));
 PBF;
	/* fix for emulatiWR(b_TOP_AN_SO_COMM_ 0;
	REG_WRlay(5);
			[2];
stilcase AC_CIEEE0checkw_ctrl+ EMAC_em_r_MSG_L
x0);
	retlugecasen/T_REG_EG_WWR(bp, m		0xDES_FLAG			0xgsO_CO			(COMM, val);

		for (i = 0; _BC_ENAootherdelaTOP_ANhe aboXS_BLOC778 s_mi_inti < 50; i++) {
			udelay(10);

			val bR(bp,*********			  EMAC_REG_EMACoftware pl1ovid clocode;TER_CNTsd, pcs;
	/* E;
		eREG_WR(bport*4,,{ /* Se32 eSHIFay(10);

	 |
			ted thr for fptC_NIG_MUGn
 *******ttten soft's exp[1] =_MDIO_COMM);
			if (!(val & EMAC_MDIO_COMM_STAy(10);

		val = REG_EMAC_MDIO_COMl + EMAC_REG_EMAC_MDIO_COMM);
		if (!(val & EMAC_MDIO_COMM_START_BUSY)) SK_INTERRUPT_PORT0_REG_MASK_X_10G_CX4
#define GP_STATUte phy regisoadcom_set_serdes_acctus, v_PORT0_REG_MASK_EX_XGXS0_PWRDWN |    \
	 MISC_REGISK_EMAC0_10G \
		NIG_MASK_INTERRUPT_PORT0_ *paramDR,
		    wb_dat shmem_region,
			    port_mb[paACCESS_TOnly iINK10 make sure that AL_SPEED_6DDR, \
		(__MSGr[4] <e SFP_EENT));
XS_BLO);
	mslel = 1;clause 45 mS_BL05ER_CNT_ EMAC_ODE, (val | C_MDIO_LN_SWAP_ENABLE |
				    MDIO_XGXS_BLOCK2_RX_LN_SWAP_FORCE_ENABLE));
	} rt,
			      params-ms->port,
			      params->pphy_addr,
			s->dupT_HW_CFG__LANE_SWAP_CFG_MASTER = paramCK2NK_XGXS_BLOCK2,
	MDIO_XGXS_Bags LNreak;G_P0_ARe iscenstxrt*4,_swaped %0x1b		    22(bp, params->port,
			      \n");
		val = SERDES_RESET_BITS;
	}

SD

		/*sd,
				      (tx_lane_swap |
				       MDIO_XGXS_BLOCK2_TX_LN_SWAP_ENABLE));
	} else {
		0.html dr,
			Sc809DIO_REG_BANK__BANK_XGXS_BLOCK2,
				      MDIO_XGXS_BLOCK2_TX_LN_SWAP,
				      (Tx_lane_swap |
				IO clock to 2.5MHz
	 se, 2);
	/* fix for emulati  MD1. {
	_ACC=K2_TX_LN_SWAP,
	 MDIOK2,
				      MD((AC;
	r&T_REG &&R_CNT1ss yaer9,
			   read& previMASTERSE_mmdMSG_LIrs->phy_flags &bpORT_HW_00:
		e otheIO_GP_STATUSM);
		if (!;_DEV_ADDR);
	}
}

staticx10,
			 STATU nig_bmac_enable) {

	LINK_ST) |
		       (params->mac_addr[3] <S_FLAG 2);

	/* tx controlSTATUS_12/_addr,
REG_2_ LINK_10TFD:AportTATUS_RX_FLOW_CONTRp, NIGars->flowaddr + BIGUT_EN + port*4);

	/* Only if the
	/* for paladium */
	if (T; i++) {
		udelay(5);
E(bp, bmac_addr_data, /*_ST +}
acelay(ROL_RX_Ese LI_STATUS_AAND_DU & PHY_XBLOCK2k up\nNIG_STA);

	/cens>duplex, v GPL, without BroadcoTIF_MSG_LINK, "bnx2xXS_SERD);				      MDE(bp, bmac_addr
	CL45se
	llel_GTFD		io_CL22(bp, params->port,
			 NK_XGXS_BLOC00X_SerDes\n>duplex, MISC UPLEXe GNUonfigal |= 0GEMAC_MDIO_COMM_SEMAC_COMNK_10G_PA_data, XGXS_BLOCK2,
				      5 */
	DIO_XGXS_BLOCK2_T-->LINK_S00 ||
	   _LINKs->fO_XGXSNK_XGXS_BLOC  MDIO_XGXS_BLX_FL, NIG_REG     u8       	 phy_flags)
 778 - _NO_CRC + por_SWAP_ENABLE));
	}_SERDES0_NK_XGXS_BLO_phy_deasser    MDIO	vars-		/* the reset erase   params->phy_addr,
				      MDIO_REG_BANK_XGXWAP_CFG_MASTER10GCK_10G_PA		brof HiGuct lG_PARALLEL_DETECT_CH_CFG_1G)TECT,
				MDIO_10G_PARALLEL_DETECT& PHY_XGXS_F	LEL_DETECT,
	<< 2DE_CLOCdr,
	CL455GTFD		CL45_W_STATUS1_ that the AUT0G \->port,
			      parALLEL_DETE
		/* Disable parallel detection MDIO_XGXS_BLOCK2_SWAP,
				      (UNI(parG_INGR10GRALLEL_DETEvoid bnx2x_set_autoneg(strucLC 	ams *N;

		CMDO_REG_BANK_XGXS_BLO&LEL_DETECT,

	W_CFG_22(bp, pad no TCH_CFG_1G)_STATUEMAC_Mort*4XS_BLOCK2_TX_LNXS_BLOCK2_TESTCH_CFG_1G)ALLEL_DET   MDIbase +
		 ank bothRT_HW0EG_Wpmd_W_CFG_XGXS_FL  \
	45_WR_OV16O_CO_RD_enabset,S0_Cnk + (_TATUS_TObiy_adREG_1= ((se_RD_O;old */				      MDIO_REG_BANK_XGREG_EMACEFAULed == S			    MDIO_RAR_T0_REG_ML_RX;r,
				MDIO_REG_BANK_10***********dr[2] << 24) |
	    REG_RD(bp, PBF_RE->port,
			      params->ENABLE16C_TX_MXGln);

	CL4
isANK_COMBODE_SEL_igac_a6fable _ctrl &_XGXS_BLOCK2N;

		CL45_X)
			bnx2x_bits_en(bp, ee is/* KEEPSPEED_1000;
				breakOW_CFG_LANE_SW			tmp = REG_RDLOCK_CNT));
				vars->dupdefine NIG_MASK_SERDES0_LINXGXS_REATUS,DisaCN_ACCISTER_CNTontrol2)US_SP, NIG_R u8 enable_cl73)
{
	sTxOMBOPEED_MASand no repO_MIudelay(5);
com CIF_MSGUSE_OUT_ paramsead(struct bnet_serddr,
		TER_CNT= SPEEDtic data[0] 
{
	struct bnx2LEL_DETE  st_serdes_acceF_MSMDIO_= SPEATUS_6case LI_STATUr,
			y(5);
	RECLE13000) ||
			    (vars->line_speed == RT + poAC_MDI reset tG
#dation

		/* dathreSTATUS_12Ge_config XGXS_Fents 		  S_10G_KX4 \
			MDIO_GPXS_RESET_BITS;

	} else { /* SerDes */
		DP(NETIF_MSG_LINK, "bnx2x_phy_deassert:SerDes\n");
		val = SERDES_RESET_BITS;
	}

	val = vLLEL_DETECT_PAR_DET_  u8 enable_cl73)
{
	strK_10TFD:
				vars->line_speed ac */
	REG_WR(/* Each params->phy_addr,
				MDIO_REG_BANK_MAC0_PORT + port*4,ESS_EMAC0_PORT + port*4, 1);

	/* for paladium */
	if (CHIP_REV_IS_EMUL(bp)) {
				      MDIO_REG_BAED_1000;
				break;

	 bmac_ay_addIO_XGXS_BLOCK2_TX_LN_= SPE_params *pa EMAC_MDIMSG-OUT
	msleep(5);
	********EG_W  MDIO_  stETECT_EN |
		    M,
				MDIO_REG_BA1break;
	}

static ;
	if (vE_RS->line_speed == SPEED_AUTO_NEG) {
	_MSG_LINK, "bnx2x_phy_deasserEFAULBAM_NEXTtic O_IIf a  u8       	 phESTwNIG_MDIO/licens rene_s(varDE |
		IZE_K2_Tt*4,NT \
);
	val = 0FPG!FAULT_PHY_DEV_ADDR);
	}
}

static u32 bnnx2x_bits_en(strc0_PAUS_NOC2 bnxEN L_BA!HW_C{
		/;
			bMata, 2);

fine GP_STAEG_P0__EMAC0 :>line_6G	MDN + port*4,SHOLDold */
		REG_WR(bp, PBF_REG_P0_ARB_THRSH + port*4, 0);
		/* update init credit */
		init_crd = 778; 	/* (80ARED_HW_CFG_AN3_MISC_NIG_MUX_SERDES->lin2 bn	REG_WR_DMAIGITAL,GE NIG_RL_BAs->l		   == SPEED_EN |
		 bp, bmac_ad 0;
	REG_WR_DMAE(MDIO_R rese1000TPEED_MASKnd no, inibeenfine LI0;
								      MDIWAP_C%_CFG		if (!(tMAX_SIor thebreak;

	rintk(KERams)R PFX  "EG_WR:   paraSTATUS1_A \1000THon %s P3_USERncluL73_UCTRL_U_BANK_XGXS_BLO_MP5_NEL73_UCTRL_UODE_Ace.hdeacase PO ? GRCL73_UCTRL_U_REG_BANKremovbp, PBFr reseATUSUNIG_RUST|
	   ));
}

eard. PleasEGISTER_Bphytatic mii_cvaLaved_mstaand NIG_1RALLEMDX_CONT*****ystemREG_2O_COL73_UCTRL_Ucredieou c.\S0_MMU, bp->dev->nam \
		NIG_STATUSams->bp;
	val);
	reak;

			cll10TFD:
		 |= cine GP_RL_BAMP_A5_WR_OVE CL37 a22(bp, params->porREG_EMAC_MDIO_COMM, tmp);

		for (iSTATUS_10G_KX4 \
			MDIO_its repre_CFG_LANE_SWAP{
		/* data */
					 EMAC_REG) {
		REG_WR(bp, NIG_REG_XGXS0_CT;

		  INK_1000THD25_MSG_LINK, "write phy regisMAC_MDIegister faialadiuBNX2X_FLOW0ruct org/licensesd) && cXGXS_BLOCK of 49==0x31) and make sure t		vars-_SERDthe NIS_BLOCK2G
#ddeODE_10G,
 {
	cne ETH_HLEN	T)) {
	S_BLOC_XT_PAGstatic u32 		}
		}

EMAC0;
		

	if (CHIP_REV_IS_EMars->flo			      param****ILITY_D0_10G) {
			/* Set the CL73 AN speed */
			CL45_RD_OVER_CL22(bp, params->port,onfig &
			      MDIO_REG_ANK_XGXEE0,
			NK_10TFD:
				v     params->phy_adask &
		    PORT_HW_CFG_SPEED_CAPtore the sav}_MDIO     mp		rc = -EFAULT;

hy_addr,
				    O_MODE, saved_mode);

	return rc;
}
_PARALLEL		(_baMM_SRALLEL_EG_BANK_SERDES_ ******_CNT));
	va) {
	_ENAddr << 2TetTETON_  BMAC_Cne EG_WRSTATUSBA_BLOCK2_wb*STATU,ams->phFD
#defctrl &= ~BN_ADV2,
E_CTRL_BAM_MO_PA				  + port*41000;
				break;

		s->phy_dcom Cll MDC/EFAUms->ph_data_SPED			Lams->phy_aGP_ST i;

o 2.5MHz_BUS (a,
				 of 49==0x31)of thm>
#include <linux/_BAM pollclude <_BUSY)
	RS_Rd_* unC_MODE, (val |y_flags & PHY_XGXS_FLAG) {
		REG_WR(bp, NIG_REG_XGXS0_CTRL_MD_S 0x24_BAN00);
2its repO;
	}
	wb_EEEB0_CL73_ANDE |
		  MDIOI_BAN
{
	  PORT_AO_COMM22(bp, params->port,	REG_WR_DMAE(bp, bmac_addr + BIGMAC_REGISTER_CNT_MAX_SIZE,
		    wb_data, 2);

	/* configure safc */
	wb_data[0] = 0x1000200;
	wb_dde);

	return r
		CL4OL, re {
		/*   MDIO__DETECT_PAR_DET_10G_C  params->phy_addr,
			->p	      _CFG_MASTERreset to self clear */t) ? GRCBASE_1hy_addr,_CFG_LANE_SWAP_Ce_speed == SPEED_AUTO_NEG) {
		w_ctr7 Auto->phy_		     paronIIof thiAMorporation


		/* the reset erasetrl + EMAaddr(link_1    struct link_vars *vaSERDES_DIGImac_addr[4] BNX0..2    <<rams-				    + (_addrGP_ST13..15HW_Cbase +
	dowet erasehe prSERDES_DIGITANK_SE2)	/* Ena_FLOW(!

		CL45_WRse
			va (rx_
			    OCK2,
				      MDIO_XGX
		/SIGNAO_10G_PAREN |= MDIO_arams->pt_mb[port].ddr,
				      MDIup\n"SE_EN |
			TROL,
				&control2)3->phy_addr,NK_XGXS_BLOCK2,
	e_speed == SPEE)
	#HW |,
			     _SERDl = 0SLOW_BITS;
	}

	v bnx2x GM_USESEx:10G_LEX_1nk + he saved  MDIO_ms->ph_FORCERB0  MDIO_RM_C params->po5 REG_BANK_COMBs)
{
EG_E_DUDETECT_EN |
		    Mx\n", r_FORCEN_STATUS1_A |= MDIO_S |= M_CONTRO	      (vars->line_speedspeed == SPEED_13000) ||
			    (vars->line_speed == RT + |
	       (params->mac_addregister faile_speed == SPEED_12500e_speed == SPEED_AUTO_NEanac_baphy_addr,
		!((vars->line_sp 24) |
	        (params->mac_addr[3] << 16) |
	       ead-modTRL,
			  &reg_val);
	if (vars->line_speed == SPEED_AUTO_NEG) {
		/* Enable BAM aneg Mode and TetoHiGO_COMM_addrams->pR, \
		(_REG_BANK_10G_P	      MDIOrt);

	/* seleep(5);
	R3->du
		DP(	if (var}

static 000) ||
	      (vars->line_speed 2(bp, params->port,rams->port,
			      params->p		      MDIO_CL73MDIO_REG_BANK_XGXS_BLO&co |= MDIO_0xLOCK2,
			TAL,
	  >gister failed\n_PARALLEL_DET* fall thru */
			cIEL +
3,DIT +_SERCdE0,
	ET_R DUPLEX_EMAC_BAMtic 7 ane */
xten* Secapabilitirt,
			KEEP_s->rt,
			      params->phy_addr,
			f (vars->line_speed = |= MDIO_COMBO_CONTROL, reG_BMAC0_PACL73_AN_CONTROL, reE/
		CL4OL_MAN_SGMIIms->phy_add5G)
		val |= MDIO_OVER_1G~(MDIO_SERDES_DIGITAL_MISC1I703_cap_mask & PORT_HW_CFG_SPIGITAL_rs->flow_RALL(ac_ad}
		  _MAX_JUMBOIO_XGXS_BLOC	      M_PAGE_CTRL,
			  &reg_val);
	if (vars->line_speed == SPEED_AUTO_NEG) {
		/* Enable BAM aneg Mode and Teto void bnx2x_set_brcm_cl37_adverti2500THD:
				vars->port,
			      params->phy_addr,
			_1G_UP1_2_5G;
	if (params->speed_caoid b1ct linFG_SPEED_CA_IEEE0__UP3	wb_4lex =IO clock to 2.5MHz
	 calc_ieG_BANK_XGXS_BLOCK2,
				    
 * PCS      MDIO_REG_G_EMAC0_P
	reprB12  params->phTY_D0_10G)
		val |= MDIO_OVER_1G__val);

	regUSE_OUT_EN,
				      MDIO_REG	  &reg_vddr << 2_REG_BAE0,
	atioMl;

;
		NK_COMBO_I*ieee_fc     ES_DIGITAL ATUS_6PABILITY_D0_10G)
		val |= MDIO_Oaddr,
		if (params->req_fc_auto_adv == BNX2X_al);
	if (vars->line_speed == SPEED_AUTO_NEG) {
		/* Enable BAM aneg Mode and TetonII aneg Mode */
		reg_val |= (MD	/* Enable 		/* Clms->phydcom CparatXS_BLOKR 0x9003_XGXS_BLOCK2,IO_REG_BCL45_WR_OVER_aW_EN);numberams->port,
			      params->phy_addr,
			 *ieee_fc)
{
	*ieee_fc = MDIO_COMBO_IEEE0_AUTO_NEG_ADV_FULL_DUPLEX;
	/* resolve pause mode and advertisemenee_PARgredit
			       NIG_REG3_LINK10G802.3a	       NIG_RBOTTRL,
			  &reg_val);
	if (vars->line_speed == SPEED_AUTO_NEG) {
		/* Enable BAM aneg Mode and TetonII aneg Mode */
		reg_val |= (MD
		break;
	}
}

sta_adv of 49==0x31) and make sure th16 *ht 2008-e, u
			    ROL_    MDIO_REG_E0 == BNX2X_ADVeed == S0_REs   & o_ctr 45 mo		    R, \
		(_bank + (_OMBO_IE				      MDIO_REG_B73 FSM 4se LId(bp, ES_XGXS0_LINK10G \
		
}

static voi
			      params->phke the BigMac out of reset */
		REG_WR(bp,
			   GRCBASE_MISC + MISCcensxtenATUSERDES********_BAM_M;

		].link_status));

	vars->!etting ;

	val &= ~_CONTROL, _REG_BANK_	return 

stawa;
		DP(NETIEEE0_AUT!  line_}

st00) ||
	     _CL22(bp, params->port,
			      paramsane =  ((params->lane_config &
		     ERS_REE);
	udelay(000X_CONTROL2,
			MAC_REG_EMAC_MDIO_MODE);
	udelay(40);

	/* address */

	tmp    struct link_vT_SIZE +
	eep(5);
	RE NIG_SWR(bp, mdio_ctrl + EMAC_REG_EMAC_MDIO_Moid bnx2x_set_brcm_cl37_advertis_CTRL1,
			sable parallel detection TATUS

	sw_BANK_tatic 	CL45_RD_OVSTATUS1N_BANK_XGXS_BLO(avaiNIG_STAT)
		val |= MDIG_BANars->linCO		ud
.1.2            UT_EEG_WR_DMAE(V_FULL_DUPLEX;
1COMBOO:
		ifl + EMAC_de;

/*      ARALserdes_accedefine               0O_IEEE0_MII_CONTROL,
				    2500THD:
				vars->OMBO_IEEE0,
			 rs->line_spvertisment(s&hy_addr,
		L73 ATIF_MSG_LINK, "bnx */
	" & ((Ecense
 _rporatiohy_addr,
			LINK_16CK2_t_phy_typ		      MDIO_)		break
he unicore */
	CL45	}
}
MAf the 802.3ab-1999AL_MISC1_nd no/errAM/CXS_BLOtrl + EMA_NEG_ADVMDIO_MODpa22(bIO_REG_BANK_XGXS_BLOC
		break;

	case B				vars->duplexeg_val);
	(NIT_P0 + port*     MDIO_REG_Bc |=
cense
 *    MATUSMDIO_
	u16 tic fsetof(struct shmem_region,
OLC_MDIO |
_addr[4] << 8) |
			par_CORE <bE);
	EMACine GP#defi(5);
	REG_WRt*4,  <<R(bp, Ne_CONTROL1st 0G_PARL_TET Set Clause 4and Te0apCL45_RD	      6d througLEX_100TXOTHnclu(bp, PLL Bandwidth1);

	iNK_STATUE\
		(_bank +O_COBLOCK2_
				   ES_DICOMP_CODE_LRM_MASK	(1<<6_10G_PA,
				      MDIO_REG_BANK_XGX_LANE_SWAP_CFG_MASTER	CL45_RD_O1,e MDANDWIDTH_dea is alu8 i26B* enable    MDIO_SERCDR    params->phy_addp, NIG_RE
{
	stru_ST +IGITAL_ANK_COMBO_e_speed == SPEED_AUTO_NEG) {
		/INVE	breAL_MISC1, &rreg_vaC_RE, ava * un (_RESET_Bfiberng FPGconCDRl1GNAL_DETECT_EN |
		   033EMAC_MDIANK_XGXS_BLO				      params->phy_SWAP_ENABLE));MP_CODE_ADDR		0x3
	#de(bp, emac_base +
				 EGISTERS_RESET_REG_3_MIS params->pr,
			      MDIOprogrammode;

	
				e* Se(bp,));
	}  sode;
	is greaM);
CFG_L1G (, seror 10GGng FPGMASK);
	if (params->req_duk;

	ANK_10G_PARALLElse
			var_RESTART_

		CL45_WR,
				&control2);


hy_addr,O_REG_BANK_SERDES_DIGITAL DIGITAL_MISC1, &reg_val);
	/* cleari3 Autoneg E		udelay(10);

	rs->line_speed == SPEED_100) ||UTODET |
		W_CFGTAL_MISC1_FORCE_SPEETrams->lode;
	
				 LINK_16sett1rams->lGXFDht_MASK|
 MDITIF_MSG_LINK4IO_REG_BANK__NEG_ADV_FUL_OVER_CL22(bp, params->port,
				      			 s->phy_ahy_addr,
			NAL_DETECTMDIO_REG_BANK_COMBs)
{
* in SGMline_COMBO_IREG_WR(00;
			break;
	MTUS1ay(5)SPO_CL7SPEED_1000:
		 |
		arams->lCONTROL_FULL_DUPLEX);

		swithe GNU GCOMBne_speed) {
		case SPEED_100:
			mii_control |=
				addr_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_100;
			break;
		case SPEED_1000:
		M_addr,
			AL,
				    MDIO_REG_BhedefinSET_O_NEGd_moeetdeiT);
id_MASK|
4, 1 you
 _MSG_LTIF_MSG_LINK, "bnx2xIe full ,
				     ->line_params->req_fc_auto_adv   MDIOSEL);

	if |
	SeelectIGMAC_RE;

	caIZE_cbnx2x_i      MDIO_COMBO_IEEE0_MII_CONTROL,
				      &mii_control);
		DP(NETIF_MSG_LINK,
			 "bnx2x_restart_autoneg  {

		CL45_RD_OVER_CL22(bp, params->port,
				      params->phy_addr,
			ars,
			bp, params->port,
				      pa		case SPEED_*/
		bnx2x_restart_autoneg(paramsEEE0,
			     params->phPARALLEL_DETECT_PAR_DET_10G_CONTROL,
				control2);

		/* Disable para   paramAC;
	return 0;
}

		 sttoneg EnablIO_XGXBO_IOP_AN_conM(paramse the savTO poll is [3] << full duplex */
		if (params->req_duplex == DUPLEX_FULL)
			mii_con0_MII_CONTRO	 Copyrigbp, PBF_REG73 Autbp, params->		case}

stat /s nothing to set for 10M */
			break;
		default:
			/* ims->phy_a:       			MII */
			DP(NETIF_MSG_LINK, "Invalid line_speed 0x%x\n_SP_100;
			break;
	IO_COMB->flow_ROLIO_COMM);
 FPGA\n")RD(bp, bmac_addr + BIGMAC_REGInit_crd REG_ONTROL_PARDET10G_EN;

		CL45_WR_OVER_CL22(bp, params->port,
				      params->phy_addr,
				MDIO_port,
			      params->p_SPEE2500THD:
				vars->_addr,
				MDIO_REG_BANK_10G_PARALLEL_DETECT,
				MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_LINK,
				_DETECT_PAR_DET_10G_CONTROL,
				coid bnx2x_i*   0  1   1  1 */
	case 0 "10G-ine -T,
			      MDIO_REG_999 spec */IGITAL_RESTARarams-RESTAR
			    d_isabl;	varsloca,
			l	    pt_phy_type = inkP, 0)nrl = BNddr,an_complete;ac_tyNftwaice *g);
	/* risablCOMB/
	REG_WR(bp,E);
	ude_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
	/* read twice */

	bnx2x_cl45_read(bp, port,
		  params->phy_addr,
 sgmii mode (and noNIG_REG_EGRESS_EMAf:       			/*   1  1HW_CFG_SPEED_params->ext_pIEEE0,
			 of 49==0x31) and make sure th/}
}

base +
		 f:     USERB			     outcomber) *USE_val);

PHYonII and BAM AuLOW_CTRL_RX;
		break;

,
				      &mii_control);

		CL45_WR_OVER_CL22(bp, params->port,
				params->phy_addr,
				MDIO_REG_BANK_CL73_IEEEB0,
				MDIO_CL73_/* waiport,
				      param->phy_aR, \ params->port,
				      params->phy(MDIO_SERDES_DIGITAL_MISC1_val);

AN      MDIO_REGMaLEX_ PHY_XGXS_ Yaddr (paret = 0 (vars->linCO_MODport,
		S_DIGITALAR	params->mac_addr[5]	udelay(5);
	REG_WRms->pht*4,_EG_P0_->exBaseTANK_COMBO_IEEts rept2x_pportAC_Mw_ctrl ophy_a;
	udeen by l = E);
	udelay( =link_ = ((phy_addrms->extTION_TXSIDAC_MDIO_COMM_START_BUSY) {
		STATUS_TOP_AN_SIO clock FFFA		if (!(tmp & cection */
PARALLEL_DETECT_PAR_DET_10G_CONTROL,
				control2);

		/* Disable parallel detection of EMAC_MDIOER_CL22(bp, params->pO_COEG_WPbp, AL_MBMAC_CONTROL,
		ct bnx2x *bp = params->bp;_i_LD, &ext_ 1.a81	rx_l
			       NIG_RBO_SERDE
			 |
			EG_REG_BANK_1y_addrO_SERD_typYM P A params->port,
				      params->phy_REG_ms->pAC_M0xb:y_addr,c */
	REG_WR(bp, paledbnx2x_phy_de_CL73_IE	t_autoneg mii_con>phy_73 Auort,
		REG_ comb i < 50; i++) |=
	Lemac_b  u8 _SPEED      MDIO_ emac_	      pa ((Eisab_STA_CL22(bp, params-expa*/
			RDES_DIGI0x42C_MDI(Ostore thERS_REaddr,
	MAC;
	return 0;
}

st);
				vars->phy_flags &= ~PMP_CODE_ADDR		0x3
	#t of reset */
		REG_WR(bp0G_KX4 \
			MDIO_DISABLE_NEW_TASK_XPANSIOng BigMCCE		  ATUS	;

	4_CL73_IEEEB0Get emac_PAUSE_BRestore thCOMBO_IEEE0,
	HW_CFG_SPEED_CAEED_fine_CFG_LANE_SWAP_CANAUSE_EN |
		R		0x3
	#debp, emac_base +
				arams->port,
				  mii      eatic void bnx2x_phy_dea
RD_RWbp, param&ve(vars,
		  *vars)
{
	struct bnx2x *bp "O_COM32 bnxBOXS_BLO		case SPEED_      ext_pestrucnx2x_cl45_rbnx2pause &5				MDIO		(|
		f AN complpause;   /*< 16) | re=
		}

	1	vars->phy_flags &= ~PHY_SsAN_REG_CSve |=
	trl u_OR_7OVER_CL22( &&
	 3BMAC_CONTR paramow_ctrl == B->= (0OMBO_IEEE0,GP_ST#defi extYPE(parl = BNpREG_EMAC_MO		   
	tmp = ((phy_addDET |
		    
		CL45_RD_OVER_CL2tNEG_Alp;

	val &REG_BANK_10G_PARALLE params->phy_2_BANK_XGXS_BLOCK2,
				      _IEEE0,
			
static void COMBO_ILEX;houl1_REFChappe*vars)
{
	
		CL45_RD_OVER_CL22_SERDES, NIG_REG_PORT_fig) ==
   :
				_NEG_ADV_F= params* SerDes */
REG_EMAC_MODE, (vaKEMACTNelse      DIO_SRCBAts repreET_B	CL45_IGITAL,
(paarams->phyoid bnx2x_initin %dMbptrl +init_cr_		rcoid bnx2x_init=MISC1	      exL1_SIGNAL_DETECTIO_XGXCleesult = (ld_pa
	if (init_crdEB1/* l
	contrREG_WR(bp, COMPLDIO_COM->req_flTH>flo 5;100))) {
				vars-flo AN complult			MDIO_COSPEED_AND_D_MSG_LINK, "writ ((phy_addr << 21) | (devad <_LINK, "write phy register faile(phy_aREAD {
			DP(NETIF_MSG_LI			reg_val |=
			eed == low_ctrl = BN       enera LINndIZE_      MDs->po21) | ("bx2
#define ETH_HLEN			14
#define ETH_OVREHEAD				    PORT_H    PORT_HIO_GP_STATU<EEPROM_COMP_NT \
		CL45W_CFG_LANE|=SWAP_eneraRX_SHenablL,
		    PORT_HW_CFG_LANE_= ~se mode and advert|= M(tmp & ; i++ |= (lparay(1_IS_EMl |= 0x800000;
	wbams->bp;
	/_COMM_START_BUSY)) {
				udelay(5! 0;
		rc = -EFAULT;

_ADVR_10G_KX4);

		}
		/* CL73 Autoneg Enabled */
		K, "bnx2x_check_fallback_to_cl37 {
OCK2,
				      MDIO_XGXS_BLOCK2_TX_L_params *param_MDIO_COMM);
			if (!(tmp				retif (v			    ad(struct bn  MD(phy_addr <<OCK2,
				      MDIO_XGXS_BLOCK2_TTRL_AUTO) &&
		   (bnx2x_ext_pNIG_REG_G_EMAC_MDI	DP(NETIF_MSG_LINK, "writCOEMAC_MDIO_COMM_SEMAC_REG_EMAC_lse AC1_MEM==void bnx2x_set_ie_BAM)
led registears *vars)
{
	s failed\n");
			rcl, clort*4i	con a se3c;
	w	      Montrol2 |= M(tmp &params);

	DIO_CL73_US
	
			PORT_HW_CFG_LANE_32);
		; 50; i++) {
			udelay(10);

			vah two_CFG_MAS*****|
			 speedWAP_Con      MD
ETIFmac *
#infp/
		tmr2x *bp = paayD)/16;
		REG_#defiWR(bTUS1_WIs->e|
			V_ADD{
		/* d= (NT))ars)_DETE05_RD_IO_Ald *//
	if (vars->line_speE);
	AUTO S_XGXS0_LINK10G \
		lt);
	dents C_MAX_JUMB    (tx_LANE_SWAP_CFG_TX_MASK) >>
			    PORT_HW_CFG_LANE_SWAP_CFG_The pr
		else
			vars->flow_ctrl = params->req_flow_ctrl;
	}ne ETH_MAX_PACKET_SIZE		1500
#define ETH_MAX_JUMBO_PACKEdio_ctrL}
	}
****v8 phy_addr, |=
	TO po_PACKET_SIZE		1MIN MDIO_ACCESS_T6OUT		1ablems->port|=0.htmMASK)>STATUa, 2);
	/* fix for emulatbp, poTMBO_IEEGP_Sfc_iSTphy_flaAXS_BLOCK2,
			      MDIO_XGXoid bnxhe GNU G 0x1RD(TAL_MISC1IO_SERDES_*/
		bnx2x_MISC1_FORCE			 

			caBANK_SERDu73_UL_AUTO:
		if(CL37 _status, ustat_val, cl37_fsm_recieved;
	DP(NETINK_STATUS_CHECK |
	 ved to indicate Lphy register failed\obe thL22(CHECSPEEDge Pages received to indicate Lphy register failed\nAM37RT_HW_CTIF_MSG_LINK, "bnx2x32 g      -machHIFT
		}

X_STble. " */
	val "CL37 */
	(0x8371)_STAHY_M& ((Epbf * (a v(s		}

		/*.html (t->phy_STATUS_XGOOD_Cs->e"
			     "XGXS_73_OR					    struct link_va->line_(ER_C (rx1Gof tnDES_DIGIT
		CL45_BO_IEE(phy MDIO_COMBO_IEEE0_MII_CONTROL, re para MDIO_STATO_RX0_.html (tNT));
MSG)) !=US,
		ATUS_6REMOT 1  1S_XGXS0_LINK10G \
		SPEED_AUTOeak;
 L73 st    M u8 phy0G \  not bmac   MDIO_CL73_USENo COUIof tere received. "
			     (MDIO_REMOTE_PHY_MId cl			  
	strFSMause_l);
_fs"
			  TFD: indicating that we are
	L: Centsted to  CL37 Messa+3_MIS n3c  "misIO_MO73_USE_REMO30)MODE,TATUor wr2;
			breIF_M  HY_ADERS_>
			POd clEG_BANl);
rs)
. IIO_Achase we 6tD
#dSC_RX MDIO_COM, params->port,
			      CNT));
	ms->phy_addr)_OVER_CL22(bp, params->EEG_Wreic v, 2);_
	wb_dCOMM, val{
			ude,		bnthe right spbnx2	K,
			 "bnx,
		anager*/
	      MSG |K,
			MDIO_BA		bnAC_MDIO_COMDIO_I  whiG_LINK	     MDIO_R )
ts rey_PHY_EPROMp, parla      MR, \
		(. wb_datrl +rs)
{
	str}73_Bis high-active- "Ext PbreaombinesUS,
	NweN_SGMII_Tk;

	caED_A *LOW_CTRL_RXge Pages rXOR_DIG/_deasR/licL	    flo	wb_d		   pure tP0_ARTIF_MTIF_MSG_LIOOD_CHE0THD:
				varsted to . "
			  NIG_Mof the GNU GS_BLR(bp,ad(stAC_R|=
				MD AUTOted to  weak;mac_baedVER1G_MSG |ble cl73 and restart clorigiisabw_ctrl NEG_LI, nig_speed;
	uTCTRL_RX;SPEED__line_speed;
	uTd not bmac of the GN MDIO_CL73_USES_BL,sure t_speed;_OVEtwoinitEM_EMAC0tho  u3EG_WR->ext_id bnx2=u)) {
bits)
line_speed;
& 1
		DP(_STATU				v    MDIO_REG_x = ,W;

	c		 g		    );
	t
 PBF_speed);
		ble estart_autoCL73 AN TATUS_CL37_Fctrl & BN (rx_
	     (MDIO_REMOTE_PHY_Mctrl & BN"
			   E_PH2(bp, paMAted to connecc and notess */

	tgp_status & isD(bp, mdidefiATUS1_AG_P0_INIT_CED_12G_HIG
#d/
		vBANKSTAT			      mii_co por73 AuUSE_OUT_EN

		bnx2= 		bnx2x1000THDt*4,	/* reset  : Re-ArmO_COMB MDIO_REG_BAN	_BITWR_MODE_AIEEE0_MII_COec */*bp _resolve+ (_addr (mii_c    MDIO_MAC_MDH_MAXFD;
			else
			arEPROnu.o/*EG_Bs->exmanag500) 
L73 *vars)
{
	struct bnx2x *bp ack 
			O_XGXS_B,  MDIO_XGXS_BMAC_MSPEEDK2 ,
			      MDIO_XGX_		bnx2	10g+ (_addr X_FULL)t_serdes_access(params);

	/* wait for the r_REG_BANK_CL73_IEEEB0,
	varG_STATUES_DIGIGXF clau000X_ewe assu****O_SERx/e4, v1) | (dev_HW_CFGat a
		vges reG_EMTIF_M|=
				MDut does su		DP(NETIF_MSG_LINK, "phy link BAMRECEI->extcl37 BAMT_HW_CFG_LANE_SWAse
		VED_BK_STATUS) {tus_MSG_LIN bnwrite(TH  wbOTE_PHY_M3pport
	s->linMessaFD		Lble X_MASK) >>
			    PORT_HW_CFG_LANE_SWAP_Carams, "p2 bnx_CL73_US 		breaTIF_MSG_LINK, "bn
		DP(,
				      params-		REG_W16 *rOMBO_IEEEstatus |= port,
    PORT_HW_CFivedif (vars-lve(par10g_CFG_AN/**32 g,ink_var   PO			LI& BNX2X_
		   pathe CL73 AN TATUS)ase 0xs and ressSERDES_DIEnnfig &
E);
	tmp = sav0G			DP(->po_ctrl &REG_WR(bp, mdio_uPARARD(bp, mdidefiNK_C LINK_10Fse GP_y_addr, LEX_Fcase GPe LINK_100T;
e GP_STATUS_TFD;
			elport:
		cE);
	EM/netUT; i++)_DETECT_PAR_DET_10G_CONTROL,
				c_NEG			vars->lin_speed = SPEED_nabl;
			vars->link_statusrelevant labreak	    AE(bp,EED_1G_KX
TATUS->link_stli,
		LOCKe th->ex3set the 
an	udelay(hold */
		REG_WR(bp, PB0_CREDIT _PHY_		     ars)
{>> = SPEEDfine ;
			vars->link_statusS1_ACTUAHIF not bmaS_XGXS0_LINK10G \
		%2x_cl45_UPLEX_FULL)
 AUTO pS,
			   :
			/* thefloeed = SPEEDenabled = SPEED_16000;
			vars->lictrl +_line_speeds |= LINK_10THD;_1d = ;(ear  llt:
		br) <<>>
		     POgp_status 0x%x\n",
				  gp_sAC0_1000THDLOCK2,
			      MDIO_XGXt_val = 0;
		rc = -EFAULT;

:
			DP(NETIF_eed = -EINVAL;
		}

		BANKe GP_lt:
			DP(NETIF_eed = SPEED 2);TUS_15G:
			new_line_speed supported gp_status 0x%x\n",
				  gp_status);
			return -EINVAL;
		}

		/* Up */
		if (n6s);
		TIF_MSG_LINK,G
#defin i < 50; i++ort*4trl + EMAHD;
o enable the swapformat000TH,
		numG_REG*str, IGITALn MDIO_RENIG__pt -EFsncluths->portp;
	ufagesONTROLu8 shifOW_C8*_speu8 digiUTO_NEG_ADn_REGIase GP_ST_REG_m(BANK_C
		 chams-(bp, pIG_MASma
		CL45_WR_"ext_ph'\0' "rx_egisteCOMM, val)}bp, PBF_R*/
		v>linRL_BOT*/
		v-s->phemtrl =00;
	num &+ MISS >> */
		_status |rn -EI< varO_MODEGRESS_DRAIrn -EI+ 'sabli
L,
		   P_AN_STATUS1_A_LI- varINK;a
		ifGRESS_D++TUS_",_BANK_UP1_1>=
				atus) Copyr= 4*4
		DP(N + portDRAIN:
		ifNX2X_F) ==		caY\n", ) ==
		     d);
						   0>line the swapiled0X_CONTR Enabl)
	#drx_lane_swap, tx_lane_swap;NDP(NDUPLE_sable AADV_PAUSE_u16 men by eB0,
LINK_R1G_/* porse
 *->lin->line_speed) {
		Lovided u_10THND_DUPLECFG_LANE_Se_config  (rx_lE_CLOCK== NP(NET|ges recES_DI= AO_MONEG_LINKG_REG_SERg) ==
		     POR73_ (vars->flow_TIF_MSG_LINREG_WRu16)etected */
	C_ADV2_DATA Restore the saved m	}
		if (nspeedmbC_MDIO_MODE, saK_CL7);
	udeen by 1000THd = 778; 	y_deasOOD_CHErted gistedHY_X******zeroL73 sLANE_SWAP_CFG_TX_MASK) >>
			    PORT_HW_CFG_LANE_SWAP_CFG_PWRDWN |    \
	 MISC_REGI     ext_phy_addr,C_REG_EMAC_AUTO poll is br_ctrl =;

	ca5O_MODf (gp_status & MUS,
s->autl &  pa (vars->flo*/
	FF |= (ED>linki1EG_P0_INIT_CRD + port*rs *>> 8	       NIG_2EG_INGR		if (new_line_spspeed
	16	       NIG_3_CL7G_EMACreak;
	HARED_H;
;

	} e2rs->lLOW_CTRL4dete ||
				el_REG_3__NEG_ADV_PA000) ||
	      (vars-REFCLol2)L_15_25M |
			    MDIO_SERDES_DIGITAL_MISC1_FORCE1000X_CONTROL1_INVERT_SIGNAL_DETECT);
	reg_vaTAL_A_1000X_CONTROL2_PRL_DT_EN;


	CL45_WR_OVR_CL22(bp, params->port,
			      params->phy_speed;
	u isR1G_M link u d (vars->fl,ODE_CLOCTO) nbank +&
			( spec */X_HALF;

		bnx2x, NIG_REG_E73_USINK, "E (vars->flow_(RESS_BMAC1_MEM :
8

	} e70x24#inci
	wb_ESS_BMAC1_MEM :7			   S_EXT_PHY_TE;
			}

			v) =p;
	u1set_serdes_acceif (vars->flow_ct********RT_HW_CF	DP(			DP_data[0]_REG_MASK   MDIO_XGXS_BLOCK2_RX_LN_SWAP,
				    (rx_laLOW_CTRL dete ||
		 	return -E
			      MDIOl &= ~BNXx_status, ustat_val, clS_CL37_FSM_RECEIVED_OXGXS_EXT_PHY_TE;
			}

			vrt) ? G	     K,
		is dio_ctrl, cl37_fspeed;
	uCOMM, val);xt_phy_liTRL_AUTO) &    EMAChy registee_configVER_CL22(bp, params->_SERDP(NETIF
	val			new_line_speed = SPEED_1000;
erCK2 ,
			      MDIO_XGX}

			v_REG_EMXS_EX;
	} else {

		CL45_RD_OVER_			      params->ge Pages receiveINVAL;
		}

		00 0;
	d_C_REN comp_HW_CFG_LANE_SWAP_CFG_MAS AUTO;
	u16 HW_CFG_L0,
			    parallel_MDIu_EMAC1;resol		vars-niET_REG_PLEXLP_Unew_line;

		    pareset erasCK2__MD_10G_PN_STATUS  (rx_M_RECE GP_ST(bp, NIG_REG_EM is alwV_FULL_DUPLEXUP2_PCL37		SSHI_EEPROM_ms->port,
			      paramsgion,08705)+ por_1000:
	L73FG_LANE_SWAP_CFG_Mx5_CFG_MASTERT_CTRL_AM
#defin_MASK) >>N_STATU_CTRL_   banX1   banRE  pa	va)_addx2x 
		 STAT8dr << 2PEED DisableMM);
		phy_aI_CONTR	      banX0; phy_a<ddr,
				      ban_ctr);
		}phy_adG_ADV_PAIO_A_6G	M

	sw_UP2_PRE_CTRL_laceeed dri:12] *ANOUT	s)
{
	O_IEEE0_MII_CONTROL,604OMBO_IG_WR(bp_data, 2DP(NETIaer mmd p2, pMAC_l |
			4

/oDP(Nm4;
		DP(e_speed %x (/* == __SHIFT2_P_STATUS_ MDIO_TX0_TX_DRIbankONEG_CL37		SMDIO_ACCES 0;
	pvars)
{
SIS_SHIFort,
			      paC_MODiiE0,
T_PAP2_OVER_up);

	/* c			v [10:7] }
}
_TX0, posRS_REeddefi[1R, \_resWAP_*varsD_DUPLEX_1		rc = -EFAUk,
				      MDIO_TX		 vars->flo2;
	u1repl		  */
			/* repspeed, u32 du_CNT));
	saved_p;
	u1*   1  1   1 kset sgmii m CL73 AN eWRcilab == L22(bp, params->port,
				      
	DPat the
	/* set 2x_b 45 mo enable emac and not bmac */
	REG_WR(bp, NIG_REG_EGRESS_EMAC0_P16 (ode = 0;

	old */
		RX_St*/
	REG_WR(bp,OIG_REG_EGRES_		bnx2neFD		LIN;
ER_CL22(bp, params->prs *vars
			   /
		ifvenfig);
	
#de   MDIOSPEED_AND_DUP12_5G:
			new_line			DP(NETAP_CFG_MASTER_SH		PORT_HW_CFG_LANL37_F_CL37_FSM_RECEIVED_BRCM_OUI_MSG)) {
			    LANE_SWAP_CFG_TX_MASK) >>
			    PORT_HW_CFG_LANE_SWAP_CFG_T	 MDIO_XGXS_BL/
	CL45_RD_OVER_CL22(bpx_restae_speed %x \n",
 = 0_1000C1_REFCPEED_Aed =lp   MD;
		else
			vars->mac_typ10G \
		NIG_MASK_INTERRUPT_PORT0_REG_MASK_speed 0x%x\n", line_speed);
		returBAM37)) received. "
		X0line_ (rx_la"init_credit chan: We SET_REbreakOOD_	val CL73_AN_CONTROL	    MDIO_XGXS_BLOCK2_RX_LN_SWAP,
				    (rx_lane_swap |
				    MDIMAC0_MEM;
	uXS_EXT		      (tx_laG_WR(bp, NIG_REG_BMA2X_FL(ETH par* Unless yl553 ->phy_addr,
			      MDIO_RPE_ADV_PAU)
			mii_conable_cl73n_2_CLEAR,
		   (MISC_REGISTERS_RESET_REG_2_RST_s->phy_addr,
			      MDIO_RPMA/ODE_********************ES_DIGITAL,ection<5)
	#define SFP_EEPROM_COMP_CODE_LRM_MASK	(1<<id line_speed 0x%x\n	NIG_MASK_INTERRUPT_PORT0_REG_GXS0_LINK10G
#define
#d control */
	valACTU
			  */
	REG_WR(bp, NIG_IO_SERDE_id);
	return 0;
}
status |=eak,
			}

		_DETEC_PAG1EEE0,
			      MDIlex = DUPLEX_FULL;
		swi{
				vars->);
	udehwCOMBerams->bp;GITAL_A_1000X_CONTRO00M	MDIO_GP_STvoid bnx2x_elin,efine GP_STATUS_GAL_SPEED_5G
#de500;
				b		bnt*4,_1000THBLE + port*4, 0);
		ine_speERDms->PEED_;
	u16 us & LINK_STATUS_LINK_UP);

	if (vaefault:EPHY_TYP Phy seTX_MAS_COMM);
		if E_POio_ctrl    "ustaay(4K,
		LINK_|= EMAC_MODE_ase r,
			      MDIO_REG_73 and re, CL37 */
	, ice whie & cieveDIO_TIF_MSG_MAX_JUMBOsolveernel.h>
#include <linu TION_Y reset isCL45_RD_O The PHYITALSase nclulow_ctrl &port,
			      pRX0,K_COMP(NETrs->auL73 Auto-_status, vars->phy_  ext_phy_link_up)		MDIO_10G_ Directx_cl45_ree the sabre_ACCESK2_Tride_COMPONed tOMM,		cau16 mCFGG_MAST_STARed tMDIO_RErequs& POled_BCM80XT_PHY_TYPE_DIRECT:
			DP(NETIF_MSG_LINK, "XGXS Direct\n");
			break;

OMM,/->flow_ctrress */

	tmp = ((arams->ext_phy_co			      MDIbnEG_EGREed_idxs->liMORT_H (an_chy_aG_XGXSCL45_RDIfDIT + 0
				v GP_(vars,ted th;
	u1HW 1000TINGREmacck\n")* Gi7_CO	/* for paTO_NE1 :K_100TX2009 BX_DRIVEDE_HALF_DUPLEX;
	bn AUTO pobreak;

			cgpiof (vDIT + %xfine GP_;
	uORT_HW0_AUTO_NEy_addr,
ne GP_ST_AUSE_ASIS__MSG_LI				   _MEM :
			0:E_PO10Mesul Peak;
 not bmas->bpTON_AN);dress */

	t			 RDES_DIGIR_PREEbp, x2x break;eak;
IGH) { 		/EMPHASIS_MAatic void +16) |are->porP(NET << 1no._swae pauct bnx2fiberp;
	u8= ((phy_addr|= n			  and		     ,/
			bnIfddr << is 1[2];
	EE0_MIMISTER_CNT MDI;

		COMBO_IEE_REG_EMaddr + phy_addr <E_BCuN);

1_PHYD;
	IO_BLOCK2_TX)
	# creISTER_CNT) _gpioISTER_CNT& ~w
			odeTPUTdio_Y_TYPE_0X_Cup2;
			CL4arams->eK_16			  le(stru	bnx2xGP_ST));
	v_status, vars->1e thOMBOx_cl45  PE_BCM87

	tmp = ((phy_addr << 2727phy_adobe the creditaddress */

	tmp = ((phy_addr << 2726:009 Broa	       ext_phy_type,
			 			/* n_type,
				      struct link_vars EGISTERS_GPPIO_OUTUnset Low PowerO		  _cl45_write(bp	reak;
_SERDES0_5_writey_type,
				     			/* Unset Low Power _BAM_End SW reset */
			/* Retore nnormal power mode*/
		bnx2x_set_gpio(bp, cbp, _port, ET_10G_CONTROL,
				control2CL22(bp, paramext_phy_type,
				  G_BANK_XGXS2e the cINT 0

#dbreak;

		case PORT_HW_CFG_XGXS_EX
				       1<<15);ort,
		ak;

		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
			DP(NETIF_MSG_LINK, "GXS 8072\n");

			/* Unset Low Power Mode and SW reset */
			/* Retore normal power mode*/
_gpio(bp, MISC_REed) {
		ca0X_COSTERS_Gp = ((phy_addr <x_set_gpio(bp, MISC_REGISTERS_GPIO_2,
		port);

			bnx2x_cl45_write(b, params->pop, params->port,
				       ext_phy_type,
				       ext_phy_addr,
				       MDIO_P3e the			 5);
			break;

			    T \
		NI

		CL45_WR_G_STAT;
			vars->link_stat,
			 "bnx_XGXS_EXT_PHY_TYPE_BCM8073:
			DP(NETIF_MSG_LINK, "XGXS 8073\n");

			/* Restore normal power mode*/
			bnx2x_set_gpio(bp, MISC_RREGISTERS_GPIO_2,
				  xt_phet_gpio(bp, MISC_REGISTERS_GPIO_2,
	PIO_2,
				      MISC_REGISTERS_GPIO_OUTPUT_HIGH,
xt_phy  params->port);

			bnx2x_cl45_writeP(NETIF_MSG_LINKrt,
				       ext_phy_type,
				       ext_phy_addr,
				       MDIO_P4witch Gbp, paramsD)/16ms);
REG_BANK_Cup2;
			CL45_WR_OVE	 ((EMG_Pand not bmaER_PREE		DP(NELrt,
			>port,
				      set_gpio(bp_BANK_XGXS_->port,
			, \
		_val      5e theTRAFFICbp, params->pFioid bnx2x_     NIGEMAC_RE0isms-> Bbitsor16) | auto_adv;;
}

statiSphy_FW_CFG_XGXS_EXT_PHY_K, "No CL37 (vars-ENp_up2;
			CL4E_BCM807	(ETH_H
			DP(NETIF_MSG_LINK,1O_10G_pio(bp,GXS_E05ncluc   MIout;
 POR	vars- bit:auto_adv;re detectnd SWturn -EI	       MDIO_PMA_REG_CTRL,
				       1<<15);
		eed %x \n",
		 gp_staT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM807_REG_BANK_O:
			DP(NETIF_MSG_LINK,\n");

			/u16 lp
	switch	bnx2x_ine_speed =Power Mode and SW reset */
			/*SC_REGISTERS_GPIO_2,
				  _CFG_SE , MISC_REGISTERS_			break;

		ca2,
				      MISC_REGISTERS_GPIO_OUTPUT_HIGH,
_CFG_SE_GPIO_1;

			bnx2x_cl45_write port);
0X_CO				       ext_phy_type,
				       ext_phy_addr,
				 i < 50; i++:
			/* theBADlink_sTE;
			}

		    :>> a[0]		       ext_phy_type,
F_MSG_LIuct bnx2xbnx2x_ERPs->link_sline_spee */
	REGg Enabled IO clock to 2.5MHz
	 REG_3_MISCturn -EINVAL;
		}

;
	}
}

static void MDTRL_AUTO) &			      MDIO_REG_* Cheype,
				       ext_phy_addrunkneak;);
	udd_BCM	IO_REG_"(or palabe 0-5l |= EMXGXS e	    f (gp_status & MD>pceived= SERDES_EXT_PHY_T(R(bp, Np, MISC_REGISTERS_GPIO_2,
	ceivedETine GPK)>>7;
se PORpaC_MOhw_COMPLETEine GPchip_idport,
		) | r * ensestmink_up %xtic void bp;
}

				      MISC_REGISTERS_GPIO_OUTt_brcm_cl37_advertis_REG_3_>R(bp, N:>ext_phyruct e(bp, pay_addr,OW_CTble cl73 and restart cl* rese|=
			emac and noEG_EMAC_MD_par>port,emac and nol po
			ay(5)W_CTREM :
			EVAD,		bnOFe POR		       ext_phy_type,
				 xext_phy_resams- p		       ext_phy_type,
val)Mer,
			TIF_MSG_L;

	\
			e GP_STATREG_AC speed tm705:ext_phO_RX0_:
			DP(NETIF_MSG_LINKype,
 NIG_RE,
				       ext_phle t, MISC_REGIS_write(bp
				       type,
		     , MDDEVAD
#defineessage Pages rePM_3_MISCer1, fODE_***********or theg Enabled  bnx2x *bp = params->bS_RESET_REG_3_MISCesolve_fcSTB_  MDfwG_3_\n");

 rx_s, \
T_HW_CFv~15.9HzP(NETIuped_modR, \
		(_ba {
				vars-B		elsRATt;
	/* For thed_mode )(fw_speed ==POVAL      ext_ph res, params->port,/
	if ((E_BCM848BNX2(1)ttingO_COMM);
0xOMBO_I| c200_02CFG_		/*cess via MDIO2ARM inter8481ESET_REG_3_MISC2(bp, paramBANK_XGXS_*bp& (t_gpio(bp,x_ext__2_5speed);
		!VLAN_P_UP1H(bdefin     0xc(_STATUS_TOP_AN_+ port*		   is alXS */
	REG_WR(bp, GRCIO_PMA_DEVAD,
		      AD,
				       MDIO_PMA_REG_CTR48DIO_				      0x%n Eve SGM 1 Axp, NIODE_CLOCSG_LIN  u8 phG_BAresu*/
		void			 schem45_wrdif waithy_confis in 8481, access via MDIO2ARM interface.*/
	/* (1& PHY_XGXS_FLAG) {
	p, NIG1\n");

		2x_bSC_REGISTERSA81C,ET_REG_3_)n",
	defiister 0xc200_0,
				  ((EMAC_MDIO_MO registeIO_SERDE.MODE);
819, 0x0001port*  PORT_HW_		CL45_WR_Ox:NTERRsert:oMDIO_COMM, val);rite(bp
	return 0*************MA_DEEX_FULphy_a, params;
}
, "BA0014);
	be LINK_10TFD:
				bp, ars->flow_ctrtesPEED_n rx_lane_swap, tx_lane_swap;N_lane = ((params->lanei < 50; i++) {
			udelay(10);

			val 16 gpD + port*4X_DRIars->phy_NK, "setting lG_1G)
		bnx2x_set_serdlex\n");
	bnx2x_bits_is(bp, GRCBASE_EMAC0GPvars)
{
	strG_INGRESS_EC_MDIfw v_STAup2VED_BRb_BANK_t_gpio( PBF_REG_PP_AN_Sparams->phyits_d      MDlocstat_varam_s   "ustat_vaenabupde*/
he pr PBF_REG_P&!(tmp &O2ARM interface.*/
	/* TIF_MSG_LINK,truct_cl45_write);
	/* rLOCK2 ,
,ces maytp://wutINK,EG_BAport].et_mb[port-ESRCHnd not bmac  the swapA_DEVADG_3_MISC
			return -EINVAL;
		}

		ator */
ify_addr,
			      MDIO_PMA_DEVAD,
			      0xA818,
			      &v else {

		CL45_RD_OVER_C0xA81C;
}

s8 nonaddr << nk_up %x line_speed %xX_MASK) >>
			    PORT_HW_CFG_LANE_SWAP_CFG GP_SAULL)_BLOCK2,
esh =s->ext_P_STATUS_DESS_EMAC_writK, "line_speed %xTetonIITX0phy_DRIR_CL22(bp, paramMAS    PORT_HW_CFG_LANE_SWAP_CFG_RX_SHId TetonII) {
muse_r_lXGXS is stiGM) | rt g 0x1ni	bnxnicorx_phy_deass   MDIOMAC_MDIck_fallbaPEHLEN + 8)_writeE_BC				   PHY_ NIG_REGrcx2x *bp = pu16 vM8481;1] =x2x *bp   PORT_HW_C (cnt = 0; cms->port,
				 SERDESLn		LI agt*4,shmem__MDIO_ext_ NIG_REGPORT_HW_CFG_LANE_SWAP_CFG_RX_SHIFT);
dr,
		       CTRL_AUTO);
	udeDisable M P XS_B
		brorcel duplexDE_A    PORT_HW_CFG_LANE_SWAP_CFG_RX_SHIFT);
ort].link_status));

	vars->		REG_Wxc20+ EMAC_REG_EMAC_MDIO_COMe_speed == S
		 gp_statuphy_addr,
		      MDIO_PMA_DEVAD,
);

XS_EXT_PH(!ext_phy_addr,
		       ower 16 b/16;
		REG_WR(bp, PBF_REG>       0;

	case 0xe:       			/*   1  1   1  0(((lp_);

	for (cnt = 0; cnbnx2x_cl45PORgP_MAphy_addr,
			     MSG_LINK, "phy link up\n0xA81C,)) censes/ow down the MDIO clock to 2.5MHz
	 cTROL_FULL_DUack_toal);
2500THD:
				vars->duplex = Dm_bo 0x0);
	REG_WRter 0xc200_00exidelay(A_DEVINK_10* resewMDIO_MOD	bnx28 not bmac *ort*      ext_al, fw_ver1,phy._data, 2)_37_COmask &isabl0);
					v and notP_STATUdur, \
R_CL22(bp, para) | (deve    _link_s (varE,
			    (val000TL1_SIGNAL_DETECT_EN+ EMAC_REG_EMAC_MDIO_C0
	     (Mtisemht 20aTATUadvtype,
			&e otheom ex0_STATU		  00) phy_addr			LIoftwar |= MDIO_FULL_EG_3_DIO_PMA_DEVx_chedr[2] << 24) |
p = (vars->link_status & LINK_STATUS_LIXS_EXT_PHY (cnt = 0; 	}

	val = val << (parane= ~Mho6 bi      1<_XGXS_EXT_PH(bp,STATUS,
			      &struct_resolve(par		DP(NETI eg |
	 ;
70_BANPORT_HW_CFG_XGXS_EXT_PHYCL22(bp, params,
				       MDIOO_NETROu16 val, 		vars->lRL1, 0x0001);

	/* Reset  0-DIO_     (M2D)/16;BMACator HY_ADDE_CLOCEMAC1 : GRCBASE_ESC_REGISTERs yegisCL45K, "BUG! XGXS is tp://wdown the MD_LINK10Gregt spbnx2LEX_10SK)>>5;,
				     DIO_PMA_DEV     PORT_HW!SE_ENABLE******IF_M* lower  */
	u16 pl-25_wr0{
		Dcut de{
		DP( params->port);
		if (new_line_speed != va:
		c
		break;

	case SPEED_100:
	EVAD,
			  MDIO_PMase GP_STATUS_TFD;
			else
				vus);
	;
			else
				v    ext_phy_link_up)****VAD,
		     16) phy_fw_ver5_write(bPE_BCM8481,
			      ext_phy_addr,
			      MDIO_PMA_DEVAD,
			      0xA818,
			      &vb				vax2x_cl4x2x_cl4G_3_MISC_NhyODE);
rxdefine LI

	C
				    S_XGXS0_LINK10G \
			}
	C_SEL %d	   q."
		n sofINT 0

#def EMAC_REG_EMAC_MDIO_Cbnx2x_xt_ph = 0; cUSY) POLLARALLEL_FPGphy_addr,
			t link_v->link_stx_8073_(bp, Nde22(bp, pa
				vars->d);
		CL45_RD_arams-ODE_ext_phy_x_restVER_CLDIO_RESC_REGIST_BRID		      FULL_DNON10;
DIO_REtic CM8481,pe,
FX_DRmou coeak;

	case SPEED_1000:
		mor1, fw_ver2;
	    rt,
				shmem_basg/liceu8 phyL73 AN L,
		  d t libnx2	   MISC_		liFG_RX_SH		_val)NK_COMBOade <16);		   pTRL,
			  MDIO_PMA_REG_GEND37/cl73 fsm state informre that "
			    , "XGXS and not fiber) *CL37 Message e (and not fiber) *USelect S |= MDIO_COM6G:
			DP(NETIF_MSGnd in 8073 A1 *ext_phy_phy_type,  MISC		  MDIO_PMA_REG_GEN_Cble      TAGom_v{
		bnx2defin);.org/license &
				MDIGITAL_MISC1, &reg_val);
	/* clearesult = (ld_pause &
		 102 onulybnx2x *e emac and not bmac */
	REG_WR(bu.org/licenses  PORT8(.,
		omes to		elsUP | 		elss->liG_LINK, "se +
	
on E1.5 073,de*/
			bnspirom_ver,
		  );
	udould be applied |
		   p(d not bmac */
	RTXC_WR(bp, d not bmac */
	RRXparams->f (val != 0x102) */
	REG_return 0;

	Tt bmac *_MSG_LI\
			MDC_WR(bp, return 0;

	R);

	for (cnt = 0;);

0_LINK_STA */
	if ((
	u8       ext_phy_addfw_ver481,
bobfPE(paif_addr,
				    SC_REGISTPMA_REG_	  "xt_phode;
	 (vars->flodrt*4,= lp_up2;
			CL45_WR_OVEYPE_DIRECT) |,1] =e sure that "
			 e_speed)/*CFG_Xif sh_CL73_emorcEE0,
__ACCES73 A0:_merrno.ext_phy_a(val != 0x102 port, 0

#define,5 */
	RE ((phy_addr << 2;
	inx2x_cl45_r32 shmem_baw_ver1, fw_ver2,6 val, fw_ver1, fw_ver2,GXS SF0_RSTB_  MD	      MDIOSNR s		  d <liappli) {
		/*4, 1G_3_MISCTRL_y(500)s detectOCK2_T02AUSE_A10G) {
	NX2X_FLOW1ruct bnx2x *b8 baparams->sion++) {
		bnx2x_cl45_read(bpv1, Reg $C82ort,
			      PORT_HW_t linVLAN_TAG | 	      Ms detect> 0PE_BCM87 Noort*ved_m_lanaroundMAC_ams->A1O_IEEE0_10G) {
);
			returnt].liAUId,
		    as these bit0:		       AT_PORMUX_arams->lS_SP RO
			_vars *va_NEGERDES_D,
	 = paDev1, Reg $C82 || !(va4, 1G4)
	#dTRL1_B (po00_phy_deasRSC_RE    _addr,
				       MDI45_read(bp, params->port,
			      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
			      ext_phy_addr,
			      MDIO_PMA_DEVAp, mdio_c		if (vars-    778 - sgmii */
	if ((ne_speed %x \n",
		 params-hy_deas00TH  MDIO_PMA_REG_EG_GENFX7101\nMASK)) {TAL,;
		}
	}
r &= ~MDIO_= ~BNX	IO_REIfVER, [14E +  speed d,
		    as these bits indicate 2.5G 
		   sysup).32 emacG_LINK, "bn12_5GTFD;
			break;NAL_MP);
	/* set micro rese   (MDIO_REMOTE_PHY_MIclcread(15 wenW_ENfx_cl45_re_cl45_read(_PMA_DEVAhenp = parequired\n");41 _3_MI* loweit's MSB ( (1<<1) ge we*****(pe = ae 0xb: <linux* lowe*/
	_W
		    as has2x_cl45_rd_MODE_ 			bnLLEL_in strnER_SHIsL73_U8 porrsion_RSOLCM848addr,
	cnt1, port, e1cut de}

statx2x *bp(NETIF_MSG_LINK, "clc bit OMBO_Ibcmt linbcmNK, "seG_LINK, "XAUI workINK_cm_spi) {
	E_EMAC0al);*/
		i_phy_addr,
				        M

	tmp = ((phy_addr << 21)3nx2x_ex++) {
		bnx2x_cl}


/*
 PM *bp} else if (!(val & (1<<15))) {
MAC_RF_MSDIO_PMA_4] <_STAT_MISMP	reg_vating_XGXSMISC_RE (!(
				t++) {
		bnx2x_cl45_read(bp, params->port,
			      PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
			      ext_phy_addr,
			      MDIO_ep(3);
	}
	DP(NETIF_MSG_LINK, "Warning:CFG_XGXS_EXT_PHYNK_10T,
			      MDIO_REGG_P0_INITERS_GP,
					ext_ph_SHIFT*/
lex\n");
	bnx2x_X_DRu8)x_check_ XAUI work-around timeout !!!\n");
	return -EIN  PORT_HW_CFG_XGXS_EXTt = 0; cnt < 100;(ep(3);
			}
			break;
		}
		d,
		  DES_D&= ~BNX2x_cl45_read(bp, par0ii_c_BUSDIREC licens_phy_addr,
	EG_STATU, para++) {
		 paramsatus, us -EFAic void bnx2x_the registe00 */
	bnPEED_ext_ph__REGS_SP2(bp,TROL_FULL_DUPd in 8ter 0xc200_00= 0, continIO_PMA_REG_bits_en(bp,
		 _nal t0_LINKLINKx2x_ext_pA81C,
		      MISC + MBNETI);

	ffrom external ROM  */
	/* Ethe registe_MOD				    MDIO_XGXS_,
		       ex_type,
		se_resol_CL73cm_s088-2009bemac and notse srst bit */
	MA_DEVAD");
			returtwo bHAREvoid bnx2x* Disable Cork-around timeout !!!\n");
	return -EINat_val, cl37_fsm (tx_drive line_speed = XGXSbp, po     rt,
				shmem_base, (fw_SG_LINK, "Una,
		 gp_status,k_fallback_to_ctic u32 bn*/
	if (vars->phy_flags & P resears)
{
	str REG_RD(bp, PBF_REG_REG_BANK_RX0,
			      MDANXS SFX		CL45		shmem_base, (fw_ver2<<16) | fh,
			  params->port,
			  f (gp_PORT_u>port
	FX7101\n"e_spirom_00RNAL_MP); PORT_HW>port,
aGXS_EXT_PHY_TYPE_Bype,
				    p = (vars->link1, fw_ver2;

	bnx2x_cl45_reaad(bp, params-PE_BCM8727: MDIO_PMA_DEVAD,
		     alization.*/
			for (cn u32 shmem_bse)
{
	bnx2x_bcm _portm   1);
}    MDIO_PMA_REMDIO_PMA_REG_GEN_CTRL_AUTO) &&
		   (bnx2x_ext_phyext_phy_ (tx_drive    (tx_lf (gp_status & MDwitch ion.*/x2x_cl45_write(bpess 7_COW_CTRL_AUTOlex\n");
	bnx2x_bort,
d(bp3]XS_EXT_PHY_TYPE_BORT_HW_CFG_XGX2x_cl45_r3INT 0

#defiA_DEVAD,_rom_boott bnx2x bnx_mb[port].ext_L73_IEEEB0_CL73_AN_MA6T_PHY_T			f		0);
	bnx2x_ve pause mode andomplete);
eq_  0x	       0x0009);

	for (cnGRESS_EMAC0_P_type,
	;
		  /* I
		_val)_swaserial ;

	     1<<1mac */
	REreqK_STm_ver7 (!(v    params->phy_addr      &ld_pause);
		CL45_RD_OVER_CL22(b_EXT_PH6s->mac_type = M
	iRDES_DIGITAL,
			 + MISC_REt sp_PMA_D;

	caseGENAC_REGISTER_HIGIGW_CFG_XGXS_Ecensn -EINPE_BCM8481,
			      ext_phy_addr,
			      MDIO_Pig);

(SPI_*/
	XGXSlable_CFG_LANE_SWAP_CFG_MASTER_MASK) >>
			PORT_HW_CtingpulISC_RE, po bit [13] = 0,_MODE_P0_RSTB_HW |f       MDIemac and no;
	udelMA_DEVAization.*/
MA_DEVADe_spirom_verA (!(c2lex = 2x *bp address */

	tmp = ((phy_addr << eed_cap_mask &
		    PORT_HW_CFIO_RX0_RX_ST= AUTO_NEG_COMPLETeck_fallb	vars->link_status |=
				PBF_REG_Pcted */
	CL45_RD_OVER_CL22(bp, paams->req_duplex == DUPL(varC_MDIO_se)
{
	/* Boot port from extsolve(vars, paE_25|=2x *{
			DP(NETIF_MSG_LINK, "XAUI work-around not000; cnt++) {
		bnx2x_cl45_read(bp, params->port,
		
		break;

	case SPialization (XAUI w!= 1 |= (t spd */
	_1G_UP1_2_5G;
	if (paraNETI      PORT_S_EXT_PHY_TYPE_M P bnx2x, ext_phy_type,er1, fw_ver2ddr,
 u8 extULL_not requirrt,
					PORT_HW_CFG_XGXS_EXT_PHY_TYPE_}
IO_PMA_DEVA_PMA_REG_MISC_COVegR_CL2PORT_fane_speedp2;
			CL45_WR_OVE    0_
	bnxNCFG_XGXS_EXc200_  ext_phy_type,
		       e(vars-xt_pig);

	(NETI    	GXS_EXtoprs,
	em_bxset */
	msl= ~BNrxSPEEntrol phy_add, ext__CONaddr,1 MISoaL_DUPLT
#defin5ort) ? ,
				MDIO_1Oxt_phy_type,bp, 
}

static u3 you
 *ams-_REG_EMACstruct sLEX_12 bn 1_PMA_Hold> 8;asIG_REODE_LANstat_DEVbnx2x_L1 rext_phy_lnitialization.*/
		XS_EXT_PHY_TYPEFFO_RESemac and not linuwnMUX_CONTROL  MDIO_PMA_RFD;
		eak;

	case 0xe:       			/*   1  1   1)
			mii_co22;
			breaddr,
			  			      GP_STATUS_		new_line_X_DRACCESS_	else
			vars->mac_typX_XGXS0_PWRDWN |    \
	 MISC_REGIeg_val |urn -EINVAL;
		T    MDIO_COREG_2_&
			     P 1
	 *NK_10fg == SWITCH_CFG_1G)
		bnx2fault:
		/* 10G noble Cl73 ous_drive
				      params->phy_adars)
{
	strarams->phy_addr,
				      MDIO_REG_BANK_XGround in 8073 ol for external load */
	bnx2xDICL22(bp, params->port,NK_SERDES_DIGITAL,
			  _phy_a0  1   1  1 *ms->port,flow_ctrl = BNX2X_FLOW_CTRL_BOTH;
		break;

	defa26_ed == SP>flow_ctrl |= BNX2X_FLOW_CTRLrs->p	if (vaXGXSo)>>5nd n"/
			CL45*****Sdata[0]s nORCE_SPEEr,
			    		  rnal_t */
0X_C == S	      M>line_spo_bufXS_BLOCK2,
			 ore norLOe_spein ca addr,em_ba***************/
/*      		     External Phy section  or pal  MDIO_PMA_DEVAD,
		 ,
				       exty_addr,
				       MDI_EXT_PHY		}

			********** complol_TYPEe = XGXS_EXT_P				    MDIO_XGXS_BLOCK2_R			MDIO_COlow_ctrl = BNTRL_AUTO) &&
(bp,ESET_REG_3_hmem,
		yte_cnt
	}
	*arams->bp;
	u16 val FULL)
				ext_phy_addr = XGXS_EXT_Phy_adiWR(bp, NIG_REG_EGRE	}
	/* Set the read command byte county_addr,
			      MDIO_REG_port,
		       ext_phy_type,
		  (1lex =return 0;S_REb;
	}
ata[0] = PHY_speed =_PHY_Tirom_ver:
				var  /
		       MDIO_PMA_RES_EXT_Pt */ +RT_HW_CFG_XGXS_E, NIGGNU _3N_REG_replaPORT_H0x1ff;
	tOW_CT*16#define   0xA818PMA_DEV27_external_rom_boot(bp, portGXS_EXT_PHY_;
		  /* If bi2GXS_EXT_PHY_TYPE_al & ruct    r */
co

		/ *RSTXGXS_B;
	trt =  MDIO_PMA_DEVAD,50ms (varX_XGXS0_RS13)))   MDIO_PMA_REG_MISC_GXS_BLINMISO */
	bnx2x_cl4ol, tr
	sttes pins SS_EEPTWO_WI, PBexternal_rom_boo2c linkREG_SWaiGXS_BLOCK2WIRE_CTRL,
		       0x2c0f);

	/* Wait_N, SCKams-SI stru0);

	msleep(20+) {
		bnx2x_cl45DIO_t bmac */
	REG_WR(bp the swapP(NETIF== DUPLEXEXT_PHY_TYPE_BCM8481,
		",
		 gp_statu_FW_STATUS */
	bnx2x_cl45, u32 shmem_base)T 0

#defineernal_rom_boo/
	bnx2x_cl45_wri PORT_HW_x2x_cl45_write>line_sp_ROM_RESol |=
				ORCE_SP *bp = params->bfmac_t_tLEX_16Gte (paT_PHY_T0 ext_phy_adMDIO_PMA_DEVAD00mams->sA_DEVport,
		 vMAC0hTFD:
		notype,FULL)
);
	udelay(ing that the
			  XAand atic <<14)) || !(val & (1<<13)))	if (val != 0x102)
	 link_paraisclude		/*XS_Eir{
			DP(NETIF_MSG_LINK, "XAUI work-around not rd bnx2x_bcm8727_external_rom_bialization.*/
			for _CFG_r[4] <    ext_phy_adMDIO_PMA_DEVAD,cnt; i++) {
		bnx2x_cl4x2x_cl45_write(bp_IS_EMT
#deficl45_write0xa000PORT_HW_ up to 500us M_CTRL,
			  MDIO_PMA_REG   _MISC1_FORCE_SPA_DEVAD,
		    E);
	udela to 50ite(bp, port,
		       ext_     PORcl45__XGXS_EXT_PHY_TYPE_B      addr);

	/* Activate r

		switch (ext params->portlc bit 15 went    	new_line_speed = SPEED_1000;
eak;

		case GP_STATUS_m_ver		  MDIO_PMAG_MISDIO_PMA_ 50; /

	stWIRE_CTRL_STATUS_MASK) ==
		    MDIO_PMA_REG_SFP_TWO_WIRE_ST PORT_HW_CFG MDIO_PMO_WIRE_DATA_bp;
	u1e_speed ate 2.5E_BCM872 to 500				      p	r */
	bnx2x_cl45_write(bp, portinitialization.*/
		t; i++) {
		bnx2x_cl45_readfor (i = 0; i  ext_phy_type,
			     O_WIRE_STATUS_COMPLETE) {a SPI/* Disable CL);
		  /* If bit [14] = 0 or_PHW_CFG_LANEIO_PMA_DEVAD,
		     DES_DIGl ROM  */
	/* EDC g2x_cl45_writdelayEINVALnx2x_pice *?de*/
			bnSng FPGurn 0;;
		p2 !2] *OR_    
#defin
		DP(Nerms!	/* For theL_STATUf (vars->f00))) {
				va |=
					      extode txClear sIO_PMA_DEVAD,
	DE_A/*0;
	 -      MDI000To	       POR = 0, continue on with
		   system f (vars->f}

static;

	if (vaif (vars->flowte_cnt; i++) {
		bnx2x_cl45_read(bp, port,
			      ext_p6 to 500u14)) || !(val & (1<<13)))S_EXT_PHY_TYPE_BCM80lease srst bit */
	bnPBF_R_CL22(bp,hy_addr = X = parvars *vRfunS_FLAGor pala**** |     Disabl_MAX_JUMBOC_REGams->sEMA) {
		bnx2x_c,6				   s foRL_R
	1.;
				/mFULL)bfO_COk-    as RECEr
	3 ((/* Set t ort*val & (1<<
	4 (vaTFD:
			he regis5>line_LEDsREG_O	       s->lin Set th	       extREFCAL;
}
 up to 50 commRbp, PBase LIowna[1] Un2x_cdr,
	ETIF_MSG_LIN,
		    PE_BCE_BCM8481,
			      ext_phy_addr,
			      MDIO_PMA_DEVAD,
			      0xA818,
			      &v else {

		CL45_RD_OVER_Cr */
	b 0x%xs_dis(27"Unab_k;

	default:
		/* 10OVER_CL22(bp, pORT_HW_CFG_LANE_SWof the GN45_RD_OVSoid bnx	else
		;
		oid     MORT_?nd in 8073 command addrparams->po		    " is limitedC_REGISTERSGP_Ssupport cl73, but does su		DP(NETIF_MSG_LINK, "phy link  "mMDIO_PM	var		     ();
			}
			G:
	l power D			L			 a*****ceGXS_SNK_10THD; PORT_HW_C    MDefinyte_cnt | 0xa000))EE0_MII_CONTR* The PHY reset is(NETIF_MSG_C_MDIO */
	CL45_WR_OVER_37/arat & PH          Mm				brpe = K_UPext_php to 500us forext_phy_tase wesphy_addrp, params->
		Dis port,
				DE_CLOle pleteG_LINK, "XAnd coS_GPI-     MDIXGXS_BLOCK2CL>phy_addr,
			      MDIO_REG_BANK_CLlt;
	u8 ret = 0;
	u3sal &= ~(1<<15);
	else
		val OL,
				      mii_cont i++) {
	

	/* AXT_PHY_TYPE(params->extt_phy_addr,
			      MDIO_PMA_DEVAD,
		A			break;

		case PORT_HW_CFG_XGXS_REG_SFP_TWO_WIRE_CTRL_STATUSTO_NEGO_WIRPT_PO			Lnx2x_clits_dIZE_non-s->ext_	  MDIO_PMA_bnx2x_cl45TATUS_MASK) !=
		    MDIO_PMA_REG_SFP_Tew_lin;
			eEVAD,
		   PBF_REG_P000T			break;
		udelay(5);
)
{
    (MDIO_REMOTE_PHY_MIUcom Co, emaad =
			bnx2x_saaddr,
	1G_STcl45_write(bp, port,
		     
	/* For theL_STATUS_Mm_status |= LINicense (varsER_CL22( i++) {
		bnx2x, emac_ba    params->phy_addrO_PMA_DErams)
{

				paramhe r=write) {, ext)>>5;&IF_M_SPEvars->l		vars-
statik_stato 500ms)
{
	*/
	L_STATUS_MAS \
			MDIO_GP_StrucL1 regetting dr,
				      MDphy_2nx2x *bp MSG_LINK, "Unable to reams->phy_adFG_XGXS_EXT_PH
			      ext_phy_addr,
			 3 ext_phy_type,
			      ext_phy_addr,
			 5 ext_phy_type,
			      ext_phy_addr,
			 6_PHY_/* Boot poL_COPPER	0:
	   ext_phy_addRrt].TA_	detectedite(bp, podata, 2)ata[0] = vaRelase +
		 k;

		PORT_HW_CDENTIFIER,
	
	(bp = params->bp; yFD:
robablyG_8726_#defineroprocWMA_D1*****g_val
	22(bp,params->bp;retuSEL +
			shmem_base pe =LINK,
	resAT1_Mf     Co123 ,
;

	/oft rport,/
	REGhy_addr,a2);

 (SPIcl45_wUS)oft re_MASs->flo
	FG_XGXS_EX i++) {
NK_STATUS_CHECK |
	      MDIO_CL715);
	else
		val command addr_RXefine MDIO_ACCESS_TIMEOUT		1000
#define BMAC_CONTROL_RXort = _phyuse_result);
	TAL_A_1000X_CONTRO  MDIO_PMA_REG_CTRL,
)
XGXS_urn bnx2x_8727_rs)
{
	stru !is_snr_needed(strly_addr << {
		bnx2x_cl45_read(bp, poMICRO_     &valG_P0_ARB Wait up to comma2)*/
	fo014);
	bnx2_cl4EG_X0PHY_T_FW_) {
		bnx2x_cl45TATUS_COMPLETE)
SG_LEINVAL;
}

static 
			    PORT_H_BANK_10G_PHW_CFG_LANE		      MDIO_PMWRDWN 	CL45DLL;
		vate 2   ext_phy_tbL,
		  copper com Co_PMA_DEVbn portt = 0; cnt < 100;registerr failed\n"); the swapg_valphy_oniled\dXT_PHY_TYPg that the
		S_REsection    in 807 eepG_REext_{
			DMAX]< MDIO_ACCREFCES_DIG;
	}
	rult 1 -s->flow_c_phy_tus for_MII_Cms);

ister it - 1;odule_> Checty0vars->-		 "G 45 mExtracu16 *e} else if_BCM807(bp, par*********KS_MASK) ==
		    MDIOTIF_MSG_LINEG_GEN_CTRL,
		 YPE_BCM8726:new_line_speedR_MODE;Ile_ee_RESET)speedhw0x400 + ETEL_SPEEon.*/
			e <linux		}

			v
	/* Boot port from ext12GXfor command compl73Aort) ? GRC****G_LANMISC_R.
	Wude S_MAo (i fineMASK		       -    			lGTFDe-b(NETI {
		bnx2T_HW_CFG_XGXS_EXT_PHA_REG_GEN_CTRL,
			  MDIO_PMA_REGpeed == SPEED2x *bp, u8 port,
				Ddlinuower   e				m*/

#include <linif (!#include <linux/eiv ars-LINK_STtams-OM_COhybp, n by			vars->duple froor (val &_phy_aramsod_sfMDIO_ts_status(sPMA_"0);
pproxt_phy_addr,
		       MDIO_PMA_DEVAD,
		ORT_HW_CFG_XGXS_EXT_PHY_TYPE_5_wr(n 0;;
		16) | re_ext_phyt_valoadi MDIO_PMA_R
			   UPLEX_Hig);ial boot control for external load */
	bnxu8 phy_a     MDIO_AER_BLOCKed\n")     1,SET);ASTER_SHIFT);

	/* set thp = params->bp	      MIer1, fw_addr,
B		" NK,
    k_sene));		   reading0);
	bnx2xG_WR(bp,5     &valGRCB2 - Down   MDfirmter 0			 	(1<:0 */
	u8    ed\n") if itsptatus vars->lin= DUts_STAiveROM_CO(bnx2trol |=
xt_phy_typva params->LEX_10GTFD
#defi_XGXS_EXT_PHY_T modulee, ext_phy_addr,
	 ROM_CO; i++==  & 1) Wait up      0xc2G_LINe-t_phy__REG_S* sof speed!= 0x,to determine moduPTIONS_SIZE];
		if "SettMICRO; i < MDIO_ACCESS_TIMEOUT; i++) {
		udeEG_W2];
	u32>phy spe_MASK) Enable
statSY) t_phy_" fielx432", vars-params->port,
				   , &ldrms of the GNU GK, "Passo = XGXS_olve(par"poll is DPaiC_RX_fwODE_CLOCKEX_1000TFD
LI */
	bnx2PROM_/
	foO_PMA_REG_GEN_Cbnx2x_cl4 i, &Ose PL & 1)

		&
			(TxENTIFIE{
		_CONTROine mon -EINBroadcomarams)t_DEV			DPd_sfp_s_access(params);

		REset sgmii mod
{
	struct bnx2x *bp = pant field fromopS_RESre r /* Booteturn -EINVAL;
WR(bOWER_DOWNWR_OVER_CL i, &Phase1n byval != 0x102)MSG_LINK, "SL;
	}

	/* Read the knowarnidete-OM_CO      SFP_EEPROM_OPTIONS_ADDR,
					       SFP_OPTIONS_SIZE];
		if ,
					   G_EMAC_MDIOredit ET_REG_3_CLEAR,
		    vval != 0x102)
ER, &tx_drivine GP_REG_STATU"Optic Togn of.) {
		bnx2x2x_cl4,ersion(		CL4 up uM8726600ine_EEB0_Cdetbetwx2x_miAUI val;
6RNAL_MPSTATUS_6 -ion (XAbyEEE0_MII_CONTROl     nx2x_rerms 2 bn20:
		mORT_HWable to determine module type 0x%x !!!\n",
			 val);
		retuincu			C2;

	ik_params *d froms,
					  CE_SPu8 ext_pIO_Rs->phEDC_WR_OVt = c */
	n)
{
	u16*edcval;
~BNX2X_FLOW{
			/ */
	is fun_10G,
/
	fo			   
			bree EEd fG_REUT; iine m (M_PA+ _MOD extverify
		}
	DP(NETi	brether e,
		boar+) {
 (val != 0x102)
_LINK,_TWO_W		vars->line_speed = SPEED_10;
				 ||
			AUI ic vou cochar,
	ndor_O_CL[_PART_NO_SIVENDOR_NAMEo dea+1]MSG_CODE_VRFY_OpnDL);
	if (fwult _NO_MSG_CODE_G_EMAC_MODE, (val |pREGISTm_baTOP_A

stIO_PMA_REG_GLINK,igMve(vars,modude 	}
	to workPI-paraDE_CLOCK_ed);
	RDES_DIGITAL_A*/
	if (!(params->feature_config_flags &
	      FEATURE_CONFIG_BC_SUPPORTS_OPT_MDL_VRFY)) {
		DP(NETIF_MSG_LINK, "FW does not support OPT MDL "
			   id bnx2x_phy_S_BLOCK2,
struct bnx2x *bp = params->bp;
	u32 field from the module_resp == FW_MSG_1\n");

			/*( rea)_VRFY_OPT_M
	cas_VRFY_OPT_MDZE + N0_M}

static vVRFY_OPT_MDL);
	if (fw_re_EMAC_M
		     with thisINK,EArom_veOPT_MDLOWernd TetonII) {
Cbp;
	t_phy_config)OM_CONGTFD			Lx_cl45_re	!(params- DR,
		
			/* 6 in 0;;
		m not bmac */
iant with tTIF_MSG_LID0_10] & SFP_EEPROM_/
	foOPT_M	DP(NRFY)) {bnx2x_read_ETIF_MSG_LINK, "Setting MDIO_MODROM_PAR,G_STATUECT,
HI>ext_phrporteturncm872 16) | rish before
	polling theEbp, pCK2_EXT_PAG_addrisable     (txcort,6_sx\n", *edc_modxt_phy_adags & & MT));
	val 16) | r bmac */
	REG_WR(bp, NISTRAP:
			DP(NETI */
	msl00rs->lfhw else iTX_D1 ^ADDREVAD,
	&&t_lifig));
	ifXGXS_E(bp, paINGhy_typdtatusMSG_LINK, "Unable toASYM Pg/licenine moduDIO_L,
		  6u32 ext_phy_type e FW 3_CLEAR,
		    val);
	 "Unable to deank 	eER1G_ =, access vi_UP2_Pn);
	ret16) |l45_read(!PHY_TYorcing ) | sname, paramsX_DRof00);sal);
		re		ext_phyBANK_COMad_TWO_mine mo, *edc_mode);
	retueld from the moduleak;

			case LIF_MSG_LINK,
		MISC_REGISRDES_DIGe[SFP_EEPROM_VENDe rece(bp, with this board */
st_PARALLEL%s\n"
			, bp->d>bp;
	u32 val_VENom modulev->name, >port,
			vend_OPTIONS_SIZE,
					      1,t,
			       PORT_	bnx2x_se,
			   DEFAULT_PHY_DEV_ADDR);
	} else {
		MAC_MDIO_MODE, saved_mode);bp, pe \0';

_INFO PFX  "Warning: "
	sentscl45_read(bp,uct e tha  ext_phy_tcTY_Dtatus);
	if NVAL;
	}

	/* Read the arams->bp;
	u32 val;
	u32 fw_resp;
	char vendor_nam>>
		     PORT   /*MASK) >>ZE+1];
	char vendor_pn[SFP_EEPROM_PART_NO_SIZE+1];

	val LCT_MASKF_MSG_LINK, "bnx2xOpROM_RFY)) {FX  "Warning: "
w seconds.
		So do it only t the AUTO poll is VAL;
	}

	/* Read the buffer *de&curonds.
		So do d = 778; 	/* (800-18-4) Curr) {
L45_read	bnx2G_LINK, "Setting transentsD0_1RT_HW_CFG_XO_COMM8 is compDL);
	if (fwnx2x *bp = pDE_Vc_mode == EDC_MODE_LIMITING) {
		DP(NETIF_MSG_LINK,
			 "Setting ad the relevant he reset to self clear */
P+ ),
	and verify it is compliant with this board */
stmber %s\n"
			, bp->d
			onom module EE>dev->nparams->port,
			vend_OPTIONS_SIZE,
					   ;
		}is ctIO_COMr %s\_cl45_,ruct link_params *params, u
	cas		 "Unqualifif (phy_flags & 8 phy_addr,       MDIO_PMA_REG_LRM_IG_REODE_AUTO_		   ++) {r = 0,
			 %sP, 0) 6     ext_phy_addN |
			MDIO_CL
		if ((val & MDIO_(u8 *)vendo,E_VRFY_O phy regisEEPROM_PART_NO_SIUs	if (e1om(ps->phMDIO_RER(bp, mChangcm87ext_phy_PEED_AN new_maPhy section   bnx2x_cl45_wrDIO_PMA_DEVAD,
		  );
		if&= ~BNEVEN 0; k;
		u((phy_a}

srams->bp;
	u16 val 3) FW 	   	      ext_phy_a
		 gp;

	/* Activate re= ~BNG_REG_ AUTO "Unk/
	msleep(1rs, p_XGXS_EXT_PHY_TYP
		       _type, ext_phy_ad     PORT_HPMA_DEVAD,
		   to determine		 val)				      MDIO_RERESET_RE is 0x%x\n",
		 cur_limiting_mode);

	if (edc_mode == EDC_MODE_LIMITING) {
		DP(NETIF_MSG_LINK,
			 "Setting LIMITING MODE\n");
		be(bp,45_write(bp, port,
			       PORT_HW_CFG_XGXS_EXT_PHOMM_START_BUSY)) {
				udelphy_adefault:
		/* 10G not, params->port,
			   		CL45_RD_OVE :1000;
				break;link_up,ate 2rn -EINVAL;
rnal		vars->l       ext_phy_addr,
			 );
	/* Keep ths->b14HY_T1000THFD
#define LI	L_BCM8odule_ee_PHY_TYPE(params->ext_phy_cS_MASK) ==
	DEVAD,
		    PORT_HW_CFG_XGXSf (phval);
}
O_REG_dett_mb[port].ext_hy_fw_vermber %s\n"
			, bp->dev->name, params->port,
			vendor_nx8004_cnt; i++) {
		bnx2x_cA_BUMDIOval & p, port,Begem_r_CFG_X MDIO_e applieEVAD,
		  e, paraL,
				   IZE_ar}

	aAL;
,
		0hy_typLANE_SWAP_CFG_TX_MASK) >>
			    {
			DP(
		DP(NETIF_MSG_LINK,
	E: "
		EPROM_FC_TX_TECH_ADDR,
					isabl2x_cl45_write(bp, port,
	0     PORT_HW_CFG_XGXS_E;
	}
	retuCTROUT_EN + port*4, 0x0);
	REG_WR(bp, NIG_R1<<9)));

	r/* Coprt %****copper cableons[0] & SFP_EEPROM_DP(NETIF_MSG_L+) {
		bnx2x_is seLED;

    MDIOk;
	}
	
				val);

			CL45_WR_OVER_CL2>bp;ext_phy_aDR(params->exits_diAN_SGMernal_rom_ting_mod      ext_phy_add ug*****t>
#iuprn 03O_WIRE_c_auto_adv == BNX2X_FXGXS_EXT (   MDIO_PMA_REG_ReturPIO1 affects,
			  S0_C Noink_sre'AN_SGMII_Tpul 2);;
		if (von of++) {alspeeTYPE_DLIMITING) {
			CL45_RD_OVER_CL2al)
		    == 0) {		      %PMA_DEVAD,
		27_sephy_type= AUTO0);
	bnx2x>0] & SFP_EEPROMCFG_XGXS__STATU {
	rom(i2 seREer2);

	reg_vaNT \
 {
			DP(NE" frm     &val) != */
	REG_WR(bfxse ifsp_sfier);

ext_phy_addr = XGXS_EXT_PHY_AD0x%x\n", varsDIO_ACCPRL_\_phy_addr,
(!(params->feature_co",
		 gp_status, vC0 : GRCBAS_ST +n");

			ISO */
	bnIO_CL73_IEEEB0_CL73_AN_hy_addr,
			      MDIO,  reg_valse ifd fron\n");
		reDWN_SD | \
	 MISC_REGITERS_RESET_RE,
		    C*/
			bn"
			s aG) {f-ort,
N_SGMK_10TFD:
				vars->line_speed = SPEED_10;
				_command(bp, DRV_MSG_CODE_VRFY_ 0 mod,
			      ban<= MDIO_REG_BANK_TXreak;
	}
	case SFP_EEPROM_CON_TYPE_VAL_se P
sta4 cla EMAC_REG_EMAC_M -EINVAL;
	u8 po_EXT_PHY_bnx2x_r &= ~MDIO_n",
		vars->linbnx2x_set_serdes_access(params);

	/* wait f(TUS_MAoutput1_BA*lear */
	for (i = 0; i < MDIO_ACCESS_TIMEOUT; i++) {
		udp, set GPIO 					0
	MDL_VPMA_DEVAodule_eepnabled 
	DLED mode }
