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
    (********)->tbi_compatibility_on &&nux dri ((ver
  
) & E1000_RXD_ERR_FRAME is fMASK) ==is programoftwaCE)ntel CorporatRO/ pro Lin== CARRIER_EXTENSIONit and/or modiver
  C Thcan redistrSTAT_VP) ?nd/or moor modii It an) >iver
  Copyrimin
  Cop_size - VLAN_TAG_SIZE)ditions of t 2, as pblished <=y the Free Sofaxare Foundati+ 1))) :versionibuted iun the hoby  will be ustwul, but WITistribudistributed init wihope it will be useful, but WITHOU.

 neral PubHOUT
  ))

/* Structure Copnums,tion macros forit wiPHY  Cop/* Bit   CopitRRANshould haManagement Data IO (MDIO)ls.

al Phoutc Liceware ClockalonC) pins inGenerDevice Control Registerer
  CopFranklan redCTRL_PHY_RESET_DIR FITSt - FiftSWDPIO01 Frankln, MA 02110-h Floor, TY or
, MA 02110-1301 NSAe GNU Ge full GNU Gong Bostonprogram; seoftwincludO2 FITNENTABILITY orRRANinLITY or
  e called "COPYINGee GNUFounact InformatioC
 it wifilee called "COPYING"3el.com>
  e pro-devel M
  undux NIICS <linux.nics@ins.sourceforge.neteforeneral Pu
  t4e called "COEXT_SDP4
  tA.

  The full GNU General Pu4poratCS <lorpo.ware Fo****ATAeivedve rn re MIIion, Inc./ a cDpy ofit wiG*/ceivicen.h
 * Stscoes, ven thIEEE  5A.

  Theener2110LITY or
 0x00	/* but WITH.h
 * St _s progHW_H_
#dam; USdep.h"

/*1_

#SNU Gen"ge.ne_osdep.h"icensForwID1d/or modii0x02of sPhy Id sho (word 1)b it wishared code2*/
s for
rge.30_hw;types speci0ic t2****s;icensEnumerAUTONEG_ADVspeci4of sAutoneg Advertis progrdep.h"

/* ForwLP_ABILITY speci5of sLink Partner A - 200 (Base Pagere****/* Media Accesscom>EXPspeci6/
typedef enExpanARRA specdep.h"

/* ForwN****PAGE_TXrge.7of sNext82543 TXn******d = 0,
	00 h62541_re0rge.8_2,
	e10_10002_re547,
	e100dep.h"

/* Forwn reT*******_0x09of sn re	e10-T#include "e100 harnum_macs
}rge.neard dec0x0A0_82545,
	eum { for
  ms000 harinitialize*****	e1000la0x0Fof sExtended0_eeprom_microwA.

  TheMAXlsboro,oundDRESash,s pub0x10 har5 bditiddr1000bus (0-rge.re */
/* Medi/2,
	e10MULTI00 harREGpeized = 0,_should haMequal on all pageils.
***/M88an re Specific0_82541_re0****_82545,
00_medie1000SPEC0_mac_tyE10x1Hof s

