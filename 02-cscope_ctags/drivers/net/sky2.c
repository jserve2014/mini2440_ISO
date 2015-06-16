/*
 * New driver for Marvell Yukon 2 chipset.
 * Based on earlier sk98lin, and skge driver.
 *
 * This driver intentionally does not support all the features
 * of the original driver such as link fail-over and link management because
 * those should be done at higher levels.
 *
 * Copyright (C) 2005 Stephen Hemminger <shemminger@osdl.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/crc32.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/pci.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/if_vlan.h>
#include <linux/prefetch.h>
#include <linux/debugfs.h>
#include <linux/mii.h>

#include <asm/irq.h>

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define SKY2_VLAN_TAG_USED 1
#endif

#include "sky2.h"

#define DRV_NAME		"sky2"
#define DRV_VERSION		"1.25"
#define PFX			DRV_NAME " "

/*
 * The Yukon II chipset takes 64 bit command blocks (called list elements)
 * that are organized into three (receive, transmit, status) different rings
 * similar to Tigon3.
 */

#define RX_LE_SIZE	    	1024
#define RX_LE_BYTES		(RX_LE_SIZE*sizeof(struct sky2_rx_le))
#define RX_MAX_PENDING		(RX_LE_SIZE/6 - 2)
#define RX_DEF_PENDING		RX_MAX_PENDING

/* This is the worst case number of transmit list elements for a single skb:
   VLAN:GSO + CKSUM + Data + skb_frags * DMA */
#define MAX_SKB_TX_LE	(2 + (sizeof(dma_addr_t)/sizeof(u32))*(MAX_SKB_FRAGS+1))
#define TX_MIN_PENDING		(MAX_SKB_TX_LE+1)
#define TX_MAX_PENDING		4096
#define TX_DEF_PENDING		127

#define STATUS_RING_SIZE	2048	/* 2 ports * (TX + 2*RX) */
#define STATUS_LE_BYTES		(STATUS_RING_SIZE*sizeof(struct sky2_status_le))
#define TX_WATCHDOG		(5 * HZ)
#define NAPI_WEIGHT		64
#define PHY_RETRIES		1000

#define SKY2_EEPROM_MAGIC	0x9955aabb


#define RING_NEXT(x,s)	(((x)+1) & ((s)-1))

static const u32 default_msg =
    NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK
    | NETIF_MSG_TIMER | NETIF_MSG_TX_ERR | NETIF_MSG_RX_ERR
    | NETIF_MSG_IFUP | NETIF_MSG_IFDOWN;

static int debug = -1;		/* defaults above */
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0=none,...,16=all)");

static int copybreak __read_mostly = 128;
module_param(copybreak, int, 0);
MODULE_PARM_DESC(copybreak, "Receive copy threshold");

static int disable_msi = 0;
module_param(disable_msi, int, 0);
MODULE_PARM_DESC(disable_msi, "Disable Message Signaled Interrupt (MSI)");

static DEFINE_PCI_DEVICE_TABLE(sky2_id_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_SYSKONNECT, 0x9000) }, /* SK-9Sxx */
	{ PCI_DEVICE(PCI_VENDOR_ID_SYSKONNECT, 0x9E00) }, /* SK-9Exx */
	{ PCI_DEVICE(PCI_VENDOR_ID_DLINK, 0x4b00) },	/* DGE-560T */
	{ PCI_DEVICE(PCI_VENDOR_ID_DLINK, 0x4001) }, 	/* DGE-550SX */
	{ PCI_DEVICE(PCI_VENDOR_ID_DLINK, 0x4B02) },	/* DGE-560SX */
	{ PCI_DEVICE(PCI_VENDOR_ID_DLINK, 0x4B03) },	/* DGE-550T */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4340) }, /* 88E8021 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4341) }, /* 88E8022 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4342) }, /* 88E8061 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4343) }, /* 88E8062 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4344) }, /* 88E8021 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4345) }, /* 88E8022 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4346) }, /* 88E8061 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4347) }, /* 88E8062 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4350) }, /* 88E8035 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4351) }, /* 88E8036 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4352) }, /* 88E8038 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4353) }, /* 88E8039 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4354) }, /* 88E8040 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4355) }, /* 88E8040T */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4356) }, /* 88EC033 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4357) }, /* 88E8042 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x435A) }, /* 88E8048 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4360) }, /* 88E8052 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4361) }, /* 88E8050 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4362) }, /* 88E8053 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4363) }, /* 88E8055 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4364) }, /* 88E8056 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4365) }, /* 88E8070 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4366) }, /* 88EC036 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4367) }, /* 88EC032 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4368) }, /* 88EC034 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4369) }, /* 88EC042 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x436A) }, /* 88E8058 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x436B) }, /* 88E8071 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x436C) }, /* 88E8072 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x436D) }, /* 88E8055 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4370) }, /* 88E8075 */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL, 0x4380) }, /* 88E8057 */
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, sky2_id_table);

/* Avoid conditionals by using array */
static const unsigned txqaddr[] = { Q_XA1, Q_XA2 };
static const unsigned rxqaddr[] = { Q_R1, Q_R2 };
static const u32 portirq_msk[] = { Y2_IS_PORT_1, Y2_IS_PORT_2 };

static void sky2_set_multicast(struct net_device *dev);

/* Access to PHY via serial interconnect */
static int gm_phy_write(struct sky2_hw *hw, unsigned port, u16 reg, u16 val)
{
	int i;

	gma_write16(hw, port, GM_SMI_DATA, val);
	gma_write16(hw, port, GM_SMI_CTRL,
		    GM_SMI_CT_PHY_AD(PHY_ADDR_MARV) | GM_SMI_CT_REG_AD(reg));

	for (i = 0; i < PHY_RETRIES; i++) {
		u16 ctrl = gma_read16(hw, port, GM_SMI_CTRL);
		if (ctrl == 0xffff)
			goto io_error;

		if (!(ctrl & GM_SMI_CT_BUSY))
			return 0;

		udelay(10);
	}

	dev_warn(&hw->pdev->dev,"%s: phy write timeout\n", hw->dev[port]->name);
	return -ETIMEDOUT;

io_error:
	dev_err(&hw->pdev->dev, "%s: phy I/O error\n", hw->dev[port]->name);
	return -EIO;
}

static int __gm_phy_read(struct sky2_hw *hw, unsigned port, u16 reg, u16 *val)
{
	int i;

	gma_write16(hw, port, GM_SMI_CTRL, GM_SMI_CT_PHY_AD(PHY_ADDR_MARV)
		    | GM_SMI_CT_REG_AD(reg) | GM_SMI_CT_OP_RD);

	for (i = 0; i < PHY_RETRIES; i++) {
		u16 ctrl = gma_read16(hw, port, GM_SMI_CTRL);
		if (ctrl == 0xffff)
			goto io_error;

		if (ctrl & GM_SMI_CT_RD_VAL) {
			*val = gma_read16(hw, port, GM_SMI_DATA);
			return 0;
		}

		udelay(10);
	}

	dev_warn(&hw->pdev->dev, "%s: phy read timeout\n", hw->dev[port]->name);
	return -ETIMEDOUT;
io_error:
	dev_err(&hw->pdev->dev, "%s: phy I/O error\n", hw->dev[port]->name);
	return -EIO;
}

static inline u16 gm_phy_read(struct sky2_hw *hw, unsigned port, u16 reg)
{
	u16 v;
	__gm_phy_read(hw, port, reg, &v);
	return v;
}


static void sky2_power_on(struct sky2_hw *hw)
{
	/* switch power to VCC (WA for VAUX problem) */
	sky2_write8(hw, B0_POWER_CTRL,
		    PC_VAUX_ENA | PC_VCC_ENA | PC_VAUX_OFF | PC_VCC_ON);

	/* disable Core Clock Division, */
	sky2_write32(hw, B2_Y2_CLK_CTRL, Y2_CLK_DIV_DIS);

	if (hw->chip_id == CHIP_ID_YUKON_XL && hw->chip_rev > 1)
		/* enable bits are inverted */
		sky2_write8(hw, B2_Y2_CLK_GATE,
			    Y2_PCI_CLK_LNK1_DIS | Y2_COR_CLK_LNK1_DIS |
			    Y2_CLK_GAT_LNK1_DIS | Y2_PCI_CLK_LNK2_DIS |
			    Y2_COR_CLK_LNK2_DIS | Y2_CLK_GAT_LNK2_DIS);
	else
		sky2_write8(hw, B2_Y2_CLK_GATE, 0);

	if (hw->flags & SKY2_HW_ADV_POWER_CTL) {
		u32 reg;

		sky2_pci_write32(hw, PCI_DEV_REG3, 0);

		reg = sky2_pci_read32(hw, PCI_DEV_REG4);
		/* set all bits to 0 except bits 15..12 and 8 */
		reg &= P_ASPM_CONTROL_MSK;
		sky2_pci_write32(hw, PCI_DEV_REG4, reg);

		reg = sky2_pci_read32(hw, PCI_DEV_REG5);
		/* set all bits to 0 except bits 28 & 27 */
		reg &= P_CTL_TIM_VMAIN_AV_MSK;
		sky2_pci_write32(hw, PCI_DEV_REG5, reg);

		sky2_pci_write32(hw, PCI_CFG_REG_1, 0);

		/* Enable workaround for dev 4.107 on Yukon-Ultra & Extreme */
		reg = sky2_read32(hw, B2_GP_IO);
		reg |= GLB_GPIO_STAT_RACE_DIS;
		sky2_write32(hw, B2_GP_IO, reg);

		sky2_read32(hw, B2_GP_IO);
	}

	/* Turn on "driver loaded" LED */
	sky2_write16(hw, B0_CTST, Y2_LED_STAT_ON);
}

static void sky2_power_aux(struct sky2_hw *hw)
{
	if (hw->chip_id == CHIP_ID_YUKON_XL && hw->chip_rev > 1)
		sky2_write8(hw, B2_Y2_CLK_GATE, 0);
	else
		/* enable bits are inverted */
		sky2_write8(hw, B2_Y2_CLK_GATE,
			    Y2_PCI_CLK_LNK1_DIS | Y2_COR_CLK_LNK1_DIS |
			    Y2_CLK_GAT_LNK1_DIS | Y2_PCI_CLK_LNK2_DIS |
			    Y2_COR_CLK_LNK2_DIS | Y2_CLK_GAT_LNK2_DIS);

	/* switch power to VAUX if supported and PME from D3cold */
	if ( (sky2_read32(hw, B0_CTST) & Y2_VAUX_AVAIL) &&
	     pci_pme_capable(hw->pdev, PCI_D3cold))
		sky2_write8(hw, B0_POWER_CTRL,
			    (PC_VAUX_ENA | PC_VCC_ENA |
			     PC_VAUX_ON | PC_VCC_OFF));

	/* turn off "driver loaded LED" */
	sky2_write16(hw, B0_CTST, Y2_LED_STAT_OFF);
}

static void sky2_gmac_reset(struct sky2_hw *hw, unsigned port)
{
	u16 reg;

	/* disable all GMAC IRQ's */
	sky2_write8(hw, SK_REG(port, GMAC_IRQ_MSK), 0);

	gma_write16(hw, port, GM_MC_ADDR_H1, 0);	/* clear MC hash */
	gma_write16(hw, port, GM_MC_ADDR_H2, 0);
	gma_write16(hw, port, GM_MC_ADDR_H3, 0);
	gma_write16(hw, port, GM_MC_ADDR_H4, 0);

	reg = gma_read16(hw, port, GM_RX_CTRL);
	reg |= GM_RXCR_UCF_ENA | GM_RXCR_MCF_ENA;
	gma_write16(hw, port, GM_RX_CTRL, reg);
}

/* flow control to advertise bits */
static const u16 copper_fc_adv[] = {
	[FC_NONE]	= 0,
	[FC_TX]		= PHY_M_AN_ASP,
	[FC_RX]		= PHY_M_AN_PC,
	[FC_BOTH]	= PHY_M_AN_PC | PHY_M_AN_ASP,
};

/* flow control to advertise bits when using 1000BaseX */
static const u16 fiber_fc_adv[] = {
	[FC_NONE] = PHY_M_P_NO_PAUSE_X,
	[FC_TX]   = PHY_M_P_ASYM_MD_X,
	[FC_RX]	  = PHY_M_P_SYM_MD_X,
	[FC_BOTH] = PHY_M_P_BOTH_MD_X,
};

/* flow control to GMA disable bits */
static const u16 gm_fc_disable[] = {
	[FC_NONE] = GM_GPCR_FC_RX_DIS | GM_GPCR_FC_TX_DIS,
	[FC_TX]	  = GM_GPCR_FC_RX_DIS,
	[FC_RX]	  = GM_GPCR_FC_TX_DIS,
	[FC_BOTH] = 0,
};


static void sky2_phy_init(struct sky2_hw *hw, unsigned port)
{
	struct sky2_port *sky2 = netdev_priv(hw->dev[port]);
	u16 ctrl, ct1000, adv, pg, ledctrl, ledover, reg;

	if ( (sky2->flags & SKY2_FLAG_AUTO_SPEED) &&
	    !(hw->flags & SKY2_HW_NEWER_PHY)) {
		u16 ectrl = gm_phy_read(hw, port, PHY_MARV_EXT_CTRL);

		ectrl &= ~(PHY_M_EC_M_DSC_MSK | PHY_M_EC_S_DSC_MSK |
			   PHY_M_EC_MAC_S_MSK);
		ectrl |= PHY_M_EC_MAC_S(MAC_TX_CLK_25_MHZ);

		/* on PHY 88E1040 Rev.D0 (and newer) downshift control changed */
		if (hw->chip_id == CHIP_ID_YUKON_EC)
			/* set downshift counter to 3x and enable downshift */
			ectrl |= PHY_M_EC_DSC_2(2) | PHY_M_EC_DOWN_S_ENA;
		else
			/* set master & slave downshift counter to 1x */
			ectrl |= PHY_M_EC_M_DSC(0) | PHY_M_EC_S_DSC(1);

		gm_phy_write(hw, port, PHY_MARV_EXT_CTRL, ectrl);
	}

	ctrl = gm_phy_read(hw, port, PHY_MARV_PHY_CTRL);
	if (sky2_is_copper(hw)) {
		if (!(hw->flags & SKY2_HW_GIGABIT)) {
			/* enable automatic crossover */
			ctrl |= PHY_M_PC_MDI_XMODE(PHY_M_PC_ENA_AUTO) >> 1;

			if (hw->chip_id == CHIP_ID_YUKON_FE_P &&
			    hw->chip_rev == CHIP_REV_YU_FE2_A0) {
				u16 spec;

				/* Enable Class A driver for FE+ A0 */
				spec = gm_phy_read(hw, port, PHY_MARV_FE_SPEC_2);
				spec |= PHY_M_FESC_SEL_CL_A;
				gm_phy_write(hw, port, PHY_MARV_FE_SPEC_2, spec);
			}
		} else {
			/* disable energy detect */
			ctrl &= ~PHY_M_PC_EN_DET_MSK;

			/* enable automatic crossover */
			ctrl |= PHY_M_PC_MDI_XMODE(PHY_M_PC_ENA_AUTO);

			/* downshift on PHY 88E1112 and 88E1149 is changed */
			if ( (sky2->flags & SKY2_FLAG_AUTO_SPEED)
			    && (hw->flags & SKY2_HW_NEWER_PHY)) {
				/* set downshift counter to 3x and enable downshift */
				ctrl &= ~PHY_M_PC_DSC_MSK;
				ctrl |= PHY_M_PC_DSC(2) | PHY_M_PC_DOWN_S_ENA;
			}
		}
	} else {
		/* workaround for deviation #4.88 (CRC errors) */
		/* disable Automatic Crossover */

		ctrl &= ~PHY_M_PC_MDIX_MSK;
	}

	gm_phy_write(hw, port, PHY_MARV_PHY_CTRL, ctrl);

	/* special setup for PHY 88E1112 Fiber */
	if (hw->chip_id == CHIP_ID_YUKON_XL && (hw->flags & SKY2_HW_FIBRE_PHY)) {
		pg = gm_phy_read(hw, port, PHY_MARV_EXT_ADR);

		/* Fiber: select 1000BASE-X only mode MAC Specific Ctrl Reg. */
		gm_phy_write(hw, port, PHY_MARV_EXT_ADR, 2);
		ctrl = gm_phy_read(hw, port, PHY_MARV_PHY_CTRL);
		ctrl &= ~PHY_M_MAC_MD_MSK;
		ctrl |= PHY_M_MAC_MODE_SEL(PHY_M_MAC_MD_1000BX);
		gm_phy_write(hw, port, PHY_MARV_PHY_CTRL, ctrl);

		if (hw->pmd_type  == 'P') {
			/* select page 1 to access Fiber registers */
			gm_phy_write(hw, port, PHY_MARV_EXT_ADR, 1);

			/* for SFP-module set SIGDET polarity to low */
			ctrl = gm_phy_read(hw, port, PHY_MARV_PHY_CTRL);
			ctrl |= PHY_M_FIB_SIGD_POL;
			gm_phy_write(hw, port, PHY_MARV_PHY_CTRL, ctrl);
		}

		gm_phy_write(hw, port, PHY_MARV_EXT_ADR, pg);
	}

	ctrl = PHY_CT_RESET;
	ct1000 = 0;
	adv = PHY_AN_CSMA;
	reg = 0;

	if (sky2->flags & SKY2_FLAG_AUTO_SPEED) {
		if (sky2_is_copper(hw)) {
			if (sky2->advertising & ADVERTISED_1000baseT_Full)
				ct1000 |= PHY_M_1000C_AFD;
			if (sky2->advertising & ADVERTISED_1000baseT_Half)
				ct1000 |= PHY_M_1000C_AHD;
			if (sky2->advertising & ADVERTISED_100baseT_Full)
				adv |= PHY_M_AN_100_FD;
			if (sky2->advertising & ADVERTISED_100baseT_Half)
				adv |= PHY_M_AN_100_HD;
			if (sky2->advertising & ADVERTISED_10baseT_Full)
				adv |= PHY_M_AN_10_FD;
			if (sky2->advertising & ADVERTISED_10baseT_Half)
				adv |= PHY_M_AN_10_HD;

		} else {	/* special defines for FIBER (88E1040S only) */
			if (sky2->advertising & ADVERTISED_1000baseT_Full)
				adv |= PHY_M_AN_1000X_AFD;
			if (sky2->advertising & ADVERTISED_1000baseT_Half)
				adv |= PHY_M_AN_1000X_AHD;
		}

		/* Restart Auto-negotiation */
		ctrl |= PHY_CT_ANE | PHY_CT_RE_CFG;
	} else {
		/* forced speed/duplex settings */
		ct1000 = PHY_M_1000C_MSE;

		/* Disable auto update for duplex flow control and duplex */
		reg |= GM_GPCR_AU_DUP_DIS | GM_GPCR_AU_SPD_DIS;

		switch (sky2->speed) {
		case SPEED_1000:
			ctrl |= PHY_CT_SP1000;
			reg |= GM_GPCR_SPEED_1000;
			break;
		case SPEED_100:
			ctrl |= PHY_CT_SP100;
			reg |= GM_GPCR_SPEED_100;
			break;
		}

		if (sky2->duplex == DUPLEX_FULL) {
			reg |= GM_GPCR_DUP_FULL;
			ctrl |= PHY_CT_DUP_MD;
		} else if (sky2->speed < SPEED_1000)
			sky2->flow_mode = FC_NONE;
	}

	if (sky2->flags & SKY2_FLAG_AUTO_PAUSE) {
		if (sky2_is_copper(hw))
			adv |= copper_fc_adv[sky2->flow_mode];
		else
			adv |= fiber_fc_adv[sky2->flow_mode];
	} else {
		reg |= GM_GPCR_AU_FCT_DIS;
 		reg |= gm_fc_disable[sky2->flow_mode];

		/* Forward pause packets to GMAC? */
		if (sky2->flow_mode & FC_RX)
			sky2_write8(hw, SK_REG(port, GMAC_CTRL), GMC_PAUSE_ON);
		else
			sky2_write8(hw, SK_REG(port, GMAC_CTRL), GMC_PAUSE_OFF);
	}

	gma_write16(hw, port, GM_GP_CTRL, reg);

	if (hw->flags & SKY2_HW_GIGABIT)
		gm_phy_write(hw, port, PHY_MARV_1000T_CTRL, ct1000);

	gm_phy_write(hw, port, PHY_MARV_AUNE_ADV, adv);
	gm_phy_write(hw, port, PHY_MARV_CTRL, ctrl);

	/* Setup Phy LED's */
	ledctrl = PHY_M_LED_PULS_DUR(PULS_170MS);
	ledover = 0;

	switch (hw->chip_id) {
	case CHIP_ID_YUKON_FE:
		/* on 88E3082 these bits are at 11..9 (shifted left) */
		ledctrl |= PHY_M_LED_BLINK_RT(BLINK_84MS) << 1;

		ctrl = gm_phy_read(hw, port, PHY_MARV_FE_LED_PAR);

		/* delete ACT LED control bits */
		ctrl &= ~PHY_M_FELP_LED1_MSK;
		/* change ACT LED control to blink mode */
		ctrl |= PHY_M_FELP_LED1_CTRL(LED_PAR_CTRL_ACT_BL);
		gm_phy_write(hw, port, PHY_MARV_FE_LED_PAR, ctrl);
		break;

	case CHIP_ID_YUKON_FE_P:
		/* Enable Link Partner Next Page */
		ctrl = gm_phy_read(hw, port, PHY_MARV_PHY_CTRL);
		ctrl |= PHY_M_PC_ENA_LIP_NP;

		/* disable Energy Detect and enable scrambler */
		ctrl &= ~(PHY_M_PC_ENA_ENE_DT | PHY_M_PC_DIS_SCRAMB);
		gm_phy_write(hw, port, PHY_MARV_PHY_CTRL, ctrl);

		/* set LED2 -> ACT, LED1 -> LINK, LED0 -> SPEED */
		ctrl = PHY_M_FELP_LED2_CTRL(LED_PAR_CTRL_ACT_BL) |
			PHY_M_FELP_LED1_CTRL(LED_PAR_CTRL_LINK) |
			PHY_M_FELP_LED0_CTRL(LED_PAR_CTRL_SPEED);

		gm_phy_write(hw, port, PHY_MARV_FE_LED_PAR, ctrl);
		break;

	case CHIP_ID_YUKON_XL:
		pg = gm_phy_read(hw, port, PHY_MARV_EXT_ADR);

		/* select page 3 to access LED control register */
		gm_phy_write(hw, port, PHY_MARV_EXT_ADR, 3);

		/* set LED Function Control register */
		gm_phy_write(hw, port, PHY_MARV_PHY_CTRL,
			     (PHY_M_LEDC_LOS_CTRL(1) |	/* LINK/ACT */
			      PHY_M_LEDC_INIT_CTRL(7) |	/* 10 Mbps */
			      PHY_M_LEDC_STA1_CTRL(7) |	/* 100 Mbps */
			      PHY_M_LEDC_STA0_CTRL(7)));	/* 1000 Mbps */

		/* set Polarity Control register */
		gm_phy_write(hw, port, PHY_MARV_PHY_STAT,
			     (PHY_M_POLC_LS1_P_MIX(4) |
			      PHY_M_POLC_IS0_P_MIX(4) |
			      PHY_M_POLC_LOS_CTRL(2) |
			      PHY_M_POLC_INIT_CTRL(2) |
			      PHY_M_POLC_STA1_CTRL(2) |
			      PHY_M_POLC_STA0_CTRL(2)));

		/* restore page register */
		gm_phy_write(hw, port, PHY_MARV_EXT_ADR, pg);
		break;

	case CHIP_ID_YUKON_EC_U:
	case CHIP_ID_YUKON_EX:
	case CHIP_ID_YUKON_SUPR:
		pg = gm_phy_read(hw, port, PHY_MARV_EXT_ADR);

		/* select page 3 to access LED control register */
		gm_phy_write(hw, port, PHY_MARV_EXT_ADR, 3);

		/* set LED Function Control register */
		gm_phy_write(hw, port, PHY_MARV_PHY_CTRL,
			     (PHY_M_LEDC_LOS_CTRL(1) |	/* LINK/ACT */
			      PHY_M_LEDC_INIT_CTRL(8) |	/* 10 Mbps */
			      PHY_M_LEDC_STA1_CTRL(7) |	/* 100 Mbps */
			      PHY_M_LEDC_STA0_CTRL(7)));/* 1000 Mbps */

		/* set Blink Rate in LED Timer Control Register */
		gm_phy_write(hw, port, PHY_MARV_INT_MASK,
			     ledctrl | PHY_M_LED_BLINK_RT(BLINK_84MS));
		/* restore page register */
		gm_phy_write(hw, port, PHY_MARV_EXT_ADR, pg);
		break;

	default:
		/* set Tx LED (LED_TX) to blink mode on Rx OR Tx activity */
		ledctrl |= PHY_M_LED_BLINK_RT(BLINK_84MS) | PHY_M_LEDC_TX_CTRL;

		/* turn off the Rx LED (LED_RX) */
		ledover |= PHY_M_LED_MO_RX(MO_LED_OFF);
	}

	if (hw->chip_id == CHIP_ID_YUKON_EC_U || hw->chip_id == CHIP_ID_YUKON_UL_2) {
		/* apply fixes in PHY AFE */
		gm_phy_write(hw, port, PHY_MARV_EXT_ADR, 255);

		/* increase differential signal amplitude in 10BASE-T */
		gm_phy_write(hw, port, 0x18, 0xaa99);
		gm_phy_write(hw, port, 0x17, 0x2011);

		if (hw->chip_id == CHIP_ID_YUKON_EC_U) {
			/* fix for IEEE A/B Symmetry failure in 1000BASE-T */
			gm_phy_write(hw, port, 0x18, 0xa204);
			gm_phy_write(hw, port, 0x17, 0x2002);
		}

		/* set page register to 0 */
		gm_phy_write(hw, port, PHY_MARV_EXT_ADR, 0);
	} else if (hw->chip_id == CHIP_ID_YUKON_FE_P &&
		   hw->chip_rev == CHIP_REV_YU_FE2_A0) {
		/* apply workaround for integrated resistors calibration */
		gm_phy_write(hw, port, PHY_MARV_PAGE_ADDR, 17);
		gm_phy_write(hw, port, PHY_MARV_PAGE_DATA, 0x3f60);
	} else if (hw->chip_id != CHIP_ID_YUKON_EX &&
		   hw->chip_id < CHIP_ID_YUKON_SUPR) {
		/* no effect on Yukon-XL */
		gm_phy_write(hw, port, PHY_MARV_LED_CTRL, ledctrl);

		if ( !(sky2->flags & SKY2_FLAG_AUTO_SPEED)
		     || sky2->speed == SPEED_100) {
			/* turn on 100 Mbps LED (LED_LINK100) */
			ledover |= PHY_M_LED_MO_100(MO_LED_ON);
		}

		if (ledover)
			gm_phy_write(hw, port, PHY_MARV_LED_OVER, ledover);

	}

	/* Enable phy interrupt on auto-negotiation complete (or link up) */
	if (sky2->flags & SKY2_FLAG_AUTO_SPEED)
		gm_phy_write(hw, port, PHY_MARV_INT_MASK, PHY_M_IS_AN_COMPL);
	else
		gm_phy_write(hw, port, PHY_MARV_INT_MASK, PHY_M_DEF_MSK);
}

