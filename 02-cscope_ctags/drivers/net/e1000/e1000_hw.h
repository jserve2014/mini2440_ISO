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

*******************************************************************************/

/* e1000_hw.h
 * Structures, enums, and macros for the MAC
 */

#ifndef _E1000_HW_H_
#define _E1000_HW_H_

#include "e1000_osdep.h"

/* Forward declarations of structures used by the shared code */
struct e1000_hw;
struct e1000_hw_stats;

/* Enumerated types specific to the e1000 hardware */
/* Media Access Controlers */
typedef enum {
	e1000_undefined = 0,
	e1000_82542_rev2_0,
	e1000_82542_rev2_1,
	e1000_82543,
	e1000_82544,
	e1000_82540,
	e1000_82545,
	e1000_82545_rev_3,
	e1000_82546,
	e1000_82546_rev_3,
	e1000_82541,
	e1000_82541_rev_2,
	e1000_82547,
	e1000_82547_rev_2,
	e1000_num_macs
} e1000_mac_type;

typedef enum {
	e1000_eeprom_uninitialized = 0,
	e1000_eeprom_spi,
	e1000_eeprom_microwire,
	e1000_eeprom_flash,
	e1000_eeprom_none,	/* No NVM support */
	e1000_num_eeprom_types
} e1000_eeprom_type;

/* Media Types */
typedef enum {
	e1000_media_type_copper = 0,
	e1000_media_type_fiber = 1,
	e1000_media_type_internal_serdes = 2,
	e1000_num_media_types
} e1000_media_type;

typedef enum {
	e1000_10_half = 0,
	e1000_10_full = 1,
	e1000_100_half = 2,
	e1000_100_full = 3
} e1000_speed_duplex_type;

/* Flow Control Settings */
typedef enum {
	E1000_FC_NONE = 0,
	E1000_FC_RX_PAUSE = 1,
	E1000_FC_TX_PAUSE = 2,
	E1000_FC_FULL = 3,
	E1000_FC_DEFAULT = 0xFF
} e1000_fc_type;

struct e1000_shadow_ram {
	u16 eeprom_word;
	bool modified;
};

/* PCI bus types */
typedef enum {
	e1000_bus_type_unknown = 0,
	e1000_bus_type_pci,
	e1000_bus_type_pcix,
	e1000_bus_type_reserved
} e1000_bus_type;

/* PCI bus speeds */
typedef enum {
	e1000_bus_speed_unknown = 0,
	e1000_bus_speed_33,
	e1000_bus_speed_66,
	e1000_bus_speed_100,
	e1000_bus_speed_120,
	e1000_bus_speed_133,
	e1000_bus_speed_reserved
} e1000_bus_speed;

/* PCI bus widths */
typedef enum {
	e1000_bus_width_unknown = 0,
	e1000_bus_width_32,
	e1000_bus_width_64,
	e1000_bus_width_reserved
} e1000_bus_width;

/* PHY status info structure and supporting enums */
typedef enum {
	e1000_cable_length_50 = 0,
	e1000_cable_length_50_80,
	e1000_cable_length_80_110,
	e1000_cable_length_110_140,
	e1000_cable_length_140,
	e1000_cable_length_undefined = 0xFF
} e1000_cable_length;

typedef enum {
	e1000_gg_cable_length_60 = 0,
	e1000_gg_cable_length_60_115 = 1,
	e1000_gg_cable_length_115_150 = 2,
	e1000_gg_cable_length_150 = 4
} e1000_gg_cable_length;

typedef enum {
	e1000_igp_cable_length_10 = 10,
	e1000_igp_cable_length_20 = 20,
	e1000_igp_cable_length_30 = 30,
	e1000_igp_cable_length_40 = 40,
	e1000_igp_cable_length_50 = 50,
	e1000_igp_cable_length_60 = 60,
	e1000_igp_cable_length_70 = 70,
	e1000_igp_cable_length_80 = 80,
	e1000_igp_cable_length_90 = 90,
	e1000_igp_cable_length_100 = 100,
	e1000_igp_cable_length_110 = 110,
	e1000_igp_cable_length_115 = 115,
	e1000_igp_cable_length_120 = 120,
	e1000_igp_cable_length_130 = 130,
	e1000_igp_cable_length_140 = 140,
	e1000_igp_cable_length_150 = 150,
	e1000_igp_cable_length_160 = 160,
	e1000_igp_cable_length_170 = 170,
	e1000_igp_cable_length_180 = 180
} e1000_igp_cable_length;

typedef enum {
	e1000_10bt_ext_dist_enable_normal = 0,
	e1000_10bt_ext_dist_enable_lower,
	e1000_10bt_ext_dist_enable_undefined = 0xFF
} e1000_10bt_ext_dist_enable;

typedef enum {
	e1000_rev_polarity_normal = 0,
	e1000_rev_polarity_reversed,
	e1000_rev_polarity_undefined = 0xFF
} e1000_rev_polarity;

typedef enum {
	e1000_downshift_normal = 0,
	e1000_downshift_activated,
	e1000_downshift_undefined = 0xFF
} e1000_downshift;

typedef enum {
	e1000_smart_speed_default = 0,
	e1000_smart_speed_on,
	e1000_smart_speed_off
} e1000_smart_speed;

typedef enum {
	e1000_polarity_reversal_enabled = 0,
	e1000_polarity_reversal_disabled,
	e1000_polarity_reversal_undefined = 0xFF
} e1000_polarity_reversal;

typedef enum {
	e1000_auto_x_mode_manual_mdi = 0,
	e1000_auto_x_mode_manual_mdix,
	e1000_auto_x_mode_auto1,
	e1000_auto_x_mode_auto2,
	e1000_auto_x_mode_undefined = 0xFF
} e1000_auto_x_mode;

typedef enum {
	e1000_1000t_rx_status_not_ok = 0,
	e1000_1000t_rx_status_ok,
	e1000_1000t_rx_status_undefined = 0xFF
} e1000_1000t_rx_status;

typedef enum {
    e1000_phy_m88 = 0,
    e1000_phy_igp,
    e1000_phy_undefined = 0xFF
} e1000_phy_type;

typedef enum {
	e1000_ms_hw_default = 0,
	e1000_ms_force_master,
	e1000_ms_force_slave,
	e1000_ms_auto
} e1000_ms_type;

typedef enum {
	e1000_ffe_config_enabled = 0,
	e1000_ffe_config_active,
	e1000_ffe_config_blocked
} e1000_ffe_config;

typedef enum {
	e1000_dsp_config_disabled = 0,
	e1000_dsp_config_enabled,
	e1000_dsp_config_activated,
	e1000_dsp_config_undefined = 0xFF
} e1000_dsp_config;

struct e1000_phy_info {
	e1000_cable_length cable_length;
	e1000_10bt_ext_dist_enable extended_10bt_distance;
	e1000_rev_polarity cable_polarity;
	e1000_downshift downshift;
	e1000_polarity_reversal polarity_correction;
	e1000_auto_x_mode mdix_mode;
	e1000_1000t_rx_status local_rx;
	e1000_1000t_rx_status remote_rx;
};

struct e1000_phy_stats {
	u32 idle_errors;
	u32 receive_errors;
};

struct e1000_eeprom_info {
	e1000_eeprom_type type;
	u16 word_size;
	u16 opcode_bits;
	u16 address_bits;
	u16 delay_usec;
	u16 page_size;
};

/* Flex ASF Information */
#define E1000_HOST_IF_MAX_SIZE  2048

typedef enum {
	e1000_byte_align = 0,
	e1000_word_align = 1,
	e1000_dword_align = 2
} e1000_align_type;

/* Error Codes */
#define E1000_SUCCESS      0
#define E1000_ERR_EEPROM   1
#define E1000_ERR_PHY      2
#define E1000_ERR_CONFIG   3
#define E1000_ERR_PARAM    4
#define E1000_ERR_MAC_TYPE 5
#define E1000_ERR_PHY_TYPE 6
#define E1000_ERR_RESET   9
#define E1000_ERR_MASTER_REQUESTS_PENDING 10
#define E1000_ERR_HOST_INTERFACE_COMMAND 11
#define E1000_BLK_PHY_RESET   12

#define E1000_BYTE_SWAP_WORD(_value) ((((_value) & 0x00ff) << 8) | \
                                     (((_value) & 0xff00) >> 8))

/* Function prototypes */
/* Initialization */
s32 e1000_reset_hw(struct e1000_hw *hw);
s32 e1000_init_hw(struct e1000_hw *hw);
s32 e1000_set_mac_type(struct e1000_hw *hw);
void e1000_set_media_type(struct e1000_hw *hw);

/* Link Configuration */
s32 e1000_setup_link(struct e1000_hw *hw);
s32 e1000_phy_setup_autoneg(struct e1000_hw *hw);
void e1000_config_collision_dist(struct e1000_hw *hw);
s32 e1000_check_for_link(struct e1000_hw *hw);
s32 e1000_get_speed_and_duplex(struct e1000_hw *hw, u16 * speed, u16 * duplex);
s32 e1000_force_mac_fc(struct e1000_hw *hw);

/* PHY */
s32 e1000_read_phy_reg(struct e1000_hw *hw, u32 reg_addr, u16 * phy_data);
s32 e1000_write_phy_reg(struct e1000_hw *hw, u32 reg_addr, u16 data);
s32 e1000_phy_hw_reset(struct e1000_hw *hw);
s32 e1000_phy_reset(struct e1000_hw *hw);
s32 e1000_phy_get_info(struct e1000_hw *hw, struct e1000_phy_info *phy_info);
s32 e1000_validate_mdi_setting(struct e1000_hw *hw);

/* EEPROM Functions */
s32 e1000_init_eeprom_params(struct e1000_hw *hw);

/* MNG HOST IF functions */
u32 e1000_enable_mng_pass_thru(struct e1000_hw *hw);

#define E1000_MNG_DHCP_TX_PAYLOAD_CMD   64
#define E1000_HI_MAX_MNG_DATA_LENGTH    0x6F8	/* Host Interface data length */

#define E1000_MNG_DHCP_COMMAND_TIMEOUT  10	/* Time in ms to process MNG command */
#define E1000_MNG_DHCP_COOKIE_OFFSET    0x6F0	/* Cookie offset */
#define E1000_MNG_DHCP_COOKIE_LENGTH    0x10	/* Cookie length */
#define E1000_MNG_IAMT_MODE             0x3
#define E1000_MNG_ICH_IAMT_MODE         0x2
#define E1000_IAMT_SIGNATURE            0x544D4149	/* Intel(R) Active Management Technology signature */

#define E1000_MNG_DHCP_COOKIE_STATUS_PARSING_SUPPORT 0x1	/* DHCP parsing enabled */
#define E1000_MNG_DHCP_COOKIE_STATUS_VLAN_SUPPORT    0x2	/* DHCP parsing enabled */
#define E1000_VFTA_ENTRY_SHIFT                       0x5
#define E1000_VFTA_ENTRY_MASK                        0x7F
#define E1000_VFTA_ENTRY_BIT_SHIFT_MASK              0x1F

struct e1000_host_mng_command_header {
	u8 command_id;
	u8 checksum;
	u16 reserved1;
	u16 reserved2;
	u16 command_length;
};

struct e1000_host_mng_command_info {
	struct e1000_host_mng_command_header command_header;	/* Command Head/Command Result Head has 4 bytes */
	u8 command_data[E1000_HI_MAX_MNG_DATA_LENGTH];	/* Command data can length 0..0x658 */
};
#ifdef __BIG_ENDIAN
struct e1000_host_mng_dhcp_cookie {
	u32 signature;
	u16 vlan_id;
	u8 reserved0;
	u8 status;
	u32 reserved1;
	u8 checksum;
	u8 reserved3;
	u16 reserved2;
};
#else
struct e1000_host_mng_dhcp_cookie {
	u32 signature;
	u8 status;
	u8 reserved0;
	u16 vlan_id;
	u32 reserved1;
	u16 reserved2;
	u8 reserved3;
	u8 checksum;
};
#endif

bool e1000_check_mng_mode(struct e1000_hw *hw);
s32 e1000_read_eeprom(struct e1000_hw *hw, u16 reg, u16 words, u16 * data);
s32 e1000_validate_eeprom_checksum(struct e1000_hw *hw);
s32 e1000_update_eeprom_checksum(struct e1000_hw *hw);
s32 e1000_write_eeprom(struct e1000_hw *hw, u16 reg, u16 words, u16 * data);
s32 e1000_read_mac_addr(struct e1000_hw *hw);

/* Filters (multicast, vlan, receive) */
u32 e1000_hash_mc_addr(struct e1000_hw *hw, u8 * mc_addr);
void e1000_mta_set(struct e1000_hw *hw, u32 hash_value);
void e1000_rar_set(struct e1000_hw *hw, u8 * mc_addr, u32 rar_index);
void e1000_write_vfta(struct e1000_hw *hw, u32 offset, u32 value);

/* LED functions */
s32 e1000_setup_led(struct e1000_hw *hw);
s32 e1000_cleanup_led(struct e1000_hw *hw);
s32 e1000_led_on(struct e1000_hw *hw);
s32 e1000_led_off(struct e1000_hw *hw);
s32 e1000_blink_led_start(struct e1000_hw *hw);

/* Adaptive IFS Functions */

/* Everything else */
void e1000_reset_adaptive(struct e1000_hw *hw);
void e1000_update_adaptive(struct e1000_hw *hw);
void e1000_tbi_adjust_stats(struct e1000_hw *hw, struct e1000_hw_stats *stats,
			    u32 frame_len, u8 * mac_addr);
void e1000_get_bus_info(struct e1000_hw *hw);
void e1000_pci_set_mwi(struct e1000_hw *hw);
void e1000_pci_clear_mwi(struct e1000_hw *hw);
void e1000_pcix_set_mmrbc(struct e1000_hw *hw, int mmrbc);
int e1000_pcix_get_mmrbc(struct e1000_hw *hw);
/* Port I/O is only supported on 82544 and newer */
void e1000_io_write(struct e1000_hw *hw, unsigned long port, u32 value);

#define E1000_READ_REG_IO(a, reg) \
    e1000_read_reg_io((a), E1000_##reg)
#define E1000_WRITE_REG_IO(a, reg, val) \
    e1000_write_reg_io((a), E1000_##reg, val)

/* PCI Device IDs */
#define E1000_DEV_ID_82542               0x1000
#define E1000_DEV_ID_82543GC_FIBER       0x1001
#define E1000_DEV_ID_82543GC_COPPER      0x1004
#define E1000_DEV_ID_82544EI_COPPER      0x1008
#define E1000_DEV_ID_82544EI_FIBER       0x1009
#define E1000_DEV_ID_82544GC_COPPER      0x100C
#define E1000_DEV_ID_82544GC_LOM         0x100D
#define E1000_DEV_ID_82540EM             0x100E
#define E1000_DEV_ID_82540EM_LOM         0x1015
#define E1000_DEV_ID_82540EP_LOM         0x1016
#define E1000_DEV_ID_82540EP             0x1017
#define E1000_DEV_ID_82540EP_LP          0x101E
#define E1000_DEV_ID_82545EM_COPPER      0x100F
#define E1000_DEV_ID_82545EM_FIBER       0x1011
#define E1000_DEV_ID_82545GM_COPPER      0x1026
#define E1000_DEV_ID_82545GM_FIBER       0x1027
#define E1000_DEV_ID_82545GM_SERDES      0x1028
#define E1000_DEV_ID_82546EB_COPPER      0x1010
#define E1000_DEV_ID_82546EB_FIBER       0x1012
#define E1000_DEV_ID_82546EB_QUAD_COPPER 0x101D
#define E1000_DEV_ID_82541EI             0x1013
#define E1000_DEV_ID_82541EI_MOBILE      0x1018
#define E1000_DEV_ID_82541ER_LOM         0x1014
#define E1000_DEV_ID_82541ER             0x1078
#define E1000_DEV_ID_82547GI             0x1075
#define E1000_DEV_ID_82541GI             0x1076
#define E1000_DEV_ID_82541GI_MOBILE      0x1077
#define E1000_DEV_ID_82541GI_LF          0x107C
#define E1000_DEV_ID_82546GB_COPPER      0x1079
#define E1000_DEV_ID_82546GB_FIBER       0x107A
#define E1000_DEV_ID_82546GB_SERDES      0x107B
#define E1000_DEV_ID_82546GB_PCIE        0x108A
#define E1000_DEV_ID_82546GB_QUAD_COPPER 0x1099
#define E1000_DEV_ID_82547EI             0x1019
#define E1000_DEV_ID_82547EI_MOBILE      0x101A
#define E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3 0x10B5

#define NODE_ADDRESS_SIZE 6
#define ETH_LENGTH_OF_ADDRESS 6

/* MAC decode size is 128K - This is the size of BAR0 */
#define MAC_DECODE_SIZE (128 * 1024)

#define E1000_82542_2_0_REV_ID 2
#define E1000_82542_2_1_REV_ID 3
#define E1000_REVISION_0       0
#define E1000_REVISION_1       1
#define E1000_REVISION_2       2
#define E1000_REVISION_3       3

#define SPEED_10    10
#define SPEED_100   100
#define SPEED_1000  1000
#define HALF_DUPLEX 1
#define FULL_DUPLEX 2

/* The sizes (in bytes) of a ethernet packet */
#define ENET_HEADER_SIZE             14
#define MINIMUM_ETHERNET_FRAME_SIZE  64	/* With FCS */
#define ETHERNET_FCS_SIZE            4
#define MINIMUM_ETHERNET_PACKET_SIZE \
    (MINIMUM_ETHERNET_FRAME_SIZE - ETHERNET_FCS_SIZE)
#define CRC_LENGTH                   ETHERNET_FCS_SIZE
#define MAX_JUMBO_FRAME_SIZE         0x3F00

/* 802.1q VLAN Packet Sizes */
#define VLAN_TAG_SIZE  4	/* 802.3ac tag (not DMAed) */

/* Ethertype field values */
#define ETHERNET_IEEE_VLAN_TYPE 0x8100	/* 802.3ac packet */
#define ETHERNET_IP_TYPE        0x0800	/* IP packets */
#define ETHERNET_ARP_TYPE       0x0806	/* Address Resolution Protocol (ARP) */

/* Packet Header defines */
#define IP_PROTOCOL_TCP    6
#define IP_PROTOCOL_UDP    0x11

/* This defines the bits that are set in the Interrupt Mask
 * Set/Read Register.  Each bit is documented below:
 *   o RXDMT0 = Receive Descriptor Minimum Threshold hit (ring 0)
 *   o RXSEQ  = Receive Sequence Error
 */
#define POLL_IMS_ENABLE_MASK ( \
    E1000_IMS_RXDMT0 |         \
    E1000_IMS_RXSEQ)

/* This defines the bits that are set in the Interrupt Mask
 * Set/Read Register.  Each bit is documented below:
 *   o RXT0   = Receiver Timer Interrupt (ring 0)
 *   o TXDW   = Transmit Descriptor Written Back
 *   o RXDMT0 = Receive Descriptor Minimum Threshold hit (ring 0)
 *   o RXSEQ  = Receive Sequence Error
 *   o LSC    = Link Status Change
 */
#define IMS_ENABLE_MASK ( \
    E1000_IMS_RXT0   |    \
    E1000_IMS_TXDW   |    \
    E1000_IMS_RXDMT0 |    \
    E1000_IMS_RXSEQ  |    \
    E1000_IMS_LSC)

/* Number of high/low register pairs in the RAR. The RAR (Receive Address
 * Registers) holds the directed and multicast addresses that we monitor. We
 * reserve one of these spots for our directed address, allowing us room for
 * E1000_RAR_ENTRIES - 1 multicast addresses.
 */
#define E1000_RAR_ENTRIES 15

#define MIN_NUMBER_OF_DESCRIPTORS  8
#define MAX_NUMBER_OF_DESCRIPTORS  0xFFF8

/* Receive Descriptor */
struct e1000_rx_desc {
	__le64 buffer_addr;	/* Address of the descriptor's data buffer */
	__le16 length;		/* Length of data DMAed into data buffer */
	__le16 csum;		/* Packet checksum */
	u8 status;		/* Descriptor status */
	u8 errors;		/* Descriptor Errors */
	__le16 special;
};

