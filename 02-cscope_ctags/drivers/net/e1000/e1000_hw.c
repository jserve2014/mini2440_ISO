/*******************************************************************************

  Intel PRO/1000 Linux driver
  Copyright(c) 1999 - 2006 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

 */

/* e1000_hw.c
 * Shared functions for accessing and configuring the MAC
 */

#include "e1000_hw.h"

static s32 e1000_check_downshift(struct e1000_hw *hw);
static s32 e1000_check_polarity(struct e1000_hw *hw,
				e1000_rev_polarity *polarity);
static void e1000_clear_hw_cntrs(struct e1000_hw *hw);
static void e1000_clear_vfta(struct e1000_hw *hw);
static s32 e1000_config_dsp_after_link_change(struct e1000_hw *hw,
					      bool link_up);
static s32 e1000_config_fc_after_link_up(struct e1000_hw *hw);
static s32 e1000_detect_gig_phy(struct e1000_hw *hw);
static s32 e1000_get_auto_rd_done(struct e1000_hw *hw);
static s32 e1000_get_cable_length(struct e1000_hw *hw, u16 *min_length,
				  u16 *max_length);
static s32 e1000_get_phy_cfg_done(struct e1000_hw *hw);
static s32 e1000_id_led_init(struct e1000_hw *hw);
static void e1000_init_rx_addrs(struct e1000_hw *hw);
static s32 e1000_phy_igp_get_info(struct e1000_hw *hw,
				  struct e1000_phy_info *phy_info);
static s32 e1000_phy_m88_get_info(struct e1000_hw *hw,
				  struct e1000_phy_info *phy_info);
static s32 e1000_set_d3_lplu_state(struct e1000_hw *hw, bool active);
static s32 e1000_wait_autoneg(struct e1000_hw *hw);
static void e1000_write_reg_io(struct e1000_hw *hw, u32 offset, u32 value);
static s32 e1000_set_phy_type(struct e1000_hw *hw);
static void e1000_phy_init_script(struct e1000_hw *hw);
static s32 e1000_setup_copper_link(struct e1000_hw *hw);
static s32 e1000_setup_fiber_serdes_link(struct e1000_hw *hw);
static s32 e1000_adjust_serdes_amplitude(struct e1000_hw *hw);
static s32 e1000_phy_force_speed_duplex(struct e1000_hw *hw);
static s32 e1000_config_mac_to_phy(struct e1000_hw *hw);
static void e1000_raise_mdi_clk(struct e1000_hw *hw, u32 *ctrl);
static void e1000_lower_mdi_clk(struct e1000_hw *hw, u32 *ctrl);
static void e1000_shift_out_mdi_bits(struct e1000_hw *hw, u32 data, u16 count);
static u16 e1000_shift_in_mdi_bits(struct e1000_hw *hw);
static s32 e1000_phy_reset_dsp(struct e1000_hw *hw);
static s32 e1000_write_eeprom_spi(struct e1000_hw *hw, u16 offset,
				  u16 words, u16 *data);
static s32 e1000_write_eeprom_microwire(struct e1000_hw *hw, u16 offset,
					u16 words, u16 *data);
static s32 e1000_spi_eeprom_ready(struct e1000_hw *hw);
static void e1000_raise_ee_clk(struct e1000_hw *hw, u32 *eecd);
static void e1000_lower_ee_clk(struct e1000_hw *hw, u32 *eecd);
static void e1000_shift_out_ee_bits(struct e1000_hw *hw, u16 data, u16 count);
static s32 e1000_write_phy_reg_ex(struct e1000_hw *hw, u32 reg_addr,
				  u16 phy_data);
static s32 e1000_read_phy_reg_ex(struct e1000_hw *hw, u32 reg_addr,
				 u16 *phy_data);
static u16 e1000_shift_in_ee_bits(struct e1000_hw *hw, u16 count);
static s32 e1000_acquire_eeprom(struct e1000_hw *hw);
static void e1000_release_eeprom(struct e1000_hw *hw);
static void e1000_standby_eeprom(struct e1000_hw *hw);
static s32 e1000_set_vco_speed(struct e1000_hw *hw);
static s32 e1000_polarity_reversal_workaround(struct e1000_hw *hw);
static s32 e1000_set_phy_mode(struct e1000_hw *hw);
static s32 e1000_do_read_eeprom(struct e1000_hw *hw, u16 offset, u16 words,
				u16 *data);
static s32 e1000_do_write_eeprom(struct e1000_hw *hw, u16 offset, u16 words,
				 u16 *data);

/* IGP cable length table */
static const
u16 e1000_igp_cable_length_table[IGP01E1000_AGC_LENGTH_TABLE_SIZE] = {
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 10, 10, 10, 10, 10, 10, 10, 20, 20, 20, 20, 20, 25, 25, 25,
	25, 25, 25, 25, 30, 30, 30, 30, 40, 40, 40, 40, 40, 40, 40, 40,
	40, 50, 50, 50, 50, 50, 50, 50, 60, 60, 60, 60, 60, 60, 60, 60,
	60, 70, 70, 70, 70, 70, 70, 80, 80, 80, 80, 80, 80, 90, 90, 90,
	90, 90, 90, 90, 90, 90, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	    100,
	100, 100, 100, 100, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,
	    110, 110,
	110, 110, 110, 110, 110, 110, 120, 120, 120, 120, 120, 120, 120, 120,
	    120, 120
};

static DEFINE_SPINLOCK(e1000_eeprom_lock);

/**
 * e1000_set_phy_type - Set the phy type member in the hw struct.
 * @hw: Struct containing variables accessed by shared code
 */
static s32 e1000_set_phy_type(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_set_phy_type");

	if (hw->mac_type == e1000_undefined)
		return -E1000_ERR_PHY_TYPE;

	switch (hw->phy_id) {
	case M88E1000_E_PHY_ID:
	case M88E1000_I_PHY_ID:
	case M88E1011_I_PHY_ID:
	case M88E1111_I_PHY_ID:
		hw->phy_type = e1000_phy_m88;
		break;
	case IGP01E1000_I_PHY_ID:
		if (hw->mac_type == e1000_82541 ||
		    hw->mac_type == e1000_82541_rev_2 ||
		    hw->mac_type == e1000_82547 ||
		    hw->mac_type == e1000_82547_rev_2) {
			hw->phy_type = e1000_phy_igp;
			break;
		}
	default:
		/* Should never have loaded on this device */
		hw->phy_type = e1000_phy_undefined;
		return -E1000_ERR_PHY_TYPE;
	}

	return E1000_SUCCESS;
}

/**
 * e1000_phy_init_script - IGP phy init script - initializes the GbE PHY
 * @hw: Struct containing variables accessed by shared code
 */
static void e1000_phy_init_script(struct e1000_hw *hw)
{
	u32 ret_val;
	u16 phy_saved_data;

	DEBUGFUNC("e1000_phy_init_script");

	if (hw->phy_init_script) {
		msleep(20);

		/* Save off the current value of register 0x2F5B to be restored at
		 * the end of this routine. */
		ret_val = e1000_read_phy_reg(hw, 0x2F5B, &phy_saved_data);

		/* Disabled the PHY transmitter */
		e1000_write_phy_reg(hw, 0x2F5B, 0x0003);
		msleep(20);

		e1000_write_phy_reg(hw, 0x0000, 0x0140);
		msleep(5);

		switch (hw->mac_type) {
		case e1000_82541:
		case e1000_82547:
			e1000_write_phy_reg(hw, 0x1F95, 0x0001);
			e1000_write_phy_reg(hw, 0x1F71, 0xBD21);
			e1000_write_phy_reg(hw, 0x1F79, 0x0018);
			e1000_write_phy_reg(hw, 0x1F30, 0x1600);
			e1000_write_phy_reg(hw, 0x1F31, 0x0014);
			e1000_write_phy_reg(hw, 0x1F32, 0x161C);
			e1000_write_phy_reg(hw, 0x1F94, 0x0003);
			e1000_write_phy_reg(hw, 0x1F96, 0x003F);
			e1000_write_phy_reg(hw, 0x2010, 0x0008);
			break;

		case e1000_82541_rev_2:
		case e1000_82547_rev_2:
			e1000_write_phy_reg(hw, 0x1F73, 0x0099);
			break;
		default:
			break;
		}

		e1000_write_phy_reg(hw, 0x0000, 0x3300);
		msleep(20);

		/* Now enable the transmitter */
		e1000_write_phy_reg(hw, 0x2F5B, phy_saved_data);

		if (hw->mac_type == e1000_82547) {
			u16 fused, fine, coarse;

			/* Move to analog registers page */
			e1000_read_phy_reg(hw,
					   IGP01E1000_ANALOG_SPARE_FUSE_STATUS,
					   &fused);

			if (!(fused & IGP01E1000_ANALOG_SPARE_FUSE_ENABLED)) {
				e1000_read_phy_reg(hw,
						   IGP01E1000_ANALOG_FUSE_STATUS,
						   &fused);

				fine = fused & IGP01E1000_ANALOG_FUSE_FINE_MASK;
				coarse =
				    fused & IGP01E1000_ANALOG_FUSE_COARSE_MASK;

				if (coarse >
				    IGP01E1000_ANALOG_FUSE_COARSE_THRESH) {
					coarse -=
					    IGP01E1000_ANALOG_FUSE_COARSE_10;
					fine -= IGP01E1000_ANALOG_FUSE_FINE_1;
				} else if (coarse ==
					   IGP01E1000_ANALOG_FUSE_COARSE_THRESH)
					fine -= IGP01E1000_ANALOG_FUSE_FINE_10;

				fused =
				    (fused & IGP01E1000_ANALOG_FUSE_POLY_MASK) |
				    (fine & IGP01E1000_ANALOG_FUSE_FINE_MASK) |
				    (coarse &
				     IGP01E1000_ANALOG_FUSE_COARSE_MASK);

				e1000_write_phy_reg(hw,
						    IGP01E1000_ANALOG_FUSE_CONTROL,
						    fused);
				e1000_write_phy_reg(hw,
						    IGP01E1000_ANALOG_FUSE_BYPASS,
						    IGP01E1000_ANALOG_FUSE_ENABLE_SW_CONTROL);
			}
		}
	}
}

/**
 * e1000_set_mac_type - Set the mac type member in the hw struct.
 * @hw: Struct containing variables accessed by shared code
 */
s32 e1000_set_mac_type(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_set_mac_type");

	switch (hw->device_id) {
	case E1000_DEV_ID_82542:
		switch (hw->revision_id) {
		case E1000_82542_2_0_REV_ID:
			hw->mac_type = e1000_82542_rev2_0;
			break;
		case E1000_82542_2_1_REV_ID:
			hw->mac_type = e1000_82542_rev2_1;
			break;
		default:
			/* Invalid 82542 revision ID */
			return -E1000_ERR_MAC_TYPE;
		}
		break;
	case E1000_DEV_ID_82543GC_FIBER:
	case E1000_DEV_ID_82543GC_COPPER:
		hw->mac_type = e1000_82543;
		break;
	case E1000_DEV_ID_82544EI_COPPER:
	case E1000_DEV_ID_82544EI_FIBER:
	case E1000_DEV_ID_82544GC_COPPER:
	case E1000_DEV_ID_82544GC_LOM:
		hw->mac_type = e1000_82544;
		break;
	case E1000_DEV_ID_82540EM:
	case E1000_DEV_ID_82540EM_LOM:
	case E1000_DEV_ID_82540EP:
	case E1000_DEV_ID_82540EP_LOM:
	case E1000_DEV_ID_82540EP_LP:
		hw->mac_type = e1000_82540;
		break;
	case E1000_DEV_ID_82545EM_COPPER:
	case E1000_DEV_ID_82545EM_FIBER:
		hw->mac_type = e1000_82545;
		break;
	case E1000_DEV_ID_82545GM_COPPER:
	case E1000_DEV_ID_82545GM_FIBER:
	case E1000_DEV_ID_82545GM_SERDES:
		hw->mac_type = e1000_82545_rev_3;
		break;
	case E1000_DEV_ID_82546EB_COPPER:
	case E1000_DEV_ID_82546EB_FIBER:
	case E1000_DEV_ID_82546EB_QUAD_COPPER:
		hw->mac_type = e1000_82546;
		break;
	case E1000_DEV_ID_82546GB_COPPER:
	case E1000_DEV_ID_82546GB_FIBER:
	case E1000_DEV_ID_82546GB_SERDES:
	case E1000_DEV_ID_82546GB_PCIE:
	case E1000_DEV_ID_82546GB_QUAD_COPPER:
	case E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3:
		hw->mac_type = e1000_82546_rev_3;
		break;
	case E1000_DEV_ID_82541EI:
	case E1000_DEV_ID_82541EI_MOBILE:
	case E1000_DEV_ID_82541ER_LOM:
		hw->mac_type = e1000_82541;
		break;
	case E1000_DEV_ID_82541ER:
	case E1000_DEV_ID_82541GI:
	case E1000_DEV_ID_82541GI_LF:
	case E1000_DEV_ID_82541GI_MOBILE:
		hw->mac_type = e1000_82541_rev_2;
		break;
	case E1000_DEV_ID_82547EI:
	case E1000_DEV_ID_82547EI_MOBILE:
		hw->mac_type = e1000_82547;
		break;
	case E1000_DEV_ID_82547GI:
		hw->mac_type = e1000_82547_rev_2;
		break;
	default:
		/* Should never have loaded on this device */
		return -E1000_ERR_MAC_TYPE;
	}

	switch (hw->mac_type) {
	case e1000_82541:
	case e1000_82547:
	case e1000_82541_rev_2:
	case e1000_82547_rev_2:
		hw->asf_firmware_present = true;
		break;
	default:
		break;
	}

	/* The 82543 chip does not count tx_carrier_errors properly in
	 * FD mode
	 */
	if (hw->mac_type == e1000_82543)
		hw->bad_tx_carr_stats_fd = true;

	if (hw->mac_type > e1000_82544)
		hw->has_smbus = true;

	return E1000_SUCCESS;
}

/**
 * e1000_set_media_type - Set media type and TBI compatibility.
 * @hw: Struct containing variables accessed by shared code
 */
void e1000_set_media_type(struct e1000_hw *hw)
{
	u32 status;

	DEBUGFUNC("e1000_set_media_type");

	if (hw->mac_type != e1000_82543) {
		/* tbi_compatibility is only valid on 82543 */
		hw->tbi_compatibility_en = false;
	}

	switch (hw->device_id) {
	case E1000_DEV_ID_82545GM_SERDES:
	case E1000_DEV_ID_82546GB_SERDES:
		hw->media_type = e1000_media_type_internal_serdes;
		break;
	default:
		switch (hw->mac_type) {
		case e1000_82542_rev2_0:
		case e1000_82542_rev2_1:
			hw->media_type = e1000_media_type_fiber;
			break;
		default:
			status = er32(STATUS);
			if (status & E1000_STATUS_TBIMODE) {
				hw->media_type = e1000_media_type_fiber;
				/* tbi_compatibility not valid on fiber */
				hw->tbi_compatibility_en = false;
			} else {
				hw->media_type = e1000_media_type_copper;
			}
			break;
		}
	}
}

/**
 * e1000_reset_hw: reset the hardware completely
 * @hw: Struct containing variables accessed by shared code
 *
 * Reset the transmit and receive units; mask and clear all interrupts.
 */
s32 e1000_reset_hw(struct e1000_hw *hw)
{
	u32 ctrl;
	u32 ctrl_ext;
	u32 icr;
	u32 manc;
	u32 led_ctrl;
	s32 ret_val;

	DEBUGFUNC("e1000_reset_hw");

	/* For 82542 (rev 2.0), disable MWI before issuing a device reset */
	if (hw->mac_type == e1000_82542_rev2_0) {
		DEBUGOUT("Disabling MWI on 82542 rev 2.0\n");
		e1000_pci_clear_mwi(hw);
	}

	/* Clear interrupt mask to stop board from generating interrupts */
	DEBUGOUT("Masking off all interrupts\n");
	ew32(IMC, 0xffffffff);

	/* Disable the Transmit and Receive units.  Then delay to allow
	 * any pending transactions to complete before we hit the MAC with
	 * the global reset.
	 */
	ew32(RCTL, 0);
	ew32(TCTL, E1000_TCTL_PSP);
	E1000_WRITE_FLUSH();

	/* The tbi_compatibility_on Flag must be cleared when Rctl is cleared. */
	hw->tbi_compatibility_on = false;

	/* Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
	msleep(10);

	ctrl = er32(CTRL);

	/* Must reset the PHY before resetting the MAC */
	if ((hw->mac_type == e1000_82541) || (hw->mac_type == e1000_82547)) {
		ew32(CTRL, (ctrl | E1000_CTRL_PHY_RST));
		msleep(5);
	}

	/* Issue a global reset to the MAC.  This will reset the chip's
	 * transmit, receive, DMA, and link units.  It will not effect
	 * the current PCI configuration.  The global reset bit is self-
	 * clearing, and should clear within a microsecond.
	 */
	DEBUGOUT("Issuing a global reset to MAC\n");

	switch (hw->mac_type) {
	case e1000_82544:
	case e1000_82540:
	case e1000_82545:
	case e1000_82546:
	case e1000_82541:
	case e1000_82541_rev_2:
		/* These controllers can't ack the 64-bit write when issuing the
		 * reset, so use IO-mapping as a workaround to issue the reset */
		E1000_WRITE_REG_IO(hw, CTRL, (ctrl | E1000_CTRL_RST));
		break;
	case e1000_82545_rev_3:
	case e1000_82546_rev_3:
		/* Reset is performed on a shadow of the control register */
		ew32(CTRL_DUP, (ctrl | E1000_CTRL_RST));
		break;
	default:
		ew32(CTRL, (ctrl | E1000_CTRL_RST));
		break;
	}

	/* After MAC reset, force reload of EEPROM to restore power-on settings to
	 * device.  Later controllers reload the EEPROM automatically, so just wait
	 * for reload to complete.
	 */
	switch (hw->mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
	case e1000_82543:
	case e1000_82544:
		/* Wait for reset to complete */
		udelay(10);
		ctrl_ext = er32(CTRL_EXT);
		ctrl_ext |= E1000_CTRL_EXT_EE_RST;
		ew32(CTRL_EXT, ctrl_ext);
		E1000_WRITE_FLUSH();
		/* Wait for EEPROM reload */
		msleep(2);
		break;
	case e1000_82541:
	case e1000_82541_rev_2:
	case e1000_82547:
	case e1000_82547_rev_2:
		/* Wait for EEPROM reload */
		msleep(20);
		break;
	default:
		/* Auto read done will delay 5ms or poll based on mac type */
		ret_val = e1000_get_auto_rd_done(hw);
		if (ret_val)
			return ret_val;
		break;
	}

	/* Disable HW ARPs on ASF enabled adapters */
	if (hw->mac_type >= e1000_82540) {
		manc = er32(MANC);
		manc &= ~(E1000_MANC_ARP_EN);
		ew32(MANC, manc);
	}

	if ((hw->mac_type == e1000_82541) || (hw->mac_type == e1000_82547)) {
		e1000_phy_init_script(hw);

		/* Configure activity LED after PHY reset */
		led_ctrl = er32(LEDCTL);
		led_ctrl &= IGP_ACTIVITY_LED_MASK;
		led_ctrl |= (IGP_ACTIVITY_LED_ENABLE | IGP_LED3_MODE);
		ew32(LEDCTL, led_ctrl);
	}

	/* Clear interrupt mask to stop board from generating interrupts */
	DEBUGOUT("Masking off all interrupts\n");
	ew32(IMC, 0xffffffff);

	/* Clear any pending interrupt events. */
	icr = er32(ICR);

	/* If MWI was previously enabled, reenable it. */
	if (hw->mac_type == e1000_82542_rev2_0) {
		if (hw->pci_cmd_word & PCI_COMMAND_INVALIDATE)
			e1000_pci_set_mwi(hw);
	}

	return E1000_SUCCESS;
}

/**
 * e1000_init_hw: Performs basic configuration of the adapter.
 * @hw: Struct containing variables accessed by shared code
 *
 * Assumes that the controller has previously been reset and is in a
 * post-reset uninitialized state. Initializes the receive address registers,
 * multicast table, and VLAN filter table. Calls routines to setup link
 * configuration and flow control settings. Clears all on-chip counters. Leaves
 * the transmit and receive units disabled and uninitialized.
 */
s32 e1000_init_hw(struct e1000_hw *hw)
{
	u32 ctrl;
	u32 i;
	s32 ret_val;
	u32 mta_size;
	u32 ctrl_ext;

	DEBUGFUNC("e1000_init_hw");

	/* Initialize Identification LED */
	ret_val = e1000_id_led_init(hw);
	if (ret_val) {
		DEBUGOUT("Error Initializing Identification LED\n");
		return ret_val;
	}

	/* Set the media type and TBI compatibility */
	e1000_set_media_type(hw);

	/* Disabling VLAN filtering. */
	DEBUGOUT("Initializing the IEEE VLAN\n");
	if (hw->mac_type < e1000_82545_rev_3)
		ew32(VET, 0);
	e1000_clear_vfta(hw);

	/* For 82542 (rev 2.0), disable MWI and put the receiver into reset */
	if (hw->mac_type == e1000_82542_rev2_0) {
		DEBUGOUT("Disabling MWI on 82542 rev 2.0\n");
		e1000_pci_clear_mwi(hw);
		ew32(RCTL, E1000_RCTL_RST);
		E1000_WRITE_FLUSH();
		msleep(5);
	}

	/* Setup the receive address. This involves initializing all of the Receive
	 * Address Registers (RARs 0 - 15).
	 */
	e1000_init_rx_addrs(hw);

	/* For 82542 (rev 2.0), take the receiver out of reset and enable MWI */
	if (hw->mac_type == e1000_82542_rev2_0) {
		ew32(RCTL, 0);
		E1000_WRITE_FLUSH();
		msleep(1);
		if (hw->pci_cmd_word & PCI_COMMAND_INVALIDATE)
			e1000_pci_set_mwi(hw);
	}

	/* Zero out the Multicast HASH table */
	DEBUGOUT("Zeroing the MTA\n");
	mta_size = E1000_MC_TBL_SIZE;
	for (i = 0; i < mta_size; i++) {
		E1000_WRITE_REG_ARRAY(hw, MTA, i, 0);
		/* use write flush to prevent Memory Write Block (MWB) from
		 * occurring when accessing our register space */
		E1000_WRITE_FLUSH();
	}

	/* Set the PCI priority bit correctly in the CTRL register.  This
	 * determines if the adapter gives priority to receives, or if it
	 * gives equal priority to transmits and receives.  Valid only on
	 * 82542 and 82543 silicon.
	 */
	if (hw->dma_fairness && hw->mac_type <= e1000_82543) {
		ctrl = er32(CTRL);
		ew32(CTRL, ctrl | E1000_CTRL_PRIOR);
	}

	switch (hw->mac_type) {
	case e1000_82545_rev_3:
	case e1000_82546_rev_3:
		break;
	default:
		/* Workaround for PCI-X problem when BIOS sets MMRBC incorrectly. */
		if (hw->bus_type == e1000_bus_type_pcix
		    && e1000_pcix_get_mmrbc(hw) > 2048)
			e1000_pcix_set_mmrbc(hw, 2048);
		break;
	}

	/* Call a subroutine to configure the link and setup flow control. */
	ret_val = e1000_setup_link(hw);

	/* Set the transmit descriptor write-back policy */
	if (hw->mac_type > e1000_82544) {
		ctrl = er32(TXDCTL);
		ctrl =
		    (ctrl & ~E1000_TXDCTL_WTHRESH) |
		    E1000_TXDCTL_FULL_TX_DESC_WB;
		ew32(TXDCTL, ctrl);
	}

	/* Clear all of the statistics registers (clear on read).  It is
	 * important that we do this after we have tried to establish link
	 * because the symbol error count will increment wildly if there
	 * is no link.
	 */
	e1000_clear_hw_cntrs(hw);

	if (hw->device_id == E1000_DEV_ID_82546GB_QUAD_COPPER ||
	    hw->device_id == E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3) {
		ctrl_ext = er32(CTRL_EXT);
		/* Relaxed ordering must be disabled to avoid a parity
		 * error crash in a PCI slot. */
		ctrl_ext |= E1000_CTRL_EXT_RO_DIS;
		ew32(CTRL_EXT, ctrl_ext);
	}

	return ret_val;
}

/**
 * e1000_adjust_serdes_amplitude - Adjust SERDES output amplitude based on EEPROM setting.
 * @hw: Struct containing variables accessed by shared code.
 */
