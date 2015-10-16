/*

  Broadcom B43 wireless driver
  IEEE 802.11a/g LP-PHY and radio device data tables

  Copyright (c) 2009 Michael Buesch <mb@bu3sch.de>
  Copyright (c) 2009 Gábor Stefanik <netrolller.3d@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
  Boston, MA 02110-1301, USA.

*/

#include "b43.h"
#include "tables_lpphy.h"
#include "phy_common.h"
#include "phy_lp.h"


/* Entry of the 2062/2063 radio init table */
struct b206x_init_tab_entry {
	u16 offset;
	u16 value_a;
	u16 value_g;
	u8 flags;
};
#define B206X_FLAG_A	0x01 /* Flag: Init in A mode */
#define B206X_FLAG_G	0x02 /* Flag: Init in G mode */

static const struct b206x_init_tab_entry b2062_init_tab[] = {
	/* { .offset = B2062_N_COMM1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = 0x0001, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_COMM2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_COMM3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_N_COMM4, .value_a = 0x0001, .value_g = 0x0000, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_N_COMM5, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_COMM6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_COMM7, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_COMM8, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_COMM9, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_COMM10, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_COMM11, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_COMM12, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_COMM13, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_COMM14, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_COMM15, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_PDN_CTL0, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_N_PDN_CTL1, .value_a = 0x0000, .value_g = 0x00CA, .flags = B206X_FLAG_G, },
	/* { .offset = B2062_N_PDN_CTL2, .value_a = 0x0018, .value_g = 0x0018, .flags = 0, }, */
	{ .offset = B2062_N_PDN_CTL3, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_N_PDN_CTL4, .value_a = 0x0015, .value_g = 0x002A, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_N_GEN_CTL0, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_IQ_CALIB, .value_a = 0x0001, .value_g = 0x0001, .flags = 0, }, */
	{ .offset = B2062_N_LGENC, .value_a = 0x00DB, .value_g = 0x00FF, .flags = B206X_FLAG_A, },
	/* { .offset = B2062_N_LGENA_LPF, .value_a = 0x0001, .value_g = 0x0001, .flags = 0, }, */
	/* { .offset = B2062_N_LGENA_BIAS0, .value_a = 0x0041, .value_g = 0x0041, .flags = 0, }, */
	/* { .offset = B2062_N_LGNEA_BIAS1, .value_a = 0x0002, .value_g = 0x0002, .flags = 0, }, */
	/* { .offset = B2062_N_LGENA_CTL0, .value_a = 0x0032, .value_g = 0x0032, .flags = 0, }, */
	/* { .offset = B2062_N_LGENA_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_LGENA_CTL2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_N_LGENA_TUNE0, .value_a = 0x00DD, .value_g = 0x0000, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_N_LGENA_TUNE1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_N_LGENA_TUNE2, .value_a = 0x00DD, .value_g = 0x0000, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_N_LGENA_TUNE3, .value_a = 0x0077, .value_g = 0x00B5, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_N_LGENA_CTL3, .value_a = 0x0000, .value_g = 0x00FF, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_N_LGENA_CTL4, .value_a = 0x001F, .value_g = 0x001F, .flags = 0, }, */
	/* { .offset = B2062_N_LGENA_CTL5, .value_a = 0x0032, .value_g = 0x0032, .flags = 0, }, */
	/* { .offset = B2062_N_LGENA_CTL6, .value_a = 0x0032, .value_g = 0x0032, .flags = 0, }, */
	{ .offset = B2062_N_LGENA_CTL7, .value_a = 0x0033, .value_g = 0x0033, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_N_RXA_CTL0, .value_a = 0x0009, .value_g = 0x0009, .flags = 0, }, */
	{ .offset = B2062_N_RXA_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	/* { .offset = B2062_N_RXA_CTL2, .value_a = 0x0018, .value_g = 0x0018, .flags = 0, }, */
	/* { .offset = B2062_N_RXA_CTL3, .value_a = 0x0027, .value_g = 0x0027, .flags = 0, }, */
	/* { .offset = B2062_N_RXA_CTL4, .value_a = 0x0028, .value_g = 0x0028, .flags = 0, }, */
	/* { .offset = B2062_N_RXA_CTL5, .value_a = 0x0007, .value_g = 0x0007, .flags = 0, }, */
	/* { .offset = B2062_N_RXA_CTL6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_RXA_CTL7, .value_a = 0x0008, .value_g = 0x0008, .flags = 0, }, */
	{ .offset = B2062_N_RXBB_CTL0, .value_a = 0x0082, .value_g = 0x0080, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_N_RXBB_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_CTL2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_GAIN0, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_N_RXBB_GAIN1, .value_a = 0x0004, .value_g = 0x0004, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_N_RXBB_GAIN2, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_N_RXBB_GAIN3, .value_a = 0x0011, .value_g = 0x0011, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_RSSI0, .value_a = 0x0043, .value_g = 0x0043, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_RSSI1, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_CALIB0, .value_a = 0x0010, .value_g = 0x0010, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_CALIB1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_CALIB2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_BIAS0, .value_a = 0x0006, .value_g = 0x0006, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_BIAS1, .value_a = 0x002A, .value_g = 0x002A, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_BIAS2, .value_a = 0x00AA, .value_g = 0x00AA, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_BIAS3, .value_a = 0x0021, .value_g = 0x0021, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_BIAS4, .value_a = 0x00AA, .value_g = 0x00AA, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_BIAS5, .value_a = 0x0022, .value_g = 0x0022, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_RSSI2, .value_a = 0x0001, .value_g = 0x0001, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_RSSI3, .value_a = 0x0055, .value_g = 0x0055, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_RSSI4, .value_a = 0x0001, .value_g = 0x0001, .flags = 0, }, */
	/* { .offset = B2062_N_RXBB_RSSI5, .value_a = 0x0055, .value_g = 0x0055, .flags = 0, }, */
	/* { .offset = B2062_N_TX_CTL0, .value_a = 0x0001, .value_g = 0x0001, .flags = 0, }, */
	/* { .offset = B2062_N_TX_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_TX_CTL2, .value_a = 0x0084, .value_g = 0x0084, .flags = 0, }, */
	/* { .offset = B2062_N_TX_CTL3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_N_TX_CTL4, .value_a = 0x0003, .value_g = 0x0003, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_N_TX_CTL5, .value_a = 0x0002, .value_g = 0x0002, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_N_TX_CTL6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_TX_CTL7, .value_a = 0x0058, .value_g = 0x0058, .flags = 0, }, */
	/* { .offset = B2062_N_TX_CTL8, .value_a = 0x0082, .value_g = 0x0082, .flags = 0, }, */
	/* { .offset = B2062_N_TX_CTL9, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_TX_CTL_A, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_TX_GC2G, .value_a = 0x00FF, .value_g = 0x00FF, .flags = 0, }, */
	/* { .offset = B2062_N_TX_GC5G, .value_a = 0x00FF, .value_g = 0x00FF, .flags = 0, }, */
	{ .offset = B2062_N_TX_TUNE, .value_a = 0x0088, .value_g = 0x001B, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_N_TX_PAD, .value_a = 0x0088, .value_g = 0x0088, .flags = 0, }, */
	/* { .offset = B2062_N_TX_PGA, .value_a = 0x0088, .value_g = 0x0088, .flags = 0, }, */
	/* { .offset = B2062_N_TX_PADAUX, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2062_N_TX_PGAAUX, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2062_N_TSSI_CTL0, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_TSSI_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_TSSI_CTL2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_IQ_CALIB_CTL0, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2062_N_IQ_CALIB_CTL1, .value_a = 0x0055, .value_g = 0x0055, .flags = 0, }, */
	/* { .offset = B2062_N_IQ_CALIB_CTL2, .value_a = 0x0032, .value_g = 0x0032, .flags = 0, }, */
	/* { .offset = B2062_N_CALIB_TS, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_CALIB_CTL0, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_CALIB_CTL1, .value_a = 0x0015, .value_g = 0x0015, .flags = 0, }, */
	/* { .offset = B2062_N_CALIB_CTL2, .value_a = 0x000F, .value_g = 0x000F, .flags = 0, }, */
	/* { .offset = B2062_N_CALIB_CTL3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_CALIB_CTL4, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_CALIB_DBG0, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_CALIB_DBG1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_CALIB_DBG2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_CALIB_DBG3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_PSENSE_CTL0, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_PSENSE_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_PSENSE_CTL2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_N_TEST_BUF0, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RADIO_ID_CODE, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_S_COMM4, .value_a = 0x0001, .value_g = 0x0000, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_S_COMM5, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM7, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM8, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM9, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM10, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM11, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM12, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM13, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM14, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_COMM15, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_S_PDS_CTL0, .value_a = 0x00FF, .value_g = 0x00FF, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_S_PDS_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_PDS_CTL2, .value_a = 0x008E, .value_g = 0x008E, .flags = 0, }, */
	/* { .offset = B2062_S_PDS_CTL3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_BG_CTL0, .value_a = 0x0006, .value_g = 0x0006, .flags = 0, }, */
	/* { .offset = B2062_S_BG_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_BG_CTL2, .value_a = 0x0011, .value_g = 0x0011, .flags = 0, }, */
	{ .offset = B2062_S_LGENG_CTL0, .value_a = 0x00F8, .value_g = 0x00D8, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_LGENG_CTL1, .value_a = 0x003C, .value_g = 0x0024, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_S_LGENG_CTL2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_LGENG_CTL3, .value_a = 0x0041, .value_g = 0x0041, .flags = 0, }, */
	/* { .offset = B2062_S_LGENG_CTL4, .value_a = 0x0002, .value_g = 0x0002, .flags = 0, }, */
	/* { .offset = B2062_S_LGENG_CTL5, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2062_S_LGENG_CTL6, .value_a = 0x0022, .value_g = 0x0022, .flags = 0, }, */
	/* { .offset = B2062_S_LGENG_CTL7, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_S_LGENG_CTL8, .value_a = 0x0088, .value_g = 0x0080, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_S_LGENG_CTL9, .value_a = 0x0088, .value_g = 0x0088, .flags = 0, }, */
	{ .offset = B2062_S_LGENG_CTL10, .value_a = 0x0088, .value_g = 0x0080, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_S_LGENG_CTL11, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL0, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL1, .value_a = 0x0007, .value_g = 0x0007, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL2, .value_a = 0x00AF, .value_g = 0x00AF, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL3, .value_a = 0x0012, .value_g = 0x0012, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL4, .value_a = 0x000B, .value_g = 0x000B, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL5, .value_a = 0x005F, .value_g = 0x005F, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL7, .value_a = 0x0040, .value_g = 0x0040, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL8, .value_a = 0x0052, .value_g = 0x0052, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL9, .value_a = 0x0026, .value_g = 0x0026, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL10, .value_a = 0x0003, .value_g = 0x0003, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL11, .value_a = 0x0036, .value_g = 0x0036, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL12, .value_a = 0x0057, .value_g = 0x0057, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL13, .value_a = 0x0011, .value_g = 0x0011, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL14, .value_a = 0x0075, .value_g = 0x0075, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL15, .value_a = 0x00B4, .value_g = 0x00B4, .flags = 0, }, */
	/* { .offset = B2062_S_REFPLL_CTL16, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_S_RFPLL_CTL0, .value_a = 0x0098, .value_g = 0x0098, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL1, .value_a = 0x0010, .value_g = 0x0010, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_S_RFPLL_CTL2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RFPLL_CTL3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RFPLL_CTL4, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_S_RFPLL_CTL5, .value_a = 0x0043, .value_g = 0x0043, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL6, .value_a = 0x0047, .value_g = 0x0047, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL7, .value_a = 0x000C, .value_g = 0x000C, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL8, .value_a = 0x0011, .value_g = 0x0011, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL9, .value_a = 0x0011, .value_g = 0x0011, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL10, .value_a = 0x000E, .value_g = 0x000E, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL11, .value_a = 0x0008, .value_g = 0x0008, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL12, .value_a = 0x0033, .value_g = 0x0033, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL13, .value_a = 0x000A, .value_g = 0x000A, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL14, .value_a = 0x0006, .value_g = 0x0006, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_S_RFPLL_CTL15, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RFPLL_CTL16, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RFPLL_CTL17, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_S_RFPLL_CTL18, .value_a = 0x003E, .value_g = 0x003E, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL19, .value_a = 0x0013, .value_g = 0x0013, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_S_RFPLL_CTL20, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_S_RFPLL_CTL21, .value_a = 0x0062, .value_g = 0x0062, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL22, .value_a = 0x0007, .value_g = 0x0007, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL23, .value_a = 0x0016, .value_g = 0x0016, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL24, .value_a = 0x005C, .value_g = 0x005C, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL25, .value_a = 0x0095, .value_g = 0x0095, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_S_RFPLL_CTL26, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RFPLL_CTL27, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RFPLL_CTL28, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RFPLL_CTL29, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_S_RFPLL_CTL30, .value_a = 0x00A0, .value_g = 0x00A0, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL31, .value_a = 0x0004, .value_g = 0x0004, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_S_RFPLL_CTL32, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_S_RFPLL_CTL33, .value_a = 0x00CC, .value_g = 0x00CC, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL34, .value_a = 0x0007, .value_g = 0x0007, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2062_S_RXG_CNT0, .value_a = 0x0010, .value_g = 0x0010, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT4, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT5, .value_a = 0x0055, .value_g = 0x0055, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT6, .value_a = 0x0055, .value_g = 0x0055, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT7, .value_a = 0x0005, .value_g = 0x0005, .flags = 0, }, */
	{ .offset = B2062_S_RXG_CNT8, .value_a = 0x000F, .value_g = 0x000F, .flags = B206X_FLAG_A, },
	/* { .offset = B2062_S_RXG_CNT9, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT10, .value_a = 0x0055, .value_g = 0x0055, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT11, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT12, .value_a = 0x0055, .value_g = 0x0055, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT13, .value_a = 0x0044, .value_g = 0x0044, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT14, .value_a = 0x00A0, .value_g = 0x00A0, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT15, .value_a = 0x0004, .value_g = 0x0004, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT16, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RXG_CNT17, .value_a = 0x0055, .value_g = 0x0055, .flags = 0, }, */
};

static const struct b206x_init_tab_entry b2063_init_tab[] = {
	{ .offset = B2063_COMM1, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	/* { .offset = B2063_COMM2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_COMM3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_COMM4, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_COMM5, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_COMM6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_COMM7, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_COMM8, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_COMM9, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_COMM10, .value_a = 0x0001, .value_g = 0x0000, .flags = B206X_FLAG_A, },
	/* { .offset = B2063_COMM11, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_COMM12, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_COMM13, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_COMM14, .value_a = 0x0006, .value_g = 0x0006, .flags = 0, }, */
	/* { .offset = B2063_COMM15, .value_a = 0x000f, .value_g = 0x000f, .flags = 0, }, */
	{ .offset = B2063_COMM16, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	{ .offset = B2063_COMM17, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	{ .offset = B2063_COMM18, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	{ .offset = B2063_COMM19, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	{ .offset = B2063_COMM20, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	{ .offset = B2063_COMM21, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	{ .offset = B2063_COMM22, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	{ .offset = B2063_COMM23, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	{ .offset = B2063_COMM24, .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	/* { .offset = B2063_PWR_SWITCH_CTL, .value_a = 0x007f, .value_g = 0x007f, .flags = 0, }, */
	/* { .offset = B2063_PLL_SP1, .value_a = 0x003f, .value_g = 0x003f, .flags = 0, }, */
	/* { .offset = B2063_PLL_SP2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_LOGEN_SP1, .value_a = 0x00e8, .value_g = 0x00d4, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2063_LOGEN_SP2, .value_a = 0x00a7, .value_g = 0x0053, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_LOGEN_SP3, .value_a = 0x00ff, .value_g = 0x00ff, .flags = 0, }, */
	{ .offset = B2063_LOGEN_SP4, .value_a = 0x00f0, .value_g = 0x000f, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_LOGEN_SP5, .value_a = 0x0001, .value_g = 0x0001, .flags = 0, }, */
	{ .offset = B2063_G_RX_SP1, .value_a = 0x001f, .value_g = 0x005e, .flags = B206X_FLAG_G, },
	{ .offset = B2063_G_RX_SP2, .value_a = 0x007f, .value_g = 0x007e, .flags = B206X_FLAG_G, },
	{ .offset = B2063_G_RX_SP3, .value_a = 0x0030, .value_g = 0x00f0, .flags = B206X_FLAG_G, },
	/* { .offset = B2063_G_RX_SP4, .value_a = 0x0035, .value_g = 0x0035, .flags = 0, }, */
	/* { .offset = B2063_G_RX_SP5, .value_a = 0x003f, .value_g = 0x003f, .flags = 0, }, */
	/* { .offset = B2063_G_RX_SP6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_G_RX_SP7, .value_a = 0x007f, .value_g = 0x007f, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_G_RX_SP8, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_G_RX_SP9, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_G_RX_SP10, .value_a = 0x000c, .value_g = 0x000c, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_G_RX_SP11, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_A_RX_SP1, .value_a = 0x003c, .value_g = 0x003f, .flags = B206X_FLAG_A, },
	{ .offset = B2063_A_RX_SP2, .value_a = 0x00fc, .value_g = 0x00fe, .flags = B206X_FLAG_A, },
	/* { .offset = B2063_A_RX_SP3, .value_a = 0x00ff, .value_g = 0x00ff, .flags = 0, }, */
	/* { .offset = B2063_A_RX_SP4, .value_a = 0x00ff, .value_g = 0x00ff, .flags = 0, }, */
	/* { .offset = B2063_A_RX_SP5, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_SP6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_A_RX_SP7, .value_a = 0x0008, .value_g = 0x0008, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_RX_BB_SP1, .value_a = 0x000f, .value_g = 0x000f, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_SP2, .value_a = 0x0022, .value_g = 0x0022, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_SP3, .value_a = 0x00a8, .value_g = 0x00a8, .flags = 0, }, */
	{ .offset = B2063_RX_BB_SP4, .value_a = 0x0060, .value_g = 0x0060, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_RX_BB_SP5, .value_a = 0x0011, .value_g = 0x0011, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_SP6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_SP7, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_RX_BB_SP8, .value_a = 0x0030, .value_g = 0x0030, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_TX_RF_SP1, .value_a = 0x0001, .value_g = 0x0001, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP2, .value_a = 0x0003, .value_g = 0x0003, .flags = 0, }, */
	{ .offset = B2063_TX_RF_SP3, .value_a = 0x000c, .value_g = 0x000b, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2063_TX_RF_SP4, .value_a = 0x0010, .value_g = 0x000f, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_TX_RF_SP5, .value_a = 0x000f, .value_g = 0x000f, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP6, .value_a = 0x0080, .value_g = 0x0080, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP7, .value_a = 0x0068, .value_g = 0x0068, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP8, .value_a = 0x0068, .value_g = 0x0068, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP9, .value_a = 0x0080, .value_g = 0x0080, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP10, .value_a = 0x00ff, .value_g = 0x00ff, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP11, .value_a = 0x0003, .value_g = 0x0003, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP12, .value_a = 0x0038, .value_g = 0x0038, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP13, .value_a = 0x00ff, .value_g = 0x00ff, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP14, .value_a = 0x0038, .value_g = 0x0038, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP15, .value_a = 0x00c0, .value_g = 0x00c0, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP16, .value_a = 0x00ff, .value_g = 0x00ff, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_SP17, .value_a = 0x00ff, .value_g = 0x00ff, .flags = 0, }, */
	{ .offset = B2063_PA_SP1, .value_a = 0x003d, .value_g = 0x00fd, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_PA_SP2, .value_a = 0x000c, .value_g = 0x000c, .flags = 0, }, */
	/* { .offset = B2063_PA_SP3, .value_a = 0x0096, .value_g = 0x0096, .flags = 0, }, */
	/* { .offset = B2063_PA_SP4, .value_a = 0x005a, .value_g = 0x005a, .flags = 0, }, */
	/* { .offset = B2063_PA_SP5, .value_a = 0x007f, .value_g = 0x007f, .flags = 0, }, */
	/* { .offset = B2063_PA_SP6, .value_a = 0x007f, .value_g = 0x007f, .flags = 0, }, */
	/* { .offset = B2063_PA_SP7, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	{ .offset = B2063_TX_BB_SP1, .value_a = 0x0002, .value_g = 0x0002, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_TX_BB_SP2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_TX_BB_SP3, .value_a = 0x0030, .value_g = 0x0030, .flags = 0, }, */
	/* { .offset = B2063_REG_SP1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_BANDGAP_CTL1, .value_a = 0x0056, .value_g = 0x0056, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_BANDGAP_CTL2, .value_a = 0x0006, .value_g = 0x0006, .flags = 0, }, */
	/* { .offset = B2063_LPO_CTL1, .value_a = 0x000e, .value_g = 0x000e, .flags = 0, }, */
	/* { .offset = B2063_RC_CALIB_CTL1, .value_a = 0x007e, .value_g = 0x007e, .flags = 0, }, */
	/* { .offset = B2063_RC_CALIB_CTL2, .value_a = 0x0015, .value_g = 0x0015, .flags = 0, }, */
	/* { .offset = B2063_RC_CALIB_CTL3, .value_a = 0x000f, .value_g = 0x000f, .flags = 0, }, */
	/* { .offset = B2063_RC_CALIB_CTL4, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_RC_CALIB_CTL5, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_RC_CALIB_CTL6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_RC_CALIB_CTL7, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_RC_CALIB_CTL8, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_RC_CALIB_CTL9, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_RC_CALIB_CTL10, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_CALNRST, .value_a = 0x0004, .value_g = 0x0004, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_IN_PLL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_IN_PLL2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_CP1, .value_a = 0x00cf, .value_g = 0x00cf, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_CP2, .value_a = 0x0059, .value_g = 0x0059, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_CP3, .value_a = 0x0007, .value_g = 0x0007, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_CP4, .value_a = 0x0042, .value_g = 0x0042, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_LF1, .value_a = 0x00db, .value_g = 0x00db, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_LF2, .value_a = 0x0094, .value_g = 0x0094, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_LF3, .value_a = 0x0028, .value_g = 0x0028, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_LF4, .value_a = 0x0063, .value_g = 0x0063, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_SG1, .value_a = 0x0007, .value_g = 0x0007, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_SG2, .value_a = 0x00d3, .value_g = 0x00d3, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_SG3, .value_a = 0x00b1, .value_g = 0x00b1, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_SG4, .value_a = 0x003b, .value_g = 0x003b, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_SG5, .value_a = 0x0006, .value_g = 0x0006, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_VCO1, .value_a = 0x0058, .value_g = 0x0058, .flags = 0, }, */
	{ .offset = B2063_PLL_JTAG_PLL_VCO2, .value_a = 0x00f7, .value_g = 0x00f7, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_PLL_JTAG_PLL_VCO_CALIB1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_VCO_CALIB2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_VCO_CALIB3, .value_a = 0x0002, .value_g = 0x0002, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_VCO_CALIB4, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_VCO_CALIB5, .value_a = 0x0009, .value_g = 0x0009, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_VCO_CALIB6, .value_a = 0x0005, .value_g = 0x0005, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_VCO_CALIB7, .value_a = 0x0016, .value_g = 0x0016, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_VCO_CALIB8, .value_a = 0x006b, .value_g = 0x006b, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_VCO_CALIB9, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_VCO_CALIB10, .value_a = 0x00b3, .value_g = 0x00b3, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_XTAL_12, .value_a = 0x0004, .value_g = 0x0004, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAG_PLL_XTAL3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_ACL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_ACL2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_ACL3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_ACL4, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_ACL5, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_INPUTS, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_CTL2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_CTL3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_WAITCNT, .value_a = 0x0002, .value_g = 0x0002, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_OVR1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_OVR2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_OVAL1, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_OVAL2, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_OVAL3, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_OVAL4, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_OVAL5, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_OVAL6, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_OVAL7, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_CALVLD1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_CALVLD2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_CVAL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_CVAL2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_CVAL3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_CVAL4, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_CVAL5, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_CVAL6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LO_CALIB_CVAL7, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_CALIB_EN, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_PEAKDET1, .value_a = 0x00ff, .value_g = 0x00ff, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_RCCR1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_VCOBUF1, .value_a = 0x0060, .value_g = 0x0060, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_MIXER1, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_MIXER2, .value_a = 0x000c, .value_g = 0x000c, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_BUF1, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_BUF2, .value_a = 0x000c, .value_g = 0x000c, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_DIV1, .value_a = 0x0001, .value_g = 0x0001, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_DIV2, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_DIV3, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_CBUFRX1, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_CBUFRX2, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_CBUFTX1, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_CBUFTX2, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_IDAC1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_SPARE1, .value_a = 0x0001, .value_g = 0x0001, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_SPARE2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_LOGEN_SPARE3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_G_RX_1ST1, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2063_G_RX_1ST2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_G_RX_1ST3, .value_a = 0x0005, .value_g = 0x0005, .flags = 0, }, */
	/* { .offset = B2063_G_RX_2ND1, .value_a = 0x0030, .value_g = 0x0030, .flags = 0, }, */
	/* { .offset = B2063_G_RX_2ND2, .value_a = 0x0055, .value_g = 0x0055, .flags = 0, }, */
	/* { .offset = B2063_G_RX_2ND3, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2063_G_RX_2ND4, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_G_RX_2ND5, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2063_G_RX_2ND6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_G_RX_2ND7, .value_a = 0x0035, .value_g = 0x0035, .flags = 0, }, */
	/* { .offset = B2063_G_RX_2ND8, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_G_RX_PS1, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2063_G_RX_PS2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_G_RX_PS3, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2063_G_RX_PS4, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_G_RX_PS5, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_G_RX_MIX1, .value_a = 0x0044, .value_g = 0x0044, .flags = 0, }, */
	/* { .offset = B2063_G_RX_MIX2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_G_RX_MIX3, .value_a = 0x0071, .value_g = 0x0071, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2063_G_RX_MIX4, .value_a = 0x0071, .value_g = 0x0071, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_G_RX_MIX5, .value_a = 0x0003, .value_g = 0x0003, .flags = 0, }, */
	/* { .offset = B2063_G_RX_MIX6, .value_a = 0x0088, .value_g = 0x0088, .flags = 0, }, */
	/* { .offset = B2063_G_RX_MIX7, .value_a = 0x0044, .value_g = 0x0044, .flags = 0, }, */
	/* { .offset = B2063_G_RX_MIX8, .value_a = 0x0001, .value_g = 0x0001, .flags = 0, }, */
	/* { .offset = B2063_G_RX_PDET1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_G_RX_SPARES1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_G_RX_SPARES2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_G_RX_SPARES3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_1ST1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_A_RX_1ST2, .value_a = 0x00f0, .value_g = 0x0030, .flags = B206X_FLAG_A, },
	/* { .offset = B2063_A_RX_1ST3, .value_a = 0x0005, .value_g = 0x0005, .flags = 0, }, */
	/* { .offset = B2063_A_RX_1ST4, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2063_A_RX_1ST5, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_2ND1, .value_a = 0x0005, .value_g = 0x0005, .flags = 0, }, */
	/* { .offset = B2063_A_RX_2ND2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_2ND3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_2ND4, .value_a = 0x0005, .value_g = 0x0005, .flags = 0, }, */
	/* { .offset = B2063_A_RX_2ND5, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_2ND6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_2ND7, .value_a = 0x0005, .value_g = 0x0005, .flags = 0, }, */
	/* { .offset = B2063_A_RX_PS1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_PS2, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2063_A_RX_PS3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_PS4, .value_a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2063_A_RX_PS5, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_A_RX_PS6, .value_a = 0x0077, .value_g = 0x0077, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_A_RX_MIX1, .value_a = 0x0088, .value_g = 0x0088, .flags = 0, }, */
	/* { .offset = B2063_A_RX_MIX2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_MIX3, .value_a = 0x0044, .value_g = 0x0044, .flags = 0, }, */
	{ .offset = B2063_A_RX_MIX4, .value_a = 0x0003, .value_g = 0x0003, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2063_A_RX_MIX5, .value_a = 0x000f, .value_g = 0x000f, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2063_A_RX_MIX6, .value_a = 0x000f, .value_g = 0x000f, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_A_RX_MIX7, .value_a = 0x0044, .value_g = 0x0044, .flags = 0, }, */
	/* { .offset = B2063_A_RX_MIX8, .value_a = 0x0001, .value_g = 0x0001, .flags = 0, }, */
	/* { .offset = B2063_A_RX_PWRDET1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_SPARE1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_SPARE2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_A_RX_SPARE3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_RX_TIA_CTL1, .value_a = 0x0077, .value_g = 0x0077, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_RX_TIA_CTL2, .value_a = 0x0058, .value_g = 0x0058, .flags = 0, }, */
	{ .offset = B2063_RX_TIA_CTL3, .value_a = 0x0077, .value_g = 0x0077, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_RX_TIA_CTL4, .value_a = 0x0058, .value_g = 0x0058, .flags = 0, }, */
	/* { .offset = B2063_RX_TIA_CTL5, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_RX_TIA_CTL6, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_CTL1, .value_a = 0x0074, .value_g = 0x0074, .flags = 0, }, */
	{ .offset = B2063_RX_BB_CTL2, .value_a = 0x0004, .value_g = 0x0004, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_RX_BB_CTL3, .value_a = 0x00a2, .value_g = 0x00a2, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_CTL4, .value_a = 0x00aa, .value_g = 0x00aa, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_CTL5, .value_a = 0x0024, .value_g = 0x0024, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_CTL6, .value_a = 0x00a9, .value_g = 0x00a9, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_CTL7, .value_a = 0x0028, .value_g = 0x0028, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_CTL8, .value_a = 0x0010, .value_g = 0x0010, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_CTL9, .value_a = 0x0055, .value_g = 0x0055, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_CTL1, .value_a = 0x0080, .value_g = 0x0080, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_IDAC_LO_RF_I, .value_a = 0x0088, .value_g = 0x0088, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_IDAC_LO_RF_Q, .value_a = 0x0088, .value_g = 0x0088, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_IDAC_LO_BB_I, .value_a = 0x0088, .value_g = 0x0088, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_IDAC_LO_BB_Q, .value_a = 0x0088, .value_g = 0x0088, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_CTL2, .value_a = 0x0080, .value_g = 0x0080, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_CTL3, .value_a = 0x0038, .value_g = 0x0038, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_CTL4, .value_a = 0x00b8, .value_g = 0x00b8, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_CTL5, .value_a = 0x0080, .value_g = 0x0080, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_CTL6, .value_a = 0x0038, .value_g = 0x0038, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_CTL7, .value_a = 0x0078, .value_g = 0x0078, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_CTL8, .value_a = 0x00c0, .value_g = 0x00c0, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_CTL9, .value_a = 0x0003, .value_g = 0x0003, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_CTL10, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_CTL14, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_TX_RF_CTL15, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_PA_CTL1, .value_a = 0x0000, .value_g = 0x0004, .flags = B206X_FLAG_A, },
	/* { .offset = B2063_PA_CTL2, .value_a = 0x000c, .value_g = 0x000c, .flags = 0, }, */
	/* { .offset = B2063_PA_CTL3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PA_CTL4, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PA_CTL5, .value_a = 0x0096, .value_g = 0x0096, .flags = 0, }, */
	/* { .offset = B2063_PA_CTL6, .value_a = 0x0077, .value_g = 0x0077, .flags = 0, }, */
	/* { .offset = B2063_PA_CTL7, .value_a = 0x005a, .value_g = 0x005a, .flags = 0, }, */
	/* { .offset = B2063_PA_CTL8, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PA_CTL9, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PA_CTL10, .value_a = 0x0021, .value_g = 0x0021, .flags = 0, }, */
	/* { .offset = B2063_PA_CTL11, .value_a = 0x0070, .value_g = 0x0070, .flags = 0, }, */
	/* { .offset = B2063_PA_CTL12, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_PA_CTL13, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_TX_BB_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_TX_BB_CTL2, .value_a = 0x00b3, .value_g = 0x00b3, .flags = 0, }, */
	/* { .offset = B2063_TX_BB_CTL3, .value_a = 0x0055, .value_g = 0x0055, .flags = 0, }, */
	/* { .offset = B2063_TX_BB_CTL4, .value_a = 0x000b, .value_g = 0x000b, .flags = 0, }, */
	/* { .offset = B2063_GPIO_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2063_VREG_CTL1, .value_a = 0x0003, .value_g = 0x0003, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B2063_AMUX_CTL1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_IQ_CALIB_GVAR, .value_a = 0x00b3, .value_g = 0x00b3, .flags = 0, }, */
	/* { .offset = B2063_IQ_CALIB_CTL1, .value_a = 0x0055, .value_g = 0x0055, .flags = 0, }, */
	/* { .offset = B2063_IQ_CALIB_CTL2, .value_a = 0x0030, .value_g = 0x0030, .flags = 0, }, */
	/* { .offset = B2063_TEMPSENSE_CTL1, .value_a = 0x0046, .value_g = 0x0046, .flags = 0, }, */
	/* { .offset = B2063_TEMPSENSE_CTL2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_TX_RX_LOOPBACK1, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_TX_RX_LOOPBACK2, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2063_EXT_TSSI_CTL1, .value_a = 0x0021, .value_g = 0x0021, .flags = 0, }, */
	/* { .offset = B2063_EXT_TSSI_CTL2, .value_a = 0x0023, .value_g = 0x0023, .flags = 0, }, */
	/* { .offset = B2063_AFE_CTL , .value_a = 0x0002, .value_g = 0x0002, .flags = 0, }, */
};

void b2062_upload_init_table(struct b43_wldev *dev)
{
	const struct b206x_init_tab_entry *e;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(b2062_init_tab); i++) {
		e = &b2062_init_tab[i];
		if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
			if (!(e->flags & B206X_FLAG_G))
				continue;
			b43_radio_write(dev, e->offset, e->value_g);
		} else {
			if (!(e->flags & B206X_FLAG_A))
				continue;
			b43_radio_write(dev, e->offset, e->value_a);
		}
	}
}

void b2063_upload_init_table(struct b43_wldev *dev)
{
	const struct b206x_init_tab_entry *e;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(b2063_init_tab); i++) {
		e = &b2063_init_tab[i];
		if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
			if (!(e->flags & B206X_FLAG_G))
				continue;
			b43_radio_write(dev, e->offset, e->value_g);
		} else {
			if (!(e->flags & B206X_FLAG_A))
				continue;
			b43_radio_write(dev, e->offset, e->value_a);
		}
	}
}

u32 b43_lptab_read(struct b43_wldev *dev, u32 offset)
{
	u32 type, value;

	type = offset & B43_LPTAB_TYPEMASK;
	offset &= ~B43_LPTAB_TYPEMASK;
	B43_WARN_ON(offset > 0xFFFF);

	switch (type) {
	case B43_LPTAB_8BIT:
		b43_phy_write(dev, B43_LPPHY_TABLE_ADDR, offset);
		value = b43_phy_read(dev, B43_LPPHY_TABLEDATALO) & 0xFF;
		break;
	case B43_LPTAB_16BIT:
		b43_phy_write(dev, B43_LPPHY_TABLE_ADDR, offset);
		value = b43_phy_read(dev, B43_LPPHY_TABLEDATALO);
		break;
	case B43_LPTAB_32BIT:
		b43_phy_write(dev, B43_LPPHY_TABLE_ADDR, offset);
		value = b43_phy_read(dev, B43_LPPHY_TABLEDATAHI);
		value <<= 16;
		value |= b43_phy_read(dev, B43_LPPHY_TABLEDATALO);
		break;
	default:
		B43_WARN_ON(1);
		value = 0;
	}

	return value;
}

void b43_lptab_read_bulk(struct b43_wldev *dev, u32 offset,
			 unsigned int nr_elements, void *_data)
{
	u32 type;
	u8 *data = _data;
	unsigned int i;

	type = offset & B43_LPTAB_TYPEMASK;
	offset &= ~B43_LPTAB_TYPEMASK;
	B43_WARN_ON(offset > 0xFFFF);

	b43_phy_write(dev, B43_LPPHY_TABLE_ADDR, offset);

	for (i = 0; i < nr_elements; i++) {
		switch (type) {
		case B43_LPTAB_8BIT:
			*data = b43_phy_read(dev, B43_LPPHY_TABLEDATALO) & 0xFF;
			data++;
			break;
		case B43_LPTAB_16BIT:
			*((u16 *)data) = b43_phy_read(dev, B43_LPPHY_TABLEDATALO);
			data += 2;
			break;
		case B43_LPTAB_32BIT:
			*((u32 *)data) = b43_phy_read(dev, B43_LPPHY_TABLEDATAHI);
			*((u32 *)data) <<= 16;
			*((u32 *)data) |= b43_phy_read(dev, B43_LPPHY_TABLEDATALO);
			data += 4;
			break;
		default:
			B43_WARN_ON(1);
		}
	}
}

void b43_lptab_write(struct b43_wldev *dev, u32 offset, u32 value)
{
	u32 type;

	type = offset & B43_LPTAB_TYPEMASK;
	offset &= ~B43_LPTAB_TYPEMASK;
	B43_WARN_ON(offset > 0xFFFF);

	switch (type) {
	case B43_LPTAB_8BIT:
		B43_WARN_ON(value & ~0xFF);
		b43_phy_write(dev, B43_LPPHY_TABLE_ADDR, offset);
		b43_phy_write(dev, B43_LPPHY_TABLEDATALO, value);
		break;
	case B43_LPTAB_16BIT:
		B43_WARN_ON(value & ~0xFFFF);
		b43_phy_write(dev, B43_LPPHY_TABLE_ADDR, offset);
		b43_phy_write(dev, B43_LPPHY_TABLEDATALO, value);
		break;
	case B43_LPTAB_32BIT:
		b43_phy_write(dev, B43_LPPHY_TABLE_ADDR, offset);
		b43_phy_write(dev, B43_LPPHY_TABLEDATAHI, value >> 16);
		b43_phy_write(dev, B43_LPPHY_TABLEDATALO, value);
		break;
	default:
		B43_WARN_ON(1);
	}
}

void b43_lptab_write_bulk(struct b43_wldev *dev, u32 offset,
			  unsigned int nr_elements, const void *_data)
{
	u32 type, value;
	const u8 *data = _data;
	unsigned int i;

	type = offset & B43_LPTAB_TYPEMASK;
	offset &= ~B43_LPTAB_TYPEMASK;
	B43_WARN_ON(offset > 0xFFFF);

	b43_phy_write(dev, B43_LPPHY_TABLE_ADDR, offset);

	for (i = 0; i < nr_elements; i++) {
		switch (type) {
		case B43_LPTAB_8BIT:
			value = *data;
			data++;
			B43_WARN_ON(value & ~0xFF);
			b43_phy_write(dev, B43_LPPHY_TABLEDATALO, value);
			break;
		case B43_LPTAB_16BIT:
			value = *((u16 *)data);
			data += 2;
			B43_WARN_ON(value & ~0xFFFF);
			b43_phy_write(dev, B43_LPPHY_TABLEDATALO, value);
			break;
		case B43_LPTAB_32BIT:
			value = *((u32 *)data);
			data += 4;
			b43_phy_write(dev, B43_LPPHY_TABLEDATAHI, value >> 16);
			b43_phy_write(dev, B43_LPPHY_TABLEDATALO, value);
			break;
		default:
			B43_WARN_ON(1);
		}
	}
}

static const u8 lpphy_min_sig_sq_table[] = {
	0xde, 0xdc, 0xda, 0xd8, 0xd6, 0xd4, 0xd2, 0xcf, 0xcd,
	0xca, 0xc7, 0xc4, 0xc1, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe,
	0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0x00,
	0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe,
	0xbe, 0xbe, 0xbe, 0xbe, 0xc1, 0xc4, 0xc7, 0xca, 0xcd,
	0xcf, 0xd2, 0xd4, 0xd6, 0xd8, 0xda, 0xdc, 0xde,
};

static const u16 lpphy_rev01_noise_scale_table[] = {
	0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4,
	0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa400, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4,
	0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0x00a4,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x4c00, 0x2d36,
	0x0000, 0x0000, 0x4c00, 0x2d36,
};

static const u16 lpphy_rev2plus_noise_scale_table[] = {
	0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4,
	0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4,
	0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x0000,
	0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4,
	0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4,
	0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4,
	0x00a4,
};

static const u16 lpphy_crs_gain_nft_table[] = {
	0x0366, 0x036a, 0x036f, 0x0364, 0x0367, 0x036d, 0x0374, 0x037f, 0x036f,
	0x037b, 0x038a, 0x0378, 0x0367, 0x036d, 0x0375, 0x0381, 0x0374, 0x0381,
	0x0392, 0x03a9, 0x03c4, 0x03e1, 0x0001, 0x001f, 0x0040, 0x005e, 0x007f,
	0x009e, 0x00bd, 0x00dd, 0x00fd, 0x011d, 0x013d,
};

static const u16 lpphy_rev01_filter_control_table[] = {
	0xa0fc, 0x10fc, 0x10db, 0x20b7, 0xff93, 0x10bf, 0x109b, 0x2077, 0xff53,
	0x0127,
};

static const u32 lpphy_rev2plus_filter_control_table[] = {
	0x000141fc, 0x000021fc, 0x000021b7, 0x0000416f, 0x0001ff27, 0x0000217f,
	0x00002137, 0x000040ef, 0x0001fea7, 0x0000024f,
};

static const u32 lpphy_rev01_ps_control_table[] = {
	0x00010000, 0x000000a0, 0x00040000, 0x00000048, 0x08080101, 0x00000080,
	0x08080101, 0x00000040, 0x08080101, 0x000000c0, 0x08a81501, 0x000000c0,
	0x0fe8fd01, 0x000000c0, 0x08300105, 0x000000c0, 0x08080201, 0x000000c0,
	0x08280205, 0x000000c0, 0xe80802fe, 0x000000c7, 0x28080206, 0x000000c0,
	0x08080202, 0x000000c0, 0x0ba87602, 0x000000c0, 0x1068013d, 0x000000c0,
	0x10280105, 0x000000c0, 0x08880102, 0x000000c0, 0x08280106, 0x000000c0,
	0xe80801fd, 0x000000c7, 0xa8080115, 0x000000c0,
};

static const u32 lpphy_rev2plus_ps_control_table[] = {
	0x00e38e08, 0x00e08e38, 0x00000000, 0x00000000, 0x00000000, 0x00002080,
	0x00006180, 0x00003002, 0x00000040, 0x00002042, 0x00180047, 0x00080043,
	0x00000041, 0x000020c1, 0x00046006, 0x00042002, 0x00040000, 0x00002003,
	0x00180006, 0x00080002,
};

static const u8 lpphy_pll_fraction_table[] = {
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
};

static const u16 lpphy_iqlo_cal_table[] = {
	0x0200, 0x0300, 0x0400, 0x0600, 0x0800, 0x0b00, 0x1000, 0x1001, 0x1002,
	0x1003, 0x1004, 0x1005, 0x1006, 0x1007, 0x1707, 0x2007, 0x2d07, 0x4007,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0200, 0x0300, 0x0400, 0x0600,
	0x0800, 0x0b00, 0x1000, 0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006,
	0x1007, 0x1707, 0x2007, 0x2d07, 0x4007, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x4000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

static const u16 lpphy_rev0_ofdm_cck_gain_table[] = {
	0x0001, 0x0001, 0x0001, 0x0001, 0x1001, 0x2001, 0x3001, 0x4001, 0x5001,
	0x6001, 0x7001, 0x7011, 0x7021, 0x2035, 0x2045, 0x2055, 0x2065, 0x2075,
	0x006d, 0x007d, 0x014d, 0x015d, 0x115d, 0x035d, 0x135d, 0x055d, 0x155d,
	0x0d5d, 0x1d5d, 0x2d5d, 0x555d, 0x655d, 0x755d,
};

static const u16 lpphy_rev1_ofdm_cck_gain_table[] = {
	0x5000, 0x6000, 0x7000, 0x0001, 0x1001, 0x2001, 0x3001, 0x4001, 0x5001,
	0x6001, 0x7001, 0x7011, 0x7021, 0x2035, 0x2045, 0x2055, 0x2065, 0x2075,
	0x006d, 0x007d, 0x014d, 0x015d, 0x115d, 0x035d, 0x135d, 0x055d, 0x155d,
	0x0d5d, 0x1d5d, 0x2d5d, 0x555d, 0x655d, 0x755d,
};

static const u16 lpphy_gain_delta_table[] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

static const u32 lpphy_tx_power_control_table[] = {
	0x00000050, 0x0000004f, 0x0000004e, 0x0000004d, 0x0000004c, 0x0000004b,
	0x0000004a, 0x00000049, 0x00000048, 0x00000047, 0x00000046, 0x00000045,
	0x00000044, 0x00000043, 0x00000042, 0x00000041, 0x00000040, 0x0000003f,
	0x0000003e, 0x0000003d, 0x0000003c, 0x0000003b, 0x0000003a, 0x00000039,
	0x00000038, 0x00000037, 0x00000036, 0x00000035, 0x00000034, 0x00000033,
	0x00000032, 0x00000031, 0x00000030, 0x0000002f, 0x0000002e, 0x0000002d,
	0x0000002c, 0x0000002b, 0x0000002a, 0x00000029, 0x00000028, 0x00000027,
	0x00000026, 0x00000025, 0x00000024, 0x00000023, 0x00000022, 0x00000021,
	0x00000020, 0x0000001f, 0x0000001e, 0x0000001d, 0x0000001c, 0x0000001b,
	0x0000001a, 0x00000019, 0x00000018, 0x00000017, 0x00000016, 0x00000015,
	0x00000014, 0x00000013, 0x00000012, 0x00000011, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x000075a0, 0x000075a0, 0x000075a1, 0x000075a1, 0x000075a2, 0x000075a2,
	0x000075a3, 0x000075a3, 0x000074b0, 0x000074b0, 0x000074b1, 0x000074b1,
	0x000074b2, 0x000074b2, 0x000074b3, 0x000074b3, 0x00006d20, 0x00006d20,
	0x00006d21, 0x00006d21, 0x00006d22, 0x00006d22, 0x00006d23, 0x00006d23,
	0x00004660, 0x00004660, 0x00004661, 0x00004661, 0x00004662, 0x00004662,
	0x00004663, 0x00004663, 0x00003e60, 0x00003e60, 0x00003e61, 0x00003e61,
	0x00003e62, 0x00003e62, 0x00003e63, 0x00003e63, 0x00003660, 0x00003660,
	0x00003661, 0x00003661, 0x00003662, 0x00003662, 0x00003663, 0x00003663,
	0x00002e60, 0x00002e60, 0x00002e61, 0x00002e61, 0x00002e62, 0x00002e62,
	0x00002e63, 0x00002e63, 0x00002660, 0x00002660, 0x00002661, 0x00002661,
	0x00002662, 0x00002662, 0x00002663, 0x00002663, 0x000025e0, 0x000025e0,
	0x000025e1, 0x000025e1, 0x000025e2, 0x000025e2, 0x000025e3, 0x000025e3,
	0x00001de0, 0x00001de0, 0x00001de1, 0x00001de1, 0x00001de2, 0x00001de2,
	0x00001de3, 0x00001de3, 0x00001d60, 0x00001d60, 0x00001d61, 0x00001d61,
	0x00001d62, 0x00001d62, 0x00001d63, 0x00001d63, 0x00001560, 0x00001560,
	0x00001561, 0x00001561, 0x00001562, 0x00001562, 0x00001563, 0x00001563,
	0x00000d60, 0x00000d60, 0x00000d61, 0x00000d61, 0x00000d62, 0x00000d62,
	0x00000d63, 0x00000d63, 0x00000ce0, 0x00000ce0, 0x00000ce1, 0x00000ce1,
	0x00000ce2, 0x00000ce2, 0x00000ce3, 0x00000ce3, 0x00000e10, 0x00000e10,
	0x00000e11, 0x00000e11, 0x00000e12, 0x00000e12, 0x00000e13, 0x00000e13,
	0x00000bf0, 0x00000bf0, 0x00000bf1, 0x00000bf1, 0x00000bf2, 0x00000bf2,
	0x00000bf3, 0x00000bf3, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
	0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x000000ff, 0x000002fc,
	0x0000fa08, 0x00000305, 0x00000206, 0x00000304, 0x0000fb04, 0x0000fcff,
	0x000005fb, 0x0000fd01, 0x00000401, 0x00000006, 0x0000ff03, 0x000007fc,
	0x0000fc08, 0x00000203, 0x0000fffb, 0x00000600, 0x0000fa01, 0x0000fc03,
	0x0000fe06, 0x0000fe00, 0x00000102, 0x000007fd, 0x000004fb, 0x000006ff,
	0x000004fd, 0x0000fdfa, 0x000007fb, 0x0000fdfa, 0x0000fa06, 0x00000500,
	0x0000f902, 0x000007fa, 0x0000fafa, 0x00000500, 0x000007fa, 0x00000700,
	0x00000305, 0x000004ff, 0x00000801, 0x00000503, 0x000005f9, 0x00000404,
	0x0000fb08, 0x000005fd, 0x00000501, 0x00000405, 0x0000fb03, 0x000007fc,
	0x00000403, 0x00000303, 0x00000402, 0x0000faff, 0x0000fe05, 0x000005fd,
	0x0000fe01, 0x000007fa, 0x00000202, 0x00000504, 0x00000102, 0x000008fe,
	0x0000fa04, 0x0000fafc, 0x0000fe08, 0x000000f9, 0x000002fa, 0x000003fe,
	0x00000304, 0x000004f9, 0x00000100, 0x0000fd06, 0x000008fc, 0x00000701,
	0x00000504, 0x0000fdfe, 0x0000fdfc, 0x000003fe, 0x00000704, 0x000002fc,
	0x000004f9, 0x0000fdfd, 0x0000fa07, 0x00000205, 0x000003fd, 0x000005fb,
	0x000004f9, 0x00000804, 0x0000fc06, 0x0000fcf9, 0x00000100, 0x0000fe05,
	0x00000408, 0x0000fb02, 0x00000304, 0x000006fe, 0x000004fa, 0x00000305,
	0x000008fc, 0x00000102, 0x000001fd, 0x000004fc, 0x0000fe03, 0x00000701,
	0x000001fb, 0x000001f9, 0x00000206, 0x000006fd, 0x00000508, 0x00000700,
	0x00000304, 0x000005fe, 0x000005ff, 0x0000fa04, 0x00000303, 0x0000fefb,
	0x000007f9, 0x0000fefc, 0x000004fd, 0x000005fc, 0x0000fffd, 0x0000fc08,
	0x0000fbf9, 0x0000fd07, 0x000008fb, 0x0000fe02, 0x000006fb, 0x00000702,
};

static const u32 lpphy_gain_idx_table[] = {
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x10000001, 0x00000000, 0x20000082, 0x00000000, 0x40000104, 0x00000000,
	0x60004207, 0x00000001, 0x7000838a, 0x00000001, 0xd021050d, 0x00000001,
	0xe041c683, 0x00000001, 0x50828805, 0x00000000, 0x80e34288, 0x00000000,
	0xb144040b, 0x00000000, 0xe1a6058e, 0x00000000, 0x12064711, 0x00000001,
	0xb0a18612, 0x00000010, 0xe1024794, 0x00000010, 0x11630915, 0x00000011,
	0x31c3ca1b, 0x00000011, 0xc1848a9c, 0x00000018, 0xf1e50da0, 0x00000018,
	0x22468e21, 0x00000019, 0x4286d023, 0x00000019, 0xa347d0a4, 0x00000019,
	0xb36811a6, 0x00000019, 0xf3e89227, 0x00000019, 0x0408d329, 0x0000001a,
	0x244953aa, 0x0000001a, 0x346994ab, 0x0000001a, 0x54aa152c, 0x0000001a,
	0x64ca55ad, 0x0000001a, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x10000001, 0x00000000, 0x20000082, 0x00000000,
	0x40000104, 0x00000000, 0x60004207, 0x00000001, 0x7000838a, 0x00000001,
	0xd021050d, 0x00000001, 0xe041c683, 0x00000001, 0x50828805, 0x00000000,
	0x80e34288, 0x00000000, 0xb144040b, 0x00000000, 0xe1a6058e, 0x00000000,
	0x12064711, 0x00000001, 0xb0a18612, 0x00000010, 0xe1024794, 0x00000010,
	0x11630915, 0x00000011, 0x31c3ca1b, 0x00000011, 0xc1848a9c, 0x00000018,
	0xf1e50da0, 0x00000018, 0x22468e21, 0x00000019, 0x4286d023, 0x00000019,
	0xa347d0a4, 0x00000019, 0xb36811a6, 0x00000019, 0xf3e89227, 0x00000019,
	0x0408d329, 0x0000001a, 0x244953aa, 0x0000001a, 0x346994ab, 0x0000001a,
	0x54aa152c, 0x0000001a, 0x64ca55ad, 0x0000001a,
};

static const u16 lpphy_aux_gain_idx_table[] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0001, 0x0002, 0x0004, 0x0016, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0002, 0x0004, 0x0016,
};

static const u32 lpphy_gain_value_table[] = {
	0x00000008, 0x0000000e, 0x00000014, 0x0000001a, 0x000000fb, 0x00000004,
	0x00000008, 0x0000000d, 0x00000001, 0x00000004, 0x00000007, 0x0000000a,
	0x0000000d, 0x00000010, 0x00000012, 0x00000015, 0x00000000, 0x00000006,
	0x0000000c, 0x00000000, 0x00000000, 0x00000000, 0x00000012, 0x00000000,
	0x00000000, 0x00000000, 0x00000018, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x0000001e, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000003, 0x00000006, 0x00000009, 0x0000000c, 0x0000000f,
	0x00000012, 0x00000015, 0x00000018, 0x0000001b, 0x0000001e, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000009, 0x000000f1,
	0x00000000, 0x00000000,
};

static const u16 lpphy_gain_table[] = {
	0x0000, 0x0400, 0x0800, 0x0802, 0x0804, 0x0806, 0x0807, 0x0808, 0x080a,
	0x080b, 0x080c, 0x080e, 0x080f, 0x0810, 0x0812, 0x0813, 0x0814, 0x0816,
	0x0817, 0x081a, 0x081b, 0x081f, 0x0820, 0x0824, 0x0830, 0x0834, 0x0837,
	0x083b, 0x083f, 0x0840, 0x0844, 0x0857, 0x085b, 0x085f, 0x08d7, 0x08db,
	0x08df, 0x0957, 0x095b, 0x095f, 0x0b57, 0x0b5b, 0x0b5f, 0x0f5f, 0x135f,
	0x175f, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

static const u32 lpphy_a0_gain_idx_table[] = {
	0x001111e0, 0x00652051, 0x00606055, 0x005b005a, 0x00555060, 0x00511065,
	0x004c806b, 0x0047d072, 0x00444078, 0x00400080, 0x003ca087, 0x0039408f,
	0x0035e098, 0x0032e0a1, 0x003030aa, 0x002d80b4, 0x002ae0bf, 0x002880ca,
	0x002640d6, 0x002410e3, 0x002220f0, 0x002020ff, 0x001e510e, 0x001ca11e,
	0x001b012f, 0x00199140, 0x00182153, 0x0016c168, 0x0015817d, 0x00145193,
	0x001321ab, 0x001211c5, 0x001111e0, 0x001021fc, 0x000f321a, 0x000e523a,
	0x000d925c, 0x000cd27f, 0x000c12a5, 0x000b62cd, 0x000ac2f8, 0x000a2325,
	0x00099355, 0x00091387, 0x000883bd, 0x000813f5, 0x0007a432, 0x00073471,
	0x0006c4b5, 0x000664fc, 0x00061547, 0x0005b598, 0x000565ec, 0x00051646,
	0x0004d6a5, 0x0004870a, 0x00044775, 0x000407e6, 0x0003d85e, 0x000398dd,
	0x00036963, 0x000339f2, 0x00030a89, 0x0002db28,
};

static const u16 lpphy_a0_aux_gain_idx_table[] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0002, 0x0014, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0002, 0x0014,
};

static const u32 lpphy_a0_gain_value_table[] = {
	0x00000008, 0x0000000e, 0x00000014, 0x0000001a, 0x000000fb, 0x00000004,
	0x00000008, 0x0000000d, 0x00000001, 0x00000004, 0x00000007, 0x0000000a,
	0x0000000d, 0x00000010, 0x00000012, 0x00000015, 0x00000000, 0x00000006,
	0x0000000c, 0x00000000, 0x00000000, 0x00000000, 0x00000012, 0x00000000,
	0x00000000, 0x00000000, 0x00000018, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x0000001e, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000003, 0x00000006, 0x00000009, 0x0000000c, 0x0000000f,
	0x00000012, 0x00000015, 0x00000018, 0x0000001b, 0x0000001e, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000f, 0x000000f7,
	0x00000000, 0x00000000,
};

static const u16 lpphy_a0_gain_table[] = {
	0x0000, 0x0002, 0x0004, 0x0006, 0x0007, 0x0008, 0x000a, 0x000b, 0x000c,
	0x000e, 0x000f, 0x0010, 0x0012, 0x0013, 0x0014, 0x0016, 0x0017, 0x001a,
	0x001b, 0x001f, 0x0020, 0x0024, 0x0030, 0x0034, 0x0037, 0x003b, 0x003f,
	0x0040, 0x0044, 0x0057, 0x005b, 0x005f, 0x00d7, 0x00db, 0x00df, 0x0157,
	0x015b, 0x015f, 0x0357, 0x035b, 0x035f, 0x075f, 0x0b5f, 0x0f5f, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

static const u16 lpphy_sw_control_table[] = {
	0x0128, 0x0128, 0x0009, 0x0009, 0x0028, 0x0028, 0x0028, 0x0028, 0x0128,
	0x0128, 0x0009, 0x0009, 0x0028, 0x0028, 0x0028, 0x0028, 0x0009, 0x0009,
	0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0018, 0x0018, 0x0018,
	0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0128, 0x0128, 0x0009, 0x0009,
	0x0028, 0x0028, 0x0028, 0x0028, 0x0128, 0x0128, 0x0009, 0x0009, 0x0028,
	0x0028, 0x0028, 0x0028, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009,
	0x0009, 0x0009, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018,
	0x0018,
};

static const u8 lpphy_hf_table[] = {
	0x4b, 0x36, 0x24, 0x18, 0x49, 0x34, 0x23, 0x17, 0x48,
	0x33, 0x23, 0x17, 0x48, 0x33, 0x23, 0x17,
};

static const u32 lpphy_papd_eps_table[] = {
	0x00000000, 0x00013ffc, 0x0001dff3, 0x0001bff0, 0x00023fe9, 0x00021fdf,
	0x00028fdf, 0x00033fd2, 0x00039fcb, 0x00043fc7, 0x0004efc2, 0x00055fb5,
	0x0005cfb0, 0x00063fa8, 0x00068fa3, 0x00071f98, 0x0007ef92, 0x00084f8b,
	0x0008df82, 0x00097f77, 0x0009df69, 0x000a3f62, 0x000adf57, 0x000b6f4c,
	0x000bff41, 0x000c9f39, 0x000cff30, 0x000dbf27, 0x000e4f1e, 0x000edf16,
	0x000f7f13, 0x00102f11, 0x00110f10, 0x0011df11, 0x0012ef15, 0x00143f1c,
	0x00158f27, 0x00172f35, 0x00193f47, 0x001baf5f, 0x001e6f7e, 0x0021cfa4,
	0x0025bfd2, 0x002a2008, 0x002fb047, 0x00360090, 0x003d40e0, 0x0045c135,
	0x004fb189, 0x005ae1d7, 0x0067221d, 0x0075025a, 0x007ff291, 0x007ff2bf,
	0x007ff2e3, 0x007ff2ff, 0x007ff315, 0x007ff329, 0x007ff33f, 0x007ff356,
	0x007ff36e, 0x007ff39c, 0x007ff441, 0x007ff506,
};

static const u32 lpphy_papd_mult_table[] = {
	0x001111e0, 0x00652051, 0x00606055, 0x005b005a, 0x00555060, 0x00511065,
	0x004c806b, 0x0047d072, 0x00444078, 0x00400080, 0x003ca087, 0x0039408f,
	0x0035e098, 0x0032e0a1, 0x003030aa, 0x002d80b4, 0x002ae0bf, 0x002880ca,
	0x002640d6, 0x002410e3, 0x002220f0, 0x002020ff, 0x001e510e, 0x001ca11e,
	0x001b012f, 0x00199140, 0x00182153, 0x0016c168, 0x0015817d, 0x00145193,
	0x001321ab, 0x001211c5, 0x001111e0, 0x001021fc, 0x000f321a, 0x000e523a,
	0x000d925c, 0x000cd27f, 0x000c12a5, 0x000b62cd, 0x000ac2f8, 0x000a2325,
	0x00099355, 0x00091387, 0x000883bd, 0x000813f5, 0x0007a432, 0x00073471,
	0x0006c4b5, 0x000664fc, 0x00061547, 0x0005b598, 0x000565ec, 0x00051646,
	0x0004d6a5, 0x0004870a, 0x00044775, 0x000407e6, 0x0003d85e, 0x000398dd,
	0x00036963, 0x000339f2, 0x00030a89, 0x0002db28,
};

static struct lpphy_tx_gain_table_entry lpphy_rev0_nopa_tx_gain_table[] = {
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 152, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 147, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 143, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 139, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 135, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 131, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 128, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 124, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 121, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 117, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 114, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 111, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 107, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 104, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 101, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 99, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 96, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 93, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 90, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 88, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 85, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 83, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 81, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 78, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 76, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 74, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 71, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 69, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 67, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 58, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 58, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 71, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 69, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 67, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 58, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 56, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 73, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 71, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 69, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 67, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 73, },
	{ .gm = 7, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 71, },
};

static struct lpphy_tx_gain_table_entry lpphy_rev0_2ghz_tx_gain_table[] = {
	{ .gm = 4, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 64, },
	{ .gm = 4, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 62, },
	{ .gm = 4, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 60, },
	{ .gm = 4, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 72, },
	{ .gm = 4, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 70, },
	{ .gm = 4, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 68, },
	{ .gm = 4, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 66, },
	{ .gm = 4, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 64, },
	{ .gm = 4, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 62, },
	{ .gm = 4, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 60, },
	{ .gm = 4, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 72, },
	{ .gm = 4, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 70, },
	{ .gm = 4, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 68, },
	{ .gm = 4, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 66, },
	{ .gm = 4, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 64, },
	{ .gm = 4, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 63, },
	{ .gm = 4, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 61, },
	{ .gm = 4, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 57, },
	{ .gm = 4, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 72, },
	{ .gm = 4, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 70, },
	{ .gm = 4, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 68, },
	{ .gm = 4, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 66, },
	{ .gm = 4, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 64, },
	{ .gm = 4, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 62, },
	{ .gm = 4, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 60, },
	{ .gm = 4, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 72, },
	{ .gm = 4, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 70, },
	{ .gm = 4, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 68, },
	{ .gm = 4, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 66, },
	{ .gm = 4, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 64, },
	{ .gm = 4, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 62, },
	{ .gm = 4, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 61, },
	{ .gm = 4, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 73, },
	{ .gm = 4, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 71, },
	{ .gm = 4, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 69, },
	{ .gm = 4, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 67, },
	{ .gm = 4, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 65, },
	{ .gm = 4, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 63, },
	{ .gm = 4, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 61, },
	{ .gm = 4, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 72, },
	{ .gm = 4, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 70, },
	{ .gm = 4, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 68, },
	{ .gm = 4, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 66, },
	{ .gm = 4, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 65, },
	{ .gm = 4, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 63, },
	{ .gm = 4, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 61, },
	{ .gm = 4, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 73, },
	{ .gm = 4, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 71, },
	{ .gm = 4, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 69, },
	{ .gm = 4, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 67, },
	{ .gm = 4, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 65, },
	{ .gm = 4, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 63, },
	{ .gm = 4, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 61, },
	{ .gm = 4, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 60, },
	{ .gm = 4, .pga = 10, .pad = 6, .dac = 0, .bb_mult = 72, },
	{ .gm = 4, .pga = 10, .pad = 6, .dac = 0, .bb_mult = 70, },
	{ .gm = 4, .pga = 10, .pad = 6, .dac = 0, .bb_mult = 68, },
	{ .gm = 4, .pga = 10, .pad = 6, .dac = 0, .bb_mult = 66, },
	{ .gm = 4, .pga = 10, .pad = 6, .dac = 0, .bb_mult = 64, },
	{ .gm = 4, .pga = 10, .pad = 6, .dac = 0, .bb_mult = 62, },
	{ .gm = 4, .pga = 10, .pad = 6, .dac = 0, .bb_mult = 60, },
	{ .gm = 4, .pga = 10, .pad = 6, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 10, .pad = 5, .dac = 0, .bb_mult = 72, },
	{ .gm = 4, .pga = 10, .pad = 5, .dac = 0, .bb_mult = 70, },
	{ .gm = 4, .pga = 10, .pad = 5, .dac = 0, .bb_mult = 68, },
	{ .gm = 4, .pga = 10, .pad = 5, .dac = 0, .bb_mult = 66, },
	{ .gm = 4, .pga = 10, .pad = 5, .dac = 0, .bb_mult = 64, },
	{ .gm = 4, .pga = 10, .pad = 5, .dac = 0, .bb_mult = 62, },
	{ .gm = 4, .pga = 10, .pad = 5, .dac = 0, .bb_mult = 60, },
	{ .gm = 4, .pga = 10, .pad = 5, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 9, .pad = 5, .dac = 0, .bb_mult = 70, },
	{ .gm = 4, .pga = 9, .pad = 5, .dac = 0, .bb_mult = 68, },
	{ .gm = 4, .pga = 9, .pad = 5, .dac = 0, .bb_mult = 66, },
	{ .gm = 4, .pga = 9, .pad = 5, .dac = 0, .bb_mult = 64, },
	{ .gm = 4, .pga = 9, .pad = 5, .dac = 0, .bb_mult = 63, },
	{ .gm = 4, .pga = 9, .pad = 5, .dac = 0, .bb_mult = 61, },
	{ .gm = 4, .pga = 9, .pad = 5, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 9, .pad = 4, .dac = 0, .bb_mult = 71, },
	{ .gm = 4, .pga = 9, .pad = 4, .dac = 0, .bb_mult = 69, },
	{ .gm = 4, .pga = 9, .pad = 4, .dac = 0, .bb_mult = 67, },
	{ .gm = 4, .pga = 9, .pad = 4, .dac = 0, .bb_mult = 65, },
	{ .gm = 4, .pga = 9, .pad = 4, .dac = 0, .bb_mult = 63, },
	{ .gm = 4, .pga = 9, .pad = 4, .dac = 0, .bb_mult = 62, },
	{ .gm = 4, .pga = 9, .pad = 4, .dac = 0, .bb_mult = 60, },
	{ .gm = 4, .pga = 9, .pad = 4, .dac = 0, .bb_mult = 58, },
	{ .gm = 4, .pga = 8, .pad = 4, .dac = 0, .bb_mult = 70, },
	{ .gm = 4, .pga = 8, .pad = 4, .dac = 0, .bb_mult = 68, },
	{ .gm = 4, .pga = 8, .pad = 4, .dac = 0, .bb_mult = 66, },
	{ .gm = 4, .pga = 8, .pad = 4, .dac = 0, .bb_mult = 65, },
	{ .gm = 4, .pga = 8, .pad = 4, .dac = 0, .bb_mult = 63, },
	{ .gm = 4, .pga = 8, .pad = 4, .dac = 0, .bb_mult = 61, },
	{ .gm = 4, .pga = 8, .pad = 4, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 7, .pad = 4, .dac = 0, .bb_mult = 68, },
	{ .gm = 4, .pga = 7, .pad = 4, .dac = 0, .bb_mult = 66, },
	{ .gm = 4, .pga = 7, .pad = 4, .dac = 0, .bb_mult = 64, },
	{ .gm = 4, .pga = 7, .pad = 4, .dac = 0, .bb_mult = 62, },
	{ .gm = 4, .pga = 7, .pad = 4, .dac = 0, .bb_mult = 61, },
	{ .gm = 4, .pga = 7, .pad = 4, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 7, .pad = 3, .dac = 0, .bb_mult = 67, },
	{ .gm = 4, .pga = 7, .pad = 3, .dac = 0, .bb_mult = 65, },
	{ .gm = 4, .pga = 7, .pad = 3, .dac = 0, .bb_mult = 63, },
	{ .gm = 4, .pga = 7, .pad = 3, .dac = 0, .bb_mult = 62, },
	{ .gm = 4, .pga = 7, .pad = 3, .dac = 0, .bb_mult = 60, },
	{ .gm = 4, .pga = 6, .pad = 3, .dac = 0, .bb_mult = 65, },
	{ .gm = 4, .pga = 6, .pad = 3, .dac = 0, .bb_mult = 63, },
	{ .gm = 4, .pga = 6, .pad = 3, .dac = 0, .bb_mult = 61, },
	{ .gm = 4, .pga = 6, .pad = 3, .dac = 0, .bb_mult = 60, },
	{ .gm = 4, .pga = 6, .pad = 3, .dac = 0, .bb_mult = 58, },
	{ .gm = 4, .pga = 5, .pad = 3, .dac = 0, .bb_mult = 68, },
	{ .gm = 4, .pga = 5, .pad = 3, .dac = 0, .bb_mult = 66, },
	{ .gm = 4, .pga = 5, .pad = 3, .dac = 0, .bb_mult = 64, },
	{ .gm = 4, .pga = 5, .pad = 3, .dac = 0, .bb_mult = 62, },
	{ .gm = 4, .pga = 5, .pad = 3, .dac = 0, .bb_mult = 60, },
	{ .gm = 4, .pga = 5, .pad = 3, .dac = 0, .bb_mult = 59, },
	{ .gm = 4, .pga = 5, .pad = 3, .dac = 0, .bb_mult = 57, },
	{ .gm = 4, .pga = 4, .pad = 2, .dac = 0, .bb_mult = 83, },
	{ .gm = 4, .pga = 4, .pad = 2, .dac = 0, .bb_mult = 81, },
	{ .gm = 4, .pga = 4, .pad = 2, .dac = 0, .bb_mult = 78, },
	{ .gm = 4, .pga = 4, .pad = 2, .dac = 0, .bb_mult = 76, },
	{ .gm = 4, .pga = 4, .pad = 2, .dac = 0, .bb_mult = 74, },
	{ .gm = 4, .pga = 4, .pad = 2, .dac = 0, .bb_mult = 72, },
};

static struct lpphy_tx_gain_table_entry lpphy_rev0_5ghz_tx_gain_table[] = {
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 99, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 96, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 93, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 90, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 88, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 85, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 83, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 81, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 78, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 76, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 74, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 15, .pad = 15, .dac = 0, .bb_mult = 55, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 58, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 56, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 55, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 71, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 69, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 67, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 58, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 56, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 73, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 71, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 69, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 67, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 58, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 71, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 69, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 67, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 58, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 15, .pad = 9, .dac = 0, .bb_mult = 56, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 14, .pad = 9, .dac = 0, .bb_mult = 58, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 13, .pad = 9, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 13, .pad = 8, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 12, .pad = 8, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 12, .pad = 7, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 11, .pad = 7, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 69, },
	{ .gm = 7, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 67, },
	{ .gm = 7, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 11, .pad = 6, .dac = 0, .bb_mult = 60, },
};

static struct lpphy_tx_gain_table_entry lpphy_rev1_nopa_tx_gain_table[] = {
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 152, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 147, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 143, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 139, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 135, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 131, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 128, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 124, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 121, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 117, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 114, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 111, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 107, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 104, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 101, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 99, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 96, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 93, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 90, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 88, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 85, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 83, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 81, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 78, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 76, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 74, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 15, .pad = 14, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 72, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 64, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 15, .pad = 13, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 71, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 69, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 67, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 62, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 58, },
	{ .gm = 7, .pga = 15, .pad = 12, .dac = 0, .bb_mult = 57, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 70, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 68, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 66, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 59, },
	{ .gm = 7, .pga = 15, .pad = 11, .dac = 0, .bb_mult = 58, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 71, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 69, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 67, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 65, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 63, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 61, },
	{ .gm = 7, .pga = 15, .pad = 10, .dac = 0, .bb_mult = 60, },
	{ .gm = 7, .pga = 15com ad3 wi0, .dac = er
 bb_mult = 58, },/*

  BroadelesB4drivreless driv802. IEEE 802.11a/g LP-PH6 and radio device data tablesadioCo9right (c) 2009 Michael 70esch <mb@bu3sch.de>t (c)pyright (c) 2009 Gábor Stefanik <n6Yesch <mb@bu3sch.de>
  Copyright (c)am is free software; you urolller.3d@gmail.com>

  This program is free software; you 4an redistribute itesch/or modify
 itheunder the terms ofse, 2e Foundation; either version 2 of the License, or
  (at yourtrolller.3d@gmail.com>adioThis progrit under the terms of th59esch <mb@bu3sch.de>
  Cop4his program is free software; youe option) any later versionwarranty of
  MERCHANTABILITY or FI the hope that it will be POSE.  See the
  GNU General Publi ce Foundation; either versadioYou should have received a copy GNU General Public Licens License
  along with this program;re FoundaSS F; eithICULAR  License
  along with this program;TNESS FOR A PARTICULAR PURPOSE.  Seese, 
  GNU General Publinse, ohop43.hatithewill be.

  You should have received a cop; without evephy.h"implied 3OSE.  See the
  GNU General PublicA 02110-1301, USA.

*/

#itCopyri */
struct b206x_init_tab_en the hope that it will be lue_a;
	u16 value_g;
	u8 flags;
};y of the GNU General Public/* Flag: Init in Aion 
	u16#definee GNU General Public Licens Init in G mode */

static const stFoundation, Inc., 51 Frank Init in G mode */

static const st3 B2062_N_COMM1, .value_43 w0x000er
 .offseg = 0x0001, .flags1= 0, }, */
	/* { .offset = 0x0001, .value_a = 0x0000, .valuat your 0, }/2063 <mb@bu flaue_a;
	u16 value_g;
	u8 flags;
};57= 0, }, */
	/* { .offset = 0x0001, 8 value_g;
	u8 flags;
};
#dy {
	u16 offset;flags .offset, */
= 0x0001, .value_a = 0x00const st 0, X_FLAG_A	0x01  Init in G= 0x0001, .value_a = 0x0 .value_g = Gx0002, .flags =  mode *G.value_g = 0 | .value_g =x0000, .value_g = 0x0000, g;
	28 fla, .value_a = 0x0000, .val== 0, }, */
	/* { .offset = 0x0001, , .value_a = 0x0000, .val00, .value = 0 and	u16*

 = 0, } 6, .value_a = 4 { .offset =phy.h"
#include "phy_commonue_a = 0x0000, .valuet = B2062_N_C of the 2062/2063 radio ini2 .offset = 0x0001, .value_a = 0x0000, .valuet = B2062_N_COMMfset = B2062_N_COMM9, .va/* OMM7, . the hope that it will be _a = 0x0000, .value_g = 0x0000, .fy of the GNU General Publicvalue_a = 0x0000,11, .value_t = 0x0 GNU General Public Licenslags = 0, }, */
	/* { .offset = B20Foundation, Inc., 51 Franklags = 0, }, */
	/* { .offset = B20value_a = 0x0000,9 { .offset = 0x0001, .value_a = 0x0000, .v/* { .offset = B2062_N_COMM_a = 0x0000, .value_g = 0x0000, .fset = 0x0001, .value_a = 0x0000, .valice MERCHANTABILITY or FIT.value_g = 0x0000, .flet /
	/ .value_g = 0 0x0001, .value_a MM13 { .offset = 0x0001, .value_a = ffset = 0x0001, .value_a6flags = 0, }, *6, .value_a = 0* { .offset = B2062_N_COMM15, 6OMM7, .value_a = 0x0000,3,{ .offset = B2062_N_COMM9, .vax00005* { .offset = B2062_N_COMM7, .value_a = 0x0PDN_CTL* { .offseoffset = B2062_N_PDN_CTL1, .value_M7, .value_a = 0x0000,15,*/
	/ue_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { of the 2062/2063 radio ini1
	/* { .offset = B2062_N_COMM15, .TNESS FOR A PARTICULAR PURffset = B2062_N_, .value_a = 0x0000 the hope that it will be B2062_N_PDN_CTL4, .value_a = 0x001y of the GNU General PublicCTL4, .value_a = 0x0015},
	/* { and GNU General Public LicensEN_CTL0, .value_a = 0x0000, .value_t = 0x0001, .value_a = 0x0EN_CTL0, .value_a = 0x0000, .value_= 0x0000, .flags = 0, }, *EN_CTL0, .value_a = 0x0000, .value_* { .offset = B2062_N_COMMB2062_N_PDN_CTL4, .value_a = 0x001fset = 0x0001, .value_a = 0x0000, .va6 .flags = 0, }, */
	/* { .offset = B2062_N_COMM9, ue_a = 0x000* { 062_N_PDN_CTL1, .va_a = 0};

static  value_lpphy_tx_gaings;
lex0000, 062_N_rev1_2ghzflags = 0, }, [] = { radio devPOSE.ata tables

  Copblesht (c) 2009 Michael 9 the hope that = B20622 0x0000, .value_g = 0x0000, .fl062_N_P8can redistribut { .offset = 0x003e_a value_a = 0x00_N_LGB2062_t = B2062_N_COM{ .offset = B2062_N_LGENA_CTL1, .value_a = 0x00= 0x0000, .flag{ .offset = B2062_N_LGENA_CTL1, .value_a = 0x00_a = 0x0000, .v, .value_a = 0x0032, .value_g = 0x0032, .flags732, .value_g = 0x0032, .flags, }, *LGENALAG_G, },
	/* t = B207 GNU General Pu62_N_PDN_CTL4, .value_a = 0x0015000, .value_
	/Foundation, Inc2062_N_LGENA_TUNE1, .value_a = 0x0000, .value_gTNESS FOR A PAR2062_N_LGENA_TUNE1, .value_a = 0x0000, .value_g06X_FLAG_A | B* { .offset = B202_N_LGENA_CTL1, .value_a = 0x00 of the GNU Genlue_00, .flags = B206X_FLAG_A TUNEue_a = 0x00000000, .flags = B206X_FLAG_A AG_A* { .offset = 0x0001, .value_alue_g = 0x0000,ags = B206X_FLAG_A | Bue_a = 0x0000, .value_g ={ .offset = 0x00DD}, */
	/* { .offset = B2062_N_, .value_a = phy.h"
#include , .value_a = 0x0032, .value_g = 0x0032, .flags of the 2062/206, .value_a = 0x0032, .POSEfset = B2062_N_LGENN_LGENA_CT = B2062_N_LGENA_CTL4, .valuex0000, .value_g = 0x0000,x001F_a = 0x0032, .value_g = 0x0032, 2062_N_PDN_CTL1, .value_= B2067dcomvalue_a = 0x00B5ue_a = 0x00B206X_FLAG_A | Bx0033, .v_G, },
	{ .offset = B2062_N_LGENA_CTLs = B206X_FLAG_A | B206X_lue_a = 0x00F .flags = 0,.value_g = 0s = B206X_FLAG_A | B206X_ .flags = B206X_FLAG_A | B6},
	{ .offset = B2062_N_LGENA_TUNE3032, .flags = 0, }, */
	{ .offset = B2062_N_LGENA_CTL7, .valu
	{ .offset = B2062_N_LGENA_TUNE3, .va_g =gs = 0, }, */
	/* { .offset = B2062_N_LGENA_CTL6, .value_a 0x0000, .flags = B206X_F032, .flags = 0, }, */
	{ .offset = B 0x0000, .flags = B206X_e_a = 0x0033, .value_g = 0x0033, .flag8, .value_g = 0x280033, ._G, },
	{ .offset = B2062_N_LGENA_CTL .offset = B2062_N_RXA_CT, .value_g = 0x0009, .flags = 0, }, * .offset = B2062_N_RXA_CTRXLAG_A | B206X_FLAG_G, }2x0033, .value_g = 
	/*  = 0x0000, .v */
	/* { .offset = B2062_N_RXA_CTL4, .value_a = 0x0028, .val
	{ .offset = B2062_N_LGENA_TUNE3, .va0001gs = 0, }, */
	/* { .offset = B2062_N_LGENA_CTL6, .value_a008N_LGENA_CTL1, .val80,gs =, .flags = 0, }, */
	{ .offset = B },
	/* { .offset = B206e_a = 0x0033, .value_g = 0x0033, .flag	/* { .offset = B2062_N_C_G, },
	{ .offset = B2062_N_LGENA_CTL	/* { .offset = B2062_N_C, .value_g = 0x0009, .flags = 0, }, *	/* { .offset = B2062_N_C_N_LGENA_TUNE1, .value_a = 0x0000, .value_g0000, .flags = B20value_a = 0x0008, .value_g = 0x0008, . },
	/* { .offset = B206
	{ .offset = B2062_N_LGENA_TUNE3, .vax000, .value_g = 0x0080, .flags = B206X_FLAG_A | B206X_FLAG_G, x0001, .value_a = 0x0000_N_RXBB_CTL1, .value_a = 0x0000, .valux0001, .value_a = 0x000e_a = 0x0033, .value_g = 0x0033, .flagvalue_a = 0x001lue_a = 0x_G, },
	{ .offset = B2062_N_LGENA_CTLBB_RSSI0, .value_a = 0x00, .value_g = 0x0009, .flags = 0, }, *BB_RSSI0, .value_a = 0x0062_N_LGENA_TUNE1, .value_a = 0x0000, .value_g0000, .flags = B262_N_RXBB_GAIN3, .value_a = 0x0011, .value_g = 0x0011, .flags
	{ .offset = B2062_N_LGENA_TUNE3, .vayright (c) 2009 Michael  { .offset = 0x00_N_LGENA_CTL4, .value
	/* { .offset = B2062_N__N_RXBB_CTL1, .value_a = 0x0000, .valyright (c) 2009 Michael e_a = 0x0033, .value_g = 0x0033, .flag = 0x0000, .flags = B206X_G, },
	{ .offset = B2062_N_LGENA_CTL = 0x0000, .flags = B206X, .value_g = 0x0009, .flags = 0, }, * = 0x0000, .flags = B206Xalue_g = 0x0000, .flags = B206X_FRXBB_CALIB .offset = B2062_N10, .value_g = 0x0010, .flags = 0, }, yright (c) 2009 Michael B	{ .offset = B2062_N_LGENA_TUNE3, .v
struct b206x_init_tab_entry {
	u16 offslue_a = 0x0041, .value_ .value_a = 0x00AA, .va_N_RXBB_CTL1, .value_a = 0x0000, .va mode */

static const st_G, },
	/* { .o_a = 0x00AA, .value_BIASx003ags = 0, }, */
2,043, .value_g = 0x0043, .flags = 0, }shed bof talueFr thS terms undation, Incffset = 0x000* { .offseIAS0, .value_a = 0x0041}, */
	/* { .offset = B206AA0033, .value_g =  = 0,B2062_N_CO10, .value_g = 0x0010, .flags = 0, },lue_a = 0x0018, .value_ .offset = B2062_, .value_a POSE.  See the
  GNU General PublicA 02110-1301, Uue_a = 0x00AA, .value_RSSI* { .offset = B20655_N_RXBB_CTL1, .value_a = 0 License
  along with this program;, .valuenclude fset = B2062_N_RXBB_RSSI3, .value_a = 0x0055,043, .value_g = 0x0043, .fl not, write tox0001, .flags = 0, }, */
	/* { , Inc_a = 0x00AA, .value_g = 0x00AA, .flags = 0,TX_0033, .value_g = 5x0033, .flagalue_g = 0x0000, .flags = B20610, .value_g = 0x0010, .fla.h"
#include " */
lp*/
	

/* E000,  .value_a = 0x0, .value_a lue_a;
	u16 value_g;
	u8 flags;
};
#d, .flags = 0,, .value_a = 0x000ue_aPDN_CTL4, .value_a = 0x0_N_RXBB_CTL1, .value_a = 0x00it in G mode */

static const st_G, },
	/* { .ofvalue_a = 0x0032, .value_a = 0x0000, .valvalu0000, .flags = B206X_FLAG_ }, */
	/*gs;
x0002,e_g = 0x00AA, .flags = 0,/
	/* 0x0032, .value_g = 0x0032, .flags 0, }, */CTLX_FLAG_A | B .offset = B20= B2062_N_RXBB_RSt = 0x0001, .value_a = 0x0000, .v= 0, }, */
	/N_RXA_CT 0x0082, .,
	/* { .offse.offset = 0x000ue_a = 0x0et = B2062_N_TX_CTL5, .value_a = 0x0value_a = 0x0018= 0, }, */
	/* { .offgs = 0, }, */
	/* { .offset = B2062_N_CO}, */
	/*= 0, }, */t = 0x0001, .value_a = 0x000N_LGENA_CTL1, .valB2062_N_TX_CTL6, }, */
	/* { .offset = B20001, .value_a =alue_g = 0x00 0x0009, .flaalue_g = 0x0000, .fl* { .offset = B2062_N_COMM9, .vag = 	/* { .offset = B2062_N_COMM9, .vag = 0xN_COMM7, .value_a = 0x0TX_LGEN { .offset = 0x000e_a = 0x00AA, .value_g = 0x00AA, .flags = 0,TX_GC2G { .off10, .value_g = 0x0010, .fla062_N_TX_CTL7, .value_a = * { .offset = B20601, .v.value_g = _a = 0x0000, .value_g = 0x0000, .fl062_N_PDN_CTL0,008* { .offsefset = B2N_RXe_a = 0x0088, .value_N_RXBB_CTL1, .value_a = 0lags = 0, }, */
	/* { .offset = B20001, .value_a = CTL3, .value_a = 0x0000, .value_a = 0xPGAAUX,0000, .flags = B206X_FLAG_ 0, }, */
	/* { .offset = B2062_N_COMM9, .vag = 0x= 0, }SSIG_G, },
	{ .offset = B01, .value_a =  0x0033, .flags = 0, }, *AD	/* 8, .value_g = 0x00, .value_g  B2062_N_TB2062_N_COMM9, .value_a = 0x0000, .value_g = 0x0000, .flags 
	/* {  0, }, */
	/N_LGENA_C= 0, }, */
	/* { .offset = B2062_N_COMM2,flags = 0, }, */
	/* { .offset = IQ_g = 0/
	/_TX_PAD, .value_a = , */
	/* { .offs_a = 0x00AA, .value_g = 0x00AA, .flags =flags = 0, }, */
	/* { .offset = 0x00B2062.off8, .flags = 0, }, */
	/*A,0CA, .flags = B206X_FLAG_G, },
	/* t = B2062_N_COMoffset = B2062_N_LGENA_TUNE3, .value_a =000, .v0x0000, .flags = 0, }, */
	/* ags = 0, },offset = B2062_N_COMM7, .value_a = 0x0_N_IQ_CALIB_CTL1, .flags = B206X_FLAG_A LP .fl.offset = 0.value_g = B2062_N_PDN_CTL4, .value_a = 0x0015x0000, .value_{062_N_PDN_CTL0}, */
	/* { .offset = B206g = .v_N_RXBB_CTL1, .value_a = 0/
	/* { .offset = B2062_N_RXBB_CALIB0, .value_a =  }, */
	/*  */
	/* { a = 0x000 .flags = 0, }, lue_g = 0x0000, .flags = 0,.value_g = 0x0033, .flags = 0, }, **/
	/* { .offs},
	/* { .offset = B2062_N_TX_PAD, .value_a =Clue_g = 0x0000, .flags = 0, }, */
	/* { .oALIB_CTL4,alue_g = 0x0009, .flags /* { .offset = B}, */DBG* { .offset = B2062_Ns = 0, }, */
	/* { .offset = B}, */
	value_a = 0x0041, .value_}, */
	/* { .of, .flags = 0, }, */
	/* { .offset = B2062_N_CO */
	/* { .offset = B2062_N_COMM9, .value_a = 0x0041, .value .offset = B2062_N_TX_CTL3,033, .value_g = 0x0033, .flags = 0,LIB_CTL2, .value_a = 0x003033, .value_g = 0x0033, .flags = 0,}, */
	/* { .offset = B2062_N_COMM15 0, }, */
	/* { .offset =ags = 0, }, */
	/* { .offset = B}, */
	/x00AA, .flags = 0, 0, */
	/* { .offset = B2062_N_COMM9, .*/
	/* { .offset = B2062_ 0x0033, .flags = 0, }, */
