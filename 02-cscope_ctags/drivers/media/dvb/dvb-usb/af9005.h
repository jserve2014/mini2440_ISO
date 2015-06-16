/* Common header-file of the Linux driver for the Afatech 9005
 * USB1.1 DVB-T receiver.
 *
 * Copyright (C) 2007 Luca Olivetti (luca@ventoso.org)
 *
 * Thanks to Afatech who kindly provided information.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * see Documentation/dvb/README.dvb-usb for more information
 */
#ifndef _DVB_USB_AF9005_H_
#define _DVB_USB_AF9005_H_

#define DVB_USB_LOG_PREFIX "af9005"
#include "dvb-usb.h"

extern int dvb_usb_af9005_debug;
#define deb_info(args...) dprintk(dvb_usb_af9005_debug,0x01,args)
#define deb_xfer(args...) dprintk(dvb_usb_af9005_debug,0x02,args)
#define deb_rc(args...)   dprintk(dvb_usb_af9005_debug,0x04,args)
#define deb_reg(args...)  dprintk(dvb_usb_af9005_debug,0x08,args)
#define deb_i2c(args...)  dprintk(dvb_usb_af9005_debug,0x10,args)
#define deb_fw(args...)   dprintk(dvb_usb_af9005_debug,0x20,args)

extern int dvb_usb_af9005_led;

/* firmware */
#define FW_BULKOUT_SIZE 250
enum {
	FW_CONFIG,
	FW_CONFIRM,
	FW_BOOT
};

/* af9005 commands */
#define AF9005_OFDM_REG  0
#define AF9005_TUNER_REG 1

#define AF9005_REGISTER_RW     0x20
#define AF9005_REGISTER_RW_ACK 0x21

#define AF9005_CMD_OFDM_REG 0x00
#define AF9005_CMD_TUNER    0x80
#define AF9005_CMD_BURST    0x02
#define AF9005_CMD_AUTOINC  0x04
#define AF9005_CMD_READ     0x00
#define AF9005_CMD_WRITE    0x01

/* af9005 registers */
#define APO_REG_RESET					0xAEFF

#define APO_REG_I2C_RW_CAN_TUNER            0xF000
#define APO_REG_I2C_RW_SILICON_TUNER        0xF001
#define APO_REG_GPIO_RW_SILICON_TUNER       0xFFFE	/*  also for OFSM */
#define APO_REG_TRIGGER_OFSM                0xFFFF	/*  also for OFSM */

/***********************************************************************
 *  Apollo Registers from VLSI					       *
 ***********************************************************************/