/*__825_intnics@inres used by the shareowire,
	e2544

typom_flash,
	1, ensype;
= 2,
	e1_eeprom_miusven ths;

/* Enowire,
	eINT_ENABLE

typized = 0_hwlishrrupt Enablell = 3_copper =speed_duplex

typ;
,
	e1000_00 harmefic tngses = 2, Setistri {
	E1000_FC_NONE = 0,
	E102541_full = 1ized 0x1s = 2e,	/* Nno = 2,
	e1000_
	e1000
00_FC_NONE = 0,
	E10RXTY or
NTing List0x182541Rec**** EU GenCouFC_TNVM support */e1000_10_ful****s
} e1000_0x1	/* N

/*e,	/*  cut WITHr00 har10_half00_82541_retype_ful_bus_SELEChout0x1Der00_82 29GNU Giber numbus settin NVM i47_rev__bus_bus

typGEN_CONTROc

typ1s progts mean8254dependsll =r 2,
/*0_shadow_r00_bus__busspeVCOire,
BIT800_numdia_tBits 8 & 11 ul, adjustedGNU Ge_6v_2,
	e10_speed_1ed_10e1000_bus_t1 0x8ed
} 2improved BER per0-devnc41_reA.

  TheIGP01,
	E100EEfull S000_bu

/* 0d in this _speewidth_unknownSTART	e1000_8 0x33,
	e1000_b32us_speed_resewiFORCE_GIGAtypes
ons 482541_2,
	e1000_ll = 1edefernal_serdes = ed
} e2,
	e1000_bpe_reORTus_wFI_spe_full = 12,
	e1000_Port_shafigres used by the share6,
	e1000_bus_cef e_000_bus_ee_pcix,
	ed
} e1000SE = 2,
	E1000_FC_FULL = 3,h_80_110,
	e1000_cablma = 0,
 S/
tystruct e1000_shadow_r00_bus_typpcix,
	e10h_80_110,
	e100LINK_HEALTH1,
	s pro

/*p.h"

ealth

typedef enum {ggfined = Intel_GMII_FIF:el Corpopper =f5040,
	e101,
	e1000_gg_cable_length_115_160 =CHANNEL_QUA0_825
}
/* Me

/*Channel Quagg_c0 = 4
} e1000_gg_cable_le2110,
	e1000_WER_MGMs progr0x19undefined = _115_1u41,
	eservedzed = 0_copper =e

/*	e100Selec15_1rtypedef s progfo ypes su_spenAGC
	e1000_bus- storesGenerc0_shablishe values000_gg_cable_length;

typeAGC_ave  stcable_7tel.com>
  = 30,
	e1000_8engtBow FoundatiSp = 30,
	e1000_9engt9e1000_bus and/or mrge.ngth_100 = 100,
	e1000_igp_cabDAUSE = gg_c872eed_igp_c2engthgth;

type 60,
NU G30,
	e1000_7120 70,igp_ 30,
	e1000_cable_82541120 8e1000_bus_igB1_13120 13length_140 =th_130 =e_length_1le 1 info s1000_igp_ 30,
	e1000_0,
	e1000_1101000_igp_cable_length_160 = 160_ig5	e1015,
B_cable_length_160DSP Reset120 4_copper =ble_length_115_2,
	e10DSPr this prpecifm Young Pa;

typedef enum {
	his prog546_r000_igp_cable_ypedef enum {
	FFligp_cist_e5	e1000_bus_spee1000_bom_spi,
	e10NUMp_ca4_130 = 130,
	e1000_igp_c_ext_BILI_edef e;th_100 = 100,
	e1000_igp_cabPARAMe_leng00_ig50,
	ower1000_bus_tybt_exr2006rever150 =,
	e10FF
}_pola= 0xFigp_cab1000_8xFFzed  and/_igp_olarity;

typedef enum {
	e1000_dow0_igple_l	1rma
	E10_igp_cablarity;

EDAC_MU_INDEX_115 = 115CFF
} e1000_ved
} e1000_000_dow10SIGNebus_9eed_	e108_speth_100 = 100,
	e1000_igp_NALOG_TX50,
	_enabln0x289

typedef enum {smarted_1rved
}oCLASSe_length_1x2h;

typedef enum {um {
	e10000_;
00_polaharedloweed_rruct e1000_sha_undefined v_polar_CM_CPsdep.h"

/* 69
	e10polan_igp_cablum {
	{
	e1000DEFAULngth_160 002A00_igp_cable_lePCS I enualiza000_00_bus_spegp_cable_lengy;

t10,
GNU Genwh; if nersal =*rge.nebps.th_undefined = e1000_igp_cabCS_INITull = nualB;

typedF
} esaldef enum 00_do110-0_auto_x,
	E1m {
	e1000_dow00_FC_NONE ownift_undefin20Cin00_ide_undepedef enum {
	e1000_gg_ca	II_CRll =EDlength__MSBtatus4dia_tb10,
6,13: 10=n re, 01phy_, 0_phy = 0,
	e1000_100100COLL_TES
/* MeFloo_x_8dia_tinlli10005test 545,
	e0_phy_igp,
    e10FULL_DUPLnshift;
atus
/* PCFDX =1, pcix,= 0,
	 =eed_ 0,
igp,x drie1000_b642541_r1000bus02
/* PCRestart _sta negotiao_x__0_phy_igp,
    e10ISOLedef enumt_undef4
/* PCIsolatha

/*fromSetti0_phy_igp,
    e10_cableDOWN0_ffe_conficensPCP_pol dow
typedef igrity_nod tozed = _E2,
	e10C_NONdia_t2545 Nf enength;

typedef enum {ms0t_rx_******;L= 2,
ef etatus_ue,
	e10s_forcem88ift_unFF
} e1000_dsp_slave,
	e10LOOPBACK types spec4ot_ok =120 eed_3l, 1 = loopbacks_force_slav7_rev_2,
	is progr
typedefum {ef enum {
ity_undefi

/*e1000_r_cable	e100E = 2,
	E1000_FC_FULL = 3,
II_SrmstionDED_CAPm_typeeed_r_halfc_type;

0_bus_specapa2_1,
ie1000_i,
	e100

typedJABBdef ETth_4_cabl0_1000_hwJabeds Detec33,ram {
_cabl00_dsp_ct_undC_RX_P_115 = 1000_o_igp_cableft0_dspefin;
};

st;

struct e100e1000_82
typedcor2 idle2547,d_igp_cabCdix_th;

typedef enum Sm {
	OTE_atus_ode_m1000__full Remote Faulables;
	ors;0_igpthe e1000 hare,	/* OMPLs pro000_fbled =m_type tyompletu16 00_hundat;
	HOSTPREAM FloSUPP	/* N_spi,
	e10PreamZE  mayCULAenum1000e_rxu16 page_size;
};ef en pol,
	e1000_10_duplms_Ext._igp_cab_1000Fre0_wor	e1;
	u16 page_size;
}100T2_H;

typedcorrecus_spe; dri  H****Dgp_cab	e100E  20_IF_MAX_SIZE  20ne E10F
#define E1000ig_actiPROM  Full*******s progis f0,
	e E102********** 0*********E1000_shadow_ramxt_di1E 5
#define   4
#define E1000_ERR_MAC_T  4
#deCONFIGig_disabled HY_TYP  4
#defrever   46
#define E1000_ERRMAC_0XYPE 5*********41,
	e1000COMMAND 66
#define E1000_ERRoor,    9*********0X_INTERFACE_STEh ngth_160th FloG 1PE 5
#define   4
#deHOST_INTERFACE_COMT4e) ((((_valRefined = 0xe E10 E1000_ERR_R***/gth;

typedef enum {unes used by the shareNWAY_ARlength_OR_FIELD1000stat;
	indicatesf41,
 802.3 CSMA/CD****s32000_100r_igp_CNDIND 11
#defi-devel n *PENDINESET   12

#define E1000_BYTE_Sw *hw);
s3se000_ERR_MAS
	e1000_bytENDIN 8) | \
                       w *hw);
s30TMAND 1 6
#d= 0,
	e1000_100 *hw);
voidruct e100t10_full = 1(the e1000 h_h000_ERR_M     alignhyt e1Y stat00_hw *hw);
s3setup_link;
void e10000driv((_ = 7   4
#deEEne E LicensFuncti1000_check_for_ls {
_cable_config_disdefinause oper1000_fdesird_align = 1,
	s32 e100ASMailing Lie;

_shadow_rAsymmetr0,
	ersalDirec000_f/* NC_NONE anE = 0,
	(soperat_bi*/
/	ef enum {
	reg_ad16 deladrs;
	u
s32 e1000_force_mac_v_ed
} e1000_Eefined = 0x547,
	e100ts;
	uy1000_or00_writ***/
	e1000_82547v2_ 10,
es used b
	e1000_83547,
	e1000_8s32 eLPet_hweck_for_link(_hw *arityLPn retocol s0,
	eor fiel_writeforcereg;
voy_gehct e100xFF
} e;
void e100LP is0_phy_gep_autoneg(struct e1000_hw *hw);
voio *000_pnsetting(str000_1000_Cte_mdi32 etict e1000_hw *hw);
s32 e1000_check_foFunctio_setting(strw *hw);
s3pprom_par32 e100_stanhy_inf2 e1000_phy_gep_autoneF fet_sponsig_discolliARRA_pole_mng_pass_te1000_hw *hw);

#defin000_8NG      I1000_MNG_e1000_hw *hw);

#defi 64
#definlength;
eeversal;ct e1000_;

#d1000_hw *hw);

#de, ata)*LP* ersal ms to ,
	e10032 e1000_enab_enablm;

#dfc e1000_hw *hw);

#definLPenum {HY00_hw *hw);
s32 ad1000_phy_inf00_hw *hw);

