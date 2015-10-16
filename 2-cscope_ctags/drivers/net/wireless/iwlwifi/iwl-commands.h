/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2005 - 2009 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2009 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
 * Please use this file (iwl-commands.h) only for uCode API definitions.
 * Please use iwl-4965-hw.h for hardware-related definitions.
 * Please use iwl-dev.h for driver implementation definitions.
 */

#ifndef __iwl_commands_h__
#define __iwl_commands_h__

struct iwl_priv;

/* uCode version contains 4 values: Major/Minor/API/Serial */
#define IWL_UCODE_MAJOR(ver)	(((ver) & 0xFF000000) >> 24)
#define IWL_UCODE_MINOR(ver)	(((ver) & 0x00FF0000) >> 16)
#define IWL_UCODE_API(ver)	(((ver) & 0x0000FF00) >> 8)
#define IWL_UCODE_SERIAL(ver)	((ver) & 0x000000FF)


/* Tx rates */
#define IWL_CCK_RATES	4
#define IWL_OFDM_RATES	8
#define IWL_MAX_RATES	(IWL_CCK_RATES + IWL_OFDM_RATES)

enum {
	REPLY_ALIVE = 0x1,
	REPLY_ERROR = 0x2,

	/* RXON and QOS commands */
	REPLY_RXON = 0x10,
	REPLY_RXON_ASSOC = 0x11,
	REPLY_QOS_PARAM = 0x13,
	REPLY_RXON_TIMING = 0x14,

	/* Multi-Station support */
	REPLY_ADD_STA = 0x18,
	REPLY_REMOVE_STA = 0x19,	/* not used */
	REPLY_REMOVE_ALL_STA = 0x1a,	/* not used */

	/* Security */
	REPLY_WEPKEY = 0x20,

	/* RX, TX, LEDs */
	REPLY_3945_RX = 0x1b,           /* 3945 only */
	REPLY_TX = 0x1c,
	REPLY_RATE_SCALE = 0x47,	/* 3945 only */
	REPLY_LEDS_CMD = 0x48,
	REPLY_TX_LINK_QUALITY_CMD = 0x4e, /* 4965 only */

	/* WiMAX coexistence */
	COEX_PRIORITY_TABLE_CMD = 0x5a,	/*5000 only */
	COEX_MEDIUM_NOTIFICATION = 0x5b,
	COEX_EVENT_CMD = 0x5c,

	/* Calibration */
	CALIBRATION_CFG_CMD = 0x65,
	CALIBRATION_RES_NOTIFICATION = 0x66,
	CALIBRATION_COMPLETE_NOTIFICATION = 0x67,

	/* 802.11h related */
	RADAR_NOTIFICATION = 0x70,	/* not used */
	REPLY_QUIET_CMD = 0x71,		/* not used */
	REPLY_CHANNEL_SWITCH = 0x72,
	CHANNEL_SWITCH_NOTIFICATION = 0x73,
	REPLY_SPECTRUM_MEASUREMENT_CMD = 0x74,
	SPECTRUM_MEASURE_NOTIFICATION = 0x75,

	/* Power Management */
	POWER_TABLE_CMD = 0x77,
	PM_SLEEP_NOTIFICATION = 0x7A,
	PM_DEBUG_STATISTIC_NOTIFIC = 0x7B,

	/* Scan commands and notifications */
	REPLY_SCAN_CMD = 0x80,
	REPLY_SCAN_ABORT_CMD = 0x81,
	SCAN_START_NOTIFICATION = 0x82,
	SCAN_RESULTS_NOTIFICATION = 0x83,
	SCAN_COMPLETE_NOTIFICATION = 0x84,

	/* IBSS/AP commands */
	BEACON_NOTIFICATION = 0x90,
	REPLY_TX_BEACON = 0x91,
	WHO_IS_AWAKE_NOTIFICATION = 0x94,	/* not used */

	/* Miscellaneous commands */
	REPLY_TX_POWER_DBM_CMD = 0x95,
	QUIET_NOTIFICATION = 0x96,		/* not used */
	REPLY_TX_PWR_TABLE_CMD = 0x97,
	REPLY_TX_POWER_DBM_CMD_V1 = 0x98,	/* old version of API */
	TX_ANT_CONFIGURATION_CMD = 0x98,	/* not used */
	MEASURE_ABORT_NOTIFICATION = 0x99,	/* not used */

	/* Bluetooth device coexistence config command */
	REPLY_BT_CONFIG = 0x9b,

	/* Statistics */
	REPLY_STATISTICS_CMD = 0x9c,
	STATISTICS_NOTIFICATION = 0x9d,

	/* RF-KILL commands and notifications */
	REPLY_CARD_STATE_CMD = 0xa0,
	CARD_STATE_NOTIFICATION = 0xa1,

	/* Missed beacons notification */
	MISSED_BEACONS_NOTIFICATION = 0xa2,

	REPLY_CT_KILL_CONFIG_CMD = 0xa4,
	SENSITIVITY_CMD = 0xa8,
	REPLY_PHY_CALIBRATION_CMD = 0xb0,
	REPLY_RX_PHY_CMD = 0xc0,
	REPLY_RX_MPDU_CMD = 0xc1,
	REPLY_RX = 0xc3,
	REPLY_COMPRESSED_BA = 0xc5,
	REPLY_MAX = 0xff
};

/******************************************************************************
 * (0)
 * Commonly used structures and definitions:
 * Command header, rate_n_flags, txpower
 *
 *****************************************************************************/

/* iwl_cmd_header flags value */
#define IWL_CMD_FAILED_MSK 0x40

#define SEQ_TO_QUEUE(s)	(((s) >> 8) & 0x1f)
#define QUEUE_TO_SEQ(q)	(((q) & 0x1f) << 8)
#define SEQ_TO_INDEX(s)	((s) & 0xff)
#define INDEX_TO_SEQ(i)	((i) & 0xff)
#define SEQ_HUGE_FRAME	cpu_to_le16(0x4000)
#define SEQ_RX_FRAME	cpu_to_le16(0x8000)

/**
 * struct iwl_cmd_header
 *
 * This header format appears in the beginning of each command sent from the
 * driver, and each response/notification received from uCode.
 */
struct iwl_cmd_header {
	u8 cmd;		/* Command ID:  REPLY_RXON, etc. */
	u8 flags;	/* 0:5 reserved, 6 abort, 7 internal */
	/*
	 * The driver sets up the sequence number to values of its choosing.
	 * uCode does not use this value, but passes it back to the driver
	 * when sending the response to each driver-originated command, so
	 * the driver can match the response to the command.  Since the values
	 * don't get used by uCode, the driver may set up an arbitrary format.
	 *
	 * There is one exception:  uCode sets bit 15 when it originates
	 * the response/notification, i.e. when the response/notification
	 * is not a direct response to a command sent by the driver.  For
	 * example, uCode issues REPLY_3945_RX when it sends a received frame
	 * to the driver; it is not a direct response to any driver command.
	 *
	 * The Linux driver uses the following format:
	 *
	 *  0:7		tfd index - position within TX queue
	 *  8:12	TX queue id
	 *  13		reserved
	 *  14		huge - driver sets this to indicate command is in the
	 *  		'huge' storage at the end of the command buffers
	 *  15		unsolicited RX or uCode-originated notification
	 */
	__le16 sequence;

	/* command or response/notification data follows immediately */
	u8 data[0];
} __attribute__ ((packed));


/**
 * struct iwl3945_tx_power
 *
 * Used in REPLY_TX_PWR_TABLE_CMD, REPLY_SCAN_CMD, REPLY_CHANNEL_SWITCH
 *
 * Each entry contains two values:
 * 1)  DSP gain (or sometimes called DSP attenuation).  This is a fine-grained
 *     linear value that multiplies the output of the digital signal processor,
 *     before being sent to the analog radio.
 * 2)  Radio gain.  This sets the analog gain of the radio Tx path.
 *     It is a coarser setting, and behaves in a logarithmic (dB) fashion.
 *
 * Driver obtains values from struct iwl3945_tx_power power_gain_table[][].
 */
struct iwl3945_tx_power {
	u8 tx_gain;		/* gain for analog radio */
	u8 dsp_atten;		/* gain for DSP */
} __attribute__ ((packed));

/**
 * struct iwl3945_power_per_rate
 *
 * Used in REPLY_TX_PWR_TABLE_CMD, REPLY_CHANNEL_SWITCH
 */
struct iwl3945_power_per_rate {
	u8 rate;		/* plcp */
	struct iwl3945_tx_power tpc;
	u8 reserved;
} __attribute__ ((packed));

/**
 * iwlagn rate_n_flags bit fields
 *
 * rate_n_flags format is used in following iwlagn commands:
 *  REPLY_RX (response only)
 *  REPLY_RX_MPDU (response only)
 *  REPLY_TX (both command and response)
 *  REPLY_TX_LINK_QUALITY_CMD
 *
 * High-throughput (HT) rate format for bits 7:0 (bit 8 must be "1"):
 *  2-0:  0)   6 Mbps
 *        1)  12 Mbps
 *        2)  18 Mbps
 *        3)  24 Mbps
 *        4)  36 Mbps
 *        5)  48 Mbps
 *        6)  54 Mbps
 *        7)  60 Mbps
 *
 *  4-3:  0)  Single stream (SISO)
 *        1)  Dual stream (MIMO)
 *        2)  Triple stream (MIMO)
 *
 *    5:  Value of 0x20 in bits 7:0 indicates 6 Mbps HT40 duplicate data
 *
 * Legacy OFDM rate format for bits 7:0 (bit 8 must be "0", bit 9 "0"):
 *  3-0:  0xD)   6 Mbps
 *        0xF)   9 Mbps
 *        0x5)  12 Mbps
 *        0x7)  18 Mbps
 *        0x9)  24 Mbps
 *        0xB)  36 Mbps
 *        0x1)  48 Mbps
 *        0x3)  54 Mbps
 *
 * Legacy CCK rate format for bits 7:0 (bit 8 must be "0", bit 9 "1"):
 *  6-0:   10)  1 Mbps
 *         20)  2 Mbps
 *         55)  5.5 Mbps
 *        110)  11 Mbps
 */
#define RATE_MCS_CODE_MSK 0x7
#define RATE_MCS_SPATIAL_POS 3
#define RATE_MCS_SPATIAL_MSK 0x18
#define RATE_MCS_HT_DUP_POS 5
#define RATE_MCS_HT_DUP_MSK 0x20

/* Bit 8: (1) HT format, (0) legacy format in bits 7:0 */
#define RATE_MCS_FLAGS_POS 8
#define RATE_MCS_HT_POS 8
#define RATE_MCS_HT_MSK 0x100

/* Bit 9: (1) CCK, (0) OFDM.  HT (bit 8) must be "0" for this bit to be valid */
#define RATE_MCS_CCK_POS 9
#define RATE_MCS_CCK_MSK 0x200

/* Bit 10: (1) Use Green Field preamble */
#define RATE_MCS_GF_POS 10
#define RATE_MCS_GF_MSK 0x400

/* Bit 11: (1) Use 40Mhz HT40 chnl width, (0) use 20 MHz legacy chnl width */
#define RATE_MCS_HT40_POS 11
#define RATE_MCS_HT40_MSK 0x800

/* Bit 12: (1) Duplicate data on both 20MHz chnls. HT40 (bit 11) must be set. */
#define RATE_MCS_DUP_POS 12
#define RATE_MCS_DUP_MSK 0x1000

/* Bit 13: (1) Short guard interval (0.4 usec), (0) normal GI (0.8 usec) */
#define RATE_MCS_SGI_POS 13
#define RATE_MCS_SGI_MSK 0x2000

/**
 * rate_n_flags Tx antenna masks
 * 4965 has 2 transmitters
 * 5100 has 1 transmitter B
 * 5150 has 1 transmitter A
 * 5300 has 3 transmitters
 * 5350 has 3 transmitters
 * bit14:16
 */
#define RATE_MCS_ANT_POS	14
#define RATE_MCS_ANT_A_MSK	0x04000
#define RATE_MCS_ANT_B_MSK	0x08000
#define RATE_MCS_ANT_C_MSK	0x10000
#define RATE_MCS_ANT_AB_MSK	(RATE_MCS_ANT_A_MSK | RATE_MCS_ANT_B_MSK)
#define RATE_MCS_ANT_ABC_MSK	(RATE_MCS_ANT_AB_MSK | RATE_MCS_ANT_C_MSK)
#define RATE_ANT_NUM 3

#define POWER_TABLE_NUM_ENTRIES			33
#define POWER_TABLE_NUM_HT_OFDM_ENTRIES		32
#define POWER_TABLE_CCK_ENTRY			32

/**
 * union iwl4965_tx_power_dual_stream
 *
 * Host format used for REPLY_TX_PWR_TABLE_CMD, REPLY_CHANNEL_SWITCH
 * Use __le32 version (struct tx_power_dual_stream) when building command.
 *
 * Driver provides radio gain and DSP attenuation settings to device in pairs,
 * one value for each transmitter chain.  The first value is for transmitter A,
 * second for transmitter B.
 *
 * For SISO bit rates, both values in a pair should be identical.
 * For MIMO rates, one value may be different from the other,
 * in order to balance the Tx output between the two transmitters.
 *
 * See more details in doc for TXPOWER in iwl-4965-hw.h.
 */
union iwl4965_tx_power_dual_stream {
	struct {
		u8 radio_tx_gain[2];
		u8 dsp_predis_atten[2];
	} s;
	u32 dw;
};

/**
 * struct tx_power_dual_stream
 *
 * Table entries in REPLY_TX_PWR_TABLE_CMD, REPLY_CHANNEL_SWITCH
 *
 * Same format as iwl_tx_power_dual_stream, but __le32
 */
struct tx_power_dual_stream {
	__le32 dw;
} __attribute__ ((packed));

/**
 * struct iwl4965_tx_power_db
 *
 * Entire table within REPLY_TX_PWR_TABLE_CMD, REPLY_CHANNEL_SWITCH
 */
struct iwl4965_tx_power_db {
	struct tx_power_dual_stream power_tbl[POWER_TABLE_NUM_ENTRIES];
} __attribute__ ((packed));

/**
 * Command REPLY_TX_POWER_DBM_CMD = 0x98
 * struct iwl5000_tx_power_dbm_cmd
 */
#define IWL50_TX_POWER_AUTO 0x7f
#define IWL50_TX_POWER_NO_CLOSED (0x1 << 6)

struct iwl5000_tx_power_dbm_cmd {
	s8 global_lmt; /*in half-dBm (e.g. 30 = 15 dBm) */
	u8 flags;
	s8 srv_chan_lmt; /*in half-dBm (e.g. 30 = 15 dBm) */
	u8 reserved;
} __attribute__ ((packed));

/******************************************************************************
 * (0a)
 * Alive and Error Commands & Responses:
 *
 *****************************************************************************/

#define UCODE_VALID_OK	cpu_to_le32(0x1)
#define INITIALIZE_SUBTYPE    (9)

/*
 * ("Initialize") REPLY_ALIVE = 0x1 (response only, not a command)
 *
 * uCode issues this "initialize alive" notification once the initialization
 * uCode image has completed its work, and is ready to load the runtime image.
 * This is the *first* "alive" notification that the driver will receive after
 * rebooting uCode; the "initialize" alive is indicated by subtype field == 9.
 *
 * See comments documenting "BSM" (bootstrap state machine).
 *
 * For 4965, this notification contains important calibration data for
 * calculating txpower settings:
 *
 * 1)  Power supply voltage indication.  The voltage sensor outputs higher
 *     values for lower voltage, and vice verse.
 *
 * 2)  Temperature measurement parameters, for each of two channel widths
 *     (20 MHz and 40 MHz) supported by the radios.  Temperature sensing
 *     is done via one of the receiver chains, and channel width influences
 *     the results.
 *
 * 3)  Tx gain compensation to balance 4965's 2 Tx chains for MIMO operation,
 *     for each of 5 frequency ranges.
 */
struct iwl_init_alive_resp {
	u8 ucode_minor;
	u8 ucode_major;
	__le16 reserved1;
	u8 sw_rev[8];
	u8 ver_type;
	u8 ver_subtype;		/* "9" for initialize alive */
	__le16 reserved2;
	__le32 log_event_table_ptr;
	__le32 error_event_table_ptr;
	__le32 timestamp;
	__le32 is_valid;

	/* calibration values from "initialize" uCode */
	__le32 voltage;		/* signed, higher value is lower voltage */
	__le32 therm_r1[2];	/* signed, 1st for normal, 2nd for HT40 */
	__le32 therm_r2[2];	/* signed */
	__le32 therm_r3[2];	/* signed */
	__le32 therm_r4[2];	/* signed */
	__le32 tx_atten[5][2];	/* signed MIMO gain comp, 5 freq groups,
				 * 2 Tx chains */
} __attribute__ ((packed));


/**
 * REPLY_ALIVE = 0x1 (response only, not a command)
 *
 * uCode issues this "alive" notification once the runtime image is ready
 * to receive commands from the driver.  This is the *second* "alive"
 * notification that the driver will receive after rebooting uCode;
 * this "alive" is indicated by subtype field != 9.
 *
 * See comments documenting "BSM" (bootstrap state machine).
 *
 * This response includes two pointers to structures within the device's
 * data SRAM (access via HBUS_TARG_MEM_* regs) that are useful for debugging:
 *
 * 1)  log_event_table_ptr indicates base of the event log.  This traces
 *     a 256-entry history of uCode execution within a circular buffer.
 *     Its header format is:
 *
 *	__le32 log_size;     log capacity (in number of entries)
 *	__le32 type;         (1) timestamp with each entry, (0) no timestamp
 *	__le32 wraps;        # times uCode has wrapped to top of circular buffer
 *      __le32 write_index;  next circular buffer entry that uCode would fill
 *
 *     The header is followed by the circular buffer of log entries.  Entries
 *     with timestamps have the following format:
 *
 *	__le32 event_id;     range 0 - 1500
 *	__le32 timestamp;    low 32 bits of TSF (of network, if associated)
 *	__le32 data;         event_id-specific data value
 *
 *     Entries without timestamps contain only event_id and data.
 *
 * 2)  error_event_table_ptr indicates base of the error log.  This contains
 *     information about any uCode error that occurs.  For 4965, the format
 *     of the error log is:
 *
 *	__le32 valid;        (nonzero) valid, (0) log is empty
 *	__le32 error_id;     type of error
 *	__le32 pc;           program counter
 *	__le32 blink1;       branch link
 *	__le32 blink2;       branch link
 *	__le32 ilink1;       interrupt link
 *	__le32 ilink2;       interrupt link
 *	__le32 data1;        error-specific data
 *	__le32 data2;        error-specific data
 *	__le32 line;         source code line of error
 *	__le32 bcon_time;    beacon timer
 *	__le32 tsf_low;      network timestamp function timer
 *	__le32 tsf_hi;       network timestamp function timer
 *
 * The Linux driver can print both logs to the system log when a uCode error
 * occurs.
 */