#define xd_p_reg_aagc_inverted_agc	0xA000
#define	reg_aagc_inverted_agc_pos 0
#define	reg_aagc_inverted_agc_len 1
#define	reg_aagc_inverted_agc_lsb 0
#define xd_p_reg_aagc_sign_only	0xA000
#define	reg_aagc_sign_only_pos 1
#define	reg_aagc_sign_only_len 1
#define	reg_aagc_sign_only_lsb 0
#define xd_p_reg_aagc_slow_adc_en	0xA000
#define	reg_aagc_slow_adc_en_pos 2
#define	reg_aagc_slow_adc_en_len 1
#define	reg_aagc_slow_adc_en_lsb 0
#define xd_p_reg_aagc_slow_adc_scale	0xA000
#define	reg_aagc_slow_adc_scale_pos 3
#define	reg_aagc_slow_adc_scale_len 5
#define	reg_aagc_slow_adc_scale_lsb 0
#define xd_p_reg_aagc_check_slow_adc_lock	0xA001
#define	reg_aagc_check_slow_adc_lock_pos 0
#define	reg_aagc_check_slow_adc_lock_len 1
#define	reg_aagc_check_slow_adc_lock_lsb 0
#define xd_p_reg_aagc_init_control	0xA001
#define	reg_aagc_init_control_pos 1
#define	reg_aagc_init_control_len 1
#define	reg_aagc_init_control_lsb 0
#define xd_p_reg_aagc_total_gain_sel	0xA001
#define	reg_aagc_total_gain_sel_pos 2
#define	reg_aagc_total_gain_sel_len 2
#define	reg_aagc_total_gain_sel_lsb 0
#define xd_p_reg_aagc_out_inv	0xA001
#define	reg_aagc_out_inv_pos 5
#define	reg_aagc_out_inv_len 1
#define	reg_aagc_out_inv_lsb 0
#define xd_p_reg_aagc_int_en	0xA001
#define	reg_aagc_int_en_pos 6
#define	reg_aagc_int_en_len 1
#define	reg_aagc_int_en_lsb 0
#define xd_p_reg_aagc_lock_change_flag	0xA001
#define	reg_aagc_lock_change_flag_pos 7
#define	reg_aagc_lock_change_flag_len 1
#define	reg_aagc_lock_change_flag_lsb 0
#define xd_p_reg_aagc_rf_loop_bw_scale_acquire	0xA002
#define	reg_aagc_rf_loop_bw_scale_acquire_pos 0
#define	reg_aagc_rf_loop_bw_scale_acquire_len 5
#define	reg_aagc_rf_loop_bw_scale_acquire_lsb 0
#define xd_p_reg_aagc_rf_loop_bw_scale_track	0xA003
#define	reg_aagc_rf_loop_bw_scale_track_pos 0
#define	reg_aagc_rf_loop_bw_scale_track_len 5
#define	reg_aagc_rf_loop_bw_scale_track_lsb 0
#define xd_p_reg_aagc_if_loop_bw_scale_acquire	0xA004
#define	reg_aagc_if_loop_bw_scale_acquire_pos 0
#define	reg_aagc_if_loop_bw_scale_acquire_len 5
#define	reg_aagc_if_loop_bw_scale_acquire_lsb 0
#define xd_p_reg_aagc_if_loop_bw_scale_track	0xA005
#define	reg_aagc_if_loop_bw_scale_track_pos 0
#define	reg_aagc_if_loop_bw_scale_track_len 5
#define	reg_aagc_if_loop_bw_scale_track_lsb 0
#define xd_p_reg_aagc_max_rf_agc_7_0	0xA006
#define	reg_aagc_max_rf_agc_7_0_pos 0
#define	reg_aagc_max_rf_agc_7_0_len 8
#define	reg_aagc_max_rf_agc_7_0_lsb 0
#define xd_p_reg_aagc_max_rf_agc_9_8	0xA007
#define	reg_aagc_max_rf_agc_9_8_pos 0
#define	reg_aagc_max_rf_agc_9_8_len 2
#define	reg_aagc_max_rf_agc_9_8_lsb 8
#define xd_p_reg_aagc_min_rf_agc_7_0	0xA008
#define	reg_aagc_min_rf_agc_7_0_pos 0
#define	reg_aagc_min_rf_agc_7_0_len 8
#define	reg_aagc_min_rf_agc_7_0_lsb 0
#define xd_p_reg_aagc_min_rf_agc_9_8	0xA009
#define	reg_aagc_min_rf_agc_9_8_pos 0
#define	reg_aagc_min_rf_agc_9_8_len 2
#define	reg_aagc_min_rf_agc_9_8_lsb 8
#define xd_p_reg_aagc_max_if_agc_7_0	0xA00A
#define	reg_aagc_max_if_agc_7_0_pos 0
#define	reg_aagc_max_if_agc_7_0_len 8
#define	reg_aagc_max_if_agc_7_0_lsb 0
#define xd_p_reg_aagc_max_if_agc_9_8	0xA00B
#define	reg_aagc_max_if_agc_9_8_pos 0
#define	reg_aagc_max_if_agc_9_8_len 2
#define	reg_aagc_max_if_agc_9_8_lsb 8
#define xd_p_reg_aagc_min_if_agc_7_0	0xA00C
#define	reg_aagc_min_if_agc_7_0_pos 0
#define	reg_aagc_min_if_agc_7_0_len 8
#define	reg_aagc_min_if_agc_7_0_lsb 0
#define xd_p_reg_aagc_min_if_agc_9_8	0xA00D
#define	reg_aagc_min_if_agc_9_8_pos 0
#define	reg_aagc_min_if_agc_9_8_len 2
#define	reg_aagc_min_if_agc_9_8_lsb 8
#define xd_p_reg_aagc_lock_sample_scale	0xA00E
#define	reg_aagc_lock_sample_scale_pos 0
#define	reg_aagc_lock_sample_scale_len 5
#define	reg_aagc_lock_sample_scale_lsb 0
#define xd_p_reg_aagc_rf_agc_lock_scale_acquire	0xA00F
#define	reg_aagc_rf_agc_lock_scale_acquire_pos 0
#define	reg_aagc_rf_agc_lock_scale_acquire_len 3
#define	reg_aagc_rf_agc_lock_scale_acquire_lsb 0
#define xd_p_reg_aagc_rf_agc_lock_scale_track	0xA00F
#define	reg_aagc_rf_agc_lock_scale_track_pos 3
#define	reg_aagc_rf_agc_lock_scale_track_len 3
#define	reg_aagc_rf_agc_lock_scale_track_lsb 0
#define xd_p_reg_aagc_if_agc_lock_scale_acquire	0xA010
#define	reg_aagc_if_agc_lock_scale_acquire_pos 0
#define	reg_aagc_if_agc_lock_scale_acquire_len 3
#define	reg_aagc_if_agc_lock_scale_acquire_lsb 0
#define xd_p_reg_aagc_if_agc_lock_scale_track	0xA010
#define	reg_aagc_if_agc_lock_scale_track_pos 3
#define	reg_aagc_if_agc_lock_scale_track_len 3
#define	reg_aagc_if_agc_lock_scale_track_lsb 0
#define xd_p_reg_aagc_rf_top_numerator_7_0	0xA011
#define	reg_aagc_rf_top_numerator_7_0_pos 0
#define	reg_aagc_rf_top_numerator_7_0_len 8
#define	reg_aagc_rf_top_numerator_7_0_lsb 0
#define xd_p_reg_aagc_rf_top_numerator_9_8	0xA012
#define	reg_aagc_rf_top_numerator_9_8_pos 0
#define	reg_aagc_rf_top_numerator_9_8_len 2
#define	reg_aagc_rf_top_numerator_9_8_lsb 8
#define xd_p_reg_aagc_if_top_numerator_7_0	0xA013
#define	reg_aagc_if_top_numerator_7_0_pos 0
#define	reg_aagc_if_top_numerator_7_0_len 8
#define	reg_aagc_if_top_numerator_7_0_lsb 0
#define xd_p_reg_aagc_if_top_numerator_9_8	0xA014
#define	reg_aagc_if_top_numerator_9_8_pos 0
#define	reg_aagc_if_top_numerator_9_8_len 2
#define	reg_aagc_if_top_numerator_9_8_lsb 8
#define xd_p_reg_aagc_adc_out_desired_7_0	0xA015
#define	reg_aagc_adc_out_desired_7_0_pos 0
#define	reg_aagc_adc_out_desired_7_0_len 8
#define	reg_aagc_adc_out_desired_7_0_lsb 0
#define xd_p_reg_aagc_adc_out_desired_8	0xA016
#define	reg_aagc_adc_out_desired_8_pos 0
#define	reg_aagc_adc_out_desired_8_len 1
#define	reg_aagc_adc_out_desired_8_lsb 0
#define xd_p_reg_aagc_fixed_gain	0xA016
#define	reg_aagc_fixed_gain_pos 3
#define	reg_aagc_fixed_gain_len 1
#define	reg_aagc_fixed_gain_lsb 0
#define xd_p_reg_aagc_lock_count_th	0xA016
#define	reg_aagc_lock_count_th_pos 4
#define	reg_aagc_lock_count_th_len 4
#define	reg_aagc_lock_count_th_lsb 0
#define xd_p_reg_aagc_fixed_rf_agc_control_7_0	0xA017
#define	reg_aagc_fixed_rf_agc_control_7_0_pos 0
#define	reg_aagc_fixed_rf_agc_control_7_0_len 8
#define	reg_aagc_fixed_rf_agc_control_7_0_lsb 0
#define xd_p_reg_aagc_fixed_rf_agc_control_15_8	0xA018
#define	reg_aagc_fixed_rf_agc_control_15_8_pos 0
#define	reg_aagc_fixed_rf_agc_control_15_8_len 8
#define	reg_aagc_fixed_rf_agc_control_15_8_lsb 8
#define xd_p_reg_aagc_fixed_rf_agc_control_23_16	0xA019
#define	reg_aagc_fixed_rf_agc_control_23_16_pos 0
#define	reg_aagc_fixed_rf_agc_control_23_16_len 8
#define	reg_aagc_fixed_rf_agc_control_23_16_lsb 16
#define xd_p_reg_aagc_fixed_rf_agc_control_30_24	0xA01A
#define	reg_aagc_fixed_rf_agc_control_30_24_pos 0
#define	reg_aagc_fixed_rf_agc_control_30_24_len 7
#define	reg_aagc_fixed_rf_agc_control_30_24_lsb 24
#define xd_p_reg_aagc_fixed_if_agc_control_7_0	0xA01B
#define	reg_aagc_fixed_if_agc_control_7_0_pos 0
#define	reg_aagc_fixed_if_agc_control_7_0_len 8
#define	reg_aagc_fixed_if_agc_control_7_0_lsb 0
#define xd_p_reg_aagc_fixed_if_agc_control_15_8	0xA01C
#define	reg_aagc_fixed_if_agc_control_15_8_pos 0
#define	reg_aagc_fixed_if_agc_control_15_8_len 8
#define	reg_aagc_fixed_if_agc_control_15_8_lsb 8
#define xd_p_reg_aagc_fixed_if_agc_control_23_16	0xA01D
#define	reg_aagc_fixed_if_agc_control_23_16_pos 0
#define	reg_aagc_fixed_if_agc_control_23_16_len 8
#define	reg_aagc_fixed_if_agc_control_23_16_lsb 16
#define xd_p_reg_aagc_fixed_if_agc_control_30_24	0xA01E
#define	reg_aagc_fixed_if_agc_control_30_24_pos 0
#define	reg_aagc_fixed_if_agc_control_30_24_len 7
#define	reg_aagc_fixed_if_agc_control_30_24_lsb 24
#define xd_p_reg_aagc_rf_agc_unlock_numerator	0xA01F
#define	reg_aagc_rf_agc_unlock_numerator_pos 0
#define	reg_aagc_rf_agc_unlock_numerator_len 6
#define	reg_aagc_rf_agc_unlock_numerator_lsb 0
#define xd_p_reg_aagc_if_agc_unlock_numerator	0xA020
#define	reg_aagc_if_agc_unlock_numerator_pos 0
#define	reg_aagc_if_agc_unlock_numerator_len 6
#define	reg_aagc_if_agc_unlock_numerator_lsb 0
#define xd_p_reg_unplug_th	0xA021
#define	reg_unplug_th_pos 0
#define	reg_unplug_th_len 8
#define	reg_aagc_rf_x0_lsb 0
#define xd_p_reg_weak_signal_rfagc_thr 0xA022
#define	reg_weak_signal_rfagc_thr_pos 0
#define	reg_weak_signal_rfagc_thr_len 8
#define	reg_weak_signal_rfagc_thr_lsb 0
#define xd_p_reg_unplug_rf_gain_th 0xA023
#define	reg_unplug_rf_gain_th_pos 0
#define	reg_unplug_rf_gain_th_len 8
#define	reg_unplug_rf_gain_th_lsb 0
#define xd_p_reg_unplug_dtop_rf_gain_th 0xA024
#define	reg_unplug_dtop_rf_gain_th_pos 0
#define	reg_unplug_dtop_rf_gain_th_len 8
#define	reg_unplug_dtop_rf_gain_th_lsb 0
#define xd_p_reg_unplug_dtop_if_gain_th 0xA025
#define	reg_unplug_dtop_if_gain_th_pos 0
#define	reg_unplug_dtop_if_gain_th_len 8
#define	reg_unplug_dtop_if_gain_th_lsb 0
#define xd_p_reg_top_recover_at_unplug_en 0xA026
#define	reg_top_recover_at_unplug_en_pos 0
#define	reg_top_recover_at_unplug_en_len 1
#define	reg_top_recover_at_unplug_en_lsb 0
#define xd_p_reg_aagc_rf_x6	0xA027
#define	reg_aagc_rf_x6_pos 0
#define	reg_aagc_rf_x6_len 8
#define	reg_aagc_rf_x6_lsb 0
#define xd_p_reg_aagc_rf_x7	0xA028
#define	reg_aagc_rf_x7_pos 0
#define	reg_aagc_rf_x7_len 8
#define	reg_aagc_rf_x7_lsb 0
#define xd_p_reg_aagc_rf_x8	0xA029
#define	reg_aagc_rf_x8_pos 0
#define	reg_aagc_rf_x8_len 8
#define	reg_aagc_rf_x8_lsb 0
#define xd_p_reg_aagc_rf_x9	0xA02A
#define	reg_aagc_rf_x9_pos 0
#define	reg_aagc_rf_x9_len 8
#define	reg_aagc_rf_x9_lsb 0
#define xd_p_reg_aagc_rf_x10	0xA02B
#define	reg_aagc_rf_x10_pos 0
#define	reg_aagc_rf_x10_len 8
#define	reg_aagc_rf_x10_lsb 0
#define xd_p_reg_aagc_rf_x11	0xA02C
#define	reg_aagc_rf_x11_pos 0
#define	reg_aagc_rf_x11_len 8
#define	reg_aagc_rf_x11_lsb 0
#define xd_p_reg_aagc_rf_x12	0xA02D
#define	reg_aagc_rf_x12_pos 0
#define	reg_aagc_rf_x12_len 8
#define	reg_aagc_rf_x12_lsb 0
#define xd_p_reg_aagc_rf_x13	0xA02E
#define	reg_aagc_rf_x13_pos 0
#define	reg_aagc_rf_x13_len 8
#define	reg_aagc_rf_x13_lsb 0
#define xd_p_reg_aagc_if_x0	0xA02F
#define	reg_aagc_if_x0_pos 0
#define	reg_aagc_if_x0_len 8
#define	reg_aagc_if_x0_lsb 0
#define xd_p_reg_aagc_if_x1	0xA030
#define	reg_aagc_if_x1_pos 0
#define	reg_aagc_if_x1_len 8
#define	reg_aagc_if_x1_lsb 0
#define xd_p_reg_aagc_if_x2	0xA031
#define	reg_aagc_if_x2_pos 0
#define	reg_aagc_if_x2_len 8
#define	reg_aagc_if_x2_lsb 0
#define xd_p_reg_aagc_if_x3	0xA032
#define	reg_aagc_if_x3_pos 0
#define	reg_aagc_if_x3_len 8
#define	reg_aagc_if_x3_lsb 0
#define xd_p_reg_aagc_if_x4	0xA033
#define	reg_aagc_if_x4_pos 0
#define	reg_aagc_if_x4_len 8
#define	reg_aagc_if_x4_lsb 0
#define xd_p_reg_aagc_if_x5	0xA034
#define	reg_aagc_if_x5_pos 0
#define	reg_aagc_if_x5_len 8
#define	reg_aagc_if_x5_lsb 0
#define xd_p_reg_aagc_if_x6	0xA035
#define	reg_aagc_if_x6_pos 0
#define	reg_aagc_if_x6_len 8
#define	reg_aagc_if_x6_lsb 0
#define xd_p_reg_aagc_if_x7	0xA036
#define	reg_aagc_if_x7_pos 0
#define	reg_aagc_if_x7_len 8
#define	reg_aagc_if_x7_lsb 0
#define xd_p_reg_aagc_if_x8	0xA037
#define	reg_aagc_if_x8_pos 0
#define	reg_aagc_if_x8_len 8
#define	reg_aagc_if_x8_lsb 0
#define xd_p_reg_aagc_if_x9	0xA038
#define	reg_aagc_if_x9_pos 0
#define	reg_aagc_if_x9_len 8
#define	reg_aagc_if_x9_lsb 0
#define xd_p_reg_aagc_if_x10	0xA039
#define	reg_aagc_if_x10_pos 0
#define	reg_aagc_if_x10_len 8
#define	reg_aagc_if_x10_lsb 0
#define xd_p_reg_aagc_if_x11	0xA03A
#define	reg_aagc_if_x11_pos 0
#define	reg_aagc_if_x11_len 8
#define	reg_aagc_if_x11_lsb 0
#define xd_p_reg_aagc_if_x12	0xA03B
#define	reg_aagc_if_x12_pos 0
#define	reg_aagc_if_x12_len 8
#define	reg_aagc_if_x12_lsb 0
#define xd_p_reg_aagc_if_x13	0xA03C
#define	reg_aagc_if_x13_pos 0
#define	reg_aagc_if_x13_len 8
#define	reg_aagc_if_x13_lsb 0
#define xd_p_reg_aagc_min_rf_ctl_8bit_for_dca	0xA03D
#define	reg_aagc_min_rf_ctl_8bit_for_dca_pos 0
#define	reg_aagc_min_rf_ctl_8bit_for_dca_len 8
#define	reg_aagc_min_rf_ctl_8bit_for_dca_lsb 0
#define xd_p_reg_aagc_min_if_ctl_8bit_for_dca	0xA03E
#define	reg_aagc_min_if_ctl_8bit_for_dca_pos 0
#define	reg_aagc_min_if_ctl_8bit_for_dca_len 8
#define	reg_aagc_min_if_ctl_8bit_for_dca_lsb 0
#define xd_r_reg_aagc_total_gain_7_0	0xA070
#define	reg_aagc_total_gain_7_0_pos 0
#define	reg_aagc_total_gain_7_0_len 8
#define	reg_aagc_total_gain_7_0_lsb 0
#define xd_r_reg_aagc_total_gain_15_8	0xA071
#define	reg_aagc_total_gain_15_8_pos 0
#define	reg_aagc_total_gain_15_8_len 8
#define	reg_aagc_total_gain_15_8_lsb 8
#define xd_p_reg_aagc_in_sat_cnt_7_0	0xA074
#define	reg_aagc_in_sat_cnt_7_0_pos 0
#define	reg_aagc_in_sat_cnt_7_0_len 8
#define	reg_aagc_in_sat_cnt_7_0_lsb 0
#define xd_p_reg_aagc_in_sat_cnt_15_8	0xA075
#define	reg_aagc_in_sat_cnt_15_8_pos 0
#define	reg_aagc_in_sat_cnt_15_8_len 8
#define	reg_aagc_in_sat_cnt_15_8_lsb 8
#define xd_p_reg_aagc_in_sat_cnt_23_16	0xA076
#define	reg_aagc_in_sat_cnt_23_16_pos 0
#define	reg_aagc_in_sat_cnt_23_16_len 8
#define	reg_aagc_in_sat_cnt_23_16_lsb 16
#define xd_p_reg_aagc_in_sat_cnt_31_24	0xA077
#define	reg_aagc_in_sat_cnt_31_24_pos 0
#define	reg_aagc_in_sat_cnt_31_24_len 8
#define	reg_aagc_in_sat_cnt_31_24_lsb 24
#define xd_r_reg_aagc_digital_rf_volt_7_0	0xA078
#define	reg_aagc_digital_rf_volt_7_0_pos 0
#define	reg_aagc_digital_rf_volt_7_0_len 8
#define	reg_aagc_digital_rf_volt_7_0_lsb 0
#define xd_r_reg_aagc_digital_rf_volt_9_8	0xA079
#define	reg_aagc_digital_rf_volt_9_8_pos 0
#define	reg_aagc_digital_rf_volt_9_8_len 2
#define	reg_aagc_digital_rf_volt_9_8_lsb 8
#define xd_r_reg_aagc_digital_if_volt_7_0	0xA07A
#define	reg_aagc_digital_if_volt_7_0_pos 0
#define	reg_aagc_digital_if_volt_7_0_len 8
#define	reg_aagc_digital_if_volt_7_0_lsb 0
#define xd_r_reg_aagc_digital_if_volt_9_8	0xA07B
#define	reg_aagc_digital_if_volt_9_8_pos 0
#define	reg_aagc_digital_if_volt_9_8_len 2
#define	reg_aagc_digital_if_volt_9_8_lsb 8
#define xd_r_reg_aagc_rf_gain	0xA07C
#define	reg_aagc_rf_gain_pos 0
#define	reg_aagc_rf_gain_len 8
#define	reg_aagc_rf_gain_lsb 0
#define xd_r_reg_aagc_if_gain	0xA07D
#define	reg_aagc_if_gain_pos 0
#define	reg_aagc_if_gain_len 8
#define	reg_aagc_if_gain_lsb 0
#define xd_p_tinr_imp_indicator	0xA080
#define	tinr_imp_indicator_pos 0
#define	tinr_imp_indicator_len 2
#define	tinr_imp_indicator_lsb 0
#define xd_p_reg_tinr_fifo_size	0xA080
#define	reg_tinr_fifo_size_pos 2
#define	reg_tinr_fifo_size_len 5
#define	reg_tinr_fifo_size_lsb 0
#define xd_p_reg_tinr_saturation_cnt_th	0xA081
#define	reg_tinr_saturation_cnt_th_pos 0
#define	reg_tinr_saturation_cnt_th_len 4
#define	reg_tinr_saturation_cnt_th_lsb 0
#define xd_p_reg_tinr_saturation_th_3_0	0xA081
#define	reg_tinr_saturation_th_3_0_pos 4
#define	reg_tinr_saturation_th_3_0_len 4
#define	reg_tinr_saturation_th_3_0_lsb 0
#define xd_p_reg_tinr_saturation_th_8_4	0xA082
#define	reg_tinr_saturation_th_8_4_pos 0
#define	reg_tinr_saturation_th_8_4_len 5
#define	reg_tinr_saturation_th_8_4_lsb 4
#define xd_p_reg_tinr_imp_duration_th_2k_7_0	0xA083
#define	reg_tinr_imp_duration_th_2k_7_0_pos 0
#define	reg_tinr_imp_duration_th_2k_7_0_len 8
#define	reg_tinr_imp_duration_th_2k_7_0_lsb 0
#define xd_p_reg_tinr_imp_duration_th_2k_8	0xA084
#define	reg_tinr_imp_duration_th_2k_8_pos 0
#define	reg_tinr_imp_duration_th_2k_8_len 1
#define	reg_tinr_imp_duration_th_2k_8_lsb 0
#define xd_p_reg_tinr_imp_duration_th_8k_7_0	0xA085
#define	reg_tinr_imp_duration_th_8k_7_0_pos 0
#define	reg_tinr_imp_duration_th_8k_7_0_len 8
#define	reg_tinr_imp_duration_th_8k_7_0_lsb 0
#define xd_p_reg_tinr_imp_duration_th_8k_10_8	0xA086
#define	reg_tinr_imp_duration_th_8k_10_8_pos 0
#define	reg_tinr_imp_duration_th_8k_10_8_len 3
#define	reg_tinr_imp_duration_th_8k_10_8_lsb 8
#define xd_p_reg_tinr_freq_ratio_6m_7_0	0xA087
#define	reg_tinr_freq_ratio_6m_7_0_pos 0
#define	reg_tinr_freq_ratio_6m_7_0_len 8
#define	reg_tinr_freq_ratio_6m_7_0_lsb 0
#define xd_p_reg_tinr_freq_ratio_6m_12_8	0xA088
#define	reg_tinr_freq_ratio_6m_12_8_pos 0
#define	reg_tinr_freq_ratio_6m_12_8_len 5
#define	reg_tinr_freq_ratio_6m_12_8_lsb 8
#define xd_p_reg_tinr_freq_ratio_7m_7_0	0xA089
#define	reg_tinr_freq_ratio_7m_7_0_pos 0
#define	reg_tinr_freq_ratio_7m_7_0_len 8
#define	reg_tinr_freq_ratio_7m_7_0_lsb 0
#define xd_p_reg_tinr_freq_ratio_7m_12_8	0xA08A
#define	reg_tinr_freq_ratio_7m_12_8_pos 0
#define	reg_tinr_freq_ratio_7m_12_8_len 5
#define	reg_tinr_freq_ratio_7m_12_8_lsb 8
#define xd_p_reg_tinr_freq_ratio_8m_7_0	0xA08B
#define	reg_tinr_freq_ratio_8m_7_0_pos 0
#define	reg_tinr_freq_ratio_8m_7_0_len 8
#define	reg_tinr_freq_ratio_8m_7_0_lsb 0
#define xd_p_reg_tinr_freq_ratio_8m_12_8	0xA08C
#define	reg_tinr_freq_ratio_8m_12_8_pos 0
#define	reg_tinr_freq_ratio_8m_12_8_len 5
#define	reg_tinr_freq_ratio_8m_12_8_lsb 8
#define xd_p_reg_tinr_imp_duration_th_low_2k	0xA08D
#define	reg_tinr_imp_duration_th_low_2k_pos 0
#define	reg_tinr_imp_duration_th_low_2k_len 8
#define	reg_tinr_imp_duration_th_low_2k_lsb 0
#define xd_p_reg_tinr_imp_duration_th_low_8k	0xA08E
#define	reg_tinr_imp_duration_th_low_8k_pos 0
#define	reg_tinr_imp_duration_th_low_8k_len 8
#define	reg_tinr_imp_duration_th_low_8k_lsb 0
#define xd_r_reg_tinr_counter_7_0	0xA090
#define	reg_tinr_counter_7_0_pos 0
#define	reg_tinr_counter_7_0_len 8
#define	reg_tinr_counter_7_0_lsb 0
#define xd_r_reg_tinr_counter_15_8	0xA091
#define	reg_tinr_counter_15_8_pos 0
#define	reg_tinr_counter_15_8_len 8
#define	reg_tinr_counter_15_8_lsb 8
#define xd_p_reg_tinr_adative_tinr_en	0xA093
#define	reg_tinr_adative_tinr_en_pos 0
#define	reg_tinr_adative_tinr_en_len 1
#define	reg_tinr_adative_tinr_en_lsb 0
#define xd_p_reg_tinr_peak_fifo_size	0xA093
#define	reg_tinr_peak_fifo_size_pos 1
#define	reg_tinr_peak_fifo_size_len 5
#define	reg_tinr_peak_fifo_size_lsb 0
#define xd_p_reg_tinr_counter_rst	0xA093
#define	reg_tinr_counter_rst_pos 6
#define	reg_tinr_counter_rst_len 1
#define	reg_tinr_counter_rst_lsb 0
#define xd_p_reg_tinr_search_period_7_0	0xA094
#define	reg_tinr_search_period_7_0_pos 0
#define	reg_tinr_search_period_7_0_len 8
#define	reg_tinr_search_period_7_0_lsb 0
#define xd_p_reg_tinr_search_period_15_8	0xA095
#define	reg_tinr_search_period_15_8_pos 0
#define	reg_tinr_search_period_15_8_len 8
#define	reg_tinr_search_period_15_8_lsb 8
#define xd_p_reg_ccifs_fcw_7_0	0xA0A0
#define	reg_ccifs_fcw_7_0_pos 0
#define	reg_ccifs_fcw_7_0_len 8
#define	reg_ccifs_fcw_7_0_lsb 0
#define xd_p_reg_ccifs_fcw_12_8	0xA0A1
#define	reg_ccifs_fcw_12_8_pos 0
#define	reg_ccifs_fcw_12_8_len 5
#define	reg_ccifs_fcw_12_8_lsb 8
#define xd_p_reg_ccifs_spec_inv	0xA0A1
#define	reg_ccifs_spec_inv_pos 5
#define	reg_ccifs_spec_inv_len 1
#define	reg_ccifs_spec_inv_lsb 0
#define xd_p_reg_gp_trigger	0xA0A2
#define	reg_gp_trigger_pos 0
#define	reg_gp_trigger_len 1
#define	reg_gp_trigger_lsb 0
#define xd_p_reg_trigger_sel	0xA0A2
#define	reg_trigger_sel_pos 1
#define	reg_trigger_sel_len 2
#define	reg_trigger_sel_lsb 0
#define xd_p_reg_debug_ofdm	0xA0A2
#define	reg_debug_ofdm_pos 3
#define	reg_debug_ofdm_len 2
#define	reg_debug_ofdm_lsb 0
#define xd_p_reg_trigger_module_sel	0xA0A3
#define	reg_trigger_module_sel_pos 0
#define	reg_trigger_module_sel_len 6
#define	reg_trigger_module_sel_lsb 0
#define xd_p_reg_trigger_set_sel	0xA0A4
#define	reg_trigger_set_sel_pos 0
#define	reg_trigger_set_sel_len 6
#define	reg_trigger_set_sel_lsb 0
#define xd_p_reg_fw_int_mask_n	0xA0A4
#define	reg_fw_int_mask_n_pos 6
#define	reg_fw_int_mask_n_len 1
#define	reg_fw_int_mask_n_lsb 0
#define xd_p_reg_debug_group	0xA0A5
#define	reg_debug_group_pos 0
#define	reg_debug_group_len 4
#define	reg_debug_group_lsb 0
#define xd_p_reg_odbg_clk_sel	0xA0A5
#define	reg_odbg_clk_sel_pos 4
#define	reg_odbg_clk_sel_len 2
#define	reg_odbg_clk_sel_lsb 0
#define xd_p_reg_ccif_sc	0xA0C0
#define	reg_ccif_sc_pos 0
#define	reg_ccif_sc_len 4
#define	reg_ccif_sc_lsb 0
#define xd_r_reg_ccif_saturate	0xA0C1
#define	reg_ccif_saturate_pos 0
#define	reg_ccif_saturate_len 2
#define	reg_ccif_saturate_lsb 0
#define xd_r_reg_antif_saturate	0xA0C1
#define	reg_antif_saturate_pos 2
#define	reg_antif_saturate_len 4
#define	reg_antif_saturate_lsb 0
#define xd_r_reg_acif_saturate	0xA0C2
#define	reg_acif_saturate_pos 0
#define	reg_acif_saturate_len 8
#define	reg_acif_saturate_lsb 0
#define xd_p_reg_tmr_timer0_threshold_7_0	0xA0C8
#define	reg_tmr_timer0_threshold_7_0_pos 0
#define	reg_tmr_timer0_threshold_7_0_len 8
#define	reg_tmr_timer0_threshold_7_0_lsb 0
#define xd_p_reg_tmr_timer0_threshold_15_8	0xA0C9
#define	reg_tmr_timer0_threshold_15_8_pos 0
#define	reg_tmr_timer0_threshold_15_8_len 8
#define	reg_tmr_timer0_threshold_15_8_lsb 8
#define xd_p_reg_tmr_timer0_enable	0xA0CA
#define	reg_tmr_timer0_enable_pos 0
#define	reg_tmr_timer0_enable_len 1
#define	reg_tmr_timer0_enable_lsb 0
#define xd_p_reg_tmr_timer0_clk_sel	0xA0CA
#define	reg_tmr_timer0_clk_sel_pos 1
#define	reg_tmr_timer0_clk_sel_len 1
#define	reg_tmr_timer0_clk_sel_lsb 0
#define xd_p_reg_tmr_timer0_int	0xA0CA
#define	reg_tmr_timer0_int_pos 2
#define	reg_tmr_timer0_int_len 1
#define	reg_tmr_timer0_int_lsb 0
#define xd_p_reg_tmr_timer0_rst	0xA0CA
#define	reg_tmr_timer0_rst_pos 3
#define	reg_tmr_timer0_rst_len 1
#define	reg_tmr_timer0_rst_lsb 0
#define xd_r_reg_tmr_timer0_count_7_0	0xA0CB
#define	reg_tmr_timer0_count_7_0_pos 0
#define	reg_tmr_timer0_count_7_0_len 8
#define	reg_tmr_timer0_count_7_0_lsb 0
#define xd_r_reg_tmr_timer0_count_15_8	0xA0CC
#define	reg_tmr_timer0_count_15_8_pos 0
#define	reg_tmr_timer0_count_15_8_len 8
#define	reg_tmr_timer0_count_15_8_lsb 8
#define xd_p_reg_suspend	0xA0CD
#define	reg_suspend_pos 0
#define	reg_suspend_len 1
#define	reg_suspend_lsb 0
#define xd_p_reg_suspend_rdy	0xA0CD
#define	reg_suspend_rdy_pos 1
#define	reg_suspend_rdy_len 1
#define	reg_suspend_rdy_lsb 0
#define xd_p_reg_resume	0xA0CD
#define	reg_resume_pos 2
#define	reg_resume_len 1
#define	reg_resume_lsb 0
#define xd_p_reg_resume_rdy	0xA0CD
#define	reg_resume_rdy_pos 3
#define	reg_resume_rdy_len 1
#define	reg_resume_rdy_lsb 0
#define xd_p_reg_fmf	0xA0CE
#define	reg_fmf_pos 0
#define	reg_fmf_len 8
#define	reg_fmf_lsb 0
#define xd_p_ccid_accumulate_num_2k_7_0	0xA100
#define	ccid_accumulate_num_2k_7_0_pos 0
#define	ccid_accumulate_num_2k_7_0_len 8
#define	ccid_accumulate_num_2k_7_0_lsb 0
#define xd_p_ccid_accumulate_num_2k_12_8	0xA101
#define	ccid_accumulate_num_2k_12_8_pos 0
#define	ccid_accumulate_num_2k_12_8_len 5
#define	ccid_accumulate_num_2k_12_8_lsb 8
#define xd_p_ccid_accumulate_num_8k_7_0	0xA102
#define	ccid_accumulate_num_8k_7_0_pos 0
#define	ccid_accumulate_num_8k_7_0_len 8
#define	ccid_accumulate_num_8k_7_0_lsb 0
#define xd_p_ccid_accumulate_num_8k_14_8	0xA103
#define	ccid_accumulate_num_8k_14_8_pos 0
#define	ccid_accumulate_num_8k_14_8_len 7
#define	ccid_accumulate_num_8k_14_8_lsb 8
#define xd_p_ccid_desired_level_0	0xA103
#define	ccid_desired_level_0_pos 7
#define	ccid_desired_level_0_len 1
#define	ccid_desired_level_0_lsb 0
#define xd_p_ccid_desired_level_8_1	0xA104
#define	ccid_desired_level_8_1_pos 0
#define	ccid_desired_level_8_1_len 8
#define	ccid_desired_level_8_1_lsb 1
#define xd_p_ccid_apply_delay	0xA105
#define	ccid_apply_delay_pos 0
#define	ccid_apply_delay_len 7
#define	ccid_apply_delay_lsb 0
#define xd_p_ccid_CCID_Threshold1	0xA106
#define	ccid_CCID_Threshold1_pos 0
#define	ccid_CCID_Threshold1_len 8
#define	ccid_CCID_Threshold1_lsb 0
#define xd_p_ccid_CCID_Threshold2	0xA107
#define	ccid_CCID_Threshold2_pos 0
#define	ccid_CCID_Threshold2_len 8
#define	ccid_CCID_Threshold2_lsb 0
#define xd_p_reg_ccid_gain_scale	0xA108
#define	reg_ccid_gain_scale_pos 0
#define	reg_ccid_gain_scale_len 4
#define	reg_ccid_gain_scale_lsb 0
#define xd_p_reg_ccid2_passband_gain_set	0xA108
#define	reg_ccid2_passband_gain_set_pos 4
#define	reg_ccid2_passband_gain_set_len 4
#define	reg_ccid2_passband_gain_set_lsb 0
#define xd_r_ccid_multiplier_7_0	0xA109
#define	ccid_multiplier_7_0_pos 0
#define	ccid_multiplier_7_0_len 8
#define	ccid_multiplier_7_0_lsb 0
#define xd_r_ccid_multiplier_15_8	0xA10A
#define	ccid_multiplier_15_8_pos 0
#define	ccid_multiplier_15_8_len 8
#define	ccid_multiplier_15_8_lsb 8
#define xd_r_ccid_right_shift_bits	0xA10B
#define	ccid_right_shift_bits_pos 0
#define	ccid_right_shift_bits_len 4
#define	ccid_right_shift_bits_lsb 0
#define xd_r_reg_ccid_sx_7_0	0xA10C
#define	reg_ccid_sx_7_0_pos 0
#define	reg_ccid_sx_7_0_len 8
#define	reg_ccid_sx_7_0_lsb 0
#define xd_r_reg_ccid_sx_15_8	0xA10D
#define	reg_ccid_sx_15_8_pos 0
#define	reg_ccid_sx_15_8_len 8
#define	reg_ccid_sx_15_8_lsb 8
#define xd_r_reg_ccid_sx_21_16	0xA10E
#define	reg_ccid_sx_21_16_pos 0
#define	reg_ccid_sx_21_16_len 6
#define	reg_ccid_sx_21_16_lsb 16
#define xd_r_reg_ccid_sy_7_0	0xA110
#define	reg_ccid_sy_7_0_pos 0
#define	reg_ccid_sy_7_0_len 8
#define	reg_ccid_sy_7_0_lsb 0
#define xd_r_reg_ccid_sy_15_8	0xA111
#define	reg_ccid_sy_15_8_pos 0
#define	reg_ccid_sy_15_8_len 8
#define	reg_ccid_sy_15_8_lsb 8
#define xd_r_reg_ccid_sy_23_16	0xA112
#define	reg_ccid_sy_23_16_pos 0
#define	reg_ccid_sy_23_16_len 8
#define	reg_ccid_sy_23_16_lsb 16
#define xd_r_reg_ccid2_sz_7_0	0xA114
#define	reg_ccid2_sz_7_0_pos 0
#define	reg_ccid2_sz_7_0_len 8
#define	reg_ccid2_sz_7_0_lsb 0
#define xd_r_reg_ccid2_sz_15_8	0xA115
#define	reg_ccid2_sz_15_8_pos 0
#define	reg_ccid2_sz_15_8_len 8
#define	reg_ccid2_sz_15_8_lsb 8
#define xd_r_reg_ccid2_sz_23_16	0xA116
#define	reg_ccid2_sz_23_16_pos 0
#define	reg_ccid2_sz_23_16_len 8
#define	reg_ccid2_sz_23_16_lsb 16
#define xd_r_reg_ccid2_sz_25_24	0xA117
#define	reg_ccid2_sz_25_24_pos 0
#define	reg_ccid2_sz_25_24_len 2
#define	reg_ccid2_sz_25_24_lsb 24
#define xd_r_reg_ccid2_sy_7_0	0xA118
#define	reg_ccid2_sy_7_0_pos 0
#define	reg_ccid2_sy_7_0_len 8
#define	reg_ccid2_sy_7_0_lsb 0
#define xd_r_reg_ccid2_sy_15_8	0xA119
#define	reg_ccid2_sy_15_8_pos 0
#define	reg_ccid2_sy_15_8_len 8
#define	reg_ccid2_sy_15_8_lsb 8
#define xd_r_reg_ccid2_sy_23_16	0xA11A
#define	reg_ccid2_sy_23_16_pos 0
#define	reg_ccid2_sy_23_16_len 8
#define	reg_ccid2_sy_23_16_lsb 16
#define xd_r_reg_ccid2_sy_25_24	0xA11B
#define	reg_ccid2_sy_25_24_pos 0
#define	reg_ccid2_sy_25_24_len 2
#define	reg_ccid2_sy_25_24_lsb 24
#define xd_p_dagc1_accumulate_num_2k_7_0	0xA120
#define	dagc1_accumulate_num_2k_7_0_pos 0
#define	dagc1_accumulate_num_2k_7_0_len 8
#define	dagc1_accumulate_num_2k_7_0_lsb 0
#define xd_p_dagc1_accumulate_num_2k_12_8	0xA121
#define	dagc1_accumulate_num_2k_12_8_pos 0
#define	dagc1_accumulate_num_2k_12_8_len 5
#define	dagc1_accumulate_num_2k_12_8_lsb 8
#define xd_p_dagc1_accumulate_num_8k_7_0	0xA122
#define	dagc1_accumulate_num_8k_7_0_pos 0
#define	dagc1_accumulate_num_8k_7_0_len 8
#define	dagc1_accumulate_num_8k_7_0_lsb 0
#define xd_p_dagc1_accumulate_num_8k_14_8	0xA123
#define	dagc1_accumulate_num_8k_14_8_pos 0
#define	dagc1_accumulate_num_8k_14_8_len 7
#define	dagc1_accumulate_num_8k_14_8_lsb 8
#define xd_p_dagc1_desired_level_0	0xA123
#define	dagc1_desired_level_0_pos 7
#define	dagc1_desired_level_0_len 1
#define	dagc1_desired_level_0_lsb 0
#define xd_p_dagc1_desired_level_8_1	0xA124
#define	dagc1_desired_level_8_1_pos 0
#define	dagc1_desired_level_8_1_len 8
#define	dagc1_desired_level_8_1_lsb 1
#define xd_p_dagc1_apply_delay	0xA125
#define	dagc1_apply_delay_pos 0
#define	dagc1_apply_delay_len 7
#define	dagc1_apply_delay_lsb 0
#define xd_p_dagc1_bypass_scale_ctl	0xA126
#define	dagc1_bypass_scale_ctl_pos 0
#define	dagc1_bypass_scale_ctl_len 2
#define	dagc1_bypass_scale_ctl_lsb 0
#define xd_p_reg_dagc1_in_sat_cnt_7_0	0xA127
#define	reg_dagc1_in_sat_cnt_7_0_pos 0
#define	reg_dagc1_in_sat_cnt_7_0_len 8
#define	reg_dagc1_in_sat_cnt_7_0_lsb 0
#define xd_p_reg_dagc1_in_sat_cnt_15_8	0xA128
#define	reg_dagc1_in_sat_cnt_15_8_pos 0
#define	reg_dagc1_in_sat_cnt_15_8_len 8
#define	reg_dagc1_in_sat_cnt_15_8_lsb 8
#define xd_p_reg_dagc1_in_sat_cnt_23_16	0xA129
#define	reg_dagc1_in_sat_cnt_23_16_pos 0
#define	reg_dagc1_in_sat_cnt_23_16_len 8
#define	reg_dagc1_in_sat_cnt_23_16_lsb 16
#define xd_p_reg_dagc1_in_sat_cnt_31_24	0xA12A
#define	reg_dagc1_in_sat_cnt_31_24_pos 0
#define	reg_dagc1_in_sat_cnt_31_24_len 8
#define	reg_dagc1_in_sat_cnt_31_24_lsb 24
#define xd_p_reg_dagc1_out_sat_cnt_7_0	0xA12B
#define	reg_dagc1_out_sat_cnt_7_0_pos 0
#define	reg_dagc1_out_sat_cnt_7_0_len 8
#define	reg_dagc1_out_sat_cnt_7_0_lsb 0
#define xd_p_reg_dagc1_out_sat_cnt_15_8	0xA12C
#define	reg_dagc1_out_sat_cnt_15_8_pos 0
#define	reg_dagc1_out_sat_cnt_15_8_len 8
#define	reg_dagc1_out_sat_cnt_15_8_lsb 8
#define xd_p_reg_dagc1_out_sat_cnt_23_16	0xA12D
#define	reg_dagc1_out_sat_cnt_23_16_pos 0
#define	reg_dagc1_out_sat_cnt_23_16_len 8
#define	reg_dagc1_out_sat_cnt_23_16_lsb 16
#define xd_p_reg_dagc1_out_sat_cnt_31_24	0xA12E
#define	reg_dagc1_out_sat_cnt_31_24_pos 0
#define	reg_dagc1_out_sat_cnt_31_24_len 8
#define	reg_dagc1_out_sat_cnt_31_24_lsb 24
#define xd_r_dagc1_multiplier_7_0	0xA136
#define	dagc1_multiplier_7_0_pos 0
#define	dagc1_multiplier_7_0_len 8
#define	dagc1_multiplier_7_0_lsb 0
#define xd_r_dagc1_multiplier_15_8	0xA137
#define	dagc1_multiplier_15_8_pos 0
#define	dagc1_multiplier_15_8_len 8
#define	dagc1_multiplier_15_8_lsb 8
#define xd_r_dagc1_right_shift_bits	0xA138
#define	dagc1_right_shift_bits_pos 0
#define	dagc1_right_shift_bits_len 4
#define	dagc1_right_shift_bits_lsb 0
#define xd_p_reg_bfs_fcw_7_0	0xA140
#define	reg_bfs_fcw_7_0_pos 0
#define	reg_bfs_fcw_7_0_len 8
#define	reg_bfs_fcw_7_0_lsb 0
#define xd_p_reg_bfs_fcw_15_8	0xA141
#define	reg_bfs_fcw_15_8_pos 0
#define	reg_bfs_fcw_15_8_len 8
#define	reg_bfs_fcw_15_8_lsb 8
#define xd_p_reg_bfs_fcw_22_16	0xA142
#define	reg_bfs_fcw_22_16_pos 0
#define	reg_bfs_fcw_22_16_len 7
#define	reg_bfs_fcw_22_16_lsb 16
#define xd_p_reg_antif_sf_7_0	0xA144
#define	reg_antif_sf_7_0_pos 0
#define	reg_antif_sf_7_0_len 8
#define	reg_antif_sf_7_0_lsb 0
#define xd_p_reg_antif_sf_11_8	0xA145
#define	reg_antif_sf_11_8_pos 0
#define	reg_antif_sf_11_8_len 4
#define	reg_antif_sf_11_8_lsb 8
#define xd_r_bfs_fcw_q_7_0	0xA150
#define	bfs_fcw_q_7_0_pos 0
#define	bfs_fcw_q_7_0_len 8
#define	bfs_fcw_q_7_0_lsb 0
#define xd_r_bfs_fcw_q_15_8	0xA151
#define	bfs_fcw_q_15_8_pos 0
#define	bfs_fcw_q_15_8_len 8
#define	bfs_fcw_q_15_8_lsb 8
#define xd_r_bfs_fcw_q_22_16	0xA152
#define	bfs_fcw_q_22_16_pos 0
#define	bfs_fcw_q_22_16_len 7
#define	bfs_fcw_q_22_16_lsb 16
#define xd_p_reg_dca_enu	0xA160
#define	reg_dca_enu_pos 0
#define	reg_dca_enu_len 1
#define	reg_dca_enu_lsb 0
#define xd_p_reg_dca_enl	0xA160
#define	reg_dca_enl_pos 1
#define	reg_dca_enl_len 1
#define	reg_dca_enl_lsb 0
#define xd_p_reg_dca_lower_chip	0xA160
#define	reg_dca_lower_chip_pos 2
#define	reg_dca_lower_chip_len 1
#define	reg_dca_lower_chip_lsb 0
#define xd_p_reg_dca_upper_chip	0xA160
#define	reg_dca_upper_chip_pos 3
#define	reg_dca_upper_chip_len 1
#define	reg_dca_upper_chip_lsb 0
#define xd_p_reg_dca_platch	0xA160
#define	reg_dca_platch_pos 4
#define	reg_dca_platch_len 1
#define	reg_dca_platch_lsb 0
#define xd_p_reg_dca_th	0xA161
#define	reg_dca_th_pos 0
#define	reg_dca_th_len 5
#define	reg_dca_th_lsb 0
#define xd_p_reg_dca_scale	0xA162
#define	reg_dca_scale_pos 0
#define	reg_dca_scale_len 4
#define	reg_dca_scale_lsb 0
#define xd_p_reg_dca_tone_7_0	0xA163
#define	reg_dca_tone_7_0_pos 0
#define	reg_dca_tone_7_0_len 8
#define	reg_dca_tone_7_0_lsb 0
#define xd_p_reg_dca_tone_12_8	0xA164
#define	reg_dca_tone_12_8_pos 0
#define	reg_dca_tone_12_8_len 5
#define	reg_dca_tone_12_8_lsb 8
#define xd_p_reg_dca_time_7_0	0xA165
#define	reg_dca_time_7_0_pos 0
#define	reg_dca_time_7_0_len 8
#define	reg_dca_time_7_0_lsb 0
#define xd_p_reg_dca_time_15_8	0xA166
#define	reg_dca_time_15_8_pos 0
#define	reg_dca_time_15_8_len 8
#define	reg_dca_time_15_8_lsb 8
#define xd_r_dcasm	0xA167
#define	dcasm_pos 0
#define	dcasm_len 3
#define	dcasm_lsb 0
#define xd_p_reg_qnt_valuew_7_0	0xA168
#define	reg_qnt_valuew_7_0_pos 0
#define	reg_qnt_valuew_7_0_len 8
#define	reg_qnt_valuew_7_0_lsb 0
#define xd_p_reg_qnt_valuew_10_8	0xA169
#define	reg_qnt_valuew_10_8_pos 0
#define	reg_qnt_valuew_10_8_len 3
#define	reg_qnt_valuew_10_8_lsb 8
#define xd_p_dca_sbx_gain_diff_7_0	0xA16A
#define	dca_sbx_gain_diff_7_0_pos 0
#define	dca_sbx_gain_diff_7_0_len 8
#define	dca_sbx_gain_diff_7_0_lsb 0
#define xd_p_dca_sbx_gain_diff_9_8	0xA16B
#define	dca_sbx_gain_diff_9_8_pos 0
#define	dca_sbx_gain_diff_9_8_len 2
#define	dca_sbx_gain_diff_9_8_lsb 8
#define xd_p_reg_dca_stand_alone	0xA16C
#define	reg_dca_stand_alone_pos 0
#define	reg_dca_stand_alone_len 1
#define	reg_dca_stand_alone_lsb 0
#define xd_p_reg_dca_upper_out_en	0xA16C
#define	reg_dca_upper_out_en_pos 1
#define	reg_dca_upper_out_en_len 1
#define	reg_dca_upper_out_en_lsb 0
#define xd_p_reg_dca_rc_en	0xA16C
#define	reg_dca_rc_en_pos 2
#define	reg_dca_rc_en_len 1
#define	reg_dca_rc_en_lsb 0
#define xd_p_reg_dca_retrain_send	0xA16C
#define	reg_dca_retrain_send_pos 3
#define	reg_dca_retrain_send_len 1
#define	reg_dca_retrain_send_lsb 0
#define xd_p_reg_dca_retrain_rec	0xA16C
#define	reg_dca_retrain_rec_pos 4
#define	reg_dca_retrain_rec_len 1
#define	reg_dca_retrain_rec_lsb 0
#define xd_p_reg_dca_api_tpsrdy	0xA16C
#define	reg_dca_api_tpsrdy_pos 5
#define	reg_dca_api_tpsrdy_len 1
#define	reg_dca_api_tpsrdy_lsb 0
#define xd_p_reg_dca_symbol_gap	0xA16D
#define	reg_dca_symbol_gap_pos 0
#define	reg_dca_symbol_gap_len 4
#define	reg_dca_symbol_gap_lsb 0
#define xd_p_reg_qnt_nfvaluew_7_0	0xA16E
#define	reg_qnt_nfvaluew_7_0_pos 0
#define	reg_qnt_nfvaluew_7_0_len 8
#define	reg_qnt_nfvaluew_7_0_lsb 0
#define xd_p_reg_qnt_nfvaluew_10_8	0xA16F
#define	reg_qnt_nfvaluew_10_8_pos 0
#define	reg_qnt_nfvaluew_10_8_len 3
#define	reg_qnt_nfvaluew_10_8_lsb 8
#define xd_p_reg_qnt_flatness_thr_7_0	0xA170
#define	reg_qnt_flatness_thr_7_0_pos 0
#define	reg_qnt_flatness_thr_7_0_len 8
#define	reg_qnt_flatness_thr_7_0_lsb 0
#define xd_p_reg_qnt_flatness_thr_9_8	0xA171
#define	reg_qnt_flatness_thr_9_8_pos 0
#define	reg_qnt_flatness_thr_9_8_len 2
#define	reg_qnt_flatness_thr_9_8_lsb 8
#define xd_p_reg_dca_tone_idx_5_0	0xA171
#define	reg_dca_tone_idx_5_0_pos 2
#define	reg_dca_tone_idx_5_0_len 6
#define	reg_dca_tone_idx_5_0_lsb 0
#define xd_p_reg_dca_tone_idx_12_6	0xA172
#define	reg_dca_tone_idx_12_6_pos 0
#define	reg_dca_tone_idx_12_6_len 7
#define	reg_dca_tone_idx_12_6_lsb 6
#define xd_p_reg_dca_data_vld	0xA173
#define	reg_dca_data_vld_pos 0
#define	reg_dca_data_vld_len 1
#define	reg_dca_data_vld_lsb 0
#define xd_p_reg_dca_read_update	0xA173
#define	reg_dca_read_update_pos 1
#define	reg_dca_read_update_len 1
#define	reg_dca_read_update_lsb 0
#define xd_r_reg_dca_data_re_5_0	0xA173
#define	reg_dca_data_re_5_0_pos 2
#define	reg_dca_data_re_5_0_len 6
#define	reg_dca_data_re_5_0_lsb 0
#define xd_r_reg_dca_data_re_10_6	0xA174
#define	reg_dca_data_re_10_6_pos 0
#define	reg_dca_data_re_10_6_len 5
#define	reg_dca_data_re_10_6_lsb 6
#define xd_r_reg_dca_data_im_7_0	0xA175
#define	reg_dca_data_im_7_0_pos 0
#define	reg_dca_data_im_7_0_len 8
#define	reg_dca_data_im_7_0_lsb 0
#define xd_r_reg_dca_data_im_10_8	0xA176
#define	reg_dca_data_im_10_8_pos 0
#define	reg_dca_data_im_10_8_len 3
#define	reg_dca_data_im_10_8_lsb 8
#define xd_r_reg_dca_data_h2_7_0	0xA178
#define	reg_dca_data_h2_7_0_pos 0
#define	reg_dca_data_h2_7_0_len 8
#define	reg_dca_data_h2_7_0_lsb 0
#define xd_r_reg_dca_data_h2_9_8	0xA179
#define	reg_dca_data_h2_9_8_pos 0
#define	reg_dca_data_h2_9_8_len 2
#define	reg_dca_data_h2_9_8_lsb 8
#define xd_p_reg_f_adc_7_0	0xA180
#define	reg_f_adc_7_0_pos 0
#define	reg_f_adc_7_0_len 8
#define	reg_f_adc_7_0_lsb 0
#define xd_p_reg_f_adc_15_8	0xA181
#define	reg_f_adc_15_8_pos 0
#define	reg_f_adc_15_8_len 8
#define	reg_f_adc_15_8_lsb 8
#define xd_p_reg_f_adc_23_16	0xA182
#define	reg_f_adc_23_16_pos 0
#define	reg_f_adc_23_16_len 8
#define	reg_f_adc_23_16_lsb 16
#define xd_r_intp_mu_7_0	0xA190
#define	intp_mu_7_0_pos 0
#define	intp_mu_7_0_len 8
#define	intp_mu_7_0_lsb 0
#define xd_r_intp_mu_15_8	0xA191
#define	intp_mu_15_8_pos 0
#define	intp_mu_15_8_len 8
#define	intp_mu_15_8_lsb 8
#define xd_r_intp_mu_19_16	0xA192
#define	intp_mu_19_16_pos 0
#define	intp_mu_19_16_len 4
#define	intp_mu_19_16_lsb 16
#define xd_p_reg_agc_rst	0xA1A0
#define	reg_agc_rst_pos 0
#define	reg_agc_rst_len 1
#define	reg_agc_rst_lsb 0
#define xd_p_rf_agc_en	0xA1A0
#define	rf_agc_en_pos 1
#define	rf_agc_en_len 1
#define	rf_agc_en_lsb 0
#define xd_p_rf_agc_dis	0xA1A0
#define	rf_agc_dis_pos 2
#define	rf_agc_dis_len 1
#define	rf_agc_dis_lsb 0
#define xd_p_if_agc_rst	0xA1A0
#define	if_agc_rst_pos 3
#define	if_agc_rst_len 1
#define	if_agc_rst_lsb 0
#define xd_p_if_agc_en	0xA1A0
#define	if_agc_en_pos 4
#define	if_agc_en_len 1
#define	if_agc_en_lsb 0
#define xd_p_if_agc_dis	0xA1A0
#define	if_agc_dis_pos 5
#define	if_agc_dis_len 1
#define	if_agc_dis_lsb 0
#define xd_p_agc_lock	0xA1A0
#define	agc_lock_pos 6
#define	agc_lock_len 1
#define	agc_lock_lsb 0
#define xd_p_reg_tinr_rst	0xA1A1
#define	reg_tinr_rst_pos 0
#define	reg_tinr_rst_len 1
#define	reg_tinr_rst_lsb 0
#define xd_p_reg_tinr_en	0xA1A1
#define	reg_tinr_en_pos 1
#define	reg_tinr_en_len 1
#define	reg_tinr_en_lsb 0
#define xd_p_reg_ccifs_en	0xA1A2
#define	reg_ccifs_en_pos 0
#define	reg_ccifs_en_len 1
#define	reg_ccifs_en_lsb 0
#define xd_p_reg_ccifs_dis	0xA1A2
#define	reg_ccifs_dis_pos 1
#define	reg_ccifs_dis_len 1
#define	reg_ccifs_dis_lsb 0
#define xd_p_reg_ccifs_rst	0xA1A2
#define	reg_ccifs_rst_pos 2
#define	reg_ccifs_rst_len 1
#define	reg_ccifs_rst_lsb 0
#define xd_p_reg_ccifs_byp	0xA1A2
#define	reg_ccifs_byp_pos 3
#define	reg_ccifs_byp_len 1
#define	reg_ccifs_byp_lsb 0
#define xd_p_reg_ccif_en	0xA1A3
#define	reg_ccif_en_pos 0
#define	reg_ccif_en_len 1
#define	reg_ccif_en_lsb 0
#define xd_p_reg_ccif_dis	0xA1A3
#define	reg_ccif_dis_pos 1
#define	reg_ccif_dis_len 1
#define	reg_ccif_dis_lsb 0
#define xd_p_reg_ccif_rst	0xA1A3
#define	reg_ccif_rst_pos 2
#define	reg_ccif_rst_len 1
#define	reg_ccif_rst_lsb 0
#define xd_p_reg_ccif_byp	0xA1A3
#define	reg_ccif_byp_pos 3
#define	reg_ccif_byp_len 1
#define	reg_ccif_byp_lsb 0
#define xd_p_dagc1_rst	0xA1A4
#define	dagc1_rst_pos 0
#define	dagc1_rst_len 1
#define	dagc1_rst_lsb 0
#define xd_p_dagc1_en	0xA1A4
#define	dagc1_en_pos 1
#define	dagc1_en_len 1
#define	dagc1_en_lsb 0
#define xd_p_dagc1_mode	0xA1A4
#define	dagc1_mode_pos 2
#define	dagc1_mode_len 2
#define	dagc1_mode_lsb 0
#define xd_p_dagc1_done	0xA1A4
#define	dagc1_done_pos 4
#define	dagc1_done_len 1
#define	dagc1_done_lsb 0
#define xd_p_ccid_rst	0xA1A5
#define	ccid_rst_pos 0
#define	ccid_rst_len 1
#define	ccid_rst_lsb 0
#define xd_p_ccid_en	0xA1A5
#define	ccid_en_pos 1
#define	ccid_en_len 1
#define	ccid_en_lsb 0
#define xd_p_ccid_mode	0xA1A5
#define	ccid_mode_pos 2
#define	ccid_mode_len 2
#define	ccid_mode_lsb 0
#define xd_p_ccid_done	0xA1A5
#define	ccid_done_pos 4
#define	ccid_done_len 1
#define	ccid_done_lsb 0
#define xd_r_ccid_deted	0xA1A5
#define	ccid_deted_pos 5
#define	ccid_deted_len 1
#define	ccid_deted_lsb 0
#define xd_p_ccid2_en	0xA1A5
#define	ccid2_en_pos 6
#define	ccid2_en_len 1
#define	ccid2_en_lsb 0
#define xd_p_ccid2_done	0xA1A5
#define	ccid2_done_pos 7
#define	ccid2_done_len 1
#define	ccid2_done_lsb 0
#define xd_p_reg_bfs_en	0xA1A6
#define	reg_bfs_en_pos 0
#define	reg_bfs_en_len 1
#define	reg_bfs_en_lsb 0
#define xd_p_reg_bfs_dis	0xA1A6
#define	reg_bfs_dis_pos 1
#define	reg_bfs_dis_len 1
#define	reg_bfs_dis_lsb 0
#define xd_p_reg_bfs_rst	0xA1A6
#define	reg_bfs_rst_pos 2
#define	reg_bfs_rst_len 1
#define	reg_bfs_rst_lsb 0
#define xd_p_reg_bfs_byp	0xA1A6
#define	reg_bfs_byp_pos 3
#define	reg_bfs_byp_len 1
#define	reg_bfs_byp_lsb 0
#define xd_p_reg_antif_en	0xA1A7
#define	reg_antif_en_pos 0
#define	reg_antif_en_len 1
#define	reg_antif_en_lsb 0
#define xd_p_reg_antif_dis	0xA1A7
#define	reg_antif_dis_pos 1
#define	reg_antif_dis_len 1
#define	reg_antif_dis_lsb 0
#define xd_p_reg_antif_rst	0xA1A7
#define	reg_antif_rst_pos 2
#define	reg_antif_rst_len 1
#define	reg_antif_rst_lsb 0
#define xd_p_reg_antif_byp	0xA1A7
#define	reg_antif_byp_pos 3
#define	reg_antif_byp_len 1
#define	reg_antif_byp_lsb 0
#define xd_p_intp_en	0xA1A8
#define	intp_en_pos 0
#define	intp_en_len 1
#define	intp_en_lsb 0
#define xd_p_intp_dis	0xA1A8
#define	intp_dis_pos 1
#define	intp_dis_len 1
#define	intp_dis_lsb 0
#define xd_p_intp_rst	0xA1A8
#define	intp_rst_pos 2
#define	intp_rst_len 1
#define	intp_rst_lsb 0
#define xd_p_intp_byp	0xA1A8
#define	intp_byp_pos 3
#define	intp_byp_len 1
#define	intp_byp_lsb 0
#define xd_p_reg_acif_en	0xA1A9
#define	reg_acif_en_pos 0
#define	reg_acif_en_len 1
#define	reg_acif_en_lsb 0
#define xd_p_reg_acif_dis	0xA1A9
#define	reg_acif_dis_pos 1
#define	reg_acif_dis_len 1
#define	reg_acif_dis_lsb 0
#define xd_p_reg_acif_rst	0xA1A9
#define	reg_acif_rst_pos 2
#define	reg_acif_rst_len 1
#define	reg_acif_rst_lsb 0
#define xd_p_reg_acif_byp	0xA1A9
#define	reg_acif_byp_pos 3
#define	reg_acif_byp_len 1
#define	reg_acif_byp_lsb 0
#define xd_p_reg_acif_sync_mode	0xA1A9
#define	reg_acif_sync_mode_pos 4
#define	reg_acif_sync_mode_len 1
#define	reg_acif_sync_mode_lsb 0
#define xd_p_dagc2_rst	0xA1AA
#define	dagc2_rst_pos 0
#define	dagc2_rst_len 1
#define	dagc2_rst_lsb 0
#define xd_p_dagc2_en	0xA1AA
#define	dagc2_en_pos 1
#define	dagc2_en_len 1
#define	dagc2_en_lsb 0
#define xd_p_dagc2_mode	0xA1AA
#define	dagc2_mode_pos 2
#define	dagc2_mode_len 2
#define	dagc2_mode_lsb 0
#define xd_p_dagc2_done	0xA1AA
#define	dagc2_done_pos 4
#define	dagc2_done_len 1
#define	dagc2_done_lsb 0
#define xd_p_reg_dca_en	0xA1AB
#define	reg_dca_en_pos 0
#define	reg_dca_en_len 1
#define	reg_dca_en_lsb 0
#define xd_p_dagc2_accumulate_num_2k_7_0	0xA1C0
#define	dagc2_accumulate_num_2k_7_0_pos 0
#define	dagc2_accumulate_num_2k_7_0_len 8
#define	dagc2_accumulate_num_2k_7_0_lsb 0
#define xd_p_dagc2_accumulate_num_2k_12_8	0xA1C1
#define	dagc2_accumulate_num_2k_12_8_pos 0
#define	dagc2_accumulate_num_2k_12_8_len 5
#define	dagc2_accumulate_num_2k_12_8_lsb 8
#define xd_p_dagc2_accumulate_num_8k_7_0	0xA1C2
#define	dagc2_accumulate_num_8k_7_0_pos 0
#define	dagc2_accumulate_num_8k_7_0_len 8
#define	dagc2_accumulate_num_8k_7_0_lsb 0
#define xd_p_dagc2_accumulate_num_8k_12_8	0xA1C3
#define	dagc2_accumulate_num_8k_12_8_pos 0
#define	dagc2_accumulate_num_8k_12_8_len 5
#define	dagc2_accumulate_num_8k_12_8_lsb 8
#define xd_p_dagc2_desired_level_2_0	0xA1C3
#define	dagc2_desired_level_2_0_pos 5
#define	dagc2_desired_level_2_0_len 3
#define	dagc2_desired_level_2_0_lsb 0
#define xd_p_dagc2_desired_level_8_3	0xA1C4
#define	dagc2_desired_level_8_3_pos 0
#define	dagc2_desired_level_8_3_len 6
#define	dagc2_desired_level_8_3_lsb 3
#define xd_p_dagc2_apply_delay	0xA1C5
#define	dagc2_apply_delay_pos 0
#define	dagc2_apply_delay_len 7
#define	dagc2_apply_delay_lsb 0
#define xd_p_dagc2_bypass_scale_ctl	0xA1C6
#define	dagc2_bypass_scale_ctl_pos 0
#define	dagc2_bypass_scale_ctl_len 3
#define	dagc2_bypass_scale_ctl_lsb 0
#define xd_p_dagc2_programmable_shift1	0xA1C7
#define	dagc2_programmable_shift1_pos 0
#define	dagc2_programmable_shift1_len 8
#define	dagc2_programmable_shift1_lsb 0
#define xd_p_dagc2_programmable_shift2	0xA1C8
#define	dagc2_programmable_shift2_pos 0
#define	dagc2_programmable_shift2_len 8
#define	dagc2_programmable_shift2_lsb 0
#define xd_p_reg_dagc2_in_sat_cnt_7_0	0xA1C9
#define	reg_dagc2_in_sat_cnt_7_0_pos 0
#define	reg_dagc2_in_sat_cnt_7_0_len 8
#define	reg_dagc2_in_sat_cnt_7_0_lsb 0
#define xd_p_reg_dagc2_in_sat_cnt_15_8	0xA1CA
#define	reg_dagc2_in_sat_cnt_15_8_pos 0
#define	reg_dagc2_in_sat_cnt_15_8_len 8
#define	reg_dagc2_in_sat_cnt_15_8_lsb 8
#define xd_p_reg_dagc2_in_sat_cnt_23_16	0xA1CB
#define	reg_dagc2_in_sat_cnt_23_16_pos 0
#define	reg_dagc2_in_sat_cnt_23_16_len 8
#define	reg_dagc2_in_sat_cnt_23_16_lsb 16
#define xd_p_reg_dagc2_in_sat_cnt_31_24	0xA1CC
#define	reg_dagc2_in_sat_cnt_31_24_pos 0
#define	reg_dagc2_in_sat_cnt_31_24_len 8
#define	reg_dagc2_in_sat_cnt_31_24_lsb 24
#define xd_p_reg_dagc2_out_sat_cnt_7_0	0xA1CD
#define	reg_dagc2_out_sat_cnt_7_0_pos 0
#define	reg_dagc2_out_sat_cnt_7_0_len 8
#define	reg_dagc2_out_sat_cnt_7_0_lsb 0
#define xd_p_reg_dagc2_out_sat_cnt_15_8	0xA1CE
#define	reg_dagc2_out_sat_cnt_15_8_pos 0
#define	reg_dagc2_out_sat_cnt_15_8_len 8
#define	reg_dagc2_out_sat_cnt_15_8_lsb 8
#define xd_p_reg_dagc2_out_sat_cnt_23_16	0xA1CF
#define	reg_dagc2_out_sat_cnt_23_16_pos 0
#define	reg_dagc2_out_sat_cnt_23_16_len 8
#define	reg_dagc2_out_sat_cnt_23_16_lsb 16
#define xd_p_reg_dagc2_out_sat_cnt_31_24	0xA1D0
#define	reg_dagc2_out_sat_cnt_31_24_pos 0
#define	reg_dagc2_out_sat_cnt_31_24_len 8
#define	reg_dagc2_out_sat_cnt_31_24_lsb 24
#define xd_r_dagc2_multiplier_7_0	0xA1D6
#define	dagc2_multiplier_7_0_pos 0
#define	dagc2_multiplier_7_0_len 8
#define	dagc2_multiplier_7_0_lsb 0
#define xd_r_dagc2_multiplier_15_8	0xA1D7
#define	dagc2_multiplier_15_8_pos 0
#define	dagc2_multiplier_15_8_len 8
#define	dagc2_multiplier_15_8_lsb 8
#define xd_r_dagc2_right_shift_bits	0xA1D8
#define	dagc2_right_shift_bits_pos 0
#define	dagc2_right_shift_bits_len 4
#define	dagc2_right_shift_bits_lsb 0
#define xd_p_cfoe_NS_coeff1_7_0	0xA200
#define	cfoe_NS_coeff1_7_0_pos 0
#define	cfoe_NS_coeff1_7_0_len 8
#define	cfoe_NS_coeff1_7_0_lsb 0
#define xd_p_cfoe_NS_coeff1_15_8	0xA201
#define	cfoe_NS_coeff1_15_8_pos 0
#define	cfoe_NS_coeff1_15_8_len 8
#define	cfoe_NS_coeff1_15_8_lsb 8
#define xd_p_cfoe_NS_coeff1_23_16	0xA202
#define	cfoe_NS_coeff1_23_16_pos 0
#define	cfoe_NS_coeff1_23_16_len 8
#define	cfoe_NS_coeff1_23_16_lsb 16
#define xd_p_cfoe_NS_coeff1_25_24	0xA203
#define	cfoe_NS_coeff1_25_24_pos 0
#define	cfoe_NS_coeff1_25_24_len 2
#define	cfoe_NS_coeff1_25_24_lsb 24
#define xd_p_cfoe_NS_coeff2_5_0	0xA203
#define	cfoe_NS_coeff2_5_0_pos 2
#define	cfoe_NS_coeff2_5_0_len 6
#define	cfoe_NS_coeff2_5_0_lsb 0
#define xd_p_cfoe_NS_coeff2_13_6	0xA204
#define	cfoe_NS_coeff2_13_6_pos 0
#define	cfoe_NS_coeff2_13_6_len 8
#define	cfoe_NS_coeff2_13_6_lsb 6
#define xd_p_cfoe_NS_coeff2_21_14	0xA205
#define	cfoe_NS_coeff2_21_14_pos 0
#define	cfoe_NS_coeff2_21_14_len 8
#define	cfoe_NS_coeff2_21_14_lsb 14
#define xd_p_cfoe_NS_coeff2_24_22	0xA206
#define	cfoe_NS_coeff2_24_22_pos 0
#define	cfoe_NS_coeff2_24_22_len 3
#define	cfoe_NS_coeff2_24_22_lsb 22
#define xd_p_cfoe_lf_c1_4_0	0xA206
#define	cfoe_lf_c1_4_0_pos 3
#define	cfoe_lf_c1_4_0_len 5
#define	cfoe_lf_c1_4_0_lsb 0
#define xd_p_cfoe_lf_c1_12_5	0xA207
#define	cfoe_lf_c1_12_5_pos 0
#define	cfoe_lf_c1_12_5_len 8
#define	cfoe_lf_c1_12_5_lsb 5
#define xd_p_cfoe_lf_c1_20_13	0xA208
#define	cfoe_lf_c1_20_13_pos 0
#define	cfoe_lf_c1_20_13_len 8
#define	cfoe_lf_c1_20_13_lsb 13
#define xd_p_cfoe_lf_c1_25_21	0xA209
#define	cfoe_lf_c1_25_21_pos 0
#define	cfoe_lf_c1_25_21_len 5
#define	cfoe_lf_c1_25_21_lsb 21
#define xd_p_cfoe_lf_c2_2_0	0xA209
#define	cfoe_lf_c2_2_0_pos 5
#define	cfoe_lf_c2_2_0_len 3
#define	cfoe_lf_c2_2_0_lsb 0
#define xd_p_cfoe_lf_c2_10_3	0xA20A
#define	cfoe_lf_c2_10_3_pos 0
#define	cfoe_lf_c2_10_3_len 8
#define	cfoe_lf_c2_10_3_lsb 3
#define xd_p_cfoe_lf_c2_18_11	0xA20B
#define	cfoe_lf_c2_18_11_pos 0
#define	cfoe_lf_c2_18_11_len 8
#define	cfoe_lf_c2_18_11_lsb 11
#define xd_p_cfoe_lf_c2_25_19	0xA20C
#define	cfoe_lf_c2_25_19_pos 0
#define	cfoe_lf_c2_25_19_len 7
#define	cfoe_lf_c2_25_19_lsb 19
#define xd_p_cfoe_ifod_7_0	0xA20D
#define	cfoe_ifod_7_0_pos 0
#define	cfoe_ifod_7_0_len 8
#define	cfoe_ifod_7_0_lsb 0
#define xd_p_cfoe_ifod_10_8	0xA20E
#define	cfoe_ifod_10_8_pos 0
#define	cfoe_ifod_10_8_len 3
#define	cfoe_ifod_10_8_lsb 8
#define xd_p_cfoe_Divg_ctr_th	0xA20E
#define	cfoe_Divg_ctr_th_pos 4
#define	cfoe_Divg_ctr_th_len 4
#define	cfoe_Divg_ctr_th_lsb 0
#define xd_p_cfoe_FOT_divg_th	0xA20F
#define	cfoe_FOT_divg_th_pos 0
#define	cfoe_FOT_divg_th_len 8
#define	cfoe_FOT_divg_th_lsb 0
#define xd_p_cfoe_FOT_cnvg_th	0xA210
#define	cfoe_FOT_cnvg_th_pos 0
#define	cfoe_FOT_cnvg_th_len 8
#define	cfoe_FOT_cnvg_th_lsb 0
#define xd_p_reg_cfoe_offset_7_0	0xA211
#define	reg_cfoe_offset_7_0_pos 0
#define	reg_cfoe_offset_7_0_len 8
#define	reg_cfoe_offset_7_0_lsb 0
#define xd_p_reg_cfoe_offset_9_8	0xA212
#define	reg_cfoe_offset_9_8_pos 0
#define	reg_cfoe_offset_9_8_len 2
#define	reg_cfoe_offset_9_8_lsb 8
#define xd_p_reg_cfoe_ifoe_sign_corr	0xA212
#define	reg_cfoe_ifoe_sign_corr_pos 2
#define	reg_cfoe_ifoe_sign_corr_len 1
#define	reg_cfoe_ifoe_sign_corr_lsb 0
#define xd_r_cfoe_fot_LF_output_7_0	0xA218
#define	cfoe_fot_LF_output_7_0_pos 0
#define	cfoe_fot_LF_output_7_0_len 8
#define	cfoe_fot_LF_output_7_0_lsb 0
#define xd_r_cfoe_fot_LF_output_15_8	0xA219
#define	cfoe_fot_LF_output_15_8_pos 0
#define	cfoe_fot_LF_output_15_8_len 8
#define	cfoe_fot_LF_output_15_8_lsb 8
#define xd_r_cfoe_ifo_metric_7_0	0xA21A
#define	cfoe_ifo_metric_7_0_pos 0
#define	cfoe_ifo_metric_7_0_len 8
#define	cfoe_ifo_metric_7_0_lsb 0
#define xd_r_cfoe_ifo_metric_15_8	0xA21B
#define	cfoe_ifo_metric_15_8_pos 0
#define	cfoe_ifo_metric_15_8_len 8
#define	cfoe_ifo_metric_15_8_lsb 8
#define xd_r_cfoe_ifo_metric_23_16	0xA21C
#define	cfoe_ifo_metric_23_16_pos 0
#define	cfoe_ifo_metric_23_16_len 8
#define	cfoe_ifo_metric_23_16_lsb 16
#define xd_p_ste_Nu	0xA220
#define	ste_Nu_pos 0
#define	ste_Nu_len 2
#define	ste_Nu_lsb 0
#define xd_p_ste_GI	0xA220
#define	ste_GI_pos 2
#define	ste_GI_len 3
#define	ste_GI_lsb 0
#define xd_p_ste_symbol_num	0xA221
#define	ste_symbol_num_pos 0
#define	ste_symbol_num_len 2
#define	ste_symbol_num_lsb 0
#define xd_p_ste_sample_num	0xA221
#define	ste_sample_num_pos 2
#define	ste_sample_num_len 2
#define	ste_sample_num_lsb 0
#define xd_p_reg_ste_buf_en	0xA221
#define	reg_ste_buf_en_pos 7
#define	reg_ste_buf_en_len 1
#define	reg_ste_buf_en_lsb 0
#define xd_p_ste_FFT_offset_7_0	0xA222
#define	ste_FFT_offset_7_0_pos 0
#define	ste_FFT_offset_7_0_len 8
#define	ste_FFT_offset_7_0_lsb 0
#define xd_p_ste_FFT_offset_11_8	0xA223
#define	ste_FFT_offset_11_8_pos 0
#define	ste_FFT_offset_11_8_len 4
#define	ste_FFT_offset_11_8_lsb 8
#define xd_p_reg_ste_tstmod	0xA223
#define	reg_ste_tstmod_pos 5
#define	reg_ste_tstmod_len 1
#define	reg_ste_tstmod_lsb 0
#define xd_p_ste_adv_start_7_0	0xA224
#define	ste_adv_start_7_0_pos 0
#define	ste_adv_start_7_0_len 8
#define	ste_adv_start_7_0_lsb 0
#define xd_p_ste_adv_start_10_8	0xA225
#define	ste_adv_start_10_8_pos 0
#define	ste_adv_start_10_8_len 3
#define	ste_adv_start_10_8_lsb 8
#define xd_p_ste_adv_stop	0xA226
#define	ste_adv_stop_pos 0
#define	ste_adv_stop_len 8
#define	ste_adv_stop_lsb 0
#define xd_r_ste_P_value_7_0	0xA228
#define	ste_P_value_7_0_pos 0
#define	ste_P_value_7_0_len 8
#define	ste_P_value_7_0_lsb 0
#define xd_r_ste_P_value_10_8	0xA229
#define	ste_P_value_10_8_pos 0
#define	ste_P_value_10_8_len 3
#define	ste_P_value_10_8_lsb 8
#define xd_r_ste_M_value_7_0	0xA22A
#define	ste_M_value_7_0_pos 0
#define	ste_M_value_7_0_len 8
#define	ste_M_value_7_0_lsb 0
#define xd_r_ste_M_value_10_8	0xA22B
#define	ste_M_value_10_8_pos 0
#define	ste_M_value_10_8_len 3
#define	ste_M_value_10_8_lsb 8
#define xd_r_ste_H1	0xA22C
#define	ste_H1_pos 0
#define	ste_H1_len 7
#define	ste_H1_lsb 0
#define xd_r_ste_H2	0xA22D
#define	ste_H2_pos 0
#define	ste_H2_len 7
#define	ste_H2_lsb 0
#define xd_r_ste_H3	0xA22E
#define	ste_H3_pos 0
#define	ste_H3_len 7
#define	ste_H3_lsb 0
#define xd_r_ste_H4	0xA22F
#define	ste_H4_pos 0
#define	ste_H4_len 7
#define	ste_H4_lsb 0
#define xd_r_ste_Corr_value_I_7_0	0xA230
#define	ste_Corr_value_I_7_0_pos 0
#define	ste_Corr_value_I_7_0_len 8
#define	ste_Corr_value_I_7_0_lsb 0
#define xd_r_ste_Corr_value_I_15_8	0xA231
#define	ste_Corr_value_I_15_8_pos 0
#define	ste_Corr_value_I_15_8_len 8
#define	ste_Corr_value_I_15_8_lsb 8
#define xd_r_ste_Corr_value_I_23_16	0xA232
#define	ste_Corr_value_I_23_16_pos 0
#define	ste_Corr_value_I_23_16_len 8
#define	ste_Corr_value_I_23_16_lsb 16
#define xd_r_ste_Corr_value_I_27_24	0xA233
#define	ste_Corr_value_I_27_24_pos 0
#define	ste_Corr_value_I_27_24_len 4
#define	ste_Corr_value_I_27_24_lsb 24
#define xd_r_ste_Corr_value_Q_7_0	0xA234
#define	ste_Corr_value_Q_7_0_pos 0
#define	ste_Corr_value_Q_7_0_len 8
#define	ste_Corr_value_Q_7_0_lsb 0
#define xd_r_ste_Corr_value_Q_15_8	0xA235
#define	ste_Corr_value_Q_15_8_pos 0
#define	ste_Corr_value_Q_15_8_len 8
#define	ste_Corr_value_Q_15_8_lsb 8
#define xd_r_ste_Corr_value_Q_23_16	0xA236
#define	ste_Corr_value_Q_23_16_pos 0
#define	ste_Corr_value_Q_23_16_len 8
#define	ste_Corr_value_Q_23_16_lsb 16
#define xd_r_ste_Corr_value_Q_27_24	0xA237
#define	ste_Corr_value_Q_27_24_pos 0
#define	ste_Corr_value_Q_27_24_len 4
#define	ste_Corr_value_Q_27_24_lsb 24
#define xd_r_ste_J_num_7_0	0xA238
#define	ste_J_num_7_0_pos 0
#define	ste_J_num_7_0_len 8
#define	ste_J_num_7_0_lsb 0
#define xd_r_ste_J_num_15_8	0xA239
#define	ste_J_num_15_8_pos 0
#define	ste_J_num_15_8_len 8
#define	ste_J_num_15_8_lsb 8
#define xd_r_ste_J_num_23_16	0xA23A
#define	ste_J_num_23_16_pos 0
#define	ste_J_num_23_16_len 8
#define	ste_J_num_23_16_lsb 16
#define xd_r_ste_J_num_31_24	0xA23B
#define	ste_J_num_31_24_pos 0
#define	ste_J_num_31_24_len 8
#define	ste_J_num_31_24_lsb 24
#define xd_r_ste_J_den_7_0	0xA23C
#define	ste_J_den_7_0_pos 0
#define	ste_J_den_7_0_len 8
#define	ste_J_den_7_0_lsb 0
#define xd_r_ste_J_den_15_8	0xA23D
#define	ste_J_den_15_8_pos 0
#define	ste_J_den_15_8_len 8
#define	ste_J_den_15_8_lsb 8
#define xd_r_ste_J_den_18_16	0xA23E
#define	ste_J_den_18_16_pos 0
#define	ste_J_den_18_16_len 3
#define	ste_J_den_18_16_lsb 16
#define xd_r_ste_Beacon_Indicator	0xA23E
#define	ste_Beacon_Indicator_pos 4
#define	ste_Beacon_Indicator_len 1
#define	ste_Beacon_Indicator_lsb 0
#define xd_r_tpsd_Frame_Num	0xA250
#define	tpsd_Frame_Num_pos 0
#define	tpsd_Frame_Num_len 2
#define	tpsd_Frame_Num_lsb 0
#define xd_r_tpsd_Constel	0xA250
#define	tpsd_Constel_pos 2
#define	tpsd_Constel_len 2
#define	tpsd_Constel_lsb 0
#define xd_r_tpsd_GI	0xA250
#define	tpsd_GI_pos 4
#define	tpsd_GI_len 2
#define	tpsd_GI_lsb 0
#define xd_r_tpsd_Mode	0xA250
#define	tpsd_Mode_pos 6
#define	tpsd_Mode_len 2
#define	tpsd_Mode_lsb 0
#define xd_r_tpsd_CR_HP	0xA251
#define	tpsd_CR_HP_pos 0
#define	tpsd_CR_HP_len 3
#define	tpsd_CR_HP_lsb 0
#define xd_r_tpsd_CR_LP	0xA251
#define	tpsd_CR_LP_pos 3
#define	tpsd_CR_LP_len 3
#define	tpsd_CR_LP_lsb 0
#define xd_r_tpsd_Hie	0xA252
#define	tpsd_Hie_pos 0
#define	tpsd_Hie_len 3
#define	tpsd_Hie_lsb 0
#define xd_r_tpsd_Res_Bits	0xA252
#define	tpsd_Res_Bits_pos 3
#define	tpsd_Res_Bits_len 5
#define	tpsd_Res_Bits_lsb 0
#define xd_r_tpsd_Res_Bits_0	0xA253
#define	tpsd_Res_Bits_0_pos 0
#define	tpsd_Res_Bits_0_len 1
#define	tpsd_Res_Bits_0_lsb 0
#define xd_r_tpsd_LengthInd	0xA253
#define	tpsd_LengthInd_pos 1
#define	tpsd_LengthInd_len 6
#define	tpsd_LengthInd_lsb 0
#define xd_r_tpsd_Cell_Id_7_0	0xA254
#define	tpsd_Cell_Id_7_0_pos 0
#define	tpsd_Cell_Id_7_0_len 8
#define	tpsd_Cell_Id_7_0_lsb 0
#define xd_r_tpsd_Cell_Id_15_8	0xA255
#define	tpsd_Cell_Id_15_8_pos 0
#define	tpsd_Cell_Id_15_8_len 8
#define	tpsd_Cell_Id_15_8_lsb 0
#define xd_p_reg_fft_mask_tone0_7_0	0xA260
#define	reg_fft_mask_tone0_7_0_pos 0
#define	reg_fft_mask_tone0_7_0_len 8
#define	reg_fft_mask_tone0_7_0_lsb 0
#define xd_p_reg_fft_mask_tone0_12_8	0xA261
#define	reg_fft_mask_tone0_12_8_pos 0
#define	reg_fft_mask_tone0_12_8_len 5
#define	reg_fft_mask_tone0_12_8_lsb 8
#define xd_p_reg_fft_mask_tone1_7_0	0xA262
#define	reg_fft_mask_tone1_7_0_pos 0
#define	reg_fft_mask_tone1_7_0_len 8
#define	reg_fft_mask_tone1_7_0_lsb 0
#define xd_p_reg_fft_mask_tone1_12_8	0xA263
#define	reg_fft_mask_tone1_12_8_pos 0
#define	reg_fft_mask_tone1_12_8_len 5
#define	reg_fft_mask_tone1_12_8_lsb 8
#define xd_p_reg_fft_mask_tone2_7_0	0xA264
#define	reg_fft_mask_tone2_7_0_pos 0
#define	reg_fft_mask_tone2_7_0_len 8
#define	reg_fft_mask_tone2_7_0_lsb 0
#define xd_p_reg_fft_mask_tone2_12_8	0xA265
#define	reg_fft_mask_tone2_12_8_pos 0
#define	reg_fft_mask_tone2_12_8_len 5
#define	reg_fft_mask_tone2_12_8_lsb 8
#define xd_p_reg_fft_mask_tone3_7_0	0xA266
#define	reg_fft_mask_tone3_7_0_pos 0
#define	reg_fft_mask_tone3_7_0_len 8
#define	reg_fft_mask_tone3_7_0_lsb 0
#define xd_p_reg_fft_mask_tone3_12_8	0xA267
#define	reg_fft_mask_tone3_12_8_pos 0
#define	reg_fft_mask_tone3_12_8_len 5
#define	reg_fft_mask_tone3_12_8_lsb 8
#define xd_p_reg_fft_mask_from0_7_0	0xA268
#define	reg_fft_mask_from0_7_0_pos 0
#define	reg_fft_mask_from0_7_0_len 8
#define	reg_fft_mask_from0_7_0_lsb 0
#define xd_p_reg_fft_mask_from0_12_8	0xA269
#define	reg_fft_mask_from0_12_8_pos 0
#define	reg_fft_mask_from0_12_8_len 5
#define	reg_fft_mask_from0_12_8_lsb 8
#define xd_p_reg_fft_mask_to0_7_0	0xA26A
#define	reg_fft_mask_to0_7_0_pos 0
#define	reg_fft_mask_to0_7_0_len 8
#define	reg_fft_mask_to0_7_0_lsb 0
#define xd_p_reg_fft_mask_to0_12_8	0xA26B
#define	reg_fft_mask_to0_12_8_pos 0
#define	reg_fft_mask_to0_12_8_len 5
#define	reg_fft_mask_to0_12_8_lsb 8
#define xd_p_reg_fft_mask_from1_7_0	0xA26C
#define	reg_fft_mask_from1_7_0_pos 0
#define	reg_fft_mask_from1_7_0_len 8
#define	reg_fft_mask_from1_7_0_lsb 0
#define xd_p_reg_fft_mask_from1_12_8	0xA26D
#define	reg_fft_mask_from1_12_8_pos 0
#define	reg_fft_mask_from1_12_8_len 5
#define	reg_fft_mask_from1_12_8_lsb 8
#define xd_p_reg_fft_mask_to1_7_0	0xA26E
#define	reg_fft_mask_to1_7_0_pos 0
#define	reg_fft_mask_to1_7_0_len 8
#define	reg_fft_mask_to1_7_0_lsb 0
#define xd_p_reg_fft_mask_to1_12_8	0xA26F
#define	reg_fft_mask_to1_12_8_pos 0
#define	reg_fft_mask_to1_12_8_len 5
#define	reg_fft_mask_to1_12_8_lsb 8
#define xd_p_reg_cge_idx0_7_0	0xA280
#define	reg_cge_idx0_7_0_pos 0
#define	reg_cge_idx0_7_0_len 8
#define	reg_cge_idx0_7_0_lsb 0
#define xd_p_reg_cge_idx0_12_8	0xA281
#define	reg_cge_idx0_12_8_pos 0
#define	reg_cge_idx0_12_8_len 5
#define	reg_cge_idx0_12_8_lsb 8
#define xd_p_reg_cge_idx1_7_0	0xA282
#define	reg_cge_idx1_7_0_pos 0
#define	reg_cge_idx1_7_0_len 8
#define	reg_cge_idx1_7_0_lsb 0
#define xd_p_reg_cge_idx1_12_8	0xA283
#define	reg_cge_idx1_12_8_pos 0
#define	reg_cge_idx1_12_8_len 5
#define	reg_cge_idx1_12_8_lsb 8
#define xd_p_reg_cge_idx2_7_0	0xA284
#define	reg_cge_idx2_7_0_pos 0
#define	reg_cge_idx2_7_0_len 8
#define	reg_cge_idx2_7_0_lsb 0
#define xd_p_reg_cge_idx2_12_8	0xA285
#define	reg_cge_idx2_12_8_pos 0
#define	reg_cge_idx2_12_8_len 5
#define	reg_cge_idx2_12_8_lsb 8
#define xd_p_reg_cge_idx3_7_0	0xA286
#define	reg_cge_idx3_7_0_pos 0
#define	reg_cge_idx3_7_0_len 8
#define	reg_cge_idx3_7_0_lsb 0
#define xd_p_reg_cge_idx3_12_8	0xA287
#define	reg_cge_idx3_12_8_pos 0
#define	reg_cge_idx3_12_8_len 5
#define	reg_cge_idx3_12_8_lsb 8
#define xd_p_reg_cge_idx4_7_0	0xA288
#define	reg_cge_idx4_7_0_pos 0
#define	reg_cge_idx4_7_0_len 8
#define	reg_cge_idx4_7_0_lsb 0
#define xd_p_reg_cge_idx4_12_8	0xA289
#define	reg_cge_idx4_12_8_pos 0
#define	reg_cge_idx4_12_8_len 5
#define	reg_cge_idx4_12_8_lsb 8
#define xd_p_reg_cge_idx5_7_0	0xA28A
#define	reg_cge_idx5_7_0_pos 0
#define	reg_cge_idx5_7_0_len 8
#define	reg_cge_idx5_7_0_lsb 0
#define xd_p_reg_cge_idx5_12_8	0xA28B
#define	reg_cge_idx5_12_8_pos 0
#define	reg_cge_idx5_12_8_len 5
#define	reg_cge_idx5_12_8_lsb 8
#define xd_p_reg_cge_idx6_7_0	0xA28C
#define	reg_cge_idx6_7_0_pos 0
#define	reg_cge_idx6_7_0_len 8
#define	reg_cge_idx6_7_0_lsb 0
#define xd_p_reg_cge_idx6_12_8	0xA28D
#define	reg_cge_idx6_12_8_pos 0
#define	reg_cge_idx6_12_8_len 5
#define	reg_cge_idx6_12_8_lsb 8
#define xd_p_reg_cge_idx7_7_0	0xA28E
#define	reg_cge_idx7_7_0_pos 0
#define	reg_cge_idx7_7_0_len 8
#define	reg_cge_idx7_7_0_lsb 0
#define xd_p_reg_cge_idx7_12_8	0xA28F
#define	reg_cge_idx7_12_8_pos 0
#define	reg_cge_idx7_12_8_len 5
#define	reg_cge_idx7_12_8_lsb 8
#define xd_p_reg_cge_idx8_7_0	0xA290
#define	reg_cge_idx8_7_0_pos 0
#define	reg_cge_idx8_7_0_len 8
#define	reg_cge_idx8_7_0_lsb 0
#define xd_p_reg_cge_idx8_12_8	0xA291
#define	reg_cge_idx8_12_8_pos 0
#define	reg_cge_idx8_12_8_len 5
#define	reg_cge_idx8_12_8_lsb 8
#define xd_p_reg_cge_idx9_7_0	0xA292
#define	reg_cge_idx9_7_0_pos 0
#define	reg_cge_idx9_7_0_len 8
#define	reg_cge_idx9_7_0_lsb 0
#define xd_p_reg_cge_idx9_12_8	0xA293
#define	reg_cge_idx9_12_8_pos 0
#define	reg_cge_idx9_12_8_len 5
#define	reg_cge_idx9_12_8_lsb 8
#define xd_p_reg_cge_idx10_7_0	0xA294
#define	reg_cge_idx10_7_0_pos 0
#define	reg_cge_idx10_7_0_len 8
#define	reg_cge_idx10_7_0_lsb 0
#define xd_p_reg_cge_idx10_12_8	0xA295
#define	reg_cge_idx10_12_8_pos 0
#define	reg_cge_idx10_12_8_len 5
#define	reg_cge_idx10_12_8_lsb 8
#define xd_p_reg_cge_idx11_7_0	0xA296
#define	reg_cge_idx11_7_0_pos 0
#define	reg_cge_idx11_7_0_len 8
#define	reg_cge_idx11_7_0_lsb 0
#define xd_p_reg_cge_idx11_12_8	0xA297
#define	reg_cge_idx11_12_8_pos 0
#define	reg_cge_idx11_12_8_len 5
#define	reg_cge_idx11_12_8_lsb 8
#define xd_p_reg_cge_idx12_7_0	0xA298
#define	reg_cge_idx12_7_0_pos 0
#define	reg_cge_idx12_7_0_len 8
#define	reg_cge_idx12_7_0_lsb 0
#define xd_p_reg_cge_idx12_12_8	0xA299
#define	reg_cge_idx12_12_8_pos 0
#define	reg_cge_idx12_12_8_len 5
#define	reg_cge_idx12_12_8_lsb 8
#define xd_p_reg_cge_idx13_7_0	0xA29A
#define	reg_cge_idx13_7_0_pos 0
#define	reg_cge_idx13_7_0_len 8
#define	reg_cge_idx13_7_0_lsb 0
#define xd_p_reg_cge_idx13_12_8	0xA29B
#define	reg_cge_idx13_12_8_pos 0
#define	reg_cge_idx13_12_8_len 5
#define	reg_cge_idx13_12_8_lsb 8
#define xd_p_reg_cge_idx14_7_0	0xA29C
#define	reg_cge_idx14_7_0_pos 0
#define	reg_cge_idx14_7_0_len 8
#define	reg_cge_idx14_7_0_lsb 0
#define xd_p_reg_cge_idx14_12_8	0xA29D
#define	reg_cge_idx14_12_8_pos 0
#define	reg_cge_idx14_12_8_len 5
#define	reg_cge_idx14_12_8_lsb 8
#define xd_p_reg_cge_idx15_7_0	0xA29E
#define	reg_cge_idx15_7_0_pos 0
#define	reg_cge_idx15_7_0_len 8
#define	reg_cge_idx15_7_0_lsb 0
#define xd_p_reg_cge_idx15_12_8	0xA29F
#define	reg_cge_idx15_12_8_pos 0
#define	reg_cge_idx15_12_8_len 5
#define	reg_cge_idx15_12_8_lsb 8
#define xd_r_reg_fft_crc	0xA2A8
#define	reg_fft_crc_pos 0
#define	reg_fft_crc_len 8
#define	reg_fft_crc_lsb 0
#define xd_p_fd_fft_shift_max	0xA2A9
#define	fd_fft_shift_max_pos 0
#define	fd_fft_shift_max_len 4
#define	fd_fft_shift_max_lsb 0
#define xd_r_fd_fft_shift	0xA2A9
#define	fd_fft_shift_pos 4
#define	fd_fft_shift_len 4
#define	fd_fft_shift_lsb 0
#define xd_r_fd_fft_frame_num	0xA2AA
#define	fd_fft_frame_num_pos 0
#define	fd_fft_frame_num_len 2
#define	fd_fft_frame_num_lsb 0
#define xd_r_fd_fft_symbol_count	0xA2AB
#define	fd_fft_symbol_count_pos 0
#define	fd_fft_symbol_count_len 7
#define	fd_fft_symbol_count_lsb 0
#define xd_r_reg_fft_idx_max_7_0	0xA2AC
#define	reg_fft_idx_max_7_0_pos 0
#define	reg_fft_idx_max_7_0_len 8
#define	reg_fft_idx_max_7_0_lsb 0
#define xd_r_reg_fft_idx_max_12_8	0xA2AD
#define	reg_fft_idx_max_12_8_pos 0
#define	reg_fft_idx_max_12_8_len 5
#define	reg_fft_idx_max_12_8_lsb 8
#define xd_p_reg_cge_program	0xA2AE
#define	reg_cge_program_pos 0
#define	reg_cge_program_len 1
#define	reg_cge_program_lsb 0
#define xd_p_reg_cge_fixed	0xA2AE
#define	reg_cge_fixed_pos 1
#define	reg_cge_fixed_len 1
#define	reg_cge_fixed_lsb 0
#define xd_p_reg_fft_rotate_en	0xA2AE
#define	reg_fft_rotate_en_pos 2
#define	reg_fft_rotate_en_len 1
#define	reg_fft_rotate_en_lsb 0
#define xd_p_reg_fft_rotate_base_4_0	0xA2AE
#define	reg_fft_rotate_base_4_0_pos 3
#define	reg_fft_rotate_base_4_0_len 5
#define	reg_fft_rotate_base_4_0_lsb 0
#define xd_p_reg_fft_rotate_base_12_5	0xA2AF
#define	reg_fft_rotate_base_12_5_pos 0
#define	reg_fft_rotate_base_12_5_len 8
#define	reg_fft_rotate_base_12_5_lsb 5
#define xd_p_reg_gp_trigger_fd	0xA2B8
#define	reg_gp_trigger_fd_pos 0
#define	reg_gp_trigger_fd_len 1
#define	reg_gp_trigger_fd_lsb 0
#define xd_p_reg_trigger_sel_fd	0xA2B8
#define	reg_trigger_sel_fd_pos 1
#define	reg_trigger_sel_fd_len 2
#define	reg_trigger_sel_fd_lsb 0
#define xd_p_reg_trigger_module_sel_fd	0xA2B9
#define	reg_trigger_module_sel_fd_pos 0
#define	reg_trigger_module_sel_fd_len 6
#define	reg_trigger_module_sel_fd_lsb 0
#define xd_p_reg_trigger_set_sel_fd	0xA2BA
#define	reg_trigger_set_sel_fd_pos 0
#define	reg_trigger_set_sel_fd_len 6
#define	reg_trigger_set_sel_fd_lsb 0
#define xd_p_reg_fd_noname_7_0	0xA2BC
#define	reg_fd_noname_7_0_pos 0
#define	reg_fd_noname_7_0_len 8
#define	reg_fd_noname_7_0_lsb 0
#define xd_p_reg_fd_noname_15_8	0xA2BD
#define	reg_fd_noname_15_8_pos 0
#define	reg_fd_noname_15_8_len 8
#define	reg_fd_noname_15_8_lsb 8
#define xd_p_reg_fd_noname_23_16	0xA2BE
#define	reg_fd_noname_23_16_pos 0
#define	reg_fd_noname_23_16_len 8
#define	reg_fd_noname_23_16_lsb 16
#define xd_p_reg_fd_noname_31_24	0xA2BF
#define	reg_fd_noname_31_24_pos 0
#define	reg_fd_noname_31_24_len 8
#define	reg_fd_noname_31_24_lsb 24
#define xd_r_fd_fpcc_cp_corr_signn	0xA2C0
#define	fd_fpcc_cp_corr_signn_pos 0
#define	fd_fpcc_cp_corr_signn_len 8
#define	fd_fpcc_cp_corr_signn_lsb 0
#define xd_p_reg_feq_s1	0xA2C1
#define	reg_feq_s1_pos 0
#define	reg_feq_s1_len 5
#define	reg_feq_s1_lsb 0
#define xd_p_fd_fpcc_cp_corr_tone_th	0xA2C2
#define	fd_fpcc_cp_corr_tone_th_pos 0
#define	fd_fpcc_cp_corr_tone_th_len 6
#define	fd_fpcc_cp_corr_tone_th_lsb 0
#define xd_p_fd_fpcc_cp_corr_symbol_log_th	0xA2C3
#define	fd_fpcc_cp_corr_symbol_log_th_pos 0
#define	fd_fpcc_cp_corr_symbol_log_th_len 4
#define	fd_fpcc_cp_corr_symbol_log_th_lsb 0
#define xd_p_fd_fpcc_cp_corr_int	0xA2C4
#define	fd_fpcc_cp_corr_int_pos 0
#define	fd_fpcc_cp_corr_int_len 1
#define	fd_fpcc_cp_corr_int_lsb 0
#define xd_p_reg_sfoe_ns_7_0	0xA320
#define	reg_sfoe_ns_7_0_pos 0
#define	reg_sfoe_ns_7_0_len 8
#define	reg_sfoe_ns_7_0_lsb 0
#define xd_p_reg_sfoe_ns_14_8	0xA321
#define	reg_sfoe_ns_14_8_pos 0
#define	reg_sfoe_ns_14_8_len 7
#define	reg_sfoe_ns_14_8_lsb 8
#define xd_p_reg_sfoe_c1_7_0	0xA322
#define	reg_sfoe_c1_7_0_pos 0
#define	reg_sfoe_c1_7_0_len 8
#define	reg_sfoe_c1_7_0_lsb 0
#define xd_p_reg_sfoe_c1_15_8	0xA323
#define	reg_sfoe_c1_15_8_pos 0
#define	reg_sfoe_c1_15_8_len 8
#define	reg_sfoe_c1_15_8_lsb 8
#define xd_p_reg_sfoe_c1_17_16	0xA324
#define	reg_sfoe_c1_17_16_pos 0
#define	reg_sfoe_c1_17_16_len 2
#define	reg_sfoe_c1_17_16_lsb 16
#define xd_p_reg_sfoe_c2_7_0	0xA325
#define	reg_sfoe_c2_7_0_pos 0
#define	reg_sfoe_c2_7_0_len 8
#define	reg_sfoe_c2_7_0_lsb 0
#define xd_p_reg_sfoe_c2_15_8	0xA326
#define	reg_sfoe_c2_15_8_pos 0
#define	reg_sfoe_c2_15_8_len 8
#define	reg_sfoe_c2_15_8_lsb 8
#define xd_p_reg_sfoe_c2_17_16	0xA327
#define	reg_sfoe_c2_17_16_pos 0
#define	reg_sfoe_c2_17_16_len 2
#define	reg_sfoe_c2_17_16_lsb 16
#define xd_r_reg_sfoe_out_9_2	0xA328
#define	reg_sfoe_out_9_2_pos 0
#define	reg_sfoe_out_9_2_len 8
#define	reg_sfoe_out_9_2_lsb 0
#define xd_r_reg_sfoe_out_1_0	0xA329
#define	reg_sfoe_out_1_0_pos 0
#define	reg_sfoe_out_1_0_len 2
#define	reg_sfoe_out_1_0_lsb 0
#define xd_p_reg_sfoe_lm_counter_th	0xA32A
#define	reg_sfoe_lm_counter_th_pos 0
#define	reg_sfoe_lm_counter_th_len 4
#define	reg_sfoe_lm_counter_th_lsb 0
#define xd_p_reg_sfoe_convg_th	0xA32B
#define	reg_sfoe_convg_th_pos 0
#define	reg_sfoe_convg_th_len 8
#define	reg_sfoe_convg_th_lsb 0
#define xd_p_reg_sfoe_divg_th	0xA32C
#define	reg_sfoe_divg_th_pos 0
#define	reg_sfoe_divg_th_len 8
#define	reg_sfoe_divg_th_lsb 0
#define xd_p_fd_tpsd_en	0xA330
#define	fd_tpsd_en_pos 0
#define	fd_tpsd_en_len 1
#define	fd_tpsd_en_lsb 0
#define xd_p_fd_tpsd_dis	0xA330
#define	fd_tpsd_dis_pos 1
#define	fd_tpsd_dis_len 1
#define	fd_tpsd_dis_lsb 0
#define xd_p_fd_tpsd_rst	0xA330
#define	fd_tpsd_rst_pos 2
#define	fd_tpsd_rst_len 1
#define	fd_tpsd_rst_lsb 0
#define xd_p_fd_tpsd_lock	0xA330
#define	fd_tpsd_lock_pos 3
#define	fd_tpsd_lock_len 1
#define	fd_tpsd_lock_lsb 0
#define xd_r_fd_tpsd_s19	0xA330
#define	fd_tpsd_s19_pos 4
#define	fd_tpsd_s19_len 1
#define	fd_tpsd_s19_lsb 0
#define xd_r_fd_tpsd_s17	0xA330
#define	fd_tpsd_s17_pos 5
#define	fd_tpsd_s17_len 1
#define	fd_tpsd_s17_lsb 0
#define xd_p_fd_sfr_ste_en	0xA331
#define	fd_sfr_ste_en_pos 0
#define	fd_sfr_ste_en_len 1
#define	fd_sfr_ste_en_lsb 0
#define xd_p_fd_sfr_ste_dis	0xA331
#define	fd_sfr_ste_dis_pos 1
#define	fd_sfr_ste_dis_len 1
#define	fd_sfr_ste_dis_lsb 0
#define xd_p_fd_sfr_ste_rst	0xA331
#define	fd_sfr_ste_rst_pos 2
#define	fd_sfr_ste_rst_len 1
#define	fd_sfr_ste_rst_lsb 0
#define xd_p_fd_sfr_ste_mode	0xA331
#define	fd_sfr_ste_mode_pos 3
#define	fd_sfr_ste_mode_len 1
#define	fd_sfr_ste_mode_lsb 0
#define xd_p_fd_sfr_ste_done	0xA331
#define	fd_sfr_ste_done_pos 4
#define	fd_sfr_ste_done_len 1
#define	fd_sfr_ste_done_lsb 0
#define xd_p_reg_cfoe_ffoe_en	0xA332
#define	reg_cfoe_ffoe_en_pos 0
#define	reg_cfoe_ffoe_en_len 1
#define	reg_cfoe_ffoe_en_lsb 0
#define xd_p_reg_cfoe_ffoe_dis	0xA332
#define	reg_cfoe_ffoe_dis_pos 1
#define	reg_cfoe_ffoe_dis_len 1
#define	reg_cfoe_ffoe_dis_lsb 0
#define xd_p_reg_cfoe_ffoe_rst	0xA332
#define	reg_cfoe_ffoe_rst_pos 2
#define	reg_cfoe_ffoe_rst_len 1
#define	reg_cfoe_ffoe_rst_lsb 0
#define xd_p_reg_cfoe_ifoe_en	0xA332
#define	reg_cfoe_ifoe_en_pos 3
#define	reg_cfoe_ifoe_en_len 1
#define	reg_cfoe_ifoe_en_lsb 0
#define xd_p_reg_cfoe_ifoe_dis	0xA332
#define	reg_cfoe_ifoe_dis_pos 4
#define	reg_cfoe_ifoe_dis_len 1
#define	reg_cfoe_ifoe_dis_lsb 0
#define xd_p_reg_cfoe_ifoe_rst	0xA332
#define	reg_cfoe_ifoe_rst_pos 5
#define	reg_cfoe_ifoe_rst_len 1
#define	reg_cfoe_ifoe_rst_lsb 0
#define xd_p_reg_cfoe_fot_en	0xA332
#define	reg_cfoe_fot_en_pos 6
#define	reg_cfoe_fot_en_len 1
#define	reg_cfoe_fot_en_lsb 0
#define xd_p_reg_cfoe_fot_lm_en	0xA332
#define	reg_cfoe_fot_lm_en_pos 7
#define	reg_cfoe_fot_lm_en_len 1
#define	reg_cfoe_fot_lm_en_lsb 0
#define xd_p_reg_cfoe_fot_rst	0xA333
#define	reg_cfoe_fot_rst_pos 0
#define	reg_cfoe_fot_rst_len 1
#define	reg_cfoe_fot_rst_lsb 0
#define xd_r_fd_cfoe_ffoe_done	0xA333
#define	fd_cfoe_ffoe_done_pos 1
#define	fd_cfoe_ffoe_done_len 1
#define	fd_cfoe_ffoe_done_lsb 0
#define xd_p_fd_cfoe_metric_vld	0xA333
#define	fd_cfoe_metric_vld_pos 2
#define	fd_cfoe_metric_vld_len 1
#define	fd_cfoe_metric_vld_lsb 0
#define xd_p_reg_cfoe_ifod_vld	0xA333
#define	reg_cfoe_ifod_vld_pos 3
#define	reg_cfoe_ifod_vld_len 1
#define	reg_cfoe_ifod_vld_lsb 0
#define xd_r_fd_cfoe_ifoe_done	0xA333
#define	fd_cfoe_ifoe_done_pos 4
#define	fd_cfoe_ifoe_done_len 1
#define	fd_cfoe_ifoe_done_lsb 0
#define xd_r_fd_cfoe_fot_valid	0xA333
#define	fd_cfoe_fot_valid_pos 5
#define	fd_cfoe_fot_valid_len 1
#define	fd_cfoe_fot_valid_lsb 0
#define xd_p_reg_cfoe_divg_int	0xA333
#define	reg_cfoe_divg_int_pos 6
#define	reg_cfoe_divg_int_len 1
#define	reg_cfoe_divg_int_lsb 0
#define xd_r_reg_cfoe_divg_flag	0xA333
#define	reg_cfoe_divg_flag_pos 7
#define	reg_cfoe_divg_flag_len 1
#define	reg_cfoe_divg_flag_lsb 0
#define xd_p_reg_sfoe_en	0xA334
#define	reg_sfoe_en_pos 0
#define	reg_sfoe_en_len 1
#define	reg_sfoe_en_lsb 0
#define xd_p_reg_sfoe_dis	0xA334
#define	reg_sfoe_dis_pos 1
#define	reg_sfoe_dis_len 1
#define	reg_sfoe_dis_lsb 0
#define xd_p_reg_sfoe_rst	0xA334
#define	reg_sfoe_rst_pos 2
#define	reg_sfoe_rst_len 1
#define	reg_sfoe_rst_lsb 0
#define xd_p_reg_sfoe_vld_int	0xA334
#define	reg_sfoe_vld_int_pos 3
#define	reg_sfoe_vld_int_len 1
#define	reg_sfoe_vld_int_lsb 0
#define xd_p_reg_sfoe_lm_en	0xA334
#define	reg_sfoe_lm_en_pos 4
#define	reg_sfoe_lm_en_len 1
#define	reg_sfoe_lm_en_lsb 0
#define xd_p_reg_sfoe_divg_int	0xA334
#define	reg_sfoe_divg_int_pos 5
#define	reg_sfoe_divg_int_len 1
#define	reg_sfoe_divg_int_lsb 0
#define xd_r_reg_sfoe_divg_flag	0xA334
#define	reg_sfoe_divg_flag_pos 6
#define	reg_sfoe_divg_flag_len 1
#define	reg_sfoe_divg_flag_lsb 0
#define xd_p_reg_fft_rst	0xA335
#define	reg_fft_rst_pos 0
#define	reg_fft_rst_len 1
#define	reg_fft_rst_lsb 0
#define xd_p_reg_fft_fast_beacon	0xA335
#define	reg_fft_fast_beacon_pos 1
#define	reg_fft_fast_beacon_len 1
#define	reg_fft_fast_beacon_lsb 0
#define xd_p_reg_fft_fast_valid	0xA335
#define	reg_fft_fast_valid_pos 2
#define	reg_fft_fast_valid_len 1
#define	reg_fft_fast_valid_lsb 0
#define xd_p_reg_fft_mask_en	0xA335
#define	reg_fft_mask_en_pos 3
#define	reg_fft_mask_en_len 1
#define	reg_fft_mask_en_lsb 0
#define xd_p_reg_fft_crc_en	0xA335
#define	reg_fft_crc_en_pos 4
#define	reg_fft_crc_en_len 1
#define	reg_fft_crc_en_lsb 0
#define xd_p_reg_finr_en	0xA336
#define	reg_finr_en_pos 0
#define	reg_finr_en_len 1
#define	reg_finr_en_lsb 0
#define xd_p_fd_fste_en	0xA337
#define	fd_fste_en_pos 1
#define	fd_fste_en_len 1
#define	fd_fste_en_lsb 0
#define xd_p_fd_sqi_tps_level_shift	0xA338
#define	fd_sqi_tps_level_shift_pos 0
#define	fd_sqi_tps_level_shift_len 8
#define	fd_sqi_tps_level_shift_lsb 0
#define xd_p_fd_pilot_ma_len	0xA339
#define	fd_pilot_ma_len_pos 0
#define	fd_pilot_ma_len_len 6
#define	fd_pilot_ma_len_lsb 0
#define xd_p_fd_tps_ma_len	0xA33A
#define	fd_tps_ma_len_pos 0
#define	fd_tps_ma_len_len 6
#define	fd_tps_ma_len_lsb 0
#define xd_p_fd_sqi_s3	0xA33B
#define	fd_sqi_s3_pos 0
#define	fd_sqi_s3_len 8
#define	fd_sqi_s3_lsb 0
#define xd_p_fd_sqi_dummy_reg_0	0xA33C
#define	fd_sqi_dummy_reg_0_pos 0
#define	fd_sqi_dummy_reg_0_len 1
#define	fd_sqi_dummy_reg_0_lsb 0
#define xd_p_fd_sqi_debug_sel	0xA33C
#define	fd_sqi_debug_sel_pos 1
#define	fd_sqi_debug_sel_len 2
#define	fd_sqi_debug_sel_lsb 0
#define xd_p_fd_sqi_s2	0xA33C
#define	fd_sqi_s2_pos 3
#define	fd_sqi_s2_len 5
#define	fd_sqi_s2_lsb 0
#define xd_p_fd_sqi_dummy_reg_1	0xA33D
#define	fd_sqi_dummy_reg_1_pos 0
#define	fd_sqi_dummy_reg_1_len 1
#define	fd_sqi_dummy_reg_1_lsb 0
#define xd_p_fd_inr_ignore	0xA33D
#define	fd_inr_ignore_pos 1
#define	fd_inr_ignore_len 1
#define	fd_inr_ignore_lsb 0
#define xd_p_fd_pilot_ignore	0xA33D
#define	fd_pilot_ignore_pos 2
#define	fd_pilot_ignore_len 1
#define	fd_pilot_ignore_lsb 0
#define xd_p_fd_etps_ignore	0xA33D
#define	fd_etps_ignore_pos 3
#define	fd_etps_ignore_len 1
#define	fd_etps_ignore_lsb 0
#define xd_p_fd_sqi_s1	0xA33D
#define	fd_sqi_s1_pos 4
#define	fd_sqi_s1_len 4
#define	fd_sqi_s1_lsb 0
#define xd_p_reg_fste_ehw_7_0	0xA33E
#define	reg_fste_ehw_7_0_pos 0
#define	reg_fste_ehw_7_0_len 8
#define	reg_fste_ehw_7_0_lsb 0
#define xd_p_reg_fste_ehw_9_8	0xA33F
#define	reg_fste_ehw_9_8_pos 0
#define	reg_fste_ehw_9_8_len 2
#define	reg_fste_ehw_9_8_lsb 8
#define xd_p_reg_fste_i_adj_vld	0xA33F
#define	reg_fste_i_adj_vld_pos 2
#define	reg_fste_i_adj_vld_len 1
#define	reg_fste_i_adj_vld_lsb 0
#define xd_p_reg_fste_phase_ini_7_0	0xA340
#define	reg_fste_phase_ini_7_0_pos 0
#define	reg_fste_phase_ini_7_0_len 8
#define	reg_fste_phase_ini_7_0_lsb 0
#define xd_p_reg_fste_phase_ini_11_8	0xA341
#define	reg_fste_phase_ini_11_8_pos 0
#define	reg_fste_phase_ini_11_8_len 4
#define	reg_fste_phase_ini_11_8_lsb 8
#define xd_p_reg_fste_phase_inc_3_0	0xA341
#define	reg_fste_phase_inc_3_0_pos 4
#define	reg_fste_phase_inc_3_0_len 4
#define	reg_fste_phase_inc_3_0_lsb 0
#define xd_p_reg_fste_phase_inc_11_4	0xA342
#define	reg_fste_phase_inc_11_4_pos 0
#define	reg_fste_phase_inc_11_4_len 8
#define	reg_fste_phase_inc_11_4_lsb 4
#define xd_p_reg_fste_acum_cost_cnt_max	0xA343
#define	reg_fste_acum_cost_cnt_max_pos 0
#define	reg_fste_acum_cost_cnt_max_len 4
#define	reg_fste_acum_cost_cnt_max_lsb 0
#define xd_p_reg_fste_step_size_std	0xA343
#define	reg_fste_step_size_std_pos 4
#define	reg_fste_step_size_std_len 4
#define	reg_fste_step_size_std_lsb 0
#define xd_p_reg_fste_step_size_max	0xA344
#define	reg_fste_step_size_max_pos 0
#define	reg_fste_step_size_max_len 4
#define	reg_fste_step_size_max_lsb 0
#define xd_p_reg_fste_step_size_min	0xA344
#define	reg_fste_step_size_min_pos 4
#define	reg_fste_step_size_min_len 4
#define	reg_fste_step_size_min_lsb 0
#define xd_p_reg_fste_frac_step_size_7_0	0xA345
#define	reg_fste_frac_step_size_7_0_pos 0
#define	reg_fste_frac_step_size_7_0_len 8
#define	reg_fste_frac_step_size_7_0_lsb 0
#define xd_p_reg_fste_frac_step_size_15_8	0xA346
#define	reg_fste_frac_step_size_15_8_pos 0
#define	reg_fste_frac_step_size_15_8_len 8
#define	reg_fste_frac_step_size_15_8_lsb 8
#define xd_p_reg_fste_frac_step_size_19_16	0xA347
#define	reg_fste_frac_step_size_19_16_pos 0
#define	reg_fste_frac_step_size_19_16_len 4
#define	reg_fste_frac_step_size_19_16_lsb 16
#define xd_p_reg_fste_rpd_dir_cnt_max	0xA347
#define	reg_fste_rpd_dir_cnt_max_pos 4
#define	reg_fste_rpd_dir_cnt_max_len 4
#define	reg_fste_rpd_dir_cnt_max_lsb 0
#define xd_p_reg_fste_ehs	0xA348
#define	reg_fste_ehs_pos 0
#define	reg_fste_ehs_len 4
#define	reg_fste_ehs_lsb 0
#define xd_p_reg_fste_frac_cost_cnt_max_3_0	0xA348
#define	reg_fste_frac_cost_cnt_max_3_0_pos 4
#define	reg_fste_frac_cost_cnt_max_3_0_len 4
#define	reg_fste_frac_cost_cnt_max_3_0_lsb 0
#define xd_p_reg_fste_frac_cost_cnt_max_9_4	0xA349
#define	reg_fste_frac_cost_cnt_max_9_4_pos 0
#define	reg_fste_frac_cost_cnt_max_9_4_len 6
#define	reg_fste_frac_cost_cnt_max_9_4_lsb 4
#define xd_p_reg_fste_w0_7_0	0xA34A
#define	reg_fste_w0_7_0_pos 0
#define	reg_fste_w0_7_0_len 8
#define	reg_fste_w0_7_0_lsb 0
#define xd_p_reg_fste_w0_11_8	0xA34B
#define	reg_fste_w0_11_8_pos 0
#define	reg_fste_w0_11_8_len 4
#define	reg_fste_w0_11_8_lsb 8
#define xd_p_reg_fste_w1_3_0	0xA34B
#define	reg_fste_w1_3_0_pos 4
#define	reg_fste_w1_3_0_len 4
#define	reg_fste_w1_3_0_lsb 0
#define xd_p_reg_fste_w1_11_4	0xA34C
#define	reg_fste_w1_11_4_pos 0
#define	reg_fste_w1_11_4_len 8
#define	reg_fste_w1_11_4_lsb 4
#define xd_p_reg_fste_w2_7_0	0xA34D
#define	reg_fste_w2_7_0_pos 0
#define	reg_fste_w2_7_0_len 8
#define	reg_fste_w2_7_0_lsb 0
#define xd_p_reg_fste_w2_11_8	0xA34E
#define	reg_fste_w2_11_8_pos 0
#define	reg_fste_w2_11_8_len 4
#define	reg_fste_w2_11_8_lsb 8
#define xd_p_reg_fste_w3_3_0	0xA34E
#define	reg_fste_w3_3_0_pos 4
#define	reg_fste_w3_3_0_len 4
#define	reg_fste_w3_3_0_lsb 0
#define xd_p_reg_fste_w3_11_4	0xA34F
#define	reg_fste_w3_11_4_pos 0
#define	reg_fste_w3_11_4_len 8
#define	reg_fste_w3_11_4_lsb 4
#define xd_p_reg_fste_w4_7_0	0xA350
#define	reg_fste_w4_7_0_pos 0
#define	reg_fste_w4_7_0_len 8
#define	reg_fste_w4_7_0_lsb 0
#define xd_p_reg_fste_w4_11_8	0xA351
#define	reg_fste_w4_11_8_pos 0
#define	reg_fste_w4_11_8_len 4
#define	reg_fste_w4_11_8_lsb 8
#define xd_p_reg_fste_w5_3_0	0xA351
#define	reg_fste_w5_3_0_pos 4
#define	reg_fste_w5_3_0_len 4
#define	reg_fste_w5_3_0_lsb 0
#define xd_p_reg_fste_w5_11_4	0xA352
#define	reg_fste_w5_11_4_pos 0
#define	reg_fste_w5_11_4_len 8
#define	reg_fste_w5_11_4_lsb 4
#define xd_p_reg_fste_w6_7_0	0xA353
#define	reg_fste_w6_7_0_pos 0
#define	reg_fste_w6_7_0_len 8
#define	reg_fste_w6_7_0_lsb 0
#define xd_p_reg_fste_w6_11_8	0xA354
#define	reg_fste_w6_11_8_pos 0
#define	reg_fste_w6_11_8_len 4
#define	reg_fste_w6_11_8_lsb 8
#define xd_p_reg_fste_w7_3_0	0xA354
#define	reg_fste_w7_3_0_pos 4
#define	reg_fste_w7_3_0_len 4
#define	reg_fste_w7_3_0_lsb 0
#define xd_p_reg_fste_w7_11_4	0xA355
#define	reg_fste_w7_11_4_pos 0
#define	reg_fste_w7_11_4_len 8
#define	reg_fste_w7_11_4_lsb 4
#define xd_p_reg_fste_w8_7_0	0xA356
#define	reg_fste_w8_7_0_pos 0
#define	reg_fste_w8_7_0_len 8
#define	reg_fste_w8_7_0_lsb 0
#define xd_p_reg_fste_w8_11_8	0xA357
#define	reg_fste_w8_11_8_pos 0
#define	reg_fste_w8_11_8_len 4
#define	reg_fste_w8_11_8_lsb 8
#define xd_p_reg_fste_w9_3_0	0xA357
#define	reg_fste_w9_3_0_pos 4
#define	reg_fste_w9_3_0_len 4
#define	reg_fste_w9_3_0_lsb 0
#define xd_p_reg_fste_w9_11_4	0xA358
#define	reg_fste_w9_11_4_pos 0
#define	reg_fste_w9_11_4_len 8
#define	reg_fste_w9_11_4_lsb 4
#define xd_p_reg_fste_wa_7_0	0xA359
#define	reg_fste_wa_7_0_pos 0
#define	reg_fste_wa_7_0_len 8
#define	reg_fste_wa_7_0_lsb 0
#define xd_p_reg_fste_wa_11_8	0xA35A
#define	reg_fste_wa_11_8_pos 0
#define	reg_fste_wa_11_8_len 4
#define	reg_fste_wa_11_8_lsb 8
#define xd_p_reg_fste_wb_3_0	0xA35A
#define	reg_fste_wb_3_0_pos 4
#define	reg_fste_wb_3_0_len 4
#define	reg_fste_wb_3_0_lsb 0
#define xd_p_reg_fste_wb_11_4	0xA35B
#define	reg_fste_wb_11_4_pos 0
#define	reg_fste_wb_11_4_len 8
#define	reg_fste_wb_11_4_lsb 4
#define xd_r_fd_fste_i_adj	0xA35C
#define	fd_fste_i_adj_pos 0
#define	fd_fste_i_adj_len 5
#define	fd_fste_i_adj_lsb 0
#define xd_r_fd_fste_f_adj_7_0	0xA35D
#define	fd_fste_f_adj_7_0_pos 0
#define	fd_fste_f_adj_7_0_len 8
#define	fd_fste_f_adj_7_0_lsb 0
#define xd_r_fd_fste_f_adj_15_8	0xA35E
#define	fd_fste_f_adj_15_8_pos 0
#define	fd_fste_f_adj_15_8_len 8
#define	fd_fste_f_adj_15_8_lsb 8
#define xd_r_fd_fste_f_adj_19_16	0xA35F
#define	fd_fste_f_adj_19_16_pos 0
#define	fd_fste_f_adj_19_16_len 4
#define	fd_fste_f_adj_19_16_lsb 16
#define xd_p_reg_feq_Leak_Bypass	0xA366
#define	reg_feq_Leak_Bypass_pos 0
#define	reg_feq_Leak_Bypass_len 1
#define	reg_feq_Leak_Bypass_lsb 0
#define xd_p_reg_feq_Leak_Mneg1	0xA366
#define	reg_feq_Leak_Mneg1_pos 1
#define	reg_feq_Leak_Mneg1_len 3
#define	reg_feq_Leak_Mneg1_lsb 0
#define xd_p_reg_feq_Leak_B_ShiftQ	0xA366
#define	reg_feq_Leak_B_ShiftQ_pos 4
#define	reg_feq_Leak_B_ShiftQ_len 4
#define	reg_feq_Leak_B_ShiftQ_lsb 0
#define xd_p_reg_feq_Leak_B_Float0	0xA367
#define	reg_feq_Leak_B_Float0_pos 0
#define	reg_feq_Leak_B_Float0_len 8
#define	reg_feq_Leak_B_Float0_lsb 0
#define xd_p_reg_feq_Leak_B_Float1	0xA368
#define	reg_feq_Leak_B_Float1_pos 0
#define	reg_feq_Leak_B_Float1_len 8
#define	reg_feq_Leak_B_Float1_lsb 0
#define xd_p_reg_feq_Leak_B_Float2	0xA369
#define	reg_feq_Leak_B_Float2_pos 0
#define	reg_feq_Leak_B_Float2_len 8
#define	reg_feq_Leak_B_Float2_lsb 0
#define xd_p_reg_feq_Leak_B_Float3	0xA36A
#define	reg_feq_Leak_B_Float3_pos 0
#define	reg_feq_Leak_B_Float3_len 8
#define	reg_feq_Leak_B_Float3_lsb 0
#define xd_p_reg_feq_Leak_B_Float4	0xA36B
#define	reg_feq_Leak_B_Float4_pos 0
#define	reg_feq_Leak_B_Float4_len 8
#define	reg_feq_Leak_B_Float4_lsb 0
#define xd_p_reg_feq_Leak_B_Float5	0xA36C
#define	reg_feq_Leak_B_Float5_pos 0
#define	reg_feq_Leak_B_Float5_len 8
#define	reg_feq_Leak_B_Float5_lsb 0
#define xd_p_reg_feq_Leak_B_Float6	0xA36D
#define	reg_feq_Leak_B_Float6_pos 0
#define	reg_feq_Leak_B_Float6_len 8
#define	reg_feq_Leak_B_Float6_lsb 0
#define xd_p_reg_feq_Leak_B_Float7	0xA36E
#define	reg_feq_Leak_B_Float7_pos 0
#define	reg_feq_Leak_B_Float7_len 8
#define	reg_feq_Leak_B_Float7_lsb 0
#define xd_r_reg_feq_data_h2_7_0	0xA36F
#define	reg_feq_data_h2_7_0_pos 0
#define	reg_feq_data_h2_7_0_len 8
#define	reg_feq_data_h2_7_0_lsb 0
#define xd_r_reg_feq_data_h2_9_8	0xA370
#define	reg_feq_data_h2_9_8_pos 0
#define	reg_feq_data_h2_9_8_len 2
#define	reg_feq_data_h2_9_8_lsb 8
#define xd_p_reg_feq_leak_use_slice_tps	0xA371
#define	reg_feq_leak_use_slice_tps_pos 0
#define	reg_feq_leak_use_slice_tps_len 1
#define	reg_feq_leak_use_slice_tps_lsb 0
#define xd_p_reg_feq_read_update	0xA371
#define	reg_feq_read_update_pos 1
#define	reg_feq_read_update_len 1
#define	reg_feq_read_update_lsb 0
#define xd_p_reg_feq_data_vld	0xA371
#define	reg_feq_data_vld_pos 2
#define	reg_feq_data_vld_len 1
#define	reg_feq_data_vld_lsb 0
#define xd_p_reg_feq_tone_idx_4_0	0xA371
#define	reg_feq_tone_idx_4_0_pos 3
#define	reg_feq_tone_idx_4_0_len 5
#define	reg_feq_tone_idx_4_0_lsb 0
#define xd_p_reg_feq_tone_idx_12_5	0xA372
#define	reg_feq_tone_idx_12_5_pos 0
#define	reg_feq_tone_idx_12_5_len 8
#define	reg_feq_tone_idx_12_5_lsb 5
#define xd_r_reg_feq_data_re_7_0	0xA373
#define	reg_feq_data_re_7_0_pos 0
#define	reg_feq_data_re_7_0_len 8
#define	reg_feq_data_re_7_0_lsb 0
#define xd_r_reg_feq_data_re_10_8	0xA374
#define	reg_feq_data_re_10_8_pos 0
#define	reg_feq_data_re_10_8_len 3
#define	reg_feq_data_re_10_8_lsb 8
#define xd_r_reg_feq_data_im_7_0	0xA375
#define	reg_feq_data_im_7_0_pos 0
#define	reg_feq_data_im_7_0_len 8
#define	reg_feq_data_im_7_0_lsb 0
#define xd_r_reg_feq_data_im_10_8	0xA376
#define	reg_feq_data_im_10_8_pos 0
#define	reg_feq_data_im_10_8_len 3
#define	reg_feq_data_im_10_8_lsb 8
#define xd_r_reg_feq_y_re	0xA377
#define	reg_feq_y_re_pos 0
#define	reg_feq_y_re_len 8
#define	reg_feq_y_re_lsb 0
#define xd_r_reg_feq_y_im	0xA378
#define	reg_feq_y_im_pos 0
#define	reg_feq_y_im_len 8
#define	reg_feq_y_im_lsb 0
#define xd_r_reg_feq_h_re_7_0	0xA379
#define	reg_feq_h_re_7_0_pos 0
#define	reg_feq_h_re_7_0_len 8
#define	reg_feq_h_re_7_0_lsb 0
#define xd_r_reg_feq_h_re_8	0xA37A
#define	reg_feq_h_re_8_pos 0
#define	reg_feq_h_re_8_len 1
#define	reg_feq_h_re_8_lsb 0
#define xd_r_reg_feq_h_im_7_0	0xA37B
#define	reg_feq_h_im_7_0_pos 0
#define	reg_feq_h_im_7_0_len 8
#define	reg_feq_h_im_7_0_lsb 0
#define xd_r_reg_feq_h_im_8	0xA37C
#define	reg_feq_h_im_8_pos 0
#define	reg_feq_h_im_8_len 1
#define	reg_feq_h_im_8_lsb 0
#define xd_p_fec_super_frm_unit_7_0	0xA380
#define	fec_super_frm_unit_7_0_pos 0
#define	fec_super_frm_unit_7_0_len 8
#define	fec_super_frm_unit_7_0_lsb 0
#define xd_p_fec_super_frm_unit_15_8	0xA381
#define	fec_super_frm_unit_15_8_pos 0
#define	fec_super_frm_unit_15_8_len 8
#define	fec_super_frm_unit_15_8_lsb 8
#define xd_r_fec_vtb_err_bit_cnt_7_0	0xA382
#define	fec_vtb_err_bit_cnt_7_0_pos 0
#define	fec_vtb_err_bit_cnt_7_0_len 8
#define	fec_vtb_err_bit_cnt_7_0_lsb 0
#define xd_r_fec_vtb_err_bit_cnt_15_8	0xA383
#define	fec_vtb_err_bit_cnt_15_8_pos 0
#define	fec_vtb_err_bit_cnt_15_8_len 8
#define	fec_vtb_err_bit_cnt_15_8_lsb 8
#define xd_r_fec_vtb_err_bit_cnt_23_16	0xA384
#define	fec_vtb_err_bit_cnt_23_16_pos 0
#define	fec_vtb_err_bit_cnt_23_16_len 8
#define	fec_vtb_err_bit_cnt_23_16_lsb 16
#define xd_p_fec_rsd_packet_unit_7_0	0xA385
#define	fec_rsd_packet_unit_7_0_pos 0
#define	fec_rsd_packet_unit_7_0_len 8
#define	fec_rsd_packet_unit_7_0_lsb 0
#define xd_p_fec_rsd_packet_unit_15_8	0xA386
#define	fec_rsd_packet_unit_15_8_pos 0
#define	fec_rsd_packet_unit_15_8_len 8
#define	fec_rsd_packet_unit_15_8_lsb 8
#define xd_r_fec_rsd_bit_err_cnt_7_0	0xA387
#define	fec_rsd_bit_err_cnt_7_0_pos 0
#define	fec_rsd_bit_err_cnt_7_0_len 8
#define	fec_rsd_bit_err_cnt_7_0_lsb 0
#define xd_r_fec_rsd_bit_err_cnt_15_8	0xA388
#define	fec_rsd_bit_err_cnt_15_8_pos 0
#define	fec_rsd_bit_err_cnt_15_8_len 8
#define	fec_rsd_bit_err_cnt_15_8_lsb 8
#define xd_r_fec_rsd_bit_err_cnt_23_16	0xA389
#define	fec_rsd_bit_err_cnt_23_16_pos 0
#define	fec_rsd_bit_err_cnt_23_16_len 8
#define	fec_rsd_bit_err_cnt_23_16_lsb 16
#define xd_r_fec_rsd_abort_packet_cnt_7_0	0xA38A
#define	fec_rsd_abort_packet_cnt_7_0_pos 0
#define	fec_rsd_abort_packet_cnt_7_0_len 8
#define	fec_rsd_abort_packet_cnt_7_0_lsb 0
#define xd_r_fec_rsd_abort_packet_cnt_15_8	0xA38B
#define	fec_rsd_abort_packet_cnt_15_8_pos 0
#define	fec_rsd_abort_packet_cnt_15_8_len 8
#define	fec_rsd_abort_packet_cnt_15_8_lsb 8
#define xd_p_fec_RSD_PKT_NUM_PER_UNIT_7_0	0xA38C
#define	fec_RSD_PKT_NUM_PER_UNIT_7_0_pos 0
#define	fec_RSD_PKT_NUM_PER_UNIT_7_0_len 8
#define	fec_RSD_PKT_NUM_PER_UNIT_7_0_lsb 0
#define xd_p_fec_RSD_PKT_NUM_PER_UNIT_15_8	0xA38D
#define	fec_RSD_PKT_NUM_PER_UNIT_15_8_pos 0
#define	fec_RSD_PKT_NUM_PER_UNIT_15_8_len 8
#define	fec_RSD_PKT_NUM_PER_UNIT_15_8_lsb 8
#define xd_p_fec_RS_TH_1_7_0	0xA38E
#define	fec_RS_TH_1_7_0_pos 0
#define	fec_RS_TH_1_7_0_len 8
#define	fec_RS_TH_1_7_0_lsb 0
#define xd_p_fec_RS_TH_1_15_8	0xA38F
#define	fec_RS_TH_1_15_8_pos 0
#define	fec_RS_TH_1_15_8_len 8
#define	fec_RS_TH_1_15_8_lsb 8
#define xd_p_fec_RS_TH_2	0xA390
#define	fec_RS_TH_2_pos 0
#define	fec_RS_TH_2_len 8
#define	fec_RS_TH_2_lsb 0
#define xd_p_fec_mon_en	0xA391
#define	fec_mon_en_pos 0
#define	fec_mon_en_len 1
#define	fec_mon_en_lsb 0
#define xd_p_reg_b8to47	0xA391
#define	reg_b8to47_pos 1
#define	reg_b8to47_len 1
#define	reg_b8to47_lsb 0
#define xd_p_reg_rsd_sync_rep	0xA391
#define	reg_rsd_sync_rep_pos 2
#define	reg_rsd_sync_rep_len 1
#define	reg_rsd_sync_rep_lsb 0
#define xd_p_fec_rsd_retrain_rst	0xA391
#define	fec_rsd_retrain_rst_pos 3
#define	fec_rsd_retrain_rst_len 1
#define	fec_rsd_retrain_rst_lsb 0
#define xd_r_fec_rsd_ber_rdy	0xA391
#define	fec_rsd_ber_rdy_pos 4
#define	fec_rsd_ber_rdy_len 1
#define	fec_rsd_ber_rdy_lsb 0
#define xd_p_fec_rsd_ber_rst	0xA391
#define	fec_rsd_ber_rst_pos 5
#define	fec_rsd_ber_rst_len 1
#define	fec_rsd_ber_rst_lsb 0
#define xd_r_fec_vtb_ber_rdy	0xA391
#define	fec_vtb_ber_rdy_pos 6
#define	fec_vtb_ber_rdy_len 1
#define	fec_vtb_ber_rdy_lsb 0
#define xd_p_fec_vtb_ber_rst	0xA391
#define	fec_vtb_ber_rst_pos 7
#define	fec_vtb_ber_rst_len 1
#define	fec_vtb_ber_rst_lsb 0
#define xd_p_reg_vtb_clk40en	0xA392
#define	reg_vtb_clk40en_pos 0
#define	reg_vtb_clk40en_len 1
#define	reg_vtb_clk40en_lsb 0
#define xd_p_fec_vtb_rsd_mon_en	0xA392
#define	fec_vtb_rsd_mon_en_pos 1
#define	fec_vtb_rsd_mon_en_len 1
#define	fec_vtb_rsd_mon_en_lsb 0
#define xd_p_reg_fec_data_en	0xA392
#define	reg_fec_data_en_pos 2
#define	reg_fec_data_en_len 1
#define	reg_fec_data_en_lsb 0
#define xd_p_fec_dummy_reg_2	0xA392
#define	fec_dummy_reg_2_pos 3
#define	fec_dummy_reg_2_len 3
#define	fec_dummy_reg_2_lsb 0
#define xd_p_reg_sync_chk	0xA392
#define	reg_sync_chk_pos 6
#define	reg_sync_chk_len 1
#define	reg_sync_chk_lsb 0
#define xd_p_fec_rsd_bypass	0xA392
#define	fec_rsd_bypass_pos 7
#define	fec_rsd_bypass_len 1
#define	fec_rsd_bypass_lsb 0
#define xd_p_fec_sw_rst	0xA393
#define	fec_sw_rst_pos 0
#define	fec_sw_rst_len 1
#define	fec_sw_rst_lsb 0
#define xd_r_fec_vtb_pm_crc	0xA394
#define	fec_vtb_pm_crc_pos 0
#define	fec_vtb_pm_crc_len 8
#define	fec_vtb_pm_crc_lsb 0
#define xd_r_fec_vtb_tb_7_crc	0xA395
#define	fec_vtb_tb_7_crc_pos 0
#define	fec_vtb_tb_7_crc_len 8
#define	fec_vtb_tb_7_crc_lsb 0
#define xd_r_fec_vtb_tb_6_crc	0xA396
#define	fec_vtb_tb_6_crc_pos 0
#define	fec_vtb_tb_6_crc_len 8
#define	fec_vtb_tb_6_crc_lsb 0
#define xd_r_fec_vtb_tb_5_crc	0xA397
#define	fec_vtb_tb_5_crc_pos 0
#define	fec_vtb_tb_5_crc_len 8
#define	fec_vtb_tb_5_crc_lsb 0
#define xd_r_fec_vtb_tb_4_crc	0xA398
#define	fec_vtb_tb_4_crc_pos 0
#define	fec_vtb_tb_4_crc_len 8
#define	fec_vtb_tb_4_crc_lsb 0
#define xd_r_fec_vtb_tb_3_crc	0xA399
#define	fec_vtb_tb_3_crc_pos 0
#define	fec_vtb_tb_3_crc_len 8
#define	fec_vtb_tb_3_crc_lsb 0
#define xd_r_fec_vtb_tb_2_crc	0xA39A
#define	fec_vtb_tb_2_crc_pos 0
#define	fec_vtb_tb_2_crc_len 8
#define	fec_vtb_tb_2_crc_lsb 0
#define xd_r_fec_vtb_tb_1_crc	0xA39B
#define	fec_vtb_tb_1_crc_pos 0
#define	fec_vtb_tb_1_crc_len 8
#define	fec_vtb_tb_1_crc_lsb 0
#define xd_r_fec_vtb_tb_0_crc	0xA39C
#define	fec_vtb_tb_0_crc_pos 0
#define	fec_vtb_tb_0_crc_len 8
#define	fec_vtb_tb_0_crc_lsb 0
#define xd_r_fec_rsd_bank0_crc	0xA39D
#define	fec_rsd_bank0_crc_pos 0
#define	fec_rsd_bank0_crc_len 8
#define	fec_rsd_bank0_crc_lsb 0
#define xd_r_fec_rsd_bank1_crc	0xA39E
#define	fec_rsd_bank1_crc_pos 0
#define	fec_rsd_bank1_crc_len 8
#define	fec_rsd_bank1_crc_lsb 0
#define xd_r_fec_idi_vtb_crc	0xA39F
#define	fec_idi_vtb_crc_pos 0
#define	fec_idi_vtb_crc_len 8
#define	fec_idi_vtb_crc_lsb 0
#define xd_g_reg_tpsd_txmod	0xA3C0
#define	reg_tpsd_txmod_pos 0
#define	reg_tpsd_txmod_len 2
#define	reg_tpsd_txmod_lsb 0
#define xd_g_reg_tpsd_gi	0xA3C0
#define	reg_tpsd_gi_pos 2
#define	reg_tpsd_gi_len 2
#define	reg_tpsd_gi_lsb 0
#define xd_g_reg_tpsd_hier	0xA3C0
#define	reg_tpsd_hier_pos 4
#define	reg_tpsd_hier_len 3
#define	reg_tpsd_hier_lsb 0
#define xd_g_reg_bw	0xA3C1
#define	reg_bw_pos 2
#define	reg_bw_len 2
#define	reg_bw_lsb 0
#define xd_g_reg_dec_pri	0xA3C1
#define	reg_dec_pri_pos 4
#define	reg_dec_pri_len 1
#define	reg_dec_pri_lsb 0
#define xd_g_reg_tpsd_const	0xA3C1
#define	reg_tpsd_const_pos 6
#define	reg_tpsd_const_len 2
#define	reg_tpsd_const_lsb 0
#define xd_g_reg_tpsd_hpcr	0xA3C2
#define	reg_tpsd_hpcr_pos 0
#define	reg_tpsd_hpcr_len 3
#define	reg_tpsd_hpcr_lsb 0
#define xd_g_reg_tpsd_lpcr	0xA3C2
#define	reg_tpsd_lpcr_pos 3
#define	reg_tpsd_lpcr_len 3
#define	reg_tpsd_lpcr_lsb 0
#define xd_g_reg_ofsm_clk	0xA3D0
#define	reg_ofsm_clk_pos 0
#define	reg_ofsm_clk_len 3
#define	reg_ofsm_clk_lsb 0
#define xd_g_reg_fclk_cfg	0xA3D1
#define	reg_fclk_cfg_pos 0
#define	reg_fclk_cfg_len 1
#define	reg_fclk_cfg_lsb 0
#define xd_g_reg_fclk_idi	0xA3D1
#define	reg_fclk_idi_pos 1
#define	reg_fclk_idi_len 1
#define	reg_fclk_idi_lsb 0
#define xd_g_reg_fclk_odi	0xA3D1
#define	reg_fclk_odi_pos 2
#define	reg_fclk_odi_len 1
#define	reg_fclk_odi_lsb 0
#define xd_g_reg_fclk_rsd	0xA3D1
#define	reg_fclk_rsd_pos 3
#define	reg_fclk_rsd_len 1
#define	reg_fclk_rsd_lsb 0
#define xd_g_reg_fclk_vtb	0xA3D1
#define	reg_fclk_vtb_pos 4
#define	reg_fclk_vtb_len 1
#define	reg_fclk_vtb_lsb 0
#define xd_g_reg_fclk_cste	0xA3D1
#define	reg_fclk_cste_pos 5
#define	reg_fclk_cste_len 1
#define	reg_fclk_cste_lsb 0
#define xd_g_reg_fclk_mp2if	0xA3D1
#define	reg_fclk_mp2if_pos 6
#define	reg_fclk_mp2if_len 1
#define	reg_fclk_mp2if_lsb 0
#define xd_I2C_i2c_m_slave_addr	0xA400
#define	i2c_m_slave_addr_pos 0
#define	i2c_m_slave_addr_len 8
#define	i2c_m_slave_addr_lsb 0
#define xd_I2C_i2c_m_data1	0xA401
#define	i2c_m_data1_pos 0
#define	i2c_m_data1_len 8
#define	i2c_m_data1_lsb 0
#define xd_I2C_i2c_m_data2	0xA402
#define	i2c_m_data2_pos 0
#define	i2c_m_data2_len 8
#define	i2c_m_data2_lsb 0
#define xd_I2C_i2c_m_data3	0xA403
#define	i2c_m_data3_pos 0
#define	i2c_m_data3_len 8
#define	i2c_m_data3_lsb 0
#define xd_I2C_i2c_m_data4	0xA404
#define	i2c_m_data4_pos 0
#define	i2c_m_data4_len 8
#define	i2c_m_data4_lsb 0
#define xd_I2C_i2c_m_data5	0xA405
#define	i2c_m_data5_pos 0
#define	i2c_m_data5_len 8
#define	i2c_m_data5_lsb 0
#define xd_I2C_i2c_m_data6	0xA406
#define	i2c_m_data6_pos 0
#define	i2c_m_data6_len 8
#define	i2c_m_data6_lsb 0
#define xd_I2C_i2c_m_data7	0xA407
#define	i2c_m_data7_pos 0
#define	i2c_m_data7_len 8
#define	i2c_m_data7_lsb 0
#define xd_I2C_i2c_m_data8	0xA408
#define	i2c_m_data8_pos 0
#define	i2c_m_data8_len 8
#define	i2c_m_data8_lsb 0
#define xd_I2C_i2c_m_data9	0xA409
#define	i2c_m_data9_pos 0
#define	i2c_m_data9_len 8
#define	i2c_m_data9_lsb 0
#define xd_I2C_i2c_m_data10	0xA40A
#define	i2c_m_data10_pos 0
#define	i2c_m_data10_len 8
#define	i2c_m_data10_lsb 0
#define xd_I2C_i2c_m_data11	0xA40B
#define	i2c_m_data11_pos 0
#define	i2c_m_data11_len 8
#define	i2c_m_data11_lsb 0
#define xd_I2C_i2c_m_cmd_rw	0xA40C
#define	i2c_m_cmd_rw_pos 0
#define	i2c_m_cmd_rw_len 1
#define	i2c_m_cmd_rw_lsb 0
#define xd_I2C_i2c_m_cmd_rwlen	0xA40C
#define	i2c_m_cmd_rwlen_pos 3
#define	i2c_m_cmd_rwlen_len 4
#define	i2c_m_cmd_rwlen_lsb 0
#define xd_I2C_i2c_m_status_cmd_exe	0xA40D
#define	i2c_m_status_cmd_exe_pos 0
#define	i2c_m_status_cmd_exe_len 1
#define	i2c_m_status_cmd_exe_lsb 0
#define xd_I2C_i2c_m_status_wdat_done	0xA40D
#define	i2c_m_status_wdat_done_pos 1
#define	i2c_m_status_wdat_done_len 1
#define	i2c_m_status_wdat_done_lsb 0
#define xd_I2C_i2c_m_status_wdat_fail	0xA40D
#define	i2c_m_status_wdat_fail_pos 2
#define	i2c_m_status_wdat_fail_len 1
#define	i2c_m_status_wdat_fail_lsb 0
#define xd_I2C_i2c_m_period	0xA40E
#define	i2c_m_period_pos 0
#define	i2c_m_period_len 8
#define	i2c_m_period_lsb 0
#define xd_I2C_i2c_m_reg_msb_lsb	0xA40F
#define	i2c_m_reg_msb_lsb_pos 0
#define	i2c_m_reg_msb_lsb_len 1
#define	i2c_m_reg_msb_lsb_lsb 0
#define xd_I2C_reg_ofdm_rst	0xA40F
#define	reg_ofdm_rst_pos 1
#define	reg_ofdm_rst_len 1
#define	reg_ofdm_rst_lsb 0
#define xd_I2C_reg_sample_period_on_tuner	0xA40F
#define	reg_sample_period_on_tuner_pos 2
#define	reg_sample_period_on_tuner_len 1
#define	reg_sample_period_on_tuner_lsb 0
#define xd_I2C_reg_rst_i2c	0xA40F
#define	reg_rst_i2c_pos 3
#define	reg_rst_i2c_len 1
#define	reg_rst_i2c_lsb 0
#define xd_I2C_reg_ofdm_rst_en	0xA40F
#define	reg_ofdm_rst_en_pos 4
#define	reg_ofdm_rst_en_len 1
#define	reg_ofdm_rst_en_lsb 0
#define xd_I2C_reg_tuner_sda_sync_on	0xA40F
#define	reg_tuner_sda_sync_on_pos 5
#define	reg_tuner_sda_sync_on_len 1
#define	reg_tuner_sda_sync_on_lsb 0
#define xd_p_mp2if_data_access_disable_ofsm	0xA500
#define	mp2if_data_access_disable_ofsm_pos 0
#define	mp2if_data_access_disable_ofsm_len 1
#define	mp2if_data_access_disable_ofsm_lsb 0
#define xd_p_reg_mp2_sw_rst_ofsm	0xA500
#define	reg_mp2_sw_rst_ofsm_pos 1
#define	reg_mp2_sw_rst_ofsm_len 1
#define	reg_mp2_sw_rst_ofsm_lsb 0
#define xd_p_reg_mp2if_clk_en_ofsm	0xA500
#define	reg_mp2if_clk_en_ofsm_pos 2
#define	reg_mp2if_clk_en_ofsm_len 1
#define	reg_mp2if_clk_en_ofsm_lsb 0
#define xd_r_mp2if_sync_byte_locked	0xA500
#define	mp2if_sync_byte_locked_pos 3
#define	mp2if_sync_byte_locked_len 1
#define	mp2if_sync_byte_locked_lsb 0
#define xd_r_mp2if_ts_not_188	0xA500
#define	mp2if_ts_not_188_pos 4
#define	mp2if_ts_not_188_len 1
#define	mp2if_ts_not_188_lsb 0
#define xd_r_mp2if_psb_empty	0xA500
#define	mp2if_psb_empty_pos 5
#define	mp2if_psb_empty_len 1
#define	mp2if_psb_empty_lsb 0
#define xd_r_mp2if_psb_overflow	0xA500
#define	mp2if_psb_overflow_pos 6
#define	mp2if_psb_overflow_len 1
#define	mp2if_psb_overflow_lsb 0
#define xd_p_mp2if_keep_sf_sync_byte_ofsm	0xA500
#define	mp2if_keep_sf_sync_byte_ofsm_pos 7
#define	mp2if_keep_sf_sync_byte_ofsm_len 1
#define	mp2if_keep_sf_sync_byte_ofsm_lsb 0
#define xd_r_mp2if_psb_mp2if_num_pkt	0xA501
#define	mp2if_psb_mp2if_num_pkt_pos 0
#define	mp2if_psb_mp2if_num_pkt_len 6
#define	mp2if_psb_mp2if_num_pkt_lsb 0
#define xd_p_reg_mpeg_full_speed_ofsm	0xA501
#define	reg_mpeg_full_speed_ofsm_pos 6
#define	reg_mpeg_full_speed_ofsm_len 1
#define	reg_mpeg_full_speed_ofsm_lsb 0
#define xd_p_mp2if_mpeg_ser_mode_ofsm	0xA501
#define	mp2if_mpeg_ser_mode_ofsm_pos 7
#define	mp2if_mpeg_ser_mode_ofsm_len 1
#define	mp2if_mpeg_ser_mode_ofsm_lsb 0
#define xd_p_reg_sw_mon51	0xA600
#define	reg_sw_mon51_pos 0
#define	reg_sw_mon51_len 8
#define	reg_sw_mon51_lsb 0
#define xd_p_reg_top_pcsel	0xA601
#define	reg_top_pcsel_pos 0
#define	reg_top_pcsel_len 1
#define	reg_top_pcsel_lsb 0
#define xd_p_reg_top_rs232	0xA601
#define	reg_top_rs232_pos 1
#define	reg_top_rs232_len 1
#define	reg_top_rs232_lsb 0
#define xd_p_reg_top_pcout	0xA601
#define	reg_top_pcout_pos 2
#define	reg_top_pcout_len 1
#define	reg_top_pcout_lsb 0
#define xd_p_reg_top_debug	0xA601
#define	reg_top_debug_pos 3
#define	reg_top_debug_len 1
#define	reg_top_debug_lsb 0
#define xd_p_reg_top_adcdly	0xA601
#define	reg_top_adcdly_pos 4
#define	reg_top_adcdly_len 2
#define	reg_top_adcdly_lsb 0
#define xd_p_reg_top_pwrdw	0xA601
#define	reg_top_pwrdw_pos 6
#define	reg_top_pwrdw_len 1
#define	reg_top_pwrdw_lsb 0
#define xd_p_reg_top_pwrdw_inv	0xA601
#define	reg_top_pwrdw_inv_pos 7
#define	reg_top_pwrdw_inv_len 1
#define	reg_top_pwrdw_inv_lsb 0
#define xd_p_reg_top_int_inv	0xA602
#define	reg_top_int_inv_pos 0
#define	reg_top_int_inv_len 1
#define	reg_top_int_inv_lsb 0
#define xd_p_reg_top_dio_sel	0xA602
#define	reg_top_dio_sel_pos 1
#define	reg_top_dio_sel_len 1
#define	reg_top_dio_sel_lsb 0
#define xd_p_reg_top_gpioon0	0xA603
#define	reg_top_gpioon0_pos 0
#define	reg_top_gpioon0_len 1
#define	reg_top_gpioon0_lsb 0
#define xd_p_reg_top_gpioon1	0xA603
#define	reg_top_gpioon1_pos 1
#define	reg_top_gpioon1_len 1
#define	reg_top_gpioon1_lsb 0
#define xd_p_reg_top_gpioon2	0xA603
#define	reg_top_gpioon2_pos 2
#define	reg_top_gpioon2_len 1
#define	reg_top_gpioon2_lsb 0
#define xd_p_reg_top_gpioon3	0xA603
#define	reg_top_gpioon3_pos 3
#define	reg_top_gpioon3_len 1
#define	reg_top_gpioon3_lsb 0
#define xd_p_reg_top_lockon1	0xA603
#define	reg_top_lockon1_pos 4
#define	reg_top_lockon1_len 1
#define	reg_top_lockon1_lsb 0
#define xd_p_reg_top_lockon2	0xA603
#define	reg_top_lockon2_pos 5
#define	reg_top_lockon2_len 1
#define	reg_top_lockon2_lsb 0
#define xd_p_reg_top_gpioo0	0xA604
#define	reg_top_gpioo0_pos 0
#define	reg_top_gpioo0_len 1
#define	reg_top_gpioo0_lsb 0
#define xd_p_reg_top_gpioo1	0xA604
#define	reg_top_gpioo1_pos 1
#define	reg_top_gpioo1_len 1
#define	reg_top_gpioo1_lsb 0
#define xd_p_reg_top_gpioo2	0xA604
#define	reg_top_gpioo2_pos 2
#define	reg_top_gpioo2_len 1
#define	reg_top_gpioo2_lsb 0
#define xd_p_reg_top_gpioo3	0xA604
#define	reg_top_gpioo3_pos 3
#define	reg_top_gpioo3_len 1
#define	reg_top_gpioo3_lsb 0
#define xd_p_reg_top_lock1	0xA604
#define	reg_top_lock1_pos 4
#define	reg_top_lock1_len 1
#define	reg_top_lock1_lsb 0
#define xd_p_reg_top_lock2	0xA604
#define	reg_top_lock2_pos 5
#define	reg_top_lock2_len 1
#define	reg_top_lock2_lsb 0
#define xd_p_reg_top_gpioen0	0xA605
#define	reg_top_gpioen0_pos 0
#define	reg_top_gpioen0_len 1
#define	reg_top_gpioen0_lsb 0
#define xd_p_reg_top_gpioen1	0xA605
#define	reg_top_gpioen1_pos 1
#define	reg_top_gpioen1_len 1
#define	reg_top_gpioen1_lsb 0
#define xd_p_reg_top_gpioen2	0xA605
#define	reg_top_gpioen2_pos 2
#define	reg_top_gpioen2_len 1
#define	reg_top_gpioen2_lsb 0
#define xd_p_reg_top_gpioen3	0xA605
#define	reg_top_gpioen3_pos 3
#define	reg_top_gpioen3_len 1
#define	reg_top_gpioen3_lsb 0
#define xd_p_reg_top_locken1	0xA605
#define	reg_top_locken1_pos 4
#define	reg_top_locken1_len 1
#define	reg_top_locken1_lsb 0
#define xd_p_reg_top_locken2	0xA605
#define	reg_top_locken2_pos 5
#define	reg_top_locken2_len 1
#define	reg_top_locken2_lsb 0
#define xd_r_reg_top_gpioi0	0xA606
#define	reg_top_gpioi0_pos 0
#define	reg_top_gpioi0_len 1
#define	reg_top_gpioi0_lsb 0
#define xd_r_reg_top_gpioi1	0xA606
#define	reg_top_gpioi1_pos 1
#define	reg_top_gpioi1_len 1
#define	reg_top_gpioi1_lsb 0
#define xd_r_reg_top_gpioi2	0xA606
#define	reg_top_gpioi2_pos 2
#define	reg_top_gpioi2_len 1
#define	reg_top_gpioi2_lsb 0
#define xd_r_reg_top_gpioi3	0xA606
#define	reg_top_gpioi3_pos 3
#define	reg_top_gpioi3_len 1
#define	reg_top_gpioi3_lsb 0
#define xd_r_reg_top_locki1	0xA606
#define	reg_top_locki1_pos 4
#define	reg_top_locki1_len 1
#define	reg_top_locki1_lsb 0
#define xd_r_* Common head2	0xA606 of the /* Common head2_pos 5h 9005
 * USB1.1 DVB-T rlen 1er.
 *
 * Copyright (C) 2file of the Linup drivdummy_7_0 Afate8h 9005
 * USBo Afatechreceie of the vided informati2007indly provided informatioso.org)
 *
 * Thanks to Afat15_8 Afate9*
 * This program iundeion.
 *
 * This program ieral ee software; you can redhed bysbsoftware; * Thanks to Afat23_16 AfateA*
 * This program i Liceion.
 *
 * This program i) any ee software; you can reds progrsb 1ch 9005
 * Thanks to Afat31_24 AfateB*
 * This program it WITion.
 *
 * This program i even ee software; you can red * MERCsb 24it will be useful,
 * but9_3e AfateCABILITY or FITNESS F GNU the implied warranty of
  moreCHANTABILITY or FITNESS Fd havesb 32rg)
 *
 * Thanks to Afat47_4h who kD*
 * This program i withion.
 *
 * This program ite to ee software; you can redndationsb 4org)
 *
 * Thanks to Afat55_4er the E*
 * This program iocumeion.
 *
 * This program i for mee software; you can reddef _DVe, M; either version 2 of the63_5ense, oF*
 * This program i_LOG_ion.
 *
 * This program idvb-usee software; you can red_af9005sb 5 it will be useful,
 * bu71_6THOUT 1 *
 * This program isug,0ion.
 *
 * This program isargs.ee software; you can redi5_debusb 6CULAR PURPOSE.  See the
 79_7e Afat1 Luca Olivettib_af9005_deion.
 *
 * This program isreg(aee software; you can rediaf9005sb 7l Public License
 * along87_8h who 1l Public #define deb_af90ion.
 *
 * This program ie deb_ee software; you can redb_usb_aion;org)
 *
 * Thanks to Afat95_8er the13g,0x10,args)