/* Receive Descriptor - Extended */
union e1000_rx_desc_extended {
	struct {
		__le64 buffer_addr;
		__le64 reserved;
	} read;
	struct {
		struct {
			__le32 mrq;	/* Multiple Rx Queues */
			union {
				__le32 rss;	/* RSS Hash */
				struct {
					__le16 ip_id;	/* IP id */
					__le16 csum;	/* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;	/* ext status/error */
			__le16 length;
			__le16 vlan;	/* VLAN tag */
		} upper;
	} wb;			/* writeback */
};

#define MAX_PS_BUFFERS 4
/* Receive Descriptor - Packet Split */
union e1000_rx_desc_packet_split {
	struct {
		/* one buffer for protocol header(s), three data buffers */
		__le64 buffer_addr[MAX_PS_BUFFERS];
	} read;
	struct {
		struct {
			__le32 mrq;	/* Multiple Rx Queues */
			union {
				__le32 rss;	/* RSS Hash */
				struct {
					__le16 ip_id;	/* IP id */
					__le16 csum;	/* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;	/* ext status/error */
			__le16 length0;	/* length of buffer 0 */
			__le16 vlan;	/* VLAN tag */
		} middle;
		struct {
			__le16 header_status;
			__le16 length[3];	/* length of buffers 1-3 */
		} upper;
		__le64 reserved;
	} wb;			/* writeback */
};

/* Receive Descriptor bit definitions */
#define E1000_RXD_STAT_DD       0x01	/* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02	/* End of Packet */
#define E1000_RXD_STAT_IXSM     0x04	/* Ignore checksum */
#define E1000_RXD_STAT_VP       0x08	/* IEEE VLAN Packet */
#define E1000_RXD_STAT_UDPCS    0x10	/* UDP xsum calculated */
#define E1000_RXD_STAT_TCPCS    0x20	/* TCP xsum calculated */
#define E1000_RXD_STAT_IPCS     0x40	/* IP xsum calculated */
#define E1000_RXD_STAT_PIF      0x80	/* passed in-exact filter */
#define E1000_RXD_STAT_IPIDV    0x200	/* IP identification valid */
#define E1000_RXD_STAT_UDPV     0x400	/* Valid UDP checksum */
#define E1000_RXD_STAT_ACK      0x8000	/* ACK Packet indication */
#define E1000_RXD_ERR_CE        0x01	/* CRC Error */
#define E1000_RXD_ERR_SE        0x02	/* Symbol Error */
#define E1000_RXD_ERR_SEQ       0x04	/* Sequence Error */
#define E1000_RXD_ERR_CXE       0x10	/* Carrier Extension Error */
#define E1000_RXD_ERR_TCPE      0x20	/* TCP/UDP Checksum Error */
#define E1000_RXD_ERR_IPE       0x40	/* IP Checksum Error */
#define E1000_RXD_ERR_RXE       0x80	/* Rx Data Error */
#define E1000_RXD_SPC_VLAN_MASK 0x0FFF	/* VLAN ID is in lower 12 bits */
#define E1000_RXD_SPC_PRI_MASK  0xE000	/* Priority is in upper 3 bits */
#define E1000_RXD_SPC_PRI_SHIFT 13
#define E1000_RXD_SPC_CFI_MASK  0x1000	/* CFI is bit 12 */
#define E1000_RXD_SPC_CFI_SHIFT 12

#define E1000_RXDEXT_STATERR_CE    0x01000000
#define E1000_RXDEXT_STATERR_SE    0x02000000
#define E1000_RXDEXT_STATERR_SEQ   0x04000000
#define E1000_RXDEXT_STATERR_CXE   0x10000000
#define E1000_RXDEXT_STATERR_TCPE  0x20000000
#define E1000_RXDEXT_STATERR_IPE   0x40000000
#define E1000_RXDEXT_STATERR_RXE   0x80000000

#define E1000_RXDPS_HDRSTAT_HDRSP        0x00008000
#define E1000_RXDPS_HDRSTAT_HDRLEN_MASK  0x000003FF

/* mask to determine if packets should be dropped due to frame errors */
#define E1000_RXD_ERR_FRAME_ERR_MASK ( \
    E1000_RXD_ERR_CE  |                \
    E1000_RXD_ERR_SE  |                \
    E1000_RXD_ERR_SEQ |                \
    E1000_RXD_ERR_CXE |                \
    E1000_RXD_ERR_RXE)

/* Same mask, but for extended and packet split descriptors */
#define E1000_RXDEXT_ERR_FRAME_ERR_MASK ( \
    E1000_RXDEXT_STATERR_CE  |            \
    E1000_RXDEXT_STATERR_SE  |            \
    E1000_RXDEXT_STATERR_SEQ |            \
    E1000_RXDEXT_STATERR_CXE |            \
    E1000_RXDEXT_STATERR_RXE)

/* Transmit Descriptor */
struct e1000_tx_desc {
	__le64 buffer_addr;	/* Address of the descriptor's data buffer */
	union {
		__le32 data;
		struct {
			__le16 length;	/* Data buffer length */
			u8 cso;	/* Checksum offset */
			u8 cmd;	/* Descriptor control */
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			u8 status;	/* Descriptor status */
			u8 css;	/* Checksum start */
			__le16 special;
		} fields;
	} upper;
};

/* Transmit Descriptor bit definitions */
#define E1000_TXD_DTYP_D     0x00100000	/* Data Descriptor */
#define E1000_TXD_DTYP_C     0x00000000	/* Context Descriptor */
#define E1000_TXD_POPTS_IXSM 0x01	/* Insert IP checksum */
#define E1000_TXD_POPTS_TXSM 0x02	/* Insert TCP/UDP checksum */
#define E1000_TXD_CMD_EOP    0x01000000	/* End of Packet */
#define E1000_TXD_CMD_IFCS   0x02000000	/* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_IC     0x04000000	/* Insert Checksum */
#define E1000_TXD_CMD_RS     0x08000000	/* Report Status */
#define E1000_TXD_CMD_RPS    0x10000000	/* Report Packet Sent */
#define E1000_TXD_CMD_DEXT   0x20000000	/* Descriptor extension (0 = legacy) */
#define E1000_TXD_CMD_VLE    0x40000000	/* Add VLAN tag */
#define E1000_TXD_CMD_IDE    0x80000000	/* Enable Tidv register */
#define E1000_TXD_STAT_DD    0x00000001	/* Descriptor Done */
#define E1000_TXD_STAT_EC    0x00000002	/* Excess Collisions */
#define E1000_TXD_STAT_LC    0x00000004	/* Late Collisions */
#define E1000_TXD_STAT_TU    0x00000008	/* Transmit underrun */
#define E1000_TXD_CMD_TCP    0x01000000	/* TCP packet */
#define E1000_TXD_CMD_IP     0x02000000	/* IP packet */
#define E1000_TXD_CMD_TSE    0x04000000	/* TCP Seg enable */
#define E1000_TXD_STAT_TC    0x00000004	/* Tx Underrun */

/* Offload Context Descriptor */
struct e1000_context_desc {
	union {
		__le32 ip_config;
		struct {
			u8 ipcss;	/* IP checksum start */
			u8 ipcso;	/* IP checksum offset */
			__le16 ipcse;	/* IP checksum end */
		} ip_fields;
	} lower_setup;
	union {
		__le32 tcp_config;
		struct {
			u8 tucss;	/* TCP checksum start */
			u8 tucso;	/* TCP checksum offset */
			__le16 tucse;	/* TCP checksum end */
		} tcp_fields;
	} upper_setup;
	__le32 cmd_and_length;	/* */
	union {
		__le32 data;
		struct {
			u8 status;	/* Descriptor status */
			u8 hdr_len;	/* Header length */
			__le16 mss;	/* Maximum segment size */
		} fields;
	} tcp_seg_setup;
};

/* Offload data descriptor */
struct e1000_data_desc {
	__le64 buffer_addr;	/* Address of the descriptor's buffer address */
	union {
		__le32 data;
		struct {
			__le16 length;	/* Data buffer length */
			u8 typ_len_ext;	/* */
			u8 cmd;	/* */
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			u8 status;	/* Descriptor status */
			u8 popts;	/* Packet Options */
			__le16 special;	/* */
		} fields;
	} upper;
};

/* Filters */
#define E1000_NUM_UNICAST          16	/* Unicast filter entries */
#define E1000_MC_TBL_SIZE          128	/* Multicast Filter Table (4096 bits) */
#define E1000_VLAN_FILTER_TBL_SIZE 128	/* VLAN Filter Table (4096 bits) */

/* Receive Address Register */
struct e1000_rar {
	volatile __le32 low;	/* receive address low */
	volatile __le32 high;	/* receive address high */
};

/* Number of entries in the Multicast Table Array (MTA). */
#define E1000_NUM_MTA_REGISTERS 128

/* IPv4 Address Table Entry */
struct e1000_ipv4_at_entry {
	volatile u32 ipv4_addr;	/* IP Address (RW) */
	volatile u32 reserved;
};

/* Four wakeup IP addresses are supported */
#define E1000_WAKEUP_IP_ADDRESS_COUNT_MAX 4
#define E1000_IP4AT_SIZE                  E1000_WAKEUP_IP_ADDRESS_COUNT_MAX
#define E1000_IP6AT_SIZE                  1

/* IPv6 Address Table Entry */
struct e1000_ipv6_at_entry {
	volatile u8 ipv6_addr[16];
};

/* Flexible Filter Length Table Entry */
struct e1000_fflt_entry {
	volatile u32 length;	/* Flexible Filter Length (RW) */
	volatile u32 reserved;
};

/* Flexible Filter Mask Table Entry */
struct e1000_ffmt_entry {
	volatile u32 mask;	/* Flexible Filter Mask (RW) */
	volatile u32 reserved;
};

/* Flexible Filter Value Table Entry */
struct e1000_ffvt_entry {
	volatile u32 value;	/* Flexible Filter Value (RW) */
	volatile u32 reserved;
};

/* Four Flexible Filters are supported */
#define E1000_FLEXIBLE_FILTER_COUNT_MAX 4

/* Each Flexible Filter is at most 128 (0x80) bytes in length */
#define E1000_FLEXIBLE_FILTER_SIZE_MAX  128

#define E1000_FFLT_SIZE E1000_FLEXIBLE_FILTER_COUNT_MAX
#define E1000_FFMT_SIZE E1000_FLEXIBLE_FILTER_SIZE_MAX
#define E1000_FFVT_SIZE E1000_FLEXIBLE_FILTER_SIZE_MAX

#define E1000_DISABLE_SERDES_LOOPBACK   0x0400

/* Register Set. (82543, 82544)
 *
 * Registers are defined to be 32 bits and  should be accessed as 32 bit values.
 * These registers are physically located on the NIC, but are mapped into the
 * host memory address space.
 *
 * RW - register is both readable and writable
 * RO - register is read only
 * WO - register is write only
 * R/clr - register is read only and is cleared when read
 * A - register array
 */
#define E1000_CTRL     0x00000	/* Device Control - RW */
#define E1000_CTRL_DUP 0x00004	/* Device Control Duplicate (Shadow) - RW */
#define E1000_STATUS   0x00008	/* Device Status - RO */
#define E1000_EECD     0x00010	/* EEPROM/Flash Control - RW */
#define E1000_EERD     0x00014	/* EEPROM Read - RW */
#define E1000_CTRL_EXT 0x00018	/* Extended Device Control - RW */
#define E1000_FLA      0x0001C	/* Flash Access - RW */
#define E1000_MDIC     0x00020	/* MDI Control - RW */
#define E1000_SCTL     0x00024	/* SerDes Control - RW */
#define E1000_FEXTNVM  0x00028	/* Future Extended NVM register */
#define E1000_FCAL     0x00028	/* Flow Control Address Low - RW */
#define E1000_FCAH     0x0002C	/* Flow Control Address High -RW */
#define E1000_FCT      0x00030	/* Flow Control Type - RW */
#define E1000_VET      0x00038	/* VLAN Ether Type - RW */
#define E1000_ICR      0x000C0	/* Interrupt Cause Read - R/clr */
#define E1000_ITR      0x000C4	/* Interrupt Throttling Rate - RW */
#define E1000_ICS      0x000C8	/* Interrupt Cause Set - WO */
#define E1000_IMS      0x000D0	/* Interrupt Mask Set - RW */
#define E1000_IMC      0x000D8	/* Interrupt Mask Clear - WO */
#define E1000_IAM      0x000E0	/* Interrupt Acknowledge Auto Mask */
#define E1000_RCTL     0x00100	/* RX Control - RW */
#define E1000_RDTR1    0x02820	/* RX Delay Timer (1) - RW */
#define E1000_RDBAL1   0x02900	/* RX Descriptor Base Address Low (1) - RW */
#define E1000_RDBAH1   0x02904	/* RX Descriptor Base Address High (1) - RW */
#define E1000_RDLEN1   0x02908	/* RX Descriptor Length (1) - RW */
#define E1000_RDH1     0x02910	/* RX Descriptor Head (1) - RW */
#define E1000_RDT1     0x02918	/* RX Descriptor Tail (1) - RW */
#define E1000_FCTTV    0x00170	/* Flow Control Transmit Timer Value - RW */
#define E1000_TXCW     0x00178	/* TX Configuration Word - RW */
#define E1000_RXCW     0x00180	/* RX Configuration Word - RO */
#define E1000_TCTL     0x00400	/* TX Control - RW */
#define E1000_TCTL_EXT 0x00404	/* Extended TX Control - RW */
#define E1000_TIPG     0x00410	/* TX Inter-packet gap -RW */
#define E1000_TBT      0x00448	/* TX Burst Timer - RW */
#define E1000_AIT      0x00458	/* Adaptive Interframe Spacing Throttle - RW */
#define E1000_LEDCTL   0x00E00	/* LED Control - RW */
#define E1000_EXTCNF_CTRL  0x00F00	/* Extended Configuration Control */
#define E1000_EXTCNF_SIZE  0x00F08	/* Extended Configuration Size */
#define E1000_PHY_CTRL     0x00F10	/* PHY Control Register in CSR */
#define FEXTNVM_SW_CONFIG  0x0001
#define E1000_PBA      0x01000	/* Packet Buffer Allocation - RW */
#define E1000_PBS      0x01008	/* Packet Buffer Size */
#define E1000_EEMNGCTL 0x01010	/* MNG EEprom Control */
#define E1000_FLASH_UPDATES 1000
#define E1000_EEARBC   0x01024	/* EEPROM Auto Read Bus Control */
#define E1000_FLASHT   0x01028	/* FLASH Timer Register */
#define E1000_EEWR     0x0102C	/* EEPROM Write Register - RW */
#define E1000_FLSWCTL  0x01030	/* FLASH control register */
#define E1000_FLSWDATA 0x01034	/* FLASH data register */
#define E1000_FLSWCNT  0x01038	/* FLASH Access Counter */
#define E1000_FLOP     0x0103C	/* FLASH Opcode Register */
#define E1000_ERT      0x02008	/* Early Rx Threshold - RW */
#define E1000_FCRTL    0x02160	/* Flow Control Receive Threshold Low - RW */
#define E1000_FCRTH    0x02168	/* Flow Control Receive Threshold High - RW */
#define E1000_PSRCTL   0x02170	/* Packet Split Receive Control - RW */
#define E1000_RDBAL    0x02800	/* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804	/* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808	/* RX Descriptor Length - RW */
#define E1000_RDH      0x02810	/* RX Descriptor Head - RW */
#define E1000_RDT      0x02818	/* RX Descriptor Tail - RW */
#define E1000_RDTR     0x02820	/* RX Delay Timer - RW */
#define E1000_RDBAL0   E1000_RDBAL	/* RX Desc Base Address Low (0) - RW */
#define E1000_RDBAH0   E1000_RDBAH	/* RX Desc Base Address High (0) - RW */
#define E1000_RDLEN0   E1000_RDLEN	/* RX Desc Length (0) - RW */
#define E1000_RDH0     E1000_RDH	/* RX Desc Head (0) - RW */
#define E1000_RDT0     E1000_RDT	/* RX Desc Tail (0) - RW */
#define E1000_RDTR0    E1000_RDTR	/* RX Delay Timer (0) - RW */
#define E1000_RXDCTL   0x02828	/* RX Descriptor Control queue 0 - RW */
#define E1000_RXDCTL1  0x02928	/* RX Descriptor Control queue 1 - RW */
#define E1000_RADV     0x0282C	/* RX Interrupt Absolute Delay Timer - RW */
#define E1000_RSRPD    0x02C00	/* RX Small Packet Detect - RW */
#define E1000_RAID     0x02C08	/* Receive Ack Interrupt Delay - RW */
#define E1000_TXDMAC   0x03000	/* TX DMA Control - RW */
#define E1000_KABGTXD  0x03004	/* AFE Band Gap Transmit Ref Data */
#define E1000_TDFH     0x03410	/* TX Data FIFO Head - RW */
#define E1000_TDFT     0x03418	/* TX Data FIFO Tail - RW */
#define E1000_TDFHS    0x03420	/* TX Data FIFO Head Saved - RW */
#define E1000_TDFTS    0x03428	/* TX Data FIFO Tail Saved - RW */
#define E1000_TDFPC    0x03430	/* TX Data FIFO Packet Count - RW */
#define E1000_TDBAL    0x03800	/* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804	/* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808	/* TX Descriptor Length - RW */
#define E1000_TDH      0x03810	/* TX Descriptor Head - RW */
#define E1000_TDT      0x03818	/* TX Descripotr Tail - RW */
#define E1000_TIDV     0x03820	/* TX Interrupt Delay Value - RW */
#define E1000_TXDCTL   0x03828	/* TX Descriptor Control - RW */
#define E1000_TADV     0x0382C	/* TX Interrupt Absolute Delay Val - RW */
#define E1000_TSPMT    0x03830	/* TCP Segmentation PAD & Min Threshold - RW */
#define E1000_TARC0    0x03840	/* TX Arbitration Count (0) */
#define E1000_TDBAL1   0x03900	/* TX Desc Base Address Low (1) - RW */
#define E1000_TDBAH1   0x03904	/* TX Desc Base Address High (1) - RW */
#define E1000_TDLEN1   0x03908	/* TX Desc Length (1) - RW */
#define E1000_TDH1     0x03910	/* TX Desc Head (1) - RW */
#define E1000_TDT1     0x03918	/* TX Desc Tail (1) - RW */
#define E1000_TXDCTL1  0x03928	/* TX Descriptor Control (1) - RW */
#define E1000_TARC1    0x03940	/* TX Arbitration Count (1) */
#define E1000_CRCERRS  0x04000	/* CRC Error Count - R/clr */
#define E1000_ALGNERRC 0x04004	/* Alignment Error Count - R/clr */
#define E1000_SYMERRS  0x04008	/* Symbol Error Count - R/clr */
#define E1000_RXERRC   0x0400C	/* Receive Error Count - R/clr */
#define E1000_MPC      0x04010	/* Missed Packet Count - R/clr */
#define E1000_SCC      0x04014	/* Single Collision Count - R/clr */
#define E1000_ECOL     0x04018	/* Excessive Collision Count - R/clr */
#define E1000_MCC      0x0401C	/* Multiple Collision Count - R/clr */
#define E1000_LATECOL  0x04020	/* Late Collision Count - R/clr */
#define E1000_COLC     0x04028	/* Collision Count - R/clr */
#define E1000_DC       0x04030	/* Defer Count - R/clr */
#define E1000_TNCRS    0x04034	/* TX-No CRS - R/clr */
#define E1000_SEC      0x04038	/* Sequence Error Count - R/clr */
#define E1000_CEXTERR  0x0403C	/* Carrier Extension Error Count - R/clr */
#define E1000_RLEC     0x04040	/* Receive Length Error Count - R/clr */
#define E1000_XONRXC   0x04048	/* XON RX Count - R/clr */
#define E1000_XONTXC   0x0404C	/* XON TX Count - R/clr */
#define E1000_XOFFRXC  0x04050	/* XOFF RX Count - R/clr */
#define E1000_XOFFTXC  0x04054	/* XOFF TX Count - R/clr */
#define E1000_FCRUC    0x04058	/* Flow Control RX Unsupported Count- R/clr */
#define E1000_PRC64    0x0405C	/* Packets RX (64 bytes) - R/clr */
#define E1000_PRC127   0x04060	/* Packets RX (65-127 bytes) - R/clr */
#define E1000_PRC255   0x04064	/* Packets RX (128-255 bytes) - R/clr */
#define E1000_PRC511   0x04068	/* Packets RX (255-511 bytes) - R/clr */
#define E1000_PRC1023  0x0406C	/* Packets RX (512-1023 bytes) - R/clr */
#define E1000_PRC1522  0x04070	/* Packets RX (1024-1522 bytes) - R/clr */
#define E1000_GPRC     0x04074	/* Good Packets RX Count - R/clr */
#define E1000_BPRC     0x04078	/* Broadcast Packets RX Count - R/clr */
#define E1000_MPRC     0x0407C	/* Multicast Packets RX Count - R/clr */
#define E1000_GPTC     0x04080	/* Good Packets TX Count - R/clr */
#define E1000_GORCL    0x04088	/* Good Octets RX Count Low - R/clr */
#define E1000_GORCH    0x0408C	/* Good Octets RX Count High - R/clr */
#define E1000_GOTCL    0x04090	/* Good Octets TX Count Low - R/clr */
#define E1000_GOTCH    0x04094	/* Good Octets TX Count High - R/clr */
#define E1000_RNBC     0x040A0	/* RX No Buffers Count - R/clr */
#define E1000_RUC      0x040A4	/* RX Undersize Count - R/clr */
#define E1000_RFC      0x040A8	/* RX Fragment Count - R/clr */
#define E1000_ROC      0x040AC	/* RX Oversize Count - R/clr */
#define E1000_RJC      0x040B0	/* RX Jabber Count - R/clr */
#define E1000_MGTPRC   0x040B4	/* Management Packets RX Count - R/clr */
#define E1000_MGTPDC   0x040B8	/* Management Packets Dropped Count - R/clr */
#define E1000_MGTPTC   0x040BC	/* Management Packets TX Count - R/clr */
#define E1000_TORL     0x040C0	/* Total Octets RX Low - R/clr */
#define E1000_TORH     0x040C4	/* Total Octets RX High - R/clr */
#define E1000_TOTL     0x040C8	/* Total Octets TX Low - R/clr */
#define E1000_TOTH     0x040CC	/* Total Octets TX High - R/clr */
#define E1000_TPR      0x040D0	/* Total Packets RX - R/clr */
#define E1000_TPT      0x040D4	/* Total Packets TX - R/clr */
#define E1000_PTC64    0x040D8	/* Packets TX (64 bytes) - R/clr */
#define E1000_PTC127   0x040DC	/* Packets TX (65-127 bytes) - R/clr */
#define E1000_PTC255   0x040E0	/* Packets TX (128-255 bytes) - R/clr */
#define E1000_PTC511   0x040E4	/* Packets TX (256-511 bytes) - R/clr */
#define E1000_PTC1023  0x040E8	/* Packets TX (512-1023 bytes) - R/clr */
#define E1000_PTC1522  0x040EC	/* Packets TX (1024-1522 Bytes) - R/clr */
#define E1000_MPTC     0x040F0	/* Multicast Packets TX Count - R/clr */
#define E1000_BPTC     0x040F4	/* Broadcast Packets TX Count - R/clr */
#define E1000_TSCTC    0x040F8	/* TCP Segmentation Context TX - R/clr */
#define E1000_TSCTFC   0x040FC	/* TCP Segmentation Context TX Fail - R/clr */
#define E1000_IAC      0x04100	/* Interrupt Assertion Count */
#define E1000_ICRXPTC  0x04104	/* Interrupt Cause Rx Packet Timer Expire Count */
#define E1000_ICRXATC  0x04108	/* Interrupt Cause Rx Absolute Timer Expire Count */
#define E1000_ICTXPTC  0x0410C	/* Interrupt Cause Tx Packet Timer Expire Count */
#define E1000_ICTXATC  0x04110	/* Interrupt Cause Tx Absolute Timer Expire Count */
#define E1000_ICTXQEC  0x04118	/* Interrupt Cause Tx Queue Empty Count */
#define E1000_ICTXQMTC 0x0411C	/* Interrupt Cause Tx Queue Minimum Threshold Count */
#define E1000_ICRXDMTC 0x04120	/* Interrupt Cause Rx Descriptor Minimum Threshold Count */
#define E1000_ICRXOC   0x04124	/* Interrupt Cause Receiver Overrun Count */
#define E1000_RXCSUM   0x05000	/* RX Checksum Control - RW */
#define E1000_RFCTL    0x05008	/* Receive Filter Control */
#define E1000_MTA      0x05200	/* Multicast Table Array - RW Array */
#define E1000_RA       0x05400	/* Receive Address - RW Array */
#define E1000_VFTA     0x05600	/* VLAN Filter Table Array - RW Array */
#define E1000_WUC      0x05800	/* Wakeup Control - RW */
#define E1000_WUFC     0x05808	/* Wakeup Filter Control - RW */
#define E1000_WUS      0x05810	/* Wakeup Status - RO */
#define E1000_MANC     0x05820	/* Management Control - RW */
#define E1000_IPAV     0x05838	/* IP Address Valid - RW */
#define E1000_IP4AT    0x05840	/* IPv4 Address Table - RW Array */
#define E1000_IP6AT    0x05880	/* IPv6 Address Table - RW Array */
#define E1000_WUPL     0x05900	/* Wakeup Packet Length - RW */
#define E1000_WUPM     0x05A00	/* Wakeup Packet Memory - RO A */
#define E1000_FFLT     0x05F00	/* Flexible Filter Length Table - RW Array */
#define E1000_HOST_IF  0x08800	/* Host Interface */
#define E1000_FFMT     0x09000	/* Flexible Filter Mask Table - RW Array */
#define E1000_FFVT     0x09800	/* Flexible Filter Value Table - RW Array */

#define E1000_KUMCTRLSTA 0x00034	/* MAC-PHY interface - RW */
#define E1000_MDPHYA     0x0003C	/* PHY address - RW */
#define E1000_MANC2H     0x05860	/* Managment Control To Host - RW */
#define E1000_SW_FW_SYNC 0x05B5C	/* Software-Firmware Synchronization - RW */

#define E1000_GCR       0x05B00	/* PCI-Ex Control */
#define E1000_GSCL_1    0x05B10	/* PCI-Ex Statistic Control #1 */
#define E1000_GSCL_2    0x05B14	/* PCI-Ex Statistic Control #2 */
#define E1000_GSCL_3    0x05B18	/* PCI-Ex Statistic Control #3 */
#define E1000_GSCL_4    0x05B1C	/* PCI-Ex Statistic Control #4 */
#define E1000_FACTPS    0x05B30	/* Function Active and Power State to MNG */
#define E1000_SWSM      0x05B50	/* SW Semaphore */
#define E1000_FWSM      0x05B54	/* FW Semaphore */
#define E1000_FFLT_DBG  0x05F04	/* Debug Register */
#define E1000_HICR      0x08F00	/* Host Interface Control */

/* RSS registers */
#define E1000_CPUVEC    0x02C10	/* CPU Vector Register - RW */
#define E1000_MRQC      0x05818	/* Multiple Receive Control - RW */
#define E1000_RETA      0x05C00	/* Redirection Table - RW Array */
#define E1000_RSSRK     0x05C80	/* RSS Random Key - RW Array */
#define E1000_RSSIM     0x05864	/* RSS Interrupt Mask */
#define E1000_RSSIR     0x05868	/* RSS Interrupt Request */
/* Register Set (82542)
 *
 * Some of the 82542 registers are located at different offsets than they are
 * in more current versions of the 8254x. Despite the difference in location,
 * the registers function in the same manner.
 */
#define E1000_82542_CTRL     E1000_CTRL
#define E1000_82542_CTRL_DUP E1000_CTRL_DUP
#define E1000_82542_STATUS   E1000_STATUS
#define E1000_82542_EECD     E1000_EECD
#define E1000_82542_EERD     E1000_EERD
#define E1000_82542_CTRL_EXT E1000_CTRL_EXT
#define E1000_82542_FLA      E1000_FLA
#define E1000_82542_MDIC     E1000_MDIC
#define E1000_82542_SCTL     E1000_SCTL
#define E1000_82542_FEXTNVM  E1000_FEXTNVM
#define E1000_82542_FCAL     E1000_FCAL
#define E1000_82542_FCAH     E1000_FCAH
#define E1000_82542_FCT      E1000_FCT
#define E1000_82542_VET      E1000_VET
#define E1000_82542_RA       0x00040
#define E1000_82542_ICR      E1000_ICR
#define E1000_82542_ITR      E1000_ITR
#define E1000_82542_ICS      E1000_ICS
#define E1000_82542_IMS      E1000_IMS
#define E1000_82542_IMC      E1000_IMC
#define E1000_82542_RCTL     E1000_RCTL
#define E1000_82542_RDTR     0x00108
#define E1000_82542_RDBAL    0x00110
#define E1000_82542_RDBAH    0x00114
#define E1000_82542_RDLEN    0x00118
#define E1000_82542_RDH      0x00120
#define E1000_82542_RDT      0x00128
#define E1000_82542_RDTR0    E1000_82542_RDTR
#define E1000_82542_RDBAL0   E1000_82542_RDBAL
#define E1000_82542_RDBAH0   E1000_82542_RDBAH
#define E1000_82542_RDLEN0   E1000_82542_RDLEN
#define E1000_82542_RDH0     E1000_82542_RDH
#define E1000_82542_RDT0     E1000_82542_RDT
#define E1000_82542_SRRCTL(_n) (0x280C + ((_n) << 8))	/* Split and Replication
							 * RX Control - RW */
#define E1000_82542_DCA_RXCTRL(_n) (0x02814 + ((_n) << 8))
#define E1000_82542_RDBAH3   0x02B04	/* RX Desc Base High Queue 3 - RW */
#define E1000_82542_RDBAL3   0x02B00	/* RX Desc Low Queue 3 - RW */
#define E1000_82542_RDLEN3   0x02B08	/* RX Desc Length Queue 3 - RW */
#define E1000_82542_RDH3     0x02B10	/* RX Desc Head Queue 3 - RW */
#define E1000_82542_RDT3     0x02B18	/* RX Desc Tail Queue 3 - RW */
#define E1000_82542_RDBAL2   0x02A00	/* RX Desc Base Low Queue 2 - RW */
#define E1000_82542_RDBAH2   0x02A04	/* RX Desc Base High Queue 2 - RW */
#define E1000_82542_RDLEN2   0x02A08	/* RX Desc Length Queue 2 - RW */
#define E1000_82542_RDH2     0x02A10	/* RX Desc Head Queue 2 - RW */
#define E1000_82542_RDT2     0x02A18	/* RX Desc Tail Queue 2 - RW */
#define E1000_82542_RDTR1    0x00130
#define E1000_82542_RDBAL1   0x00138
#define E1000_82542_RDBAH1   0x0013C
#define E1000_82542_RDLEN1   0x00140
#define E1000_82542_RDH1     0x00148
#define E1000_82542_RDT1     0x00150
#define E1000_82542_FCRTH    0x00160
#define E1000_82542_FCRTL    0x00168
#define E1000_82542_FCTTV    E1000_FCTTV
#define E1000_82542_TXCW     E1000_TXCW
#define E1000_82542_RXCW     E1000_RXCW
#define E1000_82542_MTA      0x00200
#define E1000_82542_TCTL     E1000_TCTL
#define E1000_82542_TCTL_EXT E1000_TCTL_EXT
#define E1000_82542_TIPG     E1000_TIPG
#define E1000_82542_TDBAL    0x00420
#define E1000_82542_TDBAH    0x00424
#define E1000_82542_TDLEN    0x00428
#define E1000_82542_TDH      0x00430
#define E1000_82542_TDT      0x00438
#define E1000_82542_TIDV     0x00440
#define E1000_82542_TBT      E1000_TBT
#define E1000_82542_AIT      E1000_AIT
#define E1000_82542_VFTA     0x00600
#define E1000_82542_LEDCTL   E1000_LEDCTL
#define E1000_82542_PBA      E1000_PBA
#define E1000_82542_PBS      E1000_PBS
#define E1000_82542_EEMNGCTL E1000_EEMNGCTL
#define E1000_82542_EEARBC   E1000_EEARBC
#define E1000_82542_FLASHT   E1000_FLASHT
#define E1000_82542_EEWR     E1000_EEWR
#define E1000_82542_FLSWCTL  E1000_FLSWCTL
#define E1000_82542_FLSWDATA E1000_FLSWDATA
#define E1000_82542_FLSWCNT  E1000_FLSWCNT
#define E1000_82542_FLOP     E1000_FLOP
#define E1000_82542_EXTCNF_CTRL  E1000_EXTCNF_CTRL
#define E1000_82542_EXTCNF_SIZE  E1000_EXTCNF_SIZE
#define E1000_82542_PHY_CTRL E1000_PHY_CTRL
#define E1000_82542_ERT      E1000_ERT
#define E1000_82542_RXDCTL   E1000_RXDCTL
#define E1000_82542_RXDCTL1  E1000_RXDCTL1
#define E1000_82542_RADV     E1000_RADV
#define E1000_82542_RSRPD    E1000_RSRPD
#define E1000_82542_TXDMAC   E1000_TXDMAC
#define E1000_82542_KABGTXD  E1000_KABGTXD
#define E1000_82542_TDFHS    E1000_TDFHS
#define E1000_82542_TDFTS    E1000_TDFTS
#define E1000_82542_TDFPC    E1000_TDFPC
#define E1000_82542_TXDCTL   E1000_TXDCTL
#define E1000_82542_TADV     E1000_TADV
#define E1000_82542_TSPMT    E1000_TSPMT
#define E1000_82542_CRCERRS  E1000_CRCERRS
#define E1000_82542_ALGNERRC E1000_ALGNERRC
#define E1000_82542_SYMERRS  E1000_SYMERRS
#define E1000_82542_RXERRC   E1000_RXERRC
#define E1000_82542_MPC      E1000_MPC
#define E1000_82542_SCC      E1000_SCC
#define E1000_82542_ECOL     E1000_ECOL
#define E1000_82542_MCC      E1000_MCC
#define E1000_82542_LATECOL  E1000_LATECOL
#define E1000_82542_COLC     E1000_COLC
#define E1000_82542_DC       E1000_DC
#define E1000_82542_TNCRS    E1000_TNCRS
#define E1000_82542_SEC      E1000_SEC
#define E1000_82542_CEXTERR  E1000_CEXTERR
#define E1000_82542_RLEC     E1000_RLEC
#define E1000_82542_XONRXC   E1000_XONRXC
#define E1000_82542_XONTXC   E1000_XONTXC
#define E1000_82542_XOFFRXC  E1000_XOFFRXC
#define E1000_82542_XOFFTXC  E1000_XOFFTXC
#define E1000_82542_FCRUC    E1000_FCRUC
#define E1000_82542_PRC64    E1000_PRC64
#define E1000_82542_PRC127   E1000_PRC127
#define E1000_82542_PRC255   E1000_PRC255
#define E1000_82542_PRC511   E1000_PRC511
#define E1000_82542_PRC1023  E1000_PRC1023
#define E1000_82542_PRC1522  E1000_PRC1522
#define E1000_82542_GPRC     E1000_GPRC
#define E1000_82542_BPRC     E1000_BPRC
#define E1000_82542_MPRC     E1000_MPRC
#define E1000_82542_GPTC     E1000_GPTC
#define E1000_82542_GORCL    E1000_GORCL
#define E1000_82542_GORCH    E1000_GORCH
#define E1000_82542_GOTCL    E1000_GOTCL
#define E1000_82542_GOTCH    E1000_GOTCH
#define E1000_82542_RNBC     E1000_RNBC
#define E1000_82542_RUC      E1000_RUC
#define E1000_82542_RFC      E1000_RFC
#define E1000_82542_ROC      E1000_ROC
#define E1000_82542_RJC      E1000_RJC
#define E1000_82542_MGTPRC   E1000_MGTPRC
#define E1000_82542_MGTPDC   E1000_MGTPDC
#define E1000_82542_MGTPTC   E1000_MGTPTC
#define E1000_82542_TORL     E1000_TORL
#define E1000_82542_TORH     E1000_TORH
#define E1000_82542_TOTL     E1000_TOTL
#define E1000_82542_TOTH     E1000_TOTH
#define E1000_82542_TPR      E1000_TPR
#define E1000_82542_TPT      E1000_TPT
#define E1000_82542_PTC64    E1000_PTC64
#define E1000_82542_PTC127   E1000_PTC127
#define E1000_82542_PTC255   E1000_PTC255
#define E1000_82542_PTC511   E1000_PTC511
#define E1000_82542_PTC1023  E1000_PTC1023
#define E1000_82542_PTC1522  E1000_PTC1522
#define E1000_82542_MPTC     E1000_MPTC
#define E1000_82542_BPTC     E1000_BPTC
#define E1000_82542_TSCTC    E1000_TSCTC
#define E1000_82542_TSCTFC   E1000_TSCTFC
#define E1000_82542_RXCSUM   E1000_RXCSUM
#define E1000_82542_WUC      E1000_WUC
#define E1000_82542_WUFC     E1000_WUFC
#define E1000_82542_WUS      E1000_WUS
#define E1000_82542_MANC     E1000_MANC
#define E1000_82542_IPAV     E1000_IPAV
#define E1000_82542_IP4AT    E1000_IP4AT
#define E1000_82542_IP6AT    E1000_IP6AT
#define E1000_82542_WUPL     E1000_WUPL
#define E1000_82542_WUPM     E1000_WUPM
#define E1000_82542_FFLT     E1000_FFLT
#define E1000_82542_TDFH     0x08010
#define E1000_82542_TDFT     0x08018
#define E1000_82542_FFMT     E1000_FFMT
#define E1000_82542_FFVT     E1000_FFVT
#define E1000_82542_HOST_IF  E1000_HOST_IF
#define E1000_82542_IAM         E1000_IAM
#define E1000_82542_EEMNGCTL    E1000_EEMNGCTL
#define E1000_82542_PSRCTL      E1000_PSRCTL
#define E1000_82542_RAID        E1000_RAID
#define E1000_82542_TARC0       E1000_TARC0
#define E1000_82542_TDBAL1      E1000_TDBAL1
#define E1000_82542_TDBAH1      E1000_TDBAH1
#define E1000_82542_TDLEN1      E1000_TDLEN1
#define E1000_82542_TDH1        E1000_TDH1
#define E1000_82542_TDT1        E1000_TDT1
#define E1000_82542_TXDCTL1     E1000_TXDCTL1
#define E1000_82542_TARC1       E1000_TARC1
#define E1000_82542_RFCTL       E1000_RFCTL
#define E1000_82542_GCR         E1000_GCR
#define E1000_82542_GSCL_1      E1000_GSCL_1
#define E1000_82542_GSCL_2      E1000_GSCL_2
#define E1000_82542_GSCL_3      E1000_GSCL_3
#define E1000_82542_GSCL_4      E1000_GSCL_4
#define E1000_82542_FACTPS      E1000_FACTPS
#define E1000_82542_SWSM        E1000_SWSM
#define E1000_82542_FWSM        E1000_FWSM
#define E1000_82542_FFLT_DBG    E1000_FFLT_DBG
#define E1000_82542_IAC         E1000_IAC
#define E1000_82542_ICRXPTC     E1000_ICRXPTC
#define E1000_82542_ICRXATC     E1000_ICRXATC
#define E1000_82542_ICTXPTC     E1000_ICTXPTC
#define E1000_82542_ICTXATC     E1000_ICTXATC
#define E1000_82542_ICTXQEC     E1000_ICTXQEC
#define E1000_82542_ICTXQMTC    E1000_ICTXQMTC
#define E1000_82542_ICRXDMTC    E1000_ICRXDMTC
#define E1000_82542_ICRXOC      E1000_ICRXOC
#define E1000_82542_HICR        E1000_HICR

#define E1000_82542_CPUVEC      E1000_CPUVEC
#define E1000_82542_MRQC        E1000_MRQC
#define E1000_82542_RETA        E1000_RETA
#define E1000_82542_RSSRK       E1000_RSSRK
#define E1000_82542_RSSIM       E1000_RSSIM
#define E1000_82542_RSSIR       E1000_RSSIR
#define E1000_82542_KUMCTRLSTA E1000_KUMCTRLSTA
#define E1000_82542_SW_FW_SYNC E1000_SW_FW_SYNC

/* Statistics counters collected by the MAC */
struct e1000_hw_stats {
	u64 crcerrs;
	u64 algnerrc;
	u64 symerrs;
	u64 rxerrc;
	u64 txerrc;
	u64 mpc;
	u64 scc;
	u64 ecol;
	u64 mcc;
	u64 latecol;
	u64 colc;
	u64 dc;
	u64 tncrs;
	u64 sec;
	u64 cexterr;
	u64 rlec;
	u64 xonrxc;
	u64 xontxc;
	u64 xoffrxc;
	u64 xofftxc;
	u64 fcruc;
	u64 prc64;
	u64 prc127;
	u64 prc255;
	u64 prc511;
	u64 prc1023;
	u64 prc1522;
	u64 gprc;
	u64 bprc;
	u64 mprc;
	u64 gptc;
	u64 gorcl;
	u64 gorch;
	u64 gotcl;
	u64 gotch;
	u64 rnbc;
	u64 ruc;
	u64 rfc;
	u64 roc;
	u64 rlerrc;
	u64 rjc;
	u64 mgprc;
	u64 mgpdc;
	u64 mgptc;
	u64 torl;
	u64 torh;
	u64 totl;
	u64 toth;
	u64 tpr;
	u64 tpt;
	u64 ptc64;
	u64 ptc127;
	u64 ptc255;
	u64 ptc511;
	u64 ptc1023;
	u64 ptc1522;
	u64 mptc;
	u64 bptc;
	u64 tsctc;
	u64 tsctfc;
	u64 iac;
	u64 icrxptc;
	u64 icrxatc;
	u64 ictxptc;
	u64 ictxatc;
	u64 ictxqec;
	u64 ictxqmtc;
	u64 icrxdmtc;
	u64 icrxoc;
};

/* Structure containing variables used by the shared code (e1000_hw.c) */
struct e1000_hw {
	u8 __iomem *hw_addr;
	u8 __iomem *flash_address;
	e1000_mac_type mac_type;
	e1000_phy_type phy_type;
	u32 phy_init_script;
	e1000_media_type media_type;
	void *back;
	struct e1000_shadow_ram *eeprom_shadow_ram;
	u32 flash_bank_size;
	u32 flash_base_addr;
	e1000_fc_type fc;
	e1000_bus_speed bus_speed;
	e1000_bus_width bus_width;
	e1000_bus_type bus_type;
	struct e1000_eeprom_info eeprom;
	e1000_ms_type master_slave;
	e1000_ms_type original_master_slave;
	e1000_ffe_config ffe_config_state;
	u32 asf_firmware_present;
	u32 eeprom_semaphore_present;
	unsigned long io_base;
	u32 phy_id;
	u32 phy_revision;
	u32 phy_addr;
	u32 original_fc;
	u32 txcw;
	u32 autoneg_failed;
	u32 max_frame_size;
	u32 min_frame_size;
	u32 mc_filter_type;
	u32 num_mc_addrs;
	u32 collision_delta;
	u32 tx_packet_delta;
	u32 ledctl_default;
	u32 ledctl_mode1;
	u32 ledctl_mode2;
	bool tx_pkt_filtering;
	struct e1000_host_mng_dhcp_cookie mng_cookie;
	u16 phy_spd_default;
	u16 autoneg_advertised;
	u16 pci_cmd_word;
	u16 fc_high_water;
	u16 fc_low_water;
	u16 fc_pause_time;
	u16 current_ifs_val;
	u16 ifs_min_val;
	u16 ifs_max_val;
	u16 ifs_step_size;
	u16 ifs_ratio;
	u16 device_id;
	u16 vendor_id;
	u16 subsystem_id;
	u16 subsystem_vendor_id;
	u8 revision_id;
	u8 autoneg;
	u8 mdix;
	u8 forced_speed_duplex;
	u8 wait_autoneg_complete;
	u8 dma_fairness;
	u8 mac_addr[NODE_ADDRESS_SIZE];
	u8 perm_mac_addr[NODE_ADDRESS_SIZE];
	bool disable_polarity_correction;
	bool speed_downgraded;
	e1000_smart_speed smart_speed;
	e1000_dsp_config dsp_config_state;
	bool get_link_status;
	bool serdes_has_link;
	bool tbi_compatibility_en;
	bool tbi_compatibility_on;
	bool laa_is_present;
	bool phy_reset_disable;
	bool initialize_hw_bits_disable;
	bool fc_send_xon;
	bool fc_strict_ieee;
	bool report_tx_early;
	bool adaptive_ifs;
	bool ifs_params_forced;
	bool in_ifs_mode;
	bool mng_reg_access_disabled;
	bool leave_av_bit_off;
	bool bad_tx_carr_stats_fd;
	bool has_smbus;
};

#define E1000_EEPROM_SWDPIN0   0x0001	/* SWDPIN 0 EEPROM Value */
#define E1000_EEPROM_LED_LOGIC 0x0020	/* Led Logic Word */
#define E1000_EEPROM_RW_REG_DATA   16	/* Offset to data in EEPROM read/write registers */
#define E1000_EEPROM_RW_REG_DONE   2	/* Offset to READ/WRITE done bit */
#define E1000_EEPROM_RW_REG_START  1	/* First bit for telling part to start operation */
#define E1000_EEPROM_RW_ADDR_SHIFT 2	/* Shift to the address bits */
#define E1000_EEPROM_POLL_WRITE    1	/* Flag for polling for write complete */
#define E1000_EEPROM_POLL_READ     0	/* Flag for polling for read complete */
/* Register Bit Masks */
/* Device Control */
#define E1000_CTRL_FD       0x00000001	/* Full duplex.0=half; 1=full */
#define E1000_CTRL_BEM      0x00000002	/* Endian Mode.0=little,1=big */
#define E1000_CTRL_PRIOR    0x00000004	/* Priority on PCI. 0=rx,1=fair */
#define E1000_CTRL_GIO_MASTER_DISABLE 0x00000004	/*Blocks new Master requests */
#define E1000_CTRL_LRST     0x00000008	/* Link reset. 0=normal,1=reset */
#define E1000_CTRL_TME      0x00000010	/* Test mode. 0=normal,1=test */
#define E1000_CTRL_SLE      0x00000020	/* Serial Link on 0=dis,1=en */
#define E1000_CTRL_ASDE     0x00000020	/* Auto-speed detect enable */
#define E1000_CTRL_SLU      0x00000040	/* Set link up (Force Link) */
#define E1000_CTRL_ILOS     0x00000080	/* Invert Loss-Of Signal */
#define E1000_CTRL_SPD_SEL  0x00000300	/* Speed Select Mask */
#define E1000_CTRL_SPD_10   0x00000000	/* Force 10Mb */
#define E1000_CTRL_SPD_100  0x00000100	/* Force 100Mb */
#define E1000_CTRL_SPD_1000 0x00000200	/* Force 1Gb */
#define E1000_CTRL_BEM32    0x00000400	/* Big Endian 32 mode */
#define E1000_CTRL_FRCSPD   0x00000800	/* Force Speed */
#define E1000_CTRL_FRCDPX   0x00001000	/* Force Duplex */
#define E1000_CTRL_D_UD_EN  0x00002000	/* Dock/Undock enable */
#define E1000_CTRL_D_UD_POLARITY 0x00004000	/* Defined polarity of Dock/Undock indication in SDP[0] */
#define E1000_CTRL_FORCE_PHY_RESET 0x00008000	/* Reset both PHY ports, through PHYRST_N pin */
#define E1000_CTRL_EXT_LINK_EN 0x00010000	/* enable link status from external LINK_0 and LINK_1 pins */
#define E1000_CTRL_SWDPIN0  0x00040000	/* SWDPIN 0 value */
#define E1000_CTRL_SWDPIN1  0x00080000	/* SWDPIN 1 value */
#define E1000_CTRL_SWDPIN2  0x00100000	/* SWDPIN 2 value */
#define E1000_CTRL_SWDPIN3  0x00200000	/* SWDPIN 3 value */
#define E1000_CTRL_SWDPIO0  0x00400000	/* SWDPIN 0 Input or output */
#define E1000_CTRL_SWDPIO1  0x00800000	/* SWDPIN 1 input or output */
#define E1000_CTRL_SWDPIO2  0x01000000	/* SWDPIN 2 input or output */
#define E1000_CTRL_SWDPIO3  0x02000000	/* SWDPIN 3 input or output */
#define E1000_CTRL_RST      0x04000000	/* Global reset */
#define E1000_CTRL_RFCE     0x08000000	/* Receive Flow Control enable */
#define E1000_CTRL_TFCE     0x10000000	/* Transmit flow control enable */
#define E1000_CTRL_RTE      0x20000000	/* Routing tag enable */
#define E1000_CTRL_VME      0x40000000	/* IEEE VLAN mode enable */
#define E1000_CTRL_PHY_RST  0x80000000	/* PHY Reset */
#define E1000_CTRL_SW2FW_INT 0x02000000	/* Initiate an interrupt to manageability engine */

/* Device Status */
#define E1000_STATUS_FD         0x00000001	/* Full duplex.0=half,1=full */
#define E1000_STATUS_LU         0x00000002	/* Link up.0=no,1=link */
#define E1000_STATUS_FUNC_MASK  0x0000000C	/* PCI Function Mask */
#define E1000_STATUS_FUNC_SHIFT 2
#define E1000_STATUS_FUNC_0     0x00000000	/* Function 0 */
#define E1000_STATUS_FUNC_1     0x00000004	/* Function 1 */
#define E1000_STATUS_TXOFF      0x00000010	/* transmission paused */
#define E1000_STATUS_TBIMODE    0x00000020	/* TBI mode */
#define E1000_STATUS_SPEED_MASK 0x000000C0
#define E1000_STATUS_SPEED_10   0x00000000	/* Speed 10Mb/s */
#define E1000_STATUS_SPEED_100  0x00000040	/* Speed 100Mb/s */
#define E1000_STATUS_SPEED_1000 0x00000080	/* Speed 1000Mb/s */
#define E1000_STATUS_LAN_INIT_DONE 0x00000200	/* Lan Init Completion
						   by EEPROM/Flash */
#define E1000_STATUS_ASDV       0x00000300	/* Auto speed detect value */
#define E1000_STATUS_DOCK_CI    0x00000800	/* Change in Dock/Undock state. Clear on write '0'. */
#define E1000_STATUS_GIO_MASTER_ENABLE 0x00080000	/* Status of Master requests. */
#define E1000_STATUS_MTXCKOK    0x00000400	/* MTX clock running OK */
#define E1000_STATUS_PCI66      0x00000800	/* In 66Mhz slot */
#define E1000_STATUS_BUS64      0x00001000	/* In 64 bit slot */
#define E1000_STATUS_PCIX_MODE  0x00002000	/* PCI-X mode */
#define E1000_STATUS_PCIX_SPEED 0x0000C000	/* PCI-X bus speed */
#define E1000_STATUS_BMC_SKU_0  0x00100000	/* BMC USB redirect disabled */
#define E1000_STATUS_BMC_SKU_1  0x00200000	/* BMC SRAM disabled */
#define E1000_STATUS_BMC_SKU_2  0x00400000	/* BMC SDRAM disabled */
#define E1000_STATUS_BMC_CRYPTO 0x00800000	/* BMC crypto disabled */
#define E1000_STATUS_BMC_LITE   0x01000000	/* BMC external code execution disabled */
#define E1000_STATUS_RGMII_ENABLE 0x02000000	/* RGMII disabled */
#define E1000_STATUS_FUSE_8       0x04000000
#define E1000_STATUS_FUSE_9       0x08000000
#define E1000_STATUS_SERDES0_DIS  0x10000000	/* SERDES disabled on port 0 */
#define E1000_STATUS_SERDES1_DIS  0x20000000	/* SERDES disabled on port 1 */

/* Constants used to interpret the masked PCI-X bus speed. */
#define E1000_STATUS_PCIX_SPEED_66  0x00000000	/* PCI-X bus speed  50-66 MHz */
#define E1000_STATUS_PCIX_SPEED_100 0x00004000	/* PCI-X bus speed  66-100 MHz */
#define E1000_STATUS_PCIX_SPEED_133 0x00008000	/* PCI-X bus speed 100-133 MHz */

/* EEPROM/Flash Control */
#define E1000_EECD_SK        0x00000001	/* EEPROM Clock */
#define E1000_EECD_CS        0x00000002	/* EEPROM Chip Select */
#define E1000_EECD_DI        0x00000004	/* EEPROM Data In */
#define E1000_EECD_DO        0x00000008	/* EEPROM Data Out */
#define E1000_EECD_FWE_MASK  0x00000030
#define E1000_EECD_FWE_DIS   0x00000010	/* Disable FLASH writes */
#define E1000_EECD_FWE_EN    0x00000020	/* Enable FLASH writes */
#define E1000_EECD_FWE_SHIFT 4
#define E1000_EECD_REQ       0x00000040	/* EEPROM Access Request */
#define E1000_EECD_GNT       0x00000080	/* EEPROM Access Grant */
#define E1000_EECD_PRES      0x00000100	/* EEPROM Present */
#define E1000_EECD_SIZE      0x00000200	/* EEPROM Size (0=64 word 1=256 word) */
#define E1000_EECD_ADDR_BITS 0x00000400	/* EEPROM Addressing bits based on type
					 * (0-small, 1-large) */
#define E1000_EECD_TYPE      0x00002000	/* EEPROM Type (1-SPI, 0-Microwire) */
#ifndef E1000_EEPROM_GRANT_ATTEMPTS
#define E1000_EEPROM_GRANT_ATTEMPTS 1000	/* EEPROM # attempts to gain grant */
#endif
#define E1000_EECD_AUTO_RD          0x00000200	/* EEPROM Auto Read done */
#define E1000_EECD_SIZE_EX_MASK     0x00007800	/* EEprom Size */
#define E1000_EECD_SIZE_EX_SHIFT    11
#define E1000_EECD_NVADDS    0x00018000	/* NVM Address Size */
#define E1000_EECD_SELSHAD   0x00020000	/* Select Shadow RAM */
#define E1000_EECD_INITSRAM  0x00040000	/* Initialize Shadow RAM */
#define E1000_EECD_FLUPD     0x00080000	/* Update FLASH */
#define E1000_EECD_AUPDEN    0x00100000	/* Enable Autonomous FLASH update */
#define E1000_EECD_SHADV     0x00200000	/* Shadow RAM Data Valid */
#define E1000_EECD_SEC1VAL   0x00400000	/* Sector One Valid */
#define E1000_EECD_SECVAL_SHIFT      22
#define E1000_STM_OPCODE     0xDB00
#define E1000_HICR_FW_RESET  0xC0

#define E1000_SHADOW_RAM_WORDS     2048
#define E1000_ICH_NVM_SIG_WORD     0x13
#define E1000_ICH_NVM_SIG_MASK     0xC0

/* EEPROM Read */
#define E1000_EERD_START      0x00000001	/* Start Read */
#define E1000_EERD_DONE       0x00000010	/* Read Done */
#define E1000_EERD_ADDR_SHIFT 8
#define E1000_EERD_ADDR_MASK  0x0000FF00	/* Read Address */
#define E1000_EERD_DATA_SHIFT 16
#define E1000_EERD_DATA_MASK  0xFFFF0000	/* Read Data */

/* SPI EEPROM Status Register */
#define EEPROM_STATUS_RDY_SPI  0x01
#define EEPROM_STATUS_WEN_SPI  0x02
#define EEPROM_STATUS_BP0_SPI  0x04
#define EEPROM_STATUS_BP1_SPI  0x08
#define EEPROM_STATUS_WPEN_SPI 0x80

/* Extended Device Control */
#define E1000_CTRL_EXT_GPI0_EN   0x00000001	/* Maps SDP4 to GPI0 */
#define E1000_CTRL_EXT_GPI1_EN   0x00000002	/* Maps SDP5 to GPI1 */
#define E1000_CTRL_EXT_PHYINT_EN E1000_CTRL_EXT_GPI1_EN
#define E1000_CTRL_EXT_GPI2_EN   0x00000004	/* Maps SDP6 to GPI2 */
#define E1000_CTRL_EXT_GPI3_EN   0x00000008	/* Maps SDP7 to GPI3 */
#define E1000_CTRL_EXT_SDP4_DATA 0x00000010	/* Value of SW Defineable Pin 4 */
#define E1000_CTRL_EXT_SDP5_DATA 0x00000020	/* Value of SW Defineable Pin 5 */
#define E1000_CTRL_EXT_PHY_INT   E1000_CTRL_EXT_SDP5_DATA
#define E1000_CTRL_EXT_SDP6_DATA 0x00000040	/* Value of SW Defineable Pin 6 */
#define E1000_CTRL_EXT_SDP7_DATA 0x00000080	/* Value of SW Defineable Pin 7 */
#define E1000_CTRL_EXT_SDP4_DIR  0x00000100	/* Direction of SDP4 0=in 1=out */
#define E1000_CTRL_EXT_SDP5_DIR  0x00000200	/* Direction of SDP5 0=in 1=out */
#define E1000_CTRL_EXT_SDP6_DIR  0x00000400	/* Direction of SDP6 0=in 1=out */
#define E1000_CTRL_EXT_SDP7_DIR  0x00000800	/* Direction of SDP7 0=in 1=out */
#define E1000_CTRL_EXT_ASDCHK    0x00001000	/* Initiate an ASD sequence */
#define E1000_CTRL_EXT_EE_RST    0x00002000	/* Reinitialize from EEPROM */
#define E1000_CTRL_EXT_IPS       0x00004000	/* Invert Power State */
#define E1000_CTRL_EXT_SPD_BYPS  0x00008000	/* Speed Select Bypass */
#define E1000_CTRL_EXT_RO_DIS    0x00020000	/* Relaxed Ordering disable */
#define E1000_CTRL_EXT_LINK_MODE_MASK 0x00C00000
#define E1000_CTRL_EXT_LINK_MODE_GMII 0x00000000
#define E1000_CTRL_EXT_LINK_MODE_TBI  0x00C00000
#define E1000_CTRL_EXT_LINK_MODE_KMRN 0x00000000
#define E1000_CTRL_EXT_LINK_MODE_SERDES  0x00C00000
#define E1000_CTRL_EXT_LINK_MODE_SGMII   0x00800000
#define E1000_CTRL_EXT_WR_WMARK_MASK  0x03000000
#define E1000_CTRL_EXT_WR_WMARK_256   0x00000000
#define E1000_CTRL_EXT_WR_WMARK_320   0x01000000
#define E1000_CTRL_EXT_WR_WMARK_384   0x02000000
#define E1000_CTRL_EXT_WR_WMARK_448   0x03000000
#define E1000_CTRL_EXT_DRV_LOAD       0x10000000	/* Driver loaded bit for FW */
#define E1000_CTRL_EXT_IAME           0x08000000	/* Interrupt acknowledge Auto-mask */
#define E1000_CTRL_EXT_INT_TIMER_CLR  0x20000000	/* Clear Interrupt timers after IMS clear */
#define E1000_CRTL_EXT_PB_PAREN       0x01000000	/* packet buffer parity error detection enabled */
#define E1000_CTRL_EXT_DF_PAREN       0x02000000	/* descriptor FIFO parity error detection enable */
#define E1000_CTRL_EXT_GHOST_PAREN    0x40000000

/* MDI Control */
#define E1000_MDIC_DATA_MASK 0x0000FFFF
#define E1000_MDIC_REG_MASK  0x001F0000
#define E1000_MDIC_REG_SHIFT 16
#define E1000_MDIC_PHY_MASK  0x03E00000
#define E1000_MDIC_PHY_SHIFT 21
#define E1000_MDIC_OP_WRITE  0x04000000
#define E1000_MDIC_OP_READ   0x08000000
#define E1000_MDIC_READY     0x10000000
#define E1000_MDIC_INT_EN    0x20000000
#define E1000_MDIC_ERROR     0x40000000

#define E1000_KUMCTRLSTA_MASK           0x0000FFFF
#define E1000_KUMCTRLSTA_OFFSET         0x001F0000
#define E1000_KUMCTRLSTA_OFFSET_SHIFT   16
#define E1000_KUMCTRLSTA_REN            0x00200000

#define E1000_KUMCTRLSTA_OFFSET_FIFO_CTRL      0x00000000
#define E1000_KUMCTRLSTA_OFFSET_CTRL           0x00000001
#define E1000_KUMCTRLSTA_OFFSET_INB_CTRL       0x00000002
#define E1000_KUMCTRLSTA_OFFSET_DIAG           0x00000003
#define E1000_KUMCTRLSTA_OFFSET_TIMEOUTS       0x00000004
#define E1000_KUMCTRLSTA_OFFSET_INB_PARAM      0x00000009
#define E1000_KUMCTRLSTA_OFFSET_HD_CTRL        0x00000010
#define E1000_KUMCTRLSTA_OFFSET_M2P_SERDES     0x0000001E
#define E1000_KUMCTRLSTA_OFFSET_M2P_MODES      0x0000001F

/* FIFO Control */
#define E1000_KUMCTRLSTA_FIFO_CTRL_RX_BYPASS   0x00000008
#define E1000_KUMCTRLSTA_FIFO_CTRL_TX_BYPASS   0x00000800

/* In-Band Control */
#define E1000_KUMCTRLSTA_INB_CTRL_LINK_STATUS_TX_TIMEOUT_DEFAULT    0x00000500
#define E1000_KUMCTRLSTA_INB_CTRL_DIS_PADDING  0x00000010

/* Half-Duplex Control */
#define E1000_KUMCTRLSTA_HD_CTRL_10_100_DEFAULT 0x00000004
#define E1000_KUMCTRLSTA_HD_CTRL_1000_DEFAULT  0x00000000

#define E1000_KUMCTRLSTA_OFFSET_K0S_CTRL       0x0000001E

#define E1000_KUMCTRLSTA_DIAG_FELPBK           0x2000
#define E1000_KUMCTRLSTA_DIAG_NELPBK           0x1000

#define E1000_KUMCTRLSTA_K0S_100_EN            0x2000
#define E1000_KUMCTRLSTA_K0S_GBE_EN            0x1000
#define E1000_KUMCTRLSTA_K0S_ENTRY_LATENCY_MASK   0x0003

#define E1000_KABGTXD_BGSQLBIAS                0x00050000

#define E1000_PHY_CTRL_SPD_EN                  0x00000001
#define E1000_PHY_CTRL_D0A_LPLU                0x00000002
#define E1000_PHY_CTRL_NOND0A_LPLU             0x00000004
#define E1000_PHY_CTRL_NOND0A_GBE_DISABLE      0x00000008
#define E1000_PHY_CTRL_GBE_DISABLE             0x00000040
#define E1000_PHY_CTRL_B2B_EN                  0x00000080

/* LED Control */
#define E1000_LEDCTL_LED0_MODE_MASK       0x0000000F
#define E1000_LEDCTL_LED0_MODE_SHIFT      0
#define E1000_LEDCTL_LED0_BLINK_RATE      0x0000020
#define E1000_LEDCTL_LED0_IVRT            0x00000040
#define E1000_LEDCTL_LED0_BLINK           0x00000080
#define E1000_LEDCTL_LED1_MODE_MASK       0x00000F00
#define E1000_LEDCTL_LED1_MODE_SHIFT      8
#define E1000_LEDCTL_LED1_BLINK_RATE      0x0002000
#define E1000_LEDCTL_LED1_IVRT            0x00004000
#define E1000_LEDCTL_LED1_BLINK           0x00008000
#define E1000_LEDCTL_LED2_MODE_MASK       0x000F0000
#define E1000_LEDCTL_LED2_MODE_SHIFT      16
#define E1000_LEDCTL_LED2_BLINK_RATE      0x00200000
#define E1000_LEDCTL_LED2_IVRT            0x00400000
#define E1000_LEDCTL_LED2_BLINK           0x00800000
#define E1000_LEDCTL_LED3_MODE_MASK       0x0F000000
#define E1000_LEDCTL_LED3_MODE_SHIFT      24
#define E1000_LEDCTL_LED3_BLINK_RATE      0x20000000
#define E1000_LEDCTL_LED3_IVRT            0x40000000
#define E1000_LEDCTL_LED3_BLINK           0x80000000

#define E1000_LEDCTL_MODE_LINK_10_1000  0x0
#define E1000_LEDCTL_MODE_LINK_100_1000 0x1
#define E1000_LEDCTL_MODE_LINK_UP       0x2
#define E1000_LEDCTL_MODE_ACTIVITY      0x3
#define E1000_LEDCTL_MODE_LINK_ACTIVITY 0x4
#define E1000_LEDCTL_MODE_LINK_10       0x5
#define E1000_LEDCTL_MODE_LINK_100      0x6
#define E1000_LEDCTL_MODE_LINK_1000     0x7
#define E1000_LEDCTL_MODE_PCIX_MODE     0x8
#define E1000_LEDCTL_MODE_FULL_DUPLEX   0x9
#define E1000_LEDCTL_MODE_COLLISION     0xA
#define E1000_LEDCTL_MODE_BUS_SPEED     0xB
#define E1000_LEDCTL_MODE_BUS_SIZE      0xC
#define E1000_LEDCTL_MODE_PAUSED        0xD
#define E1000_LEDCTL_MODE_LED_ON        0xE
#define E1000_LEDCTL_MODE_LED_OFF       0xF

/* Receive Address */
#define E1000_RAH_AV  0x80000000	/* Receive descriptor valid */

/* Interrupt Cause Read */
#define E1000_ICR_TXDW          0x00000001	/* Transmit desc written back */
#define E1000_ICR_TXQE          0x00000002	/* Transmit Queue empty */
#define E1000_ICR_LSC           0x00000004	/* Link Status Change */
#define E1000_ICR_RXSEQ         0x00000008	/* rx sequence error */
#define E1000_ICR_RXDMT0        0x00000010	/* rx desc min. threshold (0) */
#define E1000_ICR_RXO           0x00000040	/* rx overrun */
#define E1000_ICR_RXT0          0x00000080	/* rx timer intr (ring 0) */
#define E1000_ICR_MDAC          0x00000200	/* MDIO access complete */
#define E1000_ICR_RXCFG         0x00000400	/* RX /c/ ordered set */
#define E1000_ICR_GPI_EN0       0x00000800	/* GP Int 0 */
#define E1000_ICR_GPI_EN1       0x00001000	/* GP Int 1 */
#define E1000_ICR_GPI_EN2       0x00002000	/* GP Int 2 */
#define E1000_ICR_GPI_EN3       0x00004000	/* GP Int 3 */
#define E1000_ICR_TXD_LOW       0x00008000
#define E1000_ICR_SRPD          0x00010000
#define E1000_ICR_ACK           0x00020000	/* Receive Ack frame */
#define E1000_ICR_MNG           0x00040000	/* Manageability event */
#define E1000_ICR_DOCK          0x00080000	/* Dock/Undock */
#define E1000_ICR_INT_ASSERTED  0x80000000	/* If this bit asserted, the driver should claim the interrupt */
#define E1000_ICR_RXD_FIFO_PAR0 0x00100000	/* queue 0 Rx descriptor FIFO parity error */
#define E1000_ICR_TXD_FIFO_PAR0 0x00200000	/* queue 0 Tx descriptor FIFO parity error */
#define E1000_ICR_HOST_ARB_PAR  0x00400000	/* host arb read buffer parity error */
#define E1000_ICR_PB_PAR        0x00800000	/* packet buffer parity error */
#define E1000_ICR_RXD_FIFO_PAR1 0x01000000	/* queue 1 Rx descriptor FIFO parity error */
#define E1000_ICR_TXD_FIFO_PAR1 0x02000000	/* queue 1 Tx descriptor FIFO parity error */
#define E1000_ICR_ALL_PARITY    0x03F00000	/* all parity error bits */
#define E1000_ICR_DSW           0x00000020	/* FW changed the status of DISSW bit in the FWSM */
#define E1000_ICR_PHYINT        0x00001000	/* LAN connected device generates an interrupt */
#define E1000_ICR_EPRST         0x00100000	/* ME hardware reset occurs */

/* Interrupt Cause Set */
#define E1000_ICS_TXDW      E1000_ICR_TXDW	/* Transmit desc written back */
#define E1000_ICS_TXQE      E1000_ICR_TXQE	/* Transmit Queue empty */
#define E1000_ICS_LSC       E1000_ICR_LSC	/* Link Status Change */
#define E1000_ICS_RXSEQ     E1000_ICR_RXSEQ	/* rx sequence error */
#define E1000_ICS_RXDMT0    E1000_ICR_RXDMT0	/* rx desc min. threshold */
#define E1000_ICS_RXO       E1000_ICR_RXO	/* rx overrun */
#define E1000_ICS_RXT0      E1000_ICR_RXT0	/* rx timer intr */
#define E1000_ICS_MDAC      E1000_ICR_MDAC	/* MDIO access complete */
#define E1000_ICS_RXCFG     E1000_ICR_RXCFG	/* RX /c/ ordered set */
#define E1000_ICS_GPI_EN0   E1000_ICR_GPI_EN0	/* GP Int 0 */
#define E1000_ICS_GPI_EN1   E1000_ICR_GPI_EN1	/* GP Int 1 */
#define E1000_ICS_GPI_EN2   E1000_ICR_GPI_EN2	/* GP Int 2 */
#define E1000_ICS_GPI_EN3   E1000_ICR_GPI_EN3	/* GP Int 3 */
#define E1000_ICS_TXD_LOW   E1000_ICR_TXD_LOW
#define E1000_ICS_SRPD      E1000_ICR_SRPD
#define E1000_ICS_ACK       E1000_ICR_ACK	/* Receive Ack frame */
#define E1000_ICS_MNG       E1000_ICR_MNG	/* Manageability event */
#define E1000_ICS_DOCK      E1000_ICR_DOCK	/* Dock/Undock */
#define E1000_ICS_RXD_FIFO_PAR0 E1000_ICR_RXD_FIFO_PAR0	/* queue 0 Rx descriptor FIFO parity error */
#define E1000_ICS_TXD_FIFO_PAR0 E1000_ICR_TXD_FIFO_PAR0	/* queue 0 Tx descriptor FIFO parity error */
#define E1000_ICS_HOST_ARB_PAR  E1000_ICR_HOST_ARB_PAR	/* host arb read buffer parity error */
#define E1000_ICS_PB_PAR        E1000_ICR_PB_PAR	/* packet buffer parity error */
#define E1000_ICS_RXD_FIFO_PAR1 E1000_ICR_RXD_FIFO_PAR1	/* queue 1 Rx descriptor FIFO parity error */
#define E1000_ICS_TXD_FIFO_PAR1 E1000_ICR_TXD_FIFO_PAR1	/* queue 1 Tx descriptor FIFO parity error */
#define E1000_ICS_DSW       E1000_ICR_DSW
#define E1000_ICS_PHYINT    E1000_ICR_PHYINT
#define E1000_ICS_EPRST     E1000_ICR_EPRST

/* Interrupt Mask Set */
#define E1000_IMS_TXDW      E1000_ICR_TXDW	/* Transmit desc written back */
#define E1000_IMS_TXQE      E1000_ICR_TXQE	/* Transmit Queue empty */
#define E1000_IMS_LSC       E1000_ICR_LSC	/* Link Status Change */
#define E1000_IMS_RXSEQ     E1000_ICR_RXSEQ	/* rx sequence error */
#define E1000_IMS_RXDMT0    E1000_ICR_RXDMT0	/* rx desc min. threshold */
#define E1000_IMS_RXO       E1000_ICR_RXO	/* rx overrun */
#define E1000_IMS_RXT0      E1000_ICR_RXT0	/* rx timer intr */
#define E1000_IMS_MDAC      E1000_ICR_MDAC	/* MDIO access complete */
#define E1000_IMS_RXCFG     E1000_ICR_RXCFG	/* RX /c/ ordered set */
#define E1000_IMS_GPI_EN0   E1000_ICR_GPI_EN0	/* GP Int 0 */
#define E1000_IMS_GPI_EN1   E1000_ICR_GPI_EN1	/* GP Int 1 */
#define E1000_IMS_GPI_EN2   E1000_ICR_GPI_EN2	/* GP Int 2 */
#define E1000_IMS_GPI_EN3   E1000_ICR_GPI_EN3	/* GP Int 3 */
#define E1000_IMS_TXD_LOW   E1000_ICR_TXD_LOW
#define E1000_IMS_SRPD      E1000_ICR_SRPD
#define E1000_IMS_ACK       E1000_ICR_ACK	/* Receive Ack frame */
#define E1000_IMS_MNG       E1000_ICR_MNG	/* Manageability event */
#define E1000_IMS_DOCK      E1000_ICR_DOCK	/* Dock/Undock */
#define E1000_IMS_RXD_FIFO_PAR0 E1000_ICR_RXD_FIFO_PAR0	/* queue 0 Rx descriptor FIFO parity error */
#define E1000_IMS_TXD_FIFO_PAR0 E1000_ICR_TXD_FIFO_PAR0	/* queue 0 Tx descriptor FIFO parity error */
#define E1000_IMS_HOST_ARB_PAR  E1000_ICR_HOST_ARB_PAR	/* host arb read buffer parity error */
#define E1000_IMS_PB_PAR        E1000_ICR_PB_PAR	/* packet buffer parity error */
#define E1000_IMS_RXD_FIFO_PAR1 E1000_ICR_RXD_FIFO_PAR1	/* queue 1 Rx descriptor FIFO parity error */
#define E1000_IMS_TXD_FIFO_PAR1 E1000_ICR_TXD_FIFO_PAR1	/* queue 1 Tx descriptor FIFO parity error */
#define E1000_IMS_DSW       E1000_ICR_DSW
#define E1000_IMS_PHYINT    E1000_ICR_PHYINT
#define E1000_IMS_EPRST     E1000_ICR_EPRST

/* Interrupt Mask Clear */
#define E1000_IMC_TXDW      E1000_ICR_TXDW	/* Transmit desc written back */
#define E1000_IMC_TXQE      E1000_ICR_TXQE	/* Transmit Queue empty */
#define E1000_IMC_LSC       E1000_ICR_LSC	/* Link Status Change */
#define E1000_IMC_RXSEQ     E1000_ICR_RXSEQ	/* rx sequence error */
#define E1000_IMC_RXDMT0    E1000_ICR_RXDMT0	/* rx desc min. threshold */
#define E1000_IMC_RXO       E1000_ICR_RXO	/* rx overrun */
#define E1000_IMC_RXT0      E1000_ICR_RXT0	/* rx timer intr */
#define E1000_IMC_MDAC      E1000_ICR_MDAC	/* MDIO access complete */
#define E1000_IMC_RXCFG     E1000_ICR_RXCFG	/* RX /c/ ordered set */
#define E1000_IMC_GPI_EN0   E1000_ICR_GPI_EN0	/* GP Int 0 */
#define E1000_IMC_GPI_EN1   E1000_ICR_GPI_EN1	/* GP Int 1 */
#define E1000_IMC_GPI_EN2   E1000_ICR_GPI_EN2	/* GP Int 2 */
#define E1000_IMC_GPI_EN3   E1000_ICR_GPI_EN3	/* GP Int 3 */
#define E1000_IMC_TXD_LOW   E1000_ICR_TXD_LOW
#define E1000_IMC_SRPD      E1000_ICR_SRPD
#define E1000_IMC_ACK       E1000_ICR_ACK	/* Receive Ack frame */
#define E1000_IMC_MNG       E1000_ICR_MNG	/* Manageability event */
#define E1000_IMC_DOCK      E1000_ICR_DOCK	/* Dock/Undock */
#define E1000_IMC_RXD_FIFO_PAR0 E1000_ICR_RXD_FIFO_PAR0	/* queue 0 Rx descriptor FIFO parity error */
#define E1000_IMC_TXD_FIFO_PAR0 E1000_ICR_TXD_FIFO_PAR0	/* queue 0 Tx descriptor FIFO parity error */
#define E1000_IMC_HOST_ARB_PAR  E1000_ICR_HOST_ARB_PAR	/* host arb read buffer parity error */
#define E1000_IMC_PB_PAR        E1000_ICR_PB_PAR	/* packet buffer parity error */
#define E1000_IMC_RXD_FIFO_PAR1 E1000_ICR_RXD_FIFO_PAR1	/* queue 1 Rx descriptor FIFO parity error */
#define E1000_IMC_TXD_FIFO_PAR1 E1000_ICR_TXD_FIFO_PAR1	/* queue 1 Tx descriptor FIFO parity error */
#define E1000_IMC_DSW       E1000_ICR_DSW
#define E1000_IMC_PHYINT    E1000_ICR_PHYINT
#define E1000_IMC_EPRST     E1000_ICR_EPRST

/* Receive Control */
#define E1000_RCTL_RST            0x00000001	/* Software reset */
#define E1000_RCTL_EN             0x00000002	/* enable */
#define E1000_RCTL_SBP            0x00000004	/* store bad packet */
#define E1000_RCTL_UPE            0x00000008	/* unicast promiscuous enable */
#define E1000_RCTL_MPE            0x00000010	/* multicast promiscuous enab */
#define E1000_RCTL_LPE            0x00000020	/* long packet enable */
#define E1000_RCTL_LBM_NO         0x00000000	/* no loopback mode */
#define E1000_RCTL_LBM_MAC        0x00000040	/* MAC loopback mode */
#define E1000_RCTL_LBM_SLP        0x00000080	/* serial link loopback mode */
#define E1000_RCTL_LBM_TCVR       0x000000C0	/* tcvr loopback mode */
#define E1000_RCTL_DTYP_MASK      0x00000C00	/* Descriptor type mask */
#define E1000_RCTL_DTYP_PS        0x00000400	/* Packet Split descriptor */
#define E1000_RCTL_RDMTS_HALF     0x00000000	/* rx desc min threshold size */
#define E1000_RCTL_RDMTS_QUAT     0x00000100	/* rx desc min threshold size */
#define E1000_RCTL_RDMTS_EIGTH    0x00000200	/* rx desc min threshold size */
#define E1000_RCTL_MO_SHIFT       12	/* multicast offset shift */
#define E1000_RCTL_MO_0           0x00000000	/* multicast offset 11:0 */
#define E1000_RCTL_MO_1           0x00001000	/* multicast offset 12:1 */
#define E1000_RCTL_MO_2           0x00002000	/* multicast offset 13:2 */
#define E1000_RCTL_MO_3           0x00003000	/* multicast offset 15:4 */
#define E1000_RCTL_MDR            0x00004000	/* multicast desc ring 0 */
#define E1000_RCTL_BAM            0x00008000	/* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048        0x00000000	/* rx buffer size 2048 */
#define E1000_RCTL_SZ_1024        0x00010000	/* rx buffer size 1024 */
#define E1000_RCTL_SZ_512         0x00020000	/* rx buffer size 512 */
#define E1000_RCTL_SZ_256         0x00030000	/* rx buffer size 256 */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 1 */
#define E1000_RCTL_SZ_16384       0x00010000	/* rx buffer size 16384 */
#define E1000_RCTL_SZ_8192        0x00020000	/* rx buffer size 8192 */
#define E1000_RCTL_SZ_4096        0x00030000	/* rx buffer size 4096 */
#define E1000_RCTL_VFE            0x00040000	/* vlan filter enable */
#define E1000_RCTL_CFIEN          0x00080000	/* canonical form enable */
#define E1000_RCTL_CFI            0x00100000	/* canonical form indicator */
#define E1000_RCTL_DPF            0x00400000	/* discard pause frames */
#define E1000_RCTL_PMCF           0x00800000	/* pass MAC control frames */
#define E1000_RCTL_BSEX           0x02000000	/* Buffer size extension */
#define E1000_RCTL_SECRC          0x04000000	/* Strip Ethernet CRC */
#define E1000_RCTL_FLXBUF_MASK    0x78000000	/* Flexible buffer size */
#define E1000_RCTL_FLXBUF_SHIFT   27	/* Flexible buffer shift */

/* Use byte values for the following shift parameters
 * Usage:
 *     psrctl |= (((ROUNDUP(value0, 128) >> E1000_PSRCTL_BSIZE0_SHIFT) &
 *                  E1000_PSRCTL_BSIZE0_MASK) |
 *                ((ROUNDUP(value1, 1024) >> E1000_PSRCTL_BSIZE1_SHIFT) &
 *                  E1000_PSRCTL_BSIZE1_MASK) |
 *                ((ROUNDUP(value2, 1024) << E1000_PSRCTL_BSIZE2_SHIFT) &
 *                  E1000_PSRCTL_BSIZE2_MASK) |
 *                ((ROUNDUP(value3, 1024) << E1000_PSRCTL_BSIZE3_SHIFT) |;
 *                  E1000_PSRCTL_BSIZE3_MASK))
 * where value0 = [128..16256],  default=256
 *       value1 = [1024..64512], default=4096
 *       value2 = [0..64512],    default=4096
 *       value3 = [0..64512],    default=0
 */

#define E1000_PSRCTL_BSIZE0_MASK   0x0000007F
#define E1000_PSRCTL_BSIZE1_MASK   0x00003F00
#define E1000_PSRCTL_BSIZE2_MASK   0x003F0000
#define E1000_PSRCTL_BSIZE3_MASK   0x3F000000

#define E1000_PSRCTL_BSIZE0_SHIFT  7	/* Shift _right_ 7 */
#define E1000_PSRCTL_BSIZE1_SHIFT  2	/* Shift _right_ 2 */
#define E1000_PSRCTL_BSIZE2_SHIFT  6	/* Shift _left_ 6 */
#define E1000_PSRCTL_BSIZE3_SHIFT 14	/* Shift _left_ 14 */

/* SW_W_SYNC definitions */
#define E1000_SWFW_EEP_SM     0x0001
#define E1000_SWFW_PHY0_SM    0x0002
#define E1000_SWFW_PHY1_SM    0x0004
#define E1000_SWFW_MAC_CSR_SM 0x0008

/* Receive Descriptor */
#define E1000_RDT_DELAY 0x0000ffff	/* Delay timer (1=1024us) */
#define E1000_RDT_FPDB  0x80000000	/* Flush descriptor block */
#define E1000_RDLEN_LEN 0x0007ff80	/* descriptor length */
#define E1000_RDH_RDH   0x0000ffff	/* receive descriptor head */
#define E1000_RDT_RDT   0x0000ffff	/* receive descriptor tail */

/* Flow Control */
#define E1000_FCRTH_RTH  0x0000FFF8	/* Mask Bits[15:3] for RTH */
#define E1000_FCRTH_XFCE 0x80000000	/* External Flow Control Enable */
#define E1000_FCRTL_RTL  0x0000FFF8	/* Mask Bits[15:3] for RTL */
#define E1000_FCRTL_XONE 0x80000000	/* Enable XON frame transmission */

/* Header split receive */
#define E1000_RFCTL_ISCSI_DIS           0x00000001
#define E1000_RFCTL_ISCSI_DWC_MASK      0x0000003E
#define E1000_RFCTL_ISCSI_DWC_SHIFT     1
#define E1000_RFCTL_NFSW_DIS            0x00000040
#define E1000_RFCTL_NFSR_DIS            0x00000080
#define E1000_RFCTL_NFS_VER_MASK        0x00000300
#define E1000_RFCTL_NFS_VER_SHIFT       8
#define E1000_RFCTL_IPV6_DIS            0x00000400
#define E1000_RFCTL_IPV6_XSUM_DIS       0x00000800
#define E1000_RFCTL_ACK_DIS             0x00001000
#define E1000_RFCTL_ACKD_DIS            0x00002000
#define E1000_RFCTL_IPFRSP_DIS          0x00004000
#define E1000_RFCTL_EXTEN               0x00008000
#define E1000_RFCTL_IPV6_EX_DIS         0x00010000
#define E1000_RFCTL_NEW_IPV6_EXT_DIS    0x00020000

/* Receive Descriptor Control */
#define E1000_RXDCTL_PTHRESH 0x0000003F	/* RXDCTL Prefetch Threshold */
#define E1000_RXDCTL_HTHRESH 0x00003F00	/* RXDCTL Host Threshold */
#define E1000_RXDCTL_WTHRESH 0x003F0000	/* RXDCTL Writeback Threshold */
#define E1000_RXDCTL_GRAN    0x01000000	/* RXDCTL Granularity */

/* Transmit Descriptor Control */
#define E1000_TXDCTL_PTHRESH 0x0000003F	/* TXDCTL Prefetch Threshold */
#define E1000_TXDCTL_HTHRESH 0x00003F00	/* TXDCTL Host Threshold */
#define E1000_TXDCTL_WTHRESH 0x003F0000	/* TXDCTL Writeback Threshold */
#define E1000_TXDCTL_GRAN    0x01000000	/* TXDCTL Granularity */
#define E1000_TXDCTL_LWTHRESH 0xFE000000	/* TXDCTL Low Threshold */
#define E1000_TXDCTL_FULL_TX_DESC_WB 0x01010000	/* GRAN=1, WTHRESH=1 */
#define E1000_TXDCTL_COUNT_DESC 0x00400000	/* Enable the counting of desc.
						   still to be processed. */
/* Transmit Configuration Word */
#define E1000_TXCW_FD         0x00000020	/* TXCW full duplex */
#define E1000_TXCW_HD         0x00000040	/* TXCW half duplex */
#define E1000_TXCW_PAUSE      0x00000080	/* TXCW sym pause request */
#define E1000_TXCW_ASM_DIR    0x00000100	/* TXCW astm pause direction */
#define E1000_TXCW_PAUSE_MASK 0x00000180	/* TXCW pause request mask */
#define E1000_TXCW_RF         0x00003000	/* TXCW remote fault */
#define E1000_TXCW_NP         0x00008000	/* TXCW next page */
#define E1000_TXCW_CW         0x0000ffff	/* TxConfigWord mask */
#define E1000_TXCW_TXC        0x40000000	/* Transmit Config control */
#define E1000_TXCW_ANE        0x80000000	/* Auto-neg enable */

/* Receive Configuration Word */
#define E1000_RXCW_CW    0x0000ffff	/* RxConfigWord mask */
#define E1000_RXCW_NC    0x04000000	/* Receive config no carrier */
#define E1000_RXCW_IV    0x08000000	/* Receive config invalid */
#define E1000_RXCW_CC    0x10000000	/* Receive config change */
#define E1000_RXCW_C     0x20000000	/* Receive config */
#define E1000_RXCW_SYNCH 0x40000000	/* Receive config synch */
#define E1000_RXCW_ANC   0x80000000	/* Auto-neg complete */

/* Transmit Control */
#define E1000_TCTL_RST    0x00000001	/* software reset */
#define E1000_TCTL_EN     0x00000002	/* enable tx */
#define E1000_TCTL_BCE    0x00000004	/* busy check enable */
#define E1000_TCTL_PSP    0x00000008	/* pad short packets */
#define E1000_TCTL_CT     0x00000ff0	/* collision threshold */
#define E1000_TCTL_COLD   0x003ff000	/* collision distance */
#define E1000_TCTL_SWXOFF 0x00400000	/* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000	/* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000	/* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000	/* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000	/* Multiple request support */
/* Extended Transmit Control */
#define E1000_TCTL_EXT_BST_MASK  0x000003FF	/* Backoff Slot Time */
#define E1000_TCTL_EXT_GCEX_MASK 0x000FFC00	/* Gigabit Carry Extend Padding */

/* Receive Checksum Control */
#define E1000_RXCSUM_PCSS_MASK 0x000000FF	/* Packet Checksum Start */
#define E1000_RXCSUM_IPOFL     0x00000100	/* IPv4 checksum offload */
#define E1000_RXCSUM_TUOFL     0x00000200	/* TCP / UDP checksum offload */
#define E1000_RXCSUM_IPV6OFL   0x00000400	/* IPv6 checksum offload */
#define E1000_RXCSUM_IPPCSE    0x00001000	/* IP payload checksum enable */
#define E1000_RXCSUM_PCSD      0x00002000	/* packet checksum disabled */

/* Multiple Receive Queue Control */
#define E1000_MRQC_ENABLE_MASK              0x00000003
#define E1000_MRQC_ENABLE_RSS_2Q            0x00000001
#define E1000_MRQC_ENABLE_RSS_INT           0x00000004
#define E1000_MRQC_RSS_FIELD_MASK           0xFFFF0000
#define E1000_MRQC_RSS_FIELD_IPV4_TCP       0x00010000
#define E1000_MRQC_RSS_FIELD_IPV4           0x00020000
#define E1000_MRQC_RSS_FIELD_IPV6_TCP_EX    0x00040000
#define E1000_MRQC_RSS_FIELD_IPV6_EX        0x00080000
#define E1000_MRQC_RSS_FIELD_IPV6           0x00100000
#define E1000_MRQC_RSS_FIELD_IPV6_TCP       0x00200000

/* Definitions for power management and wakeup registers */
/* Wake Up Control */
#define E1000_WUC_APME       0x00000001	/* APM Enable */
#define E1000_WUC_PME_EN     0x00000002	/* PME Enable */
#define E1000_WUC_PME_STATUS 0x00000004	/* PME Status */
#define E1000_WUC_APMPME     0x00000008	/* Assert PME on APM Wakeup */
#define E1000_WUC_SPM        0x80000000	/* Enable SPM */

/* Wake Up Filter Control */
#define E1000_WUFC_LNKC 0x00000001	/* Link Status Change Wakeup Enable */
#define E1000_WUFC_MAG  0x00000002	/* Magic Packet Wakeup Enable */
#define E1000_WUFC_EX   0x00000004	/* Directed Exact Wakeup Enable */
#define E1000_WUFC_MC   0x00000008	/* Directed Multicast Wakeup Enable */
#define E1000_WUFC_BC   0x00000010	/* Broadcast Wakeup Enable */
#define E1000_WUFC_ARP  0x00000020	/* ARP Request Packet Wakeup Enable */
#define E1000_WUFC_IPV4 0x00000040	/* Directed IPv4 Packet Wakeup Enable */
#define E1000_WUFC_IPV6 0x00000080	/* Directed IPv6 Packet Wakeup Enable */
#define E1000_WUFC_IGNORE_TCO      0x00008000	/* Ignore WakeOn TCO packets */
#define E1000_WUFC_FLX0 0x00010000	/* Flexible Filter 0 Enable */
#define E1000_WUFC_FLX1 0x00020000	/* Flexible Filter 1 Enable */
#define E1000_WUFC_FLX2 0x00040000	/* Flexible Filter 2 Enable */
#define E1000_WUFC_FLX3 0x00080000	/* Flexible Filter 3 Enable */
#define E1000_WUFC_ALL_FILTERS 0x000F00FF	/* Mask for all wakeup filters */
#define E1000_WUFC_FLX_OFFSET 16	/* Offset to the Flexible Filters bits */
#define E1000_WUFC_FLX_FILTERS 0x000F0000	/* Mask for the 4 flexible filters */

/* Wake Up Status */
#define E1000_WUS_LNKC 0x00000001	/* Link Status Changed */
#define E1000_WUS_MAG  0x00000002	/* Magic Packet Received */
#define E1000_WUS_EX   0x00000004	/* Directed Exact Received */
#define E1000_WUS_MC   0x00000008	/* Directed Multicast Received */
#define E1000_WUS_BC   0x00000010	/* Broadcast Received */
#define E1000_WUS_ARP  0x00000020	/* ARP Request Packet Received */
#define E1000_WUS_IPV4 0x00000040	/* Directed IPv4 Packet Wakeup Received */
#define E1000_WUS_IPV6 0x00000080	/* Directed IPv6 Packet Wakeup Received */
#define E1000_WUS_FLX0 0x00010000	/* Flexible Filter 0 Match */
#define E1000_WUS_FLX1 0x00020000	/* Flexible Filter 1 Match */
#define E1000_WUS_FLX2 0x00040000	/* Flexible Filter 2 Match */
#define E1000_WUS_FLX3 0x00080000	/* Flexible Filter 3 Match */
#define E1000_WUS_FLX_FILTERS 0x000F0000	/* Mask for the 4 flexible filters */

/* Management Control */
#define E1000_MANC_SMBUS_EN      0x00000001	/* SMBus Enabled - RO */
#define E1000_MANC_ASF_EN        0x00000002	/* ASF Enabled - RO */
#define E1000_MANC_R_ON_FORCE    0x00000004	/* Reset on Force TCO - RO */
#define E1000_MANC_RMCP_EN       0x00000100	/* Enable RCMP 026Fh Filtering */
#define E1000_MANC_0298_EN       0x00000200	/* Enable RCMP 0298h Filtering */
#define E1000_MANC_IPV4_EN       0x00000400	/* Enable IPv4 */
#define E1000_MANC_IPV6_EN       0x00000800	/* Enable IPv6 */
#define E1000_MANC_SNAP_EN       0x00001000	/* Accept LLC/SNAP */
#define E1000_MANC_ARP_EN        0x00002000	/* Enable ARP Request Filtering */
#define E1000_MANC_NEIGHBOR_EN   0x00004000	/* Enable Neighbor Discovery
						 * Filtering */
#define E1000_MANC_ARP_RES_EN    0x00008000	/* Enable ARP response Filtering */
#define E1000_MANC_TCO_RESET     0x00010000	/* TCO Reset Occurred */
#define E1000_MANC_RCV_TCO_EN    0x00020000	/* Receive TCO Packets Enabled */
#define E1000_MANC_REPORT_STATUS 0x00040000	/* Status Reporting Enabled */
#define E1000_MANC_RCV_ALL       0x00080000	/* Receive All Enabled */
#define E1000_MANC_BLK_PHY_RST_ON_IDE   0x00040000	/* Block phy resets */
#define E1000_MANC_EN_MAC_ADDR_FILTER   0x00100000	/* Enable MAC address
							 * filtering */
#define E1000_MANC_EN_MNG2HOST   0x00200000	/* Enable MNG packets to host
						 * memory */
#define E1000_MANC_EN_IP_ADDR_FILTER    0x00400000	/* Enable IP address
							 * filtering */
#define E1000_MANC_EN_XSUM_FILTER   0x00800000	/* Enable checksum filtering */
#define E1000_MANC_BR_EN         0x01000000	/* Enable broadcast filtering */
#define E1000_MANC_SMB_REQ       0x01000000	/* SMBus Request */
#define E1000_MANC_SMB_GNT       0x02000000	/* SMBus Grant */
#define E1000_MANC_SMB_CLK_IN    0x04000000	/* SMBus Clock In */
#define E1000_MANC_SMB_DATA_IN   0x08000000	/* SMBus Data In */
#define E1000_MANC_SMB_DATA_OUT  0x10000000	/* SMBus Data Out */
#define E1000_MANC_SMB_CLK_OUT   0x20000000	/* SMBus Clock Out */

#define E1000_MANC_SMB_DATA_OUT_SHIFT  28	/* SMBus Data Out Shift */
#define E1000_MANC_SMB_CLK_OUT_SHIFT   29	/* SMBus Clock Out Shift */

/* SW Semaphore Register */
#define E1000_SWSM_SMBI         0x00000001	/* Driver Semaphore bit */
#define E1000_SWSM_SWESMBI      0x00000002	/* FW Semaphore bit */
#define E1000_SWSM_WMNG         0x00000004	/* Wake MNG Clock */
#define E1000_SWSM_DRV_LOAD     0x00000008	/* Driver Loaded Bit */

/* FW Semaphore Register */
#define E1000_FWSM_MODE_MASK    0x0000000E	/* FW mode */
#define E1000_FWSM_MODE_SHIFT            1
#define E1000_FWSM_FW_VALID     0x00008000	/* FW established a valid mode */

#define E1000_FWSM_RSPCIPHY        0x00000040	/* Reset PHY on PCI reset */
#define E1000_FWSM_DISSW           0x10000000	/* FW disable SW Write Access */
#define E1000_FWSM_SKUSEL_MASK     0x60000000	/* LAN SKU select */
#define E1000_FWSM_SKUEL_SHIFT     29
#define E1000_FWSM_SKUSEL_EMB      0x0	/* Embedded SKU */
#define E1000_FWSM_SKUSEL_CONS     0x1	/* Consumer SKU */
#define E1000_FWSM_SKUSEL_PERF_100 0x2	/* Perf & Corp 10/100 SKU */
#define E1000_FWSM_SKUSEL_PERF_GBE 0x3	/* Perf & Copr GbE SKU */

/* FFLT Debug Register */
#define E1000_FFLT_DBG_INVC     0x00100000	/* Invalid /C/ code handling */

typedef enum {
	e1000_mng_mode_none = 0,
	e1000_mng_mode_asf,
	e1000_mng_mode_pt,
	e1000_mng_mode_ipmi,
	e1000_mng_mode_host_interface_only
} e1000_mng_mode;

/* Host Interface Control Register */
#define E1000_HICR_EN           0x00000001	/* Enable Bit - RO */
#define E1000_HICR_C            0x00000002	/* Driver sets this bit when done
						 * to put command in RAM */
#define E1000_HICR_SV           0x00000004	/* Status Validity */
#define E1000_HICR_FWR          0x00000080	/* FW reset. Set by the Host */

/* Host Interface Command Interface - Address range 0x8800-0x8EFF */
#define E1000_HI_MAX_DATA_LENGTH         252	/* Host Interface data length */
#define E1000_HI_MAX_BLOCK_BYTE_LENGTH  1792	/* Number of bytes in range */
#define E1000_HI_MAX_BLOCK_DWORD_LENGTH  448	/* Number of dwords in range */
#define E1000_HI_COMMAND_TIMEOUT         500	/* Time in ms to process HI command */

struct e1000_host_command_header {
	u8 command_id;
	u8 command_length;
	u8 command_options;	/* I/F bits for command, status for return */
	u8 checksum;
};
struct e1000_host_command_info {
	struct e1000_host_command_header command_header;	/* Command Head/Command Result Head has 4 bytes */
	u8 command_data[E1000_HI_MAX_DATA_LENGTH];	/* Command data can length 0..252 */
};

/* Host SMB register #0 */
#define E1000_HSMC0R_CLKIN      0x00000001	/* SMB Clock in */
#define E1000_HSMC0R_DATAIN     0x00000002	/* SMB Data in */
#define E1000_HSMC0R_DATAOUT    0x00000004	/* SMB Data out */
#define E1000_HSMC0R_CLKOUT     0x00000008	/* SMB Clock out */

/* Host SMB register #1 */
#define E1000_HSMC1R_CLKIN      E1000_HSMC0R_CLKIN
#define E1000_HSMC1R_DATAIN     E1000_HSMC0R_DATAIN
#define E1000_HSMC1R_DATAOUT    E1000_HSMC0R_DATAOUT
#define E1000_HSMC1R_CLKOUT     E1000_HSMC0R_CLKOUT

/* FW Status Register */
#define E1000_FWSTS_FWS_MASK    0x000000FF	/* FW Status */

/* Wake Up Packet Length */
#define E1000_WUPL_LENGTH_MASK 0x0FFF	/* Only the lower 12 bits are valid */

#define E1000_MDALIGN          4096

/* PCI-Ex registers*/

/* PCI-Ex Control Register */
#define E1000_GCR_RXD_NO_SNOOP          0x00000001
#define E1000_GCR_RXDSCW_NO_SNOOP       0x00000002
#define E1000_GCR_RXDSCR_NO_SNOOP       0x00000004
#define E1000_GCR_TXD_NO_SNOOP          0x00000008
#define E1000_GCR_TXDSCW_NO_SNOOP       0x00000010
#define E1000_GCR_TXDSCR_NO_SNOOP       0x00000020

#define PCI_EX_NO_SNOOP_ALL (E1000_GCR_RXD_NO_SNOOP         | \
                             E1000_GCR_RXDSCW_NO_SNOOP      | \
                             E1000_GCR_RXDSCR_NO_SNOOP      | \
                             E1000_GCR_TXD_NO_SNOOP         | \
                             E1000_GCR_TXDSCW_NO_SNOOP      | \
                             E1000_GCR_TXDSCR_NO_SNOOP)

#define PCI_EX_82566_SNOOP_ALL PCI_EX_NO_SNOOP_ALL

#define E1000_GCR_L1_ACT_WITHOUT_L0S_RX 0x08000000
/* Function Active and Power State to MNG */
#define E1000_FACTPS_FUNC0_POWER_STATE_MASK         0x00000003
#define E1000_FACTPS_LAN0_VALID                     0x00000004
#define E1000_FACTPS_FUNC0_AUX_EN                   0x00000008
#define E1000_FACTPS_FUNC1_POWER_STATE_MASK         0x000000C0
#define E1000_FACTPS_FUNC1_POWER_STATE_SHIFT        6
#define E1000_FACTPS_LAN1_VALID                     0x00000100
#define E1000_FACTPS_FUNC1_AUX_EN                   0x00000200
#define E1000_FACTPS_FUNC2_POWER_STATE_MASK         0x00003000
#define E1000_FACTPS_FUNC2_POWER_STATE_SHIFT        12
#define E1000_FACTPS_IDE_ENABLE                     0x00004000
#define E1000_FACTPS_FUNC2_AUX_EN                   0x00008000
#define E1000_FACTPS_FUNC3_POWER_STATE_MASK         0x000C0000
#define E1000_FACTPS_FUNC3_POWER_STATE_SHIFT        18
#define E1000_FACTPS_SP_ENABLE                      0x00100000
#define E1000_FACTPS_FUNC3_AUX_EN                   0x00200000
#define E1000_FACTPS_FUNC4_POWER_STATE_MASK         0x03000000
#define E1000_FACTPS_FUNC4_POWER_STATE_SHIFT        24
#define E1000_FACTPS_IPMI_ENABLE                    0x04000000
#define E1000_FACTPS_FUNC4_AUX_EN                   0x08000000
#define E1000_FACTPS_MNGCG                          0x20000000
#define E1000_FACTPS_LAN_FUNC_SEL                   0x40000000
#define E1000_FACTPS_PM_STATE_CHANGED               0x80000000

/* PCI-Ex Config Space */
#define PCI_EX_LINK_STATUS           0x12
#define PCI_EX_LINK_WIDTH_MASK       0x3F0
#define PCI_EX_LINK_WIDTH_SHIFT      4

/* EEPROM Commands - Microwire */
#define EEPROM_READ_OPCODE_MICROWIRE  0x6	/* EEPROM read opcode */
#define EEPROM_WRITE_OPCODE_MICROWIRE 0x5	/* EEPROM write opcode */
#define EEPROM_ERASE_OPCODE_MICROWIRE 0x7	/* EEPROM erase opcode */
#define EEPROM_EWEN_OPCODE_MICROWIRE  0x13	/* EEPROM erase/write enable */
#define EEPROM_EWDS_OPCODE_MICROWIRE  0x10	/* EEPROM erase/write disable */

/* EEPROM Commands - SPI */
#define EEPROM_MAX_RETRY_SPI        5000	/* Max wait of 5ms, for RDY signal */
#define EEPROM_READ_OPCODE_SPI      0x03	/* EEPROM read opcode */
#define EEPROM_WRITE_OPCODE_SPI     0x02	/* EEPROM write opcode */
#define EEPROM_A8_OPCODE_SPI        0x08	/* opcode bit-3 = address bit-8 */
#define EEPROM_WREN_OPCODE_SPI      0x06	/* EEPROM set Write Enable latch */
#define EEPROM_WRDI_OPCODE_SPI      0x04	/* EEPROM reset Write Enable latch */
#define EEPROM_RDSR_OPCODE_SPI      0x05	/* EEPROM read Status register */
#define EEPROM_WRSR_OPCODE_SPI      0x01	/* EEPROM write Status register */
#define EEPROM_ERASE4K_OPCODE_SPI   0x20	/* EEPROM ERASE 4KB */
#define EEPROM_ERASE64K_OPCODE_SPI  0xD8	/* EEPROM ERASE 64KB */
#define EEPROM_ERASE256_OPCODE_SPI  0xDB	/* EEPROM ERASE 256B */

/* EEPROM Size definitions */
#define EEPROM_WORD_SIZE_SHIFT  6
#define EEPROM_SIZE_SHIFT       10
#define EEPROM_SIZE_MASK        0x1C00

/* EEPROM Word Offsets */
#define EEPROM_COMPAT                 0x0003
#define EEPROM_ID_LED_SETTINGS        0x0004
#define EEPROM_VERSION                0x0005
#define EEPROM_SERDES_AMPLITUDE       0x0006	/* For SERDES output amplitude adjustment. */
#define EEPROM_PHY_CLASS_WORD         0x0007
#define EEPROM_INIT_CONTROL1_REG      0x000A
#define EEPROM_INIT_CONTROL2_REG      0x000F
#define EEPROM_SWDEF_PINS_CTRL_PORT_1 0x0010
#define EEPROM_INIT_CONTROL3_PORT_B   0x0014
#define EEPROM_INIT_3GIO_3            0x001A
#define EEPROM_SWDEF_PINS_CTRL_PORT_0 0x0020
#define EEPROM_INIT_CONTROL3_PORT_A   0x0024
#define EEPROM_CFG                    0x0012
#define EEPROM_FLASH_VERSION          0x0032
#define EEPROM_CHECKSUM_REG           0x003F

#define E1000_EEPROM_CFG_DONE         0x00040000	/* MNG config cycle done */
#define E1000_EEPROM_CFG_DONE_PORT_1  0x00080000	/* ...for second port */

/* Word definitions for ID LED Settings */
#define ID_LED_RESERVED_0000 0x0000
#define ID_LED_RESERVED_FFFF 0xFFFF
#define ID_LED_DEFAULT       ((ID_LED_OFF1_ON2 << 12) | \
                              (ID_LED_OFF1_OFF2 << 8) | \
                              (ID_LED_DEF1_DEF2 << 4) | \
                              (ID_LED_DEF1_DEF2))