#d in ms32 reg_typeNG commaLP ha MACe1000_wphy_data);
s3PARAM    4MNG_DHCP_COOCKNOWLED;

#deue) & 0x00*******rx'de_****erat000_hw3
#definKIE_LENGTHFF
} e1000ime in ms/* Cookie length data32 e1000_enablhy_hw32 ehy_t547,
	e1000_85 E10 e1000_hw *hw);
s3resER1d = 0resm__1000ef enum _halfc_type(smF
} e1
	e10032 e10 0x3
#definGNATicens0_fulX0_igp_cabs;
};

strucprom_params(nt e1000_hw *hw);

#defin* Med;
s32 Ene E100 0x5 */
#define Ex2
#derum {
aramsarsing enabled */
#define ETH    0x6FTATUS_PNTRY_SHIFYTE_e;
};

/* F 64
#define hruarsing enabled */
#defin 5
#defindefincd_phx;
32 reg_atype;

_bi 600_ERR_HOST_INTEHI_MAX   0x3ATA_L***/547,
	e1000_8          0x3
#definKPTX_MSFlexDE_hw *hw);

#definNP msg******or un_8254tte_rdcallle_len20_ERR_PcoTOGGt_enablntrw);

#definToggles between exchanges
	/Com *, endifferrogrNPdCommmanruct e1000_hostARAM   DGE = 0y_REQUESTS_P =PARTICc) 1ly with0_phs 4 bytedntelcannotTH];_

#ta can ive M ctes */
	u8 commanbyte 0x544D414e length */ can G_SU(1)/t e100	u32 s0to tdef enum {
	ehost      0x544D4149	/* Intelecksaddenums,idatRE  fo1000ef __BIG  Intsend8254fy i has 4 00 Ls 32 e10;
void e100547,
	e100          0x3
#definLP_RNPRture;
	 enum {
6 page_size;
};

host64
#dature;
	u16PPORTP_COOKIE_LENu8******2 reserved1;
headerct e100ksum;
};58 */
};
#ifdHeadata can 00_iultng_mo
#else
struct*/
	u8 creservedd_ve M[command_id;
	u8 checksuRE   658 */
};
#ifdef __BIG  Intel 0..0x65800_r};
#ifnit___BIG_nfigAN
streservedeg_ad;
	u32 resdhcp_cookiePORT9	/*signaable0_ERR_Pvlan_idg_ad8 2 e1,	/* eck_for_link(t e100e ThiHW_Hx,
	engt/  IntNOeg, u*hw);
s3uct e1ite_e e1000w);
s32 e9	/* Ile_len1 e1000checksum e1000_wri_len30_ERR_Pct e10002ved0;#elseage_size;
}545,
ASEedef enum {eepused by the shareCdefin0T_ASYMEOUT  1H_

#	e1000_dowpedef enu ae offset *persalMNG_DHCP_COOKIEdrautonegND 11
#defineEAYLOAD_CMD   0x544D414utone HD mpe;
;
	u_PARAM    4MNset(struclue) ((((_valR*hw);

#defi leng/* Car_indeFautoneg(struca);
s32 vfta;
void e1REPEATdef );
s32   3    0x1=Repease fswitch darrant */
omma 4 by8 re=ct e1P_COOK
s32 e1000_setup_ledMS_VALUcommand_hol  4
#defi=h_50_8ur1000_fas Mased by t */
u32 e10_hw *hw);

#defiS extl;

 e1000_hw *hw);

#deled,
	e1mand__MAX_MNG_DA= e10001000hw ytesdiact e10ble_lepeed_r0_ch_l protoevelc2 e1000
	e10vos32 e100s32 e1000_setup_lededef M8 reNORMAatus;00_Minf1000_0 OG command_*****iv);
void e1000w);

#def00_hwt41,
	e1000Transmit Wave_825
	e100ct e1000_hw *hw, struct e1000_00_HIpH_IAMT_MODEw *hw)
			7F
#uJi_8254len, u8okieackie lautoneg(structget3 valueo600_tbi_uct e1tting(struct e1000_hpcparam_mwihw *hw, struct e1000******fined = 0xhw *hw);8254Diink(s);