#definfirmwion.
 *
 * This program iSIZE 2ee software; you can redW_CONFIion;; either version 2 of the103_9ense, 1CULAR PUR Software FoEG  0 Public License as publish

#defy
 * the Free Software Fo  0x20
sb 96
rg)
 *
 * Thanks tunplug_flag
#defiver.
 *
 * Cop_OFDM_REG 0ion.
 *
 * This proMD_TUNER    2007 Luca Olivetti5_CMD_BURST  that04
#define AF9005_CMDapi_dca_stes_request   00
#define AF9 005_CMD_READ     0x00
#drecei Luca OliWRITE    0x01

/* af9005 r2007 Luca Olidefine APO_REG_RESET					0xfile rg)
 *
 * Thanks tback_to_READEG 0x00
#define AF9005_Cne APO_REG_I2C_Rreceiug,0x10,args)
   0xF001
#define2007 Luca OlivettiILICON_TUNER      INC  

#define AF9005_CMDCMD_retrain 0x00
#defiine AF9005_CMD_WRITE             0xFFFFrecei
#define  */

/*******************xAEFF

#define APO_REG*****************           0xF000
#defiDyn_Top_TryI2C_RW_SILICON_TUNER     ****************************************************2007 Luca Olivetti#define xd_p_reg_aINC  7       0xF000
#defiAPIollo RegifreezeI2C_RW_SILICON_TUNER     #define	reg_aagc_invertreceine AF9005_TUNE	reg_aagc_inverted_agc_l2007 Luca Olivettireg_aagc_sign_only	0xA000INC  8       0xF000
#defi_OFDM_R11_100x01,arver.
 *
 * Cop#define	reg_a Public License as publish xd_p_ry
 * the Free Software Fo0
#definINC  0x_sign_only_len 1
#define	9_11ebug,0xch 9005
 * USBn_len 1
#defireg_aagc_slow_adc_en	0xA0000
#defne	reg_aagc_slow_adc_en_po_scale	thatbug,0x10,aine AF9005_OFDM_R27_12005_deb7e AF9005_TUNER_REG 1low_ad Public License as publishagc_sloy
 * the Free Software Fo xd_p_rethat2e it and/or modify
 * it u35_12ware */ the Free Software Fodc_loc Public License as publisheck_sloy
 * the Free Software Foreg_aagc1
