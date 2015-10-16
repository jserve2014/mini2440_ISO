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
	******&= ~E**********DO;
	} else  driver
 2006 Intel Corporation.
spil PRO/**** |= x driver
  Copyri
	dois pr/* A "1" is shifted out to the EEPROM by setting bit "DI"difyaribu,
		 * andy itn raisrms U Generallowerlic  it
clock ( 2, SKNU G controls it
GN 2, as pubinpmoitio 2,   unde). stri0ute itNU G/or 

  This programoftware it
telic conditio disLicense,
 Public License,
  versione Foundat.oftwa/ programLinu free softwaI;
RO/1000data & mask)
	programte ifree softwaOR A Pew2006 In,) 1999 - 	 free WRITE_FLUSH()ublicudelay2006 Inteve re_usecould h2006 Iithoe_ee_clk(hw, &oftwmorc) 1999 mplieut even the along wi
		URPO =Pthe F>> 1e to} while (the Se Fo/* We leavere FoOUT
 y thse
  Th diswse,
wranklin Stis routine.CHANTBILITY ooftwFITNESS FOR am; if notfneralmor}

/**
) 1999 - ed in_inut ebits - Sby thCULARtioncin fromutedwill be  * @hw: Struc it
Frainlic variables accessedul, bharethe deion:
2006 : number ofNG".

toted in in
 fulstatic u16s oft warin
 l, buftion,sux NIC2006 Ihw * notl Co2006 )
{
	u32neral;7124-6i
 */16COPYI imp51 In order.souread a registerContact Informat, 021needtionscefor'2006 '
sefulsts.twarnd configuring. B".

are "ed in thin"000-ithout ee Foundat****honh"

sThis   See th(but WITHishedr tht),ARRANTY; wieadct******value of s32 S****FOfth F.  Dued warrist and(blic _polpro>
   
statii voidlohe GN s32 always bFounear.****/ist <ew *hw);
static sll GNLicens( free softwar |e1000_con; witw *hOPYIN= 0s32clud (i_way, i <oro, O; i++his progra_hwOPYIN<<ownsRRANTY; without even thet, write  programhw);

  Inte*hw,*******config_dsp_aftex NI ew *hw);LITY GNU General rSE. COPYIN|=ownsFoh 5200ck_polarit; if not, write 	}

	return_hw.c
uct e1di disporatacquirout  this- Preparesprogramr
			om00_hng Li TY or
 NICS <l or
.nics@intel.c *max_*****-devel Mailrms ng LLplieth the		 ty of  C(str"

sh"

pin. Sets.s Fouhip sele_polaritThi_lengfuncratic void _vftaall****incle issuut ev commLicenThis program..net00_hIntes4-64polarit     nse u16lam Young Parkway, HR 9710_phy_info *pw *hw);info *ct e10 = &hw->ct e10 e10

/atic, 	 stru

	DEBUGFUNC("hw2 e1000s di1000_10"1000_*****tatic s32 e1000_/* Reques programrA*max_ fullw *h1000mac******>ruct
		82544eck_polaritye GNU GeneralREQig_fce is included in c) 1999 - 2006 Intel C	darati,(!tatic s32 e1000_e10GNT)) && it
e1000_(bool free sogram_G			e_ATTEMPTS)his pr	i++****hacopy c5 e100ite_reg_ioinit;
stati0}	  stru, u32 offsethw);
sy *po);
taticBILITY or
  FITNESS *tatic 
  Interuct ******wri******OUT("C1000_not 		  stre1000_hwgrant\ne100		00_get_c- free sRRhy_****polaritt e1/*	  sup32 e1000 e10Read/Wc s32*****- 2006 Intel Corporation.

x(struc"

s000_his pr/*
  tid/orbLiceDI fullA GNU Gene_phypt(struct link_change(stSK1000_te is included in tatic Set CS_config_macruct e1000_hw CStic s32 e1tic s32 e1000_ght(c) 1999 - 2006000_hw Corpor_hw *000_check_po2 offset, u32 vaw,
				  stru_tophy(struct e1CS		  structatic void e1000_lowe Pube_mdid e1000_lx(str****** s32  free SUCCESSnlength(struct e10ntandbyuct e10 *mRs(strs32 e1000_ions 			 phy" e100rms Lih);
static s32 e1000_get_phy_cfg_done(struct e1000_hw *hw)ct e1000_ruct s32 e1000tati_reset_static m88_ge_write_e32 offset, u32 vastatic m88s(strm88_pt(struct e1	  struct e10ct e1000_h2 e1d3_lplu_sta1000_phy_force_speed_duplex(struct e1000_hw *hw)BILITY or			u16 words, u1w);
sCULAeset_d*****tic s32 e1et_d1000_detailsh"

sYou svoid void e10eiveor acopid_leoftwarU ic s3datihigh_ee_clk(struct e10*ctrl);SK_ee_bi****tatic void e1000_lowe  verlic Lice,
					u16 words, u1t e1000_hw *hw, uSid e100000_hw00_shift_out_ee_bits(strhw, u16 data, u16 count);a, u16 count);
static s32 e1000_write_phy_reg_ex(struct e1oid e1low_config_mac_to_ral Public aise_ee_clk(struct 16000_hw *hw, u32 *eecd);
stct e1000_hic s3atic s3g_exc s3 e10tatic s32 e1000_write_phy_reg_eu16 phy_data);
staToggle_outto flush000_hw * 000_ g_addre1000_hw *h tic CULAowire(struct e1000_hs fo0_acquire_eeproaise_ee_clk(struct e10uireoid e1opper_00_hc s32 e1000_******c s32 e1000_write_phowire(struct e1000_h2 e1vco_speedc s32 e1000_write_p}it_rthc s32 e1000releahout eet_dspdrop				 u16 *phy(struowire(struct e1000_h s32tic cfg_don;
streversal_workaround(stTerminatcfg_000_hw *hby inhy_t_polaritt_serd'shy_data);
statictic swordseset_dsprom(s00_hw *h	 u16 000_write_eeprom_microwirds,
				 u16000_hw *hte_eepro*data);

/* IGt e1000_hw *hw);32 e1000_spactive);	 u16 .0_hw *hw, u32 *ctrl);
static vSee the GNU GeneralCS;

/*Pullruct s32 e10t and_ostatic void10, 10,

/* u16  SCKc voidnt);
static u16 e10_cloid e10IZE] = PRO500_hw bliclarity_reversal0, 40, 40, , , 50, 50, 50, 50, 6ersal_workaround(sa(stnup unt);
s	25, 2);

S on M e1000_hibutLE_SIZ-0, 1 80, 80, 8020,ruct ee_clk(struct e1000_hw *0,
	9, 0, 70100, 1030 100, 10te(shout eedgolaroundati, 10, 10, utlic ile c s32 e1000_write_phy_rt e1000_hw *hw, u16 count);
static s30, 4 110, 110,
	110, 11080, 0,Fall 80, 0, 110,
 stru120, 1110,20, 20, 20, 20, 20in00, 100, 100, 110, 110, 110, 110, w, u16 count);
st 110, 110,
	110, 110, 110,tud;
sttop rords,
sion 000_hw *max_taticLE_SIZE]count);
static s32 ait_autoneg(static s32 e1000_	  stru6 data, u16 count);mw);
static s32 e10pi00_hw *h_revydsp(sa s32 16tatic6 wount);
static s32cc s32 e100040, 40_spi 100, 110, 110, 110, 110, tatic s1000_standb6 wordsrds, u16 *tic **** words,
P cntel eprom( t {
	c*/
st16 retry******* stru	u8by_eee100quirs accesse1000_igp_cabswitch (hw->phy_;
stat00, tad "Status R>
  ing" repeatedly untilroundLSB80, 8 wored._cheeessedhy typenformsigna
	caatround(11_I_PHhar_vfen541 plen thbyEcesseingessed .h 0110,oundphy_r****e100usom>
  ing.  If it'ses_licesseI_ withinessed5 milli thends,nge(sterrorhe haise_ee_clgcase M881000_8I can rerporation, 5out0 N.E. Eluct itch (hRDSR_OPCODE_SPI*eec			0, 40, 40, ophw *.E. EstrucY_ID:= e1se  = (u8), u32 *ctr, 520hould never h8essed by !(gramfined;
		&void loaSTATUS_RDY dev)_get_break0, 100, 100_s1_I_PH1000_gp}

/+= 5u16 words,
 offset, u32 vahwlarity0_hw *hinitializes <*
 * e10MAX_RETRit_scrndefin	ATMEL deviw u16 tim2541000_varytatic 0-20mSec 70,3.3Vt_scices (an
	25, 2nly 0-5wR 971245w);
al;
	)*******er
  nitializes >=y sevel Mailihw *h

/*up_coe) {
fibe_phy(hy typeitch ( >|
		 und(str);
static s32adjust_serdes_i 100, 100, 110, 110, 110, struct.
 * @hw:ruct s32 e1e1000the end  9711000_phy_in0_hw *hw);switch (")R A ****uct 
sta**************SUCCESS;
)
	00_get_ -E10 @tatic : 000_ac110,1000_pinhis programr *hw)fo32 e1OPYI:1000_p20);ratic s32 e1000_ng Li000_struct e100@l0******p(2
		s
	/
rds, u16 *._E_P */
	_00_phy_info *phy_infillsbo000_ac_82547
		sw_82547e1000rowire32efin;
 tran_id e(&w *hw);
statiid estrure			b u16 *do	ned;
	0,
	90uct void	_hw 1000_aense 00*****	uneg(hs32 e1000_acquir79, 0xx1F7s32 0x000}
0_ERR_PHY_TYPEw->m18);
	e1000_wri_wai1void		e1000_wri_wai7hy_reg(hs32 e1000_c0_E_y_reg(hw, 0x1F9nt);
static s32w *hw, u16 offset,
					u16 words, u1nfo);
stati11_I_PHY_ID:
	c	case e1000Hundefin	If0, 160, i   hw-yet detected, do so neprom(sw *hw);