static s32 e1000_adjust_serdes_amplitude(struct e1000_hw *hw)
{
	u16 eeprom_data;
	s32 ret_val;

	DEBUGFUNC("e1000_adjust_serdes_amplitude");

	if (hw->media_type != e1000_media_type_internal_serdes)
		return E1000_SUCCESS;

	switch (hw->mac_type) {
	case e1000_82545_rev_3:
	case e1000_82546_rev_3:
		break;
	default:
		return E1000_SUCCESS;
	}

	ret_val = e1000_read_eeprom(hw, EEPROM_SERDES_AMPLITUDE, 1,
	                            &eeprom_data);
	if (ret_val) {
		return ret_val;
	}

	if (eeprom_data != EEPROM_RESERVED_WORD) {
		/* Adjust SERDES output amplitude only. */
		eeprom_data &= EEPROM_SERDES_AMPLITUDE_MASK;
		ret_val =
		    e1000_write_phy_reg(hw, M88E1000_PHY_EXT_CTRL, eeprom_data);
		if (ret_val)
			return ret_val;
	}

	return E1000_SUCCESS;
}

/**
 * e1000_setup_link - Configures flow control and link settings.
 * @hw: Struct containing variables accessed by shared code
 *
 * Determines which flow control settings to use. Calls the appropriate media-
 * specific link configuration function. Configures the flow control settings.
 * Assuming the adapter has a valid link partner, a valid link should be
 * established. Assumes the hardware has previously been reset and the
 * transmitter and receiver are not enabled.
 */
s32 e1000_setup_link(struct e1000_hw *hw)
{
	u32 ctrl_ext;
	s32 ret_val;
	u16 eeprom_data;

	DEBUGFUNC("e1000_setup_link");

	/* Read and store word 0x0F of the EEPROM. This word contains bits
	 * that determine the hardware's default PAUSE (flow control) mode,
	 * a bit that determines whether the HW defaults to enabling or
	 * disabling auto-negotiation, and the direction of the
	 * SW defined pins. If there is no SW over-ride of the flow
	 * control setting, then the variable hw->fc will
	 * be initialized based on a value in the EEPROM.
	 */
	if (hw->fc == E1000_FC_DEFAULT) {
		ret_val = e1000_read_eeprom(hw, EEPROM_INIT_CONTROL2_REG,
					    1, &eeprom_data);
		if (ret_val) {
			DEBUGOUT("EEPROM Read Error\n");
			return -E1000_ERR_EEPROM;
		}
		if ((eeprom_data & EEPROM_WORD0F_PAUSE_MASK) == 0)
			hw->fc = E1000_FC_NONE;
		else if ((eeprom_data & EEPROM_WORD0F_PAUSE_MASK) ==
			 EEPROM_WORD0F_ASM_DIR)
			hw->fc = E1000_FC_TX_PAUSE;
		else
			hw->fc = E1000_FC_FULL;
	}

	/* We want to save off the original Flow Control configuration just
	 * in case we get disconnected and then reconnected into a different
	 * hub or switch with different Flow Control capabilities.
	 */
	if (hw->mac_type == e1000_82542_rev2_0)
		hw->fc &= (~E1000_FC_TX_PAUSE);

	if ((hw->mac_type < e1000_82543) && (hw->report_tx_early == 1))
		hw->fc &= (~E1000_FC_RX_PAUSE);

	hw->original_fc = hw->fc;

	DEBUGOUT1("After fix-ups FlowControl is now = %x\n", hw->fc);

	/* Take the 4 bits from EEPROM word 0x0F that determine the initial
	 * polarity value for the SW controlled pins, and setup the
	 * Extended Device Control reg with that info.
	 * This is needed because one of the SW controlled pins is used for
	 * signal detection.  So this should be done before e1000_setup_pcs_link()
	 * or e1000_phy_setup() is called.
	 */
	if (hw->mac_type == e1000_82543) {
		ret_val = e1000_read_eeprom(hw, EEPROM_INIT_CONTROL2_REG,
					    1, &eeprom_data);
		if (ret_val) {
			DEBUGOUT("EEPROM Read Error\n");
			return -E1000_ERR_EEPROM;
		}
		ctrl_ext = ((eeprom_data & EEPROM_WORD0F_SWPDIO_EXT) <<
			    SWDPIO__EXT_SHIFT);
		ew32(CTRL_EXT, ctrl_ext);
	}

	/* Call the necessary subroutine to configure the link. */
	ret_val = (hw->media_type == e1000_media_type_copper) ?
	    e1000_setup_copper_link(hw) : e1000_setup_fiber_serdes_link(hw);

	/* Initialize the flow control address, type, and PAUSE timer
	 * registers to their default values.  This is done even if flow
	 * control is disabled, because it does not hurt anything to
	 * initialize these registers.
	 */
	DEBUGOUT
	    ("Initializing the Flow Control address, type and timer regs\n");

	ew32(FCT, FLOW_CONTROL_TYPE);
	ew32(FCAH, FLOW_CONTROL_ADDRESS_HIGH);
	ew32(FCAL, FLOW_CONTROL_ADDRESS_LOW);

	ew32(FCTTV, hw->fc_pause_time);

	/* Set the flow control receive threshold registers.  Normally,
	 * these registers will be set to a default threshold that may be
	 * adjusted later by the driver's runtime code.  However, if the
	 * ability to transmit pause frames in not enabled, then these
	 * registers will be set to 0.
	 */
	if (!(hw->fc & E1000_FC_TX_PAUSE)) {
		ew32(FCRTL, 0);
		ew32(FCRTH, 0);
	} else {
		/* We need to set up the Receive Threshold high and low water marks
		 * as well as (optionally) enabling the transmission of XON frames.
		 */
		if (hw->fc_send_xon) {
			ew32(FCRTL, (hw->fc_low_water | E1000_FCRTL_XONE));
			ew32(FCRTH, hw->fc_high_water);
		} else {
			ew32(FCRTL, hw->fc_low_water);
			ew32(FCRTH, hw->fc_high_water);
		}
	}
	return ret_val;
}

/**
 * e1000_setup_fiber_serdes_link - prepare fiber or serdes link
 * @hw: Struct containing variables accessed by shared code
 *
 * Manipulates Physical Coding Sublayer functions in order to configure
 * link. Assumes the hardware has been previously reset and the transmitter
 * and receiver are not enabled.
 */
static s32 e1000_setup_fiber_serdes_link(struct e1000_hw *hw)
{
	u32 ctrl;
	u32 status;
	u32 txcw = 0;
	u32 i;
	u32 signal = 0;
	s32 ret_val;

	DEBUGFUNC("e1000_setup_fiber_serdes_link");

	/* On adapters with a MAC newer than 82544, SWDP 1 will be
	 * set when the optics detect a signal. On older adapters, it will be
	 * cleared when there is a signal.  This applies to fiber media only.
	 * If we're on serdes media, adjust the output amplitude to value
	 * set in the EEPROM.
	 */
	ctrl = er32(CTRL);
	if (hw->media_type == e1000_media_type_fiber)
		signal = (hw->mac_type > e1000_82544) ? E1000_CTRL_SWDPIN1 : 0;

	ret_val = e1000_adjust_serdes_amplitude(hw);
	if (ret_val)
		return ret_val;

	/* Take the link out of reset */
	ctrl &= ~(E1000_CTRL_LRST);

	/* Adjust VCO speed to improve BER performance */
	ret_val = e1000_set_vco_speed(hw);
	if (ret_val)
		return ret_val;

	e1000_config_collision_dist(hw);

	/* Check for a software override of the flow control settings, and setup
	 * the device accordingly.  If auto-negotiation is enabled, then software
	 * will have to set the "PAUSE" bits to the correct value in the Tranmsit
	 * Config Word Register (TXCW) and re-start auto-negotiation.  However, if
	 * auto-negotiation is disabled, then software will have to manually
	 * configure the two flow control enable bits in the CTRL register.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause frames, but
	 *          not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames but we do
	 *          not support receiving pause frames).
	 *      3:  Both Rx and TX flow control (symmetric) are enabled.
	 */
	switch (hw->fc) {
	case E1000_FC_NONE:
		/* Flow control is completely disabled by a software over-ride. */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD);
		break;
	case E1000_FC_RX_PAUSE:
		/* RX Flow control is enabled and TX Flow control is disabled by a
		 * software over-ride. Since there really isn't a way to advertise
		 * that we are capable of RX Pause ONLY, we will advertise that we
		 * support both symmetric and asymmetric RX PAUSE. Later, we will
		 *  disable the adapter's ability to send PAUSE frames.
		 */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		break;
	case E1000_FC_TX_PAUSE:
		/* TX Flow control is enabled, and RX Flow control is disabled, by a
		 * software over-ride.
		 */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_ASM_DIR);
		break;
	case E1000_FC_FULL:
		/* Flow control (both RX and TX) is enabled by a software over-ride. */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
		break;
	}

	/* Since auto-negotiation is enabled, take the link out of reset (the link
	 * will be in reset, because we previously reset the chip). This will
	 * restart auto-negotiation.  If auto-negotiation is successful then the
	 * link-up status bit will be set and the flow control enable bits (RFCE
	 * and TFCE) will be set according to their negotiated value.
	 */
	DEBUGOUT("Auto-negotiation enabled\n");

	ew32(TXCW, txcw);
	ew32(CTRL, ctrl);
	E1000_WRITE_FLUSH();

	hw->txcw = txcw;
	msleep(1);

	/* If we have a signal (the cable is plugged in) then poll for a "Link-Up"
	 * indication in the Device Status Register.  Time-out if a link isn't
	 * seen in 500 milliseconds seconds (Auto-negotiation should complete in
	 * less than 500 milliseconds even if the other end is doing it in SW).
	 * For internal serdes, we just assume a signal is present, then poll.
	 */
	if (hw->media_type == e1000_media_type_internal_serdes ||
	    (er32(CTRL) & E1000_CTRL_SWDPIN1) == signal) {
		DEBUGOUT("Looking for Link\n");
		for (i = 0; i < (LINK_UP_TIMEOUT / 10); i++) {
			msleep(10);
			status = er32(STATUS);
			if (status & E1000_STATUS_LU)
				break;
		}
		if (i == (LINK_UP_TIMEOUT / 10)) {
			DEBUGOUT("Never got a valid link from auto-neg!!!\n");
			hw->autoneg_failed = 1;
			/* AutoNeg failed to achieve a link, so we'll call
			 * e1000_check_for_link. This routine will force the link up if
			 * we detect a signal. This will allow us to communicate with
			 * non-autonegotiating link partners.
			 */
			ret_val = e1000_check_for_link(hw);
			if (ret_val) {
				DEBUGOUT("Error while checking for link\n");
				return ret_val;
			}
			hw->autoneg_failed = 0;
		} else {
			hw->autoneg_failed = 0;
			DEBUGOUT("Valid Link Found\n");
		}
	} else {
		DEBUGOUT("No Signal Detected\n");
	}
	return E1000_SUCCESS;
}

/**
 * e1000_copper_link_preconfig - early configuration for copper
 * @hw: Struct containing variables accessed by shared code
 *
 * Make sure we have a valid PHY and change PHY mode before link setup.
 */
static s32 e1000_copper_link_preconfig(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;
	u16 phy_data;

	DEBUGFUNC("e1000_copper_link_preconfig");

	ctrl = er32(CTRL);
	/* With 82543, we need to force speed and duplex on the MAC equal to what
	 * the PHY speed and duplex configuration is. In addition, we need to
	 * perform a hardware reset on the PHY to take it out of reset.
	 */
	if (hw->mac_type > e1000_82543) {
		ctrl |= E1000_CTRL_SLU;
		ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
		ew32(CTRL, ctrl);
	} else {
		ctrl |=
		    (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX | E1000_CTRL_SLU);
		ew32(CTRL, ctrl);
		ret_val = e1000_phy_hw_reset(hw);
		if (ret_val)
			return ret_val;
	}

	/* Make sure we have a valid PHY */
	ret_val = e1000_detect_gig_phy(hw);
	if (ret_val) {
		DEBUGOUT("Error, did not detect valid phy.\n");
		return ret_val;
	}
	DEBUGOUT1("Phy ID = %x \n", hw->phy_id);

	/* Set PHY to class A mode (if necessary) */
	ret_val = e1000_set_phy_mode(hw);
	if (ret_val)
		return ret_val;

	if ((hw->mac_type == e1000_82545_rev_3) ||
	    (hw->mac_type == e1000_82546_rev_3)) {
		ret_val =
		    e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
		phy_data |= 0x00000008;
		ret_val =
		    e1000_write_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
	}

	if (hw->mac_type <= e1000_82543 ||
	    hw->mac_type == e1000_82541 || hw->mac_type == e1000_82547 ||
	    hw->mac_type == e1000_82541_rev_2
	    || hw->mac_type == e1000_82547_rev_2)
		hw->phy_reset_disable = false;

	return E1000_SUCCESS;
}

/**
 * e1000_copper_link_igp_setup - Copper link setup for e1000_phy_igp series.
 * @hw: Struct containing variables accessed by shared code
 */
static s32 e1000_copper_link_igp_setup(struct e1000_hw *hw)
{
	u32 led_ctrl;
	s32 ret_val;
	u16 phy_data;

	DEBUGFUNC("e1000_copper_link_igp_setup");

	if (hw->phy_reset_disable)
		return E1000_SUCCESS;

	ret_val = e1000_phy_reset(hw);
	if (ret_val) {
		DEBUGOUT("Error Resetting the PHY\n");
		return ret_val;
	}

	/* Wait 15ms for MAC to configure PHY from eeprom settings */
	msleep(15);
	/* Configure activity LED after PHY reset */
	led_ctrl = er32(LEDCTL);
	led_ctrl &= IGP_ACTIVITY_LED_MASK;
	led_ctrl |= (IGP_ACTIVITY_LED_ENABLE | IGP_LED3_MODE);
	ew32(LEDCTL, led_ctrl);

	/* The NVM settings will configure LPLU in D3 for IGP2 and IGP3 PHYs */
	if (hw->phy_type == e1000_phy_igp) {
		/* disable lplu d3 during driver init */
		ret_val = e1000_set_d3_lplu_state(hw, false);
		if (ret_val) {
			DEBUGOUT("Error Disabling LPLU D3\n");
			return ret_val;
		}
	}

	/* Configure mdi-mdix settings */
	ret_val = e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	if ((hw->mac_type == e1000_82541) || (hw->mac_type == e1000_82547)) {
		hw->dsp_config_state = e1000_dsp_config_disabled;
		/* Force MDI for earlier revs of the IGP PHY */
		phy_data &=
		    ~(IGP01E1000_PSCR_AUTO_MDIX |
		      IGP01E1000_PSCR_FORCE_MDI_MDIX);
		hw->mdix = 1;

	} else {
		hw->dsp_config_state = e1000_dsp_config_enabled;
		phy_data &= ~IGP01E1000_PSCR_AUTO_MDIX;

		switch (hw->mdix) {
		case 1:
			phy_data &= ~IGP01E1000_PSCR_FORCE_MDI_MDIX;
			break;
		case 2:
			phy_data |= IGP01E1000_PSCR_FORCE_MDI_MDIX;
			break;
		case 0:
		default:
			phy_data |= IGP01E1000_PSCR_AUTO_MDIX;
			break;
		}
	}
	ret_val = e1000_write_phy_reg(hw, IGP01E1000_PHY_PORT_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	/* set auto-master slave resolution settings */
	if (hw->autoneg) {
		e1000_ms_type phy_ms_setting = hw->master_slave;

		if (hw->ffe_config_state == e1000_ffe_config_active)
			hw->ffe_config_state = e1000_ffe_config_enabled;

		if (hw->dsp_config_state == e1000_dsp_config_activated)
			hw->dsp_config_state = e1000_dsp_config_enabled;

		/* when autonegotiation advertisement is only 1000Mbps then we
		 * should disable SmartSpeed and enable Auto MasterSlave
		 * resolution as hardware default. */
		if (hw->autoneg_advertised == ADVERTISE_1000_FULL) {
			/* Disable SmartSpeed */
			ret_val =
			    e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
					       &phy_data);
			if (ret_val)
				return ret_val;
			phy_data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val =
			    e1000_write_phy_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
						phy_data);
			if (ret_val)
				return ret_val;
			/* Set auto Master/Slave resolution process */
			ret_val =
			    e1000_read_phy_reg(hw, PHY_1000T_CTRL, &phy_data);
			if (ret_val)
				return ret_val;
			phy_data &= ~CR_1000T_MS_ENABLE;
			ret_val =
			    e1000_write_phy_reg(hw, PHY_1000T_CTRL, phy_data);
			if (ret_val)
				return ret_val;
		}

		ret_val = e1000_read_phy_reg(hw, PHY_1000T_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		/* load defaults for future use */
		hw->original_master_slave = (phy_data & CR_1000T_MS_ENABLE) ?
		    ((phy_data & CR_1000T_MS_VALUE) ?
		     e1000_ms_force_master :
		     e1000_ms_force_slave) : e1000_ms_auto;

		switch (phy_ms_setting) {
		case e1000_ms_force_master:
			phy_data |= (CR_1000T_MS_ENABLE | CR_1000T_MS_VALUE);
			break;
		case e1000_ms_force_slave:
			phy_data |= CR_1000T_MS_ENABLE;
			phy_data &= ~(CR_1000T_MS_VALUE);
			break;
		case e1000_ms_auto:
			phy_data &= ~CR_1000T_MS_ENABLE;
		default:
			break;
		}
		ret_val = e1000_write_phy_reg(hw, PHY_1000T_CTRL, phy_data);
		if (ret_val)
			return ret_val;
	}

	return E1000_SUCCESS;
}

/**
 * e1000_copper_link_mgp_setup - Copper link setup for e1000_phy_m88 series.
 * @hw: Struct containing variables accessed by shared code
 */
static s32 e1000_copper_link_mgp_setup(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 phy_data;

	DEBUGFUNC("e1000_copper_link_mgp_setup");

	if (hw->phy_reset_disable)
		return E1000_SUCCESS;

	/* Enable CRS on TX. This must be set for half-duplex operation. */
	ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;

	/* Options:
	 *   MDI/MDI-X = 0 (default)
	 *   0 - Auto for all speeds
	 *   1 - MDI mode
	 *   2 - MDI-X mode
	 *   3 - Auto for 1000Base-T only (MDI-X for 10/100Base-T modes)
	 */
	phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;

	switch (hw->mdix) {
	case 1:
		phy_data |= M88E1000_PSCR_MDI_MANUAL_MODE;
		break;
	case 2:
		phy_data |= M88E1000_PSCR_MDIX_MANUAL_MODE;
		break;
	case 3:
		phy_data |= M88E1000_PSCR_AUTO_X_1000T;
		break;
	case 0:
	default:
		phy_data |= M88E1000_PSCR_AUTO_X_MODE;
		break;
	}

	/* Options:
	 *   disable_polarity_correction = 0 (default)
	 *       Automatic Correction for Reversed Cable Polarity
	 *   0 - Disabled
	 *   1 - Enabled
	 */
	phy_data &= ~M88E1000_PSCR_POLARITY_REVERSAL;
	if (hw->disable_polarity_correction == 1)
		phy_data |= M88E1000_PSCR_POLARITY_REVERSAL;
	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	if (hw->phy_revision < M88E1011_I_REV_4) {
		/* Force TX_CLK in the Extended PHY Specific Control Register
		 * to 25MHz clock.
		 */
		ret_val =
		    e1000_read_phy_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL,
				       &phy_data);
		if (ret_val)
			return ret_val;

		phy_data |= M88E1000_EPSCR_TX_CLK_25;

		if ((hw->phy_revision == E1000_REVISION_2) &&
		    (hw->phy_id == M88E1111_I_PHY_ID)) {
			/* Vidalia Phy, set the downshift counter to 5x */
			phy_data &= ~(M88EC018_EPSCR_DOWNSHIFT_COUNTER_MASK);
			phy_data |= M88EC018_EPSCR_DOWNSHIFT_COUNTER_5X;
			ret_val = e1000_write_phy_reg(hw,
						      M88E1000_EXT_PHY_SPEC_CTRL,
						      phy_data);
			if (ret_val)
				return ret_val;
		} else {
			/* Configure Master and Slave downshift values */
			phy_data &= ~(M88E1000_EPSCR_MASTER_DOWNSHIFT_MASK |
				      M88E1000_EPSCR_SLAVE_DOWNSHIFT_MASK);
			phy_data |= (M88E1000_EPSCR_MASTER_DOWNSHIFT_1X |
				     M88E1000_EPSCR_SLAVE_DOWNSHIFT_1X);
			ret_val = e1000_write_phy_reg(hw,
						      M88E1000_EXT_PHY_SPEC_CTRL,
						      phy_data);
			if (ret_val)
				return ret_val;
		}
	}

	/* SW Reset the PHY so all changes take effect */
	ret_val = e1000_phy_reset(hw);
	if (ret_val) {
		DEBUGOUT("Error Resetting the PHY\n");
		return ret_val;
	}

	return E1000_SUCCESS;
}

/**
 * e1000_copper_link_autoneg - setup auto-neg
 * @hw: Struct containing variables accessed by shared code
 *
 * Setup auto-negotiation and flow control advertisements,
 * and then perform auto-negotiation.
 */