#deagc_sign_only_len 1
#define43_130
#defiterms of the GNU GeneA001
# Public License as publishl_pos 1y
 * the Free Software Fontrol_lethat3

#define AF9005_CMDCCIR_disdefine	reg_aagc_initaagc_totion.
 *
 * This prodefine	re2007 Luca Olivettisel_pos 2
           0xF000
#defiware Fou1_140x01,arr
 * (at your optionotal_ga Public License as publisheg_aagcy
 * the Free Software Foug_aagc_that40x04
#define AF9005_ xd_p_re9_15ebug,0xNY WARRANTY; withoutut_inv_ Public License as publisheint_eny
 * the Free Software Fount_en_pthat52_len 1
#define	reg_aagc_ou67_16005_debal Public License fo0
#defi Public License as publishe_flag	y
 * the Free Software Foock_chanthat l_len 2
#define	reg_aagc_to75_16ware */program; if not, wrieg_aagc Public License as publishfine xdy
 * the Free Software Foale_acquthat agc_sign_only_len 1
#define83_170
#defin/dvb/README.dvb-usbine	reg Public License as publishire_leny
 * the Free Software Foop_bw_scthat7

#define AF9005_CMDofsm_read_rbc_en*  alson/dvb/REAWRITEtrack	0xA003
#dee APO_REG_GPIO_loop_bw_scale_track_pxAEFF

#define APOagc_rf_loop_bw_sca           0xF000
#defice_filter_selection_totfine	reg_aagc_rf_loop 0
#define xd_p_reg_aagregisters */
#definquire	0xA004
#define	regxAEFF

#define APOcale_acquire_pos 0
#defin           0xF000
#defiOFSM_versreg_controltech*  alsoF

#define APOp_bw_scale_acquire_lsb 0ion.
 *
 * Thig_aagc_if_loop_bw_scale_track	ee software; e	reg_aagc_if_loop_bw_scale_tra#define	reg_aagc_if_loop_bw_scale_acquire_lsunde*  also "af9005"g_aagc_if_loop_bw_scale_treral Public License xd_p_reg_aagc_max_rf_agc_7_0	ack_pos 0
#define	reg_aagc_if_loop_bw_soundationfine	reg_aagc_if_loop_bw_scale_acquire_ls Lice*  alsefine	reg_agc_7_0_lsb 0
#define xd_p_reg_	0xA006
#define	reg_aagc_max_rf_agc_7_0_s program is distri9_8	0xA007
#define	reg_aagc_max_n_sel_len 2
#define	reg_aagc_to91_180x01,ar "af9005"
#include "f_agc_7 Public License as publishmin_rf_y
 * the Free Software Fo_aagc_mithat8v_len 1
#define	reg_aagc_ou99_19e Afatefine	reg__len 8
#definxd_p__agc_7_0_pos 0
#define	reg_09
#dein_rf_agc_7_0_len 8
#defin_8_pos that9efine	reg_aagc_int_ece_en AfaBCgc_min_rf_agc_min_rion.
 *
 * This proine xd2007 Luca Olivetti_agc_7_so.org)
 *
 * Thanks t 0