static const u32 phy_power[] = { PCI_Y2_PHY1_POWD, PCI_Y2_PHY2_POWD };
static const u32 coma_mode[] = { PCI_Y2_PHY1_COMA, PCI_Y2_PHY2_COMA };

static void sky2_phy_power_up(struct sky2_hw *hw, unsigned port)
{
	u32 reg1;

	sky2_write8(hw, B2_TST_CTRL1, TST_CFG_WRITE_ON);
	reg1 = sky2_pci_read32(hw, PCI_DEV_REG1);
	reg1 &= ~phy_power[port];

	if (hw->chip_id == CHIP_ID_YUKON_XL && hw->chip_rev > 1)
		reg1 |= coma_mode[port];

	sky2_pci_write32(hw, PCI_DEV_REG1, reg1);
	sky2_write8(hw, B2_TST_CTRL1, TST_CFG_WRITE_OFF);
	sky2_pci_read32(hw, PCI_DEV_REG1);

	if (hw->chip_id == CHIP_ID_YUKON_FE)
		gm_phy_write(hw, port, PHY_MARV_CTRL, PHY_CT_ANE);
	else if (hw->flags & SKY2_HW_ADV_POWER_CTL)
		sky2_write8(hw, SK_REG(port, GPHY_CTRL), GPC_RST_CLR);
}

static void sky2_phy_power_down(struct sky2_hw *hw, unsigned port)
{
	u32 reg1;
	u16 ctrl;

	/* release GPHY Control reset */
	sky2_write8(hw, SK_REG(port, GPHY_CTRL), GPC_RST_CLR);

	/* release GMAC reset */
	sky2_write8(hw, SK_REG(port, GMAC_CTRL), GMC_RST_CLR);

	if (hw->flags & SKY2_HW_NEWER_PHY) {
		/* select page 2 to access MAC control register */
		gm_phy_write(hw, port, PHY_MARV_EXT_ADR, 2);

		ctrl = gm_phy_read(hw, port, PHY_MARV_PHY_CTRL);
		/* allow GMII Power Down */
		ctrl &= ~PHY_M_MAC_GMIF_PUP;
		gm_phy_write(hw, port, PHY_MARV_PHY_CTRL, ctrl);

		/* set page register back to 0 */
		gm_phy_write(hw, port, PHY_MARV_EXT_ADR, 0);
	}

	/* setup General Purpose Control Register */
	gma_write16(hw, port, GM_GP_CTRL,
		    GM_GPCR_FL_PASS | GM_GPCR_SPEED_100 |
		    GM_GPCR_AU_DUP_DIS | GM_GPCR_AU_FCT_DIS |
		    GM_GPCR_AU_SPD_DIS);

	if (hw->chip_id != CHIP_ID_YUKON_EC) {
		if (hw->chip_id == CHIP_ID_YUKON_EC_U) {
			/* select page 2 to access MAC control register */
			gm_phy_write(hw, port, PHY_MARV_EXT_ADR, 2);

			ctrl = gm_phy_read(hw, port, PHY_MARV_PHY_CTRL);
			/* enable Power Down */
			ctrl |= PHY_M_PC_POW_D_ENA;
			gm_phy_write(hw, port, PHY_MARV_PHY_CTRL, ctrl);

			/* set page register back to 0 */
			gm_phy_write(hw, port, PHY_MARV_EXT_ADR, 0);
		}

		/* set IEEE compatible Power Down Mode (dev. #4.99) */
		gm_phy_write(hw, port, PHY_MARV_CTRL, PHY_CT_PDOWN);
	}

	sky2_write8(hw, B2_TST_CTRL1, TST_CFG_WRITE_ON);
	reg1 = sky2_pci_read32(hw, PCI_DEV_REG1);
	reg1 |= phy_power[port];		/* set PHY to PowerDown/COMA Mode */
	sky2_pci_write32(hw, PCI_DEV_REG1, reg1);
	sky2_write8(hw, B2_TST_CTRL1, TST_CFG_WRITE_OFF);
}

/* Force a renegotiation */
static void sky2_phy_reinit(struct sky2_port *sky2)
{
	spin_lock_bh(&sky2->phy_lock);
	sky2_phy_init(sky2->hw, sky2->port);
	spin_unlock_bh(&sky2->phy_lock);
}

/* Put device in state to listen for Wake On Lan */
static void sky2_wol_init(struct sky2_port *sky2)
{
	struct sky2_hw *hw = sky2->hw;
	unsigned port = sky2->port;
	enum flow_control save_mode;
	u16 ctrl;
	u32 reg1;

	/* Bring hardware out of reset */
	sky2_write16(hw, B0_CTST, CS_RST_CLR);
	sky2_write16(hw, SK_REG(port, GMAC_LINK_CTRL), GMLC_RST_CLR);

	sky2_write8(hw, SK_REG(port, GPHY_CTRL), GPC_RST_CLR);
	sky2_write8(hw, SK_REG(port, GMAC_CTRL), GMC_RST_CLR);

	/* Force to 10/100
	 * sky2_reset will re-enable on resume
	 */
	save_mode = sky2->flow_mode;
	ctrl = sky2->advertising;

	sky2->advertising &= ~(ADVERTISED_1000baseT_Half|ADVERTISED_1000baseT_Full);
	sky2->flow_mode = FC_NONE;

	spin_lock_bh(&sky2->phy_lock);
	sky2_phy_power_up(hw, port);
	sky2_phy_init(hw, port);
	spin_unlock_bh(&sky2->phy_lock);

	sky2->flow_mode = save_mode;
	sky2->advertising = ctrl;

	/* Set GMAC to no flow control and auto update for speed/duplex */
	gma_write16(hw, port, GM_GP_CTRL,
		    GM_GPCR_FC_TX_DIS|GM_GPCR_TX_ENA|GM_GPCR_RX_ENA|
		    GM_GPCR_DUP_FULL|GM_GPCR_FC_RX_DIS|GM_GPCR_AU_FCT_DIS);

	/* Set WOL address */
	memcpy_toio(hw->regs + WOL_REGS(port, WOL_MAC_ADDR),
		    sky2->netdev->dev_addr, ETH_ALEN);

	/* Turn on appropriate WOL control bits */
	sky2_write16(hw, WOL_REGS(port, WOL_CTRL_STAT), WOL_CTL_CLEAR_RESULT);
	ctrl = 0;
	if (sky2->wol & WAKE_PHY)
		ctrl |= WOL_CTL_ENA_PME_ON_LINK_CHG|WOL_CTL_ENA_LINK_CHG_UNIT;
	else
		ctrl |= WOL_CTL_DIS_PME_ON_LINK_CHG|WOL_CTL_DIS_LINK_CHG_UNIT;

	if (sky2->wol & WAKE_MAGIC)
		ctrl |= WOL_CTL_ENA_PME_ON_MAGIC_PKT|WOL_CTL_ENA_MAGIC_PKT_UNIT;
	else
		ctrl |= WOL_CTL_DIS_PME_ON_MAGIC_PKT|WOL_CTL_DIS_MAGIC_PKT_UNIT;

	ctrl |= WOL_CTL_DIS_PME_ON_PATTERN|WOL_CTL_DIS_PATTERN_UNIT;
	sky2_write16(hw, WOL_REGS(port, WOL_CTRL_STAT), ctrl);

	/* Turn on legacy PCI-Express PME mode */
	reg1 = sky2_pci_read32(hw, PCI_DEV_REG1);
	reg1 |= PCI_Y2_PME_LEGACY;
	sky2_pci_write32(hw, PCI_DEV_REG1, reg1);

	/* block receiver */
	sky2_write8(hw, SK_REG(port, RX_GMF_CTRL_T), GMF_RST_SET);

}

static void sky2_set_tx_stfwd(struct sky2_hw *hw, unsigned port)
{
	struct net_device *dev = hw->dev[port];

	if ( (hw->chip_id == CHIP_ID_YUKON_EX &&
	      hw->chip_rev != CHIP_REV_YU_EX_A0) ||
	     hw->chip_id == CHIP_ID_YUKON_FE_P ||
	     hw->chip_id == CHIP_ID_YUKON_SUPR) {
		/* Yukon-Extreme B0 and further Extreme devices */
		/* enable Store & Forward mode for TX */

		if (dev->mtu <= ETH_DATA_LEN)
			sky2_write32(hw, SK_REG(port, TX_GMF_CTRL_T),
				     TX_JUMBO_DIS | TX_STFW_ENA);

		else
			sky2_write32(hw, SK_REG(port, TX_GMF_CTRL_T),
				     TX_JUMBO_ENA| TX_STFW_ENA);
	} else {
		if (dev->mtu <= ETH_DATA_LEN)
			sky2_write32(hw, SK_REG(port, TX_GMF_CTRL_T), TX_STFW_ENA);
		else {
			/* set Tx GMAC FIFO Almost Empty Threshold */
			sky2_write32(hw, SK_REG(port, TX_GMF_AE_THR),
				     (ECU_JUMBO_WM << 16) | ECU_AE_THR);

			sky2_write32(hw, SK_REG(port, TX_GMF_CTRL_T), TX_STFW_DIS);

			/* Can't do offload because of lack of store/forward */
			dev->features &= ~(NETIF_F_TSO | NETIF_F_SG | NETIF_F_ALL_CSUM);
		}
	}
}

static void sky2_mac_init(struct sky2_hw *hw, unsigned port)
{
	struct sky2_port *sky2 = netdev_priv(hw->dev[port]);
	u16 reg;
	u32 rx_reg;
	int i;
	const u8 *addr = hw->dev[port]->dev_addr;

	sky2_write8(hw, SK_REG(port, GPHY_CTRL), GPC_RST_SET);
	sky2_write8(hw, SK_REG(port, GPHY_CTRL), GPC_RST_CLR);

	sky2_write8(hw, SK_REG(port, GMAC_CTRL), GMC_RST_CLR);

	if (hw->chip_id == CHIP_ID_YUKON_XL && hw->chip_rev == 0 && port == 1) {
		/* WA DEV_472 -- looks like crossed wires on port 2 */
		/* clear GMAC 1 Control reset */
		sky2_write8(hw, SK_REG(0, GMAC_CTRL), GMC_RST_CLR);
		do {
			sky2_write8(hw, SK_REG(1, GMAC_CTRL), GMC_RST_SET);
			sky2_write8(hw, SK_REG(1, GMAC_CTRL), GMC_RST_CLR);
		} while (gm_phy_read(hw, 1, PHY_MARV_ID0) != PHY_MARV_ID0_VAL ||
			 gm_phy_read(hw, 1, PHY_MARV_ID1) != PHY_MARV_ID1_Y2 ||
			 gm_phy_read(hw, 1, PHY_MARV_INT_MASK) != 0);
	}

	sky2_read16(hw, SK_REG(port, GMAC_IRQ_SRC));

	/* Enable Transmit FIFO Underrun */
	sky2_write8(hw, SK_REG(port, GMAC_IRQ_MSK), GMAC_DEF_MSK);

	spin_lock_bh(&sky2->phy_lock);
	sky2_phy_power_up(hw, port);
	sky2_phy_init(hw, port);
	spin_unlock_bh(&sky2->phy_lock);

	/* MIB clear */
	reg = gma_read16(hw, port, GM_PHY_ADDR);
	gma_write16(hw, port, GM_PHY_ADDR, reg | GM_PAR_MIB_CLR);

	for (i = GM_MIB_CNT_BASE; i <= GM_MIB_CNT_END; i += 4)
		gma_read16(hw, port, i);
	gma_write16(hw, port, GM_PHY_ADDR, reg);

	/* transmit control */
	gma_write16(hw, port, GM_TX_CTRL, TX_COL_THR(TX_COL_DEF));

	/* receive control reg: unicast + multicast + no FCS  */
	gma_write16(hw, port, GM_RX_CTRL,
		    GM_RXCR_UCF_ENA | GM_RXCR_CRC_DIS | GM_RXCR_MCF_ENA);

	/* transmit flow control */
	gma_write16(hw, port, GM_TX_FLOW_CTRL, 0xffff);

	/* transmit parameter */
	gma_write16(hw, port, GM_TX_PARAM,
		    TX_JAM_LEN_VAL(TX_JAM_LEN_DEF) |
		    TX_JAM_IPG_VAL(TX_JAM_IPG_DEF) |
		    TX_IPG_JAM_DATA(TX_IPG_JAM_DEF) |
		    TX_BACK_OFF_LIM(TX_BOF_LIM_DEF));

	/* serial mode register */
	reg = DATA_BLIND_VAL(DATA_BLIND_DEF) |
		GM_SMOD_VLAN_ENA | IPG_DATA_VAL(IPG_DATA_DEF);

	if (hw->dev[port]->mtu > ETH_DATA_LEN)
		reg |= GM_SMOD_JUMBO_ENA;

	gma_write16(hw, port, GM_SERIAL_MODE, reg);

	/* virtual address for data */
	gma_set_addr(hw, port, GM_SRC_ADDR_2L, addr);

	/* physical address: used for pause frames */
	gma_set_addr(hw, port, GM_SRC_ADDR_1L, addr);

	/* ignore counter overflows */
	gma_write16(hw, port, GM_TX_IRQ_MSK, 0);
	gma_write16(hw, port, GM_RX_IRQ_MSK, 0);
	gma_write16(hw, port, GM_TR_IRQ_MSK, 0);

	/* Configure Rx MAC FIFO */
	sky2_write8(hw, SK_REG(port, RX_GMF_CTRL_T), GMF_RST_CLR);
	rx_reg = GMF_OPER_ON | GMF_RX_F_FL_ON;
	if (hw->chip_id == CHIP_ID_YUKON_EX ||
	    hw->chip_id == CHIP_ID_YUKON_FE_P)
		rx_reg |= GMF_RX_OVER_ON;

	sky2_write32(hw, SK_REG(port, RX_GMF_CTRL_T), rx_reg);

	if (hw->chip_id == CHIP_ID_YUKON_XL) {
		/* Hardware errata - clear flush mask */
		sky2_write16(hw, SK_REG(port, RX_GMF_FL_MSK), 0);
	} else {
		/* Flush Rx MAC FIFO on any flow control or error */
		sky2_write16(hw, SK_REG(port, RX_GMF_FL_MSK), GMR_FS_ANY_ERR);
	}

	/* Set threshold to 0xa (64 bytes) + 1 to workaround pause bug  */
	reg = RX_GMF_FL_THR_DEF + 1;
	/* Another magic mystery workaround from sk98lin */
	if (hw->chip_id == CHIP_ID_YUKON_FE_P &&
	    hw->chip_rev == CHIP_REV_YU_FE2_A0)
		reg = 0x178;
	sky2_write16(hw, SK_REG(port, RX_GMF_FL_THR), reg);

	/* Configure Tx MAC FIFO */
	sky2_write8(hw, SK_REG(port, TX_GMF_CTRL_T), GMF_RST_CLR);
	sky2_write16(hw, SK_REG(port, TX_GMF_CTRL_T), GMF_OPER_ON);

	/* On chips without ram buffer, pause is controled by MAC level */
	if (!(hw->flags & SKY2_HW_RAM_BUFFER)) {
		sky2_write8(hw, SK_REG(port, RX_GMF_LP_THR), 768/8);
		sky2_write8(hw, SK_REG(port, RX_GMF_UP_THR), 1024/8);

		sky2_set_tx_stfwd(hw, port);
	}

	if (hw->chip_id == CHIP_ID_YUKON_FE_P &&
	    hw->chip_rev == CHIP_REV_YU_FE2_A0) {
		/* disable dynamic watermark */
		reg = sky2_read16(hw, SK_REG(port, TX_GMF_EA));
		reg &= ~TX_DYN_WM_ENA;
		sky2_write16(hw, SK_REG(port, TX_GMF_EA), reg);
	}
}

/* Assign Ram Buffer allocation to queue */
static void sky2_ramset(struct sky2_hw *hw, u16 q, u32 start, u32 space)
{
	u32 end;

	/* convert from K bytes to qwords used for hw register */
	start *= 1024/8;
	space *= 1024/8;
	end = start + space - 1;

	sky2_write8(hw, RB_ADDR(q, RB_CTRL), RB_RST_CLR);
	sky2_write32(hw, RB_ADDR(q, RB_START), start);
	sky2_write32(hw, RB_ADDR(q, RB_END), end);
	sky2_write32(hw, RB_ADDR(q, RB_WP), start);
	sky2_write32(hw, RB_ADDR(q, RB_RP), start);

	if (q == Q_R1 || q == Q_R2) {
		u32 tp = space - space/4;

		/* On receive queue's set the thresholds
		 * give receiver priority when > 3/4 full
		 * send pause when down to 2K
		 */
		sky2_write32(hw, RB_ADDR(q, RB_RX_UTHP), tp);
		sky2_write32(hw, RB_ADDR(q, RB_RX_LTHP), space/2);

		tp = space - 2048/8;
		sky2_write32(hw, RB_ADDR(q, RB_RX_UTPP), tp);
		sky2_write32(hw, RB_ADDR(q, RB_RX_LTPP), space/4);
	} else {
		/* Enable store & forward on Tx queue's because
		 * Tx FIFO is only 1K on Yukon
		 */
		sky2_write8(hw, RB_ADDR(q, RB_CTRL), RB_ENA_STFWD);
	}

	sky2_write8(hw, RB_ADDR(q, RB_CTRL), RB_ENA_OP_MD);
	sky2_read8(hw, RB_ADDR(q, RB_CTRL));
}

/* Setup Bus Memory Interface */
static void sky2_qset(struct sky2_hw *hw, u16 q)
{
	sky2_write32(hw, Q_ADDR(q, Q_CSR), BMU_CLR_RESET);
	sky2_write32(hw, Q_ADDR(q, Q_CSR), BMU_OPER_INIT);
	sky2_write32(hw, Q_ADDR(q, Q_CSR), BMU_FIFO_OP_ON);
	sky2_write32(hw, Q_ADDR(q, Q_WM),  BMU_WM_DEFAULT);
}

/* Setup prefetch unit registers. This is the interface between
 * hardware and driver list elements
 */
static void sky2_prefetch_init(struct sky2_hw *hw, u32 qaddr,
			       dma_addr_t addr, u32 last)
{
	sky2_write32(hw, Y2_QADDR(qaddr, PREF_UNIT_CTRL), PREF_UNIT_RST_SET);
	sky2_write32(hw, Y2_QADDR(qaddr, PREF_UNIT_CTRL), PREF_UNIT_RST_CLR);
	sky2_write32(hw, Y2_QADDR(qaddr, PREF_UNIT_ADDR_HI), upper_32_bits(addr));
	sky2_write32(hw, Y2_QADDR(qaddr, PREF_UNIT_ADDR_LO), lower_32_bits(addr));
	sky2_write16(hw, Y2_QADDR(qaddr, PREF_UNIT_LAST_IDX), last);
	sky2_write32(hw, Y2_QADDR(qaddr, PREF_UNIT_CTRL), PREF_UNIT_OP_ON);

	sky2_read32(hw, Y2_QADDR(qaddr, PREF_UNIT_CTRL));
}