#define ID_LED_DEF1_DEF2     0x1
#define ID_LED_DEF1_ON2      0x2
#define ID_LED_DEF1_OFF2     0x3
#define ID_LED_ON1_DEF2      0x4
#define ID_LED_ON1_ON2       0x5
#define ID_LED_ON1_OFF2      0x6
#define ID_LED_OFF1_DEF2     0x7
#define ID_LED_OFF1_ON2      0x8
#define ID_LED_OFF1_OFF2     0x9

#define IGP_ACTIVITY_LED_MASK   0xFFFFF0FF
#define IGP_ACTIVITY_LED_ENABLE 0x0300
#define IGP_LED3_MODE           0x07000000

/* Mask bits for SERDES amplitude adjustment in Word 6 of the EEPROM */
#define EEPROM_SERDES_AMPLITUDE_MASK  0x000F

/* Mask bit for PHY class in Word 7 of the EEPROM */
#define EEPROM_PHY_CLASS_A   0x8000

/* Mask bits for fields in Word 0x0a of the EEPROM */
#define EEPROM_WORD0A_ILOS   0x0010
#define EEPROM_WORD0A_SWDPIO 0x01E0
#define EEPROM_WORD0A_LRST   0x0200
#define EEPROM_WORD0A_FD     0x0400
#define EEPROM_WORD0A_66MHZ  0x0800