righ */
	/* { .offset = B2062_N_COB_RSSI0, .valux0033, .flagx0088, .flags = 0, }, */
	/* { .offsL0, .value_a = 0x0000, .value_g = 0x0000, .flags 
	/* { .ofgs = 0, }, */
	/* { .offset6, .valuS*/
	/* { .offset = 0x0001,0000, .flags = B206X_FLAG_ = 0, }, */
	/* { .offset = B2062_Soffset = B2062_N_COMM9, .v 0x0000, .value_g = 0x0000, .flags == 0, }EST_BUF* { .offset = B2062_N_TSSI_CTL1, 0x0000, .valu2062_N_004ue_a = 0x00AA, .value_g = 0x00AA, .flags = 0,LGNEA_B5AS* { .offset = 0x000N_LGENA_CTL1ice data tables

  Copgs = 0, }, */
	/* { .offsof the 2062/2063 radio ini2_N_RXBB_RSSI3, .vale_a = 0x001F, .v GNU General Public License as publiTL4, .value_a = 0x001F, .v033, .value_g = 0x0033, .f_a = 0x0000, .value_g = 0x0000, .flags = 0,FLAG_G, s = 0, }, */
	/* { .offset = B2ue_g = 0x0000, 033, .value_g = e_g = 0x00AA, .flags =  */
	/x0033, .vat = B206t = 0x0001, .value_a = 0x0_a = 0x0000, .value_g = 0x0000, .flags = 0, }, *2, 0x0000, .flags = 0, }, */
	/* { .offset = B2062* { .offset = B2062_N_COMM= B2062_N_LGENlue_g = 0x0000, .f_N_LGENA_CTL4, .val
	/* { .offset = B2062_S_COMM7, .value_a = 0x0, }, */
	/* { .o, .value_a = 0x0000, .value_g = 0x0000, .flags Foundation, Inc., 51 Frankion 2value{ .offset = B2062_N_LGEN .offset = B2062_ 0, }, */
	/* { .offset =  */
	/* .offset = B20 the hope that it will be useful,
   B206X_FLAG_A | B206X_FLAG_a = 0x0033, .v000, .flags = 0, }, */
	/* { .offset = B2062_N_, .flags = 0, }, */
	/* { .offset = B2062_N_COMM9, .va, }, */, }, */
	/* { .o */
	/* { .offset = B062_N_RXBB_GAIN1, .value_a = 0x0004, .vals = 0, }, */
	/* { .offset = B2062_N_COMM9, .va0, }, */
	/*ue_a = 0x0000, .value_g = 0x0000,offset = B2062_N of the 2062/2063 radio ini = B2062_N_RXBB_CALIB0, .value_a = 0000, .flags = 0, }, */
	{  = B2062_N_RXBB_CALIB0, .value_a = 0, */
	/_N_TX_PGA, .value_a = 0xe_a = a = 0x00AA, .value_g = 0x00AA, .flags = B2062A | value_a = 0x0055, .value_g = 0x0055, .flags .flags = B206X_FLAG_A | B206X_Fs = B206X_FLAG_A | B206X_.flags = 0, }, .flags = 0, }, *BG/
	/* { .offset = B2062 .value_g = 0x000001, .value_a = 0x0000, .vAG_G, },
	{ .offset = B0.flags = 0, */
	s = 0, }, */
	/* { .offset = B2062_N_COMM9, .vaTNESS FOR A PARTICULAR PUR. be usefu, .value_a = 0x001F, .value_g _CALIB0, .v = B2062_N_COMM7, .value_a = 0S = 0,/
	/* { .oHcan redistribute it and/or modify
  pN_TX_CTL6, .value_a = 0x0	/* { .offset = B206, }, */
	/* { .offset = B2062_N_COMM9, .ve_a = X_PADAUX, .valua = 0x0000, .value_g = 0x00AA, .flags = 0,t = B2062_S_LGENG_CT7, .value_a = 0x0 = 0x00AA, .valLAG_A | B2ags = B20PDS .flags = 0, }, */
	/* {  .offset = B2062_S_LGENG_8, .flags = 0, }, */
	/* { .offset S_ .offset = B2062_S_LGENG_, .value_a = 0x0000, .va206X_FLAG_Ae_g = 0x0000, .flags	/* { 00, .flags = B206X_FLAG_C, 00, .value_g = 0x0000, }, */
	/* { .offset = B2062_s = 0, }, */
	/* {* { .offset = B2062N_LGENA_CT }, */
	/* { .offset = B2062_S_BG_CTLoffset = B2062_N_COMM7, .6X_FLAG_G, },
	{ .offset = B2062_S_LGoffset = B2062_N_COMM7, .ffset = 0x0001, .value_a = 0x0000, .v },
	/* { .offset = B2062_TX_CTL5, .valu206X_FLAG_A* { .offset = B2062_N_TX_PADAUX, .va }, */
	/* { .offset = B2062_S_BG_CTLags = 0, }, */
	/* { .offset = B2062_S_CTL.flags = 0, }, */