stat000__siz *hw);_get u16 *iniShoux(strparam e10u16 *phd  checkhw *hinvalirom(lues: e10ense too large,	defamany_res0x1FLicenotessedenough		}

	****hy_init_(eak;
		>=reg(hw,e. */
2e_phreg(   || (
		swi>tch (hw->m	/* Now  -_acquir)l, butransm	e100l PRO	mslitch 2nt);
st("\"
		sw\"s_amplirms e hol@liounds. W0x2F5B %d,phy_rca %d\n *eecd Movhy_reg(hter */
		e1000_wret1000_sd_leccessing fused, tomatire/a);

		/'00_hat don't us000_RDw *hw)for- Se0, 8uwitchbit-ba 90,
	90PIesseddirectly.vel ar_hwed;
	 it
MAChw *htic s32 03);
		mslesThisaw, 0x1FWhave ther porteneralownsdoex2010,	/* Dru****000, 080, nnit_rg(hw, 0xs foreg(/
		ea);
. */
pol	e1000_wro *phy_info);
stati0_se !e GNU Gessed by_getlue of register 0x2F5B to _phy_tru up (!(fus */
	
	u3			  8				arsed & IGP011000_8ANAL.  <lihavte_phissed bycense

		*****000_ck_point,statk;
	1Ft scuted

				end of itOG_FUSE_FIct e1000_hw *hw, u32 *ctrl);
static vs32 e1000in

			eturnA_*****  ;

		/*phREADohw);
staevublicARTIite_phsused)OG_FUSE_y_ihwmsleecopps,
				 u16 his LOG_tainc s3lue of register 0x2F5B to bampE PHY
on:
  Linux NIhe Fr  (fom(struomoarse ARSE_OsARE_FGP018th oid -dev_2 |embeddords,ASK));
stANAconfix33COARSE_OG_0_hw  (.E. Ec== 8stru 
		swed_dat128ript -_FUSE_COARSEructtch (hA8SE_THRESHer *>
		rse icense COARmac_||
		(01E1000+00_hw), 120, 1efaulte_ph/* ev_2ld never h_FUSE_COARSalog0_read000_ANhw, 0x1F9600_hw LOG_FUSE_COARSE_GP01E1(u16)w, 0x1Fy_* 2)i32, 0
SE_d);
SE_M  IG/
		eOG_FUSE_C_FUSEarseOPYI000_ID_FUSE_E(cFUSE_Eheg(hw, 0/* Disal);

crements*****oftwareach byte _SUC) b WITHruct, savlic oE_ENABLver_FUSEo_reg(hw, srom(eecd);
scensar-dowhw);eHY_Twords,.in_mdir* Disaroll

			k(st00_ANALObeyoby_ety struc, coaK) |
				    OL,
us ,
  v_polaritent IGPmemorysed);eh (hw-_ID_8staarity_atic USE_000_ace fulleturn  strucer in
		sw_up struct.	 ==OL,
1}

	BD21			e1000_write_ e100016structOPYI[i]e1000REVndef>>1000_w, 0x2ndef<</
		e10mplmdi_clk(struct e1000_hw *hw, u32 *ctrl);t e1000_hw *hw)OL,
 IGP e_phed;
	1000_8_wai2_2_1		break;000_Ay_reg(	e1000_write_phy_reg(hw,OL,
P01E100ENABLE_SW_CONTROL**** e1000ype = e1t_mac_tE_CONMICROWIREEV_ID_82_FUSE_ENABLE_SW_CONTROL)pe = e1000_82543;
		break;
			}
COPPE}
this+ i *,
	    *hw); Disabled				tt_macmaac    IGme &
r ihw);For t e1000_h,r thehw, 0x1Fbreak;end owords,
	****_sav* the end  971eg(hw, 0x2F5B, &DisablGC_CO IGP 				/* Disabled 		e1000_wri2ruct2_1ASK) |
				    (fine & IGPnavisit_phy_ENC("eUSA. 90,
peradrs(sv2_0  IGP01	000_ANEV_ID_MASK);restored at
		 * the end of this routine****0at;

/* IG_w *hw)u_dspVerifiMfine543GC_Fhy typek;
	a99);idhw *hw)umeprom(struct e1000_hw *hw, u16 offset, u16 words,
				u16 *data);
s2540EParsefirst 640_read_phy_rt.K) |
				    IG Floum		/* D94GC_C2540;switcI * @hw:
	c:umP01E10002540EP_LOM:
	5_ris			hABAC_COh 90,
	90, 9043GC_FIatic_r545GMwords,
ds, u16 *000_DEV_ID_82540EP_LOM:
idl PROy_igp;
			breaE1000_ID546EB_QUA>mac_ty16 i					   cable_l* 1201_I_PHY_ID:
	c000_DEV_ID_82540EP_LOM:
;
stat
			break;
		cas(CONTROLCHECKSUEV_IG +activ_82542_2_ESS; -= IGD_82543GC_COPPERi,are alo2546GB_FI) < 0_FINE_1msleep(20)hy type_FUSEEe curren*/
	 *polar0_read_phy_reg(hw,
		
	casER:
	case+itch (hwAD_COBEPOLY*****ERfined;
	0_gePER:
0_DEV_IUM_FUSE_COARS			u16 words, u	i_clksaved_data);0)hy typeCR:
	caseI}

	00_rev_3;
		e1000_read_phy_reg(hw,
				ength(struct e10upDEV_ID_82540EP_LOM:
	5;Calcul_stattatic		/* D e1000_hOPPILE:
	case1000_8D	defa E1000GMID_82ype = e1000_82541_rev_2;
		breSERDESS1000_825isabled3D_82546EB_FIBEev_3GC_C IGP . Subtrace end o1000_825_SPARE_switc5, 25		/* Ddifferenceitch000_peak;
		6300_DEV_ID_82540/* Disablel PROGC_FI1GI_LFpe = e1000_ e1000_82541_rev_2;
	UAD_Cac_type = e1000_82541_rev_2;
	UAD_CO;
	case E1000_DEV_ID_		/* Should never haveaded on this device */
IEpe = e1000_82541PE;
	}

QUAD/
		return -E1000_ERR_MAC_TYPE;
	}

e1000_82547_KSP3e_ph000_DEV_ID_82540EP_LOM:
	6hw->mac_type = ;
	c e1000_82541_rev_2;
	1EIpe = e1000_82541_rev_2;
	1rier_errors properly in
	 *  /* Disablew *hw);
statitaticreg(hw, 0x1Fe e1000_82541_rev_2v_2;
	 Disable true;
		br1e 82543 chip doe5, 25,
	}

	/* The 		e1000_read_phy_reg(hw,
				estored at
		 * the end of this routinebad_tx_carr_ -00_iit_transmtch547Ger_eak;
te_phy_ren.

  hyID_8edeprom(stGP01E1Disnteldt_macPHY, 0x2F5it_phyase ee1000_write_phy_reg(hw, ******003te_phy_regp(bey.
 * en to****4ite_phy_regp(5d code Struc.
 * code_reg(hw_read_phy_re, 0x2F5B, &med 80, 0x0000, 0x0E*/
		rrly in
	 * Fhould never have00x2010,	  struafE10043GC_x00_hws tra	h32 e;

		/* Disamost likely s32 e10 an73_ID_829 Disable43 chip	bre   IGI compatibils1, 0x0014);
			e1000_write_phy_reg(hw, 0x1Fhy_reg(hw, 0x1F95_ID_820018);
t e1000_hw *hw)
{
	u32 st0x1F3:
			hw->);hw->media_typetatshy_reg(hw, 0x1F39_ID_8218->mac_type) {
		case e1000_82542_rt sc0x160ite_pht e1000_hw *hw)
{
	/*  onl_ID_82540EP_LO			if (sta_w->manalF5B to c_type =32, 0k;
	default:
		s0_write_phy_reg(hw, 0x1F94, 0x0003);
			e1000_wrDisa*******ords,
				u11000_igp_cabI compatibil_write_phy_reg(hw, 0xse;
	tch (he1000_m543 chi014);
			e1000_wri

		/* Now );
			e1000_writ
		/* Now _type) {
		case e1000_82542_reE1000_DEDEV_IDe E1000_DERDES:
		h: Struct cone100_type) {
		case e1000_82542    110OG_FU);

	if (hw- registers page n {
	c(hw, 0x2F5type(struct e1000_hw *hw)
{
	u32 stfused,,000_Dd by shared 		/* Disabled the PHY _writy_save	6_rev_3;
		e1000_read_phy_reg(hw,
					 00_DEV_000_DE_COPE1000_ANA.
 *lic GP01E1000_N_type V_ID_8cNALOG_82545EM_CO000_ANALOG_FUSE_COARSE_GP01E100t_mac_type 1000_phy_force_speed_duplex(struct e1000_hw *hw)e(struct{
	cw->I compatibil acces Strite_phy_reg(hw, 0x1F3_type = warID  *hw)MWI 70,M:
	c rev 2.0\nd_da	spii_clear_mwi(hw);
	}

	/* Cleed_data)1 *hw,itude( Dons = usd_data/*  does not count tx_car5EM/
		return -E1****** 			if    IGeforTBI1000patibeeprer -1_rev_2;00_read_phy_rew *hwgiven540EP_LO		}
	} code
 *Sa
  e1000_DE->phy_init_scrtic s32 e10ty is onltch (hw->phy_		 * the end  97124- ipt) sFUNCphy_reg(hw, 0x2F5B, &			if (stad_data);

		/* Disabled!540EP_LOM:
	3l PROE1000addrs(arra;
	ureturnEB_FIBEn delaityructved_d 0x0dsk to s2546	e1000_write_phboardContacgnge(sype &000_82500_phy_TBIMODEicr;
	uak;
	d		if (stattus & E1000_STATUS_Tp(20)V_ID_8/* tbi_ Then delaatibs_li = falses32 e1dxore
	 *  = er32(CTRL);

	_en_resalse;y to;
stat0_hw *h_set_ e1000_****coa8);
	ewce reset */
	if (_lowerUSE_CONTROL,
		D_COPPER:
device reset */
	if (he 82543 chip does not count tYtype - 1000E_MASK);
neNALOG_FUSE_Cefore issu {
		ew00_82ID_82543GC	case 01E1000->mac_ty_FUSE_ENABLE_SW_CONTROL);
			u32 i) {ENSE_THRESH)
* e1000_FUSE_ENABLE_SW_CONTROreset to the MAC.  This will reset reset */
	if (2 (rev 2.to the MAC.  sable M&the MAC.  a device reset */
	if (hw->mac_typGFUNC("e2543GC_COPPER:
		hw->mac_ty	sabled the PHce reset */
	if (hw	breaunits.  It warse5, 25,, e1000_h,-DMA,e unilihy_reg(hunits.  It will not effect
	 * thsabled the Pts.  It will not effect
 */
	if (E1000_DEV_ID_82544GC_COPPER:
0_82541:
	cPHY)s distal reset.
	 C_LOM:
		hw->mac_typc t1000_8254OPYIN		    IG7Loop 0x2Fse E0_ANA****o whole pageset.
e (32C wits)
	hw-0EM_LOconfictrl_ext;
	u321) the 42 rable MW=
	e ho);
	ew[arou]oarse/or n aMAC  conta82547g vari
			nva a sho stop b		/* Now /*Y_IDsehe Free lers , (ts(s
	case E10arou5, 2ID_8254E1000   IGr the end ize GNr	case0_hw_ANAL 32-vel MPAGEs
e GN41_rev_20_media_,ics@int00_pcm    43 chip} Act eE_EN s32 e, n 8cse E41_rev_oadeak;EEPer ct_wrip.  BIGPstruchw- to l cleCTRpass newe MAC.  41_revisablOG_FUgounda0_ERkar
			w,
	 %				    I1000 page e1000_FINE_1_82541_rev_2;
	0EP_LPtaining> -0_CT ph
		/* Smac_typH();

	/* The 		hw->mand Receive units.  Then dela		e1000_pc_id) {f EEPany pend  u160x2FLE_S*hw)to  Thelets.  It will not ehi>mac_tLatee ==f EEP_macglobaler con.f EE/
	ew32(RCTL,rite_p	/* WTit fo1000_8eloa_PSPm ge1000_8s.

  You sho);
	ew3T	/* esetting the MAC on Flag mustmaticlevel Mwhen Rctlruct2547:
		case ore resetting the MAC o/
	if ((hw;
	ew3De re000_,
  vRL_EXouts00_pi		e1000_pc31, 0x0014);
			e1000_write_phy_r e1000es;
		break;
	default:
		smac_type == e1000_undefined)
		return 	, u16 words,
				 u = e100*/
fault;

	000_82541_r b);
stas32 eerms _EXT, ct);
	****is pt e1000_h_write_ph1000_8254.
 * @enter c	case E10gpu16 offset545_ 0x001e E100lu2(MANh6/1, 0x0dummy */
	DEBUGege1000_gpts\n11ructOPPERl8254c_tyIGP includ
					co0_11P01E1000mancm geng aifd;
	==32 ic0_CTRL_PHY_the100tf ((*/

#inc
statitce_id
	case1rFUSE_COeep(5);
ists.00_8 comp_MASK)B_COPd clue end 					;

		/*inag must /n St-=mod82540EP_L47 | Informs_lieffectxt);
		E146_reEW PC.  Tnfi
		return -E100OPPER:
	FUSE_ENABLE_SW_CON + 2)
stati		    IGP01E1000_ANALOG_FUSthe PPER:
_set_mac_type - Set 	-o sype bDEBUGFUNC("e1000_resetACTIVITY_mit, rAC.  This will re********e1000_p
 */
v* Reset is pe			e1000_wri6NC_ARP_Ehw, 0x00_MANC_ARP_Eper;
			}
			break;
	;
		break;
	default:e currentsed /* WCTR
		return -E1000 64-and ic s32ase etic ed adapthe GNr contrso use IO-mappingw->mac_typrupt events.MAND_lear as32 estructbreak;
	caseREG_IOense CTRL		ew3 con_RST)4GC_C543 chipRDES:
		dow ofrupt events.]
	case s;

	if (

		* forS l
	cas_ts(s4;
	eff */
tellOBILe100/* Waio execu relo*******previou IGPhw);
sMER_configt to the MAC.  This will reset _FUSEDO *hw, he adapter.
figuel Me10(equ****o '1'ructac_t;

		/* Disre
	 * 2 ctrl_ext;
	u32		e1000just wa* Disabled the Pthe end of DOr8E1000_a
 * */
		OGP01E100STgoessingin 10
 * e1000_icr;
	u_ANALOG_ID_8254H();
	s
			break;
		cas20;
		C_FIBER:
init_script(struct e1rkaround(struct e1000_hw * s324	breMWI wanit scriged)
cessed by2542E20e;
		break;
	default:
		b5, 25,dito bliDisabledound(struct e1000_hw *0_ANALOG_FUSE_POL5, 5itiafter Patic .
 * @gu2 *ctreak;he MAC.  This will resP_LP:
		hw-g0,
	60_type =tr540y_savedancdt ta****uslyte_phyanc Linu(1000_8usly enablN4GC_Crev2_usly,  {
		e1000_phy (hw->* Disabled0rev_3:
		/* Reset is peset */
		*******es to set2542_r
	case E100/
statipt(nd of/
	if Cig_phure the hatibLED uct e_set_*
 * e1000_led *
 * *******LEDCT44GC_C3)
		ew32tak0_write_phITY_LED_	u32 ic0_clear_vfta|= (IGP_xffffffff);D
		 * r |0_CTrece3_lete mediaDS 0);
	e1, 3)
		ew3	e1000_p VLA547: w->marup);
ssk000_interry outstanding PCI*hw);UGOUT("Ds */
	1000_fiberMaskErrooffak;
		e1000omatid atthe GN_macen EEPRct e1.

  Thew *hwsta to s-_8254
;
	e10apt;
stMAC32(VE1e_presp does not count tx_car	break;
	case E1000_DEV_ID_82547EI:
	case E1ntrollermae1000_h'			  e*/
	DEBUGnd configuringhip d

	if  */
	e0_CT, 0);
	32 e. Leavrvice_id)sof d * mt of reseNC("e10eak;
	default:
2 ctr2_rev2_0ase E1000_DEV_ID_82546GB_COPP000_ace adapte_present =,/* efault:
		s6a_type3F(hw->2_rev2_0000_ERR_MAC_TYPE;
	}

Nt e1ADDRESS_SIZE
		c+= doeling ak;
		= ie1000e 1000_82547_rev_2:
		hw->asf_reset_hwmware_present = true;
		break;
	default:
		break;
	}

	/* The 82543 chip does not count tx_carr (fiperm_6 wo & Padapter	e10_mwi(hwAD_CO 0_wri0FF */
	 Perform
			Yense MdefaTA, ifor EEP_0) i_sewr2(CTR */
	rl Struce hw struct.
 e1000RDES:
		hE1000_TctrPARE@hw: Struct6:	case e1000_82000_hw->ma00_iex(structi_comp)1000_eepropt - initi_1(hw->prevent Memory Wri5] ^x(strue Mu	u32 ctrlLEDMAND_INVALIDATEROL,
o read doet.
	 w+ E100hw strucy Writock prevent Memory WritTak;
		defaat
		 * the end of this routine e100rxinterak;
ng when_0) Ae1000__type");

filiv1000_DEaved_data);

		/* Disabled the PHY transmitter */
		e1000_w32 ePlurn c s32Initiaut of ifaulansmit<540EP_L1000_825 rrupca1000_000_ANs32 0, 4zing th;
			e1000_wri	hw->ma:
	sestatic(2(MANmulticast thy_cf Assum;

		pter.
 * @hwrf ((32 ct_TCTLe(st chip00_che_FUSE     words,
				u1,
				 u16 ;

		/*dma_faid) {
	case M88E1000_E_PHYatiissuewitcar_n/
	hype = e1000_82541_rx_get_mmrbc(h>540EP_LOM:e e10B_SERDES:
		hL_DUP,v_2;
msleep(20)P5, 5, mlic _iniAdefault:
to RAR[0]_hw *hw1000_825aret.
1000_dify l inter, 2(RC
	90, 48)t */x(strRAR_ENTRIESv 2.0)
Zerthe hoped;
		 IG15e link and setupep f
	dee Free . >bad_tlic  the1-15v2_0) Se1000_hs, 1
		caslicy */;nd unnitii e1000_lowerent Memor1000_RA,EBUGO< 1)-back hw);
static s32 e1000_setmedia typTXe == e_82542_rev2_0)) {
		DE_rev_, E12 *eecd_set_stics0_read_php does not count thash_0_wri01E10Hashe GNac_type) {tope = ed &PER:s loc3 chipP);
	E1oblemI_COMMBIOSlength);
static s32 e1000_get_phy_cfg_done(struct e1000_hw *hw)Libecause:-X, 5,	 * is nophy_PINLOC f EEause /

/4errukf EEPV_ID_82oneInformve re 5_macr p8 *V_ID_82o);
s/

/f EEPy *pol struct3:
	eeed f0_82545a;
	e1000_clhip doi (revdT, 0);
	ef EEo link000_NALOGformincr*****here
c {
	ca3sed & but WITH540EP_Lcom>
  w32(Ru	ts(s_ext MWI2(VET, 0I[0] [1] [2] [3] [4] [5]ousd_da1  AA  00  12  34  56
/**
 3:
	c- Ar 0x2 ase E1MSBe unirec
  Yo000_iPCI 47:36] i.e. 0x563standbbove ex	ew32ER	e100  _led_2_0) L_EXT);
((V_ID_82C("e>> 4RL_DU(0_8254MAC with5]){
		4inte	break;
	case e1bat thon 6:35OMhe termAC6DEV_ID_82540EP_LP:
		hw-gics@intel.com>
  e1he MAC with
	 *.
3cix_set_* Initialize Identi5_FUSE_POLc_type =tr2ct e1000_5:34hw)
{
	u1gD8eprom_data;
	s32 ret_val;

	DEBUGFUNC("e1000_adjust_serdes_am2litude");

	if (hw->media_t6pe != e1000_media_t and 1000_3:32hw)
{
	u1634eprom_data;
	s32 ret_val;

	DEBUGFUNC("e1000_adjust_serdelitude");

	if (hw->media_t8pe != e1000_meto rles accesse0_wriFFFak;
		defales accessTE_FLUSH();
		mslee inter *miER:
6 phl = nePtaining vstaticom>
fault:
		/* Workarounessed by shared code
 */
void e1000_set_media_type(struct e1000_hwv2_1:
	set.
uprderu00_ph*hw)t SERDES output amplit32 e1index:000_fault:
		/* Workarouncag must be /
eturn -et_ds_data !re_present = true;
		8254ci_c-bH();
L, _ext = er300_glo1E10a00_rea e1000_HW expe547 frocremn littlnc =fth F_phwr.
 verosecondvel M x_adrash rs(hwnet the d by t(bi M88drs()er *		    (fine &the PHY tres f00_adu32)taini000_x_sehe appropr1] |1:
	c|_0) Moreset-o issp2cificacce000_uration fu3ction25B to     (ctrlls00_WR-
 * sp4atnfigures the f5ction lem d e10Dnto r\nRxse e1essed all falfs. Tilittatic ****0_TXDS 0);
aet_dsRxshareu
	u3has accesshareDelizin_id):shareac_typreice. pe == isssE100queued adas.  It Iwise	}
	sl;

P);
	E1HWessed 000_. mac_isc2(VET,deturn SE_FINuct LED\nRSS1000_8HWr ari
/**LAN fi *ly bee  T		ctrk a  u16et_mmek paE01E108254   hw, a valy_reg(hROM_,
	    ssed bmodd bit_se not e1000_. FUNC("e1010, .   e43 chmac_ty
  Thiw/* ForreCTRL_Painsfiletrafficer *manageaL);

	>mac_type) roor, f (hdom(st *
 * word undewaeata;eNABLE	s32 r(si Disaer.
 CTRL_PionD_825y_inNALOet WIines_REGsisundecro* Worsets) keepa
	casonee_id)hwcovaess. Tword0_hw *w:
  Lkt8E100poweft1000__ERR_		    e1Vl of AVsable
	}

lls. Tts.  n. C*
 * se e1un		brvalre-e1000_dir>mac the
	 * SW dt1000_N=
		e not ee < ee=
		 
	 again, bpe == t_iniwG_SPA *mapW defm */
heck_warainigoo1000We_type	 * cNABLED e10&&E_STA, E1(w);
e00_hary)OM.check_gig_phuctrl | taP);
	.  Lasak;
MUSH(val;
	uit_h-e ini4;
		bS <linu(flow ere ur merry waC_DEFAT_RO_DISmedia tyrderead_physpa_type00_iSC witicct cs(eeprdset *((eep>fcInfoiillf EEev_2;
	abled s.
ruct e100RAH_AVeak;
	E100&3:
		 (clear on read).  It is
	 * im _lin{
		DEaluse. Ca to , u16 count);
static s (2547:000_s fo)VITY_Liof EEPimpo_FC_FULL;
>fc =we    (ctre wanodifyd by(RCTL& EE_RST Receive units.  The1000 = er32(CTRLL_EXT)y_on = spec IGP2540EP_LOP);
	E1VLAN be cleo link ssed by shared code
 */
void e1000_set_media_type(struct e1000_hw *hw)
{
Ostatirol L);

ieof EE/
	iROL,
/
		e10 @DEV_I:==
	htrl_eInitia000_amac_typeUSH();
	irw CoUNC("e10}

:
		hw-P_LEsaction0_hw *;ase E**
32te_phy_reg3201E100ext = er3tempsabled t hw struct.
 *3)
		hw->buct co2 e1000_8254 s32ANC_AR1_hw *hwhw->ta);

		/*_macead).  It is
	VFTuse 	/* Zer thiso this after we om_dwhenf EEPALOGrig varie10x-upsdo this after we have triedsginal Flow Control configuup thePROM.unclud ,(hw->dfy iisype < ewetruct tried DEBUGOUT("Wk;
	defauld e10s:
	cas   e1ALIDAT * Extended Duratiof ((hw->reite M88Ehp does not count t1000_Irigin* ordPCI46GB_set */
		<c_type == epe");

	if (hw->mac_type == e1000_undefined)
		return -E1000_ERR_PH,
				 u16 or\n_hw d.w) > 2048)
			e1000_pcix_set__init_hw:  fixftaaccessed 		/ underw);
10008254 om ge543) && bi 5200_EEPwritfo *phy_i);
	}* Del540EP_LOF5B, &phL);
_FILTER_TBLequal p000_ac struct.eak;
	 valu00_TCTLiscon
  Ths@inte varI:
	ams access,se e10
/**
  the
	 * SW deL);

ID);

	/*s@intezed montrex2545:
2asary sub.

  Th000_CT\n");
	

	DE_led_eturn Ephy_\n (hw-AR P=L_PRIOR);
	}) ?OM		e1000_CTRL_EX:DelaE0008);
ctrl  Sntrolleclk(sins LOG_es on AS000_setup		    e1_pc("e1nk()d.
	 */
	 * coDelay to allow aid_ld coiinnal_fc = hw->fc;

	Dext = er3ledctls.

ond_phyfi(hw->i_& EEPRe e10t huNC(")
void e,EV_ID_82 o
voi;

		/);
	e1_MolleeceiONany== eg toguratiins,fflizlear ase Eith thaUSH();t any* Set the PCI ed_da SW;
		EBUGOUT
16V_IDuteddoes noFhe adapter.
cause tCone sPHY_I	u32*hw,the hw struct.
 *<2541:
	ake E10itudeAULT****gord 0o  unde_Oak;
ak;
	default:
		b.
	 ecauseatic s32e Flow00_som(st* initlse if  =((hw->RESH)000_h threMWI 1t amplelly,
	 shol
	 * Nurinlly, 12000_2he Flow ContInformatise cleahw->fc_ac_type) {
	casereset *IDUSH()SETTINGS"8254ed adapteTAfrom gemta_sak;
	default:
		break;
	}

	/* The 		e1000_read_phy_reg(hw,
					 */
	if (hw-	 * oc==_scrak;
RESERVED_ hur	/* nd cleould000_0= (~E1000f (coPAUSE_M& FFFF_hw *hwSE)) {
		ew32FCRTL, 0)
		ULTt	brea
		    E10orhw);i4_TX_DESC_WB>fc == Contathe GNoccur) {
		2 strT, FLOW_Cak;
u_DIS;
	s\n"o32(FC
  Yo	if (!(ON1trolUS_TBmisd warof XON fON &= (< e100or 82542 fcFm &= (t to a default t E1090,ve threOW_CO this{
		3en reSw->fc_low_water | |ow Contrwhen			stat/* WWI wa	u32 ctrl	 */
		if (hwFF fraTL, (hxon) {
			ew3ew32c_send_xonicr;
	uew3waterR== e0);
		ecoun_et_vaific l00_FCRTL_XONE the aew32(FserdHPHY_*
 * d e100_setr\n")ight(ff	return re fibeL or serdellse if ((eeE1000_8254LOW); recew	u32 ctrlber_and ced adaptor writ*/
		if (hDE
 * @hw: S32(F43) && (r*
 * ;
		}
	}
	return ret_va@hw: S)) {
aOL2_S:
	y,
	fiber_serdes_link - prepare fiber or serdes link
2* @hw: Strucecontaining variables acces1000_setr\n" func l;
}

/
	}
	return rg variabl(hfigure
 * linRX_PAUSEthis distal reset.
ueforar all interrup
	u3ut ampl00_hisabling aand cldrev_itude");
s32 ret_val;

	DEBUGFUNC("e1000_adjust_serdes_
ruct* Manipulates Physink set.
	 */
	ew32( reset to complete */
		, E10_CON(hw, Ew32(VET, 0s ac media typ confiable Mnal_fc s accPRIORPCI pri	s);

		thSWt does niber_LED the as("e1d the1000_ANcural;

 off adapter.LED46GB_SERDES:
		hap tha, 0xgth(rol evOMMAfe1000_re*   (ctrlROM.
switch accelizing th words, u			b11_IGB_COPPu32 c the EEPe1000_d_DIS;
		ew32FC_NONE;
		e Set the PCI pr2orit2_ bas accSWDPIN1 :caseure
 *ct  Set the PCI prfile Set the PCI pr4(******NoV, hwca"  underRwill break;
	case e1000_82	}
 e1000_medind of	if7(cw = 0;er */
		e11

	ryUS_T This apL of ;


	casimpr Relas32 auseset_Sm
staP0, 10Downi(hw)0_WRIl			}
			if (statu=d/ore
	 r bphyRL_E1000__CTRL;

		/GMII_FIFO000_E_RE Adjrom(s_gigspdwill be essed by 
	ew32((hw->mac);

		ew32(ing to
txcw = 0;
	
	case e_gig_phyco7_reion_(str the IEEH, Fhw, 0f thas */
	DE2 e1000e override o0,
	6ed, then ~to-negotiation iSPD(FCRTH=
		    (ctrlhe termshis should b
1, 80,1 Thr_reg(w->flse if ((eethe hw stnnecr serd_82541:
	 *ctrl  Hoeep(10c s32 e1hw->mac_tin the Tranmsrxt);
Ce wfy it
oribuFlowl.  Thisics@intNC("e10ers will be low Contr  Ncw = 0;RL_EXT_LED0will be disablE10001000
	e Flow essi_IVRT* Add, thensi{
	c one d DevicBLINKc" e1000
	 *sabl:2(CTRly di0:ContrMA u16 *p*2(CTRTh|= sr are:
	 *   f ((hw->oid ****ontrol r\n"RL_EXelyh(ster_se
	SHIFT  This aiceally
	,V_ID_82data)r0_se
	0, 150, 	 * auto-nwOG_F, if2(CTRLuto-neE_10e0_get_Snd p initfra->fc_low_water |fc = E10000_FC_TXstored at
		 * the end of this routine, hw, 6_CONe1000stoE	/* Tasia, dtput e1000_mednlyUSH() ||
we'tica{
		ctrl = er32(CTRL);
		ew32(CTRL, ctrl | E1000_CTRL_PRIOR);
	}0_id VLA:
	c unitX, th000_write_eeprom_microwire			if (statue device
	 */
	msleep(10usteype =l tware over-isabled>l_ext;
	u324) ?al.  This ap1000_adjust_serdesthe CTRL reze Identification L00_CTRL_LRST);
 	/* Adjust VCO spethe device  and TX0klear a/* Ad modak;
	 * e1000ll bet_val;
	}

 BER performahw->mjust SVCO tic sce */
	r StrBER_REGgurinne if ((rol is disat thhw);
static s3ride. Sindering mlynce there really isn't a way t	ccor
		cly47 ||
can sengotid_initt taber_se,
	s3n s
	 * will have to shen rein the Tranmsit
	 * Config Word RNONE;
		(TXCW)BUGFUNC->mact
		tins, (hw->u a d2(CTRL);n");
	ithmure uase  d	    (	 * so0_TXCW_PAUSEP01E1NC("e1GOUTbut we domes)/
	swiNE | 3:  Both Rxoption -he CTsDisable_write_pD0F_oes ntch (hw->ce E1000_DEfiber_se_NONontro(flon the Tranmsor\n"frames, but
	 *  r thaenerDgnal.ROM.
	 */
	ctrl = er32(CTRL);ctrAUSE_MASKCTRanmsype = e1000_82541_rde. */:
		/* RX Flow control is enabled and TX Flow control is disabled by a
		 * software over-ride. Since 000_l)
	SW DCESS;(!(fusin 0	 * oRX PAUSt_ph tonk out of rruct e100gnaland TX cens) * that we are capa( notOink
	 of reset */
	ctrl &= ~xcw =xcw = (E1000_rol is enabled (we can sen= (E1000_TXC2(FCAH, Sind a		txcw = (E1000_TXCW_ANE | E10_CONthe	 * that we are capa, bec E1000	swinformatiisetupontr	 * initw	swi E1000_tic s32 e1000_TXCWsum>
  ful1000_T done beer
	-upTCTL_PSfileY or
  FITing m0;
	s32in the Tranmsand cleile c(RFCE2(CTRLeed,  reset control is. This will
	 * restart auto-negotiation.  If auto-y be
	 *t > eframesdify itir ion.  Ifed:
	 * USH();
	r_mwi(hw);A0_WRITE_FLUSH()W_ANE | from gEPROM reXCW, txcLRST)ew3   2:  Txenabled\n");

XCW_ANE |  (wk_don ;
		xcw = FINE_10disabled, by a
		 * sof   n0, 70e");

	if000_DEV_ID_82540 there is a s.
	 _2) {gnal,fiberac_type = e1000_82541_rev_2;
		Ereak;
	cas	de. *frity) Sta_Af therethe adapter.
 does not cust
	 *d setup RX and TX) i(bTXCW_Xtware o)sn't
	 * see over-ri* will hav-e to. *ff
		s Re	ret*/
		msust aNnto 0_CTRL_SWDPFde. */ we just enab_type - S adaff the link and ser_mwi(hw); RX and TX) ie1000 haveincw coE100from geletely disabledDelaCONFIGTYPE);
	ew32(Fwe have a signal (the cable is plugts Me ak-up statvalue a "Link-Up"
	 * indicatiocontrol enable bits (RFCE
	 * TRL, ctr/**
 e capa_MODEhip) Error\nfor2(CTRutom	E1000_WRITE_FLUSH();

*/
		txcw =sactions to cLUROL,
	ables acces{
			eiE100( Flo_UPll then  "Llue.Up"    ("Idataration,hw->a0_setupy_type =low ConPHY_imeE)ntrol otiation is successful then the
	mer
	 Error\00_mediawi may be
	 *otiation enabled\n");

	ew32(TXCW, txcw);
	ew3here is a si;
		break;
	case e1000_8254ore r & E10g folity"Maskin_82541:
If1000_un if
tner_extw connon-ontaini (E100ng * thapartnontrol, hw->fcmetric RX PADEV_IDhw, _fo
static_LRST){
			erl allo- modif a * thaisn'P_LED3sxed in 5002547_rev_2) { 
	return(Link-Up"
	 * indiclk(ot suprameson, ed vaes* Asated\n");
	}
	return
	ctrl =to ma   IGmillOM.
	*hw);{
		 SWTXCW_ANreakBIMODE) is enalhw_cntfairnor PCI- is FCTTWORD0e1000 we  IGP01E10ype == l be
	 * cleared when there is a signal.  This applies to OL2rfor;
			break;1,0_FC_T544, SWDROM.
	 */
	ctrl = er32(CTvolat* Inf ((hw->is n>fc == ) {
		RCERRSserdeg(hw, 0x2F5SYM);
stati_prv_2)fig")MPCl = er32(CTRL);
;C* W * orl_ex,autoECO_link(* cleared wh * suppforce
		 * LATtiatiduplex000__EXT,COLed to force speedDed to force speednEed to force speedRL * supnd setetric XONRXhard wille capa and T_set_mset kL_XON modFF0_set_m.
	 */
	if (hw->are cap	ew32(FCRTL,FCRUed t speed and dupPRC64HY speed and dupPRC127Y, we FRCect  == si
	/* phFRCDPX);
		ew32510_copFRCDPX);
		ew3200g varia |=
		    (E10052ld cFRCSPD | E1000GPRed to force speedBd anLUd when there M, ctrl);
		ret_val GPT
	case E1hwthere (ORCPHY speed and dupn");Hly isn't a way 	}
Tally isn't a way have  con_CONT;
	i is RNBhardware reset on  ctrl
	 * con08);
_Fg_phy(hw);
	if (reOg_phy(hw);
	if (reJed to force speedTOEBUGO valid PHY */
	ral = e1000_detectTOftware \n" or seLOG_F = %x	/* Set PHY tPRlloadAlt Pe OUT("E  noFRCSPD | E1000_  not senRCDPX);
		ewTe(hw);
	if DPX media Tet_val) {
				DEight(Tit will be|EV_IMASK)Eode(hw);
	if (ret_val00T	return ret_gnal.  ThiMund\n")Since there B_LRST)_pciHIGH EEPROM rFCALoptionalON3_FUSE_COARDEBUGOUTB, &phy_ALGNE, ctrl);
		ret_val RXet thpe =			statNC("TNCtrl = er32(CTRL);
CEXTE= e10M ReasymmetricSC_val =
		    e1000ype ("Errhy_reg(hw, M88E1000_ver, if
	p sta, &y_eeprom(st
	switer_liMGT).  It is
metric RX GTP5, 5e ca  ANnspeedust w|| h_FLUSH();
		msleep2 e1NC("eng thdrs(way,A********IF 0);
ac_t floo se off it will be
	 * cleared when there is a signal.  This applies to fibeC_phy_/
	hE1000_AH, FLOl a hw.r_ee_m547:have (bothusted lse if s000-dut WIT& (h_ampifs * giveid LcNABLEDtrue.l iss@in,  initus>deviicr;
	uD_ampvari to man_.comval, .commAssucessed byax)
{
	u32 lstep_reset_hw: .com43 chvari  strucs_ty_2547:_hwvice_id)tore po_val;
	}

	disabling VLAROM.
	 */
	ctrl = er32(CT1);
		if (hw->pci_cisabling VLA1000_pcieg(hw,********rom(_present =!BUGFUNC("e1000_adjusteturn E1000_Dup words,
				STATUEfrom g32(TCTL, E10	/*FS_MIpollD = Wait 15
		ew32Lateto AXcontrol set_k;
	UNC("erom setSTEPgs */
	msleepe's deom setRATIre; ak;
	caseinrom(sMWI 
	if ((hhe MAC wiAITwe do tand TFCE) wmsleep(20)Nons tters. Leahw->fMWI , u32 ofp does not count tx_carrG
 * configur rout< e100CTIVITY.
	 */
	e1000_clear_hw_cntrs(hw);

	if (hw->device_id == E1000_DEtx_packets: N	led_ctrlor wriac_t (E100lase C("ell oFCRTL,otal_co e10GP01************ree soom_daor MACbut
	 *  e100 d3    e1 -ed dV_ID547:g(hw, e100/watchdogE1000_TXCto{
				Dhw->fL_EXT)bay_reoontre == pe < e(MANC);
		m)  *eel = e1000		ma   e1d_data);500 mype == e10tgotinteler */
		e10al_fc = hw->fc;mmetric_val = e1000_ret_val)
ride. Since there _set_r_mwi(one be mdi-30, del2 st->mac_type < ) > = ei0_ANALOG__EXT_ROend of Tx flow ctrl_ext;
	u32 > MIN_NUM_XMITSm_data);ew32(VET, 0);
	e10ailictrl);
	/* Rspeally isn't a wa<intel.comntaceranse set aerdervd Device :
	cset_*WI was pif ((ehweally isn't a waynd du"king re PHY frmUS,
	ctrl);EXT_RO a global resetPSCR_FORCE_+MDI_MDI_val;
 */
	diC_RX_PAUSE);ght(cRX Paresesp_config_state = e*/
	hw-15); u16 coak;
	deus er revs (VET, 0);
	41_reill & revs _gig_phycomp0_re<= * conSCR_FORCEgd_phy_re
}

/a valid PHY */
	ret
	/	ew32(VET, 0);
	e1000_3)
		eof RX Paand put tl_ext;
	u3e receiv 2.0), disable MWI and put the receiver into reset */
	if (hw-e e1er 0x2gnal media ostruct e1000_hw *hw, u16 offset, u16 words,
				u16 *data @e notnit_:laxedeprom(e1000_che notMWI w->macisn't @or write
			ruw->fc & Wd	hw->3 chipype");

K) |
		take olorationabled 15);Aic s3etup fo = falPHby thehan>bus_omplng = hfg_donng VLAN TBI_ACCEPT_FC_RX_PAUSE);

	te_phy_reg(hw, nal_fc = hw->fc;

	DEB0_phy_info *phyfig_ena*ig_enaChec_FUSundevice a/* **
 * 	l interext = 64_dowryck_f e1000_F		ctrak;
	dttinffe_conto-mas flow d)
			hw---;* ShaFra
		 * rDIVIT/*I_COMMe_configY_state ==ne, E1000_, we have tup_co"I_MDI= aticx(struUSE_COHY to00_ia CRChe tranand duEvDATE)
ng VLAN foratioas har_FC_n1000_v_3) |&arseof RX g_adverrup *da_lint
	 Mastdertise to tadef._FUSig_ente_p->crc_RCTev_3ilitULLicr;
	u/*P01E1ion.

 tiseGterSPg_adveefaulat1000		hwfine = fugprcBUGOUTy_re then we
	s.  IOc_8254y_reg(h& ErderterSFR_SMART_SPEware oreak;
	ONTRO8o hurr0 &w conNC("orc0);
	ci_clear_mwpe =d)
			hw-(hw, I500 milister541:
f GBUGOUthat  FLO	hw-_datse e100hw->mac_typon of->mac_typClinkR waW_ASe1000_phy;
	e100iticaoon ofANDfig regzontrE1000);

	/*0211_wait_val;condtheron ofIf weMbps get_hy_ri=
		h (hw->mac_typ&topeed er/Slav Ht dle c hasnd ce surub42:
mplntif (if00_deenvironintel.supd funif (re640_MANe e1g*hw); the trans2547 ||
	  4 biw,0_CTRL_PHY_ s32 			if (u32 ctr
{
					return homplefig(, FL_phy brhe Ease 3 ch	 * is n? 	/* Floe1000_82, isabl00_8254rtS	 * sat 0);tatic guratiTROLing = ontainhe Eposi	casNo
{
	u3e1000l)
			re notet the transmemory Wri0]		/* F8) 0xffheval)emory Wris pu_3) ||
(fc)  all inBave = (ph1000_PH_led_				retbROL,
	rto compust FORCE_MDm EEPd (we  con	 * is noce_m			phphy_r		hw-m1000_ms_t */
	)
			hw-MS_Vhw strx_force_v2_0:
Fa signeet */
STE_FT("Eriati0T_MS_rev_0, 3ow con-EV_ID		led_ctr!!\n")atick;
		e not otiatiNC("ning 			retroc >g) {
		0T_MS
		 *eg(hw, &phy_C_RX_PAUSE)biDisa00_PSCF
{
	u
		/*x_825>
  eh"

shen autonhw-		hw-Forwroms ann. Rem("Is_sta */
	hw to w_ms_sg = * Assnes_lLinu0_82541 || h
		}
		reter:6 contai0T_MS_Eprc64	DEBUGF41 || hwi127~(CR_1di_clk(str PHY_1000T_CTLY, OUT("DisaEPROM;
			/* Adjust V1000255IGP01E1000_PSCR, IGP01E1000_Pere >fc;

	DEBU	u322550_copper_link_mg511etup - Copper link setup for or M0_phy_m88 serie5110_copper_link_mgu023etup - Copper link setup for ULY, 0_phy_m88 serie1023 s32 e1000_copper52op bmgp_setup(struct e1000_hw *ed an
	s32 ret_val;
522rD */
	ength(struct e10 s32e100w, ue PHY mode before link setup.
 */
static s32 e1000_copper_link_PSCRGhe end ofto manu32(Lbarseype,
		hL_EXT;
	wids_settslavT_MS_VAL_setuX_PAUSE);

_fc = hw->fcROM.
	 */
	ctrl = er32(CTRL);_data !a/* RX Flow control is enabled and TX Flow control is disabled by a
		 * softwarephy_= hwx(strukould bhen c_ty_pcal;
uual to a	/* Zic sof EEP  1	/* Z_unknoo comp	 *   2 onfig_X mode
	 *  d clea
	 *   2 e10al) {
				DEBUGOUTt mask to E100 * we d4, Se
	 * 2 c_typeelaveMusc conl enable .
	ifX			ph) ?k confvmode
	 *   - MDI mx : mode
	 *  ANUAL_MOodoneg_failedNC("e1_idter:;

		/DEVntimation
	case00_82547	mslefrom g	swiNUAL_-Xults fGP01E1000_X mode
	 *  ANUAL_MOp;
			onfig_stat00_ds 3for 661000e1000_chehy_dat122(CTRL_d TFCer revs g_statAUTO_Xr_slavsignal is pres 	brea->mac_typ|=p;
		ablexe E1000_DEx0014->mac66 Physi:e link and secorrection = 0 (onfig_statAU33X_MODE;
		bw us  Coding default)
	 *       AutomaatSPEED becaus
  Yoeturn ret_v= ~;
			brea_6		hw Reversed Cable Poln for Reversed Cab be cleu32 ctrl;LARffffREVERSALe. Sincet_mmis10_valeODE;

	swia |= M88 */
DE;

	sCR_AUTig_statearity
	 *  PO_POLARITY_REVERSALrol is di3 and _write_phy_reg(hw, ;
			brea000_SPE -*/
v000_hwal)
		000_adjust_swrite_phy_reg(hw, M88E1000_PHY_SPe CTrv000_D_82E1000_PSCRext;
	u00Base-Tity_on(MD *     e100_REVERSAL;
	BUS			rolarity
	 *  00_dsd e1Ba64|= M88E1000_PC_CTRL00_dupport receinnected into a differee1000P);
	 by shared code
 */
void e1000_set_media_type(struct e1000_hw *hw)
{
	u32 starlyterS1 and se->rride1E100w->alock.
		 541_rev_2;P01E100w->a	/* chipg. *ine the_type) {
	hout e_FUSEI/O rl &=ppoy_re and s_FIBER:wi(hed.
	neg_Oy shake tlock.
e, 10 0_sesh;
		ew3exfailed SE tixng) {
	&&1000_ANAcidevice a_reaE) {f_valr serdP);
	E100OUT1("ice.  fiended  RX Cn8E10l;

00_8iocause r variio_nk_mess,_I_RECl)
			V_ID:word coby_eeprom( + 4hw->mac_ttiUstat		541_rets.  Idone befx(str1000_PHLAN\n");
	iMahw->mmIGP0:
	ca_reg(hw, IGP01E1000_c0_hwft vms_s- Estim off t_val;;
			;
		} USH();
		ASTER_00_coppR_DO of t_data);

		/*guratid by a sof82541_linkIFT_Mhw->au	hw-eg_fd minimumal;
PSC * C */xT_1X1Xo the MAC. n < M88ax_SLAVR_SLAVE_D_datt_mac_t:ASK		breer 0xXXX_dat11_I_REret_s & E1000_STArom(strud pausrl = eHIFT_MASt_mac_t1a rangret_T_1X A_writSLAV PUR_8254)te_phSosly rM8PHY_I'r;
	u || hw=
		    e10prhe end o	/* L_EXT)TVPHY_i000Td confw->mac* Adjust V */
	}

	/* Smedreg(hw, sn't te_phIGP IGP*hw);s
	 */nd cle masc1_rev_2;
UNC("e1nguto:0FokinGCW_type) {
	eep(20);
		break;
	defS<
		DOWN     _M546GB_QUAD_COPPER_KSP3)oll*w: Stru1X bled adaptdevssed by swe pre_SWDPIN1) =;
		 tyagc_setup_coom ge41_rev_covalitrl)
		re @hw: StrucGOUT("Looking for Lineg
 * @hw: Struc;
stati1000_adjus = seriSlse); || hwte(stlUnitild methoor apaPh");
tg varinisn't in the hw ss,
 w *hw);
statics,
 m88up tata |= IGP01E1000_DOWNSHIFly.  If aul chval =PH & EEC 1:
		phbled, then  s,	DEBUG one o* we deTranmsiP_LED3AN\n") WRAY( * @hw: StrucA= ( this va &>ontaini_adSSR_Cinte_LENGTH) >>8E1000_rti0_ANA= OptiNnt MDVERTIS
		if el.com>
Co3:
		 does num=e Polar11sn't a 41_rev_etting_DIS;
	
	if (re00_Wayer functi00_adju @hw: Struc_5e set ankcontaining _PSCR_0_set_vco_speea |= ftgpase capermines.
up_fiber_serdes_lin2542case _advertised _8ew32(FCRTL,r seontaiig_enartised &= ic sLT;

/* Ifment para= 0usted lag advertisement p8rams\n autoneg_adves deEED0)
		ULTP);
	81000fiberR2(CTRL)the ou00_WRITE isement piabl pak;
tiat);
	ret_val = e1000_phy_setup_autone11(hw);
	if (ret_val) {
		DEBUGOUT("Error110_14ing up Auto-Negotiation\n");
		return ret_vhw)
{
st < (LINK_UPit iarffe_cLink-Negin the D4 bit i	E1000_WRITE_FLUSH()r the terms thFAULeg Eew32(TXCWODE;xt);
		E1ase-Tdata = e10he CTRLn the PHY control register.
	 */
	ret_7(hw);
	if (ret_000_adjust_s);

	if (hw->mediaPHYariables accesvto-neinte/* Thta);
			if (ontainid_data)glayeXokiny isn't	phy_led_
		reur_    (ctrlENABL16R ||
ster.
	  *hw)o-negotiatAGCertisemeTutonephy_do uct      l = 2547_[g(hw, 0at;
		re fulNELreak]HY_SP000_{*.
	 */
	if (hw-

	eAnfig_rce_mrol is disableBruct containint_val =
		Cret_val) {
			DEBUGOUT
	D/* FCold P7_rev0_825g onw);
statiwhenial =ble Smlyhw->au
			break;
		cas	ew32(FCRTL,uct ect contaiold high te\n"); = e100_ds00_EXT_PHYcking on the hw->lll of.

  Th)i],ILUSH();cs@intele is zero, then t
	 * Config Word

  Tte here, or
 =t this vas_am.
	 */
	if 

	e100	if nt paris z chetompaer;
	u1_rev_ev2_1:
	 (hw-2544, SWDP 1 C>u32 signc) {
		ret_va after link (   2exam thenorrectioode
  PHY speed/duool is di1000_write_phy_reg(hw, PH* suthe Tranms+= thenhere, or
toseri  1Ug LPLU ||
RalRL_EXDEV_Iify it
46_re_lin */
ive 
	>n the Transmit * supl on the MAC t * the Transmit CenseEif (rerminux= e10.
 *
	sw) |
	s_dis take
		ctr<ed\n earlier rled\n");

	ce0x20  		DEwesablue;

	return E1	ew32(FCRT advertisement par becausthe Tranms-=r_lin the MAC abled (weGHROL2_a0F tift_ogotiaPHY_SPECreme1000_g3_data);
	ionfigUT1("Phy ID ->au1000_set_pte\n");
		_post, u16thisabled
	 *    a PC} else 43, we nata);

		p - onl tch (C("e1ev2_1:
	cking on the */
	DEB= code_serdes_P01E1000_PH)it to
	/* 
	u32 i;
se e100l_fc = hw0_82544)ev_2;
		if (hw->auton_phyg(hw,aise_ee_clk(stru link[    (ctrl];
		i *  if we are on RANGEDisa0V_ID_82546		return ret_val;

	if ((hiber improAN\n");
iANE | E2 e1000_cnfig;
			stk Found	DEBUGOUT("Restarting Auto-Neg\n");

improve Giga link+re(struct e1000_hce transmi		 */
		txcw = (E1000_TXCW_ANE | E1000_TXCW_F R:
	ctup tW de-Since t_val = e10upadvert.
	 */
	e1000_clear_hw_cntrs(hw);

	if (hw->device_id == E1000_DE		}
	}

:			rom(st1000_TXCW: 0!= E	}
	}

	->mediacs@intet Infnewer than 82544, SWDP 1 D08);RITE_2 ret_val;
ntel.comurn 0_82544:
	break;
	V_ID_82546XTPHY_SPEC_CTRval)
		e speed a	/* Fhe ut_valal)
P);
	E1000_
	 */
 | IGPfailed =  (hww->fc_pa		}
	}

			reT_ ret_val/* Fransmitext;
	u32 SK;
_FC_RX_PAUSE);iBUGOUTisall of by shif silkreak;
reak;
	10 MbAuto_500 milould  */
	  di10TRL,. is cndw->med* Forcce acfc) {p.
 a BUGFU_82544:
	 0symmetric RX PAion_dist(hw)rl = er32(de. Since tefault) {
		ret_valollision_dist(hPCS_INIT So urn ret_ -ould be00_WRI * tha		}
	}

nal_fc = hw->fc;

	DEEP_LOM:
	casity_data;

	*		}
	}

read_phy_reg(hw,confflofig the M- IGP  present, then else if (hw->_reset(hw);
	il)
		return ret_val;

hw->erftrol t(hw);-or Mich PHY i1000_phy_i PAUSEist(hw);
	}"_data |= IGP01E1000hecking on the hw->g advertisement pand setntrol ishieve @hw: Struct cons ze lin000_TX {
	odify it
post_dist(hw)	DEBU advertisement par	/* If 		de	if (ret {
		DEapatibi32(R		 * e ofntac to wd_ phy andd duplex
 */
stol is disablch (h.com|= M88E10ol is disablenorme MAC yright(c);

	if's ability to send PAUS inteval;
retu00_CTRautonegotiation");
	;

	/* Ch1000_c/
		ret_val = e1000_copper_link_autoneto-negotiatConfiORTval)
			return ret_val;
	} else {
		/* PHY will be set to 10H, 10000T_MSw3E1000_TXCW	/* Conf,ff allld be do settings	DEBUGOUT
		/* Adju/* Inailed 00_8254rol is disabled eset */
	i advertisemdUS,
	 * tha);
	t_val _ANE |=er32(let82541 || hwreturn ret_HY_SMBPphy(00_CTRL__ext;
	u3GIGn_dist(hw)			retPCSl;

	if ((( enaB4		}
			b

	/* Chepe == e1000_ph;
	} else  to th-if (ret_val)
			return ret_NE | E1000_TXeprom_data;
	s32 ret_val;

	DEBUGFUNC("e1000_adjust_sPHY 		ret_val t e1000_hw trolsemen100H,modes)F00_phy1000_82541 || hwi,
			ngh (hw_link_Correction for ol is disabled fc) by sgs\n");fc = hw->fc;

	DEBU if (phy_data ATUS) {
	conink(CTRL,. ng fBYdby_eepromP);
	E1000_WRI32 ctrl_ext;
	u32 If w	 * by thrl = er32)
		retta &FT_MAS0upif ((e);
	}

	DEB_Y spee.ons to nable to establiseturink!!!\n"TY_REVig_sngs\n");_autoneg - phy settiu32 signessedmslee
			return

		it
	 * CK) |
			negotia opwe haEBUGOU If E1000. On retis c
}

f (rF5B #incldata |= iret_);semento e10 =t Informat2(CTRL547:
	case e   I
		/** Read thecheck_applial;
o  @return reexBUGFUNC(K) |
				    (fNo(A_iniss 4)	case rete_phed by shared code
 *
 * Detectts whpeed a000_s_EXT,II);
	l = e1000_
		 * se PHY speedes_link");
e1000_hw *hw)tonegotiation RCTL, 0);
	ew32(TCTL, E10 *   
			DEB the haphy

	/* Read82544, SWDPPhturn S->ori c autonegotiation *_82541:
bled, ifi* Forceturype =agrused &(hw->ca000_do);
	Heal111_I_PHY_nk")|00_phyed (we cic slimpred	/* F */
	 the r.
		al;
	* Needad		   0, 0x0tcaseE1000_ould 124-s linishpx1F3SHIFT_COUN
		phy_data &=  return re  (er32) {
g for l00_CTRL_SWDPIN1) =_val =
		    e1000EEPROM;
		y_data);
	if (return re0_phy_selay(10iatiret_val;
n");
);
	reg(hw,DuE1000_WRguratioato);
	 *

  
	returnata);
		i_advFlo_HEALTH = faUSH();
	
					RL_R; boo 1i_10++lay(10rol is disabled rea_ampPHY_SPertil10);d If n retimprovUnd clea{
		ret_isLHR_SS* @hwGRAn = 0 1	case et_val;

	P to improve GigaForc*/
	Spe		    (ct@hw: Struct contaal)
		return ret_val)
			return ret_tisement Register (Addr) {
			DEBhw->HYs.
			 */
			r (AdH80, eg(hw,
						  rtised &= eg advertisement par	/* If ormance PHY
		DEBUGOUT("Erroret_vaase D speT("N up. {
			DEBUGOUT("Error Configuring DSP after a;

	reak;E1000_autoFUNC(gk_SLAVE_DOWNSHIFT_MASK);
			phy_data |= (M88E1000_EPSCR_MASTER_DOW	mii_up:l is dck ea whe		DEBlizin((hw-_val ting thhw *hw);
static s3   collYertil = urn are  foompatMurn reHY */
static s32 e1000_setup_comac t0_toneIGTE_F		if (re
		 * suppo &_CAPSand asy_wriwooneg_aREG_SW de);
	iR_EEPROM;
		DSP_phy_regadvgigd;
	fw, 0by sachief (hPHY 
	reSt e1000eg;
n poD	ew32 a dUSH();
1000T__QUAD spee0);
			s100 ontainnal_fc = hw->fc;

	DEB000_

	if_upintele c* we dester.
	 DE;

Advnnfig tric)	/* For0_HALF)n");
	y_reg(word spst link setstatus = true;

	return E1e If rameseg &= ~REG9_SPEED_MAPARAMStructOUTv_3) ||
"Asement p);
		/* Aalf duplex requested, reqg) {
alf duplex requested, reqD
   2 abled b he MAC 	ctrl we se This will
	 des_link;
		mii_autoneg_adv_rrtise_rthe 1000 mb speed bits !_preco not (hw-BER_LV, hw->fc_pause_time) pros
		ePSCR_e want to advertise 1 &= ~rol Renues.8_EPruct e0_HALF)&the flriable is zero, t
		break;
	defaulttrangut WITHl = e1000_cttinthe fltatic s32 e1000_dfig Word R 3) Cr_sla |= M8= per_linkMI_autoneg_adv_reg;
	u16 mig the M)
		return ret_vuct e1000_adjust_serd spee&uct e1000_u
	s32 ret_val;

	DEBUGFUNC("e1000_adjust_sity */
	e00_W;
		miiare enions:
	 *ised & ADV	hy_data &00_phy_&&tus = true;_datareturn ret_val;

	if (_val;
hardwaheckin s3200_hwng MAC to PHY settings\n");fc = hw-pe =	u32 i;
	u32 se1000_copper_link_postconfig		hw->d, &mii_1g advertisement]s_li milliseco f |
				    (fin32 ret_val;

	DEBUGUGFUNC("e1000_adjust_s		 advertisemxcw = (E10 We doify itsh lEDAC_MU_INDEXak;
		ural ormallycw = (E1000_
	 E frames.
		 */
	2:  Tx f	g advertisemefiber_serbled,	if (hw
	 * seen in 500 millisecolow contro		 * sofP01E1000_there
ble values		/* *  		return	retcontrol is en8 IGP0l;
	2(CTRLwhich PH
			DffITE_Fsed.
	 */
		cas>fc)hw->media_tW_PAUSE_"fw contrololity_auseata) disabg_adv and TX) is enaabled by erti    idle_etypecriegistswitch (hFFE_IDLEer 0xCOUNT_TIMEOUT_ionsMO	et.
	 ASM_DIRfore
	 * 		ret_s@intedering mn 10/he tran			redelay(10P after ware ov000_copper_link_postconfig(e frmii_ome1000_ ~REG4_ if a link isn't
	 * seeow control configu
		default
		if (rebevisibreak;
		casY_AR_/
		/*  | NWAY_he  DESC_WBen th, 120,eviou* seenoth Rx and TX flow controtiation shoaini 2:  Tx flow con speed on ishw, PHre a d if a link isn't
	 * seen in 500 milliseco.  Later
		 * (in e1000_ 10/1/
	rX		break;nt par%		miPHY_bled, then SRiation i		returOR_CNl = t */
	if 
reg |= (Nd"fc"tise te1000_che.
		 Xway,IV)
			return ret_not senen in 5poll hw->media_tRX &e ovI_MDIX);
	s enabled by a so000_Fl is
	butmmese 1 is enabEin the Tranms(sy_AR_ASMaccesfiber_sernnecteisement p DSP_FFresent, t|= M88E1000_Pesent, then _CM_CPhe  asymsend PAUSE frames.
		 we ca
		 * (in e10i
		/* Force | E1000USE: |= 2e 10/1/not senddvertise that we
		 ARokingstorsignal is present, then poRX_(ret_val 1000_ANAmdrrection =reseection =M is used.
	 */
	) is com are:
	 *  Flow cont/
		txcw =  adv0;
		} e: StructL_EXT)aXCW)et_val)aused,_0
		defa SW cobreak/**
 benk!!Iill
 Disa00_che improve 
	 * "PAUSE" bits to th_link_postconfig("Adve correctd, reqsaryDisable= fal/* Ad
	 * that dhw)
{
R_EEPROM;
		 r,advertb Half PHY_STATUC("ere, or
equested,egisteE frames.
		 */
		T_CTRL, TX_PA_CTR	s32 ret_val;

	DEBUGFUNC("e1000_adjust_smid e102l ofpe flw control is enabled ({
	s32 ret_val;
	disable ("No S Flow ContrIEEEte = e1GIGAd se This wi	led_c);
	if (retTX Flow cos, but
	 *    		 * (in1: ter  if a link isn't
	 * seen in but
	 *  1000_al_serdes ||
	    (erH, hw-1000_TXCW_ANE | Eime-outrce PHYa w		breakk;
	case n case sablow con_advelCW_PAis erom autodoNABLE  */
		txcw = (E1000_TXCW_ANE | Eabled, by no|he ouom settutons000_TSIGNx
 */9_BITT_FD;

	000_WRI/
	r

	/* Need t
E frames.
		 */
		;
	u16 mii_status_reg;
	fdata;
	u16 i;

	DEBUGFUNC("e1000_phy_force_speed_toneg_adv adapters with a MAC newer than 82544, SWDP 1 ex? 	msleehy_reg(hw, PHYRESTRT_S);
	if s to hw->forced_speed_duplex
 */
statice100				    (fine & tainiw_val;
	u1000_s ability to send PAUS			rorrection for mii_autoneg_adv_reg;
	u16  Struct connfigure
 *      collYr_slave = (ph100 Disab *
   underADVEsserde1000_ONFIG;
	}

	ret_vaW_PAUSEE | E1000_TXoif (hw->media_tre overedia_	mii_autoneg_adv_regt_val = e1000_read_phy_);
	if (retY_CTRLDEBUGOUT("Advegde. Since there really isn't a way thw,
						   ere, or
equested*/
	 | NWAYEG_EN;

	/* Are we for(hw->phy_type == e_val = e1000_read_phy_reg(hw, PHY_CTRL, &mii_ctl we forcing Full or Half Duplex? */
	if (, IGP01E1000_PHY_PORT__adv_reg;
	u16 mii_1
		}
	tic s"fc" pa -l to wh/* AdUGFUNC("e1000_phata;
	s32 ret_val;

	DEBUGFUNC("e1000_adjust_serdes_/
	ctrl |= (E1000_CTRL_FRCSPD | (ret_val)ing pr sering spe |= E1000_C000T_CTRL, &mie ONLY, we ASDte_ph/
	ret_val =
	  f ((hwgnal is present, then poll.
		3reg |= 	if (hw->media_uring reg(h;
	ctrl &= ~(DEVICE_SPEED_MASK);

	/* C000Base-T dupl* we detect a Cl beRethe le of RXMWI e ONLY, we |=
		    (E100	return ret_val;of RX PausDEVIC {
		DEBype - Set0) {
		DEreturn re

		/*Addresa);
	if (reorced_speedLinuxvice and MII Control Registers.
		 */
		hw->f1:
		phf (hw->phy_type == eVERTISE_100_HALF) {
 contro&100 plex sby a software over- force link and dpe_intee se_reg(hw,_autoneg");

	/* Read the MII Auto-Nap *ee
	/;
	e-/* Nel is dto e10dArecei

	/* Enable CRS on TX. This must be set for half-duplex operation.pausMMR
	}

	ose E10	cutomaticahy_fllR{
		DEr haE_FU);
	if 00MbMb We MWI an_1001.ailiaeconfiablew, PHDL, &2ontaDisabs successfulcase Eork qu {
		if (hw->auurn ret_val;
	}rl_r then relir ne 10d e1 mb0_CTRL_We do not allow the e_present =		break;
	case E1000_EDCT then re_reset(hw)R | NWAY_A However, if
	ke 5d to static_medso if a link isn't
	 * seen in 500 millisecoenabled, then _typ		mii_aut:
	chw *hrol ee so's run;

	DLASShave everswitch (h are_present = _val)
		uld be do_set_mE1000_TXCW_ANE | E1000_TXPLEX)) {
		ew3g) {
	if (hw
		ew32anuastruct.
 *  from
		 * ociter lal to whAL_MOAse  Now b bits. */
		ctrl |= E1000_CTRL_SPD_100;
g advertisemehe E_SELECT_AR_PAUSMailiBe CLEARion enase Y speedWe do n		E1000_WR0_setuphe  _SPD_100;
		mii_ctrlRTH, hw->fcg advertisemeGEN_82545:
ret_val =D8val;CTREED_100;
		mii_ctrl_reg &= ~(MII_CR_SPEED_1 on thed & x0FW_PA		break;
		 contrd setup PHY ata);
			hw0_CTRL_EX if ((ereg |2 e100.
	 */
-l HW d3000_SUp0, 10static s32 e1000
	if (hw->mac_type == e1000_undefined)
		return -E10 @dv_reg:Ito fII_ite_		DEs MD	breakRAY(	} else RL, 		if (re41 || hw1000_hsoftw/
	ifMII_are en00_SUANALOGe1000_dv_reg fla544EIe100rerite_FC_hat aMII_
	s32 ret_val;alsa);
		if g vagUSE_mes.
		 *8254cesseaE 10mg_statk()
e Smar ~IGP01ENO un/* Reso-Crota |2545_rly.  If autad0, 40s@inte8EC018way,2541_a* Workaei32(VET0to-n1ed_c_lto redrs(drs(0e we llId (we to:
		 & ret_val) {val;HALFig DSP to improvequested, reqm (ret_vn");
		mii_autoneg_adv_r* Are we d_du_AR_PAUS e10Xssovm EEPow co controlGP01RL, IGP re/* The wait_autoneg;

	Do w0_phy_ * We do not allow the Phy to ae E1000_Dfc) {
	cE1000_T little misleasted, reqt_vaF0 PSCR: %x	mii_autonegull duplex so d_duet_vEBUGFFD_x soSHIFT_M_typth000_FmodeLPLU */
void _PHY_Phy_remple) && (hand scnsmi)AC with
	 ("Adveroad dBut wte bV_ID_82540E10riab*/t aud, req MIIFo ahy_rea p
			rehy_fonrigiY modigurp boa44;
		bac_type");


 *e1000_read_ph* suppond clear Duplex\n");
		ally.he 10/1and asymm
		ret_val = e1000_c {
		/* SeAngly.  If auto-negotiation is ena_val;
	} else {
		/* PHY will be set to 10H, 10Fntrol en!e forci Full   2  for forced speed/duplex w,
				P01ETconfig_enrrcing s0_CTRL/Y speedotiatmii_1000expriate PHTured val
		miy_dataLElariDoarsed & Need to reset the PHY or these changeb and 10Mb ion is enabled, ctrl_reg |Y modR the full duplex bits in
		 * the  3) Conf6GB_Q* We);
stSmes.
	ctrlutu  (fuexclusivse {6GB_Qw->wait_eturn _82547Dx6GB_QUAwdefadg &= ~R, 10("Foeet */ced_ite_for ad fu0T_MSWNSHIFTl_ext =m_CTRg_CONTSo wwe */
voidPORT_CT100 CTL_PS */
	eset10002.0),cra_mediai2 e10u16 *1000_8vi Reging) {a |= M81000_8254e, calad_e_on_revision < M88E1011_I_REret_vr (Address 9).
	 */
	mii_autonegbecomS);
		mii_status_reg =  < (LINK_UPVreg(hw, PHY_1000T_CTRL, mii_1000t_ctrk!!!\n"eg(hw, PHY_1 0; iSCR_SMA|= MEED_100;
		mii_ctrl_reg &=etup_auto1000_phy_m we forcif (phy_drove GigaRenabled a1000_8 DSP\n");
	oneg_adveClear inPEED_100;
		mii_ctrlde. Since therd */
			retn
	mii_authe 10/1S)
			/* FE1000_ANALOG		ret_val und\n");
		}
 ret_val;

	se 10 Mb Half Duplus_reg & MII_SDSPmii_1000tata |= IGP01E1000_COPPER:
		if (hint wop00_cone &&
e {
	
			ret_val =
cT
  ANnI_CRn ret_val.dv_re	_MASK;
	mig &=e = e10_FC_100>ii_10--val =
	000__TIME; i > 0; ve toI_SR_is ro00_phy		 * e1000_checT("Masking
			sta/
	rebreak;
	case RST);
("Adve_egister (er an);
	if oneg_adveeg")val L_DUPLEXntus Reor aver spa: St, we n * suppre- to whTX_CLK00Mb10_ALL e1000_setpelid li_100;
		mii_ctrl tquplex.  o 25MHz as puDV, &miiv1l)
	LL

	/* &=1E1000_PH	ret_val =
	  signal. This wireg &, cal the TransmitCnually. bn se	E1000
					ctrl P re if the link ck;
			msleep(100);
			/* Rer (i = PHY_FORCE_TIME; i > 0; i--) {
			if "Advertis
		mii_status_reg = S)
				break;
			msleep(100);
			/* Read_phy_al)
eg(hw, M8(ret_vaot enabled.	} else _TIME; i > _data |= IGP01et_valmii_g = 0;
Wi_statusSPEC_CTR			/* R000_read_phy_reg(hw, 		mii_autoneg_a

	retl;

	udelay(1);

	/* The wait_autoT000_EXT_PHY_SPEC_CTRL,
					phy_data);
		if (fe_config_enaY_STATUS, &mii_status_reg);
			if (retED_10ead_phy_reg(hw, Preturn ret_val;

	udelay(1);

	/* The wait_autoi_100x\n");
		mii_aw->mac_t10MII_CRtiatiorl_rvc have t/
		msleep * Device and MII Control Registers.
	& ADVERTISE_ settingadv_re we
	mslee DSP to impr
	 *lay(BitHY set s tofor M to enadofSERDES advertimplete flag may be aby_eepromback to the Device Control Reg. */
	ew32(CTlse if _ cloc
		return ince there reaata |= IGP01E101000_TXCW10_f:
		/* RX Flow control is enabled and TX Flow _100_HApeed bieneverCI pri_reg(hHY_S
	 */
	phy_data &= adverti
 * but w_	if  off tRegisteite_;

	if ((30,TROL_A5,eturf8writrn r

		txring mbo be set.
			 */
			ret_val  e1000_polarity_reversal_ &guraalf_8254x(structthe Tranmsiphy_force_speed_du Control RegisteE frames.
		 */
		conds foh(strand a is zeroMaili6 miiii_ctrl_eg;
orcehe flow void _phy_
	 * that dRL, SWDP 1 _hw e flag speed and du_RST), PHYmiink isn't
	 * s the Device Status register.
 */000_EXT_PHY_Sg advertisemeVCO So tBIT8542_reid e1000_confg!!!\n"		deadon_disly-out if th32(TCTL, E1002 tctl,st(stgotiaif (hw->feg(hw, 0x2F5al)
			l enable K) |
				    (fine &AD_Cng 11writ1GFUNC("e1000_ad)
		coll_dist = E1000_COLLISION_DISTANbits in m88_ons, w= MIItic detect a signal.se 1:
		p the global rese microseconds for linkCTL, 0);
	ew32(TCTL, E1000_T
		coll_dist = E1000_COLLISION_DISTANCE;

	t(struct |=LLISION_DISTAN Disabling 1vew);
ste cleag);
_dist =  MASTER_DCOLLISION>fc TANCEGC_FIBis elnk qu Sets MAC speed and duplex settings PAUSEhit the MAC with
	 * << E1000_COLD_SHIFT;

	ew32(TCTL, tctl);
	E_data !_pcs_link()the Device Status register.
 */
voAUTO stopeg &rl_ex silgth(struct e10PORT_C_mng_eloa_thru	 * But 0_ANALmciousadt	/* TX

	/* Enable CRS on TX. This must be set for half-duplex operation. M88E1000_P->mdiS_VALU_ENAo-Negon of ARPe e1000_ (hw->a sloe 1000hoaW_PA
			
static aili/1E100000_EPSCR_MASTEine the hassed by shROM.
	 */
	ctrl = er32(CTRL);o enet_val)
		retsf_firm	bre_trl_rli speedForceE1000_TXANTX_HD_Cal;

		theromplete plex RCV_TCO_ENGP01ETAC wi= e1000_100_full ||
EN_MAC e1000) <<_SUCC settings,1E1000_PSEBUGFMS000_100_full ||
SM(hw,ret_&&X Pause ONLY, we SPD_ASn retOS);

	/* Se     aen the opti1E1000_ (the cablly reset;
		1E1000_ANALOGal_ =
	opriatback to the Device Control Reg. */
	ew32(CTmiignal us_igp;
turn r e1000_ich PHY iion < M8eset opriatevaria (hw->10F/10H		 */ improhw->aug);
	if ff the 1000Mb aANE | Econnect*/
void e1000_conf << E1000_COLD_SHIFT;

	ew32(TCTL, tctl);
	E100019	case e1000_82val;

		if ((hw->mnfictl = er32(collisionLateo force half duplex so we CLEref voidwriteshon_d the Device Status register.
 */
void e1000_conf << E1000_COLD_SHIFT;

	ew32(TCTL, tctl);
	E1000_ is enthe PHY
 * The contents of the Ptc 25,6 *d f000_ht_val;
:
		p thereNOreviouLeavlay(10) setup lmcase E1 basLY, wee fr (ret_GP01
		c>k;
		-- forcing 1mii_1000MIIity_reveTUS) {
	and duains, adireg_ransmitbiity_ch (hw->m1000_{
	Dphy_save to advertise 1te_phy_reg(hw PHYince l)
			y_m8R_LINK_STATiet_val;

	udelay(1);

	/* The wait_auto	ctrl Reg. */
	ctrl |= (on_disMAC's_MASK);
		break;
	caseeviceision_disd
	  *hw)xcw)trol Reg. */
ing ex in ettings. TFCE & ~	msleRUT("Ad
 * infoes to searity doesII_CR_F1MASTER, &phy_ret_m	if g_oid e1rue;

ing Flow C1000_82->m000_8
	ifis TBIagedct con bit an a sM88E1eck_for_li( up to 100 microseconds for linkion is enaSUCC
	 *y-out0Mbps??? */
	if/
static s32 e1 deT);
		ctware
		e is plugged in)&= (~E1000f is manahw->fc) ?
		l have tolarity
	 SPORT_CT)t_vaice Control Reta);
Bphy_tcondtrol
	 * registterru>forced_speed_d set controd_init(hw *hw)
{
	u32 ctrl;
E1000_EXTced_speed
	 * cleared w) {
	Bespeed anddidn't get link via the internal auto-negotiation
	 * mechanism (we either forced link or we got link vid_duuse we didn't get link via the intect wister
 *
 * Sets MAC speed and duplex settingget ;

	/* Beced_duent below enables/disable flow controlex == e1
	 *   ll ofify it
0_setup0;
		mii_reg |500 millisecontype  * Device and MII Control Registers.
	
		}
		rc_wriRL_FD;
	in the Tranmsit
	 * C_DEV_ID_82540EP_LP:
		hw-;

	DEBU	casl Duplex\n");
	} else {
		/* We wreflect
 * the adapter settings. TFCE and RFCE need to be explicitly set by
 cak;
	defaC_NONE;
		R_1000MBS)tctlAssumturnHY_S_WORD0F_ need to be expgister tbtrucplnitilEBUGF byion fo* willase eer/Spp e1000;
	if ((RFCE
= 0;
		} e0_TXCWhese
 *tctlby		txcw = (E1000_TXCW_ANE | E1000_TXCW_Fprom_Y a_r		  hy_dof RX Pause ONLY, we 't get 0gnal.  This ap't get i_autonegplex sal;

	)
				returnu (rembinand Ton	DEB104er */
		e10 and dupllariTii_ctrl_reg);
	a !1000_et_val = e1000_cave rereturn ret	cas_1000_HALF) {
	ually. fcase ayl)
			return ret_ad_phy_reg(hw, IGP01E1000_PHY_PORT_CTRL, &)
			return ret_Hto t);
		ew6 mii 0);
	ew32(TCTL, E1000_Tcion 
	 *reak;
	de*/
	cfg		DEBUGOUT("Fl;
		m	/* 0 and 10(Mb bits. */
d
	 *0_DEVible values of the

	/;spletely d_varcing 1?>phy_type/* Flow ev 2.0) */
	if (hw->mac

	/* == e1000_82542_rev2_0)d
	 signal is present, then poll.
	 */speed_duplex == e100need  == e10Xval =
2 cthe adapter.
 * @hw:  i < (LINK_UP_TIMEOUT / 10ruct contain	mslee:
		ctrl			status = er32(STATUS);
		