struct iwl_alive_resp {
	u8 ucode_minor;
	u8 ucode_major;
	__le16 reserved1;
	u8 sw_rev[8];
	u8 ver_type;
	u8 ver_subtype;			/* not "9" for runtime alive */
	__le16 reserved2;
	__le32 log_event_table_ptr;	/* SRAM address for event log */
	__le32 error_event_table_ptr;	/* SRAM address for error log */
	__le32 timestamp;
	__le32 is_valid;
} __attribute__ ((packed));

/*
 * REPLY_ERROR = 0x2 (response only, not a command)
 */
struct iwl_error_resp {
	__le32 error_type;
	u8 cmd_id;
	u8 reserved1;
	__le16 bad_cmd_seq_num;
	__le32 error_info;
	__le64 timestamp;
} __attribute__ ((packed));

/******************************************************************************
 * (1)
 * RXON Commands & Responses:
 *
 *****************************************************************************/

/*
 * Rx config defines & structure
 */
/* rx_config device types  */
enum {
	RXON_DEV_TYPE_AP = 1,
	RXON_DEV_TYPE_ESS = 3,
	RXON_DEV_TYPE_IBSS = 4,
	RXON_DEV_TYPE_SNIFFER = 6,
};


#define RXON_RX_CHAIN_DRIVER_FORCE_MSK		cpu_to_le16(0x1 << 0)
#define RXON_RX_CHAIN_DRIVER_FORCE_POS		(0)
#define RXON_RX_CHAIN_VALID_MSK			cpu_to_le16(0x7 << 1)
#define RXON_RX_CHAIN_VALID_POS			(1)
#define RXON_RX_CHAIN_FORCE_SEL_MSK		cpu_to_le16(0x7 << 4)
#define RXON_RX_CHAIN_FORCE_SEL_POS		(4)
#define RXON_RX_CHAIN_FORCE_MIMO_SEL_MSK	cpu_to_le16(0x7 << 7)
#define RXON_RX_CHAIN_FORCE_MIMO_SEL_POS	(7)
#define RXON_RX_CHAIN_CNT_MSK			cpu_to_le16(0x3 << 10)
#define RXON_RX_CHAIN_CNT_POS			(10)
#define RXON_RX_CHAIN_MIMO_CNT_MSK		cpu_to_le16(0x3 << 12)
#define RXON_RX_CHAIN_MIMO_CNT_POS		(12)
#define RXON_RX_CHAIN_MIMO_FORCE_MSK		cpu_to_le16(0x1 << 14)
#define RXON_RX_CHAIN_MIMO_FORCE_POS		(14)

/* rx_config flags */
/* band & modulation selection */
#define RXON_FLG_BAND_24G_MSK           cpu_to_le32(1 << 0)
#define RXON_FLG_CCK_MSK                cpu_to_le32(1 << 1)
/* auto detection enable */
#define RXON_FLG_AUTO_DETECT_MSK        cpu_to_le32(1 << 2)
/* TGg protection when tx */
#define RXON_FLG_TGG_PROTECT_MSK        cpu_to_le32(1 << 3)
/* cck short slot & preamble */
#define RXON_FLG_SHORT_SLOT_MSK          cpu_to_le32(1 << 4)
#define RXON_FLG_SHORT_PREAMBLE_MSK     cpu_to_le32(1 << 5)
/* antenna selection */
#define RXON_FLG_DIS_DIV_MSK            cpu_to_le32(1 << 7)
#define RXON_FLG_ANT_SEL_MSK            cpu_to_le32(0x0f00)
#define RXON_FLG_ANT_A_MSK              cpu_to_le32(1 << 8)
#define RXON_FLG_ANT_B_MSK              cpu_to_le32(1 << 9)
/* radar detection enable */
#define RXON_FLG_RADAR_DETECT_MSK       cpu_to_le32(1 << 12)
#define RXON_FLG_TGJ_NARROW_BAND_MSK    cpu_to_le32(1 << 13)
/* rx response to host with 8-byte TSF
* (according to ON_AIR deassertion) */
#define RXON_FLG_TSF2HOST_MSK           cpu_to_le32(1 << 15)


/* HT flags */
#define RXON_FLG_CTRL_CHANNEL_LOC_POS		(22)
#define RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK	cpu_to_le32(0x1 << 22)

#define RXON_FLG_HT_OPERATING_MODE_POS		(23)

#define RXON_FLG_HT_PROT_MSK			cpu_to_le32(0x1 << 23)
#define RXON_FLG_HT40_PROT_MSK			cpu_to_le32(0x2 << 23)

#define RXON_FLG_CHANNEL_MODE_POS		(25)
#define RXON_FLG_CHANNEL_MODE_MSK		cpu_to_le32(0x3 << 25)

/* channel mode */
enum {
	CHANNEL_MODE_LEGACY = 0,
	CHANNEL_MODE_PURE_40 = 1,
	CHANNEL_MODE_MIXED = 2,
	CHANNEL_MODE_RESERVED = 3,
};
#define RXON_FLG_CHANNEL_MODE_LEGACY	cpu_to_le32(CHANNEL_MODE_LEGACY << RXON_FLG_CHANNEL_MODE_POS)
#define RXON_FLG_CHANNEL_MODE_PURE_40	cpu_to_le32(CHANNEL_MODE_PURE_40 << RXON_FLG_CHANNEL_MODE_POS)
#define RXON_FLG_CHANNEL_MODE_MIXED	cpu_to_le32(CHANNEL_MODE_MIXED << RXON_FLG_CHANNEL_MODE_POS)

/* CTS to self (if spec allows) flag */
#define RXON_FLG_SELF_CTS_EN			cpu_to_le32(0x1<<30)

/* rx_config filter flags */
/* accept all data frames */
#define RXON_FILTER_PROMISC_MSK         cpu_to_le32(1 << 0)
/* pass control & management to host */
#define RXON_FILTER_CTL2HOST_MSK        cpu_to_le32(1 << 1)
/* accept multi-cast */
#define RXON_FILTER_ACCEPT_GRP_MSK      cpu_to_le32(1 << 2)
/* don't decrypt uni-cast frames */
#define RXON_FILTER_DIS_DECRYPT_MSK     cpu_to_le32(1 << 3)
/* don't decrypt multi-cast frames */
#define RXON_FILTER_DIS_GRP_DECRYPT_MSK cpu_to_le32(1 << 4)
/* STA is associated */
#define RXON_FILTER_ASSOC_MSK           cpu_to_le32(1 << 5)
/* transfer to host non bssid beacons in associated state */
#define RXON_FILTER_BCON_AWARE_MSK      cpu_to_le32(1 << 6)

/**
 * REPLY_RXON = 0x10 (command, has simple generic response)
 *
 * RXON tunes the radio tuner to a service channel, and sets up a number
 * of parameters that are used primarily for Rx, but also for Tx operations.
 *
 * NOTE:  When tuning to a new channel, driver must set the
 *        RXON_FILTER_ASSOC_MSK to 0.  This will clear station-dependent
 *        info within the device, including the station tables, tx retry
 *        rate tables, and txpower tables.  Driver must build a new station
 *        table and txpower table before transmitting anything on the RXON
 *        channel.
 *
 * NOTE:  All RXONs wipe clean the internal txpower table.  Driver must
 *        issue a new REPLY_TX_PWR_TABLE_CMD after each REPLY_RXON (0x10),
 *        regardless of whether RXON_FILTER_ASSOC_MSK is set.
 */

struct iwl3945_rxon_cmd {
	u8 node_addr[6];
	__le16 reserved1;
	u8 bssid_addr[6];
	__le16 reserved2;
	u8 wlap_bssid_addr[6];
	__le16 reserved3;
	u8 dev_type;
	u8 air_propagation;
	__le16 reserved4;
	u8 ofdm_basic_rates;
	u8 cck_basic_rates;
	__le16 assoc_id;
	__le32 flags;
	__le32 filter_flags;
	__le16 channel;
	__le16 reserved5;
} __attribute__ ((packed));

struct iwl4965_rxon_cmd {
	u8 node_addr[6];
	__le16 reserved1;
	u8 bssid_addr[6];
	__le16 reserved2;
	u8 wlap_bssid_addr[6];
	__le16 reserved3;
	u8 dev_type;
	u8 air_propagation;
	__le16 rx_chain;
	u8 ofdm_basic_rates;
	u8 cck_basic_rates;
	__le16 assoc_id;
	__le32 flags;
	__le32 filter_flags;
	__le16 channel;
	u8 ofdm_ht_single_stream_basic_rates;
	u8 ofdm_ht_dual_stream_basic_rates;
} __attribute__ ((packed));

/* 5000 HW just extend this command */
struct iwl_rxon_cmd {
	u8 node_addr[6];
	__le16 reserved1;
	u8 bssid_addr[6];
	__le16 reserved2;
	u8 wlap_bssid_addr[6];
	__le16 reserved3;
	u8 dev_type;
	u8 air_propagation;
	__le16 rx_chain;
	u8 ofdm_basic_rates;
	u8 cck_basic_rates;
	__le16 assoc_id;
	__le32 flags;
	__le32 filter_flags;
	__le16 channel;
	u8 ofdm_ht_single_stream_basic_rates;
	u8 ofdm_ht_dual_stream_basic_rates;
	u8 ofdm_ht_triple_stream_basic_rates;
	u8 reserved5;
	__le16 acquisition_data;
	__le16 reserved6;
} __attribute__ ((packed));

/*
 * REPLY_RXON_ASSOC = 0x11 (command, has simple generic response)
 */
struct iwl3945_rxon_assoc_cmd {
	__le32 flags;
	__le32 filter_flags;
	u8 ofdm_basic_rates;
	u8 cck_basic_rates;
	__le16 reserved;
} __attribute__ ((packed));

struct iwl4965_rxon_assoc_cmd {
	__le32 flags;
	__le32 filter_flags;
	u8 ofdm_basic_rates;
	u8 cck_basic_rates;
	u8 ofdm_ht_single_stream_basic_rates;
	u8 ofdm_ht_dual_stream_basic_rates;
	__le16 rx_chain_select_flags;
	__le16 reserved;
} __attribute__ ((packed));

struct iwl5000_rxon_assoc_cmd {
	__le32 flags;
	__le32 filter_flags;
	u8 ofdm_basic_rates;
	u8 cck_basic_rates;
	__le16 reserved1;
	u8 ofdm_ht_single_stream_basic_rates;
	u8 ofdm_ht_dual_stream_basic_rates;
	u8 ofdm_ht_triple_stream_basic_rates;
	u8 reserved2;
	__le16 rx_chain_select_flags;
	__le16 acquisition_data;
	__le32 reserved3;
} __attribute__ ((packed));

#define IWL_CONN_MAX_LISTEN_INTERVAL	10
#define IWL_MAX_UCODE_BEACON_INTERVAL	4 /* 4096 */
#define IWL39_MAX_UCODE_BEACON_INTERVAL	1 /* 1024 */

/*
 * REPLY_RXON_TIMING = 0x14 (command, has simple generic response)
 */
struct iwl_rxon_time_cmd {
	__le64 timestamp;
	__le16 beacon_interval;
	__le16 atim_window;
	__le32 beacon_init_val;
	__le16 listen_interval;
	__le16 reserved;
} __attribute__ ((packed));

/*
 * REPLY_CHANNEL_SWITCH = 0x72 (command, has simple generic response)
 */
struct iwl3945_channel_switch_cmd {
	u8 band;
	u8 expect_beacon;
	__le16 channel;
	__le32 rxon_flags;
	__le32 rxon_filter_flags;
	__le32 switch_time;
	struct iwl3945_power_per_rate power[IWL_MAX_RATES];
} __attribute__ ((packed));

struct iwl_channel_switch_cmd {
	u8 band;
	u8 expect_beacon;
	__le16 channel;
	__le32 rxon_flags;
	__le32 rxon_filter_flags;
	__le32 switch_time;
	struct iwl4965_tx_power_db tx_power;
} __attribute__ ((packed));

/*
 * CHANNEL_SWITCH_NOTIFICATION = 0x73 (notification only, not a command)
 */
struct iwl_csa_notification {
	__le16 band;
	__le16 channel;
	__le32 status;		/* 0 - OK, 1 - fail */
} __attribute__ ((packed));

/******************************************************************************
 * (2)
 * Quality-of-Service (QOS) Commands & Responses:
 *
 *****************************************************************************/

/**
 * struct iwl_ac_qos -- QOS timing params for REPLY_QOS_PARAM
 * One for each of 4 EDCA access categories in struct iwl_qosparam_cmd
 *
 * @cw_min: Contention window, start value in numbers of slots.
 *          Should be a power-of-2, minus 1.  Device's default is 0x0f.
 * @cw_max: Contention window, max value in numbers of slots.
 *          Should be a power-of-2, minus 1.  Device's default is 0x3f.
 * @aifsn:  Number of slots in Arbitration Interframe Space (before
 *          performing random backoff timing prior to Tx).  Device default 1.
 * @edca_txop:  Length of Tx opportunity, in uSecs.  Device default is 0.
 *
 * Device will automatically increase contention window by (2*CW) + 1 for each
 * transmission retry.  Device uses cw_max as a bit mask, ANDed with new CW
 * value, to cap the CW value.
 */
struct iwl_ac_qos {
	__le16 cw_min;
	__le16 cw_max;
	u8 aifsn;
	u8 reserved1;
	__le16 edca_txop;
} __attribute__ ((packed));

/* QoS flags defines */
#define QOS_PARAM_FLG_UPDATE_EDCA_MSK	cpu_to_le32(0x01)
#define QOS_PARAM_FLG_TGN_MSK		cpu_to_le32(0x02)
#define QOS_PARAM_FLG_TXOP_TYPE_MSK	cpu_to_le32(0x10)

/* Number of Access Categories (AC) (EDCA), queues 0..3 */
#define AC_NUM                4

/*
 * REPLY_QOS_PARAM = 0x13 (command, has simple generic response)
 *
 * This command sets up timings for each of the 4 prioritized EDCA Tx FIFOs
 * 0: Background, 1: Best Effort, 2: Video, 3: Voice.
 */
struct iwl_qosparam_cmd {
	__le32 qos_flags;
	struct iwl_ac_qos ac[AC_NUM];
} __attribute__ ((packed));

/******************************************************************************
 * (3)
 * Add/Modify Stations Commands & Responses:
 *
 *****************************************************************************/
/*
 * Multi station support
 */

/* Special, dedicated locations within device's station table */
#define	IWL_AP_ID		0
#define IWL_MULTICAST_ID	1
#define	IWL_STA_ID		2
#define	IWL3945_BROADCAST_ID	24
#define IWL3945_STATION_COUNT	25
#define IWL4965_BROADCAST_ID	31
#define	IWL4965_STATION_COUNT	32
#define IWL5000_BROADCAST_ID	15
#define	IWL5000_STATION_COUNT	16

#define	IWL_STATION_COUNT	32 	/* MAX(3945,4965)*/
#define	IWL_INVALID_STATION 	255

#define STA_FLG_TX_RATE_MSK		cpu_to_le32(1 << 2);
#define STA_FLG_PWR_SAVE_MSK		cpu_to_le32(1 << 8);
#define STA_FLG_RTS_MIMO_PROT_MSK	cpu_to_le32(1 << 17)
#define STA_FLG_AGG_MPDU_8US_MSK	cpu_to_le32(1 << 18)
#define STA_FLG_MAX_AGG_SIZE_POS	(19)
#define STA_FLG_MAX_AGG_SIZE_MSK	cpu_to_le32(3 << 19)
#define STA_FLG_HT40_EN_MSK		cpu_to_le32(1 << 21)
#define STA_FLG_MIMO_DIS_MSK		cpu_to_le32(1 << 22)
#define STA_FLG_AGG_MPDU_DENSITY_POS	(23)
#define STA_FLG_AGG_MPDU_DENSITY_MSK	cpu_to_le32(7 << 23)

/* Use in mode field.  1: modify existing entry, 0: add new station entry */
#define STA_CONTROL_MODIFY_MSK		0x01

/* key flags __le16*/
#define STA_KEY_FLG_ENCRYPT_MSK	cpu_to_le16(0x0007)
#define STA_KEY_FLG_NO_ENC	cpu_to_le16(0x0000)
#define STA_KEY_FLG_WEP		cpu_to_le16(0x0001)
#define STA_KEY_FLG_CCMP	cpu_to_le16(0x0002)
#define STA_KEY_FLG_TKIP	cpu_to_le16(0x0003)

#define STA_KEY_FLG_KEYID_POS	8
#define STA_KEY_FLG_INVALID 	cpu_to_le16(0x0800)
/* wep key is either from global key (0) or from station info array (1) */
#define STA_KEY_FLG_MAP_KEY_MSK	cpu_to_le16(0x0008)

/* wep key in STA: 5-bytes (0) or 13-bytes (1) */
#define STA_KEY_FLG_KEY_SIZE_MSK     cpu_to_le16(0x1000)
#define STA_KEY_MULTICAST_MSK        cpu_to_le16(0x4000)
#define STA_KEY_MAX_NUM		8

/* Flags indicate whether to modify vs. don't change various station params */
#define	STA_MODIFY_KEY_MASK		0x01
#define	STA_MODIFY_TID_DISABLE_TX	0x02
#define	STA_MODIFY_TX_RATE_MSK		0x04
#define STA_MODIFY_ADDBA_TID_MSK	0x08
#define STA_MODIFY_DELBA_TID_MSK	0x10

/* Receiver address (actually, Rx station's index into station table),
 * combined with Traffic ID (QOS priority), in format used by Tx Scheduler */
#define BUILD_RAxTID(sta_id, tid)	(((sta_id) << 4) + (tid))

struct iwl4965_keyinfo {
	__le16 key_flags;
	u8 tkip_rx_tsc_byte2;	/* TSC[2] for key mix ph1 detection */
	u8 reserved1;
	__le16 tkip_rx_ttak[5];	/* 10-byte unicast TKIP TTAK */
	u8 key_offset;
	u8 reserved2;
	u8 key[16];		/* 16-byte unicast decryption key */
} __attribute__ ((packed));

/* 5000 */
struct iwl_keyinfo {
	__le16 key_flags;
	u8 tkip_rx_tsc_byte2;	/* TSC[2] for key mix ph1 detection */
	u8 reserved1;
	__le16 tkip_rx_ttak[5];	/* 10-byte unicast TKIP TTAK */
	u8 key_offset;
	u8 reserved2;
	u8 key[16];		/* 16-byte unicast decryption key */
	__le64 tx_secur_seq_cnt;
	__le64 hw_tkip_mic_rx_key;
	__le64 hw_tkip_mic_tx_key;
} __attribute__ ((packed));