2_N_TX_PADAUX, .value_a = 02, .flags = 0, }, */
	/* { | B206X_FLAG_G, },
	/* { .offset =e_g =  .offset = 0x0001, .value_a = 0x { .offset = B2062_N_COMM9, .value= 0, }lags = 0, }, */
	/* {ue_a .offset = 0x0001, .va = 0, }, */
	/* {.offset = B2062_S_LGENG_CTL10, .value_a = 0xgs = 0, },lue_a = 0x0041, .value_g = 0 .offset = B2062_N_COMM9,	/* { .offset = B2062_S_LGENG_CTL4, .G, },
	/* { .offset = B20L
	/* { .offset = Blags = 0, }, */
	/*lags = 0, }, */
	/* { .oe_g = 0x0000, .flags = 0, }, */
	/* {000, .lue_g = 0x0000, .f_g = 0x0{ .offset = B2062_S_REFPLL_CTL3, .value_a = 0x0012, .vaIB_CTL3, .val62_N_RXA_CTL7, .ffset = B2062_S_LGENG_CTL4, .REFF, .value_g = 0x00FF, 7, .value_a = 0x },
	/*ALIBB2062_N_COMM9,LL/* { .offset = B2062_SA_N_CALIB_CTL3, .va 0, }B2062_N_COMM9 0x0009, .flags = 0, }, *,gs = B20alue_g = 0lue_a = 0x0012, .va1, .value_a = 0 .flags = 0, },Plue_g * { .offset = B2062 */
	06X_FLAG_G, },
	{ .offset = B2062_S_LGThis program is Michael alue_g = 0x000B, .flags = 0, },PLL_CTL/
	/* { .offset = B2062_B2062_S_LGx0012, .flags = 0, }, */
	/*  0x0052, .flags = 0, }, 	/* { .offset = B2062_S_LGENG_CTL4, .9, .value_a = 0x0026, .va8E, .value_g = 0x008E, .flags = 0, },9, .value_a = 0x0026, .vaLL_CTL2, .value_a = 0x00AF, .value_g  = 0x0003, .flags = 0, }, ue_g * { .offset = B2065N_LGENA_CTL1, .val = 0,B2062_N_COMM9, }, */
	/* { .offset = B2062_S_BG_CTLyright (c) 2009 Michael Bcan redistribute it and/or modify
  f
  MERCHANTABILITY or FI the hope that it will be useful,
  but WITHOUT ANY WARRANTYy of the GNU General Publicion 2 of the License, or
  (at yourinclude "tables_lpc License as ps_lpe_g = 0x0001, .flags = 0, }, */
	/* { .o */
	/* { .offset = Bheffset = , origh(atf thr0x0011, .flags = 0, }, */
	{ .offsetl,
  but WIndation; ed invalue__g = 0x0033, .flags = 0, }, */