/* Mask bits for fields in Word 0x0f of the EEPROM */
#define EEPROM_WORD0F_PAUSE_MASK 0x3000
#define EEPROM_WORD0F_PAUSE      0x1000
#define EEPROM_WORD0F_ASM_DIR    0x2000
#define EEPROM_WORD0F_ANE        0x0800
#define EEPROM_WORD0F_SWPDIO_EXT 0x00F0
#define EEPROM_WORD0F_LPLU       0x0001

/* Mask bits for fields in Word 0x10/0x20 of the EEPROM */
#define EEPROM_WORD1020_GIGA_DISABLE         0x0010
#define EEPROM_WORD1020_GIGA_DISABLE_NON_D0A 0x0008

/* Mask bits for fields in Word 0x1a of the EEPROM */
#define EEPROM_WORD1A_ASPM_MASK  0x000C

/* For checksumming, the sum of all words in the EEPROM should equal 0xBABA. */
#define EEPROM_SUM 0xBABA

/* EEPROM Map defines (WORD OFFSETS)*/
#define EEPROM_NODE_ADDRESS_BYTE_0 0
#define EEPROM_PBA_BYTE_1          8

#define EEPROM_RESERVED_WORD          0xFFFF

/* EEPROM Map Sizes (Byte Counts) */
#define PBA_SIZE 4