/**
 * struct sta_id_modify
 * @addr[ETH_ALEN]: station's MAC address
 * @sta_id: index of station in uCode's station table
 * @modify_mask: STA_MODIFY_*, 1: modify, 0: don't change
 *
 * Driver selects unused table index when adding new station,
 * or the index to a pre-existing station entry when modifying that station.
 * Some indexes have special purposes (IWL_AP_ID, index 0, is for AP).
 *
 * modify_mask flags select which parameters to modify vs. leave alone.
 */
struct sta_id_modify {
	u8 addr[ETH_ALEN];
	__le16 reserved1;
	u8 sta_id;
	u8 modify_mask;
	__le16 reserved2;
} __attribute__ ((packed));

/*
 * REPLY_ADD_STA = 0x18 (command)
 *
 * The device contains an internal table of per-station information,
 * with info on security keys, aggregation parameters, and Tx rates for
 * initial Tx attempt and any retries (4965 uses REPLY_TX_LINK_QUALITY_CMD,
 * 3945 uses REPLY_RATE_SCALE to set up rate tables).
 *
 * REPLY_ADD_STA sets up the table entry for one station, either creating
 * a new entry, or modifying a pre-existing one.
 *
 * NOTE:  RXON command (without "associated" bit set) wipes the station table
 *        clean.  Moving into RF_KILL state does this also.  Driver must set up
 *        new station table before transmitting anything on the RXON channel
 *        (except active scans or active measurements; those commands carry
 *        their own txpower/rate setup data).
 *
 *        When getting started on a new channel, driver must set up the
 *        IWL_BROADCAST_ID entry (last entry in the table).  For a client
 *        station in a BSS, once an AP is selected, driver sets up the AP STA
 *        in the IWL_AP_ID entry (1st entry in the table).  BROADCAST and AP
 *        are all that are needed for a BSS client station.  If the device is
 *        used as AP, or in an IBSS network, driver must set up station table
 *        entries for all STAs in network, starting with index IWL_STA_ID.
 */

struct iwl3945_addsta_cmd {
	u8 mode;		/* 1: modify existing, 0: add new station */
	u8 reserved[3];
	struct sta_id_modify sta;
	struct iwl4965_keyinfo key;
	__le32 station_flags;		/* STA_FLG_* */
	__le32 station_flags_msk;	/* STA_FLG_* */

	/* bit field to disable (1) or enable (0) Tx for Traffic ID (TID)
	 * corresponding to bit (e.g. bit 5 controls TID 5).
	 * Set modify_mask bit STA_MODIFY_TID_DISABLE_TX to use this field. */
	__le16 tid_disable_tx;

	__le16 rate_n_flags;

	/* TID for which to add block-ack support.
	 * Set modify_mask bit STA_MODIFY_ADDBA_TID_MSK to use this field. */
	u8 add_immediate_ba_tid;

	/* TID for which to remove block-ack support.
	 * Set modify_mask bit STA_MODIFY_DELBA_TID_MSK to use this field. */
	u8 remove_immediate_ba_tid;

	/* Starting Sequence Number for added block-ack support.
	 * Set modify_mask bit STA_MODIFY_ADDBA_TID_MSK to use this field. */
	__le16 add_immediate_ba_ssn;
} __attribute__ ((packed));

struct iwl4965_addsta_cmd {
	u8 mode;		/* 1: modify existing, 0: add new station */
	u8 reserved[3];
	struct sta_id_modify sta;
	struct iwl4965_keyinfo key;
	__le32 station_flags;		/* STA_FLG_* */
	__le32 station_flags_msk;	/* STA_FLG_* */

	/* bit field to disable (1) or enable (0) Tx for Traffic ID (TID)
	 * corresponding to bit (e.g. bit 5 controls TID 5).
	 * Set modify_mask bit STA_MODIFY_TID_DISABLE_TX to use this field. */
	__le16 tid_disable_tx;

	__le16	reserved1;

	/* TID for which to add block-ack support.
	 * Set modify_mask bit STA_MODIFY_ADDBA_TID_MSK to use this field. */
	u8 add_immediate_ba_tid;

	/* TID for which to remove block-ack support.
	 * Set modify_mask bit STA_MODIFY_DELBA_TID_MSK to use this field. */
	u8 remove_immediate_ba_tid;

	/* Starting Sequence Number for added block-ack support.
	 * Set modify_mask bit STA_MODIFY_ADDBA_TID_MSK to use this field. */
	__le16 add_immediate_ba_ssn;

	__le32 reserved2;
} __attribute__ ((packed));

/* 5000 */
struct iwl_addsta_cmd {
	u8 mode;		/* 1: modify existing, 0: add new station */
	u8 reserved[3];
	struct sta_id_modify sta;
	struct iwl_keyinfo key;
	__le32 station_flags;		/* STA_FLG_* */
	__le32 station_flags_msk;	/* STA_FLG_* */

	/* bit field to disable (1) or enable (0) Tx for Traffic ID (TID)
	 * corresponding to bit (e.g. bit 5 controls TID 5).
	 * Set modify_mask bit STA_MODIFY_TID_DISABLE_TX to use this field. */
	__le16 tid_disable_tx;

	__le16	rate_n_flags;		/* 3945 only */

	/* TID for which to add block-ack support.
	 * Set modify_mask bit STA_MODIFY_ADDBA_TID_MSK to use this field. */
	u8 add_immediate_ba_tid;

	/* TID for which to remove block-ack support.
	 * Set modify_mask bit STA_MODIFY_DELBA_TID_MSK to use this field. */
	u8 remove_immediate_ba_tid;

	/* Starting Sequence Number for added block-ack support.
	 * Set modify_mask bit STA_MODIFY_ADDBA_TID_MSK to use this field. */
	__le16 add_immediate_ba_ssn;

	__le32 reserved2;
} __attribute__ ((packed));


#define ADD_STA_SUCCESS_MSK		0x1
#define ADD_STA_NO_ROOM_IN_TABLE	0x2
#define ADD_STA_NO_BLOCK_ACK_RESOURCE	0x4
#define ADD_STA_MODIFY_NON_EXIST_STA	0x8
/*
 * REPLY_ADD_STA = 0x18 (response)
 */
struct iwl_add_sta_resp {
	u8 status;	/* ADD_STA_* */
} __attribute__ ((packed));

#define REM_STA_SUCCESS_MSK              0x1
/*
 *  REPLY_REM_STA = 0x19 (response)
 */
struct iwl_rem_sta_resp {
	u8 status;
} __attribute__ ((packed));

/*
 *  REPLY_REM_STA = 0x19 (command)
 */
struct iwl_rem_sta_cmd {
	u8 num_sta;     /* number of removed stations */
	u8 reserved[3];
	u8 addr[ETH_ALEN]; /* MAC addr of the first station */
	u8 reserved2[2];
} __attribute__ ((packed));

/*
 * REPLY_WEP_KEY = 0x20
 */
struct iwl_wep_key {
	u8 key_index;
	u8 key_offset;
	u8 reserved1[2];
	u8 key_size;
	u8 reserved2[3];
	u8 key[16];
} __attribute__ ((packed));

struct iwl_wep_cmd {
	u8 num_keys;
	u8 global_key_type;
	u8 flags;
	u8 reserved;
	struct iwl_wep_key key[0];
} __attribute__ ((packed));

#define WEP_KEY_WEP_TYPE 1
#define WEP_KEYS_MAX 4
#define WEP_INVALID_OFFSET 0xff
#define WEP_KEY_LEN_64 5
#define WEP_KEY_LEN_128 13

/******************************************************************************
 * (4)
 * Rx Responses:
 *
 *****************************************************************************/

#define RX_RES_STATUS_NO_CRC32_ERROR	cpu_to_le32(1 << 0)
#define RX_RES_STATUS_NO_RXE_OVERFLOW	cpu_to_le32(1 << 1)

#define RX_RES_PHY_FLAGS_BAND_24_MSK	cpu_to_le16(1 << 0)
#define RX_RES_PHY_FLAGS_MOD_CCK_MSK		cpu_to_le16(1 << 1)
#define RX_RES_PHY_FLAGS_SHORT_PREAMBLE_MSK	cpu_to_le16(1 << 2)
#define RX_RES_PHY_FLAGS_NARROW_BAND_MSK	cpu_to_le16(1 << 3)
#define RX_RES_PHY_FLAGS_ANTENNA_MSK		0xf0
#define RX_RES_PHY_FLAGS_ANTENNA_POS		4

#define RX_RES_STATUS_SEC_TYPE_MSK	(0x7 << 8)
#define RX_RES_STATUS_SEC_TYPE_NONE	(0x0 << 8)
#define RX_RES_STATUS_SEC_TYPE_WEP	(0x1 << 8)
#define RX_RES_STATUS_SEC_TYPE_CCMP	(0x2 << 8)
#define RX_RES_STATUS_SEC_TYPE_TKIP	(0x3 << 8)
#define	RX_RES_STATUS_SEC_TYPE_ERR	(0x7 << 8)

#define RX_RES_STATUS_STATION_FOUND	(1<<6)
#define RX_RES_STATUS_NO_STATION_INFO_MISMATCH	(1<<7)

#define RX_RES_STATUS_DECRYPT_TYPE_MSK	(0x3 << 11)
#define RX_RES_STATUS_NOT_DECRYPT	(0x0 << 11)
#define RX_RES_STATUS_DECRYPT_OK	(0x3 << 11)
#define RX_RES_STATUS_BAD_ICV_MIC	(0x1 << 11)
#define RX_RES_STATUS_BAD_KEY_TTAK	(0x2 << 11)

#define RX_MPDU_RES_STATUS_ICV_OK	(0x20)
#define RX_MPDU_RES_STATUS_MIC_OK	(0x40)
#define RX_MPDU_RES_STATUS_TTAK_OK	(1 << 7)
#define RX_MPDU_RES_STATUS_DEC_DONE_MSK	(0x800)


struct iwl3945_rx_frame_stats {
	u8 phy_count;
	u8 id;
	u8 rssi;
	u8 agc;
	__le16 sig_avg;
	__le16 noise_diff;
	u8 payload[0];
} __attribute__ ((packed));

struct iwl3945_rx_frame_hdr {
	__le16 channel;
	__le16 phy_flags;
	u8 reserved1;
	u8 rate;
	__le16 len;
	u8 payload[0];
} __attribute__ ((packed));

struct iwl3945_rx_frame_end {
	__le32 status;
	__le64 timestamp;
	__le32 beacon_timestamp;
} __attribute__ ((packed));

/*
 * REPLY_3945_RX = 0x1b (response only, not a command)
 *
 * NOTE:  DO NOT dereference from casts to this structure
 * It is provided only for calculating minimum data set size.
 * The actual offsets of the hdr and end are dynamic based on
 * stats.phy_count
 */
struct iwl3945_rx_frame {
	struct iwl3945_rx_frame_stats stats;
	struct iwl3945_rx_frame_hdr hdr;
	struct iwl3945_rx_frame_end end;
} __attribute__ ((packed));

#define IWL39_RX_FRAME_SIZE	(4 + sizeof(struct iwl3945_rx_frame))

/* Fixed (non-configurable) rx data from phy */

#define IWL49_RX_RES_PHY_CNT 14
#define IWL49_RX_PHY_FLAGS_ANTENNAE_OFFSET	(4)
#define IWL49_RX_PHY_FLAGS_ANTENNAE_MASK	(0x70)
#define IWL49_AGC_DB_MASK			(0x3f80)	/* MASK(7,13) */
#define IWL49_AGC_DB_POS			(7)
struct iwl4965_rx_non_cfg_phy {
	__le16 ant_selection;	/* ant A bit 4, ant B bit 5, ant C bit 6 */
	__le16 agc_info;	/* agc code 0:6, agc dB 7:13, reserved 14:15 */
	u8 rssi_info[6];	/* we use even entries, 0/2/4 for A/B/C rssi */
	u8 pad[0];
} __attribute__ ((packed));


#define IWL50_RX_RES_PHY_CNT 8
#define IWL50_RX_RES_AGC_IDX     1
#define IWL50_RX_RES_RSSI_AB_IDX 2
#define IWL50_RX_RES_RSSI_C_IDX  3
#define IWL50_OFDM_AGC_MSK 0xfe00
#define IWL50_OFDM_AGC_BIT_POS 9
#define IWL50_OFDM_RSSI_A_MSK 0x00ff
#define IWL50_OFDM_RSSI_A_BIT_POS 0
#define IWL50_OFDM_RSSI_B_MSK 0xff0000
#define IWL50_OFDM_RSSI_B_BIT_POS 16
#define IWL50_OFDM_RSSI_C_MSK 0x00ff
#define IWL50_OFDM_RSSI_C_BIT_POS 0

struct iwl5000_non_cfg_phy {
	__le32 non_cfg_phy[IWL50_RX_RES_PHY_CNT];  /* up to 8 phy entries */
} __attribute__ ((packed));


/*
 * REPLY_RX = 0xc3 (response only, not a command)
 * Used only for legacy (non 11n) frames.
 */
struct iwl_rx_phy_res {
	u8 non_cfg_phy_cnt;     /* non configurable DSP phy data byte count */
	u8 cfg_phy_cnt;		/* configurable DSP phy data byte count */
	u8 stat_id;		/* configurable DSP phy data set ID */
	u8 reserved1;
	__le64 timestamp;	/* TSF at on air rise */
	__le32 beacon_time_stamp; /* beacon at on-air rise */
	__le16 phy_flags;	/* general phy flags: band, modulation, ... */
	__le16 channel;		/* channel number */
	u8 non_cfg_phy_buf[32]; /* for various implementations of non_cfg_phy */
	__le32 rate_n_flags;	/* RATE_MCS_* */
	__le16 byte_count;	/* frame's byte-count */
	__le16 reserved3;
} __attribute__ ((packed));

struct iwl4965_rx_mpdu_res_start {
	__le16 byte_count;
	__le16 reserved;
} __attribute__ ((packed));


/******************************************************************************
 * (5)
 * Tx Commands & Responses:
 *
 * Driver must place each REPLY_TX command into one of the prioritized Tx
 * queues in host DRAM, shared between driver and device (see comments for
 * SCD registers and Tx/Rx Queues).  When the device's Tx scheduler and uCode
 * are preparing to transmit, the device pulls the Tx command over the PCI
 * bus via one of the device's Tx DMA channels, to fill an internal FIFO
 * from which data will be transmitted.
 *
 * uCode handles all timing and protocol related to control frames
 * (RTS/CTS/ACK), based on flags in the Tx command.  uCode and Tx scheduler
 * handle reception of block-acks; uCode updates the host driver via
 * REPLY_COMPRESSED_BA (4965).
 *
 * uCode handles retrying Tx when an ACK is expected but not received.
 * This includes trying lower data rates than the one requested in the Tx
 * command, as set up by the REPLY_RATE_SCALE (for 3945) or
 * REPLY_TX_LINK_QUALITY_CMD (4965).
 *
 * Driver sets up transmit power for various rates via REPLY_TX_PWR_TABLE_CMD.
 * This command must be executed after every RXON command, before Tx can occur.
 *****************************************************************************/

/* REPLY_TX Tx flags field */

/* 1: Use RTS/CTS protocol or CTS-to-self if spec allows it
 * before this frame. if CTS-to-self required check
 * RXON_FLG_SELF_CTS_EN status. */
#define TX_CMD_FLG_RTS_CTS_MSK cpu_to_le32(1 << 0)

/* 1: Use Request-To-Send protocol before this frame.
 * Mutually exclusive vs. TX_CMD_FLG_CTS_MSK. */
#define TX_CMD_FLG_RTS_MSK cpu_to_le32(1 << 1)

/* 1: Transmit Clear-To-Send to self before this frame.
 * Driver should set this for AUTH/DEAUTH/ASSOC-REQ/REASSOC mgmnt frames.
 * Mutually exclusive vs. TX_CMD_FLG_RTS_MSK. */
#define TX_CMD_FLG_CTS_MSK cpu_to_le32(1 << 2)

/* 1: Expect ACK from receiving station
 * 0: Don't expect ACK (MAC header's duration field s/b 0)
 * Set this for unicast frames, but not broadcast/multicast. */
#define TX_CMD_FLG_ACK_MSK cpu_to_le32(1 << 3)

/* For 4965:
 * 1: Use rate scale table (see REPLY_TX_LINK_QUALITY_CMD).
 *    Tx command's initial_rate_index indicates first rate to try;
 *    uCode walks through table for additional Tx attempts.
 * 0: Use Tx rate/MCS from Tx command's rate_n_flags field.
 *    This rate will be used for all Tx attempts; it will not be scaled. */
#define TX_CMD_FLG_STA_RATE_MSK cpu_to_le32(1 << 4)

/* 1: Expect immediate block-ack.
 * Set when Txing a block-ack request frame.  Also set TX_CMD_FLG_ACK_MSK. */
#define TX_CMD_FLG_IMM_BA_RSP_MASK  cpu_to_le32(1 << 6)

/* 1: Frame requires full Tx-Op protection.
 * Set this if either RTS or CTS Tx Flag gets set. */
#define TX_CMD_FLG_FULL_TXOP_PROT_MSK cpu_to_le32(1 << 7)

/* Tx antenna selection field; used only for 3945, reserved (0) for 4965.
 * Set field to "0" to allow 3945 uCode to select antenna (normal usage). */
#define TX_CMD_FLG_ANT_SEL_MSK cpu_to_le32(0xf00)
#define TX_CMD_FLG_ANT_A_MSK cpu_to_le32(1 << 8)
#define TX_CMD_FLG_ANT_B_MSK cpu_to_le32(1 << 9)

/* 1: Ignore Bluetooth priority for this frame.
 * 0: Delay Tx until Bluetooth device is done (normal usage). */
#define TX_CMD_FLG_BT_DIS_MSK cpu_to_le32(1 << 12)

/* 1: uCode overrides sequence control field in MAC header.
 * 0: Driver provides sequence control field in MAC header.
 * Set this for management frames, non-QOS data frames, non-unicast frames,
 * and also in Tx command embedded in REPLY_SCAN_CMD for active scans. */
#define TX_CMD_FLG_SEQ_CTL_MSK cpu_to_le32(1 << 13)

/* 1: This frame is non-last MPDU; more fragments are coming.
 * 0: Last fragment, or not using fragmentation. */
#define TX_CMD_FLG_MORE_FRAG_MSK cpu_to_le32(1 << 14)

/* 1: uCode calculates and inserts Timestamp Function (TSF) in outgoing frame.
 * 0: No TSF required in outgoing frame.
 * Set this for transmitting beacons and probe responses. */
#define TX_CMD_FLG_TSF_MSK cpu_to_le32(1 << 16)

/* 1: Driver inserted 2 bytes pad after the MAC header, for (required) dword
 *    alignment of frame's payload data field.
 * 0: No pad
 * Set this for MAC headers with 26 or 30 bytes, i.e. those with QOS or ADDR4
 * field (but not both).  Driver must align frame data (i.e. data following
 * MAC header) to DWORD boundary. */