static inline struct sky2_tx_le *get_tx_le(struct sky2_port *sky2, u16 *slot)
{
	struct sky2_tx_le *le = sky2->tx_le + *slot;
	struct tx_ring_info *re = sky2->tx_ring + *slot;

	*slot = RING_NEXT(*slot, sky2->tx_ring_size);
	re->flags = 0;
	re->skb = NULL;
	le->ctrl = 0;
	return le;
}

static void tx_init(struct sky2_port *sky2)
{
	struct sky2_tx_le *le;

	sky2->tx_prod = sky2->tx_cons = 0;
	sky2->tx_tcpsum = 0;
	sky2->tx_last_mss = 0;

	le = get_tx_le(sky2, &sky2->tx_prod);
	le->addr = 0;
	le->opcode = OP_ADDR64 | HW_OWNER;
	sky2->tx_last_upper = 0;
}

/* Update chip's next pointer */
static inline void sky2_put_idx(struct sky2_hw *hw, unsigned q, u16 idx)
{
	/* Make sure write' to descriptors are complete before we tell hardware */
	wmb();
	sky2_write16(hw, Y2_QADDR(q, PREF_UNIT_PUT_IDX), idx);

	/* Synchronize I/O on since next processor may write to tail */
	mmiowb();
}


static inline struct sky2_rx_le *sky2_next_rx(struct sky2_port *sky2)
{
	struct sky2_rx_le *le = sky2->rx_le + sky2->rx_put;
	sky2->rx_put = RING_NEXT(sky2->rx_put, RX_LE_SIZE);
	le->ctrl = 0;
	return le;
}

/* Build description to hardware for one receive segment */
static void sky2_rx_add(struct sky2_port *sky2,  u8 op,
			dma_addr_t map, unsigned len)
{
	struct sky2_rx_le *le;

	if (sizeof(dma_addr_t) > sizeof(u32)) {
		le = sky2_next_rx(sky2);
		le->addr = cpu_to_le32(upper_32_bits(map));
		le->opcode = OP_ADDR64 | HW_OWNER;
	}

	le = sky2_next_rx(sky2);
	le->addr = cpu_to_le32(lower_32_bits(map));
	le->length = cpu_to_le16(len);
	le->opcode = op | HW_OWNER;
}

/* Build description to hardware for one possibly fragmented skb */
static void sky2_rx_submit(struct sky2_port *sky2,
			   const struct rx_ring_info *re)
{
	int i;

	sky2_rx_add(sky2, OP_PACKET, re->data_addr, sky2->rx_data_size);

	for (i = 0; i < skb_shinfo(re->skb)->nr_frags; i++)
		sky2_rx_add(sky2, OP_BUFFER, re->frag_addr[i], PAGE_SIZE);
}


static int sky2_rx_map_skb(struct pci_dev *pdev, struct rx_ring_info *re,
			    unsigned size)
{
	struct sk_buff *skb = re->skb;
	int i;

	re->data_addr = pci_map_single(pdev, skb->data, size, PCI_DMA_FROMDEVICE);
	if (unlikely(pci_dma_mapping_error(pdev, re->data_addr)))
		return -EIO;

	pci_unmap_len_set(re, data_size, size);

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++)
		re->frag_addr[i] = pci_map_page(pdev,
						skb_shinfo(skb)->frags[i].page,
						skb_shinfo(skb)->frags[i].page_offset,
						skb_shinfo(skb)->frags[i].size,
						PCI_DMA_FROMDEVICE);
	return 0;
}

static void sky2_rx_unmap_skb(struct pci_dev *pdev, struct rx_ring_info *re)
{
	struct sk_buff *skb = re->skb;
	int i;

	pci_unmap_single(pdev, re->data_addr, pci_unmap_len(re, data_size),
			 PCI_DMA_FROMDEVICE);

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++)
		pci_unmap_page(pdev, re->frag_addr[i],
			       skb_shinfo(skb)->frags[i].size,
			       PCI_DMA_FROMDEVICE);
}

/* Tell chip where to start receive checksum.
 * Actually has two checksums, but set both same to avoid possible byte
 * order problems.
 */
static void rx_set_checksum(struct sky2_port *sky2)
{
	struct sky2_rx_le *le = sky2_next_rx(sky2);

	le->addr = cpu_to_le32((ETH_HLEN << 16) | ETH_HLEN);
	le->ctrl = 0;
	le->opcode = OP_TCPSTART | HW_OWNER;

	sky2_write32(sky2->hw,
		     Q_ADDR(rxqaddr[sky2->port], Q_CSR),
		     (sky2->flags & SKY2_FLAG_RX_CHECKSUM)
		     ? BMU_ENA_RX_CHKSUM : BMU_DIS_RX_CHKSUM);
}

/*
 * The RX Stop command will not work for Yukon-2 if the BMU does not
 * reach the end of packet and since we can't make sure that we have
 * incoming data, we must reset the BMU while it is not doing a DMA
 * transfer. Since it is possible that the RX path is still active,
 * the RX RAM buffer will be stopped first, so any possible incoming
 * data will not trigger a DMA. After the RAM buffer is stopped, the
 * BMU is polled until any DMA in progress is ended and only then it
 * will be reset.
 */
static void sky2_rx_stop(struct sky2_port *sky2)
{
	struct sky2_hw *hw = sky2->hw;
	unsigned rxq = rxqaddr[sky2->port];
	int i;

	/* disable the RAM Buffer receive queue */
	sky2_write8(hw, RB_ADDR(rxq, RB_CTRL), RB_DIS_OP_MD);

	for (i = 0; i < 0xffff; i++)
		if (sky2_read8(hw, RB_ADDR(rxq, Q_RSL))
		    == sky2_read8(hw, RB_ADDR(rxq, Q_RL)))
			goto stopped;

	printk(KERN_WARNING PFX "%s: receiver stop failed\n",
	       sky2->netdev->name);
stopped:
	sky2_write32(hw, Q_ADDR(rxq, Q_CSR), BMU_RST_SET | BMU_FIFO_RST);

	/* reset the Rx prefetch unit */
	sky2_write32(hw, Y2_QADDR(rxq, PREF_UNIT_CTRL), PREF_UNIT_RST_SET);
	mmiowb();
}

/* Clean out receive buffer area, assumes receiver hardware stopped */
static void sky2_rx_clean(struct sky2_port *sky2)
{
	unsigned i;

	memset(sky2->rx_le, 0, RX_LE_BYTES);
	for (i = 0; i < sky2->rx_pending; i++) {
		struct rx_ring_info *re = sky2->rx_ring + i;

		if (re->skb) {
			sky2_rx_unmap_skb(sky2->hw->pdev, re);
			kfree_skb(re->skb);
			re->skb = NULL;
		}
	}
}

/* Basic MII support */
static int sky2_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct mii_ioctl_data *data = if_mii(ifr);
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;
	int err = -EOPNOTSUPP;

	if (!netif_running(dev))
		return -ENODEV;	/* Phy still in reset */

	switch (cmd) {
	case SIOCGMIIPHY:
		data->phy_id = PHY_ADDR_MARV;

		/* fallthru */
	case SIOCGMIIREG: {
		u16 val = 0;

		spin_lock_bh(&sky2->phy_lock);
		err = __gm_phy_read(hw, sky2->port, data->reg_num & 0x1f, &val);
		spin_unlock_bh(&sky2->phy_lock);

		data->val_out = val;
		break;
	}

	case SIOCSMIIREG:
		spin_lock_bh(&sky2->phy_lock);
		err = gm_phy_write(hw, sky2->port, data->reg_num & 0x1f,
				   data->val_in);
		spin_unlock_bh(&sky2->phy_lock);
		break;
	}
	return err;
}

#ifdef SKY2_VLAN_TAG_USED
static void sky2_set_vlan_mode(struct sky2_hw *hw, u16 port, bool onoff)
{
	if (onoff) {
		sky2_write32(hw, SK_REG(port, RX_GMF_CTRL_T),
			     RX_VLAN_STRIP_ON);
		sky2_write32(hw, SK_REG(port, TX_GMF_CTRL_T),
			     TX_VLAN_TAG_ON);
	} else {
		sky2_write32(hw, SK_REG(port, RX_GMF_CTRL_T),
			     RX_VLAN_STRIP_OFF);
		sky2_write32(hw, SK_REG(port, TX_GMF_CTRL_T),
			     TX_VLAN_TAG_OFF);
	}
}

static void sky2_vlan_rx_register(struct net_device *dev, struct vlan_group *grp)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;
	u16 port = sky2->port;

	netif_tx_lock_bh(dev);
	napi_disable(&hw->napi);

	sky2->vlgrp = grp;
	sky2_set_vlan_mode(hw, port, grp != NULL);

	sky2_read32(hw, B0_Y2_SP_LISR);
	napi_enable(&hw->napi);
	netif_tx_unlock_bh(dev);
}
#endif

/* Amount of required worst case padding in rx buffer */
static inline unsigned sky2_rx_pad(const struct sky2_hw *hw)
{
	return (hw->flags & SKY2_HW_RAM_BUFFER) ? 8 : 2;
}

/*
 * Allocate an skb for receiving. If the MTU is large enough
 * make the skb non-linear with a fragment list of pages.
 */
static struct sk_buff *sky2_rx_alloc(struct sky2_port *sky2)
{
	struct sk_buff *skb;
	int i;

	skb = netdev_alloc_skb(sky2->netdev,
			       sky2->rx_data_size + sky2_rx_pad(sky2->hw));
	if (!skb)
		goto nomem;

	if (sky2->hw->flags & SKY2_HW_RAM_BUFFER) {
		unsigned char *start;
		/*
		 * Workaround for a bug in FIFO that cause hang
		 * if the FIFO if the receive buffer is not 64 byte aligned.
		 * The buffer returned from netdev_alloc_skb is
		 * aligned except if slab debugging is enabled.
		 */
		start = PTR_ALIGN(skb->data, 8);
		skb_reserve(skb, start - skb->data);
	} else
		skb_reserve(skb, NET_IP_ALIGN);

	for (i = 0; i < sky2->rx_nfrags; i++) {
		struct page *page = alloc_page(GFP_ATOMIC);

		if (!page)
			goto free_partial;
		skb_fill_page_desc(skb, i, page, 0, PAGE_SIZE);
	}

	return skb;
free_partial:
	kfree_skb(skb);
nomem:
	return NULL;
}

static inline void sky2_rx_update(struct sky2_port *sky2, unsigned rxq)
{
	sky2_put_idx(sky2->hw, rxq, sky2->rx_put);
}

/*
 * Allocate and setup receiver buffer pool.
 * Normal case this ends up creating one list element for skb
 * in the receive ring. Worst case if using large MTU and each
 * allocation falls on a different 64 bit region, that results
 * in 6 list elements per ring entry.
 * One element is used for checksum enable/disable, and one
 * extra to avoid wrap.
 */
static int sky2_rx_start(struct sky2_port *sky2)
{
	struct sky2_hw *hw = sky2->hw;
	struct rx_ring_info *re;
	unsigned rxq = rxqaddr[sky2->port];
	unsigned i, size, thresh;

	sky2->rx_put = sky2->rx_next = 0;
	sky2_qset(hw, rxq);

	/* On PCI express lowering the watermark gives better performance */
	if (pci_find_capability(hw->pdev, PCI_CAP_ID_EXP))
		sky2_write32(hw, Q_ADDR(rxq, Q_WM), BMU_WM_PEX);

	/* These chips have no ram buffer?
	 * MAC Rx RAM Read is controlled by hardware */
	if (hw->chip_id == CHIP_ID_YUKON_EC_U &&
	    (hw->chip_rev == CHIP_REV_YU_EC_U_A1
	     || hw->chip_rev == CHIP_REV_YU_EC_U_B0))
		sky2_write32(hw, Q_ADDR(rxq, Q_TEST), F_M_RX_RAM_DIS);

	sky2_prefetch_init(hw, rxq, sky2->rx_le_map, RX_LE_SIZE - 1);

	if (!(hw->flags & SKY2_HW_NEW_LE))
		rx_set_checksum(sky2);

	/* Space needed for frame data + headers rounded up */
	size = roundup(sky2->netdev->mtu + ETH_HLEN + VLAN_HLEN, 8);

	/* Stopping point for hardware truncation */
	thresh = (size - 8) / sizeof(u32);

	sky2->rx_nfrags = size >> PAGE_SHIFT;
	BUG_ON(sky2->rx_nfrags > ARRAY_SIZE(re->frag_addr));

	/* Compute residue after pages */
	size -= sky2->rx_nfrags << PAGE_SHIFT;

	/* Optimize to handle small packets and headers */
	if (size < copybreak)
		size = copybreak;
	if (size < ETH_HLEN)
		size = ETH_HLEN;

	sky2->rx_data_size = size;

	/* Fill Rx ring */
	for (i = 0; i < sky2->rx_pending; i++) {
		re = sky2->rx_ring + i;

		re->skb = sky2_rx_alloc(sky2);
		if (!re->skb)
			goto nomem;

		if (sky2_rx_map_skb(hw->pdev, re, sky2->rx_data_size)) {
			dev_kfree_skb(re->skb);
			re->skb = NULL;
			goto nomem;
		}

		sky2_rx_submit(sky2, re);
	}

	/*
	 * The receiver hangs if it receives frames larger than the
	 * packet buffer. As a workaround, truncate oversize frames, but
	 * the register is limited to 9 bits, so if you do frames > 2052
	 * you better get the MTU right!
	 */
	if (thresh > 0x1ff)
		sky2_write32(hw, SK_REG(sky2->port, RX_GMF_CTRL_T), RX_TRUNC_OFF);
	else {
		sky2_write16(hw, SK_REG(sky2->port, RX_GMF_TR_THR), thresh);
		sky2_write32(hw, SK_REG(sky2->port, RX_GMF_CTRL_T), RX_TRUNC_ON);
	}

	/* Tell chip about available buffers */
	sky2_rx_update(sky2, rxq);
	return 0;
nomem:
	sky2_rx_clean(sky2);
	return -ENOMEM;
}

static int sky2_alloc_buffers(struct sky2_port *sky2)
{
	struct sky2_hw *hw = sky2->hw;

	/* must be power of 2 */
	sky2->tx_le = pci_alloc_consistent(hw->pdev,
					   sky2->tx_ring_size *
					   sizeof(struct sky2_tx_le),
					   &sky2->tx_le_map);
	if (!sky2->tx_le)
		goto nomem;

	sky2->tx_ring = kcalloc(sky2->tx_ring_size, sizeof(struct tx_ring_info),
				GFP_KERNEL);
	if (!sky2->tx_ring)
		goto nomem;

	sky2->rx_le = pci_alloc_consistent(hw->pdev, RX_LE_BYTES,
					   &sky2->rx_le_map);
	if (!sky2->rx_le)
		goto nomem;
	memset(sky2->rx_le, 0, RX_LE_BYTES);

	sky2->rx_ring = kcalloc(sky2->rx_pending, sizeof(struct rx_ring_info),
				GFP_KERNEL);
	if (!sky2->rx_ring)
		goto nomem;

	return 0;
nomem:
	return -ENOMEM;
}

static void sky2_free_buffers(struct sky2_port *sky2)
{
	struct sky2_hw *hw = sky2->hw;

	if (sky2->rx_le) {
		pci_free_consistent(hw->pdev, RX_LE_BYTES,
				    sky2->rx_le, sky2->rx_le_map);
		sky2->rx_le = NULL;
	}
	if (sky2->tx_le) {
		pci_free_consistent(hw->pdev,
				    sky2->tx_ring_size * sizeof(struct sky2_tx_le),
				    sky2->tx_le, sky2->tx_le_map);
		sky2->tx_le = NULL;
	}
	kfree(sky2->tx_ring);
	kfree(sky2->rx_ring);

	sky2->tx_ring = NULL;
	sky2->rx_ring = NULL;
}

/* Bring up network interface. */
static int sky2_up(struct net_device *dev)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;
	unsigned port = sky2->port;
	u32 imask, ramsize;
	int cap, err;
	struct net_device *otherdev = hw->dev[sky2->port^1];

	/*
 	 * On dual port PCI-X card, there is an problem where status
	 * can be received out of order due to split transactions
	 */
	if (otherdev && netif_running(otherdev) &&
 	    (cap = pci_find_capability(hw->pdev, PCI_CAP_ID_PCIX))) {
 		u16 cmd;

		cmd = sky2_pci_read16(hw, cap + PCI_X_CMD);
 		cmd &= ~PCI_X_CMD_MAX_SPLIT;
 		sky2_pci_write16(hw, cap + PCI_X_CMD, cmd);

 	}

	netif_carrier_off(dev);

	err = sky2_alloc_buffers(sky2);
	if (err)
		goto err_out;

	tx_init(sky2);

	sky2_mac_init(hw, port);

	/* Register is number of 4K blocks on internal RAM buffer. */
	ramsize = sky2_read8(hw, B2_E_0) * 4;
	if (ramsize > 0) {
		u32 rxspace;

		pr_debug(PFX "%s: ram buffer %dK\n", dev->name, ramsize);
		if (ramsize < 16)
			rxspace = ramsize / 2;
		else
			rxspace = 8 + (2*(ramsize - 16))/3;

		sky2_ramset(hw, rxqaddr[port], 0, rxspace);
		sky2_ramset(hw, txqaddr[port], rxspace, ramsize - rxspace);

		/* Make sure SyncQ is disabled */
		sky2_write8(hw, RB_ADDR(port == 0 ? Q_XS1 : Q_XS2, RB_CTRL),
			    RB_RST_SET);
	}

	sky2_qset(hw, txqaddr[port]);

	/* This is copied from sk98lin 10.0.5.3; no one tells me about erratta's */
	if (hw->chip_id == CHIP_ID_YUKON_EX && hw->chip_rev == CHIP_REV_YU_EX_B0)
		sky2_write32(hw, Q_ADDR(txqaddr[port], Q_TEST), F_TX_CHK_AUTO_OFF);

	/* Set almost empty threshold */
	if (hw->chip_id == CHIP_ID_YUKON_EC_U
	    && hw->chip_rev == CHIP_REV_YU_EC_U_A0)
		sky2_write16(hw, Q_ADDR(txqaddr[port], Q_AL), ECU_TXFF_LEV);

	sky2_prefetch_init(hw, txqaddr[port], sky2->tx_le_map,
			   sky2->tx_ring_size - 1);

#ifdef SKY2_VLAN_TAG_USED
	sky2_set_vlan_mode(hw, port, sky2->vlgrp != NULL);
#endif

	err = sky2_rx_start(sky2);
	if (err)
		goto err_out;

	/* Enable interrupts from phy/mac for port */
	imask = sky2_read32(hw, B0_IMSK);
	imask |= portirq_msk[port];
	sky2_write32(hw, B0_IMSK, imask);
	sky2_read32(hw, B0_IMSK);

	if (netif_msg_ifup(sky2))
		printk(KERN_INFO PFX "%s: enabling interface\n", dev->name);

	return 0;

err_out:
	sky2_free_buffers(sky2);
	return err;
}

/* Modular subtraction in ring */
static inline int tx_inuse(const struct sky2_port *sky2)
{
	return (sky2->tx_prod - sky2->tx_cons) & (sky2->tx_ring_size - 1);
}

/* Number of list elements available for next tx */
static inline int tx_avail(const struct sky2_port *sky2)
{
	return sky2->tx_pending - tx_inuse(sky2);
}

/* Estimate of number of transmit list elements required */
static unsigned tx_le_req(const struct sk_buff *skb)
{
	unsigned count;

	count = (skb_shinfo(skb)->nr_frags + 1)
		* (sizeof(dma_addr_t) / sizeof(u32));

	if (skb_is_gso(skb))
		++count;
	else if (sizeof(dma_addr_t) == sizeof(u32))
		++count;	/* possible vlan */

	if (skb->ip_summed == CHECKSUM_PARTIAL)
		++count;

	return count;
}

static void sky2_tx_unmap(struct pci_dev *pdev,
			  const struct tx_ring_info *re)
{
	if (re->flags & TX_MAP_SINGLE)
		pci_unmap_single(pdev, pci_unmap_addr(re, mapaddr),
				 pci_unmap_len(re, maplen),
				 PCI_DMA_TODEVICE);
	else if (re->flags & TX_MAP_PAGE)
		pci_unmap_page(pdev, pci_unmap_addr(re, mapaddr),
			       pci_unmap_len(re, maplen),
			       PCI_DMA_TODEVICE);
}

/*
 * Put one packet in ring for transmit.
 * A single packet can generate multiple list elements, and
 * the number of ring elements will probably be less than the number
 * of list elements used.
 */
static netdev_tx_t sky2_xmit_frame(struct sk_buff *skb,
				   struct net_device *dev)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;
	struct sky2_tx_le *le = NULL;
	struct tx_ring_info *re;
	unsigned i, len;
	dma_addr_t mapping;
	u32 upper;
	u16 slot;
	u16 mss;
	u8 ctrl;

 	if (unlikely(tx_avail(sky2) < tx_le_req(skb)))
  		return NETDEV_TX_BUSY;

	len = skb_headlen(skb);
	mapping = pci_map_single(hw->pdev, skb->data, len, PCI_DMA_TODEVICE);

	if (pci_dma_mapping_error(hw->pdev, mapping))
		goto mapping_error;

	slot = sky2->tx_prod;
	if (unlikely(netif_msg_tx_queued(sky2)))
		printk(KERN_DEBUG "%s: tx queued, slot %u, len %d\n",
		       dev->name, slot, skb->len);

	/* Send high bits if needed */
	upper = upper_32_bits(mapping);
	if (upper != sky2->tx_last_upper) {
		le = get_tx_le(sky2, &slot);
		le->addr = cpu_to_le32(upper);
		sky2->tx_last_upper = upper;
		le->opcode = OP_ADDR64 | HW_OWNER;
	}

	/* Check for TCP Segmentation Offload */
	mss = skb_shinfo(skb)->gso_size;
	if (mss != 0) {

		if (!(hw->flags & SKY2_HW_NEW_LE))
			mss += ETH_HLEN + ip_hdrlen(skb) + tcp_hdrlen(skb);

  		if (mss != sky2->tx_last_mss) {
			le = get_tx_le(sky2, &slot);
  			le->addr = cpu_to_le32(mss);

			if (hw->flags & SKY2_HW_NEW_LE)
				le->opcode = OP_MSS | HW_OWNER;
			else
				le->opcode = OP_LRGLEN | HW_OWNER;
			sky2->tx_last_mss = mss;
		}
	}

	ctrl = 0;
#ifdef SKY2_VLAN_TAG_USED
	/* Add VLAN tag, can piggyback on LRGLEN or ADDR64 */
	if (sky2->vlgrp && vlan_tx_tag_present(skb)) {
		if (!le) {
			le = get_tx_le(sky2, &slot);
			le->addr = 0;
			le->opcode = OP_VLAN|HW_OWNER;
		} else
			le->opcode |= OP_VLAN;
		le->length = cpu_to_be16(vlan_tx_tag_get(skb));
		ctrl |= INS_VLAN;
	}