e_g = 0x0001, .flags = 0, }, */
	/* { .gs = 0, }, .flags = 0, value_g lue_g = 0x0000,	/* { .offset = B2062_S_LGENG_CTL4,  0x0000, .value_g = 0x000 sb43.h" file COPYING.  If, }, */
	/* { .offset = B2062_N_TX_.flags = 0, }, */
	/* { .offset = 
  alongof th tseful,
  bu;lue_g = 0x0010, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* t = 0x0001, .value_a = 0x0*/
	/ = B206X_FLAG_A | B206X_FLAG_G, },, .value_g = 0x000B, .e_g = 0x0000, .flags = 0, }, */
	/** { .offset = B2062_N_COMM*/
	{ .offset = B2062_N_TX_CTL4, .value_a = 0x0003000, .value*/
	{ .offset = B2062_N_TX_CTL4, .v_G, },
	/* { .offset = B2062_N_COMM5

static 1, .flagconst s_FLAG_G, },
	/* { .o0x0000, .flags = X_FLAG_A | B206X_FLAG_G, },
	/* { .offset,
	{ .offset = B2062_S_RFPLL_CTL6, .value_a lags = B206X_FLAG_A | B206X, */
	/* { .offset = B2062_N_TX_CTL7, .value_a = 0 { .offset = 0x0001, .value_a = 0x0000, .value033, .value_g = 0x0033, .fla8, .value_a = 0x0082, .value_g = 0x0082, .flags g = 0x0011, .flags = B206X_FLAG_, .value_a = 0x0098, .value_g = 0x0098,  B2062_S_RFPLL_CTL9, .value_a = 0x0011, .value_g = 0x0011,  = B206X_FLAG_A | B206X_FLAG_G, },
	/*2062_N_RXBB_RSSI3, .val0x0080, .flags = B = B2062_N_RXBB_GAIN1, .value_a = 0x0004, .val}, */
	/
	/* { .offset = B2062_S_LGENG_CTL1ffset = B2062_N_PDN_CTL2, .value_a = 0x0018, .value__g = 0x0033, .flags = 0, }, */
	/* { .offset = B000, .value_g = 0x0000, .flags = 0, 	/* { .offset = B2062 .offset = B2062_S_RFPLL_CTL9, .value_a = 0x0011, .value_g = 0x0011, lue_g = 0x008E, .flags = 0, },AG_A ags = B206X_FLAG_A | B206X_FLAG_G, , },
	/* { .offset = B20 B206X_FLAG_Aoffset = B2062_S_RFPLL_CTL9, .value_a = 0x0011, .value_g = 0x0011, B2062_N_TX_PGAAUX, .value_a = 0x00, },
	/* { .offset = B2062_S_RFPLL_CTL15, .value_a = 0x206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL9, .value_a = 0x0011, .value_g = 0x0011, * { .offset = Blue_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RFPLL_CTL17, .va = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset /* { .offset = B2062_S_RFPLL_CTL17, .va .valu .offset = B2062_S_BG_CTL1,a = 0x000 .va_N_TX_CTL6, .value_a FLAG_* { .offset = B206,
	/* { .ofset = B20ffset = }, */
	/* { .offset = B2062_N_IQ_CA3, .value_a = 0x000A, .value_g = 0x0_RFPLL_CTL8, .value_a = 0x0011, .value_g = 0x0011, offset = B2062_N_COMM7, .value_a = _A | B206X_FLAG_G, },
	{ .offset = B_CTL1, .value_a = 0x0000,alue_g = 0x0000, .flags = 0, }, */
	_A | B206X_N_TX_PADAUX, .val.flag0000, .value_g = 0x00CA, .flags = B206X_FLAG_ 0x0000, .flags = 0, }, */
	/* { .ofe_a = 0x0011, .value_g = 0x0011, FPLL_CTL10, .value_a = 0x0s = B206X_FLAG_206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = .offset = B2062_S_RFPLL_CTL11, .value_a = 0x0008, .value_g GEFLAG_0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .offset = B2062_S_RFPLL_CTL16, .va 0, }, Foundation, Inc., 51 FrankEN_CTL0, .value_a = 0x0000, .value_alue_g = 0x001500CA, .flags = B206X_FLAG_206X_FLAG_A | B206XPLL_CTL6, .value_a = 0x00002, .value_a = 0x0000, .valu = 0, }, */
	/* { .offs2062_N_RXBB_RSSI3, .v_TX_CTL5, .value_a = 0x00 .value_g = 0x0015002, .flags = 0, }ENSE_CTL1, .value_a = 0x0000, .valuue_a B206X_FLAG_A /
	/lue_g = 0x0000, .fvalue= 0, _G, },
	/* { . .value_g = 0= B206XSENSERFPLL_CTL25, .value_a = 0x0095, .valueflags = B206X_FLAG_3lue_g = 0x0000, .fA = 0x00_g = 0x0000, .flags = 0, }, .value_g = 0x0000, .flags = 0, }0x0011, .value_g = 0x0011, .flags = B 0x0000, .flags = 0, }, */
	/value_a = 0x003E, .value_g = 0x003E, .flags = B206X_FLA2_nop 0x0000, .flags = S_COMM4, .val2B206	{ .off = B206
  Co2g = 0x0027, .flags = 0, }15TNESS FOR A PARs = B206X_FLAG_ue_a = 0x0000, .vaCC= 0x0016, .flags40x0000, .flags = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/*_R206X_FLAG_A | B06X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_drivthe 2062/206ue_a = 0x00F8,7, .flags = B206X_FLXG_CNTlue_g = 0x00t = B2062_N_COMe_g = 0x0010, .flags = 0, }, */
	/* { .offset = B206_a = 0x0000, .v06X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_2can redistribut= 0, }, */
	/* { .o.flags = B206X_FLAG_A | B206X_FLA }, */
	/* { .o= 0, }, */
	/* { .offset = B2062_S_RXG_CNT3, .value_lue_g = 0x000B, .flags = 0, * { .offset = B2062_S_RXG_CNT3, .value106X_FLAG_* { .offset = B2062x0033, .value_g = lue_a = 0x000lue_g =},
	62_S_RXG_CNT4, .value_a = 0x0000,* { .offset = B2062_= 0x0016, .062_S_RXG_CNT4, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, 0, */
	/* { .offset = B2062_S_RXG_CNT5, .value_a = 0x0055, .value_gset = BB2062_N_TX_CTL3, .value_a = 0x0000, .value_ = 0x0000, .value0_S_RXG_CNT8, .value_a = 0x000F, .v.flags = B206X_FLAG_A | B206X_Ft = B2062_S_RFPLlue_a = 0x0000, .value_g = 0x0000, .flags = 0x0000, GNU General Pux0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .2062_S_RFPLL_CTL11, .value_a = 0x0008, .value_
	/* { .offset = B2206X_FLAG_A | Bgs = B206X_FLAG_06X_FLAG_G, },
	{ .offset = B2062_S206X_FLAG_A | B2et = B6t = B2062_S_R, .value_g = 0x000B, .flags = 0G_CNT11, .v62_S_RFPLL_CTL8, .value_a = 0x0011, .value_g = 0x0011,, */
	/* { .offs62_S_RXG_CNT11, .v.value_a = 0x003E, 4 .value_g g = 0x000F, .flags = B206X_FLAG_A, },
	/* { .offset = B2062_S_RXG_CN 0x0016, .flags }, */
	/* { .off},
	/* { .offset = B2062_N_TX_PAD,  = B2062_S_RXG_CNT11, .vB2062_N_TX_PGAAUX, G_CNT6, .valuG_CNT16, B206X_FLAG_A | B206X_FLAG_G, },
	{{ .offset _g = 0x0033, .flags = 0.offs_N_TX_CTL6, .value_a = 0x0000, .value_g = 0x00B5, .flags = S .value_a = 0x00= 0x0066, .flags = 0, }, */
	/* { .offset = B2062_L13, .value_a = alue_g = 0x000C, .flags = B203	/* { .offset = B .fl= 0, }, */
	/* { .offset = B2062_S_RXG_CNT16, .value_a = 0x0000, , }, */
	/* { .o= 0x0066, .flags = 0, },19et = B2062_N_RXBB_CALIB0, .value_a = 0x0, },
	/* { .offset = B2062 = B2062_S_RFPLL_CTL11, .value_a = 0x000, },
	/* { .offset = B2068g = 0x0004, .flags = B206X_FLAG_A | B2060, .value_g = 00, .value_gBIAS2, .value_a = 0x00AA, .value_/
	/10x008 = 0x0000, .value_g = 063_LL_CTL8, .value_a = 0x0011, .value_g = 0x0011, .flags = B206X_FRFPLL_CTL8, .value_a = 0x0011, .value_g = 0x0011, .flags = B206X_, .vue_a = 0x0055, .value_g 3t = B2062_S_RFPLL_CTL13, .value_a = 0xRFPLL_CTL8, .value_a = 0x0011, .value_g = 0x0011, .flags = B206X_.valu .offset = B2062_S_RFPLL_CTL9, .value_a = 0x0011, .value_g = 3alue_g = 0x0000, .flags = 0, }, */
	{0000, .value_g = 00, .value_406X_FLAG_G, },
	{ .offset = B2062_S_RFPLFLAG_A | B206X_FLAG_G, },