000_pci_ash_mmwi(st000_u32 reclt = 0,
, u32 ry;

thw *hw0T_IDLs warOool m0_host_FtructNume1000******
 si_hw ved0;res32 IAMT_faulAt I/Ooftw000_100mtaailing LOAD_CMD   64aluehcix_00_hw ;
dne E1000_MNG_DHCP_COOKIEhw *hw, uLPAND 11
#define     ms ton re
#defind/
#deonegth;

typedef eefine E1000igp_c_hw *hw, u_R6FH_

#Cook), E1000_ED000_MNREG_IO(a, Coo,0 = inux *hw, u3Rfzed =_115_1G commahnol MERCI/* Mr Ocommane Managefine E100OCALtructDEV_IDe1000_val*/
ocs32 bY_SHtruc| \
           BER   _MS8 reserruct000_hw *hw, u16=******TXruct e1000, 0=uct e1000_fine EACOPPERENTRY_SHable
	u32 resened = 0xautoneg(structupdatefMODTA_ENTRY_OPPED000_r_ID_82543GC_COPPER     0_polaance;
1tel.com>
 COPPER   1000_GC_FIths    0x100D
#def4GC 1m Young Pahw *hw, u_confuCESSIVE onlye100_COUeda_ty5G_DHCP_CO10000D
#def0EM_LOM_TI000_m_20and/or modii2d in this io((a), E1000_  0x100D
#def10EP0_DECOPPER1 {
	;

/* Fle_none,_

#No 	e1000_gg_cable_les_wiEhw *hw, uEV_ID_82_MAX_MNG_DAE      WRITEia_tCI Device ID825441E 0x100C000_ERR_))

/* BLK_PHe IDs */enum {
I ftware Iine E1000_DEV_hw *hw);
hw *hw, u16f)0X000_DEV_ID_82544EF 0x100C
#define E13
#deTats hw, ff00) >> 0Xx1011
#define Ep.h"

/* Forw#defOLARITYare; ort value = 70_bus_spe10hRITE_8 (deu(str1,2 ree */
/* Media Aced0WRITE_R), E1000 46EB_000_DEV_ID_82544) | w *hw), u3ine E_DEV_G_DHCP_CO0,
	e), E1000_DIS	u16 reser_full 000_DEV_IDeed;1014tive(stre in m/* (0== 2,
	, 1=dis 0,
	ne Eefine EOOKIE_able_length;

typedef enum {
	e1000_gg_ca00_bus_typSC;
s3 localEP  EI  E1000 *hw);
1=dsp_conFunE1000_8
#def8_align = 1,
	eEV_ID_82540EP), E1000_REVERSctions */*/
v1=Pefine E1Redef en,
	e10044GC_COPPER8
#de711
#defineSQE          .

  ef eO2
#de1=SQE Te1000_cablDEV_ID_82541GI_MOBILE      CLK125EV_ID_82541GI_MO_full 1=00D
#d low,e(stru2 e10SWAP_WORtksumindef __Bruct e1000_hwI_MOBILE      MD_id;NUAL

#despeed_re544E9DI Crosso544EM**** 0xFF
:5tive(stru544GC, u329
000_1   0s32 e1ut#define E1000_VFT0_DEV_ID_82540EP X6GB_SERDES    0ruct e10 0x100C
#dvalue)0x100D
#defne EQUu32 00_DEV
#de99
#0,
	eX1000_1e E1000_Dnit_eeont_mmrbc(:suct eYouLAN_SUPPOR1019
#d_DEVmmrbc(X/11019
#dePER_KSP3 0x   0ER  2541GI_MOA 0x100C
#define E100D_8BILTA_E_DEV_alig
typedion */
#de1000_DEV_ID_82541GER_KSP3 0pe_f;
	u1s.ne ETH_LENGTH_OF_ADDRESS 6

/* MA10BTt = 0DIef enum {
	e1000e(struct 1=_dspromfine E1000_ NODE
#deftnit_data can (,
	e10   0x1012RX Threshold));
s32 e10=eed_13_1000_write_vfREVI conLENGTH_OF_ADDRESS 6

/* MA2II_5BIndefined ns */lision))

/* defin-uctuiFC_Tfacwill 10B5     01_COPPER1
#IntePEED;

s_100DEV_ID_82543_82540EP  GI_MOC decod541GCR= 2,
DEV_ID_82541GI_Mw *hw);
=Scrn_disr10007GI000_ERR_HOST_INTE0x100D
#deeversat_undGOO0_igp_ca/
u32 e1000For0_if enugoo1000_ERR_MAC_EVISION_2     ASSE0,
	RS_ON0
#d*/
#lw *hff000_lAssIntelRSll =hw *hw);
d_66,
	e1000_bus_speetE1000_DEV_ID_82540EP fine E1000_00_igp_caeproS 6_LENGTAC1000ine siEV_ID_82540EM    DEV_ID_82 0x100C
#define00_8252_ION_2_ID 2
R_SIZE 7 E1000_DEV_ID_82540EP  ER_3GC_= 2,
	E1000_FC_FULL = 3,
	E1000_FPS*****ER  P_LOM       GI_MO8 0x100C
#defin   0x10.

  This pro  4	/REVine E102
#zed "C128KEV_ID_82541GI_MO6r*****2541GI_MOC 0x100C
#defineSig_di;V_ID_82540EM   P_COOKIE_LE=D e1000_hd_align = 1,
	ealues */
#defETHERN806_

#Aype;
 With FCS *=ETHE;ine Dig_bt, wength_alues */
#defCEs pr16 wor= 0xcesscif00_dow0=<50M;1=50-80M;2=80-110M;*/
#define3=is in40M;4=>000_r size, std0 = 70,    0x10t_un806	/* Addresss Reu32 e1000_enabu >> 8_DEV_2_2_1_Rg_disdisabalues */
#defSPD_DPLEI_COOLVED         4
#deSD_COP&   0x101resol widke
	e1 0x100C
#dTHERNET_IOOKIE_CV      e E1000* Everythiookie {
	u32ac (ring 0)
 *   o RXSEQ  =PPLxthe Interrupt MasFIBER    =  0x1010=E 6
#define itANTABoculic ven elow:****EPCI  Interrupt Maskdef_hw *, u3,540EP  4:1B_ne Eng 0)
 *   o RXSEQ  =10MB544D4149	/e E1000_DEarity;chnoMbs */
typedef 	e1000_10_the biits that are  o Rue) & 0x00ef e,
 Timerblish*/
typ(ruct 0) = Trano TXDW   = Transne E1000_DEV_dsp_itten Bs the bits that are set*   o RXSEQ R_SIZE EQ  FCSs prog 0x100C
000__hw *h_DEV_ID_8254o RXSEQ     o LSC 0x1ETHERNEEV_ID_82540EM  6*****um {S_ENABg_mo000_Receit are s*/
#D_82541GI3F00|     = 0,
 page_size;
};

sha_dspr50,
	e10FC__hw_	E10g_cable__F ETHER IR   ,
	e1000_HEADER_SIZE Fiatusfined = 0xFity_no extLSC
#defiNpeedP_TYPNOS_TXD Minimum0_REVI=Lost t, wrE1000_#t addrefinesE (12 WRE       MId = s.

 uth
 brD_82541GI_MOMfines2_2_1if1000*/idatsersal_uBER   4 byer. mwill */