#endif

	/* Handle TCP checksum offload */
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		/* On Yukon EX (some versions) encoding change. */
 		if (hw->flags & SKY2_HW_AUTO_TX_SUM)
 			ctrl |= CALSUM;	/* auto checksum */
		else {
			const unsigned offset = skb_transport_offset(skb);
			u32 tcpsum;

			tcpsum = offset << 16;			/* sum start */
			tcpsum |= offset + skb->csum_offset;	/* sum write */

			ctrl |= CALSUM | WR_SUM | INIT_SUM | LOCK_SUM;
			if (ip_hdr(skb)->protocol == IPPROTO_UDP)
				ctrl |= UDPTCP;

			if (tcpsum != sky2->tx_tcpsum) {
				sky2->tx_tcpsum = tcpsum;

				le = get_tx_le(sky2, &slot);
				le->addr = cpu_to_le32(tcpsum);
				le->length = 0;	/* initial checksum value */
				le->ctrl = 1;	/* one packet */
				le->opcode = OP_TCPLISW | HW_OWNER;
			}
		}
	}

	re = sky2->tx_ring + slot;
	re->flags = TX_MAP_SINGLE;
	pci_unmap_addr_set(re, mapaddr, mapping);
	pci_unmap_len_set(re, maplen, len);

	le = get_tx_le(sky2, &slot);
	le->addr = cpu_to_le32(lower_32_bits(mapping));
	le->length = cpu_to_le16(len);
	le->ctrl = ctrl;
	le->opcode = mss ? (OP_LARGESEND | HW_OWNER) : (OP_PACKET | HW_OWNER);


	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

		mapping = pci_map_page(hw->pdev, frag->page, frag->page_offset,
				       frag->size, PCI_DMA_TODEVICE);

		if (pci_dma_mapping_error(hw->pdev, mapping))
			goto mapping_unwind;

		upper = upper_32_bits(mapping);
		if (upper != sky2->tx_last_upper) {
			le = get_tx_le(sky2, &slot);
			le->addr = cpu_to_le32(upper);
			sky2->tx_last_upper = upper;
			le->opcode = OP_ADDR64 | HW_OWNER;
		}

		re = sky2->tx_ring + slot;
		re->flags = TX_MAP_PAGE;
		pci_unmap_addr_set(re, mapaddr, mapping);
		pci_unmap_len_set(re, maplen, frag->size);

		le = get_tx_le(sky2, &slot);
		le->addr = cpu_to_le32(lower_32_bits(mapping));
		le->length = cpu_to_le16(frag->size);
		le->ctrl = ctrl;
		le->opcode = OP_BUFFER | HW_OWNER;
	}

	re->skb = skb;
	le->ctrl |= EOP;

	sky2->tx_prod = slot;

	if (tx_avail(sky2) <= MAX_SKB_TX_LE)
		netif_stop_queue(dev);

	sky2_put_idx(hw, txqaddr[sky2->port], sky2->tx_prod);

	return NETDEV_TX_OK;

mapping_unwind:
	for (i = sky2->tx_prod; i != slot; i = RING_NEXT(i, sky2->tx_ring_size)) {
		re = sky2->tx_ring + i;

		sky2_tx_unmap(hw->pdev, re);
	}

mapping_error:
	if (net_ratelimit())
		dev_warn(&hw->pdev->dev, "%s: tx mapping error\n", dev->name);
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

/*
 * Free ring elements from starting at tx_cons until "done"
 *
 * NB:
 *  1. The hardware will tell us about partial completion of multi-part
 *     buffers so make sure not to free skb to early.
 *  2. This may run in parallel start_xmit because the it only
 *     looks at the tail of the queue of FIFO (tx_cons), not
 *     the head (tx_prod)
 */
static void sky2_tx_complete(struct sky2_port *sky2, u16 done)
{
	struct net_device *dev = sky2->netdev;
	unsigned idx;

	BUG_ON(done >= sky2->tx_ring_size);

	for (idx = sky2->tx_cons; idx != done;
	     idx = RING_NEXT(idx, sky2->tx_ring_size)) {
		struct tx_ring_info *re = sky2->tx_ring + idx;
		struct sk_buff *skb = re->skb;

		sky2_tx_unmap(sky2->hw->pdev, re);

		if (skb) {
			if (unlikely(netif_msg_tx_done(sky2)))
				printk(KERN_DEBUG "%s: tx done %u\n",
				       dev->name, idx);

			dev->stats.tx_packets++;
			dev->stats.tx_bytes += skb->len;

			dev_kfree_skb_any(skb);

			sky2->tx_next = RING_NEXT(idx, sky2->tx_ring_size);
		}
	}

	sky2->tx_cons = idx;
	smp_mb();

	if (tx_avail(sky2) > MAX_SKB_TX_LE + 4)
		netif_wake_queue(dev);
}

static void sky2_tx_reset(struct sky2_hw *hw, unsigned port)
{
	/* Disable Force Sync bit and Enable Alloc bit */
	sky2_write8(hw, SK_REG(port, TXA_CTRL),
		    TXA_DIS_FSYNC | TXA_DIS_ALLOC | TXA_STOP_RC);

	/* Stop Interval Timer and Limit Counter of Tx Arbiter */
	sky2_write32(hw, SK_REG(port, TXA_ITI_INI), 0L);
	sky2_write32(hw, SK_REG(port, TXA_LIM_INI), 0L);

	/* Reset the PCI FIFO of the async Tx queue */
	sky2_write32(hw, Q_ADDR(txqaddr[port], Q_CSR),
		     BMU_RST_SET | BMU_FIFO_RST);

	/* Reset the Tx prefetch units */
	sky2_write32(hw, Y2_QADDR(txqaddr[port], PREF_UNIT_CTRL),
		     PREF_UNIT_RST_SET);

	sky2_write32(hw, RB_ADDR(txqaddr[port], RB_CTRL), RB_RST_SET);
	sky2_write8(hw, SK_REG(port, TX_GMF_CTRL_T), GMF_RST_SET);
}

/* Network shutdown */
static int sky2_down(struct net_device *dev)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;
	unsigned port = sky2->port;
	u16 ctrl;
	u32 imask;

	/* Never really got started! */
	if (!sky2->tx_le)
		return 0;

	if (netif_msg_ifdown(sky2))
		printk(KERN_INFO PFX "%s: disabling interface\n", dev->name);

	/* Force flow control off */
	sky2_write8(hw, SK_REG(port, GMAC_CTRL), GMC_PAUSE_OFF);

	/* Stop transmitter */
	sky2_write32(hw, Q_ADDR(txqaddr[port], Q_CSR), BMU_STOP);
	sky2_read32(hw, Q_ADDR(txqaddr[port], Q_CSR));

	sky2_write32(hw, RB_ADDR(txqaddr[port], RB_CTRL),
		     RB_RST_SET | RB_DIS_OP_MD);

	ctrl = gma_read16(hw, port, GM_GP_CTRL);
	ctrl &= ~(GM_GPCR_TX_ENA | GM_GPCR_RX_ENA);
	gma_write16(hw, port, GM_GP_CTRL, ctrl);

	sky2_write8(hw, SK_REG(port, GPHY_CTRL), GPC_RST_SET);

	/* Workaround shared GMAC reset */
	if (!(hw->chip_id == CHIP_ID_YUKON_XL && hw->chip_rev == 0
	      && port == 0 && hw->dev[1] && netif_running(hw->dev[1])))
		sky2_write8(hw, SK_REG(port, GMAC_CTRL), GMC_RST_SET);

	sky2_write8(hw, SK_REG(port, RX_GMF_CTRL_T), GMF_RST_SET);

	/* Force any delayed status interrrupt and NAPI */
	sky2_write32(hw, STAT_LEV_TIMER_CNT, 0);
	sky2_write32(hw, STAT_TX_TIMER_CNT, 0);
	sky2_write32(hw, STAT_ISR_TIMER_CNT, 0);
	sky2_read8(hw, STAT_ISR_TIMER_CTRL);

	sky2_rx_stop(sky2);

	/* Disable port IRQ */
	imask = sky2_read32(hw, B0_IMSK);
	imask &= ~portirq_msk[port];
	sky2_write32(hw, B0_IMSK, imask);
	sky2_read32(hw, B0_IMSK);

	synchronize_irq(hw->pdev->irq);
	napi_synchronize(&hw->napi);

	spin_lock_bh(&sky2->phy_lock);
	sky2_phy_power_down(hw, port);
	spin_unlock_bh(&sky2->phy_lock);

	sky2_tx_reset(hw, port);

	/* Free any pending frames stuck in HW queue */
	sky2_tx_complete(sky2, sky2->tx_prod);

	sky2_rx_clean(sky2);

	sky2_free_buffers(sky2);

	return 0;
}

static u16 sky2_phy_speed(const struct sky2_hw *hw, u16 aux)
{
	if (hw->flags & SKY2_HW_FIBRE_PHY)
		return SPEED_1000;

	if (!(hw->flags & SKY2_HW_GIGABIT)) {
		if (aux & PHY_M_PS_SPEED_100)
			return SPEED_100;
		else
			return SPEED_10;
	}

	switch (aux & PHY_M_PS_SPEED_MSK) {
	case PHY_M_PS_SPEED_1000:
		return SPEED_1000;
	case PHY_M_PS_SPEED_100:
		return SPEED_100;
	default:
		return SPEED_10;
	}
}

static void sky2_link_up(struct sky2_port *sky2)
{
	struct sky2_hw *hw = sky2->hw;
	unsigned port = sky2->port;
	u16 reg;
	static const char *fc_name[] = {
		[FC_NONE]	= "none",
		[FC_TX]		= "tx",
		[FC_RX]		= "rx",
		[FC_BOTH]	= "both",
	};

	/* enable Rx/Tx */
	reg = gma_read16(hw, port, GM_GP_CTRL);
	reg |= GM_GPCR_RX_ENA | GM_GPCR_TX_ENA;
	gma_write16(hw, port, GM_GP_CTRL, reg);

	gm_phy_write(hw, port, PHY_MARV_INT_MASK, PHY_M_DEF_MSK);

	netif_carrier_on(sky2->netdev);

	mod_timer(&hw->watchdog_timer, jiffies + 1);

	/* Turn on link LED */
	sky2_write8(hw, SK_REG(port, LNK_LED_REG),
		    LINKLED_ON | LINKLED_BLINK_OFF | LINKLED_LINKSYNC_OFF);

	if (netif_msg_link(sky2))
		printk(KERN_INFO PFX
		       "%s: Link is up at %d Mbps, %s duplex, flow control %s\n",
		       sky2->netdev->name, sky2->speed,
		       sky2->duplex == DUPLEX_FULL ? "full" : "half",
		       fc_name[sky2->flow_status]);
}

static void sky2_link_down(struct sky2_port *sky2)
{
	struct sky2_hw *hw = sky2->hw;
	unsigned port = sky2->port;
	u16 reg;

	gm_phy_write(hw, port, PHY_MARV_INT_MASK, 0);

	reg = gma_read16(hw, port, GM_GP_CTRL);
	reg &= ~(GM_GPCR_RX_ENA | GM_GPCR_TX_ENA);
	gma_write16(hw, port, GM_GP_CTRL, reg);

	netif_carrier_off(sky2->netdev);

	/* Turn on link LED */
	sky2_write8(hw, SK_REG(port, LNK_LED_REG), LINKLED_OFF);

	if (netif_msg_link(sky2))
		printk(KERN_INFO PFX "%s: Link is down.\n", sky2->netdev->name);

	sky2_phy_init(hw, port);
}

static enum flow_control sky2_flow(int rx, int tx)
{
	if (rx)
		return tx ? FC_BOTH : FC_RX;
	else
		return tx ? FC_TX : FC_NONE;
}

static int sky2_autoneg_done(struct sky2_port *sky2, u16 aux)
{
	struct sky2_hw *hw = sky2->hw;
	unsigned port = sky2->port;
	u16 advert, lpa;

	advert = gm_phy_read(hw, port, PHY_MARV_AUNE_ADV);
	lpa = gm_phy_read(hw, port, PHY_MARV_AUNE_LP);
	if (lpa & PHY_M_AN_RF) {
		printk(KERN_ERR PFX "%s: remote fault", sky2->netdev->name);
		return -1;
	}

	if (!(aux & PHY_M_PS_SPDUP_RES)) {
		printk(KERN_ERR PFX "%s: speed/duplex mismatch",
		       sky2->netdev->name);
		return -1;
	}

	sky2->speed = sky2_phy_speed(hw, aux);
	sky2->duplex = (aux & PHY_M_PS_FULL_DUP) ? DUPLEX_FULL : DUPLEX_HALF;

	/* Since the pause result bits seem to in different positions on
	 * different chips. look at registers.
	 */
	if (hw->flags & SKY2_HW_FIBRE_PHY) {
		/* Shift for bits in fiber PHY */
		advert &= ~(ADVERTISE_PAUSE_CAP|ADVERTISE_PAUSE_ASYM);
		lpa &= ~(LPA_PAUSE_CAP|LPA_PAUSE_ASYM);

		if (advert & ADVERTISE_1000XPAUSE)
			advert |= ADVERTISE_PAUSE_CAP;
		if (advert & ADVERTISE_1000XPSE_ASYM)
			advert |= ADVERTISE_PAUSE_ASYM;
		if (lpa & LPA_1000XPAUSE)
			lpa |= LPA_PAUSE_CAP;
		if (lpa & LPA_1000XPAUSE_ASYM)
			lpa |= LPA_PAUSE_ASYM;
	}

	sky2->flow_status = FC_NONE;
	if (advert & ADVERTISE_PAUSE_CAP) {
		if (lpa & LPA_PAUSE_CAP)
			sky2->flow_status = FC_BOTH;
		else if (advert & ADVERTISE_PAUSE_ASYM)
			sky2->flow_status = FC_RX;
	} else if (advert & ADVERTISE_PAUSE_ASYM) {
		if ((lpa & LPA_PAUSE_CAP) && (lpa & LPA_PAUSE_ASYM))
			sky2->flow_status = FC_TX;
	}

	if (sky2->duplex == DUPLEX_HALF && sky2->speed < SPEED_1000
	    && !(hw->chip_id == CHIP_ID_YUKON_EC_U || hw->chip_id == CHIP_ID_YUKON_EX))
		sky2->flow_status = FC_NONE;

	if (sky2->flow_status & FC_TX)
		sky2_write8(hw, SK_REG(port, GMAC_CTRL), GMC_PAUSE_ON);
	else
		sky2_write8(hw, SK_REG(port, GMAC_CTRL), GMC_PAUSE_OFF);

	return 0;
}

/* Interrupt from PHY */
static void sky2_phy_intr(struct sky2_hw *hw, unsigned port)
{
	struct net_device *dev = hw->dev[port];
	struct sky2_port *sky2 = netdev_priv(dev);
	u16 istatus, phystat;

	if (!netif_running(dev))
		return;

	spin_lock(&sky2->phy_lock);
	istatus = gm_phy_read(hw, port, PHY_MARV_INT_STAT);
	phystat = gm_phy_read(hw, port, PHY_MARV_PHY_STAT);

	if (netif_msg_intr(sky2))
		printk(KERN_INFO PFX "%s: phy interrupt status 0x%x 0x%x\n",
		       sky2->netdev->name, istatus, phystat);

	if (istatus & PHY_M_IS_AN_COMPL) {
		if (sky2_autoneg_done(sky2, phystat) == 0)
			sky2_link_up(sky2);
		goto out;
	}

	if (istatus & PHY_M_IS_LSP_CHANGE)
		sky2->speed = sky2_phy_speed(hw, phystat);

	if (istatus & PHY_M_IS_DUP_CHANGE)
		sky2->duplex =
		    (phystat & PHY_M_PS_FULL_DUP) ? DUPLEX_FULL : DUPLEX_HALF;

	if (istatus & PHY_M_IS_LST_CHANGE) {
		if (phystat & PHY_M_PS_LINK_UP)
			sky2_link_up(sky2);
		else
			sky2_link_down(sky2);
	}
out:
	spin_unlock(&sky2->phy_lock);
}

/* Transmit timeout is only called if we are running, carrier is up
 * and tx queue is full (stopped).
 */
static void sky2_tx_timeout(struct net_device *dev)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;

	if (netif_msg_timer(sky2))
		printk(KERN_ERR PFX "%s: tx timeout\n", dev->name);

	printk(KERN_DEBUG PFX "%s: transmit ring %u .. %u report=%u done=%u\n",
	       dev->name, sky2->tx_cons, sky2->tx_prod,
	       sky2_read16(hw, sky2->port == 0 ? STAT_TXA1_RIDX : STAT_TXA2_RIDX),
	       sky2_read16(hw, Q_ADDR(txqaddr[sky2->port], Q_DONE)));

	/* can't restart safely under softirq */
	schedule_work(&hw->restart_work);
}

static int sky2_change_mtu(struct net_device *dev, int new_mtu)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;
	unsigned port = sky2->port;
	int err;
	u16 ctl, mode;
	u32 imask;

	if (new_mtu < ETH_ZLEN || new_mtu > ETH_JUMBO_MTU)
		return -EINVAL;

	if (new_mtu > ETH_DATA_LEN &&
	    (hw->chip_id == CHIP_ID_YUKON_FE ||
	     hw->chip_id == CHIP_ID_YUKON_FE_P))
		return -EINVAL;

	if (!netif_running(dev)) {
		dev->mtu = new_mtu;
		return 0;
	}

	imask = sky2_read32(hw, B0_IMSK);
	sky2_write32(hw, B0_IMSK, 0);

	dev->trans_start = jiffies;	/* prevent tx timeout */
	netif_stop_queue(dev);
	napi_disable(&hw->napi);

	synchronize_irq(hw->pdev->irq);

	if (!(hw->flags & SKY2_HW_RAM_BUFFER))
		sky2_set_tx_stfwd(hw, port);

	ctl = gma_read16(hw, port, GM_GP_CTRL);
	gma_write16(hw, port, GM_GP_CTRL, ctl & ~GM_GPCR_RX_ENA);
	sky2_rx_stop(sky2);
	sky2_rx_clean(sky2);

	dev->mtu = new_mtu;

	mode = DATA_BLIND_VAL(DATA_BLIND_DEF) |
		GM_SMOD_VLAN_ENA | IPG_DATA_VAL(IPG_DATA_DEF);

	if (dev->mtu > ETH_DATA_LEN)
		mode |= GM_SMOD_JUMBO_ENA;

	gma_write16(hw, port, GM_SERIAL_MODE, mode);

	sky2_write8(hw, RB_ADDR(rxqaddr[port], RB_CTRL), RB_ENA_OP_MD);

	err = sky2_rx_start(sky2);
	sky2_write32(hw, B0_IMSK, imask);

	sky2_read32(hw, B0_Y2_SP_LISR);
	napi_enable(&hw->napi);

	if (err)
		dev_close(dev);
	else {
		gma_write16(hw, port, GM_GP_CTRL, ctl);

		netif_wake_queue(dev);
	}

	return err;
}

/* For small just reuse existing skb for next receive */
static struct sk_buff *receive_copy(struct sky2_port *sky2,
				    const struct rx_ring_info *re,
				    unsigned length)
{
	struct sk_buff *skb;

	skb = netdev_alloc_skb(sky2->netdev, length + 2);
	if (likely(skb)) {
		skb_reserve(skb, 2);
		pci_dma_sync_single_for_cpu(sky2->hw->pdev, re->data_addr,
					    length, PCI_DMA_FROMDEVICE);
		skb_copy_from_linear_data(re->skb, skb->data, length);
		skb->ip_summed = re->skb->ip_summed;
		skb->csum = re->skb->csum;
		pci_dma_sync_single_for_device(sky2->hw->pdev, re->data_addr,
					       length, PCI_DMA_FROMDEVICE);
		re->skb->ip_summed = CHECKSUM_NONE;
		skb_put(skb, length);
	}
	return skb;
}

/* Adjust length of skb with fragments to match received data */
static void skb_put_frags(struct sk_buff *skb, unsigned int hdr_space,
			  unsigned int length)
{
	int i, num_frags;
	unsigned int size;

	/* put header into skb */
	size = min(length, hdr_space);
	skb->tail += size;
	skb->len += size;
	length -= size;

	num_frags = skb_shinfo(skb)->nr_frags;
	for (i = 0; i < num_frags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

		if (length == 0) {
			/* don't need this page */
			__free_page(frag->page);
			--skb_shinfo(skb)->nr_frags;
		} else {
			size = min(length, (unsigned) PAGE_SIZE);

			frag->size = size;
			skb->data_len += size;
			skb->truesize += size;
			skb->len += size;
			length -= size;
		}
	}
}

/* Normal packet - take skb from ring element and put in a new one  */
static struct sk_buff *receive_new(struct sky2_port *sky2,
				   struct rx_ring_info *re,
				   unsigned int length)
{
	struct sk_buff *skb, *nskb;
	unsigned hdr_space = sky2->rx_data_size;

	/* Don't be tricky about reusing pages (yet) */
	nskb = sky2_rx_alloc(sky2);
	if (unlikely(!nskb))
		return NULL;

	skb = re->skb;
	sky2_rx_unmap_skb(sky2->hw->pdev, re);

	prefetch(skb->data);
	re->skb = nskb;
	if (sky2_rx_map_skb(sky2->hw->pdev, re, hdr_space)) {
		dev_kfree_skb(nskb);
		re->skb = skb;
		return NULL;
	}

	if (skb_shinfo(skb)->nr_frags)
		skb_put_frags(skb, hdr_space, length);
	else
		skb_put(skb, length);
	return skb;
}

/*
 * Receive one packet.
 * For larger packets, get new buffer.
 */
static struct sk_buff *sky2_receive(struct net_device *dev,
				    u16 length, u32 status)
{
 	struct sky2_port *sky2 = netdev_priv(dev);
	struct rx_ring_info *re = sky2->rx_ring + sky2->rx_next;
	struct sk_buff *skb = NULL;
	u16 count = (status & GMR_FS_LEN) >> 16;

#ifdef SKY2_VLAN_TAG_USED
	/* Account for vlan tag */
	if (sky2->vlgrp && (status & GMR_FS_VLAN))
		count -= VLAN_HLEN;
#endif

	if (unlikely(netif_msg_rx_status(sky2)))
		printk(KERN_DEBUG PFX "%s: rx slot %u status 0x%x len %d\n",
		       dev->name, sky2->rx_next, status, length);

	sky2->rx_next = (sky2->rx_next + 1) % sky2->rx_pending;
	prefetch(sky2->rx_ring + sky2->rx_next);

	/* This chip has hardware problems that generates bogus status.
	 * So do only marginal checking and expect higher level protocols
	 * to handle crap frames.
	 */
	if (sky2->hw->chip_id == CHIP_ID_YUKON_FE_P &&
	    sky2->hw->chip_rev == CHIP_REV_YU_FE2_A0 &&
	    length != count)
		goto okay;

	if (status & GMR_FS_ANY_ERR)
		goto error;

	if (!(status & GMR_FS_RX_OK))
		goto resubmit;

	/* if length reported by DMA does not match PHY, packet was truncated */
	if (length != count)
		goto len_error;

okay:
	if (length < copybreak)
		skb = receive_copy(sky2, re, length);
	else
		skb = receive_new(sky2, re, length);
resubmit:
	sky2_rx_submit(sky2, re);

	return skb;

len_error:
	/* Truncation of overlength packets
	   causes PHY length to not match MAC length */
	++dev->stats.rx_length_errors;
	if (netif_msg_rx_err(sky2) && net_ratelimit())
		pr_info(PFX "%s: rx length error: status %#x length %d\n",
			dev->name, status, length);
	goto resubmit;

error:
	++dev->stats.rx_errors;
	if (status & GMR_FS_RX_FF_OV) {
		dev->stats.rx_over_errors++;
		goto resubmit;
	}

	if (netif_msg_rx_err(sky2) && net_ratelimit())
		printk(KERN_INFO PFX "%s: rx error, status 0x%x length %d\n",
		       dev->name, status, length);

	if (status & (GMR_FS_LONG_ERR | GMR_FS_UN_SIZE))
		dev->stats.rx_length_errors++;
	if (status & GMR_FS_FRAGMENT)
		dev->stats.rx_frame_errors++;
	if (status & GMR_FS_CRC_ERR)
		dev->stats.rx_crc_errors++;

	goto resubmit;
}