#ctrln_rf_agc_9_8_lsb 8
#defineefine	reregisters */
#gc_7_0_len 8
#def_0	0xA00A
#define	reggc_7_0_lsbc_max_if_agc_7_0_pos 0
#deste_tdig_aagc_max_if_agc_7_0_leeg_aagce APO_REG_GPIO_RW_S0
#define	reb 0
#define xd_p_reg_alen 2
#dec_max_if_agc_7_0_pos 0
#ddynamicf_agc_9_8_lsb 8
#definec_min_i*****************/
fine	reg_aa_0	0xA00A
#define	reg 0
#definc_max_if_agc_7_0_pos 0
#dconff_agc_9_8_lsb 8
#definef_aglsb 0
#define xd_p_xd_p_reg2007gc_max_if_agc_9_80xA00Dsb 8
#define xd_p_reg_aagc_mfine	rc_7_0	0xA00C
#define	rdefine ch 9005
 * USB
#define	ne	reg_aagc_min_if_agc_7_adc_sc8
#define xd_p_reg_aagcerotn_rf_agc_9_8_lsb 8
#definefine	regreceie_len 5
#definale_pos 0
#dne	reg_aagc_min_if_agcmple_scalsb 8
#define xd_p_reg_aagc_min_i_thtech whoBCeg_aagc_min_if_agc_7_0_leeg_aagd_p_reg_aagc_max_if_aguire	0xA00F
#deee software; you cgc_lock_scale_acquisample_scale_lsb 0
#define xd_p_reg_under thBCgc_max_if_agc_9_8fine	reg_aagc_refine	reg_aagc_rf_agc_lock_scale_hed by
 * the Free Softuire_lsb 0