#define TX_CMD_FLG_MH_PAD_MSK cpu_to_le32(1 << 20)

/* accelerate aggregation support
 * 0 - no CCMP encryption; 1 - CCMP encryption */
#define TX_CMD_FLG_AGG_CCMP_MSK cpu_to_le32(1 << 22)

/* HCCA-AP - disable duration overwriting. */
#define TX_CMD_FLG_DUR_MSK cpu_to_le32(1 << 25)


/*
 * TX command security control
 */
#define TX_CMD_SEC_WEP  	0x01
#define TX_CMD_SEC_CCM  	0x02
#define TX_CMD_SEC_TKIP		0x03
#define TX_CMD_SEC_MSK		0x03
#define TX_CMD_SEC_SHIFT	6
#define TX_CMD_SEC_KEY128	0x08

/*
 * security overhead sizes
 */
#define WEP_IV_LEN 4
#define WEP_ICV_LEN 4
#define CCMP_MIC_LEN 8
#define TKIP_ICV_LEN 4

/*
 * REPLY_TX = 0x1c (command)
 */

struct iwl3945_tx_cmd {
	/*
	 * MPDU byte count:
	 * MAC header (24/26/30/32 bytes) + 2 bytes pad if 26/30 header size,
	 * + 8 byte IV for CCM or TKIP (not used for WEP)
	 * + Data payload
	 * + 8-byte MIC (not used for CCM/WEP)
	 * NOTE:  Does not include Tx command bytes, post-MAC pad bytes,
	 *        MIC (CCM) 8 bytes, ICV (WEP/TKIP/CKIP) 4 bytes, CRC 4 bytes.i
	 * Range: 14-2342 bytes.
	 */
	__le16 len;

	/*
	 * MPDU or MSDU byte count for next frame.
	 * Used for fragmentation and bursting, but not 11n aggregation.
	 * Same as "len", but for next frame.  Set to 0 if not applicable.
	 */
	__le16 next_frame_len;

	__le32 tx_flags;	/* TX_CMD_FLG_* */

	u8 rate;

	/* Index of recipient station in uCode's station table */
	u8 sta_id;
	u8 tid_tspec;
	u8 sec_ctl;
	u8 key[16];
	union {
		u8 byte[8];
		__le16 word[4];
		__le32 dw[2];
	} tkip_mic;
	__le32 next_frame_info;
	union {
		__le32 life_time;
		__le32 attempt;
	} stop_time;
	u8 supp_rates[2];
	u8 rts_retry_limit;	/*byte 50 */
	u8 data_retry_limit;	/*byte 51 */
	union {
		__le16 pm_frame_timeout;
		__le16 attempt_duration;
	} timeout;

	/*
	 * Duration of EDCA burst Tx Opportunity, in 32-usec units.
	 * Set this if txop time is not specified by HCCA protocol (e.g. by AP).
	 */
	__le16 driver_txop;

	/*
	 * MAC header goes here, followed by 2 bytes padding if MAC header
	 * length is 26 or 30 bytes, followed by payload data
	 */
	u8 payload[0];
	struct ieee80211_hdr hdr[0];
} __attribute__ ((packed));

/*
 * REPLY_TX = 0x1c (response)
 */
struct iwl3945_tx_resp {
	u8 failure_rts;
	u8 failure_frame;
	u8 bt_kill_count;
	u8 rate;
	__le32 wireless_media_time;
	__le32 status;		/* TX status */
} __attribute__ ((packed));


/*
 * 4965 uCode updates these Tx attempt count values in host DRAM.
 * Used for managing Tx retries when expecting block-acks.
 * Driver should set these fields to 0.
 */
struct iwl_dram_scratch {
	u8 try_cnt;		/* Tx attempts */
	u8 bt_kill_cnt;		/* Tx attempts blocked by Bluetooth device */
	__le16 reserved;
} __attribute__ ((packed));

struct iwl_tx_cmd {
	/*
	 * MPDU byte count:
	 * MAC header (24/26/30/32 bytes) + 2 bytes pad if 26/30 header size,
	 * + 8 byte IV for CCM or TKIP (not used for WEP)
	 * + Data payload
	 * + 8-byte MIC (not used for CCM/WEP)
	 * NOTE:  Does not include Tx command bytes, post-MAC pad bytes,
	 *        MIC (CCM) 8 bytes, ICV (WEP/TKIP/CKIP) 4 bytes, CRC 4 bytes.i
	 * Range: 14-2342 bytes.
	 */
	__le16 len;

	/*
	 * MPDU or MSDU byte count for next frame.
	 * Used for fragmentation and bursting, but not 11n aggregation.
	 * Same as "len", but for next frame.  Set to 0 if not applicable.
	 */
	__le16 next_frame_len;

	__le32 tx_flags;	/* TX_CMD_FLG_* */

	/* uCode may modify this field of the Tx command (in host DRAM!).
	 * Driver must also set dram_lsb_ptr and dram_msb_ptr in this cmd. */
	struct iwl_dram_scratch scratch;

	/* Rate for *all* Tx attempts, if TX_CMD_FLG_STA_RATE_MSK is cleared. */
	__le32 rate_n_flags;	/* RATE_MCS_* */

	/* Index of destination station in uCode's station table */
	u8 sta_id;

	/* Type of security encryption:  CCM or TKIP */
	u8 sec_ctl;		/* TX_CMD_SEC_* */

	/*
	 * Index into rate table (see REPLY_TX_LINK_QUALITY_CMD) for initial
	 * Tx attempt, if TX_CMD_FLG_STA_RATE_MSK is set.  Normally "0" for
	 * data frames, this field may be used to selectively reduce initial
	 * rate (via non-0 value) for special frames (e.g. management), while
	 * still supporting rate scaling for all frames.
	 */
	u8 initial_rate_index;
	u8 reserved;
	u8 key[16];
	__le16 next_frame_flags;
	__le16 reserved2;
	union {
		__le32 life_time;
		__le32 attempt;
	} stop_time;

	/* Host DRAM physical address pointer to "scratch" in this command.
	 * Must be dword aligned.  "0" in dram_lsb_ptr disables usage. */
	__le32 dram_lsb_ptr;
	u8 dram_msb_ptr;

	u8 rts_retry_limit;	/*byte 50 */
	u8 data_retry_limit;	/*byte 51 */
	u8 tid_tspec;
	union {
		__le16 pm_frame_timeout;
		__le16 attempt_duration;
	} timeout;

	/*
	 * Duration of EDCA burst Tx Opportunity, in 32-usec units.
	 * Set this if txop time is not specified by HCCA protocol (e.g. by AP).
	 */
	__le16 driver_txop;

	/*
	 * MAC header goes here, followed by 2 bytes padding if MAC header
	 * length is 26 or 30 bytes, followed by payload data
	 */
	u8 payload[0];
	struct ieee80211_hdr hdr[0];
} __attribute__ ((packed));

/* TX command response is sent after *all* transmission attempts.
 *
 * NOTES:
 *
 * TX_STATUS_FAIL_NEXT_FRAG
 *
 * If the fragment flag in the MAC header for the frame being transmitted
 * is set and there is insufficient time to transmit the next frame, the
 * TX status will be returned with 'TX_STATUS_FAIL_NEXT_FRAG'.
 *
 * TX_STATUS_FIFO_UNDERRUN
 *
 * Indicates the host did not provide bytes to the FIFO fast enough while
 * a TX was in progress.
 *
 * TX_STATUS_FAIL_MGMNT_ABORT
 *
 * This status is only possible if the ABORT ON MGMT RX parameter was
 * set to true with the TX command.
 *
 * If the MSB of the status parameter is set then an abort sequence is
 * required.  This sequence consists of the host activating the TX Abort
 * control line, and then waiting for the TX Abort command response.  This
 * indicates that a the device is no longer in a transmit state, and that the
 * command FIFO has been cleared.  The host must then deactivate the TX Abort
 * control line.  Receiving is still allowed in this case.
 */
enum {
	TX_STATUS_SUCCESS = 0x01,
	TX_STATUS_DIRECT_DONE = 0x02,
	TX_STATUS_FAIL_SHORT_LIMIT = 0x82,
	TX_STATUS_FAIL_LONG_LIMIT = 0x83,
	TX_STATUS_FAIL_FIFO_UNDERRUN = 0x84,
	TX_STATUS_FAIL_MGMNT_ABORT = 0x85,
	TX_STATUS_FAIL_NEXT_FRAG = 0x86,
	TX_STATUS_FAIL_LIFE_EXPIRE = 0x87,
	TX_STATUS_FAIL_DEST_PS = 0x88,
	TX_STATUS_FAIL_ABORTED = 0x89,
	TX_STATUS_FAIL_BT_RETRY = 0x8a,
	TX_STATUS_FAIL_STA_INVALID = 0x8b,
	TX_STATUS_FAIL_FRAG_DROPPED = 0x8c,
	TX_STATUS_FAIL_TID_DISABLE = 0x8d,
	TX_STATUS_FAIL_FRAME_FLUSHED = 0x8e,
	TX_STATUS_FAIL_INSUFFICIENT_CF_POLL = 0x8f,
	TX_STATUS_FAIL_TX_LOCKED = 0x90,
	TX_STATUS_FAIL_NO_BEACON_ON_RADAR = 0x91,
};

#define	TX_PACKET_MODE_REGULAR		0x0000
#define	TX_PACKET_MODE_BURST_SEQ	0x0100
#define	TX_PACKET_MODE_BURST_FIRST	0x0200

enum {
	TX_POWER_PA_NOT_ACTIVE = 0x0,
};

enum {
	TX_STATUS_MSK = 0x000000ff,		/* bits 0:7 */
	TX_STATUS_DELAY_MSK = 0x00000040,
	TX_STATUS_ABORT_MSK = 0x00000080,
	TX_PACKET_MODE_MSK = 0x0000ff00,	/* bits 8:15 */
	TX_FIFO_NUMBER_MSK = 0x00070000,	/* bits 16:18 */
	TX_RESERVED = 0x00780000,		/* bits 19:22 */
	TX_POWER_PA_DETECT_MSK = 0x7f800000,	/* bits 23:30 */
	TX_ABORT_REQUIRED_MSK = 0x80000000,	/* bits 31:31 */
};

static inline bool iwl_is_tx_success(u32 status)
{
	status &= TX_STATUS_MSK;
	return (status == TX_STATUS_SUCCESS) ||
	       (status == TX_STATUS_DIRECT_DONE);
}



/* *******************************
 * TX aggregation status
 ******************************* */

enum {
	AGG_TX_STATE_TRANSMITTED = 0x00,
	AGG_TX_STATE_UNDERRUN_MSK = 0x01,
	AGG_TX_STATE_BT_PRIO_MSK = 0x02,
	AGG_TX_STATE_FEW_BYTES_MSK = 0x04,
	AGG_TX_STATE_ABORT_MSK = 0x08,
	AGG_TX_STATE_LAST_SENT_TTL_MSK = 0x10,
	AGG_TX_STATE_LAST_SENT_TRY_CNT_MSK = 0x20,
	AGG_TX_STATE_LAST_SENT_BT_KILL_MSK = 0x40,
	AGG_TX_STATE_SCD_QUERY_MSK = 0x80,
	AGG_TX_STATE_TEST_BAD_CRC32_MSK = 0x100,
	AGG_TX_STATE_RESPONSE_MSK = 0x1ff,
	AGG_TX_STATE_DUMP_TX_MSK = 0x200,
	AGG_TX_STATE_DELAY_TX_MSK = 0x400
};

#define AGG_TX_STATE_LAST_SENT_MSK  (AGG_TX_STATE_LAST_SENT_TTL_MSK | \
				     AGG_TX_STATE_LAST_SENT_TRY_CNT_MSK | \
				     AGG_TX_STATE_LAST_SENT_BT_KILL_MSK)

/* # tx attempts for first frame in aggregation */
#define AGG_TX_STATE_TRY_CNT_POS 12
#define AGG_TX_STATE_TRY_CNT_MSK 0xf000

/* Command ID and sequence number of Tx command for this frame */
#define AGG_TX_STATE_SEQ_NUM_POS 16
#define AGG_TX_STATE_SEQ_NUM_MSK 0xffff0000

/*
 * REPLY_TX = 0x1c (response)
 *
 * This response may be in one of two slightly different formats, indicated
 * by the frame_count field:
 *
 * 1)  No aggregation (frame_count == 1).  This reports Tx results for
 *     a single frame.  Multiple attempts, at various bit rates, may have
 *     been made for this frame.
 *
 * 2)  Aggregation (frame_count > 1).  This reports Tx results for
 *     2 or more frames that used block-acknowledge.  All frames were
 *     transmitted at same rate.  Rate scaling may have been used if first
 *     frame in this new agg block failed in previous agg block(s).
 *
 *     Note that, for aggregation, ACK (block-ack) status is not delivered here;
 *     block-ack has not been received by the time the 4965 records this status.
 *     This status relates to reasons the tx might have been blocked or aborted
 *     within the sending station (this 4965), rather than whether it was
 *     received successfully by the destination station.
 */
struct agg_tx_status {
	__le16 status;
	__le16 sequence;
} __attribute__ ((packed));

struct iwl4965_tx_resp {
	u8 frame_count;		/* 1 no aggregation, >1 aggregation */
	u8 bt_kill_count;	/* # blocked by bluetooth (unused for agg) */
	u8 failure_rts;		/* # failures due to unsuccessful RTS */
	u8 failure_frame;	/* # failures due to no ACK (unused for agg) */

	/* For non-agg:  Rate at which frame was successful.
	 * For agg:  Rate at which all frames were transmitted. */
	__le32 rate_n_flags;	/* RATE_MCS_*  */

	/* For non-agg:  RTS + CTS + frame tx attempts time + ACK.
	 * For agg:  RTS + CTS + aggregation tx time + block-ack time. */
	__le16 wireless_media_time;	/* uSecs */

	__le16 reserved;
	__le32 pa_power1;	/* RF power amplifier measurement (not used) */
	__le32 pa_power2;

	/*
	 * For non-agg:  frame status TX_STATUS_*
	 * For agg:  status of 1st frame, AGG_TX_STATE_*; other frame status
	 *           fields follow this one, up to frame_count.
	 *           Bit fields:
	 *           11- 0:  AGG_TX_STATE_* status code
	 *           15-12:  Retry count for 1st frame in aggregation (retries
	 *                   occur if tx failed for this frame when it was a
	 *                   member of a previous aggregation block).  If rate
	 *                   scaling is used, retry count indicates the rate
	 *                   table entry used for all frames in the new agg.
	 *           31-16:  Sequence # for this frame's Tx cmd (not SSN!)
	 */
	union {
		__le32 status;
		struct agg_tx_status agg_status[0]; /* for each agg frame */
	} u;
} __attribute__ ((packed));

/*
 * definitions for initial rate index field
 * bits [3:0] initial rate index
 * bits [6:4] rate table color, used for the initial rate
 * bit-7 invalid rate indication
 *   i.e. rate was not chosen from rate table
 *   or rate table color was changed during frame retries
 * refer tlc rate info
 */

#define IWL50_TX_RES_INIT_RATE_INDEX_POS	0
#define IWL50_TX_RES_INIT_RATE_INDEX_MSK	0x0f
#define IWL50_TX_RES_RATE_TABLE_COLOR_POS	4
#define IWL50_TX_RES_RATE_TABLE_COLOR_MSK	0x70
#define IWL50_TX_RES_INV_RATE_INDEX_MSK	0x80

/* refer to ra_tid */
#define IWL50_TX_RES_TID_POS	0
#define IWL50_TX_RES_TID_MSK	0x0f
#define IWL50_TX_RES_RA_POS	4
#define IWL50_TX_RES_RA_MSK	0xf0

struct iwl5000_tx_resp {
	u8 frame_count;		/* 1 no aggregation, >1 aggregation */
	u8 bt_kill_count;	/* # blocked by bluetooth (unused for agg) */
	u8 failure_rts;		/* # failures due to unsuccessful RTS */
	u8 failure_frame;	/* # failures due to no ACK (unused for agg) */

	/* For non-agg:  Rate at which frame was successful.
	 * For agg:  Rate at which all frames were transmitted. */
	__le32 rate_n_flags;	/* RATE_MCS_*  */

	/* For non-agg:  RTS + CTS + frame tx attempts time + ACK.
	 * For agg:  RTS + CTS + aggregation tx time + block-ack time. */
	__le16 wireless_media_time;	/* uSecs */

	u8 pa_status;		/* RF power amplifier measurement (not used) */
	u8 pa_integ_res_a[3];
	u8 pa_integ_res_b[3];
	u8 pa_integ_res_C[3];

	__le32 tfd_info;
	__le16 seq_ctl;
	__le16 byte_cnt;
	u8 tlc_info;
	u8 ra_tid;		/* tid (0:3), sta_id (4:7) */
	__le16 frame_ctrl;
	/*
	 * For non-agg:  frame status TX_STATUS_*
	 * For agg:  status of 1st frame, AGG_TX_STATE_*; other frame status
	 *           fields follow this one, up to frame_count.
	 *           Bit fields:
	 *           11- 0:  AGG_TX_STATE_* status code
	 *           15-12:  Retry count for 1st frame in aggregation (retries
	 *                   occur if tx failed for this frame when it was a
	 *                   member of a previous aggregation block).  If rate
	 *                   scaling is used, retry count indicates the rate
	 *                   table entry used for all frames in the new agg.
	 *           31-16:  Sequence # for this frame's Tx cmd (not SSN!)
	 */
	struct agg_tx_status status;	/* TX status (in aggregation -
					 * status of 1st frame) */
} __attribute__ ((packed));
/*
 * REPLY_COMPRESSED_BA = 0xc5 (response only, not a command)
 *
 * Reports Block-Acknowledge from recipient station
 */
struct iwl_compressed_ba_resp {
	__le32 sta_addr_lo32;
	__le16 sta_addr_hi16;
	__le16 reserved;

	/* Index of recipient (BA-sending) station in uCode's station table */
	u8 sta_id;
	u8 tid;
	__le16 seq_ctl;
	__le64 bitmap;
	__le16 scd_flow;
	__le16 scd_ssn;
} __attribute__ ((packed));

/*
 * REPLY_TX_PWR_TABLE_CMD = 0x97 (command, has simple generic response)
 *
 * See details under "TXPOWER" in iwl-4965-hw.h.
 */

struct iwl3945_txpowertable_cmd {
	u8 band;		/* 0: 5 GHz, 1: 2.4 GHz */
	u8 reserved;
	__le16 channel;
	struct iwl3945_power_per_rate power[IWL_MAX_RATES];
} __attribute__ ((packed));

struct iwl4965_txpowertable_cmd {
	u8 band;		/* 0: 5 GHz, 1: 2.4 GHz */
	u8 reserved;
	__le16 channel;
	struct iwl4965_tx_power_db tx_power;
} __attribute__ ((packed));