/* Transmit complete */
static inline void sky2_tx_done(struct net_device *dev, u16 last)
{
	struct sky2_port *sky2 = netdev_priv(dev);

	if (netif_running(dev))
		sky2_tx_complete(sky2, last);
}

static inline void sky2_skb_rx(const struct sky2_port *sky2,
			       u32 status, struct sk_buff *skb)
{
#ifdef SKY2_VLAN_TAG_USED
	u16 vlan_tag = be16_to_cpu(sky2->rx_tag);
	if (sky2->vlgrp && (status & GMR_FS_VLAN)) {
		if (skb->ip_summed == CHECKSUM_NONE)
			vlan_hwaccel_receive_skb(skb, sky2->vlgrp, vlan_tag);
		else
			vlan_gro_receive(&sky2->hw->napi, sky2->vlgrp,
					 vlan_tag, skb);
		return;
	}
#endif
	if (skb->ip_summed == CHECKSUM_NONE)
		netif_receive_skb(skb);
	else
		napi_gro_receive(&sky2->hw->napi, skb);
}

static inline void sky2_rx_done(struct sky2_hw *hw, unsigned port,
				unsigned packets, unsigned bytes)
{
	if (packets) {
		struct net_device *dev = hw->dev[port];

		dev->stats.rx_packets += packets;
		dev->stats.rx_bytes += bytes;
		dev->last_rx = jiffies;
		sky2_rx_update(netdev_priv(dev), rxqaddr[port]);
	}
}

/* Process status response ring */
static int sky2_status_intr(struct sky2_hw *hw, int to_do, u16 idx)
{
	int work_done = 0;
	unsigned int total_bytes[2] = { 0 };
	unsigned int total_packets[2] = { 0 };

	rmb();
	do {
		struct sky2_port *sky2;
		struct sky2_status_le *le  = hw->st_le + hw->st_idx;
		unsigned port;
		struct net_device *dev;
		struct sk_buff *skb;
		u32 status;
		u16 length;
		u8 opcode = le->opcode;

		if (!(opcode & HW_OWNER))
			break;

		hw->st_idx = RING_NEXT(hw->st_idx, STATUS_RING_SIZE);

		port = le->css & CSS_LINK_BIT;
		dev = hw->dev[port];
		sky2 = netdev_priv(dev);
		length = le16_to_cpu(le->length);
		status = le32_to_cpu(le->status);

		le->opcode = 0;
		switch (opcode & ~HW_OWNER) {
		case OP_RXSTAT:
			total_packets[port]++;
			total_bytes[port] += length;
			skb = sky2_receive(dev, length, status);
			if (unlikely(!skb)) {
				dev->stats.rx_dropped++;
				break;
			}

			/* This chip reports checksum status differently */
			if (hw->flags & SKY2_HW_NEW_LE) {
				if ((sky2->flags & SKY2_FLAG_RX_CHECKSUM) &&
				    (le->css & (CSS_ISIPV4 | CSS_ISIPV6)) &&
				    (le->css & CSS_TCPUDPCSOK))
					skb->ip_summed = CHECKSUM_UNNECESSARY;
				else
					skb->ip_summed = CHECKSUM_NONE;
			}

			skb->protocol = eth_type_trans(skb, dev);

			sky2_skb_rx(sky2, status, skb);

			/* Stop after net poll weight */
			if (++work_done >= to_do)
				goto exit_loop;
			break;

#ifdef SKY2_VLAN_TAG_USED
		case OP_RXVLAN:
			sky2->rx_tag = length;
			break;

		case OP_RXCHKSVLAN:
			sky2->rx_tag = length;
			/* fall through */
#endif
		case OP_RXCHKS:
			if (!(sky2->flags & SKY2_FLAG_RX_CHECKSUM))
				break;

			/* If this happens then driver assuming wrong format */
			if (unlikely(hw->flags & SKY2_HW_NEW_LE)) {
				if (net_ratelimit())
					printk(KERN_NOTICE "%s: unexpected"
					       " checksum status\n",
					       dev->name);
				break;
			}

			/* Both checksum counters are programmed to start at
			 * the same offset, so unless there is a problem they
			 * should match. This failure is an early indication that
			 * hardware receive checksumming won't work.
			 */
			if (likely(status >> 16 == (status & 0xffff))) {
				skb = sky2->rx_ring[sky2->rx_next].skb;
				skb->ip_summed = CHECKSUM_COMPLETE;
				skb->csum = le16_to_cpu(status);
			} else {
				printk(KERN_NOTICE PFX "%s: hardware receive "
				       "checksum problem (status = %#x)\n",
				       dev->name, status);
				sky2->flags &= ~SKY2_FLAG_RX_CHECKSUM;

				sky2_write32(sky2->hw,
					     Q_ADDR(rxqaddr[port], Q_CSR),
					     BMU_DIS_RX_CHKSUM);
			}
			break;

		case OP_TXINDEXLE:
			/* TX index reports status for both ports */
			sky2_tx_done(hw->dev[0], status & 0xfff);
			if (hw->dev[1])
				sky2_tx_done(hw->dev[1],
				     ((status >> 24) & 0xff)
					     | (u16)(length & 0xf) << 8);
			break;

		default:
			if (net_ratelimit())
				printk(KERN_WARNING PFX
				       "unknown status opcode 0x%x\n", opcode);
		}
	} while (hw->st_idx != idx);

	/* Fully processed status ring so clear irq */
	sky2_write32(hw, STAT_CTRL, SC_STAT_CLR_IRQ);

exit_loop:
	sky2_rx_done(hw, 0, total_packets[0], total_bytes[0]);
	sky2_rx_done(hw, 1, total_packets[1], total_bytes[1]);

	return work_done;
}

static void sky2_hw_error(struct sky2_hw *hw, unsigned port, u32 status)
{
	struct net_device *dev = hw->dev[port];

	if (net_ratelimit())
		printk(KERN_INFO PFX "%s: hw error interrupt status 0x%x\n",
		       dev->name, status);

	if (status & Y2_IS_PAR_RD1) {
		if (net_ratelimit())
			printk(KERN_ERR PFX "%s: ram data read parity error\n",
			       dev->name);
		/* Clear IRQ */
		sky2_write16(hw, RAM_BUFFER(port, B3_RI_CTRL), RI_CLR_RD_PERR);
	}

	if (status & Y2_IS_PAR_WR1) {
		if (net_ratelimit())
			printk(KERN_ERR PFX "%s: ram data write parity error\n",
			       dev->name);

		sky2_write16(hw, RAM_BUFFER(port, B3_RI_CTRL), RI_CLR_WR_PERR);
	}

	if (status & Y2_IS_PAR_MAC1) {
		if (net_ratelimit())
			printk(KERN_ERR PFX "%s: MAC parity error\n", dev->name);
		sky2_write8(hw, SK_REG(port, TX_GMF_CTRL_T), GMF_CLI_TX_PE);
	}

	if (status & Y2_IS_PAR_RX1) {
		if (net_ratelimit())
			printk(KERN_ERR PFX "%s: RX parity error\n", dev->name);
		sky2_write32(hw, Q_ADDR(rxqaddr[port], Q_CSR), BMU_CLR_IRQ_PAR);
	}

	if (status & Y2_IS_TCP_TXA1) {
		if (net_ratelimit())
			printk(KERN_ERR PFX "%s: TCP segmentation error\n",
			       dev->name);
		sky2_write32(hw, Q_ADDR(txqaddr[port], Q_CSR), BMU_CLR_IRQ_TCP);
	}
}

static void sky2_hw_intr(struct sky2_hw *hw)
{
	struct pci_dev *pdev = hw->pdev;
	u32 status = sky2_read32(hw, B0_HWE_ISRC);
	u32 hwmsk = sky2_read32(hw, B0_HWE_IMSK);

	status &= hwmsk;

	if (status & Y2_IS_TIST_OV)
		sky2_write8(hw, GMAC_TI_ST_CTRL, GMT_ST_CLR_IRQ);

	if (status & (Y2_IS_MST_ERR | Y2_IS_IRQ_STAT)) {
		u16 pci_err;

		sky2_write8(hw, B2_TST_CTRL1, TST_CFG_WRITE_ON);
		pci_err = sky2_pci_read16(hw, PCI_STATUS);
		if (net_ratelimit())
			dev_err(&pdev->dev, "PCI hardware error (0x%x)\n",
			        pci_err);

		sky2_pci_write16(hw, PCI_STATUS,
				      pci_err | PCI_STATUS_ERROR_BITS);
		sky2_write8(hw, B2_TST_CTRL1, TST_CFG_WRITE_OFF);
	}

	if (status & Y2_IS_PCI_EXP) {
		/* PCI-Express uncorrectable Error occurred */
		u32 err;

		sky2_write8(hw, B2_TST_CTRL1, TST_CFG_WRITE_ON);
		err = sky2_read32(hw, Y2_CFG_AER + PCI_ERR_UNCOR_STATUS);
		sky2_write32(hw, Y2_CFG_AER + PCI_ERR_UNCOR_STATUS,
			     0xfffffffful);
		if (net_ratelimit())
			dev_err(&pdev->dev, "PCI Express error (0x%x)\n", err);

		sky2_read32(hw, Y2_CFG_AER + PCI_ERR_UNCOR_STATUS);
		sky2_write8(hw, B2_TST_CTRL1, TST_CFG_WRITE_OFF);
	}

	if (status & Y2_HWE_L1_MASK)
		sky2_hw_error(hw, 0, status);
	status >>= 8;
	if (status & Y2_HWE_L1_MASK)
		sky2_hw_error(hw, 1, status);
}

static void sky2_mac_intr(struct sky2_hw *hw, unsigned port)
{
	struct net_device *dev = hw->dev[port];
	struct sky2_port *sky2 = netdev_priv(dev);
	u8 status = sky2_read8(hw, SK_REG(port, GMAC_IRQ_SRC));

	if (netif_msg_intr(sky2))
		printk(KERN_INFO PFX "%s: mac interrupt status 0x%x\n",
		       dev->name, status);

	if (status & GM_IS_RX_CO_OV)
		gma_read16(hw, port, GM_RX_IRQ_SRC);

	if (status & GM_IS_TX_CO_OV)
		gma_read16(hw, port, GM_TX_IRQ_SRC);

	if (status & GM_IS_RX_FF_OR) {
		++dev->stats.rx_fifo_errors;
		sky2_write8(hw, SK_REG(port, RX_GMF_CTRL_T), GMF_CLI_RX_FO);
	}

	if (status & GM_IS_TX_FF_UR) {
		++dev->stats.tx_fifo_errors;
		sky2_write8(hw, SK_REG(port, TX_GMF_CTRL_T), GMF_CLI_TX_FU);
	}
}

/* This should never happen it is a bug. */
static void sky2_le_error(struct sky2_hw *hw, unsigned port, u16 q)
{
	struct net_device *dev = hw->dev[port];
	u16 idx = sky2_read16(hw, Y2_QADDR(q, PREF_UNIT_GET_IDX));

	dev_err(&hw->pdev->dev, PFX
		"%s: descriptor error q=%#x get=%u put=%u\n",
		dev->name, (unsigned) q, (unsigned) idx,
		(unsigned) sky2_read16(hw, Y2_QADDR(q, PREF_UNIT_PUT_IDX)));

	sky2_write32(hw, Q_ADDR(q, Q_CSR), BMU_CLR_IRQ_CHK);
}

static int sky2_rx_hung(struct net_device *dev)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;
	unsigned port = sky2->port;
	unsigned rxq = rxqaddr[port];
	u32 mac_rp = sky2_read32(hw, SK_REG(port, RX_GMF_RP));
	u8 mac_lev = sky2_read8(hw, SK_REG(port, RX_GMF_RLEV));
	u8 fifo_rp = sky2_read8(hw, Q_ADDR(rxq, Q_RP));
	u8 fifo_lev = sky2_read8(hw, Q_ADDR(rxq, Q_RL));

	/* If idle and MAC or PCI is stuck */
	if (sky2->check.last == dev->last_rx &&
	    ((mac_rp == sky2->check.mac_rp &&
	      mac_lev != 0 && mac_lev >= sky2->check.mac_lev) ||
	     /* Check if the PCI RX hang */
	     (fifo_rp == sky2->check.fifo_rp &&
	      fifo_lev != 0 && fifo_lev >= sky2->check.fifo_lev))) {
		printk(KERN_DEBUG PFX "%s: hung mac %d:%d fifo %d (%d:%d)\n",
		       dev->name, mac_lev, mac_rp, fifo_lev, fifo_rp,
		       sky2_read8(hw, Q_ADDR(rxq, Q_WP)));
		return 1;
	} else {
		sky2->check.last = dev->last_rx;
		sky2->check.mac_rp = mac_rp;
		sky2->check.mac_lev = mac_lev;
		sky2->check.fifo_rp = fifo_rp;
		sky2->check.fifo_lev = fifo_lev;
		return 0;
	}
}

static void sky2_watchdog(unsigned long arg)
{
	struct sky2_hw *hw = (struct sky2_hw *) arg;

	/* Check for lost IRQ once a second */
	if (sky2_read32(hw, B0_ISRC)) {
		napi_schedule(&hw->napi);
	} else {
		int i, active = 0;

		for (i = 0; i < hw->ports; i++) {
			struct net_device *dev = hw->dev[i];
			if (!netif_running(dev))
				continue;
			++active;

			/* For chips with Rx FIFO, check if stuck */
			if ((hw->flags & SKY2_HW_RAM_BUFFER) &&
			     sky2_rx_hung(dev)) {
				pr_info(PFX "%s: receiver hang detected\n",
					dev->name);
				schedule_work(&hw->restart_work);
				return;
			}
		}

		if (active == 0)
			return;
	}

	mod_timer(&hw->watchdog_timer, round_jiffies(jiffies + HZ));
}

/* Hardware/software error handling */
static void sky2_err_intr(struct sky2_hw *hw, u32 status)
{
	if (net_ratelimit())
		dev_warn(&hw->pdev->dev, "error interrupt status=%#x\n", status);

	if (status & Y2_IS_HW_ERR)
		sky2_hw_intr(hw);

	if (status & Y2_IS_IRQ_MAC1)
		sky2_mac_intr(hw, 0);

	if (status & Y2_IS_IRQ_MAC2)
		sky2_mac_intr(hw, 1);

	if (status & Y2_IS_CHK_RX1)
		sky2_le_error(hw, 0, Q_R1);

	if (status & Y2_IS_CHK_RX2)
		sky2_le_error(hw, 1, Q_R2);

	if (status & Y2_IS_CHK_TXA1)
		sky2_le_error(hw, 0, Q_XA1);

	if (status & Y2_IS_CHK_TXA2)
		sky2_le_error(hw, 1, Q_XA2);
}

static int sky2_poll(struct napi_struct *napi, int work_limit)
{
	struct sky2_hw *hw = container_of(napi, struct sky2_hw, napi);
	u32 status = sky2_read32(hw, B0_Y2_SP_EISR);
	int work_done = 0;
	u16 idx;

	if (unlikely(status & Y2_IS_ERROR))
		sky2_err_intr(hw, status);

	if (status & Y2_IS_IRQ_PHY1)
		sky2_phy_intr(hw, 0);

	if (status & Y2_IS_IRQ_PHY2)
		sky2_phy_intr(hw, 1);

	while ((idx = sky2_read16(hw, STAT_PUT_IDX)) != hw->st_idx) {
		work_done += sky2_status_intr(hw, work_limit - work_done, idx);

		if (work_done >= work_limit)
			goto done;
	}

	napi_complete(napi);
	sky2_read32(hw, B0_Y2_SP_LISR);
done:

	return work_done;
}

static irqreturn_t sky2_intr(int irq, void *dev_id)
{
	struct sky2_hw *hw = dev_id;
	u32 status;

	/* Reading this mask interrupts as side effect */
	status = sky2_read32(hw, B0_Y2_SP_ISRC2);
	if (status == 0 || status == ~0)
		return IRQ_NONE;

	prefetch(&hw->st_le[hw->st_idx]);

	napi_schedule(&hw->napi);

	return IRQ_HANDLED;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void sky2_netpoll(struct net_device *dev)
{
	struct sky2_port *sky2 = netdev_priv(dev);

	napi_schedule(&sky2->hw->napi);
}
#endif

/* Chip internal frequency for clock calculations */
static u32 sky2_mhz(const struct sky2_hw *hw)
{
	switch (hw->chip_id) {
	case CHIP_ID_YUKON_EC:
	case CHIP_ID_YUKON_EC_U:
	case CHIP_ID_YUKON_EX:
	case CHIP_ID_YUKON_SUPR:
	case CHIP_ID_YUKON_UL_2:
		return 125;

	case CHIP_ID_YUKON_FE:
		return 100;

	case CHIP_ID_YUKON_FE_P:
		return 50;

	case CHIP_ID_YUKON_XL:
		return 156;

	default:
		BUG();
	}
}

static inline u32 sky2_us2clk(const struct sky2_hw *hw, u32 us)
{
	return sky2_mhz(hw) * us;
}

static inline u32 sky2_clk2us(const struct sky2_hw *hw, u32 clk)
{
	return clk / sky2_mhz(hw);
}


static int __devinit sky2_init(struct sky2_hw *hw)
{
	u8 t8;

	/* Enable all clocks and check for bad PCI access */
	sky2_pci_write32(hw, PCI_DEV_REG3, 0);

	sky2_write8(hw, B0_CTST, CS_RST_CLR);

	hw->chip_id = sky2_read8(hw, B2_CHIP_ID);
	hw->chip_rev = (sky2_read8(hw, B2_MAC_CFG) & CFG_CHIP_R_MSK) >> 4;

	switch(hw->chip_id) {
	case CHIP_ID_YUKON_XL:
		hw->flags = SKY2_HW_GIGABIT | SKY2_HW_NEWER_PHY;
		break;

	case CHIP_ID_YUKON_EC_U:
		hw->flags = SKY2_HW_GIGABIT
			| SKY2_HW_NEWER_PHY
			| SKY2_HW_ADV_POWER_CTL;
		break;

	case CHIP_ID_YUKON_EX:
		hw->flags = SKY2_HW_GIGABIT
			| SKY2_HW_NEWER_PHY
			| SKY2_HW_NEW_LE
			| SKY2_HW_ADV_POWER_CTL;

		/* New transmit checksum */
		if (hw->chip_rev != CHIP_REV_YU_EX_B0)
			hw->flags |= SKY2_HW_AUTO_TX_SUM;
		break;

	case CHIP_ID_YUKON_EC:
		/* This rev is really old, and requires untested workarounds */
		if (hw->chip_rev == CHIP_REV_YU_EC_A1) {
			dev_err(&hw->pdev->dev, "unsupported revision Yukon-EC rev A1\n");
			return -EOPNOTSUPP;
		}
		hw->flags = SKY2_HW_GIGABIT;
		break;

	case CHIP_ID_YUKON_FE:
		break;

	case CHIP_ID_YUKON_FE_P:
		hw->flags = SKY2_HW_NEWER_PHY
			| SKY2_HW_NEW_LE
			| SKY2_HW_AUTO_TX_SUM
			| SKY2_HW_ADV_POWER_CTL;
		break;

	case CHIP_ID_YUKON_SUPR:
		hw->flags = SKY2_HW_GIGABIT
			| SKY2_HW_NEWER_PHY
			| SKY2_HW_NEW_LE
			| SKY2_HW_AUTO_TX_SUM
			| SKY2_HW_ADV_POWER_CTL;
		break;

	case CHIP_ID_YUKON_UL_2:
		hw->flags = SKY2_HW_GIGABIT
			| SKY2_HW_ADV_POWER_CTL;
		break;

	default:
		dev_err(&hw->pdev->dev, "unsupported chip type 0x%x\n",
			hw->chip_id);
		return -EOPNOTSUPP;
	}

	hw->pmd_type = sky2_read8(hw, B2_PMD_TYP);
	if (hw->pmd_type == 'L' || hw->pmd_type == 'S' || hw->pmd_type == 'P')
		hw->flags |= SKY2_HW_FIBRE_PHY;

	hw->ports = 1;
	t8 = sky2_read8(hw, B2_Y2_HW_RES);
	if ((t8 & CFG_DUAL_MAC_MSK) == CFG_DUAL_MAC_MSK) {
		if (!(sky2_read8(hw, B2_Y2_CLK_GATE) & Y2_STATUS_LNK2_INAC))
			++hw->ports;
	}

	if (sky2_read8(hw, B2_E_0))
		hw->flags |= SKY2_HW_RAM_BUFFER;

	return 0;
}

static void sky2_reset(struct sky2_hw *hw)
{
	struct pci_dev *pdev = hw->pdev;
	u16 status;
	int i, cap;
	u32 hwe_mask = Y2_HWE_ALL_MASK;

	/* disable ASF */
	if (hw->chip_id == CHIP_ID_YUKON_EX) {
		status = sky2_read16(hw, HCU_CCSR);
		status &= ~(HCU_CCSR_AHB_RST | HCU_CCSR_CPU_RST_MODE |
			    HCU_CCSR_UC_STATE_MSK);
		sky2_write16(hw, HCU_CCSR, status);
	} else
		sky2_write8(hw, B28_Y2_ASF_STAT_CMD, Y2_ASF_RESET);
	sky2_write16(hw, B0_CTST, Y2_ASF_DISABLE);

	/* do a SW reset */
	sky2_write8(hw, B0_CTST, CS_RST_SET);
	sky2_write8(hw, B0_CTST, CS_RST_CLR);

	/* allow writes to PCI config */
	sky2_write8(hw, B2_TST_CTRL1, TST_CFG_WRITE_ON);

	/* clear PCI errors, if any */
	status = sky2_pci_read16(hw, PCI_STATUS);
	status |= PCI_STATUS_ERROR_BITS;
	sky2_pci_write16(hw, PCI_STATUS, status);

	sky2_write8(hw, B0_CTST, CS_MRST_CLR);

	cap = pci_find_capability(pdev, PCI_CAP_ID_EXP);
	if (cap) {
		sky2_write32(hw, Y2_CFG_AER + PCI_ERR_UNCOR_STATUS,
			     0xfffffffful);

		/* If error bit is stuck on ignore it */
		if (sky2_read32(hw, B0_HWE_ISRC) & Y2_IS_PCI_EXP)
			dev_info(&pdev->dev, "ignoring stuck error report bit\n");
		else
			hwe_mask |= Y2_IS_PCI_EXP;
	}

	sky2_power_on(hw);
	sky2_write8(hw, B2_TST_CTRL1, TST_CFG_WRITE_OFF);

	for (i = 0; i < hw->ports; i++) {
		sky2_write8(hw, SK_REG(i, GMAC_LINK_CTRL), GMLC_RST_SET);
		sky2_write8(hw, SK_REG(i, GMAC_LINK_CTRL), GMLC_RST_CLR);

		if (hw->chip_id == CHIP_ID_YUKON_EX ||
		    hw->chip_id == CHIP_ID_YUKON_SUPR)
			sky2_write16(hw, SK_REG(i, GMAC_CTRL),
				     GMC_BYP_MACSECRX_ON | GMC_BYP_MACSECTX_ON
				     | GMC_BYP_RETR_ON);
	}

	/* Clear I2C IRQ noise */
	sky2_write32(hw, B2_I2C_IRQ, 1);

	/* turn off hardware timer (unused) */
	sky2_write8(hw, B2_TI_CTRL, TIM_STOP);
	sky2_write8(hw, B2_TI_CTRL, TIM_CLR_IRQ);

	/* Turn off descriptor polling */
	sky2_write32(hw, B28_DPT_CTRL, DPT_STOP);

	/* Turn off receive timestamp */
	sky2_write8(hw, GMAC_TI_ST_CTRL, GMT_ST_STOP);
	sky2_write8(hw, GMAC_TI_ST_CTRL, GMT_ST_CLR_IRQ);

	/* enable the Tx Arbiters */
	for (i = 0; i < hw->ports; i++)
		sky2_write8(hw, SK_REG(i, TXA_CTRL), TXA_ENA_ARB);

	/* Initialize ram interface */
	for (i = 0; i < hw->ports; i++) {
		sky2_write8(hw, RAM_BUFFER(i, B3_RI_CTRL), RI_RST_CLR);

		sky2_write8(hw, RAM_BUFFER(i, B3_RI_WTO_R1), SK_RI_TO_53);
		sky2_write8(hw, RAM_BUFFER(i, B3_RI_WTO_XA1), SK_RI_TO_53);
		sky2_write8(hw, RAM_BUFFER(i, B3_RI_WTO_XS1), SK_RI_TO_53);
		sky2_write8(hw, RAM_BUFFER(i, B3_RI_RTO_R1), SK_RI_TO_53);
		sky2_write8(hw, RAM_BUFFER(i, B3_RI_RTO_XA1), SK_RI_TO_53);
		sky2_write8(hw, RAM_BUFFER(i, B3_RI_RTO_XS1), SK_RI_TO_53);
		sky2_write8(hw, RAM_BUFFER(i, B3_RI_WTO_R2), SK_RI_TO_53);
		sky2_write8(hw, RAM_BUFFER(i, B3_RI_WTO_XA2), SK_RI_TO_53);
		sky2_write8(hw, RAM_BUFFER(i, B3_RI_WTO_XS2), SK_RI_TO_53);
		sky2_write8(hw, RAM_BUFFER(i, B3_RI_RTO_R2), SK_RI_TO_53);
		sky2_write8(hw, RAM_BUFFER(i, B3_RI_RTO_XA2), SK_RI_TO_53);
		sky2_write8(hw, RAM_BUFFER(i, B3_RI_RTO_XS2), SK_RI_TO_53);
	}

	sky2_write32(hw, B0_HWE_IMSK, hwe_mask);

	for (i = 0; i < hw->ports; i++)
		sky2_gmac_reset(hw, i);

	memset(hw->st_le, 0, STATUS_LE_BYTES);
	hw->st_idx = 0;

	sky2_write32(hw, STAT_CTRL, SC_STAT_RST_SET);
	sky2_write32(hw, STAT_CTRL, SC_STAT_RST_CLR);

	sky2_write32(hw, STAT_LIST_ADDR_LO, hw->st_dma);
	sky2_write32(hw, STAT_LIST_ADDR_HI, (u64) hw->st_dma >> 32);

	/* Set the list last index */
	sky2_write16(hw, STAT_LAST_IDX, STATUS_RING_SIZE - 1);

	sky2_write16(hw, STAT_TX_IDX_TH, 10);
	sky2_write8(hw, STAT_FIFO_WM, 16);

	/* set Status-FIFO ISR watermark */
	if (hw->chip_id == CHIP_ID_YUKON_XL && hw->chip_rev == 0)
		sky2_write8(hw, STAT_FIFO_ISR_WM, 4);
	else
		sky2_write8(hw, STAT_FIFO_ISR_WM, 16);

	sky2_write32(hw, STAT_TX_TIMER_INI, sky2_us2clk(hw, 1000));
	sky2_write32(hw, STAT_ISR_TIMER_INI, sky2_us2clk(hw, 20));
	sky2_write32(hw, STAT_LEV_TIMER_INI, sky2_us2clk(hw, 100));

	/* enable status unit */
	sky2_write32(hw, STAT_CTRL, SC_STAT_OP_ON);

	sky2_write8(hw, STAT_TX_TIMER_CTRL, TIM_START);
	sky2_write8(hw, STAT_LEV_TIMER_CTRL, TIM_START);
	sky2_write8(hw, STAT_ISR_TIMER_CTRL, TIM_START);
}

/* Take device down (offline).
 * Equivalent to doing dev_stop() but this does not
 * inform upper layers of the transistion.
 */
static void sky2_detach(struct net_device *dev)
{
	if (netif_running(dev)) {
		netif_device_detach(dev);	/* stop txq */
		sky2_down(dev);
	}
}

/* Bring device back after doing sky2_detach */
static int sky2_reattach(struct net_device *dev)
{
	int err = 0;

	if (netif_running(dev)) {
		err = sky2_up(dev);
		if (err) {
			printk(KERN_INFO PFX "%s: could not restart %d\n",
			       dev->name, err);
			dev_close(dev);
		} else {
			netif_device_attach(dev);
			sky2_set_multicast(dev);
		}
	}

	return err;
}

static void sky2_restart(struct work_struct *work)
{
	struct sky2_hw *hw = container_of(work, struct sky2_hw, restart_work);
	int i;

	rtnl_lock();
	for (i = 0; i < hw->ports; i++)
		sky2_detach(hw->dev[i]);

	napi_disable(&hw->napi);
	sky2_write32(hw, B0_IMSK, 0);
	sky2_reset(hw);
	sky2_write32(hw, B0_IMSK, Y2_IS_BASE);
	napi_enable(&hw->napi);

	for (i = 0; i < hw->ports; i++)
		sky2_reattach(hw->dev[i]);

	rtnl_unlock();
}

static inline u8 sky2_wol_supported(const struct sky2_hw *hw)
{
	return sky2_is_copper(hw) ? (WAKE_PHY | WAKE_MAGIC) : 0;
}

static void sky2_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	const struct sky2_port *sky2 = netdev_priv(dev);

	wol->supported = sky2_wol_supported(sky2->hw);
	wol->wolopts = sky2->wol;
}