#define tion; either version 2 ce_s1f_agc_in_if_agc_7_0_poss1d_p_reg_aagc_max_if_ags1uire_ver.
 *
 * Cop	reg_aak_sample_scale	0xA00E
#devar_forced_valueg_aagc_rf_agc_lock_scal_aagc_if_agc_locreceiver.
 *
 * Cop10
#define	reg_aagc_2007e_acquire	0xA010
#define	reg_aagc_ck_scale_acquire_len 3
#defata_im_aagc_rf_ac_min_if_agc_9_8	gc_lock_scaefine	reg_aagc_rf_agc_l xd_p_reg_a	0xA00F
#define	reg_aatrack	0xA010n 3
#define	reg_aagc_if_agc_lock_rf_agc_gc_lock_scale_acqg_aagc_ifaagc_if_agc_lock_scale_track	0 0
#deeg_aagc_min_if_agck_scale_trn 3
#define	reg_aagc_if_agc_lore_6agc_rf_aock_scale_track_len 3
definefine	reg_aagc_max_if_ator_7_0_pos 2007e	reg_aagc_lock_satop_numerato_rf_top_numerator_7_0	0xA011
#def8_7f_agc_aagc_min_if_agc_9_fine xd_p_aagc_if_agc_lock_scale_trac8	0xA01D
#define	reg_aagc_mi_top_numeratoi2c(_track_pos 3
#define	tone_5agc_rf_aaagc_min_if_agc_9efine	reeg_aagc_max_if_agc_9_8_9_8_lsb 2007_rf_top_numerator_9_8_lsb top_numerator_7_0_lsb 0
#define12_ense,BCe	reg_aagc_lock_s7_0_pos 0d_p_reg_aagc_max_if_agp_numeratoor_7_0_len 8
#define	eg_aagc_if_gs..define	reg_aagc_min_ifeire_id_drift_th
#defi00F
#define	reg_aerator_9_8	0xA014d_p_reg_aagc_max_if_agp_numerator_9_8_po	0xA00F
#define	reg_aif_top_numerator_9_ 8
#define	reg_aagc_min_ifrator_9_count_max
#defiterms of the Gd_p_reg_aagc_adc_out_os 0
#define	reg_aagc_if_top_nuagc_adc_ou2007c_min_if_agc_9_8	0efine	reg_aagc_adc_or_9_8_lsb 8
#define xd_p_reg_aagcbias_inc_aagc_rf_ar
 * (at your  0
#define xd_p_reg_aagcos 0
#define	reg_aagc_if_top_nue	reg_aagc_ad_8_len 2
#define	reg_aagc_if_treg_aagc_adc_odc_out_desired_7_0_lsb 0
#define xd_p_reg_arf_agc_NY WARRANTY; wne xd_p_reg_aagc_fixeddc_out_desired_8_pos 0
#define	reg_aagc__track_lsb 0
#define xdfixed_gain_len 1
#defck_lsb 0
#define xd_p_reg_aagth0_aagc_rf_aal Public Liceth	0xA016
#defd_p_reg_aagc_max_if_ag_th_pos 4
#d	0xA00F
#define	reg_aount_th_len 4p_reg_aagc_lock_count_th	0xA016
#gc_rf_agc_program; if noaagc_fixed_rf_adefine	reg_aagc_lock_count_th_track	0xA00F
#define	reg_aol_7_0_pos 0
#cale_track_pos 3
#define	0xA0161_aagc_rf_an/dvb/README.deg_aagc_fixed_define	reg_aagc_lock_count_tine xd#define	reg_aagc_fixed_rf_antrol_1p_reg_aagc_lock_count_th	0xA0161_rf_agc_con "af9005"
#incos 0
#define	red_p_reg_aagc_fixed_rf_agc_contos 0
#define	reg_aagc_fixed_rf_af_agc_cool_7_0_len 8
#define	reg_aagc_f2_aagc_rf_De	reg_aagc_lock_count_t	0xA0define	reg_aagc_lock_count_tagc_co#define	reg_aagc_fixed_rf_aeg_aagcp_reg_aagc_lock_count_th	0xA0162_rf_agc_coDxA00A
#define	regf_agc_controontrol_23_16_pos 0
#define	regos 0
#define	reg_aagc_fixed_rf_a	0xA01A
ol_7_0_len 8
#define	reg_aagc_f30xA019
#degc_max_if_agc_9_8eg_aagc_fixdefine	reg_aagc_lock_count_t
#defi#define	reg_aagc_fixed_rf_antrol_3p_reg_aagc_lock_count_th	0xA0163ontrol_23_1e_acquire	0xA010
#def01B
#define	reg_aagc_fixed_rf_agc_contos 0
#define	reg_aagc_fixed_rf_axed_if_aol_7_0_len 8
#define	reg_aagc_f40xA019
#dec_min_if_agc_9_8	7_0_lsb 0
#define	reg_aagc_lock_count_t_agc_c#define	reg_aagc_fixed_rf_areg_aagp_reg_aagc_lock_count_th	0xA0164ontrol_23_1gc_lock_scale_acquirec_controcontrol_15_8	0xA01C
#define	reos 0
#define	reg_aagc_fixed_rf_adefine xol_7_0_len 8
#define	reg_aagc_f50xA019
#deaagc_min_if_agc_9aagc_fixed_define	reg_aagc_lock_count_tefine	#define	reg_aagc_fixed_rf_aol_23_1p_reg_aagc_lock_count_th	0xA0165ontrol_23_1e	reg_aagc_lock_s
#define xd_	reg_aagc_fixed_if_agc_controlos 0
#define	reg_aagc_fixed_rf_aixed_if_ol_7_0_len 8
#define	reg_aagc_f60xA019
#deine	reg_aagc_fixed_rf_a24_ledefine	reg_aagc_lock_count_t_agc_c#define	reg_aagc_fixed_rf_a xd_p_rp_reg_aagc_lock_count_th	0xA0166ontrol_23_1d_7_0	0xA015
#def_agc_unlock_control_30_24_lsb 24
#define xos 0
#define	reg_aagc_fixed_rf_a#define	cale_track_pos 3
#define	efine	resetl_23_1ut_desired_8	0xA0c_if_agc_und_p_reg_aagc_max_if_agdefine	reg_ab 0
#define xd_p_reg_aagc_m_pos 0
or_9_8_lsb 8
#define xd_p_reg_auto_clrn_rf_agcnumerator	0xA020
#_aagc_if_agc_unlfine	reg_aagc_max_if_afine xd_p_reg_unpfine	reg_aagc_fixed_gain__unplug_th_posk_sample_scale	0xA00E
#deefine	_if_a	reg_anlock_numerator_lsb 0
#define xd_p_reg_weak_eg_aagc_max_if_agc_9_8_#define	reg_weak_sib 0
#define xd_p_reg_aagc_me	reg_weak_signck_lsb 0
#define xd_p_reg_aagc_if_agnlock_numerator_lsb 0
#de xd_p_reg_unpagc_min_if_agc_7_0_posdefine	reg_unp_0	0xA00A
#define	reg#define	reg_unpmerator_len 6
#define	reg_aagcp_reg_unplug_rf_gain_th 0xA023
#fine xd_p_reg_g_aagc_min_if_agc_9_8	0xA024
#define	s 0
#define	reg_unplug_th_fine	reg_unplug_rf_gain_th_lsb 0
#de_aagut_desirf_gain_th 0xA023
#defic_out_degc_lock_scale_acquire_adc_outrf_gain_th_pos 0
#defagc_adc_out_desired_7_0_lsb 0
#deffine	reg_aagc_xA019
#de0xA016
#define	reg_aaop_if_gain_th_lenos 0
#define	reg_aagc_if_tif_gain_th_lsb 0
#_8_len 2
#define	reg_aagcer_at_unplug_en 0xAh_pos 0
#define	reg_unplug_dtop_if_gain_th_11rol_23_1g_aagc_lock_countin_th_pos 0
_en_len 1
#define xd_p_reg_top_recover_at_unplug_e0
#de_out_desired_7_0_len 8
#d0xA027
#define	reg_asb 0
#define xd_p_reg_aagc_if_agd_th_lstrol_7_0	0xA017
#sb 0
#deaagc_if_agc_unlock_numerator_d0
#define	reg_aagc_if_agc_unldut_desired_8_lsb 0
#define xd_p_reg_c_ouine	reg_xd_p_reg_aagc_rf_x
#define xd_p_replug_th	0xA021
#define	reg	reg_aagc_rf_or_7_0_len 8
#define	aagc_rf_x8_len 8
#fine	reg_aagc_rf_x7_lsb 0
#define xd_p11p_reg_aD_control_7_0_lsb #define	reg_aagc_os 0
#define	reg_aagc_if_top_nux9_len 8
agc_rf_agc_lock_scale9_lsb 0
#define xd_op_numerator_9_8_len 2
#dvarrf_x9__fixed_rf_agc_controlefine	reg_aagc_lock_countfine	reg_unplug_dtop_if_ge	reg_aagc_rf_x0_lsb 0
#define xrdyaagc_rf_x10_len 8
#defi_x11_pos agc_min_if_agc_7_0_pos11_len 8
#define	reg_aagc_rf_x7_len 8
#yxd_p_reg_aagc_rf_x9	0xA02A
#define	rout_3reg_aagc_fixed_rf_agc_con_pos 0
#define	r	reg_unplug_dtop_rf_gain_te	reg_aagc_rf_out_desired_7_0_len 8
#definegc_rf_x13D
#define	reg_aagc_rf_x12_pos 0
#defin11_THOUTBE#define	reg_aagc_if_top_nuine	reg_os 0
#define	reg_aagc_if_top_nueg_aagc_i_8_len 2
#define	reg_aagc_if_tf_x0_pos 0e, M_track_pos 3
#define	d_p_raagc_rf_ExA00A
#define	regne xd_p_reg_aagc_rf_x10_lsb 0
define	re	0xA00F
#define	reg_aefine	reg_k_sample_scale	0xA00E
#ded_p_rn 1
#defiEgc_max_if_agc_9_8d_p_reg_aeg_aagc_if_x1_pos 0
#define	reg_aagc_rf_x6_pos 0
#defreg_aagc_ifcale_track_pos 3
#define	m1ne	reg_aaf_x2	0xA031
#definaagc_ig_aagc_min_if_agc_9_8	_aagc_i_out_desired_7_0_len g_aagc_ik_sample_scale	0xA00E
#dem1	reg_aagc_rin_if_agc_7_0_posreg_aagreg_aagc_rf_x10_lsb 0
reg_aagc	0xA00F
#define	reg_areg_aagc_eg_aagc_if_x0_lsb 0
#defir6
#define	rEc_min_if_agc_9_8	reg_aagreg_aagc_rf_x10_lsb 0
reg_aagc	0xA00F
#define	reg_areg_aagc_k_sample_scale	0xA00E
#dereg_gc_rf_agcEgc_lock_scale_acqreg_aagcc_if_x5_pos 0
#define	reg_track	0xA00F
#define	reg_ag_aagc_if_cale_track_pos 3
#define	reg_ License,BEaagc_min_if_agc_9aagc_if_xc_if_x5_pos 0
#define	reg_s program is distributed_aagc_if_x7_p that it will be useful,_aagc_it WITHOUTBEe	reg_aagc_lock_s8	0xA037
c_if_x5_pos 0
#define	reg_ * MERCHANTABILITY or FIx8_pos 0
#defPARTICULAR PURPOSE.  Seex8_po3k	0xlsb 0#defing_aagc_if_x8_lsb 0
efine	reg_c_if_x5_pos 0
#define	rereg_aagc_ife	reg_aagc_if_x8_lsb 0
agc_if_x9_lsif_x5_lsb 0
#define xd_p_reefine	raagc_if_x6d_7_0	0xA015
#def_x10_pos 0
#df_x9_len 8
#define	reg_aagc_if_gc_if_x6_len 8
#define	reg_aefine xd_p_re6_lsb 0
#define xd_p_reg_aaefine	r_if_x7	0xA0ut_desired_8	0xA0eg_aagc_if_x11f_x9_len 8
#define	reg_aagc_if_x7_len 8
#define	reg_aagc_if__x12	0xA03B
#d#define xd_p_reg_aagc_if_x8	efine	rA037
#defin0xA016
#define	reine	reg_aagc_if_x9_len 8
#define	reg_aagc_if_#define	reg_aagc_if_x8_lsb 0
agc_if_x13_posd_p_reg_aagc_if_x9	0xA038
#defimagd_p_reg_aag_aagc_lock_count
#define xd_f_x9_len 8
#define	reg_aagfor_dca	0s 0
#define	reg_aagc_if_xn_rf_ctl_8xA039
#define	reg_aagc_if_x10fine aagc_if_x6trol_7_0	0xA017
#len 8
#define0xA03D
#define	reg_aagc_min_rf_d_p_reg_aagc_if_x11	0xA03A
#demin_if_ctl_gc_if_x11_pos 0
#define	reg_afine _if_x7	0xA0_control_7_0_lsb _pos 0
#define0xA03D
#define	reg_aagc_min_rf_A03B
#define	reg_aagc_if_x12_po_ctl_8bit_foreg_aagc_if_x12_len 8
#define	fine A037
#defin_fixed_rf_agc_cone	reg_aagc_tot0xA03D
#define	reg_aagc_min_rf_x13_pos 0
#define	reg_aagc_if_x	reg_aagc_toARTICULAR PURPOSE.  Seefeq_fix_e6	0xA019
#dFgc_min_rf_agc_A071
#define	rereg_aagc_rf_x10_lsb_8_pos 0
#defineee software; you cin_15_8_len 8
#dek_sample_scale	0xA00E
A071
#definegc_rf_agcF Luca Olivettic_in_sat_cnt_7_0e	reg_aagc_total_gain_15_8_len hed by
 * the Free Softeg_aagc_in_sat_cntion; either version 2 A071
#define_if_x7	0xAFug,0x10,args)
xd_p_reg_aagc_in_e	reg_aagc_total_gain_15_8_len s program is distributed 0
#define	reg_aagcthat it will be useful,A071
#defineA037
#defiF
#define FW_BU 8
#define xd_p_re	reg_aagc_total_gain_15_8_len  * MERCHANTABILITY or FIcnt_23_16_pos 0
#ded_p_reg_aagc_if_x9	0xA038
m2definee	reg_aagc_F3_len 8
#define	ret_23_16_lsb 1c_if_x4_pos 0
#define	rin_sat_cnt_31__if_x4_len 8
#define	raagc_in_sat_cntif_x3_lsb 0
#define xd_p_rt_23_16_ls_7_0	0xA07gc_lock_scale_acqreg_aagc_in_sat_24	0xA077
#define	reg_aagc_in_satrack	0xA00F
#define	reg_ane xd_r_reg_aagc8
#define	reg_aagc_rf_x6_lsftshifnlock_Faagc_min_if_agc_9olt_7_0_aagc_if_agc_unlock_numeigital_r_out_desired_7_0_len ine xd_r_r	reg_aagc_rf_x0_lsb 0
#deflt_7_0_eg_aag
#define	reg_aagc_digital__trackaagc_digital_rf_volt_9_8	_aagc_l_rf_volt_9_8_len 2
#define	A079
#define	reg_aagc_digital_rf__min_iFe	reg_aagc_lock_s_digital_rf_volt_7_0_lsb 0
#define xd_rsb 8
#aagc_digital_rf_volt_9_8	ntoso.org)
 *
 * Thanks t_lsb p_mobilck_scalt_7_0	0xA07A
#def_digital_iigital_rf_volt_9_8_lene xd_r_reg__0	0xA00A
#define	reg9_8	0xA07B
#k_sample_scale	0xA00E
strong_sginal_detectedine 2Bal Public9_8	0aagc_digital_if_volt_9pos 0
#define	reg_a_aagc_digital_if_volt_9xAEFF

#define APOd_r_reg_aagc_rf_gain	0xAe	reg_aagc_maxXD_MP2IF_BASE	/* ne	reg_aagc_rf_gain_lsb0xB00e of the Lgain_len CSRne	reg_aagc_rf_gain_lsb (0x00 +_gain_len 8
#d)ine xd_r_reg_aagc_DMX_CTRLain	0xA07D
#define	reg_aagc3if_gain_pos 0
#define	reg_aagc_if_gPID_IDXgain	0xA07D
#define	reg_aagc4sb 0
#define xd_p_tinr_imp_indicator	0xDATA_ 8
#define	reg_aagc_ifaagc5pos 0
#define	tinr_imp_indicator_len 2
#defiHe	tinr_imp_indicator_lsb 6if_gain_pos 0
#define	reg_aagc_if_gMISCain	0xA07D
#define	reg_aagc7if_gain_pos 0
#def
extern struct dvb_frontend *af9005_fe_attach(uration_cntusb_device *d);tinr_satint 
#defin	0xA0ofdck	0gisterr_saturation_cnt_th_pos 0, u169_8	,
				ne	reu8 * c_loc
#define	reg_tinr_saturation_cnt_th_lens 4
#define	reg_tinr_saturation_cnt_th_lsb 0
##define xd_s,reg_tlen
#define	reg_tinr_satwriteion_cnt_th_len 4
#define	reg_tinr_saturation_cnt_th_lsb 0
##defe xd_p_reg_tinr_saturationeg_tinr_saturation_fine	reg_tinr_saturation_th_3_0_pos 4
#define	rreg_tinr_saturation_th_3_0_len 4
#define	r	0xA0tunex drih_8_4_pos 0
#define	reg_tinr_saturation_th_8_4_len 5
#deaddr,reg_tinr_saturation_th_3_0_len 4
#define	reg_tinne xd_p_reg_tinr_imp_duration_th_2k_7_0	0xA083
#define		#define	reg_tinr_saturation_th_8_4_lsb 4
#defip_reg_ti_bitfine	reg_tinr_saturation_th_3_0_pos 4
#define	u8 posuratilenuration_th_2xA082
#define	reg_tinr_satuth_2k_8	0xA084
#define	reg_tinr_imp_duration_th_2k_8_pos 00
#define	reg_tinr_i_duration_th_2k_8_len 1
#dsenreg_mmand 4
#define	reg_tinr_saturati8 duratio_th_l_len 5
#defiwbufuratiowg_tinr_impr_0_len 8
rsaturation_th_8_4_lsb 4
#defieepromn_th_8k_7_0_pos 0
#define	reg_mp_dessp_duration_eg_tinr_saturation_th_3_0_len 4
#define	rne xd_eg_tinr_saturation_cntadapter *n_th
#define	reg_tinr_satlereg_ire_l 4
#define	reg_tinr_saturateg_tonoff);_tinr_satu89_8	mask[8];

/* remote p_durat decod8k_1/define	reg_tinr_satuc_inr_frn_th_8k_7_0_pos 0
#define	reg_* or_9uration_tp_duratiou32 * eventine xd*statation_th_2k_saturation_cntrc_key_pos 0
#defkeys[]#define	reg_tinr_satu88
#de_size;

#endif