/**
 * struct iwl3945_rate_scaling_cmd - Rate Scaling Command & Response
 *
 * REPLY_RATE_SCALE = 0x47 (command, has simple generic response)
 *
 * NOTE: The table of rates passed to the uCode via the
 * RATE_SCALE command sets up the corresponding order of
 * rates used for all related commands, including rate
 * masks, etc.
 *
 * For example, if you set 9MB (PLCP 0x0f) as the first
 * rate in the rate table, the bit mask for that rate
 * when passed through ofdm_basic_rates on the REPLY_RXON
 * command would be bit 0 (1 << 0)
 */
struct iwl3945_rate_scaling_info {
	__le16 rate_n_flags;
	u8 try_cnt;
	u8 next_rate_index;
} __attribute__ ((packed));

struct iwl3945_rate_scaling_cmd {
	u8 table_id;
	u8 reserved[3];
	struct iwl3945_rate_scaling_info table[IWL_MAX_RATES];
} __attribute__ ((packed));


/*RS_NEW_API: only TLC_RTS remains and moved to bit 0 */
#define  LINK_QUAL_FLAGS_SET_STA_TLC_RTS_MSK	(1 << 0)

/* # of EDCA prioritized tx fifos */
#define  LINK_QUAL_AC_NUM AC_NUM

/* # entries in rate scale table to support Tx retries */
#define  LINK_QUAL_MAX_RETRY_NUM 16

/* Tx antenna selection values */
#define  LINK_QUAL_ANT_A_MSK (1 << 0)
#define  LINK_QUAL_ANT_B_MSK (1 << 1)
#define  LINK_QUAL_ANT_MSK   (LINK_QUAL_ANT_A_MSK|LINK_QUAL_ANT_B_MSK)


/**
 * struct iwl_link_qual_general_params
 *
 * Used in REPLY_TX_LINK_QUALITY_CMD
 */
struct iwl_link_qual_general_params {
	u8 flags;

	/* No entries at or above this (driver chosen) index contain MIMO */
	u8 mimo_delimiter;

	/* Best single antenna to use for single stream (legacy, SISO). */
	u8 single_stream_ant_msk;	/* LINK_QUAL_ANT_* */

	/* Best antennas to use for MIMO (unused for 4965, assumes both). */
	u8 dual_stream_ant_msk;		/* LINK_QUAL_ANT_* */

	/*
	 * If driver needs to use different initial rates for different
	 * EDCA QOS access categories (as implemented by tx fifos 0-3),
	 * this table will set that up, by indicating the indexes in the
	 * rs_table[LINK_QUAL_MAX_RETRY_NUM] rate table at which to start.
	 * Otherwise, driver should set all entries to 0.
	 *
	 * Entry usage:
	 * 0 = Background, 1 = Best Effort (normal), 2 = Video, 3 = Voice
	 * TX FIFOs above 3 use same value (typically 0) as TX FIFO 3.
	 */
	u8 start_rate_index[LINK_QUAL_AC_NUM];
} __attribute__ ((packed));

#define LINK_QUAL_AGG_TIME_LIMIT_DEF	(4000) /* 4 milliseconds */
#define LINK_QUAL_AGG_TIME_LIMIT_MAX	(65535)
#define LINK_QUAL_AGG_TIME_LIMIT_MIN	(0)

#define LINK_QUAL_AGG_DISABLE_START_DEF	(3)
#define LINK_QUAL_AGG_DISABLE_START_MAX	(255)
#define LINK_QUAL_AGG_DISABLE_START_MIN	(0)

#define LINK_QUAL_AGG_FRAME_LIMIT_DEF	(31)
#define LINK_QUAL_AGG_FRAME_LIMIT_MAX	(63)
#define LINK_QUAL_AGG_FRAME_LIMIT_MIN	(0)

/**
 * struct iwl_link_qual_agg_params
 *
 * Used in REPLY_TX_LINK_QUALITY_CMD
 */
struct iwl_link_qual_agg_params {

	/* Maximum number of uSec in aggregation.
	 * Driver should set this to 4000 (4 milliseconds). */
	__le16 agg_time_limit;

	/*
	 * Number of Tx retries allowed for a frame, before that frame will
	 * no longer be considered for the start of an aggregation sequence
	 * (scheduler will then try to tx it as single frame).
	 * Driver should set this to 3.
	 */
	u8 agg_dis_start_th;

	/*
	 * Maximum number of frames in aggregation.
	 * 0 = no limit (default).  1 = no aggregation.
	 * Other values = max # frames in aggregation.
	 */
	u8 agg_frame_cnt_limit;

	__le32 reserved;
} __attribute__ ((packed));

/*
 * REPLY_TX_LINK_QUALITY_CMD = 0x4e (command, has simple generic response)
 *
 * For 4965 only; 3945 uses REPLY_RATE_SCALE.
 *
 * Each station in the 4965's internal station table has its own table of 16
 * Tx rates and modulation modes (e.g. legacy/SISO/MIMO) for retrying Tx when
 * an ACK is not received.  This command replaces the entire table for
 * one station.
 *
 * NOTE:  Station must already be in 4965's station table.  Use REPLY_ADD_STA.
 *
 * The rate scaling procedures described below work well.  Of course, other
 * procedures are possible, and may work better for particular environments.
 *
 *
 * FILLING THE RATE TABLE
 *
 * Given a particular initial rate and mode, as determined by the rate
 * scaling algorithm described below, the Linux driver uses the following
 * formula to fill the rs_table[LINK_QUAL_MAX_RETRY_NUM] rate table in the
 * Link Quality command:
 *
 *
 * 1)  If using High-throughput (HT) (SISO or MIMO) initial rate:
 *     a) Use this same initial rate for first 3 entries.
 *     b) Find next lower available rate using same mode (SISO or MIMO),
 *        use for next 3 entries.  If no lower rate available, switch to
 *        legacy mode (no HT40 channel, no MIMO, no short guard interval).
 *     c) If using MIMO, set command's mimo_delimiter to number of entries
 *        using MIMO (3 or 6).
 *     d) After trying 2 HT rates, switch to legacy mode (no HT40 channel,
 *        no MIMO, no short guard interval), at the next lower bit rate
 *        (e.g. if second HT bit rate was 54, try 48 legacy), and follow
 *        legacy procedure for remaining table entries.
 *
 * 2)  If using legacy initial rate:
 *     a) Use the initial rate for only one entry.
 *     b) For each following entry, reduce the rate to next lower available
 *        rate, until reaching the lowest available rate.
 *     c) When reducing rate, also switch antenna selection.
 *     d) Once lowest available rate is reached, repeat this rate until
 *        rate table is filled (16 entries), switching antenna each entry.
 *
 *
 * ACCUMULATING HISTORY
 *
 * The rate scaling algorithm for 4965, as implemented in Linux driver, uses
 * two sets of frame Tx success history:  One for the current/active modulation
 * mode, and one for a speculative/search mode that is being attempted.  If the
 * speculative mode turns out to be more effective (i.e. actual transfer
 * rate is better), then the driver continues to use the speculative mode
 * as the new current active mode.
 *
 * Each history set contains, separately for each possible rate, data for a
 * sliding window of the 62 most recent tx attempts at that rate.  The data
 * includes a shifting bitmap of success(1)/failure(0), and sums of successful
 * and attempted frames, from which the driver can additionally calculate a
 * success ratio (success / attempted) and number of failures
 * (attempted - success), and control the size of the window (attempted).
 * The driver uses the bit map to remove successes from the success sum, as
 * the oldest tx attempts fall out of the window.
 *
 * When the 4965 makes multiple tx attempts for a given frame, each attempt
 * might be at a different rate, and have different modulation characteristics
 * (e.g. antenna, fat channel, short guard interval), as set up in the rate
 * scaling table in the Link Quality command.  The driver must determine
 * which rate table entry was used for each tx attempt, to determine which
 * rate-specific history to update, and record only those attempts that
 * match the modulation characteristics of the history set.
 *
 * When using block-ack (aggregation), all frames are transmitted at the same
 * rate, since there is no per-attempt acknowledgment from the destination
 * station.  The Tx response struct iwl_tx_resp indicates the Tx rate in
 * rate_n_flags field.  After receiving a block-ack, the driver can update
 * history for the entire block all at once.
 *
 *
 * FINDING BEST STARTING RATE:
 *
 * When working with a selected initial modulation mode (see below), the
 * driver attempts to find a best initial rate.  The initial rate is the
 * first entry in the Link Quality command's rate table.
 *
 * 1)  Calculate actual throughput (success ratio * expected throughput, see
 *     table below) for current initial rate.  Do this only if enough frames
 *     have been attempted to make the value meaningful:  at least 6 failed
 *     tx attempts, or at least 8 successes.  If not enough, don't try rate
 *     scaling yet.
 *
 * 2)  Find available rates adjacent to current initial rate.  Available means:
 *     a)  supported by hardware &&
 *     b)  supported by association &&
 *     c)  within any constraints selected by user
 *
 * 3)  Gather measured throughputs for adjacent rates.  These might not have
 *     enough history to calculate a throughput.  That's okay, we might try
 *     using one of them anyway!
 *
 * 4)  Try decreasing rate if, for current rate:
 *     a)  success ratio is < 15% ||
 *     b)  lower adjacent rate has better measured throughput ||
 *     c)  higher adjacent rate has worse throughput, and lower is unmeasured
 *
 *     As a sanity check, if decrease was determined above, leave rate
 *     unchanged if:
 *     a)  lower rate unavailable
 *     b)  success ratio at current rate > 85% (very good)
 *     c)  current measured throughput is better than expected throughput
 *         of lower rate (under perfect 100% tx conditions, see table below)
 *
 * 5)  Try increasing rate if, for current rate:
 *     a)  success ratio is < 15% ||
 *     b)  both adjacent rates' throughputs are unmeasured (try it!) ||
 *     b)  higher adjacent rate has better measured throughput ||
 *     c)  lower adjacent rate has worse throughput, and higher is unmeasured
 *
 *     As a sanity check, if increase was determined above, leave rate
 *     unchanged if:
 *     a)  success ratio at current rate < 70%.  This is not particularly
 *         good performance; higher rate is sure to have poorer success.
 *
 * 6)  Re-evaluate the rate after each tx frame.  If working with block-
 *     acknowledge, history and statistics may be calculated for the entire
 *     block (including prior history that fits within the history windows),
 *     before re-evaluation.
 *
 * FINDING BEST STARTING MODULATION MODE:
 *
 * After working with a modulation mode for a "while" (and doing rate scaling),
 * the driver searches for a new initial mode in an attempt to improve
 * throughput.  The "while" is measured by numbers of attempted frames:
 *
 * For legacy mode, search for new mode after:
 *   480 successful frames, or 160 failed frames
 * For high-throughput modes (SISO or MIMO), search for new mode after:
 *   4500 successful frames, or 400 failed frames
 *
 * Mode switch possibilities are (3 for each mode):
 *
 * For legacy:
 *   Change antenna, try SISO (if HT association), try MIMO (if HT association)
 * For SISO:
 *   Change antenna, try MIMO, try shortened guard interval (SGI)
 * For MIMO:
 *   Try SISO antenna A, SISO antenna B, try shortened guard interval (SGI)
 *
 * When trying a new mode, use the same bit rate as the old/current mode when
 * trying antenna switches and shortened guard interval.  When switching to
 * SISO from MIMO or legacy, or to MIMO from SISO or legacy, use a rate
 * for which the expected throughput (under perfect conditions) is about the
 * same or slightly better than the actual measured throughput delivered by
 * the old/current mode.
 *
 * Actual throughput can be estimated by multiplying the expected throughput
 * by the success ratio (successful / attempted tx frames).  Frame size is
 * not considered in this calculation; it assumes that frame size will average
 * out to be fairly consistent over several samples.  The following are
 * metric values for expected throughput assuming 100% success ratio.
 * Only G band has support for CCK rates:
 *
 *           RATE:  1    2    5   11    6   9   12   18   24   36   48   54   60
 *
 *              G:  7   13   35   58   40  57   72   98  121  154  177  186  186
 *              A:  0    0    0    0   40  57   72   98  121  154  177  186  186
 *     SISO 20MHz:  0    0    0    0   42  42   76  102  124  159  183  193  202
 * SGI SISO 20MHz:  0    0    0    0   46  46   82  110  132  168  192  202  211
 *     MIMO 20MHz:  0    0    0    0   74  74  123  155  179  214  236  244  251
 * SGI MIMO 20MHz:  0    0    0    0   81  81  131  164  188  222  243  251  257
 *     SISO 40MHz:  0    0    0    0   77  77  127  160  184  220  242  250  257
 * SGI SISO 40MHz:  0    0    0    0   83  83  135  169  193  229  250  257  264
 *     MIMO 40MHz:  0    0    0    0  123 123  182  214  235  264  279  285  289
 * SGI MIMO 40MHz:  0    0    0    0  131 131  191  222  242  270  284  289  293
 *
 * After the new mode has been tried for a short while (minimum of 6 failed
 * frames or 8 successful frames), compare success ratio and actual throughput
 * estimate of the new mode with the old.  If either is better with the new
 * mode, continue to use the new mode.
 *
 * Continue comparing modes until all 3 possibilities have been tried.
 * If moving from legacy to HT, try all 3 possibilities from the new HT
 * mode.  After trying all 3, a best mode is found.  Continue to use this mode
 * for the longer "while" described above (e.g. 480 successful frames for
 * legacy), and then repeat the search process.
 *
 */
struct iwl_link_quality_cmd {

	/* Index of destination/recipient station in uCode's station table */
	u8 sta_id;
	u8 reserved1;
	__le16 control;		/* not used */
	struct iwl_link_qual_general_params general_params;
	struct iwl_link_qual_agg_params agg_params;

	/*
	 * Rate info; when using rate-scaling, Tx command's initial_rate_index
	 * specifies 1st Tx rate attempted, via index into this table.
	 * 4965 works its way through table when retrying Tx.
	 */
	struct {
		__le32 rate_n_flags;	/* RATE_MCS_*, IWL_RATE_* */
	} rs_table[LINK_QUAL_MAX_RETRY_NUM];
	__le32 reserved2;
} __attribute__ ((packed));

/*
 * REPLY_BT_CONFIG = 0x9b (command, has simple generic response)
 *
 * 3945 and 4965 support hardware handshake with Bluetooth device on
 * same platform.  Bluetooth device alerts wireless device when it will Tx;
 * wireless device can delay or kill its own Tx to accommodate.
 */
struct iwl_bt_cmd {
	u8 flags;
	u8 lead_time;
	u8 max_kill;
	u8 reserved;
	__le32 kill_ack_mask;
	__le32 kill_cts_mask;
} __attribute__ ((packed));

/******************************************************************************
 * (6)
 * Spectrum Management (802.11h) Commands, Responses, Notifications:
 *
 *****************************************************************************/

/*
 * Spectrum Management
 */
#define MEASUREMENT_FILTER_FLAG (RXON_FILTER_PROMISC_MSK         | \
				 RXON_FILTER_CTL2HOST_MSK        | \
				 RXON_FILTER_ACCEPT_GRP_MSK      | \
				 RXON_FILTER_DIS_DECRYPT_MSK     | \
				 RXON_FILTER_DIS_GRP_DECRYPT_MSK | \
				 RXON_FILTER_ASSOC_MSK           | \
				 RXON_FILTER_BCON_AWARE_MSK)

struct iwl_measure_channel {
	__le32 duration;	/* measurement duration in extended beacon
				 * format */
	u8 channel;		/* channel to measure */
	u8 type;		/* see enum iwl_measure_type */
	__le16 reserved;
} __attribute__ ((packed));

/*
 * REPLY_SPECTRUM_MEASUREMENT_CMD = 0x74 (command)
 */
struct iwl_spectrum_cmd {
	__le16 len;		/* number of bytes starting from token */
	u8 token;		/* token id */
	u8 id;			/* measurement id -- 0 or 1 */
	u8 origin;		/* 0 = TGh, 1 = other, 2 = TGk */
	u8 periodic;		/* 1 = periodic */
	__le16 path_loss_timeout;
	__le32 start_time;	/* start time in extended beacon format */
	__le32 reserved2;
	__le32 flags;		/* rxon flags */
	__le32 filter_flags;	/* rxon filter flags */
	__le16 channel_count;	/* minimum 1, maximum 10 */
	__le16 reserved3;
	struct iwl_measure_channel channels[10];
} __attribute__ ((packed));

/*
 * REPLY_SPECTRUM_MEASUREMENT_CMD = 0x74 (response)
 */
struct iwl_spectrum_resp {
	u8 token;
	u8 id;			/* id of the prior command replaced, or 0xff */
	__le16 status;		/* 0 - command will be handled
				 * 1 - cannot handle (conflicts with another
				 *     measurement) */
} __attribute__ ((packed));

enum iwl_measurement_state {
	IWL_MEASUREMENT_START = 0,
	IWL_MEASUREMENT_STOP = 1,
};

enum iwl_measurement_status {
	IWL_MEASUREMENT_OK = 0,
	IWL_MEASUREMENT_CONCURRENT = 1,
	IWL_MEASUREMENT_CSA_CONFLICT = 2,
	IWL_MEASUREMENT_TGH_CONFLICT = 3,
	/* 4-5 reserved */
	IWL_MEASUREMENT_STOPPED = 6,
	IWL_MEASUREMENT_TIMEOUT = 7,
	IWL_MEASUREMENT_PERIODIC_FAILED = 8,
};

#define NUM_ELEMENTS_IN_HISTOGRAM 8

struct iwl_measurement_histogram {
	__le32 ofdm[NUM_ELEMENTS_IN_HISTOGRAM];	/* in 0.8usec counts */
	__le32 cck[NUM_ELEMENTS_IN_HISTOGRAM];	/* in 1usec counts */
} __attribute__ ((packed));

/* clear channel availability counters */
struct iwl_measurement_cca_counters {
	__le32 ofdm;
	__le32 cck;
} __attribute__ ((packed));

enum iwl_measure_type {
	IWL_MEASURE_BASIC = (1 << 0),
	IWL_MEASURE_CHANNEL_LOAD = (1 << 1),
	IWL_MEASURE_HISTOGRAM_RPI = (1 << 2),
	IWL_MEASURE_HISTOGRAM_NOISE = (1 << 3),
	IWL_MEASURE_FRAME = (1 << 4),
	/* bits 5:6 are reserved */
	IWL_MEASURE_IDLE = (1 << 7),
};

/*
 * SPECTRUM_MEASURE_NOTIFICATION = 0x75 (notification only, not a command)
 */