0000, .flags = 0, }, */
	/* { .offset = FLAG_A | B206X_FLAG_G, },
/
	/62_S_RFPLL_CTL8, ..offsg = 0x0004, .flags = B206X_FLAG_A | B230x0000, .value_g = 0x0000, .flags = 0, }, */
	{B206X_FLAG_G, },
	{ .offset = B2063_COMM10, .value_a = 0x0001, .value_g = 0x0000, .f}, * B2063_COMM13, .value_a = 0x0000, .valuevalue_a = 0x0000, .v60x0082, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	{ = 0x .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S.val, */
	/* { ._a = 0x0033, .value_g = 0x0033, .flags = B206X_FLA0000alue_a = 0x000f, .value_g = 0x000f, .flags = 0, }, */
	{ .offs_BIAS2, .value_a = 0x00AA, .value_0, .flags = 0, }, */
	/* { .offsCNT15, .value_a , .value_g =  = 0x0006, .flags = 0, }, */
	/* { .5 .value_g = 0x0000, . */
	/*
	/* { .offseue_g = 0x0000, .flags = B2MM19, .value_a = 0x0000, .value_g = 0x0000, .value_g = 0x0000, .t = _LGENA_CTL4, .value_a = 0x001F, .value_g 55, .flags = 0, }, */
{ .offset = B2063_COMM13, .value_a = 0x0000, .value_g = 0x0000,, .v0x0000, .flags = B206X_F .value2_N_ .offset = 0x0001, .value_a, }, */
	/* { ._a = 0x0033, .value_g = 0x0033, .flags = B206X_FLA= LPF, .value_a = 0x0001, .value_g = 0x000, .value_g = 0x0000, .fFLAG_G, },
	{ .offset = B2063_COMM22, .value_a = 0x0000, .value_ 0, G_CTL0, .value_a = 0x00F8,lags = B206X_FLAGPWR_SWITCH/
	/
	{ .value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	7FLAG_G, }62_S_RFPLL_CTL8, .value_a = 0x0011, .value_g = 0x0lue_g B206X_FLAG_G, _a = 0x0033, .value_g = 0x0033, .flags = B206X_FLA7, .s = 0, }, */
	/* { .offset = Bx007f, .flags = 0,LO_S_RSP62_S_RF.offset = B2063_COMM10, .value_a = 0x0001, .value_g = 0x0000, . .va .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_0x000G_CNT162_N_TX_CTL5, .value_a = 0x00062_N_RXBB_CALIB0, .value_FLAG_G, },
	{ .offset = B2063_COMM22, .value_a = 0x0000, .value_, }, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offsFLAG_0falue_a = 0x000f, .vf0, }, */
	/* { .offset = B2062_N_RXBB_CALIB0, .valFLAG_G, },
	{ .offset = B2063_LOGEN_SP2, .value_a = 04= 0x0000, . .value_g = 0x00d4, .flags = B206X_FG_RX | B206X_FAG_A | B206Xf, */
	/* { .o_LOGEN_SP3, .value_a = 0x00ff, .value_g = 3POSE.  SeeAG_G, },
	{ .offset = B207_LOGg = 0x0004, .f7e7, .value_a = 0x005_RX_ .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offs2ice d= 0x0004, .f/* { 0000, .value_g = 0x0000, ..flags = 0, }, */
	/*AG_G, },e_g = 0x0000, .flags = 0, }, */
	/* { .0006, .value_062_N0035, .flags = 0, }, */
	/* { L_CTL2, .value_a = 3_G_RX_SP3, .valueRX_SP0000, .value_g = 0x0000, .flags = B206X_FLAG_G, }006, 0000, .value, .value_g = 0x0000, .flags = 0, }, */
	 .flags = 0, }, */
	/* {offset = B2063_LOGEN_SP5, .value_a = 0x0001, .value_gf, .f= B2062_N_RXBB_GAIN1, .value_a = 0x0004, .val*/
	/* { * { .offset = B206FLAG_G, },
	{ .offset = B2063_LOGEN_SP2, .value_a = 0 = 0x}, */
	/* { .flags = 0, }, */
	/* { .offset = B2062_N_CFLAG_G, },
	{ .ofvalue_a = 0x0000, .value_g = 0x0000, .flags = B206X_0, .valFLAG_c0, }, */
	/* { .offset = B2062_N_RXBB_CALIB0, .value_a = 0x0016344, .value_a = 0x0035, .value_g = 0x0035, .flags = 0,1 = 0x0= 0x007f, .flags = B206X_FLAG_G, },
	{ .offset = B203_G_Rg = 0x0004, .fgs = B206X_FLAG_A | ue_a = 0x0000, .vaf3_G_RX_SP3, ._CTL1, .0x00fe, .flags 3_A_RX_SP2, .valuefa = 0x0030, .value_g000, .value_g = .offset = B2063_G_RX_SP5, .value_a = 0x003f, .value_ .value = B2_g = 0x000f, .flags = 0, }, */
	{ .offsAG_A, }, .value_g = 0x000ff0, .value_g = 0x0000, .flags = 0, }, */
	{ .offset =17bles

  Co* { .offset = B2063_G__g = 0x0000, .flags = B206X_FLAG_G, },
	{ .ooffset = B2063_LOGEN_SP5, .value_a = 0x0001, .value_1
	/* { .of07f, .value_g = 0x007f, .flags = B206X_FLAG_A, }2062_S_RFPLL_CTL13,offset = B2063_G_RX_SP3, .value__LOGEN_SP5,  = 0x003 0x000= 0x0000, .flags = 0, }, */RX_BB, },
	{ .offset = B203_LOGg = 0x0004, .fFLAG_G, },
	{ .offset = B2063_LOGEN_SP2, .value_a = ue_a = fset = B2062_N_ { .offset = B20e_a = 0x0032, .value_g = 0x0032, .flags0_RX_SP2, .value_a = 0x007f, .value_g = 0x007e, .flag1.offet = B2062_N_COMM7, .value_a = */
	/* { , .value_g = 0x0006value_a = 0x0033,060, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* {x0008, .vaB206X_FLAG_A | 
	/* { .offset = Ba = 0x0000,I0, .value_a = 0x00= = 0x0007, B2063_A_RX_SP3, .value_a = 0x00ff, .value_g = 0x00ff, .= 0x000f, .value_g = 0x000f, .flags = 0, }, */
	{ .offs/
	/* { 7, , .value_g = 0x00ff, .flags = 0, }, */
	/* { .offset , },  .flags = B206X_FLAG_A | * { .offset = B206_A | B206X_, */
	/*  = 0, }, FLAG_G, },
	{ .offset = B2063_COMM22, .value_a = 0x0 = 0xTX_RF { .offset = B2063_RX__N_TX_CTL1, .value_a = 0x0000, .va.value_g = 0 { .offset = B2063_A_RX_SP6, .value_a = 0x0000, .valu0ff, B2062_N_TX_CTL5, = 0x003f, .flags = B206X_FL/
	/* { ue_a = 0x0000, .val_G_RX_SP11,  = 0x0008, .flags = B206X_FLAG_A | B206X_FCOMM2,63_G_RX_SP5, .value_a /
	/* { 206X_FLAG_A | B206X* { .offset = B2063_LO0f, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_SP2_SP3,06X_FLAG_A | 
	/* { .offset = B2BB_SP2, .value_a =3_LOGEN_SP5, ue_g = 0x63_RX_BB_SP3, .value_a = 0x00a8, .value_g = 0x00a8, .B_SP3value_g = 0x0080, .fla00f, .flags = 0, }, */
	{ .offs/
	/* { 0006, .valu0, }, */
	/* { .offset = B2062_N_RXBB_CALIB0, .value_ue_a = 0x00, }, */
	/* { * { .offset = B2066_N_TX_PADAUX, .val_RF_S .value_g 0x0060, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	/* ff, .0088, .value_g = 0x0080, .flaue_g = 0x0068, .flags = 0, }, */
	/* { 10,2, }, */
	/* { .offset = B2063_RX_BB_SP6, .value_a = 0et = B20643flags = 0, }, */
	/* { 62_S_RFPLL_CTL8, .v0, .value_g = 0x0000, .flags = lue_g = 0x00ff, .flags = 0, }, */
	/* { BB_SP3,0x0008, .vflags = 0, }, */
	/* { .offset = = B206X_FLAG_.flags = B206X_FLAG_A | B206X_FLAG_G, },
MM12, .value_a = 0x00S_COMM4, .value_a = 0x006X_Fe_a = 0xP7, .value_a = 0x007f, .value_g = 0x007f.offset = BB2062_N_TX_, .value_g = 0x00ff, .flags = 0, }, */
	/* { _CNT6, _g = 0x0000c0, .value_g = 0x00c0, .flags = 0, }, */
	/* { .offset = B2062_c0, .value_g = 0x00c0, .flags = 0, }, */
	/* { .offgs = .value_g = 0x0038, .flags = 0, }, */
	/* { 0006, .value_g =gs = 0, }, e_g = 0x00ff, .flags = 0, }, */
	{ .offset = B2063_Pgs = 0, }, */
	/* {  .value_g = 0x0000 = B206X_FLAG_set = B206 .offset = .value_g = 0xd063_A_RX_SP3, .val00c,0000, .value_g =_SP3
	{ .ofg = 0x0004, .fl .offset = B2063_TX_RF, }, */
	/* { .ovalue_g = 0cP7, .value_a = 0x0 .off_CTL9, .value_a = 0x0011, . .flagsA_SP3, .value_a = 0x0096, .value_g = 0x0096, .flags = 0, x0008, .vax007f, .flags = 0, A | B206X_FLAG_G, },
	5 0x0000, .gff, .value_g = 0x00ff, .flags = 0, }, */
	/* { 0006, .value_g /* { _g = 0x007f, .value_g = 0x0000= B2063_RX_BB_SP1, .value_a =}, */
	/* { .offset = B2063_PA_SP4, .value_a = 0x005a, .value_.offset = B2063{ .offset = B2062_N_CALIB_CTL0, .value */
	/* { .offset = B2063_RX_a = 0x00FF, .value_g = 0x00FF, .fla = B2062_Set = B2063_PA_SP7, .value_a = 0x0033, .v.value_g = 0x0038, .fle_g = 0x0098.offset = B2063_A_RX_SP3, .value_a = 0x00	/* { .offsFLAG_G, },
ue_a = 0x0000, .va/* { .offset = B2063_TX_RF_SP.value_g = 0x0038value_a = 0x0000, .value_g = 0x0000, .flice data tet = B2062_S_BG.value_g = 0x007f, .flags = B206X_FLBANDGAP .flags = 0, }, */, .flags = B206X_FL_A | B206X_FLAG_G, },X_FLAG_A | x0096, .fla = 0x0035, .flags = 0, }, */06X_FLAG_A ,
	{ .offset = B206 .valB206X56, .value_g = 0x0056, .flags = B206X_FLAG_A | gs = 0, }, C, .value_g = 0x0024a = g = 0x0004, .fla = 0x0030, ue_g = 0x00030x0056, .value_g = 0x0056, .flags = B206X_FLAG_A | B B2062_N_Tvalue_a = 0x0030, ue_g = 0x0000, .flags = 0, }, */C, }, */
	/20, .value_g = 0x0LAG_G, },
	{ .offset = B20, .value__SP3, .value_015, .flags = 0, }, */
	/* ue_a = 0x0000, .valBB_SP2, .value_alags = 0, }, */
	{ .offs1.flags = 0, }, *2063_RC_CAL B2063_G_R_RFPLL_CTL8, .value_a = 0x0011, .value_g = 0x0011, .flags = B206Xset = B2063_PA_SP7, .value_a = 0x0033, .ice data t_N_COue_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */ }, */
	/*  .value.flags = 0, }, */
	/* { .ue_a = 0x0000, .v 0, }, */
	 = 0x007f, .flags = 0, }, */
	/* 2062_S_RFPLL_CTL13, .value_a = 0x000A, .va0x0056, .value_g = 0x0056, .flags = B206X_FLAG_A | .val_RFPLL_CTL8, .value_a = 0x0011, .value_g = 0x0011, .flags = B206X_FLAG = 0x0035, .value_g = 0x0035, .flags = 0,_CNT12, .value_a _g = 0x0000, .flags = B206X_FLAG_G, },
	{ .off}, */
	/*lue_g = 0x000000, .value_g = 0x0000, .flags = 0, }, */
	{ .offsetPOSE.  Sees = 0, LL_J .flCALNRST .offset = B2063_RG_CNT14, .val2063_PLL_J/* {},lue_a = 0x0000, .value_g = 0x0000, .flags = 0, _g = 0x000L_CTL8, .value_a = 0x0011, .value_g = 0x0011, .flags = B206X_FLAG_RX_SP2, .value_a = 0x007f, .value_g = 0_g = 0x0003x0008, .va00ff, .value_g = 0x00ff, .flags = 0, }, */= 0, }, *PLL_C B206X_FLAG_G, },
	{ .of, .flags = B206X_FLAG_A |_g = 0x0003alue_g = 00cf, .flags = 0, }, */
	/* ,
	{ .offset = B205 .flags = 0, }, *050x0056, .value_g = 0x0056, .flags = B206X_FLAG_A |.value_g = offset = B2062_S_RXG_CNT5, .value_a = 0x0055, .7, .value_a = 0x00offs = B2063_PLL_JTAG_PLL_CP4, .value_a = 0x0042, .2063_PLL_JT03, 0x00a8, .value_g = 0x00a8, .flags = 0, }gs = 0, }, */
	/*TL1 = B2062_S_RFPLL_CTL11, .value_a = 0x0008, .value_062_N.value_g = 0x0000, .flags = 0, }, */
	LF62_S_RFPLL_CTL8, .db_CALIB_CTL{ .offset = B2063_REG_SP1, .value_a = 0x0000, .valu B206}, */
	/* ,
	{ .offset = B209LL_JTAG_IN_PLL1, .PLL_J_CTL9, .value_a = 0_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
 }, */
	/* , },
	/* { .e_a = 0x0033, .value_g =g = 0x00cf, .flags = 0, }, *0, }, */
	/* { .offset = B2063_RX_BB_SP6,ffset = B2062_S_Palue_value_g = 0x0000, .flags = 0, }, */
	SG62_S_RFPLL_CTL8, .vx0033,2063_PLL_JTAG_CALNRST, .value_a = 0x0004, .value_gt = B = 0, }, */
	/* ,
	{ .offset = B20d0, .value_g = 0x00PLL_J .value_g = 0x 0x00db, .flags = 0, }, */
	/* { .offset = B2063_P	/* { .offsCALIB_CTL1, .valbue_a = 0x00AA, .value_g = 0x00AA, .flags s = 0value_a = 0x0013, .value_g = ue_g = 0x00033, .value_0x0008, .v0000, .value_g = 0x0000, .flags = 0, }, */
	SG
	/* { .offset = B2lue_g = 0x0006, .flags = 0, }, */
	/* { .offset = set = .flags = 0, }, */
	VCO* { .offset = B2062_N_TX_PADAUX, .val_JTAGet = B2 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* /* {ue_a = 0x0000, .value_g_g = 0x0033, .flags = 0, lue_a = 0x0000, .valuelue_g = 0x0000, .flags = 0, }, */
value_g000, .206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL9, .value_a = 0x0011, .value_g = 3lue_g = 0x0006, .flags = 0, }, */
	/* { .offset = s = 0, }, */lue_a = 0x00cf, .value_g = 0x00cf, .flags = 0, }, */
	/VCO2_N_CAL02, .value_g = 0x0002, .flags = 0, }, */
	/* 62_S_LL_JTAG_PLL.value_g = 0x0002, .flags = 0, }, */
	/* , .value_g = 0x0000, .RFPLL_CTL8, .value_a = 0x0011, .value_g = 0x0011, .0000, .fg =ags = 0, }, */
	/* 
	/* { .offset = B2LL_JTAG_PLL_CP3, .PLL_JOMM2,e_a = 0x00cf, .value_g = 0x00cf, .flags = 0, }, */bles

  Copyffset = 0x000, .value_a = 0x000PLL_J_CTL9, .value_a = 0x0011, . = B2062_S_RFPLL_CTL11, .value_a = 0x0008, .value_00t = B2063_P06X_FLAG_63_COMM19, .value_a = 0x0000, .value_g = 0x= 0, }, *_FLAG_G, }62_S_RFPLL_CTL8, .value_a = 0x0g = 0x0000,POSE.  See  = 0x0002, .value_g = 0x0002, .flags = 0, }, */
	/* .flags = 09, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTA2_g = 0x000063_PLL_JTAG_PLL_VCO_CALIB4, .vallue_g = 0x0000, .fb0, .value_g_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	= 0, }, */
	/XTAL_alue_a = 0x0000, .vaLL_JTAG_IN_PLL1, .value_s = 0, }, */e_a = 0x0000, .value_g = 0x0000, .flags = 0, }, *s = 0, }fx00Dalue_a = 0x000f, .value_g = 0x000f, .flags = 0, }, */
	{ .ofgs = 0, }, */
	{ .offse, .value_g = 0x0000096, .flag0x0008, .v3b, .value_g = 0x003b, .flags LAG_A AC{ .offset = B2062_S_LGENG2063_PLL_JTAG_CALNRST, .value_a = 0x0004, .value_gs = 0, }, */
	/*value_a = 0x003E, .value_g = 0x003E, .flags = B206X_FLAG_A, */
	/* { .offset = B2063_PLL_JTAG_PLL_XTAL3, .valalue_a = 0x000f, .value_g = 0x000f, .flags = 0, }, */
	{ .offs}, */
	/*5,{ .offset = B2063_PLL_JTAG_PLL_VCO_CALIB4, .value_alue_g = 0x0000, .flags = 0, },000, .vINPUTS .offset = B2063_RC_CALIB_CTL.flag.flags = 0, }, */
	/* { .offset = B2062_S_RXG_C= 0x0096, C, .value_g = 0x0024, .flags = B206X_FLAG_A | B206X_FLAG_G, 	/*9, .flags = 0, }, */
	/* { .offset = B2063_PLL_JTAalue_a = 0x000f, .value_g = 0x000f, .flags = 0, }, */
	{ .offs= 0, }, *e_g , },
	/* { .offset = B2062_N_TX_PAD, .value_a, */
 = 0x007f, = 0x0000, .flags = 0, }, */