static int sky2_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;

	if ((wol->wolopts & ~sky2_wol_supported(sky2->hw))
	    || !device_can_wakeup(&hw->pdev->dev))
		return -EOPNOTSUPP;

	sky2->wol = wol->wolopts;

	if (hw->chip_id == CHIP_ID_YUKON_EC_U ||
	    hw->chip_id == CHIP_ID_YUKON_EX ||
	    hw->chip_id == CHIP_ID_YUKON_FE_P)
		sky2_write32(hw, B0_CTST, sky2->wol
			     ? Y2_HW_WOL_ON : Y2_HW_WOL_OFF);

	device_set_wakeup_enable(&hw->pdev->dev, sky2->wol);

	if (!netif_running(dev))
		sky2_wol_init(sky2);
	return 0;
}

static u32 sky2_supported_modes(const struct sky2_hw *hw)
{
	if (sky2_is_copper(hw)) {
		u32 modes = SUPPORTED_10baseT_Half
			| SUPPORTED_10baseT_Full
			| SUPPORTED_100baseT_Half
			| SUPPORTED_100baseT_Full
			| SUPPORTED_Autoneg | SUPPORTED_TP;

		if (hw->flags & SKY2_HW_GIGABIT)
			modes |= SUPPORTED_1000baseT_Half
				| SUPPORTED_1000baseT_Full;
		return modes;
	} else
		return  SUPPORTED_1000baseT_Half
			| SUPPORTED_1000baseT_Full
			| SUPPORTED_Autoneg
			| SUPPORTED_FIBRE;
}

static int sky2_get_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;

	ecmd->transceiver = XCVR_INTERNAL;
	ecmd->supported = sky2_supported_modes(hw);
	ecmd->phy_address = PHY_ADDR_MARV;
	if (sky2_is_copper(hw)) {
		ecmd->port = PORT_TP;
		ecmd->speed = sky2->speed;
	} else {
		ecmd->speed = SPEED_1000;
		ecmd->port = PORT_FIBRE;
	}

	ecmd->advertising = sky2->advertising;
	ecmd->autoneg = (sky2->flags & SKY2_FLAG_AUTO_SPEED)
		? AUTONEG_ENABLE : AUTONEG_DISABLE;
	ecmd->duplex = sky2->duplex;
	return 0;
}

static int sky2_set_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	const struct sky2_hw *hw = sky2->hw;
	u32 supported = sky2_supported_modes(hw);

	if (ecmd->autoneg == AUTONEG_ENABLE) {
		sky2->flags |= SKY2_FLAG_AUTO_SPEED;
		ecmd->advertising = supported;
		sky2->duplex = -1;
		sky2->speed = -1;
	} else {
		u32 setting;

		switch (ecmd->speed) {
		case SPEED_1000:
			if (ecmd->duplex == DUPLEX_FULL)
				setting = SUPPORTED_1000baseT_Full;
			else if (ecmd->duplex == DUPLEX_HALF)
				setting = SUPPORTED_1000baseT_Half;
			else
				return -EINVAL;
			break;
		case SPEED_100:
			if (ecmd->duplex == DUPLEX_FULL)
				setting = SUPPORTED_100baseT_Full;
			else if (ecmd->duplex == DUPLEX_HALF)
				setting = SUPPORTED_100baseT_Half;
			else
				return -EINVAL;
			break;

		case SPEED_10:
			if (ecmd->duplex == DUPLEX_FULL)
				setting = SUPPORTED_10baseT_Full;
			else if (ecmd->duplex == DUPLEX_HALF)
				setting = SUPPORTED_10baseT_Half;
			else
				return -EINVAL;
			break;
		default:
			return -EINVAL;
		}

		if ((setting & supported) == 0)
			return -EINVAL;

		sky2->speed = ecmd->speed;
		sky2->duplex = ecmd->duplex;
		sky2->flags &= ~SKY2_FLAG_AUTO_SPEED;
	}

	sky2->advertising = ecmd->advertising;

	if (netif_running(dev)) {
		sky2_phy_reinit(sky2);
		sky2_set_multicast(dev);
	}

	return 0;
}

static void sky2_get_drvinfo(struct net_device *dev,
			     struct ethtool_drvinfo *info)
{
	struct sky2_port *sky2 = netdev_priv(dev);

	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	strcpy(info->fw_version, "N/A");
	strcpy(info->bus_info, pci_name(sky2->hw->pdev));
}

static const struct sky2_stat {
	char name[ETH_GSTRING_LEN];
	u16 offset;
} sky2_stats[] = {
	{ "tx_bytes",	   GM_TXO_OK_HI },
	{ "rx_bytes",	   GM_RXO_OK_HI },
	{ "tx_broadcast",  GM_TXF_BC_OK },
	{ "rx_broadcast",  GM_RXF_BC_OK },
	{ "tx_multicast",  GM_TXF_MC_OK },
	{ "rx_multicast",  GM_RXF_MC_OK },
	{ "tx_unicast",    GM_TXF_UC_OK },
	{ "rx_unicast",    GM_RXF_UC_OK },
	{ "tx_mac_pause",  GM_TXF_MPAUSE },
	{ "rx_mac_pause",  GM_RXF_MPAUSE },
	{ "collisions",    GM_TXF_COL },
	{ "late_collision",GM_TXF_LAT_COL },
	{ "aborted", 	   GM_TXF_ABO_COL },
	{ "single_collisions", GM_TXF_SNG_COL },
	{ "multi_collisions", GM_TXF_MUL_COL },

	{ "rx_short",      GM_RXF_SHT },
	{ "rx_runt", 	   GM_RXE_FRAG },
	{ "rx_64_byte_packets", GM_RXF_64B },
	{ "rx_65_to_127_byte_packets", GM_RXF_127B },
	{ "rx_128_to_255_byte_packets", GM_RXF_255B },
	{ "rx_256_to_511_byte_packets", GM_RXF_511B },
	{ "rx_512_to_1023_byte_packets", GM_RXF_1023B },
	{ "rx_1024_to_1518_byte_packets", GM_RXF_1518B },
	{ "rx_1518_to_max_byte_packets", GM_RXF_MAX_SZ },
	{ "rx_too_long",   GM_RXF_LNG_ERR },
	{ "rx_fifo_overflow", GM_RXE_FIFO_OV },
	{ "rx_jabber",     GM_RXF_JAB_PKT },
	{ "rx_fcs_error",   GM_RXF_FCS_ERR },

	{ "tx_64_byte_packets", GM_TXF_64B },
	{ "tx_65_to_127_byte_packets", GM_TXF_127B },
	{ "tx_128_to_255_byte_packets", GM_TXF_255B },
	{ "tx_256_to_511_byte_packets", GM_TXF_511B },
	{ "tx_512_to_1023_byte_packets", GM_TXF_1023B },
	{ "tx_1024_to_1518_byte_packets", GM_TXF_1518B },
	{ "tx_1519_to_max_byte_packets", GM_TXF_MAX_SZ },
	{ "tx_fifo_underrun", GM_TXE_FIFO_UR },
};

static u32 sky2_get_rx_csum(struct net_device *dev)
{
	struct sky2_port *sky2 = netdev_priv(dev);

	return !!(sky2->flags & SKY2_FLAG_RX_CHECKSUM);
}

static int sky2_set_rx_csum(struct net_device *dev, u32 data)
{
	struct sky2_port *sky2 = netdev_priv(dev);

	if (data)
		sky2->flags |= SKY2_FLAG_RX_CHECKSUM;
	else
		sky2->flags &= ~SKY2_FLAG_RX_CHECKSUM;

	sky2_write32(sky2->hw, Q_ADDR(rxqaddr[sky2->port], Q_CSR),
		     data ? BMU_ENA_RX_CHKSUM : BMU_DIS_RX_CHKSUM);

	return 0;
}

static u32 sky2_get_msglevel(struct net_device *netdev)
{
	struct sky2_port *sky2 = netdev_priv(netdev);
	return sky2->msg_enable;
}

static int sky2_nway_reset(struct net_device *dev)
{
	struct sky2_port *sky2 = netdev_priv(dev);

	if (!netif_running(dev) || !(sky2->flags & SKY2_FLAG_AUTO_SPEED))
		return -EINVAL;

	sky2_phy_reinit(sky2);
	sky2_set_multicast(dev);

	return 0;
}

static void sky2_phy_stats(struct sky2_port *sky2, u64 * data, unsigned count)
{
	struct sky2_hw *hw = sky2->hw;
	unsigned port = sky2->port;
	int i;

	data[0] = (u64) gma_read32(hw, port, GM_TXO_OK_HI) << 32
	    | (u64) gma_read32(hw, port, GM_TXO_OK_LO);
	data[1] = (u64) gma_read32(hw, port, GM_RXO_OK_HI) << 32
	    | (u64) gma_read32(hw, port, GM_RXO_OK_LO);

	for (i = 2; i < count; i++)
		data[i] = (u64) gma_read32(hw, port, sky2_stats[i].offset);
}

static void sky2_set_msglevel(struct net_device *netdev, u32 value)
{
	struct sky2_port *sky2 = netdev_priv(netdev);
	sky2->msg_enable = value;
}

static int sky2_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return ARRAY_SIZE(sky2_stats);
	default:
		return -EOPNOTSUPP;
	}
}

static void sky2_get_ethtool_stats(struct net_device *dev,
				   struct ethtool_stats *stats, u64 * data)
{
	struct sky2_port *sky2 = netdev_priv(dev);

	sky2_phy_stats(sky2, data, ARRAY_SIZE(sky2_stats));
}

static void sky2_get_strings(struct net_device *dev, u32 stringset, u8 * data)
{
	int i;

	switch (stringset) {
	case ETH_SS_STATS:
		for (i = 0; i < ARRAY_SIZE(sky2_stats); i++)
			memcpy(data + i * ETH_GSTRING_LEN,
			       sky2_stats[i].name, ETH_GSTRING_LEN);
		break;
	}
}

static int sky2_set_mac_address(struct net_device *dev, void *p)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;
	unsigned port = sky2->port;
	const struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, addr->sa_data, ETH_ALEN);
	memcpy_toio(hw->regs + B2_MAC_1 + port * 8,
		    dev->dev_addr, ETH_ALEN);
	memcpy_toio(hw->regs + B2_MAC_2 + port * 8,
		    dev->dev_addr, ETH_ALEN);

	/* virtual address for data */
	gma_set_addr(hw, port, GM_SRC_ADDR_2L, dev->dev_addr);

	/* physical address: used for pause frames */
	gma_set_addr(hw, port, GM_SRC_ADDR_1L, dev->dev_addr);

	return 0;
}

static void inline sky2_add_filter(u8 filter[8], const u8 *addr)
{
	u32 bit;

	bit = ether_crc(ETH_ALEN, addr) & 63;
	filter[bit >> 3] |= 1 << (bit & 7);
}

static void sky2_set_multicast(struct net_device *dev)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;
	unsigned port = sky2->port;
	struct dev_mc_list *list = dev->mc_list;
	u16 reg;
	u8 filter[8];
	int rx_pause;
	static const u8 pause_mc_addr[ETH_ALEN] = { 0x1, 0x80, 0xc2, 0x0, 0x0, 0x1 };

	rx_pause = (sky2->flow_status == FC_RX || sky2->flow_status == FC_BOTH);
	memset(filter, 0, sizeof(filter));

	reg = gma_read16(hw, port, GM_RX_CTRL);
	reg |= GM_RXCR_UCF_ENA;

	if (dev->flags & IFF_PROMISC)	/* promiscuous */
		reg &= ~(GM_RXCR_UCF_ENA | GM_RXCR_MCF_ENA);
	else if (dev->flags & IFF_ALLMULTI)
		memset(filter, 0xff, sizeof(filter));
	else if (dev->mc_count == 0 && !rx_pause)
		reg &= ~GM_RXCR_MCF_ENA;
	else {
		int i;
		reg |= GM_RXCR_MCF_ENA;

		if (rx_pause)
			sky2_add_filter(filter, pause_mc_addr);

		for (i = 0; list && i < dev->mc_count; i++, list = list->next)
			sky2_add_filter(filter, list->dmi_addr);
	}

	gma_write16(hw, port, GM_MC_ADDR_H1,
		    (u16) filter[0] | ((u16) filter[1] << 8));
	gma_write16(hw, port, GM_MC_ADDR_H2,
		    (u16) filter[2] | ((u16) filter[3] << 8));
	gma_write16(hw, port, GM_MC_ADDR_H3,
		    (u16) filter[4] | ((u16) filter[5] << 8));
	gma_write16(hw, port, GM_MC_ADDR_H4,
		    (u16) filter[6] | ((u16) filter[7] << 8));

	gma_write16(hw, port, GM_RX_CTRL, reg);
}

/* Can have one global because blinking is controlled by
 * ethtool and that is always under RTNL mutex
 */
static void sky2_led(struct sky2_port *sky2, enum led_mode mode)
{
	struct sky2_hw *hw = sky2->hw;
	unsigned port = sky2->port;

	spin_lock_bh(&sky2->phy_lock);
	if (hw->chip_id == CHIP_ID_YUKON_EC_U ||
	    hw->chip_id == CHIP_ID_YUKON_EX ||
	    hw->chip_id == CHIP_ID_YUKON_SUPR) {
		u16 pg;
		pg = gm_phy_read(hw, port, PHY_MARV_EXT_ADR);
		gm_phy_write(hw, port, PHY_MARV_EXT_ADR, 3);

		switch (mode) {
		case MO_LED_OFF:
			gm_phy_write(hw, port, PHY_MARV_PHY_CTRL,
				     PHY_M_LEDC_LOS_CTRL(8) |
				     PHY_M_LEDC_INIT_CTRL(8) |
				     PHY_M_LEDC_STA1_CTRL(8) |
				     PHY_M_LEDC_STA0_CTRL(8));
			break;
		case MO_LED_ON:
			gm_phy_write(hw, port, PHY_MARV_PHY_CTRL,
				     PHY_M_LEDC_LOS_CTRL(9) |
				     PHY_M_LEDC_INIT_CTRL(9) |
				     PHY_M_LEDC_STA1_CTRL(9) |
				     PHY_M_LEDC_STA0_CTRL(9));
			break;
		case MO_LED_BLINK:
			gm_phy_write(hw, port, PHY_MARV_PHY_CTRL,
				     PHY_M_LEDC_LOS_CTRL(0xa) |
				     PHY_M_LEDC_INIT_CTRL(0xa) |
				     PHY_M_LEDC_STA1_CTRL(0xa) |
				     PHY_M_LEDC_STA0_CTRL(0xa));
			break;
		case MO_LED_NORM:
			gm_phy_write(hw, port, PHY_MARV_PHY_CTRL,
				     PHY_M_LEDC_LOS_CTRL(1) |
				     PHY_M_LEDC_INIT_CTRL(8) |
				     PHY_M_LEDC_STA1_CTRL(7) |
				     PHY_M_LEDC_STA0_CTRL(7));
		}

		gm_phy_write(hw, port, PHY_MARV_EXT_ADR, pg);
	} else
		gm_phy_write(hw, port, PHY_MARV_LED_OVER,
				     PHY_M_LED_MO_DUP(mode) |
				     PHY_M_LED_MO_10(mode) |
				     PHY_M_LED_MO_100(mode) |
				     PHY_M_LED_MO_1000(mode) |
				     PHY_M_LED_MO_RX(mode) |
				     PHY_M_LED_MO_TX(mode));

	spin_unlock_bh(&sky2->phy_lock);
}

/* blink LED's for finding board */
static int sky2_phys_id(struct net_device *dev, u32 data)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	unsigned int i;

	if (data == 0)
		data = UINT_MAX;

	for (i = 0; i < data; i++) {
		sky2_led(sky2, MO_LED_ON);
		if (msleep_interruptible(500))
			break;
		sky2_led(sky2, MO_LED_OFF);
		if (msleep_interruptible(500))
			break;
	}
	sky2_led(sky2, MO_LED_NORM);

	return 0;
}

static void sky2_get_pauseparam(struct net_device *dev,
				struct ethtool_pauseparam *ecmd)
{
	struct sky2_port *sky2 = netdev_priv(dev);

	switch (sky2->flow_mode) {
	case FC_NONE:
		ecmd->tx_pause = ecmd->rx_pause = 0;
		break;
	case FC_TX:
		ecmd->tx_pause = 1, ecmd->rx_pause = 0;
		break;
	case FC_RX:
		ecmd->tx_pause = 0, ecmd->rx_pause = 1;
		break;
	case FC_BOTH:
		ecmd->tx_pause = ecmd->rx_pause = 1;
	}

	ecmd->autoneg = (sky2->flags & SKY2_FLAG_AUTO_PAUSE)
		? AUTONEG_ENABLE : AUTONEG_DISABLE;
}

static int sky2_set_pauseparam(struct net_device *dev,
			       struct ethtool_pauseparam *ecmd)
{
	struct sky2_port *sky2 = netdev_priv(dev);