static s32 e1000_copper_link_autoneg(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 phy_data;

	DEBUGFUNC("e1000_copper_link_autoneg");

	/* Perform some bounds checking on the hw->autoneg_advertised
	 * parameter.  If this variable is zero, then set it to the default.
	 */
	hw->autoneg_advertised &= AUTONEG_ADVERTISE_SPEED_DEFAULT;

	/* If autoneg_advertised is zero, we assume it was not defaulted
	 * by the calling code so we set to advertise full capability.
	 */
	if (hw->autoneg_advertised == 0)
		hw->autoneg_advertised = AUTONEG_ADVERTISE_SPEED_DEFAULT;

	DEBUGOUT("Reconfiguring auto-neg advertisement params\n");
	ret_val = e1000_phy_setup_autoneg(hw);
	if (ret_val) {
		DEBUGOUT("Error Setting up Auto-Negotiation\n");
		return ret_val;
	}
	DEBUGOUT("Restarting Auto-Neg\n");

	/* Restart auto-negotiation by setting the Auto Neg Enable bit and
	 * the Auto Neg Restart bit in the PHY control register.
	 */
	ret_val = e1000_read_phy_reg(hw, PHY_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data |= (MII_CR_AUTO_NEG_EN | MII_CR_RESTART_AUTO_NEG);
	ret_val = e1000_write_phy_reg(hw, PHY_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	/* Does the user want to wait for Auto-Neg to complete here, or
	 * check at a later time (for example, callback routine).
	 */
	if (hw->wait_autoneg_complete) {
		ret_val = e1000_wait_autoneg(hw);
		if (ret_val) {
			DEBUGOUT
			    ("Error while waiting for autoneg to complete\n");
			return ret_val;
		}
	}

	hw->get_link_status = true;

	return E1000_SUCCESS;
}

/**
 * e1000_copper_link_postconfig - post link setup
 * @hw: Struct containing variables accessed by shared code
 *
 * Config the MAC and the PHY after link is up.
 *   1) Set up the MAC to the current PHY speed/duplex
 *      if we are on 82543.  If we
 *      are on newer silicon, we only need to configure
 *      collision distance in the Transmit Control Register.
 *   2) Set up flow control on the MAC to that established with
 *      the link partner.
 *   3) Config DSP to improve Gigabit link quality for some PHY revisions.
 */
static s32 e1000_copper_link_postconfig(struct e1000_hw *hw)
{
	s32 ret_val;
	DEBUGFUNC("e1000_copper_link_postconfig");

	if (hw->mac_type >= e1000_82544) {
		e1000_config_collision_dist(hw);
	} else {
		ret_val = e1000_config_mac_to_phy(hw);
		if (ret_val) {
			DEBUGOUT("Error configuring MAC to PHY settings\n");
			return ret_val;
		}
	}
	ret_val = e1000_config_fc_after_link_up(hw);
	if (ret_val) {
		DEBUGOUT("Error Configuring Flow Control\n");
		return ret_val;
	}

	/* Config DSP to improve Giga link quality */
	if (hw->phy_type == e1000_phy_igp) {
		ret_val = e1000_config_dsp_after_link_change(hw, true);
		if (ret_val) {
			DEBUGOUT("Error Configuring DSP after link up\n");
			return ret_val;
		}
	}

	return E1000_SUCCESS;
}

/**
 * e1000_setup_copper_link - phy/speed/duplex setting
 * @hw: Struct containing variables accessed by shared code
 *
 * Detects which PHY is present and sets up the speed and duplex
 */
static s32 e1000_setup_copper_link(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 i;
	u16 phy_data;

	DEBUGFUNC("e1000_setup_copper_link");

	/* Check if it is a valid PHY and set PHY mode if necessary. */
	ret_val = e1000_copper_link_preconfig(hw);
	if (ret_val)
		return ret_val;

	if (hw->phy_type == e1000_phy_igp) {
		ret_val = e1000_copper_link_igp_setup(hw);
		if (ret_val)
			return ret_val;
	} else if (hw->phy_type == e1000_phy_m88) {
		ret_val = e1000_copper_link_mgp_setup(hw);
		if (ret_val)
			return ret_val;
	}

	if (hw->autoneg) {
		/* Setup autoneg and flow control advertisement
		 * and perform autonegotiation */
		ret_val = e1000_copper_link_autoneg(hw);
		if (ret_val)
			return ret_val;
	} else {
		/* PHY will be set to 10H, 10F, 100H,or 100F
		 * depending on value from forced_speed_duplex. */
		DEBUGOUT("Forcing speed and duplex\n");
		ret_val = e1000_phy_force_speed_duplex(hw);
		if (ret_val) {
			DEBUGOUT("Error Forcing Speed and Duplex\n");
			return ret_val;
		}
	}

	/* Check link status. Wait up to 100 microseconds for link to become
	 * valid.
	 */
	for (i = 0; i < 10; i++) {
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
		if (ret_val)
			return ret_val;
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
		if (ret_val)
			return ret_val;

		if (phy_data & MII_SR_LINK_STATUS) {
			/* Config the MAC and PHY after link is up */
			ret_val = e1000_copper_link_postconfig(hw);
			if (ret_val)
				return ret_val;

			DEBUGOUT("Valid link established!!!\n");
			return E1000_SUCCESS;
		}
		udelay(10);
	}

	DEBUGOUT("Unable to establish link!!!\n");
	return E1000_SUCCESS;
}

/**
 * e1000_phy_setup_autoneg - phy settings
 * @hw: Struct containing variables accessed by shared code
 *
 * Configures PHY autoneg and flow control advertisement settings
 */
s32 e1000_phy_setup_autoneg(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 mii_autoneg_adv_reg;
	u16 mii_1000t_ctrl_reg;

	DEBUGFUNC("e1000_phy_setup_autoneg");

	/* Read the MII Auto-Neg Advertisement Register (Address 4). */
	ret_val = e1000_read_phy_reg(hw, PHY_AUTONEG_ADV, &mii_autoneg_adv_reg);
	if (ret_val)
		return ret_val;

	/* Read the MII 1000Base-T Control Register (Address 9). */
	ret_val =
	    e1000_read_phy_reg(hw, PHY_1000T_CTRL, &mii_1000t_ctrl_reg);
	if (ret_val)
		return ret_val;

	/* Need to parse both autoneg_advertised and fc and set up
	 * the appropriate PHY registers.  First we will parse for
	 * autoneg_advertised software override.  Since we can advertise
	 * a plethora of combinations, we need to check each bit
	 * individually.
	 */

	/* First we clear all the 10/100 mb speed bits in the Auto-Neg
	 * Advertisement Register (Address 4) and the 1000 mb speed bits in
	 * the  1000Base-T Control Register (Address 9).
	 */
	mii_autoneg_adv_reg &= ~REG4_SPEED_MASK;
	mii_1000t_ctrl_reg &= ~REG9_SPEED_MASK;

	DEBUGOUT1("autoneg_advertised %x\n", hw->autoneg_advertised);

	/* Do we want to advertise 10 Mb Half Duplex? */
	if (hw->autoneg_advertised & ADVERTISE_10_HALF) {
		DEBUGOUT("Advertise 10mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_HD_CAPS;
	}

	/* Do we want to advertise 10 Mb Full Duplex? */
	if (hw->autoneg_advertised & ADVERTISE_10_FULL) {
		DEBUGOUT("Advertise 10mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_FD_CAPS;
	}

	/* Do we want to advertise 100 Mb Half Duplex? */
	if (hw->autoneg_advertised & ADVERTISE_100_HALF) {
		DEBUGOUT("Advertise 100mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_HD_CAPS;
	}

	/* Do we want to advertise 100 Mb Full Duplex? */
	if (hw->autoneg_advertised & ADVERTISE_100_FULL) {
		DEBUGOUT("Advertise 100mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_FD_CAPS;
	}

	/* We do not allow the Phy to advertise 1000 Mb Half Duplex */
	if (hw->autoneg_advertised & ADVERTISE_1000_HALF) {
		DEBUGOUT
		    ("Advertise 1000mb Half duplex requested, request denied!\n");
	}

	/* Do we want to advertise 1000 Mb Full Duplex? */
	if (hw->autoneg_advertised & ADVERTISE_1000_FULL) {
		DEBUGOUT("Advertise 1000mb Full duplex\n");
		mii_1000t_ctrl_reg |= CR_1000T_FD_CAPS;
	}

	/* Check for a software override of the flow control settings, and
	 * setup the PHY advertisement registers accordingly.  If
	 * auto-negotiation is enabled, then software will have to set the
	 * "PAUSE" bits to the correct value in the Auto-Negotiation
	 * Advertisement Register (PHY_AUTONEG_ADV) and re-start auto-negotiation.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause frames
	 *          but not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames
	 *          but we do not support receiving pause frames).
	 *      3:  Both Rx and TX flow control (symmetric) are enabled.
	 *  other:  No software override.  The flow control configuration
	 *          in the EEPROM is used.
	 */
	switch (hw->fc) {
	case E1000_FC_NONE:	/* 0 */
		/* Flow control (RX & TX) is completely disabled by a
		 * software over-ride.
		 */
		mii_autoneg_adv_reg &= ~(NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	case E1000_FC_RX_PAUSE:	/* 1 */
		/* RX Flow control is enabled, and TX Flow control is
		 * disabled, by a software over-ride.
		 */
		/* Since there really isn't a way to advertise that we are
		 * capable of RX Pause ONLY, we will advertise that we
		 * support both symmetric and asymmetric RX PAUSE.  Later
		 * (in e1000_config_fc_after_link_up) we will disable the
		 *hw's ability to send PAUSE frames.
		 */
		mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	case E1000_FC_TX_PAUSE:	/* 2 */
		/* TX Flow control is enabled, and RX Flow control is
		 * disabled, by a software over-ride.
		 */
		mii_autoneg_adv_reg |= NWAY_AR_ASM_DIR;
		mii_autoneg_adv_reg &= ~NWAY_AR_PAUSE;
		break;
	case E1000_FC_FULL:	/* 3 */
		/* Flow control (both RX and TX) is enabled by a software
		 * over-ride.
		 */
		mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
	}

	ret_val = e1000_write_phy_reg(hw, PHY_AUTONEG_ADV, mii_autoneg_adv_reg);
	if (ret_val)
		return ret_val;

	DEBUGOUT1("Auto-Neg Advertising %x\n", mii_autoneg_adv_reg);

	ret_val = e1000_write_phy_reg(hw, PHY_1000T_CTRL, mii_1000t_ctrl_reg);
	if (ret_val)
		return ret_val;

	return E1000_SUCCESS;
}

/**
 * e1000_phy_force_speed_duplex - force link settings
 * @hw: Struct containing variables accessed by shared code
 *
 * Force PHY speed and duplex settings to hw->forced_speed_duplex
 */
static s32 e1000_phy_force_speed_duplex(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;
	u16 mii_ctrl_reg;
	u16 mii_status_reg;
	u16 phy_data;
	u16 i;

	DEBUGFUNC("e1000_phy_force_speed_duplex");

	/* Turn off Flow control if we are forcing speed and duplex. */
	hw->fc = E1000_FC_NONE;

	DEBUGOUT1("hw->fc = %d\n", hw->fc);

	/* Read the Device Control Register. */
	ctrl = er32(CTRL);

	/* Set the bits to Force Speed and Duplex in the Device Ctrl Reg. */
	ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	ctrl &= ~(DEVICE_SPEED_MASK);

	/* Clear the Auto Speed Detect Enable bit. */
	ctrl &= ~E1000_CTRL_ASDE;

	/* Read the MII Control Register. */
	ret_val = e1000_read_phy_reg(hw, PHY_CTRL, &mii_ctrl_reg);
	if (ret_val)
		return ret_val;

	/* We need to disable autoneg in order to force link and duplex. */

	mii_ctrl_reg &= ~MII_CR_AUTO_NEG_EN;

	/* Are we forcing Full or Half Duplex? */
	if (hw->forced_speed_duplex == e1000_100_full ||
	    hw->forced_speed_duplex == e1000_10_full) {
		/* We want to force full duplex so we SET the full duplex bits in the
		 * Device and MII Control Registers.
		 */
		ctrl |= E1000_CTRL_FD;
		mii_ctrl_reg |= MII_CR_FULL_DUPLEX;
		DEBUGOUT("Full Duplex\n");
	} else {
		/* We want to force half duplex so we CLEAR the full duplex bits in
		 * the Device and MII Control Registers.
		 */
		ctrl &= ~E1000_CTRL_FD;
		mii_ctrl_reg &= ~MII_CR_FULL_DUPLEX;
		DEBUGOUT("Half Duplex\n");
	}

	/* Are we forcing 100Mbps??? */
	if (hw->forced_speed_duplex == e1000_100_full ||
	    hw->forced_speed_duplex == e1000_100_half) {
		/* Set the 100Mb bit and turn off the 1000Mb and 10Mb bits. */
		ctrl |= E1000_CTRL_SPD_100;
		mii_ctrl_reg |= MII_CR_SPEED_100;
		mii_ctrl_reg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_10);
		DEBUGOUT("Forcing 100mb ");
	} else {
		/* Set the 10Mb bit and turn off the 1000Mb and 100Mb bits. */
		ctrl &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);
		mii_ctrl_reg |= MII_CR_SPEED_10;
		mii_ctrl_reg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_100);
		DEBUGOUT("Forcing 10mb ");
	}

	e1000_config_collision_dist(hw);

	/* Write the configured values back to the Device Control Reg. */
	ew32(CTRL, ctrl);

	if (hw->phy_type == e1000_phy_m88) {
		ret_val =
		    e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		/* Clear Auto-Crossover to force MDI manually. M88E1000 requires MDI
		 * forced whenever speed are duplex are forced.
		 */
		phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;
		ret_val =
		    e1000_write_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
		if (ret_val)
			return ret_val;

		DEBUGOUT1("M88E1000 PSCR: %x \n", phy_data);

		/* Need to reset the PHY or these changes will be ignored */
		mii_ctrl_reg |= MII_CR_RESET;

		/* Disable MDI-X support for 10/100 */
	} else {
		/* Clear Auto-Crossover to force MDI manually.  IGP requires MDI
		 * forced whenever speed or duplex are forced.
		 */
		ret_val =
		    e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		phy_data &= ~IGP01E1000_PSCR_AUTO_MDIX;
		phy_data &= ~IGP01E1000_PSCR_FORCE_MDI_MDIX;

		ret_val =
		    e1000_write_phy_reg(hw, IGP01E1000_PHY_PORT_CTRL, phy_data);
		if (ret_val)
			return ret_val;
	}

	/* Write back the modified PHY MII control register. */
	ret_val = e1000_write_phy_reg(hw, PHY_CTRL, mii_ctrl_reg);
	if (ret_val)
		return ret_val;

	udelay(1);

	/* The wait_autoneg_complete flag may be a little misleading here.
	 * Since we are forcing speed and duplex, Auto-Neg is not enabled.
	 * But we do want to delay for a period while forcing only so we
	 * don't generate false No Link messages.  So we will wait here
	 * only if the user has set wait_autoneg_complete to 1, which is
	 * the default.
	 */
	if (hw->wait_autoneg_complete) {
		/* We will wait for autoneg to complete. */
		DEBUGOUT("Waiting for forced speed/duplex link.\n");
		mii_status_reg = 0;

		/* We will wait for autoneg to complete or 4.5 seconds to expire. */
		for (i = PHY_FORCE_TIME; i > 0; i--) {
			/* Read the MII Status Register and wait for Auto-Neg Complete bit
			 * to be set.
			 */
			ret_val =
			    e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
			if (ret_val)
				return ret_val;

			ret_val =
			    e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
			if (ret_val)
				return ret_val;

			if (mii_status_reg & MII_SR_LINK_STATUS)
				break;
			msleep(100);
		}
		if ((i == 0) && (hw->phy_type == e1000_phy_m88)) {
			/* We didn't get link.  Reset the DSP and wait again for link. */
			ret_val = e1000_phy_reset_dsp(hw);
			if (ret_val) {
				DEBUGOUT("Error Resetting PHY DSP\n");
				return ret_val;
			}
		}
		/* This loop will early-out if the link condition has been met.  */
		for (i = PHY_FORCE_TIME; i > 0; i--) {
			if (mii_status_reg & MII_SR_LINK_STATUS)
				break;
			msleep(100);
			/* Read the MII Status Register and wait for Auto-Neg Complete bit
			 * to be set.
			 */
			ret_val =
			    e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
			if (ret_val)
				return ret_val;

			ret_val =
			    e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
			if (ret_val)
				return ret_val;
		}
	}

	if (hw->phy_type == e1000_phy_m88) {
		/* Because we reset the PHY above, we need to re-force TX_CLK in the
		 * Extended PHY Specific Control Register to 25MHz clock.  This value
		 * defaults back to a 2.5MHz clock when the PHY is reset.
		 */
		ret_val =
		    e1000_read_phy_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL,
				       &phy_data);
		if (ret_val)
			return ret_val;

		phy_data |= M88E1000_EPSCR_TX_CLK_25;
		ret_val =
		    e1000_write_phy_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL,
					phy_data);
		if (ret_val)
			return ret_val;

		/* In addition, because of the s/w reset above, we need to enable CRS on
		 * TX.  This must be set for both full and half duplex operation.
		 */
		ret_val =
		    e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;
		ret_val =
		    e1000_write_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
		if (ret_val)
			return ret_val;

		if ((hw->mac_type == e1000_82544 || hw->mac_type == e1000_82543)
		    && (!hw->autoneg)
		    && (hw->forced_speed_duplex == e1000_10_full
			|| hw->forced_speed_duplex == e1000_10_half)) {
			ret_val = e1000_polarity_reversal_workaround(hw);
			if (ret_val)
				return ret_val;
		}
	}
	return E1000_SUCCESS;
}

/**
 * e1000_config_collision_dist - set collision distance register
 * @hw: Struct containing variables accessed by shared code
 *
 * Sets the collision distance in the Transmit Control register.
 * Link should have been established previously. Reads the speed and duplex
 * information from the Device Status register.
 */
void e1000_config_collision_dist(struct e1000_hw *hw)
{
	u32 tctl, coll_dist;

	DEBUGFUNC("e1000_config_collision_dist");

	if (hw->mac_type < e1000_82543)
		coll_dist = E1000_COLLISION_DISTANCE_82542;
	else
		coll_dist = E1000_COLLISION_DISTANCE;

	tctl = er32(TCTL);

	tctl &= ~E1000_TCTL_COLD;
	tctl |= coll_dist << E1000_COLD_SHIFT;

	ew32(TCTL, tctl);
	E1000_WRITE_FLUSH();
}

/**
 * e1000_config_mac_to_phy - sync phy and mac settings
 * @hw: Struct containing variables accessed by shared code
 * @mii_reg: data to write to the MII control register
 *
 * Sets MAC speed and duplex settings to reflect the those in the PHY
 * The contents of the PHY register containing the needed information need to
 * be passed in.
 */
static s32 e1000_config_mac_to_phy(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;
	u16 phy_data;

	DEBUGFUNC("e1000_config_mac_to_phy");

	/* 82544 or newer MAC, Auto Speed Detection takes care of
	 * MAC speed/duplex configuration.*/
	if (hw->mac_type >= e1000_82544)
		return E1000_SUCCESS;

	/* Read the Device Control Register and set the bits to Force Speed
	 * and Duplex.
	 */
	ctrl = er32(CTRL);
	ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	ctrl &= ~(E1000_CTRL_SPD_SEL | E1000_CTRL_ILOS);

	/* Set up duplex in the Device Control and Transmit Control
	 * registers depending on negotiated values.
	 */
	ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	if (phy_data & M88E1000_PSSR_DPLX)
		ctrl |= E1000_CTRL_FD;
	else
		ctrl &= ~E1000_CTRL_FD;

	e1000_config_collision_dist(hw);

	/* Set up speed in the Device Control register depending on
	 * negotiated values.
	 */
	if ((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_1000MBS)
		ctrl |= E1000_CTRL_SPD_1000;
	else if ((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_100MBS)
		ctrl |= E1000_CTRL_SPD_100;

	/* Write the configured values back to the Device Control Reg. */
	ew32(CTRL, ctrl);
	return E1000_SUCCESS;
}

/**
 * e1000_force_mac_fc - force flow control settings
 * @hw: Struct containing variables accessed by shared code
 *
 * Forces the MAC's flow control settings.
 * Sets the TFCE and RFCE bits in the device control register to reflect
 * the adapter settings. TFCE and RFCE need to be explicitly set by
 * software when a Copper PHY is used because autonegotiation is managed
 * by the PHY rather than the MAC. Software must also configure these
 * bits when link is forced on a fiber connection.
 */
s32 e1000_force_mac_fc(struct e1000_hw *hw)
{
	u32 ctrl;

	DEBUGFUNC("e1000_force_mac_fc");

	/* Get the current configuration of the Device Control Register */
	ctrl = er32(CTRL);

	/* Because we didn't get link via the internal auto-negotiation
	 * mechanism (we either forced link or we got link via PHY
	 * auto-neg), we have to manually enable/disable transmit an
	 * receive flow control.
	 *
	 * The "Case" statement below enables/disable flow control
	 * according to the "hw->fc" parameter.
	 *
	 * The possible values of the "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause
	 *          frames but not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames
	 *          frames but we do not receive pause frames).
	 *      3:  Both Rx and TX flow control (symmetric) is enabled.
	 *  other:  No other values should be possible at this point.
	 */

	switch (hw->fc) {
	case E1000_FC_NONE:
		ctrl &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
		break;
	case E1000_FC_RX_PAUSE:
		ctrl &= (~E1000_CTRL_TFCE);
		ctrl |= E1000_CTRL_RFCE;
		break;
	case E1000_FC_TX_PAUSE:
		ctrl &= (~E1000_CTRL_RFCE);
		ctrl |= E1000_CTRL_TFCE;
		break;
	case E1000_FC_FULL:
		ctrl |= (E1000_CTRL_TFCE | E1000_CTRL_RFCE);
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
	}

	/* Disable TX Flow Control for 82542 (rev 2.0) */
	if (hw->mac_type == e1000_82542_rev2_0)
		ctrl &= (~E1000_CTRL_TFCE);

	ew32(CTRL, ctrl);
	return E1000_SUCCESS;
}

/**
 * e1000_config_fc_after_link_up - configure flow control after autoneg
 * @hw: Struct containing variables accessed by shared code
 *
 * Configures flow control settings after link is established
 * Should be called immediately after a valid link has been established.
 * Forces MAC flow control settings if link was forced. When in MII/GMII mode
 * and autonegotiation is enabled, the MAC flow control settings will be set
 * based on the flow control negotiated by the PHY. In TBI mode, the TFCE
 * and RFCE bits will be automatically set to the negotiated flow control mode.
 */
static s32 e1000_config_fc_after_link_up(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 mii_status_reg;
	u16 mii_nway_adv_reg;
	u16 mii_nway_lp_ability_reg;
	u16 speed;
	u16 duplex;

	DEBUGFUNC("e1000_config_fc_after_link_up");

	/* Check for the case where we have fiber media and auto-neg failed
	 * so we had to force link.  In this case, we need to force the
	 * configuration of the MAC to match the "fc" parameter.
	 */
	if (((hw->media_type == e1000_media_type_fiber) && (hw->autoneg_failed))
	    || ((hw->media_type == e1000_media_type_internal_serdes)
		&& (hw->autoneg_failed))
	    || ((hw->media_type == e1000_media_type_copper)
		&& (!hw->autoneg))) {
		ret_val = e1000_force_mac_fc(hw);
		if (ret_val) {
			DEBUGOUT("Error forcing flow control settings\n");
			return ret_val;
		}
	}

	/* Check for the case where we have copper media and auto-neg is
	 * enabled.  In this case, we need to check and see if Auto-Neg
	 * has completed, and if so, how the PHY and link partner has
	 * flow control configured.
	 */
	if ((hw->media_type == e1000_media_type_copper) && hw->autoneg) {
		/* Read the MII Status Register and check to see if AutoNeg
		 * has completed.  We read this twice because this reg has
		 * some "sticky" (latched) bits.
		 */
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;

		if (mii_status_reg & MII_SR_AUTONEG_COMPLETE) {
			/* The AutoNeg process has completed, so we now need to
			 * read both the Auto Negotiation Advertisement Register
			 * (Address 4) and the Auto_Negotiation Base Page Ability
			 * Register (Address 5) to determine how flow control was
			 * negotiated.
			 */
			ret_val = e1000_read_phy_reg(hw, PHY_AUTONEG_ADV,
						     &mii_nway_adv_reg);
			if (ret_val)
				return ret_val;
			ret_val = e1000_read_phy_reg(hw, PHY_LP_ABILITY,
						     &mii_nway_lp_ability_reg);
			if (ret_val)
				return ret_val;

			/* Two bits in the Auto Negotiation Advertisement Register
			 * (Address 4) and two bits in the Auto Negotiation Base
			 * Page Ability Register (Address 5) determine flow control
			 * for both the PHY and the link partner.  The following
			 * table, taken out of the IEEE 802.3ab/D6.0 dated March 25,
			 * 1999, describes these PAUSE resolution bits and how flow
			 * control is determined based upon these settings.
			 * NOTE:  DC = Don't Care
			 *
			 *   LOCAL DEVICE  |   LINK PARTNER
			 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | NIC Resolution
			 *-------|---------|-------|---------|--------------------
			 *   0   |    0    |  DC   |   DC    | E1000_FC_NONE
			 *   0   |    1    |   0   |   DC    | E1000_FC_NONE
			 *   0   |    1    |   1   |    0    | E1000_FC_NONE
			 *   0   |    1    |   1   |    1    | E1000_FC_TX_PAUSE
			 *   1   |    0    |   0   |   DC    | E1000_FC_NONE
			 *   1   |   DC    |   1   |   DC    | E1000_FC_FULL
			 *   1   |    1    |   0   |    0    | E1000_FC_NONE
			 *   1   |    1    |   0   |    1    | E1000_FC_RX_PAUSE
			 *
			 */
			/* Are both PAUSE bits set to 1?  If so, this implies
			 * Symmetric Flow Control is enabled at both ends.  The
			 * ASM_DIR bits are irrelevant per the spec.
			 *
			 * For Symmetric Flow Control:
			 *
			 *   LOCAL DEVICE  |   LINK PARTNER
			 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
			 *-------|---------|-------|---------|--------------------
			 *   1   |   DC    |   1   |   DC    | E1000_FC_FULL
			 *
			 */
			if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
			    (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE)) {
				/* Now we need to check if the user selected RX ONLY
				 * of pause frames.  In this case, we had to advertise
				 * FULL flow control because we could not advertise RX
				 * ONLY. Hence, we must now check to see if we need to
				 * turn OFF  the TRANSMISSION of PAUSE frames.
				 */
				if (hw->original_fc == E1000_FC_FULL) {
					hw->fc = E1000_FC_FULL;
					DEBUGOUT("Flow Control = FULL.\n");
				} else {
					hw->fc = E1000_FC_RX_PAUSE;
					DEBUGOUT
					    ("Flow Control = RX PAUSE frames only.\n");
				}
			}
			/* For receiving PAUSE frames ONLY.
			 *
			 *   LOCAL DEVICE  |   LINK PARTNER
			 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
			 *-------|---------|-------|---------|--------------------
			 *   0   |    1    |   1   |    1    | E1000_FC_TX_PAUSE
			 *
			 */
			else if (!(mii_nway_adv_reg & NWAY_AR_PAUSE) &&
				 (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
				 (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
				 (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR))
			{
				hw->fc = E1000_FC_TX_PAUSE;
				DEBUGOUT
				    ("Flow Control = TX PAUSE frames only.\n");
			}
			/* For transmitting PAUSE frames ONLY.
			 *
			 *   LOCAL DEVICE  |   LINK PARTNER
			 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
			 *-------|---------|-------|---------|--------------------
			 *   1   |    1    |   0   |    1    | E1000_FC_RX_PAUSE
			 *
			 */
			else if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
				 (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
				 !(mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
				 (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR))
			{
				hw->fc = E1000_FC_RX_PAUSE;
				DEBUGOUT
				    ("Flow Control = RX PAUSE frames only.\n");
			}
			/* Per the IEEE spec, at this point flow control should be
			 * disabled.  However, we want to consider that we could
			 * be connected to a legacy switch that doesn't advertise
			 * desired flow control, but can be forced on the link
			 * partner.  So if we advertised no flow control, that is
			 * what we will resolve to.  If we advertised some kind of
			 * receive capability (Rx Pause Only or Full Flow Control)
			 * and the link partner advertised none, we will configure
			 * ourselves to enable Rx Flow Control only.  We can do
			 * this safely for two reasons:  If the link partner really
			 * didn't want flow control enabled, and we enable Rx, no
			 * harm done since we won't be receiving any PAUSE frames
			 * anyway.  If the intent on the link partner was to have
			 * flow control enabled, then by us enabling RX only, we
			 * can at least receive pause frames and process them.
			 * This is a good idea because in most cases, since we are
			 * predominantly a server NIC, more times than not we will
			 * be asked to delay transmission of packets than asking
			 * our link partner to pause transmission of frames.
			 */
			else if ((hw->original_fc == E1000_FC_NONE ||
				  hw->original_fc == E1000_FC_TX_PAUSE) ||
				 hw->fc_strict_ieee) {
				hw->fc = E1000_FC_NONE;
				DEBUGOUT("Flow Control = NONE.\n");
			} else {
				hw->fc = E1000_FC_RX_PAUSE;
				DEBUGOUT
				    ("Flow Control = RX PAUSE frames only.\n");
			}

			/* Now we need to do one last check...  If we auto-
			 * negotiated to HALF DUPLEX, flow control should not be
			 * enabled per IEEE 802.3 spec.
			 */
			ret_val =
			    e1000_get_speed_and_duplex(hw, &speed, &duplex);
			if (ret_val) {
				DEBUGOUT
				    ("Error getting link speed and duplex\n");
				return ret_val;
			}

			if (duplex == HALF_DUPLEX)
				hw->fc = E1000_FC_NONE;

			/* Now we call a subroutine to actually force the MAC
			 * controller to use the correct flow control settings.
			 */
			ret_val = e1000_force_mac_fc(hw);
			if (ret_val) {
				DEBUGOUT
				    ("Error forcing flow control settings\n");
				return ret_val;
			}
		} else {
			DEBUGOUT
			    ("Copper PHY and Auto Neg has not completed.\n");
		}
	}
	return E1000_SUCCESS;
}

/**
 * e1000_check_for_serdes_link_generic - Check for link (Serdes)
 * @hw: pointer to the HW structure
 *
 * Checks for link up on the hardware.  If link is not up and we have
 * a signal, then we need to force link up.
 */
static s32 e1000_check_for_serdes_link_generic(struct e1000_hw *hw)
{
	u32 rxcw;
	u32 ctrl;
	u32 status;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_check_for_serdes_link_generic");

	ctrl = er32(CTRL);
	status = er32(STATUS);
	rxcw = er32(RXCW);

	/*
	 * If we don't have link (auto-negotiation failed or link partner
	 * cannot auto-negotiate), and our link partner is not trying to
	 * auto-negotiate with us (we are receiving idles or data),
	 * we need to force link up. We also need to give auto-negotiation
	 * time to complete.
	 */
	/* (ctrl & E1000_CTRL_SWDPIN1) == 1 == have signal */
	if ((!(status & E1000_STATUS_LU)) && (!(rxcw & E1000_RXCW_C))) {
		if (hw->autoneg_failed == 0) {
			hw->autoneg_failed = 1;
			goto out;
		}
		DEBUGOUT("NOT RXing /C/, disable AutoNeg and force link.\n");

		/* Disable auto-negotiation in the TXCW register */
		ew32(TXCW, (hw->txcw & ~E1000_TXCW_ANE));

		/* Force link-up and also force full-duplex. */
		ctrl = er32(CTRL);
		ctrl |= (E1000_CTRL_SLU | E1000_CTRL_FD);
		ew32(CTRL, ctrl);

		/* Configure Flow Control after forcing link up. */
		ret_val = e1000_config_fc_after_link_up(hw);
		if (ret_val) {
			DEBUGOUT("Error configuring flow control\n");
			goto out;
		}
	} else if ((ctrl & E1000_CTRL_SLU) && (rxcw & E1000_RXCW_C)) {
		/*
		 * If we are forcing link and we are receiving /C/ ordered
		 * sets, re-enable auto-negotiation in the TXCW register
		 * and disable forced link in the Device Control register
		 * in an attempt to auto-negotiate with our link partner.
		 */
		DEBUGOUT("RXing /C/, enable AutoNeg and stop forcing link.\n");
		ew32(TXCW, hw->txcw);
		ew32(CTRL, (ctrl & ~E1000_CTRL_SLU));

		hw->serdes_has_link = true;
	} else if (!(E1000_TXCW_ANE & er32(TXCW))) {
		/*
		 * If we force link for non-auto-negotiation switch, check
		 * link status based on MAC synchronization for internal
		 * serdes media type.
		 */
		/* SYNCH bit and IV bit are sticky. */
		udelay(10);
		rxcw = er32(RXCW);
		if (rxcw & E1000_RXCW_SYNCH) {
			if (!(rxcw & E1000_RXCW_IV)) {
				hw->serdes_has_link = true;
				DEBUGOUT("SERDES: Link up - forced.\n");
			}
		} else {
			hw->serdes_has_link = false;
			DEBUGOUT("SERDES: Link down - force failed.\n");
		}
	}

	if (E1000_TXCW_ANE & er32(TXCW)) {
		status = er32(STATUS);
		if (status & E1000_STATUS_LU) {
			/* SYNCH bit and IV bit are sticky, so reread rxcw. */
			udelay(10);
			rxcw = er32(RXCW);
			if (rxcw & E1000_RXCW_SYNCH) {
				if (!(rxcw & E1000_RXCW_IV)) {
					hw->serdes_has_link = true;
					DEBUGOUT("SERDES: Link up - autoneg "
						 "completed successfully.\n");
				} else {
					hw->serdes_has_link = false;
					DEBUGOUT("SERDES: Link down - invalid"
						 "codewords detected in autoneg.\n");
				}
			} else {
				hw->serdes_has_link = false;
				DEBUGOUT("SERDES: Link down - no sync.\n");
			}
		} else {
			hw->serdes_has_link = false;
			DEBUGOUT("SERDES: Link down - autoneg failed\n");
		}
	}

      out:
	return ret_val;
}

/**
 * e1000_check_for_link
 * @hw: Struct containing variables accessed by shared code
 *
 * Checks to see if the link status of the hardware has changed.
 * Called by any function that needs to check the link status of the adapter.
 */
s32 e1000_check_for_link(struct e1000_hw *hw)
{
	u32 rxcw = 0;
	u32 ctrl;
	u32 status;
	u32 rctl;
	u32 icr;
	u32 signal = 0;
	s32 ret_val;
	u16 phy_data;

	DEBUGFUNC("e1000_check_for_link");

	ctrl = er32(CTRL);
	status = er32(STATUS);

	/* On adapters with a MAC newer than 82544, SW Definable pin 1 will be
	 * set when the optics detect a signal. On older adapters, it will be
	 * cleared when there is a signal.  This applies to fiber media only.
	 */
	if ((hw->media_type == e1000_media_type_fiber) ||
	    (hw->media_type == e1000_media_type_internal_serdes)) {
		rxcw = er32(RXCW);

		if (hw->media_type == e1000_media_type_fiber) {
			signal =
			    (hw->mac_type >
			     e1000_82544) ? E1000_CTRL_SWDPIN1 : 0;
			if (status & E1000_STATUS_LU)
				hw->get_link_status = false;
		}
	}

	/* If we have a copper PHY then we only want to go out to the PHY
	 * registers to see if Auto-Neg has completed and/or if our link
	 * status has changed.  The get_link_status flag will be set if we
	 * receive a Link Status Change interrupt or we have Rx Sequence
	 * Errors.
	 */
	if ((hw->media_type == e1000_media_type_copper) && hw->get_link_status) {
		/* First we want to see if the MII Status Register reports
		 * link.  If so, then we want to get the current speed/duplex
		 * of the PHY.
		 * Read the register twice since the link bit is sticky.
		 */
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
		if (ret_val)
			return ret_val;
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
		if (ret_val)
			return ret_val;

		if (phy_data & MII_SR_LINK_STATUS) {
			hw->get_link_status = false;
			/* Check if there was DownShift, must be checked immediately after
			 * link-up */
			e1000_check_downshift(hw);

			/* If we are on 82544 or 82543 silicon and speed/duplex
			 * are forced to 10H or 10F, then we will implement the polarity
			 * reversal workaround.  We disable interrupts first, and upon
			 * returning, place the devices interrupt state to its previous
			 * value except for the link status change interrupt which will
			 * happen due to the execution of this workaround.
			 */

			if ((hw->mac_type == e1000_82544
			     || hw->mac_type == e1000_82543) && (!hw->autoneg)
			    && (hw->forced_speed_duplex == e1000_10_full
				|| hw->forced_speed_duplex == e1000_10_half)) {
				ew32(IMC, 0xffffffff);
				ret_val =
				    e1000_polarity_reversal_workaround(hw);
				icr = er32(ICR);
				ew32(ICS, (icr & ~E1000_ICS_LSC));
				ew32(IMS, IMS_ENABLE_MASK);
			}

		} else {
			/* No link detected */
			e1000_config_dsp_after_link_change(hw, false);
			return 0;
		}

		/* If we are forcing speed/duplex, then we simply return since
		 * we have already determined whether we have link or not.
		 */
		if (!hw->autoneg)
			return -E1000_ERR_CONFIG;

		/* optimize the dsp settings for the igp phy */
		e1000_config_dsp_after_link_change(hw, true);

		/* We have a M88E1000 PHY and Auto-Neg is enabled.  If we
		 * have Si on board that is 82544 or newer, Auto
		 * Speed Detection takes care of MAC speed/duplex
		 * configuration.  So we only need to configure Collision
		 * Distance in the MAC.  Otherwise, we need to force
		 * speed/duplex on the MAC to the current PHY speed/duplex
		 * settings.
		 */
		if (hw->mac_type >= e1000_82544)
			e1000_config_collision_dist(hw);
		else {
			ret_val = e1000_config_mac_to_phy(hw);
			if (ret_val) {
				DEBUGOUT
				    ("Error configuring MAC to PHY settings\n");
				return ret_val;
			}
		}

		/* Configure Flow Control now that Auto-Neg has completed. First, we
		 * need to restore the desired flow control settings because we may
		 * have had to re-autoneg with a different link partner.
		 */
		ret_val = e1000_config_fc_after_link_up(hw);
		if (ret_val) {
			DEBUGOUT("Error configuring flow control\n");
			return ret_val;
		}

		/* At this point we know that we are on copper and we have
		 * auto-negotiated link.  These are conditions for checking the link
		 * partner capability register.  We use the link speed to determine if
		 * TBI compatibility needs to be turned on or off.  If the link is not
		 * at gigabit speed, then TBI compatibility is not needed.  If we are
		 * at gigabit speed, we turn on TBI compatibility.
		 */
		if (hw->tbi_compatibility_en) {
			u16 speed, duplex;
			ret_val =
			    e1000_get_speed_and_duplex(hw, &speed, &duplex);
			if (ret_val) {
				DEBUGOUT
				    ("Error getting link speed and duplex\n");
				return ret_val;
			}
			if (speed != SPEED_1000) {
				/* If link speed is not set to gigabit speed, we do not need
				 * to enable TBI compatibility.
				 */
				if (hw->tbi_compatibility_on) {
					/* If we previously were in the mode, turn it off. */
					rctl = er32(RCTL);
					rctl &= ~E1000_RCTL_SBP;
					ew32(RCTL, rctl);
					hw->tbi_compatibility_on = false;
				}
			} else {
				/* If TBI compatibility is was previously off, turn it on. For
				 * compatibility with a TBI link partner, we will store bad
				 * packets. Some frames have an additional byte on the end and
				 * will look like CRC errors to to the hardware.
				 */
				if (!hw->tbi_compatibility_on) {
					hw->tbi_compatibility_on = true;
					rctl = er32(RCTL);
					rctl |= E1000_RCTL_SBP;
					ew32(RCTL, rctl);
				}
			}
		}
	}

	if ((hw->media_type == e1000_media_type_fiber) ||
	    (hw->media_type == e1000_media_type_internal_serdes))
		e1000_check_for_serdes_link_generic(hw);

	return E1000_SUCCESS;
}

/**
 * e1000_get_speed_and_duplex
 * @hw: Struct containing variables accessed by shared code
 * @speed: Speed of the connection
 * @duplex: Duplex setting of the connection

 * Detects the current speed and duplex settings of the hardware.
 */
s32 e1000_get_speed_and_duplex(struct e1000_hw *hw, u16 *speed, u16 *duplex)
{
	u32 status;
	s32 ret_val;
	u16 phy_data;

	DEBUGFUNC("e1000_get_speed_and_duplex");

	if (hw->mac_type >= e1000_82543) {
		status = er32(STATUS);
		if (status & E1000_STATUS_SPEED_1000) {
			*speed = SPEED_1000;
			DEBUGOUT("1000 Mbs, ");
		} else if (status & E1000_STATUS_SPEED_100) {
			*speed = SPEED_100;
			DEBUGOUT("100 Mbs, ");
		} else {
			*speed = SPEED_10;
			DEBUGOUT("10 Mbs, ");
		}

		if (status & E1000_STATUS_FD) {
			*duplex = FULL_DUPLEX;
			DEBUGOUT("Full Duplex\n");
		} else {
			*duplex = HALF_DUPLEX;
			DEBUGOUT(" Half Duplex\n");
		}
	} else {
		DEBUGOUT("1000 Mbs, Full Duplex\n");
		*speed = SPEED_1000;
		*duplex = FULL_DUPLEX;
	}

	/* IGP01 PHY may advertise full duplex operation after speed downgrade even
	 * if it is operating at half duplex.  Here we set the duplex settings to
	 * match the duplex in the link partner's capabilities.
	 */
	if (hw->phy_type == e1000_phy_igp && hw->speed_downgraded) {
		ret_val = e1000_read_phy_reg(hw, PHY_AUTONEG_EXP, &phy_data);
		if (ret_val)
			return ret_val;

		if (!(phy_data & NWAY_ER_LP_NWAY_CAPS))
			*duplex = HALF_DUPLEX;
		else {
			ret_val =
			    e1000_read_phy_reg(hw, PHY_LP_ABILITY, &phy_data);
			if (ret_val)
				return ret_val;
			if ((*speed == SPEED_100
			     && !(phy_data & NWAY_LPAR_100TX_FD_CAPS))
			    || (*speed == SPEED_10
				&& !(phy_data & NWAY_LPAR_10T_FD_CAPS)))
				*duplex = HALF_DUPLEX;
		}
	}

	return E1000_SUCCESS;
}

/**
 * e1000_wait_autoneg
 * @hw: Struct containing variables accessed by shared code
 *
 * Blocks until autoneg completes or times out (~4.5 seconds)
 */
static s32 e1000_wait_autoneg(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 i;
	u16 phy_data;

	DEBUGFUNC("e1000_wait_autoneg");
	DEBUGOUT("Waiting for Auto-Neg to complete.\n");

	/* We will wait for autoneg to complete or 4.5 seconds to expire. */
	for (i = PHY_AUTO_NEG_TIME; i > 0; i--) {
		/* Read the MII Status Register and wait for Auto-Neg
		 * Complete bit to be set.
		 */
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
		if (ret_val)
			return ret_val;
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
		if (ret_val)
			return ret_val;
		if (phy_data & MII_SR_AUTONEG_COMPLETE) {
			return E1000_SUCCESS;
		}
		msleep(100);
	}
	return E1000_SUCCESS;
}

/**
 * e1000_raise_mdi_clk - Raises the Management Data Clock
 * @hw: Struct containing variables accessed by shared code
 * @ctrl: Device control register's current value
 */
static void e1000_raise_mdi_clk(struct e1000_hw *hw, u32 *ctrl)
{
	/* Raise the clock input to the Management Data Clock (by setting the MDC
	 * bit), and then delay 10 microseconds.
	 */
	ew32(CTRL, (*ctrl | E1000_CTRL_MDC));
	E1000_WRITE_FLUSH();
	udelay(10);
}

/**
 * e1000_lower_mdi_clk - Lowers the Management Data Clock
 * @hw: Struct containing variables accessed by shared code
 * @ctrl: Device control register's current value
 */
static void e1000_lower_mdi_clk(struct e1000_hw *hw, u32 *ctrl)
{
	/* Lower the clock input to the Management Data Clock (by clearing the MDC
	 * bit), and then delay 10 microseconds.
	 */
	ew32(CTRL, (*ctrl & ~E1000_CTRL_MDC));
	E1000_WRITE_FLUSH();
	udelay(10);
}

/**
 * e1000_shift_out_mdi_bits - Shifts data bits out to the PHY
 * @hw: Struct containing variables accessed by shared code
 * @data: Data to send out to the PHY
 * @count: Number of bits to shift out
 *
 * Bits are shifted out in MSB to LSB order.
 */
static void e1000_shift_out_mdi_bits(struct e1000_hw *hw, u32 data, u16 count)
{
	u32 ctrl;
	u32 mask;

	/* We need to shift "count" number of bits out to the PHY. So, the value
	 * in the "data" parameter will be shifted out to the PHY one bit at a
	 * time. In order to do this, "data" must be broken down into bits.
	 */
	mask = 0x01;
	mask <<= (count - 1);

	ctrl = er32(CTRL);

	/* Set MDIO_DIR and MDC_DIR direction bits to be used as output pins. */
	ctrl |= (E1000_CTRL_MDIO_DIR | E1000_CTRL_MDC_DIR);

	while (mask) {
		/* A "1" is shifted out to the PHY by setting the MDIO bit to "1" and
		 * then raising and lowering the Management Data Clock. A "0" is
		 * shifted out to the PHY by setting the MDIO bit to "0" and then
		 * raising and lowering the clock.
		 */
		if (data & mask)
			ctrl |= E1000_CTRL_MDIO;
		else
			ctrl &= ~E1000_CTRL_MDIO;

		ew32(CTRL, ctrl);
		E1000_WRITE_FLUSH();

		udelay(10);

		e1000_raise_mdi_clk(hw, &ctrl);
		e1000_lower_mdi_clk(hw, &ctrl);

		mask = mask >> 1;
	}
}

/**
 * e1000_shift_in_mdi_bits - Shifts data bits in from the PHY
 * @hw: Struct containing variables accessed by shared code
 *
 * Bits are shifted in in MSB to LSB order.
 */
static u16 e1000_shift_in_mdi_bits(struct e1000_hw *hw)
{
	u32 ctrl;
	u16 data = 0;
	u8 i;

	/* In order to read a register from the PHY, we need to shift in a total
	 * of 18 bits from the PHY. The first two bit (turnaround) times are used
	 * to avoid contention on the MDIO pin when a read operation is performed.
	 * These two bits are ignored by us and thrown away. Bits are "shifted in"
	 * by raising the input to the Management Data Clock (setting the MDC bit),
	 * and then reading the value of the MDIO bit.
	 */
	ctrl = er32(CTRL);

	/* Clear MDIO_DIR (SWDPIO1) to indicate this bit is to be used as input. */
	ctrl &= ~E1000_CTRL_MDIO_DIR;
	ctrl &= ~E1000_CTRL_MDIO;

	ew32(CTRL, ctrl);
	E1000_WRITE_FLUSH();

	/* Raise and Lower the clock before reading in the data. This accounts for
	 * the turnaround bits. The first clock occurred when we clocked out the
	 * last bit of the Register Address.
	 */
	e1000_raise_mdi_clk(hw, &ctrl);
	e1000_lower_mdi_clk(hw, &ctrl);

	for (data = 0, i = 0; i < 16; i++) {
		data = data << 1;
		e1000_raise_mdi_clk(hw, &ctrl);
		ctrl = er32(CTRL);
		/* Check to see if we shifted in a "1". */
		if (ctrl & E1000_CTRL_MDIO)
			data |= 1;
		e1000_lower_mdi_clk(hw, &ctrl);
	}

	e1000_raise_mdi_clk(hw, &ctrl);
	e1000_lower_mdi_clk(hw, &ctrl);

	return data;
}


/**
 * e1000_read_phy_reg - read a phy register
 * @hw: Struct containing variables accessed by shared code
 * @reg_addr: address of the PHY register to read
 *
 * Reads the value from a PHY register, if the value is on a specific non zero
 * page, sets the page first.
 */
s32 e1000_read_phy_reg(struct e1000_hw *hw, u32 reg_addr, u16 *phy_data)
{
	u32 ret_val;

	DEBUGFUNC("e1000_read_phy_reg");

	if ((hw->phy_type == e1000_phy_igp) &&
	    (reg_addr > MAX_PHY_MULTI_PAGE_REG)) {
		ret_val = e1000_write_phy_reg_ex(hw, IGP01E1000_PHY_PAGE_SELECT,
						 (u16) reg_addr);
		if (ret_val)
			return ret_val;
	}

	ret_val = e1000_read_phy_reg_ex(hw, MAX_PHY_REG_ADDRESS & reg_addr,
					phy_data);

	return ret_val;
}

static s32 e1000_read_phy_reg_ex(struct e1000_hw *hw, u32 reg_addr,
				 u16 *phy_data)
{
	u32 i;
	u32 mdic = 0;
	const u32 phy_addr = 1;

	DEBUGFUNC("e1000_read_phy_reg_ex");

	if (reg_addr > MAX_PHY_REG_ADDRESS) {
		DEBUGOUT1("PHY Address %d is out of range\n", reg_addr);
		return -E1000_ERR_PARAM;
	}

	if (hw->mac_type > e1000_82543) {
		/* Set up Op-code, Phy Address, and register address in the MDI
		 * Control register.  The MAC will take care of interfacing with the
		 * PHY to retrieve the desired data.
		 */
		mdic = ((reg_addr << E1000_MDIC_REG_SHIFT) |
			(phy_addr << E1000_MDIC_PHY_SHIFT) |
			(E1000_MDIC_OP_READ));

		ew32(MDIC, mdic);

		/* Poll the ready bit to see if the MDI read completed */
		for (i = 0; i < 64; i++) {
			udelay(50);
			mdic = er32(MDIC);
			if (mdic & E1000_MDIC_READY)
				break;
		}
		if (!(mdic & E1000_MDIC_READY)) {
			DEBUGOUT("MDI Read did not complete\n");
			return -E1000_ERR_PHY;
		}
		if (mdic & E1000_MDIC_ERROR) {
			DEBUGOUT("MDI Error\n");
			return -E1000_ERR_PHY;
		}
		*phy_data = (u16) mdic;
	} else {
		/* We must first send a preamble through the MDIO pin to signal the
		 * beginning of an MII instruction.  This is done by sending 32
		 * consecutive "1" bits.
		 */
		e1000_shift_out_mdi_bits(hw, PHY_PREAMBLE, PHY_PREAMBLE_SIZE);

		/* Now combine the next few fields that are required for a read
		 * operation.  We use this method instead of calling the
		 * e1000_shift_out_mdi_bits routine five different times. The format of
		 * a MII read instruction consists of a shift out of 14 bits and is
		 * defined as follows:
		 *    <Preamble><SOF><Op Code><Phy Addr><Reg Addr>
		 * followed by a shift in of 18 bits.  This first two bits shifted in
		 * are TurnAround bits used to avoid contention on the MDIO pin when a
		 * READ operation is performed.  These two bits are thrown away
		 * followed by a shift in of 16 bits which contains the desired data.
		 */
		mdic = ((reg_addr) | (phy_addr << 5) |
			(PHY_OP_READ << 10) | (PHY_SOF << 12));

		e1000_shift_out_mdi_bits(hw, mdic, 14);

		/* Now that we've shifted out the read command to the MII, we need to
		 * "shift in" the 16-bit value (18 total bits) of the requested PHY
		 * register address.
		 */
		*phy_data = e1000_shift_in_mdi_bits(hw);
	}
	return E1000_SUCCESS;
}

/**
 * e1000_write_phy_reg - write a phy register
 *
 * @hw: Struct containing variables accessed by shared code
 * @reg_addr: address of the PHY register to write
 * @data: data to write to the PHY

 * Writes a value to a PHY register
 */
s32 e1000_write_phy_reg(struct e1000_hw *hw, u32 reg_addr, u16 phy_data)
{
	u32 ret_val;

	DEBUGFUNC("e1000_write_phy_reg");

	if ((hw->phy_type == e1000_phy_igp) &&
	    (reg_addr > MAX_PHY_MULTI_PAGE_REG)) {
		ret_val = e1000_write_phy_reg_ex(hw, IGP01E1000_PHY_PAGE_SELECT,
						 (u16) reg_addr);
		if (ret_val)
			return ret_val;
	}

	ret_val = e1000_write_phy_reg_ex(hw, MAX_PHY_REG_ADDRESS & reg_addr,
					 phy_data);

	return ret_val;
}

static s32 e1000_write_phy_reg_ex(struct e1000_hw *hw, u32 reg_addr,
				  u16 phy_data)
{
	u32 i;
	u32 mdic = 0;
	const u32 phy_addr = 1;

	DEBUGFUNC("e1000_write_phy_reg_ex");

	if (reg_addr > MAX_PHY_REG_ADDRESS) {
		DEBUGOUT1("PHY Address %d is out of range\n", reg_addr);
		return -E1000_ERR_PARAM;
	}

	if (hw->mac_type > e1000_82543) {
		/* Set up Op-code, Phy Address, register address, and data intended
		 * for the PHY register in the MDI Control register.  The MAC will take
		 * care of interfacing with the PHY to send the desired data.
		 */
		mdic = (((u32) phy_data) |
			(reg_addr << E1000_MDIC_REG_SHIFT) |
			(phy_addr << E1000_MDIC_PHY_SHIFT) |
			(E1000_MDIC_OP_WRITE));

		ew32(MDIC, mdic);

		/* Poll the ready bit to see if the MDI read completed */
		for (i = 0; i < 641; i++) {
			udelay(5);
			mdic = er32(MDIC);
			if (mdic & E1000_MDIC_READY)
				break;
		}
		if (!(mdic & E1000_MDIC_READY)) {
			DEBUGOUT("MDI Write did not complete\n");
			return -E1000_ERR_PHY;
		}
	} else {
		/* We'll need to use the SW defined pins to shift the write command
		 * out to the PHY. We first send a preamble to the PHY to signal the
		 * beginning of the MII instruction.  This is done by sending 32
		 * consecutive "1" bits.
		 */
		e1000_shift_out_mdi_bits(hw, PHY_PREAMBLE, PHY_PREAMBLE_SIZE);

		/* Now combine the remaining required fields that will indicate a
		 * write operation. We use this method instead of calling the
		 * e1000_shift_out_mdi_bits routine for each field in the command. The
		 * format of a MII write instruction is as follows:
		 * <Preamble><SOF><Op Code><Phy Addr><Reg Addr><Turnaround><Data>.
		 */
		mdic = ((PHY_TURNAROUND) | (reg_addr << 2) | (phy_addr << 7) |
			(PHY_OP_WRITE << 12) | (PHY_SOF << 14));
		mdic <<= 16;
		mdic |= (u32) phy_data;

		e1000_shift_out_mdi_bits(hw, mdic, 32);
	}

	return E1000_SUCCESS;
}

/**
 * e1000_phy_hw_reset - reset the phy, hardware style
 * @hw: Struct containing variables accessed by shared code
 *
 * Returns the PHY to the power-on reset state
 */
s32 e1000_phy_hw_reset(struct e1000_hw *hw)
{
	u32 ctrl, ctrl_ext;
	u32 led_ctrl;
	s32 ret_val;

	DEBUGFUNC("e1000_phy_hw_reset");

	DEBUGOUT("Resetting Phy...\n");

	if (hw->mac_type > e1000_82543) {
		/* Read the device control register and assert the E1000_CTRL_PHY_RST
		 * bit. Then, take it out of reset.
		 * For e1000 hardware, we delay for 10ms between the assert
		 * and deassert.
		 */
		ctrl = er32(CTRL);
		ew32(CTRL, ctrl | E1000_CTRL_PHY_RST);
		E1000_WRITE_FLUSH();

		msleep(10);

		ew32(CTRL, ctrl);
		E1000_WRITE_FLUSH();

	} else {
		/* Read the Extended Device Control Register, assert the PHY_RESET_DIR
		 * bit to put the PHY into reset. Then, take it out of reset.
		 */
		ctrl_ext = er32(CTRL_EXT);
		ctrl_ext |= E1000_CTRL_EXT_SDP4_DIR;
		ctrl_ext &= ~E1000_CTRL_EXT_SDP4_DATA;
		ew32(CTRL_EXT, ctrl_ext);
		E1000_WRITE_FLUSH();
		msleep(10);
		ctrl_ext |= E1000_CTRL_EXT_SDP4_DATA;
		ew32(CTRL_EXT, ctrl_ext);
		E1000_WRITE_FLUSH();
	}
	udelay(150);

	if ((hw->mac_type == e1000_82541) || (hw->mac_type == e1000_82547)) {
		/* Configure activity LED after PHY reset */
		led_ctrl = er32(LEDCTL);
		led_ctrl &= IGP_ACTIVITY_LED_MASK;
		led_ctrl |= (IGP_ACTIVITY_LED_ENABLE | IGP_LED3_MODE);
		ew32(LEDCTL, led_ctrl);
	}

	/* Wait for FW to finish PHY configuration. */
	ret_val = e1000_get_phy_cfg_done(hw);
	if (ret_val != E1000_SUCCESS)
		return ret_val;

	return ret_val;
}

/**
 * e1000_phy_reset - reset the phy to commit settings
 * @hw: Struct containing variables accessed by shared code
 *
 * Resets the PHY
 * Sets bit 15 of the MII Control register
 */
s32 e1000_phy_reset(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 phy_data;

	DEBUGFUNC("e1000_phy_reset");

	switch (hw->phy_type) {
	case e1000_phy_igp:
		ret_val = e1000_phy_hw_reset(hw);
		if (ret_val)
			return ret_val;
		break;
	default:
		ret_val = e1000_read_phy_reg(hw, PHY_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		phy_data |= MII_CR_RESET;
		ret_val = e1000_write_phy_reg(hw, PHY_CTRL, phy_data);
		if (ret_val)
			return ret_val;

		udelay(1);
		break;
	}

	if (hw->phy_type == e1000_phy_igp)
		e1000_phy_init_script(hw);

	return E1000_SUCCESS;
}

/**
 * e1000_detect_gig_phy - check the phy type
 * @hw: Struct containing variables accessed by shared code
 *
 * Probes the expected PHY address for known PHY IDs
 */
static s32 e1000_detect_gig_phy(struct e1000_hw *hw)
{
	s32 phy_init_status, ret_val;
	u16 phy_id_high, phy_id_low;
	bool match = false;

	DEBUGFUNC("e1000_detect_gig_phy");

	if (hw->phy_id != 0)
		return E1000_SUCCESS;

	/* Read the PHY ID Registers to identify which PHY is onboard. */
	ret_val = e1000_read_phy_reg(hw, PHY_ID1, &phy_id_high);
	if (ret_val)
		return ret_val;

	hw->phy_id = (u32) (phy_id_high << 16);
	udelay(20);
	ret_val = e1000_read_phy_reg(hw, PHY_ID2, &phy_id_low);
	if (ret_val)
		return ret_val;

	hw->phy_id |= (u32) (phy_id_low & PHY_REVISION_MASK);
	hw->phy_revision = (u32) phy_id_low & ~PHY_REVISION_MASK;

	switch (hw->mac_type) {
	case e1000_82543:
		if (hw->phy_id == M88E1000_E_PHY_ID)
			match = true;
		break;
	case e1000_82544:
		if (hw->phy_id == M88E1000_I_PHY_ID)
			match = true;
		break;
	case e1000_82540:
	case e1000_82545:
	case e1000_82545_rev_3:
	case e1000_82546:
	case e1000_82546_rev_3:
		if (hw->phy_id == M88E1011_I_PHY_ID)
			match = true;
		break;
	case e1000_82541:
	case e1000_82541_rev_2:
	case e1000_82547:
	case e1000_82547_rev_2:
		if (hw->phy_id == IGP01E1000_I_PHY_ID)
			match = true;
		break;
	default:
		DEBUGOUT1("Invalid MAC type %d\n", hw->mac_type);
		return -E1000_ERR_CONFIG;
	}
	phy_init_status = e1000_set_phy_type(hw);

	if ((match) && (phy_init_status == E1000_SUCCESS)) {
		DEBUGOUT1("PHY ID 0x%X detected\n", hw->phy_id);
		return E1000_SUCCESS;
	}
	DEBUGOUT1("Invalid PHY ID 0x%X\n", hw->phy_id);
	return -E1000_ERR_PHY;
}

/**
 * e1000_phy_reset_dsp - reset DSP
 * @hw: Struct containing variables accessed by shared code
 *
 * Resets the PHY's DSP
 */
static s32 e1000_phy_reset_dsp(struct e1000_hw *hw)
{
	s32 ret_val;
	DEBUGFUNC("e1000_phy_reset_dsp");

	do {
		ret_val = e1000_write_phy_reg(hw, 29, 0x001d);
		if (ret_val)
			break;
		ret_val = e1000_write_phy_reg(hw, 30, 0x00c1);
		if (ret_val)
			break;
		ret_val = e1000_write_phy_reg(hw, 30, 0x0000);
		if (ret_val)
			break;
		ret_val = E1000_SUCCESS;
	} while (0);

	return ret_val;
}

/**
 * e1000_phy_igp_get_info - get igp specific registers
 * @hw: Struct containing variables accessed by shared code
 * @phy_info: PHY information structure
 *
 * Get PHY information from various PHY registers for igp PHY only.
 */
static s32 e1000_phy_igp_get_info(struct e1000_hw *hw,
				  struct e1000_phy_info *phy_info)
{
	s32 ret_val;
	u16 phy_data, min_length, max_length, average;
	e1000_rev_polarity polarity;

	DEBUGFUNC("e1000_phy_igp_get_info");

	/* The downshift status is checked only once, after link is established,
	 * and it stored in the hw->speed_downgraded parameter. */
	phy_info->downshift = (e1000_downshift) hw->speed_downgraded;

	/* IGP01E1000 does not need to support it. */
	phy_info->extended_10bt_distance = e1000_10bt_ext_dist_enable_normal;

	/* IGP01E1000 always correct polarity reversal */
	phy_info->polarity_correction = e1000_polarity_reversal_enabled;

	/* Check polarity status */
	ret_val = e1000_check_polarity(hw, &polarity);
	if (ret_val)
		return ret_val;

	phy_info->cable_polarity = polarity;

	ret_val = e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	phy_info->mdix_mode =
	    (e1000_auto_x_mode) ((phy_data & IGP01E1000_PSSR_MDIX) >>
				 IGP01E1000_PSSR_MDIX_SHIFT);

	if ((phy_data & IGP01E1000_PSSR_SPEED_MASK) ==
	    IGP01E1000_PSSR_SPEED_1000MBPS) {
		/* Local/Remote Receiver Information are only valid at 1000 Mbps */
		ret_val = e1000_read_phy_reg(hw, PHY_1000T_STATUS, &phy_data);
		if (ret_val)
			return ret_val;

		phy_info->local_rx = ((phy_data & SR_1000T_LOCAL_RX_STATUS) >>
				      SR_1000T_LOCAL_RX_STATUS_SHIFT) ?
		    e1000_1000t_rx_status_ok : e1000_1000t_rx_status_not_ok;
		phy_info->remote_rx = ((phy_data & SR_1000T_REMOTE_RX_STATUS) >>
				       SR_1000T_REMOTE_RX_STATUS_SHIFT) ?
		    e1000_1000t_rx_status_ok : e1000_1000t_rx_status_not_ok;

		/* Get cable length */
		ret_val = e1000_get_cable_length(hw, &min_length, &max_length);
		if (ret_val)
			return ret_val;

		/* Translate to old method */
		average = (max_length + min_length) / 2;

		if (average <= e1000_igp_cable_length_50)
			phy_info->cable_length = e1000_cable_length_50;
		else if (average <= e1000_igp_cable_length_80)
			phy_info->cable_length = e1000_cable_length_50_80;
		else if (average <= e1000_igp_cable_length_110)
			phy_info->cable_length = e1000_cable_length_80_110;
		else if (average <= e1000_igp_cable_length_140)
			phy_info->cable_length = e1000_cable_length_110_140;
		else
			phy_info->cable_length = e1000_cable_length_140;
	}

	return E1000_SUCCESS;
}

/**
 * e1000_phy_m88_get_info - get m88 specific registers
 * @hw: Struct containing variables accessed by shared code
 * @phy_info: PHY information structure
 *
 * Get PHY information from various PHY registers for m88 PHY only.
 */
static s32 e1000_phy_m88_get_info(struct e1000_hw *hw,
				  struct e1000_phy_info *phy_info)
{
	s32 ret_val;
	u16 phy_data;
	e1000_rev_polarity polarity;

	DEBUGFUNC("e1000_phy_m88_get_info");

	/* The downshift status is checked only once, after link is established,
	 * and it stored in the hw->speed_downgraded parameter. */
	phy_info->downshift = (e1000_downshift) hw->speed_downgraded;

	ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_info->extended_10bt_distance =
	    ((phy_data & M88E1000_PSCR_10BT_EXT_DIST_ENABLE) >>
	     M88E1000_PSCR_10BT_EXT_DIST_ENABLE_SHIFT) ?
	    e1000_10bt_ext_dist_enable_lower :
	    e1000_10bt_ext_dist_enable_normal;

	phy_info->polarity_correction =
	    ((phy_data & M88E1000_PSCR_POLARITY_REVERSAL) >>
	     M88E1000_PSCR_POLARITY_REVERSAL_SHIFT) ?
	    e1000_polarity_reversal_disabled : e1000_polarity_reversal_enabled;

	/* Check polarity status */
	ret_val = e1000_check_polarity(hw, &polarity);
	if (ret_val)
		return ret_val;
	phy_info->cable_polarity = polarity;

	ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	phy_info->mdix_mode =
	    (e1000_auto_x_mode) ((phy_data & M88E1000_PSSR_MDIX) >>
				 M88E1000_PSSR_MDIX_SHIFT);

	if ((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_1000MBS) {
		/* Cable Length Estimation and Local/Remote Receiver Information
		 * are only valid at 1000 Mbps.
		 */
		phy_info->cable_length =
		    (e1000_cable_length) ((phy_data &
					   M88E1000_PSSR_CABLE_LENGTH) >>
					  M88E1000_PSSR_CABLE_LENGTH_SHIFT);

		ret_val = e1000_read_phy_reg(hw, PHY_1000T_STATUS, &phy_data);
		if (ret_val)
			return ret_val;

		phy_info->local_rx = ((phy_data & SR_1000T_LOCAL_RX_STATUS) >>
				      SR_1000T_LOCAL_RX_STATUS_SHIFT) ?
		    e1000_1000t_rx_status_ok : e1000_1000t_rx_status_not_ok;
		phy_info->remote_rx = ((phy_data & SR_1000T_REMOTE_RX_STATUS) >>
				       SR_1000T_REMOTE_RX_STATUS_SHIFT) ?
		    e1000_1000t_rx_status_ok : e1000_1000t_rx_status_not_ok;

	}

	return E1000_SUCCESS;
}

/**
 * e1000_phy_get_info - request phy info
 * @hw: Struct containing variables accessed by shared code
 * @phy_info: PHY information structure
 *
 * Get PHY information from various PHY registers
 */
s32 e1000_phy_get_info(struct e1000_hw *hw, struct e1000_phy_info *phy_info)
{
	s32 ret_val;
	u16 phy_data;

	DEBUGFUNC("e1000_phy_get_info");

	phy_info->cable_length = e1000_cable_length_undefined;
	phy_info->extended_10bt_distance = e1000_10bt_ext_dist_enable_undefined;
	phy_info->cable_polarity = e1000_rev_polarity_undefined;
	phy_info->downshift = e1000_downshift_undefined;
	phy_info->polarity_correction = e1000_polarity_reversal_undefined;
	phy_info->mdix_mode = e1000_auto_x_mode_undefined;
	phy_info->local_rx = e1000_1000t_rx_status_undefined;
	phy_info->remote_rx = e1000_1000t_rx_status_undefined;

	if (hw->media_type != e1000_media_type_copper) {
		DEBUGOUT("PHY info is only valid for copper media\n");
		return -E1000_ERR_CONFIG;
	}

	ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	if ((phy_data & MII_SR_LINK_STATUS) != MII_SR_LINK_STATUS) {
		DEBUGOUT("PHY info is only valid if link is up\n");
		return -E1000_ERR_CONFIG;
	}

	if (hw->phy_type == e1000_phy_igp)
		return e1000_phy_igp_get_info(hw, phy_info);
	else
		return e1000_phy_m88_get_info(hw, phy_info);
}

s32 e1000_validate_mdi_setting(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_validate_mdi_settings");

	if (!hw->autoneg && (hw->mdix == 0 || hw->mdix == 3)) {
		DEBUGOUT("Invalid MDI setting detected\n");
		hw->mdix = 1;
		return -E1000_ERR_CONFIG;
	}
	return E1000_SUCCESS;
}

/**
 * e1000_init_eeprom_params - initialize sw eeprom vars
 * @hw: Struct containing variables accessed by shared code
 *
 * Sets up eeprom variables in the hw struct.  Must be called after mac_type
 * is configured.
 */
s32 e1000_init_eeprom_params(struct e1000_hw *hw)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	u32 eecd = er32(EECD);
	s32 ret_val = E1000_SUCCESS;
	u16 eeprom_size;

	DEBUGFUNC("e1000_init_eeprom_params");

	switch (hw->mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
	case e1000_82543:
	case e1000_82544:
		eeprom->type = e1000_eeprom_microwire;
		eeprom->word_size = 64;
		eeprom->opcode_bits = 3;
		eeprom->address_bits = 6;
		eeprom->delay_usec = 50;
		break;
	case e1000_82540:
	case e1000_82545:
	case e1000_82545_rev_3:
	case e1000_82546:
	case e1000_82546_rev_3:
		eeprom->type = e1000_eeprom_microwire;
		eeprom->opcode_bits = 3;
		eeprom->delay_usec = 50;
		if (eecd & E1000_EECD_SIZE) {
			eeprom->word_size = 256;
			eeprom->address_bits = 8;
		} else {
			eeprom->word_size = 64;
			eeprom->address_bits = 6;
		}
		break;
	case e1000_82541:
	case e1000_82541_rev_2:
	case e1000_82547:
	case e1000_82547_rev_2:
		if (eecd & E1000_EECD_TYPE) {
			eeprom->type = e1000_eeprom_spi;
			eeprom->opcode_bits = 8;
			eeprom->delay_usec = 1;
			if (eecd & E1000_EECD_ADDR_BITS) {
				eeprom->page_size = 32;
				eeprom->address_bits = 16;
			} else {
				eeprom->page_size = 8;
				eeprom->address_bits = 8;
			}
		} else {
			eeprom->type = e1000_eeprom_microwire;
			eeprom->opcode_bits = 3;
			eeprom->delay_usec = 50;
			if (eecd & E1000_EECD_ADDR_BITS) {
				eeprom->word_size = 256;
				eeprom->address_bits = 8;
			} else {
				eeprom->word_size = 64;
				eeprom->address_bits = 6;
			}
		}
		break;
	default:
		break;
	}

	if (eeprom->type == e1000_eeprom_spi) {
		/* eeprom_size will be an enum [0..8] that maps to eeprom sizes 128B to
		 * 32KB (incremented by powers of 2).
		 */
		/* Set to default value for initial eeprom read. */
		eeprom->word_size = 64;
		ret_val = e1000_read_eeprom(hw, EEPROM_CFG, 1, &eeprom_size);
		if (ret_val)
			return ret_val;
		eeprom_size =
		    (eeprom_size & EEPROM_SIZE_MASK) >> EEPROM_SIZE_SHIFT;
		/* 256B eeprom size was not supported in earlier hardware, so we
		 * bump eeprom_size up one to ensure that "1" (which maps to 256B)
		 * is never the result used in the shifting logic below. */
		if (eeprom_size)
			eeprom_size++;

		eeprom->word_size = 1 << (eeprom_size + EEPROM_WORD_SIZE_SHIFT);
	}
	return ret_val;
}

/**
 * e1000_raise_ee_clk - Raises the EEPROM's clock input.
 * @hw: Struct containing variables accessed by shared code
 * @eecd: EECD's current value
 */
static void e1000_raise_ee_clk(struct e1000_hw *hw, u32 *eecd)
{
	/* Raise the clock input to the EEPROM (by setting the SK bit), and then
	 * wait <delay> microseconds.
	 */
	*eecd = *eecd | E1000_EECD_SK;
	ew32(EECD, *eecd);
	E1000_WRITE_FLUSH();
	udelay(hw->eeprom.delay_usec);
}

/**
 * e1000_lower_ee_clk - Lowers the EEPROM's clock input.
 * @hw: Struct containing variables accessed by shared code
 * @eecd: EECD's current value
 */
static void e1000_lower_ee_clk(struct e1000_hw *hw, u32 *eecd)
{
	/* Lower the clock input to the EEPROM (by clearing the SK bit), and then
	 * wait 50 microseconds.
	 */
	*eecd = *eecd & ~E1000_EECD_SK;
	ew32(EECD, *eecd);
	E1000_WRITE_FLUSH();
	udelay(hw->eeprom.delay_usec);
}

/**
 * e1000_shift_out_ee_bits - Shift data bits out to the EEPROM.
 * @hw: Struct containing variables accessed by shared code
 * @data: data to send to the EEPROM
 * @count: number of bits to shift out
 */
static void e1000_shift_out_ee_bits(struct e1000_hw *hw, u16 data, u16 count)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	u32 eecd;
	u32 mask;

	/* We need to shift "count" bits out to the EEPROM. So, value in the
	 * "data" parameter will be shifted out to the EEPROM one bit at a time.
	 * In order to do this, "data" must be broken down into bits.
	 */
	mask = 0x01 << (count - 1);
	eecd = er32(EECD****if (eeprom->type == e1000_******_microwire) {
	******&= ~E*********_DO;
	} else *********************************spil PRO/1000 |= x driver
  Copyri
	do PRO//* A "1" is shifted out to the EEPROM by setting bit "DI"difyaribu,
		 * andy itn raisrms U Generallowerrms  it
clock ( it
SK and controlsthe GN 2, as pubinpmodify it
  unde). stri0ute it and/or modify it
  undethe GN the terms conditiostriU General Public License,
  version 2, as pu.the G/RO/1000 Linux driver
  CI;

	*****data & mask)
	program is free softwaOR A Pew*******, ********	x drivWRITE_FLUSH()R A Pudelay*********ve re_usecould h****** Pube_ee_clk(hw, &r
  more ******  verlic License along wi
		URPO =PURPO >> 1e to} while (URPOSe to/* We leavere Focondiand seodifystriweralwranklin Stis routine.CHANTBILITY or
  FITNESS FOR c License for
  mor}

/**
 ********t and_inlic bits - S and CULARile cin from it will be  * @hw: Structhe Frainrms variables accessedul, bhared codeion:
*****: number ofNG".

toit and in
CHANstatic u16ribution in
  the file (sux NIC******hw *nse l Co*****)
{
	u32or
  ;7124-6i
 */16COPYI,
  51 In order.souread a registerContact Informat, 021need.sourcefor'*****'
seful".

  Contact Informat. Ble care "t and/orin"ul,  Public  2, as pu000_hon.

  This program (e terms ished by t),ARRANTY; wieadct e1000value of000_hSt - FOfth F.  Dursion 2isshift(srms ct epro>
   St - Fifth Flohould000_halways b, asear.000_/
*********************ll GNU Gene(x driver
  Co | GNU General P****CULAR= 0s32 for (i_hw * i <oro, O; i++l PRO/1000_hwCULAR<<are U General Public License along wiRO/1000 hw);
static s32  e1000_config_dsp_afteruct e*******00 Ls free softwarSE.  CULAR|=are Foh
  this program; if not, write 	}

	return_hw.c
 this distributioacquirblic**** - Prepares
  under
			om>
  ion:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing ion:L verth,
				 ty of  C(str

  .

 pin. Sets.s2, ahip select e1000Thi_lengfunctionc void _vftaalle100efore issulic L commU Genfy it
  unde..net>
  Intes4-64t e1000_hw *hw, u16lam Young Parkway, HR 971am Young Park*******info *w, u16 = &hw->w, u16
 */

/w);
, 	     

	DEBUGFUNC("hw,
				  struct e10" s32 e1000hw);
static s32 /* Request
  underAm>
  CHANT****uct mac_*****>*hw,
		82544his program is free softwREQig_fcLicense for
  more ********************	dation,(!hw);
static s32 e10GNT)) &&the c s32 (boolx driverunde_GRANT_ATTEMPTS)l PRO/	i++e100have rec5 e100ite_reg_io(struct e100}0_hw *h, u32 offset, u32 value);
hw);
1000 Linux driver
  *hw);
sstatic void e1000_wrie1000_OUT("Cuct enot 00_hw *
  undergrant\nc s3		00_get_c-x driveRRhy_typet e1000e100/*0_hwuph,
				  u16Read/Write e1000**********************************

  Intel PRO//*d_inited bU GeDICHANTABILITY or_phy(struct e1link_change(stSK0_writLicense for
  morhw);
sSet CSCHANTABILIT is free softwCS);
static void e1000_wright(c) 1999 - 2006 Intel Corporation.

  This pr;
static s32 e10t e1000_hw *h_to_phy(struct eCS00_hw *hw);
static void e1000_raise_mdi void e10*****e1000_get_cx drivSUCCESSn this distributiontandbyhw, u16 *mR_get_th,
				 tions 000_phy" 
  Iing Li  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing t>
  Intevoid s32 e1000_phy_reset_00_phy_info *phy_info);
static s32 e1000_phy_m88_get_info(struct e1000_hw *hw,
			s32 e1000_set_d3_lplu_sta**********************************

  Intel PRO/1000 Linuct e1000_hw *hw, u32 data, u16 count);
static u16 e1000details.

  You shoul have received a copy of the GNU ;
sta pubhighe1000_hw *hw, u32 *ctrl);SK u32 *eecd);
static void e1000_lower_ee_clk(struct e1000_hw *hw, u32 *eecd);
statSc void  under1000_hw *hw, u32 *ctrl);
static void e1000_lower_d e1000_lower_ee_clk(struct e1000_hw *hw, u32 *eecd);
static voilowCHANTABILITY or
  FITNESS uct e1000_hw *hw, u16 data, u16 count);
static s32 e1000_write_phy_reg_ex(str_mdi_clk(struct e1000_hw *hw, u32 *ctrl);
static voidToggle_outto flush1000_phy bool g_addr,
				  u16 phy_data);
static s32 e1000_read_phy_reg_ex(struct e1000_hw *hw, u32 reg_addr,
opper_link(struct e100eeprom(struct e1000_hw *hw);
static s32 e1000_set_vco_speed(struct e1000_hw *h}length(struct e10releaublic u16 *mdrop;
static voilength);
static s32 e1000_get_phy_cfg_done(struct e1000_hw *hw);
stTerminatl.co1000_phy_by inveruct e1000y_type's;
static void e1 u16 words, u16 *data); *hw);
static 00_phy_info *phy_info);
s, u16 *data)1000_phy_info *ph *hw);
static c s32 e1000_set_d3_lplu_staactive);tatic . Intel Corporation.

  This program is free softwCS;statPull_outd e1000_shift_oc u16 e1000_shift_statatic  SCK e1000oid e1000_raise_mdi_clave recIZE] = {
	5acquire_eeprom(struct e10IZE] = {
	5, 5, 5, 5, 5, 5, 5, 5t e1000_hw *hw);
sa(stnup tatic s	25, 2;
stS on M

  Intete iactive-0, 10, 10, 10, 20, ct e1000_hw *hw, u32 data, ue1000, 25, 25, 25, 30, 30, 30te(sublic edgolar as pub00_shift_out_ee_bits(struct e1000_hw *hw, u16 data, u16 count);
static s32 e10000, 40, 40, 40, 40, 40, 4, 100,Fall0, 100, 100,
	    100,
	100,c u16 e1000_shift_in_ee_bits(struct e1000_hw *hw, u16 count);
static s30, 40, 40, 40, 40, 40, 40,tude(sttop rtruct rms   underom>
  bool active);
static s32 e1000_wait_autoneg(stink(struct e1000_hw *hc void e1000_lower_m this distributionpi000_phy__revydsp(sad e1016th Flwordtatic s32 e1000_c1000_write_eeprom_spi(struct e1000_hw *hw, u16 offset,
				  u16 words, hw *hw,
		phy_type(struct P cable length table */
st16 retry_******hw *h	u8 phy_
  I_regct e1000_phy_info *phphy_type(struct c s32 te(stad "Status Rcessing" repeatedly until*hw);LSBte ia(stred.  The000_h  underwill signa
	caat*hw);
00_phy_har_vfen541 pled/orbyE1000_ing000_hw.h 0100,hw);inter == 
  Iusaccessing.  If it'ses_li1000_I_ within000_h5 milliseconds,eneralerrorr mouct e1000_gcase M88E1000_I can reibution in
 out0 N.E. Elnse y_type(RDSR_OPCODE_SPI the			IZE] = {
	5opailifile w *hwY_ID:
	case  = (u8)orporation, 5200 N.E. Elnse 8000_hw *h!(undefined;
		&have loaSTATUS_RDY dev)SE.  break0, 30, 30, 400_phy_phy_igp;
		+= 5one(struct 
static s32 e10hwprom(sdation,_phy_igp;
		<have loaMAX_RETRit_scr_ID:
		ATMEL SPI watic tim2541ct evaryContac0-20mSec 70,3.3V devices (an e1000only 0-5w)
{
	u35et_val;
	)= e1000****_phy_igp;
		>=y shared code
 */
statup_coetup_fiber_phy  undery_type >phy_hw);
staic s32 e1000_adjust_serdes_i_bits(struct e1000_hw *hw);
static s32 e1_rev_reset_dsp(s_hw *hw)
{
	DEBUGFUNC("e1000_set_phy_type");

	if (hw->mac_type == e1000_undefined)
		return -E10 @offset: te_phy100,DEBUGFiny it
  underions fo0_wriCULA:DEBUGFs forontact Information:
EBUGs1000-devel@l0);
	eep(20);

	/
hw *hw,
		. */
		ret_lam Young Parkway, Hillsbote_phyillsbo0);
	illsbo*CULAo);
st32D:
	;
_undn_c vo(&*************c vow *hre8E10hw,
		do		case e1000_nse :
			e10rite_ph(hw, 001);
		un	e1000_write_phy_reg(hw, 0x1F7et_c0x000}
0_ERR_PHY_TYPE;

	);
			e1000_wri82541:
		case e1000_82547:
			e1000_write_pce */y_reg(hw, 0x1F9atic s32 e1000_phy_m88_get_info(struct e1000_hw *hw, struct e1000_phy_info *ph. */
		ret_HY_ID:
		If0,
	60, i   hw-yet detected, do so n_data);************EBUG_siz*****0SE. hw,
		iniShou*****param	}

tic void  check  u16invaliructlues: reg(hw, too large,	defamanyhw, 0x1FU Genot000_henoughhw, 0x = e1000****(eg(hw, >=0,
	60,1_rev_2:
		00_p   || (
		swi>ep(20);

		/* Now  -phy_reg) the transmase e) {
		msleep(22atic s3("\"0);
	\" e1000eing  modl@liounds. W0x2F5B %d, :
		ca %d\nf the  Mov:
			e10p(20);

		/* Now et value of register 0x2F5B to be re/	if (hw-' *hwat don't usct eRDtions for- Se0, 8uwitchbit-bat e1000_PI000_hdirectly.aredar_hwcase the MAC
 */
nk(struc it
  undersfy iarite_pFW y_tyther port softwownsdoe   hw-w->marupe = e1000, 10n_leng		e1000_read
			;

			if _rev_polase e1000w,
				  struct e100hw) !is free 00_hw *SE. ic s32 e1000_adjust_serdesude(stru up (!(fuse				 70, 70, 8e = fused & IGP01E1000_ANAL.  FrahavD:
		i00_hw *Generphy type 00_8is point,d_phk;
	1F30,  it (stru *hw);
 itOG_FUSE_FI - 2006 Intel Corporation.

  This pr00_write_in(stru8000_A_ype =  f (hw->phREADon this devR A PARTIPE;

	switch (hw->phy_ihwetup_copp u16 *data);

/* IGPtaininatic s32 e1000_adjust_serdes_ampE PHY
 * @hw: Struct containiuct e10omoarse _ANALOsARE_F
			8th addrmber_2 |embeddtructASK) 000_ANAHANTAx3300_ANALOG_		    (file c== 8;
st 0);
		mslee128ript -P01E1000_ANA is _type(A8SE_THRESH)
					 e100 Gener COAR541 ||
		(000_ANA+			  )  100,
	efault:
		/* Should never hP01E1000_ANalog regisype = e1000_phy_			    IGP01E1000_ANALOG_FUS(u16)rite_phy_* 2)ice */
SE_COARSE_MASK);

		IGP01E100hw->pfuseCULAPHY_ID			    (c		    heg(hw, 0->mac_tlte_ecrementspe ==the GNeach byte _SUC) berms _rev, savrms o   IGP0verhw->po_reg(hw, sructthe GNU Genear-down the hw struct.*****er>mac_troll(stru) 19_rev_polbeyo phyty);
sta:
		c
 * @hw: Stru
			us allowct e1000ent0, 8memorysed);e(20);
_ID_8starom(stontack;
	te_phye full,
					      bool0);
	_up);
stati	 ==
			1, 0xBD21_ERR_PHY_TYPE;
	}

	r16;
statCULA[i]	ret_REV_ID:>>1000he tran_ID:<<return mplight(c) 1999 - 2006 Intel Corporation.

*

  Intel PRO/
			break;
		case E1000_82542_2_1				    fused);
				e1000_write_phy_reg(hw,
			G_FUSE_ENABLE_SW_CONTROL);
	ce */
	G_FUSE_COARSE_THRESMICROWIRE	case E1			    IGP01E1000_ANALOGG_FUSE_ENABLE_SW_CONTROL);
			}
		}
	}
}

/+ i * e10000_set_mac_type - Set the maac type member in thFor *

  Inte, by shrite_ph		    *hw);struct etype_sav00_hw *hw)
{
	FUNC("e1000_set_ac_typ;
			break		hw->mac_type = e1000_82542_rev2_1HY
 * @hw: Struct containinamplitude(sE GeneUSA.t e10peradrs(sfull ;

				fused =
				    (fbits(struct e1000_hw *hw);
static s32 e1 0x00at
static _(hw, su6 *mVerifiM:
	c000_825  under    a99);idg(hw, sumlength);
static s32 e1000_get_phy_cfg_done(struct e1000_hw *hw);
st = e10fusefirst 64hw)
{
	DEBUGt.
 * @hwhy type nd sumhw->ma9);
		2540;phy_tI	    h2542:umswitch ( = e1000_82545_ris 0xBABA
			ht e1000_hw *00_8254init_r545GMstruct w *hw,
		hw->mac_type = e1000_825id) {
	case M88E1000_E_PHY_ID546EB_QUA1000_I_16 ialog reg_hw.c
 * 1000_phy_info *phhw->mac_type = e1000_825c s32 
					      bool(0_ANALOCHECKSUE_COG +****up);
statifine -= IG			e1000_write_pi, 1e alo2546GB_FI) < 0tup_copetup_fiber  underhw->pEe current vaalue of register 0x2F5B to b1000_546EB_QUA+eep(20);GB_FIBEPOLY_****ER:
	case retu
		}
 * e100UMP01E1000_ANct e1000_hw *hw	ht(c){
		msleep(20)  underC46EB_QUAI, 0x00rrent value of register 0x2F5B to be this distributioupmac_type = e1000_82545;Calculs32 /_inithw->ma2 e1000_OPPER:
	case E1000_DEV_ID_82545GM_FIBER:
	case E1000_DEV_ID_82545GM_SERDESSe E1000_c_type 3 e1000_82545_rev_3;
		break. Subtracw *hw);e E1000_hwcase phy_ttatichw->madifferenceeep(EBUGFeg(hw, 63w->mac_type = e->mac_type) {
	_82541GI_LF:
	case E10ase E1000_DEV_ID_82546GB_COPPER:
	case E1000_DEV_ID_82546GB_FIBER:
	case E1000_DEV__82541GI_LF:
	case E10 E1000_DEV_ID_82546GB_CIE:
	case E1000_DE82546GB_QUAD_COPPER:
	case E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3:
		hw->mac_type = e1000_82546_rev_3;
		break;
	case E1000_DEV_ID_82541EI:
	case E1000_DEV_ID_825411EI:
	case E1000_DEV_ID_8254 ->mac_type*************_init1000_write_pCIE:
	case E1000_DED_8254mac_typePPER_KSP3:
1;
		break;
	casetatic 82546_rev_3;
lue of register 0x2F5B to be its(struct e1000_hw *hw);
static s32 e1bad_tx_carr_ -y_init_
		switch547GI:
		hw-w *hw, u3****sphy_saved_data);

		/* Disabled the PHY transmitter */
		e1000_write_phy_reg(hw, e == e003);
		msleep(bey_initen to0x0140);
		msleep(5);

		switch_init

		e1000_ww)
{
	DEBUGF"e1000_set_mediact InformationEB_COPPDEV_ID_82541GI_LF:
	case E1000x2010,0_hw *haftch 00_82x_addrs(
			h0_wrf (hw->mac_tmost likelyCS <linu an73, 0x009mac_typereak;
	default:
bad_tx_carr_s82541:
		case e1000_82547:
			e1000_write_phy_reg(hw, 0x1F95, 0x0001);
			e1000_write_phy_reg(hw, 0x1F71, 0xBD21);
bad_tx_carr_statshy_reg(hw, 0x1F79, 0x0018);
			e1000_write_phy_reg(hw, 0x1F30, 0x1600);
			e1000_write_phy_rw->media_type = e1000_media_type_internal_serdes;
		breace */hy_reg(hw, 0x1F9atic s32 e1000_phy_m88_get_info(struct e1000_hw ac_tpe == etruct e1000_phy_info *phbad_tx_carr__write_phy_reg(hw, 0x2010, 0x0008);
			break;

		case e1000_82541_rev_2:
		case e1000_82547_rev_2:
			e1000_write_phy_reg(hw, 0x1F73, 0x0099);
			break;
		default:
			break;
		}

		e1000_write_phy_reg(hw, 0x0000, 0x3300);
		msleep(20);

		/* Now enable the transmitter */
		e1000_write_phy_reg(hw, 0x2F5B, phy_saved_data);hw->mac_type == e1000_82547) {
			urrent value of register 0x2F5B to be reused);

				fine = fused &_inirms OG_FUSE_FINE_MASK;
				coarse =
				    fused & IGP01E1000_ANALOG_FUSE_COARSE_MASK**********************************

  Intel PRO/er */
					hw->bad_tx_carr_;
		}
		brite_phy_reg(hw, 0x1F79, 0x00ion ID ling MWI on 82542 rev 2.0\n");
	spiite_phy_reg(hw, 0x1F79, 0x00	msleep(10s32 e100/* Donetatus");

	/* case E1000_DEV_ID_82545EM_COPPER:
	casepe ==  media type and TBI compatibilener -EV_ID_82*hw)
{
	DEBUGFtionsgiven= e1000_	case ;

		/* Saessed by shared code
 */
void e1000_set_media_type(struct e1000_hw *hw)
{
	u32 status;

	DEBUGFUNC("e1000_set_media_type");

	if (hw->mac_type != e1000_82543) {
	0_ANAnctionarra0_82 8000_82545_ratibility is only valid on 82500_ERR_PHY_TYPE;

	board from generatus & E1000_STATUS_TBIMODE) {
				hw->media_type  = e1000_media_type_fiber;
				/* tbi_compatibility not valid on00_wridx				hw->tbi_compatibility_en = false;enerc s32 dation, PHY ase E10if (coa8 e10001000_ANALOG_FUSE_s.

  E_THRESH)
					fine -= IGP01E1000_ANALOG_FUSE_3;
		break;
	case E1000_DEV_IDY_MASK) |
				    (fine & IGP01E100		    fuses.

  ENABLE				e1000_. */
	000_ANA(hw,
						    IGP01E1000_ANALOG_FUS82547)) {ENon this device */
			    IGP01E1000_ANALY_MASK) |
				    (fine & IGP01E1000_ANALOG_FUSE_FINE_MASK) |
				    (coarse &
				     IGP01E1000_ANALOG_FUSE_COARSE_MASK);

				e1000_write_phy_reg(hw,
				_type == e1001000_ANALOG_FUSE_CONTROL,
						    fusetatic , receive,-DMA, and lihy_reg(h,
						    IGP01E1000_ANALOG_FUS_type == e10					    IGP01E1000_ANALLOG_FUSE_ENABLE_SW_CONTROL);
			}
		}

	case E10 PHY)/**
 * e1000_set_mac_type - Set the mac t	    fuseCULAR70, 70, 7Looptransd) {sed &if (o whole pag00_sete (32hareds)_82540EM_LOHANTA == e1000_82541) || (hw->oarse ==
	 mod e1000[ PHY](fuseed on a sha;
		de modlt:
			/* Inva mod 82542 rerev_2:
		/* These controllers , (ctrl542_rev2_1 PHYtatitype = 0_ANAult:
r0_hw *hw)iz	 * r000_pntelareda 32-ared PAGEs
	 * DEV_ID_8	break;
, variab
		brm_hw reak;
	} After MAC reset, n 8ce reDEV_ID_oad of EEP restore p.  BIGP     hw-ner l000_CTRpass new			    (DEV_IDc_typx3300g as a workaround to  %log regise e1 Now e5B, phup_cop000_DEV_ID_82540EP_LP:
		hw-> - IGP ph	_82541mplitudset_media_type - Set media type and TBI compatibil;
		}
		br allow
	 * any pending transactions to complet				    IGP01E100hit the MAC with
	 * the global reset.
	 */
	ew32(RCTL, 0);
	ew32(TCTL, E1000_TCTL_PSP);
	E1000_WRITE_FLUSH();

	/* The tbi_compatibility_on Flag must be cleared when Rctl is cleared. */
	hw->tbi_compatibility_on = false;

	/* Delay to allow any outstandin	e1000_pci82541:
		case e1000_82547:
			e1ce */
00_write_phy_reg(hw, 0x1F9(struct e1000_hw *hw, u16 offset,
					u16 words, u16 *datc type */
any o_med E1000_DEV_ before resetting the MAC */
	if ((hw-*

  InteHY_ID:
			    fuse_init_enC res000_phy_igp_get_info( (32541:
	case plus000_h6/82541:dummy			    (coeginux.nipts\n11 is I	   lmberworkreakincludD:
		i000_11switch (manc);
	}

	ifase ==547)  IGP01E1000thse Etfalssourcefo s32 etce_id)_82541r01E1000-devel@lists.
			000_		    (PHY_I1000uw *hw):
		if (hw->in!= e1000/ere -=mode = e1000.  It will not effect
	 * the curreEW PCI confiCOPPER:
	case E	}
		}
			    IGP01E1000_A + 2) s32 eefault:
		/* Should never h0			}
		}
SE_COARSE_MASK);

			-o stop bused);

				fine = fusACTIVITY_LE				    (fine & IGP01 == e100;
	}

	/* Dis541) || (hw->e e1000_82546:
	case e1000_00_MANC_ARP_Ee1000_82541_rev_2:
		/* These controllers82547)) {
		ew32(CTRCOPPER:
	case E1 64-bit write when issuing the
		 * reset, so use IO-mappingas a worka;
	}

	/* Di issue the reset */
		E1000_WRITE_REG_IO(hw, CTRL, (ctCTRL_RST));
		break;
	default:dow of;
	}

	/* Di]542_revse_eeprom(str000_CS l The _ctrl in eff */
tellOBILE:
		hw->to execu relo == e10previou

	s0_phy MERCHANTABSK) |
				    (fine & IGP01E100hw->pDOhy_m88;
		break;
	figured e10(equ== eo '1' is e hwf (hw->mac_			hw->pe == e1000_82541 ||
		    hw->mac_type == e10_hw *hw);
sDOr table MERCHCOPPDOOG_FUSE_STgogisterin 102547_rev_2) {
			hw->phy_type = 	 */
	s
					      bool20   b82542_2_1te_reg_io(struct e100hw *hw);
static s32 e1000_get_43:
	case enit scrig off1000_hw *ise E20_KSP3:
		hw->mac_type = etatic dides_liac_type hw);
static s32 e1000_adjust_serdes_amp. Initiace_id)ontac_init_guration of 				    (fine & IGP01Et containing taticitude(str540) {
		mancdis er32(MANC);
		manc &= ~(E1000_MANC_ARP_EN);
		ew32(MANC, manc);
	}

	if ((hw->mac_type 0= e1000_82541) || (hw->mac_type == e100|
		    h {
		e1000_phy_init_script(hw);

		/* Configure activity LED after PHY reset */
		led_ctrl = er32(LEDCTL);
		led_ctrl tak_82547:
		if (hw->82547) 
		led_ctrl |= (IGP_ACTIVITY_LED_ENABLE | IGP_LED3_MODE);
		eDS2(LEDCTL, led_ctrl);
	}

	/* Clear interrupt mask to stop board from generating interrupts */
	DEBUGOUT("Masking off all interrstored at
		 * the end of this routine. */

sta		   -2540;
EDCTL)aptc s3MACl = e16GB_QU	case E1000_DEV_ID_82545GM_FIBER:
	case E1000_DEV_ID_82545GM_SERDES:
		hw->mareceive' addre		    (contact Informat;
	ca_eeprohw->ma IGP32(LEDC0_wrev_2) rx_addrs(sof d * mx_addrs(s_val;
	->mac_type) {
		case;
	}

	/id) {
	case M88E1000_E_PHY_IDte_phy
		brea6GB_QUAD_CO,/* e(hw, 0x1F96, 0x003F);
		;
	}

	/ E1000_DEV_ID_82546GB_Nhis ADDRESS_SIZE  bo+=casePRO/eg(hw, = iftware UAD_COPPER:
	case E1000_DEV_:
			e1082546GB_QUAD_COPPER_KSP3:
		hw->mac_type = e1000_82546_rev_3;
		break;
	case E1000_DEV_ID_82541uct perm_word & P	break;urn EBUGOUTGB_FI &****0FF */
	ITE_REG_ARRAY(hw, MV_IDTA, i, 0);
		/* use wrlt:
	Initialswitchtive);
static ;
	}
default:
{
	u32 ctrPARE2 e1000_wai6:RITE_FLUSH();
	}

_rev_3
		E*********00_phy)static s300_phy_phy__1eep(5ITE_REG_ARRAY(hw, 5] ^******e Mu3:
	case LEDMAND_INVALIDATE)
			e1000_pci_set_mw++eep(ve);
sta(hw, MTA, ITE_REG_ARRAY(hw, MT 0x1F30, 0ct e1000_hw *hw);
static s32 e17_revrxnsmit allInitial	/* Areceivhw struct.filive phy_type");

	if (hw->mac_type == e1000_undefined)
		return -E100_wrPla;
	u000__init_rx_addi 0x1_type <= e1000ccessing 0;
	caa(strh (hw-reseeproript(hwase e1000_82545_rev_3:
	sed_init(s000_multicast tntel. Assumf (hweak;
	defaulrfals
	cas2 staeralak;
	

  ThP01E1_hw *struct e1000_u16 *data);f (hw->dma_faP cable length table */
stati* e105, 0ar_n0_82R:
	case E1000_DEV_f (hw->dma_fa>= e1000_82
	caak;
	default:
		/* ID_825etup_fiberProgrammrms ddreAtype) {
	to RAR[0]hw);
sse E1000_ar_setite_p to transmit, g of
	, 2048)ALOG*****RAR_ENTRIESMASK;

Zeror modicase   IG15
	default:
		/* ep flow control. *_initrms (hw)1-15

	/* Sreceives, 1  bool, 2048);nd uninitidetails.

  REG_ARRAYite_pRA,e1000< 1)-back 0_read_phy_reg_ex(struct ;
		ew32(TXDCTL, ctrl);
	}

	/** Clear V_ID_all of the statistics register	case E1000_DEV_IDhash_m	}

	/* SHash	 * nit_rx_addto0008)tic -= Is loceak;
	;

	DEBoblem when BIOSion:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing Libecause:-X problem whenrror count 
	 *

	/*24-64 link
	 * becauseone will delay 5ms or p8 *becauseR 97124-6
	 * y *polhw *hw,eepreen or reset aEDCTL);
		l;
	casiINE_Mdr32(LEDCT
	 *n BIOSAD_C & IGill incr== e1here
c__82543atic se terms = e1000 accessing ou	ctrl_ext |=  = er32(I[0] [1] [2] [3] [4] [5]ously 01  AA  00  12  34  56ously  IGP - Adjust SERDESMSB and recTE_FL0
		E);
	47:36] i.e. 0x563  u16 bove exa_typER ||
	  gurat2(CTRL_EXT);
((becauseval;>> 4			/*(1000_D shared 5])Clea4stop	E1000_WRITE_FL1based on 6:35OM settinAC6 * @hw: Struct containing variables accessed by shared code.
3*/
static s32 e1000_adjust_5erdes_amplitude(str2based on 5:34OM settingD8 * @hw: Struct containing variables accessed by shared code.
2*/
static s32 e1000_adjust_6erdes_amplitude(str bit d on 3:32OM settin634 * @hw: Struct containing variables accessed by shared co*/
static s32 e1000_adjust_8erdes_amplitud LED2(CTRL_EXT)&****FFF 0x1F30, 02(CTRL_EXTnd of this routine.ansmit *mi= IGan e   InePER ||
	  _link acce e1000_82545_rev_3:
	phy_saved_data);

		/* Disabled the PHY transmitter */
		e1000_wriD_825400_setupr reuSTATUons e e1000_82545_rev_3:
	0_wriindex:
	re e1000_82545_rev_3:
	c!= e1000_82/
,
				 u16 *ansmit 46GB_QUAD_COPPER_KSP3) {
rite-bset_mL, eR 97124-6	retloFUSEar_d e1MASK;

HW expe547;
		 -= n littlnc =diand_phw;
	dver_MASK) ared  funcrash ontacnet || ( funct(bigct cont))
		: Struct cont= e1000_ures fed byu32)ER ||	}


stahe appropr1] | E10 |	/* Momedia-
 * sp2cific
		}e media-
 * sp3cific2serdesw controlls the appropr4ate media-
 * sp5cific ltic voidDn LED\nRx;
	ca000_hwalllid f= e1if (;
sta = er0_TXDSct e1au16 *Rxrash unit haCTRL_EXrash Descripdrs(:rash OPPER:reAfterk;
	
 * establqueued		br				   Iwisen resng v;

	DEBHW000_hwshed. he hisc = er3d,
				e1000wnc = er32RSSE1000_HWr ariouslLAN fi *ly bee  T_typek ar
			hw->detic E_FUSE) {
unt wn LED\ny_reg(hROM_ e1000000_hw modd be
 * established. et_val;
	u16 .setureak; the modify w/* ForreGP01E1ains bittraffic)
		manageability
				e1000_roor, rom_duct e_ctrl 000_hPROMwaer are not 	e1000(si->mack;
	dGP01E1ione100UNC("at determines persisPROMcro45_resets) keepad to oneddrs(hwcoval = e1000_dationw @hw:kt tablpowefta(strion, 00_setupV45GM_AVcoars u16 ll= e1					c
		ctrl ;
	caundly valre-d the dirt that determinests to N flo establfter e flow
	 again, b {
		ctddrewG_SPAom>
pW defmLOG_This ward_phgoo_PHYWe

		ce1000C
 */
7_re&& hw-w)
{
(if ne>
  ary)OM. This configu				finta;

	D.  Lasg(hwM.
	  = er32e hw-e ini in the Frinu/* Foresetur merry waC_DEFAT_RO_DIS;
		ew32r register space */
		ESharedicait_s hw-rdANALOe hw->fc wiliill
	 *ID_8254ettings.
 is free RAH_AV          &eepro;
		ew32(TXDCTL, ctrl);
	}

	/* L, eClear aluse. Cardes e1000_lower_ee_clk(st (clear on read).  It is
	 * impo_FC_FULL;
that wew controe want to save off the oredia type and TBI compvfta allow
	 * ay *polnly valspecbrea = e1000_;

	DEBVLAN0_82543n BIOS hy_saved_data);

		/* Disabled the PHY transmitter */
		e1000_write_phy_rO Control bilities
	 */
	i)
			return  @9);
	:ll
	h with_init_te_phbilities.
	 */
	irn ret_val;
	}

ifferent
	 E1000_SUCCESS;
}

/**
327:
			e10032witch R 97124-6temp_type ==ive);
static s*********_wait_a_wri;
	}

	rite 1:
	ca1hy_saverol 
	if (hw->OARSCTL, ctrl);
	}VFT
	/*eg(hw, **** of the statistic initial
	 * polarit:
			e10x-ups of the statistics registers (clear on read).  It is
	olarity value for ,trol do this after we have tried r interrupW controlled pins, and setup the
	 * Extended Device Control reg with th	case E1000_DEV_IDa(strent
	 * or PCI-X prmac_type < e10001000_write_eeprom_spi(struct e1000_hw *hw, u16 offset,
				  u16 words, u16 *data);is called.P cable length table */
stati	E1000_WRI fixftaRL_EXT);
		/EPROM Read	/* Zero ");
			returnbi
  th;
		re *hw,
				 -E1000_ERR= e1000_0_set_phbili_FILTER_TBLi_set_mte_phy);
stati_phy_r		ctru32 stae wanodifyariablfta(diffamCTRL_EXT,g mustously at determines bilitID
			hw-ariablzed m_datexONTROL2asary subroutine to configureiouslgurat Read Error\nm_data & =return -E100) ?OM;
		}
		ctrl_ex:ERR_E detection.  So this should be done beforeRead Erro00_setup_pcs_link()
	 * or e1000_ERR_PHY_TYPE;

	id_levaliin E1000_SUCCESS;
}

/R 97124-6ledctlWRITonster fitrol i_the Free flt hu_val)isabled, because oisabf (hw-LEDCTL_Mhis LED_ONanything to
	 * initifflize these registers.
	 */_val)ITE_FLUSH();
		msle SW canything t16beca it does noF;
		break;
	}

	/* Calues.  Thisc s32 active);
static s<

	/* Take se e100/AULT== egunt woEEPROM_OM:
		hw->mac_type = e LEDtrol i0_set_d3e regi Devuct ecause pace */ =ntrol is deceive thre|= (1nd receve threshold r  Normally,
	 * th2se registers will be se2543)
		hw->			e1000_write_p0_ANALOID.
	 *SETTINGS"Zeroing the MTA\n");
	mta_s	hw->mac_type = e1000_82546_rev_3;
lue of register 0x2F5B to be reOG_FUSE_COAuse wr== time coRESERVED_t hueg(hnable  set to 0.
	 */
	if (!(hw->fc & FFFFhy_saveset to 0.
	 *
	if (!(DEFAULTty to receives, or if i4_TX_DESC_WBthat de from
		 * occur* Clea2);
sT, FLOW_Cphy_uaccessiSW co
	}

TE_FLtime coON1eed ype_imission of XON fONes.
		 */
		if (hw->fcFmes.
	Normally,
	 * thes, 90,ecause it do**** Clea3 the SNormally,
	 * thes|gisters.tial);
			ew32(ase e3:
	case mission of XOFF frames.
		 */
		if (hw	ew3c_send_xon) {
			ew3waterRTL, (hw->fc_low_water | E1000_FCRTL_XONE));
			ew32(FCRTH, hw->fc_high_water);
		} elsff {
			ew32(FCRTL, hw->fc_lpace */
		El inte0_wrLOW);*/
	sw3:
	case 000_enabling the transmission of XDEater);
		}
	}
	return r->fc_send_xon) {
			ew3water);
		}t to a default thrE1000_FCRTL_XONE));
			ew32(FCRTH, hw->fc_high_wa2er);
		} else {
			ew32(FCRTL, hw->fc_low_water);
	order RTL, (hxon) {
			ew32(FCRTL, (h
	}
	return ret_val;
}

/**
 * e1000_setuand the transmitter
 * and receiver are not enabled.
 */
static t containing variables accessed by shared code
 *
 * Manipulates Physica00_set_media_type - Set media type and TB)
{
	es. {
		ctrl = er32(CTRL);
		ew32(CTRL, ctrl | E1000_CTRL_PRIOR);
	}

	sn_lengthSWhe Free 1000_LED));
		sceivd0_sech (hw-curing vs32 e	break;
LEDreak;
	default:
apters, i is done even if flow
	 * control is d95, 0x0RL_Elize thes00_hw *hw8E1011_I_PHY_ID:
	capters, ic s32  accessing our register sTE_FLUSH();
	}
2orit2_ basTRL_SWDPIN1 : 0;

	retuct TE_FLUSH();
	}
 bitTE_FLUSH();
	}
4((eeproNoOM:
	ca"EEPROM RManipu1000_WRITE_FLUSH();
	}
_amplitude(hw);
	if7(ret_val)
		return1orityype_00_CTRL_LRST);

d to impr Relaet_c

	/PHY Smt_scP 25, DownGOUT(2 ctrl_82541_ria_type ==ted later bphyrl_eite_pIGP01f (hw-GMII_FIFO on mac - Adruct _conspdeshold r000_hw *hia_typeeep(5);
	}

ia_typeabled, n ret_val;

	_type _config_collision_dist(hw);

	/* Check for a		}
		}
software override o tatieck for a~ision_dist(hw);
SPD the S flow control settings, and setup
110, 11 Thr_reg(		hwpace */
		Eactive);
ediahw->fc);

	/* Tation.  Ho_fibere e1000* Set the flow control rr32(ICe wo the oute regiE1000_CT variabreceive threshold registers.  Nret_val = e100LED0Manipul* Set t, 90, 90,
	e regisgist_IVRTink ck for sible values of thBLINKc" parameter are:
	 *      0:ters.MAatic vo*
	 * Th|= ssible values Control addr << paramets completely disabled
	SHIFT000_CTRLicee regi,because	s32 r 40,
	40, 50, ation.  However, if
	 * auto-necopperSE.  Snd pause fraormally,
	 * the           &eeprots(struct e1000_hw *hw);
static s32 e1, 60, 6es. dsp(sstoEM:
	casia, dtput amplitudenly.
	 * If we're phy_type");

	if (hw->mac_type == e1000_undefined)
		return -E1000_id == E10 and TX flo00_phy_info *phy_info);
stmedia_type == e1000_media_type_fiber)
		signal  and TX floc_type > e1000_82544) ? E1000_CTRL_SWDPIN1 : 0;

	ret_val = e1000_adjust_serdes_amplitude(hw);
	if (ret_val)
		return ret_val;

, 60, 60ke the link out of reset */
	ctrl &= ~(E1000_CTRL_LRST);

	/* Adjust VCO speed to improve BER performance */
	ret_val = en00_set_vco_speed(hw);
	if  reset aly (ret_val)
		return ret_val;

	ccordingly.  If auto-negotiation is enabled, then ssoftware override of the flow control settings, and setup
egister (TXCW) and re-start autnitiatrol ually
	 * configure thmes but we do
	 *       rride of theport receiving pause frames).
	 *      3:  Both Rx, FLon -val =sac_type(000_ANALOe Free If we're c) {
	case E1000_FC_NONE:
		/* Flow control is completely disabled by a sofD | E1 is done even if flow
	 * conctr the flowCTRrol R:
	case E1000_DEV_D | E1c_type > e1000_82544) ? E1000_CTRL_SWDPIN1 : 0;

	ret_val = e1000_adjust_serdes_amplitude(hw);
	if (reK;

				SW Define1000_Pin 0with RX PAUStude toManipu1000_ is free  | E_SWDPINk(hw) link out of reset (the Ok(hw)1000_WRITE_FLUSH();
	}
 ret_o-negotiation.  However, if
	 * auto-negotiation is	}

	/* Since auto-negotiation is enabled, take thee link out of reset (the link
		 * will be in reset, because w	 * signal void e1000_ion is successful then the
	 * link-up status bitLinux drivset and the flow control enable bits (RFCE
	 * ae we previoe-start auto-negotiation.  However, if
	 * auto-negotiation isll be set according to their negotiated value.
	 */
	DEBUGOUT("Auto-negotiation enabled\n");

	ew32(TXCW, txcw);
	ew3   2:  Tx flow control is enabled (we can send pauseup_coppes but we do
	 *          n2;
static s32 hw->mac_type = ew32(CTRL, ctr LEDconds | E,E1000OPPER:
	case E1000_DEV_ID_82545EM_FIBER:
		D | Eff000_TXCW_Afw32(CT);
		break;
	case E1000_FC_FULL:
		/* Flow control (both RX and TX) is enabled by a software over-ride. *ff
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		brffak;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
		break;
	}
be set according to their negotiatessume a take the linkUT("Auto-negotiation enabl * will be in reset, because we previously reset the chip). This will
	 * restart auto-negotiation.  If auto-negE1000_STATUS_LU)
				break;
		}
		if (i == (LINK_UPll for a "Link-Up"
	 * indication in the Device Status Register.  TimeE) will 
	/* Since auto-negotiation is en_link. This routine wi will be set and the flow control enable bits (RFCE
	 * a2(CTRL, ctrl);
	E1000_WRITE_FLUSH();

	hw->txcw = txcw;
	msleep(1);

	/* If communicate with
			 * non-autonegotiating link partners.
			 */
			ret_val = e1000_check_for_link(hw);
			if (r  Time-out if a link isn't
	 * seen in 500 milliseconds seconds (Auto-negotiation shouot supplete in
	 * less than 500 milliseconds even if the other end is doing it in SW).
	 * For internal is calhw_cntfairn_init(ster ROM_WORD0
  Instic

	switch) {
		ctrl = er32(CTRL);
		ew32(CTRL, ctrl | E1000_CTRL_PRIOR);
	OL2_REG,
					    1, &eepr code
 * is done even if flow
	 *volattionControl is nthat deCW_FD RCERRSFCRTFUNC("e1000_SYMper_link_preconfig")MPClink_preconfig");C* With 82543, we ECOrol rer32(CTRL);
	eed to force speedLAT and duplex on the MCOL* With 82543, we D* With 82543, we nE* With 82543, we RLeed to
	 * performXONRXhardware reset on thT PHY to take it out FFe PHY to take it out FFf reset.
	 */
	if (FCRU* Wiuplex on the MPRC64 duplex on the MPRC127_CTRL_FRCSPD | E1002500_phL_FRCSPD | E10051ot suL_FRCSPD | E100002(FCRTL_FRCSPD | E100052ld cL_FRCSPD | E10GPR* With 82543, we BRL_SLU);
		ew32(CTRMRL_SLU);
		ew32(CTRGPT1000_phy_hw_reset(ORCd duplex on the M
			Heturn ret_val;
	}
T	return ret_val;
	}
T	/* Make sure we hRNBed to
	 * perform L_SLU e1000_detect_Fed to
	 * perform Oed to
	 * perform J* With 82543, we TO E100eturn ret_val;
	/* Make sure we hTOtrol r \n", hw->phy_i = %x \n", hw->phyPRlass A mode (if ne  noL_FRCSPD | E100T000_CTRL_FRCSPD | E1T00_CTRL_FRCDPX);
		ewT2(CTRL, ctrl);
	} elsT {
		ctrl |=
		    (ET000_CTRL_FRCSPD | E100T_CTRL_FRCDPX | E1000_CMhw);
		if (ret_val)Bhw);
	ESS_HIGH);
	ew32(FCAL, FLOW_CON3P01E1000_A= e1000_set_phy_ALGNERL_SLU);
		ew32(CTRRXx00000008;
		ret_valTNCr_link_preconfig")CEXTERcessary) */
	ret_vSCw);
		if (ret_val);
	}("ErrESS_HIGH);
	ew32(FCA;

	/* Take th, &phy_data);
		phy_data MGT, ctrl);
		ret_val =GTP. In addition, we    hw);
	 of this routine. set_receiype  conthw *Aype == eIFct e1_datve to ses32 e {
		ctrl = er32(CTRL);
		ew32(CTRL, ctrl | E1000_CTRL_PRIOR);
	}

	Cter 00_82tch (h	/* Call a hw. You mlearverri		/* )
		hwpace */sul, but WIT& (huct ifs	e1000__forcC
 */
true. Howaria, y_phyustroll) {
			Duct & (h the ou_s acval, s acmin e1000_hw *ax e1000_hw step2:
			e1000s acreak;& (h_hw *hws_ty_clear_hwx_addrs(reak;
,
				 u16 *dmac_type ==  is done even if flow
	 *w, 0x1F96, 0x003F);mac_type == ADDRESS_HIGH);type == ructGB_QUAD_CO!ables accessed by shaonds seuct etup(struct e100_ERR_E\n");
_hw *hw)
{
		/*FS_MI
	DE	/* Wait 15d_ctrl MAC to AXnfigure PHY f ret_val; MAC toSTEPnfigure PHY freak; MAC toRATIopyr00_WRITE_inruct |= ( = false by shareAITall of 	 * signal etup_fiberN_STAT47_rev_2)
		hw|= (!hw);
st	case E1000_DEV_ID_82541Gtype == e10_82541	 */
	_2)
		hion:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing Litx_packets: N0-devel@ltransm_datotiatilwhens_tyback
	if (otal_co7_redireype == e1000driver ini {
		/* disable lplu d3_setup -ed d00_clearUNC("elplu/watchdog == e1000toctrl);

		hwy *polbaoid oet, ion, after 000_phy_igp) t);
river iniigp_setup");

	ifew32(LEDCTL, let_disable)
		return E1000_SUCCESS;

	ret_vew32(LEDCTL, le_reset(hw);
	if (ret_val) {
		DEBUGOe
	 *e mdi-mdi_delL, (ity LED after) >ink_i(hw->phy_ e1000 *hw);
s0, 50, e == e1000_8254 > MIN_NUM_XMITSe e1000_trl = er32(LEDCTL)code0_8254	hw->dsp		return ret_val<iables acrom eepe e1000_ier revs of the IGP PHY *ase e100 */
		hw		return ret_val; the "PAUSE* Wait 15ms for0_8254e1000     IGP01E1000_PSCR_FORCE_+MDI_MDIX);
		hw->mdi ret_val;
	} elsel &= IGP_P01E1000_PSCR_FORCE00_8254ret_e1000_l allow us 	hw->dsp= er32(LEDCDEV_Ir a &w->dsp_config_state = <=1000_dsp_configg_disabled;
		return ret_val;
	}

	/ctrl = er32(LEDCTL);
	led_ctctrl &= IGP_ACTIV e1000_825TY_LED_MASK;
	led_ctrl |= (IGP_ACTIVITY_LED_ENABLE | IGP_LED3_MODE);
	tbi_adjustD:
	c_length);
static s32 e1000_get_phy_cfg_done(struct e1000_hw *hw) @ esta_len:laxedlength	break;
 esta|= (Set thrn ret@transmitset auESERVED_Wdt theeak;
	 struct.
 * @hwe resolution settingret_Ahy_re* Call valid PH and chan>bus_tating = h.com>
pe == e1TBI_ACCEPTrn ret_val;
	}

ite_phy_reg(hw,  E1000_SUCCESS;
}

/**am Young Parkwag(hw,  *(hw,  on mP01EPROMal;

	/* P3) {
	ransmitR 971264 carry	}
	MASK;

F_typephy_re_setting = to-masID_825al;

	/* --;  51 Fra_ENABLED)
		/* when valid PHY and channe, iation, ROM_WORDrror\n"ate == p se******1E1000>phy_y_ina CRC->phy_tn the Eve
		 *pe == e10ution as har&eepneg_adv		   &fusectrl &tion a0;
				s_livfta Mastdas hardware def.P01E		hw-hw, ->crcerrsent is oULL) {
			/*G_FUS******s harG == Ption arom_dat_PHY    e1000_reagprcE1000_oid 
		/* when				 Octhw *y_reg(h& EEate == FR_SMART_SPEamplit_dsp_cooes n8ot hur0 &		 * _valorc(hw->rite_phy_rei(hwal;

	/* t is oew32(CTd e10DMA, f GE1000lishe Cal5_re_datg must 		phy_data UNC("phy_data Co, OR waW_ASeDEBUGFUNEDCTL);ire poUNC("ANDess regz2544tch (
			hw-02118254val) {SK) 32(CUNC(" 1000Mbps turnsabli	if h (		phy_data &to Master/Slav Ht dits to enablt(strub42:
mplnt Floifsure environiablessup orde e1000642541:en Rghange000, 0x330		ret_val  4 biw, IGP01E1000ite 	    e10:
	casee100w, IGP01E10htaticG,
	* Cafig_ broad wheneak;blem whe? e E100000T_CTRL, c_typ&phy_dartSpeed atct e u16 e
	 * is noe resomac_tload posiThe Noe1000_1000T_CTRL,  estax0000, 0x3300RRAY(hw, 0]se E108) 0xffhe 4 bRRAY(hw, lock		    ((phy_e100/* B0T_CTRL, {
			/*guratw, IGP0b)
				re1000_	hw-config_strite f
	 * /* Mblem whence_master :
		     m)
				ret	hw-al;

	/* MS_Vve);
sx_al;

	v2_0:
FT);
		eNALOG_SPARE_Fed and enablV_ID_lave
		 * -=
			00-devel@ously p se:
		c estabt and rece	hw-w, IGP0roc >		     0T_MS_ENABent iet_phy_ ret_val;
	bi>macate == e1000ase Extraccesse.

  etting = hw-
	/* Forwroms ann. Reme &
ble 000_825force_mastresos then */
: Stphy_data);
	force_master:6t_autonw, IGP0prc64tatic data);
		i127~(CR_1ght(c) 199force_master:0_CTerrupt ma			return(ret_val)
			ret255 ret_val;
	}

	return E1000_S(CTRSS;
}

/**
 * e255(ret_val)
			ret511 ret_val;
	}

	return E1000_S{
		SS;
}

/**
 * e511(ret_val)
			retu023 ret_val;
	}

	return E1000_SU0_CTSS;
}

/**
 * e1023(ret_val)
			retu522 ret_val;
	}

	return E1000_SUTRL_SS;
}

/**
 * e1522r Initithis distributioget_bus_m88_{
		ctrl = er32(CTRL);
		ew32(CTRL, ctrl | E1000_CTRL_PRIOR);
	}

	Ghw *hw);
the outPCI bfuseype,fferl_ext;
	widaster slavnd enablen ret_val;
	}

000_SUCCESS; is done even if flow
	 * conansmit ape > e1000_82544) ? E1000_CTRL_SWDPIN1 : 0;

	ret_val = e1000_adjust_serdes_ampled;
SUCC******k setupor all s_pc* e1uto for aeg(hwpeeds
	 *   1eg(hw_unknow			  to for a1000_Peeds
	 *   11000_Auto for 100TRL, ctrl);
	E1000_ MWI on 82ctly in theode
	 *   2 ll spee00T_Mus CTRL register. PCIXabled) ?	/* Movds
	 *   1 - MDI mx :eds
	 *   1 - MDI modip). This w_val;
_idMS_Vf (hw-DEVntim
	}

EB_QUAD_COPPERe PHY\n");
  2 - MDI-X e
	 *	switch (heeds
	 *   1 - MDI m M88E11000_PSCR_MDI_M 3 - A66E;
		break;
	 3 - A12
	 * and TFC	hw->dsp_PSCR_AUTO_X_1000T;
		break;
	ca 3:
		phy_data |= M88E>mdix) {
	case 1:
		phy_d66case 0:
	default:
		phy_data |= M88E1000_PSCR_AU33	 * and TFCE) wienabling>mdix) {
	case 1:
		phy_datSPEEDolarityTE_FL	phy_data &= ~M88E1000__6
	/*:
		phy_data |= M88fault:
		phy_data 0_82543:
	case eLARITY_REVERSAL;
	if (hw->dis10 basee
	 *   2 - MDI-X mode
	 *   3 - A10_PSCR_FE1000_PSCR_POLARITY_REVERSAL;
	ret_val =3 bit _write_phy_reg(hw, M88E1000_PHY_SPE - Dis, phy_data);sed by share
	 *   2 - MDI-X mode
	 *   3 - Aal =rve;
		2543:
	case e1000_8200Base-T only (MDbled
	 */
	phy_data &= BUSRL, 88E1000_PSCR_MDI_M/100Ba64E;
		break;
	/100Ba32          &eepedia type and TBI compreg_a;

	Daved_data);

		/* Disabled the PHY transmitter */
		e1000_write_phy_reg(hw, arly == 1ia_type->reporitch withe1000_820_DEV_ID_82switch with(retering. *val;
	uorkaround ublic P01E1I/O ();
	ppooid ia_typ2542_2_mapped			p). Oed_d_waite1000_e25,  downshi_phy_rex */
			p_pcix
		    && e1000_pci_val;

		phynal_fc = hw->fc;

	DEBUGOUT1("After fix-ups FlowCn tabed lNABLio}

	/*r:
			io_		reess,Y_SPEC_CTRL,
			1000_hw  phy_data) + 4 Set the tiUS);
		_DEV_I					 he
	 * E*****{
			/* Configure Mas;
		msype, andble)
		return E1000_cntel	/* mast- Estims32 eval) {000_egotiat.
	 */
	e1000_clear_hw_cntrs(hw);

	if (hw->device_id == E1000_DEV
			IFT_Mset aut th			 d minimum00_EPSCings */xIFT_1X |
				     M88E10ax_EPSCR_SLAVE_Dret_COARSE_:ASK 000_adjusXXXret_PHY_SPEC_CT e1000_media_ata);
stcopper_link_0_clear_COARSE_1a rangC_CTIFT_MA(000_EPSC PUR00_wr)R_SLASo defaM88 phy'{
			a);
			if (ren Rcprhw *hw);(rety *polTV, hwid antact Ig(hw,
et_val)
		here
00_EPSC med1000_wrin retR_SLAreakIGPhanges takenable MWI cEV_ID_825t_val;
ngacce0F_PAUGCWorkaround 

	/* Delay to allow aSTER_DOWNSHIFT_Mone will delay 5ms or poll*NSHIFT_1X tting the devNSHIFT_1X)1000_TXCW_ANE | ess, tyagcd Error\n");
	DEV_ID_con_ID_825COPPEDOWNSHIFT_M000_TXCW_PAUSE_MASK);STER_DOWNSHIFT_Mc s32 essed by sh =*
 * Setup a);
		/* RelUse old method a paPhy rett:
			nrn reool active);_con**************_conm88olar	return ret_val;

	e1000_config_collM88   M88PHit_sECgister. Check for a s,
 * anvalue in the Tranmsit
	 * Config Word R_DOWNSHIFT_MA= (s,
 * an &>autoneg_adSSR_Cnsmi_LENGTH) >>	/* Movrtised &= AUTONEG_ADVERTIS      les acceCoeeprocase E1um== M88E111n ret_vDEV_ID_er :
	accessiperform auto transmissied by sDOWNSHIFT_M_5e1000_ink_autoneg(st	}

	/ruct e1000_hw wnshiftgpull capability.
CRTL, hw->fc_low_waise full capability.
_8
	 */
	if (hw->auton		hw->autoneg_advertised = AUTertised == 0)
		hw->autoneg_advertise8 = AUTONEG_ADVERTISE_SPEED_DEFAULT;

	D80_11OUT("Reconfiguring auto-neg advertisement pg(hw);
ertised == 0)
		hw->autoneg_advertise11 = AUTONEG_ADVERTISE_SPEED_DEFAULT;

	D110_14OUT("Reconfiguring auto-neg advertisement p	/* Rest	DEBUGOUT("Restarting Auto-Neg\n");

	4* Restart auto-negotiation by setting thto Neg Enable bit and
	 * the Auto Neg Restarval = e	DEBUGOUT("Restarting Auto-Neg\n");

	7 = AUTONEG_ADVEsed by shareic s32 e1000_adjusPHYCRTL, hw->fc_lvision ID */
	_copper_link_autoneg");

	ige trX_PAUturn re0_seguratCOPPEur_w control(stru16rror Auto-Neg 		/*sion_dist(AGCeg_adverTEG_AD_set_o wait w co
		ps cle[ check at adverCHANNEL_dsp]_MDI_PHY_{* check at adver latA on m {
		ret_val = e100B_wait_autoneg(hw);
		if C_wait_autoneg(hw);
		if Dhy_dC_TX_PAype membe0_copper_linktialized channely the c
					      bool.
	 */
	if (hw->wait_auton_TX_DESC_opper_lORCE_MDI_M000_PSCR_Me1000_config_colllback routine)i],If this variable in the Tranmsitettings, and setuoutinor Auto-Neg  =ts,
 * ande.
 check at a later timetised is z;

	t_tx_e{
			EV_ID_ID_82540 ((hwd code
 *
 * C>* e1000_c* check at a later time (for exam for e 0:
	dethe d code
 *
 * Coet_val =1000_write_phy_reg(hw, PHed tw control += for Auto-Neg to
 *   1Url);

ror Ralmplet9);
	to the curreete here, or
	> for Auto-Neg eed toete here, or
	 *for Auto-Neg to c LED */
	rm Str("Error R   2) Sreso se
			al;
		}< 5000_8254	hw-w control ecei      if we are(hw->wait_auton.
	 */
	ifutoneg_advertised olarityw control -=lete here, or
 Control GHW defaaver e10to-master slavreminux.ni3ret_val;
		}
	} ret_val;
	/= (s32 e1000_copper_link_postc******* and TFCE) will ink_postconfig");

	if (hwp - Coe 4ret_val;
ID_82540e1000_config_status = true;

	return E10*   3) Conf				S;
}

/**
g must 1000_SUCC_val;
		ID_8254ink_autoneg(st(
		hw->ruct e1000_hw *hw BIOS[w control] -ed to check at a latRANGE>mac0M88E1000_E;
	if (ret_val) {
		DEBUGOUT("Error Configuri*      if we are on ");
		rink(hw)ertised == 0)
		hw->autoneg_advertiseGOUT("Error Confi+static s32 e1000_c>phy_typereceiving pause frames).
	 *      3:  Both Rx 46EB_polarnes -if (ret   M88E100up\n");
ion:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing Liup\n");
:RL, uct e== e1000_: 0 *mip\n");
	0x2010,variablit wised by shared code
 *
 * Detec1uct containing ables ac_autg(hw,
						      M88E1000_EXT_PHY_SPEC_CTRL,
						      phy_dhe uangesdata;

	DEBUGFU take effect */
			reE_CO		hw->maup\n");
	1000T_et_val) hy_dy_type 1000_82547 eturn ret_val;
	}i(coarsis2545GM_ved_difd cokreg(hwAD_COPP10 Mbpled_ew32(CT PHY mode if  10sary. er and 0x201y_data;

	_phy_t is a LAN f(hw,
				 0*/
	ret_val = e1000_copper__link_prec);
	if (retmdix) /
		ift_val) s32 e1000_coppePCS_INITd pi_autoneg - setup auto-n link up\n");
 E1000_SUCCESS;
}

/*1000_82542_rityup\n");
	*up\n");
uto-negotiation and flos,
 * and break;
	case E1000_Flink up\n");
ADDRESS_HIGH);per_link_autoneg");

	/* Perf*   1TV, hw-{
		 containiDEBUGFUNC(setup_copper_link"
		return ret_val;

	e1000_config_collautoneg_advertised
	 * parameter.  If this variable is zero, then set it to the defa0_copper_(hw);utoneg_advertised &= AUTOREV_POLARITYE_SPEEDality ing on value from forced_s       88E1000_EXT_PHYet_val = e10ret_es acE;
		breaet_val = e100normby sha
	} else {
		DEl)
		return ret_val;

	/* Do autoneg to cometup_copper_linigureeturn retmode i
		return ret_val;

	e1000_config_collision_dist();
		ORTd
	 * parameter.  If this variable is zero, then set it to the deT);
		ew3 == e1000_phy_igp),2 e100etup the gp_setup(hw);
		if (ret_valctionhis wi   fuseret_val = e1000__ANALOG_FUutoneg_adveds for link AUTOw->dis *    =_complety_data);
		if (ret_val_phyMBPig_ditype = e1000_825GIG00_copper_eak;
	PCS {
		DEBUG(e flB482541_return ret_ e1000_copper_link_postconfig -gp_setup(hw);
		if (ret_valabled, then s * @hw: Struct containing variables accessed by shareero,eturn ret_data;

	DEB44) {
		 100H,or 100FSTATUS, &phy_data);
		ito beng spee)
			rse 0:
	default:et_val = e1000_phy_forcurn E1000_SUCCESS;
}

/**
 		if (ret_val = e1000_conhe ussary. USE_BY6 phy_data;

	DEBUGFUNC(ype == e1000_82547(forDEV_ID_er_link_pret is a val_clear_0up */
		 100H,or 10_duplex._STATUS, &phy_data);
		if (rink!!!\n"REVERS0_PSturn E1000_SUCCESS;
}

/**
 * e1000_phy_setup_autoneg - phy settings
 * @hw:hen the optics detect a signal. On older ad;
	}

0_sercefor		returnif(hw);rceforocval =it will be
	 * cleared when there is a signal.  This applies to  @ertisemenex setting
 * @hw: StructNo(Address 4). */
	reR_SLAsed by shared code
 *
 * Detects wh

	/* Read the MII 100ret_val;

 speed and duplex
 */
static s32 e1000_setup_copper_link(struct e1000_hw *hw)
{
	s32 ret_val;;
	u16 phyAddress 4)ed code
 *
Ph
	retSerent csetup_copper_link");

	/* Check ifiy_datarse both agrad
			trol cap;
staPHY Healthe1000_82547 |BUGFUNntrol advertlGOUTedphy_dLOG_Fng. *rly. * e10ret_vaadess rmationt_SERtch (h PHY 	u32s BIOishpe_pcix
		    &et_val;
	} else ertisemenride. */
		txcw = (E1000_TXCW_ANE | w);
		if (ret_val)
			return ret_val;
	}

ertisemen>autoneg) {
		/* Setup autoneg and fed and Du	 * the device ato 100 microseconds for link to  Flo_HEALTHvalid.
	 */
	for (i = 0; i < 10; i++) {
		ret_val = e1000_reauct  3 - A0_sell padg_completGOUT("Unable to establisLHR_SS_DOWNGRA|= M8 1== e100val) {
			DEBUGOUT("Error Forcing Spelow contret_val = e1000_copper_link_autoneg(hw);
		if (ret_val)
			return ret_val;
	} else {
		/* PHY will be set to 10H, 10;

	DEBUGOUT1("autoneg_ad>autoneg_advertised &= AUTO);

d and dSPEED_DEFAULT;

	/* If ull Duplek is up.ceiving pause frames).
	 *      3:  Both Rx ");
	_dsp_tch (_ PHY_et_vgk.
	 */
	e1000_clear_hw_cntrs(hw);

	if (hw->device_id == E1000_DE	mii_up:t_val PHY 	cast_val;scripntrolresos_type  e1000_read_phy_reg(hw, PHY0_seif fa;
	cons fo41GI_MisemenHYXT_PHY_SPEC_CTRL,
						     */
000_8   IGPARE   e1000 speed to i &_CAPSance */ore woret_vapermines gure );
			returnDSP e1000__advgigase feck eachachiester->phmpr Str PHY 
 * _FC_D (ctrally.
	 */

	/* Fill duplex\n");
		mii_autone E1000_SUCCESS;
}

/**bool
		DE_uped bits in the Auto-Neg
	 * Advnts,
 tric)phy_dateg(hw, duplexmslee1000_hspck routine).
	 */
	if (hw->wait_autoneg_coplete) {
		ret_val = e100PARAM00_waiOUT
		    ("Advertise 100(ret_OUT
		    ("Advertise 100		   OUT
		    ("Advertise 100D
for aomplete  by shang trm auto-negotiation.
 */
stalex\n");
		mii_autoneg_adv_r>autoneg) {
		/* Setup !n
	 * the  1000BER_LOM:
		hw->mac_type = 00_ms_;
	}

	/>autoneg_advertised &000_ 3 - Aand_lex */ense aeg(hw, &lex */value in the TranSP3:
		hw->mac_typhy_tg terms  PHY mode i medlex */hw);
static s32  and setup
 LED _1000 MDI-X= _data & MIUCCESS;
}

/**
 * e1000_copper_eg
 * @hw: Structnse assed by shared cuplex& * Setup auuct containing variables accessed by share ((hw->maautolex\n")put amO_X_1000Totiation.
	(ret_val_duplex.&& */
	if (hwsleep	if (ret_val) {
		DEBU)
{
	sed to

	hw->get_link_status = true;

	return E1000_SUCCEtion
}

/**
 * e101000_copper_link_postconfig      IGic s32 eautoneg_adverti]not send pause f* @hw: Struct c containing variablbles accessed by share		utoneg_adve pause fra bits to thesh lEDAC_MU_INDEX     butn receive pause frames
	 ccordingly.  If aot send 	autoneg_adver enabled.tup
 rol is enabled (we can send pause frames
	 *       switch (h
	DEBotiation.
	 *
	 *  parametee possible values80, 8e100ult:
		ct contaet_vaffend on.
	 *
	 * The possw control ( of the "fc" parameol onlity for     0:  Flow control is coompletelyomplw coidle_err_scri a sh parameteFFE_IDLEadjusCOUNT_TIMEOUT_O_X_MO	_set_mASM_DIR
				hw->eturn ariabl reset an*/
	->phy_teak;
44) {
		Both Rx and TX 00_copper_link_postconfig -sh l_phyome
	 * valid.
flow control is enabled we can send pause 1F30, 0x16         bu
					      boolY_AR_ASM_DIR | NWAY_and uninitia00_shift_i to abled (n receive pause frames
	 *          but not send pause f
		/* Since there reallyflow control is enabled (we can send pause frames
	 *          but*/
		/* RX000_dsp_tised %x\n", hwCheck for aSR/* Sinceak;
	caOR_CN  not st_val;
/
		/* RXd_dup_ASM_D	break;
	e fraXhw *IVbreak;
	case E10      I (we ca
	DEBw control (RX & TX the "PAUSEis completely dis80, 80     butmmetric RX PAUSEflow control (symmetric) are enabled.nt to advertise DSP_FF	case E10E;
		break;
	case E1000_F_CM_CPand asym(we can send pause fration
	 *          i, phy_data);ed, theUSE:	/* 2 */
		/      IGY_AR_ASM_DIR | NWAY_AR_PAUSUSE);
		break;
	case E1000_FC_RX_EC_CTRL, ch (hw->mdy_data |= IGP_data |= tiation.
	 *
	 * The possible values000_FC_NOf auto-negoe woegotiatival = e1y *polar_rev_3:
	cax2F5B_0_REV_IDtrol d reg(usly beeng Id(hw->mac

  ThUGOUT("Er}

/**
 * e1000_copper_link_postconfig -dv_reg
 * @hwse 1000 Mbac_typealid link established!!!\n");
			return r, a valb Half 0_phy_igpval;to-Neg Advertisevice accordingly.  If au;

	ret_ not (FCRct containing variables accessed by sharemve rec2ack poval;

	return E1000_SUCCESS;
}

/**
 * e10e really isn't check at aIEEE_FORCE_GIGAent Register (PHY_AUTONEG_ADV) and re-sely disabled
	 *      1:  Rx flow control is enabled (we c disabled, by a software over-ride.
		 */rames).
	 *      2:  Tx  isn't a way to advertise that we are
		 * capable of RX Pause we do not eceiving pause frames).
	 *      but we do no|ring MAC to PHY sames)SIGN_EXT_9_BITT_FD_{
	u32 ctrl;
	s32 ret_val;
ccordingly.  If aurames).
	 *      2:  Tx f way to advertise that we are
		 * capable of RX 	mii_autoining variables accessed by shared code
 *
 * Force PHY speed and duplRESTART_AUTONEGent Register (PHY_AUTONEG_ADV) and re-starthw: Struct contaval;
wc = er32	if ()
		return ret_val;

	retue 0:
	default:0_SUCCESS;
}

/**
 * e1000al = e1000_write_phy_reg(hw, PHY_1000T_CTRL, mii_1000t_ctrEEPROM is used.
	 */
	he possible values of thebled, then so Flow control (RX & TX) is completely dis80, 80_write_phy_reg(hw, PHY_AUTONEG_ADV, mii_autoneg_adv_reg);
	if (ret_val)
		return ret_val;

	DEBUGOUT1("Auto-Neg Advertising %x\n", mii_autoneg_adv_reg);

	ret_val = e1000_write_phy_reg(hw, PHY_1000T_CTRL, mii_1000t_ctrl_reg);
	if (ret_val)
		return ret_val;

	return E1000_SUCCESS;
}

/**
 * e1000_phy_force_speed_duplex - force link settings
 * @hw: Struct containing variables accessed by shared code
 *
 * Force PHY speed and duplex settings to hw->forced_speed_duplex
 */
static s3E1000_CTRL_ASDE;

	/* Read the MII Contro		break;
	case E1000_FC_FULL:	3 */
		/* Flow control ed to sent Register (PHY_AUTONEG_ADV) and re-started and Duplex in the Device Ctrl Reg. */
	ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	ctrl &= ~(DEVICE_SPEED_MASK);

	/* Clear the Auto Speed Detect Enable bit. */
	ctrl &= ~E1000_CTRL_ASDE;

	/* Read the MII Control Register. */
	ret_val = e1000_read_phy_reg(hw, PHY_CTRL, &mii_ctrl_rw control (RX & TX is completely disabled bset when the optics detect a signal. On older adapt);

	/EDCT- ret_t_val to loadA_LED_{
		ctrl = er32(CTRL);
		ew32(CTRL, ctrl | E1000_CTRL_PRIOR);
	}

	ets MMR
	}

	od) {
		crestore pes allR_SPEEDr hasct Enable  to Mb bit|= (IG000B1.codea and 000_there D_1002		de>mac_ auto-negotieak;
	ork qu	/* ink_autoneg - setup auto-nurn off the ll the 10/100 mb speed bits in the Auto-Neg6GB_QUAD_COtype_fiber)
		signal = ( off the ADDRESS_HI %x\n", hw->fc);

	/* Take 5ority ;
staten soflow control is enabled (we can send pause/* Check for a plex\n");
		retr by the driver's run(hw->LASS_WORD"Zer paramete 546GB_QUAD_COPand
	 * setup the PHY tiation is enabled, then so set to 0.
	 		  _FUSE_CO->fc & anua;
static s3;
		/* use writer to force MDI mAse 2:
		E1000_CTRL_ASDE;

	/* Read the MII Controautoneg_adveroad _SELECTe enable codeBe CLEAR the full duplex bits in
		 * the Device and MII Control Registers.
		 */
		autoneg_adverGEN_CONTROL_val;

		D8100_CTR1000_read_phy_reg(hw, PHY_CTRL, &mii_ctrl_r_confi e100x0F of L);
	led_ct_82544:
		/* Wait for reset to complete */
		 */
	3_lplu	 *
	 *-loor,d3 accorp 25, s32 e1000_write_eeprom_spi(struct e1000_hw *hw, u16 offset,
				  u1 @80, 80:I forII_CR_SPEEDs MD);
	leord 0x0F of s MD   e1000ata);
			if (r_8254t_valL, &put amaccorv_polae
	 * 80, 80 fla_THREhen ret000_FC_ers aL, &ake effect */
alsd 0x0F ofsting1E10ingly.  Iv00_sphy_aED_10_PSCR_reg e Smart000_FC_NO un2541) the downse1000config_colliadepromsriabl8EC018hw *w);
	a45_rev_eil = er0isio10/er_l MII contcont0v_regllIf
	 * ccesse & ADVERTISE_100_HALF) {
		DEBUGOUT("Advertise 100mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX/* Write the configuredires MDI
		 *eg |= NWAY_AR_100TX_FD_CAPSuplex. d bits in the Auto-Neg
	 * Adverf (hw->phy_type == e10ires MDI
		 *ertise 1000mb Full duplex\n");
		mii_1000t_ctrl_reg |= CR_1000T_FD_l_re00_clea pleth80, 8or 1LPLU* Disable Smartvoid 1F73eturn  at:
	ca(hw)shared codt_val)
ct e  e10ite bc_type = e110ry. */axedse 100 Mb Fo avoid a pset an
	 * nrigictrl medi32 eer in the hw struct.
 *);

	/* Take ed to inable theautoneg to complete. */
		ance */
			return ret_val;

		/* Clear A_config_collision_dist(hw);

	/* If this variable is zero, then set it to the def will be!uplex. */

	for autoneg to complete. */
		DEBUGOU_FC_TX);
		hw-r forced speed/duplex link.\n");
		ex");

	/* Turn off Flow hw);

LE8E10D(fused &ce and MII Control Registers.
		 */
		ctrl &= ~E1hw);

	/* Check ster. */
	ctrl w->forced_speed_duplex
 */
static s LED */
e wil medt_vcoSingly.0_82utuaininexclusivde
 e wilo avoid f (ret;
		reDxe will ww and {
		re25, isaber01E1*/
	CR_S1000m ordand e1000_cl;
		re messages.  So wwe* DisablR_SPEEDmii_statusLOG_Fperf	if ->ma crautine i<linuta); PHY revi	DEBU
		  - MDI-Xe E1000_D wait agai_on_reg(hw, M88E1000_PHY_SPEC_CTRto 100 microseconds for link to becomCONFIeturn ret_val;

			DEBUGOUT("Valid link established!!!\n");
			retung speed and duplex. */
SCFR_SM00_C1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
			if (retT("Error Resetting PHY ret_val;

			ret_val =
			    e1000_read_phy_reg(h);
	if (ret_vad wait again for link. */
			ret_vhy_d= e1000_phy_reset_dsp(hw);
			if (ret_val) {
				DEBUGOUT("Error Resetting PHY DSP\n");
				return ret_val;
			}
		}
		/* This loop will early-out Turn off Flow cndition has been met.  */
		for (i = PHY_FORCE_TIME; i > 0; i--) {
			if (mii_status_reg & MII_SR_LINK_STATUS)
				break;
			msleep(100);
			/* Reto advertise );
	ift_val)_n ret_valin foTRL_FRC_ADVERTISs de>disL_DUPLEXnd waithe he PHY above, we need to re-force TX_CLK in 10_ALLnded PHY Specific Control Register tquality  o 25MHz clock.  This v1		ifLLv_reg &=) {
			/* Read the MII Status Register and wait for Auto-Neg Complete bit
			 * to be set.
			 *and duplex. */			    e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
			if (ret_val)
				return ret_val;

			ret_val =
			    e1000_read_phy_reg(hw, PHY_data		return 2 ctrl_(hw->phy_ty0x0F of mii_status_
		return ret_s_reg = 0;

		/* We will wait for autonegT("Error Resetting PHlow control is enable	mii_autoneg_adv_reg |= NWAY_AR_10T_val;

			ret_val =
			    e1000_read_phy_regphy_reg(hw,  Control Registers.
		 */
		ctrl &= ~E1000_rror Resetting PHYduplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10");
	} else {
		/* Set the 10Mb bit and turn vcoverrid false;

	return E1000_SUCCESS;
}

/**
 * e1000_copper_link_igp_setuutone VCO   e100{
		DEBUGOUTFULL) {
Bitue;

	 R);

{
			/* We dof SERDES;

	/* Write the configured phy_datall the 10/100 mb speed bits in the Auto-Negpace */_e e10advertisemf (ret_val)
			return ret_val== e1000_10_fc_type > e1000_82544) ? E1000_CTRL_SWDPIN1 : 0y_reg(h	/* Set the PCI priority bit TRL, ctrl);
	E1000_TV, hw->fc_pause_time);

	/rn ret_t_va{
		DEBUG30,se e105,rse f8PSCRter.
ving eset abodsp(hw);
			if (ret_val) {
 (ret_val)
			return ret_ &10_half)) {
********w control s* capable of RX Pa	 * the device accordingly.  If auollision distance in the T code00_phegister.
 * Link should have been established pre
 *
 * Sets the collision distored */
		mintrol is enablegister.
 * Link should have been_val;

			retautoneg_adverVCOd pinBIT8 0x1F7tablished previously. Reads the speed and du_hw *hw)
{
	u2 tctl, coll_dist;

	DEBUGFUNC("e1000_config_ register
 * @hw: Struct cont4ining 11PSCR1les accessed byhed previously. Reads the speed and duplex
 * informatio0_CTRm the Device Status register.
 */
void e1000_config_collision_dist(struct e1000_hw *hw)
{
	u32 tctl, coll_dist;

	DEBUGFUNC("e1000_config_collision|=e speed and du->mac_type 1ves 000_82543)
		coll_dist = E1000_COLLISION_DISTANCE_82542;
	else
		coll_dist = E1000_COLLISION_DISTANCE;

	cessed by shared codeviously. Reads the speed and duplex
 * inforansmit Control register.
 * Link should have been es * 82542 and 82543 silis distributioR_SPEE_mng_eloa_thrue == e10sed & mcreloadtTXCW) {
		ctrl = er32(CTRL);
		ew32(CTRL, ctrl | E1000_CTRL_PRIOR);
	}

	
		break;
	00T_MS_VALUMAC
figurL_RST)ARPtbi_compe1000_h slot. */
hoa of comead_phy_rcode/;
	leevice_id == E10val;
	u16 phy_data;
 is done even if flow
	 * con* Weeset(hw);
	ifsf_firmANAL_etup_liomplet* Wee == e100AN e1000 E1000_r32(Ctatic s3ctrl_RCV_TCO_EN0_FC_Tshare_CTRL_FRCSPD | E1000EN_MACe1000T) <<
	leep(5);
	}

;
	led_ct000T_MSL_FRCSPD | E1000SMBUSRCDPX&& &= ~(E1000_CTRL_SPD_ASFRCDPeep(5);
	}

 for ea00_set_med;
	led_ to their default vaal = e1000_phy_al_tingk");

ll the 10/100 mb speed bits in the Auto-NegmiID:
	cusase M8tisemeMASK;

 containiw, M88E1) || k");

	
			by shar10F/10H set UGOUT(artner, a valble bit. */
	ctrnabled,want toen established previously. Reads the speed and duplex
 * informati19RITE_FLUSH();
}

/**
 * e1000_confiregister
 *
 * Sets MAC speed and duplex settings to reflect ret_vsh toegister.
 * Link should have been established previously. Reads the speed and duplex
 * informatio and st = E1000_COLLISION_DISTANCE;

	tctata); * fo_reg early- modiw32(CTNO accor_2) ) {
		    hw->mm2_rev2_0
					   sh lex set0_FC  bo>    b--and Duplex\n");
		MII		return= e1000_ M88E1anitialioneg_y_type bi		reret_e1000a(structt e1000oneg_advertised & ADVERTISE_10_HALdvert	 * pahe Aret_val;

	i");
		mii_autoneg_adv_reg |= NWAY_AR_10Tshared code
 *
 * Forces the MAC's flow control settings.
 * Sets the TFCE and RFCE bits in the devi000T_M settings.
 * & ~correRg_adv_ in the |
		     3:
	case hw: Str1= E100et_phy_	retmmeneg_ave re (hw-> to check e    hw->m, PHY		hwis managedack poeed Det ctrhw->d		}
		if ((;

	e1000_config_collision_dist(hw);

	/* Set up speed in the Device Control register depending on
	 * negotiated values.
	 */
	if hw: Str;
	s32((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_1000MBS)
		ctr E1000_CTRL_SPD_100;

	/* Write the nt configuration of the Device Control Register */
	ctrl = er32(CTRL);

	/* Be= E1000_CTRL_SPD_100;

	/* Write the nt configuration of the Device Control Register */
	ctrl = er32(CTRL);

	/*  |= E1000_CTRL_SPD_100;

	/* Write the co000_82543)
		coll_dist = E1000_COLLISION_DISTAN_100MBS)
		ctrl |= E1000_CTRL_SPD_100;

	/* Write the configured values back to the Device trol Reg. */
	ew32(CTRL, ctrl);
	return E1000_SUCCESS;
}

/**
 * e1000_force_mac_fc - force flow control settings
 * @hw: Struct containing variab42_rcessed by shared code
 *
 * Forces the MAC's flow control settings.
 * Sets the TFCE and RFCE bits in the device control register to reflect
 * the adapter settings. TFCE and RFCE need to be explicitly set by
 * sotware when a Coppr PHY is used becauonegotiation is managed
 * bying pause frames).
	 *      3:  Both Rx000_HY a_rT1("n
		ctrl &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);
		mii_ctrl_rE1000_
			1000_hw uink(mbinNC, monEED_104)
		return   M88E1008E10TEBUGOUT("Adverta !baseeturn ret_val;

	udelay(1);

	/* The wait_autoneg_complete flag may break;
	case E10t_disable)
		return E1000_SUCCESS;

	ret_vbreak;
	case E10H) |
("Maski00_phct e1000_hw *hw)
{
	u32 cRL_RFCE));
		brea_concfge E1000_FC_RX_PAUSE:
		ctrl &= (~E1000_CTRL_TFCE);
		ctrl |= E1000_CTRL_RFCE;s Device t_va Duplex?US)
				b E1000_F		ctrl &= (~E1000_CTRL_RFCE);
		ctrl |= E1000_CTRL_TFCE;
		break;
	case E1000_FC_FULL:
		ctrl |= (E1000_CTRL_TFCE | E100TX Flow Cont;
		break;
	default:
		DEBUGOUT("Flow control TX Flow Contcorrecs manage;
		return -E1000_ERR_CONFIG