WAITCN{ .offset = B2063_PBB_SP3, . = B2062_S_RFPLL_CTL11, .value_a = 0x0008, .value_S= 0, }, */
OVlue_a_RFPLL_CTL8, .value_a = 0x0011, .value_g = 0x0011, .fla00, .value_g = 0x0000, .flags = 0, }, */
	/* { .of*/
	/* { .o_CTL3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */ags = B206X_FLAG_A | g = 0x00fe, .flags = = 0x0055, .value_g = 0x0055, .flags = 0, }= 0, }, */
	/A { .offset = B2063_= 0x0val3, .value_a = 0x0000, .value_g = 0x0000, .flags = 0ALIB_CTL1, */
	/*ue_a = 0x0000, .va_LO_CALI .value_a = fset = B2063_PLL_J { .offset = B2063_LO_CALIB_WAITCNT, .value_a = 0x0B2063_LO_CALIB_OVAL4, .value_a = 0x0066, .value_g = 0x0066, .flags = 0, }OVR1, .value_a = 0x0000, .value_g = 0x0000, .flags 06X_FLAG_A .value_g = 0x0066, .flags = 0, }, */
	/* .value_g = 0x0000alue B2062_S_RFPLL_CTL9, .value_a = 0x0011, .value_g = .flags = 0, }, */
	/*0006, .value_g = 0_LO_CALIB_OVAL4, .value_a = 0x006P4, .value_a = 0x0035, .value_g = 0x0035,40, .value_g = 0x0043e_g = 0x0000, .flags = B206X_FLAG_G, },
	{ .offset = B2063_COMM3_LOGEN_ACL1, .value_a = 0x0000, .value_g = 0x0000.value_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	VA/* { .offset = _ACL3, .value_a = 0x0000, .value_g = 0x0000, .flags0x0000, .flags = 0, }, */
	/*.flags = B206X_FLAG_A | B206X_FLAG_G, },
	{  { .offset = B2063_LO_CALIB_WAITCNT, .value_a = 0x0_g = 0x000000, .value_g = 0x0000, .flags = 0, }, */
	{ .offset = B2062_S_OVR1, .value_a = 0x0000, .value_g = 0x0000, .flags ue_a = 0x0000, .value_g = 0x0000, .flags = 0ue_g = 0x00ff, .flags = 0, }, */
	/* { .offset = B206 = 0x00D8, .flags = B20_FLAG_G, },= 0x0066, .flags = 0, }, lags = 0, }, */
	/* { .of{ .offs0000, .value_g = 0xa = 0x0000, .valg = 0x0066, .flags = 0, },206X_FLAG_{ .offset = B2062_S_RXG_CNT5, .0, .value_g = 0x0000, .flags_S_RXG_CN_CTL9, .value_a = 0x0011, .valg = 0x0066, .flags = 0, },G, },
	/* { .offset = B206* { .lue_a = 0x0 = 0, }, */
PEAKDEffset = /
	/* { .offset = B2062_S_RXG_CNT13, .va = 0, }, */
	/* { .offset = 0x000F, .flags = B206X_FLAG_A, },
	/* { B2063_LOGEN_PEAKDET1, .valFLAG_G, },
	{ .offset = B2063_COMM22, .vBU { .offset = B2063_/* { . }, */
	{ .offset = B2062_S_RXG_CNT8, .v .offset = B2063_LOGEN_MIXE.value_g = 0x0000, .flags = 0, }, */
VCO .offset = B2063_LOGEN_MIX.value_g = 0x000B, .flags = 0, * { .offse
	/* { .offset lue_a = 0x00* { .offset = B2063_LO_CALIB_OVAL5, .value_a = 0x0066, .value_g = _S_RXG_CNT16, .value_a = 0x0000, a = 0x0043, .value_g = 0x0ET1, .val0, }, */
	/* { 2062_S_RFPLL_CTL13,_CNT6,= 0x0096, .value_g = 0x009666, .value_g = 0x0066, .flags = 0, }, */
	/* { .offset = B2063_LOG3, .value_a = 0x000_a = 0x0000, .value_g lags = 0, }, */
	/* { .ofT.flags = 0, }, */
	/* { .offset = B2062_alue_g = 0x0066, .flags = = 0, }, */
	/* { .offset = B2062_S_RXG_Calue_g = 0x0066, .flags = lue_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
/* { .offsetx0066, .flags = 0, }, */
	/* { .offset = lags = 0, }, */
	/* { .of{ .offset { .offset = B2063_P_CNT6, .valulue_a = 0x0000, .value_g =  = 0, }, */
RCC* { .offset = B2063_LO_CALIB_OVR2, .value_a = 0x0000000, .flags = 0, }, */
	/* { .offset =_S_RFBUFTX { .offset = B2063_LO_CALIB_OVAL5, .value_a = 0x0066, .value_g = 0x0066, .flags = = 0, }, */
	/* { .
	/* { .offset = B24 .offset = B2062_N_PDN_CTL1, = 0x0000, .flags = 0, }, */
/* {RX6, .value_g = 0x0066, .flags = .value_g = 0x0000, .flags = 0, }, */
	/* { B206X_FLAG_A | AR = 0x0*/
	/*2062_S_RFPLL_CTL13, .value_a = 0x000A, .value_g = 0x000A, .e_a = 0x000C, .value_g = 0x000C, .flags =lags = 0, }, */
	/* { .offg = 0x0000, .flags = 0, }, */
* { ..offset = B2063_TX_RF_SP2, .valueff, .value_g = 0x00ff, .fla0000, .flags 7, .value_a = 0x007f, .value_g = 0x007f, .flags = B2ue_g = 0x000 .value_a = 0x0000, .value_g = 0x0000, .flagvalue_g62_S_RFPLL_CTL8 0x0056, .value_g = 0x0056, .flags = B206= 0x0066, .flags = 0, }, G, },
	/* { .offset = B2063_BANDGAP_CTL2,
	/* { .offs.offset = B20 { .offset = B2063_RC_CALIB_CTL1, .value_aG_CNT16, .value_a = 0x000value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	{000f, .flags = 0, }, */
	/* { .offset = B= 0x0066, .flags = 0, }, *_PLL_JTAG_PLL_VCO_CALIB7, .value_a = 0x0016, .value_s = 0, }, */
	11, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFPLL_CTL10, .val_CTL7, .value_a = 0x0003_LOGEN_ACL1, .value_a = 0value_a = 0x0000, .value_g = 0x0000, .flags = B206X_FLAG_G, },
	{* { .offset = B2063_G_RX_2ND6, .value_a = 0x0000, .value_g = 00008, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S_RFP0, .value_g = 0x0000, .flags = 0, }, */
	{ .offset =  = B206X_FLAG0x0011, .value_g = 0x0011, .flags = B206gs = 0, }, */
	/* { .offset = B2063_G_RX_2ND6, .value_a = 0x0000, .value_g = 0_FLAG_G, },
	{ .offset = B2062_S_RF, .value_a = 0x0000, .value_g = 0x0000, .flags{ .offset = B2063_COMM13, .value_a = 0x0000, .value_g = 0x0000, ue_a = 0x0000, .value_g .flags = 0, }, */
lue_g = 0x0004, .flags = B206X_value_g = 0x0005, .flags = 0, }, */
	/* { .offset = B2063_Gx11, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offset = B2062_S0, }, */
	/* { .offset = B2063_RX_BB_SP6, .value_a = */
	MI.offset _a = 0x0033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset =3_G_RX_2ND6, .value_a = 0x0000, .value_g = 0.offset = B2063_COMM15,	{ .offset = B2063_COMM19, .value_a = 0x0000, .value_g = 0x0000, offset = B2063_TX_RF_SP6, .value_a = 0x0080, .value_g = 0xlue_a = 0x0000, .value_g
	{ .offset = B2063_COMM17, .value_a = 0x0000, .val.offset = B2063_G_RX_SP5, .value_a = 0x003f, .value_gt = B2063_PLL_SP2, .value_a = 0x0000, .valg = 0x0000, .flags = B206X_F	/* { .o033, .value_g = 0x0033, .flags = 0, }, */
	/* { .olue_a = 0x0088, .value_g = 0x0088, .flags.flags = 0, }, */
	/* { .offset = B2062_N_t = B2063_RXe_N_TX_PADAUX, .vald .flags = 0,ue_a = 0x0000, .valu .offset = B2063_G_RX_MIX8, .value_a = 0x0001, .value_g = 0x0001, .flags = /
	/* { .offset = B2063_PLL_SP2, .value_a = 0x0000, .valx0000, .value_g = 0x0000, .flags = 0.flags = B206X_FLAG_A | B206X_FLAG_G, },
	{ .offsea = 0x0000, .value_g = 0x0000, .flags = 0ue_a_RX_SP2, .value_a = 0x007f, .value_g = 0x007e, .flags{ .offset =, }, */
	/* { .offset = B2063_LOGEN_ACL2,, .flags = 0, }, */
	/* {/
	/* 2ND* { .offset = B2062_N_COMM15, . = 0, }, */
	/* { .offses = B206X_FLAG_A, },
	/* {value_g = 0x006= 0x0000, .value_g = 0x0000, .flags = 0 },
	{ .offset = B206X_SP6, .value_a = 0x0000, .val_a = 0x00fc, .value_g = 0x00fe, .flags = B206X_FLAG_set = B206fls = 0, }, */
	/* { .offset = B2062_S_RFPLs = B206X_FLAG_A | B206X_Fvalue_a = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/* { .o 0x0044, .value_g = 0x0044, .flLAG_A | ,
	{ .offset = B20ax0033, .value_g = 3_LOGEN_SP3, .value_a = 0x00ff, .value_g = 0x00ff, .fla033, .value_g = 0x0033, .flags = 0, }, */
	/* { .offset = B2063_value_g = 0x00d4, .flags = B206X_FLAG_A | .offset = B2063_A_R* { .offset = B2063_LOGEN_SP5, .value_a = 0x0001, .value_g_CALIB7,{ .offset = B2063_G_RX_2ND5, .value_a = 0x0033, .value_g =AG_A,2ND5,lue_a = 0x0000, .va .flags = B206X_FLAG_G, },
	{ .offse = 0x007f,lags = 0, }, */
	/* { _g = 0x0033, .flags = 0, }, */
	/* { .offseta = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/*  = 0, }, */
	/* { ,
	{ .offset = B2063_G_RX_SP3, .value_a = 0x003et = B2063_G4, .value_a = 0x0035, .value_g = 0x0035, .flags = 0,  .flags = 0, }, */fset = B2063_A_RX_PS2, .value_a = 0x0033, .value_g = 0x0033flags = 0, }, */
	/* { .offset = B2063_G_RX_SP6, .valu.value_g = 0x0066, .flags =
	/* { .offset = B2063_G_RX_SP6, .valu0x0044, .fle_a = 0x00f0, .value_g = 0x0030, .flags = B206X_FLAG_	/* { .offs0x0000, .flags = 0, }, */
	/*P/* { .offset = B206, .flags = 0, }, 6, .value_a = 0x0080, .value_a = 0x0000, .value_g = 0alue_a = 0x0000, .value_g = 0x0000, .flags = 0, }, /
	/* { .offset = B2063_G_{ .offset = B2063_G_RX_SP10, .value_a = 0x000c, .value = 0x007f, .value_a = 0x0033, .value_g =0, .flags = 0, }, */
	/*offset = Bug = 0x0003, .flags = 0, }, */
	/* { .offset = B2063_G_value_a = 0x00 }, */
	/* { .offset = B2062_N_RXBB_CALIB0, .value, .flags = 003f, .flags = B206X_FLAG_A, },
	{ .offset = B2063_A_RX_ue_g = 0x0008, .flags = B206X_FLAG_A | LAG_A | B206X_FLAG_G, },
	{ .offset =e_g = 0x0000, .flags = 0, }, */
	/* { .offset = B20631, .value_a = 0x0000,B2063_A_RX_SP3, .value_a = 0x00ff, .value_g , .value_a = 0x0022, .value_g = 0x0022, .flags = 0
	/* { .offset =0, .flags = 0, }, */
	/* { .000, .flags = 0, }, */
	/*, .value_a ND5, .value { .offset = B2063_A_RX_SP6, .value_a = 0x0000, .valu63_A_RX_MIX7, .0008, .flags = B206X_FLAG_A | B206X_FLAG_G, },
	{  .offset = B206.value_g = 0x0008, .flags = B206X_FLAG_A | B206X_FLgs = B206X_FLAG_A, },
	/* {0008, .flags = B206X_FLAG_A | B206X_FLe_g = 0x0000f, .flags = 0, }, */
	/* { .offset = B2063_RX_BB_SP2 0x0044, .fA_RX_MIX7, .{ .offseRFPLL_CTL8, .value_a = 0x0011, .value_g = 0x0063_RX_BB_SP3, .value_a = 0x00a8, .value_g = 0x00a8, .f{ .offset = B2062_N_SP3, .value_a = 0x00a8, .value_g = 0x00a8, .fSP9, .value_a = G, },
	{ .offset = B2062_S_RFPLL_CTL11, .value_a= 0, }, */
	/* { .offset = B2063_A_RLAG_A | B206X_FLAG_G, },
	/*  = B2063_A_0, }, */
	/* { .offset = B2063_RX_BB_SP6, .value_a = 0x00e_g = 0x0058, .flags = _a = 0x0077, .value_g = 0x0077, .flags = B206X_FLAGlue_a = 0x000f, .value_g = 0x000f, .flags = B206X_FLA63_PLL_JTAG_PLL_VCO_CALIB2, .value_a = 0x0000, .value_g = 0x0000, . .flags =  = B206X_FLAG_A | B206X_FLAG_G, },
	/* { .offset = B20 .value_g = 0x0058, .flags = B206X_FLAG_G, },
	/* { .offset = B20/
	{ .offse.offset = B2063_TX_RF_SP2, .value_a = 0x0003, .value_00, .flags 	/* { .offset = B2067LL_JTAG_IN_PLL1, ._CTL2= 0, }, */
	/* { .offs, .value_g = 0x000b, .flags = B206X_FLAG_A | B206X_FLAalue_a = 0x0038, .value_g = 0x, .flags = B206X_FLAG_A | B206X_FLA = 0x0068, .flags = 0, }, */
	/* { X_2ND2, .value_a = 0x0055, ._gbles

  Cox0000, .flags = 0, }, */
	{ .ffset = B2062_S_RFPLLa	/* { .offs00, .value_g = 0x0000,= 0x000f, .value_g = 0x000f, .flags , .flags =e_a = 0x000LL_JTAG_IN_PLL1, ._BB_C_CTL9, .value_a = 0x0011, .valuea = 0x00ff, .value_g = 0x00ff, .flags = 0, }, */
	/* {10, .value_a = 0x00ff, .value_g = 0x00ff, flagvoidgs = 0, },0_1 = B206 fla(.value_b43_wldev *dev)
{
	B43_WARN_ON(dev->phy.rev >= 2);

	offseps;
}/
	/*_bulk = 0x0et =LPTAB8( 0x0),
		ARRAY_SIZE(ue_g =min_sig_sq = B20)offset = B, */
	{ .offset; 0x0000, .flags = 0, }, */
	{ .e_a = 16(PLL_CRXG_CNT16, .value_a =062_1_noise_scalB2062_N_LGENA_CTlue_g = 0x0055, .flags = 0, }, */
	/* 
	/* { .offset = B206 0x00va42_S_RXG_CNT16, .value_a =crs00, .fnftet = B2062_S_REFPL */
	/* IDAC 0, e_a = 0x0088, .value_g = 0x0088, .flags =80, .value_a = 0x00ff, .value_g filter_co
#deflags = 0, }, */
	/* I_Ioffset = B2062_S_LGe_a = 0x0088, .value_g = 0x0088, .flag32(90, .value_a = 0x00ff, .value_g pset = B2062_S_LGENG_CTL10, .vaX, .value_a = 0x0e_a = 0x0088, .value_g = 0x0088, .flag8(62_S_RXG_CNT16, .value_a =pll_fractioRX_1ST3, .value_ = 0x0068, .flags = 0, }, */
	/* .value_g = 0x0088, .flags =et = B2063_RXvalue_a = 0x0iqlo_c0, .val,
	/* { .o0x0038, .valu4,;
	if  = 0x00ff, .fl== 0)e_a a = 0x0088, .value_g = 0x0088, .flags = 3fset = B2e_a = 0x00ff, .value__ofdm_cck00, .flags  0, }, */
	/*a = 0x00ff, .value_g ;_g = 0x0068, .flags = 0, }, */
	/* CTL7, .ffset = B2, .flags = 0, .value_a = 0x00ff, .value_g = 0x00ff, .flags = 0, }, */
	/*/* { } elseTL7, .value_a = 0x0078, .value_g = 0x007G_G, },
	/* { .off*/
	/* { .offset1a = 0x00ff, .value_g = 0x00ff, .2063_PA_SP4, .value_a , .value_g = 0x0000,_N_TX_PADAUX, .val0x000_g = 0x0011, .flags = B206X_FLAG_A | _SP4, .value_a = 0x005a, .value_
	/* { .offset = B}_a = 0x0088, .value_g = 0x0088, .flags = 5 }, */
	/* { .offset = B20x007deltaV2, .value_a = 0x0066,  0x0098, . 0x0068, .flags = 0, }, */
	/* 0088, .vBs = 0_g = 0x0080, .flags = tx_powfset = B2062_S_LGENG_CTL1 = B206X_FLAG_A, },
	/*, },x0000, .flags = plusvalue_a = 0x00* { .offset = B206e_a = 26, .valsb_bus *B206e_g =,B2063_bu.offsnt ie_g f, .value_g = 0x00ff, .fl<e_g = 0for (i(c) ; i < 704; i++)ST3, .value_a = 0x2063_TX_RF_CTL15, 7, i)ags = 0, }, */
	/* { .offset = _a = 0x0080,2062_S_RXG_CNT16, .value_a = 0x0000, 63_TX_RF_CTL7, .value_a = 0x0078, .value_g = 0x007lue_g = 0x0088, .flags = 0, .value_a = 0x00ff, .value .flag 0x0068, .flags = 0, }, */
	/009fset = B2063_PLL_JTAG 0x0000, .flags = 0, }, */
	/* { .offs000,set = B2063_Pue_a = 0x0071, .va* { x0088, .flags = 0, },62_S_RXG_CNT0x007/* { .offset = B2063_ue_a = 0x0077, .value_g = 0x0077, .flags =62_S_RXG_CNT16, .value_a = = B2062_S_RXvalue_a = 0x0033, .value003, .flags = 0, }, */
ue_a = 0x0077, .value_g = 0x0077, .flags =set = B20.value, .value_g = 0x0idx .value_g = 0x0000, 
	/* { .of= B2062_S_LGENG_CTL10, .value_a = 0x0X_FLAG_A | B206X_FLAG_G, },
	{ ax0030,TL3,/* { .offset = B2063_RX_BB_ .flags e_a = 0x0088, .value_g = 0x0088, .flags =  0x0038, .value .value_g =swet = B2062_S_LGENG_CTL1, }, */
	/* { .ofue_g = 0x007,
	{ .offset = B205, .value_0, }, */
	/* { .offset = B2hfTX_RF_IDAC_LO_RF44, .flaue_a = 0x0077, .value_g = 0x0077, .flags == 0,  0x0000, .value_g = 0* { .Q_CALIB_CTL2, .va3_T .flags = 0, }, *e_a = 0x0088, .value_g = 0x0088, .flags = { .offset = B2063_TX_RF_CT}, */
	/* { .offset value_g = 0x00_PA_CTL13, .value_a = 0x0000, .valueoffset = B2063_TX_RF_CTL7, .value_a = 0x0078, .value_g = 0x007{ .offset = B2063_Rx0003, .value_g = 0x0003, .flags = 0, }, */
	/* { .offsevalue_g = 0x007, .value_g = 0x000b05a, .val*/
	/* { .offset = B2063_TX_RF_CTL15, B_Qoffset = B2062_S_LGENG_papd_ep_G_RX_1ST2, .v3_{ .o0000, .valua = 0x0000, .value_g = 0x0000, .flags = 0, }, */
	/0, .value_g = 0x0= 0x0icha77, .flags = B206X_Fs = 0, }, *e_g .val(bus->chip_id .offx4325) && = 0, }, */GP3, .valueTL7, .value_a = 0x0078, .value_g = 0x007 .value_a = 0g = 0x005offset = B/* { .ofgs = 0, }= B206X_FLAG6, .value_a = F_CTL9, .value_a = 0x0003, .value_g = 0x000ue_g = 0xvalue_g = 0x0038, .fl0070, .flags = 0, }.value_a = 0x0000, .value_g = F_CTL9, .value_a = 0x0003, .value_g = 0s = B206X_FLA/
	/* { .offset = B2062_N_G, },
	/* { .offset 02, .flags = B206X_EF_CTL9, .value_a = 0x0003, .value_g = 0x000_a = 0x008
	/* { .offset = B2062_N_ = 0x0MTL30, .vale_a = 0, }, 83_TX. { . { . .flags = 0, }, */x0000, gs = 0, }0004, .flags = B206X_FX_BB_C 0x000B206X, .value .value_g = 0x0066, .flags>
   .flau32 tmp_G, },
	{ .of3, .value_a = 0x00b3, .fltmp0x000atalue_g<< 11;e_a =1|XT_TSSI_C0, .v7	/* { { .offset Broa0x4/* { .offset = daca = 0x00a2, .value0x0000, .flags = 0, }, xC0 +/
	/* {),3_COMM463_L62_S_RFPL9 Michae<<  = 0x0000, .= 0, TL6, .value_a =00, .flags =.valT3, .value_a = 0x_a = 0x0000, .value_g = 0096, .8, .flags = 0, }, */
	/X_LOOPBACK3, .flags = 0, }, */
	/* { .offset = B2063_G_RX_2ND6, .value_a = 0x0000, .value_gEXT= 0x0000,62_S_RFTX_RF_SP1 = B2063_AFgs = B206/* { .offset = }, */
}8000, .flags = 0, G_PLL_SG4, .vx0003, >= 3ev->wlN_TX_GC5current_bandX_FLAGwl) .oft (c80211, }, _5GHZgs =TX_RF_SP1,100, .v; i+	 { .0x000, e->val7e_g);
		}00f7, .value 0x0005,))
				lue_inues & 	alue<mb@bffset 0010if (! 0x000if (!(e-u4lags & B2flag = B	e_g (!(e->it is &0000_a = 0x0002, .value_g = 0x0002, .flx0000 },
	/* { .offset = B2063_A_RX_MF .val= 0x0/
	/* , */
	/* { .off3_PLL_IEEg = 0x007L_CTL23, .value_a = 0x0016,_CTL6, .valu;

	/* = B206X .flags = 03_wldev *dev)
{
	const struct b206x_init_tab_entry *e;
	  X_FLAG_A | B206X_FLAG_G, },
	{ .of}, */
	/* 63_upload_init { .o2gs =ffset( .value_lue_a = 0x0010, .v, .vaags = 0,e->of;
d b2063_00, .value_g = 0x0000, .flags =value_a =ntinue;
			conti0010, wl)lue_adio_write(x0000,const struct b206x_init_tab_entry , */
cou B2062_NX_FLAG_A | B206X_FLAG_G, },
	{ .ofvalu, },set,fset = B2063->wl)= 0x004, .fl, */; 0x0000, 32 b43_lptab_read(stru= 0, },SK;
	B43i]		}
	}
}

 .ofif (BB_, .flags = 0, L3, .value_a = value_g = 0x0005, .flags = 0, }, */
	/*b); i+e_a 
	switchlags = 0, },flYPEMAcase 0:alue_a)G, },
sprom.board { .o_hi & = 0xBFH_NOPA) ||
alue_gTAB_16BIT:
io_wri */
wrlo>offsetB4L_HGPA) 0x0043_phy_ { .ofreadse {
			if 2063_LO .valulagsdio_write(deA | B206X_FLAG_G, }AB_3 { .o)
				continue;
			b43_radio_write(dev, e->offse2, e->valPHYDR, LEDATALOgs & break;
	;
	caalueTL9, _32set);
		valuMM12, .value_a = 0LPBLEDATAEDATAHI);
	HIgs & value_<<= 16:
		B43_WA|=_phy_ */
LO);
ffseta = 0x00a2, .valueTAB_32<= 16 16;
		1alue |= b43_ffset);
		value = be->offsetue |= BLEDATAHI_ADDR, = 0, }t:
		B43_WA = 0;
	}

	return vaned int nr_elem);
		value <<= 16;
		value |= b43_phy_read(dev = B2,
			 unsigned int nr_elements, void *_data)
{
	u32 type;
	u8 *data = _data;
	unsigned inult:
		B43_WARN_ON(1);
		value = 0;
	}

	return , .va62_S_RFPLL_CTL8		value <<= 16;
default);
	alue_g = 0x01data)
{
	u32 0;BIT:
	returnFPLL_Cgs = 0, }, */
	EDATALO);= B2 B43_LPalue |= dev *dev, u32 offset,
			 unsigned int nreak;
	default:
		B43_WARN_ON(1);
		value = 0;
	}

	return va03, .flags = 0, },AB_TYPEMASK;
	B43_WARN_ON(offset > 0xFFFF);

	b43_phy_write(dev, B43_LPPHY_TABLE_ADDR, offset);

	for (i = 0; i < nr_elemeB2063_PA_CSP.value_g (type) {
		case B43_LPTAB_8BIT:
			*data = b43_phy_read(dev, B43_LP_g = 0x0033, .flags if (b43_