/* Collision related configuration parameters */
#define E1000_COLLISION_THRESHOLD       15
#define E1000_CT_SHIFT                  4
/* Collision distance is a 0-based value that applies to
 * half-duplex-capable hardware only. */
#define E1000_COLLISION_DISTANCE        63
#define E1000_COLLISION_DISTANCE_82542  64
#define E1000_FDX_COLLISION_DISTANCE    E1000_COLLISION_DISTANCE
#define E1000_HDX_COLLISION_DISTANCE    E1000_COLLISION_DISTANCE
#define E1000_COLD_SHIFT                12

/* Number of Transmit and Receive Descriptors must be a multiple of 8 */
#define REQ_TX_DESCRIPTOR_MULTIPLE  8
#define REQ_RX_DESCRIPTOR_MULTIPLE  8

/* Default values for the transmit IPG register */
#define DEFAULT_82542_TIPG_IPGT        10
#define DEFAULT_82543_TIPG_IPGT_FIBER  9
#define DEFAULT_82543_TIPG_IPGT_COPPER 8

#define E1000_TIPG_IPGT_MASK  0x000003FF
#define E1000_TIPG_IPGR1_MASK 0x000FFC00
#define E1000_TIPG_IPGR2_MASK 0x3FF00000

#define DEFAULT_82542_TIPG_IPGR1 2
#define DEFAULT_82543_TIPG_IPGR1 8
#define E1000_TIPG_IPGR1_SHIFT  10