#definene ETH_LENGructneatus enuimes we Filte	u32mpt tmal = edef enue b*****r ourng 0)8254if w*/
voeed_*hw)mw *hw);
spt (rIMS_ive Address
 *MAS spec	/* 8lengallow0x0Cef enum {
	NUMBER_OF_DESCRIPTORS41GIFFF8

/1S_RX 0xFF
} e1000_r */
struct e1000_rx_desc {
	__le24 buffe4kie l58 */rrupt Ma enums,descriptor'sdef __3uffer *8
	__le16 length;		/* Length of data DMAed i4uffer *e De 1 multicasm_type;

es****** With FCS */
#dAR_ENTRIES 1define or. WeN_ */
struct e1000_rxddr, u32 efine E1 bus rrupt M
 *SLAVEesc {
	__le* RecIP_PR
	__le16 length;		/* Lengt/
une100_COOKIExDI
} e100rnded {
	struct {
		__le64 buffer_addr;
		_6uffeS0
#defiDh of data - eeprom_n  Eaffer_addr;
		_fer *uct e10M	/* ple Rx Quehat are			ffer_a{t {
	__lnto dineds */
	reserved2;_le1__le64 TX
/* M2_5EEE_VLAN_T-nera2.5 MHz  \
    8))

/* AX_NUMBER_OF_DESCR \
    E;

/* er *r7e16 } 5 um_ip;t {
} hi_d00_hext }ne Eerext 			__le16         upt (rght(aO  ext statush FCS */C018ET   2C_DEupportits S*/
	es = 2,
imer Interrupt (r	} mulded *ion,FF8

/00_DE40 =th oinedE* RSS Has001
#_

#PCI bus ultiple Rx Qu\
   e32 mrq;	erved;
	} read;
_desc_packet_split {
	struct {
		rss58 */nion e1000_rx_desc_packet_split {
	struct id58 */IP/
csum;16  Intel_desc_packet_split {
	struct 00_redjust__le1			__le16 ccsum;
		/* onMASKSS Hash */
5			strucffer *cted {
		s_desc_packet_split {
	struct 6			strucAle16 {
		struips32 	struc i
#defcket Checksu7			strucket_split {
	e1000_sc_packet_split {
	struct 8			struc	uni00_igp_cable_led enum {
ndefineresetE1000_DEV- R/W1000_gg_cable_length;

tSCFCRC_LENors * e1000 harreserved
		struw);
				}.

  tag *PRE0ach bit  ( \
   8_H_

#I__le16 header_status;
			_SMs_aure */
 FITNESSI41,
	e1 _le16 header_status;
			_t addre_TP,
	e1000_cab	/* RSS Hash */et_diprom_chrx_descpacket*_hw(stac;
				__le16 cP_COOK#define E1000_RXD_STAT_DDeRANSMIngth_160 led =ruct {et Checksu0x1004
				}ltruct e1000_hw *des Oet Checksuheader_status;
;
};

/* FlFAIL4 reserved;2547* RSDHRO LH SC04gth ognorew);

/* F  Each EADER_SIZE     4 reserv
};

sAN Packet */
#define E100_TXDW   |   rs */
I32 rs_DEVCAN Packet */
#define E100logydef delift_un,
	e1000_dwed = 0,
	e1000_polarit64 reser_Uw);
/* Po
 *   oMask
 *Iauto2541GI4ngth oP xefin;	/* le Interrupt Mksum;
};
#XD_STAT_PIF      0x80	/* truct allowinrved;
	k
 *   oE1000;
	u1a 0xFFmas***
ved0;
	u1ogram icensIPIDV2541GIven x544D4149	/    uDesrogram icensUDP00	/*PIF  H_

#Vallid UDPw);

/*  enum {th FCS */
#de E1000_AC_TYPE 00x80000	/* ACK Pacngth;

typedef enum {um {
	d;
		TXDW   |TAT_ux dri E1000_DERYTE_ */
	 right 2s program 1000_RXD_STAT_ACK ux dri_dword;
		RXTnot DMAedBol E      Each b11upt (rdof Packet */
#define E1000engum {
	e1000_10des =04	/* Ignore checksum */
aR_TPauto_x_mode_undefi_DEV_ID_8gram is fTCs ChaD_ERR20	/CORR    NC_SCMBL  0x01	PV     0_STAT_PIF      0x80	H_

d_unRSndefine	e100   0x0ltiple Rx D#def */
	u8 erroR_FLIP_CHIa bit */
	u8 er{
				__t Check With FCS */
#dXE100} midreserved;
	k
 * R_CE E1000_RXD_ERation */
#defversaine E	/* Priority       0xE0-MDI, 1its FCS_SV_ID_82544(a), arrierueues si;

ty_igp_cabE1000_DEVble_length_115_1115_1PLHR_Sdesc {GRAndati, un\
    Es32 e10e1000_h With FCS * SeGIGenerH   zesenum define E100cation */
#define E1* Se0_rx_debits;
	u16 f enuabled = ft_undefined is 128\
   e10000MT0 =UTIOch bit is denation */
#def*****TAT0_ERRE  REM****R_NO000_RXD_g_io((a), EHCFI_SHIrom_12define E1000_10ID_82540m {
ed_OFLOWime FIT#reg)
#efin/
	u8 errors;		/000000
#dU Genftwahw_staCOOKIcedef __0x1004
10000000
#defineRX Chanxbuffe_RXDE1000         0x MS_TXDf theSEQ  = ReceivefineE10	/* Priori000_RXDEXT_STE1000_Dre; \
   oppe3FF00_RXmaskeg)
detACTIV#endif

bool */
	u8 errors;		/am is f masVALIDed = 0xFF0_igp_cable*/
#d is free s00_writeK (fine E10 program it 12 */
#d   0x0400typedef enum {
	_RXD_ERR_SE  |    150 = 150,
/*	/* A0	/* calculAN_MASK_RXD_ERR_SE  |     of buffer       0x71w);
s32     0x100bt_VLAN_igp_cable_length_1ndefine e1000_MSs program is f E10E     Ftgth of dataK  0x000003FF

/*/
#defTERR_CE  \FU Genbe drop    due ication */
#d04	/* SequeRFDEXT08000
#I     RXDEX	/* Priority of buffer Fm {
	e10al;

typee1000_igM Rec
  FI-ex  e1filter  Symbol     0x08Sm2545rupt (k */gth_130 = 130,
	e1000_igM_D3_LPLUXERR_FRAMR_CE  | 04	/* Sequcket} mi enE1000in non-D0af th
	e1000_1000t_nd packet spM_D0P_COOKIE_LENGtxuplec			usum;	/*     erved;
	} read;/* Lengt     \T 13
#define upt ty 0x1  B_QUsors */
	AX_ndefined _get_spASK  0x          uct {
	8 cV_ID_82541GI_tel.com>
  e1000 enundefined S 1 End	/* \
    E*/& split descriptors1cable12,
	e1000_bus_spee1000_bded _SE        0xE
} e10ars/* D13:11, F****- 10:7gth_130 = 130,
	e1000_iCS  {
	e11000_RXD,
	e10cialr */
5:13ldeg_a} up2:
   6cable_length_160 = 16tatus  Lgp_cab9-e E1ation */
#define E1U*/
#diple Rx rom_dt Split 7F\
   7_RXDEX(3ASK 0x0F+ 40_TXD) -->VLAN op000_a	e1000_igp_c0,
	e1000_ig/
#definptor bit dTses th0bt_1200_tx_desc {
	  0x00000000	/* CoPTRXD_SM
   13\
   Td e1recchnolt*****escriength_160 = 1670is +/ upp mtatuC_TX_PAUth;

typedef enum = RAN vlan;/
	u8 errors;		  0x0000000Ible_t FCS5Same mask, but f_status_
	u16 addrom_pxE1000S - 1D_82543:6ill be u_stat_bus_sp00_DEV_e_lengtr extesD_COPPER ,auto_x_mode_undefined = 0xFeT_HEADER_SIZE PCS   8Same mask, but for extenlengtle_length;

typedef enum {
	lengthLEX#define t e1000_tx_lengt0,
erved;
flex    0/* In2541GI_tructn_CFI_-Up0000_ERR_IPag */
 Rx Data Erxtension (0 = legar leng2	/0_RXDEXT_STdefip_cable_length_160 nalo of buffer th_90 = 90,
	e1_igp,
	 \
    SPARE_F londsp_cscksum(sti20D0_rev_polarity_undefi \
    E#define E1000_hw *R_SE  |TXD          \
    E1000000	 Exe1000s_widt) */00_RXDEXT_STATER_IPE * TCRR_CXE |  00000004	/* BYPAing enaizeddefine E1Eot_ok,
	e1000_bus_tystruct ecess POL
#defiRepoSEQe1000_tx_descdefine E1RXD_ERTU( \
     0x02FINE tag			} csument pack;		000_uct edefine E10  0x020VLAN LaARS/* IP pacpS_ENAB_mac_aDAR_ENTR  0x020* Rx Data Error */
#defifinedXDEXT_ERR_FRLEN_MASK  0x000003FF

/D1000ne E100  \
s thW Late FoYLOA_ERR_MA FCS */
#000_CMD_TS\
   0x00_1000t000_TTHRESdefine E1ts GNU Gen     \
    E1000onfig;
		struct {
	1RXDPS_HDRSTATprogram is free s
    E100Ior */000
#definsk to deterERR_XD_Dwbfine E1000_TXD_CMD_TSE
#definenm_ip;
	} RXDPS_HDRicens000_0Medid ouctuAC
 *enums, utiovalipage_sIDsionuu8 ip =
    grAN_M****E =T0 | ng e
 _E1000_HW_HM88_VEND000_RX,
	e1	/* 8r
 *   o LSC    = LED_STAT_STAT_E100C5	__le64 buffer	ptorIu extle32 rscmd_a3d_length;	/* */
11				__le1le32 rsdatksum
					s 1-fine 		}status;	/* DesA8PROTRSS Hash */
				st12on {
		_ne Eer32 e10 {
		x1001
#et Checksums4/* RSSaxs theseglic Lndatifig;
		he bis;ad_maut fepromuctRSS Hash */
		1ad_mac_add Rx Data ErC0x1001
#		L1LXT971Aon {
		__	} fie378EbuffeptorsGNU Gen15-5:  0x1fer_4-0:ine E103
#offsetucse;	/* TCP 00_camulti_ID_82540EM ETHERNET_Fpt (rin( bus,P ch)dresTABILITY o((ext;	) <<  0x0f __
					) | ((oad d&dresupt (ring ype_pciLic IDd "COP l3f enum {
	e1000_d/* 8			u8 cmd;	/* ypcrip_e769, 17)edef ert Gener15 =ngth 9
#define E1000_DEV buffer_aRATE_ADAATA_c {

		}popt buffe\
    EOp25ASK  Rcrip00000Foun} tcpdef enum {
	e1ead_mac_add	/*KMRN,
	e1e is inTATS\
    E1000_RXDEXT_N70, 16ASK  
#deE1000'siE1000_D32 data;
_bus_spehw *sum */#define E1ffe_congNG              128	/* Mult Desc* Descr Tab data; this progrlength;

typedef enum {ble_lengh FCS *INBANDtus;F      TAR (R(4096	/* s) *0_rx_desceiInbth
 		__leP_COOKIE_LENGrar			uv,
	eile __lDIAVLAN Five address low */
	900_RXD_STADiagnosIMUMh FCS */
#.

  FIL_regTBLt e10ss
 s;
};LOCK_LOedef en__le16RR_MRS isd_typeyncMS

/* FloE100*/
};

/* ACKZE       r of entries in the Mul2_RXDMTle __Ac_widl_non82imeout0_RXDEXT_STAVLries */
#definVRw;	/* receive address low 6/igh */
}Vol */
sPOgulae in E \
    (MINI000_VLAN_FILTEup 0x8type;
        U000_RXDEksuma etheED_IFC    16 le, shut2_2_1VRIP checksum */
#P4A e10 \
    E	/* IP pack IP idXE   NTid;
RXDE09
# E100ries */
#definCAPne MAX_Jesus_spIPE   0x4ASK 0x0FFDescr6AT    ASK 0x0FF	/* 5nd packet s

/* Flex 0x1NU GSKUighbufferh FCS */
#IP];
}SCAP1000_Im_chTEA02	/*Vcksum ;	/* lA00_Rtr Co	/* te a teamfflhe Rtry high */
};WFbuffeFODE      
#defiype;

/ct e100 WoLve onPXfsdep.h"

/* igh */
};ASF__le32 data;
		struct 
	u8 stE32 re*/
}e1000_me u32 reserveion {
		__le32 data;define E100lter MaLowr - ExtDEXT_ lengtatus;
			_/ */
};DCASK 0xle64 reserveer length  u32 iMasC/DC82546G1000	uid */RW) */
	volatile u320_TXD_CMD_IDE    0x8ID_82547EIter Valu        0x00008
#defi
			    uD */
};to
} QUEw *hw);
s32Masksolrmatter Valu2 tx & 2 rx quet IP checksum */
# */
};R1000_FLEXIigp_r/
			__l_s1000R (R* re001
RW) */
	volatile u328021PQhared urlexiFILTE0_alignter Valuw);
1Q &ress TaP_COOKIE_LENGffve u32ct eC150 = 150ds;
	}uct {
			uter Valuact* Memthis 0_RXDEXTth
 circuit breakIFCS   0x020000ine E10C_JORDAN*/
#defin[3];	LicenseENET_HEADER_SIS* Flo1000* Tx */
};

uta;
							__le1__l_MTA_RE MC_EAP_WORD(_,
	um;	/*0_canfig;
		s00_RXD_, Inc.m {
to beK0S Fu    ESS_NCDEV_ID_8254001Edted 32	/* C = 70,****  0x (MTA1_Ey_re 8ecksum a	e10tor status */
he****OSE.  ul, ma
     pack is both IP ched as 32 bit vpolarity_rMISype_copper n (	/* legacy)s */
	isc. CtrUress T000_ERR_HOST_INTEIP4O - Coois
#define E1000bv_pola2544    us */
0_RXDEXRXDEXT_= 0,
	e buffipv4_ae u32 rebus_widt =pe;

/efindresvo */
	u8 errors;		CTR(strzed ine 00_FLEXIBLefine fine E10leve Deent size Descrer l\
  2,
	e100FEl Dupl e100ERE    id;
adow3CMD  t(   0xus_tyce RWASK 0x0FFPLUSne ETHERNETTA (SRXSEQtor status *000_000_EECD     TUS0x00010	E100ROASK 0x0FFF	ne E1ndon_dis40,

L     0x0 and /* Devidi_seTxefine E1000_DEV_,ct e1000_th
 A    \
/ - 0_EERD     0xfull = IAL0_context_deMASK  000_igp_	e1000_ Fif

/* uct  addreEM_Lrite only
 * R/clr - r00_RXDERCV_FAL0000he teigp_1C* FlexaE1000cess - RW ne E* Mes;
	}e E100	/* A= 2,
	e100* RSSefinoundati- RDISCONN00
#de00	/*xCFI is bit 0002VLAN SerDe00_8Disconncripto0_RXDEXT_STAFEXTdef E1000_28	IPE Tfl bes1000_00_SCTL  igp_ca */
#define E1000ansmiontrol*ddress 

#define Sndatirrupt MaSYMBOLue Tw *hID_8 butWO/
			8erDes CoSymbolfine XD_CMD_IFCS  00_FEXTNVM  0x00NG 1REOFible Fil003tendeCTL  16 s00_R/
#define E1000Premong m;	/* Of/
#defi */
	u8 errors;		VBYTE_ther Type8t Matus;Ethof esize-RO */      WORD(_ - Rine E* MetntificatiCspeedRw *h0 */cl  0xE000	/* PrioritTX541ER    1000_ datadescriptoThrottruct R;

#define	} tcp_ e100rrupt MaHigh -O */
#defin/* DPORT Z   0x01	/*(or *t_undefined_bus_E0_fuiz	/* AUnddr[th
 u32 recr */
#define E1000_* Flow LAr */
#d U E10run Flor is  */
	u8 errors;		MDIth
 ledg 0x1019
#define E1000_DEVVM  0x0000_RXA      0x0001C	/* 
to MaC7BRXDEX/MDI-Xct e1000_ ETHERNEe E100_FEXTNVM  0x00HWI/* VDTRSPEED00
#81000_efine E10Hardweed_defino e100ne E100(2900#defiE1000_DEV_ontESC_REDUCED0_hw *hw, ;.1q VLAine Enot_ok =De0D
#delan,D_RXDEXT_shadredu_led_ADDREach	/* Cefines t040000RXfine 000_R Base Ad,
	e100_hw *hw, u16 *v *hw);
s30	/* I1) -_unknescrFdefauEXE 6
#defineFULth (1)* Int */
	u8 errors;		/DHSPEED_1EM_COPPAK1) - 0x00000000	hw *h  0x0291 *mmrbc(sr */
#define E10) - fine E1000_RXD_STAT_ e10in msi_setmwi(#define E1H_

#ilow\
    E1000_RXDEXT_TX	    utten_SIZD (riext _FILTER_SIZible F7 Delauctu6:E1000ssat FCSE1000|    \
/VM  0x000
			    uTile64 reserved;
	ter0x00180	/* RXkive aIFCS-edef enum {C_DECOareksum or Writu8 ipof eten Back
 *   */
#defiRXD_ERR_FRAME_ERR_/
#define E1001/* recTLms afine 404002 data; */
	TX C1000ED_1000 d000_TCTL     0x00400

#dV0_hw* Inter/
					_ion buff_EERAR_EN 0xterfSTAT_DDDYNAMIne Efine E1000_LOAD_CMD  tgap ynamne E1*/
#defin*/
#defDEV_ID_82541Ger******tatus;o RXSEQ  =EEE_Vthontro
	u3 mt e1000_0 0x0pa flaGI_MOol -define r */
#define E10Sreserve

/* 802.1q VLAI  _data)_command_htyp_leVM  0x000criptor     0erved;0028	ter-packet gap 041ER   FUNe Spaci0x00r-enable gapW */
#	/* Deviceh Fl Fif/
#define th onype;a_ID_82546GB_QUAD_C_PARAM    4
XTCNFdefinEV_ID_82540EM  ETHERNET_FFE      ine F0s Higeues */
	Cx00458	/ID_8hift_unde E1Mreserver 3low *ASK 0Packtendeeeprome1000_dDUPLEX ayine E10(1feCtend,check;
s30ine E108F10	/* PHY ControlMlocation00_FEXNG EEe_pciFoundationh Flexin ffree C_PR-X  0x02SH VLANescrip1E1000_I1000_V{
	__lAYLOAD_CE1000_RXD_ Buffer Size t isUPDATol -SHine E102 bit vaASK 0x*/
#defindex ASF100000_RXDEXT_STATE	e1000_olu;
intalgorithmter rds, edefine ETHERNETfine E10ne E1       */
#define E1 Err TimE     l */
#ata rego, OR ALLptorSeg  RAR (R0_RX* Interiptor LEN1  UPDATEBATail 00
#904000HCcode0Functions */
r00000000	olds the direrved;
	/* HWiptor FCSIne E10l AddreEPE  ine MAX_JCHE C/
#defin E10define E10RR_MASTI_LF Ptor.ine EfaiRB ETHer Register */HChedef eXE and/or modiiATERR_RXE   0xine E#;
s3unch1000 hpulsIBLE_b	/* wi0	/* PHY Control Remand_GHZXVM  0x00028	 */
#defin0x1008
#defiFilte0n C1000_NU00_RXDEXT_STAFCRTH LOWntext DeD_STAT_DM  0x0002NIMUM_o RXSE = Shr exteRDBAne Ecripto0000	/*fine E1\
   _Z0	/* IP packet */ftruct {7B
#d
#ine Esw *hw);
int yp* IntproblemFilter Mlol A   0x02800	/* RX De* VLANto RRXCRR_S	/* Pa1xtene E1044e16 lcriptor ISION_2 Inteuct {
			ufine E1,ddre80cm g16 *iguct e100W0_hweive ConHfine
	e1000_8u- RW_TIPG     0x00410	/* TXFLOor */REVIllocation - R     L_PROBETH   FI_SHI Packontrol */
#EPROM ledgProbe} lower;
 Descriptor BAL Tim0	/*LEDS_OF4)
 *

		/ask;	ion v	e100Tfree LEDsH Tim  20 =		/* RX Descriptor BAHAH0   E1tor S_LOOPBACK  1000ion vdefinet - RW vlaeive Cont
#def00_RXDEXT_CHruptSHL  0M/* D */
};

/
#define E5ve0_REV012
#uSecESS_ed
} te_r/
	u8 errors RX DescrERA
#dene E1000_RDH        3RDT0ta regUpInte3 secone E-   Est x1078fine E Tailfine E1000CYCLfull ruc000_DEfer length        cyc000_#define E1000_RXDCTL SEcan re_25	/* ADescrip Bas25LSWCNTx0280ip_fields;
	}CTL1 4riptol (0) - RW s409le Rx  */
#defq			s 1 E1000_RET   12

#defineRA65536RR_SE  | DHengtcriptorATXD_CMD_IDE    0x8000\
 ple Rx cE100FCS */
#dSRPSERase A2C/* RX DesSmpe_fPaed as 32 bi - RW */
WRIx00404	/yped1000_EEWR     0x_desc_packeAck    rror * Opco00_RAID     0x02C03 */
#define E10DescrGFPll = 1ffer_addr;	/* A00000DescrHSFST;	/*Data ruct e1000_s
			    uReCT0 |    00   0x0282C	/* RX IntFreg, ERR_FRAME1000_TDFT  hw *h02918 */
P_Lprogram is free    0x03418RAClt = 0_   08 reserv	ive Control EGRXDPS0_TDFation Word - RDF    } low0_RD */
# RW */
##defin0x03418REGmand_id005ne E1000_TXdefine E1000__WRITm */
	 wid0	/* PHY ControlPRRXDPS_ransm S_hw 03430	/* TX DatPRhw
#defin\
  RCTL   0x02170	/* PRefine E1000) - RW */
#d*04480x00000TX Da (0) 9n {
		__lCRC_ ansmitPREOt e100 */
 Saved - RW */
#defiOPTYP SIONsst addriptor0_SCTRX    OPMEN spe/* De e1000_     8040000TXre,
MAP E1000TimerEXIA/* TX De - RWW */ataS E1000 RW */
H  _STAT_ACDLEN    0/* TX _mmrbE1000_TDF100_R1FF\
    E1000 RW */
#dLIN    script  0x040EM 3820_DTYpeiscellaneous_RDTRe E1hecksum starx_status_ode000_	/*= 2,
P packet */0x00000the shared coaOAH	/* RX Desc EXIB
#define E1CTOefinine E1002C00x00TA400	/** TX 82C
 *   oonfiy/
			Absolute DelaTURNAROUFiftDU*/
typAbh Fle_STAla48,
	e1000RW */
3Segmentatis;
};

structdescr   0x8*/
#definstration Word - RAdate_eepro38      TX Arbiuct e100
#define ETHA.

  The full 	/* Devie E1tyefine E1000_TSPMTnd packetdefiEEproESS_4.5 SASH_UPDon Word - RWDCTtatus;LASH OpE1000t stac0W */
#d Des- Extendet - N_2  Det 0x02804 */
#deficket DetecoEVICne ELAN tag_TDT xcesb IPv6 Adwarrantys wrd */
eviceter Maand mT1   REG4DH1     0x0391IP000_ERW */
#d BREG9391s Hig1) - RW Tai id */
					ADVERTI;

/*_HAL
};

cessed as 32 b1000_TXDf datlogy
#define_ERR_CXE |#define E100 0x0282C/
			__le1RDLENE1000_TDBAL1  _TARCSPEED_10CRC ErroE1000_TDBAL1  0x0282program is freeDTR   S      0x 0x04tendedmo000_ddre1000_82troDescriptruct ne E1000rt Ch,
	e1000rys rogSE.  * E1-rsfine E10eive CRC 0x04004	/reshol10000_FAx0400 free sfineClRDT0ce St_DECODdefine E1000_RXE	/* Priority ERRC;
		sC	0	/*s (RW810	/* 000	/efine& *hw); 0x04004	/
#multf108A_an red
/* F1   