struct iwl_spectrum_notification {
	u8 id;			/* measurement id -- 0 or 1 */
	u8 token;
	u8 channel_index;	/* index in measurement channel list */
	u8 state;		/* 0 - start, 1 - stop */
	__le32 start_time;	/* lower 32-bits of TSF */
	u8 band;		/* 0 - 5.2GHz, 1 - 2.4GHz */
	u8 channel;
	u8 type;		/* see enum iwl_measurement_type */
	u8 reserved1;
	/* NOTE:  cca_ofdm, cca_cck, basic_type, and histogram are only only
	 * valid if applicable for measurement type requested. */
	__le32 cca_ofdm;	/* cca fraction time in 40Mhz clock periods */
	__le32 cca_cck;		/* cca fraction time in 44Mhz clock periods */
	__le32 cca_time;	/* channel load time in usecs */
	u8 basic_type;		/* 0 - bss, 1 - ofdm preamble, 2 -
				 * unidentified */
	u8 reserved2[3];
	struct iwl_measurement_histogram histogram;
	__le32 stop_time;	/* lower 32-bits of TSF */
	__le32 status;		/* see iwl_measurement_status */
} __attribute__ ((packed));

/******************************************************************************
 * (7)
 * Power Management Commands, Responses, Notifications:
 *
 *****************************************************************************/

/**
 * struct iwl_powertable_cmd - Power Table Command
 * @flags: See below:
 *
 * POWER_TABLE_CMD = 0x77 (command, has simple generic response)
 *
 * PM allow:
 *   bit 0 - '0' Driver not allow power management
 *           '1' Driver allow PM (use rest of parameters)
 *
 * uCode send sleep notifications:
 *   bit 1 - '0' Don't send sleep notification
 *           '1' send sleep notification (SEND_PM_NOTIFICATION)
 *
 * Sleep over DTIM
 *   bit 2 - '0' PM have to walk up every DTIM
 *           '1' PM could sleep over DTIM till listen Interval.
 *
 * PCI power managed
 *   bit 3 - '0' (PCI_CFG_LINK_CTRL & 0x1)
 *           '1' !(PCI_CFG_LINK_CTRL & 0x1)
 *
 * Fast PD
 *   bit 4 - '1' Put radio to sleep when receiving frame for others
 *
 * Force sleep Modes
 *   bit 31/30- '00' use both mac/xtal sleeps
 *              '01' force Mac sleep
 *              '10' force xtal sleep
 *              '11' Illegal set
 *
 * NOTE: if sleep_interval[SLEEP_INTRVL_TABLE_SIZE-1] > DTIM period then
 * ucode assume sleep over DTIM is allowed and we don't need to wake up
 * for every DTIM.
 */
#define IWL_POWER_VEC_SIZE 5

#define IWL_POWER_DRIVER_ALLOW_SLEEP_MSK	cpu_to_le16(BIT(0))
#define IWL_POWER_SLEEP_OVER_DTIM_MSK		cpu_to_le16(BIT(2))
#define IWL_POWER_PCI_PM_MSK			cpu_to_le16(BIT(3))
#define IWL_POWER_FAST_PD			cpu_to_le16(BIT(4))

struct iwl3945_powertable_cmd {
	__le16 flags;
	u8 reserved[2];
	__le32 rx_data_timeout;
	__le32 tx_data_timeout;
	__le32 sleep_interval[IWL_POWER_VEC_SIZE];
} __attribute__ ((packed));

struct iwl_powertable_cmd {
	__le16 flags;
	u8 keep_alive_seconds;		/* 3945 reserved */
	u8 debug_flags;			/* 3945 reserved */
	__le32 rx_data_timeout;
	__le32 tx_data_timeout;
	__le32 sleep_interval[IWL_POWER_VEC_SIZE];
	__le32 keep_alive_beacons;
} __attribute__ ((packed));

/*
 * PM_SLEEP_NOTIFICATION = 0x7A (notification only, not a command)
 * 3945 and 4965 identical.
 */
struct iwl_sleep_notification {
	u8 pm_sleep_mode;
	u8 pm_wakeup_src;
	__le16 reserved;
	__le32 sleep_time;
	__le32 tsf_low;
	__le32 bcon_timer;
} __attribute__ ((packed));

/* Sleep states.  3945 and 4965 identical. */
enum {
	IWL_PM_NO_SLEEP = 0,
	IWL_PM_SLP_MAC = 1,
	IWL_PM_SLP_FULL_MAC_UNASSOCIATE = 2,
	IWL_PM_SLP_FULL_MAC_CARD_STATE = 3,
	IWL_PM_SLP_PHY = 4,
	IWL_PM_SLP_REPENT = 5,
	IWL_PM_WAKEUP_BY_TIMER = 6,
	IWL_PM_WAKEUP_BY_DRIVER = 7,
	IWL_PM_WAKEUP_BY_RFKILL = 8,
	/* 3 reserved */
	IWL_PM_NUM_OF_MODES = 12,
};

/*
 * REPLY_CARD_STATE_CMD = 0xa0 (command, has simple generic response)
 */
#define CARD_STATE_CMD_DISABLE 0x00	/* Put card to sleep */
#define CARD_STATE_CMD_ENABLE  0x01	/* Wake up card */
#define CARD_STATE_CMD_HALT    0x02	/* Power down permanently */
struct iwl_card_state_cmd {
	__le32 status;		/* CARD_STATE_CMD_* request new power state */
} __attribute__ ((packed));

/*
 * CARD_STATE_NOTIFICATION = 0xa1 (notification only, not a command)
 */
struct iwl_card_state_notif {
	__le32 flags;
} __attribute__ ((packed));

#define HW_CARD_DISABLED   0x01
#define SW_CARD_DISABLED   0x02
#define RF_CARD_DISABLED   0x04
#define RXON_CARD_DISABLED 0x10

struct iwl_ct_kill_config {
	__le32   reserved;
	__le32   critical_temperature_M;
	__le32   critical_temperature_R;
}  __attribute__ ((packed));

/* 1000, and 6x00 */
struct iwl_ct_kill_throttling_config {
	__le32   critical_temperature_exit;
	__le32   reserved;
	__le32   critical_temperature_enter;
}  __attribute__ ((packed));

/******************************************************************************
 * (8)
 * Scan Commands, Responses, Notifications:
 *
 *****************************************************************************/

#define SCAN_CHANNEL_TYPE_PASSIVE cpu_to_le32(0)
#define SCAN_CHANNEL_TYPE_ACTIVE  cpu_to_le32(1)

/**
 * struct iwl_scan_channel - entry in REPLY_SCAN_CMD channel table
 *
 * One for each channel in the scan list.
 * Each channel can independently select:
 * 1)  SSID for directed active scans
 * 2)  Txpower setting (for rate specified within Tx command)
 * 3)  How long to stay on-channel (behavior may be modified by quiet_time,
 *     quiet_plcp_th, good_CRC_th)
 *
 * To avoid uCode errors, make sure the following are true (see comments
 * under struct iwl_scan_cmd about max_out_time and quiet_time):
 * 1)  If using passive_dwell (i.e. passive_dwell != 0):
 *     active_dwell <= passive_dwell (< max_out_time if max_out_time != 0)
 * 2)  quiet_time <= active_dwell
 * 3)  If restricting off-channel time (i.e. max_out_time !=0):
 *     passive_dwell < max_out_time
 *     active_dwell < max_out_time
 */

/* FIXME: rename to AP1, remove tpc */
struct iwl3945_scan_channel {
	/*
	 * type is defined as:
	 * 0:0 1 = active, 0 = passive
	 * 1:4 SSID direct bit map; if a bit is set, then corresponding
	 *     SSID IE is transmitted in probe request.
	 * 5:7 reserved
	 */
	u8 type;
	u8 channel;	/* band is selected by iwl3945_scan_cmd "flags" field */
	struct iwl3945_tx_power tpc;
	__le16 active_dwell;	/* in 1024-uSec TU (time units), typ 5-50 */
	__le16 passive_dwell;	/* in 1024-uSec TU (time units), typ 20-500 */
} __attribute__ ((packed));

/* set number of direct probes u8 type */
#define IWL39_SCAN_PROBE_MASK(n) ((BIT(n) | (BIT(n) - BIT(1))))

struct iwl_scan_channel {
	/*
	 * type is defined as:
	 * 0:0 1 = active, 0 = passive
	 * 1:20 SSID direct bit map; if a bit is set, then corresponding
	 *     SSID IE is transmitted in probe request.
	 * 21:31 reserved
	 */
	__le32 type;
	__le16 channel;	/* band is selected by iwl_scan_cmd "flags" field */
	u8 tx_gain;		/* gain for analog radio */
	u8 dsp_atten;		/* gain for DSP */
	__le16 active_dwell;	/* in 1024-uSec TU (time units), typ 5-50 */
	__le16 passive_dwell;	/* in 1024-uSec TU (time units), typ 20-500 */
} __attribute__ ((packed));

/* set number of direct probes __le32 type */
#define IWL_SCAN_PROBE_MASK(n) 	cpu_to_le32((BIT(n) | (BIT(n) - BIT(1))))

/**
 * struct iwl_ssid_ie - directed scan network information element
 *
 * Up to 4 of these may appear in REPLY_SCAN_CMD, selected by "type" field
 * in struct iwl_scan_channel; each channel may select different ssids from
 * among the 4 entries.  SSID IEs get transmitted in reverse order of entry.
 */
struct iwl_ssid_ie {
	u8 id;
	u8 len;
	u8 ssid[32];
} __attribute__ ((packed));

#define PROBE_OPTION_MAX_3945		4
#define PROBE_OPTION_MAX		20
#define TX_CMD_LIFE_TIME_INFINITE	cpu_to_le32(0xFFFFFFFF)
#define IWL_GOOD_CRC_TH			cpu_to_le16(1)
#define IWL_MAX_SCAN_SIZE 1024
#define IWL_MAX_PROBE_REQUEST		200

/*
 * REPLY_SCAN_CMD = 0x80 (command)
 *
 * The hardware scan command is very powerful; the driver can set it up to
 * maintain (relatively) normal network traffic while doing a scan in the
 * background.  The max_out_time and suspend_time control the ratio of how
 * long the device stays on an associated network channel ("service channel")
 * vs. how long it's away from the service channel, i.e. tuned to other channels
 * for scanning.
 *
 * max_out_time is the max time off-channel (in usec), and suspend_time
 * is how long (in "extended beacon" format) that the scan is "suspended"
 * after returning to the service channel.  That is, suspend_time is the
 * time that we stay on the service channel, doing normal work, between
 * scan segments.  The driver may set these parameters differently to support
 * scanning when associated vs. not associated, and light vs. heavy traffic
 * loads when associated.
 *
 * After receiving this command, the device's scan engine does the following;
 *
 * 1)  Sends SCAN_START notification to driver
 * 2)  Checks to see if it has time to do scan for one channel
 * 3)  Sends NULL packet, with power-save (PS) bit set to 1,
 *     to tell AP that we're going off-channel
 * 4)  Tunes to first channel in scan list, does active or passive scan
 * 5)  Sends SCAN_RESULT notification to driver
 * 6)  Checks to see if it has time to do scan on *next* channel in list
 * 7)  Repeats 4-6 until it no longer has time to scan the next channel
 *     before max_out_time expires
 * 8)  Returns to service channel
 * 9)  Sends NULL packet with PS=0 to tell AP that we're back
 * 10) Stays on service channel until suspend_time expires
 * 11) Repeats entire process 2-10 until list is complete
 * 12) Sends SCAN_COMPLETE notification
 *
 * For fast, efficient scans, the scan command also has support for staying on
 * a channel for just a short time, if doing active scanning and getting no
 * responses to the transmitted probe request.  This time is controlled by
 * quiet_time, and the number of received packets below which a channel is
 * considered "quiet" is controlled by quiet_plcp_threshold.
 *
 * For active scanning on channels that have regulatory restrictions against
 * blindly transmitting, the scan can listen before transmitting, to make sure
 * that there is already legitimate activity on the channel.  If enough
 * packets are cleanly received on the channel (controlled by good_CRC_th,
 * typical value 1), the scan engine starts transmitting probe requests.
 *
 * Driver must use separate scan commands for 2.4 vs. 5 GHz bands.
 *
 * To avoid uCode errors, see timing restrictions described under
 * struct iwl_scan_channel.
 */

struct iwl3945_scan_cmd {
	__le16 len;
	u8 reserved0;
	u8 channel_count;	/* # channels in channel list */
	__le16 quiet_time;	/* dwell only this # millisecs on quiet channel
				 * (only for active scan) */
	__le16 quiet_plcp_th;	/* quiet chnl is < this # pkts (typ. 1) */
	__le16 good_CRC_th;	/* passive -> active promotion threshold */
	__le16 reserved1;
	__le32 max_out_time;	/* max usec to be away from associated (service)
				 * channel */
	__le32 suspend_time;	/* pause scan this long (in "extended beacon
				 * format") when returning to service channel:
				 * 3945; 31:24 # beacons, 19:0 additional usec,
				 * 4965; 31:22 # beacons, 21:0 additional usec.
				 */
	__le32 flags;		/* RXON_FLG_* */
	__le32 filter_flags;	/* RXON_FILTER_* */

	/* For active scans (set to all-0s for passive scans).
	 * Does not include payload.  Must specify Tx rate; no rate scaling. */
	struct iwl3945_tx_cmd tx_cmd;

	/* For directed active scans (set to all-0s otherwise) */
	struct iwl_ssid_ie direct_scan[PROBE_OPTION_MAX_3945];

	/*
	 * Probe request frame, followed by channel list.
	 *
	 * Size of probe request frame is specified by byte count in tx_cmd.
	 * Channel list follows immediately after probe request frame.
	 * Number of channels in list is specified by channel_count.
	 * Each channel in list is of type:
	 *
	 * struct iwl3945_scan_channel channels[0];
	 *
	 * NOTE:  Only one band of channels can be scanned per pass.  You
	 * must not mix 2.4GHz channels and 5.2GHz channels, and you must wait
	 * for one scan to complete (i.e. receive SCAN_COMPLETE_NOTIFICATION)
	 * before requesting another scan.
	 */
	u8 data[0];
} __attribute__ ((packed));

struct iwl_scan_cmd {
	__le16 len;
	u8 reserved0;
	u8 channel_count;	/* # channels in channel list */
	__le16 quiet_time;	/* dwell only this # millisecs on quiet channel
				 * (only for active scan) */
	__le16 quiet_plcp_th;	/* quiet chnl is < this # pkts (typ. 1) */
	__le16 good_CRC_th;	/* passive -> active promotion threshold */
	__le16 rx_chain;	/* RXON_RX_CHAIN_* */
	__le32 max_out_time;	/* max usec to be away from associated (service)
				 * channel */
	__le32 suspend_time;	/* pause scan this long (in "extended beacon
				 * format") when returning to service chnl:
				 * 3945; 31:24 # beacons, 19:0 additional usec,
				 * 4965; 31:22 # beacons, 21:0 additional usec.
				 */
	__le32 flags;		/* RXON_FLG_* */
	__le32 filter_flags;	/* RXON_FILTER_* */

	/* For active scans (set to all-0s for passive scans).
	 * Does not include payload.  Must specify Tx rate; no rate scaling. */
	struct iwl_tx_cmd tx_cmd;

	/* For directed active scans (set to all-0s otherwise) */
	struct iwl_ssid_ie direct_scan[PROBE_OPTION_MAX];

	/*
	 * Probe request frame, followed by channel list.
	 *
	 * Size of probe request frame is specified by byte count in tx_cmd.
	 * Channel list follows immediately after probe request frame.
	 * Number of channels in list is specified by channel_count.
	 * Each channel in list is of type:
	 *
	 * struct iwl_scan_channel channels[0];
	 *
	 * NOTE:  Only one band of channels can be scanned per pass.  You
	 * must not mix 2.4GHz channels and 5.2GHz channels, and you must wait
	 * for one scan to complete (i.e. receive SCAN_COMPLETE_NOTIFICATION)
	 * before requesting another scan.
	 */
	u8 data[0];
} __attribute__ ((packed));

/* Can abort will notify by complete notification with abort status. */
#define CAN_ABORT_STATUS	cpu_to_le32(0x1)
/* complete notification statuses */
#define ABORT_STATUS            0x2

/*
 * REPLY_SCAN_CMD = 0x80 (response)
 */
struct iwl_scanreq_notification {
	__le32 status;		/* 1: okay, 2: cannot fulfill request */
} __attribute__ ((packed));

/*
 * SCAN_START_NOTIFICATION = 0x82 (notification only, not a command)
 */
struct iwl_scanstart_notification {
	__le32 tsf_low;
	__le32 tsf_high;
	__le32 beacon_timer;
	u8 channel;
	u8 band;
	u8 reserved[2];
	__le32 status;
} __attribute__ ((packed));

#define  SCAN_OWNER_STATUS 0x1;
#define  MEASURE_OWNER_STATUS 0x2;

#define NUMBER_OF_STATISTICS 1	/* first __le32 is good CRC */
/*
 * SCAN_RESULTS_NOTIFICATION = 0x83 (notification only, not a command)
 */
struct iwl_scanresults_notification {
	u8 channel;
	u8 band;
	u8 reserved[2];
	__le32 tsf_low;
	__le32 tsf_high;
	__le32 statistics[NUMBER_OF_STATISTICS];
} __attribute__ ((packed));

/*
 * SCAN_COMPLETE_NOTIFICATION = 0x84 (notification only, not a command)
 */
struct iwl_scancomplete_notification {
	u8 scanned_channels;
	u8 status;
	u8 reserved;
	u8 last_channel;
	__le32 tsf_low;
	__le32 tsf_high;
} __attribute__ ((packed));


/******************************************************************************
 * (9)
 * IBSS/AP Commands and Notifications:
 *
 *****************************************************************************/

/*
 * BEACON_NOTIFICATION = 0x90 (notification only, not a command)
 */

struct iwl3945_beacon_notif {
	struct iwl3945_tx_resp beacon_notify_hdr;
	__le32 low_tsf;
	__le32 high_tsf;
	__le32 ibss_mgr_status;
} __attribute__ ((packed));

struct iwl4965_beacon_notif {
	struct iwl4965_tx_resp beacon_notify_hdr;
	__le32 low_tsf;
	__le32 high_tsf;
	__le32 ibss_mgr_status;
} __attribute__ ((packed));

/*
 * REPLY_TX_BEACON = 0x91 (command, has simple generic response)
 */

struct iwl3945_tx_beacon_cmd {
	struct iwl3945_tx_cmd tx;
	__le16 tim_idx;
	u8 tim_size;
	u8 reserved1;
	struct ieee80211_hdr frame[0];	/* beacon frame */
} __attribute__ ((packed));

struct iwl_tx_beacon_cmd {
	struct iwl_tx_cmd tx;
	__le16 tim_idx;
	u8 tim_size;
	u8 reserved1;
	struct ieee80211_hdr frame[0];	/* beacon frame */
} __attribute__ ((packed));

/******************************************************************************
 * (10)
 * Statistics Commands and Notifications:
 *
 *****************************************************************************/