#define DEFAULT_82542_TIPG_IPGR2 10
#define DEFAULT_82543_TIPG_IPGR2 6
#define E1000_TIPG_IPGR2_SHIFT  20

#define E1000_TXDMAC_DPP 0x00000001

/* Adaptive IFS defines */
#define TX_THRESHOLD_START     8
#define TX_THRESHOLD_INCREMENT 10
#define TX_THRESHOLD_DECREMENT 1
#define TX_THRESHOLD_STOP      190
#define TX_THRESHOLD_DISABLE   0
#define TX_THRESHOLD_TIMER_MS  10000
#define MIN_NUM_XMITS          1000
#define IFS_MAX                80
#define IFS_STEP               10
#define IFS_MIN                40
#define IFS_RATIO              4

/* Extended Configuration Control and Size */
#define E1000_EXTCNF_CTRL_PCIE_WRITE_ENABLE 0x00000001
#define E1000_EXTCNF_CTRL_PHY_WRITE_ENABLE  0x00000002
#define E1000_EXTCNF_CTRL_D_UD_ENABLE       0x00000004
#define E1000_EXTCNF_CTRL_D_UD_LATENCY      0x00000008
#define E1000_EXTCNF_CTRL_D_UD_OWNER        0x00000010
#define E1000_EXTCNF_CTRL_MDIO_SW_OWNERSHIP 0x00000020
#define E1000_EXTCNF_CTRL_MDIO_HW_OWNERSHIP 0x00000040
#define E1000_EXTCNF_CTRL_EXT_CNF_POINTER   0x0FFF0000

#define E1000_EXTCNF_SIZE_EXT_PHY_LENGTH    0x000000FF
#define E1000_EXTCNF_SIZE_EXT_DOCK_LENGTH   0x0000FF00
#define E1000_EXTCNF_SIZE_EXT_PCIE_LENGTH   0x00FF0000
#define E1000_EXTCNF_CTRL_LCD_WRITE_ENABLE  0x00000001
#define E1000_EXTCNF_CTRL_SWFLAG            0x00000020

/* PBA constants */
#define E1000_PBA_8K 0x0008	/* 8KB, default Rx allocation */
#define E1000_PBA_12K 0x000C	/* 12KB, default Rx allocation */
#define E1000_PBA_16K 0x0010	/* 16KB, default TX allocation */
#define E1000_PBA_20K 0x0014
#define E1000_PBA_22K 0x0016
#define E1000_PBA_24K 0x0018
#define E1000_PBA_30K 0x001E
#define E1000_PBA_32K 0x0020
#define E1000_PBA_34K 0x0022
#define E1000_PBA_38K 0x0026
#define E1000_PBA_40K 0x0028
#define E1000_PBA_48K 0x0030	/* 48KB, default RX allocation */

#define E1000_PBS_16K E1000_PBA_16K

/* Flow Control Constants */
#define FLOW_CONTROL_ADDRESS_LOW  0x00C28001
#define FLOW_CONTROL_ADDRESS_HIGH 0x00000100
#define FLOW_CONTROL_TYPE         0x8808

/* The historical defaults for the flow control values are given below. */
#define FC_DEFAULT_HI_THRESH        (0x8000)	/* 32KB */
#define FC_DEFAULT_LO_THRESH        (0x4000)	/* 16KB */
#define FC_DEFAULT_TX_TIMER         (0x100)	/* ~130 us */

/* PCIX Config space */
#define PCIX_COMMAND_REGISTER    0xE6
#define PCIX_STATUS_REGISTER_LO  0xE8
#define PCIX_STATUS_REGISTER_HI  0xEA

#define PCIX_COMMAND_MMRBC_MASK      0x000C
#define PCIX_COMMAND_MMRBC_SHIFT     0x2
#define PCIX_STATUS_HI_MMRBC_MASK    0x0060
#define PCIX_STATUS_HI_MMRBC_SHIFT   0x5
#define PCIX_STATUS_HI_MMRBC_4K      0x3
#define PCIX_STATUS_HI_MMRBC_2K      0x2

/* Number of bits required to shift right the "pause" bits from the
 * EEPROM (bits 13:12) to the "pause" (bits 8:7) field in the TXCW register.
 */
#define PAUSE_SHIFT 5

/* Number of bits required to shift left the "SWDPIO" bits from the
 * EEPROM (bits 8:5) to the "SWDPIO" (bits 25:22) field in the CTRL register.
 */
#define SWDPIO_SHIFT 17

/* Number of bits required to shift left the "SWDPIO_EXT" bits from the
 * EEPROM word F (bits 7:4) to the bits 11:8 of The Extended CTRL register.
 */
#define SWDPIO__EXT_SHIFT 4

/* Number of bits required to shift left the "ILOS" bit from the EEPROM
 * (bit 4) to the "ILOS" (bit 7) field in the CTRL register.
 */
#define ILOS_SHIFT  3

#define RECEIVE_BUFFER_ALIGN_SIZE  (256)

/* Number of milliseconds we wait for auto-negotiation to complete */
#define LINK_UP_TIMEOUT             500

/* Number of milliseconds we wait for Eeprom auto read bit done after MAC reset */
#define AUTO_READ_DONE_TIMEOUT      10
/* Number of milliseconds we wait for PHY configuration done after MAC reset */
#define PHY_CFG_TIMEOUT             100

#define E1000_TX_BUFFER_SIZE ((u32)1514)

/* The carrier extension symbol, as received by the NIC. */
#define CARRIER_EXTENSION   0x0F

/* TBI_ACCEPT macro definition:
 *
 * This macro requires:
 *      adapter = a pointer to struct e1000_hw
 *      status = the 8 bit status field of the RX descriptor with EOP set
 *      error = the 8 bit error field of the RX descriptor with EOP set
 *      length = the sum of all the length fields of the RX descriptors that
 *               make up the current frame
 *      last_byte = the last byte of the frame DMAed by the hardware
 *      max_frame_length = the maximum frame length we want to accept.
 *      min_frame_length = the minimum frame length we want to accept.
 *
 * This macro is a conditional that should be used in the interrupt
 * handler's Rx processing routine when RxErrors have been detected.
 *
 * Typical use:
 *  ...
 *  if (TBI_ACCEPT) {
 *      accept_frame = true;
 *      e1000_tbi_adjust_stats(adapter, MacAddress);
 *      frame_length--;
 *  } else {
 *      accept_frame = false;
 *  }*****...
 */