	if (ecmd->autoneg == AUTONEG_ENABLE)
		sky2->flags |= SKY2_FLAG_AUTO_PAUSE;
	else
		sky2->flags &= ~SKY2_FLAG_AUTO_PAUSE;

	sky2->flow_mode = sky2_flow(ecmd->rx_pause, ecmd->tx_pause);

	if (netif_running(dev))
		sky2_phy_reinit(sky2);

	return 0;
}

static int sky2_get_coalesce(struct net_device *dev,
			     struct ethtool_coalesce *ecmd)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;

	if (sky2_read8(hw, STAT_TX_TIMER_CTRL) == TIM_STOP)
		ecmd->tx_coalesce_usecs = 0;
	else {
		u32 clks = sky2_read32(hw, STAT_TX_TIMER_INI);
		ecmd->tx_coalesce_usecs = sky2_clk2us(hw, clks);
	}
	ecmd->tx_max_coalesced_frames = sky2_read16(hw, STAT_TX_IDX_TH);

	if (sky2_read8(hw, STAT_LEV_TIMER_CTRL) == TIM_STOP)
		ecmd->rx_coalesce_usecs = 0;
	else {
		u32 clks = sky2_read32(hw, STAT_LEV_TIMER_INI);
		ecmd->rx_coalesce_usecs = sky2_clk2us(hw, clks);
	}
	ecmd->rx_max_coalesced_frames = sky2_read8(hw, STAT_FIFO_WM);

	if (sky2_read8(hw, STAT_ISR_TIMER_CTRL) == TIM_STOP)
		ecmd->rx_coalesce_usecs_irq = 0;
	else {
		u32 clks = sky2_read32(hw, STAT_ISR_TIMER_INI);
		ecmd->rx_coalesce_usecs_irq = sky2_clk2us(hw, clks);
	}

	ecmd->rx_max_coalesced_frames_irq = sky2_read8(hw, STAT_FIFO_ISR_WM);

	return 0;
}

/* Note: this affect both ports */
static int sky2_set_coalesce(struct net_device *dev,
			     struct ethtool_coalesce *ecmd)
{
	struct sky2_port *sky2 = netdev_priv(dev);
	struct sky2_hw *hw = sky2->hw;
	const u32 tmax = sky2_clk2us(hw, 0x0ffffff);

	if (ecmd->tx_coalesce_usecs > tmax ||
	    ecmd->rx_coalesce_usecs > tmax ||
	    ecmd->rx_coalesce_usecs_irq > tmax)
		return -EINVAL;

	if (ecmd->tx_max_coalesced_frames >= sky2->tx_ring_size-1)
		return -EINVAL;
	if (ecmd->rx_max_coalesced_frames > RX_MAX_PENDING)
		return -EINVAL;
	if (ecmd->rx_max_coalesced_frames_irq >RX_MAX_PENDING)
		return -EINVAL;

	if (ecmd->tx_coalesce_usecs == 0)
		sky2_write8(hw, STAT_TX_TIMER_CTRL, TIM_STOP);
	else {
		sky2_write32(hw, STAT_TX_TIMER_INI,
			     sky2_us2clk(hw, ecmd->tx_coalesce_usecs));
		sky2_write8(hw, STAT_TX_TIMER_CTRL, TIM_START);
	}
	sky2_write16(hw, STAT_TX_IDX_TH, ecmd->tx_max_coalesced_frames);

	if (ecmd->rx_coalesce_usecs == 0)
		sky2_write8(hw, STAT_LEV_TIMER_CTRL, TIM_STOP);
	else {
		sky2_write32(hw, STAT_LEV_TIMER_INI,
			     sky2_us2clk(hw, ecmd->rx_coalesce_usecs));
		sky2_write8(hw, STAT_LEV_TIMER_CTRL, TIM_START);
	}
	sky2_write8(hw, STAT_FIFO_WM, ecmd->rx_max_coalesced_frames);

	if (ecmd->rx_coalesce_usecs_irq == 0)
		sky2_write8(hw, STAT_ISR_TIMER_CTRL, TIM_STOP);
	else {
		sky2_write32(hw, STAT_ISR_TIMER_INI,
			     sky2_us2clk(hw, ecmd->rx_coalesce_usecs_irq));
		sky2_write8(hw, STAT_ISR_TIMER_CTRL, TIM_START);
	}
	sky2_write8(hw, STAT_FIFO_ISR_WM, ecmd->rx_max_coalesced_frames_irq);
	return 0;
}

static void sky2_get_ringparam(struct net_device *dev,
			       struct ethtool_ringparam *ering)
{
	struct sky2_port *sky2 = netdev_priv(dev);

	ering->rx_max_pending = RX_MAX_PENDING;
	ering->rx_mini_max_pending = 0;
	ering->rx_jumbo_max_pending = 0;
	ering->tx_max_pending = TX_MAX_PENDING;

	ering->rx_pending = sky2->rx_pending;
	ering->rx_mini_pending = 0;
	ering->rx_jumbo_pending = 0;
	ering->tx_pending = sky2->tx_pending;
}

static int sky2_set_ringparam(struct net_device *dev,
			      struct ethtool_ringparam *ering)
{
	struct sky2_port *sky2 = netdev_priv(dev);

	if (ering->rx_pending > RX_MAX_PENDING ||
	    ering->rx_pending < 8 ||
	    ering->tx_pending < TX_MIN_PENDING ||
	    ering->tx_pending > TX_MAX_PENDING)
		return -EINVAL;

	sky2_detach(dev);

	sky2->rx_pending = ering->rx_pending;
	sky2->tx_pending = ering->tx_pending;
	sky2->tx_ring_size = roundup_pow_of_two(sky2->tx_pending+1);

	return sky2_reattach(dev);
}

static int sky2_get_regs_len(struct net_device *dev)
{
	return 0x4000;
}

/*
 * Returns copy of control register region
 * Note: ethtool_get_regs always provides full size (16k) buffer
 */