#define IWL_TEMP_CONVERT 260

#define SUP_RATE_11A_MAX_NUM_CHANNELS  8
#define SUP_RATE_11B_MAX_NUM_CHANNELS  4
#define SUP_RATE_11G_MAX_NUM_CHANNELS  12

/* Used for passing to driver number of successes and failures per rate */
struct rate_histogram {
	union {
		__le32 a[SUP_RATE_11A_MAX_NUM_CHANNELS];
		__le32 b[SUP_RATE_11B_MAX_NUM_CHANNELS];
		__le32 g[SUP_RATE_11G_MAX_NUM_CHANNELS];
	} success;
	union {
		__le32 a[SUP_RATE_11A_MAX_NUM_CHANNELS];
		__le32 b[SUP_RATE_11B_MAX_NUM_CHANNELS];
		__le32 g[SUP_RATE_11G_MAX_NUM_CHANNELS];
	} failed;
} __attribute__ ((packed));

/* statistics command response */

struct iwl39_statistics_rx_phy {
	__le32 ina_cnt;
	__le32 fina_cnt;
	__le32 plcp_err;
	__le32 crc32_err;
	__le32 overrun_err;
	__le32 early_overrun_err;
	__le32 crc32_good;
	__le32 false_alarm_cnt;
	__le32 fina_sync_err_cnt;
	__le32 sfd_timeout;
	__le32 fina_timeout;
	__le32 unresponded_rts;
	__le32 rxe_frame_limit_overrun;
	__le32 sent_ack_cnt;
	__le32 sent_cts_cnt;
} __attribute__ ((packed));

struct iwl39_statistics_rx_non_phy {
	__le32 bogus_cts;	/* CTS received when not expecting CTS */
	__le32 bogus_ack;	/* ACK received when not expecting ACK */
	__le32 non_bssid_frames;	/* number of frames with BSSID that
					 * doesn't belong to the STA BSSID */
	__le32 filtered_frames;	/* count frames that were dumped in the
				 * filtering process */
	__le32 non_channel_beacons;	/* beacons with our bss id but not on
					 * our serving channel */
} __attribute__ ((packed));

struct iwl39_statistics_rx {
	struct iwl39_statistics_rx_phy ofdm;
	struct iwl39_statistics_rx_phy cck;
	struct iwl39_statistics_rx_non_phy general;
} __attribute__ ((packed));

struct iwl39_statistics_tx {
	__le32 preamble_cnt;
	__le32 rx_detected_cnt;
	__le32 bt_prio_defer_cnt;
	__le32 bt_prio_kill_cnt;
	__le32 few_bytes_cnt;
	__le32 cts_timeout;
	__le32 ack_timeout;
	__le32 expected_ack_cnt;
	__le32 actual_ack_cnt;
} __attribute__ ((packed));

struct statistics_dbg {
	__le32 burst_check;
	__le32 burst_count;
	__le32 reserved[4];
} __attribute__ ((packed));

struct iwl39_statistics_div {
	__le32 tx_on_a;
	__le32 tx_on_b;
	__le32 exec_time;
	__le32 probe_time;
} __attribute__ ((packed));

struct iwl39_statistics_general {
	__le32 temperature;
	struct statistics_dbg dbg;
	__le32 sleep_time;
	__le32 slots_out;
	__le32 slots_idle;
	__le32 ttl_timestamp;
	struct iwl39_statistics_div div;
} __attribute__ ((packed));

struct statistics_rx_phy {
	__le32 ina_cnt;
	__le32 fina_cnt;
	__le32 plcp_err;
	__le32 crc32_err;
	__le32 overrun_err;
	__le32 early_overrun_err;
	__le32 crc32_good;
	__le32 false_alarm_cnt;
	__le32 fina_sync_err_cnt;
	__le32 sfd_timeout;
	__le32 fina_timeout;
	__le32 unresponded_rts;
	__le32 rxe_frame_limit_overrun;
	__le32 sent_ack_cnt;
	__le32 sent_cts_cnt;
	__le32 sent_ba_rsp_cnt;
	__le32 dsp_self_kill;
	__le32 mh_format_err;
	__le32 re_acq_main_rssi_sum;
	__le32 reserved3;
} __attribute__ ((packed));

struct statistics_rx_ht_phy {
	__le32 plcp_err;
	__le32 overrun_err;
	__le32 early_overrun_err;
	__le32 crc32_good;
	__le32 crc32_err;
	__le32 mh_format_err;
	__le32 agg_crc32_good;
	__le32 agg_mpdu_cnt;
	__le32 agg_cnt;
	__le32 reserved2;
} __attribute__ ((packed));

#define INTERFERENCE_DATA_AVAILABLE      cpu_to_le32(1)

struct statistics_rx_non_phy {
	__le32 bogus_cts;	/* CTS received when not expecting CTS */
	__le32 bogus_ack;	/* ACK received when not expecting ACK */
	__le32 non_bssid_frames;	/* number of frames with BSSID that
					 * doesn't belong to the STA BSSID */
	__le32 filtered_frames;	/* count frames that were dumped in the
				 * filtering process */
	__le32 non_channel_beacons;	/* beacons with our bss id but not on
					 * our serving channel */
	__le32 channel_beacons;	/* beacons with our bss id and in our
				 * serving channel */
	__le32 num_missed_bcon;	/* number of missed beacons */
	__le32 adc_rx_saturation_time;	/* count in 0.8us units the time the
					 * ADC was in saturation */
	__le32 ina_detection_search_time;/* total time (in 0.8us) searched
					  * for INA */
	__le32 beacon_silence_rssi_a;	/* RSSI silence after beacon frame */
	__le32 beacon_silence_rssi_b;	/* RSSI silence after beacon frame */
	__le32 beacon_silence_rssi_c;	/* RSSI silence after beacon frame */
	__le32 interference_data_flag;	/* flag for interference data
					 * availability. 1 when data is
					 * available. */
	__le32 channel_load;		/* counts RX Enable time in uSec */
	__le32 dsp_false_alarms;	/* DSP false alarm (both OFDM
					 * and CCK) counter */
	__le32 beacon_rssi_a;
	__le32 beacon_rssi_b;
	__le32 beacon_rssi_c;
	__le32 beacon_energy_a;
	__le32 beacon_energy_b;
	__le32 beacon_energy_c;
} __attribute__ ((packed));

struct statistics_rx {
	struct statistics_rx_phy ofdm;
	struct statistics_rx_phy cck;
	struct statistics_rx_non_phy general;
	struct statistics_rx_ht_phy ofdm_ht;
} __attribute__ ((packed));

/**
 * struct statistics_tx_power - current tx power
 *
 * @ant_a: current tx power on chain a in 1/2 dB step
 * @ant_b: current tx power on chain b in 1/2 dB step
 * @ant_c: current tx power on chain c in 1/2 dB step
 */
struct statistics_tx_power {
	u8 ant_a;
	u8 ant_b;
	u8 ant_c;
	u8 reserved;
} __attribute__ ((packed));

struct statistics_tx_non_phy_agg {
	__le32 ba_timeout;
	__le32 ba_reschedule_frames;
	__le32 scd_query_agg_frame_cnt;
	__le32 scd_query_no_agg;
	__le32 scd_query_agg;
	__le32 scd_query_mismatch;
	__le32 frame_not_ready;
	__le32 underrun;
	__le32 bt_prio_kill;
	__le32 rx_ba_rsp_cnt;
} __attribute__ ((packed));

struct statistics_tx {
	__le32 preamble_cnt;
	__le32 rx_detected_cnt;
	__le32 bt_prio_defer_cnt;
	__le32 bt_prio_kill_cnt;
	__le32 few_bytes_cnt;
	__le32 cts_timeout;
	__le32 ack_timeout;
	__le32 expected_ack_cnt;
	__le32 actual_ack_cnt;
	__le32 dump_msdu_cnt;
	__le32 burst_abort_next_frame_mismatch_cnt;
	__le32 burst_abort_missing_next_frame_cnt;
	__le32 cts_timeout_collision;
	__le32 ack_or_ba_timeout_collision;
	struct statistics_tx_non_phy_agg agg;
	struct statistics_tx_power tx_power;
	__le32 reserved1;
} __attribute__ ((packed));


struct statistics_div {
	__le32 tx_on_a;
	__le32 tx_on_b;
	__le32 exec_time;
	__le32 probe_time;
	__le32 reserved1;
	__le32 reserved2;
} __attribute__ ((packed));

struct statistics_general {
	__le32 temperature;
	__le32 temperature_m;
	struct statistics_dbg dbg;
	__le32 sleep_time;
	__le32 slots_out;
	__le32 slots_idle;
	__le32 ttl_timestamp;
	struct statistics_div div;
	__le32 rx_enable_counter;
	__le32 reserved1;
	__le32 reserved2;
	__le32 reserved3;
} __attribute__ ((packed));

/*
 * REPLY_STATISTICS_CMD = 0x9c,
 * 3945 and 4965 identical.
 *
 * This command triggers an immediate response containing uCode statistics.
 * The response is in the same format as STATISTICS_NOTIFICATION 0x9d, below.
 *
 * If the CLEAR_STATS configuration flag is set, uCode will clear its
 * internal copy of the statistics (counters) after issuing the response.
 * This flag does not affect STATISTICS_NOTIFICATIONs after beacons (see below).
 *
 * If the DISABLE_NOTIF configuration flag is set, uCode will not issue
 * STATISTICS_NOTIFICATIONs after received beacons (see below).  This flag
 * does not affect the response to the REPLY_STATISTICS_CMD 0x9c itself.
 */
#define IWL_STATS_CONF_CLEAR_STATS cpu_to_le32(0x1)	/* see above */
#define IWL_STATS_CONF_DISABLE_NOTIF cpu_to_le32(0x2)/* see above */
struct iwl_statistics_cmd {
	__le32 configuration_flags;	/* IWL_STATS_CONF_* */
} __attribute__ ((packed));

/*
 * STATISTICS_NOTIFICATION = 0x9d (notification only, not a command)
 *
 * By default, uCode issues this notification after receiving a beacon
 * while associated.  To disable this behavior, set DISABLE_NOTIF flag in the
 * REPLY_STATISTICS_CMD 0x9c, above.
 *
 * Statistics counters continue to increment beacon after beacon, but are
 * cleared when changing channels or when driver issues REPLY_STATISTICS_CMD
 * 0x9c with CLEAR_STATS bit set (see above).
 *
 * uCode also issues this notification during scans.  uCode clears statistics
 * appropriately so that each notification contains statistics for only the
 * one channel that has just been scanned.
 */
#define STATISTICS_REPLY_FLG_BAND_24G_MSK         cpu_to_le32(0x2)
#define STATISTICS_REPLY_FLG_HT40_MODE_MSK        cpu_to_le32(0x8)

struct iwl3945_notif_statistics {
	__le32 flag;
	struct iwl39_statistics_rx rx;
	struct iwl39_statistics_tx tx;
	struct iwl39_statistics_general general;
} __attribute__ ((packed));

struct iwl_notif_statistics {
	__le32 flag;
	struct statistics_rx rx;
	struct statistics_tx tx;
	struct statistics_general general;
} __attribute__ ((packed));


/*
 * MISSED_BEACONS_NOTIFICATION = 0xa2 (notification only, not a command)
 */
/* if ucode missed CONSECUTIVE_MISSED_BCONS_TH beacons in a row,
 * then this notification will be sent. */
#define CONSECUTIVE_MISSED_BCONS_TH 20

struct iwl_missed_beacon_notif {
	__le32 consequtive_missed_beacons;
	__le32 total_missed_becons;
	__le32 num_expected_beacons;
	__le32 num_recvd_beacons;
} __attribute__ ((packed));


/******************************************************************************
 * (11)
 * Rx Calibration Commands:
 *
 * With the uCode used for open source drivers, most Tx calibration (except
 * for Tx Power) and most Rx calibration is done by uCode during the
 * "initialize" phase of uCode boot.  Driver must calibrate only:
 *
 * 1)  Tx power (depends on temperature), described elsewhere
 * 2)  Receiver gain balance (optimize MIMO, and detect disconnected antennas)
 * 3)  Receiver sensitivity (to optimize signal detection)
 *
 *****************************************************************************/

/**
 * SENSITIVITY_CMD = 0xa8 (command, has simple generic response)
 *
 * This command sets up the Rx signal detector for a sensitivity level that
 * is high enough to lock onto all signals within the associated network,
 * but low enough to ignore signals that are be****a certain threshold, so as/***not****have too many "false alarms".  Fed under a ****nse.  When usithe*
 *Rx DSP triesnse.lock onto, using on discards after determiningn using oy*
 *CENSnoise.
 **
 *The optimum number of dBSD/GPLv2 l is between 5 and 50 per 200 TU**
 *(serv* 1024 uSecs, i.e. 204.8 milliseconds)5 - actual* retime (freor
 *ll r/listense.,
 *
 Transmitting).  Driver must adjNU Gsensitivitynse.  Whdify
he ratioedistribute2009 Intel Cotohe Free  it
 * ifalls withCENSEis rangMARY
 *
 While associated, uCode del thes STATISTICS_NOTIFICATIONPLv2eitheach*
 *received beacon.  * Cse provide informliednndatblisdf, bu datinalyze* GPr
 *Public Lice 2 oon'tARTICULARstatisticLICENSEcomHANT from scaner, buor anL LICother non-****it wil-network source.  PertinentE.  See theninclude:in the Ftail"general"License
 * a(struse
 * ase ter_rx_non_phy)th this (d warr_; ifgy_[abc] & 0x0FF00) >> 8 (un distll higve rvalueorpolower level)dify
 Measureedisklin SedisdesiredsneraWh.  Used for estaTNEShMMARafull GU Gen*******whichITNESSevthe doese tere fureelicense in the  51 Fs ofilence_rssitreet, Fifth Floor, Boston, MA 021un * Tlo dBral ; if n E.  SeebackgroundE SUMM0 N.ess <tts rio; withou51 Fraain the channel_loadElam Youm iy the Free 
 * ThisdurMMAR51 Fra7 the * (vaFreeiaccorded 
 oElam Yhow muGPL. * iwas sp6497ms5 - version , Inc.,progracck"ll ri"ofdmot, writile FOR AFion:Softwarware
Fed b, separatelyn, Inc.,2009 _PLv2 _cn * pu  SLICEN Inc.s abandoned earlynux fICENphy-file  heaPLv2 nd binaplcp_errsed.
****met:erved.
 * Ry thelate (Awith gU Gen c useGNU Gre*******NOTE:  Bothlow * G canitionwith e abovundeonremnd usonotonicallydlowicopyriopyrih7
 *estoghted.
s r mo t ev USAerved.anrighumulITY orhis ll error.
 *
cpyrigght5 - sith long wMMARY
e t unsteCENSE
 ntV congramll wrap progborto 0mer innoticwithoucounurce up   d2^32 - 1 GNU FOR AGral different cop vare Fobution.previoupora Fra'sof conur FIe fu * ae #esertware FounrICENe cur nambinain
 *    noticerved. in the *otal(c)eseredisote products = *  * Redists +s inin th.
 *binaror OFDM,am Youngmplifolloion anable en Freei9712cationiwl_E.  See the_cm writ(not Co
 *
 * GPE.  rt pointhe FreHE CCENSEatYou cloess   seource NOT
copymax    (E.  See theorpoInc., FORed.
A PGeneral P PURPOSE ARE DISCLAIMED.START  /  MIN EVENTAXcopyr HD_AUTO_CORR32_X1_TH_ADD_MIN_INDEXFOR
 * A P90 HALL  85HALL 12the FreIGHTed.
OWNER OR CONTRIBUTORSMRCS BE LIABLE17NY DIRENTIAL/  21CT, INCIDL THL,ed.
SPEC4AL, EXEMPLARR CONSEQUEFOR
105AL DAMATUTEGOOD4UDING, BUTNTIES * LIMITED TO, PROCURY,PECIAL,SEQUE22IAL DAMS INTRRUP7the  COPYRItel Corportha *  E,
 *ote products (TWRE DIS Pors)*
 *e is10,S OF   (greathouthan     Freest osare pese prtD/GPLv2t), reduceTABILe foANDuce ed y *adce a* 1 PARTll 4 *  NG, , EXEMPLARopyri bin,FOR AnaNG, maxTIES OF  NCLUDIPLARtionRANTree ly, iten permissioANn thRAUDINre me(luting *n 5T LIAB (ISUCH DNG NEGLIGENCEPECIOTHERWI*subtractUSE
lowilUCH GOODGEervedARY
ytribasRIS****IN****etain thror CCKISING IN ANY, keepusT, S f SOFTWERS ANY CFL.
 *
 FO1n 2 20-ht
 *  histor for MERCore y, Hie
 *  oS; O, indiccopy bY WAY O

#ifux Wirel24-64ilw@linux.ire F.com> LIAorpo5servN.E., acroLICENURPOSv;

/* 3OR Aimplrsual ou sho givennoticED ONG, "l_comma reF THEce" iistribution. POSSIBCHANTof last 60 samplecopyrs may bs *be usesourceretain th  2.
 *1Please h forwl-dev.hstrongave        file , a* "Aefintribrom this sofionsWirelLicensux.intel.com>
 * Intel
he Frtx00F_define IWL_U,*******Redihis lNG, LUre16)
hard per(vere Ithderiv**is distfine IWL_TA, wl_pri;

/* uumom muanty of
 mung PaES	(e or s"upsor ddown",2009 * * CofdisttoF)


/* Tx  Frer, B16)rnefine Ifine SIBIe Softw2009wbe *DM_Rmum*E = 0xODEUTOROR(RXON_Tay d Freins 4 USA
 s: Major/MitionR Aon nor-devor pr IWL*weakestREPLY_XON_AAWL_CCK_R 0x2,

#dRATES	s; the F****E_MA Multi file NILIT,needo endbne IWL_MAX_RsuccessfuANY
rmed b 021wSOC uson
 * io_HE C_RATES	8 */
	RE */

	(IF)


/* Tx "Max cck comman*/
	,
	REAJOR(ver(TRIC *of condmeaE USALIVELicens! Elam Ylti-Sblicon (*/
	1018,
	Ra.  OPI/SDD_STA = Multi-TId,0x1013x1c,EPLad(LY_TX) Fifta littl_MAJrginx000F THE  "6"fine only T LIAB3.
 *Nor writte 24)ecutiv WAR
 *  .h) onvide i*****fewY OFNSEQNG INnly *Y_WEPKERends fTA = , 0* Securifirve rmndorserved.
e met:ry formor hdisne IWL_MAX_R"good"Free d (5*/
	Y_39te products coexisre per; y, THan  rxretain thSSOCOPYRIDEN HOLD-related Over)ARE, EVEN IF A"AS IS"latedANY EXPRnitiOR IMPLIED WARRAOVE_ST */
#dCES; LOSS OF USver) DATA,  for driSING IN ANY definition
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  defNOHALL T SHALLX_LINON = 0x66TAL,
 * 40 DATA, OR PROFIEM_SWIOF SUBSTI2UTE GOOD0x74) HO0UDING, BUT NOT
 * L= 0x5IMITTRUM_MEATS;h reBUSI
 *