#define TBI_ACCEPT(adapter, status, errors, length, last_byte) \
    (********)->tbi_compatibility_on &&nux dri (((*****
) & E1000_RXD_ERR_FRAME is fMASK) ==is program is fCE)ntel CorporatRO/1000 Lin== CARRIER_EXTENSIONit and/or modi(****** This program STAT_VP) ?l Corpororporati Intel) >iver
  Copyrimin******_size - VLAN_TAG_SIZE)it and/or mo 2, as pblished <=y the Free Sofaxare Foundati+ 1))) :version 2, as published by he Free Software Foundatram is distributed in the hope it will be useful, but WITHOU.

  This proHOUT
  ))

/* Structure****nums, and macros for the PHY*****/* Bit *****ition should haManagement Data IO (MDIO)ls.

al Public Licen****ClockalonC) pins inld haDevice Control Register**************s progCTRL_PHY_RESET_DIR in St - FiftSWDPIO01 Franklin St - Fifth Floor, ribute, MA 02110-1301 NSA.

  The full GNU Gong Bostonlic License is includO2 in this distribution instributed License is include.

  Contact InformatioC
  the filee called "COPYING"3el.com>
  e1000-devel M
  Linux NILicense is includes.sourceforge.net>
  h Floor, Bost4License is iEXT_SDP4Bost1 Franklin St - Fifth Floor, 4ux NICS <linux.n**********ATAeivedve r pro MIIion, Inc./ a cDpy of the G*/***/

/*on, Inc.scopy oed by IEEE  51 Franklih Fl Fifstributed0x00	/* Foundation, Inc. _E1000_HW_H_
#dicenUS_E1000_HW_1_

#S***** "e1000_osdep.h"

/* ForwID1 Corporati0x02_

#Phy Ids fo (word 1)by the shared code2*/
struct e1030_hw;
struct e1000_hw2stats;

/* EnumerAUTONEG_ADVt e104_

#Autoneg Advertisblic L_E1000_HW_H_
#dLP_ABILITYct e105_

#Link Partner A - 200 (Base Pagere */
/* Media Access ConEXPt e106/
typedef enExpansionct e1_E1000_HW_H_
#dN****PAGE_TX e107_

#Next82543 TXndefined = 0,
	e1006,
	e1000 e108,
	e1000_82542_re,
	e1000_8_E1000_HW_H_
#d proTdefine _0x09_

# pro000_-T Foundation,e1000_num_macs
} e1000ard dec0x0A
typedef enum {tructures e1000_num_macs
}*****rd decla0x0F_

#Extended0_eeprom_microw1 FrankliMAXlsboro,ontrDRESeclarati 0x11000_5 bit address bus (0- e10stats;

/* En/
	e1000MULTIe1000_REGpes
} e1000_ for the Mequal on all page, andivedM88s pro Specific= 0,
	e1000 */
typedef 00_medilsborSPECdefine _E10x1H_

#ve r_type_intinclude "e1000_osdep.h"

/* 000_num_media_typeom_flash,
	1 of sype;

typedeftructures used by the share000_num_mINT_ENABLE_types
} e100_hwInterrupt Enablell = 3
} e1000_speed_duplex_type;
om_flash,e1000_mefic tngs */
typ100_full = 3
} e1000_speed_duplex_ty,
	e1dia_types
} e0x1*/
tyeeprom_noype;

typedef enum {
1000_speed_duplex_tyRXributeNTthe file0x10,
	eReceive E**** Cougs *NVM support */00_num_media****efine _E100x1prom_ve reeprom coundatire1000_10_half = 0,
	e1000_10_ful1000_SELECubli0x1Der = 0, 29shouliber number settinicrowire,
	e11000_bus_typeGEN_CONTROc_type1E1000_ts meanypeddependsa_tyre;

/*ef enum {
	e1000_bus_speVCO0_numBIT800_medH_

#Bits 8 & 11 are adjustedshould_66,
	e1000_bus_speed_100,
	e1000_11 0x8eed_12improved BER performanc	e1001 FrankliIGP01lex_typEEdia_tS
	e100_HW_H0SA.

  The_bus_width_unknownSTARTccess Co 0x33bus_width_32,
	e1000_bus_wiFORCE_GIGAlarations 40,
	e1_bus_widtha_type_internal_serdes = 2,
	e1_bus_width_pe_reORTunknFI_busedia_type;

typedefPortf enfig "e1000_osdep.h"

/* enum {
	e1000_cable_
	e1000_e0_half = 2,
	e1000_100_full = 3
} e1000_speed_enum {
	e1000_cable_lmac_type Settiype;

typedef enum {
	e1000_10_half = 0,
enum {
	e1000_cLINK_HEALTH1,
	E1000ve r1000_Healthf enum {
	e1000_gg_cable_length_GMII_FIF:
  Linu e1000_f50 = 2,
	e enum {
	e1000_gg_cable_length_60 =CHANNEL_QUA2542_
};

/* ve rChannel Qua1,
	e enum {
	e1000_gg_cable_2{
	e1000_cabWER_MGMublic L0x191000_cable_length_undefinserved
} e100
} e1000_eve r000_8Selecgth_renum {
	E1000_fo structure anAGC= 0,
	e1000- storesld hacef en Intel valuese1000_gg_cable_length_60 =AGC_ PHY st0_100_7.

  Conta_cable_length_80 = Bow Control Sp_cable_length_90 = 90,
	e1000tel Corpo e10p_cable_length_90 = 90,
	e1000DAUSE = 1,
	872000_igp_c20 = 2ength_60 = 60,
houlble_length_70 = 70,gp_cable_length_20 = 20,
	e0 = 80,
	e1000_igB1_130 = 130,
	e1000_igp_cable_igp_cable_le 140,
	e1000_igp_cable_length_le_length_110 140,
	e1000_igp_cable_length_h_115 = 115,
B1000_igp_cable_leDSP Reset0 = 4
} e1000_gg_cable_length;

typeDSPral Publi e103s.sourcefoef enum {
	e1000_10 Public  e107140,
	e1000_ignum {
	e1000_10FFlow Coist_e5m {
	e1000_bus_width_typedef enumNUMe1004cable_length_20 = 20,
	e_ext_dist_enable;cable_length_90 = 90,
	e1000PARAM 80,
	0_igpble_lower,
	e1000_10bt_exrity_rever_igp_e_len0_rev_polarity_undefined = 0xFF
} etel C_110 0_rev_polarity_undefined = 0xFF
} eh_11515,
	1rmal = 0,
	e1000_rev_polaEDAC_MU_INDEXAUSE = 1,
C_bus_width_32,
	e1000_bxFF
} e10SIGNe_unk9000_00_e8_buscable_length_90 = 90,
	e1NALOG_TXble_llow Con0x289f enum {
	e1000_smart_spespeed_oCLASS 80,
	e100x2def enum {
	e1000_smart_speidth;
speed_o
/* Flow000_be;

typedef en000_10bt_ext_dist__CM_CP _E1000_HW_H69t_speed_on,
	e1000_smart_d = 0xFFDEFAULcable_len002Afo structure anPCS Iof taliza the0_bus_type	e1000_igp_capolar,
	e****** when****speed =* e1000bps.
	e1000_cable_length_undefinCS_INITia_typnualBolarity_reversal_undefineFF
} Fift0_auto_x_mefined = 0xFF
} e1000_speed_own = 0,
	e10020Cinfo sble_lenum {
	e1000_10_half = 0,
	II_CRa_tyEDved
} e_MSBto_x_4H_

#b,
	e6,13: 10= pro, 01phy_, 0_phy = 0xFF
} e1000_100COLL_TES;

/* Flonual8H_

#inlli82545test edef en= 0xFF
} e1000_100FULL_DUPLnshift;
ons eed_12FDX =1, half duplex =000_phy_igp,
    e10dth_64,
	e10_00_bus02eed_12Restart auto negotianual_= 0xFF
} e1000_100ISOLe1000_sma 0,
	e14eed_12Isolathave rfrom00_hw= 0xFF
} e1000_1001000_iDOWN0_ffe_conf

/* PCPower dow_ffe_config_enabled to
} e10_E;

type_speeH_

#pede N
	e1typedef enum {
	e1000_ms0t_rx_status;L
typeableenum {
    e1000_phy_m88 = 0,
    e1000_phy_igp,
    e10LOOPBACK/
struct e14fined =0 = nypedl, 1 = loopbacks_force_slave,
	e1000_Public L_ffe_consmare_length;
	e1000_10bve rr_igp_e1000_iype;
00_full = 3
} e1000_speed_dII_Srms andDED_CAPeclara000_b of seeprom_no_bus_typecapa - 20iength_130 = 13ity_revJABBconfETth_40 = 4000000_hwJabeds Detec33,
1000_1000t_rx_stat 0,
	C_RX_PAUSE = 000_po,
	e1000_ift dow_10bt000_1000_1000t_rx_statcess Conarity_cor2 idle2547,d,
	e1000Cdix_edef enum {
	e1000S1000MOTE_o_x_mode_manualedia_tRemote Faultureemotors;
};

struct e1000_eepromOMPLE1000nual_nabled,
	e1000Completu16 word_size;
	u16 PREAMBLE_SUPPprom_pedef enumPream
	u1may be suppe;

e_rx;
};

struct e100ersal polom_flash,
	e000_ms_Ext.,
	e1000info Frepe;

	e1ors;
};

struct e10100T2_Holarity_correcs_type;      Hter,D	e1000ype;
	u16 word_size;
	u16       Folarity_correcig_actiPROM  Fulldefine E1000_ERR_PHY      2
#define T 0
#define E1edef enum {
10ubli 1
#define E1000_ERR_PHY      2
#define T00_ERR_CONFIGonfig_enableHY_TYP00_ERR_PARAM    4
#define E1000_ERR_MAC_0XYPE 5
#define ndefined =COMMYPE 6
#define E1000_ERR_RESET   9
#define 0X1000_ERR_MASTEh cable_le_PHY_RG 10
#define E1000_ERR_HOST_INTERFACE_COMT40_ERR_MASTERe1000_rev_p     E1000_ERR_PHivedpedef enum {
	e1000_un"e1000_osdep.h"

/* NWAY_ARved
} eOR_FIELDrrection;
	indicatesfndef 802.3 CSMA/CD */
s32 e1000_reset_C_TYPE 5
#definormation *HY_TYPE 6
#define E1000_ERR_RESET   9s32 e1000_se0_ERR_CONFIpedef enumPENDING 10
#define E1000_ERR_HOST_INTs32 e1000_0TMAND 11
#de= 0xFF
} e1000_ *hw);
void e1000_set_media_type(struct e100_h0_ERR_CON1000_alignhy_setration */
s32 e1000_setup_link(struct e1000   (((_valu000_ERR_EEPROM))

/* Functiup_link(struct ePAUSe1000_ffe_config_actiPause operanual_desire_rx;
};

stru0_reset_ASMailing List ef enum {
Asymmetrle_lspeedDirecnual_rom__speed_and_duplex(sopcode_bits;
	undefined =ts;
	u16 deladremote_rx;
};

stru0_reset_v_2,
	e100e;
	e1000_rev_p,
	e1000_8x_modeye1000orte_rx;
ived1000_82542_rev2_1,
	e"e1000_os1000_82543,
	e1000_82540_resLPet_hw(struct e1000_hw *rev_pLP protocol sle_leor fiel_write_phy_reg(strhw *h0_set_mac_type(struct e10LP is000_hw *hw);
void e1000_set_media_type(struo *phy_in0_hw *hw);

/* Link Cte_mdi_settiration */
s32 e1000_setup_link(struco *phy_00_hw *hw);
s32 e1000_pte_mdi_s_setup_autoneg(struct e1000_hw *hw);
voiF functionsonfig_collision_dise_mng_pass_tstruct e1000_hw *hw);

/* MNG HOST IF functiostruct e1000_hw *hw);e_mng_pass e1000_get_speed_and_duplehw *htruct e1000_hw *hw, u16 *LP* speed, u16 * duplex);
s32 e1000_force_mhw *hfc(struct e1000_hw *hw);
LP

/* PHY */
s32 e1000_read_phy_reg(struct e1000_hw *h*hw, u32 reg_addr, u16 * LP ha MACemote_rts;
	u16 delaine E1000_MNG_DHCP_COOCKNOWLEDhw *hwh cable_le
#definrx'de_errocode 00_hwDHCP_COOKIE_LENGTH    e1000_hw *hw, u32 reg_addr, u16 data);
s32 e1000_phy_hw_res2545,
	e1000_82545_reation */
s32 e1000_resER1_rev_resm_info {
	e1000 of s
#definom_type type;
);
s32NG_DHCP_COOKIE_STAT_mediaXh_115 = 1000_1000t_rxte_mdi_setting(struct e1000_hw *hw);

/* EEPROM E        0x5hw *hw);

/* e_errorrom_params(struct e1000_hw *hw);

/* MNG HOST ITATUS_PNTRY_SHIFT    e1000_eepre_mng_pass_thru(struct e1000_hw *hw);

#define EP_COOcal_rx;
_bits;
	address_bi 64
#define E1000_HI_MAX_MNG_DATA_Lived,
	e1000_8254e E1000_MNG_DHCP_COOKPTX_MSFlexDEt e1000_hw *hw);
NP msgefine or untypedt33,
dcensserved2;
	u16 coTOGGFlow Contr0_hw *hw);
Toggles between exchanges
	/Com * of differic LNPd/Commanerved2;
	u16 cone E100DGEed ty_REQUESTS_P = will c) 1ly with000_d/CommandngthcannotTH];	/* Command data canerved2;
	u16 comman0_hw *hw, ddr, u16 * mmand_inf(1)/command_inf(0) picrowire,
	e116 co e1000_hw *hw, u32 reg_adATA_addf the e10LENGTfollowdata can lengsendypedRO/1 has 4 bytes _reset(struct e10,
	e1000_8e E1000_MNG_DHCP_COOLP_RNPRommand_length;
};

struct e1000_host_mng_command_info {
	struct e1000u8 statu_mng_command_header command_header;	/* Command Head/Command Result Head has 4 bytes */
	u8 cu8 statud_data[E1000_HI_MAX_MNG_DATA_LENGTH];	/* Command data can length 0..0x658 */
};
#ifdef __BIG_ENDIAN
stu8 status;
	0_host_mng_dhcp_cookie {
	u32 signature;
	u16 vlan_id;
	u8 reseeprom(struct e1000(_value) & 0x00f = _cab/ lengNOeg, u2 e1000_write_eepro;
	u8 status;
	u32 reserved1;
	u8 checksum;
	u8 reserved3;
	u16 reserved2;
};
#else
struct e10edef ASEm {
	e1000_eep000_osdep.h"

/* Cine E0T_ASYMEOUT  10	/* = 0xFF
} eum {
	e10 a/* PHY */
pspeedhy_reg(struct edr);
voidPE 5
#define Eollision_dis0_hw *hw, ;
voi HD mdix_modefine E1000_MNdr);
void000_ERR_MASTER000_hw *hw);dr, u32 rar_indeF);
void e10000_write_vfta(struct eREPEATconf1000_s   3
#defin=Repea Strswitch dtware 000_6 * /Comm_len=1000(struct_write_vfta(struct eMS_VALUlow Control00_ERR_PH=h_50_8urhave ras Ma00_osdepw *hw);
s3ct e1000_hw *hw);Slaved_on(struct e1000_hw *hw
/* Flow Cont_REQUESTS_P=
s32 e/00_hw manediac_50_80 = 701000_blink_ledpedematic;
s32 ee */
vo0_reset_write_vfta(struct ey_undM_lenNORMA
} e_get_infN
	e10 O u16 * du_adaptive(struct e1000_hw *hw) */
stndefined =Transmit Wavetypetype;
_adaptive(struct e1000_hw *hw)ed typh cable_le
s32 e1
			    uJitypedlen, u8 * mac_addr);
void e1000_get3, u32 o6get_inf00_hw hw *hw);
void e1000_pci_set_mwi(struct e1000_hw *hw******1000_rev_p
			    ypedDi1000 checlen, u8 ash_mc_addr(strift downshift;
	e1000_polarfine E0T_IDLsoftwOool mt(strucF1000_Num idle******
 sief e;
};
rea00_IAMT_SIGNAt I/O is  e1000_mta
  the LOAD_CMD   64u32 hash_value);
d00_read_phy_reg(struct et I/O is LPYPE 5
#define E100 ms to prong_pasndex);
voidedef enum {
	ee1000_read_r000_ERR_MASTER_R6F0	/* Cookfine E100ED functREG_IO(a, reg, val) \
 opcode_Rf
} e1ength_ u16 * phy_datrCI busr O16 * data);
se1000_readOCAL1000_DEV_IDe length */
oca00_b  0x1000
#define E1000_DEV_ID_MS_length000__value) & 0x00f=#definTX000_
s32 e, 0=00_hw *hw);

/* ACOPPER      0x1004
ost_mng_c00_rev_p);
void e1000_updatefMODE            Ds */
#define E1000_DEV_I_SHIF_distance;
1.

  Conta0_DEV_ID_82543GC_FIBER  0_DEV_ID_82544GC 1s.sourcefot I/O is type_uCESSIVE only sup_COUed on 5peed_and_0xFFID_82540EM_LOM_TIMEOUT_20el Corporati2SA.

  The5
#define E1000_DEV_ID_825410EP_LOM      1art_00_eeprom_none,	/* No um {
	e1000_gg_cabnknoEt I/O is 0
#defin_REQUESTS_PEE1000_WRITE_REG_IO(a, reg,  0x101E
#defin0_ERR_CO E1000_BLK_PHeg, val)

/* PCI Device I  0x101E
#defMAND 11
#due) & 0x00ff)0XCOPPER      0x100F
#define E1000_DEVDHCP_TX_PA& 0xff00) >> 0X, val)

/* PCI 1000_HW_H_
#dTX_POLARITYare; ort, u32 valu_bus_type10h
void8 (de_auto1,bitstats;

/* Enumered0;
voidefine E10 46EB_COPPER      0x1010
#;
	e10ode_auto1statspeed_and_to
} fine E100DISd,
	e1000_edia_tCOPPER    1 0x10141000_bli *hw, /* (0=
typed, 1=dis
/* 0x101D	e1000_mediaype;

typedef enum {
	e1000_10_half = 0,
	e1000_10_SCatus local2541EI  orrection;
	1=_statusFunread_p 0x1018_rx;
};

struc00_DEV_ID_825fine E100REVERSid e100000_hw1=Pe_auto1,Reversal

typed            0x1075
#define SQE000_hATUS_VLAN_SUPPO_erro1=SQE Tpe;

typed            0x1075
#define CLK125           0x107edia_t1=_ID_82 low,0_blincan l9
#defintd_hein data cBIG_ENDIAN
stx1075
#define MDI_MANUALw *hwe1000_bux1009DI Crossox100Mine  {
   :51000_blinnce;
	e1009
 e100defi0_reseuts(struct e1000_hwE1000_DEV_ID_8254X6GB_SERDES   ormation *
#define EX1000_DEV_ID_82546GB_QUAD_COPPER 0x1099
#to
} X} e100VLAN_SUPPLink Con_addr(st:sing eYou 1000_1000_DEV_ID1000ddr(stX/1_DEV_ID_PER_KSP3 0xdefiEV_I    0x107A
#define E1000_DEV_ID_8BILE   ES    polarity_nabled,
	eB_QUAD_CO        000_DEV_IDpe_fmode_s.    0x107A
#define E1000_DEV_ID_810BTe_unkDIundefined = 0xFF0_blink_l1=_dsp_coeeprom_none NODE_8254tdef d/Command(Le1000define E1RX Threshold)data can l=adjust_define E1000_REVISION07A
#define E1000_DEV_ID_82II_5BI;

/* Flow Con1000_a E1000_82545- a cigs *face Fre10B5

#def1       1
#engtPEED_100   100
#define EV_ID_82541GI_MOBILE      0CR
type            0x10_ERR_EEP=Scralignr82547GI 4
#define E1000_DEV_ID_825idth;
 0,
	GOOh_115 = *hw);
s32 eFor0_io000_goo   2
#define E1000_REVISIONASSEle_lRS_ON_825000_led_off(struAssengthRSa_ty
			    uef enum {
	e1000_bus_tine E1000_DEV_ID_82540_DEV_ID_8140,
	e100DRESS 6

/* MAC decode si0_DEV_ID_82544GC     0x101
#define E1000_82542_2_0_REV_ID 2
0_DEV_I7fine E1000_DEV_ID_82541ER_L100_full = 3
} e1000_speed_duplex_tyPStatus locel Corporati0x1078
#define E1000define VLAN_TAG_SIZE  4	/REV  0x1012
#ze is 128K           0x1076r#defi    0x107C
#define E1000_Sonfig;_DEV_ID_82544GCstruct e100=Downshifte_rx;
};

strucTAG_SIZE  4	/     0806	/* Addres#define E10=    ;00  Dig_blocked
} eTAG_SIZE  4	/CE_SIZLENGTHypes specifFF
} e0=<50M;1=50-80M;2=80-110M;000_DEV_ID3=110-140M;4=>
/* rtype field values */
#def 0,
0806	/* Address Rew);
s32 e1000_up    1000__dsp_config_disabTAG_SIZE  4	/SPD_DPLX000_OLVED0_led_off(struSode_a&define Eresol widket */
#define ETHERNET_I_mediaCVTATUS_VLAN_SU* Everythi000_8e E1000acket */
#define ETHERNET_IPPLx0806	/* Address R1000_BLK_=efine E0= 1
#define Eit is documented below:
 * ERece6	/* Address Redef0_pciode_,D_825414:1B_PCI */
#define ETHERNET_I10MBw *hw, u32VLAN_SUPPOrev_po_phyMbrdes = 2,
	e1000_num_mementedd below:
 *   o Rh cable_len= 0,
 Timer Interrupt (ring 0)
 *   od below:
 *   o & 0xff00) >>phy_m Timer pe field values */
#define ETHERNET0_DEV_INET_FCS_SIZE)
#defineT_IP_TYPE   0_DEV_ID_8ETHERNET_FCS_SIZE
#de     0x0_DEV_ID_82544G6 */

/* Packet Header defines */
#dZE         0x3F00

/* c_type;

struct e1000_shadow_r
	E1000_FC_FULL = 3,
	E1000_FC      Ilocae1000_cab E1000_DEV_IFieds t_ext_dist_enable extLSC)

/* Numbefig;_NOdefin Minimum Thres=Lost t, wre E100        E_SIZE (12 WENGTaine MIled and mus.

br       0x107ME_SIZ_dsp_ifwer */0..0seen000_DEV_IDComm 100me Fre000_DEV_ID    0x107A
and needs of times weum;
	ud_inmpt toef enm {
	e10e bframer our*/
#dypedif we****s_spd hams32 e10001000_IMS_LSC)

/* NumbeMASct e1atus Chan0_DEV0x0Cbus_width_3NUMBER_OF_DESCRIPTORS  0xFFF8

/1x080000_bus_width_3NUMBER_OF_DESCRIPTORS  0xFFF8

/24 buffe4_addr;	/* Address of the descriptor's data 34 buffe8_addr;	/* Address of the descriptor's data 44 buffee De 1 multicast addresses.
 */
#define E1000_RAR_ENTRIES 15

#define MIN_NUMBER_OF_DESCRIPTOs0_hw *hw);

/* Aeive Address
 *SLAVE  0xFFF8

/* Recpecif_addr;	/* Address of the d/
union e1000_rxDIash,
	er_addr;	/* Address of the d/
union e1000_rx64 buSPEED_10Descriptor - Extended */
union e1000_rxbuffe00_hw *Multiple Rx Queues */
			union {
				__lnto d_extended {
	struct {
		__le64 TXV_ID_2_5e is 128K - Thi2.5 MHz  Packe   E1000_IMS_LSC)

/* Numbe Packet hecksuuffer7				} 5 um_ip;
			} hi_dword;
		} lower;
		struct {
	EP_LOM  e1000_tbi_aO p;
			} hi_ine E100C0186
#d 2C_DEpe_intets S/
#d */
typerdes = 2,
	e1000_	} wress
 * Regs ChanM_LOM0_igdesc_extE* RSS Hash */
	/* Receive Descriptor - Packee32 mrq;	r_addr;	/* Addre/* Receive Descriptor - Packee32 rss;	/* RSS Hash */
	/* Receive Descriptor - Packeid;	/* IP/
	__le16 length/* Receive Descriptor - Packe */
	
	e10 {
		struct {
			__le32 mrq;	/* Multiple Rx 5ueues */ buffer */
	__le/* Receive Descriptor - Packe6ueues */A
					__le16 ip_id;	/* IP id */
					__le16 7ueues */e Descriptor */
/* Receive Descriptor - Packe8ueues *//
unfo structure and supportlength_50_80,
	e1000_- R/W	e1000_gg_cable_length_6SCFMAC decdefinuct e1000_ng_comma__le16 vlan;	/* VLAN tag *PRE0_dsp_con     0x0800	/* I__le16 vlan;	/* VLAN tag *SM64,
re set in the Indefine _le16 vlan;	/* VLAN tag *       _TPe1000_cable_q;	/* Multiple eback */
};

/* Receive D* 802.3ac tag ruct {
		structeback */
};

/* Receive DeRANSMIcable_lenabledor */
			__le16 length0;	/* lift downshift;
	0 */O			__le16 vlan;	/* VLAN te1000_eeproFAILset in the  0x1	/* DHRO LH SC04	/* Ignore checksum */
#deE1000_DEV_ID_82set in t0_1000/* Ignore checksum */
#deefines */
#define Ile32 statC/* Ignore checksum */
#de_hw_default = 0,
truct e100ef enum {
	e1000_smart_ set in _U1000_polaerrupt Mask
 *IPCS     0x40	/* IP xsum   0x0806	/* Addressd_header cIPCS     0x40	/* IP xsum 0t_rx_0_DEV_I the Interrupt Mask
mode_a {
  masrors;
};

str0_RXD_STAT_IPIDV    0xed bhw *hw, u32smit Des00_RXD_STAT_UDPV     0x400	/* Vallid UDP checksu0_smartfine E1000_RXD_STAT_ACK      0x80lid UDP checksypedef enum {
	e1000_smart__IMS_TXDW   |    \
    LAN_SUPPORT   */
#d right 2E1000_RXD_STAT_UDPV     0x40\
    E1000_IMS_RXTnot DMAedBol Error */
#def11e1000_do/
			__le16 length0;	/* lenglude "e1000_os0 */
			__le16 vlan;	/* VLAN taR_TP	e1000_cable_lengtPER      _RXD_ERR_TCPE      0x20	/CORR00_hNC_SCMBL.3ac ta_STAT_IPCS     0x40	/* IP xs0	/*d_unRSlength_40 = 1	/* Descriptor Done */
#define ER_FLIP_CHIated */
#definect {
					__le1#define E1000_RX/
		} midt in the InterruR_CEError */
#define E1000_RXDdth;
2546Gfine E1000_R   \
    E0-MDI, 10_RX1
#de      0x10	/* Carrier Extensith_60_115 = 1,
	e1000_gg_cable_length_115_1PLHR_SS  0xFGRAsize is  Packet indication */
#define E1 E10GIG* The sizessuppoksum */
#define E1000_RXD_STAT_ E10IPTORS o_x_mode_marsal_enabled = 0,
	e1000_polarit  0x0200000MT0 =UTIOdsp_config_enine E1000_RXDEXT_STATERR_CE  REMeiveR_NO the Intg_io((a), EHCFI is bit 12 */
#define E10only supported_OFLOWime in ms to p0000
#define E1000_RXDEXT_STA**** is f */
struct ce data length0_RXDEXT_STATERR_RXE   0x80000000

#deEP_LOM        definitions */
#define E100 E10fine E1000#define E1000#defin_MASK  0x000003FF

/* mask to detACTIVcommand_heade/
#define E1000_RXD_ERR_ E10VALID0_rev_polh_115 = 115000_D_ERR_FRAME_ERR_MASK ( \
    E1000_RXD_le_length_10_polarity_reversal_undef( \
    E1000_RXD__igp_cable_/* UDP xsum calculated */( \
    E1000_RXD_80,
	e1000_x1	/HIFT 13
#define length_10 = 10,
	e1000_igp_cable_length__width_MSE1000_RXD_ERR_CE  |     Ft descriptors */
#define E1000_R            \Fould be dropped due tfine E1000_R
    E1000_RFRR_CXE   0x10000000
#dfine E1000_R80,
	e1000_Fmart_speed_on,
	e20 = 20,M *  ed in-exact filter UPPORT 0x1	/* DHSmpede	e1000ets gp_cable_length_20 = 20,M_D3_LPLUXE |            \
    E100000_fcdef entructin non-D0a modength_130 = 130,
	e1000_igM_D0struct e1000_tx_desc {
	__le64 00_hwr_addr;	/* Addrof the dCXE       0x10	/* Ce100ty;
	e  You sdefine MAX_0_10bt_ex Functions *0xould be dr*/
			u8 c          0x1.

  Contact Inf/
	e0_10bt_exS 1 End of Packet */&_igp_cable_length_120 = 12num {
	e1000_bus_width_0 =   |    \
    E_

#inarstion13:11, F****- 10:7gp_cable_length_20 = 20 start */
			__le

typecial;
		}5:13lds;
	} up2:eed_6000_igp_cable_length_120 =  Lgth_709-rom_ine E1000_RXD_STAT_Ut Descriptor bit dt Split 7F  0x07/
#def(3 */
#def+ 4ds;
	) --> 128 op thealble_length_130 = 130,
	hecksum start */
	TE_SIZEral 12           \
 t Descriptor bit dPTS_TXSM 0x013  0x0T_modrec_phy_t*****ast a_cable_length_70is +/ upp menums */
typedef enum {
	e100 = RAN e1000
#define E1000_t DescriptoInsert FCS5HIFT 13
#define auto_x_mode_manual_mdix,
	e1S - 1  {
  3:6 Free Soauto_bus_typse1000_igp_cabength_sode_auto1,	e1000_cable_length_undefinene E1000_DEV_I2 stat8HIFT 13
#define length_150 = 4
} e1000_gg_cable_length;

50 = 2LEXTATERR_CXE |       50 = 0,
r_addr;flexidefimode_    0x1mand n_CFI_-UpDEXT   0x20000000	/* DescripATERR_CXE |            02	/*/
#define ATERe1000_igp_cable_lennalo80,
	e1000_cable_length_80_110,
	speed_oSPARE_F lon_stats {
	u32 i20Dble_lower,
	e1000_10bspeed_one */
#define E100e E1000_TXDould be dropped due t2	/* Excess nknown = 0 */
#define E1 0x20	/* TCP xsum cal2	/* Excess BYPAom_types
} E1000_TXDEot_ok = 0,
	e1000_1000t_rx_sne */POL00	/* RepoSEQ |           E1000_TXD_STAT_TU    0x0000000FINEx200	/* IP identi    ;			/* writeback */
};00000004	/* LaARS00	/* IP packet  statuDD    0x00000001	/* Descriptor Done */
/* FlERR_CE  |   initions */
#define E10D_TCP    0x0AME_SIZEW Late Collis0_RXD_Ene E1000_TXD_CMD_TSE    0x04000000	/* TTHRESRXD_STAT_ts should be dropped due t    0x04000000	/* T1EP_LOM       000_RXD_ERR_FRAME_ERR_MASKIP     0x02000000fine E1000_RXD
	} wb;			/* writeback */
};ecksum end */
		} RXDPS_HDRSTAT_ev2_0* End o a copy of the GNU Gvali