static void sky2_get_regs(struct net_device *dev, struct ethtool_regs *regs,
			  void *p)
{
	const struct sky2_port *sky2 = netdev_priv(dev);
	const void __iomem *io = sky2->hw->regs;
	unsigned int b;

	regs->version = 1;

	for (b = 0; b < 128; b++) {
		/* This complicated switch statement is to make sure and
		 * only access regions that are unreserved.
		 * Some blocks are only valid on dual port cards.
		 * and block 3 has some special diagnostic registers that
		 * are poison.chips/
		switch (b) {
		case 3:
			/* skipriver for Maramarvelon earli	memcpy_fromio(p + 0x10, io all the 128 -ll th);ly dbreak;

ver.
dual port cards only earli skge5:ver.
Tx Arbiter 2d link mana9: er.
RXthose should14 ... 15:ent b at higher lev7: er lev9: doneam Buff * those should22ls.
 23HemmiTxinger <shemminger@osdl.or5be donex MAC Fifo 1nger@osdl.or7be donTredistributthose should31 * CoGPHYthe terms of 40ls.
 47HemmiPatterningerthose should52 Stephe54 This CP Segmentattionat higher lev1g>
 *
116HemmiGdistthose s	if (sky2->hw->nk fs == 1)is d	goto reservedginal/* fall throughd link mana0ement Controlnger@osdl.orement Mac addressal Public Liement because
 * e it and/or 7ement PCI exp * MEregd link mana8ement RXe it and/or nse.
 *
* This License for mo6 Stephen8:can renger <shemme it and/or mcense 21 This program is fre it and/or m4ou can redistribute it and/or m6dify
 * it under te it and/or m8ublic  HemmiDescriptor and status unite terms of t0e GNU Gene1 terms of tg>
 *
3 Hemmiished by
 * e it and/or 48are Foun0ation; either version e it and/or 56ls.
 6inux/cSE. spaceGNU General cense 84e GNU diste it andoes not supporte fe * ofiginal driver		default:
 be usefdrivememsetl.h>
 * ofigina}r sup +=* ofci.hfeate <linux}
}

/* In order to do Jumbo packetver  these chips, need
#inturn offh>
#
 * transmit store/forward. Thereforncluecksumue.hload won't work.
 ear, USic int no_tx_ch.h>
#(struct net_devi#incdev)
{
	consux/ii.h>
ibut_nk fa*ibut =

#idev_priv(sm/i;.h>

#if defined(CONhw *hw =ed(COed ier srekqueudev->mtu > ETH_DATA_LEN &&  in lude_idope CHIP_ID_YUKON_EC_U;de <bugfs.h>
#id(CONsetludecsum/mii.h>

#include <asm/, u32 datairq.histr Yukky2.nclude <linux/sm/ihat TAG_USE-EINVALLAN_TAG_USEethtool_op25"
#define P/*
 * Yukoine DDRV_VERSION		"1.25"
#dsoPFX			DRV_NAME " "

/*
 * The Yukon II chipset takes 64 bit command blocks (called list elements)
 * that are orgso into three (recRV_VERSION		"1.2get_eeprom_len/mii.h>

#include <asm/irq.h defined(CONFIG_VLAN_8021Q) || defined(CONFI_8021Q_MODULE)
#define SKY2_VLA	u16
 * 2LAN_TAg8021d(CONFci_read16(hw,ping_DEV_REG2ONFITAG_USE1 << ( (( a si& + CKVPD_ROM_SZ) >> 14) + /ip.e))
#define RX_MAX_vpd_wait(G_VLAN_8021Q_MODULE)
#de,h>
#icap, elembusyirq.hunsigned long9, Urt = jiffieser swhile* DMle skb:
   VLAN:GSO cat alfine MAXADDR)#define MAXne S_F)ope NG		(, andthouan take up
#in10.6 ms for writincludeistrtime_after(X_MAX_P,#define+ HZ/4)orts *	|| derr(& in tD 1
#/*
 *PFX "VPD cycle TUS_d out"iginalks (calleTIMEDOUTp.h>
#		mdelay(1ONFI
#inTAG_USE0zeof(dma_addr_t)/sizeof(u  VL/mii.h>
S+1))
#define TX_MIN_Pvoid * Yuk,tus_PENDIoffset, size_t lengthon II nt rc =ROM_DING		409=
     > 0orts * Thevaler sule skb:
 _LE_BF_PENDING		127

#define S,u32 defigina_DRV /sizeof(u32))*ENDING	, riginaistrrchat i driver suvalingle skb:
   VL32PENDING		127

#deficlud)er suoes noipset, &val, min(lt_mof(val), =
    Nigina32 defp.h>RM_DESCu3ata +	pset one,...,16=all)");PROBE |-ne,...,16=all)")e SKY2_EEPRrczeof(dma_addr_t)/sizeof(u3LE_BRING_NEXT(x,s)	(((x)+1) & ((s)>

#if-1))

static connst u32 defauSKB_TX_LE_MSG=
    NETIF = 0;
module_i;IF_MSG_DRV | NETATUS(iRV |  i <_param(_msione,...,16=allTIF_MSG_LINK
 = *6=al *)ipset + iram(de NETIF_MSG_TIMfaults above */
module_pa,INK
igina_DEVICE_TABLE(sky2_id_table) = {
	{ | NETIF_MSG |US_RING_SIZE	204er suRR
    | NETIF_MSG_IFUP | NE{ PCI_DEVICE(PCI_MSG_IFDOWN;

static IES	param(copybreak, int, 0);
MODUPENDING		(PFX			DRV_NAME " "

/*
 * define
 * thatING		( *ING		(tic int u8

statX_DEF_PENDING		RX_MAX_PENDING

/* This is the wor TX_MIN = b:
 find_capabilityributed in tCHDOG	CI_CAME		"VPDCI_VEistr!capblocks (called list elING		(->magiDRV SKY2_EEP_SKBMAGICLAN_TAG_USEbb


#define RI SKY2_VUP | NEt, 0);ELL, 0x432 defauELL, 0x4lenizeof(dma_addr_t)/size5"
#/
	{ PCI_DEVICE(PCI_VENDOR_ID_DLINK, 0x4001) }, 	/* DGE-550SX */
	{ PCI_DEVICE(PCI_VENDOR_ID_DLINK, 0x4B02) },	/* DGE-560SX */
	{ PCI_DEVICE(PCI_VENDOR_ID_DLINK, 0x4B03) },	/* DGE-550T */
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVistrELL, 0x4340) }!, /* 88E8021 */
	{ blocks (called list elnel.hrtw drARM_Ds not supnk fed earlistrCI_VENDOR9Sxx */& 3) ||PCI_VENDORlenPCI_Dblocks (called list elements)
MODULE_PARM_DESARVELL, 0x4341) }, /* 88E8022 */
	{ PCI_DEVICE(PCI_VE)
#defin>

#if define
 * that as5 */
	_ID_MARVELL,=, an.PENDsettings	    | N*/
	{ PCI_DE,
	.5"
#{ PCI_DEVICE(PCILL, 0x4353) ARVEPENDdrvinfoVICE(PCI_VENCE(PCI_PCI_DEVIwolVICE(PCI_VENwolARVELL, }, /* 88E80I_DEVICPCI_DEVImsgleve /* 88E8040 *, 0x4355ARVELL, , 0x4355) }, /* CI_DEVICE(PCARVEnway_ be tVICE(PCI356) }, /*PCI_DEVIregsRX_LVICE(PCI_VENPCI_VEND_DEVICE(PCI_OR_ID_MARVELL, 0PCI_DEVIlink	=)
 * that arE(PCI_VEPCI_DEVIING		(RX_LVICE(PCI_VENING		(RX_LA) }, /* 88E80/
	{ PCI_DEVICE(PCARVELL, MARVELL, 0x43652 */
	{ PARVELL, 0g NDOR_ID_MARVELD_MARVARVELL, define CI_DEVICE(PCdefine E8050 */
s_VENDOR_IARVELL,_DEVICE(P	{ PCI_DEVICE(053 */
	{ PARVELL, */
	{ PCI_DEVIC_MARVELL, 0PCI_DEVIstrI_DEVICE(PCI_VENDCI_DEVPCI_DEVIcoalesceVICE(PCI_VEN0x4364) ARVELL, 0x4364) }, /* 88DEVICE(PCI_V_DEVICE(PingparaLL, 0x4360) } /* 88E80R_ID_MARV/* 88E8070 */
	{ENDOR_ID_MARVPCI_DEVIpause88E80
    | N6 */
	{ PCI_DEARVELL, 
	{ PCI_DEVICE(PCI, 0x4367) }, /ARVEphys_idVICE(PCI(PCI_VE5 */
	{ PDEVICEunne TE(PCI_VEND }, /* 88A) }, /*  * that, US88E8GE-560T */OR_ID_MARVEL,
};

#ifdef CONFIG_/* 88DEBUG PCI_DEVI definedentryVLAN_8_debug;
 <lincluRead02139parseh>
# first_VENt of Vital Prod.h>
Dataux/de#define  MAXSIZE	128 PCI_DEVICE(P/
	{ 	0x82 PCI_DEVICE(PCI_VENDORof(utag, anchar tag[2]NFIGVEND*label;
}I_DEVICEs[]8E8038{ "PN",	"(PCI Number" },/
	{ EC", "EngineeR_ID L4355NDOR_ID_MMNVELLManufacturVENDOR_ID_MS	{ PCSerw drPCI_VENDOR_ID_MYAVELLAPCI_ TagNDOR_ID_MVLVELLFARVELError Log Message};

MODULEFMARVELcondBLE(pci, sky2_id_table);

/*BVELLBoot Agent ROMout figurrsion};

MODULEEVELL,FI UNDId txqaddr[] = { Q_X
	{ bugfs.h-1))
8EC032howeof(RING_NEXTeq_f		40*seq_DLINK, 0MODULE)
#deX_DEF_t_msg of(uY2_I;
	loff_0x43fsst e8_parst eKB_TX_LEVELL, bufst elements for a single skb:
   VLAN:GSO + CKSUM + Data +ORT_2 }; =rags * DMA */
#define MAX_SKB_TX_LE	(2 + (size
	t u3printf(irq_m"%s /* 88E8071 *\n",DEVICname( 0x4B03)ug lebuf = kmalloc(ORT_2 };, GFP_KERNELSX */PCI_buforts *, u16uts u16 vano memory!\nWEIGHTTAG_USmodule_M_SMb:
   VL
stat 0x4B03) }0,PORT_2 };, _CTRL<NETIF_MS    GM_SMI_CT_P* HZ  VL failedADDR_MAR willoutM_SMI_CT_REbuf[0]ARVEELL, 0x43++) {
		u16 reg, u16 va* HZICE(mismatch: %#xgma_wl == 0 GM_SMI_CTRL);
		i __r /* 2f[1_MARM_SM}

	d= 0EVIC}

	>PORT_2 }; - 4o io_error;

		if (!(ctInvalid id_param(: %ort,ug, " GM_SMI_CTRL);
		if , u16 reg, u16 val.*sUT;

io_	retu + 3/
	{oid  =s: ph+ 3 NETIF_MSG_[port< write timeout\n", nt, 0);	{ PCI_!oes mp("RW		retu +void , 2))VICEend markernclude nux/pci.h}

	dev_wa[port+ D_MARhw->p6(hw, p>name); >=PORT_2 };WN;

static intSC(disable_msi, ARRAYPCI_V
	gma 88E); i++status_uct sky2_hw , /* 88E8i].tag unsigned port, utatus_->pdev->dev, "%s:  %s: phy I/O trl =
	{ 0; i < PHY_R0x436 error\n", hw6(hw, p->dev{
	int i;

	IES		ES		6(hw, ]->name);
		}
out:
	kfreerl =izeof(dma_addr_t)/size58 */R2 };tic const u32 portirq_m-1))

RX_DEF_PENDIN
#include <asm/L, 0eq->efinat;

s>

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIst case number of transmit list eKB_TX_LEnk faine SKY2nk f: phy I/O eridx, lasportION		opport, Q_R2 };
statirq_mhw port, u16 reg, u16 va\nIRQ src=%x maskd poct even=%)
			chip  { Y2_* defaults B0_ISRC);
	__gm_phy_read(hw, port, MSK, &v);
	return v;
}


statiY2_SP_ICRug l GM_SMInetif_runningmand b io_error;

		if (!(ctnet <li_MARVer to VADDR_MARV) | GROM_dule_napi_disableefine AUX_/
	{urn L, 0x436  VLAN:GSO STAT_PUT_IDX{
	/* swi in st_idx->deurn hat     GM_SMI_CT_PS USA.
0) },(empty)ADDR_MAelse+) {
		u16 ctrl = gmY2_CLK_CTRLADDR_MARSC(dision,.h"

ivisio;;
	rARVEVAUX_&&/* en<

	/*US_RINGPCI_V	if  e8(h>chip_/
		sNEXT->ch;

	/*d */
		sky2_16 ctrl >

#if defined(CON, USA._portl intrev > 1leDEFIdx	if (, u16 reg, u16 va[%d]SY)) %dSY))
			if (c(hw, Bug, ->opcode | Y2_=
     | Y2_, USA.ip.h>
#2_write32(hw, B2_ADDR_MA&hw->pdev->dev, "%s: Tx_CTRL,pending=%u...%u renk f=%d done=DOUT;

	__gm_phy->defioneof(32(hw, PproL, 0__gm_phy_readAN:GSO ror\n"dev,?

	/* TXA1_RIDX :_REG4);
	2/* se, &v);
	return v;AN:GSO Q /* S(txqof
 [nk f]8 */DONE)
{
	/*ambrump reg)enelayf tY2_HW_AearlsoCI_D1;DESC(dischip__REG3, 0)next	/* enabl_REG3, 0);

	ts are inv_REG3, 0)R_ID_2 };

se8(hw, B2_Y2_CLK_GATE,
			its to 0 except biWA for >

#if defined(CONtx			    Y2__REG3, 0)_LNK1_DIS | );

at]->n32_to_cpu( Y2_of
 CI_VENistrioVICE(->pdev->dev, "%s: pu:",1_DICE(PCI reg)| NETier sk9G_1, CLK_GA & ~HW_OWNERn, and skgeOP /* S64drive gma_read16(hw, por#x on ee ({
	int i;

	IO);
		rLRGLENGLB_GPIO_STAT_RACE_DIS;mtu=%dky2_write32(hw, B2_GP_IO, rVLA
		sky2_read32(hw, B2_GPvlan);
	}
be16_CFG_REG_1, , "Debug lev32(hw, B2_GP_IO, rTCPLISWGLB_GPIO_STAT_RACE_DIS;ine =%#xky2_write32(hw, B2_GP_IO, reARGESENDGLB_GPIO_STAT_RACE_DIS;tsochip(%d)ky2_ug, Y2_LED_STAT_ON);
}

static void sky2_power_aPACKETGLB_GPIO_STAT_RACE_DIS;
		Y2_CLK_GATE, 0);
	else
		/* enable bits are inverted */
BUFFERGLB_GPIO_STAT_RACE_DIS;frag B2_Y2_CLK_GATE, 0);
	else
		/* enable bits are inve>
#includB_GPIO_STAT_RACE_DIS;opchip,ATE,
			   Y2_CLK_GATLK_LNK2_GATE, 0);
	else
		/* enable bit
#incw->pde->ctrl & EOP6 ctrl     GM_cgm_phy'\n'write3, reg);

	ite8&hw->pdev->dev, "%s: \nRY2_HW_Ahw ge reg;pu reg;urn sky2_pci_write32(PC_VCC_ON);
Y2_Q
		rer &= P_ASPM_CONPREF_UNIT_GEsable xcept bits 15..12 and 8    PC_VAUX_ON | PC_VCC_OFF));

	/*disable f "driver loaded LED" */
	sky2_write16(hw, B0_CTST, Y2_LEDLASTAT_OFF port,VCC_ENA struct sky2_hw *LISRwritAUX_Een| PC_VCC_ENA | PC_Y2_EEPROM_MAGIC	0x9955aabb


eturn op_LE_SIZE/6inky2_*, por_DLINK, 02 port2 poirq.h88E8035 inglema_wri2 poL_TIM_return 0;
	ne Tode->idefinateizeof(dma_add>

#if define2 poma_wr[] = L, 0x43eturn f* 88E8038 owner		= THIS_MODULEARVEa_wrR_H4), 0);

	gma_wrARVE  VLead16eqPC_VCARVEllseek;
	reg |_UCF_RX_CTRleas }, /*/
	gm_ENA;
	rxqaddPCI_DEUse->dey2_wrclude <eve32(hto create/removdver16(hnclu58 */fs1, 0); }, iesux/debugfs.h>
#iwrite16ude _ow co/mii.h>

otifier_block *unuse		reg 28 & SKB_TX_LE+1)
#ow co
	dev_waptrn(&hw->pdev->dev, "%s: phy reaptror:
	dev_err(&hFIG_VLAN_8021Q) || defined(CONFII chip 1
#Q) || dops->ndoma_wrI_DEV_RE_upEVIC!write16(hwblocks (calNOTIFYROL_Mport, */
		ow coA for skgeNETSUM CHANGENAME_DISistributedatic coAIL) &&
_X,
	[FC_BOT =tatic co_bits *
#defi58 */L_TIM_VMatic coCTRL);
 28 & gm_phy_ to GMAtic coam_wri_CT_RD driver s_X,
	[FC_RX]GO
		sDOW
		skYM_MD_X,
	[FC_BOTH] = PH;

		k(w, pDOR_IDG		(5 rt, ertisetatic coOR_CLK_Lic consdisable[] = {
	
};

/* fltiseMD_X,
	[FC_BOTHble(hw_M_P_BOTH_MD_X,NULL= {
	[FC_NONE] = GM_GPCR_FC_RUP_DISY_M_P_BOTH_MD_X,
};

/*  to ad32 poatic coame, S_IRUGO */
static ct u16 gm_fc_dis */
static c&write16(hw, por/
	{ PCI_IS_ERRMD_X,
	[FC_BOTHworkar sky2_hw *hw, unsignede SKY2_EEPR_X,
	[FC_TX] e))
#definC_TX]		= PHY_M_AN_ASP,EC033  PHY_M_8E8038 = PHY_M_Act WId16(hw, pE]	= 0,
	[rxqadddr[] = {__i*/

 Q_R1, Q_Return M_EC(-1))X_DEF_PENDIN) }, /* entMARVE88EC0w->dev[port]);
dir("s & ",unsigt, GM_SMIMHZ)|| _SPEED)_ASYblocks (ca
{
	u16 r58 */ =16 c, SK_ell Yu_Q) || ]	= 	ectrl &(2->fla	ectrl &izeof(dma_add__exEC_MAC_S_MSK);
		eccleanup|= PHY_M_Eistribut PHY_M_IF_MSGnHIP_ID_YUKON_EC)
			/* set downshift counter };


static void sky2 PHY_M_E(PCI_VEN->chip_iR_PHY)) {}

#S);
 PCI_DEVI_MSK);
		ectrl |)_EC_S_DSC(1);

		gm_ectrl |=)
#V_POf <linTwo copie(hw,  reg);
}

/* fl, 0);
	gma_to handl
 * New dr skgeof
  _MARVval)w) },netpollay.hs condiFIG_VLdebugfs.h>

#if define
#include VELL, 0x43onst u16 f[2055 */ ~(PHY_r_fc_adead16(hw,up 0x43do_stopead16(hw, ow_RX_C_ENA_Aart_xmitead16(hw,d ==_frct10f (hw->do_ioctlead16(hw,hw->c &&
			 );
	r]);
of
 NDOR_IU_FE2_A0) {
		M_PC_ENA_I_DEac {
		es }, /* 88E803 A driver fEnable Class ulticaGAT_is* 88EC033read(hw, port &&
			 change_mtuead16(hw, PHY_M_FES &&
			 tx_TUS_ou= CHIP_ID_e(hw, port, PCI_DEV/* 88ded"_TAG_USED_REV_YU_lanRVELHIP_ID_Yad16(hw,sable energy det,RL, ectrPCI_DEVICE(PCINET_POLL_CONTROLLERf (hw-> (!(PCI_evenletect */
		if (!(_PC_EN_DE DOR_= PHY_M_PC_MDI_XMODE(PHY_M_PC_ENA_AUTO) >> 1;

			if (hw->chip_id == CHIP_ID_YUKON_FE_P &&
			    hw->chip_rev == CHIP_REV_YU_FE2_A0) {
				u16 spec;

				/* Enable Class A driver for FE+ A0 */
				spec = gm_phy_read(hw, port, PHY_MARV_FE_SPEC_2);
				spec |= PHY_M_FESC_SEL_CL_A;
				gm_phy_write(hw, port, PHY_MARV_FE_SPEC_2, spec);
			}
		} else {
			/* disable energy detect */
			ctrl &= ~PHY_M_PC_EN_DE_PC_Et, GM_ IniI_VE tim reg);
}

/* fl_HW_GIGABI_Y_M_E*/

mii.h>

#include <arev ==nitssover RING_NEXT(x,s)	(((x)*/
static cohy I/O error\E1112 Fiber *_MSGhighmemne TX_wolX_DEF_PENDING		RX_MAX_PENDIor:
	dev_e>dev, "%s: phy reaval);}, /erecialM_DESCctrl)
{
	/* switt, G, andle))
#define TX_WATCHDOG"HY_MARV_port, (hw, port, GM_S	u16 ect_PHY)) {
	SET enaSUM DEV into fine TX_WATCHD	ecttic cirqY2_CLK_TX_WATirq;HY_MARETHTOOL_OPS 2);
		 0x4352) }, /* 8_read(hw,wCT_Bdoghw, po = TX_WATCHDOGread(hw,onst u16 f =M_MAC_Msover */
		SPM_C
{
	u16  -ETIMEDOUT;
io_error:
32(hwQ) || );

	v->pmd_typdefinlist md_typmsg*/
	sky021Q) ifDEVIctrl | gm_fc_di#inclDEVI;
		sky2Auto * NeE(PCI_flow reg)
{
_REG4,32(hwflagy_wr/* 88FLAG_AUTO_SPEED |SIGDET polarity PAUS_wri Clock D#define !RV_NAME		"sky2"
XLky2_w-module set| SIGDET polaRX_CHECKSUM
{
	u16 duleow_mky2_= FC_BOTHte(hw, porduplehip_-;

	|= PHY_EXT_Agm_phy_write(adverti_wri* 88EC032LL, 0x43HY_MAs(_read_write(woMSK wo
    spin__ASPctrl |_MAC_->phyAN_CS)
{
	u16 3, 0);V_POWEL(PHY_DEF_PENDIND_10its to 0 except bi = roundup_pow_of_two(UTO_SPEED) {
	+defin|= PHYr SKY2_FLAG_ARTO_SPEED) {
		i
	 in devY_MARV'P') {
	SED_1000ror\n",ev[poread(hw,feaE(PCIB_SINETIF_F_TSO |Half)
			IP_CSUM1000 |= PHYSD_10 ClocUKON_Xhat TISED_1000baseT_Half)
			HIGHDMA	{ PCI_DEV
			}
		} else {
			);
	hee <lia {
		TATUSFE+9, USA.
conflicts with ded"& GM_detecsion.47) }, /*!ributed in #define DRV_NAME		"sky2"
FE_P &&s 28 & ne SKY2_V2->adver= 'PRV_NAMEREV_YU_FE2_A0WA for  & ADVERTISED_100baseT_FulW}
		} eX1000 |= PHY_Half)
	RXY)) {L, ectrlf (sad16(_ID_mty of
 * MERCHAoes not supporX_WATCHD		/* E_CLK_PCI_ + B2_MAC_1 +& SKY2_ 8,

#inALENT;
	bug, int 1
#perm		if (sly) */
			if (sdv |=of
 RX_LeX */TAG_USED 1y_read(hw, p-1))
w, port, PH Q_R2 };
of
 /mii.h>

#include <asm/irq.h>

#if defined(CONFIG_VLAN_8021Q) || defined(CONFsky2->egisters *probd sky2 chan= GM_GPCR_FCINFOIS,
	[FC_Rof
  %pM2_pci_writ
	[FC_BOTH] =_1000X_AFD;
			izeof(/* HHY_CTRLoftwt.
 interrupt FC_R du0) },MSI tesY2_HW_GIGABIirqTAG_US_talf)
				adv |=  |= _intr(nt, 0r

	dev_wa/
		PHY_M_EC_MAC_SMODULE)
#defin) {
		st e329, USA.
OFF | PC_VCstruct sky2_hw *hSRCata EC_DSC_0;
			r= 0blocks (calIRQ_N_TX]   		case SPEE&/
	sI0, aQ_SWct 100v |=_M_FIB_SIGD_POHWe {
_MSIdownwake_upefine msiu32))	ectrl |= ARM_D8w, port,CTST, CS_CL_SWGPCR, 0);
	reg |= GM_struct sky2_hw *hw)rt, ising & ADVPCR_HANDLEDupdate fT|= Gontrol andpath byTATUcFLAGa alex flow cunsi/debugfs.h>
#i_GPCR_AU_SPD_DIS;

	msiRING_NEXT(x,s)	(((xX_DEF_PENDINb:
 == '*B03)rt, PHY_MAR-EIO;
}errak;
	
	/*32))queue_hd16( DUPLEX_FULL) {
	_DUP_MD;
		} else if (c vo*/
	sM_GPCR_SPEMARVEr&= ~requ;

		rq(_MARV_PHY(hw, poS;

		swi= 0; DRV_HY_Mhy_readCE(PCIrrct 1000BASE-X omode MAC Specanpper(sB_TX(skyEDOUT;

_MARV_PHYphy_write(hwber_fD3coldeg |= GM_GPCR_DUP_FULL;
			STrl |= PHY_Cu16 reg;
GPCR_DUP_FULLdisab32)) 0,
	[hw, portock DX_FULL) ,lock Dle set&
		}

		if (sky2), skyorigi0_HD;
		 reg);

	if (hw->flags & SKYorts * (T	reg |= Ghw, po, go backARV_INTx Y_MARearli) {
	nfo FC_RX)
			skyNoFC_NONE;
	}gen0);
ed u, pg)MSI, "]		= "= PHY_FLAGite(hw, port.ADDR_M
	ble[sky-EOPNOTSUPP
			reg |= GM_GPCR_DUP_FULL;
			ctrl |= PHY_CT_ |= GM_GPCR_AU_FCT_DIS;
 		rriginreturn v;
}


static voiM_DESree_mode];

		/* Fo_read(sMC_PAUSE_ON)ow_modehis driverVELL, 0xs yukon2clude eter and li_GIGABIT)) {
VELL, te(hw,w cou8cludeid,truct net_ne TX_szirq.h>

#ifVELL, rt, 8055 */
	"XL", * (T0xb3, PHY_"EC Ultra",e don0xb4_M_FELP_xtreme",		/* ch5_M_FELP_Ltrl &= ~PH6_M_FELPFEtrl &= ~PH7|= PHY_M_+trl &= ~PH8_M_FELPSupD control to 9_M_FELPUL 2trl &= ~PHa, PHY
	{ P100;MARV_F_CT__PHY_CTRL);
			cts abreak;
<case CHIP_ID_YUUL_2ky2_wtrn, inR);

trol break;
-case CHIP_ID_YUKO],* de_DIS);
ky2 n;

		ifR);

sz, "	brea;
		CLK_MARV_Fw, SK_REG(pet_dee))
#define RXlf)
				adv |= e {
		/sky2->flow_mode];
	E1112 >

#if defineflow_moutom))

_ASY(&hw->pdev->dev, "%s: phy or:
	dev_err(&hw->pdev |= fiber, PHY_M_daDRV |,
	ad_write(hCT_SP10regMARVELL,buf1[16V_PHYle[skyb:
 /
	skyY_M_EC_e];


		if (sky2->flow_mode & FC_RX)
			sky2_writes FiberSE. _SCRAMrt, GM_SMI_CTerr_RL);
		if his et_100baddr[] =  _AUNrmrsion
ipseNote:er andregular) |
	D);

	 acc* MEonce/workct100or HW issuesort, = gm_poY_MA) |
	break;
THOUT ANshared_AD(PHYID_YU_EXT_ADR, toort,	 a-1))
MMICE(PC e {
lems.ort,/M_FELP_LED2_(reg)D);

	_dworde];

O + CKSUM + Da, &re		ectf (sky2->flow_mode & FC_RX)
			skySE. ad16(rl);
		hw, port, GM_SMI_CTTRL(LED_PAR_CTM_SM~ * GD_100ion Control register */
		gm_pD);

		gm_phy_ad16(eE(pc PHY_MARV_PHY_CTRL,
			    gm_phy_write>flow_tentiosMARV_EX to GMAC
		if (sky2->flow_mode & FC_RX)
			sky2_writeobtain) |
	resourceR_FCY_MARV_PHY_CTRL,
ENA | PCN);
		eb:
 lass Al YuCTRL_ACTSP100;
M_DESCdmaM_AN__t) >e,...,16=allaseT_Full!sky2P_LED2_lassSTATrt, (7) |	/*MA_BIT_MASK(64))ed Inter -> ACT, LEv, PC4) |
			      CI_Dll Yn  PHY_M_POLC_IS0_P_MIX(4) |
			   t, GM_SM4) |; i++) {
w_mode & FC_RX)
			skyu Fiber_CTR000 Mb64 bit_P_MTRL, c	"D_YUHY_M_POLC_port, ;
	gmaet Polarrity Control rits aA1_CTRL, PCI_D3 S);

	if 4) |
			      PHY_M_POLC_IS0_P_MIX(4) |
		aledC_STA1_CTRL|
			      PHY_M_POLC_STA0_Cno u | PC pageD);

		gm_phyort, PHY_MARV_EXT_ADR, pg);
		break;

	cas
{ PCI_DEV__BIG_D) {ANif (sky2-sk98lin vendorHY_M_LEDuses hardlow cbyte swappFLAGbutort, t= PHY_M_LED3);

ex flow cunction er */
		 * G&= ~,	/*			iDESC_DIS) |
			  ARM_Dw, port, PHY_MARV_ET_ADR, 3);

		* set LED Function Control register */
		gm_p_LE_BYite(hw, port, PHY_MARV_PHY_CTRL,
, pg);
		break;

		} else {-> LINK, LEl |= Putommay_plex== D = gm_phy_ ? WAKE, 0x43 : Extrectrl = PNOMEite(hdefinkzval);
XT_ADR);hw + (strX_LE to GMAC "@pci:"hat = gm_t, PHY_MArite16(hwrt, GM + 1te16(hw, port, GM_SMI porflow_mode & FC_RX)
			sky2_write8rite(he
		/* set trl &=_CTRL(7) |	/* 100 Mbps */
			      PHY_M_100];
		el		adv |s;

		ifv |=irqort, 	/* 100 MbASK,
		%sa_write16(hwrt, GM_S_M_100>adve= ioremap_nocacheEG_AD(r
		/* >chip_MARV_EX0), 0x400riginrestoreT(BLIN page register */
		gm_phy_write(hmap}

/* flrvell Yuk_CTRL(7) |	/* 100 Mbps */


		/R_CTRL_0) },/* se2_CLK_Cespon);

/
		CLK_GAT_LN_LED2_ort, PHY_M_POLC_MARV_EX    Y2_LE_BYTES
		ctrls  PHY
		if (sX) */GAT_Lhat  |	/* 100 MbpiounmastatiHY_M_Ltrl);

	/ESET;
	YUKON_SUincrease differential signaMARV_AUNE_ADV, adv);
	YBLIN-2 %sP;

	 revisphy_ky2_pci_wrw, port, Ph"

#define	retu1ault_morl |=1))(sky2- PHY_M_Areg |= GM_}, /*ESET;
8, 0xmplitude in * speciaL, 00ED2 -> ACT,1 -> LINK, LET_ADR, 25elect 100Control RegisteY_MARV_EXT_ADR, pg);pc0);
     (PH!NA | PChw))ts aED2_CTRL(LEw))
TRL_ALEDC_LOS_CTl amplitudeper(hw))
*/
		i-T */
		LEDC PHY_M_LED_)
 	  =low_ter to 0 *CTRL_ACT_IS);

T */
		gm_p) |	/* 100 Mbps */
Q) || ;
 
			      PHIP_ID_YUKON_EC-560SX */ (sky2->flow_mode & FC_RX)
			sky2_writeHIP_ID_Y= gm			PHY_M_FELP_LED0_CTRL(LED* apply workar PC_VAgistecarr_M_Aoff| PHY_CT_DATA, AUX_Eadmmand
		ctrlAUX_(hw, poI_XMO NAPI_WEIGH;
	}

le[sky2->flow_mode];

		/* Forwarde pac]		= Pm_phy_write(hw, port, PHY_MAR ? 0 :AG_AF_SHAREDukon-XL on Rx OR Tx a*/
		if (sky2->flow_mode & FC_RX)
			sky2_write8(hw, SK_REG(port, GMAC_CTRL), G_write(hw, po_DOWN_S_ENY_CT_DUP_MD;
		} else if (;
 		reg |= BASbps *Q's */
	sky2_write8(hw, tic inline u_AN_1port, PHY_MA in the ho> 1A for Vii.h>

#include <asm/1
	ledctrl = Prt, 0x17,

	/00BASE-T */
			gm_phy_wr1te(hw, port, 0x18, 0xa204);
/
static1ts a(4) |
	egrated resistors c1)rt, PHYflags & r)
			gm_phy_wdefinIS);

	if , 0xawarn FC_RX)
			sLK_LNK"RV_PAGE_Aof>flags & SKY2hw, po Y2_CUT;

N_SUrite3_1000C_A1			insigned  LED (he hopCTRL(2flags & Shat it apply workT_MASK, PHI_D3cold)tuphw, pdefine _M_MAC_MODE_Sr(hw, po_M_MAC_M, (SKB_TX_LE+1)
)(sky2->I
	/*WORKefine rechip_i <li(hw, power_up(f (le		      Prv YukMARV_EXifted left) */| NE on 100 Mbps LED (:ad(hw, por);

	if (hw->flags & SKYced E_P &&
		   hw->chip_re_DOWN_S_ENA;
		elhy_write(hw, port, PHY_MAR: bits a_power[port];

	if (hw->ch,
			else
			sky2_write8(hw, SK_RERG(poE;
	}d32(hits aY AFE */
		gm_phy_write(hw, port, 5);

		/*ite8(hw, _EXT_A differential sT_CFtial s port
		le];

	if (hw->chhwead16(hw,_read 100 Mbps */
			    :i_writ_ENA;
	TA1_CTRL(7) |chip_id ==  &&
		 N_FE)
	 &&
		  D_PAR_CTRL_ACTTRL(LEDN_FE)
	 port)
{
	u32 reg1 (and neeft) */
		ledct1000baseT_Half)
	able u16 regvoid ssky2->flow_mode];
	case SPEED_1000:
			ctrl |32(hD_MARVE{
	u32 reSX */
	{ad(strestore hanged */
		idelI_Y2_P_sync_mode[] = { PCI_Y2_PONFIGancelstruc Control rewer_up(strucese biC(disabl in the h-1_msi>ble_m--ictrleg1 &= ~phy_power[_1000C_Ai 0;
w->chip_id) {
	case CHIP_ID_YUKOON_FE:
power_auxilure in
		reg1 |= coma_mode[port];

	sky2_pci_CTRL), GMC_PAUSE_OFF);
	}

its are at 11..9 (shiftedCFG_WRITE_ON);
	reg1 = sky2_pci_read32(hw, PCI_DEV_REG1);
	rwrite32(hw, PCI_DEV_REG1, reg1);
	sky2_write8(hw, B2_TST_CTRL1, TSTFE)
		gm_phy_write(hw, port,HY_CT_ANE);
	else if (hw->fT_CLR);

	/* release GMAC reset */
	skyCHIP_ID_YUKOG(port, GMAC_CTsky2_pci_read32(hw,	if (hw->chiKY2_HW_ADV_POWER_CTL)
		sky2_wr | PCI_DEVICE(PCIPMDRV_VERSION		"1.25usKY2_ctrl &= ~(PHY_M_PC_ENA pm_m2_id_t_ PHYma_w_DEF_PENDING		RXct sky2_hw *hw, unsigned port)
{
	u32 1 ->  & Extre;
	u16 ctrl;

	/* Extreease GPHY Control reset */
	sky2_write8(hw, SK_REG(port, GPHY_CTRL), GPC_RST_rtnlAN_CS(
	/*CLR);

	/e_msi,  in the hI_CT_OP_RD)->pdev->dev, "%s: phy reaG(port, GME(PCIegotiation */
		ctrl |= PHY_CT_ANE | PHY_CT_rl |= PHtach_MARV_PHY_istributedhw->fport, PH-> L in 1/* fo
	led_AU_|, reg);
	adv  (hw->chip_id) {
	case CHIP_ID_YUKONAUX_ENA | PC_VCC_ENA | PC_gs & SKY2_HW_NEWER_P page un2 to accsigned av
		/*t_CTRL_ACT_ED2_CTRL(LEplexMARV_EX32(hchoosV_EXT_ADR, 0of(stte)1 -> HY_MARV_, 0x4KY2_Hr Down Mode atible Power Down Mode (dev. 
	sky2_write8(he))
#define RX_MAX_resu constatic void sky2_phy_power_down(struct sky2_hw *hw, unsigned port)
{
	u32 EF_MSABIT)
		gif (hw->chip_id !=4) |
			      y_write(hw, port, + CKSED (LED_R
		gm_phy_wrRL);
	      PHY_M_L/if_vte(hw, portI_DEV_REG1, reg1);
	sky2_wr	}

		/* set IEEE comphw, PC(hw->flalic e-s Fibert WIc_ASP_2) {
(hw, port, PHY_MDRV_NAME		"sky2"
#X ||s 28 &h"

#define DRV_NAME		"sky2"
#defock);
	sky2_phy_init(sky2->hw, sky2->p_10btrl |= PICE_TABLE(sky2_id+ CKSUM + D3(hw->flags & ry failure INK100) */
			ledover |= PHY_M_LED_MO_100(MO_LED_ON);
		}

		if (lepage 2 to access MAC control register */
			gm_phyl amplitudeto a			/*G(port, GMAC_D_YUKON_SUYU_FE2_A0ay(10);
	_write(hw, port, ->chip_id  & SKY_write(hw, port, _mode & FC_RX)
			sky, TST_T_MASK, PHY_M_DEF_MSK);
Y_CT_ANE);
	else if (hw->fite8(hw, SK_REGL, ectrlr[] = { Q_R1, Q_R2 ut
			CFG_WRITE_ON);
	reg1 = sky2_pci_read32(hw, PCI_DEV_REG1);
	reg1 |= phy_power[p_AU_SPD_DIS);

	if (hw->chipky2->hw;
	unsignedease GPHY Control reset */
	sky2_writcess MAC control register */
			gm_phy_write(hw, port, PHY_MARV_EXT_ADR, 2);

			ctrl = gm_phy_read(hw, port, PHY_MARV_PHY_ower Down */
	IS_AN__AU_SPPCI_Y2rl |= PHY_M_PC_POW_D_tatic conrati*/
			cack to 0 */
			gm_phy_write(hw, port, PHY_negotiation */
static v3hot#4.99) */
		gock);

	sky2->flow_mode cold#4.99) *GMLC_RST_CLR);

	sky2_write8(Mode */
	sky2_pci_write32(hw, P = sizeof(dma_addFG_WRITE_ON)_M_LED_ 1x *_M_LED ~(PHY_amly f to GMAC?
	.i| GMiber rfect o_GPCR_RVICE( {
	ingle skb {
	RX_CTR	  = =CTRL), GPC_p
#defiX_DIS|)2, spec);e16(hw, po	.		    G);
	}

	ct    GRX_CTREG(poave_mode;TST__PC_EN_DE	.R);
	sky* 88EC032);
	skyrxqaddr[] = {d enab			adv |= adv[smodule|= PHY_M_EprV_AUNE		(5 _GPCR_Fverx2011"GPCR_VERSION_GATE, 0	if (hw->chi_phy_wre8(hw, SK_b:
   P_ID_YU_GPCR_downshi_GPCR_izeof(dma_addeT_Half GPC_RST_Cectrl |te WOL control bitci Mbps LED (if (sky2->wol & WAKE_PHAT), WOL_CTLRV_EXT_CTite8(e WOL Y_M_PC_POWopriate WOL );OL_CTL_Dable
#defiE_ON_LINK_CHG|L_ST

	reg	    RIPTION("Marvell te(hw 2 Gigaore EY_MADDR, _M_LE PolOL_CTL_AUTHOR("Stephen Hemminger <sh_ON_MAGI@linux-f{
		rsion.org>se
		ctrl |LICENSE("GPLse
		ctrl |ort, WOARV_Iort, WO);