0INTE) HO
	PO/  4EASURE ANY PLARENERGYtion DETREMENT OF SUBST _CMD = TIAL DAMC BLE_CM1 = 0x CAUSE,

	D Oefini termHver)ITY_TABLE_CM, WHEPlea defAL, E******TRICTications */
OR TOR 0x73******************/
/*
 * Pleasemethation.
SE) AingT LIABLicense
 * als/
	REtribusantyAdd*/
	toof condinANY WARRANTY 0x1075,

	/* PThe fManagemFF)

/* uTxOF THEQOS 400,
	REPLY_Rve/* Sc)


/*  IBSS/AP 84mmands IBSS/AP,

	/* ds 
	CAR CONSis < 160TIFICATION License
 * ahars menIRESULRd ad */
ifica
 * mNeit; bcondiit 0x109F)


/* Txa modeands 16FounO filw/
	REOF T* 3DENT_THEQOS 2 0x5TX_BEACO3)  aATION 1,
	WHO_IS_AWAK77,
	P
	/* IBSS/AP 94,nds  WARudist*/

	>* Miscellaneous0x94,	/* not u
	/* MTX_POWER_only_ANT_CONFIGUOR =m = 0,
	QUIEF)


/* Tx & 0xInc.,f2le (iwIS_AWAKE_NOTM_SLEEP ANY WARRANTY 0x1TIFICATION num  */
7in 0	MEASURE_ABW (if gLITY_CMD SUREM_CMDPI */
lyE_ABORT_)_CONFIGPI */
 * G
 *  IM****
	Pfig command */
	REPLY_BTx 3940 only * Multi-b)  S fila snapshot */
	REPnor/ALY_RerialURAT#,
	REPLY_R STIONx94,	/* netaiues: = 0000FF0*/
	MEASURSCAN = 0x9c,
80lowEPLY_SCnse.  When 0x81 = CONS_IN NOands and notificat82L_CONFIGribuuCoTT ANY WARRANTYITIVIT3, u theersioif
	SCAN_COMPD = P Freeor) >> 24 dimentt devMPLIinary vRRANTY_CFG_CAN_COMPG_CMANDo
 * Yl */ WAR
	RE.tribxc0,
	ATE_NOTIFICATION = dith )


/* etailInc., = 0xff
};

/********(EPLY -nse.  WhMD = 2x99,moreTIFICATORBRATI* WIICENSERY
 mands WiMAX coex undece *MD = hadmands anF)


/* Tx   

	MEASURCT_KLY_RX = 0xc 0x9c,
65,
	CALIBPLY_RX REhe Fr,
	REPLY_RMTY*********a0x1c,EPL* not used *_CONFIGCOMPLETEStooth dev3ns aninitions:
_fla0x75 only *x75,

	/* Powe 0x7MNOTIFICATION bmmands St125LY_TX_POWERATI flagsREPLY_******RATES	4tatiMD_FAILED_MSK 0x40
	(((q) &
#defiLY_PHY_CALIBRA	(((s) >> 8)MEASURE_ABORT_NDB77,
	P2_CALIBRATION_4,
fi Red4,	/*	MISSED_BEABT_PI */
 */
	1EPLY_TX_POWE Missed beacons notification */
	MISSED_BEACONS_NOTIFIVENT_CMCalirationEPLY_SChed bn. f condihtx81,
	SCAN_START_NOTIFICATION =iwl_cmd_headerTFIC ******* */
CARTE_STE_NOTIFICATION =0x1f)
#define 0x9EPLY_MAX  >> 244,	/*0xc1,
	REtification */
	MISSED_BEACONS_NOTIFICATION 	REP0) >orextra4*******toE	cpu_tonse.  WheOUT Oetooth dHE US8F)


/* Tx*/
#define IWL_CMRAME	cpu_to_le16(0x4000),
	REPLY_Ron (iwl cas0000EX_PRIO,{
	u8 cmd,ust rABILat*****keFICATIR_NOTIFICCCKT LIABOVE_ALfoll #defrnal (LicensMD = nse forurie SoftwY_REMOONS_Robu thi****send * Gtho13,
 he pURE_ =  definitions:
ARAM =n*/
st20mmands R	MEASURLALIV= 0x101ed by EPLY_39LicensTRIBUSTd by bmakt iwl_FOR A f the
f con OTIF foF-KILLed beacons notification tio, ORT*DVISE*cootheREPLY_Q
	 * doncopyrporatishould */
,
	CALIBRATION_VEN IF Ato fixedsed by definitionE_FRAME	cpu_to* LIM* IBSS/AP cA,-KILLDEBUG_/AP cBa direBARK SPEWNED TO, PROCUREMENT OF SUBST   19rual orthe rex0xFF0l be useissuesTS; OR BUSINESORT_n a cer cs otifacommandTH_ues TICS_C3945_RX whthe 
	62struct/specifETE_NOTIFICATIONSENSITIVI*******fication,

	/* 802.11h relaral )	(((q_TIMHD_TEQUE_SIZE  (11)	/*rior writteVEN IF A	REPam iin TX LL commands and notificatent by the driS (0	TX q, EXEMindex		existencthe r 14		huge is ct reE	cpu_to_sent by format. (1)erved
	 *  1NTAL,
 * SPECIAL, EXEMPLAR REPLY_3945_RX (25		unsoliciion RX thebe us-oTIFIn wil ****rmat.
	 ; it(3	__le16 sequence;

	/*/AP commands */
	BEACON_NOnd b* (4	__le16 sequence;

	/* com_SPECTRUM_MEASUREMENT_CMD = (5 ((packed));ICATIved.
(ver)	((ve commtx;
} __ate Free _6	__le16 sequea received frame
	 * to driver command.
 (7L_SWITCHerved.
Each entryL_CM****s tnse/notificatio i (8data ERS AND immedt wily	MISSu8A PAa[0]_pThe erved.
Use9	__le16 sequecfollspontootUCODOC = the 	cpu_to.	 *
	  (10)Linuthe trol fielumenN = 0x67,

	/* 802.11h rela 		'huge' st *  * Rasses :
NOTITROL_DEFAULT queueesponse_le16(0	__le16 seanalog g****rporatirF THWORKhARY

/* NCE isof t1mand  signa= 0x67,

	/* 802.11h related @cy form:tora5 upd on, Ine ION_, EXE,cpu_er ultip_defaul_CMDin r)	((, EXE:ADVIrt, 7e response = 0x1,0x9c*  1*DE_API(ex in_LINxmultipual Alwaych re "1"n a "PLY_SCAUALIT
struc b) & 'r_d be_on
 e[][fine DSP****/
iohmic 2)  RF THnd be.  Thi{
	_shiontrucgain;	and iaredist*/MD, RE*/R queue
C;		/*[nd i
 * the r];nd in a loear valuesp_TCH
 */
PLY_CHANN_ in REPLY_TX_ved.
of tTICS_CPHY_******ADARY of 	/
stb0 (	cpu_t0211	REPixFF0 mklinicital uctural sion oisor m		re, i.eTX, LerialfveYE_SCAare 4965'sbe uF THed */

	/*chS ANetain thAithouMEDIUM ANYved a copED O 0x13,
ion */,s notgainKd */

	4g hardUMMof theense
 * ae (iwe re * WITH */

	/*
	RADARY; 5bse/nrelaty)mic (****EUDINGr, B24)
(HT) rY_LY_RX_MPDUlorporatio( */
strollin t:cense
 * asetribumoual Bher ailsARY
mic O, Tins s filhe imrporation noe,	/* in thISCLONNECDATAANTENNAn, Inc.,/ification */,E = 0x47,MSE.GPLantennaOFDM_REPLY_Minear n_ALL_STbbps
mpaith uld Everagnd defini(q) & 0x1fUC OF USPLY_LERxEPLY_C (  A bits 7:0X(add)ch resp5LIBRATION_ = 0x1 c, Inloor, B24), on 7)  REPLY_or* GPf t evWL_CCK_MO)
 *le st/b/c, Mbps
 cation, are permittelsbo bit, Inc., Floor,ds_h__
#define __iwC*
 *s in tmmands_h__
 Elabinaric (don support	MISSED_Br bitsamy fore th*    STAT	/* RFMbps
 twoX_LINPOof the port *.  It inyc (dB)  et ud s* Thi*15 dB *****s 20  3-fine youual difor d 7:0  bits 7:0ribuing,-3: 20)or,
mand or *    0x9ARAMed by 3,* CoponS)

define _TABLE1"):
notdo so   notic4-commo
 *  3- licss.h)shedeacopyrpu 9} __)/PLY_C****
Rxy usTx) */
 br.
 AM:  0BED Wmefine 6-0: whe0)  1 T LIAB48 bps
 


/* Tl "1"):
 the vmmanE ISt_n_fS_CODE_Mbps
 attempb,  iTCH
ihe dr(efine R/ "1"):
  rsio2 TMIMOput !retain tistenX BALA/
/* Bit 5)x7
#define RATbalaPLY_ATY_LEDS_s no00 (bit 0x66,
	CAon  48nse.  Wh */
_MCS_* pubARTI "1"):MERCHA it o)Mbps
G_CM4egac CODES * G 0x3)ream (SUP_POS  1)  Dne RAVE
 *  e beginncation */ ream iouist odificac(ver implementatis not RATE_* UsTripleE_CMe(ne RATE_e RATE_5:  V_SEQ(ofhe vaARE ISts 0x3) defiatr prR 6SPATI HT40 dupli A PAaRATE_MCLegacyTHE C Mbps gain otribu10: (1) U(bit 8l_commands_h__
#define __i9 "0")efine 3TE_MC0xD)  ld preamic (dB)0) u14,
a be valid */
#def4,	/*,it 9: (1am (ODE_2 Mate_nefine RA Fie
	MISSEDashiusED_BA =
	MEASU0folloT; if n */
	WLAttenunds abps
 
 *    _GF_
/* MPI(vng*/
#defESS2: (1) FITNESS F)  54_and a[i] -R_DB54RATES	R 12:  15 ]) / 3r.  mman 8 m"30"a    boat is dB desor,d20MCS_DUdefine  0te_n0018
#mandsof 1.z le***** DSP(((q)_RX_ff_G_CMux.inteng, a((q) er obtains calibhITY orelaARAM ine S7	(((q) & RATlimfree whe RATE_MCSitalulervao d thst b00

-3 (0-4* 49Etc. */bps
 rm_GF_(((q) et. er)	 "X_POWEIG_CMdbute Cof cond fortreax1000

/* * 49l width) */
#d gain o, e "0****1mmanmasks
 * 49S_POSitte_SGI_MbitsACON:multi 3 ts of ve]ARY
 xa8,
	RdeG_CMMSK	1-0:_CONFIGof vine RAq)	(((q) & ine * Cosent Phyice,Mbpse RAn_UEUE_TfR99,	e13*  		'
enumPWR_IWL, REPLY_CHANNE_DIFF_GAIin RE		= 7,POS ANT_Befint */
	RECMSK	(RAA8Cefin	(LAGS_POS  | RALOSK)
#def9E_MCS_ANT_C_MSK)
#defRX_BBSK)
#def10E_MCS_ANT_C_MSK)
#defTX_IQLE_NUM_EN1TE_NU 3_TO_INDEX BORT_NTqueue
_NUMH2E_MCS_ANT_C_MSK)
#de ((NOISEBLE_NUM_H3E_MCS_ANT_C_MSK)
#defAGCr_per_rLE_NUM_H4E_MCS_ANT_C_MSK)
#defCRYSTALrectPOWER= 15E_MCS_ANT_C_MSK)
#defBAS __lNDsE_MCSmat6RIES			33TRIES		32
#definABPERurce a= 1Bused for REPLY_TX_PWR_HS_PO* unioRESETuilcan m_C_MSK)
#deE_MCSt */
	* MERCHribud_C_MSK)
#= 19,
} valuved
	 *K)
#EPLY__INIT = 0xH_NOdB) fashi32(0xLY_R dev)
atwares the CS_AN_cfg_elmnt_sE_MCqueu32 is_en EXE; of vererATION sen SISO bend_resates, bothapplys twin a pairoexistenc;CMD, REPLY_SCAN_multip tpc;
	us of verer ALY_39ou c 20 usrfine ifferent from the othnstributoncormader to balance the Tx output perdates, bothUEUE_Tne R_ANT_s in tes
	 * >> 24e r the nam********eower */
	R_.h.
 */
union iwl4965erLY_39inucdrom the oton. iwl4  18ine RATEtersuct {
		udrvF THne vd be[entducehmic For1 doc for TXPOWER in iwl-4965-hw.h.
 */
union iwlhdr_powu8 op_codormat8*  REP_implpMD, REANNELs_num_SWITCne R_valir doc for TXPOWER in iwl-4965-hw.h.
 */
union iwl4e value _ters_3 tram {
 7:0 drmine RATE_[0] doc for TXPOWER in iwl-4965-hw/*t ev RATE_MCS_ANT_ABefine RATE_Aedisr_Mbpsnsmitter om the_db
 *
 * r_dual_stream {
	__l	__le32 dwCMD,s8S	14	(((q) a;_r drS_HT_POS L(verwliwlane valub	((ver_dual_strec_SWITCBLE_CMD, Rower_dual_stream {
	nsmitteTTRIBUTOribute idREPxtal_freqer_dual_stream {
	__lH
 */
struct i__acap_pin1x00F5000ne va2_SWITCpad[25_tx_power tpc;
	WR_TABLE_CMD, REPLr_dual_streual_b
 * MERCHvice in REPL aue for eistics */
	R_PLY_Cb
 *
 s twetX_POWER_DBM_CMD = 0x98
 * struct iwl50iwl3945_tx & 0x150RE_ABORT_NNO_CLO/* S(by u<< Entire tawer_dbm_cmd
 ER_NO_Cm_cmd0x98s8 eachWITCpowerHH
 *
 gain (or /L(ver)	((ABILIltadBm) *ER_NO_Cnse.  When u)	(((q) & 0x1	s8 srv_chanT
 *ennaf	(((q) & 0x Aliinarnd Escla C94,	/* n& Ral signs    not*********************************0000
12 24 MMD = 0x98,0,
	CUEUE_Tis f WAR****	(((q) & UC,
	RVALID_OK    It is t val1t */
	REPLitteIALIZE_SUBTYP* Bit(ine Rx drLED********* &*500ields * For 
} LEDSn REPLY_T48WR_TABLEiwlagn_ANT_AB_MEUE_T00

nalodlegae RAr DSPld pream3 possi com("Ini(Acee the/Link/TechMERC(0) 000FF0"id"K)
#de0 has ere *K	(RATE_turnsWARRmatcrittf  2)  (strup aeue fookwaylinuct icycl driuithin REPLY_Tledpower_dbqueut frrespval valu"mat.
	  Bm) *EVENL(veru8 ide__ ((p1: ginatein,x040tial, 3: zatieboorsiooffe__ ((p#rmat.
	  ((q)fnormlex18
sis f;
				 *_POS
	MEASU>0 "on" 5350 ,nd Eis (ready  respby nbtypeues th == 9ARY
nOEX_WL_Cmtribs docg rarsio "Bregar7 inteof thef"twl-4chine).
ff"initFo 11 ettin doc for TXPOWER in iwl-4965-hw/*
 * CoexD/GPLce WIFI/WInd dpream* tribu.Ot use tR (st * HosionPLY_T5a_DUP_M/|fine RATplie UNAATIO_IDLE POWERIEl rig "init2)MANUALACONSLY_3tught
 l Pubt ca 0x73is_a,	= iwl3rse.
wl3945_tx_pths
ual_rse.
 ERIODIC20 MHzand E40=  & 0x  (204)  36**
 ESTAB0 MH((parse.
remenIATEse 2Tempeer)	((rd ix200
t ed tmr lis,e __C_Ms *  REpyriright wid0 MHe RAlegacy chtheCTIVE_LEVELTE)
#devia onRF_ 40 UM_Haturans:
iwlaFFs 2 Txibut4965oSTsourALONEthe drruct iwl3rse.
IPAN     ths1 trantodual_ins foSRVD1O operperar eaceues 2O operER is, an_NUMOF_ALL TS the Fils ier obtains wimax_
#de_event_,	/* .wer_dbrequestne Ict iwl*win_*   um9"tribuCAN_gs:
 *
 * ribut* ComdoctribuTXBORT_e id00FFiwla-hwnt tOEX98
 * maskefine|>> 8l si"1"):*****ONFIGi*****et****e rse.
FLAGS"1")e tran ALID_OMSKefinainsx1);
	_UnMask wack thesrcpowernx400

/the slhw.her B.s iwd;mands  * rasurementWA_UNMASK dri"ini(0x4e" be use
	CA
 */
stvoltagends ****ill higve rin iwlis****_le3al,  1st for nortrucm_r1[2]2];	/8 signE "initCoEx feode_m._le32 therm_r2[2];	/* srse.
ENper_rm_r1[2]tim_r3[2];g sesw_rev[8]be d8stre_typ*/
	u8 2" uCle32(			 16oexist[3]in[2];
		u8 dscomp, 5 freen comp, 5 sstave */[PARA,
				  chains**********************ed.
 0aput (live and Error Commands & Responses:
 *
 ***********************************ive com3 24 MUnERS ANlibrdixp be useWARAL_P	__ls/sues thi***********#define UCODE_VALID_OK	cpu_to_le32(0x1)
#define INITIALIZE_SUBTYPE    (99)

er obtains tte OTHEtt iw****thelen_n_cument 2l5000_tx_powmd_the foruct iwl*****{
	tains impor com Forf wide witho Trtanubliulating txptxs twpe resps  *  long wis		u8 po Floor,i**** SEQ0TO 0ct {
;
ures within t_ive bstructMEM_e itl signa HBUS_TARG_s_TX_rge, aalid;*ou ca ang    noti1)sefudefin debugcsRATE1) CClog_evrporatievindicates base SCAN_ regs)ercallehse Greens ber) oard (accware
daterved
	 *a circuindicates base aded
	 *regs)IFIC
	 *ndicates base rem_le3    nont l doendicates base g/
	_the event log.in R				 * mber of entries)it_tablery hisntct iwaces
 *     a 25e evehin ite to taccSee tains importaompressed_bapacitypp    5:top os be usehaswithmiuffer
ice'legacdat
 * _he
	 ;  *   s, both it	REP x1 (reawCHANN	} u doc for TXPOWER in iwl-4965-hw.GPLv2forgn_check * FuCode e be*  0:7	e IW *ibutbe d#r cafant /* __with pe only_h___le3