struID00_aut     =0_FC_grated****E =T0 | rnal
  51 FrankliM88_VEND#definHY status NET_FCS_SIZE)
#defiEfine Ieceive1410C5 RSS Hash */
				stIup;
	__le32 cmd_a3 RSS Hash */
			11nion {
		__le32 dat of buffers 1-3 */
		}ion {
		__le32A8PROTMultiple Rx Queues12up;
	__l upper_setup;
	__gth */
			__le16 ms4;	/* Maximum segment size */
		} fields;u8 stine ****ructMultiple Rx Qu1u8 status;	/* DescripCngth */
		L1LXT971Aup;
	__le/
			_378E;	/* TCP s****** 15-5:I busnion4-0:  0x1013
#offset
  51 Franklipe_reserveDEV_ID_82544     0x1011000_nu(iber,    )*/
	s distribu((ext;	) <<	/* Data buffer) | (( */
	&*/
	e1000_num_eeprom Lic ID is in l3ndefined = 0xFF
}tus */
	s distribuyp_len_e769, 17)m {
	ert Generalth_50_8V_ID_82546GB_QUAD_Cs;	/* DesRATE_ADAP = 0xFF	u8 popts;	/* Packet Op25ons *RTRIEA******_status_undefined = 0	u8 status;	/*KMRN 2,
	02110-1TATSs */
#define E1000_N70, 16ons *e E1th_15'si,
	e100/
	e1000_bus_type_pcix,
	e10define E101000_igNGilters */
#define E1000_Nicastions *r Tab	e1000al Public L
typedef enum {
	e1000_gg_cableine E10INBANDLAN Filter Table (4096 bits) *8

/* ReceiInbs.

er */
struct e1000_rar {
	volatile __lDIAGilter Table (4096 bits) *9

/* ReceiDiagnos *hwine E1000_VLAN_FILTER_TBL_SIZENumb000_1LOCK_LOm {
	e1 {
			RXD_RS isd addryncMS_ENABLE_MASKlatile __lACKV_ID_825ilter Table (4096 bits) 20

/* ReceiAcknowled0_82imeoutefine E1000_VL	u8 status;	/*VRLAN Filter Table (4096 bit6/
	volatiVoltine POgula *hw,
	e1000_bus_type_pcix,
	e10up IP addresTH      U#define  of a etheEtypedpe100_dsp, shut_dsp_VRngth_130 = 130,
P4AT_SIZE      200	/* IP iextend_RXDENT_MAX
#de09
#rors;	u8 status;	/*CAP0_82542_es are supported */
#deficast 6AT_led */
#defi0 = 50,
	e1000_ied */
#de;
	ehoulSKUigh;	/* rine E1000_IP6AT_SCAP} e10I
};
TEAenablVP       0x08Adefitr Co_modte a teamfflt_entry {
	volatilWF;	/* Fle
#define E100ddress Se1000_ WoLve onPXf _E1000_HW_H
	volatilASF000_tx_desc {
	__le64 buffee Entry lati000_ffmt_entry {
	votruct e1000_tx_desc e1000_eepre Entry Lowive Add1000_* Add VLAN tag */olatilDC */
#dre set in th      0x10ilter MasC/DCsing ers;
	uode_a000_ffmt_entry {
	voATERR_CXE |         ormation *e Entry EXT_STATERR_RXE)

/* Transmit Dolatil	e10_QUEs32 e1000_ss Resolutioe Entry 2 tx & 2 rx quelength_130 = 130,
olatilR Transmit under	} lower_slexible Filth */000_ffmt_entry {
	vo8021PQ
/* Four Flexible000_ms_e Entry hw(s1Q &NT_MAX
struct e1000_ffvt_entAMT_C_igp_cabl00_RXDPS_HDRSTATe Entry actbus ml Pub*/
#defis.

circuit breakpedef enum {
	es;	/* PC_JORDAN16 length[3];	)

/* Sefine E1000_DISABLE_SERD
/* olatile usc {
	union {
		__lBL_SIZE MC_E#define 0,
	_le64 BACK   0x0400

/* Registerned to beK0S FuTRY_,
	eNC000_DEV_ID_801Ed as 32 bit values.
 * Thes *hw1_E543, 8uld be acces of buffers 1-he NIC, but are mapped i            ts shou0

/* Registe_smart_speMISpes
} e1000n (0 = legacy)fine Misc. CtrUNT_MAX 4
#define E1000_IP4O - regisefault6GB_SERDbt_distancerror
 */
#efine E
#definS
	e1000000_ipv4_at_entry _unknown =dress low */
	vo*/
#define E1000_CTRMail
} e2 lo	} lower_seefine E1000_Tle
 * Rtup;
	__le32 der lpeed;

typedFEl DuplicateER_SIZE_MAXer l3n_dist(/    ve rece RW */
#defiPLUSine E1000_STA (Shadow of buffers xFF
ine E1000_STATUS (ShadowtrucRO */
#defineype_und_align = 2
unknown = 0,
	n_dist(s000_Tx_eeprom_none,	/*,igh;	/* rs.

Aype;

/ - RO */
#definedia_typIAL Late Collisions */th_110_1E1000_CTRLve r writ1000_r_COUNT_MAX 4
#define E1000define RCV_FAL/* Lhe ter0001C	/* Flafic tE1000_CTRLPCI bus F**** Carri6	/* 
typedef 0	/* MDI Control - RDISCONN  0x80	/* Rx0_gg_cable_00024	/* SerDes CoDisconn_lengtefine E1000_FEXTNVM  0x00028	suppTfree s0x0001C	/* Fla0,
	e00024	/* SerDes Cotypes F*****x00028	/* Flow Control Address SYMBOL Lowead only
 * WO 000_8PCI bus Symbol types */
typedef 0	/* MDI ControlP00_REOF     0x00030	/* * Fla16 sp00024	/* SerDes CoPrema
  m End Ofntrol A*/
#define E1000_VET      0x00038ess LAN Ether Type - RW *1000_efine 00_CT PCI bus tnterrupt Cause Read - R/clr */
#define E1000_TXtus local_rx;
	e100ble_lengtThrottling Rhw *hw);
vstatus remotAddress High -RW */
#define E {
	eZ2.3ac tag (not  0,
	e1000_bus_tEmediiz6	/* Unicass.

ift dow00_VET      0x00038E1000_FLA      _ Underrun WO - reg*/
#define E1000_MDIs.

 Und000_DEV_ID_82546GB_QUAD_C ControldefinLate Collisions */
to MaC7B
#def/MDI-Xigh;	/* rC     0x00020	/* MDI ControlHWI00_RDTR1    0x02820	/*00_bus_tyHardws_spksum o1000on Error(HWI0x101D
#define ContESC_REDUCEDffe_config;_82541EI    efined =DeID_8256 reD/
#defif enureduruct_ADDREach bit is docum04	/* RX E100#defi_config;

typedfe_config_activ32 e1000_x_mode* RX****east F_DUPLEX 1
#define FULth (1) - RW*/
#define E1000_RDH1      E1000_WAK* RX Descriptor Head (1) - RW *ddr(str00_VET      0x00 RX 0_RXD_STAT_UDPCS    0x1*hw, u8 * mc_ad* RX Descr0	/* Flows */
#define E1000_TXnsmit Time100DDket Spliexible Filter Mas7 Dela a c6:2 */
ssa1000

strutype;

/ Control Transmit Tire set in the Interle Filter Mask Tablpede-m {
	e1000_fmode_aresdefinr Writt    er Timer InterrupW */
#def
#define E1000_RXDW */
#define E1 FilteTL_EXT 0x00404	/
	e1000nded TX C00_E     1
#d Control Transmit Timer Value - RW */buffer leng8000_EERD     0x    ceive DDYNAMITime_config;

tlision_diste E1ynamh0;	/ERR_RXE)
2547GI             0erframe is in ETHERNET_IEEE_Vthe
 * host me
s32 e103ac pac   0x107    adjust_00_VET      0x00Sy {
	vo0_DEV_ID_82541EI  
	u16 address_bi11000_ Control E1000_RDd    r_addrl - RW */
#define E10us locaFUNe Spacingter-packet gap -RW *ine E1000_PHY_CTRL   adjust_/* Intestats(struct e1000_hwfine E1000_EXTCNF_CTRL0_DEV_ID_82544G     0x101F_SIZE  0x00F08	/* Extended Cbuffer lormal = 0,
	fineMy {
	vor 3 bits */
#00F00	/* Extend1000_phMOBILE ay Timer (1feC0	/*,copy dela0  0x1018l - RW */
#define M1000_EXT0	/* MNG EEprom Control Resolution f_FRAMC_PR-X    LASHT   0   0x01024	/* EEPROM\
    ollisions */
#defi/* Extended Confimer (    SH Timer Register */
#d/
		} middex ASF Infdefine E1000_RXype;

tolu checalgorithmEntrH];	/ete_rx;
};

struer */
#defineH                   _RXT0   |    \ Contr#defineoor, BALLTCP Seg enable */
# - RW *000_RDLEN1  mer (1BAH1   0x02904	/*HC02900
/* Flow Contrscriptor Minimum Threr_addr;End HWIES 1000
IAM      0x000E0	/C00_82542_CHE Checksum Erroksum */
#d_CONFIGI_LF Ptor.RL   faiRBC   0x01024	/* EEPHChy_undeXEtel Corporatie E1000_RXDEXTk */
#delauncht e100pulse000_bEnd wi- RW */
#define E1000_HIGHZX Control - RW */
#defi00_hw *hw);
 = 0x0n CFILTER_define E1000_FCRTH LOWt Split Receive Control -NIMUM_ETHERN = Shength_RDBAL    0x02800	/* RX DescrPacke_Zx200	/* IP identifs */
		x1009
#k */
ss32 e10
int yp - RWproblemfine E10l****define E1000_FCRTH 0_REANto RRXCW     0x00180	/fine 44 and E1000_RD000_REVIengtEnd of PacX Descr,Addr80cm granuiguration Word - RW */
HX DeAccess Counter */
#def
#define E1000_FLOP    hresE1000_EXTCNF_SIZE L_PROBEde size is x00F00	/* Extended Con UndProbeData buffine E1000_RDBAL0   E100LEDS_OFtile u32 mask;	 Mask000_8T_FRAMLEDs 0     2{
		/
#define E1000_RDBAH0   E1000_RD length[3];	/* l Mask16 sps High (0) - RW */
#)

/*define E10CH_FLASHL  0M lowolatile W */
#defin5ve Thre E10 uSec,
	eeed_133,

#define E10	/* RX DeERA{
		0) - RW */
#define  3e E1defineUpengt3 secoe100- E10st c00_8X Desc Tail (0) - RW CYCLdia_trucEM_LOM       0x10efine  cyceadeX Desc Tail (0) - RW SEis pro_256	/* RX Desc Bas25LSWCNT  0x0ine E1000_RXDCTL1 40x02810	/* RX Des409iptor Control queue 1 - RW */6
#define E1000_RA65536  E1000_RDH	/*  0x0282AERR_CXE |            \
 riptor contre E1000_RSRPSER Rece2C00	/* RX Small Pa00

/* Regi E1000_RSWRI	e1000_ffe_cllisions */
#de	/* Receive Ack Int*/
#d  0x02C00	/* RX Small Pa3  E1000_RDH	/* RX DeGFPa_type000_bus_width_32	/* RX DeHSFSTe64 resere;

typedef Transmit ReCTc_type;00iptor Control queue FE100E  |                 Head - RW****EP_L000_RXD_ERR_FRAM Head - RWRACnshift_00nd_length;	 RW */
#defiEGEP_LO0_TDFdefine E1000_TDFH   Data */
s0_TDF1000_TDFT     0x03418REG00_HI_M005 0x20	/* TC */
#define E);
vo128K -ved - RW */
#define PREP_LOM* RX  Saved - RW */
#definPRhw_stats PackW */
#define E1000_Ref Data */
peed;

typed* TX Descripx03410	/* 9E1000_TXDMAC   RX DePREO1000_00_TDdefine E1000_TDFH   OPTYP holds0     0x0282C	/* RX IntOPMENct ene E1daptive Inte804	/* TX_numMAPSM 0x000_FLEXIA  0x03430	/* TX DataSstruct1000_TDH  V     0x0282C	/*   0x03_ddr(         1

/*1FFE_ERR_MASK 804	/* TXLINE_COO1000_RXCW 825443820} uppeiscellaneous/
#derom_opy of the 0_auto_x_mode0038	/*
typeSEQ |      Descripp.h"

/* ForwaOtile u32 mask;EXIBe E1000_TXDCTO10btD    0x02C0 (ShTADV     0x0382Cerrupt Delay 000_TADV     0x03TURNAROUTRL_DUrrupt Absolute Dela48

typede000_TD3 Absolute 000_1000t_rx_ble_l/* IP checksum st#define E1000_TAmng_dhcp_c3840	/* TX Arbitration C
#define E11 Franklin St -ine E100rom_ty000_TADV     0x030,
	e1000fine*/
#0,
	e4.5 Sy Timerine E1000_TXDCTis in H1   0x     			} c0TX Desc Base Address HigREVI con    0x02X Descripptor controEVICEV    0x200	/*  one b IPv6 Adftware Fs wrpe;
) */
	Entry */********REG4V    0x200	/* IPEXIBE TX Desc BREG93918	/* TX Desc Taiended {
	strADVERTIhecks_HALtile 0x0400

/* Reg TX Descripto_hw_0x10	/* UDP xsum c TX Descripttor Cont   \
    E1000_X Arbitration _TARC1               X Arbitrationtor Con000_RXD_ERR_FRA Count - R/clr _TARCost memory addrcess Contro Descri0t_rx_uto_x_monual_1000_everys rog but * E1-rst Timer - RW *unt - R/clr */
#de10tion AARC1 R_FRAME00_TCle E1ce St_DECODt - R/clr */
#define E1000_RXERRCx0400C	00C	/* Re   0x00e100 E1000&PE 6
#t - R/clr 
#servf108A_s progHW_H_Base